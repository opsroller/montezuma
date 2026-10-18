# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Simulate a brazier fire and bake it into motion-vector flipbooks, with numpy.

    python3 tools/assets/fx/gen_fire.py [tools/assets/fx/fire.json]

A small 3D gas simulation of our own (stable fluids: semi-Lagrangian
advection, buoyancy, vorticity confinement, an FFT pressure projection, and a
fuel/temperature/soot combustion model like Blender's Mantaflow fire) runs
for the warm-up plus the loop, then each frame is ray-marched side on into a
tile. The flipbooks are build outputs, not sources: git ignores them, and
CMake and CI run this script before anything loads the game. It records a
hash of its inputs (this script, the parameters, numpy and Pillow versions)
in game/assets/textures/<theme>/fx/.inputs and does nothing while that hash
and every output still match.

Writes, for the parameters' theme:

- game/assets/textures/<theme>/fx/T_Fire_Flame.png       4x4 sheet (mobile, web)
- game/assets/textures/<theme>/fx/T_Fire_Motion.png      its motion vectors
- game/assets/textures/<theme>/fx/hi/T_Fire_Flame.png    8x8 sheet (desktop)
- game/assets/textures/<theme>/fx/hi/T_Fire_Motion.png   its motion vectors

Flame sheet channels (linear, not sRGB): R emission (gamma-encoded, see
render.emission_gamma), G emission-weighted temperature mapped from
render.temperature_range to 0..1 (the shader turns it into a blackbody
colour), B smoke opacity, A 1. Motion sheet: R and G are the screen motion of
the tile content from this frame to the next, in tile UV units, as
0.5 + motion / (2 * motion_scale), motion_scale from the sheet's
parameters (the materials use the same number); B 0, A 1. Image rows go top down, so
rising gas has a negative G motion. Same parameters and seed give the same
bytes on the same numpy.
"""

import hashlib
import json
import sys
import time
from pathlib import Path

import numpy as np
import PIL
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]
DEBUG = False


# --------------------------------------------------------------------------
# Fields live on a collocated grid indexed [z, y, x]; y is up. Velocity is in
# cells per frame, so a substep of 1/substeps frames moves gas u/substeps cells.


def smooth_noise(rng, shape, cells):
    """Periodic smooth noise in about [-1, 1], features about `cells` cells across."""
    white = rng.standard_normal(shape).astype(np.float32)
    freqs = np.meshgrid(*[np.fft.fftfreq(n) for n in shape], indexing="ij")
    f2 = sum(f * f for f in freqs)
    spectrum = np.fft.fftn(white) * np.exp(-f2 * (np.pi * cells) ** 2)
    out = np.real(np.fft.ifftn(spectrum)).astype(np.float32)
    return out / (np.abs(out).max() + 1e-12)


class Sampler:
    """Trilinear lookups of many fields at one set of back-traced points."""

    def __init__(self, shape, z, y, x):
        nz, ny, nx = shape
        z = np.clip(z, 0, nz - 1.001)
        y = np.clip(y, 0, ny - 1.001)
        x = np.clip(x, 0, nx - 1.001)
        z0, y0, x0 = z.astype(np.int32), y.astype(np.int32), x.astype(np.int32)
        fz, fy, fx = z - z0, y - y0, x - x0
        sy, sz = nx, nx * ny
        base = z0 * sz + y0 * sy + x0
        self.idx = [base + dz * sz + dy * sy + dx for dz in (0, 1) for dy in (0, 1) for dx in (0, 1)]
        self.w = [(fz if dz else 1 - fz) * (fy if dy else 1 - fy) * (fx if dx else 1 - fx)
                  for dz in (0, 1) for dy in (0, 1) for dx in (0, 1)]

    def __call__(self, field, bounds=False):
        flat = field.ravel()
        out = np.zeros(field.shape, np.float32)
        lo = hi = None
        for i, w in zip(self.idx, self.w):
            c = flat[i]
            out += c * w
            if bounds:
                lo = c if lo is None else np.minimum(lo, c)
                hi = c if hi is None else np.maximum(hi, c)
        return (out, lo, hi) if bounds else out


def maccormack(forward, backward, field):
    """Second-order advection: a forward step corrected by its own round-trip error,
    clamped to the cells it came from so it never overshoots."""
    ahead, lo, hi = forward(field, bounds=True)
    back = backward(ahead)
    return np.clip(ahead + 0.5 * (field - back), lo, hi)


def curl(u, v, w):
    """Curl of (u, v, w) = (x, y, z) velocity by central differences."""
    def d(f, axis):
        return (np.roll(f, -1, axis) - np.roll(f, 1, axis)) * 0.5
    # axes: 0 z, 1 y, 2 x
    cx = d(w, 1) - d(v, 0)
    cy = d(u, 0) - d(w, 2)
    cz = d(v, 2) - d(u, 1)
    return cx, cy, cz


class Projector:
    """Makes a periodic velocity field divergence free, spectrally."""

    def __init__(self, shape):
        k = np.meshgrid(*[np.sin(2 * np.pi * np.fft.fftfreq(n)) for n in shape], indexing="ij")
        self.kz, self.ky, self.kx = (a.astype(np.float32) for a in k)
        k2 = self.kz ** 2 + self.ky ** 2 + self.kx ** 2
        k2[k2 == 0] = 1.0
        self.inv_k2 = 1.0 / k2

    def __call__(self, u, v, w):
        uh, vh, wh = np.fft.fftn(u), np.fft.fftn(v), np.fft.fftn(w)
        div = (self.kx * uh + self.ky * vh + self.kz * wh) * self.inv_k2
        u = np.real(np.fft.ifftn(uh - self.kx * div)).astype(np.float32)
        v = np.real(np.fft.ifftn(vh - self.ky * div)).astype(np.float32)
        w = np.real(np.fft.ifftn(wh - self.kz * div)).astype(np.float32)
        return u, v, w


def simulate(p):
    """Runs the fire and yields (emission, temperature, smoke, u, v) per kept frame."""
    nx, ny, nz = p["grid"]
    shape = (nz, ny, nx)
    rng = np.random.default_rng(p["seed"])
    gas, comb, burner = p["gas"], p["combustion"], p["burner"]
    substeps = p["substeps"]
    dt = 1.0 / substeps

    u, v, w = (np.zeros(shape, np.float32) for _ in range(3))
    fuel, temp, smoke = (np.zeros(shape, np.float32) for _ in range(3))
    project = Projector(shape)

    zz, yy, xx = np.meshgrid(*[np.arange(n, dtype=np.float32) for n in shape], indexing="ij")
    # Sponge: velocity and scalars fade to zero near every face, which keeps the
    # periodic domain from feeding the top of the plume back into the bottom.
    s = gas["sponge_cells"]
    edge = np.minimum.reduce([zz, nz - 1 - zz, xx, nx - 1 - xx, ny - 1 - yy]) / s
    sponge = np.clip(edge, 0, 1).astype(np.float32) ** 2
    r = np.sqrt((xx - (nx - 1) / 2) ** 2 + (zz - (nz - 1) / 2) ** 2)
    in_burner = (r < burner["radius"]) & (yy >= burner["y"]) & (yy < burner["y"] + burner["height"])
    # Fuel bed: thickest at the centre, like coals heaped in the bowl.
    bed = np.clip(1 - (r[:, 0, :] / burner["radius"]) ** 2, 0, 1).astype(np.float32)

    # Slowly evolving noise for the fuel bed and a weak turbulent force.
    key_every = 4
    bed_keys = [smooth_noise(rng, (nz, nx), burner["noise_cells"]) for _ in range(2)]
    force_keys = [[smooth_noise(rng, shape, 6.0) for _ in range(3)] for _ in range(2)]

    total = p["warmup_frames"] + p["frames"] + p["loop_blend_frames"]
    for frame in range(total):
        if frame % key_every == 0 and frame:
            bed_keys = [bed_keys[1], smooth_noise(rng, (nz, nx), burner["noise_cells"])]
            force_keys = [force_keys[1], [smooth_noise(rng, shape, 6.0) for _ in range(3)]]
        a = (frame % key_every) / key_every
        a = a * a * (3 - 2 * a)
        bed_noise = (1 - a) * bed_keys[0] + a * bed_keys[1]
        force = [(1 - a) * f0 + a * f1 for f0, f1 in zip(*force_keys)]
        emission = np.zeros(shape, np.float32)

        for _ in range(substeps):
            # Inflow: fuel and heat over the bed, gas pushed up out of the bowl.
            amount = burner["fuel"] * bed * np.clip(1 - burner["fuel_noise"] + burner["fuel_noise"] * (0.5 + 0.5 * bed_noise), 0, 1)
            amount3 = np.broadcast_to(amount[:, None, :], shape)
            fuel = np.where(in_burner, np.maximum(fuel, amount3), fuel)
            temp = np.where(in_burner & (amount3 > 0.05), np.maximum(temp, burner["temperature"]), temp)
            v = np.where(in_burner, np.maximum(v, burner["inflow_speed"] * bed[:, None, :]), v)

            # Combustion: hot fuel burns, releasing heat and soot.
            lit = np.clip((temp - comb["ignition"]) / 0.15, 0, 1)
            burned = fuel * (1 - np.exp(-comb["burn_rate"] * dt)) * lit
            fuel -= burned
            temp += comb["heat_release"] * burned
            smoke += comb["smoke_yield"] * burned
            emission += burned / dt

            # Forces: buoyancy of hot gas against the weight of soot, swirl, turbulence.
            v += (gas["buoyancy"] * temp - gas["smoke_weight"] * smoke) * dt
            cx, cy, cz = curl(u, v, w)
            mag = np.sqrt(cx * cx + cy * cy + cz * cz)
            gz, gy, gx = np.gradient(mag)
            gl = np.sqrt(gx * gx + gy * gy + gz * gz) + 1e-6
            nxv, nyv, nzv = gx / gl, gy / gl, gz / gl
            e = gas["vorticity"] * dt
            u += e * (nyv * cz - nzv * cy)
            v += e * (nzv * cx - nxv * cz)
            w += e * (nxv * cy - nyv * cx)
            t = gas["turbulence"] * dt * np.clip(temp, 0, 1)
            u += t * force[0]
            v += t * force[1] * 0.5
            w += t * force[2]

            # Advection, then make the velocity divergence free.
            fwd = Sampler(shape, zz - w * dt, yy - v * dt, xx - u * dt)
            bwd = Sampler(shape, zz + w * dt, yy + v * dt, xx + u * dt)
            u, v, w, fuel, temp, smoke = (maccormack(fwd, bwd, f) for f in (u, v, w, fuel, temp, smoke))
            damp = (1 - gas["velocity_damping"] * dt) * sponge
            u, v, w = u * damp, v * damp, w * damp
            u, v, w = project(u, v, w)
            temp *= np.exp(-gas["cooling"] * dt) * sponge
            smoke *= np.exp(-gas["smoke_dissipation"] * dt) * sponge
            fuel *= np.exp(-gas["fuel_dissipation"] * dt) * sponge

        if frame % 10 == 0 and DEBUG:
            print(frame, f"vmax {v.max():.2f} tmax {temp.max():.2f} fuel {fuel.sum():.0f} smoke {smoke.max():.2f} em {emission.max():.3f}")
        if frame >= p["warmup_frames"]:
            yield emission / substeps, temp.copy(), smoke.copy(), u.copy(), v.copy()


def blur2d(img, sigma):
    """Gaussian blur by FFT, zero-padded so nothing wraps across the tile."""
    pad = int(np.ceil(3 * sigma)) + 1
    big = np.pad(img, pad)
    fy = np.fft.fftfreq(big.shape[0])[:, None]
    fx = np.fft.fftfreq(big.shape[1])[None, :]
    k = np.exp(-2 * (np.pi * sigma) ** 2 * (fx * fx + fy * fy))
    out = np.real(np.fft.ifft2(np.fft.fft2(big) * k)).astype(np.float32)
    return out[pad:-pad, pad:-pad]


def render(p, emission, temp, smoke, u, v):
    """Ray-marches one frame side on (along z) into the crop rectangle.

    Returns emission, temperature, smoke opacity and the weighted (x, y)
    screen velocity in cells per frame, as [y, x] images with y down.
    """
    rp = p["render"]
    x0, y0, x1, y1 = rp["crop"]
    sl = (slice(None), slice(y0, y1), slice(x0, x1))
    e, t, s, uu, vv = (f[sl] for f in (emission, temp, smoke, u, v))
    # Front-to-back transmittance through the soot.
    tau = np.cumsum(rp["absorption"] * s, axis=0) - rp["absorption"] * s * 0.5
    trans = np.exp(-tau)
    we = e * trans
    E = blur2d(we.sum(axis=0), rp["blur"])
    T = (we * t).sum(axis=0) / (E + 1e-6)
    S = 1 - np.exp(-(rp["absorption"] * s).sum(axis=0))
    # Motion follows the visible stuff: the flame where it glows, else the smoke.
    wm = we + 0.05 * s * trans
    W = wm.sum(axis=0)
    MX = (wm * uu).sum(axis=0)
    MY = (wm * vv).sum(axis=0)
    flip = (slice(None, None, -1), slice(None))
    return E[flip], T[flip], S[flip], MX[flip], MY[flip], W[flip]


def resize(img, size):
    return np.asarray(Image.fromarray(img.astype(np.float32), mode="F").resize(size, Image.Resampling.LANCZOS))


def bake(p, frames):
    """Builds the sheets from the rendered frames; returns {variant: (flame, motion)}."""
    rp = p["render"]
    n = p["frames"]
    blend = p["loop_blend_frames"]
    # Seamless loop: the first `blend` frames fade from the frames after the
    # loop's end into the loop's own start.
    def looped(i, k):
        if i >= blend:
            return frames[i][k]
        a = i / blend
        return (1 - a) * frames[n + i][k] + a * frames[i][k]

    raw = [[looped(i, k) for k in range(6)] for i in range(n)]
    e_ref = np.percentile(np.stack([f[0] for f in raw]), 99.8) + 1e-6
    t_lo, t_hi = rp["temperature_range"]
    tw, th = rp["tile"]
    crop_w = rp["crop"][2] - rp["crop"][0]
    crop_h = rp["crop"][3] - rp["crop"][1]

    out = {}
    for variant, sheet in p["sheets"].items():
        g, stride = sheet["grid"], sheet["stride"]
        count = g * g
        assert count * stride == n, f"{variant}: {g}x{g} frames every {stride} must cover the {n} simulated frames"
        flame = np.zeros((g * th, g * tw, 4), np.float32)
        mv_tiles = []
        for i in range(count):
            f0 = i * stride
            E = sum(raw[(f0 + j) % n][0] for j in range(stride)) / stride
            T = raw[f0][1]
            S = raw[f0][2]
            # Motion to the next sheet frame: `stride` simulated frames of travel.
            W = blur2d(raw[f0][5], 1.5) + 1e-4
            mx = blur2d(raw[f0][3], 1.5) / W * stride
            my = blur2d(raw[f0][4], 1.5) / W * stride
            row, col = divmod(i, g)
            ys, xs = slice(row * th, (row + 1) * th), slice(col * tw, (col + 1) * tw)
            flame[ys, xs, 0] = np.clip(resize(E / e_ref, (tw, th)), 0, 1) ** rp["emission_gamma"]
            flame[ys, xs, 1] = np.clip((resize(T, (tw, th)) - t_lo) / (t_hi - t_lo), 0, 1)
            flame[ys, xs, 2] = np.clip(resize(S, (tw, th)), 0, 1)
            flame[ys, xs, 3] = 1
            # Cells per frame to tile UV per sheet frame; image y is down.
            mv_tiles.append((ys, xs, resize(mx / crop_w, (tw, th)), resize(-my / crop_h, (tw, th))))
        # The scale is a fixed parameter, shared with the material, so the
        # committed materials never depend on a build output. The measured
        # 99.5th percentile is printed to help retune it.
        measured = float(np.percentile(np.abs(np.stack([m[2] for m in mv_tiles] + [m[3] for m in mv_tiles])), 99.5))
        scale = sheet["motion_scale"]
        print(f"{variant}: motion 99.5th percentile {measured:.4f} tile UV per frame, encoded with scale {scale}")
        motion = np.zeros((g * th, g * tw, 4), np.float32)
        for ys, xs, mx, my in mv_tiles:
            motion[ys, xs, 0] = np.clip(0.5 + mx / (2 * scale), 0, 1)
            motion[ys, xs, 1] = np.clip(0.5 + my / (2 * scale), 0, 1)
        motion[..., 3] = 1
        out[variant] = (flame, motion)
    return out


def to_png(path, img):
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(np.round(np.clip(img, 0, 1) * 255).astype(np.uint8), mode="RGBA").save(path, optimize=True)


def main():
    params_path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "tools/assets/fx/fire.json"
    p = json.loads(params_path.read_text())
    out_dir = ROOT / "game/assets/textures" / p["theme"] / "fx"
    outputs = [out_dir / "T_Fire_Flame.png", out_dir / "T_Fire_Motion.png",
               out_dir / "hi/T_Fire_Flame.png", out_dir / "hi/T_Fire_Motion.png"]

    h = hashlib.sha256()
    h.update(Path(__file__).read_bytes())
    h.update(params_path.read_bytes())
    h.update(f"numpy {np.__version__} Pillow {PIL.__version__}".encode())
    stamp = out_dir / ".inputs"
    if stamp.exists() and stamp.read_text().strip() == h.hexdigest() and all(o.exists() for o in outputs):
        print(f"{p['theme']} fire: up to date")
        return

    start = time.time()
    frames = [render(p, *f) for f in simulate(p)]
    sheets = bake(p, frames)
    names = {"base": "", "hi": "hi/"}
    for variant, (flame, motion) in sheets.items():
        to_png(out_dir / f"{names[variant]}T_Fire_Flame.png", flame)
        to_png(out_dir / f"{names[variant]}T_Fire_Motion.png", motion)
    stamp.write_text(h.hexdigest() + "\n")
    print(f"{p['theme']} fire: {len(frames)} frames simulated and baked in {time.time() - start:.0f} s")


if __name__ == "__main__":
    main()

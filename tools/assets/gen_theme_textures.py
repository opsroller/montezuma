# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Generate a board theme's physically based texture sets with numpy.

    python3 tools/assets/gen_theme_textures.py [params.json ...]   (default: every tools/assets/themes/*.json)

The textures are build outputs, not sources: git ignores them, and CMake and
CI run this script before anything loads the game. It records a hash of its
inputs (this script, the parameters, numpy and Pillow versions) in
game/assets/textures/<theme>/.inputs and does nothing while that hash and
every output still match, so calling it on every build is cheap.

Writes one PNG set per entry in the parameters' "textures.sets":

- game/assets/textures/<theme>/T_<Set>_{BC,N,ORM[,H]}.png      base, textures.size (1024)
- game/assets/textures/<theme>/hi/T_<Set>_{BC,N,ORM[,H]}.png   desktop, textures.hi_size (2048),
                                                                only for sets with "hi": true

BC is base colour (sRGB, no lighting or occlusion), N a tangent-space normal
map (OpenGL, green up), ORM packs occlusion, roughness and metallic (linear,
R/G/B) and H is height (linear, white high) for parallax. A set with a hi
variant is drawn once at hi size and box-filtered down for the base, so both
show the same surface. Every texture tiles seamlessly (noise is built in the
frequency domain, cells wrap), so materials map them triplanar without UVs.
Same parameters and seed give the same bytes.
"""

import hashlib
import json
import sys
from pathlib import Path

import numpy as np
import PIL
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
NORMAL_REF = 1024  # normal_strength is tuned for this edge length


def fbm(rng, n, octaves, falloff=1.8, stretch=(1.0, 1.0)):
    """Periodic fractal noise in [0, 1]: white noise shaped by a 1/f^falloff spectrum.

    stretch scales the x and y frequencies; (1, 6) gives streaks along x.
    """
    fx = np.fft.fftfreq(n)[None, :] * stretch[0]
    fy = np.fft.fftfreq(n)[:, None] * stretch[1]
    f = np.sqrt(fx * fx + fy * fy)
    f[0, 0] = 1.0
    spectrum = np.fft.fft2(rng.standard_normal((n, n)))
    amp = f ** -falloff
    amp[f < 1.0 / n] = 0
    amp[f > octaves / 64.0] *= 0.5
    out = np.real(np.fft.ifft2(spectrum * amp))
    out -= out.min()
    return (out / out.max()).astype(np.float32)


def voronoi(rng, n, cells):
    """Periodic Voronoi on a jittered grid of about `cells` points.

    Returns distance to the nearest point, gap to the second nearest (0 on a
    border) and a per-cell random id, all per pixel.
    """
    k = max(2, int(round(np.sqrt(cells))))
    jitter = rng.random((k, k, 2))
    px = (np.arange(k)[None, :] + jitter[..., 0]) * (n / k)  # point of grid cell [row, col]
    py = (np.arange(k)[:, None] + jitter[..., 1]) * (n / k)
    ids = rng.random((k, k)).astype(np.float32)
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    gy, gx = (yy * k / n).astype(np.int32), (xx * k / n).astype(np.int32)
    d1 = np.full((n, n), np.inf, np.float32)
    d2 = np.full((n, n), np.inf, np.float32)
    ident = np.zeros((n, n), np.float32)
    for oy in (-1, 0, 1):
        for ox in (-1, 0, 1):
            cy, cx = (gy + oy) % k, (gx + ox) % k
            dx = np.abs(xx - px[cy, cx])
            dy = np.abs(yy - py[cy, cx])
            dx = np.minimum(dx, n - dx)
            dy = np.minimum(dy, n - dy)
            d = np.sqrt(dx * dx + dy * dy)
            closer = d < d1
            d2 = np.where(closer, d1, np.minimum(d2, d))
            ident = np.where(closer, ids[cy, cx], ident)
            d1 = np.where(closer, d, d1)
    return d1, d2 - d1, ident


def blur(field, radius):
    """Periodic box blur by summed shifts; radius in pixels."""
    out = field.copy()
    for axis in (0, 1):
        acc = np.zeros_like(out)
        for s in range(-radius, radius + 1):
            acc += np.roll(out, s, axis=axis)
        out = acc / (2 * radius + 1)
    return out


def cavity(height, radius):
    """Occlusion from height: how far a pixel sits below its blurred neighbourhood, 1 = open."""
    return np.clip(1.0 - np.maximum(blur(height, radius) - height, 0) * 4.0, 0, 1)


def mix(a, b, t):
    t = t[..., None] if a.ndim == 3 and t.ndim == 2 else t
    return a * (1 - t) + b * t


def rgb(value, tint):
    return np.stack([value * tint[0], value * tint[1], value * tint[2]], axis=-1)


# --- Recipes: each returns albedo (display 0..1), height (0..1), occlusion, roughness, metallic ---

def stone(rng, n):
    """Weathered carved sandstone: bedding bands, mottling, grain, pits, chisel strokes, hairline cracks."""
    s = n / 1024
    large = fbm(rng, n, 6, 2.3)
    bands = fbm(rng, n, 10, 2.0, stretch=(0.15, 1.0))
    mid = fbm(rng, n, 24, 1.5)
    grain = fbm(rng, n, 64, 1.0)
    pits = np.clip((fbm(rng, n, 64, 0.85) - 0.64) * 5, 0, 1)
    chisel = fbm(rng, n, 40, 1.4, stretch=(1.0, 0.18))
    _, gap, _ = voronoi(rng, n, 18)
    crack_mask = np.clip((fbm(rng, n, 8, 2.0) - 0.55) * 4, 0, 1)
    crack = np.clip(1.0 - gap / (2.2 * s), 0, 1) * crack_mask
    height = 0.30 * large + 0.14 * bands + 0.18 * chisel + 0.20 * mid + 0.10 * grain - 0.25 * pits - 0.35 * crack
    height = (height - height.min()) / (height.max() - height.min())
    v = 0.64 + 0.14 * (large - 0.5) + 0.10 * (bands - 0.5) + 0.08 * (mid - 0.5) + 0.10 * (grain - 0.5)
    v -= 0.16 * pits + 0.12 * crack
    albedo = rgb(v, (1.03, 0.98, 0.90))
    occ = cavity(height, max(1, int(6 * s))) * (1 - 0.35 * crack)
    rough = 0.80 + 0.10 * (grain - 0.5) + 0.10 * pits - 0.10 * np.clip(large - 0.65, 0, 1)
    return albedo, height, occ, rough, np.zeros_like(v)


def ground(rng, n):
    """Jungle floor: sunken flagstones with moss and leaf litter in the joints."""
    s = n / 1024
    _, gap, ident = voronoi(rng, n, 64)
    joint = np.clip(1.0 - gap / (12 * s), 0, 1)
    moss_noise = fbm(rng, n, 16, 1.7)
    moss = np.clip(joint * 1.3 + (moss_noise - 0.5) * 2.2, 0, 1)
    grain = fbm(rng, n, 64, 1.2)
    stone_v = 0.36 + 0.12 * (ident - 0.5) + 0.1 * (grain - 0.5)
    moss_rgb = np.stack([0.20 + 0.08 * grain, 0.30 + 0.12 * grain, 0.13 + 0.04 * grain], axis=-1)
    albedo = mix(rgb(stone_v, (1.02, 0.98, 0.88)), moss_rgb, moss)
    height = (1 - joint) * 0.6 + 0.3 * grain + 0.1 * ident
    occ = cavity(height, max(1, int(5 * s))) * (1 - 0.3 * joint)
    rough = 0.82 + 0.12 * moss
    return albedo, height, occ, rough, np.zeros_like(grain)


def gold(rng, n):
    """Hammered gold leaf: shallow peened dimples, faint brushing, tarnish in the hollows."""
    s = n / 1024
    d1, _, _ = voronoi(rng, n, 260)
    dimple = np.clip(d1 / (38 * s), 0, 1) ** 1.5
    brush = fbm(rng, n, 64, 1.1, stretch=(0.08, 1.0))
    wear = fbm(rng, n, 8, 2.0)
    height = 0.7 * dimple + 0.15 * brush + 0.15 * wear
    occ = cavity(height, max(1, int(4 * s)))
    tarnish = np.clip((0.55 - wear) * 3, 0, 1) * (1 - dimple)
    v = 0.96 - 0.10 * tarnish
    albedo = rgb(v, (1.0, 0.99, 0.96))
    rough = 0.12 + 0.08 * (brush - 0.5) + 0.12 * tarnish + 0.04 * (1 - dimple)
    return albedo, height, occ, rough, np.ones_like(v)


def gem(rng, n):
    """Polished gem surface, tinted by the material: cloudy inclusions, silk and fine polishing marks."""
    s = n / 1024
    cloud = fbm(rng, n, 12, 1.9)
    silk = fbm(rng, n, 64, 1.0, stretch=(1.0, 0.1))
    scratches = np.clip((fbm(rng, n, 64, 0.7, stretch=(0.05, 1.0)) - 0.78) * 6, 0, 1)
    v = 0.90 + 0.08 * (cloud - 0.5) - 0.06 * np.clip(cloud - 0.7, 0, 1) + 0.02 * (silk - 0.5)
    albedo = rgb(v, (1.0, 1.0, 1.0))
    height = 0.6 * cloud + 0.2 * silk - 0.2 * scratches * s
    rough = 0.04 + 0.04 * cloud + 0.12 * scratches
    return albedo, height, np.ones_like(v) * 0.98, rough, np.zeros_like(v)


def lapis(rng, n):
    """Lapis lazuli: mottled ultramarine, white calcite streaks and brassy pyrite flecks (metal)."""
    s = n / 1024
    mottle = fbm(rng, n, 10, 2.1)
    fine = fbm(rng, n, 64, 1.1)
    streak = fbm(rng, n, 16, 1.8, stretch=(0.3, 1.0))
    calcite = np.clip((streak - 0.74) * 5, 0, 1) * 0.8
    d1, _, ident = voronoi(rng, n, 900)
    pyrite = ((d1 < 2.6 * s) & (ident > 0.55)).astype(np.float32)
    blue = np.stack([0.11 + 0.06 * mottle, 0.25 + 0.10 * mottle, 0.70 + 0.16 * mottle], axis=-1)
    blue *= (0.92 + 0.16 * fine)[..., None]
    albedo = mix(blue, np.full_like(blue, 0.78), calcite)
    albedo = mix(albedo, rgb(np.ones_like(mottle), (0.86, 0.74, 0.44)), pyrite)
    height = 0.6 * mottle + 0.3 * fine + 0.2 * pyrite
    rough = 0.16 + 0.06 * fine + 0.10 * calcite - 0.06 * pyrite
    return albedo, height, np.ones_like(mottle) * 0.97, rough, pyrite


def jade(rng, n):
    """Nephrite jade, tinted by the material: cloudy body with paler veins and darker flecks."""
    cloud = fbm(rng, n, 10, 2.0)
    vein_src = fbm(rng, n, 16, 1.7)
    vein = np.clip(1.0 - np.abs(vein_src - 0.5) * 30, 0, 1) * 0.6
    fleck = np.clip((fbm(rng, n, 64, 0.9) - 0.8) * 5, 0, 1)
    v = 0.80 + 0.12 * (cloud - 0.5) + 0.12 * vein - 0.18 * fleck
    albedo = rgb(v, (0.97, 1.02, 0.97))
    height = 0.7 * cloud + 0.3 * vein
    rough = 0.20 + 0.06 * (cloud - 0.5) + 0.05 * fleck
    return albedo, height, np.ones_like(v) * 0.98, rough, np.zeros_like(v)


def moonstone(rng, n):
    """Moonstone, tinted by the material: milky body with soft parallel bands of the blue sheen."""
    cloud = fbm(rng, n, 8, 2.2)
    bands = fbm(rng, n, 16, 1.6, stretch=(0.12, 1.0))
    v = 0.86 + 0.07 * (cloud - 0.5) + 0.05 * (bands - 0.5)
    albedo = np.stack([v * 0.98, v * 0.99, v * 1.03], axis=-1)
    height = 0.6 * cloud + 0.4 * bands
    rough = 0.14 + 0.05 * (bands - 0.5)
    return albedo, height, np.ones_like(v) * 0.98, rough, np.zeros_like(v)


RECIPES = {"stone": stone, "ground": ground, "gold": gold, "gem": gem, "lapis": lapis, "jade": jade,
           "moonstone": moonstone}


def normal_from_height(h, strength):
    gx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * strength
    gy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * strength
    nz = np.ones_like(h)
    length = np.sqrt(gx * gx + gy * gy + nz * nz)
    # OpenGL convention (green up), which is what Godot expects.
    n = np.stack([-gx / length, gy / length, nz / length], axis=-1)
    return np.round((n * 0.5 + 0.5) * 255).astype(np.uint8)


def downsample(a, factor):
    """Box filter by an integer factor; keeps tiling because the edge divides evenly."""
    if factor == 1:
        return a
    n = a.shape[0] // factor
    shape = (n, factor, n, factor) + a.shape[2:]
    return a.reshape(shape).mean(axis=(1, 3))


def quantise(a):
    """Values here are authored as stored values (sRGB for BC, linear for the rest); just quantise."""
    return np.round(np.clip(a, 0, 1) * 255).astype(np.uint8)


def save(img, path, mode):
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(img, mode).save(path, compress_level=6)


def write_set(out, name, maps, n, spec):
    albedo, height, occ, rough, metal = maps
    strength = spec.get("normal_strength", 1.0) * NORMAL_REF / n
    save(quantise(albedo), out / f"T_{name}_BC.png", "RGB")
    save(normal_from_height(height.astype(np.float32), strength), out / f"T_{name}_N.png", "RGB")
    save(quantise(np.stack([occ, rough, metal], axis=-1)), out / f"T_{name}_ORM.png", "RGB")
    if spec.get("height"):
        save(quantise(height), out / f"T_{name}_H.png", "L")


def outputs(theme_dir, tex):
    for name, spec in sorted(tex["sets"].items()):
        maps = ["BC", "N", "ORM"] + (["H"] if spec.get("height") else [])
        for folder in [theme_dir] + ([theme_dir / "hi"] if spec.get("hi") else []):
            yield from (folder / f"T_{name}_{m}.png" for m in maps)


def inputs_hash(params_bytes):
    h = hashlib.sha256()
    for part in (Path(__file__).read_bytes(), params_bytes, np.__version__.encode(), PIL.__version__.encode()):
        h.update(hashlib.sha256(part).digest())
    return h.hexdigest()


def generate(params_path):
    raw = params_path.read_bytes()
    p = json.loads(raw)
    tex = p["textures"]
    base_n, hi_n = tex["size"], tex.get("hi_size", tex["size"])
    out = ROOT / "game/assets/textures" / p["theme"]
    stamp = out / ".inputs"
    digest = inputs_hash(raw)
    expected = list(outputs(out, tex))
    if stamp.exists() and stamp.read_text().strip() == digest and all(f.exists() for f in expected):
        print(f"{p['theme']}: textures up to date")
        return
    for old in list(out.glob("T_*.png")) + list(out.glob("hi/T_*.png")):
        if old not in expected:
            old.unlink()
    for index, (name, spec) in enumerate(sorted(tex["sets"].items())):
        rng = np.random.default_rng([p["seed"], index])
        recipe = RECIPES[spec["recipe"]]
        if spec.get("hi"):
            maps = recipe(rng, hi_n)
            write_set(out / "hi", name, maps, hi_n, spec)
            maps = [downsample(m, hi_n // base_n) for m in maps]
            sizes = f"{hi_n} (hi) + {base_n}"
        else:
            maps = recipe(rng, base_n)
            sizes = f"{base_n}"
        write_set(out, name, maps, base_n, spec)
        print(f"T_{name}: {sizes}, BC/N/ORM{'/H' if spec.get('height') else ''}")
    stamp.write_text(digest + "\n")


def main():
    paths = [Path(a) for a in sys.argv[1:]] or sorted((ROOT / "tools/assets/themes").glob("*.json"))
    for params_path in paths:
        generate(params_path)


if __name__ == "__main__":
    main()

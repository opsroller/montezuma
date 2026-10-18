# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Generate a board theme's tiling textures with numpy.

    python3 tools/assets/gen_theme_textures.py [tools/assets/themes/temple.json]

Writes PNG texture sets under game/assets/textures/<theme>/. Every texture
tiles seamlessly (noise is built in the frequency domain, cells wrap), so
materials can map them triplanar without UVs. Base colour is near neutral
and carries no lighting; materials tint it. Same seed, same pixels.
"""

import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]


def fbm(rng, n, octaves, falloff=1.8):
    """Periodic fractal noise in [0, 1]: white noise shaped by a 1/f^falloff spectrum."""
    fx = np.fft.fftfreq(n)[:, None]
    fy = np.fft.fftfreq(n)[None, :]
    f = np.sqrt(fx * fx + fy * fy)
    f[0, 0] = 1.0
    spectrum = np.fft.fft2(rng.standard_normal((n, n)))
    amp = f ** -falloff
    amp[f < 1.0 / n] = 0
    amp[f > octaves / 64.0] *= 0.5
    out = np.real(np.fft.ifft2(spectrum * amp))
    out -= out.min()
    return out / out.max()


def voronoi_edges(rng, n, cells):
    """Periodic Voronoi: distance to the nearest cell border, plus a per-cell random id."""
    pts = rng.random((cells, 2)) * n
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    d1 = np.full((n, n), np.inf, np.float32)
    d2 = np.full((n, n), np.inf, np.float32)
    ident = np.zeros((n, n), np.float32)
    ids = rng.random(cells)
    for i, (px, py) in enumerate(pts):
        dx = np.abs(xx - px)
        dy = np.abs(yy - py)
        dx = np.minimum(dx, n - dx)
        dy = np.minimum(dy, n - dy)
        d = np.sqrt(dx * dx + dy * dy)
        closer = d < d1
        d2 = np.where(closer, d1, np.minimum(d2, d))
        ident = np.where(closer, ids[i], ident)
        d1 = np.where(closer, d, d1)
    return (d2 - d1), ident


def normal_from_height(h, strength):
    gx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * strength
    gy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * strength
    nz = np.ones_like(h)
    length = np.sqrt(gx * gx + gy * gy + nz * nz)
    # OpenGL convention (green up), which is what Godot expects.
    n = np.stack([-gx / length, gy / length, nz / length], axis=-1)
    return ((n * 0.5 + 0.5) * 255).astype(np.uint8)


def save(img, path, mode):
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(img, mode).save(path, optimize=True)


def stone(rng, n):
    """Weathered limestone: mottling, pits and fine grain. Neutral warm grey."""
    large = fbm(rng, n, 8, 2.2)
    grain = fbm(rng, n, 64, 1.1)
    pits = fbm(rng, n, 64, 0.9)
    pits = np.clip((pits - 0.72) * 6, 0, 1)
    height = 0.55 * large + 0.35 * grain - 0.25 * pits
    v = 0.62 + 0.22 * (large - 0.5) + 0.12 * (grain - 0.5) - 0.18 * pits
    albedo = np.stack([v * 1.02, v * 0.98, v * 0.92], axis=-1)
    rough = 0.82 + 0.1 * (grain - 0.5) + 0.12 * pits
    return albedo, height, rough


def ground(rng, n):
    """Jungle floor: sunken flagstones with moss and leaf litter in the joints."""
    edge, ident = voronoi_edges(rng, n, 64)
    joint = np.clip(1.0 - edge / (n * 0.012), 0, 1)
    moss_noise = fbm(rng, n, 16, 1.7)
    moss = np.clip(joint * 1.3 + (moss_noise - 0.5) * 2.2, 0, 1)
    grain = fbm(rng, n, 64, 1.2)
    stone_v = 0.34 + 0.12 * (ident - 0.5) + 0.1 * (grain - 0.5)
    stone_rgb = np.stack([stone_v * 1.02, stone_v * 0.98, stone_v * 0.88], axis=-1)
    moss_rgb = np.stack([0.20 + 0.08 * grain, 0.30 + 0.12 * grain, 0.12 + 0.04 * grain], axis=-1)
    albedo = stone_rgb * (1 - moss[..., None]) + moss_rgb * moss[..., None]
    height = (1 - joint) * 0.6 + 0.3 * grain + 0.1 * ident
    rough = 0.85 + 0.1 * moss
    return albedo, height, rough


def to_srgb8(linear01):
    """Albedo values here are authored as display (sRGB) values; just quantise."""
    return (np.clip(linear01, 0, 1) * 255).astype(np.uint8)


def main():
    params_path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "tools/assets/themes/temple.json"
    p = json.loads(params_path.read_text())
    n = p["textures"]["size"]
    out = ROOT / "game/assets/textures" / p["theme"]
    rng = np.random.default_rng(p["seed"])
    for name, fn, strength in (("Stone_01", stone, 6.0), ("Ground_01", ground, 9.0)):
        albedo, height, rough = fn(rng, n)
        save(to_srgb8(albedo), out / f"T_{name}_BC.png", "RGB")
        save(normal_from_height(height.astype(np.float32), strength), out / f"T_{name}_N.png", "RGB")
        save((np.clip(rough, 0, 1) * 255).astype(np.uint8), out / f"T_{name}_R.png", "L")
        print(f"T_{name}: {n}x{n}, BC/N/R")


if __name__ == "__main__":
    main()

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
- game/assets/textures/<theme>/grade/T_Grade_LUT.png              the colour grade as a 3D LUT,
                                                                for themes with a "grade" (grade_lut.py)

BC is base colour (sRGB, no lighting or occlusion), N a tangent-space normal
map (OpenGL, green up), ORM packs occlusion, roughness and metallic (linear,
R/G/B) and H is height (linear, white high) for parallax. A set with a hi
variant is drawn once at hi size and box-filtered down for the base, so both
show the same surface. Every texture tiles seamlessly (noise is built in the
frequency domain, cells wrap), so materials map them triplanar without UVs.
Same parameters and seed give the same bytes.

Next to every PNG it writes the Godot .import file with the map's import
settings (tools/assets/texture_import.py: mipmaps, VRAM compression, RGTC for
normal maps), on every run, since git and the CI texture cache keep neither.
"""

import hashlib
import json
import sys
from pathlib import Path

import numpy as np
import PIL
from PIL import Image

import grade_lut
import texture_import

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


def voronoi(rng, n, cells, spread=1.0):
    """Periodic Voronoi on a jittered grid of about `cells` points.

    spread scales the jitter around each grid cell's centre: 1 places points
    anywhere in their cell, lower values give cells of more even size.
    Returns distance to the nearest point, gap to the second nearest (0 on a
    border) and a per-cell random id, all per pixel.
    """
    k = max(2, int(round(np.sqrt(cells))))
    jitter = rng.random((k, k, 2))
    if spread != 1.0:
        jitter = 0.5 + (jitter - 0.5) * spread
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


def warp(fields, rng, n, amount, octaves=6):
    """Domain-warp periodic fields: sample each at positions pushed by one smooth noise.

    fields is a list of (n, n) arrays, all pushed the same way; amount is the
    largest push in pixels. Bilinear, wrapping at the edges.
    """
    dx = (fbm(rng, n, octaves, 2.2) - 0.5) * 2 * amount
    dy = (fbm(rng, n, octaves, 2.2) - 0.5) * 2 * amount
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    x, y = (xx + dx) % n, (yy + dy) % n
    x0, y0 = np.floor(x).astype(np.int32), np.floor(y).astype(np.int32)
    fx, fy = x - x0, y - y0
    x0, y0 = x0 % n, y0 % n
    x1, y1 = (x0 + 1) % n, (y0 + 1) % n
    out = []
    for f in fields:
        top = f[y0, x0] * (1 - fx) + f[y0, x1] * fx
        bottom = f[y1, x0] * (1 - fx) + f[y1, x1] * fx
        out.append(top * (1 - fy) + bottom * fy)
    return out


def leaves(rng, n, count, size):
    """Scattered fallen leaves: a per-pixel coverage mask (0..1) and a per-leaf random id.

    Each leaf is a pointed ellipse of about `size` pixels, randomly turned,
    with a darker midrib; the stamps wrap at the edges so the tile stays seamless.
    """
    cover = np.zeros((n, n), np.float32)
    ident = np.zeros((n, n), np.float32)
    r = int(np.ceil(size * 1.4))
    oy, ox = np.mgrid[-r:r + 1, -r:r + 1].astype(np.float32)
    for _ in range(count):
        cy, cx = rng.random(2) * n
        angle = rng.random() * np.pi
        length = size * (0.6 + 0.8 * rng.random())
        c, si = np.cos(angle), np.sin(angle)
        u = (ox * c + oy * si) / length          # along the leaf, -1..1
        v = (-ox * si + oy * c) / (length * 0.38)  # across it
        width = np.clip(1 - u * u, 0, 1) ** 0.7     # pointed at both ends
        inside = np.clip((width - np.abs(v)) * length * 0.5, 0, 1)
        midrib = np.clip(1 - np.abs(v) * length * 0.38 / 0.8, 0, 1) * inside
        stamp = inside * (1 - 0.35 * midrib)
        rows = (np.arange(-r, r + 1) + int(cy)) % n
        cols = (np.arange(-r, r + 1) + int(cx)) % n
        patch = cover[np.ix_(rows, cols)]
        over = stamp > 0.02
        cover[np.ix_(rows, cols)] = np.where(over, stamp, patch)
        ident[np.ix_(rows, cols)] = np.where(over, rng.random(), ident[np.ix_(rows, cols)])
    return cover, ident


# --- Recipes: each returns albedo (display 0..1), height (0..1), occlusion, roughness, metallic ---

def stone(rng, n):
    """Weathered Mesoamerican limestone: bedding, porous vugs, chisel strokes, cracks, run-off
    stains, dark biofilm in the hollows, crustose lichen and traces of red stucco pigment.

    Kept near-neutral around mid value: the board materials multiply it by their own tints.
    """
    s = n / 1024
    large = fbm(rng, n, 6, 2.3)
    (bands,) = warp([fbm(rng, n, 10, 2.0, stretch=(0.15, 1.0))], rng, n, 40 * s)
    mid = fbm(rng, n, 24, 1.5)
    grain = fbm(rng, n, 64, 1.0)
    chisel = fbm(rng, n, 40, 1.4, stretch=(1.0, 0.18))
    # Vugs: small dissolution pores in clusters along the soft beds, and shallow
    # irregular solution pits where the surface has eroded.
    d1, _, pore_id = voronoi(rng, n, 5000)
    (d1,) = warp([d1], rng, n, 3 * s, octaves=32)
    soft = np.clip((bands - 0.45) * 3, 0, 1) * np.clip((fbm(rng, n, 12, 1.8) - 0.45) * 3, 0, 1)
    pores = np.clip(1.0 - d1 / ((0.6 + 1.6 * pore_id) * s), 0, 1) ** 0.7 * (pore_id > 0.975 - 0.6 * soft)
    pores *= np.clip((grain - 0.25) * 3, 0, 1)   # ragged, not drilled
    erosion = np.clip((fbm(rng, n, 32, 1.3) - 0.66) * 5, 0, 1)
    # Hairline cracks that wander and thin out instead of running straight between cell corners.
    _, gap, _ = voronoi(rng, n, 18)
    (gap,) = warp([gap], rng, n, 14 * s, octaves=24)
    crack_mask = np.clip((fbm(rng, n, 8, 2.0) - 0.55) * 4, 0, 1)
    crack = np.clip(1.0 - gap / ((0.8 + 2.2 * fbm(rng, n, 32, 1.4)) * s), 0, 1) * crack_mask
    height = 0.30 * large + 0.14 * bands + 0.18 * chisel + 0.20 * mid + 0.10 * grain - 0.12 * pores - 0.12 * erosion - 0.35 * crack
    height = (height - height.min()) / (height.max() - height.min())
    hollow = np.clip((blur(height, max(1, int(10 * s))) - height) * 8, 0, 1)
    # Weathering layers, each a mask in 0..1.
    (streaks,) = warp([fbm(rng, n, 24, 1.6, stretch=(0.06, 1.0))], rng, n, 10 * s, octaves=16)
    runoff = np.clip((streaks - 0.55) * 3, 0, 1) * np.clip((large - 0.3) * 2, 0, 1)
    biofilm = np.clip((fbm(rng, n, 32, 1.1) - 0.5) * 3 + hollow * 1.5 - 0.2, 0, 1) * 0.8
    # Crustose lichen: many small ragged colonies on the exposed faces, a few of them orange.
    # No large blotches: the board shows about one tile per cell, so they would read as a repeat.
    lichen = np.clip((fbm(rng, n, 48, 0.9) + 0.35 * (grain - 0.5) - 0.56) * 7, 0, 1) * (1 - hollow)
    lichen *= np.clip((fbm(rng, n, 32, 1.1) - 0.42) * 4, 0, 1)
    lichen_orange = lichen * np.clip((fbm(rng, n, 24, 1.6) - 0.7) * 8, 0, 1)
    pigment = np.clip(hollow * 1.6 - 0.4, 0, 1) * np.clip((fbm(rng, n, 6, 2.2) - 0.6) * 4, 0, 1) * 0.55
    v = 0.66 + 0.06 * (large - 0.5) + 0.10 * (bands - 0.5) + 0.08 * (mid - 0.5) + 0.10 * (grain - 0.5)
    v -= 0.08 * pores + 0.09 * erosion + 0.12 * crack
    albedo = rgb(v, (1.03, 0.99, 0.92))
    albedo = mix(albedo, albedo * np.array([0.92, 0.80, 0.64], np.float32), runoff * 0.6)
    albedo = mix(albedo, rgb(0.30 + 0.06 * grain, (1.0, 1.0, 0.95)), biofilm * 0.45)
    albedo = mix(albedo, rgb(0.80 + 0.08 * grain, (0.96, 1.0, 0.90)), lichen * (1 - lichen_orange) * 0.55)
    albedo = mix(albedo, rgb(0.78 + 0.06 * grain, (1.0, 0.72, 0.38)), lichen_orange * 0.6)
    albedo = mix(albedo, rgb(0.58 + 0.06 * grain, (1.0, 0.56, 0.44)), pigment)
    height = np.clip(height + 0.03 * lichen, 0, 1)
    occ = cavity(height, max(1, int(6 * s))) * (1 - 0.35 * crack) * (1 - 0.2 * pores)
    rough = 0.80 + 0.10 * (grain - 0.5) + 0.12 * pores + 0.05 * erosion + 0.06 * biofilm - 0.10 * np.clip(large - 0.65, 0, 1)
    rough += 0.05 * lichen - 0.08 * pigment
    return albedo, height, occ, rough, np.zeros_like(v)


def ground(rng, n):
    """Old plaza paving in the jungle: irregular limestone flags of uneven size and hue, worn
    domed, settled at different depths and chipped at the edges, pitted on top, dark soil in
    the joints, moss creeping out of the joints and pooling in hollows, crustose lichen,
    fallen leaves and damp patches.

    Darker and lower in contrast than the board stone so the board stays in front.
    """
    s = n / 1024
    _, gap, ident = voronoi(rng, n, 40, spread=0.75)
    # Wobbly flag outlines; bilinear blends the ids only along the joints, which soil covers.
    gap, ident = warp([gap, ident], rng, n, 9 * s, octaves=14)
    hue = (ident * 7.31) % 1.0
    joint_w = (5 + 12 * fbm(rng, n, 16, 1.5)) * s
    edge = np.clip(gap / joint_w, 0, 1) ** 0.7     # 0 in the joint, 1 a joint-width in
    inner = np.clip(gap / (50 * s), 0, 1)
    dome = blur(inner * inner * (3 - 2 * inner), max(1, int(6 * s)))   # rounded, worn arris
    chips = np.clip((fbm(rng, n, 48, 1.1) - 0.56) * 5, 0, 1) * np.clip(1 - gap / (22 * s), 0, 1)
    grain = fbm(rng, n, 64, 1.2)
    speckle = fbm(rng, n, 64, 0.6)
    mid = fbm(rng, n, 20, 1.7)
    pits = np.clip((fbm(rng, n, 64, 1.0) - 0.7) * 6, 0, 1) * np.clip((mid - 0.4) * 3, 0, 1)
    sink = 0.22 * (ident - 0.5)                    # flags settled at different depths
    stone_h = (0.55 + sink + 0.20 * dome + 0.10 * mid + 0.06 * grain - 0.28 * chips - 0.08 * pits) * edge
    soil = 1 - edge
    hollow = np.clip((blur(stone_h, max(1, int(12 * s))) - stone_h) * 6, 0, 1)
    patches = fbm(rng, n, 12, 1.8)
    moss_fuzz = fbm(rng, n, 64, 0.8)
    # Moss: patchy in the joints, creeping a ragged way onto the flags and pooling in hollows.
    near_joint = np.clip(1 - gap / (55 * s), 0, 1) ** 1.5
    thrive = np.clip((patches - 0.4) * 2.5, 0, 1)   # bare joints where it is dry, spilling moss where damp
    moss = (soil + 1.2 * near_joint + hollow) * (0.3 + 1.5 * thrive) + 1.0 * (moss_fuzz - 0.5) - 0.6
    moss = np.clip(moss * 3, 0, 1)
    leaf, leaf_id = leaves(rng, n, int(200 * s * s), 13 * s)
    leaf *= np.clip((fbm(rng, n, 8, 2.0) - 0.35) * 2.5, 0, 1) * 0.95   # litter drifts in patches
    damp = np.clip((fbm(rng, n, 6, 2.2) - 0.55) * 4, 0, 1) * np.clip(hollow + soil + 0.3, 0, 1)

    stone_v = 0.36 + 0.11 * (ident - 0.5) + 0.06 * (grain - 0.5) + 0.05 * (mid - 0.5) + 0.03 * dome
    stone_v += 0.08 * (speckle - 0.5) - 0.06 * chips - 0.06 * pits
    stain = fbm(rng, n, 24, 1.5)                   # weathering mottle within each flag
    stone_v *= 1 - 0.22 * np.clip((stain - 0.45) * 2, 0, 1) - 0.18 * near_joint   # grime gathers by the joints
    warm = np.array([1.06, 1.0, 0.86], np.float32)
    grey = np.array([0.99, 0.99, 0.95], np.float32)
    albedo = stone_v[..., None] * (warm * (1 - hue[..., None]) + grey * hue[..., None])
    soil_rgb = np.stack([0.11 + 0.05 * grain, 0.085 + 0.04 * grain, 0.06 + 0.03 * grain], axis=-1)
    lichen = np.clip((fbm(rng, n, 64, 1.2) + 0.3 * (speckle - 0.5) - 0.55) * 6, 0, 1)
    lichen *= np.clip((fbm(rng, n, 12, 1.8) - 0.35) * 3, 0, 1) * edge * (1 - hollow)
    albedo = mix(albedo, rgb(0.50 + 0.08 * speckle, (1.0, 1.0, 0.88)), lichen * 0.5)
    albedo = mix(albedo, soil_rgb, soil)
    moss_rgb = mix(np.stack([0.13 + 0.06 * moss_fuzz, 0.19 + 0.08 * moss_fuzz, 0.07 + 0.03 * moss_fuzz], axis=-1),
                   np.stack([0.24 + 0.05 * moss_fuzz, 0.27 + 0.06 * moss_fuzz, 0.10 + 0.03 * moss_fuzz], axis=-1),
                   mid)
    albedo = mix(albedo, moss_rgb, moss * (0.65 + 0.35 * moss_fuzz))
    dry = np.stack([0.34, 0.24, 0.13], axis=-1) * np.ones((n, n, 1), np.float32)
    olive = np.stack([0.27, 0.26, 0.13], axis=-1) * np.ones((n, n, 1), np.float32)
    leaf_rgb = mix(dry, olive, leaf_id) * (0.55 + 0.6 * ((leaf_id * 13.7) % 1.0) * (0.7 + 0.3 * grain))[..., None]
    albedo = mix(albedo, leaf_rgb, leaf)
    albedo *= (1 - 0.18 * damp)[..., None]       # wet stone and soil darken

    height = stone_h + 0.06 * moss * moss_fuzz + 0.04 * leaf
    height = (height - height.min()) / (height.max() - height.min())
    occ = cavity(height, max(1, int(5 * s))) * (1 - 0.35 * soil)
    rough = 0.84 + 0.06 * (grain - 0.5) + 0.08 * moss + 0.08 * soil - 0.10 * leaf
    rough = np.clip(rough * (1 - 0.40 * damp), 0, 1)
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
    for png in expected:
        texture_import.write(png, png.stem.rsplit("_", 1)[-1])
    # The colour grade LUT takes milliseconds, so it is checked on every run.
    if "grade" in p and grade_lut.write(out, p["grade"]):
        print(f"T_Grade_LUT: {p['grade']['lut_size']}^3")
    if stamp.exists() and stamp.read_text().strip() == digest and all(f.exists() for f in expected):
        print(f"{p['theme']}: textures up to date")
        return
    for old in list(out.glob("T_*.png")) + list(out.glob("hi/T_*.png")):
        if old not in expected:
            old.unlink()
            old.with_name(old.name + ".import").unlink(missing_ok=True)
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

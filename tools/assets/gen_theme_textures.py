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
import multiprocessing
import os
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


def norm(a):
    """Stretch a field to the full 0..1 range (a _H map needs its whole range)."""
    lo, hi = float(a.min()), float(a.max())
    return (a - lo) / (hi - lo) if hi - lo > 1e-9 else np.zeros_like(a)


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


# --- Plant library recipes (tools/assets/themes/plants.json, tools/assets/plants.py) ---
#
# These two are parameterised, because a leaf is the same recipe with a different
# venation, outline, colour and amount of damage for every species, and 25 near-copies
# of one function would rot. The parameters come from `Species.leaf_tex` and
# `Species.bark_tex` in tools/assets/plants.py, which is the single place a species is
# described; plants.py writes them into themes/plants.json, and `spec["args"]` below
# hands them to the recipe.
#
# A leaf set is an ATLAS, not a tiling surface: 2x2 tiles, each the surface of one
# leaf in blade-local coordinates - u across the blade with the midrib at 0.5 and the
# margins at 0 and 1, v from base to tip - because the blade's outline is geometry
# here, not an alpha cutout. Tile (0,0) is mature, (1,0) the young flush, (0,1) an old
# blotched leaf and (1,1) a dead one. A specimen samples one tile, so a stand of one
# species carries four readings off one material and one draw call.
#
# Measured colour rules these follow (reference sampling, docs/reference/plant-library.md):
# gloss reads as saturation collapsing toward a NEUTRAL sheen band about 1.35x the
# lamina value, never as a bright highlight, and it never clips; midribs are about
# 1.7x the lamina value; damage carries a 1-2 mm pale halo; lamina saturation is lower
# and its value range wider than the usual guess. Nothing here is drawn from a
# photograph: the leaf is numpy, like every other surface in this file.


def _vein_field(venation, u, v, count):
    """Distance, in vein spacings, to the nearest secondary vein. 0 is on a vein."""
    s = (u - 0.5) * 2
    if venation == "parallel":
        # Along the blade: the straps and the palms. Veins run base to tip.
        phase = s * count * 0.5
    elif venation == "palmate":
        # Out of one point at the base: the ceiba's leaflets, a Bauhinia blade.
        phase = np.arctan2(np.maximum(v, 1e-3), s) / np.pi * count
    elif venation == "dichotomous":
        # Moss, lichen, spikemoss: no order at all, so this is noise, not a vein.
        return np.abs(np.sin(s * count * 0.7 + v * count * 0.9)) * 0.5
    else:
        # Pinnate: off the midrib at an angle, the angle opening toward the base.
        phase = v * count - np.abs(s) * (0.35 + 0.35 * v) * count / 3.0
    return np.abs(phase - np.round(phase))


def plant_leaf(rng, n, venation="pinnate", outline="ovate", green=(0.26, 0.42, 0.16),
               young=(0.42, 0.60, 0.18), dead=(0.40, 0.30, 0.18), vein=0.5, vein_count=14,
               midrib=1.7, blotch=0.3, damage=0.25, gloss=0.5, grid=(2, 2)):
    """One species' leaf atlas: the same leaf surface in four states."""
    gx, gy = int(grid[0]), int(grid[1])
    m = n // gy
    albedo = np.zeros((n, n, 3), np.float32)
    height = np.zeros((n, n), np.float32)
    occ = np.zeros((n, n), np.float32)
    rough = np.zeros((n, n), np.float32)
    states = [
        # (base colour, age 0..1, extra blotching, extra damage)
        (green, 0.5, 1.0, 1.0),
        (young, 0.0, 0.3, 0.3),
        (green, 1.0, 1.9, 1.8),
        (dead, 1.0, 2.4, 2.6),
    ]
    for index, (base, age, blotch_k, damage_k) in enumerate(states[:gx * gy]):
        tx, ty = index % gx, index // gx
        maps = _leaf_tile(rng, m, np.array(base, np.float32), age, venation, vein, vein_count,
                          midrib, blotch * blotch_k, damage * damage_k, gloss,
                          is_dead=index == 3)
        ys, xs = slice(ty * m, (ty + 1) * m), slice(tx * m, (tx + 1) * m)
        albedo[ys, xs], height[ys, xs], occ[ys, xs], rough[ys, xs] = maps
    return albedo, height, occ, np.clip(rough, 0.05, 1.0), np.zeros((n, n), np.float32)


def _leaf_tile(rng, m, base, age, venation, vein, vein_count, midrib, blotch, damage, gloss,
               is_dead=False):
    s_ratio = m / 512
    yy, xx = np.mgrid[0:m, 0:m].astype(np.float32)
    u, v = xx / (m - 1), yy / (m - 1)
    s = (u - 0.5) * 2
    across = np.abs(s)
    # Midrib: a pale raised keel on the centre line, narrowing toward the tip.
    rib_w = 0.05 * (1.25 - 0.6 * v)
    rib = np.exp(-(across / rib_w) ** 2)
    # Secondary veins, and a finer net between them.
    dist = _vein_field(venation, u, v, vein_count)
    sec = np.exp(-(dist / 0.13) ** 2) * (1 - rib)
    net = np.exp(-(_vein_field(venation, u, v, vein_count * 3) / 0.22) ** 2) * 0.35
    grain = fbm(rng, m, 64, 1.1)
    # Quilting: the lamina sits lower between the veins, 5-10 mm of it.
    quilt = (1 - np.clip(sec + 0.5 * net, 0, 1)) * 0.5
    height_f = norm(0.55 * rib + 0.22 * sec + 0.08 * net - 0.18 * quilt + 0.06 * grain)
    # Margin: browned and nicked, with the pale halo a healing edge carries.
    margin = np.clip((across - 0.86) / 0.14, 0, 1)
    nicks = np.clip((fbm(rng, m, 26, 1.3) - 0.52) * 4, 0, 1) * margin * damage
    holes = np.clip((fbm(rng, m, 40, 1.2) - 0.76) * 6, 0, 1) * damage * (1 - rib)
    halo = np.clip(blur(holes + nicks, max(1, int(3 * s_ratio))) * 2.2 - (holes + nicks), 0, 1)
    spots = np.clip((fbm(rng, m, 14, 1.6) - 0.56) * 3, 0, 1) * blotch
    chlorosis = np.clip((fbm(rng, m, 8, 2.0) - 0.5) * 2, 0, 1) * blotch * 0.8
    # Lamina value: shade shifts green bluer as well as darker, so the shaded side of
    # the quilting loses a little red and gains no green.
    body = base[None, None, :] * (0.92 + 0.16 * grain)[..., None]
    shade = (0.86 + 0.14 * (1 - quilt))[..., None]
    body = body * shade
    body[..., 0] *= 1.0 - 0.10 * quilt
    body[..., 2] *= 1.0 + 0.08 * quilt
    lamina = body
    # Veins: the midrib reads about 1.7x the lamina, the secondaries a little over 1.2x.
    lamina = mix(lamina, np.clip(body * midrib, 0, 0.92), rib * (0.35 + 0.5 * vein))
    lamina = mix(lamina, np.clip(body * (1.0 + 0.35 * (midrib - 1.0)), 0, 0.92),
                 sec * vein * 0.7 + net * vein * 0.3)
    # Age: yellowing between the veins first, then brown blotches and a dry margin.
    yellow = np.array([0.52, 0.46, 0.14], np.float32)
    brown = np.array([0.34, 0.24, 0.13], np.float32)
    lamina = mix(lamina, yellow[None, None, :], chlorosis * (0.3 + 0.7 * age) * (1 - rib))
    lamina = mix(lamina, brown[None, None, :], spots * 0.75)
    lamina = mix(lamina, brown[None, None, :] * 0.8, np.clip(margin * age * 0.8, 0, 1) * 0.7)
    # Damage: a hole is drawn, not cut (the blade's outline is geometry, and an alpha
    # cutout would cost the whole sorting and shadow-bias problem for nothing).
    lamina = mix(lamina, brown[None, None, :] * 0.55, np.clip(holes + nicks, 0, 1) * 0.85)
    lamina = mix(lamina, np.clip(base[None, None, :] * 2.1, 0, 0.88), halo * 0.5)
    # Sheen: saturation collapses toward a neutral band, it does not brighten.
    sheen_band = np.exp(-((v - 0.42) / 0.35) ** 2) * np.exp(-((s - 0.35) / 0.7) ** 2)
    sheen = sheen_band * gloss * (0.3 if is_dead else 1.0)
    grey = lamina.mean(axis=-1, keepdims=True) * 1.12
    lamina = mix(lamina, np.minimum(grey.repeat(3, axis=-1), 0.86), sheen * 0.45)
    occ_f = cavity(height_f, max(1, int(4 * s_ratio))) * (1 - 0.25 * np.clip(holes, 0, 1))
    rough_f = (0.86 - 0.42 * gloss) + 0.08 * (grain - 0.5) + 0.10 * spots + 0.12 * margin * age
    rough_f -= 0.18 * sheen
    # Base colour holds no light, so it holds no black and no white either: the floor
    # and ceiling keep the map inside the 30-240 sRGB window the validator enforces,
    # whatever a species' own colours are.
    return np.clip(lamina, 0.13, 0.94), height_f, occ_f, rough_f


def plant_bark(rng, n, style="fissured", colour=(0.30, 0.26, 0.21), lichen=0.3, moss=0.25):
    """Bark and root for the plant library, as a tiling surface.

    One recipe, six habits: a palm's leaf-scar rings, a dicot's vertical fissures, a
    tree fern's root-matted fibre, a spine-armed palm, the lenticelled grey-green of a
    ceiba, and smooth. The crustose lichen is the important part and the easy one to
    get wrong: at 150-300 m a trunk is a mottled, map-like patchwork of pale cream and
    off-white crust with irregular dark edges, LIGHTER than the bark, with moss only in
    the bottom metre or two. There is no beard lichen here at all.
    """
    s = n / 1024
    base = np.array(colour, np.float32)
    fine = fbm(rng, n, 64, 1.0)
    mid = fbm(rng, n, 22, 1.5)
    if style == "ringed":
        # Palm leaf scars: horizontal rings at a real spacing, not a sine wave.
        rings = np.abs(np.sin(np.mgrid[0:n, 0:n][0].astype(np.float32) / n * np.pi * 14))
        (rings,) = warp([rings], rng, n, 6 * s, octaves=12)
        relief = 0.55 * rings + 0.2 * mid
        gap = np.clip((0.35 - rings) * 3, 0, 1)
        height = norm(relief - 0.3 * gap + 0.12 * fine)
    elif style == "fibrous":
        # A tree fern's trunk is a mat of old root and frond base, all vertical.
        strands = fbm(rng, n, 40, 1.1, stretch=(1.0, 0.05))
        (strands,) = warp([strands], rng, n, 8 * s, octaves=14)
        hairs = fbm(rng, n, 64, 0.9, stretch=(1.0, 0.12))
        height = norm(0.5 * strands + 0.3 * hairs + 0.14 * fine)
        gap = np.clip((0.45 - strands) * 3, 0, 1)
    elif style == "spiny":
        d1, _, ident = voronoi(rng, n, 150)
        spine = np.clip(1.0 - d1 / (14 * s), 0, 1) ** 0.6 * (ident > 0.45)
        height = norm(0.3 * mid + 0.16 * fine + 0.9 * spine)
        gap = np.zeros_like(fine)
    elif style == "lenticel":
        d1, _, ident = voronoi(rng, n, 2600)
        lenticel = np.clip(1.0 - d1 / (2.2 * s), 0, 1) * (ident > 0.72)
        bumps = np.clip((fbm(rng, n, 30, 1.3) - 0.66) * 5, 0, 1)      # old prickle bases
        height = norm(0.45 * mid + 0.2 * fine + 0.3 * bumps - 0.2 * lenticel)
        gap = lenticel * 0.6
    elif style == "smooth":
        height = norm(0.6 * mid + 0.3 * fine)
        gap = np.zeros_like(fine)
    else:                                   # fissured
        ridge_src = fbm(rng, n, 16, 1.5, stretch=(1.0, 0.08))
        (ridge,) = warp([ridge_src], rng, n, 14 * s, octaves=10)
        cross = np.clip((fbm(rng, n, 28, 1.3, stretch=(0.25, 1.0)) - 0.6) * 4, 0, 1)
        gap = np.clip((0.5 - ridge) * 4, 0, 1)
        height = norm(0.6 * ridge + 0.16 * fine - 0.4 * gap - 0.16 * cross)
    hollow = np.clip((blur(height, max(1, int(8 * s))) - height) * 6, 0, 1)
    v = 1.0 + 0.22 * (fine - 0.5) + 0.18 * (mid - 0.5) - 0.22 * gap
    albedo = base[None, None, :] * v[..., None]
    albedo = mix(albedo, albedo * 0.6, hollow * 0.5)
    # Crustose lichen: ragged map-like colonies on the exposed faces, a few ochre.
    crust = np.clip((fbm(rng, n, 30, 1.0) + 0.3 * (fine - 0.5) - 0.52) * 6, 0, 1) * (1 - hollow)
    crust *= np.clip((fbm(rng, n, 11, 1.8) - 0.38) * 3, 0, 1) * lichen
    ochre = crust * np.clip((fbm(rng, n, 18, 1.6) - 0.62) * 6, 0, 1)
    albedo = mix(albedo, rgb(0.74 + 0.08 * fine, (1.0, 0.99, 0.93)), np.clip(crust - ochre, 0, 1) * 0.75)
    albedo = mix(albedo, rgb(0.66 + 0.06 * fine, (1.0, 0.86, 0.52)), ochre * 0.65)
    # Moss: only what gathers in the fissures and hollows, never a furry trunk.
    growth = np.clip(hollow * 1.4 + 0.8 * (fbm(rng, n, 26, 1.3) - 0.58) - 0.3, 0, 1) * moss
    albedo = mix(albedo, rgb(0.30 + 0.07 * fine, (0.70, 1.0, 0.52)), growth * 0.6)
    height = np.clip(height + 0.03 * crust + 0.05 * growth, 0, 1)
    occ = cavity(height, max(1, int(6 * s))) * (1 - 0.3 * gap)
    rough = 0.88 + 0.08 * (fine - 0.5) + 0.06 * gap - 0.06 * crust + 0.04 * growth
    # As for the leaf: no baked light means no black, and the darkest barks here
    # (the spiny palms) would otherwise dip under the validator's floor in a fissure.
    return np.clip(albedo, 0.14, 0.94), height, occ, np.clip(rough, 0.05, 1.0), np.zeros_like(v)


RECIPES = {"stone": stone, "ground": ground, "gold": gold, "gem": gem, "lapis": lapis, "jade": jade,
           "moonstone": moonstone,
           # Plant library (parameterised by the set's "args")
           "leaf": plant_leaf, "plant_bark": plant_bark}


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
    """Write one map.

    compress_level 1, not the default 6. On a 4096 map that is 0.8 s instead of 4.8 s
    and costs 3 MB of a file that is a build output, gitignored, and read exactly once
    by Godot's importer before it becomes a .ctex. Over a 36-set library that is seven
    minutes a run spent making an intermediate file smaller.
    """
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(img, mode).save(path, compress_level=1)


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
    jobs = [(index, name, spec, out, base_n, hi_n, p["seed"])
            for index, (name, spec) in enumerate(sorted(tex["sets"].items()))]
    # Every set is independent and seeded from its own index, so the bytes do not depend
    # on the order they are built in, and a machine with 32 cores has no business
    # building 36 of them one at a time: the plant library took forty minutes a run and
    # blocked every look at it behind that. Workers are capped well under the core count
    # because each 4096 set holds several hundred megabytes of float arrays at once.
    workers = min(len(jobs), max(1, (os.cpu_count() or 4) // 3), 12)
    if workers > 1:
        with multiprocessing.get_context("fork").Pool(workers) as pool:
            for line in pool.imap_unordered(_build_set, jobs):
                print(line)
    else:
        for job in jobs:
            print(_build_set(job))
    stamp.write_text(digest + "\n")


def _build_set(job):
    """Build and write one texture set. Must stay self-contained: it runs in a worker,
    and returning the maps rather than writing them would pickle hundreds of megabytes."""
    index, name, spec, out, base_n, hi_n, seed = job
    rng = np.random.default_rng([seed, index])
    recipe = RECIPES[spec["recipe"]]
    # A set may carry "args": the parameters of a parameterised recipe (the plant
    # library's leaf and bark sets, one per species). Recipes without them see none.
    args = spec.get("args", {})
    if spec.get("hi"):
        maps = recipe(rng, hi_n, **args)
        write_set(out / "hi", name, maps, hi_n, spec)
        maps = [downsample(m, hi_n // base_n) for m in maps]
        sizes = f"{hi_n} (hi) + {base_n}"
    else:
        maps = recipe(rng, base_n, **args)
        sizes = f"{base_n}"
    write_set(out, name, maps, base_n, spec)
    return f"T_{name}: {sizes}, BC/N/ORM{'/H' if spec.get('height') else ''}"


def main():
    paths = [Path(a) for a in sys.argv[1:]] or sorted((ROOT / "tools/assets/themes").glob("*.json"))
    for params_path in paths:
        generate(params_path)


if __name__ == "__main__":
    main()

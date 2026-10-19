# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Vectorised noise for the key-art generators: gradient noise, fBm, ridged and
cellular noise on world positions, plus colour-space helpers. Deterministic:
the same inputs and seeds give the same values."""
import numpy as np

_M = np.uint64(0xFFFFFFFF)


def _hash(*ints, seed=0):
    h = np.uint64(seed * 0x9E3779B1 + 0x85EBCA6B) & _M
    for k, v in zip((0x27D4EB2F, 0x165667B1, 0xD3A2646C, 0xFD7046C5), ints):
        h = (h ^ (v.astype(np.int64).astype(np.uint64) * np.uint64(k))) & _M
        h = ((h ^ (h >> np.uint64(15))) * np.uint64(0x2C1B3C6D)) & _M
        h = ((h ^ (h >> np.uint64(12))) * np.uint64(0x297A2D39)) & _M
        h = (h ^ (h >> np.uint64(15))) & _M
    return h


def _fade(t):
    return t * t * t * (t * (t * 6 - 15) + 10)


_G3 = np.array([[1, 1, 0], [-1, 1, 0], [1, -1, 0], [-1, -1, 0], [1, 0, 1], [-1, 0, 1], [1, 0, -1], [-1, 0, -1],
                [0, 1, 1], [0, -1, 1], [0, 1, -1], [0, -1, -1], [1, 1, 0], [-1, 1, 0], [0, -1, 1], [0, -1, -1]], float)


def perlin3(x, y, z, seed=0):
    """Gradient noise in about [-1, 1]."""
    x = np.asarray(x, float); y = np.asarray(y, float); z = np.asarray(z, float)
    xi = np.floor(x); yi = np.floor(y); zi = np.floor(z)
    xf = x - xi; yf = y - yi; zf = z - zi
    xi = xi.astype(np.int64); yi = yi.astype(np.int64); zi = zi.astype(np.int64)
    u = _fade(xf); v = _fade(yf); w = _fade(zf)
    out = 0.0
    acc = {}
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                g = _G3[(_hash(xi + dx, yi + dy, zi + dz, seed=seed) & np.uint64(15)).astype(np.int64)]
                d = g[..., 0] * (xf - dx) + g[..., 1] * (yf - dy) + g[..., 2] * (zf - dz)
                acc[(dx, dy, dz)] = d
    x00 = acc[0, 0, 0] + u * (acc[1, 0, 0] - acc[0, 0, 0])
    x10 = acc[0, 1, 0] + u * (acc[1, 1, 0] - acc[0, 1, 0])
    x01 = acc[0, 0, 1] + u * (acc[1, 0, 1] - acc[0, 0, 1])
    x11 = acc[0, 1, 1] + u * (acc[1, 1, 1] - acc[0, 1, 1])
    y0 = x00 + v * (x10 - x00)
    y1 = x01 + v * (x11 - x01)
    return (y0 + w * (y1 - y0)) * 1.1


def fbm3(p, freq=1.0, octaves=5, gain=0.5, lac=2.0, seed=0, scale=(1.0, 1.0, 1.0)):
    """p: (...,3) world positions. scale stretches axes (e.g. (6,0.5,6) for vertical streaks)."""
    x = p[..., 0] * scale[0]; y = p[..., 1] * scale[1]; z = p[..., 2] * scale[2]
    tot = 0.0; amp = 1.0; norm = 0.0; f = freq
    for o in range(octaves):
        tot = tot + amp * perlin3(x * f + 17.3 * o, y * f - 9.1 * o, z * f + 3.7 * o, seed=seed + o * 101)
        norm += amp; amp *= gain; f *= lac
    return tot / norm


def ridged3(p, freq=1.0, octaves=4, gain=0.5, seed=0, scale=(1, 1, 1)):
    x = p[..., 0] * scale[0]; y = p[..., 1] * scale[1]; z = p[..., 2] * scale[2]
    tot = 0.0; amp = 1.0; norm = 0.0; f = freq
    for o in range(octaves):
        n = 1.0 - np.abs(perlin3(x * f + 5.1 * o, y * f + 1.3 * o, z * f - 7.7 * o, seed=seed + 31 * o))
        tot = tot + amp * n * n
        norm += amp; amp *= gain; f *= 2.0
    return tot / norm


def worley2(x, y, seed=0, jitter=0.9):
    """2D cellular noise: returns F1, F2, cell id hash (uint64), and nearest feature offset."""
    xi = np.floor(x).astype(np.int64); yi = np.floor(y).astype(np.int64)
    f1 = np.full(np.shape(x), 9.0); f2 = np.full(np.shape(x), 9.0)
    cid = np.zeros(np.shape(x), np.uint64)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            cx = xi + dx; cy = yi + dy
            h = _hash(cx, cy, seed=seed)
            fx = cx + 0.5 + jitter * (((h & np.uint64(0xFFFF)).astype(float) / 65535.0) - 0.5)
            fy = cy + 0.5 + jitter * ((((h >> np.uint64(16)) & np.uint64(0xFFFF)).astype(float) / 65535.0) - 0.5)
            d = np.sqrt((x - fx) ** 2 + (y - fy) ** 2)
            closer = d < f1
            f2 = np.where(closer, f1, np.minimum(f2, d))
            cid = np.where(closer, h, cid)
            f1 = np.where(closer, d, f1)
    return f1, f2, cid


def hash01(h, k=0):
    return (((h >> np.uint64(k)) & np.uint64(0xFFFF)).astype(float)) / 65535.0


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def srgb2lin(c):
    c = np.asarray(c, float)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def lin2srgb(c):
    c = np.clip(np.asarray(c, float), 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)


def hexlin(h):
    return srgb2lin([int(h[i:i + 2], 16) / 255 for i in (1, 3, 5)])

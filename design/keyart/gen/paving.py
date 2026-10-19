# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Tiling basalt-slab paving texture set (6.4 m tile)."""
import numpy as np
from .noise import _hash, _fade, _G3, hash01, smoothstep, hexlin, worley2
from .atlas import save_srgb, save_u8, height_to_normal


def tnoise(x, y, per, seed=0):
    """periodic 2D gradient noise, period `per` lattice cells."""
    xi = np.floor(x).astype(np.int64); yi = np.floor(y).astype(np.int64)
    xf = x - xi; yf = y - yi
    u = _fade(xf); v = _fade(yf)
    acc = {}
    z0 = np.zeros_like(xi)
    for dx in (0, 1):
        for dy in (0, 1):
            g = _G3[(_hash((xi + dx) % per, (yi + dy) % per, z0, seed=seed) & np.uint64(15)).astype(np.int64)]
            acc[dx, dy] = g[..., 0] * (xf - dx) + g[..., 1] * (yf - dy)
    a = acc[0, 0] + u * (acc[1, 0] - acc[0, 0])
    b = acc[0, 1] + u * (acc[1, 1] - acc[0, 1])
    return (a + v * (b - a)) * 1.4


def tfbm(x, y, base, oct, seed, gain=0.5):
    t = 0; amp = 1; n = 0; per = base
    for o in range(oct):
        t = t + amp * tnoise(x * per, y * per, per, seed + o * 7)
        n += amp; amp *= gain; per *= 2
    return t / n


def tworley(x, y, per, seed=0):
    """periodic cellular F1/F2 on unit tile with per cells."""
    X = x * per; Y = y * per
    xi = np.floor(X).astype(np.int64); yi = np.floor(Y).astype(np.int64)
    f1 = np.full(X.shape, 9.0); f2 = np.full(X.shape, 9.0)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            cx = xi + dx; cy = yi + dy
            h = _hash(cx % per, cy % per, np.zeros_like(cx), seed=seed)
            fx = cx + hash01(h, 0); fy = cy + hash01(h, 16)
            d = np.hypot(X - fx, Y - fy)
            f2 = np.where(d < f1, f1, np.minimum(f2, d)); f1 = np.minimum(f1, d)
    return f1, f2


def make(out, N=2048, TILE=6.4, seed=5):
    rng = np.random.default_rng(seed)
    # rows along z
    rh = rng.uniform(0.75, 1.45, 40)
    acc = np.cumsum(rh); nrow = int(np.searchsorted(acc, TILE)) + 1
    rh = rh[:nrow] * TILE / rh[:nrow].sum()
    rows = np.concatenate([[0], np.cumsum(rh)])
    slabs = []
    for r in range(nrow):
        L = rng.uniform(0.8, 2.1, 30)
        n = int(np.searchsorted(np.cumsum(L), TILE)) + 1
        L = L[:n] * TILE / L[:n].sum()
        off = rng.uniform(0, TILE)
        slabs.append((np.concatenate([[0], np.cumsum(L)]), off))
    t = (np.arange(N) + 0.5) / N
    U, V = np.meshgrid(t, t)          # u -> x, v -> z (image down)
    x = U * TILE; z = V * TILE
    # domain-warp edges so slabs are hand-dressed, not machined
    wx = 0.035 * tfbm(U, V, 6, 3, 11); wz = 0.035 * tfbm(U, V, 6, 3, 12)
    xw = x + wx; zw = z + wz
    r = np.clip(np.searchsorted(rows, zw % TILE, side='right') - 1, 0, nrow - 1)
    dz = np.minimum(zw % TILE - rows[r], rows[r + 1] - zw % TILE)
    sid = np.zeros_like(r); dx = np.zeros(x.shape)
    for ri in range(nrow):
        m = r == ri
        b, off = slabs[ri]
        xx = (xw[m] + off) % TILE
        k = np.clip(np.searchsorted(b, xx, side='right') - 1, 0, len(b) - 2)
        sid[m] = ri * 100 + k
        dx[m] = np.minimum(xx - b[k], b[k + 1] - xx)
    d = np.minimum(dx, dz)
    # joints: 8-14 mm half width, rounded arris 2 cm
    jw = 0.013 + 0.005 * (0.5 + 0.5 * tfbm(U, V, 8, 2, 21))
    joint = smoothstep(jw + 0.004, jw, d)
    hsl = hash01(_hash(sid, np.zeros_like(sid), np.zeros_like(sid), seed=3))
    hsl2 = hash01(_hash(sid, np.zeros_like(sid), np.zeros_like(sid), seed=4))
    hsl3 = hash01(_hash(sid, np.zeros_like(sid), np.zeros_like(sid), seed=8))
    # slab height: tilt + dish, edge rounding, pits
    tilt = (hsl - 0.5) * 0.006 * (x - TILE / 2) / TILE * 4 + (hsl2 - 0.5) * 0.006 * (z - TILE / 2) / TILE * 4
    lift = (hsl3 - 0.5) * 0.004
    arris = -0.012 * np.clip(1 - (d - jw) / 0.03, 0, 1) ** 2
    grain = 0.0012 * tfbm(U, V, 64, 4, 31)
    undul = 0.003 * tfbm(U, V, 4, 3, 33)
    f1, f2 = tworley(U, V, 220, seed=41)
    pits = -0.003 * smoothstep(0.32, 0.12, f1) * smoothstep(-0.1, 0.4, tfbm(U, V, 10, 3, 43))
    chip_n = tfbm(U, V, 24, 3, 45)
    chips = -0.008 * smoothstep(0.25, 0.5, chip_n + 0.6 * np.exp(-(d - jw) / 0.03)) * (d < 0.08)
    h = tilt + lift + arris + grain + undul + pits + chips
    h = np.where(joint > 0.5, -0.02 + 0.003 * tfbm(U, V, 32, 2, 47), h)
    # albedo
    c_dark = hexlin('#2e2d2c'); c_mid = hexlin('#45433f'); c_lt = hexlin('#5d5954')
    tone = np.clip(0.5 + 1.2 * (hsl - 0.5) + 0.25 * tfbm(U, V, 12, 4, 51), 0, 1)
    alb = c_dark + (c_mid - c_dark) * smoothstep(0, 0.6, tone)[..., None] + (c_lt - c_mid) * smoothstep(0.55, 1, tone)[..., None]
    speck = smoothstep(0.25, 0.6, tfbm(U, V, 128, 2, 53))
    alb = alb * (1 + 0.18 * speck[..., None])
    alb = alb * (1 - 0.35 * smoothstep(0.0, -0.002, pits)[..., None])
    dust_c = hexlin('#857a6a')
    dust = np.clip(0.12 + 0.5 * smoothstep(-0.2, 0.5, tfbm(U, V, 3, 4, 55)) * 0.6 + 0.25 * np.exp(-(d - jw) / 0.015), 0, 0.9)
    dust = dust * (0.6 + 0.4 * smoothstep(-0.3, 0.3, tfbm(U, V, 40, 3, 57)))
    alb = alb + (dust_c - alb) * (dust * 0.45)[..., None]
    # worn, paler arris along every slab edge, then dust packed into the joint
    arr = np.clip(1 - (d - jw) / 0.045, 0, 1) * (1 - joint)
    alb = alb + (hexlin('#77706a') - alb) * (0.45 * arr)[..., None]
    jcol = hexlin('#7a6c58') * (0.85 + 0.3 * tfbm(U, V, 30, 2, 59))[..., None]
    alb = alb * (1 - joint[..., None]) + jcol * joint[..., None]
    # foot-polished slab faces give a sheen at grazing angles; dust and joints stay matt
    rough = np.clip(0.32 + 0.10 * tfbm(U, V, 20, 3, 61) + 0.08 * (1 - hsl2) + 0.35 * dust + 0.4 * joint + 0.15 * arr, 0.35, 1)
    ao = 1 - 0.5 * joint - 0.3 * smoothstep(0.0, -0.003, pits) - 0.3 * np.clip(1 - (d - jw) / 0.015, 0, 1) * (1 - joint)
    n = height_to_normal(h, TILE / N)
    save_srgb(out + '/paving_albedo.png', np.clip(alb, 0, 1))
    save_u8(out + '/paving_normal.png', ((n * 0.5 + 0.5) * 255 + 0.5).astype(np.uint8))
    orm = np.stack([np.clip(ao, 0, 1), rough, np.zeros_like(rough)], -1)
    save_u8(out + '/paving_orm.png', (orm * 255 + 0.5).astype(np.uint8))


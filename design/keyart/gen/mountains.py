# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Eastern skyline of the Basin of Mexico, scaled 1:10 in distance and height
(angles kept): low hills, the Iztaccihuatl ridge and the Popocatepetl cone."""
import numpy as np
from .noise import fbm3, ridged3, smoothstep, hexlin
from .atlas import save_srgb

HDR = '# SPDX-License-Identifier: GPL-2.0-or-later\n# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors\n'
X0, X1, Z0, Z1 = -7000.0, 7000.0, -9500.0, -2600.0
NX, NZ = 280, 110


def pos(b, d):
    # bearing from north; north = -X, east = -Z, south = +X
    r = np.radians(b)
    return -d * np.cos(r), -d * np.sin(r)


def height(x, z):
    P = np.stack([x, np.zeros_like(x), z], -1) / 1000.0
    dist = np.hypot(x, z)
    hills = 70 * ridged3(P, 0.9, 5, seed=3) * smoothstep(2800, 5000, dist)
    hills += 40 * fbm3(P, 0.35, 4, seed=4)
    h = np.maximum(hills, 0)
    # Iztaccihuatl: long N-S ridge with summits
    ix, iz = pos(101, 6600)
    t = (x - ix) / 900.0
    across = (z - iz) / 650.0
    prof = 300 * np.exp(-across ** 2) * np.exp(-(t / 1.25) ** 4)
    bumps = 1 + 0.10 * np.exp(-((t + 0.7) / 0.18) ** 2) + 0.16 * np.exp(-((t - 0.05) / 0.25) ** 2) + 0.08 * np.exp(-((t - 0.65) / 0.2) ** 2)
    h = np.maximum(h, prof * bumps + 25 * ridged3(P, 3.0, 4, seed=9) * np.exp(-across ** 2))
    # Popocatepetl: concave cone with a shallow crater
    px, pz = pos(111, 7200)
    r = np.hypot(x - px, z - pz) / 1700.0
    cone = 335 * np.clip(1 - r, 0, 1) ** 1.6 - 25 * np.exp(-(r / 0.03) ** 2)
    cone += 18 * ridged3(P, 4.0, 3, seed=11) * np.clip(1 - r, 0, 1)
    h = np.maximum(h, cone)
    return h


def make(out):
    xs = np.linspace(X0, X1, NX); zs = np.linspace(Z0, Z1, NZ)
    X, Z = np.meshgrid(xs, zs)
    Hh = height(X, Z)
    Hh = Hh - 8 * smoothstep(-3200, -2600, Z)  # sink the near edge below the plaza horizon
    gx = np.gradient(Hh, xs, axis=1); gz = np.gradient(Hh, zs, axis=0)
    N = np.stack([-gx, np.ones_like(Hh), -gz], -1); N /= np.linalg.norm(N, axis=-1, keepdims=True)
    with open(out + '/meshes/skyline_east.obj', 'w') as f:
        f.write(HDR + '# Eastern skyline: hills, Iztaccihuatl and Popocatepetl, distances and heights scaled 1:10\n')
        for j in range(NZ):
            for i in range(NX):
                f.write(f'v {X[j,i]:.1f} {Hh[j,i]:.2f} {Z[j,i]:.1f}\n')
        for j in range(NZ):
            for i in range(NX):
                f.write(f'vt {i/(NX-1):.5f} {1 - j/(NZ-1):.5f}\n')
        for j in range(NZ):
            for i in range(NX):
                n = N[j, i]; f.write(f'vn {n[0]:.3f} {n[1]:.3f} {n[2]:.3f}\n')
        for j in range(NZ - 1):
            for i in range(NX - 1):
                a = j * NX + i + 1; b = a + 1; c = a + NX + 1; d = a + NX
                f.write(f'f {a}/{a}/{a} {b}/{b}/{b} {c}/{c}/{c} {d}/{d}/{d}\n')
    # albedo, top-down
    W, H = 2048, 1024
    xs2 = np.linspace(X0, X1, W); zs2 = np.linspace(Z0, Z1, H)
    X2, Z2 = np.meshgrid(xs2, zs2)
    h2 = height(X2, Z2)
    gx = np.gradient(h2, xs2, axis=1); gz = np.gradient(h2, zs2, axis=0)
    slope = np.hypot(gx, gz)
    P = np.stack([X2, np.zeros_like(X2), Z2], -1) / 1000.0
    rock = hexlin('#6b6258'); dry = hexlin('#8a7a62'); forest = hexlin('#3b3f2e'); snow = hexlin('#e8e6e2')
    alb = dry + (rock - dry) * smoothstep(0.2, 0.6, slope)[..., None]
    fz = smoothstep(60, 110, h2) * smoothstep(240, 190, h2) * smoothstep(-0.2, 0.2, fbm3(P, 3.0, 4, seed=21))
    alb = alb + (forest - alb) * (0.8 * fz)[..., None]
    sn = smoothstep(205, 235, h2 + 25 * fbm3(P, 6.0, 4, seed=23) - 60 * np.clip(slope - 0.5, 0, 1))
    streak = smoothstep(-0.1, 0.3, fbm3(P * np.array([6, 1, 1]), 2.0, 4, seed=25))
    sn = np.clip(sn * (0.75 + 0.25 * streak), 0, 1)
    alb = alb + (snow - alb) * sn[..., None]
    alb = alb * (0.85 + 0.25 * fbm3(P, 8.0, 3, seed=27))[..., None]
    save_srgb(out + '/textures/skyline_albedo.png', np.clip(alb, 0, 1))


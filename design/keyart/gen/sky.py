# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Golden-hour sky panorama and a high-resolution cloud sheet for the plaza still."""
import numpy as np
import multiprocessing as mp
from .noise import fbm3, smoothstep, lin2srgb
from .atlas import save_u8
from PIL import Image

SUN = None
HDR = '# SPDX-License-Identifier: GPL-2.0-or-later\n# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors\n'


def sun_dir(az, el):
    a = np.radians(az); e = np.radians(el)
    return np.array([-np.sin(a) * np.cos(e), np.sin(e), np.cos(a) * np.cos(e)])


def sky_rgb(d, s):
    mu = np.clip((d * s).sum(-1), -1, 1)
    ang = np.arccos(mu)
    y = d[..., 1]
    yp = np.clip(y, 0, 1)
    zen = np.array([0.045, 0.13, 0.42])
    mid = np.array([0.16, 0.34, 0.70])
    hor_far = np.array([0.66, 0.70, 0.76])
    hor_sun = np.array([1.35, 0.86, 0.50])
    sunside = smoothstep(-0.3, 1.0, mu)[..., None]
    hor = hor_far + (hor_sun - hor_far) * sunside
    t1 = (1 - yp) ** 6
    t2 = (1 - yp) ** 2
    sky = zen * (1 - t2[..., None]) + mid * t2[..., None]
    sky = sky * (1 - t1[..., None]) + hor * t1[..., None]
    belt = np.exp(-((y - 0.05) / 0.045) ** 2) * smoothstep(0.1, -0.7, mu)
    sky = sky + np.array([0.16, 0.06, 0.08]) * belt[..., None]
    sky = sky + np.array([1.6, 0.95, 0.45]) * (np.exp(-ang / 0.10) * 1.2 + np.exp(-ang / 0.45) * 0.35)[..., None]
    sky = sky + np.array([40.0, 30.0, 18.0]) * smoothstep(0.0095, 0.0085, ang)[..., None]
    gnd = np.array([0.30, 0.26, 0.22])
    gb = smoothstep(0.0, -0.06, y)[..., None]
    return sky * (1 - gb) + gnd * gb, hor


def clouds(d, s, seed=0):
    """A thin cloud layer at 4 km with light marched toward the sun through the layer's
    density (self-shadowing, darker thick bases, bright thin edges): colour, alpha, fade."""
    y = d[..., 1]
    hgt = 4.0
    tt = hgt / np.maximum(y, 0.015)
    P = np.stack([d[..., 0] * tt, np.zeros_like(tt), d[..., 2] * tt], -1)
    warp = np.stack([fbm3(P, 0.05, 3, seed=seed + 5), np.zeros_like(tt), fbm3(P, 0.05, 3, seed=seed + 6)], -1)
    Q = P + warp * 2.5

    def dens_at(Q, octaves=8):
        big = fbm3(Q, 0.025, 3, seed=seed + 1)
        dd = fbm3(Q, 0.14, octaves, seed=seed + 2, scale=(1.0, 1.0, 1.4), gain=0.55)
        billow = np.abs(fbm3(Q, 0.5, max(octaves - 4, 2), seed=seed + 3))
        return dd + 0.75 * big - 0.12 * billow - 0.14
    dens = dens_at(Q)
    thick = np.clip(dens - 0.06, 0, None) * 6.0          # optical thickness of the layer
    alpha = 1 - np.exp(-thick * 2.2)
    sh = np.array([s[0], 0, s[2]]); sh = sh / np.linalg.norm(sh)
    acc = np.zeros_like(dens)
    for i in range(1, 6):
        acc += np.clip(dens_at(Q + sh * 0.35 * i, 5) - 0.06, 0, None)
    tr = np.exp(-acc * 2.6)                               # light reaching this column from the sun
    mu = np.clip((d * s).sum(-1), -1, 1)
    phase = 0.6 + 0.9 * np.exp(-np.arccos(mu) / 0.5)
    powder = 1 - np.exp(-thick * 4.0)
    sun_c = np.array([1.75, 1.12, 0.74]); amb_top = np.array([0.42, 0.52, 0.72]); amb_base = np.array([0.30, 0.28, 0.33])
    ao = np.exp(-thick * 0.9)
    amb = amb_base + (amb_top - amb_base) * ao[..., None]
    ccol = sun_c * (tr * phase * (0.35 + 0.65 * powder))[..., None] + amb * (0.55 + 0.45 * ao)[..., None]
    fade = np.exp(-tt / 70.0)
    return ccol, alpha * fade * (y > 0), fade


def _pano_rows(args):
    v0, v1, W, H, s = args
    u = (np.arange(W) + 0.5) / W; v = (np.arange(v0, v1) + 0.5) / H
    U, V = np.meshgrid(u, v)
    lon = (U - 0.5) * 2 * np.pi; lat = (0.5 - V) * np.pi
    d = np.stack([np.sin(lon) * np.cos(lat), np.sin(lat), -np.cos(lon) * np.cos(lat)], -1)
    rgb, hor = sky_rgb(d, s)
    if CLOUDS_IN_PANO:
        ccol, a, fade = clouds(d, s)
        ccol = ccol * fade[..., None] + hor * 0.95 * (1 - fade[..., None])
        rgb = rgb * (1 - a[..., None]) + ccol * a[..., None]
    return rgb


CLOUDS_IN_PANO = True
SUN_AZ, SUN_EL = 62.0, 9.0


def pano(path, az, el, W=4096, H=2048, scale=0.5):
    s = sun_dir(az, el)
    chunks = [(i, min(i + 64, H), W, H, s) for i in range(0, H, 64)]
    with mp.get_context('fork').Pool(24) as p:
        rows = p.map(_pano_rows, chunks)
    rgb = np.concatenate(rows, 0)
    save_u8(path, (lin2srgb(np.clip(rgb * scale, 0, 1)) * 255 + 0.5).astype(np.uint8))


# cloud sheet: a spherical patch around the view direction (-Z), az in [-A, A], el in [0, E]
A_DEG, E_DEG = 70.0, 50.0


def _sheet_rows(args):
    j0, j1, W, H, s = args
    az = np.radians((np.arange(W) + 0.5) / W * 2 * A_DEG - A_DEG)
    el = np.radians((1 - (np.arange(j0, j1) + 0.5) / H) * E_DEG)
    AZ, EL = np.meshgrid(az, el)
    d = np.stack([np.sin(AZ) * np.cos(EL), np.sin(EL), -np.cos(AZ) * np.cos(EL)], -1)
    ccol, a, fade = clouds(d, s)
    _, hor = sky_rgb(d, s)
    ccol = ccol * fade[..., None] + hor * 0.95 * (1 - fade[..., None])
    a = a * smoothstep(0.0, 0.02, d[..., 1])
    return np.concatenate([ccol, a[..., None]], -1)


def sheet(out, az, el, W=8192, H=2944, scale=0.5, R=15000.0):
    s = sun_dir(az, el)
    chunks = [(i, min(i + 32, H), W, H, s) for i in range(0, H, 32)]
    with mp.get_context('fork').Pool(24) as p:
        rows = p.map(_sheet_rows, chunks)
    img = np.concatenate(rows, 0)
    rgb = (lin2srgb(np.clip(img[..., :3] * scale, 0, 1)) * 255 + 0.5).astype(np.uint8)
    al = (np.clip(img[..., 3], 0, 1) * 255 + 0.5).astype(np.uint8)
    Image.fromarray(np.dstack([rgb, al]), 'RGBA').save(out + '/textures/clouds_sheet.png', compress_level=6)
    # mesh: sphere patch, centred on the origin (the scene places it at the camera)
    na, ne = 96, 40
    with open(out + '/meshes/cloud_sheet.obj', 'w') as f:
        f.write(HDR + f'# Cloud sheet: sphere patch r={R:.0f} m, azimuth +-{A_DEG:.0f} deg about -Z, elevation 0-{E_DEG:.0f} deg\n')
        for j in range(ne + 1):
            for i in range(na + 1):
                azr = np.radians(i / na * 2 * A_DEG - A_DEG); elr = np.radians(j / ne * E_DEG)
                f.write(f'v {R*np.sin(azr)*np.cos(elr):.1f} {R*np.sin(elr):.1f} {-R*np.cos(azr)*np.cos(elr):.1f}\n')
        for j in range(ne + 1):
            for i in range(na + 1):
                f.write(f'vt {i/na:.5f} {j/ne:.5f}\n')
        f.write('vn 0 0 1\n')
        for j in range(ne):
            for i in range(na):
                a = j * (na + 1) + i + 1; b = a + 1; c = a + na + 2; dd = a + na + 1
                f.write(f'f {a}/{a}/1 {b}/{b}/1 {c}/{c}/1 {dd}/{dd}/1\n')


def studio_env(path, W=2048, H=1024):
    """Reflection environment for the gold title: dark warm surround, a bright
    low sun-side band and two large soft boxes, so polished metal shows
    contrast instead of one flat sky colour."""
    u = (np.arange(W) + 0.5) / W; v = (np.arange(H) + 0.5) / H
    U, V = np.meshgrid(u, v)
    lon = (U - 0.5) * 2 * np.pi; lat = (0.5 - V) * np.pi
    d = np.stack([np.sin(lon) * np.cos(lat), np.sin(lat), -np.cos(lon) * np.cos(lat)], -1)
    rgb = np.zeros(d.shape) + np.array([0.035, 0.026, 0.02])
    rgb += np.array([0.10, 0.07, 0.05]) * smoothstep(-0.3, 0.6, d[..., 1])[..., None]
    s = sun_dir(58.0, 14.0)
    mu = (d * s).sum(-1)
    band = np.exp(-((d[..., 1] - 0.12) / 0.10) ** 2) * smoothstep(-0.2, 0.7, mu)
    rgb += np.array([1.4, 0.95, 0.55]) * band[..., None]
    for c, w, col in (((-0.45, 0.55, 0.70), 0.22, (1.6, 1.25, 0.9)), ((0.55, 0.75, 0.37), 0.18, (0.8, 0.75, 0.7))):
        c = np.array(c) / np.linalg.norm(c)
        a = np.arccos(np.clip((d * c).sum(-1), -1, 1))
        rgb += np.array(col) * smoothstep(w, w * 0.8, a)[..., None]
    # wide soft boxes front and back, graded top to bottom and broken into soft
    # vertical strips: the polished faces of a frontal sign reflect one of them,
    # so the letters pick up bright and dark bands like cast metal under studio light
    box = np.maximum(smoothstep(0.8, 0.6, np.pi - np.abs(lon)), smoothstep(0.8, 0.6, np.abs(lon)))
    box = box * smoothstep(-0.05, 0.08, d[..., 1]) * smoothstep(0.8, 0.55, d[..., 1])
    strips = 0.25 + 0.75 * (0.5 + 0.5 * np.cos(lon * 40.0)) ** 2
    grad = 0.4 + 1.1 * smoothstep(0.02, 0.45, d[..., 1])
    rgb += np.array([1.7, 1.25, 0.75]) * (box * grad * strips)[..., None]
    save_u8(path, (lin2srgb(np.clip(rgb * 0.5, 0, 1)) * 255 + 0.5).astype(np.uint8))


def make(out):
    studio_env(out + '/textures/title_env.png')
    pano(out + '/textures/sky_golden.png', SUN_AZ, SUN_EL)
    sheet(out, SUN_AZ, SUN_EL)

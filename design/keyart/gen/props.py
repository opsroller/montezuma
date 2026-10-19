# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Props: hourglass temple brazier (form after OB-05), ember bed, reed mat
(petate) texture, paper banner on a pole."""
import numpy as np
from .noise import hexlin, smoothstep
from .atlas import save_srgb, save_u8, height_to_normal
from .paving import tfbm

HDR = '# SPDX-License-Identifier: GPL-2.0-or-later\n# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors\n'


def lathe(path, prof, seg=48, note=''):
    prof = np.asarray(prof, float)
    L = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(prof, axis=0), axis=1))])
    # smooth normals along profile
    t = np.gradient(prof, axis=0); t /= np.linalg.norm(t, axis=1, keepdims=True)
    n2 = np.stack([t[:, 1], -t[:, 0]], 1)  # outward for profile going up the outside
    vs = []; vt = []; vn = []; fs = []
    for i in range(seg + 1):
        a = 2 * np.pi * i / seg
        c, s = np.cos(a), np.sin(a)
        for k, (r, y) in enumerate(prof):
            vs.append((r * c, y, r * s)); vt.append((i / seg, L[k] / L[-1]))
            vn.append((n2[k, 0] * c, n2[k, 1], n2[k, 0] * s))
    m = len(prof)
    for i in range(seg):
        for k in range(m - 1):
            a = i * m + k + 1; b = (i + 1) * m + k + 1
            fs.append((a, a + 1, b + 1, b))
    with open(path, 'w') as f:
        f.write(HDR + note)
        for v in vs: f.write('v %.4f %.4f %.4f\n' % v)
        for v in vt: f.write('vt %.5f %.5f\n' % v)
        for v in vn: f.write('vn %.4f %.4f %.4f\n' % v)
        for q in fs:
            # winding chosen to match the temple meshes (counter-clockwise from outside)
            f.write('f ' + ' '.join(f'{i}/{i}/{i}' for i in q) + '\n')


def brazier(out):
    # hourglass temple brazier (after OB-05 form, scaled up for a temple door), metres
    pr = [(0.0, 0.0), (0.40, 0.0), (0.42, 0.03), (0.40, 0.07), (0.33, 0.20), (0.24, 0.38), (0.21, 0.48),
          (0.23, 0.56), (0.30, 0.68), (0.40, 0.82), (0.49, 0.95), (0.53, 1.02), (0.55, 1.07), (0.54, 1.10),
          (0.50, 1.10), (0.47, 1.05), (0.40, 0.98), (0.0, 0.96)]
    lathe(out + '/meshes/brazier.obj', pr, 64, '# Hourglass temple brazier, 1.1 m (form after OB-05), lathe\n')


def ember(out):
    pr = [(0.0, 0.0), (0.46, 0.0), (0.44, 0.04), (0.30, 0.09), (0.12, 0.12), (0.0, 0.13)]
    lathe(out + '/meshes/ember_bed.obj', pr, 48, '# Glowing charcoal heap inside the brazier bowl\n')


def petate(out, N=1024, TILE=0.6):
    """twill-woven reed mat, tiling 0.6 m."""
    t = (np.arange(N) + 0.5) / N
    U, V = np.meshgrid(t, t)
    x = U * TILE; y = V * TILE
    w = 0.025  # strip width
    ix = np.floor(x / w).astype(int); iy = np.floor(y / w).astype(int)
    fx = x / w - ix; fy = y / w - iy
    over = ((ix + iy) // 2) % 2 == 0  # 2/2 twill
    # height: strip crown across its width
    hx = np.sin(np.pi * fy) ; hy = np.sin(np.pi * fx)
    h = np.where(over, 0.0015 * hx + 0.0006, 0.0015 * hy)
    edge = np.where(over, smoothstep(0.0, 0.12, np.minimum(fy, 1 - fy)), smoothstep(0.0, 0.12, np.minimum(fx, 1 - fx)))
    fib = tfbm(U, V, 96, 3, 7) * np.where(over, 1, 0) + tfbm(V, U, 96, 3, 9) * np.where(over, 0, 1)
    base = hexlin('#b79a62')
    tone = 1 + 0.12 * tfbm(U, V, 8, 3, 13) + 0.08 * fib
    sun = np.where(over, 1.0, 0.86)
    alb = base * (tone * sun * (0.55 + 0.45 * edge))[..., None]
    h = h + 0.0002 * fib
    n = height_to_normal(h, TILE / N)
    save_srgb(out + '/textures/petate_albedo.png', np.clip(alb, 0, 1))
    save_u8(out + '/textures/petate_normal.png', ((n * 0.5 + 0.5) * 255 + 0.5).astype(np.uint8))




def banner(out):
    """paper banner (amapanitl-like streamer) on a pole: pole + crossbar + hanging strip, metres."""
    vs = []; vt = []; fs = []
    # strip: 0.7 m wide, 3.4 m long, hangs from y=6.6 at x in [0.05, 0.75], blown gently to +X/-Z
    nu, nv = 6, 28
    base = len(vs)
    for j in range(nv + 1):
        t = j / nv
        for i in range(nu + 1):
            u = i / nu
            x = 0.05 + 0.5 * u + 0.35 * t ** 1.5
            y = 9.6 - 4.6 * t * (1 - 0.04 * t)
            z = 0.06 * np.sin(np.pi * u) * t + 0.10 * np.sin(2.2 * np.pi * t + 1.0 * u) * t
            vs.append((x, y, z)); vt.append((u, 1 - t))
    for j in range(nv):
        for i in range(nu):
            a = base + j * (nu + 1) + i + 1
            fs.append(('strip', (a, a + 1, a + nu + 2, a + nu + 1)))
    pole = []
    seg = 10
    for k, (r, y0, y1) in enumerate(((0.07, 0.0, 10.0),)):
        b = len(vs)
        for i in range(seg + 1):
            a_ = 2 * np.pi * i / seg
            vs.append((r * np.cos(a_), y0, r * np.sin(a_))); vt.append((i / seg, 0))
            vs.append((r * np.cos(a_), y1, r * np.sin(a_))); vt.append((i / seg, 1))
        for i in range(seg):
            a = b + 2 * i + 1
            fs.append(('pole', (a, a + 1, a + 3, a + 2)))
    # crossbar
    b = len(vs)
    for (x, y, z) in ((0, 9.62, -0.03), (0.6, 9.62, -0.03), (0.6, 9.68, -0.03), (0, 9.68, -0.03),
                      (0, 9.62, 0.03), (0.6, 9.62, 0.03), (0.6, 9.68, 0.03), (0, 9.68, 0.03)):
        vs.append((x, y, z)); vt.append((0, 0))
    for q in ((3, 2, 1, 0), (4, 5, 6, 7), (0, 1, 5, 4), (7, 6, 2, 3)):
        fs.append(('pole', tuple(b + 1 + i for i in q)))
    with open(out + '/meshes/banner.obj', 'w') as f:
        f.write(HDR + '# Paper banner on a pole (streamer hung from a crossbar), 10 m pole; surface 0 strip, 1 pole\n')
        for v in vs: f.write('v %.4f %.4f %.4f\n' % v)
        for v in vt: f.write('vt %.5f %.5f\n' % v)
        cur = None
        for grp, q in sorted(fs, key=lambda x: x[0] != 'strip'):
            if grp != cur:
                f.write(f'usemtl {grp}\n'); cur = grp
            f.write('f ' + ' '.join(f'{i}/{i}' for i in q) + '\n')


def banner_tex(out, accent, name, W=256, H=1024):
    t = (np.arange(W) + 0.5) / W; s = (np.arange(H) + 0.5) / H
    U, V = np.meshgrid(t, s)
    paper = hexlin('#e4dccb')
    fib = tfbm(U, V * 4, 16, 4, 3) * 0.06 + tfbm(U * 8, V, 8, 3, 5) * 0.04
    alb = paper * (1 + fib)[..., None]
    bands = [(0.03, 0.06, accent)]
    for a, b, c in bands:
        m = smoothstep(a - 0.004, a, V) * smoothstep(b + 0.004, b, V)
        alb = alb * (1 - m[..., None]) + c * (1 + fib)[..., None] * m[..., None]
    edge = smoothstep(0.0, 0.03, np.minimum(U, 1 - U))
    alb = alb * (0.85 + 0.15 * edge)[..., None]
    save_srgb(out + f'/textures/{name}.png', np.clip(alb, 0, 1))



def make(out):
    brazier(out); ember(out); petate(out); banner(out)
    banner_tex(out, hexlin('#3f7f86'), 'banner_blue'); banner_tex(out, hexlin('#aa7a3a'), 'banner_ochre')

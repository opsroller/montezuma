# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Invented background buildings of the sacred precinct, generic forms only:
a smaller west-facing temple, a palace platform with a range of rooms and the
precinct wall, with inferred stepped roof crests."""
import numpy as np
from .atlas import Extrusion, Planar
from .temple import chamfer, PAINT


def rect_sweep(prefix, x0, x1, z0, z1, prof, pk, pp, dens, dfac=None, overrides=None, extra=None, skip_front=None):
    """sweep a profile around an axis-aligned rectangle; returns groups. dfac per side."""
    dfac = dfac or {'front': 1, 'right': 1.5, 'back': 4, 'left': 1.5}
    sides = [((x0, z1), (x1, z1), (0, 0, 1), 'front'), ((x1, z1), (x1, z0), (1, 0, 0), 'right'),
             ((x1, z0), (x0, z0), (0, 0, -1), 'back'), ((x0, z0), (x0, z1), (-1, 0, 0), 'left')]
    out = []
    for (ax, az), (bx, bz), o, nm in sides:
        if nm == 'front' and skip_front is not None:
            # split the front around a stair block [sx0, sx1]
            sx0, sx1 = skip_front
            for (qa, qb, m0, m1, ec, tag) in (((x0, z1), (sx0, z1), 1, 0, (True, False), 'fl'), ((sx1, z1), (x1, z1), 0, 1, (False, True), 'fr')):
                A = np.array([qb[0] - qa[0], 0, 0.0]); L = np.linalg.norm(A)
                out.append(Extrusion(f'{prefix}_{tag}', (qa[0], 0, qa[1]), A, o, (0, 1, 0), prof, 0, L, m0, m1, pp, pk,
                                     end_convex=ec, dens=dens * dfac['front'], extra=extra))
            continue
        A = np.array([bx - ax, 0, bz - az], float); L = np.linalg.norm(A)
        ov = (overrides or {}).get(nm)
        out.append(Extrusion(f'{prefix}_{nm}', (ax, 0, az), A, o, (0, 1, 0), prof, 0, L, 1, 1, pp, pk,
                             end_convex=(True, True), overrides=ov, dens=dens * dfac[nm], extra=extra))
    return out


def tier_profile(nt, th, hc, batter, pc, ter, y0=0.0):
    s = (th - hc) / np.tan(np.radians(batter))
    db = lambda i: (s - pc) + (nt - 1 - i) * (s - pc + ter)
    pts = []; kinds = []; paints = []
    for i in range(nt):
        yb = y0 + i * th
        if i == 0:
            pts.append((db(i), y0))
        pts += [(db(i) - s, yb + th - hc), (db(i) - s + pc, yb + th - hc), (db(i) - s + pc, yb + th)]
        kinds += ['talud', 'soffit', 'cornice']; paints += [PAINT['red'], PAINT['lime'], PAINT['lime']]
        if i < nt - 1:
            pts.append((db(i + 1), yb + th)); kinds.append('terrace'); paints.append(PAINT['lime'])
    p, k, pp = chamfer(pts, 0.04, kinds, paints)
    return p, k, pp, db(0)


def crest_row(name, pa, pb, outward, y0, dens, th=0.3, pitch=1.3, hb=0.22, w1=0.86, h1=0.32, w2=0.46, h2=0.3):
    """A row of stepped crests (merlons) on a roof edge from pa to pb (x, z), facing outward."""
    out = np.array(outward, float)
    E1 = np.cross([0.0, 1.0, 0.0], out); E1 /= np.linalg.norm(E1)
    a = np.array([pa[0], 0.0, pa[1]]); b = np.array([pb[0], 0.0, pb[1]])
    if np.dot(b - a, E1) < 0:
        a, b = b, a
    L = float(np.dot(b - a, E1))
    n = max(int(L // pitch), 1)
    m = (L - n * pitch) / 2
    ytop = y0 + hb
    pts = [(L, y0), (L, ytop)]
    for k in reversed(range(n)):
        c = m + (k + 0.5) * pitch
        pts += [(c + w1 / 2, ytop), (c + w1 / 2, ytop + h1), (c + w2 / 2, ytop + h1), (c + w2 / 2, ytop + h1 + h2),
                (c - w2 / 2, ytop + h1 + h2), (c - w2 / 2, ytop + h1), (c - w1 / 2, ytop + h1), (c - w1 / 2, ytop)]
    pts += [(0.0, ytop), (0.0, y0)]
    K = len(pts) - 1
    groups = [Extrusion(name, a, -out, E1, (0, 1, 0), pts, 0.0, th, 0, 0, [PAINT['lime']] * K, ['cap'] * K,
                        end_convex=(True, True), dens=dens)]
    for side, off, nn in (('f', 0.0, out), ('b', th, -out)):
        poly = [a - out * off + E1 * e1 + np.array([0, e2, 0]) for (e1, e2) in pts]
        g = Planar(f'{name}{side}', [poly], E1, (0, 1, 0), paint=PAINT['lime'], kind='cap',
                   convex_edges=[[True] * (len(poly) - 1) + [None]], dens=dens)
        g.set_normal(nn)
        groups.append(g)
    return groups


def crest_rect(prefix, x0, x1, z0, z1, y0, dens, inset=0.15):
    x0 += inset; x1 -= inset; z0 += inset; z1 -= inset
    gs = []
    for nm, pa, pb, o in (('f', (x0, z1), (x1, z1), (0, 0, 1)), ('b', (x0, z0), (x1, z0), (0, 0, -1)),
                          ('l', (x0, z0 + 0.3), (x0, z1 - 0.3), (-1, 0, 0)), ('r', (x1, z0 + 0.3), (x1, z1 - 0.3), (1, 0, 0))):
        gs += crest_row(f'{prefix}_cr{nm}', pa, pb, o, y0, dens)
    return gs


def generic_temple(prefix, cx, cz, nt=3, th=4.0, xt=9.0, zt=8.0, sw=7.0, aw=1.0, shrine_col=PAINT['lime'], dens=0.04):
    groups = []
    prof, pk, pp, d0 = tier_profile(nt, th, 0.45, 76, 0.25, 1.2)
    HP = nt * th
    x0, x1, z0, z1 = cx - xt, cx + xt, cz - zt, cz + zt
    sx0, sx1 = cx - sw / 2 - aw, cx + sw / 2 + aw
    groups += rect_sweep(prefix, x0, x1, z0, z1, prof, pk, pp, dens, skip_front=(sx0, sx1))
    rise, tread = 0.28, 0.25
    n = int(round(HP / rise)); rise = HP / n
    zf = z1 + n * tread
    pts = [(zf, 0.0)]; kinds = []; paints = []
    for i in range(n):
        z = zf - i * tread
        pts += [(z, (i + 1) * rise), (z - tread, (i + 1) * rise)]
        kinds += ['riser', 'tread']; paints += [PAINT['lime'], PAINT['lime']]
    ps, ks, pps = chamfer(pts, 0.03, kinds, paints)
    stairs = [(cx - sw / 2, cx + sw / 2)]
    groups.append(Extrusion(f'{prefix}_stair', (0, 0, 0), (1, 0, 0), (0, 0, 1), (0, 1, 0), ps, cx - sw / 2, cx + sw / 2, 0, 0,
                            pps, ks, end_convex=(False, False), dens=dens * 0.8, extra={'stairs': stairs}))
    # balustrades: constant slope, small dado at top
    up = 0.8
    ny = lambda z: rise + (zf - z) * (rise / tread)
    apts = [(zf + 0.3, 0.0), (zf + 0.3, ny(zf + 0.3) + up), (z1 + 1.2, ny(z1 + 1.2) + up), (z1 + 0.6, HP + 1.4),
            (z1 - 0.6, HP + 1.4), (z1 - 0.6, HP)]
    aprof, ak, ap = chamfer(apts, 0.04, ['alf_side', 'alf_top', 'alf_top', 'alf_top', 'alf_side'], [PAINT['red']] * 5)
    for ai, (xa, xb) in enumerate(((sx0, sx0 + aw), (sx1 - aw, sx1))):
        groups.append(Extrusion(f'{prefix}_alf{ai}', (0, 0, 0), (1, 0, 0), (0, 0, 1), (0, 1, 0), aprof, xa, xb, 0, 0, ap, ak,
                                end_convex=(True, True), dens=dens))
        poly2 = [tuple(p) for p in aprof] + [(z1 - 1.0, 0.0)]
        for x, sgn in ((xa, -1.0), (xb, 1.0)):
            poly = [np.array([x, y, z]) for (z, y) in poly2]
            g = Planar(f'{prefix}_alf{ai}{int(sgn)}', [poly], (0, 0, -1), (0, 1, 0), paint=PAINT['red'], kind='alf_side',
                       convex_edges=[[True] * (len(poly2) - 2) + [False, False]], dens=dens * 1.3)
            g.set_normal((sgn, 0, 0)); groups.append(g)
    e = 0.04
    fl = [np.array(p) for p in ((x0 + e, HP, z0 + e), (x1 - e, HP, z0 + e), (x1 - e, HP, z1 - e), (x0 + e, HP, z1 - e))]
    g = Planar(f'{prefix}_top', [fl], (1, 0, 0), (0, 0, -1), paint=PAINT['lime'], kind='floor', convex_edges=[[None] * 4], dens=dens * 2)
    g.set_normal((0, 1, 0)); groups.append(g)
    # single shrine
    hw, hd = xt * 0.62, zt * 0.55
    scz = cz - zt * 0.2
    sp = [(0.4, HP), (0.4, HP + 0.4), (0.0, HP + 0.4), (0.0, HP + 5.5), (0.1, HP + 5.5), (0.1, HP + 7.3), (0.5, HP + 7.3),
          (0.5, HP + 7.7), (-0.4, HP + 7.7)]
    sk = ['plinth', 'floor', 'shrine_wall', 'soffit', 'frieze', 'soffit', 'cornice', 'cap']
    spp = [PAINT['lime'], PAINT['lime'], shrine_col, PAINT['lime'], PAINT['frieze'], PAINT['lime'], PAINT['lime'], PAINT['lime']]
    sprof, skk, sppp = chamfer(sp, 0.03, sk, spp)
    kw = skk.index('shrine_wall')
    Lw = sprof[kw + 1][1] - sprof[kw][1]
    W = 2 * hw
    ov = {'front': {kw: [(0, W / 2 - 1.0, 0, Lw), (W / 2 + 1.0, W, 0, Lw), (W / 2 - 1.0, W / 2 + 1.0, 3.2, Lw)]}}
    groups += rect_sweep(prefix + '_sh', cx - hw, cx + hw, scz - hd, scz + hd, sprof, skk, sppp, dens * 0.7,
                         overrides=ov, extra={'accent': shrine_col, 'frieze_y0': HP + 5.5, 'frieze_h': 1.8})
    yt = HP + 7.7
    rt = [np.array(p) for p in ((cx - hw + 0.4, yt, scz - hd + 0.4), (cx + hw - 0.4, yt, scz - hd + 0.4), (cx + hw - 0.4, yt, scz + hd - 0.4), (cx - hw + 0.4, yt, scz + hd - 0.4))]
    groups += crest_rect(prefix + '_shc', cx - hw + 0.4, cx + hw - 0.4, scz - hd + 0.4, scz + hd - 0.4, yt, dens * 0.8)
    g = Planar(f'{prefix}_shroof', [rt], (1, 0, 0), (0, 0, -1), paint=PAINT['lime'], kind='cap', convex_edges=[[None] * 4], dens=dens * 2)
    g.set_normal((0, 1, 0)); groups.append(g)
    dz = scz + hd - 0.8
    door = [np.array(p) for p in ((cx - 1.0, HP + 0.4, dz), (cx + 1.0, HP + 0.4, dz), (cx + 1.0, HP + 3.6, dz), (cx - 1.0, HP + 3.6, dz))]
    g = Planar(f'{prefix}_door', [door], (1, 0, 0), (0, 1, 0), paint=PAINT['soot'], kind='interior', convex_edges=[[False] * 4], dens=0.05)
    g.set_normal((0, 0, 1)); groups.append(g)
    for side, xx, n in (('l', cx - 1.0, (1, 0, 0)), ('r', cx + 1.0, (-1, 0, 0))):
        jp = [np.array(p) for p in ((xx, HP + 0.4, scz + hd), (xx, HP + 0.4, dz), (xx, HP + 3.6, dz), (xx, HP + 3.6, scz + hd))]
        g = Planar(f'{prefix}_j{side}', [jp], (0, 0, -1), (0, 1, 0), paint=PAINT['lime'], kind='jamb', convex_edges=[[None, False, None, True]], dens=0.03)
        g.set_normal(n); groups.append(g)
    lp = [np.array(p) for p in ((cx - 1.0, HP + 3.6, scz + hd), (cx + 1.0, HP + 3.6, scz + hd), (cx + 1.0, HP + 3.6, dz), (cx - 1.0, HP + 3.6, dz))]
    g = Planar(f'{prefix}_lin', [lp], (1, 0, 0), (0, 0, 1), paint=PAINT['lime'], kind='soffit', convex_edges=[[True, None, None, None]], dens=0.03)
    g.set_normal((0, -1, 0)); groups.append(g)

    def floor_fn(x, z):
        d = np.maximum(np.abs(x - cx) - xt, np.abs(z - cz) - zt)
        h = np.zeros_like(x)
        s = (th - 0.45) / np.tan(np.radians(76))
        for i in range(nt):
            dcf = (nt - 1 - i) * (s - 0.25 + 1.2)
            h = np.where(d <= dcf, (i + 1) * th, h)
        st = (np.abs(x - cx) < sw / 2 + aw) & (z > z1) & (z < zf)
        h = np.where(st, np.maximum(h, np.clip(rise * np.ceil((zf - z) / tread), 0, HP)), h)
        return h
    for g in groups:
        g.floor_fn = floor_fn
    return groups


def palace(prefix, cx, cz, hl, hd, dens=0.05):
    """long platform (two tiers) with a range of white rooms, red door frames painted as bands."""
    groups = []
    prof, pk, pp, d0 = tier_profile(2, 2.4, 0.4, 76, 0.2, 1.4)
    HP = 4.8
    groups += rect_sweep(prefix, cx - hl, cx + hl, cz - hd, cz + hd, prof, pk, pp, dens)
    e = 0.04
    fl = [np.array(p) for p in ((cx - hl + e, HP, cz - hd + e), (cx + hl - e, HP, cz - hd + e), (cx + hl - e, HP, cz + hd - e), (cx - hl + e, HP, cz + hd - e))]
    g = Planar(f'{prefix}_top', [fl], (1, 0, 0), (0, 0, -1), paint=PAINT['lime'], kind='floor', convex_edges=[[None] * 4], dens=dens * 2)
    g.set_normal((0, 1, 0)); groups.append(g)
    # the hall: walls face -X (toward the temple plaza)
    bx0, bx1, bz0, bz1 = cx - hl + 3.0, cx + hl - 6.0, cz - hd + 3.0, cz + hd - 3.0
    sp = [(0.0, HP), (0.0, HP + 4.2), (0.1, HP + 4.2), (0.1, HP + 5.2), (0.45, HP + 5.2), (0.45, HP + 5.5), (-0.3, HP + 5.5)]
    sk = ['shrine_wall', 'soffit', 'frieze', 'soffit', 'cornice', 'cap']
    spp = [PAINT['lime'], PAINT['lime'], PAINT['red'], PAINT['lime'], PAINT['lime'], PAINT['lime']]
    sprof, skk, sppp = chamfer(sp, 0.03, sk, spp)
    kw = skk.index('shrine_wall'); Lw = sprof[kw + 1][1] - sprof[kw][1]
    L = bz1 - bz0
    ndoor = int(L // 7)
    doors = [bz0 + (k + 0.5) * L / ndoor for k in range(ndoor)]
    # the left (-X) side runs from z0 to z1; a = z - bz0
    rects = []; a = 0.0
    for dzc in doors:
        da = dzc - bz0
        rects.append((a, da - 0.9, 0, Lw)); rects.append((da - 0.9, da + 0.9, 2.6, Lw)); a = da + 0.9
    rects.append((a, L, 0, Lw))
    ov = {'left': {kw: rects}}
    hall = rect_sweep(prefix + '_hall', bx0, bx1, bz0, bz1, sprof, skk, sppp, dens * 0.8, overrides=ov,
                      dfac={'front': 1.5, 'right': 4, 'back': 1.5, 'left': 1})
    groups += hall
    yt = HP + 5.5
    rt = [np.array(p) for p in ((bx0 + 0.3, yt, bz0 + 0.3), (bx1 - 0.3, yt, bz0 + 0.3), (bx1 - 0.3, yt, bz1 - 0.3), (bx0 + 0.3, yt, bz1 - 0.3))]
    groups += crest_rect(prefix + '_hc', bx0 + 0.3, bx1 - 0.3, bz0 + 0.3, bz1 - 0.3, yt, dens)
    g = Planar(f'{prefix}_roof', [rt], (1, 0, 0), (0, 0, -1), paint=PAINT['lime'], kind='cap', convex_edges=[[None] * 4], dens=dens * 3)
    g.set_normal((0, 1, 0)); groups.append(g)
    for k, dzc in enumerate(doors):
        dp = [np.array(p) for p in ((bx0 + 0.9, HP, dzc + 0.9), (bx0 + 0.9, HP, dzc - 0.9), (bx0 + 0.9, HP + 2.6, dzc - 0.9), (bx0 + 0.9, HP + 2.6, dzc + 0.9))]
        g = Planar(f'{prefix}_d{k}', [dp], (0, 0, -1), (0, 1, 0), paint=PAINT['soot'], kind='interior', convex_edges=[[False] * 4], dens=0.06)
        g.set_normal((-1, 0, 0)); groups.append(g)
        for zz, n in ((dzc - 0.9, (0, 0, 1)), (dzc + 0.9, (0, 0, -1))):
            jp = [np.array(p) for p in ((bx0, HP, zz), (bx0 + 0.9, HP, zz), (bx0 + 0.9, HP + 2.6, zz), (bx0, HP + 2.6, zz))]
            g = Planar(f'{prefix}_j{k}{n[2]}', [jp], (1, 0, 0), (0, 1, 0), paint=PAINT['red'], kind='jamb', convex_edges=[[None, False, None, True]], dens=0.04)
            g.set_normal(n); groups.append(g)
        lp = [np.array(p) for p in ((bx0, HP + 2.6, dzc - 0.9), (bx0, HP + 2.6, dzc + 0.9), (bx0 + 0.9, HP + 2.6, dzc + 0.9), (bx0 + 0.9, HP + 2.6, dzc - 0.9))]
        g = Planar(f'{prefix}_l{k}', [lp], (0, 0, 1), (1, 0, 0), paint=PAINT['red'], kind='soffit', convex_edges=[[True, None, None, None]], dens=0.04)
        g.set_normal((0, -1, 0)); groups.append(g)
    s = (2.4 - 0.4) / np.tan(np.radians(76))

    def floor_fn(x, z):
        d = np.maximum(np.abs(x - cx) - hl, np.abs(z - cz) - hd)
        h = np.zeros_like(x)
        for i in range(2):
            h = np.where(d <= (1 - i) * (s - 0.2 + 1.4), (i + 1) * 2.4, h)
        return h
    for g in groups:
        g.floor_fn = floor_fn
    return groups


def wall(prefix, length=40.0, dens=0.05):
    pts = [(1.0, 0.0), (0.7, 1.1), (0.75, 1.1), (0.75, 1.25), (0.5, 1.25), (0.5, 3.6), (0.62, 3.6), (0.62, 3.85), (-0.6, 3.85)]
    kinds = ['talud', 'soffit', 'cornice', 'terrace', 'shrine_wall', 'soffit', 'cornice', 'cap']
    paints = [PAINT['red'], PAINT['lime'], PAINT['lime'], PAINT['lime'], PAINT['lime'], PAINT['lime'], PAINT['lime'], PAINT['lime']]
    p, kk, pp = chamfer(pts, 0.03, kinds, paints)
    g = Extrusion(prefix, (-length / 2, 0, 0), (1, 0, 0), (0, 0, 1), (0, 1, 0), p, 0, length, 0, 0, pp, kk,
                  end_convex=(None, None), dens=dens)
    g.floor_fn = lambda x, zz: np.zeros_like(x)
    return [g]


def build_parts():
    """each part centred on its own origin, sharing one atlas."""
    return {
        'temple_small': generic_temple('tS', 0.0, 0.0, nt=3, th=4.2, xt=10.0, zt=9.0, sw=7.0, aw=1.0, shrine_col=PAINT['lime']),
        'palace': palace('pa', 0.0, 0.0, 34.0, 13.0),
        'wall': wall('wall'),
    }

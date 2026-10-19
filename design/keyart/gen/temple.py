# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""The twin temple (Templo Mayor, Stage VII as inferred; see
docs/reference/history-mexica-art.md, Artist brief): geometry and the shared
painted-stucco material bake used by every building in the key art."""
import numpy as np
from .atlas import Extrusion, Planar, autopack, write_obj, height_to_normal, save_srgb, save_u8, KIND
from .noise import fbm3, ridged3, worley2, hash01, smoothstep, hexlin, perlin3, _hash

# ---------------------------------------------------------------- dimensions (metres)
H_T = 7.0          # tier height
H_C = 0.7          # cornice (moulding) height
H_TAL = H_T - H_C
BATTER = np.radians(76.0)
S_IN = H_TAL / np.tan(BATTER)  # talud inset
P_C = 0.35         # cornice projection
TER = 1.6          # terrace width
NT = 4
HP = NT * H_T      # platform height 28
XT, ZT = 19.0, 13.0  # top platform half extents


def dbase(i):
    return (S_IN - P_C) + (NT - 1 - i) * (S_IN - P_C + TER)


def dcf(i):  # cornice front offset of tier i
    return dbase(i) - S_IN + P_C


# stairs
RISE, TREAD, NSTEP = 0.28, 0.23, 100
Z_TOP = ZT
Z_FRONT = Z_TOP + NSTEP * TREAD   # 49.2
ALF_W = 1.8
ST_W = 11.0
X_IN = 1.8   # central double alfarda half width
STAIRS = [(-X_IN - ST_W, -X_IN), (X_IN, X_IN + ST_W)]
X_OUT = X_IN + ST_W + ALF_W  # 14.6
ALF_UP = 1.0
Z_ALF_FRONT = Z_FRONT + 0.4


def nosing_y(z):
    return RISE + (Z_FRONT - z) * (RISE / TREAD)


# shrines
SH_HW, SH_HD = 6.3, 6.0
SH_CZ = 2.0
SH_CX = (-(X_IN + ST_W / 2), (X_IN + ST_W / 2))
PLINTH_H, PLINTH_M = 0.6, 0.5
WALL_H = 8.0
FRIEZE_H, FRIEZE_P = 2.8, 0.12
CORN_H, CORN_P = 0.5, 0.72
ROOF_H, ROOF_IN = 1.5, 0.4
CAP_H, CAP_P = 0.35, 0.1
DOOR_W, DOOR_H = 2.4, 4.0
WALL_T = 1.5

PAINT = {'lime': 0, 'red': 1, 'blue': 2, 'ochre': 3, 'soot': 4, 'wine': 5, 'frieze': 6}


def chamfer(pts, c, kinds, paints, only_convex=True, skip_first=True):
    """chamfer convex corners of an open polyline (normal on right of travel)."""
    pts = [np.asarray(p, float) for p in pts]
    out = [pts[0]]; ok = []; op = []
    for i in range(1, len(pts) - 1):
        t0 = pts[i] - pts[i - 1]; t1 = pts[i + 1] - pts[i]
        L0 = np.linalg.norm(t0); L1 = np.linalg.norm(t1)
        cr = t0[0] * t1[1] - t0[1] * t1[0]
        cc = c if np.ndim(c) == 0 else c[i]
        if cr > 1e-9 and cc > 0:
            out.append(pts[i] - t0 / L0 * min(cc, L0 * 0.45))
            ok.append(kinds[i - 1]); op.append(paints[i - 1])
            out.append(pts[i] + t1 / L1 * min(cc, L1 * 0.45))
            ok.append('chamfer'); op.append(0 if 0 in (paints[i], paints[i - 1]) else paints[i - 1])
        else:
            out.append(pts[i]); ok.append(kinds[i - 1]); op.append(paints[i - 1])
    out.append(pts[-1]); ok.append(kinds[-1]); op.append(paints[-1])
    return np.array(out), ok, op


def pyramid_profile():
    pts = []; kinds = []; paints = []
    for i in range(NT):
        y0 = i * H_T; db = dbase(i)
        if i == 0:
            pts.append((db, 0.0))
        pts.append((db - S_IN, y0 + H_TAL)); kinds.append('talud'); paints.append(PAINT['red'])
        pts.append((db - S_IN + P_C, y0 + H_TAL)); kinds.append('soffit'); paints.append(PAINT['lime'])
        pts.append((db - S_IN + P_C, y0 + H_T)); kinds.append('cornice'); paints.append(PAINT['lime'])
        if i < NT - 1:
            pts.append((dbase(i + 1), y0 + H_T)); kinds.append('terrace'); paints.append(PAINT['lime'])
    return chamfer(pts, 0.05, kinds, paints)


def pyramid_floor_d(d):
    """height of the walkable top surface of the platform at outward offset d."""
    h = np.zeros_like(d)
    for i in range(NT):
        h = np.where(d <= dcf(i), (i + 1) * H_T, h)
    return h


def alfarda_profile():
    z_c_top = 24.8
    zc = Z_FRONT - (z_c_top - ALF_UP - RISE) / (RISE / TREAD)
    y_d = HP + 2.6
    z_d = zc - (y_d - z_c_top) / np.tan(np.radians(76))
    z_back = Z_TOP - 0.6
    y_front = nosing_y(Z_ALF_FRONT) + ALF_UP
    pts = [(Z_ALF_FRONT, 0.0), (Z_ALF_FRONT, y_front), (zc, z_c_top), (z_d, y_d), (z_back, y_d), (z_back, HP)]
    kinds = ['alf_side', 'alf_top', 'alf_top', 'alf_top', 'alf_side']
    paints = [PAINT['red']] * 5
    return pts, kinds, paints


def floor_h(x, z):
    """walkable surface height under (x, z): plaza, terraces, stairs, alfardas, platform, plinths."""
    d = np.maximum(np.abs(x) - XT, np.abs(z) - ZT)
    h = pyramid_floor_d(d)
    st = (np.abs(x) < X_OUT) & (z > Z_TOP) & (z < Z_ALF_FRONT)
    steps = np.clip(RISE * np.ceil((Z_FRONT - z) / TREAD), 0, HP)
    h = np.where(st, np.maximum(h, steps), h)
    for cx in SH_CX:
        inp = (np.abs(x - cx) < SH_HW + PLINTH_M) & (np.abs(z - SH_CZ) < SH_HD + PLINTH_M)
        h = np.where(inp, HP + PLINTH_H, h)
    return h


# ---------------------------------------------------------------- groups
def build():
    groups = []
    prof, pk, pp = pyramid_profile()
    E2 = (0, 1, 0)
    # pyramid sides: (start, end, outward, m0, m1, end_convex, density factor)
    sides = [((-XT, ZT), (-X_OUT, ZT), (0, 0, 1), 1, 0, (True, False), 1.0, 'pyr_front_l'),
             ((X_OUT, ZT), (XT, ZT), (0, 0, 1), 0, 1, (False, True), 1.0, 'pyr_front_r'),
             ((XT, ZT), (XT, -ZT), (1, 0, 0), 1, 1, (True, True), 1.6, 'pyr_right'),
             ((XT, -ZT), (-XT, -ZT), (0, 0, -1), 1, 1, (True, True), 4.0, 'pyr_back'),
             ((-XT, -ZT), (-XT, ZT), (-1, 0, 0), 1, 1, (True, True), 1.4, 'pyr_left')]
    for (x0, z0), (x1, z1), out, m0, m1, ec, df, name in sides:
        A = np.array([x1 - x0, 0, z1 - z0], float); L = np.linalg.norm(A)
        g = Extrusion(name, (x0, 0, z0), A, out, E2, prof, 0.0, L, m0, m1, pp, pk, end_convex=ec,
                      dens=0.022 * df)
        groups.append(g)
    # stairs
    for si, (xa, xb) in enumerate(STAIRS):
        pts = [(Z_FRONT, 0.0)]; kinds = []; paints = []
        for i in range(NSTEP):
            z = Z_FRONT - i * TREAD
            pts.append((z, (i + 1) * RISE)); kinds.append('riser'); paints.append(PAINT['lime'])
            pts.append((z - TREAD, (i + 1) * RISE)); kinds.append('tread'); paints.append(PAINT['lime'])
        pts = [(p[0], p[1]) for p in pts]
        # profile coords are (z, y); E1 = +Z
        prof_s, ks, ps = chamfer(pts, 0.05, kinds, paints)
        CH = 120
        for c0 in range(0, len(ks), CH):
            c1 = min(c0 + CH, len(ks))
            g = Extrusion(f'stair{si}_{c0}', (0, 0, 0), (1, 0, 0), (0, 0, 1), E2, prof_s[c0:c1 + 1], xa, xb, 0, 0,
                          ps[c0:c1], ks[c0:c1], end_convex=(False, False), dens=0.02)
            groups.append(g)
    # alfardas
    apts, ak, ap = alfarda_profile()
    aprof, ak2, ap2 = chamfer(apts, 0.05, ak, ap)
    alf_x = [(-X_OUT, -X_OUT + ALF_W), (-X_IN, -0.05), (0.05, X_IN), (X_OUT - ALF_W, X_OUT)]
    for ai, (xa, xb) in enumerate(alf_x):
        g = Extrusion(f'alf{ai}', (0, 0, 0), (1, 0, 0), (0, 0, 1), E2, aprof, xa, xb, 0, 0, ap2, ak2,
                      end_convex=(True, True), dens=0.022)
        groups.append(g)
        # side faces: closed polygon in (z, y), plus inside closure
        poly2 = [tuple(p) for p in aprof] + [(Z_TOP - 0.6, 0.0)]
        for side, x in (('l', xa), ('r', xb)):
            outward = np.array([-1.0 if side == 'l' else 1.0, 0, 0])
            visible_outer = (ai == 0 and side == 'l') or (ai == 3 and side == 'r')
            poly = [np.array([x, y, z]) for (z, y) in poly2]
            ne = len(poly2)
            cvx = [True] * (ne - 2) + [False, False]
            pg = Planar(f'alf{ai}{side}', [poly], (0, 0, -1), (0, 1, 0), paint=PAINT['red'], kind='alf_side',
                        convex_edges=[cvx], dens=0.03 if visible_outer else 0.05)
            pg.set_normal(outward)
            groups.append(pg)
    # top platform floor
    e = 0.05
    fl = [np.array(p) for p in ((-XT + e, HP, -ZT + e), (XT - e, HP, -ZT + e), (XT - e, HP, ZT - e), (-XT + e, HP, ZT - e))]
    pg = Planar('platform', [fl], (1, 0, 0), (0, 0, -1), paint=PAINT['lime'], kind='floor',
                convex_edges=[[None] * 4], dens=0.04)
    pg.set_normal((0, 1, 0))
    groups.append(pg)
    return groups


def shrine_profile():
    y = HP
    pts = [(PLINTH_M, y), (PLINTH_M, y + PLINTH_H), (0.0, y + PLINTH_H), (0.0, y + PLINTH_H + WALL_H)]
    kinds = ['plinth', 'floor', 'shrine_wall']
    paints = [PAINT['lime'], PAINT['lime'], -1]
    yw = y + PLINTH_H + WALL_H
    pts += [(FRIEZE_P, yw), (FRIEZE_P, yw + FRIEZE_H), (CORN_P, yw + FRIEZE_H), (CORN_P, yw + FRIEZE_H + CORN_H),
            (-ROOF_IN, yw + FRIEZE_H + CORN_H), (-ROOF_IN, yw + FRIEZE_H + CORN_H + ROOF_H),
            (CAP_P, yw + FRIEZE_H + CORN_H + ROOF_H), (CAP_P, yw + FRIEZE_H + CORN_H + ROOF_H + CAP_H),
            (-0.5, yw + FRIEZE_H + CORN_H + ROOF_H + CAP_H)]
    kinds += ['soffit', 'frieze', 'soffit', 'cornice', 'terrace', 'roof', 'soffit', 'cap', 'cap']
    paints += [PAINT['lime'], PAINT['frieze'], PAINT['lime'], PAINT['lime'], PAINT['lime'], PAINT['red'],
               PAINT['lime'], PAINT['lime'], PAINT['lime']]
    return pts, kinds, paints


def build_shrines():
    groups = []
    pts, kinds, paints = shrine_profile()
    for si, cx in enumerate(SH_CX):
        col = PAINT['blue'] if si == 0 else PAINT['ochre']
        pp = [col if p == -1 else p for p in paints]
        prof, pk, pp2 = chamfer(pts, 0.04, kinds, pp)
        kw = pk.index('shrine_wall')
        x0, x1, z0, z1 = cx - SH_HW, cx + SH_HW, SH_CZ - SH_HD, SH_CZ + SH_HD
        sides = [((x0, z1), (x1, z1), (0, 0, 1), 'front', 1.0), ((x1, z1), (x1, z0), (1, 0, 0), 'right', 1.3),
                 ((x1, z0), (x0, z0), (0, 0, -1), 'back', 3.0), ((x0, z0), (x0, z1), (-1, 0, 0), 'left', 1.2)]
        for (ax, az), (bx, bz), out, nm, df in sides:
            A = np.array([bx - ax, 0, bz - az], float); L = np.linalg.norm(A)
            ov = {}
            if nm == 'front':
                Lw = prof[kw + 1][1] - prof[kw][1]
                c = L / 2
                ov[kw] = [(0.0, c - DOOR_W / 2, 0.0, Lw), (c + DOOR_W / 2, L, 0.0, Lw), (c - DOOR_W / 2, c + DOOR_W / 2, DOOR_H, Lw)]
            g = Extrusion(f'sh{si}_{nm}', (ax, 0, az), A, out, (0, 1, 0), prof, 0.0, L, 1, 1, pp2, pk,
                          end_convex=(True, True), overrides=ov, dens=0.012 * df, extra={'accent': col})
            groups.append(g)
        # roof top
        yt = prof[-1][1]; r = 0.5
        rt = [np.array(p) for p in ((x0 + r, yt, z0 + r), (x1 - r, yt, z0 + r), (x1 - r, yt, z1 - r), (x0 + r, yt, z1 - r))]
        g = Planar(f'sh{si}_rooftop', [rt], (1, 0, 0), (0, 0, -1), paint=PAINT['lime'], kind='cap',
                   convex_edges=[[None] * 4], dens=0.03)
        g.set_normal((0, 1, 0)); groups.append(g)
        # door reveal and interior
        yb = HP + PLINTH_H; yd = yb + DOOR_H
        dl, dr = cx - DOOR_W / 2, cx + DOOR_W / 2
        zf, zb = z1, z1 - WALL_T
        jl = [np.array(p) for p in ((dl, yb, zf), (dl, yb, zb), (dl, yd, zb), (dl, yd, zf))]
        g = Planar(f'sh{si}_jl', [jl], (0, 0, -1), (0, 1, 0), paint=PAINT['lime'], kind='jamb',
                   convex_edges=[[False, None, False, True]], dens=0.012)
        g.set_normal((1, 0, 0)); groups.append(g)
        jr = [np.array(p) for p in ((dr, yb, zf), (dr, yb, zb), (dr, yd, zb), (dr, yd, zf))]
        g = Planar(f'sh{si}_jr', [jr], (0, 0, 1), (0, 1, 0), paint=PAINT['lime'], kind='jamb',
                   convex_edges=[[False, None, False, True]], dens=0.012)
        g.set_normal((-1, 0, 0)); groups.append(g)
        so = [np.array(p) for p in ((dl, yd, zf), (dr, yd, zf), (dr, yd, zb), (dl, yd, zb))]
        g = Planar(f'sh{si}_lintel', [so], (1, 0, 0), (0, 0, 1), paint=PAINT['lime'], kind='soffit',
                   convex_edges=[[True, False, None, False]], dens=0.012)
        g.set_normal((0, -1, 0)); groups.append(g)
        th = [np.array(p) for p in ((dl, yb, zf), (dr, yb, zf), (dr, yb, zb), (dl, yb, zb))]
        g = Planar(f'sh{si}_thresh', [th], (1, 0, 0), (0, 0, -1), paint=PAINT['lime'], kind='floor',
                   convex_edges=[[True, False, None, False]], dens=0.012)
        g.set_normal((0, 1, 0)); groups.append(g)
        # interior room
        rx0, rx1, rz0, rz1, ry1 = cx - 3.6, cx + 3.6, zb - 4.5, zb, yb + 5.2
        faces = [
            ('back', ((rx0, yb, rz0), (rx1, yb, rz0), (rx1, ry1, rz0), (rx0, ry1, rz0)), (1, 0, 0), (0, 1, 0), (0, 0, 1)),
            ('l', ((rx0, yb, rz0), (rx0, yb, rz1), (rx0, ry1, rz1), (rx0, ry1, rz0)), (0, 0, 1), (0, 1, 0), (1, 0, 0)),
            ('r', ((rx1, yb, rz0), (rx1, yb, rz1), (rx1, ry1, rz1), (rx1, ry1, rz0)), (0, 0, -1), (0, 1, 0), (-1, 0, 0)),
            ('ceil', ((rx0, ry1, rz0), (rx1, ry1, rz0), (rx1, ry1, rz1), (rx0, ry1, rz1)), (1, 0, 0), (0, 0, 1), (0, -1, 0)),
            ('floor', ((rx0, yb, rz0), (rx1, yb, rz0), (rx1, yb, rz1), (rx0, yb, rz1)), (1, 0, 0), (0, 0, -1), (0, 1, 0)),
        ]
        for nm, poly, U, V, n in faces:
            g = Planar(f'sh{si}_in_{nm}', [[np.array(p, float) for p in poly]], U, V, paint=PAINT['soot'],
                       kind='interior', convex_edges=[[False] * 4], dens=0.05)
            g.set_normal(n); groups.append(g)
        # inner face of the front wall, around the door
        fw = [((rx0, yb, rz1), (dl, yb, rz1), (dl, ry1, rz1), (rx0, ry1, rz1)),
              ((dr, yb, rz1), (rx1, yb, rz1), (rx1, ry1, rz1), (dr, ry1, rz1)),
              ((dl, yd, rz1), (dr, yd, rz1), (dr, ry1, rz1), (dl, ry1, rz1))]
        g = Planar(f'sh{si}_in_front', [[np.array(p, float) for p in q] for q in fw], (-1, 0, 0), (0, 1, 0),
                   paint=PAINT['soot'], kind='interior', convex_edges=[[False] * 4] * 3, dens=0.05)
        g.set_normal((0, 0, -1)); groups.append(g)
    return groups


# ---------------------------------------------------------------- materials
PAL = {
    0: hexlin('#cfc6b3'),   # lime wash
    1: hexlin('#7e3429'),   # hematite red
    2: hexlin('#3f7f86'),   # maya blue
    3: hexlin('#aa7a3a'),   # ochre
    4: hexlin('#2a2420'),   # soot interior
    5: hexlin('#5e2222'),   # wine
}
PLASTER = hexlin('#c4baa8')
PLASTER_D = hexlin('#a89d8a')
TEZONTLE = hexlin('#5a3a30')
BASALT = hexlin('#4a4744')
DUST = hexlin('#9c8b74')
GRIME = hexlin('#4e463c')


def frieze_paint(y, accent, y0=None, fh=None):
    y0 = HP + PLINTH_H + WALL_H if y0 is None else y0
    fh = FRIEZE_H if fh is None else fh
    yr = (y - y0) / fh
    p = np.full(y.shape, PAINT['lime'])
    p = np.where((yr > 0.18) & (yr < 0.40), PAINT['red'], p)
    p = np.where((yr > 0.56) & (yr < 0.84), accent, p)
    p = np.where((yr < 0.06) | (yr > 0.95), PAINT['soot'], p)
    p = np.where(((yr > 0.42) & (yr < 0.46)) | ((yr > 0.50) & (yr < 0.54)), PAINT['soot'], p)
    return p


def bake_group(g, info, hfloor):
    P = info['P']; N = info['N']; paint = info['paint'].copy(); kind = info['kind']
    dconv = info['dconv']; dconc = info['dconc']
    sh = P.shape[:2]
    if 'accent' in g.extra:
        fz = paint == PAINT['frieze']
        if fz.any():
            paint = np.where(fz, frieze_paint(P[..., 1], g.extra['accent'], g.extra.get('frieze_y0'), g.extra.get('frieze_h')), paint)
    paint = np.where(paint == PAINT['frieze'], PAINT['lime'], paint)
    base = np.zeros(sh + (3,))
    for k, c in PAL.items():
        base[paint == k] = c
    up = np.clip(N[..., 1], 0, 1)
    vert = 1 - np.abs(N[..., 1])
    is_tread = kind == KIND['tread']
    is_int = kind == KIND['interior']

    # paint: flat, with only slow large-scale fading. Lime paint chalks and pales
    # where sun and rain reach it most: tops, sun-facing (west and south) faces, high up.
    m0 = fbm3(P, 0.04, 3, seed=2)
    m1 = fbm3(P, 0.3, 3, seed=3)
    base = base * (1 + 0.06 * m0[..., None] + 0.025 * m1[..., None])
    sunward = np.clip(N[..., 2] * 0.6 + N[..., 0] * 0.3 + N[..., 1] * 0.8, 0, 1)
    fade = 0.06 + 0.16 * sunward + 0.12 * smoothstep(2.0, 34.0, P[..., 1]) + 0.05 * (0.5 + 0.5 * m0)
    lum = base.mean(-1, keepdims=True)
    base = base + (lum * 1.12 - base) * fade[..., None] * 0.55
    base = base * (1 + 0.10 * fade[..., None])

    # wear: crisp chips on convex edges and arrises only, never mid-face
    nw = fbm3(P, 1.6, 4, seed=7)
    nw2 = fbm3(P, 9.0, 3, seed=8)
    edge = np.exp(-dconv / 0.04)
    traffic = np.zeros(sh)
    if is_tread.any() or (kind == KIND['chamfer']).any():
        xs = P[..., 0]
        for (xa, xb) in g.extra.get('stairs', STAIRS):
            c = (xa + xb) / 2
            traffic += np.exp(-((xs - c) / 2.4) ** 2) * ((xs > xa) & (xs < xb))
    stairish = is_tread | (kind == KIND['riser']) | ((kind == KIND['chamfer']) & (traffic > 0))
    w = edge * (0.5 + 0.9 * (0.5 + nw2) + 0.25 * nw) + 0.35 * traffic * edge * (kind == KIND['chamfer'])
    paint_loss = smoothstep(0.80, 0.86, w)
    spall = np.zeros(sh)
    paint_loss = np.where(is_int, paint_loss * 0.3, paint_loss)

    plaster = PLASTER + (PLASTER_D - PLASTER) * (0.5 + 0.5 * fbm3(P, 3.0, 3, seed=12))[..., None]
    # under-stone: tezontle for the body, basalt mixed in
    sel = smoothstep(-0.1, 0.1, fbm3(P, 0.5, 2, seed=14))
    stone = TEZONTLE + (BASALT - TEZONTLE) * sel[..., None]
    # stone coursing visible in spalls: rows 0.42 m, blocks 0.3-0.7 m
    row = np.floor(P[..., 1] / 0.42)
    hz = P[..., 0] * np.abs(N[..., 2]) + P[..., 2] * np.abs(N[..., 0]) + (P[..., 0] + P[..., 2]) * np.abs(N[..., 1])
    off = (row * 0.37) % 1.0
    bl = (hz / 0.55 + off)
    bfrac = bl - np.floor(bl)
    yfrac = P[..., 1] / 0.42 - row
    joint = np.maximum(smoothstep(0.06, 0.0, np.minimum(bfrac, 1 - bfrac) * 0.55),
                       smoothstep(0.05, 0.0, np.minimum(yfrac, 1 - yfrac) * 0.42))
    joint = np.where(up > 0.7, 0, joint)
    stone_var = 0.78 + 0.44 * hash01(_hash(np.floor(bl).astype(np.int64), row.astype(np.int64), seed=21))
    stone = stone * (stone_var[..., None]) * (1 - 0.45 * joint[..., None]) + hexlin('#8a8070') * 0.45 * joint[..., None] * 0.5

    pl = 0.4 * paint_loss
    alb = base * (1 - pl[..., None]) + plaster * pl[..., None]
    alb = alb * (1 - spall[..., None]) + stone * spall[..., None]
    # small crisp chips through to the tezontle core, on the sharpest arrises
    chip = smoothstep(0.98, 1.02, w + 0.25 * np.exp(-dconv / 0.02)) * (~is_int)
    alb = alb * (1 - chip[..., None]) + stone * 0.85 * chip[..., None]
    spall = chip * 0.6

    # runoff: vertical grime streaks that start under the moulding drip lines
    wallish = np.isin(kind, [KIND['talud'], KIND['alf_side'], KIND['shrine_wall'], KIND['wall'], KIND['plinth'], KIND['frieze']])
    under = np.exp(-np.minimum(info['s_rem'], 30) / 2.5) * wallish
    st = fbm3(P, 1.0, 4, seed=11, scale=(3.2, 0.12, 3.2))
    run = smoothstep(0.05, 0.40, st) * (0.25 + 0.75 * under) * vert
    alb = alb + (GRIME * 0.85 - alb) * (0.38 * run)[..., None]

    # damp grime at the foot of every wall, about 0.5 m deep, with a soft ragged top
    hf = np.clip(hfloor, 0, 50)
    damp = smoothstep(0.62, 0.28, hf + 0.10 * fbm3(P, 2.5, 3, seed=17)) * vert
    alb = alb + (hexlin('#43382f') - alb) * (0.62 * damp)[..., None]
    splash = damp

    # dust on up-facing surfaces: smooth, heavier in inner corners and at the sides
    # of the stairs, swept clean along the central walking line
    dn = fbm3(P, 0.12, 3, seed=19)
    dust = up * np.clip(0.14 + 0.4 * np.exp(-dconc / 0.3) + 0.06 * dn - 0.22 * traffic * is_tread, 0, 0.8)
    alb = alb + (DUST - alb) * dust[..., None]
    # walked line: worn smoother and slightly paler on treads and nosings
    alb = np.where(stairish[..., None], alb + (PLASTER * 1.05 - alb) * (0.18 * traffic)[..., None], alb)
    alb = np.where(is_int[..., None], alb * 0.9, alb)
    rpm = np.zeros(sh)

    # roughness
    rough = 0.84 + 0.05 * fbm3(P, 5.0, 2, seed=23) + 0.08 * fbm3(P, 0.4, 3, seed=24)
    rough = rough + 0.08 * spall + 0.08 * dust + 0.06 * run + 0.05 * splash - 0.18 * traffic * is_tread - 0.05 * rpm
    rough = np.clip(rough, 0.55, 1.0)

    # height (metres)
    stepk = (is_tread | (kind == KIND['riser']) | (kind == KIND['chamfer']))
    h = 0.004 * fbm3(P, 0.8, 4, seed=25) + 0.0012 * fbm3(P, 6.0, 3, seed=26) + 0.03 * fbm3(P, 0.22, 3, seed=29) * (~stepk)
    h = h - 0.0006 * paint_loss - 0.012 * spall - 0.004 * joint * spall
    r_e = 0.035 + 0.03 * traffic
    h = h - 0.02 * np.clip(1 - dconv / r_e, 0, 1) ** 2
    # hairline cracks
    uvx = hz; uvy = P[..., 1] + P[..., 2] * up
    f1, f2, _ = worley2(uvx / 1.6, uvy / 1.1, seed=31)
    crack = smoothstep(0.03, 0.0, f2 - f1) * smoothstep(0.2, 0.5, fbm3(P, 0.6, 3, seed=33)) * (~(is_tread | (kind == KIND['riser'])))
    h = h - 0.0025 * crack
    alb = alb * (1 - 0.12 * crack[..., None])

    ao = 1 - 0.45 * np.exp(-dconc / 0.25) - 0.25 * spall * 0.5
    ao = np.where(is_int, ao * 0.7, ao)
    return alb, rough, np.clip(ao, 0, 1), h


_G = None


def _bake_one(i):
    g = _G[i]
    info = g.texel_info()
    P = info['P']; N = info['N']
    nh = N.copy(); nh[..., 1] = 0
    ln = np.linalg.norm(nh, axis=-1, keepdims=True)
    nh = np.where(ln > 1e-3, nh / np.maximum(ln, 1e-6), 0)
    Q = P + nh * 0.4
    hfl = P[..., 1] - getattr(g, 'floor_fn', floor_h)(Q[..., 0], Q[..., 2])
    hfl = np.where(N[..., 1] > 0.7, 0.0, hfl)
    a, r, ao, hh = bake_group(g, info, hfl)
    n = height_to_normal(hh, g.dens, seg=info['seg'] if np.ndim(info['seg']) else None)
    return a.astype(np.float32), r.astype(np.float32), ao.astype(np.float32), n.astype(np.float32)


def bake(groups, W, H, prefix):
    global _G
    import multiprocessing as mp
    _G = groups
    alb = np.zeros((H, W, 3), np.float32); nor = np.zeros((H, W, 3), np.float32); nor[..., 2] = 1
    orm = np.zeros((H, W, 3), np.float32); orm[..., 0] = 1; orm[..., 1] = 0.9
    order = sorted(range(len(groups)), key=lambda i: -groups[i].rect[2] * groups[i].rect[3])
    with mp.get_context('fork').Pool(24) as pool:
        res = pool.map(_bake_one, order, chunksize=1)
    for i, (a, r, ao, n) in zip(order, res):
        x0, y0, w, h = groups[i].rect
        alb[y0:y0 + h, x0:x0 + w] = a
        nor[y0:y0 + h, x0:x0 + w] = n
        orm[y0:y0 + h, x0:x0 + w, 0] = ao
        orm[y0:y0 + h, x0:x0 + w, 1] = r
    save_srgb(prefix + '_albedo.png', np.clip(alb, 0, 1))
    save_u8(prefix + '_normal.png', ((nor * 0.5 + 0.5) * 255 + 0.5).astype(np.uint8))
    save_u8(prefix + '_orm.png', (np.clip(orm, 0, 1) * 255 + 0.5).astype(np.uint8))

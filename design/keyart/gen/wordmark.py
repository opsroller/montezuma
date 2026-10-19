# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""The drawn Montezuma's wordmark (design/logo/wordmark.svg, written by
design/identity/build.py) as a solid mesh: each glyph's straight-segment
outline extruded, with a 45-degree chamfer round the front face so polished
metal catches crisp edge highlights.

Writes meshes/wordmark.obj with three surfaces: 0 front faces, 1 bevel,
2 side walls. 1 unit = 1 m; the baseline is y = 0, the word is centred on
x = 0 and the front face looks down +Z."""

import re
from pathlib import Path

import numpy as np

from .atlas import earclip

HDR = '# SPDX-License-Identifier: GPL-2.0-or-later\n# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors\n'
SVG = Path(__file__).resolve().parents[2] / 'logo' / 'wordmark.svg'
CAP = 100.0          # cap height in wordmark units
SCALE = 0.0085       # metres per unit: cap height 0.85 m
DEPTH = 0.12 * CAP   # extrusion, 0.12 of the cap height
BEVEL = 0.05 * CAP   # 45-degree chamfer on the front edge


def subpaths(svg_text):
    d = re.search(r'\sd="([^"]+)"', svg_text).group(1)
    out = []
    for chunk in d.split('M')[1:]:
        chunk = chunk.replace('Z', ' ').strip()
        pts = [tuple(float(v) for v in p.split(',')) for p in chunk.split()]
        out.append([(x, CAP - y) for x, y in pts])   # y up, baseline 0
    return out


def area(p):
    return sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(p, p[1:] + p[:1])) / 2


def inside(pt, poly):
    x, y = pt; c = False
    for (x0, y0), (x1, y1) in zip(poly, poly[1:] + poly[:1]):
        if (y0 > y) != (y1 > y) and x < x0 + (y - y0) * (x1 - x0) / (y1 - y0):
            c = not c
    return c


def glyphs(paths):
    """group outlines into (outer, [holes]); outers counter-clockwise, holes clockwise."""
    outers = [p for p in paths if not any(q is not p and inside(p[0], q) for q in paths)]
    res = []
    for o in outers:
        holes = [p for p in paths if p is not o and inside(p[0], o)]
        o = o if area(o) > 0 else o[::-1]
        holes = [h if area(h) < 0 else h[::-1] for h in holes]
        res.append((o, holes))
    return res


def inset(poly, c):
    """offset a closed polygon to its left (the filled side for these orientations) by c, mitred."""
    P = np.array(poly, float); n = len(P)
    out = []
    for i in range(n):
        a, b, d = P[i - 1], P[i], P[(i + 1) % n]
        t0 = (b - a) / np.linalg.norm(b - a); t1 = (d - b) / np.linalg.norm(d - b)
        n0 = np.array([-t0[1], t0[0]]); n1 = np.array([-t1[1], t1[0]])
        m = n0 + n1
        cosh = np.dot(m / np.linalg.norm(m), n0)
        out.append(tuple(b + m / np.linalg.norm(m) * c / max(cosh, 0.35)))
    return out


def bridge(outer, holes):
    """join holes into the outer contour with zero-width cuts so ear clipping works."""
    poly = list(outer)
    for h in sorted(holes, key=lambda h: -max(p[0] for p in h)):
        hi = max(range(len(h)), key=lambda i: h[i][0])
        hp = np.array(h[hi])
        best = None
        for oi, op in enumerate(poly):
            op = np.array(op)
            if op[0] < hp[0]:
                continue
            ok = True
            for (a, b) in zip(poly, poly[1:] + poly[:1]):
                if _cross(hp, op, np.array(a), np.array(b)):
                    ok = False; break
            if ok:
                dd = np.linalg.norm(op - hp)
                if best is None or dd < best[0]:
                    best = (dd, oi)
        oi = best[1]
        ring = h[hi:] + h[:hi] + [h[hi]]
        poly = poly[:oi + 1] + ring + poly[oi:]
    return poly


def _cross(p1, p2, p3, p4):
    def o(a, b, c):
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    if np.allclose(p2, p3) or np.allclose(p2, p4) or np.allclose(p1, p3) or np.allclose(p1, p4):
        return False
    d1, d2, d3, d4 = o(p3, p4, p1), o(p3, p4, p2), o(p1, p2, p3), o(p1, p2, p4)
    return (d1 * d2 < 0) and (d3 * d4 < 0)


def make(out):
    paths = subpaths(SVG.read_text())
    xs = [x for p in paths for x, _ in p]
    cx = (min(xs) + max(xs)) / 2
    verts = []; faces = {0: [], 1: [], 2: []}

    def V(x, y, z):
        verts.append(((x - cx) * SCALE, y * SCALE, z * SCALE))
        return len(verts)

    def face(surf, idx, normal):
        a, b, c = (np.array(verts[i - 1]) for i in idx[:3])
        if np.dot(np.cross(b - a, c - a), normal) < 0:
            idx = idx[::-1]
        faces[surf].append(idx)

    z1 = DEPTH - BEVEL
    for outer, holes in glyphs(paths):
        contours = [outer] + holes
        for C in contours:
            I = inset(C, BEVEL)
            n = len(C)
            for i in range(n):
                j = (i + 1) % n
                e = np.array(C[j]) - np.array(C[i])
                nout = np.array([e[1], -e[0], 0.0]); nout /= np.linalg.norm(nout)  # right of travel = outside
                face(2, [V(*C[i], 0), V(*C[j], 0), V(*C[j], z1), V(*C[i], z1)], nout)
                face(1, [V(*C[i], z1), V(*C[j], z1), V(*I[j], DEPTH), V(*I[i], DEPTH)],
                     (nout + np.array([0, 0, 1.0])) / np.sqrt(2))
        front = bridge(inset(outer, BEVEL), [inset(h, BEVEL) for h in holes])
        for tri in earclip(front):
            face(0, [V(*front[k], DEPTH) for k in tri], np.array([0, 0, 1.0]))
    with open(out + '/meshes/wordmark.obj', 'w') as f:
        f.write(HDR + '# The Montezuma\'s wordmark (design/logo/wordmark.svg) extruded 0.12 of the cap height with a\n'
                '# 45-degree front chamfer; surfaces: 0 face, 1 bevel, 2 sides\n')
        for v in verts:
            f.write('v %.5f %.5f %.5f\n' % v)
        for surf, name in ((0, 'face'), (1, 'bevel'), (2, 'side')):
            f.write(f'usemtl {name}\n')
            for idx in faces[surf]:
                f.write('f ' + ' '.join(str(i) for i in idx) + '\n')

# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Planar-face geometry with a baked texture atlas.

A building is a set of groups: extrusions of a 2D profile along an axis (swept
tiers, stairs, balustrades, crests) and planar polygons. Each group gets a
rectangle in a shared atlas at its own texel density; the material functions
bake albedo, ORM and a height field per texel from world position, and the
height becomes a tangent-space normal map (OpenGL, green up). Meshes are
written as Wavefront OBJ with UVs into that atlas."""
import numpy as np
from PIL import Image
from .noise import lin2srgb

PAD = 4


def nrm(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


class Group:
    dens = 0.025
    weight = 1.0
    rect = None  # (x, y, w, h) in texels incl. pad

    def size(self):
        (u0, u1), (s0, s1) = self.extent()
        return int(np.ceil((u1 - u0) / self.dens)) + 2 * PAD, int(np.ceil((s1 - s0) / self.dens)) + 2 * PAD

    def uv(self, a, s, W, H):
        (u0, u1), (s0, s1) = self.extent()
        x0, y0, w, h = self.rect
        px = x0 + PAD + (a - u0) / self.dens
        py = y0 + PAD + (s1 - s) / self.dens
        return px / W, py / H

    def grid(self):
        """parameter values (a, s) at texel centres of the whole rect, shape (h, w)."""
        (u0, u1), (s0, s1) = self.extent()
        x0, y0, w, h = self.rect
        i = np.arange(w); j = np.arange(h)
        a = u0 + (i + 0.5 - PAD) * self.dens
        s = s1 - (j + 0.5 - PAD) * self.dens
        return np.meshgrid(a, s)


class Extrusion(Group):
    """profile (K,2) in (e1, e2), extruded along A over [a0, a1]; world = O + a A + e1 E1 + e2 E2.
    m0/m1: miter slopes at the two ends (end extends by m*e1). Profile runs so that the outward
    normal is on the right: n = t.y E1 - t.x E2."""

    def __init__(self, name, O, A, E1, E2, profile, a0, a1, m0=0.0, m1=0.0, paints=None, kinds=None,
                 end_convex=(True, True), skip=(), overrides=None, dens=0.025, extra=None):
        self.name = name
        self.O = np.asarray(O, float); self.A = nrm(A); self.E1 = nrm(E1); self.E2 = nrm(E2)
        self.P = np.asarray(profile, float)
        self.a0, self.a1, self.m0, self.m1 = a0, a1, m0, m1
        K = len(self.P) - 1
        self.K = K
        d = np.diff(self.P, axis=0)
        self.L = np.linalg.norm(d, axis=1)
        self.T = d / self.L[:, None]
        self.S = np.concatenate([[0], np.cumsum(self.L)])
        self.paints = paints if paints is not None else [0] * K
        self.kinds = kinds if kinds is not None else ['wall'] * K
        # convexity at profile vertices (between seg k-1 and k)
        cv = np.zeros(K + 1, bool)
        for k in range(1, K):
            t0, t1 = self.T[k - 1], self.T[k]
            cv[k] = (t0[0] * t1[1] - t0[1] * t1[0]) > 1e-6
        self.vconvex = cv
        self.vconcave = np.zeros(K + 1, bool)
        for k in range(1, K):
            t0, t1 = self.T[k - 1], self.T[k]
            self.vconcave[k] = (t0[0] * t1[1] - t0[1] * t1[0]) < -1e-6
        self.end_convex = end_convex
        self.skip = set(skip)
        self.overrides = overrides or {}
        self.dens = dens
        self.extra = extra or {}

    def normal3(self, k):
        t = self.T[k]
        return t[1] * self.E1 - t[0] * self.E2

    def extent(self):
        e1 = self.P[:, 0]
        lo = min(self.a0 - self.m0 * e1.max(), self.a0 - self.m0 * e1.min())
        hi = max(self.a1 + self.m1 * e1.max(), self.a1 + self.m1 * e1.min())
        return (lo, hi), (0.0, self.S[-1])

    def world(self, a, s):
        a = float(a); s = float(s)
        k = int(np.clip(np.searchsorted(self.S, s, side='right') - 1, 0, self.K - 1))
        f = (s - self.S[k]) / self.L[k]
        e = self.P[k] + (self.P[k + 1] - self.P[k]) * f
        return self.O + a * self.A + e[0] * self.E1 + e[1] * self.E2

    def quads(self):
        """yield (list of (a, s) corners, normal) for each emitted quad."""
        for k in range(self.K):
            n = self.normal3(k)
            if k in self.overrides:
                for (qa0, qa1, qs0, qs1) in self.overrides[k]:
                    yield [(qa0, self.S[k] + qs0), (qa1, self.S[k] + qs0), (qa1, self.S[k] + qs1), (qa0, self.S[k] + qs1)], n
                continue
            if k in self.skip:
                continue
            d0, d1 = self.P[k, 0], self.P[k + 1, 0]
            yield [(self.a0 - self.m0 * d0, self.S[k]), (self.a1 + self.m1 * d0, self.S[k]),
                   (self.a1 + self.m1 * d1, self.S[k + 1]), (self.a0 - self.m0 * d1, self.S[k + 1])], n

    def texel_info(self):
        a, s = self.grid()
        k = np.clip(np.searchsorted(self.S, s, side='right') - 1, 0, self.K - 1)
        f = (s - self.S[k]) / self.L[k]
        e = self.P[k] + (self.P[k + 1] - self.P[k]) * f[..., None]
        P = self.O + a[..., None] * self.A + e[..., 0:1] * self.E1 + e[..., 1:2] * self.E2
        N = self.T[k][..., 1:2] * self.E1 - self.T[k][..., 0:1] * self.E2
        s_in = s - self.S[k]; s_rem = self.S[k + 1] - s
        big = 99.0
        dconv = np.full(a.shape, big); dconc = np.full(a.shape, big)
        dconv = np.where(self.vconvex[k], np.minimum(dconv, s_in), dconv)
        dconv = np.where(self.vconvex[k + 1], np.minimum(dconv, s_rem), dconv)
        dconc = np.where(self.vconcave[k], np.minimum(dconc, s_in), dconc)
        dconc = np.where(self.vconcave[k + 1], np.minimum(dconc, s_rem), dconc)
        lo = self.a0 - self.m0 * e[..., 0]; hi = self.a1 + self.m1 * e[..., 0]
        # distance to the miter line measured in the face plane, approx horizontal
        dlo = (a - lo) / np.sqrt(1 + self.m0 ** 2) if self.m0 else (a - lo)
        dhi = (hi - a) / np.sqrt(1 + self.m1 ** 2) if self.m1 else (hi - a)
        for dd, cvx in ((dlo, self.end_convex[0]), (dhi, self.end_convex[1])):
            if cvx is None:
                continue
            if cvx:
                dconv = np.minimum(dconv, np.abs(dd))
            else:
                dconc = np.minimum(dconc, np.abs(dd))
        paint = np.asarray(self.paints)[k]
        kind = np.asarray([KIND[x] for x in self.kinds])[k]
        segfrac = f
        return dict(P=P, N=N, dconv=dconv, dconc=dconc, paint=paint, kind=kind, seg=k, s=s, a=a,
                    segfrac=segfrac, s_in=s_in, s_rem=s_rem, seglen=self.L[k])


class Planar(Group):
    """convex planar polygon (or several sharing a plane) given in world; frame U (image right), V (image up)."""

    def __init__(self, name, polys, U, V, paint=0, kind='wall', convex_edges=None, dens=0.025, extra=None):
        self.name = name
        self.polys = [np.asarray(p, float) for p in polys]
        self.U = nrm(U); self.V = nrm(V)
        self.N = np.cross(self.U, self.V)
        self.O = self.polys[0][0].copy()
        self.paint = paint; self.kind = kind
        self.dens = dens
        self.convex_edges = convex_edges  # list per poly of bools per edge, or None = all convex
        self.extra = extra or {}

    def set_normal(self, n):
        # orientation of U x V vs desired outward normal: flip U if needed
        if np.dot(self.N, n) < 0:
            self.U = -self.U
            self.N = np.cross(self.U, self.V)

    def loc(self, p):
        d = p - self.O
        return np.dot(d, self.U), np.dot(d, self.V)

    def extent(self):
        uv = np.array([self.loc(p) for poly in self.polys for p in poly])
        return (uv[:, 0].min(), uv[:, 0].max()), (uv[:, 1].min(), uv[:, 1].max())

    def quads(self):
        for poly in self.polys:
            yield [self.loc(p) for p in poly], self.N

    def world(self, a, s):
        return self.O + a * self.U + s * self.V

    def texel_info(self):
        a, s = self.grid()
        P = self.O + a[..., None] * self.U + s[..., None] * self.V
        N = np.broadcast_to(self.N, P.shape).copy()
        dconv = np.full(a.shape, 99.0); dconc = np.full(a.shape, 99.0)
        for pi, poly in enumerate(self.polys):
            uv = np.array([self.loc(p) for p in poly])
            n = len(uv)
            for e in range(n):
                p0 = uv[e]; p1 = uv[(e + 1) % n]
                cvx = True if self.convex_edges is None else self.convex_edges[pi][e]
                if cvx is None:
                    continue
                d = p1 - p0; L2 = np.dot(d, d)
                t = np.clip(((a - p0[0]) * d[0] + (s - p0[1]) * d[1]) / L2, 0, 1)
                dist = np.hypot(a - (p0[0] + t * d[0]), s - (p0[1] + t * d[1]))
                if cvx:
                    dconv = np.minimum(dconv, dist)
                else:
                    dconc = np.minimum(dconc, dist)
        return dict(P=P, N=N, dconv=dconv, dconc=dconc, paint=np.full(a.shape, self.paint),
                    kind=np.full(a.shape, KIND[self.kind]), seg=np.zeros(a.shape, int), s=s, a=a,
                    segfrac=np.zeros(a.shape), s_in=np.full(a.shape, 99.0), s_rem=np.full(a.shape, 99.0),
                    seglen=np.full(a.shape, 99.0))


KINDS = ['wall', 'talud', 'cornice', 'soffit', 'floor', 'terrace', 'riser', 'tread', 'chamfer', 'alf_top', 'alf_side',
         'shrine_wall', 'frieze', 'roof', 'interior', 'plinth', 'jamb', 'cap']
KIND = {k: i for i, k in enumerate(KINDS)}


def pack(groups, W, H, scale=1.0):
    for g in groups:
        g.dens = g.base_dens * scale
    sizes = [g.size() for g in groups]
    order = sorted(range(len(groups)), key=lambda i: (-sizes[i][1], -sizes[i][0]))
    STEP = 4
    sky = np.zeros(W // STEP + 1, int)
    for i in order:
        w, h = sizes[i]
        if w > W or h > H:
            return False
        nw = (w + STEP - 1) // STEP
        best = None
        for xs in range(0, W // STEP - nw + 1):
            y = sky[xs:xs + nw].max()
            if best is None or y < best[0]:
                best = (y, xs)
        y, xs = best
        if y + h > H:
            return False
        groups[i].rect = (xs * STEP, int(y), w, h)
        sky[xs:xs + nw] = y + h
    return True


def autopack(groups, W, H):
    for g in groups:
        if not hasattr(g, 'base_dens'):
            g.base_dens = g.dens
    sc = 0.5
    while not pack(groups, W, H, sc):
        sc *= 1.03
    print(f'atlas {W}x{H}: density scale {sc:.3f} (front {groups[0].dens*100:.2f} cm/texel)')
    return sc


def earclip(pts):
    pts = [np.asarray(p, float) for p in pts]
    n = len(pts)
    if n == 3:
        return [(0, 1, 2)]
    area = sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1] for i in range(n))
    sign = 1.0 if area > 0 else -1.0
    rem = list(range(n)); tris = []
    guard = 0
    while len(rem) > 3 and guard < 10000:
        guard += 1
        m = len(rem)
        for j in range(m):
            i0, i1, i2 = rem[(j - 1) % m], rem[j], rem[(j + 1) % m]
            a, b, c = pts[i0], pts[i1], pts[i2]
            cr = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
            if cr * sign <= 1e-12:
                continue
            ok = True
            for k in rem:
                if k in (i0, i1, i2):
                    continue
                p = pts[k]
                d1 = (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])
                d2 = (c[0] - b[0]) * (p[1] - b[1]) - (c[1] - b[1]) * (p[0] - b[0])
                d3 = (a[0] - c[0]) * (p[1] - c[1]) - (a[1] - c[1]) * (p[0] - c[0])
                if d1 * sign > 0 and d2 * sign > 0 and d3 * sign > 0:
                    ok = False; break
            if ok:
                tris.append((i0, i1, i2)); rem.pop(j); break
        else:
            break
    if len(rem) == 3:
        tris.append(tuple(rem))
    return tris


def write_obj(path, groups, W, H, header, flip=False):
    vs = []; vts = []; vns = []; fs = []
    nmap = {}
    for g in groups:
        for corners, n in g.quads():
            n = np.asarray(n, float)
            key = tuple(np.round(n, 4))
            if key not in nmap:
                vns.append(n); nmap[key] = len(vns)
            ni = nmap[key]
            idx = []
            for (a, s) in corners:
                p = g.world(np.float64(a), np.float64(s))
                u, v = g.uv(a, s, W, H)
                vs.append(p); vts.append((u, 1.0 - v))
                idx.append(len(vs))
            for tri in earclip([tuple(c) for c in corners]):
                t = [idx[i] for i in tri]
                p0, p1, p2 = vs[t[0] - 1], vs[t[1] - 1], vs[t[2] - 1]
                c = np.cross(p1 - p0, p2 - p0)
                if np.linalg.norm(c) < 1e-12:
                    continue
                if np.dot(c, n) < 0:
                    t = t[::-1]
                fs.append((t, ni))
    with open(path, 'w') as f:
        f.write(header)
        for p in vs:
            f.write(f'v {p[0]:.4f} {p[1]:.4f} {p[2]:.4f}\n')
        for t in vts:
            f.write(f'vt {t[0]:.6f} {t[1]:.6f}\n')
        for n in vns:
            f.write(f'vn {n[0]:.4f} {n[1]:.4f} {n[2]:.4f}\n')
        for idx, ni in fs:
            f.write('f ' + ' '.join(f'{i}/{i}/{ni}' for i in idx) + '\n')
    print(f'{path}: {len(vs)} verts, {len(fs)} faces')


def height_to_normal(h, dens, seg=None, strength=1.0):
    """h in metres on a texel grid whose rows go downward = -s. Returns tangent-space normal (OpenGL, +G = image up)."""
    dhdx = np.gradient(h, axis=1) / dens
    dhds = -np.gradient(h, axis=0) / dens  # row index increases as s decreases
    if seg is not None:
        edge = np.zeros_like(h, bool)
        edge[1:] |= seg[1:] != seg[:-1]
        edge[:-1] |= seg[1:] != seg[:-1]
        dhds = np.where(edge, 0.0, dhds)
        dhdx = np.where(edge, dhdx, dhdx)
    n = np.stack([-dhdx * strength, -dhds * strength, np.ones_like(h)], -1)
    return n / np.linalg.norm(n, axis=-1, keepdims=True)


def save_srgb(path, lin):
    save_u8(path, (lin2srgb(lin) * 255 + 0.5).astype(np.uint8))


def save_u8(path, img):
    Image.fromarray(img).save(path, compress_level=6)

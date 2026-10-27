# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Generate the shared prop library: set dressing a level picks by tag.

    python3 tools/assets/props.py [--only <id> ...] [--list] [--no-lookdev]

The library is permanent and grows: a prop stays in the repo whether or not a
level currently places it, so levels across locations, cultures and eras draw
from one pool. Nothing here is a board theme; board themes
(tools/assets/gen_theme_meshes.py, gen_theme_scenes.py) own the board, the
pieces and the environment, and this file owns everything that stands around
them.

Writes, all from the parameters and seeds in this file, so the same checkout
gives byte-identical files:

- game/assets/models/props/<family>/SM_Prop_<Name>.obj   one OBJ per (prop, material)
- game/assets/models/props/<family>/PF_Prop_<Name>.tscn  the scene a level instances
- game/assets/materials/props/MI_*.tres                  the library's materials
- game/assets/materials/props/hi/MI_*.tres               their desktop variants
- game/assets/lookdev/LD_Props_*.tscn                    look-dev pages, by family
- game/assets/library.json                               the catalogue levels query

The texture sets these materials use are built by
tools/assets/gen_theme_textures.py from tools/assets/themes/props.json (a
texture-set group, not a board theme), into game/assets/textures/props/.

Geometry is authored directly in Godot space: metres, Y up, +Z toward the
player, origin at the centre of the prop's ground contact so a level can drop
it on terrain at y = 0 and turn it about Y. No Blender and no UVs: every
material maps triplanar in world space, which also means two instances of one
prop at different positions do not show the same texture patch.

Needs only the standard library.
"""

import argparse
import json
import math
import random
import sys
from pathlib import Path

import gen_theme_scenes as scenes

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "game/assets"
PARAMS = ROOT / "tools/assets/themes/props.json"
THEME_FOR_LOOKDEV = ROOT / "tools/assets/themes/temple.json"
CATALOGUE = ASSETS / "library.json"
LIBRARY_VERSION = 1

# Triangle budget per size class, LOD0, for the whole prop (every part
# together). The mobile and web tiers allow 200k and 150k triangles in a whole
# visible frame (docs/reference/visual-target.md); a level that dresses a board
# with 40 props therefore has roughly 2k triangles each to spend, and the
# classes below sit inside that with room for the board and the backdrop.
# Godot's OBJ importer builds the lower LODs (generate_lods, on by default).
TRI_BUDGET = {"tiny": 800, "small": 2500, "medium": 7500, "large": 13000, "huge": 22000}
# Longest side in metres per size class; a prop is classified from its bounds.
SIZE_CLASS = (("tiny", 0.3), ("small", 1.0), ("medium", 2.5), ("large", 6.0), ("huge", 1e9))


# --------------------------------------------------------------------------
# Noise. Hash-based value noise, so a prop's surface is reproducible from its
# seed on any machine without numpy.


def _hash3(ix, iy, iz, seed):
    h = (ix * 374761393 + iy * 668265263 + iz * 1442695040 + seed * 2654435761) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFFFFFF) / 4294967295.0


def _smooth(t):
    return t * t * (3.0 - 2.0 * t)


def value_noise(x, y, z, seed):
    """Value noise in 0..1 on the integer lattice, smoothstep-interpolated."""
    ix, iy, iz = math.floor(x), math.floor(y), math.floor(z)
    fx, fy, fz = _smooth(x - ix), _smooth(y - iy), _smooth(z - iz)
    out = 0.0
    for dz in (0, 1):
        wz = fz if dz else 1.0 - fz
        for dy in (0, 1):
            wy = fy if dy else 1.0 - fy
            for dx in (0, 1):
                wx = fx if dx else 1.0 - fx
                out += wx * wy * wz * _hash3(ix + dx, iy + dy, iz + dz, seed)
    return out


def fbm(x, y, z, seed, octaves=4, lacunarity=2.0, gain=0.5):
    """Fractal value noise in 0..1."""
    total, amp, norm, f = 0.0, 1.0, 0.0, 1.0
    for o in range(octaves):
        total += amp * value_noise(x * f, y * f, z * f, seed + o * 1013)
        norm += amp
        amp *= gain
        f *= lacunarity
    return total / norm


# --------------------------------------------------------------------------
# Mesh


class Mesh:
    """Triangle and quad soup in Godot space (metres, Y up).

    Faces are tuples of vertex indices wound counter-clockwise seen from
    outside. Builders may wind a face either way and then call orient_by(),
    which flips every face that points at its own reference point; every
    primitive here is star-shaped about one, so winding cannot go wrong.
    """

    def __init__(self):
        self.verts = []
        self.faces = []

    def vert(self, x, y, z):
        self.verts.append((float(x), float(y), float(z)))
        return len(self.verts) - 1

    def face(self, *indices):
        self.faces.append(tuple(indices))

    def ring(self, radius, y, sides, phase=0.0, sx=1.0, sz=1.0):
        """A closed ring of `sides` vertices in the XZ plane; returns their indices."""
        return [self.vert(sx * radius * math.cos(phase + 2 * math.pi * i / sides), y,
                          sz * radius * math.sin(phase + 2 * math.pi * i / sides))
                for i in range(sides)]

    def bridge(self, lower, upper):
        """Quad band between two rings of equal length."""
        n = len(lower)
        for i in range(n):
            j = (i + 1) % n
            self.face(lower[i], upper[i], upper[j], lower[j])

    def tri_count(self):
        return sum(len(f) - 2 for f in self.faces)

    def bounds(self):
        if not self.verts:
            return (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)
        lo = tuple(min(v[a] for v in self.verts) for a in range(3))
        hi = tuple(max(v[a] for v in self.verts) for a in range(3))
        return lo, hi

    def centroid(self, f):
        n = len(f)
        return tuple(sum(self.verts[i][a] for i in f) / n for a in range(3))

    def newell(self, f):
        """Newell's normal, unnormalised: its length is twice the face's area."""
        nx = ny = nz = 0.0
        for i in range(len(f)):
            ax, ay, az = self.verts[f[i]]
            bx, by, bz = self.verts[f[(i + 1) % len(f)]]
            nx += (ay - by) * (az + bz)
            ny += (az - bz) * (ax + bx)
            nz += (ax - bx) * (ay + by)
        return nx, ny, nz

    def normal(self, f):
        """Newell's normal, unit length (zero vector for a degenerate face)."""
        nx, ny, nz = self.newell(f)
        length = math.sqrt(nx * nx + ny * ny + nz * nz)
        return (0.0, 0.0, 0.0) if length < 1e-12 else (nx / length, ny / length, nz / length)

    def orient_by(self, ref, first=0):
        """Flip every face from index `first` on that faces its reference point.

        `ref` is a point or a function of the face centroid returning one.
        """
        pick = ref if callable(ref) else (lambda _c: ref)
        for k in range(first, len(self.faces)):
            f = self.faces[k]
            c = self.centroid(f)
            n = self.normal(f)
            r = pick(c)
            if (c[0] - r[0]) * n[0] + (c[1] - r[1]) * n[1] + (c[2] - r[2]) * n[2] < 0:
                self.faces[k] = tuple(reversed(f))

    def transform(self, scale=(1.0, 1.0, 1.0), yaw=0.0, pitch=0.0, roll=0.0, move=(0.0, 0.0, 0.0)):
        """Scale, then roll (about Z), pitch (about X), yaw (about Y), then move."""
        cy, sy = math.cos(yaw), math.sin(yaw)
        cp, sp = math.cos(pitch), math.sin(pitch)
        cr, sr = math.cos(roll), math.sin(roll)
        out = []
        for x, y, z in self.verts:
            x, y, z = x * scale[0], y * scale[1], z * scale[2]
            x, y = x * cr - y * sr, x * sr + y * cr
            y, z = y * cp - z * sp, y * sp + z * cp
            x, z = x * cy + z * sy, -x * sy + z * cy
            out.append((x + move[0], y + move[1], z + move[2]))
        self.verts = out
        return self

    def add(self, other):
        """Append another mesh's geometry."""
        base = len(self.verts)
        self.verts.extend(other.verts)
        self.faces.extend(tuple(i + base for i in f) for f in other.faces)
        return self

    def weld(self, places=4):
        """Merge vertices that round to the same position and drop collapsed faces.

        Welding is what lets separately built parts share smooth normals and
        what keeps a displaced surface watertight.
        """
        index, keep, remap = {}, [], []
        for v in self.verts:
            key = (round(v[0], places), round(v[1], places), round(v[2], places))
            if key not in index:
                index[key] = len(keep)
                keep.append(v)
            remap.append(index[key])
        faces = []
        for f in self.faces:
            out = []
            for i in f:
                m = remap[i]
                if not out or out[-1] != m:
                    out.append(m)
            if len(out) > 2 and out[0] == out[-1]:
                out.pop()
            if len(out) > 2:
                faces.append(tuple(out))
        self.verts, self.faces = keep, faces
        return self

    def displace(self, amount, frequency, seed, octaves=4, direction=None, floor=None):
        """Push every vertex along `direction(v)` (its outward normal by default)
        by amount * (fbm - 0.5), leaving vertices at or below `floor` in place.

        This is what turns a primitive into stone: the texture carries the
        chisel marks and the pores, the displacement carries the shape the
        silhouette shows.
        """
        if direction is None:
            direction = self.vertex_normals()
        out = []
        for i, (x, y, z) in enumerate(self.verts):
            if floor is not None and y <= floor + 1e-6:
                out.append((x, y, z))
                continue
            d = direction[i] if isinstance(direction, list) else direction(x, y, z)
            k = amount * (fbm(x * frequency, y * frequency, z * frequency, seed, octaves) - 0.5)
            out.append((x + d[0] * k, y + d[1] * k, z + d[2] * k))
        self.verts = out
        return self

    def vertex_normals(self):
        """Area-weighted normal per vertex (all faces, no smoothing angle)."""
        acc = [[0.0, 0.0, 0.0] for _ in self.verts]
        for f in self.faces:
            n = self.normal(f)
            for i in f:
                acc[i][0] += n[0]
                acc[i][1] += n[1]
                acc[i][2] += n[2]
        out = []
        for a in acc:
            length = math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])
            out.append((0.0, 1.0, 0.0) if length < 1e-12 else (a[0] / length, a[1] / length, a[2] / length))
        return out

    def flatten_base(self, y=0.0):
        """Lift every vertex below y up to it, so the prop sits flat on terrain."""
        self.verts = [(vx, max(vy, y), vz) for vx, vy, vz in self.verts]
        return self.clean()

    def clean(self, min_area=1e-8):
        """Drop faces too small to have a reliable normal, and vertices no face
        uses.

        Flattening a base or welding a displaced surface can leave a face
        collapsed to a line or to a sliver. Its normal is meaningless, and an
        OBJ that carries a zero-length normal renders black in Godot, so the
        test is on the real area (half Newell's length), not on the normalised
        vector, which is unit length for a sliver too. min_area is in square
        metres: 1e-8 is a tenth of a millimetre square.
        """
        faces = []
        for f in self.faces:
            if len(f) < 3 or len(set(f)) < 3:
                continue
            nx, ny, nz = self.newell(f)
            if 0.5 * math.sqrt(nx * nx + ny * ny + nz * nz) > min_area:
                faces.append(f)
        used = sorted({i for f in faces for i in f})
        remap = {old: new for new, old in enumerate(used)}
        self.verts = [self.verts[i] for i in used]
        self.faces = [tuple(remap[i] for i in f) for f in faces]
        return self

    def drop_to(self, y=0.0):
        """Move the mesh so its lowest point sits at y."""
        lo, _ = self.bounds()
        return self.transform(move=(0.0, y - lo[1], 0.0))

    def largest_part(self):
        """Keep only the biggest piece joined by shared vertices.

        Cutting can shear a corner clean off; the loose piece is a sliver that
        hangs in the air beside the prop. There is no reason to keep one.
        """
        if not self.faces:
            return self
        parent = list(range(len(self.verts)))

        def find(i):
            while parent[i] != i:
                parent[i] = parent[parent[i]]
                i = parent[i]
            return i

        for f in self.faces:
            r = find(f[0])
            for i in f[1:]:
                parent[find(i)] = r
        area = {}
        for f in self.faces:
            nx, ny, nz = self.newell(f)
            area[find(f[0])] = area.get(find(f[0]), 0.0) + math.sqrt(nx * nx + ny * ny + nz * nz)
        best = max(area, key=area.get)
        self.faces = [f for f in self.faces if find(f[0]) == best]
        return self.clean()

    def cut(self, point, normal, bevel=0.0):
        """Slice with a plane, keep the half the normal points away from, and
        cap the cut with a flat face whose arris is chamfered by `bevel` metres.

        This is a real fracture, not a dent: the cap is a new flat surface that
        stops where the weathered surface starts, which is what makes broken
        stone read as broken rather than as dented. `bevel` is the width of the
        rounded-off arris; stone that has stood in the rain carries 10-30 mm.
        """
        length = math.sqrt(sum(c * c for c in normal))
        if length < 1e-12:
            return self
        n = tuple(c / length for c in normal)

        def dist(v):
            return sum((v[a] - point[a]) * n[a] for a in range(3))

        d = [dist(v) for v in self.verts]
        if max(d) <= 0:          # nothing to remove
            return self
        if min(d) >= 0:          # everything removed; refuse rather than empty it
            return self
        # A plane that takes most of the body turns a solid into an open shell.
        # Refuse it: a fracture is a face off a rock, not half the rock.
        if sum(1 for x in d if x > 0) > 0.45 * len(d):
            return self

        kept, cuts = [], []
        for f in self.faces:
            poly, made = [], []
            for k in range(len(f)):
                i, j = f[k], f[(k + 1) % len(f)]
                di, dj = d[i], d[j]
                if di <= 0:
                    poly.append(i)
                if (di <= 0) != (dj <= 0):
                    a, bv = self.verts[i], self.verts[j]
                    t = di / (di - dj)
                    x = self.vert(a[0] + (bv[0] - a[0]) * t,
                                  a[1] + (bv[1] - a[1]) * t,
                                  a[2] + (bv[2] - a[2]) * t)
                    poly.append(x)
                    made.append(x)
            if len(poly) > 2:
                kept.append(tuple(poly))
            # A face crossing the plane makes exactly two new points; the
            # segment between them is one edge of the cap's boundary.
            if len(made) == 2:
                cuts.append(tuple(made))
        self.faces = kept

        # Record the boundary by position, not by index, and weld before
        # capping. Every intersection point is built once per face that touches
        # it, so until the weld there are two or more vertices at each one;
        # moving only one of them for the chamfer would tear the surface open.
        def key_of(v):
            return (round(v[0], 4), round(v[1], 4), round(v[2], 4))

        edges = [(key_of(self.verts[a]), key_of(self.verts[bv])) for a, bv in cuts]
        self.weld()
        at = {key_of(v): i for i, v in enumerate(self.verts)}

        link = {}
        for ka, kb in edges:
            if ka == kb or ka not in at or kb not in at:
                continue
            link.setdefault(ka, set()).add(kb)
            link.setdefault(kb, set()).add(ka)

        seen, loops = set(), []
        for start in link:
            if start in seen:
                continue
            loop, prev, cur = [start], None, start
            seen.add(start)
            while True:
                nxt = next((c for c in link.get(cur, ()) if c != prev and c not in seen), None)
                if nxt is None:
                    break
                loop.append(nxt)
                seen.add(nxt)
                prev, cur = cur, nxt
            # Only a loop that returns to where it started bounds a face.
            if len(loop) > 2 and loop[0] in link.get(loop[-1], ()):
                loops.append([at[k] for k in loop])

        for loop in loops:
            self._cap(loop, n, bevel)
        return self.weld().clean()

    def _cap(self, loop, n, bevel):
        """Close one cut loop with a flat cap, chamfering the arris by `bevel`."""
        pts = [self.verts[i] for i in loop]
        c = tuple(sum(p[a] for p in pts) / len(pts) for a in range(3))
        # Wind the cap so it faces along +n, which is out of the solid.
        area = 0.0
        for k in range(len(pts)):
            a, bv = pts[k], pts[(k + 1) % len(pts)]
            area += (a[1] - bv[1]) * (a[2] + bv[2]) * n[0] \
                + (a[2] - bv[2]) * (a[0] + bv[0]) * n[1] \
                + (a[0] - bv[0]) * (a[1] + bv[1]) * n[2]
        if area < 0:
            loop, pts = loop[::-1], pts[::-1]

        if bevel <= 0:
            self.face(*loop)
            return

        # Chamfer: drop the boundary ring into the solid by `bevel`, and lay the
        # cap on the plane inset by `bevel` towards the loop's centre. The band
        # between the two rings is the rounded-off arris.
        inset = []
        for i, p in zip(loop, pts):
            to_c = [c[a] - p[a] for a in range(3)]
            length = math.sqrt(sum(t * t for t in to_c))
            if length < 1e-9:
                inset.append(i)
                continue
            step = min(bevel, length * 0.45)
            inset.append(self.vert(*[p[a] + to_c[a] / length * step for a in range(3)]))
            self.verts[i] = tuple(p[a] - n[a] * bevel for a in range(3))
        for k in range(len(loop)):
            j = (k + 1) % len(loop)
            self.face(loop[k], loop[j], inset[j], inset[k])
        self.face(*inset)


# --------------------------------------------------------------------------
# Primitives. Every one is star-shaped about a reference point, so orient_by()
# settles the winding.


def rounded_box(hx, hy, hz, radius, seg=3):
    """A box of half-extents (hx, hy, hz) with its edges and corners rounded by
    `radius`, as a grid `seg` quads across each face.

    Every cut stone in the library starts here: a cube reads as a bevelled box,
    a rounded box with a little noise on it reads as a dressed stone that has
    stood in the rain.
    """
    m = Mesh()
    half = (hx, hy, hz)
    # seg 1 would leave a face with no vertex of its own in the middle, so the
    # rounding would pull all eight corners together into a diamond.
    seg = max(2, seg)
    radius = max(1e-4, min(radius, min(half) * 0.98))
    inner = [h - radius for h in half]
    for axis in (0, 1, 2):
        u, v = (axis + 1) % 3, (axis + 2) % 3
        for sign in (-1, 1):
            grid = []
            for iu in range(seg + 1):
                row = []
                for iv in range(seg + 1):
                    p = [0.0, 0.0, 0.0]
                    p[axis] = sign * half[axis]
                    p[u] = (iu / seg * 2 - 1) * half[u]
                    p[v] = (iv / seg * 2 - 1) * half[v]
                    # Project onto the rounded box: clamp into the inner box,
                    # then step back out by the radius along the offset.
                    q = [max(-inner[a], min(inner[a], p[a])) for a in range(3)]
                    d = [p[a] - q[a] for a in range(3)]
                    length = math.sqrt(sum(c * c for c in d))
                    if length > 1e-9:
                        p = [q[a] + d[a] / length * radius for a in range(3)]
                    row.append(m.vert(*p))
                grid.append(row)
            for iu in range(seg):
                for iv in range(seg):
                    m.face(grid[iu][iv], grid[iu][iv + 1], grid[iu + 1][iv + 1], grid[iu + 1][iv])
    m.weld()
    m.orient_by((0.0, 0.0, 0.0))
    return m


def icosphere(subdiv=2, radius=1.0):
    """A geodesic sphere: an icosahedron with each face split `subdiv` times.

    Unlike a rounded box this has no edges, no poles and near-uniform triangle
    size, so noise displacement reads the same in every direction. Every rock
    in the library starts here; a rounded box cannot stop looking like a box
    however hard it is displaced, because its silhouette is still six flats and
    twelve arrises.

    Triangle count is 20 * 4**subdiv: 80, 320, 1280, 5120.
    """
    t = (1.0 + math.sqrt(5.0)) / 2.0
    verts = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0),
             (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
             (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    faces = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11),
             (1, 5, 9), (5, 11, 4), (11, 10, 2), (10, 7, 6), (7, 1, 8),
             (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9),
             (4, 9, 5), (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    verts = [tuple(c / math.sqrt(1 + t * t) for c in v) for v in verts]

    # Split each edge once per round, sharing the midpoint between the two
    # faces that own the edge so the sphere stays watertight.
    for _ in range(max(0, subdiv)):
        mid, out = {}, []

        def midpoint(i, j):
            key = (min(i, j), max(i, j))
            if key not in mid:
                a, b = verts[i], verts[j]
                p = [(a[k] + b[k]) / 2 for k in range(3)]
                length = math.sqrt(sum(c * c for c in p))
                verts.append(tuple(c / length for c in p))
                mid[key] = len(verts) - 1
            return mid[key]

        for a, b, c in faces:
            ab, bc, ca = midpoint(a, b), midpoint(b, c), midpoint(c, a)
            out += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        faces = out

    m = Mesh()
    for v in verts:
        m.vert(v[0] * radius, v[1] * radius, v[2] * radius)
    for f in faces:
        m.face(*f)
    m.orient_by((0.0, 0.0, 0.0))
    return m


def lathe(profile, sides=12, phase=0.0, sx=1.0, sz=1.0):
    """Solid of revolution about Y from a profile of (radius, y) points.

    A radius of 0 at either end closes it with a pole instead of a cap.
    """
    m = Mesh()
    rings = []
    for r, y in profile:
        rings.append(("pole", m.vert(0.0, y, 0.0)) if abs(r) < 1e-6
                     else ("ring", m.ring(r, y, sides, phase, sx, sz)))
    # Winding is set here, not by orient_by(): a cap's centroid sits on the
    # axis, so comparing it with a point on the axis decides nothing.
    for (ka, a), (kb, b) in zip(rings, rings[1:]):
        if ka == "ring" and kb == "ring":
            m.bridge(a, b)
        elif ka == "pole" and kb == "ring":
            for i in range(sides):
                m.face(a, b[i], b[(i + 1) % sides])
        elif ka == "ring" and kb == "pole":
            for i in range(sides):
                m.face(a[(i + 1) % sides], a[i], b)
    if rings[0][0] == "ring":
        m.face(*rings[0][1])            # base, rising angles wind it downward
    if rings[-1][0] == "ring":
        m.face(*reversed(rings[-1][1]))  # top, the other way round
    return m.weld().clean()


def tube(path, radii, sides=8):
    """A closed tube through `path` (a list of points) with a radius at each.

    Sections are built in the plane across the path, so a bent log or a root
    keeps its thickness through the bend.
    """
    m = Mesh()
    rings, centres = [], []
    for i, (p, r) in enumerate(zip(path, radii)):
        nxt = path[min(i + 1, len(path) - 1)]
        prv = path[max(i - 1, 0)]
        t = [nxt[a] - prv[a] for a in range(3)]
        length = math.sqrt(sum(c * c for c in t)) or 1.0
        t = [c / length for c in t]
        up = (0.0, 1.0, 0.0) if abs(t[1]) < 0.9 else (1.0, 0.0, 0.0)
        u = (t[1] * up[2] - t[2] * up[1], t[2] * up[0] - t[0] * up[2], t[0] * up[1] - t[1] * up[0])
        ul = math.sqrt(sum(c * c for c in u)) or 1.0
        u = [c / ul for c in u]
        v = (t[1] * u[2] - t[2] * u[1], t[2] * u[0] - t[0] * u[2], t[0] * u[1] - t[1] * u[0])
        ring = []
        for k in range(sides):
            a = 2 * math.pi * k / sides
            ring.append(m.vert(p[0] + r * (math.cos(a) * u[0] + math.sin(a) * v[0]),
                               p[1] + r * (math.cos(a) * u[1] + math.sin(a) * v[1]),
                               p[2] + r * (math.cos(a) * u[2] + math.sin(a) * v[2])))
        rings.append(ring)
        centres.append(p)
    for a, b in zip(rings, rings[1:]):
        m.bridge(a, b)

    def nearest(c):
        return min(centres, key=lambda p: sum((c[a] - p[a]) ** 2 for a in range(3)))

    m.orient_by(nearest)
    # The caps sit on the path, so their centroid is their own reference and
    # orient_by cannot judge them; point each along the path instead.
    for ring, along in ((rings[0], [centres[0][a] - centres[1][a] for a in range(3)]),
                        (rings[-1], [centres[-1][a] - centres[-2][a] for a in range(3)])):
        m.face(*ring)
        n = m.normal(m.faces[-1])
        if sum(n[a] * along[a] for a in range(3)) < 0:
            m.faces[-1] = tuple(reversed(m.faces[-1]))
    return m.weld().clean()


def torus(major, minor, major_seg=16, minor_seg=8):
    """Ring in the XZ plane, centred on the origin; the ball-court ring's stone."""
    m = Mesh()
    rings, centres = [], []
    for i in range(major_seg):
        a = 2 * math.pi * i / major_seg
        cx, cz = major * math.cos(a), major * math.sin(a)
        centres.append((cx, 0.0, cz))
        ring = []
        for k in range(minor_seg):
            b = 2 * math.pi * k / minor_seg
            ring.append(m.vert(cx + minor * math.cos(b) * math.cos(a), minor * math.sin(b),
                               cz + minor * math.cos(b) * math.sin(a)))
        rings.append(ring)
    for i in range(major_seg):
        m.bridge(rings[i], rings[(i + 1) % major_seg])
    m.weld()

    def nearest(c):
        return min(centres, key=lambda p: (c[0] - p[0]) ** 2 + (c[2] - p[2]) ** 2)

    m.orient_by(nearest)
    return m


def slab(sx, sy, sz, rng, *, radius=0.03, seg=3, relief=0.02, frequency=4.0, chip=0.0):
    """A dressed stone block standing on y = 0, weathered.

    `relief` is how far the surface wanders (metres), `chip` how much the
    corners are knocked off. Blocks are the library's unit of masonry: walls,
    stairs, altars and platforms are built out of them, which keeps the
    winding, the budget and the look consistent.
    """
    seed = rng.randrange(1 << 20)
    m = rounded_box(sx / 2, sy / 2, sz / 2, radius + chip * rng.uniform(0.5, 1.5), seg)
    if relief > 0:
        m.displace(relief, frequency, seed, octaves=3)
    m.transform(move=(0.0, sy / 2, 0.0))
    return m


def rock(rx, ry, rz, rng, *, angular=0.35, seg=4, relief=0.30, frequency=2.2, flatten=True,
         fractures=None, bevel=None):
    """A boulder: a displaced geodesic sphere with fracture planes cut through it.

    `angular` from 0 (a weathered round boulder, few fractures, soft arrises) to
    1 (a freshly broken block, many fractures, crisp arrises). `seg` is the
    sphere's subdivision level, so the triangle count is about 20 * 4**seg
    before the cuts.

    The shape comes from three things, in the order the eye reads them: the
    fracture planes give the flat faces and the straight arrises that say
    "stone"; the low-frequency displacement gives the lumpy overall mass; the
    high-frequency displacement gives the surface its grain. A rock with only
    the last of these is a bevelled box with a bumpy texture, which is what the
    library used to build.
    """
    seed = rng.randrange(1 << 20)
    seg = max(1, min(3, int(seg)))
    m = icosphere(seg, 1.0)
    m.transform(scale=(rx, ry, rz))

    # Amplitudes go with the mean radius, not the smallest: a boulder that has
    # been flattened in Y still has to wander as far in X and Z as a round one,
    # and keying off the smallest radius is what left the old rocks smooth.
    mean = (rx + ry + rz) / 3.0
    span = max(rx, ry, rz)
    # Three scales, in the order the eye reads them. The first is the one that
    # decides whether the thing is a rock or a box, and it has to be large: a
    # boulder is not a sphere with bumps, it is a lump.
    m.displace(1.50 * relief * mean, 0.85 * frequency / span, seed, octaves=2)
    m.displace(0.60 * relief * mean, 2.40 * frequency / span, seed + 77, octaves=3)
    m.displace(0.18 * relief * mean, 6.80 * frequency / span, seed + 931, octaves=2)

    # Real fracture planes. A weathered boulder keeps a couple of old break
    # faces; a freshly split block is nearly all break face. The cut has to bite
    # deep enough to leave a flat with an arris round it -- a plane that only
    # grazes the surface reads as a scratch, not as a break.
    if fractures is None:
        fractures = int(round(2 + 4 * angular))
    if bevel is None:
        # Weathered arrises are 10-30 mm; a fresh break keeps a sharper one.
        bevel = (0.030 - 0.018 * angular) * min(1.0, mean / 0.40)
    bevel = max(0.004, min(bevel, mean * 0.20))
    for _ in range(max(0, fractures)):
        # Aim the plane at a point inside the rock so the cut always bites, and
        # bias it away from straight down so the flat base stays the base.
        nx = rng.gauss(0.0, 1.0)
        ny = rng.gauss(0.0, 1.0) + 0.25
        nz = rng.gauss(0.0, 1.0)
        length = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
        n = (nx / length, ny / length, nz / length)
        # Measure how far the rock actually reaches in this direction *now*,
        # after the displacement and the cuts already made. Aiming at the
        # original ellipsoid instead is what turned these into broken shells.
        lo, hi = m.bounds()
        c = tuple((lo[a] + hi[a]) / 2 for a in range(3))
        reach = max(sum((v[a] - c[a]) * n[a] for a in range(3)) for v in m.verts)
        if reach <= 1e-6:
            continue
        depth = rng.uniform(0.55, 0.78) if angular > 0.5 else rng.uniform(0.70, 0.90)
        p = tuple(c[a] + n[a] * reach * depth for a in range(3))
        m.cut(p, n, bevel=bevel)
    m.largest_part()

    m.transform(yaw=rng.uniform(0, 2 * math.pi))
    m.drop_to(0.0)
    if flatten:
        # Cut the base off flat rather than folding it up: folding leaves a
        # skirt of slivers all round the contact, which is where the contact
        # shadow has to read.
        lo, hi = m.bounds()
        m.cut((0.0, lo[1] + (hi[1] - lo[1]) * 0.06, 0.0), (0.0, -1.0, 0.0), bevel=bevel * 0.5)
        m.drop_to(0.0)
    return m


def moss_shell(mesh, rng, *, coverage=0.45, lift=0.012, up_bias=0.25, frequency=2.5):
    """A thin shell of the faces of `mesh` that moss would hold: upward-facing
    ones, in patches, lifted off the stone so it reads as a mat and not a tint.

    Returns a new mesh; an empty one when nothing qualifies.
    """
    seed = rng.randrange(1 << 20)
    out = Mesh()
    # Lift along the vertex normal, not the face normal: neighbouring patches
    # then land on exactly the same points and weld into one mat instead of
    # standing as separate quads with cracks between them.
    vn = mesh.vertex_normals()
    index = {}

    # Score every face that could hold moss, then keep the dampest `coverage`
    # of them. Comparing each score against a fixed threshold instead makes
    # moss a lottery on the seed: on a prop as thin as a log the candidate
    # faces lie almost in a line, so one unlucky noise field leaves it bare.
    candidates = []
    for f in mesh.faces:
        n = mesh.normal(f)
        if n[1] < up_bias:
            continue
        c = mesh.centroid(f)
        damp = fbm(c[0] * frequency, c[1] * frequency * 0.6, c[2] * frequency, seed, 3)
        candidates.append((damp * (0.4 + 0.6 * n[1]), damp, f))
    if not candidates:
        return out
    candidates.sort(key=lambda row: -row[0])
    keep = max(1, int(round(len(candidates) * max(0.0, min(1.0, coverage)))))

    for _score, damp, f in candidates[:keep]:
        thick = lift * (0.6 + 0.8 * damp)
        corners = []
        for i in f:
            key = (i, round(thick, 4))
            if key not in index:
                index[key] = out.vert(mesh.verts[i][0] + vn[i][0] * thick,
                                      mesh.verts[i][1] + vn[i][1] * thick,
                                      mesh.verts[i][2] + vn[i][2] * thick)
            corners.append(index[key])
        out.face(*corners)
    return out.weld().clean()


# --------------------------------------------------------------------------
# OBJ output


def _fmt(v):
    text = f"{v:.5f}"
    return "0.00000" if text == "-0.00000" else text


def obj_text(mesh, name, smooth_angle):
    """Wavefront OBJ with per-corner normals, in a fixed order so runs match.

    A corner takes the average of the normals of the faces around its vertex
    that lie within `smooth_angle` of its own face, which is how a lathe stays
    round while a chisel facet keeps its edge.
    """
    around = [[] for _ in mesh.verts]
    normals = [mesh.normal(f) for f in mesh.faces]
    for k, f in enumerate(mesh.faces):
        for i in f:
            around[i].append(k)
    limit = math.cos(math.radians(smooth_angle))
    lines = ["# SPDX-License-Identifier: GPL-2.0-or-later",
             "# Generated by tools/assets/props.py; edit the generator, not this file.",
             f"o {name}"]
    lines += [f"v {_fmt(x)} {_fmt(y)} {_fmt(z)}" for x, y, z in mesh.verts]
    table, faces = {}, []
    for k, f in enumerate(mesh.faces):
        fn = normals[k]
        corners = []
        for i in f:
            ax = ay = az = 0.0
            for j in around[i]:
                o = normals[j]
                if smooth_angle <= 0.0 and j != k:
                    continue
                if fn[0] * o[0] + fn[1] * o[1] + fn[2] * o[2] < limit:
                    continue
                ax, ay, az = ax + o[0], ay + o[1], az + o[2]
            length = math.sqrt(ax * ax + ay * ay + az * az)
            n = fn if length < 1e-9 else (ax / length, ay / length, az / length)
            key = (_fmt(n[0]), _fmt(n[1]), _fmt(n[2]))
            corners.append((i + 1, table.setdefault(key, len(table) + 1)))
        faces.append(corners)
    lines += [f"vn {x} {y} {z}" for x, y, z in table]
    lines.append("s 0")
    lines += ["f " + " ".join(f"{v}//{n}" for v, n in face) for face in faces]
    return "\n".join(lines) + "\n"


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


# --------------------------------------------------------------------------
# Materials. One entry per surface the library uses, in the format
# gen_theme_scenes.material() reads, so prop materials follow exactly the
# conventions the board's do (triplanar without UVs, the ORM set's roughness
# and occlusion, no anisotropic repeat sampler).
#
# Every stone material maps in world space: two instances of one prop standing
# a metre apart sample different parts of the tile, so a row of the same rock
# does not read as a row of the same rock.

MATERIALS = {
    # Lowland limestone, the Maya lowland material set (mesoamerican-reference.md).
    "MI_Limestone": {"albedo": "#ffffff", "set": "Limestone_01", "triplanar_scale": 0.55,
                     "world_triplanar": True, "normal_scale": 1.0,
                     "hi": {"shader": "res://assets/materials/shaders/SH_TriplanarPBR.gdshader",
                            "params": {"height_depth": 0.03}}},
    # The same stone, warmer and greyer, for variety within one ruin.
    "MI_LimestoneWarm": {"albedo": "#d8c49c", "albedo_tint": "#d8c49c", "set": "Limestone_01",
                         "triplanar_scale": 0.5, "world_triplanar": True},
    "MI_LimestoneGrey": {"albedo": "#b9bcb4", "albedo_tint": "#b9bcb4", "set": "Limestone_01",
                         "triplanar_scale": 0.62, "world_triplanar": True},
    # Highland volcanic stone: the Mexica register (basalt, andesite, tezontle).
    "MI_Basalt": {"albedo": "#ffffff", "set": "Basalt_01", "triplanar_scale": 0.6,
                  "world_triplanar": True},
    "MI_Andesite": {"albedo": "#ffffff", "set": "Andesite_01", "triplanar_scale": 0.6,
                    "world_triplanar": True},
    "MI_Tezontle": {"albedo": "#ffffff", "set": "Tezontle_01", "triplanar_scale": 0.7,
                    "world_triplanar": True},
    # Lime plaster over masonry, and paint over the plaster.
    "MI_Stucco": {"albedo": "#ffffff", "set": "Stucco_01", "triplanar_scale": 0.45,
                  "world_triplanar": True},
    "MI_Polychrome": {"albedo": "#ffffff", "set": "Polychrome_01", "triplanar_scale": 0.4,
                      "world_triplanar": True},
    "MI_Bronze": {"albedo": "#ffffff", "set": "Bronze_01", "triplanar_scale": 1.4,
                  "world_triplanar": True, "anisotropy": 0.15},
    "MI_Clay": {"albedo": "#ffffff", "set": "Clay_01", "triplanar_scale": 1.6,
                "world_triplanar": True},
    "MI_Moss": {"albedo": "#ffffff", "set": "Moss_01", "triplanar_scale": 1.3,
                "world_triplanar": True, "backlight": "#1d3a14"},
    "MI_WetStone": {"albedo": "#ffffff", "set": "WetStone_01", "triplanar_scale": 0.6,
                    "world_triplanar": True, "ior": 1.5},
    # Water. Opaque on purpose: the shared material writer has no transparency
    # key and this library does not own it, so depth colour comes from a dark
    # body colour and a near-mirror roughness instead of from alpha. The tiers
    # that have them add screen-space reflections on top. Real depth, refraction
    # and caustics need the water shader the visual target asks for; see the
    # open items in docs/reference/prop-library.md.
    "MI_Water": {"albedo": "#1d3a39", "albedo_tint": "#1d3a39", "set": "Water_01",
                 "triplanar_scale": 0.45, "world_triplanar": True, "normal_scale": 0.8,
                 "ior": 1.333},
    # Falling and broken water: aerated, so it is genuinely near-opaque.
    "MI_WaterFall": {"albedo": "#cfe4e2", "albedo_tint": "#cfe4e2", "set": "Water_01",
                     "triplanar_scale": 1.1, "world_triplanar": True, "normal_scale": 1.4,
                     "ior": 1.333, "double_sided": True},
    "MI_Foam": {"albedo": "#e8f1ee", "metallic": 0.0, "roughness": 0.65},
    "MI_Bark": {"albedo": "#ffffff", "set": "Bark_01", "triplanar_scale": 0.9,
                "world_triplanar": True},
    # Look-dev only: the plaza the prop pages stand on, the board's floor.
    "MI_Plaza": {"albedo": "#ffffff", "set": "Plaza_01", "triplanar_scale": 0.22,
                 "world_triplanar": True},
}

# Stone the library's weathering grows on: a moss variant of a prop puts a
# shell over these parts and leaves metal, clay, water and wood alone.
STONE_MATERIALS = {"MI_Limestone", "MI_LimestoneWarm", "MI_LimestoneGrey", "MI_Basalt",
                   "MI_Andesite", "MI_Tezontle", "MI_Stucco", "MI_Polychrome", "MI_WetStone"}

LITHOLOGY = {
    "limestone": ("MI_Limestone", ["maya-lowland", "neutral"]),
    "limestone_warm": ("MI_LimestoneWarm", ["maya-lowland", "neutral"]),
    "limestone_grey": ("MI_LimestoneGrey", ["maya-lowland", "neutral"]),
    "basalt": ("MI_Basalt", ["mexica-highland", "neutral"]),
    "andesite": ("MI_Andesite", ["mexica-highland", "neutral"]),
    "tezontle": ("MI_Tezontle", ["mexica-highland"]),
    "stucco": ("MI_Stucco", ["maya-lowland", "mexica-highland"]),
    "polychrome": ("MI_Polychrome", ["maya-lowland", "mexica-highland"]),
}


def mat(lithology):
    return LITHOLOGY[lithology][0]


# --------------------------------------------------------------------------
# Composite primitives built on the ones above.


def vessel(outer, inner, sides=16):
    """A hollow vessel: `outer` is the outside profile from the base up to the
    rim, `inner` the inside from the rim back down to the floor.

    The rim is the band between the two top rings, so the wall has a real
    thickness you can see over. Winding is set by construction here, not by
    orient_by(): a hollow shape is not star-shaped about any point.
    """
    m = Mesh()
    o = [m.ring(r, y, sides) for r, y in outer]
    i = [m.ring(r, y, sides) for r, y in inner]
    for a, b in zip(o, o[1:]):
        m.bridge(a, b)        # outside wall, normals out
    for a, b in zip(i, i[1:]):
        m.bridge(a, b)        # inside wall, normals toward the axis
    m.bridge(o[-1], i[0])     # the rim, facing up
    m.face(*o[0])             # the base, facing down
    m.face(*reversed(i[-1]))  # the floor of the cavity, facing up
    return m.weld().clean()


def masonry(rng, length, height, depth, *, course=0.26, ruin=0.0, batter=0.05,
            block=(0.34, 0.68), lithology="limestone"):
    """A stretch of coursed masonry standing on y = 0, centred on the origin,
    running along X, with its courses broken by a wandering crest.

    Masonry is the library's unit of ruin: a wall, a corner, a stair and a
    platform are the same blocks in different arrangements, which keeps their
    scale, their weathering and their triangle cost consistent.
    """
    m = Mesh()
    seed = rng.randrange(1 << 20)

    def crest(x):
        t = (x + length / 2) / max(length, 1e-6)
        return height * (1.0 - ruin * (0.15 + 1.05 * fbm(t * 2.3, 0.0, 0.0, seed, 3)))

    y, row = 0.0, 0
    while y < height - 1e-6:
        # Courses in a Maya wall are not a module. Letting the height wander by
        # a quarter is the difference between masonry and brickwork.
        h = min(course * rng.uniform(0.78, 1.26), height - y)
        x = -length / 2 - (row % 2) * 0.27
        lo_x, hi_x = None, None
        while x < length / 2 - 1e-6:
            w = min(rng.uniform(*block), length / 2 - x)
            cx = x + w / 2
            if w > 0.11 and cx > -length / 2 and y + h * 0.45 <= crest(cx):
                d = max(0.14, depth - 2 * batter * y)
                # Full course height: the joint is the chamfer between two
                # blocks, not a gap between them. Leaving one opened a 15 mm
                # slot clean through the wall, and every joint showed sky.
                blk = slab(w * 0.988, h, d, rng, radius=0.009, seg=2, relief=0.009,
                           frequency=7.0, chip=0.012)
                blk.transform(yaw=rng.uniform(-0.035, 0.035),
                              move=(cx, y - h * rng.uniform(0.0, 0.05),
                                    rng.uniform(-0.012, 0.012)))
                m.add(blk)
                lo_x = cx - w / 2 if lo_x is None else min(lo_x, cx - w / 2)
                hi_x = cx + w / 2 if hi_x is None else max(hi_x, cx + w / 2)
            x += w
        # A rubble core behind the facing. Without it the wall is a row of
        # loose boxes and daylight comes through every joint; with it a joint
        # is a recess, which is what reads as a joint.
        if lo_x is not None and hi_x - lo_x > 0.05:
            d = max(0.10, depth - 2 * batter * y - 0.09)
            core = slab(hi_x - lo_x, h * 1.04, d, rng, radius=0.01, seg=2, relief=0.012,
                        frequency=6.0)
            core.transform(move=((lo_x + hi_x) / 2, y - h * 0.02, 0.0))
            m.add(core)
        y += h
        row += 1
    return m.weld().clean()


def grid_sheet(sx, sy, segs=(4, 4), bend=0.0, spread=0.0):
    """A quad sheet in the XY plane, origin at its top centre, hanging down.

    `bend` pushes the bottom toward +Z (a falling sheet of water leaves the
    lip and curves outward); `spread` widens it as it falls.
    """
    m = Mesh()
    nu, nv = segs
    grid = []
    for iv in range(nv + 1):
        t = iv / nv
        row = []
        for iu in range(nu + 1):
            u = (iu / nu - 0.5) * sx * (1.0 + spread * t)
            row.append(m.vert(u, -t * sy, bend * t * t))
        grid.append(row)
    # Wound so the front of the sheet faces +Z, the way the fall leaves the lip.
    for iv in range(nv):
        for iu in range(nu):
            m.face(grid[iv][iu], grid[iv + 1][iu], grid[iv + 1][iu + 1], grid[iv][iu + 1])
    return m


def water_plane(sx, sz, rng, *, y=0.0, segs=5, ripple=0.012):
    """A still water surface with a little relief, so the sun breaks on it."""
    m = Mesh()
    seed = rng.randrange(1 << 20)
    grid = []
    for iz in range(segs + 1):
        row = []
        for ix in range(segs + 1):
            px, pz = (ix / segs - 0.5) * sx, (iz / segs - 0.5) * sz
            row.append(m.vert(px, y + ripple * (fbm(px * 2.6, 0, pz * 2.6, seed, 2) - 0.5), pz))
        grid.append(row)
    for iz in range(segs):
        for ix in range(segs):
            m.face(grid[iz][ix], grid[iz][ix + 1], grid[iz + 1][ix + 1], grid[iz + 1][ix])
    m.orient_by((0.0, y - 10.0, 0.0))
    return m


def boss(rng, w, h, d, x, y, z, parent):
    """A shallow raised panel on a carved face: the library's unit of relief.

    `y` is the centre of the panel and `z` the face it stands on, so half of
    its depth is buried in the stone and half of it is the relief.
    """
    b = slab(w, h, d, rng, radius=0.008, seg=2, relief=0.004, frequency=18.0)
    b.transform(move=(x, y - h / 2, z))
    parent.add(b)


# --------------------------------------------------------------------------
# Prop builders. Each fills a Build with (part name, mesh, material, smoothing
# angle) and says how the prop meets the ground.


class Build:
    def __init__(self, rng):
        self.rng = rng
        self.parts = []
        self.footing = "sits"
        self.sink = 0.0

    def add(self, name, mesh, material, smooth=32.0):
        mesh.clean()
        if mesh.faces:
            self.parts.append((name, mesh, material, smooth))
        return mesh


# -- rock ------------------------------------------------------------------


def p_rock(b, size, lithology="limestone", angular=0.3, seg=4):
    m = rock(size[0] / 2, size[1] / 2, size[2] / 2, b.rng, angular=angular, seg=seg,
             relief=0.30 - 0.10 * angular)
    b.add("Mesh", m, mat(lithology), smooth=0.0 if angular > 0.6 else 34.0)
    b.footing, b.sink = "embedded", round(size[1] * 0.10, 3)


def p_outcrop(b, size, lithology="limestone", blocks=4):
    rng, m = b.rng, Mesh()
    for i in range(blocks):
        t = i / max(1, blocks - 1)
        w = size[0] * rng.uniform(0.34, 0.58)
        h = size[1] * rng.uniform(0.55, 1.0)
        d = size[2] * rng.uniform(0.40, 0.72)
        r = rock(w / 2, h / 2, d / 2, rng, angular=0.62, seg=3, relief=0.22, flatten=False)
        r.transform(yaw=rng.uniform(0, 2 * math.pi), pitch=rng.uniform(-0.14, 0.14),
                    roll=rng.uniform(-0.14, 0.14),
                    move=((t - 0.5) * size[0] * 0.58 + rng.uniform(-0.08, 0.08),
                          -h * rng.uniform(0.06, 0.34), rng.uniform(-0.28, 0.28) * size[2]))
        m.add(r)
    b.add("Mesh", m.weld(), mat(lithology), smooth=22.0)
    b.footing, b.sink = "embedded", round(size[1] * 0.30, 3)


def p_rubble_pile(b, radius=0.9, height=0.45, count=11, lithology="limestone"):
    rng, m = b.rng, Mesh()
    for i in range(count):
        t = i / count
        rad = radius * (1.0 - t) ** 0.6 * rng.uniform(0.3, 1.0)
        ang = rng.uniform(0, 2 * math.pi)
        s = radius * rng.uniform(0.14, 0.30)
        r = rock(s, s * rng.uniform(0.4, 0.7), s * rng.uniform(0.7, 1.1), rng,
                 angular=0.75, seg=2, relief=0.2, flatten=False)
        r.transform(yaw=rng.uniform(0, 2 * math.pi), pitch=rng.uniform(-0.5, 0.5),
                    roll=rng.uniform(-0.5, 0.5),
                    move=(rad * math.cos(ang), height * (1.0 - t) * rng.uniform(0.0, 0.8),
                          rad * math.sin(ang)))
        m.add(r)
    m.weld().drop_to(0.0)
    b.add("Mesh", m, mat(lithology), smooth=0.0)
    b.footing, b.sink = "sits", 0.0


def p_rubble_scatter(b, spread=1.4, count=7, lithology="limestone"):
    rng, m = b.rng, Mesh()
    for _ in range(count):
        s = spread * rng.uniform(0.05, 0.13)
        r = rock(s, s * rng.uniform(0.3, 0.55), s * rng.uniform(0.7, 1.2), rng,
                 angular=0.8, seg=2, relief=0.22)
        r.transform(yaw=rng.uniform(0, 2 * math.pi),
                    move=(rng.uniform(-0.5, 0.5) * spread, 0.0, rng.uniform(-0.5, 0.5) * spread))
        m.add(r)
    b.add("Mesh", m.weld(), mat(lithology), smooth=0.0)
    b.footing, b.sink = "sits", 0.0


def p_facing_stone(b, size=(0.78, 0.34, 0.26), lithology="limestone", faced=True, broken=0.5):
    """A piece of dressed facing stone off a building: three cut faces, one
    broken, and the stucco still on its front where it was sheltered."""
    rng = b.rng
    m = slab(size[0], size[1], size[2], rng, radius=0.016, seg=3, relief=0.008, frequency=9.0)
    seed = rng.randrange(1 << 20)
    # Knock the +X end off: push the vertices there out along a coarse noise.
    verts = []
    for x, y, z in m.verts:
        t = max(0.0, (x / (size[0] / 2) - 0.25) / 0.75)
        k = broken * 0.12 * t * (fbm(x * 9, y * 9, z * 9, seed, 3) - 0.45)
        verts.append((x + k * 2.2, y + k, z + k))
    m.verts = verts
    m.clean()
    m.transform(yaw=rng.uniform(0, 2 * math.pi), roll=rng.uniform(-0.06, 0.06))
    m.drop_to(0.0)
    b.add("Mesh", m, mat(lithology), smooth=26.0)
    if faced:
        face = slab(size[0] * 0.62, size[1] * 0.7, 0.02, rng, radius=0.006, seg=2, relief=0.004,
                    frequency=16.0)
        face.transform(move=(-size[0] * 0.12, size[1] * 0.12, size[2] / 2))
        b.add("Stucco", face, "MI_Stucco", smooth=20.0)
    b.footing, b.sink = "sits", 0.0


# -- ruin ------------------------------------------------------------------


def p_wall(b, length=3.0, height=1.5, depth=0.55, ruin=0.45, lithology="limestone", stucco=False):
    rng = b.rng
    b.add("Mesh", masonry(rng, length, height, depth, ruin=ruin), mat(lithology), smooth=26.0)
    if stucco:
        # A surviving patch of lime plaster on the sheltered face.
        patch = slab(length * rng.uniform(0.3, 0.5), height * rng.uniform(0.35, 0.6), 0.025, rng,
                     radius=0.01, seg=3, relief=0.008, frequency=7.0)
        patch.transform(move=(rng.uniform(-0.2, 0.2) * length, 0.0, depth / 2 - 0.01))
        b.add("Stucco", patch, "MI_Stucco", smooth=22.0)
    spill = Mesh()
    for _ in range(int(6 + 8 * ruin)):
        s = rng.uniform(0.07, 0.17)
        r = rock(s, s * rng.uniform(0.35, 0.6), s * rng.uniform(0.7, 1.2), rng, angular=0.8,
                 seg=2, relief=0.2)
        r.transform(yaw=rng.uniform(0, 2 * math.pi),
                    move=(rng.uniform(-0.5, 0.5) * length,
                          rng.uniform(0.0, 0.06),
                          (depth / 2 + rng.uniform(0.05, 0.6)) * rng.choice((-1, 1))))
        spill.add(r)
    b.add("Spill", spill.weld(), mat(lithology), smooth=0.0)
    b.footing, b.sink = "sits", 0.02


def p_wall_corner(b, arm=1.8, height=1.6, ruin=0.6, lithology="limestone"):
    rng = b.rng
    m = masonry(rng, arm, height, 0.5, ruin=ruin)
    m.transform(move=(arm / 2 - 0.25, 0.0, -arm / 2 + 0.25))
    other = masonry(rng, arm, height * rng.uniform(0.75, 1.0), 0.5, ruin=min(1.0, ruin + 0.2))
    other.transform(yaw=math.pi / 2, move=(-arm / 2 + 0.25, 0.0, arm / 2 - 0.25))
    m.add(other)
    collapse = Mesh()
    for _ in range(14):
        s = rng.uniform(0.09, 0.22)
        r = rock(s, s * rng.uniform(0.3, 0.6), s * rng.uniform(0.7, 1.3), rng, angular=0.8,
                 seg=2, relief=0.22)
        ang = rng.uniform(0.2, 1.4)
        rad = rng.uniform(0.5, 1.5)
        r.transform(yaw=rng.uniform(0, 2 * math.pi), pitch=rng.uniform(-0.3, 0.3),
                    move=(rad * math.cos(ang) - 0.2, rng.uniform(0.0, 0.12), rad * math.sin(ang) - 0.2))
        collapse.add(r)
    b.add("Mesh", m.weld(), mat(lithology), smooth=26.0)
    b.add("Collapse", collapse.weld(), mat(lithology), smooth=0.0)
    b.footing, b.sink = "sits", 0.02


def _drum(rng, radius, height, lithology, bands=3):
    """One column drum: a slightly barrelled, chipped cylinder."""
    profile = [(radius * 0.97, 0.0)]
    for i in range(1, bands):
        t = i / bands
        profile.append((radius * (1.0 + 0.02 * math.sin(math.pi * t)), height * t))
    profile.append((radius * 0.97, height))
    m = lathe(profile, sides=14)
    m.displace(radius * 0.05, 5.0 / radius, rng.randrange(1 << 20), octaves=3)
    return m


def p_column_fallen(b, drums=3, radius=0.26, lithology="limestone"):
    rng, m = b.rng, Mesh()
    z = -drums * radius * 1.9 / 2
    for i in range(drums):
        h = radius * rng.uniform(1.5, 2.2)
        d = _drum(rng, radius, h, lithology)
        # Lay it on its side: the drum's axis becomes the world Z axis, with a
        # little swing so the drums do not lie in one dead straight line.
        d.transform(pitch=math.pi / 2, yaw=rng.uniform(-0.09, 0.09),
                    move=(rng.uniform(-0.06, 0.06), radius, z))
        m.add(d)
        z += h * 1.08
    m.weld().drop_to(0.0)
    b.add("Mesh", m, mat(lithology), smooth=30.0)
    b.footing, b.sink = "sits", round(radius * 0.12, 3)


def p_column_stump(b, drums=2, radius=0.3, lithology="limestone"):
    rng, m = b.rng, Mesh()
    y = 0.0
    for i in range(drums):
        h = radius * rng.uniform(1.4, 2.0)
        d = _drum(rng, radius, h, lithology)
        d.transform(yaw=rng.uniform(0, 2 * math.pi), move=(rng.uniform(-0.02, 0.02), y,
                                                           rng.uniform(-0.02, 0.02)))
        m.add(d)
        y += h
    # Break the top off along a tilted, ragged plane.
    seed = rng.randrange(1 << 20)
    top = y
    verts = []
    for x, vy, z in m.verts:
        limit = top - radius * (0.35 + 0.5 * fbm(x * 3.0, 0.0, z * 3.0, seed, 3)) + 0.22 * x
        verts.append((x, min(vy, limit), z))
    m.verts = verts
    b.add("Mesh", m.weld().clean(), mat(lithology), smooth=28.0)
    b.footing, b.sink = "sits", 0.02


def p_stair(b, steps=5, width=2.2, rise=0.22, run=0.32, lithology="limestone"):
    rng, m = b.rng, Mesh()
    length = steps * run
    across = max(2, int(round(width / 0.7)))
    for i in range(steps):
        h = (i + 1) * rise
        zc = length / 2 - (i + 0.5) * run
        x = -width / 2
        for k in range(across):
            w = min((width / across) * rng.uniform(0.88, 1.12), width / 2 - x)
            if w < 0.1:
                break
            blk = slab(w * 0.98, h, run * 1.04, rng, radius=0.012, seg=2, relief=0.007,
                       frequency=9.0, chip=0.012)
            blk.transform(move=(x + w / 2, 0.0, zc))
            m.add(blk)
            x += w
    b.add("Mesh", m.weld(), mat(lithology), smooth=24.0)
    b.footing, b.sink = "sits", 0.0


def p_balustrade(b, steps=5, rise=0.22, run=0.32, width=0.46, lithology="limestone"):
    """The alfarda: the sloping block wall that flanks a stair, with the
    vertical upper panel that carries the carving."""
    rng, m = b.rng, Mesh()
    length = steps * run
    for i in range(steps):
        h = (i + 1) * rise + 0.26
        zc = length / 2 - (i + 0.5) * run
        blk = slab(width, h, run * 1.04, rng, radius=0.014, seg=2, relief=0.008, frequency=8.0,
                   chip=0.01)
        blk.transform(move=(0.0, 0.0, zc))
        m.add(blk)
    panel = slab(width * 1.08, steps * rise * 0.55, run * 1.4, rng, radius=0.02, seg=2,
                 relief=0.008, frequency=8.0)
    panel.transform(move=(0.0, steps * rise + 0.26, -length / 2 + run * 0.7))
    m.add(panel)
    b.add("Mesh", m.weld(), mat(lithology), smooth=24.0)
    b.footing, b.sink = "sits", 0.0


# -- carved ----------------------------------------------------------------


def p_stela(b, height=2.4, width=0.82, depth=0.34, lithology="limestone", painted=False):
    """A standing carved slab: a tapering shaft, a framed panel of low relief
    on the front and two columns of glyph cartouches on the side."""
    rng = b.rng
    m = slab(width, height, depth, rng, radius=0.025, seg=4, relief=0.012, frequency=4.5)
    # Taper and lean, the way a stela settles.
    m.verts = [(x * (1.0 - 0.07 * y / height), y, z * (1.0 - 0.05 * y / height))
               for x, y, z in m.verts]
    front = depth / 2
    panel = Mesh()
    # Frame: four thin bars around a sunken field.
    fw, fh = width * 0.74, height * 0.52
    cy = height * 0.60
    # Each bar is centred on its own side of the frame. Offsetting every bar by
    # half its own height suits the two horizontal ones and throws the two
    # vertical ones half a frame upwards, which stood them out of the top of
    # the stela like a pair of rods.
    for dx, dy, bw, bh in ((0, fh / 2, fw, 0.055), (0, -fh / 2, fw, 0.055),
                           (-fw / 2, 0, 0.055, fh), (fw / 2, 0, 0.055, fh)):
        boss(rng, bw, bh, 0.045, dx, cy + dy, front, panel)
    # The figure: a tall oval boss, shoulders and a headdress bar. Maya low
    # relief stands 30-80 mm proud; shallower than that and the sun cannot
    # throw the shadow that makes it read as carving rather than as staining.
    boss(rng, fw * 0.40, fh * 0.52, 0.062, 0.0, cy + fh * 0.12, front, panel)
    boss(rng, fw * 0.56, fh * 0.10, 0.052, 0.0, cy + fh * 0.22, front, panel)
    boss(rng, fw * 0.26, fh * 0.14, 0.074, 0.0, cy + fh * 0.30, front, panel)
    # Glyph cartouches down the lower register.
    for row in range(4):
        for col in (-1, 1):
            boss(rng, fw * 0.21, fh * 0.11, 0.042, col * fw * 0.17,
                 cy - fh * 0.16 - row * fh * 0.135, front, panel)
    b.add("Mesh", m, mat(lithology), smooth=26.0)
    b.add("Relief", panel.weld(), "MI_Polychrome" if painted else mat(lithology), smooth=20.0)
    b.footing, b.sink = "embedded", 0.05


def p_altar_round(b, radius=0.62, height=0.5, lithology="basalt"):
    rng = b.rng
    m = lathe([(radius * 0.94, 0.0), (radius, height * 0.18), (radius * 0.97, height * 0.82),
               (radius, height), (radius * 0.86, height)], sides=20)
    m.displace(0.012, 7.0, rng.randrange(1 << 20), octaves=3)
    band = Mesh()
    for i in range(10):
        a = 2 * math.pi * i / 10
        boss(rng, 0.10, height * 0.34, 0.03, radius * 0.99 * math.cos(a), height * 0.72,
             radius * 0.99 * math.sin(a), band)
    b.add("Mesh", m, mat(lithology), smooth=34.0)
    b.add("Relief", band.weld(), mat(lithology), smooth=18.0)
    b.footing, b.sink = "sits", 0.0


def p_altar_block(b, length=1.5, width=0.85, height=0.72, lithology="limestone"):
    rng, m = b.rng, Mesh()
    for sx in (-1, 1):
        for sz in (-1, 1):
            leg = slab(0.26, height - 0.16, 0.26, rng, radius=0.02, seg=2, relief=0.008,
                       frequency=9.0)
            leg.transform(move=(sx * (length / 2 - 0.24), 0.0, sz * (width / 2 - 0.22)))
            m.add(leg)
    top = slab(length, 0.17, width, rng, radius=0.022, seg=3, relief=0.008, frequency=6.0,
               chip=0.012)
    top.transform(move=(0.0, height - 0.17, 0.0))
    m.add(top)
    b.add("Mesh", m.weld(), mat(lithology), smooth=26.0)
    b.footing, b.sink = "sits", 0.0


def p_serpent_head(b, scale=1.0, lithology="basalt", painted=False):
    """A tenoned serpent-head block: the tenon that goes into the wall, a
    blocky snout, the brow ridge, the eye and the upper fangs."""
    rng, m = b.rng, Mesh()
    s = scale
    tenon = slab(0.46 * s, 0.42 * s, 0.75 * s, rng, radius=0.02, seg=2, relief=0.008,
                 frequency=8.0)
    tenon.transform(move=(0.0, 0.10 * s, -0.42 * s))
    m.add(tenon)
    head = slab(0.62 * s, 0.56 * s, 0.68 * s, rng, radius=0.045, seg=3, relief=0.012,
                frequency=6.0)
    head.transform(move=(0.0, 0.04 * s, 0.05 * s))
    m.add(head)
    snout = lathe([(0.30 * s, 0.0), (0.27 * s, 0.16 * s), (0.17 * s, 0.30 * s)], sides=10)
    snout.transform(pitch=math.pi / 2 * 0.92, move=(0.0, 0.26 * s, 0.38 * s))
    m.add(snout)
    detail = Mesh()
    boss(rng, 0.60 * s, 0.10 * s, 0.05 * s, 0.0, 0.52 * s, 0.30 * s, detail)      # brow
    for sx in (-1, 1):
        boss(rng, 0.17 * s, 0.17 * s, 0.05 * s, sx * 0.21 * s, 0.44 * s, 0.30 * s, detail)  # eyes
        boss(rng, 0.07 * s, 0.17 * s, 0.05 * s, sx * 0.13 * s, 0.14 * s, 0.37 * s, detail)  # fangs
    boss(rng, 0.40 * s, 0.07 * s, 0.05 * s, 0.0, 0.20 * s, 0.37 * s, detail)      # lip
    b.add("Mesh", m.weld(), mat(lithology), smooth=24.0)
    b.add("Relief", detail.weld(), "MI_Polychrome" if painted else mat(lithology), smooth=16.0)
    b.footing, b.sink = "wall_tenon", 0.0


def p_chacmool(b, scale=1.0, lithology="andesite"):
    """A reclining figure holding a dish on its belly, stylised into the few
    blocks the real sculpture is cut from: it must read from the gameplay
    camera, not reward a close look."""
    rng, m = b.rng, Mesh()
    s = scale
    base = slab(1.20 * s, 0.14 * s, 0.56 * s, rng, radius=0.02, seg=3, relief=0.008, frequency=7.0)
    m.add(base)
    body = slab(0.70 * s, 0.34 * s, 0.44 * s, rng, radius=0.07, seg=3, relief=0.012, frequency=6.0)
    body.transform(move=(-0.08 * s, 0.14 * s, 0.0))
    m.add(body)
    for sx in (-1, 1):   # drawn-up knees
        knee = slab(0.26 * s, 0.40 * s, 0.20 * s, rng, radius=0.07, seg=2, relief=0.01,
                    frequency=8.0)
        knee.transform(move=(0.40 * s, 0.14 * s, sx * 0.14 * s))
        m.add(knee)
        shin = slab(0.34 * s, 0.17 * s, 0.17 * s, rng, radius=0.06, seg=2, relief=0.01,
                    frequency=8.0)
        shin.transform(move=(0.16 * s, 0.14 * s, sx * 0.15 * s))
        m.add(shin)
    shoulders = slab(0.26 * s, 0.26 * s, 0.50 * s, rng, radius=0.06, seg=2, relief=0.01,
                     frequency=7.0)
    shoulders.transform(move=(-0.44 * s, 0.14 * s, 0.0))
    m.add(shoulders)
    head = slab(0.24 * s, 0.26 * s, 0.26 * s, rng, radius=0.07, seg=2, relief=0.01, frequency=9.0)
    head.transform(yaw=0.55, move=(-0.48 * s, 0.40 * s, 0.10 * s))
    m.add(head)
    dish = vessel([(0.17 * s, 0.0), (0.19 * s, 0.09 * s)],
                  [(0.15 * s, 0.09 * s), (0.13 * s, 0.03 * s)], sides=14)
    dish.transform(move=(0.0, 0.48 * s, 0.0))
    m.add(dish)
    b.add("Mesh", m.weld(), mat(lithology), smooth=30.0)
    b.footing, b.sink = "sits", 0.0


def p_standard_bearer(b, scale=1.0, lithology="andesite"):
    """A kneeling standard-bearer with the socket its banner stood in."""
    rng, m = b.rng, Mesh()
    s = scale
    base = slab(0.52 * s, 0.12 * s, 0.52 * s, rng, radius=0.02, seg=2, relief=0.008, frequency=8.0)
    m.add(base)
    legs = slab(0.40 * s, 0.30 * s, 0.46 * s, rng, radius=0.07, seg=2, relief=0.01, frequency=8.0)
    legs.transform(move=(0.0, 0.12 * s, 0.02 * s))
    m.add(legs)
    torso = slab(0.36 * s, 0.46 * s, 0.30 * s, rng, radius=0.07, seg=3, relief=0.012, frequency=7.0)
    torso.transform(pitch=-0.07, move=(0.0, 0.40 * s, -0.02 * s))
    m.add(torso)
    for sx in (-1, 1):   # arms closed around the shaft
        arm = slab(0.11 * s, 0.34 * s, 0.13 * s, rng, radius=0.045, seg=2, relief=0.01,
                   frequency=10.0)
        arm.transform(roll=sx * 0.18, move=(sx * 0.21 * s, 0.44 * s, 0.09 * s))
        m.add(arm)
    head = slab(0.22 * s, 0.26 * s, 0.22 * s, rng, radius=0.07, seg=2, relief=0.01, frequency=9.0)
    head.transform(move=(0.0, 0.86 * s, -0.01 * s))
    m.add(head)
    socket = vessel([(0.09 * s, 0.0), (0.095 * s, 0.14 * s)],
                    [(0.055 * s, 0.14 * s), (0.05 * s, 0.02 * s)], sides=12)
    socket.transform(move=(0.0, 0.70 * s, 0.14 * s))
    m.add(socket)
    b.add("Mesh", m.weld(), mat(lithology), smooth=30.0)
    b.footing, b.sink = "sits", 0.0


def p_ball_court_ring(b, radius=0.42, lithology="basalt", painted=False):
    rng, m = b.rng, Mesh()
    ring = torus(radius, radius * 0.30, major_seg=18, minor_seg=8)
    ring.transform(pitch=math.pi / 2, move=(0.0, radius + 0.1, 0.0))
    m.add(ring)
    tenon = slab(radius * 0.9, radius * 0.9, radius * 1.5, rng, radius=0.03, seg=2, relief=0.01,
                 frequency=8.0)
    tenon.transform(move=(0.0, radius + 0.1 - radius * 0.45, -radius * 0.9))
    m.add(tenon)
    detail = Mesh()
    for i in range(8):
        a = 2 * math.pi * i / 8
        boss(rng, 0.07, 0.07, 0.025, radius * 1.02 * math.cos(a),
             radius + 0.1 + radius * 1.02 * math.sin(a) + 0.035, radius * 0.28, detail)
    b.add("Mesh", m.weld(), mat(lithology), smooth=32.0)
    b.add("Relief", detail.weld(), "MI_Polychrome" if painted else mat(lithology), smooth=18.0)
    b.footing, b.sink = "wall_tenon", 0.0


def p_glyph_block(b, size=(0.56, 0.52, 0.42), lithology="limestone", painted=False):
    rng = b.rng
    m = slab(size[0], size[1], size[2], rng, radius=0.022, seg=3, relief=0.009, frequency=7.0,
             chip=0.012)
    panel = Mesh()
    fw, fh = size[0] * 0.72, size[1] * 0.62
    cy = size[1] * 0.60
    front = size[2] / 2
    for dx, dy, bw, bh in ((0, fh / 2, fw, 0.04), (0, -fh / 2, fw, 0.04),
                           (-fw / 2, 0, 0.04, fh), (fw / 2, 0, 0.04, fh)):
        boss(rng, bw, bh, 0.028, dx, cy + dy + bh / 2, front, panel)
    for row in range(2):
        for col in range(2):
            boss(rng, fw * 0.34, fh * 0.30, 0.022, (col - 0.5) * fw * 0.42,
                 cy + (0.5 - row) * fh * 0.36 + fh * 0.15, front, panel)
    b.add("Mesh", m, mat(lithology), smooth=24.0)
    b.add("Relief", panel.weld(), "MI_Polychrome" if painted else mat(lithology), smooth=16.0)
    b.footing, b.sink = "sits", 0.0


# -- ritual and vessels ----------------------------------------------------


def p_brazier(b, radius=0.44, height=0.74, lithology="basalt"):
    """A stone brazier: a pedestal, a flaring bowl and the ash in it. The fire
    itself is the theme's PF_BrazierFire, which a level parents to the bowl."""
    rng, m = b.rng, Mesh()
    foot = lathe([(radius * 0.72, 0.0), (radius * 0.66, 0.06), (radius * 0.40, 0.16),
                  (radius * 0.38, height * 0.52), (radius * 0.52, height * 0.60)], sides=16)
    m.add(foot)
    bowl = vessel([(radius * 0.52, height * 0.58), (radius * 0.92, height * 0.78),
                   (radius, height)],
                  [(radius * 0.88, height), (radius * 0.80, height * 0.86),
                   (radius * 0.30, height * 0.70)], sides=18)
    m.add(bowl)
    m.displace(0.008, 9.0, rng.randrange(1 << 20), octaves=3)
    ash = lathe([(radius * 0.80, height * 0.80), (radius * 0.55, height * 0.86),
                 (0.0, height * 0.88)], sides=14)
    b.add("Mesh", m.weld(), mat(lithology), smooth=34.0)
    b.add("Ash", ash, "MI_Clay", smooth=40.0)
    b.footing, b.sink = "sits", 0.0


def p_censer(b, radius=0.17, height=0.26):
    """A fired-clay tripod censer, the household and offering kind."""
    rng, m = b.rng, Mesh()
    body = vessel([(radius * 0.52, 0.10), (radius, height * 0.62), (radius * 0.96, height)],
                  [(radius * 0.86, height), (radius * 0.80, height * 0.70),
                   (radius * 0.26, height * 0.26)], sides=14)
    m.add(body)
    for i in range(3):
        a = 2 * math.pi * i / 3
        leg = lathe([(0.034, 0.0), (0.030, 0.09), (0.020, 0.13)], sides=7)
        leg.transform(move=(radius * 0.62 * math.cos(a), 0.0, radius * 0.62 * math.sin(a)))
        m.add(leg)
    spikes = Mesh()
    for i in range(8):
        a = 2 * math.pi * i / 8 + 0.2
        sp = lathe([(0.022, 0.0), (0.0, 0.055)], sides=6)
        sp.transform(move=(radius * 1.0 * math.cos(a), height * 0.74, radius * 1.0 * math.sin(a)))
        spikes.add(sp)
    b.add("Mesh", m.weld(), "MI_Clay", smooth=40.0)
    b.add("Spikes", spikes.weld(), "MI_Clay", smooth=30.0)
    b.footing, b.sink = "sits", 0.0


def p_offering_bowl(b, radius=0.30, height=0.17, lithology="limestone"):
    rng = b.rng
    m = vessel([(radius * 0.66, 0.0), (radius * 0.94, height * 0.55), (radius, height)],
               [(radius * 0.88, height), (radius * 0.80, height * 0.55),
                (radius * 0.24, height * 0.22)], sides=16)
    m.displace(0.005, 14.0, rng.randrange(1 << 20), octaves=2)
    b.add("Mesh", m.weld(), mat(lithology), smooth=36.0)
    b.footing, b.sink = "sits", 0.0


def p_olla(b, height=0.62, radius=0.27):
    """A big storage jar, the shape that holds water and maize."""
    rng = b.rng
    m = vessel([(radius * 0.40, 0.0), (radius * 0.86, height * 0.22), (radius, height * 0.44),
                (radius * 0.78, height * 0.74), (radius * 0.50, height * 0.92),
                (radius * 0.52, height)],
               [(radius * 0.44, height), (radius * 0.42, height * 0.90),
                (radius * 0.80, height * 0.55), (radius * 0.30, height * 0.10)], sides=16)
    m.displace(0.006, 11.0, rng.randrange(1 << 20), octaves=2)
    b.add("Mesh", m.weld(), "MI_Clay", smooth=42.0)
    b.footing, b.sink = "sits", 0.0


def p_tripod_vessel(b, height=0.26, radius=0.15, painted=True):
    rng, m = b.rng, Mesh()
    body = vessel([(radius, 0.08), (radius * 1.02, height * 0.7), (radius, height)],
                  [(radius * 0.88, height), (radius * 0.86, height * 0.5),
                   (radius * 0.30, height * 0.14)], sides=16)
    m.add(body)
    for i in range(3):
        a = 2 * math.pi * i / 3 + 0.4
        leg = lathe([(0.040, 0.0), (0.036, 0.055), (0.030, 0.09)], sides=8)
        leg.transform(move=(radius * 0.70 * math.cos(a), 0.0, radius * 0.70 * math.sin(a)))
        m.add(leg)
    b.add("Mesh", m.weld(), "MI_Polychrome" if painted else "MI_Clay", smooth=42.0)
    b.footing, b.sink = "sits", 0.0


def p_sherds(b, count=9, spread=0.7):
    """Broken pottery where a vessel went over: thin curved pieces lying flat."""
    rng, m = b.rng, Mesh()
    for _ in range(count):
        w = rng.uniform(0.05, 0.13)
        sh = rounded_box(w, 0.006, w * rng.uniform(0.5, 1.0), 0.004, 2)
        # Less than the shard's half thickness, or the displacement turns it
        # inside out.
        sh.displace(0.006, 9.0, rng.randrange(1 << 20), octaves=2)
        sh.transform(yaw=rng.uniform(0, 2 * math.pi), pitch=rng.uniform(-0.3, 0.3),
                     roll=rng.uniform(-0.3, 0.3),
                     move=(rng.uniform(-0.5, 0.5) * spread, 0.008, rng.uniform(-0.5, 0.5) * spread))
        m.add(sh)
    b.add("Mesh", m.weld(), "MI_Clay", smooth=24.0)
    b.footing, b.sink = "sits", 0.0


# -- water -----------------------------------------------------------------


def p_basin(b, radius=0.72, height=0.44, lithology="limestone", water=True):
    rng = b.rng
    m = vessel([(radius * 0.92, 0.0), (radius, height * 0.4), (radius * 0.97, height)],
               [(radius * 0.84, height), (radius * 0.78, height * 0.5),
                (radius * 0.24, height * 0.12)], sides=18)
    m.displace(0.008, 8.0, rng.randrange(1 << 20), octaves=3)
    b.add("Mesh", m.weld(), mat(lithology), smooth=34.0)
    # A band of water-stained stone just inside the wall, from the old water
    # line up to the rim. It has to stay inside the basin's cavity, which
    # narrows fast below half height, so it only covers the upper wall.
    b.add("Wet", vessel([(radius * 0.770, height * 0.55), (radius * 0.820, height * 0.94)],
                        [(radius * 0.806, height * 0.94), (radius * 0.756, height * 0.55)],
                        sides=18), "MI_WetStone", smooth=36.0)
    if water:
        b.add("Water", water_plane(radius * 1.56, radius * 1.56, rng, y=height * 0.56, segs=4),
              "MI_Water", smooth=60.0)
    b.footing, b.sink = "sits", 0.0


def p_drain(b, length=0.9, lithology="limestone"):
    """A stone drain block with its channel cut through, the kind that takes
    rain off a platform and spits it clear of the facing."""
    rng, m = b.rng, Mesh()
    w, h = 0.40, 0.30
    for sx in (-1, 1):
        side = slab(0.13, h, length, rng, radius=0.012, seg=2, relief=0.007, frequency=9.0)
        side.transform(move=(sx * (w / 2 - 0.065), 0.0, 0.0))
        m.add(side)
    floor = slab(w, 0.11, length, rng, radius=0.012, seg=2, relief=0.007, frequency=9.0)
    m.add(floor)
    lip = slab(w * 1.25, 0.12, 0.16, rng, radius=0.02, seg=2, relief=0.008, frequency=10.0)
    lip.transform(move=(0.0, 0.06, length / 2 + 0.06))
    m.add(lip)
    b.add("Mesh", m.weld(), mat(lithology), smooth=24.0)
    wet = slab(w * 0.82, 0.015, length * 0.98, rng, radius=0.005, seg=2, relief=0.003,
               frequency=14.0)
    wet.transform(move=(0.0, 0.105, 0.0))
    b.add("Wet", wet, "MI_WetStone", smooth=30.0)
    b.footing, b.sink = "sits", 0.0


def p_channel(b, length=2.4, lithology="limestone", water=True):
    """A straight run of stone-lined water channel, laid flush with the ground."""
    rng, m = b.rng, Mesh()
    inner, wall, depth = 0.34, 0.16, 0.26
    for sx in (-1, 1):
        side = masonry(rng, length, depth, wall, course=0.14, ruin=0.0, batter=0.0,
                       block=(0.35, 0.60))
        # Turn it to run along Z first, then set it beside the channel: one
        # call, because a move followed by a yaw swings the piece round the
        # world origin instead of its own.
        side.transform(yaw=math.pi / 2, move=(sx * (inner / 2 + wall / 2), 0.0, 0.0))
        m.add(side)
    floor = slab(inner + 2 * wall, 0.09, length, rng, radius=0.01, seg=3, relief=0.006,
                 frequency=8.0)
    m.add(floor)
    b.add("Mesh", m.weld(), mat(lithology), smooth=24.0)
    wet = slab(inner * 0.98, 0.02, length * 0.99, rng, radius=0.006, seg=3, relief=0.004,
               frequency=12.0)
    wet.transform(move=(0.0, 0.085, 0.0))
    b.add("Wet", wet, "MI_WetStone", smooth=30.0)
    if water:
        b.add("Water", water_plane(inner * 0.96, length * 0.98, rng, y=0.145, segs=3),
              "MI_Water", smooth=60.0)
    b.footing, b.sink = "embedded", 0.08


def p_pool(b, size=(3.0, 2.2), depth=0.5, lithology="limestone"):
    """A stone-lined pool: a coping of masonry, wet stone below the line, and
    a water surface a little under it."""
    rng, m = b.rng, Mesh()
    sx, sz = size
    for side, (length, yaw, px, pz) in enumerate(((sx, 0.0, 0.0, -sz / 2 + 0.14),
                                                  (sx, 0.0, 0.0, sz / 2 - 0.14),
                                                  (sz, math.pi / 2, -sx / 2 + 0.14, 0.0),
                                                  (sz, math.pi / 2, sx / 2 - 0.14, 0.0))):
        wall = masonry(rng, length, depth, 0.28, course=0.25, ruin=0.0, batter=0.0,
                       block=(0.45, 0.80))
        wall.transform(yaw=yaw, move=(px, 0.0, pz))
        m.add(wall)
    b.add("Mesh", m.weld(), mat(lithology), smooth=24.0)
    liner = slab(sx - 0.56, 0.08, sz - 0.56, rng, radius=0.01, seg=4, relief=0.006, frequency=5.0)
    b.add("Wet", liner, "MI_WetStone", smooth=30.0)
    b.add("Water", water_plane(sx - 0.52, sz - 0.52, rng, y=depth * 0.70, segs=6),
          "MI_Water", smooth=60.0)
    b.footing, b.sink = "embedded", round(depth * 0.3, 3)


def p_cascade(b, steps=3, width=1.8, lithology="limestone"):
    """A stepped cascade: each step a lip of wet stone, a sheet falling off it
    and a sill of broken water below."""
    rng, m = b.rng, Mesh()
    wet, sheets, foam = Mesh(), Mesh(), Mesh()
    rise, run = 0.42, 0.72
    for i in range(steps):
        y = (steps - i - 1) * rise
        z = (i - (steps - 1) / 2) * run
        block = slab(width, y + rise, run * 1.02, rng, radius=0.02, seg=3, relief=0.01,
                     frequency=6.0, chip=0.015)
        block.transform(move=(0.0, 0.0, z))
        m.add(block)
        lip = slab(width * 0.9, 0.02, run * 0.5, rng, radius=0.006, seg=3, relief=0.004,
                   frequency=11.0)
        lip.transform(move=(0.0, y + rise, z + run * 0.2))
        wet.add(lip)
        sheet = grid_sheet(width * 0.78, rise, segs=(4, 3), bend=0.14, spread=0.1)
        sheet.transform(move=(0.0, y + rise, z + run * 0.52))
        sheets.add(sheet)
        pad = water_plane(width * 0.8, run * 0.5, rng, y=y + 0.03, segs=3)
        pad.transform(move=(0.0, 0.0, z + run * 0.62))
        foam.add(pad)
    b.add("Mesh", m.weld(), mat(lithology), smooth=24.0)
    b.add("Wet", wet.weld(), "MI_WetStone", smooth=30.0)
    b.add("Fall", sheets.weld(), "MI_WaterFall", smooth=50.0)
    b.add("Foam", foam.weld(), "MI_Foam", smooth=60.0)
    b.footing, b.sink = "sits", 0.0


def p_waterfall(b, height=2.6, width=1.3, lithology="limestone"):
    """A waterfall over a rock face into its basin: the lip, the sheet, the wet
    and algae-dark stone the water has polished, and the broken water below."""
    rng, m = b.rng, Mesh()
    face = masonry(rng, width * 2.1, height, 0.7, course=0.3, ruin=0.25, block=(0.4, 0.8))
    m.add(face)
    for sx in (-1, 1):   # the rock shoulders that pinch the fall
        r = rock(width * 0.42, height * 0.30, 0.5, rng, angular=0.6, seg=3, relief=0.26,
                 flatten=False)
        r.transform(move=(sx * width * 0.85, height * 0.80, 0.22))
        m.add(r)
    basin_wall = Mesh()
    for ang in range(10):
        a = 2 * math.pi * ang / 10
        r = rock(0.30, 0.22, 0.26, rng, angular=0.7, seg=2, relief=0.24)
        r.transform(move=(1.25 * math.cos(a), 0.0, 1.05 + 1.0 * math.sin(a)))
        basin_wall.add(r)
    m.add(basin_wall)
    b.add("Mesh", m.weld(), mat(lithology), smooth=24.0)
    wet = slab(width * 1.1, height * 0.98, 0.06, rng, radius=0.01, seg=4, relief=0.012,
               frequency=5.0)
    wet.transform(move=(0.0, 0.0, 0.36))
    b.add("Wet", wet, "MI_WetStone", smooth=30.0)
    sheet = grid_sheet(width, height * 0.94, segs=(5, 6), bend=0.30, spread=0.16)
    sheet.transform(move=(0.0, height, 0.40))
    b.add("Fall", sheet, "MI_WaterFall", smooth=50.0)
    b.add("Water", water_plane(2.1, 1.9, rng, y=0.18, segs=5).transform(move=(0.0, 0.0, 1.05)),
          "MI_Water", smooth=60.0)
    b.add("Foam", water_plane(width * 1.2, 0.8, rng, y=0.22, segs=4)
          .transform(move=(0.0, 0.0, 0.62)), "MI_Foam", smooth=60.0)
    b.footing, b.sink = "embedded", 0.12


# -- organic ---------------------------------------------------------------


def p_fallen_log(b, length=3.2, radius=0.22):
    """A fallen trunk going soft: a bent tube, broken at both ends, with the
    stubs of two branches."""
    rng, m = b.rng, Mesh()
    seed = rng.randrange(1 << 20)
    steps = 7
    path, radii = [], []
    for i in range(steps):
        t = i / (steps - 1)
        path.append(((t - 0.5) * length,
                     radius * (0.95 + 0.25 * fbm(t * 3.0, 0, 0, seed, 2)),
                     0.42 * length * 0.22 * (fbm(t * 2.0, 5.0, 0, seed, 2) - 0.5)))
        radii.append(radius * (1.12 - 0.35 * t) * (0.9 + 0.2 * fbm(t * 4.0, 9.0, 0, seed, 2)))
    trunk = tube(path, radii, sides=9)
    trunk.displace(radius * 0.14, 3.0, seed + 11, octaves=3)
    m.add(trunk)
    for sign, t in ((1, 0.32), (-1, 0.68)):
        i = int(t * (steps - 1))
        p = path[i]
        stub = tube([p, (p[0] + sign * 0.30, p[1] + 0.26, p[2] + sign * 0.22)],
                    [radius * 0.42, radius * 0.16], sides=7)
        m.add(stub)
    m.weld().drop_to(0.0)
    b.add("Mesh", m, "MI_Bark", smooth=38.0)
    b.footing, b.sink = "sits", round(radius * 0.18, 3)


def p_roots_over_stone(b, size=(1.3, 0.8, 1.1)):
    """A strangler's roots gripping a fallen block: the stone the jungle is
    taking back, which is what a ruin in the Petén actually looks like."""
    rng = b.rng
    stone = rock(size[0] / 2, size[1] / 2, size[2] / 2, rng, angular=0.5, seg=3, relief=0.2)
    roots = Mesh()
    seed = rng.randrange(1 << 20)
    for k in range(5):
        a = 2 * math.pi * (k + 0.17 * fbm(k, 0, 0, seed, 1)) / 5
        top = (0.22 * size[0] * math.cos(a), size[1] * rng.uniform(0.85, 1.05),
               0.22 * size[2] * math.sin(a))
        mid = (0.56 * size[0] * math.cos(a), size[1] * 0.45,
               0.56 * size[2] * math.sin(a))
        toe = (0.78 * size[0] * math.cos(a) * rng.uniform(1.0, 1.4), 0.03,
               0.78 * size[2] * math.sin(a) * rng.uniform(1.0, 1.4))
        r = rng.uniform(0.045, 0.075)
        roots.add(tube([top, mid, toe], [r * 0.7, r, r * 0.55], sides=6))
    collar = torus(size[0] * 0.40, 0.05, major_seg=12, minor_seg=6)
    collar.transform(move=(0.0, size[1] * 0.72, 0.0))
    roots.add(collar)
    b.add("Mesh", stone, "MI_Limestone", smooth=32.0)
    b.add("Roots", roots.weld(), "MI_Bark", smooth=40.0)
    b.footing, b.sink = "sits", 0.0


def p_scale_figure(b):
    """Look-dev only: a 1.7 m post with shoulders, so a page shows real scale."""
    rng, m = b.rng, Mesh()
    m.add(slab(0.34, 0.04, 0.34, rng, radius=0.01, seg=2, relief=0.0))
    post = lathe([(0.075, 0.0), (0.07, 1.42), (0.085, 1.46), (0.08, 1.56)], sides=10)
    m.add(post)
    head = lathe([(0.0, 1.56), (0.085, 1.62), (0.085, 1.66), (0.0, 1.72)], sides=10)
    m.add(head)
    shoulders = slab(0.44, 0.07, 0.14, rng, radius=0.02, seg=2, relief=0.0)
    shoulders.transform(move=(0.0, 1.38, 0.0))
    m.add(shoulders)
    b.add("Mesh", m.weld(), "MI_Stucco", smooth=36.0)


# --------------------------------------------------------------------------
# The catalogue: which props exist, and the tags a level selects them by.
#
# This table is the library. Adding a prop is adding a builder above and a row
# here; nothing else in the repo changes, and the prop stays whether or not a
# level places it today. The vocabularies below are closed on purpose: a level
# that queries an unknown tag gets nothing back, and a typo should be caught by
# the validator rather than silently matching nothing.

FAMILIES = ("rock", "ruin", "carved", "ritual", "vessel", "water", "organic")
KINDS = ("boulder", "outcrop", "rubble", "scatter", "facing_stone", "wall", "corner",
         "column", "stair", "balustrade", "stela", "altar", "sculpture", "serpent_head",
         "ball_court_ring", "glyph_block", "brazier", "censer", "bowl", "jar", "sherds",
         "basin", "drain", "channel", "pool", "cascade", "waterfall", "log", "roots")
CULTURES = ("neutral", "maya-lowland", "mexica-highland")
ERAS = ("natural", "classic", "postclassic", "any")
BIOMES = ("any", "rainforest", "highland")
FOOTINGS = ("sits", "embedded", "wall_tenon")
SURFACES = ("dry", "mossy")


def P(pid, name, family, kind, build, args=None, *, culture=("neutral",), era=("any",),
      biome=("any",), tags=(), mossy=True, yaw_free=True):
    return {"id": pid, "name": name, "family": family, "kind": kind, "build": build,
            "args": dict(args or {}), "culture": list(culture), "era": list(era),
            "biome": list(biome), "tags": list(tags), "mossy": mossy, "yaw_free": yaw_free}


def _lith_tags(lithology):
    material, cultures = LITHOLOGY[lithology]
    biome = ("rainforest", "any") if "limestone" in lithology else ("highland", "any")
    return cultures, biome


def library():
    """Every prop in the library, in a stable order."""
    items = []

    # -- rock. Geology belongs to no culture, so these carry "neutral" and are
    # placed by biome: limestone in the lowland forest, volcanic in the highland.
    for lith in ("limestone", "basalt", "tezontle"):
        cultures, biome = _lith_tags(lith)
        for key, dims, seg in (("S", (0.52, 0.34, 0.46), 3),
                               ("M", (1.06, 0.70, 0.92), 4),
                               ("L", (2.10, 1.35, 1.80), 4)):
            for letter, angular in (("A", 0.22), ("B", 0.60)):
                items.append(P(f"boulder_{lith}_{key.lower()}{letter.lower()}",
                               f"Boulder{lith.title().replace('_', '')}_{key}{letter}",
                               "rock", "boulder", p_rock,
                               dict(size=dims, lithology=lith, angular=angular, seg=seg),
                               culture=cultures, era=("natural",), biome=biome,
                               tags=("natural", "rounded" if angular < 0.4 else "angular")))
    for lith in ("limestone", "basalt"):
        cultures, biome = _lith_tags(lith)
        items.append(P(f"outcrop_{lith}_a", f"Outcrop{lith.title()}_A", "rock", "outcrop",
                       p_outcrop, dict(size=(3.2, 1.5, 2.0), lithology=lith, blocks=4),
                       culture=cultures, era=("natural",), biome=biome, tags=("natural", "bedded")))
        items.append(P(f"rubble_pile_{lith}_a", f"RubblePile{lith.title()}_A", "rock", "rubble",
                       p_rubble_pile, dict(radius=0.95, height=0.5, count=11, lithology=lith),
                       culture=cultures, era=("any",), biome=biome, tags=("ruined", "loose")))
        items.append(P(f"rubble_scatter_{lith}_a", f"RubbleScatter{lith.title()}_A", "rock",
                       "scatter", p_rubble_scatter,
                       dict(spread=1.5, count=8, lithology=lith),
                       culture=cultures, era=("any",), biome=biome, tags=("ruined", "loose")))
    items.append(P("facing_stone_broken_a", "FacingStoneBroken_A", "rock", "facing_stone",
                   p_facing_stone, dict(size=(0.80, 0.34, 0.26), faced=True, broken=0.6),
                   culture=("maya-lowland", "mexica-highland"), era=("any",),
                   tags=("ruined", "dressed", "stucco")))
    items.append(P("facing_stone_plain_a", "FacingStonePlain_A", "rock", "facing_stone",
                   p_facing_stone, dict(size=(0.72, 0.30, 0.26), faced=False, broken=0.3),
                   culture=("maya-lowland", "mexica-highland"), era=("any",),
                   tags=("ruined", "dressed")))

    # -- ruin
    for key, length, height, ruin in (("short", 2.0, 1.1, 0.35), ("long", 4.2, 1.7, 0.5),
                                      ("stub", 1.3, 0.75, 0.6)):
        items.append(P(f"wall_{key}_a", f"Wall{key.title()}_A", "ruin", "wall", p_wall,
                       dict(length=length, height=height, depth=0.55, ruin=ruin),
                       culture=("maya-lowland",), era=("classic", "postclassic"),
                       biome=("rainforest", "any"), tags=("ruined", "masonry"), yaw_free=True))
    items.append(P("wall_long_stucco_a", "WallLongStucco_A", "ruin", "wall", p_wall,
                   dict(length=4.2, height=1.7, depth=0.55, ruin=0.4, stucco=True),
                   culture=("maya-lowland",), era=("classic",), biome=("rainforest", "any"),
                   tags=("ruined", "masonry", "stucco")))
    items.append(P("wall_corner_a", "WallCorner_A", "ruin", "corner", p_wall_corner,
                   dict(arm=2.0, height=1.6, ruin=0.6),
                   culture=("maya-lowland",), era=("classic", "postclassic"),
                   tags=("ruined", "masonry", "collapsed")))
    items.append(P("column_fallen_a", "ColumnFallen_A", "ruin", "column", p_column_fallen,
                   dict(drums=3, radius=0.27),
                   culture=("maya-lowland", "mexica-highland"), era=("postclassic",),
                   tags=("ruined", "toppled")))
    items.append(P("column_fallen_b", "ColumnFallen_B", "ruin", "column", p_column_fallen,
                   dict(drums=2, radius=0.33),
                   culture=("maya-lowland", "mexica-highland"), era=("postclassic",),
                   tags=("ruined", "toppled")))
    items.append(P("column_stump_a", "ColumnStump_A", "ruin", "column", p_column_stump,
                   dict(drums=2, radius=0.30),
                   culture=("maya-lowland", "mexica-highland"), era=("postclassic",),
                   tags=("ruined", "broken")))
    for steps, width, key in ((4, 2.2, "low"), (7, 3.0, "tall")):
        items.append(P(f"stair_{key}_a", f"Stair{key.title()}_A", "ruin", "stair", p_stair,
                       dict(steps=steps, width=width),
                       culture=("maya-lowland", "mexica-highland"), era=("any",),
                       tags=("masonry",), yaw_free=False))
    items.append(P("balustrade_a", "Balustrade_A", "ruin", "balustrade", p_balustrade,
                   dict(steps=7),
                   culture=("maya-lowland", "mexica-highland"), era=("any",),
                   tags=("masonry", "carved"), yaw_free=False))

    # -- carved
    for key, height, painted in (("tall", 2.8, False), ("short", 1.8, False),
                                 ("painted", 2.4, True)):
        items.append(P(f"stela_{key}_a", f"Stela{key.title()}_A", "carved", "stela", p_stela,
                       dict(height=height, painted=painted),
                       culture=("maya-lowland",), era=("classic",), biome=("rainforest", "any"),
                       tags=("carved", "painted") if painted else ("carved",), yaw_free=False))
    items.append(P("altar_round_a", "AltarRound_A", "carved", "altar", p_altar_round,
                   dict(radius=0.62, height=0.5, lithology="basalt"),
                   culture=("mexica-highland", "maya-lowland"), era=("any",),
                   tags=("carved", "ritual")))
    items.append(P("altar_block_a", "AltarBlock_A", "carved", "altar", p_altar_block,
                   dict(length=1.5, lithology="limestone"),
                   culture=("maya-lowland",), era=("classic",), tags=("ritual",)))
    for key, painted in (("plain", False), ("painted", True)):
        items.append(P(f"serpent_head_{key}_a", f"SerpentHead{key.title()}_A", "carved",
                       "serpent_head", p_serpent_head, dict(scale=1.0, painted=painted),
                       culture=("mexica-highland",), era=("postclassic",),
                       tags=("carved", "painted") if painted else ("carved",), yaw_free=False))
    items.append(P("chacmool_a", "Chacmool_A", "carved", "sculpture", p_chacmool,
                   dict(scale=1.0), culture=("mexica-highland",), era=("postclassic",),
                   tags=("carved", "figure"), yaw_free=False))
    items.append(P("standard_bearer_a", "StandardBearer_A", "carved", "sculpture",
                   p_standard_bearer, dict(scale=1.0), culture=("mexica-highland",),
                   era=("postclassic",), tags=("carved", "figure"), yaw_free=False))
    for key, painted in (("plain", False), ("painted", True)):
        items.append(P(f"ball_court_ring_{key}_a", f"BallCourtRing{key.title()}_A", "carved",
                       "ball_court_ring", p_ball_court_ring, dict(radius=0.42, painted=painted),
                       culture=("maya-lowland", "mexica-highland"), era=("any",),
                       tags=("carved", "ball_game"), yaw_free=False))
    items.append(P("glyph_block_a", "GlyphBlock_A", "carved", "glyph_block", p_glyph_block,
                   dict(), culture=("maya-lowland",), era=("classic",), tags=("carved",)))

    # -- ritual and vessels
    for key, radius, height in (("small", 0.34, 0.58), ("large", 0.50, 0.86)):
        items.append(P(f"brazier_{key}_a", f"Brazier{key.title()}_A", "ritual", "brazier",
                       p_brazier, dict(radius=radius, height=height, lithology="basalt"),
                       culture=("mexica-highland", "maya-lowland"), era=("any",),
                       tags=("ritual", "fire")))
    items.append(P("censer_a", "Censer_A", "ritual", "censer", p_censer, dict(),
                   culture=("maya-lowland", "mexica-highland"), era=("any",),
                   tags=("ritual", "clay"), mossy=False))
    items.append(P("offering_bowl_a", "OfferingBowl_A", "ritual", "bowl", p_offering_bowl,
                   dict(), culture=("maya-lowland", "mexica-highland"), era=("any",),
                   tags=("ritual",)))
    for key, height, radius in (("large", 0.70, 0.30), ("small", 0.44, 0.20)):
        items.append(P(f"olla_{key}_a", f"Olla{key.title()}_A", "vessel", "jar", p_olla,
                       dict(height=height, radius=radius),
                       culture=("maya-lowland", "mexica-highland"), era=("any",),
                       tags=("clay", "household"), mossy=False))
    items.append(P("tripod_vessel_a", "TripodVessel_A", "vessel", "bowl", p_tripod_vessel,
                   dict(), culture=("maya-lowland",), era=("classic",),
                   tags=("clay", "painted"), mossy=False))
    items.append(P("sherds_a", "Sherds_A", "vessel", "sherds", p_sherds, dict(),
                   culture=("maya-lowland", "mexica-highland"), era=("any",),
                   tags=("clay", "broken"), mossy=False))

    # -- water
    items.append(P("basin_a", "Basin_A", "water", "basin", p_basin, dict(),
                   culture=("maya-lowland", "mexica-highland"), era=("any",),
                   tags=("water",)))
    items.append(P("basin_dry_a", "BasinDry_A", "water", "basin", p_basin, dict(water=False),
                   culture=("maya-lowland", "mexica-highland"), era=("any",),
                   tags=("ruined",)))
    items.append(P("drain_a", "Drain_A", "water", "drain", p_drain, dict(),
                   culture=("maya-lowland",), era=("classic",), tags=("water", "masonry"),
                   yaw_free=False))
    items.append(P("channel_a", "Channel_A", "water", "channel", p_channel, dict(),
                   culture=("maya-lowland", "mexica-highland"), era=("any",),
                   tags=("water", "masonry"), yaw_free=False))
    items.append(P("pool_a", "Pool_A", "water", "pool", p_pool, dict(),
                   culture=("maya-lowland", "mexica-highland"), era=("any",),
                   tags=("water", "masonry"), yaw_free=False))
    items.append(P("cascade_a", "Cascade_A", "water", "cascade", p_cascade, dict(),
                   culture=("neutral",), era=("any",), tags=("water",), yaw_free=False))
    items.append(P("waterfall_a", "Waterfall_A", "water", "waterfall", p_waterfall, dict(),
                   culture=("neutral",), era=("natural",), biome=("rainforest", "any"),
                   tags=("water",), yaw_free=False))

    # -- organic. The plants themselves are another generator's
    # (tools/assets/plants.py); what is here is dead wood and root over stone.
    for key, length, radius in (("long", 3.4, 0.24), ("short", 1.9, 0.18)):
        items.append(P(f"fallen_log_{key}_a", f"FallenLog{key.title()}_A", "organic", "log",
                       p_fallen_log, dict(length=length, radius=radius),
                       culture=("neutral",), era=("natural",), biome=("rainforest", "any"),
                       tags=("natural", "wood"), mossy=True))
    items.append(P("roots_over_stone_a", "RootsOverStone_A", "organic", "roots",
                   p_roots_over_stone, dict(),
                   culture=("neutral",), era=("natural",), biome=("rainforest", "any"),
                   tags=("natural", "wood", "ruined")))
    return items


LOOKDEV_ONLY = P("scale_figure", "ScaleFigure", "rock", "boulder", p_scale_figure, dict(),
                 mossy=False)


# --------------------------------------------------------------------------
# Writing it out

HEADER = ("; SPDX-License-Identifier: GPL-2.0-or-later\n"
          "; Generated by tools/assets/props.py; edit the generator, not this file.\n")
# Water surfaces do not cast shadows: a sheet of falling water casting a hard
# shadow on the stone behind it is the first thing that gives it away.
NO_SHADOW = {"MI_Water", "MI_Foam", "MI_WaterFall"}
MODELS = "res://assets/models/props"
MATERIALS_RES = "res://assets/materials/props"


class Res:
    """Godot text resource writer, the same shape as gen_theme_scenes.Resource
    but stamped with this generator's name."""

    def __init__(self):
        self.ext = []
        self.body = []

    def ext_id(self, rtype, path):
        key = (rtype, path)
        if key not in self.ext:
            self.ext.append(key)
        return f'ExtResource("{self.ext.index(key) + 1}")'

    def text(self, head):
        out = [head.format(steps=len(self.ext) + 1), HEADER]
        for i, (rtype, path) in enumerate(self.ext):
            out.append(f'[ext_resource type="{rtype}" path="{path}" id="{i + 1}"]')
        if self.ext:
            out.append("")
        out.extend(self.body)
        return "\n".join(out).rstrip() + "\n"


def retitle(text):
    """Point a generated file's header line at this generator; the material
    body comes from the shared writer, but the content comes from here."""
    return "\n".join(
        "; Generated by tools/assets/props.py; edit MATERIALS in that file, not this one."
        if line.startswith("; Generated by ") else line for line in text.split("\n"))


def part_stem(item, part):
    return f"SM_Prop_{item['name']}" + ("" if part == "Mesh" else f"_{part}")


def size_class(size):
    longest = max(size)
    for name, limit in SIZE_CLASS:
        if longest <= limit:
            return name
    return "huge"


def union_bounds(parts):
    lo = [1e9, 1e9, 1e9]
    hi = [-1e9, -1e9, -1e9]
    for _name, mesh, _mat, _smooth in parts:
        a, c = mesh.bounds()
        for i in range(3):
            lo[i], hi[i] = min(lo[i], a[i]), max(hi[i], c[i])
    return lo, hi


def prop_scene(item, parts):
    """The scene a level instances: one MeshInstance3D per (mesh, material)."""
    r = Res()
    r.body.append(f'[node name="{item["name"]}" type="Node3D"]')
    for part, material in parts:
        mesh = r.ext_id("Mesh", f"{MODELS}/{item['family']}/{part_stem(item, part)}.obj")
        mat = r.ext_id("Material", f"{MATERIALS_RES}/{material}.tres")
        r.body += ["", f'[node name="{part}" type="MeshInstance3D" parent="."]',
                   f"mesh = {mesh}", f"surface_material_override/0 = {mat}"]
        if material in NO_SHADOW:
            r.body.append("cast_shadow = 0")
    return r.text("[gd_scene load_steps={steps} format=3]")


def catalogue_entry(item, build, parts, surface, scene_res, tris, size):
    return {
        "scene": scene_res,
        "family": item["family"],
        "kind": item["kind"],
        "culture": item["culture"],
        "era": item["era"],
        "biome": item["biome"],
        "tags": sorted(set(item["tags"]) | ({"mossy"} if surface == "mossy" else set())),
        "surface": surface,
        "size_class": size_class(size),
        "size_m": size,
        "footing": build.footing,
        "sink_m": round(build.sink, 3),
        "yaw_free": item["yaw_free"],
        "triangles": tris,
        "materials": sorted({m for _n, m in parts}),
        "seed": f"props:{item['id']}",
    }


# --------------------------------------------------------------------------
# Look-dev. Every prop is judged at real metre size, on the plaza floor, under
# the temple theme's sun and sky, from the three-quarter view that shows its
# form and from the game's near-top-down camera that is the only view a player
# ever gets. A 1.7 m scale post stands at the end of every row.


def look_at(eye, target, up=(0.0, 1.0, 0.0)):
    """A Transform3D looking from `eye` at `target`, written basis-rows first."""
    f = [target[i] - eye[i] for i in range(3)]
    fl = math.sqrt(sum(c * c for c in f)) or 1.0
    z = [-c / fl for c in f]
    x = [up[1] * z[2] - up[2] * z[1], up[2] * z[0] - up[0] * z[2], up[0] * z[1] - up[1] * z[0]]
    xl = math.sqrt(sum(c * c for c in x)) or 1.0
    x = [c / xl for c in x]
    y = [z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]]
    rows = [x[0], y[0], z[0], x[1], y[1], z[1], x[2], y[2], z[2], eye[0], eye[1], eye[2]]
    return "Transform3D(" + ", ".join(f"{v:.6f}" for v in rows) + ")"


def lookdev_shell(r, p, ground=46.0):
    """Sub-resources and nodes for the sky, sun, exposure and plaza floor.

    The values are the temple theme's, read from its parameter file, but none
    of the temple is in the scene: a prop has to hold up on its own before it
    is judged next to the board.
    """
    env, light = p["environment"], p["lighting"]
    exposure, sun = light["exposure"], light["sun"]
    c = scenes.color
    sub = [
        '[sub_resource type="ProceduralSkyMaterial" id="sky_mat"]',
        f"sky_top_color = {c(env['sky_top'])}",
        f"sky_horizon_color = {c(env['sky_horizon'])}",
        f"ground_bottom_color = {c(env['ground_bottom'])}",
        f"ground_horizon_color = {c(env['sky_horizon'])}",
        "sun_angle_max = 20.0", "",
        '[sub_resource type="Sky" id="sky"]', 'sky_material = SubResource("sky_mat")', "",
        '[sub_resource type="CameraAttributesPractical" id="camera"]',
        f"exposure_sensitivity = {exposure['sensitivity']}",
        f"exposure_multiplier = {exposure['multiplier']}", "",
        '[sub_resource type="Environment" id="env"]',
        "background_mode = 2", f"background_intensity = {light['sky_nits']}",
        'sky = SubResource("sky")', "ambient_light_source = 3",
        f"ambient_light_energy = {env['ambient_energy']}", "reflected_light_source = 2",
        "tonemap_mode = 4", f"tonemap_agx_white = {light['agx_white']}",
        f"tonemap_agx_contrast = {light['agx_contrast']}",
        "ssao_enabled = true", "ssil_enabled = true", "glow_enabled = true",
        "glow_intensity = 0.6", "glow_bloom = 0.05", "glow_hdr_threshold = 1.1",
        "adjustment_enabled = true", f"adjustment_saturation = {env['saturation']}", "",
        '[sub_resource type="PlaneMesh" id="ground"]',
        f"size = Vector2({ground}, {ground})", "subdivide_width = 8", "subdivide_depth = 8", "",
    ]
    plaza = r.ext_id("Material", f"{MATERIALS_RES}/MI_Plaza.tres")
    nodes = [
        "", '[node name="WorldEnvironment" type="WorldEnvironment" parent="."]',
        'environment = SubResource("env")', 'camera_attributes = SubResource("camera")',
        "", '[node name="Sun" type="DirectionalLight3D" parent="."]',
        f"transform = {env['sun']['transform']}",
        f"light_intensity_lux = {sun['lux']}", f"light_temperature = {sun['kelvin']}",
        f"light_color = {c(sun['color'])}",
        f"light_angular_distance = {sun['angular_distance']}", "shadow_enabled = true",
        "directional_shadow_max_distance = 60.0",
    ]
    # The theme's fill light, when it has one. Without it a prop is judged
    # under a different key than the board it will stand on, which is the whole
    # thing these pages exist to avoid.
    fill = light.get("sky_fill")
    if fill:
        nodes += [
            "", '[node name="SkyFill" type="AreaLight3D" parent="." groups=["quality:area_light"]]',
            f"transform = {fill['transform']}", f"light_intensity_lumens = {fill['lumens']}",
            f"light_color = {c(fill['color'])}", "light_bake_mode = 0",
            f"area_size = Vector2({fill['size'][0]}, {fill['size'][1]})",
            f"area_range = {fill['range']}",
        ]
    nodes += [
        "", '[node name="Ground" type="MeshInstance3D" parent="."]',
        'mesh = SubResource("ground")', f"surface_material_override/0 = {plaza}",
    ]
    return sub, nodes


def lookdev_page(name, row, p, top_down):
    """One look-dev page: a row of props with a scale post at each end."""
    r = Res()
    r.body.append(f'[node name="{name}" type="Node3D"]')
    sub, nodes = lookdev_shell(r, p)
    gap = 1.0
    width = sum(size[0] for _res, size, _label in row) + gap * (len(row) + 1)
    x = -width / 2 + gap
    placed = []
    for scene_res, size, label in row:
        placed.append((scene_res, x + size[0] / 2, label))
        x += size[0] + gap
    scale_res = f"{MODELS}/rock/PF_Prop_ScaleFigure.tscn"
    for sx in (-1, 1):
        placed.append((scale_res, sx * (width / 2 + 0.7), f"Scale{'L' if sx < 0 else 'R'}"))
    for scene_res, px, label in placed:
        ref = r.ext_id("PackedScene", scene_res)
        nodes += ["", f'[node name="{label}" parent="." instance={ref}]',
                  f"transform = Transform3D(1, 0, 0, 0, 1, 0, 0, 0, 1, {round(px, 4)}, 0, 0)"]
    reach = max(width, 4.0)
    if top_down:
        # The game's camera: 15 degrees off straight down, toward the player.
        height = reach * 0.92 + 3.0
        eye = (0.0, height * math.cos(math.radians(15.0)), height * math.sin(math.radians(15.0)))
        fov = 45.0
    else:
        eye = (reach * 0.20, 2.35, reach * 0.60 + 3.1)
        fov = 48.0
    nodes += ["", '[node name="Camera" type="Camera3D" parent="."]',
              f"transform = {look_at(eye, (0.0, 0.55, 0.0))}", f"fov = {fov}"]
    # Every [sub_resource] has to come before the first [node] block.
    r.body = sub + [r.body[0]] + nodes
    return r.text("[gd_scene load_steps={steps} format=3]")


def chunks(seq, n):
    return [seq[i:i + n] for i in range(0, len(seq), n)]


# --------------------------------------------------------------------------


def check_vocabulary(item):
    for key, allowed in (("family", FAMILIES), ("kind", KINDS)):
        if item[key] not in allowed:
            raise SystemExit(f"{item['id']}: {key} '{item[key]}' is not one of {allowed}")
    for key, allowed in (("culture", CULTURES), ("era", ERAS), ("biome", BIOMES)):
        for value in item[key]:
            if value not in allowed:
                raise SystemExit(f"{item['id']}: {key} '{value}' is not one of {allowed}")


def main():
    ap = argparse.ArgumentParser(description="Generate the shared prop library.")
    ap.add_argument("--only", nargs="*", metavar="ID", help="generate only these catalogue ids")
    ap.add_argument("--list", action="store_true", help="print the catalogue ids and stop")
    ap.add_argument("--no-lookdev", action="store_true", help="skip the look-dev pages")
    args = ap.parse_args()

    params = json.loads(PARAMS.read_text())
    theme = json.loads(THEME_FOR_LOOKDEV.read_text())
    scenes.TEXTURE_SETS.update(params["textures"]["sets"])
    scenes.EMISSION_NITS["scale"] = 3072007.0 / theme["lighting"]["exposure"]["sensitivity"]

    items = library()
    for item in items:
        check_vocabulary(item)
    if args.list:
        for item in items:
            print(f"{item['id']:34s} {item['family']:8s} {item['kind']}")
        return
    partial = bool(args.only)
    if partial:
        wanted = set(args.only)
        unknown = wanted - {i["id"] for i in items}
        if unknown:
            raise SystemExit(f"unknown id(s): {', '.join(sorted(unknown))}")
        items = [i for i in items if i["id"] in wanted]

    for name, spec in MATERIALS.items():
        write(ASSETS / "materials/props" / f"{name}.tres", retitle(scenes.material(spec, "props")))
        if "hi" in spec:
            write(ASSETS / "materials/props/hi" / f"{name}.tres",
                  retitle(scenes.material(spec, "props", hi=True)))

    catalogue, report, errors, pages, expected = {}, [], [], {}, set()
    for item in items + [LOOKDEV_ONLY]:
        rng = random.Random(f"props:{item['id']}")
        build = Build(rng)
        item["build"](build, **item["args"])
        if not build.parts:
            errors.append(f"{item['id']}: produced no geometry")
            continue
        folder = ASSETS / "models/props" / item["family"]
        dry = []
        for part, mesh, material, smooth in build.parts:
            stem = part_stem(item, part)
            write(folder / f"{stem}.obj", obj_text(mesh, stem, smooth))
            expected.add(folder / f"{stem}.obj")
            dry.append((part, material))
        tris = sum(mesh.tri_count() for _p, mesh, _m, _s in build.parts)
        scene = folder / f"PF_Prop_{item['name']}.tscn"
        write(scene, prop_scene(item, dry))
        expected.add(scene)
        scene_res = f"{MODELS}/{item['family']}/{scene.name}"
        if item is LOOKDEV_ONLY:
            continue
        lo, hi = union_bounds(build.parts)
        size = [round(hi[i] - lo[i], 3) for i in range(3)]
        cls = size_class(size)
        report.append((item["id"], cls, tris, size))
        if tris > TRI_BUDGET[cls]:
            errors.append(f"{item['id']}: {tris} triangles, {cls} budget {TRI_BUDGET[cls]}; "
                          "lower a segment count or split the prop")
        catalogue[item["id"]] = catalogue_entry(item, build, dry, "dry", scene_res, tris, size)
        pages.setdefault(item["family"], []).append((scene_res, size, item["name"]))

        if item["mossy"]:
            stone = Mesh()
            for _p, mesh, material, _s in build.parts:
                if material in STONE_MATERIALS or material == "MI_Bark":
                    stone.add(mesh)
            shell = moss_shell(stone.weld(), random.Random(f"props:{item['id']}:moss"))
            if not shell.faces:
                errors.append(f"{item['id']}: asked for a moss variant but nothing faces up")
                continue
            stem = part_stem(item, "Moss")
            write(folder / f"{stem}.obj", obj_text(shell, stem, 40.0))
            expected.add(folder / f"{stem}.obj")
            mossy_item = dict(item, name=f"{item['name']}_Moss", id=f"{item['id']}_moss")
            mossy_scene = folder / f"PF_Prop_{item['name']}_Moss.tscn"
            # The moss variant reuses the dry meshes and adds the shell, so the
            # two share one stone mesh in memory and cost one extra draw call.
            r = Res()
            r.body.append(f'[node name="{mossy_item["name"]}" type="Node3D"]')
            for part, material in dry + [("Moss", "MI_Moss")]:
                mesh_res = f"{MODELS}/{item['family']}/{part_stem(item, part)}.obj"
                mesh_id = r.ext_id("Mesh", mesh_res)
                mat_id = r.ext_id("Material", f"{MATERIALS_RES}/{material}.tres")
                r.body += ["", f'[node name="{part}" type="MeshInstance3D" parent="."]',
                           f"mesh = {mesh_id}", f"surface_material_override/0 = {mat_id}"]
                if material in NO_SHADOW:
                    r.body.append("cast_shadow = 0")
            write(mossy_scene, r.text("[gd_scene load_steps={steps} format=3]"))
            expected.add(mossy_scene)
            moss_tris = tris + shell.tri_count()
            report.append((mossy_item["id"], cls, moss_tris, size))
            if moss_tris > TRI_BUDGET[cls]:
                errors.append(f"{mossy_item['id']}: {moss_tris} triangles, {cls} budget "
                              f"{TRI_BUDGET[cls]}; thin the moss coverage")
            catalogue[mossy_item["id"]] = catalogue_entry(
                mossy_item, build, dry + [("Moss", "MI_Moss")], "mossy",
                f"{MODELS}/{item['family']}/{mossy_scene.name}", moss_tris, size)

    if not partial:
        root = ASSETS / "models/props"
        for stale in sorted(root.rglob("*")):
            if stale.is_file() and stale.suffix in (".obj", ".tscn") and stale not in expected:
                stale.unlink()
                stale.with_name(stale.name + ".import").unlink(missing_ok=True)
        write(CATALOGUE, json.dumps(write_catalogue(catalogue), indent=2) + "\n")
        if not args.no_lookdev:
            written = []
            for family, row in sorted(pages.items()):
                for i, page in enumerate(chunks(row, 6), 1):
                    for suffix, top in (("", False), ("_Top", True)):
                        name = f"LD_Props_{family.title()}{i}{suffix}"
                        write(ASSETS / "lookdev" / f"{name}.tscn",
                              lookdev_page(name, page, theme, top))
                        written.append(name)
            print(f"look-dev: {len(written)} pages")

    for pid, cls, tris, size in report:
        print(f"{pid:40s} {cls:7s} {tris:6d} tris  {size[0]:.2f} x {size[1]:.2f} x {size[2]:.2f} m")
    print(f"{len(catalogue)} catalogue entries, "
          f"{sum(t for _i, _c, t, _s in report)} triangles in the library")
    if errors:
        sys.exit("\n".join(errors))


def write_catalogue(props):
    """The catalogue a level queries.

    Until docs/reference/asset-library.md fixes the schema, this is the shape
    the library uses, and docs/how-to/add-a-prop.md documents the selection
    rule: filter by every key the query names (a prop matches a key when its
    own list and the query's share a value, and "any" matches anything), sort
    the survivors by id, then draw from that list with a generator seeded from
    the level's seed, so a level dresses itself the same way every time.
    """
    return {
        "$description": "Shared prop library catalogue (sRGB-free data). Generated by "
                        "tools/assets/props.py; edit that file, not this one. "
                        "SPDX-License-Identifier: GPL-2.0-or-later",
        "version": LIBRARY_VERSION,
        "vocabulary": {
            "family": list(FAMILIES), "kind": list(KINDS), "culture": list(CULTURES),
            "era": list(ERAS), "biome": list(BIOMES), "surface": list(SURFACES),
            "footing": list(FOOTINGS), "size_class": [n for n, _ in SIZE_CLASS],
        },
        "footing_meaning": {
            "sits": "rests on the terrain with its own base at y = 0",
            "embedded": "sink it by sink_m so it reads as part of the ground",
            "wall_tenon": "its tenon goes into a wall; not a free-standing placement",
        },
        "props": dict(sorted(props.items())),
    }


if __name__ == "__main__":
    main()


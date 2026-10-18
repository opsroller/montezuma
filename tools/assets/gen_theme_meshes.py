# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Generate a board theme's meshes (artifact pieces, board, backdrop) with Blender.

Runs headless, either as Blender's Python module or inside Blender:

    python3 tools/assets/gen_theme_meshes.py [tools/assets/themes/temple.json]
    blender -b --factory-startup -P tools/assets/gen_theme_meshes.py -- [params.json]

Writes one Wavefront OBJ per (asset, material) part under
game/assets/models/<theme>/, so each Godot MeshInstance3D gets exactly one
surface and one material override. Units are metres, one cell is
`cell_size`; Blender Z-up is exported as Godot Y-up with Blender -Y as Godot +Z
(toward the player). Same parameters and seed give the same files.
"""

import json
import math
import random
import sys
from pathlib import Path

import bpy  # first: as a Python module, bpy makes bmesh importable
import bmesh

ROOT = Path(__file__).resolve().parents[2]
TRI_BUDGET_PIECE = 2000
TRI_BUDGET_BOARD = 20000


# --------------------------------------------------------------------------
# bmesh helpers


def ring(bm, outline, scale, z, twist=0.0):
    """Add a closed ring of verts: `outline` (x, y) scaled about the origin at height z."""
    c, s = math.cos(twist), math.sin(twist)
    return [bm.verts.new((scale * (x * c - y * s), scale * (x * s + y * c), z)) for x, y in outline]


def bridge(bm, a, b):
    n = len(a)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((a[i], a[j], b[j], b[i]))


def cap(bm, r, up):
    bm.faces.new(r if up else list(reversed(r)))


def lathe_outline(bm, outline, profile, twist_step=0.0):
    """Solid of stacked rings: profile is [(scale, z), ...] from bottom to top."""
    rings = [ring(bm, outline, s, z, twist_step * i) for i, (s, z) in enumerate(profile)]
    for a, b in zip(rings, rings[1:]):
        bridge(bm, a, b)
    cap(bm, rings[0], up=False)
    cap(bm, rings[-1], up=True)


def box(bm, cx, cy, z0, sx, sy, sz):
    outline = [(-sx / 2, -sy / 2), (sx / 2, -sy / 2), (sx / 2, sy / 2), (-sx / 2, sy / 2)]
    verts_before = set(bm.verts)
    lathe_outline(bm, outline, [(1.0, z0), (1.0, z0 + sz)])
    for v in set(bm.verts) - verts_before:
        v.co.x += cx
        v.co.y += cy


def circle(n, r, phase=0.0, sx=1.0, sy=1.0):
    return [(sx * r * math.cos(phase + 2 * math.pi * i / n), sy * r * math.sin(phase + 2 * math.pi * i / n)) for i in range(n)]


def bevel_all(bm, offset, segments=1, angle_deg=30.0):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.normal_update()
    edges = [e for e in bm.edges if e.calc_face_angle(0.0) > math.radians(angle_deg)]
    if edges:
        bmesh.ops.bevel(bm, geom=edges, offset=offset, segments=segments, affect="EDGES", profile=0.5,
                        clamp_overlap=True)


def transform(bm, rot_z=0.0, dx=0.0, dy=0.0, dz=0.0):
    c, s = math.cos(rot_z), math.sin(rot_z)
    for v in bm.verts:
        x, y = v.co.x, v.co.y
        v.co.x, v.co.y = x * c - y * s + dx, x * s + y * c + dy
        v.co.z += dz


# --------------------------------------------------------------------------
# Artifact pieces. Each kind differs in silhouette as well as colour and
# value, so the board reads in greyscale and under colour-vision deficiency.


def piece_round_gem(bm, r):
    outline = circle(16, r * 0.92)
    lathe_outline(bm, outline, [(0.45, 0.02), (1.0, 0.13), (1.0, 0.16), (0.8, 0.29), (0.52, 0.35)],
                  twist_step=math.pi / 16)
    return 0.0  # smooth angle: fully faceted


def piece_pyramid(bm, r):
    w = 2 * r * 0.94
    z = 0.02
    tiers = [(size, 0.085 if i < 3 else 0.07) for i, size in enumerate((1.0, 0.8, 0.6, 0.42))]
    tiers.append((0.24, 0.07))  # shrine on top
    for size, h in tiers:
        # Bevel each block on its own: bevelling touching blocks together is order-dependent.
        sub = bmesh.new()
        box(sub, 0, 0, z, w * size, w * size, h)
        bevel_all(sub, 0.012)
        merge_into(bm, sub)
        z += h
    return 0.0


def piece_sun(bm, r):
    rays = 8
    star = []
    for i in range(rays * 2):
        rr = r * (1.0 if i % 2 == 0 else 0.6)
        a = math.pi / 2 + math.pi * i / rays
        star.append((rr * math.cos(a), rr * math.sin(a)))
    lathe_outline(bm, star, [(0.9, 0.02), (1.0, 0.07), (0.94, 0.11)])
    lathe_outline(bm, circle(24, r * 0.5), [(1.0, 0.1), (1.0, 0.15), (0.82, 0.2), (0.6, 0.23)])
    lathe_outline(bm, circle(16, r * 0.26), [(1.0, 0.22), (0.9, 0.28), (0.5, 0.31)])
    return 0.0


def piece_leaf(bm, r):
    n = 28
    outline = []
    for i in range(n):
        a = 2 * math.pi * i / n
        x = 0.78 * math.sin(a) * math.sin(a / 2)
        y = math.cos(a)
        outline.append((x * r, y * r * 0.98))
    lathe_outline(bm, outline, [(0.55, 0.02), (0.92, 0.08), (1.0, 0.13), (0.88, 0.22), (0.62, 0.29), (0.28, 0.33)])
    transform(bm, rot_z=-math.pi / 4)
    return 50.0  # polished jade: smooth, crisp girdle


def piece_diamond(bm, r):
    corners = [(0.0, r), (0.74 * r, 0.0), (0.0, -r), (-0.74 * r, 0.0)]
    outline = []
    for i, a in enumerate(corners):
        b = corners[(i + 1) % 4]
        for t in (0.0, 0.5):
            outline.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
    lathe_outline(bm, outline, [(0.45, 0.02), (1.0, 0.12), (1.0, 0.15), (0.72, 0.28), (0.42, 0.34)])
    return 0.0


def piece_hex_crystal(bm, r):
    hexa = circle(6, r * 0.62, phase=math.pi / 6)
    lathe_outline(bm, hexa, [(0.85, 0.02), (1.0, 0.06), (1.0, 0.3), (0.55, 0.46), (0.06, 0.52)])
    for ang, tilt_dir in ((math.radians(150), -1), (math.radians(-35), 1)):
        sub = bmesh.new()
        lathe_outline(sub, circle(6, r * 0.24, phase=math.pi / 6), [(1.0, 0.0), (1.0, 0.16), (0.08, 0.3)])
        tilt = math.radians(32)
        for v in sub.verts:
            # tilt outward around the tangent axis, then place at the crystal's flank
            x, z = v.co.x, v.co.z
            v.co.x, v.co.z = x * math.cos(tilt) + z * math.sin(tilt), -x * math.sin(tilt) + z * math.cos(tilt)
        transform(sub, rot_z=ang, dx=math.cos(ang) * r * 0.52, dy=math.sin(ang) * r * 0.52, dz=0.02)
        merge_into(bm, sub)
    return 0.0


def piece_crescent(bm, r):
    seg_t, seg_c = 22, 12
    arc_r = r * 0.6
    a0, a1 = math.radians(-145), math.radians(145)
    rings = []
    for i in range(seg_t + 1):
        t = i / seg_t
        a = a0 + (a1 - a0) * t
        thick = r * (0.06 + 0.34 * math.sin(math.pi * t) ** 0.7)
        cx, cy = arc_r * math.cos(a) - r * 0.12, arc_r * math.sin(a)
        nx, ny = math.cos(a), math.sin(a)
        verts = []
        for j in range(seg_c):
            b = 2 * math.pi * j / seg_c
            radial = thick * math.cos(b)
            up = thick * 0.75 * math.sin(b)
            verts.append(bm.verts.new((cx + nx * radial, cy + ny * radial, 0.02 + thick * 0.75 + up)))
        rings.append(verts)
    for a, b in zip(rings, rings[1:]):
        bridge(bm, a, b)
    cap(bm, rings[0], up=False)
    cap(bm, rings[-1], up=True)
    transform(bm, rot_z=math.radians(-40) + math.pi)
    return 70.0


PIECES = {
    "round_gem": piece_round_gem,
    "pyramid": piece_pyramid,
    "sun": piece_sun,
    "leaf": piece_leaf,
    "diamond": piece_diamond,
    "hex_crystal": piece_hex_crystal,
    "crescent": piece_crescent,
}


def merge_into(dst, src):
    mesh = bpy.data.meshes.new("tmp_merge")
    src.to_mesh(mesh)
    src.free()
    dst.from_mesh(mesh)
    bpy.data.meshes.remove(mesh)


# --------------------------------------------------------------------------
# Board: carved tiles in a stone frame with a step-fret border.


def board_parts(p, rng):
    w, h = p["board"]["width"], p["board"]["height"]
    cs = p["cell_size"]
    gap, th = p["board"]["tile_gap"], p["board"]["tile_height"]
    fw, fh = p["board"]["frame_width"], p["board"]["frame_height"]
    half_w, half_h = w * cs / 2, h * cs / 2

    light, dark = bmesh.new(), bmesh.new()
    for y in range(h):
        for x in range(w):
            target = light if (x + y) % 2 == 0 else dark
            cx = (x - (w - 1) / 2) * cs
            cy = -(y - (h - 1) / 2) * cs  # board y grows toward the player (Blender -Y)
            sub = bmesh.new()
            # tiles vary slightly in height so the floor reads as laid stone
            jitter = rng.uniform(-0.008, 0.004)
            box(sub, cx, cy, -th, cs - gap, cs - gap, th + jitter)
            bevel_all(sub, 0.025)
            merge_into(target, sub)

    base = bmesh.new()
    box(base, 0, 0, -0.42, 2 * half_w + 0.02, 2 * half_h + 0.02, 0.42 - th + 0.01)

    frame = bmesh.new()
    outer_w = 2 * half_w + 2 * fw
    for sx, sy, cx, cy in (
        (outer_w, fw, 0, half_h + fw / 2),
        (outer_w, fw, 0, -half_h - fw / 2),
        (fw, 2 * half_h, half_w + fw / 2, 0),
        (fw, 2 * half_h, -half_w - fw / 2, 0),
    ):
        sub = bmesh.new()
        box(sub, cx, cy, -0.42, sx, sy, 0.42 + fh)
        bevel_all(sub, 0.03)
        merge_into(frame, sub)

    # Step-fret relief: stepped blocks marching along each side of the frame.
    fret = bmesh.new()
    step = 0.5
    for side in range(4):
        length = 2 * half_w if side < 2 else 2 * half_h
        n = int(round(length / step))
        for i in range(n):
            t = -length / 2 + (i + 0.5) * step
            for k, (off, size) in enumerate(((-0.1, 0.34), (0.08, 0.2))):
                lift = 0.035 * (k + 1)
                bx, by = size if side < 2 else 0.16, 0.16 if side < 2 else size
                o = off if i % 2 == 0 else -off
                if side == 0:
                    cx, cy = t, half_h + fw / 2 + o * 0.6
                elif side == 1:
                    cx, cy = t, -half_h - fw / 2 - o * 0.6
                elif side == 2:
                    cx, cy = half_w + fw / 2 + o * 0.6, t
                else:
                    cx, cy = -half_w - fw / 2 - o * 0.6, t
                sub = bmesh.new()
                box(sub, cx, cy, fh - 0.01, bx, by, lift)
                bevel_all(sub, 0.008)
                merge_into(fret, sub)

    gold = bmesh.new()
    for sx in (-1, 1):
        for sy in (-1, 1):
            cx, cy = sx * (half_w + fw / 2), sy * (half_h + fw / 2)
            sub = bmesh.new()
            box(sub, cx, cy, fh - 0.01, fw * 0.8, fw * 0.8, 0.06)
            bevel_all(sub, 0.02)
            merge_into(frame, sub)
            sub = bmesh.new()
            lathe_outline(sub, circle(20, fw * 0.24), [(1.0, fh + 0.04), (1.0, fh + 0.08), (0.75, fh + 0.12), (0.4, fh + 0.14)])
            transform(sub, dx=cx, dy=cy)
            merge_into(gold, sub)

    return {
        "TilesLight": (light, "MI_StoneTileLight", 0.0),
        "TilesDark": (dark, "MI_StoneTileDark", 0.0),
        "Base": (base, "MI_StoneDark", 0.0),
        "Frame": (frame, "MI_StoneFrame", 0.0),
        "Fret": (fret, "MI_StoneFrame", 0.0),
        "Gold": (gold, "MI_Gold", 40.0),
    }


# --------------------------------------------------------------------------
# Backdrop: the temple platform the board sits on, braziers and jungle floor.


def backdrop_parts(p, rng):
    cs = p["cell_size"]
    half = p["board"]["width"] * cs / 2 + p["board"]["frame_width"]
    tiers = p["backdrop"]["tiers"]
    stone, stone_dark, bronze, flame, foliage, ground, rocks = (bmesh.new() for _ in range(7))

    top = -0.42
    size = 2 * half + 1.2
    tier_h = 0.5
    for i in range(tiers):
        target = stone if i % 2 == 0 else stone_dark
        sub = bmesh.new()
        box(sub, 0, 0, top - tier_h, size, size, tier_h)
        bevel_all(sub, 0.05)
        merge_into(target, sub)
        # Stairs on the far side climb each tier, like a temple's front stair.
        for s in range(4):
            sub = bmesh.new()
            depth = 0.32
            box(sub, 0, size / 2 + depth * (s + 0.5), top - tier_h, 2.6 - 0.1 * s, depth,
                tier_h * (1 - (s + 1) / 5))
            bevel_all(sub, 0.02)
            merge_into(stone_dark if i % 2 == 0 else stone, sub)
        top -= tier_h
        size += 2.4
    ground_z = top

    # Braziers on pillars at the platform's corners.
    pillar_xy = half + 0.95
    for sx in (-1, 1):
        for sy in (-1, 1):
            cx, cy = sx * pillar_xy, sy * pillar_xy
            sub = bmesh.new()
            box(sub, cx, cy, -0.42 - 0.5, 0.62, 0.62, 1.05)
            box(sub, cx, cy, 0.13, 0.78, 0.78, 0.12)
            bevel_all(sub, 0.03)
            merge_into(stone_dark, sub)
            sub = bmesh.new()
            lathe_outline(sub, circle(16, 0.34), [(0.45, 0.25), (0.8, 0.3), (1.0, 0.42), (1.05, 0.47), (0.92, 0.47)])
            transform(sub, dx=cx, dy=cy)
            merge_into(bronze, sub)
            for k in range(3):
                sub = bmesh.new()
                a = 2 * math.pi * k / 3 + rng.uniform(0, 1)
                lathe_outline(sub, circle(8, 0.16 - 0.03 * k), [(1.0, 0.44), (0.85, 0.6 + 0.08 * k), (0.02, 0.92 + 0.14 * k)],
                              twist_step=0.4)
                transform(sub, dx=cx + 0.1 * math.cos(a), dy=cy + 0.1 * math.sin(a))
                merge_into(flame, sub)

    # Jungle floor and ferns around the temple.
    gs = p["backdrop"]["ground_size"]
    sub = bmesh.new()
    box(sub, 0, 0, ground_z - 0.2, gs, gs, 0.2)
    merge_into(ground, sub)

    clumps = p["backdrop"]["foliage_clumps"]
    placed = 0
    attempts = 0
    while placed < clumps and attempts < clumps * 20:
        attempts += 1
        x, y = rng.uniform(-gs / 3, gs / 3), rng.uniform(-gs / 3, gs / 3)
        if max(abs(x), abs(y)) < half + 4.2:
            continue
        if abs(x) < 2.2 and y > 0:
            continue  # keep the stair approach clear
        fern(foliage, rng, x, y, ground_z, rng.uniform(0.9, 1.7))
        if rng.random() < 0.5:
            boulder(rocks, rng, x + rng.uniform(-1, 1), y + rng.uniform(-1, 1), ground_z, rng.uniform(0.3, 0.7))
        placed += 1

    return {
        "Stone": (stone, "MI_StoneFrame", 0.0),
        "StoneDark": (stone_dark, "MI_StoneDark", 0.0),
        "Bronze": (bronze, "MI_Bronze", 40.0),
        "Flame": (flame, "MI_Flame", 80.0),
        "Ground": (ground, "MI_Ground", 0.0),
        "Foliage": (foliage, "MI_Foliage", 80.0),
        "Rocks": (rocks, "MI_StoneDark", 0.0),
    }


def fern(bm, rng, x, y, z, scale):
    fronds = 7
    for f in range(fronds):
        a = 2 * math.pi * f / fronds + rng.uniform(-0.3, 0.3)
        length = scale * rng.uniform(0.8, 1.2)
        segs = 6
        left, right = [], []
        for s in range(segs + 1):
            t = s / segs
            width = length * 0.16 * math.sin(math.pi * min(1.0, t * 1.15)) + 0.004
            dist = length * t
            height = z + length * (0.75 * t - 0.85 * t * t) + 0.02
            px, py = x + math.cos(a) * dist, y + math.sin(a) * dist
            nx, ny = -math.sin(a), math.cos(a)
            left.append(bm.verts.new((px + nx * width, py + ny * width, height)))
            right.append(bm.verts.new((px - nx * width, py - ny * width, height)))
        for s in range(segs):
            bm.faces.new((left[s], right[s], right[s + 1], left[s + 1]))


def boulder(bm, rng, x, y, z, r):
    sub = bmesh.new()
    bmesh.ops.create_icosphere(sub, subdivisions=1, radius=r)
    for v in sub.verts:
        k = 1.0 + rng.uniform(-0.18, 0.18)
        v.co.x, v.co.y, v.co.z = v.co.x * k + x, v.co.y * k + y, v.co.z * k * 0.6 + z
    merge_into(bm, sub)


# --------------------------------------------------------------------------
# Output


def tri_count(bm):
    return sum(len(f.verts) - 2 for f in bm.faces)


def _fmt(v):
    text = f"{v:.5f}"
    return "0.00000" if text == "-0.00000" else text


def obj_text(mesh):
    """Wavefront OBJ with per-corner normals, written in a fixed order so output is
    byte-identical across runs. Blender (x, y, z) Z-up becomes Godot (x, z, -y) Y-up."""
    lines = ["# SPDX-License-Identifier: GPL-2.0-or-later", "# Generated by tools/assets/gen_theme_meshes.py", f"o {mesh.name}"]
    for v in mesh.vertices:
        x, y, z = v.co
        lines.append(f"v {_fmt(x)} {_fmt(z)} {_fmt(-y)}")
    normals, faces = {}, []
    corner_normals = mesh.corner_normals
    for poly in mesh.polygons:
        corners = []
        for li in poly.loop_indices:
            nx, ny, nz = corner_normals[li].vector
            key = (_fmt(nx), _fmt(nz), _fmt(-ny))
            corners.append((mesh.loops[li].vertex_index + 1, normals.setdefault(key, len(normals) + 1)))
        faces.append(corners)
    lines += [f"vn {x} {y} {z}" for x, y, z in normals]
    lines.append("s 0")
    lines += ["f " + " ".join(f"{v}//{n}" for v, n in face) for face in faces]
    return "\n".join(lines) + "\n"


def write_obj(bm, path, smooth_angle):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bmesh.ops.triangulate(bm, faces=bm.faces, quad_method="BEAUTY", ngon_method="BEAUTY")
    mesh = bpy.data.meshes.new(path.stem)
    bm.to_mesh(mesh)
    bm.free()
    if smooth_angle > 0:
        mesh.shade_smooth()
        mesh.set_sharp_from_angle(angle=math.radians(smooth_angle))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(obj_text(mesh))
    bpy.data.meshes.remove(mesh)


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    params_path = Path(args[0]) if args else ROOT / "tools/assets/themes/temple.json"
    p = json.loads(params_path.read_text())
    theme = p["theme"]
    out = ROOT / "game/assets/models" / theme
    for o in list(bpy.context.scene.objects):
        bpy.data.objects.remove(o)

    report = {}
    errors = []
    r = p["artifacts"]["piece_width"] * p["cell_size"] / 2
    for kind in p["artifacts"]["kinds"]:
        bm = bmesh.new()
        smooth = PIECES[kind["shape"]](bm, r)
        tris = tri_count(bm)
        report[f"artifact_{kind['index']}"] = tris
        if tris > TRI_BUDGET_PIECE:
            errors.append(f"SM_Artifact_{kind['name']}: {tris} triangles, budget {TRI_BUDGET_PIECE}; lower segment counts")
        write_obj(bm, out / "artifacts" / f"SM_Artifact_{kind['name']}.obj", smooth)

    for group, parts in (("board", board_parts(p, random.Random(p["seed"]))),
                         ("backdrop", backdrop_parts(p, random.Random(p["seed"] + 1)))):
        total = 0
        for part, (bm, _material, smooth) in parts.items():
            total += tri_count(bm)
            write_obj(bm, out / group / f"SM_{group.capitalize()}_{part}.obj", smooth)
        report[group] = total
        if group == "board" and total > TRI_BUDGET_BOARD:
            errors.append(f"board: {total} triangles, budget {TRI_BUDGET_BOARD}; fewer bevels or fret blocks")

    print(json.dumps({"theme": theme, "triangles": report}, indent=2))
    if errors:
        sys.exit("\n".join(errors))


if __name__ == "__main__":
    main()

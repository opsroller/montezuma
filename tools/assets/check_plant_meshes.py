# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Check the generated plant meshes for the faults a frame will not show you.

    python3 tools/assets/check_plant_meshes.py            # game/assets/models/plants
    python3 tools/assets/check_plant_meshes.py --dir <d> --verbose

Exits 1 if anything is wrong. Four checks, each of which has caught something real:

ZERO      Polygons with no area. Every leaf profile is zero wide at its base and its
          tip - that is what makes it a leaf and not a rectangle - so the first and
          last quad row of every blade sits on one point and triangulates into one
          real triangle and one with none. Unwelded, that was 11,115 of 65,061
          triangles across the library: a third of a strangler fig, and zero-length
          normals feeding the smooth-shading pass at exactly the base and tip of
          every leaf.

FLIP      Two faces sharing an edge that traverse it the same way, so one of them is
          wound the other way round and its normal points through the blade. Face
          winding here is authored and never recalculated, because a leaf is an open
          surface and recalculation flips it at random, so nothing else catches this.

NORMAL    A per-corner normal in the OBJ pointing against its own face's geometric
          normal. Hard-edged dark patches on a blade look exactly like this and
          exactly like a self-shadow; the only way to tell them apart is to measure.

STRADDLE  A leaf face whose UVs cross a tile boundary of its 2x2 atlas, so it samples
          two leaf states at once. A part legitimately reads two tiles - its own live
          one and the dead tile its dying leaves and litter use - so the test has to
          be per face, not per mesh.

ORPHAN    A mesh on disk that the catalogue does not reference. How many LODs a
          variant needs falls out of its triangle count, so cutting triangles orphans
          LODs that nothing points at and nothing notices are stale.
"""

import argparse
import json
import math
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MODELS = ROOT / "game/assets/models/plants"
CATALOGUE = ROOT / "game/assets/library.json"
AREA_EPS = 1e-10
GRID = 2
# Parts that read off the species' leaf atlas, and so must respect its tiles. Bark
# parts tile their texture freely and have no business being checked against a grid.
LEAF_PARTS = ("leaf", "canopy", "frond", "litter", "moss", "lichen")


def read_obj(path):
    verts, uvs, normals, faces = [], [], [], []
    for line in path.read_text().splitlines():
        if line.startswith("v "):
            verts.append(tuple(float(x) for x in line.split()[1:4]))
        elif line.startswith("vt "):
            uvs.append(tuple(float(x) for x in line.split()[1:3]))
        elif line.startswith("vn "):
            normals.append(tuple(float(x) for x in line.split()[1:4]))
        elif line.startswith("f "):
            corners = []
            for token in line.split()[1:]:
                bits = token.split("/")
                corners.append((
                    int(bits[0]) - 1,
                    int(bits[1]) - 1 if len(bits) > 1 and bits[1] else None,
                    int(bits[2]) - 1 if len(bits) > 2 and bits[2] else None,
                ))
            faces.append(corners)
    return verts, uvs, normals, faces


def polygon_normal(points):
    """Newell's normal; its length is twice the polygon's area."""
    n = [0.0, 0.0, 0.0]
    for i, a in enumerate(points):
        b = points[(i + 1) % len(points)]
        n[0] += a[1] * b[2] - a[2] * b[1]
        n[1] += a[2] * b[0] - a[0] * b[2]
        n[2] += a[0] * b[1] - a[1] * b[0]
    return n


def is_leaf_part(stem):
    """Which part of the plant this mesh is, from `SM_Plant_<Species><Variant>_<Part>`.

    Matched on the part field, not on the whole name: a substring test calls
    `SM_Plant_CanopyTreeMature_Trunk` a canopy part, because the *species* is the
    canopy tree, and then flags all 864 faces of a bark-mapped trunk for straddling
    atlas tiles it does not use.
    """
    bits = stem.split("_")
    if bits and bits[-1].startswith("LOD"):
        bits = bits[:-1]
    return len(bits) >= 4 and bits[3].lower() in LEAF_PARTS


def check_mesh(path):
    verts, uvs, normals, faces = read_obj(path)
    counts = defaultdict(int)
    if not faces:
        return {"EMPTY": 1}, 0
    leafy = is_leaf_part(path.stem)
    edges = defaultdict(list)
    for index, face in enumerate(faces):
        ids = [v for v, _, _ in face]
        for k in range(len(ids)):
            a, b = ids[k], ids[(k + 1) % len(ids)]
            edges[frozenset((a, b))].append((index, a))
        normal = polygon_normal([verts[v] for v in ids])
        if 0.5 * math.sqrt(sum(c * c for c in normal)) < AREA_EPS:
            counts["ZERO"] += 1
            continue
        for _, _, ni in face:
            if ni is not None and ni < len(normals):
                written = normals[ni]
                if sum(normal[i] * written[i] for i in range(3)) < 0:
                    counts["NORMAL"] += 1
                    break
        if leafy:
            us = [uvs[t][0] for _, t, _ in face if t is not None and t < len(uvs)]
            vs = [uvs[t][1] for _, t, _ in face if t is not None and t < len(uvs)]
            if us:
                tu, tv = int(min(us) * GRID + 1e-6), int(min(vs) * GRID + 1e-6)
                if (max(us) > (tu + 1) / GRID + 1e-6
                        or max(vs) > (tv + 1) / GRID + 1e-6):
                    counts["STRADDLE"] += 1
    for uses in edges.values():
        if len(uses) == 2 and uses[0][1] == uses[1][1]:
            counts["FLIP"] += 1
    return counts, len(faces)


def orphans(directory):
    if not CATALOGUE.exists():
        return []
    plants = json.loads(CATALOGUE.read_text()).get("plants", {})
    wanted = {Path(part["mesh"]).name
              for entry in plants.values()
              for lod in entry.get("parts", {}).values()
              for part in lod.values()}
    if not wanted:
        return []
    return [p.name for p in sorted(directory.glob("SM_Plant_*.obj")) if p.name not in wanted]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--dir", type=Path, default=MODELS)
    ap.add_argument("--verbose", action="store_true", help="list clean meshes too")
    args = ap.parse_args()

    meshes = sorted(args.dir.glob("*.obj"))
    if not meshes:
        sys.exit(f"{args.dir}: no meshes; run `plants.py build` first")

    totals, faces_total, bad = defaultdict(int), 0, 0
    for path in meshes:
        counts, faces = check_mesh(path)
        faces_total += faces
        for key, value in counts.items():
            totals[key] += value
        if counts:
            bad += 1
            detail = "  ".join(f"{k} {v}/{faces}" for k, v in sorted(counts.items()))
            print(f"{path.name:<48} {detail}")
        elif args.verbose:
            print(f"{path.name:<48} ok ({faces} faces)")

    stale = orphans(args.dir)
    for name in stale:
        print(f"{name:<48} ORPHAN: no catalogue entry references it")

    print(f"\n{len(meshes)} meshes, {faces_total} faces; {bad} with faults"
          + (f", {len(stale)} orphaned" if stale else ""))
    for key in ("ZERO", "FLIP", "NORMAL", "STRADDLE", "EMPTY"):
        if totals[key]:
            print(f"  {key}: {totals[key]} "
                  f"({totals[key] / max(1, faces_total) * 100:.1f}% of faces)")
    return 1 if bad or stale else 0


if __name__ == "__main__":
    sys.exit(main())

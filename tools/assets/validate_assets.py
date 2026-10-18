# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Check game/assets against the asset rules; exit 1 on any error.

    python3 tools/assets/validate_assets.py

Rules (docs/reference/board-themes.md): every theme in the manifest names
each required logical name; every path resolves; meshes stay inside the
triangle budgets; textures are power-of-two and at most 1024 px; scenes and
materials only reference files that exist; every file parses as its format;
every theme has a provenance record; source files carry the SPDX header;
names follow the prefixes.
Needs only the standard library and Pillow.
"""

import json
import re
import sys
from pathlib import Path

from PIL import Image, UnidentifiedImageError

ROOT = Path(__file__).resolve().parents[2]
GAME = ROOT / "game"
ASSETS = GAME / "assets"
REQUIRED = [f"artifact_{k}" for k in range(7)] + ["board", "environment", "cell_highlight", "fx_match"]
PIECE_TRIS, BOARD_TRIS, MAX_TEXTURE = 2000, 20000, 1024
PREFIXES = {".obj": "SM_", ".png": "T_", ".tres": "MI_", ".gdshader": "SH_"}
SPDX = "SPDX-License-Identifier: GPL-2.0-or-later"
HEADERS = {".tscn": "[gd_scene", ".tres": "[gd_resource"}


def res_to_path(res):
    return GAME / res.removeprefix("res://")


def obj_tris(path):
    tris = 0
    for line in path.read_text().splitlines():
        if line.startswith("f "):
            tris += len(line.split()) - 3
    return tris


def obj_format_error(path):
    """None if the OBJ has vertices and faces whose indices are in range."""
    verts, faces = 0, 0
    for n, line in enumerate(path.read_text().splitlines(), 1):
        if line.startswith("v "):
            verts += 1
        elif line.startswith("f "):
            faces += 1
            for corner in line.split()[1:]:
                try:
                    index = int(corner.split("/")[0])
                except ValueError:
                    return f"line {n}: face corner '{corner}' is not a vertex index"
                if index == 0 or abs(index) > verts:
                    return f"line {n}: face index {index} out of range (1..{verts})"
    if not verts or not faces:
        return f"{verts} vertices, {faces} faces; a mesh needs both"
    return None


def scene_refs(path):
    return re.findall(r'path="(res://[^"]+)"', path.read_text())


def scene_tris(path, seen=None):
    """Triangles of every OBJ a scene pulls in, following nested scenes."""
    seen = seen or set()
    total = 0
    for ref in scene_refs(path):
        p = res_to_path(ref)
        if p in seen or not p.exists():
            continue
        seen.add(p)
        if p.suffix == ".obj":
            total += obj_tris(p)
        elif p.suffix == ".tscn":
            total += scene_tris(p, seen)
    return total


def main():
    errors, warnings = [], []
    manifest_path = ASSETS / "manifest.json"
    if not manifest_path.exists():
        sys.exit(f"{manifest_path}: missing; run tools/assets/gen_theme_scenes.py")
    manifest = json.loads(manifest_path.read_text())
    themes = manifest.get("themes", {})
    if manifest.get("default_theme") not in themes:
        errors.append(f"manifest.json: default_theme '{manifest.get('default_theme')}' is not one of {sorted(themes)}")
    provenance = (ASSETS / "PROVENANCE.md").read_text() if (ASSETS / "PROVENANCE.md").exists() else ""

    for theme, entry in themes.items():
        for name in REQUIRED:
            if name not in entry:
                warnings.append(f"manifest.json: theme '{theme}' has no '{name}'; the view will use a placeholder")
        for name, res in entry.items():
            path = res_to_path(res)
            if not path.exists():
                errors.append(f"manifest.json: {theme}.{name} -> {res} does not exist; regenerate or fix the path")
                continue
            tris = scene_tris(path)
            if name.startswith("artifact_") and tris > PIECE_TRIS:
                errors.append(f"{res}: {tris} triangles, piece budget {PIECE_TRIS}")
            if name == "board" and tris > BOARD_TRIS:
                errors.append(f"{res}: {tris} triangles, board budget {BOARD_TRIS}")
        if f"theme `{theme}`" not in provenance:
            errors.append(f"PROVENANCE.md: no record for theme `{theme}`; add one before merging")

    for path in sorted(ASSETS.rglob("*")):
        if not path.is_file() or path.suffix == ".import":
            continue
        rel = path.relative_to(ROOT)
        prefix = PREFIXES.get(path.suffix)
        if prefix and not path.name.startswith(prefix):
            errors.append(f"{rel}: name must start with {prefix} (see naming table in docs/reference/board-themes.md)")
        if path.suffix == ".tscn" and not re.match(r"(PF|LD)_", path.name):
            errors.append(f"{rel}: scene names start with PF_ (asset scene) or LD_ (look-dev)")
        if path.suffix in HEADERS and not path.read_text().lstrip().startswith(HEADERS[path.suffix]):
            errors.append(f"{rel}: not a Godot text {path.suffix} (must start with {HEADERS[path.suffix]})")
        if path.suffix == ".obj" and (problem := obj_format_error(path)):
            errors.append(f"{rel}: invalid OBJ: {problem}")
        if path.suffix in (".tscn", ".tres"):
            for ref in scene_refs(path):
                if not res_to_path(ref).exists():
                    errors.append(f"{rel}: references missing {ref}")
        if path.suffix in (".gdshader", ".obj", ".tscn", ".tres") and SPDX not in path.read_text()[:400]:
            errors.append(f"{rel}: missing the SPDX header line")
        if path.suffix == ".png":
            try:
                with Image.open(path) as image:
                    if image.format != "PNG":
                        raise UnidentifiedImageError(f"{image.format} data")
                    (w, h), mode = image.size, image.mode
                    image.load()
            except (UnidentifiedImageError, OSError, SyntaxError) as problem:
                errors.append(f"{rel}: not a readable PNG ({problem}); regenerate it")
                continue
            if w > MAX_TEXTURE or h > MAX_TEXTURE or w & (w - 1) or h & (h - 1):
                errors.append(f"{rel}: {w}x{h}; textures must be power-of-two and at most {MAX_TEXTURE}px")
            if path.stem.endswith("_N") and mode != "RGB":
                errors.append(f"{rel}: normal map must be RGB")

    tokens_path = ROOT / "design/tokens/temple-board.json"
    params_path = ROOT / "tools/assets/themes/temple.json"
    if tokens_path.exists() and params_path.exists():
        tokens = json.loads(tokens_path.read_text())["kind"]
        params = json.loads(params_path.read_text())
        for kind in params["artifacts"]["kinds"]:
            want = params["materials"][kind["material"]]["albedo"].lower()
            have = tokens.get(str(kind["index"]), {}).get("value", "").lower()
            if want != have:
                errors.append(f"design/tokens/temple-board.json: kind {kind['index']} is {have or 'missing'}, "
                              f"theme parameters say {want}; update the token")

    for w in warnings:
        print(f"warning: {w}")
    for e in errors:
        print(f"error: {e}")
    print(f"{len(themes)} theme(s), {len(errors)} error(s), {len(warnings)} warning(s)")
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()

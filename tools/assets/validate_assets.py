# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Check game/assets against the asset rules; exit 1 on any error.

    python3 tools/assets/validate_assets.py

Rules (docs/reference/board-themes.md): every theme in the manifest names
each required logical name; every path resolves; meshes stay inside the
triangle budgets; textures are power-of-two and at most 1024 px; scenes and
materials only reference files that exist; every file parses as its format;
every theme has a provenance record; source files carry the SPDX header;
names follow the prefixes. Texture sets: each map has the channels and
colour space of its suffix, is wired to the matching material slot, and
holds physically plausible values; no material or shader filters
anisotropically with repeat (it does not compile on 16-slot Metal GPUs);
every desktop variant under hi/ has a
base file; mobile and web export presets leave the hi/ variants out.
Lighting: every static board and backdrop mesh imports with a lightmap UV2,
and every theme has the bake scene tools/assets/bake_lightmaps.sh bakes.
Colour grade: a theme's grade LUT matches its "grade" parameters, imports as a
3D texture, and keeps the palette within the grade's check limits.
Needs Pillow, and numpy for the colour grade (the assets CI job has both).
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
PIECE_TRIS, BOARD_TRIS, MAX_TEXTURE, MAX_TEXTURE_HI = 2000, 20000, 1024, 2048
PREFIXES = {".obj": "SM_", ".png": "T_", ".tres": "MI_", ".gdshader": "SH_", ".gdshaderinc": "SH_"}
# Texture map suffix -> (allowed PIL modes, colour space, material slots that may use it).
MAPS = {
    "BC": (("RGB", "RGBA"), "sRGB", ("albedo_texture", "shader_parameter/texture_albedo")),
    "N": (("RGB",), "linear", ("normal_texture", "shader_parameter/texture_normal")),
    "ORM": (("RGB",), "linear", ("orm_texture", "shader_parameter/texture_orm")),
    "H": (("L",), "linear", ("shader_parameter/texture_height",)),
    "R": (("L",), "linear", ("roughness_texture",)),
    # Fire flipbooks (tools/assets/fx/gen_fire.py): packed data, not colour.
    "Flame": (("RGBA",), "linear", ("shader_parameter/flame_sheet",)),
    "Motion": (("RGBA",), "linear", ("shader_parameter/motion_sheet",)),
}
# Shader samplers by colour space: colour data needs source_color, data maps must not have it.
SRGB_SAMPLERS = {"texture_albedo", "texture_emission"}
DATA_SAMPLERS = {"texture_normal", "texture_orm", "texture_height", "texture_roughness", "flame_sheet", "motion_sheet"}
TIERED = "hi"  # desktop variant folder; mobile and web exports leave it out
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


def map_suffix(path):
    return path.stem.rsplit("_", 1)[-1] if "_" in path.stem else ""


def percentile(hist, q):
    """Value below which q of the pixels of a 256-bin histogram fall."""
    total, seen = sum(hist), 0
    for value, count in enumerate(hist):
        seen += count
        if seen >= q * total:
            return value
    return 255


def texture_errors(path, rel):
    """Format, size, colour space and value checks for one texture map."""
    errors = []
    try:
        with Image.open(path) as image:
            if image.format != "PNG":
                raise UnidentifiedImageError(f"{image.format} data")
            image.load()
            image = image.copy()
    except (UnidentifiedImageError, OSError, SyntaxError) as problem:
        return [f"{rel}: not a readable PNG ({problem}); regenerate it"]
    (w, h), mode = image.size, image.mode
    limit = MAX_TEXTURE_HI if path.parent.name == TIERED else MAX_TEXTURE
    if w > limit or h > limit or w & (w - 1) or h & (h - 1):
        errors.append(f"{rel}: {w}x{h}; textures must be power-of-two and at most {limit}px"
                      f"{' (hi/ desktop variant)' if limit == MAX_TEXTURE_HI else ''}")
    suffix = map_suffix(path)
    if suffix not in MAPS:
        return errors + [f"{rel}: unknown map suffix '_{suffix}'; use one of "
                         f"{', '.join('_' + s for s in MAPS)} (docs/reference/board-themes.md)"]
    modes, space, _ = MAPS[suffix]
    if mode not in modes:
        errors.append(f"{rel}: {mode} image; a _{suffix} map must be {' or '.join(modes)}")
        return errors
    # Godot reads 8-bit PNGs as stored values; a gamma or ICC chunk means the
    # file was saved by a tool that converted it, and would be misread.
    if "gamma" in image.info or "icc_profile" in image.info:
        errors.append(f"{rel}: carries a gamma or ICC chunk; save {space} values without colour management")
    if suffix == "BC":
        lum = image.convert("L")
        orm_path = path.with_name(path.name.replace("_BC.png", "_ORM.png"))
        metal = None
        if orm_path.exists():
            with Image.open(orm_path) as orm:
                if orm.mode == "RGB" and orm.size == image.size:
                    metal = orm.split()[2].point(lambda v: 255 if v >= 128 else 0)
        dielectric = metal.point(lambda v: 255 - v) if metal else None
        hist = lum.histogram(mask=dielectric)
        if sum(hist):
            lo, hi = percentile(hist, 0.005), percentile(hist, 0.995)
            if lo < 30 or hi > 240:
                errors.append(f"{rel}: dielectric base colour spans {lo}-{hi} sRGB; keep it within 30-240 "
                              "(no baked light or shadow)")
        if metal and sum(hist := lum.histogram(mask=metal)):
            if percentile(hist, 0.01) < 180:
                errors.append(f"{rel}: metal base colour drops to {percentile(hist, 0.01)} sRGB; raw metal "
                              "reflects 180-255")
    elif suffix == "N":
        r, g, b = image.split()
        if b.getextrema()[0] < 128:
            errors.append(f"{rel}: blue drops below 128 (normals facing into the surface); "
                          "is it a normal map in OpenGL convention?")
        mean_r, mean_g = (sum(i * c for i, c in enumerate(ch.histogram())) / (w * h) for ch in (r, g))
        if abs(mean_r - 127.5) > 12 or abs(mean_g - 127.5) > 12:
            errors.append(f"{rel}: average normal leans ({mean_r:.0f}, {mean_g:.0f}); a tiling map averages "
                          "flat (128, 128)")
        sample = image.resize((64, 64), Image.NEAREST).tobytes()
        for x, y, z in zip(sample[0::3], sample[1::3], sample[2::3]):
            length = ((x / 127.5 - 1) ** 2 + (y / 127.5 - 1) ** 2 + (z / 127.5 - 1) ** 2) ** 0.5
            if abs(length - 1) > 0.06:
                errors.append(f"{rel}: normals are not unit length ({length:.2f}); regenerate from height")
                break
    elif suffix == "ORM":
        occ, rough, metal = image.split()
        hist = metal.histogram()
        mixed = sum(hist[26:230]) / (w * h)
        if mixed > 0.05:
            errors.append(f"{rel}: {mixed:.0%} of metallic values are between 0.1 and 0.9; metal is 0 or 1 "
                          "apart from thin transitions")
        if rough.getextrema()[0] < 5:
            errors.append(f"{rel}: roughness reaches {rough.getextrema()[0]}; below 0.02 highlights alias, "
                          "keep it at 5 or more")
    elif suffix == "H":
        lo, hi = image.getextrema()
        if hi - lo < 50:
            errors.append(f"{rel}: height spans only {lo}-{hi}; a flat height map belongs in a parameter")
    return errors


def slot_errors(path, rel):
    """Each texture a material uses sits in the slot of its map kind (colour space follows the slot)."""
    text = path.read_text()
    ids = dict(re.findall(r'\[ext_resource type="Texture2D" path="(res://[^"]+)" id="([^"]+)"\]', text))
    by_id = {v: k for k, v in ids.items()}
    errors = []
    for slot, ref in re.findall(r'^(\S+) = ExtResource\("([^"]+)"\)', text, re.M):
        res = by_id.get(ref)
        if not res or not res.endswith(".png"):
            continue
        suffix = map_suffix(Path(res))
        if suffix in MAPS and slot not in MAPS[suffix][2]:
            errors.append(f"{rel}: {Path(res).name} ({MAPS[suffix][1]} _{suffix} map) is wired to {slot}; "
                          f"use {' or '.join(MAPS[suffix][2])}")
    return errors


def sampler_errors(path, rel):
    """Colour samplers are marked source_color (sRGB); data samplers are not."""
    errors = []
    for name, hints in re.findall(r"uniform\s+sampler2D\s+(\w+)\s*(?::\s*([^;=]+))?", path.read_text()):
        srgb = "source_color" in (hints or "")
        if name in SRGB_SAMPLERS and not srgb:
            errors.append(f"{rel}: sampler {name} holds colour; add the source_color hint")
        if name in DATA_SAMPLERS and srgb:
            errors.append(f"{rel}: sampler {name} holds linear data; remove the source_color hint")
    return errors


def anisotropic_repeat_errors(path, rel):
    """No anisotropic filtering with repeat: Godot's Metal backend binds that
    sampler at slot 17, past the 16 slots of GPUs without argument buffers."""
    text = path.read_text()
    errors = []
    if path.suffix == ".gdshader":
        for name, hints in re.findall(r"uniform\s+sampler2D\s+(\w+)\s*(?::\s*([^;=]+))?", text):
            if "anisotropic" in (hints or "") and "repeat_disable" not in hints:
                errors.append(f"{rel}: sampler {name} filters anisotropically with repeat, which does not compile "
                              "on 16-slot Metal GPUs; use repeat_disable and wrap in the shader (tri_tex)")
    else:
        # One check per resource block, so a texture_repeat in one material does not excuse another.
        for block in re.split(r"^\[", text, flags=re.M):
            if re.search(r"^texture_filter = [45]$", block, re.M) and \
                    not re.search(r"^texture_repeat = false$", block, re.M):
                errors.append(f"{rel}: anisotropic texture_filter with repeat does not compile on 16-slot Metal "
                              "GPUs; use the default trilinear filter")
    return errors


def export_errors():
    """Mobile and web presets exclude the desktop texture and material variants; desktop presets keep them."""
    path = GAME / "export_presets.cfg"
    if not path.exists():
        return []
    errors = []
    for block in re.split(r"^\[preset\.\d+\]$", path.read_text(), flags=re.M)[1:]:
        name = re.search(r'^name="([^"]*)"', block, re.M)
        platform = re.search(r'^platform="([^"]*)"', block, re.M)
        exclude = re.search(r'^exclude_filter="([^"]*)"', block, re.M)
        name = name.group(1) if name else "?"
        platform = platform.group(1) if platform else ""
        filters = [f.strip() for f in (exclude.group(1) if exclude else "").split(",")]
        strips = f"*/{TIERED}/*" in filters
        if platform in ("Android", "iOS", "Web") and not strips:
            errors.append(f"game/export_presets.cfg: preset '{name}' ({platform}) must list */{TIERED}/* in "
                          "exclude_filter so desktop-only variants stay out of the build")
        if platform in ("Windows Desktop", "Linux", "macOS") and strips:
            errors.append(f"game/export_presets.cfg: desktop preset '{name}' excludes */{TIERED}/*; "
                          "desktop builds ship the hi/ variants")
    return errors


def lighting_errors(theme):
    """Static theme meshes need lightmap UV2 from their import settings (OBJ holds one UV set)."""
    errors = []
    for part in ("board", "backdrop"):
        for obj in sorted((ASSETS / "models" / theme / part).glob("*.obj")):
            rel = obj.relative_to(ROOT)
            imp = obj.with_name(obj.name + ".import")
            if not imp.exists() or not re.search(r"^generate_lightmap_uv2=true$", imp.read_text(), re.M):
                errors.append(f"{rel}: static mesh without lightmap UV2; commit {imp.name} with "
                              "generate_lightmap_uv2=true (see docs/how-to/make-a-board-theme.md)")
    bake = ASSETS / "lookdev" / f"LD_{theme.capitalize()}_Bake.tscn"
    if not bake.exists():
        errors.append(f"{bake.relative_to(ROOT)}: missing lightmap bake scene; run tools/assets/gen_theme_scenes.py")
    return errors


def grade_errors(params_path):
    """A theme's colour grade: its LUT is current and imports as a Texture3D, and it keeps the palette."""
    params = json.loads(params_path.read_text())
    if "grade" not in params:
        return []
    import numpy as np
    import grade_lut
    grade, errors = params["grade"], []
    png = grade_lut.lut_path(ASSETS / "textures" / params["theme"])
    rel = png.relative_to(ROOT)
    if not png.exists():
        return [f"{rel}: missing; run tools/assets/gen_theme_textures.py"]
    with Image.open(png) as image:
        if not np.array_equal(np.asarray(image.convert("RGB")), grade_lut.table(grade)):
            errors.append(f"{rel}: does not match the theme's grade; run tools/assets/gen_theme_textures.py")
    imp = png.with_name(png.name + ".import")
    if not imp.exists() or (bad := grade_lut.import_mismatches(grade, imp.read_text())):
        errors.append(f"{rel}: must import as a 3D texture ({'missing .import' if not imp.exists() else bad}); "
                      "run tools/assets/gen_theme_textures.py")
    errors += [f"{params_path.relative_to(ROOT)}: {e}" for e in grade_lut.check(grade, *grade_lut.theme_palette(params))[1]]
    return errors


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
        errors += lighting_errors(theme)
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
                    hint = "; textures are build outputs, run tools/assets/gen_theme_textures.py and " \
                        "tools/assets/fx/gen_fire.py first" \
                        if "/textures/" in ref else ""
                    errors.append(f"{rel}: references missing {ref}{hint}")
        if path.suffix in (".gdshader", ".obj", ".tscn", ".tres") and SPDX not in path.read_text()[:400]:
            errors.append(f"{rel}: missing the SPDX header line")
        if path.suffix == ".png" and path.parent.name != "grade":  # grade LUTs: grade_errors
            errors += texture_errors(path, rel)
        if path.suffix == ".tres":
            errors += slot_errors(path, rel)
        if path.suffix == ".gdshader":
            errors += sampler_errors(path, rel)
        if path.suffix in (".gdshader", ".tres", ".tscn"):
            errors += anisotropic_repeat_errors(path, rel)
        if path.parent.name == TIERED and path.suffix in (".png", ".tres"):
            base = path.parent.parent / path.name
            if not base.exists():
                errors.append(f"{rel}: desktop variant has no base file {base.relative_to(ROOT)}; "
                              "every tier falls back to the base, so add it")
            elif path.suffix == ".png":
                with Image.open(path) as hi, Image.open(base) as lo:
                    if hi.size[0] < lo.size[0] or hi.mode != lo.mode:
                        errors.append(f"{rel}: {hi.size[0]}px {hi.mode}, base is {lo.size[0]}px {lo.mode}; "
                                      "a desktop variant is the same map at the same or a larger size")

    errors += export_errors()
    for params_path in sorted((ROOT / "tools/assets/themes").glob("*.json")):
        errors += grade_errors(params_path)

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

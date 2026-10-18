# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Godot import settings for generated texture maps, shared by the generator
(which writes them) and validate_assets.py (which checks them).

Git ignores the generated PNGs and their .import files, so without these every
checkout would import with Godot's defaults: no mipmaps (the triplanar shaders
sample with a mipmap filter, so minified tiles alias) and detect_3d, which
silently switches a texture to VRAM compression the first time the editor sees
it in 3D. Every map is VRAM compressed on purpose (S3TC/BPTC on desktop and web,
ETC2/ASTC on mobile) with mipmaps; normal maps use the two-channel RGTC/BC5
format, whose missing Z the shaders rebuild (tri_unpack).
Reference: docs/reference/board-themes.md, "Import settings".
"""

import re

BASE = {
    "compress/mode": "2",
    "compress/high_quality": "false",
    "compress/normal_map": "2",
    "mipmaps/generate": "true",
    "detect_3d/compress_to": "0",
}
# Map suffix -> import parameters (see MAPS in validate_assets.py).
PARAMS = {
    "BC": BASE,
    "N": {**BASE, "compress/normal_map": "1"},
    "ORM": BASE,
    "H": BASE,
    "R": BASE,
}

TEMPLATE = """[remap]

importer="texture"
type="CompressedTexture2D"

[params]

{params}
"""


def render(suffix):
    return TEMPLATE.format(params="\n".join(f"{k}={v}" for k, v in PARAMS[suffix].items()))


def parse(text):
    """The [params] of a .import file as a dict of raw values."""
    block = re.search(r"^\[params\]$(.*?)(?=^\[|\Z)", text, re.M | re.S)
    return dict(re.findall(r"^([\w/]+)=(.*)$", block.group(1), re.M)) if block else {}


def mismatches(suffix, text):
    """Parameters of the preset for suffix that the .import text lacks or sets otherwise."""
    have = parse(text)
    return {k: (have.get(k), v) for k, v in PARAMS[suffix].items() if have.get(k) != v}


def write(png, suffix):
    """Write png's .import from the preset, keeping Godot's rewritten file once it agrees."""
    path = png.with_name(png.name + ".import")
    if path.exists() and not mismatches(suffix, path.read_text()):
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(render(suffix))
    return True

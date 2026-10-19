# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""A board theme's colour grade as a 3D lookup table, and its palette check.

gen_theme_textures.py calls write() for every theme with a "grade" section;
validate_assets.py calls check() on the palette. The grade is an ASC CDL
(slope, offset, power per channel, then saturation) applied in display space:
Godot applies Environment.adjustment_color_correction after tonemapping and
the sRGB transfer, so the table maps sRGB-encoded colours to sRGB-encoded
colours and an identity table changes nothing.

Writes game/assets/textures/<theme>/grade/T_Grade_LUT.png: size x size
slices of size x size laid side by side (red along x within a slice, green
along y, blue by slice), imported as a Texture3D with that many horizontal
slices. Godot samples the table with textureLod(lut, colour) and no
half-texel remap, so texel i along an axis is read at (i + 0.5) / size: the
table holds the grade at texel centres, and lookup() models the same
sampling. Reference: docs/reference/board-themes.md, "Colour grade".
"""

import re
from pathlib import Path

import numpy as np
from PIL import Image

# Rec. 709 luma weights; the CDL saturation operator uses them on the graded
# (display-space) values, as the ASC CDL specification does.
LUMA = np.array([0.2126, 0.7152, 0.0722])

IMPORT_PARAMS = {
    "compress/mode": "0",
    "mipmaps/generate": "false",
    "slices/horizontal": None,  # the table size
    "slices/vertical": "1",
}

IMPORT_TEMPLATE = """[remap]

importer="3d_texture"
type="CompressedTexture3D"

[params]

{params}
"""


def cdl(rgb, grade):
    """The theme's grade applied to sRGB-encoded colours in [0, 1], any shape (..., 3)."""
    rgb = np.asarray(rgb, dtype=np.float64)
    slope = np.array(grade.get("slope", [1.0, 1.0, 1.0]))
    offset = np.array(grade.get("offset", [0.0, 0.0, 0.0]))
    power = np.array(grade.get("power", [1.0, 1.0, 1.0]))
    out = np.clip(rgb * slope + offset, 0.0, 1.0) ** power
    luma = (out * LUMA).sum(axis=-1, keepdims=True)
    return np.clip(luma + grade.get("saturation", 1.0) * (out - luma), 0.0, 1.0)


def table(grade):
    """The grade sampled at the size^3 texel centres, as a (size, size * size, 3) uint8 strip."""
    n = grade["lut_size"]
    axis = (np.arange(n) + 0.5) / n
    b, g, r = np.meshgrid(axis, axis, axis, indexing="ij")  # [slice, row, column]
    graded = cdl(np.stack([r, g, b], axis=-1), grade)
    strip = graded.transpose(1, 0, 2, 3).reshape(n, n * n, 3)  # slices side by side
    return np.round(strip * 255).astype(np.uint8)


def lookup(strip, rgb):
    """Trilinear lookup of sRGB-encoded colours (..., 3) in [0, 1] through a strip, as the GPU samples it:
    texel centres at (i + 0.5) / size, clamped to the edge texels."""
    n = strip.shape[0]
    lut = strip.reshape(n, n, n, 3).transpose(1, 0, 2, 3).astype(np.float64) / 255  # [b, g, r]
    pos = np.clip(np.asarray(rgb, dtype=np.float64) * n - 0.5, 0.0, n - 1)
    i0 = np.minimum(np.floor(pos).astype(int), n - 2)
    f = pos - i0
    out = 0.0
    for db in (0, 1):
        for dg in (0, 1):
            for dr in (0, 1):
                w = ((f[..., 2] if db else 1 - f[..., 2]) * (f[..., 1] if dg else 1 - f[..., 1])
                     * (f[..., 0] if dr else 1 - f[..., 0]))
                out = out + w[..., None] * lut[i0[..., 2] + db, i0[..., 1] + dg, i0[..., 0] + dr]
    return out


def lut_path(theme_dir):
    return theme_dir / "grade" / "T_Grade_LUT.png"


def import_params(grade):
    return {k: (str(grade["lut_size"]) if v is None else v) for k, v in IMPORT_PARAMS.items()}


def import_mismatches(grade, text):
    """Import parameters the .import text lacks or sets otherwise than the grade needs."""
    block = re.search(r"^\[params\]$(.*?)(?=^\[|\Z)", text, re.M | re.S)
    have = dict(re.findall(r"^([\w/]+)=(.*)$", block.group(1), re.M)) if block else {}
    importer = re.search(r'^importer="([^"]*)"', text, re.M)
    out = {k: (have.get(k), v) for k, v in import_params(grade).items() if have.get(k) != v}
    if not importer or importer.group(1) != "3d_texture":
        out["importer"] = (importer.group(1) if importer else None, "3d_texture")
    return out


def write(theme_dir, grade):
    """Write the theme's LUT and its .import when missing or different; returns whether anything was written."""
    png = lut_path(theme_dir)
    png.parent.mkdir(parents=True, exist_ok=True)
    data = table(grade)
    wrote = False
    if not png.exists() or not np.array_equal(np.asarray(Image.open(png).convert("RGB")), data):
        Image.fromarray(data, "RGB").save(png, optimize=False, compress_level=9)
        wrote = True
    imp = png.with_name(png.name + ".import")
    # Godot rewrites the file after importing (uid, paths); keep its version while it agrees.
    if not imp.exists() or import_mismatches(grade, imp.read_text()):
        params = "\n".join(f"{k}={v}" for k, v in import_params(grade).items())
        imp.write_text(IMPORT_TEMPLATE.format(params=params))
        wrote = True
    return wrote


# ---------------------------------------------------------------- palette check

def srgb_to_linear(c):
    c = np.asarray(c, dtype=np.float64)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def oklab(srgb):
    """OKLab (Ottosson 2020) of sRGB-encoded colours (..., 3)."""
    lin = srgb_to_linear(srgb)
    m1 = np.array([[0.4122214708, 0.5363325363, 0.0514459929],
                   [0.2119034982, 0.6806995451, 0.1073969566],
                   [0.0883024619, 0.2817188376, 0.6299787005]])
    m2 = np.array([[0.2104542553, 0.7936177850, -0.0040720468],
                   [1.9779984951, -2.4285922050, 0.4505937099],
                   [0.0259040371, 0.7827717662, -0.8086757660]])
    return np.cbrt(lin @ m1.T) @ m2.T


# Colour-vision deficiency at full severity in linear RGB (Machado, Oliveira and
# Fernandes 2009), the same matrices tools/bench/check_readability.py uses.
CVD = {
    "protanopia": [[0.152286, 1.052583, -0.204868], [0.114503, 0.786281, 0.099216], [-0.003882, -0.048116, 1.051998]],
    "deuteranopia": [[0.367322, 0.860646, -0.227968], [0.280085, 0.672501, 0.047413], [-0.011820, 0.042940, 0.968881]],
    "tritanopia": [[1.255528, -0.076749, -0.178779], [-0.078411, 0.930809, 0.147602], [0.004733, 0.691367, 0.303900]],
}


def linear_to_srgb(c):
    c = np.clip(c, 0.0, 1.0)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)


def views(srgb):
    """The colours as seen normally, in greyscale and under each full colour-vision deficiency."""
    lin = srgb_to_linear(srgb)
    out = {"normal": np.asarray(srgb, dtype=np.float64)}
    out["greyscale"] = np.repeat(linear_to_srgb((lin * LUMA).sum(axis=-1, keepdims=True)), 3, axis=-1)
    for name, m in CVD.items():
        out[name] = linear_to_srgb(lin @ np.array(m).T)
    return out


def hex_rgb(h):
    return [int(h[i:i + 2], 16) / 255 for i in (1, 3, 5)]


def check(grade, pieces, surfaces):
    """Palette check of a grade: returns (report lines, errors).

    pieces and surfaces map names to "#rrggbb". Each colour may move by at most
    grade["check"]["max_shift_pieces"] / ["max_shift_surfaces"] in OKLab, and the
    closest pair of piece colours, normally, in greyscale and under each
    deficiency, may come closer by at most grade["check"]["max_separation_drop"]
    in OKLab. The drop is absolute because the dark gems share a value on purpose
    (silhouettes tell them apart), so their greyscale distance is near zero and a
    relative limit would judge rounding noise.
    """
    limits = grade["check"]
    strip = table(grade)
    lines, errors = [], []
    for group, colours, limit in (("piece", pieces, limits["max_shift_pieces"]),
                                  ("surface", surfaces, limits["max_shift_surfaces"])):
        for name, h in colours.items():
            before = np.array(hex_rgb(h))
            shift = float(np.linalg.norm(oklab(lookup(strip, before)) - oklab(before)))
            lines.append(f"{group} {name}: OKLab shift {shift:.3f} (max {limit})")
            if shift > limit:
                errors.append(f"grade moves {group} colour {name} by {shift:.3f} in OKLab (max {limit})")
    names = list(pieces)
    before = np.array([hex_rgb(pieces[n]) for n in names])
    after = lookup(strip, before)
    for view, (b, a) in {k: (views(before)[k], views(after)[k]) for k in views(before)}.items():
        lb, la = oklab(b), oklab(a)
        pairs = [(i, j) for i in range(len(names)) for j in range(i + 1, len(names))]
        db = min(pairs, key=lambda p: np.linalg.norm(lb[p[0]] - lb[p[1]]))
        d_before = float(np.linalg.norm(lb[db[0]] - lb[db[1]]))
        d_after = min(float(np.linalg.norm(la[i] - la[j])) for i, j in pairs)
        drop = d_before - d_after
        lines.append(f"{view}: closest piece pair {d_before:.3f} -> {d_after:.3f} in OKLab")
        if drop > limits["max_separation_drop"]:
            errors.append(f"grade brings the closest piece pair in {view} {drop:.3f} closer in OKLab "
                          f"(max {limits['max_separation_drop']})")
    return lines, errors


def theme_palette(p):
    """Piece and surface colours of a theme's parameters, as check() takes them."""
    m = p["materials"]
    pieces = {k["name"]: m[k["material"]]["albedo"] for k in p["artifacts"]["kinds"]}
    surfaces = {key: m[mat]["albedo"] for key, mat in p.get("surface_tokens", {}).items()}
    return pieces, surfaces | dict(p.get("extra_tokens", {}))


def main(argv):
    import json
    for path in [Path(a) for a in argv] or sorted((Path(__file__).parent / "themes").glob("*.json")):
        p = json.loads(path.read_text())
        if "grade" not in p:
            continue
        lines, errors = check(p["grade"], *theme_palette(p))
        print(f"{p['theme']} grade:")
        print("\n".join("  " + s for s in lines))
        for e in errors:
            print("  ERROR " + e)
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main(sys.argv[1:]))

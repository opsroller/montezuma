# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Montezuma's visual identity: writes every final SVG and both token files.

    python3 design/identity/build.py

Outputs (all deterministic, standard library only):

    design/logo/*.svg              mark, small mark, one-colour, reversed,
                                   wordmark, lockups
    design/patterns/step-fret-band.svg
    design/icons/app-icon.svg      1024 master; app-icon-16.svg pixel variant
    design/tokens/montezuma.tokens.json   W3C DTCG tokens (game drop-in)
    design/tokens/tokens.css              CSS custom properties (site drop-in)

PNG previews are rendered separately by design/identity/render.py.
The rules these files follow are in design/README.md; the reasons are in
docs/explanation/about-the-visual-identity.md.
"""
import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
DESIGN = os.path.dirname(HERE)

SVG_HEAD = "<!-- SPDX-License-Identifier: GPL-2.0-or-later -->\n" \
           "<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->\n"


def fmt(v):
    """Short, stable number formatting for SVG coordinates."""
    v = round(v, 3)
    if v == int(v):
        return str(int(v))
    return f"{v:.3f}".rstrip("0").rstrip(".")


# =====================================================================
# Colour
# =====================================================================
# Brand anchors: one per stone pigment family in the Mexica art reference
# (essential 5: CO-01, CO-02, CO-03, CO-06) plus cochineal, a painted-book
# colour kept to UI and cloth (CO-07). Values are chosen sRGB, not measured
# pigment colourimetry (the reference lists exact Tlaloc blue as a gap).
BRAND = {
    "soot": "#1e1916",      # soot / charcoal black: outline, text, dark surface
    "lime": "#f4ecdc",      # lime / calcite white: light surface, stairs, roofs
    "hematite": "#b9412f",  # hematite red: the platform
    "wine": "#6a2224",      # dark wine red (hematite family, darker)
    "ochre": "#d8901e",     # goethite ochre: south shrine, Huitzilopochtli
    "maya": "#2a9fb4",      # Maya-blue turquoise: north shrine, Tlaloc
    "cochineal": "#a8174a",  # codex crimson: UI danger only, never on stone
}

# Seven piece colours stay where the technical artist generates them
# (design/tokens/temple-board.json); the UI ochre is kept clearly apart
# from the yellow (#ffcc4d) and orange (#e2661c) pieces (check_contrast.py).


def _hex_to_rgb(h):
    h = h.lstrip("#")
    return [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]


def _rgb_to_hex(rgb):
    return "#" + "".join(f"{max(0, min(255, round(c * 255))):02x}" for c in rgb)


def _to_lin(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def _from_lin(c):
    return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055


def _oklab(h):
    r, g, b = (_to_lin(c) for c in _hex_to_rgb(h))
    l_ = (0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b) ** (1 / 3)
    m_ = (0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b) ** (1 / 3)
    s_ = (0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b) ** (1 / 3)
    return (0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
            1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
            0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_)


def _oklch_to_rgb(L, C, hdeg):
    a, b = C * math.cos(math.radians(hdeg)), C * math.sin(math.radians(hdeg))
    l_ = (L + 0.3963377774 * a + 0.2158037573 * b) ** 3
    m_ = (L - 0.1055613458 * a - 0.0638541728 * b) ** 3
    s_ = (L - 0.0894841775 * a - 1.2914855480 * b) ** 3
    return [4.0767416621 * l_ - 3.3077115913 * m_ + 0.2309699292 * s_,
            -1.2684380046 * l_ + 2.6097574011 * m_ - 0.3413193965 * s_,
            -0.0041960863 * l_ - 0.7034186147 * m_ + 1.7076147010 * s_]


def _oklch_hex(L, C, hdeg):
    """OKLCH to sRGB hex, reducing chroma until the colour is in gamut."""
    for _ in range(60):
        rgb = _oklch_to_rgb(L, C, hdeg)
        if all(-1e-6 <= c <= 1 + 1e-6 for c in rgb):
            break
        C *= 0.95
    return _rgb_to_hex([_from_lin(max(0.0, min(1.0, c))) for c in rgb])


STEPS = ["50", "100", "200", "300", "400", "500", "600", "700", "800", "900", "950"]
# OKLab lightness per step: even perceptual steps from paper to soot.
STEP_L = [0.975, 0.945, 0.885, 0.815, 0.735, 0.655, 0.57, 0.485, 0.40, 0.315, 0.235]


def ramp(anchor, pin, chroma_scale=1.0, chroma_floor=0.0, hue=None):
    """An 11-step ramp through `anchor`, which is pinned at step `pin`.

    Chroma follows the anchor's, tapering towards white and black so the
    light steps stay paper-like and the dark ones stay ink-like."""
    L0, a0, b0 = _oklab(anchor)
    C0 = math.hypot(a0, b0) * chroma_scale
    h = hue if hue is not None else math.degrees(math.atan2(b0, a0)) % 360
    out = {}
    for step, L in zip(STEPS, STEP_L):
        if step == pin:
            out[step] = anchor
            continue
        t = (1 - L) / (1 - L0) if L > L0 else L / L0
        C = max(chroma_floor, C0 * max(0.0, t) ** 0.7)
        out[step] = _oklch_hex(L, C, h)
    return out


def _neutral():
    """Warm neutral ramp from lime white (100) to soot (950)."""
    L1, a1, b1 = _oklab(BRAND["lime"])
    L9, a9, b9 = _oklab(BRAND["soot"])
    h = math.degrees(math.atan2(b1 + b9, a1 + a9)) % 360
    out = {}
    for step, L in zip(STEPS, STEP_L):
        if step == "100":
            out[step] = BRAND["lime"]
        elif step == "950":
            out[step] = BRAND["soot"]
        else:
            out[step] = _oklch_hex(L, 0.018, h)
    return out


RAMPS = {
    "neutral": _neutral(),
    "hematite": ramp(BRAND["hematite"], "600"),
    "ochre": ramp(BRAND["ochre"], "400"),
    "maya": ramp(BRAND["maya"], "500"),
    "cochineal": ramp(BRAND["cochineal"], "700"),
    # Codex green (CO-07, painted-book palette): success only, UI only.
    "green": ramp("#2e7d4f", "700"),
}
RAMPS["hematite"]["800"] = BRAND["wine"]  # wine red is the hematite 800 step

# Semantic roles. Each value is "family.step". Light is the site's default
# scheme; dark is the game UI and the site's dark scheme.
ROLES = {
    #  role               light            dark             description
    "surface":          ("neutral.100", "neutral.950", "Page and screen panels"),
    "surface-raised":   ("neutral.50", "neutral.900", "Buttons, cards, slider tracks"),
    "surface-sunken":   ("neutral.200", "neutral.950", "Wells and inputs; text and text-muted only (dark: same as surface, set off by an outline)"),
    "text":             ("neutral.950", "neutral.100", "Body text, labels"),
    "text-muted":       ("neutral.700", "neutral.300", "Captions, help, metadata (never disabled-looking)"),
    "text-on-accent":   ("neutral.50", "neutral.950", "Text and icons on accent fills"),
    "accent":           ("hematite.600", "ochre.400", "Primary buttons, titles, links, meters"),
    "accent-pressed":   ("hematite.800", "ochre.300", "Pressed and selected primary controls"),
    "outline":          ("neutral.600", "neutral.500", "Control boundaries, dividers"),
    "focus":            ("maya.700", "maya.300", "Focus ring for keyboard and controller"),
    "link":             ("maya.700", "maya.300", "Inline links where accent is not used"),
    "danger":           ("cochineal.700", "cochineal.400", "Errors and destructive actions; always with an icon or word"),
    "success":          ("green.900", "green.200", "Success and completion; always with an icon or word"),
    "warning":          ("ochre.800", "ochre.200", "Warnings; always with an icon or word"),
    "info":             ("maya.800", "maya.200", "Neutral notices; always with an icon or word"),
    "disabled":         ("neutral.500", "neutral.600", "Disabled control text and borders (exempt from contrast)"),
    "north":            ("maya.500", "maya.500", "Tlaloc's shrine, north: always left of south"),
    "south":            ("ochre.400", "ochre.400", "Huitzilopochtli's shrine, south: always right of north"),
    "stage":            ("neutral.900", "neutral.950", "Behind the playable demo"),
    "stage-text":       ("neutral.100", "neutral.100", "Text on the stage"),
}

SCRIM = ("#000000", 0.63)


def colour(ref):
    fam, step = ref.split(".")
    return RAMPS[fam][step]


# =====================================================================
# Tokens
# =====================================================================
def dtcg_colour(h, alpha=None):
    r, g, b = _hex_to_rgb(h)
    v = {"colorSpace": "srgb", "components": [round(r, 4), round(g, 4), round(b, 4)], "hex": h}
    if alpha is not None:
        v["alpha"] = alpha
    return v


def px(n):
    return {"value": n, "unit": "px"}


# Type scale. Sizes for the game (1280 x 720 reference) keep the
# placeholder steps; the site uses rem steps of a 16 px root.
TYPE = {
    #        family     weight width  size  line  tracking(em)  case
    "display": ("display", 800, 125, 56, 60, 0.02, "upper"),
    "title":   ("display", 800, 125, 56, 60, 0.02, "upper"),
    "heading": ("display", 700, 112, 32, 38, 0.01, "none"),
    "subheading": ("display", 700, 100, 24, 30, 0.0, "none"),
    "body":    ("body", 400, None, 22, 32, 0.0, "none"),
    "label":   ("body", 700, None, 20, 26, 0.01, "none"),
    "figure":  ("body", 700, None, 22, 26, 0.0, "none"),
    "caption": ("body", 400, None, 16, 22, 0.01, "none"),
}
SITE_TYPE = {"display": 3.0, "heading": 1.75, "subheading": 1.25, "body": 1.0,
             "label": 0.9375, "caption": 0.875}

FONT_FAMILY = {
    "display": "Archivo",
    "body": "Atkinson Hyperlegible Next",
}


def tokens():
    c = {"$type": "color"}
    dark = {k: v[1] for k, v in ROLES.items()}
    # Game drop-in keys (game/ui/theme/placeholder.tokens.json): dark UI.
    game_keys = {
        "surface": "Screen panels",
        "surface-raised": "Buttons, cards, tracks",
        "text": "Text on surface and surface-raised",
        "text-muted": "Captions and disabled text",
        "text-on-accent": "Text on accent and accent-pressed fills",
        "accent": "Primary actions, titles, meters",
        "accent-pressed": "Pressed primary actions",
        "focus": "Focus ring for keyboard and controller",
        "danger": "DangerLabel; pair with an icon or word",
    }
    for k, d in game_keys.items():
        c[k] = {"$value": "{color.role.dark." + k + "}", "$description": d}
    c["scrim"] = {"$value": dtcg_colour(SCRIM[0], SCRIM[1]), "$description": "Behind modals (black at 63 percent)"}
    c["brand"] = {k: {"$value": dtcg_colour(v)} for k, v in BRAND.items()}
    c["brand"]["$description"] = "Anchors, one per pigment family; each is pinned in its ramp"
    for fam, steps in RAMPS.items():
        c[fam] = {s: {"$value": dtcg_colour(h)} for s, h in steps.items()}
    role = {"light": {}, "dark": {}}
    for k, (lt, dk, d) in ROLES.items():
        role["light"][k] = {"$value": "{color." + lt + "}", "$description": d}
        role["dark"][k] = {"$value": "{color." + dk + "}", "$description": d}
    role["light"]["scrim"] = {"$value": "{color.scrim}"}
    role["dark"]["scrim"] = {"$value": "{color.scrim}"}
    c["role"] = role

    size = {"$type": "dimension"}
    for k in ("caption", "body", "heading", "title"):
        size[k] = {"$value": px(TYPE[k][3])}
    for k in ("display", "subheading", "label", "figure"):
        size[k] = {"$value": px(TYPE[k][3])}
    typo = {"$type": "typography"}
    for k, (fam, w, wd, s, lh, tr, case) in TYPE.items():
        typo[k] = {"$value": {
            "fontFamily": "{font.family." + fam + "}",
            "fontWeight": w,
            "fontSize": "{font.size." + k + "}",
            "lineHeight": round(lh / s, 3),
            "letterSpacing": px(round(tr * s, 2)),
        }, "$description": ("Archivo width " + str(wd) + ", " if wd else "") + ("uppercase" if case == "upper" else "sentence case")}
    return {
        "$description": ("Montezuma's design tokens (W3C DTCG 2025.10), WP-04. Generated by "
                         "design/identity/build.py; edit that file, not this one. Drop-in for "
                         "game/ui/theme/placeholder.tokens.json: the top-level color, font, space, "
                         "radius, border and motion keys are the game's dark UI. Light and dark roles "
                         "are under color.role. Rules: design/README.md. Contrast: "
                         "design/identity/check_contrast.py. SPDX-License-Identifier: GPL-2.0-or-later"),
        "color": c,
        "font": {
            "size": size,
            "family": {
                "body": {"$value": "", "$description": "Atkinson Hyperlegible Next (OFL 1.1), variable wght 200-800. Not vendored yet: set the res:// path once the ui-developer adds the font (design/README.md, Type). Empty uses Godot's default"},
                "display": {"$value": "", "$description": "Archivo (OFL 1.1), variable wdth 62-125, wght 100-900; use wdth 125 wght 800 for titles. Not vendored yet: set the res:// path once added. Empty uses Godot's default"},
            },
            "weight": {"$type": "fontWeight", "regular": {"$value": 400}, "bold": {"$value": 700},
                       "black": {"$value": 800}},
            "line-height": {"$type": "number",
                            **{k: {"$value": round(v[4] / v[3], 3)} for k, v in TYPE.items()}},
        },
        "typography": typo,
        "space": {
            "$type": "dimension",
            "1": {"$value": px(4)}, "2": {"$value": px(8)}, "3": {"$value": px(12)},
            "4": {"$value": px(16)}, "5": {"$value": px(24)}, "6": {"$value": px(40)},
        },
        "radius": {
            "control": {"$type": "dimension", "$value": px(4), "$description": "Corners of controls (panels use 2x). Small: cut stone, not pills"},
        },
        "border": {
            "focus-width": {"$type": "dimension", "$value": px(3)},
            "outline-width": {"$type": "dimension", "$value": px(2), "$description": "Soot outline on drawn art and panel frames"},
        },
        "motion": {
            "duration": {
                "$type": "duration",
                "fast": {"$value": {"value": 100, "unit": "ms"}},
                "normal": {"$value": {"value": 150, "unit": "ms"}},
            },
        },
        "band": {
            "$description": "Step-fret band (design/patterns/step-fret-band.svg): tile 48 x 72 at 1x, scale by whole numbers",
            "height": {"$type": "dimension", "$value": px(36), "$description": "Default band height in UI and site headers (tile at 0.5x)"},
            "height-min": {"$type": "dimension", "$value": px(18), "$description": "Below this, use plain colour rules instead of the fret"},
        },
    }


def tokens_css():
    def role_vars(i):
        return {k: colour(v[i]) for k, v in ROLES.items()}
    lt, dk = role_vars(0), role_vars(1)

    def site(vals):
        return [
            ("--color-bg", vals["surface"]),
            ("--color-surface", vals["surface-raised"]),
            ("--color-text", vals["text"]),
            ("--color-muted", vals["text-muted"]),
            ("--color-accent", vals["accent"]),
            ("--color-accent-text", vals["text-on-accent"]),
            ("--color-border", vals["outline"]),
            ("--color-focus", vals["focus"]),
            ("--color-stage", vals["stage"]),
            ("--color-stage-text", vals["stage-text"]),
            ("--color-accent-pressed", vals["accent-pressed"]),
            ("--color-link", vals["link"]),
            ("--color-danger", vals["danger"]),
            ("--color-success", vals["success"]),
            ("--color-warning", vals["warning"]),
            ("--color-info", vals["info"]),
            ("--color-north", vals["north"]),
            ("--color-south", vals["south"]),
        ]
    out = ["/* SPDX-License-Identifier: GPL-2.0-or-later */",
           "/* Copyright (C) 2026 Charles (opsroller) and Montezuma contributors */",
           "",
           "/* Montezuma's design tokens for the website (WP-04). Generated by",
           "   design/identity/build.py from the same values as",
           "   design/tokens/montezuma.tokens.json; edit the script, not this file.",
           "   Drop-in for site/src/assets/tokens.css: every earlier name is kept.",
           "   Every text pair meets WCAG 2.2 AA (design/identity/check_contrast.py).",
           "   Fonts are not vendored yet: until the @font-face rules are added the",
           "   stacks fall back to system fonts (design/README.md, Type). */",
           ":root {"]
    for k, v in site(lt):
        out.append(f"  {k}: {v};")
    out.append("")
    out.append("  /* Brand anchors: fills and art only; use the role colours above for text. */")
    for k, v in BRAND.items():
        out.append(f"  --brand-{k}: {v};")
    out += [
        "",
        '  --font-body: "Atkinson Hyperlegible Next", system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;',
        '  --font-display: "Archivo", "Arial Black", system-ui, sans-serif;',
        "  --font-display-stretch: 125%;",
        "  --font-display-weight: 800;",
        "  --font-display-tracking: 0.02em;",
        "",
    ]
    for k, v in SITE_TYPE.items():
        out.append(f"  --text-{k}: {v:g}rem;")
    out += [
        "",
        "  --radius: 0.25rem;",
        "  --space: 1rem;",
        "  --measure: 70ch;",
        "  --band-height: 2.25rem;",
        "}",
        "",
        "@media (prefers-color-scheme: dark) {",
        "  :root {",
    ]
    for k, v in site(dk):
        out.append(f"    {k}: {v};")
    out += ["  }", "}", ""]
    return "\n".join(out)


# =====================================================================
# Geometry helpers
# =====================================================================
def poly_d(pts):
    return "M" + " ".join(f"{fmt(x)},{fmt(y)}" for x, y in pts) + "Z"


def rect_pts(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def _signed_area(pts):
    return sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1])) / 2


def orient(pts, clockwise=True):
    """Clockwise on screen (y down) means positive shoelace area."""
    pts = list(pts)
    if (_signed_area(pts) > 0) != clockwise:
        pts.reverse()
    return pts


def chamfer(pts, c, lo=80, hi=100):
    """Cut every convex corner whose angle is between lo and hi degrees by
    `c` along each edge: the cut-stone corner of the wordmark."""
    pts = orient(pts)
    n = len(pts)
    out = []
    for i in range(n):
        p0, p1, p2 = pts[i - 1], pts[i], pts[(i + 1) % n]
        v1 = (p0[0] - p1[0], p0[1] - p1[1])
        v2 = (p2[0] - p1[0], p2[1] - p1[1])
        l1, l2 = math.hypot(*v1), math.hypot(*v2)
        ang = math.degrees(math.acos(max(-1, min(1, (v1[0] * v2[0] + v1[1] * v2[1]) / (l1 * l2)))))
        cross = (p1[0] - p0[0]) * (p2[1] - p1[1]) - (p1[1] - p0[1]) * (p2[0] - p1[0])
        if cross > 0 and lo <= ang <= hi:
            k = min(c, l1 / 2.5, l2 / 2.5)
            out.append((p1[0] + v1[0] / l1 * k, p1[1] + v1[1] / l1 * k))
            out.append((p1[0] + v2[0] / l2 * k, p1[1] + v2[1] / l2 * k))
        else:
            out.append(p1)
    return out


# =====================================================================
# The mark: the twin temple
# =====================================================================
# Construction grid: 96 units. The viewer stands in the plaza facing
# east, up the west stairs (AR-10), so Tlaloc's blue shrine (north) is on
# the LEFT and Huitzilopochtli's ochre shrine (south) on the RIGHT. Never
# mirror it.
SW = 3.0                       # soot outline, centred on every edge
TIERS = [                      # y top, y bottom, x bottom-left/right, x top-left/right (talud)
    (72, 88, 6, 90, 9, 87),
    (58, 72, 15, 81, 17.5, 78.5),
    (46, 58, 23.5, 72.5, 25.5, 70.5),
]
STAIRS = [(30, 40), (56, 66)]   # x ranges; both run from y 46 to the base (88)
STAIR_TOP, BASE = 46, 88
TREADS = [52, 58, 64, 70, 76, 82]
TREAD_W = 1.6
SHRINES = [(26, 44), (52, 70)]  # x ranges, y 28..46
SHRINE_TOP = 28
DOOR_W, DOOR_TOP = 6, 36        # doorway width; it runs down to the shrine floor
ROOFS = [(24.75, 45.25), (50.75, 71.25)]
ROOF_TOP = 22
MARK_BOX = (6 - SW / 2, ROOF_TOP - SW / 2, 90 + SW / 2, BASE + SW / 2)  # tight bounds incl. outline
MONO_GAP = 1.5                  # knockout line between tiers in one colour
SHRINE_UNIT = SHRINES[0][1] - SHRINES[0][0] + SW   # "x": the clear-space unit (21)


def _lerp(a, b, t):
    return a + (b - a) * t


def tier_pieces():
    """Each tier as three polygons split by the two stairs."""
    out = []
    for yt, yb, xbl, xbr, xtl, xtr in TIERS:
        (s0a, s0b), (s1a, s1b) = STAIRS
        out.append([(xtl, yt), (s0a, yt), (s0a, yb), (xbl, yb)])
        out.append(rect_pts(s0b, yt, s1a, yb))
        out.append([(s1b, yt), (xtr, yt), (xbr, yb), (s1b, yb)])
    return out


def shrine_poly(x0, x1, mono=False):
    """A shrine is a box with its doorway cut out of the foot (a notch), so
    the one-colour and reversed marks keep the doorway as a knockout. In one
    colour the notch is widened by one outline so the opening shows as wide
    as the soot doorway does in full colour."""
    cx = (x0 + x1) / 2
    w = DOOR_W + (SW if mono else 0)
    top = DOOR_TOP - (SW / 2 if mono else 0)
    d0, d1 = cx - w / 2, cx + w / 2
    return [(x0, SHRINE_TOP), (x1, SHRINE_TOP), (x1, STAIR_TOP), (d1, STAIR_TOP),
            (d1, top), (d0, top), (d0, STAIR_TOP), (x0, STAIR_TOP)]


def mark_elements(style="colour", soot=None, halo=None):
    """SVG elements for the mark on its 96-unit grid.

    style: "colour"   full colour, soot outline (light grounds)
           "on-dark"  full colour with a lime keyline round the silhouette
           "mono"     one colour (soot or any `soot` override); fills that are
                      lime in colour, and the doorways, are knockouts
    """
    ink = soot or BRAND["soot"]
    mono = style == "mono"
    plat = ink if mono else BRAND["hematite"]
    lime = "none" if mono else BRAND["lime"]
    north = ink if mono else BRAND["maya"]
    south = ink if mono else BRAND["ochre"]
    stroke = f'stroke="{ink}" stroke-width="{fmt(SW)}" stroke-linejoin="miter"'
    shapes = []   # (points, fill)
    for i, p in enumerate(tier_pieces()):
        if mono and i < 6:
            # One colour: lower each tier's top edge so a knockout line
            # (MONO_GAP wide) separates it from the tier above.
            yt = p[0][1]
            drop = SW + MONO_GAP
            t = drop / (p[3][1] - yt)
            p = [(_lerp(p[0][0], p[3][0], t), yt + drop), (_lerp(p[1][0], p[2][0], t), yt + drop), p[2], p[3]]
        shapes.append((p, plat))
    for x0, x1 in STAIRS:
        shapes.append((rect_pts(x0, STAIR_TOP, x1, BASE), lime))
    for (x0, x1), fill in zip(SHRINES, (north, south)):
        shapes.append((shrine_poly(x0, x1, mono), fill))
    for x0, x1 in ROOFS:
        shapes.append((rect_pts(x0, ROOF_TOP, x1, SHRINE_TOP), lime))
    el = []
    if style == "on-dark":
        k = halo or 2.5
        d = " ".join(poly_d(p) for p, _ in shapes)
        el.append(f'<path d="{d}" fill="{BRAND["lime"]}" stroke="{BRAND["lime"]}" '
                  f'stroke-width="{fmt(SW + 2 * k)}" stroke-linejoin="miter"/>')
    if not mono:
        # Doorways: soot openings behind the notch in each shrine.
        for x0, x1 in SHRINES:
            cx = (x0 + x1) / 2
            el.append(f'<path d="{poly_d(rect_pts(cx - DOOR_W / 2, DOOR_TOP, cx + DOOR_W / 2, STAIR_TOP))}" fill="{ink}"/>')
    for p, fill in shapes:
        el.append(f'<path d="{poly_d(p)}" fill="{fill}" {stroke}/>')
    for x0, x1 in STAIRS:
        for y in TREADS:
            el.append(f'<path d="{poly_d(rect_pts(x0, y - TREAD_W / 2, x1, y + TREAD_W / 2))}" fill="{ink}"/>')
    return el


def mark_box(pad=0.0):
    x0, y0, x1, y1 = MARK_BOX
    return x0 - pad, y0 - pad, x1 + pad, y1 + pad


def svg_doc(view, w, h, title, body, desc=None):
    vx, vy, vw, vh = view
    d = f"\n  <desc>{desc}</desc>" if desc else ""
    return (SVG_HEAD + f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{fmt(vx)} {fmt(vy)} {fmt(vw)} {fmt(vh)}" '
            f'width="{fmt(w)}" height="{fmt(h)}" role="img" aria-label="{title}">\n'
            f"  <title>{title}</title>{d}\n  " + "\n  ".join(body) + "\n</svg>\n")


def mark_svg(style="colour", soot=None, title="Montezuma's mark: the twin temple", pad=0.0):
    x0, y0, x1, y1 = mark_box(pad)
    if style == "on-dark":
        x0, y0, x1, y1 = mark_box(pad + 2.5)
    w, h = x1 - x0, y1 - y0
    return svg_doc((x0, y0, w, h), w * 6, h * 6, title, mark_elements(style, soot))


# Small mark: drawn pixel by pixel on a 16-unit grid for 16 to 31 px
# (1 unit = 1 px at 16 px, 2 px at 32 px). Two shrines, each a soot box
# with a 4 x 1 lime roof and a 2 px doorway open to the floor, over a
# two-tier platform; two 2 px lime stairs run up to the doorways. No
# treads or talud.
def pixel_mark(field=None, ink=None):
    S, L = ink or BRAND["soot"], BRAND["lime"]
    px_ = []
    for x0, fill in ((1, BRAND["maya"]), (9, BRAND["ochre"])):   # north left, south right
        px_ += [(x0, 0, 6, 7, S), (x0 + 1, 1, 4, 1, L), (x0 + 1, 3, 4, 3, fill), (x0 + 2, 4, 2, 3, S)]
    px_ += [
        (2, 6, 12, 5, S), (3, 7, 10, 3, BRAND["hematite"]),    # upper tier
        (0, 10, 16, 5, S), (1, 11, 14, 3, BRAND["hematite"]),  # lower tier
        (3, 7, 2, 7, L), (11, 7, 2, 7, L),                    # stairs
    ]
    el = []
    if field:
        el.append(f'<rect width="16" height="16" fill="{field}"/>')
    el += [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{c}"/>' for x, y, w, h, c in px_]
    return el


def mark_small_svg():
    body = pixel_mark()
    return (SVG_HEAD + '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" width="16" height="16" '
            'shape-rendering="crispEdges" role="img" aria-label="Montezuma\'s mark, small (16 to 32 px)">\n'
            "  <title>Montezuma's mark, small (16 to 31 px)</title>\n  " + "\n  ".join(body) + "\n</svg>\n")


# =====================================================================
# Wordmark: MONTEZUMA'S, drawn (no font)
# =====================================================================
# Cap height 100 units, baseline at y 100 (y runs down). Vertical stems
# 20, horizontal bars 16, diagonals about 19: the small contrast between
# stems and bars is what keeps it from reading as pixel type. Every
# square outer corner is cut by CUT units, like a dressed stone block;
# round letters (O, S, U) are cut big on the diagonal and overshoot the
# cap and base lines by OV. Inner corners stay sharp.
CAP = 100
CUT = 4.5
OV = 2.0


def _glyph_M():
    # Upper edge of the left diagonal: (26, 0) to the notch (58, 66). The
    # lower edge is parallel, 19 units away, cut off at the baseline.
    d = 19
    ux, uy = 32, 66
    n = math.hypot(ux, uy)
    nx, ny = -uy / n * d, ux / n * d          # perpendicular, down-left
    ax, ay = 26 + nx, 0 + ny                   # point on the lower edge
    y20 = ay + (20 - ax) * uy / ux             # meets the stem at x 20
    xb = ax + (CAP - ay) * ux / uy             # meets the baseline
    return 116, [[(0, CAP), (0, 0), (26, 0), (58, 66), (90, 0), (116, 0), (116, CAP), (96, CAP),
                  (96, y20), (116 - xb, CAP), (xb, CAP), (20, y20), (20, CAP)]], []


def _glyph_O():
    o = [(30, -OV), (70, -OV), (100, 30 - OV), (100, 70 + OV), (70, CAP + OV), (30, CAP + OV),
         (0, 70 + OV), (0, 30 - OV)]
    # Counter: stems 21, bars 17, diagonal cut parallel to the outer one at 20.
    k = 30 - OV + 20 * math.sqrt(2)            # inner diagonal: y = k - x (top left)
    i0, i1, t, b = 21, 79, 17 - OV, CAP + OV - 17
    hole = [(k - t, t), (100 - (k - t), t), (i1, k - (100 - i1)), (i1, CAP - (k - (100 - i1))),
            (100 - (k - t), b), (k - t, b), (i0, CAP - (k - i0)), (i0, k - i0)]
    return 100, [o], [hole]


def _glyph_N():
    # Diagonal between (24, 0)-(94, 100) and the parallel line through (70, 100).
    return 94, [[(0, 0), (24, 0), (74, 0 + 50 / 0.7), (74, 0), (94, 0), (94, CAP), (70, CAP),
                 (20, CAP - 50 / 0.7), (20, CAP), (0, CAP)]], []


def _glyph_T():
    return 88, [[(0, 0), (88, 0), (88, 16), (54, 16), (54, CAP), (34, CAP), (34, 16), (0, 16)]], []


def _glyph_E():
    return 72, [[(0, 0), (72, 0), (72, 16), (20, 16), (20, 42), (64, 42), (64, 58), (20, 58),
                 (20, 84), (72, 84), (72, CAP), (0, CAP)]], []


def _glyph_Z():
    return 84, [[(0, 0), (84, 0), (84, 16), (25, 84), (84, 84), (84, CAP), (0, CAP), (0, 84),
                 (59, 16), (0, 16)]], []


def _glyph_U():
    k = 20 * math.sqrt(2)
    yi = 74 + OV - k + 20                      # inner diagonal meets the stem
    xi = 20 + (CAP + OV - 17) - yi             # ...and the inner bowl floor
    return 94, [[(0, 0), (20, 0), (20, yi), (xi, CAP + OV - 17), (94 - xi, CAP + OV - 17),
                 (74, yi), (74, 0), (94, 0), (94, 74 + OV), (66, CAP + OV), (28, CAP + OV),
                 (0, 74 + OV)]], []


def _glyph_A():
    # Legs slope 0.37 in x per unit of y; perpendicular stroke about 19.
    s = 0.37
    w = 20.3                                   # horizontal leg width
    apex_y = (100 - w - w) / (2 * s)           # where the inner edges meet, from the base
    inner_apex = (50, CAP - (50 - w) / s)
    bar_t, bar_b = 58, 74

    def left_in(y):
        return w + s * (CAP - y)

    def right_in(y):
        return 100 - w - s * (CAP - y)
    outer = [(0, CAP), (37, 0), (63, 0), (100, CAP), (100 - w, CAP), (right_in(bar_b), bar_b),
             (left_in(bar_b), bar_b), (w, CAP)]
    hole = [inner_apex, (right_in(bar_t), bar_t), (left_in(bar_t), bar_t)]
    del apex_y
    return 100, [outer], [hole]


def _glyph_apos():
    # A real apostrophe: a squared head that tapers into a tail turning left.
    return 22, [[(0, 0), (22, 0), (22, 22), (13, 42), (3, 42), (9, 24), (0, 24)]], []


def _glyph_S():
    r2 = 20 * math.sqrt(2)                     # diagonal cut thickness
    a = 24 - OV + r2                           # inner top-left cut: y = a - x
    ti = 17 - OV                               # inner top bar line
    p = [(24, -OV), (60, -OV), (84, 24 - OV), (84, 31), (64, 31),
         (64 - (31 - ti), ti), (a - ti, ti), (21, a - 21), (21, 36), (27, 42), (60, 42),
         (84, 66), (84, 78 - 2 + OV), (60, CAP + OV), (24, CAP + OV), (0, 76 + OV), (0, 69),
         (20, 69), (20 + (CAP - ti - 69), CAP - ti), (84 - (a - ti), CAP - ti),
         (63, CAP - (a - 21)), (63, 64), (57, 58), (24, 58), (0, 34), (0, 24 - OV)]
    return 84, [p], []


GLYPHS = {
    "M": _glyph_M(), "O": _glyph_O(), "N": _glyph_N(), "T": _glyph_T(), "E": _glyph_E(),
    "Z": _glyph_Z(), "U": _glyph_U(), "A": _glyph_A(), "'": _glyph_apos(), "S": _glyph_S(),
}
# Side bearings (left, right): stems 9, rounds 6, open shapes less.
BEARINGS = {"M": (9, 9), "O": (6, 6), "N": (9, 9), "T": (2, 2), "E": (9, 4), "Z": (4, 4),
            "U": (9, 9), "A": (1, 1), "'": (5, 5), "S": (5, 5)}
KERN = {("A", "'"): -16, ("'", "S"): -4, ("N", "T"): -2, ("T", "E"): -1, ("M", "A"): -2}
TRACK = 6          # extra open tracking for display


def word_paths(text="MONTEZUMA'S"):
    """One path per glyph (outer contours clockwise, counters anticlockwise,
    nonzero fill). Returns (paths, width) in cap-100 units."""
    paths, x, prev = [], 0.0, None
    for ch in text:
        w, outers, holes = GLYPHS[ch]
        lsb, rsb = BEARINGS[ch]
        if prev is not None:
            x += KERN.get((prev, ch), 0) + TRACK
        x0 = x + (lsb if prev is not None else 0)
        rings = []
        for o in outers:
            rings.append(poly_d([(px_ + x0, py) for px_, py in chamfer(o, CUT)]))
        for h in holes:
            rings.append(poly_d([(px_ + x0, py) for px_, py in orient(h, clockwise=False)]))
        paths.append(" ".join(rings))
        x = x0 + w + rsb
        prev = ch
    last_rsb = BEARINGS[text[-1]][1]
    return paths, x - last_rsb


WORD_PATHS, WORD_W = word_paths()


def wordmark_group(fill, x=0.0, y=0.0, cap=CAP):
    s = cap / CAP
    d = " ".join(WORD_PATHS)
    return f'<path transform="translate({fmt(x)} {fmt(y)}) scale({fmt(s)})" fill="{fill}" d="{d}"/>'


def wordmark_svg(fill, title):
    pad = 4
    view = (-pad, -OV - pad, WORD_W + 2 * pad, CAP + 2 * OV + 2 * pad)
    return svg_doc(view, view[2] * 2, view[3] * 2, title, [wordmark_group(fill)])


# =====================================================================
# Lockups
# =====================================================================
# Horizontal: the wordmark's cap height equals tiers 1 and 2 (30 units)
# and its baseline sits on the platform's base line, so word and temple
# stand on one ground. Gap between them: 0.75 x (x = shrine width + outline).
H_CAP = BASE - TIERS[1][0]          # 30
H_GAP = 0.75 * SHRINE_UNIT
# Stacked: mark centred over the word; word cap 24, gap 0.5 x.
S_CAP = 24
S_GAP = 0.5 * SHRINE_UNIT


def lockup_horizontal(reversed_=False):
    style = "on-dark" if reversed_ else "colour"
    word = BRAND["lime"] if reversed_ else BRAND["soot"]
    x0, y0, x1, y1 = mark_box(2.5 if reversed_ else 0)
    wx = x1 + H_GAP
    s = H_CAP / CAP
    body = mark_elements(style) + [wordmark_group(word, wx, BASE - H_CAP, H_CAP)]
    w = wx + WORD_W * s - x0
    h = y1 - y0
    name = "Montezuma's lockup, horizontal" + (", reversed" if reversed_ else "")
    return svg_doc((x0, y0, w, h), w * 3, h * 3, name, body)


def lockup_stacked(reversed_=False):
    style = "on-dark" if reversed_ else "colour"
    word = BRAND["lime"] if reversed_ else BRAND["soot"]
    x0, y0, x1, y1 = mark_box(2.5 if reversed_ else 0)
    s = S_CAP / CAP
    ww = WORD_W * s
    cx = (MARK_BOX[0] + MARK_BOX[2]) / 2
    wy = y1 + S_GAP + OV * s
    body = mark_elements(style) + [wordmark_group(word, cx - ww / 2, wy, S_CAP)]
    vx0 = min(x0, cx - ww / 2)
    vw = max(x1, cx + ww / 2) - vx0
    vh = wy + S_CAP + OV * s - y0
    name = "Montezuma's lockup, stacked" + (", reversed" if reversed_ else "")
    return svg_doc((vx0, y0, vw, vh), vw * 3, vh * 3, name, body)


# =====================================================================
# Step-fret band (from Direction 2): a seamless horizontal tile
# =====================================================================
# Cells of 4 units. One fret unit is 12 x 8 cells: a ground line, a stair
# rising in steps of two cells and a squared hook turning in on itself
# (the xicalcoliuhqui, DR-10), drawn as plain grid geometry. The tile stacks
# Maya blue (3 cells), a soot rule, the fret row on ochre, a soot rule and
# hematite red (3 cells): 12 x 18 cells, 48 x 72 units.
CELL = 4
FRET_W, FRET_H = 12, 8
FRET_CELLS = (
    [(x, 7) for x in range(12)]                                          # ground line
    + [(1, 6), (1, 5), (2, 5), (3, 5), (3, 4), (3, 3), (4, 3), (5, 3), (5, 2), (5, 1), (5, 0)]  # stair
    + [(x, 0) for x in range(6, 12)] + [(11, y) for y in range(1, 6)]    # hook: top, right
    + [(x, 5) for x in range(7, 11)] + [(7, y) for y in range(2, 5)]     # bottom, left
    + [(8, 2), (9, 2), (9, 3)]                                           # turn in
)
BAND_ROWS = [("maya", 3), ("soot", 1), ("ochre", 1), ("fret", FRET_H), ("ochre", 1), ("soot", 1),
             ("hematite", 3)]


def trace_cells(cells):
    """Outline a set of grid cells as closed loops (one loop per outline)."""
    cells = set(cells)
    edges = {}
    for (x, y) in cells:
        for a, b, nb in (((x, y), (x + 1, y), (x, y - 1)), ((x + 1, y), (x + 1, y + 1), (x + 1, y)),
                         ((x + 1, y + 1), (x, y + 1), (x, y + 1)), ((x, y + 1), (x, y), (x - 1, y))):
            if nb not in cells:
                edges.setdefault(a, []).append(b)
    loops = []
    while edges:
        start = min(edges)
        loop, cur = [start], start
        while True:
            nxt = edges[cur].pop(0)
            if not edges[cur]:
                del edges[cur]
            cur = nxt
            if cur == start:
                break
            loop.append(cur)
        simple = [p for i, p in enumerate(loop)
                  if not ((loop[i - 1][0] == p[0] == loop[(i + 1) % len(loop)][0]) or
                          (loop[i - 1][1] == p[1] == loop[(i + 1) % len(loop)][1]))]
        loops.append(simple)
    return loops


def band_elements(width_units, oy=0.0, ox=0.0):
    """Band elements spanning `width_units` (a multiple of the 48-unit tile
    is seamless; anything else is cut at the right edge)."""
    reps = int(math.ceil(width_units / (FRET_W * CELL))) + 2
    el, y = [], oy
    fret_y = None
    for name, rows in BAND_ROWS:
        h = rows * CELL
        if name == "fret":
            fret_y = y
            name = "ochre"
        el.append(f'<rect x="{fmt(ox)}" y="{fmt(y)}" width="{fmt(width_units)}" height="{fmt(h)}" fill="{BRAND[name]}"/>')
        y += h
    cells = [(x + FRET_W * r, yy) for r in range(-1, reps) for (x, yy) in FRET_CELLS]
    loops = trace_cells(cells)
    d = " ".join(poly_d([(ox + x * CELL, fret_y + yy * CELL) for x, yy in lp]) for lp in loops)
    clip = f"band-clip-{fmt(ox)}-{fmt(oy)}".replace(".", "_")
    el.append(f'<clipPath id="{clip}"><rect x="{fmt(ox)}" y="{fmt(oy)}" width="{fmt(width_units)}" height="{fmt(y - oy)}"/></clipPath>')
    el.append(f'<g clip-path="url(#{clip})"><path d="{d}" fill="{BRAND["soot"]}" stroke="{BRAND["soot"]}" '
              f'stroke-width="2" stroke-linejoin="miter"/><path d="{d}" fill="{BRAND["lime"]}"/></g>')
    return el, y - oy


BAND_TILE_W = FRET_W * CELL                     # 48
BAND_TILE_H = sum(r for _, r in BAND_ROWS) * CELL  # 72


def band_svg():
    el, h = band_elements(BAND_TILE_W)
    return svg_doc((0, 0, BAND_TILE_W, h), BAND_TILE_W, h,
                   "Montezuma's step-fret band, seamless tile", el,
                   desc="Repeat horizontally only. Never rotate; mirror top to bottom only to close the foot of a frame.")


# =====================================================================
# App icon (1024 master) and its 16 px pixel variant
# =====================================================================
# Field: deep Maya blue (maya 900), which holds on light and dark home
# screens; the temple stands on the step-fret band at the foot. Full
# bleed, no baked corners or shadows: stores and launchers mask it.
ICON_FIELD = RAMPS["maya"]["900"]
ICON_BAND_SCALE = 2.0            # band tile at 2x: 144 px tall
ICON_MARK_W = 820                # mark width in px (80 percent)
ICON_BAND_GAP = 20               # field between the keyline and the band


def app_icon_svg():
    band_el, bh = band_elements(1024 / ICON_BAND_SCALE)
    band_h = bh * ICON_BAND_SCALE
    x0, y0, x1, y1 = MARK_BOX
    k = ICON_MARK_W / (x1 - x0)
    base_y = 1024 - band_h
    tx = (1024 - ICON_MARK_W) / 2 - x0 * k
    # the keyline (on-dark mark) clears the band by ICON_BAND_GAP px of field
    ty = base_y - ICON_BAND_GAP - (BASE + SW / 2 + 4) * k
    body = [f'<rect width="1024" height="1024" fill="{ICON_FIELD}"/>',
            f'<g transform="translate(0 {fmt(base_y)}) scale({fmt(ICON_BAND_SCALE)})">' + "".join(band_el) + "</g>",
            f'<g transform="translate({fmt(tx)} {fmt(ty)}) scale({fmt(k)})">' + "".join(mark_elements("on-dark")) + "</g>"]
    return svg_doc((0, 0, 1024, 1024), 1024, 1024, "Montezuma's app icon", body,
                   desc="1024 master. Full bleed; the platform masks it. Store exports: WP-38.")


def app_icon_16_svg():
    body = pixel_mark(field=ICON_FIELD)
    return (SVG_HEAD + '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" width="16" height="16" '
            'shape-rendering="crispEdges" role="img" aria-label="Montezuma\'s app icon, 16 px">\n'
            "  <title>Montezuma's app icon, 16 px</title>\n  " + "\n  ".join(body) + "\n</svg>\n")


# =====================================================================
# Write everything
# =====================================================================
def write(rel, text):
    path = os.path.join(DESIGN, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    return rel


def main():
    out = [
        write("logo/mark.svg", mark_svg()),
        write("logo/mark-on-dark.svg", mark_svg("on-dark", title="Montezuma's mark, full colour on dark")),
        write("logo/mark-mono.svg", mark_svg("mono", title="Montezuma's mark, one colour")),
        write("logo/mark-reversed.svg", mark_svg("mono", soot=BRAND["lime"], title="Montezuma's mark, reversed")),
        write("logo/mark-small.svg", mark_small_svg()),
        write("logo/wordmark.svg", wordmark_svg(BRAND["soot"], "Montezuma's wordmark")),
        write("logo/wordmark-reversed.svg", wordmark_svg(BRAND["lime"], "Montezuma's wordmark, reversed")),
        write("logo/lockup-horizontal.svg", lockup_horizontal()),
        write("logo/lockup-horizontal-reversed.svg", lockup_horizontal(True)),
        write("logo/lockup-stacked.svg", lockup_stacked()),
        write("logo/lockup-stacked-reversed.svg", lockup_stacked(True)),
        write("patterns/step-fret-band.svg", band_svg()),
        write("icons/app-icon.svg", app_icon_svg()),
        write("icons/app-icon-16.svg", app_icon_16_svg()),
        write("tokens/montezuma.tokens.json", json.dumps(tokens(), indent=2, ensure_ascii=False) + "\n"),
        write("tokens/tokens.css", tokens_css()),
    ]
    for rel in out:
        print("wrote design/" + rel)


if __name__ == "__main__":
    main()

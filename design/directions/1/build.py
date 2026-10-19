# SPDX-License-Identifier: GPL-2.0-or-later
"""Direction 1, "The twin temple": writes every SVG in this folder and
renders the PNG previews with headless Chromium.

    python3 design/directions/1/build.py            # SVGs only
    python3 design/directions/1/build.py --png      # SVGs and PNGs

All geometry is on integer or half-unit grids defined below; no fonts are
used. Colours come from the palette in palette.md.
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SPDX = "<!-- SPDX-License-Identifier: GPL-2.0-or-later -->\n"

# Palette (palette.md)
SOOT = "#1e1916"      # outline, text
LIME = "#f4ecdc"      # lime-white field, stairs
HEMATITE = "#b23a2a"  # platform body
WINE = "#6a2224"      # deep red (field on dark, shadow band)
OCHRE = "#e09a26"     # south shrine, Huitzilopochtli
MAYA = "#2a9fb4"      # north shrine, Tlaloc


# ---------------------------------------------------------------- mark ----
# 64-unit grid. Viewer stands in the plaza facing east, so north (Tlaloc,
# blue) is on the left and south (Huitzilopochtli, ochre) on the right.
# Construction: outline 2.5 units; platform of
# three tiers, each 4 units narrower per side than the one below; two
# stairs 8 units wide whose centres line up under the shrine doorways;
# shrines 15 x 15 with an 8-unit gap between them; roof slab 1.5 wider
# per side.
def mark_shapes(outline=SOOT, plat=HEMATITE, stair=LIME,
                north=MAYA, south=OCHRE, door=SOOT, roof=LIME):
    """Return SVG elements for the symbol on a 64x64 grid."""
    sw = 2.5
    s = []
    o = f'stroke="{outline}" stroke-width="{sw}" stroke-linejoin="miter"'
    tiers = [(5, 47, 54, 10), (9, 39, 46, 8), (13, 31, 38, 8)]
    stairs = [(16.5, 31, 8, 26), (39.5, 31, 8, 26)]
    shrines = [(13, 16, 15, 15, north), (36, 16, 15, 15, south)]
    roofs = [(11.5, 12, 18, 4), (34.5, 12, 18, 4)]
    doors = [(18, 22, 5, 9), (41, 22, 5, 9)]
    for x, y, w, h in tiers:
        s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{plat}" {o}/>')
    for x, y, w, h in stairs:
        s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{stair}" {o}/>')
        for ty in (y + 7, y + 13, y + 19):
            s.append(f'<rect x="{x}" y="{ty}" width="{w}" height="1" fill="{outline}"/>')
    for x, y, w, h, c in shrines:
        s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{c}" {o}/>')
    for x, y, w, h in roofs:
        s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{roof}" {o}/>')
    for x, y, w, h in doors:
        s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{door}"/>')
    return s


# 16 px variant, drawn pixel by pixel on a 16-unit grid (1 unit = 1 px at
# 16 px, 3 px at 48 px): two 6 x 6 outlined shrines with a 2 px gap, a
# two-tier platform, two 2 px stairs running up into the doorways.
def pixel_mark_shapes(field=LIME):
    px = [
        (1, 1, 6, 6, SOOT), (9, 1, 6, 6, SOOT),      # shrine outlines
        (2, 2, 4, 4, MAYA), (10, 2, 4, 4, OCHRE),     # north, south
        (2, 6, 12, 5, SOOT), (3, 7, 10, 3, HEMATITE),  # upper tier
        (0, 10, 16, 5, SOOT), (1, 11, 14, 3, HEMATITE),  # lower tier
        (3, 7, 2, 7, LIME), (11, 7, 2, 7, LIME),      # stairs
        (3, 4, 2, 2, SOOT), (11, 4, 2, 2, SOOT),      # doorways
    ]
    return [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{c}"/>'
            for x, y, w, h, c in px]


def mark_small_svg():
    body = "\n  ".join(pixel_mark_shapes())
    return (SPDX + '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" '
            'width="256" height="256" shape-rendering="crispEdges" role="img" '
            'aria-label="Montezuma\'s twin-temple mark, 16 px">\n'
            "  <title>Montezuma's twin-temple mark, 16 px</title>\n"
            f'  <rect width="16" height="16" fill="{LIME}"/>\n  {body}\n</svg>\n')


def mark_svg(**kw):
    body = "\n  ".join(mark_shapes(**kw))
    title = "Montezuma's twin-temple mark" + (", one colour" if kw else "")
    return (SPDX + f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" '
            f'width="512" height="512" role="img" aria-label="{title}">\n'
            f'  <title>{title}</title>\n  {body}\n</svg>\n')


# ------------------------------------------------------------ wordmark ----
# Cap height 10 units, stem 2 units. Corners are notched by one unit, the
# same step as a temple terrace. Letters are unions of rects and polygons.
def R(x, y, w, h):
    return ("rect", (x, y, w, h))


def P(*pts):
    return ("poly", pts)


def P_hole(outer, hole):
    return ("hole", (outer, hole))


# Cap height 12, stem 2.6. Straight letters are square-cut; round letters
# (O, U, A, S) take a 1.5-unit chamfer, like a cut stone block. M and N
# keep true diagonals.
GLYPHS = {
    "M": (11, [P((0, 0), (2.8, 0), (5.5, 6.2), (8.2, 0), (11, 0), (11, 12), (8.4, 12),
                 (8.4, 5.4), (6.6, 9.6), (4.4, 9.6), (2.6, 5.4), (2.6, 12), (0, 12))]),
    "O": (9, [P_hole([(1.5, 0), (7.5, 0), (9, 1.5), (9, 10.5), (7.5, 12), (1.5, 12),
                      (0, 10.5), (0, 1.5)],
                     [(2.6, 2.6), (6.4, 2.6), (6.4, 9.4), (2.6, 9.4)])]),
    "N": (9, [P((0, 0), (2.6, 0), (6.4, 7.4), (6.4, 0), (9, 0), (9, 12), (6.4, 12),
                (2.6, 4.6), (2.6, 12), (0, 12))]),
    "T": (9, [R(0, 0, 9, 2.6), R(3.2, 0, 2.6, 12)]),
    "E": (7.5, [R(0, 0, 2.6, 12), R(0, 0, 7.5, 2.6), R(0, 4.7, 6.6, 2.6),
                R(0, 9.4, 7.5, 2.6)]),
    "Z": (8.5, [R(0, 0, 8.5, 2.6), R(0, 9.4, 8.5, 2.6),
                P((5.6, 2.6), (8.5, 2.6), (2.9, 9.4), (0, 9.4))]),
    "U": (9, [P((0, 0), (2.6, 0), (2.6, 9.4), (6.4, 9.4), (6.4, 0), (9, 0), (9, 10.5),
                (7.5, 12), (1.5, 12), (0, 10.5))]),
    "A": (9.5, [P_hole([(0, 12), (0, 1.5), (1.5, 0), (8, 0), (9.5, 1.5), (9.5, 12),
                        (6.9, 12), (6.9, 7.8), (2.6, 7.8), (2.6, 12)],
                       [(2.6, 2.6), (6.9, 2.6), (6.9, 5.2), (2.6, 5.2)])]),
    "'": (2.6, [P((0, 0), (2.6, 0), (2.6, 2.6), (1.3, 4.6), (0, 4.6))]),
    "S": (8.5, [P((1.5, 0), (8.5, 0), (8.5, 2.6), (2.6, 2.6), (2.6, 4.7), (7, 4.7),
                  (8.5, 6.2), (8.5, 10.5), (7, 12), (0, 12), (0, 9.4), (5.9, 9.4),
                  (5.9, 7.3), (1.5, 7.3), (0, 5.8), (0, 1.5))]),
}
CAP = 12
TRACK = 1.6


def pts_str(pts, dx):
    return " ".join(f"{x + dx:g},{y:g}" for x, y in pts)


def _area(pts):
    return sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]))


def _ring(pts, dx, clockwise=True):
    pts = list(pts)
    # SVG y points down, so a positive shoelace sum is clockwise on screen.
    if (_area(pts) > 0) != clockwise:
        pts.reverse()
    return "M" + " ".join(f"{x + dx:g},{y:g}" for x, y in pts) + "Z"


def word_elements(text="MONTEZUMA'S", fill=SOOT, x0=0.0, y0=0.0, scale=1.0):
    """Each glyph is one path (non-zero fill) so abutting parts leave no seam."""
    out, x = [], 0.0
    for i, ch in enumerate(text):
        w, parts = GLYPHS[ch]
        rings = []
        for kind, d in parts:
            if kind == "rect":
                rx, ry, rw, rh = d
                rings.append(_ring([(rx, ry), (rx + rw, ry), (rx + rw, ry + rh), (rx, ry + rh)], x))
            elif kind == "poly":
                rings.append(_ring(d, x))
            else:
                outer, hole = d
                rings.append(_ring(outer, x))
                rings.append(_ring(hole, x, clockwise=False))
        out.append(f'<path d="{" ".join(rings)}"/>')
        x += w + (TRACK if i < len(text) - 1 else 0)
    width = x
    g = (f'<g fill="{fill}" transform="translate({x0:g} {y0:g}) scale({scale:g})">'
         + "".join(out) + "</g>")
    return g, width


def wordmark_svg(fill=SOOT):
    g, w = word_elements(fill=fill)
    return (SPDX + f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="-2 -2 {w + 4:g} {CAP + 4}" '
            f'width="{(w + 4) * 8:g}" height="{(CAP + 4) * 8}" role="img" aria-label="Montezuma\'s">\n'
            f"  <title>Montezuma's wordmark</title>\n  {g}\n</svg>\n")


# -------------------------------------------------------------- lockup ----
def lockup_svg():
    # Mark 64 units tall; wordmark cap height = 24 units (scale 2.4),
    # centred on the mark's platform line (y 31..55 -> centre 43).
    g_mark = "\n  ".join(mark_shapes())
    _, w = word_elements()
    scale = 2.0
    wx = 64 + 12
    wy = 44 - CAP / 2 * scale
    g, _ = word_elements(x0=wx, y0=wy, scale=scale)
    total = wx + w * scale + 4
    return (SPDX + f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {total:g} 64" '
            f'width="{total * 6:g}" height="384" role="img" aria-label="Montezuma\'s">\n'
            f"  <title>Montezuma's lockup</title>\n"
            f'  <rect x="0" y="0" width="{total:g}" height="64" fill="{LIME}"/>\n'
            f"  {g_mark}\n  {g}\n</svg>\n")


# ------------------------------------------------------------ app icon ----
def app_icon_svg(field=LIME, ground=None):
    # 1024 canvas, full-bleed field; stores and launchers apply their own
    # corner masks. The mark fills the central 78 percent, inside the
    # Android adaptive-icon safe circle once its empty top corners are
    # counted. An optional ground band runs under the bottom tier.
    body = "\n    ".join(mark_shapes())
    k = 1024 * 0.78 / 64
    off = (1024 - 64 * k) / 2
    band = ""
    if ground:
        gy = off + 57 * k
        band = f'  <rect y="{gy:g}" width="1024" height="{1024 - gy:g}" fill="{ground}"/>\n'
    return (SPDX + '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024" '
            'width="1024" height="1024" role="img" aria-label="Montezuma\'s app icon">\n'
            "  <title>Montezuma's app icon</title>\n"
            f'  <rect width="1024" height="1024" fill="{field}"/>\n' + band +
            f'  <g transform="translate({off:g} {off:g}) scale({k:g})">\n    {body}\n  </g>\n</svg>\n')


def write(name, text):
    with open(os.path.join(HERE, name), "w") as f:
        f.write(text)


def render(svg_name, png_name, w, h):
    svg = open(os.path.join(HERE, svg_name)).read().split("\n", 1)[1]
    html = ('<!doctype html><html><head><style>html,body{margin:0;padding:0;'
            'background:transparent}svg{display:block;width:%dpx;height:%dpx}'
            '</style></head><body>%s</body></html>' % (w, h, svg))
    with tempfile.NamedTemporaryFile("w", suffix=".html", delete=False) as t:
        t.write(html)
    subprocess.run(["chromium", "--headless", "--disable-gpu", "--no-sandbox",
                    "--hide-scrollbars", f"--window-size={w},{h}",
                    "--default-background-color=00000000",
                    f"--screenshot={os.path.join(HERE, png_name)}", t.name],
                   check=True, capture_output=True)
    os.unlink(t.name)


if __name__ == "__main__":
    write("mark.svg", mark_svg())
    write("mark-mono.svg", mark_svg(plat=LIME, stair=LIME, north=SOOT, south=SOOT,
                                    door=LIME, roof=LIME))
    write("mark-small.svg", mark_small_svg())
    write("wordmark.svg", wordmark_svg())
    write("lockup.svg", lockup_svg())
    write("app-icon.svg", app_icon_svg())
    if "--png" in sys.argv:
        render("app-icon.svg", "icon-1024.png", 1024, 1024)
        render("mark-small.svg", "icon-48.png", 48, 48)
        render("mark-small.svg", "icon-16.png", 16, 16)
        _, ww = word_elements()
        render("lockup.svg", "lockup.png", round((64 + 12 + ww * 2.0 + 4) * 6), 384)

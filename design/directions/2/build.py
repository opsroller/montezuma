#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Direction 2, "The step-fret band": builds every SVG in this folder.

All geometry is drawn on an integer grid. Strokes are centre-line polylines
of width 2 grid units, rasterised to grid cells, then traced back into
closed outline paths, so no SVG uses a font or live text.

Run: python3 design/directions/2/build.py
"""
from pathlib import Path

OUT = Path(__file__).resolve().parent
HEAD = ("<!-- SPDX-License-Identifier: GPL-2.0-or-later -->\n"
        "<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->\n")

# Palette sketch (see palette.md)
SOOT = "#1c1714"      # soot black: outline and text
LIME = "#f5efe1"      # lime white: light surface
OCHRE = "#e0a030"     # goethite ochre: brand primary
RED = "#9c2f1c"       # hematite red: stone red
WINE = "#5e1f26"      # wine red: dark surface
BLUE = "#2f9fb4"      # Maya blue: accent, north
CRIMSON = "#b0173f"   # cochineal crimson: UI only


# ---------------------------------------------------------------- geometry
def stroke_cells(pts, w=2):
    """Cells covered by a polyline of width w along integer centre-lines."""
    h = w // 2
    cells = set()
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        xa, xb = sorted((x0, x1))
        ya, yb = sorted((y0, y1))
        for x in range(xa - h, xb + h):
            for y in range(ya - h, yb + h):
                cells.add((x, y))
    return cells


def rect_cells(x, y, w, h):
    return {(i, j) for i in range(x, x + w) for j in range(y, y + h)}


def trace(cells):
    """Trace a cell set into closed loops (clockwise outer, holes reversed)."""
    edges = {}
    for (x, y) in cells:
        for a, b, n in (((x, y), (x + 1, y), (x, y - 1)),
                        ((x + 1, y), (x + 1, y + 1), (x + 1, y)),
                        ((x + 1, y + 1), (x, y + 1), (x, y + 1)),
                        ((x, y + 1), (x, y), (x - 1, y))):
            if n not in cells:
                edges.setdefault(a, []).append(b)
    loops = []
    while edges:
        start = next(iter(edges))
        loop = [start]
        cur = start
        prev_dir = None
        while True:
            outs = edges[cur]
            if len(outs) > 1 and prev_dir is not None:
                # at a pinch point turn right first, keeping loops simple
                def turn(b):
                    d = (b[0] - cur[0], b[1] - cur[1])
                    cross = prev_dir[0] * d[1] - prev_dir[1] * d[0]
                    return -cross
                outs.sort(key=turn)
            nxt = outs.pop(0)
            if not outs:
                del edges[cur]
            prev_dir = (nxt[0] - cur[0], nxt[1] - cur[1])
            cur = nxt
            if cur == start:
                break
            loop.append(cur)
        # drop collinear points
        simple = []
        n = len(loop)
        for i, p in enumerate(loop):
            a, c = loop[i - 1], loop[(i + 1) % n]
            if (p[0] - a[0]) * (c[1] - p[1]) != (p[1] - a[1]) * (c[0] - p[0]):
                simple.append(p)
        loops.append(simple)
    return loops


def path_d(cells, ox=0, oy=0, s=1):
    out = []
    for loop in trace(cells):
        pts = " ".join(f"{fmt((x + ox) * s)},{fmt((y + oy) * s)}" for x, y in loop)
        out.append(f"M{pts}Z")
    return "".join(out)


def fmt(v):
    return f"{v:g}"


def shift(cells, dx, dy=0):
    return {(x + dx, y + dy) for x, y in cells}


def mirror(cells, width):
    return {(width - 1 - x, y) for x, y in cells}


# --------------------------------------------------------------- the mark
def m_half_full():
    """One xicalcoliuhqui unit: a hooked spiral foot, a stem, then a stepped
    line descending to the centre (20-unit cell, left half). Stroke and
    every gap are 2 units."""
    return stroke_cells([(5, 15), (5, 13), (8, 13), (8, 19), (1, 19), (1, 1),
                         (5, 1), (5, 5), (9, 5), (9, 9), (10, 9)])


def m_full():
    h = m_half_full()
    return h | mirror(h, 20)


def m_small():
    """16 px variant: stepped M only, no hook; 8 x 8 cells, stroke 2 px."""
    cells = rect_cells(0, 0, 2, 8) | rect_cells(6, 0, 2, 8)
    cells |= rect_cells(2, 1, 1, 2) | rect_cells(5, 1, 1, 2)
    cells |= rect_cells(2, 0, 1, 1) | rect_cells(5, 0, 1, 1)
    cells |= rect_cells(3, 2, 2, 2)
    return cells


def frame_square(size, edge):
    """Square plaque: soot frame, blue bands top and bottom, ochre field."""
    e = edge
    s = size
    b = e * 2.2
    return (
        f'<rect x="0" y="0" width="{fmt(s)}" height="{fmt(s)}" fill="{SOOT}"/>'
        f'<rect x="{fmt(e)}" y="{fmt(e)}" width="{fmt(s-2*e)}" height="{fmt(s-2*e)}" fill="{OCHRE}"/>'
        f'<rect x="{fmt(e)}" y="{fmt(e)}" width="{fmt(s-2*e)}" height="{fmt(b)}" fill="{BLUE}"/>'
        f'<rect x="{fmt(e)}" y="{fmt(e+b)}" width="{fmt(s-2*e)}" height="{fmt(e*0.6)}" fill="{SOOT}"/>'
        f'<rect x="{fmt(e)}" y="{fmt(s-e-b)}" width="{fmt(s-2*e)}" height="{fmt(b)}" fill="{BLUE}"/>'
        f'<rect x="{fmt(e)}" y="{fmt(s-e-b-e*0.6)}" width="{fmt(s-2*e)}" height="{fmt(e*0.6)}" fill="{SOOT}"/>'
    )


def outlined(cells, fill, ox, oy, s, ow):
    """Painted-book figure: flat fill inside a soot outline of width ow."""
    d = path_d(cells, ox, oy, s)
    return (f'<path d="{d}" fill="{fill}" stroke="{SOOT}" stroke-width="{fmt(ow)}" '
            f'stroke-linejoin="miter" paint-order="stroke"/>')


def mark_svg(size=240):
    return svg(size, size, mark_svg_inner(size), "Montezuma's symbol: step-fret M")


def mark_small_svg():
    body = (f'<rect width="16" height="16" fill="{SOOT}"/>'
            f'<rect x="1" y="1" width="14" height="14" fill="{OCHRE}"/>'
            f'<rect x="1" y="1" width="14" height="2" fill="{BLUE}"/>'
            f'<rect x="1" y="13" width="14" height="2" fill="{BLUE}"/>'
            f'<path d="{path_d(m_small(), 4, 4)}" fill="{SOOT}"/>')
    return svg(16, 16, body, "Montezuma's symbol, 16 px variant",
               extra='shape-rendering="crispEdges"')


# ------------------------------------------------------------ the wordmark
def letters():
    L = {}
    L["M"] = (12, stroke_cells([(1, 9), (1, 1), (3, 1), (3, 3), (5, 3), (5, 5), (7, 5),
                                 (7, 3), (9, 3), (9, 1), (11, 1), (11, 9)]))
    o = stroke_cells([(1, 1), (9, 1), (9, 9), (1, 9), (1, 1)])
    o -= {(0, 0), (9, 0), (0, 9), (9, 9)}
    L["O"] = (10, o)
    L["N"] = (10, stroke_cells([(1, 9), (1, 1), (3, 1), (3, 3), (5, 3), (5, 5), (7, 5),
                                 (7, 7), (9, 7)]) | stroke_cells([(9, 1), (9, 9)]))
    L["T"] = (10, stroke_cells([(1, 1), (9, 1)]) | stroke_cells([(5, 1), (5, 9)]))
    L["E"] = (8, stroke_cells([(7, 1), (1, 1), (1, 9), (7, 9)]) | stroke_cells([(1, 5), (6, 5)]))
    L["Z"] = (10, stroke_cells([(1, 1), (9, 1), (9, 3), (7, 3), (7, 5), (5, 5), (5, 7),
                                 (3, 7), (3, 9)]) | stroke_cells([(1, 7), (1, 9), (9, 9)]))
    u = stroke_cells([(1, 1), (1, 9), (9, 9), (9, 1)])
    u -= {(0, 9), (9, 9)}
    L["U"] = (10, u)
    L["A"] = (10, stroke_cells([(1, 9), (1, 3), (3, 3), (3, 1), (7, 1), (7, 3), (9, 3), (9, 9)])
              | stroke_cells([(1, 6), (9, 6)]))
    L["'"] = (2, stroke_cells([(1, 0), (1, 4)]) - {(0, 3)})
    s = stroke_cells([(9, 1), (1, 1), (1, 5), (9, 5), (9, 9), (1, 9)])
    s -= {(0, 0), (9, 9)}
    L["S"] = (10, s)
    return L


def word_cells(text="MONTEZUMA'S", gap=2):
    L = letters()
    cells, x = set(), 0
    for ch in text:
        w, c = L[ch]
        cells |= shift(c, x)
        x += w + gap
    return cells, x - gap


def wordmark_svg(u=8, colour=SOOT, bands=True):
    cells, w = word_cells()
    pad = 2
    if bands:
        # wordmark sits between two colour bands, each with a soot rule
        H = 10 + 2 * 4
        body = (f'<rect x="0" y="0" width="{(w+2*pad)*u}" height="{2*u}" fill="{BLUE}"/>'
                f'<rect x="0" y="{2*u}" width="{(w+2*pad)*u}" height="{0.6*u:g}" fill="{SOOT}"/>'
                f'<rect x="0" y="{(H-2)*u}" width="{(w+2*pad)*u}" height="{2*u}" fill="{RED}"/>'
                f'<rect x="0" y="{(H-2.6)*u:g}" width="{(w+2*pad)*u}" height="{0.6*u:g}" fill="{SOOT}"/>')
        body += f'<path d="{path_d(cells, pad, 4, u)}" fill="{colour}"/>'
        return svg((w + 2 * pad) * u, H * u, body, "Montezuma's wordmark")
    body = f'<path d="{path_d(cells, 0, 0, u)}" fill="{colour}"/>'
    return svg(w * u, 10 * u, body, "Montezuma's wordmark")


# ------------------------------------------------------------------ band
def fret_unit():
    """Band fret, 16 x 12 cells: rises in steps from the ground line and
    ends in a squared hook spiral."""
    return stroke_cells([(1, 11), (1, 8), (4, 8), (4, 5), (7, 5), (7, 2),
                         (13, 2), (13, 9), (10, 9), (10, 6)])


def band_body(u, repeats, oy=0):
    """One band strip: blue / soot / fret row / soot / red; 18 units tall."""
    W = 16 * repeats * u
    y = oy
    parts = [
        f'<rect x="0" y="{fmt(y)}" width="{fmt(W)}" height="{fmt(2*u)}" fill="{BLUE}"/>',
        f'<rect x="0" y="{fmt(y+2*u)}" width="{fmt(W)}" height="{fmt(u)}" fill="{SOOT}"/>',
        f'<rect x="0" y="{fmt(y+3*u)}" width="{fmt(W)}" height="{fmt(12*u)}" fill="{OCHRE}"/>',
        f'<rect x="0" y="{fmt(y+15*u)}" width="{fmt(W)}" height="{fmt(u)}" fill="{SOOT}"/>',
        f'<rect x="0" y="{fmt(y+16*u)}" width="{fmt(W)}" height="{fmt(2*u)}" fill="{RED}"/>',
    ]
    cells = set()
    for i in range(repeats):
        cells |= shift(fret_unit(), 16 * i)
    parts.append(outlined(cells, LIME, 0, 3 + oy / u, u, u * 0.8))
    return "".join(parts)


def band_svg(u=4, repeats=1):
    return svg(16 * repeats * u, 18 * u, band_body(u, repeats),
               "Montezuma's band tile: step-fret row between colour bands")


# --------------------------------------------------------- lockup & icon
def lockup_svg():
    u = 8
    cells, w = word_cells()
    mark_size = 18 * u  # plaque height equals banded wordmark height
    gap = 3 * u
    ww = (w + 4) * u
    W = mark_size + gap + ww
    m = mark_svg_inner(mark_size)
    H = 18 * u
    body = m
    body += f'<g transform="translate({mark_size + gap},0)">'
    body += (f'<rect x="0" y="0" width="{ww}" height="{2*u}" fill="{BLUE}"/>'
             f'<rect x="0" y="{2*u}" width="{ww}" height="{0.6*u:g}" fill="{SOOT}"/>'
             f'<rect x="0" y="{16*u}" width="{ww}" height="{2*u}" fill="{RED}"/>'
             f'<rect x="0" y="{15.4*u:g}" width="{ww}" height="{0.6*u:g}" fill="{SOOT}"/>')
    body += f'<path d="{path_d(cells, 2, 4, u)}" fill="{SOOT}"/>'
    body += '</g>'
    return svg(W, H, body, "Montezuma's lockup")


def mark_svg_inner(size):
    u = size / 30  # 30-unit plaque; the M sits on 20 units, inset 5
    return frame_square(size, u) + outlined(m_full(), RED, 5, 5, u, u * 0.6)


def icon_svg():
    S = 1024
    u = 10
    body = f'<rect width="{S}" height="{S}" fill="{OCHRE}"/>'
    # band across top and bottom, full bleed (outside the 824 px safe zone
    # only the colour fields reach the edge)
    reps = S // (16 * u) + 1
    body += f'<g>{band_body(u, reps, 0)}</g>'
    body += (f'<g transform="translate(0,{S})"><g transform="scale(1,-1)">'
             f'{band_body(u, reps, 0)}</g></g>')
    # the M, large, centred
    mu = 30
    off = (S - 20 * mu) / 2 / mu
    body += outlined(m_full(), RED, off, off, mu, mu * 0.8)
    return svg(S, S, body, "Montezuma's app icon mock")


def svg(w, h, body, title, extra=""):
    return (HEAD + f'<svg xmlns="http://www.w3.org/2000/svg" width="{fmt(w)}" height="{fmt(h)}" '
            f'viewBox="0 0 {fmt(w)} {fmt(h)}" {extra}>'
            f'<title>{title}</title>{body}</svg>\n')


if __name__ == "__main__":
    (OUT / "mark.svg").write_text(mark_svg())
    (OUT / "mark-small.svg").write_text(mark_small_svg())
    (OUT / "wordmark.svg").write_text(wordmark_svg())
    (OUT / "band.svg").write_text(band_svg())
    (OUT / "lockup.svg").write_text(lockup_svg())
    (OUT / "app-icon.svg").write_text(icon_svg())

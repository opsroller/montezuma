#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Generate direction 3 ("Three treasures in a row") SVGs."""
import os, sys

OUT = sys.argv[1]
HDR = "<!-- SPDX-License-Identifier: GPL-2.0-or-later -->\n"

# palette
SOOT = "#1c1a17"
LIME = "#f4eedf"
OCHRE = "#e3a92b"     # reed-mat yellow (goethite ochre family)
OCHRE_D = "#b97c16"   # weave lines / shade field
HEMATITE = "#b0341f"  # Spondylus red sits in the hematite family
SHELL_LIP = "#f1d9c4"
GREEN = "#3e8a5e"     # burnished serpentine
GREEN_L = "#7fb98a"   # flat polish patch
GOLD = "#ecc85a"      # pale silver-rich gold
GOLD_L = "#f8e6a4"
BLUE = "#2a9db5"      # Maya-blue field

# ---------- wordmark glyphs on a 10-unit cap grid, stem 2 ----------
G = {
 "M": (9, ["M0 0H2.4L4.5 4.6L6.6 0H9V10H7V4.2L5.5 7.6H3.5L2 4.2V10H0Z"]),
 "O": (8, ["M2.5 0H5.5L8 2.5V7.5L5.5 10H2.5L0 7.5V2.5Z M3 2L2 3V7L3 8H5L6 7V3L5 2Z"]),
 "N": (8, ["M0 0H2.3L6 6.2V0H8V10H5.7L2 3.8V10H0Z"]),
 "T": (8, ["M0 0H8V2H5V10H3V2H0Z"]),
 "E": (7, ["M0 0H7V2H2V4H6V6H2V8H7V10H0Z"]),
 "Z": (8, ["M0 0H8V2L2.8 8H8V10H0V8L5.2 2H0Z"]),
 "U": (8, ["M0 0H2V7L3 8H5L6 7V0H8V7.5L5.5 10H2.5L0 7.5Z"]),
 "A": (8, ["M2.5 0H5.5L8 2.5V10H6V6.5H2V10H0V2.5Z M3 2L2 3V4.5H6V3L5 2Z"]),
 "'": (2, ["M0 0H2V2.6L1.2 4.4H0Z"]),
 "S": (8, ["M2.5 0H8V2H3L2 3V4H5.5L8 6.5V7.5L5.5 10H0V8H5L6 7V6H2.5L0 3.5V2.5Z"]),
}
TRACK = 1.4

def word(text, x0=0.0, y0=0.0, scale=1.0, fill=SOOT):
    parts, x = [], x0
    for i, ch in enumerate(text):
        w, ds = G[ch]
        for d in ds:
            parts.append(f'<path transform="translate({x:.2f} {y0:.2f}) scale({scale})" d="{d}" fill="{fill}" fill-rule="evenodd"/>')
        x += (w + TRACK) * scale
    return "\n".join(parts), x - x0 - TRACK * scale

NAME = "MONTEZUMA'S"

# ---------- the three treasures, drawn on a 0..300 x 0..140 box ----------
def treasures(o=6.0, detail=True):
    """o = outline width. Objects sit on a reed-mat strip."""
    s = []
    # reed mat strip (petlatl, DR-12: yellow, diagonal weave)
    my, mh = (92, 40) if detail else (96, 34)
    s.append(f'<rect x="8" y="{my}" width="284" height="{mh}" rx="3" fill="{OCHRE}"/>')
    if detail:
        s.append('<clipPath id="mat"><rect x="8" y="92" width="284" height="40" rx="3"/></clipPath>')
        lines = []
        for k in range(-60, 330, 20):
            lines.append(f'M{k} 132L{k+40} 92')
            lines.append(f'M{k} 92L{k+40} 132')
        s.append(f'<path clip-path="url(#mat)" d="{"".join(lines)}" stroke="{OCHRE_D}" stroke-width="{o*0.7:.1f}" fill="none"/>')
    s.append(f'<rect x="8" y="{my}" width="284" height="{mh}" rx="3" fill="none" stroke="{SOOT}" stroke-width="{o if detail else o*0.55:.1f}"/>')

    # 1. greenstone bead (MA-05): round, burnished, drilled
    s.append(f'<circle cx="58" cy="66" r="38" fill="{GREEN}" stroke="{SOOT}" stroke-width="{o}"/>')
    s.append(f'<path d="M34 56A28 28 0 0 1 52 36A38 38 0 0 0 40 62Z" fill="{GREEN_L}"/>')
    s.append(f'<circle cx="58" cy="66" r="{9 if detail else 11}" fill="{SOOT}"/>')

    # 2. red Spondylus shell piece (NU-13 facts): a polished valve with its hinge
    shell = "M150 28C176 28 194 48 192 74C190 90 180 100 168 102H132C120 100 110 90 108 74C106 48 124 28 150 28Z"
    s.append(f'<path d="{shell}" fill="{HEMATITE}" stroke="{SOOT}" stroke-width="{o}" stroke-linejoin="round"/>')
    if detail:
        s.append(f'<path d="M150 42V94M133 48L140 94M167 48L160 94" stroke="{SOOT}" stroke-width="{o*0.6:.1f}" stroke-linecap="round" fill="none"/>')
    s.append(f'<path d="M128 102L122 114H178L172 102Z" fill="{SHELL_LIP}" stroke="{SOOT}" stroke-width="{o*(0.8 if detail else 1):.1f}" stroke-linejoin="round"/>')

    # 3. cast gold bell (OB-06): Mesoamerican rattle bell -- loop, pear body, slit
    s.append(f'<path d="M232 40V30A8 8 0 0 1 248 30V40" fill="none" stroke="{SOOT}" stroke-width="{o*1.2:.1f}"/>')
    bell = "M240 38C222 38 210 56 207 74C204 94 220 106 240 106C260 106 276 94 273 74C270 56 258 38 240 38Z"
    s.append(f'<path d="{bell}" fill="{GOLD}" stroke="{SOOT}" stroke-width="{o}" stroke-linejoin="round"/>')
    s.append(f'<path d="M218 74C219 62 225 52 233 48C227 58 225 66 225 78Z" fill="{GOLD_L}"/>')
    s.append(f'<ellipse cx="240" cy="105" rx="{11 if detail else 13}" ry="{5 if detail else 7}" fill="{SOOT}"/>')
    return "\n".join(s)

def svg(w, h, body, title, vb=None):
    vb = vb or f"0 0 {w} {h}"
    return (HDR + f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{vb}" width="{w}" height="{h}" role="img" aria-label="{title}">\n'
            f'<title>{title}</title>\n{body}\n</svg>\n')

def write(name, text):
    with open(os.path.join(OUT, name), "w") as f:
        f.write(text)

# mark
write("mark.svg", svg(300, 140, treasures(), "Montezuma's symbol: three treasures on a reed mat"))

# wordmark (10-unit cap grid scaled x10)
wm, ww = word(NAME, 0, 0, 10)
write("wordmark.svg", svg(int(ww) + 1, 100, wm, "Montezuma's wordmark"))

# lockup: symbol above wordmark (stacked) -- width from wordmark
S = 10
wm2, ww2 = word(NAME, 40, 210, S)
W = int(ww2 + 80)
mark_scale = 1.3
mx = (W - 300 * mark_scale) / 2
lock = f'<g transform="translate({mx:.1f} 20) scale({mark_scale})">{treasures()}</g>\n{wm2}'
write("lockup.svg", svg(W, 340, lock, "Montezuma's lockup"))

# app icon 1024: Maya-blue field, lime-white inner band, treasures large
icon = [f'<rect width="1024" height="1024" fill="{BLUE}"/>',
        # repeated colour bands at the foot (essential 7)
        f'<rect y="852" width="1024" height="36" fill="{HEMATITE}"/>',
        f'<rect y="888" width="1024" height="36" fill="{LIME}"/>',
        f'<rect y="924" width="1024" height="100" fill="{SOOT}"/>',
        f'<g transform="translate(62 200) scale(3)">{treasures(o=7)}</g>']
write("app-icon.svg", svg(1024, 1024, "\n".join(icon), "Montezuma's app icon"))

# small-size variant: no weave, no ribs, fat outlines, objects enlarged
small = [f'<rect width="1024" height="1024" fill="{BLUE}"/>',
         f'<g transform="translate(2 264) scale(3.4)">{treasures(o=14, detail=False)}</g>']
write("mark-small.svg", svg(1024, 1024, "\n".join(small), "Montezuma's small-size icon"))

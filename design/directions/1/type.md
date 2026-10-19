<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Direction 1 type pairing: the twin temple

The wordmark is drawn, not typeset (see `wordmark.svg` and `build.py`):
cap height 12 units, stem 2.6, square-cut straight letters, round
letters with a 1.5-unit chamfer like a cut stone block. The fonts below
are for everything else and are chosen to sit beside it.

| Job | Family | Licence | Source | Why |
|---|---|---|---|---|
| Display: titles, level names, store and site headings | **Archivo** (variable, `wdth` 62 to 125, `wght` 100 to 900); use weight 800, width 125, uppercase, +2 % tracking | SIL Open Font License 1.1 | `github.com/google/fonts`, `ofl/archivo/` (`METADATA.pb` reads `license: "OFL"`), checked 2026-10-19 | Wide, square-shouldered grotesque that echoes the drawn wordmark's proportions without imitating it; Latin and Latin Extended cover Spanish, French, Portuguese, German and Nahuatl spellings |
| Text and UI: HUD, menus, body, captions | **Atkinson Hyperlegible Next** (variable, `wght` 200 to 800); 400 body, 700 labels and figures | SIL Open Font License 1.1 | `github.com/google/fonts`, `ofl/atkinsonhyperlegiblenext/` (`METADATA.pb` reads `license: "OFL"`), checked 2026-10-19 | Built for legibility at small sizes (16 px phone captions in the brief); clearly different letter shapes (I, l, 1; O, 0) and tabular figures for score and timer |

Rendered sample (scratch, not committed): Archivo 800/125 for
"LEVEL 12 · TENOCHTITLAN" and "MOTECUHZOMA · XICALCOLIUHQUI · SEÑOR ·
CHÂTEAU · ÜBER"; Atkinson Hyperlegible Next for French, Spanish,
Portuguese and German body text and tabular score figures. All
accents rendered; nothing fell back.

Licence notes for the provenance log: OFL 1.1 allows embedding in the
app and on the web, and use in logos; the fonts may not be sold on
their own, and a modified version may not use the reserved font name.
Ship each font with its `OFL.txt`. Neither font is used in the
wordmark, so the logo carries no font dependency.

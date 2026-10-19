<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# Direction 2 palette sketch: the step-fret band

Five stone pigment families for the mark, the band and every frame
(CO-01, CO-02, CO-03, CO-06), plus two painted-book colours kept to UI,
cloth and paper (CO-07). Flat, opaque, no gradients; a soot outline does
the separating (CO-02, CO-06, CO-07). Hex values are sRGB and are a
sketch for the chosen-direction step, not tokens yet.

| Swatch | Hex | Family and source | Role |
|---|---|---|---|
| Soot | `#1c1714` | Pine-soot black (CO-03 p. 22, CO-06) | Outline, rules between bands, text on light |
| Lime white | `#f5efe1` | Calcite or lime white (CO-03 p. 20) | Light surface, fret fill, text on dark |
| Ochre | `#e0a030` | Goethite ochre, the dominant family (CO-03 p. 19, CO-06) | Brand primary: plaque field, fret ground, display text on dark |
| Hematite red | `#9c2f1c` | Hematite red (CO-03 p. 18) | The M, lower band, headings on light |
| Maya blue | `#2f9fb4` | Maya-blue turquoise (CO-03 p. 22, CO-04, CO-05) | Upper band, accent, focus on dark |
| Wine red | `#5e1f26` | Hematite with titanomagnetite (CO-06) | Dark panel surface, pressed states |
| Cochineal crimson | `#b0173f` | Codex red, not stone (CO-07) | UI-only accent: danger, alerts; never on stone |
| Night | `#16110d` | Soot-based neutral | Game UI and dark site background |

Ramp step for blue text on light: `#1d6f80` (5.04:1 on lime white).

The blue-over-red band order borrows the blue-north, ochre-south reading
(CO-01 p. 103, CO-09) only loosely: blue sits above (sky, water), red
below (earth). It is not a copy of any surviving band sequence; the one
recorded sequence (Casa de las Águilas, CO-03 p. 32) is held out (NU-17).

## Contrast (WCAG 2.2, computed)

| Pair | Ratio | Use |
|---|---|---|
| Soot on lime white | 15.50 | Body text, light |
| Lime white on night | 16.35 | Body text, dark |
| Ochre on night | 8.25 | Display text and HUD numbers, dark |
| Soot on ochre | 7.82 | Text on brand fields, mark outline |
| Hematite red on lime white | 6.46 | Headings, light |
| Cochineal on lime white | 6.02 | Danger text, light |
| Blue on night | 6.02 | Links and focus, dark |
| Soot on Maya blue | 5.70 | Text on the blue band |
| Ochre on wine | 5.44 | Text on dark panels |
| Hematite red on ochre | 3.26 | The M against its field (graphic, 3:1 met; the soot outline adds 7.82) |
| Hematite red or cochineal on night | 2.7 to 2.8 | Fails: never as text or as the only boundary on dark; give red shapes on dark a lime-white or ochre edge |

## Piece colours

The seven HUD swatches in `design/tokens/temple-board.json` are not
changed. The brand palette sits round them: ochre is close to the yellow
piece (`#ffcc4d`), so HUD frames use the band, not plain ochre, beside
quota icons.

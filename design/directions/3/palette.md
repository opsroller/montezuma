<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Direction 3 palette sketch

Five stone pigment families (CO-01, CO-02, CO-03, CO-06) carry the mark and
frames; the painted-book palette (CO-07) adds the material colours of the
three treasures and a cochineal crimson for UI only. All fills are flat;
every shape sits inside a soot outline, which carries the edge where two
fills are close in lightness.

| Role | Hex | Family / source | Use |
|---|---|---|---|
| Ink (outline, text) | `#1c1a17` | soot black | outlines, wordmark, body text on light |
| Paper (surface light) | `#f4eedf` | lime white | site background, light panels, text on dark |
| Field (brand) | `#2a9db5` | Maya-blue turquoise | app-icon field, header band |
| Field deep | `#1d6f82` | Maya blue, darker step | links and buttons on paper |
| Mat (accent) | `#e3a92b` | goethite ochre; reed mat DR-12 | the mat strip, highlights, selection |
| Shell red | `#b0341f` | hematite red (stone) | Spondylus piece, frame bands |
| Greenstone | `#3e8a5e` | burnished serpentine (MA-05) | bead; success role candidate |
| Gold | `#ecc85a` | pale silver-rich gold (OB-06, NU-10 facts) | bell, focus ring on dark |
| UI crimson | `#a3173c` | cochineal (CO-07), paper and cloth only | danger/alert, never on stone |

Flat polish patches (`#7fb98a` on greenstone, `#f8e6a4` on gold) and the
weave line (`#b97c16`) are shade steps of the above, not new hues.

## Contrast (WCAG 2.2, computed)

| Pair | Ratio | Verdict |
|---|---|---|
| Ink on paper | 15.0 | text AA/AAA |
| Hematite red on paper | 5.4 | text AA |
| UI crimson on paper | 6.6 | text AA |
| Field deep on paper | 4.97 | text AA |
| Ink on field (blue) | 5.45 | text AA; outline vs field passes 3:1 |
| Ochre on ink | 8.25 | text AA (dark UI) |
| Gold on ink | 10.7 | text AA, focus ring on dark UI |
| Paper on field | 2.75 | fails for text: no white text on the blue field |
| Greenstone / red / gold on field | 1.3 to 2.0 | fill-to-field fails 3:1; the soot outline (5.45) carries the boundary, so the outline is mandatory |

Colour-independence: the three treasures differ by silhouette (round
drilled bead, ribbed fan, looped bell), so the mark reads in greyscale.
Hematite red on soot (2.8) is low; never put red objects on black without
a lime-white keyline.

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Direction 1 palette sketch: the twin temple

Six stone colours, one per pigment family in the
[Mexica art reference](../../../docs/reference/history-mexica-art.md#artist-brief)
(essential 5: CO-01, CO-02, CO-03, CO-06), plus two painted-book colours
kept to UI and paper (CO-07, DR-12). Paint is flat, inside a soot
outline, with no gradient (CO-02, CO-06, CO-07). Values are sRGB, chosen
for this sketch, not measured pigment colourimetry (the reference lists
exact Tlaloc blue as a gap).

| Token (sketch) | Hex | Family | Role |
|---|---|---|---|
| `soot` | `#1e1916` | soot / char black | Outline, text on light, dark UI surface |
| `lime` | `#f4ecdc` | lime / calcite white | Light surface, icon field, stairs and roof slabs, text on dark |
| `hematite` | `#b23a2a` | hematite red | Brand primary: the temple platform; red text on light |
| `wine` | `#6a2224` | dark wine red | Deep accent, pressed states, dark panels |
| `ochre` | `#e09a26` | goethite ochre | South shrine (Huitzilopochtli); HUD figures and highlights on dark |
| `maya` | `#2a9fb4` | Maya-blue turquoise | North shrine (Tlaloc); links and focus on dark |
| `maya-deep` | `#1c6f80` | Maya blue, darker step | Links and focus on light |
| `cochineal` | `#a8174a` | codex cochineal crimson (UI and cloth only) | Danger and alert states, never on stone |

The north-blue, south-ochre pair is the identity's signature: blue always
left, ochre always right, as a viewer facing east up the stairs sees them
(CO-01 p. 103, CO-09, AR-10). Keep hematite (stone) and cochineal
(cloth, UI) apart, as the reference asks.

## Contrast (WCAG 2.2, computed from the hex values)

| Pair | Ratio | Use allowed |
|---|---|---|
| `lime` on `soot` | 14.8:1 | All text |
| `ochre` on `soot` | 7.3:1 | All text (HUD numbers) |
| `maya` on `soot` | 5.6:1 | All text, focus ring |
| `wine` on `lime` | 9.6:1 | All text |
| `cochineal` on `lime` | 6.2:1 | All text |
| `hematite` on `lime` | 5.1:1 | All text |
| `maya-deep` on `lime` | 4.9:1 | All text, focus ring |
| `hematite` on `soot` | 2.9:1 | Fills only, always with outline; not text, not a lone icon |
| `maya` on `lime` | 2.7:1 | Fills inside a soot outline only |
| `ochre` on `lime` | 2.0:1 | Fills inside a soot outline only |
| `maya` against `ochre` | 1.3:1 | Never the only cue: the shrines also differ by position |

Under protanopia and deuteranopia simulation the shrines read
grey-blue and olive-yellow; under tritanopia teal and salmon; in
greyscale both are mid grey and only position and outline tell them
apart, which is allowed because the colour carries no game state.

## Dark and light

Game UI (dark over the 3D board): `soot` surface, `lime` text, `ochre`
figures, `maya` focus. Site light scheme: `lime` surface, `soot` text,
`hematite` or `maya-deep` for accents and links. The seven piece
swatches in `design/tokens/temple-board.json` are untouched.

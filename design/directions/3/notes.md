<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Direction 3: Three treasures in a row

**Idea.** The identity says what the game is: three small precious things
in a row, the moment a match is made. A burnished greenstone bead, a red
Spondylus shell piece and a cast gold bell sit on a yellow reed mat, drawn
the painted-book way: black outline, flat colour, no gradients, no facets.

## Files

`mark.svg` (symbol), `mark-small.svg` (16 to 32 px variant: no weave, no
ribs, heavier outlines, objects enlarged), `wordmark.svg` (drawn on a grid,
no font), `lockup.svg` and `lockup.png` (stacked), `app-icon.svg` with
`icon-1024.png` and `icon-48.png`; `icon-16.png` is rendered from
`mark-small.svg`. `palette.md`, `type.md`. Everything regenerates with
`render.sh` (runs `build.py`, then headless Chromium).

## Sources

- MA-05: greenstone is ground and burnished, round beads, drilled (the
  bead, with its perforation and one flat polish patch); essential 10.
- NU-13 facts: red Spondylus set beside turquoise in Mexica mosaics (the
  shell piece; red kept in the hematite family). Facts only; no NU-13
  object is drawn or traced.
- OB-06: a 2.5 cm cast gold bell (the bell: loop, closed body, slit),
  drawn pale yellow per NU-10 facts and essential 12. Not the head-shaped
  form: no face.
- DR-12: reed mats (petlatl) drawn yellow with a diagonal weave (the mat).
- CO-07: painted-book method and the richer UI palette; CO-01, CO-02,
  CO-03, CO-06: the five stone families; essential 7: the colour bands at
  the foot of the icon.
- Essential 9: precious materials are small and personal, so they are
  shown as jewels on a mat, never as architecture.

## Strengths

- Says "match three" at a glance, in any language, and needs no Mexica
  imagery that is held back (no deity, glyph, Sun Stone, eagle, skull).
- Every object is attested and on the piece-material options table, so the
  identity and the board can share materials if the technical artist
  adopts them.
- Three silhouettes (circle, fan, bell) read apart in greyscale and under
  colour-vision deficiency.
- Recognisable at 16 px as three coloured beads on a yellow bar; at 48 px
  all three objects read.
- Warm and welcoming; breaks the category cliche of a single faceted gem.

## Weaknesses

- Wide (about 2:1). It does not fill a square icon or an adaptive-icon
  safe circle; the icon has empty field above and below, and Android's
  mask may clip the mat ends. A square stacked variant (bead over shell
  and bell) would be needed.
- The fills are close in lightness to the blue field (1.3 to 2.0); the soot
  outline does all the separating, and at 16 px it is under a pixel.
- Three objects make the mark busier than a single symbol; the full mark
  needs the small variant below about 64 px.
- Commits the brand to three of the seven piece materials before the
  technical artist has decided them; if the pieces stay ruby, gold disc and
  jade, the mark and board disagree.
- The drawn wordmark is legible but plain and slightly digital; its cut
  corners need more craft (curved S, M with a deeper vertex) to feel
  painted rather than pixel.
- A shell with ribs drifts towards a generic scallop (fuel-brand
  association); the Spondylus reads only with its hinge lip.

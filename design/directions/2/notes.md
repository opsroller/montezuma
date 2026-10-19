<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# Direction 2: the step-fret band

**Idea.** The identity is a pattern, not a picture: a step-fret
(xicalcoliuhqui, a stepped line ending in a squared hooked spiral) run
between horizontal bands of the five stone pigment families, which frames
the wordmark, UI panels, store art and the site header. The symbol is an
M made of two mirrored step-fret units (spiral foot, stem, stepped descent
to the centre) on an ochre plaque between blue and red bands.

## Files

| File | What |
|---|---|
| `mark.svg` | Symbol: 30-unit plaque, M on a 20-unit grid, stroke and gaps 2 units |
| `mark-small.svg` | 16 px variant: 8 x 8 px stepped M in soot, no spirals, pixel-snapped |
| `wordmark.svg` | MONTEZUMA'S drawn on a 10-unit cap grid, stroke 2, stepped diagonals, between a blue and a red band |
| `lockup.svg`, `lockup.png` | Plaque + banded wordmark, horizontal |
| `band.svg` | Repeatable tile, 16 x 18 units: blue, soot rule, fret row (lime fret, soot outline, ochre ground), soot rule, red |
| `app-icon.svg`, `icon-1024.png`, `icon-48.png` | Ochre field, band top and bottom (bottom mirrored), red M centred in the safe zone |
| `icon-16.png` | Rendered from `mark-small.svg`: the full icon does not survive 16 px |
| `palette.md`, `type.md` | Palette sketch with contrast; OFL type pairing |

PNG renders: headless Chromium 155, `--window-size` at the target size.

## Sources

- DR-10: step-fret on Mexica feather regalia (the motif itself).
- CO-01, CO-03 (and CO-02, CO-06): five stone pigment families, flat
  opaque paint; repeated horizontal colour bands for frames (inferred, as
  the reference grades it).
- IC-01: sky and earth bands framing a frieze, as a model for border
  bands (bands only; nothing of the captive scenes or glyphs).
- CO-07: painted-book method (black outline, flat fill) and the codex
  crimson kept to UI.
- CO-01 p. 103, CO-09: blue above, warm below, read loosely.

Not used: deity faces, glyphs or anything glyph-like, skulls, Sun Stone,
eagle on a nopal, jungle. The step-fret is drawn as plain square geometry
on a grid; no surviving piece's fret was traced or measured.

## Strengths

- One system everywhere: the band is the frame for HUD panels, store
  capsules and the site header, so the brand reads even where the mark
  does not appear.
- Strongest source footing of the patterns available: the step-fret is
  attested (DR-10) and the reference names bands and frets as the frame
  motif for UI.
- Pure grid geometry: tiles losslessly, renders crisp at integer scales,
  cheap to build in Godot as a StyleBox texture or 9-slice and in CSS as a
  repeating background.
- Flat fills and a soot outline meet the painted-book rule with no
  gradients; contrast pairs pass AA (see `palette.md`).

## Weaknesses

- The full mark needs a separate 16 px variant; the spirals vanish below
  about 32 px, and at 48 px the fret rows in the icon become texture.
- The step-fret is a common pan-Mesoamerican and neo-Aztec souvenir motif;
  without the bands and palette it can read as generic or as the pastiche
  the brief rules out. The Greek-key meander is a near neighbour.
- The squared, pixel-grid wordmark can read as retro arcade rather than
  crafted; it needs a careful letter-spacing and stroke pass if chosen.
- The two spiral feet under a T-shaped centre can, for some viewers,
  suggest a face; a review against the "no deity faces" rule is needed.
- Ochre is close to the yellow piece colour (`#ffcc4d`) on the HUD.
- Bands eat height: on the 184 x 69 Steam small capsule the band must
  drop to plain colour rules.

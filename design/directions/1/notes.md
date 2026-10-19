<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# Direction 1: the twin temple

**Idea.** The symbol is the twin-shrine temple as the AR-10 drawing records it, reduced to one stepped platform
carrying two stairs and two box shrines, painted the way the sources say
it was coded: Tlaloc's shrine Maya-blue on the north, Huitzilopochtli's
ochre on the south. It is drawn by the painted-book method (heavy soot
outline, flat fills, no gradient or bevel), so the one mark says "this
city, this temple" and cannot be mistaken for a Maya single-stair pyramid
or a jungle ruin.

## Construction

- Grid: 64 units. Outline 2.5. Platform three tiers (54, 46, 38 wide,
  each 4 narrower per side). Stairs 8 wide, three thin treads, centred
  under the doorways. Shrines 15 x 15, 8 apart, each under a lime roof
  slab 1.5 wider per side; doorways 5 x 9.
- Orientation: the viewer stands in the plaza facing east, up the west
  stairs (AR-10), so north (Tlaloc, blue) is on the left and south
  (Huitzilopochtli, ochre) on the right. Always this way round.
- Small size: `mark-small.svg` is drawn pixel by pixel on a 16-unit grid
  (1 unit is 1 px at 16 px and 3 px at 48 px): two tiers, two 2 px stairs
  running into 2 px doorways, no treads or roof slabs. `icon-16.png` and
  `icon-48.png` are rendered from it.
- One colour: `mark-mono.svg`, soot outline and shrines with lime
  platform, stairs, roofs and doorways.
- Wordmark: drawn on a 12-unit cap grid, stem 2.6, tracking 1.6; round
  letters chamfered 1.5 like cut stone, M and N with true diagonals. No
  font is involved. The lockup sets the wordmark at 2x (cap 24) beside
  the 64-unit mark, centred on the platform.
- App icon: 1024 lime field, mark at 78 percent, full bleed for the
  platform masks to cut.

## Sources it rests on

- AR-10: Florentine Codex XI f. 240v, one platform, two parallel
  west-facing stairs, two shrines (a later witness, labelled so).
- CO-01 (p. 103) and CO-09: blue north for Tlaloc, ochre south for
  Huitzilopochtli at Tenochtitlan.
- AR-08 and the "Top shrines" gap row: box-like shrines with doorways;
  the flat roof slab is one side of a contested question.
- CO-01, CO-02, CO-03, CO-06: the five stone pigment families and flat,
  hard-edged, unshaded paint; CO-03 to CO-05 for Maya blue.
- AR-06: lime stucco, used for the stairs and roof slabs.
- CO-07 and DR-12: black outline and flat fill for graphics, codex
  colours (cochineal) for UI only.
- NU-15 fact: about 46 m high; the tier count is a graphic reduction,
  not a Stage VII elevation.

## Strengths

- Specific to the place: twin stairs and twin shrines are the Templo
  Mayor's own silhouette, not a generic "Aztec" or Maya pyramid, and
  rest directly on the reference's essential 2 and Do list.
- The blue/ochre pair is a sourced meaning, not decoration, and can run
  through the whole system (board frame, menus, site) as north/south.
- Pure rectangles and outlines: no faces, glyphs, skulls, Sun Stone or
  eagle; nothing that needs a heritage decision.
- Holds at 16 px as a pixel-drawn variant: two coloured boxes on a red
  stepped base with two white stairs.
- Survives protan, deutan and tritan simulation; the two shrines are
  told apart by position as well as colour.
- Wordmark and mark share the same rectangular, cut-stone geometry; no
  font licence sits inside the logo.

## Weaknesses

- Says "temple", not "match-3": nothing hints at gems or play. A store
  icon may need a piece added, which would crowd it.
- Rectilinear stepped forms can read as a generic building, castle or
  pixel art at mid sizes (24 to 48 px); the full mark is too busy below
  about 64 px and needs the separate small variant.
- Maya blue and ochre are near-equal in lightness (1.3:1), so in
  greyscale the colour coding disappears.
- Lime field is pale on light home screens and wallpapers.
- The Templo Mayor is a national monument; the reference lists Mexican
  law on reproducing monuments as unverified. A stylised reduction is
  probably fine but should be flagged to the project manager.
- Stage VII elevations and shrine roofs are inferred or contested; the
  mark commits to flat roofs and three tiers.
- No store or trademark conflict search was run for this sketch.

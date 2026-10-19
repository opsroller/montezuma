<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# About the visual identity

*Explanation. Why Montezuma's looks the way it does outside the 3D
board: why the twin temple is the mark, what it rests on, why the
identity is now led by realistic rendered art, what was rejected and
what is still open. The rules themselves are in the
[style guide](../../design/README.md); the steps to apply them are in
[Use the identity](../how-to/use-the-identity.md); the brief they answer
is [`design/identity/brief.md`](../../design/identity/brief.md).*

## The problem

The game needed one look that works from a 16 px favicon to a title
screen, in a dark game UI and on a light website, and that says
"Tenochtitlan in the reign of Motecuhzoma II" without falling into the
three traps the brief names: jungle adventure, grim imagery and
neo-Aztec pastiche. Since 2026-10-19 sourced history leads the look, so
the identity also had to rest on the
[Mexica art reference](../reference/history-mexica-art.md) rather than on
the remembered original.

## Why the twin temple

Three directions were explored (kept as a record in
[`design/directions/`](../../design/directions/README.md)). The twin
temple won because it is specific to the place. One stepped platform
with two parallel stairs to two shrines is the Templo Mayor's own
silhouette, not a generic "Aztec" pyramid and not a Maya single-stair
temple, and it needs nothing the reference holds back: no deity, glyph,
skull, Sun Stone or eagle. It is built from rectangles and outlines,
which is also how the painted books draw, so mark, wordmark, band and UI
can share one geometry.

What it rests on, by item id in the reference:

- **AR-10**, Florentine Codex Book XI f. 240v (1577): a teocalli with
  two shrines on a stepped base. It is a later witness, made under
  Franciscan direction, so the mark is a graphic reduction of a
  convention, not a Stage VII elevation. The tier count and flat roofs
  are choices; the reference grades the roofs as contested.
- **CO-01** (p. 103) and **CO-09**: at Tenochtitlan the north, Tlaloc
  side is coded blue and the south, Huitzilopochtli side ochre. This is
  the one piece of meaning the colours carry.
- **CO-01, CO-02, CO-03, CO-06**: the five stone pigment families and
  flat, hard-edged, unshaded paint, which give the anchors (soot, lime,
  hematite, ochre, Maya blue) and the no-gradient rule.
- **CO-07** and **DR-12**: black outline and flat fill for graphics, and
  the richer codex colours (cochineal, greens) kept to UI and cloth,
  never to stone.
- **AR-06**: lime stucco, which is why stairs and roof slabs are lime.
- **DR-10**: the step-fret on feather regalia, for the band.
- **NU-08 facts**: blue for the north and Tlaloc side is a fact the
  reference allows; no Tlaloc imagery is used.

## Blue north on the left, ochre south on the right

The temple faces west, and its stairs climb from the west plaza. A
viewer standing in the plaza and looking up the stairs faces east, so
north is on the left and south on the right. The mark always takes that
viewpoint: Tlaloc's blue shrine left, Huitzilopochtli's ochre shrine
right. Mirroring it would put the gods on the wrong sides, which is why
mirroring is the first item on the misuse page.

The two colours are close in lightness (1.18 : 1), so in greyscale, in
one colour and for most colour-vision deficiencies the shrines look
alike. Rather than darken one and break the pigment logic, the identity
lets position and the doorways carry the pair: left is always north,
and each shrine keeps its doorway in every version, so it always reads
as a building. The same rule stops the UI from ever using blue against
ochre as its only signal.

## The refinements in the final round

The exploration's twin temple had four problems, each fixed here:

- **The wordmark read as arcade type.** All three explored wordmarks
  were square, even-stroked letters on a coarse grid. The new one is
  wider, has thicker stems than bars, cuts every square corner the same
  way like dressed stone, lets O, S and U overshoot, and has a real
  apostrophe. It is still drawn, not typed, so no font licence sits in
  the logo.
- **The icon field was pale lime**, which vanished on light wallpapers.
  It is now deep Maya blue, which holds on light screens and stays a
  coloured tile on black. The keylined temple sits above the band with
  about 34 px of field between them, so nothing sits on the band. (This
  is now the flat fallback icon; the master is the 3D icon described
  below.)
- **The one-colour mark lost its doorways.** Doorways are now cut into
  the shrine shapes, so the soot and lime one-colour versions keep them
  as knockouts, along with the stairs and roofs.
- **The UI ochre sat near the yellow piece.** It is now a deeper ochre
  `#d8901e`, at least CIEDE2000 15 from both the yellow and the orange
  pieces, checked by script.

## From the painted book to rendered art

The first identity drew everything the painted-book way: a soot outline
and flat colour inside it, no gradients or bevels. It was correct to its
sources and worked at every size, but beside the game it pulled the
wrong way. The board is a lit 3D scene, and a flat mark on the title
screen and the store page set a lower target than the game itself. The
design direction is that the identity leads the product's look upward:
the identity sets the realism bar, and the game follows it.

So the primary identity is now the realistic 3D key art in
[`design/keyart/`](../../design/keyart/): the same twin temple, built in
3D as lit, painted and aged stucco architecture, with the drawn wordmark
cast as a bevelled pale-gold solid. The applications follow: a rendered
app icon, glass UI panels over the key art, and a photographic site
hero.

This does not break with the sources. The codex convention of flat
colour inside a black outline (CO-07) is a fact about how painted books
were drawn, not a requirement for an interface. The buildings
themselves were solid, painted lime stucco under highland sun, and the
reference's facts about them (AR-06, the pigment families, blue north
and ochre south) hold in a render exactly as in a drawing. The same
facts set the render's limits: golden-hour light, honest wear on
painted stucco, a dry basin rather than jungle, and nothing from the
reference's Avoid list. The gold is the one material not taken from the
building: it is the title, set apart as metal, and it keeps to the
pale, silver-rich gold of the Basin rather than the reddish tumbaga the
reference warns against.

A render cannot do everything. At 16 to 47 px its light and texture
turn to noise, a one-colour job cannot hold shading, and an embossed or
engraved surface needs an outline. The flat marks stay for those: the
favicon and anything below 48 px, one-colour and knocked-out versions,
embossing, and the HUD, where small shapes sit over the moving
board. Because both are the same geometry in the same orientation and
colours, the flat mark reads as the small form of the rendered one,
not as a second logo. The palette, type, tokens and the step-fret band
(now a secondary ornament) serve both.

## What was folded in, and what was left

**Direction 2, the step-fret band**, was not chosen as the identity: as
a mark, its M of two mirrored frets could read as a face, the fret on
its own is a common souvenir motif close to the Greek key, and its
squared wordmark read as retro arcade. As a pattern it was the best
idea of the three: the step-fret is attested (DR-10) and the reference
names bands and frets as the frame motif for UI. So it is kept as the
**secondary band**: panel frames, the site header and the foot of the
icon and store art, where it carries the brand even when the mark is
absent.

**Direction 3, three treasures in a row**, said "match three" best but
was wide (about 2 : 1), filled a square icon badly, needed its own small
variant and committed the brand to three piece materials before the
technical artist had chosen them. Its idea is left for **store art**
(WP-38), where showing play is the point.

## The palette as a system

Each anchor is one pigment family, and each has an eleven-step ramp, so
UI and web can pick a passing pair without inventing colours. Roles
alias ramp steps, with a light and a dark value for each, so a theme
change is a remap. Hematite (stone) and cochineal (cloth and UI) are kept
apart, as the reference asks. A codex green exists only for success.
Every intended pair is checked by `design/identity/check_contrast.py`,
which reads the committed token files and fails the run on any
regression.

## Type

Archivo was chosen for display because its wide, square-shouldered
capitals at width 125 sit beside the drawn wordmark without imitating
it. Atkinson Hyperlegible Next was chosen for everything else because it
was designed for low-vision reading: distinct 1, l and I, 0 and O, and
tabular figures for score and timer, which matter at the 16 px phone
caption the brief sets. Both are OFL 1.1 from Google Fonts, so they may
ship in the game and on the site. Both cover the required European
accents and Nahuatl's usual spelling; Atkinson lacks ō, so macron
spellings stay out of UI text.

## Open risks

For the project manager. These are flagged, not ruled on.

- **The Templo Mayor is a national monument.** The mark is a stylised
  reduction of AR-10, not of the site, but it depicts that building. The
  reference lists Mexico's Ley Federal sobre Monumentos y Zonas
  Arqueológicos, Artísticos e Históricos and INAH authorisation for new
  art closely based on a monument as **unverified** (consultation item
  5), and holds the Templo Mayor's own fabric out until the site split
  is decided (NU-14, NU-15, consultation item 9). A decision, and legal
  advice if wanted, should be recorded before store submission.
- **Name conflicts.** "Montezuma's" is close to other marks in use:
  Montezuma's, a UK chocolate brand; *Montezuma's Revenge*, a 1984 video
  game; and Montezuma tequila. No trademark search was run; clearance is
  a legal question for the project manager before stores (WP-38).
- **Use of a ruler's name.** The title uses Motecuhzoma II's name in its
  colonial English form. This is the project's name, not the identity's
  choice, but the identity puts it on every surface.
- **Inferred details.** Stage VII's elevation, the shrine roofs and the
  facade colours are inferred or contested in the reference. If later
  sources settle them differently, the mark is a stylisation and need
  not change, but the explanation above should be updated.
- **The game's look must catch up.** The key art now sets a higher
  realism bar than the board. Until the game's own lighting and
  materials reach it, store art built from the key art promises more
  than the screenshots show; the store-art batch (WP-38) should pair the
  two honestly.
- **Glass values are not tokens yet.** The glass panel values live in
  the style guide and `design/identity/render.py`, not in the token
  files, so the game and site must copy them by hand until they are
  added.
- **Fonts not yet vendored.** Until the ui-developer and web developer
  add the fonts, the game and site fall back to system fonts and the
  type scale only approximates the design.

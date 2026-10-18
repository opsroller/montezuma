<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# About the board view

Explanation of why the 3D board is built the way it is. For anyone
changing the board view or the code that drives it. The facts (API,
constants, scenes) are in the [board view reference](../reference/board-view.md).

## The view shows; it never decides

The rules live in `core/` and run without Godot. The board view only
turns what the rules say happened into motion. It asks for a swap with
`swap_requested` and is handed the result to `play`; it never calls
`try_swap` itself. Something else owns the board: `BoardPlaytest` for free
play, the game scene ([About the game scene](about-the-game-scene.md)) for
real levels.

That split keeps three things true. The rules are tested without a
renderer. Speeding up, skipping or reducing animation can never change a
score or a cascade. And the same view serves the intro level on the web,
the full game and later rule variants under the "Montezuma's" umbrella,
since it knows nothing about any of them.

## Timing a move

A move arrives as a list of events the rules already resolved: the swap,
then for each cascade step the cells cleared, the pieces that fell and the
new pieces. The animator turns that list into a timed plan and nothing
else. It does not look at the board while planning, so the picture always
tells the story the rules reported, in their order.

Steps play one after another because that is how a player reads a chain:
a match pops, the gap is visible for an instant, pieces fall, and only
then does the next match pop. Inside a step everything happens together,
as in the rules. Falls use gravity rather than a fixed time, so a long
drop accelerates and a one-row drop stays quick; that is most of what
makes a cascade feel physical.

The plan and the walk that tracks which piece is in which cell are plain
C++ with no Godot types. Tests run them on boards where the answer is
known, and prove that speeding the animation up changes nothing but its
length. When the animation ends, or the player skips it, the view checks
itself against the board and resyncs, so a bug in presentation can never
turn into a wrong board on screen for more than one move.

## Two ways to swap

The original was played with a mouse: click one piece, click its
neighbour. Phones expect a swipe. The board takes both on every pointer
type, with no mode switch, because the two never conflict: a tap that does
not move is tap-tap, a press that travels is a swipe. The swipe fires the
moment it crosses the threshold rather than on release, which is what makes
it feel immediate, and the threshold scales with the cell on screen so it
feels the same on a phone and a monitor, with a floor in millimetres so a
shaky thumb on a dense screen still taps.

Presses during an animation are ignored on purpose. The pieces under the
finger are about to move, so a buffered tap would land on a piece the
player never aimed at. Releases and cancels are always handled, so nothing
stays half-pressed after a system gesture or switching apps.

The recogniser is plain C++ fed with positions and cells, so every rule
above is a unit test, not a manual check.

## A camera that fits, and nothing more

The original was a flat 2D board. A camera close to top-down (15° of
tilt) keeps that readability while showing the pieces' depth. The board
must stay whole on every screen we target, from a 9:21 phone held upright
to a 21:9 monitor. So the camera does one job: back off to the shortest
distance where the drawn board, frame included, and the pieces' tops fit
inside the viewport minus a margin, and redo that when the window or
board changes.

It does not follow, pan or shake during play. A match-3 board has no
moving target to follow, and a still camera means a tap always picks the
cell the player saw. Effects that want impact (WP-29) will shake the
pieces or add particles, not move the camera, and any such effect gets a
reduced-motion setting.

The fit uses a numeric search over the eight corners of the board's box
instead of a closed-form formula. With tilt, the near edge looks bigger
than the far one and piece height matters; the search handles every case
with one tested function, and it runs only on resize.

## Placeholders that already read

When a theme lacks a piece, or the manifest is missing, the pieces are
engine primitives. Each kind gets its own outline as seen from
above (circle, diamond, octagon, triangle, ring, pill, hexagon) as well as
its own colour, so kinds stay apart in grayscale and for colour-blind
players. Theme art keeps that rule (the temple's ruby, carnelian, sun disc,
jade, lapis, amethyst and moonstone each have their own shape). Alternating cell tiles on the slab
make rows and columns easy to count.

Piece nodes are pooled per kind, so a long cascade reuses nodes instead of
allocating new ones every move.

## Effects ride on events, markers ride on pieces

Totem and power-up effects (WP-29) follow the same rule as the move
animation: the session reports what happened, step by step, and the view
shows it. The game controller packs each step's report (power-ups going
off with the cells they clear, markers placed or lost, the totem that
fires) into the result it hands the view, so the view never asks the
rules what a TNT reaches or which pieces paint chose. A totem that clears
nothing still gets a step of its own, with a short lead-in, because the
player should see the time totem fire, not just a timer that jumps.

Markers (power-up badges, gems, steel, chains) are children of their
piece. A falling piece carries its badge without the view tracking it
separately, and a cleared piece takes its markers with it. The view
walks the same plan the animation plays and applies each marker change
at the moment the rules made it: at the step's clear, or after its
refill. The result also carries the markers the session ends with, so
the view can check its walk and resync, the same way it reconciles
pieces. `effects_tests` plays thousands of real session moves and checks
that the walk lands on the session's markers every time.

Timing lives in one planner (`effect_plan.hpp`, no Godot) that clamps
every transient effect to the step it belongs to. That is the visual
target's readability rule 3 turned into code: an effect can be as rich as
the theme likes, but it cannot sit over a cell after that cell's step is
over. The placeholders are deliberately plain (rings, flashes, labels,
small particle bursts that every renderer runs); the realistic versions
are theme scenes under the `fx_*` names, so the technical artist can
replace each one without touching code (WP-53).

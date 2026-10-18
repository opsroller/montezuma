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
play now, the game scene (WP-28) for real levels.

That split keeps three things true. The rules are tested without a
renderer. Speeding up, skipping or reducing animation can never change a
score or a cascade. And the same view serves the intro level on the web,
the full game and later rule variants under the "Montezuma's" umbrella,
since it knows nothing about any of them.

## A camera that fits, and nothing more

The original was a flat 2D board. A camera close to top-down (15° of
tilt) keeps that readability while showing the pieces' depth. The board
must stay whole on every screen we target, from a 9:21 phone held upright
to a 21:9 monitor. So the camera does one job: back off to the shortest
distance where the slab and the pieces' tops fit inside the viewport minus
a margin, and redo that when the window or board changes.

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

Until the art thread's models arrive through the asset manifest, the
pieces are engine primitives. Each kind gets its own outline as seen from
above (circle, diamond, octagon, triangle, ring, pill, hexagon) as well as
its own colour, so kinds stay apart in grayscale and for colour-blind
players. Final art must keep that rule. Alternating cell tiles on the slab
make rows and columns easy to count.

Piece nodes are pooled per kind, so a long cascade reuses nodes instead of
allocating new ones every move.

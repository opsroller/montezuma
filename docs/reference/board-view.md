<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Board view

Reference for the 3D board: its classes, scenes, layout, camera and feel
constants. For people changing or using the board view. Why it is built
this way: [About the board view](../explanation/about-the-board-view.md).

Code: `extension/src/board/`. Scenes: `game/scenes/board/`. Owner:
graphics-programmer (WP-12 to 14, 29).

![Board on a 16:9 desktop window](../images/board-view-desktop.png)

## Classes

All in namespace `montezuma::godot_binding`, registered by
`register_board()` (`extension/src/board/register_board.cpp`).

### `BoardView3D` (Node3D)

Draws a `Match3Board` and plays back results. It never applies a rule.

| Member | Kind | Meaning |
|---|---|---|
| `show_board(board: Match3Board)` | method | Draws the board as it is now, no animation. Call again after the board is re-dealt |
| `play(result: Dictionary)` | method | Plays a `Match3Board.try_swap` result, then emits `animation_finished`. Uses `result.swap` when present, otherwise the last requested pair |
| `play_illegal(a: Vector2i, b: Vector2i)` | method | Bounces the two pieces toward each other and back, then emits `animation_finished` |
| `is_busy()` | method | True while an animation plays; taps are ignored then |
| `cell_at_screen(pos: Vector2) -> Vector2i` | method | Cell under a screen position using the active camera; `(-1, -1)` off the board |
| `get_board_width()`, `get_board_height()` | method | Size of the board last shown |
| `cell_size` | property | World size of one cell, 0.25 to 4, default 1 |
| `swap_requested(a: Vector2i, b: Vector2i)` | signal | The player asked to swap two adjacent cells |
| `animation_finished` | signal | A `play` or `play_illegal` finished; the view now matches the board |
| `layout_changed` | signal | The board was (re)built; cameras refit |

Until WP-13 lands, `play` animates the swap, then every changed cell drops
in at once.

### `BoardCamera3D` (Camera3D)

Fixed camera that frames the whole board at any window size.

| Property | Default | Meaning |
|---|---|---|
| `board_view_path` | empty | The `BoardView3D` to frame |
| `tilt_degrees` | 15 | Pitch away from straight down, toward the player |
| `margin` | 0.08 | Fraction of the viewport kept empty on each side |

It refits on viewport `size_changed` and the view's `layout_changed`, and
otherwise never moves. Field of view is vertical, 40°.

### `BoardPlaytest` (Node3D)

Free play on one 8×8, 7-kind board with no level: answers
`swap_requested` with `try_swap`, then calls `play` or `play_illegal`.
Deals a new board after a `stuck` result. Properties: `board_view_path`,
`seed`. Read-only: `get_board()`, `get_score()`. The game scene
(`Match3Game`, WP-28) replaces it for real levels.

### `PieceLibrary` (C++ only)

Resolves visuals from the asset manifest and pools piece nodes per kind.

| Manifest name | Used for | Fallback |
|---|---|---|
| `artifact_0` … `artifact_6` | Piece of that kind: a `PackedScene` (`.glb`, `.tscn`) or a `Mesh` | Placeholder primitive (below) |
| `temple_board` | Board under the pieces: a `PackedScene` | Stone slab with alternating cell tiles |

`game/assets/manifest.json` is one flat JSON object of name to `res://`
path. A missing file, name, or a path that does not load gives a warning
and the fallback. Art is modelled for a 1×1 cell, at most 0.8 wide, origin
at the piece centre, +Y up.

Placeholders differ by outline as well as colour, seen from above:

| Kind | Colour | Outline |
|---|---|---|
| 0 | red | circle (sphere) |
| 1 | orange | diamond (turned cube) |
| 2 | yellow | octagon (gold coin) |
| 3 | green | triangle |
| 4 | blue | ring (torus) |
| 5 | purple | pill (capsule lying flat) |
| 6 | white | hexagon |

## Scenes

| Scene | Root | Contents |
|---|---|---|
| `game/scenes/board/board.tscn` | `Board` (Node3D) | `WorldEnvironment`, `KeyLight`, `FillLight`, `View` (BoardView3D), `Camera` (BoardCamera3D) |
| `game/scenes/board/board_playtest.tscn` | `BoardPlaytest` | `board.tscn` instance, wired to `Board/View` |
| `game/scenes/main.tscn` | `Main` | `board_playtest.tscn` instance (until WP-28) |

Layers, back to front: environment (sky, ambient), slab, cell tiles,
pieces. All opaque. Lights: one warm key light about 65° up with soft
shadows (opacity 0.6), one cool fill light at 0.35 energy, no shadow.

## Layout

The board lies on the view's XZ plane, centred on the view's origin. Cell
`x` runs along +X, cell `y` along +Z, so row 0 is the far edge and shows
at the top of the screen. Cell centre:
`((x - (w-1)/2) * cell, 0, (y - (h-1)/2) * cell)`. Pure functions in
`board_layout.hpp`, tested in `extension/src/board/tests/`.

## Camera fit

The camera looks at the view's origin from `tilt_degrees` off vertical and
backs off to the smallest distance where every corner of the slab, at
board level and at piece height (0.6 cells), is inside the viewport minus
`margin`. Portrait phones fit by width, wide screens by height; the board
is never cropped.

![Board on a 9:19.5 phone](../images/board-view-phone.png)

## Feel constants

`extension/src/board/feel.hpp`, tuned in the feel pass (WP-40).

| Constant | Value | Meaning |
|---|---|---|
| `kSwapSeconds` | 0.12 s | A swap's travel time |
| `kIllegalLegSeconds` | 0.09 s | Each leg of an illegal bounce |
| `kIllegalReach` | 0.45 cell | How far an illegal swap travels before bouncing |
| `kSettleSeconds` | 0.18 s | A changed piece dropping into place |
| `kSettleDropCells` | 1.5 cells | Height it drops from |
| `kSelectedScale` | 1.18 | Size of the selected piece |
| `kSelectedLiftCells` | 0.12 cell | Lift of the selected piece |
| `kCameraTiltDegrees` | 15° | Camera pitch from straight down |
| `kCameraFovDegrees` | 40° | Vertical field of view |
| `kCameraMargin` | 0.08 | Empty viewport fraction per side |

## Input (until WP-14)

Tap or click a piece to select it, then an adjacent piece to ask for the
swap. Tapping a non-adjacent piece moves the selection; tapping off the
board clears it. Input is ignored while `is_busy()`.

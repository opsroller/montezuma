---
name: board-scene
description: Structure of the Montezuma 3D board scene and how it consumes core results. Use when building or changing scenes/board/.
---

# Board scene

```
BoardView (Node3D, scenes/board/board_view.tscn)
├── Camera3D        near top-down, ~12-18° tilt, orthographic-feeling FOV (~30°)
├── Lights          one warm key, soft fill, baked-looking ambient
├── Temple          environment from assets manifest "temple_board"
└── Pieces          one Node3D per cell, pooled, mesh from manifest "artifact_<kind>"
```

## Flow

1. Input picks two cells and emits `swap_requested(a, b)`.
2. The game scene calls `Board.try_swap`. Null means illegal: play the
   bounce-back animation.
3. A `MoveResult` comes back; `BoardView.play(result)` animates swap,
   then for each cascade step: clear cells, fall, spawn, in that order,
   awaiting each tween.
4. Input is locked while animating; `animation_finished` re-enables it.

Cell (x, y) maps to world `Vector3(x - 3.5, 0, y - 3.5) * CELL_SIZE`, with
y = 0 at the top row, matching `core/`.

Keep the view stateless beyond the node pool: after `play` finishes, the
pieces must equal `Board.cells`. A debug assert checks this in editor
builds.

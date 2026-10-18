<!--
SPDX-License-Identifier: GPL-2.0-or-later
Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
-->

# MoveResult reference

What `montezuma::Board` reports for a move, field by field, so a view can
animate it without knowing the rules. Declared in
`core/include/montezuma/board.hpp`; the contract between WP-01 and WP-13 is
in [contracts.md](contracts.md#moveresult-events-owner-gameplay-programmer-wp-01--implemented).

Coordinates: `x` runs left to right and `y` top to bottom, from 0. Pieces
fall toward higher `y`. Kinds are artifact indices `0 .. kinds-1`;
`Board::kEmpty` (`-1`) is an empty cell.

## Producing a MoveResult

| Call | Returns | Empty when |
|---|---|---|
| `Board::try_swap(Cell a, Cell b)` | the player's swap and everything it set off | the swap is illegal (not adjacent, out of bounds, or makes no run); the board is unchanged |
| `Board::clear_cells(const std::vector<Cell>&)` | an effect (totem, bonus) clearing those cells and everything it set off | no in-bounds, non-empty cell is left after dropping duplicates and out-of-bounds cells; the board is unchanged |

Both take an optional last argument, `const StepHooks* hooks` (default
`nullptr`); see [Step hooks](#step-hooks).

Neither throws for game reasons. Both resolve the same way: clear, collapse,
refill, repeat while runs remain, then shuffle if no legal move is left.

## Fields

```cpp
struct MoveResult {
    std::vector<std::map<int, int>> steps;           // per step: kind -> pieces cleared
    int  matched_kind = -1;
    bool shuffled = false;
    bool stuck = false;
    std::optional<std::array<Cell, 2>> swap;
    std::vector<CascadeStep> cascades;
    std::vector<std::pair<Cell, Cell>> shuffle_map;  // {from, to}
    bool redealt = false;
    std::map<int, int> cleared_by_kind() const;
    int  score() const;
};
```

| Field | Meaning |
|---|---|
| `steps` | One map per cascade step, kind to number of pieces cleared. Same length as `cascades`; `steps[i]` counts `cascades[i].cleared` by kind. Quotas and score use it. |
| `matched_kind` | `try_swap`: the kind that made up most of the swap's own match (drives Power Totem streaks). `clear_cells`: `-1`, because an effect is not a player match. |
| `shuffled` | No legal move was left after resolving, and the board was reshuffled (see `shuffle_map`) or redealt (see `redealt`). |
| `stuck` | No legal move was left and neither a reshuffle nor a fresh deal worked; the board is unchanged and the caller should end or restart the level. Practically unreachable. |
| `swap` | `try_swap`: the two cells in the order passed. `clear_cells`: empty (`std::nullopt`). |
| `cascades` | One `CascadeStep` per step. Step 0 is the swap's own match, or the cells `clear_cells` removed. Never empty. |
| `shuffle_map` | When `shuffled && !redealt`: every piece that changed place, as `{from, to}`, ordered by `to` in row-major order (y, then x). Pieces not listed stayed put. Empty otherwise. |
| `redealt` | When `shuffled`: the existing pieces could not be rearranged into a playable board, so a new board was dealt. `shuffle_map` is empty; read `Board::cells()` for the new layout (fade the old pieces out and the new ones in). |
| `score()` | 10 points per piece times the 1-based step: `10 * count * (i + 1)`. |
| `cleared_by_kind()` | `steps` summed over all steps. |

### CascadeStep

The order of events inside one step is: `runs` are found, `cleared` cells
empty at once, `falls` move down at once, `spawns` drop in.

| Field | Type | Order |
|---|---|---|
| `runs` | `Run { std::vector<Cell> cells; int kind; }` | Every horizontal run (rows top to bottom, left to right within a row), then every vertical run (columns left to right, top to bottom within a column). Cells inside a run go left to right or top to bottom. `Run::horizontal()` tells the two apart. |
| `cleared` | `std::vector<Cell>` | Row-major (y, then x), each cell once. |
| `falls` | `Fall { Cell from, to; int kind; }` | Column by column left to right; bottom to top within a column. Only pieces that move. |
| `spawns` | `Spawn { Cell to; int kind; int drop; }` | Column by column left to right; bottom to top within a column. |

- **Runs.** Each straight line of 3 or more of one kind. A cell shared by
  two runs (L, T and plus shapes) appears in both runs but once in
  `cleared`. A 4-run or 5-run is one run of that length. For a match step,
  `cleared` is exactly the union of the runs, plus any cells an
  `extend_clear` hook added. For step 0 of `clear_cells`,
  `runs` is empty and `cleared` is the effect's cells.
- **Falls.** `from.x == to.x` and `to.y > from.y`, always. No two falls
  share a `from` or a `to`. Apply them simultaneously: every `to` is empty
  once the cleared cells and every `from` are vacated.
- **Spawns.** `drop >= 1` is how many rows above the top edge the new piece
  starts: it starts at `y = -drop`. In a column with `n` new pieces,
  the piece landing in row `y` has `drop = n - y`, so the column's new
  pieces enter as a solid stack (the lowest starts one row above the board,
  the highest `n` rows above). Spawns fill exactly the cells still empty
  after the falls.

## Step hooks

Rules layered on the board, such as power-ups ([bonuses.md](bonuses.md)),
act inside a step through `StepHooks`. The board calls them and never
interprets them.

```cpp
struct StepHooks {
    std::function<std::vector<Cell>(const Board&, const std::vector<Cell>& cleared, int step)> extend_clear;
    std::function<void(const Board&, const CascadeStep& step, int index)> after_refill;
    std::function<bool(const Board&, Cell)> matchable;  // null: every piece
    std::function<bool(const Board&, Cell)> movable;    // null: every piece
    bool settle_shuffle = true;
    int max_steps = 0;                                  // 0: no cap
};
```

| Hook | Called | Effect |
|---|---|---|
| `extend_clear` | once per step, before its cells empty, with the pieces still on the board; `cleared` is the step's cells so far (row-major), `step` its index in `cascades` | the returned cells are cleared in the same step: added to `cleared` and counted in `steps`. Out-of-bounds, empty and duplicate cells are ignored. `runs` stays as found, so for a match step `cleared` is then a superset of the runs' union |
| `after_refill` | once per step, after its falls and spawns | none on the board; the hook sees the step as recorded |
| `matchable` | by run finding, legality and shuffles | a piece it rejects ends a run (steel, `OBS-01`, `RES-01`) |
| `movable` | by legality and shuffles | a piece it rejects cannot be swapped and keeps its cell in a shuffle (`MOVE-03`, `OBS-06`) |
| `settle_shuffle` | at the end of a move | `false`: the board never shuffles; the caller does after its own rules (`RES-08`) |
| `max_steps` | before each cascade step | at the cap the move stops with `stuck` set (`Board::kMaxCascadeSteps` is 100) |

`Board::find_runs(hooks)`, `is_legal_swap(a, b, hooks)`,
`has_legal_move(hooks)` and `shuffle(hooks, map, redealt)` are the
hook-aware overloads; a shuffle with hooks moves only movable pieces and a
redeal deals only movable cells.

Either may be empty. For an illegal swap, or a `clear_cells` call with no
valid cell, no hook is called. Hooks draw nothing from the board's random
generator, so a move with hooks that add no cells gives the same
`MoveResult` and board as one without. Exceptions thrown by a hook
propagate.

## Replaying a result

Starting from a copy of `Board::cells()` taken before the call, this
reproduces the board after every step and after the move, exactly:

1. If `swap` is set, swap its two cells.
2. For each `CascadeStep`: set every `cleared` cell to empty; lift every
   `falls[i].from` piece, then put each at its `to`; put each spawn's `kind`
   at its `to`.
3. If `shuffled && !redealt`: `after[to] = before[from]` for each pair in
   `shuffle_map`, reading from a copy taken before this stage.
4. If `redealt`: the board is `Board::cells()`.

`core/tests/test_cascade_events.cpp` checks this replay, and every
invariant above, over thousands of random moves and effects on many seeds
and board sizes.

## Determinism

The same seed and the same calls give identical `MoveResult`s (every field
compares equal with `operator==`). Random draws happen in a fixed order:
refills column by column left to right, bottom to top; shuffles as a
Fisher-Yates pass from the last cell down. Adding these events did not
change any board: a seed plays out exactly as it did before.

## For the Godot binding

`Match3Board.try_swap(a, b)` and `Match3Board.clear_cells(cells)` return
the MoveResult as a `Dictionary`, or an empty one when the swap is illegal
or no listed cell holds a piece. Cells are `Vector2i`; arrays are plain
`Array`.

| Key | Value |
|---|---|
| `steps` | `[{kind: count}]`, one per cascade step |
| `matched_kind`, `shuffled`, `stuck`, `score`, `redealt` | as above |
| `swap` | `[a, b]`; absent for `clear_cells` |
| `cascades` | `[{cleared: [cell], falls: [{from, to, kind}], spawns: [{to, kind, drop}], runs: [{cells: [cell], kind}]}]` |
| `shuffle_map` | `[[from, to]]` |

# Contracts

The interfaces between work packages. They let packages run in parallel:
each side builds against this file, with stubs, before the other side
lands. Each section has an owner (see `docs/reference/work-packages.md`); to change
one, message the owner (coordinate skill), and the owner lands it as a
`feat(contracts):` commit.

Status: **proposed** until the package that implements it merges.

Each contract gives the C++ shape (namespace `montezuma`) and, where Godot
sees it, the surface the binding exposes (`Dictionary`, `Array`,
`Vector2i`). The binding converts one to the other; both sides build
against both.

Coordinates: `x` runs left to right and `y` top to bottom, from 0; pieces
fall toward higher `y`. The 3D view maps `y` to depth or height, never
the reverse.

## MoveResult events (owner: gameplay-programmer, WP-01) — implemented

Implemented in `core/include/montezuma/board.hpp` (field-by-field
reference, ordering rules and the replay recipe:
[move-result.md](move-result.md)). Every legal swap, and every effect
through `Board::clear_cells`, reports cell-level events so the board view
animates without knowing the rules. The count fields the binding already
uses are unchanged.

```cpp
namespace montezuma {
struct Cell  { int x, y; };                      // x left to right, y top to bottom, from 0
struct Fall  { Cell from, to; int kind; };       // to.x == from.x, to.y > from.y
struct Spawn { Cell to; int kind; int drop; };   // starts at y = to.y - drop, drop >= 1
struct Run   { std::vector<Cell> cells; int kind; bool horizontal() const; };

struct CascadeStep {
    std::vector<Cell>  cleared;                  // row-major, each cell once
    std::vector<Fall>  falls;                    // by column left to right, bottom to top
    std::vector<Spawn> spawns;                   // by column left to right, bottom to top
    std::vector<Run>   runs;                     // horizontal runs, then vertical; empty for clear_cells step 0
};

struct MoveResult {
    std::vector<std::map<int, int>> steps;       // per step: kind -> pieces cleared (== cascades[i].cleared)
    int  matched_kind = -1;                      // drives Power Totem streaks; -1 for clear_cells
    bool shuffled = false;
    bool stuck = false;                          // no playable board could be made; board unchanged
    std::optional<std::array<Cell, 2>> swap;     // as passed to try_swap; empty for clear_cells
    std::vector<CascadeStep> cascades;           // step 0 is the swap's own match (or the cleared cells)
    std::vector<std::pair<Cell, Cell>> shuffle_map;  // {from, to} for each moved piece, by `to` row-major
    bool redealt = false;                        // shuffle fell back to a fresh deal; shuffle_map empty
    int  score() const;                          // 10 x pieces x (step + 1)
    std::map<int, int> cleared_by_kind() const;
};

std::optional<MoveResult> Board::try_swap(Cell a, Cell b);                 // empty if illegal
std::optional<MoveResult> Board::clear_cells(const std::vector<Cell>&);    // empty if no valid cell
std::vector<Run> Board::find_runs() const;
}
```

Differences from the proposal, and why:

- `swap` is `std::optional`: `clear_cells` results have no swap, and an
  empty optional says so without a sentinel cell.
- `redealt` is new: when the pieces cannot be rearranged the shuffle deals
  a new board, which no `{from, to}` map can describe. The view reads
  `Board::cells()` for the new layout.
- `steps` and `stuck` stay for the binding and `LevelState`.

`Match3Board` (GDExtension) exposes the same as a `Dictionary` with keys
matching the field names and `Vector2i` cells; until the binding adds the
cell-level keys it returns `{ steps, matched_kind, shuffled, stuck, score }`.

## LevelData (owner: gameplay-programmer, WP-02; content: game-designer) — proposed

JSON at `game/data/levels/e<episode>/l<NN>.json`, parsed by `montezuma::LevelData::parse(std::string_view)`;
the engine reads the file and passes the text in.

| Field | Type | Notes |
|---|---|---|
| `id` | string | `e1-l03` |
| `episode`, `index` | int | 1-based |
| `board` | `{width, height, kinds}` | defaults 8, 8, 7 |
| `quotas` | `{kind_name: int}` | kind names below |
| `time_limit` | `{relaxed: float, normal: float}` | seconds |
| `totems_unlocked` | `[totem_id]` | |
| `blocked_cells` | `[[x, y]]` | optional |
| `seed` | int | for sim and tests |

Kind names, in index order: `red`, `orange`, `yellow`, `green`, `blue`,
`purple`, `white`.

## Game signals (owner: graphics-programmer, WP-10/WP-26) — proposed

`Match3Game` (C++ node in `extension/src/game/`, placed in `game/scenes/game/game.tscn`)
owns the board and level state and emits, for the HUD and audio:

```
level_started(level: LevelData, difficulty: String)
quota_changed(kind: int, remaining: int)
time_changed(seconds_left: float)
score_changed(score: int)
streak_changed(kind: int, length: int)
totem_charge_changed(totem_id: String, charge: float)   # 0..1
bonus_changed(bonus_id: String, count: int)
level_finished(outcome: int, score: int)                # LevelState.Outcome
```

`BoardView3D` API: `show_board(board: Match3Board)`, `play(result: Dictionary)`
(emits `animation_finished` when done), `play_illegal(a, b)`, signals `swap_requested(a: Vector2i,
b: Vector2i)` and `animation_finished`.
**Implemented** in WP-12 (`extension/src/board/`), plus `is_busy()`,
`cell_at_screen(pos)` and a `layout_changed` signal; see
[Board view](board-view.md).

## Asset manifest (owner: technical-artist, WP-07/08) — proposed

`game/assets/manifest.json` maps logical names to resource paths. The board view
falls back to a placeholder primitive when a name is missing.

`artifact_0` … `artifact_6` (by kind index), `temple_board`, `temple_env`,
`cell_highlight`, `fx_match`, `fx_cascade`, `totem_<id>`, `bonus_<id>`.

## Audio cues (owner: sound-designer, WP-14) — proposed

Played by name with `GameAudio::play(name)`; a missing cue warns and is silent.

`swap`, `swap_illegal`, `match`, `cascade_1` … `cascade_8`, `piece_land`,
`bonus_earned`, `bonus_use_<id>`, `totem_charge`, `totem_fire_<id>`,
`quota_done`, `time_warning`, `level_won`, `level_lost`, `ui_click`,
`ui_back`, `trophy`, `music_menu`, `music_play_<episode>`,
`ambience_jungle`.

## Totem and bonus ids (owner: game-designer, WP-03) — proposed

Totems: `row_clear`, `random_blast`, `time`, `lightning`, `color_bomb`,
`multiplier`. Bonuses: `hammer`, `lightning`, `extra_time`, `shuffle`,
`dynamite`, `freeze`. Placeholders until WP-03 settles the originals'
behaviour; ids may be renamed then.

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

## MoveResult events (owner: gameplay-programmer, WP-01) — proposed

**Today** (`core/include/montezuma/board.hpp`, merged): a legal swap
returns a `MoveResult` with one entry per cascade step mapping artifact
kind to pieces cleared, plus `matched_kind`, `shuffled` and `score()`.
`Match3Board::try_swap` returns it as a `Dictionary`
`{ steps, matched_kind, shuffled, score }`, or an empty one for an
illegal swap.

**Proposed** for WP-13 (animation needs cells, not counts): extend it
with cell-level events so the board view animates without knowing rules.
The code is the truth for field names once WP-13 lands.

```cpp
namespace montezuma {
struct Cell { int x, y; };                 // x left to right, y top to bottom, from 0
struct Fall  { Cell from, to; int kind; };
struct Spawn { Cell to; int kind; int drop; };   // drop = rows above the board it starts
struct Run   { std::vector<Cell> cells; int kind; };

struct CascadeStep {
    std::vector<Cell>  cleared;
    std::vector<Fall>  falls;
    std::vector<Spawn> spawns;
    std::vector<Run>   runs;               // for bonus pickups and effects
};

struct MoveResult {
    std::array<Cell, 2>      swap;
    std::vector<CascadeStep> cascades;     // step 0 is the swap's own match
    int  matched_kind = -1;                // drives Power Totem streaks
    bool shuffled = false;
    std::vector<std::pair<Cell, Cell>> shuffle_map;
    int  score() const;                    // 10 x pieces x (step + 1)
    std::map<int, int> cleared_by_kind() const;
};

std::optional<MoveResult> Board::try_swap(Cell a, Cell b);
}
```

`Match3Board` (GDExtension) exposes the same as a `Dictionary` with keys
matching the field names and `Vector2i` cells.

## LevelData (owner: gameplay-programmer, WP-02; content: game-designer) — proposed

JSON at `game/data/levels/e<episode>/l<NN>.json`, parsed by `montezuma::LevelData::parse(std::string_view)`;
the engine reads the file and passes the text in.

| Field | Type | Notes |
|---|---|---|
| `id` | string | `e1-l03` |
| `episode`, `index` | int | 1-based |
| `board` | `{width, height, kinds}` | defaults 8, 8, 7 |
| `gem_goal` | `{normal: int, hard: int}` | gems to collect; a level needs `gem_goal` or `quotas` |
| `quotas` | `{kind_name: int}` | optional; kind names below. With `gem_goal`, both must be met (`GOAL-06`) |
| `time_limit` | `{normal: float, hard: float}` | optional, seconds; default `270 + 30 × timer tier` (tiers 0 to 4) |
| `totems_unlocked` | `[totem_id]` | available on this level; the tier comes from player progress (WP-08) |
| `powerups_unlocked` | `[power-up id]` | optional, as above |
| `steel_rate` | float | optional, 0 to 1, default 0 (`OBS-02`) |
| `blocked_cells` | `[[x, y]]` | optional |
| `chained_cells` | `[[x, y]]` | optional (`OBS-05`) |
| `rules` | string | optional, default `classic` |
| `theme` | string | optional, default `temple` |
| `seed` | int | for sim and tests |

Difficulties: `normal`, `hard`.

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

## Totem and power-up ids (owner: game-designer, WP-03) — settled

Rules: [rules-totems.md](rules-totems.md), [rules-power-ups.md](rules-power-ups.md).

Totems, one per kind, in kind order: `fire` (red), `time` (orange),
`gold` (yellow), `harvest` (green), `ward` (blue), `gift` (purple),
`paint` (white).

Power-ups (the original's bonuses): `tnt`, `color_clear`, `lightning`,
`clock`, `multiplier`, `hint`.

Bonus games: `bg_accuracy`, `bg_memory`, `bg_reaction`, `bg_attention`.

Rule sets: `classic`. Themes: `temple`.

These replace the placeholders `row_clear`, `random_blast`, `color_bomb`
(totems) and `hammer`, `extra_time`, `shuffle`, `dynamite`, `freeze`
(bonuses). Asset and audio names follow: `totem_<id>`, `bonus_<id>`,
`totem_fire_<id>`, `bonus_use_<id>` use these ids.

The LevelData fields above were agreed with the LevelData owner and land with WP-05 (#7).

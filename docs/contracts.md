# Contracts

The interfaces between work packages. They let packages run in parallel:
each side builds against this file, with stubs, before the other side
lands. Each section has an owner (see `docs/work-packages.md`); to change
one, message the owner (coordinate skill), and the owner lands it as a
`feat(contracts):` commit.

Status: **proposed** until the package that implements it merges.

## MoveResult events (owner: gameplay-engineer, WP-01) — proposed

`Board.try_swap(a, b)` keeps returning `MoveResult` (null when illegal).
WP-01 adds cell-level events so the board view can animate without
knowing rules. Existing fields (`steps`, `matched_kind`, `shuffled`,
`score()`, `cleared_by_kind()`) stay.

```gdscript
var swap: Array[Vector2i]            # [a, b]
var cascades: Array[CascadeStep]     # same length and order as steps

class CascadeStep:
    var cleared: Array[Vector2i]     # cells emptied this step
    var falls: Array[Dictionary]     # {from: Vector2i, to: Vector2i, kind: int}
    var spawns: Array[Dictionary]    # {to: Vector2i, kind: int, drop: int}  drop = rows above the board it starts
    var runs: Array[Dictionary]      # {cells: Array[Vector2i], kind: int}  for bonus pickups and effects

var shuffle_map: Array[Dictionary]   # {from: Vector2i, to: Vector2i} when shuffled
```

Coordinates: `x` left to right, `y` top to bottom, both from 0.

## LevelData (owner: gameplay-engineer, WP-02; content: level-designer) — proposed

JSON at `data/levels/e<episode>/l<NN>.json`, loaded by `LevelData.load(path)`.

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

## Game signals (owner: board-presentation, WP-09/WP-29) — proposed

The game scene (`scenes/game/game.tscn`) owns the `Board` and `LevelState`
and emits, for the HUD and audio:

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

`BoardView` API: `show_board(board: Board)`, `play(result: MoveResult) -> void`
(awaitable), `play_illegal(a, b)`, signals `swap_requested(a: Vector2i,
b: Vector2i)` and `animation_finished`.

## Asset manifest (owner: asset-artist, WP-06/07) — proposed

`assets/manifest.json` maps logical names to resource paths. The board view
falls back to a placeholder primitive when a name is missing.

`artifact_0` … `artifact_6` (by kind index), `temple_board`, `temple_env`,
`cell_highlight`, `fx_match`, `fx_cascade`, `totem_<id>`, `bonus_<id>`.

## Audio cues (owner: audio-designer, WP-14) — proposed

Played by name with `Audio.play(name)`; a missing cue warns and is silent.

`swap`, `swap_illegal`, `match`, `cascade_1` … `cascade_8`, `piece_land`,
`bonus_earned`, `bonus_use_<id>`, `totem_charge`, `totem_fire_<id>`,
`quota_done`, `time_warning`, `level_won`, `level_lost`, `ui_click`,
`ui_back`, `trophy`, `music_menu`, `music_play_<episode>`,
`ambience_jungle`.

## Totem and bonus ids (owner: level-designer, WP-03) — proposed

Totems: `row_clear`, `random_blast`, `time`, `lightning`, `color_bomb`,
`multiplier`. Bonuses: `hammer`, `lightning`, `extra_time`, `shuffle`,
`dynamite`, `freeze`. Placeholders until WP-03 settles the originals'
behaviour; ids may be renamed then.

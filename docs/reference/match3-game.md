<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Match3Game (Godot binding)

Reference for programmers working on the game scene, the HUD or audio.
`Match3Game` is the game scene's controller: it loads a level file, plays
it with the core [Level session](level-session.md), drives the
[board view](board-view.md), counts time down, emits the
[Game signals](contracts.md#game-signals-owner-graphics-programmer-wp-10wp-26--implemented)
and plays the [audio cues](audio.md#when-each-cue-plays). It holds no
rules: every decision is the session's. Why it is built this way:
[About the game scene](../explanation/about-the-game-scene.md).

Code: `extension/src/game/match3_game.*`, registered by
`extension/src/game/register_game.cpp`. Scene: `game/scenes/game/game.tscn`.

## Scene

| Scene | Root | Contents |
|---|---|---|
| `game/scenes/game/game.tscn` | `Game` (Match3Game, group `match3_game`) | `board.tscn` instance, wired to `Board/View` |
| `game/scenes/main.tscn` | `Main` | `game.tscn` instance |

The group is set in the scene so the HUD finds the node as it enters the
tree. The node connects to the view and UiRoot on `_ready` and again
whenever it re-enters the tree, and disconnects on leaving it. On `_ready` the node connects to `/root/UiRoot`: `play_requested`
starts a level and `level_quit_requested` stops it. A level that fails to
start sends UiRoot the `quit_level` action, so the episode map comes back.

## Properties

| Property | Default | Notes |
|---|---|---|
| `board_view_path` | `Board/View` in the scene | The `BoardView3D` to drive |
| `levels_dir` | `res://data/levels` | Level `e<N>-l<NN>` is read from `<levels_dir>/e<N>/l<NN>.json` |
| `autostart_level` | empty | A level id to start on `_ready` without the menus (for a test scene); empty waits for UiRoot |
| `autostart_difficulty` | `normal` | `normal` or `hard` |
| `warning_seconds` | 10 | `time_warning` plays when time left crosses this |
| `effects_showcase` | false | Look-dev: after `autostart_level`, fires every totem and sets off every power-up in turn, forever (`game/scenes/board/effects/effects_showcase.tscn`) |

## Methods

| Method | Description |
|---|---|
| `start_level(level_id: String, difficulty: String) -> bool` | Loads and starts a level. False, with an engine error and the previous state kept, for an id not shaped `e<N>-l<NN>`, a difficulty other than `normal` or `hard`, a missing file, a file the [LevelData parser](level-data.md) rejects, a file whose `id` differs from the requested one, or a level the session cannot deal (for example one with `blocked_cells`) |
| `stop_level()` | Ends the level with no result and stops the ambience |
| `shuffle()` | HUD command: a manual shuffle (`SHUF-01`). Ignored while the board animates, with none left, or after the end |
| `buy_hint()` | HUD command: buys a hint (`SHUF-04`) and emits `hint_shown` |
| `request_swap(a: Vector2i, b: Vector2i) -> bool` | What the view's `swap_requested` calls. True for a legal swap. Ignored while the board animates or after the end |
| `is_playing() -> bool` | A level is running and not over |
| `get_level_id()`, `get_outcome()`, `get_score()`, `get_time_left()`, `get_gems_collected()`, `get_gem_goal()`, `get_moves()` | The session's state; `get_outcome()` is `PLAYING` (0), `WON` (1) or `LOST` (2) |
| `get_board() -> Match3Board` | The board the view draws: a copy of the session's board after the latest command |
| `get_legal_moves() -> Array` | Every legal swap as `[a, b]`, for bots and tests |

Core exceptions stop in this class: a failure logs an engine error and
changes nothing.

## Signals

| Signal | When |
|---|---|
| `level_started(level: Dictionary, difficulty: String)` | A level starts. `level` holds the LevelData fields with kind names as `quotas` keys; `time_limit` holds, per difficulty, the time a session starts from: the level's base clock plus the timer tier bonus (`GOAL-03`) |
| `time_changed(seconds_left: float)` | On start, every frame while playing, and after a move that changes the time |
| `score_changed(score: int)` | On start and when the score changes (moves, bought hints) |
| `gems_changed(collected: int, goal: int)` | On start and when gems are collected; goal 0 means the level has no gem goal |
| `quota_changed(kind: int, remaining: int)` | On start and when a quota's remaining count changes; levels with quotas only |
| `totem_changed(totem_id: String, charge: float, uses_left: int)` and `totem_charge_changed(totem_id: String, charge: float)` | On start and when an owned totem's charge or uses change. A totem that fired shows charge 1 until the next move |
| `frenzy_changed(bar: float, seconds_left: float)` | On start and when either changes. `bar` is the fill (0..1) while Frenzy is off and the time left as a fraction while it is on |
| `shuffles_changed(left: int)` | On start and after a manual shuffle |
| `hint_cost_changed(cost: int, affordable: bool)` | On start and when affordability changes: true while playing with a score of at least `cost`, or a hint already shown |
| `hint_shown(a: Vector2i, b: Vector2i)` | A bought hint, or the free onboarding hint (`SHUF-05`). The node also rings the pair on the board until the next move; a free hint that arrives while a move is playing is ringed when the move finishes |
| `level_finished(outcome: int, score: int)` | Once, after the last move's animation ends. The HUD opens the results screen on it |

`streak_changed` and `bonus_changed` from the first contract block are not
emitted: the rules have no streak apart from totem charges, and no held
power-ups.

## How a command plays

A legal swap becomes one board animation: the swap's cascades, then each
fired totem's cascades, then the automatic reshuffle. A totem that clears
nothing gets an empty step of its own. Each step carries what it set off
(power-ups, markers, the totem firing, `paint` repaints) and the result
carries every cell's markers after the move, in the keys the
[board view's effects](board-view.md#effects) read; the packing is
`extension/src/game/display_move.hpp`. The mirrored board is updated to
the session's board before the view plays, so the view ends on the
session's state, markers included. On a level start the node calls the
view's `show_marks`.

When a `hint` power-up goes off (`PWR-11`), every legal swap is ringed
after each move until its time runs out; another one restarts it.

| Cue | Played |
|---|---|
| `swap`, `swap_illegal` | When the request is answered |
| `match`, `cascade_<n>` (8 and over: `cascade_8`), `gem_collect`, `bonus_use_<id>`, `steel_break`, `chain_break`, `bonus_earned` | On the view's `cascade_step` for that step. `n` counts display steps across the whole move, so a totem's first step plays `cascade_<n>`, not `match` |
| `totem_fire_<id>` | On the totem's first step, its own empty step for a totem that clears nothing |
| `totem_charge` | At most once per move, when any owned totem gains charge without firing |
| `quota_done` | When a quota reaches 0 |
| `time_warning` | When time left crosses `warning_seconds` downwards |
| `level_won`, `level_lost` | With `level_finished` |
| `music_play_<episode>`, `ambience_jungle` | On start |

`piece_land` is not played yet: it needs the view's landing frames.

## Defaults until other packages land

- Upgrades: every tier 0 until player progress (WP-08), so a level
  starts from its base clock (`GOAL-03`).
- Seed: the level's `seed`, so a level deals the same board each time.

## Self-test

```sh
godot --headless --path game --quit-after 120 -- --game-selftest
```

First starts `e1-l05` on hard and checks that the session and the
`level_started` dictionary's `time_limit.hard` both start from the
level's own base clock (300 s, above the 270 s default) plus the timer
tier bonus (`GOAL-03`). Then plays `e1-l01` on normal by always taking the first legal move, through
`request_swap`, `shuffle` and the view, checks that the mirrored board
matches the session after every move, that every HUD signal fired, that
`level_finished` fired once with a win, and that every cue it would play
is in the cue table. Then it starts the level again and, for each totem and power-up in turn,
sets the session up (a charged totem at tier 3, or a power-up on a piece
of a legal move's run, steel for ward and chains for harvest) and plays
the move: the view must schedule that effect and end with the session's
markers. It prints
`Match3Game effects: 7 totems and 6 power-ups staged in <n> moves`, then
`Match3Game self-test: <n> moves, outcome 1, <n> cues, 0 failures`, which
CI checks. Two expected errors precede it (a missing level and a bad id).

See also: [Board view](board-view.md), [UI](ui.md#hud),
[Add a level](../how-to/add-a-level.md).

# Match3Level (Godot binding)

`Match3Level` is a `RefCounted` class from the montezuma GDExtension. It
exposes the core `LevelState`: per-kind quotas, the countdown timer, score,
the same-kind streak and the win/lose outcome, fed every move result and
the frame delta. The game scene plays levels with the full rules through
[Match3Game](match3-game.md) instead; `Match3Level` stays for quota-only
rule variants and tools.

Gem goals (`GOAL-01`) come with the gem rules in the core; until then the
goal is per-kind quotas (`GOAL-06`).

## Methods

| Method | Description |
|---|---|
| `setup(quotas: Dictionary, time_limit: float)` | Starts a level. `quotas` maps kind (int) to pieces to clear. Empty quotas, a count of 0 or less, or a time of 0 or less is an engine error and leaves the level unset. Emits `score_changed(0)` and `time_changed(time_limit)`. |
| `apply_move(result: Dictionary)` | Applies a `Match3Board.try_swap` or `clear_cells` result. Reads `steps` and `matched_kind`. An empty Dictionary (illegal swap) is ignored. |
| `tick(delta: float)` | Counts the timer down while the level is playing. |
| `is_ready() -> bool` | Whether `setup` succeeded. |
| `get_outcome() -> int` | `PLAYING` (0), `WON` (1) or `LOST` (2). |
| `get_score() -> int` | Score so far. |
| `get_time_left() -> float` | Seconds left. |
| `get_quotas() -> Dictionary` | Kind to pieces still needed. |
| `get_streak_kind() -> int`, `get_streak() -> int` | Current same-kind streak; kind is -1 before the first match. Effect results (`clear_cells`) leave it unchanged. |

## Signals

| Signal | When |
|---|---|
| `score_changed(score: int)` | After a move that scored, and on `setup`. |
| `quota_changed(kind: int, remaining: int)` | For each kind whose remaining count changed in a move. |
| `time_changed(seconds_left: float)` | Every `tick` while playing, and on `setup`. |
| `streak_changed(kind: int, length: int)` | When a move changes the streak. |
| `level_finished(outcome: int, score: int)` | Once, when the level is won (all quotas met) or lost (time ran out). |

See also: [MoveResult events](move-result.md) for the result Dictionary.

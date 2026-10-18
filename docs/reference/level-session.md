<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Level session

Reference for `montezuma::LevelSession`, which plays one level by the full
`classic` rules: the board, gems, steel and chains, power-ups, totems,
Score Frenzy, scoring, the goal, the timer and shuffles. It is what the
balance simulator (WP-22, `tools/sim/`) plays, and what the game layer
can drive. Rules are in the [rules spec](rules-spec.md),
[power-ups](rules-power-ups.md), [Power Totems](rules-totems.md) and
[progression](progression.md); code cites them as `// rule: ID`.

Code: `core/include/montezuma/level_session.hpp`,
`core/src/level_session.cpp`; tests in `core/tests/test_level_session.cpp`.

## Building a session

```cpp
LevelSession s(level, Difficulty::Normal, upgrades, seed);
```

| Input | Meaning |
|---|---|
| `level` | a parsed `LevelData` |
| `difficulty` | `Normal` or `Hard`; hard changes only the gem goal (`DIFF-03`) |
| `upgrades` | what the player owns: `timer_tier` 0..4, `gems_tier` 0..3 (`gems_max` = 5 + tier), `frenzy_tier` 0..4 (0 locked; 15, 20, 25, 30 s), `totem_tiers[TotemId]`, `power_up_tiers[PowerUpId]` |
| `seed` | every random draw comes from streams derived from it |

Totem and power-up tiers apply only to the ids the level lists; a listed
id with tier 0 is owned at tier 1 (`PROG-03`); unlisted ids are not owned.
The constructor throws `std::invalid_argument` for a tier out of range, an
unknown difficulty, a level with no goal, a chained cell off the board
(`OBS-05`), and a level with `blocked_cells` (not supported yet), and `std::runtime_error` when no playable board can
be dealt.

At the start: `gems_max` gems on distinct plain pieces, the level's
chains, no steel and no power-ups (`BOARD-04`); no run and a legal move
that uses no chained piece (`BOARD-03`; if the chains leave none, the
movable pieces are reshuffled at no cost); totems at `start_level()`
(`TOT-06`); score, gems, bar and star sand 0; two manual shuffles.

## Commands

| Command | Returns | Notes |
|---|---|---|
| `swap(a, b)` | `optional<MoveReport>` | empty, with nothing changed, for an illegal swap or after the level ended |
| `tick(dt)` | `optional<TickReport>` | lowers `time_left` and `frenzy_left`; `dt < 0` throws; empty after the level ended. Game time counts in whole microseconds (`dt` rounded to nearest), so the same time gives the same game however it is split into ticks |
| `shuffle()` | `optional<ShuffleReport>` | manual shuffle, twice per level, free, not a move (`SHUF-01`) |
| `buy_hint()` | `optional<Swap>` | one legal move for 300 points, cleared by the next move or shuffle (`SHUF-04`); while a hint is shown it is returned again at no cost |

The session has no pause: the caller does not tick while paused or on a
result screen (`GOAL-04`). Moves resolve instantly, so the caller decides
how much game time a move's animation takes.

`legal_moves()` lists every legal swap once as `(a, b)` with `b` right of
or below `a`, row-major by `a`, right before down; it is steel and chain
aware and changes nothing. `runs_for_swap(a, b)` gives the step-0 runs a
swap would make; `runs()` the runs on the board (empty between moves).

The `set_*` functions and `totems_for_setup()` change state with no rule
checks; they exist for tests and tools that need an exact position.

## Order of one move

`swap` resolves in the rules spec's order (`RES-01..08`):

1. Each step: runs (steel-aware), plus every power-up they set off; for
   each cleared piece its gem, its points, the Frenzy bar; fall and
   refill; steel rolls for the new pieces, then new gems on them, then
   earned power-ups.
2. After the last cascade, the totems this swap charged to 2, in kind
   order. Each effect is chosen on the board the previous one left and
   its clear resolves fully, as further steps of the same move.
3. Points: the sum of every step (Frenzy-doubled where Frenzy was on),
   gold and harvest points, times the highest multiplier of the move.
4. The goal check (`GOAL-02`). Then, if not won and no legal move is
   left, the automatic reshuffle and its 5 s penalty (`SHUF-02`).

## Reports

`StepReport`, per step in display order: `step` (within the move),
`runs`, `cleared`, `gems_collected`, `cleared_by_kind`, `points`,
`frenzy` (doubled), `frenzy_bar_after`, `frenzy_started`,
`power_ups_triggered` and `power_up_effects`, `falls`, `spawns`,
`steel_spawned`, `gems_placed`, `power_ups_placed`, `chains_broken`,
`steel_removed`.

`MoveReport`: `swap`, the board's own `result`, `steps`, `totem_charges`
(each owned totem: 0, 0.5, or 1 for one that fired), `totems_fired` (each
a `TotemFiring`: its `TotemEffect`, board `result`, `steps`, `points`,
`gems_harvested`, `gems_placed`, `chains_removed`, `power_ups_placed`,
Frenzy flags), `base_points`, `multiplier`, `points`, `gems_collected`,
`time_delta_seconds`, `hint_seconds`, `frenzy_started`,
`frenzy_restarted`, `star_sand_gained`, `outcome`, `auto_shuffle`,
`stuck`.

`TickReport`: `time_left`, `frenzy_left`, `frenzy_ended`, `free_hint`
(`SHUF-05`, on e1-l01 and e1-l02 after 8 s without a move, once until the
next move; presentation only), `outcome`, `outcome_changed`.

`ShuffleReport`: `shuffled`, `redealt`, `shuffle_map`, `stuck`,
`time_delta_seconds` (-5 for the automatic one).

Replaying a report's swap, then each step's clears, falls, spawns, steel,
gems and power-ups, then each totem's repaint, gem and chain changes and
steps, then the shuffle map, rebuilds the board and every mark exactly. A
redeal has no map: read the session again.

## Constants

| Name | Value | Rule |
|---|---|---|
| `kBaseGemsOnBoard`, `kMaxGemsTier` | 5, 3 | `GEM-02`, progression |
| `kFrenzySeconds` | 0, 15, 20, 25, 30 | `FRZ-02` |
| `kFrenzyBarMax` | 60 | `FRZ-02` |
| `kPointsPerPiece`, `kPointsPerGem`, `kFrenzyPointsFactor` | 10, 50, 2 | `SCORE-01..03` |
| `kMaxSteelOnBoard` | 8 | `OBS-02` |
| `kSteelRollScale` | 1 000 000 | steel roll precision |
| `kManualShufflesPerLevel`, `kAutoShufflePenaltySeconds` | 2, 5 | `SHUF-01`, `SHUF-02` |
| `kHintCost`, `kFreeHintIdleSeconds` | 300, 8 | `SHUF-04`, `SHUF-05` |
| `Board::kMaxCascadeSteps` | 100 | cascade cap (rules spec edge cases) |

## Determinism

The session seed derives three streams with `derive_seed(seed, stream)`,
which mixes the seed and the stream key separately before combining them,
so no two seeds share a stream by construction:
1 for the board's refills and shuffles, 2 for the level generator (gems,
steel, power-ups, totems). Callers derive their own from stream 3 up (the
simulator's bots use 3). No clock is read. Inside one refill the draws
come in this order: a steel roll per spawned piece in spawn order, then
the gem placements, then the power-up placements.

## Defaults chosen where the rules are silent

Each is a named constant or a single place in the code; the game designer
may change any of them.

- Steel ends a run through the additive `StepHooks::matchable` hook; the
  board's own end-of-move shuffle is off (`settle_shuffle = false`) so the
  session reshuffles after the goal check, keeping steel and chains in
  place.
- A move reaching `kMaxCascadeSteps` stops, reports `stuck` and, unless
  the goal was met, loses.
- Steel roll: steel when `rng.below(1000000) < round(steel_rate × 1000000)`,
  one draw per new piece, none when `steel_rate` is 0, after ward tier 2,
  or at 8 steel.
- A totem's steps continue the move's step numbering, so `SCORE-01`'s
  multiplier keeps rising.
- Gold points are doubled when Frenzy was on before gold fired, and the
  move's multiplier scales them. The highest multiplier from any part of
  the move, totem clears included, scales the whole move.
- Chained pieces count as plain: they may carry a gem or a power-up.
- An effect's first step (a totem's clear, paint's runs) earns no
  power-ups; its cascades earn as usual.
- The 5 s reshuffle penalty clamps `time_left` at 0 and loses at once.
- Quotas count every cleared piece of the kind, steel and effect clears
  included.
- After a redeal the movable pieces' gems are put back on random plain
  pieces with the level generator, after the power-ups.
- Gold's Frenzy needs Frenzy unlocked (tier 1+); with Frenzy locked it
  starts nothing.
- A manual shuffle that finds nothing playable is reported `stuck`,
  changes nothing (the board's random stream included) and does not use
  up a shuffle.
- `DIFF-03`'s default hard goal (normal × 1.5, rounded up to 5) is not
  applied: the level data always has `gem_goal.hard`.

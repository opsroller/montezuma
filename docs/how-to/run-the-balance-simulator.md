<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Run the balance simulator

How to play level files with bots and check them against the
[balance targets](../reference/balance.md). The simulator is
`montezuma_sim`, built from `tools/sim/` with the core. It plays each level
through [`LevelSession`](../reference/level-session.md).

## Build it

The simulator builds by default (`MONTEZUMA_BUILD_SIM=ON`). Use a Release
build for real runs:

```sh
cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DMONTEZUMA_BUILD_EXTENSION=OFF
cmake --build build-rel -j
```

Pass `-DMONTEZUMA_BUILD_SIM=OFF` to skip it; the core is unaffected.

## Run it

```sh
build-rel/tools/sim/montezuma_sim game/data/levels          # every level, all bots, both difficulties
build-rel/tools/sim/montezuma_sim tools/sim/examples/e1-l03.json --seeds 50
build-rel/tools/sim/montezuma_sim game/data/levels/e2 --bots greedy --difficulty normal --format csv
build-rel/tools/sim/montezuma_sim game/data/levels --check   # exit 1 when any target fails
```

Inputs are level files or directories (scanned recursively for `*.json`;
every one must be a valid level). Levels run in campaign order.

| Option | Default | Values |
|---|---|---|
| `--seeds N` | 200 | runs per level, bot and difficulty, 1 or more |
| `--bots LIST` | all | comma list of `random`, `greedy`, `planner` |
| `--difficulty` | `both` | `normal`, `hard`, `both` |
| `--upgrades` | `auto` | `auto`: every tier a player owns on reaching the level; `none`: base timer and gems, listed items at tier 1, Frenzy tier 1 from e1-l04 |
| `--format` | `table` | `table`, `csv`, `json` |
| `--check` | off | exit 1 if a target (moves floor included), curve rule or clock rule fails |

Exit codes: 0 done, 1 a failure with `--check`, 2 bad options or a bad
level file (its errors go to stderr and nothing runs).

## Read the report

One row per level, bot and difficulty: seeds, wins, win rate, median time
left over the wins (`-` when none won), mean moves over all runs, runs
that ended on a stuck board, the target and `PASS` or `FAIL`. Only
`greedy`/normal, `random`/normal and `planner`/hard have targets. From
episode 2 on, the `greedy`/normal target also names the moves floor
(for example `win>=95% median>=70.0s moves>=50`): a level whose mean
`moves` is under it fails as too easy, even when it wins every run in
time. Lengthen it with the tuning levers below, the gem goal first. Below
the rows, the two curve rules: `curve-1` for each pair of consecutive
campaign levels (a jump of more than 10 points of `greedy` win rate
fails; a level with a new idea must be easier than the one before, or
both must win every run), and `curve-2` for each episode's opener against its third level. A new idea
is a totem or power-up id, chains or steel that no earlier level in the
run has, so chains returning after a steel-only level are practice, not
a new idea. Then one `clock` line per level and difficulty: the level's
base clock (timer tier 0) must reach the [clock rule](../reference/balance.md#clock)'s
seconds for the moves `greedy`'s wins take on that difficulty, for example
`clock e2-l03 hard FAIL base 270.0s must reach 490s (119.7 moves a win x 3.5 s x 1.15)`.
The simulator plays these `greedy` runs itself, so the `clock` lines are
the same whatever `--bots`, `--difficulty` and `--upgrades` say; they
follow `--seeds`.
A rule whose levels or rows are not all in the run is `skipped`.

## How the bots play

Every 1.5 s of game time a bot ticks the session, then makes one move
while the level is on. `random` picks uniformly among the legal moves;
`greedy` the move whose own runs clear the most pieces, gems counting
double; `planner` first a move that fires an owned totem, then one that
makes a run of 4 or more, then by `greedy`'s count. Ties go to the first
move in board order. Bots never buy hints or shuffle; power-ups and
totems go off by the rules. Run `i` of a level uses a seed made only from
the level's `seed` and `i`, so results do not depend on which bots or
difficulties are in the run, and the same command prints the same report.

## Tune a level

Change one lever at a time (balance.md: the gem goal in steps of 5, then
`steel_rate`, then chains), rerun the level and its neighbours, and record
before and after in the PR. Set `base_clock` last, from the `clock`
lines, since every other lever changes the moves it is based on.

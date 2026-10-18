<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Balance targets

Reference, for game designers tuning levels and programmers working on
the balance simulator (WP-22, `tools/sim/`) and the level packages (WP-23
to WP-27). A level ships only when it meets its row.
Rules are in the [rules spec](rules-spec.md); levels in
[progression](progression.md).

## Bots

| Bot | Plays |
|---|---|
| `random` | a uniformly random legal swap |
| `greedy` | the swap that clears the most pieces in step 0, gems counted double |
| `planner` | `greedy`, but prefers a swap that charges an owned totem to 2, and the swap that makes a run of 4 or more |

Bots move every 1.5 s of game time and fire power-ups
and totems as the rules do. Each level runs 200 seeds per bot and
difficulty, with the upgrades a player would own on reaching it
(every tier bought as soon as it is available, stars permitting).

## Targets

| Levels | `greedy` win rate, normal | `greedy` median time left | `greedy` mean moves, normal | `random` win rate, normal | `planner` win rate, hard |
|---|---|---|---|---|---|
| e1-l01 | 100 % | ≥ 150 s | no floor | ≥ 95 % | ≥ 99 % |
| rest of e1 | ≥ 98 % | ≥ 100 s | no floor | ≥ 70 % | ≥ 90 % |
| e2 | ≥ 95 % | ≥ 70 s | ≥ 50 | ≥ 40 % | ≥ 80 % |
| e3 | ≥ 90 % | ≥ 50 s | ≥ 70 | ≥ 20 % | ≥ 70 % |
| e4 | ≥ 85 % | ≥ 35 s | ≥ 90 | none (informational) | ≥ 60 % |
| e5 | ≥ 80 % | ≥ 25 s | ≥ 110 | none (informational) | ≥ 50 % |

- Too easy: from episode 2 on, `greedy`'s mean moves on normal (over
  all runs, wins and losses) must be at least the floor in the table:
  50 (e2), 70 (e3), 90 (e4) and 110 (e5). Moves, not time used,
  because bots move every 1.5 s and moves resolve without animation:
  a human at about 3.5 s a move takes sim moves × 3.5 s, so e1-l01's 18
  moves are about 63 s, inside its brief. Episode 1 has no floor; its
  briefs ask for quick first wins. `--check` counts the floor as part of
  the `greedy`/normal target (WP-56).
- From episode 4 on, `random` has no target and its row is
  informational; the moves floor is the too-easy check there. The old
  ceilings (≤ 20 % in e4, ≤ 10 % in e5) cannot be met with clocks that
  fit human pace
  ([why](../explanation/about-the-episode-4-levels.md#why-random-has-no-target)).
- No level may be harder than the next by more than 10 points of
  `greedy` win rate, except a level that introduces a new idea, which
  must be easier than the level before it (the rest after a spike). A
  new idea is a totem or power-up, chains or steel on its first level in
  campaign order; chains coming back after a level without them are not
  new. Two levels that both win every run count as easier: 100 % is the
  ceiling, and every episode 1 level introduces an idea. There is no tiebreak on
  median time left: episode 1 is a teaching ramp whose gem goal rises by
  5 a level, so it does not follow the spike-and-rest shape, and a time
  tiebreak would need flat or falling goals there.
- The first level of each episode is a rest: its `greedy` win rate is at
  least that of the episode's third level.
- Tuning levers, in this order: the gem goal (in steps of 5, keeping the
  +5 curve within ±10), then `steel_rate`, then chains. Change one at a
  time and record before and after in the PR, and set the base clock
  last ([Clock](#clock)). Measured chain effect, e3-l04 with no upgrades:
  on the default 270 s clock: no chains 100 % `greedy` normal, 149 s median, 86.5 % hard; its ten
  chains 98.5 %, 141 s, 84 %; 32 chains 97.5 %, 129.5 s, 75 %
  ([why chains are weak](../explanation/about-the-episode-3-levels.md#what-the-simulator-measures-here)).

## Clock

For each level and difficulty:

```
required clock = ceil(greedy mean moves × 3.5 s × 1.15 / 10) × 10
```

- `greedy mean moves`: the mean moves of `greedy`'s winning runs, from
  runs the simulator plays for this rule alone (`--seeds` runs, 200 by
  default, both difficulties) with the `auto` upgrades except the timer,
  which stays at tier 0. `--bots`, `--difficulty` and `--upgrades` do not
  change them.
- 3.5 s: a human's time a move. 1.15: a 15 % margin. The result rounds
  up to whole 10 s.
- The base clock at timer tier 0 must reach the required clock; a
  difficulty `greedy` never wins fails. `--check` fails either case (the
  `clock` lines).
- A level sets `base_clock` when either difficulty needs more than 270 s,
  with both values: each is the larger of 270 s and that difficulty's
  required clock. Otherwise it leaves `base_clock` out (270 s).
- Each `base_clock` is a fixed point of the rule: re-running `--check`
  with it gives the same values ([add a level](../how-to/add-a-level.md)).
- Timer upgrades add 30 s a tier on top (`GOAL-03`).

With no upgrades at all the requirement is higher: e2-l06 380/570 s
instead of 310/470 s, e3-l08 730/1100 s instead of 540/800 s. Why the rule
is built this way is in [about level clocks](../explanation/about-level-clocks.md).

## Current results

`montezuma_sim game/data/levels --check`, 200 seeds, `auto` upgrades.
Win rates in %, median time left and base clocks in seconds.

| Level | `greedy` normal | `greedy` median | `greedy` moves | `random` normal | `planner` hard | Base clock normal / hard | Result |
|---|---|---|---|---|---|---|---|
| e1-l01 | 100 | 246.0 | 18.1 | 100 | 100 | 270 / 270 | pass |
| e1-l02 | 100 | 264.0 | 26.6 | 100 | 100 | 270 / 270 | pass |
| e1-l03 | 100 | 255.0 | 32.8 | 100 | 100 | 270 / 270 | pass |
| e1-l04 | 100 | 241.0 | 41.9 | 100 | 100 | 270 / 270 | pass |
| e1-l05 | 100 | 246.2 | 49.0 | 100 | 100 | 270 / 300 | pass |
| e2-l01 | 100 | 256.5 | 59.3 | 100 | 100 | 270 / 380 | pass |
| e2-l02 | 100 | 257.5 | 68.1 | 100 | 100 | 280 / 420 | pass |
| e2-l03 | 100 | 277.5 | 77.0 | 100 | 100 | 310 / 490 | pass |
| e2-l04 | 100 | 269.2 | 73.9 | 99.5 | 100 | 300 / 450 | pass |
| e2-l05 | 100 | 262.8 | 69.9 | 100 | 100 | 290 / 440 | pass |
| e2-l06 | 100 | 275.5 | 76.6 | 100 | 100 | 310 / 470 | pass |
| e3-l01 | 100 | 297.8 | 82.2 | 100 | 100 | 340 / 520 | pass |
| e3-l02 | 100 | 371.2 | 89.5 | 100 | 100 | 370 / 550 | pass |
| e3-l03 | 100 | 371.8 | 95.1 | 100 | 100 | 390 / 590 | pass |
| e3-l04 | 100 | 413.5 | 100.2 | 100 | 100 | 410 / 620 | pass |
| e3-l05 | 100 | 445.2 | 105.6 | 100 | 100 | 430 / 670 | pass |
| e3-l06 | 100 | 498.8 | 127.4 | 100 | 100 | 520 / 800 | pass |
| e3-l07 | 100 | 538.0 | 144.5 | 100 | 100 | 590 / 900 | pass |
| e3-l08 | 100 | 505.5 | 132.1 | 100 | 100 | 540 / 800 | pass |

Both curve rules and every clock line pass for episodes 1 to 3. Each
level's base clock is the larger of 270 s and its [clock rule](#clock)
value. Medians are time left on the base clock plus the `auto` timer
tier (+30 s from e1-l02, +60 s from e3-l04). Every episode 2 level clears the moves floor of 50 (59
to 77) and every episode 3 level the floor of 70 (82 to 145), at the gem
goals in [progression](progression.md); every level wins every `greedy`
run, so the curve rule holds by the 100 % tie. Why episode 3 is built and
measured this way is in
[about the episode 3 levels](../explanation/about-the-episode-3-levels.md).

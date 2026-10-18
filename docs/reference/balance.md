<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Balance targets

Reference for the balance simulator (WP-22, `tools/sim/`) and the level
packages (WP-23 to WP-27). A level ships only when it meets its row.
Rules are in the [rules spec](rules-spec.md); levels in
[progression](progression.md).

## Bots

| Bot | Plays |
|---|---|
| `random` | a uniformly random legal swap |
| `greedy` | the swap that clears the most pieces in step 0, gems counted double |
| `planner` | `greedy`, but prefers a swap that charges an owned totem to 2, and the swap that makes a run of 4 or more |

Bots move every 1.5 s of game time (a relaxed human pace) and fire power-ups
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
| e4 | ≥ 85 % | ≥ 35 s | ≥ 90 | ≤ 20 % | ≥ 60 % |
| e5 | ≥ 80 % | ≥ 25 s | ≥ 110 | ≤ 10 % | ≥ 50 % |

- Too easy: from episode 2 on, `greedy`'s mean moves on normal (over
  all runs, wins and losses) must be at least the floor in the table:
  50 (e2), 70 (e3), 90 (e4) and 110 (e5). Moves, not time used,
  because bots move every 1.5 s and moves resolve without animation:
  a human at about 3.5 s a move takes sim moves × 3.5 s, so e1-l01's 18
  moves are about 63 s, inside its brief. Episode 1 has no floor; its
  briefs ask for quick first wins. `--check` counts the floor as part of
  the `greedy`/normal target (WP-56).
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
  last (below). Chains are a weak lever: the simulator applies their rule
  (a chained piece cannot be swapped; a run through it frees it), but a
  run soon passes through each one. On e3-l04 with no upgrades, its ten
  chains moved `greedy` from 100 % to 98.5 % and its median from 149 s
  to 141 s on normal, and from 86.5 % to 84 % on hard; even 32 chains
  gave only 97.5 % and 129.5 s.

## Clock

A level's clock must cover a human's pace, not just the bots'. Bots move
every 1.5 s; a human takes about 3.5 s a move. For each difficulty:

```
required clock = ceil(greedy mean moves × 3.5 s × 1.15 / 10) × 10
```

with `greedy`'s mean moves from the `--check` run (200 seeds, `auto`
upgrades) and a 15 % margin, rounded up to whole 10 s. The level's base
clock at timer tier 0, a player who bought no timer upgrade, must reach
it. Each level's `base_clock` is the larger of 270 s and the required
clock; a level whose requirement fits in 270 s leaves `base_clock` out.
Timer upgrades add 30 s a tier on top (`GOAL-03`), so they always buy
time. A longer clock lets losing runs play longer, which raises the mean
moves, so a level's clock is set again until it stops changing.
`--check` fails a level and difficulty whose base clock is below the
required clock (the `clock` lines).

## Current results

`montezuma_sim game/data/levels --check`, 200 seeds, `auto` upgrades.
Win rates in %, median time left in seconds.

| Level | `greedy` normal | `greedy` median | `greedy` moves | `random` normal | `planner` hard | Result |
|---|---|---|---|---|---|---|
| e1-l01 | 100 | 246.0 | 18.1 | 100 | 100 | pass |
| e1-l02 | 100 | 264.0 | 26.6 | 100 | 100 | pass |
| e1-l03 | 100 | 255.0 | 32.8 | 100 | 100 | pass |
| e1-l04 | 100 | 241.0 | 41.9 | 100 | 100 | pass |
| e1-l05 | 100 | 246.2 | 49.0 | 100 | 100 | pass |
| e2-l01 | 100 | 256.5 | 59.3 | 100 | 100 | pass |
| e2-l02 | 100 | 247.5 | 68.1 | 100 | 100 | pass |
| e2-l03 | 100 | 237.5 | 77.0 | 99 | 100 | pass |
| e2-l04 | 100 | 239.2 | 73.9 | 99.5 | 100 | pass |
| e2-l05 | 100 | 242.8 | 69.9 | 100 | 100 | pass |
| e2-l06 | 100 | 235.5 | 76.6 | 100 | 100 | pass |

Both curve rules pass for episodes 1 and 2. Each level meets its row with
no obstacles and no time override, so no lever was used. Every episode 2
level clears the moves floor of 50 (59 to 77) at the gem goals in
[progression](progression.md); every level wins every `greedy` run, so
the curve rule holds by the 100 % tie.

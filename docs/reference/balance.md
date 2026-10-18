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

| Levels | `greedy` win rate, normal | `greedy` median time left | `random` win rate, normal | `planner` win rate, hard |
|---|---|---|---|---|
| e1-l01 | 100 % | ≥ 150 s | ≥ 95 % | ≥ 99 % |
| rest of e1 | ≥ 98 % | ≥ 100 s | ≥ 70 % | ≥ 90 % |
| e2 | ≥ 95 % | ≥ 70 s | ≥ 40 % | ≥ 80 % |
| e3 | ≥ 90 % | ≥ 50 s | ≥ 20 % | ≥ 70 % |
| e4 | ≥ 85 % | ≥ 35 s | ≤ 20 % | ≥ 60 % |
| e5 | ≥ 80 % | ≥ 25 s | ≤ 10 % | ≥ 50 % |

- Too easy: from episode 2 on, `greedy`'s mean moves on normal must be
  at least 50 (e2), 70 (e3), 90 (e4) and 110 (e5). Moves, not time used,
  because bots move every 1.5 s and moves resolve without animation:
  a human at about 3.5 s a move takes sim moves × 3.5 s, so e1-l01's 18
  moves are about 63 s, inside its brief. Episode 1 has no floor; its
  briefs ask for quick first wins. The simulator does not check this
  floor yet; read it from the `moves` column.
- No level may be harder than the next by more than 10 points of
  `greedy` win rate, except a level that introduces a new idea, which
  must be easier than the level before it (the rest after a spike). Two
  levels that both win every run count as easier: 100 % is the ceiling,
  and every episode 1 level introduces an idea. There is no tiebreak on
  median time left: episode 1 is a teaching ramp whose gem goal rises by
  5 a level, so it does not follow the spike-and-rest shape, and a time
  tiebreak would need flat or falling goals there.
- The first level of each episode is a rest: its `greedy` win rate is at
  least that of the episode's third level.
- Tuning levers, in this order: chains and `steel_rate`, then the gem
  goal (in steps of 5, keeping the +5 curve within ±10), then the time
  override. Change one at a time and record before and after in the PR.

## Current results

`montezuma_sim game/data/levels --check`, 200 seeds, `auto` upgrades.
Win rates in %, median time left in seconds.

| Level | `greedy` normal | `greedy` median | `random` normal | `planner` hard | Result |
|---|---|---|---|---|---|
| e1-l01 | 100 | 246.0 | 100 | 100 | pass |
| e1-l02 | 100 | 264.0 | 100 | 100 | pass |
| e1-l03 | 100 | 255.0 | 100 | 100 | pass |
| e1-l04 | 100 | 241.0 | 100 | 100 | pass |
| e1-l05 | 100 | 246.2 | 100 | 100 | pass |

Both curve rules pass for episode 1. Each level meets its row with no
obstacles and no time override, so no lever was used.

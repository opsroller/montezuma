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

- No level may be harder than the next by more than 10 points of
  `greedy` win rate, except a level that introduces a new idea, which
  must be easier than the level before it (the rest after a spike).
- The first level of each episode is a rest: its `greedy` win rate is at
  least that of the episode's third level.
- Tuning levers, in this order: chains and `steel_rate`, then the gem
  goal (in steps of 5, keeping the +5 curve within ±10), then the time
  override. Change one at a time and record before and after in the PR.

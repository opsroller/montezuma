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
  last ([Clock](#clock)). Measured chain effect since chains hold
  (`OBS-05`, WP-59), e3-l04 with no upgrades on the default 270 s
  clock, 200 seeds, `greedy`: no chains 100 % normal, 149 s median,
  123.1 moves, 86.5 % hard, 77.5 s; its ten chains 99 %, 138.2 s, 130.3
  moves, 82.5 %, 62.5 s; 32 chains (the bottom four rows) 94 %, 114 s,
  136.4 moves, 65 %, 49.5 s. Before chains held, the ten cost 1.5 points
  and 8 s on normal and 2.5 points and 8 s on hard, the 32 cost 2.5 points
  and 19.5 s on normal and 11.5 points and 21 s on hard
  ([what the simulator measures](../explanation/about-the-episode-3-levels.md#what-the-simulator-measures-here)).

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
instead of 310/470 s, e3-l08 730/1100 s instead of 540/820 s (the no-upgrade figure was not
measured again after WP-59). Why the rule
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
| e3-l01 | 100 | 293.0 | 83.6 | 100 | 100 | 340 / 530 | pass |
| e3-l02 | 100 | 379.0 | 93.0 | 100 | 100 | 380 / 560 | pass |
| e3-l03 | 100 | 382.5 | 96.8 | 100 | 100 | 390 / 600 | pass |
| e3-l04 | 100 | 445.2 | 105.2 | 100 | 100 | 430 / 640 | pass |
| e3-l05 | 100 | 448.2 | 107.5 | 100 | 100 | 440 / 680 | pass |
| e3-l06 | 100 | 498.8 | 127.4 | 100 | 100 | 520 / 800 | pass |
| e3-l07 | 100 | 533.8 | 146.4 | 100 | 100 | 590 / 910 | pass |
| e3-l08 | 100 | 498.5 | 133.8 | 100 | 100 | 540 / 820 | pass |
| e4-l01 | 100 | 522.5 | 137.2 | 100 (info) | 100 | 560 / 870 | pass |
| e4-l02 | 100 | 544.0 | 143.7 | 100 (info) | 100 | 580 / 870 | pass |
| e4-l03 | 100 | 579.0 | 155.0 | 100 (info) | 100 | 630 / 960 | pass |
| e4-l04 | 100 | 588.8 | 160.1 | 100 (info) | 100 | 650 / 980 | pass |
| e4-l05 | 100 | 588.5 | 156.2 | 100 (info) | 100 | 630 / 980 | pass |
| e4-l06 | 100 | 643.5 | 170.4 | 100 (info) | 100 | 690 / 1050 | pass |
| e4-l07 | 100 | 670.2 | 173.9 | 100 (info) | 100 | 710 / 1080 | pass |
| e4-l08 | 100 | 869.0 | 183.1 | 100 (info) | 100 | 740 / 1110 | pass |
| e4-l09 | 100 | 846.0 | 162.7 | 100 (info) | 100 | 660 / 1020 | pass |
| e4-l10 | 100 | 836.8 | 164.4 | 100 (info) | 100 | 670 / 1010 | pass |
| e5-l01 | 100 | 886.5 | 171.0 | 100 (info) | 100 | 690 / 1060 | pass |
| e5-l02 | 100 | 824.2 | 155.0 | 100 (info) | 100 | 630 / 950 | pass |
| e5-l03 | 100 | 833.0 | 159.0 | 100 (info) | 100 | 640 / 980 | pass |
| e5-l04 | 100 | 864.0 | 163.2 | 100 (info) | 100 | 660 / 1010 | pass |
| e5-l05 | 100 | 1009.0 | 163.4 | 100 (info) | 100 | 660 / 1020 | pass |
| e5-l06 | 100 | 1010.8 | 173.5 | 100 (info) | 100 | 700 / 1070 | pass |
| e5-l07 | 100 | 1019.2 | 176.9 | 100 (info) | 100 | 720 / 1100 | pass |
| e5-l08 | 100 | 1078.0 | 184.6 | 100 (info) | 100 | 750 / 1140 | pass |
| e5-l09 | 100 | 1077.8 | 185.5 | 100 (info) | 100 | 750 / 1160 | pass |
| e5-l10 | 100 | 1124.2 | 194.1 | 100 (info) | 100 | 790 / 1190 | pass |
| e5-l11 | 100 | 1100.8 | 187.9 | 100 (info) | 100 | 760 / 1170 | pass |
| e5-l12 | 100 | 1126.5 | 194.8 | 100 (info) | 100 | 790 / 1190 | pass |

Both curve rules and every clock line pass for episodes 1 to 5. Each
level's base clock is the larger of 270 s and its [clock rule](#clock)
value. Medians are time left on the base clock plus the `auto` timer
tier (+30 s from e1-l02, +60 s from e3-l04, +90 s from e4-l06, +120 s
from e5-l08; the [time totem](rules-totems.md#numbers) gains a second
use from e4-l08 and +240 s per use (tier 4) from e5-l05). Every
episode 2 level clears the moves floor of 50 (59 to 77) and every
episode 3 level the floor of 70 (84 to 146), at the gem goals in
[progression](progression.md); every level wins every `greedy` run, so
the curve rule holds by the 100 % tie. Every episode 4 level clears the
moves floor of 90 (137 to 183) and wins every `greedy` run; `random` is
informational there and wins every run on the human-pace clocks. Every
episode 5 level clears the moves floor of 110 (155 to 195) and wins
every `greedy` run; its base clocks run 630 to 790 s on normal and 950
to 1190 s on hard. Why episodes 3, 4 and 5 are built and measured this
way is in [about the episode 3
levels](../explanation/about-the-episode-3-levels.md), [about the
episode 4 levels](../explanation/about-the-episode-4-levels.md) and
[about the episode 5
levels](../explanation/about-the-episode-5-levels.md).

`greedy` on normal and `planner` on hard win every run on every level,
so the curve rules pass only by the 100 % tie: the win-rate curve is
unverified, and the human targets in [progression](progression.md) carry
it until a playtest. The moves floor and the clocks are still measured.

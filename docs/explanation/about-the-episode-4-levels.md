<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# About the episode 4 levels

Explanation, for designers and reviewers: why episode 4's levels are
ordered and built the way [progression](../reference/progression.md#episode-4-levels)
lists them, and what the simulator can and cannot tell us about them.
Targets are in [balance](../reference/balance.md); the ward totem is
`TOT-14` in the [totem rules](../reference/rules-totems.md) and the
obstacles are `OBS-*` in the [rules spec](../reference/rules-spec.md).
Episode 3's reasoning, which this episode builds on, is in
[about the episode 3 levels](about-the-episode-3-levels.md).

## One new rule across ten levels

Episode 4 brings only one new rule, the `ward` totem (e4-l01), so nine of
its ten levels develop, twist, test or rest. Ward is a single answer to
steel: one use a level until tier 3 in episode 5, fired by two blue
matches in a row whether the player meant it or not. The episode
therefore teaches *when* to spend it before it teaches what it cannot
do:

- e4-l01 and e4-l02 have steel and no chains, so the player first sees
  ward clear every obstacle on the board, then learns to hold blue back
  until steel has gathered.
- e4-l03 brings chains back, and the first ward that meets them leaves
  them standing: ward is for steel only.
- The rest of the episode varies the chains' shape, each with its own
  lesson: a locked floor (e4-l04), columns that split the board
  (e4-l06), single locks far apart that blasts rarely reach two of
  (e4-l07), clusters where a blast pays off (e4-l08), and chains high on
  the board that fall into heavy steel (e4-l10).

Rests follow the first test and the spike (e4-l05 after e4-l04, e4-l09
after e4-l08); the finale, e4-l10, is followed by the Memory 2 bonus
level and episode 5's opener. Both obstacles reach
the guidance maximums (`steel_rate` 0.06, 10 chains) only on e4-l08 and
e4-l10, so the spike and the finale ask more than the first test.

A chain pattern above the bottom row only holds at the start: chained
pieces fall like any other (`OBS-05`). Levels whose lesson needs the
pattern to last keep their chains on the bottom rows; e4-l07 and e4-l10
use the drift on purpose. Since chains hold (WP-59), a pattern also
lasts longer than it did: a run through a link breaks the chain but
leaves the piece in place, so a wall such as e4-l06's columns needs two
hits per link to open. Neither the simulator nor balance.md measures
how long a pattern lasts, so that is a playtest question.

## Why random has no target

Balance.md used to ask that `random` win at most 20 % of runs in episode
4 and 10 % in episode 5, as a sign the levels need skill. At the
progression's goals that cannot hold. Probes on an e4-l01-shaped board, with the `auto` upgrades,
gave `random` 92 to 96 % at 105 gems and 34 to 40 % at 150 gems on the
timer tier alone (330 s, no per-level clock), and steel up to 0.15 barely changed it. Under the
[level clocks](about-level-clocks.md), sized for a human's pace,
`random` wins every run: a bot that moves every 1.5 s gets several
hundred moves on a clock meant for a person at 3.5 s a move. Holding
the ceiling would mean clocks too short for people, which is what the
clock rule exists to prevent. So `random` is informational from
episode 4 on, and the moves floor (90 in e4) is the check that a level
is not too easy.

## What the simulator measures here

As in episode 3, obstacles move the simulator little: `greedy` wins
every or nearly every run on each level, so the curve rules hold by the
100 % tie or within a point. The climb shows in `greedy`'s mean moves,
which rise with the gem goal until e4-l08, drop at e4-l09 and climb
only a little at e4-l10: the simulator
buys every upgrade as soon as it can, and ward tier 2 (no new steel
after the ward), color_clear tier 3 and harvest tier 3 (removes every
chain) arrive at e4-l09 and e4-l10. A player who skips them meets
longer levels; [about level clocks](about-level-clocks.md) explains why
the clock rule still uses the `auto` upgrades. e4-l03 keeps its steel
light (0.02) so that its twist is no harder than the level before it:
`greedy` wins every run there.

The human first-try rates in the briefs carry the sawtooth the
simulator cannot show, and they are provisional until a playtest.

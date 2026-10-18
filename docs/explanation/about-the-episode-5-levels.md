<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# About the episode 5 levels

Explanation, for designers and reviewers: why episode 5's levels are
ordered and built the way [progression](../reference/progression.md#episode-5-levels)
lists them, and what the simulator can and cannot tell us about them.
Targets are in [balance](../reference/balance.md); the obstacles are
`OBS-*` in the [rules spec](../reference/rules-spec.md) and the totems
`TOT-*` in the [totem rules](../reference/rules-totems.md). Episode 4's
reasoning, which this episode builds on, is in
[about the episode 4 levels](about-the-episode-4-levels.md).

## A final episode with no new rule

Episode 5 is the campaign's longest (twelve levels, `PROG-01`) and brings
no new rule: every totem, power-up and obstacle is already known by
e4-l10. What arrives are optional upper tiers (gold, fire, time, paint,
gift and harvest tier 4, hint and clock tier 3, ward tier 3, 8 gems on
the board, Score Frenzy's 30 s, the timer's +120 s), so every level develops, twists, tests
or rests on earlier lessons. Its difficulty has to come from two places:

- **The goal.** The normal gem goal keeps its +5 a level (hard rises
  with it, `DIFF-03`) until both reach the cap of 200 on normal and 300
  on hard at e5-l10 (`PROG-08`). e5-l10,
  e5-l11 and e5-l12 share that goal, so from e5-l10 on the obstacles
  alone set how hard a level is.
- **The obstacles.** Steel and chains stay inside the guidance range
  (`steel_rate` 0.02 to 0.06, 2 to 10 chains) but reach both maximums together
  on the spike (e5-l08) and the finale (e5-l12); the tests e5-l04 and e5-l10 stay under
  the steel maximum (e5-l04 with steel 0.05 and 8 chains, e5-l10 with
  steel 0.05 and the chain maximum of 10). The chains take shapes the campaign has not
  used: a diagonal stair (e5-l02), a whole top row that falls through
  the board (e5-l03), a full row across the middle that no swap can
  enter (e5-l04), a U on the lower half where links have no outer
  neighbour (e5-l06), a chessboard floor where no two links are side by side
  (e5-l08), a block in the centre where most runs happen (e5-l10), and
  chains in five rows at once (e5-l12).

## The shape of the episode

The sawtooth has four rests: the opener (e5-l01, a rest by load, at
least as easy as e5-l03 by the curve rule), e5-l05 after the first test
and the Reaction 2 bonus level, e5-l09 after the spike and the Attention
2 bonus level, and e5-l11 between the capped-goal test and the finale.
The rests are by obstacle load, never by length, and each asks for a
different habit: the episode 4 play at a bigger goal (e5-l01),
gem-carrying runs of 4 with 8 gems on the board (e5-l05), planning two
totems at once (e5-l09), and building power-ups while falling links come
down by themselves (e5-l11).

e5-l07 is the one level built around an optional totem tier: ward tier 3 also
clears the pieces next to destroyed steel, so steel that lands beside a
chain becomes a way to free it. Tier 2's "no new steel after the ward"
stays at tier 3, so the lesson is timing the first ward; a player
without tier 3 meets the same choice with tnt and lightning.

## What the simulator measures here

As in episodes 3 and 4, `greedy` and `planner` win every run, so the
curve rules hold by the 100 % tie and the moves floor (110) and the
clocks are the measured checks. The simulator buys every tier as soon as
it can, including harvest tier 3 (removes every chain, from e4-l10) and
ward tier 2 (no steel after the ward, from e4-l09), so it barely sees
the episode's chain shapes and heavy steel: its mean moves rise with the
goal, dip at e5-l02 where 8 gems on the board shorten every level, and
flatten from e5-l10 where the goal stops rising. The briefs' human
targets are for a player who skipped those obstacle removers, and they
carry the sawtooth the simulator cannot show. They follow the curve rule
that [balance](../reference/balance.md#targets) sets for `greedy` (no
level more than 10 points harder than the next) and are provisional
until a playtest. Why the clock rule still uses the simulator's
upgrades is in [about level clocks](about-level-clocks.md).

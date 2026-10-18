<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# About the episode 3 levels

Explanation, for designers and reviewers: why episode 3's levels are
ordered and built the way [progression](../reference/progression.md#episode-3-levels)
lists them, and what the simulator can and cannot tell us about them.
Targets are in [balance](../reference/balance.md); the obstacle rules are
`OBS-*` in the [rules spec](../reference/rules-spec.md).

## Two obstacles across eight levels

Episode 3 brings two ideas, chains (e3-l01) and steel (e3-l06), so most of
its levels practise or test rather than introduce. Chains get five levels,
each asking something different of the same piece: a fixed lock (bottom
row, nothing below it), a falling lock (top half), a dense band that one
blast can break, a packed test, and a rest with only two. Steel then
arrives on a level with no chains, so the player meets one new obstacle at
a time, and the last two levels put both on the board together.

Chains are placed only on the starting board (`OBS-05`) and fall like any
piece, while steel only arrives later from refills (`OBS-02`). A chain
high on the board can drift or leave early, so levels that must show both
obstacles at once keep their chains on the bottom rows, where nothing
below them can make them fall.

Each level also makes one or two tier-2 upgrades buyable. They are
optional purchases (`PROG-04`), so no level's first contact relies on
them.

## What the simulator measures here

Chains barely move the simulator: on e3-l04 with no upgrades, its ten
chains lower `greedy`'s win rate on normal from 100 % to 98.5 % and its
median time left from 149 s to 141 s
([balance](../reference/balance.md)), because a chain goes as soon as a
run passes through it. Episode 3's simulated
difficulty therefore comes from the gem goal and, more weakly, from
`steel_rate` (raising e3-l08's from 0.05 to 0.06 barely changed its
results). The climb shows in `greedy` mean moves (82 to 145). Before the levels had
their own clocks, `random` also fell on hard, from 78 % on e3-l05 to 17 %
on e3-l07; with human-pace clocks it wins every run on both difficulties
([about level clocks](about-level-clocks.md)). `greedy` wins every
run on every level, so the curve rules hold by the 100 % tie rather than
by a measured sawtooth; chains are a teaching device more than a
difficulty lever.

The finale, e3-l08, has the biggest goal and the most obstacles, yet takes
fewer simulated moves than e3-l07: it is where the seventh gem on the
board becomes available, and the simulator buys every tier as soon as it
can. A player who has not bought it meets a longer finale. Its brief
therefore promises the most obstacles, not the longest level.

## Human time against the clock

[Balance](../reference/balance.md) converts simulator moves to human time
at 3.5 s a move. At that pace every episode 3 level outlasts the old
270 s clock, so each now carries its own base clock from the
[clock rule](../reference/balance.md#clock): the moves `greedy`'s wins
take, at 3.5 s a move plus 15 %, without the timer upgrade. Episode 3's
clocks run from 340 s (e3-l01 normal) to 900 s (e3-l07 hard) and follow
the moves: they rise with the gem goal and dip on the finale, which takes
fewer moves for the reason above. Each timer tier adds 30 s on top, so
upgrades stay worth buying.

The rule assumes a player who owns the gem and item upgrades on offer.
One who skips them makes more moves and leans on the time totem (+60 s,
one use) and clock power-ups (+15 s each). Until a playtest settles the
real pace, the human first-try rates in the briefs are provisional; if
players still run out of time, the pace or margin in the clock rule is
what changes, and every level's clock follows from it.

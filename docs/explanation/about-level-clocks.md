<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# About level clocks

Explanation, for game designers and programmers: why each level has its
own base clock, why timer upgrades sit on top of it, and why the
[clock rule](../reference/balance.md#clock) counts what it counts. The
field is in [level data](../reference/level-data.md#timer-tiers); how to
set it is in [add a level](../how-to/add-a-level.md).

## A clock for people, not bots

The simulator's bots move every 1.5 s and their moves resolve without
animation. A person looks, thinks and waits for cascades, about 3.5 s a
move. Every level used to share one 270 s base, which covered episode 1 but not
the longer levels after it: at a human's pace e2-l03 on hard needs about
490 s. The clock rule turns the simulator's move count into human time,
adds 15 % so an average player is not cut off at the last move, and
rounds up to 10 s so clocks read cleanly.

## Upgrades on top

A level that set its own time used to replace the timer tier, so a
player who bought a timer upgrade got nothing for it on that level. Now
every level has a base clock and each timer tier adds 30 s to it, so the
upgrade buys time everywhere. The rule judges the base clock alone,
timer tier 0, so the level is fair to a player who bought no timer
upgrade, and an upgrade is a bonus rather than a requirement.

## Why only wins count

A losing run plays until the clock runs out, so its move count is set by
the clock itself: about one move per 1.5 s. Counting losses would make
the requirement grow with every second added, about 2.7 s per second on
a level `greedy` mostly loses, and the clock would never settle. The moves
a win takes are what a player needs to finish, so the rule uses those.
A longer clock can still turn a narrow loss into a long win, which is why
the clock is set again until it stops changing; so far that has taken
one extra pass. A difficulty that `greedy` never wins has no clock that
fits, so it fails the rule rather than asking for ever more time.

## Which player

The moves come from a player who owns the gem and item upgrades on offer
(the `auto` upgrades), because that is how the targets are measured. A
player who skips them makes more moves; with no upgrades at all the
clocks would be much longer (e3-l08 on hard: 1100 s instead of 800 s).
That player leans on the time totem and the clock power-up.

A longer clock also helps the `random` bot: with human-pace clocks it
wins every episode 3 run on normal
([current results](../reference/balance.md#current-results)). For the
same reason `random` has no target from episode 4 on: a ceiling on it
could only be met with clocks too short for people
([why](about-the-episode-4-levels.md#why-random-has-no-target)).

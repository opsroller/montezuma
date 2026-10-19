<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# About the rules

Why *Montezuma's* plays the way the [rules spec](../reference/rules-spec.md)
says, which version of the original it follows, and every place it
chooses or deviates. Evidence ids `[S*]` are in
[sources](../reference/rules-sources.md).

## Context

The project's direction is a remake that keeps the original's feel. The
first draft of the spec was written from store listings and memory, and
guessed three systems that matter most to the feel: what a level asks of
the player, how totems charge, and what bonuses are. Reading walkthroughs,
database entries and reviews showed all three guesses were wrong.

## Which original

The PC release is the best documented: one walkthrough lists every
level's unlocks (S1), and two reviews and a database entry agree with it
(S3, S4, S5). The iOS listing of 2009 (S6) gives different counts (53
levels, 6 totems, 8 bonuses) but no details. We follow the documented
version: 41 levels, 7 totems, 6 power-ups. Rejected: inventing 12 extra
levels and dropping a totem to match S6's counts without knowing which.

## The three corrections

**The goal is gems, not colour quotas.** Four sources agree that some
pieces carry gems and a level asks for a number of gems before the timer
ends, with the number rising and the time staying put. This is what
gives the original its rhythm: the player hunts the few pieces that
matter instead of matching anything. Colour quotas stay available as a
variant for other rule sets (`GOAL-06`), because they cost nothing to
keep and suit some mini games.

**Totems belong to colours and fire on two moves in a row.** Every
source that explains totems says the same: two consecutive matches of the
totem's colour. This makes a totem a small plan ("match red now so that
red next move fires"), which is a meaningful, visible choice. The earlier
model (one shared streak, different thresholds) asked for long streaks
that a random board rarely gives.

**Bonuses are power-ups on the board.** They appear on pieces after long
matches and go off when those pieces are cleared (S4, S5). Held,
targeted bonuses (hammer, freeze) are a different game; they are dropped.

## Choices where the sources are silent

| Choice | Reason |
|---|---|
| Only the swap's own match charges a totem (`TOT-02`) | Keeps the totem a player decision, not luck; cascades would fire totems by accident |
| Totems fire one at a time, each on the board the last left (`TOT-05`) | Each effect is readable on its own, and no effect targets pieces that have moved |
| Board 8 × 8 | Not found; the genre's standard, and it fits the project's budgets |
| 10 × (step + 1) points per piece, 50 per gem | Not found; keeps cascades worth more, and score never decides a win |
| First gem goal 10 | S3 gives +5 per level up to 200; starting at 10 reaches 200 at level 39 of 41 |
| Base timer 270 s, tiers of 30 s | S1 gives the tiers from 300 s; the base is one step below |
| Frenzy bar 60 pieces | About 5 to 8 moves, so Frenzy comes two or three times a level |
| Runs of 4 give 1 power-up, 5+ give 2, at most 4 on the board | S4 names 4 and 5 matches; the cap stops the board filling with them |
| Chains from e3-l01, steel from e3-l06 | Each arrives one to five levels before its counter (harvest tier 3, ward), and episode 3 is where S1's upgrades for them begin |
| Steel cleared only by effects | Gives the power-ups and ward a job; a matchable steel would be a plain piece |
| Hard = normal × 1.5 gems | S3 says higher goals and a faster pace; more gems in the same time is that pace |
| Upgrade tiers cost 1 star; tier 1 is free | S1 has stars buy tiers; free tier 1 lets each new idea arrive without a shopping step |
| Idle hint on the first two levels | Onboarding for the web demo's first-time players |
| Trophies | The original's nine were not found, so these are our own |

Each of these is a lever: the simulator (WP-22) and playtests may tune the
numbers within the same rules.

## Deliberate improvements

- A move that started before time ran out finishes and can still win
  (`GOAL-02`): losing to an animation feels unfair.
- Large tap targets in the bonus games (`BG-10`) for touch screens.

## Consequences

- The LevelData contract changes: gem goals, optional quotas, optional
  base clock (`base_clock`, which the timer tier adds to), `normal` and `hard`, power-up unlocks, obstacles, and
  rule-set and theme ids. See the PR that adds this page.
- The core's totem and bonus code (WP-06, WP-07) changes model, and the
  ids change: totems `fire`, `time`, `gold`, `harvest`, `ward`, `gift`,
  `paint`; power-ups `tnt`, `color_clear`, `lightning`, `clock`,
  `multiplier`, `hint`.
- The HUD shows a gem counter, seven totem meters and a Frenzy bar;
  quota counters only when a level has quotas.
- The [chosen] numbers are not checked against footage of the original:
  the owner stopped study of the original on 2026-10-19, so the feel pass
  (WP-40) sets them from our own targets and playtests.

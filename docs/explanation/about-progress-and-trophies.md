<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# About progress and trophies

Why the player's profile works the way it does. The interface is in the
[progress reference](../reference/progress.md); the rules are in
[progression](../reference/progression.md).

## A profile is a record of wins, not of unlocks

The save stores what the player did: wins, best scores, bonus results,
tiers bought, totems fired, trophies held. What is open, and which free
tiers are owned, is worked out from those every time. The save has no
list of open levels to get out of step with the wins, and a change to the
unlock tables applies to old saves at once. (The loader checks each value
on its own, so a hand-edited save that claims a win still loads as
written.)

## Choices the spec left open

Each sits in one place in `progress.cpp`, so the game designer can change
it without touching the rest.

- **Unlocks per difficulty.** Hard opens only once e5-l12 is won on normal
  (`DIFF-02`). From there winning a level on hard opens the next one on
  hard: hard is its own run through the campaign, which is what makes
  `legend` worth more than `explorer`.
- **"Reached" a tier's level** (`PROG-04`) means that level is open on
  either difficulty. Upgrades are shared by both difficulties
  (`PROG-05`), so the furthest a player has got on either one counts.
- **Bonus stars on the first play only.** `BG-01` says a bonus level is
  played once, and `BG-02` says replays give no star whether the first
  play passed or failed. So a failed first play never gives the star
  later. A later pass still counts toward `sharp_eye`, so practice is not
  wasted.
- **Best score from any play.** Score never decides a win (`SCORE-05`),
  so a high score in a lost play is still the player's best.
- **Bonus level ids** are `<game>-<tier>`, from the game ids in the
  contracts and the two appearances of each game.

## How trophies are counted

Trophies split in two. Those about a single play (`chain_reaction`,
`demolition`, `frenzy`) need the move-by-move reports, so `TrophyWatch`
reads them while the level is played. Those about the profile
(`first_find`, `totem_keeper`, `sharp_eye`, `explorer`, `legend`) and
`quick_hands`, which needs only the final outcome and clock, are decided by
`Progress` from what it records. `Progress` ignores profile trophies in a
play's list, so a play can't award them.

- `chain_reaction` counts only the swap's own cascade steps. Totem steps
  carry on the move's step numbers, and counting them would let a totem
  firing late in a short move look like a long chain.
- `demolition` counts everything the move cleared, totems included: from
  the player's seat it is all one move's blast.
- `frenzy` counts the star sand a move gains, because `FRZ-04` gives one
  sand for each gem collected in Frenzy, so the two are the same number
  and the session already reports it. A gold-totem restart (`TOT-12`)
  extends the Frenzy the player is in, so its count carries on.

## Why the save is strict

A save that loads half-way is worse than none: the player would lose
stars or tiers without being told. So `try_from_json` takes the whole
file or nothing, names the field it rejected, and never throws, which
lets the engine side keep its no-exceptions boundary. For the same reason
the star count stops at the largest value the save accepts instead of
wrapping past it, so every file `to_json` writes loads again. The `version` field
lets a later format read this one instead of guessing.

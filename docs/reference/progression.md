<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Progression

The design reference for the `classic` campaign: episodes, levels, gem
goals, what unlocks when, upgrades, golden stars, trophies and the intro
level. Terms and certainty marks are in the [rules spec](rules-spec.md);
sources in [sources](rules-sources.md). Level files follow the LevelData
contract and live in `game/data/levels/e<N>/l<NN>.json`.

## Rules (`PROG-*`)

1. `PROG-01` Five episodes of 5, 6, 8, 10 and 12 levels, 41 in all, played
   in order [S1, S3, S7].
2. `PROG-02` Winning a level for the first time on a difficulty awards 1
   golden star; passing a bonus level for the first time awards 1
   [S1, S5].
3. `PROG-03` Tier 1 of a totem or power-up, and Score Frenzy's tier 1,
   are granted free when their unlock level starts [chosen]. Every timer
   and gems-on-board tier costs a star.
4. `PROG-04` Every higher tier costs 1 golden star and can be bought on
   the upgrade screen before any level once the player has reached the
   tier's level [S1, S5, chosen cost].
5. `PROG-05` Upgrades are kept for the whole profile, in both
   difficulties, and never lost.
6. `PROG-06` Every 50 star sand turns into 1 golden star at once
   [S1, chosen rate].
7. `PROG-07` A lost level costs nothing and can be retried at once.
8. `PROG-08` Gem goal on `normal`: 10 at the first level, 5 more each
   level, at most 200 [S3; first value chosen]. `hard` per `DIFF-03`, at
   most 300 [chosen].

Star budget: 41 levels and 8 bonus levels give 49 stars, plus star sand.
All tiers above tier 1 cost 42 stars, so a player who wins everything once
can own every tier by the end, and a player who skips bonus levels is a
few tiers short.

## Upgrades

Board items: the base value a new player has, then the tiers and the
level each becomes available:

| Item | Base | Tier 1 | Tier 2 | Tier 3 | Tier 4 | Source |
|---|---|---|---|---|---|---|
| Timer | 270 s | 300 s (e1-l02) | 330 s (e3-l04) | 360 s (e4-l06) | 390 s (e5-l08) | [S1]; base chosen |
| Gems on board | 5 | 6 (e2-l05) | 7 (e3-l08) | 8 (e5-l02) | — | [S1] |
| Score Frenzy | — | 15 s (e1-l04, free) | 20 s (e3-l03) | 25 s (e4-l05) | 30 s (e5-l07) | [S1] |

## Unlocks

Level at which each tier of a totem or power-up becomes available
[S1]. Tier 1 is free (`PROG-03`); later tiers cost 1 star.

| Item | Tier 1 | Tier 2 | Tier 3 | Tier 4 |
|---|---|---|---|---|
| `tnt` | e1-l01 | e3-l01 | e4-l08 | — |
| `color_clear` | e1-l02 | e3-l06 | e4-l09 | — |
| `lightning` | e1-l04 | e1-l05 | e4-l01 | — |
| `clock` | e2-l01 | e4-l02 | e5-l04 | — |
| `multiplier` | e2-l02 | e3-l05 | e4-l10 | — |
| `hint` | e2-l06 | e4-l03 | e5-l03 | — |
| `fire` | e1-l03 | e3-l03 | e4-l04 | e5-l02 |
| `time` | e1-l05 | e3-l02 | e4-l08 | e5-l05 |
| `gold` | e2-l01 | e3-l06 | e4-l03 | e5-l01 |
| `gift` | e2-l03 | e3-l05 | e4-l07 | e5-l09 |
| `harvest` | e2-l04 | e3-l07 | e4-l10 | e5-l09 |
| `paint` | e2-l05 | e3-l07 | e4-l06 | e5-l06 |
| `ward` | e4-l01 | e4-l09 | e5-l07 | — |

Obstacles: chains from e3-l01, steel from e3-l06 [chosen, so that each
arrives with its first counter one or two levels later].

## Episodes

`#` is the level's number in the campaign. "New" is the one idea the
level introduces (the original's unlock order; where it lands two, the
second is a passive upgrade). Bonus levels follow the level they are
listed under [S1].

| Level | # | Gems normal | Gems hard | New | Bonus level after |
|---|---|---|---|---|---|
| e1-l01 | 1 | 10 | 15 | swapping, gems, `tnt` | |
| e1-l02 | 2 | 15 | 25 | `color_clear`, first upgrade (timer) | |
| e1-l03 | 3 | 20 | 30 | `fire` totem | |
| e1-l04 | 4 | 25 | 40 | `lightning`, Score Frenzy | |
| e1-l05 | 5 | 30 | 45 | `time` totem | Accuracy 1 |
| e2-l01 | 6 | 35 | 55 | `gold` totem, `clock` | |
| e2-l02 | 7 | 40 | 60 | `multiplier` | |
| e2-l03 | 8 | 45 | 70 | `gift` totem | |
| e2-l04 | 9 | 50 | 75 | `harvest` totem | |
| e2-l05 | 10 | 55 | 85 | `paint` totem | |
| e2-l06 | 11 | 60 | 90 | `hint` | Memory 1 |
| e3-l01 | 12 | 65 | 100 | chains | |
| e3-l02 | 13 | 70 | 105 | | |
| e3-l03 | 14 | 75 | 115 | | |
| e3-l04 | 15 | 80 | 120 | | Reaction 1 |
| e3-l05 | 16 | 85 | 130 | | |
| e3-l06 | 17 | 90 | 135 | steel | |
| e3-l07 | 18 | 95 | 145 | | |
| e3-l08 | 19 | 100 | 150 | | Attention 1 |
| e4-l01 | 20 | 105 | 160 | `ward` totem | |
| e4-l02 | 21 | 110 | 165 | | |
| e4-l03 | 22 | 115 | 175 | | |
| e4-l04 | 23 | 120 | 180 | | |
| e4-l05 | 24 | 125 | 190 | | Accuracy 2 |
| e4-l06 | 25 | 130 | 195 | | |
| e4-l07 | 26 | 135 | 205 | | |
| e4-l08 | 27 | 140 | 210 | | |
| e4-l09 | 28 | 145 | 220 | | |
| e4-l10 | 29 | 150 | 225 | | Memory 2 |
| e5-l01 | 30 | 155 | 235 | | |
| e5-l02 | 31 | 160 | 240 | | |
| e5-l03 | 32 | 165 | 250 | | |
| e5-l04 | 33 | 170 | 255 | | Reaction 2 |
| e5-l05 | 34 | 175 | 265 | | |
| e5-l06 | 35 | 180 | 270 | | |
| e5-l07 | 36 | 185 | 280 | | |
| e5-l08 | 37 | 190 | 285 | | Attention 2 |
| e5-l09 | 38 | 195 | 295 | | |
| e5-l10 | 39 | 200 | 300 | | |
| e5-l11 | 40 | 200 | 300 | | |
| e5-l12 | 41 | 200 | 300 | finale | |

Every level: 8 × 8 board, 7 kinds, the player's timer tier, every item
unlocked so far. Steel rate and chains per level are set in WP-23 to
WP-27 against the simulator targets in
[balance](balance.md); guidance: chains 2 to 10 cells,
`steel_rate` 0.02 to 0.06.

## Trophies

Nine trophies [S6, S7]; the original's list was not found, so these are
our own [chosen].

| Id | Name | Earned when |
|---|---|---|
| `first_find` | First Find | e1-l01 is won |
| `chain_reaction` | Chain Reaction | one move reaches step 5 (six clears in a row) |
| `demolition` | Demolition | one move clears 40 pieces |
| `totem_keeper` | Totem Keeper | each of the seven totems has fired at least once |
| `quick_hands` | Quick Hands | a level is won with at least 150 s left |
| `frenzy` | Frenzy | 12 gems are collected during one Score Frenzy |
| `sharp_eye` | Sharp Eye | all eight bonus levels are passed |
| `explorer` | Explorer | e5-l12 is won on `normal` |
| `legend` | Legend | e5-l12 is won on `hard` |

## Intro level

`e1-l01` is the first level of the campaign and the website's playable
demo (WP-34, WP-35). Brief:

- **Teaches:** swap to match, gems are the goal; a run of 4 makes a
  `tnt` the player sees go off.
- **Feeling:** an easy, quick first win with one surprise (the first
  `tnt`).
- **Target:** median 60 to 90 s; at least 95 % of first-time players win
  on the first try. Simulator: the greedy bot wins every seed with at
  least 150 s left.
- **Afterwards the player knows:** what a gem is, that bigger matches
  give power-ups, and that the timer matters later.
- **Help:** free move hint after 8 s idle (`SHUF-05`).

Data, in the proposed LevelData fields (see the contract change in the
PR that adds this page; the file itself lands with WP-23 once the parser
accepts the fields):

```json
{
  "id": "e1-l01",
  "episode": 1,
  "index": 1,
  "rules": "classic",
  "theme": "temple",
  "board": { "width": 8, "height": 8, "kinds": 7 },
  "gem_goal": { "normal": 10, "hard": 15 },
  "totems_unlocked": [],
  "powerups_unlocked": ["tnt"],
  "seed": 20090326
}
```

In the web demo the player has base upgrades (270 s, 5 gems) and no
stars; winning emits `level_completed`.

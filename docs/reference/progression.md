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
| Timer, added to the level's base clock (`GOAL-03`) | +0 s | +30 s (e1-l02) | +60 s (e3-l04) | +90 s (e4-l06) | +120 s (e5-l08) | [S1]; amounts chosen. The base clock is 270 s unless the level sets `base_clock` ([clock rule](balance.md#clock)) |
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

Every level: 8 × 8 board, 7 kinds, its base clock plus the player's timer tier, every item
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

Data, in the LevelData fields, in
[`game/data/levels/e1/l01.json`](../../game/data/levels/e1/l01.json):

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

## Episode 1 levels

Briefs for the rest of episode 1 (WP-23). Each level adds one idea to the
intro level's swap-and-collect loop and stays a quick win: episode 1
teaches, it does not test. Simulator targets are the "rest of e1" row in
[balance](balance.md); every level also has every item unlocked so far,
the player's timer tier and 5 gems on the board.

### e1-l02: Solidarity

- **Teaches:** a run of 5 makes two power-ups, and `color_clear` clears
  every piece of one kind; the timer has its first upgrade (300 s).
- **Feeling:** "bigger matches pay more", a second easy win that ends on a
  big clear.
- **Target:** median 90 to 120 s; at least 95 % of players win on the
  first try. Simulator: `greedy` wins every seed.
- **Afterwards the player knows:** runs of 4 and 5 are worth looking for,
  and upgrades buy more time.
- **Help:** free move hint after 8 s idle (`SHUF-05`), the last level
  with it.

### e1-l03: First totem

- **Teaches:** the `fire` totem charges on two moves in a row whose own
  match is red, then clears random pieces.
- **Feeling:** discovery: planning one move ahead in one colour pays off
  with fireballs.
- **Target:** median 100 to 140 s; at least 95 % of players win on the
  first try. Simulator: `greedy` wins every seed, though it never aims at
  the totem.
- **Afterwards the player knows:** totems exist, which colour feeds which
  totem, and that matching the same colour twice in a row fires it.

### e1-l04: Frenzy

- **Teaches:** `lightning`, which clears the column above its piece, and
  Score Frenzy: the bar fills with every cleared piece and doubles points
  for 15 s when full.
- **Feeling:** a rush: the first level where the board lights up on its
  own.
- **Target:** median 120 to 160 s; at least 95 % of players win on the
  first try. Simulator: `greedy` wins every seed.
- **Afterwards the player knows:** cascades and big clears fill the
  Frenzy bar, and Frenzy is the time to score.

### e1-l05: Borrowed time

- **Teaches:** the `time` totem, two orange moves in a row add 60 s; the
  level combines every episode 1 idea before the first bonus level.
- **Feeling:** the episode's finale, the first time the clock is worth
  watching, and the relief of winning time back.
- **Target:** median 140 to 180 s; at least 90 % of players win on the
  first try. Simulator: the "rest of e1" row.
- **Afterwards the player knows:** the timer can be refilled, and the
  episode's ideas work together.

| Level | File | Gems normal / hard | Totems | Power-ups | Seed |
|---|---|---|---|---|---|
| e1-l01 | [`l01.json`](../../game/data/levels/e1/l01.json) | 10 / 15 | | `tnt` | 20090326 |
| e1-l02 | [`l02.json`](../../game/data/levels/e1/l02.json) | 15 / 25 | | `tnt`, `color_clear` | 20090402 |
| e1-l03 | [`l03.json`](../../game/data/levels/e1/l03.json) | 20 / 30 | `fire` | `tnt`, `color_clear` | 20090403 |
| e1-l04 | [`l04.json`](../../game/data/levels/e1/l04.json) | 25 / 40 | `fire` | `tnt`, `color_clear`, `lightning` | 20090404 |
| e1-l05 | [`l05.json`](../../game/data/levels/e1/l05.json) | 30 / 45 | `fire`, `time` | `tnt`, `color_clear`, `lightning` | 20090405 |

No episode 1 level has chains or steel. e1-l05 sets `base_clock` 270/300 from the [clock rule](balance.md#clock).

## Episode 2 levels

Briefs for episode 2 (WP-24). Each level introduces one item, in the
original's unlock order, and keeps the one before it unlocked for a
second look; difficulty rises only with the +5 gem goal, so the episode
stays nearly flat and greedy wins every run on each level (two 100 %
levels count as easier under [balance](balance.md)'s curve rule).
Simulator targets are the e2 row there, including the moves floor of 50.
Power-ups come only from runs of 4 or more, drawn among the unlocked
ones; a level file cannot place a piece, so a new power-up's first
contact rests on those draws.

### e2-l01: Gold rush

- **Idea:** The gold totem: two yellow moves in a row add 1500 points; the clock power-up (from a run of 4+) adds 15 s.
- **Feeling:** relief: an easy episode opener that pays out.
- **Duration:** 175 to 225 s median (sim greedy about 50 to 65 moves at 3.5 s a move).
- **Target success rate:** Normal: at least 90 % of players win on the first try; hard: at least 75 %. Simulator: the e2 row of [balance](balance.md), with `greedy` expected to win every run. Its `greedy` win rate is at least e2-l03's (the opener is a rest).
- **Takeaway:** The player knows yellow feeds a points totem and that a clock on the board adds time.
- **Role and levers:** rest; goal size: 35 gems.
- **First contact:** Gold only adds points and the clock only adds time, so neither can hurt. Power-ups come from runs of 4 or more, which a player makes every few moves, and each draw is 1 in 4 among the unlocked ones, so a player usually meets the clock more than once in a level of 50+ moves.

### e2-l02: Times five

- **Idea:** The multiplier power-up: the move it goes off in scores x5.
- **Feeling:** greed: one big-number moment.
- **Duration:** 200 to 250 s median (sim greedy about 55 to 70 moves).
- **Target success rate:** Normal: at least 85 % first try; hard: at least 70 %. Simulator: the e2 row of [balance](balance.md), with `greedy` expected to win every run.
- **Takeaway:** The player knows a multiplier makes the move it goes off in score five times as much.
- **Role and levers:** introduce; goal size: 40 gems.
- **First contact:** Points never cost time, so a wasted multiplier costs nothing. Power-ups come from runs of 4 or more, which a player makes every few moves, and each draw is 1 in 5 among the unlocked ones, so a player usually meets the multiplier more than once in a level of 50+ moves. The clock from e2-l01 stays unlocked for a second look.

### e2-l03: Offerings

- **Idea:** The gift totem: two purple moves in a row place one power-up on the board.
- **Feeling:** a small gift: a tool appears out of nowhere.
- **Duration:** 220 to 270 s median (sim greedy about 65 to 80 moves).
- **Target success rate:** Normal: at least 85 % first try; hard: at least 70 %. Simulator: the e2 row of [balance](balance.md), with `greedy` expected to win every run.
- **Takeaway:** The player knows purple feeds a totem that puts a power-up on the board.
- **Role and levers:** introduce; goal size: 45 gems; complexity: four totems.
- **First contact:** A gift only adds a helpful piece; the gift totem charges like every totem the player has fired since e1-l03. The multiplier from e2-l02 stays unlocked and can be the gift.

### e2-l04: Harvest

- **Idea:** The harvest totem: two green moves in a row collect every gem on the board at once.
- **Feeling:** a windfall: the gem counter jumps.
- **Duration:** 210 to 260 s median (sim greedy about 60 to 75 moves).
- **Target success rate:** Normal: at least 85 % first try; hard: at least 70 %. Simulator: the e2 row of [balance](balance.md), with `greedy` expected to win every run.
- **Takeaway:** The player knows green collects every gem in view at once, about 5 toward the goal.
- **Role and levers:** introduce; goal size: 50 gems; complexity: five totems.
- **First contact:** Harvest only helps the goal, and the board always holds 5 gems, so it pays the same whenever it fires. The gift totem from e2-l03 stays for a second look.

### e2-l05: Fresh paint

- **Idea:** The paint totem: two white moves in a row repaint 4 random pieces to one kind; the gems-on-board upgrade (6) becomes available as a passive extra.
- **Feeling:** curiosity: a few pieces change colour, sometimes into a match.
- **Duration:** 200 to 250 s median (sim greedy about 55 to 70 moves).
- **Target success rate:** Normal: at least 85 % first try; hard: at least 70 %. Simulator: the e2 row of [balance](balance.md), with `greedy` expected to win every run.
- **Takeaway:** The player knows white repaints a few pieces into one colour, and that one more gem on the board can be bought.
- **Role and levers:** introduce; goal size: 55 gems; complexity: six totems.
- **First contact:** Paint never removes progress, and any run it happens to make resolves for free. The harvest totem from e2-l04 stays for a second look.

### e2-l06: The adviser

- **Idea:** The hint power-up (shows every legal swap for 20 s), in the episode finale that uses every episode 2 item.
- **Feeling:** confidence: the whole kit at once, with help when stuck.
- **Duration:** 220 to 270 s median (sim greedy about 65 to 80 moves).
- **Target success rate:** Normal: at least 85 % first try; hard: at least 70 %. Simulator: the e2 row of [balance](balance.md), with `greedy` expected to win every run.
- **Takeaway:** The player knows hint shows every move for a while, and has used every totem and power-up of the first two episodes.
- **Role and levers:** introduce; goal size: 60 gems; complexity: every e1-e2 item at once.
- **First contact:** Hint is help, not a threat. Power-ups come from runs of 4 or more, which a player makes every few moves, and each draw is 1 in 6 among the unlocked ones, so a player usually meets the hint more than once in a level of 50+ moves. The paint totem from e2-l05 stays for a second look. Not a difficulty spike: episode 3's chains are the next test.

| Level | File | Gems normal / hard | Totems | Power-ups | Seed |
|---|---|---|---|---|---|
| e2-l01 | [`l01.json`](../../game/data/levels/e2/l01.json) | 35 / 55 | `fire`, `time`, `gold` | `tnt`, `color_clear`, `lightning`, `clock` | 20090501 |
| e2-l02 | [`l02.json`](../../game/data/levels/e2/l02.json) | 40 / 60 | `fire`, `time`, `gold` | `tnt`, `color_clear`, `lightning`, `clock`, `multiplier` | 20090502 |
| e2-l03 | [`l03.json`](../../game/data/levels/e2/l03.json) | 45 / 70 | `fire`, `time`, `gold`, `gift` | `tnt`, `color_clear`, `lightning`, `clock`, `multiplier` | 20090503 |
| e2-l04 | [`l04.json`](../../game/data/levels/e2/l04.json) | 50 / 75 | `fire`, `time`, `gold`, `gift`, `harvest` | `tnt`, `color_clear`, `lightning`, `clock`, `multiplier` | 20090504 |
| e2-l05 | [`l05.json`](../../game/data/levels/e2/l05.json) | 55 / 85 | `fire`, `time`, `gold`, `gift`, `harvest`, `paint` | `tnt`, `color_clear`, `lightning`, `clock`, `multiplier` | 20090505 |
| e2-l06 | [`l06.json`](../../game/data/levels/e2/l06.json) | 60 / 90 | `fire`, `time`, `gold`, `gift`, `harvest`, `paint` | `tnt`, `color_clear`, `lightning`, `clock`, `multiplier`, `hint` | 20090506 |

No episode 2 level has chains or steel: chains arrive in e3-l01 and steel
in e3-l06. Every episode 2 level sets `base_clock` from the
[clock rule](balance.md#clock), 270 to 310 s on normal and 380 to 490 s
on hard.

## Episode 3 levels

Briefs for episode 3 (WP-25): chains from e3-l01, steel from e3-l06.
Simulator targets are the e3 row of [balance](balance.md), including the
moves floor of 70; `greedy` is expected to win every run on each level.
Each level also makes one or two tier-2 upgrades buyable, all optional
(`PROG-04`). Why the episode is ordered this way, what the simulator
measures for chains, and the open question of human time against the
clock are in [about the episode 3 levels](../explanation/about-the-episode-3-levels.md).
From this episode on, **Duration** gives simulator moves first, then the
human time at 3.5 s a move, and the level table adds an Obstacles column.

### e3-l01: Locked in

- **Idea:** Chains: a chained piece cannot be swapped but still matches when a run forms through it, and the chain goes with it.
- **Feeling:** curiosity.
- **Duration:** Sim `greedy` 75 to 90 mean moves on normal (65 gems), about 112 to 135 on hard (100 gems); about 262 to 315 s of play at 3.5 s a move.
- **Target success rate:** Normal: at least 75 % of players win on the first try; hard: at least 60 %. Human rates are provisional until a playtest ([why](../explanation/about-the-episode-3-levels.md#human-time-against-the-clock)). Simulator: the e3 row of [balance](balance.md). It introduces chains after e2-l06, which wins every greedy run, so `greedy` must win every run here too (the 100 % tie); that also makes it at least as easy as e3-l03.
- **Takeaway:** The player knows a chained piece can't be moved but can be freed by lining up its colour next to it, and that it then plays like any piece.
- **Role and levers:** introduce; goal size: 65 gems; complexity: 6 chained cells, all on the bottom row.
- **First contact:** Chains only stop swaps: they cost no time or gems and still match. On the bottom row nothing lies below them, so they stay where the player first sees them, a fixed and readable lock, until a run through them or a blast clears them; the rest of the board plays as in episode 2. tnt tier 2 (a diamond of radius 2) becomes buyable here; it helps but is optional (PROG-04).

### e3-l02: Falling locks

- **Idea:** Falling chains: chains start high and fall into play as the pieces under them clear.
- **Feeling:** watchfulness.
- **Duration:** Sim `greedy` 80 to 95 mean moves on normal (70 gems), about 120 to 142 on hard (105 gems); about 280 to 332 s of play at 3.5 s a move.
- **Target success rate:** Normal: at least 75 % first try; hard: at least 60 %. Human rates are provisional until a playtest ([why](../explanation/about-the-episode-3-levels.md#human-time-against-the-clock)). Simulator: the e3 row of [balance](balance.md), `greedy` expected to win every run, so the curve rules hold by the 100 % tie.
- **Takeaway:** The player knows chains fall like other pieces, so a lock can land in the middle of a planned move.
- **Role and levers:** develop; goal size: 70 gems; complexity: 8 chained cells in the top half.
- **First contact:** Builds on e3-l01: the same lock, now moving. The time totem's tier 2 (+120 s) becomes buyable here; optional.

### e3-l03: The band

- **Idea:** A band of chains: two adjacent rows packed with chains, where one blast frees several at once.
- **Feeling:** satisfaction (when one blast frees several chains).
- **Duration:** Sim `greedy` 85 to 100 mean moves on normal (75 gems), about 128 to 150 on hard (115 gems); about 298 to 350 s of play at 3.5 s a move.
- **Target success rate:** Normal: at least 70 % first try; hard: at least 55 %. Human rates are provisional until a playtest ([why](../explanation/about-the-episode-3-levels.md#human-time-against-the-clock)). Simulator: the e3 row of [balance](balance.md), `greedy` expected to win every run, so the curve rules hold by the 100 % tie.
- **Takeaway:** The player knows any blast that reaches the band (tnt, lightning's row from tier 2, fire) frees several chains at once.
- **Role and levers:** twist; goal size: 75 gems; complexity: 8 chained cells in two adjacent middle rows, in pairs side by side.
- **First contact:** Chains are known from e3-l01 and e3-l02; the twist is their density: a power-up lands on a random piece, and whenever one goes off in or beside the band (tier-1 tnt's 3x3 covers both rows) it frees several chains together. Fire tier 2 and Score Frenzy tier 2 become buyable here; optional.

### e3-l04: Bound centre

- **Idea:** Chain test: the most chains of the episode, packed in the centre where most swaps happen, before the Reaction 1 bonus level.
- **Feeling:** tension.
- **Duration:** Sim `greedy` 90 to 110 mean moves on normal (80 gems), about 135 to 165 on hard (120 gems); about 315 to 385 s of play at 3.5 s a move.
- **Target success rate:** Normal: at least 65 % first try; hard: at least 50 %. Human rates are provisional until a playtest ([why](../explanation/about-the-episode-3-levels.md#human-time-against-the-clock)). Simulator: the e3 row of [balance](balance.md), `greedy` expected to win every run, so the curve rules hold by the 100 % tie.
- **Takeaway:** The player knows how to open a locked centre: line up runs through the chained pieces and set off power-ups next to them.
- **Role and levers:** test; goal size: 80 gems; complexity: 10 chained cells (the guidance maximum) in the four centre columns, on the bottom three rows so they last.
- **First contact:** Tests chains with every earlier tool; nothing new arrives in the level. Timer tier 2 (330 s) becomes buyable here; optional, and the brief's human time does not assume it.

### e3-l05: Loose ends

- **Idea:** Rest after the spike: the curve's next goal with only 2 chains, so the board is open again.
- **Feeling:** relief.
- **Duration:** Sim `greedy` 95 to 115 mean moves on normal (85 gems), about 142 to 172 on hard (130 gems); about 332 to 402 s of play at 3.5 s a move.
- **Target success rate:** Normal: at least 75 % first try; hard: at least 60 %. Human rates are provisional until a playtest ([why](../explanation/about-the-episode-3-levels.md#human-time-against-the-clock)). Simulator: the e3 row of [balance](balance.md), `greedy` expected to win every run, so the curve rules hold by the 100 % tie.
- **Takeaway:** The player knows they can read and free chains on sight: two locks on an open board are no longer a problem.
- **Role and levers:** rest; goal size: 85 gems (the progression curve); complexity: 2 chained cells (the guidance minimum).
- **First contact:** Mixes everything from episodes 1 and 2 with a light touch of chains; the relief is in fewer locks, not a shorter level (the +5 goal still rises). Multiplier tier 2 and gift tier 2 become buyable here; optional.

### e3-l06: Cold iron

- **Idea:** Steel: some new pieces fall as steel, which can't be swapped or matched wherever it lands, and only fire, tnt and lightning break it (and color_clear from tier 2, its kind only).
- **Feeling:** wariness.
- **Duration:** Sim `greedy` 115 to 135 mean moves on normal (90 gems), about 172 to 202 on hard (135 gems); about 402 to 472 s of play at 3.5 s a move.
- **Target success rate:** Normal: at least 75 % first try; hard: at least 60 %, the same as e3-l05 (it sits at the same difficulty). Human rates are provisional until a playtest ([why](../explanation/about-the-episode-3-levels.md#human-time-against-the-clock)). Simulator: the e3 row of [balance](balance.md). It introduces steel, so `greedy` must win every run here and on e3-l05 (the 100 % tie the curve rule allows).
- **Takeaway:** The player knows steel can't be swapped or matched wherever it falls, and that only fire, tnt, lightning and color_clear from tier 2 (its colour only) break it, so power-ups are worth keeping for steel.
- **Role and levers:** introduce; goal size: 90 gems; complexity: steel_rate 0.02 (the guidance minimum), no chains.
- **First contact:** Steel is rare: at 2 % of refills, a level of this length (several hundred refills) brings about a dozen steel pieces spread over the whole level, and at most 8 can stand at once (OBS-02). The level has no chains, so the player meets one obstacle at a time. Tier-1 fire, tnt and lightning, owned since episode 1, already break steel. Color_clear tier 2 and gold tier 2 become buyable here; optional purchases, not ideas the level teaches.

### e3-l07: Iron and chain

- **Idea:** Steel with chains: both obstacles on the board together, each with its own answer.
- **Feeling:** juggling.
- **Duration:** Sim `greedy` 130 to 150 mean moves on normal (95 gems), about 195 to 225 on hard (145 gems); about 455 to 525 s of play at 3.5 s a move.
- **Target success rate:** Normal: at least 70 % first try; hard: at least 55 %. Human rates are provisional until a playtest ([why](../explanation/about-the-episode-3-levels.md#human-time-against-the-clock)). Simulator: the e3 row of [balance](balance.md), `greedy` expected to win every run, so the curve rules hold by the 100 % tie.
- **Takeaway:** The player knows chains are freed by matching through them and steel only by fire, tnt, lightning or a tier-2 color_clear of its colour, and picks the tool for each.
- **Role and levers:** develop; goal size: 95 gems; complexity: steel_rate 0.03 and 4 chained cells on the bottom row, away from the centre, so they last while steel builds up.
- **First contact:** Both obstacles have been met alone (chains e3-l01 to e3-l05, steel e3-l06); bottom-row chains have nothing below them and stay until matched, so they are still there when steel starts to fall. Harvest tier 2 and paint tier 2 become buyable here; optional.

### e3-l08: The vault

- **Idea:** Episode finale: the most chains and the most steel of the episode together at the 100-gem goal, before the Attention 1 bonus level.
- **Feeling:** triumph.
- **Duration:** Sim `greedy` 125 to 145 mean moves on normal (100 gems), about 188 to 218 on hard (150 gems); about 438 to 508 s of play at 3.5 s a move.
- **Target success rate:** Normal: at least 65 % first try; hard: at least 50 %. Human rates are provisional until a playtest ([why](../explanation/about-the-episode-3-levels.md#human-time-against-the-clock)). Simulator: the e3 row of [balance](balance.md), `greedy` expected to win every run, so the curve rules hold by the 100 % tie.
- **Takeaway:** The player knows every obstacle of episode 3 and has beaten the episode's biggest goal with the most obstacles on the board.
- **Role and levers:** test; goal size: 100 gems; complexity: steel_rate 0.05 and 10 chained cells (equal to e3-l04's maximum) on the bottom two rows, so they last while steel builds up.
- **First contact:** Mixes chains, steel and every totem and power-up so far; bottom-row chains stay until matched, so both obstacles share the board through the middle of the level. Gems on board tier 2 (7) becomes buyable here; optional. The rest that follows is the Attention 1 bonus level and episode 4's opener.

| Level | File | Gems normal / hard | Totems | Power-ups | Obstacles | Seed |
|---|---|---|---|---|---|---|
| e3-l01 | [`l01.json`](../../game/data/levels/e3/l01.json) | 65 / 100 | every e1-e2 totem | every e1-e2 power-up | 6 chains | 20090601 |
| e3-l02 | [`l02.json`](../../game/data/levels/e3/l02.json) | 70 / 105 | every e1-e2 totem | every e1-e2 power-up | 8 chains | 20090602 |
| e3-l03 | [`l03.json`](../../game/data/levels/e3/l03.json) | 75 / 115 | every e1-e2 totem | every e1-e2 power-up | 8 chains | 20090603 |
| e3-l04 | [`l04.json`](../../game/data/levels/e3/l04.json) | 80 / 120 | every e1-e2 totem | every e1-e2 power-up | 10 chains | 20090604 |
| e3-l05 | [`l05.json`](../../game/data/levels/e3/l05.json) | 85 / 130 | every e1-e2 totem | every e1-e2 power-up | 2 chains | 20090605 |
| e3-l06 | [`l06.json`](../../game/data/levels/e3/l06.json) | 90 / 135 | every e1-e2 totem | every e1-e2 power-up | steel 0.02 | 20090606 |
| e3-l07 | [`l07.json`](../../game/data/levels/e3/l07.json) | 95 / 145 | every e1-e2 totem | every e1-e2 power-up | steel 0.03, 4 chains | 20090607 |
| e3-l08 | [`l08.json`](../../game/data/levels/e3/l08.json) | 100 / 150 | every e1-e2 totem | every e1-e2 power-up | steel 0.05, 10 chains | 20090608 |

Every episode 3 level has all six totems and six power-ups unlocked so
far (`ward` arrives in e4-l01). Each sets `base_clock` from the
[clock rule](balance.md#clock): 340 to 590 s on normal and 520 to 900 s
on hard, peaking at e3-l07. `chained_cells` and
`steel_rate` are defined in [level data](level-data.md); y = 0 is the
top row.

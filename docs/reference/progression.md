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
| Timer, added to the level's base clock (`GOAL-03`, [clock rule](balance.md#clock)) | +0 s | +30 s (e1-l02) | +60 s (e3-l04) | +90 s (e4-l06) | +120 s (e5-l08) | [S1]; amounts chosen |
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

Every level: 8 × 8 board, 7 kinds, its base clock plus the player's
timer tier, every item unlocked so far. Steel rate and chains per level are set in WP-23 to
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
its base clock plus the player's timer tier, and 5 gems on the board.

### e1-l02: Solidarity

- **Teaches:** a run of 5 makes two power-ups, and `color_clear` clears
  every piece of one kind; the timer has its first upgrade (+30 s).
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

No episode 1 level has chains or steel. e1-l05 sets `base_clock` 270/300
from the [clock rule](balance.md#clock).

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
- **First contact:** Tests chains with every earlier tool; nothing new arrives in the level. Timer tier 2 (+60 s on top of the base clock) becomes buyable here; optional, and the brief's human time does not assume it.

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

## Episode 4 levels

Briefs for episode 4 (WP-26): the `ward` totem from e4-l01, then steel
and chains in new amounts and shapes. Simulator targets are the e4 row of
[balance](balance.md), including the moves floor of 90; `random` is
informational from this episode on. Every tier-3 upgrade of the
episode is optional (`PROG-04`), and the human targets are for a player
who has not bought them. Why the episode is ordered this way and what the
simulator measures here are in
[about the episode 4 levels](../explanation/about-the-episode-4-levels.md).
Ward has one use a level at tiers 1 and 2 (`TOT-06`) and fires by itself
after two moves in a row whose swap match is blue (`TOT-02`, `TOT-03`).

### e4-l01: Blue ward

- **Idea:** The ward totem (blue): two blue matches in a row fire it once a level, and it destroys every steel piece on the board at once.
- **Feeling:** relief when the steel vanishes.
- **Duration:** Sim `greedy` 129 to 145 mean moves on normal (105 gems), 201 to 226 on hard (160 gems); about 452 to 508 s of play on normal and 704 to 791 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 80 % of players win on the first try; hard: at least 65 %. Human rates are provisional until a playtest. Simulator: the e4 row of balance.md (`random` informational from e4 on) and the curve rules as `--check` runs them; it introduces an idea after a level that wins every `greedy` run, so it must win every run too (the 100 % tie).
- **Takeaway:** The player knows two blue matches in a row fire the ward, and that it clears every steel piece on the board, once a level.
- **Role and levers:** introduce; goal size: 105 gems; complexity: steel_rate 0.04, no chains.
- **First contact:** Safe because the level does not need it: steel at 0.04 is lighter than e3-l08's 0.05, which players beat without ward, so a ward spent early by accident costs nothing but the chance to watch it work. No chains, so the player meets one thing at a time. As an introducing level after e3-l08 it must win every greedy run (the 100 % tie) and be at least as easy as e4-l03 ([balance](balance.md): the curve rule ties each episode opener to its third level). Lightning tier 3 becomes buyable here; optional.
- **Clock:** Normal 480 s (137.2 draft mean moves x 3.5), hard 748 s (213.6 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. The first level with ward; the human time here is the episode's lowest.

### e4-l02: Iron rain

- **Idea:** Timing the one ward against steel at the guidance maximum.
- **Feeling:** tension while holding blue back, then satisfaction when the ward clears a crowd of steel.
- **Duration:** Sim `greedy` 135 to 152 mean moves on normal (110 gems), 201 to 227 on hard (165 gems); about 472 to 532 s of play on normal and 704 to 794 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 75 % of players win on the first try; hard: at least 60 %. Human rates are provisional until a playtest. Simulator: the e4 row of balance.md (`random` informational from e4 on) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft, within 10 points of its neighbours.
- **Takeaway:** The player knows to avoid two blue matches in a row until several steel pieces stand, then fire the ward on purpose.
- **Role and levers:** develop; goal size: 110 gems; complexity: steel_rate 0.06 (guidance maximum), no chains.
- **First contact:** Builds on e4-l01: the same single ward, now with enough steel (up to 8 can stand, OBS-02) that firing it early wastes it. Steel left after the ward goes to fire, tnt, lightning and color_clear (its colour, from tier 2) as in episode 3. Clock tier 2 becomes buyable here; optional.
- **Clock:** Normal 503 s (143.7 draft mean moves x 3.5), hard 749 s (214.1 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. Most steel of the first half; the clock covers it from the rule's margin.

### e4-l03: The chains stay

- **Idea:** Ward does not touch chains: chains and steel need different answers on the same board.
- **Feeling:** discovery (the chains stay after a ward).
- **Duration:** Sim `greedy` 139 to 157 mean moves on normal (115 gems), 214 to 241 on hard (175 gems); about 486 to 550 s of play on normal and 749 to 844 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 78 % of players win on the first try; hard: at least 63 %. Human rates are provisional until a playtest. Simulator: the e4 row of balance.md (`random` informational from e4 on) and the curve rules as `--check` runs them; chains coming back are not a new idea for the curve rule, but as a twist it is kept at least as easy as e4-l02: `greedy` wins every run.
- **Takeaway:** The player knows ward clears only steel, and chains still need a run through them or a blast.
- **Role and levers:** twist; goal size: 115 gems; complexity: steel_rate 0.02 and 8 chained cells in the four centre columns of the bottom two rows.
- **First contact:** The first level of the episode with chains. On e4-l01 and e4-l02 the ward cleared every obstacle on the board, because steel was the only one; here the first ward meets chains and leaves them. The chains sit in the centre of the bottom two rows, where the bottom-row links cannot fall and the row above only falls when a link under it clears, so they are still there when steel has gathered and the ward fires. Hint tier 2 and gold tier 3 become buyable here; optional. Steel stays light (0.02) so the chains, not the steel, are what the player watches, and so the twist is no harder than e4-l02.
- **Clock:** Normal 517 s (147.7 draft mean moves x 3.5), hard 795 s (227.2 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. Bottom-row chains barely change the simulated length ([about the episode 3 levels](../explanation/about-the-episode-3-levels.md#what-the-simulator-measures-here)).

### e4-l04: Locked floor

- **Idea:** Test: ward, steel and a full row of chains on the floor.
- **Feeling:** pressure from a locked floor.
- **Duration:** Sim `greedy` 146 to 165 mean moves on normal (120 gems), 218 to 246 on hard (180 gems); about 511 to 578 s of play on normal and 763 to 861 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 65 % of players win on the first try; hard: at least 50 %. Human rates are provisional until a playtest. Simulator: the e4 row of balance.md (`random` informational from e4 on) and the curve rules as `--check` runs them; `greedy` wins 99 % on the draft, within 10 points of its neighbours.
- **Takeaway:** The player knows how to split their tools: the ward for steel, runs and blasts for chains, with a floor of locks that never moves on its own.
- **Role and levers:** test; goal size: 120 gems; complexity: steel_rate 0.05 and 8 chained cells filling the bottom row.
- **First contact:** Mixes ward, steel and chains; a chained bottom row has nothing below it, so it stays until matched or blasted. Obstacles stay below the episode's maximums so the spike (e4-l08) and the finale (e4-l10) can ask more. Fire tier 3 becomes buyable here; optional.
- **Clock:** Normal 544 s (155.5 draft mean moves x 3.5), hard 811 s (231.8 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. First test of the episode.

### e4-l05: Open ground

- **Idea:** Building runs of 4 or more on an open board: the lightest obstacle load of the episode at the curve's next goal, before the Accuracy 2 bonus level.
- **Feeling:** relief.
- **Duration:** Sim `greedy` 147 to 165 mean moves on normal (125 gems), 226 to 255 on hard (190 gems); about 514 to 578 s of play on normal and 791 to 892 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 75 % of players win on the first try; hard: at least 60 %. Human rates are provisional until a playtest. Simulator: the e4 row of balance.md (`random` informational from e4 on) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft, within 10 points of its neighbours.
- **Takeaway:** The player knows a light board leaves room to set up runs of 4 or more, so power-ups are already on the board when the few obstacles arrive.
- **Role and levers:** rest; goal size: 125 gems (the progression curve); complexity: steel_rate 0.02 and 2 chained cells (guidance minimums).
- **First contact:** Mixes everything so far at the lightest obstacle load of the episode; the relief is in fewer obstacles, not a shorter level (the +5 goal still rises). Score Frenzy tier 3 becomes buyable here; optional.
- **Clock:** Normal 546 s (155.9 draft mean moves x 3.5), hard 842 s (240.5 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. A rest by load, not by length.

### e4-l06: Pillars

- **Idea:** Chained columns: two vertical lines of chains wall off the sides of the lower board.
- **Feeling:** relief when a column opens.
- **Duration:** Sim `greedy` 157 to 177 mean moves on normal (130 gems), 236 to 266 on hard (195 gems); about 550 to 620 s of play on normal and 826 to 931 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 68 % of players win on the first try; hard: at least 53 %. Human rates are provisional until a playtest. Simulator: the e4 row of balance.md (`random` informational from e4 on) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft, within 10 points of its neighbours.
- **Takeaway:** The player knows a column of chains cannot be swapped, so it splits the board, but its links still match, so runs through them and blasts beside them open the wall.
- **Role and levers:** twist; goal size: 130 gems; complexity: steel_rate 0.03 and 10 chained cells in two columns, five cells each, on the bottom five rows.
- **First contact:** Chains are familiar; the new shape is a line of them: chained pieces cannot be swapped, so the column splits the lower board. Chained pieces still match normally (OBS-05), so a run that lines up with a link clears it, and the links above a cleared one drop down the column. Steel stays light so the columns are the point. Timer tier 3 and paint tier 3 become buyable here; optional.
- **Clock:** Normal 584 s (166.8 draft mean moves x 3.5), hard 880 s (251.3 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. Moves per gem barely change from e4-l05 (1.25 to 1.28): the columns shape the play more than the length.

### e4-l07: Scattered locks

- **Idea:** Spread locks: four single chains far apart, so most blasts free one at a time and runs through each link do most of the work.
- **Feeling:** steady satisfaction as each lock goes.
- **Duration:** Sim `greedy` 161 to 182 mean moves on normal (135 gems), 248 to 279 on hard (205 gems); about 564 to 637 s of play on normal and 868 to 976 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 65 % of players win on the first try; hard: at least 50 %. Human rates are provisional until a playtest. Simulator: the e4 row of balance.md (`random` informational from e4 on) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft, within 10 points of its neighbours.
- **Takeaway:** The player knows scattered chains are freed mostly one at a time, by runs through them, so blasts go further on steel; a blast frees two only when chains have drifted close or happen to share its reach.
- **Role and levers:** develop; goal size: 135 gems; complexity: steel_rate 0.05 and 4 chained cells, no two in the same row or column and each more than four cells (row plus column) from any other, on the bottom five rows.
- **First contact:** Develops e4-l03 and e4-l06 with a new spacing, not a new rule: packed chains rewarded blasts (e3-l03), spread ones reward runs. At the start no two chains share a row or column and each pair is more than four cells apart, so a blast seldom reaches two; chains fall like any piece (OBS-05), so the spacing loosens as the level goes on, and fire, a lightning cross or color_clear can still catch two. Gift tier 3 becomes buyable here; optional.
- **Clock:** Normal 600 s (171.3 draft mean moves x 3.5), hard 922 s (263.4 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. Fewer chains than e4-l06 but more steel.

### e4-l08: The forge

- **Idea:** Episode spike: both obstacles at their maximums, with steel still arriving after the one ward is spent.
- **Feeling:** pressure.
- **Duration:** Sim `greedy` 166 to 187 mean moves on normal (140 gems), 249 to 281 on hard (210 gems); about 581 to 654 s of play on normal and 872 to 984 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 60 % of players win on the first try; hard: at least 45 %. Human rates are provisional until a playtest. Simulator: the e4 row of balance.md (`random` informational from e4 on) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft, within 10 points of its neighbours. The human targets are for a player who has not bought time tier 3 (buyable here, +180 s twice a level); a player who has finds it lighter.
- **Takeaway:** The player knows that after the one ward, every blast has to be shared between steady steel and three clusters of chains, and that a blast on a cluster frees several chains at once.
- **Role and levers:** test; goal size: 140 gems; complexity: steel_rate 0.06 and 10 chained cells (guidance maximums) in three clusters on the bottom two rows.
- **First contact:** Tests everything in episode 4 at its heaviest: the first level with both obstacles at their maximums, so one ward cannot cover the level: after it, steel goes to fire, tnt, lightning and color_clear (its colour, from tier 2), the same tools the chain clusters want. Clusters let a blast free several chains at once (e3-l03's lesson). Tnt tier 3 and time tier 3 become buyable here; optional.
- **Clock:** Normal 616 s (176.1 draft mean moves x 3.5), hard 929 s (265.3 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. The spike; the longest level of the episode in simulated moves.

### e4-l09: Quiet before

- **Idea:** Rest before the finale: lighter obstacles at the next goal, where one ward does most of the work.
- **Feeling:** breather.
- **Duration:** Sim `greedy` 152 to 171 mean moves on normal (145 gems), 230 to 259 on hard (220 gems); about 532 to 598 s of play on normal and 805 to 906 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 68 % of players win on the first try; hard: at least 53 %. Human rates are provisional until a playtest. Simulator: the e4 row of balance.md (`random` informational from e4 on) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft, within 10 points of its neighbours.
- **Takeaway:** The player knows that on a light board one well-timed ward does most of the work: at tier 1 fire it once steel has gathered and blast the few that follow, or with tier 2 (buyable here) fire it early to stop new steel for good.
- **Role and levers:** rest; goal size: 145 gems (the progression curve); complexity: steel_rate 0.03 and 4 chained cells on the bottom row.
- **First contact:** A rest at e3-l07's obstacle load, now with ward to answer the steel, after the e4-l08 spike. Ward tier 2 and color_clear tier 3 become buyable here; optional, and the takeaway holds at either ward tier.
- **Clock:** Normal 565 s (161.3 draft mean moves x 3.5), hard 855 s (244.3 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. Simulated moves fall here from e4-l08 because the obstacles are lighter and the simulator buys ward tier 2 (no new steel after the ward) and color_clear tier 3 as they become available; a player who skipped them takes longer ([about level clocks](../explanation/about-level-clocks.md#which-player)).

### e4-l10: Falling iron

- **Idea:** Finale: both obstacles at their maximums with the chains high on the board, falling into the steel, at the episode's biggest goal, before the Memory 2 bonus level.
- **Feeling:** triumph.
- **Duration:** Sim `greedy` 155 to 175 mean moves on normal (150 gems), 234 to 264 on hard (225 gems); about 542 to 612 s of play on normal and 819 to 924 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 55 % of players win on the first try; hard: at least 40 %. Human rates are provisional until a playtest. Simulator: the e4 row of balance.md (`random` informational from e4 on) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft, within 10 points of its neighbours. The human targets are for a player who skipped ward tier 2 and harvest tier 3.
- **Takeaway:** The player knows they can handle falling locks and heavy steel together: the ward for the steel, runs and blasts for the chains as they land.
- **Role and levers:** test; goal size: 150 gems; complexity: steel_rate 0.06 and 10 chained cells spread over the upper half.
- **First contact:** Mixes chains, steel, ward and every totem and power-up so far. Unlike e4-l08's fixed floor locks, these chains start high and fall (e3-l02's falling locks) into a board crowded with steel. A player who bought ward tier 2 (e4-l09) stops steel after the ward and one who buys harvest tier 3 here can clear every chain at once: those purchases make the finale lighter, which is their reward; the brief's targets are for a player who skipped them. Multiplier tier 3 and harvest tier 3 become buyable here; optional.
- **Clock:** Normal 579 s (165.4 draft mean moves x 3.5), hard 873 s (249.3 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. Simulated moves stay below e4-l08 because the simulator owns ward tier 2 and buys harvest tier 3 (removes every chain) here; the human targets are for a player who skipped them.

| Level | File | Gems normal / hard | Totems | Power-ups | Obstacles | Seed |
|---|---|---|---|---|---|---|
| e4-l01 | [`l01.json`](../../game/data/levels/e4/l01.json) | 105 / 160 | all seven | all six | steel 0.04 | 20090701 |
| e4-l02 | [`l02.json`](../../game/data/levels/e4/l02.json) | 110 / 165 | all seven | all six | steel 0.06 | 20090702 |
| e4-l03 | [`l03.json`](../../game/data/levels/e4/l03.json) | 115 / 175 | all seven | all six | steel 0.02, 8 chains | 20090703 |
| e4-l04 | [`l04.json`](../../game/data/levels/e4/l04.json) | 120 / 180 | all seven | all six | steel 0.05, 8 chains | 20090704 |
| e4-l05 | [`l05.json`](../../game/data/levels/e4/l05.json) | 125 / 190 | all seven | all six | steel 0.02, 2 chains | 20090705 |
| e4-l06 | [`l06.json`](../../game/data/levels/e4/l06.json) | 130 / 195 | all seven | all six | steel 0.03, 10 chains | 20090706 |
| e4-l07 | [`l07.json`](../../game/data/levels/e4/l07.json) | 135 / 205 | all seven | all six | steel 0.05, 4 chains | 20090707 |
| e4-l08 | [`l08.json`](../../game/data/levels/e4/l08.json) | 140 / 210 | all seven | all six | steel 0.06, 10 chains | 20090708 |
| e4-l09 | [`l09.json`](../../game/data/levels/e4/l09.json) | 145 / 220 | all seven | all six | steel 0.03, 4 chains | 20090709 |
| e4-l10 | [`l10.json`](../../game/data/levels/e4/l10.json) | 150 / 225 | all seven | all six | steel 0.06, 10 chains | 20090710 |

Every episode 4 level has all seven totems and six power-ups unlocked.
`chained_cells` and `steel_rate` are defined in [level data](level-data.md);
y = 0 is the top row. Each sets `base_clock` from the
[clock rule](balance.md#clock): 560 to 710 s on normal and 870 to 1080 s
on hard, peaking at e4-l08.

## Episode 5 levels

Briefs for episode 5 (WP-27), the last episode: twelve levels and no new
rule. The gem goal reaches its cap of 200 / 300 at e5-l10, and steel and
chains reach their guidance maximums together on e5-l08 and e5-l12. Simulator
targets are the e5 row of [balance](balance.md), including the moves
floor of 110; `random` is informational. Every tier above tier 1 is
optional (`PROG-04`). Why the episode is ordered this way and what the
simulator measures here are in
[about the episode 5 levels](../explanation/about-the-episode-5-levels.md).

### e5-l01: Return to the temple

- **Idea:** Return to the temple: steel and chains together at a light load, where one ward and blasts on the bottom-row links are still enough.
- **Feeling:** confidence.
- **Duration:** Sim `greedy` 159 to 180 mean moves on normal (155 gems), 243 to 274 on hard (235 gems); about 556 to 630 s of play on normal and 850 to 959 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 75 % of players win on the first try; hard: at least 60 %. Human rates are provisional until a playtest, and within the episode no level's is more than 10 points below the next level's. The human targets are for a player who has bought the tiers that were on offer, except the optional obstacle removers (ward tiers 2 and 3, harvest tiers 3 and 4); a player who owns those finds the level lighter, which is their reward. Simulator: the e5 row of balance.md (`random` informational) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft.
- **Takeaway:** The player knows the episode 4 play still wins at a bigger goal: one ward once steel has gathered, and blasts or runs through the bottom-row links.
- **Role and levers:** rest; goal size: 155 gems; complexity: steel_rate 0.03 and 4 chained cells on the bottom two rows (two on the bottom row near the edges, two above the centre).
- **First contact:** Mixes every totem, power-up and both obstacles from episodes 1 to 4 at a light load (steel 0.03, 4 chains on the bottom two rows, well below e4-l10's maximums) after the Memory 2 bonus level. As the episode opener it is a rest by obstacle load, not by length: its goal is the curve's next step, and it must be at least as easy as e5-l03 (the curve rule ties each opener to its third level). Gold tier 4 becomes buyable here; optional.
- **Clock:** Normal 593 s (169.5 draft mean moves x 3.5), hard 905 s (258.6 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. The opener; moves sit above e5-l02's because the simulator buys gems on board tier 3 (8 gems) only at e5-l02.

### e5-l02: The stair

- **Idea:** A diagonal stair of chains, one link in each of columns 2 to 7, whose upper links drift down first.
- **Feeling:** curiosity.
- **Duration:** Sim `greedy` 145 to 163 mean moves on normal (160 gems), 221 to 249 on hard (240 gems); about 508 to 570 s of play on normal and 774 to 872 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 72 % of players win on the first try; hard: at least 57 %. Human rates are provisional until a playtest, and within the episode no level's is more than 10 points below the next level's. The human targets are for a player who has bought the tiers that were on offer, except the optional obstacle removers (ward tiers 2 and 3, harvest tiers 3 and 4); a player who owns those finds the level lighter, which is their reward. Simulator: the e5 row of balance.md (`random` informational) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft.
- **Takeaway:** The player knows a link falls each time a piece under it clears, so the upper links of the diagonal, with the most pieces under them, drift down first and the stair flattens toward the floor.
- **Role and levers:** develop; goal size: 160 gems; complexity: steel_rate 0.04 and 6 chained cells, one in each of columns 2 to 7, on a diagonal that rises one row per column from the bottom row.
- **First contact:** Develops falling chains (e3-l02, e4-l10) with a new shape: one link in each of columns 2 to 7 on a diagonal from the bottom row up to row 2, so no column has more than one lock. The bottom link cannot fall; the upper ones have more pieces under them and drift first. Steel at 0.04 stays below the episode's maximum. Fire tier 4 and gems on board tier 3 (8) become buyable here; optional.
- **Clock:** Normal 539 s (154.1 draft mean moves x 3.5), hard 822 s (235.0 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. Simulated moves drop from e5-l01: 8 gems on the board (gems on board tier 3, bought here by the simulator) bring each gem goal closer.

### e5-l03: Falling crown

- **Idea:** A whole top row of chains falling through every row of the board.
- **Feeling:** surprise.
- **Duration:** Sim `greedy` 148 to 167 mean moves on normal (165 gems), 228 to 257 on hard (250 gems); about 518 to 584 s of play on normal and 798 to 900 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 68 % of players win on the first try; hard: at least 53 %. Human rates are provisional until a playtest, and within the episode no level's is more than 10 points below the next level's. The human targets are for a player who has bought the tiers that were on offer, except the optional obstacle removers (ward tiers 2 and 3, harvest tiers 3 and 4); a player who owns those finds the level lighter, which is their reward. Simulator: the e5 row of balance.md (`random` informational) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft.
- **Takeaway:** The player knows chains that start on the top row fall through the whole board and land at different heights, so they are freed where they land, by runs and blasts in the lower half.
- **Role and levers:** twist; goal size: 165 gems; complexity: steel_rate 0.03 and 8 chained cells filling the top row (y = 0).
- **First contact:** Twists falling chains: e3-l02 had a single link on the top row; here the whole top row is chained, so every link has seven pieces under it and the row breaks up within the first moves. Steel stays light (0.03) so the falling links are the point, and so the twist is no harder than e5-l04's test. Hint tier 3 becomes buyable here; optional.
- **Clock:** Normal 550 s (157.1 draft mean moves x 3.5), hard 848 s (242.4 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. Top-row chains barely change the simulated length; the simulator owns harvest tier 3, which removes every chain.

### e5-l04: The middle wall

- **Idea:** Test: a full row of chains across the middle with heavy steel, before Reaction 2.
- **Feeling:** pressure.
- **Duration:** Sim `greedy` 152 to 171 mean moves on normal (170 gems), 231 to 261 on hard (255 gems); about 532 to 598 s of play on normal and 808 to 914 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 60 % of players win on the first try; hard: at least 45 %. Human rates are provisional until a playtest, and within the episode no level's is more than 10 points below the next level's. The human targets are for a player who has bought the tiers that were on offer, except the optional obstacle removers (ward tiers 2 and 3, harvest tiers 3 and 4); a player who owns those finds the level lighter, which is their reward. Simulator: the e5 row of balance.md (`random` informational) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft.
- **Takeaway:** The player knows a full row of chained pieces cannot be swapped, so no swap moves a piece into or along row 4 until a link is matched, blasted or drops, and that blasts on the row open it faster than single runs.
- **Role and levers:** test; goal size: 170 gems; complexity: steel_rate 0.05 and 8 chained cells filling row 4 (y = 4).
- **First contact:** Tests the episode so far: a full row of chains across the middle of the board (row 4) with steel at 0.05. Chained pieces cannot be swapped (OBS-05), so no swap can move a piece into or along row 4 until links clear; a vertical run through a link still clears it, and links fall when a piece under them clears. Obstacles stay below the maximums so the spike (e5-l08) can ask more. Clock tier 3 becomes buyable here; optional. The Reaction 2 bonus level follows.
- **Clock:** Normal 565 s (161.4 draft mean moves x 3.5), hard 861 s (246.0 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. First test of the episode.

### e5-l05: Open court

- **Idea:** Rest: with 8 gems on the board and almost no obstacles, play for gem-carrying runs of 4 or more.
- **Feeling:** relief.
- **Duration:** Sim `greedy` 154 to 174 mean moves on normal (175 gems), 238 to 268 on hard (265 gems); about 539 to 609 s of play on normal and 833 to 938 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 70 % of players win on the first try; hard: at least 55 %. Human rates are provisional until a playtest, and within the episode no level's is more than 10 points below the next level's. The human targets are for a player who has bought the tiers that were on offer, except the optional obstacle removers (ward tiers 2 and 3, harvest tiers 3 and 4); a player who owns those finds the level lighter, which is their reward. Simulator: the e5 row of balance.md (`random` informational) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft.
- **Takeaway:** The player knows that with gems on board at tier 3 (8 gems) a run of 4 or more that includes gems both collects them and leaves a power-up, so on a light board those runs are worth waiting for.
- **Role and levers:** rest; goal size: 175 gems; complexity: steel_rate 0.02 and 2 chained cells, the two bottom corners.
- **First contact:** A rest after the first test and the Reaction 2 bonus level: the lightest load of the episode (guidance minimums: steel 0.02, 2 chains in the bottom corners) at the curve's next goal, so the relief is in fewer obstacles, not a shorter level. Unlike e4-l05's open board, the board now carries 8 gems for a player who bought gems on board tier 3 at e5-l02 (the simulator does), so gem-carrying runs of 4 come up often. Time tier 4 becomes buyable here; optional.
- **Clock:** Normal 573 s (163.8 draft mean moves x 3.5), hard 885 s (252.8 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. A rest by load, not by length.

### e5-l06: The cradle

- **Idea:** A U of chains on the lower half: the lower side columns joined at the bottom corners, where a link has no neighbour on its outer side.
- **Feeling:** frustration, then release.
- **Duration:** Sim `greedy` 162 to 182 mean moves on normal (180 gems), 249 to 281 on hard (270 gems); about 567 to 637 s of play on normal and 872 to 984 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 60 % of players win on the first try; hard: at least 45 %. Human rates are provisional until a playtest, and within the episode no level's is more than 10 points below the next level's. The human targets are for a player who has bought the tiers that were on offer, except the optional obstacle removers (ward tiers 2 and 3, harvest tiers 3 and 4); a player who owns those finds the level lighter, which is their reward. Simulator: the e5 row of balance.md (`random` informational) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft.
- **Takeaway:** The player knows a link in a side column has no neighbour on its outer side, so it is freed by a vertical run along the edge, a run reaching in from the board, or a blast next to it.
- **Role and levers:** twist; goal size: 180 gems; complexity: steel_rate 0.04 and 10 chained cells: the bottom four cells of both side columns (x = 0 and x = 7) and the bottom-row cells next to each corner.
- **First contact:** Twists the chained columns of e4-l06: the columns move to the board's edges, where a link has no partner on its outer side and fewer runs can reach it; the bottom-row cells next to each corner are chained too. Steel at 0.04. Paint tier 4 becomes buyable here; optional.
- **Clock:** Normal 602 s (172.1 draft mean moves x 3.5), hard 927 s (264.9 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. Edge links barely change the simulated length.

### e5-l07: Iron neighbours

- **Idea:** Steel beside chains: with ward tier 3, fire the ward when steel sits next to the chains, so it frees them too.
- **Feeling:** satisfaction.
- **Duration:** Sim `greedy` 166 to 188 mean moves on normal (185 gems), 255 to 288 on hard (280 gems); about 581 to 658 s of play on normal and 892 to 1008 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 58 % of players win on the first try; hard: at least 43 %. Human rates are provisional until a playtest, and within the episode no level's is more than 10 points below the next level's. The human targets are for a player who has bought the tiers that were on offer, except the optional obstacle removers (ward tiers 2 and 3, harvest tiers 3 and 4); a player who owns those finds the level lighter, which is their reward. Without ward tier 3 the same steel is cleared by tnt and lightning, which also reach the chain beside it; ward tier 3 makes the lesson a single well-timed ward. Simulator: the e5 row of balance.md (`random` informational) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft.
- **Takeaway:** The player knows to hold the ward until steel sits beside the chains, because with ward tier 3 that ward frees those chains too, and without it a tnt or lightning on that steel does the same.
- **Role and levers:** develop; goal size: 185 gems; complexity: steel_rate 0.06 and 6 chained cells spread over rows 3 to 7, no two orthogonally adjacent.
- **First contact:** Develops ward with its optional tier 3 (buyable here, TOT-14: the ward also clears every piece orthogonally next to a destroyed steel piece): steel at the maximum 0.06 lands among six chains spread over rows 3 to 7, so steel often ends beside a link. At tier 3 the ward has two uses a level (TOT-06), but tier 2's effect stays (no new steel after the first ward), so only the first ward meets real steel and the lesson is timing: fire it when steel crowds the chains, not at the first steel. A player without tier 3 meets the same choice with tnt (its 3x3 and more) and lightning, which clear steel and the cells around or in line with it. Ward tier 3 and Score Frenzy tier 4 (30 s) become buyable here; optional.
- **Clock:** Normal 620 s (177.1 draft mean moves x 3.5), hard 950 s (271.5 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. The simulator buys ward tier 3 here as it becomes available.

### e5-l08: Chessboard floor

- **Idea:** Episode spike: both obstacles at their maximums with chains on one colour of a chessboard floor, before Attention 2.
- **Feeling:** pressure.
- **Duration:** Sim `greedy` 168 to 190 mean moves on normal (190 gems), 258 to 291 on hard (285 gems); about 588 to 665 s of play on normal and 903 to 1018 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 50 % of players win on the first try; hard: at least 35 %. Human rates are provisional until a playtest, and within the episode no level's is more than 10 points below the next level's. The human targets are for a player who has bought the tiers that were on offer, except the optional obstacle removers (ward tiers 2 and 3, harvest tiers 3 and 4); a player who owns those finds the level lighter, which is their reward. Simulator: the e5 row of balance.md (`random` informational) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft.
- **Takeaway:** The player knows that with both obstacles at their maximums the blasts must be shared, and that a blast on the floor frees several links while a run of 3 or 4 frees at most two.
- **Role and levers:** test; goal size: 190 gems; complexity: steel_rate 0.06 and 10 chained cells on one colour of a chessboard over the bottom three rows: alternate cells of rows 7 and 6, and two cells of row 5 (x = 0 and x = 6), so no two links are side by side.
- **First contact:** The episode spike: both obstacles at the guidance maximums (steel 0.06, 10 chains) on one colour of a chessboard over the bottom three rows, before the Attention 2 bonus level. No two links are side by side, so a run of 3 or 4 frees at most two links (a run of 5 along a row can free three), but a blast on the floor frees several. One ward cannot cover the level for a player without ward tier 2. Timer tier 4 (+120 s) becomes buyable here; optional.
- **Clock:** Normal 627 s (179.2 draft mean moves x 3.5), hard 959 s (274.1 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. The spike; timer tier 4 (+120 s) arrives on top for a player who buys it.

### e5-l09: Still water

- **Idea:** Rest: a light board with the chains held up in the centre, leaving room to plan totems two moves ahead.
- **Feeling:** relaxed.
- **Duration:** Sim `greedy` 174 to 196 mean moves on normal (195 gems), 268 to 302 on hard (295 gems); about 609 to 686 s of play on normal and 938 to 1057 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 60 % of players win on the first try; hard: at least 45 %. Human rates are provisional until a playtest, and within the episode no level's is more than 10 points below the next level's. The human targets are for a player who has bought the tiers that were on offer, except the optional obstacle removers (ward tiers 2 and 3, harvest tiers 3 and 4); a player who owns those finds the level lighter, which is their reward. Simulator: the e5 row of balance.md (`random` informational) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft.
- **Takeaway:** The player knows that two swaps in a row whose own matches both have runs of the same two colours fire both of those totems (TOT-03, TOT-04), and that a light board is where they can afford to look for them.
- **Role and levers:** rest; goal size: 195 gems; complexity: steel_rate 0.03 and 4 chained cells in the centre of rows 3 and 4.
- **First contact:** A rest after the e5-l08 spike and the Attention 2 bonus level, at e4-l09's obstacle load (steel 0.03, 4 chains) with the chains in the centre of rows 3 and 4 instead of the floor, and the curve's next goal. With the obstacles light, the level leaves room, without setting it up, for the hardest totem plan in the rules: a swap whose step 0 has runs of two kinds charges both totems (TOT-04), and a second swap in a row with runs of the same two kinds fires both (TOT-03, TOT-05). Gift tier 4 and harvest tier 4 become buyable here; optional.
- **Clock:** Normal 646 s (184.6 draft mean moves x 3.5), hard 998 s (285.1 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. A rest by load at the curve's next goal; simulated moves still rise with the goal.

### e5-l10: The locked heart

- **Idea:** The goal reaches its cap: a centre block of chains with heavy steel.
- **Feeling:** determination.
- **Duration:** Sim `greedy` 178 to 201 mean moves on normal (200 gems), 275 to 310 on hard (300 gems); about 623 to 704 s of play on normal and 962 to 1085 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 50 % of players win on the first try; hard: at least 35 %. Human rates are provisional until a playtest, and within the episode no level's is more than 10 points below the next level's. The human targets are for a player who has bought the tiers that were on offer, except the optional obstacle removers (ward tiers 2 and 3, harvest tiers 3 and 4); a player who owns those finds the level lighter, which is their reward. Simulator: the e5 row of balance.md (`random` informational) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft.
- **Takeaway:** The player knows the game's largest goal (200 gems) can be met with the board's centre locked: runs below the block make its links fall, and runs through it or blasts on it clear them.
- **Role and levers:** test; goal size: 200 gems; complexity: steel_rate 0.05 and 10 chained cells in a centre block: a 4 x 2 block over rows 3 and 4 plus the two centre cells of row 5.
- **First contact:** Tests the episode at the gem goal's cap (200 normal, 300 hard): a block of 10 chains in the centre of the board with steel at 0.05. The centre is where most runs happen, so the block takes those cells away, and its links fall as pieces under them clear. Every tier is now buyable; nothing new unlocks.
- **Clock:** Normal 662 s (189.2 draft mean moves x 3.5), hard 1025 s (292.8 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. The gem goal reaches its cap; the longest level of the episode in simulated moves.

### e5-l11: Last light

- **Idea:** Rest before the finale at the capped goal: two top-row links fall the whole board while the player works the open lower half.
- **Feeling:** unhurried.
- **Duration:** Sim `greedy` 177 to 199 mean moves on normal (200 gems), 270 to 304 on hard (300 gems); about 620 to 696 s of play on normal and 945 to 1064 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 60 % of players win on the first try; hard: at least 45 %. Human rates are provisional until a playtest, and within the episode no level's is more than 10 points below the next level's. The human targets are for a player who has bought the tiers that were on offer, except the optional obstacle removers (ward tiers 2 and 3, harvest tiers 3 and 4); a player who owns those finds the level lighter, which is their reward. Simulator: the e5 row of balance.md (`random` informational) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft.
- **Takeaway:** The player knows to spend a light 200-gem level building power-ups on the lower half instead of chasing the two top-row links, which fall to them anyway.
- **Role and levers:** rest; goal size: 200 gems; complexity: steel_rate 0.03 and 3 chained cells: two on the top row (x = 1 and x = 6) and one in the centre of row 5.
- **First contact:** A rest between the e5-l10 test and the finale, at the same 200 / 300 goal: light obstacles (steel 0.03, 3 chains, two on the top row that fall and one low in the centre), so the relief is in the load and the goal stays the cap. It mixes everything at its lightest since e5-l05.
- **Clock:** Normal 658 s (188.1 draft mean moves x 3.5), hard 1005 s (287.2 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. Same goal as e5-l10 at a lighter load, so slightly fewer simulated moves.

### e5-l12: The treasure

- **Idea:** Finale: both obstacles at their maximums with chains in five rows from row 1 to the bottom, the campaign's last level.
- **Feeling:** triumph.
- **Duration:** Sim `greedy` 177 to 200 mean moves on normal (200 gems), 273 to 308 on hard (300 gems); about 620 to 700 s of play on normal and 956 to 1078 s on hard at 3.5 s a move.
- **Target success rate:** Normal: at least 45 % of players win on the first try; hard: at least 30 %. Human rates are provisional until a playtest, and within the episode no level's is more than 10 points below the next level's. The human targets are for a player who has bought the tiers that were on offer, except the optional obstacle removers (ward tiers 2 and 3, harvest tiers 3 and 4); a player who owns those finds the level lighter, which is their reward. Simulator: the e5 row of balance.md (`random` informational) and the curve rules as `--check` runs them; `greedy` wins 100 % on the draft.
- **Takeaway:** The player knows they can win the campaign's last level, at the game's largest goal and heaviest load, with chains falling from five rows at once: the Explorer trophy on normal and Legend on hard.
- **Role and levers:** test; goal size: 200 gems; complexity: steel_rate 0.06 and 10 chained cells, two in each of rows 1, 2, 4, 5 and 7, mirrored left to right.
- **First contact:** The finale and the campaign's last level (the Explorer and Legend trophies are for winning it on normal and hard): both obstacles at the guidance maximums with chains in five rows from row 1 to the bottom row, so the upper links fall into the steel while the bottom ones hold. It mixes every lesson of episodes 3 to 5 at the capped goal (200 / 300).
- **Clock:** Normal 661 s (188.8 draft mean moves x 3.5), hard 1018 s (290.9 x 3.5) of human play; the `base_clock` in the level file is the [clock rule](balance.md#clock)'s fixed point. The finale; the simulator's owned obstacle removers keep it close to e5-l10 in moves, and the human targets carry the step up.

| Level | File | Gems normal / hard | Totems | Power-ups | Obstacles | Seed |
|---|---|---|---|---|---|---|
| e5-l01 | [`l01.json`](../../game/data/levels/e5/l01.json) | 155 / 235 | all seven | all six | steel 0.03, 4 chains | 20090801 |
| e5-l02 | [`l02.json`](../../game/data/levels/e5/l02.json) | 160 / 240 | all seven | all six | steel 0.04, 6 chains | 20090802 |
| e5-l03 | [`l03.json`](../../game/data/levels/e5/l03.json) | 165 / 250 | all seven | all six | steel 0.03, 8 chains | 20090803 |
| e5-l04 | [`l04.json`](../../game/data/levels/e5/l04.json) | 170 / 255 | all seven | all six | steel 0.05, 8 chains | 20090804 |
| e5-l05 | [`l05.json`](../../game/data/levels/e5/l05.json) | 175 / 265 | all seven | all six | steel 0.02, 2 chains | 20090805 |
| e5-l06 | [`l06.json`](../../game/data/levels/e5/l06.json) | 180 / 270 | all seven | all six | steel 0.04, 10 chains | 20090806 |
| e5-l07 | [`l07.json`](../../game/data/levels/e5/l07.json) | 185 / 280 | all seven | all six | steel 0.06, 6 chains | 20090807 |
| e5-l08 | [`l08.json`](../../game/data/levels/e5/l08.json) | 190 / 285 | all seven | all six | steel 0.06, 10 chains | 20090808 |
| e5-l09 | [`l09.json`](../../game/data/levels/e5/l09.json) | 195 / 295 | all seven | all six | steel 0.03, 4 chains | 20090809 |
| e5-l10 | [`l10.json`](../../game/data/levels/e5/l10.json) | 200 / 300 | all seven | all six | steel 0.05, 10 chains | 20090810 |
| e5-l11 | [`l11.json`](../../game/data/levels/e5/l11.json) | 200 / 300 | all seven | all six | steel 0.03, 3 chains | 20090811 |
| e5-l12 | [`l12.json`](../../game/data/levels/e5/l12.json) | 200 / 300 | all seven | all six | steel 0.06, 10 chains | 20090812 |

Every episode 5 level has all seven totems and six power-ups unlocked.
`chained_cells` and `steel_rate` are defined in [level data](level-data.md);
y = 0 is the top row. Each sets `base_clock` from the
[clock rule](balance.md#clock): 630 to 770 s on normal and 950 to 1180 s
on hard, peaking at e5-l10 (and e5-l12 on hard).

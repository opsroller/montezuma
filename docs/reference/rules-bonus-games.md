<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Bonus games rules

The design reference for the four bonus games, each played at two
difficulty levels, for 8 bonus levels in all [S1, S6]. Terms and
certainty marks are in the [rules spec](rules-spec.md); sources in
[sources](rules-sources.md). These are WP-30 to WP-33, one game each.

## Common rules (`BG-*`)

1. `BG-01` A bonus level is played once, right after the level that
   comes before it in [the progression](progression.md#episodes).
   Passing it awards 1 golden star [S1, S3]; failing awards nothing and
   the game continues.
2. `BG-02` A passed or failed bonus level can be replayed from the
   episode map for practice; replays award no stars [chosen].
3. `BG-03` Every bonus game uses the level's seeded generator, so a seed
   replays the same game.
4. `BG-04` Tier 2 of each game is its second appearance: faster or
   larger, as in each table [S1].
5. `BG-05` The bonus games use the board's artifact pieces as their
   tokens; they need no match-3 rules.

## Accuracy: `bg_accuracy`

Catch the stars before they leave the screen [S1].

| Rule | Tier 1 | Tier 2 |
|---|---|---|
| Stars in the round | 30 | 30 |
| A new star every | 1.0 s | 0.75 s |
| A star stays on screen | 2.5 s | 2.0 s |
| Pass when missed at most | 3 | 2 |

- `BG-10` Stars enter from a random screen edge and cross the screen on a
  straight line at constant speed. A tap within the star's radius
  catches it (radius at least 44 points on touch screens [chosen]).
- `BG-11` A tap on empty space does nothing [chosen].

## Memory: `bg_memory`

Find the pairs of tokens hidden under plates, then do it twice more [S1].

| Rule | Tier 1 | Tier 2 |
|---|---|---|
| Rounds | 3 | 3 |
| Plates per round | 8, 12, 16 | 12, 16, 20 |
| Show at the start of a round | 2 s | 1.5 s |
| Time for all rounds | 120 s | 100 s |

- `BG-20` Each round shows every token for the start time, then covers
  them. The player turns two plates; a pair stays open, a mismatch closes
  after 0.8 s. The round ends when every pair is open.
- `BG-21` Pass when all three rounds are cleared within the time.

## Reaction: `bg_reaction`

Plates turn over briefly; press Stop when the target token shows. Mistakes
cost time [S1].

| Rule | Tier 1 | Tier 2 |
|---|---|---|
| Plates | 9 (3 × 3) | 9 |
| A plate shows its token for | 0.7 s | 0.5 s |
| Gap between plates | 0.4 s | 0.3 s |
| Chance the shown token is the target | 25 % | 25 % |
| Time | 45 s | 45 s |
| Penalty for a wrong Stop | −3 s | −3 s |
| Pass at hits | 10 | 12 |

- `BG-30` The target kind is shown above the plates for the whole game.
  One plate at a time turns over. Stop while a target shows is a hit; Stop
  at any other time is a mistake.

## Attention: `bg_attention`

Follow the plate that hides the token while the plates shuffle [S1].

| Rule | Tier 1 | Tier 2 |
|---|---|---|
| Plates | 3 | 3 |
| Tries | 10 | 10 |
| Pass at correct guesses | 8 | 8 |
| Swaps per shuffle | 4 + try number | 6 + try number |
| First swap duration | 0.6 s | 0.45 s |
| Each try's swaps are faster by | 8 % | 8 % |

- `BG-40` Each try: the token is shown under one plate for 1 s, the
  plates swap in pairs, then the player picks one. Each guess speeds up
  the next shuffle [S1].

## Acceptance checks

| Rule | Given | When | Then |
|---|---|---|---|
| `BG-01` | First play of a bonus level | the player passes | +1 golden star |
| `BG-02` | A replay | the player passes | no star |
| Accuracy | Tier 1 | 4 stars leave uncaught | failed |
| Memory | Tier 1 | round 3 cleared at 119 s | passed |
| Reaction | Tier 1, 9 hits, 1 mistake at 40 s | time runs out | failed |
| Attention | 8 of 10 correct | the 10th try ends | passed |

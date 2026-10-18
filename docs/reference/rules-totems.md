<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Power Totems rules

The design reference for Power Totems in the `classic` rule set. Terms and
certainty marks are defined in the [rules spec](rules-spec.md); evidence
ids `[S*]` are in [sources](rules-sources.md). The code's reference page is
`totems.md` (with WP-06).

## Purpose

Totems reward planning two moves ahead in one colour: a player who sees a
second match of the same kind coming gets a strong, visible payoff.

## State

Per totem: `tier` (0 = not owned, 1..4; ward 1..3), `charge` (0 or 1),
`uses_left` this level.

## Ids

One totem per kind [S1, S4, S5, S8]. The id is the contract id.

| Kind | Id | Effect in one line |
|---|---|---|
| red (0) | `fire` | Fireballs clear random pieces |
| orange (1) | `time` | Adds time |
| yellow (2) | `gold` | Adds points, later starts Score Frenzy |
| green (3) | `harvest` | Collects every gem on the board |
| blue (4) | `ward` | Destroys steel pieces |
| purple (5) | `gift` | Places power-ups on the board |
| white (6) | `paint` | Repaints pieces to one kind |

## Rules

Charging and firing:

1. `TOT-01` A totem takes part only while `tier >= 1` and `uses_left > 0`.
2. `TOT-02` After each move, look at the kinds of the runs cleared in
   **step 0** (the swap's own match). For each taking-part totem: if its
   kind is among them, `charge` +1, otherwise `charge` = 0
   [S1, S4, S5, S3]. Cascades, power-ups and totem effects never charge
   [chosen].
3. `TOT-03` A totem whose `charge` reaches 2 fires at the end of that
   move (`RES-07`): `charge` = 0, `uses_left` −1. Two moves in a row with
   a run of its kind fire it; a third in a row charges it to 1 again.
4. `TOT-04` A swap whose step 0 has runs of two kinds charges both
   totems.
5. `TOT-05` Several totems firing on one move fire one at a time in
   kind order. Each effect is chosen on the board as the previous one
   left it, and its clears resolve (fall, refill, cascades) before the
   next is chosen [chosen].
6. `TOT-06` `uses_left` starts each level at 1 (tiers 1-2) or 2
   (tiers 3-4) [S1]. A spent totem shows empty and does not charge.
7. `TOT-07` Illegal swaps, shuffles and hints do not change any charge.

Effects (each tier replaces the one before):

8. `TOT-10` **fire**: launches fireballs; each clears *k* distinct random
   pieces (steel included). All targets are distinct across fireballs.
9. `TOT-11` **time**: adds seconds to `time_left`.
10. `TOT-12` **gold**: adds points (the first and second use of a level
    can differ); from tier 2 also starts Score Frenzy at once
    (`FRZ-03`).
11. `TOT-13` **harvest**: collects every gem on the board (the pieces
    stay); from tier 2 then places *g* new gems on random plain pieces,
    ignoring `gems_max` for this placement; from tier 3 also removes every
    chain.
12. `TOT-14` **ward**: destroys every steel piece; from tier 2 no steel
    spawns for the rest of the level; at tier 3 also clears every piece
    orthogonally next to a destroyed steel piece.
13. `TOT-15` **gift**: places *n* power-ups, each drawn uniformly from the
    unlocked power-ups, on random plain pieces, up to 8 power-ups on the
    board.
14. `TOT-16` **paint**: picks a target kind uniformly among the kinds on
    the board, then repaints *n* random non-steel pieces of other kinds to it
    (gems and power-ups stay on their pieces). Runs this makes resolve as steps.
15. `TOT-17` Random choices draw from the level's seeded generator in the
    order listed in each rule.

## Numbers

| Totem | Tier 1 | Tier 2 | Tier 3 | Tier 4 | Source |
|---|---|---|---|---|---|
| `fire` | 2 fireballs × 3 pieces | 4 × 3 | 2 × 4 and 2 × 5 | 4 × 5 | [S1]; 3 per small fireball [chosen] |
| `time` | +60 s | +120 s | +180 s | +240 s | [S1]; tier 1 [chosen] |
| `gold` | +1500 | +2000, Frenzy | +2000 then +2500, Frenzy | +2500 then +3000, Frenzy | [S1] |
| `harvest` | collect | collect, +8 gems | collect, +9 gems, chains | collect, +10 gems, chains | [S1] |
| `ward` | destroy steel | + no new steel | + clear neighbours | — | [S1] |
| `gift` | 1 power-up | 2 | 3 | 4 | [S1] |
| `paint` | 4 pieces | 6 | 8 | 10 | [S1] "several, moderate, big, huge"; counts [chosen] |
| uses per level | 1 | 1 | 2 | 2 | [S1] |

When each totem and tier becomes available, and what tiers cost:
[progression](progression.md#unlocks).

## Edge cases

- `fire` with fewer pieces on the board than targets clears what there
  is.
- `harvest` with no gems on the board still places its new gems.
- `ward` with no steel still sets its tier-2 "no new steel" state.
- `paint` with one kind on the board does nothing (no other kind to
  repaint) but still spends its use.
- `gift` with no plain piece places nothing.
- A totem's clear that wins the level still resolves fully (`GOAL-02`).

## Feedback

The game emits `totem_charge_changed(id, charge / 2.0)` after each move
for each owned totem (0, 0.5 or 1 just before it resets), plays
`totem_charge` when a charge reaches 1 and `totem_fire_<id>` when it
fires.

## Acceptance checks

| Rule | Given | When | Then |
|---|---|---|---|
| `TOT-02` | `fire` owned, charge 0 | a swap's own match is red | `fire` charge 1 |
| `TOT-02` | `fire` charge 1 | a swap's own match is blue and a cascade is red | `fire` charge 0 |
| `TOT-03` | `fire` charge 1 | a swap's own match is red | `fire` fires after the cascades, charge 0, uses −1 |
| `TOT-04` | `fire` and `time` charge 1 | a swap matches red and orange at once | both fire, `fire` first |
| `TOT-06` | `time` tier 1 used once | two orange moves in a row | nothing fires, charge stays 0 |
| `TOT-10` | `fire` tier 1, seed fixed | it fires | 6 distinct pieces clear; same seed gives the same 6 |
| `TOT-13` | `harvest` tier 2, 5 gems on board | it fires | `gems_collected` +5, then 8 new gems are on the board |
| `TOT-16` | `paint` tier 1 | it fires | 4 pieces now have the target kind; any runs resolve |

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Power-ups rules

The design reference for power-ups (the original's "bonuses") in the
`classic` rule set. Terms and certainty marks are defined in the
[rules spec](rules-spec.md); evidence ids `[S*]` are in
[sources](rules-sources.md). The code's reference page is
`bonuses.md` (with WP-07).

## Purpose

Power-ups reward long matches with a burst that is placed on the board,
not held in a pocket: the player then plans how to reach and match it.

## State

Per power-up: `tier` (0 = not unlocked, 1..3). On the board: which pieces
carry which power-up (at most 4 at once, 8 with `gift`).

## Ids

| Id | Original name | Effect in one line |
|---|---|---|
| `tnt` | TNT | Clears an area around its piece |
| `color_clear` | Solidarity | Clears every piece of its piece's kind |
| `lightning` | Flash | Clears lines from its piece |
| `clock` | Clock | Adds seconds |
| `multiplier` | Score Multiplier | Multiplies the move's points |
| `hint` | Adviser | Shows every legal move for a while |

The six power-ups are the original's six [S1, S2, S8].

## Rules

Earning:

1. `PWR-01` For each run of 4 cleared in any step, one power-up is placed
   after that step's refill; for each run of 5 or more, two
   [S4, chosen counts]. An L or T shape counts as its two runs.
2. `PWR-02` Each placed power-up is drawn uniformly from the unlocked
   power-ups and put on a random plain piece anywhere on the board
   [S5, chosen placement]. At 4 power-ups on the board, further ones are
   not placed.
3. `PWR-03` A power-up stays on its piece until that piece is cleared;
   it falls and shuffles with it. It does not carry over to the next
   level.

Triggering:

4. `PWR-04` A power-up goes off when its piece is cleared by anything:
   a run, another power-up, or a totem [S5]. Its cells are added to the
   same step's clear. A power-up inside that area goes off too, in the
   same step, until no new one is reached.
5. `PWR-05` Pieces cleared by a power-up collect gems, score and fill the
   Frenzy bar like matched pieces (`RES-03`). They do not charge totems.

Effects (each tier replaces the one before; areas are clipped to the
board):

6. `PWR-06` **tnt**: clears the cells around its piece: tier 1 the 3×3
   square (9), tier 2 the diamond of radius 2 (13), tier 3 the 5×5
   square without its corners (21) [S1]. Clears steel.
7. `PWR-07` **color_clear**: clears every non-steel piece of its piece's
   kind; tier 2 also that kind's steel pieces; tier 3 also every piece
   orthogonally next to a cleared one [S1].
8. `PWR-08` **lightning**: clears from its piece: tier 1 every cell above
   it in its column; tier 2 above plus the whole row; tier 3 the whole
   row and column [S1, chosen tiers]. Clears steel.
9. `PWR-09` **clock**: adds 15, 20, 25 s [S1].
10. `PWR-10` **multiplier**: the move it goes off in scores × 5, × 7,
    × 10 [S1; tier 1 chosen]. Several in one move do not stack; the
    highest applies.
11. `PWR-11` **hint**: highlights every legal swap for 20, 25, 30 s,
    updated after each move [S1; tier 1 chosen].

## Numbers

| Id | Tier 1 | Tier 2 | Tier 3 |
|---|---|---|---|
| `tnt` | 9 cells | 13 cells | 21 cells |
| `color_clear` | its kind | + its kind's steel | + neighbours |
| `lightning` | up | up + row | row + column |
| `clock` | +15 s | +20 s | +25 s |
| `multiplier` | × 5 | × 7 | × 10 |
| `hint` | 20 s | 25 s | 30 s |
| Power-ups per run of 4 / 5+ | 1 / 2 | | |
| On the board at once | 4 (8 through `gift`) | | |

When each becomes available and what tiers cost:
[progression](progression.md#unlocks).

## Not in this rule set

The earlier placeholders `hammer`, `shuffle`, `dynamite`, `freeze` and a
held inventory are not in the original and are dropped. The manual
shuffle is a rule of its own (`SHUF-01`).

## Edge cases

- A power-up whose piece becomes steel cannot happen: steel only comes
  from refills, and power-ups only sit on plain pieces.
- `color_clear` on a board where its kind has only its own piece clears
  just that piece.
- A `clock` that goes off after time ran out during the move still adds
  its seconds before the goal check.
- `hint` going off while another `hint` runs restarts the duration.

## Feedback

`bonus_earned` plays when a power-up is placed and `bonus_use_<id>` when
it goes off. The game emits `bonus_changed(id, count_on_board)`.

## Acceptance checks

| Rule | Given | When | Then |
|---|---|---|---|
| `PWR-01` | `tnt` unlocked | a swap makes a run of 4 | one `tnt` appears on a plain piece after the refill |
| `PWR-01` | | a swap makes a run of 5 | two power-ups appear |
| `PWR-02` | 4 power-ups on the board | a run of 4 | no power-up is added |
| `PWR-04` | `tnt` tier 1 on a piece in a run, a `clock` inside its 3×3 | the move resolves | the 3×3 and the run clear in one step and time rises 15 s |
| `PWR-07` | `color_clear` tier 1 on a blue piece, 9 blue pieces | it goes off | all 9 blue clear |
| `PWR-08` | `lightning` tier 1 at (3, 5) | it goes off | cells (3, 0) to (3, 4) clear with (3, 5) |
| `PWR-10` | `multiplier` tier 1 and a move worth 400 | it goes off | the move scores 2000 |

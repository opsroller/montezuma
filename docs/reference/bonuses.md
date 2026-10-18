<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Power-ups

Reference for on-board power-ups in the core rules library (WP-07): ids,
tiers, how they are placed, how they move and go off, what each one does,
and the C++ API. For gameplay programmers wiring power-ups into the game
and for the game designer checking the code against the rules.

Code: `core/include/montezuma/bonuses.hpp`, `core/src/bonuses.cpp`.
Tests: `core/tests/test_bonuses.cpp`. (The files keep their `bonuses.*`
names; the API calls the concept "power-up", the original's "bonus".)

The rules are the game designer's [power-ups rules](rules-power-ups.md)
(`PWR-01` to `PWR-11`, with `RES-*`, `GEM-*`, `OBS-*`, `SHUF-*` and
`SCORE-*` from the [rules spec](rules-spec.md)). The code cites the rule id
next to each rule as `// rule: <id>`.

## Model

Power-ups sit on board pieces. There is no inventory.

- **Earning** (`PWR-01`, `RES-05`). In **every step** of a move, each
  cleared run of exactly 4 earns **one** power-up and each run of 5 or more
  earns **two**; runs of 3 earn none. An L or T counts as its two runs. The
  steps of a totem's clear (`clear_cells`) earn the same way; its step 0
  has no runs. Earned power-ups are placed **after that step's refill**,
  before the next step looks for runs.
- **Placement** (`PWR-02`). Each one is drawn uniformly from the
  **unlocked** power-ups (tier 1 or more) and put on a random **plain
  piece** anywhere on the board: occupied, not steel, no gem, no power-up
  (`GEM-01`, `OBS-04`). Chained pieces are plain. At **4 power-ups on the
  board**, further ones are not placed (dropped, not kept). When no plain
  piece is left, they wait in `PowerUpLayer::pending` and are placed after
  the next refill (rules spec edge case).
- **Staying** (`PWR-03`). A power-up stays on its piece until that piece is
  cleared, and falls and shuffles with it (`SHUF-03`, `GEM-05`). A
  **redeal** (`MoveResult::redealt`) keeps the same power-ups and puts them
  back on random plain pieces of the new board. A layer is per level; start
  a new one for the next level.
- **Going off** (`PWR-04`, `RES-02`). A power-up goes off when its piece is
  cleared by anything: a run, another power-up, or a totem. Its cells are
  added to the **same step's clear**, before the fall; a power-up inside
  that area goes off too, in the same step, until no new one is reached.
- **What its cells do** (`PWR-05`, `RES-03`). Pieces a power-up clears
  collect gems, score `10 x (step + 1)` and fill the Frenzy bar like
  matched pieces. They do not charge totems: `MoveResult::matched_kind`
  comes from the swap's own runs only, and `StepClear::power_up_cells`
  lists the cells power-ups added.
- **Tiers.** A runtime input from player progress (WP-08):
  `PowerUpTiers`, 0 = not unlocked, 1 to 3. Tests, the simulator and the web
  demo use `kAllPowerUpsTier1` until progress supplies real tiers. Unlock
  levels are in [progression](progression.md#unlocks) and belong to WP-08;
  this package does not store them.

## Ids and tiers

Stable `PowerUpId` order (the index into `PowerUpTiers` and the defs
table; new ids are appended, never reordered). Each tier replaces the one
before. Areas are clipped to the board.

| Index | `PowerUpId` | Id | Rule | Tier 1 | Tier 2 | Tier 3 |
|---|---|---|---|---|---|---|
| 0 | `Tnt` | `tnt` | `PWR-06` | 3x3 square (9 cells) | diamond of radius 2 (13) | 5x5 without its corners (21) |
| 1 | `ColorClear` | `color_clear` | `PWR-07` | every non-steel piece of its piece's kind | also that kind's steel | also every non-steel piece orthogonally next to a cleared one |
| 2 | `Lightning` | `lightning` | `PWR-08` | every cell above it in its column | above plus the whole row | the whole row and column |
| 3 | `Clock` | `clock` | `PWR-09` | +15 s | +20 s | +25 s |
| 4 | `Multiplier` | `multiplier` | `PWR-10` | the move scores x5 | x7 | x10 |
| 5 | `Hint` | `hint` | `PWR-11` | highlight every legal swap for 20 s | 25 s | 30 s |

- Every effect includes the power-up's own cell (its piece is the one
  being cleared). So lightning at the top edge still clears its own cell.
- **Steel** (`OBS-03`). TNT and lightning clear steel. `color_clear`
  clears its kind's steel only from tier 2, and its tier-3 neighbours never
  include steel of another kind.
- **Kinds** are `0 red`, `1 orange`, `2 yellow`, `3 green`, `4 blue`,
  `5 purple`, `6 white`. `color_clear` reads the kind of its piece from the
  board when it goes off, so a piece repainted by the paint totem
  (`TOT-16`) clears its new kind.
- **Multiplier** (`PWR-10`, `SCORE-04`). Several in one move do not stack;
  the highest applies to the whole move's points, after Frenzy doubling.
- **Clock** (`PWR-09`, `GOAL-05`). Clocks add up; there is no cap. A clock
  that goes off after time ran out during the move still adds its seconds
  before the goal check.
- **Hint** (`PWR-11`). Highlights every legal swap, updated after each
  move; a hint going off while one runs restarts the duration.

`power_up_name(id)` and `power_up_from_name(name)` convert between the
enum and the contract id (case-sensitive).

## Resolving a move

`swap_with_power_ups` and `clear_cells_with_power_ups` run a move through
`Board::try_swap` and `Board::clear_cells` with `StepHooks`
([move-result.md](move-result.md#step-hooks)). Per step:

1. `extend_clear`: `layer.expand_clear(board, cleared, step)` returns the
   step's full clear (`PWR-04`); the board clears those cells too.
2. The board empties those cells, then falls and refills.
3. `after_refill`: `layer.apply_step(step)` drops the power-ups that went
   off and moves the rest with their falling pieces; then
   `layer.place_for_step(step.runs, ...)` places what the step earned
   (`PWR-01`, `PWR-02`).

After the last step, `layer.apply_shuffle(result, ...)` follows the
automatic reshuffle or redeal. The game layer then applies the report:

```cpp
PowerUpMoveReport rep;
auto r = swap_with_power_ups(board, layer, a, b, rng, tiers, rep);  // empty if illegal
// r->score() x rep.score_multiplier (after Frenzy doubling)
// time_left += rep.time_delta_seconds
// if (rep.hint_seconds > 0) (re)start the hint for that long
// per step: rep.steps[i].clear.triggered -> bonus_use_<id> cues,
//           rep.steps[i].placed          -> bonus_earned cues
```

`rng` is the level's generator for power-up draws (the board refills from
its own). Both functions throw `std::invalid_argument`, before changing
anything, for a bad tier table or a layer whose size differs from the
board's. An illegal swap returns empty and changes nothing.

## API

All in namespace `montezuma`; no engine headers.

### Constants

| Name | Value | Rule |
|---|---|---|
| `kPowerUpsPerRunOf4`, `kPowerUpsPerRunOf5` | 1, 2 | `PWR-01` |
| `kMaxPowerUpsOnBoard` | 4 | `PWR-02` |
| `kMaxPowerUpsOnBoardGift` | 8 | `PWR-02`, `TOT-15` |
| `kAllPowerUpsTier1` | every tier 1 | default for tests, sim, demo |

### Defs table

```cpp
struct PowerUpTierParams {
    int area_chebyshev = -1;       // area: |dx|,|dy| <= this (-1 = no area) ...
    int area_manhattan = 0;        // ... and |dx| + |dy| <= this
    unsigned rays = kRayNone;      // kRayUp | kRayDown | kRayLeft | kRayRight
    bool kind_clear = false;       // every non-steel piece of its kind
    bool kind_steel = false;       // also that kind's steel
    bool kind_neighbours = false;  // also non-steel orthogonal neighbours
    double time_seconds = 0.0;
    int score_multiplier = 1;
    double hint_seconds = 0.0;
};
struct PowerUpDef { PowerUpId id; std::string_view name; std::array<PowerUpTierParams, 3> tiers; };
```

- `power_up_defs()` is the table, `power_up_def(id)` one row,
  `PowerUpDef::at_tier(tier)` one tier (clamped to 1..3).
- TNT's three shapes are areas: tier 1 `(1, 2)`, tier 2 `(2, 2)`, tier 3
  `(2, 3)`.
- `validate_power_up_defs(span)` throws `std::invalid_argument` unless
  every id appears once in order with a unique non-empty name and every
  tier is well formed. The built-in table is checked at compile time.
- `validate_power_up_tiers(tiers)` throws `std::invalid_argument` for a
  tier outside 0..3.

### Piece marks

`Board` does not model steel or gems yet. `PieceMarks { steel, gem }` are
row-major `std::vector<bool>` the caller passes (empty means none). They
keep power-ups off steel and gem pieces and decide which steel an effect
takes. Inside one move they are read as given for every step; once steel
and gems live in `Board`, the marks move with their pieces there.

### Effects

```cpp
struct PowerUpEffect {
    PowerUpId id;
    int tier;
    std::vector<Cell> cells_to_clear;   // occupied, in bounds, row-major, unique; holds its own cell
    double time_delta_seconds;          // 0 unless clock
    int score_multiplier;               // 1 unless multiplier
    double hint_seconds;                // 0 unless hint
};

PowerUpEffect power_up_effect(PowerUpId id, int tier, const Board& board, Cell at,
                              const PieceMarks& marks = {}) noexcept;
PowerUpEffect power_up_effect(const TriggeredPowerUp& t, const Board& board,
                              const PieceMarks& marks = {}) noexcept;
int move_score_multiplier(std::span<const PowerUpEffect>) noexcept;   // highest, 1 if none
int power_ups_earned_by(std::span<const Run> runs) noexcept;          // PWR-01
```

Effects are computed on the board just before the step's clear, with the
power-up's piece still in place. `tier` clamps to 1..3. An out-of-bounds
`at` gives no cells but keeps the value fields. An effect never changes the
board.

### Layer

```cpp
struct PowerUp { PowerUpId id; int tier; };
struct PowerUpPlacement { Cell cell; PowerUp power_up; };
struct TriggeredPowerUp { Cell cell; PowerUp power_up; int step; };
struct StepClear {
    std::vector<Cell> cells;            // the step's full clear, row-major
    std::vector<Cell> power_up_cells;   // cells only power-ups added (PWR-05)
    std::vector<TriggeredPowerUp> triggered;
    std::vector<PowerUpEffect> effects; // parallel to triggered
};

struct PowerUpLayer {
    int width, height;
    std::vector<std::optional<PowerUp>> cells;           // row-major, y * width + x
    std::vector<std::optional<PowerUpId>> pending;       // waiting for a plain piece
    ...
};
```

`PowerUpLayer` is plain data (public members, `operator==`), so it can be
saved with a level in progress.

| Member | Meaning |
|---|---|
| `PowerUpLayer(w, h)`, `PowerUpLayer(const Board&)` | empty layer; throws `std::invalid_argument` for a size below 1 |
| `at`, `has`, `set`, `erase`, `clear`, `count`, `entries` | lookups and edits; out-of-bounds cells read as empty and are ignored on write; `clear` also empties `pending`; `count` is on the board only |
| `expand_clear(board, run_cells, step, marks) const` | the step's full clear with every power-up it reaches (`PWR-04`); `triggered` lists those on run cells first (row-major), then each one reached, in order |
| `apply_swap(a, b)`, `apply_step(step)` | follow a swap; drop power-ups on cleared cells and move the rest with the falls |
| `apply_shuffle(result, board, rng, marks)` | follow `shuffle_map`; on a redeal, re-place every power-up |
| `remap(result, board, rng, marks)` | `apply_swap`, `apply_step` per step, `apply_shuffle` |
| `triggered_by(result) const` | power-ups whose pieces a finished result cleared, by step then `cleared` order |
| `place_for_step(runs, board, rng, tiers, marks, max = 4)` | after a step's refill: place `pending`, then what `runs` earn |
| `place_random(n, board, rng, tiers, marks, max = 8)` | the gift totem's placement (`TOT-15`); nothing waits |

**Placement draws** (`PWR-02`). For each power-up, in order: if it has no
id yet, `rng.below(number unlocked)` over unlocked ids in `PowerUpId`
order; then `rng.below(number of plain pieces)` over plain pieces in
row-major order, minus cells already chosen. At the cap the rest are
dropped without drawing. With no plain piece left, the rest wait in
`pending` (`place_for_step`) or are dropped (`place_random`). Nothing is
drawn or kept when no power-up is unlocked. A waiting id whose tier is
now 0 is drawn again.

**Redeal draws.** For each power-up in row-major order of the layer before
the redeal: `rng.below(number of plain pieces)` on the new board. Id and
tier are kept.

The same seed and the same moves always give the same placements.

## Removed

The bonus inventory of the first WP-07 draft is gone: `hammer`, `shuffle`,
`dynamite` and `freeze`, stored stock and caps, upgrades by stock, and the
save text. `extra_time` became `clock`. The earlier power-up draft's
after-the-move `clear_cells` loop, `place_from_move`, `PowerUpSet`,
`PowerUp::kind` and `clear_colour_steel` are replaced by the per-step
resolution above.

## Open points

- `Board` has no steel, gems or chains yet; `PieceMarks` stands in, and
  does not follow pieces inside a move.
- `Board::shuffle()` (the player's manual shuffle, `SHUF-01`) reports no
  map, so the layer cannot follow it yet; the automatic reshuffle in a
  `MoveResult` is followed.

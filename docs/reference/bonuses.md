<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Power-ups

Reference for on-board power-ups in the core rules library (WP-07): ids,
tiers, how they are placed, how they move and trigger, what each one does,
and the C++ API. For gameplay programmers wiring power-ups into the game
and for the game designer checking the rules.

Code: `core/include/montezuma/bonuses.hpp`, `core/src/bonuses.cpp`.
Tests: `core/tests/test_bonuses.cpp`. (The files keep their `bonuses.*`
names; the API calls the concept "power-up".)

The rules come from the game designer (WP-03). Their spec, with rule ids,
is pending; the code marks each rule with `// rule: <tbd>` until then.
Points still to confirm are marked **[verify]** here and in the code.

## Model

Power-ups sit on board pieces. There is no inventory.

- A legal swap whose match has a **run of exactly 4** places **one**
  power-up on a random plain piece (a piece without a power-up). A **run of
  5 or more** places **two**. Runs of 3 place nothing.
- Runs are counted in `MoveResult::cascades[s].runs`, over **every cascade
  step** of the swap **[verify]**. A `clear_cells` result (a totem or
  power-up effect, `swap` empty) places nothing, even when its cascades
  make runs **[verify]**.
- The power-up placed is a uniform random pick among the **unlocked**
  ones **[verify]**, at the tier the player owns.
- A power-up moves with its piece through falls and shuffles. It
  **triggers when its piece is cleared** by anything (a match, a totem,
  another power-up), at the cell where the piece was cleared.
- A redeal (`MoveResult::redealt`) replaces every piece, so every power-up
  on the board is lost **[verify]**.
- Upgrades cost golden stars and belong to WP-08. Here the tier is an
  input (`PowerUpTiers`).

## Ids and tiers

Stable `PowerUpId` order (the index into `PowerUpSet`, `PowerUpTiers` and
the defs table; new ids are appended, never reordered):

| Index | `PowerUpId` | Contract id | L1 | L2 | L3 |
|---|---|---|---|---|---|
| 0 | `Tnt` | `tnt` | 3x3 around it (9 cells) | diamond of radius 2 (13) | 5x5 minus the 4 corners (21) |
| 1 | `ColorClear` | `color_clear` | every piece of its piece's colour | L1, plus that colour's steel pieces (flag) | L2, plus every piece orthogonally next to those cells **[verify]** |
| 2 | `Lightning` | `lightning` | the column above it | up, left and right | the full row and column |
| 3 | `Clock` | `clock` | +15 s | +20 s | +25 s |
| 4 | `Multiplier` | `multiplier` | the triggering move scores x5 | x7 | x10 |
| 5 | `Hint` | `hint` | highlight every legal move for 20 s | 25 s | 30 s |

- Cell counts are for the middle of the board; shapes are clipped at
  edges and corners (a 3x3 TNT in a corner clears 4 cells).
- Lightning's rays include its own cell **[verify]**, so a lightning on
  the top row still clears that one cell at L1.
- `color_clear` uses the colour of the piece the power-up sat on, which is
  recorded at placement (`PowerUp::kind`) because that piece is gone by
  the time the effect is computed.
- Steel pieces do not exist in `Board` yet; L2+ `color_clear` reports
  `clear_colour_steel = true` and the game layer acts on it.

`power_up_name(id)` and `power_up_from_name(name)` convert between the
enum and the contract id (case-sensitive).

## Game-layer loop

The core computes; the game layer applies. After each legal swap:

```cpp
auto r = board.try_swap(a, b);                      // std::optional<MoveResult>
std::vector<TriggeredPowerUp> queue = layer.triggered_by(*r);   // before remap
layer.remap(*r);
for (std::size_t i = 0; i < queue.size(); ++i) {
    PowerUpEffect e = power_up_effect(queue[i], board);
    // add e.time_delta_seconds to the timer, scale the move's score by
    // e.score_multiplier, show hints for e.hint_seconds, handle
    // e.clear_colour_steel
    if (!e.cells_to_clear.empty())
        if (auto er = board.clear_cells(e.cells_to_clear)) {
            auto more = layer.triggered_by(*er);   // chained triggers
            queue.insert(queue.end(), more.begin(), more.end());
            layer.remap(*er);
        }
}
layer.place_from_move(*r, board, rng, unlocked, tiers);   // once the chain is done
```

- **Chained triggers.** When a power-up's cells hold another power-up,
  `clear_cells` clears that piece and the next `triggered_by` reports it.
  The core never recurses; the game layer's queue resolves the chain.
- **Order matters.** Call `triggered_by` before `remap` for the same
  result: `remap` removes the triggered power-ups.
- **Placement last.** Placing after the chain keeps a fresh power-up from
  being consumed by the effects of the move that earned it **[verify]**.
  `place_from_move` reads only the swap's runs and the current board, so
  it works after any number of `clear_cells` calls.
- Effects are computed on the board **after** the move (and after earlier
  effects in the chain), centred on the cell where the piece was cleared.
- Whether effect clears score, count toward quotas or extend the totem
  streak is the game layer's call per the rules spec.

## API

All in namespace `montezuma`; no engine headers.

### Defs table

```cpp
struct PowerUpTierParams {
    int area_chebyshev = -1;      // area: |dx|,|dy| <= this (-1 = no area) ...
    int area_manhattan = 0;       // ... and |dx| + |dy| <= this
    unsigned rays = kRayNone;     // kRayUp | kRayDown | kRayLeft | kRayRight
    bool colour = false;          // every piece of the triggering colour
    bool colour_steel = false;    // also the colour's steel (flag only)
    bool colour_adjacent = false; // also orthogonal neighbours of colour cells
    double time_seconds = 0.0;
    int score_multiplier = 1;
    double hint_seconds = 0.0;
};
struct PowerUpDef { PowerUpId id; std::string_view name; std::array<PowerUpTierParams, 3> tiers; };
```

- `power_up_defs()` is the table, `power_up_def(id)` one row,
  `PowerUpDef::at_tier(tier)` one tier (clamped to 1..3).
- TNT's three shapes are areas: L1 `(1, 2)`, L2 `(2, 2)`, L3 `(2, 3)`.
- `validate_power_up_defs(span)` throws `std::invalid_argument` unless
  every id appears once in order with a unique non-empty name and every
  tier is well formed. The built-in table is checked at compile time.
- `validate_power_up_tiers(tiers)` throws `std::invalid_argument` for a
  tier outside 1..3.

### Layer

```cpp
struct PowerUp { PowerUpId id; int tier; int kind; };      // kind: colour of its piece
struct PowerUpPlacement { Cell cell; PowerUp power_up; };
struct TriggeredPowerUp { Cell cell; PowerUp power_up; int step; };

struct PowerUpLayer {
    int width, height;
    std::vector<std::optional<PowerUp>> cells;   // row-major, y * width + x
    ...
};
```

`PowerUpLayer` is plain data (public members, `operator==`), so it can be
saved with a level in progress.

| Member | Meaning |
|---|---|
| `PowerUpLayer(w, h)`, `PowerUpLayer(const Board&)` | empty layer; throws `std::invalid_argument` for a size below 1 |
| `at(cell)`, `has(cell)`, `set(cell, p)`, `erase(cell)`, `clear()`, `count()` | lookups and edits; out-of-bounds cells read as empty and are ignored on write |
| `entries()` | every power-up, row-major |
| `triggered_by(const MoveResult&) const` | power-ups whose pieces the result cleared, by step then `CascadeStep::cleared` order (row-major); `cell` is where the piece was when cleared |
| `remap(const MoveResult&)` | moves power-ups with their pieces; removes triggered ones; a redeal clears the layer |
| `place_from_move(result, board, rng, unlocked, tiers)` | places earned power-ups and returns them in placement order |
| `earned_by(result)` (static) | how many the result earns before capping by plain pieces |

`triggered_by` and `remap` replay the result in event order (see
[move-result.md](move-result.md)): the `swap` exchanges the two cells'
entries; for each step, entries on `cleared` cells trigger and go, then
every `falls[i].from` entry is lifted and dropped at its `to` (`spawns`
are plain pieces); then, when `shuffled && !redealt`, `after[to] =
before[from]` for each `shuffle_map` pair. Cells outside the layer are
ignored. Both are `noexcept`.

**Placement draws.** For each earned power-up, in order: pick the id with
`rng.below(number of unlocked)` over unlocked ids in `PowerUpId` order,
then the cell with `rng.below(number of plain pieces)` over plain pieces
in row-major order (occupied board cells with no power-up, minus cells
already chosen). Placement stops, without drawing, when no plain piece is
left. Nothing is drawn when nothing is earned or nothing is unlocked. The
same seed and the same moves always give the same placements.
`place_from_move` throws `std::invalid_argument` for a bad `tiers` table or
a layer whose size differs from the board's. Call it after `remap` on the
board after the move.

**Invariant.** After `remap` with the board's own results, every power-up
sits on an occupied cell whose piece has the recorded `kind`. The random
sweep in the tests checks this after every swap, effect and shuffle.

### Effects

```cpp
struct PowerUpEffect {
    PowerUpId id;
    int tier;
    std::vector<Cell> cells_to_clear;   // occupied, in bounds, row-major, unique
    double time_delta_seconds;          // 0 unless clock
    int score_multiplier;               // 1 unless multiplier
    double hint_seconds;                // 0 unless hint
    bool clear_colour_steel;            // color_clear L2+
};

PowerUpEffect power_up_effect(PowerUpId id, int tier, const Board& board, Cell at,
                              int colour = Board::kEmpty) noexcept;
PowerUpEffect power_up_effect(const TriggeredPowerUp& t, const Board& board) noexcept;
```

`tier` clamps to 1..3. `colour` is the triggering piece's kind; when
negative, the piece now at `at` is used. An out-of-bounds `at` gives no
cells but keeps the value fields. The effect never changes the board.

## Removed

The bonus inventory of the first WP-07 draft is gone: `hammer`, `shuffle`,
`dynamite` and `freeze`, stored stock and caps, upgrades by stock, and the
save text. `extra_time` became `clock`.

## Open points

- The contract ids in [contracts.md](contracts.md#totem-and-bonus-ids-owner-game-designer-wp-03--proposed)
  still list the old bonus ids; the game designer updates them with the
  spec.
- The `bonus_changed`, `bonus_earned` and `bonus_use_<id>` signal and cue
  names in the contracts predate power-ups.

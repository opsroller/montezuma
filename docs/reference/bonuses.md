<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Bonuses

Reference for the bonus system in the core rules library (WP-07): ids,
how pickups are earned, what each bonus does, upgrades and the C++ API.
For gameplay programmers wiring bonuses into the game and for the game
designer tuning them.

Code: `core/include/montezuma/bonuses.hpp`, `core/src/bonuses.cpp`.
Tests: `core/tests/test_bonuses.cpp`.

**Every number on this page is a placeholder** (`[verify]`) until WP-03
(rules spec) settles the original game's bonuses. The numbers live in one
table, `kBonusDefs` in `core/src/bonuses.cpp`; change them there and in
this page together, with a rules-spec change.

## Ids

From the contracts doc, in the stable `BonusId` order used for storage:

| Index | `BonusId` | Contract id | Targeted |
|---|---|---|---|
| 0 | `Hammer` | `hammer` | yes, one cell |
| 1 | `Lightning` | `lightning` | yes, a cell and an axis |
| 2 | `ExtraTime` | `extra_time` | no |
| 3 | `Shuffle` | `shuffle` | no |
| 4 | `Dynamite` | `dynamite` | yes, one cell |
| 5 | `Freeze` | `freeze` | no |

`bonus_name(id)` and `bonus_from_name(name)` convert between the two
(case-sensitive). New ids are appended, never reordered.

## Earning

The rules spec says a match of 4 or more grants a bonus pickup.

- For each cascade step of a legal move, each artifact kind that step
  cleared **4 or more** of (`kBonusEarnThreshold`) earns one pickup.
- The pickup is one bonus, picked by weight among the bonuses whose
  `earn_min_cleared` that count reaches and that are below their cap.
- If every eligible bonus is at its cap, the pickup is lost.
- Randomness comes only from the `Rng&` passed in, iterating steps in
  order and kinds in ascending order, so the same seed and moves always
  give the same pickups. A move with no qualifying step draws no random
  numbers.

**Known approximation.** `MoveResult` today carries only pieces cleared
per kind per step, so two separate runs of 3 of one kind in the same
step (6 pieces) also earn a pickup, and an L or T of 5 counts as 5. When
the cell-level `MoveResult` from the contracts doc lands (`CascadeStep::runs`),
earning switches to one pickup per run of 4+ and uses the run length for
`earn_min_cleared`.

## Effects and upgrades

Each bonus has upgrade levels 1 to 3 (`kMinBonusLevel`, `kMaxBonusLevel`).
All start at level 1. Shapes are clipped to the board.

| Bonus | Earned from | Weight | Effect | Level 1 | Level 2 | Level 3 |
|---|---|---|---|---|---|---|
| `hammer` | 4+ | 30 | Clears cells within Manhattan distance *n* of the target | 1 cell (n=0), max 3 | plus of 5 (n=1), max 4 | diamond of 13 (n=2), max 5 |
| `lightning` | 4+ | 25 | Clears the target's row or column | chosen line, max 2 | row and column, max 3 | 3-wide row and column, max 3 |
| `extra_time` | 4+ | 20 | Adds seconds to the level timer | +10 s, max 2 | +15 s, max 3 | +20 s, max 3 |
| `shuffle` | 4+ | 15 | Reshuffles the board | max 1 | max 2 | max 3 |
| `dynamite` | 5+ | 6 | Clears a square around the target | 3x3, max 1 | 5x5, max 2 | 7x7, max 2 |
| `freeze` | 5+ | 4 | Stops the level timer | 5 s, max 1 | 8 s, max 2 | 12 s, max 2 |

"max" is the stock cap at that level. Upgrading keeps the current stock.
What an upgrade costs and when the player may buy one is the progress
package's concern; `BonusInventory::upgrade` only raises the level.

## API

All in namespace `montezuma`; no engine headers.

### Tuning table

- `struct BonusLevelParams { max_stock, extent, both_axes, time_seconds, freeze_seconds, reshuffle }`:
  the effect at one level.
- `struct BonusDef { id, shape, earn_min_cleared, earn_weight, levels[3] }`:
  one table row. `shape` is `None`, `Diamond` (hammer), `Line`
  (lightning) or `Square` (dynamite). `targeted()`, `at_level(level)`.
- `bonus_defs()` returns the table; `bonus_def(id)` one row. A
  `static_assert` checks ids are in order, caps are at least 1 and never
  shrink with level, and no bonus is earned below the threshold.

### Effects

```cpp
struct BonusTarget { Cell cell; Axis axis = Axis::Row; };   // axis: lightning only

struct Effect {
    BonusId bonus;
    std::vector<Cell> cells_to_clear;   // row-major, unique, in bounds
    double time_delta_seconds;
    double freeze_seconds;
    bool reshuffle;
};

Effect bonus_effect(BonusId id, int level, const Board& board, BonusTarget target = {});
```

`bonus_effect` computes an effect without touching any inventory (use it
to preview a target). Out-of-range levels clamp to 1..3. Untargeted
bonuses ignore the target. The cell list does not look at cell contents;
empty or (later) blocked cells are skipped when the effect is applied.

### Inventory

`BonusInventory` is a plain struct, so the progress package can save it
between levels:

| Member | Meaning |
|---|---|
| `std::array<int, 6> stock` | pickups held, by `BonusId` index; `0 <= stock <= cap` |
| `std::array<int, 6> level` | upgrade level, 1..3 |
| `count(id)`, `level_of(id)`, `cap(id)` | lookups; `cap` is the current level's max stock |
| `add(id)` | adds one pickup; false at the cap |
| `earn_from(const MoveResult&, Rng&)` | awards pickups for a move (see Earning); returns the awarded ids in order |
| `use(id, const Board&, BonusTarget = {})` | spends one and returns `UseResult` |
| `upgrade(id)` | raises the level by one; false at level 3 |
| `to_text()`, `from_text(text)`, `try_from_text(text)` | text form for saving |

`UseResult { std::optional<Effect> effect; UseError error; }` converts to
`true` on success. Errors leave the inventory unchanged and never throw:

| `UseError` | When |
|---|---|
| `None` | success; `effect` is set |
| `NoStock` | none of that bonus held |
| `TargetOutOfBounds` | a targeted bonus aimed off the board |

Text form, one line per bonus in `BonusId` order:

```
hammer 1 1
lightning 2 2
extra_time 0 1
shuffle 0 1
dynamite 0 1
freeze 1 3
```

Fields are `<id> <stock> <level>`. `from_text` accepts lines in any order,
blank lines and CRLF; it requires every id exactly once with level 1..3
and stock 0..cap, and throws `std::invalid_argument` otherwise.
`try_from_text` is the non-throwing variant for the binding layer.

## Not done here

- **Applying effects.** `use` returns an `Effect`; it does not change the
  board or the level timer. A later integration step (game layer or a
  `Board`/`LevelState` API) clears `cells_to_clear` with collapse and
  refill, scores them, adds `time_delta_seconds`, pauses the timer for
  `freeze_seconds` and calls `Board::shuffle()` for `reshuffle`. Whether
  bonus clears score, count toward quotas or extend the totem streak is a
  WP-03 question.
- **Signals.** The game layer emits `bonus_changed(bonus_id, count)` and
  plays `bonus_earned` / `bonus_use_<id>` from `earn_from`'s return value
  and successful `use` calls.
- **Persistence and upgrade costs**: the progress package.

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Power Totems

Reference for programmers wiring Power Totems into the game layer, and for
the game designer tuning them. Rules: `docs/rules-spec.md`, "Power Totems".
Code: `core/include/montezuma/totems.hpp`, `core/src/totems.cpp`, tests in
`core/tests/test_totems.cpp`.

Every number below is a **placeholder until WP-03** (game designer) settles
the original's behaviour; each is marked `// [verify]` in the code.

## Ids

From the contract (`docs/reference/contracts.md`). The enum order is the
order totems are charged, fired and reported.

| `TotemId` | Contract id | Effect |
|---|---|---|
| `RowClear` | `row_clear` | Clears one whole row |
| `RandomBlast` | `random_blast` | Clears N random pieces |
| `Time` | `time` | Adds seconds to the level timer |
| `Lightning` | `lightning` | Clears a column, or a row and a column (cross) |
| `ColorBomb` | `color_bomb` | Clears every piece of one kind |
| `Multiplier` | `multiplier` | Multiplies score for the next moves |

`totem_name(id)` and `totem_from_name(name)` convert between the two.

## Tuning table

`kTotemDefs` in `totems.hpp`, one `TotemDef` per totem. A `TotemSystem`
can be built with another `TotemTable` (for tests or rule variants).

| Totem | `threshold` | Effect parameters | Target choice |
|---|---|---|---|
| `row_clear` | 3 [verify] | — | Row uniformly at random [verify] |
| `random_blast` | 4 [verify] | `blast_count` 8 [verify] | Distinct occupied cells at random |
| `time` | 3 [verify] | `time_delta_seconds` 10.0 [verify] | — |
| `lightning` | 5 [verify] | `lightning_shape` `Cross` [verify] | Column (and row) at random |
| `color_bomb` | 6 [verify] | — | The streak's kind [verify] |
| `multiplier` | 4 [verify] | `score_multiplier` 2, `multiplier_moves` 3 [verify] | — |

| Field | Type | Range | Used by |
|---|---|---|---|
| `threshold` | int | ≥ 1 | all: consecutive same-kind matches to fire |
| `blast_count` | int | ≥ 0, capped at pieces on the board | `random_blast` |
| `time_delta_seconds` | double | finite | `time` |
| `lightning_shape` | `Column` or `Cross` | — | `lightning` |
| `score_multiplier` | int | ≥ 1 | `multiplier` |
| `multiplier_moves` | int | ≥ 0 | `multiplier` |

The `TotemSystem` constructor throws `std::invalid_argument` for a table
out of `TotemId` order or a value out of range.

## Charging

- Input is each move's own match kind, `MoveResult::matched_kind`.
  Cascades do not charge.
- The same kind as the previous match extends the streak and adds 1 to
  every unlocked totem's charge.
- A different kind starts a new streak of 1: every unlocked totem's charge
  becomes 1.
- A negative kind (no match) clears the streak and every charge to 0.
- A totem whose charge reaches its `threshold` fires and drops to 0. It
  then recharges along the same streak if it continues.
- Locked totems never charge and never fire. `unlock` keeps the current
  streak; the totem charges from the next match.
- Several totems firing on one match are reported in `TotemId` order.

## Effects

Firing returns a `TotemEffect`; nothing touches the board.

| Field | Meaning |
|---|---|
| `totem` | Which totem fired |
| `cells_to_clear` | Unique, in bounds, occupied cells, sorted row-major (`y`, then `x`) |
| `time_delta_seconds` | Seconds to add to the timer (0 unless `time`) |
| `score_multiplier`, `multiplier_moves` | Score factor and how many following moves it lasts (1 and 0 unless `multiplier`) |

Target choices read a `const Board&` (the board after the move resolved)
and draw from a `montezuma::Rng&` the caller passes, in a fixed order: one
draw for `row_clear`, up to `blast_count` for `random_blast`, one
(`Column`) or two (`Cross`, column then row) for `lightning`, and one for
`color_bomb` only when the streak's kind is not on the board (it then
picks among the kinds present, ascending). `time` and `multiplier` draw
none. Same board, same seed: same targets.

Applying an effect is the game layer's job: it passes `cells_to_clear` to
the board's clear-and-cascade API (being added in parallel to `board.*`),
adds `time_delta_seconds` to `LevelState`'s timer and applies the score
multiplier. That integration is a later step.

## API

```cpp
namespace montezuma {
class TotemSystem {
public:
    explicit TotemSystem(const std::vector<TotemId>& unlocked = {}, const TotemTable& defs = kTotemDefs);
    static TotemSystem from_names(const std::vector<std::string_view>& unlocked,
                                  const TotemTable& defs = kTotemDefs);  // LevelData totems_unlocked

    bool unlocked(TotemId) const;
    void unlock(TotemId);
    const TotemDef& def(TotemId) const;
    int charge_count(TotemId) const;   // matches counted toward the threshold
    double charge(TotemId) const;      // 0..1, for totem_charge_changed
    int streak_kind() const;
    int streak() const;

    std::vector<TotemId> record_match(int kind);    // fired totems
    TotemEffect make_effect(TotemId, const Board&, Rng&, int target_kind) const;
    std::vector<TotemEffect> apply_move(const MoveResult&, const Board&, Rng&);
};
}
```

The game layer emits `totem_charge_changed(totem_name(id), charge(id))` for
each unlocked totem after every move.

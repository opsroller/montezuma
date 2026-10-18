<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Power Totems

Reference for programmers wiring Power Totems into the game layer, and for
the game designer tuning them. The rules are the game designer's
[Power Totems rules](rules-totems.md) (`TOT-*`, with `RES-*`, `GOAL-*`,
`FRZ-*`, `OBS-*` from the [rules spec](rules-spec.md)); the code cites
them as `// rule: TOT-nn`. Code: `core/include/montezuma/totems.hpp`,
`core/src/totems.cpp`, tests in `core/tests/test_totems.cpp`.

## Ids and colours

One totem per piece colour. A totem's `TotemId` value is the board kind
index of its colour, in the contract kind order (red, orange, yellow,
green, blue, purple, white). The mapping lives only in the `TotemId` enum
in `totems.hpp`. Kind order is the order totems are charged, fired and
reported.

| Kind | Colour | `TotemId` | Id | Effect |
|---|---|---|---|---|
| 0 | red | `Fire` | `fire` | Fireballs clear random pieces (`TOT-10`) |
| 1 | orange | `Time` | `time` | Adds seconds to the level timer (`TOT-11`) |
| 2 | yellow | `Gold` | `gold` | Awards points; tier 2+ starts Score Frenzy (`TOT-12`) |
| 3 | green | `Harvest` | `harvest` | Collects every gem; tier 2+ places new gems; tier 3+ removes chains (`TOT-13`) |
| 4 | blue | `Ward` | `ward` | Destroys steel; tier 2 stops steel spawning; tier 3 clears around it (`TOT-14`) |
| 5 | purple | `Gift` | `gift` | Places random power-ups (`TOT-15`) |
| 6 | white | `Paint` | `paint` | Repaints random pieces to one kind on the board (`TOT-16`) |

`totem_name(id)` and `totem_from_name(name)` convert between `TotemId` and
the id string. `totem_kind(id)` and `totem_for_kind(kind)` convert between
`TotemId` and the board kind; kinds 7 and up have no totem.

## Charging

- Input is each legal swap's `MoveResult`. The colours that charge are the
  kinds of the runs in step 0, `cascades[0].runs`: the swap's own match
  (`TOT-02`). A swap with runs of two kinds charges both (`TOT-04`).
  Cascades, and pieces a future power-up clears in step 0 without a run, do
  not charge.
- For each unlocked totem with uses left (`TOT-01`): if its colour is in
  this move's set, its charge grows by 1; otherwise it drops to 0
  (`TOT-02`).
- At `charges_to_fire` (2) the totem fires, its charge drops to 0 and it
  spends one use (`TOT-03`). A third move in a row charges it to 1 again.
- Only a player swap counts. A result without `swap` or without
  `cascades` (a `Board::clear_cells` effect) leaves every charge unchanged;
  illegal swaps, shuffles and hints produce no result to record
  (`TOT-02`, `TOT-07`).
- Locked totems never charge or fire. `unlock` keeps the charge (0 for a
  newly unlocked totem), so it charges from the next move.

## Tiers and uses

Each totem has tiers 1..4 (ward has 1..3). A totem can fire
`uses_per_level` times per level: 1, 1, 2, 2 by tier (`TOT-06`). Once its
uses are spent it shows empty and stops charging until `start_level()`
refills them. `uses_left(id)` and `uses_spent(id)` expose the counts.

The tier is a runtime input from player progress (WP-08), not level data.
LevelData's `totems_unlocked` only says which totems are available:
`from_names` unlocks each at tier 1, the default for tests, the sim and
the web demo; the game then calls `unlock(id, tier)` with the player's
tier. `unlock` refills the uses for that tier; a tier the totem does not
have throws `std::invalid_argument`. Which level unlocks each totem and
tier is in [progression](progression.md#unlocks) and is not represented in
the core.

## Tuning table

`kTotemDefs` in `totems.hpp`: one `TotemDef` per totem (`charges_to_fire`,
`tier_count`, `tiers[4]` of `TotemTierDef`), the Numbers table of
`rules-totems.md`. A `TotemSystem` can be built with another `TotemTable`
for tests or rule variants.

| Totem | Tier 1 | Tier 2 | Tier 3 | Tier 4 |
|---|---|---|---|---|
| uses per level (all) | 1 | 1 | 2 | 2 |
| `fire` fireballs × pieces | 2×3 | 4×3 | 2×4 + 2×5 | 4×5 |
| `time` seconds | 60 | 120 | 180 | 240 |
| `gold` points, first / second use | 1500 | 2000 | 2000 / 2500 | 2500 / 3000 |
| `gold` Score Frenzy | no | yes | yes | yes |
| `harvest` | collect gems | collect + 8 gems | collect + 9 gems, remove chains | collect + 10 gems, remove chains |
| `ward` | destroy steel | + no new steel | + clear neighbours | — |
| `gift` power-ups (board cap 8) | 1 | 2 | 3 | 4 |
| `paint` pieces repainted | 4 | 6 | 8 | 10 |

| Field | Range | Used by |
|---|---|---|
| `charges_to_fire` | ≥ 1 | all |
| `tier_count` | 1..4 | all |
| `uses_per_level` | 1..2 (`kMaxTotemUses`) | all, per tier |
| `fireballs[2]` (`count`, `size`) | ≥ 0 | `fire` |
| `time_seconds` | finite, ≥ 0 | `time` |
| `points[2]` (first, second use), `start_frenzy` | ≥ 0 | `gold` |
| `collect_all_gems`, `gems_to_place` (≥ 0), `remove_chains` | — | `harvest` |
| `destroy_steel`, `stop_steel`, `clear_around_steel` | — | `ward` |
| `powerups_to_place` | ≥ 0 | `gift` |
| `repaint_count` | ≥ 0 | `paint` |

The `TotemSystem` constructor throws `std::invalid_argument` for a table
out of kind order or a value out of range. Tiers past `tier_count` are not
checked.

## Effects

`make_effect(id, board, rng)` returns a `TotemEffect` (`totem`, `tier`,
`use`, and one group of fields per totem); it does not touch the board.
Fields a totem does not use keep their neutral values: empty cell lists,
0, `false`, `repaint_kind == -1`.

| Totem | Fields |
|---|---|
| `fire` | `cells_to_clear`: distinct, in bounds, occupied, sorted row-major (`y`, then `x`) |
| `time` | `time_delta_seconds`, added to `time_left` with no cap (`GOAL-05`) |
| `gold` | `points` for this use, `start_frenzy` (starts Frenzy, or restarts its duration during one, `FRZ-03`) |
| `harvest` | `collect_all_gems`, `gems_to_place`, `remove_chains`, applied in that order |
| `ward` | `destroy_steel`, `stop_steel`, `clear_around_steel` |
| `gift` | `powerups_to_place`, `powerup_board_cap` (8) |
| `paint` | `cells_to_repaint` (same guarantees as fire, none already `repaint_kind`), `repaint_kind` |

`use` is the use of the level just spent (`uses_spent`, at least 1); gold's
points depend on it (`TOT-12`).

Gems, steel, chains and power-ups do not exist in `Board` yet, so harvest,
ward and gift return flags and counts for the game layer, which applies
them per the rules:

- `harvest` (`TOT-13`): collect every gem (`GEM-03`; the pieces stay),
  then place `gems_to_place` gems on random plain pieces ignoring
  `gems_max`, even if none were collected, then remove every chain.
- `ward` (`TOT-14`): destroy every steel piece (a clear, so it scores and
  fills the Frenzy bar, `OBS-03`); `stop_steel` holds for the rest of the
  level, even with no steel on the board; `clear_around_steel` also clears
  every piece orthogonally next to a destroyed steel piece.
- `gift` (`TOT-15`): each power-up drawn uniformly from the unlocked
  power-ups, on a random plain piece, stopping at 8 power-ups on the
  board; with no plain piece it places nothing.

### Random choices

Effects read a `const Board&` and draw from the level's seeded
`montezuma::Rng&` in the order each rule lists (`TOT-17`). Same board,
same seed: same targets.

- `fire` (`TOT-10`): one pool of every piece (steel included once steel
  exists), row-major. Fireball groups in table order, each fireball taking
  `size` distinct cells by partial Fisher-Yates: one draw per cell, so no
  cell is hit by two fireballs. When the pool runs out the rest are
  skipped: with fewer pieces than targets it clears what there is.
- `paint` (`TOT-16`): first the target kind, one draw uniform among the
  kinds present (ascending); then up to `repaint_count` cells, one draw
  each, from the pieces of other kinds. With fewer than two kinds on the
  board it does nothing and draws nothing, but the use is still spent.
  Gems and power-ups stay on their pieces. Steel pieces are to be left out
  of the pool once steel lands in `Board`; today every piece is eligible.
- The others draw nothing.

### Applying effects: the firing loop

Totems fired on one move resolve one at a time in kind order, after the
move's last cascade (`TOT-05`, `RES-07`). Each effect is chosen on the
board as the previous one left it, and its clears resolve fully (fall,
refill, cascades) through `Board::clear_cells` before the next is chosen:

```cpp
// After the swap's MoveResult has resolved:
for (TotemId id : totems.apply_move(result)) {          // fired, kind order
    TotemEffect e = totems.make_effect(id, board, rng);  // on the board as it is now
    std::optional<MoveResult> steps = apply_board_effect(e, board);
    // game layer: time, points, Frenzy, gems, chains, steel, power-ups from `e`;
    // score, gems and Frenzy for `steps` as further steps of the same move.
}
// Then the goal check (GOAL-02) and, if no legal move, the reshuffle (RES-08).
```

`apply_move` is `record_move` under the name the loop reads with: it
returns the fired totems in kind order and builds nothing.
`apply_board_effect(effect, board)` applies what `Board` models today:
it repaints `cells_to_repaint`, then passes `cells_to_clear` plus every
run now on the board (runs the repaint made) to `Board::clear_cells`,
which clears them in one step and resolves the cascades. It returns that
`MoveResult`, or empty when nothing was cleared. The result has no `swap`,
so recording it leaves charges unchanged. A clear that wins the level
still resolves fully (`GOAL-02`).

## API

```cpp
namespace montezuma {
struct TotemUnlock { TotemId id; int tier = 1; };

class TotemSystem {
public:
    explicit TotemSystem(const std::vector<TotemUnlock>& unlocked = {}, const TotemTable& defs = kTotemDefs);
    static TotemSystem from_names(const std::vector<std::string_view>& unlocked,
                                  const TotemTable& defs = kTotemDefs);  // each at tier 1

    bool unlocked(TotemId) const;
    int tier(TotemId) const;
    void unlock(TotemId, int tier = 1);   // tier from player progress (WP-08)
    const TotemDef& def(TotemId) const;
    const TotemTierDef& tier_def(TotemId) const;
    void start_level();                   // refill uses, empty charges

    int charge_count(TotemId) const;      // 0 or 1 with the default table
    double charge(TotemId) const;         // charge_count / charges_to_fire
    int uses_left(TotemId) const;
    int uses_spent(TotemId) const;

    std::vector<TotemId> record_move(const MoveResult&);          // fired totems, kind order
    std::vector<TotemId> apply_move(const MoveResult&);           // same as record_move
    TotemEffect make_effect(TotemId, const Board&, Rng&) const;   // at the current tier and use
};

std::optional<MoveResult> apply_board_effect(const TotemEffect&, Board&);
}
```

After every move the game layer emits
`totem_charge_changed(totem_name(id), value)` for each unlocked totem:
`charge(id)` (0 or 0.5, and 0 for a spent totem), except that a totem in
this move's fired list is shown at 1 before it resets. It plays
`totem_charge` when a charge reaches 1 and `totem_fire_<id>` when a totem
fires.

## Open points

- `Board::clear_cells` numbers its steps from 0, so the game layer offsets
  them to score a totem's clears as further steps of the move
  (`SCORE-01`, `RES-07`).
- Steel, gems, chains and power-ups are not in `Board`: ward, harvest and
  gift are flags for the game layer, and paint's non-steel filter and
  fire's "steel included" apply once steel lands.
- A repaint that leaves no run and no legal move needs the game layer's
  reshuffle check (`RES-08`); `apply_board_effect` only resolves clears.

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Power Totems

Reference for programmers wiring Power Totems into the game layer, and for
the game designer tuning them. Rules come from the game designer's sources
(WP-03); the spec PR with rule ids is pending, so the code marks each rule
`// rule: <tbd>` until it can cite one. Code:
`core/include/montezuma/totems.hpp`, `core/src/totems.cpp`, tests in
`core/tests/test_totems.cpp`.

## Ids and colours

One totem per piece colour. A totem's `TotemId` value is the board kind
index of its colour; the mapping lives only in the `TotemId` enum in
`totems.hpp` and is **assumed until the spec confirms it**. Kind order is
the order totems are charged, fired and reported.

| Kind | Colour | `TotemId` | Id | Effect |
|---|---|---|---|---|
| 0 | red | `Fire` | `fire` | Fireballs clear random pieces |
| 1 | orange | `Time` | `time` | Adds seconds to the level timer |
| 2 | yellow | `Gold` | `gold` | Awards points; L2+ starts Score Frenzy |
| 3 | purple | `Gift` | `gift` | Places random power-ups |
| 4 | green | `Harvest` | `harvest` | Collects every gem; L2+ places new gems; L3+ removes chains |
| 5 | white | `Paint` | `paint` | Repaints random pieces to one colour on the board |
| 6 | blue | `Ward` | `ward` | Destroys steel; L2 stops steel spawning; L3 clears around it |

`totem_name(id)` and `totem_from_name(name)` convert between `TotemId` and
the id string. `totem_kind(id)` and `totem_for_kind(kind)` convert between
`TotemId` and the board kind; kinds 7 and up have no totem.

## Charging

- Input is each legal swap's `MoveResult`. The colours that charge are the
  keys of `steps[0]`: every kind cleared by the swap's own match.
  Cascades (`steps[1..]`) do not charge.
- For each unlocked totem with uses left: if its colour is in this move's
  set, its charge grows by 1; otherwise it drops to 0.
- At `charges_to_fire` (2) the totem fires, its charge drops to 0 and it
  spends one use.
- A result without a swap (an effect's resolution: `steps` empty or
  `matched_kind == -1`) leaves every charge unchanged.
- Several totems firing on one move resolve in kind order.
- Locked totems never charge or fire. `unlock` keeps the charge (0 for a
  newly unlocked totem), so it charges from the next move.
- The HUD charge fraction is `charge_count / charges_to_fire` (0 or 0.5),
  and 0 for a locked or spent totem.

## Tiers and uses

Each totem has tiers L1..L4 (ward has L1..L3). A totem can fire
`uses_per_level` times per level: 1, 1, 2, 2 by tier. Once its uses are
spent it stops charging and firing until `start_level()` refills them.
`uses_left(id)` exposes the count. `unlock(id, tier)` sets the tier and
refills the uses for it; a tier the totem does not have throws
`std::invalid_argument`. `from_names` (LevelData `totems_unlocked`)
unlocks at L1, since the contract carries no tier yet.

## Tuning table

`kTotemDefs` in `totems.hpp`: one `TotemDef` per totem (`charges_to_fire`,
`tier_count`, `tiers[4]` of `TotemTierDef`). A `TotemSystem` can be built
with another `TotemTable` for tests or rule variants. Numbers come from the
designer's sources; only gold's points are marked [verify].

| Totem | L1 | L2 | L3 | L4 |
|---|---|---|---|---|
| uses per level (all) | 1 | 1 | 2 | 2 |
| `fire` fireballs × pieces | 2×3 | 4×3 | 2×4 + 2×5 | 4×5 |
| `time` seconds | 60 | 120 | 180 | 240 |
| `gold` points [verify] | 1500 | 2000 | 2500 | 3000 |
| `gold` Score Frenzy | no | yes | yes | yes |
| `gift` power-ups | 1 | 2 | 3 | 4 |
| `harvest` | collect gems | collect + 8 gems | collect + 9 gems, remove chains | collect + 10 gems, remove chains |
| `paint` pieces repainted | 4 | 6 | 8 | 10 |
| `ward` | destroy steel | + stop steel | + clear around steel | — |

Gold's points are [verify]: the sources read 1500/2000/2000, then
2500/2500, then 3000, which does not split cleanly into four tiers.

| Field | Range | Used by |
|---|---|---|
| `charges_to_fire` | ≥ 1 | all |
| `tier_count` | 1..4 | all |
| `uses_per_level` | ≥ 1 | all, per tier |
| `fireballs[2]` (`count`, `size`) | ≥ 0 | `fire` |
| `time_seconds` | finite, ≥ 0 | `time` |
| `points`, `start_frenzy` | ≥ 0 | `gold` |
| `powerups_to_place` | ≥ 0 | `gift` |
| `collect_all_gems`, `gems_to_place` (≥ 0), `remove_chains` | — | `harvest` |
| `repaint_count` | ≥ 0 | `paint` |
| `destroy_steel`, `stop_steel`, `clear_around_steel` | — | `ward` |

The `TotemSystem` constructor throws `std::invalid_argument` for a table
out of kind order or a value out of range. Tiers past `tier_count` are not
checked.

## Effects

Firing returns a `TotemEffect` (`totem`, `tier`, and one group of fields
per totem); nothing touches the board. Fields a totem does not use keep
their neutral values: empty cell lists, 0, `false`, `repaint_kind == -1`.

| Totem | Fields |
|---|---|
| `fire` | `cells_to_clear`: unique, in bounds, occupied, sorted row-major (`y`, then `x`) |
| `time` | `time_delta_seconds` |
| `gold` | `points`, `start_frenzy` |
| `gift` | `powerups_to_place` |
| `harvest` | `collect_all_gems`, `gems_to_place`, `remove_chains` |
| `paint` | `cells_to_repaint` (same guarantees as above, none already `repaint_kind`), `repaint_kind` |
| `ward` | `destroy_steel`, `stop_steel`, `clear_around_steel` |

Gems, steel and chains do not exist in `Board` yet, so harvest and ward
return flags and counts for the game layer, not cells. Gift returns a
count: where the power-ups go and which ones they are is the game layer's
job, using the power-up system.

### Random choices

Effects read a `const Board&` (the board after the move resolved) and draw
from a `montezuma::Rng&` the caller passes, in a fixed order. Same board,
same seed: same targets.

- `fire`: one pool of occupied cells, row-major. Fireball groups in table
  order, each fireball taking `size` distinct cells by partial
  Fisher-Yates: one draw per cell. When the pool runs out the rest are
  skipped, so the total is capped at the occupied count. The result is the
  union, sorted.
- `paint`: first the target kind, one draw among the kinds present
  (ascending); then up to `repaint_count` cells, one draw each, from the
  occupied cells not already that kind. An empty board repaints nothing
  (`repaint_kind == -1`, no draws); a one-colour board sets `repaint_kind`
  but has no cells to repaint.
- The others draw nothing.

`apply_move` builds fired totems' effects in kind order from the same
`Rng`.

### Applying effects

Applying an effect is the game layer's job: it adds time to `LevelState`'s
timer, awards points, starts Score Frenzy, places power-ups and gems, and
handles steel and chains once those exist.

All effects returned by one `apply_move` are computed against the same
board. Apply the paint repaint first, then clear the union of every
effect's `cells_to_clear` in one clear-and-cascade step; applying them one
by one with a cascade in between would hit pieces that have since moved.
The cascade that follows is an effect result, so passing it to
`record_move` leaves charges unchanged.

## API

```cpp
namespace montezuma {
struct TotemUnlock { TotemId id; int tier = 1; };

class TotemSystem {
public:
    explicit TotemSystem(const std::vector<TotemUnlock>& unlocked = {}, const TotemTable& defs = kTotemDefs);
    static TotemSystem from_names(const std::vector<std::string_view>& unlocked,
                                  const TotemTable& defs = kTotemDefs);  // each at L1

    bool unlocked(TotemId) const;
    int tier(TotemId) const;
    void unlock(TotemId, int tier = 1);
    const TotemDef& def(TotemId) const;
    const TotemTierDef& tier_def(TotemId) const;
    void start_level();                 // refill uses, empty charges

    int charge_count(TotemId) const;    // 0 or 1 with the default table
    double charge(TotemId) const;       // charge_count / charges_to_fire, for totem_charge_changed
    int uses_left(TotemId) const;

    std::vector<TotemId> record_move(const MoveResult&);          // fired totems, kind order
    TotemEffect make_effect(TotemId, const Board&, Rng&) const;   // at the current tier
    std::vector<TotemEffect> apply_move(const MoveResult&, const Board&, Rng&);
};
}
```

The game layer emits `totem_charge_changed(totem_name(id), charge(id))` for
each unlocked totem after every move.

## Open points

- Colour-to-kind mapping, and every `// rule: <tbd>`, wait on the spec PR.
- Gold's points per tier ([verify] above).
- LevelData's `totems_unlocked` has no tier field, so `from_names` uses L1.
- `docs/reference/contracts.md` still lists the earlier totem ids.

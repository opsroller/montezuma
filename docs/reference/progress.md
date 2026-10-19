<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Progress, save and trophies

Reference for programmers using the player profile: the profile for the
`classic` campaign (WP-08), in the core
library: `montezuma/progress.hpp` and `montezuma/trophies.hpp`, namespace
`montezuma`. The rules it applies are in [progression](progression.md)
and [bonus games](rules-bonus-games.md#common-rules-bg-); why it works
this way is in
[about progress and trophies](../explanation/about-progress-and-trophies.md).
Tests: `core/tests/test_progress.cpp`.

Nothing engine-side calls it yet: the episode map's unlock stub, the
trophies screen and `Match3Game`'s upgrades are wired in a follow-up,
[#104](https://github.com/opsroller/montezuma/issues/104).

## Campaign and bonus levels

| Function | Returns |
|---|---|
| `campaign_level_ids()` | the 41 ids `e1-l01` .. `e5-l12` in play order (`PROG-01`) |
| `campaign_index(id)` | 0-based play order, empty for any other id |
| `bonus_levels()` | the 8 bonus levels, in play order, each with `id`, `game`, `tier` and the level it follows (`after`) |
| `bonus_level(id)` | one of them, or empty |

Bonus level ids are `<game>-<tier>`:

| Id | After |
|---|---|
| `bg_accuracy-1` | e1-l05 |
| `bg_memory-1` | e2-l06 |
| `bg_reaction-1` | e3-l04 |
| `bg_attention-1` | e3-l08 |
| `bg_accuracy-2` | e4-l05 |
| `bg_memory-2` | e4-l10 |
| `bg_reaction-2` | e5-l04 |
| `bg_attention-2` | e5-l08 |

## `Progress`

A value type: copy it, compare it with `==`.

| Member | Does |
|---|---|
| `is_open(id, difficulty)` | `e1-l01` on normal is always open, on hard once e5-l12 is won on normal (`DIFF-02`); every other level opens when the one before it is won on the same difficulty |
| `is_won(id, difficulty)`, `best_score(id, difficulty)` | per level and difficulty; the best score is the highest of any play, won or lost |
| `record_level(id, difficulty, play)` | records a finished `LevelPlay` and returns a `Reward` |
| `bonus_open(id)` | the level it follows is won on either difficulty |
| `bonus_result(id)` | `NotPlayed`, `Passed` or `Failed` |
| `record_bonus(id, passed)` | records a bonus play and returns a `Reward` |
| `stars()`, `star_sand()` | golden stars to spend, at most `kMaxStars` (gains past it are dropped, never wrapped); star sand, always below 50 |
| `add_star_sand(n)` | adds sand, turns every 50 into a star at once, returns the stars made (`PROG-06`) |
| `tier(item)` | the tier owned, free tiers included |
| `can_buy(item)`, `buy(item)` | the next tier for 1 star (`PROG-04`); `buy` returns false and changes nothing when it can't |
| `upgrades_for(id)` | the `Upgrades` to start a `LevelSession` with |
| `has_trophy(id)`, `totem_fired(id)` | trophies held; totems that have fired in any play |
| `to_json()`, `try_from_json(text)` | the save file, below |

`record_level` and `record_bonus` throw `std::invalid_argument` for an
unknown or closed level; `record_level` also for a play still `Playing`,
a negative score, negative sand, or a `time_left` that is negative or NaN. `add_star_sand` throws for a negative
amount; `upgrades_for` for an unknown level.

### What a play gives

| Event | Reward |
|---|---|
| First win of a level on a difficulty | +1 star (`PROG-02`) |
| Any later win, any loss | nothing (`PROG-07`) |
| A play's star sand (won or lost) | added; every 50 is +1 star |
| First play of a bonus level, passed | +1 star (`BG-01`) |
| First play failed, or any replay | no star (`BG-02`); a later pass marks it `Passed` |

`Reward` holds `stars` (added by this call) and `trophies` (newly earned,
in `TrophyId` order).

### Upgrades

`UpgradeItem::timer()`, `gems()`, `frenzy()`, `totem(TotemId)` and
`power_up(PowerUpId)` name an item. `max_tier(item)` and
`unlock_level(item, tier)` give the
[Upgrades](progression.md#upgrades) and [Unlocks](progression.md#unlocks)
tables.

- Tier 1 of a totem, a power-up and Score Frenzy is owned for free once
  its level is open on either difficulty (`PROG-03`). Timer and gems tiers
  all cost a star.
- A tier can be bought once its level is open on either difficulty and
  every tier below it is owned.
- Tiers are kept for the whole profile, on both difficulties (`PROG-05`).
- `upgrades_for(id)` gives the owned tiers, except `frenzy_tier`, which is
  0 on levels before e1-l04 (`FRZ-01`). `LevelSession` uses a totem's or a
  power-up's tier only when the level lists it.

## Trophies

`TrophyId` in this order (the stable index), with contract ids from
`trophy_name` and `trophy_from_name`:

| Id | Earned | Decided by |
|---|---|---|
| `first_find` | e1-l01 won | `Progress` |
| `chain_reaction` | a swap's cascade reaches step 5 (`kChainReactionStep`) | `TrophyWatch` |
| `demolition` | one move clears 40 pieces (`kDemolitionPieces`), its totems' clears included | `TrophyWatch` |
| `totem_keeper` | all seven totems have fired, over any number of plays | `Progress` |
| `quick_hands` | a level won with at least 150 s left (`kQuickHandsSeconds`) | `Progress`, from the play |
| `frenzy` | 12 gems (`kFrenzyTrophyGems`) in one Score Frenzy | `TrophyWatch` |
| `sharp_eye` | all eight bonus levels passed | `Progress` |
| `explorer` | e5-l12 won on normal | `Progress` |
| `legend` | e5-l12 won on hard | `Progress` |

A trophy is earned once. `record_level` takes only `chain_reaction`,
`demolition` and `frenzy` from `LevelPlay::earned`; it decides the others
itself.

### `TrophyWatch`

Follows one `LevelSession`. Pass it every `MoveReport` (`on_move`) and
`TickReport` (`on_tick`), then `play(session)` gives the `LevelPlay` to
record:

| `LevelPlay` field | From |
|---|---|
| `outcome`, `score`, `time_left`, `star_sand` | the session |
| `totems_fired` | every `MoveReport::totems_fired` seen |
| `earned` | the in-level trophies, in `TrophyId` order |

Frenzy gems are the star sand each move gains (`FRZ-04` gives one per gem
in Frenzy). The count restarts at a `frenzy_started` move and at a
`frenzy_ended` tick; a gold-totem restart (`frenzy_restarted`) keeps it.

## Save file

`to_json()` writes compact JSON with sorted keys, so the same progress
always gives the same text. `try_from_json(text)` never throws: it returns
`ProgressLoadResult` with `progress` set, or `progress` empty and `error`
naming the field. Empty or blank text loads a fresh profile.

```json
{
  "version": 1,
  "stars": 3,
  "star_sand": 37,
  "levels": {
    "normal": {"e1-l01": {"won": true, "best_score": 4242}},
    "hard": {}
  },
  "bonus": {"bg_accuracy-1": "passed", "bg_memory-1": "failed"},
  "upgrades": {"timer": 1, "gems": 0, "frenzy": 0,
               "totems": {"fire": 2}, "power_ups": {"lightning": 2}},
  "totems_fired": ["fire", "harvest"],
  "trophies": ["first_find", "quick_hands"]
}
```

| Field | Rule on load |
|---|---|
| `version` | required, `1` (`kSaveVersion`) |
| `stars` | integer, 0..`kMaxStars`; default 0 |
| `star_sand` | integer 0..49; default 0 |
| `levels.<normal\|hard>.<id>` | campaign ids only; `won` a boolean, `best_score` an integer at least 0 |
| `bonus.<id>` | bonus level ids only; `passed` or `failed` |
| `upgrades` | tiers bought, 0..the item's last tier; totems and power-ups by contract id, absent means 0. Free tiers are not stored |
| `totems_fired`, `trophies` | arrays of contract ids |

Any other field, an unknown id or a value out of range rejects the whole
file. Open levels and free tiers are not stored; they follow from the wins.
The loader checks each value on its own, not whether values agree: a
hand-edited save can hold a bought tier or a win the player never reached,
and it loads as written.

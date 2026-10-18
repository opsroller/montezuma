<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Level data

Reference for the level file format and its parser, for game designers
writing level files and programmers loading them. The interface is the
LevelData contract in [contracts.md](contracts.md). The schema follows the
game designer's sourced schema (WP-03); rule ids from the rules spec will
be cited here once that spec lands.

## Files

- Location: `game/data/levels/e<episode>/l<NN>.json`.
- Encoding: one JSON object, UTF-8. Comments are not allowed.
- The engine reads the file and passes its text to
  `montezuma::LevelData::parse`.

## Example

```json
{
  "id": "e2-l07",
  "episode": 2,
  "index": 7,
  "board": { "width": 9, "height": 7, "kinds": 6 },
  "gem_goal": { "normal": 40, "hard": 60 },
  "totems_unlocked": ["fire", "time"],
  "powerups_unlocked": ["tnt", "hint"],
  "steel_rate": 0.25,
  "blocked_cells": [[0, 0], [8, 6]],
  "chained_cells": [[1, 1], [2, 3]],
  "seed": 12345
}
```

This level has no `base_clock`, so it starts from the default 270 s plus
the player's timer tier, and uses the default `rules` (`classic`) and
`theme` (`temple`). A rule-variant level adds `quotas`, and a level that
needs a longer clock adds `base_clock`:

```json
"quotas": { "red": 10, "blue": 8, "purple": 3 },
"base_clock": { "normal": 310, "hard": 490 }
```

## Fields

| Field | Type | Required | Default | Valid values |
|---|---|---|---|---|
| `id` | string | yes | | `e<N>-l<NN>`: `N` is the episode with no leading zero, `NN` is the index zero-padded to at least two digits. Must equal the id built from `episode` and `index` (episode 2, index 7 is `e2-l07`) |
| `episode` | integer | yes | | 1 or more |
| `index` | integer | yes | | 1 or more |
| `board` | object | no | `{"width": 8, "height": 8, "kinds": 7}` | Keys `width`, `height`, `kinds` only; each key is optional and defaults on its own |
| `board.width` | integer | no | 8 | 3 to 64 |
| `board.height` | integer | no | 8 | 3 to 64 |
| `board.kinds` | integer | no | 7 | 3 to 7. The board uses kinds 0 to `kinds - 1` |
| `gem_goal` | object | see [Goals](#goals) | absent | Keys `normal` and `hard` only, both required (`GOAL-01`) |
| `gem_goal.normal` | integer | yes | | Gems to collect on Normal, 1 or more |
| `gem_goal.hard` | integer | yes | | Gems to collect on Hard, 1 or more |
| `quotas` | object | see [Goals](#goals) | absent | Rule-variant goal. Kind name to pieces to clear. At least one entry. Each count is an integer of 1 or more. Each kind's index is below `board.kinds`. With `gem_goal`, both goals must be met (`GOAL-06`) |
| `base_clock` | object | no | absent: 270 s on both difficulties | Keys `normal` and `hard` only, both required. The [timer tier](#timer-tiers)'s bonus is added on top (`GOAL-03`). Set from the [clock rule](balance.md#clock) |
| `base_clock.normal` | number | yes | | Seconds, greater than 0; integer or fraction |
| `base_clock.hard` | number | yes | | Seconds, greater than 0; integer or fraction |
| `totems_unlocked` | array of strings | yes | | Totem ids below, each at most once. May be empty |
| `powerups_unlocked` | array of strings | no | `[]` | Power-up ids below, each at most once |
| `steel_rate` | number | no | 0 | Chance per spawned piece to be steel, 0 to 1 inclusive (`OBS-02`) |
| `blocked_cells` | array of `[x, y]` | no | `[]` | Integer pairs; `0 <= x < width`, `0 <= y < height`; each cell at most once. `x` runs left to right, `y` top to bottom |
| `chained_cells` | array of `[x, y]` | no | `[]` | Same rules as `blocked_cells`. A cell cannot be both chained and blocked (`OBS-05`) |
| `rules` | string | no | `"classic"` | Rule-set id: one or more of `a-z`, `0-9`, `_` |
| `theme` | string | no | `"temple"` | Theme id: one or more of `a-z`, `0-9`, `_` |
| `seed` | integer | yes | | 0 to 18446744073709551615 |

Any other top-level, `board`, `gem_goal` or `base_clock` key is an error.
Integer fields reject numbers written with a fraction (`8.0`), strings
(`"8"`) and booleans. A key repeated in the same object is an error.

## Goals

The original game's level goal is collecting gems: `gem_goal`. `quotas`
(pieces of given kinds to clear) is kept as a rule-variant goal. A level
has `gem_goal`, `quotas`, or both; a level with neither is an error.

## Difficulties

`normal` and `hard`. The original has Adventure (our `normal`) and Hard,
and no relaxed mode.

## Timer tiers

A level starts with its base clock plus the player's timer tier bonus,
which upgrades raise. The bonus applies on every level, so a bought timer
upgrade always keeps its value:

| Tier | Bonus | Level without `base_clock` |
|---|---|---|
| 0 (default) | +0 s | 270 s |
| 1 | +30 s | 300 s |
| 2 | +60 s | 330 s |
| 3 | +90 s | 360 s |
| 4 | +120 s | 390 s |

For example, a level with `"base_clock": { "normal": 310, "hard": 490 }`
starts at 310 s on normal with no upgrades and at 400 s with tier 3.

`time_limit`, which used to replace the tier's time, is no longer
accepted: a file that still has it fails with an error naming
`base_clock`.

## Kind names

| Name | Index |
|---|---|
| `red` | 0 |
| `orange` | 1 |
| `yellow` | 2 |
| `green` | 3 |
| `blue` | 4 |
| `purple` | 5 |
| `white` | 6 |

Names are lower case. In C++: `montezuma::kKindNames`,
`montezuma::kind_index(name)`.

## Totem ids

`fire`, `time`, `gold`, `gift`, `harvest`, `paint`, `ward`.

## Power-up ids

`tnt`, `color_clear`, `lightning`, `clock`, `multiplier`, `hint`.

The parser keeps both id lists privately in `core/src/level_data.cpp`
until the totem (PR #6) and power-up (PR #8) code lands; it then validates
with `totem_from_name()` and the power-up equivalent, and these sections
will point at those pages.

## C++ API

Header: `core/include/montezuma/level_data.hpp`, namespace `montezuma`.

| Member | Returns | Throws |
|---|---|---|
| `LevelData::parse(std::string_view json)` | `LevelParseResult`: `level` set when valid, otherwise `errors` holds every problem found | Never (`noexcept`) |
| `LevelData::load(std::string_view json)` | `LevelData` | `std::invalid_argument` whose message is `Invalid level data:` followed by one error per line |
| `LevelData::goal()` | `LevelGoal`: `gems` and `quotas`, true for each goal kind the level has | Never |
| `LevelData::base_clock_for(Difficulty)` | The difficulty's base clock in seconds: `base_clock` when present, otherwise 270 | Never |
| `LevelData::time_limit_for(Difficulty, int timer_tier = 0)` | Seconds the level starts with: `base_clock_for()` plus 30 s per timer tier | `std::invalid_argument` when `timer_tier` is outside 0 to 4 |
| `LevelData::make_state(Difficulty, int timer_tier = 0)` | `LevelState` with the level's quotas and `time_limit_for()`'s time | `std::invalid_argument` for a bad `timer_tier`, or `gem goals need the gem rules (coming)` for a level with `gem_goal` and no `quotas`, until `LevelState` tracks gems |
| `difficulty_from_name(std::string_view)` | `Difficulty` for `"normal"` or `"hard"`, otherwise empty | Never |
| `LevelParseResult::error_text()` | All errors, one `path: message` per line | |

`LevelData` fields: `id`, `episode`, `index`, `board` (`BoardSpec`:
`width`, `height`, `kinds`), `gem_goal` (`std::optional<GemGoal>`:
`normal`, `hard`), `quotas` (`std::map<int, int>`, kind index to count;
empty when absent), `base_clock` (`std::optional<BaseClock>`: `normal`,
`hard`, seconds), `totems_unlocked` and `powerups_unlocked` (in file
order), `steel_rate` (`double`), `blocked_cells` and `chained_cells`
(`std::vector<Cell>`, in file order), `rules` and `theme`
(`std::string`), `seed` (`std::uint64_t`).

Constants: `kMinTimerTier` (0), `kMaxTimerTier` (4), `kBaseTimerSeconds`
(270), `kTimerTierStepSeconds` (30), `kDefaultRules` (`"classic"`),
`kDefaultTheme` (`"temple"`).

## Errors

Each `LevelDataError` has a `path` and a `message`; `to_string()` gives
`path: message`. The path names the field with `.` for object keys and
`[i]` for array elements, from 0: `quotas.pink`, `board.kinds`,
`blocked_cells[2][0]`, `totems_unlocked[1]`. Problems with the whole
document have an empty path. Every independent problem is reported, in a
fixed order for the same input. Quota kind and cell bounds are not checked
against an invalid `board`.

| Path | Message | Cause |
|---|---|---|
| (empty) | `invalid JSON: parse error at line L, column C: ...` | Text is not valid JSON, has comments, or has text after the object. Reported alone |
| (empty) | `could not read level data: <reason>` | The JSON library failed in another way, for example a number too large for a double (`1e400`). Reported alone |
| (empty) | `expected an object at the top level, got <type>` | Document is not an object |
| (empty) | `needs a goal: gem_goal, quotas or both` | Neither `gem_goal` nor `quotas` is present |
| (empty) | `duplicate key "<key>"` | A key appears twice in one object |
| `<field>` | `unknown field` | Key not in the format, at top level, in `board`, `gem_goal` or `base_clock` |
| `time_limit` | `replaced by base_clock, which the timer tier adds to (GOAL-03)` | The old clock override is present; move its values to `base_clock` |
| `<field>` | `required field missing` | A required field is absent |
| `<field>` | `expected an integer, got <value>` | Integer field holds a fraction, string, boolean, null, array or object |
| `<field>` | `must be at least <min>, got <value>` | `episode`, `index`, a `gem_goal` count or a quota count below 1 |
| `<field>` | `too large, got <value>` | `episode`, `index`, a `gem_goal` count or a quota count above 2147483647 |
| `<field>` | `must be between <min> and <max>, got <value>` | `board.width`, `board.height` or `board.kinds` out of range |
| `id` | `expected a string, got <type>` | `id` is not a string |
| `id` | `expected the form e<N>-l<NN> (e.g. e1-l03), got "<id>"` | Bad id format |
| `id` | `"<id>" does not match episode <E> and index <I> (expected "<id>")` | Id disagrees with `episode` and `index` |
| `board` | `expected an object, got <type>` | `board` is not an object |
| `gem_goal` | `expected an object with normal and hard, got <type>` | `gem_goal` is not an object |
| `quotas` | `expected an object of kind name to count, got <type>` | `quotas` is not an object |
| `quotas` | `must list at least one kind` | `quotas` is `{}` |
| `quotas.<name>` | `unknown kind (expected one of red, orange, ...)` | Name not in the kind list |
| `quotas.<name>` | `kind <i> is not on a board with <n> kinds` | Kind index is `board.kinds` or more |
| `base_clock` | `expected an object with normal and hard, got <type>` | `base_clock` is not an object |
| `base_clock.<mode>` | `expected a number of seconds, got <type>` | Not a number |
| `base_clock.<mode>` | `must be greater than 0 seconds, got <value>` | 0 or negative |
| `totems_unlocked` | `expected an array of totem ids, got <type>` | Not an array |
| `totems_unlocked[i]` | `expected a totem id string, got <value>` | Element is not a string |
| `totems_unlocked[i]` | `unknown totem "<id>" (expected one of ...)` | Id not in the totem list |
| `totems_unlocked[i]` | `duplicate totem "<id>"` | Id listed earlier |
| `powerups_unlocked` | `expected an array of power-up ids, got <type>` | Not an array |
| `powerups_unlocked[i]` | `expected a power-up id string, got <value>` | Element is not a string |
| `powerups_unlocked[i]` | `unknown power-up "<id>" (expected one of ...)` | Id not in the power-up list |
| `powerups_unlocked[i]` | `duplicate power-up "<id>"` | Id listed earlier |
| `steel_rate` | `expected a number from 0 to 1, got <type>` | Not a number |
| `steel_rate` | `must be between 0 and 1, got <value>` | Below 0 or above 1 |
| `blocked_cells`, `chained_cells` | `expected an array of [x, y] pairs, got <type>` | Not an array |
| `<cells>[i]` | `expected [x, y], got <value>` | Element is not a two-element array |
| `<cells>[i][0]`, `[i][1]` | `x <v> is outside the board (0 to <max>)` (or `y`) | Coordinate outside the board |
| `<cells>[i]` | `duplicate of <cells>[j]` | Same cell listed earlier in the same list |
| `chained_cells[i]` | `cell is also blocked (blocked_cells[j])` | Cell is in both lists |
| `rules`, `theme` | `expected a string, got <value>` | Not a string |
| `rules`, `theme` | `expected a non-empty id of a-z, 0-9 and _, got "<id>"` | Empty, or has another character |
| `seed` | `must not be negative, got <value>` | Negative seed |
| `seed` | `expected an integer, got <value>` | Not an integer |

## JSON library

Parsing uses nlohmann/json 3.11.3 (MIT), vendored at
`thirdparty/nlohmann/json.hpp` with its license in
`thirdparty/nlohmann/LICENSE.MIT`. It is a private include of
`montezuma_core`; the public header does not expose it.

### Third-party notice

nlohmann/json 3.11.3 is MIT-licensed, Copyright (c) 2013-2023 Niels
Lohmann. Its license text is in `thirdparty/nlohmann/LICENSE.MIT` and must
ship with every copy, binaries included: `tools/release/notices.py` puts
it in `THIRD-PARTY-NOTICES.txt`, and `CREDITS.md` lists it under
Third-party works.

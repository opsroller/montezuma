<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Level data

Reference for the level file format and its parser, for game designers
writing level files and programmers loading them. The interface is the
LevelData contract in [contracts.md](contracts.md).

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
  "quotas": { "red": 10, "blue": 8, "purple": 3 },
  "time_limit": { "relaxed": 240, "normal": 150.5 },
  "totems_unlocked": ["row_clear", "time"],
  "blocked_cells": [[0, 0], [8, 6]],
  "seed": 12345
}
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
| `quotas` | object | yes | | Kind name to pieces to clear. At least one entry. Each count is an integer of 1 or more. Each kind's index is below `board.kinds` |
| `time_limit` | object | yes | | Keys `relaxed` and `normal` only, both required |
| `time_limit.relaxed` | number | yes | | Seconds, greater than 0; integer or fraction |
| `time_limit.normal` | number | yes | | Seconds, greater than 0; integer or fraction |
| `totems_unlocked` | array of strings | yes | | Totem ids below, each at most once. May be empty |
| `blocked_cells` | array of `[x, y]` | no | `[]` | Integer pairs; `0 <= x < width`, `0 <= y < height`; each cell at most once. `x` runs left to right, `y` top to bottom |
| `seed` | integer | yes | | 0 to 18446744073709551615 |

Any other top-level, `board` or `time_limit` key is an error. Integer
fields reject numbers written with a fraction (`8.0`), strings (`"8"`) and
booleans. A key repeated in the same object is an error.

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

`row_clear`, `random_blast`, `time`, `lightning`, `color_bomb`,
`multiplier`. In C++: `montezuma::kTotemIds`.

## C++ API

Header: `core/include/montezuma/level_data.hpp`, namespace `montezuma`.

| Member | Returns | Throws |
|---|---|---|
| `LevelData::parse(std::string_view json)` | `LevelParseResult`: `level` set when valid, otherwise `errors` holds every problem found | Never (`noexcept`) |
| `LevelData::load(std::string_view json)` | `LevelData` | `std::invalid_argument` whose message is `Invalid level data:` followed by one error per line |
| `LevelData::time_limit_for(Difficulty)` | Seconds for `Difficulty::Relaxed` or `Difficulty::Normal` | Never |
| `LevelData::make_state(Difficulty)` | `LevelState` with the level's quotas and the difficulty's time limit | Never for a parsed level |
| `difficulty_from_name(std::string_view)` | `Difficulty` for `"relaxed"` or `"normal"`, otherwise empty | Never |
| `LevelParseResult::error_text()` | All errors, one `path: message` per line | |

`LevelData` fields: `id`, `episode`, `index`, `board` (`BoardSpec`:
`width`, `height`, `kinds`), `quotas` (`std::map<int, int>`, kind index
to count), `time_limit` (`TimeLimit`: `relaxed`, `normal`, seconds),
`totems_unlocked` (in file order), `blocked_cells` (`std::vector<Cell>`,
in file order), `seed` (`std::uint64_t`).

## Errors

Each `LevelDataError` has a `path` and a `message`; `to_string()` gives
`path: message`. The path names the field with `.` for object keys and
`[i]` for array elements, from 0: `quotas.pink`, `board.kinds`,
`blocked_cells[2][0]`, `totems_unlocked[1]`. Problems with the whole
document have an empty path. Every independent problem is reported, in a
fixed order for the same input. Quota kind and blocked-cell bounds are not
checked against an invalid `board`.

| Path | Message | Cause |
|---|---|---|
| (empty) | `invalid JSON: parse error at line L, column C: ...` | Text is not valid JSON, has comments, or has text after the object. Reported alone |
| (empty) | `expected an object at the top level, got <type>` | Document is not an object |
| (empty) | `duplicate key "<key>"` | A key appears twice in one object |
| `<field>` | `unknown field` | Key not in the format, at top level, in `board` or in `time_limit` |
| `<field>` | `required field missing` | A required field is absent |
| `<field>` | `expected an integer, got <value>` | Integer field holds a fraction, string, boolean, null, array or object |
| `<field>` | `must be at least <min>, got <value>` | `episode`, `index` or a quota count below 1 |
| `<field>` | `too large, got <value>` | `episode`, `index` or a quota count above 2147483647 |
| `<field>` | `must be between <min> and <max>, got <value>` | `board.width`, `board.height` or `board.kinds` out of range |
| `id` | `expected a string, got <type>` | `id` is not a string |
| `id` | `expected the form e<N>-l<NN> (e.g. e1-l03), got "<id>"` | Bad id format |
| `id` | `"<id>" does not match episode <E> and index <I> (expected "<id>")` | Id disagrees with `episode` and `index` |
| `board` | `expected an object, got <type>` | `board` is not an object |
| `quotas` | `expected an object of kind name to count, got <type>` | `quotas` is not an object |
| `quotas` | `must list at least one kind` | `quotas` is `{}` |
| `quotas.<name>` | `unknown kind (expected one of red, orange, ...)` | Name not in the kind list |
| `quotas.<name>` | `kind <i> is not on a board with <n> kinds` | Kind index is `board.kinds` or more |
| `time_limit` | `expected an object with relaxed and normal, got <type>` | `time_limit` is not an object |
| `time_limit.<mode>` | `expected a number of seconds, got <type>` | Not a number |
| `time_limit.<mode>` | `must be greater than 0 seconds, got <value>` | 0 or negative |
| `totems_unlocked` | `expected an array of totem ids, got <type>` | Not an array |
| `totems_unlocked[i]` | `expected a totem id string, got <value>` | Element is not a string |
| `totems_unlocked[i]` | `unknown totem "<id>" (expected one of ...)` | Id not in the totem list |
| `totems_unlocked[i]` | `duplicate totem "<id>"` | Id listed earlier |
| `blocked_cells` | `expected an array of [x, y] pairs, got <type>` | Not an array |
| `blocked_cells[i]` | `expected [x, y], got <value>` | Element is not a two-element array |
| `blocked_cells[i][0]`, `[i][1]` | `x <v> is outside the board (0 to <max>)` (or `y`) | Coordinate outside the board |
| `blocked_cells[i]` | `duplicate of blocked_cells[j]` | Same cell listed earlier |
| `seed` | `must not be negative, got <value>` | Negative seed |
| `seed` | `expected an integer, got <value>` | Not an integer |

## JSON library

Parsing uses nlohmann/json 3.11.3 (MIT), vendored at
`thirdparty/nlohmann/json.hpp` with its license in
`thirdparty/nlohmann/LICENSE.MIT`. It is a private include of
`montezuma_core`; the public header does not expose it.

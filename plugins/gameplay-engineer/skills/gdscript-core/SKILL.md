---
name: gdscript-core
description: Conventions for game logic in core/ of Montezuma 3D. Use when adding or changing a class in core/.
---

# core/ conventions

- Classes extend `RefCounted`, declare `class_name`, and use static typing
  everywhere (`var x: int`, `-> void`).
- No `Node`, `SceneTree`, `Input`, `Time` or rendering calls. Time comes in
  as `delta` arguments; randomness comes from a seeded
  `RandomNumberGenerator` passed in or owned by `Board`.
- Results are data. A player action returns a result object
  (`MoveResult`, `TotemEffect`, `BonusResult`) listing what changed, in the
  order it should be shown, at cell level. The board view animates from that
  list alone.
- Tunable numbers (thresholds, scores, durations) live in one place: a
  `const` block at the top of the class or a field in level data, named for
  what they mean.
- Tabs for indentation, `##` doc comment on every public function and class,
  snake_case functions, PascalCase classes, matching the existing files.
- Keep public APIs listed in `docs/contracts.md`. Changing one is a
  `feat(core)!:` commit and a message to every dependant.

## Existing API (as of the first core PR)

- `Board.new(w, h, kinds, seed)`, `try_swap(a, b) -> MoveResult` (null if
  illegal), `is_legal_swap`, `has_legal_move`, `find_matches`, `shuffle`.
- `MoveResult.steps` (kind -> count per cascade step), `matched_kind`,
  `shuffled`, `score()`, `cleared_by_kind()`.
- `LevelState.new(quotas, time_limit)`, `apply_move(result)`,
  `tick(delta)`, `outcome`, `streak`, `streak_kind`.

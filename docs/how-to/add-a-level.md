<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->

# Add a level

Write one level file for the `classic` campaign from its row in the
progression tables. The intro level,
[`game/data/levels/e1/l01.json`](../../game/data/levels/e1/l01.json), is
the worked example.

## Before you start

Find the level's row in [Progression](../reference/progression.md):
its gem goals in [Episodes](../reference/progression.md#episodes) and the
totems and power-ups whose tier 1 has arrived by that level in
[Unlocks](../reference/progression.md#unlocks). Field rules and error
messages are in [Level data](../reference/level-data.md).

## Write the file

1. Create `game/data/levels/e<N>/l<NN>.json`: episode `N` with no leading
   zero, index `NN` padded to two digits (`e2/l07.json`).
2. Start from the intro level and change, in this order:
   - `id`, `episode` and `index`. The id must read `e<N>-l<NN>` and agree
     with the other two.
   - `gem_goal`: the row's normal and hard counts.
   - `totems_unlocked` and `powerups_unlocked`: every id unlocked at or
     before this level, in the order the Unlocks table lists them.
   - `seed`: a new fixed integer, different from every other level's, so
     the simulator and tests repeat.
3. Add obstacles only where the row or the balance work calls for them:
   `steel_rate`, `chained_cells`, `blocked_cells`.
4. Leave out `time_limit` unless the level has its own clock; without it
   the player's timer tier decides.
5. Keep `rules` and `theme` as `classic` and `temple` for the campaign.

## Check it

- The file is a single JSON object with no comments and no extra keys.
- Every number matches the progression tables; any difference is a
  change to [Progression](../reference/progression.md) in the same PR.
- `montezuma::LevelData::parse` accepts it. A rejected file reports each
  problem as `path: message`; look the message up in
  [Level data errors](../reference/level-data.md#errors).
- The core tests pass. `test_level_files` reads every file under
  `game/data/levels/`, so it checks the new one with no change: it parses,
  its id matches its path, and it deals a playable board on normal and
  hard. Run it with the core build in [Build and test](build-and-test.md):

  ```sh
  ctest --test-dir build -R test_level_files --output-on-failure
  ```

## Play it

Start the game, pick the level on the episode map, or start it directly
from a scene by setting `autostart_level` on the `Game` node
([Match3Game](../reference/match3-game.md#properties)) to its id.

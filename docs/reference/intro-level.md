<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# IntroLevel (web demo scene)

`IntroLevel` is a `Node3D` class from the montezuma GDExtension
(`extension/src/demo/`). It is the root of `res://scenes/demo/intro_level.tscn`,
the web build's main scene (`application/run/main_scene.web` in
`game/project.godot`). It plays one level file on a `BoardView3D` and
announces the win to the page. The page side is the
[web demo hand-off contract](contracts.md#web-demo-hand-off-owner-gameplay-programmer-wp-21wp-34-consumer-frontend-web-developer-wp-35--agreed);
how the build is made is in [Build pipeline](build-pipeline.md#web-demo).

## Properties

| Property | Default | Description |
|---|---|---|
| `board_view_path: NodePath` | `Board/View` in the scene | The `BoardView3D` to play on |
| `level_path: String` | `res://data/levels/e1/l01.json` | The LevelData file to play |

## Methods

| Method | Description |
|---|---|
| `get_level_id() -> String` | The level file's `id`, e.g. `e1-l01` |
| `get_score() -> int` | Score so far |
| `is_completed() -> bool` | Whether the level was won |
| `complete()` | Wins the level now: emits `level_completed` and tells the page. Only the first call after the level loaded does anything |

## Signals

| Signal | When |
|---|---|
| `level_completed(level_id: String, score: int)` | Once, when the level is won |

## Page events

Only on the web build (`OS.has_feature("web")`); elsewhere nothing is
sent. Each is one `window.dispatchEvent(new CustomEvent(...))` run through
`JavaScriptBridge.eval`.

| Event | `detail` | When |
|---|---|---|
| `montezuma:ready` | `{"v":1,"version":"<application/config/version>"}` | First processed frame |
| `montezuma:level-completed` | `{"v":1,"level":"<level id>","score":<int>}` | With `level_completed` |

String fields are JSON-escaped, including `<`, `>`, `&` and U+2028/2029
(`extension/src/demo/handoff_script.hpp`, tested by `demo_tests`).

## When the level is won

| Level goal | Win |
|---|---|
| `quotas` (rule-variant goal) | `Match3Level.level_finished` with `WON` |
| `gem_goal` only (the intro level `e1-l01`) | Not detected yet: the core does not track gems. The level plays without a win until the level session lands (WP-34) |
| Any, with the user argument `--complete-level` | On the first frame. Used by the browser smoke test |

User arguments come after `--`: `godot --path game -- --complete-level`,
or `args: ["--", "--complete-level"]` in the page's `Engine` config.

## Board

Size, kinds and seed come from the level file. When no move is left the
board is dealt again with the next seed, as in `BoardPlaytest`.

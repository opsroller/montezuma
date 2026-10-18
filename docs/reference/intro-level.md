<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# IntroLevel (web demo scene)

`IntroLevel` is a `Node3D` class from the montezuma GDExtension
(`extension/src/demo/`). It is the root of `res://scenes/demo/intro_level.tscn`,
the web build's main scene (`application/run/main_scene.web` in
`game/project.godot`). It is a thin wrapper over the game scene: its
`Game` child is the [Match3Game](match3-game.md) scene (`game.tscn`) with
`autostart_level = "e1-l01"`, which plays the level with the HUD, the
results screen, the audio and the core level session. `IntroLevel` has no
rules of its own; it listens to the game and announces the win to the
page. The page side is the
[web demo hand-off contract](contracts.md#web-demo-hand-off-owner-gameplay-programmer-wp-21wp-34-consumer-frontend-web-developer-wp-35--agreed);
how the build is made is in [Build pipeline](build-pipeline.md#web-demo).

## Properties

| Property | Default | Description |
|---|---|---|
| `game_path: NodePath` | `Game` in the scene | The `Match3Game` that plays the intro level. Its `autostart_level` is the intro level |

## Methods

| Method | Description |
|---|---|
| `get_level_id() -> String` | The intro level's id, the game's `autostart_level` (`e1-l01`) |
| `get_score() -> int` | The game's score so far; the winning score once completed |
| `is_completed() -> bool` | Whether the intro level was won |
| `complete()` | Wins now with the current score: emits `level_completed` and tells the page. Only the first completion does anything |

## Signals

| Signal | When |
|---|---|
| `level_completed(level_id: String, score: int)` | Once, the first time the game emits `level_finished(WON, score)` for the intro level |

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

| Case | Result |
|---|---|
| The game ends the intro level with `WON` (its gem goal, 10 gems at normal) | `level_completed` and the page event, once |
| The game ends it with `LOST` | Nothing; the results screen offers a retry |
| A later win (a retry after a win) | Nothing more: the page has already been told |
| The user argument `--complete-level` | On the first frame, with score 0. Used by the browser smoke test |

`--complete-level` works in release builds too. That is harmless: the
page controls the arguments, and the popup only shows download links.
It also means the event is not proof that a player won.

User arguments come after `--`: `godot --path game -- --complete-level`,
or `args: ["--", "--complete-level"]` in the page's `Engine` config.

## Intro level only

The demo plays the intro level and nothing else. When the game starts any
other level (the results screen's next button, the episode map, a level
picked from the menus), `IntroLevel` starts the intro level again on the
next idle frame, through `UiRoot`'s `play` action so the retry button
replays it too.

## Self-test

`godot --headless --path game res://scenes/demo/intro_level.tscn -- --intro-selftest`
plays the intro level by the first legal move until it ends, twice, and
asks the UI for another level. It fails when the level did not start on
load or has no gem goal, when a loss completes the demo, when the level
is not won with its gem goal, when `level_completed` fires other than
once or with a score other than the game's, or when the other level is
not sent back to the intro level. It prints
`IntroLevel self-test: <moves> moves, outcome 1, 1 completions, 0 failures`;
CI's `extension` job greps for that line ([CI pipeline](ci-pipeline.md)).

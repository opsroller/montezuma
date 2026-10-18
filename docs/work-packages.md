# Work packages

The port split into packages that separate agents run at the same time.
Each package has one role (a generic plugin from `opsroller/claude-plugins`),
owns its paths so parallel branches don't collide, and builds against
`docs/contracts.md` with stubs until its dependencies land. Stack, commands
and the role map are in `docs/project-profile.md`.

**Running one:** a thread titled `WP-NN <name>` loads `engineering-practices`
and the role's plugin, then follows the `work-packages` skill: branch
`wp/NN-slug`, owned paths, tests, Conventional Commit PR, review by the
code reviewer, squash-merge when green, tell dependants.

**Status:** `todo` · `active` · `review` · `done`. A thread updates its own
row in the PR that finishes the package.

## Wave 0: foundations

| WP | Package | Role | Owns | Depends on | Done when | Status |
|---|---|---|---|---|---|---|
| 00 | Project profile, contracts, this plan | — | `CLAUDE.md`, `.claude/`, `docs/project-profile.md`, `docs/contracts.md`, `docs/work-packages.md`, `CONTRIBUTING.md` | — | merged | review |
| 01 | C++ core: board, moves, cascades, level state, with MoveResult events | gameplay-programmer (port thread) | `core/` | — | rules from the spec in C++20 with CTest tests, MoveResult per contracts | active |
| 02 | GDExtension scaffold and core bindings | gameplay-programmer (port thread) | `extension/` (except area subfolders), `extension/src/core_bindings/` | 01 | `MzBoard` loads in Godot headless on desktop CI; each area has a `register_<area>()` stub | todo |
| 03 | Settle the rules spec | game-designer | `docs/rules-spec.md`, `docs/design/` | — | no **[verify]** left; totems, bonuses, mini-games and intro levels written; contract ids confirmed | todo |
| 04 | Visual identity | graphic-designer | `design/` | — | logo, palette, type, style guide and tokens | todo |

## Wave 1: build in parallel (17 at once)

| WP | Package | Role | Owns | Depends on | Done when | Status |
|---|---|---|---|---|---|---|
| 05 | Level data parser | gameplay-programmer | `core/src/level_data.*`, `core/include/mz/level_data.hpp`, `core/tests/test_level_data.cpp` | 01 | parses and validates the LevelData contract with clear errors | todo |
| 06 | Power Totems | gameplay-programmer | `core/**/totems.*`, `core/tests/test_totems.cpp` | 01, 03 | each totem charges from streaks and returns a tested effect result | todo |
| 07 | Bonuses | gameplay-programmer | `core/**/bonuses.*`, `core/tests/test_bonuses.cpp` | 01, 03 | earned, stored, used and upgraded per spec, tested | todo |
| 08 | Progress, save and trophies | gameplay-programmer | `core/**/progress.*`, `core/**/trophies.*`, tests | 05 | unlocks, best scores and 9 trophies serialise; tested | todo |
| 09 | Artifact piece models | technical-artist | `assets/models/artifacts/`, `tools/assets/artifacts*` | 04 | 7 pieces, distinct colour and silhouette, in the manifest, under budget | todo |
| 10 | Temple board and environment | technical-artist | `assets/models/temple/`, `tools/assets/temple*` | 04 | board and backdrop in the manifest, fits 8×8 | todo |
| 11 | Materials and effect shaders | technical-artist | `assets/materials/`, `assets/textures/` | 04 | stone, gold, jade, highlight, match flash on Mobile, Forward+ and Compatibility | todo |
| 12 | Board view and camera | graphics-programmer | `extension/src/board/`, `scenes/board/` | 02 | `MzBoardView` shows a board with placeholders, camera and lights set | todo |
| 13 | Swap, match and cascade animation | graphics-programmer | `extension/src/board/animator.*` | 01, 12 | plays any MoveResult in order at feel targets; view equals board after | todo |
| 14 | Input | graphics-programmer | `extension/src/board/input.*` | 12 | tap-tap and swipe, mouse, illegal bounce | todo |
| 15 | Sound effects | sound-designer | `assets/audio/sfx/`, `tools/audio/sfx*` | — | every effect cue in the contract exists | todo |
| 16 | Audio system | sound-designer | `extension/src/audio/`, `scenes/audio/` | 02 | `MzAudio` autoload, cue table, buses, volume API | todo |
| 17 | Music and ambience | sound-designer | `assets/audio/music/`, `tools/audio/music*` | — | menu theme, 5 episode loops, jungle ambience | todo |
| 18 | UI theme and HUD | ui-developer | `ui/theme/`, `ui/hud/`, `extension/src/ui/hud*` | 02, 04 | quotas, timer, score, totem meters, bonus slots bound to game signals | todo |
| 19 | Screens and navigation | ui-developer | `ui/screens/`, `ui/widgets/`, `ui/strings.csv`, `extension/src/ui/screens*` | 02, 04 | title, episode map, intro, pause, results, trophies, settings, credits | todo |
| 20 | Desktop builds in CI | devops-engineer | `export_presets.cfg`, `.github/workflows/build.yml` | 02 | Windows, macOS, Linux builds as CI artifacts on `main` | todo |
| 21 | Web demo build | devops-engineer | `.github/workflows/web.yml`, `tools/release/web*` | 02 | extension compiled with Emscripten; single-threaded web export runs in Chrome, Firefox and Safari | todo |

## Wave 2: content, integration, distribution

| WP | Package | Role | Owns | Depends on | Done when | Status |
|---|---|---|---|---|---|---|
| 22 | Balance simulator | gameplay-programmer | `tools/sim/` | 05, 06, 07 | C++ CLI reports win rate and time left per level and difficulty | todo |
| 23 | Episode 1 levels (includes the intro levels) | game-designer | `data/levels/e1/` | 03, 05, 22 | all load and hit sim targets | todo |
| 24-27 | Episodes 2, 3, 4, 5 (one package each) | game-designer | `data/levels/e<N>/` | 23 | same | todo |
| 28 | Game scene integration | graphics-programmer | `extension/src/game/`, `scenes/game/` | 05, 13, 16, 18 | a level plays start to finish with HUD, audio and results | todo |
| 29 | Totem and bonus visuals | graphics-programmer | `extension/src/board/effects/`, `scenes/board/effects/` | 06, 07, 11, 13 | every effect animates | todo |
| 30-33 | Bonus mini-games (one package each) | gameplay-programmer + graphics-programmer | `core/**/minigames/<id>.*`, `extension/src/minigames/<id>/`, `scenes/minigames/<id>/` | 03, 28 | playable and tested; split once WP-03 names them | todo |
| 34 | Demo mode | gameplay-programmer | `extension/src/demo/`, `scenes/demo/` | 23, 28 | demo build plays the intro levels only and, on finishing, calls the page through `JavaScriptBridge` (`window.mzDemoComplete()`); no-op off web | todo |
| 35 | Website: demo shell, downloads, credits | frontend-web-developer | `site/` | 04, 21 | site opens on the playable demo; finishing (or a skip link) shows the visitor's platform download or store badge, all platforms listed; credits page from `CREDITS.md`; deploys on merge | todo |
| 36 | Android build | devops-engineer | Android job in `.github/workflows/build.yml` | 20 | debug APK as a CI artifact | todo |
| 37 | iOS build | devops-engineer | iOS job in `.github/workflows/build.yml` | 20 | builds on a macOS runner; signing documented | todo |
| 38 | App icons and store art | graphic-designer | `design/icons/`, `design/store/` | 04, 28 | icons at every size, store graphics and captioned screenshots per store spec | todo |
| 39 | License notices and credits | devops-engineer | `CREDITS.md` structure, `tools/release/notices*` | 20 | every dependency's notice ships in-app and in releases; GPL source link per release | todo |

## Wave 3: ship

| WP | Package | Role | Owns | Depends on | Done when | Status |
|---|---|---|---|---|---|---|
| 40 | Feel pass against reference | game-designer + graphics-programmer | `extension/src/board/feel.hpp`, spec feel section | 28 | timings in the spec match reference video | todo |
| 41 | Performance pass | graphics-programmer | with owners | 28 | 60 fps mid-range Android and older iPhones; demo under 40 MB compressed | todo |
| 42 | Releases and changelog | devops-engineer | `.github/workflows/release.yml`, `tools/release/` | 20 | tag `vX.Y.Z` builds every target and publishes notes from Conventional Commits | todo |
| 43 | Store submissions | devops-engineer | `docs/stores/` | 36, 37, 38, 39, 42 | App Store, Google Play and desktop listings prepared; steps needing Charles's accounts listed for him | todo |

Wave 1 is the widest point: 17 packages that only share contracts.
Episodes 2 to 5 and the mini-games also run in parallel once their first
example sets the pattern.

Code review: every PR is reviewed by the code-reviewer role before its
author squash-merges it.

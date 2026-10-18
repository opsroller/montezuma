# Work packages

The port split into packages that separate agents run at the same time.
Each package has one role (a generic plugin from `opsroller/claude-plugins`),
owns its paths so parallel branches don't collide, and builds against
`docs/reference/contracts.md` with stubs until its dependencies land. Stack, commands
and the role map are in `docs/reference/project-profile.md`.

**Role first:** a package starts only once its role plugin is merged in
the catalog.

**Running one:** a thread titled `WP-NN <name>` loads `engineering-practices`
and the role's plugin, then follows the `work-packages` skill: branch
`wp/NN-slug`, owned paths, tests, Conventional Commit PR, review by the
code reviewer, squash-merge when green, tell dependants. Each package
also updates the docs its change needs, by Diataxis kind (see the
project profile).

**Status:** `todo` · `active` · `review` · `done`. A thread updates its own
row in the PR that finishes the package.

## Wave 0: foundations

| WP | Package | Role | Owns | Depends on | Done when | Status |
|---|---|---|---|---|---|---|
| 00 | Project profile, contracts, this plan | project-manager | `CLAUDE.md`, `.claude/`, `docs/README.md`, `docs/reference/project-profile.md`, `docs/reference/contracts.md`, `docs/reference/work-packages.md`, `CONTRIBUTING.md` | — | merged | done (#2) |
| 01 | C++ core: board, moves, cascades, level state | gameplay-programmer (port thread) | `core/` | — | rules from the spec in C++20 with CTest tests | done (#1, #4) |
| 02 | GDExtension scaffold, core bindings and Godot project | gameplay-programmer (port thread) | `extension/`, `game/`, `thirdparty/` | 01 | `Match3Board` and `BoardView3D` load in Godot headless on desktop CI | done (#3) |
| 03 | Settle the rules spec | game-designer | `docs/reference/rules-*.md`, `docs/reference/progression.md`, `docs/explanation/about-the-rules.md`, `docs/explanation/about-rule-sets-and-themes.md`, `docs/reference/balance.md` | — | no **[verify]** left; totems, bonuses, mini-games and intro levels written; contract ids confirmed | review (#14) |
| 04 | Visual identity | graphic-designer | `design/` | — | logo, palette, type, style guide and tokens | active |

## Wave 1: build in parallel (17 at once)

| WP | Package | Role | Owns | Depends on | Done when | Status |
|---|---|---|---|---|---|---|
| 05 | Level data parser | gameplay-programmer | `core/src/level_data.*`, `core/include/montezuma/level_data.hpp`, `core/tests/test_level_data.cpp` | 01 | parses and validates the LevelData contract with clear errors | todo |
| 06 | Power Totems | gameplay-programmer | `core/**/totems.*`, `core/tests/test_totems.cpp` | 01, 03 | each totem charges from streaks and returns a tested effect result | done |
| 07 | Bonuses | gameplay-programmer | `core/**/bonuses.*`, `core/tests/test_bonuses.cpp` | 01, 03 | power-ups placed from runs of 4 and 5+, carried with their pieces, set off in the step that clears them, with each tier's effect per spec, tested | review |
| 08 | Progress, save and trophies | gameplay-programmer | `core/**/progress.*`, `core/**/trophies.*`, tests | 05 | unlocks, best scores and 9 trophies serialise; tested | todo |
| 09 | Artifact piece models | technical-artist | `game/assets/models/artifacts/`, `tools/assets/artifacts*` | 04 | 7 pieces, distinct colour and silhouette, in the manifest, under budget | active (placeholder style until 04) |
| 10 | Temple board and environment | technical-artist | `game/assets/models/temple/`, `tools/assets/temple*` | 04 | board and backdrop in the manifest, fits 8×8 | active (placeholder style until 04) |
| 11 | Materials and effect shaders | technical-artist | `game/assets/materials/`, `game/assets/textures/` | 04 | stone, gold, jade, highlight, match flash on Mobile, Forward+ and Compatibility | active (placeholder style until 04) |
| 12 | Board view and camera | graphics-programmer | `extension/src/board/`, `game/scenes/board/` | 02 | `BoardView3D` shows a board with placeholders, camera and lights set | done |
| 13 | Swap, match and cascade animation | graphics-programmer | `extension/src/board/animator.*` | 01, 12 | plays any MoveResult in order at feel targets; view equals board after | review |
| 14 | Input | graphics-programmer | `extension/src/board/input.*` | 12 | tap-tap and swipe, mouse, illegal bounce | todo |
| 15 | Sound effects | sound-designer | `game/assets/audio/sfx/`, `tools/audio/sfx*` | — | every effect cue in the contract exists | done |
| 16 | Audio system | sound-designer | `extension/src/audio/`, `game/scenes/audio/` | 02 | `GameAudio` autoload, cue table, buses, volume API | done |
| 17 | Music and ambience | sound-designer | `game/assets/audio/music/`, `tools/audio/music*` | — | menu theme, 5 episode loops, jungle ambience | done |
| 18 | UI theme and HUD | ui-developer | `game/ui/theme/`, `game/ui/hud/`, `extension/src/ui/hud*` | 02, 04 | gem counter, timer, score, 7 totem meters and Frenzy bar bound to game signals; quota counters only on levels with quotas | active (theme on placeholder tokens until 04) |
| 19 | Screens and navigation | ui-developer | `game/ui/screens/`, `game/ui/widgets/`, `game/ui/strings.csv`, `extension/src/ui/screens*` | 02, 04 | title, episode map, intro, pause, results, trophies, settings, credits | review (#19) |
| 20 | Desktop builds in CI | devops-engineer | `game/export_presets.cfg`, `.github/workflows/build.yml` | 02 | Windows, macOS, Linux builds as CI artifacts on `main` | todo |
| 21 | Web demo build | gameplay-programmer (port thread) | `.github/workflows/web.yml`, `tools/release/web*` | 02 | extension compiled with Emscripten; single-threaded web export of the intro level runs in Chrome, Firefox and Safari | todo |

## Wave 2: content, integration, distribution

| WP | Package | Role | Owns | Depends on | Done when | Status |
|---|---|---|---|---|---|---|
| 22 | Balance simulator | gameplay-programmer | `tools/sim/` | 05, 06, 07 | C++ CLI reports win rate and time left per level and difficulty | todo |
| 23 | Episode 1 levels (includes the intro levels) | game-designer | `game/data/levels/e1/` | 03, 05, 22 | all load and hit sim targets | todo |
| 24-27 | Episodes 2, 3, 4, 5 (one package each) | game-designer | `game/data/levels/e<N>/` | 23 | same | todo |
| 28 | Game scene integration | graphics-programmer | `extension/src/game/`, `game/scenes/game/` | 05, 13, 16, 18 | a level plays start to finish with HUD, audio and results | todo |
| 29 | Totem and bonus visuals | graphics-programmer | `extension/src/board/effects/`, `game/scenes/board/effects/` | 06, 07, 11, 13 | every effect animates | todo |
| 30-33 | Bonus games: 30 `bg_accuracy`, 31 `bg_memory`, 32 `bg_reaction`, 33 `bg_attention` (one package each; rules in `docs/reference/rules-bonus-games.md`) | gameplay-programmer + graphics-programmer | `core/**/minigames/<id>.*`, `extension/src/minigames/<id>/`, `game/scenes/minigames/<id>/` | 03, 28 | playable and tested per the rules | todo |
| 34 | Intro-level mode | gameplay-programmer (port thread) | `extension/src/demo/`, `game/scenes/demo/` | 23, 28 | web build plays the intro level only and emits `level_completed` when it is won, forwarded to the page as a `montezuma:level-completed` DOM event through `JavaScriptBridge`; no-op off web | todo |
| 35 | Website: demo page, download popup, credits | frontend-web-developer | `site/` | 04, 21 | site opens on the playable intro level; on `montezuma:level-completed` (or a skip link) a popup offers the visitor's platform download or store badge, all platforms listed; credits page from `CREDITS.md`; deploys on merge | todo |
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

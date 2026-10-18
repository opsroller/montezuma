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
| 05 | Level data parser | gameplay-programmer | `core/src/level_data.*`, `core/include/montezuma/level_data.hpp`, `core/tests/test_level_data.cpp` | 01 | parses and validates the LevelData contract with clear errors | review |
| 06 | Power Totems | gameplay-programmer | `core/**/totems.*`, `core/tests/test_totems.cpp` | 01, 03 | each totem charges from streaks and returns a tested effect result | done |
| 07 | Bonuses | gameplay-programmer | `core/**/bonuses.*`, `core/tests/test_bonuses.cpp` | 01, 03 | power-ups placed from runs of 4 and 5+, carried with their pieces, set off in the step that clears them, with each tier's effect per spec, tested | done |
| 08 | Progress, save and trophies | gameplay-programmer | `core/**/progress.*`, `core/**/trophies.*`, tests | 05 | unlocks, best scores and 9 trophies serialise; tested | todo |
| 09 | Artifact piece models | technical-artist | `game/assets/models/<theme>/artifacts/`, piece shapes in `tools/assets/gen_theme_meshes.py` | 04 | 7 pieces, distinct colour and silhouette, in the manifest, under budget | done (#18) |
| 10 | Temple board and environment | technical-artist | `game/assets/models/<theme>/{board,backdrop}/`, `tools/assets/themes/` | 04 | board and backdrop in the manifest, fits 8×8 | done (#18) |
| 11 | Materials and effect shaders | technical-artist | `game/assets/materials/`, `game/assets/textures/`, `tools/assets/gen_theme_textures.py` | 04 | stone, gold, jade, highlight, match flash on Mobile, Forward+ and Compatibility | done (#18) |
| 12 | Board view and camera | graphics-programmer | `extension/src/board/`, `game/scenes/board/` | 02 | `BoardView3D` shows a board with placeholders, camera and lights set | done |
| 13 | Swap, match and cascade animation | graphics-programmer | `extension/src/board/animator.*` | 01, 12 | plays any MoveResult in order at feel targets; view equals board after | done |
| 14 | Input | graphics-programmer | `extension/src/board/input.*` | 12 | tap-tap and swipe, mouse, illegal bounce | done |
| 15 | Sound effects | sound-designer | `game/assets/audio/sfx/`, `tools/audio/sfx*` | — | every effect cue in the contract exists | done |
| 16 | Audio system | sound-designer | `extension/src/audio/`, `game/scenes/audio/` | 02 | `GameAudio` autoload, cue table, buses, volume API | done |
| 17 | Music and ambience | sound-designer | `game/assets/audio/music/`, `tools/audio/music*` | — | menu theme, 5 episode loops, jungle ambience | done |
| 18 | UI theme and HUD | ui-developer | `game/ui/theme/`, `game/ui/hud/`, `extension/src/ui/hud*` | 02, 04 | gem counter, timer, score, 7 totem meters and Frenzy bar bound to game signals; quota counters only on levels with quotas | done (#27; theme on placeholder tokens until 04) |
| 19 | Screens and navigation | ui-developer | `game/ui/screens/`, `game/ui/widgets/`, `game/ui/strings.csv`, `extension/src/ui/screens*` | 02, 04 | title, episode map, intro, pause, results, trophies, settings, credits | done (#19) |
| 20 | Desktop builds in CI | devops-engineer | `game/export_presets.cfg`, `.github/workflows/build.yml`, `tools/release/` | 02 | Windows, macOS, Linux builds as CI artifacts on `main` | done (#16) |
| 21 | Web demo build | gameplay-programmer (port thread) | `.github/workflows/web.yml`, `tools/release/web*` | 02 | extension compiled with Emscripten; single-threaded web export of the intro level runs in Chrome, Firefox and Safari; the first web export uses the Godot 4.7.2 pin from WP-44, which closes WP-44's web-export check | done (#40) |

## Wave 2: content, integration, distribution

| WP | Package | Role | Owns | Depends on | Done when | Status |
|---|---|---|---|---|---|---|
| 22 | Balance simulator | gameplay-programmer | `tools/sim/` | 05, 06, 07 | C++ CLI reports win rate and time left per level and difficulty | review |
| 56 | Simulator moves floor | gameplay-programmer | `tools/sim/` | 22 | `--check` fails a level from e2 on whose `greedy`/normal mean moves are under its [balance](balance.md) floor (50, 70, 90, 110); episode 1 has none. Gates 24-27 | done (#42) |
| 58 | Level clocks | gameplay-programmer + game-designer | `base_clock` in `core/` level data and session, `tools/sim/` clock check, `base_clock` in `game/data/levels/` | 22, 25 | `time_limit` replaced by `base_clock`, which the timer tier adds to (`GOAL-03`); `--check` fails a level and difficulty whose base clock is under the [clock rule](balance.md#clock); every level's clock set from it | done (#53) |
| 59 | Chains that hold | game-designer + gameplay-programmer, graphics-programmer for the view | `OBS-05` in the rules spec, chain rules in `core/`, chain effects in `extension/src/board/effects/` | 58 | chains are a lever the simulator measures: a level's chains move `greedy`'s win rate or moves more than the 1.5 points and 8 s that e3-l04's ten chains do today (see [balance](balance.md)), for example a run that breaks a chain but leaves its piece in place | todo |
| 23 | Episode 1 levels (includes the intro levels) | game-designer | `game/data/levels/e1/` | 03, 05, 22 | all load and hit sim targets | done (#37) |
| 24 | Episode 2 levels | game-designer | `game/data/levels/e2/` | 23, 56 | all load and hit sim targets, moves floor included | done (#45) |
| 25 | Episode 3 levels | game-designer | `game/data/levels/e3/` | 24 | all load and hit sim targets, moves floor included | done (#49) |
| 26 | Episode 4 levels | game-designer | `game/data/levels/e4/` | 25 | all load and hit sim targets, moves floor and clocks included | done (#59) |
| 27 | Episode 5 levels | game-designer | `game/data/levels/e5/` | 26 | same | done (#64) |
| 28 | Game scene integration | graphics-programmer | `extension/src/game/`, `game/scenes/game/` | 05, 13, 16, 18 | a level plays start to finish with HUD, audio and results | done |
| 29 | Totem and bonus visuals | graphics-programmer | `extension/src/board/effects/`, `game/scenes/board/effects/` | 06, 07, 11, 13 | every effect animates | done |
| 30-33 | Bonus games: 30 `bg_accuracy`, 31 `bg_memory`, 32 `bg_reaction`, 33 `bg_attention` (one package each; rules in `docs/reference/rules-bonus-games.md`) | gameplay-programmer + graphics-programmer | `core/**/minigames/<id>.*`, `extension/src/minigames/<id>/`, `game/scenes/minigames/<id>/` | 03, 28 | playable and tested per the rules | todo |
| 34 | Intro-level mode | gameplay-programmer (port thread) | `extension/src/demo/`, `game/scenes/demo/` | 23, 28 | web build plays the intro level only and emits `level_completed` when it is won, forwarded to the page as a `montezuma:level-completed` DOM event through `JavaScriptBridge`; no-op off web | done (`IntroLevel` wraps `Match3Game` and starts `e1-l01` through the UI; the gem goal wins in play; CI runs `--intro-selftest`) |
| 35 | Website: demo page, download popup, credits | frontend-web-developer | `site/` | 04, 21 | site opens on the playable intro level; on `montezuma:level-completed` (or a skip link) a popup offers the visitor's platform download or store badge, all platforms listed; credits page from `CREDITS.md`; deploys on merge | done (#51: the real web demo plays on the page; deploying waits on going public. Since WP-34 winning the intro level opens the popup too) |
| 36 | Android build | devops-engineer | Android job in `.github/workflows/build.yml` | 20 | debug APK as a CI artifact | todo |
| 37 | iOS build | devops-engineer | iOS job in `.github/workflows/build.yml` | 20 | builds on a macOS runner; signing documented | todo |
| 38 | App icons and store art | graphic-designer | `design/icons/`, `design/store/` | 04, 28 | icons at every size, store graphics and captioned screenshots per store spec | todo |
| 39 | License notices and credits | devops-engineer | `CREDITS.md` structure, `tools/release/notices*` | 20 | every dependency's notice ships in-app and in releases; GPL source link per release | todo |

## Wave V: high-end visual target

Brings the look up to [Visual target](visual-target.md) (why:
[About the visual target](../explanation/about-the-visual-target.md)).
44 goes first; 45 and 46 next; 47 to 53 and 55 then run in parallel. Each
look package commits its look-dev reference frames and passes the
target's acceptance tests for every tier it touches. WP-46's benchmark
owns frame-time numbers; WP-41 uses it. Wave 3 (ship) waits for Wave V,
except the optional WP-54: nothing goes public until it looks finished.

| WP | Package | Role | Owns | Depends on | Done when | Status |
|---|---|---|---|---|---|---|
| 44 | Engine upgrade to Godot 4.7 | devops-engineer + graphics-programmer | Godot pin in `.github/workflows/`, `tools/release/godot.env`, `thirdparty/godot-cpp` bump, version lines in docs | 20 | CI, desktop builds and the web export run on 4.7-stable; godot-cpp matches; no new warnings | done (#36, #40) |
| 45 | Quality tiers | graphics-programmer | `extension/src/render/` (except `rt/`), `game/scenes/render/`, tier render settings in `game/project.godot` | 44 | `desktop-high`, `desktop-standard`, `mobile`, `web` picked at start from renderer and GPU, switchable at runtime, each turning on exactly its features from the target; settings key for the UI | done ([Quality tiers](quality-tiers.md); settings screen option is the ui-developer's) |
| 46 | Benchmark scene and frame-time capture | graphics-programmer + technical-artist | `game/scenes/bench/`, `extension/src/bench/`, `tools/bench/` | 44, 13 | scripted 60 s busiest-board replay writes p50/p95 CPU and GPU frame time and the active feature list as JSON; readability capture script checks rules 2, 4, 5 and 6; how-to for running it on a device | done ([Benchmark](benchmark.md)) |
| 47 | Lighting and global illumination | technical-artist + graphics-programmer | `game/assets/models/<theme>/backdrop/` lighting nodes, `game/assets/lookdev/`, bake outputs, `tools/assets/` bake step | 45, 46 | physical light units and AgX; baked `LightmapGI` with probes on every tier; VoxelGI or SDFGI (chosen by measurement) plus SSIL on desktop; soft sun and fire shadows per tier; reference frames committed | done |
| 48 | PBR material and texture upgrade | technical-artist | `game/assets/materials/`, `game/assets/textures/`, `tools/assets/gen_theme_textures.py` | 44 | stone, gold, gem, jade and moonstone as PBR sets (base colour, normal, ORM, height) at the target's sizes; parallax stone, SSS jade and moonstone, refractive gems with fallbacks; desktop `hi/` variants plus base 1024² sets; validator checks colour space, value ranges and that every variant has a base; mobile and web exports contain only their tier's texture sizes | done |
| 49 | Fire VFX | technical-artist | `game/assets/models/<theme>/fx/fire/`, `game/assets/textures/<theme>/fx/`, `tools/assets/fx/` (fluid sim) | 45, 47 | brazier flame from our own fluid simulation (numpy; Blender's solver does not run headless, see `game/assets/PROVENANCE.md`) as motion-vector flipbooks on GPU particles, smoke, embers with turbulence and trails, flickering shadowed lights, heat haze and fog contribution on desktop; mobile and web substitutes per the target; sim settings and a script that re-runs the sims committed, recorded in `PROVENANCE.md` | done (#57) |
| 50 | Water | technical-artist | `game/assets/models/<theme>/backdrop/water*`, `game/assets/materials/shaders/SH_Water*` | 45, 47 | water channels or pool beside the platform with normals, depth colour, refraction, caustics and SSR on desktop, probe reflections on mobile and web; never inside the board rectangle | todo |
| 51 | Volumetrics and atmosphere | technical-artist | `FogVolume`s and dust in `game/assets/models/<theme>/backdrop/`, light-shaft cards | 47 | sun shafts and dust through volumetric fog on desktop, card substitutes on mobile and web, zero density over the board | todo |
| 52 | Post-processing and colour grade | technical-artist + graphic-designer | theme `WorldEnvironment` and `CameraAttributes`, `game/assets/textures/<theme>/grade/`, grade notes in `design/` | 47 | glow, LUT grade, backdrop-only depth of field and per-tier anti-aliasing and upscaling set per the target; grade checked against the palette and readability rules; re-check glow on Forward+ first: WP-44 compared 4.4.1 and 4.7.2 on Compatibility only (same exposure, cleaner shadows), not the 4.6 glow-before-tonemap change | todo |
| 53 | Realistic match, power-up and totem effects | technical-artist | `game/assets/models/<theme>/fx/`, effect scenes for `fx_*` manifest names | 45, 29 | every board effect rebuilt on GPU particles (sparks, stone dust, shards, light) with mobile and web variants, inside rule 3 of the target | todo |
| 54 | Hardware ray-tracing spike (optional) | graphics-programmer | `extension/src/render/rt/` (behind a build option) | 44, 46 | ray-traced reflections or shadows on gems and gold via the 4.7 `RenderingDevice` ray-tracing API as a compositor effect on Vulkan desktop; benchmark numbers and a written keep-or-drop decision; never required by any tier | todo |
| 55 | High-detail asset pass | technical-artist | `game/assets/models/<theme>/`, `tools/assets/gen_theme_meshes.py` | 48 | pieces, board, frame and backdrop rebuilt to the desktop budgets with LOD chains down to the mobile budgets; silhouettes and sizes unchanged; validator checks every LOD; mobile and web export presets strip LOD0 and desktop-only files; mobile install under 150 MB and web demo under 40 MB checked | todo |
| 57 | Tier material variants in the game | graphics-programmer (+ ui-developer settings row) | material swap in `extension/src/render/quality_tiers.*`, the Graphics quality row in `game/ui/screens/settings/` and `extension/src/ui/screens/settings_screen.*` | 45, 48 | the game shows the `hi/` materials on desktop tiers and the base on mobile and web, switching at runtime; settings screen offers `quality_tier`; self-test checks the swap | done (#47) |

## Wave 3: ship

| WP | Package | Role | Owns | Depends on | Done when | Status |
|---|---|---|---|---|---|---|
| 40 | Feel pass against reference | game-designer + graphics-programmer | `extension/src/board/feel.hpp`, spec feel section | 28 | timings in the spec match reference video | todo |
| 41 | Performance pass | graphics-programmer | with owners | 28, 46 | 60 fps mid-range Android and older iPhones; demo under 40 MB compressed | todo |
| 42 | Releases and changelog | devops-engineer | `.github/workflows/release.yml`, `tools/release/` | 20 | tag `vX.Y.Z` builds every target and publishes notes from Conventional Commits | todo |
| 43 | Store submissions | devops-engineer | `docs/stores/` | 36, 37, 38, 39, 42 | App Store, Google Play and desktop listings prepared; steps needing Charles's accounts listed for him | todo |

Wave 1 is the widest point: 17 packages that only share contracts.
Episodes 2 to 5 and the mini-games also run in parallel once their first
example sets the pattern.

Code review: every PR is reviewed by the code-reviewer role before its
author squash-merges it.

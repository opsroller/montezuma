# Work packages

The port split into packages that separate agents can run at the same
time. Each package has one role (a plugin in `plugins/`), owns its paths so
parallel branches don't collide, and builds against `docs/contracts.md`
with stubs until its dependencies land.

**Running one:** start a thread titled `WP-NN <name>`, install the role's
plugin (see `plugins/README.md`), and ask it to do the package. The
`work-package` skill covers the rest: branch `wp/NN-slug`, owned paths,
tests, Conventional Commit PR, telling dependants.

**Status:** `todo` · `active` · `review` · `done`. A thread updates its own
row in the PR that finishes the package.

## Wave 0: foundations (run first, 3 in parallel)

| WP | Package | Role | Owns | Depends on | Done when | Status |
|---|---|---|---|---|---|---|
| 00 | Role plugins, contracts, this plan | — | `plugins/`, `.claude-plugin/`, `docs/contracts.md`, `docs/work-packages.md`, `CONTRIBUTING.md` | — | merged | review |
| 01 | Animation events in MoveResult | gameplay-engineer | `core/board.gd`, `core/move_result.gd`, `tests/test_move_events.gd` | first core PR | MoveResult carries the events in contracts, tested for swap, cascade, refill, shuffle | todo |
| 02 | Level data loader | gameplay-engineer | `core/level_data.gd`, `tests/test_level_data.gd`, `data/levels/schema.json` | first core PR | loads and validates the LevelData contract; bad files fail with a clear error | todo |
| 03 | Settle the rules spec | level-designer | `docs/rules-spec.md` (after the port thread hands it over) | — | no **[verify]** left; totem, bonus and mini-game rules written; contract ids confirmed | todo |

## Wave 1: build in parallel (up to 16 at once)

All of these can start as soon as Wave 0 contracts are written; they stub
anything not merged yet.

| WP | Package | Role | Owns | Depends on | Done when | Status |
|---|---|---|---|---|---|---|
| 04 | Power Totems logic | gameplay-engineer | `core/totems.gd`, `tests/test_totems.gd` | 01, 03 | each totem charges from streaks and returns a tested effect result | todo |
| 05 | Bonuses logic | gameplay-engineer | `core/bonuses.gd`, `tests/test_bonuses.gd` | 01, 03 | each bonus is earned, stored, used and upgraded per spec, tested | todo |
| 06 | Progress, save and trophies | gameplay-engineer | `core/progress.gd`, `core/trophies.gd`, `tests/test_progress.gd` | 02 | unlocks, best scores and 9 trophies persist to `user://`, tested | todo |
| 07 | Artifact piece models | asset-artist | `assets/models/artifacts/`, `tools/assets/artifacts*` | — | 7 distinct pieces in the manifest, under budget, readable top-down | todo |
| 08 | Temple board and environment | asset-artist | `assets/models/temple/`, `tools/assets/temple*` | — | board and backdrop in the manifest, fits an 8×8 grid | todo |
| 09 | Materials and effect shaders | asset-artist | `assets/materials/`, `assets/textures/` | — | stone, gold, jade, highlight, match flash; Mobile and Compatibility renderers | todo |
| 10 | Board scene and camera | board-presentation | `scenes/board/board_view.*`, `scenes/board/feel.gd` | — | shows a `Board` with placeholders, camera and lighting set | todo |
| 11 | Swap, match and cascade animation | board-presentation | `scenes/board/animator.gd`, `tests/test_board_view.gd` | 01, 10 | plays any MoveResult in order at feel targets; view equals board after | todo |
| 12 | Input | board-presentation | `scenes/board/input.gd` | 10 | tap-tap and swipe on touch and mouse, illegal bounce | todo |
| 13 | Sound effects | audio-designer | `assets/audio/sfx/`, `tools/audio/sfx*` | — | every effect cue in the contract exists | todo |
| 14 | Audio system | audio-designer | `scenes/audio/` | — | `Audio` autoload, cue table, buses, volume API | todo |
| 15 | Music and ambience | audio-designer | `assets/audio/music/`, `tools/audio/music*` | — | menu theme, 5 episode loops, jungle ambience | todo |
| 16 | HUD | ui-engineer | `ui/hud/`, `ui/theme/` | game signals contract | quotas, timer, score, totem meters, bonus slots bound to signals | todo |
| 17 | Screens and navigation | ui-engineer | `ui/screens/`, `ui/widgets/`, `ui/strings.csv` | — | title, episode map, intro, pause, results, trophies, settings | todo |
| 18 | Export presets and build CI | release-engineer | `export_presets.cfg`, `.github/workflows/export.yml` | — | Windows, macOS, Linux and web builds as CI artifacts on `main` | todo |
| 19 | Changelog and releases | release-engineer | `tools/release/`, `.github/workflows/release.yml` | — | tag builds a release with notes from Conventional Commits | todo |

## Wave 2: content and integration

| WP | Package | Role | Owns | Depends on | Done when | Status |
|---|---|---|---|---|---|---|
| 20 | Balance simulator | level-designer | `tools/sim/` | 02, 04, 05 | reports win rate and time left per level and difficulty | todo |
| 21 | Episode 1 levels | level-designer | `data/levels/e1/` | 02, 03, 20 | all levels load and hit the sim targets | todo |
| 22 | Episode 2 levels | level-designer | `data/levels/e2/` | 21 | same | todo |
| 23 | Episode 3 levels | level-designer | `data/levels/e3/` | 21 | same | todo |
| 24 | Episode 4 levels | level-designer | `data/levels/e4/` | 21 | same | todo |
| 25 | Episode 5 levels | level-designer | `data/levels/e5/` | 21 | same | todo |
| 26 | Game scene integration | board-presentation | `scenes/game/` | 02, 11, 14, 16 | a level plays start to finish with HUD, audio and results | todo |
| 27 | Totem and bonus visuals | board-presentation | `scenes/board/effects/` | 04, 05, 09, 11 | every totem and bonus effect animates | todo |
| 28 | Android build | release-engineer | `.github/workflows/export.yml` (Android job) | 18 | debug APK as a CI artifact | todo |
| 29-32 | Bonus mini-games (one each) | gameplay-engineer + board-presentation | `core/minigames/<id>.gd`, `scenes/minigames/<id>/` | 03, 26 | each mini-game playable and tested; split into one package per game once WP-03 names them | todo |

## Wave 3: ship

| WP | Package | Role | Owns | Depends on | Done when | Status |
|---|---|---|---|---|---|---|
| 33 | Feel pass against reference | board-presentation + level-designer | `scenes/board/feel.gd`, spec feel section | 26 | timings recorded in the spec match reference video | todo |
| 34 | Performance pass | board-presentation | as needed, with owners | 26 | 60 fps mid-range Android, web loads under 20 MB | todo |
| 35 | iOS build | release-engineer | `.github/workflows/export.yml` (iOS job) | 18 | builds on a macOS runner; signing documented, waits on Charles's Apple account | todo |

Episodes 2 to 5 (WP-22 to 25) run in parallel once episode 1 sets the
pattern. Wave 1 is the widest point: 16 packages with no dependency on each
other beyond the contracts.

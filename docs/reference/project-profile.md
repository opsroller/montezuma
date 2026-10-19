# Project profile

What a generic role needs to know to work on Montezuma's. Roles say how
to do the job; this file says with what, and where.

## Product

- A modern 3D remake of the gameplay of *The Treasures of Montezuma*
  (developed by Visual Shape, published by Alawar Entertainment; PC 2006,
  iOS 2009; sources in `docs/reference/rules-sources.md`). It plays like
  the original and looks new. Rules are in
  `docs/reference/rules-spec.md`.
- Design direction (owner, 2026-10-19): look, setting and design follow
  sourced history of the late Mexica empire (about 1502 to 1520), led by
  the historian; the original game is a reference, not the target. The
  rules above still come from the original.
- Targets: Windows, macOS, Linux, Android, iOS, plus a web demo. The
  website's entry is a playable intro level (the game's opening levels,
  compiled to WebAssembly); finishing it opens the download and store
  links for the visitor's platform.
- Name: **Montezuma's**, an umbrella title that can later host mini games
  with their own themes and rule variants, so keep themes and rule sets
  swappable.
- License: **GPL-2.0-or-later** (see `LICENSE`), 100% open source and
  royalty free. Distribution: app stores and desktop releases, with source
  and credits linked from the project website.

## Stack

| Area | Choice |
|---|---|
| Engine | Godot 4.7.2 (version pinned in `tools/release/godot.env`) |
| Game code | C++20 only. Engine-independent rules library in `core/` (`core/include/montezuma/`, `core/src/`, `core/tests/`, namespace `montezuma`); Godot classes via GDExtension in `extension/src/` (namespace `montezuma::godot_binding`, Godot class names without prefix, e.g. `Match3Board`, `BoardView3D`) |
| Engine bindings | godot-cpp 10.0 as a git submodule in `thirdparty/godot-cpp`, built for the Godot 4.7 API (`GODOTCPP_API_VERSION` in `extension/CMakeLists.txt`); the game's `.gdextension` needs Godot 4.7 or later |
| Errors | Exceptions ON for every target (`GODOTCPP_DISABLE_EXCEPTIONS=OFF`); core reports errors by exception, binding uses noexcept `Board::try_create` and `MoveResult::stuck` |
| Godot project | `game/`: `project.godot`, `scenes/`, `assets/`, `ui/`, `data/`; built extension libraries land in `game/bin/` (not committed) |
| Build | CMake (core, extension, tools), Godot export presets for packaging |
| Tests | Core unit tests via CTest in `core/tests/`: one CTest test per `core/tests/*.cpp`, each a standalone program with its own `main` and named `test_*.cpp`; shared test helpers go in headers. New `core/src/*.cpp` files are picked up automatically. `ui_model_tests` is a CTest test built with the extension. Extension load check headless in Godot. CI (`.github/workflows/tests.yml`) runs Linux and macOS with warnings as errors on every push to any branch, and Windows too on `main`, plus an ASan/UBSan job; see [CI pipeline](ci-pipeline.md) |
| Not allowed | GDScript, JavaScript or TypeScript for game logic. Scenes (`.tscn`) and resources are data and are fine |
| Offline tools | Blender (headless, Python; the `bpy` module from pip works the same) for models; Python + numpy + ffmpeg for audio. Tools live in `tools/` and never ship |
| Renderers | Mobile (Android, iOS), Forward+ (desktop), Compatibility (web demo) |
| Web demo | Same C++ code compiled to WebAssembly with Emscripten; Godot 4.7 single-threaded web export with GDExtension support, so no special server headers are needed. The game emits `level_completed` when the intro level is won and forwards it to the page through `JavaScriptBridge` as a `montezuma:level-completed` DOM event; the page shows the download popup. The page is the only JavaScript |
| Website | Static HTML/CSS in `site/`, minimal JavaScript for platform detection and the demo hand-off. The native-code rule covers the game, not the site |

If the code and this file disagree, the code wins and this file is
updated in the same PR.

## Commands

Building, testing and running are a how-to: see
[`docs/how-to/build-and-test.md`](../how-to/build-and-test.md). CI runs the
same commands.

## Role map

Each role loads `engineering-practices` plus its own plugin from
`opsroller/claude-plugins`.

| Role plugin | Owns in this repo | Notes |
|---|---|---|
| `game-designer` | `docs/reference/rules-*.md`, `docs/reference/progression.md`, `docs/explanation/about-the-rules.md`, `docs/explanation/about-rule-sets-and-themes.md`, `docs/reference/balance.md`, `game/data/levels/` | Level files are JSON in the LevelData contract. Sim targets in `docs/reference/balance.md` |
| `gameplay-programmer` | `core/`, `extension/src/match3_board.*`, `tools/sim/` | Namespace `montezuma`; bindings in `montezuma::godot_binding` |
| `graphics-programmer` | `extension/src/board/`, `extension/src/game/`, `extension/src/render/`, `extension/src/bench/`, `game/scenes/`, `tools/bench/` | Feel constants in `extension/src/board/feel.hpp` |
| `technical-artist` | `game/assets/models/`, `game/assets/materials/`, `game/assets/textures/`, `game/assets/lookdev/`, `game/assets/manifest.json`, `tools/assets/` | Budgets below; themes, naming and validator in `docs/reference/board-themes.md`; look in `docs/explanation/art-direction.md` |
| `sound-designer` | `game/assets/audio/`, `extension/src/audio/`, `tools/audio/` | Cue list in contracts |
| `ui-developer` | `game/ui/`, `extension/src/ui/` | Credits screen reads `CREDITS.md`; implements the graphic designer's specs |
| `devops-engineer` | `.github/`, `game/export_presets.cfg`, `tools/release/`, `docs/stores/` | Builds every target including the wasm demo; store checklists in `docs/stores/` |
| `frontend-web-developer` | `site/` | Site shell around the web demo, downloads, credits page; deploys to GitHub Pages |
| `graphic-designer` | `design/` (identity, icons, store art, UI and site visuals) | Style guide in `design/README.md`; exports tokens for `game/ui/theme/` and `site/` |
| `historian` | `docs/reference/history-*.md` | Sourced history briefs and reference sets for look, setting and design; game-designer and technical-artist apply them in their own files, and the technical-artist links them from `art-direction.md`. Cites and paraphrases; images or long excerpts only under an allowed license, logged in `game/assets/PROVENANCE.md`. Commits use the `docs` scope |
| `project-manager` | `docs/reference/work-packages.md` status, plan changes | Coordinator: staffs packages, holds the owner's standing rules |
| `code-reviewer` | — (reviews every PR) | Uses the review notes below |

Shared, append-only for everyone: `game/assets/PROVENANCE.md`, `CREDITS.md`,
`CHANGELOG.md` (generated). `extension/src/register_types.cpp`: each role
adds one `register_<area>()` call, nothing else. `game/project.godot`: each
role adds only its own autoload, input-action or setting entries.

## Conventional Commit scopes

`core`, `bindings`, `sim`, `spec`, `levels`, `board`, `game`, `assets`,
`audio`, `ui`, `demo`, `ci`, `build`, `release`, `site`, `design`,
`stores`, `contracts`, `docs`.

## License headers

Every source file starts with:

```
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
```

Use `#` instead of `//` in CMake, YAML, shell and Python files. Data
files without comments (JSON, images, audio) are covered by
`game/assets/PROVENANCE.md` instead.

## Provenance (project specifics)

- All code and assets are our own. Gameplay that plays like the original
  is fine; the original's files (code, art, audio, text, levels) are
  never copied, extracted or traced.
- The original game, *The Treasures of Montezuma*, and its makers are
  credited by name as the inspiration, in-game and on the website.
- Allowed third-party licenses: CC0, CC-BY, MIT, BSD, zlib, OFL (fonts),
  and GPL-compatible. Log every asset in `game/assets/PROVENANCE.md`.

## Documentation

All docs follow [Diataxis](https://diataxis.fr), filed by kind:

| Folder | Kind | For |
|---|---|---|
| `docs/tutorials/` | Tutorial | Learning: a newcomer's first build and first change |
| `docs/how-to/` | How-to guide | A task: add a level, add a sound cue, cut a release |
| `docs/reference/` | Reference | Facts to look up: this profile, contracts, work packages, rules spec |
| `docs/explanation/` | Explanation | Why: design decisions, architecture, the port's direction |

Everything we create (code, assets, docs, plugins) is GPL-2.0-or-later.
Third-party inputs under the compatible licenses listed under Provenance
are allowed.

Every PR that changes behaviour updates the matching doc kind in the same
PR. `docs/README.md` maps the docs.

## Credits

`CREDITS.md` is the single list: the original game and its makers as the
inspiration, Claude for writing the code, Charles (opsroller) for
direction, every contributor, and third-party works with licenses. The
credits screen and the website both read from it.

## Budgets

These are the caps for everything the mobile and web builds ship.
Desktop builds also carry larger texture and LOD variants (up to 2048²,
8k-triangle pieces) that the mobile and web exports strip; per-tier
budgets, frame times and the look each tier must show are in
[Visual target](visual-target.md#how-each-tier-gets-its-assets).

- 60 fps on mid-range Android and older iPhones.
- Pieces under 2k triangles, board under 20k; textures 1024² max.
- Install size target under 150 MB on mobile; web demo under 40 MB
  compressed, playable within 10 s on broadband.

## Review notes (for `code-reviewer`)

- Godot: no logic in GDScript; GDExtension classes register in
  `extension/src/register_types.cpp`, one line per class; no per-frame allocations in `_process`; tweens
  created from C++ chain a finished callback instead of being left
  running; signals disconnected
  on free.
- C++: no Godot headers in `core/`; deterministic RNG and time; warnings
  as errors; sanitizers clean in Debug; SPDX header on every file.
- Core exceptions never cross into Godot: the binding layer catches them
  (or uses the non-throwing factory) and reports an error instead.
- The extension builds with the same warning and exception flags as the
  core, and CI builds it on Windows (on pushes to `main`).
- Every CI job configures CMake with the options it needs
  (`MONTEZUMA_BUILD_EXTENSION`, `MONTEZUMA_BUILD_TESTS`, `MONTEZUMA_WERROR`).
- Docs: the right Diataxis kind in the right folder, updated with the
  change.
- Every PR title is a Conventional Commit (it becomes the squash commit).

### Pitfalls seen in more than one review

A finding raised in two or more reviews is listed here as a rule.

- Performance figures (#35, #75, #79, #80). An absolute figure links to
  its section of `docs/reference/benchmark-results.md` and names the row
  (machine, tier, date or run); that row gives the tier, Resolution,
  3D scaling and vsync or cap mode, and a capped row says why (for
  example, Metal display sync). A before/after or "costs X ms" claim also
  compares against a run of the merge base on the same machine, tier,
  Resolution, 3D scaling and cap settings, uncapped where the driver
  allows it. Frame-time numbers come from the benchmark the plan names as
  their owner (today WP-46's), not from the row's own package.
- Plan dependencies (#35, #80). In the plan
  (`docs/reference/work-packages.md`), every package a `todo` or `active`
  row waits on, needs, or shares owned files or numbers with is in its
  `Depends on` column. Mentions of finished work, and the bracketed reason
  of a `blocked` row, are not dependencies.
- Two open PRs changing the same plan row (#7, #13, #80). A PR edits only
  its own package's row. On rebase it keeps `main`'s text for every other
  row; a PR that edits another package's row drops that edit or moves it
  to the owning PR.
- Docs rules (#80, #81). A new rule uses only terms the docs define where
  they belong (the plan's status key, `benchmark-results.md`), contradicts
  no rule already written down, and holds for the project's own existing
  rows.
- Docs left describing the old value, rule or metric (#69, #78, #86,
  #88). A PR that changes a figure, a rule, a metric or what a shader
  samples greps the whole `docs/` tree for the old wording and numbers
  and fixes every page that states them, including pages outside the
  diff and figures measured with an old metric (re-measure them or say
  which check they come from). A reference row that describes a check or
  shader matches the code at head: its formula, units and defaults.
- A decision with no explanation (#86, #88). A choice a reader would
  question (a feature on for some tiers only, a new metric and where its
  thresholds come from) is explained on an explanation page under
  `docs/explanation/` in the same PR; a reference row states what, not
  why.

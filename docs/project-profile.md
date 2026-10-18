# Project profile

What a generic role needs to know to work on Montezuma 3D. Roles say how
to do the job; this file says with what, and where.

## Product

- A modern 3D remake of the gameplay of *The Treasures of Montezuma*
  (Alawar, 2009). It plays like the original and looks new. Rules are in
  `docs/rules-spec.md`.
- Targets: Windows, macOS, Linux, Android, iOS, plus a web demo. The
  website's entry is a playable intro level (the game's opening levels,
  compiled to WebAssembly); finishing it opens the download and store
  links for the visitor's platform.
- License: GPL v2 (see `LICENSE`). Distribution: app stores and desktop
  releases, with source and credits linked from a project website.

## Stack

| Area | Choice |
|---|---|
| Engine | Godot 4.4.1 (version pinned in CI) |
| Game code | C++20 only. Engine-independent rules library in `core/`; Godot classes via GDExtension (godot-cpp) in `extension/` |
| Build | CMake (core, extension, tools), Godot export presets for packaging |
| Tests | Core unit tests via CTest in `core/tests/`; extension load check headless in Godot |
| Not allowed | GDScript, JavaScript or TypeScript for game logic. Scenes (`.tscn`) and resources are data and are fine |
| Offline tools | Blender (headless, Python) for models; Python + numpy + ffmpeg for audio. Tools live in `tools/` and never ship |
| Renderers | Mobile (Android, iOS), Forward+ (desktop), Compatibility (web demo) |
| Web demo | Same C++ code compiled to WebAssembly with Emscripten; Godot 4.4 single-threaded web export with GDExtension support, so no special server headers are needed. The demo signals completion to the page through `JavaScriptBridge`; the page is the only JavaScript |
| Website | Static HTML/CSS in `site/`, minimal JavaScript for platform detection and the demo hand-off. The native-code rule covers the game, not the site |

The first C++ core PR sets exact layout and test framework; if it differs
from this file, the code wins and this file is updated.

## Commands

```sh
# core library and tests
cmake -S core -B build/core -DCMAKE_BUILD_TYPE=Debug
cmake --build build/core -j && ctest --test-dir build/core --output-on-failure

# GDExtension
cmake -S extension -B build/ext -DCMAKE_BUILD_TYPE=Debug && cmake --build build/ext -j
godot --headless --path . --quit          # loads the extension; errors show here

# export (needs 4.4.1 export templates)
godot --headless --path . --export-release "<Preset>" build/<target>/...
```

## Role map

Each role loads `engineering-practices` plus its own plugin from
`opsroller/claude-plugins`.

| Role plugin | Owns in this repo | Notes |
|---|---|---|
| `game-designer` | `docs/rules-spec.md`, `docs/design/`, `data/levels/` | Level files are JSON in the LevelData contract. Sim targets in `docs/design/balance.md` |
| `gameplay-programmer` | `core/`, `extension/src/core_bindings/`, `tools/sim/` | Namespace `mz`; bound classes prefixed `Mz` |
| `graphics-programmer` | `extension/src/board/`, `extension/src/game/`, `scenes/` | Feel constants in `extension/src/board/feel.hpp` |
| `technical-artist` | `assets/models/`, `assets/materials/`, `assets/textures/`, `assets/manifest.json`, `tools/assets/` | Budgets below |
| `sound-designer` | `assets/audio/`, `extension/src/audio/`, `tools/audio/` | Cue list in contracts |
| `ui-developer` | `ui/`, `extension/src/ui/` | Credits screen reads `CREDITS.md`; implements the graphic designer's specs |
| `devops-engineer` | `.github/`, `export_presets.cfg`, `tools/release/`, `docs/stores/` | Builds every target including the wasm demo; store checklists in `docs/stores/` |
| `frontend-web-developer` | `site/` | Site shell around the web demo, downloads, credits page; deploys to GitHub Pages |
| `graphic-designer` | `design/` (identity, icons, store art, UI and site visuals) | Style guide in `design/README.md`; exports tokens for `ui/theme/` and `site/` |
| `code-reviewer` | — (reviews every PR) | Uses the review notes below |

Shared, append-only for everyone: `assets/PROVENANCE.md`, `CREDITS.md`,
`CHANGELOG.md` (generated). `extension/src/register_types.cpp`: each role
adds one `register_<area>()` call, nothing else.

## Conventional Commit scopes

`core`, `bindings`, `sim`, `spec`, `levels`, `board`, `game`, `assets`,
`audio`, `ui`, `demo`, `ci`, `build`, `release`, `site`, `design`,
`stores`, `contracts`, `docs`.

## Provenance (project specifics)

- The original's `.ipa` on archive.org is preserved, not openly licensed.
  Never copy, extract or imitate its code, art, audio, text or level files.
- The game's name is **Montezuma 3D**. The original's title and makers are
  named only in credits (in-game and website), as the inspiration.
- Allowed third-party licenses: CC0, CC-BY, MIT, BSD, zlib, OFL (fonts),
  and GPL-compatible. Log every asset in `assets/PROVENANCE.md`.

## Credits

`CREDITS.md` is the single list: the original game and its makers as the
inspiration, Claude for writing the code, Charles (opsroller) for
direction, every contributor, and third-party works with licenses. The
credits screen and the website both read from it.

## Budgets

- 60 fps on mid-range Android and older iPhones.
- Pieces under 2k triangles, board under 20k; textures 1024² max.
- Install size target under 150 MB on mobile; web demo under 40 MB
  compressed, playable within 10 s on broadband.

## Review notes (for `code-reviewer`)

- Godot: no logic in GDScript; GDExtension classes register through
  their area's function; no per-frame allocations in `_process`; tweens
  created from code are awaited, not left running; signals disconnected
  on free.
- C++: no Godot headers in `core/`; deterministic RNG and time; warnings
  as errors; sanitizers clean in Debug.
- Every PR title is a Conventional Commit (it becomes the squash commit).

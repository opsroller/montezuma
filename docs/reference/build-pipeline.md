# Build pipeline

Facts about the desktop build pipeline in `.github/workflows/build.yml`
and the web demo build in `.github/workflows/web.yml` ([Web demo](#web-demo)).
Tests run separately in `.github/workflows/tests.yml`.

## Triggers

| Event | Runs |
|---|---|
| Push to `main` | Every time |
| Pull request | Linux and macOS only, when it changes `build.yml`, `tools/release/`, `CMakeLists.txt`, `core/`, `extension/`, `game/` or `thirdparty/` (docs-only PRs skip it) |
| Manual (`workflow_dispatch`) | On demand, from the Actions tab |

Windows (`extension (windows)`, the Windows export and `smoke (windows)`)
runs only on pushes to `main` and manual runs, to keep PR feedback fast.
A Windows-only break shows up on `main` after merge; the next PR fixes it.

The workflow token is read-only (`contents: read`). No job uses secrets.

## Jobs

| Job | Runner | What it does | Red means |
|---|---|---|---|
| `extension (linux)` | `ubuntu-22.04` | Builds `libmontezuma.linux.template_release.x86_64.so` | The extension does not compile on Linux (GCC) |
| `extension (windows)` | `windows-2022` | Builds `libmontezuma.windows.template_release.x86_64.dll` (MSVC) | The extension does not compile with MSVC |
| `extension (macos)` | `macos-14` | Builds `libmontezuma.macos.template_release.universal.dylib` (arm64 + x86_64, macOS 10.15+) | The extension does not compile with Apple Clang for both architectures |
| `export` | `ubuntu-24.04` | Fetches the pinned Godot, exports the three presets and packages them | A preset is wrong, a library is missing, or ad-hoc signing of the macOS app failed. The `export-logs` artifact has Godot's output |
| `smoke (<platform>)` | Same runner as the matching `extension` job | Verifies the checksum, unpacks the package, runs the game headless for 120 frames, then renders the title screen and the first level at 1280×720 with Godot's Movie Maker | The packaged game does not start or logs an engine error, for example the extension not loading; or the game renders no frame on that OS |

All extension builds use `-DGODOTCPP_TARGET=template_release
-DMONTEZUMA_BUILD_EXTENSION=ON -DMONTEZUMA_BUILD_TESTS=OFF
-DMONTEZUMA_WERROR=ON`.

## Artifacts

| Artifact | Contents | Kept |
|---|---|---|
| `montezuma-linux` | `montezuma-<version>-linux-x86_64.tar.gz` and its `.sha256` | 90 days (repository default) |
| `montezuma-windows` | `montezuma-<version>-windows-x86_64.zip` and its `.sha256` | 90 days |
| `montezuma-macos` | `montezuma-<version>-macos-universal.zip` and its `.sha256` | 90 days |
| `screenshots-<platform>` | `title.png` (title screen) and `board.png` (level `e1-l01` in play, with the HUD), rendered by that platform's package | 90 days |
| `export-logs` | Godot's import and export output per platform | 7 days |
| `extension-<platform>` | The compiled libraries, passed from `extension` to `export` | 3 days |

Each package holds one folder named after the archive, containing the
game, the extension library, `LICENSE.txt` (GPL-2.0), and two files
written by `tools/release/notices.py`: `THIRD-PARTY-NOTICES.txt` (Godot's
license and `COPYRIGHT.txt`, godot-cpp's and nlohmann/json's licenses) and
`SOURCE.txt` (the commit and where to get its source). The web demo ships
the same two files from the same script.

The same files are also inside the game, readable from `res://`:

| `res://` path | From |
|---|---|
| `CREDITS.md` | root `CREDITS.md` (the credits screen shows it) |
| `LICENSE.txt` | root `LICENSE` |
| `THIRD-PARTY-NOTICES.txt` | `notices.py` |
| `SOURCE.txt` | `notices.py` |

`export-desktop.sh` stages them into `game/` with `game-files.py stage`
before exporting, the presets' `include_filter` packs them, and
`game-files.py check` fails the export if any of them is missing from the
exported game. They are generated and ignored by git.

| Platform | Game inside the folder |
|---|---|
| Linux | `montezuma.x86_64` (data embedded) and the `.so` |
| Windows | `montezuma.exe` (data embedded) and the `.dll` |
| macOS | `Montezuma's.app`, ad-hoc signed, not notarized |

Archives are packed by `tools/release/package.py` with sorted entries,
no owners, and every timestamp set to the commit time
(`SOURCE_DATE_EPOCH`).

Artifacts stay on the Actions run: no job publishes a release, a page or
a store upload. Open a run in the Actions tab and download them from its
**Artifacts** list (GitHub requires signing in to download).

### Screenshot rendering

| Platform | Renderer | Display |
|---|---|---|
| Linux | `opengl3` (Mesa software) | Xvfb, `1280x720x24` |
| Windows | `opengl3_angle` (ANGLE over Direct3D 11) | The runner's desktop session |
| macOS | `metal`, then `opengl3` if Metal fails: on the runner's paravirtual GPU Metal has crashed (Godot 4.4), Vulkan runs out of device memory, and OpenGL is Apple's software renderer at several seconds a frame | The runner's desktop session |

Each shot runs the game at the project's 1280×720 window size with
`--write-movie`, `--fixed-fps 30` and `--quit-after` (15 frames for the
title, 30 for the board, so the opening fade and the deal have finished)
and keeps the last frame. If a renderer exits with an error or writes no
frame, the next one in the table is tried. The board shot passes
`-- --start-level=e1-l01`, which skips the menus and plays that level
(see [UI](ui.md#command-line-arguments)).

## Versions

`tools/release/version.sh` prints the version of the checked-out commit:

| Commit | Version |
|---|---|
| Tagged `vX.Y.Z` | `X.Y.Z` |
| Anything else | `<last tag, or 0.0.0>-dev.<commits on the branch>.g<short sha>`, e.g. `0.0.0-dev.8.gb98fcf7` |

## Pinned inputs

| Input | Pin | Where |
|---|---|---|
| Godot editor and export templates | 4.7.2-stable, SHA-512 verified | `tools/release/godot.env` |
| godot-cpp | Submodule commit (10.0.0-stable), API version 4.7 | `thirdparty/godot-cpp`, `extension/CMakeLists.txt` |
| GitHub Actions | Full commit SHA, version in a comment | `build.yml` |
| Runner images | Named versions (`ubuntu-22.04`, `windows-2022`, `macos-14`, `ubuntu-24.04`), not `-latest` | `build.yml` |

The Godot downloads are cached under the key
`godot-<hash of godot.env>`; changing the pin changes the key.

The extension jobs compile through a compiler cache, restored from and
saved to the newest cache with the key prefix `build-extension-godot-cpp-10-<platform>`
(500 MB cap). The **Compiler cache stats** step prints hits and misses.

| Platform | Cache | Generator |
|---|---|---|
| linux | ccache | Ninja |
| windows | sccache, MSVC environment from `ilammy/msvc-dev-cmd` | Ninja |
| macos | ccache | Unix Makefiles |

See [About CI caching](../explanation/about-ci-caching.md) for why a
compiler cache is safe for release builds.

## Scripts in `tools/release/`

| Script | Use |
|---|---|
| `godot.env` | Godot version, file names and SHA-512 checksums |
| `fetch-godot.sh <dir> <template>...` | Downloads and verifies the editor and templates, installs the named templates |
| `version.sh` | Prints the version, as above |
| `export-desktop.sh <linux\|windows\|macos> <version> <outdir>` | Exports one preset and writes the archive and `.sha256` |
| `package.py <tar.gz\|zip> <root> <name> <archive>` | Packs a folder with stable bytes; refuses symlinks |
| `notices.py <outdir>` | Writes `THIRD-PARTY-NOTICES.txt` and `SOURCE.txt`; add a row to its `COMPONENTS` for every library linked into the game |
| `game-files.py stage <gamedir>` | Copies `CREDITS.md` and `LICENSE` into the Godot project and runs `notices.py` there |
| `game-files.py check <gamedir> <export>` | Exits 1 unless every staged file's bytes are inside the exported game (a file or a `.app` folder) |
| `notices/` | Verbatim upstream notice texts (Godot 4.7.2 `LICENSE.txt`, `COPYRIGHT.txt`) |
| `smoke-test.sh <archive> [<screens-dir>]` | Verifies, unpacks and runs a package on its own platform; with a folder, also saves `title.png` and `board.png` there |
| `web-fetch-emsdk.sh <dir>` | Installs the pinned Emscripten (`EMSDK_VERSION` in `godot.env`) into `<dir>` and prints its `emsdk_env.sh` |
| `web-export.sh <version> <outdir>` | Exports the `Web` preset, checks the download budget and writes the zip and `.sha256` |
| `web-smoke.py <archive> [browser...]` | Verifies and serves a web package, starts it the way the site does and checks the hand-off events |
| `web-requirements.txt` | Hash-pinned Python packages for `web-smoke.py` (Playwright) |

## Export presets

`game/export_presets.cfg` has three presets, all `x86_64` except macOS:

| Preset | Platform | Notable options |
|---|---|---|
| `Linux` | Linux | PCK embedded |
| `Windows` | Windows Desktop | PCK embedded; `application/modify_resources=false` (no rcedit, so no icon or version info in the `.exe` yet) |
| `macOS` | macOS | Universal; bundle id `io.github.opsroller.montezuma`; built-in ad-hoc signing; no notarization; minimum macOS 10.15 (x86_64) and 11.0 (arm64) |
| `Web` | Web | Single-threaded (`variant/thread_support=false`), GDExtension on (`variant/extensions_support=true`), desktop texture compression only, no PWA |

## Web demo

`.github/workflows/web.yml` builds the web demo: the intro level as a
single-threaded Godot web export with the extension compiled to
WebAssembly. It never deploys; the package is the run's artifact.

| Item | Value |
|---|---|
| Triggers | Push to `main`; pull requests touching `web.yml`, `tools/release/`, `CMakeLists.txt`, `core/`, `extension/`, `game/` or `thirdparty/`; manual |
| Runner | `ubuntu-24.04`, one job `web` |
| Emscripten | 4.0.20 (`EMSDK_VERSION` in `godot.env`, the version Godot 4.7.2's official web templates are built with), cached under `emsdk-<hash of godot.env>` |
| Extension build | `emcmake cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DMONTEZUMA_BUILD_EXTENSION=ON -DMONTEZUMA_BUILD_TESTS=OFF -DMONTEZUMA_WERROR=ON -DGODOTCPP_TARGET=template_release -DGODOTCPP_THREADS=OFF`, through ccache (`EM_COMPILER_WRAPPER=ccache`, key prefix `build-web-extension`) |
| Library | `game/bin/libmontezuma.web.template_release.wasm32.nothreads.wasm` (`web.release.wasm32` in `game/montezuma.gdextension`) |
| Template | `web_dlink_nothreads_release.zip` from the pinned templates archive |
| Textures | Generated by `.github/actions/textures` before export (they are not committed); the Web preset excludes the desktop `*/hi/*` sets |
| Renderer | Compatibility (`rendering/renderer/rendering_method.web`), 2× MSAA, 1024 directional shadow map: the `web` tier of the [visual target](visual-target.md) |
| Main scene | `res://scenes/demo/intro_level.tscn` (`application/run/main_scene.web`); see [IntroLevel](intro-level.md) |
| Budget | All files gzipped at most 40 MB, checked by `web-export.sh` |
| Browser check | `web-smoke.py` in headless Chromium 141, Firefox 142 and WebKit 26 (Playwright 1.56.0): `montezuma:ready`, then `montezuma:level-completed` for `e1-l01` with `--complete-level`, and no browser errors |

| Artifact | Contents | Kept |
|---|---|---|
| `montezuma-web` | `montezuma-<version>-web.zip` and its `.sha256` | 90 days |
| `web-export-logs` | Godot's import and export output | 7 days |

The zip holds one folder `montezuma-<version>-web/` with `montezuma.js`,
`montezuma.wasm`, `montezuma.side.wasm`, `montezuma.pck`, the extension
library, the audio worklet scripts, `montezuma.html` (Godot's page, for a
quick preview; the site uses its own), `LICENSE.txt`,
`THIRD-PARTY-NOTICES.txt` and `SOURCE.txt`.

## Not yet covered

- Real code signing and notarization (needs the owner's Apple Developer
  and Windows signing accounts; see the store work packages).
- Linux arm64 and Windows arm64 packages.
- Version numbers inside the binaries (Windows file version, macOS
  bundle version).
- Bit-for-bit reproducibility across runners: archives are stable, but
  two runners have not yet been compared.

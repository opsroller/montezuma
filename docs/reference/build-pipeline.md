# Build pipeline

Facts about the desktop build pipeline in `.github/workflows/build.yml`,
the web demo build in `.github/workflows/web.yml` ([Web demo](#web-demo))
and the release workflow in `.github/workflows/release.yml`
([Releases](#releases)).
Tests run separately in `.github/workflows/tests.yml`.

## Triggers

| Event | Runs |
|---|---|
| Push to `main` | Every time |
| Push to any other branch | Linux and macOS only, every time; a new push cancels the branch's older run |
| Pull request from a fork | Linux and macOS only, when it changes `build.yml`, `tools/release/`, `CMakeLists.txt`, `core/`, `extension/`, `game/` or `thirdparty/` (docs-only PRs skip it). A PR from a branch of this repository shows its push runs instead; see [CI pipeline: Triggers](ci-pipeline.md#triggers) |
| Manual (`workflow_dispatch`) | On demand, from the Actions tab |
| Called by `release.yml` (`workflow_call`) | For a pushed release tag `vX.Y.Z`; all three platforms |

Windows (`extension (windows)`, the Windows export and `smoke (windows)`)
runs only on pushes to `main`, release tags and manual runs, to keep branch and PR feedback fast.
PRs that change the build system also build and test on Windows in the
[`windows` workflow](ci-pipeline.md#windows-workflow); any other
Windows-only break shows up on `main` after merge, and the next PR fixes it.

The workflow token is read-only (`contents: read`). No job uses secrets.

## Jobs

| Job | Runner | What it does | Red means |
|---|---|---|---|
| `extension (linux)` | `ubuntu-22.04` | Builds `libmontezuma.linux.template_release.x86_64.so` | The extension does not compile on Linux (GCC) |
| `extension (windows)` | `windows-2022` | Builds `libmontezuma.windows.template_release.x86_64.dll` (MSVC) | The extension does not compile with MSVC |
| `extension (macos)` | `macos-14` | Builds `libmontezuma.macos.template_release.universal.dylib` (arm64 + x86_64, macOS 10.15+) | The extension does not compile with Apple Clang for both architectures |
| `export` | `ubuntu-24.04` | Fetches the pinned Godot, exports the three presets and packages them | A preset is wrong, a library is missing, or ad-hoc signing of the macOS app failed. The `export-logs` artifact has Godot's output |
| `smoke (<platform>)` | Same runner as the matching `extension` job | Verifies the checksum, unpacks the package, runs the game headless for 120 frames, then renders the title screen and the first level at 1280×720 with Godot's Movie Maker | The packaged game does not start or logs an engine error, for example the extension not loading; or the game renders no frame on that OS. Two lines Godot prints because of the project's own settings are allowed by exact text and tracked as work package 64: the separate render thread's "finalize can only be called from the render thread" at exit, and the OpenGL frame capture's image-format error that HDR 2D causes ([Engine issues](engine-issues.md)) |

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

`export-desktop.cmake` stages them into `game/` with `game-files.py stage`
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

Artifacts stay on the Actions run: only `release.yml` publishes them, as a
GitHub release for a tag ([Releases](#releases)); nothing uploads a page
or a store build. Open a run in the Actions tab and download them from its
**Artifacts** list (GitHub requires signing in to download).

### Screenshot rendering

| Platform | Renderer | Display |
|---|---|---|
| Linux | `opengl3` (Mesa software) | Xvfb, `1280x720x24` |
| Windows | `opengl3_angle` (ANGLE over Direct3D 11) | The runner's desktop session |
| macOS | `metal`, then `opengl3` if Metal fails: Metal works on the runner's paravirtual GPU with Godot 4.7.2 (4.4 crashed there), Vulkan runs out of device memory, and OpenGL is Apple's software renderer at several seconds a frame | The runner's desktop session |

Each shot runs the game at the project's 1280×720 window size with
`--write-movie` and `--fixed-fps 30`, quits through the game's own
`--quit-after-frames` (15 frames for the title, 30 for the board, so the
opening fade and the deal have finished; see
[Audio](audio.md#quitting)) with Godot's `--quit-after` as a safety net,
and keeps the last frame. If a renderer exits with an error or writes no
frame, the next one in the table is tried. If the renderer that wrote
the frames printed an engine `ERROR` to its log file or its console
(errors at exit, such as resources still in use, reach only the
console), the smoke job fails. The board shot passes
`-- --start-level=e1-l01`, which skips the menus and plays that level
(see [UI](ui.md#command-line-arguments)).

## Versions

`cmake -P tools/release/version.cmake` prints the version of the checked-out commit:

| Commit | Version |
|---|---|
| Tagged `vX.Y.Z` | `X.Y.Z` |
| Anything else | `<last tag, or 0.0.0>-dev.<commits on the branch>.g<short sha>`, e.g. `0.0.0-dev.8.gb98fcf7` |

## Pinned inputs

| Input | Pin | Where |
|---|---|---|
| Godot editor and export templates | 4.8-beta1, SHA-512 verified, downloaded from `godotengine/godot-builds` (`GODOT_RELEASES_URL`, where Godot publishes its pre-releases) | `tools/release/godot.env` |
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

## Build steps

The build steps are CMake scripts with no shell in them, so they run the
same on Linux, macOS and Windows. Run one from the repository root with
`cmake -D<VAR>=<value>... -P <script>`; options go before `-P`. Each
script's header lists its variables. Relative paths are taken from the
current folder and a leading `~` is expanded.

Every child process a step starts (Godot, git, Python, emsdk, the game)
runs with a hard timeout. A step that runs past it stops with
`<command>: timed out after <N> s` and fails; there are no retries.
Downloads stop after 600 s in all, or after 60 s with no data.

| Step | Timeouts |
|---|---|
| `fetch-godot.cmake` | each download 600 s; `godot --version` 60 s |
| `version.cmake` | each git call 60 s |
| `export-desktop.cmake`, `web-export.cmake` | Godot import 900 s, export 900 s; each Python helper 300 s; packaging 600 s |
| `web-fetch-emsdk.cmake` | clone 300 s; `emsdk install` 1200 s; `emsdk activate` 300 s |
| `smoke-test.cmake` | headless run 300 s; each screenshot capture 600 s |
| `tools/bench/run_bench.cmake` | benchmark run `SECONDS` + `WARMUP` + 300 s; readability capture 600 s; each check 300 s |

A step that changes a committed file for its run (`web-export.cmake`
stamps `game/project.godot`, `run_bench.cmake` writes
`game/override.cfg`) puts it back when it fails too.

## Scripts in `tools/release/`

| Script | Use |
|---|---|
| `godot.env` | Godot version, file names and SHA-512 checksums |
| `steps.cmake` | Helpers the steps share: running a command with a timeout, the version, checksums |
| `fetch-godot.cmake` (`DIR`, `TEMPLATES`) | Downloads and verifies the editor and templates, installs the named templates (a `;`-separated list); with `GODOT_MACOS_SWAP_TEMPLATE_VARIANTS=1` in `godot.env`, runs `macos-template-swap.py` on an installed `macos.zip` |
| `macos-template-swap.py <macos.zip>` | Swaps the release and debug binary names in a macOS template, working around Godot 4.8 beta 1's universal export picking the wrong variant ([About the engine version](../explanation/about-the-engine-version.md)) |
| `version.cmake` | Prints the version, as above |
| `export-desktop.cmake` (`PLATFORM` = `linux`, `windows` or `macos`, `OUT`, `VERSION`, `GODOT`) | Exports one preset and writes the archive and `.sha256` |
| `package.py <tar.gz\|zip> <root> <name> <archive>` | Packs a folder with stable bytes; refuses symlinks |
| `notices.py <outdir>` | Writes `THIRD-PARTY-NOTICES.txt` and `SOURCE.txt`; add a row to its `COMPONENTS` for every library linked into the game |
| `game-files.py stage <gamedir>` | Copies `CREDITS.md` and `LICENSE` into the Godot project and runs `notices.py` there |
| `game-files.py check <gamedir> <export>` | Exits 1 unless every staged file's bytes are inside the exported game (a file or a `.app` folder) |
| `notices/` | Verbatim upstream notice texts (Godot 4.8 beta 1 `LICENSE.txt`, `COPYRIGHT.txt`) |
| `smoke-test.cmake` (`ARCHIVE`, `SCREENS`) | Verifies, unpacks and runs a package on its own platform; `ARCHIVE` may be a folder holding one package; with `SCREENS`, also saves `title.png` and `board.png` there; on GitHub's macOS runners it sets `GODOT_MTL_SYNC_MODE=none`, since their paravirtual GPU cannot run Godot 4.8's default Metal barrier sync |
| `web-fetch-emsdk.cmake` (`DIR`) | Installs and activates the pinned Emscripten (`EMSDK_VERSION` in `godot.env`) in `DIR` |
| `web-export.cmake` (`OUT`, `VERSION`, `GODOT`) | Exports the `Web` preset, checks the download budget and writes the zip and `.sha256` |
| `web-smoke.py <archive> [browser...]` | Verifies and serves a web package, starts it the way the site does and checks the hand-off events |
| `changelog.py notes [--ref REF]` | Release notes for `REF` (default `HEAD`): commit titles since the previous release tag that is an ancestor of it |
| `changelog.py changelog [--next X.Y.Z] [--out FILE]` | The whole `CHANGELOG.md`, one section per release tag, newest first; `--next` adds the untagged commits as `X.Y.Z` |
| `changelog.py check-tag TAG` | Exits 1 unless `TAG` is `vX.Y.Z` |
| `publish.py --tag vX.Y.Z [--only T,...] [--repo R] [--out DIR] [--dry-run] DIR...` | Finds each target's package of that version under the `DIR`s, verifies its `.sha256`, writes `SHA256SUMS` and `notes.md` to `--out` (default `dist/release`) and runs `gh release create`, or finishes a draft release for the tag; refuses a published one; `--dry-run` stops before `gh`; each `gh` call stops after 600 s; `MONTEZUMA_GH` names another `gh` (a `.py` file runs with the same Python), which the tests use |
| `tests/` | Tests for `changelog.py` and `publish.py` against throwaway repositories (CTest `release_tools_tests`) |
| `web-requirements.txt` | Hash-pinned Python packages for `web-smoke.py` (Playwright), for CPython 3.12 and 3.13 on Linux x86_64 |

## Export presets

`game/export_presets.cfg` has three presets, all `x86_64` except macOS:

| Preset | Platform | Notable options |
|---|---|---|
| `Linux` | Linux | PCK embedded |
| `Windows` | Windows Desktop | PCK embedded; `application/modify_resources=false` (no rcedit, so no icon or version info in the `.exe` yet) |
| `macOS` | macOS | Universal; bundle id `io.github.opsroller.montezuma`; built-in ad-hoc signing; no notarization; minimum macOS 10.15 (x86_64) and 27.0 (arm64) |
| `Web` | Web | Single-threaded (`variant/thread_support=false`), GDExtension on (`variant/extensions_support=true`), desktop texture compression only, no PWA |

## Web demo

`.github/workflows/web.yml` builds the web demo: the intro level as a
single-threaded Godot web export with the extension compiled to
WebAssembly. It never deploys; the package is the run's artifact.

| Item | Value |
|---|---|
| Triggers | Push to any branch (every time); pull requests from forks touching `web.yml`, `tools/release/`, `CMakeLists.txt`, `core/`, `extension/`, `game/` or `thirdparty/`; manual; called by `release.yml` for a release tag |
| Runner | `ubuntu-24.04`, one job `web` |
| Emscripten | 6.0.12 (`EMSDK_VERSION` in `godot.env`, the version Godot 4.8 beta 1's official web templates are built with: `Dockerfile.web` on the main branch of `godotengine/build-containers`, which 4.8 betas build from), cached under `emsdk-<hash of godot.env>` |
| Extension build | `cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=~/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake -DCMAKE_BUILD_TYPE=Release -DMONTEZUMA_BUILD_EXTENSION=ON -DMONTEZUMA_BUILD_TESTS=OFF -DMONTEZUMA_WERROR=ON -DGODOTCPP_TARGET=template_release -DGODOTCPP_THREADS=OFF` with `EM_CONFIG=~/emsdk/.emscripten`, through ccache (`EM_COMPILER_WRAPPER=ccache`, key prefix `build-web-extension`) |
| Library | `game/bin/libmontezuma.web.template_release.wasm32.nothreads.wasm` (`web.release.wasm32` in `game/montezuma.gdextension`) |
| Template | `web_dlink_nothreads_release.zip` from the pinned templates archive |
| Textures | Generated by `.github/actions/textures` before export (they are not committed); the Web preset excludes the desktop `*/hi/*` sets and uses the `web/` materials ([Board themes](board-themes.md#manifest)) |
| Lightmaps | Restored or baked by `.github/actions/lightmaps` after the textures ([CI pipeline: caches](ci-pipeline.md#caches)) |
| Renderer | Compatibility (`rendering/renderer/rendering_method.web`), 2× MSAA, 1024 directional shadow map: the `web` tier of the [visual target](visual-target.md) |
| Main scene | `res://scenes/demo/intro_level.tscn` (`application/run/main_scene.web`); see [IntroLevel](intro-level.md) |
| Budget | All files gzipped at most 40 MB, checked by `web-export.cmake` |
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

## Releases

`.github/workflows/release.yml` turns a pushed tag into a GitHub
release. How to cut one: [Cut a release](../how-to/cut-a-release.md).

| Item | Value |
|---|---|
| Trigger | Push of a tag matching `v*.*.*`; nothing else |
| Concurrency | One run per tag, never cancelled |
| Token | `contents: read`, except `publish`: `contents: write` |
| Secrets | None; `publish` uses the run's `GITHUB_TOKEN` |
| Time limits | `tag is vX.Y.Z on main` 5 min; `build` and `web` as their own jobs (longest path 80 min); `publish` 30 min, because finishing a draft takes up to three `gh` calls of at most 600 s each. A whole run ends within 115 min. Releases run only for tags, so no PR or branch run waits on them |

| Job | What it does | Red means |
|---|---|---|
| `tag is vX.Y.Z on main` | `changelog.py check-tag`, checks the tagged commit is on `main` and that `version.cmake` gives it the tag's version; prints the notes to the run summary | The tag is not `vX.Y.Z`, not on `main`, or another release tag on the same commit wins the version; nothing is built |
| `build / ...` | `build.yml` for the tag: all three extensions, exports and smoke tests ([Jobs](#jobs)) | As in [Jobs](#jobs) |
| `web / web` | `web.yml` for the tag ([Web demo](#web-demo)) | As in [Web demo](#web-demo) |
| `publish` | Downloads the `montezuma-*` artifacts and runs `publish.py`: creates the release, or finishes a draft an earlier run left | A package or its checksum is missing or wrong, the release for the tag is already published, or `gh` failed |

The version of every package is the tag's (`X.Y.Z`, see
[Versions](#versions)).

| Release file | Contents |
|---|---|
| `montezuma-X.Y.Z-linux-x86_64.tar.gz` | The Linux package |
| `montezuma-X.Y.Z-windows-x86_64.zip` | The Windows package |
| `montezuma-X.Y.Z-macos-universal.zip` | The macOS package |
| `montezuma-X.Y.Z-web.zip` | The web demo |
| `<each>.sha256` | That file's checksum, `sha256sum` format |
| `SHA256SUMS` | All four checksums |

The release is titled `Montezuma's X.Y.Z`. Its notes group the
Conventional Commit titles since the previous release tag:

| Section | Commits |
|---|---|
| Breaking changes | `type!:` or a `BREAKING CHANGE:` footer |
| Features | `feat` |
| Fixes | `fix` |
| Performance | `perf` |
| Other changes | Every other type, and titles that are not Conventional Commits |

`chore(release): ...` commits are left out. A scope is shown in bold
before the description. The first release (no earlier release tag) has
no Breaking changes section: those commits go under their type. The notes end with how to check the downloads
and a link to the tag's source.

## Not yet covered

- Real code signing and notarization (needs the owner's Apple Developer
  and Windows signing accounts; see the store work packages).
- Linux arm64 and Windows arm64 packages.
- Version numbers inside the binaries (Windows file version, macOS
  bundle version).
- Build provenance and an SBOM for release files (for example
  `actions/attest-build-provenance`); releases carry only SHA-256 sums.
- A tag ruleset limiting who may create, move or delete `v*` tags; until
  then anyone with write access can start a release.
- Bit-for-bit reproducibility across runners: archives are stable, but
  two runners have not yet been compared.

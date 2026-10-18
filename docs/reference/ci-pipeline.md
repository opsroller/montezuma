<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# CI pipeline: tests, windows, assets and levels workflows

Facts about `.github/workflows/tests.yml`,
`.github/workflows/windows.yml`, `.github/workflows/assets.yml` and
`.github/workflows/levels.yml`. Desktop packaging is the separate `build`
workflow. To run any check locally, use the commands in
[Build and test](../how-to/build-and-test.md).

## Triggers

| Event | Runs when | Concurrency |
|---|---|---|
| `push` to `main` | the push changes anything outside `docs/**` and `*.md` | every run finishes |
| `pull_request` | the PR changes anything outside `docs/**` and `*.md` | a new push cancels the PR's older run |

A PR that only changes documentation gets no `tests` run.

Token permissions: `contents: read`.

## Jobs

| Job | Runners | What red means |
|---|---|---|
| `core` | Ubuntu, macOS; Windows too on pushes to `main` | the core or a Godot-free test fails to build with warnings as errors, or a CTest test fails, including `test_level_files`, which parses every level file and checks its id against its path on every PR (the balance check `montezuma_sim --check` also parses them), and the website's `site_build_tests` (Python) and `site_js_tests` (Node), which CTest adds when the runner has Python 3.10+ and Node |
| `extension` | Ubuntu; Windows too on pushes to `main` | the GDExtension (with godot-cpp) or a test fails to build, or a CTest test fails (every test, including `ui_model_tests`); on Ubuntu, Godot (the version in `tools/release/godot.env`) logs an error while loading the game headless, the audio, game or intro level self-test reports a failure ([Match3Game self-test](match3-game.md#self-test), [IntroLevel self-test](intro-level.md#self-test)), quitting from level `e1-l01` with `--quit-after-frames` prints an error or a leak ([Audio: Quitting](audio.md#quitting)), or the quality tier self-test (`--quality-selftest`, run once each with `--rendering-method forward_plus`, `mobile` and `gl_compatibility`) reports a failure, or the [benchmark](benchmark.md) smoke run (3 s headless at a locked 640×360) logs an error, drifts from its plan, loses its render size or writes no report (`check_run.py --no-budget`) |
| `sanitizers` | Ubuntu | a CTest test fails under ASan or UBSan |

Windows does not run in this workflow on PRs. Pushes to `main` run the
Windows `core` and `extension` jobs here, and the Windows package in the
`build` workflow. PRs that change the build system also get the
[`windows` workflow](#windows-workflow); any other Windows-only break
shows up on `main` after merge, and the next PR fixes it.

## Caches

| Cache | Job | Tool | Key | Size cap |
|---|---|---|---|---|
| Compiler cache | `extension (ubuntu-latest)` | ccache | `tests-extension-godot-cpp-10-ubuntu-latest` + timestamp | 500 MB |
| Compiler cache | `extension (windows-latest)` on `main`; `build and test (windows)` in the `windows` workflow | sccache | `tests-extension-godot-cpp-10-windows-latest` + timestamp | 500 MB |
| Lightmaps | `extension (ubuntu-latest)`; also the `build` export job and the `web` job | `.github/actions/lightmaps` | `lightmaps-` plus a hash of the bake inputs (see below) | about 1 MB per theme |

- The newest cache with the key prefix is restored, and a new one is saved
  at the end of every run.
- The `extension` job builds with Ninja so every compile goes through the
  cache launcher. On Windows the MSVC environment comes from
  `ilammy/msvc-dev-cmd`.
- The **Compiler cache stats** step prints hits and misses for the run.
- Caches written by a PR are visible only to that PR. PRs read the caches
  written on `main`.
- Godot itself (one download of about 60 MB, a few seconds) is not cached. The job fetches the editor pinned in `tools/release/godot.env` with `tools/release/fetch-godot.sh`, the same script and checksum the build pipeline uses.

- **Lightmaps.** The composite action `.github/actions/lightmaps` runs
  after the textures action. It restores each theme's
  `PF_Environment.lmbake`, `.exr`, `.exr.import` and `.lightmap-inputs`
  stamp, keyed by a hash of the board and backdrop folders, the materials,
  the textures' `.inputs` stamps, the `LD_*_Bake.tscn` scenes, the bake
  driver `lightmap_baker.cpp`, `godot.env` and `project.godot`. Then
  `tools/assets/bake_lightmaps.sh --check` compares the stamps; only when
  one is stale does the action install Mesa lavapipe and xvfb, build the
  native extension if the job has none (and delete it afterwards, so it
  never reaches a web or desktop export), import the project and bake
  (about 4 minutes for the temple on a runner without a GPU). See
  [Board themes: Lighting](board-themes.md#lighting).

The `core` and `sanitizers` jobs build in seconds and use no cache.

See [About CI caching](../explanation/about-ci-caching.md) for why the
caches are set up this way.

## Windows workflow

`.github/workflows/windows.yml` builds the core, the GDExtension and every
CTest test with MSVC and warnings as errors, then runs CTest, on
`windows-latest`.

| Event | Runs when |
|---|---|
| `pull_request` | the PR changes `CMakeLists.txt`, any `**/CMakeLists.txt`, any `**/*.cmake`, `.gitmodules`, `thirdparty/**` or `windows.yml` |
| `workflow_dispatch` | on demand, from the Actions tab |

| Job | What red means |
|---|---|
| `build and test (windows)` | the build fails to configure, compile or link with MSVC (for example a C runtime mismatch between targets, error LNK2038), or a CTest test fails on Windows. Reproduce with [Build and test on Windows](../how-to/build-and-test.md#build-and-test-on-windows) |

It reads and writes the sccache cache `tests-extension-godot-cpp-10-windows-latest`
(500 MB cap), the same key as the `extension (windows-latest)` job, so a PR
starts from the cache `main` wrote. Token permissions: `contents: read`; no
secrets, no artifacts. It is path-filtered, so it is not a required check.
Why it runs only on these PRs:
[About CI caching](../explanation/about-ci-caching.md#why-windows-is-off-the-pr-path).

## Assets workflow

`.github/workflows/assets.yml` generates the board theme textures, then
runs `tools/assets/validate_assets.py` against `game/assets/` and the
theme manifest. Rules it enforces:
[Board themes: validator rules](board-themes.md#validator-rules).
It then runs the readability-check and `check_run.py` tests in
`tools/bench/tests/` with the same Pillow ([Benchmark](benchmark.md#readability-checks)),
and the texture import settings tests in `tools/assets/tests/`.

| Item | Value |
|---|---|
| Triggers | every `push` to `main` and every `pull_request`, no path filter; a new push cancels the PR's older run |
| Job | `validate`, on `ubuntu-24.04`, 10-minute timeout |
| Token permissions | `contents: read` |
| Python packages | `.github/requirements/assets.txt`: numpy 2.5.3 and Pillow 12.3.0, installed with `--require-hashes` into a virtual environment |
| Textures | composite action `.github/actions/textures`: restores `game/assets/textures/*/T_*.png`, `*/hi/T_*.png` and `.inputs` from the Actions cache, key `textures-` plus a hash of `gen_theme_textures.py`, `tools/assets/themes/*.json` and `assets.txt`; then always runs `gen_theme_textures.py`, which on a hit only checks the stamp and writes the `.import` files the cache does not hold (about 40 s on a miss). The `tests` extension job (Linux), the `build` export job and the `web` job use the same action before Godot imports the project |
| What red means | a failed `Generate textures` step (a generator error), or an `error:` line in the log names the file and the broken rule: a manifest path or scene reference that does not exist, a file that does not parse as its format, a piece or board over its triangle budget, a texture over 1024 px (2048 px in `hi/`) or not power-of-two, a texture map in the wrong colour space, slot or value range, a texture without its `.import` or with the wrong import settings, a desktop `hi/` variant without a base file, a mobile or web export preset that ships `hi/`, a missing provenance record or SPDX line, or a name off the naming table |

The job takes seconds, so it has no path filter: a check that a PR can
skip could never be made required.

## Levels workflow

`.github/workflows/levels.yml` builds the
[balance simulator](../how-to/run-the-balance-simulator.md) and plays every
level under `game/data/levels/` against the
[balance targets](balance.md).

| Item | Value |
|---|---|
| Triggers | `push` to `main` and `pull_request`, only when the change touches `game/data/levels/**`, `tools/sim/**`, `core/**`, `thirdparty/nlohmann/**` (the JSON library the core parses levels with), `CMakeLists.txt` or `.github/workflows/levels.yml`; a new push cancels the PR's older run |
| Job | `balance`, on `ubuntu-24.04`, 10-minute timeout |
| Build | Release, `MONTEZUMA_WERROR=ON`, `MONTEZUMA_BUILD_EXTENSION=OFF`, `MONTEZUMA_BUILD_TESTS=OFF`, `MONTEZUMA_BUILD_SIM=ON`; target `montezuma_sim` only |
| Command | `montezuma_sim game/data/levels --check` (200 seeds, all bots, both difficulties) |
| Token permissions | `contents: read` |
| Secrets, artifacts, caches | none |
| Required check | no: a path-filtered check never reports on PRs it skips, so requiring it would block them |
| What red means | exit 2: a level file does not parse or uses an unsupported feature, and stderr names the file and the error; exit 1: a level misses a balance target or curve rule, shown as `FAIL` rows in the report. The log ends with the local command |

Run it locally:

```sh
cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DMONTEZUMA_BUILD_EXTENSION=OFF -DMONTEZUMA_BUILD_TESTS=OFF
cmake --build build-rel --target montezuma_sim -j
build-rel/tools/sim/montezuma_sim game/data/levels --check
```

The `assets` workflow has no path filter so that it can be made required;
`levels` is filtered because building the simulator only matters when one
of its inputs changes.

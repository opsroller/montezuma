<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# CI pipeline: tests and assets workflows

Facts about `.github/workflows/tests.yml` and
`.github/workflows/assets.yml`. Desktop packaging is the separate `build`
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
| `core` | Ubuntu, macOS; Windows too on pushes to `main` | the core or a Godot-free test fails to build with warnings as errors, or a CTest test fails, including the website's `site_build_tests` (Python) and `site_js_tests` (Node), which CTest adds when the runner has Python 3.10+ and Node |
| `extension` | Ubuntu; Windows too on pushes to `main` | the GDExtension (with godot-cpp) or a test fails to build, or a CTest test fails (every test, including `ui_model_tests`); on Ubuntu, Godot 4.4.1 logs an error while loading the game headless, or the audio or game self-test reports a failure ([Match3Game self-test](match3-game.md#self-test)) |
| `sanitizers` | Ubuntu | a CTest test fails under ASan or UBSan |

Windows does not run on PRs. Pushes to `main` run the Windows `core` and
`extension` jobs here, and the Windows package in the `build` workflow,
so a Windows-only break shows up on `main` after merge, and the next PR
fixes it.

## Caches

| Cache | Job | Tool | Key | Size cap |
|---|---|---|---|---|
| Compiler cache | `extension (ubuntu-latest)` | ccache | `tests-extension-ubuntu-latest` + timestamp | 500 MB |
| Compiler cache | `extension (windows-latest)`, `main` only | sccache | `tests-extension-windows-latest` + timestamp | 500 MB |

- The newest cache with the key prefix is restored, and a new one is saved
  at the end of every run.
- The `extension` job builds with Ninja so every compile goes through the
  cache launcher. On Windows the MSVC environment comes from
  `ilammy/msvc-dev-cmd`.
- The **Compiler cache stats** step prints hits and misses for the run.
- Caches written by a PR are visible only to that PR. PRs read the caches
  written on `main`.
- Godot itself (one 60 MB download, about one second) is not cached.

The `core` and `sanitizers` jobs build in seconds and use no cache.

See [About CI caching](../explanation/about-ci-caching.md) for why the
caches are set up this way.

## Assets workflow

`.github/workflows/assets.yml` runs `tools/assets/validate_assets.py`
against `game/assets/` and the theme manifest. Rules it enforces:
[Board themes: validator rules](board-themes.md#validator-rules).

| Item | Value |
|---|---|
| Triggers | every `push` to `main` and every `pull_request`, no path filter; a new push cancels the PR's older run |
| Job | `validate`, on `ubuntu-24.04`, 5-minute timeout |
| Token permissions | `contents: read` |
| Python packages | `.github/requirements/assets.txt`: Pillow 12.3.0, installed with `--require-hashes` into a virtual environment |
| What red means | an `error:` line in the log names the file and the broken rule: a manifest path or scene reference that does not exist, a file that does not parse as its format, a piece or board over its triangle budget, a texture over 1024 px or not power-of-two, a missing provenance record or SPDX line, or a name off the naming table |

The job takes seconds, so it has no path filter: a check that a PR can
skip could never be made required.

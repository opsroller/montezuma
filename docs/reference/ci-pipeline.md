<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# CI pipeline: tests workflow

Facts about `.github/workflows/tests.yml`. Desktop packaging is the
separate `build` workflow. To run any check locally, use the commands in
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
| `core` | Ubuntu, Windows, macOS | the core or a Godot-free test fails to build with warnings as errors, or a CTest test fails |
| `extension` | Ubuntu, Windows | the GDExtension (with godot-cpp) or a test fails to build, or a CTest test fails (every test, including `ui_model_tests` and `asset_tests`, the asset validator on Python 3.12 with Pillow 12.3.0); on Ubuntu, Godot 4.4.1 logs an error while loading the game headless |
| `sanitizers` | Ubuntu | a CTest test fails under ASan or UBSan |

## Caches

| Cache | Job | Tool | Key | Size cap |
|---|---|---|---|---|
| Compiler cache | `extension (ubuntu-latest)` | ccache | `tests-extension-ubuntu-latest` + timestamp | 500 MB |
| Compiler cache | `extension (windows-latest)` | sccache | `tests-extension-windows-latest` + timestamp | 500 MB |

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

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# About the engine version

The game builds and exports with Godot 4.8 beta 1, a pre-release, not
with the last stable release, 4.7.2. This page says why, what that costs
and what stays the same. The pinned version and its checksums are in
`tools/release/godot.env`; every pinned input is listed in
[Build pipeline](../reference/build-pipeline.md#pinned-inputs).

## Why a beta

The goal for the Mac is native 5K at 120 fps with every effect on. The
Mac mini M4 renders the temple at about 113 ms a frame at native 5K
([Benchmark results](../reference/benchmark-results.md#mac-mini-m4-at-native-5k)),
and it is bound by the GPU. Godot 4.8 brings performance work in its Metal driver
([4.8 beta 1 release notes](https://godotengine.org/article/dev-snapshot-godot-4-8-beta-1/)),
a change that can speed up every effect at once without changing how
anything looks. Waiting for 4.8 stable, planned for
later this quarter, would hold back the Mac performance work by that
long, so the project moved to the first beta.

## What it costs

- **A beta can have bugs that stable does not.** Each beta is pinned
  exactly, by version and SHA-512, like a stable release, so a build
  never changes under us; moving to the next beta or to 4.8 stable is an
  ordinary pin change.
- **Pre-releases live elsewhere.** Godot publishes betas on
  `godotengine/godot-builds`, not `godotengine/godot`, so the download
  location is part of the pin (`GODOT_RELEASES_URL`).
- **The web toolchain moves with it.** The web templates of 4.8 betas
  are built with Emscripten 6.0.12 instead of 4.0.20, and the extension's
  web build must use the same version to load.
- **Distro packages lag.** Package managers ship stable Godot only, so a
  contributor runs the official 4.8 beta 1 editor binary.

## What stays the same

- **The extension is unchanged.** godot-cpp has no 4.8 API yet (10.0,
  and its `master`, stop at 4.7), so the extension is still built for the
  4.7 API. Godot loads an extension built for an older API, so the same
  library runs on 4.7.2 and 4.8 beta 1, and `compatibility_minimum`
  stays at 4.7.
- **The look.** The same replay frame captured on both engines differs
  only where GPU particles (fire embers, sparkles) land, which come out
  in other places on 4.8; the board, light and grade match.
- **Speed on Vulkan.** On the RTX 4080 at native 4K the frame costs the
  same on both versions; the change is in the Metal driver, so the gain
  can only show on the Mac. Both measurements are in
  [Benchmark results](../reference/benchmark-results.md#godot-48-beta-1-against-472).

## Going back

Restore the previous `tools/release/godot.env`, `fetch-godot.cmake`
and the notice texts from git; nothing else depends on the version.

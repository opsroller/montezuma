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
anything looks. Waiting for 4.8 stable would hold back the Mac performance work by that
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

- **The binding API.** godot-cpp has no 4.8 API yet (10.0,
  and its `master`, stop at 4.7), so the extension is still built for the
  4.7 API. Godot loads an extension built for an older API, so the same
  library runs on 4.7.2 and 4.8 beta 1, and `compatibility_minimum`
  stays at 4.7.
- **The look.** The same replay frame captured on both engines differs
  only where GPU particles (fire embers, sparkles) land, which come out
  in other places on 4.8; the board, light and grade match.
- **Speed, so far.** On the RTX 4080 at native 4K the frame costs the
  same on both versions. On the M4 at native 5K beta 1 is no faster
  either (117.5 ms against 113.1 ms, within the spread), so the hoped-for
  Metal gain has not shown up yet; later betas are compared the same
  way. One thing did improve: 4.8's Metal driver reports GPU
  timestamps, so per-pass GPU time can now be measured on the Mac. The
  measurements are in
  [Benchmark results](../reference/benchmark-results.md#godot-48-beta-1-against-472).

- **Three engine changes needed workarounds.**
  - 4.8 binds `RenderingServer.get_rendering_device`,
    `get_video_adapter_type` and `get_video_adapter_api_version` only in
    builds with a RenderingDevice, so not in the web templates, and a
    release build of the extension calls a missing method through a null
    pointer. The extension checks that a method is bound before calling
    it (`extension/src/render/rendering_device_access.hpp`).
  - For a universal macOS export, beta 1's exporter keeps the binaries of
    the wrong variant, so a release export shipped the debug engine,
    which then looked for the extension's debug library.
    `fetch-godot.cmake` swaps the two variants' names in the installed
    template (`GODOT_MACOS_SWAP_TEMPLATE_VARIANTS` in `godot.env`); set
    it to 0 for a Godot release that fixes the exporter.
  - 4.8's Metal driver syncs with barriers by default, which needs
    placement heaps; the paravirtual GPU of GitHub's macOS runners cannot
    make them, and the game crashed there. The macOS smoke test sets
    `GODOT_MTL_SYNC_MODE=none` (the older hazard tracking) on GitHub
    runners only; real Macs keep the default.

## Going back

Restore `tools/release/godot.env`, `fetch-godot.cmake`, the notice texts
and `config/features` in `game/project.godot` from git. The method checks
in the extension and the template swap script are harmless on 4.7.2 (with
the swap turned off).

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# Benchmark results

*Reference. Frame times and readability results measured with the
[benchmark and readability capture](benchmark.md), per machine, tier,
driver and resolution, against the
[tier budgets](visual-target.md#quality-tiers). How to run it and add a
row: [Run the benchmark](../how-to/run-the-benchmark.md). Why the tiers and
budgets are set as they are:
[About quality tiers](../explanation/about-quality-tiers.md).*

All times are milliseconds, nearest-rank percentiles over a 60 s capture
after a 10 s warm-up, unless a row says otherwise. "Over budget" counts
frames over the tier's `budget_ms`.

## Machines

| Machine | CPU and GPU | Memory | Display | OS | Godot | Build |
|---|---|---|---|---|---|---|
| Mac mini (Mac16,10) | Apple M4, 10-core GPU (Metal 4, Apple9) | 16 GB | Studio Display, 5K, 60 Hz | macOS 27 | 4.7.2-stable editor binary | `main` at 1b780d1 (WP-29 effects, WP-46 benchmark); universal extension, `CMAKE_BUILD_TYPE=Release`; textures from `gen_theme_textures.py` |

## Frame time

Mac mini M4, Metal, Forward+, 2026-10-18.

| Tier | Resolution | 3D scaling | `frame_ms` p50 | p95 | p99 | max | Over budget | Budget p95 | Verdict |
|---|---|---|---|---|---|---|---|---|---|
| `desktop-high` | 2560×1440 | native, TAA | 22.97 | 25.06 | 25.58 | 3561 (one hitch) | 2326 of 2326 | 16.6 | over budget, about 40 fps |
| `desktop-high` (second run) | 2560×1440 | native, TAA | 23.09 | 25.14 | 25.66 | 29.4 | 2574 of 2574 | 16.6 | over budget |
| `desktop-standard` | 1920×1080 | MetalFX temporal, 0.77 | 16.66 | 16.87 | 17.39 | 26.0 | 2761 of 3599 | 16.6 | holds 60 Hz; over only by the display period (below) |

| Tier | `cpu_ms` p50 | p95 | p99 | `gpu_timing` | Draw calls p95 (max) | Primitives p95 (max) | Video memory max | `replay.diverged` |
|---|---|---|---|---|---|---|---|---|
| `desktop-high` | 25.55 | 26.20 | 2215 | false | 129 (165) | 143 396 (217 548) | 454 MB | false |
| `desktop-high` (second run) | 25.61 | 26.46 | 29.66 | false | 129 (170) | 144 630 (217 548) | 452 MB | false |
| `desktop-standard` | 17.63 | 17.86 | 26.35 | false | 137 (192) | 131 586 (202 686) | 364 MB | false |

Measurement notes:

- **No GPU time on Metal.** Every `viewport_get_measured_render_time_gpu`
  sample is 0 with Godot 4.7.2's Metal driver, so `gpu_timing` is false
  and `gpu_ms` means nothing.
- **`cpu_ms` includes waiting for the GPU.** On Metal its p50 is above
  `frame_ms` p50, so it counts the wait for the next drawable, not only
  CPU work.
- **`desktop-high` with `--disable-vsync`.** A 30 s run gives p50 22.34,
  p95 25.27, p99 25.63, the same as with vsync.
- **`cpu_ms` p99 of 2215 in the first `desktop-high` run** is in that
  run's report as written; `frame_ms` for the same run has p99 25.58, so
  those `cpu_ms` samples do not match the frames' wall-clock time.
- **`desktop-standard` is capped by the display.** At 60 Hz each frame
  is 16.67 ms, just above the 16.6 ms budget, so frames that keep up still
  count as over budget. Godot's Metal driver stays display-synced with
  `--disable-vsync` (30 s runs: p50 16.66 to 16.67, p95 16.98 to 16.99),
  so how much headroom there is under 16.6 ms is unknown.
- **The window can change size mid-run.** In three of six
  `desktop-standard` runs the report's `device.resolution` was 3140×2104,
  5120×2576 or 5120×2880 instead of 1920×1080. Those runs are not in the
  table; only runs whose `device.resolution` matches `--size` count.
- **These runs used report schema 1.** Schema 2
  ([Benchmark](benchmark.md#budget)) judges `frame_ms` against whole
  60 Hz periods with a tolerance (so `desktop-standard` above passes and
  `desktop-high` fails), locks the render size and fails a run whose
  render or window size changes, writes `gpu_ms` as null on Metal, and
  times `process_ms` per frame instead of `Performance.TIME_PROCESS`, a
  once-a-second maximum that explains the 2215 ms `cpu_ms` p99 above.

## Draw calls

Draw calls are a count: a software renderer gives the same numbers a GPU
does with the same renderer, scene and size, so these runs count against
the cap though their frame times do not. Cloud container, llvmpipe (Mesa,
LLVM 20.1.2) under Xvfb, `opengl3` driver, Compatibility renderer,
`--quality=web`, 1280×720, 60 s capture after 10 s warm-up, extension
built `Release`, 2026-10-18. The replay matched its plan in every run.

| Build | Draw calls p95 | max | Primitives p95 | Web cap |
|---|---|---|---|---|
| `main` at b7a7157, before the fire | 313 | 355 | 106 602 | 200 |
| `main` at b23a88a, with the WP-49 fire | 324 | 366 | 99 614 | 200 |
| Same, with web batching and one sun cascade | 74 | 116 | 51 852 | 200 |

Where the draws went, from 30 s runs at b7a7157 changing one thing each:
the sun's shadow (four cascades on Compatibility, each drawing every
caster again) was about 195 of 304; one orthogonal cascade alone brought
304 to 211; batching the pieces and markers ([Board view](board-view.md#draw-calls))
took it the rest of the way, to 60. The four omni torch lights cost no
draws (Compatibility shades unshadowed lights in the base pass). Godot
4.7.2's Compatibility renderer reports its shadow-map draws under the
scene pass, so `draw_calls_by_pass.shadow_p95` is 0 there.

## WP-29 effects

The busiest-board replay plays every totem and power-up effect, so the
numbers above include them. On this machine they fit `desktop-standard`
(it keeps 60 Hz) and do not fit `desktop-high` (p95 25.1 ms against
16.6 ms). The M4's 10-core GPU is in the "Apple M1 Pro class" that
[Visual target](visual-target.md#quality-tiers) names for `desktop-high`,
so this is a miss on that tier's own hardware. The cost of the effects alone was not isolated: the benchmark
has no switch to turn them off.

## WP-51 atmosphere

Cost of the volumetric haze, dust and light-shaft cards alone: `main` at
83feede (WP-47 lighting) against the same plus the WP-51 commits
(dc2ed98). Mac mini M4, Metal, Forward+, `desktop-high`, 2560×1440,
vsync off, busiest-board replay seed 52, 10 s warm-up and 60 s capture
with `tools/bench/run_bench.sh`, 2026-10-19. Textures and fire
generated and lightmaps baked on both builds; the two builds ran
alternately in one session, two runs each.

| `frame_ms` | `main` run 1 | `main` run 2 | WP-51 run 1 | WP-51 run 2 | Change (mean of runs) |
|---|---|---|---|---|---|
| mean | 47.71 | 48.09 | 48.10 | 47.95 | +0.13 |
| p50 | 46.71 | 47.23 | 47.13 | 46.91 | +0.05 |
| p95 | 54.75 | 55.49 | 55.78 | 55.41 | +0.48 |
| p99 | 56.74 | 57.09 | 57.82 | 57.33 | +0.66 |
| Frames in 60 s | 1258 | 1248 | 1248 | 1252 | −0.2% |

| Measure | `main` | WP-51 |
|---|---|---|
| Video memory max | 1482 MB | 1504 MB |
| Draw calls p95 (scene pass) | 274 (97) | 275 (98) |
| CPU p95 (ms) | 0.8 | 0.8 |
| Scene report | `volumetric_fog` false, 0 fog volumes, 12 GPU particle systems (160 particles) | `volumetric_fog` true, 1 fog volume, 13 GPU particle systems (480 particles) |
| Saturation, `all_on` readability capture at 2560×1440 (0 to 255) | 58.8 | 72.2 |
| Mean luma, same capture | 133.3 | 131.1 |

The two `main` runs differ by 0.5 ms at p50, so the p50 change is within
run-to-run noise. With the haze, 14 of 17 `desktop-high` feature rows are
met; volumetric fog is now one of them.

## Pieces out of GI

Pieces were dynamic GI objects (`gi_mode = 2`) after WP-47, so Godot
voxelized all 64 into `VoxelGI` on every frame; they now stay out of GI
(`gi_mode = 0`) and only receive it. Machine: RTX 4080 SUPER, Ryzen 9
7950X3D, NixOS, GNOME on Wayland, Godot 4.7.2 from nixpkgs, Vulkan,
Forward+, `desktop-high` at 2560×1440 (FSR 2 at 0.5), 60 s capture after
10 s warm-up, on the live desktop. GPU timing is available on this
driver.

GNOME keeps FIFO presentation when Godot asks for vsync off, so the
uncapped runs set `display/window/vsync/vsync_mode=3` (mailbox) in an
`override.cfg`.

| Build | Presentation | `frame_ms` p50 | p95 | p99 | `gpu_ms` p50 | Video memory max |
|---|---|---|---|---|---|---|
| `main` at b67b96a | vsync, 60 Hz | 16.74 | 17.86 | 20.71 | 3.08 | 1467 MB |
| Pieces out of GI | vsync, 60 Hz | 16.70 | 16.83 | 20.08 | 4.75 | 711 MB |
| `main` at b67b96a | mailbox, uncapped | lost the Vulkan device in warm-up, 2 runs of 2 | | | | |
| Pieces out of GI | mailbox, uncapped | 2.15 | 5.35 | 6.32 | 1.98 | 711 MB |

With vsync on, the GPU clocks down between frames, so `gpu_ms` there is
not a measure of the work. Uncapped, `main` stopped with
`VK_ERROR_DEVICE_LOST` in warm-up both times, while the pieces out of GI
ran at `frame_ms` p50 2.15 ms.

On the Mac mini M4 (Metal, `desktop-high`, 2560×1440, vsync off, seed
52, three alternating runs per build, which agreed within 0.6 ms; both
builds without baked lightmaps):

| Build | `frame_ms` p50 | p95 | Video memory max | Verdict |
|---|---|---|---|---|
| `main` at b67b96a | 50.0 | 59.4 | 1503 MB | over budget |
| Pieces out of GI (a83326a) | 17.5 | 18.4 | 747 MB | over budget: p95 0.7 to 1.3 ms above the 17.5 ms limit |

`desktop-high` p95 on the M4 is 18.4 ms against the 17.5 ms limit: still
a miss. The M4 presents paced to the display even with vsync off, so no
frame there is shorter than 16.67 ms and p50 sits near that floor; the
miss is in the tail. The follow-up is the M4's p95 tail (hitches and
frames that miss a present), not the mean cost. The budget is unchanged.

## Feature rows

`check_features.py` on the same reports. Rows that fail are the
lighting, reflection and atmosphere work in WP-47 to WP-53, still to do.

| Tier | Rows met | Met | Not met |
|---|---|---|---|
| `desktop-high` | 6 of 18 | Forward+, sun shadows 4096, depth and height fog, glow, TAA, GPU particles under the limit | AgX, auto exposure, LightmapGI, VoxelGI or SDFGI, SSIL, SSAO, shadowed fire lights, SSR, reflection probes, volumetric fog, colour-grade LUT, depth of field |
| `desktop-standard` | 8 of 19 | Forward+, no VoxelGI, no SDFGI, sun shadows (4096, need 2048), depth and height fog, glow, MetalFX temporal, GPU particles under the limit | AgX, auto exposure, LightmapGI, SSIL, SSAO, shadowed fire lights, SSR, reflection probes, volumetric fog, colour-grade LUT, depth of field |

## Readability

Captured at 540×1170 with each tier's settings; default thresholds from
[Readability checks](benchmark.md#readability-checks). Both tiers fail
the same four rules.

| Rule | `desktop-high` | `desktop-standard` | Pass |
|---|---|---|---|
| 2 Persistent effects leave the board alone | FAIL: mean 12.56, p99 30 | FAIL: mean 12.66, p99 29 | mean ≤ 1.0, p99 ≤ 4 |
| 4 Piece-to-tile contrast holds | FAIL: worst change 21.6% | FAIL: worst change 21.2% | ≤ 10% |
| 5 Kinds tell apart (closest pair, shade 0 / shade 1) | FAIL: greyscale 10.34 / 6.18, protanopia 14.86 / 11.40, deuteranopia 14.84 / 11.78, tritanopia 17.59 / 16.27 | FAIL: greyscale 9.96 / 5.90, protanopia 14.40 / 11.19, deuteranopia 15.04 / 11.16, tritanopia 17.53 / 15.81 | ≥ 12 and ≥ 1.5 × the in-kind spread |
| 6 Bloom stays within a quarter cell | FAIL: 5946 px, 4.18% of the board | FAIL: 4220 px, 2.97% of the board | ≤ 0.1% |

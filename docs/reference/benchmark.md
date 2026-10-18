<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Benchmark and readability capture

*Reference. The benchmark scene, its replay, its report, and the
readability capture and checks that measure a build against the
[Visual target](visual-target.md#acceptance-tests). Running them:
[Run the benchmark](../how-to/run-the-benchmark.md).*

## Files

| Path | What |
|---|---|
| `game/scenes/bench/bench.tscn` | Board scene plus a `BenchmarkRunner` |
| `game/scenes/bench/readability.tscn` | Board scene plus a `ReadabilityCapture` |
| `extension/src/bench/replay.*` | Godot-free replay planner (`plan_replay`) |
| `extension/src/bench/frame_stats.hpp` | Godot-free percentiles (`summarize`) |
| `extension/src/bench/render_probe.*` | Feature list, device info, effect switching |
| `extension/src/bench/benchmark_runner.*` | `BenchmarkRunner` node |
| `extension/src/bench/readability_capture.*` | `ReadabilityCapture` node |
| `extension/src/bench/tests/` | `bench_tests` CTest test (planner and statistics) |
| `tools/bench/run_bench.sh` | Runs both scenes and the checks on a desktop |
| `tools/bench/check_features.py` | Feature list against one tier (`tiers.json`) |
| `tools/bench/check_readability.py` | Readability rules 2, 4, 5 and 6 on the captures (Pillow) |
| `tools/bench/tiers.json` | What each tier must have on, from the feature tables |
| `tools/bench/tests/` | Tests of the readability checks on synthetic captures (the `assets` CI job runs them) |

Both scenes hide the UI layers the autoloads draw (the menus), so the
frame is the 3D board, its theme environment and its effects.

## Replay

The busiest-board scenario. `plan_replay` scripts it on
`montezuma::Board` before the run; `BenchmarkRunner` plays the script on a
`Match3Board` built with the same size (8×8, 7 kinds) and seed, which
resolves every beat the same way. After each beat the runner compares the
board with the planned one and reports `diverged` if they differ.

Each round is, in order:

| Beats | What | Board change |
|---|---|---|
| 3 | swap | the legal swap with the most cascade steps, then the most cleared pieces |
| 1 | `powerup:tnt` | TNT at tier 3 on a cell inside the outer ring, through `clear_cells` |
| 3 | swap | as above |
| 1 | `powerup:lightning` | Lightning at tier 3, as above |
| 2 | swap | as above |
| 7 | `totem:fire`, `totem:time`, `totem:gold`, `totem:harvest`, `totem:ward`, `totem:gift`, `totem:paint` | every totem at its top tier; fire clears its cells, the six whose effects the Board does not model hold the board for 0.6 s |

The plan has 6 rounds; the runner starts it again from a fresh deal
(`replay.loops`) if the capture outlasts it. Beats play back to back at
feel speed (animation speed 1). Seed 52, the scene's default, gives seven
beats of four or more cascade steps in the first three rounds, about the
first 70 s; `bench_tests` holds it to at least five.

`BenchmarkRunner` emits `bench_event(label: String, cells: Array)` as each
beat starts. `bench.tscn` connects it to the board view's `play_effect`
(WP-29), so every totem and power-up beat plays its effect and the
capture measures it; WP-53's `fx_*` scenes replace the placeholders
through the same call.

## Measurement

| Phase | Length | What happens |
|---|---|---|
| Warm-up | `warmup_seconds` (10 s) | replay runs, nothing is recorded (shader compiles, first pool allocations) |
| Capture | `measure_seconds` (60 s) | one sample per frame |

Per frame, from the previous frame (the newest the engine has):

| Sample | Source |
|---|---|
| `cpu_ms` | `Performance.TIME_PROCESS` plus `RenderingServer.get_frame_setup_time_cpu()` plus `viewport_get_measured_render_time_cpu()` of the root viewport |
| `gpu_ms` | `RenderingServer.viewport_get_measured_render_time_gpu()` of the root viewport |
| `frame_ms` | wall-clock time between frames |
| draw calls, primitives | `Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME`, `RENDER_TOTAL_PRIMITIVES_IN_FRAME` |
| video memory | `Performance.RENDER_VIDEO_MEM_USED`, maximum |

Percentiles are nearest-rank. Wall-clock time sets both phase lengths, so
a slow device plays fewer beats in the same 60 s.

## Options

Properties on `BenchmarkRunner`, or user arguments after `--` on the
command line:

| Property | Argument | Default |
|---|---|---|
| `board_view_path` | — | `../Board/View` in the scene |
| `seed` | `--bench-seed=N` | 52 |
| `warmup_seconds` | `--bench-warmup=S` | 10 |
| `measure_seconds` | `--bench-seconds=S` | 60 |
| `output_path` | `--bench-out=PATH` | `user://bench/benchmark.json` |
| `tier` | `--bench-tier=NAME` | empty: the tier [`QualityTiers`](quality-tiers.md) has in effect. A label only; to run another tier, pass `--quality=NAME` (QualityTiers' own argument) |
| `budget_ms` | `--bench-budget=MS` | 0: the tier's p95 budget, 33.3 for `web`, else 16.6 |
| `quit_when_done` | `--bench-stay` sets it false | true |

The runner quits with status 1 when the replay diverged, else 0.

`ReadabilityCapture`: `seed` (`--capture-seed=N`, 52), `output_dir`
(`--capture-out=DIR`, `user://bench/readability`), `settle_frames` (45,
frames each effect set renders before its capture).

## Report

`BenchmarkRunner` writes one JSON object to `output_path` and prints it on
one line starting `BENCH_JSON ` (for devices where the file is hard to
reach):

| Field | Meaning |
|---|---|
| `schema` | 1 |
| `tier`, `budget_ms` | as resolved above |
| `warmup_seconds`, `measure_seconds` | as run |
| `device` | `os`, `model`, `godot`, `rendering_method` (`forward_plus`, `mobile`, `gl_compatibility`), `rendering_driver`, `adapter`, `vendor`, `api_version`, `resolution` [w, h] |
| `gpu_timing` | false when every GPU sample was 0 (headless, or a driver without timer queries); then `gpu_ms` means nothing |
| `cpu_ms`, `gpu_ms`, `frame_ms` | each `{frames, mean, p50, p95, p99, max, over_budget}`; `over_budget` counts frames over `budget_ms` |
| `render` | `draw_calls_p95`, `draw_calls_max`, `primitives_p95`, `primitives_max`, `video_mem_max_mb` |
| `replay` | `scenario` (`busiest-board`), `seed`, `beats`, `loops`, `deep_cascades` (beats of 4+ steps played), `max_cascade_steps`, `diverged` |
| `features` | the feature list below, read when the capture starts |
| `quality_features` | `QualityTiers.get_active_features()`: the feature ids the active tier switches on ([Quality tiers](quality-tiers.md#feature-ids)) |

## Features

Read from the root viewport, its world environment, the active camera's
attributes and the visible nodes of the scene tree.

| Key | Type | True or counted when |
|---|---|---|
| `environment` | bool | a world environment is active |
| `tonemapper` | string | `linear`, `reinhardt`, `filmic`, `aces`, `agx` |
| `glow`, `fog`, `volumetric_fog`, `ssr`, `ssao`, `ssil`, `sdfgi` | bool | that `Environment` setting is on |
| `color_grade_lut` | bool | adjustments on with a colour-correction texture |
| `auto_exposure` | bool | the camera attributes have auto exposure on |
| `depth_of_field` | bool | near or far DOF blur on (`CameraAttributesPractical`) |
| `lightmap_gi`, `voxel_gi` | bool | a visible `LightmapGI` / `VoxelGI` node |
| `reflection_probes`, `fog_volumes`, `decals` | int | visible nodes of that type |
| `gpu_particles`, `gpu_particle_amount` | int | emitting `GPUParticles3D` nodes and their summed `amount` |
| `cpu_particles` | int | emitting `CPUParticles3D` nodes |
| `sun_shadows`, `sun_shadow_size` | bool, int | a `DirectionalLight3D` casts shadows; the directional shadow atlas size setting |
| `omni_spot_lights`, `shadowed_omni_spot_lights` | int | visible omni and spot lights, and those casting shadows |
| `taa` | bool | the viewport uses TAA |
| `msaa_3d` | int | 0, 2, 4 or 8 |
| `screen_space_aa` | string | `none`, `fxaa`, `smaa` |
| `scaling_3d_mode`, `scaling_3d_scale` | string, float | `bilinear`, `fsr1`, `fsr2`, `metalfx_spatial`, `metalfx_temporal`, `nearest`; render scale |

`check_features.py REPORT [--tier NAME]` (the tier defaults to the report's; a readability `layout.json` needs it) compares these, plus
`device.rendering_method`, with the tier's rows in `tiers.json` and exits 1
when a row is not met. Rows cover what the features can show; resolution
of SSR, volumetric fog quality, HDR output and flipbook sizes are not in
the list.

## Readability capture

`ReadabilityCapture` deals the board for its seed, then renders it under
four effect sets, `settle_frames` frames each, and saves the viewport:

| File | Effects on |
|---|---|
| `all_on.png` | everything the scene has |
| `all_off.png` | none of the groups below |
| `atmosphere_only.png` | atmosphere: depth and volumetric fog, `FogVolume`s, depth of field, particles |
| `glow_only.png` | glow |

The third group, screen-space passes (SSR, SSAO, SSIL, SDFGI), is on only
in `all_on.png`. A group the scene has off stays off in every capture.
`layout.json` gives `viewport` [w, h], `board_rect` [x0, y0, x1, y1] (the
cells, frame excluded), `cell_px`, `kinds`, `seed`, every cell's `x`, `y`,
`kind`, `shade` ((x + y) mod 2, the two tile shades) and screen `rect`,
plus `features` and `device` as in the report.

## Readability checks

`check_readability.py DIR [--json REPORT]` prints a PASS or FAIL line per
rule and exits 1 if any fails. Luma is 0-255 (8-bit captures).

| Rule | Measured | Pass (defaults, options) |
|---|---|---|
| 2 Persistent effects leave the board alone | luma difference between `atmosphere_only` and `all_off` inside `board_rect` | mean ≤ 1.0 (`--rule2-mean`) and 99th percentile ≤ 4 (`--rule2-p99`) |
| 4 Piece-to-tile contrast holds | per cell, tile = ring inside the cell edge, piece = middle pixels that differ from the tile in `all_off`; WCAG contrast ratio of their mean relative luminance, averaged per kind and shade, `all_on` versus `all_off` | change ≤ 10% for all 14 kind-shade groups (`--rule4-change`) |
| 5 Kinds tell apart in greyscale and colour-blindness | each kind's cells on one shade shrunk to 12×12 and averaged; simulated in greyscale (Rec. 709) and protanopia, deuteranopia, tritanopia (Machado et al. 2009, severity 1); mean per-pixel difference of the closest pair | closest pair ≥ 12 (`--rule5-distance`) and ≥ 1.5 × the largest difference between two halves of one kind's cells (`--rule5-ratio`), on both shades |
| 6 Bloom stays within a quarter cell | pixels where `glow_only` is at least 6 brighter than `all_off` (`--bloom-level`), outside a quarter cell of a pixel at luma 235 or more in `all_off` (`--emissive-level`) | at most 0.1% of the board (`--rule6-share`) |

Rule 5 is meant for a capture at the smallest phone size (540×1170, as
`run_bench.sh --readability` does). The thresholds are starting values for
the technical-artist to tune against reviewed captures; a change to one is
a change to this table.

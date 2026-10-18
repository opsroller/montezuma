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
| `extension/src/bench/frame_stats.hpp` | Godot-free percentiles (`summarize`), budget verdict (`judge_budget`), render-size tracking (`SizeTracker`) |
| `extension/src/bench/render_probe.*` | Feature list, device info, effect switching |
| `extension/src/bench/benchmark_runner.*` | `BenchmarkRunner` node |
| `extension/src/bench/readability_capture.*` | `ReadabilityCapture` node |
| `extension/src/bench/tests/` | `bench_tests` CTest test (planner, statistics, budget, render size) |
| `tools/bench/run_bench.sh` | Runs both scenes and the checks on a desktop |
| `tools/bench/check_run.py` | Budget verdict, render size and replay of one report |
| `tools/bench/check_features.py` | Feature list against one tier (`tiers.json`) |
| `tools/bench/check_readability.py` | Readability rules 2, 4, 5 and 6 on the captures (Pillow) |
| `tools/bench/tiers.json` | What each tier must have on, from the feature tables |
| `tools/bench/tests/` | Tests of the readability checks on synthetic captures and of `check_run.py` on synthetic reports (the `assets` CI job runs them) |

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
| `process_ms` | `Performance.TIME_PROCESS`: game logic |
| `render_cpu_ms` | `RenderingServer.get_frame_setup_time_cpu()` plus `viewport_get_measured_render_time_cpu()` of the root viewport. On Metal this includes waiting for the next drawable, so with vsync on it grows to fill the display period (`timing.render_cpu_includes_display_wait`) |
| `cpu_ms` | `process_ms` plus `render_cpu_ms` |
| `gpu_ms` | `RenderingServer.viewport_get_measured_render_time_gpu()` of the root viewport |
| `frame_ms` | wall-clock time between frames |
| draw calls, primitives | `Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME`, `RENDER_TOTAL_PRIMITIVES_IN_FRAME` |
| video memory | `Performance.RENDER_VIDEO_MEM_USED`, maximum |
| render size | the root viewport's texture size, checked against the requested size |

At the start of the capture the runner also reads the display's refresh
rate (`DisplayServer.screen_get_refresh_rate()`, -1 when unknown) and the
vsync mode.

Percentiles are nearest-rank. Wall-clock time sets both phase lengths, so
a slow device plays fewer beats in the same 60 s.

## Render size

With `render_size` set (`--bench-size=WxH`, which `run_bench.sh` always
passes), the runner locks the size before the warm-up: the window gets
that size and may not be resized, and the root window's content scale
mode is `viewport` at that size (aspect `ignore`), so the frame renders at
exactly W×H even if the OS moves, shrinks or rescales the window. Every
captured frame's render size is compared with the request; with no
request, with the first frame's. Any mismatch fails the run.

## Budget

The [visual target's](visual-target.md#quality-tiers) frame budgets are
whole periods of a 60 Hz display (period 1000/60 = 16.667 ms; "16.6 ms"
and "33.3 ms" in the target are one and two periods). The verdict judges
`frame_ms`, the time the player sees, because GPU timing is not available
on every driver:

| Tier | Rule |
|---|---|
| `desktop-high`, `desktop-standard`, `mobile` | p95 ≤ one period |
| `web` | p95 ≤ two periods and p50 ≤ one period |

Each rule passes when the percentile is at most its budget × (1 +
`tolerance`), tolerance 0.05 by default: a vsync-locked 60 fps run sits a
little either side of 16.667 ms and passes, a 23 ms p95 fails. The
graphics-programmer owns the tolerance; a change to it is a change to this
section. `over_budget` counts frames above the p95 limit (budget plus
tolerance). A display slower than 60 Hz less the tolerance (a 50 Hz panel)
cannot show the target, so with vsync on the verdict fails as
`display-limited`; with vsync off, or when the refresh is unknown, it is
not held against the run.

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
| `budget_ms` | `--bench-budget=MS` | 0: the tier's p95 budget, two 60 Hz periods (33.333) for `web`, else one (16.667); a value replaces the p95 budget |
| `tolerance` | `--bench-tolerance=F` | 0.05 |
| `render_size` | `--bench-size=WxH` | (0, 0): not locked, but the size must not change |
| `disable_vsync` | `--bench-vsync=off` sets it true | false: the project's vsync |
| `quit_when_done` | `--bench-stay` sets it false | true |

The runner quits with status 1 when the replay diverged, the render size
did not hold or `--bench-size` is malformed, else 0. An over-budget run
still quits 0: the verdict is in the report, and `check_run.py` fails on
it.

`ReadabilityCapture`: `seed` (`--capture-seed=N`, 52), `output_dir`
(`--capture-out=DIR`, `user://bench/readability`), `settle_frames` (45,
frames each effect set renders before its capture).

## Report

`BenchmarkRunner` writes one JSON object to `output_path` and prints it on
one line starting `BENCH_JSON ` (for devices where the file is hard to
reach):

| Field | Meaning |
|---|---|
| `schema` | 2 (1 had no `budget`, `resolution`, `display`, `timing`, `process_ms`, `render_cpu_ms`) |
| `tier`, `budget_ms` | as resolved above; `budget_ms` is the p95 budget before the tolerance |
| `budget` | `metric` (`frame_ms`), `target_hz` (60), `period_ms`, `tolerance`, `limits` (each `percentile`, `budget_ms`, `limit_ms`, `value_ms`, `pass`), `pass`, `reason` (empty on a pass) |
| `resolution` | `requested` [w, h] or null, `locked`, `render` (first frame), `min`, `max`, `mismatched_frames`, `pass` |
| `display` | `refresh_hz` (-1 when unknown), `vsync` (`disabled`, `enabled`, `adaptive`, `mailbox`) |
| `timing` | `gpu` (as `gpu_timing`), `gpu_note` (why, when false), `render_cpu_includes_display_wait` (true on Metal) |
| `warmup_seconds`, `measure_seconds` | as run |
| `device` | `os`, `model`, `godot`, `rendering_method` (`forward_plus`, `mobile`, `gl_compatibility`), `rendering_driver`, `adapter`, `vendor`, `api_version`, `resolution` [w, h] |
| `gpu_timing` | false when every GPU sample was 0 (headless, or a driver without timer queries: Metal in Godot 4.7.2); then `gpu_ms` means nothing and no verdict uses it |
| `process_ms`, `render_cpu_ms`, `cpu_ms`, `gpu_ms`, `frame_ms` | each `{frames, mean, p50, p95, p99, max, over_budget}`; `over_budget` counts frames over the p95 limit |
| `render` | `draw_calls_p95`, `draw_calls_max`, `primitives_p95`, `primitives_max`, `video_mem_max_mb` |
| `replay` | `scenario` (`busiest-board`), `seed`, `beats`, `loops`, `deep_cascades` (beats of 4+ steps played), `max_cascade_steps`, `diverged` |
| `features` | the feature list below, read when the capture starts |
| `quality_features` | `QualityTiers.get_active_features()`: the feature ids the active tier switches on ([Quality tiers](quality-tiers.md#feature-ids)) |

`check_run.py REPORT [--size WxH] [--no-budget]` prints a PASS or FAIL
line for the budget verdict (with its reason), the render size (with
`--size`, also that the run was locked at that size) and the replay, plus
NOTE lines for the display, missing GPU timing and the Metal display
wait. It exits 1 when a check fails and 2 when the report is not schema 2.
`--no-budget` prints the budget as SKIP (headless or software-rendered
runs).

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

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Render tuning screen

Reference for the render tuning screen: a panel over the running board
demo with a live performance readout and a control for every rendering
and lighting setting. For people tuning the look or chasing frame time.
How to use it for a task: [Tune rendering live](../how-to/tune-rendering-live.md).

Owner: graphics-programmer.

## Files

| Path | What |
|---|---|
| `game/scenes/bench/tuner.tscn` | The board, a `BenchmarkRunner` with `capture = false` playing the benchmark replay in a loop, and a `RenderTuner` on a `CanvasLayer` |
| `extension/src/bench/render_tuner.*` | `RenderTuner` (the panel), `FrameGraph` (the frame-time graph), `TunerSettings` (settings with no object of their own, shown as properties) |
| `extension/src/bench/tuner_model.hpp` | Godot-free hint-string parsing and the rolling frame-time window, tested by `bench_tests` |

## Opening it

```sh
godot --path game res://scenes/bench/tuner.tscn
```

Any `QualityTiers` argument works too, for example `-- --quality=desktop-standard`
or `-- --quality-native` ([Quality tiers](quality-tiers.md#command-line)).

| Key or argument | Effect |
|---|---|
| F1 | Hides or shows the panel (it costs a few draw calls of its own; hide it to read the scene alone) |
| F12 | Saves the window to `user://tuner/shot-<n>.png` |
| `--gpu-profile` (Godot's own, before `--`) | Godot captures a GPU timestamp per pass, and the readout lists the slowest passes |
| `-- --tuner-shot=<png>` | Saves a screenshot after 6 s and quits |
| `-- --tuner-selftest` | Opens every section, writes every control's value back unchanged, forces the glow feature off, drives the glow intensity slider, resets, and prints `RenderTuner self-test: N checks, F failures`; exits 1 on a failure. Runs headless too |

The panel scales with the window: 1× up to 1080 lines on the short side,
2× at 2160, adjustable under **Display and frame pacing**.

## Readout

Refreshed four times a second; averages cover the last second of frames.

| Line | Source |
|---|---|
| Tier, rendering method, driver, GPU | `QualityTiers`, `RenderingServer` |
| Output and 3D size, scale, upscaler, TAA, vsync, refresh rate | root viewport texture size, `scaling_3d_scale` and `scaling_3d_mode`, `DisplayServer` |
| FPS, frame mean, p99, max | wall-clock time between frames (nearest-rank p99, as the [benchmark](benchmark.md#measurement)) |
| GPU mean and p99, CPU render | `viewport_get_measured_render_time_gpu()` and `get_frame_setup_time_cpu()` + `viewport_get_measured_render_time_cpu()` of the root viewport; frames with no GPU timestamp are left out |
| Draw calls by pass, objects, triangles | `Performance` monitors and `viewport_get_render_info` (`VISIBLE`, `SHADOW`, `CANVAS`) |
| VRAM | `Performance.RENDER_VIDEO_MEM_USED`, `RENDER_TEXTURE_MEM_USED`, `RENDER_BUFFER_MEM_USED` |
| Graph | the last 600 frames: whole frame (white) and GPU (orange), with lines at 120 and 60 fps |
| Slowest GPU passes | `RenderingDevice` captured timestamps of the last frame: the time from each to the next. Without `--gpu-profile` only the viewport start and the project's own markers (Backdrop Blur, RT Reflections) are there |

The readout is a live view, not a measurement: numbers that go into
[Benchmark results](benchmark-results.md) come from the
[benchmark](../how-to/run-the-benchmark.md).

## Sections

Each section builds its rows the first time it opens. **Rescan** finds
the scene's nodes again; the panel also rescans after a tier change.

| Section | Edits |
|---|---|
| Quality tier and effects | the tier (`QualityTiers.preview_choice`, not saved) and every [feature id](quality-tiers.md#feature-ids): Tier, Off or On (`set_feature_override`). Each row says whether the tier has the feature and whether it is on now |
| Display and frame pacing | vsync mode, `Engine.max_fps`, window mode, panel scale |
| Viewport | root viewport `scaling_3d_mode`, `scaling_3d_scale`, `fsr_sharpness`, `texture_mipmap_bias`, `anisotropic_filtering_level`, `msaa_3d`, `screen_space_aa`, `use_taa`, `use_debanding`, `use_occlusion_culling`, `mesh_lod_threshold`, the positional shadow atlas, `debug_draw` (Godot's debug views: lighting, overdraw, wireframe, GI buffers and more), `use_hdr_2d`, `vrs_mode` |
| Renderer quality | `RenderingServer` globals: sun shadow atlas size and depth, soft shadow filter quality (sun and omni/spot), SSAO and SSIL quality, half size, adaptive target, blur passes and fade, SSR half size, glow bicubic upscale, GI half resolution, VoxelGI quality, SDFGI rays and convergence, volumetric fog froxel grid and filter, subsurface scattering quality and scale, specular roughness limiter. They have no getters, so values start from the project settings and, for those the tier sets, from the tier's profile |
| Environment | every editable bool, number and colour of each `Environment` in the scene (background, ambient and reflected light, tonemap, SSR, SSAO, SSIL, SDFGI, glow, fog, volumetric fog, adjustments), grouped as in the editor |
| Camera / auto exposure | each `CameraAttributes`: exposure, auto exposure, depth of field |
| Custom effects | each compositor effect on a `WorldEnvironment` or `Camera3D`: `BackdropBlur`, `RtReflections` when built in, with `enabled` and their own properties |
| Lights, GI and reflection probes, Fog volumes, Particles, Decals | one sub-section per node: `visible` plus the properties its class adds below `VisualInstance3D` (`GeometryInstance3D` for particles) |
| Shader parameters | every `ShaderMaterial` on a mesh, overlay, fog volume or particle process material: its `shader_parameter/*` uniforms and render priority (the fog veil, water, fire and the other custom shaders) |

Properties of other types (textures, meshes, curves) and flag or layer
masks are left out.

Changes apply at once and last for the run. **Reset all** clears the
feature overrides, goes back to the tier the run started with, and puts
every setting shown so far back to the value it had when first shown.

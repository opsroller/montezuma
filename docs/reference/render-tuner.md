<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Render tuning screen

Reference for the render tuning screen: a panel over the running board
demo with a live performance readout and a control for every rendering
and lighting setting. For people tuning the look or chasing frame time.
How to use it for a task: [Tune rendering live](../how-to/tune-rendering-live.md)
and [Measure what a frame costs, step by step](../how-to/measure-a-frame-step-by-step.md).

Owner: graphics-programmer.

## Files

| Path | What |
|---|---|
| `game/scenes/bench/tuner.tscn` | The board, a `BenchmarkRunner` with `capture = false` playing the benchmark replay in a loop, and a `RenderTuner` on a `CanvasLayer`. Unlike `bench.tscn` ([Benchmark](benchmark.md#files)) it has no `ScenePreloader`, so its first seconds compile pipelines ([About pipeline compilation](../explanation/about-pipeline-compilation.md)) |
| `game/scenes/bench/game_tuner.tscn` | The game (`scenes/main.tscn`, menus included) with a `RenderTuner` on a `CanvasLayer` |
| `game/scenes/bench/yardstick/*.tscn` | The [yardstick](#yardstick) scenes, each with a `RenderTuner` on a `CanvasLayer` |
| `extension/src/bench/render_tuner.*` | `RenderTuner` (the panel), `FrameGraph` (the frame-time graph), `TunerSettings` (settings with no object of their own, shown as properties) |
| `extension/src/bench/tuner_model.hpp` | Godot-free hint-string parsing and the rolling frame-time window, tested by `bench_tests` |

## Opening it

```sh
godot --path game res://scenes/bench/tuner.tscn
```

| Key or argument | Effect |
|---|---|
| F1 | Hides or shows the panel (it costs a few draw calls of its own; hide it to read the scene alone) |
| F12 | Saves the window to `user://tuner/shot-<n>.png` |
| `--gpu-profile` (Godot's own, before `--`) | Godot captures a GPU timestamp per pass, and the readout lists the slowest passes |
| `-- --start-screen=` (`UiRoot`'s, with nothing after `=`) | Empties the menu stack, so the autoloaded UI draws nothing over the scene |
| `-- --tuner-print=<seconds>` | Prints the readout, and with `--gpu-profile` the per-pass list, to stdout that often, each block headed `RenderTuner <run time> s`; the run starts with the panel hidden |
| `-- --tuner-quit=<seconds>` | Quits after that long |
| `-- --tuner-off=<a,b,...>` | Starts with those [custom-rendering](#sections) toggles off: a shader name such as `SH_TriplanarPBR`, or `effects`, `flicker`, `timer` |
| `-- --tuner-shot=<png>` | Saves a screenshot after 6 s and quits |
| `-- --tuner-selftest` | Opens every section, writes every control's value back unchanged, drives the glow intensity slider, resets, and prints `RenderTuner self-test: N checks, F failures`; exits 1 on a failure. Runs headless too |

The panel scales with the window: 1× up to 1080 lines on the short side,
2× at 2160, adjustable under **Display and frame pacing**.

## Readout

Refreshed four times a second; averages cover the last second of frames.

| Line | Source |
|---|---|
| Rendering method, driver, GPU | `RenderingServer` |
| Output and 3D size, scale, upscaler, TAA, vsync, refresh rate | root viewport texture size, `scaling_3d_scale` and `scaling_3d_mode`, `DisplayServer` |
| MSAA 3D, MSAA 2D, screen-space AA, HDR 2D | root viewport `msaa_3d`, `msaa_2d`, `screen_space_aa`, `use_hdr_2d` (the last shown only when on) |
| FPS, frame mean, p99, max | wall-clock time between frames (nearest-rank p99, as the [benchmark](benchmark.md#measurement)) |
| GPU mean and p99, CPU render | `viewport_get_measured_render_time_gpu()` and `get_frame_setup_time_cpu()` + `viewport_get_measured_render_time_cpu()` of the root viewport; frames with no GPU timestamp are left out |
| Draw calls by pass, objects, triangles | `Performance` monitors and `viewport_get_render_info` (`VISIBLE`, `SHADOW`, `CANVAS`) |
| VRAM | `Performance.RENDER_VIDEO_MEM_USED`, `RENDER_TEXTURE_MEM_USED`, `RENDER_BUFFER_MEM_USED` |
| Graph | the last 600 frames: whole frame (white) and GPU (orange), with lines at 120 and 60 fps |
| Slowest GPU passes | `RenderingDevice` captured timestamps of the last frame: the time from each to the next. Without `--gpu-profile` only the viewport start and the project's own markers (RT Reflections) are there |

The readout is a live view, not a measurement: numbers that go into
[Benchmark results](benchmark-results.md) come from the
[benchmark](../how-to/run-the-benchmark.md), or from a yardstick run
recorded as such.

## Sections

Each section builds its rows the first time it opens. **Rescan** finds
the scene's nodes again.

| Section | Edits |
|---|---|
| Custom rendering (on/off) | everything the project draws on top of Godot's own rendering, all on at start (the scene as the game shows it): the level timer (`Match3Game.timer_enabled`), the board's match effects (`BoardEffects`), the brazier light flicker (`FlickerLight3D`); each of our surface shaders (`SH_Gem`, `SH_TriplanarPBR`, `SH_Water`, `SH_WaterSimple`: off replaces the material with a plain grey `StandardMaterial3D`); each of our effect shaders (`SH_Fire`, `SH_FireSmoke`, `SH_Ember`, `SH_EmberTrail`, `SH_Spark`, `SH_HeatHaze`, `SH_AtmosphereFog`, `SH_Dust`, `SH_CellHighlight`: off hides the node). A toggle also applies to nodes added later |
| Display and frame pacing | vsync mode, `Engine.max_fps`, window mode, panel scale |
| Viewport: resolution, AA, debug views | root viewport `scaling_3d_mode`, `scaling_3d_scale`, `fsr_sharpness`, `texture_mipmap_bias`, `anisotropic_filtering_level`, `msaa_3d`, `screen_space_aa`, `use_taa`, `use_debanding`, `use_occlusion_culling`, `mesh_lod_threshold`, the positional shadow atlas, `debug_draw` (Godot's debug views: lighting, overdraw, wireframe, GI buffers and more), `use_hdr_2d`, `vrs_mode` |
| Renderer quality (shadows, SSAO, SSIL, GI, fog) | `RenderingServer` globals: sun shadow atlas size and depth, soft shadow filter quality (sun and omni/spot), SSAO and SSIL quality, half size, adaptive target, blur passes and fade, SSR half size, glow bicubic upscale, GI half resolution, VoxelGI quality, SDFGI rays and convergence, volumetric fog froxel grid and filter, subsurface scattering quality and scale, specular roughness limiter. They have no getters, so values start from the project settings |
| Environment | every editable bool, number and colour of each `Environment` in the scene (background, ambient and reflected light, tonemap, SSR, SSAO, SSIL, SDFGI, glow, fog, volumetric fog, adjustments), grouped as in the editor |
| Camera / auto exposure | each `CameraAttributes`: exposure, auto exposure, depth of field |
| Custom effects (compositor) | each compositor effect on a `WorldEnvironment` or `Camera3D`: `RtReflections` when built in, with `enabled` and its own properties |
| Lights, GI and reflection probes, Fog volumes, Particles, Decals | one sub-section per node: `visible` plus the properties its class adds below `VisualInstance3D` (`GeometryInstance3D` for particles) |
| Shader parameters | every `ShaderMaterial` on a mesh, overlay, fog volume or particle process material: its `shader_parameter/*` uniforms and render priority (the fog veil, water, fire and the other custom shaders) |

Properties of other types (textures, meshes, curves) and flag or layer
masks are left out.

Changes apply at once and last for the run. **Reset all** puts every
setting shown so far back to the value it had when first shown.

## Yardstick

Scenes under `game/scenes/bench/yardstick/` that add the game's content
one step at a time, so that each step's cost is the difference between
two readouts. Every scene has a `RenderTuner` on a `CanvasLayer` (except
`v0_camera_plain`) and is run as

```sh
godot --path game [--gpu-profile] res://scenes/bench/yardstick/<scene>.tscn -- --start-screen= --tuner-print=10 --tuner-quit=62
```

with project settings varied through a temporary `game/override.cfg`
(ignored by git), never by editing `project.godot`
([Measure what a frame costs, step by step](../how-to/measure-a-frame-step-by-step.md)).
The camera in every scene stands where `BoardCamera3D` puts it for an
8×8 board (fov 40°); the camera attributes are the theme's, with the far
depth of field at 17.3 m, where the camera places it.

| Scene | What is in it |
|---|---|
| `s0_empty` | the project settings and autoloads only; no camera, so Godot draws no 3D |
| `v0_camera` | a camera alone, no `WorldEnvironment` (Godot's default clear colour) |
| `v0_camera_plain` | the same without a tuner, for Godot's `--print-fps` |
| `v0_camera_attrs` | the camera with the theme's `CameraAttributesPractical` (depth of field on), no `Environment` |
| `v0_default_env` | the camera and a default `Environment` (no sky, default tonemapper) |
| `v0_sky` | the camera and an `Environment` with the theme's procedural sky only |
| `v0_sky_agx` | the sky plus the theme's AgX tonemap, ambient and reflection sources and intensities |
| `s1_env` | the camera plus the theme's `WorldEnvironment` (`PF_Environment` with every mesh, light, probe, lightmap, fog volume and particle system hidden) |
| `s2_stone` | `s1_env` plus the temple's stone meshes (stone, bronze, ground, foliage, rocks), unlit |
| `s3_lights` | `s2_stone` plus the sun with shadows, the sky fill area light, the reflection probe and the lightmap |
| `s4_water` | `s3_lights` plus the water strips |
| `s5_fire` | `s4_water` plus the four brazier fires |
| `s6_fog` | `s5_fire` plus the fog bank and the dust: the whole backdrop |
| `v1_same` | the theme's `Environment` copied into the scene with nothing changed (the control for the `v1_*_off` scenes) |
| `v1_dof_off`, `v1_ssil_off`, `v1_ssao_off`, `v1_glow_off`, `v1_lut_off`, `v1_vfog_off` | the same with one effect off: depth of field, SSIL, SSAO, glow, the colour-grade LUT, volumetric fog |
| `v1_bare` | the same with depth of field, SSIL, SSAO, glow and the colour grade all off (the floor) |
| `board_plain_camera` | the board demo seen through a default `Camera3D` (fov 75, near 0.05, far 4000) instead of `BoardCamera3D` |

The `v1_*` scenes hold a copy of the environment as the generator wrote
it when they were made; after a theme change, compare them with `s1_env`
(which instances the current `PF_Environment`) before trusting them. The
numbers measured with these scenes are in
[Benchmark results](benchmark-results.md#mac-mini-m4-at-native-5k-project-settings).

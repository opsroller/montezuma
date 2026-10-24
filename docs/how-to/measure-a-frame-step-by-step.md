<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Measure what a frame costs, step by step

Use this to find where the GPU time of a frame goes: which scene content,
which effect of the theme's environment, or which project setting. It
runs the [yardstick scenes](../reference/render-tuner.md#yardstick) (an
empty scene, then a camera, then the theme's environment, then the
backdrop one part at a time) with the render tuning screen printing its
readout, and changes one project setting at a time through a temporary
`game/override.cfg`. For trying settings by hand on the running board
see [Tune rendering live](tune-rendering-live.md); for a frame-time claim
that goes in the docs, run [the benchmark](run-the-benchmark.md).

Needs the extension built in Release, the theme textures generated and
the lightmaps baked ([Build and test](build-and-test.md)), on the machine
and display you want to measure. The numbers below come from a Mac mini
M4 at 5120×2880 ([Benchmark results](../reference/benchmark-results.md#mac-mini-m4-at-native-5k-project-settings)).

## Run one yardstick scene

1. Close other programs that use the GPU, and check that
   `game/override.cfg` does not exist (it is ignored by git; move any you
   have aside).
2. Run a scene with the readout printed every 10 s and a quit after
   62 s. `--gpu-profile` (Godot's own argument, before `--`) adds the
   per-pass GPU times; `--start-screen=` with nothing after it keeps the
   menus from drawing over the scene:

   ```sh
   godot --path game --gpu-profile res://scenes/bench/yardstick/s1_env.tscn -- --start-screen= --tuner-print=10 --tuner-quit=62
   ```

   The window opens as the project settings say (full screen, vsync
   off) with the panel hidden. Every 10 s Godot prints the readout: the
   rendering method and GPU, the output and 3D sizes, the anti-aliasing
   line, FPS and frame time, GPU and CPU time, draw calls, video memory
   and, with `--gpu-profile`, the slowest passes of the last frame.
3. Read the last printout, not the first: the first 10 to 20 s include
   loading and the first frames of every shader. On the M4 the frame is
   all GPU, so FPS is the number to compare; on a machine where the CPU
   line is close to the frame time, compare the GPU line.

To see Godot's own frame counter instead of the tuner, run the plain
camera scene with `--print-fps`:

```sh
godot --path game --print-fps res://scenes/bench/yardstick/v0_camera_plain.tscn -- --start-screen=
```

## Find which content costs

Run the steps in order and subtract each result from the next:

| Scene | Adds |
|---|---|
| `s0_empty.tscn` | nothing: the project settings and autoloads. With no camera Godot draws no 3D, so this is the cost of the window alone |
| `v0_camera.tscn` | a camera: the whole 3D pipeline on an empty view (anti-aliasing, upscaler, tonemap) |
| `s1_env.tscn` | the theme's `WorldEnvironment` and camera attributes: sky, SSAO, glow, volumetric fog, colour grade, and SSIL or depth of field where the theme has them |
| `s2_stone.tscn` … `s6_fog.tscn` | the backdrop prefab, more of it visible at each step: stone, then lights and the lightmap, water, fire, fog and dust |
| `res://scenes/bench/tuner.tscn` | the board demo ([Tune rendering live](tune-rendering-live.md)) |
| `res://scenes/bench/game_tuner.tscn` | the game with its menus |

A step that costs more than its content should (the stone at 60 ms
instead of 11 on the M4) points at something other than the shader: in
that case the generated textures had been imported with Godot's defaults
(no mipmaps, no VRAM compression). `tools/assets/gen_theme_textures.py`
writes the `.import` files from `tools/assets/texture_import.py`; run it
again, then `godot --headless --path game --import`.

## Find which effect of the environment costs

The `v1_*` scenes are `s1_env` with one effect of the theme's environment
off each (`v1_dof_off`, `v1_ssil_off`, `v1_ssao_off`, `v1_glow_off`,
`v1_lut_off`, `v1_vfog_off`), `v1_same` with nothing changed (the
control) and `v1_bare` with all of them off (the floor). Run `v1_same`
first and again last; a difference between those two runs is the noise
of the method, and a `v1_*_off` result inside it means nothing.

With `--gpu-profile` the readout's pass list gives the same answer more
directly: an effect's own pass (depth of field, SSIL, SSAO, glow,
tonemap) with its time in milliseconds.

## Find which project setting costs

1. Write the setting under test, and only that, to `game/override.cfg`.
   Godot reads it over `project.godot`; the keys and values are those of
   the `[rendering]` and `[display]` sections of `game/project.godot`:

   ```sh
   printf '[rendering]\nanti_aliasing/quality/screen_space_aa=0\n' > game/override.cfg
   ```

2. Run the scene the setting acts on (`s1_env.tscn` for most; a
   per-platform override such as `scaling_3d/mode.macos` needs the key
   with its feature tag) and read the last printout. The anti-aliasing
   line of the readout confirms that the override took.
3. Compare with the same scene without the file, run right before or
   after; then remove the file:

   ```sh
   rm game/override.cfg
   ```

Settings that cost nothing measurable on one machine (3D MSAA, VRS, the
thread model on the M4) stay as they are; a setting that costs frames
changes in `game/project.godot`, with its measurement in
[Benchmark results](../reference/benchmark-results.md).

## Find what our own code costs

`--tuner-off=<a,b,...>` starts the scene with those custom-rendering
toggles off: a shader name (`SH_TriplanarPBR`, `SH_Water`, `SH_Fire`, …:
a surface shader is replaced by a plain material, an effect shader is
hidden), `effects` (the board's match effects), `flicker` (the brazier
lights' flicker) or `timer` (the level clock). Run the board demo once
with and once without, back to back:

```sh
godot --path game --gpu-profile res://scenes/bench/tuner.tscn -- --start-screen= --tuner-print=10 --tuner-quit=62 --tuner-off=SH_TriplanarPBR
```

## Keep what you found

| What you changed | Where it goes |
|---|---|
| A project setting | `game/project.godot` (a per-platform value through its feature tag), with the numbers in [Benchmark results](../reference/benchmark-results.md) |
| An effect of the theme's environment | the theme: `tools/assets/themes/temple.json` (for example `lighting.ssil` or the `post` section), then `python3 tools/assets/gen_theme_scenes.py` ([Board themes](../reference/board-themes.md#lighting)) |
| A texture import setting | `tools/assets/texture_import.py` and the generator that writes the `.import` |
| A frame-time claim | a run of [the benchmark](run-the-benchmark.md) |

A pipeline compile at start (Godot issue 123481: a Metal pipeline compile
hang on a canvas shader with many texture-array reads, macOS 26, M4 and
M5) is a startup stall, not a per-frame cost: no shader compiles during
a capture once the first frames are past (the benchmark report's
`render.pipeline_compilations.during_capture` counts them).

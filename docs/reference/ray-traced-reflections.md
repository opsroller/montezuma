<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# Ray-traced reflections

*Reference. The optional hardware ray-traced reflections on the board's
pieces (WP-54): how to build and switch them on, what they need, what
they trace, their defaults and what they cost. Why they work this way
and whether they ship: [About ray tracing](../explanation/about-ray-tracing.md).
How to build with them: [Build and test](../how-to/build-and-test.md#build-with-ray-traced-reflections).*

Code: `extension/src/render/rt/` (`rt_math.*` is Godot-free and tested by
`rt_tests`; `rt_reflections.*` is the `RtReflections` compositor effect).
Owner: graphics-programmer.

## Switches

| Switch | Where | Default | Effect |
|---|---|---|---|
| `MONTEZUMA_RAY_TRACING` | CMake option of the extension | `OFF` | `ON` compiles `RtReflections` and its hooks in; `OFF` leaves no trace of them, and the flags below are ignored |
| `--rt-reflections` | user argument (after `--`) | off | attaches the effect where the GPU can run it |
| `--rt-strength=<s>` | user argument | `1.0` | how much of the traced reflection replaces the engine's, 0 to 2; out of range clamps, unparsable keeps 1.0; does nothing without `--rt-reflections` |
| `--rt-debug` | user argument | off | turns the effect on and paints instead of shading: blue for a piece pixel it traced from but whose ray hit nothing (brighter where the material is smoother), green for a hit the frame shows, red for a hit it hides |
| `--rt-selftest` | user argument | off | turns the effect on, rebuilds the board once, checks it after 90 drawn frames, prints each check and quits with 0 or 1; quits with 1 at once where the effect cannot run |
| `GAME_ARGS` | `tools/bench/run_bench.cmake` | empty | passes user arguments such as `--rt-reflections` to the benchmark and readability runs |

No quality tier turns the effect on, and no setting saves it.

## Requirements

The effect attaches only when all of these hold (`rt::should_attach`):

| Condition | Checked with |
|---|---|
| Built with `MONTEZUMA_RAY_TRACING=ON` | the preprocessor |
| `--rt-reflections`, `--rt-debug` or `--rt-selftest` given | `OS::get_cmdline_user_args` |
| Forward+ rendering method | `RenderingServer::get_current_rendering_method() == "forward_plus"` |
| Vulkan driver | `RenderingServer::get_current_rendering_driver_name() == "vulkan"` |
| Ray-tracing pipelines on the GPU | `RenderingDevice::has_feature(SUPPORTS_RAYTRACING_PIPELINE)` |

Godot 4.7.2 offers ray tracing only on its Vulkan driver, never on
Metal (macOS, iOS) or the web. It never enables Vulkan ray queries
(`SUPPORTS_RAY_QUERY` is false on every GPU), so the effect uses a
ray-tracing pipeline. When asked for on a renderer or GPU that cannot
run it, the effect prints one warning naming the rendering method and
driver, for example `RtReflections: --rt-reflections needs Forward+ on
Vulkan with ray-tracing pipelines (mobile, vulkan); running without it`,
and the game runs as without it. On Windows, where Godot may pick
Direct3D 12, add `--rendering-driver vulkan` before `--`.

## What it traces

Each frame, after every other `frame_pre_draw` handler, the effect
gathers the visible geometry under every node in the `ray_traced` group:

| Group member | Set by |
|---|---|
| `Pieces` (each piece's meshes) | `BoardView3D` |
| `PieceBatches` (the piece batcher's multimeshes, one instance per piece) | `BoardView3D` |
| `Slab` (the theme's board: tiles, frame, gold inlay) | `BoardView3D` |

A surface takes part when it is visible, its layer mask is not 0 (the
piece batcher hides the meshes it draws itself that way) and its
material is a `BaseMaterial3D` with transparency off or any
`ShaderMaterial`. Its values are read once per
material:

| Value | From a `BaseMaterial3D` | From a `ShaderMaterial` |
|---|---|---|
| Albedo | `albedo_color` × mean of the albedo texture | `albedo` × mean of `texture_albedo` |
| Roughness, metalness | mean green and blue of the ORM texture, else `roughness` and `metallic` × their textures | mean green and blue of `texture_orm`, else rough and not metal |
| Index of refraction | 1.5 | `ior` when the shader has it, else 1.5 |

Every distinct mesh gets one bottom-level acceleration structure (BLAS)
the first time it is seen. A mesh not gathered for 30 frames (a board
rebuilt for a new level or theme) is freed with its BLAS and buffers. Every frame
the instances go into one top-level acceleration structure (TLAS) of at
most 4096 instances, built for fast building.

## How a pixel is shaded

After transparents, a ray-tracing pass runs one ray generation per
internal-resolution pixel:

1. A ray from the camera to the pixel's depth. When it hits a traced
   surface within 1% of that distance, the pixel is that surface's.
2. If the surface's trace weight is above 0, a mirror ray leaves it
   along the reflected view direction, up to 64 world units.
3. A hit the frame shows (its depth there is within 1% of the hit's
   distance) takes the lit colour of that pixel. A hidden or off-screen
   hit takes the hit surface's albedo × 0.25.
4. The pixel's colour moves toward that colour by Schlick's Fresnel for
   the surface's F0 × its trace weight × strength, clamped to 1 per
   channel. A miss leaves the pixel as the engine drew it, with its
   screen-space reflection and probe.

A compute pass then copies the changed pixels over the frame.

## Defaults

| Name (`rt_math.hpp`) | Value | Meaning |
|---|---|---|
| `kRoughnessCutoff` | 0.5 | trace weight is `(1 − roughness / 0.5)²`, 0 from this roughness up |
| `kDefaultIor` | 1.5 | a dielectric's index when its material names none (F0 4%) |
| `kDefaultStrength` | 1.0 | `--rt-strength` default |
| `kMaxStrength` | 2.0 | `--rt-strength` upper clamp |
| `kOffscreenAmbient` | 0.25 | light on a hit the frame cannot show, × its albedo |
| `kMaxInstances` | 4096 | TLAS capacity; more instances are left out with one warning |
| `kEvictFrames` | 30 | frames a mesh may be missing from the board before it is freed |
| `kDepthTolerance` (`rt_reflections.cpp`) | 0.01 | fraction of distance for "on this surface" and "shown on screen" |
| `kMaxRayDistance` (`rt_reflections.cpp`) | 64 | reflected ray length, world units |

F0 is `((ior − 1) / (ior + 1))²` blended toward the albedo by
metalness: a ruby (ior 1.76) is 7.6%, gold is its albedo.

## Self-test

```sh
godot --path game -- --start-level=e1-l01 --rt-selftest
```

Needs a window on a GPU that can run it (not `--headless`); anywhere
else it prints `FAIL` with the reason and exits 1 at once. At frame 20
it rebuilds every board, as a new level does. After 90 frames it checks:
the GPU is supported; pieces were gathered; an attached effect traced
frames; its TLAS held as many instances as were gathered; it holds
exactly one BLAS per mesh on the board (none left from the old board);
it traced in the last frames. It prints `RtReflections self-test passed` and exits 0, or
`FAILED` and exits 1.

## Benchmark feature key

The benchmark report's `features` gains `rt_reflections` (bool): an
`RtReflections` traced in one of the last three frames. It is `false`
in builds without the option.

## Cost

RTX 4080 SUPER, Vulkan, Forward+, `desktop-high`, exclusive full screen
3840×2160 with no upscaler, vsync off, the 60 s busiest-board replay
after a 10 s warm-up, 2026-10-19, built from this branch:

| Run | GPU p50 | GPU p95 | CPU p50 | CPU p95 | Frame p50 | Frame p95 |
|---|---|---|---|---|---|---|
| Effect off | 8.94 | 9.26 | 0.31 | 0.50 | 9.47 | 10.47 |
| `--rt-reflections` | 9.61 | 9.97 | 0.41 | 0.62 | 10.15 | 11.97 |
| Difference | +0.67 | +0.71 | +0.10 | +0.12 | +0.68 | +1.50 |

75 instances (64 pieces and the board's meshes), 14 BLASes.

With `-DGPU_PROFILE=ON`, 20 s captures run off, on, off: GPU p50 9.06,
9.78, 9.17 ms (+0.66 against the mean of the two off runs), and the
`RT Reflections` pass, which includes the per-frame TLAS build and
uploads, reads 0.69 to 0.70 ms.

Readability
rules 2, 4 and 6 pass with the effect on; rule 5 fails by the same
margin with it off.

## Known limits

- One mirror ray per pixel, no denoising: rough surfaces are left to the
  engine (trace weight 0 from roughness 0.5).
- Hits hidden from the camera are lit by albedo × 0.25, not by the
  scene's lights.
- Only geometry in the `ray_traced` group is traced; stone, water, fire
  and the backdrop are neither reflected nor traced from.
- The first frame a new material or mesh is seen (first board, tier
  switch, theme change) reads its textures and arrays back from the GPU
  on the main thread: a one-off hitch.
- Every compositor `QualityTiers` touches gets its own effect, each
  compiling its pipelines; only the one Godot draws with traces.
- No CI job builds with `MONTEZUMA_RAY_TRACING=ON`.
- Godot 4.7.2's shader reflection does not understand acceleration
  structures: it prints `OpTypeAccelerationStructureKHR is not supported
  yet` once per stage when the shader compiles, and storage buffers read
  only in the hit stage come back empty, so the hit stage only reports
  where it hit and the ray generation stage reads the normals.

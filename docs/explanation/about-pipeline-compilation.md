<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# About pipeline compilation

*Explanation. Why the benchmark scene loads the board through a
`ScenePreloader` with a primer scene, and what that does and does not
fix. Reference for the scene: [Benchmark: Files](../reference/benchmark.md#files).*

## The problem

Godot's rendering device builds a GPU pipeline for every combination of
shader, vertex format and render pass the first time it draws one. On
Metal that compile is slow and runs on the first frame that needs the
pipeline, so a level's first seconds hitch while braziers, gems, water and
the post-processing stack each take their turn. Godot 4.4 and later
precompiles pipelines ahead of the first draw, but only for the rendering
features it has already seen turned on, and only for meshes that are
already loaded
([Godot docs: Pipeline compilations](https://docs.godotengine.org/en/stable/tutorials/performance/pipeline_compilations.html)).

## What we do

`ScenePreloader` (`extension/src/render/scene_preloader.*`) is a node
that loads the scene it is given, and every asset the theme manifest
names, on background threads, then adds the scene as its child and emits
`loaded`:

1. It first adds the primer scene (`game/scenes/render/pipeline_primer.tscn`):
   a `WorldEnvironment` with SSAO, SSIL, SSR, glow and volumetric fog on,
   a camera, a box with a subsurface-scattering material and an omni light
   with shadows. For a frame or two before any asset loads, Godot has seen
   every rendering feature the game uses, so its precompilation covers
   them.
2. It requests the scene and every theme asset with
   `ResourceLoader.load_threaded_request` with sub-threads on, so mesh and
   texture uploads and the pipeline precompilation they trigger run off
   the main thread.
3. When every request has loaded it keeps the resources referenced (later
   synchronous loads are cache hits), frees the primer, adds the scene and
   emits `loaded`. The benchmark report records the time this took as
   `load_seconds`.

A request that fails stops the load with an error naming the path, as the
project's no-fallback rule asks.

## What it does not do

It does not change the cost of a frame once everything is loaded. On the
Mac mini M4 at native 5K the frame is GPU-bound in full-screen passes and
lighting ([Benchmark results](../reference/benchmark-results.md#mac-mini-m4-at-native-5k-project-settings));
no shader compiles happen during a capture, so the warm-up only moves the
first-frame hitch off the measured window. The Metal shader load strategy
(`GODOT_MTL_SHADER_LOAD_STRATEGY=lazy`) and the export shader baker change
when shaders compile, not how long a frame takes.

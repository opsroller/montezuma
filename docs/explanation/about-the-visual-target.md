<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# About the visual target

*Explanation, written as a decision record. Why Montezuma's aims for a
realistic, high-end look, why it gets there with Godot's own renderer
rather than hardware ray tracing, and what that costs. The tiers,
features and pass criteria are in [Visual target](../reference/visual-target.md).*

## Status

Accepted, October 2026. Supersedes nothing; it raises the bar set by the
first-pass temple theme in [Art direction](art-direction.md), which stays
valid for its rules on readability, silhouettes and palette.

## Context

The first temple theme proved the pipeline: generated pieces, board and
backdrop that run on all three renderers. It reads well, but it looks
like a first pass: flat procedural stone, simple lights, particle cards
for fire. The game is going onto current PCs, Macs, phones and tablets,
and nothing is published until it looks good. A match-3 game is judged
in the first five seconds by how its board looks in a store screenshot
and a trailer, so the look is a product requirement, not polish.

The original's appeal was a warm temple and a board you read instantly.
Any new look has to keep that: the camera stays nearly top-down and the
eye must still find a match in a fraction of a second.

## Decision

1. **Target a realistic, physically based look on current hardware.**
   Physically based materials with real texture detail, fire that looks
   like fire (simulated flame, smoke and embers that light their
   surroundings), bounce light, reflections, volumetric light and a filmic
   grade.
2. **Use stock Godot, upgraded to 4.7.** Its Forward+ renderer provides
   real-time GI (VoxelGI, SDFGI), screen-space indirect light (SSIL),
   rewritten Hi-Z screen-space reflections (4.6), volumetric fog with fog
   volumes, GPU particles with trails, turbulence and collision, sub-surface
   scattering, area lights (4.7), the AgX tonemapper with HDR output, TAA,
   FSR 2.2 and MetalFX temporal upscaling. Static stone gets baked
   lighting from `LightmapGI`, which is a GPU path tracer: the light on the
   temple is ray traced, just ahead of time.
3. **Scale by tier, never by camera.** Four tiers (desktop high, desktop
   standard, mobile, web) share one scene and one set of logical asset
   names. Higher tiers add effects; lower tiers swap them for named
   substitutes (baked light for real-time GI, light-shaft cards for
   volumetric fog). Desktop gets larger texture and LOD variants that the
   phone and web exports leave out, so mobile and web keep the project's
   existing size limits. Each tier has a frame budget and a feature list
   that a benchmark checks.
4. **Readability rules are acceptance criteria.** Atmosphere stays off the
   board, transient effects clear within their animation step, and effects
   may not reduce piece-to-tile contrast. Realism that hides a match is a
   bug.
5. **Hardware ray tracing is an optional extra, not the target.** A spike
   (WP-54) tries ray-traced reflections and shadows on desktop through
   Godot 4.7's new ray-tracing API. If it pays for itself it becomes a
   desktop setting; the game never depends on it. The spike built
   reflections and kept them off: subtle from the game's camera for
   0.7 ms ([About ray tracing](about-ray-tracing.md)).

## What is and is not possible with ray tracing

People say "ray traced" to mean "looks like a modern game". Here is
plainly where Godot stands in October 2026:

- **Stock Godot has no built-in ray-traced lighting.** No release ships
  ray-traced GI, reflections or shadows as a renderer feature.
- **Godot 4.7 (June 2026) added the plumbing.** `RenderingDevice` gained
  experimental acceleration structures, ray-tracing pipelines and ray
  queries. It works only on the Vulkan driver, and not on macOS or iOS
  even on capable hardware, and never on the web. Anything built on it is
  our own renderer code, written in C++ in the extension as a compositor
  effect. Godot 4.7.2 never enables the ray queries, so only the
  pipelines work ([About ray tracing](about-ray-tracing.md)).
- **NVIDIA's RTX fork of Godot** (shown at GDC 2026) adds path tracing for
  RTX GPUs. It tracks development snapshots, needs NVIDIA-class hardware
  and would fork our engine away from the mobile, web and macOS builds. We
  do not use it.
- **What we get instead looks the part.** VoxelGI or SDFGI for live bounce
  light, SSIL for contact bounce, SSR plus probes for reflections, a
  path-traced lightmap bake for static light, and volumetric fog lit by
  the sun and the fire. For a small, bounded temple seen from above, that
  is most of what a path tracer would add, at a cost a mid-range GPU can
  hold at 60 fps.

## Alternatives considered

| Option | Why not |
|---|---|
| Keep the first-pass look | Does not meet "nothing goes public until polished"; looks dated next to current match-3 games on the same stores |
| Stylised, non-realistic look | Cheaper and ages well, but not the direction set for this remake |
| Another engine with built-in ray tracing | Throws away the C++ GDExtension work, the web demo path and the GPL-compatible engine license |
| NVIDIA RTX fork | Desktop-NVIDIA-only, unstable base, splits our builds |
| One look for every platform | Either phones fail the frame budget or desktop looks like a phone game |

## Consequences

- The engine moves to Godot 4.7 first (WP-44); every tier feature above
  depends on it.
- Assets get a second pass: higher-detail meshes with LODs, real PBR
  texture sets and fluid-simulated fire flipbooks, still generated by our
  own scripts and simulations so provenance stays simple (WP-48, WP-49,
  WP-55). Desktop budgets rise; mobile and web budgets stay where the
  project profile has them.
- Lighting moves from hand-placed lights to physical units with baked
  lightmaps plus real-time GI (WP-47). Bakes become a pipeline step with
  committed outputs.
- Performance is measured, not guessed: a benchmark scene (WP-46) gates
  each tier, and the quality tier system (WP-45) is the one place where
  features are switched.
- The web demo keeps the Compatibility renderer and the 40 MB limit, so
  it looks noticeably simpler than desktop. That is accepted: it is a
  playable taste that hands off to the full download.
- Store screenshots and trailers are captured on `desktop-high`.

## Sources

Checked October 2026. WP-44 re-checks these against the version it
installs; WP-54 starts from the ray-tracing ones.

- Renderer feature matrix (Forward+, Mobile, Compatibility, including
  `LightmapGI` rendering under Compatibility, SSR, SSIL, SDFGI, volumetric
  fog, TAA, FSR 2.2, HDR output): [Overview of renderers, Godot 4.7 docs](https://docs.godotengine.org/en/4.7/tutorials/rendering/renderers.html)
- `RenderingDevice` ray tracing (`SUPPORTS_RAYTRACING_PIPELINE`,
  `SUPPORTS_RAY_QUERY`: "only supported when using Vulkan … not supported
  on macOS and iOS"): [RenderingDevice class reference, 4.7](https://github.com/godotengine/godot/blob/4.7-stable/doc/classes/RenderingDevice.xml)
  and [godot#99119, Vulkan raytracing plumbing](https://github.com/godotengine/godot/pull/99119)
- 4.7 stable date, ray tracing as API only, `AreaLight3D`, HDR output
  platforms: [Godot 4.7 overview (Ziva)](https://ziva.sh/blogs/godot-4-7),
  [AreaLight3D class reference, 4.7](https://github.com/godotengine/godot/blob/4.7-stable/doc/classes/AreaLight3D.xml),
  [Phoronix on 4.7 beta](https://www.phoronix.com/news/Godot-4.7-Beta)
- Hi-Z SSR rewrite, octahedral reflection probes, glow before tonemapping,
  AgX HDR: [Godot 4.6 release page](https://godotengine.org/releases/4.6/),
  [CG Channel on Godot 4.6](https://www.cgchannel.com/2026/01/discover-5-key-features-for-cg-artists-in-godot-4-6/)
- MetalFX spatial and temporal upscaling modes: [Viewport class reference, 4.7](https://github.com/godotengine/godot/blob/4.7-stable/doc/classes/Viewport.xml)
- Auto exposure Forward+ only: [CameraAttributes class reference, 4.7](https://github.com/godotengine/godot/blob/4.7-stable/doc/classes/CameraAttributes.xml)
- NVIDIA RTX fork of Godot, GDC 2026: [GameFromScratch](https://gamefromscratch.com/nvidia-release-rtx-powered-godot-fork/)


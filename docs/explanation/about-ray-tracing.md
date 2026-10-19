<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# About ray tracing

*Explanation, written as a decision record. What the WP-54 hardware
ray-tracing spike tried, why it is built the way it is, what it showed,
and whether it ships. The switches, defaults and measured cost are in
[Ray-traced reflections](../reference/ray-traced-reflections.md); why
the game's look does not depend on ray tracing is in
[About the visual target](about-the-visual-target.md).*

## Status

Accepted, October 2026: keep the code behind its build option, off by
default; do not ship it in release builds or add it to a quality tier.
Revisit when a Godot release enables ray queries or when a scene gives
reflections more to show.

## Context

- [About the visual target](about-the-visual-target.md) makes hardware
  ray tracing an optional extra: a spike tries it on desktop, and it
  becomes a desktop setting only if it pays for itself.
- Godot 4.7 added acceleration structures, ray-tracing pipelines and ray
  queries to `RenderingDevice`, on Vulkan only. The engine draws nothing
  with them itself: anything ray traced is our own C++ in a compositor
  effect.
- Godot 4.7.2 never asks the Vulkan driver for ray queries, so
  `SUPPORTS_RAY_QUERY` is false even on an RTX 4080. Only ray-tracing
  pipelines (ray generation, miss and hit stages and their shader
  table) work.
- The camera looks down on an 8 × 8 board of separate pieces. Gems are
  transparent and refract the board beneath them; gold suns are opaque
  metal. Screen-space reflections cannot see what lies under or beside
  a piece, so piece edges reflect the probe's view of the temple instead.

## Decision

1. **Reflections, not shadows.** The sun and fire shadows are already
   soft shadow maps; the pieces are what the player looks at, and their
   reflections are what screen space gets wrong.
2. **Trace the pieces and the board only.** Pieces reflect their
   neighbours and the tiles and frame around them, and only pieces and
   the board's smooth gold are traced from. Stone, water, fire and the
   backdrop keep the engine's reflections. A node joins the traced scene
   by being in the `ray_traced` group, so a theme or a mini game can add
   its own without touching the effect.
3. **The ray-traced scene finds its own receivers.** Transparent gems
   write depth but not the normal and roughness buffer, so the effect
   cannot tell a gem pixel from the stone behind it with screen data.
   It casts a ray from the camera to every pixel's depth instead: a hit
   on a traced surface at that distance names the instance, its
   material and its interpolated normal, exact to the pixel.
4. **Shade from the frame where it can.** A reflected hit the camera
   also sees takes the lit colour already in the frame, with the
   engine's lights, GI and fog. Only hidden hits fall back to a flat
   albedo, so no lighting is duplicated in the effect.
5. **Replace, do not add.** The traced colour replaces part of the
   pixel by Fresnel × the material's smoothness, as a mirror reflection
   would replace the probe's. Adding it would double the reflection and
   brighten every gem.
6. **Behind a build option and a flag.** The API is marked experimental
   and needs a GPU most of the target platforms do not have. Off by
   default, nothing about it reaches a release build or a tier.

## What the spike showed

- **It works.** On the RTX 4080 SUPER the effect finds every piece,
  rebuilds the 75-instance scene every frame and traces at native 4K.
- **It is cheap.** +0.67 ms GPU at p50 and +0.71 ms at p95 over the
  busiest-board replay, inside the 1 ms the spike allowed
  ([Benchmark results](../reference/benchmark-results.md#ray-traced-reflections-wp-54)).
- **It is subtle.** From the game's camera most mirror rays leave the
  tops of the pieces upward and miss everything. Only the bevels and
  sides hit something, so the change is a faint band of tile colour and
  neighbouring colour around each piece's rim: closer to the real thing,
  rarely noticed.
- **The API is rough.** Ray queries are not available; the engine's
  shader reflection does not understand acceleration structures and
  loses storage buffers read only in the hit stage. The spike works
  around both.

## Alternatives considered

| Option | Why not |
|---|---|
| Ray-traced sun and fire shadows | The shadow maps already hold the look; traced shadows would cost more rays per pixel for a smaller visible change |
| Ray queries in a compute shader | Not available in Godot 4.7.2 (the driver never enables them) |
| Screen-space data to find gem pixels | Transparent gems are not in the normal-roughness buffer |
| Ship it as a desktop-high setting now | 0.7 ms for a change few players would see, on an experimental API, for one GPU vendor class |
| Drop the code | It costs nothing when not built, measures the API for later, and documents the engine's gaps |

## Consequences

- Release builds and every tier stay as they were; WP-54 never blocks
  another package.
- The benchmark and readability runs can measure the effect at any
  time with `GAME_ARGS=--rt-reflections`.
- The engine gaps (no ray queries, reflection of acceleration
  structures) are written down for the next Godot upgrade to re-check.

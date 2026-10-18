<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# About quality tiers

*Explanation, written as a decision record. How the game picks between
the four quality tiers, why the renderer is fixed per platform, and why
tiers only switch effects off. The tier table and API are in
[Quality tiers](../reference/quality-tiers.md); the look per tier is in
[Visual target](../reference/visual-target.md); why that look, in
[About the visual target](about-the-visual-target.md).*

## Status

Accepted, October 2026 (WP-45). Decided with the graphics-programmer
renderer-decision workflow; costs are estimates until the WP-46
benchmark measures them on the reference hardware.

## Context

The visual target sets four tiers with a feature list each. Something
has to turn that list into engine settings on every platform, pick a
tier on a machine nobody has tested, let the player change it, and give
art (WP-47 to 53) one way to say "this node is desktop only".

## Decision

1. **One renderer per platform, fixed at start.** Forward+ on desktop,
   Mobile on Android and iOS, Compatibility on the web, set by
   `rendering_method` and its `.mobile` override (Compatibility is
   Godot's default for web exports). Godot
   cannot change the rendering method while running, so tiers are grouped
   by renderer: a desktop switches between `desktop-high` and
   `desktop-standard` at runtime; phones and the web have one tier each.
   A desktop whose GPU cannot run Forward+ and that Godot falls back to
   Compatibility gets the `web` tier, `lo/` textures included. That is
   intended, not a misdetection: it is the same renderer and the same
   capability as the web build, so the web tier's settings are the ones
   that fit.
2. **Pick the desktop tier from the adapter.** Discrete GPUs and Apple
   M-series Pro, Max and Ultra chips start on `desktop-high`; integrated,
   software and unknown adapters on `desktop-standard`. A wrong guess
   costs one trip to the settings screen, and the cheaper tier is the
   safe default: a too-plain image is better than a game that misses
   60 fps.
3. **Tiers subtract, themes add.** A theme authors its full
   `desktop-high` environment and content; each tier switches off what it
   lacks and shows the named substitute instead. A tier never turns on an
   effect the theme left off, so a theme that does not use SDFGI or fog
   never gets it, and readability rule 2 (no atmosphere over the board)
   stays the theme's to keep.
4. **Content opts in by group, not code.** Nodes join `quality:<id>` or
   `quality_fallback:<id>`; brazier lights join `quality_shadow_light`.
   Artists tag scenes in the editor; no per-theme C++.
5. **The tier table is plain C++ with tests.** It holds no Godot types,
   so CI and the sanitizers check it on every platform; the Godot node
   only applies it.

## Alternatives considered

| Option | Why not |
|---|---|
| Let desktop players pick `mobile` or `web` | Needs a restart into another renderer and a second settings path; `desktop-standard` already covers integrated GPUs |
| Tiers force effects on | Turns on SDFGI or fog in a theme that never meant to use them, and can put fog over the board |
| Detect by benchmark at first start | Slow first launch and noisy on laptops; WP-46 measures per tier instead, and the player can override |
| One global Low/Medium/High switch per effect in the UI | More settings than a match-3 player wants; the visual target defines tiers as the unit the benchmark checks |
| Mobile renders 3D at 0.85 with FSR 1 | FSR 1 and 2.2 are Forward+ only in Godot 4.7; bilinear below 1.0 blurs the pieces, so mobile starts at 1.0 with 2× MSAA |

## Costs

Estimated GPU cost of the switched effects on each tier's weakest
device, from Godot's documentation and published measurements, not yet
measured on our scenes:

| Tier | Switched effects (estimate) | Budget |
|---|---|---|
| `desktop-high` (RTX 3060, 1440p) | VoxelGI 1.5 to 2.5 ms, SSIL 1, SSAO 0.8, SSR 1.5, volumetric fog 1.5, sun and 8 light shadows 4.4, TAA 0.5: about 11 to 12 ms | 16.6 ms |
| `desktop-standard` (Radeon 680M, 0.77 scale) | SSIL 1.5, SSAO 1, SSR half 1.2, fog 0.8, shadows 1.6, FSR 2.2 1: about 7 ms | 16.6 ms |
| `mobile` (Pixel 7a) | sun shadow 1, 2× MSAA resolved on-tile 0.5: about 1.5 ms | 16.6 ms |
| `web` (UHD 620) | sun shadow 0.6, 2× MSAA 1: about 1.6 ms | 33.3 ms p95 |

The rest of each budget is the board, backdrop, particles and the game's
own CPU work. Measured p50 and p95 from the WP-46 benchmark are in
[Benchmark results](../reference/benchmark-results.md); WP-41 moves any
tier setting the numbers say to move.

## Consequences

- WP-47 to 53 tag their nodes with the groups above and author their
  environments for `desktop-high`.
- The settings screen adds one option, `quality_tier`, shown only on
  desktop.
- The benchmark (WP-46) runs each tier with `--quality=` and records
  `get_active_features()`.
- A new tier means a new row in `quality.cpp`, its tests and the tables
  in the reference and visual target.

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# About quality settings

*Explanation, written as a decision record. Why the game has no quality
tier system of its own and runs on Godot's project settings alone, what
the temple theme's effects cost at native 5K, and why the look stays as
authored. The settings themselves are in `game/project.godot`; the look
the game aims for per platform class is in
[Visual target](../reference/visual-target.md); the measurements are in
[Benchmark results](../reference/benchmark-results.md#mac-mini-m4-at-native-5k-project-settings).*

## Status

Accepted, October 2026. Replaces the quality tier system of WP-45 and
WP-57 (a `QualityTiers` autoload that picked a tier from the GPU, switched
effects, node groups and material variants, and applied its own render
settings per tier).

## Context

The tier system existed to fit four feature lists onto four platform
classes. Measuring the game on the Mac mini M4 at native 5120×2880 with
the [render tuning screen](../reference/render-tuner.md) and the
[yardstick scenes](../how-to/measure-a-frame-step-by-step.md) showed that
the frame's cost was not in what the tiers switched. A camera alone cost
31 fps at 5K with the project's anti-aliasing on (187 fps with it off);
the theme's environment alone brought that to 9.5 fps. The tier system
carried its own copies of those decisions (per-tier render settings,
`hi/` and `lo/` material folders, feature overrides) on top of the
project settings Godot already has, and every measurement had to be
made twice: once through the tiers and once through the settings.

## Decision

1. **Quality is Godot's project settings.** `game/project.godot` holds
   the render settings (anti-aliasing, scaling, shadow filters, SSAO and
   SSIL quality, VRS, HDR output); per-platform differences use Godot's
   feature tags (`.mobile`, `.macos`, `.ios`, `.web` overrides), not our
   code. The rendering method per platform is one of those settings:
   Forward+ on desktop, Compatibility under the `mobile` tag.
2. **One set of materials.** `game/assets/materials/<theme>/` is the
   only material folder; the generator writes each material once, with
   the keys a theme's `hi` block adds merged in. There are no `hi/` or
   `lo/` material folders and no runtime variant swap.
3. **Themes author what they want, and keep it.** A theme that turns an
   effect on gets it everywhere its renderer can draw it; nothing turns
   it off per tier. The temple keeps its far depth of field
   (`post.depth_of_field`), its screen-space indirect light
   (`lighting.ssil`, default on) and subsurface scattering on jade,
   moonstone and carnelian
   ([Board themes](../reference/board-themes.md#post-processing)).
4. **Cost is measured with the settings, not with switches.** A setting
   under test goes in a temporary `game/override.cfg`
   ([Measure what a frame costs](../how-to/measure-a-frame-step-by-step.md)),
   so `project.godot` only changes when a number says so.

## What the effects cost, and why they stay

The 5K measurements on the M4 (Metal, Forward+, Godot 4.8 beta 1,
exclusive full screen, vsync off) put a number on each effect: depth of
field cost 37 to 42 ms a frame and SSIL 29 to 32 ms. As experiments,
taking depth of field out of the theme took the board demo from 5.4 to
6.8 fps, and taking SSIL out as well to 9.1 fps; the opaque pass that
remained was mostly the lights (the four brazier lights and the sun's
soft shadow), and the Mobile renderer, which drops those effects itself,
ran the same scene at 19.4 fps. The numbers are in
[Benchmark results](../reference/benchmark-results.md#mac-mini-m4-at-native-5k-project-settings).

The owner's decision is that the look stays: the temple is authored with
these effects and ships with them. Native 5K at 60 fps on an M4 is not a
target of the [visual target](../reference/visual-target.md#quality-tiers)
(`desktop-high` is 1440p output with the 3D at 720 lines, upscaled), and
a player who wants more frames on a 5K display has Godot's scaling and
anti-aliasing settings, which the measurements show matter as much as
the effects. The experiments stand as measurements, not as the shipped
configuration.

## The one material change: no transmittance

Godot 4.8 beta 1's Forward Mobile renderer generates a scene shader for
a material with subsurface transmittance on that it cannot compile
(`light_compute: no matching overloaded function found`, then
`missing #endif`, then a fatal empty pipeline list), instead of
stripping the feature as it does for subsurface scattering itself, which
it only warns about. So `gen_theme_scenes.py` writes transmittance only
when a theme's `subsurface` block has a `transmittance` colour, and the
temple's blocks have only `strength`
([Board themes](../reference/board-themes.md#parameter-file)). With that,
every material of ours compiles on Forward+, Mobile and Compatibility.
The scattering itself stays on.

## What the tier system did that nothing does now

The tier system also switched content per renderer: no fog volume and
cube-map omni shadows on Compatibility, fewer particles on mobile and
web. With one material set and one scene, the Compatibility renderer
now logs that the fog volume's shader is unsupported, and the braziers
were moved to cube-map shadows for every renderer. A replacement that is
Godot's own would be per-feature-tag entries in the asset manifest (as
`run/main_scene.web` already picks the web demo's scene); that is a
work package of its own, not part of the change that removed the tiers.

## Consequences

- The settings screen has no graphics quality row.
- The benchmark has no `--quality*` arguments and no feature check
  against a tier table; its `tier` is a label for the budget to judge
  against ([Benchmark](../reference/benchmark.md#options)).
- The visual target's tier tables describe targets and budgets per
  platform class, not runtime modes.
- A new per-platform setting is a feature-tag override in
  `game/project.godot`, measured first with `override.cfg`.
- Reproducing a 5K experiment means editing the theme file and
  regenerating, not flipping a switch; the how-to says where
  ([Measure what a frame costs](../how-to/measure-a-frame-step-by-step.md)).

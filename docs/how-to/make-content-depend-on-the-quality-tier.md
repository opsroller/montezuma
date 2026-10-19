<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Make content depend on the quality tier

Use this when a node, light or texture in a theme should only appear on
some quality tiers, or should be swapped for a cheaper stand-in on the
others. The ids and groups are listed in [Quality tiers](../reference/quality-tiers.md).

## Show a node only on tiers that have a feature

1. Pick the feature id from the [feature table](../reference/quality-tiers.md#feature-ids),
   for example `heat_haze` for a heat-distortion quad above a brazier.
2. In the Godot editor select the node, open **Node > Groups** and add
   the group `quality:heat_haze`. In a `.tscn` file this is
   `groups=["quality:heat_haze"]` on the node line.

The node is hidden on `mobile` and `web` and shown on the desktop tiers.

## Add the cheaper stand-in

Put the substitute next to it in the group `quality_fallback:<id>`. For
sun shafts: the temple's light-shaft cards go in
`quality_fallback:volumetric_fog`, so they show only where the tier has
no volumetric fog.

## Limit fire-light shadows

1. Give each brazier light the shadow it should have on `desktop-high`.
2. Add it to the group `quality_shadow_light`.

The tier keeps shadows on the nearest lights to the camera (8, 2, 0, 0
from `desktop-high` to `web`) and turns the rest off. Lights that light
the volumetric fog also go in `quality_fog_light`.

## Author the environment once

Turn on everything `desktop-high` should have in the theme's
`WorldEnvironment` (SSAO, SSIL, SSR, volumetric fog, glow) and its
`CameraAttributesPractical` (auto exposure, backdrop depth of field).
Lower tiers switch off what they lack; they never turn on what the
theme left off.

## Ship a tier variant of a texture

1. Put the desktop set in a `hi/` folder next to the base set, or the web
   set in `lo/`: `textures/temple/hi/stone_albedo.png` beside
   `textures/temple/stone_albedo.png`.
2. Load it through the tier:

   ```cpp
   #include "render/quality_tiers.hpp"

   using montezuma::godot_binding::QualityTiers;

   const String path = QualityTiers::get_singleton()->resolve_variant(
       "res://assets/textures/temple/stone_albedo.png");
   ```

When the variant file is missing the base path comes back, so a theme
works before its variants exist.

## Ship a tier variant of a material

1. Put the desktop material in a `hi/` folder next to the base one, with
   the same file name: `materials/temple/hi/MI_Gold.tres` beside
   `materials/temple/MI_Gold.tres`.
2. Keep naming the base material in prefabs, meshes and the manifest.

`QualityTiers` swaps it in on every mesh on desktop and back on mobile
and web, including after a runtime tier change; no code is needed. On
particles, give the effect one material (in `material_override` or on
every draw-pass surface), and keep it in its own `.tres` file, not
embedded in the scene. A loader outside `PieceLibrary` calls
`preload_variants(scene)` when it loads a scene, so the desktop textures
load then and not in the frame the first copy appears.

## Check it on every tier

```sh
godot --path game -- --quality=desktop-standard
godot --path game --rendering-method mobile
godot --path game --rendering-method gl_compatibility
godot --headless --path game --quit-after 120 -- --quality-selftest
```

`--quality=` picks a desktop tier without saving it; the renderer flag
starts the mobile or web tier. The self-test must print `0 failures`.

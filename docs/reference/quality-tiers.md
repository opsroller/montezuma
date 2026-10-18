<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Quality tiers

Reference for the quality tier system: the `QualityTiers` autoload, the
settings key, the feature ids, the node groups content uses and the
exact render settings each tier applies. For people building scenes,
settings UI or benchmarks against it. The look each tier must show is in
[Visual target](visual-target.md); why the system works this way is in
[About quality tiers](../explanation/about-quality-tiers.md); how to tag
content is in [Make content depend on the quality tier](../how-to/make-content-depend-on-the-quality-tier.md).

Code: `extension/src/render/` (`quality.*` is the Godot-free tier table,
tested by `quality_tests`; `quality_tiers.*` applies it). Scene:
`game/scenes/render/quality_tiers.tscn`, the first autoload in
`game/project.godot`. Owner: graphics-programmer (WP-45).

## Tiers and renderers

| Tier id | Rendering method | Picked automatically when |
|---|---|---|
| `desktop-high` | Forward+ (`forward_plus`) | the GPU reports a discrete adapter, or its name is an Apple M-series Pro, Max or Ultra |
| `desktop-standard` | Forward+ | any other Forward+ adapter: integrated (Iris Xe, Radeon 680M, Steam Deck, Apple M1), virtual, software or unknown |
| `mobile` | Mobile (`mobile`) | the game runs on the Mobile renderer (Android, iOS) |
| `web` | Compatibility (`gl_compatibility`) | the game runs on Compatibility (the web export) |

The rendering method is set per platform in `game/project.godot`
(`rendering/renderer/rendering_method` and its `.mobile` override; the
web export uses Compatibility, Godot's `.web` default) and is fixed for
the whole run. A desktop whose GPU has no Vulkan, D3D12 or Metal and
falls back to Compatibility runs the `web` tier. A tier is available only when
its method is the running one, so a desktop offers the two desktop tiers,
and phones and the web each offer one.

## `QualityTiers`

Autoload `/root/QualityTiers`; from C++ `QualityTiers::get_singleton()`
(`#include "render/quality_tiers.hpp"`).

| Member | Returns | Meaning |
|---|---|---|
| `get_choice()` | String | The saved setting: `auto` or a tier id |
| `set_choice(choice)` | — | Saves the setting to `user://quality.cfg` and applies it at once. A tier the running method cannot show is refused with a warning and nothing changes |
| `get_tier()` | String | The tier in effect |
| `get_detected_tier()` | String | The tier `auto` picks on this machine |
| `get_available_tiers()` | PackedStringArray | Tier ids `set_choice` accepts here, highest first |
| `has_feature(id)` | bool | Whether the active tier has a feature (ids below) |
| `get_active_features()` | PackedStringArray | Feature ids of the active tier, for the benchmark's feature list |
| `resolve_variant(path)` | String | The active tier's asset variant, `dir/hi/file` on desktop and `dir/lo/file` on web, or `path` itself when that file does not exist or the tier has none (mobile) |
| `apply_to(node)` | — | Re-applies the tier to a subtree, materials included; nodes entering the tree are handled already. Call it after giving a node in the tree a new mesh |
| `preload_variants(scene)` | — | Loads the tier's variants of every material in a `PackedScene` ahead of use, so its first copy entering the tree only looks them up. `PieceLibrary` calls it for each manifest entry when a theme loads |
| signal `tier_changed(tier)` | — | After a runtime switch is applied |

### Material variants

Every `GeometryInstance3D` shows the active tier's variant of each
material loaded from a file, swapped as the node enters the tree and
again on every tier switch:

| Slot | Swapped |
|---|---|
| `material_override`, `material_overlay` | yes |
| `MeshInstance3D` surface override | yes |
| A mesh's own surface material (no override) | yes, the variant is set as the surface override; it follows a new mesh on re-entry or `apply_to` |
| `CSGPrimitive3D.material` | yes |
| `CPUParticles3D` mesh, `GPUParticles3D` draw passes, `MultiMeshInstance3D` mesh | when all their surfaces share one material, its variant is set as `material_override`; meshes with several materials keep the base |
| A material made in code or embedded in a scene (`res://x.tscn::id`) | no, it has no file to vary |
| `next_pass` chains, `ParticleProcessMaterial`, sky, fog volumes, decals | no |

The variant is `resolve_variant` of the base path, so a `hi/` material
turns back into its base on `mobile` and `web`, and a material with no
variant file stays the base. Content loads the base path; the board
view, the benchmark and look-dev scenes need no code for it. Overrides
`QualityTiers` set itself are recorded in the node's
`_quality_variant` meta, so they are told apart from content's own. A
folder named `hi` or `lo` directly above a file is always read as a
variant folder. Each resolved path is cached, keeping its material
loaded, until the tier changes. A material set on a node already in the
tree is swapped at the next `apply_to` or tier change.

Cost (measured headless with the dummy renderer on a cloud CPU, so an
estimate for real devices): no GPU cost of its own; the `hi/` materials'
cost is the [benchmark](benchmark.md)'s to measure. A pooled piece
re-entering the tree adds about 1 µs; a new copy on a desktop tier 6 to
40 µs; a tier switch about 1 ms with one board and up to 5 ms with 700
pieces in the tree. Loading a variant
takes up to a few hundred ms for 2048² sets, which is why loaders call
`preload_variants` when a theme loads; the self-test fails when a
preloaded piece's first copy takes 1 ms or more to enter the tree.
Which materials have variants: [Board themes](board-themes.md#quality-tier-variants).

### Settings key

The settings screen's **Graphics quality** row (`%Quality` in
`game/ui/screens/settings/settings.tscn`) is the key **`quality_tier`**:
an option list of `auto` plus `get_available_tiers()`, labelled with the
strings `settings.quality.<id>`, read with `get_choice()` and written with
`set_choice()`, so a change applies at once. It is stored by
`QualityTiers` in `user://quality.cfg` (`[quality] tier=...`), not in the
UI's settings file. The row is hidden when `get_available_tiers()` has
one entry (phones, web).

### Command line

| Argument (after `--`) | Effect |
|---|---|
| `--quality=<tier id or auto>` | Uses that tier for this run without saving it (benchmark captures) |
| `--quality-selftest` | Switches through every available tier on scratch nodes, checks each switch (environment, groups, shadow budget and material variants of `MI_Gold` and `MI_Lapis`, also after a node leaves and re-enters the tree), prints `QualityTiers self-test: N checks, F failures` |

## Feature ids

One id per row of the visual target that the system switches. ✔ on, —
off. The ✔ rows a tier lacks fall back to the substitute the visual
target names.

| Id | Switches | `desktop-high` | `desktop-standard` | `mobile` | `web` |
|---|---|---|---|---|---|
| `ssao` | `Environment.ssao_enabled` | ✔ | ✔ | — | — |
| `ssil` | `Environment.ssil_enabled` | ✔ | ✔ | — | — |
| `ssr` | `Environment.ssr_enabled` (half size on standard) | ✔ | ✔ | — | — |
| `sdfgi` | `Environment.sdfgi_enabled` (only if WP-47 picks SDFGI over VoxelGI) | ✔ | — | — | — |
| `voxel_gi` | nodes in `quality:voxel_gi` | ✔ | — | — | — |
| `volumetric_fog` | `Environment.volumetric_fog_enabled` | ✔ | ✔ | — | — |
| `fog_fire_light` | volumetric fog energy of lights in `quality_fog_light` | ✔ | — | — | — |
| `auto_exposure` | `CameraAttributes.auto_exposure_enabled` | ✔ | ✔ | — | — |
| `depth_of_field` | `CameraAttributesPractical` far and near blur | ✔ | ✔ | — | — |
| `glow` | `Environment.glow_enabled` | ✔ | ✔ | ✔ | ✔ |
| `subsurface_scattering` | nodes in `quality:subsurface_scattering`; SSS quality high, medium, off | ✔ | ✔ | — | — |
| `parallax_occlusion` | nodes in `quality:parallax_occlusion` | ✔ | ✔ | — | — |
| `gem_refraction` | nodes in `quality:gem_refraction` | ✔ | ✔ | — | — |
| `water_refraction` | the `hi/MI_Water` variant (`SH_Water`, refraction and screen-space reflections) on the water strips; mobile and web keep `MI_Water` ([Water](water.md#tiers)) | ✔ | ✔ | — | — |
| `heat_haze` | nodes in `quality:heat_haze` | ✔ | ✔ | — | — |
| `decals` | nodes in `quality:decals` | ✔ | ✔ | ✔ | — |
| `area_light` | nodes in `quality:area_light` | ✔ | — | — | — |
| `particle_trails` | nodes in `quality:particle_trails`; on a `GPUParticles3D` in that group it also sets `trail_enabled` and, where trails are on, makes `metadata/trail_mesh` its first draw pass. Author trails off and the trail mesh in that metadata: the Compatibility renderer warns about any particle system with trails or a trail mesh, hidden or not | ✔ | ✔ | ✔ | — |
| `fire_flipbook_8x8` | nodes in `quality:fire_flipbook_8x8` (4×4 in the fallback group) | ✔ | ✔ | — | — |

## Node groups

| Group | On a | Effect |
|---|---|---|
| `quality:<feature id>` | any node with `visible` | shown only where the tier has the feature |
| `quality_fallback:<feature id>` | any node with `visible` | shown only where the tier lacks it (light-shaft cards, cube-map gems, 4×4 flipbooks) |
| `quality_shadow_light` | `Light3D` | the nearest N to the active camera keep the shadow the scene gave them, the rest lose it |
| `quality_fog_light` | `Light3D` | keeps its authored volumetric fog energy only with `fog_fire_light` |

A group naming an unknown feature id logs a warning and is ignored.

Environment, camera attribute and light switches only turn things off:
an effect is on when the scene authored it on **and** the tier has it.
Themes author their full `desktop-high` look; switching back to a higher
tier restores what the scene authored (kept in the `_quality_authored`
metadata of the resource or light).

## Render settings per tier

Applied to the root viewport and `RenderingServer` at start and on every
switch. The root viewport re-applies on resize.

| Setting | `desktop-high` | `desktop-standard` | `mobile` | `web` |
|---|---|---|---|---|
| 3D scaling | bilinear 1.0 with TAA; above 1440p output: temporal upscaler at 0.67, no TAA | temporal upscaler at 0.77 | bilinear 1.0 | bilinear 1.0 |
| Temporal upscaler | MetalFX temporal on Metal, FSR 2.2 elsewhere | same | — | — |
| MSAA (3D) | off | off | 2× | 2× |
| Anisotropic filtering | 16× | 8× | 4× | 2× |
| Mesh LOD threshold (px) | 1.0 | 1.0 | 2.0 | 2.0 |
| Sun shadow atlas | 4096 | 2048 | 2048 | 1024 |
| Soft shadow filter (sun and positional) | soft high | soft low | soft very low | hard |
| Positional shadow atlas | 4096 | 2048 | 0 (off) | 0 (off) |
| Shadow-casting lights (`quality_shadow_light`) | 8 | 2 | 0 | 0 |
| SSR half size | no | yes | — | — |
| Volumetric fog froxels (size × depth) | 64 × 64 | 32 × 32 | — | — |
| Subsurface scattering quality | high | medium | off | off |
| Texture variant folder | `hi/` | `hi/` | base only | `lo/` |

FSR 1 and FSR 2.2 run only on Forward+ in Godot 4.7, so the Mobile
renderer has no upscaler. Frame-time costs behind these numbers are
estimates until the WP-46 benchmark measures them; see
[About quality tiers](../explanation/about-quality-tiers.md#costs).

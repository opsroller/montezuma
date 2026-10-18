<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Visual target

*Reference. The look Montezuma's ships with, per quality tier, and the
numbers a build must hit to claim a tier. Why this target and what the
engine can and cannot do: [About the visual target](../explanation/about-the-visual-target.md).
The current temple theme is the first pass described in
[Art direction](../explanation/art-direction.md); the work that closes the
gap is listed in [Work packages](work-packages.md#wave-v-high-end-visual-target).*

## The look in one paragraph

A sunlit Aztec temple that looks real on current hardware: physically
based stone, gold and gemstone that hold up close, braziers with real
flame, smoke and embers that light the stone around them, light that
bounces (warm sandstone tints the shadows), reflections in polished
stone and water, shafts of sun through dusty air, and a filmic colour
grade. The camera, the board layout and how fast the board reads stay
exactly as in the original: nearly top-down, every cell visible, pieces
told apart at a glance.

## Engine baseline

| Item | Value |
|---|---|
| Engine for the target | Godot **4.7** stable (upgrade from 4.4.1 in WP-44) |
| Ray tracing in stock Godot 4.7 | Low-level, experimental `RenderingDevice` API only (acceleration structures, ray-tracing pipelines, ray queries), **Vulkan only**, not on macOS or iOS, not on web. No built-in ray-traced GI, reflections or shadows |
| Ray tracing in forks | NVIDIA's RTX fork of Godot (path tracing, RTX GPUs, tracks development snapshots). Not used: not shippable to our mobile, web or macOS targets and not a stable base |
| Ray-traced lighting we do use | `LightmapGI` bakes static lighting with a GPU path tracer, so static stone gets path-traced bounce light on every tier; dynamic light uses real-time GI below |

## Quality tiers

| Tier | Renderer | Platforms | Reference hardware | Resolution | Frame budget |
|---|---|---|---|---|---|
| `desktop-high` | Forward+ | Windows, Linux, macOS | NVIDIA RTX 3060 or AMD RX 6600 XT class (2021 mid-range); Apple M1 Pro class | 2560×1440 output, 3D at 720 lines (0.5) with FSR 2.2 / MetalFX temporal; native with TAA up to 720p output | p95 frame time ≤ 16.6 ms (60 fps) |
| `desktop-standard` | Forward+ | Windows, Linux, macOS, Steam Deck | Steam Deck; Intel Iris Xe / AMD Radeon 680M integrated; Apple M1 | 1280×800 to 1920×1080 output, 3D at 0.77 with FSR 2.2 / MetalFX temporal | p95 ≤ 16.6 ms |
| `mobile` | Mobile | Android, iOS | Pixel 7a / Galaxy A54 class; iPhone 12 | native output, 3D at 0.75 to 1.0 | p95 ≤ 16.6 ms, held for 10 minutes without thermal throttling below 55 fps |
| `web` | Compatibility | Chrome, Firefox, Safari (WebAssembly) | 2020 laptop with Intel UHD 620 class integrated GPU | 1280×720 canvas | p95 ≤ 33.3 ms, median ≤ 16.6 ms; download ≤ 40 MB compressed; first frame ≤ 10 s on broadband |

A higher-end desktop runs `desktop-high` at 4K (3840×2160) with FSR 2.2
Quality; that is a setting, not a separate tier. The tier is chosen at
start from the renderer and GPU and can be changed in the settings menu;
how, and the exact settings each tier applies, are in
[Quality tiers](quality-tiers.md).

## Features per tier

✔ present and required for the tier · ◐ cheaper substitute named in the
cell · — absent

### Lighting and global illumination

| Feature | `desktop-high` | `desktop-standard` | `mobile` | `web` |
|---|---|---|---|---|
| Physical light units, AgX tonemapper, auto exposure | ✔ | ✔ | ✔ (no auto exposure) | ◐ AgX, fixed exposure |
| Baked path-traced static lighting (`LightmapGI`, with probes for moving pieces) | ✔ | ✔ | ✔ | ✔ |
| Real-time bounce light from moving lights (fire flicker, effects) | ✔ `VoxelGI` over the temple, on the pieces (SDFGI only for a theme too big for one volume) | ◐ SSIL | ◐ baked only | ◐ baked only |
| Screen-space indirect light (SSIL) | ✔ | ✔ | — | — |
| Ambient occlusion | ✔ SSAO + baked | ✔ SSAO + baked | ◐ baked | ◐ baked |
| Sun shadows | ✔ soft (PCSS), 4096 | ✔ 2048 | ✔ 2048 | ✔ 1024 |
| Fire light with shadows | ✔ every brazier up to 8, soft, dual paraboloid | ✔ nearest two cast shadows | ◐ no shadows, flicker | ◐ no shadows, flicker |
| Area light for sky fill (`AreaLight3D`) | ✔ | — | — | — |

### Reflections

| Feature | `desktop-high` | `desktop-standard` | `mobile` | `web` |
|---|---|---|---|---|
| Screen-space reflections (Hi-Z SSR, 4.6+) on polished stone, gold and water | ✔ full resolution | ✔ half resolution | — | — |
| Reflection probes (octahedral, 4.6+) | ✔ | ✔ | ✔ | ✔ (2 per mesh) |
| Gem refraction and dispersion (screen texture) | ✔ | ✔ | ◐ cube-map fake | ◐ cube-map fake |

### Materials

| Feature | `desktop-high` | `desktop-standard` | `mobile` | `web` |
|---|---|---|---|---|
| PBR texture sets (base colour, normal, ORM) | ✔ 2048² hero, 1024² rest | ✔ 1024² | ✔ 1024² | ✔ 512² |
| Height / parallax on carved stone | ✔ parallax occlusion | ✔ | ◐ normal only | ◐ normal only |
| Sub-surface scattering (jade, moonstone, foliage) | ✔ | ✔ | — | — |
| Clearcoat, anisotropy (gold) | ✔ | ✔ | ✔ | ✔ |
| Decals (moss, cracks, soot above braziers) | ✔ | ✔ | ✔ | — |

### Fire, water and particles

| Feature | `desktop-high` | `desktop-standard` | `mobile` | `web` |
|---|---|---|---|---|
| Flame: flipbook from our own fluid sim on GPU particles | ✔ 8×8 flipbook, motion vectors | ✔ | ✔ 4×4 | ✔ 4×4 |
| Smoke and embers with turbulence | ✔ trails, SDF collision | ✔ | ✔ trails | ◐ no trails |
| Heat haze (screen distortion) above flames | ✔ | ✔ | — | — |
| Fire lights its smoke and the fog | ✔ (volumetric fog) | — | — | — |
| Water (channels or pool around the platform): normals, refraction, depth colour, caustics | ✔ | ✔ | ◐ no refraction | ◐ no refraction |
| Match, power-up and totem effects on GPU particles | ✔ | ✔ | ✔ | ✔ (no trails) |

### Atmosphere and post-processing

| Feature | `desktop-high` | `desktop-standard` | `mobile` | `web` |
|---|---|---|---|---|
| Volumetric fog with sun shafts and dust (`FogVolume`s outside the board) | ✔ | ◐ low resolution, no fire contribution | ◐ light-shaft cards | ◐ light-shaft cards |
| Depth and height fog | ◐ only where the readability capture passes on every tier; the temple has none (see [About lighting](../explanation/about-lighting.md)) | ◐ same | ◐ same | ◐ same |
| Glow (bloom), before tonemapping | ✔ | ✔ | ✔ | ✔ |
| Colour grade (LUT) | ✔ | ✔ | ✔ | ✔ |
| Depth of field on the backdrop only | ✔ | ✔ | — | — |
| Anti-aliasing | ✔ TAA or FSR 2.2 / MetalFX temporal | ✔ FSR 2.2 / MetalFX temporal | ✔ SMAA or 2× MSAA | ✔ 2× MSAA |
| HDR output | ✔ where the display supports it | ✔ where supported | ✔ where supported (iOS; not Android) | — |
| Motion blur, film grain, chromatic aberration, vignette over the board | — never | — | — | — |

## Readability rules (every tier)

Readability beats realism. These are acceptance criteria, not taste:

1. The camera, its angle and the board framing are those in
   [Board view](board-view.md); no effect moves them.
2. Persistent effects (fog, haze, light shafts, depth-of-field blur, dust)
   have zero density or blur inside the board rectangle.
3. Transient effects (match bursts, power-ups, totems) cover a cell for no
   longer than the animation step they belong to in
   [MoveResult events](move-result.md).
4. With all effects on versus all effects off, the luminance contrast
   between each piece and its tile changes by no more than 10% in a
   look-dev capture of every kind on both tile shades.
5. The seven kinds stay distinguishable in greyscale and under
   protanopia, deuteranopia and tritanopia simulation at the smallest phone
   size (the shape rules in [Art direction](../explanation/art-direction.md)).
6. Bloom never spreads more than a quarter cell beyond an emissive edge on
   the board at default exposure.

## Acceptance tests

A tier is met when all of these pass on its reference hardware:

| Check | How it is measured | Pass |
|---|---|---|
| Frame time | [Benchmark scene](benchmark.md) (WP-46): 60 s scripted replay of the busiest board (four-step cascades, two power-ups, all totems firing) with the braziers lit; GPU and CPU frame time from `RenderingServer` per frame, after 10 s warm-up | p95 within the tier budget above |
| Thermal (mobile) | Benchmark replay looped for 10 minutes | never below 55 fps over any 5 s window |
| Effects present | Benchmark writes the active feature list; `tools/bench/check_features.py` compares it with the tier | every ✔ for the tier on, every — off |
| Readability | [Readability capture](benchmark.md#readability-checks) (`game/scenes/bench/readability.tscn`, `tools/bench/check_readability.py`), rules 2, 4, 5 and 6 above | all pass |
| Look | Side-by-side captures against the look-dev reference frames committed with WP-47 to WP-52, judged by the technical-artist and code reviewer | approved in the PR |
| Fallbacks | Each tier starts on hardware without the higher tier's features without errors or missing materials | clean log |
| Web size and load | Web export (WP-21) | ≤ 40 MB compressed, first frame ≤ 10 s |

## Content budgets per tier

The technical-artist owns these and revises them from benchmark numbers.
The project profile's [budgets](project-profile.md#budgets) (pieces 2k
triangles, textures 1024² max) are the `mobile` and `web` caps and still
bind every asset those builds ship; the larger desktop numbers apply only
to the desktop-only asset variants described below.

| Budget | `desktop-high` | `desktop-standard` | `mobile` | `web` |
|---|---|---|---|---|
| Triangles per piece (LOD0) | 8k | 8k | 2k (LOD1) | 2k (LOD1) |
| Board and frame | 150k | 80k | 20k | 20k |
| Whole visible frame | 2M | 1M | 200k | 150k |
| Draw calls | 2,000 | 1,000 | 300 | 200 |
| Texture memory | 1.5 GB | 768 MB | 256 MB | 128 MB |
| Visible GPU particles | 50k | 20k | 5k | 2k |
| Shadow-casting omni/spot lights | 8 | 2 | 0 | 0 |

## How each tier gets its assets

One scene and one set of logical asset names (the manifest) serve every
tier, but not one set of files. Assets that differ by tier come in
variants, and each export carries only its own:

| What | Desktop (`desktop-high`, `desktop-standard`) | `mobile` | `web` |
|---|---|---|---|
| Textures | `game/assets/textures/<theme>/hi/` 2048² sets, plus the base sets | base 1024² sets only | base sets, or `lo/` 512² sets where the size budget needs them |
| Meshes | LOD0 (desktop budget) to LODn | LOD1 and lower only | LOD1 and lower only |
| Fire flipbooks | 8×8 sheets | 4×4 sheets | 4×4 sheets |
| Bake outputs | lightmaps at desktop texel density | lightmaps at mobile density | lightmaps at mobile density |

- **Selection at run time.** The quality tier system (WP-45) resolves an
  asset path to the variant for the active tier
  (`QualityTiers::resolve_variant`) and falls back to the base variant
  when a tier variant is missing.
- **Stripping at export.** The Android, iOS and web export presets
  exclude the desktop-only folders and LOD files with export
  `exclude_filter`s, so the desktop sets never reach a phone or the web
  build. The desktop presets keep everything.
- **Checked, not trusted.** The asset validator checks that every desktop
  variant has a base variant, and the release checks that the mobile
  install stays under 150 MB and the web demo under 40 MB compressed.


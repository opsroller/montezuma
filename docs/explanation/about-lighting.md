<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# About lighting

*Explanation, written as a decision record. How the board is lit on each
quality tier, which global illumination each tier gets and why, and what
it costs. The nodes and parameters are in
[Board themes: Lighting](../reference/board-themes.md#lighting); the
switches per tier in [Quality tiers](../reference/quality-tiers.md); the
look each tier must show in [Visual target](../reference/visual-target.md#lighting-and-global-illumination).*

## Status

Accepted, October 2026 (WP-47). Revises the lighting rows of the visual
target after the graphics-programmer renderer-decision check. Costs are
estimates from Godot's documentation and published measurements, labelled
as such: the only machine this package could render on is a software
Vulkan driver (lavapipe), which says nothing about GPU cost. The WP-46
benchmark on the reference Mac mini M4 replaces them.

## Context

- The visual target asks for a realistic high-end look on desktop: physical
  light, baked and real-time bounce light, soft fire shadows and a sky
  fill, scaled down per tier.
- The scene is small and bounded: an 8 × 8 board on a platform, four
  braziers and a backdrop, about 31,000 triangles, seen from one camera.
  Everything but the 64 pieces and effects is static.
- The WP-46 benchmark on a Mac mini M4 measured the pre-WP-47 temple at
  p50 23 ms / p95 25.1 ms on `desktop-high` at 1440p (native with TAA)
  against 16.6 ms, and found the depth fog fails readability rules 2
  and 4.
- Godot 4.7.2 has no hardware ray tracing. Its GI options are
  `LightmapGI` (baked in the editor, all renderers), `VoxelGI` (Forward+
  only, baked, re-lit every frame), SDFGI (Forward+ only, real-time,
  cascaded) and SSIL (screen space, Forward+ only).

## Decision

1. **Physical light units and AgX on every tier.** Lux for the sun,
   lumens for the braziers and the sky fill, nits for the sky, and a
   camera sensitivity in ISO (`CameraAttributesPractical`). A theme author
   sets values a lighting reference can check ("a 1900 K flame of 60,000
   lm") instead of energies tuned by eye per scene. AgX handles the bright
   sky and fire without the hue shifts of Filmic. Auto exposure only on
   the desktop tiers (the tier table leaves it off on mobile, and
   Compatibility does not implement it); mobile and web use the theme's
   fixed sensitivity.
2. **`LightmapGI` with probes is the GI every tier shares.** The board and
   backdrop are static, so a path-traced bake carries their bounce light
   and soft occlusion for the cost of one texture sample. The bake's
   probes light the moving pieces on every tier. The sun and braziers stay
   real-time lights (bake mode dynamic): the bake keeps their indirect
   light only, so the flames can flicker and the pieces cast real-time
   shadows. No shadowmask: every tier renders the sun's shadow itself.
3. **`VoxelGI` for real-time bounce on `desktop-high`, not SDFGI.** In
   Godot, a lightmapped surface takes its indirect light from the
   lightmap only; `VoxelGI` and SDFGI reach only the pieces. What the top
   tier adds is bounce from the moving lights (flicker, effects) on the
   pieces. `VoxelGI` fits that: one volume covers the bounded scene
   (14 × 4 × 14 m at 64 cells, about 22 cm a cell), it is baked once
   when the board is built (`VoxelGI.bake()` from C++, under 100 ms on
   a GPU) and re-lit from the dynamic lights every frame. It is the
   item with the least certain cost: on software Vulkan it was the most
   expensive one by far (see [Costs](#costs)), so it is the first real
   feature to switch off if the M4 re-run says so. SDFGI exists for
   large or open scenes; its cascades cost more and resolve the board's
   1 m cells worse. Rule for later themes: `VoxelGI` while the scene fits
   one volume of about 20 m, SDFGI (feature id `sdfgi`) only for a theme
   that does not.
4. **SSIL on both desktop tiers** adds screen-space bounce from moving
   pieces onto the lightmapped stone, which neither GI volume can do.
   Mobile and web keep the baked light only.
5. **Brazier shadows: dual paraboloid, capped by the tier.** Each brazier
   is an `OmniLight3D` in `quality_shadow_light`; the tier keeps shadows
   on the nearest 8 (`desktop-high`), 2 (`desktop-standard`) or none.
   "Every brazier" in the visual target means every brazier up to that
   cap of 8; the temple has four. Dual paraboloid shadows render two
   passes instead of a cube's six; the light is turned so the seam lies
   flat at flame height, above the pieces.
6. **One `AreaLight3D` sky fill on `desktop-high`** (`quality:area_light`,
   not baked), for the soft top light a single sun cannot give.
7. **No environment-wide fog.** Depth fog failed rules 2 and 4. Height fog
   that starts below the platform (`fog_height` −0.6 m) passed on web, but
   in the board view on Forward+ it still left a haze over the board (rule
   2: mean change 11 against a limit of 1 on `desktop-high`). So the
   temple's environment has no fog, and haze in the backdrop comes from
   WP-51's `FogVolume`s, which are kept off the board. A theme can still
   set `lighting.fog` for height fog if its readability capture passes on
   every tier. Measured afterwards at 1280 × 720 (software rendering, so
   look only, not speed), with the WP-49 braziers in: on `web` rule 2
   passes (mean change 0.003) and rule 4 passes (0.9%); on `desktop-high`
   rule 2 passes (mean 0.72, p99 4) and rule 4 passes for the atmosphere
   alone (0.4%), but fails at 12.5% with glow on too, because glow blooms
   the fire and the gold. Glow, and with it rule 6, belongs to WP-52;
   rule 5 to WP-48.
8. **`desktop-high` renders 3D at most 720 lines high.** Above a 720-line
   output the temporal upscaler (MetalFX on Metal, FSR 2.2 elsewhere)
   takes over at 720 / output height, never below 0.5: 0.67 at 1080p, 0.5
   at 1440p and 4K. At native 1440p with TAA the M4 ran p95 25.1 ms before
   this package. A fixed internal height gives every output up to 1440p
   the same shading cost, so one estimate covers them (see
   [Costs](#costs)); 4K at the 0.5 floor shades 2.25 times as many pixels
   and is for machines above the reference class, as the visual target
   says.
9. **Emission in nits, with `lo/` copies for web.** Under physical units
   the RD renderers scale emission by the exposure, Compatibility in Godot
   4.7.2 does not. Base materials carry emission in nits, and each emissive
   material has a `lo/` copy at 1 nit that `QualityTiers` loads on the
   web tier, the same way it loads `hi/` materials on desktop.

## Bake settings

| Setting | Value | Why |
|---|---|---|
| Quality, bounces | medium, 3 | Indirect light converges for a sunny scene; high adds bake time, not visible change |
| Texel density | 0.2 m per texel at import (`generate_lightmap_uv2_texel_size`), `texel_scale` 1.0 | 5 × 5 texels a board cell; the lightmap holds indirect light and occlusion, which are soft, while sharp shadows stay real-time |
| Atlas | one 1024² layer (measured), 0.6 MB EXR plus a 42 KB `.lmbake`; max texture size 2048 | Fits every tier's texture budget; on web it loads as an uncompressed half-float texture of about 8 MB in memory |
| Probes | subdivision 2 | Enough density over the board for the pieces; probes are cheap to sample |
| Camera attributes | the theme's | Required with physical units, or the bake overflows |
| Where | `tools/assets/bake_lightmaps.sh`, editor only (the bake needs RenderingDevice, so it runs on Vulkan, never under Compatibility); the result serves every tier, web included | Godot exposes the bake only through the editor |

## Costs

Estimated GPU cost on each tier's weakest device. Every number is an
estimate unless marked measured; sources: Godot 4.7 documentation
(*Lightmap global illumination*, *Using Voxel global illumination*,
*Environment and post-processing* on SSAO and SSIL, *3D lights and
shadows* performance notes), AMD's FSR 2 performance tables, and the
WP-46 M4 benchmark ([Benchmark results](../reference/benchmark-results.md), PR #54).
Budget: 16.6 ms p95 (web: 33.3 ms), planned to at most about 85%.

| Item | `desktop-high` (M4 / RTX 3060, 720 lines) | `desktop-standard` (Iris Xe / Steam Deck, 0.77) | `mobile` (Pixel 7a) | `web` (UHD 620) |
|---|---|---|---|---|
| Lightmap and probe sampling | 0.1 | 0.1 | 0.2 | 0.3 |
| `VoxelGI` (64 cells, pieces only, dynamic re-light) | 0.6 to 1.0 | — | — | — |
| SSIL (half size) | 0.6 | 1.2 | — | — |
| SSAO | 0.4 | 1.0 | — | — |
| Sun shadow | 1.2 (4096, soft) | 1.0 (2048) | 1.0 (2048) | 0.6 (1024) |
| Brazier shadows (dual paraboloid) | 4 × 0.3 = 1.2 (8 lights: 2.4) | 2 × 0.4 = 0.8 | — | — |
| Brazier lights (shaded in the same pass) | 0.2 | 0.3 | 0.3 | 0.6 |
| Area light | 0.3 | — | — | — |
| Upscaler / TAA | 1.0 | 1.2 | — | — |
| **Lighting total** | **about 5.8 ms** | **about 5.6 ms** | **about 1.5 ms** | **about 1.5 ms** |

**`desktop-high` whole frame.** The M4 measured p95 25.1 ms at native
1440p (3.7 Mpx) before this package, sun shadow and TAA included. Assume
4 to 6 ms of that does not scale with pixels (shadow passes, vertex and
draw work, glow's small mips; an assumption, not measured) and the rest
does. At 720 lines (0.92 Mpx, 25%) the scaling part becomes 4.8 to 5.3 ms.
The desktop `hi/` materials, which the board view now loads (parallax
stone, refracting gems), add an assumed 1 to 2 ms at that size. This
package's items not in the measurement (`VoxelGI` 0.8, SSIL 0.6, SSAO 0.4,
brazier shadows 1.2 and lights 0.2, area light 0.3, softer PCSS sun 0.3,
the upscaler over TAA 0.5) add about 4.3 ms. Central estimate:
4.5 + 5.1 + 1.4 + 4.3 ≈ **15.3 ms, 92% of 16.6 ms** (range about 14 to
17), the same shading at every output from 720p to 1440p plus a little
upscaler and post work that grows with output size. That is over the 85%
planning share, so the cuts below are pre-decided, and the effects the
visual target still adds on this tier (SSR, volumetric fog, depth of
field: WP-50 to 52, about 3 ms) do not fit without them or a lower
internal height.

**`desktop-standard` whole frame.** The M4 ran it at p95 16.87 ms at
1080p × 0.77. The tier's weakest devices (Iris Xe, Steam Deck) have
roughly a third of an M4's GPU, so the same scene would take about 45 to
50 ms there before this package adds about 5.6 ms. That gap predates
WP-47 and lighting cannot close it; it goes to WP-41 with the
measurement (the tier needs a lower scale, cheaper materials or a
narrower device class). This package's first switches there are SSIL off
and brazier shadows from 2 to 0 (about 2 ms).

**Mobile and web** add only lightmap sampling (and on web the brazier
lights) to what WP-46 measures; WP-41 checks them on devices, including
mobile's 10-minute thermal run.

**If the M4 re-run is over budget,** the switches, in order of least
visible loss: area light off, `VoxelGI` off (the probes still light the
pieces), brazier shadows from 8 to 4 lights, then a 640-line internal
height.

**Software Vulkan (measured, not GPU cost).** On lavapipe at 640 × 360,
`desktop-high` took about 1.0 s a frame without `VoxelGI`, 2.9 s with
it at 64 cells and 6.9 s at 128 cells; with SDFGI instead, 1.0 s. That
is the CPU emulating compute shaders and says nothing about a GPU, but it
marks the voxel re-light as the item to measure first, and is why the
volume is 64 cells.

**The reflection probe (measured on the same lavapipe setup, `VoxelGI`
off).** The scene's one box probe, captured once, cost about 35 to 50 ms
of a 356 ms p50 frame. With its cull mask emptied it cost about the same,
so the cost is sampling, not repeated captures. Godot's default
reflection atlas (64 probes) added 500 MB of video memory for that one
probe, so `project.godot` sets the atlas to 8 probes, which brings it to
about 80 MB. Every probe added to a theme takes one of those 8 slots and
is captured on load.

## Options considered

| Option | Why not |
|---|---|
| SDFGI on `desktop-high` | Higher cost for a small scene, coarse at 1 m cells, cascades re-update as the camera shakes; kept as a feature id for open themes |
| `VoxelGI` without a lightmap | No bounce on mobile and web, which cannot run it, and a visible step between tiers |
| Only SSIL for real-time bounce | Misses bounce from off-screen lights, and nothing reaches the pieces' undersides |
| Shadowmask (static sun shadow in the lightmap) | Blurry board shadows at 5 texels a cell, and the pieces would still need real-time shadows |
| Baking at export time in CI from the game, not the editor | `LightmapGI.bake` is editor-only in Godot 4.7; the extension's editor plugin drives the editor's own button instead |
| Committing the bake | Generated binary; built and cached like the textures (no LFS) |
| Depth fog tuned lighter | Fails rule 2 at some aspect ratios however light |
| Height fog below the platform | Passed on web, but left a haze over the board on Forward+ (rule 2 failed on `desktop-high`) |

## Constraints for art

- Board and backdrop are static and lightmapped: their `.obj.import`
  files carry `generate_lightmap_uv2=true` at 0.2 m per texel; a theme's
  lightmap must fit one 2048² layer (`max_texture_size`).
- Pieces and anything else that moves are dynamic GI objects
  (`gi_mode = 2`).
- `VoxelGI` while the scene fits one volume of about 20 m; SDFGI only for
  a theme that does not, and that theme then has to fit its cost.
- Shadow-casting braziers per theme: the budget above is for 4. Up to the
  tier cap of 8 is allowed, at about 0.3 ms each on `desktop-high`.
- Omni lights with shadows are dual paraboloid, turned so the seam lies
  flat above the pieces (`Transform3D(1, 0, 0, 0, 0, -1, 0, 1, 0, …)`).
- Every emissive material gets a `lo/` copy at 1 nit (the scene generator
  writes it); emission is authored in nits for the other tiers.
- No environment fog over the board; backdrop haze is `FogVolume`s outside it (WP-51).

## Open questions

- Auto detection sends a base Apple M4 to `desktop-standard` (only Pro,
  Max and Ultra names count as high), though the M4 is the reference the
  `desktop-high` budget uses. WP-41 decides with the re-run numbers.
- Whether Compatibility leaves emission unexposed was found by rendering
  (pieces came out white on web with nit-scaled emission) and reading
  Godot 4.7.2's `drivers/gles3` scene shader; a Godot release that adds
  it would make the `lo/` copies redundant.

## Consequences

- Every new theme gets `LD_<Theme>_Bake.tscn` and a bake in CI; its board
  and backdrop `.obj.import` files are committed with lightmap UV2.
- Pieces are dynamic GI objects (`gi_mode = 2`); any new moving content
  must be too.
- A theme whose emissive material is new needs its `lo/` copy (the scene
  generator writes it).
- The M4 benchmark re-run after this package confirms or moves the
  `desktop-high` internal height and the switches above.
- Without a bake the board view logs a warning naming the missing file.

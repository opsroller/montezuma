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
- Godot 4.7.2 has no built-in hardware ray-traced lighting (its
  `RenderingDevice` ray-tracing API is plumbing for our own effects; see
  [About ray tracing](about-ray-tracing.md)). Its GI options are
  `LightmapGI` (baked in the editor, all renderers), `VoxelGI` (Forward+
  only, baked, re-lit every frame), SDFGI (Forward+ only, real-time,
  cascaded) and SSIL (screen space, Forward+ only).

## Sun and sky balance

Daylight reads as daylight when the sun is much brighter than the sky
light in shade, which gives dark, hard-edged shadows, and when the two
differ in colour: white-warm sun, cool blue shade. The temple's first
setup had an 11,000 lux, 4500 K sun under a tan sky, plus a warm colour
grade that also pulled the shadows down. The sun was too weak against the
sky to give real shadows, so the scene looked flat, and the tan sky, the
low sun temperature and the grade turned the stone and the mist brown and
the whole frame dark.

The temple now has a 30,000 lux, 5800 K sun; a sky that is blue overhead
and pale blue-grey at the horizon, which lights the shade and the mist;
a lightmap bake that takes that sky at 0.5 of its 15,000 nits; AgX white
8 and contrast 1.5; exposure × 0.8, since the auto range (ISO 400 to 480)
is too narrow to absorb the brighter sun; and a near-neutral grade. With
the light setting the colour, the mist renders white-grey and the water
blue. Check any change to these with before-and-after captures of the
game itself, at the size people play it: the balance shows on the plaza,
the water and the mist around the board, not in a close look-dev frame.

## Decision

1. **Physical light units and AgX on every tier.** Lux for the sun,
   lumens for the braziers and the sky fill, nits for the sky, and a
   camera sensitivity in ISO (`CameraAttributesPractical`). A theme author
   sets values a lighting reference can check ("a 1900 K flame of 90,000
   lm") instead of energies tuned by eye per scene. AgX handles the bright
   sky and fire without the hue shifts of Filmic. Auto exposure only on
   the desktop tiers (the tier table leaves it off on mobile, and
   Compatibility does not implement it); mobile and web use the theme's
   fixed sensitivity. The temple has since turned auto exposure off on
   every tier ([Revision: fixed exposure](#revision-fixed-exposure)).
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
   2, under the luma-difference check of the time: mean change 11 against
   a limit of 1 on `desktop-high`). So the
   temple's environment has no fog, and haze in the backdrop comes from
   WP-51's `FogVolume`s, which leave only a thin, patchy veil over the board. A theme can still
   set `lighting.fog` for height fog if its readability capture passes on
   every tier. Measured afterwards at 1280 × 720 (software rendering, so
   look only, not speed), with the WP-49 braziers in, all under that
   check: on `web` rule 2 passes (mean change 0.003) and rule 4 passes (0.9%); on `desktop-high`
   rule 2 passes (mean 0.72,
   p99 4) and rule 4 passes for the atmosphere
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

## Revision: darker shadows (WP-60)

The first temple lighting read flat: the sky lit every surface about as
much as the sun did. Two fills did it. The `LightmapGI` baked the scene's
full 15,000-nit sky as bounce light, and lightmapped surfaces take their
indirect light only from the lightmap, so lowering the environment's
ambient alone changed nothing on the static stone. The sky fill area light
and the ambient energy added more on top.

So the lightmap now bakes the theme's sky at a reduced energy
(`lighting.lightmap.sky_energy`, 0.3 at the time, 0.5 since the [sun and sky balance](#sun-and-sky-balance) fix), the ambient energy drops from 0.4
to 0.15 and the sky fill from 20,000 to 6,000 lm. The braziers go up to
90,000 lm so their pools of light show against the darker shadows, and
SSAO goes to intensity 3 (power 1.5, light affect 0.3) so the creases
between tiles and pieces darken. On the desktop tiers the auto exposure
target (`auto_scale`) drops from 0.45 to 0.32 so sunlit stone no longer
sits near white. A lower exposure multiplier was tried first (0.7), but
mobile and web have no auto exposure and run at the fixed exposure, and
there the board went so dark that one kind of piece no longer stood out
from its tile (readability rule 4); `auto_scale` reaches only the tiers
that have auto exposure. The colour grade (glow, contrast, saturation) is
WP-52's.

Luminance of the board view at 1920 × 1080 on the first level (darkest
5%, median, brightest 5%), `main` against this change:

| Tier | `main` | WP-60 |
|---|---|---|
| `desktop-high` | 0.31, 0.50, 0.66 | 0.21, 0.41, 0.59 |
| `mobile` | 0.04, 0.17, 0.30 | 0.04, 0.16, 0.28 |
| `web` | 0.22, 0.43, 0.61 | 0.16, 0.42, 0.62 |

Mobile was already the darkest tier and changes least; its brightness is
a separate question for the mobile tier's own pass.

Darker shadows show banding in smooth gradients on 8-bit output, so the
desktop tiers turn on the viewport's debanding, which dithers the 3D
output before it is quantised. Its cost is a few instructions in the
tonemap pass. Web stays off because the Compatibility renderer ignores
the flag. Mobile stays off for now: the Mobile renderer supports it, but
nothing has measured it on the weakest phone the tier targets, whose
frame budget is the tightest, so turning it on is left to that tier's
next measured pass (WP-41).

Lowering the ambient also lowers what the volumetric fog picks up
through its ambient injection, so the ground mist's tuning (#79 and its
veil) is checked against these values.

Readability capture, `main` (with the ground mist) against this change,
RTX 4080 SUPER: `desktop-high` at 3840 × 2160, `mobile` (Mobile renderer)
and `web` (Compatibility) at 1920 × 1080.

| Rule | `desktop-high` | `mobile` | `web` |
|---|---|---|---|
| 2 effects inside the board (mean) | fail, 2.24 → 2.81 | pass, 0.007 → 0.008 | pass, 0.002 → 0.003 |
| 4 piece and tile contrast change | pass, 6.9% → 6.9% | pass, 1.2% → 1.2% | pass, 4.7% → 4.7% |
| 5 closest greyscale pair (light / dark tiles) | fail, 7.2 / 5.3 → 7.5 / 5.3 | fail, 8.9 / 7.1 → 8.9 / 7.1 | fail, 7.5 / 8.5 → 7.5 / 8.5 |
| 6 bloom past a quarter cell | fail, 19.5% → 13.3% | pass, 0.010% → 0.011% | fail, 7.0% → 7.0% |

No rule that passes on `main` fails. Rule 2 on `desktop-high` gets worse
because the mist stands out more against the darker shadows; the mist's
own tuning (the veil) owns that rule.

## Revision: fixed exposure

Auto exposure metered the whole frame, and the frame is mostly sunlit
stone and plaza mist, so it lifted the exposure until the board looked
washed out: bright, with the gem colours greyed. Switched off, the
desktop tiers render at the theme's fixed exposure (ISO 440, multiplier
0.8), the same one mobile and web already used, and the board reads
darker and richer.

Mean of the board view at 1280 × 720 on `desktop-high` (`e1-l01`, frame
150, RTX 4080 SUPER), HSV value and saturation:

| | Brightness | Saturation |
|---|---|---|
| Auto exposure (ISO 400 to 480, `auto_scale` 0.32) | 0.62 | 0.25 |
| Fixed exposure | 0.43 | 0.35 |

Readability on `desktop-high` at 540 × 1170, auto exposure against fixed:
rule 2 worst cell contrast loss 22% to 14% (clear cells 47% to 67%),
rule 4 6.9% to 6.6%, rule 6 0.000% on both. Rule 5 fails on both, as on
`main` before; its closest greyscale pair on shade 1 moves from 8.33 to
6.4 (12.1 needed).

A fixed exposure also means the plaza mist can no longer shift the board
(see [About the atmosphere](about-the-atmosphere.md#how-rule-2-is-measured)),
and every tier now shows the board at one exposure. The quality tiers
keep their `auto_exposure` switch for a theme that wants it: a theme
turns it on with `lighting.exposure.auto`
([Board themes](../reference/board-themes.md#lighting)).

## Bake settings

| Setting | Value | Why |
|---|---|---|
| Quality, bounces | medium, 3 | Indirect light converges for a sunny scene; high adds bake time, not visible change |
| Texel density | 0.2 m per texel at import (`generate_lightmap_uv2_texel_size`), `texel_scale` 1.0 | 5 × 5 texels a board cell; the lightmap holds indirect light and occlusion, which are soft, while sharp shadows stay real-time |
| Atlas | one 1024² layer (measured), 0.6 MB EXR plus a 42 KB `.lmbake`; max texture size 2048 | Fits every tier's texture budget; on web it loads as an uncompressed half-float texture of about 8 MB in memory |
| Probes | subdivision 2 | Enough density over the board for the pieces; probes are cheap to sample |
| Camera attributes | the theme's | Required with physical units, or the bake overflows |
| Sky | the theme's sky as a custom sky at `lighting.lightmap.sky_energy` (temple 0.5) | Lightmapped surfaces take indirect light only from the bake, so the scene's full sky would light every shadow (WP-60) |
| Where | `tools/assets/bake_lightmaps.cmake`, editor only (the bake needs RenderingDevice, so it runs on Vulkan, never under Compatibility); the result serves every tier, web included | Godot exposes the bake only through the editor |

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
- Pieces and anything else that moves stay out of GI (`gi_mode = 0`):
  they receive the lightmap's probes and `VoxelGI`, but never feed them.
  A dynamic GI object (`gi_mode = 2`) is voxelized into `VoxelGI` again on
  every frame; with 64 pieces that more than doubled video memory (711 MB
  to 1467 MB) and stalled every frame on an RTX 4080 SUPER at 1440p, for
  no visible bounce, because lightmapped surfaces take their indirect
  light from the lightmap and SSIL ([numbers](../reference/benchmark-results.md#pieces-out-of-gi)).
- `VoxelGI` while the scene fits one volume of about 20 m; SDFGI only for
  a theme that does not, and that theme then has to fit its cost.
- Shadow-casting braziers per theme: the budget above is for 4. Up to the
  tier cap of 8 is allowed, at about 0.3 ms each on `desktop-high`.
- Omni lights with shadows are dual paraboloid, turned so the seam lies
  flat above the pieces (`Transform3D(1, 0, 0, 0, 0, -1, 0, 1, 0, …)`).
- Every emissive material gets a `lo/` copy at 1 nit (the scene generator
  writes it); emission is authored in nits for the other tiers.
- No environment fog over the board; haze is `FogVolume`s, thin and patchy over the board (WP-51).

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
- Pieces stay out of GI (`gi_mode = 0`); any new moving content must
  too, so its cost does not grow with the number of things on screen.
- A theme whose emissive material is new needs its `lo/` copy (the scene
  generator writes it).
- The M4 re-run with the pieces out of GI keeps the 720-line internal
  height and the switches above: p50 17.5 ms, but p95 18.4 ms against the
  17.5 ms limit, still a miss. The follow-up is the M4's p95 tail
  (hitches and frames that miss a present), not the mean cost
  ([numbers](../reference/benchmark-results.md#pieces-out-of-gi)).
- Without a bake the board view logs a warning naming the missing file.

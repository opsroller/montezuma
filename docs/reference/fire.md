<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Fire

*Reference. The brazier fire (WP-49): the simulation that makes its
flipbooks, the sheets, the scene, the shaders and their parameters, the
flickering light, and what each quality tier shows. Why it is built this
way: [About the fire](../explanation/about-the-fire.md). How to change it:
[Tune the fire](../how-to/tune-the-fire.md). Owner: technical-artist.*

## Files

| Path | What | Committed |
|---|---|---|
| `tools/assets/fx/gen_fire.py` | Simulates the fire and bakes the flipbooks (numpy, Pillow) | yes |
| `tools/assets/fx/fire.json` | Simulation, render and sheet parameters | yes |
| `game/assets/textures/<theme>/fx/T_Fire_Flame.png`, `T_Fire_Motion.png` | 4×4 sheets (mobile, web) | no, build output |
| `game/assets/textures/<theme>/fx/hi/T_Fire_Flame.png`, `T_Fire_Motion.png` | 8×8 sheets (desktop) | no, build output |
| `game/assets/materials/shaders/SH_Fire.gdshader` | Flame | yes |
| `game/assets/materials/shaders/SH_FireSmoke.gdshader` | Smoke | yes |
| `game/assets/materials/shaders/SH_Ember.gdshader`, `SH_EmberTrail.gdshader` | Embers without and with trails | yes |
| `game/assets/materials/shaders/SH_HeatHaze.gdshader` | Heat haze | yes |
| `game/assets/materials/<theme>/MI_Fire.tres`, `MI_FireSmoke.tres` (+ `hi/`), `MI_Ember.tres`, `MI_EmberTrail.tres`, `MI_HeatHaze.tres` | Materials, written by `gen_theme_scenes.py` | yes |
| `game/assets/models/<theme>/fx/fire/PF_BrazierFire.tscn` | One brazier's fire, written by `gen_theme_scenes.py` | yes |
| `game/assets/lookdev/LD_Temple_Fire.tscn`, `LD_Temple_Fire_Hi.tscn` | Close look-dev view of a brazier, base and desktop materials | yes |
| `extension/src/render/flicker.*`, `flicker_light_3d.*` | `FlickerLight3D` and its noise | yes |

`PF_Environment` instances `PF_BrazierFire` as `Fire1` to `Fire4` at the
rim of each corner brazier (`fire.rim_height`).

## Commands

```sh
python3 tools/assets/fx/gen_fire.py                     # default tools/assets/fx/fire.json
python3 tools/assets/gen_theme_scenes.py                # materials and scenes from temple.json
```

`gen_fire.py` hashes its inputs (the script, `fire.json`, numpy and Pillow
versions) into `game/assets/textures/<theme>/fx/.inputs` and does nothing
while the hash and all four outputs match. A fresh run takes about five
minutes on four cores. The CMake target `textures` and the CI action
`.github/actions/textures` run it, cached by the same inputs.

## Simulation (`fire.json`)

A 3D gas on a collocated grid, y up, cells as the length unit and one
simulation frame (`1 / fps` s) as the time unit.

| Key | Meaning | Value |
|---|---|---|
| `seed` | every random choice | 20261018 |
| `grid` | cells x, y, z | 64, 128, 64 |
| `fps`, `substeps` | frame rate of the sheets; solver steps per frame | 30, 2 |
| `warmup_frames` | frames simulated before the first kept one | 60 |
| `frames` | frames kept for the loop | 64 |
| `loop_blend_frames` | frames after the loop cross-faded into its start | 12 |
| `burner` | fuel bed: `radius`, `height`, `y` (cells); `fuel` and `fuel_noise` (amount and how much the smooth noise over the bed varies it, `noise_cells` across); `inflow_speed` (cells/frame); `temperature` of fresh fuel | |
| `combustion` | `burn_rate` (per frame), `ignition` temperature, `heat_release` and `smoke_yield` per unit of fuel burnt | |
| `gas` | `buoyancy` of heat, `smoke_weight`, `cooling`, `smoke_dissipation`, `fuel_dissipation` (per frame), `vorticity` (confinement strength), `turbulence` (weak noise force), `velocity_damping`, `sponge_cells` (absorbing border) | |
| `render.crop` | x0, y0, x1, y1 cells seen side on; aspect is the tile's | 14, 10, 50, 82 |
| `render.tile` | tile size in pixels, width and height | 128, 256 |
| `render.absorption` | soot absorption per cell | 1.6 |
| `render.blur` | blur of the emission image, cells | 1.0 |
| `render.emission_gamma` | encoding of the emission channel (the shader decodes with the same number) | 1.0 |
| `render.temperature_range` | simulated temperatures stored as 0 and 1 | 0.3, 1.0 |
| `sheets.<variant>` | `grid` (tiles per side), `stride` (simulated frames per tile), `motion_scale` (tile UV per sheet frame that motion 1.0 stands for) | `hi`: 8, 1, 0.03; `base`: 4, 4, 0.12 |

Each solver step: inflow over the bed, combustion (hot fuel burns into
heat and soot; burnt fuel per step is the flame's emission), buoyancy,
vorticity confinement and turbulence, MacCormack advection of every field,
an FFT pressure projection, then cooling and dissipation. Each kept frame
is ray-marched along z through the soot.

## Sheets

Linear RGBA8 PNGs, frame `i` at column `i mod grid`, row `i div grid`,
top-left first. The last `loop_blend_frames` of the simulation are
cross-faded into the first, so frame 63 runs into frame 0.

| Sheet | Channel | Holds |
|---|---|---|
| `T_Fire_Flame` | R | emission, normalised to the run's 99.8th percentile, then `^ emission_gamma` |
| | G | emission-weighted temperature, `render.temperature_range` mapped to 0..1 |
| | B | smoke opacity |
| | A | 1 |
| `T_Fire_Motion` | R, G | motion of the content to the next sheet frame, tile UV, `0.5 + m / (2 * motion_scale)`; image y is down, so rising gas is below 0.5 in G |
| | B, A | 0, 1 |

| Variant | Size | Frames | Played at |
|---|---|---|---|
| `hi/` (desktop) | 1024×2048 | 64 (8×8) | 30 fps |
| base (mobile, web) | 512×1024 | 16 (4×4, every 4th frame, emission averaged over the 4) | 7.5 fps |

Both loops last 2.13 s, so the materials share `loop_seconds`.

## `PF_BrazierFire`

Parameters come from the theme's `fire` block (`tools/assets/themes/temple.json`)
and the light's colour, energy and range from `environment.torch`.

| Node | Type | Group | What |
|---|---|---|---|
| `Smoke` | `GPUParticles3D` | | 12 particles, 2.6 s, rising 0.25 to 0.45 m/s from 0.55 m above the rim, growing, with turbulence; `MI_FireSmoke` |
| `Flame` | `GPUParticles3D` | | 10 particles, 1.2 s, almost still, 0.6 × 1.2 m quads based 0.1 m inside the bowl; `MI_Fire` |
| `Embers` | `GPUParticles3D` | `quality:particle_trails` | 18 particles, 1.6 s, 0.6 to 1.3 m/s up within 25°, turbulence, 0.12 s tube trails; `MI_EmberTrail` |
| `EmbersNoTrail` | `GPUParticles3D` | `quality_fallback:particle_trails` | the same embers as dots; `MI_Ember` |
| `HeatHaze` | `MeshInstance3D` | `quality:heat_haze` | 0.9 × 1.1 m quad 0.95 m above the rim; `MI_HeatHaze` |
| `Light` | `FlickerLight3D` | `quality_shadow_light`, `quality_fog_light` | 0.42 m above the rim; shadows on, blur 1.5; volumetric fog energy 2.0; bake mode dynamic |

Every particle system starts full (`preprocess` = lifetime) and casts no
shadow. Visible GPU particles for four fires: 160 counted by the
benchmark (both ember systems count, though one is hidden).

## Shaders

All four billboard their quads in the vertex shader (`skip_vertex_transform`):
the quad faces the camera and its up axis is world up as the camera sees
it, so flames rise out of the bowl from any view.

### `SH_Fire`

Unshaded, additive, no depth write, no fog. Per pixel: two motion
samples, two flame samples (frame and next frame, each displaced along its
motion by the blend fraction, then cross-faded), one depth sample for
soft edges.

| Uniform | Range | Default | Meaning |
|---|---|---|---|
| `flame_sheet`, `motion_sheet` | | | the sheets (data, not `source_color`) |
| `sheet_grid` | 1 to 16 | 4 (8 in `hi/`) | tiles per side |
| `motion_scale` | | 0.12 (0.03 in `hi/`) | must equal the sheet's `motion_scale` in `fire.json` |
| `loop_seconds` | | 2.1333 | loop length |
| `emission_gamma` | | 1.0 | must equal `render.emission_gamma` |
| `temperature_min_k`, `temperature_max_k` | 600 to 2500, 1000 to 4000 | 1100, 1900 | kelvin for the sheet's temperature 0 and 1 |
| `temperature_brightness` | 0 to 4 | 1.5 | brightness grows as (T / max)^(4 × this) |
| `intensity` | 0 to 40 | 10 | HDR brightness |
| `foreshorten` | 0 to 1 | 0.45 | how much a quad shortens when seen from above |
| `soft_distance` | 0 to 1 | 0.12 | metres over which the flame fades into geometry |

Colour is a blackbody at the stored temperature (Planckian locus fit by
Krystek, converted to linear sRGB), times emission, brightness and the
particle colour ramp (fade in over the first 25 % of life, out over the
last 30 %).

### `SH_FireSmoke`

Lit (Lambert wrap, backlight for light from behind), alpha-blended, no
depth write. One flame-sheet sample (B) for the current frame, one depth
sample.

| Uniform | Range | Default |
|---|---|---|
| `flame_sheet`, `sheet_grid`, `loop_seconds` | | as `SH_Fire` |
| `smoke_color` | colour | `#3a3633` |
| `opacity` | 0 to 1 | 0.35 |
| `translucency` | 0 to 1 | 0.5 |
| `soft_distance` | 0 to 2 | 0.3 |

### `SH_Ember`, `SH_EmberTrail`

Unshaded, additive. Colour from the particle ramp: yellow-white, orange,
dull red, gone. `intensity` 0 to 40, default 8. `SH_EmberTrail` uses the
`particle_trails` render mode and fades along the trail.

### `SH_HeatHaze`

Unshaded, blended, reads the screen texture; `render_priority` −1 so it
draws before the flame. Two noise samples, one screen sample.

| Uniform | Range | Default | Meaning |
|---|---|---|---|
| `noise` | | `NoiseTexture2D`, 256², seed 7 | procedural, built at load |
| `strength` | 0 to 0.05 | 0.006 | largest shift, fraction of the screen |
| `speed` | 0 to 4 | 0.8 | rise speed, noise tiles per second |

## `FlickerLight3D`

`OmniLight3D` subclass (C++, `extension/src/render/flicker_light_3d.*`).
At `_ready` it records the energy and position the scene gave it; every
frame it sets energy to `energy × (1 + flicker × n)` with
`n = flicker::fire(t, speed, seed)` in [−1, 1], and moves the light up to
`sway` metres from its rest position on a slower noise. In the editor it
stays as authored.

| Property | Range | Default | Meaning |
|---|---|---|---|
| `flicker` | 0 to 1 | 0.25 (scene: 0.22) | energy swing as a fraction of the authored energy |
| `speed` | 0.5 to 20 Hz | 7 | typical flicker rate; octaves at 0.23, 1, 2.17 and 4.71 × speed |
| `sway` | 0 to 0.2 m | 0.04 | largest drift |
| `seed` | int | 0 | 0: seed from the light's global position, so no two braziers flicker in step |

Shadows, volumetric fog energy and tier switches are handled by
`QualityTiers` through the light's groups, as for any light.

## Per tier

| | `desktop-high` | `desktop-standard` | `mobile` | `web` |
|---|---|---|---|---|
| Flame flipbook | 8×8, motion vectors (`hi/` materials) | 8×8 | 4×4 | 4×4 |
| Smoke | lit, 8×8 | lit, 8×8 | lit, 4×4 | lit, 4×4 |
| Embers | trails | trails | trails | dots, no trails |
| Heat haze | yes | yes | no | no |
| Fire light shadows | all four | nearest two | none | none |
| Fire light in volumetric fog | yes | no | no fog | no fog |

The `hi/` materials reach the game through `QualityTiers::resolve_variant`
like every other desktop material; the look-dev scene `LD_Temple_Fire_Hi`
sets them directly. The web export strips `hi/`, so it never carries the
8×8 sheets.

## Cost

| Item | Number |
|---|---|
| Particles, four fires | 4 × (10 flame + 12 smoke + 18 embers) = 160, budget 2,000 on `web` |
| Draw calls added | 11 on `web` (268 → 279 at the benchmark's p95) |
| Texture memory | two 1024×2048 sheets (`hi/`) and two 512×1024 (base): about 22 MB and 6 MB as RGBA8 with mipmaps, a quarter of that once Godot VRAM-compresses them on import |
| Flame pixel | 4 texture samples + 1 depth sample, about 60 ALU |
| Measured | Benchmark, `web` tier, llvmpipe software renderer: mean GPU frame 868 ms with the fire against 819 ms without (+6 %). Software timings only rank costs; the tier budgets are checked on reference hardware with [Run the benchmark](../how-to/run-the-benchmark.md). |

The Compatibility renderer logs three warnings at start that it does not
support particle trails; they come from the hidden `Embers` node and are
harmless.

## Readability

The braziers stand outside the board (brazier centres 5.5 m from the
middle, board edge 4.55 m) and every effect stays above them. The
readability capture gives the same numbers with and without the fire:
on `web` with llvmpipe, rule 2 mean difference 19.36 against 19.36 on
`main`, rule 4 23.3 % against 23.3 %, rule 6 15 px against 14 px. Rules
2, 4 and 5 fail on `main` already (depth fog and the lapis and amethyst
pair; WP-47, WP-51, WP-52).

## Look-dev frames

Rendered in Godot 4.7.2 at 1280×720, frame 46 at 30 fps, software Vulkan
(lavapipe) and OpenGL (llvmpipe).

| Frame | Scene | Renderer |
|---|---|---|
| [temple-fire-before-wp49.jpg](../images/lookdev/temple-fire-before-wp49.jpg) | close brazier, before WP-49 (mesh flame) | Forward+ |
| [temple-fire-desktop.jpg](../images/lookdev/temple-fire-desktop.jpg) | `LD_Temple_Fire_Hi` | Forward+, `desktop-high` |
| [temple-fire-mobile.jpg](../images/lookdev/temple-fire-mobile.jpg) | `LD_Temple_Fire` | Mobile |
| [temple-fire-compatibility.jpg](../images/lookdev/temple-fire-compatibility.jpg) | `LD_Temple_Fire` | Compatibility, `web` |
| [temple-board-fire-desktop.jpg](../images/lookdev/temple-board-fire-desktop.jpg) | `LD_Temple_Hi`, game camera | Forward+, `desktop-high` |

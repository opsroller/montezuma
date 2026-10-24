<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Board themes

*Reference. The files, names, rules and numbers of a board theme. Why it
looks this way: [Art direction](../explanation/art-direction.md). Making
one: [Make a board theme](../how-to/make-a-board-theme.md).*

## Manifest

`game/assets/manifest.json` (contract: [Asset manifest](contracts.md#asset-manifest-owner-technical-artist-wp-09-11--proposed)):

```json
{
  "default_theme": "temple",
  "themes": {
    "temple": { "artifact_0": "res://assets/models/temple/artifacts/PF_Artifact_Ruby.tscn", "...": "..." }
  }
}
```

| Name | Scene root | What the view does with it |
|---|---|---|
| `artifact_0` … `artifact_6` | `Node3D` | one instance per cell of that kind, origin at the cell centre on the board plane |
| `board` | `Node3D` | replaces the placeholder slab; tile tops at y = 0 |
| `environment` | `Node3D` | added once; brings `WorldEnvironment`, the sun and other lights, and the backdrop |
| `cell_highlight` | `Node3D` | shown at the selected cell |
| `fx_match` | `CPUParticles3D` | one-shot at each cleared cell; freed after `lifetime` |
| `bonus_<power-up id>` | `Node3D` | badge on a piece carrying that power-up; origin at the badge centre, about 0.35 cell across |
| `marker_gem`, `marker_steel`, `marker_chain` | `Node3D` | gem (the view places it at the far-left corner, at piece-top height), steel and chain (origin at the cell centre on the board plane) on a piece |
| `fx_totem_<totem id>`, `fx_power_up_<power-up id>`, `fx_gem_collect`, `fx_steel_break`, `fx_chain_break`, `fx_power_up_placed`, `fx_hint` | `Node3D` | one effect; particles restart when it starts, and it is freed when its beat ends or its particles finish ([Board view, Effects](board-view.md#effects)) |

All values are `res://` paths to `PackedScene`s. A missing name falls back
to the view's placeholder.

## Space and scale

- 1 unit = 1 m = one cell (`cell_size` 1.0). Y up; board `y` grows toward
  the player (+Z); x left to right. Cell (x, y) of a W×H board is at
  `((x - (W-1)/2), 0, (y - (H-1)/2))`.
- Pieces fit in 0.78 × 0.78, bottom at y = 0.02, height ≤ 0.55.
- The board generator builds the board for `board.width` × `board.height`
  cells (temple: 8 × 8).

## Folders and naming

| Path | Contents |
|---|---|
| `tools/assets/themes/<theme>.json` | the theme's parameters: seed, sizes, kinds, materials, environment, look-dev |
| `game/assets/models/<theme>/{artifacts,board,backdrop,fx}/` | `SM_*.obj` meshes and `PF_*.tscn` scenes |
| `game/assets/materials/<theme>/` | `MI_*.tres` materials, one file per material for every renderer |
| `game/assets/materials/shaders/` | `SH_*.gdshader` and `SH_*.gdshaderinc`, shared by all themes |
| `game/assets/textures/<theme>/` | `T_<Set>_{BC,N,ORM,H}.png` base texture sets, at most 1024²; build outputs, ignored by git (see below) |
| `game/assets/textures/<theme>/hi/` | the hero sets at up to 2048², same file names, which the materials with a `hi` block name directly (see [Texture sizes](#texture-sizes)); build outputs too |
| `game/assets/textures/<theme>/grade/` | `T_Grade_LUT.png`, the colour grade as a 3D LUT (see [Colour grade](#colour-grade)); build output too |
| `game/assets/lookdev/` | `LD_*.tscn` look-dev scenes (game camera, line-up) and `LD_<Theme>_Bake.tscn`, the scene each lightmap is baked from |
| `game/assets/models/<theme>/backdrop/PF_Environment.lmbake` and `.exr` | the baked lightmap; build output, ignored by git (see [Lighting](#lighting)) |
| `game/assets/PROVENANCE.md` | one record per theme or asset family |

| Prefix / suffix | Meaning |
|---|---|
| `SM_` | static mesh (OBJ, one material each) |
| `PF_` | asset scene the manifest points at |
| `LD_` | look-dev scene, never shipped from code |
| `MI_` | material |
| `SH_` | shader |
| `T_` + `_BC` | base colour: sRGB, RGB, no light or occlusion baked in |
| `T_` + `_N` | normal: tangent space, OpenGL (green up), RGB |
| `T_` + `_ORM` | occlusion, roughness, metallic in R, G, B: linear |
| `T_` + `_H` | height: linear, greyscale, white is high (parallax) |
| `T_` + `_R` | roughness: linear, greyscale (legacy; new sets pack it in `_ORM`) |
| `T_Grade_LUT` | colour grade lookup table in `grade/`: display-space RGB, imported as a 3D texture |

Names use `A-Z a-z 0-9 _` only. Code refers to logical names in the
manifest, never to these paths.

## Texture sizes

There is one material per surface, used on every renderer; nothing swaps
materials at run time ([About quality settings](../explanation/about-quality-settings.md)).
A texture set with `hi: true` is drawn at `hi_size` (2048²) into
`textures/<theme>/hi/` and box-filtered down to the 1024² base set next
to it. A material whose theme entry has a `hi` block is written with
those keys merged in and names the `hi/` set where the set has one; a
material without a `hi` block names the base set. Every export preset
ships both folders: the materials name what they need.

| Material | What it is |
|---|---|
| Stone (`MI_Stone*`) | `SH_TriplanarPBR`: 2048² `Stone_01` set, triplanar, with parallax occlusion from `_H` |
| Gold (`MI_Gold`) | `ORMMaterial3D`, 2048² `Gold_01`, metallic 1, anisotropy |
| Ruby, amethyst | `SH_Gem`: refraction of the scene behind with dispersion and absorption by the body colour |
| Carnelian, jade, moonstone | `ORMMaterial3D` with sub-surface scattering (no transmittance: see [Parameter file](#parameter-file)); moonstone adds a stronger clearcoat |
| Lapis | `ORMMaterial3D`, 1024² `Lapis_01` (blue, calcite, metallic pyrite flecks) |
| Water (`MI_Water`) | `SH_WaterSimple`: blended, absorption to the flat floor height, caustics, sky reflections | `SH_Water`: opaque; the refracted ray traced through the channel to its stone, absorption, caustics, the engine's reflections ([Water](water.md)) |

Texture filtering: the `ORMMaterial3D` set uses Godot's default trilinear
filter with repeat. `SH_TriplanarPBR` and `SH_Gem` filter anisotropically
(the tier's 2× to 16×) through a clamped sampler and wrap the coordinates
in `SH_Triplanar.gdshaderinc` (`tri_tex`). No material filters
anisotropically with repeat: Godot's Metal backend gives that sampler slot
17, and Metal GPUs without argument buffers (Apple5 family and older, such
as the macOS CI runner) address slots 0 to 15 only, so the material fails
to compile there.

## Generators

Run from the repository root, in this order. Same parameters and seed give
byte-identical files.

| Command | Writes | Needs |
|---|---|---|
| `python3 tools/assets/gen_theme_meshes.py [params]` | `SM_*.obj`, prints triangle counts, fails over budget | Blender 4.2+ as `bpy` (`pip install bpy`) or `blender -b --factory-startup -P <script> -- [params]` |
| `python3 tools/assets/gen_theme_textures.py [params]` | `T_*_{BC,N,ORM,H}.png` (tiling), base and `hi/`, each with its `.import`; `grade/T_Grade_LUT.png` and its `.import` for a theme with a `grade` ([Colour grade](#colour-grade)) | numpy, Pillow |
| `python3 tools/assets/gen_theme_scenes.py [params]` | `MI_*.tres`, `PF_*.tscn`, `LD_*.tscn` (with `LD_<Theme>_Bake.tscn`), the theme's manifest entry, `design/tokens/<theme>-board.json` | standard library |
| `cmake [-DGODOT=PATH] [-DXVFB=ON] [-DFORCE=ON] [-DCHECK=ON [-DCHECK_OUTPUT=FILE]] -P tools/assets/bake_lightmaps.cmake` | each theme's `PF_Environment.lmbake` and `.exr`, and a `.lightmap-inputs` stamp; skips themes whose stamp matches (`CHECK` only lists the stale ones, and with `CHECK_OUTPUT` appends `stale=true` or `stale=false` to that file); the import is stopped after 900 s and the editor after 900 s per scene | the Godot editor from `tools/release/godot.env`, the native extension built, the textures generated, a Vulkan driver (Mesa lavapipe works without a GPU) |
| `python3 tools/assets/validate_assets.py` | nothing; exits 1 on errors | Pillow, numpy (colour grade) |
| `python3 tools/assets/grade_lut.py [params]` | nothing; prints each theme's palette check ([Colour grade](#colour-grade)) | numpy, Pillow |

`params` defaults to `tools/assets/themes/temple.json`
(`gen_theme_textures.py` with no argument does every theme in
`tools/assets/themes/`).

The `T_*` texture sets are build outputs, not committed: git ignores
`game/assets/textures/*/T_*.png`, `*/hi/T_*.png`, `*/grade/T_*.png` and the `.inputs` stamp.
Hand-made textures elsewhere in `game/assets/textures/` are still
committed. `gen_theme_textures.py` writes a theme's `.inputs` stamp, a
SHA-256 of the script, the theme's parameters and the numpy and Pillow
versions, and skips drawing the theme when the stamp matches and every
file exists (it still writes any missing or wrong `.import` file). The
grade LUT takes milliseconds, so it is compared and rewritten on every
run, stamp or not. Three things run it: the CMake target `textures` (part of `all`
when CMake finds Python 3 with numpy and Pillow; otherwise configure
prints how to run it), the CI action `.github/actions/textures` (see
[CI pipeline](ci-pipeline.md#assets-workflow)), and you, before opening
the project in Godot from a fresh checkout.

### Import settings

Git ignores the `.import` files too, so `gen_theme_textures.py` writes
one beside every PNG on every run, from `tools/assets/texture_import.py`,
and leaves alone a file Godot has rewritten as long as it still agrees.
Godot's defaults (no mipmaps; `detect_3d` switching a texture to VRAM
compression the first time the editor sees it in 3D) never apply.

| Parameter | `_BC`, `_ORM`, `_H` (and the legacy `_R`) | `_N` | Why |
|---|---|---|---|
| `compress/mode` | `2` (VRAM compressed) | `2` | GPU formats: S3TC/BPTC on desktop and web, ETC2/ASTC on mobile |
| `compress/high_quality` | `false` | `false` | BC1/BC4 and ETC2, half the memory of BC7/ASTC 4×4 |
| `compress/normal_map` | `2` (off) | `1` (on) | normal maps go two-channel (RGTC/BC5 on desktop and web, EAC RG11 on mobile); the shaders rebuild Z (`tri_unpack`) |
| `mipmaps/generate` | `true` | `true` | the triplanar samplers filter with mipmaps; without them minified tiles alias |
| `detect_3d/compress_to` | `0` (off) | `0` | no silent reimport in the editor |

Change the settings in `texture_import.py`, not in the editor's Import
dock: the generator puts them back on the next run. Another texture
generator writing under `game/assets/textures/` writes its own `.import`
files with mipmaps and compression set and `detect_3d/compress_to=0`.
Git ignores every `.import`, so a hand-made texture under
`game/assets/textures/` also needs its `.import` committed (a `!` line in
`.gitignore`) to pass the validator.

### Parameter file

| Key | Meaning |
|---|---|
| `theme`, `seed`, `cell_size` | theme id (folder and manifest key), the one seed every random choice derives from, cell size |
| `board` | `width`, `height`, `tile_gap`, `tile_height`, `frame_width`, `frame_height` |
| `artifacts.piece_width` | piece footprint as a fraction of a cell |
| `artifacts.kinds[]` | `index`, `name` (file names), `shape` (a function in `gen_theme_meshes.py`: `round_gem`, `pyramid`, `sun`, `leaf`, `diamond`, `hex_crystal`, `crescent`), `material` |
| `backdrop` | `tiers`, `foliage_clumps`, `ground_size` |
| `water` | optional moat round the lowest tier: `inset`, `width`, `depth`, `level`, `coping_width`, `coping_height`, `causeway_width` ([Water](water.md#parameters)); without it the ground is unbroken |
| `textures.size`, `textures.hi_size` | base and desktop texture edge in pixels (powers of two, ≤ 1024 and ≤ 2048) |
| `textures.sets.<Set>` | one PBR set `T_<Set>_*`: `recipe` (a function in `gen_theme_textures.py`: `stone`, `ground`, `gold`, `gem`, `lapis`, `jade`, `moonstone`), `normal_strength` (slope at 1024 px), `hi` (also write a `hi/` set at `hi_size`; the base is filtered down from it), `height` (also write `_H`) |
| `materials.<MI_name>` | `albedo` (the colour the kind reads as; also the HUD token), optional `albedo_tint` (multiplies the texture instead, for sets that carry their own colour), `set` (a texture set: makes an `ORMMaterial3D` whose roughness, metallic and occlusion come from `_ORM`) with `triplanar_scale`, `normal_scale`, `world_triplanar`; without `set`, `metallic` and `roughness`; `ior` (index of refraction; sets the dielectric reflectance) or `specular`; `emission` + `emission_energy`, `rim` + `rim_tint`, `clearcoat` + `clearcoat_roughness`, `anisotropy`, `subsurface` (`strength`; optional `transmittance` colour with `depth` and `boost`: transmittance is written only when the colour is given, because Godot 4.8 beta 1's Forward Mobile renderer generates a scene shader it cannot compile for a material with transmittance on, where it only warns about subsurface scattering itself; the temple's blocks have only `strength`, [About quality settings](../explanation/about-quality-settings.md#the-one-material-change-no-transmittance)), `backlight`, `double_sided`. Or a ShaderMaterial: `shader` + `params` (a hex string is a colour, a list of numbers a `Vector2`/`Vector3`; `water: true` adds the channel's geometry for the water shaders), optional `textures` (uniform to a path under `textures/<theme>/`; a material with a `hi` block takes the file's `hi/` copy), `noise_textures` (uniform to a `NoiseTexture2D`: `seed`, `frequency`, `size`) and `render_priority` (with `set`, the shader gets `texture_albedo`, `texture_normal`, `texture_orm`, `texture_height`, `albedo`, `triplanar_scale`, `normal_scale`). `hi`: keys merged over these when the material is written (one material results; see [Texture sizes](#texture-sizes)) |
| `environment` | sky colours, `ambient_energy`, `saturation`, `sun.transform` (as written in a `.tscn`, basis rows first) |
| `lighting` | physical light values, see [Lighting](#lighting); `lighting.torch` also sets each brazier's fire light |
| `grade` | the colour grade and its palette limits, see [Colour grade](#colour-grade) |
| `post` | glow and depth of field, see [Post-processing](#post-processing) |
| `lookdev` | game camera `camera_transform` and `fov`, `highlight_cell`; `fire_camera_transform` and `fire_fov` for the brazier close-up `LD_<Theme>_Fire` |
| `fire` | brazier fire: `rim_height` and the `flame`, `smoke`, `embers`, `light` and `haze` settings of `PF_BrazierFire` ([Fire](fire.md#pf_brazierfire)) |
| `surface_tokens`, `extra_tokens` | palette entries exported to `design/tokens/<theme>-board.json`: token name to material (its `albedo`) or to a hex value |

## Lighting

Lights use physical units (`rendering/lights_and_shadows/use_physical_light_units`):
lux for the sun, lumens for the rest, nits for the sky and emission, a
camera sensitivity in ISO. Why:
[About lighting](../explanation/about-lighting.md).

`gen_theme_scenes.py` writes these nodes into `PF_Environment.tscn` from
the parameter file's `lighting` section:

| Node | Type | Where | Parameters (`lighting.*`) | Temple |
|---|---|---|---|---|
| `WorldEnvironment` | sky, AgX tonemapper, SSAO, SSIL unless the theme turns it off, optional height fog; glow and the grade LUT from [Post-processing](#post-processing) and [Colour grade](#colour-grade) | all | `sky_nits`, `agx_white`, `agx_contrast`, optional `ssil` (bool, default true: writes `ssil_enabled`), `ssao` (`intensity`, `power`, `light_affect`), optional `fog` (`color`, `height`, `height_density`, `sky_affect`) | 15,000 nits, blue overhead (`#3a78b8`) and pale blue-grey at the horizon (`#b4c6d6`); white 8, contrast 1.5; SSIL on (the key is not set); SSAO intensity 3, power 1.5, light affect 0.3; no height fog. Volumetric fog on, with no density of its own: see [Atmosphere](#atmosphere) |
| `camera` (`CameraAttributesPractical`) | exposure, on the environment and `LightmapGI` | all | `exposure` (`sensitivity`, `multiplier`, `auto`; with `auto` true also `auto_scale`, `auto_speed`, `auto_min_sensitivity`, `auto_max_sensitivity`) | ISO 440, multiplier 0.8, `auto` false: every tier renders at the same fixed exposure, darker and more saturated than auto exposure made it ([About lighting](../explanation/about-lighting.md#revision-fixed-exposure)) |
| `Sun` | `DirectionalLight3D`, shadows | all | `sun` (`lux`, `kelvin`, `color`, `angular_distance`, optional `fog_energy` → `light_volumetric_fog_energy`) | 30,000 lux, 5800 K, 0.6°, fog energy 3 (see [Atmosphere](#atmosphere) and [About lighting](../explanation/about-lighting.md#sun-and-sky-balance)) |
| `Fire1`…`Fire4/Light` | `FlickerLight3D` in each brazier's `PF_BrazierFire` ([Fire](fire.md)), cube-map shadows (dual paraboloid is not supported on the Compatibility renderer) | all | `torch` (`lumens`, `kelvin`, `color`, `size`, `range`) | 90,000 lm, 1900 K, range 4.5 m |
| `SkyFill` | `AreaLight3D` over the board, not baked | all | `sky_fill` (`transform`, `lumens`, `color`, `size`, `range`) | 6,000 lm, cool (`#b8d4ff`), 10 × 10 m |
| `ReflectionProbe` | box-projected probe; leaves out render layer 2 (fire effects, [Fire](fire.md#pf_brazierfire)) | all | `reflection_probe` (`position`, `size`) | 12 × 4 × 12 m |
| `Lightmap` | `LightmapGI` with probes | all | `lightmap` (`quality`, `bounces`, `texel_scale`, `max_texture_size`, `probes_subdiv`, optional `sky_energy`: bake the theme's sky at this energy instead of the scene's) | medium, 3 bounces, sky 0.5 |

Static meshes (board and backdrop) bake into the lightmap: their
`.obj.import` files set `generate_lightmap_uv2=true` and are committed.
Pieces stay out of GI (`gi_mode = 0`): the lightmap's probes light them.
Lightmapped surfaces take indirect light only from the lightmap; SSIL
adds the moving bounce on them.

**Baking.** `LightmapGI` bakes only inside the editor, so
`tools/assets/bake_lightmaps.cmake` imports the project, then starts the editor with
`--bake-lightmaps=res://assets/lookdev/LD_<Theme>_Bake.tscn`. The
extension's editor plugin (`extension/src/render/lightmap_baker.cpp`)
opens each scene, selects its `Lightmap` node, presses Bake, saves to the
path in the node's `light_data_path` metadata and prints
`LIGHTMAP_BAKE ok|fail <scene> <path> <ms>`. The bake scene holds the
environment and board scenes under the same names as the board view, so
the board view finds the bake at run time
(`extension/src/render/baked_lighting.cpp` loads it into the `Lightmap`
node). Without a bake the board view logs a warning naming the missing file, and the
scene still renders, without baked light. Like the textures, bakes are
build outputs: git ignores them and CI restores or makes them
([CI pipeline](ci-pipeline.md#caches)). The temple bakes in about four
minutes on lavapipe (a 0.6 MB EXR atlas).

**Emission on web.** Compatibility in Godot 4.7.2 applies the camera
exposure to lights and the sky but not to emission. Materials carry
emission in nits (`emission_intensity`, or the `emission_energy` of
`SH_Gem`) at 1 nit per unit (`EMISSION_NITS` in `gen_theme_scenes.py`),
so the web build reads the same value as the others.

## Colour grade

A theme's `grade` section is an ASC CDL applied in display space after
tonemapping: per channel `out = clamp(in × slope + offset) ^ power`, then
`saturation` around Rec. 709 luma. `gen_theme_textures.py` bakes it with
`tools/assets/grade_lut.py` into
`game/assets/textures/<theme>/grade/T_Grade_LUT.png`, a `lut_size`³ table
laid out as `lut_size` slices side by side (red across, green down, blue
by slice, each texel holding the grade at its centre, `(i + 0.5) / lut_size`,
since Godot samples the table with no half-texel remap), and writes its `.import` (`importer="3d_texture"`, lossless,
no mipmaps, `slices/horizontal` = `lut_size`). The theme's
`WorldEnvironment` uses it as `adjustment_color_correction` on every tier.
Why a CDL and a LUT: [About post-processing](../explanation/about-post-processing.md).

| Key (`grade.*`) | Meaning | Temple |
|---|---|---|
| `lut_size` | texels per axis | 32 (a 1024 × 32 PNG, 96 KB in memory) |
| `slope`, `offset`, `power` | CDL per channel, sRGB-encoded values in 0 to 1 | slope 1.02, 1.01, 1.0; offset 0; power 1 (a near-neutral grade: the light, not the grade, sets the colour; see [`design/grade/temple.md`](../../design/grade/temple.md)) |
| `saturation` | CDL saturation, 1 keeps it | 1.04 |
| `check.max_shift_pieces`, `check.max_shift_surfaces` | most a piece colour (`materials.<kind>.albedo`) or palette colour (`surface_tokens`, `extra_tokens`) may move, OKLab distance | 0.04, 0.05 |
| `check.max_separation_drop` | most the closest pair of piece colours may come closer, OKLab distance, in normal vision, greyscale, protanopia, deuteranopia and tritanopia (Machado et al. 2009, severity 1) | 0.005 |

`python3 tools/assets/grade_lut.py` prints the check for each theme;
`validate_assets.py` fails a grade outside its limits, a LUT that does
not match the parameters, and a LUT that does not import as a 3D
texture.

## Post-processing

`gen_theme_scenes.py` writes the theme environment's glow, and a far
depth of field if the theme asks for one, from the `post` section. The
temple asks for both. What each costs at native 5K on an M4, and why
they stay: [Benchmark results](benchmark-results.md#mac-mini-m4-at-native-5k-project-settings),
[About quality settings](../explanation/about-quality-settings.md).

| Key (`post.*`) | Writes | Temple |
|---|---|---|
| `glow.levels` | `glow_levels/1` to `/7` (mip weights; higher levels spread wider) | 0, 1, 0.5, 0, 0, 0, 0 |
| `glow.intensity`, `glow.strength`, `glow.bloom` | `glow_intensity`, `glow_strength`, `glow_bloom` | 0.5, 1.0, 0 |
| `glow.blend` | `glow_blend_mode`: `additive`, `screen`, `softlight`, `replace` or `mix` | additive |
| `glow.hdr_threshold`, `glow.hdr_scale`, `glow.hdr_luminance_cap` | the exposed scene value glow starts at (before tonemapping, Godot 4.6+), its gain, its cap | 8, 2, 12 |
| `glow.hdr_threshold_by_tier` | tier id to its own threshold, written as `glow_hdr_threshold_<tier>` metadata (dashes as underscores) on the environment; nothing reads it now that the tier system is gone, so every renderer uses `hdr_threshold` | `mobile` 0.6, `web` 0.6 |
| `depth_of_field.beyond_board` (optional block) | turns on the far blur of the theme's `CameraAttributesPractical` (`dof_blur_far_enabled`); the start in metres past the board's far edge, written as `dof_blur_far_distance` and as the `dof_far_beyond_board` metadata `BoardCamera3D` reads ([Board view](board-view.md#camera-fit)) | 0.5 m |
| `depth_of_field.transition`, `depth_of_field.amount` | `dof_blur_far_transition`, `dof_blur_amount` | 2.0, 0.1 |

Glow and any blur are checked by readability rules 6 and 2 with the
readability capture ([Benchmark](benchmark.md#readability-checks)).

## Atmosphere

Ground mist and dust around the temple: volumetric fog lying in banks on
the plaza, cut by the shadows of the temple and ferns, and dust.
Over the board lies only a thin, patchy veil of the mist that the tiles
and pieces read through (readability rule 2 in the
[Visual target](visual-target.md#readability-rules-every-tier)). Why it is
built this way: [About the atmosphere](../explanation/about-the-atmosphere.md).

`gen_theme_scenes.py` writes it into `PF_Environment.tscn` from the
parameter file's `lighting.atmosphere` section and the `MI_AtmosphereFog`
and `MI_Dust` materials:

| Node | Type | Where | Parameters | Temple |
|---|---|---|---|---|
| `WorldEnvironment` | volumetric fog on, global density 0 | Forward+ (the froxel grid is the project's `rendering/environment/volumetric_fog/volume_size` and `volume_depth`) | `atmosphere.fog` (`anisotropy`, `length`, `detail_spread`, `gi_inject`, `ambient_inject`, `temporal_amount`) | isotropic; 30 m, detail spread 1 (even slices, so the low mist far below the camera is as finely sliced as the air near it); GI 0.05, ambient 0.5 (mist in shade takes the cool sky light, so it reads white-grey, not glowing and not tan) |
| `AtmosphereFog` | box `FogVolume` around the temple, `MI_AtmosphereFog` as its material | all | `atmosphere.volumes` (`outer`, `bottom`, `top`); `MI_AtmosphereFog` (below) | 38 × 38 m, from 2.6 m below the board to 0.45 m above it |
| `Dust` | `GPUParticles3D`, ring around the platform, `MI_Dust` | all | `atmosphere.dust` (`amount`, `lifetime`, `inner_radius`, `outer_radius`, `bottom`, `top`, `size`, `speed`, `turbulence`) | 320 motes, 8.5 to 13 m from the centre, 5 cm, at most 5 cm/s |

The fog volume covers the board too. Over it `SH_AtmosphereFog` lays only
a thin veil: drifting patches on the tile tops (y = 0) that thin to nothing
by `veil_height`, below the floating score labels (0.8 cell and up), with
clear tiles between them. The banks lie on the plaza around the platform.
The material's parameters:

| Parameter | Temple | Meaning |
|---|---|---|
| `density` | 3.5 (0 to 8) | extinction per metre in the core of a bank: the paving shows faintly through one |
| `albedo` | `#ebedf0` | scattering colour; mist is water and scatters every colour alike, so it is near white and the light gives it its colour |
| `board_half` | 4.55 (set by the generator) | half the side of the board, frame included; the veil lies inside it and the banks thicken with distance outside it |
| `ground_y` | −1.92 m | the plaza floor the mist lies on (platform top −0.42 m less three 0.5 m tiers) |
| `bank_height`, `top_soft` | 1.4 m, 0.6 m (0.05 to 2) | tallest a bank gets above the floor, and how far its top fades out |
| `thicken_over`, `near_amount` | 7 m, 0.3 (0 to 1) | banks next to the platform are `near_amount` as tall as those `thicken_over` metres out: more mist further away |
| `bank_scale`, `coverage` | 9 m (1 to 40), (0.56, 0.64) | size of one tile of bank noise, and the noise values mapped to no bank and a full bank; the narrow range leaves clear floor between banks |
| `wisp_scale`, `wisp_stretch`, `wisp_amount` | 4 m (0.5 to 20), 3 (1 to 8), 0.35 (0 to 1) | finer noise drawn out along the wind that breaks up the banks' tops and edges |
| `wind` | (0.12, 0, −0.05) m/s | drift of the banks (the wisps drift 1.6 times faster) |
| `base_shade` | 0.75 (0 to 1) | share of the light the bottom of a bank scatters against its top; Godot's fog does not shadow itself, so this gives the banks lit tops |
| `veil_density` | 1.0 (0 to 2) | extinction per metre on the tiles in the core of a patch: thin enough to read the tiles and pieces through |
| `veil_height` | 0.25 m (0.05 to 0.4) | height above the tiles where the veil has thinned to nothing; the range stops under the score labels' bottom edge (about 0.5 m) |
| `veil_scale`, `veil_coverage` | 12 m (2 to 24), (0.58, 0.78) | size of one tile of veil noise, and the noise values mapped to no veil and full veil: patches a few cells across with clear cells between them. Keep `wind` × 3600 s (Godot's `TIME` rollover) a whole number of `veil_scale` tiles, or the patches jump once an hour: with the temple's wind, (432, −180) m, that is 2, 3, 4, 6, 9, 12 or 18 m. The wisps that shade the veil still shift a little at the rollover |
| `veil_spread` | 1 m (0 to 2) | how far past the board edge the veil fades out |
| `density_texture` | `NoiseTexture3D`, 64³, seed 51, frequency 0.08 | from `noise_textures_3d`, built by Godot at load |

The veil scatters all the light it receives (it is not darkened by
`base_shade`); where it overlaps a bank, the shade follows each one's
share of the density. To retune it: [Tune the fog veil](../how-to/tune-the-fog-veil.md).

The sun's `fog_energy` (3) makes up for Godot lighting the fog by single
scattering only: mist this thick scatters light many times, and with
single scattering alone it renders darker than the floor under it, a grey
veil instead of white mist. Scaling the shader's albedo above 1 does not
do it: in our captures albedo × 4 and × 7 looked the same.

**Placement check.** The generator refuses to write the scene when any
point of the ring a dust mote can reach (the inner radius less
`lifetime` × top speed and `turbulence`) would draw over the board, seen
from the game camera at 11, 13 and 36 m (wide window to tall phone) with
`board_clearance` (0.5 m) of margin, unless that point is inside the
stone platform.

## Shaders

| Shader | Used by | Render mode | Cost |
|---|---|---|---|
| `SH_Fire` | brazier flames (`MI_Fire`) | unshaded, additive, no depth write; motion-vector flipbook, blackbody colour, camera-facing billboard | 4 texture and 1 depth sample; see [Fire](fire.md#shaders) |
| `SH_FireSmoke` | brazier smoke (`MI_FireSmoke`) | lit, alpha-blended, no depth write | 1 texture and 1 depth sample |
| `SH_Ember`, `SH_EmberTrail` | embers (`MI_EmberTrail` on the braziers; `MI_Ember` is written but no scene uses it) | unshaded, additive; `SH_EmberTrail` draws tube trails | no texture samples |
| `SH_HeatHaze` | heat haze (`MI_HeatHaze`) | screen-texture distortion, drawn before the flame | 2 noise and 1 screen sample |
| `SH_CellHighlight` | `cell_highlight` | unshaded, additive | one 1×1 quad; no texture samples |
| `SH_Spark` | `fx_match` | unshaded, additive; fades by `INSTANCE_CUSTOM.y` | 16 tiny spheres per burst |
| `SH_AtmosphereFog` | the temple's `FogVolume` (`MI_AtmosphereFog`) | fog shader; no static switches, 1 variant | 3 3D-noise samples per froxel in the volume (banks, wisps, veil); no measurable cost against the clear-square version ([Benchmark results](benchmark-results.md#fog-veil-over-the-board)) |
| `SH_Dust` | dust motes (`MI_Dust`) | lit, additive, no depth write, camera-facing | no texture samples |
| `SH_LightShaft` | `MI_LightShaft`, written by the generator but used by no scene since the light-shaft cards went | unshaded, additive, no depth write; fades out toward the ground in UV | no texture samples |
| `SH_TriplanarPBR` | stone (`MI_Stone*`) | opaque, Burley diffuse, GGX; object-space triplanar PBR set; parallax occlusion on upward faces (`height_depth` metres, 8 to 24 layers by view angle) | 3 texture samples where one projection carries the surface (most pixels; projections under 2% weight are skipped), up to 9 on edges, plus up to 25 height samples on upward faces |
| `SH_Gem` | ruby and amethyst (`MI_Ruby`, `MI_Amethyst`) | transparent pass (reads the screen texture), depth write; refraction with `ior` and `dispersion`, absorption by the body colour over `thickness` | 3 to 9 texture samples (as `SH_TriplanarPBR`), 4 screen samples at LOD 0 (no screen mip chain) and 3 depth samples |
| `SH_Triplanar.gdshaderinc` | included by the two above | object-space triplanar sampling, normal-map blending, Y-face parallax | none of its own |
| `SH_Water` | water (`MI_Water`) | opaque; refracted ray traced through the channel box, absorption, caustics | 1 texture sample (the channel stone), no screen or depth texture ([Water](water.md#shaders)) |
| `SH_WaterSimple` | the cheaper water shader; no material uses it since `MI_Water` became one file ([Water](water.md)) | blended; absorption to the floor height, caustics | no texture samples |
| `SH_Water.gdshaderinc` | included by the two above | ripple noise and slope, caustics, absorption, Fresnel | none |

Where each shader compiles, as last checked: `SH_CellHighlight` and
`SH_Spark` on Forward+, Mobile and Compatibility (Godot 4.4.1); the fire
shaders on all three in Godot 4.7.2; `SH_TriplanarPBR`, `SH_Gem` and
`SH_Water` on Forward+ only (Godot 4.7.2); `SH_Dust` and `SH_LightShaft`
on all three; `SH_AtmosphereFog` on Forward+ and Mobile only, and
Compatibility prints an error for any fog shader it is handed;
`SH_WaterSimple` on Mobile and Compatibility. The materials are now one
set on every renderer, so what the Mobile and Compatibility builds show
for the Forward+-only shaders has to be checked again on those
renderers.

## Budgets and the temple theme's numbers

Budgets from the project profile: piece < 2,000 triangles, board < 20,000,
textures ≤ 1024² for everything mobile and web ship; `hi/` sets up to
2048² ([Visual target](visual-target.md#content-budgets-per-tier)).

| Asset | Triangles |
|---|---|
| Ruby | 156 |
| Carnelian | 220 |
| Sun disc | 372 |
| Jade | 332 |
| Lapis | 76 |
| Amethyst | 120 |
| Moonstone | 548 |
| Board (tiles, frame, fret, studs) | 9,436 |
| Backdrop (platform, braziers, ground, ferns, rocks) | 4,220 |

A full 8×8 board of pieces averages about 260 triangles each (~17k), so
the whole scene stays near 31k triangles. Draw calls on desktop: one per
piece (64), 6 board parts, 6 backdrop parts, the brazier fires (11 draw
calls), plus shadow passes; pieces share 7 materials. Textures: seven base sets at 1024² (`Stone_01` with `_H`;
22 PNGs, 15 MB generated) and 2048² `hi/` sets for the hero surfaces
`Stone_01` and `Gold_01` (7 PNGs, 23 MB generated); all are VRAM-compressed
on import ([Import settings](#import-settings)). In VRAM on desktop
(BC1/BC4 about 0.7 and RGTC about 1.3 bytes per pixel with mipmaps; ETC2
and EAC on mobile are the same sizes) the base sets take about 20 MB and
the `hi/` sets add about 25 MB, well inside every tier's texture budget
([Visual target](visual-target.md#content-budgets-per-tier)).
Frame time on target phones
is not measured yet; that is WP-41.

## Look-dev reference frames

Rendered in Godot 4.7.2 at 1280×720 from the look-dev scenes (software
Vulkan and OpenGL, so lighting is the theme's, not tuned per GPU). Later
look packages compare against these.

| Frame | Scene | Renderer | Materials |
|---|---|---|---|
| [temple-board-before-wp48.jpg](../images/lookdev/temple-board-before-wp48.jpg) | `LD_Temple` | Forward+ | first pass, before WP-48 |
| [temple-board-base-forward.jpg](../images/lookdev/temple-board-base-forward.jpg) | `LD_Temple` | Forward+ | base PBR |
| [temple-board-desktop.jpg](../images/lookdev/temple-board-desktop.jpg) | `LD_Temple_Hi` | Forward+ | desktop `hi/` |
| [temple-board-base-compatibility.jpg](../images/lookdev/temple-board-base-compatibility.jpg) | `LD_Temple` | Compatibility (web) | base PBR |
| [temple-lineup-before-wp48.jpg](../images/lookdev/temple-lineup-before-wp48.jpg) | `LD_Temple_LineUp` | Forward+ | first pass, before WP-48 |
| [temple-lineup-base-forward.jpg](../images/lookdev/temple-lineup-base-forward.jpg) | `LD_Temple_LineUp` | Forward+ | base PBR |
| [temple-lineup-desktop.jpg](../images/lookdev/temple-lineup-desktop.jpg) | `LD_Temple_LineUp_Hi` | Forward+ | desktop `hi/` |
| [temple-board-water-desktop.jpg](../images/lookdev/temple-board-water-desktop.jpg) | `LD_Temple` | Forward+ | desktop `hi/`, with the water channel (WP-50) |
| [temple-water-desktop.jpg](../images/lookdev/temple-water-desktop.jpg), [-mobile](../images/lookdev/temple-water-mobile.jpg), [-compatibility](../images/lookdev/temple-water-compatibility.jpg) | `LD_Temple_Water` | Forward+, Mobile, Compatibility | water close-up ([Water](water.md)) |
| [temple-board-lit-web.jpg](../images/lookdev/temple-board-lit-web.jpg) | game, benchmark board | Compatibility (`web`) | WP-47 lighting, lightmap baked |
| [temple-board-lit-desktop-high.jpg](../images/lookdev/temple-board-lit-desktop-high.jpg) | game, benchmark board | Forward+ (`desktop-high`) | WP-47 lighting, lightmap and `VoxelGI` |
| [temple-board-before-wp52-desktop-high.jpg](../images/lookdev/temple-board-before-wp52-desktop-high.jpg) | `readability.tscn`, 540×1170 | Forward+ (`desktop-high`) | WP-47 lighting, old glow (before WP-52) |
| [temple-board-post-desktop-high.jpg](../images/lookdev/temple-board-post-desktop-high.jpg) | `readability.tscn`, 1280×720 | Forward+ (`desktop-high`) | WP-52 glow, grade and depth of field, before the 19 October 2026 lighting fix |
| [temple-board-post-web.jpg](../images/lookdev/temple-board-post-web.jpg) | `readability.tscn`, 540×1170 | Compatibility (`web`) | WP-52 glow and grade, before the 19 October 2026 lighting fix |
| [temple-board-post-mobile.jpg](../images/lookdev/temple-board-post-mobile.jpg) | `readability.tscn`, 540×1170 | Mobile (`mobile`) | WP-52 glow and grade, before the 19 October 2026 lighting fix |
| [temple-atmosphere-before-mist.jpg](../images/lookdev/temple-atmosphere-before-mist.jpg) | `LD_Temple_Hi` | Forward+ (`desktop-high`) | WP-51 volumetric fog and dust as first built (an even cream layer), no lightmap |
| [temple-atmosphere-desktop-high.jpg](../images/lookdev/temple-atmosphere-desktop-high.jpg) | game, benchmark board | Forward+ (`desktop-high`), RTX 4080 SUPER | ground mist in banks and dust, lightmap and `VoxelGI` |
| [temple-atmosphere-mobile.jpg](../images/lookdev/temple-atmosphere-mobile.jpg) | `LD_Temple` | Mobile (`mobile`) | WP-51 light-shaft cards, no lightmap |
| [temple-atmosphere-web.jpg](../images/lookdev/temple-atmosphere-web.jpg) | `LD_Temple` | Compatibility (`web`) | WP-51 light-shaft cards, no lightmap |
| [temple-board-contrast-before-desktop-high.jpg](../images/lookdev/temple-board-contrast-before-desktop-high.jpg) | game, first level | Forward+ (`desktop-high`), RTX 4080 SUPER | before WP-60: `main` with the ground mist |
| [temple-board-contrast-desktop-high.jpg](../images/lookdev/temple-board-contrast-desktop-high.jpg) | game, first level | Forward+ (`desktop-high`), RTX 4080 SUPER | WP-60 lighting: darker shadows, 90,000 lm braziers, SSAO 3 |
| [temple-board-contrast-before-mobile.jpg](../images/lookdev/temple-board-contrast-before-mobile.jpg) | game, first level | Mobile (`mobile`), RTX 4080 SUPER | before WP-60: `main` with the ground mist |
| [temple-board-contrast-mobile.jpg](../images/lookdev/temple-board-contrast-mobile.jpg) | game, first level | Mobile (`mobile`), RTX 4080 SUPER | WP-60 lighting: darker shadows, 90,000 lm braziers, SSAO 3 |
| [temple-board-contrast-before-web.jpg](../images/lookdev/temple-board-contrast-before-web.jpg) | game, first level | Compatibility (`web`), RTX 4080 SUPER | before WP-60: `main` with the ground mist |
| [temple-board-contrast-web.jpg](../images/lookdev/temple-board-contrast-web.jpg) | game, first level | Compatibility (`web`), RTX 4080 SUPER | WP-60 lighting: darker shadows, 90,000 lm braziers, SSAO 3 |

## Validator rules

CI runs `validate_assets.py` on every push and PR (the `assets`
workflow, see [CI pipeline](ci-pipeline.md#assets-workflow)). It reports
each as `error: <file>: <rule and fix>`:

- `default_theme` names a theme in the manifest; every manifest path exists.
- Every required name is present (missing ones are warnings, since the view
  falls back).
- Triangles of every OBJ a scene pulls in: pieces < 2,000, board < 20,000.
- Prefixes per the naming table; scenes start `PF_` or `LD_`.
- Every `res://` reference in a `.tscn` or `.tres` exists.
- Every file parses as its format: PNGs decode, OBJs have vertices and
  faces with in-range indices, `.tscn` and `.tres` are Godot text
  resources.
- `.gdshader`, `.obj`, `.tscn`, `.tres` carry the SPDX line.
- PNGs are power-of-two, ≤ 1024 px (≤ 2048 px in `hi/`).
- Each map's suffix is one of `_BC`, `_N`, `_ORM`, `_H` (and the legacy
  `_R`) with its channels: RGB, RGB, RGB, greyscale. No gamma or ICC
  chunk (values are stored as they are meant).
- Colour space follows the slot: a `_BC` map is wired only to an albedo
  slot and the data maps only to their own slots in every `.tres`; shader
  samplers `texture_albedo` and `texture_emission` carry `source_color`,
  `texture_normal`, `texture_orm`, `texture_height` do not.
- Values are physically plausible: dielectric base colour within
  30-240 sRGB and metal (where `_ORM` blue ≥ 0.5) at 180 or more; normals
  unit length, facing out, averaging flat; metallic 0 or 1 except at most
  5% of pixels; roughness ≥ 0.02; height spans a real range.
- Every PNG under `game/assets/textures/` has a `.import` beside it.
  `_BC`, `_N`, `_ORM`, `_H` and legacy `_R` maps carry exactly the
  [import settings](#import-settings); any other texture there sets
  `compress/mode` and `mipmaps/generate` and turns
  `detect_3d/compress_to` off.
- No `.tres`, `.tscn` or `.gdshader` filters anisotropically with repeat
  (`texture_filter = 4` or `5` without `texture_repeat = false`, or a
  `filter_*_anisotropic` sampler without `repeat_disable`); see texture
  filtering above.
- Every file in a `hi/` folder has a base file of the same name; a `hi/`
  texture is the same map at the same or a larger size.
- A theme's `grade` keeps its palette within its `check` limits, its
  `grade/T_Grade_LUT.png` matches the parameters, and its `.import`
  imports it as a 3D texture ([Colour grade](#colour-grade)). Grade LUTs
  are exempt from the texture map rules above.
- No export preset excludes the `hi/` texture folder: the folded
  materials name those files.
- `PROVENANCE.md` has a record for each theme.
- Every board and backdrop OBJ has a committed `.obj.import` with
  `generate_lightmap_uv2=true`, and each theme has
  `game/assets/lookdev/LD_<Theme>_Bake.tscn`.
- `design/tokens/temple-board.json` piece colours match the theme parameters (catches a hand edit of the generated file).

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
| `game/assets/materials/<theme>/` | `MI_*.tres` materials: the base variant every tier can load (mobile, web, fallback) |
| `game/assets/materials/<theme>/hi/` | desktop variants of some of those materials, same file names |
| `game/assets/materials/<theme>/lo/` | web variants of the emissive materials, same file names (see [Lighting](#lighting)) |
| `game/assets/materials/shaders/` | `SH_*.gdshader` and `SH_*.gdshaderinc`, shared by all themes |
| `game/assets/textures/<theme>/` | `T_<Set>_{BC,N,ORM,H}.png` base texture sets, at most 1024²; build outputs, ignored by git (see below) |
| `game/assets/textures/<theme>/hi/` | desktop variants of the hero sets, same file names, at most 2048²; build outputs too |
| `game/assets/lookdev/` | `LD_*.tscn` look-dev scenes (game camera, line-up) and `LD_<Theme>_Bake.tscn`, the scene each lightmap is baked from |
| `game/assets/models/<theme>/backdrop/PF_Environment.lmbake` and `.exr` | the baked lightmap; build output, ignored by git (see [Lighting](#lighting)) |
| `game/assets/PROVENANCE.md` | one record per theme or asset family |

| Prefix / suffix | Meaning |
|---|---|
| `SM_` | static mesh (OBJ, one material each) |
| `PF_` | asset scene the manifest points at |
| `LD_` | look-dev scene, never shipped from code; `_Hi` swaps in the `hi/` materials |
| `MI_` | material |
| `SH_` | shader |
| `T_` + `_BC` | base colour: sRGB, RGB, no light or occlusion baked in |
| `T_` + `_N` | normal: tangent space, OpenGL (green up), RGB |
| `T_` + `_ORM` | occlusion, roughness, metallic in R, G, B: linear |
| `T_` + `_H` | height: linear, greyscale, white is high (parallax) |
| `T_` + `_R` | roughness: linear, greyscale (legacy; new sets pack it in `_ORM`) |

Names use `A-Z a-z 0-9 _` only. Code refers to logical names in the
manifest, never to these paths.

## Quality tier variants

A material or texture that differs on desktop has a second file with the
same name in a `hi/` folder next to the base one. The base file is what
`mobile` and `web` load and what every tier falls back to; `desktop-high`
and `desktop-standard` load the `hi/` file when it exists:
`QualityTiers::resolve_variant` maps a path to it and falls back to the
base ([Quality tiers](quality-tiers.md)). `QualityTiers` swaps the
materials of every mesh as it enters the tree, so prefabs and the
manifest name the base files and the game shows the `hi/` look on
desktop; `LD_<Theme>_Hi.tscn` names the `hi/` files directly. Mobile and web export presets
list `*/hi/*` in `exclude_filter`, so those builds carry only their own
sizes. The web tier's `lo/` folder holds only the emissive materials (see
[Lighting](#lighting)); the temple's base texture sets fit the web
texture budget.

| Material | Base (mobile, web) | `hi/` (desktop) |
|---|---|---|
| Stone (`MI_Stone*`) | `ORMMaterial3D`, 1024² `Stone_01` set, normal map, triplanar | `SH_TriplanarPBR`: 2048² `Stone_01` set with parallax occlusion from `_H` |
| Gold (`MI_Gold`) | `ORMMaterial3D`, 1024² `Gold_01`, metallic 1, anisotropy | same with the 2048² set |
| Ruby, amethyst | `ORMMaterial3D`, `Gem_01` tinted, opaque, Fresnel from the real index of refraction, rim | `SH_Gem`: refraction of the scene behind with dispersion and absorption by the body colour |
| Carnelian, jade, moonstone | `ORMMaterial3D` with backlight for cheap translucency | sub-surface scattering with a transmittance colour; moonstone adds a stronger clearcoat |
| Lapis | `ORMMaterial3D`, `Lapis_01` (blue, calcite, metallic pyrite flecks) | none; the base is used everywhere |
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
| `python3 tools/assets/gen_theme_textures.py [params]` | `T_*_{BC,N,ORM,H}.png` (tiling), base and `hi/`, each with its `.import` | numpy, Pillow |
| `python3 tools/assets/gen_theme_scenes.py [params]` | `MI_*.tres`, `hi/MI_*.tres` and `lo/MI_*.tres`, `PF_*.tscn`, `LD_*.tscn` (with `LD_<Theme>_Bake.tscn`), the theme's manifest entry, `design/tokens/<theme>-board.json` | standard library |
| `cmake [-DGODOT=PATH] [-DXVFB=ON] [-DFORCE=ON] [-DCHECK=ON [-DCHECK_OUTPUT=FILE]] -P tools/assets/bake_lightmaps.cmake` | each theme's `PF_Environment.lmbake` and `.exr`, and a `.lightmap-inputs` stamp; skips themes whose stamp matches (`CHECK` only lists the stale ones, and with `CHECK_OUTPUT` appends `stale=true` or `stale=false` to that file); the import is stopped after 900 s and the editor after 900 s per scene | Godot 4.7.2 editor, the native extension built, the textures generated, a Vulkan driver (Mesa lavapipe works without a GPU) |
| `python3 tools/assets/validate_assets.py` | nothing; exits 1 on errors | Pillow |

`params` defaults to `tools/assets/themes/temple.json`
(`gen_theme_textures.py` with no argument does every theme in
`tools/assets/themes/`).

The `T_*` texture sets are build outputs, not committed: git ignores
`game/assets/textures/*/T_*.png`, `*/hi/T_*.png` and the `.inputs` stamp.
Hand-made textures elsewhere in `game/assets/textures/` are still
committed. `gen_theme_textures.py` writes a theme's `.inputs` stamp, a
SHA-256 of the script, the theme's parameters and the numpy and Pillow
versions, and skips drawing the theme when the stamp matches and every
file exists (it still writes any missing or wrong `.import` file). Three things run it: the CMake target `textures` (part of `all`
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
| `materials.<MI_name>` | `albedo` (the colour the kind reads as; also the HUD token), optional `albedo_tint` (multiplies the texture instead, for sets that carry their own colour), `set` (a texture set: makes an `ORMMaterial3D` whose roughness, metallic and occlusion come from `_ORM`) with `triplanar_scale`, `normal_scale`, `world_triplanar`; without `set`, `metallic` and `roughness`; `ior` (index of refraction; sets the dielectric reflectance) or `specular`; `emission` + `emission_energy`, `rim` + `rim_tint`, `clearcoat` + `clearcoat_roughness`, `anisotropy`, `subsurface` (`strength`, `transmittance`, `depth`, `boost`), `backlight`, `double_sided`. Or a ShaderMaterial: `shader` + `params` (a hex string is a colour, a list of numbers a `Vector2`/`Vector3`; `water: true` adds the channel's geometry for the water shaders), optional `textures` (uniform to a path under `textures/<theme>/`; the `hi` variant takes the file's `hi/` copy), `noise_textures` (uniform to a `NoiseTexture2D`: `seed`, `frequency`, `size`) and `render_priority` (with `set`, the shader gets `texture_albedo`, `texture_normal`, `texture_orm`, `texture_height`, `albedo`, `triplanar_scale`, `normal_scale`). `hi`: keys that replace these for the desktop variant in `hi/` |
| `environment` | sky colours, `ambient_energy`, `saturation`, `sun.transform` (as written in a `.tscn`, basis rows first) |
| `lighting` | physical light values, see [Lighting](#lighting); `lighting.torch` also sets each brazier's fire light |
| `lookdev` | game camera `camera_transform` and `fov`, `highlight_cell`; `fire_camera_transform` and `fire_fov` for the brazier close-up `LD_<Theme>_Fire` |
| `fire` | brazier fire: `rim_height` and the `flame`, `smoke`, `embers`, `light` and `haze` settings of `PF_BrazierFire` ([Fire](fire.md#pf_brazierfire)) |
| `surface_tokens`, `extra_tokens` | palette entries exported to `design/tokens/<theme>-board.json`: token name to material (its `albedo`) or to a hex value |

## Lighting

Lights use physical units (`rendering/lights_and_shadows/use_physical_light_units`):
lux for the sun, lumens for the rest, nits for the sky and emission, a
camera sensitivity in ISO. Why, and what each tier gets:
[About lighting](../explanation/about-lighting.md).

`gen_theme_scenes.py` writes these nodes into `PF_Environment.tscn` from
the parameter file's `lighting` section:

| Node | Type | Tiers | Parameters (`lighting.*`) | Temple |
|---|---|---|---|---|
| `WorldEnvironment` | sky, AgX tonemapper, SSAO, SSIL, glow, optional height fog | all (switches per [Quality tiers](quality-tiers.md)) | `sky_nits`, `agx_white`, `agx_contrast`, `ssao` (`intensity`, `power`, `light_affect`), optional `fog` (`color`, `height`, `height_density`, `sky_affect`) | 15,000 nits; white 16, contrast 1.25; SSAO intensity 3, power 1.5, light affect 0.3; no height fog. Volumetric fog on, with no density of its own: see [Atmosphere](#atmosphere) |
| `camera` (`CameraAttributesPractical`) | exposure, on the environment, `LightmapGI` and `VoxelGI` | auto exposure on desktop only | `exposure` (`sensitivity`, `multiplier`, `auto_scale`, `auto_speed`, `auto_min_sensitivity`, `auto_max_sensitivity`) | ISO 440 (auto 300 to 640), `auto_scale` 0.32 (desktop only, since only desktop has auto exposure: darker sunlit stone, WP-60) |
| `Sun` | `DirectionalLight3D`, shadows | all | `sun` (`lux`, `kelvin`, `color`, `angular_distance`, optional `fog_energy` → `light_volumetric_fog_energy`) | 11,000 lux, 4500 K, 0.6°, fog energy 7 (see [Atmosphere](#atmosphere)) |
| `Fire1`…`Fire4/Light` | `FlickerLight3D` in each brazier's `PF_BrazierFire` ([Fire](fire.md)), dual paraboloid shadows (seam flat above the board), group `quality_shadow_light` | all; shadows on the nearest 8 or 2 | `torch` (`lumens`, `kelvin`, `color`, `size`, `range`) | 90,000 lm, 1900 K, range 4.5 m |
| `SkyFill` | `AreaLight3D` over the board, group `quality:area_light`, not baked | `desktop-high` | `sky_fill` (`transform`, `lumens`, `color`, `size`, `range`) | 6,000 lm, 10 × 10 m |
| `ReflectionProbe` | box-projected probe; leaves out render layer 2 (fire effects, [Fire](fire.md#pf_brazierfire), and the light-shaft cards) | all | `reflection_probe` (`position`, `size`) | 12 × 4 × 12 m |
| `VoxelGI` | real-time bounce received by the moving pieces, group `quality:voxel_gi`, baked at run time | `desktop-high` | `voxel_gi` (`position`, `size`, `subdiv`) | 14 × 4 × 14 m, 64 cells |
| `Lightmap` | `LightmapGI` with probes | all | `lightmap` (`quality`, `bounces`, `texel_scale`, `max_texture_size`, `probes_subdiv`, optional `sky_energy`: bake the theme's sky at this energy instead of the scene's) | medium, 3 bounces, sky 0.3 |

Static meshes (board and backdrop) bake into the lightmap: their
`.obj.import` files set `generate_lightmap_uv2=true` and are committed.
Pieces stay out of GI (`gi_mode = 0`): the lightmap's probes light them,
and on `desktop-high` they receive `VoxelGI`'s bounce without being
voxelized into it each frame. Lightmapped surfaces
take indirect light only from the lightmap; SSIL adds the moving bounce
on them.

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
node and bakes `VoxelGI` once the board is built). Without a bake the board view logs a warning naming the missing file, and the
scene still renders, without baked light. Like the textures, bakes are
build outputs: git ignores them and CI restores or makes them
([CI pipeline](ci-pipeline.md#caches)). The temple bakes in about four
minutes on lavapipe (a 0.6 MB EXR atlas).

**Emission on web.** Compatibility in Godot 4.7.2 applies the camera
exposure to lights and the sky but not to emission. Base materials carry
emission in nits (`emission_intensity`, or the `emission_energy` of
`SH_Gem`, scaled so the theme's `emission_energy` reads the same at the
theme's sensitivity); each emissive material also gets a `lo/` copy at
1 nit for web, which `QualityTiers` loads there.

## Atmosphere

Ground mist and dust around the temple: volumetric fog lying in banks on
the plaza, cut by the shadows of the temple and ferns, and dust on the desktop tiers, light-shaft cards in its place on mobile and web.
Nothing of it is ever over the board (readability rule 2 in the
[Visual target](visual-target.md#readability-rules-every-tier)). Why it is
built this way: [About the atmosphere](../explanation/about-the-atmosphere.md).

`gen_theme_scenes.py` writes it into `PF_Environment.tscn` from the
parameter file's `lighting.atmosphere` section and the `MI_AtmosphereFog`,
`MI_Dust` and `MI_LightShaft` materials:

| Node | Type | Tiers | Parameters | Temple |
|---|---|---|---|---|
| `WorldEnvironment` | volumetric fog on, global density 0 | `desktop-high` (64 × 64 froxels), `desktop-standard` (32 × 32) | `atmosphere.fog` (`anisotropy`, `length`, `detail_spread`, `gi_inject`, `ambient_inject`, `temporal_amount`) | isotropic; 30 m, detail spread 1 (even slices, so the low mist far below the camera is as finely sliced as the air near it); GI 0.05, ambient 0.2 (mist in shade is dim and lit by the sky, not glowing) |
| `AtmosphereFog` | box `FogVolume` around the temple, group `quality:volumetric_fog`; its material path in `metadata/fog_material_path` | desktop | `atmosphere.volumes` (`clearance`, `outer`, `bottom`, `top`); `MI_AtmosphereFog` (below) | 38 × 38 m, from 2.6 m below the board to the board's height |
| `Dust` | `GPUParticles3D`, ring around the platform, `MI_Dust`, group `quality:volumetric_fog` | desktop | `atmosphere.dust` (`amount`, `lifetime`, `inner_radius`, `outer_radius`, `bottom`, `top`, `size`, `speed`, `turbulence`) | 320 motes, 8.5 to 13 m from the centre, 5 cm, at most 5 cm/s |
| `LightShaftCards` | `Node3D` holding one `MeshInstance3D` quad per card (`Card01`…), along the sun, `MI_LightShaft`, group `quality_fallback:volumetric_fog`; the cards are on render layer 2 so the reflection probe leaves them out | `mobile`, `web` | `atmosphere.shafts` (`count`, `inner_radius`, `outer_radius`, `length`, `width`, `ground`) | 12 cards, 3.5 to 5.5 m long, 12 draw calls |

The fog volume covers the board too; its shader keeps it empty there.
`SH_AtmosphereFog` gives zero density inside a square of half-side
`clear_half` around the board centre (board plus `volumes.clearance`) and
fades in over `fade` metres outside it. The material's parameters:

| Parameter | Temple | Meaning |
|---|---|---|
| `density` | 6 | extinction per metre in the core of a bank: the floor all but disappears under one |
| `albedo` | `#ebedf0` | scattering colour; mist is water and scatters every colour alike, so it is near white and the light gives it its colour |
| `clear_half` | 6.15 (set by the generator) | half the side of the fog-free square: board half-size 4.55 + clearance 1.6 |
| `fade` | 1.2 m | ramp from no fog to full fog outside that square |
| `ground_y` | −1.92 m | the plaza floor the mist lies on (platform top −0.42 m less three 0.5 m tiers) |
| `bank_height`, `top_soft` | 1.4 m, 0.6 m | tallest a bank gets above the floor, and how far its top fades out |
| `thicken_over`, `near_amount` | 7 m, 0.5 | banks next to the platform are `near_amount` as tall as those `thicken_over` metres out: more mist further away |
| `bank_scale`, `coverage` | 9 m, (0.56, 0.64) | size of one tile of bank noise, and the noise values mapped to no bank and a full bank; the narrow range leaves clear floor between banks |
| `wisp_scale`, `wisp_stretch`, `wisp_amount` | 4 m, 3, 0.35 | finer noise drawn out along the wind that breaks up the banks' tops and edges |
| `wind` | (0.12, 0, −0.05) m/s | drift of the banks (the wisps drift 1.6 times faster) |
| `base_shade` | 0.75 | share of the light the bottom of a bank scatters against its top; Godot's fog does not shadow itself, so this gives the banks lit tops |
| `density_texture` | `NoiseTexture3D`, 64³, seed 51, frequency 0.08 | from `noise_textures_3d`, built by Godot at load |

The sun's `fog_energy` (7) makes up for Godot lighting the fog by single
scattering only: mist this thick scatters light many times, and with
single scattering alone it renders darker than the floor under it, a grey
veil instead of white mist. Scaling the shader's albedo above 1 does not
do it: in our captures albedo × 4 and × 7 looked the same.

The fog material is not in the scene as a resource. `QualityTiers` loads
it from the volume's metadata on tiers with volumetric fog and clears it on
the others, because the Compatibility renderer prints an error for any fog
shader it is handed ([Quality tiers](quality-tiers.md#node-groups)).

**Placement check.** The generator refuses to write the scene when any
point of a shaft card (a 9 × 9 grid over each card), or of the ring a dust
mote can reach (the inner radius less `lifetime` × top speed and
`turbulence`), would draw over the board, seen from the game camera at 11,
13 and 36 m (wide window to tall phone) with `board_clearance` (0.5 m) of
margin, unless that point is inside the stone platform. Cards are placed at
random (seed + 51) until they pass.

## Shaders

| Shader | Used by | Render mode | Cost |
|---|---|---|---|
| `SH_Fire` | brazier flames (`MI_Fire`) | unshaded, additive, no depth write; motion-vector flipbook, blackbody colour, camera-facing billboard | 4 texture and 1 depth sample; see [Fire](fire.md#shaders) |
| `SH_FireSmoke` | brazier smoke (`MI_FireSmoke`) | lit, alpha-blended, no depth write | 1 texture and 1 depth sample |
| `SH_Ember`, `SH_EmberTrail` | embers (`MI_Ember`, `MI_EmberTrail`) | unshaded, additive; trails on tiers with `particle_trails` | no texture samples |
| `SH_HeatHaze` | heat haze (`MI_HeatHaze`), desktop | screen-texture distortion, drawn before the flame | 2 noise and 1 screen sample |
| `SH_CellHighlight` | `cell_highlight` | unshaded, additive | one 1×1 quad; no texture samples |
| `SH_Spark` | `fx_match` | unshaded, additive; fades by `INSTANCE_CUSTOM.y` | 16 tiny spheres per burst |
| `SH_AtmosphereFog` | the temple's `FogVolume` (`MI_AtmosphereFog`), desktop | fog shader | 1 3D-noise sample per froxel in the volume |
| `SH_Dust` | dust motes (`MI_Dust`), desktop | lit, additive, no depth write, camera-facing | no texture samples |
| `SH_LightShaft` | light-shaft cards (`MI_LightShaft`), mobile and web | unshaded, additive, no depth write; fades out toward the ground in UV | no texture samples |
| `SH_TriplanarPBR` | desktop stone (`hi/MI_Stone*`) | opaque, Burley diffuse, GGX; object-space triplanar PBR set; parallax occlusion on upward faces (`height_depth` metres, 8 to 24 layers by view angle) | 3 texture samples where one projection carries the surface (most pixels; projections under 2% weight are skipped), up to 9 on edges, plus up to 25 height samples on upward faces |
| `SH_Gem` | desktop ruby and amethyst (`hi/MI_Ruby`, `hi/MI_Amethyst`) | transparent pass (reads the screen texture), depth write; refraction with `ior` and `dispersion`, absorption by the body colour over `thickness` | 3 to 9 texture samples (as `SH_TriplanarPBR`), 4 screen samples at LOD 0 (no screen mip chain) and 3 depth samples |
| `SH_Triplanar.gdshaderinc` | included by the two above | object-space triplanar sampling, normal-map blending, Y-face parallax | none of its own |
| `SH_Water` | desktop water (`hi/MI_Water`) | opaque; refracted ray traced through the channel box, absorption, caustics | 1 texture sample (the channel stone), no screen or depth texture ([Water](water.md#shaders)) |
| `SH_WaterSimple` | mobile and web water (`MI_Water`) | blended; absorption to the floor height, caustics | no texture samples |
| `SH_Water.gdshaderinc` | included by the two above | ripple noise and slope, caustics, absorption, Fresnel | none |

`SH_CellHighlight` and `SH_Spark` run on Forward+, Mobile and
Compatibility (checked in Godot 4.4.1 on all three); the fire shaders on
all three in Godot 4.7.2, except `SH_HeatHaze`, which only the desktop
tiers show. `SH_TriplanarPBR` and
`SH_Gem` serve the desktop tiers on Forward+ only (checked in Godot 4.7.2);
mobile and web get the base materials. `SH_Dust` and `SH_LightShaft`
compile on all three renderers in Godot 4.7.2; `SH_AtmosphereFog` only
on Forward+ and Mobile, so it is loaded only on tiers with volumetric fog.
`SH_Water` serves the desktop tiers on Forward+; `SH_WaterSimple` runs on
Mobile and Compatibility (both checked in Godot 4.7.2).

## Budgets and the temple theme's numbers

Budgets from the project profile: piece < 2,000 triangles, board < 20,000,
textures ≤ 1024² for everything mobile and web ship; desktop `hi/`
variants up to 2048² ([Visual target](visual-target.md#content-budgets-per-tier)).

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
calls), plus shadow passes; pieces share 7 materials. On `mobile` and
`web` the pieces and markers are batched to one draw per piece kind and
marker ([Board view](board-view.md#draw-calls)), and `web` draws its sun
shadow in one cascade. Textures: seven base sets at 1024² (`Stone_01` with `_H`;
22 PNGs, 15 MB generated) and 2048² `hi/` sets for the hero surfaces
`Stone_01` and `Gold_01` (7 PNGs, 23 MB generated); all are VRAM-compressed
on import ([Import settings](#import-settings)). In VRAM on desktop
(BC1/BC4 about 0.7 and RGTC about 1.3 bytes per pixel with mipmaps; ETC2
and EAC on mobile are the same sizes) the base sets take about 20 MB and
the `hi/` sets add about 25 MB, well inside every tier's texture budget.
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
- Android, iOS and Web export presets list `*/hi/*` in `exclude_filter`;
  desktop presets do not.
- `PROVENANCE.md` has a record for each theme.
- Every board and backdrop OBJ has a committed `.obj.import` with
  `generate_lightmap_uv2=true`, and each theme has
  `game/assets/lookdev/LD_<Theme>_Bake.tscn`.
- `design/tokens/temple-board.json` piece colours match the theme parameters (catches a hand edit of the generated file).

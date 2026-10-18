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
| `game/assets/materials/shaders/` | `SH_*.gdshader` and `SH_*.gdshaderinc`, shared by all themes |
| `game/assets/textures/<theme>/` | `T_<Set>_{BC,N,ORM,H}.png` base texture sets, at most 1024²; build outputs, ignored by git (see below) |
| `game/assets/textures/<theme>/hi/` | desktop variants of the hero sets, same file names, at most 2048²; build outputs too |
| `game/assets/lookdev/` | `LD_*.tscn` look-dev scenes (game camera, line-up) |
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
sizes. The web tier's `lo/` folder is not used by the temple: its base
sets fit the web texture budget.

| Material | Base (mobile, web) | `hi/` (desktop) |
|---|---|---|
| Stone (`MI_Stone*`) | `ORMMaterial3D`, 1024² `Stone_01` set, normal map, triplanar | `SH_TriplanarPBR`: 2048² `Stone_01` set with parallax occlusion from `_H` |
| Gold (`MI_Gold`) | `ORMMaterial3D`, 1024² `Gold_01`, metallic 1, anisotropy | same with the 2048² set |
| Ruby, amethyst | `ORMMaterial3D`, `Gem_01` tinted, opaque, Fresnel from the real index of refraction, rim | `SH_Gem`: refraction of the scene behind with dispersion and absorption by the body colour |
| Carnelian, jade, moonstone | `ORMMaterial3D` with backlight for cheap translucency | sub-surface scattering with a transmittance colour; moonstone adds a stronger clearcoat |
| Lapis | `ORMMaterial3D`, `Lapis_01` (blue, calcite, metallic pyrite flecks) | none; the base is used everywhere |

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
| `python3 tools/assets/gen_theme_scenes.py [params]` | `MI_*.tres` and `hi/MI_*.tres`, `PF_*.tscn`, `LD_*.tscn`, the theme's manifest entry, `design/tokens/<theme>-board.json` | standard library |
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
| `textures.size`, `textures.hi_size` | base and desktop texture edge in pixels (powers of two, ≤ 1024 and ≤ 2048) |
| `textures.sets.<Set>` | one PBR set `T_<Set>_*`: `recipe` (a function in `gen_theme_textures.py`: `stone`, `ground`, `gold`, `gem`, `lapis`, `jade`, `moonstone`), `normal_strength` (slope at 1024 px), `hi` (also write a `hi/` set at `hi_size`; the base is filtered down from it), `height` (also write `_H`) |
| `materials.<MI_name>` | `albedo` (the colour the kind reads as; also the HUD token), optional `albedo_tint` (multiplies the texture instead, for sets that carry their own colour), `set` (a texture set: makes an `ORMMaterial3D` whose roughness, metallic and occlusion come from `_ORM`) with `triplanar_scale`, `normal_scale`, `world_triplanar`; without `set`, `metallic` and `roughness`; `ior` (index of refraction; sets the dielectric reflectance) or `specular`; `emission` + `emission_energy`, `rim` + `rim_tint`, `clearcoat` + `clearcoat_roughness`, `anisotropy`, `subsurface` (`strength`, `transmittance`, `depth`, `boost`), `backlight`, `double_sided`. Or a ShaderMaterial: `shader` + `params`, optional `textures` (uniform to a path under `textures/<theme>/`; the `hi` variant takes the file's `hi/` copy), `noise_textures` (uniform to a `NoiseTexture2D`: `seed`, `frequency`, `size`) and `render_priority` (with `set`, the shader gets `texture_albedo`, `texture_normal`, `texture_orm`, `texture_height`, `albedo`, `triplanar_scale`, `normal_scale`). `hi`: keys that replace these for the desktop variant in `hi/` |
| `environment` | sky colours, `ambient_energy`, `exposure`, `fog`, `fog_density`, `saturation`, `sun` (`transform` as written in a `.tscn`, basis rows first; `color`, `energy`), `torch` (`color`, `energy`, `range` of each brazier's fire light) |
| `lookdev` | game camera `camera_transform` and `fov`, `highlight_cell`; `fire_camera_transform` and `fire_fov` for the brazier close-up `LD_<Theme>_Fire` |
| `fire` | brazier fire: `rim_height` and the `flame`, `smoke`, `embers`, `light` and `haze` settings of `PF_BrazierFire` ([Fire](fire.md#pf_brazierfire)) |
| `surface_tokens`, `extra_tokens` | palette entries exported to `design/tokens/<theme>-board.json`: token name to material (its `albedo`) or to a hex value |

## Shaders

| Shader | Used by | Render mode | Cost |
|---|---|---|---|
| `SH_Fire` | brazier flames (`MI_Fire`) | unshaded, additive, no depth write; motion-vector flipbook, blackbody colour, camera-facing billboard | 4 texture and 1 depth sample; see [Fire](fire.md#shaders) |
| `SH_FireSmoke` | brazier smoke (`MI_FireSmoke`) | lit, alpha-blended, no depth write | 1 texture and 1 depth sample |
| `SH_Ember`, `SH_EmberTrail` | embers (`MI_Ember`, `MI_EmberTrail`) | unshaded, additive; trails on tiers with `particle_trails` | no texture samples |
| `SH_HeatHaze` | heat haze (`MI_HeatHaze`), desktop | screen-texture distortion, drawn before the flame | 2 noise and 1 screen sample |
| `SH_CellHighlight` | `cell_highlight` | unshaded, additive | one 1×1 quad; no texture samples |
| `SH_Spark` | `fx_match` | unshaded, additive; fades by `INSTANCE_CUSTOM.y` | 16 tiny spheres per burst |
| `SH_TriplanarPBR` | desktop stone (`hi/MI_Stone*`) | opaque, Burley diffuse, GGX; object-space triplanar PBR set; parallax occlusion on upward faces (`height_depth` metres, 8 to 24 layers by view angle) | 9 texture samples, plus up to 25 height samples on upward faces |
| `SH_Gem` | desktop ruby and amethyst (`hi/MI_Ruby`, `hi/MI_Amethyst`) | transparent pass (reads the screen texture), depth write; refraction with `ior` and `dispersion`, absorption by the body colour over `thickness` | 9 texture samples, 3 screen and 3 depth samples |
| `SH_Triplanar.gdshaderinc` | included by the two above | object-space triplanar sampling, normal-map blending, Y-face parallax | none of its own |

`SH_CellHighlight` and `SH_Spark` run on Forward+, Mobile and
Compatibility (checked in Godot 4.4.1 on all three); the fire shaders on
all three in Godot 4.7.2, except `SH_HeatHaze`, which only the desktop
tiers show. `SH_TriplanarPBR` and
`SH_Gem` serve the desktop tiers on Forward+ only (checked in Godot 4.7.2);
mobile and web get the base materials.

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
the whole scene stays near 31k triangles. Draw calls: one per piece (64),
6 board parts, 6 backdrop parts, the brazier fires (11 draw calls), plus shadow passes; pieces share 7
materials. Textures: seven base sets at 1024² (`Stone_01` with `_H`;
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
- `design/tokens/temple-board.json` piece colours match the theme parameters (catches a hand edit of the generated file).

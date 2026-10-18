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
| `game/assets/materials/<theme>/` | `MI_*.tres` materials |
| `game/assets/materials/shaders/` | `SH_*.gdshader`, shared by all themes |
| `game/assets/textures/<theme>/` | `T_*_{BC,N,R}.png` |
| `game/assets/lookdev/` | `LD_*.tscn` look-dev scenes (game camera, line-up) |
| `game/assets/PROVENANCE.md` | one record per theme or asset family |

| Prefix / suffix | Meaning |
|---|---|
| `SM_` | static mesh (OBJ, one material each) |
| `PF_` | asset scene the manifest points at |
| `LD_` | look-dev scene, never shipped from code |
| `MI_` | material |
| `SH_` | shader |
| `T_` + `_BC` / `_N` / `_R` | texture: base colour (sRGB) / normal (OpenGL, green up) / roughness (linear, red) |

Names use `A-Z a-z 0-9 _` only. Code refers to logical names in the
manifest, never to these paths.

## Generators

Run from the repository root, in this order. Same parameters and seed give
byte-identical files.

| Command | Writes | Needs |
|---|---|---|
| `python3 tools/assets/gen_theme_meshes.py [params]` | `SM_*.obj`, prints triangle counts, fails over budget | Blender 4.2+ as `bpy` (`pip install bpy`) or `blender -b --factory-startup -P <script> -- [params]` |
| `python3 tools/assets/gen_theme_textures.py [params]` | `T_*.png` (tiling) | numpy, Pillow |
| `python3 tools/assets/gen_theme_scenes.py [params]` | `MI_*.tres`, `PF_*.tscn`, `LD_*.tscn`, the theme's manifest entry, `design/tokens/<theme>-board.json` | standard library |
| `python3 tools/assets/validate_assets.py` | nothing; exits 1 on errors | Pillow |

`params` defaults to `tools/assets/themes/temple.json`.

### Parameter file

| Key | Meaning |
|---|---|
| `theme`, `seed`, `cell_size` | theme id (folder and manifest key), the one seed every random choice derives from, cell size |
| `board` | `width`, `height`, `tile_gap`, `tile_height`, `frame_width`, `frame_height` |
| `artifacts.piece_width` | piece footprint as a fraction of a cell |
| `artifacts.kinds[]` | `index`, `name` (file names), `shape` (a function in `gen_theme_meshes.py`: `round_gem`, `pyramid`, `sun`, `leaf`, `diamond`, `hex_crystal`, `crescent`), `material` |
| `backdrop` | `tiers`, `foliage_clumps`, `ground_size` |
| `textures.size` | texture edge in pixels (power of two, ≤ 1024) |
| `materials.<MI_name>` | StandardMaterial3D: `albedo`, `metallic`, `roughness`, `specular`, `emission` + `emission_energy`, `rim` + `rim_tint`, `clearcoat`, `backlight`, `double_sided`, `albedo_texture` / `normal_texture` / `roughness_texture` with `triplanar_scale`, `normal_scale`, `world_triplanar`. Or ShaderMaterial: `shader` + `params` |
| `environment` | sky colours, `ambient_energy`, `exposure`, `fog`, `fog_density`, `saturation`, `sun` (`transform` as written in a `.tscn`, basis rows first; `color`, `energy`), `torch` (`color`, `energy`, `range`) |
| `lookdev` | game camera `camera_transform` and `fov`, `highlight_cell` |
| `surface_tokens`, `extra_tokens` | palette entries exported to `design/tokens/<theme>-board.json`: token name to material (its `albedo`) or to a hex value |

## Shaders

| Shader | Used by | Render mode | Cost |
|---|---|---|---|
| `SH_Flame` | brazier flames | unshaded, additive, no depth write; vertex wobble by height | 12 small cones; no texture samples |
| `SH_CellHighlight` | `cell_highlight` | unshaded, additive | one 1×1 quad; no texture samples |
| `SH_Spark` | `fx_match` | unshaded, additive; fades by `INSTANCE_CUSTOM.y` | 16 tiny spheres per burst |

All three run on Forward+, Mobile and Compatibility (checked in Godot
4.4.1 on all three).

## Budgets and the temple theme's numbers

Budgets from the project profile: piece < 2,000 triangles, board < 20,000,
textures ≤ 1024².

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
| Backdrop (platform, braziers, ground, ferns, rocks) | 4,748 |

A full 8×8 board of pieces averages about 260 triangles each (~17k), so
the whole scene stays near 31k triangles. Draw calls: one per piece (64),
6 board parts, 7 backdrop parts, plus shadow passes; pieces share 7
materials. Textures: two sets of three 512² PNGs
(1.8 MB on disk, VRAM-compressed on import). Frame time on target phones
is not measured yet; that is WP-41.

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
- PNGs are power-of-two, ≤ 1024 px; `_N` maps are RGB.
- `PROVENANCE.md` has a record for each theme.
- `design/tokens/temple-board.json` piece colours match the theme parameters (catches a hand edit of the generated file).

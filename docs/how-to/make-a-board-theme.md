<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Make a board theme

*How-to. Make a new look for the board (for a mini game, a season, an
episode) or change the temple's. Rules and file formats:
[Board themes](../reference/board-themes.md).*

You need Python 3 with `bpy` (Blender as a module), numpy and Pillow:

```sh
python3 -m pip install bpy numpy pillow
```

With a Blender install instead of `bpy`, run the mesh step as
`blender -b --factory-startup -P tools/assets/gen_theme_meshes.py -- <params>`.

## Change the temple

1. Edit `tools/assets/themes/temple.json`: colours under `materials`, light
   under `environment`, sizes under `board` and `artifacts`.
2. Regenerate and check:

   ```sh
   python3 tools/assets/gen_theme_meshes.py
   python3 tools/assets/gen_theme_textures.py
   python3 tools/assets/gen_theme_scenes.py
   python3 tools/assets/validate_assets.py
   ```

   The textures are not committed. In a fresh checkout run
   `gen_theme_textures.py` (or build the CMake target `textures`) before
   opening the project in Godot; it does nothing when its inputs have not
   changed. It also writes each texture's Godot import settings
   (mipmaps, compression); leave the editor's Import dock alone for them.

3. Open `game/assets/lookdev/LD_Temple.tscn` (whole board from the game
   camera) and `LD_Temple_LineUp.tscn` (pieces close up) in Godot, or render
   them headless:

   ```sh
   godot --path game --write-movie /tmp/look.png --fixed-fps 30 --quit-after 10 \
     res://assets/lookdev/LD_Temple.tscn
   ```

   Check it in greyscale too: every kind must still be told apart.
   `LD_Temple_Hi.tscn` and `LD_Temple_LineUp_Hi.tscn` show the desktop
   materials; render them with the Forward+ renderer (`--rendering-driver
   vulkan`) and compare them with the base scenes.
4. Commit the parameter file and every regenerated file together.

## Add a texture set

1. Add a recipe to `tools/assets/gen_theme_textures.py`: a function
   `(rng, n)` that returns base colour, height, occlusion, roughness and
   metallic arrays for an `n`×`n` tile. Build it from the periodic helpers
   (`fbm`, `voronoi`, `warp`, `blur`, `cavity`, `leaves`) so it tiles. Keep base colour free of
   light and shadow; put roughness variation where the surface story is.
   Register it in `RECIPES`.
2. Add the set under `textures.sets` in the parameter file, for example
   `"Shell_01": {"recipe": "shell", "normal_strength": 1.0}`. Add
   `"hi": true` for a hero surface that needs a 2048² desktop set and
   `"height": true` if a parallax shader will read `_H`.
3. Point a material at it with `"set": "Shell_01"`, then regenerate and
   validate. A value error (`dielectric base colour spans ...`, `metallic
   values are between ...`) means the recipe's numbers are off, not the
   validator.

## Give a material a desktop variant

Add a `hi` object to the material with only the keys that change on
desktop. For example, sub-surface scattering on a translucent stone:

```json
"MI_Jade": {
  "albedo": "#2b9a57", "set": "Jade_01", "backlight": "#1d6b3a",
  "hi": {"subsurface": {"strength": 0.8, "transmittance": "#5fd38a", "depth": 0.25}}
}
```

or a custom shader: `"hi": {"shader": "res://assets/materials/shaders/SH_Gem.gdshader", "params": {...}}`.
`gen_theme_scenes.py` writes `materials/<theme>/hi/<name>.tres` and uses
`hi/` textures where the set has them. Keep the base material good on its
own: mobile and web never see the variant.

## Add a new theme

1. Copy the parameter file: `cp tools/assets/themes/temple.json
   tools/assets/themes/<theme>.json` and set `"theme": "<theme>"` and a new
   `seed`.
2. Restyle it. Keep seven distinct silhouettes; pick from the `shape`
   functions or add one to `gen_theme_meshes.py` (a function that fills a
   bmesh for a piece of radius `r` and returns its smoothing angle).
3. Point the materials' texture paths at `res://assets/textures/<theme>/`,
   or reuse the temple's.
4. Run the three generators with your file as the argument, then the
   validator. Your theme is added to `manifest.json` next to the others.
5. Add a provenance record for `theme <theme>` to
   `game/assets/PROVENANCE.md` (copy the temple's and edit it).
6. Select it in a scene by setting the board's `theme` property to
   `<theme>`; leave it empty to use `default_theme`.

## Fix a validator error

Each error names the file and the rule. Over budget: lower the segment
counts in the shape function or the bevels. Missing reference: re-run
`gen_theme_scenes.py`. Missing provenance: add the record. Missing or
wrong `.import` for a generated texture: re-run `gen_theme_textures.py`;
to change an import setting, edit `tools/assets/texture_import.py`. For a
hand-made texture under `game/assets/textures/`, import it once in Godot
with the settings in
[Import settings](../reference/board-themes.md#import-settings), then
commit its `.import` with a `!` line in `.gitignore`. Never hand-edit
generated `.obj`, `.tres` or `.tscn` files; change the parameters or the
generator, or your edit is lost on the next run.

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

3. Open `game/assets/lookdev/LD_Temple.tscn` (whole board from the game
   camera) and `LD_Temple_LineUp.tscn` (pieces close up) in Godot, or render
   them headless:

   ```sh
   godot --path game --write-movie /tmp/look.png --fixed-fps 30 --quit-after 10 \
     res://assets/lookdev/LD_Temple.tscn
   ```

   Check it in greyscale too: every kind must still be told apart.
4. Commit the parameter file and every regenerated file together.

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
`gen_theme_scenes.py`. Missing provenance: add the record. Never hand-edit
generated `.obj`, `.tres` or `.tscn` files; change the parameters or the
generator, or your edit is lost on the next run.

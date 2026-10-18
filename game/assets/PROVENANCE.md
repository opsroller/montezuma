<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Asset provenance

Where every asset in `game/assets/` came from and why we may ship it.
Shared and append-only: add your record, don't rewrite others'. Format and
rules: `docs/reference/board-themes.md` (assets) and the project profile
(Provenance). No record, no asset.

## Board theme `temple`

```yaml
asset: themes.temple (artifact_0..6, board, environment, cell_highlight, fx_match)
files:
  - game/assets/models/temple/**        # SM_*.obj meshes, PF_*.tscn scenes
  - game/assets/materials/temple/*.tres
  - game/assets/materials/shaders/SH_Flame.gdshader
  - game/assets/materials/shaders/SH_CellHighlight.gdshader
  - game/assets/materials/shaders/SH_Spark.gdshader
  - game/assets/textures/temple/T_*.png
  - game/assets/lookdev/LD_Temple*.tscn
origin: generated
authors: [technical-artist (Claude), graphic-designer (Claude)]
created: 2026-10-18
source:
  - tools/assets/gen_theme_meshes.py     # Blender 5.2 LTS (bpy module), headless
  - tools/assets/gen_theme_textures.py   # numpy + Pillow
  - tools/assets/gen_theme_scenes.py     # writes Godot text resources
  - game/assets/materials/shaders/*.gdshader (hand-written)
generator:
  params: tools/assets/themes/temple.json
  seed: 1517
  version: the commit that last changed tools/assets/
inputs: []                               # no scans, photos, fonts or third-party files
license: GPL-2.0-or-later
attribution: ""
changes: ""
notes: >
  All shapes, colours and patterns are drawn from scratch by the generators.
  The step-fret border and stepped-temple forms are generic Mesoamerican
  motifs; nothing is copied, traced or extracted from The Treasures of
  Montezuma or any other game. Regenerate with the commands in
  docs/how-to/make-a-board-theme.md.
```

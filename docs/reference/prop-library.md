<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Prop library

*Reference. The shared library of set dressing a level draws from: its files,
names, catalogue, tags, budgets and materials. Adding one:
[Add a prop](../how-to/add-a-prop.md). The board, the pieces and the
environment are not props; they belong to a board theme
([Board themes](board-themes.md)). What the stone and the paint are copied
from, and why: [Mesoamerican reference](mesoamerican-reference.md).*

## What the library is for

The game is not one temple with ten levels. It grows into many levels across
locations, cultures and eras, so the art is a **permanent, growing library**
rather than a set of props for the scene in front of us. Three rules follow:

1. **A prop stays.** Nothing is deleted because no level currently places it.
2. **A level picks by tag, not by path.** No level, and no game code, names a
   prop file. A level names what it wants — a mossy limestone boulder for a
   rainforest ruin — and the catalogue answers.
3. **Everything is generated.** One generator, one seed, one file; a prop can
   always be rebuilt, and its provenance is the script.

## Files

| Path | Contents |
|---|---|
| `tools/assets/props.py` | every generator, the catalogue table, the material table, the look-dev pages |
| `tools/assets/themes/props.json` | the library's **texture sets**. Not a board theme: it has no board, pieces, environment or manifest entry, and `gen_theme_scenes.py` is never run on it. It lives in `themes/` only so `gen_theme_textures.py`, which builds every file there, makes these sets from CMake and CI with no extra wiring |
| `game/assets/models/props/<family>/SM_Prop_<Name>.obj` | one mesh per (prop, material) |
| `game/assets/models/props/<family>/PF_Prop_<Name>.tscn` | the scene a level instances |
| `game/assets/materials/props/MI_*.tres` | the library's materials; `hi/` holds the desktop variants |
| `game/assets/textures/props/T_*.png` | the texture sets; **build outputs**, ignored by git, same as a theme's |
| `game/assets/lookdev/LD_Props_*.tscn` | one look-dev page per family, and a `_Top` twin from the game camera |
| `game/assets/library.json` | the catalogue |

Prefixes and texture-map suffixes are the project's
([Board themes, Folders and naming](board-themes.md#folders-and-naming)):
`SM_` mesh, `PF_` scene, `MI_` material, `T_…_{BC,N,ORM,H}` texture maps.
A prop's parts are `SM_Prop_<Name>.obj` for its main body and
`SM_Prop_<Name>_<Part>.obj` for each further material (`_Moss`, `_Wet`,
`_Water`, `_Relief`, `_Stucco`, `_Fall`, `_Foam`, `_Spill`, `_Roots`, `_Ash`).

Families, which are only a filing and look-dev grouping: `rock`, `ruin`,
`carved`, `ritual`, `vessel`, `water`, `organic`.

## Space, scale and footing

Geometry is authored directly in Godot space, as the board is: **metres, Y up,
+Z toward the player**, 1 unit = 1 m. A prop's origin is the centre of its
ground contact and its lowest point is at y = 0, so a level drops it on
terrain at y = 0 and turns it about Y.

`footing` says how it meets the ground:

| `footing` | Place it like this |
|---|---|
| `sits` | on the terrain, base at y = 0 |
| `embedded` | sunk by `sink_m`, so it reads as part of the ground rather than resting on it |
| `wall_tenon` | its tenon goes into a wall; it is not a free-standing placement |

`yaw_free` is true when any rotation about Y is as good as any other. It is
false for anything with a front: a stela, a stair, a serpent head, a channel.

## The catalogue

`game/assets/library.json`, written by `tools/assets/props.py`. One entry per
prop, keyed by its id:

```json
"boulder_limestone_ma": {
  "scene": "res://assets/models/props/rock/PF_Prop_BoulderLimestone_MA.tscn",
  "family": "rock", "kind": "boulder",
  "culture": ["maya-lowland", "neutral"],
  "era": ["natural"],
  "biome": ["rainforest", "any"],
  "tags": ["natural", "rounded"],
  "surface": "dry",
  "size_class": "medium", "size_m": [1.09, 0.71, 0.95],
  "footing": "embedded", "sink_m": 0.07,
  "yaw_free": true,
  "triangles": 192,
  "materials": ["MI_Limestone"],
  "seed": "props:boulder_limestone_ma"
}
```

The file also carries a `vocabulary` block with every allowed value, so a
query can be validated rather than silently matching nothing, and a
`footing_meaning` block.

| Key | Values |
|---|---|
| `family` | `rock`, `ruin`, `carved`, `ritual`, `vessel`, `water`, `organic` |
| `kind` | what the thing *is*: `boulder`, `outcrop`, `rubble`, `scatter`, `facing_stone`, `wall`, `corner`, `column`, `stair`, `balustrade`, `stela`, `altar`, `sculpture`, `serpent_head`, `ball_court_ring`, `glyph_block`, `brazier`, `censer`, `bowl`, `jar`, `sherds`, `basin`, `drain`, `channel`, `pool`, `cascade`, `waterfall`, `log`, `roots` |
| `culture` | `neutral` (geology, water, dead wood), `maya-lowland`, `mexica-highland` |
| `era` | `natural`, `classic`, `postclassic`, `any` |
| `biome` | `any`, `rainforest`, `highland` |
| `surface` | `dry`, `mossy` |
| `size_class` | `tiny` ≤ 0.3 m, `small` ≤ 1 m, `medium` ≤ 2.5 m, `large` ≤ 6 m, `huge` above, by longest side |

`culture` and `era` are deliberately separate from `biome`: a limestone
boulder belongs to no culture and to the rainforest, while a serpent-head
block belongs to the Mexica and to whichever place a level puts them in.

### Selecting props for a level

> **Provisional.** The owner is writing the library's catalogue schema and the
> per-level seeded selection contract into `docs/reference/asset-library.md`.
> Until that page lands this is the rule the generator and this page assume,
> and it is written here so the two can be compared and reconciled.

A level names a **query** and a count, never a path:

- A prop matches a query key when the query's values and the prop's share at
  least one value. A prop tagged `any` matches every value of that key.
- A key the query leaves out does not filter.
- The matches are sorted by id, which gives a stable order independent of
  file-system order and of how many props the library has gained since.
- The level draws from that sorted list with a generator seeded from the
  level's own seed and the query's index in the level file, so a level dresses
  itself identically on every machine and every run, and gaining a new prop
  changes a level's dressing only where that prop matches its query.

Nothing in `core/` or `extension/` may name a prop path; the catalogue is the
only way in, the way `game/assets/manifest.json` is for a board theme.

## Materials and texture sets

Every stone material maps **triplanar in world space**. There are no UVs
anywhere in the library, and world-space mapping also means two instances of
one prop standing a metre apart sample different parts of the tile, so a row
of the same rock does not read as a row of the same rock.

| Material | Texture set | Used for |
|---|---|---|
| `MI_Limestone`, `MI_LimestoneWarm`, `MI_LimestoneGrey` | `Limestone_01` (has a 2048² `hi/`) | the Maya lowland set: boulders, masonry, stelae, stairs |
| `MI_Basalt` | `Basalt_01` | Mexica sculpture, altars, braziers |
| `MI_Andesite` | `Andesite_01` | fine Mexica sculpture (chacmool, standard bearer) |
| `MI_Tezontle` | `Tezontle_01` | red highland scoria |
| `MI_Stucco` | `Stucco_01` | lime plaster over masonry |
| `MI_Polychrome` | `Polychrome_01` | paint over plaster, worn |
| `MI_Bronze` | `Bronze_01` | metal fittings; patina is a dielectric, the bare metal is metallic 1 |
| `MI_Clay` | `Clay_01` | censers, jars, sherds, ash |
| `MI_Moss` | `Moss_01` | the moss shell of a `mossy` variant |
| `MI_WetStone` | `WetStone_01` | stone the water runs over |
| `MI_Water`, `MI_WaterFall` | `Water_01` | still and falling water |
| `MI_Foam` | — | broken water at the foot of a fall |
| `MI_Bark` | `Bark_01` | fallen logs and roots |
| `MI_Plaza` | `Plaza_01` | look-dev only: the floor the pages stand on |

The recipes are in `tools/assets/gen_theme_textures.py` and are procedural
throughout: noise, Voronoi cells, domain warping and stamped shapes computed
by the script. No photograph, scan or material library is used, traced or
sampled. Which weathering to model comes from
[Mesoamerican reference](mesoamerican-reference.md) and
[Art direction](../explanation/art-direction.md).

### Dry and mossy

A prop that can take moss gets a second catalogue entry whose scene is the
same stone meshes plus one extra mesh: a shell of the faces moss would hold
— the upward-facing ones, in patches, lifted about a centimetre off the stone
so it reads as a mat and not a tint. The two variants therefore **share their
stone meshes in memory** and the mossy one costs one extra draw call.

## Budgets

The project's [budgets](project-profile.md#budgets) and the per-tier
[content budgets](visual-target.md#content-budgets-per-tier) bind the frame,
not the prop: 200k triangles and 300 draw calls in a whole visible frame on
`mobile`, 150k and 200 on `web`. A level that dresses a board with about forty
props therefore has roughly 2k triangles and two draw calls each to spend, and
the library's own caps sit inside that:

| `size_class` | Longest side | Triangles, LOD0, all parts |
|---|---|---|
| `tiny` | ≤ 0.3 m | 200 |
| `small` | ≤ 1 m | 600 |
| `medium` | ≤ 2.5 m | 1,600 |
| `large` | ≤ 6 m | 3,200 |
| `huge` | above | 6,000 |

`tools/assets/props.py` prints each prop's measured count and class and exits
non-zero when one is over its cap.

**LODs come from the importer.** Godot's Wavefront OBJ importer has
`generate_lods` on by default, so every prop mesh gets its LOD chain and its
shadow mesh built at import. The library does not commit `.obj.import` files
and does not author LOD meshes by hand; a prop expensive enough to need an
authored chain would be a reason to split it, not to hand-build one.

## Open items

These are known gaps, not oversights:

- **Not yet verified in the engine.** The generators, the texture recipes and
  the look-dev pages have not been run: the session that wrote them had no
  working shell (see the hand-off notes). Nothing here may be treated as
  measured until `tools/assets/props.py` and `validate_assets.py` have run and
  the look-dev pages have been captured.
- **Water is opaque.** The shared material writer
  (`gen_theme_scenes.py::material`) has no transparency key, and the prop
  library does not own that file, so depth colour comes from a dark body
  colour and a near-mirror roughness rather than from alpha. Real depth,
  refraction and caustics need the water shader the
  [visual target](visual-target.md#fire-water-and-particles) asks for.
- **Props are not lightmapped.** A theme's static meshes carry
  `generate_lightmap_uv2=true` in a committed `.obj.import`, and only the
  board and the backdrop are in the bake scene. Whether props join the bake —
  and so pay for a UV2 unwrap each — depends on how a level composes its
  scene, which is not decided yet.
- **No MultiMesh.** A level that places many copies of one rock pays a draw
  call each. If a level's draw-call count becomes the binding budget, batching
  identical props is the first thing to do.
- **The selection contract is provisional** (see above).

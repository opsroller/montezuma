<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# Asset library

The scenery a level is dressed with — plants, props, carved stone, water —
comes from one growing library, not from a scene built by hand per level.
Entries stay in the library whether or not a level currently uses them:
the library is meant to outlive any single theme, and to carry more
locations, cultures and periods as the game grows.

Related: [Board themes](board-themes.md) for the board itself,
[Visual target](visual-target.md) for the look each tier must hit,
[Mesoamerican reference](mesoamerican-reference.md) for what this first
setting is built from.

## Where it lives

| Path | Holds |
|---|---|
| `tools/assets/plants.py` | plant generators, one function per species |
| `tools/assets/props.py` | everything that is not a plant: stone, sculpture, water, debris |
| `tools/assets/library/*.json` | catalogue entries, one file per pack |
| `game/assets/library/<pack>/` | generated meshes, LODs and textures |

A pack is a set that belongs together — `temple-lowland`, say — and a
level names packs, never individual files.

## Catalogue entry

Every generator has one entry. Fields:

| Field | Meaning |
|---|---|
| `id` | stable, unique, lowercase: `plant.heliconia.tall` |
| `kind` | `plant`, `prop`, `architecture`, `water`, `terrain` |
| `name` | what it is, in plain words |
| `reference` | the real species, object or building practice it is drawn from, with its source |
| `culture`, `era`, `biome` | what it belongs to; `null` where it is neutral |
| `band` | where it sits: `groundcover`, `understory`, `mid`, `canopy`, `epiphyte`, `vine`, `ground`, `wall`, `water`, `focal` |
| `height_m` | `[min, max]` of the real thing |
| `variants` | how many distinct builds the generator makes |
| `generator` | `module:function`, called with `(bm, rng, x, y, z, params)` |
| `textures` | texture sets it needs |
| `tris` | triangle count per LOD, highest first |
| `tags` | free-form: `wet`, `mossy`, `ruined`, `painted`, `shade`, `edge` |

Anything an artist had to assume rather than source goes in `reference`
as an assumption, not a citation.

## How a level is dressed

A level's data gives a biome, the packs it draws from and a seed. The
scatter then places entries itself; no level stores a list of props.

```json
"scenery": { "packs": ["temple-lowland"], "biome": "lowland-rainforest",
             "seed": 4817, "density": 1.0 }
```

Rules the scatter keeps to:

- Deterministic: the same level id and seed give the same planting every
  time, so a level is stable unless it asks for variation. A level may
  set `"seed": "random"` to be dressed differently on each load.
- Band densities come from the biome, not the level: groundcover is
  dense, canopy is sparse, and each band has a radius range from the
  board.
- Nothing is placed over the board rectangle or the stair approach, and
  the readability rules in [Visual target](visual-target.md) decide what
  may stand near the camera.
- Entries are weighted by `tags` matching the level's biome and mood, so
  a wet level leans on `wet` and `mossy` variants of the same species.
- Per-instance variation (scale, yaw, lean, tint) is applied by the
  scatter, so one generator never reads as a repeated object.

## Budgets

The caps in [Project profile](project-profile.md#budgets) hold per tier:
the scatter drops bands and swaps LODs to stay inside them rather than
refusing to place. A pack states its own worst-case cost so a level can
be checked before it ships.

## Adding to the library

Add the generator, add its catalogue entry, generate its meshes and
textures, render it at real size in the engine and look at it, record
where it came from in `game/assets/PROVENANCE.md`, and write its
how-to row. An entry with no render and no provenance is not in the
library.

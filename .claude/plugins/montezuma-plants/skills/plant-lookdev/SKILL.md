---
name: plant-lookdev
description: Build, render and judge the jungle plant library (tools/assets/plants.py) at 3840x2160. Use when changing a plant species, a leaf atlas, a band's colours or the shedding rules, or when asked how a plant or a band currently looks.
---

# Plant look-dev loop

The library is judged by eye on rendered frames, never from the code.

## First: be in the environment

```sh
direnv allow     # once per checkout
```

`.envrc` is `use flake` then `layout pipenv`: the flake delivers godot, blender,
gamescope, python3 and the native libraries the wheels link against, and the Pipfile
delivers numpy and Pillow on top. Inside it, `python3` has what the tools need and
**nothing takes an interpreter path in front of it**.

A shell that shows no prompt never fires direnv's hook, so a tool-spawned shell can
land outside the environment. If `import numpy` fails, that is why — load it, don't
paper over it with a path.

## The loop

```sh
python3 tools/assets/lookdev.py all --species heliconia
python3 tools/assets/lookdev.py capture --scope rows
python3 tools/assets/lookdev.py check
```

`lookdev.py` is the pipeline: `params`, `atlases`, `build`, `pages`, `import`,
`check`, `capture`, `sheet`, `all`. It holds the things that have exactly one right
answer — 3840×2160 captures, `.scratch/` for output, the GPU lock derived from
`git rev-parse --git-common-dir`, and `import` before any capture — so none of them
is retyped or forgotten.

| Step | Why |
|---|---|
| `params` | writes `themes/plants.json`; must precede `atlases`, which read it |
| `atlases` | the 36 leaf and bark atlases at 4096. ~3 min changed, instant otherwise |
| `build` | meshes, materials and the plant section of `game/assets/library.json` |
| `pages` | a close-up page per specimen with its empty companion, plus band rows |
| `import` | **required** after any mesh or texture change |
| `check` | mesh faults a frame cannot show |
| `capture` | 3840×2160 into `.scratch/lookdev` |
| `sheet` | labelled contact sheets of the captures |

Blender runs as the flake's pinned binary — the version is part of the asset, since
two committed meshes do not reproduce byte for byte across versions:

```sh
blender -b --factory-startup -P tools/assets/plants.py -- build --only heliconia
```

## Traps that cost hours

- **Render after import, not before.** Godot serves meshes from `.godot/imported`, so
  a rebuilt OBJ does not reach a frame until an import pass. A whole 66-page batch
  once rendered the previous meshes this way.
- **A plant rendering white or untextured**: check its `.import` for `valid=false`
  before anything else. Godot writes that when an import fails and then skips the
  file on every later pass, silently and permanently.
- **Judge on a close-up, not a band row.** A row is as wide as the sum of its
  spreads, so the understory row's camera stands 55 m back and a 1 m blade lands on
  40 pixels. Rows are for comparing species against each other, nothing else.
- **Read the set as contact sheets.** Nearly every fault here was found by seeing
  nine species at once — a white palm, a grass tuft in 124 pieces of litter, moss the
  colour of butter, a leaf floating 7 m above its plant — and none by looking at one
  specimen at a time.
- **Anything sized from `Species.height`/`spread` is sized from the species, not the
  variant.** The range covers every variant: monstera's runs to 12 m because one of
  three climbs. Measure the built geometry instead.
- **Don't poll a background job**, and don't hand-roll a waiter. A long step is one
  call that awaits its process.
- Scratch is `.scratch/` in the workspace, gitignored, safe to delete whole.

## Reading the measurements

`measure_foliage.py` prints each figure against
`docs/reference/foliage-reference.md` and marks FAIL. Treat a FAIL as a question,
then look at the frame — the numbers cannot see folded cardboard. Two are about the
page rather than the plant: `contact_shadow` depends on the environment's ambient and
sky fill, and `dead_area` counts the litter skirt the close-up page deliberately
frames.

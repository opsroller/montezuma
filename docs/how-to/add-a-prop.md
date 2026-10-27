<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Add a prop

Add one piece of set dressing — a rock, a ruined wall, a carved stone, a
vessel, a water feature — to the shared library levels draw from. Facts about
the library (files, tags, budgets, materials):
[Prop library](../reference/prop-library.md). This is not how you change the
board, the pieces or the environment; those belong to a board theme
([Make a board theme](make-a-board-theme.md)).

Everything is one file: `tools/assets/props.py`. A prop is a builder function
plus a row in the catalogue table. Nothing else in the repository changes.

## Before you start

```sh
python3 -m venv .venv                                     # once
.venv/bin/pip install --require-hashes -r .github/requirements/assets.txt
.venv/bin/python tools/assets/gen_theme_textures.py       # builds the props/ texture sets too
```

The prop generator itself needs only the standard library, so you can run it
with plain `python3`; the textures and the validator need numpy and Pillow.

## 1. Decide what it is before you model it

Write down, in this order, the three things the library needs and the
generator cannot guess:

- **What it is** (`kind`) and **who made it** (`culture`, `era`). Geology,
  water and dead wood are `neutral` and `natural`. Anything built or carved
  belongs to `maya-lowland` or `mexica-highland` and to an era — and the
  honest way to choose is
  [Mesoamerican reference](../reference/mesoamerican-reference.md), whose
  first section is exactly this decision. Do not mix the two registers in one
  prop; that pastiche is the thing the reference page exists to stop.
- **Where it grows or stands** (`biome`): `rainforest`, `highland`, or `any`.
- **How it meets the ground** (`footing`): see the table in the reference.
  A prop that `sits` with a flat base on uneven terrain will float; most
  natural things want `embedded`.

## 2. Write the builder

Add a function next to the others in the family's section of
`tools/assets/props.py`. It fills a `Build` with one mesh per material:

```python
def p_stone_marker(b, height=1.1, lithology="limestone"):
    """A low boundary marker: a dressed post with a plain capital."""
    rng, m = b.rng, Mesh()
    m.add(slab(0.30, height, 0.26, rng, radius=0.02, seg=3, relief=0.01, frequency=7.0))
    cap = slab(0.42, 0.14, 0.38, rng, radius=0.025, seg=2, relief=0.008, frequency=8.0)
    cap.transform(move=(0.0, height, 0.0))
    m.add(cap)
    b.add("Mesh", m.weld(), mat(lithology), smooth=26.0)
    b.footing, b.sink = "embedded", 0.06
```

Rules that keep the library consistent:

- **Build out of the shared primitives**, not out of bespoke geometry:
  `slab` (a weathered dressed block), `rock`, `masonry` (coursed blocks),
  `lathe`, `vessel` (anything hollow), `tube`, `torus`, `boss` (a panel of
  low relief), `grid_sheet` and `water_plane`. They are star-shaped about a
  reference point, so `orient_by` settles their winding and you cannot get
  an inside-out face.
- **Metres, Y up, +Z toward the player.** The origin is the centre of the
  ground contact and the lowest point is y = 0; `drop_to(0.0)` does that for
  you.
- **One `b.add` per material.** Each becomes its own OBJ and its own
  `MeshInstance3D`, because an OBJ carries one material.
- **Rotate and move in one `transform` call.** A move followed by a rotation
  swings the piece around the world origin instead of its own.
- **Take all randomness from `b.rng`.** It is seeded from the prop's id, so
  the same checkout gives byte-identical files.
- **Let the texture carry the small stuff.** Geometry carries what the
  silhouette shows — the broken corner, the bedding, the missing course.
  Chisel marks, pores and lichen are the texture's job. A bevelled box with a
  good stone texture still reads as a bevelled box, so put the irregularity
  where the outline is.

## 3. Add the catalogue row

In `library()`, in the right family block:

```python
items.append(P("stone_marker_a", "StoneMarker_A", "ruin", "stela", p_stone_marker,
               dict(height=1.1),
               culture=("maya-lowland",), era=("classic",),
               biome=("rainforest", "any"), tags=("carved",), yaw_free=False))
```

`kind`, `culture`, `era` and `biome` must come from the vocabularies in
`props.py`; the generator stops with a clear error if they do not. Set
`mossy=False` for anything moss would not take (fired clay, metal, water).

## 4. Generate, look at it, iterate

```sh
python3 tools/assets/props.py --list                 # the catalogue ids
python3 tools/assets/props.py --only stone_marker_a  # just yours, fast
python3 tools/assets/props.py                        # the whole library and the look-dev pages
```

It prints each prop's size class, measured triangle count and bounding size,
and exits non-zero if one is over its budget.

Then look at it in the engine, at real metre size, under the temple theme's
sun and sky. Never judge a prop from the numbers or from a mental picture:

```sh
printf '[display]\nwindow/size/window_width_override=1920\nwindow/size/window_height_override=1080\n' \
  > game/override.cfg
godot --headless --path game --import && git checkout game/project.godot
mkdir -p /tmp/props
flock /home/claude/work/montezuma/.godot-gpu.lock gamescope --backend headless -W 1920 -H 1080 -- \
  godot --path game res://assets/lookdev/LD_Props_Ruin1.tscn \
  --write-movie /tmp/props/f.png --fixed-fps 30 --quit-after 60
rm game/override.cfg
```

Frames land as `/tmp/props/fNNNNNNNN.png`; the last one is the one to look at.
Each family has a page and a `_Top` twin shot from the game's near-top-down
camera, which is the only view a player ever gets — check both. A 1.7 m scale
post stands at each end of every row.

What to look for, in this order:

1. **Scale against the post.** A prop that is the wrong size is wrong before
   anything else is worth judging.
2. **Silhouette.** Does the outline read as carved or weathered stone, or as a
   box with its edges taken off? If it is the box, the fix is in the geometry
   (more relief, a broken corner, a missing course), not in the texture.
3. **From the top camera.** Most of what you built is invisible from there.
   What is visible must still read.
4. **Value.** Squint, or look at the frame in greyscale: a prop that sits at
   the same value as the plaza floor disappears.

Importing rewrites `game/project.godot`, so `git checkout game/project.godot`
after any `--import`. `game/override.cfg` is git-ignored; delete it when you
are done.

## 5. Validate and record it

```sh
.venv/bin/python tools/assets/validate_assets.py     # exits 1 on any error
```

Then add the prop to the library's record in
`game/assets/PROVENANCE.md` if it needs anything the existing record does not
already cover (it covers everything generated by `props.py` from noise). Add a
new record only for something with a new kind of input — and the library takes
no inputs: no scans, photographs, material libraries or third-party files, and
nothing from the original game.

Update [Prop library](../reference/prop-library.md) if you added a vocabulary
value, a material, a texture set or a family.

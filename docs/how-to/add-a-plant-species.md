<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Add a plant species

*How-to. Add a species to the jungle plant library, or change one. What the
library holds and the rules it follows:
[Plant library](../reference/plant-library.md). Where the setting comes from:
[Mesoamerican reference](../reference/mesoamerican-reference.md).*

You need Python 3 with `bpy` (Blender as a module), numpy and Pillow:

```sh
python3 -m pip install bpy numpy pillow
```

With a Blender install instead of `bpy`, run the mesh steps as
`blender -b --factory-startup -P tools/assets/plants.py -- build`.

Everything happens in `tools/assets/plants.py` and, for the pixels, in the two
plant recipes in `tools/assets/gen_theme_textures.py`. Nothing else needs
editing: the generator writes the meshes, the materials, the manifest and the
look-dev scenes.

## 1. Settle the plant before writing any code

A generated plant that does not grow in the Petén lowlands is worse than no
plant: it is a visible error that survives into screenshots. Before you write
a generator, write down, and keep:

- **that it occurs here** — lowland Maya rainforest, 150–300 m. Check the
  elevation and the country records, not just the genus. Several obvious
  choices are montane (*Monstera deliciosa*), absent (*Socratea*) or
  post-contact (*Musa*); the [replacements
  table](../reference/plant-library.md#species-that-were-replaced-and-why)
  lists the ones already caught.
- **its proportions**, in metres: mature and juvenile height, stem diameter
  and whether the stem shows, leaf count, leaf length and width, petiole
  length, leaflet count and ratio, rosette or crown diameter, and the band it
  belongs to.
- **its one tell** at 5–15 m, the gameplay viewing distance. If you cannot
  name it, the species will not read and is not worth the triangles.
- **what you could not source.** Write it in `assumed`. An honest estimate
  recorded as an estimate is fine; an estimate that reads as a measurement is
  not.

Ask the historian role for the sourcing pass if the species is new to the
project — that is what it is for, and the research belongs in the catalogue
entry, not in a chat log.

## 2. Add the catalogue entry

Add one `@species(...)`-decorated builder in the band's section of
`plants.py`. The decorator is the catalogue entry and the function is the
generator:

```python
@species(
    key="my_plant",
    common="Common name",
    latin="Genus species",
    band="understory",                 # litter|groundcover|understory|mid|canopy|epiphyte|vine
    height=(0.4, 2.0),                 # (juvenile, mature) metres
    spread=(0.8, 2.4),                 # (juvenile, mature) crown or rosette diameter
    reference="Where every number above comes from, with the detail that makes it "
              "checkable: record counts, elevations, the flora and page.",
    assumed="What is invented rather than sourced.",
    tells="The one thing that identifies it at 5-15 m.",
    variants=("Mature", "Young"),
    parts={"leaf": "MyPlantLeaf", "stem": "MyPlantStem"},
    leaf_tex=LeafTex(name="Leaf_MyPlant", venation="pinnate", outline="heart",
                     green=hsv(120, 56, 30), young=hsv(104, 70, 45), dead=hsv(38, 44, 40),
                     under=1.7, vein=0.6, vein_count=11, blotch=0.2, damage=0.3, gloss=0.6),
)
def _my_plant(parts, rng, p, s):
    height = s.height_at(p.age) * p.scale
    for i, a in enumerate(rosette(4, start=rng.uniform(0, 6.28))):
        ...
```

Rules that are easy to miss:

- **Multiply by `p.scale`** wherever a size comes from the catalogue, and read
  heights through `s.height_at(p.age)`, never as a constant.
- **Use `p.detail(full, mid, low)`** for every segment, leaflet and leaf
  count, so the LODs fall out of the same code.
- **Space leaves with `rosette()`**, which uses the golden angle. Evenly
  spaced leaves are the first thing that makes a generated plant read as fake.
- **Vary with age within the plant**: the outer, older leaves are larger and
  arch over, the inner ones are small and erect. One `old = i / (count - 1)`
  driving length, rise, droop and tilt does most of the work.
- **Give it a keel**, not a flat card: pass `fold` as `(base, middle, tip)`.
  Real blades are folded hard where they leave the petiole, flat across the
  middle and curled over at the tip.
- **Give it a litter skirt** with `litter_skirt()` unless the species genuinely
  has none (a tree fern has no dead-frond skirt; a palm has a deep one).
  Nothing in this forest touches bare soil.
- **One part per material.** `parts` maps a part name to a material key;
  foliage part names (`leaf`, `canopy`, `litter`, `moss`, `lichen`, `bract`)
  get the leaf atlas, and `trunk`, `stem` and `root` get bark or a plain
  colour. A stem that shares the leaf atlas must pass `tile=(0.46, 0.54)` to
  `tube()` so its UVs stay inside the species' tile.

## 3. Draw its textures

The `leaf` and `plant_bark` recipes are parameterised, so a new species
usually needs no new pixel code: pick its `venation`, `outline`, colours and
damage in `LeafTex`, and its `style` in `BarkTex`. Add a recipe in
`gen_theme_textures.py` only for a surface the existing two cannot reach, and
keep the
[colour rules](../reference/plant-library.md#colour-rules-the-recipes-follow)
— especially that gloss is a neutral desaturation and not a highlight, and
that base colour may not carry baked light.

Then regenerate, in this order:

```sh
python3 tools/assets/plants.py textures          # themes/plants.json
python3 tools/assets/gen_theme_textures.py       # the PNG sets
.venv/bin/python tools/assets/plants.py build    # meshes, materials, manifest
python3 tools/assets/validate_assets.py
```

`build` prints the triangle chain per variant and fails if one is over its
band's budget, or if two species in a band are too close in hue and
saturation. Cut leaf counts or segments in the builder rather than raising the
budget.

## 4. Judge it in the engine, not in the abstract

Render it at real metre size under the temple's own sun and sky, and look at
the frames. A plant that reads on paper and not in the frame is not finished.

```sh
.venv/bin/python tools/assets/plants.py lookdev
flock /home/claude/work/montezuma/.godot-gpu.lock \
  gamescope --backend headless -W 1920 -H 1080 -- \
  godot --path game --rendering-driver vulkan \
  res://assets/lookdev/LD_Plants_Understory.tscn \
  --write-movie /tmp/plants/f.png --fixed-fps 30 --quit-after 60
```

Window size comes from the gitignored `game/override.cfg`; there is no xvfb on
the build host, so use `gamescope --backend headless`. If you pass `--import`,
run `git checkout game/project.godot` afterwards, because the import rewrites
it. The grey block in the frame is 1.8 m tall and an 18% grey card: check the
plant against it for scale and the frame against it for exposure.

Iterate until the specimen reads as a plant rather than folded cardboard. What
usually fixes it, in order: the keel (`fold` as a triple), the within-plant age
range, the golden-angle spacing, a real size range between juvenile and mature,
dead leaves and the litter skirt, and only then the texture.

## 5. Record it

- Add a line to `game/assets/PROVENANCE.md` only if the species family is new
  there; the library has one record covering all of it.
- `docs/reference/plant-library.md` carries the species table: add the row.
- If you changed a budget, a band or the atlas layout, say so in the same PR
  and tell the graphics-programmer, who owns what the backdrop does with it.

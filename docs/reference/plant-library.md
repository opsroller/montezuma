<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Plant library

*Reference. The species a level can plant, what each one is built to, and the
files the generator writes. Adding one: [Add a plant
species](../how-to/add-a-plant-species.md). Why the setting is lowland Maya
rainforest and not highland Mexica:
[Mesoamerican reference](mesoamerican-reference.md). Budgets:
[project profile](project-profile.md#budgets).*

The library is `tools/assets/plants.py`: a catalogue of 26 species, each a
generator that builds one specimen into a bmesh at real metre size, with
parameters (age, health, lean, variant, LOD, scale) and two to four variants
each. It is a library first and a command second — a level or a backdrop
generator imports it and places what it likes, including a species drawn at
random at load time:

```python
import plants

species = plants.pick(rng, band="understory")      # a random understory plant
parts = plants.build(species.key, rng, plants.Params(age=0.7, lean=0.15))
for part, bm in parts.items():                     # part name -> bmesh, metres, Z-up
    ...                                            # species.parts[part] names its material
```

Species stay in the repo whether or not a level uses them. That is the point:
a level picks from the library, so the library is wider than any one level.

## Commands

| Command | Needs | Writes |
|---|---|---|
| `python3 tools/assets/plants.py list` | stdlib | nothing; prints the catalogue |
| `python3 tools/assets/plants.py textures` | stdlib | `tools/assets/themes/plants.json` |
| `.venv/bin/python tools/assets/plants.py build` | `bpy` | `game/assets/models/plants/`, `game/assets/materials/plants/`, the manifest |
| `.venv/bin/python tools/assets/plants.py lookdev` | `bpy` | `game/assets/lookdev/LD_Plants_<Band>.tscn` |

The order is `plants.py textures`, then `gen_theme_textures.py` (which picks
`themes/plants.json` up from that folder like any other parameter file and
draws the PNG sets into `game/assets/textures/plants/`), then `plants.py
build`. `build --only <keys>` limits the run to a few species.

## Bands and budgets

A band is where a plant sits in the forest and, with it, what one specimen may
cost. Groundcover is placed by the hundred and a canopy tree once or twice, so
the budgets differ by a factor of forty.

| Band | Triangles at LOD0 | What it holds |
|---|---|---|
| `litter` | 160 | fallen leaves and frond segments |
| `groundcover` | 400 | moss, lichen, grasses, spikemoss, the cycad |
| `understory` | 1,400 | ferns, aroids, heliconia, calathea, small palms |
| `mid` | 3,000 | banana, fan palm, the strangler fig |
| `canopy` | 7,000 | ceiba, cohune, ramón |
| `epiphyte` | 600 | bromeliads, air plants, the nest fern |
| `vine` | 700 | lianas, aerial-root curtains |

A variant over 900 triangles also gets an authored LOD1, and over 3,000 an
LOD2. They are authored rather than decimated because decimation eats whole
leaves at random, while a lower LOD here drops the smallest leaves, the
fenestrations and the segment counts on purpose. Godot's OBJ importer still
generates its own chain below those.

Measured triangle counts per species, variant and LOD, with the material and
mesh path of every part, are in `game/assets/models/plants/manifest.json`,
which `plants.py build` writes and which fails the build if a variant is over
its band's budget. Nothing in this page states a count: the generator measures
them, so they are never out of date in the manifest and never right here.

## The species

| Key | Species | Band | Height (m) | Variants |
|---|---|---|---|---|
| `leaf_litter` | mixed | litter | 0.01–0.05 | Drift, Scatter, Deep, Fronds |
| `moss_patch` | Bryophyta, *Octoblepharum*-like | groundcover | 0.01–0.06 | Stone, Log |
| `lichen_crust` | Lecanoromycetes | groundcover | 0.002–0.01 | Pale, Ochre |
| `forest_grass` | *Olyra latifolia* | groundcover | 0.3–1.8 | Clump, Sparse |
| `shade_grass` | *Pharus latifolius* | groundcover | 0.15–1.0 | Tuft, Single |
| `spikemoss` | *Selaginella* | groundcover | 0.05–0.3 | Mat |
| `cycad` | *Zamia prasina* | groundcover | 0.2–1.3 | Shade, Sun |
| `tree_fern` | *Sphaeropteris myosuroides* | understory | 0.4–7.0 | Mature, Young, Leaning |
| `philodendron` | *Philodendron radiatum* | understory | 0.4–2.0 | Terrestrial, Climbing, Juvenile |
| `monstera` | *Monstera acuminata* | understory | 0.5–12.0 | Climber, Shingle, Ground |
| `heliconia` | *Heliconia latispatha* | understory | 0.8–3.0 | Clump, Flowering, Young |
| `calathea` | *Calathea lutea* | understory | 0.8–3.0 | Clump, Single |
| `bromeliad_ground` | *Aechmea magdalenae* | understory | 0.4–1.8 | Rosette, Thicket |
| `pacaya_palm` | *Chamaedorea tepejilote* | understory | 0.8–4.0 | Mature, Young, Pair |
| `spiny_palm` | *Astrocaryum mexicanum* | understory | 0.5–5.5 | Mature, Young |
| `banana` | *Musa* × *paradisiaca* | mid | 1.2–6.0 | Mature, Young |
| `fan_palm` | *Cryosophila stauracantha* | mid | 1.0–8.0 | Mature, Young |
| `strangler_fig` | *Ficus obtusifolia* | mid | 3.0–30.0 | Young, Enveloping, Masonry |
| `ceiba` | *Ceiba pentandra* | canopy | 8.0–45.0 | Emergent, Young |
| `corozo_palm` | *Attalea cohune* | canopy | 1.5–15.0 | Mature, Stemless |
| `canopy_tree` | *Brosimum alicastrum* | canopy | 6.0–35.0 | Mature, Sapling |
| `bromeliad_epiphyte` | *Aechmea bracteata* | epiphyte | 0.2–1.2 | Single, Cluster |
| `tillandsia` | *Tillandsia* spp. | epiphyte | 0.1–0.5 | Clump |
| `nest_fern` | *Asplenium serratum* | epiphyte | 0.3–1.2 | Mature, Young |
| `liana` | *Bauhinia herrerae* | vine | 3.0–18.0 | Hanging, Ladder, Curtain |
| `root_curtain` | *Ficus* / *Philodendron* roots | vine | 1.0–12.0 | Sparse, Dense |

Each entry in `plants.py` carries, in the code beside its generator, the
sourced proportions it was built to (`reference`), what had to be invented
(`assumed`), the one feature that identifies it at 5–15 m (`tells`), and, where
it applies, why it may not be planted (`historical`). Read those before
changing a number: several of them record a species that was *replaced*
because it does not occur here.

### Species that were replaced, and why

| Not used | Why | Used instead |
|---|---|---|
| *Socratea exorrhiza* (stilt palm) | 1,910 GBIF specimens, northern limit Nicaragua, none from Mexico, Guatemala, Belize or Honduras. There is no cone-of-stilt-roots palm in the Petén | `spiny_palm`, *Astrocaryum mexicanum* |
| *Monstera deliciosa* | 450–2,620 m; every georeferenced Guatemalan record is highland | `monstera`, *M. acuminata* |
| *Alsophila cuspidata* | montane here; the Guatemalan records are Alta Verapaz at 956 m | `tree_fern`, *Sphaeropteris myosuroides* |
| *Zamia loddigesii*, *Dioon* | wrong range; Dioon returns three doubtful records for all of Belize and Guatemala | `cycad`, *Zamia prasina* |
| *Ficus aurea* | known in Guatemala from one highland collection; the lowland strangler is better attested | `strangler_fig`, *F. obtusifolia* |
| *Carex*, lawn grasses | montane, and there is no grass floor in this forest at all | `shade_grass`, `forest_grass` as accents |
| Beard lichen (*Usnea*-type) | a cloud-forest signature, essentially absent at 150–300 m | `lichen_crust`, flat and pale |

`banana` is kept, flagged, and left out of random picks: *Musa* is Asian and
arrived in the 16th century. `pick()` drops every species carrying a
`historical` note unless asked for `pre_contact=False`, so a pre-contact scene
cannot plant one by accident.

## Meshes

- Metres, Blender Z-up, exported Godot Y-up, origin at the base of the stem on
  the ground, so a specimen can be dropped straight onto terrain.
- One OBJ per (species, variant, LOD, part), named
  `SM_Plant_<Species><Variant>_<Part>[_LODn].obj`, so each one is a single
  Godot surface with a single material override.
- **Leaves carry UVs**, unlike the board and backdrop meshes, which are mapped
  triplanar. A leaf needs its veins, its margin damage and its own tint, and a
  triplanar projection can give it none of them.
- **Leaves are geometry, not alpha cutouts.** The blade's outline is the mesh
  outline, the midrib is a real crease, the margins really curl and a torn
  banana blade is really torn. That costs vertices and saves the whole
  alpha-coverage, sorting and shadow-bias problem, and it is what stops a
  plant reading as folded cardboard from the game's low camera.
- Face winding is authored and normals are never recalculated: a leaf is an
  open surface, and recalculation flips it at random.

## Textures

One leaf atlas and, where the plant has a woody part, one bark set per
species, drawn by the `leaf` and `plant_bark` recipes in
`tools/assets/gen_theme_textures.py` from the parameters in each species'
`LeafTex` and `BarkTex`. Everything is authored in numpy; no photograph is
used, sampled or traced anywhere.

A leaf set is an **atlas**, not a tiling surface: a 2×2 grid, each tile the
surface of one leaf in blade-local coordinates — u across the blade with the
midrib at 0.5 and the margins at 0 and 1, v from base to tip.

| Tile | State |
|---|---|
| (0, 0) | mature |
| (1, 0) | the young flush: lighter, yellower, clean |
| (0, 1) | old: blotched, nicked, browning at the margin |
| (1, 1) | dead — forced for dying leaves and for the litter skirt |

A specimen samples one tile, chosen from its seed, so a stand of one species
carries three live readings from one material and one draw call, and no two
plants read identically. `Params.health` biases that choice toward the old
tile.

Sets are 512 px (256 px per tile), `_BC` + `_N` + `_ORM`, no `hi/` variant.
That is roughly 0.6 MB of VRAM per set once compressed, so a level planting a
dozen species pays something like 8 MB — an estimate from the format, not a
measurement; measure it with the GPU profiler before relying on it.

### Colour rules the recipes follow

From sampling openly-licensed reference plates, and the reason several of these
are counter-intuitive:

- Colours in the catalogue are **albedo**, written as `hsv(hue, sat, value)`.
  A sampled photograph carries its lighting and a base-colour map may not; the
  asset validator rejects a `_BC` map whose 0.5–99.5 percentile leaves
  30–240 sRGB as having light baked in.
- Gloss reads as **saturation collapsing toward a neutral sheen band** about
  1.35× the lamina value — never as a bright highlight, and never clipped.
- Midribs read about **1.7×** the lamina value.
- Backfaces are **1.5–2.5× the value** of the front and 30–50% less saturated;
  that is the `under` field, and it sets each foliage material's `backlight`.
- Backlit leaves are a **flat, bright, saturated field** shifted 15–20° toward
  yellow with a dark vein net, and that flatness is the look.
- Shade shifts green **bluer as well as darker**.
- 10–20% of a crown's leaf area is dead or yellowing and hangs vertically; a
  third to a half of leaves carry 3–15 mm holes or nicked margins with a
  1–2 mm pale halo.
- Neighbouring species must not share a green: `plants.py build` fails if two
  species in one band differ by less than 18° of hue *and* less than 20 points
  of saturation.
- Litter is two materials, not one: damp shaded floor is near-neutral at
  4–10% saturation, dry sunlit litter warm at 39–43%.
- Exposed roots are **darker** than the soil they come out of, and nearly
  neutral.

## Materials

`plants.py build` writes `game/assets/materials/plants/MI_Plant_<Key>.tres`,
one per material key named in `Species.parts`:

- a textured `ORMMaterial3D` for leaves and bark, with `cull_mode = 2` and
  `backlight_enabled` on foliage;
- a plain stock `StandardMaterial3D` for a petiole or a bract, because a 2 cm
  stalk cannot show a texture and a material without one is a material the
  mobile tier never pays for.

Both are stock Godot materials: no custom shader.

## Look-dev

`plants.py lookdev` writes one scene per band, `LD_Plants_<Band>.tscn`: the
band's specimens in a row at real metre size, spaced by their own spread,
standing on the temple's plaza material under the temple's own sun and sky,
with a 1.8 m block at the end of the row that is also an 18% grey card, so
scale and exposure are readable off the frame instead of argued about. Capture
them with the headless recipe in
[Capture the effects showcase](../how-to/capture-the-effects-showcase.md).

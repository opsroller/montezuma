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
| `.venv/bin/python tools/assets/plants.py build` | `bpy` | `game/assets/models/plants/`, `game/assets/materials/plants/`, the plant section of `game/assets/library.json` |
| `python3 tools/assets/plants.py lookdev` | stdlib | `game/assets/lookdev/LD_Plants_<Band>.tscn`, one row per band |
| `python3 tools/assets/plants.py closeups [--empty]` | stdlib | `game/assets/lookdev/LD_Plant_<Species><Variant>.tscn`, one page per specimen |
| `.venv/bin/python tools/assets/measure_foliage.py <frame.png>` | numpy, Pillow | nothing; prints a frame's figures against [Foliage reference](foliage-reference.md) |

The order is `plants.py textures`, then `gen_theme_textures.py` (which picks
`themes/plants.json` up from that folder like any other parameter file and
draws the PNG sets into `game/assets/textures/plants/`), then `plants.py
build`. `build --only <keys>` limits the run to a few species.

## Bands and ceilings

A band is where a plant sits in the forest. The triangle figure beside it is a
**ceiling, not a budget**: it catches a builder that has run away — a loop that
puts four hundred leaflets on one frond — and it is set from what the species in
that band measure, with room above them. It is not a quality lever. A plant that
needs more geometry to stop reading as folded cardboard gets more geometry, and
nothing here is cut to buy frame time (the project's performance work is about
object scale, not polygon counts).

| Band | Triangle ceiling at LOD0 | What it holds |
|---|---|---|
| `litter` | 300 | fallen leaves and frond segments |
| `groundcover` | 900 | moss, lichen, grasses, spikemoss, the cycad |
| `understory` | 4,000 | ferns, aroids, heliconia, calathea, small palms |
| `mid` | 4,000 | banana, fan palm, the strangler fig |
| `canopy` | 7,000 | ceiba, cohune, ramón |
| `epiphyte` | 1,200 | bromeliads, air plants, the nest fern |
| `vine` | 1,000 | lianas, aerial-root curtains |

A variant over 900 triangles also gets an authored LOD1, and over 3,000 an
LOD2. They are authored rather than decimated because decimation eats whole
leaves at random, while a lower LOD here drops the smallest leaves, the
fenestrations and the segment counts on purpose. Godot's OBJ importer still
generates its own chain below those.

Measured triangle counts per species, variant and LOD, with each build's real
bounding box and the material and mesh path of every part, are in the plant
section of `game/assets/library.json` — the one shared catalogue, described in
[Asset library](asset-library.md) — which `plants.py build` writes and which
fails the build if a variant goes over its band's ceiling. Nothing in this page
states a count: the generator measures them, so they are never out of date in
the catalogue and never right here.

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
- Every blade's profile is zero wide at its base and its tip — that is what
  makes it a leaf and not a rectangle — so the first and last row of its vertex
  grid sits on one point. `build()` welds those zero-length edges; without it
  17% of the library's triangles had no area, and zero-length normals fed the
  smooth-shading pass at exactly the base and tip of every leaf.

## What every plant gets

Three things the measured reference says are on every plant, and that are easy
to leave off one species at a time, are applied to the whole library at once
rather than left to each builder to remember.

- **A litter skirt.** "Every plant sits in its own litter skirt… green never
  meets bare soil." `build()` adds one, sized from the species' own spread,
  unless `Species.litter` says `own` (the builder sheds something more specific
  — a banana's 1.5 m sheaths, a palm's whole fronds) or `none` (a ground mat, an
  epiphyte on a branch, a hanging vine).
- **Dead leaves hanging vertically**, 10–20% of leaf area, drawn from the
  atlas's dead tile so they cost no extra material. `Species.dying` works the
  same way. This is a silhouette cue, and a plant with a complete set of live
  leaves reads as generated from any distance.
- **Bitten margins.** Damage is on a third to a half of all leaves. The
  3–15 mm holes the reference describes are drawn into the leaf atlas, but past
  about two metres the texture is gone and the silhouette carries everything, so
  `blade()` also takes notches out of the margin by default — moving the margin
  column inward, which costs no triangles. Pass `bite=()` for a leaf that must
  be intact.

## Colour separation

Each species' hue and saturation is sampled from its own reference plates, and
sampled independently they land on top of each other: twenty pairs of this
library sat within 18° of hue and 20 points of saturation of a neighbour in the
same band, which is the "one global green" the reference calls the loudest
failure. `separate_greens()` runs once at import, after every species is
registered, and spreads each band's greens across a 50–96° window in the order
their own sources put them, alternating saturation high and low along that
order. Every neighbouring pair then differs by the full saturation split and
every other pair by more than 20° of hue, which is what the reference asks for —
20–35° of hue *and* 20–30 points of saturation between adjacent species — in a
way eight understory species can actually satisfy inside the green part of the
spectrum.

What is sourced survives: the ranking, which species is the yellow-green one and
which the blue-green one, each plant's value (its band decides how much light it
gets), and the young, dead and backlit colours, recomputed from the new green
with the offsets the species declared. A species whose colour is not a green —
the lichen crust, the grey Tillandsia, the litter — is left alone.

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

Sets are 4096 px (2048 px per tile), `_BC` + `_N` + `_ORM`, no `hi/` variant,
per the quality floor in `CLAUDE.md`: across a real 0.6–0.9 m blade that is
23–34 px/cm, which is what the vein net and the 1–2 mm pale halo on a chewed
margin need to survive the game's camera. The mobile and web export presets
strip them down. Roughly 11 MB of VRAM per map once compressed, so the whole
36-set library is on the order of a gigabyte on desktop — an estimate from the
format, not a measurement; measure it with the GPU profiler before relying on
it.

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

## The catalogue

There is one catalogue for the whole project, `game/assets/library.json`, whose
schema and seeded selection rule are in [Asset library](asset-library.md). The
plant library writes the `plants` section of it and the prop library writes its
own, so each generator merges key by key and never rewrites the file whole.

Three places where plants could not follow that page as written, which whoever
builds the scatter has to settle:

| The page says | What plants do | Why |
|---|---|---|
| one entry per generator, with `variants` counting the builds | one entry per (species, variant), with `variants` still counting them and `species` linking the siblings | one generator makes a 3 m young strangler fig and a 30 m enveloping one; they cannot share one `height_m`, one `tris` ladder or one set of tags |
| `generator` is `module:function`, called `(bm, rng, x, y, z, params)` | `plants:build`, called `plants.build(key, rng, Params(...))`, returning one bmesh per material | a plant is several surfaces with several materials, and the caller decides where it stands |
| props instance through a `scene` | plants give `parts`: mesh and material per part, per LOD | plants write no per-variant scene yet; the two have to converge before a scatter can place either |

The page's `band` vocabulary also has no `litter`, which this library needs, so
`plants.py` adds its band names to the shared `vocabulary` rather than mapping
them onto the prop library's. Each entry additionally carries `bounds_m`, the
build's measured bounding box, which the scatter wants for spacing and the
look-dev page uses for framing.

## Look-dev

Two kinds of page, both standing on the temple's plaza material under the
temple's own sun and sky, with a 1.8 m block that is also an 18% grey card so
scale and exposure are readable off the frame instead of argued about.

- `plants.py lookdev` writes one scene per band, `LD_Plants_<Band>.tscn`: the
  band's specimens in a row at real metre size, spaced by their own spread.
  This is the page for comparing species — whether two neighbours share a
  green, whether the sizes are right against each other.
- `plants.py closeups` writes one scene per specimen,
  `LD_Plant_<Species><Variant>.tscn`, framed from its measured bounding box so
  the plant fills about 80% of the frame, from three quarters round at eye
  height. **This is the page for judging whether a plant reads as a plant.** A
  band row cannot answer that: a row is as wide as the sum of its spreads, so
  the understory row is 68 m across, its camera stands 55 m back, and a 1 m
  blade lands on 40 pixels of a 4K frame. `--empty` writes each page a second
  time with the plant removed, which is what `measure_foliage.py --against`
  needs to tell foliage from ground.

Both pages give the camera its own `CameraAttributesPractical`: the theme's
exposure, without the theme's depth of field, whose far blur starts at 0.5 m
because it exists to push the backdrop away from the board and which otherwise
blurs the subject of the page. They also hide the theme's atmosphere fog, dust,
light shafts and braziers.

Capture them with the headless recipe in
[Capture the effects showcase](../how-to/capture-the-effects-showcase.md), at
3840×2160 or larger, then read the figures back with
`measure_foliage.py`, which checks a frame against
[Foliage reference](foliage-reference.md): value range inside the mass, hue
spread, lamina saturation, whether the sheen stayed neutral, dead leaf area and
the contact shadow under the plant.

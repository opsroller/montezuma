# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""A library of lowland-Maya-rainforest plants, generated as meshes with Blender.

    python3 tools/assets/plants.py list                     # the catalogue, one line per variant
    python3 tools/assets/plants.py textures                 # tools/assets/themes/plants.json
    .venv/bin/python tools/assets/plants.py build           # meshes + manifest + materials
    .venv/bin/python tools/assets/plants.py build --only heliconia,ceiba
    .venv/bin/python tools/assets/plants.py lookdev         # look-dev scenes, one row per band

`list` and `textures` need only the standard library; `build` and `lookdev` need `bpy`
(Blender as a module), the same dependency as gen_theme_meshes.py. `textures` writes the
parameter file that gen_theme_textures.py turns into the PNG sets, so the order is
`plants.py textures`, then `gen_theme_textures.py`, then `plants.py build`.

This file is a *library* first and a command second. Place plants yourself with:

    import plants
    parts = plants.build("tree_fern", rng, plants.Params(age=0.7, lean=0.15))
    for name, bm in parts.items():   # name -> bmesh, metres, Blender Z-up, origin at the base
        merge_into(my_bmesh[name], bm)

Nothing here reaches into a theme, a scene or the backdrop: a caller decides where
a specimen stands and which material its parts get (`Species.parts` names them).
`pick()` draws a species at random from a band, which is what a level that plants
itself at load time needs.

Conventions, all shared with `gen_theme_meshes.py`: metres, Blender Z-up (the OBJ
writer turns it into Godot Y-up), origin at the base of the stem on the ground, one
OBJ per (specimen, part) so each part is one Godot surface with one material.
Same species, variant, seed and parameters give the same bytes.

Unlike the theme meshes, plant meshes carry **UVs**: a leaf needs its veins, its
margin damage and its own tint, and triplanar projection cannot give it any of
them. Every leaf of a species maps into one tile of that species' leaf atlas
(`Species.leaf_tex`, a 2x2 grid of tint and age variants, drawn by the `leaf`
recipe in `gen_theme_textures.py`); a specimen picks its tile from its seed, so
two plants of one species never read identically although they share a material
and a draw call. Bark maps tile along the trunk.

Leaves are geometry, not alpha-tested cards: the blade's outline is its mesh
outline, the midrib is a real crease and the margin really curls. That costs
vertices and saves the whole alpha-coverage, sorting and shadow-bias problem,
and it is the only way a plant stops reading as folded cardboard from the
game's low camera.

Sources for the proportions of each species are in `Species.reference`; the
setting they belong to is settled in `docs/reference/mesoamerican-reference.md`
(lowland Maya rainforest, 150-300 m, closed canopy 25-35 m). Anything this
library invents rather than sources says so in `Species.assumed`.

Four rules that come out of that setting and are easy to get backwards:

- **The temple is an overgrown ruin**, not a restored painted building, so this is
  built for growth reclaiming stone: `strangler_fig/Masonry` runs its straps down a
  wall, moss and crustose lichen hold the damp lower courses, every plant stands in
  a skirt of its own dead leaves, and nothing is placed as though a gardener had
  been through.
- **No beard lichen, no furry green trunks.** Hanging fruticose lichen is a
  cloud-forest signature; at 150-300 m trunks carry flat crustose lichen that reads
  as a pale stain, with moss only in the bottom metre or two. A green furry trunk
  makes the shot read as 1,500 m.
- **No grass floor.** The lowland herb layer is aroids and ferns, not grasses: the
  floor is brown litter with isolated green accents, and litter, not soil, is the
  ground surface. `shade_grass` and `forest_grass` are accents and gap plants.
- **Nothing that does not grow here.** No banana in a pre-contact scene, no stilt
  palm, no Monstera deliciosa, no Dioon: `pick()` leaves out anything carrying a
  `historical` note, and where a species was replaced for this reason the
  replacement's `reference` names what it replaced and why.
"""

import argparse
import json
import math
import random
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MODELS = ROOT / "game/assets/models/plants"
MATERIALS = ROOT / "game/assets/materials/plants"
LOOKDEV = ROOT / "game/assets/lookdev"
TEXTURES = "res://assets/textures/plants"

SPDX = "SPDX-License-Identifier: GPL-2.0-or-later"

# Vertical bands of a rainforest, and the triangle ceiling one specimen of each may
# cost at LOD0. These are a guard against a builder running away - a loop that puts
# 400 leaflets on a frond - not a quality lever: a band's ceiling is set from what the
# species in it actually measure, with room above, and the answer to a plant that
# needs more geometry to read as a plant is more geometry. Nothing here is cut to buy
# frame time. Measured counts per variant are in game/assets/library.json.
BANDS = ("litter", "groundcover", "understory", "mid", "canopy", "epiphyte", "vine")
# These were an order of magnitude too low, and the claim above that they are a guard
# rather than a quality lever did not survive contact with the measurements: a
# heliconia clump measures 4,400 against a ceiling of 5,500, so the builder was
# authored to fit under the number rather than to the reference. The whole
# twenty-six-species library came to 209,300 triangles - less than one character in a
# current game - and it rendered as flat-sided stems and folded-paper blades because
# there was no geometry to be round with. The target hardware runs current titles at
# 8K, every variant above the LOD thresholds carries an authored LOD1 and LOD2, and
# the project's standing rule is that nothing is cut to buy frame time. So the
# ceilings are what a plant that reads as a plant at 3840x2160 costs.
TRI_BUDGET = {
    "litter": 3000,
    # A Zamia rosette of 3-8 pinnate fronds measured 1,440 under the old ceiling of
    # 1800 - the big end of this band by a long way, and a mat of moss is 40.
    "groundcover": 14000,
    "epiphyte": 12000,
    "vine": 12000,
    # A 6 m tree fern with bipinnate fronds is not a 0.5 m calathea, and holding both
    # to one number is what made five variants read as over budget when they were
    # simply the big end of their band.
    "understory": 45000,
    "mid": 45000,
    # The canopy band is the one place the library's "leaves are geometry, never alpha
    # cutouts" rule costs real money. A 30 m crown has hundreds of square metres of
    # envelope to fill, and a 15 cm leaf covers 0.02 of one: at 7,000 triangles the
    # crown is about 0.5% covered and a 45 m ceiba renders as a bare winter tree. These
    # trees are placed once or twice in a level, and the alternative is cutting the look
    # to buy frame time, so the ceiling is what a crown that reads as foliage measures.
    "canopy": 60000,
}
# Above this many triangles a variant also gets a reduced LOD1 (and LOD2 above the
# second figure). Godot's OBJ importer generates its own LOD chain by decimation;
# these are authored instead, because decimating a plant eats whole leaves at random
# while an authored LOD drops the smallest leaves and the segment counts on purpose.
LOD1_OVER, LOD2_OVER = 900, 3000


# --------------------------------------------------------------------------
# Blender, imported lazily so the catalogue (and gen_theme_textures.py, which reads
# the texture sets from here) works without bpy installed.

_bmesh = None


def _bm_module():
    global _bmesh
    if _bmesh is None:
        import bpy  # noqa: F401  (importing bpy first makes bmesh importable)
        import bmesh

        _bmesh = bmesh
    return _bmesh


def new_bmesh():
    return _bm_module().new()


# --------------------------------------------------------------------------
# Parameters


@dataclass(frozen=True)
class Params:
    """How one specimen differs from the next.

    age     0 seedling .. 1 full size: height, leaf count, stem thickness, how far
            the oldest leaves have arched over.
    health  0 dying .. 1 lush. It biases which atlas tile the specimen samples (a sick
            plant draws the old, blotched one) and, in the species that shred, how far
            the blades have gone. It is deliberately not a droop multiplier: a wilting
            plant changes shape per species, and faking it globally reads as melted.
    lean    radians off vertical (a plant that grew toward a light gap), with
            lean_dir the compass direction it leans in.
    variant index into Species.variants.
    tile    which tile of the leaf atlas this specimen samples (-1 picks from the rng).
    lod     0 full detail, 1 and 2 the reduced chain.
    scale   a last multiplier on everything, for a specimen that must fit a spot.
    """

    age: float = 1.0
    health: float = 1.0
    lean: float = 0.0
    lean_dir: float = 0.0
    variant: int = 0
    tile: int = -1
    lod: int = 0
    scale: float = 1.0

    def detail(self, full, mid, low):
        """Pick a segment or leaflet count for this LOD."""
        return (full, mid, low)[min(self.lod, 2)]


def _lerp(a, b, t):
    return a + (b - a) * t


def _clamp(v, lo=0.0, hi=1.0):
    return lo if v < lo else hi if v > hi else v


# --------------------------------------------------------------------------
# Texture sets. The pixels are drawn by the `leaf` and `bark` recipes in
# gen_theme_textures.py; these say what each species needs from them.


def hsv(h, s, v):
    """sRGB 0..1 from hue degrees and saturation/value percent, as a colour sampler reports them.

    Colours here are **albedo**, not what a photograph shows: a sampled leaf carries its
    lighting, and base colour may not. So hue and saturation come from the reference and
    the value is held in the band a leaf's albedo can physically sit in. The asset
    validator enforces the same thing from the other end: a base-colour map whose
    0.5-99.5 percentile leaves 30-240 sRGB is rejected as having light baked in.
    """
    import colorsys

    return tuple(round(c, 4) for c in colorsys.hsv_to_rgb((h % 360) / 360.0, s / 100.0, v / 100.0))


@dataclass(frozen=True)
class LeafTex:
    """One species' leaf atlas: a 2x2 grid of the same leaf in four states.

    Tile (0,0) is a mature leaf, (1,0) the young flush (lighter, yellower, clean),
    (0,1) an old leaf with blotching and margin damage, and (1,1) a dead or dying one.
    A specimen draws its tile from its seed, so a stand of one species carries three
    live readings off one material; dead leaves and the litter skirt force tile (1,1).

    From the measured reference: gloss shows as saturation collapsing toward a neutral
    sheen band about 1.35x the lamina value, never as a bright clipped highlight;
    midribs read about 1.7x the lamina value; backfaces are 1.5-2.5x the value of the
    front and 30-50% less saturated; shade shifts green bluer as well as darker.
    """

    name: str                      # T_<name>_BC.png etc.
    venation: str                  # pinnate | parallel | palmate | midrib | dichotomous
    outline: str = "ovate"         # ovate | strap | pinnule | lobed | fan | heart | wedge
    green: tuple = hsv(112, 60, 31)        # mature upper surface, sRGB albedo
    young: tuple = hsv(98, 74, 46)         # young flush
    # The dead tile and the litter skirt. The measured floor is two materials: dry
    # sunlit litter at 39-43% saturation and value 37-41, and wet or shaded floor that
    # is almost neutral (10%). 46% sat sat just above the dry figure and rendered as a
    # ring of ochre slabs, so this sits inside the measured band.
    dead: tuple = hsv(36, 41, 39)
    under: float = 1.6             # how much paler the underside is (1.5-2.5 measured)
    vein: float = 0.5              # how strongly veins show, 0..1
    vein_count: int = 14           # secondary veins per side (pinnate) or per blade (parallel)
    midrib: float = 1.7            # midrib value against the lamina
    blotch: float = 0.3            # age mottling and fungal spotting
    damage: float = 0.25           # insect holes and nicked margins, with their pale halo
    gloss: float = 0.5             # 0 matt .. 1 waxy; a neutral sheen band, not a highlight
    grid: tuple = (2, 2)           # tiles across and down; every set is TEXTURE_SIZE px

    def backlight(self):
        """What a leaf of this species looks like lit from behind: a flat, bright,
        saturated field 15-20 degrees toward yellow, with the veins reading dark."""
        r, g, bl = self.green
        return (round(min(0.95, r * 1.9 + 0.08), 4),
                round(min(0.95, g * 1.55 + 0.10), 4),
                round(min(0.95, bl * 1.3), 4))


@dataclass(frozen=True)
class BarkTex:
    """Bark and root. Exposed roots are *darker* than the soil they come out of and
    nearly neutral, which is the opposite of the usual guess; wet shaded bark sits at
    4-10% saturation and only dry sunlit wood runs warm."""

    name: str
    style: str = "fissured"        # smooth | ringed | fissured | fibrous | spiny | lenticel
    colour: tuple = hsv(28, 22, 30)
    lichen: float = 0.3
    moss: float = 0.25
    relief: float = 1.0            # normal strength multiplier


@dataclass(frozen=True)
class Species:
    key: str
    common: str
    latin: str
    band: str
    height: tuple                  # (juvenile, mature) metres, trunk/stem included
    spread: tuple                  # (juvenile, mature) crown or rosette diameter, metres
    reference: str                 # where the proportions come from
    build: object = None           # build(parts, rng, p, s)
    variants: tuple = ("A",)
    parts: dict = field(default_factory=dict)      # part name -> material key
    leaf_tex: LeafTex = None
    bark_tex: BarkTex = None
    assumed: str = ""              # what is invented rather than sourced
    historical: str = ""           # a note when the plant does not belong in every scene
    tells: str = ""                # what makes it unmistakable at 5-15 m
    # Two things the measured reference says are on every plant and are easy to leave
    # off, so `build()` adds them for the whole library rather than each builder
    # remembering: the skirt of its own shed leaves a plant stands in, and the 10-20%
    # of its leaf area that is dead and hanging vertically.
    #   "auto"  build() adds it, sized from the species
    #   "own"   the builder makes its own, better informed; build() leaves it alone
    #   "none"  the plant genuinely has neither (a ground mat, an epiphyte on a branch)
    litter: str = "auto"
    dying: str = "auto"

    def height_at(self, age):
        return _lerp(self.height[0], self.height[1], _clamp(age) ** 0.8)

    def spread_at(self, age):
        return _lerp(self.spread[0], self.spread[1], _clamp(age) ** 0.8)


SPECIES = {}


def species(**meta):
    def register(fn):
        key = meta["key"]
        SPECIES[key] = Species(build=fn, **meta)
        return fn

    return register


def get(key):
    try:
        return SPECIES[key]
    except KeyError:
        raise KeyError(f"no plant species '{key}'; have {', '.join(sorted(SPECIES))}") from None


def by_band(band):
    return [s for s in SPECIES.values() if s.band == band]


def pick(rng, band=None, exclude=(), pre_contact=True):
    """A species at random, which is what a level planting itself at load time wants.

    `pre_contact` leaves out everything a Maya lowland scene before 1492 cannot show.
    """
    pool = [s for s in SPECIES.values()
            if (band is None or s.band == band)
            and s.key not in exclude
            and not (pre_contact and s.historical)]
    if not pool:
        raise ValueError(f"no species left for band={band!r} after exclusions")
    return rng.choice(sorted(pool, key=lambda s: s.key))


# The project's quality floor (CLAUDE.md): desktop texture sets are 4096 square, and the
# export caps strip them down for mobile and web. A leaf atlas is 2x2, so one leaf tile is
# 2048 px; across a 0.6-0.9 m blade that is 23-34 px/cm, enough for the vein net and the
# 1-2 mm pale halo on a chewed margin to survive the game's close camera. A bark set at
# 4096 tiles at about 1.5 m, so 27 px/cm on a trunk.
TEXTURE_SIZE = 4096


def texture_sets():
    """{set name: {recipe, normal_strength, args}} for gen_theme_textures.py."""
    sets = {}
    for s in sorted(SPECIES.values(), key=lambda s: s.key):
        if s.leaf_tex:
            t = s.leaf_tex
            sets[t.name] = {
                "recipe": "leaf",
                "normal_strength": 2.0,
                "args": {"venation": t.venation, "outline": t.outline, "green": list(t.green),
                         "young": list(t.young), "dead": list(t.dead), "vein": t.vein,
                         "vein_count": t.vein_count, "midrib": t.midrib, "blotch": t.blotch,
                         "damage": t.damage, "gloss": t.gloss, "grid": list(t.grid)},
            }
        if s.bark_tex:
            t = s.bark_tex
            sets[t.name] = {
                "recipe": "plant_bark",
                "normal_strength": round(5.0 * t.relief, 2),
                "args": {"style": t.style, "colour": list(t.colour), "lichen": t.lichen,
                         "moss": t.moss},
            }
    return sets


def texture_params():
    """The whole tools/assets/themes/plants.json: the parameter file gen_theme_textures.py
    picks up from that folder, exactly as the prop library does. It is not a board theme -
    no board, no artifacts, no manifest entry - and gen_theme_scenes.py is never run on it."""
    return {
        "_comment": "Texture sets for the shared plant library (tools/assets/plants.py). "
                    "SPDX-License-Identifier: GPL-2.0-or-later. Generated by "
                    "`plants.py textures`: edit the species in tools/assets/plants.py, not "
                    "this file. This is NOT a board theme (no board, artifacts, environment "
                    "or manifest entry) and gen_theme_scenes.py is never run on it; it sits "
                    "in tools/assets/themes/ only so gen_theme_textures.py builds its sets "
                    "into game/assets/textures/plants/ from CMake and CI with no extra "
                    "wiring. The materials that use these sets are written by plants.py.",
        "theme": "plants",
        "seed": 20261027,
        "textures": {"size": TEXTURE_SIZE, "sets": texture_sets()},
    }


# --------------------------------------------------------------------------
# Mesh primitives. Everything writes UVs; `Build` carries the bmesh, its UV layer
# and the atlas tile the current specimen draws from.


class Build:
    def __init__(self, bm, tile=(0, 0), grid=(2, 2), rng=None):
        self.bm = bm
        self.uv = bm.loops.layers.uv.verify()
        self.tile = tile
        self.grid = grid
        self.rng = rng or random.Random(0)

    def vert(self, x, y, z):
        return self.bm.verts.new((x, y, z))

    def face(self, verts, uvs):
        f = self.bm.faces.new(verts)
        for loop, uv in zip(f.loops, uvs):
            loop[self.uv].uv = uv
        return f

    def tile_uv(self, u, v):
        """Map a 0..1 leaf-local coordinate into this specimen's atlas tile.

        v is written upside down on purpose. A Wavefront OBJ's v runs from the bottom
        of the image, Godot's from the top, and Godot's OBJ importer flips it on the
        way in; the atlas is written top row first (`_leaf_tile` puts the blade's base
        at the top of its tile). Without the flip every specimen samples the tile
        mirrored in ty: a plant's own litter skirt, which asks for the dead tile at
        (1,1), came back the bright young flush at (1,0) - green ribbons of fresh leaf
        lying round the base of every plant, which is what the first rendered frame
        showed.
        """
        gx, gy = self.grid
        tx, ty = self.tile
        return ((tx + _clamp(u)) / gx, 1.0 - (ty + _clamp(v)) / gy)

    def dead(self):
        """The same bmesh, reading the atlas's dead tile: for the dying leaves every plant
        carries (10-20% of its leaf area) and for its own litter skirt."""
        clone = Build.__new__(Build)
        clone.bm, clone.uv, clone.grid = self.bm, self.uv, self.grid
        clone.rng = self.rng
        clone.tile = (self.grid[0] - 1, self.grid[1] - 1)
        return clone


def tris(bm):
    return sum(len(f.verts) - 2 for f in bm.faces)


def _norm(v):
    length = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) or 1.0
    return (v[0] / length, v[1] / length, v[2] / length)


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def frames(points):
    """Rotation-minimising frames along a polyline: (tangent, normal, binormal) per point.

    Parallel transport, so a long liana does not spin its cross-section as it bends.
    """
    tangents = []
    for i, p in enumerate(points):
        a = points[max(i - 1, 0)]
        b = points[min(i + 1, len(points) - 1)]
        tangents.append(_norm((b[0] - a[0], b[1] - a[1], b[2] - a[2])))
    up = (0.0, 0.0, 1.0)
    seed = (1.0, 0.0, 0.0) if abs(tangents[0][2]) > 0.95 else up
    n = _norm(_cross(seed, tangents[0]))
    out = []
    for t in tangents:
        # Re-orthogonalise the carried normal against the new tangent.
        d = n[0] * t[0] + n[1] * t[1] + n[2] * t[2]
        n = _norm((n[0] - t[0] * d, n[1] - t[1] * d, n[2] - t[2] * d))
        out.append((t, n, _cross(t, n)))
    return out


def tube(b, points, radii, sides=6, v_scale=1.0, cap_start=True, cap_end=False, u_repeat=1.0,
         squash=(1.0, 1.0), tile=None):
    """A swept tube along `points` with per-point `radii`; the trunk, stem and liana primitive.

    UVs run around in u and along the real length in v, so bark tiles at a fixed
    texel density whatever the plant's size. `squash` scales the cross-section in the
    frame's two lateral axes: (1, 0.25) gives the flat woody ribbon a monkey-ladder
    liana actually is, which is the whole tell of that plant.

    `tile` is for a stem that shares a leaf atlas instead of a bark set - a frond's
    rachis, a grass culm - and maps it into a (u_lo, u_hi) band of the species' tile
    with v running base to tip. Without it the stem's v runs in metres, which on an
    atlas would sample straight across the other three tiles.
    """
    rings = []
    run = 0.0
    vs = []
    for i, (p, f) in enumerate(zip(points, frames(points))):
        if i:
            q = points[i - 1]
            run += math.dist(p, q)
        vs.append(run * v_scale)
        _, n, bn = f
        r = radii[i]
        ring = []
        for k in range(sides):
            a = 2 * math.pi * k / sides
            c, s = math.cos(a) * r * squash[0], math.sin(a) * r * squash[1]
            ring.append(b.vert(p[0] + n[0] * c + bn[0] * s,
                               p[1] + n[1] * c + bn[1] * s,
                               p[2] + n[2] * c + bn[2] * s))
        rings.append(ring)
    total = vs[-1] or 1.0

    def uv(k, i):
        u = k / sides * u_repeat
        if tile is None:
            return (u, vs[i])
        lo, hi = tile
        # Fold the way round into the band so the far side mirrors the near side.
        folded = abs((k / sides) * 2 - 1)
        return b.tile_uv(_lerp(lo, hi, folded), vs[i] / total)

    for i in range(len(rings) - 1):
        for k in range(sides):
            j = (k + 1) % sides
            b.face((rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]),
                   (uv(k, i), uv(k + 1, i), uv(k + 1, i + 1), uv(k, i + 1)))
    if cap_start:
        b.face(list(reversed(rings[0])), [uv(0, 0)] * sides)
    if cap_end:
        b.face(rings[-1], [uv(0, len(rings) - 1)] * sides)
    return rings


def arc(start, azim, length, rise, droop, segs, tilt=0.0, wobble=0.0, rng=None):
    """Points of a stem or midrib curve: out along `azim`, up by `rise`, over by `droop`."""
    ca, sa = math.cos(azim), math.sin(azim)
    pts = []
    for i in range(segs + 1):
        t = i / segs
        reach = length * t * math.cos(tilt * t)
        h = rise * math.sin(math.pi * t * 0.5) - droop * t * t + length * t * math.sin(tilt * t)
        x = start[0] + ca * reach
        y = start[1] + sa * reach
        if wobble and rng is not None and 0 < i < segs:
            x += rng.uniform(-wobble, wobble) * length
            y += rng.uniform(-wobble, wobble) * length
            h += rng.uniform(-wobble, wobble) * length * 0.4
        pts.append((x, y, start[2] + h))
    return pts


# Width profiles: half-width factor across 0..1 of the blade's length. The caller
# gives the real half width in metres; these only say how it changes along the blade.
# Aspect ratios (length : full width) are set by the species, from the measured
# reference: aroid blades 1.25-1.45:1, heliconia 3-5:1, banana 2.5-3.5:1.
def _profile(kind, t):
    if kind == "strap":            # banana, heliconia: parallel sides, blunt tip
        return math.sin(math.pi * min(1.0, 0.12 + t * 0.92)) ** 0.35
    if kind == "pinnule":          # fern pinna, palm leaflet: widest near the base
        return math.sin(math.pi * min(1.0, t * 1.1)) ** 0.9 * (1.0 - 0.35 * t)
    if kind == "heart":            # aroid: broadest a third out, basal lobes behind the petiole
        return math.sin(math.pi * min(1.0, 0.22 + t * 0.82)) ** 0.55
    if kind == "wedge":            # cycad leaflet, praemorse palm leaflet: stiff, cut off
        return (0.25 + 0.75 * t) * math.sin(math.pi * min(1.0, t * 1.05)) ** 0.5
    if kind == "fan":              # one segment of a palmate fan
        return 0.35 + 0.65 * t
    return math.sin(math.pi * min(1.0, t * 1.08)) ** 0.7     # ovate


def _along(value, t):
    """A parameter given either as one number or as (base, middle, tip)."""
    if isinstance(value, (tuple, list)):
        base, mid, tip = value
        return _lerp(base, mid, t * 2) if t < 0.5 else _lerp(mid, tip, (t - 0.5) * 2)
    return value


def _cut(lobes, t, side, phase):
    """How much of the lamina survives at `t` on one side of a pinnatifid blade.

    The cuts run 50-80% of the way to the midrib, widen toward the tip, and the two
    sides carry different phases: a real leaf is never mirror-symmetric, and symmetry
    is the first thing that gives a generated plant away.
    """
    if not lobes:
        return 1.0
    depth = _lerp(0.5, 0.8, t)
    wave = abs(math.sin(math.pi * lobes * (0.10 + 0.90 * t) + phase * side))
    return 1.0 - depth * (1.0 - wave ** 0.55)


def bites(rng, chance=0.42, most=3):
    """Notches to take out of one blade's margin, or () for an undamaged leaf.

    The measured reference puts damage on a third to a half of all leaves and calls an
    intact leaf set the loudest tell that something was generated. The holes it
    describes are 3-15 mm, which is texture work and is drawn into the leaf atlas
    already - but past about two metres the texture is gone and the silhouette carries
    everything, so the margin has to be nicked in the mesh as well. Each notch is
    (position along the blade, which side, how deep as a fraction of the half width)
    and costs no triangles: it moves the margin column in, it does not subdivide.
    """
    if rng.random() > chance:
        return ()
    return tuple((rng.uniform(0.12, 0.95), 1 if rng.random() < 0.5 else -1,
                  rng.uniform(0.18, 0.55))
                 for _ in range(rng.randint(1, most)))


def _bitten(bite, t, side):
    """How much of the half width survives at `t` on one side, 1.0 where nothing bit."""
    keep = 1.0
    for at, which, depth in bite:
        if which != side:
            continue
        # A bite is a smooth notch about 12% of the blade long, not a sawtooth.
        keep -= depth * math.exp(-((t - at) / 0.12) ** 2)
    return max(0.08, keep)


def blade(b, base, azim, length, width, *, rise=0.0, droop=0.0, fold=0.2, curl=0.0,
          # Tessellation, not anatomy: how finely the blade's curve, keel fold and
          # ripple are sampled, with no effect on its size or shape. cols=3 made the
          # fold two flat facets and the ripple invisible, so a 2 m musoid blade came
          # back as folded paper - at 3840x2160 the facet edges are plainly visible
          # along every leaf in the library. A caller that wants a cheaper LOD passes
          # p.detail() as it did before.
          twist=0.0, tilt=0.0, segs=14, cols=9, outline="ovate", lobes=0, ripple=0.0,
          holes=(), tears=(), tear_from=0.35, sag=0.0, phase=0.0, u0=0.0, u1=1.0,
          bite=None):
    """One leaf blade as a curved, folded, twisted surface.

    `fold` is the keel: how far the midrib stands above the margins, as a fraction of
    the half width (1.0 is about 45 degrees). Give it as one number or as
    (base, middle, tip) to get the real thing, which is keeled hard where the blade
    leaves the petiole, flat across the middle and curled over at the tip. `curl`
    rolls the margins down and takes the same form. `twist` turns the blade about its
    own midrib, `ripple` is the low wave a thin blade carries, `lobes` cuts a
    pinnatifid margin, `holes` are closed fenestrations (t, s, radius) and `tears` are
    the open splits a banana or heliconia blade shreds into, given as column indices;
    every strip beyond a tear sags by `sag` as it is no longer braced by its neighbour.
    `bite` is what has been eaten out of the margin: left alone, every blade in the
    library draws its own from `bites()` off its builder's rng, which is what puts
    damage on a third to a half of all leaves without each of the twenty-one callers
    having to remember to ask. Pass `bite=()` for a leaf that must be intact.
    `cols` must be odd so a column of vertices sits on the midrib.
    """
    if bite is None:
        bite = bites(b.rng)
    cols = max(3, cols | 1)
    mid = (cols - 1) // 2
    pts = arc(base, azim, length, rise, droop, segs, tilt=tilt)
    fr = frames(pts)
    tears = tuple(tears)
    grid = []
    for i in range(segs + 1):
        t = i / segs
        w = width * _profile(outline, t)
        _, n, bn = fr[i]
        tw = twist * t
        fold_t, curl_t = _along(fold, t), _along(curl, t)
        row = []
        for j in range(cols):
            s = j / (cols - 1) * 2 - 1
            side = 1 if s >= 0 else -1
            off = s * w * _cut(lobes, t, side, phase) * (_bitten(bite, t, side) if bite else 1.0)
            lift = fold_t * w * (1 - s * s) - curl_t * w * s * s
            if ripple:
                lift += ripple * w * math.sin(2.6 * math.pi * t) * (0.2 + 0.8 * abs(s))
            if sag and tears:
                # Count the tears between this column and the midrib: the further out
                # past a split, the more the free strip hangs.
                free = sum(1 for cut in tears if (mid <= cut < j) or (j <= cut < mid))
                lift -= sag * w * free * max(0.0, (t - tear_from) / max(1e-3, 1 - tear_from))
            c, sn = math.cos(tw), math.sin(tw)
            dx = n[0] * off * c + bn[0] * (off * sn + lift)
            dy = n[1] * off * c + bn[1] * (off * sn + lift)
            dz = n[2] * off * c + bn[2] * (off * sn + lift)
            p = pts[i]
            row.append(b.vert(p[0] + dx, p[1] + dy, p[2] + dz))
        grid.append(row)
    for i in range(segs):
        t0, t1 = i / segs, (i + 1) / segs
        tm = (t0 + t1) / 2
        if width * _profile(outline, tm) < 1e-4:
            continue
        for j in range(cols - 1):
            s0 = j / (cols - 1) * 2 - 1
            s1 = (j + 1) / (cols - 1) * 2 - 1
            if j in tears and tm > tear_from:
                continue
            sm = (s0 + s1) / 2
            if any((tm - ht) ** 2 + ((sm - hs) * 0.45) ** 2 < hr * hr for ht, hs, hr in holes):
                continue
            # Winding: along the blade first, then across. The lateral offset grows
            # along the frame's normal and the keel along its binormal, so this order
            # is the one whose face normal comes out of the leaf's upper surface.
            # The other order faces the ground, and a leaf lit from below is the kind
            # of mistake that survives a screenshot and ruins a frame.
            b.face((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]),
                   (b.tile_uv(_lerp(u0, u1, (s0 + 1) / 2), t0),
                    b.tile_uv(_lerp(u0, u1, (s0 + 1) / 2), t1),
                    b.tile_uv(_lerp(u0, u1, (s1 + 1) / 2), t1),
                    b.tile_uv(_lerp(u0, u1, (s1 + 1) / 2), t0)))
    return pts


def frond(b, base, azim, length, *, rise, droop, leaflets, leaflet_len, leaflet_w,
          rachis_r, tilt=0.0, segs=7, angle=(0.9, 1.3), outline="pinnule", droop_leaflet=0.3,
          fold=0.3, rng=None, bare=0.12):
    """A pinnate frond: a rachis tube with paired leaflets whose length follows a profile.

    One leaflet is one blade with cols=3, so the second order of division (a tree
    fern's pinnules) is carried by the leaf texture, not by geometry. That is the
    difference between a 600-triangle fern and a 12,000-triangle one.
    """
    spine = arc(base, azim, length, rise, droop, segs, tilt=tilt)
    # The rachis shares the species' leaf atlas, so it reads off the midrib band.
    tube(b, spine, [rachis_r * (1 - 0.75 * i / segs) for i in range(segs + 1)],
         sides=12, cap_end=True, tile=(0.46, 0.54))
    fr = frames(spine)
    for i in range(1, leaflets + 1):
        t = _lerp(bare, 1.0, i / (leaflets + 1))
        k = t * segs
        lo = min(int(k), segs - 1)
        f = k - lo
        p = tuple(_lerp(spine[lo][d], spine[lo + 1][d], f) for d in range(3))
        tangent = fr[lo][0]
        local = math.atan2(tangent[1], tangent[0])
        # Leaflets are longest two thirds of the way out, short at both ends.
        taper = math.sin(math.pi * min(1.0, 0.12 + t * 0.95)) ** 0.5
        ln = leaflet_len * taper
        if ln < leaflet_len * 0.12:
            continue
        for side in (-1, 1):
            jitter = rng.uniform(*angle) if rng else sum(angle) / 2
            blade(b, p, local + side * jitter, ln, ln * leaflet_w,
                  rise=ln * 0.12, droop=ln * droop_leaflet, fold=fold, curl=0.1,
                  segs=3, cols=3, outline=outline, tilt=-0.25)
    return spine


def rosette(count, phyllo=2.39996, start=0.0):
    """Golden-angle azimuths. Real leaves are not spaced evenly round a stem, and an
    evenly spaced rosette is the first thing that makes a generated plant read as fake."""
    return [start + phyllo * i for i in range(count)]


def fin(b, inner, outer_r, azim, height, thickness, segs=5, curve=1.6, v_scale=1.0):
    """One buttress root: a thin vertical plate from the trunk out to the ground."""
    ca, sa = math.cos(azim), math.sin(azim)
    nx, ny = -sa, ca
    rows = []
    for i in range(segs + 1):
        t = i / segs
        r = _lerp(0.0, outer_r, t)
        top = height * (1 - t) ** curve
        th = thickness * (1 - 0.65 * t)
        pair = []
        for side in (-1, 1):
            x = inner[0] + ca * r + nx * th * side
            y = inner[1] + sa * r + ny * th * side
            pair.append((b.vert(x, y, inner[2] + top), b.vert(x, y, inner[2])))
        rows.append(pair)
    for i in range(segs):
        a, c = rows[i], rows[i + 1]
        v0, v1 = i / segs * v_scale, (i + 1) / segs * v_scale
        for side in (0, 1):
            top0, bot0 = a[side]
            top1, bot1 = c[side]
            # Each face of the plate looks away from the plate, so side 0 (the one
            # offset along -n) winds the other way round.
            quad = (top1, top0, bot0, bot1) if side == 0 else (top0, top1, bot1, bot0)
            b.face(quad, ((0.0, v0), (0.0, v1), (0.35, v1), (0.35, v0)))
        # The ridge along the top of the plate, so it is a solid, not a card.
        b.face((a[0][0], c[0][0], c[1][0], a[1][0]),
               ((0.4, v0), (0.4, v1), (0.6, v1), (0.6, v0)))
    return rows


def disc(b, cx, cy, z, radius, rng, sides=9, bump=0.25, u_repeat=1.0):
    """A low mat (moss, lichen, a litter drift): one irregular, slightly domed fan."""
    centre = b.vert(cx, cy, z + radius * bump * 0.5)
    rim = []
    for k in range(sides):
        a = 2 * math.pi * k / sides
        r = radius * rng.uniform(0.62, 1.0)
        rim.append(b.vert(cx + math.cos(a) * r, cy + math.sin(a) * r,
                          z + radius * bump * rng.uniform(0.0, 0.3)))
    for k in range(sides):
        j = (k + 1) % sides
        b.face((centre, rim[k], rim[j]),
               (b.tile_uv(0.5, 0.5),
                b.tile_uv(0.5 + 0.5 * math.cos(2 * math.pi * k / sides) * u_repeat,
                          0.5 + 0.5 * math.sin(2 * math.pi * k / sides) * u_repeat),
                b.tile_uv(0.5 + 0.5 * math.cos(2 * math.pi * j / sides) * u_repeat,
                          0.5 + 0.5 * math.sin(2 * math.pi * j / sides) * u_repeat)))
    return rim


def flat_leaf(b, cx, cy, z, azim, length, width, rng, curl=0.1, tilt=0.45):
    """A single fallen leaf lying on the ground: two triangles, one atlas tile.

    Wound base-to-tip then across, so the face looks up out of the ground.

    `tilt` is how far out of the horizontal it can lie, in radians. A skirt of leaves
    all lying perfectly flat is the most sun-facing surface in a frame, so it came back
    as the brightest thing in it - a ring of ochre slabs round the base of every plant,
    brighter than any living leaf. Real litter is a jumbled mat: leaves propped on each
    other, cupped, standing on edge. Tilting each one is what breaks that up, and it
    costs nothing.
    """
    ca, sa = math.cos(azim), math.sin(azim)
    nx, ny = -sa, ca
    # One random slope per leaf, as a rise across the blade and along it.
    pitch, roll = rng.uniform(-tilt, tilt), rng.uniform(-tilt, tilt)
    corners, uvs = [], []
    for t, s in ((0.0, -0.35), (1.0, -0.35), (1.0, 0.35), (0.0, 0.35)):
        x = cx + ca * length * (t - 0.5) + nx * width * s
        y = cy + sa * length * (t - 0.5) + ny * width * s
        lift = (length * (t - 0.5) * math.sin(pitch)
                + width * s * math.sin(roll)
                + abs(s) * width * curl)
        corners.append(b.vert(x, y, z + lift + rng.uniform(0, 0.004)))
        uvs.append(b.tile_uv(0.5 + s * 1.15, t))
    b.face(tuple(corners), tuple(uvs))


def litter_skirt(b, rng, radius, count, leaf_len, p, frond=False):
    """The ring of its own dead leaves every plant stands in.

    Nothing in this forest touches bare soil: a palm, a banana or a heliconia sits in
    0.6-1.0 m of its own shed sheaths and fronds, often 10-25 cm deep, and a generated
    plant without one reads as a model dropped onto a surface. Drawn from the atlas's
    dead tile, so it costs no extra material.

    It is a *mat*, not a ring of whole leaves. Litter is mostly broken material - a leaf
    that fell a season ago is in pieces - and a skirt built only from whole shed blades
    reads as a handful of straw strips with stone showing between them, which is what
    the frames showed. So two layers: a few whole leaves, and three times as many
    fragments at a third to two thirds the length, laid thickest at the base and
    thinning outward. Each piece is two triangles, so forty of them cost eighty.
    """
    d = b.dead()
    whole = max(2, int(count * p.detail(1.0, 0.5, 0.25)))
    for i in range(whole * 4):
        fragment = i >= whole
        a = rng.uniform(0, 2 * math.pi)
        # Fragments gather in close, whole leaves fall further out.
        r = radius * ((0.12 + 0.75 * rng.random() ** 1.6) if fragment
                      else (0.35 + 0.65 * math.sqrt(rng.random())))
        length = leaf_len * (rng.uniform(0.3, 0.65) if fragment else rng.uniform(0.6, 1.25))
        flat_leaf(d, math.cos(a) * r, math.sin(a) * r, rng.uniform(0.004, 0.05),
                  rng.uniform(0, 2 * math.pi), length,
                  length * (0.07 if frond and not fragment else rng.uniform(0.25, 0.55)), rng,
                  curl=rng.uniform(0.1, 0.6))
        if fragment and p.lod:
            break


def dying_leaves(b, rng, base, count, length, width, p, **kw):
    """The 10-20% of a crown's leaf area that is dead or yellowing, hanging vertically."""
    d = b.dead()
    for _ in range(max(1, int(count * p.detail(1.0, 0.5, 0.0)))):
        a = rng.uniform(0, 2 * math.pi)
        blade(d, base, a, length * rng.uniform(0.7, 1.0), width,
              rise=-length * 0.15, droop=length * rng.uniform(0.85, 1.1),
              fold=0.15, curl=0.35, twist=rng.uniform(-0.8, 0.8), tilt=-0.9,
              segs=p.detail(12, 3, 2), cols=3, **kw)


def lean_all(bm, lean, lean_dir):
    """Tip a finished specimen about its base, the way a plant grows toward a light gap."""
    if not lean:
        return
    ca, sa = math.cos(lean_dir), math.sin(lean_dir)
    axis = (-sa, ca, 0.0)
    c, s = math.cos(lean), math.sin(lean)
    for v in bm.verts:
        x, y, z = v.co
        # Rodrigues about the horizontal axis through the origin.
        dot = axis[0] * x + axis[1] * y
        cx, cy, cz = (axis[1] * z - axis[2] * y, axis[2] * x - axis[0] * z, axis[0] * y - axis[1] * x)
        v.co.x = x * c + cx * s + axis[0] * dot * (1 - c)
        v.co.y = y * c + cy * s + axis[1] * dot * (1 - c)
        v.co.z = z * c + cz * s + axis[2] * dot * (1 - c)


# ==========================================================================
# The catalogue.
#
# Every entry carries the proportions it was built to and where they come from.
# Heights are metres for a plant growing in closed-canopy lowland forest at
# 150-300 m, the setting docs/reference/mesoamerican-reference.md settles on;
# a species that only belongs in another register says so in `historical`.


# --- Litter and groundcover -----------------------------------------------


@species(
    key="leaf_litter",
    litter="none", dying="none",       # this IS the litter the others stand in
    common="Forest-floor leaf litter",
    latin="(mixed)",
    band="litter",
    height=(0.01, 0.05),
    spread=(0.5, 1.4),
    reference="Litter, not soil, IS the ground surface here: southern Yucatan mature forest "
              "drops about 6.2 Mg/ha/yr (Calakmul 5.2-7.1) of which leaves are 84-91%, and "
              "litterfall peaks in the dry season with a pulse in late March and early April, "
              "so a March scene has the deepest, freshest, most yellow-tan floor. Fallen "
              "blades are mesophyll class, 45-182 cm2, so 12-25 cm long and 5-10 cm wide, with "
              "a long tail of 3-8 cm Fabaceae leaflets. Whole leaves become fragments in about "
              "2.1 years, and the visible layer is replaced roughly annually. Palm frond "
              "segments 1-3 m long weather paler than dicot leaves - straw to silver-grey - "
              "and stay recognisable far longer, so they read as the pale shapes on a dark mat.",
    assumed="Depth in centimetres, ground-cover percentage and leaf count per patch: no source "
            "found. Built as 2-5 cm with 10-20 cm in buttress pockets and against logs, and "
            "90-100% cover under closed canopy.",
    tells="Fresh pale leaves standing proud and curled on a dark flat mat, with 1-3 m "
          "silver-grey palm frond segments as the only large shapes.",
    variants=("Drift", "Scatter", "Deep", "Fronds"),
    parts={"litter": "Litter"},
    # Litter is two materials, not one: the damp, shaded floor is near-neutral
    # (4-10% saturation) and only dry sunlit litter runs warm at 39-43%. The atlas
    # carries both, so one drift can mix them.
    leaf_tex=LeafTex(name="Leaf_Litter", venation="pinnate", outline="ovate",
                     green=hsv(28, 41, 37), young=hsv(22, 8, 52), dead=hsv(26, 43, 32),
                     under=1.05, vein=0.65, vein_count=9, midrib=1.4,
                     blotch=0.75, damage=0.8, gloss=0.15),
)
def _leaf_litter(parts, rng, p, s):
    b = parts("litter")
    kind = s.variants[p.variant]
    count = {"Drift": 26, "Scatter": 14, "Deep": 44, "Fronds": 10}[kind]
    count = int(count * p.detail(1.0, 0.5, 0.25) * _lerp(0.5, 1.0, p.age))
    radius = s.spread_at(p.age) / 2 * p.scale
    for _ in range(max(3, count)):
        a = rng.uniform(0, 2 * math.pi)
        # Drifts pile up at the rim, a scatter is even.
        r = radius * (rng.random() ** (0.4 if kind == "Drift" else 0.5))
        # 12-25 cm blades with a tail of 3-8 cm leaflets.
        length = rng.uniform(0.12, 0.25) if rng.random() < 0.7 else rng.uniform(0.03, 0.08)
        flat_leaf(b, math.cos(a) * r, math.sin(a) * r, rng.uniform(0.0, 0.012),
                  rng.uniform(0, 2 * math.pi), length, length * rng.uniform(0.3, 0.5), rng,
                  curl=rng.uniform(0.05, 0.35))
    if kind == "Fronds":
        # The large pale elements: half-collapsed palm frond segments, 1-3 m.
        d = b.dead()
        for _ in range(2 if p.lod else 3):
            a = rng.uniform(0, 2 * math.pi)
            r = radius * rng.uniform(0.0, 0.6)
            length = rng.uniform(1.0, 2.6)
            blade(d, (math.cos(a) * r, math.sin(a) * r, 0.01), rng.uniform(0, 6.28),
                  length, length * 0.045, rise=length * 0.02, droop=0.0,
                  fold=(0.4, 0.1, 0.0), curl=(0.0, 0.2, 0.5), twist=rng.uniform(-0.6, 0.6),
                  segs=p.detail(14, 4, 3), cols=3, outline="strap", ripple=0.25)


@species(
    key="moss_patch",
    litter="none", dying="none",       # a mat on stone or a log; it has no stem to shed from
    common="Moss mat",
    latin="(Bryophyta, mixed)",
    band="groundcover",
    height=(0.01, 0.06),
    spread=(0.15, 0.7),
    reference="Lowland moss is thin creeping weft, not cushion, and it lives in the bottom "
              "1-2 m: trunk bases, buttresses, rotting logs, permanently shaded or seeping "
              "limestone. Octoblepharum albidum, the signature pale moss of lowland trunks at "
              "about 200 m, makes tufts 0.5-1.5 cm tall of 3-6 mm strap leaves and is described "
              "as whitish green - LIGHTER than the bark it sits on, which is what makes it "
              "recognisable. Mats run 10-40 cm and 3-10 mm thick on bark, up to 1-2 m and "
              "10-25 mm thick on wet limestone. Wetting darkens it and rotates the hue toward "
              "green; it does not simply darken.",
    assumed="Patch counts, and cover on shaded limestone (30-60% on permanently damp shaded "
            "rock, under 5% on sun-struck or dusty rock) are estimates: no source found.",
    tells="A chalky, almost chalk-pale green that is lighter than the stone and bark around "
          "it, hugging the bottom metre and the seeping joints only.",
    variants=("Stone", "Log"),
    parts={"moss": "Moss"},
    # Moss on damp stone is a deep, saturated green, not a pale one. At 22% saturation
    # and value 62 it was the lightest thing in any frame it appeared in and rendered as
    # a blob of butter on the plaza; it was also under the 25% floor that brings a
    # species into the band's colour separation, so nothing pulled it back.
    leaf_tex=LeafTex(name="Leaf_Moss", venation="dichotomous", outline="pinnule",
                     green=hsv(104, 48, 26), young=hsv(96, 58, 34), dead=hsv(44, 26, 36),
                     under=1.1, vein=0.25, vein_count=40, midrib=1.2,
                     blotch=0.45, damage=0.1, gloss=0.3),
)
def _moss_patch(parts, rng, p, s):
    b = parts("moss")
    radius = s.spread_at(p.age) / 2 * p.scale
    lumps = 3 if p.lod else 5
    disc(b, 0, 0, 0, radius, rng, sides=9 if not p.lod else 6, bump=0.18, u_repeat=2.0)
    for _ in range(lumps):
        a = rng.uniform(0, 2 * math.pi)
        r = radius * rng.uniform(0.25, 0.85)
        disc(b, math.cos(a) * r, math.sin(a) * r, rng.uniform(0.0, 0.01),
             radius * rng.uniform(0.25, 0.5), rng, sides=6, bump=0.35, u_repeat=1.2)


@species(
    key="lichen_crust",
    litter="none", dying="none",       # crust on stone, 2-10 mm thick
    common="Crustose lichen",
    latin="(Lecanoromycetes, mixed)",
    band="groundcover",
    height=(0.002, 0.01),
    spread=(0.08, 0.4),
    reference="Crustose lichen, not beard lichen: fruticose hanging strands are a cloud-forest "
              "signature and are essentially absent at 150-300 m, so a furry green trunk reads "
              "as 1500 m and wrecks the altitude. Lowland bark carries flat, paint-like crusts "
              "with no free edge - script lichens as black pen-strokes on off-white, "
              "Pyrenulaceae as cream-ochre crusts with black dots, Herpothallon as felty "
              "white-grey, sometimes rust-orange Trentepohlia algal film. Patches 2-15 cm, "
              "merging into mottled sheets 30-80 cm along a trunk, under 1 mm thick.",
    assumed="Colony count per patch. Cover figures (50-80% on saplings, 20-40% on big trunks) "
            "are scaled down from a Mexican tropical DRY forest study, the only measurement found.",
    tells="Value pattern, not texture: a map-like patchwork of pale off-white and cream with "
          "irregular dark edges, lighter than the stone, and no free margins anywhere.",
    variants=("Pale", "Ochre"),
    parts={"lichen": "Lichen"},
    # Crustose lichen is genuinely pale cream - that is the tell, and it is why it reads
    # lighter than the bark or stone it grows on - but at value 76 it was lighter than
    # the limestone plaza itself and read as a spill rather than a crust.
    leaf_tex=LeafTex(name="Leaf_Lichen", venation="dichotomous", outline="pinnule",
                     green=hsv(58, 14, 62), young=hsv(44, 24, 66), dead=hsv(28, 40, 44),
                     under=1.0, vein=0.15, vein_count=60, midrib=1.1,
                     blotch=0.8, damage=0.3, gloss=0.1),
)
def _lichen_crust(parts, rng, p, s):
    b = parts("lichen")
    radius = s.spread_at(p.age) / 2 * p.scale
    # "Patches 2-15 cm, merging into mottled sheets": the crust is continuous, with
    # irregular dark edges and no free margin anywhere. Six discs scattered inside the
    # patch radius is not a crust - it is three pale blobs on a stone with bare rock
    # between them, which is exactly how this rendered. The colonies have to overlap
    # into one sheet, so there are enough of them to close and they are placed out to
    # the rim rather than clustered at the middle.
    colonies = 8 if p.lod else rng.randint(22, 36)
    for n in range(colonies):
        a = rng.uniform(0, 2 * math.pi)
        # sqrt keeps the placement even across the disc instead of piling up at the
        # centre, which is what a uniform radius does.
        r = radius * math.sqrt(rng.uniform(0.0, 1.0)) * 0.92
        disc(b, math.cos(a) * r, math.sin(a) * r, rng.uniform(0.001, 0.004),
             radius * rng.uniform(0.28, 0.58), rng, sides=7, bump=0.05, u_repeat=1.0)


@species(
    key="forest_grass",
    common="Forest-floor bamboo grass",
    latin="Olyra latifolia / Lasiacis spp.",
    band="groundcover",
    height=(0.3, 1.8),
    spread=(0.3, 1.1),
    reference="Olyra latifolia is a broad-leaved woody grass of Mesoamerican lowland forest, "
              "0.6-4 m but 0.6-1.5 m in deep shade, with woody cane-like culms and distinct "
              "nodes, blades 0.10-0.20 m by 0.030-0.060 m (about 3.3:1). It belongs to gaps, "
              "treefalls, trail and plaza edges, NOT the deep interior - 14 of 96 Guatemalan "
              "records are Peten.",
    assumed="Culm count per clump.",
    tells="Thin bamboo-like canes arching out of a gap, with wide flat drooping blades. Never "
          "a lawn tuft, and never continuous cover.",
    variants=("Clump", "Sparse"),
    parts={"leaf": "GrassBlade"},
    leaf_tex=LeafTex(name="Leaf_Grass", venation="parallel", outline="strap",
                     green=hsv(104, 60, 34), young=hsv(92, 74, 48), dead=hsv(44, 44, 46),
                     under=1.7, vein=0.35, vein_count=11, blotch=0.2, damage=0.35, gloss=0.35),
)
def _forest_grass(parts, rng, p, s):
    b = parts("leaf")
    height = s.height_at(p.age) * p.scale
    culms = (5 if s.variants[p.variant] == "Clump" else 3) if not p.lod else 2
    for a in rosette(culms, start=rng.uniform(0, 2 * math.pi)):
        h = height * rng.uniform(0.6, 1.0)
        cane = arc((0, 0, 0), a, h * 0.25, h * 0.9, h * 0.1, 3, tilt=1.1)
        tube(b, cane, [0.006 * h, 0.005 * h, 0.004 * h, 0.003 * h], sides=p.detail(10, 5, 3), tile=(0.47, 0.53))
        for i, node in enumerate(cane[1:], 1):
            length = h * rng.uniform(0.16, 0.26)
            blade(b, node, a + rng.uniform(-2.6, 2.6) + i, length, length * 0.19,
                  rise=length * 0.25, droop=length * 0.5, fold=0.22, curl=0.08,
                  segs=p.detail(12, 3, 2), cols=3, outline="strap", twist=0.5 * rng.uniform(-1, 1))


@species(
    key="shade_grass",
    common="Hoja de murciélago",
    latin="Pharus latifolius",
    band="groundcover",
    height=(0.15, 1.0),
    spread=(0.4, 1.5),
    reference="The signature grass of closed lowland forest, and nothing like turf: a culm "
              "0.30-1.00 m with a stem only about 3 mm thick, mostly hidden by its own blades, "
              "each 0.15-0.30 m long and 0.03-0.08 m wide (about 4:1) on a false petiole "
              "0.05-0.07 m. The blades are RESUPINATE - the false petiole twists so the "
              "morphological underside faces up - and sit near-horizontal, like scattered "
              "plates. There is no grass floor in this forest: at Los Tuxtlas the lowland "
              "herb layer was 23 species dominated by aroids (13) and ferns (4), with no "
              "grasses or sedges among the dominants, so plant this as isolated accents on "
              "brown litter, never as cover.",
    assumed="Blade angle, tuft count, and herbaceous cover (5-15% under intact canopy, 40-80% "
            "only in treefall gaps): no source gives a percentage.",
    tells="Broad near-horizontal blades sitting low and flat like scattered plates, widely "
          "separated, with brown litter between them.",
    variants=("Tuft", "Single"),
    parts={"leaf": "ShadeGrassLeaf"},
    leaf_tex=LeafTex(name="Leaf_ShadeGrass", venation="parallel", outline="strap",
                     green=hsv(110, 52, 36), young=hsv(98, 64, 50), dead=hsv(42, 40, 44),
                     under=1.6, vein=0.4, vein_count=16, blotch=0.25, damage=0.3, gloss=0.4),
)
def _shade_grass(parts, rng, p, s):
    b = parts("leaf")
    height = s.height_at(p.age) * p.scale
    blades = (4 if s.variants[p.variant] == "Tuft" else 2) if not p.lod else 2
    for i, a in enumerate(rosette(blades, start=rng.uniform(0, 6.28))):
        z = height * rng.uniform(0.1, 0.55)
        stalk = arc((0, 0, 0), a, height * 0.08, z, 0.0, 2, tilt=1.3)
        tube(b, stalk, [0.0018, 0.0016, 0.0014], sides=p.detail(10, 5, 3), tile=(0.47, 0.53))
        length = rng.uniform(0.15, 0.30)
        # Held almost flat, and twisted: the blade shows its underside to the sky.
        blade(b, stalk[-1], a + rng.uniform(-0.5, 0.5), length, length * 0.14,
              rise=length * 0.12, droop=length * 0.18, fold=-0.1, curl=0.05,
              twist=math.pi * rng.uniform(0.8, 1.0), tilt=-0.1,
              segs=p.detail(12, 3, 2), cols=3, outline="strap", ripple=0.08)


@species(
    key="spikemoss",
    litter="none",                     # the mat lies on the litter rather than in a skirt
    common="Spikemoss mat",
    latin="Selaginella spp.",
    band="groundcover",
    height=(0.05, 0.3),
    spread=(0.2, 0.8),
    reference="Selaginella forms low, frondose mats 5-30 cm high over deep-shade forest floor "
              "and damp stone in the Petén, with flat, finely divided sprays held almost "
              "horizontally, often with a blue-green iridescence in low light.",
    assumed="Spray count per mat.",
    tells="A flat, fern-like carpet, far finer than any seedling, hugging the ground.",
    variants=("Mat",),
    parts={"leaf": "Spikemoss"},
    leaf_tex=LeafTex(name="Leaf_Spikemoss", venation="dichotomous", outline="pinnule",
                     green=hsv(136, 58, 26), young=hsv(120, 66, 40), dead=hsv(40, 38, 32),
                     under=1.5, vein=0.3, vein_count=26, blotch=0.25, damage=0.15, gloss=0.55),
)
def _spikemoss(parts, rng, p, s):
    b = parts("leaf")
    height = s.height_at(p.age) * p.scale
    spread = s.spread_at(p.age) / 2 * p.scale
    for a in rosette(6 if not p.lod else 3, start=rng.uniform(0, 2 * math.pi)):
        r = spread * rng.uniform(0.0, 0.5)
        base = (math.cos(a) * r, math.sin(a) * r, 0.0)
        length = spread * rng.uniform(0.5, 0.9)
        blade(b, base, a + rng.uniform(-0.4, 0.4), length, length * 0.45,
              rise=height * 0.5, droop=height * 0.2, fold=0.08, curl=-0.05,
              segs=p.detail(12, 3, 2), cols=3, outline="pinnule", lobes=4)


# --- Understory -----------------------------------------------------------


@species(
    key="tree_fern",
    common="Tree fern",
    latin="Sphaeropteris myosuroides (= Cyathea myosuroides)",
    band="understory",
    height=(0.4, 7.0),
    spread=(1.2, 4.0),
    reference="The lowland Maya tree fern, not the montane one: GBIF holds 181 Guatemalan and "
              "143 Belizean records of S. myosuroides, with hard Peten records at 233 m (San "
              "Luis) and Cockscomb Basin at 100 m, while Alsophila cuspidata is montane here "
              "(all three Guatemalan records Alta Verapaz, 956 m). Trunk 6-12 cm across, "
              "3-7 m (typically 4-5); 8-15 bipinnate fronds 1.5-4 m (a measured frond 2.35 x "
              "1.04 m); crown 3-4 m, wider than the plant is tall. Only 10 of 2,528 Guatemalan "
              "Cyatheaceae records are Peten, so place it in ravines, streamsides and damp "
              "limestone sinks, never on dry karst upland.",
    assumed="Frond count per crown, and the juvenile rosette size. The second and third orders "
            "of division are painted into the leaf texture instead of built, which is what "
            "keeps a specimen near 1 k triangles.",
    tells="A slender, bare, almost-black pole with no dead-frond skirt, carrying one flat "
          "shuttlecock of lace-fine fronds wider than the plant is tall.",
    variants=("Mature", "Young", "Leaning"),
    parts={"leaf": "FernFrond", "trunk": "FernTrunk"},
    leaf_tex=LeafTex(name="Leaf_TreeFern", venation="pinnate", outline="pinnule",
                     green=hsv(108, 64, 29), young=hsv(94, 78, 45), dead=hsv(34, 48, 36),
                     under=1.9, vein=0.6, vein_count=22, blotch=0.25, damage=0.3, gloss=0.3),
    bark_tex=BarkTex(name="Bark_FernTrunk", style="fibrous", colour=hsv(26, 36, 24),
                     lichen=0.2, moss=0.45, relief=1.3),
)
def _tree_fern(parts, rng, p, s):
    kind = s.variants[p.variant]
    age = p.age * (0.45 if kind == "Young" else 1.0)
    height = s.height_at(age) * p.scale
    trunk_h = height * 0.62
    t = parts("trunk")
    segs = p.detail(6, 4, 3)
    axis = arc((0, 0, 0), rng.uniform(0, 2 * math.pi), trunk_h * 0.08, trunk_h, 0.0, segs,
               tilt=1.45, wobble=0.012, rng=rng)
    r = 0.045 * _lerp(0.6, 1.0, age)      # 6-12 cm across the trunk, not the 20 cm of a montane Cyathea
    tube(t, axis, [r * (1.25 - 0.35 * i / segs) for i in range(segs + 1)],
         sides=p.detail(7, 5, 4), v_scale=1.2, u_repeat=2.0)
    crown = axis[-1]
    b = parts("leaf")
    # A mature Sphaeropteris crown carries 15-25 fronds. Thirteen left the crown
    # transparent against the sky, which is the opposite of the reference's "sky
    # through the mass is a few percent at most, as small scattered holes".
    fronds = int(_lerp(7, 20, age) * p.detail(1.0, 0.6, 0.35))
    for i, a in enumerate(rosette(max(3, fronds), start=rng.uniform(0, 2 * math.pi))):
        old = i / max(1, fronds - 1)              # 0 newest .. 1 oldest and most arched
        length = height * _lerp(0.30, 0.46, old) * rng.uniform(0.9, 1.1)
        frond(b, crown, a, length,
              rise=length * _lerp(0.55, 0.22, old), droop=length * _lerp(0.25, 0.75, old),
              leaflets=p.detail(9, 6, 4), leaflet_len=length * 0.26, leaflet_w=0.3,
              rachis_r=0.012 * height / 3, tilt=_lerp(0.5, -0.1, old), segs=p.detail(16, 4, 3),
              rng=rng, outline="pinnule", fold=0.35)
    if age > 0.3 and not p.lod:
        # Two croziers standing in the middle of the crown: the tell of a living fern.
        for a in (rng.uniform(0, 6.28), rng.uniform(0, 6.28)):
            c = arc(crown, a, height * 0.05, height * 0.16, 0.0, 4, tilt=1.3)
            tube(b, c, [0.012, 0.011, 0.010, 0.013, 0.016], sides=p.detail(10, 5, 3), cap_end=True,
                 tile=(0.44, 0.56))


@species(
    key="philodendron",
    common="Lobed philodendron",
    latin="Philodendron radiatum",
    band="understory",
    height=(0.4, 2.0),
    spread=(0.8, 2.4),
    reference="Croat's revision gives P. radiatum blades 0.27-1.01 m long by 0.25-0.90 m wide "
              "- length to width about 1.1-1.2:1, so nearly as wide as long, never a long quad "
              "- on petioles 0.28-1.08 m, with internodes 3-12 cm and 1-8 cm thick, incised to "
              "within 1-8 cm of the midrib, about 7-13 divisions a side, each pinna 3.0-4.5 cm "
              "wide. It climbs 3-12 m with the stem flat against the bark. Aroids and ferns, "
              "not grasses, dominate this forest's herb layer (13 of 23 understory species at "
              "Los Tuxtlas were aroids).",
    assumed="Leaf count per rosette and climbing height.",
    tells="A near-black-green hand of narrow finger-lobes up to a metre across, off a thick "
          "green stem glued flat to the trunk, with pale cord roots dropping free below.",
    variants=("Terrestrial", "Climbing", "Juvenile"),
    parts={"leaf": "PhilodendronLeaf", "stem": "PhilodendronStem"},
    leaf_tex=LeafTex(name="Leaf_Philodendron", venation="pinnate", outline="lobed",
                     green=hsv(132, 56, 25), young=hsv(116, 70, 42), dead=hsv(38, 44, 38),
                     under=1.6, vein=0.7, vein_count=9, blotch=0.2, damage=0.3, gloss=0.65),
)
def _philodendron(parts, rng, p, s):
    kind = s.variants[p.variant]
    age = p.age * (0.4 if kind == "Juvenile" else 1.0)
    height = s.height_at(age) * p.scale
    leaves = int(_lerp(3, 7, age) * p.detail(1.0, 0.7, 0.4))
    st, b = parts("stem"), parts("leaf")
    climb = kind == "Climbing"
    for i, a in enumerate(rosette(max(2, leaves), start=rng.uniform(0, 2 * math.pi))):
        old = i / max(1, leaves - 1)
        z = height * (0.1 + 0.75 * old) if climb else 0.0
        petiole = height * _lerp(0.30, 0.55, old) * rng.uniform(0.85, 1.1)
        rise = petiole * _lerp(0.9, 0.5, old)
        spine = arc((0, 0, z), a, petiole, rise, petiole * 0.12, 4, tilt=0.6)
        tube(st, spine, [0.016 * height ** 0.6 * (1 - 0.4 * k / 4) for k in range(5)],
             sides=p.detail(12, 6, 4), v_scale=3.0)
        tip = spine[-1]
        blade_len = min(1.0, height * _lerp(0.26, 0.42, old))
        # Half width 0.45 of the length: the blade is almost as wide as it is long.
        blade(b, tip, a + rng.uniform(-0.3, 0.3), blade_len, blade_len * 0.45,
              rise=blade_len * 0.05, droop=blade_len * 0.45,
              fold=(0.55, 0.15, 0.3), curl=(0.0, 0.1, 0.35),
              twist=rng.uniform(-0.35, 0.35), tilt=-0.5, phase=rng.uniform(0, 3.0),
              segs=p.detail(18, 5, 3), cols=p.detail(14, 5, 3), outline="heart",
              lobes=0 if kind == "Juvenile" else p.detail(9, 5, 0), ripple=0.12)
    if climb and not p.lod:
        # Cord roots hanging free of the stem: the tell that it is climbing, not standing.
        for k in range(3):
            z = height * rng.uniform(0.3, 0.9)
            a = rng.uniform(0, 6.28)
            drop = height * rng.uniform(0.2, 0.5)
            pts = [(math.cos(a) * 0.05, math.sin(a) * 0.05, z),
                   (math.cos(a) * 0.09, math.sin(a) * 0.09, z - drop * 0.5),
                   (math.cos(a) * 0.10, math.sin(a) * 0.10, z - drop)]
            tube(st, pts, [0.008, 0.007, 0.006], sides=p.detail(10, 5, 3), v_scale=2.0, cap_end=True)


@species(
    key="monstera",
    common="Window-leaf monstera",
    latin="Monstera acuminata (NOT M. deliciosa)",
    band="understory",
    height=(0.5, 12.0),
    spread=(0.6, 2.8),
    reference="M. deliciosa is montane here - 450-2620 m, and every georeferenced Guatemalan "
              "record is Sacatepequez, Alta Verapaz, Solola or Guatemala, none in Peten, Belize "
              "or Yucatan - so the lowland species is M. acuminata (Belize Toledo, 50-1000 m) "
              "or M. acacoyaguensis (0-200 m). M. acuminata: internodes 1.0-8.5 cm and 1-3 cm "
              "thick, blades 0.35-0.72 x 0.15-0.40 m (1.8-2.3:1), petioles 0.25-0.66 m, and "
              "1-6 (rarely 9) oblong-elliptical holes set between the midrib and the margin, "
              "with 6-15 lateral veins a side at 35-60 degrees. M. tuberculata's juvenile "
              "shingle phase is 5-10 cm, leathery, pressed flat to the host, and has no holes.",
    assumed="Hole placement is procedural along the laterals; real fenestrations are more "
            "irregular toward the margin. Climbing height.",
    tells="A short vertical file of sickle blades on long erect petioles, each punched with one "
          "or two rows of long oval holes that read as light, not as leaf.",
    variants=("Climber", "Shingle", "Ground"),
    parts={"leaf": "MonsteraLeaf", "stem": "MonsteraStem"},
    leaf_tex=LeafTex(name="Leaf_Monstera", venation="pinnate", outline="heart",
                     green=hsv(96, 50, 30), young=hsv(86, 66, 46), dead=hsv(42, 42, 40),
                     under=1.5, vein=0.55, vein_count=8, blotch=0.15, damage=0.2, gloss=0.8),
)
def _monstera(parts, rng, p, s):
    kind = s.variants[p.variant]
    # The species range runs to 12 m because one of these three climbs. Taking it
    # whole gave Shingle - M. tuberculata's juvenile phase, which is pressed flat to
    # bark with 5-10 cm leathery blades - a twelve-metre spine carrying 9 cm leaves,
    # and it rendered as a bare pole with specks on it. Each variant is sized from
    # what it actually is.
    reach = {"Climber": 1.0, "Shingle": 0.22, "Ground": 0.35}[kind]
    height = s.height_at(p.age) * p.scale * reach
    st, b = parts("stem"), parts("leaf")
    climb_h = height if kind != "Ground" else height * 0.2
    # A node every 25-40 cm of climb, not three to eight for any length of vine: the
    # tell is "a short vertical file of sickle blades", and a file needs blades in it.
    steps = max(3, int(climb_h / rng.uniform(0.25, 0.40) * p.detail(1.0, 0.6, 0.4)))
    spine = arc((0, 0, 0), rng.uniform(0, 6.28), climb_h * 0.06, climb_h, 0.0,
                max(3, steps), tilt=1.45, wobble=0.01, rng=rng)
    tube(st, spine, [0.022 * (1 - 0.3 * i / max(1, len(spine) - 1)) for i in range(len(spine))],
         sides=p.detail(12, 6, 4), v_scale=2.0)
    for i in range(1, len(spine)):
        up = i / (len(spine) - 1)
        if kind == "Shingle":
            # M. tuberculata's juvenile phase: small, entire, pressed flat to the bark.
            size, holes = 0.09, ()
        else:
            # Blades enlarge as the vine climbs, but a real M. acuminata blade stops at 0.72 m.
            size = min(0.72, height * _lerp(0.045, 0.09, up))
            rows = 2 if up > 0.4 else 1
            holes = tuple((0.3 + 0.22 * k, side * rng.uniform(0.40, 0.68), 0.085)
                          for k in range(rows) for side in (-1, 1)
                          if rng.random() < 0.8)      # 1-6 holes, not a regular grid
        a = 2.39996 * i + rng.uniform(-0.2, 0.2)
        stalk = arc(spine[i], a, size * 0.7, size * 0.35, size * 0.05, 3, tilt=0.5)
        tube(st, stalk, [0.012, 0.011, 0.010, 0.009], sides=p.detail(10, 5, 3), v_scale=3.0)
        blade(b, stalk[-1], a, size, size * 0.25,     # 0.35-0.72 long by 0.15-0.40 wide
              rise=size * 0.04, droop=size * 0.5, fold=(0.5, 0.15, 0.25), curl=(0.0, 0.1, 0.3),
              tilt=-0.55, phase=rng.uniform(0, 3.0),
              segs=p.detail(18, 5, 3), cols=p.detail(12, 5, 3), outline="heart",
              holes=() if p.lod > 1 else holes, ripple=0.08)


@species(
    key="heliconia",
    litter="own",                      # its builder sheds its own strap blades
    common="Platanillo",
    latin="Heliconia latispatha",
    band="understory",
    height=(0.8, 3.0),
    spread=(1.0, 2.4),
    reference="Lowland Heliconia stand 1.5-3 m, with banana-like blades 0.6-1.2 m long and "
              "0.15-0.3 m wide on long sheathing petioles, held steeply erect and tearing along "
              "the lateral veins with age; the inflorescence is a zig-zag of red-orange bracts.",
    assumed="Bract geometry is simplified to one zig-zag spike; the bracts' exact count varies.",
    tells="Erect, torn, strap-shaped blades in one plane, with a flame-coloured spike.",
    variants=("Clump", "Flowering", "Young"),
    parts={"leaf": "HeliconiaLeaf", "stem": "HeliconiaStem", "bract": "HeliconiaBract"},
    leaf_tex=LeafTex(name="Leaf_Heliconia", venation="parallel", outline="strap",
                     green=hsv(114, 66, 33), young=hsv(100, 80, 48), dead=hsv(36, 50, 42),
                     under=1.8, vein=0.45, vein_count=34, blotch=0.2, damage=0.55, gloss=0.5),
)
def _heliconia(parts, rng, p, s):
    kind = s.variants[p.variant]
    age = p.age * (0.45 if kind == "Young" else 1.0)
    height = s.height_at(age) * p.scale
    st, b = parts("stem"), parts("leaf")
    # A Heliconia stand is a thicket, not a bouquet: the reference measures mats three
    # to five layers deep with no leaf in any photograph fully visible, and each leaf
    # 30-60% occluded. Six shoots of four to seven blades gets the clump to that; three
    # shoots of five read as a potted plant from any distance. The band's triangle
    # ceiling has the room, and a plant that needs geometry to read as a plant gets it.
    shoots = (6 if kind == "Clump" else 3) if not p.lod else 1
    for shoot in range(shoots):
        sa = rng.uniform(0, 6.28)
        # Shoots of one clump come off one rhizome: 10-40 cm apart, not metres. Spaced
        # by a fraction of the plant's height they drifted to 2.6 m and read as three
        # separate plants standing near each other.
        sr = min(0.40, height * 0.05 * shoot) * rng.uniform(0.6, 1.0)
        base = (math.cos(sa) * sr, math.sin(sa) * sr, 0.0)
        blades = int(_lerp(4, 7, age))
        # Shoots of one clump are of very different ages - the reference puts a x3.5 to
        # x6 size range inside one species in one frame, and a clump is where that
        # starts - so each shoot after the first is a fraction of the leader's size.
        shoot_scale = 1.0 if shoot == 0 else rng.uniform(0.35, 0.85)
        for i, a in enumerate(rosette(blades, start=sa + shoot)):
            old = i / max(1, blades - 1)
            length = (height * shoot_scale
                      * _lerp(0.85, 0.60, old) * rng.uniform(0.9, 1.05))
            stalk = arc(base, a, length * 0.12, length * 0.45, 0.0, 3, tilt=1.25)
            tube(st, stalk, [0.022 * height / 2, 0.020 * height / 2, 0.017 * height / 2,
                             0.014 * height / 2], sides=p.detail(12, 6, 4), v_scale=2.0)
            # The keel is the whole read of a musoid leaf: 20-60 degrees of fold at the
            # base, flat across the middle, curling over again at the tip. Older blades
            # tear along the secondary veins; the newest one is still whole.
            tears = () if old < 0.25 else tuple(
                k for k in (1, 3, 5) if rng.random() < 0.35 + old + 0.3 * (1 - p.health))
            blade(b, stalk[-1], a, length * 0.75, length * 0.11,
                  rise=length * _lerp(0.40, 0.18, old), droop=length * _lerp(0.25, 0.6, old),
                  fold=(0.95, 0.18, 0.5), curl=(0.0, 0.05, 0.25),
                  twist=rng.uniform(-0.5, 0.5), tilt=_lerp(0.8, 0.15, old),
                  segs=p.detail(20, 5, 3), cols=p.detail(14, 5, 3), outline="strap", ripple=0.16,
                  tears=tears, tear_from=_lerp(0.6, 0.3, old), sag=0.5)
        if kind == "Flowering" and shoot == 0 and not p.lod:
            br = parts("bract")
            spike = arc(base, sa, height * 0.1, height * 0.95, 0.0, 5, tilt=1.3)
            tube(br, spike, [0.012, 0.011, 0.010, 0.009, 0.008, 0.006], sides=p.detail(10, 5, 3), v_scale=4.0)
            for i, node in enumerate(spike[1:5], 1):
                side = 1 if i % 2 else -1
                length = height * 0.10
                blade(br, node, sa + side * 1.2, length, length * 0.30,
                      rise=length * 0.35, droop=length * 0.1, fold=0.5, curl=-0.2,
                      segs=3, cols=3, outline="pinnule", tilt=0.5)
    # One skirt for the clump, not one per shoot. Inside the loop above, a six-shoot
    # clump shed six skirts of fourteen half-metre blades: eighty-four pieces of dead
    # heliconia around one plant, measured at 63% of the frame against the 10-45% the
    # reference allows, with the mass reading brown at hue 38 instead of green. The
    # shoots come off one rhizome and the blades they drop land in one pile.
    litter_skirt(b, rng, max(0.6, height * 0.3), 16, height * 0.16, p)


@species(
    key="calathea",
    common="Hoja de sal / maxán",
    latin="Goeppertia lutea (Calathea lutea)",
    band="understory",
    height=(0.8, 3.0),
    spread=(1.0, 2.6),
    reference="Calathea lutea stands 2-3 m in wet lowland openings, with upright paddle blades "
              "0.5-1.0 m long and 0.3-0.5 m wide on long petioles; the underside carries a waxy "
              "white bloom that flashes when the leaf turns. The leaves are the tamale wrapper "
              "of the Maya lowlands, so it belongs near any inhabited clearing.",
    assumed="Leaf count per clump.",
    tells="Huge upright paddles with a chalk-white back, standing almost vertically.",
    variants=("Clump", "Single"),
    parts={"leaf": "CalatheaLeaf", "stem": "CalatheaStem"},
    leaf_tex=LeafTex(name="Leaf_Calathea", venation="parallel", outline="strap",
                     green=hsv(126, 52, 34), young=hsv(110, 68, 49), dead=hsv(40, 46, 44),
                     under=2.4, vein=0.4, vein_count=40, blotch=0.15, damage=0.3, gloss=0.45),
)
def _calathea(parts, rng, p, s):
    height = s.height_at(p.age) * p.scale
    st, b = parts("stem"), parts("leaf")
    # Goeppertia throws a sheaf of blades from the rhizome, 5-12 in a clump.
    leaves = max(2, int(_lerp(5, 12, p.age)
                        * (1.0 if s.variants[p.variant] == "Clump" else 0.35)))
    for i, a in enumerate(rosette(max(1, int(leaves * p.detail(1.0, 0.7, 0.5))),
                                  start=rng.uniform(0, 6.28))):
        old = i / max(1, leaves - 1)
        petiole = height * _lerp(0.55, 0.40, old)
        spine = arc((0, 0, 0), a, petiole * 0.25, petiole, 0.0, 4, tilt=1.35)
        tube(st, spine, [0.020, 0.018, 0.016, 0.014, 0.012], sides=p.detail(12, 6, 4), v_scale=2.5)
        length = height * _lerp(0.40, 0.30, old)
        # A paddle over 1 m long and 0.6 m wide, keeled where it leaves the petiole,
        # flat across the middle, tipping over at the end.
        blade(b, spine[-1], a, length, length * 0.30,
              rise=length * 0.3, droop=length * 0.35, fold=(0.7, 0.12, 0.35), curl=(0.0, 0.05, 0.2),
              tilt=_lerp(0.9, 0.4, old), twist=rng.uniform(-0.3, 0.3),
              segs=p.detail(18, 5, 3), cols=p.detail(12, 3, 3), outline="strap", ripple=0.1)


@species(
    key="bromeliad_ground",
    common="Pita / wild pineapple",
    latin="Aechmea magdalenae",
    band="understory",
    height=(0.4, 1.8),
    spread=(0.9, 2.2),
    reference="A. magdalenae forms dense terrestrial rosettes in Petén forest, its spine-edged "
              "strap leaves 1-2 m long and 5-10 cm wide, recurving from an upright base; the "
              "fibre was worked into cordage. It makes near-impassable thickets.",
    assumed="Spine geometry is left to the texture; building spines would cost more than the leaf.",
    tells="A fountain of stiff, spine-edged straps with a reddish base and recurved tips.",
    variants=("Rosette", "Thicket"),
    parts={"leaf": "BromeliadLeaf"},
    leaf_tex=LeafTex(name="Leaf_BromeliadGround", venation="parallel", outline="strap",
                     green=hsv(88, 46, 31), young=hsv(76, 58, 44), dead=hsv(32, 48, 38),
                     under=1.5, vein=0.3, vein_count=18, blotch=0.35, damage=0.45, gloss=0.4),
)
def _bromeliad_ground(parts, rng, p, s):
    b = parts("leaf")
    height = s.height_at(p.age) * p.scale
    heads = 2 if s.variants[p.variant] == "Thicket" and not p.lod else 1
    for head in range(heads):
        ha = rng.uniform(0, 6.28)
        hr = height * 0.25 * head
        base = (math.cos(ha) * hr, math.sin(ha) * hr, 0.0)
        count = int(_lerp(7, 13, p.age) * p.detail(1.0, 0.6, 0.4))
        for i, a in enumerate(rosette(max(4, count), start=ha)):
            old = i / max(1, count - 1)
            length = height * _lerp(1.0, 0.72, old) * rng.uniform(0.9, 1.05)
            blade(b, base, a, length, length * 0.055,
                  rise=length * _lerp(0.72, 0.42, old), droop=length * _lerp(0.4, 0.85, old),
                  fold=0.5, curl=0.0, tilt=_lerp(1.1, 0.55, old),
                  segs=p.detail(16, 4, 3), cols=3, outline="strap", twist=rng.uniform(-0.3, 0.3))


@species(
    key="cycad",
    common="Camotillo cycad",
    latin="Zamia prasina (= Z. polymorpha)",
    band="groundcover",
    height=(0.2, 1.3),
    spread=(0.5, 1.6),
    reference="The Maya-lowland cycad is Z. prasina, distributed through Belize, Peten, "
              "Campeche, Chiapas, Tabasco, Quintana Roo and Yucatan (GBIF: MX 336, BZ 128, "
              "GT 14). Z. loddigesii is Tamaulipas to Chiapas and Dioon is montane, so neither "
              "belongs. The stem is HYPOGEOUS - subterranean, to 10-15 cm - so there is no "
              "trunk to model; that is the commonest mistake in game cycads. 2-3 leaves "
              "(range 1-5) 0.5-1.5 m, petiole 0.15-0.50 m and prickled, 3-12 leaflet pairs, "
              "largest leaflets 0.20-0.30 x 0.02-0.08 m (about 5:1), serrate on the upper two "
              "thirds, with many fine parallel veins and no midrib. It grows on rocky limestone.",
    assumed="Cones are omitted; only the foliage is built.",
    tells="An apparently trunkless, stiff rosette of a handful of very large saw-edged "
          "leaflets standing straight out of bare limestone litter: a fern-palm with no stem "
          "and far too few leaves.",
    variants=("Shade", "Sun"),
    parts={"leaf": "CycadLeaf"},
    leaf_tex=LeafTex(name="Leaf_Cycad", venation="parallel", outline="wedge",
                     green=hsv(140, 62, 24), young=hsv(124, 70, 38), dead=hsv(36, 42, 34),
                     under=1.5, vein=0.5, vein_count=16, blotch=0.2, damage=0.2, gloss=0.75),
)
def _cycad(parts, rng, p, s):
    """Sun leaves are coriaceous: narrower, stiffer, more upright. Shade leaves are
    papyraceous: broader, flatter, darker, and arch over."""
    sun = s.variants[p.variant] == "Sun"
    height = s.height_at(p.age) * p.scale
    # A Zamia is a stiff rosette of pinnate fronds, not one arching leaf: 3-8 on a
    # mature plant. At one or two it rendered as a stray frond beside its own dead one.
    leaves = max(2, int(_lerp(3, 8, p.age) * p.detail(1.0, 0.7, 0.5)))
    for i, a in enumerate(rosette(leaves, start=rng.uniform(0, 6.28))):
        length = height * rng.uniform(0.85, 1.1)
        frond(parts("leaf"), (0, 0, 0.02), a, length,
              rise=length * (0.72 if sun else 0.55), droop=length * (0.22 if sun else 0.45),
              leaflets=p.detail(7, 5, 4),           # 3-12 pairs; the texture carries the teeth
              leaflet_len=length * 0.26, leaflet_w=0.20 if sun else 0.26,
              rachis_r=0.008 * height, tilt=0.95 if sun else 0.6,
              segs=p.detail(14, 4, 3), rng=rng, angle=(1.1, 1.35),
              outline="wedge", droop_leaflet=0.06, fold=0.18 if sun else 0.1, bare=0.3)


@species(
    key="pacaya_palm",
    litter="own",                      # its builder sheds pinnae
    common="Pacaya palm",
    latin="Chamaedorea tepejilote",
    band="understory",
    height=(0.8, 4.0),
    spread=(0.8, 2.2),
    reference="Chamaedorea tepejilote is the common understory palm of Petén and Chiapas wet "
              "forest: a solitary ringed cane 2-6 cm thick to about 3-5 m, with 4-8 pinnate "
              "leaves 0.6-1.2 m long whose leaflets are broad and few. Its male inflorescence "
              "is eaten; it grows where the canopy is closed.",
    assumed="Leaf count per crown.",
    tells="A green bamboo-like cane with prominent rings and a loose crown of few broad leaflets.",
    variants=("Mature", "Young", "Pair"),
    parts={"leaf": "PacayaLeaf", "trunk": "PacayaCane"},
    leaf_tex=LeafTex(name="Leaf_Pacaya", venation="parallel", outline="pinnule",
                     green=hsv(118, 58, 31), young=hsv(104, 72, 46), dead=hsv(38, 46, 40),
                     under=1.7, vein=0.4, vein_count=13, blotch=0.2, damage=0.3, gloss=0.6),
    bark_tex=BarkTex(name="Bark_PalmCane", style="ringed", colour=hsv(78, 30, 32),
                     lichen=0.25, moss=0.3, relief=0.8),
)
def _pacaya(parts, rng, p, s):
    kind = s.variants[p.variant]
    age = p.age * (0.4 if kind == "Young" else 1.0)
    canes = 2 if kind == "Pair" and not p.lod else 1
    height = s.height_at(age) * p.scale
    for cane in range(canes):
        ca = rng.uniform(0, 6.28)
        base = (math.cos(ca) * 0.12 * cane, math.sin(ca) * 0.12 * cane, 0.0)
        h = height * (1.0 if cane == 0 else rng.uniform(0.6, 0.85))
        segs = p.detail(6, 4, 3)
        axis = arc(base, ca, h * 0.10, h * 0.72, 0.0, segs, tilt=1.42, wobble=0.008, rng=rng)
        tube(parts("trunk"), axis, [0.030 * (1 - 0.15 * i / segs) for i in range(segs + 1)],
             sides=p.detail(6, 5, 4), v_scale=3.0, u_repeat=1.0)
        crown = axis[-1]
        # Chamaedorea tepejilote carries 4-8 fronds. At three it read as a cane with a
        # dead frond beside it rather than a palm.
        leaves = int(_lerp(4, 8, age) * p.detail(1.0, 0.7, 0.5))
        for i, a in enumerate(rosette(max(3, leaves), start=ca)):
            old = i / max(1, leaves - 1)
            length = h * _lerp(0.30, 0.22, old)
            frond(parts("leaf"), crown, a, length,
                  rise=length * _lerp(0.5, 0.15, old), droop=length * _lerp(0.2, 0.6, old),
                  leaflets=p.detail(5, 4, 3), leaflet_len=length * 0.38, leaflet_w=0.26,
                  rachis_r=0.009, tilt=_lerp(0.4, -0.15, old), segs=p.detail(14, 4, 3), rng=rng,
                  outline="pinnule", fold=0.3, bare=0.2)
        litter_skirt(parts("leaf"), rng, max(0.5, h * 0.18), 4, h * 0.10, p)


@species(
    key="fan_palm",
    litter="own",                      # its builder sheds fan segments, not blades
    common="Give-and-take palm / escoba",
    latin="Cryosophila stauracantha (= C. argentea)",
    band="mid",
    height=(1.0, 8.0),
    spread=(1.6, 3.0),
    reference="Belize, Peten and Chiapas, to 8 m and often on limestone (GBIF MX 245, BZ 82, "
              "GT 34). The leaf is PALMATE, not costapalmate - there is no midrib running out "
              "into the blade - with about 44 ribs and 9 divisions a side, cleft almost to the "
              "petiole, the blade about 1.0-1.4 m across on a petiole 0.70-1.20 m, green above "
              "and rusty-scurfy beneath. The trunk is a very slender pole about 5 cm thick, "
              "armed with pungent simple or branched spiny rhizoids pointing downward: the "
              "give-and-take.",
    assumed="Spine length and density are printed nowhere reachable (5-15 cm, dense over the "
            "lower 1-3 m, branched into 2-5 points); they are carried by the bark texture. "
            "Leaf count per crown.",
    tells="The silver-white underside flashing over a trunk bristling with downward root-spines.",
    variants=("Mature", "Young"),
    parts={"leaf": "FanPalmLeaf", "trunk": "FanPalmTrunk"},
    leaf_tex=LeafTex(name="Leaf_FanPalm", venation="parallel", outline="fan",
                     green=hsv(100, 54, 33), young=hsv(88, 68, 48), dead=hsv(34, 50, 44),
                     under=2.5, vein=0.45, vein_count=9, blotch=0.2, damage=0.4, gloss=0.35),
    bark_tex=BarkTex(name="Bark_FanPalm", style="spiny", colour=hsv(32, 26, 26),
                     lichen=0.3, moss=0.3, relief=1.4),
)
def _fan_palm(parts, rng, p, s):
    age = p.age * (0.4 if s.variants[p.variant] == "Young" else 1.0)
    height = s.height_at(age) * p.scale
    segs = p.detail(5, 4, 3)
    axis = arc((0, 0, 0), rng.uniform(0, 6.28), height * 0.06, height * 0.70, 0.0, segs,
               tilt=1.46, wobble=0.006, rng=rng)
    tube(parts("trunk"), axis, [0.025 * (1.1 - 0.2 * i / segs) for i in range(segs + 1)],
         sides=p.detail(6, 5, 4), v_scale=1.5, u_repeat=2.0)
    crown = axis[-1]
    b = parts("leaf")
    fans = int(_lerp(4, 7, age) * p.detail(1.0, 0.65, 0.4))
    for i, a in enumerate(rosette(max(3, fans), start=rng.uniform(0, 6.28))):
        old = i / max(1, fans - 1)
        # The fan is about 1.0-1.4 m across whatever the trunk's height, on a 0.7-1.2 m petiole.
        radius = rng.uniform(0.50, 0.70) * p.scale
        petiole = rng.uniform(0.70, 1.10) * p.scale
        spine = arc(crown, a, petiole, petiole * _lerp(0.8, 0.25, old), petiole * 0.1, 3,
                    tilt=_lerp(0.9, 0.1, old))
        tube(parts("trunk"), spine, [0.012, 0.011, 0.010, 0.009], sides=p.detail(12, 6, 4), v_scale=2.0)
        hub = spine[-1]
        # Palmate: segments radiate from one hub, cleft almost to the petiole, each with its
        # own V-fold, the outer ones sagging. About 9 divisions a side over 170 degrees.
        rays = p.detail(17, 11, 6)
        for k in range(rays):
            across = k / (rays - 1) - 0.5                 # -0.5 .. 0.5 across the fan
            ka = a + across * 2.97                        # about 170 degrees of arc
            edge = abs(across) * 2                        # 0 at the middle, 1 at the outside
            blade(b, hub, ka, radius * _lerp(1.0, 0.78, edge), radius * 0.055,
                  rise=radius * 0.12 * (1 - edge), droop=radius * _lerp(0.3, 0.75, edge),
                  fold=(0.9, 0.5, 0.2), curl=0.0,
                  tilt=_lerp(0.3, -0.25, max(old, edge)), segs=p.detail(12, 3, 2), cols=3,
                  outline="fan")
    litter_skirt(parts("leaf"), rng, max(0.7, height * 0.2), 6, 0.5, p, frond=True)


# --- Mid-storey -----------------------------------------------------------


@species(
    key="banana",
    litter="own",                      # its builder sheds whole 1.5 m sheaths, not leaf flakes
    common="Banana / plantain",
    latin="Musa x paradisiaca",
    band="mid",
    height=(1.2, 6.0),
    spread=(1.8, 4.0),
    reference="A pseudostem of rolled sheaths 0.2-0.4 m thick to 3-6 m, with 8-12 blades "
              "2-3 m long and 0.5-0.9 m wide that tear along the lateral veins into ribbons "
              "within weeks of opening.",
    historical="Musa is Asian: POWO gives its native range as tropical and subtropical Asia to "
               "the western Pacific, with no American country in it. The defensible claim is "
               "'16th century, via the Canaries and the Caribbean'; the usual '1516, Berlanga' "
               "is traditional attribution, not fact. No banana of any kind in a pre-contact "
               "scene - not background, not thatch, not a wrapping leaf. Substitute "
               "`calathea` (Calathea lutea) or a big heliconia; what separates them at a "
               "distance is that the native blades stay entire instead of shredding into the "
               "banana comb, and that there is no fruit hand or drooping purple bell.",
    assumed="Tear placement is procedural.",
    tells="Ribbon-torn giant blades on a smooth green column.",
    variants=("Mature", "Young"),
    parts={"leaf": "BananaLeaf", "trunk": "BananaStem"},
    leaf_tex=LeafTex(name="Leaf_Banana", venation="parallel", outline="strap",
                     green=hsv(106, 68, 36), young=hsv(92, 82, 51), dead=hsv(30, 43, 37),
                     under=1.9, vein=0.5, vein_count=46, blotch=0.25, damage=0.7, gloss=0.45),
    bark_tex=BarkTex(name="Bark_Banana", style="smooth", colour=hsv(86, 34, 40),
                     lichen=0.1, moss=0.15, relief=0.5),
)
def _banana(parts, rng, p, s):
    age = p.age * (0.4 if s.variants[p.variant] == "Young" else 1.0)
    height = s.height_at(age) * p.scale
    stem_h = height * 0.55
    segs = p.detail(5, 4, 3)
    axis = arc((0, 0, 0), 0.0, 0.0, stem_h, 0.0, segs, tilt=1.5708)
    tube(parts("trunk"), axis, [0.12 * (1.0 - 0.35 * i / segs) * height / 4 for i in range(segs + 1)],
         sides=p.detail(8, 6, 4), v_scale=1.0, u_repeat=1.0)
    b = parts("leaf")
    crown = axis[-1]
    leaves = int(_lerp(4, 8, age) * p.detail(1.0, 0.6, 0.4))
    for i, a in enumerate(rosette(max(3, leaves), start=rng.uniform(0, 6.28))):
        old = i / max(1, leaves - 1)
        # 1.0-1.8 m of blade on a mature plant, 2.5-3.5:1 - each half carries half the
        # width, so the full blade comes out at 2.9:1.
        length = height * _lerp(0.40, 0.31, old)
        # Each half is built as its own strip so the midrib reads as a spine, and the
        # halves shred independently: 5-15 splits per leaf, only the newest blade whole,
        # and the strips beyond a split hang free. A split runs 20-60% of the HALF WIDTH,
        # inward from the margin - it is not a separation down to the midrib. Letting
        # any interior column tear took the blade apart into loose ribbons and a mature
        # banana came back looking like a dracaena, so only the outer columns may go.
        for side, u0, u1 in ((-1, 0.0, 0.5), (1, 0.5, 1.0)):
            cols = p.detail(9, 5, 3)
            shred = 0.25 + 0.55 * old + 0.3 * (1 - p.health)
            outer = range(max(1, int(cols * 0.6)), cols - 1)
            tears = () if old < 0.2 else tuple(k for k in outer if rng.random() < shred)
            blade(b, crown, a, length, length * 0.17,
                  rise=length * _lerp(0.55, 0.15, old), droop=length * _lerp(0.3, 0.95, old),
                  fold=(0.9, 0.2, 0.45), curl=(0.0, 0.1 * side, 0.3),
                  twist=side * rng.uniform(0.1, 0.5),
                  tilt=_lerp(0.85, 0.0, old), segs=p.detail(20, 5, 3), cols=cols,
                  outline="strap", ripple=0.22 * (0.4 + old), u0=u0, u1=u1,
                  tears=tears, tear_from=_lerp(0.72, 0.45, old), sag=0.6)
    litter_skirt(parts("leaf"), rng, max(0.8, height * 0.25), 7, height * 0.12, p, frond=True)


@species(
    key="spiny_palm",
    litter="own",                      # its builder sheds pinnae
    common="Chocho / spiny understory palm",
    latin="Astrocaryum mexicanum",
    band="understory",
    height=(0.5, 5.5),
    spread=(1.5, 3.0),
    reference="This replaces the stilt palm, which does not belong: GBIF holds 1,910 preserved "
              "records of Socratea exorrhiza with a northern limit in Nicaragua and ZERO from "
              "Mexico, Guatemala, Belize or Honduras, and Iriartea the same. There is no "
              "cone-of-stilt-roots palm in the Peten, and a walking palm here is a visible "
              "error. Astrocaryum mexicanum is the palm that is actually there (GBIF MX 231, "
              "BZ 54, HN 34, GT 33; the Flora of Guatemala calls it common and often abundant "
              "below 400 m): 1.5-5.5 m on a plain slender trunk only 2.5-4.5 cm thick, armed "
              "with very numerous spreading stout black spines 3-5 cm long, carrying 4-8 "
              "pinnate leaves around a metre long whose few, very long broad segments are "
              "glaucous blue-grey beneath. No stilt roots and no crownshaft.",
    assumed="Leaf count per crown, segment width, and spine density (about 10-25 per 10 cm of "
            "trunk); the spines are carried by the bark texture, not built.",
    tells="A pencil-thin black-spined pole under a huge feather that flashes blue-grey when "
          "it turns.",
    variants=("Mature", "Young"),
    parts={"leaf": "SpinyPalmLeaf", "trunk": "SpinyPalmTrunk"},
    leaf_tex=LeafTex(name="Leaf_SpinyPalm", venation="parallel", outline="wedge",
                     green=hsv(112, 60, 30), young=hsv(98, 74, 45), dead=hsv(32, 46, 40),
                     under=2.3, vein=0.4, vein_count=12, blotch=0.2, damage=0.45, gloss=0.4),
    bark_tex=BarkTex(name="Bark_SpinyPalm", style="spiny", colour=hsv(34, 18, 24),
                     lichen=0.35, moss=0.25, relief=1.5),
)
def _spiny_palm(parts, rng, p, s):
    age = p.age * (0.35 if s.variants[p.variant] == "Young" else 1.0)
    height = s.height_at(age) * p.scale
    t = parts("trunk")
    segs = p.detail(6, 4, 3)
    axis = arc((0, 0, 0), rng.uniform(0, 6.28), height * 0.05, height * 0.66, 0.0, segs,
               tilt=1.47, wobble=0.006, rng=rng)
    tube(t, axis, [0.019 * (1.12 - 0.2 * i / segs) for i in range(segs + 1)],
         sides=p.detail(6, 5, 4), v_scale=2.5, u_repeat=2.0)
    crown = axis[-1]
    # Astrocaryum mexicanum holds 5-12 fronds in its crown. At three the palm read as a
    # pole with a dead frond hanging off it.
    fronds = int(_lerp(5, 12, age) * p.detail(1.0, 0.7, 0.5))
    for i, a in enumerate(rosette(max(3, fronds), start=rng.uniform(0, 6.28))):
        old = i / max(1, fronds - 1)
        length = height * _lerp(0.32, 0.26, old)
        frond(parts("leaf"), crown, a, length,
              rise=length * _lerp(0.5, 0.1, old), droop=length * _lerp(0.25, 0.7, old),
              leaflets=p.detail(7, 5, 4),           # few, very long segments, not a fine comb
              leaflet_len=length * 0.5, leaflet_w=0.09,
              rachis_r=0.014, tilt=_lerp(0.45, -0.2, old), segs=p.detail(16, 4, 3), rng=rng,
              outline="wedge", fold=0.25, droop_leaflet=0.35, bare=0.2)
    litter_skirt(parts("leaf"), rng, max(0.6, height * 0.2), 5, height * 0.14, p, frond=True)


@species(
    key="strangler_fig",
    common="Strangler fig / matapalo",
    latin="Ficus obtusifolia",
    band="mid",
    height=(3.0, 30.0),
    spread=(3.0, 18.0),
    reference="F. obtusifolia, not F. aurea: the Flora of Guatemala knew Guatemalan F. aurea "
              "only from one Sacatepequez collection at about 1500 m, while F. obtusifolia is "
              "described as 'usually a conspicuous strangler, especially when juvenile', "
              "reaches 40 m, and carries entire glossy leaves 0.15-0.25 x 0.06-0.12 m "
              "clustered at the branch ends with 8-11 vein pairs and green globose figs "
              "1.5-3 cm. F. involuta is the Peten-and-Izabal alternative. The aerial roots "
              "reach the ground and form a shell-like trunk around the host; the local name "
              "is matapalo. This is the tree that grows out of Maya ruins, so it earns its "
              "place on an overgrown temple more than any other plant in the library.",
    assumed="All the lattice geometry: strand diameter 20-150 mm at head height tapering to "
            "8-25 mm, spacing 100-400 mm where open and fused to a sheet below, sheathing the "
            "lower 3-8 m. No source gives any of it, nor how long strangling takes - do not "
            "put a duration in player-facing text.",
    tells="The vertical seam-and-weld texture of the root column: parallel woody straps "
          "running straight down, fusing at irregular crossings, daylight through the gaps, "
          "wrapped around a straight dead trunk of completely different bark.",
    variants=("Young", "Enveloping", "Masonry"),
    parts={"root": "FigRoot", "leaf": "FigLeaf"},
    leaf_tex=LeafTex(name="Leaf_Fig", venation="pinnate", outline="ovate",
                     green=hsv(122, 58, 28), young=hsv(106, 72, 44), dead=hsv(38, 44, 40),
                     under=1.6, vein=0.5, vein_count=11, blotch=0.2, damage=0.25, gloss=0.85),
    bark_tex=BarkTex(name="Bark_Fig", style="smooth", colour=hsv(30, 20, 38),
                     lichen=0.4, moss=0.45, relief=0.7),
)
def _strangler_fig(parts, rng, p, s):
    kind = s.variants[p.variant]
    height = s.height_at(p.age) * p.scale
    host_r = height * (0.035 if kind == "Young" else 0.05)
    r = parts("root")
    strands = p.detail(7, 5, 3) if kind != "Young" else p.detail(4, 3, 2)
    segs = p.detail(7, 5, 3)
    for k in range(strands):
        a0 = 2 * math.pi * k / strands + rng.uniform(-0.3, 0.3)
        sway = rng.uniform(0.5, 1.4) * (1 if rng.random() < 0.5 else -1)
        pts, radii = [], []
        for i in range(segs + 1):
            t = i / segs
            if kind == "Masonry":
                # Over a wall or a stair, not around a trunk: the straps splay down a
                # flat face and grip its edge. The temple is a ruin, so this is the
                # variant that reclaims it.
                across = (k / max(1, strands - 1) - 0.5) * host_r * 3.0
                pts.append((across * (1 + 0.5 * t) + 0.1 * sway * t * t,
                            host_r * (0.9 + 0.25 * math.sin(5 * t)), height * (1 - t * 0.02) * t))
            else:
                a = a0 + sway * t * t
                rr = host_r * (1.0 + 0.25 * math.sin(6 * t))
                pts.append((math.cos(a) * rr, math.sin(a) * rr, height * t))
            radii.append(host_r * _lerp(0.22, 0.07, t))
        tube(r, pts, radii, sides=p.detail(5, 4, 3), v_scale=1.0, cap_end=True,
             squash=(1.0, 0.55) if kind == "Masonry" else (1.0, 1.0))
    if not p.lod:
        for k in range(p.detail(3, 2, 1)):      # cross links where roots have fused
            t = rng.uniform(0.25, 0.8)
            a = rng.uniform(0, 6.28)
            z = height * t
            link = [(math.cos(a) * host_r, math.sin(a) * host_r, z),
                    (math.cos(a + 0.9) * host_r * 1.05, math.sin(a + 0.9) * host_r * 1.05,
                     z + height * 0.04)]
            tube(r, link, [host_r * 0.1, host_r * 0.08], sides=p.detail(12, 6, 4), v_scale=2.0, cap_end=True)
    b = parts("leaf")
    tufts = p.detail(6, 4, 2)
    for k in range(tufts):
        a = rng.uniform(0, 6.28)
        z = height * rng.uniform(0.72, 1.0)
        base = (math.cos(a) * host_r * 1.1, math.sin(a) * host_r * 1.1, z)
        for la in rosette(p.detail(5, 3, 2), start=a):
            length = height * 0.035
            blade(b, base, la, length, length * 0.42, rise=length * 0.2,
                  droop=length * 0.45, fold=0.2, curl=0.15, segs=3, cols=3, outline="ovate")


# --- Canopy ---------------------------------------------------------------


@species(
    key="ceiba",
    common="Ceiba / yaxché",
    latin="Ceiba pentandra",
    band="canopy",
    height=(8.0, 45.0),
    spread=(6.0, 40.0),
    reference="10-30 m is common, 30 modal, 60-70 exceptional, so 35-45 m is the honest "
              "emergent and 50 m a hero tree; bole 1.0-3.0 m, use 1.2-2.0 m dbh above the "
              "flare. Branches are 'spreading, whorled and almost horizontal' under a thin "
              "pagoda crown 25-40 m across; leaves are palmately compound with 5-9 leaflets "
              "(usually 5 on mature trees) 0.08-0.16 x 0.02-0.04 m; the trunk carries short "
              "sharp prickles. It is the Maya world tree, the axis mundi joining Underworld, "
              "earth and sky, so it is the one canopy tree a temple scene must not get wrong.",
    assumed="The buttresses, which are also the thing the player sees: the circulating "
            "12-15 m high / 20 m spread figures trace to popular books, not measurement, and "
            "the one measured datum is a 49.7 m BCI tree whose whole buttressed base was 15 m "
            "across (about 6-7 m radial). Built here as 5-9 planks about 0.15-0.20 of the "
            "tree's height, spreading 3-6 m, 0.3-0.6 m thick at the ground, with surface "
            "roots running on across the litter, which is the regionally correct read on "
            "Peten karst where roots spread horizontally rather than plunging. Spine size "
            "and shedding age, and the leafless months, are also unsourced.",
    tells="The buttresses. Tall sinuous grey walls taller than a person, radiating across the "
          "litter, with a spine-studded grey bole rising out of them. At 5-15 m the crown is "
          "out of frame: the tree IS its buttresses.",
    variants=("Emergent", "Young"),
    parts={"trunk": "CeibaBark", "canopy": "CeibaCanopy"},
    leaf_tex=LeafTex(name="Leaf_Ceiba", venation="palmate", outline="pinnule",
                     green=hsv(94, 56, 32), young=hsv(82, 70, 47), dead=hsv(40, 48, 44),
                     under=1.6, vein=0.35, vein_count=7, blotch=0.25, damage=0.3, gloss=0.45),
    bark_tex=BarkTex(name="Bark_Ceiba", style="lenticel", colour=hsv(70, 12, 44),
                     lichen=0.5, moss=0.3, relief=0.9),
)
def _ceiba(parts, rng, p, s):
    age = p.age * (0.35 if s.variants[p.variant] == "Young" else 1.0)
    height = s.height_at(age) * p.scale
    t = parts("trunk")
    r0 = height * 0.022
    segs = p.detail(7, 5, 3)
    axis = [(0, 0, height * (i / segs) ** 1.05) for i in range(segs + 1)]
    tube(t, axis, [r0 * _lerp(1.45, 0.45, (i / segs) ** 0.7) for i in range(segs + 1)],
         sides=p.detail(9, 6, 5), v_scale=0.4, u_repeat=3.0)
    buttresses = p.detail(7, 5, 3)
    for k in range(buttresses):
        a = 2 * math.pi * k / max(1, buttresses) + rng.uniform(-0.15, 0.15)
        reach = r0 * rng.uniform(4.0, 6.5)
        fin(t, (0, 0, 0), reach, a, height * rng.uniform(0.15, 0.21),
            r0 * 0.25, segs=p.detail(14, 3, 2), curve=1.5, v_scale=2.0)
        if not p.lod:
            # The plank does not stop: on karst the root runs on over the litter.
            run = reach * rng.uniform(1.4, 2.4)
            pts = [(math.cos(a) * reach, math.sin(a) * reach, 0.05 * r0),
                   (math.cos(a) * run * 0.7, math.sin(a) * run * 0.7, 0.03 * r0),
                   (math.cos(a + 0.2) * run, math.sin(a + 0.2) * run, 0.0)]
            tube(t, pts, [r0 * 0.22, r0 * 0.13, r0 * 0.07], sides=p.detail(12, 6, 4), v_scale=1.0,
                 cap_end=True, squash=(1.0, 0.6))
    c = parts("canopy")
    # A ceiba crown is horizontal and layered: a few tiers of long, nearly level limbs.
    # Each tier used to be ONE limb carrying three leaves sized off the tree's height -
    # 5 m leaves on a 45 m tree - which rendered as a bent lamp-post with three paddles.
    # A ceiba leaflet is 5-20 cm, palmately in fives to nines, so the crown is made of
    # thousands of small things and has to be built as limbs, twigs and sprays.
    tiers = p.detail(5, 3, 2)
    for k in range(tiers):
        up = _lerp(0.74, 1.0, k / max(1, tiers - 1))
        for limb in range(p.detail(7, 3, 2)):
            a = rng.uniform(0, 6.28)
            reach = height * _lerp(0.22, 0.38, up) * rng.uniform(0.7, 1.0)
            branch = arc((0, 0, height * up), a, reach, height * 0.04, 0.0, 3,
                         tilt=rng.uniform(0.1, 0.35))
            tube(t, branch, [r0 * 0.30, r0 * 0.22, r0 * 0.15, r0 * 0.10], sides=p.detail(12, 6, 4), v_scale=1.0)
            for twig in range(p.detail(5, 2, 1)):
                ta = a + rng.uniform(-1.0, 1.0)
                sub = arc(branch[1 + twig % 3], ta, reach * rng.uniform(0.25, 0.6),
                          height * 0.012, height * 0.02, 3, tilt=rng.uniform(0.0, 0.5))
                tube(t, sub, [r0 * 0.11, r0 * 0.08, r0 * 0.06, r0 * 0.04], sides=p.detail(10, 5, 3), v_scale=1.6)
                # Leaves go along the whole twig and the limb behind it, not just at the
                # tip. Hung at the ends only, 2,200 leaves sat in a few dozen pom-poms
                # and the crown volume between them was empty: a bare tree with green
                # tips, through three rounds of raising the count.
                for node in sub + branch[1:]:
                    # One mesh element is one whole palmate LEAF, 0.3-0.5 m across, and
                    # the five-to-nine leaflets it divides into are carried by the leaf
                    # atlas - the same trade the tree fern makes for its pinnules. Built
                    # leaflet by leaflet instead, each covering 0.014 m2, filling a 27 m
                    # crown three layers deep needs about 120,000 of them and the better
                    # part of a million triangles; at any count that fits, the tree
                    # renders bare. At whole-leaf size the same triangles cover it.
                    for leaf in range(p.detail(5, 3, 2)):
                        size = rng.uniform(0.30, 0.52)
                        la = ta + leaf * 2.39996 + rng.uniform(-0.3, 0.3)
                        # Scattered off the node, so a whorl is a tuft of foliage with
                        # depth rather than a flat rosette on one point. Offset from the
                        # node itself each time, not from the last leaf, or the whorl
                        # walks away down the branch.
                        at = (node[0] + rng.uniform(-1, 1) * size * 1.3,
                              node[1] + rng.uniform(-1, 1) * size * 1.3,
                              node[2] + rng.uniform(-1, 1) * size * 0.9)
                        blade(c, at, la, size, size * 0.42,
                              rise=size * 0.06, droop=size * 0.34, fold=0.14, curl=0.22,
                              twist=rng.uniform(-0.6, 0.6), segs=3, cols=3,
                              outline="ovate", lobes=3, ripple=0.2,
                              tilt=rng.uniform(-0.5, 0.2))


@species(
    key="corozo_palm",
    litter="own", dying="own",         # its builder hangs whole dead fronds and sheds them
    common="Corozo / cohune palm",
    latin="Attalea cohune",
    band="canopy",
    height=(1.5, 15.0),
    spread=(5.0, 13.0),
    reference="9-15 m attested (the 20-30 m figures are secondary), with 20-30 fronds that the "
              "Flora of Guatemala describes as sometimes 10 m long and 2 m wide, 'plume-like and "
              "graceful despite their size'. The trunk is clean below and collared with "
              "persistent leaf bases above. Young plants are acaulescent - a giant stemless "
              "shuttlecock standing on the floor, and very common. It is 'abundant in the "
              "Atlantic lowlands... chiefly at 300 m or lower', which is the Belize cohune "
              "ridge; the Maya name is tutz, and the pendent fruiting panicle carries 800-1000 "
              "fruits like small coconuts and can pass 45 kg.",
    assumed="Frond count is cut to a third of the real 20-30 for the triangle budget and the "
            "texture carries the leaflet density the geometry cannot; trunk diameter and "
            "leaflet width come from secondary sources only.",
    tells="A shuttlecock: a funnel of near-vertical plumose fronds, wider than it is tall, on "
          "a trunk collared with old leaf bases.",
    variants=("Mature", "Stemless"),
    parts={"leaf": "CorozoLeaf", "trunk": "CorozoTrunk"},
    leaf_tex=LeafTex(name="Leaf_Corozo", venation="parallel", outline="pinnule",
                     green=hsv(110, 62, 31), young=hsv(96, 76, 46), dead=hsv(36, 44, 46),
                     under=1.7, vein=0.4, vein_count=15, blotch=0.2, damage=0.4, gloss=0.5),
    bark_tex=BarkTex(name="Bark_Corozo", style="ringed", colour=hsv(30, 26, 30),
                     lichen=0.4, moss=0.35, relief=1.2),
)
def _corozo(parts, rng, p, s):
    stemless = s.variants[p.variant] == "Stemless"
    height = s.height_at(p.age) * p.scale
    trunk_h = 0.0 if stemless else height * 0.55
    crown = (0, 0, trunk_h)
    if trunk_h:
        segs = p.detail(5, 4, 3)
        axis = [(0, 0, trunk_h * i / segs) for i in range(segs + 1)]
        tube(parts("trunk"), axis, [0.22 * (1.1 - 0.15 * i / segs) for i in range(segs + 1)],
             sides=p.detail(8, 6, 4), v_scale=1.0, u_repeat=2.0)
    fronds = int(_lerp(7, 12, p.age) * p.detail(1.0, 0.6, 0.35))
    for i, a in enumerate(rosette(max(4, fronds), start=rng.uniform(0, 6.28))):
        old = i / max(1, fronds - 1)
        length = height * _lerp(0.55, 0.42, old)
        frond(parts("leaf"), crown, a, length,
              rise=length * _lerp(0.85, 0.35, old), droop=length * _lerp(0.3, 0.95, old),
              leaflets=p.detail(12, 8, 5), leaflet_len=length * 0.16, leaflet_w=0.2,
              rachis_r=0.035, tilt=_lerp(1.0, 0.15, old), segs=p.detail(18, 5, 3), rng=rng,
              outline="pinnule", fold=0.4, droop_leaflet=0.45, bare=0.12)
    # A cohune stands in a raft of its own shed fronds, and the 1-3 m frond segments
    # weather pale straw to silver-grey: the largest and palest shapes on the floor.
    litter_skirt(parts("leaf"), rng, max(1.4, height * 0.3), 8, height * 0.16, p, frond=True)
    if not p.lod:
        dying_leaves(parts("leaf"), rng, crown, 2, height * 0.3, height * 0.012, p,
                     outline="strap")


@species(
    key="canopy_tree",
    common="Ramón / breadnut",
    latin="Brosimum alicastrum",
    band="canopy",
    height=(6.0, 35.0),
    spread=(4.0, 20.0),
    reference="Brosimum alicastrum is the dominant canopy tree of Petén forest, 20-35 m with a "
              "straight unbuttressed-to-lightly-buttressed bole and a dense, rounded crown of "
              "glossy entire leaves; it grows thickly on Maya ruins, where the Maya may have "
              "encouraged it. It is the right default wall of jungle behind a temple.",
    assumed="Crown is built as overlapping leaf shells; at 20-40 m only the silhouette carries.",
    tells="A dense, dark, rounded crown over a clean grey bole.",
    variants=("Mature", "Sapling"),
    parts={"trunk": "CanopyBark", "canopy": "CanopyFoliage"},
    leaf_tex=LeafTex(name="Leaf_Canopy", venation="pinnate", outline="ovate",
                     green=hsv(130, 60, 27), young=hsv(114, 74, 42), dead=hsv(34, 46, 42),
                     under=1.5, vein=0.4, vein_count=13, blotch=0.3, damage=0.3, gloss=0.6),
    bark_tex=BarkTex(name="Bark_Canopy", style="fissured", colour=hsv(28, 18, 34),
                     lichen=0.45, moss=0.4, relief=1.1),
)
def _canopy_tree(parts, rng, p, s):
    age = p.age * (0.25 if s.variants[p.variant] == "Sapling" else 1.0)
    height = s.height_at(age) * p.scale
    t, c = parts("trunk"), parts("canopy")
    r0 = height * 0.016
    segs = p.detail(6, 4, 3)
    axis = arc((0, 0, 0), rng.uniform(0, 6.28), height * 0.04, height * 0.72, 0.0, segs,
               tilt=1.47, wobble=0.004, rng=rng)
    tube(t, axis, [r0 * _lerp(1.4, 0.5, i / segs) for i in range(segs + 1)],
         sides=p.detail(8, 6, 4), v_scale=0.6, u_repeat=3.0)
    top = axis[-1]
    # A ramon leaf is 5-15 cm. Twelve leaves scaled to a fraction of the TREE's height
    # made twelve three-metre paddles on four branches - a parasol, not a crown - which
    # is what the first frame of this species showed. So: leaves at their own real size,
    # on a branch system big enough to carry a crown, and enough of them that the
    # silhouette is leaves rather than sticks.
    primaries = p.detail(8, 4, 3)
    for k in range(primaries):
        a = 2 * math.pi * k / primaries + rng.uniform(-0.5, 0.5)
        reach = height * rng.uniform(0.18, 0.30)
        branch = arc(top, a, reach, height * rng.uniform(0.06, 0.14), 0.0, 3,
                     tilt=rng.uniform(0.5, 0.9))
        tube(t, branch, [r0 * 0.45, r0 * 0.34, r0 * 0.22, r0 * 0.13], sides=p.detail(12, 6, 4), v_scale=1.2)
        for sec in range(p.detail(4, 2, 1)):
            sa = a + rng.uniform(-0.9, 0.9)
            start = branch[1 + sec % 2]
            sub = arc(start, sa, reach * rng.uniform(0.4, 0.7), height * 0.04, height * 0.02,
                      3, tilt=rng.uniform(0.2, 0.8))
            tube(t, sub, [r0 * 0.18, r0 * 0.13, r0 * 0.09, r0 * 0.06], sides=p.detail(10, 5, 3), v_scale=1.6)
            # Leaves sit in sprays along the twig, which is how a crown fills: a spray
            # of 15 cm leaves reads as foliage where one 3 m leaf reads as a sail.
            # As on the ceiba: one element is one leaf at its own size, not one leaflet,
            # because a leaflet-sized element cannot cover a crown at any affordable
            # count. A ramon leaf is 5-15 cm, so a spray of them is the unit here.
            for node in sub[1:] + branch[2:]:
                for _ in range(p.detail(7, 3, 2)):
                    size = rng.uniform(0.22, 0.42)
                    la = sa + rng.uniform(-2.2, 2.2)
                    blade(c, (node[0] + rng.uniform(-1, 1) * size * 1.6,
                              node[1] + rng.uniform(-1, 1) * size * 1.6,
                              node[2] + rng.uniform(-1, 1) * size * 1.2), la, size, size * 0.42,
                          rise=size * 0.1, droop=size * 0.45, fold=0.2, curl=0.15,
                          twist=rng.uniform(-0.8, 0.8), segs=2, cols=3,
                          outline="ovate", tilt=rng.uniform(-0.6, 0.3))


# --- Epiphytes ------------------------------------------------------------


@species(
    key="bromeliad_epiphyte",
    litter="none",                     # it sits on a branch; its litter falls to someone else's
    common="Tank bromeliad",
    latin="Aechmea bracteata",
    band="epiphyte",
    height=(0.2, 1.2),
    spread=(0.4, 1.4),
    reference="Tank bromeliads sit in branch forks and trunk crotches through the Petén, their "
              "rosettes 0.4-1.2 m across holding water between overlapping, often red-flushed "
              "strap leaves; they are the commonest large epiphyte on old trees and ruins.",
    assumed="The water tank is implied by the leaf bases, not modelled.",
    tells="A stiff green urn clamped onto a branch where nothing should grow.",
    variants=("Single", "Cluster"),
    parts={"leaf": "EpiBromeliadLeaf"},
    leaf_tex=LeafTex(name="Leaf_BromeliadEpi", venation="parallel", outline="strap",
                     green=hsv(92, 44, 32), young=hsv(78, 56, 45), dead=hsv(26, 52, 40),
                     under=1.5, vein=0.3, vein_count=15, blotch=0.4, damage=0.35, gloss=0.55),
)
def _bromeliad_epiphyte(parts, rng, p, s):
    b = parts("leaf")
    size = s.spread_at(p.age) * p.scale
    heads = 3 if s.variants[p.variant] == "Cluster" and not p.lod else 1
    for head in range(heads):
        ha = rng.uniform(0, 6.28)
        base = (math.cos(ha) * size * 0.22 * head, math.sin(ha) * size * 0.22 * head, 0.0)
        count = p.detail(9, 6, 4)
        for i, a in enumerate(rosette(count, start=ha)):
            old = i / max(1, count - 1)
            length = size * _lerp(0.55, 0.38, old)
            blade(b, base, a, length, length * 0.14,
                  rise=length * _lerp(0.85, 0.5, old), droop=length * _lerp(0.3, 0.9, old),
                  fold=0.6, curl=-0.05, tilt=_lerp(1.2, 0.5, old),
                  segs=p.detail(14, 4, 3), cols=3, outline="strap")


@species(
    key="tillandsia",
    litter="none",                     # on a branch
    common="Grey air plant",
    latin="Tillandsia spp.",
    band="epiphyte",
    height=(0.1, 0.5),
    spread=(0.15, 0.6),
    reference="Atmospheric Tillandsia clump on thin branches and on exposed stone, their narrow "
              "channelled leaves covered in silver trichomes that make them read grey-green and "
              "matt even in rain; clumps run 10-50 cm.",
    assumed="Clump count.",
    tells="A silver-grey spiky ball on a bare branch, much paler than every leaf around it.",
    variants=("Clump",),
    parts={"leaf": "TillandsiaLeaf"},
    leaf_tex=LeafTex(name="Leaf_Tillandsia", venation="parallel", outline="strap",
                     green=hsv(84, 14, 54), young=hsv(70, 18, 62), dead=hsv(34, 30, 44),
                     under=1.2, vein=0.2, vein_count=8, blotch=0.3, damage=0.25, gloss=0.12),
)
def _tillandsia(parts, rng, p, s):
    b = parts("leaf")
    size = s.spread_at(p.age) * p.scale
    for i, a in enumerate(rosette(p.detail(11, 7, 5), start=rng.uniform(0, 6.28))):
        length = size * rng.uniform(0.5, 0.85)
        blade(b, (0, 0, 0), a, length, length * 0.08,
              rise=length * rng.uniform(0.5, 0.9), droop=length * rng.uniform(0.2, 0.6),
              fold=0.7, curl=-0.1, tilt=rng.uniform(0.5, 1.2), twist=rng.uniform(-0.8, 0.8),
              segs=p.detail(12, 3, 2), cols=3, outline="strap")


@species(
    key="nest_fern",
    litter="none",                     # on a branch; it catches litter in its basket instead
    common="Bird's-nest fern",
    latin="Asplenium serratum",
    band="epiphyte",
    height=(0.3, 1.2),
    spread=(0.6, 1.6),
    reference="Asplenium serratum, the neotropical bird's-nest fern, holds a funnel of simple "
              "undivided strap fronds 0.4-1.2 m long with wavy, finely toothed margins, on trunks "
              "and in branch crotches where litter collects in the funnel.",
    assumed="Frond count.",
    tells="A smooth green funnel of undivided fronds: a fern with no divisions at all.",
    variants=("Mature", "Young"),
    parts={"leaf": "NestFernLeaf"},
    leaf_tex=LeafTex(name="Leaf_NestFern", venation="pinnate", outline="strap",
                     green=hsv(116, 62, 32), young=hsv(102, 76, 47), dead=hsv(38, 48, 44),
                     under=1.8, vein=0.55, vein_count=30, blotch=0.25, damage=0.3, gloss=0.65),
)
def _nest_fern(parts, rng, p, s):
    b = parts("leaf")
    age = p.age * (0.45 if s.variants[p.variant] == "Young" else 1.0)
    size = s.spread_at(age) * p.scale
    count = p.detail(9, 6, 4)
    for i, a in enumerate(rosette(count, start=rng.uniform(0, 6.28))):
        old = i / max(1, count - 1)
        length = size * _lerp(0.6, 0.42, old)
        blade(b, (0, 0, 0), a, length, length * 0.13,
              rise=length * _lerp(0.8, 0.45, old), droop=length * _lerp(0.25, 0.85, old),
              fold=0.45, curl=0.05, tilt=_lerp(1.15, 0.45, old), ripple=0.18,
              segs=p.detail(16, 4, 3), cols=3, outline="strap")


# --- Vines ----------------------------------------------------------------


@species(
    key="liana",
    litter="none",                     # it hangs from the canopy, with no base of its own here
    common="Monkey-ladder liana / escalera de mono",
    latin="Bauhinia herrerae (= Schnella herrerae)",
    band="vine",
    height=(3.0, 18.0),
    spread=(0.6, 2.5),
    reference="The Bauhinia-group liana of this exact setting: Peten, Alta Verapaz and Izabal "
              "below 300 m, the most-collected candidate (GBIF MX 700, BZ 83, GT 46), to 18 m "
              "long, with bilobate blades 3-8 cm long and about as wide, cordate, 9-nerved, "
              "glabrate above and brownish-pilose beneath so the foliage flips fawn-brown in "
              "wind, and numerous tendrils. Related Schnella stems are 'flattened and folded, "
              "with wings on both sides' when old and round when young, and B. hondurensis "
              "stems are 'strongly zigzag, perforated'. In Yucatan the Maya have long used the "
              "vine to lash roof frames.",
    assumed="The ribbon's millimetres are nowhere in print: 60-200 mm wide against 15-40 mm "
            "thick (4:1 to 6:1), reversing every 0.6-1.5 m with 60-150 mm of amplitude, free "
            "span 3-12 m between crowns. All estimated.",
    tells="A leafless grey flat ribbon of wood the width of a hand, hanging clear of "
          "everything and twisting through a slow S every metre: a strap, never a cylinder.",
    variants=("Hanging", "Ladder", "Curtain"),
    parts={"stem": "LianaStem", "leaf": "LianaLeaf"},
    leaf_tex=LeafTex(name="Leaf_Liana", venation="palmate", outline="ovate",
                     green=hsv(124, 56, 29), young=hsv(108, 70, 44), dead=hsv(32, 42, 40),
                     # Brownish-pilose beneath: the foliage flips fawn-brown in wind.
                     under=2.2, vein=0.5, vein_count=5, blotch=0.3, damage=0.35, gloss=0.6),
    bark_tex=BarkTex(name="Bark_Liana", style="fibrous", colour=hsv(26, 16, 34),
                     lichen=0.35, moss=0.4, relief=1.0),
)
def _liana(parts, rng, p, s):
    kind = s.variants[p.variant]
    length = s.height_at(p.age) * p.scale
    st, b = parts("stem"), parts("leaf")
    ropes = {"Hanging": 1, "Ladder": 1, "Curtain": 3}[kind]
    if p.lod:
        ropes = 1
    for rope in range(ropes):
        a0 = rng.uniform(0, 6.28)
        off = (math.cos(a0) * 0.3 * rope, math.sin(a0) * 0.3 * rope)
        segs = p.detail(11, 7, 4)
        pts, radii = [], []
        for i in range(segs + 1):
            t = i / segs
            z = length * (1 - t)
            # The S reverses on a 0.6-1.5 m wavelength with 60-150 mm of amplitude.
            wave = length * t / rng.uniform(0.8, 1.4)
            if kind == "Ladder":
                sway = 0.12 * math.sin(wave * 4.2)
            else:
                sway = 0.10 * math.sin(wave * 2.6 + rope) + 0.05 * t
            pts.append((off[0] + sway, off[1] + 0.07 * math.cos(wave * 3.1 + rope), z))
            radii.append(_lerp(0.050, 0.030, t))
        # Flat: 100 mm of ribbon against 30 mm of thickness. A round liana is the
        # commonest way to make this plant read as rope instead of wood.
        tube(st, pts, radii, sides=p.detail(6, 6, 4), v_scale=1.5, cap_end=True,
             squash=(1.0, 0.30))
        if kind != "Curtain":
            for i in range(2, segs, 2):
                leaf = rng.uniform(0.03, 0.08)        # 3-8 cm, and about as wide as long
                for side in (-1, 1):
                    # Bilobate: the two lobes are built as two blades off one point, which
                    # is what the butterfly leaf of a Bauhinia reads as at any distance.
                    for lobe in (-0.33, 0.33):
                        blade(b, pts[i], a0 + side * 1.3 + i + lobe, leaf, leaf * 0.42,
                              rise=-leaf * 0.1, droop=leaf * 0.4, fold=0.25, curl=0.1,
                              segs=2, cols=3, outline="ovate")


@species(
    key="root_curtain",
    litter="none", dying="none",       # roots, no leaves
    common="Aerial root curtain",
    latin="Ficus / Philodendron spp.",
    band="vine",
    height=(1.0, 12.0),
    spread=(0.5, 3.0),
    reference="Aerial roots hang in curtains from fig crowns and from aroids on high branches, "
              "unbranched, 5-25 mm thick, pale brown and hairless, thickening where they reach "
              "the ground and root.",
    assumed="Strand count and spacing.",
    tells="Dead-straight pale strands, nothing growing on them, hanging free of the trunk.",
    variants=("Sparse", "Dense"),
    parts={"root": "AerialRoot"},
    # Exposed roots are darker than the soil they come out of, and nearly neutral.
    bark_tex=BarkTex(name="Bark_AerialRoot", style="smooth", colour=hsv(10, 8, 40),
                     lichen=0.15, moss=0.2, relief=0.6),
)
def _root_curtain(parts, rng, p, s):
    r = parts("root")
    length = s.height_at(p.age) * p.scale
    width = s.spread_at(p.age) / 2 * p.scale
    strands = {"Sparse": 5, "Dense": 11}[s.variants[p.variant]]
    strands = max(2, int(strands * p.detail(1.0, 0.6, 0.35)))
    for k in range(strands):
        a = rng.uniform(0, 6.28)
        rr = width * math.sqrt(rng.random())
        x0, y0 = math.cos(a) * rr, math.sin(a) * rr
        h = length * rng.uniform(0.55, 1.0)
        segs = p.detail(5, 4, 3)
        pts = [(x0 + rng.uniform(-0.03, 0.03) * i, y0 + rng.uniform(-0.03, 0.03) * i,
                h * (1 - i / segs)) for i in range(segs + 1)]
        thick = rng.uniform(0.006, 0.018)
        tube(r, pts, [thick * (0.8 + 0.5 * i / segs) for i in range(segs + 1)],
             sides=p.detail(10, 5, 3), v_scale=2.0, cap_end=True)


# ==========================================================================
# Building a specimen


class Parts:
    """The bmeshes one specimen fills, one per material, created on demand."""

    def __init__(self, tile, grid, rng=None):
        self.tile = tile
        self.grid = grid
        self.rng = rng
        self.builds = {}

    def __call__(self, name):
        if name not in self.builds:
            self.builds[name] = Build(new_bmesh(), self.tile, self.grid, self.rng)
        return self.builds[name]

    def meshes(self):
        return {name: b.bm for name, b in self.builds.items() if b.bm.faces}


def build(key, rng=None, params=None):
    """Build one specimen; returns {part name: bmesh}, metres, Z-up, origin at the base.

    The caller owns the bmeshes and must free or merge them. Part names index
    `Species.parts`, which names the material each one wants.
    """
    s = get(key)
    p = params or Params()
    rng = rng or random.Random(0)
    grid = s.leaf_tex.grid if s.leaf_tex else (1, 1)
    count = grid[0] * grid[1]
    # The last tile is the dead one; a live specimen draws from the rest, and an
    # unhealthy one draws the old, blotched tile rather than the young flush.
    live = max(1, count - 1)
    if p.tile >= 0:
        tile_index = min(p.tile, count - 1)
    elif p.health < 0.6 and live >= 3:
        tile_index = 2 if rng.random() < (1.0 - p.health) else rng.randrange(live)
    else:
        tile_index = rng.randrange(live)
    parts = Parts((tile_index % grid[0], tile_index // grid[0]), grid, rng)
    s.build(parts, rng, p, s)
    _shed(parts, rng, p, s)
    meshes = parts.meshes()
    for bm in meshes.values():
        lean_all(bm, p.lean, p.lean_dir)
        weld_collapsed(bm)
    return meshes


def weld_collapsed(bm, dist=1e-5):
    """Collapse zero-length edges, which a blade's own outline produces by the hundred.

    Every `_profile` is zero at the base and the tip - that is what makes a leaf a
    leaf and not a rectangle - so the first and last row of a blade's vertex grid sit
    on one point, and the quads of those two rows triangulate into one real triangle
    and one with no area at all. Left in, they were 17% of the whole library's
    triangles: budget spent on nothing, and zero-length normals feeding into the
    smooth-shading pass at exactly the base and tip of every leaf. The same thing
    happens at a tube whose end radius is zero and at the centre of a `disc` fan.

    Welding the coincident vertices (not `remove_doubles`, which would also weld the
    UV seam that runs up the midrib) turns each collapsed quad into the single
    triangle it always was.
    """
    bmesh = _bm_module()
    bmesh.ops.dissolve_degenerate(bm, dist=dist, edges=list(bm.edges))
    dead = [f for f in bm.faces if f.calc_area() < dist * dist]
    if dead:
        bmesh.ops.delete(bm, geom=dead, context="FACES")
    bm.verts.index_update()


def leaf_part(s):
    """The part that reads off the species' leaf atlas, so the dead tile is available."""
    for name in ("leaf", "canopy", "litter", "moss", "lichen"):
        if name in s.parts:
            return name
    return next(iter(s.parts), None)


def _shed(parts, rng, p, s):
    """Add the litter skirt and the hanging dead leaves to any plant missing them.

    The reference is blunt about both - "every plant sits in its own litter skirt",
    "dead leaves hanging vertically on nearly every stem", "green never meets bare
    soil" - and the first rendered frames showed the library honouring neither on
    twenty of its twenty-six species: the helpers existed and six builders called
    them. A plant standing on clean stone with a full set of live leaves is the
    loudest tell in the frame, so this is done once, for everything, here.
    """
    name = leaf_part(s)
    if name is None or not s.leaf_tex:
        return
    b = parts(name)
    if not b.bm.faces:
        return
    # Measured off the geometry that was just built, not off the species' range. The
    # range belongs to the species and a variant can be anywhere in it: monstera runs to
    # 12 m because one of its three variants climbs, so the half-metre ground mat had its
    # dying leaves hung 7.4 m in the air - a leaf alone in the sky, and a bounding box
    # that framed the page from ten metres away. The litter area is measured first too,
    # because the skirt goes into the same bmesh and would otherwise swamp the dead-leaf
    # budget below.
    live_area = sum(f.calc_area() for f in b.bm.faces)
    # Where this plant's leaves actually are. A dead leaf hangs off the crown that
    # grew it, and the only honest way to know where that is, for twenty-six builders
    # with nothing in common, is to look at the leaves this one just built. Sampled
    # rather than kept whole: a ceiba crown is thousands of faces and one in a hundred
    # describes the volume just as well.
    live_faces = b.bm.faces[:]
    stride = max(1, len(live_faces) // 200)
    live_spots = [tuple(live_faces[i].calc_center_median())
                  for i in range(0, len(live_faces), stride)]
    xs = [v.co[0] for v in b.bm.verts]
    ys = [v.co[1] for v in b.bm.verts]
    zs = [v.co[2] for v in b.bm.verts]
    height = max(0.05, max(zs))
    spread = max(0.1, max(max(xs) - min(xs), max(ys) - min(ys)))
    if s.litter == "auto":
        # 0.6-1.0 m of skirt around a crown-sized plant, dense enough that the ground
        # does not read through it - but sized against the plant that shed it, not just
        # against its spread. A shade-grass tuft has a 1.5 m spread and two blades, and
        # off spread alone it got 124 pieces of litter: a pile of straw with a plant
        # somewhere in it. The reference does say there is often more dead material by
        # area than green at ground level, so the target is a little under one to one.
        radius = _clamp(spread * 0.6, 0.12, 1.1)
        leaf_len = _clamp(spread * 0.35, 0.06, 0.55)
        piece = max(1e-4, 0.35 * leaf_len * leaf_len)
        count = int(_clamp(live_area * 0.8 / (4 * piece), 2, 44))
        litter_skirt(b, rng, radius, count, leaf_len, p)
    if s.dying == "auto":
        # The reference's figure is an *area* one: 10-20% of a crown's leaf area is dead
        # or dying. Sizing the dead leaves off the plant's spread instead gave a
        # calathea - one big paddle on a 2.6 m spread - a brown sail as large as its
        # living blade. So measure the live leaf area first and add dead leaves until
        # they are about 15% of it, which self-corrects for a plant with one big leaf
        # and for one with forty small ones.
        # Hung among the live leaves, not on the trunk axis at a fraction of the
        # plant's height. On a rosette the two are the same place and this was never
        # visibly wrong; on anything with a trunk they are nowhere near each other,
        # and a ceiba got a dead leaf at eleven metres on bare wood, three metres
        # under a crown it was supposed to be hanging from - a pale leaf alone in the
        # sky, in every tree frame in the library.
        # Narrow, and no longer than the plant is wide. One wide slab of a dead leaf
        # reads as a sail hanging beside the crown - which is what a palm looked like -
        # where several narrow ones read as a plant shedding. Narrower also means each
        # costs less area, so the 15% budget admits more of them.
        length = _clamp(min(height * 0.30, spread * 0.55), 0.05, 1.6)
        after_litter = sum(f.calc_area() for f in b.bm.faces)
        for _ in range(14):
            dead_so_far = sum(f.calc_area() for f in b.bm.faces) - after_litter
            if dead_so_far >= live_area * 0.15:
                break
            spot = live_spots[rng.randrange(len(live_spots))] if live_spots else \
                (0.0, 0.0, height * 0.5)
            dying_leaves(b, rng, spot, 1, length * rng.uniform(0.7, 1.0),
                         length * 0.12, p)


def merge_into(dst, src, free=True):
    """Copy one bmesh into another, UVs included. `dst` may be empty."""
    uv_src = src.loops.layers.uv.active or src.loops.layers.uv.verify()
    uv_dst = dst.loops.layers.uv.verify()
    moved = {v: dst.verts.new(v.co) for v in src.verts}
    for f in src.faces:
        nf = dst.faces.new([moved[v] for v in f.verts])
        for old, new in zip(f.loops, nf.loops):
            new[uv_dst].uv = old[uv_src].uv
    dst.verts.index_update()
    if free:
        src.free()


# ==========================================================================
# Materials
#
# Leaves and bark are textured ORM materials; a petiole or a bract is a stock
# StandardMaterial3D with a colour, because a 2 cm stalk cannot show a texture and
# a material without one is a material the mobile tier never pays for.

STEM_COLOUR = {
    # Albedo, so held in the same band as the leaf colours: a stem in deep shade is
    # dark because of the light, not because its base colour is nearly black.
    "PhilodendronStem": hsv(104, 52, 28),
    "MonsteraStem": hsv(98, 48, 30),
    "HeliconiaStem": hsv(108, 58, 32),
    "CalatheaStem": hsv(116, 50, 33),
    # The flame-coloured bract spike, the one thing in the library that is not green.
    "HeliconiaBract": hsv(14, 78, 62),
}
FOLIAGE_PARTS = ("leaf", "canopy", "litter", "moss", "lichen", "bract")


def _hsv_of(rgb):
    import colorsys

    h, s, v = colorsys.rgb_to_hsv(*rgb)
    return h * 360.0, s * 100.0, v * 100.0


def separate_greens():
    """Spread each band's greens apart, because one global green is the loudest failure.

    Every species' hue and saturation is sampled from its own reference plates, and
    sampled independently they land on top of each other: twenty pairs of the library
    sat within 18 degrees of hue and 20 points of saturation of a neighbour, and the
    first rendered frame of a clump was one plastic green from the outside of the mass
    to the inside. The measured reference asks for 20-35 degrees of hue and 20-30
    points of saturation between adjacent species, over a 50-100 degree spread within
    one frame.

    Eight understory species cannot each be 20 degrees from all the others inside the
    green part of the spectrum, and the reference does not ask them to: it asks that a
    plant differ from *the plant beside it*, in both hue and saturation. So each band's
    greens are placed evenly across a wide window in the order their own sources put
    them - a yellow-green species stays the yellow-green one - and saturation
    alternates high and low along that order, which gives every neighbouring pair the
    full saturation split and every other pair more than 20 degrees of hue.

    What is sourced survives: the ranking, which species is yellower or greyer, each
    plant's value (its band decides how much light it gets), and the young, dead and
    backlit colours, which are recomputed from the new green with the offsets the
    species declared. Species whose colour is not a green at all - the lichen crust,
    the grey Tillandsia, the litter - are left alone.
    """
    import colorsys
    from dataclasses import replace

    for band in BANDS:
        members = [s for s in sorted(by_band(band), key=lambda s: s.key)
                   if s.leaf_tex and 60 <= _hsv_of(s.leaf_tex.green)[0] <= 185
                   and _hsv_of(s.leaf_tex.green)[1] >= 25]
        if len(members) < 2:
            continue
        ranked = sorted(members, key=lambda s: _hsv_of(s.leaf_tex.green)[0])
        hues = [_hsv_of(s.leaf_tex.green)[0] for s in ranked]
        sats = [_hsv_of(s.leaf_tex.green)[1] for s in ranked]
        n = len(ranked)
        centre = sorted(hues)[n // 2]
        width = min(96.0, max(50.0, 13.0 * (n - 1)))
        lo = _clamp(centre - width / 2, 74.0, 170.0 - width)
        sat_centre = sorted(sats)[n // 2]
        sat_lo = _clamp(sat_centre - 17.0, 22.0, 70.0)
        sat_hi = _clamp(sat_centre + 17.0, sat_lo + 22.0, 92.0)
        # Two ways to alternate; take the one that moves the sourced saturations least.
        phase = 0 if (sum(abs(sats[i] - (sat_lo if i % 2 == 0 else sat_hi))
                          for i in range(n))
                      <= sum(abs(sats[i] - (sat_hi if i % 2 == 0 else sat_lo))
                             for i in range(n))) else 1
        for i, s in enumerate(ranked):
            tex = s.leaf_tex
            h0, s0, v0 = _hsv_of(tex.green)
            hue = lo + width * (i / (n - 1))
            sat = (sat_lo if (i + phase) % 2 == 0 else sat_hi)
            moved = {}
            # `dead` is deliberately not in this list. It is not a green, so separating
            # the greens must not drag it: pushed along with heliconia's hue it came out
            # at 48 degrees and 55% saturation - an acid yellow - and the frame showed a
            # chrome-yellow mat at the base of the plant. A dead leaf is tan whatever
            # green the living one was.
            for field_name in ("green", "young"):
                ch, cs, cv = _hsv_of(getattr(tex, field_name))
                # Keep each state's own offset from the mature green.
                dh = ((ch - h0 + 180) % 360) - 180
                moved[field_name] = tuple(
                    round(c, 4) for c in colorsys.hsv_to_rgb(
                        ((hue + dh) % 360) / 360.0,
                        _clamp(sat + (cs - s0), 4.0, 96.0) / 100.0,
                        _clamp(cv, 4.0, 96.0) / 100.0))
            SPECIES[s.key] = replace(s, leaf_tex=replace(tex, **moved))


def temper_dead():
    """Hold every species' dead-leaf colour inside the measured floor.

    The reference measures the forest floor as two materials: dry sunlit litter at
    39-43% saturation and value 37-42, and wet or shaded floor that is almost neutral
    at 10%. Seven of the twenty-five species had picked a dead colour above the dry
    figure - up to 52% saturation and value 52 - and because a shed leaf lies flat and
    faces straight up, it is the most sun-facing surface in a frame: the skirt came back
    as a ring of ochre slabs brighter than any living leaf on the plant.

    Each species keeps its own hue, which is what distinguishes a dead palm frond from a
    dead aroid; only how saturated and how light it may be is held to the measurement.
    """
    import colorsys
    from dataclasses import replace

    for key, s in list(SPECIES.items()):
        if not s.leaf_tex:
            continue
        h, sat, v = _hsv_of(s.leaf_tex.dead)
        held = (min(sat, 43.0), _clamp(v, 32.0, 42.0))
        if (sat, v) == held:
            continue
        dead = tuple(round(c, 4) for c in
                     colorsys.hsv_to_rgb(h / 360.0, held[0] / 100.0, held[1] / 100.0))
        SPECIES[key] = replace(s, leaf_tex=replace(s.leaf_tex, dead=dead))


def colour_separation_errors():
    """Neighbouring species must not share a green.

    A stand of foliage reads as one plastic mass when every species sits on the same
    hue and saturation. The measured rule is 20-35 degrees of hue and 20-30 points of
    saturation between neighbours; this check passes a pair within one band if it
    clears either, which is what the available hue range (greens live in about 60
    degrees) actually allows.
    """
    import colorsys

    errors = []
    for band in BANDS:
        members = sorted(by_band(band), key=lambda s: s.key)
        for i, a in enumerate(members):
            for b in members[i + 1:]:
                if not (a.leaf_tex and b.leaf_tex):
                    continue
                ha, sa, _ = colorsys.rgb_to_hsv(*a.leaf_tex.green)
                hb, sb, _ = colorsys.rgb_to_hsv(*b.leaf_tex.green)
                d_hue = abs(ha - hb) * 360
                d_hue = min(d_hue, 360 - d_hue)
                d_sat = abs(sa - sb) * 100
                if d_hue < 18 and d_sat < 20:
                    errors.append(f"{a.key} and {b.key} are both in band '{band}' and differ by "
                                  f"only {d_hue:.0f} degrees of hue and {d_sat:.0f} points of "
                                  "saturation; neighbours need 18+ degrees or 20+ points")
    return errors


def materials():
    """{material key: spec} for every part of every species."""
    out = {}
    for s in sorted(SPECIES.values(), key=lambda s: s.key):
        for part, key in sorted(s.parts.items()):
            foliage = part in FOLIAGE_PARTS
            tex = s.leaf_tex if foliage else s.bark_tex
            spec = {"species": s.key, "part": part, "foliage": foliage}
            if key in STEM_COLOUR or tex is None:
                colour = STEM_COLOUR.get(key, (0.15, 0.17, 0.10))
                spec.update(kind="plain", albedo=colour, roughness=0.62)
            else:
                spec.update(kind="orm", texture=tex.name,
                            albedo=(1.0, 1.0, 1.0), roughness=None)
                if foliage:
                    spec["backlight"] = s.leaf_tex.backlight()
            out[key] = spec
    return out


def _colour(c):
    return f"Color({c[0]:.4f}, {c[1]:.4f}, {c[2]:.4f}, 1.0000)"


def material_text(key, spec):
    head = [f"; {SPDX}", "; Generated by tools/assets/plants.py; edit the species, not this file."]
    if spec["kind"] == "plain":
        return "\n".join([
            "[gd_resource type=\"StandardMaterial3D\" load_steps=1 format=3]", *head, "",
            "[resource]",
            f"albedo_color = {_colour(spec['albedo'])}",
            "metallic = 0.0",
            "metallic_specular = 0.5",
            f"roughness = {spec['roughness']:.2f}",
            "cull_mode = 2" if spec["foliage"] else "cull_mode = 0",
            "",
        ])
    tex = spec["texture"]
    lines = ["[gd_resource type=\"ORMMaterial3D\" load_steps=4 format=3]", *head, ""]
    for i, suffix in enumerate(("BC", "ORM", "N"), 1):
        lines.append(f'[ext_resource type="Texture2D" path="{TEXTURES}/T_{tex}_{suffix}.png" id="{i}"]')
    lines += ["", "[resource]",
              f"albedo_color = {_colour(spec['albedo'])}",
              'albedo_texture = ExtResource("1")',
              'orm_texture = ExtResource("2")',
              "metallic_specular = 0.5",
              "normal_enabled = true",
              "normal_scale = 1.0",
              'normal_texture = ExtResource("3")']
    if spec["foliage"]:
        lines += ["cull_mode = 2", "backlight_enabled = true",
                  f"backlight = {_colour(spec['backlight'])}"]
    return "\n".join(lines + [""])


def material_path(key):
    return MATERIALS / f"MI_Plant_{key}.tres"


# ==========================================================================
# Output


def _fmt(v):
    text = f"{v:.5f}"
    return "0.00000" if text == "-0.00000" else text


def obj_text(mesh, name):
    """Wavefront OBJ with UVs and per-corner normals, in a fixed order so the file is
    byte-identical across runs. Blender (x, y, z) Z-up becomes Godot (x, z, -y) Y-up."""
    lines = [f"# {SPDX}", "# Generated by tools/assets/plants.py", f"o {name}"]
    for v in mesh.vertices:
        x, y, z = v.co
        lines.append(f"v {_fmt(x)} {_fmt(z)} {_fmt(-y)}")
    uvs, normals, faces = {}, {}, []
    uv_data = mesh.uv_layers[0].data if mesh.uv_layers else None
    corner_normals = mesh.corner_normals
    for poly in mesh.polygons:
        corners = []
        for li in poly.loop_indices:
            nx, ny, nz = corner_normals[li].vector
            nkey = (_fmt(nx), _fmt(nz), _fmt(-ny))
            if uv_data is None:
                ukey = ("0.00000", "0.00000")
            else:
                u, v = uv_data[li].uv
                ukey = (_fmt(u), _fmt(v))
            corners.append((mesh.loops[li].vertex_index + 1,
                            uvs.setdefault(ukey, len(uvs) + 1),
                            normals.setdefault(nkey, len(normals) + 1)))
        faces.append(corners)
    lines += [f"vt {u} {v}" for u, v in uvs]
    lines += [f"vn {x} {y} {z}" for x, y, z in normals]
    lines.append("s 0")
    lines += ["f " + " ".join(f"{v}/{t}/{n}" for v, t, n in face) for face in faces]
    return "\n".join(lines) + "\n"


def write_obj(bm, path, smooth_angle=38.0):
    """Triangulate, shade and write one part. Face winding is authored, so the normals
    are never recalculated: a leaf is an open surface and recalculation would flip it.

    Returns (triangles, bounds) with bounds in Godot's axes and metres, because what a
    build actually measures is not what its species' range says: the monstera's range
    runs to 12 m because one of its three variants climbs, and the ground mat is half a
    metre. A look-dev camera framed off the range put that mat 15 m away.
    """
    import bpy

    bmesh = _bm_module()
    bmesh.ops.triangulate(bm, faces=bm.faces, quad_method="BEAUTY", ngon_method="BEAUTY")
    mesh = bpy.data.meshes.new(path.stem)
    bm.to_mesh(mesh)
    bm.free()
    mesh.shade_smooth()
    mesh.set_sharp_from_angle(angle=math.radians(smooth_angle))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(obj_text(mesh, path.stem))
    count = len(mesh.polygons)
    lo = [min((v.co[i] for v in mesh.vertices), default=0.0) for i in range(3)]
    hi = [max((v.co[i] for v in mesh.vertices), default=0.0) for i in range(3)]
    # Blender Z-up to Godot Y-up, the same swap obj_text writes the positions with.
    bounds = {"min": [round(lo[0], 3), round(lo[2], 3), round(-hi[1], 3)],
              "max": [round(hi[0], 3), round(hi[2], 3), round(-lo[1], 3)]}
    bpy.data.meshes.remove(mesh)
    return count, bounds


def union_bounds(a, b):
    return {"min": [min(a["min"][i], b["min"][i]) for i in range(3)],
            "max": [max(a["max"][i], b["max"][i]) for i in range(3)]}


def camel(key):
    return "".join(word.capitalize() for word in key.split("_"))


def obj_name(key, variant, part, lod):
    return f"SM_Plant_{camel(key)}{variant}_{part.capitalize()}" + (f"_LOD{lod}" if lod else "")


# --------------------------------------------------------------------------
# The catalogue. There is one for the whole project - game/assets/library.json,
# whose schema and seeded selection rule are docs/reference/asset-library.md -
# and the prop library writes its own section of the same file, so this one
# merges rather than replaces.

CATALOGUE = ROOT / "game/assets/library.json"
LIBRARY_VERSION = 1
BIOME = "lowland-rainforest"


def entry_tags(s, variant):
    """The scatter weights entries by tag; these are the ones the species data proves.

    Nothing is invented here: `shade` comes from the band, `mossy` from the species
    carrying moss or lichen in its own texture, `ruined` from a variant that grows on
    stone (which is the whole direction - the temple is an overgrown ruin), and
    `post-contact` from the species' own historical flag, so a pre-contact level can
    exclude it the same way `pick()` does.
    """
    tags = {s.band}
    if s.band in ("litter", "groundcover", "understory"):
        tags.add("shade")
    if s.band in ("canopy", "mid"):
        tags.add("edge")
    if s.key in ("moss_patch", "lichen_crust", "spikemoss"):
        tags.add("mossy")
    if s.bark_tex and (s.bark_tex.moss >= 0.3 or s.bark_tex.lichen >= 0.35):
        tags.add("mossy")
    if variant in ("Masonry", "Stone", "Log"):
        tags.add("ruined")
    if s.historical:
        tags.add("post-contact")
    return sorted(tags)


def library_entries(catalogue):
    """The build record as catalogue entries, one per build, in the shared schema.

    One entry per (species, variant) rather than per generator: a generator here makes
    builds of very different size - a 3 m young strangler fig and a 30 m enveloping one
    off the same function - and they cannot share one `height_m`, one `tris` ladder or
    one set of tags. `variants` still says how many builds the generator makes, and
    `species` links the siblings, so a query can go either way.
    """
    entries = {}
    for key, sp in catalogue.items():
        s = get(key)
        for variant, data in sp["variants"].items():
            lods = data["lods"]
            entries[f"plant.{key.replace('_', '-')}.{variant.lower()}"] = {
                "kind": "plant",
                "name": f"{sp['common']} ({variant.lower()})",
                "species": key,
                "variant": variant,
                "variants": len(sp["variants"]),
                "latin": sp["latin"],
                "reference": sp["reference"],
                "assumed": sp["assumed"] or None,
                "tells": sp["tells"],
                "culture": None,
                "era": None,
                "biome": BIOME,
                "band": sp["band"],
                "height_m": sp["height_m"],
                "spread_m": sp["spread_m"],
                "generator": "plants:build",
                "generator_call": f"plants.build({key!r}, rng, plants.Params(variant="
                                  f"{list(sp['variants']).index(variant)}, lod=0))",
                "textures": sorted(sp["textures"].values()),
                "tris": [lod["triangles"] for lod in lods],
                "bounds_m": lods[0]["bounds"],
                "budget_triangles": sp["budget_triangles"],
                "parts": {lod["lod"]: lod["parts"] for lod in lods},
                "tags": entry_tags(s, variant),
                "seed": f"plants:{key}:{variant}",
            }
    return entries


def write_library(entries, partial=False):
    """Merge the plant entries into the one catalogue, leaving every other key alone.

    Two generators write this file, so neither may rewrite it whole: props.py owns its
    own section and the vocabulary keys it needs, and this only ever replaces `plants`
    (or, for a `--only` build, the entries it rebuilt) and adds the band names plants
    use to the shared vocabulary.
    """
    doc = {}
    if CATALOGUE.exists():
        try:
            doc = json.loads(CATALOGUE.read_text())
        except json.JSONDecodeError:
            doc = {}
    doc.setdefault("$description",
                   "The shared asset library catalogue a level dresses itself from. "
                   "Generated by tools/assets/plants.py and tools/assets/props.py; edit "
                   "those, not this file. Schema and selection rule: "
                   "docs/reference/asset-library.md. "
                   "SPDX-License-Identifier: GPL-2.0-or-later")
    doc["version"] = max(doc.get("version", 0), LIBRARY_VERSION)
    plants = doc.get("plants", {}) if partial else {}
    plants.update(entries)
    doc["plants"] = dict(sorted(plants.items()))
    vocab = doc.setdefault("vocabulary", {})
    vocab["band"] = sorted(set(vocab.get("band", [])) | set(BANDS))
    vocab["kind"] = sorted(set(vocab.get("kind", [])) | {"plant"})
    vocab["biome"] = sorted(set(vocab.get("biome", [])) | {BIOME})
    doc["plant_triangle_budget"] = dict(TRI_BUDGET)
    CATALOGUE.parent.mkdir(parents=True, exist_ok=True)
    CATALOGUE.write_text(json.dumps(doc, indent=2) + "\n")
    return CATALOGUE


def build_all(only=None, seed=7):
    """Build every species and variant, write the OBJs, materials and the catalogue."""
    catalogue, totals = {}, {"objs": 0, "triangles": 0}
    errors = [] if only else colour_separation_errors()
    for s in sorted(SPECIES.values(), key=lambda s: s.key):
        if only and s.key not in only:
            continue
        entry = {
            "common": s.common, "latin": s.latin, "band": s.band,
            "height_m": list(s.height), "spread_m": list(s.spread),
            "tells": s.tells, "reference": s.reference,
            "assumed": s.assumed, "historical": s.historical,
            "budget_triangles": TRI_BUDGET[s.band],
            "textures": {k: v for k, v in (("leaf", s.leaf_tex.name if s.leaf_tex else None),
                                           ("bark", s.bark_tex.name if s.bark_tex else None))
                         if v},
            "variants": {},
        }
        for vi, variant in enumerate(s.variants):
            lods = []
            lod = 0
            while True:
                rng = random.Random(f"{seed}:{s.key}:{variant}:{lod}")
                meshes = build(s.key, rng, Params(variant=vi, lod=lod))
                record = {"lod": lod, "triangles": 0, "parts": {}, "bounds": None}
                for part, bm in sorted(meshes.items()):
                    count = tris(bm)
                    name = obj_name(s.key, variant, part, lod)
                    written, bounds = write_obj(bm, MODELS / f"{name}.obj")
                    record["bounds"] = (bounds if record["bounds"] is None
                                        else union_bounds(record["bounds"], bounds))
                    key = s.parts.get(part, part)
                    record["parts"][part] = {
                        "mesh": f"res://assets/models/plants/{name}.obj",
                        "material": f"res://assets/materials/plants/MI_Plant_{key}.tres",
                        "triangles": written,
                    }
                    record["triangles"] += written
                    totals["objs"] += 1
                    totals["triangles"] += written
                    if count != written:
                        errors.append(f"{name}: {count} triangles before triangulation, "
                                      f"{written} after; a face was n-gon or degenerate")
                lods.append(record)
                if lod == 0 and record["triangles"] > TRI_BUDGET[s.band]:
                    errors.append(f"{s.key}/{variant}: {record['triangles']} triangles at LOD0, "
                                  f"budget {TRI_BUDGET[s.band]} for band '{s.band}'; cut leaf "
                                  "count or segments in its builder")
                full = lods[0]["triangles"]
                if lod == 0 and full > LOD1_OVER:
                    lod = 1
                elif lod == 1 and full > LOD2_OVER:
                    lod = 2
                else:
                    break
            entry["variants"][variant] = {"lods": lods}
        catalogue[s.key] = entry

    specs = materials()
    for key, spec in sorted(specs.items()):
        if only and spec["species"] not in only:
            continue
        path = material_path(key)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(material_text(key, spec))

    manifest = {
        "bands": list(BANDS),
        "triangle_budget": TRI_BUDGET,
        "totals": totals,
        "textures": {name: {"recipe": spec["recipe"], "size": TEXTURE_SIZE}
                     for name, spec in sorted(texture_sets().items())},
        "species": catalogue,
    }
    MODELS.mkdir(parents=True, exist_ok=True)
    write_library(library_entries(catalogue), partial=bool(only))
    stale_manifest = MODELS / "manifest.json"
    if stale_manifest.exists():
        # There is one catalogue, and it is game/assets/library.json.
        stale_manifest.unlink()
    if not only:
        # A full build owns the folder: how many LODs a variant needs falls out of its
        # triangle count, so cutting triangles leaves the LOD a species no longer has
        # behind. Stale meshes are worse than clutter - nothing references them, so
        # nothing notices they are stale, and they ship.
        wanted = {Path(part["mesh"]).name
                  for sp in catalogue.values() for v in sp["variants"].values()
                  for lod in v["lods"] for part in lod["parts"].values()}
        for obj in sorted(MODELS.glob("SM_Plant_*.obj")):
            if obj.name not in wanted:
                obj.unlink()
                obj.with_name(obj.name + ".import").unlink(missing_ok=True)
                print(f"removed stale {obj.name}")
    return manifest, errors


# --------------------------------------------------------------------------
# Look-dev scenes: one row per band, every species at its real metre size under the
# temple theme's own sun and sky, with a 1.8 m human reference at the end of the row.

ENVIRONMENT = "res://assets/models/temple/backdrop/PF_Environment.tscn"


def lookdev_scenes(plants):
    """One scene per band, from the catalogue's plant entries."""
    written = []
    for band in BANDS:
        rows = [(eid, e) for eid, e in sorted(plants.items()) if e["band"] == band]
        if rows:
            written.append(_lookdev_scene(band, rows))
    return written


GROUND_MATERIAL = "res://assets/materials/temple/MI_Ground.tres"
MATERIAL_RES = "res://assets/materials/plants"
# What a band's plants grow on, for the look-dev page to lend them.
HOST_FOR = {"vine": "trunk", "epiphyte": "branch"}
THEME_PARAMS = ROOT / "tools/assets/themes/temple.json"
# Atmosphere the theme puts between the player and the temple, which has no business
# between the camera and a plant that is three metres away. The braziers go too: their
# flipbooks are built by tools/assets/fx/gen_fire.py, and a page rendered without that
# step shows four white cards the size of a door and nothing else.
HIDE_IN_LOOKDEV = ("AtmosphereFog", "Dust", "LightShaftCards",
                   "Fire1", "Fire2", "Fire3", "Fire4")


def _camera_attributes():
    """The theme's exposure, without the theme's depth of field.

    `PF_Environment` carries both on one `CameraAttributesPractical`, and its far blur
    starts at 0.5 m because it is there to push the backdrop away from the board. On a
    look-dev page it blurs the subject: the first frame of the library came back soft
    from front to back. A Camera3D's own attributes override the WorldEnvironment's, so
    the page keeps the theme's exposure - which is the thing the grey card is for - and
    drops the blur.
    """
    exposure = json.loads(THEME_PARAMS.read_text())["lighting"]["exposure"]
    return ('[sub_resource type="CameraAttributesPractical" id="camera"]\n'
            f'exposure_sensitivity = {exposure["sensitivity"]}\n'
            f'exposure_multiplier = {exposure["multiplier"]}\n'
            "dof_blur_far_enabled = false\n")


def _hide_atmosphere():
    return [f'[node name="{name}" parent="Environment"]\nvisible = false\n'
            for name in HIDE_IN_LOOKDEV]


def _sun_for(extent):
    """Pull the sun's shadow range in around the subject of the page.

    The theme casts directional shadows over 40 m, which is right for a temple and
    wrong for one plant: spread over that range the shadow map has so few texels on a
    3 m heliconia that a leaf's shadow on the leaf below it comes back as hard black
    rectangles with stepped edges. A plant's own shadow on itself is most of what makes
    a leaf mass read as a mass, so the page gives it the texels.
    """
    return (f'[node name="Sun" parent="Environment"]\n'
            f"directional_shadow_max_distance = {max(8.0, extent * 3.0):.1f}\n"
            "directional_shadow_blend_splits = true\n")


def closeup_scenes(plants, only=None, empty=False):
    """One scene per specimen, framed so a leaf is big enough to judge.

    The band rows are the wrong page for the only question that matters - does this
    read as a plant or as folded cardboard - because a band's row is as wide as the
    sum of its spreads: the understory row is 68 m across, so its camera stands 55 m
    back and a 1 m blade lands on 40 pixels of a 4K frame. Nothing about a keel, an
    age mix or a litter skirt survives that.

    So: one plant, filling the frame, from a three-quarter view at human eye height,
    with the 1.8 m grey card beside it for scale and exposure.
    """
    written = []
    for eid, e in sorted(plants.items()):
        if only and e["species"] not in only:
            continue
        written.append(_closeup_scene(eid, e))
        if empty:
            # The same page with the plant taken out. Measuring a frame means telling
            # foliage from ground, and hue cannot do it here: the temple's limestone
            # plaza is a warm tan that lands squarely in the dead-leaf hue window, so
            # the first measurement counted the floor as 43% dead leaf. Differencing
            # the two frames gives an exact mask, and the ground that the plant covers
            # also gives the contact-shadow figure for free.
            written.append(_closeup_scene(eid, e, empty=True))
    return written


def _closeup_scene(eid, e, empty=False):
    ext, nodes = [], []
    next_id = [1]

    def resource(kind, path):
        rid = str(next_id[0])
        next_id[0] += 1
        ext.append(f'[ext_resource type="{kind}" path="{path}" id="{rid}"]')
        return rid

    env_id = resource("PackedScene", ENVIRONMENT)
    ground_id = resource("Material", GROUND_MATERIAL)
    box = e.get("bounds_m")
    if box:
        height = max(0.25, box["max"][1])
        spread = max(0.4, max(box["max"][0] - box["min"][0], box["max"][2] - box["min"][2]))
    else:
        height = max(0.25, e["height_m"][1])
        spread = max(0.4, e["spread_m"][1])
    lod0 = e["parts"]["0"] if "0" in e["parts"] else e["parts"][0]
    if not empty:
        for part, info in sorted(lod0.items()):
            mesh_id = resource("Mesh", info["mesh"])
            mat_id = resource("Material", info["material"])
            nodes.append(f'[node name="{part.capitalize()}" type="MeshInstance3D" parent="."]\n'
                         f'mesh = ExtResource("{mesh_id}")\n'
                         f'surface_material_override/0 = ExtResource("{mat_id}")\n')
    ground = max(12.0, spread * 4 + height * 2)
    subs = [f'[sub_resource type="BoxMesh" id="ground"]\nsize = Vector3({ground:.1f}, 0.1, '
            f'{ground:.1f})\n',
            '[sub_resource type="BoxMesh" id="ref"]\nsize = Vector3(0.45, 1.8, 0.25)\n',
            '[sub_resource type="StandardMaterial3D" id="grey"]\n'
            "albedo_color = Color(0.4600, 0.4600, 0.4600, 1.0000)\nroughness = 0.9\n",
            _camera_attributes()]
    nodes.extend(_hide_atmosphere())
    nodes.append('[node name="Ground" type="MeshInstance3D" parent="."]\n'
                 "transform = Transform3D(1, 0, 0, 0, 1, 0, 0, 0, 1, 0, -0.05, 0)\n"
                 'mesh = SubResource("ground")\n'
                 f'surface_material_override/0 = ExtResource("{ground_id}")\n')
    nodes.append('[node name="GreyCard" type="MeshInstance3D" parent="."]\n'
                 "transform = Transform3D(1, 0, 0, 0, 1, 0, 0, 0, 1, "
                 f"{spread * 0.5 + 0.6:.2f}, 0.9, {-spread * 0.3:.2f})\n"
                 'mesh = SubResource("ref")\n'
                 'surface_material_override/0 = SubResource("grey")\n')
    # A climber, a vine and an epiphyte are built to grow on something, and on an empty
    # plaza they render as a bare stem with a leaf on it - the monstera shingle and the
    # liana came back as a vertical line in the sky. The page lends them a host: a plain
    # trunk behind a climber, a branch under an epiphyte. It is scenery for the page, not
    # part of the plant, which is why it is a stock cylinder and not a generated tree.
    # Climbing variants of understory aroids need one too, and they are named for it.
    host = ("trunk" if e["variant"] in ("Climber", "Shingle", "Climbing", "Masonry",
                                        "Enveloping")
            else HOST_FOR.get(e["band"]))
    if host == "trunk":
        h = max(2.5, height * 1.25)
        subs.append(f'[sub_resource type="CylinderMesh" id="host"]\ntop_radius = 0.17\n'
                    f"bottom_radius = 0.22\nheight = {h:.2f}\nradial_segments = 12\n")
        bark_id = resource("Material", f"{MATERIAL_RES}/MI_Plant_CanopyBark.tres")
        nodes.append('[node name="HostTrunk" type="MeshInstance3D" parent="."]\n'
                     "transform = Transform3D(1, 0, 0, 0, 1, 0, 0, 0, 1, 0, "
                     f"{h / 2:.2f}, -0.30)\n"
                     'mesh = SubResource("host")\n'
                     f'surface_material_override/0 = ExtResource("{bark_id}")\n')
    elif host == "branch":
        subs.append('[sub_resource type="CylinderMesh" id="host"]\ntop_radius = 0.11\n'
                    "bottom_radius = 0.13\nheight = 2.4\nradial_segments = 10\n")
        bark_id = resource("Material", f"{MATERIAL_RES}/MI_Plant_CanopyBark.tres")
        nodes.append('[node name="HostBranch" type="MeshInstance3D" parent="."]\n'
                     "transform = Transform3D(0, -1, 0, 1, 0, 0, 0, 0, 1, 0, -0.07, "
                     f"{-spread * 0.1:.2f})\n"
                     'mesh = SubResource("host")\n'
                     f'surface_material_override/0 = ExtResource("{bark_id}")\n')
    # Frame the plant: 50 degrees vertical over the taller of its height and its spread,
    # with a quarter of a frame of air, from three quarters round so the depth of the
    # mass reads rather than a flat elevation.
    extent = max(height, spread * 0.75)
    distance = max(1.2, extent * 1.34)          # the plant fills about 80% of the frame
    nodes.append(_sun_for(extent))
    eye = max(1.65, height * 0.55)
    target = (0.0, height * 0.5, 0.0)
    yaw = math.radians(35.0)
    pos = (math.sin(yaw) * distance, eye, math.cos(yaw) * distance)
    nodes.append('[node name="Camera" type="Camera3D" parent="."]\n'
                 f"transform = {look_at(pos, target)}\nfov = 50.0\n"
                 'attributes = SubResource("camera")\n')
    text = ("[gd_scene load_steps=%d format=3]\n" % (len(ext) + len(subs) + 1)
            + f"; {SPDX}\n"
            + f"; Generated by tools/assets/plants.py closeups ({eid}"
            + ("; the same page with the plant removed, for measuring against" if empty else "")
            + "); not shipped.\n\n"
            + "\n".join(ext) + "\n\n" + "\n".join(subs)
            + f'\n[node name="LookDev_{camel(e["species"])}{e["variant"]}'
            + f'{"Empty" if empty else ""}" type="Node3D"]\n\n'
            + f'[node name="Environment" parent="." instance=ExtResource("{env_id}")]\n\n'
            + "\n".join(nodes))
    path = (LOOKDEV
            / f"LD_Plant_{camel(e['species'])}{e['variant']}{'_Empty' if empty else ''}.tscn")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return path


def look_at(eye, target, up=(0.0, 1.0, 0.0)):
    """A Transform3D looking from `eye` at `target`, written basis-rows first."""
    f = [target[i] - eye[i] for i in range(3)]
    fl = math.sqrt(sum(c * c for c in f)) or 1.0
    z = [-c / fl for c in f]
    x = [up[1] * z[2] - up[2] * z[1], up[2] * z[0] - up[0] * z[2], up[0] * z[1] - up[1] * z[0]]
    xl = math.sqrt(sum(c * c for c in x)) or 1.0
    x = [c / xl for c in x]
    y = [z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]]
    rows = [x[0], y[0], z[0], x[1], y[1], z[1], x[2], y[2], z[2], eye[0], eye[1], eye[2]]
    return "Transform3D(" + ", ".join(f"{v:.6f}" for v in rows) + ")"


def _lookdev_scene(band, rows):
    """One scene: the band's specimens in a row, spaced by their own spread.

    The row stands on the temple's own plaza material under the temple's own sun
    and sky, so what the frame shows is what the game shows. The grey block at the
    end is 1.8 m tall and an 18% grey card, so both scale and exposure are readable
    off the frame instead of argued about.
    """
    ext, nodes, step = [], [], 0.0
    next_id = [1]

    def resource(kind, path):
        rid = str(next_id[0])
        next_id[0] += 1
        ext.append(f'[ext_resource type="{kind}" path="{path}" id="{rid}"]')
        return rid

    env_id = resource("PackedScene", ENVIRONMENT)
    ground_id = resource("Material", GROUND_MATERIAL)
    positions = []
    for eid, e in rows:
        spread = max(0.6, e["spread_m"][1])
        step += spread * 0.6
        positions.append((eid, e, step))
        step += spread * 0.6
    tallest = max(e["height_m"][1] for _, e, _ in positions)
    for eid, e, x in positions:
        lod0 = e["parts"]["0"] if "0" in e["parts"] else e["parts"][0]
        for part, info in sorted(lod0.items()):
            mesh_id = resource("Mesh", info["mesh"])
            mat_id = resource("Material", info["material"])
            nodes.append(f'[node name="{camel(e["species"])}{e["variant"]}'
                         f'{part.capitalize()}" '
                         'type="MeshInstance3D" parent="."]\n'
                         "transform = Transform3D(1, 0, 0, 0, 1, 0, 0, 0, 1, "
                         f"{x - step / 2:.3f}, 0, 0)\n"
                         f'mesh = ExtResource("{mesh_id}")\n'
                         f'surface_material_override/0 = ExtResource("{mat_id}")\n')
    subs = [f'[sub_resource type="BoxMesh" id="ground"]\nsize = Vector3({step + 6:.1f}, 0.1, 14)\n',
            '[sub_resource type="BoxMesh" id="ref"]\nsize = Vector3(0.45, 1.8, 0.25)\n',
            '[sub_resource type="StandardMaterial3D" id="grey"]\n'
            "albedo_color = Color(0.4600, 0.4600, 0.4600, 1.0000)\nroughness = 0.9\n",
            _camera_attributes()]
    nodes.extend(_hide_atmosphere())
    nodes.append('[node name="Ground" type="MeshInstance3D" parent="."]\n'
                 "transform = Transform3D(1, 0, 0, 0, 1, 0, 0, 0, 1, 0, -0.05, 0)\n"
                 'mesh = SubResource("ground")\n'
                 f'surface_material_override/0 = ExtResource("{ground_id}")\n')
    nodes.append('[node name="GreyCard" type="MeshInstance3D" parent="."]\n'
                 "transform = Transform3D(1, 0, 0, 0, 1, 0, 0, 0, 1, "
                 f"{step / 2 + 1.2:.3f}, 0.9, 0)\n"
                 'mesh = SubResource("ref")\n'
                 'surface_material_override/0 = SubResource("grey")\n')
    # Eye height, far enough back that the whole row fits at 50 degrees.
    distance = max(6.0, step * 0.8, tallest * 1.4)
    height = max(1.65, tallest * 0.42)
    pitch = -math.atan2(height - tallest * 0.45, distance)
    c, sn = math.cos(pitch), math.sin(pitch)
    nodes.append('[node name="Camera" type="Camera3D" parent="."]\n'
                 f"transform = Transform3D(1, 0, 0, 0, {c:.6f}, {-sn:.6f}, 0, {sn:.6f}, {c:.6f}, "
                 f"0, {height:.2f}, {distance:.2f})\n"
                 "fov = 50.0\n"
                 'attributes = SubResource("camera")\n')
    text = ("[gd_scene load_steps=%d format=3]\n" % (len(ext) + len(subs) + 1)
            + f"; {SPDX}\n"
            + "; Generated by tools/assets/plants.py lookdev; not shipped.\n\n"
            + "\n".join(ext) + "\n\n" + "\n".join(subs)
            + f'\n[node name="LookDev_Plants_{band.capitalize()}" type="Node3D"]\n\n'
            + f'[node name="Environment" parent="." instance=ExtResource("{env_id}")]\n\n'
            + "\n".join(nodes))
    path = LOOKDEV / f"LD_Plants_{band.capitalize()}.tscn"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return path


# ==========================================================================
# Command line


def cmd_list(args):
    rows = [("key", "band", "variants", "height m", "spread m", "latin")]
    for s in sorted(SPECIES.values(), key=lambda s: (BANDS.index(s.band), s.key)):
        rows.append((s.key, s.band, "/".join(s.variants),
                     f"{s.height[0]:g}-{s.height[1]:g}", f"{s.spread[0]:g}-{s.spread[1]:g}",
                     s.latin + (" (post-contact)" if s.historical else "")))
    widths = [max(len(r[i]) for r in rows) for i in range(len(rows[0]))]
    for row in rows:
        print("  ".join(cell.ljust(w) for cell, w in zip(row, widths)).rstrip())
    print(f"\n{len(SPECIES)} species, "
          f"{sum(len(s.variants) for s in SPECIES.values())} variants, "
          f"{len(texture_sets())} texture sets")


def cmd_build(args):
    only = set(args.only.split(",")) if args.only else None
    if only:
        for key in only:
            get(key)
    manifest, errors = build_all(only=only, seed=args.seed)
    for key, entry in manifest["species"].items():
        for variant, data in entry["variants"].items():
            chain = ", ".join(f"LOD{step['lod']} {step['triangles']}" for step in data["lods"])
            print(f"{key}/{variant}: {chain} (budget {entry['budget_triangles']})")
    print(f"{manifest['totals']['objs']} meshes, {manifest['totals']['triangles']} triangles")
    if errors:
        sys.exit("\n".join(errors))


def cmd_textures(args):
    path = ROOT / "tools/assets/themes/plants.json"
    path.write_text(json.dumps(texture_params(), indent=2) + "\n")
    print(f"{path.relative_to(ROOT)}: {len(texture_sets())} sets at {TEXTURE_SIZE} px")
    for e in colour_separation_errors():
        print(f"warning: {e}")


def _catalogue_plants():
    if not CATALOGUE.exists():
        sys.exit(f"{CATALOGUE} is missing; run `plants.py build` first")
    plants = json.loads(CATALOGUE.read_text()).get("plants")
    if not plants:
        sys.exit(f"{CATALOGUE} holds no plant entries; run `plants.py build` first")
    return plants


def cmd_lookdev(args):
    for scene in lookdev_scenes(_catalogue_plants()):
        print(scene.relative_to(ROOT))


def cmd_closeups(args):
    only = set(args.only.split(",")) if args.only else None
    for scene in closeup_scenes(_catalogue_plants(), only=only, empty=args.empty):
        print(scene.relative_to(ROOT))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("list", help="print the catalogue").set_defaults(run=cmd_list)
    build_cmd = sub.add_parser("build", help="write meshes, materials and the manifest")
    build_cmd.add_argument("--only", help="comma-separated species keys")
    build_cmd.add_argument("--seed", type=int, default=7)
    build_cmd.set_defaults(run=cmd_build)
    sub.add_parser("textures", help="write tools/assets/themes/plants.json").set_defaults(
        run=cmd_textures)
    sub.add_parser("lookdev", help="write the look-dev scenes").set_defaults(run=cmd_lookdev)
    close_cmd = sub.add_parser("closeups", help="write one look-dev scene per specimen")
    close_cmd.add_argument("--only", help="comma-separated species keys")
    close_cmd.add_argument("--empty", action="store_true",
                           help="also write each page with the plant removed, to measure against")
    close_cmd.set_defaults(run=cmd_closeups)
    if argv is None:
        # Inside Blender (`blender -b -P plants.py -- build`) everything before the
        # `--` belongs to Blender, not to us.
        argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    args = parser.parse_args(argv)
    args.run(args)


# Every species is registered by now, so the band-wide colour separation can run. It
# has to happen here rather than inside a command: `textures`, `build` and an importing
# caller must all see the same greens.
separate_greens()
temper_dead()


if __name__ == "__main__":
    main()

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# Foliage reference

Measured observations of tropical understory from thirteen openly
licensed photographs, for authoring plants in
[Asset library](asset-library.md). Colour figures are machine-sampled
medians in sRGB (with HSV where it matters); the photographs carry their
own processing, so ratios and hue offsets are firmer than absolute
values. Nothing from these images is used in an asset — they are
measurements only.

Setting and species belong to [Mesoamerican reference](mesoamerican-reference.md);
this page is about how leaves are shaped, coloured and lit.

## Shape

| Plant | Blade | Form |
|---|---|---|
| Monstera, philodendron | aspect 1.25–1.45:1, mature 0.6–0.9 m | pinnate splits 50–80% to the midrib, 7–12 per side, never symmetric, gaps widening to the tip; closed fenestrations only on the basal third; juveniles on the same mat 0.12–0.25 m and entire — one species, two shapes, ×6 size range |
| Heliconia | aspect 3–5:1, 0.4–0.9 m | keeled: folded 20–60° along the midrib at the base, flat mid-blade, curling down near the tip |
| Banana | aspect 2.5–3.5:1, 1.0–1.8 m | shreds along secondary veins, 5–15 splits per leaf running 20–60% of the half-width; only the newest leaf is intact |
| Fan palm | fan 0.7–1.1 m | costapalmate, 20–30 segments over 160–200°, split 60–75% of the radius, each segment with its own 2–4 mm V-fold, outer ones sagging 10–20 cm; a third carry 1–4 cm of dead tip |
| Ferns | frond 1.5–2.5 m | bipinnate to tripinnate; pinnae at 55–70° to the rachis, 8–15 cm; pinnules 8–15 mm with 4–8 teeth |
| Taro, Alocasia | aspect 1.3:1, 0.5–1.0 m | sagittate-cordate, peltate, undulating margin, lamina quilted 5–10 mm between veins |

No flat leaves occur in any photograph. Every blade is a saddle: 20–40°
dihedral between halves, the midrib arcing down 15–30°, lobes twisting
10–45° out of plane. Petioles are 0.4–0.8 m, stiff and arcing, and the
blade plane is rarely perpendicular to the petiole.

## Colour

- **Value range is the headline.** One species in one frame runs 5.5:1 —
  sunlit Monstera `#7D8B62` (sat 28%, val 55) against a deep-shade leaf
  at val 10. In a layered thicket 23% of the frame sits below value 12,
  where open plates (palmetto carpet, backlit leaf) have essentially
  none. Near-black belongs to depth of mass, not to foliage.
- **Hue spread within one frame is 50–100°** (p10→p90), and adjacent
  species differ by 20–35° of hue and 20–30 points of saturation. One
  global green is the loudest failure.
- **Shade shifts hue bluer as well as darker**: sunlit young banana
  `#89D310` (sat 92%, val 83) against shaded mature `#3F8A21` (sat 76%,
  val 54) — 0.65× value, ~20° bluer.
- **Lamina saturation is low**: 16–33% on the plates measured.
- **Backfaces** are 1.5–2.5× the value of the front, 30–50% less
  saturated, and matte.
- **Midribs are never the blade colour**, and go both ways: a pale
  Alocasia petiole at 1.7× lamina value, a dark red-brown fern rachis.
- **Non-green is 10–20% of leaf area**: one yellow leaf per 6–12 green,
  dead leaves hanging vertically on nearly every stem — a silhouette cue
  usually missing from rendered plants.

## Light response

- **The satin highlight is neutral and barely brighter.** Shaded
  Alocasia lamina `#788972` (sat 16%, val 54); its sheen band `#B6B9B8`
  (sat 2%, val 73). Gloss shows as saturation collapsing toward grey at
  about 1.35× value, never clipping. A bright green specular is wrong.
- Gloss is broad, not a hotspot — roughness around 0.3–0.4 on aroids,
  the band spanning 30–40% of the blade — and it follows the raised
  veins, each carrying a 2–4 mm highlight line, which is what makes a
  leaf read ribbed at a distance. Heliconia and banana are satin and
  anisotropic along the pleats; palmetto is near-matte; litter is matte.
- **Backlit leaves are flat, bright and saturated** (`#87AD47`, sat 60%)
  with a dark vein net at ~40% of lamina value and an opaque near-black
  midrib. The whole frame spans only p5/p95 = 38/75 — that narrowness is
  the look. A backlit leaf is brighter and more saturated than a leaf in
  reflected light in the same frame.
- **Self-shadow dominates the directional light**: value falls roughly
  exponentially with depth into a clump — one layer down ~50%, two ~25%,
  three ~10%.
- Sun dapples in hard-edged patches covering 15–30% of a leaf mass at a
  10–50 cm scale. Uniform lighting across a plant mass is wrong even in
  full sun.

## Detail by distance

At 30 cm the full reticulate net resolves, with a hard sub-millimetre
margin and the blade reading as a zero-thickness sheet. **Past about 2 m
veins are gone**: silhouette, the dihedral fold with its self-shadow, and
the dark slot between lobes carry everything. Spend geometry on
curvature, not veins.

Damage is routine, not an accent: 3–15 mm holes and nicked margins each
with a 1–2 mm pale halo, 8–12% of pinna area lost on a bitten frond, and
a third to a half of all leaves carrying at least one. An intact leaf set
is the loudest tell that something was generated.

## Ground

- **Two floor materials.** Dry sunlit litter is warm and moderately
  saturated (`#5E4C35`, `#6A593E`, sat 39–43%); wet or shaded floor is
  near-neutral (soil `#7E7573` at sat 10%, litter `#85817C` at 10%, pale
  bare soil `#B4B1AD` at 4%).
- **Exposed roots are darker than the soil** (`#6A6365` against
  `#7E7573`), spreading 1–3 m, braiding, alternately buried and 5–15 cm
  proud.
- **Every plant sits in its own litter skirt**: dead sheaths and fronds
  0.6–1.0 m around the base, 10–25 cm deep, often more dead material by
  area than green at ground level. Green never meets bare soil.
- **Contact shadow is ~41% of lit ground value** — not black — but tight,
  falling off over 3–10 cm. Soil under a plant is 20–30% darker than a
  metre away. Renders commonly invert this: contact too dark, mass
  interior too light.
- Moss is patchy and local, never a uniform skirt.

## Composition

Area fractions in a mixed understory: large palmate and compound leaves
~45%, fine pinnate fern and grass ~30%, bare woody stems ~15%, litter
~10%. Near-vertical stems take 25–30% of the frame in a heliconia stand —
that vertical component is what reads as jungle rather than shrubbery.
Sky through the mass is a few percent at most, as small scattered holes,
never a readable gradient. Each leaf is 30–60% occluded and mats are 3–5
layers deep; no leaf in any photograph is fully visible.

Within one species in one frame, sizes vary ×3.5 to ×6. A ±20% scale
jitter is an order of magnitude short. What breaks repetition, in order
of visual weight: orientation (azimuth uniform over 360°, tilt spread
~50°), leaf count and age mix per plant, damage pattern, how far
neighbours bury each plant, and only last, scale — which is the one
procedural systems usually lean on alone.

## Sources

All images openly licensed; used for measurement only, none redistributed.

| # | Subject | Licence and credit |
|---|---|---|
| 01, 02 | Monstera carpet, Maui | CC BY 3.0 US, Forest & Kim Starr |
| 03 | Heliconia hedge with litter mat, Maui | CC BY 3.0 US, Forest & Kim Starr |
| 04 | Heliconia stand, Veragua, Costa Rica | CC BY-SA 4.0, Thomas Liptak |
| 05 | Backlit heliconia paddles | CC BY-SA 2.0, Dinesh Valke |
| 06 | Heliconia and calathea, dappled light, Brazil | CC BY-SA 4.0, Cesar Augusto Chirosa Horie |
| 07, 16 | Saw palmetto, Florida | CC BY 2.0, Daniel Oines; CC BY 2.0, Miguel Vieira |
| 08 | Monstera thicket with sun patches, Madeira | CC BY 2.0, Sebastian from the EU |
| 09 | Tree-fern frond underside, Maui | CC BY 3.0 US, Forest & Kim Starr |
| 10 | Dicksonia squarrosa, New Zealand | CC0, Bernard Spragg |
| 11 | Leaf litter and soil, Western Ghats | CC BY-SA 4.0, 12voltechnologies |
| 13 | Wind-shredded banana clump, Maui | CC BY 3.0 US, Forest & Kim Starr |
| 14 | Backlit taro leaf | CC BY-SA 3.0, Avenue |
| 15 | Alocasia macrorrhizos leaf, Maui | CC BY 3.0 US, Forest & Kim Starr |
| 17 | Forest floor and surface roots, Sinharaja | CC BY-SA 4.0, Ruwan Illeperuma |
| 18 | Mixed understory, Way Kambas, Sumatra | CC BY-SA 4.0, Andreas Christian |

Gaps worth filling: no plate shows bright direct sun on a dense
understory from a few metres, and none shows a philodendron at ground
level.

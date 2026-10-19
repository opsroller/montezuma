<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# About the atmosphere

*Explanation. Why the temple's mist is one fog volume that is empty over
the board, why it lies in banks on the plaza and is lit as brightly as it
is, and why mobile and web get cards. The parameters are in
[Board themes: Atmosphere](../reference/board-themes.md#atmosphere).*

## What it has to do

The [visual target](../reference/visual-target.md) asks for "shafts of sun
through dusty air" on desktop and light-shaft cards in their place on
mobile and web, and readability rule 2 asks for zero fog, haze or dust
over the board. The first lighting pass (WP-47) found that even height
fog starting below the platform left a haze over the board on Forward+,
so the temple has no environment fog at all
([About lighting](about-lighting.md)). Whatever adds air has to add it
only where the board is not.

## One volume, empty in the middle

Godot's volumetric fog sums the density of every `FogVolume` into a grid
of froxels (frustum-shaped cells) in front of the camera and lights them
with the scene's lights and shadows. A ring of box volumes around the
platform would keep the board clear, but where two boxes meet, each
fades out at its edge and a thin seam of clear air shows between them.

So the temple has a single box over the whole scene, and its fog shader
decides the density from the world position: zero inside a square around
the board centre, ramping up over 1.2 m outside it. The rule is written
in metres in world space, so it holds for every camera, window shape and
tier, and nothing can drift onto the board. The square is the board plus
1.6 m. That margin covers what a froxel smears sideways: at 64 froxels
across a 1440p screen a froxel is about 25 cm wide where the board is,
and the fog's blur and temporal reprojection spread it by a few froxels
at most.

The fog stays low (it thins to nothing 2.4 m above the board), because
seen from a camera nearly straight above, rays to the board's edge pass
over the platform at a height that grows with distance from the board;
low air beside the platform is never in their way.

## Why the first pass looked like dirty glass

The first pass (WP-51) filled the plaza with an even layer of cream air,
2.4 m deep, lit by single scattering. From a camera looking almost
straight down, an even layer is just a tint over the floor: nothing in it
changes from place to place, so the eye reads it as a filter on the
picture rather than as something lying in the scene. It was also dim.
Godot lights fog by single scattering, and an isotropic droplet layer lit
that way returns less light than the sunlit floor under it, so the "fog"
made the plaza greyer and browner rather than whiter. And because auto
exposure meters the whole frame, the brighter haze pulled the board
darker too.

## Mist in banks

Real mist (see [Fog references](art-direction.md#fog-references)) is
read from how its thickness changes: floor crisp in a gap, floor gone
under a bank, a fern sinking into it. So the shader builds the mist from
the floor up. A large, slowly drifting noise decides where banks lie and
how tall each is, with a narrow mapping so there is clear floor between
them; a finer noise drawn out along the wind breaks up their tops and
edges. Density is full at the floor and fades out over the top of each
bank. Banks next to the platform are half as tall as those 7 m out, so
the mist deepens away from the temple, which from above reads as depth.

The fog volume now stops at the board's height. Nothing above the plaza
needs fog any more, and the smaller volume leaves fewer froxels for the
shader to fill.

## Lit like mist, not like dust

Mist droplets scatter almost all light and absorb almost none, at every
colour, so the albedo is near white; the warm late sun gives the mist its
colour. Since Godot's fog only scatters once, the sun's
`light_volumetric_fog_energy` is raised to 7 to stand in for the many
scatterings in mist this thick, which is what makes real mist about as
bright as a white wall in the same light. Ambient and GI injection are
low, so mist in shade is dim and slightly cool, and the shadows of the
temple and ferns stay dark lanes through it: that is where shafts of light
come from in real fog. Godot's fog does not shadow itself either, so the
lower part of each bank scatters three quarters of the light its top
does, which gives the banks lit tops.

The phase function stays isotropic. The sun is behind and above the
camera: light that reaches the camera from the mist has turned through
about 144 degrees. A single-scattering forward lobe (real droplets have
one) would send that light away from the camera and darken the mist, but
in mist this thick the many scatterings even the lobe out, so isotropic is
the closer match to what the eye sees.

Auto exposure, which only the desktop tiers have, as they alone have the
fog, still meters the brighter frame. The theme's `auto_scale` goes from
0.4 to 0.45 so the board renders as bright as it does with no fog at all
(mean RGB (162, 135, 114) against (163, 136, 114) without fog, 1920×1080,
`e1-l01`, frame 150, RTX 4080 SUPER).

## Dust

The motes are 320 tiny camera-facing particles in a ring around the
platform. They are lit and additive, so a mote shows only where the sun
or a brazier reaches it and disappears in shadow, which is what makes dust
in a sunbeam read. They exist only on the desktop tiers, with the fog.

## Cards on mobile and web

The Mobile and Compatibility renderers have no volumetric fog. The cards
that stand in for it are twelve soft additive quads along the sun's
direction, beside the platform: twelve draw calls, which the web build's
draw-call budget has room for. They are plain meshes with no
depth-texture soft edge (they fade toward the ground in their own UVs
instead): the cards reading the depth texture made ANGLE's
Metal backend, which runs the Compatibility renderer on macOS, abort on
the virtual Mac GPU the release smoke test runs on. The generator checks
every point of every card against the game camera at the distances it
uses from a wide window to a tall phone, and places them again until none
draws over the board.

The fog shader itself cannot even load on Compatibility (Godot prints an
error for any fog shader there), so the fog volume names its material by
path and the quality tier system loads it only on tiers with volumetric
fog.

## Cost

The fog grid's cost is set by the tier (64 froxels across and 64 deep on
`desktop-high`, 32 and 32 on `desktop-standard`), not by the volumes in
it; the temple's one volume adds one 3D noise sample per froxel. The
dust is one particle system of 320 quads and one draw call.

On the reference Mac (Mac mini M4, Godot 4.7.2 on Metal) at
`desktop-high`, 2560×1440 with vsync off, lightmaps baked and the
busiest-board replay, the atmosphere adds about 0.05 ms to the median
frame, 0.13 ms to the mean, 0.48 ms at p95 and 0.66 ms at p99. Two runs
of the same build differ by 0.5 ms at the median, so the median change is
within run-to-run noise. It adds 22 MB of video memory and one draw call,
and leaves CPU time unchanged. The haze does not wash the board out: the
frame's saturation rises from 58.8 to 72.2 and its mean luma falls from
133.3 to 131.1. The runs and their conditions are in
[Benchmark results](../reference/benchmark-results.md#wp-51-atmosphere).

The ground mist rework (banks on the floor, a volume that stops at the
board's height) costs next to nothing: on the reference Mac at native 5K
with no upscaler the median frame is 139.9 ms against 140.3 ms before, and
on an RTX 4080 SUPER at native 4K, full screen and uncapped, the GPU time
per frame goes from 9.78 to 9.85 ms; video memory is unchanged on both
([Benchmark results](../reference/benchmark-results.md#ground-mist-rework)).

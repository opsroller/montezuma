<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# About the atmosphere

*Explanation. Why the temple's dusty air is one fog volume that is empty
over the board, why its shafts come from stretched noise, and why mobile
and web get cards. The parameters are in
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

## Shafts from above

Shafts in fog come from shadow: lit air beside air that something shades.
From the game camera, looking almost straight down, a shaft is not a beam
coming at you but a streak along the ground in the sun's direction. The
temple's tiers, pillars and ferns cast some, but they are short. The fog
shader adds the rest by stretching its noise eight times along the
direction to the sun: thicker and thinner air line up into slanted
columns, which the camera sees as streaks, and which the dust in them
catches. The noise drifts slowly with a wind, so the streaks move a
little, as dust does.

The phase function is isotropic. Fog that scatters forward (the usual
choice for god rays seen against the sun) would send the sunlight down,
away from a camera that looks down with the sun behind it, and the fog
would only darken the ground.

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

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# About the atmosphere

*Explanation. Why the temple's air is dust on desktop and light-shaft
cards on mobile and web, and why it has no volumetric fog. The
parameters are in
[Board themes: Atmosphere](../reference/board-themes.md#atmosphere).*

## What it has to do

The [visual target](../reference/visual-target.md) asks for "shafts of sun
through dusty air" beside the temple, and readability rule 2 asks for zero
fog, haze or dust over the board. The first lighting pass (WP-47) found
that even height fog starting below the platform left a haze over the
board on Forward+, so the temple has no environment fog at all
([About lighting](about-lighting.md)). Whatever adds air has to add it
only where the board is not.

## Why there is no volumetric fog

WP-51 first gave the desktop tiers one `FogVolume` over the whole scene,
with a fog shader that kept its density at zero over the board and
stretched its noise along the sun into shafts. It stayed off the board,
but from the game camera, which looks almost straight down, it did not
read as fog: it showed as a flat tinted wash over the ground around the
platform rather than air with depth. It was taken out, with the
environment's volumetric fog settings and its material and shader. A
theme can bring volumetric fog back; the quality tiers still switch it
per tier ([Quality tiers](../reference/quality-tiers.md)).

## Dust

The motes are 320 tiny camera-facing particles in a ring around the
platform. They are lit and additive, so a mote shows only where the sun
or a brazier reaches it and disappears in shadow, which is what makes dust
in a sunbeam read. They exist only on the desktop tiers.

## Cards on mobile and web

The cards are twelve soft additive quads along the sun's direction,
beside the platform: twelve draw calls, which the web build's draw-call
budget has room for. They are plain meshes with no depth-texture soft
edge (they fade toward the ground in their own UVs instead): the cards
reading the depth texture made ANGLE's Metal backend, which runs the
Compatibility renderer on macOS, abort on the virtual Mac GPU the release
smoke test runs on. The generator checks every point of every card
against the game camera at the distances it uses from a wide window to a
tall phone, and places them again until none draws over the board.

## Cost

The dust is one particle system of 320 quads and one draw call; the
cards are twelve draw calls on mobile and web. The cost measured for
WP-51 on the reference Mac included the fog volume, so it is an upper
bound for what is left
([Benchmark results](../reference/benchmark-results.md#wp-51-atmosphere)).

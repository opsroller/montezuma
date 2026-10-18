<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# About the fire

*Explanation. Why the brazier fire is a simulated flipbook on particles,
why the simulation is ours rather than Blender's, and how it stays out of
the way of the board. The parameters are in [Fire](../reference/fire.md).*

## From a cone to a fire

The first temple pass lit each brazier with three twisted cones in an
additive shader that wobbled them. From the game camera that read as
"there is a fire there", but close up it was a cartoon: smooth, symmetric,
the same shape every second. The [visual target](../reference/visual-target.md)
asks for flame, smoke and embers that look real and light the stone
around them.

Real flame is a thin sheet of burning gas that rises, breaks into tongues
and fades as it cools. Modelling that live in a game is far too expensive,
so games film it: a fluid simulation is rendered offline into a grid of
frames (a flipbook) and played back on camera-facing quads. That is what
the fire does.

## Why our own simulation

The work package named Blender's fluid solver, Mantaflow. It could not
run where our assets are built. The `bpy` wheels on PyPI (5.1, 5.2) ship a
Mantaflow module whose classes have lost their base classes, so the first
grid operation of any gas simulation fails; Debian's Blender 4.0 cannot
register the module at all; and Blender's own builds are not reachable
from the build machines. Rather than ship a flame nobody can rebuild,
`tools/assets/fx/gen_fire.py` runs the same physical model in numpy:
fuel that burns when hot, heat that lifts the gas, soot that darkens it,
vorticity confinement for the curling detail coarse grids lose, a
pressure projection that keeps the gas incompressible, and MacCormack
advection so tongues stay sharp instead of smearing. It needs nothing
beyond the numpy and Pillow the asset pipeline already pins, so CI
builds it the way it builds the textures, and it is entirely our own
work under our licence.

The simulation is small (64 × 128 × 64 cells) because each tile is only
128 × 256 pixels; more cells would cost minutes in every uncached build
and add detail the tile cannot show.

## Motion vectors

Sixty-four frames at 30 fps is two seconds of fire, and phones get only
sixteen. Played as they are, sixteen frames per two seconds stutter.
Blending neighbouring frames hides the stutter but ghosts: a tongue
fades out in one place while it fades in a little higher.

The baker also writes, for every frame, where its content moves by the
next frame, taken from the simulated velocity weighted by what is
visible. The shader pushes the current frame forward along that motion
and pulls the next frame back along its own, then cross-fades. The
tongue slides instead of ghosting, so sixteen frames play nearly as
smoothly as sixty-four, and sixty-four look like footage.

## Colour from temperature

The flame sheet stores how much light each pixel emits and how hot the gas
there is, not a colour. The shader turns temperature into colour with the
blackbody curve, the colour a hot body glows at that temperature, so the
core goes yellow-white and the tips deep orange-red the way soot in a real
flame does, and hotter parts are brighter. Artists move the kelvin range
and brightness instead of painting a gradient, and the result stays
physically plausible.

## Seen from above

The game camera looks almost straight down. A flat flame card standing
upright would vanish edge-on; a card turned fully to the camera would lie
flat along the screen. The fire's quads face the camera but take their up
direction from world up as the camera sees it, so flames always rise out
of the bowl toward the top of the screen and slightly outward, as
vertical things do in perspective. Seen from above they shorten by
`foreshorten`, part way to what a real vertical sheet would do, which
keeps them full enough to read as fire at the board's distance.

Several flame quads overlap at different sizes, spins and points in the
loop, and each fades in and out over its life, so the fire never repeats
visibly and no quad pops.

## Light that moves

A flame's light pulses and its shadows shift. `FlickerLight3D` is an omni
light whose energy follows smooth noise around the energy the scene
gave it (mostly 1 to 3 Hz swells with faster crackle) and which drifts a
few centimetres so its shadows sway. The noise is seeded by the light's
position, so the four braziers never pulse together. It is C++ because the
project keeps game behaviour out of scripts, and its noise is a plain
function so it is unit tested.

The light keeps the groups the tier system uses: it casts shadows only
where the tier allows that many shadowed lights, and lights the volumetric
fog only on `desktop-high`.

## Smoke, embers and haze

Smoke uses the soot channel of the same simulation on larger, slower
particles. Unlike the flame it is lit, so the fire below warms its
underside and light shows through it from behind. Embers are tiny
additive particles thrown upward with turbulence; they cool from
yellow-white to dull red and burn out in the air above the bowl, so they
never need to collide with the temple. Heat haze redraws the screen above
the flame shifted by rising noise; it reads the screen texture, which is
why only the desktop tiers have it.

## Staying off the board

Readability beats realism. The braziers stand outside the board and
every part of the fire rises above them, never across the tiles. The
readability capture shows the same board contrast with and without the
fire. The fire is ambience: if a player notices it while matching
stones, it is too much.

## Not committed

The sheets are build outputs, like the board textures: the generator and
its parameters are the source, git ignores the PNGs, and CI caches them by
a hash of their inputs. Changing `fire.json` or the script re-runs the
simulation once, in about five minutes.

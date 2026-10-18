<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Tune the fire

Use this to change how the brazier fire looks: its shape and motion (the
simulation), its colour and brightness (the materials), or how much
smoke, embers and light it has (the scene). Every setting is listed in
[Fire](../reference/fire.md). Needs Python 3 with the pinned numpy and
Pillow ([Check the assets](build-and-test.md#check-the-assets)) and, to
look at it, Godot 4.7.2 with the extension built
([Build and test](build-and-test.md)).

## Change the flame's shape or motion

1. Edit `tools/assets/fx/fire.json`. Common changes:

   | To | Change |
   |---|---|
   | taller flames | lower `combustion.burn_rate` or raise `gas.buoyancy` |
   | a wider fire | raise `burner.radius` |
   | more curl and licking | raise `gas.vorticity` (above about 1 it turns stringy) |
   | calmer, rounder flames | lower `gas.vorticity`, raise `burner.noise_cells` |
   | more or less smoke | `combustion.smoke_yield` |
   | the flame filling more of its tile | shrink `render.crop` (keep it twice as tall as wide) |

2. Re-run the simulation (about five minutes):

   ```sh
   .venv/bin/python tools/assets/fx/gen_fire.py
   ```

   It prints the measured motion per sheet, for example
   `hi: motion 99.5th percentile 0.0270 tile UV per frame, encoded with scale 0.03`.

3. If a measured value is above its `motion_scale`, raise that sheet's
   `motion_scale` in `fire.json` to just above it, and set the same
   number as `motion_scale` of `MI_Fire` (base sheet) or its `hi`
   block (desktop sheet) in `tools/assets/themes/temple.json`. Re-run
   step 2 and then the scene generator (next section).

## Change colour, brightness or the particles

1. Edit the `MI_Fire`, `MI_FireSmoke`, `MI_Ember*` and `MI_HeatHaze`
   entries under `materials`, or the `fire` block, in
   `tools/assets/themes/temple.json`. Change both the base `params`
   and the `hi.params` of a material so the tiers match.
2. Regenerate the materials and scenes:

   ```sh
   .venv/bin/python tools/assets/gen_theme_scenes.py
   .venv/bin/python tools/assets/validate_assets.py
   ```

## Look at it

Render the close-up and the game camera on the desktop and web renderers,
without the menus (delete the `UiRoot=` line from a scratch copy of
`game/project.godot`, as in
[Capture the effects showcase](capture-the-effects-showcase.md)):

```sh
printf '[display]\nwindow/size/window_width_override=1280\nwindow/size/window_height_override=720\n' > game/override.cfg
xvfb-run -a godot --path game --rendering-driver vulkan --write-movie /tmp/fire/f.png \
  --fixed-fps 30 --quit-after 46 res://assets/lookdev/LD_Temple_Fire_Hi.tscn -- --quality=desktop-high
xvfb-run -a godot --path game --rendering-driver opengl3 --rendering-method gl_compatibility \
  --write-movie /tmp/fire-web/f.png --fixed-fps 30 --quit-after 46 res://assets/lookdev/LD_Temple_Fire.tscn -- --quality=web
rm game/override.cfg
```

Compare frame 46 with the [look-dev frames](../reference/fire.md#look-dev-frames).
Check the board from the game camera (`LD_Temple_Hi.tscn`) too: nothing of
the fire may cross the tiles.

## Check readability and cost

Run the benchmark with the readability capture for a tier
([Run the benchmark](run-the-benchmark.md)) and compare it with a run on
`main`: the fire must not change the rule 2, 4 and 6 numbers.

## Commit

Commit `fire.json`, `temple.json` and the regenerated materials and scenes;
the sheets are build outputs and stay out of git. Update the
[look-dev frames](../reference/fire.md#look-dev-frames) when the look
changed.

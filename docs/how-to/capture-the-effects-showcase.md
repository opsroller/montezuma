<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# Capture the effects showcase

How to record every totem and power-up effect to PNG frames, to judge a
change to the effects or an `fx_*` theme scene. Needs the extension built
([Build and test](build-and-test.md)) and `xvfb-run` on a machine without
a display.

`game/scenes/board/effects/effects_showcase.tscn` plays `e1-l01` and,
one after another, fires each totem (fire, time, gold, harvest, ward,
gift, paint) and sets off each power-up (tnt, color_clear, lightning,
clock, multiplier, hint), with a short pause between them, forever.

1. Hide the menus, which the `UiRoot` autoload draws over the board:
   work in a scratch copy of `game/` and delete the `UiRoot=` line from
   its `project.godot`.

   ```sh
   cp -r game /tmp/fx-game
   sed -i '/^UiRoot=/d' /tmp/fx-game/project.godot
   ```

2. Set the window size (Movie Maker ignores `--resolution` since Godot
   4.7):

   ```sh
   printf '[display]\nwindow/size/window_width_override=1280\nwindow/size/window_height_override=720\n' > /tmp/fx-game/override.cfg
   ```

3. Record 22 seconds at 15 frames per second, which covers all 13
   effects once:

   ```sh
   mkdir -p /tmp/fx-shots
   xvfb-run -a -s "-screen 0 1600x1600x24" godot --path /tmp/fx-game --rendering-driver opengl3 \
     res://scenes/board/effects/effects_showcase.tscn \
     --write-movie /tmp/fx-shots/f.png --fixed-fps 15 --quit-after 330
   ```

   `--rendering-driver opengl3` renders as the `web` tier does. For the
   desktop look, drop it and install a Vulkan driver first (on Debian and
   Ubuntu, `mesa-vulkan-drivers`).

Frames land as `/tmp/fx-shots/fNNNNNNNN.png`. Check each effect against
the [readability rules](../reference/visual-target.md#readability-rules-every-tier):
pieces still read under every flash, and nothing stays on a cell after
its step.

What each effect shows: [Board view, Effects](../reference/board-view.md#effects).
Why effects are built this way: [About the board view](../explanation/about-the-board-view.md#effects-ride-on-events-markers-ride-on-pieces).

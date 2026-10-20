<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Tune rendering live

Use this to try lighting and effect settings on the running board and see
what each one costs in frame time, before changing a theme or a tier.
Every control and readout line is listed in
[Render tuning screen](../reference/render-tuner.md). Needs the extension
built, the theme textures generated and the lightmaps baked
([Build and test](build-and-test.md)), on a machine with a GPU.

## Open the screen

1. Build the extension in Release and import the project once, as in
   [Run the benchmark](run-the-benchmark.md#on-a-desktop-windows-macos-linux-steam-deck).
2. Start the screen. Add `--gpu-profile` to see the cost of each GPU pass:

   ```sh
   godot --path game --gpu-profile res://scenes/bench/tuner.tscn
   ```

   The board demo plays the benchmark replay in a loop. The panel on the
   right shows the frame rate, frame, GPU and CPU times, draw calls and
   video memory, a graph of the last 600 frames and, with
   `--gpu-profile`, the slowest GPU passes.

## See what a setting costs

1. Under **Display and frame pacing**, set **V-Sync** to Off or Mailbox,
   so the frame time is the work, not the wait for the display.
2. Note the GPU line, change one setting, and read the GPU line again
   once the graph settles.
3. To compare an effect on and off, use its row under **Quality tier and
   effects** (Off, On, or Tier to follow the tier).
4. Press F1 to hide the panel and see the scene on its own.

## Keep what you found

The screen saves nothing. Put the values you settled on where they live:

| What you changed | Where it goes |
|---|---|
| Environment, camera attributes, lights, shader parameters | the theme: `tools/assets/themes/temple.json`, then `python3 tools/assets/gen_theme_scenes.py` ([Make a board theme](make-a-board-theme.md)) |
| A feature, shadow size or renderer setting for a tier | the tier table in `extension/src/render/quality.cpp` and [Quality tiers](../reference/quality-tiers.md) |
| A frame-time claim | a run of [the benchmark](run-the-benchmark.md), recorded in [Benchmark results](../reference/benchmark-results.md) |

Press F12 to save a screenshot of what you tuned to `user://tuner/`.

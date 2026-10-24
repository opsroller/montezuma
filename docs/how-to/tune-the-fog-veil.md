<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Tune the fog veil

Use this to make the thin mist over the board thicker, thinner, larger or
sparser and check that it still passes readability rule 2 (see-through,
never the whole board). Every parameter is listed in
[Board themes: Atmosphere](../reference/board-themes.md#atmosphere). Needs
Python 3 with the pinned Pillow
([Check the assets](build-and-test.md#check-the-assets)) and Godot 4.8 beta 1
with the extension built and the lightmaps baked
([Build and test](build-and-test.md)), on a machine with a GPU.

## Change the veil

1. Edit the `veil_*` values under `materials.MI_AtmosphereFog.params` in
   `tools/assets/themes/temple.json`:

   | To | Change |
   |---|---|
   | a thicker or thinner veil | raise or lower `veil_density` (0 to 2) |
   | mist that rises higher round the pieces | raise `veil_height` (at most 0.4 m, under the score labels) |
   | larger or smaller patches | raise or lower `veil_scale`; keep `wind` × 3600 a whole number of `veil_scale` tiles (with the temple's wind: 2, 3, 4, 6, 9, 12 or 18 m) |
   | more clear cells between patches | raise both values of `veil_coverage` |

2. Regenerate the material and scene:

   ```sh
   python3 tools/assets/gen_theme_scenes.py
   ```

3. Look at it in play, full screen on the screen you care about (the
   game's full-screen setting, or the `override.cfg` below for 4K):

   ```sh
   godot --path game -- --start-level=e1-l01
   ```

   The patches drift; watch for a minute rather than judging one frame.

## Check rule 2

The veil needs Forward+'s volumetric fog; a coarser fog grid
(`rendering/environment/volumetric_fog/volume_size` in a temporary
`game/override.cfg`) smears patches sideways and runs out of clear cells
first, so check the grid you ship. Run the readability capture and
checks:

```sh
cmake -DREADABILITY=ON -DOUT=bench-out/high -P tools/bench/run_bench.cmake
```

These capture at 540×1170. To check at 4K as well, put a landscape 4K
full-screen window in `game/override.cfg` (it is not committed; move any
you have aside first), capture the readability scene and run the check
on it:

```sh
printf '[display]\nwindow/size/viewport_width=3840\nwindow/size/viewport_height=2160\nwindow/size/mode=3\n' > game/override.cfg
godot --path game res://scenes/bench/readability.tscn -- --capture-out=bench-out/high-4k
python3 tools/bench/check_readability.py bench-out/high-4k
rm game/override.cfg
```

A `--resolution` flag or a window-size override alone gives a portrait
capture on a desktop, which is not what the 4K figures measure.

Each prints a line like
`PASS rule 2: worst cell contrast loss 23% (max 40%), clear cells 34% (min 25%)`.
If the worst cell loses too much, lower `veil_density`. If too few cells
stay clear, raise `veil_coverage`. Then repeat from step 2. Rule 4 is in
the same output. At 540×1170 it passes; at 4K it fails on `main` too
(glow and exposure raise one kind's contrast), and the veil lowers it a
little, so the 4K check exits 1 and that is expected. Compare its rule 4
figure with the recorded one for that tier and size in
[Benchmark results](../reference/benchmark-results.md#fog-veil-over-the-board):
new values must not make it worse.

Record the new values and figures in
[Benchmark results](../reference/benchmark-results.md#fog-veil-over-the-board)
in the same change.

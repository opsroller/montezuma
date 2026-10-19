<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Tune the fog veil

Use this to make the thin mist over the board thicker, thinner, larger or
sparser and check that it still passes readability rule 2 (see-through,
never the whole board). Every parameter is listed in
[Board themes: Atmosphere](../reference/board-themes.md#atmosphere). Needs
Python 3 with the pinned Pillow
([Check the assets](build-and-test.md#check-the-assets)) and Godot 4.7.2
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

3. Look at it in play at the size you care about, for example a level
   full screen at 4K:

   ```sh
   godot --path game --resolution 3840x2160 -- --start-level=e1-l01 --quality=desktop-high
   godot --path game --resolution 3840x2160 -- --start-level=e1-l01 --quality=desktop-standard
   ```

   The patches drift; watch for a minute rather than judging one frame.

## Check rule 2 on both fog tiers

The veil shows only on `desktop-high` and `desktop-standard`, and
`desktop-standard`'s coarser fog grid smears patches sideways, so it is
the tier that runs out of clear cells first. Run the readability capture
and checks on both:

```sh
cmake -DTIER=desktop-high -DREADABILITY=ON -DOUT=bench-out/high -P tools/bench/run_bench.cmake
cmake -DTIER=desktop-standard -DREADABILITY=ON -DOUT=bench-out/standard -P tools/bench/run_bench.cmake
```

These capture at 540×1170. To check at 4K as well, capture the
readability scene at that size and run the check on it, for each tier:

```sh
godot --path game --resolution 3840x2160 res://scenes/bench/readability.tscn -- --capture-out=bench-out/high-4k --quality=desktop-high
python3 tools/bench/check_readability.py bench-out/high-4k
```

Each prints a line like
`PASS rule 2: worst cell contrast loss 23% (max 40%), clear cells 34% (min 25%)`.
If the worst cell loses too much, lower `veil_density`. If too few cells
stay clear, raise `veil_coverage`. Then repeat from step 2. Rule 4 is in
the same output: a veil that passes rule 2 rarely moves it, but check it
did not get worse.

Record the new values and figures in
[Benchmark results](../reference/benchmark-results.md#fog-veil-over-the-board)
in the same change.

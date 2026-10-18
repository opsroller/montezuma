<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Run the benchmark

How to measure frame time and check readability on a desktop or a phone.
What the benchmark replays and every field it writes:
[Benchmark and readability capture](../reference/benchmark.md).

Use a release-like build of the extension, close other heavy programs, and
plug in laptops and phones. Numbers from a debug build, the editor window
or a cloud machine without a GPU do not count against the
[tier budgets](../reference/visual-target.md#quality-tiers).

## On a desktop (Windows, macOS, Linux, Steam Deck)

1. Build the extension in Release, as in [Build and test](build-and-test.md),
   and import the project once:

   ```sh
   cmake -S . -B build-ext -DCMAKE_BUILD_TYPE=Release -DMONTEZUMA_BUILD_EXTENSION=ON
   cmake --build build-ext -j
   godot --headless --path game --import
   ```

2. Run the benchmark and the readability capture for the tier the machine
   stands for (on Windows, use Git Bash or WSL for the script):

   ```sh
   tools/bench/run_bench.sh --tier desktop-standard --readability --out bench-out
   ```

   Add `--driver vulkan`, `metal`, `d3d12` or `opengl3` to pick the
   rendering driver, `--size 2560x1440` for `desktop-high`, and `--godot
   PATH` when Godot is not on `PATH`. The run takes about 70 s plus two
   short windows for the captures. Do not touch the window while it runs.

3. Read the results in `bench-out/`:

   - `benchmark.json`: `cpu_ms.p95` and `gpu_ms.p95` against the tier's
     budget, `over_budget` frames, and `replay.diverged`, which must be
     false.
   - `features.txt`: one line per feature row of the tier, `FAIL` where
     the scene does not match the target.
   - `readability.txt`: one line per rule; open the PNGs in
     `bench-out/readability/` to see what failed.

   The script exits 1 if any run or check failed.

To run the benchmark alone, without the script:

```sh
godot --path game res://scenes/bench/bench.tscn -- --bench-tier=desktop-high --bench-out=/tmp/bench.json
```

## On an Android or iOS phone

Phones take no command-line arguments, so the benchmark scene becomes the
main scene of a local debug export. Do not commit these changes.

1. In `game/scenes/bench/bench.tscn`, set the `BenchmarkRunner`'s `tier` to
   `mobile` (and change `measure_seconds` if needed). In `game/project.godot`,
   set `run/main_scene="res://scenes/bench/bench.tscn"` under
   `[application]`.
2. Build the extension for the phone and export a debug build. The
   project has no Android or iOS preset until WP-36 and WP-37; until then,
   add one locally in the editor (Project > Export > Add) and do not commit
   it.
3. Install and start it on the phone, plugged in, screen on.
4. Read the report from the log. It is printed on one line starting with
   `BENCH_JSON `:

   ```sh
   adb logcat -s godot | grep BENCH_JSON          # Android
   ```

   On iOS, read the same line from the device console in Xcode
   (Window > Devices and Simulators > Open Console). The app quits when
   the capture ends.
5. Put `bench.tscn` and `project.godot` back (`git checkout -- game/`).

For the 10-minute thermal check, set `measure_seconds` to 600 and look at
`frame_ms` over time on the phone's own performance overlay, or split the
run into ten 60 s runs and compare their p95.

The phone steps follow Godot's export and logging behaviour; they have not
yet been run on a phone in this project. WP-41 (performance pass) is the
first to use them.

## In CI

The `tests` workflow runs the benchmark headless for 3 s on every PR to
check that the replay plans, plays without drifting and writes its report
(see [CI pipeline](../reference/ci-pipeline.md)). It measures nothing.

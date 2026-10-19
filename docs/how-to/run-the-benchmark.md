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
   cmake -S . -B build-ext -G Ninja -DCMAKE_BUILD_TYPE=Release -DMONTEZUMA_BUILD_EXTENSION=ON
   cmake --build build-ext
   godot --headless --path game --import
   ```

   On a Mac, add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
   -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15` to the configure line: the game
   only loads a universal library, and without it Godot reports `Can't
   open dynamic library` and every game class is missing. The Godot app's
   binary is `/Applications/Godot.app/Contents/MacOS/Godot`.

2. Run the benchmark and the readability capture for the tier the machine
   stands for (the same command on Windows, macOS and Linux):

   ```sh
   cmake -DTIER=desktop-standard -DREADABILITY=ON -DOUT=bench-out -P tools/bench/run_bench.cmake
   ```

   Add `-DDRIVER=vulkan`, `metal`, `d3d12` or `opengl3` to pick the
   rendering driver, `-DSIZE=2560x1440` for `desktop-high`, and
   `-DGODOT=PATH` when Godot is not on `PATH` (every `-D` goes before
   `-P`). The runner locks the render at
   `SIZE` (default 1920x1080) and fails the run if any frame rendered
   at another size. The checks need Python 3; the
   readability check also needs Pillow, installed as in
   [Check the assets](build-and-test.md#check-the-assets) (run the command
   with that venv active). The run takes about 70 s plus two
   short windows for the captures. Do not touch the window while it runs.

   On a display faster than 60 Hz, or to see what a frame costs rather
   than when the display shows it, run again with `-DVSYNC=off`. Godot
   4.7.2's Metal driver has been seen to stay display-synced even then
   ([Benchmark results](../reference/benchmark-results.md#frame-time));
   `display.vsync` in the report says what was asked for, not what the
   driver did.

3. Read the results in `bench-out/`:

   - `run.txt`: the draw calls against the tier's cap (a count, so a
     cloud machine's software renderer measures it too), and the budget verdict (`frame_ms` percentiles against whole
     60 Hz periods, with its reason when it fails), the render size and
     the replay, one PASS or FAIL line each. A vsync-locked 60 fps run
     passes. NOTE lines say when the driver gives no GPU timing.
   - `benchmark.json`: the full report; `budget`, `resolution` and
     `timing` explain the verdict. See the
     notes in [Benchmark results](../reference/benchmark-results.md#frame-time).
   - `features.txt`: one line per feature row of the tier, `FAIL` where
     the scene does not match the target.
   - `readability.txt`: one line per rule; open the PNGs in
     `bench-out/readability/` to see what failed.

   The script exits 1 if any run or check failed.

   On macOS with Metal, `gpu_ms` is null (`timing.gpu` false): judge
   by `frame_ms`. `render_cpu_ms` there includes the wait for the next
   drawable, so with vsync on `cpu_ms` fills the frame whatever the CPU
   does; read `process_ms` for game logic, and try `-DVSYNC=off` to see
   the work without that wait (see the note above on Metal).

4. Add the numbers to [Benchmark results](../reference/benchmark-results.md):
   the machine once, then a row per tier in each table.

To run the benchmark alone, without the script:

```sh
godot --path game res://scenes/bench/bench.tscn -- --quality=desktop-high --bench-size=2560x1440 --bench-out=/tmp/bench.json
python3 tools/bench/check_run.py /tmp/bench.json --size 2560x1440
```

### Check what one effect or edit costs, in seconds

Run the way players see the game: full screen, at the screen's own
resolution, with no render-scale cut or upscaler. The script does that
by default (`NATIVE`, `--quality-native`); `-DFULLSCREEN=ON` makes the
window exclusive full screen at the screen's size and ignores `SIZE`.
For a quick look at where GPU time goes, or what a small change did,
run a short capture with Godot's per-pass profile instead of the full
minute:

```sh
cmake -DTIER=desktop-high -DFULLSCREEN=ON -DVSYNC=off -DSECONDS=6 -DWARMUP=3 -DGPU_PROFILE=ON -P tools/bench/run_bench.cmake
```

The run takes about 15 s. Each `GPU PROFILE` block it prints gives the
GPU time of every render pass in one frame (depth pre-pass, opaque pass,
SSAO, glow, TAA and so on), and `gpu_ms` in `benchmark.json` gives the
whole frame. Add `-DQUALITY_OFF=glow` (any
[feature ids](../reference/quality-tiers.md#feature-ids), comma-separated)
and compare `gpu_ms` p50 with a run without it to get that effect's cost;
the feature check then fails for those rows, which is expected. Run the
two back to back: run-to-run spread is a few tenths of a millisecond.

Compare `gpu_ms` with vsync off: with vsync on the GPU clocks down
between frames. GNOME keeps vsync on when Godot asks for it off; put
`display/window/vsync/vsync_mode=3` (mailbox) under `[display]` in
`game/override.cfg` for the run, and remove it afterwards.

On Metal, Godot 4.7.2 returns no GPU timestamps, so the profile prints
zeros there: judge by `frame_ms` or use Apple's Metal tools.

`render.pipeline_compilations.during_capture` in the report counts
shaders compiled inside the timed window; `draw` above 0 means frames
stalled on a compile.

To check that a change leaves the picture alone, capture the same frame
before and after (`godot --write-movie <dir>/f.png --fixed-fps 30
--quit-after 82` on the benchmark scene) and compare the images pixel by
pixel against two captures of one build, which differ a little on their
own.

## On an Android or iOS phone

Phones take no command-line arguments, so the benchmark scene becomes the
main scene of a local debug export. Do not commit these changes.

1. In `game/scenes/bench/bench.tscn`, change the `BenchmarkRunner`'s
   `measure_seconds` if needed (phones run the `mobile` tier on their own).
   In `game/project.godot`,
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
check that the replay plans, plays without drifting, holds its locked
render size (`check_run.py --no-budget`) and writes its report
(see [CI pipeline](../reference/ci-pipeline.md)). It measures nothing.

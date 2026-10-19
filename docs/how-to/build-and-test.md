# Build and test

How to build the core and the extension, run the tests and load the game,
the same way CI does. Run from the repository root.

## Get the sources

```sh
git clone https://github.com/opsroller/montezuma
cd montezuma
git submodule update --init --recursive    # godot-cpp in thirdparty/
```

Configure with Ninja (`-G Ninja`, as below) and build without `-j`: Ninja
already runs one job per core plus two. Do not pass a bare `-j` to a Make
build: Make then starts every compile at once, and godot-cpp alone has about
a thousand, which runs a 16 GB machine out of memory with the compilers
stalled in swap. To build one thing, name it with `--target`.

CMake will not switch an existing build folder to another generator. If a
folder was configured with Make (the old default), delete it before
configuring it with Ninja, for example:

```sh
cmake -E rm -rf build-ext
```

## Compile godot-cpp only once

godot-cpp is most of the build: about a thousand files. Build through
[ccache](https://ccache.dev) so it is compiled once per godot-cpp version
and compiler, and every later build folder, clean rebuild or worktree
reuses the cached objects (a fresh extension build folder then takes
seconds, not minutes). Install ccache and set these before configuring:

```sh
export CMAKE_C_COMPILER_LAUNCHER=ccache
export CMAKE_CXX_COMPILER_LAUNCHER=ccache
export CCACHE_BASEDIR="$HOME/src"   # a folder that holds all your checkouts
```

CMake reads the two launcher variables when it configures a new build
folder, so an existing folder keeps compiling without the cache until it
is configured again from scratch. `CCACHE_BASEDIR` makes paths under it
hash relative, so two checkouts in different folders share the cache.
`ccache --show-stats` shows the hits. CI does the same (see
[About CI caching](../explanation/about-ci-caching.md)).

## Build and test the core only

Needs CMake 3.22+, Ninja and a C++20 compiler. No Godot needed.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMONTEZUMA_WERROR=ON -DMONTEZUMA_BUILD_EXTENSION=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

This also builds the balance simulator (`tools/sim/`) and runs its tests;
add `-DMONTEZUMA_BUILD_SIM=OFF` to skip it. Running it is in
[Run the balance simulator](run-the-balance-simulator.md).

## Run the tests under sanitizers

```sh
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMONTEZUMA_WERROR=ON -DMONTEZUMA_BUILD_EXTENSION=OFF \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

## Build the extension and load the game

Needs the godot-cpp submodule and Godot 4.7.2 (an older Godot refuses to load the extension). The extension is built
from the root project, never from `extension/` on its own, and in its own
build folder: CMake caches options, so reusing the core-only `build/`
would keep the extension off. The library lands in `game/bin/`.

```sh
cmake -S . -B build-ext -G Ninja -DCMAKE_BUILD_TYPE=Release -DMONTEZUMA_BUILD_EXTENSION=ON
cmake --build build-ext
ctest --test-dir build-ext --output-on-failure   # every test, as CI's extension job
godot --headless --path game --import
godot --headless --path game --quit-after 120    # errors show here
```

On macOS, build the universal (arm64 and x86_64) library: the game's
`.gdextension` only lists `libmontezuma.macos.*.universal.dylib`, so a
single-architecture build does not load. Configure with:

```sh
cmake -S . -B build-ext -G Ninja -DCMAKE_BUILD_TYPE=Release -DMONTEZUMA_BUILD_EXTENSION=ON \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
```

This build also copies `CREDITS.md` and `LICENSE` into `game/` so the
credits screen shows them. The tests include `ui_model_tests`, which
needs the extension build.

Play the intro level to its end headless, as CI does:

```sh
godot --headless --path game --quit-after 120 -- --game-selftest
```

It ends with `Match3Game self-test: <n> moves, outcome 1, <n> cues, 0 failures`.

Check the web demo's scene, which must be won in play and tell the page
once ([IntroLevel](../reference/intro-level.md#self-test)):

```sh
godot --headless --path game --quit-after 120 res://scenes/demo/intro_level.tscn -- --intro-selftest
```

It ends with `IntroLevel self-test: <n> moves, outcome 1, 1 completions, 0 failures`.

Quit from inside a level, with music playing, as CI does; nothing may
print an error or a leak at exit:

```sh
godot --headless --path game --quit-after 600 -- --start-level=e1-l01 --quit-after-frames=60
```

## Build with ray-traced reflections

The optional ray-traced reflections on the pieces
([Ray-traced reflections](../reference/ray-traced-reflections.md)) need
a Vulkan GPU with ray-tracing pipelines (on Linux or Windows) and a
window; they do not run headless or on macOS.

1. Configure the extension with the option on, and build:

   ```sh
   cmake -S . -B build-ext -G Ninja -DCMAKE_BUILD_TYPE=Release -DMONTEZUMA_BUILD_EXTENSION=ON -DMONTEZUMA_RAY_TRACING=ON
   cmake --build build-ext
   ctest --test-dir build-ext --output-on-failure
   ```

2. Run the self-test. It prints six `ok` lines and
   `RtReflections self-test passed`:

   ```sh
   godot --path game -- --start-level=e1-l01 --rt-selftest
   ```

3. Play with the effect on, or see what it traces:

   ```sh
   godot --path game -- --start-level=e1-l01 --rt-reflections
   godot --path game -- --start-level=e1-l01 --rt-debug
   ```

   `--rt-debug` paints pieces it traced from blue, and reflections that
   hit something green (shown on screen) or red (hidden).

4. Measure it against the same run without it, as in
   [Run the benchmark](run-the-benchmark.md#check-what-one-effect-or-edit-costs-with-a-short-run), with
   `-DGAME_ARGS=--rt-reflections`.

Configure with `-DMONTEZUMA_RAY_TRACING=OFF` (the default) to build
without it again; the flags are then ignored.

## Build and test on Windows

Needs Visual Studio 2022 with the C++ workload, CMake 3.22+ and Ninja
(both come with Visual Studio). From an **x64 Native Tools Command Prompt**,
so MSVC is on the path:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMONTEZUMA_WERROR=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

This is what the `windows` workflow runs on PRs that change the build
system. Every target links the static MSVC runtime, as godot-cpp does;
the root `CMakeLists.txt` sets it before the first target.

## Check the assets

Needs Python 3, numpy and Pillow. This is the `assets` CI job. The board
theme textures are generated, not committed, so generate them first; the
same step makes them for the Godot editor. A CMake build does it too
(target `textures`) when it finds numpy and Pillow.

```sh
python3 -m venv .venv
.venv/bin/pip install --require-hashes -r .github/requirements/assets.txt
.venv/bin/python tools/assets/gen_theme_textures.py # skips themes whose inputs did not change
.venv/bin/python tools/assets/validate_assets.py    # exits 1 on any error
```

The hashes in `assets.txt` cover every platform numpy and Pillow ship wheels for,
so the same commands work on Linux, macOS and Windows. On Windows the
venv's programs are in `.venv\Scripts\`.

## Bake the lightmaps

Needs the native extension built, the textures generated and the Godot
editor from `tools/release/godot.env`, with a Vulkan driver. Without a GPU,
install Mesa's lavapipe and a virtual display (`sudo apt install
mesa-vulkan-drivers xvfb` on Debian or Ubuntu) and add `-DXVFB=ON`.

```sh
cmake -DGODOT="$(command -v godot)" -DXVFB=ON -P tools/assets/bake_lightmaps.cmake
```

It skips themes whose bake inputs did not change, imports the project
before a bake, and prints
`LIGHTMAP_BAKE ok res://assets/lookdev/LD_<Theme>_Bake.tscn …` for each
bake. If the editor starts an import of its own during a bake, the bake
fails, prints `LIGHTMAP_BAKE retry …` and runs once more. Without a bake
the game still runs, without baked light. Details:
[Board themes: Lighting](../reference/board-themes.md#lighting).

## Capture screenshots without a display

Godot's Movie Maker renders frames to PNG files. On a machine without a
screen, run it under a virtual X display with the OpenGL driver (needs
`xvfb-run`):

Since Godot 4.7, Movie Maker ignores `--resolution`, so set the window
size with a local `game/override.cfg` (ignored by git), then delete it:

```sh
printf '[display]\nwindow/size/window_width_override=540\nwindow/size/window_height_override=1170\n' > game/override.cfg
xvfb-run -a -s "-screen 0 1600x1600x24" godot --path game --rendering-driver opengl3 \
  --write-movie /tmp/shots/frame.png --fixed-fps 30 --quit-after 20
rm game/override.cfg
```

Each frame lands as `/tmp/shots/frameNNNNNNNN.png`. Change the two sizes
to check a phone (`540x1170`) and a desktop (`960x540` or `1280x720`)
framing. The menu overlay (the `UiRoot` autoload) draws over the board;
add `-- --start-level=e1-l01` to go straight into a level, or
`-- --start-screen=settings` to open on another screen.

To screenshot a packaged build the way CI does (title screen and the
first level at 1280×720, saved as `title.png` and `board.png`):

```sh
xvfb-run -a -s "-screen 0 1280x720x24" \
  cmake -DARCHIVE=dist/linux -DSCREENS=/tmp/screens -P tools/release/smoke-test.cmake
```

## Export a build

See [Export a desktop build](export-a-desktop-build.md). CI builds the
Windows, macOS and Linux packages on every push to `main`, and the macOS
and Linux packages on every push to any other branch.

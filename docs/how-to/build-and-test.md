# Build and test

How to build the core and the extension, run the tests and load the game,
the same way CI does. Run from the repository root.

## Get the sources

```sh
git clone https://github.com/opsroller/montezuma
cd montezuma
git submodule update --init --recursive    # godot-cpp in thirdparty/
```

## Build and test the core only

Needs CMake 3.22+ and a C++20 compiler. No Godot needed.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DMONTEZUMA_WERROR=ON -DMONTEZUMA_BUILD_EXTENSION=OFF
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Run the tests under sanitizers

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DMONTEZUMA_WERROR=ON -DMONTEZUMA_BUILD_EXTENSION=OFF \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

## Build the extension and load the game

Needs the godot-cpp submodule and Godot 4.4.1. The extension is built
from the root project, never from `extension/` on its own, and in its own
build folder: CMake caches options, so reusing the core-only `build/`
would keep the extension off. The library lands in `game/bin/`.

```sh
cmake -S . -B build-ext -DCMAKE_BUILD_TYPE=Release -DMONTEZUMA_BUILD_EXTENSION=ON
cmake --build build-ext -j
ctest --test-dir build-ext --output-on-failure   # every test, as CI's extension job
godot --headless --path game --import
godot --headless --path game --quit-after 120    # errors show here
```

This build also copies `CREDITS.md` and `LICENSE` into `game/` so the
credits screen shows them. The tests include `ui_model_tests`, which
needs the extension build.

## Capture screenshots without a display

Godot's Movie Maker renders frames to PNG files. On a machine without a
screen, run it under a virtual X display with the OpenGL driver (needs
`xvfb-run`):

```sh
xvfb-run -a -s "-screen 0 1600x1600x24" godot --path game --rendering-driver opengl3 \
  --resolution 540x1170 --write-movie /tmp/shots/frame.png --fixed-fps 30 --quit-after 20
```

Each frame lands as `/tmp/shots/frameNNNNNNNN.png`. Change `--resolution`
to check a phone (`540x1170`) and a desktop (`1280x720`) framing.

## Export a build

See [Export a desktop build](export-a-desktop-build.md). CI builds the
Windows, macOS and Linux packages on every push to `main`.

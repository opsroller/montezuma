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
cmake -S . -B build-ext -DCMAKE_BUILD_TYPE=Release -DMONTEZUMA_BUILD_EXTENSION=ON -DMONTEZUMA_BUILD_TESTS=OFF
cmake --build build-ext -j
godot --headless --path game --import
godot --headless --path game --quit-after 120    # errors show here
```

## Export a build

See [Export a desktop build](export-a-desktop-build.md). CI builds the
Windows, macOS and Linux packages on every push to `main`.

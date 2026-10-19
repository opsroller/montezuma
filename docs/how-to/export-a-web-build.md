<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# How to export the web demo

How to produce the same web package CI builds, on a Linux machine, and
check it in a browser. Run from the repository root. For what each file
is, see [the build pipeline reference](../reference/build-pipeline.md#web-demo).

## Get a CI build instead

Every push to `main` builds it. Open the latest **web** run in the
repository's Actions tab and download `montezuma-web` from its
artifacts, then check it:

```sh
sha256sum -c montezuma-*-web.zip.sha256
```

To build a branch, run the **web** workflow from the Actions tab with
**Run workflow** and pick the branch.

## Build it yourself

1. Install the pinned Emscripten (about 1 GB, reused once installed):

   ```sh
   cmake -DDIR=~/emsdk -P tools/release/web-fetch-emsdk.cmake
   ```

2. Build the extension for the web with Emscripten's toolchain file and
   config. Use its own build folder; CMake caches the toolchain:

   ```sh
   export EM_CONFIG=~/emsdk/.emscripten
   cmake -S . -B build/web -G Ninja -DCMAKE_BUILD_TYPE=Release \
     -DCMAKE_TOOLCHAIN_FILE=~/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
     -DMONTEZUMA_BUILD_EXTENSION=ON -DMONTEZUMA_BUILD_TESTS=OFF -DMONTEZUMA_WERROR=ON \
     -DGODOTCPP_TARGET=template_release -DGODOTCPP_THREADS=OFF
   cmake --build build/web -j 4
   ```

   The library lands in `game/bin/` as
   `libmontezuma.web.template_release.wasm32.nothreads.wasm`.

3. Fetch the pinned Godot and the web template:

   ```sh
   cmake -DDIR=~/godot-dl -DTEMPLATES=web_dlink_nothreads_release.zip -P tools/release/fetch-godot.cmake
   ```

4. Export and package:

   ```sh
   cmake -DGODOT=~/godot-dl/godot -DOUT=dist/web -P tools/release/web-export.cmake
   ```

   If the export fails, Godot's output is in `dist/web/logs/`.

5. Run it in headless Chromium the way the site loads it:

   ```sh
   python3 -m venv ~/web-smoke
   ~/web-smoke/bin/pip install --require-hashes -r tools/release/web-requirements.txt
   ~/web-smoke/bin/python -m playwright install chromium
   ~/web-smoke/bin/python tools/release/web-smoke.py dist/web/montezuma-*-web.zip
   ```

   The hashes cover CPython 3.12 and 3.13 on Linux x86_64. On another
   Python or platform, install `playwright==1.56.0` without
   `--require-hashes` instead.

   Add `firefox` or `webkit` after the archive to try those browsers
   (install them the same way).

## Play it

Unzip the package and serve the folder over HTTP; browsers will not load
WebAssembly from `file://`:

```sh
unzip dist/web/montezuma-*-web.zip -d dist/web/play
python3 -m http.server 8000 --directory dist/web/play/montezuma-*-web
```

Open <http://localhost:8000/montezuma.html>. To see it inside the site,
pass the unzipped folder to `site/build.py --demo-dir` (see
[How to build and preview the website](build-and-preview-the-website.md)).

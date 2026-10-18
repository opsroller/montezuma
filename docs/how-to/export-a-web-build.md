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

1. Install the pinned Emscripten (about 1 GB, reused once installed) and
   load it into the shell:

   ```sh
   source "$(tools/release/web-fetch-emsdk.sh ~/emsdk)"
   ```

2. Build the extension for the web. Use its own build folder; CMake
   caches the toolchain:

   ```sh
   emcmake cmake -S . -B build/web -G Ninja -DCMAKE_BUILD_TYPE=Release \
     -DMONTEZUMA_BUILD_EXTENSION=ON -DMONTEZUMA_BUILD_TESTS=OFF -DMONTEZUMA_WERROR=ON \
     -DGODOTCPP_TARGET=template_release -DGODOTCPP_THREADS=OFF
   cmake --build build/web -j
   ```

   The library lands in `game/bin/` as
   `libmontezuma.web.template_release.wasm32.nothreads.wasm`.

3. Fetch the pinned Godot and the web template:

   ```sh
   tools/release/fetch-godot.sh ~/godot-dl web_dlink_nothreads_release.zip
   ```

4. Export and package:

   ```sh
   GODOT=~/godot-dl/godot tools/release/web-export.sh "$(tools/release/version.sh)" dist/web
   ```

   If the export fails, Godot's output is in `dist/web/logs/`.

5. Run it in headless Chromium the way the site loads it:

   ```sh
   python3 -m pip install playwright==1.56.0
   python3 -m playwright install chromium
   python3 tools/release/web-smoke.py dist/web/montezuma-*-web.zip
   ```

   Add `firefox` or `webkit` after the archive to try those browsers
   (install them the same way).

   With Docker, run all three browsers in the same image CI uses, with
   nothing to install:

   ```sh
   docker run --rm -v "$PWD:/w" -w /w mcr.microsoft.com/playwright/python:v1.56.0-noble \
     python3 tools/release/web-smoke.py dist/web/montezuma-*-web.zip chromium firefox webkit
   ```

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

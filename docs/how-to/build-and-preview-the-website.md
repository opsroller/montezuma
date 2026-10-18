<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# How to build and preview the website

Build `site/` into a static folder, look at it in a browser and run its
tests. Run from the repository root. Needs Python 3.10+ and, for the
tests, Node 20+. Field-by-field facts are in the
[website reference](../reference/website.md); the reasons behind the
site's shape are in [About the website](../explanation/about-the-website.md).

## Build and preview

```sh
python3 site/build.py
python3 -m http.server 8000 --directory site/_site
```

Open <http://localhost:8000/>. Without a web export the page runs the stub
demo: "Try the demo preview", then "Finish the preview level", fires the
same `montezuma:level-completed` event the game will and opens the
download popup.

## Preview with the real web demo

Get a web package, from CI or built yourself (see
[How to export the web demo](export-a-web-build.md)), check it and unzip
it:

```sh
sha256sum -c montezuma-*-web.zip.sha256
unzip montezuma-*-web.zip -d web-export
```

Point the build at the unzipped folder, the one containing
`montezuma.js`:

```sh
python3 site/build.py --demo-dir web-export/montezuma-*-web
python3 -m http.server 8000 --directory site/_site
```

The build copies the export into `site/_site/play/` and the page loads it
when the visitor clicks "Play the intro level (N MB)". Open the page over
`http://`, not `file://`; browsers won't load WebAssembly from a file.
If the build stops with "the web export is missing …", the folder is not
a complete `web` package: get a fresh one rather than copying files in.

## Update the embedded web build

The site takes whatever package it is given, so a new game build needs
no site change. Repeat the steps above with the newer
`montezuma-<version>-web.zip`, click "Play the intro level" and wait for
"Intro level running". If the game starts without its board, or the
browser console shows `Can't open dynamic library`, the package and the
loader disagree on the extension's file name: compare it with the
[demo hand-off contract](../reference/website.md#demo-hand-off-contract).

## Check a deployed demo

On a deployed copy of the site, check that the host compresses the demo's
largest files (the play button's size assumes it) and lets browsers cache
them:

```sh
for f in montezuma.side.wasm montezuma.wasm montezuma.pck; do
  curl -sI -H 'Accept-Encoding: gzip' "https://<site>/play/$f" \
    | grep -iE '^(content-type|content-encoding|cache-control):'
done
```

Each `.wasm` shows `content-type: application/wasm`, each file
`content-encoding: gzip`, and `cache-control` is not `no-store`.

## Run the tests

```sh
python3 -m unittest discover site/tests
node --test site/tests/*.test.mjs
```

CTest runs the same two as `site_build_tests` and `site_js_tests`, so
`ctest --test-dir build` from [Build and test](build-and-test.md) and
every CI `tests` job include them.

## Add or change release data

Edit `site/data/releases.json` only to change its shape; the release
tooling writes the values. Give a released file its `url`, `size_bytes`
and `sha256` and set the top-level `version`; give a store listing its
`url`. Rebuild: the build stops with the field to fix if anything is
missing.

## Add a third-party notice

When the game or demo ships a new third-party component, add its license
text as `site/data/notices/<name>.txt` (copied exactly from the
component's release) and an entry in `site/data/notices.json` with its
name, https URL, license and file. Add it under "Third-party works" in
`CREDITS.md` too, then rebuild.

## Add a credit

Add a line under the right heading in `CREDITS.md` (list items, links as
`[text](https://…)`), then rebuild. The credits page and the in-game
credits screen both read that file.

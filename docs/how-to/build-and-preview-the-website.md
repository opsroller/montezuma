# How to build and preview the website

Build `site/` into a static folder, look at it in a browser and run its
tests. Run from the repository root. Needs Python 3.10+ and, for the
tests, Node 20+. Field-by-field facts are in the
[website reference](../reference/website.md).

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

Export the game for the web (see [How to export the web demo](export-a-web-build.md)), then point the build at
the export folder, the one containing `montezuma.js`:

```sh
python3 site/build.py --demo-dir path/to/web-export
python3 -m http.server 8000 --directory site/_site
```

The build copies the export into `site/_site/play/` and the page loads it
when the visitor clicks "Play the intro level". Open the page over
`http://`, not `file://`; browsers won't load WebAssembly from a file.

## Run the tests

```sh
python3 -m unittest discover site/tests
node --test site/tests/*.test.mjs
```

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

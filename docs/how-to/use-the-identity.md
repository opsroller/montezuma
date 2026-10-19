<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Use the identity

*How-to. Swap the game's and the site's placeholder tokens for the
identity's, vendor the two fonts, place the logo in a new layout, and
rebuild the 3D key art. The
rules and values: [Style guide](../../design/README.md). Why:
[About the visual identity](../explanation/about-the-visual-identity.md).*

You need Python 3. Regenerating previews also needs `chromium` on `PATH`.

## Regenerate and check the identity files

Do this after any change to `design/identity/build.py`, and before
copying tokens anywhere.

```sh
python3 design/identity/build.py          # SVGs and both token files
python3 design/identity/check_contrast.py # must print "all checks pass"
python3 design/identity/render.py         # PNG previews (downloads fonts to a cache)
```

`render.py` downloads Archivo and Atkinson Hyperlegible Next to
`~/.cache/montezuma/fonts` (or `$MONTEZUMA_FONT_CACHE`) and checks their
sha256; it never copies them into the repository. Offline, add
`--no-fonts`.

## Swap the game's tokens (ui-developer)

1. Copy the tokens file over the placeholder, keeping the identity's file
   as the source:

   ```sh
   cp design/tokens/montezuma.tokens.json game/ui/theme/montezuma.tokens.json
   ```

2. Point the UI at it: change `tokens_path_` in
   `extension/src/ui/ui_root.hpp` to
   `res://ui/theme/montezuma.tokens.json`, or overwrite
   `placeholder.tokens.json` with the new file if you would rather keep
   the path. Every key the placeholder had is present.
3. Check what changed: every colour key now holds the identity's dark
   role value (the style guide's
   [tokens table](../../design/README.md#tokens-and-how-they-map) maps
   each one), and `radius.control` is 4 px (was 8); panels use twice
   that. Font sizes, spacing, the focus width and motion are unchanged.
4. Build and run the extension self-test and look at the menus
   ([Build and test](build-and-test.md)); check them with
   [Check UI text and layout](check-ui-text-and-layout.md).
5. Update the "Theme tokens" section of `docs/reference/ui.md` to name the
   new file, in the same PR.

## Vendor the fonts (ui-developer and web developer)

1. Download the files listed in the style guide's
   [source and licence table](../../design/README.md#source-and-licence)
   from google/fonts at the pinned commit, including each `OFL.txt`.
   The two families have different `OFL.txt` files with the same name,
   so download each family into its own folder (`archivo/` and
   `atkinsonhyperlegiblenext/`, as in google/fonts).
2. Check each file's sha256 against the table:

   ```sh
   sha256sum archivo/* atkinsonhyperlegiblenext/*
   ```

3. Put each family in its own folder with its own `OFL.txt` beside it,
   never two families in one folder: for the game under
   `game/ui/fonts/archivo/` and `game/ui/fonts/atkinsonhyperlegiblenext/`,
   for the site under `site/src/assets/fonts/archivo/` and
   `site/src/assets/fonts/atkinsonhyperlegiblenext/`. A WOFF2 conversion
   or subset for the web is fine; keep the family name and ship that
   family's `OFL.txt` with it.
4. Game: set `font.family.body` to the Atkinson file's `res://` path and
   `font.family.display` to the Archivo path in the tokens file. The
   tokens file cannot set a variable font's axes, and the game's theme
   is built in C++, so the ui-developer sets them in the `UiTheme`
   builder (`extension/src/ui/ui_theme.cpp`): wrap the loaded display
   font in a `godot::FontVariation` with `variation_opentype` set to
   `wdth` 125 and `wght` 800 for `TitleLabel`, and `wdth` 112 and `wght`
   700 for `HeadingLabel` and `DangerLabel`, then build and run the
   extension self-test.
5. Site: add `@font-face` rules for both families (Archivo with
   `font-stretch: 62% 125%`), then use `font-stretch:
   var(--font-display-stretch)` and `font-weight:
   var(--font-display-weight)` on headings.
6. Add a row per family to `game/assets/PROVENANCE.md` and an entry to
   `CREDITS.md` (family, copyright line, OFL 1.1, source URL and commit).

## Swap the site's tokens (web developer)

1. Copy the CSS over the placeholder:

   ```sh
   cp design/tokens/tokens.css site/src/assets/tokens.css
   ```

   Every variable the old file had is kept, in the light block and the
   `prefers-color-scheme: dark` block.
2. Note what changes: every colour value moves to the identity's palette,
   and `--radius` is 0.25 rem (was 0.5).
3. Use the new variables where they help: `--color-link`,
   `--color-danger`, `--color-success`, `--color-warning`,
   `--color-info`, `--text-*`, `--band-height`.
4. Build and preview the site
   ([Build and preview the website](build-and-preview-the-website.md))
   in both colour schemes, and update `docs/reference/website.md` if it
   lists token values.

## Add the band to a header or panel

1. Copy `design/patterns/step-fret-band.svg` next to the page or into the
   Godot project.
2. On the web, use it as a repeating background:

   ```css
   .band {
     height: var(--band-height);              /* 36 px */
     background: url("step-fret-band.svg") 0 0 / auto 100% repeat-x;
   }
   ```

3. In Godot, use it as a `TextureRect` (stretch mode tile) or a
   `StyleBoxTexture` strip at the top and bottom of the panel, 36 px high.
4. Below 18 px high, draw plain colour rules instead
   (blue, soot, ochre, soot, red).

## Place the logo in a new layout

1. Choose rendered or flat: at 48 px and up in full colour, use the
   renders in `design/keyart/renders/` (`lockup-hero.png`,
   `mark-hero.png`, `title-wordmark.png` over key art or soot; see the
   style guide's
   [rendered mark](../../design/README.md#the-rendered-mark-and-key-art)
   and [gold wordmark](../../design/README.md#gold-wordmark) rules).
   Otherwise choose a flat version by the space and the ground:
   - wide space: `lockup-horizontal.svg`; square or tall: `lockup-stacked.svg`;
   - light ground: the plain files; dark ground: `-reversed` lockups or
     `mark-on-dark.svg`;
   - one colour only: `mark-mono.svg` (soot) or `mark-reversed.svg` (lime);
   - under 40 px wide on a light ground: `mark-small.svg` at 32 px, or at
     16 px where 32 does not fit; on a dark ground use `app-icon-16.svg`,
     which carries its own field.
2. Size it at or above the minimum in the style guide's
   [clear space and minimum size](../../design/README.md#clear-space-and-minimum-size)
   table.
3. Work out the clear space: x = 0.24 x the mark's width (in either lockup,
   x = 0.24 x the width of the mark inside it; for the horizontal lockup
   that is 0.048 x the lockup's width). Keep 1 x empty on every
   side.
4. Check the ground: the soot outline (light grounds) or the lime keyline
   (dark grounds) must reach 3 : 1 against it. Test a colour with:

   ```sh
   python3 -c "import sys; sys.path.insert(0, 'design/identity'); import check_contrast as c; print(c.ratio('#1e1916', '#RRGGBB'))"
   ```

5. Compare the result with the
   [misuse sheet](../../design/previews/misuse.png): not mirrored,
   recoloured, stretched, rotated or given effects.

## Rebuild the key art

The renders in `design/keyart/renders/` are made from the scenes in
`design/keyart/` and the meshes and textures that
`design/keyart/gen/build.py` generates. Rebuild them after changing a
scene, a generator, or `design/logo/wordmark.svg` (the gold wordmark is
built from its outlines).

You need:

- Python 3 with numpy and Pillow: the project venv from
  [Build and test: Check the assets](build-and-test.md#check-the-assets),
  activated (`. .venv/bin/activate`) so `python3` finds them.
- The Godot version pinned in `tools/release/godot.env` (4.7 or later) with a Vulkan GPU and a
  display: the bake opens the editor and the render opens a window.
- The game's native extension built first
  ([Build and test: Build the extension](build-and-test.md#build-the-extension-and-load-the-game)).
  `design/keyart/keyart.gdextension` loads `game/bin/libmontezuma.*`
  for its lightmap baker; without it the `--bake-lightmaps` step does
  nothing.

Run from the repository root:

1. Generate the meshes and textures (about 15 minutes on a 32-thread
   machine; they are build outputs and git ignores them):

   ```sh
   python3 design/keyart/gen/build.py
   ```

2. Import the project once, then restore `project.godot`, which the
   import rewrites:

   ```sh
   godot --headless --path design/keyart --import
   git checkout design/keyart/project.godot
   ```

3. Bake the lightmaps of both stages. The editor opens, bakes, prints
   `LIGHTMAP_BAKE ok <scene> <ms>` for each and quits:

   ```sh
   godot --editor --path design/keyart -- --bake-lightmaps=res://stage_studio.tscn,res://stage_plaza.tscn
   ```

4. Render every still in one launch. `render_all.tscn` lays the shots
   out on one 8192 x 8192 canvas and writes one PNG per frame:

   ```sh
   mkdir -p /tmp/keyart
   godot --path design/keyart --write-movie /tmp/keyart/all.png --fixed-fps 1 --quit-after 16
   ```

5. Cut the last frame into the five renders:

   ```sh
   python3 design/keyart/gen/finish.py /tmp/keyart/all00000015.png
   ```

6. Look at each file in `design/keyart/renders/`, then refresh the
   applications previews, which are built on the renders:

   ```sh
   python3 design/identity/render.py
   ```

Commit the scenes, the generator code and the renders; the meshes,
textures and lightmaps stay uncommitted.

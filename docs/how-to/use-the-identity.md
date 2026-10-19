<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Use the identity

*How-to. Swap the game's and the site's placeholder tokens for the
identity's, vendor the two fonts, and place the logo in a new layout. The
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
3. Note the one changed value: `radius.control` is 4 px (was 8). Panels
   use twice that.
4. Build and run the extension self-test and look at the menus
   ([Build and test](build-and-test.md)); check them with
   [Check UI text and layout](check-ui-text-and-layout.md).
5. Update the "Theme tokens" section of `docs/reference/ui.md` to name the
   new file, in the same PR.

## Vendor the fonts (ui-developer and web developer)

1. Download the files listed in the style guide's
   [source and licence table](../../design/README.md#source-and-licence)
   from google/fonts at the pinned commit, including each `OFL.txt`.
2. Check each file's sha256 against the table:

   ```sh
   sha256sum Archivo*.ttf AtkinsonHyperlegibleNext*.ttf OFL.txt
   ```

3. Put each font with its `OFL.txt` beside it: for the game under
   `game/ui/fonts/<family>/`, for the site under `site/src/assets/fonts/`.
   A WOFF2 conversion or subset for the web is fine; keep the family
   name and ship `OFL.txt` with it.
4. Game: set `font.family.body` to the Atkinson file's `res://` path and
   `font.family.display` to the Archivo path in the tokens file. For
   titles, set Archivo's variation to width 125, weight 800 in the theme.
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
2. Note the changed value: `--radius` is 0.25 rem (was 0.5).
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

1. Choose the version by the space and the ground:
   - wide space: `lockup-horizontal.svg`; square or tall: `lockup-stacked.svg`;
   - light ground: the plain files; dark ground: `-reversed` lockups or
     `mark-on-dark.svg`;
   - one colour only: `mark-mono.svg` (soot) or `mark-reversed.svg` (lime);
   - under 40 px wide: `mark-small.svg` at exactly 16 or 32 px.
2. Size it at or above the minimum in the style guide's
   [clear space and minimum size](../../design/README.md#clear-space-and-minimum-size)
   table.
3. Work out the clear space: x = 0.24 x the mark's width (for a lockup,
   x = 0.048 x the horizontal lockup's width). Keep 1 x empty on every
   side.
4. Check the ground: the soot outline (light grounds) or the lime keyline
   (dark grounds) must reach 3 : 1 against it. Test a colour with:

   ```sh
   python3 -c "import sys; sys.path.insert(0, 'design/identity'); import check_contrast as c; print(c.ratio('#1e1916', '#RRGGBB'))"
   ```

5. Compare the result with the
   [misuse sheet](../../design/previews/misuse.png): not mirrored,
   recoloured, stretched, rotated or given effects.

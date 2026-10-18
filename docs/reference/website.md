# Website

Facts about the project website in `site/` (owner: frontend-web-developer,
WP-35). To build it, see
[How to build and preview the website](../how-to/build-and-preview-the-website.md);
for the reasons behind its shape, see
[About the website](../explanation/about-the-website.md).

## Pages

| URL (under the base URL) | Source template | Filled from |
|---|---|---|
| `/` | `site/src/index.html` | `site/data/releases.json`, demo mode |
| `/credits/` | `site/src/credits/index.html` | `CREDITS.md` |
| `/license/` | `site/src/license/index.html` | `LICENSE`, `site/data/releases.json` (`source_url`, `version`) |
| `/sitemap.xml`, `/robots.txt` | generated | page list, base URL |

Base URL: `https://opsroller.github.io/montezuma/` (GitHub Pages project
site) unless `--base-url` says otherwise. All links inside the site are
relative, so the same build works under any path.

Every page has one `h1`, a skip link, a canonical URL and a footer linking
Download, Credits, License and Source code.

## Files

| Path | What it is |
|---|---|
| `site/build.py` | Build script, Python standard library only. Writes `site/_site/` (not committed) |
| `site/src/` | Page templates (`{{name}}` placeholders), `assets/` (CSS), `js/` (ES modules) |
| `site/src/assets/tokens.css` | Design tokens. Placeholders until the visual identity (WP-04) exports real ones |
| `site/src/js/platform.js` | `detectPlatform(navigator)`, the only platform guess |
| `site/src/js/main.js` | Demo loader, stub demo, download popup |
| `site/data/releases.json` | Release data for the download list |
| `site/data/notices.json`, `site/data/notices/` | Third-party software in the game and demo: name, URL, license, and the full license text shown on `/license/` |
| `site/tests/` | `test_build.py` (unittest) and `platform.test.mjs` (`node --test`) |

## Build options

| Option | Default | Effect |
|---|---|---|
| `--out DIR` | `site/_site` | Output folder; replaced on every build. An existing non-empty folder is deleted only if it holds the `.montezuma-site-build` marker an earlier build wrote |
| `--base-url URL` | `https://opsroller.github.io/montezuma/` | Canonical URLs and sitemap |
| `--demo-dir DIR` | none | Godot web export folder containing `montezuma.js`. Copied to `play/`; the page runs the real demo. Without it the page runs the stub demo |

The build fails, naming the file and field, when release data is invalid,
a credits link is not `https://` or `#…`, or a template placeholder has no
value.

## Release data: `site/data/releases.json`

Produced by the release tooling (devops-engineer). The committed file is a
stub with no builds, so every platform shows "Coming soon".

| Field | Type | Rule |
|---|---|---|
| `version` | string or null | Required once any `file` platform has a `url` |
| `released` | `YYYY-MM-DD` or null | Shown next to the version |
| `source_url` | https URL | Source code for this release (a tagged archive once releases exist) |
| `notes_url` | https URL or null | Release notes link |
| `platforms` | list | Exactly one entry per key below, any order |

Each platform:

| Field | Type | Rule |
|---|---|---|
| `key` | `windows`, `macos`, `linux`, `android`, `ios` | Shown in this fixed order |
| `label` | string | Visible name, e.g. `iPhone and iPad` |
| `kind` | `file` or `store` | A downloadable file or a store listing |
| `url` | https URL or null | null means not released on this platform yet |
| `store` | `app-store` or `google-play` | Required for `kind: store` |
| `size_bytes` | positive integer | Required for a released `file` |
| `sha256` | 64 lowercase hex digits | Required for a released `file` |
| `min_os` | string | Optional, e.g. `Windows 10` |

Store listings show as text links ("Get it on Google Play") until the
official badge artwork is added; badges are never redrawn.

## Platform detection

`detectPlatform` returns a key above or `null`. Evidence, in order:

1. `navigator.userAgentData.platform` when present: `Windows`, `macOS`,
   `Android`, `iOS`, `Linux` map to keys; anything else (including
   `Chrome OS`) gives `null`.
2. User-agent tokens: `Android` (checked before `Linux`), `iPhone`/`iPod`/`iPad`,
   `CrOS` (gives `null`), `Windows`, `Macintosh`/`Mac OS X`, `Linux`.
3. A Mac user agent with `navigator.maxTouchPoints > 1` is an iPad
   asking for the desktop site: `ios`.

The guess only picks the popup's primary button. Every platform is always
listed, on the page and in the popup.

## Demo hand-off contract

Agreed with the web demo's owner (gameplay-programmer, WP-21/WP-34). The
canonical copy is [Web demo hand-off in contracts](contracts.md#web-demo-hand-off-owner-gameplay-programmer-wp-21wp-34-consumer-frontend-web-developer-wp-35--agreed);
this section summarises it for the site.

**Files.** The Godot 4.4 single-threaded web export, named `montezuma`
(`montezuma.js`, `montezuma.wasm`, `montezuma.side.wasm`, `montezuma.pck`,
the audio worklet scripts) and the game's extension library
`libmontezuma.web.template_release.wasm32.nothreads.wasm`, all in one
folder: the `montezuma-<version>-web.zip` artifact of the `web` workflow
(see [Build pipeline](build-pipeline.md#web-demo)). The site does not use
the export's own HTML page.

**Loading.** On the visitor's click, the page loads `play/montezuma.js`
and starts it with Godot's `Engine` API:

```js
const lib = "libmontezuma.web.template_release.wasm32.nothreads.wasm";
const engine = new Engine({
  executable: "play/montezuma",
  canvas,                      // created by the page inside #stage; needs an id
  canvasResizePolicy: 0,       // the page sizes the canvas
  ensureCrossOriginIsolationHeaders: false,
  onProgress(current, total) { /* updates the <progress> bar */ },
});
await engine.preloadFile(`play/${lib}`, lib);  // the game's extension
await engine.startGame();
```

The extension must be preloaded under its bare file name: Godot opens it
by that name from the game's file system. Without it the game starts
with no game classes and logs `Can't open dynamic library`. (Godot's
`gdextensionLibs` setting does not work from a subfolder: it fetches the
file relative to the page and opens it by the same path.) The canvas
needs an `id`; Godot selects it with `#<id>`. Amended in WP-21
(2026-10-18) after running the export in a browser.

It checks `WebAssembly` and `Engine.isWebGLAvailable(2)` first and shows a
message pointing to the downloads when either is missing.

**Events, game to page.** Dispatched on `window` as `CustomEvent`s (from
`JavaScriptBridge`). Unknown events and extra fields are ignored.

| Event | `detail` | When | Page does |
|---|---|---|---|
| `montezuma:level-completed` | `{ v: 1, level: "<level id>", score: <int> }` | Once, when the intro level is won | Opens the download popup with "You beat the intro level!" |
| `montezuma:ready` (optional) | `{ v: 1, version: "<game version>" }` | First frame shown | Nothing yet |

**Events, page to game.** None.

**Skip.** The "Skip the level and get the game" link opens the same popup
without the win heading. Without JavaScript it jumps to `#download`.

## Hosting

**Not public yet.** The site stays unpublished until the game itself is
polished. The team owns that quality bar and decides when the site is
ready; the project-manager then takes that decision to the owner, whose
only sign-off is going public. Until
then CI builds the site as a downloadable preview artifact only; there is
no Pages deployment.

When it goes live, the target is GitHub Pages, deployed from `main` by a
workflow owned by devops-engineer. The single-threaded export needs no
cross-origin-isolation headers; GitHub Pages serves `.wasm` as
`application/wasm`.

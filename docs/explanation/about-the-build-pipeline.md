# About the build pipeline

Why the desktop builds are made the way they are. The facts are in the
[build pipeline reference](../reference/build-pipeline.md).

## Compile natively, export once

The game is two parts: the C++ extension, which is compiled code for each
operating system, and the Godot project, which is packed into a data file
and joined to Godot's prebuilt export template. Only the first part needs
each platform's compiler. So each extension library is built on its own
OS (MSVC on Windows, Apple Clang on macOS for a universal library, GCC on
Linux), and one Linux job exports all three games.

Exporting on one machine means one download of the 1.2 GB templates
archive (cached), one place where presets are applied, and Godot's
built-in signer can ad-hoc sign the macOS app without a Mac. The
trade-off is that the macOS and Windows packages are first run in the
`smoke` jobs, after packaging. That is deliberate: the smoke test runs the
package a player would download, not the source tree.

## Release templates only

The packages use `template_release` libraries and release export
templates: they are what players get, without debug checks. Debug builds
stay a developer's local choice; the tests workflow covers the debug
extension.

## Pinned and verified inputs

The Godot editor and templates are downloaded from Godot's GitHub release
and checked against the SHA-512 sums Godot publishes; a mismatch stops
the job. Actions are pinned to commit SHAs and runners to named image
versions, so a build changes only when this repository changes. Archives
are packed with stable entry order and timestamps, so the same inputs
give the same archive; checking that two runners agree byte for byte is
still to be done.

## Why the macOS app is only ad-hoc signed

Apple silicon Macs will not run unsigned code, so the app is ad-hoc
signed, which needs no account. Players still see a Gatekeeper warning
until the app is signed with a Developer ID and notarized. That needs the
owner's Apple Developer account and certificates in the CI secret store,
which come with the store packages, not here.

## Desktop on every push to main

Every push to `main` builds all three packages, so `main` always has
downloadable builds. Pull requests build them whenever they change
anything that goes into a package (the pipeline, the C++ code, the Godot
project, godot-cpp), so a change that breaks an export is caught before
it reaches `main`. Docs-only changes skip it.

## The web demo

The web demo is built by its own workflow, `web.yml`, in one Linux job:
Emscripten and Godot's web templates both run there, so there is nothing
to split across runners.

**Single-threaded.** Godot's threaded web build needs `SharedArrayBuffer`,
which browsers only allow on pages served with cross-origin isolation
headers. Static hosts such as GitHub Pages cannot set them, and the
demo should run wherever the site is served, so the export is
single-threaded and the extension is built with `GODOTCPP_THREADS=OFF`.
The intro level does not need threads.

**The extension is a side module.** On the web, Godot loads a GDExtension
as a WebAssembly side module that links against the engine's main module
at start. That only works when both come from the same Emscripten
version, so the pin is the version Godot's own release used for its
templates, not the newest Emscripten. When the Godot pin moves, the
Emscripten pin moves with it.

**Tested in a real browser.** Running the export headless in Godot would
not exercise what can break on the web: loading the side module,
WebGL 2 and the events the page waits for. The smoke test serves the
package the way the site does, starts it through Godot's `Engine` API
with the site's settings, and checks both hand-off events arrive. It
uses headless Chromium; Firefox and Safari are checked by hand until
their headless WebGL runs reliably in CI.

**Built, not deployed.** Nothing goes public until the game is polished,
so the workflow only produces the package. The website deploy picks it up
later.

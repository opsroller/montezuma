<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# About the website

Explanation of why the website is shaped the way it is. For anyone
changing `site/`. The facts are in the
[website reference](../reference/website.md); the steps to build it are in
[How to build and preview the website](../how-to/build-and-preview-the-website.md).

The website exists to turn a curious visitor into a player. Its front
page is the game itself: the visitor plays the intro level in the browser,
and winning it opens the download for their device. The credits and
license pages make good on the project's promise that the game is open
source and that the people and the game behind it are named.

## Why the page plays the game instead of describing it

A match-3 game sells itself in thirty seconds of play better than in any
screenshot. Making the intro level the entry point puts the strongest
argument first, and making the download popup the reward for finishing it
asks for the download at the moment the visitor is most interested. A
"Skip the level" link and a plain download section keep the page useful to
anyone who would rather not play first, including visitors without
JavaScript or WebGL.

## Why the demo loads on click

The web demo is tens of megabytes. Loading it with the page would make the
first visit slow on phones and metered connections and would hurt the
page's loading metrics. The page reserves the demo's space, shows a play
button, and downloads the build only when asked, with a progress bar.
The button names the download size, worked out from the package at build
time, so a visitor on a phone plan knows the cost before choosing.

## Why the page watches the game's downloads

Godot's loader was written for its own page, which can afford a vague
bar and a spinner. Ours promises a size on the button, so the bar has to
reach the end only when the download does, and a broken download has to
end in a message and a retry, not a wait. Godot misses both: half the
bytes (the side module) arrive outside its progress reports, and a failed
file leaves its start pending forever. Watching the page's own requests
covers both without downloading anything twice or patching Godot's
files, which the site takes unchanged from each web build. A timer that
gives up after a minute without progress stays as a backstop. Godot also
keeps a failed start in its module state, which is why a retry reloads
the page. The behaviour is listed in the
[demo hand-off contract](../reference/website.md#demo-hand-off-contract).

## Why the extension is preloaded by hand

Godot's `gdextensionLibs` setting fetches the extension relative to the
page and opens it by that same path, so it fails when the build lives in
a subfolder such as `play/`. Preloading it under its bare name puts it
where the game opens it. This was found in WP-21 by running the export
in a browser.

## Why the site is not public yet

The site goes public only once the game is polished: its front page is
the game itself, so it is the project's first impression, and an
unfinished level there would undercut the downloads it leads to. The team owns that
quality bar and decides when the site is ready; the project-manager
takes the decision to the owner, whose only sign-off is going public.
What is still open before then is listed in the
[website audit](../reference/website-audit.md).

## Why the demo runs in the page, not an iframe

An iframe is the usual way to isolate a web build that needs special
headers or comes from another origin. This one needs neither: Godot 4.7's
single-threaded web export runs without cross-origin isolation, and the
build is served from the same site. Running it in the page lets the game
announce a win with a plain DOM event (`montezuma:level-completed`) that
the page listens for, with no `postMessage` origin checks to get wrong. If
a later build needs threads, and so cross-origin isolation, it moves into
an iframe with those headers scoped to it.

## Why the popup lists every platform

Browsers report their platform unreliably: iPads ask for desktop sites
with a Mac user agent, and some browsers freeze or hide the value. The
page uses its guess only to choose the big button and always lists every
platform beneath it, so a wrong guess costs a glance rather than a lost
player.

## Why the pages are generated

Versions, download links, checksums, credits and the license text all
change, and copies typed by hand drift. The build fills the pages from the
files that own those facts (`site/data/releases.json` from the release
tooling, `CREDITS.md`, `LICENSE`), so the website, the game's credits
screen and the release always agree.

## Why the stub demo

The website was built before the web demo existed. The stub fires the
same event the game will, so the page, the popup and their tests are
finished and only the build needs swapping in when it lands. It still
runs whenever the site is built without a web package, which keeps site
work and its tests independent of the game's slow WebAssembly build.

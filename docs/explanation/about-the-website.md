# About the website

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
finished and only the build needs swapping in when it lands.

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# Website audit

The state of the website in `site/` against the frontend-web-developer
audit checks, for the build with the real web demo (WP-35). The site
itself is described in the [website reference](website.md); how to build
and check it is in
[How to build and preview the website](../how-to/build-and-preview-the-website.md);
why it is not public yet is in [About the website](../explanation/about-the-website.md).

**Result:** no blockers. The checks in the matrix were run on a local
build of `/`, `/credits/` and `/license/` in Chromium (Playwright), with
`--demo-dir` pointing at a web package built from this repository; the
ones a local build cannot answer are under [Not checked](#not-checked).

In the page columns, ✓ means the page passes, ✗ means it fails, and a
blank cell means the check does not apply to that page. A check with no
page marks is about the site as a whole. Open items name the role that
owns them.

## Test matrix

| Check | `/` | `/credits/` | `/license/` | Result | Owner |
|---|---|---|---|---|---|
| axe-core 4.10, light and dark, with the popup open | ✓ | ✓ | ✓ | Pass, 0 violations |  |
| Headings, landmarks, titles, skip link, focus rings | ✓ | ✓ | ✓ | Pass |  |
| Reflow at 320 px, text spacing, reduced motion, contrast | ✓ | ✓ | ✓ | Pass |  |
| Works without JavaScript | ✓ | ✓ | ✓ | Pass: `<noscript>` message, downloads list |  |
| Same header navigation on every page | ✓ | ✗ | ✗ | Open (minor): subpages have only the footer navigation | frontend-web-developer |
| Load metrics (throttled mobile) | ✓ | | | Pass: LCP about 0.5 s, CLS 0, TBT about 30 ms, page under 32 KB |  |
| No game bytes before the click; side module fetched once | ✓ | | | Pass |  |
| Demo progress, failure, retry and unsupported states | ✓ | | | Pass |  |
| A failed request Godot retries does not end the load | ✓ | | | Pass: aborting the first 1 to 3 requests for a file still starts the game |  |
| Demo keyboard entry and exit (Shift+Escape) | ✓ | | | Pass |  |
| Demo events (v1 only, type checked) | ✓ | | | Pass |  |
| Cross-origin isolation and `postMessage` | ✓ | | | Not needed: single-threaded export in the same document |  |
| Every platform listed; detection only picks the primary button | ✓ | | | Pass, including an iPad reporting as a Mac |  |
| Internal links resolve | ✓ | ✓ | ✓ | Pass (checked by hand; no automated test) |  |
| Credits match `CREDITS.md`; license text matches `LICENSE` | | ✓ | ✓ | Pass |  |
| Demo version, commit and source named | | | ✓ | Pass, from `play/SOURCE.txt` |  |
| Third-party notices for the demo linked | | | ✓ | Pass, `play/THIRD-PARTY-NOTICES.txt` |  |
| Emscripten runtime, musl and libc++ in the package notices | | | ✗ | Open (major), before going public | devops-engineer |
| Titles, descriptions, canonical URLs, sitemap | ✓ | ✓ | ✓ | Pass |  |
| Open Graph image and card | ✗ | ✗ | ✗ | Open (major on `/`) | frontend-web-developer, with the image from graphic-designer |
| Custom 404 page | | | | Open (minor) | frontend-web-developer |
| Favicon | ✗ | ✗ | ✗ | Open (minor) | frontend-web-developer, with the game's icons (WP-38) |
| Content-Security-Policy and referrer policy | ✗ | ✗ | ✗ | Open (minor): what `/` needs is in the [hosting section](website.md#hosting) | frontend-web-developer |
| Third-party requests, cookies, analytics | ✓ | ✓ | ✓ | Pass: none |  |
| Privacy note (the demo uses IndexedDB and a `sessionStorage` flag) | ✗ | | | Open (minor), before going public | frontend-web-developer |
| Accessibility statement; real screen readers | | | | Open (minor), before going public | frontend-web-developer |
| Performance budget written and tested | | | | Open (minor) | frontend-web-developer |
| Download copy matches release data | ✗ | | | Open (minor): the popup and description promise downloads while every platform is "Coming soon" | frontend-web-developer |

## Not checked

These need a deployment, and the site is not deployed:

- Whether GitHub Pages compresses `play/*.wasm` and `play/montezuma.pck`.
  The play button's size assumes it does. See
  [Check a deployed demo](../how-to/build-and-preview-the-website.md#check-a-deployed-demo).
- HTTPS, HSTS and the response headers GitHub Pages sends.

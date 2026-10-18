#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Run a packaged web demo in headless browsers the way the site does.

Usage: web-smoke.py <archive> [browser ...]
  <archive>  montezuma-<version>-web.zip (its .sha256 file sits beside it)
  browser    chromium, firefox or webkit; default chromium

Verifies the checksum, unpacks the archive into play/ under a local HTTP
server, and opens a page at the server root that starts the game with
Godot's Engine API exactly as the contract in docs/reference/contracts.md
(Web demo hand-off) describes. The game gets the user argument
--complete-level, which wins the level on its first frame. Passes when the
page receives montezuma:ready and then montezuma:level-completed with the
contract's detail, and the browser logs no Godot error.

Needs the Python package playwright (tools/release/web-requirements.txt)
and its browsers (python -m playwright install <browser>).
"""

import functools
import hashlib
import http.server
import json
import os
import sys
import tempfile
import threading
import zipfile

from playwright.sync_api import sync_playwright

TIMEOUT_MS = 90_000
LEVEL_ID = "e1-l01"
EXTENSION = "libmontezuma.web.template_release.wasm32.nothreads.wasm"

# The site's loader (site/src/js/main.js), reduced to what the contract
# needs, plus listeners that record the hand-off events for the test.
PAGE = """<!doctype html>
<meta charset="utf-8">
<title>web smoke</title>
<div id="stage"></div>
<script>
window.handoff = [];
for (const name of ["montezuma:ready", "montezuma:level-completed"]) {
  window.addEventListener(name, (e) => window.handoff.push({ type: e.type, detail: e.detail }));
}
const s = document.createElement("script");
s.src = "play/montezuma.js";
s.onload = () => {
  const canvas = document.createElement("canvas");
  canvas.id = "game-canvas";
  canvas.width = 1280;
  canvas.height = 720;
  document.getElementById("stage").append(canvas);
  const engine = new Engine(Object.assign(CONFIG, {
    canvas,
    canvasResizePolicy: 0,
    ensureCrossOriginIsolationHeaders: false,
    args: ["--", "--complete-level"],
  }));
  // The extension goes into the game's file system under its bare name,
  // where Godot's dlopen finds it (contract: Loading).
  engine.preloadFile("play/" + EXTENSION, EXTENSION)
    .then(() => engine.startGame())
    .catch((e) => window.handoff.push({ type: "error", detail: String(e) }));
};
document.head.append(s);
</script>
"""


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map,
                      ".wasm": "application/wasm", ".pck": "application/octet-stream"}

    def log_message(self, *args):
        pass


class Server(http.server.ThreadingHTTPServer):
    def handle_error(self, request, client_address):
        # A browser closing a download early is not a test failure.
        if not isinstance(sys.exc_info()[1], ConnectionError):
            super().handle_error(request, client_address)


def verify(archive):
    with open(archive + ".sha256", encoding="utf-8") as f:
        want = f.read().split()[0]
    with open(archive, "rb") as f:
        got = hashlib.sha256(f.read()).hexdigest()
    if got != want:
        sys.exit(f"{archive}: checksum mismatch")


def unpack(archive, root):
    with zipfile.ZipFile(archive) as z:
        z.extractall(root)
    (top,) = os.listdir(root)
    os.rename(os.path.join(root, top), os.path.join(root, "play"))


def run(browser_type, url):
    errors = []
    browser = browser_type.launch()
    try:
        page = browser.new_page()
        page.on("console", lambda m: m.type == "error" and errors.append(m.text))
        page.on("pageerror", lambda e: errors.append(str(e)))
        page.goto(url)
        page.wait_for_function(
            "window.handoff && window.handoff.some(e => e.type !== 'montezuma:ready')",
            timeout=TIMEOUT_MS)
        events = page.evaluate("window.handoff")
    finally:
        browser.close()
    return events, errors


def check(name, events, errors):
    problems = []
    types = [e["type"] for e in events]
    if types != ["montezuma:ready", "montezuma:level-completed"]:
        problems.append(f"expected ready then level-completed, got {json.dumps(events)}")
    else:
        ready, done = (e["detail"] for e in events)
        if ready.get("v") != 1 or not isinstance(ready.get("version"), str):
            problems.append(f"bad montezuma:ready detail {ready}")
        if (done.get("v") != 1 or done.get("level") != LEVEL_ID
                or not isinstance(done.get("score"), int)):
            problems.append(f"bad montezuma:level-completed detail {done}")
    problems += [f"browser error: {e}" for e in errors]
    for p in problems:
        print(f"{name}: {p}", file=sys.stderr)
    if not problems:
        print(f"{name}: ok {json.dumps(events)}")
    return not problems


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    archive = os.path.abspath(argv[1])
    browsers = argv[2:] or ["chromium"]
    verify(archive)
    with tempfile.TemporaryDirectory() as root:
        unpack(archive, root)
        page = (PAGE.replace("CONFIG", json.dumps({"executable": "play/montezuma"}))
                .replace("EXTENSION", json.dumps(EXTENSION)))
        with open(os.path.join(root, "index.html"), "w", encoding="utf-8") as f:
            f.write(page)
        server = Server(
            ("127.0.0.1", 0), functools.partial(Handler, directory=root))
        threading.Thread(target=server.serve_forever, daemon=True).start()
        url = f"http://127.0.0.1:{server.server_address[1]}/index.html"
        ok = True
        try:
            with sync_playwright() as p:
                for name in browsers:
                    events, errors = run(getattr(p, name), url)
                    ok = check(name, events, errors) and ok
        finally:
            server.shutdown()
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main(sys.argv)

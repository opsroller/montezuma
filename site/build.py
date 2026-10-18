#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Build the static website into site/_site/.

Fills the page templates in site/src/ from their single sources: release
data (site/data/releases.json), credits (CREDITS.md) and the license
(LICENSE). Copies the web demo export in when one is given, otherwise the
page runs its stub demo. Standard library only.

Usage: python3 site/build.py [--out DIR] [--base-url URL] [--demo-dir DIR]
"""

from __future__ import annotations

import argparse
import gzip
import html
import json
import re
import shutil
import sys
from pathlib import Path

SITE = Path(__file__).resolve().parent
REPO = SITE.parent
DEFAULT_BASE_URL = "https://opsroller.github.io/montezuma/"

PLATFORM_KEYS = ("windows", "macos", "linux", "android", "ios")
STORE_NAMES = {"app-store": "the App Store", "google-play": "Google Play"}
DEMO_ENTRY = "montezuma.js"
# The game's extension, which the page preloads into Godot's file system.
DEMO_EXTENSION = "libmontezuma.web.template_release.wasm32.nothreads.wasm"
# Files of the export the site never serves: Godot's own page and its
# default icons (Godot's logo, not our artwork).
DEMO_UNUSED = ("montezuma.html", "*.png")
# The package's source and third-party notices, linked from /license/.
DEMO_SOURCE = "SOURCE.txt"
DEMO_NOTICES = "THIRD-PARTY-NOTICES.txt"
# What the page fetches after the click (contract: Web demo hand-off).
DEMO_FETCHED = (DEMO_ENTRY, "montezuma.wasm", "montezuma.side.wasm",
                "montezuma.pck", DEMO_EXTENSION)
PAGES = ("index.html", "credits/index.html", "license/index.html")
# Written into every build; only a folder carrying it is ever deleted.
MARKER = ".montezuma-site-build"


class BuildError(Exception):
    """Source data is missing or malformed; the message says what to fix."""


# ---- Release data -----------------------------------------------------------

def load_releases(path: Path) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise BuildError(f"{path}: {error}") from error

    platforms = data.get("platforms")
    if not isinstance(platforms, list):
        raise BuildError(f"{path}: 'platforms' must be a list")
    keys = [p.get("key") for p in platforms]
    if sorted(keys) != sorted(PLATFORM_KEYS):
        raise BuildError(f"{path}: platforms must be exactly {', '.join(PLATFORM_KEYS)}; got {keys}")
    if not _is_https(data.get("source_url")):
        raise BuildError(f"{path}: 'source_url' must be an https URL")

    for p in platforms:
        where = f"{path}: platform {p['key']}"
        if not p.get("label"):
            raise BuildError(f"{where}: missing 'label'")
        kind = p.get("kind")
        if kind not in ("file", "store"):
            raise BuildError(f"{where}: 'kind' must be 'file' or 'store'")
        if kind == "store" and p.get("store") not in STORE_NAMES:
            raise BuildError(f"{where}: 'store' must be one of {', '.join(STORE_NAMES)}")
        url = p.get("url")
        if url is None:
            continue  # not released on this platform yet
        if not _is_https(url):
            raise BuildError(f"{where}: 'url' must be an https URL or null")
        if kind == "file":
            if not data.get("version"):
                raise BuildError(f"{path}: 'version' is required once any file is released")
            if not isinstance(p.get("size_bytes"), int) or p["size_bytes"] <= 0:
                raise BuildError(f"{where}: 'size_bytes' must be a positive integer")
            if not re.fullmatch(r"[0-9a-f]{64}", p.get("sha256") or ""):
                raise BuildError(f"{where}: 'sha256' must be 64 lowercase hex digits")
    return data


def _is_https(value) -> bool:
    return isinstance(value, str) and value.startswith("https://")


def format_size(size: int) -> str:
    megabytes = size / 1_000_000
    return f"{megabytes:.0f} MB" if megabytes >= 10 else f"{megabytes:.1f} MB"


def render_platforms(data: dict, heading: int) -> str:
    """The all-platforms list, in fixed order, every platform present."""
    by_key = {p["key"]: p for p in data["platforms"]}
    items = []
    for key in PLATFORM_KEYS:
        p = by_key[key]
        label = html.escape(p["label"])
        parts = [f'<li data-platform="{key}" data-label="{label}">',
                 f"<h{heading}>{label}</h{heading}>"]
        url = p.get("url")
        if url is None:
            parts.append('<p class="soon">Coming soon.</p>')
        elif p["kind"] == "store":
            store = STORE_NAMES[p["store"]]
            # Text link until the official badge artwork is added (see the
            # website reference): badges are never redrawn.
            parts.append(f'<a class="get button" href="{html.escape(url)}">'
                         f"Get it on {store}</a>")
        else:
            version = html.escape(data["version"])
            size = format_size(p["size_bytes"])
            parts.append(f'<a class="get button" href="{html.escape(url)}" '
                         f'aria-label="Download for {label}, version {version}, {size}">'
                         f"Download for {label}</a>")
            meta = [f"Version {version}", size]
            if p.get("min_os"):
                meta.append(f"Needs {html.escape(p['min_os'])}")
            parts.append(f'<p class="meta">{" · ".join(meta)}<br>'
                         f'SHA-256: <code>{p["sha256"]}</code></p>')
        parts.append("</li>")
        items.append("".join(parts))
    return '<ul class="platforms">' + "".join(items) + "</ul>"


def render_release_line(data: dict) -> str:
    if not data.get("version"):
        return "<p>Montezuma's is in development. Builds appear here with the first release.</p>"
    line = f"<p>Version {html.escape(data['version'])}"
    if data.get("released"):
        line += f", released {html.escape(data['released'])}"
    line += "."
    if _is_https(data.get("notes_url")):
        line += f' <a href="{html.escape(data["notes_url"])}">Release notes</a>.'
    return line + "</p>"


def render_notices(path: Path) -> str:
    """Full license texts of third-party software shipped in the game and demo."""
    try:
        entries = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise BuildError(f"{path}: {error}") from error
    parts = []
    for entry in entries:
        for field in ("name", "url", "license", "file"):
            if not entry.get(field):
                raise BuildError(f"{path}: an entry is missing '{field}'")
        if not _is_https(entry["url"]):
            raise BuildError(f"{path}: {entry['name']}: 'url' must be https")
        try:
            text = (path.parent / entry["file"]).read_text(encoding="utf-8")
        except OSError as error:
            raise BuildError(f"{path}: {entry['name']}: {error}") from error
        parts.append(f'<h3 id="notice-{_slug(entry["name"])}">'
                     f'<a href="{html.escape(entry["url"])}">{html.escape(entry["name"])}</a> '
                     f'({html.escape(entry["license"])})</h3>'
                     f'<pre class="license-text">{html.escape(text)}</pre>')
    return "\n".join(parts)


def render_source_line(data: dict) -> str:
    url = html.escape(data["source_url"])
    text = (f'The complete source code is at <a href="{url}">{url}</a>.')
    if data.get("version"):
        text += f" The source for version {html.escape(data['version'])} is the tagged release there."
    return text


def render_demo_line(source: Path) -> str:
    """Which build the bundled demo is and where its exact source is (GPL section 3)."""
    text = source.read_text(encoding="utf-8")
    version = re.match(r"Montezuma's (\S+)", text)
    tree = re.search(r"https://\S+/tree/([0-9a-f]{40})", text)
    if not version or not tree:
        raise BuildError(f"{source}: expected \"Montezuma's <version>\" and a /tree/<commit> source URL")
    url = html.escape(tree.group(0))
    return (f'<p>The playable demo on this site is version {html.escape(version.group(1))}, '
            f'built from commit <a href="{url}"><code>{tree.group(1)[:12]}</code></a>, where its '
            f'complete source is. Its <a href="../play/{DEMO_SOURCE}">source notice</a> and '
            f'<a href="../play/{DEMO_NOTICES}">complete third-party notices</a>, including the '
            f'libraries Godot bundles, ship with it.</p>')


# ---- Credits (a small Markdown subset) -------------------------------------

_INLINE = [
    (re.compile(r"`([^`]+)`"), r"<code>\1</code>"),
    (re.compile(r"\*\*([^*]+)\*\*"), r"<strong>\1</strong>"),
    (re.compile(r"\*([^*]+)\*"), r"<cite>\1</cite>"),
]
_LINK = re.compile(r"\[([^\]]+)\]\(([^)\s]+)\)")


def _inline(text: str) -> str:
    text = html.escape(text, quote=False)
    for pattern, replacement in _INLINE:
        text = pattern.sub(replacement, text)

    def link(match: re.Match) -> str:
        url = html.unescape(match.group(2))
        if not (url.startswith("https://") or url.startswith("#")):
            raise BuildError(f"CREDITS.md: link {url!r} must be https or a #fragment")
        return f'<a href="{html.escape(url)}">{match.group(1)}</a>'

    return _LINK.sub(link, text)


def _slug(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")


def render_markdown(source: str) -> str:
    """Headings, paragraphs and bullet lists with wrapped lines.

    The first top-level heading is dropped (the page has its own h1); every
    other heading keeps its level, with h1 shown as h2.
    """
    out: list[str] = []
    block: list[str] = []   # lines of the current paragraph or list item
    kind = None             # "p" or "li"
    in_list = False
    seen_title = False

    def flush():
        nonlocal block, kind
        if block:
            text = _inline(" ".join(block))
            out.append(f"<li>{text}</li>" if kind == "li" else f"<p>{text}</p>")
        block, kind = [], None

    def close_list():
        nonlocal in_list
        if in_list:
            out.append("</ul>")
            in_list = False

    for raw in source.splitlines():
        line = raw.rstrip()
        heading = re.match(r"(#{1,6})\s+(.*)", line)
        bullet = re.match(r"[-*]\s+(.*)", line)
        if not line.strip():
            flush()
            continue
        if heading:
            flush()
            close_list()
            level, text = len(heading.group(1)), heading.group(2).strip()
            if level == 1 and not seen_title:
                seen_title = True
                continue
            level = max(level, 2)
            out.append(f'<h{level} id="{_slug(text)}">{_inline(text)}</h{level}>')
        elif bullet:
            flush()
            if not in_list:
                out.append("<ul>")
                in_list = True
            block, kind = [bullet.group(1)], "li"
        elif raw.startswith((" ", "\t")) and kind:
            block.append(line.strip())   # continuation of a wrapped line
        else:
            if kind == "li":
                flush()
            close_list()
            if kind is None:
                kind = "p"
            block.append(line.strip())
    flush()
    close_list()
    return "\n".join(out)


# ---- Pages ------------------------------------------------------------------

def render_footer(root: str, source_url: str) -> str:
    return (
        '<footer class="site-footer"><nav aria-label="Footer"><ul>'
        f'<li><a href="{root}#download">Download</a></li>'
        f'<li><a href="{root}credits/">Credits</a></li>'
        f'<li><a href="{root}license/">License</a></li>'
        f'<li><a href="{html.escape(source_url)}">Source code</a></li>'
        "</ul></nav>"
        "<p>Montezuma's is free software under GPL-2.0-or-later. "
        "Inspired by <cite>The Treasures of Montezuma</cite>; not affiliated with its makers.</p>"
        "</footer>"
    )


def fill(template: str, values: dict[str, str], name: str) -> str:
    def replace(match: re.Match) -> str:
        key = match.group(1)
        if key not in values:
            raise BuildError(f"{name}: no value for {{{{{key}}}}}")
        return values[key]

    return re.sub(r"\{\{(\w+)\}\}", replace, template)


def demo_download_size(demo_dir: Path) -> int:
    """Bytes the visitor downloads after the click, compressed as the host serves them."""
    return sum(len(gzip.compress((demo_dir / f).read_bytes(), 6)) for f in DEMO_FETCHED)


def build(out: Path, base_url: str, demo_dir: Path | None,
          releases: Path = SITE / "data" / "releases.json",
          credits: Path = REPO / "CREDITS.md",
          license_file: Path = REPO / "LICENSE",
          notices: Path = SITE / "data" / "notices.json") -> Path:
    if not base_url.endswith("/"):
        base_url += "/"
    data = load_releases(releases)
    notices_html = render_notices(notices)
    try:
        credits_html = render_markdown(credits.read_text(encoding="utf-8"))
        license_text = html.escape(license_file.read_text(encoding="utf-8"))
    except OSError as error:
        raise BuildError(str(error)) from error

    demo_mode = "stub"
    play_label = "Try the demo preview"
    file_sizes = {}
    demo_line = ""
    if demo_dir is not None:
        if not (demo_dir / DEMO_ENTRY).is_file():
            raise BuildError(f"{demo_dir}: no {DEMO_ENTRY}; is this the web export?")
        missing = [f for f in DEMO_FETCHED + (DEMO_SOURCE, DEMO_NOTICES) if not (demo_dir / f).is_file()]
        if missing:
            raise BuildError(f"{demo_dir}: the web export is missing {', '.join(missing)}")
        demo_mode = "godot"
        play_label = f"Play the intro level ({format_size(demo_download_size(demo_dir))})"
        demo_line = render_demo_line(demo_dir / DEMO_SOURCE)
        # Godot's loader needs the sizes up front to report progress; it
        # looks them up by the URL it fetches. main.js sums them all for the
        # progress bar, the side module (which it fetches itself) included.
        file_sizes = {f"play/{f}": (demo_dir / f).stat().st_size
                      for f in ("montezuma.wasm", "montezuma.side.wasm", "montezuma.pck", DEMO_EXTENSION)}

    if out.exists():
        if not (out / MARKER).is_file() and any(out.iterdir()):
            raise BuildError(f"{out}: not empty and not an earlier site build; "
                             "refusing to delete it (pick an empty or new --out)")
        shutil.rmtree(out)
    shutil.copytree(SITE / "src", out, ignore=shutil.ignore_patterns("*.html"))

    common = {
        "base_url": html.escape(base_url),
        "source_url": html.escape(data["source_url"]),
    }
    values = {
        "index.html": {
            "demo_mode": demo_mode,
            "play_label": play_label,
            "demo_file_sizes": html.escape(json.dumps(file_sizes, sort_keys=True)),
            "release_line": render_release_line(data),
            "platform_list_h3": render_platforms(data, 3),
            "platform_list_h4": render_platforms(data, 4),
            "footer": render_footer("", data["source_url"]),
        },
        "credits/index.html": {
            "credits": credits_html,
            "footer": render_footer("../", data["source_url"]),
        },
        "license/index.html": {
            "source_line": render_source_line(data),
            "demo_line": demo_line,
            "license_text": license_text,
            "notices": notices_html,
            "footer": render_footer("../", data["source_url"]),
        },
    }
    for page in PAGES:
        template = (SITE / "src" / page).read_text(encoding="utf-8")
        target = out / page
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(fill(template, common | values[page], page), encoding="utf-8")

    if demo_dir is not None:
        # The export's own page is a second, unlinked way into the game
        # without the site's fallbacks; the site never uses it.
        shutil.copytree(demo_dir, out / "play", ignore=shutil.ignore_patterns(*DEMO_UNUSED))

    urls = [base_url + page.removesuffix("index.html") for page in PAGES]
    (out / "sitemap.xml").write_text(
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'
        + "".join(f"  <url><loc>{html.escape(u)}</loc></url>\n" for u in urls)
        + "</urlset>\n", encoding="utf-8")
    (out / "robots.txt").write_text(f"Sitemap: {base_url}sitemap.xml\n", encoding="utf-8")
    (out / ".nojekyll").write_text("", encoding="utf-8")  # serve files as-is on GitHub Pages
    (out / MARKER).write_text("Built by site/build.py; safe to delete.\n", encoding="utf-8")
    return out


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", type=Path, default=SITE / "_site")
    parser.add_argument("--base-url", default=DEFAULT_BASE_URL)
    parser.add_argument("--demo-dir", type=Path,
                        help="Godot web export folder (with montezuma.js); omit for the stub demo")
    args = parser.parse_args(argv)
    try:
        out = build(args.out, args.base_url, args.demo_dir)
    except BuildError as error:
        print(f"site build failed: {error}", file=sys.stderr)
        return 1
    print(f"site built in {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

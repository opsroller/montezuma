# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Tests for site/build.py. Run: python3 -m unittest discover site/tests"""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from html.parser import HTMLParser
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build  # noqa: E402

SHA = "0" * 64


def release(**overrides) -> dict:
    data = {
        "version": "1.2.0",
        "released": "2027-01-01",
        "source_url": "https://github.com/opsroller/montezuma",
        "notes_url": None,
        "platforms": [
            {"key": "windows", "label": "Windows", "kind": "file",
             "url": "https://example.org/m-win.zip", "size_bytes": 85_000_000, "sha256": SHA},
            {"key": "macos", "label": "macOS", "kind": "file", "url": None},
            {"key": "linux", "label": "Linux", "kind": "file", "url": None},
            {"key": "android", "label": "Android", "kind": "store", "store": "google-play",
             "url": "https://play.google.com/store/apps/details?id=x"},
            {"key": "ios", "label": "iPhone and iPad", "kind": "store", "store": "app-store", "url": None},
        ],
    }
    data.update(overrides)
    return data


class Ids(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids: list[str] = []
        self.h1 = 0

    def handle_starttag(self, tag, attrs):
        self.h1 += tag == "h1"
        self.ids += [v for k, v in attrs if k == "id"]


class BuildTest(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())

    def write_releases(self, data: dict) -> Path:
        path = self.tmp / "releases.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        return path

    def run_build(self, data: dict | None = None, demo_dir: Path | None = None) -> Path:
        releases = self.write_releases(data) if data else build.SITE / "data" / "releases.json"
        return build.build(self.tmp / "out", "https://example.org/m", demo_dir, releases=releases)

    def test_committed_stub_builds_with_stub_demo(self):
        out = self.run_build()
        page = (out / "index.html").read_text()
        self.assertIn('data-demo="stub"', page)
        self.assertEqual(page.count("Coming soon."), 10)  # 5 platforms, page + popup
        self.assertNotIn("{{", page)

    def test_every_platform_listed_in_fixed_order(self):
        page = (self.run_build(release()) / "index.html").read_text()
        positions = [page.index(f'data-platform="{k}"') for k in build.PLATFORM_KEYS]
        self.assertEqual(positions, sorted(positions))
        self.assertEqual(page.count('data-platform="'), 10)

    def test_file_download_shows_version_size_and_checksum(self):
        page = (self.run_build(release()) / "index.html").read_text()
        self.assertIn('aria-label="Download for Windows, version 1.2.0, 85 MB"', page)
        self.assertIn(SHA, page)
        self.assertIn("Get it on Google Play", page)

    def test_pages_have_one_h1_and_unique_ids(self):
        out = self.run_build(release())
        for page in build.PAGES:
            parser = Ids()
            parser.feed((out / page).read_text())
            self.assertEqual(parser.h1, 1, page)
            self.assertEqual(len(parser.ids), len(set(parser.ids)), page)

    def test_canonical_and_sitemap_use_base_url(self):
        out = self.run_build()
        self.assertIn('<link rel="canonical" href="https://example.org/m/credits/">',
                      (out / "credits/index.html").read_text())
        self.assertIn("<loc>https://example.org/m/license/</loc>", (out / "sitemap.xml").read_text())

    def test_credits_come_from_credits_md(self):
        out = self.run_build()
        credits = (out / "credits/index.html").read_text()
        source = (build.REPO / "CREDITS.md").read_text()
        for heading in [line[3:] for line in source.splitlines() if line.startswith("## ")]:
            self.assertIn(f">{heading}</h2>", credits)
        self.assertIn("Claude", credits)
        self.assertIn("The Treasures of Montezuma", credits)

    def test_license_page_carries_full_license(self):
        page = (self.run_build() / "license/index.html").read_text()
        self.assertIn("GNU GENERAL PUBLIC LICENSE", page)
        self.assertIn("Version 2, June 1991", page)

    def test_license_page_carries_third_party_notices(self):
        page = (self.run_build() / "license/index.html").read_text()
        self.assertIn('id="notice-godot-engine"', page)
        self.assertIn("Juan Linietsky, Ariel Manzur", page)
        self.assertIn("Permission is hereby granted, free of charge", page)

    def test_refuses_to_delete_a_folder_it_did_not_build(self):
        precious = self.tmp / "out"
        precious.mkdir()
        (precious / "keep.txt").write_text("mine")
        with self.assertRaises(build.BuildError):
            self.run_build()
        self.assertTrue((precious / "keep.txt").is_file())

    def test_rebuilds_over_its_own_output(self):
        out = self.run_build()
        (out / "stale.txt").write_text("old")
        self.run_build()
        self.assertFalse((out / "stale.txt").exists())

    def make_export(self) -> Path:
        demo = self.tmp / "export"
        demo.mkdir()
        for name in build.DEMO_FETCHED:
            (demo / name).write_bytes(b"\0asm")
        return demo

    def test_demo_dir_switches_to_godot_and_is_copied(self):
        out = self.run_build(demo_dir=self.make_export())
        page = (out / "index.html").read_text()
        self.assertIn('data-demo="godot"', page)
        self.assertTrue((out / "play/montezuma.wasm").is_file())
        self.assertTrue((out / "play" / build.DEMO_EXTENSION).is_file())

    def test_demo_file_sizes_and_no_export_page(self):
        demo = self.make_export()
        (demo / "montezuma.html").write_text("<!doctype html>")
        out = self.run_build(demo_dir=demo)
        page = (out / "index.html").read_text()
        self.assertIn('data-file-sizes="{&quot;play/libmontezuma.web.template_release.wasm32.nothreads.wasm&quot;: 4, '
                      '&quot;play/montezuma.pck&quot;: 4, &quot;play/montezuma.wasm&quot;: 4}"', page)
        self.assertFalse((out / "play/montezuma.html").exists())

    def test_stub_has_no_file_sizes(self):
        self.assertIn('data-file-sizes="{}"', (self.run_build() / "index.html").read_text())

    def test_play_button_shows_download_size(self):
        demo = self.make_export()
        size = build.format_size(build.demo_download_size(demo))
        page = (self.run_build(demo_dir=demo) / "index.html").read_text()
        self.assertIn(f"Play the intro level ({size})", page)

    def test_demo_dir_without_extension_fails(self):
        demo = self.make_export()
        (demo / build.DEMO_EXTENSION).unlink()
        with self.assertRaisesRegex(build.BuildError, build.DEMO_EXTENSION):
            self.run_build(demo_dir=demo)

    def test_demo_dir_without_export_fails(self):
        with self.assertRaises(build.BuildError):
            self.run_build(demo_dir=self.tmp)

    def test_rejects_bad_release_data(self):
        def without_linux(d):
            d["platforms"] = [p for p in d["platforms"] if p["key"] != "linux"]

        def bad_sha(d):
            d["platforms"][0]["sha256"] = "abc"

        def http_url(d):
            d["platforms"][0]["url"] = "http://example.org/x"

        def no_version(d):
            d["version"] = None

        def bad_store(d):
            d["platforms"][3]["store"] = "itch"

        for mutate in (without_linux, bad_sha, http_url, no_version, bad_store):
            data = release()
            mutate(data)
            with self.subTest(mutate.__name__), self.assertRaises(build.BuildError):
                self.run_build(data)


class MarkdownTest(unittest.TestCase):
    def test_subset(self):
        html_out = build.render_markdown(
            "# Title\n\nIntro *game*\nwraps.\n\n## People\n\n- One\n  continued\n- [Two](https://x.org) `c`\n")
        self.assertEqual(html_out, "\n".join([
            "<p>Intro <cite>game</cite> wraps.</p>",
            '<h2 id="people">People</h2>',
            "<ul>",
            "<li>One continued</li>",
            '<li><a href="https://x.org">Two</a> <code>c</code></li>',
            "</ul>",
        ]))

    def test_escapes_html(self):
        self.assertIn("&lt;script&gt;", build.render_markdown("<script>x</script>"))

    def test_rejects_unsafe_links(self):
        with self.assertRaises(build.BuildError):
            build.render_markdown("[x](javascript:alert(1))")


if __name__ == "__main__":
    unittest.main()

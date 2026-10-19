# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Tests for tools/release/changelog.py against a throwaway git repository."""

import os
import subprocess
import sys
import tempfile
import unittest

SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "changelog.py")


class ChangelogTest(unittest.TestCase):
    def setUp(self):
        self._dir = tempfile.TemporaryDirectory()
        self.repo = self._dir.name
        self.git("init", "-q", "-b", "main")
        self.git("config", "user.name", "Test")
        self.git("config", "user.email", "test@example.invalid")
        self.git("config", "commit.gpgsign", "false")
        self.git("config", "tag.gpgsign", "false")

    def tearDown(self):
        self._dir.cleanup()

    def git(self, *args):
        env = dict(os.environ, GIT_COMMITTER_DATE="2026-10-19T12:00:00Z", GIT_AUTHOR_DATE="2026-10-19T12:00:00Z")
        return subprocess.run(["git", *args], cwd=self.repo, check=True, capture_output=True, text=True, env=env)

    def commit(self, message):
        self.git("commit", "-q", "--allow-empty", "-m", message)

    def run_script(self, *args, check=True):
        result = subprocess.run([sys.executable, SCRIPT, *args], cwd=self.repo, capture_output=True, text=True)
        if check:
            self.assertEqual(result.returncode, 0, result.stderr)
        return result

    def test_notes_group_by_type_since_previous_tag(self):
        self.commit("feat(core): first rules")
        self.git("tag", "v0.1.0")
        self.commit("fix(board): swap bounce (#5)")
        self.commit("feat(audio): music")
        self.commit("perf(render): fewer draw calls")
        self.commit("docs: how to play")
        self.commit("not a conventional title")
        self.git("tag", "v0.2.0")
        self.commit("feat: after the release")

        out = self.run_script("notes", "--ref", "v0.2.0").stdout
        self.assertEqual(
            out,
            "### Features\n\n- **audio:** music\n\n"
            "### Fixes\n\n- **board:** swap bounce (#5)\n\n"
            "### Performance\n\n- **render:** fewer draw calls\n\n"
            "### Other changes\n\n- how to play\n- not a conventional title\n",
        )

    def test_breaking_changes_come_first(self):
        self.commit("feat: first release")
        self.git("tag", "v0.1.0")
        self.commit("feat(core)!: new level format")
        self.commit("fix: old saves\n\nBREAKING CHANGE: saves from 0.x no longer load")
        self.commit("feat: plain")
        out = self.run_script("notes").stdout
        self.assertTrue(out.startswith("### Breaking changes\n\n- **core:** new level format\n- old saves\n"), out)
        self.assertIn("### Features\n\n- plain\n", out)

    def test_first_release_has_no_breaking_changes(self):
        self.commit("feat(core)!: new level format")
        self.commit("fix: old saves\n\nBREAKING CHANGE: saves from 0.x no longer load")
        out = self.run_script("notes").stdout
        self.assertEqual(out, "### Features\n\n- **core:** new level format\n\n### Fixes\n\n- old saves\n")

    def test_release_commit_is_left_out(self):
        self.commit("feat: kept")
        self.commit("chore(release): v1.0.0")
        out = self.run_script("notes").stdout
        self.assertEqual(out, "### Features\n\n- kept\n")

    def test_only_semver_tags_count_and_order_by_version(self):
        self.commit("feat: one")
        self.git("tag", "v0.9.0")
        self.commit("feat: two")
        self.git("tag", "v0.10.0")
        self.git("tag", "vnext")
        self.commit("feat: three")
        self.git("tag", "v0.10.1-rc1")
        out = self.run_script("notes").stdout
        self.assertEqual(out, "### Features\n\n- three\n")

    def test_changelog_lists_every_release_newest_first(self):
        self.commit("feat: one")
        self.git("tag", "v0.1.0")
        self.commit("fix: two")
        self.git("tag", "v0.1.1")
        self.commit("feat: three")
        self.commit("chore(release): v0.2.0")
        out = self.run_script("changelog", "--next", "0.2.0").stdout
        self.assertIn("# Changelog\n", out)
        body = out[out.index("## "):]
        self.assertEqual(
            body,
            "## v0.2.0 (2026-10-19)\n\n### Features\n\n- three\n\n"
            "## v0.1.1 (2026-10-19)\n\n### Fixes\n\n- two\n\n"
            "## v0.1.0 (2026-10-19)\n\n### Features\n\n- one\n",
        )
        # Once the release commit is tagged, the same file comes out.
        self.git("tag", "v0.2.0")
        self.assertEqual(self.run_script("changelog", "--next", "0.2.0").stdout, out)
        self.assertEqual(self.run_script("changelog").stdout, out)

    def test_empty_release_says_so(self):
        self.commit("chore(release): v0.1.0")
        self.assertEqual(self.run_script("notes").stdout, "No changes.\n")

    def test_check_tag(self):
        self.assertEqual(self.run_script("check-tag", "v1.2.3").returncode, 0)
        for bad in ("1.2.3", "v1.2", "v01.2.3", "v1.2.3-rc1"):
            self.assertNotEqual(self.run_script("check-tag", bad, check=False).returncode, 0, bad)
        self.assertNotEqual(self.run_script("changelog", "--next", "v1.0.0", check=False).returncode, 0)


if __name__ == "__main__":
    unittest.main()

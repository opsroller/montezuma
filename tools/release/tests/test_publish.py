# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Tests for tools/release/publish.py (--dry-run) against a throwaway repository."""

import hashlib
import os
import subprocess
import sys
import tempfile
import unittest

SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "publish.py")
TARGETS = ["linux", "windows", "macos", "web"]
NAMES = [
    "montezuma-1.2.0-linux-x86_64.tar.gz",
    "montezuma-1.2.0-windows-x86_64.zip",
    "montezuma-1.2.0-macos-universal.zip",
    "montezuma-1.2.0-web.zip",
]


class PublishTest(unittest.TestCase):
    def setUp(self):
        self._dir = tempfile.TemporaryDirectory()
        self.repo = self._dir.name
        self.git("init", "-q", "-b", "main")
        self.git("config", "user.name", "Test")
        self.git("config", "user.email", "test@example.invalid")
        self.git("config", "tag.gpgsign", "false")
        self.git("commit", "-q", "--allow-empty", "-m", "feat: first")
        self.git("tag", "v1.1.0")
        self.git("commit", "-q", "--allow-empty", "-m", "fix(board): swap bounce")
        self.git("tag", "v1.2.0")
        for i, (target, name) in enumerate(zip(TARGETS, NAMES)):
            self.package(os.path.join("dist", target), name, f"bytes {i}".encode())

    def tearDown(self):
        self._dir.cleanup()

    def git(self, *args):
        env = dict(os.environ, GIT_COMMITTER_DATE="2026-10-19T12:00:00Z", GIT_AUTHOR_DATE="2026-10-19T12:00:00Z")
        subprocess.run(["git", "-c", "commit.gpgsign=false", *args], cwd=self.repo, check=True,
                       capture_output=True, env=env)

    def package(self, folder, name, data, digest=None):
        path = os.path.join(self.repo, folder)
        os.makedirs(path, exist_ok=True)
        with open(os.path.join(path, name), "wb") as f:
            f.write(data)
        with open(os.path.join(path, name + ".sha256"), "w") as f:
            f.write(f"{digest or hashlib.sha256(data).hexdigest()}  {name}\n")

    def run_script(self, *args):
        return subprocess.run([sys.executable, SCRIPT, *args], cwd=self.repo, capture_output=True, text=True)

    def test_dry_run_lists_every_package_and_writes_notes_and_sums(self):
        result = self.run_script("--tag", "v1.2.0", "--dry-run", "dist")
        self.assertEqual(result.returncode, 0, result.stderr)
        for name in NAMES:
            self.assertIn(name, result.stdout)
            self.assertIn(name + ".sha256", result.stdout)
        with open(os.path.join(self.repo, "dist", "release", "SHA256SUMS")) as f:
            sums = f.read().splitlines()
        self.assertEqual([line.split("  ")[1] for line in sums], NAMES)
        with open(os.path.join(self.repo, "dist", "release", "notes.md")) as f:
            notes = f.read()
        self.assertTrue(notes.startswith("### Fixes\n\n- **board:** swap bounce\n"))
        self.assertIn("https://github.com/opsroller/montezuma/tree/v1.2.0", notes)
        self.assertNotIn("first", notes)

    def test_missing_target_fails(self):
        os.remove(os.path.join(self.repo, "dist", "web", NAMES[3]))
        result = self.run_script("--tag", "v1.2.0", "--dry-run", "dist")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing release packages: web", result.stderr)

    def test_only_narrows_the_targets(self):
        result = self.run_script("--tag", "v1.2.0", "--only", "linux,web", "--dry-run", "dist")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("windows", result.stdout)

    def test_bad_checksum_fails(self):
        self.package("dist/linux", NAMES[0], b"other bytes", digest="0" * 64)
        result = self.run_script("--tag", "v1.2.0", "--dry-run", "dist")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("checksum does not match", result.stderr)

    def test_package_of_another_version_fails(self):
        self.package("dist/old", "montezuma-1.1.0-web.zip", b"old")
        result = self.run_script("--tag", "v1.2.0", "--dry-run", "dist")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("another version", result.stderr)

    def test_unknown_or_bad_tag_fails(self):
        self.assertIn("not a release tag", self.run_script("--tag", "1.2.0", "--dry-run", "dist").stderr)
        self.assertIn("no such tag", self.run_script("--tag", "v9.9.9", "--dry-run", "dist").stderr)


if __name__ == "__main__":
    unittest.main()

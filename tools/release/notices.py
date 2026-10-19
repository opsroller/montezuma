#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Write the notices every build ships with.

Usage: notices.py <outdir>
  <outdir>/THIRD-PARTY-NOTICES.txt  licenses of the engine and libraries in the build
  <outdir>/SOURCE.txt               where to get the exact source of the build

Run from the repository root. The desktop packages and the web demo both
use this, so they ship the same text.
"""

import os
import subprocess
import sys

REPO_URL = "https://github.com/opsroller/montezuma"

# (component, license file) in the order they appear. Add a row for every
# library linked into the game.
COMPONENTS = [
    ("Godot Engine 4.7.2 (MIT)", "tools/release/notices/godot/LICENSE.txt"),
    ("godot-cpp (MIT)", "thirdparty/godot-cpp/LICENSE.md"),
    ("nlohmann/json 3.11.3 (MIT)", "thirdparty/nlohmann/LICENSE.MIT"),
    ("Third-party code included in Godot Engine 4.7.2",
     "tools/release/notices/godot/COPYRIGHT.txt"),
]

HEADER = """Montezuma's is free software under the GNU General Public License,
version 2 or (at your option) any later version; see LICENSE.txt.
Its source is available as described in SOURCE.txt.

It is built with the following third-party software, whose notices follow.
"""

SOURCE = """Montezuma's {version}

This build was made from commit {commit} of
{repo}

The complete corresponding source, including the build scripts and the
godot-cpp submodule, is at:
{repo}/tree/{commit}

Montezuma's is free software under the GNU General Public License,
version 2 or (at your option) any later version.
"""


def git(*args):
    return subprocess.run(["git", *args], check=True, capture_output=True,
                          text=True).stdout.strip()


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    outdir = argv[1]
    os.makedirs(outdir, exist_ok=True)

    parts = [HEADER]
    for name, path in COMPONENTS:
        with open(path, encoding="utf-8") as f:
            text = f.read().strip()
        rule = "=" * 72
        parts.append(f"{rule}\n{name}\n{rule}\n\n{text}\n")
    with open(os.path.join(outdir, "THIRD-PARTY-NOTICES.txt"), "w",
              encoding="utf-8", newline="\n") as f:
        f.write("\n".join(parts))

    version = subprocess.run(["cmake", "-P", "tools/release/version.cmake"],
                             check=True, capture_output=True, text=True,
                             timeout=120).stdout.strip()
    with open(os.path.join(outdir, "SOURCE.txt"), "w", encoding="utf-8",
              newline="\n") as f:
        f.write(SOURCE.format(version=version, commit=git("rev-parse", "HEAD"),
                              repo=REPO_URL))


if __name__ == "__main__":
    main(sys.argv)

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Put the credits and license files into the Godot project, or check an
export carries them.

Usage:
  game-files.py stage <gamedir>       copy them into <gamedir> (res://)
  game-files.py check <gamedir> <export>
                                      exit 1 unless every staged file's bytes
                                      are inside <export> (a file, or a
                                      folder such as a macOS .app)

The game reads them as res://CREDITS.md (credits screen),
res://THIRD-PARTY-NOTICES.txt, res://LICENSE.txt and res://SOURCE.txt.
They are generated, not committed; game/export_presets.cfg includes them
by name. Run from the repository root.
"""

import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# Name inside the game -> where it comes from (None: written by notices.py).
FILES = {
    "CREDITS.md": "CREDITS.md",
    "LICENSE.txt": "LICENSE",
    "THIRD-PARTY-NOTICES.txt": None,
    "SOURCE.txt": None,
}


def stage(gamedir):
    for name, source in FILES.items():
        if source:
            shutil.copyfile(source, os.path.join(gamedir, name))
    subprocess.run([sys.executable, os.path.join(HERE, "notices.py"), gamedir],
                   check=True)


def export_blobs(export):
    if os.path.isfile(export):
        yield export
        return
    for root, _, names in os.walk(export):
        for name in names:
            yield os.path.join(root, name)


def check(gamedir, export):
    wanted = {}
    for name in FILES:
        with open(os.path.join(gamedir, name), "rb") as f:
            wanted[name] = f.read()
    for path in export_blobs(export):
        with open(path, "rb") as f:
            data = f.read()
        wanted = {n: b for n, b in wanted.items() if b not in data}
        if not wanted:
            return
    sys.exit(f"{export}: not packed: {', '.join(sorted(wanted))} "
             "(check include_filter in game/export_presets.cfg)")


def main(argv):
    if len(argv) == 3 and argv[1] == "stage":
        stage(argv[2])
    elif len(argv) == 4 and argv[1] == "check":
        check(argv[2], argv[3])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv)

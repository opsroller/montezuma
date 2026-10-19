#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Swap the release and debug binaries in an installed macOS export template.

Usage: macos-template-swap.py <macos.zip>

Godot 4.8 beta 1's macOS template holds one binary per architecture
(godot_macos_{release,debug}.{arm64,x86_64}), and its exporter, for a
universal export, keeps the binaries of the other variant: a release
export gets the debug binaries, which then look for the extension's
template_debug library. Swapping the names gives each export the right
binaries. Run once on a freshly installed template (fetch-godot.cmake does,
when GODOT_MACOS_SWAP_TEMPLATE_VARIANTS=1 in godot.env); remove it with
that setting once Godot fixes the exporter.
"""

import os
import sys
import zipfile

PREFIX = "macos_template.app/Contents/MacOS/godot_macos_"


def swapped(name):
    if name.startswith(PREFIX + "release."):
        return PREFIX + "debug." + name[len(PREFIX + "release."):]
    if name.startswith(PREFIX + "debug."):
        return PREFIX + "release." + name[len(PREFIX + "debug."):]
    return name


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    path = argv[1]
    tmp = path + ".swap"
    count = 0
    with zipfile.ZipFile(path) as src, zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED) as dst:
        for info in src.infolist():
            data = src.read(info)
            name = swapped(info.filename)
            count += name != info.filename
            info.filename = name
            dst.writestr(info, data)
    if count != 4:
        os.remove(tmp)
        sys.exit(f"{path}: expected 4 per-architecture binaries to swap, found {count}")
    os.replace(tmp, path)
    print(f"{path}: swapped release and debug binaries")


if __name__ == "__main__":
    main(sys.argv)

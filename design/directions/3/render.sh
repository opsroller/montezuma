#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Regenerate direction 3: python3 build.py . then render PNGs with headless Chromium.
set -e; D=$(cd "$(dirname "$0")" && pwd); T=$(mktemp -d)
python3 "$D/build.py" "$D"
r() { printf '<!doctype html><body style="margin:0"><img src="file://%s" style="display:block;width:%spx;height:%spx">' "$1" "$3" "$4" > "$T/p.html"
      chromium --headless --disable-gpu --no-sandbox --hide-scrollbars --default-background-color=00000000 --window-size="$3,$4" --screenshot="$2" "$T/p.html" 2>/dev/null; }
r "$D/app-icon.svg" "$D/icon-1024.png" 1024 1024
r "$D/app-icon.svg" "$D/icon-48.png" 48 48
r "$D/mark-small.svg" "$D/icon-16.png" 16 16
r "$D/lockup.svg" "$D/lockup.png" 1050 340
rm -r "$T"

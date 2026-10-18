#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Checks a packaged desktop build: verifies its checksum, unpacks it, runs
# the game headless for 120 frames and fails on any engine error (for
# example the GDExtension library missing or not loading).
#
# Usage: tools/release/smoke-test.sh <archive>   (the .sha256 file sits beside it)
# Runs on the archive's own platform: Linux, Windows (Git Bash) or macOS.
set -euo pipefail

archive="$(cd "$(dirname "${1:?usage: smoke-test.sh <archive>}")" && pwd)/$(basename "$1")"
cd "$(dirname "$archive")"
if command -v sha256sum >/dev/null; then
  sha256sum -c "$(basename "$archive").sha256"
else
  shasum -a 256 -c "$(basename "$archive").sha256"
fi

work="$(mktemp -d)"
case "$archive" in
  *.tar.gz) tar -xzf "$archive" -C "$work" ;;
  *.zip)
    if [[ "${OSTYPE:-}" == msys* || "${OSTYPE:-}" == cygwin* ]]; then
      pwsh -NoProfile -Command "Expand-Archive -LiteralPath '$(cygpath -w "$archive")' -DestinationPath '$(cygpath -w "$work")'"
    else
      unzip -q "$archive" -d "$work"
    fi ;;
  *) echo "unknown archive type: $archive" >&2; exit 2 ;;
esac

name="$(basename "$archive")"; name="${name%.tar.gz}"; name="${name%.zip}"
case "$name" in
  *-linux-*)   game="$work/$name/montezuma.x86_64" ;;
  *-windows-*) game="$work/$name/montezuma.exe" ;;
  *-macos-*)   game="$work/$name/Montezuma's.app/Contents/MacOS/Montezuma's" ;;
  *) echo "unknown platform in $name" >&2; exit 2 ;;
esac
[[ -f "$game" ]] || { echo "game binary missing: $game" >&2; exit 1; }

log="$work/smoke.log"
log_arg="$log"
if command -v cygpath >/dev/null; then log_arg="$(cygpath -w "$log")"; fi
"$game" --headless --quit-after 120 --log-file "$log_arg" || { cat "$log" 2>/dev/null; exit 1; }
cat "$log"
grep -q "Godot Engine" "$log" || { echo "the game did not start" >&2; exit 1; }
if grep -E "ERROR|SCRIPT ERROR|Can't open dynamic library" "$log"; then
  echo "engine errors during the smoke run" >&2
  exit 1
fi
echo "smoke test passed: $name"

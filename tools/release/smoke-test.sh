#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Checks a packaged desktop build: verifies its checksum, unpacks it, runs
# the game headless for 120 frames and fails on any engine error (for
# example the GDExtension library missing or not loading). Given a
# <screens-dir>, it then renders the title screen and the board with
# Godot's Movie Maker at the project's window size (1280x720) and saves
# them there as title.png and board.png.
#
# Usage: tools/release/smoke-test.sh <archive> [<screens-dir>]
#   (the .sha256 file sits beside the archive)
# Runs on the archive's own platform: Linux, Windows (Git Bash) or macOS.
# Screenshots need a display: on Linux run the script under xvfb-run.
set -euo pipefail

archive="$(cd "$(dirname "${1:?usage: smoke-test.sh <archive> [<screens-dir>]}")" && pwd)/$(basename "$1")"
screens=""
if [[ -n "${2:-}" ]]; then mkdir -p "$2" && screens="$(cd "$2" && pwd)"; fi
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
[[ -n "$screens" ]] || exit 0

winpath() { if command -v cygpath >/dev/null; then cygpath -w "$1"; else echo "$1"; fi; }
# Renderers that work on GPU-less CI runners, tried in order: Mesa under
# Xvfb on Linux, ANGLE over Direct3D 11 (WARP) on Windows. On macOS
# virtual machines Metal has crashed (Godot 4.4: no argument encoders on
# the paravirtual GPU) and Vulkan runs out of device memory, so a Metal
# failure falls back to OpenGL, which there is Apple's software renderer
# (several seconds a frame): keep the frame counts low.
case "$name" in
  *-linux-*)   drivers=(opengl3) ;;
  *-windows-*) drivers=(opengl3_angle) ;;
  *)           drivers=(metal opengl3) ;;
esac

capture() {  # capture <shot> <frames> [<game user arg>...]: keeps the last frame
  local shot="$1" frames="$2" dir="$work/movie-$1" slog="$work/$1.log" driver
  shift 2
  for driver in "${drivers[@]}"; do
    rm -rf "$dir" && mkdir -p "$dir"
    if "$game" --rendering-driver "$driver" --fixed-fps 30 --quit-after "$frames" \
      --write-movie "$(winpath "$dir/frame.png")" --log-file "$(winpath "$slog")" -- "$@"; then
      local last
      last="$(find "$dir" -name 'frame*.png' | sort | tail -n 1)"
      if [[ -s "$last" ]]; then
        cp "$last" "$screens/$shot.png"
        echo "captured $shot.png with $driver from $(basename "$last")"
        return
      fi
    fi
    cat "$slog" 2>/dev/null
    echo "no frames rendered for $shot with $driver" >&2
  done
  exit 1
}

# 15 frames (0.5 s) let the title fade in; 30 frames (1 s) let the board
# settle. --start-level plays the first level, so the board shows with the
# in-level HUD over it.
capture title 15
capture board 30 --start-level=e1-l01

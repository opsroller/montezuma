#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Runs the frame-time benchmark and, with --readability, the readability
# capture on this machine, then the checks. How-to:
# docs/reference/benchmark.md and docs/how-to/run-the-benchmark.md.
#
# Usage: tools/bench/run_bench.sh [options]
#   --godot PATH      Godot 4.7.2 editor binary (default: godot on PATH)
#   --tier NAME       desktop-high, desktop-standard, mobile or web; checks the
#                     feature list against it (default: unset, no check)
#   --driver NAME     --rendering-driver for Godot, e.g. vulkan, metal, d3d12,
#                     opengl3 (default: the project's)
#   --size WxH        window size for the benchmark (default 1920x1080)
#   --seconds N       capture length (default 60)
#   --warmup N        warm-up before the capture (default 10)
#   --out DIR         where reports and captures go (default bench-out/)
#   --readability     also capture at 540x1170 and check rules 2, 4, 5 and 6
#   --xvfb            run under xvfb-run (machines without a display)
# Exit status: 0 when every run and check passed, 1 otherwise.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
godot=godot tier=unset driver="" size=1920x1080 seconds=60 warmup=10 out=bench-out readability=0 xvfb=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --godot) godot="$2"; shift 2 ;;
    --tier) tier="$2"; shift 2 ;;
    --driver) driver="$2"; shift 2 ;;
    --size) size="$2"; shift 2 ;;
    --seconds) seconds="$2"; shift 2 ;;
    --warmup) warmup="$2"; shift 2 ;;
    --out) out="$2"; shift 2 ;;
    --readability) readability=1; shift ;;
    --xvfb) xvfb=1; shift ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done
mkdir -p "$out"
out="$(cd "$out" && pwd)"

# Since Godot 4.7, the window size comes from an override.cfg next to
# project.godot; keep any the user has and put it back afterwards.
cfg="$root/game/override.cfg"
saved=""
if [[ -f "$cfg" ]]; then saved="$(mktemp)"; cp "$cfg" "$saved"; fi
restore() { if [[ -n "$saved" ]]; then mv "$saved" "$cfg"; else rm -f "$cfg"; fi; }
trap restore EXIT
window() {
  printf '[display]\nwindow/size/window_width_override=%s\nwindow/size/window_height_override=%s\n' "${1%x*}" "${1#*x}" > "$cfg"
}

run_godot() {
  local cmd=("$godot" --path "$root/game")
  [[ -n "$driver" ]] && cmd+=(--rendering-driver "$driver")
  cmd+=("$@")
  if [[ "$xvfb" == 1 ]]; then cmd=(xvfb-run -a -s "-screen 0 2048x2048x24" "${cmd[@]}"); fi
  "${cmd[@]}"
}

status=0
window "$size"
run_godot res://scenes/bench/bench.tscn -- "--bench-seconds=$seconds" "--bench-warmup=$warmup" \
  "--bench-tier=$tier" "--bench-out=$out/benchmark.json" || status=1
if [[ "$tier" != unset && -f "$out/benchmark.json" ]]; then
  python3 "$root/tools/bench/check_features.py" "$out/benchmark.json" | tee "$out/features.txt" || status=1
fi

if [[ "$readability" == 1 ]]; then
  window 540x1170
  run_godot res://scenes/bench/readability.tscn -- "--capture-out=$out/readability" || status=1
  python3 "$root/tools/bench/check_readability.py" "$out/readability" --json "$out/readability.json" \
    | tee "$out/readability.txt" || status=1
fi
exit "$status"

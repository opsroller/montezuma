#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Check a benchmark run: frame-time budget, render size and replay.

Usage: check_run.py REPORT [--size WxH] [--no-budget]

REPORT is the schema-2 JSON game/scenes/bench/bench.tscn writes. The budget
verdict is the runner's (report.budget: frame_ms percentiles against whole
60 Hz periods with a tolerance, docs/reference/benchmark.md#budget); this
prints it with its reason. --size also requires the run to have been locked
at that render size. --no-budget reports the budget without judging it
(headless or software-rendered runs, whose times mean nothing). Prints one
line per check, exits 1 when any fails, 2 when the report is unreadable.
Standard library only.
"""

import argparse
import json
import re
import sys
from pathlib import Path


def size(text):
    m = re.fullmatch(r"([1-9]\d{0,4})x([1-9]\d{0,4})", text)
    if not m:
        raise argparse.ArgumentTypeError(f"size must look like 1920x1080, got {text!r}")
    return [int(m.group(1)), int(m.group(2))]


def wh(v):
    return f"{v[0]}x{v[1]}" if isinstance(v, list) and len(v) == 2 else "none"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("report", type=Path)
    ap.add_argument("--size", type=size, help="render size the run must have been locked at")
    ap.add_argument("--no-budget", action="store_true", help="report the budget, do not judge it")
    args = ap.parse_args(argv)

    report = json.loads(args.report.read_text())
    if report.get("schema") != 2 or not all(k in report for k in ("budget", "resolution", "timing", "frame_ms")):
        print(f"{args.report}: not a schema-2 benchmark report (schema {report.get('schema')!r}); "
              "rebuild the extension and run again", file=sys.stderr)
        return 2

    failed = 0

    def line(ok, name, text):
        nonlocal failed
        failed += 0 if ok else 1
        print(f"{'PASS' if ok else 'FAIL'} {name}: {text}")

    budget = report["budget"]
    limits = ", ".join(f"p{l['percentile']:.0f} {l['value_ms']:.2f} ms (limit {l['limit_ms']:.2f})"
                       for l in budget.get("limits", []))
    summary = f"{report.get('tier')}, {budget.get('metric')} {limits}; " \
              f"{budget.get('target_hz'):.0f} Hz target, tolerance {budget.get('tolerance', 0) * 100:.0f}%"
    if args.no_budget:
        print(f"SKIP budget: {summary}")
    else:
        line(bool(budget.get("pass")), "budget", summary if budget.get("pass") else f"{budget.get('reason')}; {summary}")

    res = report["resolution"]
    text = f"rendered {wh(res.get('render'))} (min {wh(res.get('min'))}, max {wh(res.get('max'))}), " \
           f"requested {wh(res.get('requested'))}, {res.get('mismatched_frames')} mismatched frames"
    ok = bool(res.get("pass"))
    if args.size is not None:
        if not res.get("locked") or res.get("requested") is None:
            ok, text = False, f"not locked at {wh(args.size)}; {text}"
        elif res.get("requested") != args.size or res.get("render") != args.size:
            ok, text = False, f"wanted {wh(args.size)}; {text}"
    line(ok, "resolution", text)

    line(not report.get("replay", {}).get("diverged", True), "replay",
         "diverged from the plan" if report.get("replay", {}).get("diverged", True) else "matched the plan")

    timing = report["timing"]
    display = report.get("display", {})
    print(f"NOTE display: {display.get('refresh_hz')} Hz, vsync {display.get('vsync')}")
    if not timing.get("gpu"):
        print(f"NOTE gpu: {timing.get('gpu_note')}; judge GPU cost from frame_ms with vsync off")
    if timing.get("render_cpu_includes_display_wait"):
        p95 = lambda k: report.get(k, {}).get("p95", 0.0)  # noqa: E731
        print(f"NOTE cpu: render_cpu_ms includes the display wait on this driver, so cpu_ms is not CPU cost; "
              f"game logic (process_ms) p95 {p95('process_ms'):.2f} ms, render CPU p95 {p95('render_cpu_ms'):.2f} ms")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

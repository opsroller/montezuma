#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Check a benchmark run: frame-time budget, draw calls, render size and replay.

Usage: check_run.py REPORT [--size WxH] [--no-budget]

REPORT is the schema-2 JSON game/scenes/bench/bench.tscn writes. The budget
is frame_ms percentiles against whole 60 Hz periods with a tolerance
(docs/reference/benchmark.md#budget); this re-checks the runner's verdict
against the tier's rules and the numbers in the report rather than taking
its pass flag (a display slower than 60 Hz with vsync on included), and
never passes headless or software-rendered frames.
--size also requires the run to have been locked at that render size.
--no-budget reports the budget without judging it (CI smoke runs). Draw
calls are a count, the same on any GPU or a software renderer, so their
p95 is judged against the tier's cap whenever frames were drawn, with or
without --no-budget. Prints
one line per check, exits 1 when any fails, 2 when the report is
unreadable or inconsistent. Standard library only.
"""

import argparse
import json
import re
import sys
from pathlib import Path

PERIOD_MS = 1000.0 / 60.0
MAX_TOLERANCE = 0.5
MIN_FRAMES = 30
# Percentile -> budget in 60 Hz periods, per tier (visual-target.md, quality tiers).
RULES = {
    "desktop-high": {95.0: 1},
    "desktop-standard": {95.0: 1},
    "mobile": {95.0: 1},
    "web": {95.0: 2, 50.0: 1},
}
PERCENTILE_KEYS = {50.0: "p50", 95.0: "p95", 99.0: "p99"}
# Draw calls per frame, per tier (visual-target.md, content budgets).
DRAW_CALL_CAPS = {"desktop-high": 2000, "desktop-standard": 1000, "mobile": 300, "web": 200}


class Unreadable(Exception):
    pass


def size(text):
    m = re.fullmatch(r"([1-9]\d{0,4})x([1-9]\d{0,4})", text)
    if not m:
        raise argparse.ArgumentTypeError(f"size must look like 1920x1080, got {text!r}")
    return [int(m.group(1)), int(m.group(2))]


def wh(v):
    return f"{v[0]}x{v[1]}" if isinstance(v, list) and len(v) == 2 else "none"


def load(path):
    try:
        report = json.loads(path.read_text())
    except (OSError, ValueError) as e:
        raise Unreadable(f"{path}: cannot read a JSON report ({e})")
    if not isinstance(report, dict):
        raise Unreadable(f"{path}: the report is not a JSON object")
    if report.get("schema") != 2 or not all(
            isinstance(report.get(k), dict) for k in ("budget", "resolution", "timing", "frame_ms", "device")):
        raise Unreadable(f"{path}: not a schema-2 benchmark report (schema {report.get('schema')!r}); "
                         "rebuild the extension and run again")
    for l in report["budget"].get("limits", []):
        if not isinstance(l, dict) or not all(isinstance(l.get(k), (int, float)) and not isinstance(l.get(k), bool)
                                              for k in ("percentile", "budget_ms", "limit_ms", "value_ms")):
            raise Unreadable(f"{path}: a budget limit is not numbers: {l!r}")
    timing = report["timing"]
    if not timing.get("gpu") and report.get("gpu_ms") is not None:
        raise Unreadable(f"{path}: gpu_ms holds numbers although timing.gpu is false; "
                         "those zeros are not a measurement")
    return report


def budget_problems(report):
    """Why the budget does not pass, re-derived from the report; [] on a pass."""
    budget, frames = report["budget"], report["frame_ms"]
    tier = report.get("tier")
    if tier not in RULES:
        return [f"unknown tier {tier!r}: no budget to judge against"]
    device = report["device"]
    if device.get("headless"):
        return ["not a measurement: headless, nothing rendered"]
    if device.get("software"):
        return [f"not a measurement: software renderer ({device.get('adapter')}), frame times are not a GPU's"]
    if report.get("ablation"):
        return [f"not a measurement: ablation run with {', '.join(report['ablation'])} off, not the tier"]
    problems = []
    tol = budget.get("tolerance")
    if not isinstance(tol, (int, float)) or not 0 <= tol <= MAX_TOLERANCE:
        problems.append(f"tolerance {tol!r} is outside 0 to {MAX_TOLERANCE}")
        tol = 0.0
    if not isinstance(frames.get("frames"), int) or frames["frames"] < MIN_FRAMES:
        problems.append(f"only {frames.get('frames')} frames captured, fewer than {MIN_FRAMES}")
    limits = {l.get("percentile"): l for l in budget.get("limits", []) if isinstance(l, dict)}
    for pct, periods in RULES[tier].items():
        l = limits.get(pct)
        value = frames.get(PERCENTILE_KEYS[pct])
        if l is None or not isinstance(value, (int, float)):
            problems.append(f"no p{pct:.0f} rule in the report, which {tier} needs")
            continue
        budget_ms = l.get("budget_ms")
        if not isinstance(budget_ms, (int, float)) or budget_ms > periods * PERIOD_MS + 1e-6:
            problems.append(f"p{pct:.0f} budget {budget_ms!r} ms is looser than {periods} period(s) "
                            f"({periods * PERIOD_MS:.2f} ms)")
            continue
        limit = budget_ms * (1 + tol)
        if value > limit + 1e-9 or l.get("value_ms", value) > limit + 1e-9:
            problems.append(f"frame_ms p{pct:.0f} {value:.2f} ms over {limit:.2f} ms "
                            f"({budget_ms:.2f} ms budget + {tol * 100:.0f}%)")
    display = report.get("display") or {}
    hz = display.get("refresh_hz")
    if display.get("vsync") != "disabled" and isinstance(hz, (int, float)) and 0 < hz < 60.0 * (1 - tol):
        problems.append(f"display-limited: the display refreshes at {hz:.2f} Hz, below the 60 Hz target")
    if not problems and not budget.get("pass"):
        problems.append(budget.get("reason") or "the runner failed the budget")
    return problems


def draw_call_line(report):
    """(problems, text) for the draw-call cap, or None when nothing was drawn."""
    render = report.get("render")
    if report["device"].get("headless") or not isinstance(render, dict):
        return None
    p95, worst = render.get("draw_calls_p95"), render.get("draw_calls_max")
    if not isinstance(p95, (int, float)):
        return ["no draw_calls_p95 in the report"], ""
    cap = DRAW_CALL_CAPS.get(report.get("tier"))
    text = f"p95 {p95:.0f}, max {f'{worst:.0f}' if isinstance(worst, (int, float)) else '?'}"
    passes = render.get("draw_calls_by_pass")
    if isinstance(passes, dict):
        text += " (" + ", ".join(f"{k.removesuffix('_p95')} {v:.0f}" for k, v in passes.items()
                                  if isinstance(v, (int, float))) + ")"
    if cap is None:
        return [f"no draw-call cap for tier {report.get('tier')!r}"], text
    text += f"; cap {cap}"
    return ([f"p95 {p95:.0f} is over the {report.get('tier')} cap of {cap}"] if p95 > cap else []), text


def resolution_problems(res, want):
    problems = []
    render, lo, hi = res.get("render"), res.get("min"), res.get("max")
    if not isinstance(res.get("mismatched_frames"), int) or res["mismatched_frames"] != 0:
        problems.append(f"{res.get('mismatched_frames')} frames at another render size")
    if lo != render or hi != render:
        problems.append(f"render size moved between {wh(lo)} and {wh(hi)}")
    requested = res.get("requested")
    if requested is not None and requested != render:
        problems.append(f"rendered {wh(render)}, not the requested {wh(requested)}")
    window = res.get("window") or {}
    if window.get("changed_frames", 0) != 0 or window.get("min") != window.get("max"):
        problems.append(f"window resized during the capture ({wh(window.get('min'))} to {wh(window.get('max'))}, "
                        f"{window.get('changed_frames')} frames)")
    if want is not None:
        if not res.get("locked") or requested is None:
            problems.append(f"not locked at {wh(want)}")
        elif requested != want or render != want:
            problems.append(f"wanted {wh(want)}")
    if not problems and not res.get("pass"):
        problems.append("the runner failed the render size")
    return problems


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("report", type=Path)
    ap.add_argument("--size", type=size, help="render size the run must have been locked at")
    ap.add_argument("--no-budget", action="store_true", help="report the budget, do not judge it")
    args = ap.parse_args(argv)

    try:
        report = load(args.report)
    except Unreadable as e:
        print(e, file=sys.stderr)
        return 2

    failed = 0

    def line(problems, name, text):
        nonlocal failed
        failed += 1 if problems else 0
        print(f"{'FAIL' if problems else 'PASS'} {name}: {'; '.join(problems + [text]) if problems else text}")

    budget, frames = report["budget"], report["frame_ms"]
    limits = ", ".join(f"p{l.get('percentile', 0):.0f} {l.get('value_ms', 0):.2f} ms (limit {l.get('limit_ms', 0):.2f})"
                       for l in budget.get("limits", []) if isinstance(l, dict))
    summary = f"{report.get('tier')}, frame_ms {limits or 'no limits'}; 60 Hz target, " \
              f"tolerance {budget.get('tolerance')}, {frames.get('frames')} frames"
    if args.no_budget:
        print(f"SKIP budget: {summary}")
    else:
        line(budget_problems(report), "budget", summary)

    draws = draw_call_line(report)
    if draws is None:
        print("SKIP draw calls: nothing drawn (headless) or no render numbers")
    else:
        line(draws[0], "draw calls", draws[1])

    res = report["resolution"]
    window = res.get("window") or {}
    line(resolution_problems(res, args.size), "resolution",
         f"rendered {wh(res.get('render'))} (min {wh(res.get('min'))}, max {wh(res.get('max'))}), requested "
         f"{wh(res.get('requested'))}, window {wh(window.get('min'))} to {wh(window.get('max'))}")

    diverged = report.get("replay", {}).get("diverged", True)
    line(["diverged from the plan"] if diverged else [], "replay", "matched the plan")

    # process_ms is part of each frame: a median above the frame median is
    # not per-frame data (TIME_PROCESS, a once-a-second maximum, was).
    proc = report.get("process_ms") or {}
    if isinstance(proc.get("p50"), (int, float)) and proc["p50"] > frames.get("p50", 0) + 1e-6:
        line([f"process_ms p50 {proc['p50']:.2f} ms is above frame_ms p50 {frames.get('p50', 0):.2f} ms, "
              "so it is not per-frame"], "process_ms", "")

    timing, display, device = report["timing"], report.get("display", {}), report["device"]
    print(f"NOTE device: {device.get('adapter') or 'no adapter'} ({device.get('rendering_driver')})"
          f"{', headless' if device.get('headless') else ''}"
          f"{', software renderer: frame times are not a measurement' if device.get('software') else ''}")
    print(f"NOTE display: {display.get('refresh_hz')} Hz, vsync {display.get('vsync')}")
    if budget.get("tolerance") not in (None, 0.05) or any(
            isinstance(l, dict) and abs(l.get("budget_ms", 0) - RULES.get(report.get("tier"), {}).get(
                l.get("percentile"), 0) * PERIOD_MS) > 1e-6 for l in budget.get("limits", [])):
        print("NOTE budget: tolerance or budget differs from the defaults (0.05, whole 60 Hz periods)")
    if not timing.get("gpu"):
        print(f"NOTE gpu: {timing.get('gpu_note')}; judge GPU cost from frame_ms with vsync off")
    if timing.get("render_cpu_includes_display_wait"):
        p95 = lambda k: (report.get(k) or {}).get("p95", 0.0)  # noqa: E731
        print(f"NOTE cpu: render_cpu_ms includes the display wait on this driver, so cpu_ms is not CPU cost; "
              f"game logic (process_ms) p95 {p95('process_ms'):.2f} ms, render CPU p95 {p95('render_cpu_ms'):.2f} ms")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

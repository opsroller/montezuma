# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Tests for check_run.py on synthetic benchmark reports (schema 2).
Run: python3 -m unittest discover -s tools/bench/tests (standard library only)."""

import contextlib
import copy
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_run  # noqa: E402

PERIOD = 1000.0 / 60.0

# A vsync-locked desktop-standard run at 1920x1080 on Metal, as the Mac mini
# M4 gave it: within budget, no GPU timing.
GOOD = {
    "schema": 2,
    "tier": "desktop-standard",
    "budget_ms": PERIOD,
    "budget": {"metric": "frame_ms", "target_hz": 60.0, "period_ms": PERIOD, "tolerance": 0.05,
               "limits": [{"percentile": 95.0, "budget_ms": PERIOD, "limit_ms": PERIOD * 1.05,
                           "value_ms": 16.9, "pass": True}],
               "pass": True, "reason": ""},
    "resolution": {"requested": [1920, 1080], "locked": True, "render": [1920, 1080], "min": [1920, 1080],
                   "max": [1920, 1080], "mismatched_frames": 0,
                   "window": {"first": [1920, 1080], "min": [1920, 1080], "max": [1920, 1080], "changed_frames": 0},
                   "pass": True},
    "display": {"refresh_hz": 60.0, "vsync": "enabled"},
    "timing": {"gpu": False, "gpu_note": "no GPU timestamps from the metal driver; gpu_ms is null", "gpu_frames": 0,
               "render_cpu_includes_display_wait": True},
    "device": {"rendering_driver": "metal", "adapter": "Apple M4", "vendor": "Apple", "headless": False,
               "software": False, "resolution": [1920, 1080]},
    "frame_ms": {"frames": 3600, "p50": 16.66, "p95": 16.9, "p99": 17.2},
    "cpu_ms": {"frames": 3600, "p50": 16.1, "p95": 16.5, "p99": 16.8},
    "process_ms": {"frames": 3600, "p50": 1.1, "p95": 1.6, "p99": 2.0},
    "render_cpu_ms": {"frames": 3600, "p50": 15.0, "p95": 15.2, "p99": 15.5},
    "gpu_ms": None,
    "replay": {"diverged": False},
}


def run(report, *args):
    with tempfile.TemporaryDirectory() as d:
        path = Path(d) / "benchmark.json"
        path.write_text(json.dumps(report))
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            code = check_run.main([str(path), *args])
        return code, out.getvalue()


class CheckRunTest(unittest.TestCase):
    def test_good_run_passes(self):
        code, out = run(GOOD, "--size", "1920x1080")
        self.assertEqual(code, 0, out)
        self.assertIn("PASS budget", out)
        self.assertIn("PASS resolution", out)

    def test_no_gpu_timing_is_said_not_judged(self):
        code, out = run(GOOD)
        self.assertEqual(code, 0, out)
        self.assertIn("NOTE gpu", out)
        self.assertIn("gpu_ms is null", out)
        self.assertIn("display wait", out)

    def test_over_budget_fails(self):
        r = copy.deepcopy(GOOD)
        r["tier"] = "desktop-high"
        r["budget"]["limits"][0].update(value_ms=23.0, **{"pass": False})
        r["frame_ms"].update(p95=23.0)
        r["budget"].update(reason="frame_ms p95 23.00 ms over 17.50 ms (16.67 ms budget + 5%)", **{"pass": False})
        code, out = run(r)
        self.assertEqual(code, 1)
        self.assertIn("FAIL budget", out)
        self.assertIn("23.00", out)

    def test_resized_window_fails(self):
        r = copy.deepcopy(GOOD)
        r["resolution"].update(min=[2560, 1386], max=[2560, 1440], render=[2560, 1440], requested=[2560, 1440],
                               mismatched_frames=1800, **{"pass": False})
        r["device"]["resolution"] = [2560, 1440]
        code, out = run(r)
        self.assertEqual(code, 1)
        self.assertIn("FAIL resolution", out)
        self.assertIn("1800", out)

    def test_size_other_than_asked_fails(self):
        code, out = run(GOOD, "--size", "2560x1440")
        self.assertEqual(code, 1)
        self.assertIn("FAIL resolution", out)

    def test_unlocked_run_fails_when_a_size_is_asked(self):
        r = copy.deepcopy(GOOD)
        r["resolution"].update(requested=None, locked=False)
        code, out = run(r, "--size", "1920x1080")
        self.assertEqual(code, 1)
        self.assertIn("not locked", out)

    def test_diverged_replay_fails(self):
        r = copy.deepcopy(GOOD)
        r["replay"]["diverged"] = True
        code, out = run(r)
        self.assertEqual(code, 1)
        self.assertIn("FAIL replay", out)

    def test_schema_1_report_is_refused(self):
        code, out = run({"schema": 1, "cpu_ms": {}, "frame_ms": {}})
        self.assertEqual(code, 2)
        self.assertIn("schema", out)

    def test_missing_report_exits_2(self):
        out = io.StringIO()
        with tempfile.TemporaryDirectory() as d, contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            code = check_run.main([str(Path(d) / "missing.json")])
        self.assertEqual(code, 2, out.getvalue())

    def test_malformed_report_exits_2(self):
        with tempfile.TemporaryDirectory() as d:
            for text in ("", "{not json", "[]"):
                path = Path(d) / "benchmark.json"
                path.write_text(text)
                out = io.StringIO()
                with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
                    code = check_run.main([str(path)])
                self.assertEqual(code, 2, f"{text!r}: {out.getvalue()}")

    def test_budget_pass_flag_alone_does_not_pass(self):
        r = copy.deepcopy(GOOD)
        r["budget"]["limits"] = []
        code, out = run(r)
        self.assertEqual(code, 1, "a budget with no limits judged nothing: " + out)
        r = copy.deepcopy(GOOD)
        r["budget"]["limits"][0]["value_ms"] = 23.0
        code, out = run(r)
        self.assertEqual(code, 1, "p95 23 ms over the 17.5 ms limit: " + out)
        r = copy.deepcopy(GOOD)
        r["frame_ms"]["frames"] = 0
        code, out = run(r)
        self.assertEqual(code, 1, "no frames captured: " + out)

    def test_resolution_pass_flag_alone_does_not_pass(self):
        r = copy.deepcopy(GOOD)
        r["resolution"].update(min=[2560, 1386], max=[2560, 1440], mismatched_frames=1800)
        code, out = run(r)
        self.assertEqual(code, 1, out)

    def test_window_resized_mid_run_fails(self):
        r = copy.deepcopy(GOOD)
        r["resolution"]["window"].update(min=[500, 300], max=[1920, 1080], changed_frames=40)
        code, out = run(r, "--size", "1920x1080")
        self.assertEqual(code, 1, out)
        self.assertIn("FAIL resolution", out)
        self.assertIn("window", out)

    def test_unknown_tier_fails(self):
        r = copy.deepcopy(GOOD)
        r["tier"] = "unset"
        code, out = run(r)
        self.assertNotEqual(code, 0, out)
        self.assertIn("tier", out)

    def test_web_report_without_median_rule_fails(self):
        r = copy.deepcopy(GOOD)
        r["tier"] = "web"  # limits carry only p95; web also needs p50 <= one period
        code, out = run(r)
        self.assertNotEqual(code, 0, out)

    def test_tolerance_above_the_maximum_fails(self):
        r = copy.deepcopy(GOOD)
        r["budget"]["tolerance"] = 10
        code, out = run(r)
        self.assertNotEqual(code, 0, out)

    def test_headless_run_is_not_a_budget_pass(self):
        r = copy.deepcopy(GOOD)
        r["device"].update(adapter="", vendor="", headless=True, software=False)
        r["display"] = {"refresh_hz": -1.0, "vsync": "enabled"}
        code, out = run(r, "--size", "1920x1080")
        self.assertNotIn("PASS budget", out)
        self.assertNotEqual(code, 0, out)
        self.assertIn("not a measurement", out)

    def test_software_renderer_frame_times_are_labelled(self):
        r = copy.deepcopy(GOOD)
        r["device"].update(adapter="llvmpipe (LLVM 20.1.2, 256 bits)", vendor="Mesa", software=True)
        r["timing"] = {"gpu": True, "gpu_note": "", "gpu_frames": 3600, "render_cpu_includes_display_wait": False}
        r["gpu_ms"] = {"frames": 3600, "p50": 5.0, "p95": 6.0, "p99": 7.0}
        code, out = run(r, "--size", "1920x1080")
        self.assertIn("software", out.lower(), out)
        self.assertNotIn("PASS budget", out)

    def test_missing_gpu_timing_is_not_reported_as_zero_ms(self):
        r = copy.deepcopy(GOOD)
        r["gpu_ms"] = {"frames": 3600, "mean": 0.0, "p50": 0.0, "p95": 0.0, "p99": 0.0, "max": 0.0, "over_budget": 0}
        code, out = run(r)
        self.assertEqual(code, 2, out)
        self.assertIn("gpu_ms", out)

    def test_process_ms_that_is_not_per_frame_is_refused(self):
        r = copy.deepcopy(GOOD)
        r["frame_ms"].update(frames=3600, mean=132.0, p50=126.0, p95=150.0, p99=160.8, max=160.8)
        r["process_ms"].update(frames=3600, mean=163.4, p50=152.0, p95=202.6, p99=202.6, max=202.6)
        code, out = run(r, "--no-budget")
        self.assertEqual(code, 1, out)
        self.assertIn("process_ms", out)

    def test_bad_size_argument(self):
        with self.assertRaises(SystemExit):
            run(GOOD, "--size", "big")

    def test_no_budget_mode_reports_only(self):
        r = copy.deepcopy(GOOD)
        r["budget"].update(**{"pass": False}, reason="frame_ms p95 30.00 ms over 17.50 ms")
        code, out = run(r, "--no-budget")
        self.assertEqual(code, 0, out)
        self.assertIn("SKIP budget", out)


if __name__ == "__main__":
    unittest.main()

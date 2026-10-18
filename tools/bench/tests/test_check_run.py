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
                   "max": [1920, 1080], "mismatched_frames": 0, "pass": True},
    "display": {"refresh_hz": 60.0, "vsync": "enabled"},
    "timing": {"gpu": False, "gpu_note": "no GPU timestamps from the metal driver (or headless): gpu_ms is all zero, "
               "not a measurement", "render_cpu_includes_display_wait": True},
    "device": {"rendering_driver": "metal", "resolution": [1920, 1080]},
    "frame_ms": {"frames": 3600, "p50": 16.66, "p95": 16.9, "p99": 17.2},
    "cpu_ms": {"frames": 3600, "p50": 16.1, "p95": 16.5, "p99": 16.8},
    "process_ms": {"frames": 3600, "p50": 1.1, "p95": 1.6, "p99": 2.0},
    "render_cpu_ms": {"frames": 3600, "p50": 15.0, "p95": 15.2, "p99": 15.5},
    "gpu_ms": {"frames": 3600, "p50": 0.0, "p95": 0.0, "p99": 0.0},
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
        self.assertIn("not a measurement", out)
        self.assertIn("display wait", out)

    def test_over_budget_fails(self):
        r = copy.deepcopy(GOOD)
        r["tier"] = "desktop-high"
        r["budget"]["limits"][0].update(value_ms=23.0, **{"pass": False})
        r["budget"].update(reason="frame_ms p95 23.00 ms over 17.50 ms (16.67 ms budget + 5%)", **{"pass": False})
        code, out = run(r)
        self.assertEqual(code, 1)
        self.assertIn("FAIL budget", out)
        self.assertIn("23.00", out)

    def test_resized_window_fails(self):
        r = copy.deepcopy(GOOD)
        r["resolution"].update(min=[2560, 1386], max=[2560, 1440], render=[2560, 1440], requested=[2560, 1440],
                               mismatched_frames=1800, **{"pass": False})
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

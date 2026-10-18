#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Check a benchmark report's feature list against one quality tier.

Usage: check_features.py REPORT [--tier TIER]

REPORT is the JSON game/scenes/bench/bench.tscn writes (or the readability
capture's layout.json). The tier defaults to the one the report names. The
rows come from tools/bench/tiers.json, which follows the feature tables in
docs/reference/visual-target.md. Prints one line per row, exits 1 when any
row is not met. Standard library only.
"""

import argparse
import json
import sys
from pathlib import Path

TIERS = Path(__file__).with_name("tiers.json")


def met(cond, features):
    if "any" in cond:
        return any(met(c, features) for c in cond["any"])
    if cond["key"] not in features:
        return False
    v = features[cond["key"]]
    if "eq" in cond and v != cond["eq"]:
        return False
    if "in" in cond and v not in cond["in"]:
        return False
    if "min" in cond and not (isinstance(v, (int, float)) and v >= cond["min"]):
        return False
    if "max" in cond and not (isinstance(v, (int, float)) and v <= cond["max"]):
        return False
    return True


def describe(cond, features):
    if "any" in cond:
        return " or ".join(describe(c, features) for c in cond["any"])
    want = ", ".join(f"{k} {cond[k]}" for k in ("eq", "in", "min", "max") if k in cond)
    return f"{cond['key']}={json.dumps(features.get(cond['key']))} (want {want})"


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("report", type=Path)
    ap.add_argument("--tier", help="tier to check against (default: the report's tier)")
    args = ap.parse_args(argv)

    report = json.loads(args.report.read_text())
    tiers = json.loads(TIERS.read_text())["tiers"]
    tier = args.tier or report.get("tier", "")
    if tier not in tiers:
        print(f"unknown tier {tier!r}; pass --tier, one of: {', '.join(tiers)}", file=sys.stderr)
        return 2
    features = dict(report.get("features", {}))
    features["rendering_method"] = report.get("device", {}).get("rendering_method")

    failed = 0
    for row in tiers[tier]:
        ok = met(row, features)
        failed += 0 if ok else 1
        print(f"{'PASS' if ok else 'FAIL'} {row['row']}: {describe(row, features)}")
    print(f"{tier}: {len(tiers[tier]) - failed} of {len(tiers[tier])} rows met")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

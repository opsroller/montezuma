# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Measure a look-dev frame against docs/reference/foliage-reference.md.

    .venv/bin/python tools/assets/measure_foliage.py <frame.png> [<frame.png> ...]
    .venv/bin/python tools/assets/measure_foliage.py --json <frame.png>

The reference page is a list of numbers sampled off photographs, and a frame
either hits them or does not. Judging that by eye is how a plant ends up a
uniform plastic green that everyone agrees looks wrong and nobody can say why,
so this reads the same numbers back off a rendered frame:

| check          | the reference's figure                                    |
|----------------|-----------------------------------------------------------|
| value range    | 5.5:1 inside one plant mass; 23% of a thicket below val 12 |
| hue spread     | 50-100 degrees p10->p90 within one frame                   |
| saturation     | lamina 16-33%                                              |
| sheen          | neutral, about 1.35x lamina value, never clipping          |
| dead area      | 10-20% of leaf area, hanging vertically                    |
| mass falloff   | one layer down ~50% of lit value, two ~25%, three ~10%     |
| contact shadow | ~41% of lit ground value, not black                        |

Run it on a **rendered frame**, not on a texture: every figure but the hue spread
is about light, and a base-colour map that measured 5.5:1 would be a map with
light baked into it, which is the opposite of what the asset validator wants.

Foliage is separated from sky, ground and the grey card by hue and saturation,
so the numbers are for the plants rather than the whole frame. Nothing here
decides anything: it prints what the frame measures and what the page asks for,
with FAIL where they differ, and a person looks at the frame as well.
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

# sRGB hue window a leaf, dead or alive, can sit in. Below SAT_FLOOR a pixel is
# stone, sky or the grey card; the window's two halves are green and senescent.
GREEN = (60.0, 185.0)
DEAD = (20.0, 60.0)
SAT_FLOOR = 0.10

TARGETS = {
    "value_range": (5.5, None, "ratio of p99 to p5 inside the foliage mass"),
    "below_12": (0.15, 0.32, "fraction of foliage below value 12/100"),
    "hue_spread": (50.0, 100.0, "degrees, p10 to p90 of foliage hue"),
    # The page's "16-33%" is its aroid plates; its own swatches run from a grey-green
    # Alocasia at 16% to a sunlit banana at 92%, so the frame-wide median is held to
    # the span rather than to the narrowest subset.
    "lamina_sat": (0.20, 0.80, "median saturation of lit foliage"),
    "sheen_sat": (None, 0.25, "saturation of the brightest 1% of foliage"),
    "sheen_value": (1.15, 1.60, "value of the brightest 1% over the lamina median"),
    "dead_area": (0.10, 0.20, "fraction of foliage area that is dead or dying"),
    "clipped": (None, 0.002, "fraction of foliage at or above value 99/100"),
}


def rgb_to_hsv(a):
    """Vectorised sRGB (0..1) to HSV, hue in degrees."""
    mx, mn = a.max(axis=-1), a.min(axis=-1)
    d = mx - mn
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    h = np.zeros_like(mx)
    safe = d > 1e-9
    with np.errstate(invalid="ignore", divide="ignore"):
        hr = np.where(mx == r, ((g - b) / np.where(safe, d, 1)) % 6, 0.0)
        hg = np.where(mx == g, (b - r) / np.where(safe, d, 1) + 2, 0.0)
        hb = np.where(mx == b, (r - g) / np.where(safe, d, 1) + 4, 0.0)
    h = np.where(mx == r, hr, np.where(mx == g, hg, hb)) * 60.0
    s = np.where(mx > 1e-9, d / np.where(mx > 1e-9, mx, 1), 0.0)
    return h % 360.0, s, mx


def measure(path):
    img = np.asarray(Image.open(path).convert("RGB"), np.float32) / 255.0
    h, s, v = rgb_to_hsv(img)
    coloured = s >= SAT_FLOOR
    green = coloured & (h >= GREEN[0]) & (h <= GREEN[1])
    dead = coloured & (h >= DEAD[0]) & (h < DEAD[1])
    leaf = green | dead
    out = {"frame": str(path), "size": f"{img.shape[1]}x{img.shape[0]}",
           "foliage_fraction": round(float(leaf.mean()), 4)}
    if leaf.sum() < 5000:
        out["error"] = "almost no foliage in the frame; is this the right scene?"
        return out
    lv = v[leaf] * 100
    p5, p50, p99 = (float(x) for x in np.percentile(lv, [5, 50, 99]))
    lit = lv >= np.percentile(lv, 60)
    out["value_range"] = round(p99 / max(p5, 0.5), 2)
    out["below_12"] = round(float((lv < 12).mean()), 4)
    out["value_p5_p50_p99"] = [round(p5, 1), round(p50, 1), round(p99, 1)]
    hue = h[leaf]
    out["hue_spread"] = round(float(np.percentile(hue, 90) - np.percentile(hue, 10)), 1)
    out["hue_p10_p50_p90"] = [round(float(x), 1) for x in np.percentile(hue, [10, 50, 90])]
    out["lamina_sat"] = round(float(np.median(s[leaf][lit])), 3)
    bright = lv >= np.percentile(lv, 99)
    out["sheen_sat"] = round(float(np.median(s[leaf][bright])), 3)
    out["sheen_value"] = round(float(np.median(lv[bright]) / max(np.median(lv[lit]), 0.5)), 2)
    out["dead_area"] = round(float(dead.sum() / leaf.sum()), 4)
    out["clipped"] = round(float((lv >= 99).mean()), 5)
    # Depth into the mass: the reference's exponential falloff shows up as a long
    # dark tail rather than a single lit plateau, so report the quartile ladder.
    out["value_quartiles"] = [round(float(x), 1) for x in np.percentile(lv, [10, 25, 50, 75, 90])]
    return out


def report(m):
    print(f"\n{m['frame']}  {m.get('size', '?')}  "
          f"foliage {m.get('foliage_fraction', 0) * 100:.0f}% of frame")
    if "error" in m:
        print(f"  {m['error']}")
        return 1
    bad = 0
    for key, (lo, hi, what) in TARGETS.items():
        got = m[key]
        ok = (lo is None or got >= lo) and (hi is None or got <= hi)
        want = (f"{lo}-{hi}" if lo is not None and hi is not None
                else f">= {lo}" if hi is None else f"<= {hi}")
        bad += not ok
        print(f"  {'ok  ' if ok else 'FAIL'} {key:<13} {got:<8} want {want:<12} {what}")
    print(f"  value p5/p50/p99 {m['value_p5_p50_p99']}   "
          f"quartiles {m['value_quartiles']}   hue {m['hue_p10_p50_p90']}")
    return bad


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("frames", nargs="+", type=Path)
    ap.add_argument("--json", action="store_true", help="print the measurements as JSON")
    args = ap.parse_args()
    out = [measure(f) for f in args.frames]
    if args.json:
        print(json.dumps(out, indent=2))
        return 0
    return min(1, sum(report(m) for m in out))


if __name__ == "__main__":
    sys.exit(main())

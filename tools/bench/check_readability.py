#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Check readability rules 2, 4, 5 and 6 of docs/reference/visual-target.md
on the captures game/scenes/bench/readability.tscn writes.

Usage: check_readability.py CAPTURE_DIR [--json REPORT] [threshold options]

Reads CAPTURE_DIR/layout.json and the four PNGs it lists, prints one line
per rule and exits 1 when any rule fails. Needs Pillow only. Thresholds and
what each check measures: docs/reference/benchmark.md#readability-checks.
"""

import argparse
import itertools
import json
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageFilter, ImageStat

# Colour-vision deficiency at full severity, in linear RGB: Machado, Oliveira
# and Fernandes, "A Physiologically-based Model for Simulation of Color
# Vision Deficiency", IEEE TVCG 15(6), 2009, table for severity 1.0.
CVD = {
    "protanopia": ((0.152286, 1.052583, -0.204868), (0.114503, 0.786281, 0.099216), (-0.003882, -0.048116, 1.051998)),
    "deuteranopia": ((0.367322, 0.860646, -0.227968), (0.280085, 0.672501, 0.047413), (-0.011820, 0.042940, 0.968881)),
    "tritanopia": ((1.255528, -0.076749, -0.178779), (-0.078411, 0.930809, 0.147602), (0.004733, 0.691367, 0.303900)),
}
# Rec. 709 / sRGB luminance weights, linear RGB.
LUMA = (0.2126, 0.7152, 0.0722)

TO_LINEAR = [((v / 255) / 12.92) if v <= 10 else (((v / 255) + 0.055) / 1.055) ** 2.4 for v in range(256)]


def to_srgb8(x):
    x = min(1.0, max(0.0, x))
    v = x * 12.92 if x <= 0.0031308 else 1.055 * x ** (1 / 2.4) - 0.055
    return int(round(v * 255))


def relative_luminance(rgb):
    r, g, b = rgb
    return LUMA[0] * TO_LINEAR[r] + LUMA[1] * TO_LINEAR[g] + LUMA[2] * TO_LINEAR[b]


def box(rect, inset=0.0):
    x0, y0, x1, y1 = rect
    dx, dy = (x1 - x0) * inset, (y1 - y0) * inset
    return (int(round(x0 + dx)), int(round(y0 + dy)), int(round(x1 - dx)), int(round(y1 - dy)))


def percentile(values, p):
    if not values:
        return 0.0
    s = sorted(values)
    k = max(0, min(len(s) - 1, int(-(-p / 100 * len(s) // 1)) - 1))
    return s[k]


def luma8(img):
    return img.convert("L")


# ---------------------------------------------------------------- rule 2


def rule_2(off, atmosphere, layout, args):
    """Persistent effects (fog, haze, shafts, DOF, dust) stay see-through
    over the board and never cover all of it. Measured per cell as the share
    of its luma contrast (standard deviation) they take away, which a
    change of exposure alone leaves about as it is: no cell may lose more
    than rule2_cell_max, and at least rule2_clear_share of the cells must
    lose no more than rule2_clear."""
    a, o = luma8(atmosphere), luma8(off)
    losses = []
    for cell in layout["cells"]:
        r = box(cell["rect"])
        base = ImageStat.Stat(o.crop(r)).stddev[0]
        losses.append(1.0 - ImageStat.Stat(a.crop(r)).stddev[0] / base if base > 0 else 0.0)
    worst = max(losses)
    clear = sum(1 for v in losses if v <= args.rule2_clear) / len(losses)
    ok = worst <= args.rule2_cell_max and clear >= args.rule2_clear_share
    return ok, {"worst_cell_loss": round(worst, 3), "max_cell_loss": args.rule2_cell_max,
                "clear_share": round(clear, 3), "min_clear_share": args.rule2_clear_share,
                "clear_loss": args.rule2_clear}


# ---------------------------------------------------------------- rule 4


def piece_and_tile(img_on, img_off, cell, args):
    """Mean relative luminance of the piece and of its tile in both captures.
    The tile is the ring just inside the cell edge; the piece is every pixel
    in the middle of the cell that differs from the tile in the effects-off
    capture, so the same pixels are read in both."""
    outer = box(cell["rect"], args.ring_inset)
    inner = box(cell["rect"], args.piece_inset)
    off_crop = img_off.crop(outer)
    on_crop = img_on.crop(outer)
    w, h = off_crop.size
    ix0, iy0, ix1, iy1 = inner[0] - outer[0], inner[1] - outer[1], inner[2] - outer[0], inner[3] - outer[1]
    ring = max(1, int(round(min(w, h) * args.ring_width)))
    off_px, on_px = off_crop.load(), on_crop.load()
    tile_off, tile_on = [], []
    for y in range(h):
        for x in range(w):
            if x < ring or y < ring or x >= w - ring or y >= h - ring:
                tile_off.append(relative_luminance(off_px[x, y]))
                tile_on.append(relative_luminance(on_px[x, y]))
    tile_ref = sorted(tile_off)[len(tile_off) // 2]
    piece_off, piece_on = [], []
    for y in range(iy0, iy1):
        for x in range(ix0, ix1):
            lo = relative_luminance(off_px[x, y])
            if abs(lo - tile_ref) >= args.piece_delta:
                piece_off.append(lo)
                piece_on.append(relative_luminance(on_px[x, y]))
    if not piece_off:
        return None
    avg = lambda v: sum(v) / len(v)
    return avg(piece_off), avg(tile_off), avg(piece_on), avg(tile_on)


def contrast(a, b):
    hi, lo = max(a, b), min(a, b)
    return (hi + 0.05) / (lo + 0.05)


def rule_4(on, off, layout, args):
    """Piece-to-tile luminance contrast changes by no more than 10% with all
    effects on versus all off, for every kind on both tile shades."""
    groups = {}
    for cell in layout["cells"]:
        m = piece_and_tile(on, off, cell, args)
        if m is None:
            continue
        groups.setdefault((cell["kind"], cell["shade"]), []).append(m)
    rows, ok = [], True
    for kind in range(layout["kinds"]):
        for shade in (0, 1):
            ms = groups.get((kind, shade))
            if not ms:
                ok = False
                rows.append({"kind": kind, "shade": shade, "missing": True})
                continue
            c_off = sum(contrast(p, t) for p, t, _, _ in ms) / len(ms)
            c_on = sum(contrast(p, t) for _, _, p, t in ms) / len(ms)
            change = abs(c_on - c_off) / c_off
            ok = ok and change <= args.rule4_change
            rows.append({"kind": kind, "shade": shade, "cells": len(ms), "contrast_off": round(c_off, 3),
                         "contrast_on": round(c_on, 3), "change": round(change, 4)})
    worst = max((r.get("change", 1.0) for r in rows), default=1.0)
    return ok, {"max_change": args.rule4_change, "worst_change": round(worst, 4), "groups": rows}


# ---------------------------------------------------------------- rule 5


def simulate(img, name):
    """`img` as seen in greyscale or with a colour-vision deficiency."""
    lin = [[TO_LINEAR[v] for v in px] for px in img.get_flattened_data()]
    if name == "greyscale":
        out = []
        for r, g, b in lin:
            v = to_srgb8(LUMA[0] * r + LUMA[1] * g + LUMA[2] * b)
            out.append((v, v, v))
    else:
        m = CVD[name]
        out = [tuple(to_srgb8(row[0] * r + row[1] * g + row[2] * b) for row in m) for r, g, b in lin]
    res = Image.new("RGB", img.size)
    res.putdata(out)
    return res


def kind_patches(img, layout, args):
    """Each kind's average look: every cell of the kind, shrunk to a small
    patch and averaged, so tile shades even out and shape and value remain."""
    n = args.patch_px
    sums = {}
    for cell in layout["cells"]:
        patch = img.crop(box(cell["rect"], args.patch_inset)).resize((n, n), Image.Resampling.BOX)
        acc = sums.setdefault(cell["kind"], [[0.0, 0.0, 0.0] for _ in range(n * n)] + [0])
        for i, px in enumerate(patch.get_flattened_data()):
            for c in range(3):
                acc[i][c] += px[c]
        acc[-1] += 1
    patches = {}
    for kind, acc in sums.items():
        count = acc[-1]
        im = Image.new("RGB", (n, n))
        im.putdata([tuple(int(round(v / count)) for v in px) for px in acc[:-1]])
        patches[kind] = im
    return patches


def patch_distance(a, b):
    d = list(ImageChops.difference(a, b).get_flattened_data())
    return sum(sum(px) for px in d) / (3 * len(d))


def rule_5(on, layout, args):
    """The seven kinds stay distinguishable in greyscale and under
    protanopia, deuteranopia and tritanopia simulation at the capture size
    (capture at the smallest phone size). Kinds are compared on the same
    tile shade, so only the piece differs; the closest pair must stand
    further apart than a fixed floor and than the spread between cells of
    one kind (`rule5_ratio` times the largest such spread)."""
    ok = True
    shades = {}
    for shade in (0, 1):
        cells = [c for c in layout["cells"] if c["shade"] == shade]
        sub = dict(layout, cells=cells)
        patches = kind_patches(on, sub, args)
        halves = [kind_patches(on, dict(layout, cells=cells[i::2]), args) for i in (0, 1)]
        ok = ok and len(patches) == layout["kinds"]
        sims = {}
        for name in ("greyscale", "protanopia", "deuteranopia", "tritanopia"):
            seen = {k: simulate(p, name) for k, p in patches.items()}
            spread = max((patch_distance(simulate(halves[0][k], name), simulate(halves[1][k], name))
                          for k in halves[0] if k in halves[1]), default=0.0)
            closest = min(((patch_distance(seen[a], seen[b]), a, b)
                           for a, b in itertools.combinations(sorted(seen), 2)), default=(0.0, -1, -1))
            need = max(args.rule5_distance, args.rule5_ratio * spread)
            passed = closest[0] >= need
            ok = ok and passed
            sims[name] = {"pass": passed, "closest_pair": [closest[1], closest[2]], "distance": round(closest[0], 2),
                          "within_kind_spread": round(spread, 2), "needed": round(need, 2)}
        shades[str(shade)] = sims
    worst = min((v["distance"] - v["needed"], f"shade {sh} {name}") for sh, sims in shades.items()
                for name, v in sims.items())
    return ok, {"min_distance": args.rule5_distance, "ratio": args.rule5_ratio, "viewport": layout["viewport"],
                "tightest": worst[1], "shades": shades}


# ---------------------------------------------------------------- rule 6


def rule_6(off, glow, board, cell_px, args):
    """Bloom spreads no more than a quarter cell beyond an emissive edge on
    the board."""
    off_l = luma8(off.crop(board))
    glow_l = luma8(glow.crop(board))
    sources = off_l.point(lambda v: 255 if v >= args.emissive_level else 0)
    reach = max(1, int(round(cell_px * 0.25)))
    allowed = sources.filter(ImageFilter.MaxFilter(2 * reach + 1)) if reach > 0 else sources
    bloom = ImageChops.subtract(glow_l, off_l).point(lambda v: 255 if v >= args.bloom_level else 0)
    outside = ImageChops.subtract(bloom, allowed)
    stray = sum(1 for v in outside.get_flattened_data() if v)
    total = off_l.size[0] * off_l.size[1]
    share = stray / total
    ok = share <= args.rule6_share
    return ok, {"reach_px": reach, "emissive_pixels": sum(1 for v in sources.get_flattened_data() if v),
                "bloom_pixels": sum(1 for v in bloom.get_flattened_data() if v), "stray_pixels": stray,
                "stray_share": round(share, 5), "max_share": args.rule6_share}


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("capture_dir", type=Path)
    ap.add_argument("--json", type=Path, help="also write the full report here")
    ap.add_argument("--rule2-cell-max", type=float, default=0.40,
                    help="max share of a cell's luma contrast taken away: see-through (default 0.40)")
    ap.add_argument("--rule2-clear", type=float, default=0.05,
                    help="contrast loss up to which a cell counts as clear (default 0.05)")
    ap.add_argument("--rule2-clear-share", type=float, default=0.25,
                    help="min share of cells that stay clear (default 0.25)")
    ap.add_argument("--rule4-change", type=float, default=0.10, help="max relative contrast change (default 0.10)")
    ap.add_argument("--rule5-distance", type=float, default=12.0,
                    help="min mean per-pixel difference between two kinds, 0-255 (default 12)")
    ap.add_argument("--rule5-ratio", type=float, default=1.5,
                    help="closest pair must be this many times the within-kind spread (default 1.5)")
    ap.add_argument("--rule6-share", type=float, default=0.001,
                    help="max share of board pixels bloomed beyond reach (default 0.001)")
    ap.add_argument("--emissive-level", type=int, default=235, help="luma that counts as emissive (default 235)")
    ap.add_argument("--bloom-level", type=int, default=6, help="luma gain that counts as bloom (default 6)")
    ap.add_argument("--piece-delta", type=float, default=0.03, help=argparse.SUPPRESS)
    ap.add_argument("--ring-inset", type=float, default=0.06, help=argparse.SUPPRESS)
    ap.add_argument("--ring-width", type=float, default=0.08, help=argparse.SUPPRESS)
    ap.add_argument("--piece-inset", type=float, default=0.2, help=argparse.SUPPRESS)
    ap.add_argument("--patch-inset", type=float, default=0.08, help=argparse.SUPPRESS)
    ap.add_argument("--patch-px", type=int, default=12, help=argparse.SUPPRESS)
    args = ap.parse_args(argv)

    layout = json.loads((args.capture_dir / "layout.json").read_text())
    img = {name: Image.open(args.capture_dir / f"{name}.png").convert("RGB")
           for name in ("all_on", "all_off", "atmosphere_only", "glow_only")}
    sizes = {im.size for im in img.values()}
    if sizes != {tuple(layout["viewport"])}:
        print(f"captures are {sorted(sizes)}, layout.json says {layout['viewport']}", file=sys.stderr)
        return 2
    board = box(layout["board_rect"])

    results = {
        "rule_2": rule_2(img["all_off"], img["atmosphere_only"], layout, args),
        "rule_4": rule_4(img["all_on"], img["all_off"], layout, args),
        "rule_5": rule_5(img["all_on"], layout, args),
        "rule_6": rule_6(img["all_off"], img["glow_only"], board, layout["cell_px"], args),
    }
    summary = {
        "rule_2": lambda d: (f"worst cell contrast loss {d['worst_cell_loss']:.0%} (max {d['max_cell_loss']:.0%}), "
                             f"clear cells {d['clear_share']:.0%} (min {d['min_clear_share']:.0%})"),
        "rule_4": lambda d: f"worst contrast change {d['worst_change']:.1%} (max {d['max_change']:.0%})",
        "rule_5": lambda d: "; ".join(
            f"shade {sh}: " + ", ".join(f"{k} {v['distance']}/{v['needed']}" for k, v in sims.items())
            for sh, sims in d["shades"].items()) + " (closest pair / needed)",
        "rule_6": lambda d: f"{d['stray_pixels']} px bloomed past {d['reach_px']} px ({d['stray_share']:.3%}, max {d['max_share']:.1%})",
    }
    failed = 0
    for rule, (ok, detail) in results.items():
        failed += 0 if ok else 1
        print(f"{'PASS' if ok else 'FAIL'} {rule.replace('_', ' ')}: {summary[rule](detail)}")
    if args.json:
        report = {"layout": str(args.capture_dir / "layout.json"), "features": layout.get("features", {}),
                  "rules": {k: {"pass": ok, **d} for k, (ok, d) in results.items()}}
        args.json.write_text(json.dumps(report, indent=2) + "\n")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

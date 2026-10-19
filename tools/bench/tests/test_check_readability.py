# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Tests for check_readability.py on small synthetic captures: a 7x2 board
whose kinds sit on both tile shades. Run: python3 -m unittest discover -s tools/bench/tests
(needs Pillow, as the assets CI job installs it)."""

import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_readability  # noqa: E402

CELL = 40
COLS, ROWS = 7, 2
ORIGIN = (20, 20)
SIZE = (COLS * CELL + 40, ROWS * CELL + 40)
TILES = ((150, 140, 120), (110, 100, 85))


def layout():
    cells = []
    for y in range(ROWS):
        for x in range(COLS):
            x0, y0 = ORIGIN[0] + x * CELL, ORIGIN[1] + y * CELL
            cells.append({"x": x, "y": y, "kind": x, "shade": (x + y) % 2, "rect": [x0, y0, x0 + CELL, y0 + CELL]})
    return {"schema": 1, "seed": 0, "kinds": COLS, "viewport": list(SIZE),
            "board_rect": [ORIGIN[0], ORIGIN[1], ORIGIN[0] + COLS * CELL, ORIGIN[1] + ROWS * CELL],
            "cell_px": CELL, "cells": cells, "captures": []}


# Grey levels well away from both tile shades, so each kind differs in
# value as well as shape.
VALUES = (0, 30, 60, 175, 205, 230, 255)


def board(same=False):
    """Tiles in two shades, and a grey piece per cell whose shape and value
    depend on its kind (or are all alike with `same`)."""
    im = Image.new("RGB", SIZE, (60, 60, 60))
    d = ImageDraw.Draw(im)
    for c in layout()["cells"]:
        x0, y0, x1, y1 = c["rect"]
        d.rectangle((x0, y0, x1 - 1, y1 - 1), fill=TILES[c["shade"]])
        k = 3 if same else c["kind"]
        v = VALUES[k]
        box = (x0 + 4, y0 + 4, x1 - 5, y1 - 5)
        if k % 2:
            d.ellipse(box, fill=(v, v, v))
        else:
            d.rectangle(box, fill=(v, v, v))
    return im


class CheckReadability(unittest.TestCase):
    def run_check(self, all_on, all_off=None, atmosphere=None, glow=None):
        all_off = all_off or board()
        with tempfile.TemporaryDirectory() as tmp:
            d = Path(tmp)
            (d / "layout.json").write_text(json.dumps(layout()))
            all_on.save(d / "all_on.png")
            all_off.save(d / "all_off.png")
            (atmosphere or all_off).save(d / "atmosphere_only.png")
            (glow or all_off).save(d / "glow_only.png")
            with contextlib.redirect_stdout(io.StringIO()):
                code = check_readability.main([str(d), "--json", str(d / "r.json")])
            report = json.loads((d / "r.json").read_text())
        return code, {k: v["pass"] for k, v in report["rules"].items()}

    def test_unchanged_board_passes_every_rule(self):
        code, rules = self.run_check(board())
        self.assertEqual(rules, {"rule_2": True, "rule_4": True, "rule_5": True, "rule_6": True})
        self.assertEqual(code, 0)

    def test_fog_over_the_whole_board_fails_rule_2(self):
        fog = Image.blend(board(), Image.new("RGB", SIZE, (200, 200, 200)), 0.15)
        code, rules = self.run_check(board(), atmosphere=fog)
        self.assertFalse(rules["rule_2"])
        self.assertEqual(code, 1)

    def test_thin_fog_patches_with_clear_cells_pass_rule_2(self):
        fog = Image.blend(board(), Image.new("RGB", SIZE, (200, 200, 200)), 0.15)
        patchy = board()
        # A patch over the left four columns; the other three stay clear.
        patchy.paste(fog.crop((0, 0, ORIGIN[0] + 4 * CELL, SIZE[1])), (0, 0))
        _, rules = self.run_check(board(), atmosphere=patchy)
        self.assertTrue(rules["rule_2"])

    def test_thick_fog_patch_fails_rule_2(self):
        thick = board()
        fog = Image.blend(board(), Image.new("RGB", SIZE, (230, 230, 230)), 0.6)
        thick.paste(fog.crop((0, 0, ORIGIN[0] + 2 * CELL, SIZE[1])), (0, 0))
        _, rules = self.run_check(board(), atmosphere=thick)
        self.assertFalse(rules["rule_2"])

    def test_washed_out_pieces_fail_rule_4(self):
        washed = Image.blend(board(), Image.new("RGB", SIZE, (130, 120, 100)), 0.5)
        _, rules = self.run_check(washed)
        self.assertFalse(rules["rule_4"])

    def test_identical_pieces_fail_rule_5(self):
        same = board(same=True)
        _, rules = self.run_check(same, all_off=same)
        self.assertFalse(rules["rule_5"])

    def test_bloom_within_a_quarter_cell_passes_rule_6(self):
        self.assertTrue(self.bloom(spread=CELL // 8)["rule_6"])

    def test_bloom_past_a_quarter_cell_fails_rule_6(self):
        self.assertFalse(self.bloom(spread=CELL // 2)["rule_6"])

    def bloom(self, spread):
        """An emissive square in one cell, with a halo `spread` pixels wide
        in the glow capture."""
        off = board()
        x0, y0 = ORIGIN[0] + 3 * CELL + 14, ORIGIN[1] + 14
        ImageDraw.Draw(off).rectangle((x0, y0, x0 + 11, y0 + 11), fill=(255, 255, 255))
        halo = Image.new("RGB", SIZE, (0, 0, 0))
        ImageDraw.Draw(halo).rectangle((x0 - spread, y0 - spread, x0 + 11 + spread, y0 + 11 + spread),
                                       fill=(40, 40, 40))
        glow = ImageChops.add(off, halo)
        _, rules = self.run_check(off, all_off=off, glow=glow)
        return rules


if __name__ == "__main__":
    unittest.main()

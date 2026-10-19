# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Tests for the colour grade LUT (grade_lut.py) and its palette check.
Run: python3 -m unittest discover -s tools/assets/tests (needs numpy and Pillow,
as the assets CI job installs them)."""

import json
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import grade_lut  # noqa: E402

IDENTITY = {"lut_size": 17, "check": {"max_shift_pieces": 0.04, "max_shift_surfaces": 0.05,
                                       "max_separation_drop": 0.005}}
TEMPLE = json.loads((Path(__file__).resolve().parents[1] / "themes/temple.json").read_text())


class TableTest(unittest.TestCase):
    def test_identity_grade_changes_nothing(self):
        strip = grade_lut.table(IDENTITY)
        self.assertEqual(strip.shape, (17, 17 * 17, 3))
        # Between the outermost texel centres; outside them the GPU clamps to the edge texel.
        colours = 0.5 / 17 + np.random.default_rng(1).random((200, 3)) * (1 - 1 / 17)
        self.assertLess(np.abs(grade_lut.lookup(strip, colours) - colours).max(), 1 / 255)

    def test_identity_keeps_dark_tones(self):
        # Godot reads texel i at (i + 0.5) / size with no remap; a lattice at i / (size - 1)
        # would darken 0.1 to about 0.087 on a 32^3 table.
        strip = grade_lut.table(dict(IDENTITY, lut_size=32))
        dark = np.array([[0.1, 0.1, 0.1], [0.05, 0.12, 0.15]])
        self.assertLess(np.abs(grade_lut.lookup(strip, dark) - dark).max(), 1 / 255)

    def test_layout_is_red_across_green_down_blue_by_slice(self):
        strip = grade_lut.table(IDENTITY).astype(int)
        n = 17
        # Slice 3, row 5, column 7 is the texel centre (r, g, b) = ((7, 5, 3) + 0.5) / n.
        np.testing.assert_array_equal(strip[5, 3 * n + 7], np.round((np.array([7, 5, 3]) + 0.5) / n * 255))

    def test_lookup_matches_the_grade_between_lattice_points(self):
        # A 32^3 table, as the temple uses: trilinear filtering between texel
        # centres stays within a few 8-bit steps of the exact grade.
        grade = dict(IDENTITY, lut_size=32, slope=[1.03, 1.0, 0.96], offset=[0, 0, 0.01], saturation=1.06)
        colours = 0.5 / 32 + np.random.default_rng(2).random((500, 3)) * (1 - 1 / 32)
        error = np.abs(grade_lut.lookup(grade_lut.table(grade), colours) - grade_lut.cdl(colours, grade))
        self.assertLess(error.max(), 3 / 255)
        self.assertLess(error.mean(), 0.5 / 255)

    def test_saturation_keeps_luma(self):
        rgb = np.array([[0.8, 0.3, 0.1]])
        more = grade_lut.cdl(rgb, {"saturation": 1.2})
        self.assertAlmostEqual(float((more * grade_lut.LUMA).sum()), float((rgb * grade_lut.LUMA).sum()), places=6)


class WriteTest(unittest.TestCase):
    def test_writes_once_and_keeps_godots_rewrite(self):
        with tempfile.TemporaryDirectory() as tmp:
            theme = Path(tmp)
            self.assertTrue(grade_lut.write(theme, IDENTITY))
            png = grade_lut.lut_path(theme)
            self.assertEqual(Image.open(png).size, (17 * 17, 17))
            imp = png.with_name(png.name + ".import")
            self.assertEqual(grade_lut.import_mismatches(IDENTITY, imp.read_text()), {})
            # Godot adds a uid and paths after importing; that file still agrees.
            imp.write_text(imp.read_text().replace('type="CompressedTexture3D"',
                                                   'type="CompressedTexture3D"\nuid="uid://x"'))
            self.assertFalse(grade_lut.write(theme, IDENTITY))
            self.assertIn("uid://x", imp.read_text())

    def test_a_2d_import_is_rewritten(self):
        with tempfile.TemporaryDirectory() as tmp:
            theme = Path(tmp)
            grade_lut.write(theme, IDENTITY)
            imp = grade_lut.lut_path(theme).with_name("T_Grade_LUT.png.import")
            imp.write_text('[remap]\n\nimporter="texture"\n\n[params]\n\ncompress/mode=0\n')
            self.assertIn("importer", grade_lut.import_mismatches(IDENTITY, imp.read_text()))
            self.assertTrue(grade_lut.write(theme, IDENTITY))
            self.assertEqual(grade_lut.import_mismatches(IDENTITY, imp.read_text()), {})


class CheckTest(unittest.TestCase):
    def test_temple_grade_keeps_the_palette(self):
        lines, errors = grade_lut.check(TEMPLE["grade"], *grade_lut.theme_palette(TEMPLE))
        self.assertEqual(errors, [], "\n".join(lines))

    def test_a_strong_cast_fails(self):
        grade = dict(IDENTITY, slope=[1.0, 1.0, 0.7], offset=[0.05, 0.02, 0.0])
        _, errors = grade_lut.check(grade, *grade_lut.theme_palette(TEMPLE))
        self.assertTrue(any("moves piece colour" in e for e in errors), errors)

    def test_desaturating_brings_pieces_together(self):
        grade = dict(IDENTITY, saturation=0.5)
        _, errors = grade_lut.check(grade, *grade_lut.theme_palette(TEMPLE))
        self.assertTrue(any("closest piece pair" in e for e in errors), errors)


if __name__ == "__main__":
    unittest.main()

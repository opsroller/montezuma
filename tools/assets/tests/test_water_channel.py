# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""The mesh and scene generators each compute the water channel
(water_channel()); the meshes need Blender's bpy, so this test reads both
functions from source and checks they place the moat at the same numbers.
Run: python3 -m unittest discover -s tools/assets/tests"""

import ast
import json
import unittest
from pathlib import Path

ASSETS = Path(__file__).resolve().parents[1]


def load_water_channel(script):
    tree = ast.parse((ASSETS / script).read_text())
    fn = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == "water_channel")
    namespace = {}
    exec(compile(ast.Module(body=[fn], type_ignores=[]), script, "exec"), namespace)
    return namespace["water_channel"]


class WaterChannelTest(unittest.TestCase):
    def test_generators_agree(self):
        meshes = load_water_channel("gen_theme_meshes.py")
        scenes = load_water_channel("gen_theme_scenes.py")
        for path in sorted((ASSETS / "themes").glob("*.json")):
            p = json.loads(path.read_text())
            if "water" not in p:
                continue
            with self.subTest(theme=path.stem):
                m, s = meshes(p), scenes(p)
                for key in s:
                    self.assertAlmostEqual(m[key], s[key], places=9, msg=key)


if __name__ == "__main__":
    unittest.main()

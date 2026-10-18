# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Tests for the generated textures' import settings (texture_import.py) and the
validator rule that checks them. Run: python3 -m unittest discover -s tools/assets/tests
(needs Pillow, as the assets CI job installs it)."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import texture_import  # noqa: E402
import validate_assets  # noqa: E402

# What Godot 4.7.2 wrote back after importing the generated T_Stone_01_N.png.
GODOT_REWRITE = """[remap]

importer="texture"
type="CompressedTexture2D"
uid="uid://b0hayedr8qlvi"
path.s3tc="res://.godot/imported/T_Stone_01_N.png-761f0b998ac4026f62013c381456803a.s3tc.ctex"
path.etc2="res://.godot/imported/T_Stone_01_N.png-761f0b998ac4026f62013c381456803a.etc2.ctex"
metadata={
"imported_formats": ["s3tc_bptc", "etc2_astc"],
"vram_texture": true
}

[deps]

source_file="res://assets/textures/temple/T_Stone_01_N.png"
dest_files=["res://.godot/imported/T_Stone_01_N.png-761f0b998ac4026f62013c381456803a.s3tc.ctex", "res://.godot/imported/T_Stone_01_N.png-761f0b998ac4026f62013c381456803a.etc2.ctex"]

[params]

compress/mode=2
compress/high_quality=false
compress/lossy_quality=0.7
compress/uastc_level=0
compress/rdo_quality_loss=0.0
compress/hdr_compression=1
compress/normal_map=1
compress/channel_pack=0
mipmaps/generate=true
mipmaps/limit=-1
roughness/mode=0
roughness/src_normal=""
process/channel_remap/red=0
process/channel_remap/green=1
process/channel_remap/blue=2
process/channel_remap/alpha=3
process/fix_alpha_border=true
process/premult_alpha=false
process/normal_map_invert_y=false
process/hdr_as_srgb=false
process/hdr_clamp_exposure=false
process/size_limit=0
detect_3d/compress_to=0
"""


class TextureImportTest(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp())

    def png(self, name, imp=None):
        path = self.dir / name
        path.write_bytes(b"")
        if imp is not None:
            path.with_name(name + ".import").write_text(imp)
        return path

    def errors(self, path):
        return validate_assets.import_errors(path, path.name)

    def test_written_preset_passes(self):
        for suffix in texture_import.PARAMS:
            path = self.png(f"T_Test_{suffix}.png")
            self.assertTrue(texture_import.write(path, suffix))
            self.assertEqual(self.errors(path), [], suffix)

    def test_godot_rewrite_is_kept(self):
        path = self.png("T_Stone_01_N.png", GODOT_REWRITE)
        self.assertFalse(texture_import.write(path, "N"))
        self.assertIn("uid=", path.with_name(path.name + ".import").read_text())
        self.assertEqual(self.errors(path), [])

    def test_missing_import_fails(self):
        (error,) = self.errors(self.png("T_Stone_01_BC.png"))
        self.assertIn("no T_Stone_01_BC.png.import", error)
        self.assertIn("gen_theme_textures.py", error)

    def test_godot_defaults_fail(self):
        defaults = GODOT_REWRITE.replace("compress/mode=2", "compress/mode=0") \
            .replace("mipmaps/generate=true", "mipmaps/generate=false") \
            .replace("detect_3d/compress_to=0", "detect_3d/compress_to=1")
        (error,) = self.errors(self.png("T_Stone_01_N.png", defaults))
        for part in ("compress/mode=0 (want 2)", "mipmaps/generate=false (want true)",
                     "detect_3d/compress_to=1 (want 0)"):
            self.assertIn(part, error)

    def test_normal_map_flag_fails(self):
        imp = GODOT_REWRITE.replace("compress/normal_map=1", "compress/normal_map=0")
        (error,) = self.errors(self.png("T_Stone_01_N.png", imp))
        self.assertIn("compress/normal_map=0 (want 1)", error)

    def test_rewrite_replaces_wrong_file(self):
        path = self.png("T_Stone_01_ORM.png", GODOT_REWRITE.replace("mipmaps/generate=true", "mipmaps/generate=false"))
        self.assertTrue(texture_import.write(path, "ORM"))
        self.assertEqual(self.errors(path), [])

    def test_other_generated_texture_states_its_choice(self):
        # A sheet another generator writes may turn mipmaps off on purpose, but must say so.
        sheet = GODOT_REWRITE.replace("mipmaps/generate=true", "mipmaps/generate=false")
        self.assertEqual(self.errors(self.png("T_Fire_Motion.png", sheet)), [])
        (error,) = self.errors(self.png("T_Fire_Flame.png", sheet.replace("detect_3d/compress_to=0", "")))
        self.assertIn("detect_3d/compress_to=0", error)


if __name__ == "__main__":
    unittest.main()

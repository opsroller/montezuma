<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Temple colour grade notes

Grade notes for the `temple` board theme (WP-52). The parameters live in
`tools/assets/themes/temple.json` (`grade` and `post`); why they are set up
this way is in
[About post-processing](../../docs/explanation/about-post-processing.md).

## Intent

A gentle primary grade that warms the sunlit limestone and keeps the
shadows a touch cool, so the board reads as late afternoon in a jungle
clearing without tinting the pieces.

| CDL term | Red | Green | Blue | Effect |
|---|---|---|---|---|
| Slope | 1.03 | 1.00 | 0.96 | warm highlights |
| Offset | 0 | 0 | 0.01 | slight blue lift in the shadows |
| Power | 1 | 1 | 1 | none |
| Saturation | 1.06 | | | replaces the old environment saturation of 1.08 |

## Palette check

Output of `python3 tools/assets/grade_lut.py` (also run by
`validate_assets.py`). Limits: a piece colour may move by 0.04 in OKLab,
a surface colour by 0.05, and the closest piece pair may come at most
0.005 closer in any view.

| Colour | OKLab shift |
|---|---|
| Ruby | 0.027 |
| Carnelian | 0.024 |
| Amethyst | 0.015 |
| Moonstone | 0.013 |
| Jade | 0.008 |
| SunDisc | 0.006 |
| Lapis | 0.003 |
| Surfaces (sandstone light and dark, frame, shadow, gold, jungle, torch, glow) | 0.003 to 0.013 |

| View | Closest piece pair before | After |
|---|---|---|
| Normal | 0.155 | 0.169 |
| Greyscale | 0.002 | 0.014 |
| Protanopia | 0.008 | 0.007 |
| Deuteranopia | 0.040 | 0.046 |
| Tritanopia | 0.123 | 0.119 |

The grade separates the pieces slightly in normal vision, greyscale and
deuteranopia, and brings them at most 0.004 closer under protanopia and
tritanopia, within the limit. The greyscale pair near zero (ruby, lapis
and amethyst share a value) is by design: silhouettes separate them.

## Readability captures

`tools/bench/check_readability.py` on `readability.tscn`, 64 frames
averaged, fire lights held. Software Vulkan and GL (lavapipe, llvmpipe);
the real-GPU run on the M4 replaces these.

| Tier | Size | Rule 2 (blur on board) | Rule 4 (contrast change) | Rule 6 (bloom spread) |
|---|---|---|---|---|
| `desktop-high`, before | 540×1170 | fail, 1.209 | fail, 16.1% | fail, 26.29% |
| `desktop-high`, after | 1280×720 | pass, 0.191 | pass, 5.7% | pass, 0% |
| `desktop-high`, after | 540×1170 | pass, 0.32 | pass, 1.9% | fail, 0.11% |
| `web`, before | 540×1170 | pass, 0.004 | pass, 1.1% | fail, 0.228% |
| `web`, after | 540×1170 | pass, 0.0 | pass, 0.4% | pass, 0% |
| `mobile`, before | 540×1170 | pass, 0.007 | pass, 0.2% | pass, 0% |
| `mobile`, after | 540×1170 | pass, 0.0 | pass, 0.1% | pass, 0% |

`desktop-high` at phone size keeps a capture noise floor of about 0.09%
from the FSR 2 upscaler's jitter at 540 internal lines, so its rule 6 is
judged at desktop size, where it runs. Rule 5 (kinds apart in greyscale
and colour-vision deficiency) fails on every tier before and after the
grade and moves by less than 0.5: it is a piece design question.

Frames: [before, `desktop-high`](../../docs/images/lookdev/temple-board-before-wp52-desktop-high.jpg),
[after, `desktop-high`](../../docs/images/lookdev/temple-board-post-desktop-high.jpg),
[after, `web`](../../docs/images/lookdev/temple-board-post-web.jpg),
[after, `mobile`](../../docs/images/lookdev/temple-board-post-mobile.jpg).

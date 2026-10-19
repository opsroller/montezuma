<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Temple colour grade notes

Grade notes for the `temple` board theme (WP-52). The parameters live in
`tools/assets/themes/temple.json` (`grade` and `post`); why they are set up
this way is in
[About post-processing](../../docs/explanation/about-post-processing.md).

## Intent

A near-neutral grade: slope 1.02, 1.01, 1.0, no offset, power 1,
saturation 1.04. The light gives the scene its colour (a 5800 K sun and
a pale blue sky, [About lighting](../../docs/explanation/about-lighting.md#sun-and-sky-balance)).
The first grade, below, warmed the highlights and pulled the shadows down
on top of a 4500 K sun and a tan sky; together they turned the stone and
the mist brown and the scene dark, so it was replaced in the lighting fix
of 19 October 2026.

First grade (replaced):

| CDL term | Red | Green | Blue | Effect |
|---|---|---|---|---|
| Slope | 1.0712 | 1.04 | 0.9984 | warm highlights (1.03, 1.00, 0.96) times a contrast of 1.04 |
| Offset | −0.02 | −0.02 | −0.0096 | slight blue lift in the shadows (0, 0, 0.01), with the contrast pivoting on 0.5 |
| Power | 1 | 1 | 1 | none |
| Saturation | 1.04 | | | |

## Lighting pass values, October 2026

The lighting pass sent environment adjustments tuned on its own
lighting: contrast 1.12, saturation 1.18, AgX contrast 1.6, and a
soft-light glow (mips 3 to 7 at 0.5, 1, 1, 0.8, 0.5; intensity 0.6;
bloom 0.05; threshold 1.6). They go into the grade and `post` as
follows, without loosening any check.

| Value | Asked | Kept | Why |
|---|---|---|---|
| Contrast | 1.12 | 1.04, in the CDL | with saturation 1.18 it moves ruby 0.078, carnelian 0.052 and lapis 0.043 (limit 0.04) and closes the tritanopia pair by 0.041 (limit 0.005). The passing pairs run from contrast 1.10 at saturation 1.00 to contrast 1.00 at saturation 1.06; 1.04 and 1.04 raises both |
| Saturation | 1.18 | 1.04, in the CDL | as above; not a second environment adjustment stacked on the LUT |
| AgX contrast | 1.6 | 1.25 then; 1.5 since the lighting fix of 19 October 2026, where rule 4 passes at 4.5% on `mobile`, `web` and `desktop-high` | at 1.6 amethyst on the light tile came within 0.03 luminance of the tile on `mobile`, so rule 4 could not find it (it passes at 1.25: 0.9%); at 4K on `desktop-high` it also doubled the capture noise rule 6 reads (0.26% to 0.49%) |
| Glow blend, mips, bloom | soft light, 3 to 7, 0.05 | additive, 2 and 3, 0 | at 4K the soft-light glow spread past a quarter cell over 6.1% of the board at threshold 1.6 and 1.1% at 8 (rule 6 allows 0.1%) |
| Glow intensity | 0.6 | 0.5 (unchanged) | 0.6 raised `web`'s rule 6 at 4K from 2.8% to 6.1%; on Forward+ it changes nothing at threshold 8 |
| Glow threshold | 1.6 | 8 (Forward+); 0.6 on mobile and web | at 8 only flames, gem highlights and gold glints bloom |
| Auto exposure range | — | ISO 400 to 480 (was 300 to 640) | the plaza mist pulled the exposure: rules 2 and 4 failed at 4K |

## Lighting fix, 19 October 2026

The lighting fix set AgX contrast to 1.5 and the grade above to near
neutral. Readability on dropkick (RTX 4080 SUPER, 2560 × 1440 for
`desktop-high`): rule 4 passes at 4.5% on `desktop-high`, `mobile` and
`web` (main before the fix: 8.6% on `desktop-high`), rule 6 at 0.000%,
rule 2 at 27 to 30% worst cell contrast loss. Rule 5 fails on main and
still fails, but every closest pair moved apart: on `desktop-high`,
greyscale 7.04 to 9.63 (shade 0) and 4.84 to 9.24 (shade 1).

## Palette check

Output of `python3 tools/assets/grade_lut.py` (also run by
`validate_assets.py`). Limits: a piece colour may move by 0.04 in OKLab,
a surface colour by 0.05, and the closest piece pair may come at most
0.005 closer in any view.

Columns: the WP-52 draft grade, the warm grade it became (slope 1.0712,
1.04, 0.9984; replaced) and the near-neutral grade now in use.

| Colour | OKLab shift, WP-52 draft | Warm grade (replaced) | Now |
|---|---|---|---|
| Ruby | 0.027 | 0.026 | 0.018 |
| Carnelian | 0.024 | 0.029 | 0.016 |
| Amethyst | 0.015 | 0.015 | 0.012 |
| Moonstone | 0.013 | 0.021 | 0.009 |
| Jade | 0.008 | 0.011 | 0.009 |
| SunDisc | 0.006 | 0.010 | 0.007 |
| Lapis | 0.003 | 0.006 | 0.008 |
| Surfaces (sandstone light and dark, frame, shadow, gold, jungle, torch, glow) | 0.003 to 0.013 | 0.005 to 0.019 | 0.004 to 0.010 |

| View | Closest piece pair, ungraded | WP-52 draft | Warm grade (replaced) | Now |
|---|---|---|---|---|
| Normal | 0.155 | 0.169 | 0.172 | 0.164 |
| Greyscale | 0.002 | 0.014 | 0.013 | 0.008 |
| Protanopia | 0.008 | 0.007 | 0.005 | 0.007 |
| Deuteranopia | 0.040 | 0.046 | 0.048 | 0.043 |
| Tritanopia | 0.123 | 0.119 | 0.119 | 0.122 |

The grade separates the pieces slightly in normal vision, greyscale and
deuteranopia, and brings them at most 0.001 closer under protanopia and
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
| `desktop-high`, lighting fold-in, after main's mist | 1280×720 | pass, 0.019 | pass, 6.5% | pass, 0% |
| `web`, first grade, after main's mist | 540×1170 | pass, 0.0 | pass, 3.4% | fail, 1.76% |
| `web`, lighting fold-in, after main's mist | 540×1170 | pass, 0.0 | pass, 3.5% | fail, 1.97% |
| `mobile`, lighting fold-in, after main's mist | 540×1170 | pass, 0.0 | pass, 0.0% | pass, 0% |

The last four rows are on an RTX 4080 SUPER, each window held at its
size by gamescope. `web`'s rule 6 failure came in with main's ground
mist (#79): it fails with the first grade too, and the lighting
fold-in adds 0.2 points. The frames below are these captures.

`desktop-high` at phone size keeps a capture noise floor of about 0.09%
from the FSR 2 upscaler's jitter at 540 internal lines, so its rule 6 is
judged at desktop size, where it runs. Rule 5 (kinds apart in greyscale
and colour-vision deficiency) fails on every tier before and after the
grade and moves by less than 0.5: it is a piece design question.

Frames (WP-52, before the lighting fix of 19 October 2026): [before, `desktop-high`](../../docs/images/lookdev/temple-board-before-wp52-desktop-high.jpg),
[after, `desktop-high`](../../docs/images/lookdev/temple-board-post-desktop-high.jpg),
[after, `web`](../../docs/images/lookdev/temple-board-post-web.jpg),
[after, `mobile`](../../docs/images/lookdev/temple-board-post-mobile.jpg).

### Native 4K, October 2026

Captured full screen at 3840×2160 on an RTX 4080 SUPER (Vulkan), 64
frames averaged. "Before" is this PR with main's ground mist (#79) merged
and the first grade; "after" is the grade and exposure range above. `main`
is shown for reference: it has the old glow.

| Tier | Rule 2 | Rule 4 | Rule 6 |
|---|---|---|---|
| `desktop-high`, `main` | fail, 6.2 | fail, 14.8% | fail, 13.0% |
| `desktop-high`, before | fail, 4.66 | fail, 11.4% | fail, 0.19% |
| `desktop-high`, after | pass, 0.48 | pass, 6.7% | fail, 0.26% |
| `desktop-standard`, after | pass, 0.18 | pass, 5.6% | pass, 0.011% |
| `mobile`, before | pass, 0.0 | pass, 0.8% | pass, 0% |
| `mobile`, after | pass, 0.0 | pass, 0.9% | pass, 0% |
| `web`, before | pass, 0.0 | pass, 5.5% | fail, 1.91% |
| `web`, after | pass, 0.0 | pass, 5.7% | fail, 2.25% |

`desktop-high`'s rule 6 at 4K is not bloom: with glow intensity 0 it
reads the same 0.49% (at AgX 1.6), all of it one-pixel lines along the
dark seams between tiles, where the glow-only and effects-off captures
differ by about 7 levels. That is capture noise the check counts at this
size; recalibrating the check is outside this PR. `web`'s rule 6 at 4K is
real bloom from its 0.6 threshold and failed before this change too;
the 540×1170 figures above are with the first grade.

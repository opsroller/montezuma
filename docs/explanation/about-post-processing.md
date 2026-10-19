<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# About post-processing and the colour grade

*Explanation, written as a decision record. How the board's glow, colour
grade and depth of field are set up on each quality tier, and why. The
parameters are in [Board themes: Colour grade](../reference/board-themes.md#colour-grade)
and [Post-processing](../reference/board-themes.md#post-processing); the
switches per tier in [Quality tiers](../reference/quality-tiers.md); the
grade's palette check and the captures in
[`design/grade/temple.md`](../../design/grade/temple.md).*

## Status

Accepted, October 2026 (WP-52). Built on the physical lighting of WP-47
([About lighting](about-lighting.md)). Costs are estimates, labelled as
such: the only renderer this package could run is software Vulkan and GL
(lavapipe, llvmpipe), which says nothing about GPU time.

## Context

- The [Visual target](../reference/visual-target.md#atmosphere-and-post-processing)
  asks for glow before tonemapping and a LUT colour grade on every tier,
  depth of field on the backdrop only on the desktop tiers, and per-tier
  anti-aliasing and upscaling. Readability rule 6 limits bloom to a
  quarter cell past an emissive edge; rule 2 forbids blur inside the board
  rectangle.
- Since Godot 4.6 glow runs before tonemapping, on scene values after
  exposure. WP-44 compared Godot 4.4.1 and 4.7.2 only on Compatibility.
  With WP-47's physical units and AgX (white 16), sunlit sandstone sits
  well above 1.0 in the HDR buffer, so the theme's old glow (threshold
  1.1, mip levels 3 and 5, soft light blend) bloomed the whole frame on
  `desktop-high`: 26% of the board's pixels failed rule 6 and the
  piece-to-tile contrast dropped 16% (rule 4). Mobile already passed;
  web failed rule 6 at 0.23%.
- Anti-aliasing and upscaling per tier already exist: WP-45 set TAA, FSR
  2.2 / MetalFX and MSAA per tier, and WP-47 capped `desktop-high` at 720
  internal lines. This package keeps them.
- The board camera's distance changes with the window's aspect ratio
  (a portrait phone frame puts it about twice as far as a 16:9 one), so a
  fixed depth-of-field distance either blurs the board on one aspect or
  misses the backdrop on another.

## Decision

1. **Glow only from what is truly bright, on small mips, added.** Glow
   starts at an exposed value of 8 (`hdr_threshold`), well above lit
   stone, so only the flames, gem highlights and gold glints bloom. Mip
   levels 2 and 3 only (radius of a few pixels at the internal
   resolution), additive blend at intensity 0.5, no `glow_bloom` (which
   blooms everything). Additive suits a pre-tonemap glow: soft light and
   screen assume values in 0 to 1. The lighting pass's soft-light glow on
   mips 3 to 7 with bloom 0.05 spread past a quarter cell over 6.1% of the
   board at threshold 1.6 and 1.1% at 8 (rule 6 allows 0.1%), and its
   intensity of 0.6 tripled the bloom on `web`, so the glow stays as it
   was. Mobile and Compatibility put much lower
   values in the buffer glow reads: at a threshold of 1.1 or more nothing
   bloomed there at all. So `mobile` and `web` get their own threshold of
   0.6 (environment metadata `QualityTiers` applies per tier), which
   blooms the flames and gold glints and still passes rule 6.
2. **The grade is a 3D LUT made from an ASC CDL.** The theme sets slope,
   offset and power per channel and a saturation; `grade_lut.py` bakes
   them into a 32³ table that `Environment.adjustment_color_correction`
   applies after tonemapping, in display (sRGB-encoded) space. Godot
   applies it there on every renderer (checked by rendering a square-root
   table on Compatibility: the result matched a display-space model to 1.5
   levels and a linear one to 3.5). Godot samples the table with
   `textureLod` and no half-texel remap, so the generator writes the grade
   at texel centres, `(i + 0.5) / 32`; a table with its points at
   `i / 31` would darken an input of 0.1 to about 0.087 even as an
   identity. The CDL is the colourists' standard
   for a primary grade, so a theme author can read and adjust it, and
   the LUT keeps the cost at one texture read on every tier. The temple
   grade is gentle: warm highlights (red slope 1.03, blue 0.96), a slight
   blue lift in the shadows (offset 0.01), a contrast of 1.04 around mid
   grey folded into slope and offset, and saturation 1.04. The lighting
   pass asked for contrast 1.12 and saturation 1.18 as environment
   adjustments; in the grade those move ruby by 0.078 and close the
   tritanopia pair by 0.04, far past the limits below, and 1.04 and 1.04
   is the nearest pair that passes with both raised. Its AgX contrast of
   1.6 made amethyst on the light tile as bright as the tile on `mobile`
   (rule 4), so AgX contrast stays at 1.25
   ([`design/grade/temple.md`](../../design/grade/temple.md)).
3. **The grade has to keep the palette.** `validate_assets.py` fails a
   grade that moves any piece colour by more than 0.04 in OKLab (about
   one just-noticeable difference) or a surface colour by more than 0.05,
   or that brings the closest pair of piece colours more than 0.005
   closer in normal vision, greyscale or any of the three colour-vision
   deficiencies. The limit is absolute because ruby, lapis and amethyst
   share a value by design and only their silhouettes separate them in
   greyscale.
4. **Depth of field follows the camera.** The theme turns on the far blur
   of its `CameraAttributesPractical` and sets how far past the board it
   starts (`beyond_board`, 0.5 m) and how long it ramps (2 m).
   `BoardCamera3D` writes the far distance every time it refits: the view
   depth of the board's far edge plus that margin. The blur then never
   reaches the board at any aspect ratio, and the jungle floor two metres
   below the platform softens. Both desktop tiers draw it; mobile and web
   do not, as the target says.

   The blur is our own `BackdropBlur` compositor effect, not Godot's depth
   of field, which `QualityTiers` keeps off on every tier. The M4 runs
   (below) put Godot's low box blur at about 3 ms, the whole cost of this
   package: Godot runs it after the upscaler, at the output size, in four
   passes over every pixel, the board included, though the board never
   blurs. `BackdropBlur` reads the same three settings (far distance,
   transition, amount) from the theme's `CameraAttributesPractical` and
   runs after the transparent pass, before TAA and the temporal upscaler,
   at the internal size. It has two compute passes, and both stop at once
   on any pixel short of the far distance. A half-size pass averages a 4×4
   grid of bilinear taps across each pixel's blur radius, weighted by each
   tap's own blur so the board never bleeds into the backdrop. A full-size
   pass then blends that over the sharp image the way Godot's composite
   does. The radius is Godot's (`amount` × 64 pixels at the output size),
   so at 4K it matches Godot's frame to under 1/255 on average, with the
   board unchanged. With no blur shape to choose, the box bokeh, sample
   count and jitter settings went away.
5. **Readability captures average 64 frames, hold the fire lights and
   switch glow off by intensity.**
   With TAA or the temporal upscaler, every frame is jittered, and the
   brazier lights flicker. Two single-frame captures of the same board on
   `desktop-high` differed by 6 or more luma at about 4,300 board pixels,
   which failed rule 2 with no atmosphere at all and counted noise as
   bloom in rule 6. The capture now holds `FlickerLight3D`s at their
   authored energy and saves the mean of 64 frames per effect set. On
   Compatibility, enabling glow also moves the 3D render to an HDR buffer,
   which changes specular highlights by itself: with glow at intensity 0
   the web capture still "bloomed" 0.21% of the board. The capture now
   turns glow off by intensity 0 and keeps it enabled.

## Costs

Measured on the Mac mini M4 (Godot 4.7.2, Metal, release extension),
busiest-board benchmark seed 52, 10 s warm-up and 60 s capture, vsync
off, this package against `main` with both sides run alternately. Both
sides predate the VoxelGI fix, so the absolute frame times are high; the
deltas are what this package adds.

| `desktop-high`, 2560×1440 | `main` | this package | delta |
|---|---|---|---|
| p50 frame time, first version | 47.1 ms | 52.2 ms | +5.1 ms |
| p95 frame time, first version | 55.4 ms | 60.6 ms | +5.2 ms |
| p50 frame time, low box depth of field | 46.2 ms | 49.2 ms | +3.0 ms |
| p95 frame time, low box depth of field | 55.3 ms | 58.5 ms | +3.2 ms |

The first two rows are one run each side; the last two are three runs
each side. Draw calls (p95 about 274), video memory (1,503 MB on both
sides in the second run) and CPU time did not change. The scene report
differs only in the colour grade LUT and depth of field, and a LUT read
in the tonemap pass is a fraction of a millisecond, so the cost is
depth of field. The first version used medium quality with the default
hexagon bokeh, which Godot runs at the 1440p output size after the
upscaler; low quality with a box bokeh, Godot's cheapest, took 2 ms off.
That left the package about 3 ms over a target of under 1 ms, so
`BackdropBlur` replaced Godot's depth of field (decision 4). On the RTX
4080 at 3840×2160, `--gpu-profile` puts Godot's low box blur at 0.63 ms
on `desktop-high` and 0.58 ms on `desktop-standard`, and `BackdropBlur`
at 0.05 ms and 0.09 ms
([Quality tiers](../reference/quality-tiers.md#backdrop-depth-of-field)).
Its half-size buffer takes about 17 MB of video memory at native 4K.

With `BackdropBlur`, the M4 was re-measured at the owner's benchmark
standard: `desktop-high` full screen at native 5120×2880, no upscaler,
seed 52, 5 s warm-up and one 10 s capture per side, run back to back
against the merge base db718de
([Benchmark results](../reference/benchmark-results.md#mac-mini-m4-at-native-5k)).

| `desktop-high`, 5120×2880 | merge base | this package | delta |
|---|---|---|---|
| p50 frame time | 113.9 ms | 114.8 ms | +0.9 ms |
| p95 frame time | 115.1 ms | 115.9 ms | +0.8 ms |
| video memory max | 2,107 MB | 2,135 MB | +28 MB |

The whole package now costs under 1 ms at p50 and p95 on the M4, at
about four times the pixels of the 1440p rows above.

`desktop-standard` at 1920×1080 measured p95 17.5 ms on `main` and
17.4 ms with this package. Both sit on the 60 Hz floor, because the
Metal driver stays display-synced with vsync off, so the cost there is
below what this Mac resolves. Mobile and web add only glow and the LUT,
no new passes. No draw calls are added on any tier.

## Options considered

| Option | Why not |
|---|---|
| Keep soft light glow and lower its intensity | Soft light on pre-tonemap values above 1 brightens all lit stone; rule 6 still failed at any visible intensity |
| Raise only the threshold, keep mip 5 | Mip 5 spreads more than a quarter cell at phone size |
| One glow threshold for every tier | 8 blooms nothing on Mobile and Compatibility; 0.6 blooms all lit stone on Forward+ |
| A LUT painted in an image editor | Not reviewable as a diff and not checkable against the palette; the CDL numbers are |
| Grading with the environment's brightness, contrast and saturation only | No per-channel control, so no warm highlights or cool shadows |
| A fixed depth-of-field distance | Blurs the board on portrait windows or misses the backdrop on wide ones |
| Near blur too | Nothing in the frame is nearer than the board |
| Godot's own depth of field at low quality with a box bokeh | About 3 ms on the M4: it runs after the upscaler at the output size, in four passes over every pixel, the board included |
| Turning backdrop depth of field off on `desktop-high` | The target asks for it on both desktop tiers, and `BackdropBlur` costs a tenth as much |
| Raising rule 6's thresholds to absorb the capture noise | Hides real bloom; the noise came from the capture, so the capture changed |

## Consequences

- A theme gets its grade and glow from its parameter file; a new theme
  copies the temple's `grade` and `post` sections and checks rules 2, 4
  and 6 with the readability capture on every tier.
- A grade change regenerates the LUT on the next texture run and fails
  validation when it breaks the palette.
- Readability captures take 64 extra frames per effect set (about a
  second on a GPU, minutes on a software renderer).
- Rule 5 (kinds apart in greyscale and colour-blindness) still fails on
  every tier, before and after this package: the grade moves it by less
  than 0.5. It is a piece design question (silhouettes and values), not a
  post-processing one.
- `desktop-high`'s rule 6 is judged at desktop size (1280×720). At phone
  size its capture keeps a noise floor of about 0.09% from the FSR 2
  jitter at 540 internal lines, just under the 0.1% limit, so a real
  bloom regression there would hide in the noise.

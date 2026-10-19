<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# Direction 3 type pairing

The wordmark is not set in a font: it is drawn on a 10-unit cap grid with a
2-unit stem and 45-degree cut corners (`wordmark.svg`), the cut
corners echoing ground stone rather than a pixel font. The fonts below are
for everything else.

| Job | Family | Licence | Source | Why |
|---|---|---|---|---|
| Display: titles, menu headings, store and site headlines | **Rubik** (variable, wght 300 to 900) | SIL Open Font License 1.1; copyright 2015 The Rubik Project Authors; no Reserved Font Name | `google/fonts` `ofl/rubik/` (OFL.txt and METADATA.pb read 2026-10-19) | Slightly rounded heavy grotesque: friendly, chunky like the drawn wordmark without imitating it; Latin Extended covers Spanish, French, Portuguese, German and Nahuatl spellings |
| Text: UI labels, HUD numbers, body | **Atkinson Hyperlegible Next** (variable, wght 200 to 800) | SIL Open Font License 1.1; copyright 2020-2024 The Atkinson Hyperlegible Next Project Authors; no Reserved Font Name | `google/fonts` `ofl/atkinsonhyperlegiblenext/` (read 2026-10-19) | Built for low-vision legibility: distinct 1/l/I and 0/O at the 16 px phone caption size |

Both OFL fonts may be embedded in the game, served as web fonts and used
in a logo; the OFL text must ship with them. Font files were downloaded to
the scratch directory only to look at them and are not in the repo.

Scale sketch (px, dark game UI): caption 16 Atkinson 500; body 18 Atkinson
400; button 20 Atkinson 700; heading 28 Rubik 700; title 48 Rubik 800,
tracking +2 %. Numerals: Atkinson Next tabular figures for scores.

Not chosen: any "Aztec" or faux-glyph display font (brief: no pastiche).

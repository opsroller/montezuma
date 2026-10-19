<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
<!-- Copyright (C) 2026 Charles (opsroller) and Montezuma contributors -->
# Direction 2 type pairing: the step-fret band

The wordmark is drawn, not typed (`wordmark.svg`, drawn on a
2-unit stroke grid with stepped diagonals). Fonts carry everything else.

| Job | Family | Licence | Source | Why |
|---|---|---|---|---|
| Display: titles, level names, store and site headings | **Big Shoulders Display** (variable, wght 100 to 900; use 700 to 800) | SIL Open Font License 1.1, no Reserved Font Name; Copyright 2019 The Big Shoulders Project Authors | <https://github.com/google/fonts/tree/main/ofl/bigshouldersdisplay> (upstream <https://github.com/xotypeco/big_shoulders>) | Condensed, square-shouldered capitals that sit with the stepped wordmark without imitating it; latin-ext subset covers ñ, ç, ã, ß, œ |
| Text: UI, HUD numbers, body, captions | **Atkinson Hyperlegible Next** (variable, wght 200 to 800) | SIL Open Font License 1.1, no Reserved Font Name; Copyright 2020-2024 The Atkinson Hyperlegible Next Project Authors | <https://github.com/google/fonts/tree/main/ofl/atkinsonhyperlegiblenext> (upstream <https://github.com/googlefonts/atkinson-hyperlegible-next>) | Built for low-vision legibility: 1, I, l and 0, O apart; clear at 12 to 16 px caption sizes on a phone |

Licences read on 2026-10-19 from each `OFL.txt` in the google/fonts
repository. The OFL covers desktop use, app embedding, web fonts and
static images; artwork made with the fonts is not bound by it. Shipping a
font file means shipping its `OFL.txt` beside it. No font file is
committed in this direction.

## Tested

Rendered in headless Chromium with: `LEVEL 12 · TENOCHTITLAN`,
`Motecuhzoma · xicalcoliuhqui · Niveau terminé — Größe · Señal · ação`,
`¿Listo? ¡Sí!`, `œuvre`, `Übung ß`, `coração`, digits `0123456789` and a
12 px caption. All glyphs present; no fallback.

## Scale sketch

| Token idea | Family, weight | Size / line height |
|---|---|---|
| display-xl | Big Shoulders Display 800, caps, +2 % tracking | 56 / 60 |
| display-m | Big Shoulders Display 700 | 32 / 36 |
| body | Atkinson Hyperlegible Next 400 | 16 / 24 |
| hud-number | Atkinson Hyperlegible Next 700, tabular | 20 / 24 |
| caption | Atkinson Hyperlegible Next 400 | 12 / 16 (16 on phones) |

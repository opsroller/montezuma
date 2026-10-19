# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Cut the last movie frame of render_all.tscn into the key-art renders.

    python3 design/keyart/gen/finish.py <frame.png>

The frame is the 8192x8192 canvas laid out in render_all.tscn; every shot is
rendered at twice its output size and box-filtered (Lanczos) down. Writes
design/keyart/renders/{mark-hero,mark-on-plaza,app-icon-3d,title-wordmark,lockup-hero}.png.
"""

import sys
from pathlib import Path

from PIL import Image

RENDERS = Path(__file__).resolve().parents[1] / 'renders'
# name: (x, y, w, h) on the canvas, output size, keep alpha
SHOTS = {
    'mark-hero': ((0, 0, 4096, 4096), (2048, 2048), False),
    'mark-on-plaza': ((4096, 0, 3840, 2160), (1920, 1080), False),
    'app-icon-3d': ((4096, 2160, 2048, 2048), (1024, 1024), False),
    'title-wordmark': ((4096, 4256, 3840, 1200), (1920, 600), True),
    'lockup-hero': ((0, 4096, 4096, 4096), (2048, 2048), False),
}


def main():
    frame = Image.open(sys.argv[1]).convert('RGBA')
    for name, ((x, y, w, h), size, alpha) in SHOTS.items():
        im = frame.crop((x, y, x + w, y + h))
        if alpha:
            # resize in premultiplied space so edges keep their colour
            im = im.convert('RGBa').resize(size, Image.LANCZOS).convert('RGBA')
        else:
            im = im.convert('RGB').resize(size, Image.LANCZOS)
        im.save(RENDERS / f'{name}.png', optimize=True)
        print(RENDERS / f'{name}.png')


if __name__ == '__main__':
    main()

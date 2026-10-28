# SPDX-License-Identifier: GPL-2.0
"""Lay rendered look-dev frames out as a labelled contact sheet.

    python3 tools/assets/contact_sheet.py <frames dir> <sheet.png> [--per 9]
    python3 tools/assets/contact_sheet.py <dir> <out.png> --only Heliconia,Ceiba

A 4K frame per specimen is the right thing to judge one plant on and the wrong thing
to judge a library on. Nearly every fault in the plant library was found by seeing
nine species beside each other - a palm rendering pure white, a grass tuft buried in
its own litter, moss the colour of butter, a leaf floating seven metres above the
plant that shed it - and none of them by looking at one specimen at a time. Frames
over about eight get split across numbered sheets rather than shrunk further, because
a tile too small to see a leaf in defeats the point.

Writes <out>.png, or <out>-1.png, <out>-2.png ... when there is more than one sheet.
"""

import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw

TILE = (620, 349)          # 16:9, three to a row, readable at a glance
LABEL = 18
BACKDROP = (24, 24, 26)
TEXT = (214, 214, 214)


def frames(directory, only=None, empties=False):
    out = []
    for path in sorted(directory.glob("*.png")):
        if not empties and path.stem.endswith("_Empty"):
            continue
        if only and not any(k.lower() in path.stem.lower() for k in only):
            continue
        out.append(path)
    return out


def label_of(path):
    return (path.stem
            .replace("LD_Plants_", "row: ")
            .replace("LD_Plant_", "")
            .replace("LD_", ""))


def sheet(paths, out_path):
    cols = 3 if len(paths) > 4 else min(2, len(paths))
    rows = (len(paths) + cols - 1) // cols
    cw, ch = TILE
    image = Image.new("RGB", (cols * cw, rows * (ch + LABEL)), BACKDROP)
    draw = ImageDraw.Draw(image)
    for i, path in enumerate(paths):
        with Image.open(path) as frame:
            tile = frame.convert("RGB").resize(TILE, Image.LANCZOS)
        x, y = (i % cols) * cw, (i // cols) * (ch + LABEL)
        image.paste(tile, (x, y))
        draw.text((x + 6, y + ch + 3), label_of(path), fill=TEXT)
    image.save(out_path)
    return out_path


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("directory", type=Path, help="where the captured frames are")
    ap.add_argument("out", type=Path, help="sheet to write (gets -1, -2 ... if split)")
    ap.add_argument("--per", type=int, default=4,
                    help="frames per sheet (default 4). Nine panes of a 3840x2160 "
                         "frame land at about 600 px each, which is too small to "
                         "judge a leaf edge, a tear or a contact shadow - the things "
                         "the sheet exists to compare")
    ap.add_argument("--only", help="comma-separated substrings of the frame names")
    ap.add_argument("--empties", action="store_true",
                    help="include the _Empty pages, which are normally measurement only")
    args = ap.parse_args()

    only = args.only.split(",") if args.only else None
    found = frames(args.directory, only, args.empties)
    if not found:
        sys.exit(f"{args.directory}: no frames matched")

    batches = [found[i:i + args.per] for i in range(0, len(found), args.per)]
    for index, batch in enumerate(batches, 1):
        out = (args.out if len(batches) == 1
               else args.out.with_name(f"{args.out.stem}-{index}{args.out.suffix}"))
        print(f"{sheet(batch, out)}  {len(batch)} frames")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
"""Pack a directory into a .tar.gz or .zip with stable bytes.

Entries are sorted, owners are dropped and every timestamp is
SOURCE_DATE_EPOCH, so the archive depends only on the files' contents
and modes.

Usage: package.py <tar.gz|zip> <root> <name> <archive>
  packs <root>/<name> (kept as the top folder in the archive) into <archive>.
"""

import gzip
import io
import os
import sys
import tarfile
import time
import zipfile


def entries(root, name):
    top = os.path.join(root, name)
    found = [name]
    for base, dirs, files in os.walk(top):
        dirs.sort()
        rel = os.path.relpath(base, root)
        found += [os.path.join(rel, d) for d in dirs]
        found += [os.path.join(rel, f) for f in sorted(files)]
    return sorted(set(found))


def reject_symlinks(root, name):
    # Zip has no portable symlinks; keep both formats the same by refusing them.
    links = [rel for rel in entries(root, name) if os.path.islink(os.path.join(root, rel))]
    if links:
        sys.exit("symlinks are not packaged: " + ", ".join(links))


def pack_tar(root, name, archive, epoch):
    reject_symlinks(root, name)
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w", format=tarfile.PAX_FORMAT) as tar:
        for rel in entries(root, name):
            info = tar.gettarinfo(os.path.join(root, rel), arcname=rel)
            info.mtime = epoch
            info.uid = info.gid = 0
            info.uname = info.gname = ""
            if info.isfile():
                with open(os.path.join(root, rel), "rb") as f:
                    tar.addfile(info, f)
            else:
                tar.addfile(info)
    with open(archive, "wb") as out:
        with gzip.GzipFile(filename="", mode="wb", fileobj=out, mtime=epoch) as gz:
            gz.write(buf.getvalue())


def pack_zip(root, name, archive, epoch):
    reject_symlinks(root, name)
    # Zip timestamps start in 1980 and have two-second resolution.
    stamp = time.gmtime(max(epoch, 315532800))[:6]
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
        for rel in entries(root, name):
            path = os.path.join(root, rel)
            st = os.lstat(path)
            is_dir = os.path.isdir(path)
            info = zipfile.ZipInfo(rel.replace(os.sep, "/") + ("/" if is_dir else ""), stamp)
            info.create_system = 3  # Unix, so the mode bits below are honoured
            info.external_attr = (st.st_mode & 0xFFFF) << 16
            if is_dir:
                info.external_attr |= 0x10
                z.writestr(info, b"")
            else:
                info.compress_type = zipfile.ZIP_DEFLATED
                with open(path, "rb") as f:
                    z.writestr(info, f.read())


def main(argv):
    if len(argv) != 5 or argv[1] not in ("tar.gz", "zip"):
        sys.exit(__doc__)
    fmt, root, name, archive = argv[1:]
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "0"))
    (pack_tar if fmt == "tar.gz" else pack_zip)(root, name, archive, epoch)


if __name__ == "__main__":
    main(sys.argv)

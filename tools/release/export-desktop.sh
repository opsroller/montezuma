#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Exports one desktop target with Godot and packages it as
# <outdir>/montezuma-<version>-<platform>-<arch>.<ext> plus a .sha256 file.
#
# Usage: tools/release/export-desktop.sh <linux|windows|macos> <version> <outdir>
# Needs: GODOT (path to the editor, default "godot"), the matching export
# template (fetch-godot.sh) and the extension's release library for the
# platform in game/bin/. Run from the repository root.
set -euo pipefail

platform="${1:?usage: export-desktop.sh <linux|windows|macos> <version> <outdir>}"
version="${2:?missing version}"
outdir="${3:?missing outdir}"
godot="${GODOT:-godot}"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

case "$platform" in
  linux)   preset=Linux;   arch=x86_64;    binary=montezuma.x86_64;   format=tar.gz ;;
  windows) preset=Windows; arch=x86_64;    binary=montezuma.exe;      format=zip ;;
  macos)   preset=macOS;   arch=universal; binary="Montezuma's.app";  format=zip ;;
  *) echo "unknown platform: $platform" >&2; exit 2 ;;
esac

name="montezuma-$version-$platform-$arch"
mkdir -p "$outdir"
outdir="$(cd "$outdir" && pwd)"
stage="$outdir/stage/$name"
rm -rf "$stage"
mkdir -p "$stage"

# Commit time for archive timestamps, so packaging adds no clock noise.
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct)}"

"$godot" --headless --path game --import >/dev/null 2>&1 || true
mkdir -p "$outdir/logs"
log="$outdir/logs/$name.export.log"
if ! "$godot" --headless --path game --export-release "$preset" "$stage/$binary" >"$log" 2>&1; then
  tail -n 40 "$log" >&2
  echo "Godot export of preset \"$preset\" failed (full log: $log)" >&2
  exit 1
fi
# Godot only warns when ad-hoc signing fails; an unsigned macOS app will not
# start on Apple silicon, so treat it as a failure.
if grep -E "CodeSign|Code Signing" "$log" | grep -qiE "error|fail"; then
  grep -E "CodeSign|Code Signing" "$log" >&2
  exit 1
fi

cp LICENSE "$stage/LICENSE.txt"

archive="$outdir/$name.$format"
python3 "$here/package.py" "$format" "$outdir/stage" "$name" "$archive"
(cd "$outdir" && sha256sum "$name.$format" > "$name.$format.sha256")
rm -rf "$outdir/stage"
cat "$archive.sha256"

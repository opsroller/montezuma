#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Exports the web demo (Godot preset "Web": single-threaded, GDExtension on)
# and packages it as <outdir>/montezuma-<version>-web.zip plus a .sha256
# file. The archive's folder holds montezuma.js, montezuma.wasm,
# montezuma.pck, montezuma.side.wasm, the extension's .wasm, the export's own
# montezuma.html (for previewing; the site uses its own page), LICENSE.txt
# and the notices. Fails when the files gzip to more than the web budget
# (40 MB, docs/reference/visual-target.md).
#
# Usage: tools/release/web-export.sh <version> <outdir>
# Needs: GODOT (path to the editor, default "godot"), the
# web_dlink_nothreads_release.zip template (fetch-godot.sh) and
# game/bin/libmontezuma.web.template_release.wasm32.nothreads.wasm.
# Run from the repository root.
set -euo pipefail

version="${1:?usage: web-export.sh <version> <outdir>}"
outdir="${2:?missing outdir}"
godot="${GODOT:-godot}"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
budget=$((40 * 1000 * 1000))

lib=game/bin/libmontezuma.web.template_release.wasm32.nothreads.wasm
[[ -f "$lib" ]] || { echo "missing $lib: build the extension with Emscripten first" >&2; exit 1; }

name="montezuma-$version-web"
mkdir -p "$outdir"
outdir="$(cd "$outdir" && pwd)"
stage="$outdir/stage/$name"
rm -rf "$stage"
mkdir -p "$stage" "$outdir/logs"

# Commit time for archive timestamps, so packaging adds no clock noise.
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct)}"

# The game reports its version in montezuma:ready; stamp it for this export
# only and put project.godot back afterwards.
cp game/project.godot "$outdir/project.godot.orig"
trap 'cp "$outdir/project.godot.orig" game/project.godot; rm -f "$outdir/project.godot.orig"' EXIT
sed -i.bak "s|^config/name=.*|&\nconfig/version=\"$version\"|" game/project.godot && rm -f game/project.godot.bak

# The import pass prints errors for libraries of other targets; the export
# below is what must succeed, so its log is the one that gates.
"$godot" --headless --path game --import >"$outdir/logs/$name.import.log" 2>&1 || true
log="$outdir/logs/$name.export.log"
if ! "$godot" --headless --path game --export-release Web "$stage/montezuma.html" >"$log" 2>&1; then
  tail -n 40 "$log" >&2
  echo "Godot export of preset \"Web\" failed (full log: $log)" >&2
  exit 1
fi
for f in montezuma.js montezuma.wasm montezuma.pck montezuma.side.wasm "$(basename "$lib")"; do
  [[ -f "$stage/$f" ]] || { ls -l "$stage" >&2; echo "export is missing $f" >&2; exit 1; }
done

cp LICENSE "$stage/LICENSE.txt"
python3 "$here/notices.py" "$stage"

# What a visitor downloads: every file, compressed as a server would.
size=0
for f in "$stage"/*; do
  size=$((size + $(gzip -9 -c "$f" | wc -c)))
done
echo "Web demo: $size bytes gzipped (budget $budget)"
if (( size > budget )); then
  echo "the web demo is over its download budget" >&2
  exit 1
fi

archive="$outdir/$name.zip"
python3 "$here/package.py" zip "$outdir/stage" "$name" "$archive"
(cd "$outdir" && sha256sum "$name.zip" > "$name.zip.sha256")
rm -rf "$outdir/stage"
cat "$archive.sha256"

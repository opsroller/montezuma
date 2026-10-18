#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Downloads the pinned Godot editor (Linux x86_64) and export templates,
# verifies both against their SHA-512 checksums, and installs only the
# templates asked for.
#
# Usage: tools/release/fetch-godot.sh <dir> <template>...
#   <dir>       where the editor binary goes (as <dir>/godot) and downloads are kept
#   <template>  file names inside the templates archive, e.g. linux_release.x86_64
#
# Templates are installed into Godot's per-user templates folder, where the
# editor looks for them. Downloads already in <dir> are re-verified, not
# re-fetched, so <dir> can be cached between runs.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source-path=SCRIPTDIR source=godot.env
source "$here/godot.env"

dir="${1:?usage: fetch-godot.sh <dir> <template>...}"
shift
mkdir -p "$dir"
base="https://github.com/godotengine/godot/releases/download/$GODOT_TAG"

fetch() {  # fetch <file> <sha512>
  local file="$1" sum="$2"
  if [[ -f "$dir/$file" ]] && echo "$sum  $dir/$file" | sha512sum -c --status -; then
    return
  fi
  curl -sSfL --retry 3 -o "$dir/$file" "$base/$file"
  echo "$sum  $dir/$file" | sha512sum -c -
}

fetch "$GODOT_EDITOR_LINUX_ZIP" "$GODOT_EDITOR_LINUX_SHA512"
unzip -qo "$dir/$GODOT_EDITOR_LINUX_ZIP" -d "$dir"
mv -f "$dir/${GODOT_EDITOR_LINUX_ZIP%.zip}" "$dir/godot"
chmod +x "$dir/godot"

if [[ $# -gt 0 ]]; then
  fetch "$GODOT_TEMPLATES_TPZ" "$GODOT_TEMPLATES_SHA512"
  templates="${XDG_DATA_HOME:-$HOME/.local/share}/godot/export_templates/$GODOT_TEMPLATES_DIR_NAME"
  mkdir -p "$templates"
  members=(templates/version.txt)
  for t in "$@"; do members+=("templates/$t"); done
  unzip -qjo "$dir/$GODOT_TEMPLATES_TPZ" "${members[@]}" -d "$templates"
  echo "Installed templates in $templates: $*"
fi

"$dir/godot" --version

#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Installs the pinned Emscripten SDK (EMSDK_VERSION in godot.env) into <dir>
# and prints the path of its emsdk_env.sh. emsdk is cloned at the tag of the
# same version, and it checks the SDK it downloads against the release
# hashes that tag lists. An existing install of the right version in <dir>
# is reused, so <dir> can be cached between runs.
#
# Usage: tools/release/web-fetch-emsdk.sh <dir>
#   then: source "$(tools/release/web-fetch-emsdk.sh <dir>)"
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source-path=SCRIPTDIR source=godot.env
source "$here/godot.env"

dir="${1:?usage: web-fetch-emsdk.sh <dir>}"
if [[ ! -x "$dir/emsdk" ]] || [[ "$(git -C "$dir" describe --tags --exact-match 2>/dev/null)" != "$EMSDK_VERSION" ]]; then
  rm -rf "$dir"
  git clone -q --depth 1 --branch "$EMSDK_VERSION" https://github.com/emscripten-core/emsdk.git "$dir"
fi
"$dir/emsdk" install "$EMSDK_VERSION" >&2
"$dir/emsdk" activate "$EMSDK_VERSION" >&2
echo "$dir/emsdk_env.sh"

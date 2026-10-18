#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Bakes every board theme's LightmapGI (static path-traced light with probes
# for the moving pieces). The bakes are build outputs, like the generated
# textures: git ignores them and CI restores or makes them before exporting.
# Reference: docs/reference/board-themes.md#lighting; how-to:
# docs/how-to/make-a-board-theme.md.
#
# Usage: tools/assets/bake_lightmaps.sh [--godot PATH] [--xvfb] [--force] [--check]
#   --godot PATH  Godot 4.7.2 editor binary (default: godot on PATH)
#   --xvfb        run the editor under xvfb-run (machines without a display)
#   --force       bake even when the inputs have not changed
#   --check       bake nothing; exit 0 when every bake is up to date, else 3
#
# Needs: the extension built (its editor plugin drives the bake), the theme
# textures generated (tools/assets/gen_theme_textures.py) and a Vulkan
# driver; on a machine without a GPU, Mesa's lavapipe works (slowly).
# Each theme's bake scene is game/assets/lookdev/LD_<Theme>_Bake.tscn; its
# output lands next to the backdrop scene as PF_Environment.lmbake plus
# textures, with a .lightmap-inputs stamp of what it was baked from. A theme
# whose stamp still matches is skipped.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
godot=godot xvfb=0 force=0 check=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --godot) godot="$2"; shift 2 ;;
    --xvfb) xvfb=1; shift ;;
    --force) force=1; shift ;;
    --check) check=1; shift ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done

# What a theme's bake depends on: its static meshes and their import
# settings (lightmap UV2), materials, shaders, generated textures (by their
# stamp), the scenes, the bake driver and the Godot release.
inputs_hash() {  # inputs_hash <theme>
  local t="$1"
  (
    cd "$root"
    {
      find "game/assets/models/$t/board" "game/assets/models/$t/backdrop" \
        -maxdepth 1 \( -name '*.obj' -o -name '*.obj.import' -o -name 'PF_*.tscn' \) -print
      find "game/assets/materials/$t" game/assets/materials/shaders \( -name '*.tres' -o -name '*.gdshader*' \) -print
      ls "game/assets/textures/$t/.inputs" "game/assets/lookdev/LD_${t^}_Bake.tscn" \
        extension/src/render/lightmap_baker.cpp tools/release/godot.env game/project.godot
    } | grep -v '\.uid$' | LC_ALL=C sort | xargs sha256sum | sha256sum | cut -d' ' -f1
  )
}

scenes=() stamps=()
for scene in "$root"/game/assets/lookdev/LD_*_Bake.tscn; do
  [[ -e "$scene" ]] || continue
  name="$(basename "$scene" .tscn)"; name="${name#LD_}"; theme="${name%_Bake}"; theme="${theme,,}"
  stamp="$root/game/assets/models/$theme/backdrop/.lightmap-inputs"
  hash="$(inputs_hash "$theme")"
  if [[ "$force" == 0 && -f "$stamp" && "$(cat "$stamp")" == "$hash" \
        && -f "$root/game/assets/models/$theme/backdrop/PF_Environment.lmbake" ]]; then
    echo "$theme: lightmap up to date"
    continue
  fi
  scenes+=("res://${scene#"$root/game/"}")
  stamps+=("$stamp=$hash")
done
if [[ ${#scenes[@]} -eq 0 ]]; then exit 0; fi
if [[ "$check" == 1 ]]; then
  printf 'stale: %s\n' "${scenes[@]}"
  exit 3
fi

cmd=("$godot" --editor --path "$root/game" --rendering-driver vulkan -- "--bake-lightmaps=$(IFS=,; echo "${scenes[*]}")")
if [[ "$xvfb" == 1 ]]; then cmd=(xvfb-run -a -s "-screen 0 1920x1080x24" "${cmd[@]}"); fi
log="$(mktemp)"
status=0
# The editor re-saves project.godot in its own layout (dropping the SPDX
# line and settings equal to defaults); put the committed file back.
project="$(mktemp)"
cp "$root/game/project.godot" "$project"
"${cmd[@]}" 2>&1 | tee "$log" | grep --line-buffered 'LIGHTMAP_BAKE' || true
cp "$project" "$root/game/project.godot"
rm -f "$project"
# The driver prints one result line per scene; trust those, not the exit
# status alone (the editor's own exit can be noisy).
for s in "${scenes[@]}"; do
  grep -q "LIGHTMAP_BAKE ok $s " "$log" || { echo "bake failed: $s" >&2; status=1; }
done
rm -f "$log"
if [[ "$status" == 0 ]]; then
  for entry in "${stamps[@]}"; do printf '%s\n' "${entry#*=}" > "${entry%%=*}"; done
fi
exit "$status"

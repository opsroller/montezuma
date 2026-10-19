# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Bakes every board theme's LightmapGI (static path-traced light with probes
# for the moving pieces). The bakes are build outputs, like the generated
# textures: git ignores them and CI restores or makes them before exporting.
# Reference: docs/reference/board-themes.md#lighting; how-to:
# docs/how-to/make-a-board-theme.md.
#
# Usage: cmake [-DGODOT=<editor>] [-DXVFB=ON] [-DFORCE=ON] [-DCHECK=ON [-DCHECK_OUTPUT=<file>]]
#              -P tools/assets/bake_lightmaps.cmake
#   GODOT         Godot 4.7.2 editor binary (default: godot on PATH)
#   XVFB          run the editor under xvfb-run (machines without a display)
#   FORCE         bake even when the inputs have not changed
#   CHECK         bake nothing; print which bakes are stale. With
#                 CHECK_OUTPUT, also append stale=true or stale=false to
#                 that file (a GitHub Actions step output)
#
# Needs: the extension built (its editor plugin drives the bake), the theme
# textures generated (tools/assets/gen_theme_textures.py) and a Vulkan
# driver; on a machine without a GPU, Mesa's lavapipe works (slowly).
# Each theme's bake scene is game/assets/lookdev/LD_<Theme>_Bake.tscn; its
# output lands next to the backdrop scene as PF_Environment.lmbake plus
# textures, with a .lightmap-inputs stamp of what it was baked from. A theme
# whose stamp still matches is skipped. Before a bake the project is
# imported (stopped after 900 s); the editor is stopped after 900 s per
# scene it bakes. Either limit failing the step means the editor hung.
include("${CMAKE_CURRENT_LIST_DIR}/../release/steps.cmake")
if(NOT GODOT)
  set(GODOT godot)
endif()
set(root "${MONTEZUMA_ROOT}")

# What a theme's bake depends on: its static meshes and their import
# settings (lightmap UV2), materials, shaders, generated textures (by their
# stamp), the scenes, the bake driver and the Godot release. The hash is
# sha256sum's listing ("<hash>  <path>" per file, paths from the root,
# sorted) hashed again, so stamps match the earlier bake tool's.
function(inputs_hash out theme scene)
  file(GLOB files RELATIVE "${root}"
    "${root}/game/assets/models/${theme}/board/*.obj"
    "${root}/game/assets/models/${theme}/board/*.obj.import"
    "${root}/game/assets/models/${theme}/board/PF_*.tscn"
    "${root}/game/assets/models/${theme}/backdrop/*.obj"
    "${root}/game/assets/models/${theme}/backdrop/*.obj.import"
    "${root}/game/assets/models/${theme}/backdrop/PF_*.tscn")
  file(GLOB_RECURSE materials RELATIVE "${root}"
    "${root}/game/assets/materials/${theme}/*.tres"
    "${root}/game/assets/materials/${theme}/*.gdshader*"
    "${root}/game/assets/materials/shaders/*.tres"
    "${root}/game/assets/materials/shaders/*.gdshader*")
  list(APPEND files ${materials}
    "game/assets/textures/${theme}/.inputs" "${scene}"
    extension/src/render/lightmap_baker.cpp tools/release/godot.env game/project.godot)
  list(FILTER files EXCLUDE REGEX "\\.uid$")
  list(REMOVE_DUPLICATES files)
  list(SORT files)
  set(listing "")
  foreach(f IN LISTS files)
    if(NOT EXISTS "${root}/${f}")
      montezuma_fail("bake input missing: ${f}")
    endif()
    file(SHA256 "${root}/${f}" h)
    string(APPEND listing "${h}  ${f}\n")
  endforeach()
  string(SHA256 h "${listing}")
  set(${out} "${h}" PARENT_SCOPE)
endfunction()

set(scenes "")
set(stamps "")
set(hashes "")
file(GLOB bake_scenes "${root}/game/assets/lookdev/LD_*_Bake.tscn")
foreach(scene IN LISTS bake_scenes)
  get_filename_component(name "${scene}" NAME_WE)
  string(REGEX REPLACE "^LD_(.*)_Bake$" "\\1" theme "${name}")
  string(TOLOWER "${theme}" theme)
  set(backdrop "${root}/game/assets/models/${theme}/backdrop")
  file(RELATIVE_PATH rel "${root}" "${scene}")
  inputs_hash(hash "${theme}" "${rel}")
  message(STATUS "${theme}: bake inputs ${hash}")
  if(NOT FORCE AND EXISTS "${backdrop}/.lightmap-inputs" AND EXISTS "${backdrop}/PF_Environment.lmbake")
    file(STRINGS "${backdrop}/.lightmap-inputs" stamp LIMIT_COUNT 1)
    if(stamp STREQUAL hash)
      message(STATUS "${theme}: lightmap up to date")
      continue()
    endif()
  endif()
  file(RELATIVE_PATH res "${root}/game" "${scene}")
  list(APPEND scenes "res://${res}")
  list(APPEND stamps "${backdrop}/.lightmap-inputs")
  list(APPEND hashes "${hash}")
endforeach()

if(CHECK)
  if(scenes)
    foreach(s IN LISTS scenes)
      montezuma_print("stale: ${s}")
    endforeach()
    set(stale true)
  else()
    set(stale false)
  endif()
  if(CHECK_OUTPUT)
    file(APPEND "${CHECK_OUTPUT}" "stale=${stale}\n")
  endif()
  return()
endif()
if(NOT scenes)
  return()
endif()

set(work "${root}/game/.godot/lightmap-bake")
file(MAKE_DIRECTORY "${work}")
set(log "${work}/bake.log")
# The editor re-saves project.godot in its own layout (dropping the SPDX
# line and settings equal to defaults); put the committed file back, also
# when the editor fails or hangs.
set(project "${root}/game/project.godot")
set(saved "${work}/project.godot.orig")
file(COPY_FILE "${project}" "${saved}")
function(restore_project)
  file(COPY_FILE "${saved}" "${project}")
  file(REMOVE "${saved}")
endfunction()
set(MONTEZUMA_ON_FAIL restore_project)

set(prefix "")
if(XVFB)
  set(prefix xvfb-run -a -s "-screen 0 1920x1080x24")
endif()
montezuma_run(TIMEOUT 900 LOG "${work}/import.log" RESULT ignored
  COMMAND "${GODOT}" --headless --path "${root}/game" --import)
list(LENGTH scenes count)
math(EXPR timeout "900 * ${count}")
list(JOIN scenes "," arg)
# The driver prints one result line per scene; trust those, not the exit
# status alone (the editor's own exit can be noisy).
montezuma_run(TIMEOUT ${timeout} LOG "${log}" RESULT ignored
  COMMAND ${prefix} "${GODOT}" --editor --path "${root}/game" --rendering-driver vulkan -- "--bake-lightmaps=${arg}")
restore_project()
set(MONTEZUMA_ON_FAIL "")

montezuma_log_has(_ "${log}" REGEX "LIGHTMAP_BAKE")
set(failed FALSE)
foreach(s IN LISTS scenes)
  file(STRINGS "${log}" ok REGEX "LIGHTMAP_BAKE ok ${s} ")
  if(NOT ok)
    message(NOTICE "bake failed: ${s}")
    set(failed TRUE)
  endif()
endforeach()
if(failed)
  # The engine's own errors say why (the driver only relays the editor's
  # dialog text). The editor colours its output; strip that first.
  file(STRINGS "${log}" lines)
  string(ASCII 27 esc)
  set(shown "")
  foreach(line IN LISTS lines)
    string(REGEX REPLACE "${esc}\\[[0-9;]*m" "" line "${line}")
    if(line MATCHES "^ *(ERROR|WARNING|SCRIPT ERROR):")
      list(APPEND shown "${line}")
    endif()
  endforeach()
  list(LENGTH shown n)
  if(n GREATER 60)
    math(EXPR first "${n} - 60")
    list(SUBLIST shown ${first} 60 shown)
  endif()
  list(JOIN shown "\n" shown)
  message(NOTICE "${shown}")
  montezuma_fail("lightmap bake failed (full log: ${log})")
endif()
foreach(stamp hash IN ZIP_LISTS stamps hashes)
  file(WRITE "${stamp}" "${hash}\n")
endforeach()

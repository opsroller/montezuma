# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Exports the web demo (Godot preset "Web": single-threaded, GDExtension on)
# and packages it as <OUT>/montezuma-<version>-web.zip plus a .sha256
# file. The archive's folder holds montezuma.js, montezuma.wasm,
# montezuma.pck, montezuma.side.wasm, the extension's .wasm, the export's own
# montezuma.html (for previewing; the site uses its own page), LICENSE.txt
# and the notices. Fails when the files gzip to more than the web budget
# (40 MB, docs/reference/visual-target.md).
#
# Usage: cmake -DOUT=<outdir> [-DVERSION=<version>] [-DGODOT=<editor>] -P tools/release/web-export.cmake
#   VERSION  defaults to the checked-out commit's (tools/release/version.cmake)
#   GODOT    path to the editor (default: godot on PATH)
# Needs the web_dlink_nothreads_release.zip template (fetch-godot.cmake) and
# game/bin/libmontezuma.web.template_release.wasm32.nothreads.wasm.
include("${CMAKE_CURRENT_LIST_DIR}/steps.cmake")
montezuma_require(OUT)
montezuma_abspath(OUT)
if(NOT GODOT)
  set(GODOT godot)
endif()
if(NOT VERSION)
  montezuma_version(VERSION)
endif()
montezuma_python(python)
set(budget 40000000)
set(game "${MONTEZUMA_ROOT}/game")
set(here "${MONTEZUMA_ROOT}/tools/release")

set(lib_name libmontezuma.web.template_release.wasm32.nothreads.wasm)
if(NOT EXISTS "${game}/bin/${lib_name}")
  montezuma_fail("missing game/bin/${lib_name}: build the extension with Emscripten first")
endif()

set(name "montezuma-${VERSION}-web")
set(stage "${OUT}/stage/${name}")
file(REMOVE_RECURSE "${stage}")
file(MAKE_DIRECTORY "${stage}" "${OUT}/logs")
montezuma_source_date_epoch()

# The game reports its version in montezuma:ready; stamp it for this export
# only and put project.godot back afterwards, also when a later step fails.
set(project "${game}/project.godot")
set(saved "${OUT}/project.godot.orig")
file(COPY_FILE "${project}" "${saved}")
function(restore_project)
  file(COPY_FILE "${saved}" "${project}")
  file(REMOVE "${saved}")
endfunction()
set(MONTEZUMA_ON_FAIL restore_project)
file(READ "${saved}" text)
string(REGEX REPLACE "(\nconfig/name=[^\n]*)" "\\1\nconfig/version=\"${VERSION}\"" text "${text}")
file(WRITE "${project}" "${text}")

# Credits and license files the game reads from res:// (generated, not
# committed; the preset includes them by name).
montezuma_run(TIMEOUT 300 WORKING_DIRECTORY "${MONTEZUMA_ROOT}" COMMAND "${python}" "${here}/game-files.py" stage game)

# The import pass prints errors for libraries of other targets; the export
# below is what must succeed, so its log is the one that gates. A hung
# import still fails.
montezuma_run(TIMEOUT 900 LOG "${OUT}/logs/${name}.import.log" RESULT ignored
  COMMAND "${GODOT}" --headless --path "${game}" --import)
set(log "${OUT}/logs/${name}.export.log")
montezuma_run(TIMEOUT 900 LOG "${log}" RESULT rc
  COMMAND "${GODOT}" --headless --path "${game}" --export-release Web "${stage}/montezuma.html")
if(NOT rc EQUAL 0)
  montezuma_tail("${log}" 40)
  montezuma_fail("Godot export of preset \"Web\" failed (full log: ${log})")
endif()
restore_project()
set(MONTEZUMA_ON_FAIL "")

foreach(f montezuma.js montezuma.wasm montezuma.pck montezuma.side.wasm "${lib_name}")
  if(NOT EXISTS "${stage}/${f}")
    file(GLOB present RELATIVE "${stage}" "${stage}/*")
    montezuma_fail("export is missing ${f} (has: ${present})")
  endif()
endforeach()

montezuma_run(TIMEOUT 300 WORKING_DIRECTORY "${MONTEZUMA_ROOT}" COMMAND "${python}" "${here}/game-files.py" check game "${stage}/montezuma.pck")

file(COPY_FILE "${MONTEZUMA_ROOT}/LICENSE" "${stage}/LICENSE.txt")
montezuma_run(TIMEOUT 300 WORKING_DIRECTORY "${MONTEZUMA_ROOT}" COMMAND "${python}" "${here}/notices.py" "${stage}")

# What a visitor downloads: every file, compressed as a server would.
set(size 0)
set(gz "${OUT}/size-check.gz")
file(GLOB files "${stage}/*")
foreach(f IN LISTS files)
  file(ARCHIVE_CREATE OUTPUT "${gz}" PATHS "${f}" FORMAT raw COMPRESSION GZip COMPRESSION_LEVEL 9)
  file(SIZE "${gz}" n)
  math(EXPR size "${size} + ${n}")
endforeach()
file(REMOVE "${gz}")
montezuma_print("Web demo: ${size} bytes gzipped (budget ${budget})")
if(size GREATER budget)
  montezuma_fail("the web demo is over its download budget")
endif()

set(archive "${OUT}/${name}.zip")
montezuma_run(TIMEOUT 600 WORKING_DIRECTORY "${MONTEZUMA_ROOT}" COMMAND "${python}" "${here}/package.py" zip "${OUT}/stage" "${name}" "${archive}")
file(REMOVE_RECURSE "${OUT}/stage")
montezuma_write_sha256("${archive}")

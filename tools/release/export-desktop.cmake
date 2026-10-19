# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Exports one desktop target with Godot and packages it as
# <OUT>/montezuma-<version>-<platform>-<arch>.<ext> plus a .sha256 file.
#
# Usage: cmake -DPLATFORM=<linux|windows|macos> -DOUT=<outdir>
#              [-DVERSION=<version>] [-DGODOT=<editor>] -P tools/release/export-desktop.cmake
#   VERSION  defaults to the checked-out commit's (tools/release/version.cmake)
#   GODOT    path to the editor (default: godot on PATH)
# Needs the matching export template (fetch-godot.cmake) and the extension's
# release library for the platform in game/bin/.
include("${CMAKE_CURRENT_LIST_DIR}/steps.cmake")
montezuma_require(PLATFORM OUT)
montezuma_abspath(OUT)
if(NOT GODOT)
  set(GODOT godot)
endif()
if(NOT VERSION)
  montezuma_version(VERSION)
endif()
montezuma_python(python)

if(PLATFORM STREQUAL "linux")
  set(preset Linux)
  set(arch x86_64)
  set(binary montezuma.x86_64)
  set(format tar.gz)
elseif(PLATFORM STREQUAL "windows")
  set(preset Windows)
  set(arch x86_64)
  set(binary montezuma.exe)
  set(format zip)
elseif(PLATFORM STREQUAL "macos")
  set(preset macOS)
  set(arch universal)
  set(binary "Montezuma's.app")
  set(format zip)
else()
  montezuma_fail("unknown platform: ${PLATFORM}")
endif()

set(name "montezuma-${VERSION}-${PLATFORM}-${arch}")
set(stage "${OUT}/stage/${name}")
file(REMOVE_RECURSE "${stage}")
file(MAKE_DIRECTORY "${stage}" "${OUT}/logs")
montezuma_source_date_epoch()
set(game "${MONTEZUMA_ROOT}/game")
set(here "${MONTEZUMA_ROOT}/tools/release")

# Credits and license files the game reads from res:// (generated, not
# committed; the presets include them by name).
montezuma_run(TIMEOUT 300 WORKING_DIRECTORY "${MONTEZUMA_ROOT}" COMMAND "${python}" "${here}/game-files.py" stage game)

# The import pass prints errors for libraries of other targets; the export
# below is what must succeed, so its log is the one that gates. A hung
# import still fails.
montezuma_run(TIMEOUT 900 LOG "${OUT}/logs/${name}.import.log" RESULT ignored
  COMMAND "${GODOT}" --headless --path "${game}" --import)
set(log "${OUT}/logs/${name}.export.log")
montezuma_run(TIMEOUT 900 LOG "${log}" RESULT rc
  COMMAND "${GODOT}" --headless --path "${game}" --export-release "${preset}" "${stage}/${binary}")
if(NOT rc EQUAL 0)
  montezuma_tail("${log}" 40)
  montezuma_fail("Godot export of preset \"${preset}\" failed (full log: ${log})")
endif()
# Godot only warns when ad-hoc signing fails; an unsigned macOS app will not
# start on Apple silicon, so treat it as a failure.
file(STRINGS "${log}" signing REGEX "CodeSign|Code Signing")
foreach(line IN LISTS signing)
  string(TOLOWER "${line}" lower)
  if(lower MATCHES "error|fail")
    list(JOIN signing "\n" shown)
    montezuma_fail("${shown}")
  endif()
endforeach()

montezuma_run(TIMEOUT 300 WORKING_DIRECTORY "${MONTEZUMA_ROOT}" COMMAND "${python}" "${here}/game-files.py" check game "${stage}/${binary}")

file(COPY_FILE "${MONTEZUMA_ROOT}/LICENSE" "${stage}/LICENSE.txt")
montezuma_run(TIMEOUT 300 WORKING_DIRECTORY "${MONTEZUMA_ROOT}" COMMAND "${python}" "${here}/notices.py" "${stage}")

set(archive "${OUT}/${name}.${format}")
montezuma_run(TIMEOUT 600 WORKING_DIRECTORY "${MONTEZUMA_ROOT}" COMMAND "${python}" "${here}/package.py" "${format}" "${OUT}/stage" "${name}" "${archive}")
file(REMOVE_RECURSE "${OUT}/stage")
montezuma_write_sha256("${archive}")

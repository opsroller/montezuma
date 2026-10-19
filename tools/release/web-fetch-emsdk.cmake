# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Installs and activates the pinned Emscripten SDK (EMSDK_VERSION in
# godot.env) in DIR. emsdk is cloned at the tag of the same version, and it
# checks the SDK it downloads against the release hashes that tag lists. An
# existing install of the right version in DIR is reused, so DIR can be
# cached between runs.
#
# Usage: cmake -DDIR=<dir> -P tools/release/web-fetch-emsdk.cmake
# Then configure with the SDK's toolchain file and its config, no shell
# environment needed:
#   EM_CONFIG=<dir>/.emscripten cmake -S . -B build -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=<dir>/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake ...
include("${CMAKE_CURRENT_LIST_DIR}/steps.cmake")
montezuma_load_env()
montezuma_require(DIR)
montezuma_abspath(DIR)
montezuma_python(python)

set(tag "")
if(EXISTS "${DIR}/emsdk.py")
  execute_process(COMMAND git -C "${DIR}" describe --tags --exact-match
    TIMEOUT 60 RESULT_VARIABLE rc OUTPUT_VARIABLE tag ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT rc MATCHES "^[0-9]+$")
    montezuma_fail("git describe in ${DIR}: ${rc}")
  endif()
endif()
if(NOT tag STREQUAL EMSDK_VERSION)
  file(REMOVE_RECURSE "${DIR}")
  montezuma_run(TIMEOUT 300 COMMAND git clone -q --depth 1 --branch "${EMSDK_VERSION}"
    https://github.com/emscripten-core/emsdk.git "${DIR}")
endif()
montezuma_run(TIMEOUT 1200 WORKING_DIRECTORY "${DIR}" COMMAND "${python}" emsdk.py install "${EMSDK_VERSION}")
montezuma_run(TIMEOUT 300 WORKING_DIRECTORY "${DIR}" COMMAND "${python}" emsdk.py activate "${EMSDK_VERSION}")
foreach(f .emscripten upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake)
  if(NOT EXISTS "${DIR}/${f}")
    montezuma_fail("emsdk activate left no ${DIR}/${f}")
  endif()
endforeach()

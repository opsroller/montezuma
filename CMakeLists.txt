# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.22)
project(montezuma LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MONTEZUMA_BUILD_TESTS "Build core rules tests" ON)
# The extension needs the godot-cpp submodule; default to building it only
# when the submodule is checked out, so core-only checkouts just work.
if(EXISTS "${PROJECT_SOURCE_DIR}/thirdparty/godot-cpp/CMakeLists.txt")
  set(_montezuma_has_godot_cpp ON)
else()
  set(_montezuma_has_godot_cpp OFF)
endif()
option(MONTEZUMA_BUILD_EXTENSION "Build the Godot GDExtension (needs thirdparty/godot-cpp)" ${_montezuma_has_godot_cpp})
option(MONTEZUMA_WERROR "Treat compiler warnings as errors" OFF)
option(MONTEZUMA_BUILD_SIM "Build the balance simulator (tools/sim)" ON)

# godot-cpp picks the MSVC C runtime (static by default) by caching
# CMAKE_MSVC_RUNTIME_LIBRARY when extension/ adds it, which is after
# montezuma_core exists, and a DLL whose objects disagree fails to link
# (LNK2038). Cache the same choice here, before any target, so every target
# uses it; godot-cpp keeps a cached value.
if(MONTEZUMA_BUILD_EXTENSION AND MSVC)
  option(GODOTCPP_USE_STATIC_CPP "Link MinGW/MSVC C++ runtime libraries statically" ON)
  option(GODOTCPP_DEBUG_CRT "Compile with MSVC's debug CRT (/MDd)" OFF)
  set(CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<IF:$<BOOL:${GODOTCPP_DEBUG_CRT}>,DebugDLL,$<$<NOT:$<BOOL:${GODOTCPP_USE_STATIC_CPP}>>:DLL>>"
    CACHE STRING "Select the MSVC runtime library for use by compilers targeting the MSVC ABI.")
endif()

# Engine-independent game rules. The Godot GDExtension links against this.
# Sources are picked up automatically so parallel work packages can add
# files without editing this list.
file(GLOB MONTEZUMA_CORE_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/core/src/*.cpp")
add_library(montezuma_core STATIC ${MONTEZUMA_CORE_SOURCES})
target_include_directories(montezuma_core PUBLIC core/include)
# Vendored single-header JSON library (MIT), used only inside core sources.
target_include_directories(montezuma_core SYSTEM PRIVATE thirdparty)

# Warnings for our own targets (not third-party code).
function(montezuma_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- $<$<BOOL:${MONTEZUMA_WERROR}>:/WX>)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic $<$<BOOL:${MONTEZUMA_WERROR}>:-Werror>)
  endif()
endfunction()
montezuma_warnings(montezuma_core)

if(MONTEZUMA_BUILD_TESTS)
  enable_testing()
  # One executable and one CTest test per file in core/tests/.
  file(GLOB MONTEZUMA_CORE_TESTS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/core/tests/*.cpp")
  foreach(test_source ${MONTEZUMA_CORE_TESTS})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(montezuma_${test_name} ${test_source})
    target_link_libraries(montezuma_${test_name} PRIVATE montezuma_core)
    montezuma_warnings(montezuma_${test_name})
    # Tests that read committed data (such as game/data/levels) find it here.
    target_compile_definitions(montezuma_${test_name} PRIVATE MONTEZUMA_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME ${test_name} COMMAND montezuma_${test_name})
  endforeach()
endif()

# Balance simulator CLI (WP-22); a desktop tool, never shipped.
if(MONTEZUMA_BUILD_SIM)
  add_subdirectory(tools/sim)
endif()

if(MONTEZUMA_BUILD_EXTENSION)
  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/thirdparty/godot-cpp/CMakeLists.txt")
    message(FATAL_ERROR "godot-cpp is missing. Run: git submodule update --init --recursive")
  endif()
  add_subdirectory(extension)
  # The credits screen reads res://CREDITS.md and the license as
  # res://LICENSE.txt; copy them in so editor and headless runs show them.
  # Exports also add the notices (tools/release/game-files.py).
  configure_file(CREDITS.md "${PROJECT_SOURCE_DIR}/game/CREDITS.md" COPYONLY)
  configure_file(LICENSE "${PROJECT_SOURCE_DIR}/game/LICENSE.txt" COPYONLY)
endif()

# Board theme textures and fire flipbooks are build outputs, never committed:
# generate them into game/assets/textures when Python 3 with numpy and Pillow
# is on the machine. The generators skip work whose inputs hash has not
# changed (the fire simulation takes a few minutes the first time).
# Docs: docs/reference/board-themes.md
find_package(Python3 3.10 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import numpy, PIL"
    RESULT_VARIABLE _montezuma_texture_deps OUTPUT_QUIET ERROR_QUIET)
endif()
if(Python3_Interpreter_FOUND AND _montezuma_texture_deps EQUAL 0)
  add_custom_target(textures ALL
    COMMAND "${Python3_EXECUTABLE}" tools/assets/gen_theme_textures.py
    COMMAND "${Python3_EXECUTABLE}" tools/assets/fx/gen_fire.py
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Generating board theme textures and fire flipbooks")
else()
  message(STATUS "Board theme textures: numpy and Pillow not found, so they are not generated. "
    "Run: pip install -r .github/requirements/assets.txt && python3 tools/assets/gen_theme_textures.py "
    "&& python3 tools/assets/fx/gen_fire.py")
endif()

# Godot-free parts of the board view (layout, camera fit) and the quality
# tier table: no godot-cpp needed, so they run with the core tests on every
# platform and under sanitizers.
if(MONTEZUMA_BUILD_TESTS)
  add_subdirectory(extension/src/board/tests)
  add_subdirectory(extension/src/render/tests)
  add_subdirectory(extension/src/bench/tests)
  add_subdirectory(extension/src/demo/tests)
endif()

# The website's tests (site/tests), when Python 3.10+ and Node are on the
# machine. Docs: docs/how-to/build-and-preview-the-website.md
if(MONTEZUMA_BUILD_TESTS)
  find_package(Python3 3.10 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME site_build_tests
      COMMAND "${Python3_EXECUTABLE}" -m unittest discover site/tests
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    # Read and write text as UTF-8 on Windows too.
    set_tests_properties(site_build_tests PROPERTIES ENVIRONMENT "PYTHONUTF8=1")
    add_test(NAME release_tools_tests
      COMMAND "${Python3_EXECUTABLE}" -m unittest discover tools/release/tests
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    set_tests_properties(release_tools_tests PROPERTIES ENVIRONMENT "PYTHONUTF8=1")
  endif()
  find_program(MONTEZUMA_NODE node)
  if(MONTEZUMA_NODE)
    file(GLOB MONTEZUMA_SITE_JS_TESTS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/site/tests/*.test.mjs")
    add_test(NAME site_js_tests COMMAND "${MONTEZUMA_NODE}" --test ${MONTEZUMA_SITE_JS_TESTS})
  endif()
endif()

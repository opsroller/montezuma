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
    add_test(NAME ${test_name} COMMAND montezuma_${test_name})
  endforeach()
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

# Godot-free parts of the board view (layout, camera fit): no godot-cpp needed,
# so they run with the core tests on every platform and under sanitizers.
if(MONTEZUMA_BUILD_TESTS)
  add_subdirectory(extension/src/board/tests)
endif()

# The asset validator (Python + Pillow) as a CTest test. Default: on when
# Pillow is installed. CI turns it on, so a missing Pillow fails there.
if(MONTEZUMA_BUILD_TESTS)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  set(_montezuma_has_pillow OFF)
  if(Python3_Interpreter_FOUND)
    execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import PIL"
      RESULT_VARIABLE _montezuma_pillow_rc OUTPUT_QUIET ERROR_QUIET)
    if(_montezuma_pillow_rc EQUAL 0)
      set(_montezuma_has_pillow ON)
    endif()
  endif()
  option(MONTEZUMA_TEST_ASSETS "Run tools/assets/validate_assets.py as a test (needs Python 3 and Pillow)" ${_montezuma_has_pillow})
  if(MONTEZUMA_TEST_ASSETS)
    if(NOT _montezuma_has_pillow)
      message(FATAL_ERROR "MONTEZUMA_TEST_ASSETS needs Python 3 with Pillow: python3 -m pip install Pillow")
    endif()
    add_test(NAME asset_tests COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/assets/validate_assets.py")
    # The validator reads text files with the default encoding; make that
    # UTF-8 on Windows too.
    set_tests_properties(asset_tests PROPERTIES ENVIRONMENT "PYTHONUTF8=1")
  endif()
endif()

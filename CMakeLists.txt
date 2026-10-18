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
endif()

# Godot-free parts of the board view (layout, camera fit): no godot-cpp needed,
# so they run with the core tests on every platform and under sanitizers.
if(MONTEZUMA_BUILD_TESTS)
  add_subdirectory(extension/src/board/tests)
endif()

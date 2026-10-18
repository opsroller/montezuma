# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.22)
project(montezuma LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MONTEZUMA_BUILD_TESTS "Build core rules tests" ON)
option(MONTEZUMA_BUILD_EXTENSION "Build the Godot GDExtension (needs thirdparty/godot-cpp)" ON)
option(MONTEZUMA_WERROR "Treat compiler warnings as errors" OFF)

# Engine-independent game rules. The Godot GDExtension links against this.
add_library(montezuma_core STATIC
  core/src/board.cpp
  core/src/level_state.cpp
)
target_include_directories(montezuma_core PUBLIC core/include)

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
  add_executable(montezuma_core_tests core/tests/core_tests.cpp)
  target_link_libraries(montezuma_core_tests PRIVATE montezuma_core)
  montezuma_warnings(montezuma_core_tests)
  add_test(NAME core_tests COMMAND montezuma_core_tests)
endif()

if(MONTEZUMA_BUILD_EXTENSION)
  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/thirdparty/godot-cpp/CMakeLists.txt")
    message(FATAL_ERROR "godot-cpp is missing. Run: git submodule update --init --recursive")
  endif()
  add_subdirectory(extension)
endif()

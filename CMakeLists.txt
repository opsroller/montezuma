# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.22)
project(montezuma LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MONTEZUMA_BUILD_TESTS "Build core rules tests" ON)

# Engine-independent game rules. The Godot GDExtension links against this.
add_library(montezuma_core STATIC
  core/src/board.cpp
  core/src/level_state.cpp
)
target_include_directories(montezuma_core PUBLIC core/include)
if(MSVC)
  target_compile_options(montezuma_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(montezuma_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(MONTEZUMA_BUILD_TESTS)
  enable_testing()
  add_executable(montezuma_core_tests core/tests/core_tests.cpp)
  target_link_libraries(montezuma_core_tests PRIVATE montezuma_core)
  add_test(NAME core_tests COMMAND montezuma_core_tests)
endif()

# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
# Tests for the Godot-free quality tier table and fire flicker noise; built with the core tests.
add_executable(montezuma_quality_tests quality_tests.cpp ../quality.cpp)
target_include_directories(montezuma_quality_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
montezuma_warnings(montezuma_quality_tests)
add_test(NAME quality_tests COMMAND montezuma_quality_tests)

add_executable(montezuma_flicker_tests flicker_tests.cpp ../flicker.cpp)
target_include_directories(montezuma_flicker_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
montezuma_warnings(montezuma_flicker_tests)
add_test(NAME flicker_tests COMMAND montezuma_flicker_tests)

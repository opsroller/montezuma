# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
# Tests for the Godot-free part of the web demo hand-off; built with the core tests.
add_executable(montezuma_demo_tests demo_tests.cpp)
target_include_directories(montezuma_demo_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
montezuma_warnings(montezuma_demo_tests)
add_test(NAME demo_tests COMMAND montezuma_demo_tests)

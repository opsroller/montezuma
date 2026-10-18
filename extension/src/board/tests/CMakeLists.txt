# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
# Tests for the Godot-free parts of the board view; built with the core tests.
add_executable(montezuma_board_tests board_tests.cpp)
target_include_directories(montezuma_board_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(montezuma_board_tests PRIVATE montezuma_core)
montezuma_warnings(montezuma_board_tests)
add_test(NAME board_tests COMMAND montezuma_board_tests)

# Totem and power-up effects (WP-29): plan timing and markers through real sessions.
add_executable(montezuma_effects_tests effects_tests.cpp)
target_include_directories(montezuma_effects_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(montezuma_effects_tests PRIVATE montezuma_core)
montezuma_warnings(montezuma_effects_tests)
add_test(NAME effects_tests COMMAND montezuma_effects_tests)

# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
# Tests for the Godot-free parts of the benchmark; built with the core tests.
add_executable(montezuma_bench_tests bench_tests.cpp ../replay.cpp)
target_include_directories(montezuma_bench_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(montezuma_bench_tests PRIVATE montezuma_core)
montezuma_warnings(montezuma_bench_tests)
add_test(NAME bench_tests COMMAND montezuma_bench_tests)

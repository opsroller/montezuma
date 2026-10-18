# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors

# Balance simulator (WP-22). Never ships; see
# docs/how-to/run-the-balance-simulator.md.
add_library(montezuma_sim_lib STATIC sim.cpp)
target_include_directories(montezuma_sim_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(montezuma_sim_lib PUBLIC montezuma_core)
montezuma_warnings(montezuma_sim_lib)

add_executable(montezuma_sim main.cpp)
target_link_libraries(montezuma_sim PRIVATE montezuma_sim_lib)
montezuma_warnings(montezuma_sim)

if(MONTEZUMA_BUILD_TESTS)
  add_executable(montezuma_test_sim tests/test_sim.cpp)
  target_link_libraries(montezuma_test_sim PRIVATE montezuma_sim_lib)
  montezuma_warnings(montezuma_test_sim)
  add_test(NAME test_sim COMMAND montezuma_test_sim)
  # Runs the CLI on the example level twice and checks the reports match,
  # plus the exit codes for bad input.
  add_test(NAME sim_smoke
           COMMAND ${CMAKE_COMMAND}
                   -DSIM=$<TARGET_FILE:montezuma_sim>
                   -DEXAMPLES=${CMAKE_CURRENT_SOURCE_DIR}/examples
                   -DWORK=${CMAKE_CURRENT_BINARY_DIR}/sim_smoke
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.cmake)
endif()

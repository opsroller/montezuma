# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
# In-game UI (WP-18, WP-19). Included from extension/CMakeLists.txt.

# Engine-independent view logic, testable without Godot.
add_library(montezuma_ui_model STATIC
  model/format.cpp
  model/nav_stack.cpp
  model/settings.cpp
)
target_include_directories(montezuma_ui_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
montezuma_warnings(montezuma_ui_model)

file(GLOB MONTEZUMA_UI_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/screens/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/hud/*.cpp"
)
target_sources(montezuma_extension PRIVATE ${MONTEZUMA_UI_SOURCES})
target_link_libraries(montezuma_extension PRIVATE montezuma_ui_model)

if(MONTEZUMA_BUILD_TESTS)
  add_executable(montezuma_ui_model_tests tests/ui_model_tests.cpp)
  target_link_libraries(montezuma_ui_model_tests PRIVATE montezuma_ui_model)
  montezuma_warnings(montezuma_ui_model_tests)
  add_test(NAME ui_model_tests COMMAND montezuma_ui_model_tests)
endif()

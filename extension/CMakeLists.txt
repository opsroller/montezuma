# SPDX-License-Identifier: GPL-2.0-or-later
# Godot GDExtension wrapping the engine-independent core.

add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/godot-cpp ${CMAKE_BINARY_DIR}/godot-cpp)

add_library(montezuma_extension SHARED
  src/register_types.cpp
  src/match3_board.cpp
  src/board_view_3d.cpp
)
target_link_libraries(montezuma_extension PRIVATE montezuma_core godot-cpp)

get_target_property(GODOTCPP_SUFFIX godot-cpp GODOTCPP_SUFFIX)
set(MONTEZUMA_BIN_DIR "${PROJECT_SOURCE_DIR}/game/bin")

set_target_properties(montezuma_extension PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  BUILD_RPATH_USE_ORIGIN ON
  PREFIX "lib"
  OUTPUT_NAME "montezuma${GODOTCPP_SUFFIX}"
  LIBRARY_OUTPUT_DIRECTORY "$<1:${MONTEZUMA_BIN_DIR}>"
  RUNTIME_OUTPUT_DIRECTORY "$<1:${MONTEZUMA_BIN_DIR}>"
  PDB_OUTPUT_DIRECTORY "$<1:${MONTEZUMA_BIN_DIR}>"
)

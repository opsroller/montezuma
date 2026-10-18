# SPDX-License-Identifier: GPL-2.0-or-later
# Godot GDExtension wrapping the engine-independent core.

# One exception setting for every target: the core reports errors with
# exceptions, and mixing settings is an ODR hazard on MSVC.
set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)
# Keep godot-cpp's own warnings out of our -Werror builds.
set(GODOTCPP_SYSTEM_HEADERS ON CACHE BOOL "" FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/godot-cpp ${CMAKE_BINARY_DIR}/godot-cpp)

add_library(montezuma_extension SHARED
  src/register_types.cpp
  src/match3_board.cpp
  src/match3_level.cpp
  src/board/board_view_3d.cpp
  src/board/board_camera_3d.cpp
  src/board/board_playtest.cpp
  src/board/piece_library.cpp
  src/board/register_board.cpp
  src/audio/game_audio.cpp
  src/audio/register_audio.cpp
  src/game/match3_game.cpp
  src/game/register_game.cpp
)
target_include_directories(montezuma_extension PRIVATE src)
target_link_libraries(montezuma_extension PRIVATE montezuma_core godot-cpp)
montezuma_warnings(montezuma_extension)

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

# In-game UI (owner: ui-developer).
add_subdirectory(src/ui)

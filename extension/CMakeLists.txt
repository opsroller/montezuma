# SPDX-License-Identifier: GPL-2.0-or-later
# Godot GDExtension wrapping the engine-independent core.

# One exception setting for every target: the core reports errors with
# exceptions, and mixing settings is an ODR hazard on MSVC.
set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)
# godot-cpp 10 is versioned apart from Godot; pick the engine API we target.
# Must match the Godot pinned in tools/release/godot.env.
set(GODOTCPP_API_VERSION "4.7" CACHE STRING "" FORCE)
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
  src/board/effects/board_effects.cpp
  src/board/register_board.cpp
  src/audio/game_audio.cpp
  src/audio/register_audio.cpp
  src/demo/intro_level.cpp
  src/demo/register_demo.cpp
  src/game/match3_game.cpp
  src/game/register_game.cpp
  src/render/flicker.cpp
  src/render/flicker_light_3d.cpp
  src/render/quality.cpp
  src/render/quality_tiers.cpp
  src/render/register_render.cpp
  src/bench/benchmark_runner.cpp
  src/bench/readability_capture.cpp
  src/bench/register_bench.cpp
  src/bench/render_probe.cpp
  src/bench/replay.cpp
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
# On the web the extension is a WebAssembly side module that Godot links
# against its main module at start. godot-cpp only compiles with
# -sSIDE_MODULE, so link with it too; Godot looks for a .wasm file, and
# CMake's default shared-library suffix under Emscripten is .so.
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  target_link_options(montezuma_extension PRIVATE -sSIDE_MODULE=1)
  set_target_properties(montezuma_extension PROPERTIES SUFFIX ".wasm")
endif()

# In-game UI (owner: ui-developer).
add_subdirectory(src/ui)

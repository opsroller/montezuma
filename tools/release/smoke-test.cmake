# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Checks a packaged desktop build: verifies its checksum, unpacks it, runs
# the game headless for 120 frames and fails on any engine error (for
# example the GDExtension library missing or not loading). Given SCREENS,
# it then renders the title screen and the board with Godot's Movie Maker
# at the project's window size (1280x720) and saves them there as
# title.png and board.png. A capture whose log has an engine error,
# including one printed at exit such as resources still in use, fails too.
#
# Usage: cmake -DARCHIVE=<archive or folder> [-DSCREENS=<dir>] -P tools/release/smoke-test.cmake
#   ARCHIVE  the package (its .sha256 file sits beside it), or a folder that
#            holds exactly one .tar.gz or .zip package
# Runs on the archive's own platform: Linux, Windows or macOS. Screenshots
# need a display: on Linux run it under xvfb-run.
include("${CMAKE_CURRENT_LIST_DIR}/steps.cmake")
montezuma_require(ARCHIVE)
montezuma_abspath(ARCHIVE)
if(IS_DIRECTORY "${ARCHIVE}")
  file(GLOB found "${ARCHIVE}/*.tar.gz" "${ARCHIVE}/*.zip")
  list(LENGTH found n)
  if(NOT n EQUAL 1)
    montezuma_fail("expected one package in ${ARCHIVE}, found: ${found}")
  endif()
  set(ARCHIVE "${found}")
endif()
if(SCREENS)
  montezuma_abspath(SCREENS)
  file(MAKE_DIRECTORY "${SCREENS}")
endif()

get_filename_component(file_name "${ARCHIVE}" NAME)
file(STRINGS "${ARCHIVE}.sha256" sum_line LIMIT_COUNT 1)
string(REGEX MATCH "^([0-9a-f]+) +\\*?(.+)$" _ "${sum_line}")
if(NOT CMAKE_MATCH_2 STREQUAL file_name)
  montezuma_fail("${ARCHIVE}.sha256 is for \"${CMAKE_MATCH_2}\", not ${file_name}")
endif()
file(SHA256 "${ARCHIVE}" have)
if(NOT have STREQUAL CMAKE_MATCH_1)
  montezuma_fail("${file_name}: checksum mismatch (expected ${CMAKE_MATCH_1}, got ${have})")
endif()
message(STATUS "${file_name}: checksum OK")

set(work "${ARCHIVE}.smoke")
file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${work}")
file(ARCHIVE_EXTRACT INPUT "${ARCHIVE}" DESTINATION "${work}")

string(REGEX REPLACE "\\.(tar\\.gz|zip)$" "" name "${file_name}")
if(name MATCHES "-linux-")
  set(game "${work}/${name}/montezuma.x86_64")
  # Renderers that work on GPU-less CI runners: Mesa under Xvfb.
  set(drivers opengl3)
elseif(name MATCHES "-windows-")
  set(game "${work}/${name}/montezuma.exe")
  # ANGLE over Direct3D 11 (WARP).
  set(drivers opengl3_angle)
elseif(name MATCHES "-macos-")
  set(game "${work}/${name}/Montezuma's.app/Contents/MacOS/Montezuma's")
  # On macOS virtual machines Metal works on Godot 4.7.2 (the paravirtual
  # GPU; 4.4 crashed there for lack of argument encoders) and Vulkan runs
  # out of device memory. A Metal failure still falls back to OpenGL, which
  # there is Apple's software renderer (several seconds a frame): keep the
  # frame counts low.
  set(drivers metal opengl3)
  # Godot 4.8's Metal driver defaults to barrier sync, which needs placement
  # heaps; the paravirtual GPU of GitHub's macOS runners cannot make them
  # ("Unable to create placement heap", then a crash). Use the older
  # hazard-tracking mode there; real Macs keep the default.
  if(DEFINED ENV{GITHUB_ACTIONS} AND NOT DEFINED ENV{GODOT_MTL_SYNC_MODE})
    set(ENV{GODOT_MTL_SYNC_MODE} none)
  endif()
else()
  montezuma_fail("unknown platform in ${name}")
endif()
if(NOT EXISTS "${game}")
  montezuma_fail("game binary missing: ${game}")
endif()

# show_output(<console> <log>): the console output, or the log file where
# the console stays empty (a Windows GUI build).
function(show_output console logfile)
  string(STRIP "${console}" trimmed)
  if(trimmed STREQUAL "" AND EXISTS "${logfile}")
    file(READ "${logfile}" console)
  endif()
  message(NOTICE "${console}")
endfunction()

set(log "${work}/smoke.log")
file(TO_NATIVE_PATH "${log}" log_arg)
montezuma_run(TIMEOUT 300 RESULT rc COMMAND "${game}" --headless --xr-mode off --quit-after 120 --log-file "${log_arg}")
if(EXISTS "${log}")
  file(READ "${log}" text)
  message(NOTICE "${text}")
endif()
if(NOT rc EQUAL 0)
  montezuma_fail("the game exited with status ${rc}")
endif()
file(STRINGS "${log}" started REGEX "Godot Engine")
if(NOT started)
  montezuma_fail("the game did not start")
endif()
# Lines Godot prints that the project's own settings cause and the shipped
# game shows too (docs/reference/engine-issues.md; work package 64):
# - the separate render thread (rendering/driver/threads/thread_model=2,
#   experimental in Godot 4.8) logs this once at exit on macOS;
# - with HDR output requested (display/window/hdr/request_hdr_output),
#   HDR 2D is on, and --write-movie's frame capture on the OpenGL driver
#   logs this for every frame it converts (Image::linear_to_srgb).
set(known_engine_lines
  "This function \\(finalize\\) can only be called from the render thread"
  "format != FORMAT_RGB8 && format != FORMAT_RGBA8 && format != FORMAT_RGBH && format != FORMAT_RGBAH")
montezuma_log_has(errors "${log}" REGEX "ERROR|SCRIPT ERROR|Can't open dynamic library" IGNORE ${known_engine_lines})
if(errors)
  montezuma_fail("engine errors during the smoke run")
endif()
montezuma_print("smoke test passed: ${name}")
if(NOT SCREENS)
  return()
endif()

# capture(<shot> <frames> [<game user arg>...]): keeps the last frame.
function(capture shot frames)
  set(dir "${work}/movie-${shot}")
  set(slog "${work}/${shot}.log")
  file(TO_NATIVE_PATH "${dir}/frame.png" movie_arg)
  file(TO_NATIVE_PATH "${slog}" slog_arg)
  math(EXPR quit_after "${frames} + 300")
  foreach(driver IN LISTS drivers)
    file(REMOVE_RECURSE "${dir}")
    file(MAKE_DIRECTORY "${dir}")
    file(REMOVE "${slog}")
    # The game quits itself after <frames> (--quit-after-frames), stopping
    # its sounds first; Godot's --quit-after, set later, catches a game that
    # keeps running, and the timeout a process that hangs.
    execute_process(COMMAND "${game}" --xr-mode off --rendering-driver ${driver} --fixed-fps 30 --quit-after ${quit_after}
        --write-movie "${movie_arg}" --log-file "${slog_arg}" -- "--quit-after-frames=${frames}" ${ARGN}
      # 1200 s: the board capture measured 8:53 on GitHub's llvmpipe runner
      # (2026-10-24, Compatibility content), so twice that is the limit.
      TIMEOUT 1200 RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
    if(NOT rc MATCHES "^-?[0-9]+$")
      show_output("${out}" "${slog}")
      montezuma_fail("capturing ${shot} with ${driver}: ${rc} (timeout 600 s)")
    endif()
    if(rc EQUAL 0)
      file(GLOB shots "${dir}/frame*.png")
      list(SORT shots)
      list(LENGTH shots n)
      if(n GREATER 0)
        list(GET shots -1 last)
        file(SIZE "${last}" bytes)
      else()
        set(bytes 0)
      endif()
      if(bytes GREATER 0)
        show_output("${out}" "${slog}")
        # An error in the run that rendered fails the capture: falling back
        # to another driver would not fix it. Errors printed at exit (such
        # as resources still in use) reach only the console, not the log.
        file(WRITE "${work}/${shot}.out" "${out}")
        montezuma_log_has(errors "${slog}" "${work}/${shot}.out" REGEX "ERROR|SCRIPT ERROR" IGNORE ${known_engine_lines})
        if(errors)
          montezuma_fail("engine errors while capturing ${shot} with ${driver}")
        endif()
        file(COPY_FILE "${last}" "${SCREENS}/${shot}.png")
        get_filename_component(last_name "${last}" NAME)
        montezuma_print("captured ${shot}.png with ${driver} from ${last_name}")
        return()
      endif()
    endif()
    show_output("${out}" "${slog}")
    message(NOTICE "no frames rendered for ${shot} with ${driver}")
  endforeach()
  montezuma_fail("no frames rendered for ${shot} with any of: ${drivers}")
endfunction()

# 15 frames (0.5 s) let the title fade in; 30 frames (1 s) let the board
# settle. --start-level plays the first level, so the board shows with the
# in-level HUD over it.
capture(title 15)
# GitHub's macOS runners expose an Apple Paravirtual device that reports
# 32-bit image atomics and then refuses the R32_UINT atomic image Godot 4.8
# beta 1's volumetric fog creates, so the first in-level frame segfaults
# there (docs/reference/engine-issues.md, work package 65). The board shot
# is skipped on that device only; any real GPU still captures it and still
# fails loudly.
file(STRINGS "${work}/title.log" paravirtual REGEX "Apple Paravirtual device")
if(paravirtual)
  montezuma_print("skipping the board capture: Apple Paravirtual device cannot run volumetric fog (engine-issues.md, WP-65)")
else()
  capture(board 30 --start-level=e1-l01)
endif()

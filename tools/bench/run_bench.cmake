# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Runs the frame-time benchmark and, with READABILITY, the readability
# capture on this machine, then the checks. Reference:
# docs/reference/benchmark.md; how-to: docs/how-to/run-the-benchmark.md.
#
# Usage: cmake [-D<OPTION>=<value>...] -P tools/bench/run_bench.cmake
#   GODOT        Godot 4.7.2 editor binary (default: godot on PATH)
#   TIER         desktop-high, desktop-standard, mobile or web: runs with
#                that quality tier (--quality) and checks the feature list
#                against it (default: the tier QualityTiers picks)
#   DRIVER       --rendering-driver for Godot, e.g. vulkan, metal, d3d12,
#                opengl3 (default: the project's)
#   SIZE         render size for the benchmark, WxH (default 1920x1080); the
#                runner locks it and the run fails if it does not hold
#   VSYNC=off    turn vsync off, so frame time is the work, not the
#                display's pace (default: the project's vsync); also asks
#                for mailbox presentation in override.cfg
#   TOLERANCE    how far over a whole 60 Hz period a percentile may sit
#                (default 0.05)
#   SECONDS      capture length (default 60)
#   WARMUP       warm-up before the capture (default 10)
#   OUT          where reports and captures go (default bench-out/)
#   NATIVE=OFF   use the tier's render scale and upscaler (FSR 2, MetalFX);
#                by default the 3D renders at SIZE with no upscaler
#                (--quality-native), so the upscale costs nothing
#   FULLSCREEN=ON  run the benchmark exclusive full screen at the screen's
#                size (--bench-fullscreen); SIZE is then ignored
#   QUALITY_OFF  comma-separated feature ids to turn off for the runs
#                (--quality-off), to measure what each costs; the feature
#                check then fails for those rows
#   GPU_PROFILE=ON  print Godot's GPU time per render pass about once a
#                second (--gpu-profile; zeros on Metal in Godot 4.7.2)
#   READABILITY=ON  also capture at 540x1170 and check rules 2, 4, 5 and 6
#   XVFB=ON      run under xvfb-run (machines without a display)
# Each Godot run is stopped after SECONDS + WARMUP + 300 s (the readability
# capture after 600 s) and counts as a failure.
# Exit status: 0 when every run and check passed (budget, render size,
# replay, features, readability), non-zero otherwise.
include("${CMAKE_CURRENT_LIST_DIR}/../release/steps.cmake")
if(NOT GODOT)
  set(GODOT godot)
endif()
if(NOT SIZE)
  set(SIZE 1920x1080)
endif()
if(NOT SECONDS)
  set(SECONDS 60)
endif()
if(NOT WARMUP)
  set(WARMUP 10)
endif()
if(NOT OUT)
  set(OUT bench-out)
endif()
if(NOT SIZE MATCHES "^[1-9][0-9]*x[1-9][0-9]*$")
  montezuma_fail("SIZE must look like 1920x1080, got ${SIZE}")
endif()
if(DEFINED VSYNC AND NOT VSYNC STREQUAL "off")
  montezuma_fail("VSYNC takes only off")
endif()
if(NOT SECONDS MATCHES "^[0-9]+$" OR NOT WARMUP MATCHES "^[0-9]+$")
  montezuma_fail("SECONDS and WARMUP take whole seconds")
endif()
montezuma_abspath(OUT)
file(MAKE_DIRECTORY "${OUT}")
montezuma_python(python)
set(bench_tools "${MONTEZUMA_ROOT}/tools/bench")

# Since Godot 4.7, the window size comes from an override.cfg next to
# project.godot; keep any the user has and put it back afterwards, also when
# a run times out.
set(cfg "${MONTEZUMA_ROOT}/game/override.cfg")
set(saved "${OUT}/override.cfg.saved")
file(REMOVE "${saved}")
if(EXISTS "${cfg}")
  file(COPY_FILE "${cfg}" "${saved}")
endif()
function(restore_override)
  if(EXISTS "${saved}")
    file(RENAME "${saved}" "${cfg}")
  else()
    file(REMOVE "${cfg}")
  endif()
endfunction()
set(MONTEZUMA_ON_FAIL restore_override)
# With VSYNC=off the window also asks for mailbox presentation: some
# compositors (GNOME) keep vsync on when Godot asks for it off, and
# mailbox never waits for the display.
function(window size)
  string(REGEX MATCH "^([0-9]+)x([0-9]+)$" _ "${size}")
  set(present "")
  if(VSYNC STREQUAL "off")
    set(present "window/vsync/vsync_mode=3\n")
  endif()
  file(WRITE "${cfg}" "[display]\nwindow/size/window_width_override=${CMAKE_MATCH_1}\nwindow/size/window_height_override=${CMAKE_MATCH_2}\n${present}")
endfunction()

# run_godot(<timeout> <result var> <arg>...)
function(run_godot timeout result)
  set(cmd "${GODOT}" --path "${MONTEZUMA_ROOT}/game")
  if(DRIVER)
    list(APPEND cmd --rendering-driver "${DRIVER}")
  endif()
  list(APPEND cmd ${ARGN})
  if(XVFB)
    list(PREPEND cmd xvfb-run -a -s "-screen 0 2048x2048x24")
  endif()
  montezuma_run(TIMEOUT ${timeout} RESULT rc COMMAND ${cmd})
  set(${result} ${rc} PARENT_SCOPE)
endfunction()

# check(<report file> <arg>...): runs one check script, prints its output
# and keeps it in the report file; a failure sets status.
function(check report)
  montezuma_run(TIMEOUT 300 RESULT rc OUTPUT out ECHO COMMAND "${python}" ${ARGN})
  file(WRITE "${report}" "${out}")
  if(NOT rc EQUAL 0)
    set(status 1 PARENT_SCOPE)
  endif()
endfunction()

set(quality "")
if(TIER)
  set(quality "--quality=${TIER}")
endif()
if(NOT DEFINED NATIVE OR NATIVE)
  list(APPEND quality --quality-native)
endif()
if(QUALITY_OFF)
  list(APPEND quality "--quality-off=${QUALITY_OFF}")
endif()
set(engine "")
if(GPU_PROFILE)
  set(engine --gpu-profile)
endif()
set(bench "--bench-seconds=${SECONDS}" "--bench-warmup=${WARMUP}" "--bench-size=${SIZE}" "--bench-out=${OUT}/benchmark.json")
if(FULLSCREEN)
  list(APPEND bench --bench-fullscreen)
endif()
if(VSYNC STREQUAL "off")
  list(APPEND bench --bench-vsync=off)
endif()
if(NOT "${TOLERANCE}" STREQUAL "")
  list(APPEND bench "--bench-tolerance=${TOLERANCE}")
endif()
math(EXPR bench_timeout "${SECONDS} + ${WARMUP} + 300")

set(status 0)
# A stale report from an earlier run must not pass for this one.
file(REMOVE "${OUT}/benchmark.json")
window("${SIZE}")
run_godot(${bench_timeout} rc ${engine} res://scenes/bench/bench.tscn -- ${bench} ${quality})
if(NOT rc EQUAL 0)
  set(status 1)
endif()
if(EXISTS "${OUT}/benchmark.json")
  set(size_check --size "${SIZE}")
  if(FULLSCREEN)
    set(size_check "")
  endif()
  check("${OUT}/run.txt" "${bench_tools}/check_run.py" "${OUT}/benchmark.json" ${size_check})
  check("${OUT}/features.txt" "${bench_tools}/check_features.py" "${OUT}/benchmark.json")
else()
  message(NOTICE "FAIL benchmark: no report written")
  set(status 1)
endif()

if(READABILITY)
  window(540x1170)
  run_godot(600 rc res://scenes/bench/readability.tscn -- "--capture-out=${OUT}/readability" ${quality})
  if(NOT rc EQUAL 0)
    set(status 1)
  endif()
  check("${OUT}/readability.txt" "${bench_tools}/check_readability.py" "${OUT}/readability" --json "${OUT}/readability.json")
endif()

restore_override()
set(MONTEZUMA_ON_FAIL "")
if(NOT status EQUAL 0)
  message(FATAL_ERROR "benchmark: a run or check failed (see above and ${OUT})")
endif()

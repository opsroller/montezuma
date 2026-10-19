# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Shared helpers for the build steps, which are CMake scripts run with
# `cmake -D<VAR>=<value>... -P <step>.cmake`. Every child process goes
# through montezuma_run, which gives it a hard timeout: a step that hangs
# fails with "timed out after N s" instead of holding the job.
# Reference: docs/reference/build-pipeline.md#build-steps
cmake_minimum_required(VERSION 3.22)

get_filename_component(MONTEZUMA_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# A command to call before a step stops with an error, for steps that change
# a committed file for the length of a run and must put it back.
set(MONTEZUMA_ON_FAIL "")

# montezuma_fail(<message>...): undoes the step's temporary changes and stops.
macro(montezuma_fail)
  if(MONTEZUMA_ON_FAIL)
    cmake_language(CALL ${MONTEZUMA_ON_FAIL})
  endif()
  message(FATAL_ERROR ${ARGN})
endmacro()

# montezuma_require(<var>... ): stops when a -D variable is missing.
function(montezuma_require)
  foreach(var IN LISTS ARGN)
    if("${${var}}" STREQUAL "")
      montezuma_fail("missing -D${var}=<value> (see the usage at the top of ${CMAKE_SCRIPT_MODE_FILE})")
    endif()
  endforeach()
endfunction()

# montezuma_abspath(<var>): makes a -D path absolute against the current
# directory, expanding a leading ~ (the shell leaves it in -DVAR=~/dir).
function(montezuma_abspath var)
  string(REGEX REPLACE "^~" "$ENV{HOME}" path "${${var}}")
  get_filename_component(path "${path}" ABSOLUTE)
  set(${var} "${path}" PARENT_SCOPE)
endfunction()

# montezuma_print(<text>): writes a line to standard output, unlike message().
function(montezuma_print text)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "${text}")
endfunction()

# montezuma_tail(<file> <lines>): prints the last lines of a log to stderr.
function(montezuma_tail file count)
  if(NOT EXISTS "${file}")
    return()
  endif()
  file(STRINGS "${file}" lines)
  list(LENGTH lines n)
  if(n GREATER count)
    math(EXPR first "${n} - ${count}")
    list(SUBLIST lines ${first} ${count} lines)
  endif()
  list(JOIN lines "\n" text)
  message(NOTICE "${text}")
endfunction()

# montezuma_run(TIMEOUT <seconds> COMMAND <cmd>...
#               [WORKING_DIRECTORY <dir>] [LOG <file>] [OUTPUT <var> [ECHO]] [RESULT <var>])
# Runs one child process. LOG sends stdout and stderr to a file, OUTPUT
# captures both into a variable (ECHO also prints them as they come);
# otherwise they stream to the console. A non-zero exit stops the step, unless RESULT names a variable to
# receive the exit code. Running past TIMEOUT always stops the step.
function(montezuma_run)
  cmake_parse_arguments(PARSE_ARGV 0 R "ECHO" "TIMEOUT;WORKING_DIRECTORY;LOG;OUTPUT;RESULT" "COMMAND")
  if(NOT R_TIMEOUT OR NOT R_COMMAND)
    message(FATAL_ERROR "montezuma_run needs TIMEOUT and COMMAND")
  endif()
  set(args COMMAND ${R_COMMAND} TIMEOUT ${R_TIMEOUT} RESULT_VARIABLE rc)
  if(R_WORKING_DIRECTORY)
    list(APPEND args WORKING_DIRECTORY "${R_WORKING_DIRECTORY}")
  endif()
  if(R_LOG)
    list(APPEND args OUTPUT_FILE "${R_LOG}" ERROR_FILE "${R_LOG}")
  elseif(R_OUTPUT)
    list(APPEND args OUTPUT_VARIABLE out ERROR_VARIABLE out)
    if(R_ECHO)
      list(APPEND args ECHO_OUTPUT_VARIABLE ECHO_ERROR_VARIABLE)
    endif()
  endif()
  execute_process(${args})
  list(JOIN R_COMMAND " " shown)
  if(R_OUTPUT)
    set(${R_OUTPUT} "${out}" PARENT_SCOPE)
  endif()
  if(NOT rc MATCHES "^-?[0-9]+$")
    if(R_LOG)
      montezuma_tail("${R_LOG}" 40)
    endif()
    if(rc MATCHES "timeout")
      montezuma_fail("${shown}: timed out after ${R_TIMEOUT} s")
    endif()
    montezuma_fail("${shown}: ${rc}")
  endif()
  if(R_RESULT)
    set(${R_RESULT} ${rc} PARENT_SCOPE)
  elseif(NOT rc EQUAL 0)
    if(R_LOG)
      montezuma_tail("${R_LOG}" 40)
      montezuma_fail("${shown}: exit status ${rc} (full log: ${R_LOG})")
    endif()
    montezuma_fail("${shown}: exit status ${rc}")
  endif()
endfunction()

# montezuma_load_env(): sets the variables in tools/release/godot.env.
macro(montezuma_load_env)
  file(STRINGS "${MONTEZUMA_ROOT}/tools/release/godot.env" _env_lines REGEX "^[A-Z0-9_]+=")
  foreach(_line IN LISTS _env_lines)
    string(REGEX MATCH "^([A-Z0-9_]+)=(.*)$" _ "${_line}")
    set(${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
  endforeach()
endmacro()

# montezuma_python(<var>): the Python 3 interpreter the helper scripts run on.
function(montezuma_python var)
  find_program(_python NAMES python3 python REQUIRED)
  set(${var} "${_python}" PARENT_SCOPE)
endfunction()

# montezuma_version(<var>): the version of the checked-out commit (needs
# full history and tags).
#   tagged vX.Y.Z -> X.Y.Z
#   otherwise     -> <last tag or 0.0.0>-dev.<commit count>.g<short sha>
function(montezuma_version var)
  set(git git -C "${MONTEZUMA_ROOT}")
  execute_process(COMMAND ${git} describe --tags --exact-match --match "v[0-9]*"
    TIMEOUT 60 RESULT_VARIABLE rc OUTPUT_VARIABLE tag ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(rc STREQUAL "0")
    string(REGEX REPLACE "^v" "" tag "${tag}")
    set(${var} "${tag}" PARENT_SCOPE)
    return()
  elseif(NOT rc MATCHES "^[0-9]+$")
    montezuma_fail("git describe: ${rc}")
  endif()
  execute_process(COMMAND ${git} describe --tags --abbrev=0 --match "v[0-9]*"
    TIMEOUT 60 RESULT_VARIABLE rc OUTPUT_VARIABLE last ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(rc STREQUAL "0")
    string(REGEX REPLACE "^v" "" last "${last}")
  elseif(rc MATCHES "^[0-9]+$")
    set(last 0.0.0)
  else()
    montezuma_fail("git describe: ${rc}")
  endif()
  montezuma_run(TIMEOUT 60 OUTPUT count COMMAND ${git} rev-list --count HEAD)
  montezuma_run(TIMEOUT 60 OUTPUT sha COMMAND ${git} rev-parse --short=7 HEAD)
  string(STRIP "${count}" count)
  string(STRIP "${sha}" sha)
  set(${var} "${last}-dev.${count}.g${sha}" PARENT_SCOPE)
endfunction()

# montezuma_source_date_epoch(): commit time for archive timestamps, so
# packaging adds no clock noise. Kept when the caller already set it.
function(montezuma_source_date_epoch)
  if(NOT "$ENV{SOURCE_DATE_EPOCH}" STREQUAL "")
    return()
  endif()
  execute_process(COMMAND git -C "${MONTEZUMA_ROOT}" log -1 --format=%ct
    TIMEOUT 60 RESULT_VARIABLE rc OUTPUT_VARIABLE t OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT rc STREQUAL "0")
    montezuma_fail("git log: ${rc}")
  endif()
  set(ENV{SOURCE_DATE_EPOCH} "${t}")
endfunction()

# montezuma_write_sha256(<file>): writes <file>.sha256 in sha256sum's format
# and prints it.
function(montezuma_write_sha256 path)
  file(SHA256 "${path}" hash)
  get_filename_component(name "${path}" NAME)
  file(WRITE "${path}.sha256" "${hash}  ${name}\n")
  montezuma_print("${hash}  ${name}")
endfunction()

# montezuma_log_has(<var> <file>... REGEX <regex>): TRUE in <var> when any
# line of the files matches; the matching lines are printed to stderr.
function(montezuma_log_has var)
  cmake_parse_arguments(PARSE_ARGV 1 L "" "REGEX" "")
  set(found FALSE)
  foreach(f IN LISTS L_UNPARSED_ARGUMENTS)
    if(EXISTS "${f}")
      file(STRINGS "${f}" hits REGEX "${L_REGEX}")
      foreach(h IN LISTS hits)
        message(NOTICE "${h}")
        set(found TRUE)
      endforeach()
    endif()
  endforeach()
  set(${var} ${found} PARENT_SCOPE)
endfunction()

# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Downloads the pinned Godot editor (Linux x86_64) and export templates,
# verifies both against their SHA-512 checksums, and installs only the
# templates asked for.
#
# Usage: cmake -DDIR=<dir> [-DTEMPLATES=<template>;...] -P tools/release/fetch-godot.cmake
#   DIR        where the editor binary goes (as <dir>/godot) and downloads are kept
#   TEMPLATES  file names inside the templates archive, e.g. linux_release.x86_64
#
# Templates are installed into Godot's per-user templates folder, where the
# editor looks for them. Downloads already in DIR are re-verified, not
# re-fetched, so DIR can be cached between runs.
include("${CMAKE_CURRENT_LIST_DIR}/steps.cmake")
montezuma_load_env()
montezuma_require(DIR)
montezuma_abspath(DIR)
file(MAKE_DIRECTORY "${DIR}")
set(base "${GODOT_RELEASES_URL}/${GODOT_TAG}")

function(fetch file sum)
  set(path "${DIR}/${file}")
  if(EXISTS "${path}")
    file(SHA512 "${path}" have)
    if(have STREQUAL sum)
      return()
    endif()
  endif()
  file(DOWNLOAD "${base}/${file}" "${path}" TIMEOUT 600 INACTIVITY_TIMEOUT 60
    TLS_VERIFY ON EXPECTED_HASH SHA512=${sum} STATUS status)
  list(GET status 0 code)
  if(NOT code EQUAL 0)
    file(REMOVE "${path}")
    montezuma_fail("download of ${base}/${file} failed: ${status}")
  endif()
  message(STATUS "${file}: checksum OK")
endfunction()

fetch("${GODOT_EDITOR_LINUX_ZIP}" "${GODOT_EDITOR_LINUX_SHA512}")
string(REGEX REPLACE "\\.zip$" "" editor "${GODOT_EDITOR_LINUX_ZIP}")
file(ARCHIVE_EXTRACT INPUT "${DIR}/${GODOT_EDITOR_LINUX_ZIP}" DESTINATION "${DIR}" PATTERNS "${editor}")
file(RENAME "${DIR}/${editor}" "${DIR}/godot")
file(CHMOD "${DIR}/godot" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

if(TEMPLATES)
  fetch("${GODOT_TEMPLATES_TPZ}" "${GODOT_TEMPLATES_SHA512}")
  if(NOT "$ENV{XDG_DATA_HOME}" STREQUAL "")
    set(data "$ENV{XDG_DATA_HOME}")
  else()
    set(data "$ENV{HOME}/.local/share")
  endif()
  set(templates "${data}/godot/export_templates/${GODOT_TEMPLATES_DIR_NAME}")
  set(members templates/version.txt)
  foreach(t IN LISTS TEMPLATES)
    list(APPEND members "templates/${t}")
  endforeach()
  set(unpack "${DIR}/templates-unpack")
  file(REMOVE_RECURSE "${unpack}")
  file(ARCHIVE_EXTRACT INPUT "${DIR}/${GODOT_TEMPLATES_TPZ}" DESTINATION "${unpack}" PATTERNS ${members})
  foreach(m IN LISTS members)
    if(NOT EXISTS "${unpack}/${m}")
      montezuma_fail("${GODOT_TEMPLATES_TPZ} has no ${m}")
    endif()
  endforeach()
  file(MAKE_DIRECTORY "${templates}")
  file(COPY "${unpack}/templates/" DESTINATION "${templates}")
  file(REMOVE_RECURSE "${unpack}")
  list(JOIN TEMPLATES " " shown)
  message(STATUS "Installed templates in ${templates}: ${shown}")
endif()

montezuma_run(TIMEOUT 60 COMMAND "${DIR}/godot" --version)

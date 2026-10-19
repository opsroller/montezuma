# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Prints the version of the checked-out commit (needs full history and tags).
#   tagged vX.Y.Z  -> X.Y.Z
#   otherwise      -> <last tag or 0.0.0>-dev.<commit count>.g<short sha>
#
# Usage: cmake -P tools/release/version.cmake
include("${CMAKE_CURRENT_LIST_DIR}/steps.cmake")
montezuma_version(version)
montezuma_print("${version}")

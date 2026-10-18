#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Prints the version of the checked-out commit (needs full history and tags).
#   tagged vX.Y.Z  -> X.Y.Z
#   otherwise      -> <last tag or 0.0.0>-dev.<commit count>.g<short sha>
set -euo pipefail

if tag="$(git describe --tags --exact-match --match 'v[0-9]*' 2>/dev/null)"; then
  echo "${tag#v}"
  exit
fi
last="$(git describe --tags --abbrev=0 --match 'v[0-9]*' 2>/dev/null || echo v0.0.0)"
echo "${last#v}-dev.$(git rev-list --count HEAD).g$(git rev-parse --short=7 HEAD)"

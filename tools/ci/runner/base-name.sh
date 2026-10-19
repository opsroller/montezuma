#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Prints the base image volume name: a hash of the provisioning inputs, so a
# change to them is baked into a new volume instead of reusing a stale one.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
hash="$(cat "$here/user-data.yaml" "$here/bake.sh" | sha256sum | cut -c1-12)"
echo "montezuma-runner-base-$hash.qcow2"

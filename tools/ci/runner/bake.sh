#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Charles (opsroller) and Montezuma contributors
#
# Builds the self-hosted runner base image in a libvirt storage pool: the
# pinned Ubuntu 24.04 cloud image, provisioned once by user-data.yaml. The
# volume is named after a hash of this directory, so run.sh finds it and a
# changed provision makes a new one. Idempotent: an existing base is kept.
# Usage: tools/ci/runner/bake.sh   (env: POOL, LIBVIRT_DEFAULT_URI)
# Reference: docs/reference/ci-pipeline.md
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
export LIBVIRT_DEFAULT_URI="${LIBVIRT_DEFAULT_URI:-qemu:///system}"
pool="${POOL:-default}"
image_url=https://cloud-images.ubuntu.com/releases/noble/release-20261017/ubuntu-24.04-server-cloudimg-amd64.img
image_sha256=04e8f2461a16a44e78a370284471f29616cdd9baf1893df25807b71f985b6c94
base="$("$here/base-name.sh")"

if virsh vol-info --pool "$pool" "$base" >/dev/null 2>&1; then
  echo "Base image $pool/$base already exists."
  exit 0
fi

work="$(mktemp -d)"
trap 'kill "${server:-}" 2>/dev/null || true; virsh destroy montezuma-runner-bake >/dev/null 2>&1 || true; rm -rf "$work"' EXIT

echo "Downloading the Ubuntu cloud image..."
curl -sfL -o "$work/ubuntu.img" "$image_url"
echo "$image_sha256  $work/ubuntu.img" | sha256sum -c

partial="$base.partial"
virsh vol-delete --pool "$pool" "$partial" >/dev/null 2>&1 || true
virsh vol-create-as "$pool" "$partial" "$(stat -c %s "$work/ubuntu.img")" --format qcow2 >/dev/null
virsh vol-upload --pool "$pool" "$partial" "$work/ubuntu.img"
virsh vol-resize --pool "$pool" "$partial" 80G >/dev/null

# NoCloud seed over HTTP from the libvirt network's host address; the guest
# fetches /done as its last step, which is how a successful bake is told
# apart from one that stopped early.
host_ip="$(virsh net-dumpxml default | sed -n "s/.*<ip address='\([^']*\)'.*/\1/p" | head -1)"
mkdir -p "$work/seed"
cp "$here/user-data.yaml" "$work/seed/user-data"
printf 'instance-id: montezuma-runner-bake\nlocal-hostname: montezuma-runner\n' >"$work/seed/meta-data"
port=$((20000 + RANDOM % 10000))
python3 -m http.server --bind "$host_ip" --directory "$work/seed" "$port" 2>"$work/http.log" &
server=$!

echo "Provisioning (packages, actions runner); this takes a few minutes..."
virt-install --name montezuma-runner-bake --transient --import \
  --memory 8192 --vcpus 8 --osinfo ubuntu24.04 \
  --disk "vol=$pool/$partial,bus=virtio" --network network=default \
  --sysinfo "system.serial=ds=nocloud;s=http://$host_ip:$port/" \
  --graphics none --noautoconsole --wait 30

if ! grep -q 'GET /done' "$work/http.log"; then
  echo "Provisioning did not finish; keeping no image." >&2
  virsh vol-delete --pool "$pool" "$partial" >/dev/null
  exit 1
fi
virsh vol-clone --pool "$pool" "$partial" "$base" >/dev/null
virsh vol-delete --pool "$pool" "$partial" >/dev/null
echo "Base image ready: $pool/$base"

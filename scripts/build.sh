#!/usr/bin/env bash
# Build inside a Fedora 44 container (the host is immutable, no toolchain).
set -euo pipefail
cd "$(dirname "$0")/.."

IMAGE=falcos-bootc-updates-builder

# --network=host --dns works around rootless container DNS on some hosts.
if ! podman image exists "$IMAGE"; then
    podman build --network=host --dns 127.0.0.53 -t "$IMAGE" -f scripts/Containerfile.builder scripts
fi

podman run --rm -v "$PWD:/src:Z" -w /src "$IMAGE" bash -c '
    cmake -B build -GNinja -DCMAKE_INSTALL_PREFIX=/usr -DKDE_INSTALL_LIBEXECDIR=/usr/libexec
    cmake --build build
'

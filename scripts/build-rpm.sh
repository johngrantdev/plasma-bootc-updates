#!/usr/bin/env bash
# Build the RPM inside the builder container, output to ./dist.
set -euo pipefail
cd "$(dirname "$0")/.."

IMAGE=falcos-bootc-updates-builder

# --network=host --dns works around rootless container DNS on some hosts.
if ! podman image exists "$IMAGE"; then
    podman build --network=host --dns 127.0.0.53 -t "$IMAGE" -f scripts/Containerfile.builder scripts
fi

podman run --rm -v "$PWD:/src:Z" -w /src "$IMAGE" bash -c '
    VERSION=$(awk "/^Version:/ {print \$2}" packaging/falcos-bootc-updates.spec)
    mkdir -p ~/rpmbuild/SOURCES dist
    tar czf ~/rpmbuild/SOURCES/falcos-bootc-updates-$VERSION.tar.gz \
        --transform "s,^,falcos-bootc-updates-$VERSION/," \
        --exclude build --exclude dist --exclude .git .
    rpmbuild -ba packaging/falcos-bootc-updates.spec
    cp ~/rpmbuild/RPMS/x86_64/*.rpm dist/
'

#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# tests/systemd.sh on the Debian a Raspberry Pi OS is made from, with its
# own systemd, in a container: the update path on bullseye (Python 3.9)
# and on the newer one (the regression gate, decided 17 Nov 2026). CI runs
# it on arm64, as a Raspberry Pi is.
#
#   tests/pios.sh bullseye|bookworm|trixie
#
# On a spare computer with Docker: the container is privileged (systemd).
set -euo pipefail
cd "$(dirname "$0")/.."
DEB=${1:?which Debian: bullseye, bookworm or trixie}
NAME=aera-pios-$DEB
cleanup() { docker rm -f "$NAME" >/dev/null 2>&1 || true; }
trap cleanup EXIT
cleanup

docker build --progress=plain -t "aera-pios:$DEB" - <<EOF
FROM debian:$DEB
# (bullseye's support ended: its security updates are on archive.debian.org now)
RUN if [ "$DEB" = bullseye ]; then \
      sed -i 's|http://deb.debian.org/debian-security|http://archive.debian.org/debian-security|' /etc/apt/sources.list; fi \
    && apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y -q --no-install-recommends \
      systemd systemd-sysv dbus sudo git python3 iproute2 ca-certificates procps curl \
    && rm -rf /var/lib/apt/lists/* \
    && git config --system --add safe.directory '*'
STOPSIGNAL SIGRTMIN+3
CMD ["/sbin/init"]
EOF

docker run -d --name "$NAME" --privileged --cgroupns=host -v /sys/fs/cgroup:/sys/fs/cgroup:rw \
  --tmpfs /run --tmpfs /run/lock --tmpfs /tmp:exec -v "$PWD:/src" "aera-pios:$DEB" >/dev/null
for _ in $(seq 1 60); do
  state=$(docker exec "$NAME" systemctl is-system-running 2>/dev/null || true)
  case "$state" in running|degraded) break ;; esac
  sleep 1
done
echo "== Debian $DEB ($(docker exec "$NAME" python3 --version)), systemd: $state"
docker exec "$NAME" bash -c "cd /src && tests/systemd.sh"

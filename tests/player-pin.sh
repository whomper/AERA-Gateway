#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# go-librespot's pinned release (gateway/aera-gateway's PLAYER_VERSION and
# PLAYER_SHA256): each architecture's tarball downloads and has its pinned
# sha256. A variant with no pin yet is printed, to be pinned, not failed.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp" <<'PY'
import hashlib, importlib.machinery, importlib.util, sys, urllib.request
loader = importlib.machinery.SourceFileLoader("ag", "gateway/aera-gateway")
spec = importlib.util.spec_from_loader("ag", loader)
ag = importlib.util.module_from_spec(spec)
loader.exec_module(ag)
bad = 0
for variant in sorted(set(ag.PLAYER_ARCH.values())):
    url = ag.PLAYER_URL % (ag.PLAYER_VERSION, variant)
    data = urllib.request.urlopen(url, timeout=120).read()
    got = hashlib.sha256(data).hexdigest()
    want = ag.PLAYER_SHA256.get(variant)
    if want is None:
        print("::warning::%s has no pin yet: %r: %r," % (variant, variant, got))
    elif got != want:
        print("%s: sha256 %s, pinned %s" % (variant, got, want))
        bad += 1
    else:
        print("%s: %s ok (%d bytes)" % (variant, got, len(data)))
sys.exit(1 if bad else 0)
PY

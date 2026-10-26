#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# jsc/eweb/ from EWeb (github.com/whomper/EWeb): what aerajsc, webgate's
# compiler of scripts for EWeb, is built from (EWeb's tools/jsc and its
# third_party/quickjs, EWeb's patches applied), at COMMIT (default: EWeb's
# main), and jsc/EWEB that commit. Change those files in EWeb, never here:
# a change made only in this copy is lost at the next sync (and aerajsc's
# bytecode would no longer be what EWEB.PRG reads). Sync at each EWeb
# release whose QuickJS changed: aerajsc compiles only for the same tag.
#
#   tools/sync-jsc.sh [COMMIT]
set -e
cd "$(dirname "$0")/.."
ref=${1:-main}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
git clone -q --filter=blob:none --no-checkout https://github.com/whomper/EWeb "$tmp/e"
git -C "$tmp/e" sparse-checkout set --no-cone /tools/jsc /third_party/quickjs /COPYING
git -C "$tmp/e" -c advice.detachedHead=false checkout -q "$ref"
rm -rf jsc/eweb
mkdir -p jsc/eweb/tools jsc/eweb/third_party
cp -R "$tmp/e/tools/jsc" jsc/eweb/tools/
cp -R "$tmp/e/third_party/quickjs" jsc/eweb/third_party/
rm -rf jsc/eweb/tools/jsc/build
git -C "$tmp/e" rev-parse HEAD > jsc/EWEB
echo "jsc/eweb: EWeb $(cat jsc/EWEB)"

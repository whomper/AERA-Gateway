#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# gateway/radiogate.py, spotgate.py and their tests from EPlay (github.com/whomper/EPlay,
# radio/), at COMMIT (default: EPlay's main), and gateway/EPLAY that commit.
# radiogate and spotgate are EPlay's: change it there, never here (a change made only in
# this copy is lost at the next sync). Sync at each EPlay commit EPlay names.
#
#   tools/sync-radio.sh [COMMIT]
set -e
cd "$(dirname "$0")/.."
ref=${1:-main}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
git clone -q --filter=blob:none --no-checkout https://github.com/whomper/EPlay "$tmp/e"
git -C "$tmp/e" sparse-checkout set --no-cone /radio/radiogate.py /radio/test_radiogate.py \
    /radio/spotgate.py /radio/test_spotgate.py
git -C "$tmp/e" -c advice.detachedHead=false checkout -q "$ref"
for f in radiogate.py test_radiogate.py spotgate.py test_spotgate.py; do cp "$tmp/e/radio/$f" gateway/; done
chmod 755 gateway/radiogate.py gateway/spotgate.py
git -C "$tmp/e" rev-parse HEAD > gateway/EPLAY
echo "gateway/radiogate.py, spotgate.py: EPlay $(cat gateway/EPLAY)"

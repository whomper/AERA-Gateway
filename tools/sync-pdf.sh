#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# pdf/epdf/ from EPDF (github.com/whomper/EPDF): what aerapdf, pdfgate's
# converter, is built from (EPDF's converter and the reader under it, the
# third-party code it builds with and the Liberation fonts), at COMMIT
# (default: EPDF's main), and pdf/EPDF that commit. Change those files in
# EPDF, never here: a change made only in this copy is lost at the next
# sync, and the Pi's converted PDFs would no longer be EConvert's. Sync at
# each EPDF release whose converter changed.
#
#   tools/sync-pdf.sh [COMMIT]
set -e
cd "$(dirname "$0")/.."
ref=${1:-main}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
git clone -q --filter=blob:none --no-checkout https://github.com/whomper/EPDF "$tmp/e"
git -C "$tmp/e" sparse-checkout set --no-cone /Makefile /VERSION /COPYING /src /tools/econvert.c \
    /common/src /common/third_party/bearssl /common/third_party/libjpeg-turbo /common/third_party/nsgif \
    /third_party/freetype /third_party/liberation
git -C "$tmp/e" -c advice.detachedHead=false checkout -q "$ref"
rm -rf pdf/epdf
mkdir -p pdf/epdf
for p in Makefile VERSION COPYING src tools common third_party; do
	cp -R "$tmp/e/$p" pdf/epdf/
done
rm -rf pdf/epdf/build
git -C "$tmp/e" rev-parse HEAD > pdf/EPDF
echo "pdf/epdf: EPDF $(cat pdf/EPDF)"

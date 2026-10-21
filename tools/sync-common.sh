#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# pens/common/ from AERA-Atari's common/ (github.com/whomper/AERA-Atari):
# what aerapens is built from (its tools/pens, src/pic.c and the decoders
# it uses), at COMMIT (default: AERA-Atari's main), and pens/COMMON that
# commit. Change those files in AERA-Atari, never here: a change made
# only in this copy is lost at the next sync (and aerapens' pens would no
# longer be the Atari's own).
#
#   tools/sync-common.sh [COMMIT]
set -e
cd "$(dirname "$0")/.."
ref=${1:-main}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
git clone -q --filter=blob:none --no-checkout https://github.com/whomper/AERA-Atari "$tmp/a"
git -C "$tmp/a" sparse-checkout set --no-cone /common/src /common/third_party/libjpeg-turbo \
    /common/third_party/tjpgd /common/third_party/nsgif /common/tools/pens /COPYING
git -C "$tmp/a" -c advice.detachedHead=false checkout -q "$ref"
from="$tmp/a/common"
rm -rf pens/common
mkdir -p pens/common/src pens/common/third_party pens/common/tools
for f in pic.c pic.h png.c png.h inflate.c inflate.h util.c util.h pic_lj8.c jpegdc.c jpegdc.h; do
    cp "$from/src/$f" pens/common/src/
done
cp -R "$from/third_party/libjpeg-turbo" "$from/third_party/tjpgd" "$from/third_party/nsgif" pens/common/third_party/
cp -R "$from/tools/pens" pens/common/tools/
rm -rf pens/common/tools/pens/build
git -C "$tmp/a" rev-parse HEAD > pens/COMMON
echo "pens/common: AERA-Atari $(cat pens/COMMON)"

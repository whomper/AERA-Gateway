# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# aerapens, for webgate's pictures as the Atari's pens (EWeb's protocol):
# built on the Pi when web is installed (cc and nm, from gcc's binutils):
#
#   make -C pens PENS_BUILD=v1.6     pens/aerapens
#
# Its source is AERA-Atari's common/ at the commit in COMMON, copied into
# common/ by tools/sync-common.sh and never edited here; its pens are the
# Atari's to the bit (AERA-Atari's common/tests/pic: make pens, and make
# fuzzpens for its input). It reads one request on stdin and writes one
# answer on stdout, opening nothing else: systemd runs one a connection
# on /run/webgate-pens.sock (webgate-pens.socket), as its own user, with
# little memory and time.
CC         ?= cc
PENS_BUILD ?= local
SRC        := $(shell find common -name '*.[ch]' -o -name Makefile)

all: aerapens
aerapens: COMMON $(SRC)
	$(MAKE) -C common/tools/pens CC="$(CC)" PENS_COMMON="$$(cut -c1-12 COMMON)" PENS_BUILD="$(PENS_BUILD)"
	cp common/tools/pens/build/aerapens $@
clean:
	rm -rf aerapens common/tools/pens/build
.PHONY: all clean

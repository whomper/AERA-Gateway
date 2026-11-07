# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# aerapdf, pdfgate's converter for EConvert and EPDF (EPDF's protocol):
# built on the Pi when pdf is installed, with cc and make only:
#
#   make -C pdf                      pdf/aerapdf
#
# Its source is EPDF's converter (src/, the reader and src/conv/; its
# FreeType, libjpeg, BearSSL and nsgif copies; the Liberation fonts) at
# the commit in EPDF, copied into epdf/ by tools/sync-pdf.sh and never
# edited here: what it writes is what EConvert writes on a Falcon, byte
# for byte. pdfgate runs one a conversion, as its own user, with little
# memory and time.
CC  ?= cc
SRC := $(shell find epdf -name '*.[chS]' -o -name Makefile)

all: aerapdf
aerapdf: EPDF $(SRC)
	$(MAKE) -C epdf CC="$(CC)" build/econvert
	cp epdf/build/econvert $@
clean:
	rm -rf aerapdf epdf/build
.PHONY: all clean

# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
# aerajsc, for webgate's scripts compiled for EWeb (EWeb's protocol):
# built on the Pi when web is installed (cc):
#
#   make -C jsc                      jsc/aerajsc
#
# Its source is EWeb's tools/jsc and third_party/quickjs at the commit in
# EWEB, copied into eweb/ by tools/sync-jsc.sh and never edited here: its
# bytecode is EWEB.PRG's own (EWeb's make bc68k). It reads one request on
# stdin and writes one answer on stdout, opening nothing else: systemd
# runs one a connection on /run/webgate-js.sock (webgate-js.socket), as
# its own user, with little memory and time.
CC  ?= cc
SRC := $(shell find eweb -name '*.[ch]' -o -name Makefile -o -name VERSION)

all: aerajsc
aerajsc: EWEB $(SRC)
	$(MAKE) -C eweb/tools/jsc CC="$(CC)"
	cp eweb/tools/jsc/build/aerajsc $@
clean:
	rm -rf aerajsc eweb/tools/jsc/build
.PHONY: all clean

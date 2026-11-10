# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 AERA | Erez Yaary
#
#   make            binary/EPDF.PRG (one program for every ST, TT and Falcon); host tools in build/
#   make test       host unit tests, and no FPU instruction in the Atari programs
#   make help       binary/EPDF.HYP and .REF from docs/GUIDE.md (HCP=path to hcp:
#                   common/tools/build-hcp.sh DIR builds it)
#   make dist       a release's files in dist/, named in lowercase: each file, and epdf-x.y.zip
#   make icons      icons/, src/gem/icon*.h and src/econv/icon*.h from common/icons/EPDF.txt, EPDFDOC.txt
#                   (and ECONVERT.txt when AERA-Atari has it)
#   make gui-test   EPDF.PRG driven in Hatari (tests/hatari/gui.py)
#   make rovl-test  render.ovl, the page as printed (stage 2), loaded and drawn in Hatari (tests/hatari/rovl.py)
#   make files-test each file's opens on a 1 MB ST against the budget (tests/hatari/files.py, 6.5)
#   big documents on a 1 MB ST: python3 tests/hatari/big.py WORK [NAME...] (the corpus)
#   make measure-atari  the stage 0 measuring programs (MEASURE.PRG and the others)
#   make measure    the measurement run on this computer (corpus: tools/fetch-corpus.py)
#
# binary/ is what make builds, never committed: releases carry the program.
# The version is read from VERSION and never written by the build.

VERSION_STR := $(shell cat VERSION)
BSSL   := common/third_party/bearssl
CC     ?= cc
CFLAGS ?= -O2 -g
HCFLAGS = $(CFLAGS) -fwrapv -std=gnu99 -Wall -Wextra -Wno-sign-compare -Wno-unused-parameter -Wno-unused-function \
          -Isrc -Icommon/src -I$(BSSL)/inc -I$(BSSL)/src

PDF_SRC  = src/pdfobj.c src/pdfxref.c src/pdffilt.c src/pdfcrypt.c src/pdfdoc.c src/pdftext.c src/pdfmeas.c
COMMON_SRC = common/src/inflate.c common/src/util.c common/src/futil.c
BSSL_SRC = $(addprefix $(BSSL)/src/, hash/md5.c hash/sha2small.c hash/sha2big.c \
           symcipher/aes_big_cbcdec.c symcipher/aes_big_cbcenc.c symcipher/aes_big_dec.c symcipher/aes_big_enc.c symcipher/aes_common.c \
           codec/dec32be.c codec/enc32be.c codec/dec32le.c codec/enc32le.c codec/dec64be.c codec/enc64be.c)
HDRS     = $(wildcard src/*.h src/conv/*.h src/rovl/*.h src/gem/*.h src/view/*.h src/econv/*.h common/src/*.h common/ui/*.h common/atari/*.h common/atari/include/*.h)

.PHONY: all test measure dist help clean atari measure-atari fttest imgbench gui-test files-test viewtest
all: build/pdfmeasure build/pdftext build/pdfmem build/glyphjob build/imgdump binary/EPDF.PRG
build:
	mkdir -p build
build/pdfmeasure: tools/pdfmeasure.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -o $@ tools/pdfmeasure.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC)

build/pdftext: tools/pdftext.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -o $@ tools/pdftext.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC)

HPIC_SRC = src/pdfimg.c src/ccitt.c src/rscale.c common/src/pic.c common/src/png.c common/src/jpegdc.c \
           common/third_party/tjpgd/tjpgd.c common/third_party/nsgif/src/gif.c common/third_party/nsgif/src/lzw.c
build/imgshow: tools/imgshow.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HPIC_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -Icommon/third_party/tjpgd -Icommon/third_party/nsgif/include -Icommon/third_party/nsgif/src -w \
	  -o $@ tools/imgshow.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HPIC_SRC)

# the reader's memory a page (phase 0.3: a 1 MB ST); PDF_MEMCAP=KB refuses past it
build/pdfmem: tools/pdfmem.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -o $@ tools/pdfmem.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC)

build/glyphjob: tools/glyphjob.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -o $@ tools/glyphjob.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC)

build/imgdump: tools/imgdump.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -o $@ tools/imgdump.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC)

build/test_trpara: tests/test_trpara.c src/view/trpara.c src/view/trpara.h common/src/charset.c | build
	$(CC) $(HCFLAGS) -Isrc/view -fsanitize=address,undefined -o $@ tests/test_trpara.c src/view/trpara.c common/src/charset.c

build/test_pens: tests/test_pens.c src/rovl/pens.c src/rovl/pens.h | build
	$(CC) $(HCFLAGS) -Isrc/rovl -fsanitize=address,undefined -o $@ tests/test_pens.c src/rovl/pens.c

build/test_ccitt: tests/test_ccitt.c src/ccitt.c src/ccitt.h src/ccitt_tab.h | build
	$(CC) $(HCFLAGS) -fsanitize=address,undefined -o $@ tests/test_ccitt.c src/ccitt.c

build/pdfmeasure_asan: tools/pdfmeasure.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined -o $@ tools/pdfmeasure.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC)

build/test_textview: tests/test_textview.c src/view/textview.c src/view/textview.h | build
	$(CC) -Isrc/view -Wall -Wextra -O2 -o $@ tests/test_textview.c src/view/textview.c

build/test_search: tests/test_search.c src/view/search.c src/view/search.h | build
	$(CC) -Isrc/view -Wall -Wextra -O2 -o $@ tests/test_search.c src/view/search.c
build/test_inf: tests/test_inf.c src/view/inf.c src/view/inf.h | build
	$(CC) -Isrc/view -Wall -Wextra -O2 -o $@ tests/test_inf.c src/view/inf.c
VIEW_TEST_SRC = src/view/docview.c src/view/textview.c src/view/tstore.c src/pdfio_host.c
build/test_tstore build/test_docview: build/test_%: tests/test_%.c $(VIEW_TEST_SRC) $(wildcard src/view/*.h) src/plat.h | build
	$(CC) -Isrc -Isrc/view -Icommon/src -Wall -Wextra -O2 -o $@ $< $(VIEW_TEST_SRC)

build/test_xe: tests/test_xe.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(HDRS) | build
	$(CC) $(HCFLAGS) -fsanitize=address,undefined -o $@ tests/test_xe.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC)

test: build/test_xe build/test_ccitt build/test_textview build/test_inf build/test_tstore build/test_docview build/test_search build/pdftext build/econvert build/test_pens build/test_trpara
	build/test_xe
	build/test_pens
	build/test_trpara
	python3 tests/convtext.py
	python3 tests/translate.py
	python3 tests/ccitt.py
	python3 tests/glyphwise.py
	python3 tests/crypt.py
	build/test_textview
	build/test_inf
	build/test_tstore build
	build/test_docview
	build/test_search
	@if [ -f build/m68000/epdf.elf ]; then python3 common/tools/oddaccess.py build/m68000/epdf.elf; fi
	tools/nofpu.sh build/m68000/epdf.elf build/m68030/measure.elf build/m68030/fttest.elf
	@if [ -f build/m68030/econvert.elf ]; then tools/nofpu.sh build/m68030/econvert.elf; fi
	@if [ -f build/m68030ovl/render.elf ]; then tools/nofpu.sh build/m68030ovl/render.elf; fi
measure: build/pdfmeasure
	@echo "see tools/measure.py"
clean:
	rm -rf build/pdfmeasure

# ---------------------------------------------------------------- the Atari programs
# Built like the family's other programs: a stock m68k ELF compiler, the shared runtime in
# common/atari, and common/tools/elf2tos.py for the TOS header and fixups.
XCC      = m68k-linux-gnu-gcc
GCCINC  := $(shell $(XCC) -print-file-name=include 2>/dev/null)
XOPT    ?= -O2
XFLAGS   = $(XCPU) $(XOPT) -fomit-frame-pointer -ffreestanding -fno-builtin -fno-pic -fno-pie \
           -fno-tree-loop-distribute-patterns -fno-store-merging -fno-asynchronous-unwind-tables \
           -ffunction-sections -fdata-sections -fwrapv -Wall -Wextra -Werror=implicit-function-declaration \
           -Wno-sign-compare -Wno-unused-parameter -Wno-unused-function -std=gnu99 -DPDF_TOS \
           -nostdinc -isystem $(GCCINC) -Iatari/include -Icommon/atari/include -Icommon/atari -Isrc -Icommon/src \
           -I$(BSSL)/inc -I$(BSSL)/src
XLDFLAGS = $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld \
           -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments
RT_C     = common/atari/libc.c common/atari/tos.c
STACK_SIZE ?= 49152
ATARI_SRC = $(PDF_SRC) src/pdfio_tos.c atari/measure.c $(COMMON_SRC) $(BSSL_SRC) $(RT_C)

# the 68000 build runs on every ST, TT and Falcon; the 68030 one on the TT and the Falcon
define ATARI_BUILD
build/$(1)/%.o: %.c $(HDRS) | build/$(1)
	@mkdir -p $$(dir $$@)
	@echo CC $$<
	@$$(XCC) $$(XFLAGS) -c -o $$@ $$<
build/$(1)/crt0.o: common/atari/crt0.S | build/$(1)
	$$(XCC) $$(XCPU) -DSTACK_SIZE=$$(STACK_SIZE) -c -o $$@ $$<
build/$(1)/nf.o: common/atari/nf.S | build/$(1)
	$$(XCC) $$(XCPU) -c -o $$@ $$<
build/$(1)/inflate_fast.o: common/atari/inflate_fast.S | build/$(1)
	$$(XCC) $$(XCPU) -c -o $$@ $$<
build/$(1)/common/src/inflate.o build/$(1)/src/pdffilt.o: XFLAGS += -DINFLATE_ASM
build/$(1):
	mkdir -p $$@
endef
# the CPU follows the folder, whichever program asks for an object first
build/m68000/%: XCPU = -m68000 -mstrict-align -fdisable-tree-bswap
build/m68030/%: XCPU = -m68030 -msoft-float
build/m68030lj/%: XCPU = -m68030 -msoft-float
build/m68000ec/%: XCPU = -m68000 -mstrict-align -fdisable-tree-bswap
build/m68030cv/%: XCPU = -m68030 -msoft-float
binary/LAUNCH.PRG: XCPU = -m68000 -mstrict-align -fdisable-tree-bswap
# the GEM building blocks as the family builds them, for size (at -O2 GCC 13
# once filled dd.c's char name[] with long moves at an odd stack offset:
# common c80905c copies it now; oddaccess.py in make test watches for more)
build/m68000/common/ui/%: XOPT = -Os
$(eval $(call ATARI_BUILD,m68000))
$(eval $(call ATARI_BUILD,m68030))
$(eval $(call ATARI_BUILD,m68030cv))
$(eval $(call ATARI_BUILD,m68030lj))
$(eval $(call ATARI_BUILD,m68000ec))

OBJS = $(patsubst %.c,%.o,$(ATARI_SRC))
binary:
	mkdir -p binary
binary/MEASURE.PRG: XCPU = -m68000 -mstrict-align -fdisable-tree-bswap
binary/MEASURE.PRG: $(addprefix build/m68000/,$(OBJS)) build/m68000/crt0.o build/m68000/nf.o build/m68000/inflate_fast.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68000/measure.elf \
	  build/m68000/crt0.o build/m68000/nf.o build/m68000/inflate_fast.o $(addprefix build/m68000/,$(OBJS)) -lgcc
	python3 common/tools/elf2tos.py build/m68000/measure.elf $@
binary/MEAS30.PRG: XCPU = -m68030 -msoft-float
binary/MEAS30.PRG: $(addprefix build/m68030/,$(OBJS)) build/m68030/crt0.o build/m68030/nf.o build/m68030/inflate_fast.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68030/measure.elf \
	  build/m68030/crt0.o build/m68030/nf.o build/m68030/inflate_fast.o $(addprefix build/m68030/,$(OBJS)) -lgcc
	python3 common/tools/elf2tos.py build/m68030/measure.elf $@

# ---------------------------------------------------------------- FreeType (third_party/freetype, 2.13.3)
FT      := third_party/freetype
FT_SRC  := $(addprefix $(FT)/src/, base/ftbase.c base/ftinit.c base/ftdebug.c base/ftbbox.c base/ftglyph.c base/ftbitmap.c \
           base/ftsynth.c sfnt/sfnt.c truetype/truetype.c cff/cff.c type1/type1.c psaux/psaux.c pshinter/pshinter.c \
           psnames/psmodule.c raster/raster.c smooth/smooth.c autofit/autofit.c) $(FT)/epdf/ftsystem.c
FT_DEFS  = -include $(FT)/epdf/ftprep.h -DFT2_BUILD_LIBRARY '-DFT_CONFIG_OPTIONS_H="epdf/ftoption.h"' '-DFT_CONFIG_MODULES_H="epdf/ftmodule.h"' \
           $(FT_EXTRA)
FT_INC   = -I$(FT)/include -I$(FT) -I$(FT)/epdf/include
define FT_BUILD
build/$(1)/ft/%.o: $(FT)/src/%.c | build/$(1)
	@mkdir -p $$(dir $$@)
	@echo CC $$<
	@$$(XCC) $$(XFLAGS) $$(FT_DEFS) $$(FT_INC) -Wno-unused-variable -Wno-implicit-fallthrough -Wno-cast-function-type -Wno-missing-field-initializers -Wno-dangling-pointer -I$$(dir $$<) -c -o $$@ $$<
build/$(1)/ft/epdf/%.o: $(FT)/epdf/%.c | build/$(1)
	@mkdir -p $$(dir $$@)
	@echo CC $$<
	@$$(XCC) $$(XFLAGS) $$(FT_DEFS) $$(FT_INC) -c -o $$@ $$<
build/$(1)/ft/setjmp.o: $(FT)/epdf/setjmp.S | build/$(1)
	@mkdir -p $$(dir $$@)
	$$(XCC) $$(XCPU) -c -o $$@ $$<
endef
$(eval $(call FT_BUILD,m68000))
$(eval $(call FT_BUILD,m68030))
FT_OBJS = $(patsubst $(FT)/src/%.c,ft/%.o,$(filter $(FT)/src/%,$(FT_SRC))) ft/epdf/ftsystem.o ft/setjmp.o

# EConvert's converter on this computer (src/conv/, FreeType built for the host in build/host-ft)
CONV_SRC = src/conv/conv.c src/conv/render.c src/conv/raster.c src/conv/cvjpeg.c src/conv/deflate.c src/conv/quant.c src/conv/jpegenc.c common/src/charset.c src/view/trpara.c
HFT_OBJS = $(patsubst $(FT)/src/%.c,build/host-ft/%.o,$(filter $(FT)/src/%,$(FT_SRC))) build/host-ft/epdf/ftsystem.o
build/host-ft/%.o: $(FT)/src/%.c
	@mkdir -p $(dir $@)
	@$(CC) -O2 -DFT2_BUILD_LIBRARY '-DFT_CONFIG_OPTIONS_H="epdf/ftoption.h"' '-DFT_CONFIG_MODULES_H="epdf/ftmodule.h"' \
	  -I$(FT)/include -I$(FT) -I$(dir $<) -w -c -o $@ $<
build/host-ft/epdf/%.o: $(FT)/epdf/%.c
	@mkdir -p $(dir $@)
	@$(CC) -O2 -DFT2_BUILD_LIBRARY '-DFT_CONFIG_OPTIONS_H="epdf/ftoption.h"' '-DFT_CONFIG_MODULES_H="epdf/ftmodule.h"' \
	  -I$(FT)/include -I$(FT) -w -c -o $@ $<
# its pictures through libjpeg-turbo (CMYK and progressive JPEGs too: pic.c's PIC_LIBJPEG, EWeb's)
LJ       := common/third_party/libjpeg-turbo
LJ_C     := jaricom jcomapi jdapimin jdapistd jdarith jdcoefct jdcolor jddctmgr jdhuff jdinput jdmainct jdmarker \
            jdmaster jdmerge jdphuff jdpostct jdsample jdtrans jidctfst jidctint jidctred jmemmgr jmemnobs \
            jquant1 jquant2 jutils
LJ_DEFS  := -DPIC_LIBJPEG -DNO_GETENV -DNO_PUTENV -DAERA_NO_FLOAT -I$(LJ)/src -I$(LJ)/config
HLJ_OBJS = $(addprefix build/host-lj/,$(addsuffix .o,$(LJ_C)))
build/host-lj/%.o: $(LJ)/src/%.c
	@mkdir -p $(dir $@)
	@$(CC) -O2 -w $(LJ_DEFS) -c -o $@ $<
CPIC_SRC = src/pdfimg.c src/ccitt.c src/rscale.c common/src/pic.c common/src/png.c common/src/pic_lj8.c \
           common/third_party/nsgif/src/gif.c common/third_party/nsgif/src/lzw.c
build/econvert: tools/econvert.c $(CONV_SRC) src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(CPIC_SRC) $(HFT_OBJS) $(HLJ_OBJS) $(HDRS) src/conv/*.h | build
	$(CC) $(HCFLAGS) -DPDF_DRAW -Isrc/conv -Isrc/view -I$(FT)/include '-DFT_CONFIG_OPTIONS_H="epdf/ftoption.h"' '-DFT_CONFIG_MODULES_H="epdf/ftmodule.h"' -I$(FT) \
	  $(LJ_DEFS) -Icommon/third_party/nsgif/include -Icommon/third_party/nsgif/src \
	  -o $@ tools/econvert.c $(CONV_SRC) src/conv/cvio_host.c src/pdfio_host.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) $(CPIC_SRC) $(HFT_OBJS) $(HLJ_OBJS)

FTTEST_SRC = atari/fttest.c src/pdfio_tos.c $(COMMON_SRC) $(RT_C)
build/m68000/atari/fttest.o build/m68030/atari/fttest.o: XFLAGS += $(FT_DEFS) $(FT_INC)

binary/FTTEST.PRG: XCPU = -m68000 -mstrict-align -fdisable-tree-bswap
binary/FTTEST.PRG: $(addprefix build/m68000/,$(FT_OBJS)) $(addprefix build/m68000/,$(patsubst %.c,%.o,$(FTTEST_SRC))) build/m68000/crt0.o build/m68000/nf.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68000/fttest.elf \
	  build/m68000/crt0.o build/m68000/nf.o $(addprefix build/m68000/,$(patsubst %.c,%.o,$(FTTEST_SRC))) \
	  $(addprefix build/m68000/,$(FT_OBJS)) -lgcc
	python3 common/tools/elf2tos.py build/m68000/fttest.elf $@
binary/FTTEST30.PRG: XCPU = -m68030 -msoft-float
binary/FTTEST30.PRG: $(addprefix build/m68030/,$(FT_OBJS)) $(addprefix build/m68030/,$(patsubst %.c,%.o,$(FTTEST_SRC))) build/m68030/crt0.o build/m68030/nf.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68030/fttest.elf \
	  build/m68030/crt0.o build/m68030/nf.o $(addprefix build/m68030/,$(patsubst %.c,%.o,$(FTTEST_SRC))) \
	  $(addprefix build/m68030/,$(FT_OBJS)) -lgcc
	python3 common/tools/elf2tos.py build/m68030/fttest.elf $@
# the text view's 68000 code on the machine (its assembler halves; make test runs the C):
# tests/hatari/viewtest.py
VT_SRC     = atari/viewtest.c src/view/textview.c $(RT_C)
binary/VIEWTEST.PRG: XCPU = -m68000 -mstrict-align -fdisable-tree-bswap
binary/VIEWTEST.PRG: $(addprefix build/m68000/,$(patsubst %.c,%.o,$(VT_SRC))) build/m68000/crt0.o build/m68000/nf.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68000/viewtest.elf \
	  build/m68000/crt0.o build/m68000/nf.o $(addprefix build/m68000/,$(patsubst %.c,%.o,$(VT_SRC))) -lgcc
	python3 common/tools/elf2tos.py build/m68000/viewtest.elf $@
viewtest: binary/VIEWTEST.PRG
	python3 tests/hatari/viewtest.py

# the picture test: CCITT, the shrinker and common's pic.c (TJpgDec, PNG, GIF)
T         := common/third_party
PIC_SRC    = common/src/pic.c common/src/png.c common/src/jpegdc.c $(T)/tjpgd/tjpgd.c $(T)/nsgif/src/gif.c $(T)/nsgif/src/lzw.c
IMG_SRC    = atari/imgbench.c src/ccitt.c src/rscale.c src/pdfio_tos.c $(PIC_SRC) $(COMMON_SRC) $(RT_C)
IMG_OBJ    = $(patsubst %.c,%.o,$(IMG_SRC))
build/m68000/atari/imgbench.o build/m68030/atari/imgbench.o build/m68000/common/src/pic.o build/m68030/common/src/pic.o \
build/m68000/common/src/png.o build/m68030/common/src/png.o build/m68000/common/src/jpegdc.o build/m68030/common/src/jpegdc.o \
build/m68000/common/third_party/tjpgd/tjpgd.o build/m68030/common/third_party/tjpgd/tjpgd.o \
build/m68000/common/third_party/nsgif/src/gif.o build/m68030/common/third_party/nsgif/src/gif.o \
build/m68000/common/third_party/nsgif/src/lzw.o build/m68030/common/third_party/nsgif/src/lzw.o: \
	XFLAGS += -fwrapv -I$(T)/tjpgd -I$(T)/nsgif/include -I$(T)/nsgif/src -Wno-error
binary/IMGBENCH.PRG: XCPU = -m68000 -mstrict-align -fdisable-tree-bswap
binary/IMGBENCH.PRG: $(addprefix build/m68000/,$(IMG_OBJ)) build/m68000/crt0.o build/m68000/nf.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68000/imgbench.elf \
	  build/m68000/crt0.o build/m68000/nf.o $(addprefix build/m68000/,$(IMG_OBJ)) -lgcc
	python3 common/tools/elf2tos.py build/m68000/imgbench.elf $@
binary/IMGB30.PRG: XCPU = -m68030 -msoft-float
binary/IMGB30.PRG: $(addprefix build/m68030/,$(IMG_OBJ)) build/m68030/crt0.o build/m68030/nf.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68030/imgbench.elf \
	  build/m68030/crt0.o build/m68030/nf.o $(addprefix build/m68030/,$(IMG_OBJ)) -lgcc
	python3 common/tools/elf2tos.py build/m68030/imgbench.elf $@
# the same with libjpeg-turbo and the DSP (EWeb's JPEG path), 68030 only
LJ        := $(T)/libjpeg-turbo
LJ_C      := jaricom jcomapi jdapimin jdapistd jdarith jdcoefct jdcolor jddctmgr jdhuff jdinput jdmainct jdmarker \
             jdmaster jdmerge jdphuff jdpostct jdsample jdtrans jidctfst jidctint jidctred jmemmgr jmemnobs \
             jquant1 jquant2 jutils
LJ_SRC    := $(addprefix $(LJ)/src/,$(addsuffix .c,$(LJ_C)))
IMGLJ_SRC  = atari/imgbench.c src/ccitt.c src/rscale.c src/pdfio_tos.c common/src/pic.c common/src/png.c common/src/pic_lj8.c \
             $(T)/nsgif/src/gif.c $(T)/nsgif/src/lzw.c common/atari/dspjpeg.c $(LJ_SRC) $(COMMON_SRC) $(RT_C)
IMGLJ_OBJ  = $(patsubst %.c,%.o,$(IMGLJ_SRC))
build/m68030lj/%.o: XFLAGS += -DPIC_LIBJPEG -DNO_GETENV -DNO_PUTENV -DAERA_NO_FLOAT -fwrapv -I$(LJ)/src -I$(LJ)/config \
                              -I$(T)/nsgif/include -I$(T)/nsgif/src -Wno-error -w
binary/IMGDSP.PRG: XCPU = -m68030 -msoft-float
binary/IMGDSP.PRG: $(addprefix build/m68030lj/,$(IMGLJ_OBJ)) build/m68030lj/crt0.o build/m68030lj/nf.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68030lj/imgdsp.elf \
	  build/m68030lj/crt0.o build/m68030lj/nf.o $(addprefix build/m68030lj/,$(IMGLJ_OBJ)) -lgcc
	python3 common/tools/elf2tos.py build/m68030lj/imgdsp.elf $@
imgbench: binary/IMGBENCH.PRG binary/IMGB30.PRG binary/IMGDSP.PRG
# the 68030 programs again with the TOS header flags 7: loaded and Malloc'd in TT RAM where there is some
binary/MEAS30F.PRG: build/m68030/measure.elf | binary
	PRGFLAGS=7 python3 common/tools/elf2tos.py $< $@
binary/FTTEST30F.PRG: build/m68030/fttest.elf | binary
	PRGFLAGS=7 python3 common/tools/elf2tos.py $< $@
atari: binary/EPDF.PRG binary/LAUNCH.PRG binary/ECONVERT.PRG binary/render.ovl
measure-atari: binary/MEASURE.PRG binary/MEAS30.PRG binary/MEAS30F.PRG
fttest: binary/FTTEST.PRG binary/FTTEST30.PRG

# ---------------------------------------------------------------- EConvert's converter on a 68030
# (CONVTEST.PRG, tests/hatari/convtest.py; ECONVERT.PRG's core): the reader with
# its drawing hooks, the renderer, FreeType, libjpeg-turbo, all soft-float
CV_ATARI_SRC = $(CONV_SRC) src/conv/cvio_tos.c src/pdfio_tos.c $(PDF_SRC) $(COMMON_SRC) $(BSSL_SRC) \
               src/pdfimg.c src/ccitt.c src/rscale.c common/src/pic.c common/src/png.c common/src/pic_lj8.c \
               $(T)/nsgif/src/gif.c $(T)/nsgif/src/lzw.c common/atari/dspjpeg.c $(LJ_SRC) $(RT_C)
build/m68030cv/%.o: XFLAGS += -DPDF_DRAW -DPIC_LIBJPEG -DNO_GETENV -DNO_PUTENV -DAERA_NO_FLOAT -fwrapv -I$(LJ)/src -I$(LJ)/config \
                              -I$(T)/nsgif/include -I$(T)/nsgif/src -Isrc/conv -Isrc/view $(FT_INC) '-DFT_CONFIG_OPTIONS_H="epdf/ftoption.h"' \
                              '-DFT_CONFIG_MODULES_H="epdf/ftmodule.h"'
build/m68030cv/$(LJ)/%.o build/m68030cv/$(T)/%.o: XFLAGS += -w
build/m68030cv/common/src/inflate.o build/m68030cv/src/pdffilt.o: XFLAGS += -DINFLATE_ASM
CV_ATARI_OBJ = $(addprefix build/m68030cv/,$(patsubst %.c,%.o,$(CV_ATARI_SRC))) build/m68030/inflate_fast.o \
               $(addprefix build/m68030/,$(FT_OBJS))
binary/CONVTEST.PRG: XCPU = -m68030 -msoft-float
binary/CONVTEST.PRG: $(CV_ATARI_OBJ) build/m68030cv/atari/convtest.o build/m68030/crt0.o build/m68030/nf.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68030/convtest.elf \
	  build/m68030/crt0.o build/m68030/nf.o build/m68030cv/atari/convtest.o $(CV_ATARI_OBJ) -lgcc
	PRGFLAGS=7 python3 common/tools/elf2tos.py build/m68030/convtest.elf $@

# ---------------------------------------------------------------- render.ovl (stage 2)
# The page as printed, drawn on a Falcon or TT (src/rovl/rovl.h): the
# renderer and the reader as CONVTEST.PRG has them, built for the 68030,
# loaded by EPDF.PRG with common's ovl.c (an ST never loads it); its heap
# is the program's (libc.c with LIBC_NO_HEAP)
ROVL_OBJ = build/m68030cv/src/rovl/head.o build/m68030cv/src/rovl/pens.o build/m68030ovl/libc.o \
           $(filter-out build/m68030cv/common/atari/libc.o,$(CV_ATARI_OBJ))
build/m68030cv/src/rovl/head.o build/m68030cv/src/rovl/pens.o: XFLAGS += -Isrc/rovl -DROVL_VERSION=\"$(VERSION_STR)\"
build/m68030ovl/libc.o: common/atari/libc.c $(HDRS)
	@mkdir -p $(dir $@)
	$(XCC) $(XFLAGS) -DLIBC_NO_HEAP -c -o $@ $<
build/m68030ovl/%: XCPU = -m68030 -msoft-float
binary/render.ovl: XCPU = -m68030 -msoft-float
binary/render.ovl: $(ROVL_OBJ) common/tlsovl/ovl.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/tlsovl/ovl.ld -Wl,-e,rovl \
	  -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments \
	  -o build/m68030ovl/render.elf $(ROVL_OBJ) -lgcc
	python3 common/tools/elf2tos.py build/m68030ovl/render.elf $@

# ROVLTEST.PRG (tests/hatari/rovl.py): render.ovl loaded and its pages drawn
# as EPDF.PRG would (rdraw.c, the 68000 build), without EPDF's window
ROVLT_OBJ = $(addprefix build/m68000/,atari/rovltest.o src/gem/rdraw.o common/src/ovl.o src/pdfio_tos.o \
              $(patsubst %.c,%.o,$(RT_C)))
build/m68000/atari/rovltest.o build/m68000/src/gem/rdraw.o: XFLAGS += -Isrc/rovl -Isrc/gem
binary/ROVLTEST.PRG: XCPU = -m68000 -mstrict-align -fdisable-tree-bswap
binary/ROVLTEST.PRG: $(ROVLT_OBJ) build/m68000/crt0.o build/m68000/nf.o common/atari/link.ld | binary
	$(XCC) $(XLDFLAGS) -o build/m68000/rovltest.elf build/m68000/crt0.o build/m68000/nf.o $(ROVLT_OBJ) -lgcc
	python3 common/tools/elf2tos.py build/m68000/rovltest.elf $@

# ---------------------------------------------------------------- ECONVERT.PRG (the converter's window)
# One program for every machine (Erez, 7 Nov 2026: Gateway mode on an ST
# too): its window, files and the Gateway's exchange built for the 68000
# (build/m68000ec), the converter for the 68030 (build/m68030cv) as before,
# so a Falcon's own conversions lose nothing (a 68000 converter: the test
# book 29.7 -> 32.1 s on Falcon-4). The converter runs only where the
# window has found a 68030 (Convert on: This Atari); an ST only sends.
EC_GEM_SRC = src/econv/econvert.c src/gem/gemui.c src/view/inf.c common/atari/gem.c common/atari/crash.c \
             common/ui/mbar.c common/ui/aboutbox.c common/ui/aboutline.c common/ui/alert.c common/ui/dialog.c \
             common/ui/field.c common/ui/av.c common/ui/text.c common/ui/help.c common/ui/dd.c common/ui/progbar.c common/ui/popup.c common/ui/panebar.c common/ui/iconify.c common/ui/settings.c common/ui/dlgbuild.c common/ui/tooltip.c common/ui/offdraw.c \
             common/ui/click.c common/ui/clip.c common/ui/accmem.c common/src/bidi.c common/ui/fsel.c common/ui/scrsize.c \
             src/pdfio_tos.c src/conv/cvio_tos.c common/src/util.c common/src/futil.c $(RT_C) \
             $(BSSL_SRC) src/pigate.c src/pftime_tos.c common/src/conn.c common/src/tls_none.c src/gem/gatedlg.c \
             common/atari/net_tos.c common/atari/sting.c
EC_CV_SRC  = $(filter-out src/conv/cvio_tos.c src/pdfio_tos.c common/src/util.c common/src/futil.c \
               $(RT_C) $(BSSL_SRC),$(CV_ATARI_SRC))
build/m68000ec/%.o: XFLAGS += -DDLG_CLICK -DPDF_DRAW -DAERA_NO_FLOAT -Isrc/gem -Isrc/view -Icommon/ui -Isrc/conv
build/m68000ec/version.o: build/m68000/version.c | build/m68000ec
	$(XCC) $(XFLAGS) -c -o $@ $<
build/m68000ec/coro.o: src/gem/coro.S | build/m68000ec
	$(XCC) -m68000 -c -o $@ $<
build/m68000ec/crash_s.o: common/atari/crash.S | build/m68000ec
	$(XCC) -m68000 -c -o $@ $<
build/m68000ec/sting_s.o: common/atari/sting.S | build/m68000ec
	$(XCC) -m68000 -c -o $@ $<
build/m68000ec/crt0_ec.o: common/atari/crt0.S | build/m68000ec
	$(XCC) -m68000 -DSTACK_SIZE=16384 -c -o $@ $<
EC_OBJS = $(patsubst %.c,build/m68000ec/%.o,$(EC_GEM_SRC)) build/m68000ec/version.o build/m68000ec/sting_s.o \
          $(addprefix build/m68030cv/,$(patsubst %.c,%.o,$(EC_CV_SRC))) build/m68030/inflate_fast.o \
          $(addprefix build/m68030/,$(FT_OBJS))
binary/ECONVERT.PRG: XCPU = -m68000
binary/ECONVERT.PRG: $(EC_OBJS) build/m68000ec/crt0_ec.o build/m68000/nf.o build/m68000ec/crash_s.o build/m68000ec/coro.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68030/econvert.elf \
	  build/m68000ec/crt0_ec.o build/m68000/nf.o build/m68000ec/crash_s.o build/m68000ec/coro.o $(EC_OBJS) -lgcc
	PRGFLAGS=7 python3 common/tools/elf2tos.py build/m68030/econvert.elf $@

# ---------------------------------------------------------------- EPDF.PRG (the GEM program)
# one build for every machine (68000); the text view of the pages
build/m68000/version.c: FORCE | build/m68000
	@printf '/* written by the Makefile: do not edit */\nconst char app_version[] = "%s";\n' "$(VERSION_STR)" > $@.new
	@if cmp -s $@.new $@; then rm $@.new; else mv $@.new $@; fi
FORCE:
.PHONY: FORCE
GEM_SRC = src/gem/epdf.c src/gem/doc.c src/gem/rdraw.c common/src/ovl.c src/gem/textwin.c src/gem/badge.c src/gem/find.c src/gem/font.c src/gem/pics.c src/gem/gemui.c src/gem/menu.c src/gem/about.c \
          src/view/textview.c src/view/docview.c src/view/tstore.c src/view/inf.c src/view/search.c common/ui/bgwork.c common/ui/twin.c \
          src/pdfimg.c src/ccitt.c src/rscale.c $(PIC_SRC) \
          common/atari/gem.c common/atari/crash.c common/ui/mbar.c common/ui/aboutbox.c common/ui/aboutline.c \
          common/ui/alert.c common/ui/dialog.c common/ui/field.c common/ui/av.c common/ui/text.c common/ui/clip.c common/ui/iconify.c common/ui/help.c common/ui/dd.c common/ui/popup.c common/ui/fontsel.c common/ui/click.c common/ui/progbar.c common/ui/enlarge.c \
          common/src/charset.c common/src/bidi.c common/ui/accmem.c common/atari/bigbuf.c common/atari/dspjpeg.c \
          src/gem/gwopen.c src/gem/gatedlg.c src/pigate.c src/pftime_tos.c src/conv/cvio_tos.c common/src/conn.c \
          common/src/tls_none.c common/atari/net_tos.c common/atari/sting.c common/ui/scrsize.c \
          src/gem/trview.c src/view/trpara.c common/src/translate.c common/ui/langs.c common/ui/panebar.c common/ui/settings.c common/ui/dlgbuild.c
GEM_OBJ = $(patsubst %.c,build/m68000/%.o,$(GEM_SRC)) build/m68000/version.o build/m68000/sting_s.o
build/m68000/sting_s.o: common/atari/sting.S | build/m68000
	$(XCC) -m68000 -c -o $@ $<
$(patsubst %.c,build/m68000/%.o,$(GEM_SRC)): XFLAGS += -DDLG_CLICK -Isrc/gem -Isrc/rovl -Isrc/view -Icommon/ui -I$(T)/tjpgd -I$(T)/nsgif/include -I$(T)/nsgif/src
# the Falcon's DSP for JPEGs, from pic.c's own header reader (one 68000
# program for every Atari, guideline 7.4); no PNG or GIF decoder (a PDF's
# pictures are JPEG or its own streams: 20 KB of the resident size, the
# room NVDI's menus want under four accessories, 2 Nov 2026)
build/m68000/common/src/pic.o: XFLAGS += -DPIC_TJPGD_DSP -DPIC_NO_GIF -DPIC_NO_PNG
# dialogs drawn partly by the CPU on a 68000 (EPDF.PRG runs on an ST; the
# profiled build links the same object)
build/m68000/common/ui/dialog.o: XFLAGS += -DDLG_CPU
build/m68000/version.o: build/m68000/version.c
	$(XCC) $(XFLAGS) -c -o $@ $<
# EPDF's own start: a 16 KB main stack (the reading and the pictures have
# stacks of their own; GEM work alone needs a few KB: EHypView found 3.2 KB
# at its deepest), not the measuring programs' 48 KB
build/m68000/crt0_epdf.o: common/atari/crt0.S | build/m68000
	$(XCC) -m68000 -DSTACK_SIZE=16384 -c -o $@ $<
build/m68000/coro.o: src/gem/coro.S | build/m68000
	$(XCC) -m68000 -c -o $@ $<
build/m68000/crash_s.o: common/atari/crash.S | build/m68000
	$(XCC) $(XCPU) -c -o $@ $<
binary/EPDF.PRG: XCPU = -m68000 -mstrict-align -fdisable-tree-bswap
EPDF_OBJS = $(GEM_OBJ) build/m68000/inflate_fast.o $(addprefix build/m68000/,$(patsubst %.c,%.o,$(RT_C))) $(addprefix build/m68000/,$(patsubst %.c,%.o,$(PDF_SRC) src/pdfio_tos.c $(COMMON_SRC))) $(addprefix build/m68000/,$(patsubst %.c,%.o,$(BSSL_SRC)))
binary/EPDF.PRG: $(EPDF_OBJS) build/m68000/crt0_epdf.o build/m68000/nf.o build/m68000/crash_s.o build/m68000/coro.o common/atari/link.ld | binary
	$(XCC) $(XCPU) -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68000/epdf.elf \
	  build/m68000/crt0_epdf.o build/m68000/nf.o build/m68000/crash_s.o build/m68000/coro.o $(filter-out %/crt0_epdf.o %/nf.o %/crash_s.o %/coro.o %/link.ld,$(filter build/%,$^)) -lgcc
	PRGFLAGS=7 python3 common/tools/elf2tos.py build/m68000/epdf.elf $@

# starts a program with a command line, for the rigs (atari/launch.c)
binary/LAUNCH.PRG: build/m68000/atari/launch.o build/m68000/common/atari/gem.o build/m68000/common/atari/tos.o build/m68000/common/atari/libc.o build/m68000/crt0.o build/m68000/nf.o common/atari/link.ld | binary
	$(XCC) -m68000 -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/m68000/launch.elf \
	  build/m68000/crt0.o build/m68000/nf.o build/m68000/atari/launch.o build/m68000/common/atari/gem.o build/m68000/common/atari/tos.o build/m68000/common/atari/libc.o -lgcc
	python3 common/tools/elf2tos.py build/m68000/launch.elf $@

# ---------------------------------------------------------------- the icons (guidelines 5.5)
# drawn in AERA-Atari (common/icons/EPDF.txt, EPDFDOC.txt for the documents); made into
# icons/ (committed), src/gem/icon*.h and docs/icon*.png by common/tools/appicon.py
icons: common/icons/EPDF.txt common/icons/EPDFDOC.txt common/tools/appicon.py
	mkdir -p icons docs
	python3 common/tools/appicon.py EPDF --rsc icons/EPDF.RSC --rscnv icons/EPDFNV.RSC \
		--icn icons/EPDF.ICN --mask icons/EPDFMK.ICN --headers src/gem --docs docs
	python3 common/tools/appicon.py EPDFDOC --rsc icons/EPDFDOC.RSC --rscnv icons/EPDFDNV.RSC --label PDF
	python3 common/tools/appicon.py $(EC_ICON) --rsc icons/ECONVERT.RSC --rscnv icons/ECONVNV.RSC \
		--icn icons/ECONVERT.ICN --mask icons/ECONVMK.ICN --headers src/econv --label ECONVERT
.PHONY: icons
# EConvert's own icon files (icons/ECONV*, src/econv/icon*.h): drawn from
# common/icons/ECONVERT.txt once it is in AERA-Atari, EPDF's drawing until then
EC_ICON := $(if $(wildcard common/icons/ECONVERT.txt),ECONVERT,EPDF)

# ---------------------------------------------------------------- the guide and the release
# EPDF.HYP from docs/GUIDE.md with the family's front page (docs/front.json,
# common/tools/helpfront.py), and EPDF.REF, its pages' names for ST-Guide's index
HCP ?= hcp
help: binary/EPDF.HYP
binary/EPDF.HYP: docs/GUIDE.md tools/mkhelp.py VERSION docs/front.json docs/help/WINDOW.IMG \
                 common/help/EPDF.IMG common/tools/helpfront.py common/tools/gemimg.py | binary
	mkdir -p build/help
	python3 tools/mkhelp.py docs/GUIDE.md VERSION build/help/EPDF.STG
	python3 common/tools/helpfront.py page docs/front.json build/help/EPDF.STG build/help/EPDF.STG
	$(HCP) -q -o $@ build/help/EPDF.STG >/dev/null
	test -s binary/EPDF.REF

# what a user needs, by name: the program, its guide, the licences and the icons.
# Every file a release carries, and every folder in its zip, is named in
# lowercase (guidelines 6.6: on MiNT's ext2 EPDF.INF and epdf.inf are two files)
DIST  := binary/EPDF.PRG binary/render.ovl binary/EPDF.HYP binary/EPDF.REF NOTICES.TXT
ICONS := icons/EPDF.RSC icons/EPDFNV.RSC icons/EPDF.ICN icons/EPDFMK.ICN icons/EPDFDOC.RSC icons/EPDFDNV.RSC \
         icons/ECONVERT.RSC icons/ECONVNV.RSC icons/ECONVERT.ICN icons/ECONVMK.ICN
lc = $(shell echo '$(notdir $(1))' | tr A-Z a-z)
# EConvert (a 68030's): its program and the standard fonts it draws with
# (third_party/liberation, the OFL's text beside them), in the zip's econvert folder
EC_DIST    := binary/ECONVERT.PRG
EC_FONTS   := $(wildcard third_party/liberation/fonts/*.ttf) third_party/liberation/OFL.txt
dist: binary/EPDF.PRG binary/render.ovl binary/ECONVERT.PRG help
	rm -rf dist
	mkdir -p dist/zip/icons dist/zip/econvert/fonts
	$(foreach f,$(DIST),cp $(f) dist/$(call lc,$(f)) && cp $(f) dist/zip/$(call lc,$(f)) &&) true
	$(foreach f,$(ICONS),cp $(f) dist/$(call lc,$(f)) && cp $(f) dist/zip/icons/$(call lc,$(f)) &&) true
	$(foreach f,$(EC_DIST),cp $(f) dist/$(call lc,$(f)) && cp $(f) dist/zip/econvert/$(call lc,$(f)) &&) true
	$(foreach f,$(EC_FONTS),cp $(f) dist/$(call lc,$(f)) && cp $(f) dist/zip/econvert/fonts/$(call lc,$(f)) &&) true
	cd dist/zip && zip -q -9 -r ../epdf-$(VERSION_STR).zip .
	rm -rf dist/zip
	@up=$$(find dist -name '*[A-Z]*'; unzip -Z1 dist/epdf-$(VERSION_STR).zip | grep '[A-Z]'); \
	  test -z "$$up" || { echo "dist: not lowercase: $$up"; exit 1; }

gui-test: binary/EPDF.PRG binary/LAUNCH.PRG
	python3 tests/hatari/gui.py ST:mono build/gui
	python3 tests/hatari/fontdlg.py build/fontdlg ST:mono

# each file's opens and lookups from Hatari's GEMDOS trace, a 40-page book on
# a 1 MB ST (the text goes to EPDF.TXC), against the job's budget (6.5)
files-test: binary/EPDF.PRG binary/LAUNCH.PRG
	python3 tests/hatari/files.py build/files ST-1

# the Gateway's exchange (src/pigate.c) against EPDF's stand-in for it
# (tests/pigate/fakegate.py) and this build's converter: byte for byte the
# direct one, a stop, a refusal (docs/PI-GATEWAY.md): against pdfgate.py from
# the read-only gateway/ copy (tools/sync-gateway.sh), which EPDF tests at every
# change; PDFGATE=FOLDER another one, PDFGATE=none EPDF's stand-in
pi-test: build/econvert
	PDFGATE=$(PDFGATE) tests/pigate/run.sh $(PDF)
.PHONY: pi-test
rovl-test: build/econvert binary/ROVLTEST.PRG binary/render.ovl
	python3 tests/hatari/rovl.py build/rovl-test
.PHONY: rovl-test

# docs/screenshot.png and docs/help/WINDOW.IMG again, from Hatari (commit them)
shots: binary/EPDF.PRG binary/LAUNCH.PRG
	python3 tests/hatari/shots.py build/shots
.PHONY: shots

# the speed rig (the guidelines' Speed): EPDF.PRG with common/atari/gem.c built
# -DAES_PROFILE=20, each stretch of 20 ms or more between event calls on the
# emulator's console (AESGAP); tests/hatari/speed.py checks them on the stock
# machines. EPDF's own code as it is released: textwin.c's timing marks
# (EPDF_TIMES, a line logged through NatFeats at each) cost a page 25 ms on an
# ST, so they are in build/times only, for breaking a stretch down
AES_LINK = $(XCC) -m68000 -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments
build/aprof/gem.o: common/atari/gem.c $(HDRS) | build
	mkdir -p build/aprof
	$(XCC) $(XFLAGS) -m68000 -mstrict-align -fdisable-tree-bswap -DAES_PROFILE=20 -c -o $@ $<
build/aprof/EPDF.PRG: binary/EPDF.PRG build/aprof/gem.o
	$(AES_LINK) -o build/aprof/epdf.elf \
	  build/m68000/crt0_epdf.o build/m68000/nf.o build/m68000/crash_s.o build/m68000/coro.o \
	  $(filter-out build/m68000/common/atari/gem.o,$(filter build/%.o,$(EPDF_OBJS))) \
	  build/aprof/gem.o -lgcc
	PRGFLAGS=7 python3 common/tools/elf2tos.py build/aprof/epdf.elf $@
# the same with textwin.c's marks ("EPDF t copy 35", "t rows 15"...), for a breakdown
build/times/textwin.o: src/gem/textwin.c $(HDRS) | build
	mkdir -p build/times
	$(XCC) $(XFLAGS) -m68000 -mstrict-align -fdisable-tree-bswap -Isrc/gem -Isrc/rovl -Isrc/view -Icommon/ui -DEPDF_TIMES -c -o $@ $<
build/times/EPDF.PRG: binary/EPDF.PRG build/aprof/gem.o build/times/textwin.o
	$(AES_LINK) -o build/times/epdf.elf \
	  build/m68000/crt0_epdf.o build/m68000/nf.o build/m68000/crash_s.o build/m68000/coro.o \
	  $(filter-out build/m68000/common/atari/gem.o build/m68000/src/gem/textwin.o,$(filter build/%.o,$(EPDF_OBJS))) \
	  build/aprof/gem.o build/times/textwin.o -lgcc
	PRGFLAGS=7 python3 common/tools/elf2tos.py build/times/epdf.elf $@
speed-test: build/aprof/EPDF.PRG binary/LAUNCH.PRG
	python3 tests/hatari/speed.py build/speed
.PHONY: speed-test
# EConvert's the same way (a 68030's: tests/hatari/econv_speed.py, Falcon-4)
build/aprof/gem00ec.o: common/atari/gem.c $(HDRS) | build
	mkdir -p build/aprof
	$(XCC) $(XFLAGS) -m68000 -DDLG_CLICK -DAERA_NO_FLOAT -DAES_PROFILE=20 -c -o $@ $<
build/aprof/ECONVERT.PRG: binary/ECONVERT.PRG build/aprof/gem00ec.o
	$(XCC) -m68000 -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/aprof/econvert.elf \
	  build/m68000ec/crt0_ec.o build/m68000/nf.o build/m68000ec/crash_s.o build/m68000ec/coro.o \
	  $(filter-out build/m68000ec/common/atari/gem.o,$(EC_OBJS)) build/aprof/gem00ec.o -lgcc
	PRGFLAGS=7 python3 common/tools/elf2tos.py build/aprof/econvert.elf $@
econv-speed-test: build/aprof/ECONVERT.PRG binary/LAUNCH.PRG
	python3 tests/hatari/econv_speed.py build/econv-speed
.PHONY: econv-speed-test

# the stacks' use (EPDF.PRG with doc.c built -DEPDF_STACKS): "EPDF stacks ..."
# every 25 pages and at the end, for sizing READ_STACK and PIC_STACK
build/stk/doc.o: src/gem/doc.c $(HDRS) | build
	mkdir -p build/stk
	$(XCC) $(XFLAGS) -m68000 -mstrict-align -fdisable-tree-bswap -Isrc/gem -Isrc/rovl -Isrc/view -Icommon/ui -I$(T)/tjpgd \
	  -DEPDF_STACKS -c -o $@ $<
build/stk/EPDF.PRG: binary/EPDF.PRG build/stk/doc.o
	$(XCC) -m68000 -nostdlib -static -no-pie -Wl,--emit-relocs -Wl,-T,common/atari/link.ld -Wl,--gc-sections \
	  -Wl,--build-id=none -Wl,-z,noexecstack -Wl,--no-warn-rwx-segments -o build/stk/epdf.elf \
	  build/m68000/crt0_epdf.o build/m68000/nf.o build/m68000/crash_s.o build/m68000/coro.o \
	  $(filter-out build/m68000/src/gem/doc.o,$(filter build/%.o,$(EPDF_OBJS))) build/stk/doc.o -lgcc
	PRGFLAGS=7 python3 common/tools/elf2tos.py build/stk/epdf.elf $@

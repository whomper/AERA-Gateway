/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * Padding for the renderer's FreeType, which has no auto-hinter
 * (Makefile's FT_CV_OBJS): linked where the auto-hinter was (and after
 * ftinit, whose module list is a pointer shorter), so the code and the
 * tables after it keep their places in the 68030's caches (addresses mod
 * 256, tools/rpad.py). Kept by the link's -u; never run or read.
 * FT_PAD_NAME names each copy; FT_PAD_TEXT, _RO, _STR and _DATA are its
 * bytes of code, of tables, in the merged strings and of data.
 */
#define CAT2(a, b) a##b
#define CAT(a, b) CAT2(a, b)

	.section .text.FT_PAD_NAME,"ax"
	.globl	FT_PAD_NAME
FT_PAD_NAME:
#if FT_PAD_TEXT > 0
	.skip	FT_PAD_TEXT
#endif

	.section .rodata.FT_PAD_NAME,"a"
	.globl	CAT(FT_PAD_NAME, _ro)
CAT(FT_PAD_NAME, _ro):
#if FT_PAD_RO > 0
	.skip	FT_PAD_RO
#endif

#if FT_PAD_DATA > 0
	/* (the code is shorter by a multiple of 256 less this: the data, the
	   tables in the bss and the blocks after them at their places too) */
	.section .data.FT_PAD_NAME,"aw"
	.globl	CAT(FT_PAD_NAME, _data)
CAT(FT_PAD_NAME, _data):
	.skip	FT_PAD_DATA
#endif

#if FT_PAD_STR > 0
	/* (the auto-hinter's strings left the merged strings: as many again,
	   mod 256, so the tables after them keep their places) */
	.section .rodata.str1.1,"aMS",@progbits,1
	.globl	CAT(FT_PAD_NAME, _str)
CAT(FT_PAD_NAME, _str):
	.rept	FT_PAD_STR
	.byte	0x7f
	.endr
	.byte	0
#endif

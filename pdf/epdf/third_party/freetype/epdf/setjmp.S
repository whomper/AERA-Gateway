/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
| setjmp / longjmp for the 68000 and up: d2-d7 and a2-a6 are the registers a C
| function keeps for its caller, then the stack pointer and the return address.
	.text
	.globl	setjmp
setjmp:
	move.l	4(%sp),%a0		| env
	movem.l	%d2-%d7/%a2-%a6,(%a0)	| 11 registers
	lea	44(%a0),%a1
	move.l	%sp,(%a1)+		| sp as the caller will have it
	move.l	(%sp),(%a1)		| return address
	moveq	#0,%d0
	rts

	.globl	longjmp
longjmp:
	move.l	4(%sp),%a0		| env
	move.l	8(%sp),%d0		| val
	bne.s	1f
	moveq	#1,%d0
1:	movem.l	(%a0),%d2-%d7/%a2-%a6
	lea	44(%a0),%a1
	move.l	(%a1)+,%sp
	move.l	(%a1),(%sp)		| the return address, where setjmp's caller will find it
	rts

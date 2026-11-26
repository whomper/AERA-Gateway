/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
| coro.S - the switch between EPDF's main stack and the reading's own
| (coro.h), for the 68000 and up. d2-d7 and a2-a6 are the registers a C
| function keeps for its caller: they go on the stack being left, its
| stack pointer into *from, and the other stack's come back from *to.
	.text
	.globl	coro_switch
coro_switch:
	move.l	4(%sp),%a0		| from
	move.l	8(%sp),%a1		| to
#ifdef __mcoldfire__
| the FireBee's edition: a ColdFire's movem has no -(sp) or (sp)+, and
| with its FPU fp2-fp7 are kept for the caller too (coro.h's CORO_WORDS)
	lea	-44(%sp),%sp
	movem.l	%d2-%d7/%a2-%a6,(%sp)
#ifdef __mcffpu__
	lea	-48(%sp),%sp
	fmovem.d %fp2-%fp7,(%sp)
#endif
	move.l	%sp,(%a0)
	move.l	(%a1),%sp
#ifdef __mcffpu__
	fmovem.d (%sp),%fp2-%fp7
	lea	48(%sp),%sp
#endif
	movem.l	(%sp),%d2-%d7/%a2-%a6
	lea	44(%sp),%sp
	rts
#else
	movem.l	%d2-%d7/%a2-%a6,-(%sp)	| 11 registers, 44 bytes
	move.l	%sp,(%a0)
	move.l	(%a1),%sp
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts
#endif

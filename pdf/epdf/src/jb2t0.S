/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
| jb2t0.S - JBIG2's generic region, template 0 with its AT pixels in place
| (jbig2.c's generic_t0), a row's whole bytes, for the 68000 and up: the
| MQ coder (mq.h's MQ_DECODE) and the context's sliding (T.88's bit order:
| this row's x-4 .. x-1 in bits 3-0, the row above's x-3 .. x+3 in 10-4,
| the one above that's x-2 .. x+2 in 15-11) with every register in one.
|
| unsigned jb2_t0_bytes(MQ *m, uint8_t *cx, uint8_t *out, const uint8_t *r1,
|                       const uint8_t *r2, long n, unsigned ctx)
|
| n bytes of out from the row's start: their pixels decoded with the
| contexts cx (65536 bytes), r1 and r2 the rows above (each read to its
| byte n, the one after the last decoded: a row of zeros where there is
| none); the context in and out. m's registers are taken and given back.
|
|   d0 ctx   d1 C   d2 A (16 bits)   d3 CT   d4 the rows above's bits
|   (r2's pair in the high word, r1's in the low)   d5 the byte so far
|   (from 1: its top bit says it is done)   d6 d7 the decision's own
|   a0 cx   a1 mq_tab   a2 m   a3 its next byte   a4 out   a5 r1   a6 r2
	.text
	.globl	jb2_t0_bytes
jb2_t0_bytes:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)	| 44 bytes: the arguments from 48(%sp)
	move.l	48(%sp),%a2
	move.l	52(%sp),%a0
	move.l	56(%sp),%a4
	move.l	60(%sp),%a5
	move.l	64(%sp),%a6
	move.l	72(%sp),%d0
	move.l	8(%a2),%d1
	move.l	12(%a2),%d2
	move.l	16(%a2),%d3
	move.l	(%a2),%a3
	lea	mq_tab,%a1
	tst.l	68(%sp)
	beq	.Ldone
.Lbyte:
	moveq	#0,%d4
	move.b	(%a6)+,%d4		| r2's pair: the high word
	lsl.w	#8,%d4
	move.b	(%a6),%d4
	swap	%d4
	move.b	(%a5)+,%d4		| r1's: the low
	lsl.w	#8,%d4
	move.b	(%a5),%d4
	moveq	#1,%d5
.Lpix:
	moveq	#0,%d6
	move.b	0(%a0,%d0.l),%d6	| the context's state: index * 2 + MPS
	move.w	%d6,%d7
	and.w	#0xFE,%d7
	add.w	%d7,%d7
	move.l	0(%a1,%d7.w),%d7	| Qe << 16 | NMPS << 9 | NLPS << 1 | SWITCH
	swap	%d7			| Qe
	sub.w	%d7,%d2			| A -= Qe
	swap	%d1			| C's high half
	cmp.w	%d7,%d1
	bcs.s	.Llps
	sub.w	%d7,%d1			| C -= Qe << 16
	swap	%d1
	tst.w	%d2
	bmi.s	.Lmps			| A still at 0x8000 or more: the MPS, no renorm
	cmp.w	%d7,%d2
	bcs.s	.Lxlps
	bra.s	.Lxmps
.Llps:
	swap	%d1
	cmp.w	%d7,%d2
	bcs.s	.Llmps
	move.w	%d7,%d2			| A = Qe
	bra.s	.Lxlps
.Llmps:
	move.w	%d7,%d2
.Lxmps:					| the MPS: the state its NMPS
	swap	%d7
	lsr.w	#8,%d7			| NMPS * 2
	and.w	#1,%d6			| the MPS: the decision
	or.b	%d6,%d7
	move.b	%d7,0(%a0,%d0.l)
	bra.s	.Lrenorm
.Lxlps:					| the LPS: the state its NLPS, the MPS switched if so
	swap	%d7
	and.w	#1,%d6
	eor.b	%d6,%d7			| NLPS * 2 | (SWITCH ^ MPS)
	move.b	%d7,0(%a0,%d0.l)
	eor.b	#1,%d6			| the decision: not the MPS
.Lrenorm:
	tst.w	%d3
	bne.s	.Lrn1
	bsr	.Lbytein
.Lrn1:
	add.w	%d2,%d2
	add.l	%d1,%d1
	subq.w	#1,%d3
	tst.w	%d2
	bpl.s	.Lrenorm
	bra.s	.Lout
.Lmps:
	and.w	#1,%d6
.Lout:
	add.w	%d5,%d5			| the byte
	or.w	%d6,%d5
	add.w	%d0,%d0			| the context along
	and.w	#0xF7EE,%d0
	or.w	%d6,%d0
	move.l	%d4,%d7
	lsr.l	#7,%d7
	and.w	#0x10,%d7		| the row above's x + 4
	or.w	%d7,%d0
	move.l	%d4,%d7
	swap	%d7
	lsr.w	#1,%d7
	and.w	#0x800,%d7		| the one above that's x + 3
	or.w	%d7,%d0
	add.l	%d4,%d4
	cmp.w	#256,%d5
	bcs	.Lpix
	move.b	%d5,(%a4)+
	subq.l	#1,68(%sp)
	bne	.Lbyte
.Ldone:
	move.l	%d1,8(%a2)
	and.l	#0xFFFF,%d2
	move.l	%d2,12(%a2)
	move.l	%d3,16(%a2)
	move.l	%a3,(%a2)
	and.l	#0xFFFF,%d0
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

| mq.c's mq_byte_in on the registers: C in d1, CT in d3, the next byte in
| a3, the data's end at 4(a2)
.Lbytein:
	movem.l	%d6/%a1,-(%sp)
	move.l	4(%a2),%a1
	cmp.l	%a1,%a3
	bcc.s	.Lbieof			| past the end: 0xFF, a marker's
	cmp.b	#0xFF,(%a3)
	bne.s	.Lbinorm
	addq.l	#1,%a3			| 0xFF: its next byte
	cmp.l	%a1,%a3
	bcc.s	.Lbimark
	moveq	#0,%d6
	move.b	(%a3),%d6
	cmp.w	#0x8F,%d6
	bhi.s	.Lbimark
	lsl.l	#8,%d6			| C += B << 9, CT = 7
	add.l	%d6,%d6
	add.l	%d6,%d1
	moveq	#7,%d3
	bra.s	.Lbiret
.Lbimark:
	subq.l	#1,%a3			| a marker: 1 bits from here, the byte not taken
.Lbieof:
	add.l	#0xFF00,%d1
	moveq	#8,%d3
	bra.s	.Lbiret
.Lbinorm:
	addq.l	#1,%a3			| C += B << 8, CT = 8
	move.l	#0xFF,%d6
	cmp.l	%a1,%a3
	bcc.s	.Lbin2
	moveq	#0,%d6
	move.b	(%a3),%d6
.Lbin2:
	lsl.l	#8,%d6
	add.l	%d6,%d1
	moveq	#8,%d3
.Lbiret:
	movem.l	(%sp)+,%d6/%a1
	rts

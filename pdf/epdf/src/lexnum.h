/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * lexnum.h - a PDF number read fast, for the content interpreter (and
 * lex_number): inline, so the interpreter calls nothing for each of a
 * map's hundreds of thousands of numbers (a call saved and restored nine
 * registers: some 350 cycles a number on a 68000, 1 Nov 2026).
 */
#ifndef LEXNUM_H
#define LEXNUM_H

#include <stdint.h>
#include "pdf.h"

/*
 * fp / 10^fd as 16.16, rounded (fd 1 to 5, fp < 10^fd): one divu.w on a
 * 68000, where the 32-bit division was a library call of some 600 cycles
 * for every real number (udivmod, 7 % of a map's page on an ST, 1 Nov
 * 2026). Five places: both halved, 50000 still fits a word.
 */
static inline uint32_t frac_fx(uint32_t fp, int fd)
{
	static const unsigned short scale[6] = { 1, 10, 100, 1000, 10000, 50000 };
	uint32_t n = fd < 5 ? (fp << 16) + scale[fd] / 2 : (fp << 15) + 25000;
	unsigned short dv = scale[fd];

#if defined(__mc68000__) || defined(__m68k__)
	__asm__("divu.w %1,%0" : "+d"(n) : "d"(dv) : "cc");
	return n & 0xFFFF;
#else
	return n / dv;
#endif
}

/*
 * Up to n digits from s (n from 1 to 32767) added to *v (v * 10 + digit
 * each): where the digits stop. A dbra loop of some 86 cycles a digit on
 * a 68000 (the C, with its long compares, was some 140).
 */
static inline const unsigned char *lex_digits(const unsigned char *s, short n, uint32_t *v)
{
#if defined(__mc68000__) || defined(__m68k__)
	uint32_t a = *v;

	__asm__("moveq #0,%%d0\n\t"
		"moveq #48,%%d3\n\t"
		"moveq #9,%%d4\n\t"
		"subq.w #1,%1\n"
		"1:\tmove.b (%0)+,%%d0\n\t"
		"sub.b %%d3,%%d0\n\t"
		"cmp.b %%d4,%%d0\n\t"
		"bhi.s 2f\n\t"
		"add.l %2,%2\n\t"
		"move.l %2,%%d2\n\t"
		"add.l %2,%2\n\t"
		"add.l %2,%2\n\t"
		"add.l %%d2,%2\n\t"
		"add.l %%d0,%2\n\t"
		"dbra %1,1b\n\t"
		"bra.s 3f\n"
		"2:\tsubq.l #1,%0\n"
		"3:"
		: "+a"(s), "+d"(n), "+d"(a)
		:
		: "d0", "d2", "d3", "d4", "cc");
	*v = a;
	return s;
#else
	uint32_t a = *v;

	while (n-- > 0 && (unsigned char)(*s - '0') <= 9)
		a = a * 10 + (uint32_t)(*s++ - '0');
	*v = a;
	return s;
#endif
}

/*
 * The number token at s (up to e) in one pass: what lex_number gives for
 * the run of 0-9 . + - there, and where that run ends; *ok 0 when the
 * run isn't a number. (The content's numbers were read twice, the run
 * found and then parsed.) More than 9 digits before the point, rare,
 * go the long way, as lex_number counts them.
 */
static inline const unsigned char *lex_number_at(const unsigned char *s, const unsigned char *e, fx *fxv, int32_t *iv,
						 int *isreal, int *ok)
{
	const unsigned char *t, *d;
	int neg = 0, digits = 0, fd = 0, real = 0;
	uint32_t ip = 0, fp = 0;
	long room;

	if (s < e && (*s == '+' || *s == '-')) {
		neg = *s == '-';
		s++;
	}
	while (s < e && *s == '-')
		s++;
	room = e - s;
	if (room > 0) {
		d = lex_digits(s, (short)(room > 9 ? 9 : room), &ip);
		digits = (int)(d - s);
		s = d;
		while (s < e && (unsigned char)(*s - '0') <= 9) {	/* past 9 digits */
			ip = ip < 429496729u ? ip * 10 + (uint32_t)(*s - '0') : 0xFFFFFFFFu;
			s++;
			digits++;
		}
	}
	if (s < e && *s == '.') {
		real = 1;
		s++;
		room = e - s;
		if (room > 0) {
			d = lex_digits(s, (short)(room > 5 ? 5 : room), &fp);
			fd = (int)(d - s);
			digits += fd;
			s = d;
			while (s < e && (unsigned char)(*s - '0') <= 9) {	/* past 5 places: not kept */
				s++;
				digits++;
			}
		}
	}
	/* the rest of the run: anything there makes it no number */
	for (t = s; t < e && ((unsigned char)(*t - '0') <= 9 || *t == '.' || *t == '-' || *t == '+'); t++)
		;
	*ok = digits && t == s;
	if (*ok) {
		uint32_t v;

		*isreal = real;
		*iv = neg ? -(int32_t)ip : (int32_t)ip;
		if (ip > 32767)
			ip = 32767;
		v = ip << 16;
		if (real && fd)
			v += frac_fx(fp, fd);
		*fxv = neg ? -(fx)v : (fx)v;
	}
	return t;
}

#endif

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#include <stdlib.h>
#include <string.h>
#include "textview.h"

void tv_init(TV *v)
{
	memset(v, 0, sizeof *v);
}

void tv_free(TV *v)
{
	free(v->start);
	tv_init(v);
}

static int add(TV *v, unsigned long off)
{
	if (v->nlines == v->cap) {
		long cap = v->cap ? v->cap * 2 : 256;
		unsigned long *p = realloc(v->start, (size_t)cap * sizeof *p);

		if (!p)
			return -1;
		v->start = p;
		v->cap = cap;
	}
	v->start[v->nlines++] = off;
	return 0;
}

/*
 * From p, n characters at most (1 to 32768): where a '\n' is (the
 * pointer to it), else p + n; *brk the last ' ' before that, or NULL.
 * One test a character on a 68000 (above ' ', the most of them, goes
 * on) and a word counter: the loop in C, with its long index and two
 * tests, was 63 ms a page of an ST's (4300 characters), the most of
 * "next page" (gemprof.py, 1 Nov 2026).
 */
static const char *scan(const char *p, long n, const char **brk)
{
#if defined(__m68k__)
	const char *b = 0;

	__asm__("subq.w #1,%2\n"
		"1:\tmove.b (%0)+,%%d0\n\t"
		"cmp.b #32,%%d0\n\t"
		"bhi.s 3f\n\t"
		"beq.s 2f\n\t"
		"cmp.b #10,%%d0\n\t"
		"bne.s 3f\n\t"
		"subq.l #1,%0\n\t"
		"bra.s 4f\n"
		"2:\tlea -1(%0),%1\n"
		"3:\tdbra %2,1b\n"
		"4:"
		: "+a"(p), "+a"(b), "+d"(n)
		:
		: "d0", "cc");
	*brk = b;
	return p;
#else
	const char *e = p + n, *b = NULL;

	for (; p < e && *p != '\n'; p++)
		if (*p == ' ')
			b = p;
	*brk = b;
	return p;
#endif
}

long tv_find(const char *p, long n, char c)
{
	const char *s = p;

	while (n > 0) {
		long k = n > 32768 ? 32768 : n;

		n -= k;
#if defined(__m68k__)
		{
			short f = 0;

			/* cmp.b and dbeq: 18 cycles a character */
			__asm__("subq.w #1,%1\n"
				"1:\tcmp.b (%0)+,%3\n\t"
				"dbeq %1,1b\n\t"
				"seq %2"
				: "+a"(p), "+d"(k), "+d"(f)
				: "d"(c)
				: "cc");
			if (f)
				return p - 1 - s;
		}
#else
		{
			long i;

			for (i = 0; i < k; i++)
				if (p[i] == c)
					return p + i - s;
			p += k;
		}
#endif
	}
	return -1;
}

int tv_layout(TV *v, const char *text, long len, int cols)
{
	long pos = 0;

	v->nlines = 0;
	v->text = text;
	v->len = len;
	v->cols = cols < 1 ? 1 : cols;
	while (pos < len) {
		long end, brk, next, n = len - pos;
		const char *b;

		if (add(v, (unsigned long)pos))
			return -1;
		if (n > v->cols)
			n = v->cols;
		end = scan(text + pos, n, &b) - text;
		brk = b ? b - text : -1;
		if (end >= len || text[end] == '\n') {
			next = end < len ? end + 1 : end;
		} else if (text[end] == ' ') {
			next = end + 1;
		} else if (brk > pos) {
			next = brk + 1;
		} else {
			next = end;
		}
		pos = next;
		if (pos >= len && len > 0 && text[len - 1] == '\n')
			break;
	}
	return 0;
}

const char *tv_line(const TV *v, long i, long *n)
{
	long a, b;

	if (i < 0 || i >= v->nlines)
		return NULL;
	a = (long)v->start[i];
	b = i + 1 < v->nlines ? (long)v->start[i + 1] : v->len;
	while (b > a && (v->text[b - 1] == '\n' || v->text[b - 1] == ' '))
		b--;
	*n = b - a;
	return v->text + a;
}

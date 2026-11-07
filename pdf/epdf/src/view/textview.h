/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * textview.h - a text laid out in lines of at most `cols` characters
 * (the text view's model; no GEM, so the host tests it). Lines end at a
 * newline, else at the last blank that fits, else at `cols`.
 */
#ifndef TEXTVIEW_H
#define TEXTVIEW_H

typedef struct {
	const char *text;		/* not copied */
	long len;
	unsigned long *start;		/* the offset of each line's first character */
	long nlines, cap;
	int cols;
} TV;

void tv_init(TV *v);
void tv_free(TV *v);
/* lay text[0..len) out for cols characters a line; 0, or -1 out of memory */
int tv_layout(TV *v, const char *text, long len, int cols);
/* where c is in p's n characters (its offset), or -1 */
long tv_find(const char *p, long n, char c);
/* line i: its first character and length, the end of line and the blank
   it broke at left out; NULL beyond the last */
const char *tv_line(const TV *v, long i, long *n);

#endif

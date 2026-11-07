/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * ccitt.c - CCITTFaxDecode (ITU-T T.4 and T.6). Group 4 codes each row against the one before
 * as a list of the places the colour changes; the run-length codes are looked up 13 bits at a
 * time in two tables built on first use (ccitt_tab.h, made by tools/gen-ccitt.py).
 */
#include <stdlib.h>
#include <string.h>
#include "ccitt.h"
#include "ccitt_tab.h"

static uint16_t *g_wtab, *g_btab;
static int g_refs;

static int tables(CCITT *c)
{
	int i, j;

	if (!g_wtab) {
		g_wtab = calloc(8192, 2);
		g_btab = calloc(8192, 2);
		if (!g_wtab || !g_btab) {
			free(g_wtab);
			free(g_btab);
			g_wtab = g_btab = NULL;
			return -1;
		}
		for (i = 0; i < 104; i++) {
			int lw = cc_white[i].len, lb = cc_black[i].len;
			unsigned bw = (unsigned)cc_white[i].code << (13 - lw), bb = (unsigned)cc_black[i].code << (13 - lb);
			unsigned ew = 1u << (13 - lw), eb = 1u << (13 - lb);

			for (j = 0; j < (int)ew; j++)
				g_wtab[bw + j] = (uint16_t)(lw << 12 | cc_white[i].run);
			for (j = 0; j < (int)eb; j++)
				g_btab[bb + j] = (uint16_t)(lb << 12 | cc_black[i].run);
		}
	}
	g_refs++;
	c->wtab = g_wtab;
	c->btab = g_btab;
	return 0;
}

int ccitt_open(CCITT *c, const uint8_t *data, long n, int cols, int k, int byte_align)
{
	memset(c, 0, sizeof *c);
	c->cols = cols;
	c->k = k;
	c->align = byte_align;
	c->in = data;
	c->nbits = n * 8;
	c->lim = 0x7FFFFFFFL;
	if (cols < 1 || cols > 65000)
		return -1;
	c->ref = malloc((size_t)(cols + 8) * 2);
	c->cur = malloc((size_t)(cols + 8) * 2);
	if (!c->ref || !c->cur || tables(c) != 0) {
		free(c->ref);
		free(c->cur);
		return -1;
	}
	c->ref[0] = c->ref[1] = (uint16_t)cols;		/* the line above the first: all white */
	c->nref = 0;
	return 0;
}

#define CCBUF 4096

/* the bytes not read yet to buf's start, and buf filled again behind them */
static void refill(CCITT *c)
{
	long p = c->pos >> 3, have = (c->nbits >> 3) - p;

	if (have < 0)
		have = 0;
	if (have)
		memmove(c->buf, c->buf + p, (size_t)have);
	c->pos &= 7;
	while (have < CCBUF) {
		long k = c->fill(c->fu, c->buf + have, CCBUF - have);

		if (k <= 0) {
			c->fill = NULL;		/* the end: what's in buf is all */
			break;
		}
		have += k;
	}
	c->nbits = have * 8;
	/* a code is 13 bits at most and peek looks 3 bytes on: refilled 8 bytes before buf's end */
	c->lim = c->fill ? (have - 8) * 8 : 0x7FFFFFFFL;
}

#define MORE(c) do { if ((c)->pos > (c)->lim) refill(c); } while (0)

int ccitt_open_pull(CCITT *c, long (*fill)(void *u, uint8_t *buf, long n), void *u, int cols, int k, int byte_align)
{
	uint8_t *buf = malloc(CCBUF);

	if (!buf)
		return -1;
	if (ccitt_open(c, NULL, 0, cols, k, byte_align)) {
		free(buf);
		return -1;
	}
	c->in = c->buf = buf;
	c->fill = fill;
	c->fu = u;
	c->lim = -1;			/* filled at the first row */
	return 0;
}

void ccitt_close(CCITT *c)
{
	free(c->buf);
	free(c->ref);
	free(c->cur);
	if (g_refs && --g_refs == 0) {
		free(g_wtab);
		free(g_btab);
		g_wtab = g_btab = NULL;
	}
}

/* the next n (<= 13) bits, past the end zeros */
static inline unsigned peek(const CCITT *c, int n)
{
	long p = c->pos >> 3;
	long bytes = c->nbits >> 3;
	uint32_t v = 0;

	if (p < bytes)
		v = (uint32_t)c->in[p] << 16;
	if (p + 1 < bytes)
		v |= (uint32_t)c->in[p + 1] << 8;
	if (p + 2 < bytes)
		v |= c->in[p + 2];
	return (v >> (24 - (int)(c->pos & 7) - n)) & ((1u << n) - 1);
}

static int run(CCITT *c, int black)
{
	const uint16_t *tab = black ? c->btab : c->wtab;
	int total = 0;

	for (;;) {
		unsigned e;
		int r;

		MORE(c);
		e = tab[peek(c, 13)];

		if (!e) {
			c->err = 1;
			return -1;
		}
		c->pos += e >> 12;
		r = e & 0xFFF;
		total += r;
		if (r < 64)
			return total;
	}
}

static void setbits(uint8_t *row, int x0, int x1)
{
	int b0 = x0 >> 3, b1 = x1 >> 3;
	unsigned m0 = 0xFFu >> (x0 & 7), m1 = 0xFF00u >> (x1 & 7) & 0xFF;

	if (x0 >= x1)
		return;
	if (b0 == b1) {
		row[b0] |= (uint8_t)(m0 & m1);
		return;
	}
	row[b0] |= (uint8_t)m0;
	if (b1 > b0 + 1)
		memset(row + b0 + 1, 0xFF, (size_t)(b1 - b0 - 1));
	if (m1)
		row[b1] |= (uint8_t)m1;
}

static void render(CCITT *c, uint8_t *row)
{
	int i;

	memset(row, 0, (size_t)(c->cols + 7) / 8);
	for (i = 0; i + 1 < c->ncur; i += 2)
		setbits(row, c->cur[i], c->cur[i + 1] < c->cols ? c->cur[i + 1] : c->cols);
	if (c->ncur & 1)
		setbits(row, c->cur[c->ncur - 1], c->cols);
}

static void push(CCITT *c, int x)
{
	if (x > c->cols)
		x = c->cols;
	if (c->ncur > 0 && x < c->cur[c->ncur - 1])
		x = c->cur[c->ncur - 1];
	if (c->ncur < c->cols + 4)
		c->cur[c->ncur++] = (uint16_t)x;
}

int ccitt_row(CCITT *c, uint8_t *row)
{
	int cols = c->cols, a0 = -1, color = 0, bi = 0;

	if (c->eof || c->err)
		return 0;
	if (c->align)
		c->pos = (c->pos + 7) & ~7L;
	MORE(c);
	if (c->pos >= c->nbits) {
		c->eof = 1;
		return 0;
	}
	c->ncur = 0;
	if (c->k == 0) {					/* group 3, one-dimensional: runs from the left */
		int x = 0, black = 0;

		while (peek(c, 12) == 0 && c->pos < c->nbits) {	/* fill bits, then the EOL */
			c->pos++;
			MORE(c);
		}
		if (peek(c, 12) == 1)
			c->pos += 12;
		while (x < cols) {
			int r = run(c, black);

			if (r < 0) {
				c->eof = 1;
				return 0;
			}
			x += r;
			push(c, x);
			black ^= 1;
		}
		render(c, row);
		return 1;
	}
	while (a0 < cols) {
		unsigned v;
		int b1, b2;

		MORE(c);
		v = peek(c, 7);

		/* b1: the first change on the line above, right of a0, to the other colour */
		while (bi > 0 && c->ref[bi - 1] > a0)
			bi--;
		while (bi < c->nref && (c->ref[bi] <= a0 || (bi & 1) != color))
			bi++;
		b1 = bi < c->nref ? c->ref[bi] : cols;
		b2 = bi + 1 < c->nref ? c->ref[bi + 1] : cols;
		if (v & 0x40) {					/* 1: V0 */
			c->pos += 1;
			push(c, b1);
			a0 = b1;
			color ^= 1;
		} else if ((v >> 4) == 3) {			/* 011: VR1 */
			c->pos += 3;
			a0 = b1 + 1;
			push(c, a0);
			color ^= 1;
		} else if ((v >> 4) == 2) {			/* 010: VL1 */
			c->pos += 3;
			a0 = b1 - 1;
			push(c, a0);
			color ^= 1;
		} else if ((v >> 4) == 1) {			/* 001: horizontal */
			int r1, r2;

			c->pos += 3;
			r1 = run(c, color);
			r2 = r1 < 0 ? -1 : run(c, !color);
			if (r2 < 0)
				break;
			if (a0 < 0)
				a0 = 0;
			push(c, a0 + r1);
			a0 += r1 + r2;
			push(c, a0);
		} else if ((v >> 3) == 1) {			/* 0001: pass */
			c->pos += 4;
			a0 = b2;
		} else if ((v >> 1) == 3) {			/* 000011: VR2 */
			c->pos += 6;
			a0 = b1 + 2;
			push(c, a0);
			color ^= 1;
		} else if ((v >> 1) == 2) {			/* 000010: VL2 */
			c->pos += 6;
			a0 = b1 - 2;
			push(c, a0);
			color ^= 1;
		} else if (v == 3) {				/* 0000011: VR3 */
			c->pos += 7;
			a0 = b1 + 3;
			push(c, a0);
			color ^= 1;
		} else if (v == 2) {				/* 0000010: VL3 */
			c->pos += 7;
			a0 = b1 - 3;
			push(c, a0);
			color ^= 1;
		} else {					/* the end of the data (EOFB) or damage */
			c->eof = 1;
			if (a0 < 0)
				return 0;
			break;
		}
		if (a0 > cols)
			a0 = cols;
	}
	/* a line that ran short is white to its end */
	render(c, row);
	c->cur[c->ncur] = (uint16_t)cols;
	c->cur[c->ncur + 1] = (uint16_t)cols;
	{
		uint16_t *t = c->ref;

		c->ref = c->cur;
		c->cur = t;
		c->nref = c->ncur + 2;
	}
	return 1;
}

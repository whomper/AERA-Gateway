/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * jbig2.c - JBIG2Decode (jbig2.h), written from ITU-T T.88 (06/2000 and
 * its amendments): the segments of the globals and of the page in their
 * order, each region decoded into a bitmap of its own and put onto the
 * page by its combination operator.
 *
 *   generic region      6.2: templates 0-3 with their AT pixels, typical
 *                       prediction (TPGDON); MMR through ccitt.c (T.6)
 *   generic refinement  6.3: templates 0 and 1, TPGRON
 *   text region         6.4, the arithmetic coder's (SBHUFF 0)
 *   symbol dictionary   6.5, the same, refinement and aggregate symbols
 *                       too (SDREFAGG)
 *   page information, end of stripe, end of page
 *
 * Not yet: the Huffman-coded dictionaries and text regions (SDHUFF,
 * SBHUFF), pattern dictionaries and halftone regions: JB2_UNSUPPORTED.
 * Every PDF measured (16 Nov 2026: the corpus's archive.org scans and
 * the archive.org text PDFs of 2024-25) uses only the arithmetic coder.
 */
#include <stdlib.h>
#include <string.h>
#ifdef JB2_STATS
#include <stdio.h>
#endif
#include "mq.h"
#include "ccitt.h"
#include "jbig2.h"

/* the file is built for size (-Os: a 68000's EPDF.PRG carries it); its
   inner loops for speed */
#if defined(__GNUC__) && !defined(__clang__)
#define HOT __attribute__((optimize("O2")))
#else
#define HOT
#endif

#ifdef JB2_TIMES
long pdf_us(void);
long jb2_t_decode, jb2_t_put, jb2_t_alloc, jb2_t_dict, jb2_t_text;
#define T0(v) long v = pdf_us()
#define TADD(acc, v) (acc += pdf_us() - (v))
#else
#define T0(v)
#define TADD(acc, v)
#endif

/* ------------------------------------------------------------------ bitmaps */

static int bm_new(JB2BM *b, int w, int h, int fill)
{
	b->w = w;
	b->h = h;
	b->stride = (w + 7) >> 3;
	b->d = NULL;
	if (w <= 0 || h <= 0)
		return w >= 0 && h >= 0;	/* (an empty bitmap: a space's symbol) */
	b->d = malloc((size_t)b->stride * (size_t)h);
	if (!b->d)
		return 0;
	memset(b->d, fill ? 0xFF : 0, (size_t)b->stride * (size_t)h);
	return 1;
}

/* bm_new's blank bitmap, cleared 32 KB at a time with a turn between (a
   big symbol's whole was 80 ms on Falcon-4): 1, 0 refused memory, -1
   stopped */
static int bm_new_turns(JB2BM *b, int w, int h, int (*poll)(void))
{
	long n, k;

	b->w = w;
	b->h = h;
	b->stride = (w + 7) >> 3;
	b->d = NULL;
	if (w <= 0 || h <= 0)
		return w >= 0 && h >= 0;
	n = b->stride * (long)h;
	if (!(b->d = malloc((size_t)n)))
		return 0;
	for (k = 0; k < n; k += 32768L) {
		memset(b->d + k, 0, (size_t)(n - k < 32768L ? n - k : 32768L));
		if (poll && !poll())
			return -1;
	}
	return 1;
}

void jb2_bm_free(JB2BM *b)
{
	free(b->d);
	b->d = NULL;
	b->w = b->h = 0;
}

static int px(const JB2BM *b, int x, int y)
{
	if (x < 0 || y < 0 || x >= b->w || y >= b->h)
		return 0;
	return (b->d[(long)y * b->stride + (x >> 3)] >> (7 - (x & 7))) & 1;
}

static void setpx(JB2BM *b, int x, int y, int v)
{
	uint8_t *p = b->d + (long)y * b->stride + (x >> 3);
	int m = 0x80 >> (x & 7);

	*p = (uint8_t)(v ? *p | m : *p & ~m);
}

/* 8 bits of row s (n bytes) from bit b on, 0 outside it */
HOT static unsigned sbits(const uint8_t *s, long n, long b)
{
	long i = b >= 0 ? b >> 3 : -((-b + 7) >> 3);
	int k = (int)(b - i * 8);
	unsigned hi = i >= 0 && i < n ? s[i] : 0, lo = i + 1 >= 0 && i + 1 < n ? s[i + 1] : 0;

	return ((hi << 8 | lo) >> (8 - k)) & 0xFF;
}

/* src onto dst with its top left at x, y: 0 OR, 1 AND, 2 XOR, 3 XNOR, 4 REPLACE,
   a byte of dst at a time (src's bits past its width are 0) */
HOT static void compose(JB2BM *dst, const JB2BM *src, int x, int y, int op)
{
	int sy;
	long dx0 = x < 0 ? 0 : x, dx1 = (long)x + src->w > dst->w ? dst->w : (long)x + src->w, j;

	if (dx0 >= dx1 || !src->d)
		return;
	if ((op == 0 || op == 2) && x >= 0 && (long)x + src->w <= dst->w) {
		/* OR or XOR, wholly across (a symbol on a row, the usual): each
		   source byte shifted into place, its spill carried to the next */
		int sh = x & 7;
		long n = src->stride, i;

		for (sy = 0; sy < src->h; sy++) {
			const uint8_t *sr = src->d + (long)sy * src->stride;
			uint8_t *d;
			int dy = y + sy;

			if (dy < 0 || dy >= dst->h)
				continue;
			d = dst->d + (long)dy * dst->stride + (x >> 3);
			if (op == 0) {
				if (!sh)
					for (i = 0; i < n; i++)
						d[i] |= sr[i];
				else {
					unsigned carry = 0;

					for (i = 0; i < n; i++) {
						d[i] |= (uint8_t)(carry | sr[i] >> sh);
						carry = (unsigned)(sr[i] << (8 - sh)) & 0xFF;
					}
					if (carry)
						d[n] |= (uint8_t)carry;	/* (inside: the source's bits stop by dst->w) */
				}
			} else {
				if (!sh)
					for (i = 0; i < n; i++)
						d[i] ^= sr[i];
				else {
					unsigned carry = 0;

					for (i = 0; i < n; i++) {
						d[i] ^= (uint8_t)(carry | sr[i] >> sh);
						carry = (unsigned)(sr[i] << (8 - sh)) & 0xFF;
					}
					if (carry)
						d[n] ^= (uint8_t)carry;
				}
			}
		}
		return;
	}
	for (sy = 0; sy < src->h; sy++) {
		const uint8_t *s = src->d + (long)sy * src->stride;
		uint8_t *d;
		int dy = y + sy;

		if (dy < 0 || dy >= dst->h)
			continue;
		d = dst->d + (long)dy * dst->stride;
		for (j = dx0 >> 3; j <= (dx1 - 1) >> 3; j++) {
			unsigned sb = sbits(s, src->stride, j * 8 - x), m = 0xFF, v = d[j];

			if (j * 8 < dx0)
				m &= 0xFFu >> (dx0 - j * 8);
			if (j * 8 + 8 > dx1)
				m &= (0xFFu << (j * 8 + 8 - dx1)) & 0xFF;
			switch (op) {
			case 0: v |= sb & m; break;
			case 1: v &= sb | ~m; break;
			case 2: v ^= sb & m; break;
			case 3: v = (v & ~m) | (~(v ^ sb) & m); break;
			default: v = (v & ~m) | (sb & m); break;
			}
			d[j] = (uint8_t)v;
		}
	}
}

/* ------------------------------------------------------------------ the integer decoders (annex A) */

typedef struct {
	uint8_t cx[512];
} IAX;

/* 1 for OOB, else 0 and *v */
static int ia_int(MQ *m, IAX *ia, int32_t *v)
{
	unsigned prev = 1;
	int s, i, n;
	uint32_t val, off;

#define IA_BIT(b)                                                                            \
	do {                                                                                 \
		(b) = mq_decode(m, &ia->cx[prev]);                                           \
		prev = prev < 256 ? (prev << 1 | (unsigned)(b)) : (((prev << 1 | (unsigned)(b)) & 511) | 256); \
	} while (0)
	IA_BIT(s);
	IA_BIT(i);
	if (!i)
		n = 2, off = 0;
	else {
		IA_BIT(i);
		if (!i)
			n = 4, off = 4;
		else {
			IA_BIT(i);
			if (!i)
				n = 6, off = 20;
			else {
				IA_BIT(i);
				if (!i)
					n = 8, off = 84;
				else {
					IA_BIT(i);
					if (!i)
						n = 12, off = 340;
					else
						n = 32, off = 4436;
				}
			}
		}
	}
	val = 0;
	while (n--) {
		int b;

		IA_BIT(b);
		val = val << 1 | (uint32_t)b;
	}
#undef IA_BIT
	val += off;
	if (s && val == 0)
		return 1;
	*v = s ? -(int32_t)val : (int32_t)val;
	return 0;
}

/* IAID (A.3): len bits */
static uint32_t ia_id(MQ *m, uint8_t *cx, int len)
{
	uint32_t prev = 1;
	int i;

	for (i = 0; i < len; i++)
		prev = prev << 1 | (uint32_t)mq_decode(m, &cx[prev]);
	return prev - (1u << len);
}

/* ------------------------------------------------------------------ generic region (6.2) */

static const int8_t at_default[4][8] = {
	{ 3, -1, -3, -1, 2, -2, -2, -2 }, { 3, -1 }, { 2, -1 }, { 2, -1 }
};

/* the context of x, y for template t (T.88 figures 3-6, the AT pixels in
   their places) */
static unsigned gctx(const JB2BM *b, int x, int y, int t, const int8_t *at)
{
	switch (t) {
	case 0:
		return (unsigned)px(b, x - 1, y) | (unsigned)px(b, x - 2, y) << 1 | (unsigned)px(b, x - 3, y) << 2 |
		       (unsigned)px(b, x - 4, y) << 3 | (unsigned)px(b, x + at[0], y + at[1]) << 4 |
		       (unsigned)px(b, x + 2, y - 1) << 5 | (unsigned)px(b, x + 1, y - 1) << 6 | (unsigned)px(b, x, y - 1) << 7 |
		       (unsigned)px(b, x - 1, y - 1) << 8 | (unsigned)px(b, x - 2, y - 1) << 9 |
		       (unsigned)px(b, x + at[2], y + at[3]) << 10 | (unsigned)px(b, x + at[4], y + at[5]) << 11 |
		       (unsigned)px(b, x + 1, y - 2) << 12 | (unsigned)px(b, x, y - 2) << 13 | (unsigned)px(b, x - 1, y - 2) << 14 |
		       (unsigned)px(b, x + at[6], y + at[7]) << 15;
	case 1:
		return (unsigned)px(b, x - 1, y) | (unsigned)px(b, x - 2, y) << 1 | (unsigned)px(b, x - 3, y) << 2 |
		       (unsigned)px(b, x + at[0], y + at[1]) << 3 | (unsigned)px(b, x + 2, y - 1) << 4 |
		       (unsigned)px(b, x + 1, y - 1) << 5 | (unsigned)px(b, x, y - 1) << 6 | (unsigned)px(b, x - 1, y - 1) << 7 |
		       (unsigned)px(b, x - 2, y - 1) << 8 | (unsigned)px(b, x + 2, y - 2) << 9 | (unsigned)px(b, x + 1, y - 2) << 10 |
		       (unsigned)px(b, x, y - 2) << 11 | (unsigned)px(b, x - 1, y - 2) << 12;
	case 2:
		return (unsigned)px(b, x - 1, y) | (unsigned)px(b, x - 2, y) << 1 | (unsigned)px(b, x + at[0], y + at[1]) << 2 |
		       (unsigned)px(b, x + 1, y - 1) << 3 | (unsigned)px(b, x, y - 1) << 4 | (unsigned)px(b, x - 1, y - 1) << 5 |
		       (unsigned)px(b, x - 2, y - 1) << 6 | (unsigned)px(b, x + 1, y - 2) << 7 | (unsigned)px(b, x, y - 2) << 8 |
		       (unsigned)px(b, x - 1, y - 2) << 9;
	default:
		return (unsigned)px(b, x - 1, y) | (unsigned)px(b, x - 2, y) << 1 | (unsigned)px(b, x - 3, y) << 2 |
		       (unsigned)px(b, x - 4, y) << 3 | (unsigned)px(b, x + at[0], y + at[1]) << 4 |
		       (unsigned)px(b, x + 1, y - 1) << 5 | (unsigned)px(b, x, y - 1) << 6 | (unsigned)px(b, x - 1, y - 1) << 7 |
		       (unsigned)px(b, x - 2, y - 1) << 8 | (unsigned)px(b, x - 3, y - 1) << 9;
	}
}

static const unsigned sltp_cx[4] = { 0x9B25, 0x0795, 0x00E5, 0x0195 };

/* with the AT pixels where the templates have them, each row's part of the
   context is a run of neighbours: a window slid along (row y-2's, y-1's
   and y's so far), a bit read and a shift a pixel */
static const struct {
	int8_t w2, r2, w1, r1, w0, s2, s1;	/* widths, rightmost x offsets, shifts */
} slide[4] = {
	{ 5, 2, 7, 3, 4, 11, 4 }, { 4, 2, 6, 3, 3, 9, 3 }, { 3, 1, 5, 2, 2, 7, 2 }, { 0, 0, 6, 2, 4, 0, 4 }
};

HOT static unsigned rbit(const uint8_t *row, int x, int w)
{
	return row && x >= 0 && x < w ? (row[x >> 3] >> (7 - (x & 7))) & 1 : 0;
}

static unsigned window(const uint8_t *row, int width, int right, int w)
{
	unsigned v = 0;
	int k;

	for (k = right - width + 1; k <= right; k++)
		v = v << 1 | rbit(row, k, w);
	return v;
}

static int at_is_default(int t, const int8_t *at)
{
	return !memcmp(at, at_default[t], t == 0 ? 8 : 2);
}

/* template 0 with its AT pixels in place, the one every PDF measured uses:
   the context kept in one register (T.88's own bit order: this row's x-4
   .. x-1 in bits 3-0, the row above's x-3 .. x+3 in 10-4, the one above
   that's x-2 .. x+2 in 15-11), shifted along a pixel at a time with the
   two pixels coming in from the rows above, read two bytes at a time;
   the coder inline on its registers. A 68030's inner loop */
#if defined(__m68k__) && !defined(JB2_NO_ASM)
/* jb2t0.S: a row's whole bytes, the same as the C below */
unsigned jb2_t0_bytes(MQ *m, uint8_t *cx, uint8_t *out, const uint8_t *r1, const uint8_t *r2, long n, unsigned ctx);
#define T0_ASM 1
#endif

/* one row o of template 0 (its AT pixels in place), r1 and r2 the rows
   above (zeros above the first: never NULL), W pixels, st bytes a row */
HOT static void t0_row(MQ *m, uint8_t *cx, uint8_t *o, const uint8_t *r1, const uint8_t *r2, int W, long st)
{
	uint32_t c, a;
	int ct, x = 0;
	unsigned ctx = window(r2, 5, 2, W) << 11 | window(r1, 7, 3, W) << 4;
	long j = 0;

#ifdef T0_ASM
	{
		long na = (W >> 3) < st - 1 ? (W >> 3) : st - 1;

		if (na > 0) {
			ctx = jb2_t0_bytes(m, cx, o, r1, r2, na, ctx);
			x = (int)(na * 8);
			j = na;
		}
	}
#endif
	MQ_LOAD(m, c, a, ct);
	for (; x < W; j++) {
		/* the bits coming in, both rows in one long shifted together:
		   the row above's x + 4 at bit 11, the one above that's x + 3
		   at bit 28; acc's top bit (from 1) says the byte is done */
		uint32_t w = ((uint32_t)r1[j] << 8 | (j + 1 < st ? r1[j + 1] : 0)) |
			     ((uint32_t)r2[j] << 24 | (uint32_t)(j + 1 < st ? r2[j + 1] : 0) << 16);
		unsigned acc = 1;

		if (x + 8 <= W) {
			do {
				int v;

				MQ_DECODE(m, c, a, ct, &cx[ctx], v);
				acc = acc << 1 | (unsigned)v;
				ctx = ((ctx << 1) & 0xF7EE) | (unsigned)v | ((w >> 7) & 0x10) | ((w >> 17) & 0x800);
				w <<= 1;
			} while (acc < 256);
			o[j] = (uint8_t)acc;
			x += 8;
		} else {
			int k = 0;

			for (; x < W; k++, x++) {
				int v;

				MQ_DECODE(m, c, a, ct, &cx[ctx], v);
				acc = acc << 1 | (unsigned)v;
				ctx = ((ctx << 1) & 0xF7EE) | (unsigned)v | ((w >> 7) & 0x10) | ((w >> 17) & 0x800);
				w <<= 1;
			}
			o[j] = (uint8_t)(acc << (8 - k));
		}
	}
	MQ_SAVE(m, c, a, ct);
}

/* templates 1-3 the same way (their windows: slide[]) */
HOT static void slide_row(MQ *m, uint8_t *cx, int t, uint8_t *o, const uint8_t *c, const uint8_t *a, int W)
{
	const int w2 = slide[t].w2, r2 = slide[t].r2, w1 = slide[t].w1, r1 = slide[t].r1;
	const unsigned m2 = (1u << w2) - 1, m1 = (1u << w1) - 1, m0 = (1u << slide[t].w0) - 1;
	const int s2 = slide[t].s2, s1 = slide[t].s1;
	unsigned v2 = w2 ? window(a, w2, r2, W) : 0, v1 = window(c, w1, r1, W), v0 = 0, acc = 0;
	int x;

	for (x = 0; x < W; x++) {
		unsigned v = (unsigned)mq_decode(m, &cx[v2 << s2 | v1 << s1 | v0]);

		acc = acc << 1 | v;
		if ((x & 7) == 7) {
			o[x >> 3] = (uint8_t)acc;
			acc = 0;
		}
		v0 = (v0 << 1 | v) & m0;
		v1 = (v1 << 1 | rbit(c, x + 1 + r1, W)) & m1;
		v2 = (v2 << 1 | rbit(a, x + 1 + r2, W)) & m2;
	}
	if (W & 7)
		o[W >> 3] = (uint8_t)(acc << (8 - (W & 7)));
}

/* the generic region's decoder a row at a time (6.2.5): its coder, its
   contexts and the rows so far, in a bitmap of its own whole (b, row y
   next) or of three rows only (a streamed region's: the row being made
   last, above it the two before) */
typedef struct {
	MQ m;
	uint8_t *cx;			/* 65536 contexts (template 0), fewer for the others */
	int t, tpgdon, ltp, fast;
	int8_t at[8];
} GEN;

static void gen_init(GEN *g, uint8_t *cx, int t, int tpgdon, const int8_t *at)
{
	g->cx = cx;
	g->t = t;
	g->tpgdon = tpgdon;
	g->ltp = 0;
	memcpy(g->at, at, 8);
	g->fast = at_is_default(t, at);
}

/* row y of b (rows y-2 and y-1 above it in b, or zeros above row 0):
   decoded, or copied from the row above (typical prediction) */
static void gen_row(GEN *g, JB2BM *b, int y, const uint8_t *zr)
{
	uint8_t *o = b->d + (long)y * b->stride;
	const uint8_t *r1 = y >= 1 ? o - b->stride : zr, *r2 = y >= 2 ? o - 2 * b->stride : zr;
	int x;

	if (g->tpgdon) {
		g->ltp ^= mq_decode(&g->m, &g->cx[sltp_cx[g->t]]);
		if (g->ltp) {
			if (y > 0)
				memcpy(o, o - b->stride, (size_t)b->stride);
			else
				memset(o, 0, (size_t)b->stride);
			return;
		}
	}
	if (g->fast) {
		if (g->t == 0)
			t0_row(&g->m, g->cx, o, r1, r2, b->w, b->stride);
		else
			slide_row(&g->m, g->cx, g->t, o, r1, r2, b->w);
		return;
	}
	memset(o, 0, (size_t)b->stride);
	for (x = 0; x < b->w; x++)
		if (mq_decode(&g->m, &g->cx[gctx(b, x, y, g->t, g->at)]))
			setpx(b, x, y, 1);
}

/* b (made, cleared) decoded whole with the contexts cx (65536 bytes for
   template 0, 8192 for 1, 1024 for 2 and 3, carried from symbol to symbol) */
static int generic_arith(MQ *m, uint8_t *cx, JB2BM *b, int t, int tpgdon, const int8_t *at, int (*poll)(void))
{
	GEN g;
	uint8_t *zr = calloc((size_t)b->stride + 2, 1);	/* (the rows above the first) */
	int y;

	if (!zr)
		return JB2_MEMORY;
	g.m = *m;
	gen_init(&g, cx, t, tpgdon, at);
	for (y = 0; y < b->h; y++) {
		gen_row(&g, b, y, zr);
		if (poll && !poll()) {	/* (every row: a wide symbol's 4 were 120 ms on Falcon-4) */
			free(zr);
			return JB2_STOPPED;
		}
	}
	*m = g.m;
	free(zr);
	return JB2_OK;
}

static int generic_mmr(const uint8_t *d, long n, JB2BM *b)
{
	CCITT c;
	int y;

	if (ccitt_open(&c, d, n, b->w, -1, 0))
		return JB2_MEMORY;
	for (y = 0; y < b->h; y++)
		if (!ccitt_row(&c, b->d + (long)y * b->stride))
			break;		/* (the rest stays white) */
	ccitt_close(&c);
	return JB2_OK;
}

/* ------------------------------------------------------------------ generic refinement (6.3) */

static unsigned rctx(const JB2BM *b, const JB2BM *r, int x, int y, int dx, int dy, int t, const int8_t *at)
{
	int rx = x - dx, ry = y - dy;

	if (t == 0)
		return (unsigned)px(b, x - 1, y) | (unsigned)px(b, x + 1, y - 1) << 1 | (unsigned)px(b, x, y - 1) << 2 |
		       (unsigned)px(b, x + at[0], y + at[1]) << 3 | (unsigned)px(r, rx + 1, ry + 1) << 4 |
		       (unsigned)px(r, rx, ry + 1) << 5 | (unsigned)px(r, rx - 1, ry + 1) << 6 | (unsigned)px(r, rx + 1, ry) << 7 |
		       (unsigned)px(r, rx, ry) << 8 | (unsigned)px(r, rx - 1, ry) << 9 | (unsigned)px(r, rx + 1, ry - 1) << 10 |
		       (unsigned)px(r, rx, ry - 1) << 11 | (unsigned)px(r, rx + at[2], ry + at[3]) << 12;
	return (unsigned)px(b, x - 1, y) | (unsigned)px(b, x + 1, y - 1) << 1 | (unsigned)px(b, x, y - 1) << 2 |
	       (unsigned)px(b, x - 1, y - 1) << 3 | (unsigned)px(r, rx + 1, ry + 1) << 4 | (unsigned)px(r, rx, ry + 1) << 5 |
	       (unsigned)px(r, rx + 1, ry) << 6 | (unsigned)px(r, rx, ry) << 7 | (unsigned)px(r, rx - 1, ry) << 8 |
	       (unsigned)px(r, rx, ry - 1) << 9;
}

/* the same slid along (as generic_slide): with the AT pixels at (-1, -1),
   template 0's context is the reference's rows y-dy-1, y-dy and y-dy+1
   three pixels each and this row's x-1 and the row above's three;
   template 1's the reference's one, three and two */
HOT static int refine_slide(MQ *m, uint8_t *cx, JB2BM *b, const JB2BM *r, int dx, int dy, int t)
{
	int x, y, W = b->w, RW = r->w, RH = r->d ? r->h : 0;

	if (W <= 0 || !b->d)
		return JB2_OK;		/* (no pixels: nothing decoded, fuzz_jbig2) */
	for (y = 0; y < b->h; y++) {
		uint8_t *o = b->d + (long)y * b->stride;
		const uint8_t *c = y >= 1 ? o - b->stride : NULL;
		int ry = y - dy;
		const uint8_t *rm = ry - 1 >= 0 && ry - 1 < RH ? r->d + (long)(ry - 1) * r->stride : NULL;
		const uint8_t *r0 = ry >= 0 && ry < RH ? r->d + (long)ry * r->stride : NULL;
		const uint8_t *rp = ry + 1 >= 0 && ry + 1 < RH ? r->d + (long)(ry + 1) * r->stride : NULL;
		unsigned vc = window(c, 3, 1, W), v0 = 0, acc = 0, va, vb, vd;

		/* the reference's windows at x = 0, rx = -dx */
		if (t == 0) {
			va = window(rm, 3, 1 - dx, RW);
			vb = window(r0, 3, 1 - dx, RW);
			vd = window(rp, 3, 1 - dx, RW);
		} else {
			va = rbit(rm, -dx, RW);
			vb = window(r0, 3, 1 - dx, RW);
			vd = window(rp, 2, 1 - dx, RW);
		}
		for (x = 0; x < W; x++) {
			unsigned ctx = t == 0 ? va << 10 | vb << 7 | vd << 4 | vc << 1 | v0 : va << 9 | vb << 6 | vd << 4 | vc << 1 | v0;
			unsigned v = (unsigned)mq_decode(m, &cx[ctx]);
			int nx = x + 1 + 1 - dx;	/* the reference's column coming in */

			acc = acc << 1 | v;
			if ((x & 7) == 7) {
				o[x >> 3] = (uint8_t)acc;
				acc = 0;
			}
			v0 = v;
			vc = (vc << 1 | rbit(c, x + 2, W)) & 7;
			if (t == 0) {
				va = (va << 1 | rbit(rm, nx, RW)) & 7;
				vd = (vd << 1 | rbit(rp, nx, RW)) & 7;
			} else {
				va = rbit(rm, nx - 1, RW);
				vd = (vd << 1 | rbit(rp, nx, RW)) & 3;
			}
			vb = (vb << 1 | rbit(r0, nx, RW)) & 7;
		}
		if (W & 7)
			o[W >> 3] = (uint8_t)(acc << (8 - (W & 7)));
	}
	return JB2_OK;
}

/* b (made, cleared) refined from r at dx, dy; cx 8192 bytes (template 0) or 1024 */
static int refine(MQ *m, uint8_t *cx, JB2BM *b, const JB2BM *r, int dx, int dy, int t, const int8_t *at, int tpgron)
{
	int x, y, ltp = 0;

	if (!tpgron && (t == 1 || (at[0] == -1 && at[1] == -1 && at[2] == -1 && at[3] == -1)))
		return refine_slide(m, cx, b, r, dx, dy, t);

	for (y = 0; y < b->h; y++) {
		if (tpgron)
			ltp ^= mq_decode(m, &cx[t == 0 ? 0x0010 : 0x0008]);
		for (x = 0; x < b->w; x++) {
			if (ltp) {
				int rx = x - dx, ry = y - dy, v = px(r, rx, ry), same = 1, i, j;

				for (j = -1; j <= 1 && same; j++)
					for (i = -1; i <= 1; i++)
						if (px(r, rx + i, ry + j) != v) {
							same = 0;
							break;
						}
				if (same) {
					if (v)
						setpx(b, x, y, 1);
					continue;
				}
			}
			if (mq_decode(m, &cx[rctx(b, r, x, y, dx, dy, t, at)]))
				setpx(b, x, y, 1);
		}
	}
	return JB2_OK;
}

/* ------------------------------------------------------------------ segments */

typedef struct {
	uint32_t num;
	int type;
	uint32_t *refs;
	int nrefs;
	const uint8_t *d;
	long n;
	/* a symbol dictionary's exported symbols: its own new ones (own) and
	   those it passes on from the dictionaries it refers to, not copied */
	JB2BM **syms;
	int nsyms;
	JB2BM *own;
	uint32_t nown;
} SEG;

typedef struct {
	SEG *seg;
	int nseg, aseg;
	JB2BM page;
	int page_h_known;
	int def_op;
	long sym_bytes;		/* the dictionaries' symbols so far (JB2_DICT_BYTES) */
	int (*poll)(void);
} DEC;

int jb2_whole_page;		/* (the tests: every page decoded whole) */

/* the most a page or a region (16 MB a bit a pixel: 128 megapixels, an A3
   page at 600 dpi is 70) and the dictionaries' symbols together (4 MB; the
   corpus's biggest dictionary is 699 KB) may take: more is a picture too big
   for the memory, and a bomb's 6 KB of thousands of page-sized symbols
   (fuzz_jbig2) is refused at once, not decoded for minutes */
#define JB2_MAX_BYTES (16L << 20)
#define JB2_DICT_BYTES (4L << 20)

/* the work before a page's rows: its symbols and placements, as met
   (jb2_progress; one page is decoded at a time) */
void (*jb2_progress)(long done, long of);
static long work_done, work_of;

static void work(long done, long of)
{
	work_done += done;
	work_of += of;
	if (jb2_progress)
		jb2_progress(work_done, work_of > work_done ? work_of : work_done);
}

static uint32_t r32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static unsigned r16(const uint8_t *p)
{
	return (unsigned)p[0] << 8 | p[1];
}

/* the segment headers and data of buf, onto dec's list: 0 or a reason */
static int read_segments(DEC *dc, const uint8_t *buf, long n)
{
	long p = 0;

	while (p + 11 <= n) {
		SEG s;
		int flags, cnt, i, rsize;
		long len;

		memset(&s, 0, sizeof s);
		s.num = r32(buf + p);
		flags = buf[p + 4];
		s.type = flags & 63;
		p += 5;
		cnt = buf[p] >> 5;
		if (cnt == 7) {
			if (p + 4 > n)
				return JB2_DAMAGED;
			cnt = (int)(r32(buf + p) & 0x1FFFFFFF);
			p += 4 + (cnt + 8) / 8;
		} else if (cnt > 4)
			return JB2_DAMAGED;
		else
			p++;
		rsize = s.num <= 256 ? 1 : s.num <= 65536 ? 2 : 4;
		if (cnt < 0 || cnt > 4096 || p + (long)cnt * rsize > n)
			return JB2_DAMAGED;
		if (cnt) {
			s.refs = malloc((size_t)cnt * sizeof *s.refs);
			if (!s.refs)
				return JB2_MEMORY;
		}
		for (i = 0; i < cnt; i++, p += rsize)
			s.refs[i] = rsize == 1 ? buf[p] : rsize == 2 ? r16(buf + p) : r32(buf + p);
		s.nrefs = cnt;
		p += flags & 64 ? 4 : 1;	/* the page's association */
		if (p + 4 > n) {
			free(s.refs);
			return JB2_DAMAGED;
		}
		len = (long)r32(buf + p);
		p += 4;
		if (len == (long)0xFFFFFFFFL || len < 0 || p + len > n) {
			if (s.type == 51 || s.type == 49) {	/* (a last segment cut short) */
				free(s.refs);
				break;
			}
			free(s.refs);
			return len == (long)0xFFFFFFFFL ? JB2_UNSUPPORTED : JB2_DAMAGED;
		}
		s.d = buf + p;
		s.n = len;
		p += len;
		if (dc->nseg == dc->aseg) {
			int na = dc->aseg ? dc->aseg * 2 : 16;
			SEG *ns = realloc(dc->seg, (size_t)na * sizeof *ns);

			if (!ns) {
				free(s.refs);
				return JB2_MEMORY;
			}
			dc->seg = ns;
			dc->aseg = na;
		}
		dc->seg[dc->nseg++] = s;
	}
	return JB2_OK;
}

static SEG *find(DEC *dc, uint32_t num)
{
	int i;

	for (i = 0; i < dc->nseg; i++)
		if (dc->seg[i].num == num)
			return &dc->seg[i];
	return NULL;
}

/* the symbols of the dictionaries s refers to, in order: an array of
   pointers (*n of them), freed by the caller */
static JB2BM **in_symbols(DEC *dc, SEG *s, int *n)
{
	int i, k, t = 0;
	JB2BM **v;

	for (i = 0; i < s->nrefs; i++) {
		SEG *r = find(dc, s->refs[i]);

		if (r && r->type == 0)
			t += r->nsyms;
	}
	*n = t;
	v = malloc((size_t)(t ? t : 1) * sizeof *v);
	if (!v)
		return NULL;
	t = 0;
	for (i = 0; i < s->nrefs; i++) {
		SEG *r = find(dc, s->refs[i]);

		if (r && r->type == 0)
			for (k = 0; k < r->nsyms; k++)
				v[t++] = r->syms[k];
	}
	return v;
}

static int log2up(uint32_t n)
{
	int l = 0;

	while ((1u << l) < n)
		l++;
	return l;
}

/* ------------------------------------------------------------------ text region (6.4) */

typedef struct {
	IAX dt, fs, ds, it, ri, rdw, rdh, rdx, rdy;
	uint8_t *id;			/* 1 << idlen */
	int idlen;
	uint8_t *gr;			/* refinement contexts, 8192 */
} TXCX;

typedef struct {
	int strips, refine, refcorner, transposed, combop, defpixel, dsoffset, rtemplate;
	int8_t rat[4];
	uint32_t ninst;
} TXP;

static int txcx_new(TXCX *c, int idlen)
{
	memset(c, 0, sizeof *c);
	c->idlen = idlen;
	c->id = calloc((size_t)1 << idlen, 1);
	c->gr = calloc(8192, 1);
	return c->id && c->gr;
}

static void txcx_free(TXCX *c)
{
	free(c->id);
	free(c->gr);
}

/* where a text region's symbols go: composed into region (a bitmap), or,
   with region NULL, kept as a list of placements for a page made a row
   at a time (a refined symbol's own bitmap with it) */
typedef struct {
	int x, y;
	const JB2BM *b;			/* a dictionary's symbol, or &own */
	JB2BM own;
} PLACE;

typedef struct {
	JB2BM *region;
	PLACE *pl;
	long n, an;
} PSINK;

/* ib at x, y: 0 for memory (ib let go if it was refined: own) */
static int place(PSINK *k, JB2BM *ib, int x, int y, int own, int combop)
{
	PLACE *p;

	if (k->region || !ib->d) {
		if (ib->d)
			compose(k->region, ib, x, y, combop);
		if (own)
			jb2_bm_free(ib);
		return 1;
	}
	if (k->n == k->an) {
		long na = k->an ? k->an * 2 : 64;
		PLACE *np = realloc(k->pl, (size_t)na * sizeof *np);

		if (!np) {
			if (own)
				jb2_bm_free(ib);
			return 0;
		}
		k->pl = np;
		k->an = na;
	}
	p = &k->pl[k->n++];
	p->x = x;
	p->y = y;
	if (own) {
		p->own = *ib;
		p->b = NULL;		/* (&p->own: the array may move) */
	} else {
		p->own.d = NULL;
		p->b = ib;
	}
	return 1;
}

static const JB2BM *place_bm(const PLACE *p)
{
	return p->b ? p->b : &p->own;
}

/* a text region's coordinates are sums of numbers the stream gives, each
   as big as 2^31: any beyond 2^24 (no page is near it) is damage, and the
   sums can't overflow (fuzz_jbig2 found one) */
#define TR_LIM (1L << 24)
#define TR_OUT(v) ((v) > TR_LIM || (v) < -TR_LIM)

static int text_region(MQ *m, TXCX *c, const TXP *tp, PSINK *sink, JB2BM **syms, int nsyms, int (*poll)(void))
{
	int32_t stript, firsts = 0, dt, curs, v;
	uint32_t ninst = 0;
	enum { BL, TL, BR, TR };

	if (ia_int(m, &c->dt, &stript) || TR_OUT(stript))
		return JB2_DAMAGED;
	work(0, (long)tp->ninst);
	stript *= -tp->strips;
	while (ninst < tp->ninst) {
		int first = 1;

		if (ia_int(m, &c->dt, &dt) || TR_OUT(dt))
			return JB2_DAMAGED;
		stript += dt * tp->strips;
		if (TR_OUT(stript))
			return JB2_DAMAGED;
		for (;;) {
			int32_t curt = 0, t, x, y;
			uint32_t id;
			JB2BM *ib, rb;
			int ri = 0, wi, hi, own = 0;

			if (first) {
				if (ia_int(m, &c->fs, &v) || TR_OUT(v))
					return JB2_DAMAGED;
				firsts += v;
				if (TR_OUT(firsts))
					return JB2_DAMAGED;
				curs = firsts;
				first = 0;
			} else {
				if (ia_int(m, &c->ds, &v))
					break;		/* OOB: the strip's end (read after the last too) */
				if (ninst >= tp->ninst || TR_OUT(v))
					return JB2_DAMAGED;
				curs += v + tp->dsoffset;
			}
			if ((tp->strips != 1 && ia_int(m, &c->it, &curt)) || TR_OUT(curs) || TR_OUT(curt))
				return JB2_DAMAGED;
			t = stript + curt;
			id = ia_id(m, c->id, c->idlen);
			if (id >= (uint32_t)nsyms)
				return JB2_DAMAGED;
			if (tp->refine && ia_int(m, &c->ri, &v) == 0)
				ri = v;
			ib = syms[id];
			if (ri) {
				int32_t rdw, rdh, rdx, rdy;

				if (ia_int(m, &c->rdw, &rdw) || ia_int(m, &c->rdh, &rdh) || ia_int(m, &c->rdx, &rdx) ||
				    ia_int(m, &c->rdy, &rdy) || TR_OUT(rdw) || TR_OUT(rdh) || TR_OUT(rdx) || TR_OUT(rdy) ||
				    ib->w + rdw < 0 || ib->w + rdw > 65535 || ib->h + rdh < 0 || ib->h + rdh > 65535)
					return JB2_DAMAGED;
				if (!bm_new(&rb, ib->w + rdw, ib->h + rdh, 0))
					return JB2_MEMORY;
				refine(m, c->gr, &rb, ib, (rdw >= 0 ? rdw / 2 : -((-rdw + 1) / 2)) + rdx,
				       (rdh >= 0 ? rdh / 2 : -((-rdh + 1) / 2)) + rdy, tp->rtemplate, tp->rat, 0);
				ib = &rb;
				own = 1;
			}
			wi = ib->w;
			hi = ib->h;
			if (!tp->transposed && (tp->refcorner == TR || tp->refcorner == BR))
				curs += wi - 1;
			else if (tp->transposed && (tp->refcorner == BL || tp->refcorner == BR))
				curs += hi - 1;
			if (!tp->transposed) {
				x = tp->refcorner == TR || tp->refcorner == BR ? curs - wi + 1 : curs;
				y = tp->refcorner == BL || tp->refcorner == BR ? t - hi + 1 : t;
			} else {
				x = tp->refcorner == TR || tp->refcorner == BR ? t - wi + 1 : t;
				y = tp->refcorner == BL || tp->refcorner == BR ? curs - hi + 1 : curs;
			}
			if (!place(sink, ib, x, y, own, tp->combop))
				return JB2_MEMORY;
			if (!tp->transposed && (tp->refcorner == TL || tp->refcorner == BL))
				curs += wi - 1;
			else if (tp->transposed && (tp->refcorner == TL || tp->refcorner == TR))
				curs += hi - 1;
			ninst++;
			if ((ninst & 15) == 0)
				work(16, 0);
			if ((ninst & 15) == 0 && poll && !poll())	/* (every 16: an instance is about 1 ms on Falcon-4) */
				return JB2_STOPPED;
		}
	}
	return JB2_OK;
}

/* ------------------------------------------------------------------ symbol dictionary (6.5) */

static int symbol_dict(DEC *dc, SEG *s)
{
	const uint8_t *d = s->d;
	long p = 2;
	unsigned fl;
	int huff, refagg, tmpl, rtmpl, i, r = JB2_OK, nin, idlen;
	int8_t at[8], rat[4];
	uint32_t nex, nnew, ndec = 0;
	int32_t hcheight = 0;
	JB2BM **in, *nw = NULL;
	MQ m;
	IAX *dh = NULL, *dw = NULL, *ex = NULL, *ai = NULL;
	uint8_t *gb = NULL;
	TXCX tc;			/* a refinement's and an aggregate's decoders (6.5.8.2) */
	int have_tc = 0;

	if (s->n < 2)
		return JB2_DAMAGED;
	fl = r16(d);
	huff = fl & 1;
	refagg = (fl >> 1) & 1;
	tmpl = (fl >> 10) & 3;
	rtmpl = (fl >> 12) & 1;
	if (huff)
		return JB2_UNSUPPORTED;
	memcpy(at, at_default[tmpl], 8);
	for (i = 0; i < (tmpl == 0 ? 8 : 2); i++)
		at[i] = (int8_t)d[p++];
	if (refagg && rtmpl == 0)
		for (i = 0; i < 4; i++)
			rat[i] = (int8_t)d[p++];
	else
		memset(rat, -1, sizeof rat);
	if (p + 8 > s->n)
		return JB2_DAMAGED;
	nex = r32(d + p);
	nnew = r32(d + p + 4);
	p += 8;
	if (nnew > 100000 || nex > 200000)
		return JB2_DAMAGED;
	in = in_symbols(dc, s, &nin);
	if (!in)
		return JB2_MEMORY;
	idlen = log2up((uint32_t)nin + nnew);
	nw = calloc(nnew ? nnew : 1, sizeof *nw);
	work(0, (long)nnew);
	dh = calloc(1, sizeof *dh);
	dw = calloc(1, sizeof *dw);
	ex = calloc(1, sizeof *ex);
	ai = calloc(1, sizeof *ai);
	gb = calloc(65536, 1);
	if (!nw || !dh || !dw || !ex || !ai || !gb) {
		r = JB2_MEMORY;
		goto out;
	}
	if (refagg) {
		have_tc = 1;
		if (!txcx_new(&tc, idlen)) {
			r = JB2_MEMORY;
			goto out;
		}
	}
	mq_init(&m, d + p, s->n - p);
	while (ndec < nnew) {
		int32_t hcdh, symw = 0, dwv;

		if (ia_int(&m, dh, &hcdh)) {
			r = JB2_DAMAGED;
			goto out;
		}
		hcheight += hcdh;
		if (hcheight < 0 || hcheight > 65535) {
			r = JB2_DAMAGED;
			goto out;
		}
		for (;;) {
			JB2BM *b = &nw[ndec];

			if (ia_int(&m, dw, &dwv))
				break;		/* OOB: the height class's end */
			if (ndec >= nnew) {
				r = JB2_DAMAGED;
				goto out;
			}
			symw += dwv;
			if (symw < 0 || symw > 65535) {
				r = JB2_DAMAGED;
				goto out;
			}
			if ((dc->sym_bytes += (long)((symw + 7) >> 3) * hcheight) > JB2_DICT_BYTES) {
				r = JB2_MEMORY;
				goto out;
			}
			if ((r = bm_new_turns(b, symw, hcheight, dc->poll)) <= 0) {
				r = r ? JB2_STOPPED : JB2_MEMORY;
				goto out;
			}
			r = JB2_OK;
			if (!refagg) {
				if (b->d && (r = generic_arith(&m, gb, b, tmpl, 0, at, dc->poll)) != JB2_OK)
					goto out;
			} else {
				int32_t ninst;

				if (ia_int(&m, ai, &ninst) || ninst < 1) {
					r = JB2_DAMAGED;
					goto out;
				}
				if (ninst == 1) {
					uint32_t id = ia_id(&m, tc.id, idlen);
					int32_t x, y;
					JB2BM *ref;

					if (ia_int(&m, &tc.rdx, &x) || ia_int(&m, &tc.rdy, &y) || id >= (uint32_t)nin + ndec) {
						r = JB2_DAMAGED;
						goto out;
					}
					ref = id < (uint32_t)nin ? in[id] : &nw[id - nin];
					if (b->d)
						refine(&m, tc.gr, b, ref, x, y, rtmpl, rat, 0);
				} else {
					/* an aggregate: a text region of the symbols so far */
					JB2BM **all;
					TXP tp;
					uint32_t k;

					all = malloc(((size_t)nin + ndec + 1) * sizeof *all);
					if (!all) {
						r = JB2_MEMORY;
						goto out;
					}
					for (k = 0; k < (uint32_t)nin; k++)
						all[k] = in[k];
					for (k = 0; k < ndec; k++)
						all[nin + k] = &nw[k];
					memset(&tp, 0, sizeof tp);
					tp.strips = 1;
					tp.refine = 1;
					tp.refcorner = 1;	/* TOPLEFT */
					tp.rtemplate = rtmpl;
					memcpy(tp.rat, rat, 4);
					tp.ninst = (uint32_t)ninst;
					if (b->d) {
						PSINK k;

						memset(&k, 0, sizeof k);
						k.region = b;
						r = text_region(&m, &tc, &tp, &k, all, nin + (int)ndec, dc->poll);
					}
					free(all);
					if (r != JB2_OK)
						goto out;
				}
			}
			ndec++;
			work(1, 0);
			/* (a turn a symbol: sti's dictionary of 2,000 ran 1.9 s on
			   Falcon-4 between its height classes' turns) */
			if (dc->poll && !dc->poll()) {
				r = JB2_STOPPED;
				goto out;
			}
		}
		if (dc->poll && !dc->poll()) {
			r = JB2_STOPPED;
			goto out;
		}
	}
	/* the exported symbols (6.5.10) */
	{
		uint32_t total = (uint32_t)nin + nnew, k = 0, got = 0, runs = 0;
		int cur = 0;
		uint8_t *flag = calloc(total ? total : 1, 1);

		if (!flag) {
			r = JB2_MEMORY;
			goto out;
		}
		while (k < total) {
			int32_t run;

			/* (no more runs than symbols and two: past the data's end the
			   coder gives runs of 0 for ever, fuzz_jbig2) */
			if (ia_int(&m, ex, &run) || run < 0 || k + (uint32_t)run > total || ++runs > total + 2) {
				free(flag);
				r = JB2_DAMAGED;
				goto out;
			}
			if (cur)
				memset(flag + k, 1, (size_t)run);
			k += (uint32_t)run;
			cur = !cur;
		}
		s->syms = calloc(nex ? nex : 1, sizeof *s->syms);
		if (!s->syms) {
			free(flag);
			r = JB2_MEMORY;
			goto out;
		}
		for (k = 0; k < total && got < nex; k++)
			if (flag[k])
				s->syms[got++] = k < (uint32_t)nin ? in[k] : &nw[k - nin];
		/* the new ones not exported let go; the rest kept with the dictionary */
		for (k = 0; k < nnew; k++)
			if (!flag[nin + k])
				jb2_bm_free(&nw[k]);
		s->own = nw;
		s->nown = nnew;
		nw = NULL;
		s->nsyms = (int)got;
#ifdef JB2_STATS
		{ long tot = 0, i2; for (i2 = 0; i2 < (long)nnew; i2++) tot += s->own[i2].d ? s->own[i2].stride * (long)s->own[i2].h : 0;
		  fprintf(stderr, "dict: %u new, %u exported, %ld bytes own\n", nnew, got, tot); }
#endif
		free(flag);
	}
out:
	if (nw)
		for (ndec = 0; ndec < nnew; ndec++)
			jb2_bm_free(&nw[ndec]);
	free(nw);
	free(in);
	free(dh);
	free(dw);
	free(ex);
	free(ai);
	free(gb);
	if (have_tc)
		txcx_free(&tc);
	return r;
}

/* ------------------------------------------------------------------ regions onto the page */

typedef struct {
	int w, h, x, y, op;
} RINFO;

static int region_info(const SEG *s, RINFO *ri)
{
	if (s->n < 17)
		return 0;
	ri->w = (int)r32(s->d);
	ri->h = (int)r32(s->d + 4);
	ri->x = (int)r32(s->d + 8);
	ri->y = (int)r32(s->d + 12);
	ri->op = s->d[16] & 7;
	return ri->w >= 0 && ri->h >= 0 && ri->w < 65536 && ri->h < 65536 &&
	       (long)((ri->w + 7) >> 3) * ri->h <= JB2_MAX_BYTES;
}

/* the page grown to hold rows to y (a page of unknown height, striped) */
static int page_grow(DEC *dc, int y)
{
	JB2BM *p = &dc->page;
	uint8_t *nd;

	if (dc->page_h_known || y <= p->h)
		return 1;
	/* (a page 0 bytes wide: realloc(d, 0) would free it, fuzz_jbig2; past
	   JB2_MAX_BYTES, or a sum that wrapped: stride * y may not fit a
	   68000's size_t) */
	if (p->stride <= 0 || y < 0 || (long)y > JB2_MAX_BYTES / p->stride)
		return 0;
	nd = realloc(p->d, (size_t)p->stride * (size_t)y);
	if (!nd)
		return 0;
	memset(nd + (size_t)p->stride * (size_t)p->h, 0, (size_t)p->stride * (size_t)(y - p->h));
	p->d = nd;
	p->h = y;
	return 1;
}

static int put_region(DEC *dc, const RINFO *ri, JB2BM *b)
{
	if (!dc->page.d && dc->page.w == 0)
		return JB2_DAMAGED;		/* (no page information) */
	if (!page_grow(dc, ri->y + ri->h))
		return JB2_MEMORY;
	compose(&dc->page, b, ri->x, ri->y, ri->op);
	return JB2_OK;
}


/* a generic region's header (7.4.6): 0, or a reason; *p its data's start */
static int gen_params(const SEG *s, RINFO *ri, int *mmr, int *t, int *tpgdon, int8_t *at, long *p)
{
	int fl, i;

	*p = 18;
	if (!region_info(s, ri) || s->n < 18)
		return JB2_DAMAGED;
	fl = s->d[17];
	*mmr = fl & 1;
	*t = (fl >> 1) & 3;
	*tpgdon = (fl >> 3) & 1;
	memcpy(at, at_default[*t], 8);
	if (!*mmr) {
		if (*p + (*t == 0 ? 8 : 2) > s->n)
			return JB2_DAMAGED;
		for (i = 0; i < (*t == 0 ? 8 : 2); i++)
			at[i] = (int8_t)s->d[(*p)++];
	}
	return JB2_OK;
}

static int generic_region(DEC *dc, SEG *s)
{
	RINFO ri;
	JB2BM b;
	int mmr, t, tpgdon, r;
	int8_t at[8];
	long p;

	if ((r = gen_params(s, &ri, &mmr, &t, &tpgdon, at, &p)) != JB2_OK)
		return r;
	T0(ta);
	if (!bm_new(&b, ri.w, ri.h, 0))
		return JB2_MEMORY;
	TADD(jb2_t_alloc, ta);
	if (!b.d)
		return JB2_OK;
	T0(td);
	if (mmr)
		r = generic_mmr(s->d + p, s->n - p, &b);
	else {
		uint8_t *cx = calloc(65536, 1);
		MQ m;

		if (!cx) {
			jb2_bm_free(&b);
			return JB2_MEMORY;
		}
		mq_init(&m, s->d + p, s->n - p);
		r = generic_arith(&m, cx, &b, t, tpgdon, at, dc->poll);
		free(cx);
	}
	TADD(jb2_t_decode, td);
	T0(tp);
	if (r == JB2_OK)
		r = put_region(dc, &ri, &b);
	TADD(jb2_t_put, tp);
	jb2_bm_free(&b);
	return r;
}

/* a text region's header (7.4.3): 0, or a reason; *p its data's start */
static int txt_params(const SEG *s, RINFO *ri, TXP *tp, long *p)
{
	unsigned fl;
	int i;

	*p = 17;
	if (!region_info(s, ri) || s->n < 19)
		return JB2_DAMAGED;
	fl = r16(s->d + *p);
	*p += 2;
	if (fl & 1)
		return JB2_UNSUPPORTED;		/* SBHUFF */
	memset(tp, 0, sizeof *tp);
	tp->refine = (fl >> 1) & 1;
	tp->strips = 1 << ((fl >> 2) & 3);
	tp->refcorner = (fl >> 4) & 3;
	tp->transposed = (fl >> 6) & 1;
	tp->combop = (fl >> 7) & 3;
	tp->defpixel = (fl >> 9) & 1;
	tp->dsoffset = (fl >> 10) & 31;
	if (tp->dsoffset > 15)
		tp->dsoffset -= 32;
	tp->rtemplate = (fl >> 15) & 1;
	memset(tp->rat, -1, sizeof tp->rat);
	if (tp->refine && tp->rtemplate == 0) {
		if (*p + 4 > s->n)
			return JB2_DAMAGED;
		for (i = 0; i < 4; i++)
			tp->rat[i] = (int8_t)s->d[(*p)++];
	}
	if (*p + 4 > s->n)
		return JB2_DAMAGED;
	tp->ninst = r32(s->d + *p);
	*p += 4;
	return JB2_OK;
}

static int text_segment(DEC *dc, SEG *s)
{
	RINFO ri;
	TXP tp;
	TXCX c;
	JB2BM b, **syms;
	long p;
	int nsyms, r;
	MQ m;

	if ((r = txt_params(s, &ri, &tp, &p)) != JB2_OK)
		return r;
	syms = in_symbols(dc, s, &nsyms);
	if (!syms)
		return JB2_MEMORY;
	if (!txcx_new(&c, log2up((uint32_t)nsyms))) {
		txcx_free(&c);
		free(syms);
		return JB2_MEMORY;
	}
	if (!bm_new(&b, ri.w, ri.h, tp.defpixel)) {
		txcx_free(&c);
		free(syms);
		return JB2_MEMORY;
	}
	r = JB2_OK;
	if (b.d) {
		PSINK k;

		memset(&k, 0, sizeof k);
		k.region = &b;
		mq_init(&m, s->d + p, s->n - p);
		r = text_region(&m, &c, &tp, &k, syms, nsyms, dc->poll);
		if (r == JB2_OK)
			r = put_region(dc, &ri, &b);
	}
	jb2_bm_free(&b);
	txcx_free(&c);
	free(syms);
	return r;
}

/* an immediate refinement region: the page's part under it refined */
static int refine_segment(DEC *dc, SEG *s)
{
	RINFO ri;
	JB2BM ref, b;
	int fl, t, tpgron, i, y, r;
	int8_t at[4];
	long p = 18;
	uint8_t *cx;
	MQ m;

	if (!region_info(s, &ri) || s->n < 18)
		return JB2_DAMAGED;
	if (s->nrefs)
		return JB2_UNSUPPORTED;		/* (refining an intermediate region) */
	fl = s->d[17];
	t = fl & 1;
	tpgron = (fl >> 1) & 1;
	memset(at, -1, sizeof at);
	if (t == 0) {
		if (p + 4 > s->n)
			return JB2_DAMAGED;
		for (i = 0; i < 4; i++)
			at[i] = (int8_t)s->d[p++];
	}
	if (!dc->page.d && dc->page.w == 0)
		return JB2_DAMAGED;		/* (no page information) */
	if (!page_grow(dc, ri.y + ri.h) || !bm_new(&ref, ri.w, ri.h, 0))
		return JB2_MEMORY;
	if (!ref.d)
		return JB2_OK;
	for (y = 0; y < ri.h; y++)
		for (i = 0; i < ri.w; i++)
			if (px(&dc->page, ri.x + i, ri.y + y))
				setpx(&ref, i, y, 1);
	if (!bm_new(&b, ri.w, ri.h, 0) || !(cx = calloc(8192, 1))) {
		jb2_bm_free(&ref);
		jb2_bm_free(&b);
		return JB2_MEMORY;
	}
	mq_init(&m, s->d + p, s->n - p);
	r = refine(&m, cx, &b, &ref, 0, 0, t, at, tpgron);
	ri.op = 4;			/* (it takes the page's place) */
	if (r == JB2_OK)
		r = put_region(dc, &ri, &b);
	free(cx);
	jb2_bm_free(&b);
	jb2_bm_free(&ref);
	return r;
}

static int page_info(DEC *dc, SEG *s)
{
	uint32_t w, h;
	int fl;

	if (s->n < 19)
		return JB2_DAMAGED;
	w = r32(s->d);
	h = r32(s->d + 4);
	fl = s->d[16];
	if (w == 0 || w > 65535)
		return JB2_DAMAGED;
	jb2_bm_free(&dc->page);
	dc->def_op = (fl >> 3) & 3;
	if (h == 0xFFFFFFFFu) {
		dc->page.w = (int)w;
		dc->page.h = 0;
		dc->page.stride = ((int)w + 7) >> 3;
		dc->page.d = NULL;
		dc->page_h_known = 0;
		return JB2_OK;
	}
	if (h > 65535)
		return JB2_DAMAGED;
	if ((long)((w + 7) >> 3) * (long)h > JB2_MAX_BYTES)
		return JB2_MEMORY;
	dc->page_h_known = 1;
	return bm_new(&dc->page, (int)w, (int)h, (fl >> 2) & 1) ? JB2_OK : JB2_MEMORY;
}

static int run(DEC *dc, int from)
{
	int i, r = JB2_OK;

	for (i = from; i < dc->nseg && r == JB2_OK; i++) {
		SEG *s = &dc->seg[i];

		switch (s->type) {
		case 0: {
			T0(t);
			r = symbol_dict(dc, s);
			TADD(jb2_t_dict, t);
			break;
		}
		case 4:
			r = JB2_OK;		/* (an intermediate text region: unused) */
			break;
		case 6:
		case 7: {
			T0(t);
			r = text_segment(dc, s);
			TADD(jb2_t_text, t);
			break;
		}
		case 36:
			r = JB2_OK;
			break;
		case 38:
		case 39:
			r = generic_region(dc, s);
			break;
		case 42:
		case 43:
			r = refine_segment(dc, s);
			break;
		case 48:
			r = page_info(dc, s);
			break;
		case 50:		/* end of stripe: the page so far */
			if (s->n >= 4 && !page_grow(dc, (int)r32(s->d) + 1))
				r = JB2_MEMORY;
			break;
		case 16:
		case 20:
		case 22:
		case 23:
			r = JB2_UNSUPPORTED;	/* pattern dictionary, halftone */
			break;
		case 53:
			r = JB2_OK;		/* (tables: for the Huffman coder) */
			break;
		default:
			break;		/* end of page, end of file, profiles, extensions */
		}
	}
	return r;
}

/* ------------------------------------------------------------------ the page a row at a time */
/*
 * A page whose regions are generic ones (their AT pixels within the two
 * rows above) and text regions is made a row at a time, never whole (a
 * text region's operators, OR, AND, XOR and XNOR, give the same in any
 * order: its symbols on a row are put in as they come): a generic region's decoder goes a row along with
 * three rows of its own (the archive.org text pages' masks: 1656 x 2527,
 * 520 KB whole, 1 KB so); a text region's symbols are kept as placements
 * (where each goes) and drawn into the row being made. Any other page
 * (refinement regions, stripes of unknown height) is decoded whole first,
 * as before, and its rows handed on from that.
 */
typedef struct {
	int kind;			/* 1 generic (arithmetic), 2 generic (MMR), 3 text */
	RINFO ri;
	long y;				/* its next row */
	GEN g;
	uint8_t *cx;
	JB2BM ring;			/* a generic region's last three rows */
	uint8_t *zr;			/* (and zeros above its first) */
	CCITT cc;
	int cc_open;
	PSINK k;			/* a text region's placements */
	long *ord;			/* by their top rows */
	long next, *act, nact;
	int defpix, combop;
	JB2BM row;			/* the row made */
} REG;

struct JB2 {
	DEC dc;
	int stream;
	REG *reg;
	int nreg;
	long y;
	int w, h, defpix;
	int err;
};

/* ord (n) by the placements' top rows, kept in their order where alike (a
   shell sort: no qsort's statics) */
/* ord: the placements' indices by y, a tie by index (a merge sort,
   stable: O(n log n) whatever order they came in, and a turn every pass,
   shut-002's thousands were 107 ms without one); 0 when poll says stop or
   memory is refused */
static int sort_ord(const PLACE *pl, long *ord, long n, int (*poll)(void))
{
	long w, i, *t, *a = ord, *b;

	for (i = 0; i < n; i++)
		ord[i] = i;
	if (n < 2)
		return 1;
	if (!(t = malloc((size_t)n * sizeof *t)))
		return 0;
	b = t;
	for (w = 1; w < n; w *= 2) {
		long lo;

		for (lo = 0; lo < n; lo += 2 * w) {
			long mid = lo + w < n ? lo + w : n, hi = lo + 2 * w < n ? lo + 2 * w : n, x = lo, y = mid, k = lo;

			while (x < mid && y < hi)
				b[k++] = pl[a[y]].y < pl[a[x]].y ? a[y++] : a[x++];
			while (x < mid)
				b[k++] = a[x++];
			while (y < hi)
				b[k++] = a[y++];
		}
		{
			long *c = a;

			a = b;
			b = c;
		}
		if (poll && !poll()) {
			free(t);
			return 0;
		}
	}
	if (a != ord)
		memcpy(ord, a, (size_t)n * sizeof *ord);
	free(t);
	return 1;
}

static void reg_free(REG *g)
{
	long i;

	free(g->cx);
	jb2_bm_free(&g->ring);
	free(g->zr);
	if (g->cc_open)
		ccitt_close(&g->cc);
	for (i = 0; i < g->k.n; i++)
		jb2_bm_free(&g->k.pl[i].own);
	free(g->k.pl);
	free(g->ord);
	free(g->act);
	jb2_bm_free(&g->row);
}

/* the region's next row into g->row */
static void reg_row(REG *g)
{
	JB2BM *o = &g->row;

	if (g->kind == 1) {
		long st = g->ring.stride;
		int yy = g->y < 2 ? (int)g->y : 2;

		if (g->y > 2)
			memmove(g->ring.d, g->ring.d + st, (size_t)(2 * st));
		gen_row(&g->g, &g->ring, yy, g->zr);
		memcpy(o->d, g->ring.d + yy * st, (size_t)st);
	} else if (g->kind == 2) {
		if (!g->cc_open || !ccitt_row(&g->cc, o->d))
			memset(o->d, 0, (size_t)o->stride);		/* (the rest white) */
	} else {
		long i;

		memset(o->d, g->defpix ? 0xFF : 0, (size_t)o->stride);
		while (g->next < g->k.n && g->k.pl[g->ord[g->next]].y <= g->y)
			g->act[g->nact++] = g->ord[g->next++];
		for (i = 0; i < g->nact;) {
			const PLACE *p = &g->k.pl[g->act[i]];
			const JB2BM *b = place_bm(p);
			JB2BM v;

			if (g->y >= p->y + b->h) {
				g->act[i] = g->act[--g->nact];	/* (done with: the order doesn't matter) */
				continue;
			}
			v.w = b->w;
			v.h = 1;
			v.stride = b->stride;
			v.d = b->d + (g->y - p->y) * b->stride;
			compose(o, &v, p->x, 0, g->combop);
			i++;
		}
	}
	g->y++;
}

/* can this page be made a row at a time? (see above) */
static int streamable(DEC *dc)
{
	int i, page = 0;

	for (i = 0; i < dc->nseg; i++) {
		SEG *s = &dc->seg[i];
		RINFO ri;

		switch (s->type) {
		case 48:
			if (page++ || s->n < 19 || r32(s->d + 4) == 0xFFFFFFFFu)
				return 0;	/* (a second page, or one of unknown height) */
			break;
		case 38:
		case 39: {
			int mmr, t, tp, k;
			int8_t at[8];
			long p;

			if (gen_params(s, &ri, &mmr, &t, &tp, at, &p) != JB2_OK)
				return 0;
			for (k = 0; k < (t == 0 ? 4 : 1); k++)
				if (at[2 * k + 1] < -2)
					return 0;
			break;
		}
		case 6:
		case 7: {
			TXP tp;
			long p;

			if (txt_params(s, &ri, &tp, &p) != JB2_OK)
				return 0;
			break;
		}
		case 0:
		case 4:
		case 36:
		case 49:
		case 51:
		case 52:
		case 53:
		case 62:
			break;
		default:
			return 0;		/* refinement, halftone, stripes...: whole */
		}
	}
	return page == 1;
}

/* the page's regions set up (a text region's placements decoded now), its
   dictionaries decoded on the way, in the segments' order */
static int stream_open(JB2 *j)
{
	DEC *dc = &j->dc;
	int i, r = JB2_OK;

	j->reg = calloc((size_t)dc->nseg + 1, sizeof *j->reg);
	if (!j->reg)
		return JB2_MEMORY;
	for (i = 0; i < dc->nseg && r == JB2_OK; i++) {
		SEG *s = &dc->seg[i];
		REG *g = &j->reg[j->nreg];

		switch (s->type) {
		case 0: {
			T0(t);
			r = symbol_dict(dc, s);
			TADD(jb2_t_dict, t);
			break;
		}
		case 48:
			j->w = (int)r32(s->d);
			j->h = (int)r32(s->d + 4);
			j->defpix = (s->d[16] >> 2) & 1;
			if (j->w <= 0 || j->w > 65535 || j->h <= 0 || j->h > 65535)
				r = JB2_DAMAGED;
			else if ((long)((j->w + 7) >> 3) * j->h > JB2_MAX_BYTES)
				r = JB2_MEMORY;
			break;
		case 38:
		case 39: {
			int mmr, t, tp;
			int8_t at[8];
			long p;

			if ((r = gen_params(s, &g->ri, &mmr, &t, &tp, at, &p)) != JB2_OK)
				break;
			j->nreg++;
			if (g->ri.w == 0 || g->ri.h == 0) {
				j->nreg--;
				break;
			}
			if (!bm_new(&g->row, g->ri.w, 1, 0)) {
				r = JB2_MEMORY;
				break;
			}
			if (mmr) {
				g->kind = 2;
				if (ccitt_open(&g->cc, s->d + p, s->n - p, g->ri.w, -1, 0)) {
					r = JB2_MEMORY;
					break;
				}
				g->cc_open = 1;
			} else {
				g->kind = 1;
				g->cx = calloc(65536, 1);
				g->zr = calloc((size_t)g->row.stride + 2, 1);
				if (!g->cx || !g->zr || !bm_new(&g->ring, g->ri.w, 3, 0)) {
					r = JB2_MEMORY;
					break;
				}
				mq_init(&g->g.m, s->d + p, s->n - p);
				gen_init(&g->g, g->cx, t, tp, at);
			}
			break;
		}
		case 6:
		case 7: {
			TXP tp;
			TXCX c;
			JB2BM **syms;
			int nsyms;
			long p;
			MQ m;
			T0(tt);

			if ((r = txt_params(s, &g->ri, &tp, &p)) != JB2_OK)
				break;
			j->nreg++;
			g->kind = 3;
			g->defpix = tp.defpixel;
			g->combop = tp.combop;
			if (!bm_new(&g->row, g->ri.w, 1, 0)) {
				r = JB2_MEMORY;
				break;
			}
			if (g->ri.w == 0 || g->ri.h == 0)
				break;
			syms = in_symbols(dc, s, &nsyms);
			if (!syms) {
				r = JB2_MEMORY;
				break;
			}
			if (!txcx_new(&c, log2up((uint32_t)nsyms))) {
				txcx_free(&c);
				free(syms);
				r = JB2_MEMORY;
				break;
			}
			mq_init(&m, s->d + p, s->n - p);
			r = text_region(&m, &c, &tp, &g->k, syms, nsyms, dc->poll);
			txcx_free(&c);
			free(syms);
			TADD(jb2_t_text, tt);
			if (r != JB2_OK)
				break;
			g->ord = malloc((size_t)(g->k.n ? g->k.n : 1) * sizeof *g->ord);
			g->act = malloc((size_t)(g->k.n ? g->k.n : 1) * sizeof *g->act);
			if (!g->ord || !g->act) {
				r = JB2_MEMORY;
				break;
			}
			if (!sort_ord(g->k.pl, g->ord, g->k.n, dc->poll)) {
				r = dc->poll && !dc->poll() ? JB2_STOPPED : JB2_MEMORY;
				break;
			}
			break;
		}
		default:
			break;
		}
	}
	if (r == JB2_OK && j->w == 0)
		r = JB2_DAMAGED;
	return r;
}

JB2 *jb2_open(const uint8_t *globals, long gn, const uint8_t *data, long n, int (*poll)(void), int *err)
{
	JB2 *j = calloc(1, sizeof *j);
	int r;

	if (!j) {
		*err = JB2_MEMORY;
		return NULL;
	}
	j->dc.poll = poll;
	work_done = work_of = 0;
	r = globals && gn > 0 ? read_segments(&j->dc, globals, gn) : JB2_OK;
	if (r == JB2_OK)
		r = read_segments(&j->dc, data, n);
	if (r == JB2_OK) {
		j->stream = !jb2_whole_page && streamable(&j->dc);
		if (poll && !poll())
			r = JB2_STOPPED;
		else if (j->stream)
			r = stream_open(j);
		else {
			r = run(&j->dc, 0);
			if (r == JB2_OK && !j->dc.page.d)
				r = JB2_DAMAGED;
			j->w = j->dc.page.w;
			j->h = j->dc.page.h;
		}
	}
	if (r != JB2_OK) {
		jb2_close(j);
		*err = r;
		return NULL;
	}
	*err = JB2_OK;
	return j;
}

void jb2_size(const JB2 *j, int *w, int *h)
{
	*w = j->w;
	*h = j->h;
}

int jb2_row(JB2 *j, uint8_t *row)
{
	long st = ((long)j->w + 7) >> 3;
	int i;

	if (j->err || j->y >= j->h)
		return 0;
	if (!j->stream) {
		memcpy(row, j->dc.page.d + j->y * j->dc.page.stride, (size_t)st);
		j->y++;
		return 1;
	}
	{
		JB2BM pr;

		pr.w = j->w;
		pr.h = 1;
		pr.stride = st;
		pr.d = row;
		memset(row, j->defpix ? 0xFF : 0, (size_t)st);
		if (j->defpix && (j->w & 7))
			row[st - 1] &= (uint8_t)(0xFF << (8 - (j->w & 7)));
		for (i = 0; i < j->nreg; i++) {
			REG *g = &j->reg[i];

			if (j->y < g->ri.y || j->y >= (long)g->ri.y + g->ri.h)
				continue;
			reg_row(g);
			compose(&pr, &g->row, g->ri.x, 0, g->ri.op);
		}
	}
	j->y++;
	if (j->dc.poll && !j->dc.poll()) {	/* (every row: a 1,656-wide region's 4 were 50 ms on Falcon-4) */
		j->err = JB2_STOPPED;
		return 0;
	}
	return 1;
}

int jb2_error(const JB2 *j)
{
	return j->err;
}

void jb2_close(JB2 *j)
{
	int i;

	if (!j)
		return;
	for (i = 0; i < j->nreg; i++)
		reg_free(&j->reg[i]);
	free(j->reg);
	jb2_bm_free(&j->dc.page);
	for (i = 0; i < j->dc.nseg; i++) {
		int k;

		for (k = 0; k < (int)j->dc.seg[i].nown; k++)
			jb2_bm_free(&j->dc.seg[i].own[k]);
		free(j->dc.seg[i].own);
		free(j->dc.seg[i].syms);
		free(j->dc.seg[i].refs);
	}
	free(j->dc.seg);
	free(j);
}

int jb2_decode(const uint8_t *globals, long gn, const uint8_t *data, long n, JB2BM *page, int (*poll)(void))
{
	int r, y;
	JB2 *j = jb2_open(globals, gn, data, n, poll, &r);

	memset(page, 0, sizeof *page);
	if (!j)
		return r;
	if (!bm_new(page, j->w, j->h, 0)) {
		jb2_close(j);
		return JB2_MEMORY;
	}
	for (y = 0; y < j->h; y++)
		if (!jb2_row(j, page->d + (long)y * page->stride))
			break;
	r = y < j->h ? (j->err ? j->err : JB2_DAMAGED) : JB2_OK;
	jb2_close(j);
	if (r != JB2_OK)
		jb2_bm_free(page);
	return r;
}

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pens.c - a page drawn by render.ovl to the screen's 2 to 256 pens:
 * Floyd-Steinberg's error diffusion, each colour's nearest pen worked out
 * once (a table of 32 K, by its 15 bits) rather than searched for at
 * each pixel, and white kept white: a pixel of 0xFFFF is the white pen
 * and drops the error it is given, as black does (common's dither took 5-6-5's white as
 * 248, dotting a page's ground; and a page is mostly white, which costs
 * next to nothing here). The ordered dither of EConvert's pages moves all
 * three channels together and lost hues on TOS's 16 colours (the
 * showcase's navy band came out grey, 7 Nov 2026).
 *
 * A colour near one of the pens (a flat fill, a grey text's ink, a glyph's
 * edge) is that pen, its error dropped, as white and black are: diffused, the
 * few pens of 16 colours broke a dark red band into red, black and green dots
 * and a grey text into specks (Erez's photos of EDOS_01.PDF, 9 Nov 2026). Only
 * the colours between the pens (a photo, a gradient) are dithered.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "pens.h"

static short *inv;			/* 15-bit colour -> its nearest pen + 1 (0: not yet) */
static short *snap;			/* 15-bit colour -> its pen + 1 when near one, -1 not near, 0 not yet */
#define SNAP_D (9L * 28 * 28)		/* "near": 28 a channel, weighted as nearest() weighs */
static unsigned char pal_of[256][3];
static int n_of;
static short *err;			/* the next row's errors, r g b a pixel (w + 2 of them) */
static int err_w;
static long next_y = -1;		/* the row the errors are for */

static int nearest(const unsigned char (*pal)[3], int n, int r, int g, int b)
{
	int k, best = 0;
	long bd = -1;

	for (k = 0; k < n; k++) {
		int dr = r - pal[k][0], dg = g - pal[k][1], db = b - pal[k][2];
		long d = (long)dr * dr * 3 + (long)dg * dg * 4 + (long)db * db * 2;

		if (bd < 0 || d < bd) {
			bd = d;
			best = k;
		}
	}
	return best;
}

/* the pen colour c (5-6-5, before any error) is taken as, or -1 */
static int snap_pen(unsigned c, const unsigned char (*pal)[3], int n)
{
	int key = (int)((c >> 11) << 10 | ((c >> 6) & 31) << 5 | (c & 31));

	if (!snap[key]) {
		int r = (int)((c >> 11) << 3 | c >> 13), g = (int)(((c >> 5) & 63) << 2 | ((c >> 9) & 3));
		int b = (int)((c & 31) << 3 | ((c >> 2) & 7)), k = nearest(pal, n, r, g, b);
		int dr = r - pal[k][0], dg = g - pal[k][1], db = b - pal[k][2];
		long d = (long)dr * dr * 3 + (long)dg * dg * 4 + (long)db * db * 2;

		snap[key] = (short)(d < SNAP_D ? k + 1 : -1);
	}
	return snap[key] > 0 ? snap[key] - 1 : -1;
}

static int clamp(int v)
{
	return v < 0 ? 0 : v > 255 ? 255 : v;
}

/* each pen's four bits spread one to a byte (c2p): a long shifted in per
   pixel, its bytes the four planes' after eight */
static uint32_t c2p[16];

/* the error diffusion's state along a row (16ths): to the right, and the
   next row's sums for f[x - 1] and f[x] */
typedef struct {
	int cr, cg, cb, p0r, p0g, p0b, p1r, p1g, p1b;
} FS;

/* pixels x0 .. x0 + n - 1 (n at most 8) of a row: their pens into pn, the
   next row's errors into f. A function of its own, its loop small: the
   68030's instruction cache is 256 bytes (one loop doing all of it, white
   included, was 9 to 15 us a pixel on Falcon-4) */
static __attribute__((noinline)) void fs_run(const unsigned short *s, const short *e, short *f, int n, FS *st,
					      const unsigned char (*pal)[3], int np, int white, int black,
					      unsigned char *pn)
{
	FS t = *st;
	int i;

	for (i = 0; i < n; i++, e += 3, f += 3) {
		unsigned c = s[i];
		int r, g, b, k, key, er, eg, eb;

		if (c == 0xFFFF || c == 0) {
			/* white, or a glyph's black ink: its pen, its error dropped */
			pn[i] = (unsigned char)(c ? white : black);
			er = eg = eb = 0;
		} else if ((k = snap_pen(c, pal, np)) >= 0) {
			/* near a pen: that pen, its error dropped (a fill, a text's ink) */
			pn[i] = (unsigned char)k;
			er = eg = eb = 0;
		} else {
			r = clamp((int)((c >> 11) << 3 | c >> 13) + ((e[0] + t.cr) >> 4));
			g = clamp((int)(((c >> 5) & 63) << 2 | ((c >> 9) & 3)) + ((e[1] + t.cg) >> 4));
			b = clamp((int)((c & 31) << 3 | ((c >> 2) & 7)) + ((e[2] + t.cb) >> 4));
			key = (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3);
			if (!inv[key])
				inv[key] = (short)(nearest(pal, np, r | 4, g | 4, b | 4) + 1);
			k = inv[key] - 1;
			pn[i] = (unsigned char)k;
			er = r - pal[k][0];
			eg = g - pal[k][1];
			eb = b - pal[k][2];
		}
		/* 7/16 right, 3/16 down-left, 5/16 down, 1/16 down-right */
		t.cr = er * 7;
		t.cg = eg * 7;
		t.cb = eb * 7;
		f[-3] = (short)(t.p0r + er * 3);
		f[-2] = (short)(t.p0g + eg * 3);
		f[-1] = (short)(t.p0b + eb * 3);
		t.p0r = t.p1r + er * 5;
		t.p0g = t.p1g + eg * 5;
		t.p0b = t.p1b + eb * 5;
		t.p1r = er;
		t.p1g = eg;
		t.p1b = eb;
	}
	*st = t;
}

/* eight pens (fewer at the row's end: the rest 0) into byte x8 of each
   plane's row */
static void pack(const unsigned char *pn, int n, int planes, long rb, unsigned char *o)
{
	uint32_t acc = 0;
	int i, p;

	if (planes > 4) {
		/* 256 colours: a bit a plane a pixel (the four planes' long
		   below holds 4) */
		for (p = 0; p < planes; p++, o += rb) {
			unsigned b = 0;

			for (i = 0; i < 8; i++)
				b = b << 1 | (i < n ? (pn[i] >> p) & 1 : 0);
			*o = (unsigned char)b;
		}
		return;
	}
	for (i = 0; i < 8; i++)
		acc = acc << 1 | (i < n ? c2p[pn[i] & 15] : 0);
	for (p = 0; p < planes; p++, o += rb)
		*o = (unsigned char)(acc >> (24 - 8 * p));
}

int pens_rows(const unsigned short *px, int w, int rows, int ytop, const unsigned char (*pal)[3], int n,
	      unsigned char *out)
{
	return pens_planes(px, w, rows, ytop, pal, n, 0, out);
}

int pens_planes(const unsigned short *px, int w, int rows, int ytop, const unsigned char (*pal)[3], int n,
		int planes, unsigned char *out)
{
	int x, y, white, black, i, k;
	short *cur, *nxt;
	unsigned char *pn;
	long rb = (w + 15) / 16 * 2;

	if (n < 2 || n > 256 || w <= 0 || planes > 8)
		return -1;
	if (!c2p[15])
		for (i = 0; i < 16; i++)
			c2p[i] = (uint32_t)(i & 1) << 24 | (uint32_t)((i >> 1) & 1) << 16 | (uint32_t)((i >> 2) & 1) << 8 |
				 (uint32_t)((i >> 3) & 1);
	if (!inv && !(inv = malloc(32768 * sizeof *inv)))
		return -1;
	if (!snap && !(snap = malloc(32768 * sizeof *snap)))
		return -1;
	if (n != n_of || memcmp(pal_of, pal, (size_t)n * 3)) {
		memset(inv, 0, 32768 * sizeof *inv);
		memset(snap, 0, 32768 * sizeof *snap);
		memcpy(pal_of, pal, (size_t)n * 3);
		n_of = n;
	}
	if (!err || err_w != w) {
		free(err);
		err_w = 0;
		if (!(err = malloc((size_t)(w + 2) * 3 * 2 * sizeof *err)))
			return -1;
		err_w = w;
		next_y = -1;
	}
	if (!(pn = malloc((size_t)w + 8)))
		return -1;
	cur = err;
	nxt = err + (w + 2) * 3;
	if (ytop != next_y)
		memset(cur, 0, (size_t)(w + 2) * 3 * sizeof *cur);	/* a page's first row */
	white = nearest(pal, n, 255, 255, 255);
	black = nearest(pal, n, 0, 0, 0);
	for (y = 0; y < rows; y++) {
		const unsigned short *s = px + (long)y * w;
		unsigned char *po = planes > 0 ? out + (long)y * rb * planes : NULL;
		/* this row's errors (from the row above) at e[x]; the next row's
		   written once each at f[x - 1] */
		const short *e = cur + 3;
		short *f = nxt + 3;
		FS st;

		memset(&st, 0, sizeof st);
		x = 0;
		while (x < w) {
			int x0 = x;

			/* white eight at a time, read as four longs: a loop of a few
			   instructions; their planes' bytes, pens and errors after */
			if (!(((uintptr_t)(s + x)) & 3)) {
				const uint32_t *l = (const uint32_t *)(s + x);

				while (x + 8 <= w && (l[0] & l[1] & l[2] & l[3]) == 0xFFFFFFFFUL) {
					x += 8;
					l += 4;
				}
			}
			if (x > x0) {
				short *z = f + (x0 - 1) * 3;

				memset(pn + x0, white, (size_t)(x - x0));
				if (po)
					for (k = 0; k < planes; k++)
						memset(po + k * rb + (x0 >> 3), (white >> k) & 1 ? 0xFF : 0, (size_t)((x - x0) >> 3));
				z[0] = (short)st.p0r;
				z[1] = (short)st.p0g;
				z[2] = (short)st.p0b;
				z[3] = (short)st.p1r;
				z[4] = (short)st.p1g;
				z[5] = (short)st.p1b;
				memset(z + 6, 0, (size_t)(x - x0 - 2) * 3 * sizeof *z);	/* f[x0 + 1] .. f[x - 2] */
				memset(&st, 0, sizeof st);
				continue;
			}
			k = w - x < 8 ? w - x : 8;
			fs_run(s + x, e + x * 3, f + x * 3, k, &st, pal, n, white, black, pn + x);
			if (po)
				pack(pn + x, k, planes, rb, po + (x >> 3));
			x += k;
		}
		f[(w - 1) * 3] = (short)st.p0r;
		f[(w - 1) * 3 + 1] = (short)st.p0g;
		f[(w - 1) * 3 + 2] = (short)st.p0b;
		if (po && (w + 7) / 8 < rb)
			for (k = 0; k < planes; k++)
				po[k * rb + rb - 1] = 0;	/* (a row of an odd number of bytes: its last one) */
		if (!po)
			memcpy(out + (long)y * w, pn, (size_t)w);
		{
			short *t = cur;

			cur = nxt;
			nxt = t;
		}
	}
	/* (the next call's first row takes these errors: kept in err's first half) */
	if (cur != err)
		memcpy(err, cur, (size_t)(w + 2) * 3 * sizeof *err);
	free(pn);
	next_y = ytop + rows;
	return 0;
}

void pens_free(void)
{
	free(inv);
	free(snap);
	free(err);
	inv = NULL;
	snap = NULL;
	err = NULL;
	err_w = 0;
	n_of = 0;
	next_y = -1;
}

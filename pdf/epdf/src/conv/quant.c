/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * quant.c - a picture's colours cut to a palette (median cut over its
 * 5-6-5 colours), and pages dithered to 1 bit or to 16 colours
 * (Floyd-Steinberg), for EConvert's pictures (quant.h).
 */
#include <stdlib.h>
#include <string.h>
#include "quant.h"
#include "conv.h"

#define R8(c) ((((c) >> 11) << 3) | ((c) >> 13))
#define G8(c) (((((c) >> 5) & 63) << 2) | (((c) >> 9) & 3))
#define B8(c) ((((c) & 31) << 3) | (((c) >> 2) & 7))

typedef struct {
	long a, b;			/* its colours, cols[a..b) */
	long pixels;
	int lo[3], hi[3];
} BOX;

static uint16_t *g_cols;
static int g_axis;

static int chan(uint16_t c, int ax)
{
	return ax == 0 ? R8(c) : ax == 1 ? G8(c) : B8(c);
}

static int by_axis(const void *x, const void *y)
{
	int a = chan(*(const uint16_t *)x, g_axis), b = chan(*(const uint16_t *)y, g_axis);

	return a - b;
}

static void box_fit(BOX *b, const uint16_t *cols)
{
	long i;
	int k;

	for (k = 0; k < 3; k++) {
		b->lo[k] = 255;
		b->hi[k] = 0;
	}
	for (i = b->a; i < b->b; i++)
		for (k = 0; k < 3; k++) {
			int v = chan(cols[i], k);

			if (v < b->lo[k]) b->lo[k] = v;
			if (v > b->hi[k]) b->hi[k] = v;
		}
}

int quant(const uint16_t *px, long count, int ncol, unsigned char (*pal)[3], unsigned char *map)
{
	uint32_t *hist = calloc(65536, sizeof *hist);
	uint16_t *cols;
	BOX *box;
	long nc = 0, i;
	int nb = 1, k;

	if (!hist)
		return 0;
	for (i = 0; i < count; i++) {
		if ((i & 16383) == 16383 && cv_turn)
			cv_turn();
		hist[px[i]]++;
	}
	for (i = 0; i < 65536; i++)
		nc += hist[i] != 0;
	cols = malloc((size_t)(nc ? nc : 1) * sizeof *cols);
	box = malloc((size_t)ncol * sizeof *box);
	if (!cols || !box) {
		free(hist);
		free(cols);
		free(box);
		return 0;
	}
	nc = 0;
	for (i = 0; i < 65536; i++)
		if (hist[i])
			cols[nc++] = (uint16_t)i;
	box[0].a = 0;
	box[0].b = nc;
	box[0].pixels = count;
	box_fit(&box[0], cols);
	while (nb < ncol) {
		if ((nb & 7) == 7 && cv_turn)
			cv_turn();
		/* the box to split: the most pixels times its widest side */
		long best = -1, score = -1, j, half, acc = 0;
		int ax = 0;

		for (k = 0; k < nb; k++) {
			int w = 0, a;

			if (box[k].b - box[k].a < 2)
				continue;
			for (a = 0; a < 3; a++)
				if (box[k].hi[a] - box[k].lo[a] > w)
					w = box[k].hi[a] - box[k].lo[a];
			if (box[k].pixels * (long)w > score) {
				score = box[k].pixels * (long)w;
				best = k;
			}
		}
		if (best < 0)
			break;
		for (k = 1; k < 3; k++)
			if (box[best].hi[k] - box[best].lo[k] > box[best].hi[ax] - box[best].lo[ax])
				ax = k;
		g_cols = cols;
		g_axis = ax;
		qsort(cols + box[best].a, (size_t)(box[best].b - box[best].a), sizeof *cols, by_axis);
		half = box[best].pixels / 2;
		for (j = box[best].a; j < box[best].b - 1; j++) {
			acc += hist[cols[j]];
			if (acc >= half)
				break;
		}
		box[nb].a = j + 1;
		box[nb].b = box[best].b;
		box[best].b = j + 1;
		box[nb].pixels = box[best].pixels - acc;
		box[best].pixels = acc;
		box_fit(&box[best], cols);
		box_fit(&box[nb], cols);
		nb++;
	}
	for (k = 0; k < nb; k++) {
		unsigned long r = 0, g = 0, b = 0, n = 0;

		for (i = box[k].a; i < box[k].b; i++) {
			unsigned long h = hist[cols[i]];

			r += h * R8(cols[i]);
			g += h * G8(cols[i]);
			b += h * B8(cols[i]);
			n += h;
			if (map)
				map[cols[i]] = (unsigned char)k;
		}
		if (!n)
			n = 1;
		pal[k][0] = (unsigned char)(r / n);
		pal[k][1] = (unsigned char)(g / n);
		pal[k][2] = (unsigned char)(b / n);
	}
	free(hist);
	free(cols);
	free(box);
	return nb;
}

static int clamp8(int v)
{
	return v < 0 ? 0 : v > 255 ? 255 : v;
}

/*
 * The whole pages: an ordered dither (Bayer's 8 x 8), white and black
 * passed at once (a page is mostly white). Floyd-Steinberg was some 900
 * cycles a pixel on a Falcon (a page of 640 x 828 in 16 colours, 30 s);
 * this packs smaller too, its patterns repeating.
 */
static const unsigned char bayer[64] = {
	0, 32, 8, 40, 2, 34, 10, 42, 48, 16, 56, 24, 50, 18, 58, 26, 12, 44, 4, 36, 14, 46, 6, 38, 60, 28, 52, 20,
	62, 30, 54, 22, 3, 35, 11, 43, 1, 33, 9, 41, 51, 19, 59, 27, 49, 17, 57, 25, 15, 47, 7, 39, 13, 45, 5, 37,
	63, 31, 55, 23, 61, 29, 53, 21 };

void dither_mono(const uint16_t *px, int w, int h, unsigned char *out)
{
	dither_mono_rows(px, w, h, 0, out);
}

void dither_mono_rows(const uint16_t *px, int w, int h, int ytop, unsigned char *out)
{
	unsigned char thr[64];
	int x, y, rb = (w + 7) / 8, i;

	for (i = 0; i < 64; i++)
		thr[i] = (unsigned char)(bayer[i] * 4 + 2);
	memset(out, 0, (size_t)rb * h);
	for (y = 0; y < h; y++) {
		const uint16_t *s = px + (long)y * w;
		unsigned char *o = out + (long)y * rb;
		const unsigned char *t = thr + ((y + ytop) & 7) * 8;

		if ((y & 3) == 3 && cv_turn)
			cv_turn();

		for (x = 0; x < w; x++) {
			int c = s[x];

			if ((x & 7) == 0 && x + 8 <= w && c == 0xFFFF && s[x + 1] == 0xFFFF && s[x + 2] == 0xFFFF &&
			    s[x + 3] == 0xFFFF && s[x + 4] == 0xFFFF && s[x + 5] == 0xFFFF && s[x + 6] == 0xFFFF &&
			    s[x + 7] == 0xFFFF) {
				o[x >> 3] = 0xFF;	/* eight white pixels: a byte at once */
				x += 7;
				continue;
			}
			if (c == 0xFFFF)
				o[x >> 3] |= (unsigned char)(0x80 >> (x & 7));		/* 1 white */
			else if (c) {
				int v = (R8(c) * 77 + G8(c) * 150 + B8(c) * 29) >> 8;

				if (v > t[x & 7])
					o[x >> 3] |= (unsigned char)(0x80 >> (x & 7));
			}
		}
	}
}

/* the nearest of pal's n colours (red counting 3, green 4, blue 2) */
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

/* c near one of pal's pens: that pen, else -1 (render.ovl's snap_pen, 2.5:
   the 16 colours' page took a pattern over every grey, its text's edges
   too, Erez's photos of 10 Nov 2026); kept by colour in snap (-2 unknown) */
#define SNAP_D (9L * 28 * 28)

static int snap_of(signed char *snap, const unsigned char (*pal)[3], int n, unsigned c)
{
	int key = (int)((c >> 11) << 10 | ((c >> 6) & 31) << 5 | (c & 31));

	if (snap[key] == -2) {
		int r = R8(c), g = G8(c), b = B8(c), k = nearest(pal, n, r, g, b);
		int dr = r - pal[k][0], dg = g - pal[k][1], db = b - pal[k][2];
		long d = (long)dr * dr * 3 + (long)dg * dg * 4 + (long)db * db * 2;

		snap[key] = (signed char)(d < SNAP_D ? k : -1);
	}
	return snap[key];
}

void dither_pal(const uint16_t *px, int w, int h, const unsigned char (*pal)[3], int n, unsigned char *out)
{
	dither_pal_rows(px, w, h, 0, pal, n, out);
}

void dither_pal_rows(const uint16_t *px, int w, int h, int ytop, const unsigned char (*pal)[3], int n,
		     unsigned char *out)
{
	/* each 15-bit colour's nearest, the first found keeping it: kept for the
	   whole page (cleared at its first row), so a page in bands gets the
	   same as in one piece */
	static signed char *inv, *snap;
	int x, y, rb = (w + 1) / 2, white = nearest(pal, n, 255, 255, 255), black = nearest(pal, n, 0, 0, 0);
	short off[64];

	for (x = 0; x < 64; x++)
		off[x] = (short)((bayer[x] - 32) * 3);	/* about -96 .. 93: the 16 colours' steps */
	if (!inv)
		inv = malloc(32768);
	if (!snap)
		snap = malloc(32768);
	if (inv && ytop == 0)
		memset(inv, -1, 32768);
	if (snap && ytop == 0)
		memset(snap, -2, 32768);
	memset(out, 0, (size_t)rb * h);
	for (y = 0; y < h; y++) {
		const uint16_t *s = px + (long)y * w;
		unsigned char *o = out + (long)y * rb;
		const short *t = off + ((y + ytop) & 7) * 8;

		if ((y & 3) == 3 && cv_turn)
			cv_turn();

		for (x = 0; x < w; x++) {
			int c = s[x], best;

			if (c == 0xFFFF)
				best = white;
			else if (!c)
				best = black;
			else if (snap && (best = snap_of(snap, pal, n, c)) >= 0)
				;	/* near a pen: that pen, no pattern (a fill, a text's ink and its edge's grey) */
			else {
				int d = t[x & 7], r = clamp8(R8(c) + d), g = clamp8(G8(c) + d), b = clamp8(B8(c) + d);
				int key = (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3);

				if (inv && inv[key] >= 0)
					best = inv[key];
				else {
					best = nearest(pal, n, r, g, b);
					if (inv)
						inv[key] = (signed char)best;
				}
			}
			o[x >> 1] |= (unsigned char)(x & 1 ? best : best << 4);
		}
	}
}

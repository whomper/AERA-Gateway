/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * The "Optimised" badge's ground (Erez, 4 Nov 2026: "darker green with
 * nice shading in 16 colors using what is available and a great looking
 * shaded optimized for 256 colors").
 *
 * A rounded box with a dark edge, shaded from its top down, drawn a band
 * of rows at a time (rows alike are one rectangle). EPDF sets no pens: the
 * shades are the loaded palette's, found once with vq_color.
 *   256 colours and more: a gloss (Erez, 5 Nov: "shade it much better.
 *     Use more hues as needed"): a bright mint top falling to an emerald
 *     body and a deep green foot, five stops each the palette's nearest
 *     green of any hue (TOS's 256 have greens at four brightnesses, each
 *     with blue and yellow beside it, and no pale ones), each row a stop
 *     or two neighbouring ones dithered (the VDI's 50 % pattern, which
 *     reads as one colour where a finer dither of two hues showed as a
 *     texture).
 *   16 colours: the VDI's dark green (pen 11, the darkest green of the 16:
 *     darker only by a dither behind the text, which showed as dots), its
 *     first row dithered with the light green (pen 3) for a lit top edge,
 *     its last two with black for the shade below; a black edge.
 *   4 colours: the green (pen 3), the last two rows dithered with black.
 *   mono: a black box (the text white on it).
 *   A box under 10 pixels (ST low and medium's 8-pixel line): one green,
 *   a pixel off each corner, no edge, no shadow (they cut into its
 *   letters). The text is the system's small font (textwin.c).
 */
#include "gemui.h"
#include "badge.h"

#define NSH 5

static short nsh, sh[NSH], edge = 1, body = 3, lit = -1, dark = 1;

/* the gloss's stops, top to foot, and the edge, in the VDI's 0 to 1000
   (two stops alike: the body holds its colour a while) */
static const short stop[NSH][3] = {
	{ 0, 1000, 333 }, { 0, 733, 200 }, { 0, 467, 133 }, { 0, 467, 133 }, { 0, 267, 67 }
};
static const short edge_rgb[3] = { 0, 240, 60 };

static long dist(const short *a, const short *b)
{
	long dr = a[0] - b[0], dg = a[1] - b[1], db = a[2] - b[2];
	return 3 * dr * dr + 4 * dg * dg + 2 * db * db;
}

static long lum(const short *c)
{
	return 299L * c[0] + 587L * c[1] + 114L * c[2];
}

/* a green: green the strongest by a clear step, red and blue well under
   it (a teal's or a yellow-green's dither shows as a texture) */
static int green(const short *c)
{
	short mx = c[0] > c[2] ? c[0] : c[2];
	return c[1] - mx >= 175 && c[0] * 100L <= c[1] * 45L && c[2] * 100L <= c[1] * 60L;
}

void badge_pens(void)
{
	short rgb[3], p, i, n = planes >= 8 ? 256 : (short)(1 << planes);

	nsh = 0;
	if (planes == 1)
		return;
	if (planes < 8) {
		/* the VDI's pens: 3 the green, 11 the dark green (16 colours) */
		body = 3;
		lit = -1;
		if (planes >= 4) {
			vq_color(vdi_h, 11, rgb);
			if (green(rgb)) {
				body = 11;
				lit = 3;
			}
		}
		dark = edge = 1;
		return;
	}
	{
		short crgb[256][3];		/* (the greens' colours, read once; on the stack: no resident bytes) */
		short cand[256], nc = 0, j;
		long bd, el = 0x7fffffffL;

		edge = 1;
		for (p = 1; p < n; p++) {
			vq_color(vdi_h, p, rgb);
			if (!green(rgb))
				continue;
			crgb[nc][0] = rgb[0];
			crgb[nc][1] = rgb[1];
			crgb[nc][2] = rgb[2];
			cand[nc++] = p;
			if (dist(rgb, edge_rgb) < el) {
				el = dist(rgb, edge_rgb);
				edge = p;
			}
		}
		if (nc == 0) {
			/* no green at all: as on 16 colours */
			body = 3;
			lit = -1;
			dark = 1;
			return;
		}
		/* each stop: the nearest green */
		for (i = 0; i < NSH; i++) {
			short best = 0;

			bd = 0x7fffffffL;
			for (j = 0; j < nc; j++)
				if (dist(crgb[j], stop[i]) < bd) {
					bd = dist(crgb[j], stop[i]);
					best = j;
				}
			sh[i] = cand[best];
		}
		nsh = NSH;
	}
}

/* a row's colours: pen a, and b dithered over it (-1 none) */
static void row_pens(short i, short h, short *a, short *b)
{
	short inner = h - 2, r = i - 1;		/* the rows inside the edge: 1 .. h-2 */

	*b = -1;
	if (nsh) {
		/* along the ramp: 2 nsh - 1 steps, the odd ones two pens dithered */
		short steps = (short)(2 * nsh - 1), s = inner > 1 ? (short)((long)r * (steps - 1) / (inner - 1)) : 0;

		*a = sh[s / 2];
		if (s & 1)
			*b = sh[s / 2 + 1];
		return;
	}
	*a = body;
	if (r == 0 && lit >= 0)
		*b = lit;
	else if (inner >= 5 && r >= inner - 2)
		*b = dark;
}

/* the corner's inset of row i (rounded: 2 1 at a tall box's ends, 1 at a short one's) */
static short inset(short i, short h)
{
	short e = i < h - 1 - i ? i : (short)(h - 1 - i);

	if (h >= 10)
		return e == 0 ? 2 : e == 1 ? 1 : 0;
	return e == 0 ? 1 : 0;
}

static void bar(short x1, short y1, short x2, short y2, short pen, short pattern)
{
	short p[4];

	p[0] = x1;
	p[1] = y1;
	p[2] = x2;
	p[3] = y2;
	vsf_color(vdi_h, pen);
	if (pattern) {
		vswr_mode(vdi_h, MD_TRANS);
		vsf_interior(vdi_h, FIS_PATTERN);
		vsf_style(vdi_h, 4);		/* the 50 % dither */
	} else {
		vswr_mode(vdi_h, MD_REPLACE);
		vsf_interior(vdi_h, FIS_SOLID);
	}
	vr_recfl(vdi_h, p);
}

short badge_draw(short x, short y, short w, short h)
{
	short i;

	vsf_perimeter(vdi_h, 0);
	if (planes == 1) {
		short p[4];

		p[0] = x;
		p[1] = y;
		p[2] = (short)(x + w - 1);
		p[3] = (short)(y + h - 1);
		vswr_mode(vdi_h, MD_REPLACE);
		vsf_interior(vdi_h, FIS_SOLID);
		vsf_color(vdi_h, 1);
		v_rfbox(vdi_h, p);
		return -1;
	}
	if (h < 10) {
		/* a short box (an 8-pixel line: ST low and medium): an edge and
		   shaded rows cut into its letters, and so did v_rfbox's corners;
		   one green, a pixel off each corner */
		short pen = nsh ? sh[nsh / 2] : body;

		bar((short)(x + 1), y, (short)(x + w - 2), y, pen, 0);
		bar(x, (short)(y + 1), (short)(x + w - 1), (short)(y + h - 2), pen, 0);
		bar((short)(x + 1), (short)(y + h - 1), (short)(x + w - 2), (short)(y + h - 1), pen, 0);
		return -1;
	}
	/* the edge: each row's span, rows alike together */
	for (i = 0; i < h;) {
		short k = i, in = inset(i, h);

		while (k + 1 < h && inset((short)(k + 1), h) == in)
			k++;
		bar((short)(x + in), (short)(y + i), (short)(x + w - 1 - in), (short)(y + k), edge, 0);
		i = (short)(k + 1);
	}
	/* inside it, a pixel in: the shades, rows alike together */
	for (i = 1; i < h - 1;) {
		short k = i, a, b, a2, b2, in;

		in = inset((short)(i - 1), h);
		if (inset(i, h) + 1 > in)
			in = (short)(inset(i, h) + 1);
		if (inset((short)(i + 1), h) > in)
			in = inset((short)(i + 1), h);
		row_pens(i, h, &a, &b);
		while (k + 1 < h - 1) {
			short n2 = inset((short)(k), h), in2;

			in2 = inset((short)(k + 1), h) + 1 > n2 ? (short)(inset((short)(k + 1), h) + 1) : n2;
			if (inset((short)(k + 2), h) > in2)
				in2 = inset((short)(k + 2), h);
			row_pens((short)(k + 1), h, &a2, &b2);
			if (in2 != in || a2 != a || b2 != b)
				break;
			k++;
		}
		bar((short)(x + in), (short)(y + i), (short)(x + w - 1 - in), (short)(y + k), a, 0);
		if (b >= 0)
			bar((short)(x + in), (short)(y + i), (short)(x + w - 1 - in), (short)(y + k), b, 1);
		i = (short)(k + 1);
	}
	return nsh ? edge : 1;		/* the text's shadow */
}

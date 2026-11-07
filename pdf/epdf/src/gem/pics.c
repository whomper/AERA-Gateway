/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pics.c - the pictures the text view shows. A picture is decoded on the
 * reading's own stack (doc.c, pdfimg.c), dithered to the screen's colours
 * and laid out as the VDI's standard bitmap there too, a slice at a time;
 * here it becomes the screen's own bitmap in ST RAM (vr_trnfm), and is
 * drawn by rows with vro_cpyfm. A few are kept, the least used let go.
 *
 * The bitmap's pixels are colour registers, not VDI pens: the dither is
 * given each register's colour (vq_color of the pen that uses it, TOS's
 * pen-to-register order). A 16-bit Falcon screen takes RGB 5-6-5 as it is.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "gemui.h"
#include "pic.h"
#include "epdf.h"
#include "pics.h"
#include "pdfimg.h"
#include "docview.h"

#define NPICS 8

static PICE cache[NPICS];
static PIC_PAL *pal;			/* taken at the first picture (not resident) */
static int pal_planes = -1;
static unsigned long tick;

/* VDI pen n uses colour register reg[n] (TOS, 2 to 16 colours; with 256
   the 16 first so and the rest as they are, pen 1 the last register) */
static int pen_reg(int pen, int planes)
{
	static const unsigned char r4[16] = { 0, 15, 1, 2, 4, 6, 3, 5, 7, 8, 9, 10, 12, 14, 11, 13 };
	static const unsigned char r2[4] = { 0, 3, 1, 2 };

	if (planes == 1)
		return pen;
	if (planes == 2)
		return r2[pen & 3];
	if (planes == 4)
		return r4[pen & 15];
	if (pen < 16)
		return pen == 1 ? 255 : r4[pen];
	return pen == 255 ? 15 : pen;
}

/* the registers' colours, for the dither */
PIC_PAL *pics_palette(void)
{
	unsigned char rgb[256][3];
	int n, pen;

	if (planes > 8)
		return NULL;
	if (pal_planes == planes)
		return pal;
	if (!pal && !(pal = calloc(1, sizeof *pal)))
		return NULL;
	n = 1 << planes;
	memset(rgb, 0, sizeof rgb);
	if (planes == 1) {
		/* white and black, without asking: EmuTOS's mono screen answers
		   vq_color with colours that made the dither run away */
		rgb[0][0] = rgb[0][1] = rgb[0][2] = 255;
		pic_pal_init(pal, 2, (const unsigned char (*)[3])rgb);
		pal_planes = planes;
		return pal;
	}
	for (pen = 0; pen < n; pen++) {
		short c[3];
		int r = pen_reg(pen, planes);

		vq_color(vdi_h, pen, c);
		rgb[r][0] = (unsigned char)(c[0] * 255L / 1000);
		rgb[r][1] = (unsigned char)(c[1] * 255L / 1000);
		rgb[r][2] = (unsigned char)(c[2] * 255L / 1000);
	}
	pic_pal_init(pal, n, (const unsigned char (*)[3])rgb);
	pal_planes = planes;
	return pal;
}

/* the picture as the VDI's standard bitmap (planes one after another, a
   row of words each), or the 16-bit screen's pixels: pure work, done on
   the reading's stack; NULL out of memory or stopped */
unsigned char *pics_standard(PIC *p, int nplanes, PIC_PAL *pl, int flip, long *bytes)
{
	long wd = (p->w + 15) / 16, plane = wd * 2 * p->h, y, x;
	unsigned char *pens, *std;
	int k;

	/* turned the way its page shows it (its pixels are the decoder's) */
	if (flip & DV_FLIPH)
		for (y = 0; y < p->h; y++) {
			unsigned short *a = p->px + y * p->w, *b = a + p->w - 1, t;

			for (; a < b; a++, b--)
				t = *a, *a = *b, *b = t;
		}
	if (flip & DV_FLIPV)
		for (y = 0; y < p->h / 2; y++) {
			unsigned short *a = p->px + y * p->w, *b = p->px + (p->h - 1 - y) * p->w, t;

			for (x = 0; x < p->w; x++, a++, b++)
				t = *a, *a = *b, *b = t;
		}

	if (nplanes == 16) {
		/* the Falcon's high colour: RGB 5-6-5, a word a pixel, rows of whole words */
		std = malloc((size_t)(wd * 16 * 2 * p->h));
		if (!std)
			return NULL;
		for (y = 0; y < p->h; y++) {
			unsigned short *o = (unsigned short *)(std + y * wd * 32);

			memcpy(o, p->px + y * p->w, (size_t)p->w * 2);
		}
		*bytes = wd * 32 * p->h;
		return std;
	}
	pens = malloc((size_t)p->w * (size_t)p->h);
	std = calloc(1, (size_t)(plane * nplanes));
	if (!pens || !std) {
		free(pens);
		free(std);
		return NULL;
	}
	if (!pic_dither(p, pl, pens)) {
		free(pens);
		free(std);
		return NULL;
	}
	for (y = 0; y < p->h; y++) {
		const unsigned char *r = pens + y * p->w;

		if ((y & 15) == 15 && pic_poll && !pic_poll()) {
			free(pens);
			free(std);
			return NULL;
		}
		for (k = 0; k < nplanes; k++) {
			unsigned char *o = std + k * plane + y * wd * 2;
			int bit = 1 << k;

			for (x = 0; x < p->w; x++)
				if (r[x] & bit)
					o[x >> 3] |= (unsigned char)(0x80 >> (x & 7));
		}
	}
	free(pens);
	*bytes = plane * nplanes;
	return std;
}

/* ---- a picture a row at a time, shown from the top down as its rows
   come (0.3: a scan on a 1 MB ST; 0.4: Erez's "rows top to bottom") ---- */

#define BAND 16				/* colour rows turned to the screen's format together */

/* the picture being made: its rows dithered straight into the screen's
   bitmap (one plane, or 16 bits: the standard format is the screen's), or
   into a band in the standard format that the main stack turns (vr_trnfm
   isn't for the decoder's stack) */
typedef struct {
	PICE *e;
	int nplanes;
	long w, wd, row;		/* row: a bitmap row's bytes (the screen's format) */
	unsigned char *pens;
	PIC_DITHER *dt;
	unsigned char *band;		/* colour: BAND rows, each plane BAND rows apart */
	long b0, bn;			/* the band's first row, and rows in it */
	int full;			/* the band waits for pics_band */
	int nomem;
	unsigned short *mir;		/* a mirrored picture's row, right to left */
	/* a palette's colours each with a pen near it (pdf_image_palette):
	   each colour (5-6-5) and its pen, no dither */
	int direct, dn, bpc;
	long sw, sh, ny, *xs;		/* the raw rows': the source's size, the next shown row, its columns */
	/* 1 bit a pixel shown narrower: each source byte's mask of the bits
	   kept (mid: which of the few), and for each mask and byte the bits
	   kept (cv) and how many (cn) */
	unsigned char *mid, *cv, cn[16];
	/* its ink kept: a column or a row not shown goes into the next one
	   shown (the ink wins), or a 10-pixel page's letters lost strokes
	   (Erez, 4 Nov 2026); racc: the rows passed over, ink0: 0 is ink */
	unsigned char *racc;
	int ink0, dark0;		/* dark0: colour 0 the darker of the two */
	unsigned char *mrow;		/* colour: a 1-bit row, spread to the band's planes */
	unsigned short dc[16];
	unsigned char dp[16];
} SINK;

static SINK *sink;			/* the one being made */
void (*pics_wait)(void);

static void *bm_alloc(long bytes, int *heap);

static void put_pens(SINK *k, long y);
static void spread(SINK *k, long y);
static void band_row(SINK *k, long y);

/* the bitmap at the first row (a JPEG made whole gives none); 0 none */
static int bm_first(SINK *k)
{
	PICE *e = k->e;

	if (!e->bm.fd_addr && !k->nomem) {
		/* the bitmap at the first row (a JPEG made whole gives none) */
		long bytes = k->row * e->h;
		void *bm = bm_alloc(bytes, &e->heap);

		if (!bm)
			k->nomem = 1;
		else {
			memset(bm, 0, (size_t)bytes);
			memset(&e->bm, 0, sizeof e->bm);
			e->bm.fd_addr = bm;
			e->bm.fd_w = (short)(k->wd * 16);
			e->bm.fd_h = (short)e->h;
			e->bm.fd_wdwidth = (short)k->wd;
			e->bm.fd_nplanes = (short)k->nplanes;
			e->bm.fd_stand = 0;
		}
	}
	return !k->nomem;
}

/* a row of the picture's own samples (pdf_image_raw_rows: shown at its
   size, each colour its pen): a mono page's rows are the bitmap's as
   they are (or inverted), the rest each pixel's pen */
static void sink_raw(void *u, long sy, const unsigned char *r)
{
	SINK *k = u;
	PICE *e = k->e;
	long x, y;

	if (!bm_first(k))
		return;
	/* the shown rows that are this source row's (fewer, or as many) */
	if (k->ny >= e->h || k->ny * k->sh / e->h != sy) {
		if (k->racc && k->ny < e->h) {
			long i, sb = (k->sw + 7) / 8;

			for (i = 0; i < sb; i++)
				k->racc[i] = (unsigned char)(k->ink0 ? k->racc[i] & r[i] : k->racc[i] | r[i]);
		}
		return;
	}
	y = k->ny++;
	if (k->w == k->sw && k->mrow) {
		memcpy(k->mrow, r, (size_t)((k->w + 7) / 8));
		spread(k, y);
		if (k->ny < e->h && k->ny * k->sh / e->h == sy)
			sink_raw(u, sy, r);
		return;
	}
	if (k->w == k->sw && k->nplanes == 1 && k->bpc == 1 && k->dn == 2 && k->dp[0] != k->dp[1]) {
		unsigned char *o = (unsigned char *)e->bm.fd_addr + y * k->row;
		long n = (k->w + 7) / 8;

		if (k->dp[0] == 1)
			for (x = 0; x < n; x++)
				o[x] = (unsigned char)~r[x];
		else
			memcpy(o, r, (size_t)n);
		if (k->w & 7)
			o[n - 1] &= (unsigned char)(0xFF00 >> (k->w & 7));
		e->made = y + 1;
		e->ready = y + 1;
		return;
	}
	if ((k->nplanes == 1 || k->mrow) && k->bpc == 1 && k->dn == 2 && k->dp[0] != k->dp[1] && k->cv) {
		/* a mono page shown narrower: each source byte's kept bits (a
		   table for each of the few masks), one after another (a pixel
		   at a time it was 17 s of an ST's 20 for a page) */
		unsigned char *o0 = k->mrow ? k->mrow : (unsigned char *)e->bm.fd_addr + y * k->row;
		unsigned char *o = o0, *oe = o + (k->w + 7) / 8;
		unsigned char inv = !k->mrow && k->dp[0] == 1 ? 0xFF : 0;
		const unsigned char *mid = k->mid, *cv = k->cv, *cn = k->cn;
		long i, sb = (k->sw + 7) / 8;
		unsigned acc = 0;
		int nb = 0;

		for (i = 0; i < sb && o < oe; i++) {
			int m = mid[i], c = cn[m], v = r[i];

			if (k->racc) {
				/* the rows passed over since the last shown one, in it */
				v = k->ink0 ? v & k->racc[i] : v | k->racc[i];
				k->racc[i] = (unsigned char)(k->ink0 ? 0xFF : 0);
			}
			acc = (acc << c) | cv[(m << 8) | v];
			nb += c;
			if (nb >= 8) {
				nb -= 8;
				*o++ = (unsigned char)((acc >> nb) ^ inv);
			}
		}
		if (nb > 0 && o < oe)
			*o++ = (unsigned char)((acc << (8 - nb)) ^ inv);
		if (k->mrow)
			spread(k, y);
		else {
			o = o0;
			if (k->w & 7)
				o[(k->w + 7) / 8 - 1] &= (unsigned char)(0xFF00 >> (k->w & 7));
			e->made = y + 1;
			e->ready = y + 1;
		}
		if (k->ny < e->h && k->ny * k->sh / e->h == sy)
			sink_raw(u, sy, r);
		return;
	}
	for (x = 0; x < k->w; x++) {
		long c = k->xs ? k->xs[x] : x;
		int i = k->bpc == 8 ? r[c] : k->bpc == 4 ? (r[c >> 1] >> (c & 1 ? 0 : 4)) & 15 :
			k->bpc == 2 ? (r[c >> 2] >> (6 - 2 * (c & 3))) & 3 : (r[c >> 3] >> (7 - (c & 7))) & 1;

		k->pens[x] = i < k->dn ? k->dp[i] : 0;
	}
	put_pens(k, y);
	if (k->ny < e->h && k->ny * k->sh / e->h == sy)
		sink_raw(u, sy, r);	/* (a row shown twice: a picture made bigger) */
}

static void sink_row(void *u, long y, const unsigned short *px)
{
	SINK *k = u;
	PICE *e = k->e;
	long x;

	if (!bm_first(k))
		return;
	if (k->mir) {
		for (x = 0; x < k->w; x++)
			k->mir[x] = px[k->w - 1 - x];
		px = k->mir;
	}
	if (k->nplanes == 16) {
		memcpy((unsigned char *)e->bm.fd_addr + y * k->row, px, (size_t)k->w * 2);
		e->made = y + 1;
		if (!(e->flip & DV_FLIPV))
			e->ready = y + 1;
		return;
	}
	if (k->direct) {
		/* a palette's colours each with a pen near it: the pen, no dither */
		unsigned short last = px[0] ^ 1;
		unsigned char q = 0;

		for (x = 0; x < k->w; x++) {
			if (px[x] != last) {
				int i;

				last = px[x];
				for (i = 0; i < k->dn && k->dc[i] != last; i++)
					;
				q = i < k->dn ? k->dp[i] : 0;
			}
			k->pens[x] = q;
		}
	} else
		pic_dither_row(k->dt, px, NULL, k->pens);
	put_pens(k, y);
}

/* the row's pens into the bitmap (one plane) or the band (the rest) */
static void put_pens(SINK *k, long y)
{
	PICE *e = k->e;
	long x;
	int p;

	if (k->nplanes == 1) {
		unsigned char *o = (unsigned char *)e->bm.fd_addr + y * k->row;

		for (x = 0; x < k->w; x++)
			if (k->pens[x] & 1)
				o[x >> 3] |= (unsigned char)(0x80 >> (x & 7));
		e->made = y + 1;
		if (!(e->flip & DV_FLIPV))
			e->ready = y + 1;
		return;
	}
	for (p = 0; p < k->nplanes; p++) {
		unsigned char *o = k->band + (p * BAND + y - k->b0) * k->wd * 2;
		int bit = 1 << p;

		for (x = 0; x < k->w; x++)
			if (k->pens[x] & bit)
				o[x >> 3] |= (unsigned char)(0x80 >> (x & 7));
	}
	band_row(k, y);
}

/* a 1-bit row (mrow: a bit set is colour 1) into the band: each plane
   the row, its inverse, all set or none, as the two pens' bits say (a
   pixel at a time, the double page took 22.7 s on a Falcon in 16
   colours) */
static void spread(SINK *k, long y)
{
	long n = (k->w + 7) / 8, i;
	unsigned char last = (unsigned char)(0xFF00 >> ((k->w & 7) ? (k->w & 7) : 8));
	int p;

	for (p = 0; p < k->nplanes; p++) {
		unsigned char *o = k->band + (p * BAND + y - k->b0) * k->wd * 2;
		int one = (k->dp[1] >> p) & 1, zero = (k->dp[0] >> p) & 1;

		if (one && zero)
			memset(o, 0xFF, (size_t)n);
		else if (one)
			memcpy(o, k->mrow, (size_t)n);
		else if (zero)
			for (i = 0; i < n; i++)
				o[i] = (unsigned char)~k->mrow[i];
		else
			continue;
		o[n - 1] &= last;
	}
	band_row(k, y);
}

/* the band's row y is in: a full band (or the last row) turned on the
   main stack */
static void band_row(SINK *k, long y)
{
	PICE *e = k->e;

	if (++k->bn == BAND || y == e->h - 1) {
		/* the band to the screen's format, on the main stack */
		k->full = 1;
		while (k->full && pics_wait)
			pics_wait();
		k->b0 += k->bn;
		k->bn = 0;
		memset(k->band, 0, (size_t)(k->wd * 2 * BAND * k->nplanes));
	}
}

int pics_band(void)
{
	SINK *k = sink;
	MFDB src, dst;
	int p;

	if (!k || !k->full)
		return 0;
	if (k->bn < BAND)
		/* a short last band: its planes moved together, as the standard
		   format of bn rows has them */
		for (p = 1; p < k->nplanes; p++)
			memmove(k->band + p * k->bn * k->wd * 2, k->band + p * BAND * k->wd * 2, (size_t)(k->bn * k->wd * 2));
	memset(&src, 0, sizeof src);
	src.fd_addr = k->band;
	src.fd_w = (short)(k->wd * 16);
	src.fd_h = (short)k->bn;
	src.fd_wdwidth = (short)k->wd;
	src.fd_nplanes = (short)k->nplanes;
	src.fd_stand = 1;
	dst = src;
	dst.fd_addr = (unsigned char *)k->e->bm.fd_addr + k->b0 * k->row;
	dst.fd_stand = 0;
	vr_trnfm(vdi_h, &src, &dst);
	k->e->made = k->b0 + k->bn;
	if (!(k->e->flip & DV_FLIPV))
		k->e->ready = k->b0 + k->bn;	/* (upside down: shown when it is whole, turned) */
	k->full = 0;
	return 1;
}

/* the tables a mono page shown narrower is made with (sink_raw): the
   source columns kept (xs), by bytes; none (k->cv NULL) when there are
   more than 16 masks */
static void mono_masks(SINK *k)
{
	long sb = (k->sw + 7) / 8, x, i;
	unsigned char *keep = calloc((size_t)sb, 1), masks[16];
	int nm = 0, m, v;

	k->mid = malloc((size_t)sb);
	if (!keep || !k->mid) {
		free(keep);
		free(k->mid);
		k->mid = NULL;
		return;
	}
	for (x = 0; x < k->w; x++)
		keep[k->xs[x] >> 3] |= (unsigned char)(0x80 >> (k->xs[x] & 7));
	for (i = 0; i < sb; i++) {
		for (m = 0; m < nm && masks[m] != keep[i]; m++)
			;
		if (m == nm) {
			if (nm == 16) {
				free(keep);
				free(k->mid);
				k->mid = NULL;
				return;
			}
			masks[nm++] = keep[i];
		}
		k->mid[i] = (unsigned char)m;
	}
	free(keep);
	k->cv = malloc((size_t)nm * 256);
	if (!k->cv) {
		free(k->mid);
		k->mid = NULL;
		return;
	}
	for (m = 0; m < nm; m++) {
		int c = 0, b;

		for (b = 0; b < 8; b++)
			c += (masks[m] >> b) & 1;
		k->cn[m] = (unsigned char)c;
		for (v = 0; v < 256; v++) {
			/* each kept bit with the dropped ones before it (after the
			   byte's last kept bit: into that one), the ink winning */
			int o = 0, pend = k->ink0 ? 1 : 0, any = 0, bit;

			for (b = 7; b >= 0; b--) {
				bit = (v >> b) & 1;
				if (masks[m] & (1 << b)) {
					o = o << 1 | (k->ink0 ? bit & pend : bit | pend);
					pend = k->ink0 ? 1 : 0;
					any = 1;
				} else
					pend = k->ink0 ? pend & bit : pend | bit;
			}
			if (any)
				o = k->ink0 ? o & (~1 | pend) : o | pend;
			k->cv[(m << 8) | v] = (unsigned char)o;
		}
	}
}

int pics_rows(PDOC *d, PICE *e, int nplanes, PIC_PAL *pl, int *why)
{
	SINK k;
	int how, sharp = 0;

	memset(&k, 0, sizeof k);
	k.e = e;
	k.nplanes = nplanes;
	k.w = e->w;
	k.wd = (e->w + 15) / 16;
	k.row = k.wd * 2 * nplanes;
	e->ready = 0;
	e->made = 0;
	if (nplanes != 16) {
		k.pens = malloc((size_t)e->w);
		k.dt = pic_dither_open(e->w, pl);
	}
	if (nplanes != 1 && nplanes != 16)
		k.band = calloc(1, (size_t)(k.wd * 2 * BAND * nplanes));
	if (e->flip & DV_FLIPH)
		k.mir = malloc((size_t)e->w * 2);
	if ((nplanes != 16 && (!k.pens || !k.dt)) || (nplanes != 1 && nplanes != 16 && !k.band) ||
	    ((e->flip & DV_FLIPH) && !k.mir)) {
		free(k.mir);
		free(k.pens);
		free(k.band);
		pic_dither_close(k.dt);
		*why = PIC_MEMORY;
		return 0;
	}
	if (nplanes != 16) {
		/* already dithered to colours the screen has (EConvert's whole
		   pages: TOS's 16, or black and white): each colour its pen
		   (a page of 640 x 828 in 16 colours, dithered again, took
		   23 s on a Falcon) */
		unsigned char ip[256 * 3];
		int n = 0, i, ok;

		ok = pdf_image_palette(d, e->obj, ip, &n);
		/* Sharp (Erez, 5 Nov 2026): a picture of few colours (a
		   screenshot, a diagram), more than the pens near them hold, on 16
		   colours or fewer: each pixel its nearest pen, no dither (a dither
		   made its letters dots); photos (EConvert's have 256) keep it */
		sharp = ok && n <= 128 && nplanes <= 4;
		ok = ok && n <= 16;
		for (i = 0; ok && i < n; i++) {
			int q = pic_pal_nearest(pl, ip[3 * i], ip[3 * i + 1], ip[3 * i + 2]), c;

			for (c = 0; c < 3; c++) {
				int dv = ip[3 * i + c] - pl->rgb[q][c];

				if (dv > 48 || dv < -48)
					ok = 0;
			}
			k.dc[i] = (unsigned short)(((ip[3 * i] & 0xF8) << 8) | ((ip[3 * i + 1] & 0xFC) << 3) | (ip[3 * i + 2] >> 3));
			k.dp[i] = (unsigned char)q;
		}
		if (n == 2)
			k.dark0 = ip[0] * 3 + ip[1] * 6 + ip[2] < ip[3] * 3 + ip[4] * 6 + ip[5];
		k.dn = n;
		k.direct = ok;
		if (sharp && !k.direct)
			pic_dither_sharp(k.dt);	/* (common's: the dither's nearest pens) */
	}
	sink = &k;
	{
		POBJ *o = k.direct && !e->flip ? pdf_get(d, e->obj) : NULL;

		k.bpc = o ? (int)pd_int(d, o, "BitsPerComponent", 8) : 0;
		k.sw = o ? pd_int(d, o, "Width", 0) : 0;
		k.sh = o ? pd_int(d, o, "Height", 0) : 0;
		if (o && k.sw > 0 && k.sh > 0 && k.sw != e->w && (k.xs = malloc((size_t)e->w * sizeof *k.xs)) != NULL) {
			long x;

			for (x = 0; x < e->w; x++)
				k.xs[x] = x * k.sw / e->w;
			if (k.bpc == 1 && k.sw > e->w) {
				k.ink0 = nplanes == 1 ? k.dp[0] == 1 : k.dark0;	/* (pen 1: black) */
				mono_masks(&k);
				if (k.cv && (k.racc = malloc((size_t)(k.sw + 7) / 8)) != NULL)
					memset(k.racc, k.ink0 ? 0xFF : 0, (size_t)(k.sw + 7) / 8);
			}
		}
		if (o && k.bpc == 1 && k.dn == 2 && k.dp[0] != k.dp[1] && nplanes > 1 && nplanes != 16 &&
		    !e->flip && (k.sw == e->w || k.cv))
			k.mrow = calloc(1, (size_t)(k.wd * 2));
		if (o && k.sw > 0 && k.sh > 0 && (k.sw == e->w || k.xs))
			/* its samples as they are, each colour its pen, a column
			   and a row taken where it is shown smaller */
			how = pdf_image_raw_rows(d, e->obj, sink_raw, &k, 2L << 20, why);
		else
			how = pdf_image_rows(d, e->obj, e->w, e->h, sink_row, &k, 2L << 20, why);
		free(k.xs);
		free(k.mid);
		free(k.cv);
		free(k.racc);
		free(k.mrow);
	}
	sink = NULL;
	free(k.mir);
	free(k.pens);
	free(k.band);
	pic_dither_close(k.dt);
	if (how > 0 && (k.nomem || !e->bm.fd_addr)) {
		*why = PIC_MEMORY;
		how = 0;
	}
	return how;
}

static void free_bm(PICE *e)
{
	if (e->bm.fd_addr) {
		if (e->heap)
			free(e->bm.fd_addr);
		else
			Mfree(e->bm.fd_addr);
	}
	e->bm.fd_addr = NULL;
	e->ready = 0;
}

static void let_go(PICE *e)
{
	free_bm(e);
	memset(e, 0, sizeof *e);
}

/* the picture obj at w x h: its slot (ready, wanted, decoding or failed) */
PICE *pics_get(long obj, int w, int h, int flip)
{
	int i, best = -1;

	tick++;
	for (i = 0; i < NPICS; i++)
		if (cache[i].state && cache[i].obj == obj && cache[i].w == w && cache[i].h == h &&
		    cache[i].flip == flip) {
			cache[i].used = tick;
			cache[i].ahead = 0;		/* (shown now) */
			return &cache[i];
		}
	for (i = 0; i < NPICS; i++) {
		if (!cache[i].state) {
			best = i;
			break;
		}
		if (cache[i].state != PICS_DECODING && (best < 0 || cache[i].used < cache[best].used))
			best = i;
	}
	if (best < 0)
		return NULL;
	let_go(&cache[best]);
	cache[best].obj = obj;
	cache[best].w = w;
	cache[best].h = h;
	cache[best].flip = flip;
	cache[best].made = 0;
	cache[best].state = PICS_WANTED;
	cache[best].used = tick;
	return &cache[best];
}

int pics_waiting(void)
{
	int i;

	for (i = 0; i < NPICS; i++)
		if (cache[i].state == PICS_WANTED)
			return 1;
	return 0;
}

/* a picture waiting to be decoded (now marked so), or NULL */
PICE *pics_next(void)
{
	int i, pass;

	for (pass = 0; pass < 2; pass++)	/* one the window shows, then one ahead */
		for (i = 0; i < NPICS; i++)
			if (cache[i].state == PICS_WANTED && cache[i].ahead == pass) {
				cache[i].state = PICS_DECODING;
				return &cache[i];
			}
	return NULL;
}

int pics_shown_waiting(void)
{
	int i;

	for (i = 0; i < NPICS; i++)
		if (cache[i].state == PICS_WANTED && !cache[i].ahead)
			return 1;
	return 0;
}

void pics_release(PICE *e)
{
	let_go(e);
}

/*
 * A picture ahead of the view, decoded quietly in the turns nothing else
 * wants (Erez, 4 Nov 2026): only into a free slot, at most AHEAD of them
 * waiting or decoding, and only while the memory has room to spare (its
 * bitmap twice and AHEAD_SPARE more: a 1 MB ST seldom has, and never goes
 * short for it). One shown always comes first (pics_next), and stops one
 * ahead that is being made (doc_pic_drop_ahead).
 */
#define AHEAD 2
#define AHEAD_SPARE (384L * 1024)

int pics_ahead(long obj, int w, int h, int flip, long page)
{
	int i, n = 0, slot = -1;
	long bytes;

	for (i = 0; i < NPICS; i++) {
		if (cache[i].state && cache[i].obj == obj && cache[i].w == w && cache[i].h == h && cache[i].flip == flip)
			return 0;
		if (cache[i].ahead && (cache[i].state == PICS_WANTED || cache[i].state == PICS_DECODING))
			n++;
		if (!cache[i].state && slot < 0)
			slot = i;
	}
	if (n >= AHEAD || slot < 0 || w <= 0 || h <= 0)
		return -1;
	bytes = (long)((w + 15) / 16) * 2 * (planes > 8 ? 16 : planes) * h;
	if ((long)Malloc(-1L) < 2 * bytes + AHEAD_SPARE)
		return -1;
	memset(&cache[slot], 0, sizeof cache[slot]);
	cache[slot].obj = obj;
	cache[slot].w = w;
	cache[slot].h = h;
	cache[slot].flip = flip;
	cache[slot].page = page;
	cache[slot].state = PICS_WANTED;
	cache[slot].ahead = 1;
	cache[slot].used = 0;		/* (the first let go when a shown one needs the room) */
	return 1;
}

static int in_st_ram(const void *p)
{
	return (unsigned long)p < 0x01000000UL;
}

/* no TT or Fast RAM (TOS 1.x has no Mxalloc: none) */
static int st_only(void)
{
	static int only = -1;

	if (only < 0) {
		long tt = (long)Mxalloc(-1L, 1);

		only = tt == -32 || tt <= 0;
	}
	return only;
}

/* a bitmap the screen can be copied from (ST RAM): without TT RAM the
   heap's (ST RAM), so what it holds free is used before GEMDOS is asked
   for more (a 1 MB ST, 0.3); else GEMDOS's ST RAM, then the heap's if it
   is ST RAM. *heap: malloc's */
static void *bm_alloc(long bytes, int *heap)
{
	void *m = NULL;

	*heap = 1;
	if (st_only())
		m = malloc((size_t)bytes);
	else {
		m = Mxalloc(bytes, 0);
		if ((long)m <= 0)
			m = NULL;
		else
			*heap = 0;
	}
	if (!m) {
		m = malloc((size_t)bytes);
		if (m && !in_st_ram(m)) {
			free(m);
			m = NULL;
		}
		*heap = 1;
	}
	return m;
}

static void failed(PICE *e, int why, const char *how)
{
	char a[48];

	if (e->ahead && (!why || why == PIC_MEMORY || why == PIC_STOPPED)) {
		let_go(e);		/* (ahead: tried again when it is shown) */
		return;
	}

	e->state = PICS_FAILED;
	e->why = why ? why : PIC_MEMORY;
	if (e->why == PIC_STOPPED)
		return;			/* (the document closing, the font changing) */
	snprintf(a, sizeof a, "%ld %dx%d %s", e->obj, e->w, e->h, how);
	log_event("picture-failed", a, NULL);	/* the rigs count them */
}

void pics_made(PICE *e, int ok, int why)
{
	char a[16];

	if (!ok) {
		free_bm(e);
		snprintf(a, sizeof a, "why %d", why ? why : PIC_MEMORY);
		failed(e, why, a);
		return;
	}
	if (e->flip & DV_FLIPV) {
		/* made top row first where its page shows it at the bottom:
		   turned over now that it is whole (rows are the screen's
		   format's whole words, so a row moves as it is) */
		long row = e->bm.fd_wdwidth * (long)e->bm.fd_nplanes, y;	/* words */

		for (y = 0; y < e->h / 2; y++) {
			unsigned short *a = (unsigned short *)e->bm.fd_addr + y * row,
				       *b = (unsigned short *)e->bm.fd_addr + (e->h - 1 - y) * row, t;
			long i;

			for (i = 0; i < row; i++, a++, b++)
				t = *a, *a = *b, *b = t;
		}
	}
	e->ready = e->h;
	e->state = PICS_READY;
	log_event("picture", "ready", NULL);
}

/* the reading's answer: the standard bitmap made the screen's own, in
   ST RAM (the screen can't be copied from TT RAM) */
void pics_done(PICE *e, unsigned char *std, long bytes, int why)
{
	MFDB src;
	void *dst;
	char a[40];

	if (!std) {
		snprintf(a, sizeof a, "why %d", why ? why : PIC_MEMORY);
		failed(e, why, a);
		return;
	}
	if (in_st_ram(std) && (planes == 1 || planes == 16)) {
		/* one plane, or pixels: the standard bitmap is the screen's own */
		dst = std;
		std = NULL;
		e->heap = 1;
	} else if (!(dst = bm_alloc(bytes, &e->heap))) {
		free(std);
		snprintf(a, sizeof a, "ST RAM %ld", bytes);
		failed(e, PIC_MEMORY, a);
		return;
	}
	memset(&src, 0, sizeof src);
	src.fd_w = (short)(((e->w + 15) / 16) * 16);
	src.fd_h = (short)e->h;
	src.fd_wdwidth = (short)((e->w + 15) / 16);
	src.fd_nplanes = planes;
	src.fd_stand = 1;
	src.fd_addr = std ? std : dst;
	e->bm = src;
	e->bm.fd_addr = dst;
	e->bm.fd_stand = 0;
	if (std && (planes == 1 || planes == 16))
		memcpy(dst, std, (size_t)bytes);	/* in TT RAM: copied to ST RAM as it is */
	else if (std)
		vr_trnfm(vdi_h, &src, &e->bm);
	free(std);
	e->ready = e->h;
	e->state = PICS_READY;
	log_event("picture", "ready", NULL);
}

/* the font or the window changed: the pictures are made again at their new sizes */
void pics_forget(void)
{
	int i;

	for (i = 0; i < NPICS; i++)
		if (cache[i].state != PICS_DECODING)
			let_go(&cache[i]);
}

void pics_exit(void)
{
	pics_forget();
	if (pal) {
		pic_pal_free(pal);
		free(pal);
		pal = NULL;
	}
}

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pdfimg.c - an image XObject as a picture at the size it is shown
 * (common/src/pic.h's PIC: RGB 5-6-5), for the text view.
 *
 *   DCTDecode       common's pic_decode (TJpgDec: unpacked at 1/2, 1/4
 *                   or 1/8 when shown that small; a progressive JPEG from
 *                   its blocks' averages, an eighth of its size)
 *   CCITTFaxDecode  src/ccitt.c, shrunk to grey by 2, 4 or 8 (rscale.c)
 *   no image filter DeviceGray, DeviceRGB, Indexed (and CalGray, CalRGB,
 *                   ICCBased taken by their number of colours), 1 or 8
 *                   bits a component
 *   JBIG2Decode     src/jbig2.c, a row at a time, then as CCITT
 *   JPXDecode       src/jpx.c, reduced (a half, a quarter...) while it
 *                   stays at least as big as shown, then sampled
 *
 * pic_poll (pic.h) is called every few rows here too: the caller's turn
 * for the machine, and its way to stop.
 */
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pic.h"
#include "ccitt.h"
#include "jbig2.h"
#include "jpx.h"
#include "rscale.h"
#include "pdfimg.h"

static unsigned short rgb565(int r, int g, int b)
{
	return (unsigned short)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static PIC *pic_new(int w, int h)
{
	PIC *p = calloc(1, sizeof *p);

	if (!p)
		return NULL;
	p->w = w;
	p->h = h;
	p->px = malloc((size_t)w * (size_t)h * 2);
	if (!p->px) {
		free(p);
		return NULL;
	}
	return p;
}

static int poll_ok(void)
{
	return !pic_poll || pic_poll();
}

/* the number of colours of a colour space (1 grey, 3 RGB, 4 CMYK), its
   palette when Indexed (pal: 256 x 3, *hival), 0 when it isn't known */
/* why a stream couldn't be decoded whole: too big for max, refused
   memory (the reader's count of refusals went up since oom), or damaged */
static int stream_why(PDOC *d, const SBUF *b, long max, long oom)
{
	if (b->len >= max)
		return PIC_TOO_BIG;
	return d->st.oom != oom ? PIC_MEMORY : PIC_DAMAGED;
}

static int colours(PDOC *d, POBJ *cs, unsigned char *pal, int *hival)
{
	if (!cs)
		return 1;
	if (cs->t == PO_NAME) {
		if (!strcmp(cs->u.p, "DeviceGray") || !strcmp(cs->u.p, "CalGray") || !strcmp(cs->u.p, "G"))
			return 1;
		if (!strcmp(cs->u.p, "DeviceRGB") || !strcmp(cs->u.p, "CalRGB") || !strcmp(cs->u.p, "RGB"))
			return 3;
		if (!strcmp(cs->u.p, "DeviceCMYK") || !strcmp(cs->u.p, "CMYK"))
			return 4;
		return 0;
	}
	if (cs->t == PO_ARR && cs->a >= 1) {
		POBJ *k = pdf_res(d, pa_at(cs, 0));

		if (!k || k->t != PO_NAME)
			return 0;
		if (!strcmp(k->u.p, "ICCBased") && cs->a >= 2) {
			POBJ *s = pdf_res(d, pa_at(cs, 1));
			long n = s && s->t == PO_DICT ? pd_int(d, s, "N", 0) : 0;

			return n == 1 || n == 3 || n == 4 ? (int)n : 0;
		}
		if (!strcmp(k->u.p, "CalGray"))
			return 1;
		if (!strcmp(k->u.p, "CalRGB"))
			return 3;
		if ((!strcmp(k->u.p, "Indexed") || !strcmp(k->u.p, "I")) && cs->a >= 4 && pal) {
			POBJ *base = pdf_res(d, pa_at(cs, 1)), *hv = pdf_res(d, pa_at(cs, 2));
			POBJ *lk = pa_at(cs, 3), *lr = pdf_res(d, lk);
			int bc = colours(d, base, NULL, NULL), i, n;
			const unsigned char *t = NULL;
			long tn = 0;
			SBUF b;

			if ((bc != 1 && bc != 3) || !hv || hv->t != PO_INT)
				return 0;
			*hival = hv->a < 0 ? 0 : hv->a > 255 ? 255 : (int)hv->a;
			sb_init(&b);
			if (lr && lr->t == PO_STR) {
				t = (const unsigned char *)lr->u.p;
				tn = lr->a;
			} else if (lr && lr->t == PO_DICT && pdf_stream(d, lr, lk && lk->t == PO_REF ? lk->a : -1, &b, NULL, 4096) == 0) {
				t = (const unsigned char *)b.s;
				tn = b.len;
			}
			n = *hival + 1;
			for (i = 0; i < 256; i++) {
				long at = (long)i * bc;
				int g = i < n && at + bc <= tn ? t[at] : 0;

				pal[i * 3] = (unsigned char)g;
				pal[i * 3 + 1] = (unsigned char)(bc == 3 && i < n && at + 3 <= tn ? t[at + 1] : g);
				pal[i * 3 + 2] = (unsigned char)(bc == 3 && i < n && at + 3 <= tn ? t[at + 2] : g);
			}
			sb_free(&b);
			return -1;		/* Indexed: one byte (or fewer bits) a pixel, through pal */
		}
	}
	return 0;
}

/* one shown row: the source row r, xs[x] the source column of shown pixel
   x, its colour as 5-6-5 to o (from_samples's and the rows' alike) */
static void conv_row(const unsigned char *r, const long *xs, int w, int bpc, int nc, const unsigned char *pal, int invert,
		     unsigned short *o)
{
	long x;

	for (x = 0; x < w; x++) {
		long sx = xs[x];
		int r8, g8, b8, v;

		if (bpc == 1) {
			v = (r[sx >> 3] >> (7 - (sx & 7))) & 1;
			if (nc < 0) {
				r8 = pal[v * 3], g8 = pal[v * 3 + 1], b8 = pal[v * 3 + 2];
			} else {
				v = (v ^ invert) ? 255 : 0;
				r8 = g8 = b8 = v;
			}
		} else if (bpc != 8) {
			/* 2 or 4 bits a sample, one component (grey, Indexed:
			   EConvert's whole pages are 16 colours, 4 bits) */
			long bit = sx * bpc;

			v = (r[bit >> 3] >> (8 - bpc - (bit & 7))) & ((1 << bpc) - 1);
			if (nc < 0) {
				r8 = pal[v * 3], g8 = pal[v * 3 + 1], b8 = pal[v * 3 + 2];
			} else {
				v = v * 255 / ((1 << bpc) - 1);
				r8 = g8 = b8 = invert ? 255 - v : v;
			}
		} else if (nc < 0) {
			v = r[sx];
			r8 = pal[v * 3], g8 = pal[v * 3 + 1], b8 = pal[v * 3 + 2];
		} else if (nc == 1) {
			r8 = g8 = b8 = r[sx];
		} else if (nc == 3) {
			r8 = r[sx * 3], g8 = r[sx * 3 + 1], b8 = r[sx * 3 + 2];
		} else {
			/* CMYK, as printers' ink: no colour management */
			int c = r[sx * 4], m = r[sx * 4 + 1], ye = r[sx * 4 + 2], k = r[sx * 4 + 3];

			r8 = 255 - (c + k > 255 ? 255 : c + k);
			g8 = 255 - (m + k > 255 ? 255 : m + k);
			b8 = 255 - (ye + k > 255 ? 255 : ye + k);
		}
		o[x] = rgb565(r8, g8, b8);
	}
}

/* raw samples (no image filter left) to a picture of w x h, each shown
   pixel the source pixel at its place */
static PIC *from_samples(const unsigned char *s, long n, long sw, long sh, int bpc, int nc, const unsigned char *pal,
			 int invert, int w, int h, int *why)
{
	long row_bytes, x, y;
	int comps = nc < 0 ? 1 : nc;
	PIC *p;
	long *xs;

	row_bytes = (sw * comps * bpc + 7) / 8;
	if (!(bpc == 1 || bpc == 8 || ((bpc == 2 || bpc == 4) && comps == 1)) || (nc == 4 && bpc != 8) || sw <= 0 || sh <= 0) {
		*why = PIC_UNSUPPORTED;
		return NULL;
	}
	if (row_bytes * sh > n) {
		*why = PIC_DAMAGED;
		return NULL;
	}
	if (!(p = pic_new(w, h)) || !(xs = malloc((size_t)w * sizeof *xs))) {
		if (p)
			pic_free(p);
		*why = PIC_MEMORY;
		return NULL;
	}
	for (x = 0; x < w; x++)
		xs[x] = x * sw / w;
	for (y = 0; y < h; y++) {
		if ((y & 15) == 15 && !poll_ok()) {
			free(xs);
			pic_free(p);
			*why = PIC_STOPPED;
			return NULL;
		}
		conv_row(s + (y * sh / h) * row_bytes, xs, w, bpc, nc, pal, invert, p->px + y * w);
	}
	free(xs);
	p->how = PIC_PNG;
	return p;
}

/* ------------------------------------------------------------------ a bit a pixel: CCITT and JBIG2 */
/*
 * Which bits are ink: a coder's 1 bits are its black (a G4 black run, a
 * JBIG2 1); the filter makes them the samples 0 (CCITT with BlackIs1
 * false, and JBIG2) or 1 (BlackIs1 true); DeviceGray's 0 is black, an
 * ImageMask paints where 0; /Decode [1 0] turns either round. So the ink
 * is the coder's 1 bits, turned for BlackIs1 and again for /Decode (the
 * same as poppler and mupdf draw them: tests/onebit.py).
 */
typedef struct {
	int (*row)(void *u, unsigned char *row);	/* the next row, 1 the coder's black; 0 at the end */
	void *u;
	long cols;
	int flip;			/* the coder's 1 bits are paper */
} BITS;

/* /Decode [1 0]: the first pair turned round */
static int decode_inverted(PDOC *d, POBJ *o)
{
	POBJ *dec = pd_get(d, o, "Decode"), *a, *b;
	long va, vb;

	if (!dec || dec->t != PO_ARR || dec->a < 2)
		return 0;
	a = pdf_res(d, pa_at(dec, 0));
	b = pdf_res(d, pa_at(dec, 1));
	if (!a || !b || (a->t != PO_INT && a->t != PO_REAL) || (b->t != PO_INT && b->t != PO_REAL))
		return 0;
	va = a->t == PO_INT ? (long)a->a * 65536 : a->a;
	vb = b->t == PO_INT ? (long)b->a * 65536 : b->a;
	return va > vb;
}

static POBJ *decode_parms(PDOC *d, POBJ *o)
{
	POBJ *dp = pd_get(d, o, "DecodeParms");

	if (dp && dp->t == PO_ARR && dp->a)
		dp = pdf_res(d, pa_at(dp, dp->a - 1));
	return dp && dp->t == PO_DICT ? dp : NULL;
}

/* the source's rows (sh of them) to a picture of w x h: shrunk to grey
   (rscale) by 2, 4 or 8 while it stays at least w wide, then sampled */
static PIC *from_bits(BITS *bs, long sh, int w, int h, int *why)
{
	long cols = bs->cols, gw, gh = 0, y, nb = (cols + 7) / 8;
	int f = 1;
	RSCALE rs;
	unsigned char *row, *grey;
	PIC *p = NULL;

	while (f < 8 && cols / (f * 2) >= w)
		f *= 2;
	gw = f > 1 ? cols / f : cols;
	row = malloc((size_t)nb);
	grey = malloc((size_t)gw * (size_t)(sh / f + 1));
	if (!row || !grey || (f > 1 && rs_init(&rs, (int)cols, f))) {
		free(row);
		free(grey);
		*why = PIC_MEMORY;
		return NULL;
	}
	for (y = 0; y < sh && bs->row(bs->u, row); y++) {
		long i;

		if (bs->flip)
			for (i = 0; i < nb; i++)
				row[i] = (unsigned char)~row[i];
		/* (1 is ink from here: rscale's) */
		if (f > 1) {
			if (rs_row(&rs, row, grey + gh * gw))
				gh++;
		} else {
			for (i = 0; i < cols; i++)
				grey[gh * gw + i] = (row[i >> 3] >> (7 - (i & 7))) & 1 ? 0 : 255;
			gh++;
		}
		if ((y & 63) == 63 && !poll_ok()) {
			*why = PIC_STOPPED;
			goto out;
		}
	}
	if (gh <= 0) {
		*why = PIC_DAMAGED;
		goto out;
	}
	p = from_samples(grey, gw * gh, gw, gh, 8, 1, NULL, 0, w, h, why);
out:
	if (f > 1)
		rs_close(&rs);
	free(row);
	free(grey);
	return p;
}

static long scan_fill(void *u, unsigned char *buf, long n);

static int ccitt_src(void *u, unsigned char *row)
{
	return ccitt_row(u, row);
}

/* a CCITT scan's source, opened from s, n (or pulled from src): 0, or -1
   and *why */
static int ccitt_bits(PDOC *d, POBJ *o, PCSRC *src, const unsigned char *s, long n, long sw, CCITT *c, BITS *bs, int *why)
{
	POBJ *dp = decode_parms(d, o);
	long k = 0, cols = sw;
	int black1 = 0, align = 0;

	if (dp) {
		POBJ *b;

		k = pd_int(d, dp, "K", 0);
		cols = pd_int(d, dp, "Columns", sw);
		b = pd_get(d, dp, "BlackIs1");
		black1 = b && b->t == PO_BOOL && b->a;
		b = pd_get(d, dp, "EncodedByteAlign");
		align = b && b->t == PO_BOOL && b->a;
	}
	if (k > 0 || cols <= 0) {
		*why = k > 0 ? PIC_UNSUPPORTED : PIC_DAMAGED;	/* group 3, two-dimensional */
		return -1;
	}
	if (src ? ccitt_open_pull(c, scan_fill, src, (int)cols, (int)k, align) : ccitt_open(c, s, n, (int)cols, (int)k, align)) {
		*why = PIC_MEMORY;
		return -1;
	}
	bs->row = ccitt_src;
	bs->u = c;
	bs->cols = cols;
	bs->flip = black1 ^ decode_inverted(d, o);
	return 0;
}

static PIC *from_ccitt(PDOC *d, POBJ *o, const unsigned char *s, long n, long sw, long sh, int w, int h, int *why)
{
	CCITT c;
	BITS bs;
	PIC *p;

	if (ccitt_bits(d, o, NULL, s, n, sw, &c, &bs, why))
		return NULL;
	p = from_bits(&bs, sh, w, h, why);
	ccitt_close(&c);
	return p;
}

/* a JBIG2 picture: its page a row at a time (jbig2.c: a generic region's
   three rows, a text region's symbols; never the page whole where it can
   help it) */
typedef struct {
	JB2 *j;
	long h;
} JB2ROWS;

static int jb2_src(void *u, unsigned char *row)
{
	return jb2_row(((JB2ROWS *)u)->j, row);
}

static int jb2_why(int r)
{
	return r == JB2_MEMORY ? PIC_MEMORY : r == JB2_UNSUPPORTED ? PIC_UNSUPPORTED : r == JB2_STOPPED ? PIC_STOPPED : PIC_DAMAGED;
}

/* data, n: the stream's bytes for the JBIG2 filter, the globals' into gb
   (the decoder copies neither: both stay till jbig2_done) */
static int jbig2_bits(PDOC *d, POBJ *o, const unsigned char *data, long n, long max_bytes, JB2ROWS *j, BITS *bs, SBUF *gb,
		      int *why)
{
	POBJ *dp = decode_parms(d, o), *gr = dp ? pd_get(d, dp, "JBIG2Globals") : NULL, *g;
	int r, w, h;

	sb_init(gb);
	memset(j, 0, sizeof *j);
	if (gr && gr->t == PO_REF && (g = pdf_get(d, gr->a)) && g->t == PO_DICT && pdf_stream(d, g, gr->a, gb, NULL, max_bytes) != 0) {
		sb_free(gb);
		*why = PIC_DAMAGED;
		return -1;
	}
	j->j = jb2_open((const uint8_t *)gb->s, gb->len, data, n, poll_ok, &r);
	if (!j->j) {
		sb_free(gb);
		*why = jb2_why(r);
		return -1;
	}
	jb2_size(j->j, &w, &h);
	j->h = h;
	bs->row = jb2_src;
	bs->u = j;
	bs->cols = w;
	bs->flip = decode_inverted(d, o);
	return 0;
}

/* the decoder done with: the reason its rows stopped short, if they did */
static void jbig2_done(JB2ROWS *j, SBUF *gb, int *why)
{
	if (j->j && jb2_error(j->j))
		*why = jb2_why(jb2_error(j->j));
	jb2_close(j->j);
	j->j = NULL;
	sb_free(gb);
}

static PIC *from_jbig2(PDOC *d, POBJ *o, const unsigned char *s, long n, long sh, int w, int h, long max_bytes, int *why)
{
	JB2ROWS j;
	BITS bs;
	SBUF gb;
	PIC *p;

	if (jbig2_bits(d, o, s, n, max_bytes, &j, &bs, &gb, why))
		return NULL;
	p = from_bits(&bs, sh < j.h ? sh : j.h, w, h, why);
	if (!p)
		jbig2_done(&j, &gb, why);
	else
		jbig2_done(&j, &gb, &(int){ 0 });
	return p;
}

static PIC *from_jpx(PDOC *d, POBJ *o, const unsigned char *s, long n, int w, int h, int *why);

static PIC *image_whole(PDOC *d, long obj, int w, int h, long max_bytes, int *why)
{
	POBJ *o = pdf_get(d, obj), *cs;
	const char *term = NULL;
	long sw, sh, oom;
	int bpc, nc, hival = 0, invert = 0;
	unsigned char pal[256 * 3];
	SBUF b;
	PIC *p = NULL;

	*why = PIC_DAMAGED;
	if (!o || o->t != PO_DICT || w <= 0 || h <= 0)
		return NULL;
	sw = pd_int(d, o, "Width", 0);
	sh = pd_int(d, o, "Height", 0);
	bpc = (int)pd_int(d, o, "BitsPerComponent", 8);
	/* a stencil paints where 0, shown black there as DeviceGray's 0 is:
	   either turned round by /Decode [1 0] */
	invert = decode_inverted(d, o);
	if (pdf_is_name(pd_get(d, o, "ImageMask"), "true") || ((cs = pd_get(d, o, "ImageMask")) && cs->t == PO_BOOL && cs->a))
		bpc = 1, cs = NULL, nc = 1;
	else
		nc = colours(d, pd_get(d, o, "ColorSpace"), pal, &hival);
	if (sw <= 0 || sh <= 0) {
		*why = PIC_DAMAGED;
		return NULL;
	}
	sb_init(&b);
	oom = d->st.oom;
	if (pdf_stream(d, o, obj, &b, &term, max_bytes) != 0) {
		*why = stream_why(d, &b, max_bytes, oom);
		sb_free(&b);
		return NULL;
	}
	if (term && !strcmp(term, "DCT"))
		p = pic_decode((const unsigned char *)b.s, b.len, w, h, 8L << 20, why);
	else if (term && !strcmp(term, "CCITT"))
		p = from_ccitt(d, o, (const unsigned char *)b.s, b.len, sw, sh, w, h, why);
	else if (term && !strcmp(term, "JBIG2"))
		p = from_jbig2(d, o, (const unsigned char *)b.s, b.len, sh, w, h, max_bytes, why);
	else if (term && !strcmp(term, "JPX"))
		p = from_jpx(d, o, (const unsigned char *)b.s, b.len, w, h, why);
	else if (term)
		*why = PIC_UNSUPPORTED;
	else if (nc == 0)
		*why = PIC_UNSUPPORTED;
	else
		p = from_samples((const unsigned char *)b.s, b.len, sw, sh, bpc, nc, pal, invert, w, h, why);
	sb_free(&b);
	if (p)
		*why = PIC_OK;
	return p;
}

/* ------------------------------------------------------------------ a row at a time */
/*
 * A scan or a picture of samples needn't be whole in memory (phase 0.3: a
 * 6600 x 5096 fax scan, 4.2 MB a bit a pixel, on a 1 MB ST): its rows at
 * w x h come to row() one after another, top to bottom, each made as it
 * goes from the rows decoded so far (a scan through ccitt.c and rscale.c,
 * samples stored or Flate-packed read a piece at a time, others decoded
 * whole as before). The same pixels as pdf_image's for the same picture.
 */
typedef struct {
	int w, h;
	long ny;			/* the next shown row */
	unsigned short *out;		/* a shown row */
	PIMG_ROW row;
	void *u;
} ROWS;

/* the shown rows that are source row sy's (of sh, shown at h): made from
   it by conv (src a row of samples) or from grey (a scan's), and handed on;
   0 when the caller's turn says stop */
static int emit(ROWS *o, long sy, long sh, void (*make)(void *, unsigned short *), void *mk)
{
	while (o->ny < o->h && o->ny * sh / o->h == sy) {
		make(mk, o->out);
		o->row(o->u, o->ny, o->out);
		o->ny++;
		if ((o->ny & 15) == 0 && !poll_ok())
			return 0;
	}
	return 1;
}

typedef struct {
	const unsigned char *r;
	const long *xs;
	int w, bpc, nc, invert;
	const unsigned char *pal;
} SMK;

static void make_samples(void *u, unsigned short *o)
{
	SMK *m = u;

	conv_row(m->r, m->xs, m->w, m->bpc, m->nc, m->pal, m->invert, o);
}

typedef struct {
	const unsigned char *g;		/* a grey row */
	const long *xs;
	int w;
} GMK;

static void make_grey(void *u, unsigned short *o)
{
	GMK *m = u;
	long x;

	for (x = 0; x < m->w; x++) {
		int v = m->g[m->xs[x]];

		o[x] = rgb565(v, v, v);
	}
}

static int rows_samples(PDOC *d, POBJ *o, long obj, long sw, long sh, int bpc, int nc, const unsigned char *pal, int invert,
			ROWS *out, long max_bytes, int *why)
{
	int comps = nc < 0 ? 1 : nc;
	long row_bytes = (sw * comps * bpc + 7) / 8, sy, x, *xs, oom0;
	PCSRC *src;
	SBUF whole;
	unsigned char *rb = NULL;
	SMK m;
	int ok = 1;

	if (!(bpc == 1 || bpc == 8 || ((bpc == 2 || bpc == 4) && comps == 1)) || (nc == 4 && bpc != 8) || sw <= 0 || sh <= 0) {
		*why = PIC_UNSUPPORTED;
		return 0;
	}
	sb_init(&whole);
	oom0 = d->st.oom;
	src = pdf_stream_open(d, o, obj);
	if (!src) {
		const char *term = NULL;
		long oom = d->st.oom;
		int r;

		d->expect = (row_bytes + 1) * sh;	/* (a PNG predictor's tag byte a row) */
		r = pdf_stream(d, o, obj, &whole, &term, max_bytes);
		d->expect = 0;
		if (r != 0) {
			*why = stream_why(d, &whole, max_bytes, oom);
			sb_free(&whole);
			return 0;
		}
		if (row_bytes * sh > whole.len) {
			*why = PIC_DAMAGED;
			sb_free(&whole);
			return 0;
		}
	}
	xs = malloc((size_t)out->w * sizeof *xs);
	if (src)
		rb = malloc((size_t)row_bytes);
	if (!xs || (src && !rb)) {
		*why = PIC_MEMORY;
		ok = 0;
		goto done;
	}
	for (x = 0; x < out->w; x++)
		xs[x] = x * sw / out->w;
	m.xs = xs;
	m.w = out->w;
	m.bpc = bpc;
	m.nc = nc;
	m.invert = invert;
	m.pal = pal;
	for (sy = 0; sy < sh && out->ny < out->h; sy++) {
		if (src) {
			long got = 0, k;

			while (got < row_bytes && (k = pdf_stream_read(src, rb + got, row_bytes - got)) > 0)
				got += k;
			if (got < row_bytes) {
				/* the data stops short: as pdf_image says, damaged; or
				   memory was refused while it was inflated (a 1 MB ST,
				   lowmem.py: a photo given up as damaged), tried again */
				*why = d->st.oom != oom0 ? PIC_MEMORY : PIC_DAMAGED;
				ok = 0;
				break;
			}
			m.r = rb;
		} else
			m.r = (const unsigned char *)whole.s + sy * row_bytes;
		if (!emit(out, sy, sh, make_samples, &m) || ((sy & 63) == 63 && !poll_ok())) {
			*why = PIC_STOPPED;
			ok = 0;
			break;
		}
	}
done:
	free(xs);
	free(rb);
	if (src)
		pdf_stream_close(src);
	sb_free(&whole);
	return ok;
}

static long scan_fill(void *u, unsigned char *buf, long n)
{
	long k = pdf_stream_read(u, buf, n);

	return k > 0 ? k : 0;
}

/* the source's rows (sh of them) a shown row at a time to out: shrunk to
   grey as from_bits, each shown row made from the grey row at its place */
static int rows_bits(BITS *bs, long sh, ROWS *out, int *why)
{
	long cols = bs->cols, gw, gh, gi = 0, y, x, *xs = NULL, nb = (cols + 7) / 8;
	int f = 1, ok = 1;
	RSCALE rs;
	unsigned char *row, *grey;
	GMK m;

	while (f < 8 && cols / (f * 2) >= out->w)
		f *= 2;
	gw = f > 1 ? cols / f : cols;
	gh = f > 1 ? sh / f : sh;		/* the grey rows there will be: pdf_image's gh */
	row = malloc((size_t)nb);
	grey = malloc((size_t)(f > 1 ? (cols + f - 1) / f : cols));	/* (rs_row's row: its width rounded up) */
	xs = malloc((size_t)out->w * sizeof *xs);
	if (!row || !grey || !xs || (f > 1 && rs_init(&rs, (int)cols, f))) {
		free(row);
		free(grey);
		free(xs);
		*why = PIC_MEMORY;
		return 0;
	}
	for (x = 0; x < out->w; x++)
		xs[x] = x * gw / out->w;
	m.g = grey;
	m.xs = xs;
	m.w = out->w;
	for (y = 0; y < sh && out->ny < out->h && bs->row(bs->u, row); y++) {
		long i;
		int full = 1;

		if (bs->flip)
			for (i = 0; i < nb; i++)
				row[i] = (unsigned char)~row[i];
		if (f > 1)
			full = rs_row(&rs, row, grey);
		else
			for (i = 0; i < cols; i++)
				grey[i] = (row[i >> 3] >> (7 - (i & 7))) & 1 ? 0 : 255;
		if (full) {
			if (!emit(out, gi, gh, make_grey, &m)) {
				*why = PIC_STOPPED;
				ok = 0;
				break;
			}
			gi++;
		}
		if ((y & 63) == 63 && !poll_ok()) {
			*why = PIC_STOPPED;
			ok = 0;
			break;
		}
	}
	if (ok && gi == 0) {
		*why = PIC_DAMAGED;
		ok = 0;
	}
	/* a scan cut short: its last grey row stands for the rest */
	while (ok && out->ny < out->h) {
		make_grey(&m, out->out);
		out->row(out->u, out->ny++, out->out);
	}
	if (f > 1)
		rs_close(&rs);
	free(row);
	free(grey);
	free(xs);
	return ok;
}

/* src: the coded bytes pulled a piece at a time; else s, n whole */
static int rows_ccitt(PDOC *d, POBJ *o, PCSRC *src, const unsigned char *s, long n, long sw, long sh, ROWS *out, int *why)
{
	CCITT c;
	BITS bs;
	int r;

	if (ccitt_bits(d, o, src, s, n, sw, &c, &bs, why))
		return 0;
	r = rows_bits(&bs, sh, out, why);
	ccitt_close(&c);
	return r;
}

/* a JPEG: its bytes read a piece at a time, its rows made one at a time
   (common's pic_jpeg_rows: TJpgDec, a 1 MB ST's); -1 one TJpgDec doesn't
   take (progressive, CMYK), or not stored so: pdf_image, whole */
typedef struct {
	PIMG_ROW row;
	void *u;
} JROWS;

static void jpeg_row(void *u, long y, const unsigned short *px, const unsigned char *alpha)
{
	JROWS *j = u;

	(void)alpha;			/* a JPEG's are solid */
	j->row(j->u, y, px);
}

static int rows_jpeg(PDOC *d, POBJ *o, long obj, int w, int h, PIMG_ROW row, void *u, int *why)
{
	PCSRC *src = pdf_stream_open_coded(d, o, obj);
	JROWS j;
	int r;

	if (!src)
		return -1;
	j.row = row;
	j.u = u;
	r = pic_jpeg_rows(scan_fill, src, w, h, jpeg_row, &j, why);
	pdf_stream_close(src);
	if (r > 0)
		*why = PIC_OK;
	return r;
}

/* a JPEG 2000 picture (its bytes s, n whole): decoded reduced by 2^r, the
   largest r it stays at least w x h at, each shown row the decoded one at
   its place (as from_samples); the PDF's Indexed colours taken for a grey
   one's samples */
typedef struct {
	ROWS *out;
	long rh;			/* the decoded rows */
	SMK m;
	int stop;
} JXR;

static void jpx_row(void *u, long y, const unsigned char *px)
{
	JXR *j = u;

	j->m.r = px;
	if (!j->stop && !emit(j->out, y, j->rh, make_samples, &j->m))
		j->stop = 1;
}

static int jpx_why(int r)
{
	return r == JPX_MEMORY ? PIC_MEMORY : r == JPX_UNSUPPORTED ? PIC_UNSUPPORTED : r == JPX_STOPPED ? PIC_STOPPED : PIC_DAMAGED;
}

static int rows_jpx(PDOC *d, POBJ *o, const unsigned char *s, long n, ROWS *out, int *why)
{
	int jw, jh, jc, lv, r, red = 0, rw, rh, nc, hival = 0;
	unsigned char pal[256 * 3];
	long x, *xs;
	JXR j;

	if ((r = jpx_info(s, n, 0, &jw, &jh, &jc, &lv)) != JPX_OK) {
		*why = jpx_why(r);
		return 0;
	}
	while (red < lv && jw >> (red + 1) >= out->w && jh >> (red + 1) >= out->h)
		red++;
	nc = colours(d, pd_get(d, o, "ColorSpace"), pal, &hival);
	if (!(nc < 0 && jc == 1))
		nc = jc >= 3 ? 3 : 1;		/* (the code-stream's own colours) */
	xs = malloc((size_t)out->w * sizeof *xs);
	if (!xs) {
		*why = PIC_MEMORY;
		return 0;
	}
	/* refused memory before a row came (the decoder's 4 bytes a pixel kept,
	   a 1 MB ST's): smaller, a half each time, rather than not at all */
	do {
		jpx_info(s, n, red, &rw, &rh, &jc, &lv);
		for (x = 0; x < out->w; x++)
			xs[x] = x * rw / out->w;
		memset(&j, 0, sizeof j);
		j.out = out;
		j.rh = rh;
		j.m.xs = xs;
		j.m.w = out->w;
		j.m.bpc = 8;
		j.m.nc = nc;
		j.m.pal = pal;
#ifdef PDFIMG_WHAT
		pdfimg_reduce = red;
		pdfimg_dw = rw;
		pdfimg_dh = rh;
#endif
		r = jpx_decode(s, n, red, jpx_row, &j, poll_ok, &jw, &jh, &jc);
	} while (r == JPX_MEMORY && out->ny == 0 && red++ < lv);
	free(xs);
	if (r == JPX_OK && j.stop)
		r = JPX_STOPPED;
	if (r != JPX_OK) {
		*why = jpx_why(r);
		return 0;
	}
	return 1;
}

static void into_pic(void *u, long y, const unsigned short *px)
{
	PIC *p = u;

	memcpy(p->px + y * p->w, px, (size_t)p->w * 2);
}

static PIC *from_jpx(PDOC *d, POBJ *o, const unsigned char *s, long n, int w, int h, int *why)
{
	PIC *p = pic_new(w, h);
	ROWS out;

	memset(&out, 0, sizeof out);
	out.w = w;
	out.h = h;
	out.row = into_pic;
	out.u = p;
	out.out = malloc((size_t)w * 2);
	if (!p || !out.out) {
		free(out.out);
		if (p)
			pic_free(p);
		*why = PIC_MEMORY;
		return NULL;
	}
	if (!rows_jpx(d, o, s, n, &out, why) || out.ny < h) {
		if (out.ny < h && *why == PIC_OK)
			*why = PIC_DAMAGED;
		free(out.out);
		pic_free(p);
		return NULL;
	}
	free(out.out);
	p->how = PIC_PNG;
	return p;
}

int pdf_image_palette(PDOC *d, long obj, unsigned char *pal, int *n)
{
	POBJ *o = pdf_get(d, obj), *fl;
	int hival = 0, nc;

	if (!o || o->t != PO_DICT)
		return 0;
	fl = pd_get(d, o, "Filter");
	if (fl && fl->t == PO_ARR && fl->a)
		fl = pdf_res(d, pa_at(fl, fl->a - 1));
	if (fl && fl->t == PO_NAME && strcmp(fl->u.p, "FlateDecode") && strcmp(fl->u.p, "Fl"))
		return 0;			/* (a JPEG, a scan: not samples of a palette) */
	if (pd_get(d, o, "ImageMask") || pd_get(d, o, "Decode"))
		return 0;
	nc = colours(d, pd_get(d, o, "ColorSpace"), pal, &hival);
	if (nc < 0) {
		*n = hival + 1;
		return 1;
	}
	if (nc == 1 && pd_int(d, o, "BitsPerComponent", 8) == 1) {
		pal[0] = pal[1] = pal[2] = 0;
		pal[3] = pal[4] = pal[5] = 255;
		*n = 2;
		return 1;
	}
	return 0;
}

int pdf_image_raw_rows(PDOC *d, long obj, void (*row)(void *u, long y, const unsigned char *r), void *u,
		       long max_bytes, int *why)
{
	POBJ *o = pdf_get(d, obj);
	long sw, sh, rbytes, y, oom0;
	int bpc, nc, hival = 0, ok = 1;
	unsigned char pal[256 * 3], *rb;
	PCSRC *src;
	SBUF whole;

	*why = PIC_DAMAGED;
	if (!o || o->t != PO_DICT)
		return 0;
	sw = pd_int(d, o, "Width", 0);
	sh = pd_int(d, o, "Height", 0);
	bpc = (int)pd_int(d, o, "BitsPerComponent", 8);
	nc = colours(d, pd_get(d, o, "ColorSpace"), pal, &hival);
	if (sw <= 0 || sh <= 0 || nc == 0)
		return 0;
	rbytes = (sw * (nc < 0 ? 1 : nc) * bpc + 7) / 8;
	sb_init(&whole);
	oom0 = d->st.oom;
	src = pdf_stream_open(d, o, obj);
	if (!src) {
		const char *term = NULL;
		long oom = d->st.oom;
		int r;

		d->expect = (rbytes + 1) * sh;		/* (as rows_samples) */
		r = pdf_stream(d, o, obj, &whole, &term, max_bytes);
		d->expect = 0;
		if (r != 0 || term || rbytes * sh > whole.len) {
			*why = term ? PIC_UNSUPPORTED : stream_why(d, &whole, max_bytes, oom);
			sb_free(&whole);
			return 0;
		}
	}
	rb = src ? malloc((size_t)rbytes) : NULL;
	if (src && !rb) {
		*why = PIC_MEMORY;
		pdf_stream_close(src);
		return 0;
	}
	for (y = 0; y < sh; y++) {
		if (src) {
			long got = 0, k;

			while (got < rbytes && (k = pdf_stream_read(src, rb + got, rbytes - got)) > 0)
				got += k;
			if (got < rbytes) {
				*why = d->st.oom != oom0 ? PIC_MEMORY : PIC_DAMAGED;	/* (as above) */
				ok = 0;
				break;
			}
			row(u, y, rb);
		} else
			row(u, y, (const unsigned char *)whole.s + y * rbytes);
		if ((y & 15) == 15 && !poll_ok()) {
			*why = PIC_STOPPED;
			ok = 0;
			break;
		}
	}
	free(rb);
	if (src)
		pdf_stream_close(src);
	sb_free(&whole);
	if (ok)
		*why = PIC_OK;
	return ok;
}

#ifdef PDFIMG_WHAT
static void note_kind(PDOC *d, long obj);
#else
#define note_kind(d, obj) ((void)0)
#endif

static int image_rows(PDOC *d, long obj, int w, int h, PIMG_ROW row, void *u, long max_bytes, int *why)
{
	POBJ *o = pdf_get(d, obj), *cs, *fl;
	long sw, sh;
	int bpc, nc, hival = 0, invert = 0, r;
	unsigned char pal[256 * 3];
	const char *fn = NULL;
	ROWS out;

	note_kind(d, obj);	/* (here: a mask decoded before it began its own work, and cleared it) */
	*why = PIC_DAMAGED;
	if (!o || o->t != PO_DICT || w <= 0 || h <= 0)
		return 0;
	fl = pd_get(d, o, "Filter");
	if (fl && fl->t == PO_ARR && fl->a)
		fl = pdf_res(d, pa_at(fl, fl->a - 1));
	if (fl && fl->t == PO_NAME)
		fn = fl->u.p;
	if (fn && (!strcmp(fn, "DCTDecode") || !strcmp(fn, "DCT")))
		return rows_jpeg(d, o, obj, w, h, row, u, why);
	sw = pd_int(d, o, "Width", 0);
	sh = pd_int(d, o, "Height", 0);
	bpc = (int)pd_int(d, o, "BitsPerComponent", 8);
	invert = decode_inverted(d, o);		/* (as pdf_image's) */
	if (pdf_is_name(pd_get(d, o, "ImageMask"), "true") || ((cs = pd_get(d, o, "ImageMask")) && cs->t == PO_BOOL && cs->a))
		bpc = 1, cs = NULL, nc = 1;
	else
		nc = colours(d, pd_get(d, o, "ColorSpace"), pal, &hival);
	if (sw <= 0 || sh <= 0)
		return 0;
	if (nc == 0) {
		*why = PIC_UNSUPPORTED;
		return 0;
	}
	memset(&out, 0, sizeof out);
	out.w = w;
	out.h = h;
	out.row = row;
	out.u = u;
	out.out = malloc((size_t)w * 2);
	if (!out.out) {
		*why = PIC_MEMORY;
		return 0;
	}
	if (fn && (!strcmp(fn, "CCITTFaxDecode") || !strcmp(fn, "CCF"))) {
		SBUF b;
		const char *term = NULL;
		PCSRC *src = pdf_stream_open_coded(d, o, obj);
		long oom = d->st.oom;

		sb_init(&b);
		if (src) {
			r = rows_ccitt(d, o, src, NULL, 0, sw, sh, &out, why);
			pdf_stream_close(src);
		} else if (pdf_stream(d, o, obj, &b, &term, max_bytes) != 0) {
			*why = stream_why(d, &b, max_bytes, oom);
			r = 0;
		} else
			r = rows_ccitt(d, o, NULL, (const unsigned char *)b.s, b.len, sw, sh, &out, why);
		sb_free(&b);
	} else if (fn && !strcmp(fn, "JBIG2Decode")) {
		/* (its bytes whole; the page a row at a time, jbig2.c's streaming) */
		SBUF b;
		const char *term = NULL;
		long oom = d->st.oom;
		JB2ROWS j;
		BITS bs;
		SBUF gb;

		sb_init(&b);
		r = 0;
		if (pdf_stream(d, o, obj, &b, &term, max_bytes) != 0)
			*why = stream_why(d, &b, max_bytes, oom);
		else if (!term || strcmp(term, "JBIG2"))
			*why = PIC_DAMAGED;
		else if (jbig2_bits(d, o, (const unsigned char *)b.s, b.len, max_bytes, &j, &bs, &gb, why) == 0) {
			r = rows_bits(&bs, sh < j.h ? sh : j.h, &out, why);
			if (!r)
				jbig2_done(&j, &gb, why);
			else
				jbig2_done(&j, &gb, &(int){ 0 });
		}
		sb_free(&b);
	} else if (fn && !strcmp(fn, "JPXDecode")) {
		/* (decoded whole: a code-block's coefficients reach many rows) */
		SBUF b;
		const char *term = NULL;
		long oom = d->st.oom;

		sb_init(&b);
		r = 0;
		if (pdf_stream(d, o, obj, &b, &term, max_bytes) != 0)
			*why = stream_why(d, &b, max_bytes, oom);
		else if (!term || strcmp(term, "JPX"))
			*why = PIC_DAMAGED;
		else
			r = rows_jpx(d, o, (const unsigned char *)b.s, b.len, &out, why);
		sb_free(&b);
	} else
		r = rows_samples(d, o, obj, sw, sh, bpc, nc, pal, invert, &out, max_bytes, why);
	free(out.out);
	if (r)
		*why = PIC_OK;
	return r;
}

/* ------------------------------------------------------------------ a soft mask over white */
/*
 * A picture with an /SMask (archive.org's text pages: a foreground drawn
 * through a JBIG2 mask) is shown as it looks on white paper: its mask
 * decoded first at the picture's size (a bit a pixel when the mask is
 * 1-bit, so a 600 x 800 picture's is 60 KB), then each row blended as it
 * passes. render.c draws the picture through the mask itself
 * (pdfimg_raw_smask).
 */
int pdfimg_raw_smask;

typedef struct {
	unsigned char *a;
	int bits;			/* 1: a bit a pixel, else a byte */
	long w, h;
	PIMG_ROW row;
	void *u;
	unsigned short *tmp;
} SMASK;

static long smask_of(POBJ *o)
{
	POBJ *sm = pd_getraw(o, "SMask");

	return sm && sm->t == PO_REF ? sm->a : -1;
}

static int grey_of(unsigned short c)
{
	return ((c >> 11) * 8 + ((c >> 5) & 63) * 4 * 2 + (c & 31) * 8) / 4;
}

static void mask_in(void *u, long y, const unsigned short *px)
{
	SMASK *m = u;
	long x;

	if (y < 0 || y >= m->h)
		return;
	if (m->bits) {
		unsigned char *r = m->a + y * ((m->w + 7) / 8);

		for (x = 0; x < m->w; x++)
			if (grey_of(px[x]) >= 128)
				r[x >> 3] |= (unsigned char)(0x80 >> (x & 7));
	} else
		for (x = 0; x < m->w; x++)
			m->a[y * m->w + x] = (unsigned char)grey_of(px[x]);
}

/* the mask of obj (mobj) at w x h into m: 1, or 0 (shown without it) */
static int mask_load(PDOC *d, long mobj, int w, int h, long max_bytes, SMASK *m)
{
	POBJ *mo = pdf_get(d, mobj);
	long bytes;
	int why, r;

	memset(m, 0, sizeof *m);
	if (!mo || mo->t != PO_DICT)
		return 0;
	m->bits = pd_int(d, mo, "BitsPerComponent", 8) == 1;
	m->w = w;
	m->h = h;
	bytes = m->bits ? (long)((w + 7) / 8) * h : (long)w * h;
	if (bytes > 256L * 1024 || !(m->a = calloc((size_t)bytes, 1)))
		return 0;
	r = image_rows(d, mobj, w, h, mask_in, m, max_bytes, &why);
	if (r < 0) {
		PIC *p = image_whole(d, mobj, w, h, max_bytes, &why);
		long y;

		if (p)
			for (y = 0; y < h; y++)
				mask_in(m, y, p->px + y * (long)w);
		r = p != NULL;
		pic_free(p);
	}
	if (r != 1) {
		free(m->a);
		m->a = NULL;
		return 0;
	}
	return 1;
}

/* row y of the picture over white through the mask */
static void blend(const SMASK *m, long y, unsigned short *px)
{
	long x;

	if (m->bits) {
		const unsigned char *r = m->a + y * ((m->w + 7) / 8);

		for (x = 0; x < m->w; x++)
			if (!(r[x >> 3] & (0x80 >> (x & 7))))
				px[x] = 0xFFFF;
	} else {
		const unsigned char *r = m->a + y * m->w;

		for (x = 0; x < m->w; x++) {
			unsigned a = r[x], c = px[x], k = 255 - a;
			unsigned R = ((c >> 11) * a + 31 * k) >> 8, G = (((c >> 5) & 63) * a + 63 * k) >> 8, B = ((c & 31) * a + 31 * k) >> 8;

			px[x] = (unsigned short)(R << 11 | G << 5 | B);
		}
	}
}

static void row_through(void *u, long y, const unsigned short *px)
{
	SMASK *m = u;

	if (y < 0 || y >= m->h)
		return;
	memcpy(m->tmp, px, (size_t)m->w * 2);
	blend(m, y, m->tmp);
	m->row(m->u, y, m->tmp);
}

/* the decoders' work before a picture's rows (pdfimg.h) */
long pdfimg_work_done, pdfimg_work_of;
#ifdef PDFIMG_WHAT
/* the picture last begun: its filter (the last of an array), and a JPEG
   2000 one's reduction and size as decoded (-1: none); for EPDF's log
   line (its build only: render.ovl's and EConvert's code where 2.36 had
   it) */
const char *pdfimg_kind = "";
int pdfimg_reduce = -1;
long pdfimg_dw, pdfimg_dh;
#endif

static void work_hook(long done, long of)
{
	pdfimg_work_done = done;
	pdfimg_work_of = of;
}

#ifdef PDFIMG_WHAT
/* the filter a picture is decoded by (the last of an array), for the logs */
static void note_kind(PDOC *d, long obj)
{
	static char kind[24];	/* (copied: the object may be gone when the log line is written) */
	POBJ *o = pdf_get(d, obj), *fl = o && o->t == PO_DICT ? pd_get(d, o, "Filter") : NULL;

	if (fl && fl->t == PO_ARR && fl->a)
		fl = pdf_res(d, pa_at(fl, fl->a - 1));
	strncpy(kind, fl && fl->t == PO_NAME ? fl->u.p : "none", sizeof kind - 1);
	pdfimg_kind = kind;
}
#endif

static void work_begin(void)
{
	pdfimg_work_done = pdfimg_work_of = 0;
#ifdef PDFIMG_WHAT
	pdfimg_reduce = -1;
	pdfimg_dw = pdfimg_dh = 0;
#endif
	jpx_progress = work_hook;
	jb2_progress = work_hook;
}

PIC *pdf_image(PDOC *d, long obj, int w, int h, long max_bytes, int *why)
{
	PIC *p;
	POBJ *o;
	long mo;
	SMASK m;

	work_begin();
	note_kind(d, obj);
	p = image_whole(d, obj, w, h, max_bytes, why);

	if (p && !pdfimg_raw_smask && (o = pdf_get(d, obj)) && (mo = smask_of(o)) >= 0 &&
	    mask_load(d, mo, p->w, p->h, max_bytes, &m)) {
		long y;

		for (y = 0; y < p->h; y++)
			blend(&m, y, p->px + y * (long)p->w);
		free(m.a);
	}
	return p;
}

/* what pdf_image_rows will take for obj shown w x h on a monochrome screen,
   about, beyond what the reader keeps: for a heap zone sized to it (EPDF
   on a 68000: a picture too big refused at its start, not after 11 s of
   work; the hub's heap_zone, 18 Nov 2026). 0: not known (JBIG2, JPX: their
   decoders' own state), no zone. Fitted to the zones' peaks on a 4 MB ST
   (big.py, 18 Nov 2026), each a little over what was measured:
     Flate (and none): inflate's state (pdffilt.c's PCSRC_INFLATE), 15
       bytes a shown pixel's column, 2.5 a source's (novel-boymechanic's
       scans: 1460 wide 82,028, 773 wide 80,292, both 350 shown);
     CCITT: the coded bytes' piece, 10 a source column, 2 a shown one
       (scan-stinternals' pages, 1950 wide, 468 shown: 51,924 to 53,720);
     DCT: TJpgDec's work and 22 a source column (scan-gfabasic's 1354
       wide 41,108; scan-stinternals' 2022 wide 55,600);
   with a soft mask's alpha whole and its own stream on top */
#define NEED_INFLATE 73000L	/* pdffilt.c's PCSRC_INFLATE */
#define NEED_PIECE 32768L	/* its RAW_PIECE, the coded bytes read at a time */
long pdf_image_rows_need(PDOC *d, long obj, int w, int h)
{
	POBJ *o = pdf_get(d, obj), *fl;
	const char *fn = NULL;
	long sw, n;

	if (!o || o->t != PO_DICT || w <= 0 || h <= 0)
		return 0;
	fl = pd_get(d, o, "Filter");
	if (fl && fl->t == PO_ARR && fl->a)
		fl = pdf_res(d, pa_at(fl, fl->a - 1));
	if (fl && fl->t == PO_NAME)
		fn = fl->u.p;
	sw = pd_int(d, o, "Width", 0);
	if (sw <= 0 || pd_int(d, o, "Height", 0) <= 0)
		return 0;
	if (fn && (!strcmp(fn, "DCTDecode") || !strcmp(fn, "DCT")))
		n = 12288L + sw * 22;
	else if (fn && (!strcmp(fn, "CCITTFaxDecode") || !strcmp(fn, "CCF")))
		n = NEED_PIECE + 10240 + sw * 5 + (long)w * 2;	/* (ccitt.c: its 32 KB of tables and 4 per column; a 6,600-wide scan 74.6 KB on the host, 2.39) */
	else if (!fn || !strcmp(fn, "FlateDecode") || !strcmp(fn, "Fl"))
		n = NEED_INFLATE + (long)w * 15 + sw * 5 / 2;
	else
		return 0;
	if (smask_of(o) >= 0)
		n += (long)w * h + NEED_INFLATE + (long)w * 2;	/* the mask's alpha whole, its stream, a row */
	return n;
}

int pdf_image_rows(PDOC *d, long obj, int w, int h, PIMG_ROW row, void *u, long max_bytes, int *why)
{
	POBJ *o;
	long mo;
	SMASK m;
	int r;

	work_begin();
	if (pdfimg_raw_smask || !(o = pdf_get(d, obj)) || o->t != PO_DICT || (mo = smask_of(o)) < 0 ||
	    !mask_load(d, mo, w, h, max_bytes, &m))
		return image_rows(d, obj, w, h, row, u, max_bytes, why);
	m.row = row;
	m.u = u;
	m.tmp = malloc((size_t)w * 2);
	if (!m.tmp) {
		free(m.a);
		return image_rows(d, obj, w, h, row, u, max_bytes, why);
	}
	r = image_rows(d, obj, w, h, row_through, &m, max_bytes, why);
	free(m.tmp);
	free(m.a);
	return r;
}

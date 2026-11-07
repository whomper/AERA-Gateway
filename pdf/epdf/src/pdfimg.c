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
 * JPXDecode and JBIG2Decode can't be shown (PIC_UNSUPPORTED).
 *
 * pic_poll (pic.h) is called every few rows here too: the caller's turn
 * for the machine, and its way to stop.
 */
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pic.h"
#include "ccitt.h"
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

/* a CCITT scan: decoded a row at a time, shrunk to grey (rscale), then
   sampled to w x h */
static PIC *from_ccitt(PDOC *d, POBJ *o, const unsigned char *s, long n, long sw, long sh, int w, int h, int *why)
{
	POBJ *dp = pd_get(d, o, "DecodeParms");
	long k = 0, cols = sw;
	int black1 = 0, f = 1, align = 0;
	CCITT c;
	RSCALE rs;
	unsigned char *row, *g = NULL, *grey;
	long gw, gh = 0, y;
	PIC *p = NULL;

	if (dp && dp->t == PO_ARR && dp->a)
		dp = pdf_res(d, pa_at(dp, dp->a - 1));
	if (dp && dp->t == PO_DICT) {
		POBJ *b;

		k = pd_int(d, dp, "K", 0);
		cols = pd_int(d, dp, "Columns", sw);
		b = pd_get(d, dp, "BlackIs1");
		black1 = b && b->t == PO_BOOL && b->a;
		b = pd_get(d, dp, "EncodedByteAlign");
		align = b && b->t == PO_BOOL && b->a;
	}
	if (k > 0) {
		*why = PIC_UNSUPPORTED;		/* group 3, two-dimensional */
		return NULL;
	}
	while (f < 8 && cols / (f * 2) >= w)
		f *= 2;
	if (ccitt_open(&c, s, n, (int)cols, (int)k, align)) {
		*why = PIC_MEMORY;
		return NULL;
	}
	gw = f > 1 ? cols / f : cols;
	row = malloc((size_t)(cols + 7) / 8);
	grey = malloc((size_t)gw * (size_t)(sh / f + 1));
	if (!row || !grey || (f > 1 && rs_init(&rs, (int)cols, f))) {
		free(row);
		free(grey);
		ccitt_close(&c);
		*why = PIC_MEMORY;
		return NULL;
	}
	for (y = 0; y < sh && ccitt_row(&c, row); y++) {
		long i;

		if (!black1)
			for (i = 0; i < (cols + 7) / 8; i++)
				row[i] = (unsigned char)~row[i];
		/* the rows come as 1 for black: to rscale's ink */
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
	g = grey;
	p = from_samples(g, gw * gh, gw, gh, 8, 1, NULL, 0, w, h, why);
out:
	if (f > 1)
		rs_close(&rs);
	ccitt_close(&c);
	free(row);
	free(grey);
	return p;
}

PIC *pdf_image(PDOC *d, long obj, int w, int h, long max_bytes, int *why)
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
	if (pdf_is_name(pd_get(d, o, "ImageMask"), "true") || ((cs = pd_get(d, o, "ImageMask")) && cs->t == PO_BOOL && cs->a))
		bpc = 1, cs = NULL, nc = 1, invert = 1;	/* a stencil: painted where 0 */
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
	else if (term)
		*why = PIC_UNSUPPORTED;		/* JPX, JBIG2 */
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
	long row_bytes = (sw * comps * bpc + 7) / 8, sy, x, *xs;
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
	src = pdf_stream_open(d, o, obj);
	if (!src) {
		const char *term = NULL;
		long oom = d->st.oom;

		if (pdf_stream(d, o, obj, &whole, &term, max_bytes) != 0) {
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
				*why = PIC_DAMAGED;	/* the data stops short: as pdf_image says */
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

/* src: the coded bytes pulled a piece at a time; else s, n whole */
static int rows_ccitt(PDOC *d, POBJ *o, PCSRC *src, const unsigned char *s, long n, long sw, long sh, ROWS *out, int *why)
{
	POBJ *dp = pd_get(d, o, "DecodeParms");
	long k = 0, cols = sw, gw, gh, gi = 0, y, x, *xs = NULL;
	int black1 = 0, f = 1, align = 0, ok = 1;
	CCITT c;
	RSCALE rs;
	unsigned char *row, *grey;
	GMK m;

	if (dp && dp->t == PO_ARR && dp->a)
		dp = pdf_res(d, pa_at(dp, dp->a - 1));
	if (dp && dp->t == PO_DICT) {
		POBJ *b;

		k = pd_int(d, dp, "K", 0);
		cols = pd_int(d, dp, "Columns", sw);
		b = pd_get(d, dp, "BlackIs1");
		black1 = b && b->t == PO_BOOL && b->a;
		b = pd_get(d, dp, "EncodedByteAlign");
		align = b && b->t == PO_BOOL && b->a;
	}
	if (k > 0) {
		*why = PIC_UNSUPPORTED;		/* group 3, two-dimensional */
		return 0;
	}
	while (f < 8 && cols / (f * 2) >= out->w)
		f *= 2;
	if (src ? ccitt_open_pull(&c, scan_fill, src, (int)cols, (int)k, align) : ccitt_open(&c, s, n, (int)cols, (int)k, align)) {
		*why = PIC_MEMORY;
		return 0;
	}
	gw = f > 1 ? cols / f : cols;
	gh = f > 1 ? sh / f : sh;		/* the grey rows there will be: pdf_image's gh */
	row = malloc((size_t)(cols + 7) / 8);
	grey = malloc((size_t)(f > 1 ? (cols + f - 1) / f : cols));	/* (rs_row's row: its width rounded up) */
	xs = malloc((size_t)out->w * sizeof *xs);
	if (!row || !grey || !xs || (f > 1 && rs_init(&rs, (int)cols, f))) {
		free(row);
		free(grey);
		free(xs);
		ccitt_close(&c);
		*why = PIC_MEMORY;
		return 0;
	}
	for (x = 0; x < out->w; x++)
		xs[x] = x * gw / out->w;
	m.g = grey;
	m.xs = xs;
	m.w = out->w;
	for (y = 0; y < sh && out->ny < out->h && ccitt_row(&c, row); y++) {
		long i;
		int full = 1;

		if (!black1)
			for (i = 0; i < (cols + 7) / 8; i++)
				row[i] = (unsigned char)~row[i];
		/* the rows come as 1 for black: to rscale's ink */
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
	ccitt_close(&c);
	free(row);
	free(grey);
	free(xs);
	return ok;
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
	long sw, sh, rbytes, y;
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
	src = pdf_stream_open(d, o, obj);
	if (!src) {
		const char *term = NULL;
		long oom = d->st.oom;

		if (pdf_stream(d, o, obj, &whole, &term, max_bytes) != 0 || term || rbytes * sh > whole.len) {
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

int pdf_image_rows(PDOC *d, long obj, int w, int h, PIMG_ROW row, void *u, long max_bytes, int *why)
{
	POBJ *o = pdf_get(d, obj), *cs, *fl;
	long sw, sh;
	int bpc, nc, hival = 0, invert = 0, r;
	unsigned char pal[256 * 3];
	const char *fn = NULL;
	ROWS out;

	*why = PIC_DAMAGED;
	if (!o || o->t != PO_DICT || w <= 0 || h <= 0)
		return 0;
	fl = pd_get(d, o, "Filter");
	if (fl && fl->t == PO_ARR && fl->a)
		fl = pdf_res(d, pa_at(fl, fl->a - 1));
	if (fl && fl->t == PO_NAME)
		fn = fl->u.p;
	if (fn && (!strcmp(fn, "JPXDecode") || !strcmp(fn, "JBIG2Decode")))
		return -1;			/* not shown: pdf_image says so */
	if (fn && (!strcmp(fn, "DCTDecode") || !strcmp(fn, "DCT")))
		return rows_jpeg(d, o, obj, w, h, row, u, why);
	sw = pd_int(d, o, "Width", 0);
	sh = pd_int(d, o, "Height", 0);
	bpc = (int)pd_int(d, o, "BitsPerComponent", 8);
	if (pdf_is_name(pd_get(d, o, "ImageMask"), "true") || ((cs = pd_get(d, o, "ImageMask")) && cs->t == PO_BOOL && cs->a))
		bpc = 1, cs = NULL, nc = 1, invert = 1;	/* a stencil: painted where 0 */
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
	} else
		r = rows_samples(d, o, obj, sw, sh, bpc, nc, pal, invert, &out, max_bytes, why);
	free(out.out);
	if (r)
		*why = PIC_OK;
	return r;
}

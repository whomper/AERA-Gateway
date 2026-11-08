/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * cvjpeg.c - a PDF's JPEG (DCTDecode) for EConvert, through libjpeg-turbo:
 * every kind (progressive, CMYK, YCCK), its colours as the PDF means them.
 * A CMYK JPEG in a PDF is ink as stored unless its /Decode says it is
 * inverted ([1 0 1 0 1 0 1 0], Photoshop's); common's pic.c takes CMYK the
 * web's way, always inverted (an Adobe marker's), which turned a map's
 * relief black (3 Nov 2026).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jpeglib.h"
#include "jerror.h"
#include "cvjpeg.h"
#include "pdfimg.h"

typedef struct {
	struct jpeg_error_mgr pub;
	void *jmp[5];
} ERR;

static void err_exit(j_common_ptr c)
{
	ERR *e = (ERR *)c->err;

	__builtin_longjmp(e->jmp, 1);
}

static void err_out(j_common_ptr c)
{
	(void)c;
}

static void err_emit(j_common_ptr c, int level)
{
	(void)c;
	(void)level;
}

static void err_fmt(j_common_ptr c, char *buf)
{
	(void)c;
	buf[0] = 0;
}

static void err_reset(j_common_ptr c)
{
	c->err->num_warnings = 0;
	c->err->msg_code = 0;
}

static void src_init(j_decompress_ptr c) { (void)c; }
static boolean src_fill(j_decompress_ptr c)
{
	static const JOCTET eoi[2] = { 0xFF, JPEG_EOI };

	c->src->next_input_byte = eoi;	/* a cut stream ends */
	c->src->bytes_in_buffer = 2;
	return TRUE;
}
static void src_skip(j_decompress_ptr c, long n)
{
	if (n > 0) {
		if ((size_t)n > c->src->bytes_in_buffer)
			n = (long)c->src->bytes_in_buffer;
		c->src->next_input_byte += n;
		c->src->bytes_in_buffer -= (size_t)n;
	}
}
static void src_term(j_decompress_ptr c) { (void)c; }

PIC *cv_jpeg(const unsigned char *d, long n, int w, int h, int inverted, int *why)
{
	struct jpeg_decompress_struct ci;
	struct jpeg_source_mgr src;
	ERR err;
	PIC *volatile p = NULL;
	unsigned char *volatile row = NULL;
	long *volatile ys = NULL;
	int k;

	*why = PIC_DAMAGED;
	if (w <= 0 || h <= 0)
		return NULL;
	memset(&ci, 0, sizeof ci);
	/* (libjpeg's jerror.c is not built: common's pic.c has its own) */
	memset(&err, 0, sizeof err);
	err.pub.error_exit = err_exit;
	err.pub.emit_message = err_emit;
	err.pub.output_message = err_out;
	err.pub.format_message = err_fmt;
	err.pub.reset_error_mgr = err_reset;
	ci.err = &err.pub;
	if (__builtin_setjmp(err.jmp)) {
		if (err.pub.msg_code == JERR_OUT_OF_MEMORY)
			*why = PIC_MEMORY;
		jpeg_destroy_decompress(&ci);
		free(row);
		free(ys);
		if (p && p->px) {
			/* what was made up to the damage is kept */
			*why = PIC_OK;
			return p;
		}
		pic_free(p);
		return NULL;
	}
	jpeg_create_decompress(&ci);
	src.init_source = src_init;
	src.fill_input_buffer = src_fill;
	src.skip_input_data = src_skip;
	src.resync_to_restart = jpeg_resync_to_restart;
	src.term_source = src_term;
	src.next_input_byte = d;
	src.bytes_in_buffer = (size_t)n;
	ci.src = &src;
	jpeg_read_header(&ci, TRUE);
	ci.out_color_space = ci.jpeg_color_space == JCS_CMYK || ci.jpeg_color_space == JCS_YCCK ? JCS_CMYK :
			     ci.num_components == 1 ? JCS_GRAYSCALE : JCS_RGB;
	for (k = 1; k < 8; k++)
		if (((long)ci.image_width * k + 7) / 8 >= w && ((long)ci.image_height * k + 7) / 8 >= h)
			break;
	ci.scale_num = (unsigned)k;
	ci.scale_denom = 8;
	jpeg_start_decompress(&ci);
	p = calloc(1, sizeof *p);
	if (!p || !(p->px = malloc((size_t)w * h * 2)) ||
	    !(row = malloc((size_t)ci.output_width * ci.output_components)) || !(ys = malloc((size_t)h * sizeof *ys))) {
		*why = PIC_MEMORY;
		jpeg_destroy_decompress(&ci);
		free(row);
		free(ys);
		pic_free(p);
		return NULL;
	}
	p->w = w;
	p->h = h;
	p->how = PIC_JPEG;
	memset(p->px, 0xFF, (size_t)w * h * 2);
	{
		long y, x, oy = 0, ow = (long)ci.output_width, oh = (long)ci.output_height;
		int nc = ci.output_components;

		/* each output row from the source row at its middle (nearest);
		   each pixel from the source pixels it covers, averaged */
		for (y = 0; y < h; y++)
			ys[y] = (y * 2 + 1) * oh / (2L * h);
		for (y = 0; y < oh && oy < h; y++) {
			JSAMPROW r = row;

			jpeg_read_scanlines(&ci, &r, 1);
			while (oy < h && ys[oy] == y) {
				unsigned short *o = p->px + oy * w;

				for (x = 0; x < w; x++) {
					long a = x * ow / w, b = (x + 1) * ow / w, i, sr = 0, sg = 0, sb = 0, cnt;
					int R, G, B;

					if (b <= a)
						b = a + 1;
					cnt = b - a;
					for (i = a; i < b; i++) {
						const unsigned char *s = row + i * nc;

						if (nc == 1) {
							sr += s[0];
							sg += s[0];
							sb += s[0];
						} else if (nc == 3) {
							sr += s[0];
							sg += s[1];
							sb += s[2];
						} else {
							/* ink, or its inverse (Decode [1 0 ...]) */
							int c0 = s[0], m0 = s[1], y0 = s[2], k0 = s[3];

							if (inverted) {
								c0 = 255 - c0;
								m0 = 255 - m0;
								y0 = 255 - y0;
								k0 = 255 - k0;
							}
							sr += (255 - c0) * (255 - k0) / 255;
							sg += (255 - m0) * (255 - k0) / 255;
							sb += (255 - y0) * (255 - k0) / 255;
						}
					}
					R = (int)(sr / cnt);
					G = (int)(sg / cnt);
					B = (int)(sb / cnt);
					o[x] = (unsigned short)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3));
				}
				oy++;
			}
		}
		jpeg_abort_decompress(&ci);
	}
	jpeg_destroy_decompress(&ci);
	free(row);
	free(ys);
	*why = PIC_OK;
	return p;
}

/* an image XObject as a picture of w x h: a JPEG through cv_jpeg, the
   rest through the reader's pdf_image */
/* a row the row decoder made, into the picture */
static void row_in(void *u, long y, const unsigned short *px)
{
	PIC *p = u;

	if (y >= 0 && y < p->h)
		memcpy(p->px + y * (long)p->w, px, (size_t)p->w * 2);
}

/* the picture made a row at a time (pdf_image_rows): only its w x h in
   memory, never its decoded stream (EDOS_01.PDF's 1368 x 815 CMYK
   screenshot was 4.4 MB of samples, refused on a 4 MB Falcon, the file
   lost); 1 made, 0 not (why), -1 a kind it doesn't make */
static int by_rows(PDOC *d, long obj, int w, int h, PIC **out, int *why)
{
	PIC *p = calloc(1, sizeof *p);
	int r;

	*out = NULL;
	if (!p || !(p->px = malloc((size_t)w * h * 2))) {
		free(p);
		*why = PIC_MEMORY;
		return 0;
	}
	p->w = w;
	p->h = h;
	r = pdf_image_rows(d, obj, w, h, row_in, p, 32L << 20, why);
	if (r == 1)
		*out = p;
	else
		pic_free(p);
	return r;
}

int cv_jpeg_pic;

/* the JPEG's components (its frame header's), 0 if none is found */
static int jpeg_comps(const unsigned char *d, long n)
{
	long i = 2;

	while (i + 9 < n && d[i] == 0xFF) {
		int m = d[i + 1];
		long len = (long)d[i + 2] << 8 | d[i + 3];

		if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC)
			return d[i + 9];
		if (m == 0xFF) {
			i++;
			continue;
		}
		i += 2 + len;
	}
	return 0;
}

PIC *cv_image(PDOC *d, long obj, int w, int h, int *why)
{
	POBJ *o = pdf_get(d, obj), *fl, *dec;
	const char *term = NULL;
	SBUF b;
	PIC *p;
	int inv;

	*why = PIC_DAMAGED;
	if (!o || o->t != PO_DICT)
		return NULL;
	fl = pd_get(d, o, "Filter");
	if (fl && fl->t == PO_ARR && fl->a)
		fl = pdf_res(d, &fl->u.v[fl->a - 1]);
	if (!fl || fl->t != PO_NAME || (strcmp(fl->u.p, "DCTDecode") && strcmp(fl->u.p, "DCT"))) {
		PIC *q;
		int r = by_rows(d, obj, w, h, &q, why);

		if (r >= 0)
			return q;
		return pdf_image(d, obj, w, h, 32L << 20, why);
	}
	dec = pd_get(d, o, "Decode");
	inv = dec && dec->t == PO_ARR && dec->a >= 2 && pa_at(dec, 0)->a > pa_at(dec, 1)->a;
	sb_init(&b);
	if (pdf_stream(d, o, obj, &b, &term, 32L << 20) != 0 || !term || strcmp(term, "DCT")) {
		sb_free(&b);
		return pdf_image(d, obj, w, h, 32L << 20, why);
	}
	p = NULL;
	if (cv_jpeg_pic && b.len > 4) {
		int nc = jpeg_comps((const unsigned char *)b.s, b.len);

		/* (CMYK stays cv_jpeg's: pic.c takes it as always inverted) */
		if (nc == 1 || nc == 3)
			p = pic_decode((const unsigned char *)b.s, b.len, w, h, 32L << 20, why);
	}
	if (!p)
		p = cv_jpeg((const unsigned char *)b.s, b.len, w, h, inv, why);
	sb_free(&b);
	return p;
}

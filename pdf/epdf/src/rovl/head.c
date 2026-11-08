/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * head.c - render.ovl's side of rovl.h: the ROVL table at the start of
 * its text (common/tlsovl/ovl.ld keeps its section first), and the
 * program's memory and turns through the ROVL_HOST it was given. The
 * rest is EConvert's renderer and the reader as EConvert has them.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pic.h"
#include "render.h"
#include "conv.h"
#include "cvjpeg.h"
#include "dspjpeg.h"
#include "rovl.h"
#include "pens.h"

#ifndef ROVL_VERSION
#define ROVL_VERSION "0.0"
#endif

extern int mem_unaligned_ok;		/* libc.c */

static const ROVL_HOST *H;
static int stopped;
static int dsp_on = 1;

typedef struct {
	PDOC d;
	RPAGE r;
} RDOC;

void *malloc(size_t n) { return H->malloc(n); }
void *calloc(size_t n, size_t m) { return H->calloc(n, m); }
void *realloc(void *p, size_t n) { return H->realloc(p, n); }
void free(void *p) { H->free(p); }

/* the renderer's turns (cv_turn) and the reader's (PDOC.stop): once the
   program says stop, the page runs out without asking again */
static int turn(void)
{
	if (!stopped && H->turn && H->turn())
		stopped = 1;
	return stopped;
}

static int reader_turn(void *u)
{
	(void)u;
	return turn();
}

static int init(const ROVL_HOST *h)
{
	if (!h || h->size < (long)sizeof(ROVL_HOST))
		return 0;
	H = h;
	mem_unaligned_ok = 1;		/* a 68020 or later: the program checked */
	cv_jpeg_pic = 1;		/* the JPEGs through pic.c: on the DSP */
	return 1;
}

static void *rv_open(const char *path, const char *fontdir, long font_budget, long canvas_max, char *err, int errlen)
{
	RDOC *x = calloc(1, sizeof *x);

	if (!x) {
		snprintf(err, (size_t)errlen, "not enough memory");
		return NULL;
	}
	stopped = 0;
	cv_turn = turn;
	if (pdf_open_hook(&x->d, path, reader_turn, NULL)) {
		snprintf(err, (size_t)errlen, "%s", x->d.err[0] ? x->d.err : "can't be read");
		free(x);
		return NULL;
	}
	if (render_init(&x->r, &x->d, fontdir)) {
		snprintf(err, (size_t)errlen, "not enough memory");
		pdf_close(&x->d);
		free(x);
		return NULL;
	}
	render_font_budget = font_budget;
	x->r.canvas_max = canvas_max;
	return x;
}

static long rv_pages(void *doc)
{
	return pdf_pages(&((RDOC *)doc)->d);
}

static unsigned long ms(void)
{
	return (unsigned long)pdf_ms();
}

static int rv_page(void *doc, long n, int width, int y0, ROVL_PAGE *out)
{
	RDOC *x = doc;
	int rc;

	stopped = 0;
	x->d.stopping = 0;
	cv_turn = turn;
	render_clock = ms;
	pic_dsp = dsp_on && dspjpeg_present() ? &dspjpeg_port_jpeg : NULL;
	rc = render_page_from(&x->r, n, width, y0);
	memset(out, 0, sizeof *out);
	if (stopped || x->d.stopping)
		return -2;
	if (rc)
		return -1;
	out->w = x->r.c.w;
	out->h = x->r.c.h;
	out->y0 = x->r.c.y0;
	/* (the last band's rows only as far as the page goes) */
	out->rows = x->r.c.rows < x->r.c.h - x->r.c.y0 ? x->r.c.rows : x->r.c.h - x->r.c.y0;
	out->px = x->r.c.px;
	out->glyphs = x->r.glyphs;
	out->images = x->r.images;
	out->glyphs_missed = x->r.glyphs_missed;
	out->images_missed = x->r.images_missed;
	out->image_ms = x->r.image_ms;
	return 0;
}

static void rv_close(void *doc)
{
	RDOC *x = doc;

	if (!x)
		return;
	render_done(&x->r);
	pdf_close(&x->d);
	free(x);
	pens_free();
	cv_turn = NULL;
}

static int rv_progress(void *doc)
{
	return doc ? ((RDOC *)doc)->d.page_prog : 0;
}

static int rv_pens(void *doc, const unsigned char (*pal)[3], int n, int planes, int y, int rows, unsigned char *out)
{
	RCANVAS *c = &((RDOC *)doc)->r.c;

	if (y < 0 || rows <= 0 || y + rows > c->rows || !c->px || planes < 1 || planes > 4)
		return -1;
	return pens_planes(c->px + (long)y * c->w, c->w, rows, c->y0 + y, pal, n, planes, out);
}

static void use_dsp(int on)
{
	dsp_on = on;
}

__attribute__((section(".tlsovl"), used))
const ROVL rovl = {
	ROVL_MAGIC, ROVL_MAJOR, sizeof(ROVL), ROVL_VERSION,
	init, rv_open, rv_pages, rv_page, rv_close, use_dsp, pic_dsp_state, rv_progress, rv_pens
};

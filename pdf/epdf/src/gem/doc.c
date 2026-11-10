/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * doc.c - the open document: the reader (src/pdf*.c), its pages' text in
 * the store (src/view/tstore.c: memory, then EPDF.TXC beside the program)
 * and the view of them (src/view/docview.c).
 *
 * The text is read a page at a time, the pages from the one shown
 * onwards first, and never as one stretch (the guidelines' Speed): the
 * reading runs on a stack of its own (coro.h), and the reader's hook
 * (d->stop, every 256 operators) hands the machine back to the event
 * loop when the slice is up; the next slice goes on where it was.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "gemui.h"
#include "charset.h"
#include "pdf.h"
#include "plat.h"
#include "tstore.h"
#include "crash.h"
#include "docview.h"
#include "epdf.h"
#include "coro.h"
#include "pdfimg.h"
#include "pics.h"
#include "rdraw.h"

static PDOC *doc;
static TSTORE store;
static DOCVIEW view;
static char name[64];
static long npages;
static long next;			/* where the reading goes on */
static long done;			/* pages read */
static long cache_kb;			/* the store's memory */

/*
 * Two stacks of their own, each worked a slice at a time: the pages' text
 * is read on one, a picture the window shows is decoded on the other, so
 * the text goes on being read while a big JPEG takes minutes on an ST.
 * They share the document: a picture uses its objects only before its
 * first turn is handed back (then it works on its own buffers), and a page
 * holding objects keeps them across a picture's lookups (the page's memory
 * goes only when the next page starts).
 */
/* four times the most they were seen to use (an EPDF_STACKS build on the
   corpus: the reading 4 KB, a JPEG 2.3 KB; make build/stk/EPDF.PRG) */
#define READ_STACK 16384L
#define PIC_STACK  12288L
/* a 68020 or better: the pictures' stack draws pages too (render.ovl:
   FreeType and the reader, EConvert's job used 31 KB of its) */
#define DRAW_STACK 65536L
static CORO main_co, read_co, pic_co;
static CORO *running;			/* the stack working now */
static char *read_stack, *pic_stack;
static long pic_stack_size;
static int read_started, pic_started;
static long rpage = -1;			/* the page being read, -1 none */
static int rdone;			/* the reader has finished rpage */
static int abandon;			/* the document closes: stop the page and the picture */
/* a document being opened, on the reader's stack (pdf_open_hook with the
   slices' hook): doc stays NULL until it is done */
static PDOC *odoc;
static char opath[256], ocache[256];
/* the PDF a copy shown was made from (the Gateway's: its name the
   window's, File > Save Optimised...'s and the page drawn bigger's) */
static char orig[256];
static int opening, open_ok, ocols, olines;
static long onpages;
static unsigned long slice_end;		/* tos_ms() when the slice is up */
static PTEXT *rt;			/* the page read (taken with the document) */
static PPREP rprep;			/* a converted PDF's page read: its whole pictures */
static SBUF rtrans;			/* and its /Translation, when it has one (rprep.trans) */
static int rtrans_ok;

/* View > Translate (Erez, 8 Nov 2026; AERA-Atari standards/TRANSLATION-
   SERVICE.md): each page's translation in a store of its own, in memory
   only (never on the Atari's disk), over the pages as read (trstore.under:
   a page not translated yet shows as it is). The view shows it while
   Translate is on; the original store is untouched, so Off shows the
   pages as they are at once */
static TSTORE trstore;
static int tr_open, tr_mode;
static signed char *trst;		/* each page: TR_* */
static char (*trfrom)[8];		/* the language it was in ("" not known) */
static char tr_target[8];		/* the language asked for */
static long tr_reread;			/* a converted PDF's pages read again for their /Translation: left to do */

/* a converted PDF the Gateway translated into the language asked for:
   its pages' /Translation shown, nothing asked of translategate */
static int tr_from_file(void)
{
	return tr_open && doc && doc->prepared && doc->tr_to[0] && !strcmp(doc->tr_to, tr_target);
}


/* a converted PDF's pages whole (View > Pages): each page a picture's line,
   its own picture (16 colours where the screen has them, else mono) */
static TSTORE pstore;
static int pstore_open, pages_mode, pages_have;
/* any other PDF on a Falcon or TT (stage 2): its pages drawn as printed by
   render.ovl, each page a picture of its own (PICS_PAGE + the page) */
static int drawn;
static long rbox_w, rbox_h;		/* the page read: its size in points, turned as shown */
static void *rdoc;			/* render.ovl's document, opened at the first page drawn */
static long draw_page = -1;		/* the page render.ovl draws now, -1 none */
static int draw_rows;			/* its rows are being shown */
char doc_font_dir[200] = "fonts";	/* the standard fonts' (epdf.c: beside the program) */

/* each page's picture View > Pages shows and its double (EConvert's for
   a machine with the room: shown whole when the page is clicked), its
   size: kept by page, read on the main stack (doc_double_of) */
static struct {
	long obj, dbl, w, h;
	long stand;			/* the stored page shown till the sharp one is drawn (0: none) */
} *pdbl;

/* a converted copy's pages drawn sharp on a Falcon or TT in colour, from
   its original by render.ovl, over the stored page (Erez, 10 Nov 2026: the
   Pages view's stored 16 colours looked worse than the text view's
   pictures): -1 not known yet for this document */
static int sharp = -1;

static int sharp_pages(void)
{
	if (sharp < 0)
		sharp = doc && doc->prepared && planes >= 4 && rdraw_machine() && pf_size(rdraw_path) > 0 &&
			orig[0] && pf_size(orig) > 0;
	return sharp;
}

static void pages_note(long n, const PPREP *pp)
{
	char b[48];
	/* (colour only: a mono screen shows the colour page, dithered) */
	int col = pp->colour >= 0 && (planes >= 4 || pp->mono < 0);
	long obj = col ? pp->colour : pp->mono, w = col ? pp->colour_w : pp->mono_w, h = col ? pp->colour_h : pp->mono_h;
	int k = 0;
	long stand;

	if (!pstore_open)
		return;
	if (w <= 0 || h <= 0) {
		w = 640;		/* (a file of 1.1's) */
		h = pp->w > 0 ? 640L * pp->h / pp->w : 828L;
	}
	stand = 0;
	/* (where memory holds the stored page, the sharp one and render.ovl's
	   canvas whole: on a 4 MB Falcon it didn't, its canvas went to bands,
	   33 s, and the sharp page failed) */
	if (obj >= 0 && sharp_pages() &&
	    (long)Malloc(-1L) >= 2 * ((w + 15) / 16 * 2 * planes * h) + w * h * 2 + 768 * 1024L) {
		stand = obj;		/* (shown till render.ovl's is drawn) */
		obj = PICS_PAGE + n;
	}
	if (obj >= 0) {
		/* (its size the picture's own: shown as it is made where the
		   window has room, not a point a pixel) */
		k = snprintf(b, sizeof b, "\x7F%ld %ldx%ld\n", obj, w, h);
		pages_have = 1;
	}
	if (!pdbl && onpages > 0)
		pdbl = calloc((size_t)onpages, sizeof *pdbl);
	if (pdbl && n >= 0 && n < onpages) {
		pdbl[n].obj = obj;
		pdbl[n].dbl = pp->dbl >= 0 && pp->dbl_w > w ? pp->dbl : -1;
		pdbl[n].w = pp->dbl_w;
		pdbl[n].h = pp->dbl_h;
		pdbl[n].stand = stand;
	}
	ts_put(&pstore, n, b, k);		/* (none: the page's head alone) */
}

/* a page to be drawn as printed: its line, 640 wide at its shape */
static void pages_drawn_note(long n)
{
	char b[48];
	long h;
	int k;

	if (!pstore_open)
		return;
	if (rbox_w <= 0 || rbox_h <= 0) {
		rbox_w = 612;		/* (no box: a Letter page) */
		rbox_h = 792;
	}
	h = 640L * rbox_h / rbox_w;
	if (h < 8)
		h = 8;
	if (h > 4096)
		h = 4096;
	k = snprintf(b, sizeof b, "\x7F%ld 640x%ld\n", PICS_PAGE + n, h);
	pages_have = 1;
	ts_put(&pstore, n, b, k);
}

/* the page read's size, on the reader's stack */
static void page_box(long n)
{
	POBJ *res, *box;
	long rot = 0, v[4] = { 0, 0, 612, 792 };
	int k;

	if (!pdf_page(doc, n, &res, &box, &rot))
		return;
	if (box && box->t == PO_ARR && box->a >= 4)
		for (k = 0; k < 4; k++) {
			POBJ *o = pa_at(box, k);

			v[k] = !o ? 0 : o->t == PO_INT ? o->a : o->t == PO_REAL ? (long)(o->a >> 16) : 0;
		}
	rbox_w = v[2] > v[0] ? v[2] - v[0] : v[0] - v[2];
	rbox_h = v[3] > v[1] ? v[3] - v[1] : v[1] - v[3];
	rot = ((rot % 360) + 360) % 360;
	if (rot == 90 || rot == 270) {
		long t = rbox_w;

		rbox_w = rbox_h;
		rbox_h = t;
	}
}

int doc_double_of(long obj, long *dbl, long *w, long *h)
{
	long n;

	for (n = 0; pdbl && n < onpages; n++)
		if (pdbl[n].obj == obj && pdbl[n].dbl >= 0) {
			*dbl = pdbl[n].dbl;
			*w = pdbl[n].w;
			*h = pdbl[n].h;
			return 1;
		}
	return 0;
}

/* the stored page shown for page picture obj (PICS_PAGE + n) till it is
   drawn sharp: its object, or -1 */
long doc_stand_in(long obj)
{
	long n = obj - PICS_PAGE;

	return obj >= PICS_PAGE && pdbl && n >= 0 && n < onpages && pdbl[n].stand > 0 ? pdbl[n].stand : -1;
}

int doc_has_pages(void)
{
	return doc && (doc->prepared || drawn) && pages_have;
}

int doc_page_drawable(void)
{
	return doc && (drawn || (doc->prepared && rdraw_machine() && pf_size(rdraw_path) > 0));
}

int doc_drawn(void)
{
	return doc && drawn;
}

int doc_pages_mode(void)
{
	return doc && pages_mode;
}

int doc_prepared(void)
{
	return doc && doc->prepared;
}

static void page_pic_stop(void);

/* the view on the pages' text or on their pictures: laid out again from
   the page at the top (the caller lays the window out and draws it) */
static TSTORE *text_store(void);

void doc_set_pages(int on)
{
	long top;
	int cols, lm;

	if (!doc || !pstore_open || !on == !pages_mode)
		return;
	if (!on)
		page_pic_stop();
	top = dv_page_at_top(&view);
	cols = view.cols;
	lm = view.lines_mode;
	dv_free(&view);
	pages_mode = on;
	if (dv_init(&view, on ? &pstore : text_store(), npages, cols, lm) != 0) {
		pages_mode = 0;
		dv_init(&view, &store, npages, cols, lm);
	}
	dv_goto_page(&view, top);
}

/* ---------------------------------------------------------------- View > Translate */

/* the store the text view shows */
static TSTORE *text_store(void)
{
	return tr_mode && tr_open ? &trstore : &store;
}

int doc_tr_begin(const char *to)
{
	long budget;

	if (!doc)
		return -1;
	if (tr_open && strcmp(tr_target, to))
		doc_tr_forget();	/* (another language: those translations are no use) */
	if (tr_open)
		return 0;
	snprintf(tr_target, sizeof tr_target, "%s", to);
	/* in memory alone: an eighth of what is free, 32 to 512 KB */
	budget = (long)Malloc(-1L) / 8;
	budget = budget < 32768L ? 32768L : budget > 524288L ? 524288L : budget;
	if (ts_open(&trstore, npages, budget, "") != 0)
		return -1;
	trst = calloc((size_t)npages, 1);
	trfrom = calloc((size_t)npages, sizeof *trfrom);
	if (!trst || !trfrom) {
		free(trst);
		free(trfrom);
		trst = NULL;
		trfrom = NULL;
		ts_close(&trstore);
		return -1;
	}
	trstore.under = &store;
	tr_open = 1;
	/* a converted PDF translated into it: its pages read again for their
	   /Translation, the page in view first (doc_slice) */
	tr_reread = tr_from_file() ? npages : 0;
	return 0;
}

int doc_tr_from_file(void)
{
	return tr_from_file();
}

/* the translations dropped (another language, the document closed) */
void doc_tr_forget(void)
{
	if (!tr_open)
		return;
	if (tr_mode && !pages_mode)
		doc_set_translate(0);
	tr_mode = 0;
	ts_close(&trstore);
	free(trst);
	free(trfrom);
	trst = NULL;
	trfrom = NULL;
	tr_open = 0;
}

void doc_set_translate(int on)
{
	long top;
	int cols, lm;

	if (!doc || (on && !tr_open) || !on == !tr_mode)
		return;
	tr_mode = on;
	if (pages_mode)
		return;			/* (the pages' pictures stay; the text when Pages goes off) */
	top = dv_page_at_top(&view);
	cols = view.cols;
	lm = view.lines_mode;
	dv_free(&view);
	if (dv_init(&view, text_store(), npages, cols, lm) != 0) {
		tr_mode = 0;
		dv_init(&view, &store, npages, cols, lm);
	}
	dv_goto_page(&view, top);
}

int doc_tr_state(long n)
{
	return tr_open && n >= 0 && n < npages ? trst[n] : TR_NONE;
}

const char *doc_tr_from(long n)
{
	return tr_open && n >= 0 && n < npages ? trfrom[n] : "";
}

int doc_tr_put(long n, int state, const char *text, long len, const char *from)
{
	if (!tr_open || n < 0 || n >= npages)
		return -1;
	if (text && ts_put(&trstore, n, text, len) != 0)
		return -1;
	trst[n] = (signed char)state;
	snprintf(trfrom[n], sizeof trfrom[n], "%s", from ? from : "");
	if (tr_mode && !pages_mode)
		dv_page_ready(&view, n);	/* (its rows counted again: the window draws them) */
	return 0;
}

/* the page's text as read (the original), for its translation */
const char *doc_page_original(long n, long *len)
{
	return doc ? ts_get(view.ts ? view.ts : &store, n, len) : NULL;
}

/* the document's own translation's language (a converted PDF the Gateway translated) */
const char *doc_tr_file_to(void)
{
	return doc && doc->prepared ? doc->tr_to : "";
}
static long rpage_oom;			/* the reader's refused allocations before it */
static PICE *rpic;			/* the picture being decoded, NULL none */
static int pdone;			/* the decoder has finished rpic */
static int rpic_planes;
static PIC_PAL *rpic_pal;
static unsigned char *rpic_std;
static long rpic_bytes;
static int rpic_why;
static int rpic_rows;			/* made a row at a time into rpic->bm (pics_rows): rpic_ok */
static int rpic_ok;
static long rpic_shown;			/* its rows the window has been told of */
/* memory refused (0.3): the store gives its pool back and the page or the
   picture is tried again, once */
static long retried_page = -1;
static long retried_obj = -1;
#define STORE_KEEP (16L * 1024)

/* the reader's hook: the machine back to the event loop when the slice
   is up; 1 (stop) when the document is closing */
static int slice_up(void *ctx)
{
	(void)ctx;
	if (abandon)
		return 1;
	if (!running)
		return 0;	/* on the main stack (a document's object read there): nothing to hand back */
	if (tos_ms() >= slice_end) {
		coro_switch(running, &main_co);
		if (abandon)
			return 1;
	}
	return 0;
}

/* pic.h's poll, while a picture is decoded: the same hand-over */
static int pic_turn(void)
{
	return !slice_up(NULL);
}

/* the text store's memory given back for a page or a picture refused it:
   1 when there is enough to try again */
static int give_back(const char *what)
{
	long b = ts_shrink(&store, STORE_KEEP);
	char a[24];

	snprintf(a, sizeof a, "%ld", b);
	log_event("memory-back", what, a);
	if (b > 0)
		cache_kb = store.budget / 1024;
	return b >= 4096;
}

/* the pages' stack: a page at a time, for good */
static void reader(void)
{
	for (;;) {
		if (opening) {
			open_ok = pdf_open_hook(odoc, opath, slice_up, NULL) == 0 && (onpages = pdf_pages(odoc)) > 0;
			rdone = 1;
			coro_switch(&read_co, &main_co);
			continue;
		}
		pdf_page_reset(doc);
		doc->st.mem_peak = doc->st.mem;	/* this page's own peak, for the log */
		rpage_oom = doc->st.oom;
		ptext_init(rt);
		rt->soft = 1;
		rt->pics = 1;
		if (doc->prepared) {
			/* a converted PDF: the page's text as the store keeps it */
			if (pdf_prepared_page(doc, rpage, &rt->text, &rprep) != 0)
				rt->failed = 1;
			else if (rprep.trans >= 0 && tr_from_file()) {
				/* (read only with Translate on: Off reads nothing new) */
				rtrans.len = 0;
				rtrans_ok = pdf_prepared_translation(doc, &rprep, &rtrans) == 0;
			}
		} else
			pdf_page_text(doc, rpage, rt);
		rdone = 1;
		coro_switch(&read_co, &main_co);
	}
}

/* a band of the picture's colour rows waits for pics_band, on the main
   stack: the machine handed back meanwhile */
static void band_wait(void)
{
	coro_switch(&pic_co, &main_co);
}

/* the pictures' stack: one at a time, decoded, dithered and laid out as
   the standard bitmap */
static void decoder(void)
{
	for (;;) {
		unsigned long t0 = tos_ms(), t1;
		PIC *p;

		rpic_std = NULL;
		rpic_bytes = 0;
		{
			/* a scan, samples, a JPEG TJpgDec or the DSP takes: a row at a
			   time into the window's bitmap, shown from the top down as the
			   rows come (never the whole picture in memory); the rest
			   whole, then dithered */
			int how;

			{
				/* its own size, for its click to enlarge (textwin.c):
				   read here, before the stack's first hand-back */
				POBJ *o = rpic->obj < PICS_PAGE ? pdf_get(doc, rpic->obj) : NULL;

				rpic->sw = o && o->t == PO_DICT ? pd_int(doc, o, "Width", 0) : 0;
				rpic->sh = o && o->t == PO_DICT ? pd_int(doc, o, "Height", 0) : 0;
			}
			how = pics_rows(doc, rpic, rpic_planes, rpic_pal, &rpic_why);

			rpic_rows = how >= 0;
			if (how >= 0) {
				static char a[48];

				rpic_ok = how > 0;
				if (!rpic_ok && abandon)
					rpic_why = PIC_STOPPED;
				snprintf(a, sizeof a, "rows %lu", tos_ms() - t0);
				log_event("picture-ms", a, NULL);
				pdone = 1;
				coro_switch(&pic_co, &main_co);
				continue;
			}
		}
		p = pdf_image(doc, rpic->obj, rpic->w, rpic->h, 2L << 20, &rpic_why);
		t1 = tos_ms();
		if (p) {
			static char a[48];

			rpic_std = pics_standard(p, rpic_planes, rpic_pal, rpic->flip, &rpic_bytes);
			if (!rpic_std)
				rpic_why = abandon ? PIC_STOPPED : PIC_MEMORY;
			pic_free(p);
			snprintf(a, sizeof a, "decode %lu dither %lu", t1 - t0, tos_ms() - t1);
			log_event("picture-ms", a, NULL);	/* wall clock: the turns between included */
		}
		pdone = 1;
		coro_switch(&pic_co, &main_co);
	}
}

/* render.ovl's turns: the decoder's (1 stops the page: the document closes) */
static int draw_turn(void)
{
	return slice_up(NULL);
}

/* pics_page_rows: page n drawn by render.ovl w wide, its rows to row(),
   on the decoder's stack */
static int page_rows(long n, int w, int h, void (*row)(void *u, long y, const unsigned short *px),
		     void (*planes_row)(void *u, long y, const unsigned char *planes), const PIC_PAL *pal, int planes,
		     void *u, int *why)
{
	unsigned char *pn = NULL;
	unsigned long pens_ms = 0, rows_t0;
	const ROVL *o;
	ROVL_PAGE pg;
	char err[160];
	long y = 0;
	int rc, y0 = 0, i;

	*why = PIC_DAMAGED;
	if (!rdraw_load(draw_turn, err, sizeof err)) {
		log_event("render-ovl", err, NULL);
		*why = PIC_UNSUPPORTED;
		return 0;
	}
	o = rdraw_ovl();
	if (!rdoc) {
		long fr = (long)Malloc(-1L);

		/* its canvas a page whole where it can (bands read the page again
		   for each: 4 to 15 times slower) */
		/* a converted copy's pages drawn bigger from its original
		   (Erez, 8 Nov 2026: zoomed in colour), where it is still there;
		   else from the copy itself */
		const char *from = orig[0] && pf_size(orig) > 0 ? orig : opath;

		rdoc = o->open(from, doc_font_dir, fr / 8, fr * 2 / 3, err, sizeof err);
		if (!rdoc) {
			log_event("render-open", err, NULL);
			*why = abandon ? PIC_STOPPED : PIC_MEMORY;
			return 0;
		}
	}
	draw_page = n;
	draw_rows = 0;
	do {
		unsigned long t0 = tos_ms();

		rc = o->page(rdoc, n, w, y0, &pg);
		if (rc) {
			free(pn);
			draw_page = -1;
			*why = rc == -2 ? PIC_STOPPED : PIC_MEMORY;
			return 0;
		}
		{
			char a[64];

			snprintf(a, sizeof a, "%ld ms %d rows from %d glyphs %ld images %ld", tos_ms() - t0, pg.rows, y0,
				 pg.glyphs, pg.images);
			log_event("page-drawn", a, NULL);
		}
		if (pg.images_missed > 0 && pg.images_missed == pg.images && pg.glyphs == 0) {
			/* nothing on it drawn: a page that is one picture (a
			   converted copy's) without the memory for it, white */
			free(pn);
			draw_page = -1;
			*why = PIC_MEMORY;
			return 0;
		}
		draw_rows = 1;
		rows_t0 = tos_ms();
		if (pal && pal->n >= 2 && pal->n <= 256 && planes >= 1 && planes <= 8 && o->size >= (short)sizeof(ROVL) && !pn)
			pn = malloc((size_t)((w + 15) / 16 * 2) * planes * 8);
		for (i = 0; i < pg.rows && y < h; i++, y++) {
			/* 2 to 256 colours: eight rows at a time dithered by
			   render.ovl into the screen's planes (common's dither as
			   68000 code took 35 s of a page's 39 on Falcon-4, and dotted
			   the white: 248; the pens put into planes a pixel at a
			   time were seconds more) */
			if (pn) {
				int k = i % 8;

				unsigned long q = tos_ms();

				if (k == 0 && o->pens(rdoc, (const unsigned char (*)[3])pal->rgb, pal->n, planes, i,
						      pg.rows - i < 8 ? pg.rows - i : 8, pn)) {
					free(pn);
					pn = NULL;
				}
				pens_ms += tos_ms() - q;
				if (pn)
					planes_row(u, y, pn + (long)k * ((w + 15) / 16 * 2) * planes);
				else
					row(u, y, pg.px + (long)i * pg.w);
			} else
				row(u, y, pg.px + (long)i * pg.w);
			if ((y & 7) == 7 && slice_up(NULL)) {
				free(pn);
				draw_page = -1;
				*why = PIC_STOPPED;
				return 0;
			}
		}
		{
			char a[64];

			snprintf(a, sizeof a, "%lu ms, pens %lu", tos_ms() - rows_t0, pens_ms);
			log_event("page-shown", a, NULL);
		}
		y0 += pg.rows;
	} while (pg.rows > 0 && y0 < pg.h && y < h);
	if (y < h) {
		/* (the page a row shorter than its line said: white) */
		unsigned short *wr = malloc((size_t)w * 2);

		if (wr) {
			memset(wr, 0xFF, (size_t)w * 2);
			while (y < h)
				row(u, y++, wr);
			free(wr);
		}
	}
	free(pn);
	draw_page = -1;
	return 1;
}

/* a page being drawn, not shown any more (View > Pages off): stopped, its
   slot let go */
static void page_pic_stop(void)
{
	if (!rpic || rpic->obj < PICS_PAGE)
		return;
	abandon = 1;
	while (!pdone) {
		running = &pic_co;
		coro_switch(&main_co, &pic_co);
		running = NULL;
		pics_band();
	}
	free(rpic_std);
	rpic_std = NULL;
	pics_release(rpic);
	rpic = NULL;
	abandon = 0;
	log_event("picture", "page-stopped", NULL);
}

/* a page being drawn that isn't obj at w x h stopped at once, its slot let
   go: a zoom wants its own page now (Erez, 9 Nov 2026: "as soon as the
   user zooms in, the current image viewing process need to stop and the
   zoomed in image viewing must start"); it is drawn again when it is
   wanted again */
void doc_pic_stop_page_unless(long obj, int w, int h)
{
	if (rpic && rpic->obj >= PICS_PAGE && !(rpic->obj == obj && rpic->w == w && rpic->h == h))
		page_pic_stop();
}

int doc_pic_drawing(void)
{
	return rpic && rpic->obj >= PICS_PAGE;
}

/* the page and the picture under way given up (their memory freed as
   they unwind) */
static void stop_reading(void)
{
	abandon = 1;
	if (rpage >= 0) {
		while (!rdone) {
			running = &read_co;
			coro_switch(&main_co, &read_co);
			running = NULL;
		}
		ptext_free(rt);
		rpage = -1;
	}
	if (rpic) {
		while (!pdone) {
			running = &pic_co;
			coro_switch(&main_co, &pic_co);
			running = NULL;
			pics_band();		/* (a band waiting is let go on) */
		}
		free(rpic_std);
		if (rpic_rows)
			pics_made(rpic, 0, PIC_STOPPED);
		else {
			rpic->state = PICS_FAILED;
			rpic->why = PIC_STOPPED;
		}
		rpic = NULL;
	}
	abandon = 0;
}

/* a picture the window wants: its decoding begun */
int doc_pic_start(void)
{
	PICE *e;

	if (!doc || rpic || !(e = pics_next()))
		return 0;
	{
		/* its stack: 12 KB, 64 KB once pages are drawn (render.ovl's);
		   between pictures the decoder waits at its loop's top, so it
		   starts again on a bigger one */
		/* (any document whose pages render.ovl draws: a converted
		   copy's zoomed page too, since 1.10. That one ran on the
		   pictures' 12 KB and overran it: Erez's EDOS copy, a page
		   clicked in View > Pages, 4 bombs past the stack's top, 8 Nov) */
		long need = drawn || (e->obj >= PICS_PAGE) || doc_page_drawable() ? DRAW_STACK : PIC_STACK;

		if (pic_stack && pic_stack_size < need) {
			free(pic_stack);
			pic_stack = NULL;
			pic_started = 0;
		}
		if (!pic_stack) {
			if (!(pic_stack = malloc(need))) {
				e->state = PICS_FAILED;
				e->why = PIC_MEMORY;
				return 0;
			}
			pic_stack_size = need;
		}
	}
	if (!pic_started) {
		coro_start(&pic_co, pic_stack, pic_stack_size, decoder);
		pic_started = 1;
	}
	rpic_pal = pics_palette();
	if (!rpic_pal && planes <= 8) {
		e->state = PICS_FAILED;		/* no memory for the palette's table */
		e->why = PIC_MEMORY;
		return 0;
	}
	rpic = e;
	rpic_planes = planes;
	rpic_shown = 0;
	rpic_rows = 0;
	pdone = 0;
	pic_poll = pic_turn;
	pics_wait = band_wait;
	pics_page_rows = page_rows;
	return 1;
}

/* the picture on for ms milliseconds: 1 when it is done (the window's
   bitmap made, or failed), its object in *obj */
/* the background stacks' deepest use so far, in crash.log's "last status"
   (the hub, 8 Nov 2026: a crash near a stack's end shows itself there);
   asked after a picture on a 68020 or better (a scan of the stack's unused
   part, a few ms there), only when it is deeper than before */
static void stacks_note(void)
{
	static long rmax, pmax;
	long r = read_started ? coro_used(read_stack, READ_STACK) : 0;
	long p = pic_started ? coro_used(pic_stack, pic_stack_size) : 0;
	char a[80];

	if (r <= rmax && p <= pmax)
		return;
	if (r > rmax)
		rmax = r;
	if (p > pmax)
		pmax = p;
	snprintf(a, sizeof a, "stacks: reading %ld of %ld, pictures %ld of %ld", rmax, READ_STACK, pmax,
		 pic_stack_size);
	crash_note(a);
	log_event("stacks", a, NULL);
}

int doc_pic_slice(unsigned long ms, long *obj)
{
	if (!rpic)
		return 0;
	slice_end = tos_ms() + ms;
	running = &pic_co;
	coro_switch(&main_co, &pic_co);
	running = NULL;			/* back on the main stack: slice_up hands nothing back */
	pics_band();			/* a band of colour rows to the screen's format */
	if (!pdone)
		return 0;
	if ((rpic_rows ? !rpic_ok : !rpic_std) && rpic_why == PIC_MEMORY && retried_obj != rpic->obj && give_back("picture")) {
		retried_obj = rpic->obj;
		if (rpic_rows)
			pics_made(rpic, 0, PIC_STOPPED);	/* its rows so far let go */
		rpic->state = PICS_DECODING;
		rpic_shown = 0;
		pdone = 0;		/* the decoder starts it again in the next slice */
		return 0;
	}
	*obj = rpic->obj;
	if (rdraw_machine())
		stacks_note();
	if (rpic_rows)
		pics_made(rpic, rpic_ok, rpic_why);
	else
		pics_done(rpic, rpic_std, rpic_bytes, rpic_why);
	rpic = NULL;
	return 1;
}

/* the picture being made: rows made since the window was last told, from
   *from to *to (its object *obj); 0 none */
int doc_pic_rows(long *obj, long *from, long *to)
{
	if (!rpic || rpic->ready <= rpic_shown)
		return 0;
	*obj = rpic->obj;
	*from = rpic_shown;
	*to = rpic->ready;
	rpic_shown = rpic->ready;
	return 1;
}

int doc_pic_busy(void)
{
	return rpic != NULL;
}

/* the picture being made is one ahead of the view and one shown waits:
   the one ahead stopped, its slot let go (made again later, if room) */
void doc_pic_drop_ahead(void)
{
	/* one ahead of the view gives way to one it shows; one the window
	   shows but not on the page in focus gives way to that page's (Erez,
	   8 Nov 2026: the page in focus first) */
	if (!rpic || !((rpic->ahead && pics_shown_waiting()) ||
		       (!rpic->ahead && pics_focus >= 0 && rpic->page != pics_focus && pics_focus_busy())))
		return;
	abandon = 1;
	while (!pdone) {
		running = &pic_co;
		coro_switch(&main_co, &pic_co);
		running = NULL;
		pics_band();		/* (a band waiting is let go on) */
	}
	free(rpic_std);
	rpic_std = NULL;
	pics_release(rpic);
	rpic = NULL;
	abandon = 0;
	log_event("picture", "ahead-stopped", NULL);
}

/* the progress row's (Erez, 2 Nov 2026) */
long doc_reading(int *prog)
{
	if (!doc || done >= npages)
		return -1;
	*prog = rpage >= 0 && !rdone ? doc->page_prog : 0;
	if (*prog > 990)
		*prog = 990;			/* its content read, the page still being put together */
	return rpage >= 0 ? rpage : next;
}

/* page picture obj (PICS_PAGE + n) being drawn: how far, in thousandths,
   or -1 when it isn't the one being drawn */
int doc_pic_permille(long obj)
{
	long page, made, of;

	if (!rpic || rpic->obj != obj || !doc_pic_progress(&page, &made, &of) || of <= 0)
		return -1;
	return (int)(made * 1000 / of);
}

int doc_pic_progress(long *page, long *made, long *of)
{
	if (!rpic || rpic->ahead)
		return 0;		/* (one ahead of the view is made quietly) */
	*page = rpic->page;
	*made = rpic->made;
	*of = rpic->h;			/* (made whole, by the DSP or as a standard bitmap: 0 until done) */
	if (rpic->obj >= PICS_PAGE) {
		/* a page drawn: its content read, four fifths; its rows shown, the rest */
		const ROVL *o = rdraw_ovl();

		*page = rpic->obj - PICS_PAGE;
		*of = 1000;
		*made = draw_rows ? 800 + rpic->made * 200 / (rpic->h > 0 ? rpic->h : 1) :
			o && rdoc && o->size >= (short)sizeof(ROVL) ? o->progress(rdoc) * 8 / 10 : 0;
	}
	return 1;
}

#ifdef EPDF_STACKS
/* a test build's report: how much of each stack has been used */
static void stacks_used(void)
{
	char a[64];

	snprintf(a, sizeof a, "read %ld of %ld, pictures %ld of %ld", read_started ? coro_used(read_stack, READ_STACK) : 0L,
		 READ_STACK, pic_started ? coro_used(pic_stack, PIC_STACK) : 0L, PIC_STACK);
	log_event("stacks", a, NULL);
}
#endif

int doc_is_open(void)
{
	return doc != NULL;
}

/* the open document's path (the one being opened, or open) */
const char *doc_path(void)
{
	return opath;
}

const char *doc_name(void)
{
	return name;
}

void doc_set_original(const char *path)
{
	const char *base = strrchr(path, '\\');

	strncpy(orig, path, sizeof orig - 1);
	orig[sizeof orig - 1] = '\0';
	strncpy(name, base ? base + 1 : path, sizeof name - 1);
	name[sizeof name - 1] = '\0';
}

const char *doc_original(void)
{
	return orig[0] ? orig : opath;
}

long doc_pages(void)
{
	return npages;
}

long doc_read(void)
{
	return done;
}

DOCVIEW *doc_view(void)
{
	return doc ? &view : NULL;
}

/* an open under way given up: the reader stops at its next piece */
static void stop_opening(void)
{
	if (!opening)
		return;
	abandon = 1;
	while (!rdone) {
		running = &read_co;
		coro_switch(&main_co, &read_co);
		running = NULL;
	}
	abandon = 0;
	rdone = 0;
	opening = 0;
	pdf_close(odoc);
	free(odoc);
	odoc = NULL;
	name[0] = '\0';
}

int doc_opening(void)
{
	return opening;
}

void doc_close(void)
{
	stop_opening();
	if (!doc)
		return;
	stop_reading();
#ifdef EPDF_STACKS
	stacks_used();
#endif
	dv_free(&view);
	doc_tr_forget();
	ts_close(&store);
	if (pstore_open)
		ts_close(&pstore);
	pstore_open = pages_mode = pages_have = 0;
	if (rdoc)
		rdraw_ovl()->close(rdoc);
	rdoc = NULL;
	rdraw_unload();			/* (its 676 KB back for the next document's text) */
	drawn = 0;
	free(pdbl);
	pdbl = NULL;
	sharp = -1;
	pdf_close(doc);
	free(doc);
	doc = NULL;
	npages = done = next = 0;
	name[0] = '\0';
	heap_trim();			/* the document's blocks back to TOS */
	heap_grain = 64L * 1024;	/* small again with no document (epdf.c) */
}

int doc_open(const char *path, int cols, int lines_mode, const char *cache)
{
	const char *base;

	doc_close();
	{
		/* a document's heap as the libc's has always been: a first block
		   of a quarter of what is free (taken and given back to the heap
		   here), then doubling. 64 KB steps, and the libc's later blocks
		   alone, left a document's memory in pieces: memory refused on
		   ST-1 in up to six of the corpus's documents, none before */
		long q = ((long)Malloc(-1L) / 4) & ~1L;
		void *p;

		heap_grain = q;
		p = q > 1024 ? malloc((size_t)(q - 1024)) : NULL;
		free(p);
		heap_grain = 0;
	}
	if ((!read_stack && !(read_stack = malloc(READ_STACK))) || (!rt && !(rt = calloc(1, sizeof *rt))) ||
	    !(odoc = malloc(sizeof *odoc)))
		return -1;
	if (!read_started) {
		coro_start(&read_co, read_stack, READ_STACK, reader);
		read_started = 1;
	}
	strncpy(opath, path, sizeof opath - 1);
	opath[sizeof opath - 1] = '\0';
	orig[0] = '\0';
	strncpy(ocache, cache ? cache : "", sizeof ocache - 1);
	ocache[sizeof ocache - 1] = '\0';
	ocols = cols;
	olines = lines_mode;
	base = strrchr(path, '\\');
	strncpy(name, base ? base + 1 : path, sizeof name - 1);
	name[sizeof name - 1] = '\0';
	opening = 1;
	rdone = 0;
	log_event("opening", name, NULL);
	return 0;
}

/* the reader has opened odoc (or not): the store and the view made, and
   it is the document */
static long open_done(void)
{
	long free_mem, budget;

	opening = 0;
	rdone = 0;
	if (!open_ok) {
		log_event("open-failed", opath, odoc->err);
		pdf_close(odoc);
		free(odoc);
		odoc = NULL;
		name[0] = '\0';
		return DOC_FAILED;
	}
	/* a quarter of the memory free keeps text, at most 256 KB; the rest
	   goes to the cache file */
	free_mem = (long)Malloc(-1L);
	budget = free_mem / 4;
	if (budget > 256L * 1024)
		budget = 256L * 1024;
	if (budget < 16L * 1024)
		budget = 16L * 1024;
	if (ts_open(&store, onpages, budget, ocache) != 0 || dv_init(&view, &store, onpages, ocols, olines) != 0) {
		ts_close(&store);
		pdf_close(odoc);
		free(odoc);
		odoc = NULL;
		name[0] = '\0';
		return DOC_FAILED;
	}
	doc = odoc;
	odoc = NULL;
	npages = onpages;
	/* any other PDF drawn as printed where render.ovl can be (stage 2) */
	drawn = !doc->prepared && rdraw_machine() && pf_size(rdraw_path) > 0;
	if (!doc->prepared && rdraw_machine())
		log_event("pages-drawn", drawn ? "1" : "0", rdraw_path);
	pstore_open = (doc->prepared || drawn) && ts_open(&pstore, onpages, 32L * 1024, "") == 0;
	cache_kb = budget / 1024;
	retried_page = retried_obj = -1;
	next = done = 0;
	{
		char n[16];

		snprintf(n, sizeof n, "%ld", npages);
		log_event("opened", name, n);
	}
	return DOC_OPENED;
}

int doc_pending(void)
{
	return opening || (doc && (done < npages || tr_reread > 0));
}

void doc_want(long page)
{
	if (page >= 0 && page < npages)
		next = page;
}

/* the reading on for ms milliseconds: the page it finished then, or -1
   (it goes on in the next slice) */
long doc_slice(unsigned long ms)
{
	long i, n = -1, len = 0;
	char *a;
	int rt_failed, was_read;

	if (!doc_pending())
		return -1;
	if (opening) {
		slice_end = tos_ms() + ms;
		running = &read_co;
		coro_switch(&main_co, &read_co);
		running = NULL;
		return rdone ? open_done() : -1;
	}
	if (rpage < 0) {
		/* the next page not yet read, from `next` on and round */
		for (i = 0; i < npages; i++) {
			n = (next + i) % npages;
			if (!ts_have(&store, n) || (tr_reread > 0 && trst[n] == TR_NONE))
				break;
		}
		if (i == npages) {
			tr_reread = 0;
			return -1;
		}
		rpage = n;
		rdone = 0;
	}
	slice_end = tos_ms() + ms;
	running = &read_co;
	coro_switch(&main_co, &read_co);
	running = NULL;
	if (!rdone)
		return -1;
	n = rpage;
	if (rt->failed && doc->st.oom != rpage_oom && retried_page != n && give_back("page")) {
		retried_page = n;
		ptext_free(rt);
		rdone = 0;		/* the reader reads it again in the next slice */
		return -1;
	}
	rpage = -1;
	rt_failed = rt->failed;
	if (doc->prepared) {
		/* (in the Atari's set already, soft breaks as CR) */
		len = rt->text.len;
		a = len > 0 ? malloc((size_t)len) : NULL;
		if (a)
			memcpy(a, rt->text.s, (size_t)len);
		else
			len = 0;
		pages_note(n, &rprep);
	} else {
		if (drawn)
			pages_drawn_note(n);
		/* the soft break as CR in the store: the conversion keeps CR, LF and Tab only */
		for (i = 0; i < rt->text.len; i++)
			if (rt->text.s[i] == '\v')
				rt->text.s[i] = '\r';
		a = rt->text.s && rt->text.len > 0 ? cs_to_atari(rt->text.s, rt->text.len, CS_UTF8, &len) : NULL;
	}
	was_read = ts_have(&store, n);	/* (read again for its /Translation: counted once) */
	if (ts_put(&store, n, a ? a : "", a ? len : 0) != 0) {
		/* no room for it: shown as empty, not read again */
		ts_put(&store, n, "", 0);
		log_event("page-lost", NULL, NULL);
	} else
		ts_set_pics(&store, n, doc->prepared ? a && tv_find(a, len, 0x7F) >= 0 : rt->piclines > 0);
	free(a);
	if (tr_from_file()) {
		/* a page the Gateway translated with the conversion: kept as it came */
		if (rprep.same)
			doc_tr_put(n, TR_SAME, NULL, 0, "");
		else if (rtrans_ok && rtrans.len > 0)
			doc_tr_put(n, TR_DONE, rtrans.s, rtrans.len, rprep.from);
		else
			doc_tr_put(n, TR_NOTHING, NULL, 0, "");
		if (tr_reread > 0)
			tr_reread--;
		rtrans_ok = 0;
		sb_free(&rtrans);
	}
	ptext_free(rt);
	if (!was_read)
		done++;
#ifdef EPDF_STACKS
	if (done % 25 == 0)
		stacks_used();
#endif
	next = n + 1 < npages ? n + 1 : 0;
	dv_page_ready(&view, n);
	{
		/* "page N LEN": the rigs count them; the reader's peak for the
		   page, the heap's size and the allocations refused (oom) with it */
		char p[16], l[96];

		snprintf(p, sizeof p, "%ld", n + 1);
		snprintf(l, sizeof l, "%ld peak %ld heap %ld oom %ld%s", len, doc->st.mem_peak / 1024,
			 (heap_st + heap_tt) / 1024, doc->st.oom - rpage_oom, rt_failed ? " failed" : "");
		log_event("page", p, l);
	}
	return n;
}

/* page n's text as it is stored (soft breaks as CR), or NULL when it
   isn't read yet; valid until the store is next used */
const char *doc_page_text(long n, long *len)
{
	return doc ? ts_get(&store, n, len) : NULL;
}

/* page n's text's length as stored, without reading it; -1 when not read */
long doc_page_len(long n)
{
	TSTORE *t = view.ts ? view.ts : &store;
	long len;

	if (!doc || !ts_have(t, n))
		return -1;
	if (t != &store && t->pg && t->pg[n].len >= 0)
		return t->pg[n].len;
	(void)len;
	return store.pg[n].len;
}

const char *doc_status(void)
{
	static char s[96];

	if (!doc)
		return "No document";
	if (done < npages)
		snprintf(s, sizeof s, "%s  \xFA  %ld of %ld pages read", name, done, npages);
	else
		snprintf(s, sizeof s, "%s  \xFA  %ld page%s", name, npages, npages == 1 ? "" : "s");
	return s;
}

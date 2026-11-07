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
#include "docview.h"
#include "epdf.h"
#include "coro.h"
#include "pdfimg.h"
#include "pics.h"

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
static CORO main_co, read_co, pic_co;
static CORO *running;			/* the stack working now */
static char *read_stack, *pic_stack;
static int read_started, pic_started;
static long rpage = -1;			/* the page being read, -1 none */
static int rdone;			/* the reader has finished rpage */
static int abandon;			/* the document closes: stop the page and the picture */
/* a document being opened, on the reader's stack (pdf_open_hook with the
   slices' hook): doc stays NULL until it is done */
static PDOC *odoc;
static char opath[256], ocache[256];
static int opening, open_ok, ocols, olines;
static long onpages;
static unsigned long slice_end;		/* tos_ms() when the slice is up */
static PTEXT *rt;			/* the page read (taken with the document) */
static PPREP rprep;			/* a converted PDF's page read: its whole pictures */
/* a converted PDF's pages whole (View > Pages): each page a picture's line,
   its own picture (16 colours where the screen has them, else mono) */
static TSTORE pstore;
static int pstore_open, pages_mode, pages_have;

/* each page's picture View > Pages shows and its double (EConvert's for
   a machine with the room: shown whole when the page is clicked), its
   size: kept by page, read on the main stack (doc_double_of) */
static struct {
	long obj, dbl, w, h;
} *pdbl;

static void pages_note(long n, const PPREP *pp)
{
	char b[48];
	int col = pp->colour >= 0 && planes >= 4;
	long obj = col ? pp->colour : pp->mono, w = col ? pp->colour_w : pp->mono_w, h = col ? pp->colour_h : pp->mono_h;
	int k = 0;

	if (!pstore_open)
		return;
	if (w <= 0 || h <= 0) {
		w = 640;		/* (a file of 1.1's) */
		h = pp->w > 0 ? 640L * pp->h / pp->w : 828L;
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
	}
	ts_put(&pstore, n, b, k);		/* (none: the page's head alone) */
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

int doc_has_pages(void)
{
	return doc && doc->prepared && pages_have;
}

int doc_pages_mode(void)
{
	return doc && pages_mode;
}

int doc_prepared(void)
{
	return doc && doc->prepared;
}

/* the view on the pages' text or on their pictures: laid out again from
   the page at the top (the caller lays the window out and draws it) */
void doc_set_pages(int on)
{
	long top;
	int cols, lm;

	if (!doc || !pstore_open || !on == !pages_mode)
		return;
	top = dv_page_at_top(&view);
	cols = view.cols;
	lm = view.lines_mode;
	dv_free(&view);
	pages_mode = on;
	if (dv_init(&view, on ? &pstore : &store, npages, cols, lm) != 0) {
		pages_mode = 0;
		dv_init(&view, &store, npages, cols, lm);
	}
	dv_goto_page(&view, top);
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
				POBJ *o = pdf_get(doc, rpic->obj);

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
	if (!pic_stack && !(pic_stack = malloc(PIC_STACK))) {
		e->state = PICS_FAILED;
		e->why = PIC_MEMORY;
		return 0;
	}
	if (!pic_started) {
		coro_start(&pic_co, pic_stack, PIC_STACK, decoder);
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
	return 1;
}

/* the picture on for ms milliseconds: 1 when it is done (the window's
   bitmap made, or failed), its object in *obj */
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
	if (!rpic || !rpic->ahead || !pics_shown_waiting())
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

int doc_pic_progress(long *page, long *made, long *of)
{
	if (!rpic || rpic->ahead)
		return 0;		/* (one ahead of the view is made quietly) */
	*page = rpic->page;
	*made = rpic->made;
	*of = rpic->h;			/* (made whole, by the DSP or as a standard bitmap: 0 until done) */
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
	ts_close(&store);
	if (pstore_open)
		ts_close(&pstore);
	pstore_open = pages_mode = pages_have = 0;
	free(pdbl);
	pdbl = NULL;
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
	pstore_open = doc->prepared && ts_open(&pstore, onpages, 32L * 1024, "") == 0;
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
	return opening || (doc && done < npages);
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
	int rt_failed;

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
			if (!ts_have(&store, n))
				break;
		}
		if (i == npages)
			return -1;
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
		/* the soft break as CR in the store: the conversion keeps CR, LF and Tab only */
		for (i = 0; i < rt->text.len; i++)
			if (rt->text.s[i] == '\v')
				rt->text.s[i] = '\r';
		a = rt->text.s && rt->text.len > 0 ? cs_to_atari(rt->text.s, rt->text.len, CS_UTF8, &len) : NULL;
	}
	if (ts_put(&store, n, a ? a : "", a ? len : 0) != 0) {
		/* no room for it: shown as empty, not read again */
		ts_put(&store, n, "", 0);
		log_event("page-lost", NULL, NULL);
	} else
		ts_set_pics(&store, n, doc->prepared ? a && tv_find(a, len, 0x7F) >= 0 : rt->piclines > 0);
	free(a);
	ptext_free(rt);
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
	return doc && ts_have(&store, n) ? store.pg[n].len : -1;
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

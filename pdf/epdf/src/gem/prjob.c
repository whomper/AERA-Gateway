/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * prjob.c - File > Print... (^P): the document through the GDOS (NVDI)
 * to any printer it drives (decided 21 Nov 2026; common's ui/print.c).
 *
 * On a Falcon or TT each page is drawn by render.ovl at the printer's
 * width (a converted copy's from its original where EPDF knows it), in
 * bands that fit memory, dithered to the printer's pens and copied to its
 * workstation: one plane as it is (vrt_cpyfm), more a pen at a time (a
 * mask of the band's pixels in that pen, vrt_cpyfm transparent in it: no
 * guess at the driver's own plane order). On an ST, or without render.ovl,
 * the page's text as the text view has it, set with v_gtext in the
 * printer's system font, text only (decided 21 Nov 2026).
 *
 * The job runs on a stack of its own, a band (or some lines) a turn of
 * the event loop; render.ovl's turns are the job's while it prints, and
 * the pictures wait (doc_pics_pause). Esc stops it between bands; the
 * printer's job is closed at once. Nothing on its stack calls the AES.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "gemui.h"
#include "plat.h"
#include "coro.h"
#include "print.h"
#include "rdraw.h"
#include "epdf.h"
#include "prjob.h"
#include "alert.h"

const char *prjob_device;
long prjob_first, prjob_last;

#define PR_STACK 65536L		/* render.ovl's and FreeType's, as the pictures' DRAW_STACK */
#define PR_ROWS 32		/* the rows dithered and copied at a time */

static PRINTJOB pj;
static int on, stop_req, finished, failed;
static int drawn_mode;
/* a short machine (under 2 MB free once the printer is open): render.ovl's
   document closed after every page, its fonts, caches and canvas let go,
   so the next page has all the room (decided 21 Nov 2026); with room it stays open (no fonts read again a page) */
static int page_alone;
/* a picture left out for memory on a page printed (the end's words say so:
   decided 21 Nov 2026) */
static int left_out;
static CORO pr_co, pr_main;
static char *stack;
static unsigned long slice_end;
static long k_done, k_total, cur_page;
static unsigned long t_page;
static int page_permille;
static void *rdoc;
static char why[120];
/* what went to the printer on the page (the log's "print-bands": decided 22
   Nov 2026, EPDF 2.43 printed nothing on his printer where EMail does) */
static long k_bands, k_ink;

static int pr_turn(void)
{
	if (tos_ms() >= slice_end)
		coro_switch(&pr_co, &pr_main);
	return stop_req;
}

/* ---- a page drawn by render.ovl ---- */

/* the printer's pens 0 .. n - 1 as render.ovl's palette (0..1000 to 0..255) */
static int printer_pens(unsigned char (*pal)[3])
{
	int n = pj.planes >= 4 ? 16 : pj.planes == 3 ? 8 : pj.planes == 2 ? 4 : 2, p, c;
	short rgb[3];

	if (n == 2) {
		pal[0][0] = pal[0][1] = pal[0][2] = 255;
		pal[1][0] = pal[1][1] = pal[1][2] = 0;
		return 2;
	}
	for (p = 0; p < n; p++) {
		vq_color(pj.h, (short)p, rgb);
		for (c = 0; c < 3; c++)
			pal[p][c] = (unsigned char)((rgb[c] < 0 ? 0 : rgb[c] > 1000 ? 1000 : rgb[c]) * 255L / 1000);
	}
	return n;
}

static int printer_npens(void)
{
	return pj.planes >= 4 ? 16 : pj.planes == 3 ? 8 : pj.planes == 2 ? 4 : 2;
}

/* rows y .. y + rows - 1 of the band (dithered into buf: each row its
   planes' words, pen bit p in plane p) copied to the printer at (x, top) */
static void put_rows(int w, int x, long top, int rows, int nplanes, int npens, const unsigned short *buf,
		     unsigned short *mask)
{
	int wdw = (w + 15) / 16;
	MFDB src, dst;
	short pxy[8], col[2];

	memset(&dst, 0, sizeof dst);
	memset(&src, 0, sizeof src);
	src.fd_w = (short)(wdw * 16);
	src.fd_h = (short)rows;
	src.fd_wdwidth = (short)wdw;
	src.fd_stand = 0;
	src.fd_nplanes = 1;
	pxy[0] = 0;
	pxy[1] = 0;
	pxy[2] = (short)(w - 1);
	pxy[3] = (short)(rows - 1);
	pxy[4] = (short)x;
	pxy[5] = (short)top;
	pxy[6] = (short)(x + w - 1);
	pxy[7] = (short)(top + rows - 1);
	if (npens == 2) {
		src.fd_addr = (void *)buf;	/* (one plane: its rows one after the other) */
		col[0] = 1;			/* black where a bit is set */
		col[1] = 0;
		vrt_cpyfm(pj.h, 1, pxy, &src, &dst, col);	/* MD_REPLACE */
		k_bands++;
		{
			long i, nw = (long)wdw * rows;

			for (i = 0; i < nw; i++)
				k_ink += buf[i] != 0;	/* (words with ink) */
		}
		return;
	}
	/* a pen at a time: its pixels' mask, put down transparent in it */
	{
		int p, r, i, pl;

		src.fd_addr = mask;
		for (p = 1; p < npens; p++) {
			int any = 0;

			for (r = 0; r < rows; r++) {
				const unsigned short *row = buf + (long)r * nplanes * wdw;
				unsigned short *m = mask + (long)r * wdw;

				for (i = 0; i < wdw; i++) {
					unsigned short b = 0xFFFF;

					for (pl = 0; pl < nplanes; pl++) {
						unsigned short v = row[pl * wdw + i];

						b &= (p >> pl) & 1 ? v : (unsigned short)~v;
					}
					m[i] = b;
					any |= b != 0;
				}
			}
			if (!any)
				continue;
			col[0] = (short)p;
			col[1] = 0;
			vrt_cpyfm(pj.h, 2, pxy, &src, &dst, col);	/* MD_TRANS */
			k_bands++;
			k_ink++;		/* (a pen put down) */
		}
	}
}

/* a black-and-white printer's rows: the band's 5-6-5 pixels to grey and
   error diffusion into one plane here (render.ovl's pens take 128 KB of
   tables, which a 4 MB Falcon hasn't left once the printer's driver holds
   its page: about 1.1 MB at 300 dpi), err a row of w + 2 shorts kept from
   band to band, the serpentine's direction by the page's row */
static unsigned char gr[32], gg[64], gb[32];	/* each channel's share of the grey, 0..255 in all */

static void mono_rows(const unsigned short *px, int w, int rows, long row0, short *err, unsigned short *out)
{
	int wdw = (w + 15) / 16, r, x;

	if (!gg[63]) {
		for (x = 0; x < 32; x++) {
			gr[x] = (unsigned char)(x * 77 * 255 / 31 >> 8);
			gb[x] = (unsigned char)(x * 28 * 255 / 31 >> 8);
		}
		for (x = 0; x < 64; x++)
			gg[x] = (unsigned char)(x * 151 * 255 / 63 >> 8);
	}

	for (r = 0; r < rows; r++) {
		const unsigned short *p = px + (long)r * w;
		unsigned short *o = out + (long)r * wdw;
		short *e = err + 1;
		int carry = 0, back = (int)((row0 + r) & 1);

		memset(o, 0, (size_t)wdw * 2);
		for (x = 0; x < w; x++) {
			int i = back ? w - 1 - x : x, v, g;
			unsigned short c = p[i];

			/* 0..255 grey: (r * 77 + g * 151 + b * 28) / 256 of the 5-6-5's */
			g = gr[c >> 11] + gg[(c >> 5) & 63] + gb[c & 31];
			v = g + carry + e[i];
			e[i] = 0;
			if (v < 128) {
				o[i >> 4] |= (unsigned short)(0x8000 >> (i & 15));
				carry = v;
			} else
				carry = v - 255;
			/* 7/16 on along the row, 3/16, 5/16, 1/16 to the next row's */
			{
				int d = back ? -1 : 1;

				e[i - d] += (short)(carry * 3 / 16);
				e[i] += (short)(carry * 5 / 16);
				e[i + d] += (short)(carry / 16);
				carry = carry * 7 / 16;
			}
		}
	}
}

/* a black-and-white row rw pixels wide made sc times as wide (each bit sc
   bits) into dst */
static void widen(const unsigned short *src, int rw, int sc, unsigned short *dst, int w)
{
	int x, wdw = (w + 15) / 16;

	memset(dst, 0, (size_t)wdw * 2);
	for (x = 0; x < rw; x++)
		if (src[x >> 4] & (0x8000 >> (x & 15))) {
			int k, X = x * sc;

			for (k = 0; k < sc && X + k < w; k++)
				dst[(X + k) >> 4] |= (unsigned short)(0x8000 >> ((X + k) & 15));
		}
}

/* page n (from 0): 0 printed, -1 failed (why), -2 stopped. Drawn at the
   paper's width where render.ovl has the memory for it; where it hasn't
   (a 4 MB Falcon once the driver holds its page: about 1.1 MB at 300
   dpi) at half or a quarter of it, each pixel then printed 2 x 2 or 4 x 4
   (black and white: a colour printer's page is drawn at its width or not) */
static int page_drawn_at(long n, int sc0);

static int page_drawn(long n)
{
	int sc, r = -1;

	/* a band refused memory part way down: the page again at the next
	   size down (its rows put down again over what was printed of it) */
	for (sc = 1; sc <= 4; sc *= 2) {
		why[0] = '\0';
		r = page_drawn_at(n, sc);
		if (r == -3 && sc == 4)
			r = page_drawn_at(n, -4);	/* (the smallest: printed with what it could draw) */
		if (r == -3 && printer_npens() > 2)
			r = page_drawn_at(n, -1);	/* (a colour page is drawn at its width or not) */
		if ((r != -1 && r != -3) || printer_npens() > 2)
			break;
		if (rdoc) {
			/* (its canvas and caches as the wider page left them let go:
			   opened again for the narrower one) */
			rdraw_ovl()->close(rdoc);
			rdoc = NULL;
		}
	}
	return r;
}

/* sc0 the size (1, 2, 4: the paper's width over it); negative, that size
   with pictures it had no memory for left out. -3: a picture was refused
   memory (the page to be tried smaller) */
static int page_drawn_at(long n, int sc0)
{
	int keep_missing = sc0 < 0;
	const ROVL *o;
	ROVL_PAGE pg;
	unsigned char pal[16][3];
	unsigned short *buf = NULL, *mask = NULL, *one = NULL;
	short *err = NULL;
	int npens, nplanes, w, rw, sc, x, rc = -1, y0 = 0, chunk;
	long done_rows = 0;

	if (!rdraw_load(pr_turn, why, sizeof why))
		return -1;
	o = rdraw_ovl();
	if (!rdoc) {
		long fr = (long)Malloc(-1L);

		rdoc = o->open(doc_original(), doc_font_dir, fr / 8, fr / 4, why, sizeof why);
		{
			char a[48];

			snprintf(a, sizeof a, "free %ld ok %d", fr, rdoc != NULL);
			log_event("print-ovl-open", a, rdoc ? NULL : why);
		}
		if (!rdoc)
			return stop_req ? -2 : -1;
	}
	npens = printer_pens(pal);
	nplanes = npens == 16 ? 4 : npens == 8 ? 3 : npens == 4 ? 2 : 1;
	if (sc0 < 0)
		sc0 = -sc0;
	for (sc = sc0; sc <= sc0; sc *= 2) {
		rw = pj.w / sc;
		rc = o->page(rdoc, n, rw, 0, &pg);
		if (rc == 0 && (long)pg.h * sc > pj.ht) {
			/* a page taller than the paper's area for its width: narrower */
			rw = (int)((long)rw * pj.ht / ((long)pg.h * sc));
			rc = o->page(rdoc, n, rw, 0, &pg);
		}
		{
			char a[64];

			snprintf(a, sizeof a, "%ld w %d rc %d free %ld heap %ld missed %ld", n + 1, rw, rc, (long)Malloc(-1L),
				 heap_room(), rc ? 0L : pg.images_missed);
			log_event("print-draw", a, NULL);
		}
		if (rc != -1)
			break;
	}
	if (rc)
		goto out;
	if (pg.images_missed > 0 && !keep_missing) {
		rc = -3;
		goto out;
	}
	if (pg.images_missed > 0)
		left_out = 1;
	if (sc > 1) {
		char a[40];

		snprintf(a, sizeof a, "%ld %d", n + 1, sc);
		log_event("print-scaled", a, NULL);
	}
	w = rw * sc;
	x = (pj.w - w) / 2;
	chunk = PR_ROWS / sc;
	buf = malloc((size_t)((w + 15) / 16) * 2 * nplanes * PR_ROWS);
	mask = npens > 2 ? malloc((size_t)((w + 15) / 16) * 2 * PR_ROWS) : NULL;
	err = npens == 2 ? calloc((size_t)rw + 2, sizeof(short)) : NULL;
	one = sc > 1 ? malloc((size_t)((rw + 15) / 16) * 2 * chunk) : NULL;
	if (!buf || (npens > 2 && !mask) || (npens == 2 && !err) || (sc > 1 && !one)) {
		snprintf(why, sizeof why, "no memory for a band");
		rc = -1;
		goto out;
	}
	for (;;) {
		int y;

		for (y = 0; y < pg.rows; y += chunk) {
			int rows = pg.rows - y < chunk ? pg.rows - y : chunk;

			if (npens == 2 && sc > 1) {
				int r, k, rwd = (rw + 15) / 16, wd = (w + 15) / 16;

				mono_rows(pg.px + (long)y * pg.w, rw, rows, (long)pg.y0 + y, err, one);
				for (r = 0; r < rows; r++)
					for (k = 0; k < sc; k++)
						widen(one + (long)r * rwd, rw, sc, buf + ((long)r * sc + k) * wd, w);
			} else if (npens == 2)
				mono_rows(pg.px + (long)y * pg.w, rw, rows, (long)pg.y0 + y, err, buf);
			else if (o->pens(rdoc, (const unsigned char (*)[3])pal, npens, nplanes, y, rows, (unsigned char *)buf)) {
				snprintf(why, sizeof why, "the page's rows couldn't be dithered");
				rc = -1;
				goto out;
			}
			put_rows(w, x, ((long)pg.y0 + y) * sc, rows * sc, nplanes, npens, buf, mask);
			done_rows += rows;
			page_permille = (int)(done_rows * 1000 / (pg.h > 0 ? pg.h : 1));
			if (pr_turn()) {
				rc = -2;
				goto out;
			}
		}
		y0 = pg.y0 + pg.rows;
		if (y0 >= pg.h)
			break;
		rc = o->page(rdoc, n, rw, y0, &pg);
		if (rc)
			goto out;
		if (pg.images_missed > 0 && !keep_missing) {
			rc = -3;
			goto out;
		}
		if (pg.images_missed > 0)
			left_out = 1;
	}
out:
	free(buf);
	free(mask);
	free(err);
	free(one);
	if (rc == -1 && !why[0])
		snprintf(why, sizeof why, "page %ld couldn't be drawn", n + 1);
	if (rc == -3) {
		char a[32];

		snprintf(a, sizeof a, "%ld %d", n + 1, sc0);
		log_event("print-picture-refused", a, NULL);
		return stop_req ? -2 : -3;
	}
	return rc == -2 || stop_req ? -2 : rc ? -1 : 0;
}

/* ---- a page's text (an ST, or no render.ovl) ---- */

static short pr_cw, pr_ch, cols, lines, line;

static void text_setup(void)
{
	short d;

	vst_font(pj.h, 1);
	vst_point(pj.h, 10, &d, &d);
	vst_alignment(pj.h, 0, 5);		/* (the cell's top) */
	vswr_mode(pj.h, 2);
	vst_color(pj.h, 1);
	pr_cw = vqt_width(pj.h, 'M');
	{
		short a[10];

		vqt_attributes(pj.h, a);
		pr_ch = a[9] > 0 ? a[9] : 16;	/* (the cell's height) */
	}
	if (pr_cw <= 0)
		pr_cw = 8;
	cols = (short)(pj.w / pr_cw);
	lines = (short)(pj.ht / pr_ch);
	if (cols < 10)
		cols = 10;
	if (lines < 4)
		lines = 4;
}

/* a line out; a new sheet when the page's area is full (0, or -2 stopped) */
static int text_line(const char *s, int n)
{
	if (line >= lines) {
		print_page(&pj, 0);
		line = 0;
	}
	if (n > 0)
		v_gtext_n(pj.h, 0, (short)(line * pr_ch), s, (short)n);
	line++;
	return (line % 16) == 0 && pr_turn() ? -2 : 0;
}

/* page n's text: each paragraph wrapped to the paper's width (the soft
   break, CR, a space; a picture's line left out) */
static int page_text(long n)
{
	const char *t;
	long len, i = 0;
	char out[256];

	while (!(t = doc_page_text(n, &len))) {
		if (doc_read() > n)
			return 0;		/* (read, and nothing kept for it: an empty sheet) */
		if (pr_turn())
			return -2;
	}
	if (cols > (short)sizeof out)
		cols = (short)sizeof out;
	line = 0;
	while (i < len) {
		long e = i, o = 0, last_sp = -1;

		while (e < len && t[e] != '\n')
			e++;
		if (t[i] == 0x7F) {		/* (a picture's line) */
			i = e + 1;
			continue;
		}
		for (; i < e; i++) {
			char c = t[i] == '\r' || t[i] == '\t' ? ' ' : t[i];

			if (o >= cols) {
				long cut = last_sp > 0 ? last_sp : o;

				if (text_line(out, (int)cut))
					return -2;
				o = o > cut ? o - cut - (last_sp > 0 ? 1 : 0) : 0;
				memmove(out, out + (o ? cut + (last_sp > 0 ? 1 : 0) : 0), (size_t)o);
				last_sp = -1;
			}
			if (c == ' ')
				last_sp = o;
			out[o++] = c;
		}
		if (text_line(out, (int)o))
			return -2;
		i = e + 1;
		page_permille = (int)(i * 1000 / (len > 0 ? len : 1));
	}
	return 0;
}

/* ---- the job ---- */

static void job(void)
{
	int c;
	long n;

	if (!drawn_mode)
		text_setup();
	for (c = 0; c < pj.copies && !stop_req && !failed; c++)
		for (n = pj.first; n <= pj.last && !stop_req && !failed; n++) {
			int r;

			cur_page = n;
			page_permille = 0;
			t_page = tos_ms();
			k_bands = k_ink = 0;
			r = drawn_mode ? page_drawn(n - 1) : page_text(n - 1);
			if (r == -1) {
				failed = 1;
				break;
			}
			if (r == -2)
				break;
			if (page_alone && rdoc) {
				rdraw_ovl()->close(rdoc);
				rdoc = NULL;
			}
			print_page(&pj, c == pj.copies - 1 && n == pj.last);
			k_done++;
			{
				char a[24];

				char b[24];

				snprintf(a, sizeof a, "%ld", n);
				snprintf(b, sizeof b, "%lu ms", tos_ms() - t_page);
				log_event("printed-page", a, b);
				if (drawn_mode) {
					char pb[64];

					snprintf(pb, sizeof pb, "%ld copies %ld ink %ld%s", n, k_bands, k_ink,
						 c == pj.copies - 1 && n == pj.last ? " last" : "");
					log_event("print-bands", pb, NULL);
				}
			}
			pr_turn();
		}
	finished = 1;
	for (;;)
		coro_switch(&pr_co, &pr_main);
}

int prjob_on(void)
{
	return on;
}

int prjob_begin(void)
{
	int r;

	if (on || !doc_is_open())
		return 0;
	r = print_setup(&pj, (short)(doc_pages() > 32767 ? 32767 : doc_pages()));
	log_event("print-setup", r == 1 ? "wdialog" : r == 2 ? "default" : "no-gdos", NULL);
	if (r <= 0) {
		alert(1, "[1][Printing needs NVDI (or another GDOS)|with a printer driver set up.][ OK ]");
		return 0;
	}
	{
		/* the rigs: NVDI's picture driver to a file (PRINTTO= in the INF,
		   "91 C:\\PAGE.IMG"): Hatari opens no real printer's driver */
		const char *to = prjob_device;

		if (to && to[0]) {
			short drv = (short)atoi(to);
			const char *dev = strchr(to, ' ');

			print_device(&pj, drv, dev ? dev + 1 : "");
		}
	}
	/* (the rigs, with their own device and range: no dialog to answer, as
	   common's tests/print) */
	if (!(prjob_device && prjob_device[0]) && print_dialog(&pj, doc_name()) != 1) {
		print_end(&pj);
		log_event("print-dialog", "cancel", NULL);
		return 0;
	}
	if (prjob_first > 0 && prjob_first <= pj.pages) {
		/* (the rigs' range, as the dialog would give it: PRINTRANGE=) */
		pj.first = (short)prjob_first;
		pj.last = (short)(prjob_last >= prjob_first && prjob_last <= pj.pages ? prjob_last : prjob_first);
	}
	drawn_mode = rdraw_machine() && pf_size(rdraw_path) > 0;
	if (drawn_mode) {
		/* the room given back before the driver opens: a printer's driver
		   takes its page's buffer at v_opnwk (a 4 MB Falcon in 256 colours) */
		doc_pics_pause(1);	/* (render.ovl is the job's while it prints) */
		doc_print_room();
	}
	if (!print_open(&pj)) {
		char a[40];

		print_end(&pj);
		if (drawn_mode)
			doc_pics_pause(0);
		snprintf(a, sizeof a, "failed free %ld", (long)Malloc(-1L));
		log_event("print-open", a, NULL);
		alert(1, "[1][The printer can't be opened.|Check NVDI's printer driver|and that the printer is on.][ OK ]");
		return 0;
	}
	/* a device with no paper (NVDI's metafile: 0..32767 at 85 um, 2.8 m
	   across) set at an A4 sheet; a printer's area is its paper's, smaller */
	if (pj.um_w > 0 && pj.w > 210000L / pj.um_w)
		pj.w = (short)(210000L / pj.um_w);
	if (pj.um_h > 0 && pj.ht > 297000L / pj.um_h)
		pj.ht = (short)(297000L / pj.um_h);
	if (!stack && !(stack = malloc(PR_STACK))) {
		print_end(&pj);
		if (drawn_mode)
			doc_pics_pause(0);
		return 0;
	}
	{
		char a[80];

		char d[48];

		snprintf(a, sizeof a, "%d %d um %d %d planes %d pages %d-%d copies %d %s", pj.w, pj.ht, pj.um_w, pj.um_h,
			 pj.planes, pj.first, pj.last, pj.copies, drawn_mode ? "drawn" : "text");
		/* WDIALOG's choice (its PRN_SETTINGS: the driver's id at 32, the
		   device at 128), or the default printer, device 21 */
		if (pj.set)
			snprintf(d, sizeof d, "driver %d %.32s", *(short *)((char *)pj.set + 32), (char *)pj.set + 128);
		else
			snprintf(d, sizeof d, "driver 21 (the default)");
		log_event("print-open", a, d);
	}
	if (drawn_mode)
		page_alone = (long)Malloc(-1L) < 2L * 1024 * 1024;
	coro_start_long(&pr_co, stack, PR_STACK, job);
	on = 1;
	stop_req = finished = failed = left_out = 0;
	why[0] = '\0';
	k_done = 0;
	k_total = (long)(pj.last - pj.first + 1) * pj.copies;
	cur_page = pj.first;
	page_permille = 0;
	win_status(NULL);
	return 1;
}

void prjob_stop(void)
{
	if (on)
		stop_req = 1;
}

/* the job on for up to ms; at its end the printer's job closed and the
   status bar told */
void prjob_slice(unsigned long ms)
{
	if (!on)
		return;
	slice_end = tos_ms() + ms;
	coro_switch(&pr_main, &pr_co);
	if (!finished)
		return;
	print_end(&pj);
	if (rdoc) {
		rdraw_ovl()->close(rdoc);
		rdoc = NULL;
	}
	free(stack);
	stack = NULL;
	if (drawn_mode)
		doc_pics_pause(0);
	on = 0;
	{
		char a[24];

		snprintf(a, sizeof a, "%ld of %ld", k_done, k_total);
		log_event(failed ? "print-failed" : stop_req ? "print-stopped" : "print-done", a, failed ? why : NULL);
	}
	{
		const char *say = stop_req || failed ? "Printing stopped." :
				  left_out ? "Printed without some pictures: not enough memory." : "Printed.";

		win_status(say);
		log_event("print-said", say, NULL);
	}
}

/* the status bar's words and its bar: "Printing page 3 of 12" */
int prjob_progress(char *w, int n, long *done, long *total)
{
	long k;

	if (!on)
		return 0;
	k = k_done + 1 > k_total ? k_total : k_done + 1;
	snprintf(w, (size_t)n, "Printing page %ld of %ld", k, k_total);
	*done = k_done * 1000L + page_permille;
	*total = k_total * 1000L;
	return 1;
}

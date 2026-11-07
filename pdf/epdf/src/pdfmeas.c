/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pdfmeas.c - the measurement run: open a document, read the pages asked
 * for, and say per page what it took: time, bytes read, objects, streams
 * unpacked, glyphs, pictures, paths. One JSON line each.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pdfmeas.h"

static void json_name(char *out, int n, const char *s)
{
	int i = 0;

	for (; *s && i < n - 1; s++)
		if (*s != '"' && *s != '\\' && (unsigned char)*s >= 32)
			out[i++] = *s;
	out[i] = 0;
}

static long clock_now(const PMEAS *m)
{
	return m->us ? m->us() : m->ms() * 1000;
}

/* the reader's stop hook, never stopping: the longest stretch between
   its calls (where EPDF hands the machine back), the page's start and end
   counted as calls */
static const PMEAS *gap_m;
static long gap_last, gap_max;
static void *gap_prev, *gap_from, *gap_to;	/* where it began and ended: the hook's callers */

static int gap_hook(void *ctx)
{
	long t = clock_now(gap_m);
	void *at = __builtin_return_address(0);

	if (t - gap_last > gap_max) {
		gap_max = t - gap_last;
		gap_from = gap_prev;
		gap_to = at;
	}
	gap_prev = at;
	gap_last = t;
	(void)ctx;
	return 0;
}

int pdf_measure(const PMEAS *m, const char *path, const char *name, const char *pages)
{
	PDOC *d = malloc(sizeof *d);
	char line[1024], nm[64];
	long t0, t1, npages, i, *sel = NULL, nsel = 0, v;
	PSTAT base;
	int rc;

	if (!d)
		return -1;
	json_name(nm, sizeof nm, name);
	if (m->heap_mark)
		m->heap_mark();
	t0 = clock_now(m);
	rc = pdf_open(d, path);
	t1 = clock_now(m);
	if (rc != 0) {
		snprintf(line, sizeof line, "{\"file\":\"%s\",\"open\":false,\"why\":\"%s\",\"ms\":%ld}", nm, d->err, (t1 - t0) / 1000);
		m->emit(line);
		pdf_close(d);
		free(d);
		return rc;
	}
	d->clock = m->us ? m->us : m->ms;
	{
		long t2 = clock_now(m);

		npages = pdf_pages(d);
		snprintf(line, sizeof line,
			 "{\"file\":\"%s\",\"open\":true,\"size\":%ld,\"ms\":%ld,\"pages\":%ld,\"pageidx_ms\":%ld,\"reads\":%ld,\"read_kb\":%ld,"
			 "\"objs\":%ld,\"objstm\":%ld,\"xref_entries\":%ld,\"xref_kb\":%ld,\"repaired\":%d,\"encrypted\":%d,\"ver\":%d,\"mem_kb\":%ld,\"heap_kb\":%ld}",
			 nm, d->size, (t1 - t0) / 1000, npages, (clock_now(m) - t2) / 1000, d->st.reads, d->st.bytes_read / 1024, d->st.objs,
			 d->st.objstm_loaded, d->nxe, d->nxe * (long)sizeof(PXE) / 1024, d->repaired, d->encrypted, d->ver, d->st.mem_peak / 1024,
			 m->heap_peak ? m->heap_peak() / 1024 : -1);
		m->emit(line);
	}
	if (npages <= 0) {
		pdf_close(d);
		free(d);
		return -1;
	}
	/* which pages */
	sel = malloc((size_t)(npages > 0 ? npages : 1) * sizeof *sel);
	if (!strcmp(pages, "all")) {
		for (i = 0; i < npages; i++)
			sel[nsel++] = i;
	} else if (!strncmp(pages, "first:", 6)) {
		v = atol(pages + 6);
		for (i = 0; i < npages && i < v; i++)
			sel[nsel++] = i;
	} else if (!strncmp(pages, "sample:", 7)) {
		v = atol(pages + 7);
		if (v < 1)
			v = 1;
		for (i = 0; i < v && nsel < npages; i++) {
			long p = v == 1 ? 0 : (npages - 1) * i / (v - 1);

			if (nsel == 0 || sel[nsel - 1] != p)
				sel[nsel++] = p;
		}
	} else {
		v = atol(pages);
		if (v >= 1 && v <= npages)
			sel[nsel++] = v - 1;
	}
	for (i = 0; i < nsel; i++) {
		PTEXT t;
		long ta, tb, hp = -1;
		PSTAT s;

		pdf_page_reset(d);
		base = d->st;
		if (m->heap_mark)
			m->heap_mark();
		ptext_init(&t);
		gap_m = m;
		d->stop = gap_hook;
		d->stop_ctx = NULL;
		gap_max = 0;
		gap_prev = gap_from = gap_to = NULL;
		ta = gap_last = clock_now(m);
		rc = pdf_page_text(d, sel[i], &t);
		tb = clock_now(m);
		gap_hook(NULL);
		d->stop = NULL;
		s = d->st;
		if (m->heap_peak)
			hp = m->heap_peak();
		snprintf(line, sizeof line,
			 "{\"file\":\"%s\",\"page\":%ld,\"ok\":%d,\"us\":%ld,\"reads\":%ld,\"read_kb\":%ld,\"objs\":%ld,\"objstm\":%ld,"
			 "\"streams\":%ld,\"raw_kb\":%ld,\"out_kb\":%ld,\"crypt_kb\":%ld,\"glyphs\":%ld,\"chunks\":%ld,\"chars\":%ld,\"lines\":%ld,"
			 "\"unmapped\":%ld,\"nomap_fonts\":%ld,\"fonts_new\":%ld,\"fonts_used\":%d,\"prog_kb\":%ld,"
			 "\"paths\":%ld,\"paints\":%ld,\"images\":%ld,\"img_px\":%ld,\"img_kb\":%ld,\"dct\":%ld,\"jpx\":%ld,\"ccitt\":%ld,\"jbig2\":%ld,\"flate\":%ld,"
			 "\"forms\":%ld,\"tu_ent\":%ld,\"t_stream\":%ld,\"t_font\":%ld,\"t_asm\":%ld,\"mem_kb\":%ld,\"heap_kb\":%ld,\"gap_ms\":%ld,\"gap_at\":\"%lx %lx %lx\"}",
			 nm, sel[i] + 1, rc == 0 && !t.failed, tb - ta, s.reads - base.reads, (s.bytes_read - base.bytes_read) / 1024,
			 s.objs - base.objs, s.objstm_loaded - base.objstm_loaded, s.streams - base.streams, (s.raw_in - base.raw_in) / 1024,
			 (s.out - base.out) / 1024, (s.crypt_bytes - base.crypt_bytes) / 1024, s.glyphs - base.glyphs, s.chunks - base.chunks,
			 t.text.len, t.lines, t.unmapped, (long)t.fonts_nomap, s.fonts - base.fonts, t.nfp, 
			 ({ long kb = 0; int k; for (k = 0; k < t.nfp; k++) kb += t.fp[k].len; kb / 1024; }),
			 s.ops_path - base.ops_path, s.ops_paint - base.ops_paint, s.images - base.images, s.image_px - base.image_px,
			 (s.image_raw - base.image_raw) / 1024, s.img_dct - base.img_dct, s.img_jpx - base.img_jpx, s.img_ccitt - base.img_ccitt,
			 s.img_jbig2 - base.img_jbig2, s.img_flate - base.img_flate, s.forms - base.forms, s.tounicode_ent - base.tounicode_ent,
			 s.t_stream - base.t_stream, s.t_font - base.t_font, s.t_asm - base.t_asm,
			 s.mem_peak / 1024, hp >= 0 ? hp / 1024 : -1, gap_max / 1000,
			 /* the hook's callers where the longest stretch began and
			    ended, and pdf_measure's own address to place them */
			 (unsigned long)gap_from, (unsigned long)gap_to, (unsigned long)pdf_measure);
		m->emit(line);
		if (m->text && t.text.s)
			m->text(sel[i] + 1, t.text.s, t.text.len);
		ptext_free(&t);
	}
	free(sel);
	pdf_close(d);
	free(d);
	return 0;
}

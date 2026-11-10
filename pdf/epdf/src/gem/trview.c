/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * trview.c - View > Translate (Erez, 8 Nov 2026; AERA-Atari
 * standards/TRANSLATION-SERVICE.md, "EWeb and EPDF: what Erez approved"):
 * the text view's paragraphs translated, page by page from the page in
 * view, the original shown until each page's translation has come.
 *
 * Direct: a page's paragraphs (src/view/trpara.c) go to translategate on
 * the Gateway's machine (common's translate.c, POST /translate, 2 KB
 * pieces), on a small stack of its own: conn.c's waits call pf_idle, which
 * hands the machine back when the slice is up (nothing on it calls the
 * AES or the VDI). Its result goes to doc.c on the main stack. Esc stops it.
 * Through the Gateway the conversion brings the translation (pdfgate's
 * translate=CODE) and nothing is asked here (doc_tr_from_file).
 *
 * With Translate off nothing here runs: tr_slice returns at once, no
 * request is made (the rigs count "translate-piece").
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "gemui.h"
#include "plat.h"
#include "conn.h"
#include "util.h"
#include "coro.h"
#include "translate.h"
#include "trpara.h"
#include "charset.h"
#include "epdf.h"
#include "gwopen.h"
#include "trview.h"

void log_event(const char *what, const char *a, const char *b);	/* (epdf.c's) */

#define TR_STACK (16L * 1024)

static CORO main_co, job_co;
static char *stack;
static int started, net_ready;
static unsigned long slice_end;

static char host[104], to[TR_LANG];
static unsigned short port = TR_PORT;
static int on;				/* View > Translate */
static int halted;			/* the document's translation given up (a refusal, Esc) */
static char why[160];			/* and why, for the one alert a document */
static int why_new;

/* the job's page and its answer, for the main stack */
static long job_page = -1;
static int job_done, job_state;
static char *job_text;
static long job_len;
static char job_from[TR_LANG];
static long pieces;			/* requests made for this document (the rigs' counter) */

/* conn.c's waits call it at most every 40 ms (CONN_SLICE_MS): each call a
   turn for the machine, the slice's end not waited for on top (they were
   90-100 ms on ST:mono); the joining and splitting of a page too
   (trpara.c's tp_turn, every few paragraphs: a page put back took 275 ms) */
static void idle(void)
{
	coro_switch(&job_co, &main_co);
}

static void tp_idle(void)
{
	if (tos_ms() >= slice_end)
		coro_switch(&job_co, &main_co);
}

/* the page's translation, on the job's stack: job_* set, then back */
static void translate_page(long n)
{
	const char *t;
	long len, ol = 0;
	char *orig;
	TPARA p;
	TRANSLATE tr;
	SBUF out;
	int r;

	job_text = NULL;
	job_len = 0;
	job_from[0] = '\0';
	t = doc_page_original(n, &len);
	if (!t) {
		job_state = TR_NOTHING;
		return;
	}
	if (!(orig = malloc((size_t)(len > 0 ? len : 1)))) {
		job_state = TR_FAILED;
		return;
	}
	memcpy(orig, t, (size_t)len);	/* (the store's copy goes with its next use) */
	tp_turn = tp_idle;
	if (tp_split(orig, len, &p) != 0) {
		free(orig);
		job_state = TR_FAILED;
		return;
	}
	if (p.nsend == 0) {
		tp_free(&p);
		free(orig);
		job_state = TR_NOTHING;
		return;
	}
	sb_init(&out);
	translate_begin(&tr, host, port, to, NULL, p.utf8, p.ulen);
	pf_idle = idle;
	while ((r = translate_step(&tr, &out)) == 1) {
		char a[16];

		pieces++;
		snprintf(a, sizeof a, "%ld", n + 1);
		log_event("translate-piece", a, NULL);
		idle();			/* (a piece's answer handled: a turn before the next) */
	}
	pf_idle = NULL;
	if (r < 0) {
		/* (stopped from here, tr_stop: the page asked again later, nothing
		   halted; Esc halts the document itself, tr_halt) */
		if (!conn_stop) {
			snprintf(why, sizeof why, "%s", tr.err);
			why_new = 1;
			halted = 1;
		}
		job_state = TR_FAILED;
	} else if (tr.same) {
		job_state = TR_SAME;
		snprintf(job_from, sizeof job_from, "%s", to);
	} else {
		job_text = tp_join(orig, len, &p, out.s, out.len, &ol);
		job_len = ol;
		job_state = job_text ? TR_DONE : TR_FAILED;
		snprintf(job_from, sizeof job_from, "%s", tr.from_used);
	}
	tp_turn = NULL;
	sb_free(&out);
	tp_free(&p);
	free(orig);
}

static void job(void)
{
	for (;;) {
		translate_page(job_page);
		job_done = 1;
		coro_switch(&job_co, &main_co);
	}
}

void tr_setup(const char *gate_host, unsigned short trport)
{
	snprintf(host, sizeof host, "%s", gate_host ? gate_host : "");
	port = trport ? trport : TR_PORT;
}

void tr_set(int want_on, const char *lang)
{
	if (lang && strcmp(lang, to)) {
		tr_stop();
		snprintf(to, sizeof to, "%s", lang);
		doc_tr_forget();	/* (another language: the old translations are no use) */
	}
	on = want_on && to[0];
	tr_document();
}

int tr_on(void)
{
	return on;
}

const char *tr_to(void)
{
	return to;
}

/* a document opened, or Translate turned on or off: the view as it should be */
void tr_document(void)
{
	if (!doc_is_open())
		return;
	if (!on) {
		doc_set_translate(0);
		return;
	}
	if (doc_tr_begin(to) == 0)
		doc_set_translate(1);
}

void tr_new_document(void)
{
	tr_stop();
	halted = 0;
	why_new = 0;
	pieces = 0;
	tr_document();
}

/* the next page to translate: the one in view, then on from it, of those
   read; -1 none */
static long next_page(void)
{
	DOCVIEW *v = doc_view();
	long top, i, np = doc_pages();

	if (!v || np <= 0)
		return -1;
	top = dv_page_at_top(v);
	for (i = 0; i < np; i++) {
		long n = (top + i) % np;

		if (doc_tr_state(n) == TR_NONE && doc_page_len(n) >= 0)
			return n;
	}
	return -1;
}

int tr_busy(void)
{
	return job_page >= 0 ||
	       (on && !halted && host[0] && doc_is_open() && !doc_opening() && !doc_tr_from_file() &&
		!gw_busy() && next_page() >= 0);
}

long tr_slice(int ms)
{
	long n;

	if (!on || !doc_is_open())
		return -1;
	if (job_page < 0) {
		if (halted || !host[0] || doc_opening() || doc_tr_from_file() || gw_busy())
			return -1;	/* (through the Gateway: the conversion brings it) */
		if ((n = next_page()) < 0)
			return -1;
		if (!net_ready && !(net_ready = net_init())) {
			/* (no STinG, MiNTnet or MagiC-Net: said once, and the document's
			   translation stopped) */
			snprintf(why, sizeof why, "the Gateway is reached through STinG, MiNTnet or MagiC-Net, and none is running");
			why_new = 1;
			halted = 1;
			return -1;
		}
		if (!stack && !(stack = malloc(TR_STACK)))
			return -1;
		if (!started) {
			coro_start(&job_co, stack, TR_STACK, job);
			started = 1;
		}
		job_page = n;
		job_done = 0;
		conn_stop = 0;
	}
	slice_end = tos_ms() + (unsigned long)ms;
	coro_switch(&main_co, &job_co);
	if (!job_done)
		return -1;
	n = job_page;
	job_page = -1;
	doc_tr_put(n, job_state, job_state == TR_DONE ? job_text : NULL, job_len, job_from);
	free(job_text);
	job_text = NULL;
	{
		char a[16];

		snprintf(a, sizeof a, "%ld", n + 1);
		log_event("translated", a, job_state == TR_DONE ? "done" : job_state == TR_SAME ? "same" :
			  job_state == TR_NOTHING ? "nothing" : "failed");
	}
	return n;
}

void tr_stop(void)
{
	if (job_page < 0)
		return;
	conn_stop = 1;
	while (job_page >= 0 && !job_done) {
		slice_end = tos_ms() + 50;
		coro_switch(&main_co, &job_co);
	}
	free(job_text);
	job_text = NULL;
	job_page = -1;
	conn_stop = 0;
}

void tr_halt(void)
{
	tr_stop();
	halted = 1;
}

int tr_page(long *n)
{
	*n = job_page;
	return job_page >= 0;
}

const char *tr_failed(void)
{
	if (!why_new)
		return NULL;
	why_new = 0;
	return why;
}

long tr_pieces(void)
{
	return pieces;
}

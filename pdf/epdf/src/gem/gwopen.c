/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * gwopen.c - Options > Open on the Gateway (Erez, 7 Nov 2026): a PDF that
 * isn't converted is shown as it is (the text view) while the Gateway
 * converts it; the converted copy comes into a file of EPDF's own beside
 * the program (gate1.pdf or gate2.pdf, the one not open) and EPDF takes it
 * up when it has come. Nothing is kept beside the PDF (Erez, 8 Nov 2026:
 * "no pre-save"): File > Save Optimised... keeps the copy where the user
 * says.
 *
 * The exchange is src/pigate.c on a small stack of its own: conn.c's
 * waits call pf_idle, which hands the machine back when the slice is up
 * (nothing on it calls the AES). Esc (conn_stop) stops it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "gem.h"
#include "plat.h"
#include "conn.h"
#include "pigate.h"
#include "coro.h"
#include "scrsize.h"
#include "gwopen.h"
#include "gemui.h"

void log_event(const char *what, const char *a, const char *b);	/* (epdf.c's, the rigs' console) */

#define GW_STACK (12L * 1024)

static CORO main_co, job_co;
static char *stack;
static int started, busy, finished, rc, waiting, net_ready, phase;
static long done, total, part, whole;	/* (part, whole: the phase's bytes) */
static unsigned long slice_end;
static char in_path[256], out_path[256], host[64], err[120];
static char translate_to[8];

void gw_set_translate(const char *to)
{
	snprintf(translate_to, sizeof translate_to, "%s", to ? to : "");
}
static unsigned short port;

static void idle(void)
{
	if (tos_ms() >= slice_end)
		coro_switch(&job_co, &main_co);
}

static int progress(void *u, int ph, long d, long t)
{
	(void)u;
	if (t <= 0)
		t = 1;
	waiting = ph == PG_WORK && d == 0;
	phase = ph;
	if (ph != PG_WORK) {
		part = d;
		whole = t;
	}
	total = 1000;
	done = ph == PG_SEND ? d * 100 / t : ph == PG_WORK ? 100 + d * 800 / t : 900 + d * 100 / t;
	idle();
	return 0;
}

/* the sizes EConvert would send on this machine (its screen_sizes: EPDF's
   window's kind, the text column's margins) */
static int sz_page = 640, sz_pic = 640;

/* asked of the AES on the main stack (gw_start): the job's stack never
   calls it */
static void sizes_measure(void)
{
	short w, h;

	scr_work_size(NAME | CLOSER | FULLER | MOVER | SIZER | UPARROW | DNARROW | VSLIDE, 8, &w, &h);
	sz_page = w;
	scr_work_size(NAME | CLOSER | FULLER | MOVER | SIZER | UPARROW | DNARROW | VSLIDE, 0, &w, &h);
	sz_pic = w;
}

static void sizes(int *page_w, int *pic_w)
{
	*page_w = sz_page;
	*pic_w = sz_pic;
}

static void job(void)
{
	for (;;) {
		PGOPT o;
		int pw, iw;

		memset(&o, 0, sizeof o);
		/* on a screen of 16 colours or more only the pages in colour
		   ("colouronly": a page with colour gets its /Colour, one without
		   its /Mono; nothing made twice, AERA-Gateway 1.42 on; Erez, 13
		   Nov 2026); a mono screen asks for the mono pages (7 Nov 2026) */
		o.pages = planes >= 4 ? "colouronly" : "mono";
		o.photos = "indexed";
		o.pictures = "screen";
		sizes(&pw, &iw);
		o.page_w = pw;
		o.pic_w = iw;
		o.page_double = -1;
		o.translate = translate_to[0] ? translate_to : NULL;	/* (View > Translate on) */
		{
			char q[24];

			snprintf(q, sizeof q, "%d %d", pw, iw);	/* (the rigs' direct conversion takes the same) */
			log_event("gateway-sizes", q, NULL);
		}
		pf_idle = idle;
		rc = pg_convert(host, port, &o, in_path, out_path, progress, NULL, err, sizeof err);
		pf_idle = NULL;
		finished = 1;
		coro_switch(&job_co, &main_co);
	}
}

static char temp_dir[160];
static int temp_k;
static char temp_made[2];	/* a copy written there this session: only those are deleted (files.py, 6.5) */

void gw_set_temp_dir(const char *dir)
{
	snprintf(temp_dir, sizeof temp_dir, "%s", dir);
}

/* EPDF's own two: the one written now and the other (open, perhaps) */
static void temp_name(int k, char *p, int n)
{
	snprintf(p, (size_t)n, "%s%sgate%d.pdf", temp_dir, temp_dir[0] ? "\\" : "", k + 1);
}

int gw_is_temp(const char *path)
{
	char p[200];
	int k;

	for (k = 0; k < 2; k++) {
		temp_name(k, p, sizeof p);
		if (!strcasecmp(p, path))
			return 1;
	}
	return 0;
}

void gw_forget(const char *open_now)
{
	char p[200];
	int k;

	for (k = 0; k < 2; k++) {
		if (!temp_made[k])
			continue;	/* (none written: nothing to look for on the disk) */
		temp_name(k, p, sizeof p);
		if (!open_now || strcasecmp(p, open_now)) {
			Fdelete(p);
			temp_made[k] = 0;
		}
	}
}

const char *gw_source(void)
{
	return in_path;
}

int gw_start(const char *pdf, const char *open_now, const char *h, unsigned short p)
{
	if (busy)
		return GW_BUSY;
	if (!h || !h[0])
		return GW_NO_ADDRESS;
	if (!net_ready && !(net_ready = net_init()))
		return GW_NO_NETWORK;
	if (!stack && !(stack = malloc(GW_STACK)))
		return GW_NO_MEMORY;
	if (!started) {
		coro_start(&job_co, stack, GW_STACK, job);
		started = 1;
	}
	sizes_measure();
	strncpy(in_path, pdf, sizeof in_path - 1);
	/* the one that isn't the document open (a copy shown till now) */
	temp_name(temp_k, out_path, sizeof out_path);
	if (open_now && !strcasecmp(out_path, open_now)) {
		temp_k ^= 1;
		temp_name(temp_k, out_path, sizeof out_path);
	}
	temp_made[temp_k] = 1;		/* (written from now on: deleted when let go) */
	strncpy(host, h, sizeof host - 1);
	port = p;
	busy = 1;
	finished = waiting = 0;
	phase = PG_SEND;
	part = whole = 0;
	done = 0;
	total = 1000;
	conn_stop = 0;
	return GW_STARTED;
}

int gw_busy(void)
{
	return busy;
}

int gw_slice(int ms)
{
	if (!busy)
		return 0;
	slice_end = tos_ms() + (unsigned long)ms;
	coro_switch(&main_co, &job_co);
	if (finished)
		busy = 0;
	return finished;
}

void gw_stop(void)
{
	if (busy)
		conn_stop = 1;
}

int gw_progress(long *d, long *t)
{
	*d = done;
	*t = total;
	return waiting;
}

/* the status line's words for where it is (guideline 7.3, Erez 7 Nov
   2026: the three phases): "Sending to the Gateway... 12 KB of 3.4 MB",
   "The Gateway is converting...", "Receiving from the Gateway... 40 KB of
   80 KB"; the
   bytes of the phase in *d of *t (0 of 0 while it converts) */
/* "820 KB", "3.4 MB" (the family's sizes in a job: Erez, 9 Nov 2026, D35) */
static void size_words(char *s, size_t n, long bytes)
{
	if (bytes >= 1024L * 1024L)
		snprintf(s, n, "%ld.%ld MB", bytes / (1024L * 1024L), bytes % (1024L * 1024L) * 10 / (1024L * 1024L));
	else
		snprintf(s, n, "%ld KB", (bytes + 1023) / 1024);
}

const char *gw_words(char *w, int n, long *d, long *t)
{
	if (phase == PG_SEND || phase == PG_RECV) {
		char a[16], b[16];

		size_words(a, sizeof a, part);
		size_words(b, sizeof b, whole);
		if (whole > 0)	/* "12 KB of 3.4 MB" */
			snprintf(w, (size_t)n, "%s the Gateway... %s of %s", phase == PG_SEND ? "Sending to" : "Receiving from",
				 a, b);
		else
			snprintf(w, (size_t)n, "%s the Gateway... %s", phase == PG_SEND ? "Sending to" : "Receiving from", a);
		*d = part;
		*t = whole;
	} else {
		snprintf(w, (size_t)n, waiting ? "Waiting for the Gateway" : translate_to[0] ? "The Gateway is converting and translating..." : "The Gateway is converting...");
		*d = *t = 0;
	}
	return w;
}

int gw_result(char *copy, int ncopy, char *why, int nwhy)
{
	snprintf(copy, (size_t)ncopy, "%s", out_path);
	snprintf(why, (size_t)nwhy, "%s", err);
	finished = 0;
	return rc;
}

const char *gw_host(void)
{
	return host;
}

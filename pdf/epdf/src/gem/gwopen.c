/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * gwopen.c - File > Open on the Gateway (Erez, 7 Nov 2026): a PDF that
 * isn't converted is shown as it is (the text view) while the Gateway
 * converts it; the converted copy goes into a folder "atari" beside it
 * (guideline 6.6: the family's names lowercase; the copy keeps the
 * original's name) and EPDF takes it up when it has come. A copy there
 * that is newer than the PDF is opened at once.
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

void log_event(const char *what, const char *a, const char *b);	/* (epdf.c's, the rigs' console) */

#define GW_STACK (12L * 1024)

static CORO main_co, job_co;
static char *stack;
static int started, busy, finished, rc, waiting, net_ready, phase;
static long done, total, part, whole;	/* (part, whole: the phase's bytes) */
static unsigned long slice_end;
static char in_path[256], out_path[256], host[64], err[120];
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
static void sizes(int *page_w, int *pic_w)
{
	short w, h;

	scr_work_size(NAME | CLOSER | FULLER | MOVER | SIZER | UPARROW | DNARROW | VSLIDE, 8, &w, &h);
	*page_w = w;
	scr_work_size(NAME | CLOSER | FULLER | MOVER | SIZER | UPARROW | DNARROW | VSLIDE, 0, &w, &h);
	*pic_w = w;
}

static void job(void)
{
	for (;;) {
		PGOPT o;
		int pw, iw;

		memset(&o, 0, sizeof o);
		o.pages = "mono";
		o.photos = "indexed";
		o.pictures = "screen";
		sizes(&pw, &iw);
		o.page_w = pw;
		o.pic_w = iw;
		o.page_double = -1;
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

/* the folder the copy goes in: "atari" beside pdf */
static void folder_of(const char *pdf, char *dir, int n)
{
	const char *s = strrchr(pdf, '\\');
	int k = s ? (int)(s - pdf) : 0;

	snprintf(dir, (size_t)n, "%.*s%satari", k, pdf, k ? "\\" : "");
}

int gw_in_copy_folder(const char *pdf)
{
	const char *s = strrchr(pdf, '\\'), *p;

	if (!s)
		return 0;
	for (p = s; p > pdf && p[-1] != '\\'; p--)
		;
	return s - p == 5 && !strncasecmp(p, "atari", 5);
}

/* a file's date and time as one number (GEMDOS's), or -1 */
static long stamp(const char *path)
{
	DTA dta, *old = Fgetdta();
	long r = -1;

	Fsetdta(&dta);
	if (Fsfirst(path, 0x07) == 0)
		r = (long)dta.d_date << 16 | dta.d_time;
	Fsetdta(old);
	return r;
}

int gw_copy(const char *pdf, char *copy, int n)
{
	char dir[256];
	const char *base = strrchr(pdf, '\\');
	long a, b;

	folder_of(pdf, dir, sizeof dir);
	snprintf(copy, (size_t)n, "%s\\%s", dir, base ? base + 1 : pdf);
	a = stamp(pdf);
	b = stamp(copy);
	return a >= 0 && b >= 0 && b >= a;
}

int gw_start(const char *pdf, const char *h, unsigned short p)
{
	char dir[256];
	const char *base = strrchr(pdf, '\\');

	if (busy)
		return GW_BUSY;
	if (!h || !h[0])
		return GW_NO_ADDRESS;
	folder_of(pdf, dir, sizeof dir);
	if (Fattrib(dir, 0, 0) < 0 && Dcreate(dir) < 0)
		return GW_NO_FOLDER;
	if (!net_ready && !(net_ready = net_init()))
		return GW_NO_NETWORK;
	if (!stack && !(stack = malloc(GW_STACK)))
		return GW_NO_MEMORY;
	if (!started) {
		coro_start(&job_co, stack, GW_STACK, job);
		started = 1;
	}
	strncpy(in_path, pdf, sizeof in_path - 1);
	snprintf(out_path, sizeof out_path, "%s\\%s", dir, base ? base + 1 : pdf);
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
   2026: the three phases): "Sending to the Gateway... 12 KB", "The
   Gateway is converting...", "Receiving from the Gateway... 40 KB"; the
   bytes of the phase in *d of *t (0 of 0 while it converts) */
const char *gw_words(char *w, int n, long *d, long *t)
{
	if (phase == PG_SEND || phase == PG_RECV) {
		snprintf(w, (size_t)n, "%s the Gateway... %ld KB", phase == PG_SEND ? "Sending to" : "Receiving from",
			 (part + 1023) / 1024);
		*d = part;
		*t = whole;
	} else {
		snprintf(w, (size_t)n, waiting ? "Waiting for the Gateway" : "The Gateway is converting...");
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

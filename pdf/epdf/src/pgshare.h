/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pgshare.h - a Gateway conversion's progress by time (pgshare.c)
 */
#ifndef PGSHARE_H
#define PGSHARE_H

/* where the job is (pigate.h's) */
enum { PG_SEND, PG_WORK, PG_RECV };

typedef struct {
	unsigned long t0, ph0, last;	/* the exchange's start, this part's, the last call's (ms) */
	int ph;			/* PG_SEND, PG_WORK, PG_RECV */
	long size, sent;	/* the PDF's bytes, sent so far */
	long send_ms, work_ms;	/* those parts' times once over; -1 */
	long out, got, got0;	/* the result's bytes (-1 not yet said), come so far, at its first call */
	long work_done;		/* the Gateway's X-Progress (of its total) */
	long shown;		/* the share given so far (never less) */
	long left;		/* the time still to come at the last call (ms) */
	int over;		/* the last byte has come */
} PGSHARE;

void pgs_start(PGSHARE *s, unsigned long now);
/* the share of the whole exchange gone, in thousandths, after a call
   of pigate's progress with these (phase, done of total) at now */
long pgs_share(PGSHARE *s, int phase, long done, long total, unsigned long now);

/* the share at now, between calls: the time to come counted down */
long pgs_now(PGSHARE *s, unsigned long now);

#endif

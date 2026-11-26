/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pgshare.c - how far a conversion on the Gateway is, by time: the share
 * of the whole exchange (sending the PDF, the Gateway's work, the
 * converted PDF coming back) that has gone, in thousandths, for the
 * progress bar. Included by pigate.c (one object for the programs'
 * layout); compiled alone by tests/pgshare.c.
 *
 * Fixed shares of the bar (a tenth for sending, eight tenths for the
 * Gateway's work, a tenth for receiving) left it near 20 % while the Atari
 * sent and received, which is most of the time, and jumped to 90 % in the
 * second or two the Pi took (decided 24 Nov 2026: the bar to follow the
 * Gateway's quick work as it is). Here the share is the time
 * gone over the time gone and the time still to come, each part's time to
 * come from what has been measured:
 * - sending: the bytes still to send at the rate they have gone so far;
 * - the work: from the Gateway's own X-Progress once it has said any
 *   (the time it took for what it has done); before that, the last
 *   conversion's work for each second of sending, or a fifth of the
 *   sending;
 * - receiving: the converted PDF's size (once the answer says it; before,
 *   the last conversion's size for each byte sent, or half the PDF) at the
 *   rate it comes (until it has come for a second, the rate it was sent).
 * The share never goes back (a part found to be longer holds the bar; one
 * found shorter is caught up over a few reads),
 * and it stays under 990 until the last byte has come. Between the
 * exchange's calls (a piece on a slow line, the Gateway's quiet work)
 * pgs_now moves it on with the time, the time to come counted down.
 */
#include "pgshare.h"

/* what the last conversion took, for the next one's first guess
   (thousandths: the work's time for each of the sending's, the result's
   bytes for each of the PDF's) */
static long work_per_send = 200, out_per_in = 500;

void pgs_start(PGSHARE *s, unsigned long now)
{
	s->t0 = s->ph0 = s->last = now;
	s->ph = PG_SEND;
	s->size = s->sent = 0;
	s->send_ms = s->work_ms = -1;
	s->out = -1;
	s->got = s->got0 = 0;
	s->work_done = 0;
	s->shown = 0;
	s->left = -1;	/* (no call yet: nothing to go by) */
	s->over = 0;
}

/* a * b / c without overflow for the times and sizes here (ms up to an
   hour, bytes up to 2 GB) */
static long muldiv(long a, long b, long c)
{
	if (c <= 0)
		return 0;
	while (a > 0 && b > 0 && a > 0x7fffffffL / b) {
		a >>= 1;	/* (a little precision for no overflow, and no floating point) */
		c >>= 1;
		if (c <= 0)
			return 0x7fffffffL;
	}
	return a * b / c;
}

/* the share shown moved up to share: a part found shorter than guessed
   catches up over a few reads, not in one jump */
static long rise(PGSHARE *s, long share)
{
	long d = share - s->shown;

	if (d > 40)
		d = 40 + (d - 40) / 4;
	if (d > 0)
		s->shown += d;
	return s->shown;
}

long pgs_share(PGSHARE *s, int phase, long done, long total, unsigned long now)
{
	long gone, send_left, work_left, recv_left, sent_ms, rate_ms, out, share;

	if (total <= 0)
		total = 1;
	if (phase != s->ph) {
		/* a part over: its time kept. Sending ended at its last call;
		   the work from then to the answer (a Gateway quick enough to
		   say nothing on the way included) */
		if (s->ph == PG_SEND) {
			s->send_ms = (long)(s->last - s->t0);
			s->ph0 = s->last;
		}
		if (phase == PG_RECV) {
			s->work_ms = (long)(now - s->ph0);
			s->ph0 = now;
			s->got0 = done;	/* (what came before its first call: no part of its rate) */
		}
		s->ph = phase;
	}
	s->last = now;
	gone = (long)(now - s->t0);
	if (gone < 1)
		gone = 1;
	if (phase == PG_SEND) {
		s->sent = done;
		s->size = total;
	} else if (phase == PG_WORK)
		s->work_done = done;
	else {
		s->out = total;
		s->got = done;
	}
	/* the sending's time, gone and to come */
	sent_ms = s->send_ms >= 0 ? s->send_ms : (long)(now - s->ph0);
	if (sent_ms < 1)
		sent_ms = 1;
	send_left = 0;
	if (phase == PG_SEND)
		send_left = s->sent > 0 ? muldiv(s->size - s->sent, sent_ms, s->sent) : sent_ms;
	/* the work still to come */
	if (phase == PG_RECV)
		work_left = 0;
	else if (phase == PG_WORK && s->work_done > 0)
		work_left = muldiv((long)(now - s->ph0), total - s->work_done, s->work_done);
	else {
		long all = muldiv(sent_ms + send_left, work_per_send, 1000);

		work_left = all - (phase == PG_WORK ? (long)(now - s->ph0) : 0);
		if (work_left < 0)
			work_left = 0;
	}
	/* the result still to come, at the rate it comes (or was sent) */
	out = s->out >= 0 ? s->out : muldiv(s->size, out_per_in, 1000);
	if (phase == PG_RECV && (long)(now - s->ph0) >= 1000 && s->got > s->got0)
		rate_ms = (long)(now - s->ph0), recv_left = muldiv(out - s->got, rate_ms, s->got - s->got0);
	else
		recv_left = s->size > 0 ? muldiv(out - (phase == PG_RECV ? s->got : 0), sent_ms + send_left, s->size) : 0;
	if (recv_left < 0)
		recv_left = 0;
	s->left = send_left + work_left + recv_left;
	share = muldiv(gone, 1000, gone + s->left);
	if (phase == PG_RECV && done >= total) {
		share = 1000;
		s->over = 1;
		/* for the next one's first guess */
		if (s->send_ms > 0 && s->work_ms >= 0)
			work_per_send = muldiv(s->work_ms, 1000, s->send_ms);
		if (s->size > 0)
			out_per_in = muldiv(s->out, 1000, s->size);
		s->shown = 1000;
		return 1000;
	}
	if (share > 990)
		share = 990;
	return rise(s, share);
}

long pgs_now(PGSHARE *s, unsigned long now)
{
	long gone = (long)(now - s->t0), left = s->left - (long)(now - s->last), share;

	if (s->over || s->left < 0)
		return s->shown;
	if (left < 0)
		left = 0;
	if (gone < 1)
		gone = 1;
	share = muldiv(gone, 1000, gone + left);
	if (share > 990)
		share = 990;
	return rise(s, share);
}

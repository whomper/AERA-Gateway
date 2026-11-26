/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * rdraw.c - render.ovl loaded with common's ovl.c (as TLS.OVL is) and
 * handed the program's memory and turn (rovl.h). The overlay draws; the
 * program shows what it drew.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "plat.h"
#include "ovl.h"
#include "rdraw.h"
int pics_soft_machine(void);	/* (pics.c) */

char rdraw_path[200] = RDRAW_FILE;

static const ROVL *ov;
static void *image;
static long taken;
static ROVL_HOST host;
static void *resv;		/* render.ovl's room set aside (rdraw_reserve), or NULL */
static long resv_size;

/* a 68020 or better that runs 68030 code: render.ovl is built for the 68030.
   Not a FireBee (_CF_), whose FireTOS says _CPU 60 for a ColdFire that has
   no 68030 code's 64-bit multiplies or cache instructions: there EPDF keeps
   to the text view and a converted copy's stored pages, as on an ST (the
   hub's FireBee stand-in, 15 Nov 2026: render.ovl ended EmuTOS on its first
   page) */
int rdraw_machine(void)
{
#ifdef __mcoldfire__
	return 1;			/* the FireBee package's: a FireBee, render.ovl its ColdFire build */
#endif
	return get_cookie(0x5F435055L) >= 20	/* _CPU: 0, 10, 20, 30, 40, 60 */
	       && !get_cookie(0x5F43465FL);	/* _CF_ */
}

/* a screen of 16 colours or fewer: render.ovl's small glyphs a bit a
   pixel (ROVL_HOST's crisp), from the next page drawn */
void rdraw_crisp(int on)
{
	host.crisp = on;
}

int rdraw_load(int (*turn)(void), char *err, int errlen)
{
	const ROVL *o;

	host.turn = turn;
	if (ov)
		return 1;
	if (!rdraw_machine()) {
		snprintf(err, (size_t)errlen, "drawing pages needs a 68020 or better (a TT, a Falcon)");
		return 0;
	}
	image = resv ? ovl_load_into(rdraw_path, resv, resv_size, &taken, err, errlen) : ovl_load(rdraw_path, &taken, err, errlen);
	if (!image) {
		if (pf_size(rdraw_path) < 0)
			snprintf(err, (size_t)errlen, RDRAW_NAME " is missing: copy it from EPDF's release, beside " RDRAW_PRG);
		return 0;
	}
	ovl_flush();			/* its code was written as data */
	o = image;
	host.size = sizeof host;
	host.soft = pics_soft_machine();
	host.malloc = malloc;
	host.calloc = calloc;
	host.realloc = realloc;
	host.free = free;
	if (o->magic != ROVL_MAGIC || o->size < (short)sizeof(ROVL)) {
		snprintf(err, (size_t)errlen, RDRAW_NAME " is damaged: copy it again from EPDF's release");
		rdraw_unload();
		return 0;
	}
	if (o->major != ROVL_MAJOR || !o->init(&host)) {
		snprintf(err, (size_t)errlen, RDRAW_NAME " %.7s doesn't suit this EPDF: take the one from its release",
			 o->version);
		rdraw_unload();
		return 0;
	}
	ov = o;
	return 1;
}

const ROVL *rdraw_ovl(void)
{
	return ov;
}

long rdraw_taken(void)
{
	return ov ? taken : 0;
}

void rdraw_unload(void)
{
	ov = NULL;
	if (image != resv)	/* (one in the room set aside: the room kept) */
		free(image);
	image = NULL;
	taken = 0;
}

/*
 * render.ovl's room set aside, a GEMDOS block of its size, before a
 * document's heap grows: on a 4 MB Falcon under NVDI its 643 KB came
 * after the heap's blocks (about 577 KB each) and the page's bitmap had
 * split memory, and it loaded only when two blocks lay end to end (2.34,
 * by luck: 100 bytes more and View > Pages drew nothing; the rig's
 * drawn.py, 16 Nov 2026). Only where it must: a machine that draws pages,
 * render.ovl beside the program, no TT RAM, and less than RDRAW_ROOMY
 * free in one piece (a 4 MB machine: a Falcon had 2.3 MB at the
 * document's opening and split it later). 1 when set aside
 */
#define RDRAW_ROOMY (4096L * 1024)

int rdraw_reserve(void)
{
	long need, tt, fr;
	void *m;

	if (resv || ov || !rdraw_machine() || (need = ovl_need(rdraw_path)) <= 0)
		return 0;
	tt = (long)Mxalloc(-1L, 1);
	if (tt != -32 && tt > 0)
		return 0;		/* TT RAM: room enough there */
	fr = (long)Malloc(-1L);
	if (fr >= RDRAW_ROOMY || fr < need)
		return 0;
	m = (void *)Malloc(need);
	if ((long)m <= 0)
		return 0;
	resv = m;
	resv_size = need;
	return 1;
}

long rdraw_reserved(void)
{
	return resv ? resv_size : 0;
}

/* the room set aside that a refusal would give back: render.ovl not in it */
long rdraw_spare(void)
{
	return resv && image != resv ? resv_size : 0;
}

/* the room given back to TOS (render.ovl not in it: rdraw_unload first) */
void rdraw_unreserve(void)
{
	if (!resv || image == resv)
		return;
	Mfree(resv);
	resv = NULL;
	resv_size = 0;
}

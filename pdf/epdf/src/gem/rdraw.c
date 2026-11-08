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

char rdraw_path[200] = "render.ovl";

static const ROVL *ov;
static void *image;
static long taken;
static ROVL_HOST host;

int rdraw_machine(void)
{
	return get_cookie(0x5F435055L) >= 20;	/* _CPU: 0, 10, 20, 30, 40, 60 */
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
	image = ovl_load(rdraw_path, &taken, err, errlen);
	if (!image) {
		if (pf_size(rdraw_path) < 0)
			snprintf(err, (size_t)errlen, "RENDER.OVL is missing: copy it from EPDF's release, beside EPDF.PRG");
		return 0;
	}
	ovl_flush();			/* its code was written as data */
	o = image;
	host.size = sizeof host;
	host.malloc = malloc;
	host.calloc = calloc;
	host.realloc = realloc;
	host.free = free;
	if (o->magic != ROVL_MAGIC || o->size < (short)sizeof(ROVL)) {
		snprintf(err, (size_t)errlen, "RENDER.OVL is damaged: copy it again from EPDF's release");
		rdraw_unload();
		return 0;
	}
	if (o->major != ROVL_MAJOR || !o->init(&host)) {
		snprintf(err, (size_t)errlen, "RENDER.OVL %.7s doesn't suit this EPDF: take the one from its release",
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
	free(image);
	image = NULL;
	taken = 0;
}

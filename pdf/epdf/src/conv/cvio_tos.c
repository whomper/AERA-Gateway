/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* cvio_tos.c - EConvert's files written on GEMDOS (Fcreate, Fwrite) */
#include "tos.h"
#include "cvio.h"

void *cv_create(const char *path)
{
	long h = Fcreate(path, 0);

	return h < 0 ? (void *)0 : (void *)(h + 1);	/* handle 0 is a handle, not NULL */
}

long cv_write(void *h, const void *b, long n)
{
	long k = Fwrite((short)((long)h - 1), n, b);

	return k == n ? n : -1;
}

int cv_close(void *h)
{
	return Fclose((short)((long)h - 1)) < 0 ? -1 : 0;
}

void cv_remove(const char *path)
{
	Fdelete(path);
}

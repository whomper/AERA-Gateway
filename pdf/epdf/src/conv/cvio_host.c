/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* cvio_host.c - EConvert's files written on this computer (stdio) */
#include <stdio.h>
#include "cvio.h"

void *cv_create(const char *path)
{
	return fopen(path, "wb");
}

long cv_write(void *h, const void *b, long n)
{
	return (long)fwrite(b, 1, (size_t)n, (FILE *)h) == n ? n : -1;
}

int cv_close(void *h)
{
	return fclose((FILE *)h) ? -1 : 0;
}

void cv_remove(const char *path)
{
	remove(path);
}

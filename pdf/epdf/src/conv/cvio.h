/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* cvio.h - EConvert's files written: the host's stdio or GEMDOS */
#ifndef CVIO_H
#define CVIO_H

void *cv_create(const char *path);		/* NULL when it can't be made */
long cv_write(void *h, const void *b, long n);	/* bytes written, -1 */
int cv_close(void *h);				/* 0 ok */
void cv_remove(const char *path);

#endif

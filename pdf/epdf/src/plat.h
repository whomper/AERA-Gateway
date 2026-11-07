/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * plat.h - the file layer common/src (util.c, futil.c) and EPDF's own
 * files (the INF, the text cache) take: GEMDOS on the Atari
 * (pdfio_tos.c), POSIX on the host (pdfio_host.c) for the tests.
 */
#ifndef PLAT_H
#define PLAT_H

#ifdef PDF_TOS
#define PF_SEP '\\'
#else
#define PF_SEP '/'
#endif

#define PF_READ   0
#define PF_WRITE  1			/* create or truncate */
#define PF_UPDATE 2			/* read and write a file there is */

/* a handle >= 0, or < 0 on error */
int  pf_open(const char *path, int mode);
long pf_read(int h, void *buf, long n);
long pf_write(int h, const void *buf, long n);
long pf_seek(int h, long off);		/* from the start: the new position, or < 0 */
void pf_close(int h);
long pf_size(const char *path);		/* by its folder's listing; -1 if there is none */
int  pf_remove(const char *path);	/* 0 when done */
int  pf_rename(const char *from, const char *to);

/* common/src/futil.c */
char *pf_load(const char *path, long *len);
int  pf_save(const char *path, const void *data, long len);
extern long (*pf_size_open)(int h);
extern void (*pf_save_turn)(void);

#endif

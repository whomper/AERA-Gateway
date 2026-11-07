/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * plat.h - the file layer common/src (util.c, futil.c) and EPDF's own
 * files (the INF, the text cache) take: GEMDOS on the Atari
 * (pdfio_tos.c), POSIX on the host (pdfio_host.c) for the tests. And
 * what common's conn.c and net_tos.c take for the Pi converter: the
 * clock, the date and the turn they give while they wait (pftime_tos.c;
 * the host's tests bring their own).
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
#define PF_APPEND 3			/* write at the end; made if there is none */

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

/* common/src/conn.c, common/atari/net_tos.c */
typedef struct {
	short year, mon, day, hour, min, sec;
} PFTIME;
void pf_now(PFTIME *t);
unsigned long pf_ms(void);		/* tos_ms(): the family's clock, corrected */
extern void (*pf_idle)(void);		/* a turn for the machine while they wait; NULL none */
void pf_entropy(unsigned char *buf, int n);	/* unguessable bytes (a DNS id), not keys */

/* common/atari/net_tos.c (STinG, MiNTnet): a slot >= 0, or < 0 with err */
int  net_init(void);
const char *net_stack(void);
int  net_open(const char *host, unsigned short port, char *err, int errlen);
long net_write(int slot, const void *buf, long n);
long net_read(int slot, void *buf, long max);	/* 0: nothing yet; < 0: closed */
void net_close(int slot);
void net_close_wait(int slot, int seconds);

#endif

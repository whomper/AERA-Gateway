/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pigate.h - a PDF converted by the Pi (pdfgate, AERA-Gateway's pdf
 * service) instead of by the Atari: EConvert's Pi mode and EPDF's. The
 * exchange is docs/PI-GATEWAY.md: POST /convert, 102 Processing with
 * X-Progress while the Pi works, the converted PDF back.
 *
 * The PDF is read from disk and the result written to disk a piece at a
 * time (nothing held whole: an ST with 1 MB can do it). The waits are
 * conn.c's, which give pf_idle its turns; conn_stop (Esc, Stop) ends
 * them. Nothing here runs unless the caller asks for the Pi.
 */
#ifndef PIGATE_H
#define PIGATE_H

/* what the Pi is asked for: EConvert's settings (CVOPT's), as the query */
typedef struct {
	const char *pages;		/* "none", "mono", "colour"; NULL: the Pi's default */
	const char *photos;		/* "indexed", "jpeg"; NULL */
	const char *pictures;		/* "page", "screen"; NULL */
	int page_w, pic_w;		/* 0: the Pi's default */
	int page_double;		/* 1, 0; -1: the Pi's default */
} PGOPT;

/* where the job is, for the progress bar */
enum { PG_SEND, PG_WORK, PG_RECV };

/* progress: called between pieces with the phase and how far it is (done
   of total; bytes, or for PG_WORK thousandths of the file's pages); 1
   stops the job */
typedef int (*pg_progress)(void *u, int phase, long done, long total);

/* in converted by the Gateway at host:port into out. 0 done; -1 failed
   (err: the Gateway's own "pdfgate: ..." line, or what the network
   said); -2 stopped (conn_stop or progress); -3 the Gateway didn't
   answer (no connection: err says why). out is removed unless whole */
int pg_convert(const char *host, unsigned short port, const PGOPT *o, const char *in, const char *out,
	       pg_progress progress, void *u, char *err, int errlen);

/* the query for o and the PDF's name (its own, for the result's AERA
   entry: characters pdfgate doesn't take become '_'), into q; its length */
int pg_query(const PGOPT *o, const char *name, char *q, int qlen);

#endif

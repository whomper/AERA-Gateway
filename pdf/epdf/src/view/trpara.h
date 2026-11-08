/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * trpara.h - a page's text as EPDF stores it (Atari characters, a soft
 * break CR, a line's end LF, a picture's line "\x7F..."), cut into the
 * paragraphs a translation is asked for, and the translation put back
 * (Erez, 8 Nov 2026; docs/CONVERTED-PDF.md, "The translation"). The
 * same for EConvert's aerapdf on the Gateway and EPDF's own requests.
 */
#ifndef TRPARA_H
#define TRPARA_H

/* a piece of the page: [a, b) of its text, sent (a paragraph) or kept
   as it is (a picture's line, an empty line, a table's line with a Tab) */
typedef struct {
	long a, b;
	int send;
} TPSEG;

typedef struct {
	TPSEG *seg;
	int nseg, nsend;
	char *utf8;			/* the paragraphs to send, UTF-8, an empty line between */
	long ulen;
} TPARA;

/* text[0..n) cut up: 0, or -1 (memory). nsend 0: nothing to translate */
int tp_split(const char *text, long n, TPARA *p);
/* the translation (UTF-8, the same paragraphs in the same order, an empty
   line between) put in their places: a new text in Atari characters,
   *outlen its bytes; NULL when its paragraphs don't match (or memory) */
char *tp_join(const char *text, long n, const TPARA *p, const char *utf8, long ulen, long *outlen);
void tp_free(TPARA *p);

#endif

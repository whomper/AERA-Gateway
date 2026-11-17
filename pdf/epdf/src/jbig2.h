/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * jbig2.h - JBIG2Decode (ITU-T T.88, as a PDF embeds it: one page's
 * segments, and the JBIG2Globals stream's): the generic region (the
 * arithmetic coder, and MMR through ccitt.c), symbol dictionaries, text
 * regions, generic refinement. Out comes the page, a bit a pixel, 1 for
 * black (JBIG2's own sense; a PDF's image takes 1 as black, as an
 * ImageMask's 1 as not painted - the caller turns it).
 */
#ifndef JBIG2_H
#define JBIG2_H

#include <stdint.h>

typedef struct {
	int w, h;
	long stride;			/* bytes a row */
	uint8_t *d;
} JB2BM;

enum { JB2_OK, JB2_MEMORY, JB2_DAMAGED, JB2_UNSUPPORTED, JB2_STOPPED };

/* the page of data (n bytes) with the globals' segments (gn bytes, or none):
   0 and *page filled (free it with jb2_bm_free), or a JB2_ reason. poll,
   if any, is asked every few rows: 0 stops the decoding (JB2_STOPPED) */
int jb2_decode(const uint8_t *globals, long gn, const uint8_t *data, long n, JB2BM *page, int (*poll)(void));
void jb2_bm_free(JB2BM *b);

/* the same page a row at a time (a page of generic and text regions never
   whole in memory: a generic region's three rows, a text region's
   symbols and where each goes): NULL and *err, or the decoder; its size;
   the next row ((w + 7) / 8 bytes) 1, or 0 at the end or when stopped or
   damaged (jb2_error) */
typedef struct JB2 JB2;
JB2 *jb2_open(const uint8_t *globals, long gn, const uint8_t *data, long n, int (*poll)(void), int *err);
void jb2_size(const JB2 *j, int *w, int *h);
int jb2_row(JB2 *j, uint8_t *row);
int jb2_error(const JB2 *j);
void jb2_close(JB2 *j);

/* every page decoded whole first, the rows handed on from it (the tests:
   both ways the same) */
extern int jb2_whole_page;

/* if set, told the work before the page's rows come: done of of (its
   dictionaries' symbols and its text regions' placements, as they are
   met), often enough for a progress line */
extern void (*jb2_progress)(long done, long of);

#endif

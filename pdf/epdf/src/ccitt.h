/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * ccitt.h - CCITTFaxDecode: group 4 (K < 0) and group 3 one-dimensional (K = 0), a row at a
 * time. The rows come out as bits, 1 for black (a PDF's BlackIs1 flips them when it is false).
 */
#ifndef CCITT_H
#define CCITT_H

#include <stdint.h>

typedef struct {
	int cols, k, align;
	const uint8_t *in;
	long nbits, pos;
	uint16_t *ref, *cur;
	int nref, ncur;
	uint16_t *wtab, *btab;		/* 8192 entries: length << 12 | run */
	int err, eof;
	/* pulled: the coded bytes come from fill a piece at a time, into buf */
	long (*fill)(void *u, uint8_t *buf, long n);	/* bytes given, 0 at the end */
	void *fu;
	uint8_t *buf;
	long lim;			/* past this bit, buf is refilled */
} CCITT;

/* 0 or -1 (memory). The tables take 32 KB; they are the same for every picture (cc_tables) */
int ccitt_open(CCITT *c, const uint8_t *data, long n, int cols, int k, int byte_align);
/* the same with the coded bytes pulled from fill as the rows want them
   (4 KB at a time): a scan's data needn't be in memory whole */
int ccitt_open_pull(CCITT *c, long (*fill)(void *u, uint8_t *buf, long n), void *u, int cols, int k, int byte_align);
/* the next row into row ((cols + 7) / 8 bytes): 1, or 0 at the end or on damaged data */
int ccitt_row(CCITT *c, uint8_t *row);
void ccitt_close(CCITT *c);

#endif

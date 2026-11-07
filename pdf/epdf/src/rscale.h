/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * rscale.h - a black and white page (1 bit, 1 = black) shrunk by 2, 4 or 8 to grey, a row at a
 * time: each output byte is how much of its f x f block is ink, 255 for white paper. Byte tables
 * count the ink 8 pixels at a time, so a 68000 touches each source byte once.
 */
#ifndef RSCALE_H
#define RSCALE_H

#include <stdint.h>

typedef struct {
	int f, cols, outw, rows_in;
	uint16_t *acc;
} RSCALE;

int rs_init(RSCALE *r, int cols, int f);		/* 0, or -1 (memory or f not 2, 4 or 8) */
/* feed one source row; 1 when out (outw bytes) holds a finished row */
int rs_row(RSCALE *r, const uint8_t *row, uint8_t *out);
void rs_close(RSCALE *r);

#endif

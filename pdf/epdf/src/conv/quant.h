/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef QUANT_H
#define QUANT_H

#include <stdint.h>

/* the colours of px[0..count) (5-6-5) cut to at most ncol: their palette
   in pal (r g b) and, in map (65536 entries, or NULL), each colour that
   occurs its entry. The palette's size, 0 when memory is short */
int quant(const uint16_t *px, long count, int ncol, unsigned char (*pal)[3], unsigned char *map);
/* w x h pixels dithered to 1 bit a pixel (rows of (w + 7) / 8 bytes, 1 white) */
void dither_mono(const uint16_t *px, int w, int h, unsigned char *out);
/* dithered to the n colours of pal, 4 bits a pixel (rows of (w + 1) / 2 bytes) */
void dither_pal(const uint16_t *px, int w, int h, const unsigned char (*pal)[3], int n, unsigned char *out);

/* the same for rows ytop .. ytop + h - 1 of a page (px and out at its
   first): a band at a time gives the bytes the page whole gives */
void dither_mono_rows(const uint16_t *px, int w, int h, int ytop, unsigned char *out);
void dither_pal_rows(const uint16_t *px, int w, int h, int ytop, const unsigned char (*pal)[3], int n,
		     unsigned char *out);

#endif

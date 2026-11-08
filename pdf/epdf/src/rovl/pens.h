/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef PENS_H
#define PENS_H

/* rows of px (5-6-5, w a row; the page's row ytop first) to the n (2 to
   16) pens of pal, a pen a pixel into out (w a row): 0 ok, -1 no memory
   for its tables (64 KB, kept while the palette stays) */
int pens_rows(const unsigned short *px, int w, int rows, int ytop, const unsigned char (*pal)[3], int n,
	      unsigned char *out);
/* the same as planes rows: each row `planes` rows of (w + 15) / 16 words,
   pen bit p in plane p (the VDI's standard format's) */
int pens_planes(const unsigned short *px, int w, int rows, int ytop, const unsigned char (*pal)[3], int n,
		int planes, unsigned char *out);
void pens_free(void);

#endif

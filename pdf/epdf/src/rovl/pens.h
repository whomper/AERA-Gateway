/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef PENS_H
#define PENS_H

/* rows of px (5-6-5, w a row; the page's row ytop first) to the n (2 to
   256) pens of pal, a pen a pixel into out (w a row): 0 ok, -1 no memory
   for its tables (128 KB, kept while the palette stays) */
int pens_rows(const unsigned short *px, int w, int rows, int ytop, const unsigned char (*pal)[3], int n,
	      unsigned char *out);
/* the same as planes rows (1 to 8): each row `planes` rows of (w + 15) / 16 words,
   pen bit p in plane p (the VDI's standard format's) */
int pens_planes(const unsigned short *px, int w, int rows, int ytop, const unsigned char (*pal)[3], int n,
		int planes, unsigned char *out);
void pens_free(void);
/* penssoft.c: pens_planes with pens_soft sixteenths of each error passed on
   (16: all of it, pens_planes itself); pens_free_soft gives its tables back */
extern int pens_soft;
int pens_planes_soft(const unsigned short *px, int w, int rows, int ytop, const unsigned char (*pal)[3], int n,
		     int planes, unsigned char *out);
void pens_free_soft(void);
int pens_planes_share(const unsigned short *px, int w, int rows, int ytop, const unsigned char (*pal)[3], int n,
		      int planes, unsigned char *out);

#endif

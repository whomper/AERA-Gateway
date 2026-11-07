/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef PDFIMG_H
#define PDFIMG_H

#include "pdf.h"
#include "pic.h"

/* image XObject obj as a picture of w x h pixels; NULL and why (pic.h's
   PIC_*). max_bytes: the most its decoded stream may take */
PIC *pdf_image(PDOC *d, long obj, int w, int h, long max_bytes, int *why);

/* the same picture a row at a time, never whole in memory: row(u, y, px)
   for y = 0 .. h - 1 as they are made (a program shows them as they
   come), px w pixels 5-6-5, the same pixels as pdf_image's. 1 done; 0 not
   (why); -1 a picture this doesn't make (a progressive or CMYK JPEG,
   JPX, JBIG2): pdf_image */
typedef void (*PIMG_ROW)(void *u, long y, const unsigned short *px);
int pdf_image_rows(PDOC *d, long obj, int w, int h, PIMG_ROW row, void *u, long max_bytes, int *why);

/* a picture of a palette's colours (Indexed, or 1 bit of grey; Flate or
   stored): 1, its colours (r g b) in pal (256 x 3) and their number in *n;
   0 any other (pics.c: one whose colours all have a pen near them is
   shown without its dither, EConvert's whole pages) */
int pdf_image_palette(PDOC *d, long obj, unsigned char *pal, int *n);
/* its rows of samples as they are stored (packed, its own size), top to
   bottom: 1 done, 0 not (why) */
int pdf_image_raw_rows(PDOC *d, long obj, void (*row)(void *u, long y, const unsigned char *r), void *u,
		       long max_bytes, int *why);

#endif

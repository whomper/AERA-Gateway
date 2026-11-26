/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * penssoft.c - render.ovl's dither with softer colours (decided 22 Nov 2026,
 * as EWeb's: a FireBee or a 68040 or more, told by the host; a 68030
 * keeps the whole error): each pixel passes on pens_soft sixteenths of its
 * error. pens.c built again under other names, in a file of its own, so
 * that pens.c's code, the 68030's loop, stays byte for byte as it was (a
 * second copy of its loop in the same file changed what GCC made of it).
 */
#define pens_rows pens_rows_soft
#define pens_planes pens_planes_soft
#define pens_free pens_free_soft
#define PENS_SHARE 1
#include "pens.c"
#undef pens_rows
#undef pens_planes
#undef pens_free

int pens_soft = 16;

int pens_planes(const unsigned short *px, int w, int rows, int ytop, const unsigned char (*pal)[3], int n,
		int planes, unsigned char *out);	/* (pens.c's own: its header came in under the names above) */

int pens_planes_share(const unsigned short *px, int w, int rows, int ytop, const unsigned char (*pal)[3], int n,
		      int planes, unsigned char *out)
{
	return pens_soft < 16 ? pens_planes_soft(px, w, rows, ytop, pal, n, planes, out) :
				pens_planes(px, w, rows, ytop, pal, n, planes, out);
}

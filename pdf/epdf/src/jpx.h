/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * jpx.h - JPXDecode: a JPEG 2000 picture (ITU-T T.800, a JP2 file or its
 * bare code-stream, as PDFs carry it) decoded at a reduced resolution,
 * only as fine as it is shown: each reduction halves its width and height
 * and the code-blocks of the finer subbands aren't decoded at all. Rows of
 * 8-bit samples, grey or RGB, one component worked on at a time.
 */
#ifndef JPX_H
#define JPX_H

#include <stdint.h>

enum { JPX_OK, JPX_MEMORY, JPX_DAMAGED, JPX_UNSUPPORTED, JPX_STOPPED };

/* the picture's size reduced by reduce (as jpx_decode makes it: with an
   image offset not always its whole size's halved), its components and
   wavelet levels (the most it can be reduced by): 0, or a JPX_ reason */
int jpx_info(const uint8_t *d, long n, int reduce, int *w, int *h, int *comps, int *levels);

/* the picture reduced by reduce (0 .. levels: its size divided by
   2^reduce, rounded up): its rows top to bottom to row(u, y, px), px w
   pixels of 1 byte (grey) or 3 (RGB) as *comps says; *w, *h its size.
   poll, if any, is asked often: 0 stops it (JPX_STOPPED) */
typedef void (*JPX_ROW)(void *u, long y, const uint8_t *px);
int jpx_decode(const uint8_t *d, long n, int reduce, JPX_ROW row, void *u, int (*poll)(void), int *w, int *h, int *comps);

/* if set, told the decoding's progress before its rows come: done of of
   (its code-blocks), often enough for a progress line */
extern void (*jpx_progress)(long done, long of);

#endif

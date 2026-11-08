/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef CVJPEG_H
#define CVJPEG_H

#include "pic.h"
#include "pdf.h"

/* the JPEG d[0..n) as a picture of w x h pixels (any kind libjpeg reads);
   inverted: a CMYK one's /Decode is [1 0 1 0 1 0 1 0]. NULL and why */
PIC *cv_jpeg(const unsigned char *d, long n, int w, int h, int inverted, int *why);

/* 1: a JPEG of 1 or 3 components through common's pic_decode (on a
   Falcon its DSP, where pic_dsp is set: the page EPDF draws); 0 (as it
   starts): through cv_jpeg, the same pixels on every machine (EConvert's
   and the Gateway's converted files are the same byte for byte) */
extern int cv_jpeg_pic;

/* an image XObject as a picture of w x h (JPEGs through cv_jpeg) */
PIC *cv_image(PDOC *d, long obj, int w, int h, int *why);

#endif

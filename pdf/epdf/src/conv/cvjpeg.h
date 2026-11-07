/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef CVJPEG_H
#define CVJPEG_H

#include "pic.h"
#include "pdf.h"

/* the JPEG d[0..n) as a picture of w x h pixels (any kind libjpeg reads);
   inverted: a CMYK one's /Decode is [1 0 1 0 1 0 1 0]. NULL and why */
PIC *cv_jpeg(const unsigned char *d, long n, int w, int h, int inverted, int *why);

/* an image XObject as a picture of w x h (JPEGs through cv_jpeg) */
PIC *cv_image(PDOC *d, long obj, int w, int h, int *why);

#endif

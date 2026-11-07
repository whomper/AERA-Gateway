/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef JPEGENC_H
#define JPEGENC_H

#include <stdint.h>
#include "util.h"

/* w x h pixels (5-6-5) as a baseline JPEG added to out: grey (1) or
   colour (Y Cb Cr, 4:2:0); qual 1..100 (libjpeg's scale). 0 ok */
int jpeg_encode(const uint16_t *px, int w, int h, int grey, int qual, SBUF *out);

#endif

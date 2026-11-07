/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef DEFLATE_H
#define DEFLATE_H

#include "util.h"

/* d[0..n) as a zlib stream (FlateDecode's) added to out; 0 ok, -1 memory */
int zdeflate(const unsigned char *d, long n, SBUF *out);

#endif

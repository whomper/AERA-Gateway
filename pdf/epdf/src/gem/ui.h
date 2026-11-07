/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* what EPDF gives common/ui/gemui.h: the types and the text flags */
#ifndef EPDF_UI_H
#define EPDF_UI_H

typedef struct { short x, y, w, h; } GRECT;

#define KEY_SCAN(k)  (((unsigned short)(k)) >> 8)
#define KEY_ASCII(k) ((k) & 0xff)

#define TX_BOLD    1
#define TX_INVERSE 2
#define TX_LTR     4
#define TX_DIM     8

#endif

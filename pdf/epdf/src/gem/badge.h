/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* The "Optimised" badge's ground: a dark green rounded box, shaded from
 * its top down (badge.c). */
#ifndef BADGE_H
#define BADGE_H

void badge_pens(void);			/* once, after the workstation opens */
/* the box at x, y, w x h; answers the pen for the text's shadow, or -1
   for none (mono: the box is black, the text white on it) */
short badge_draw(short x, short y, short w, short h);

#endif

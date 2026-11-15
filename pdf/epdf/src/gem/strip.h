/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* The page strip (strip.c): a column of the pages' previews at the
   window's left (View > Page strip, PAGESTRIP= in the INF) */
#ifndef STRIP_H
#define STRIP_H
#include "gemui.h"
void strip_set(int on);
int strip_on(void);
short strip_width(short whole_w);	/* the column it takes from a work area that wide: 0 none */
/* its place in the window: the cells and their bar; redraw draws an area
   of the window over, through its rectangles */
int strip_layout(short x, short y, short w, short h, void (*redraw)(const GRECT *a));	/* 1 if it changed */
void strip_refresh(void);		/* what the cells show changed (View > Pages): all of them again, in turns */
void strip_draw(const GRECT *clip);
int strip_in(short mx, short my);	/* the point is in the strip (its cells or its bar) */
/* a press in it, the button still down: its bar's (pb_click), or a cell's
   (press_track): the page let go over, else -1 */
long strip_press(short mx, short my, void (*held)(short *msg));
/* the view's top page now page: its frame drawn at once, or the cells due
   when they move on */
void strip_follow(long page);
void strip_blank(void);			/* laid out again: its ground now, its cells due */
void strip_pic_ready(long obj);		/* a preview made: its cell due */
void strip_page_ready(long page);	/* a page read: its cell due */
/* the cells' parts due, drawn one a turn of the event loop after the
   text's (an 8 MHz ST's cell some 50 ms whole): 1 while some are due */
int strip_turn_due(void);
int strip_turn(void);			/* the next one drawn: 1, 0 none */
void strip_forget(void);		/* a document closed: from its first page again */
#endif

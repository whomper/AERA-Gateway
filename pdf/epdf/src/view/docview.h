/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * docview.h - the text view's model: every page of a document one after
 * another, each under a row of its own that names it, laid out for the
 * window's width. No GEM: the host tests it. The pages' text is in the
 * Atari character set, a paragraph's soft break a CR (the extractor's
 * '\v': the conversion keeps CR, LF and Tab only).
 *
 * The place shown is a page and a row in it, so it stays put while the
 * pages before it are extracted and their number of rows changes. A page
 * not extracted yet has one row ("..."), a guess until it is.
 *
 * A picture is a line of its own in the page's text, "\x7F<object>
 * <w>x<h>" (its size in points, the extractor's PTEXT.pics), shown as as
 * many rows as it takes at the window's width.
 *
 * In reflow mode a paragraph's lines are joined and wrapped to the width
 * (the soft break is a space); in lines mode each line
 * of the page is a row, wrapped where it is too long.
 */
#ifndef DOCVIEW_H
#define DOCVIEW_H

#include "tstore.h"
#include "textview.h"

#define DV_LAID 4			/* pages kept laid out */

enum { DV_FLIPV = 1, DV_FLIPH = 2 };	/* a picture shown upside down, mirrored (pdf.h's PIMG_V, PIMG_H) */
enum { DV_TEXT, DV_HEAD, DV_WAIT, DV_PIC };	/* a row of text, a page's head, a page still to come,
						   a row of a picture */

/* a picture's row (dv_row's DV_PIC): which picture, the size it shows at
   (pixels) and which of its rows this is */
typedef struct {
	long obj;			/* its image XObject */
	int w, h;			/* shown at w x h pixels */
	int flip;			/* DV_FLIPV: its rows bottom up, DV_FLIPH: right to left */
	long page;			/* the page it is on */
	long row, rows;			/* this row, of rows */
} DVPIC;

typedef struct {
	TSTORE *ts;
	long npages;
	int cols, lines_mode;
	long *rows;			/* each page's rows, its head included */
	char *stale;			/* rows[] a guess from another width, to count again */
	long nstale;
	long total;
	long top_page, top_row;		/* the first row shown */
	struct { long page; char *text, *mem; long len; TV tv; long *lrow; unsigned long used; } lay[DV_LAID];	/* mem: text's allocation */
	int pic_w, pic_ch, pic_aspect;	/* pictures: at most pic_w pixels wide, rows of pic_ch,
					   pixels pic_aspect / 16 as tall as wide */
	DVPIC pic;			/* the last DV_PIC row dv_row gave */
	unsigned long tick;
	/* the selection: from page p0, offset o0 in its text, to p1, o1
	   (o1 not taken); none while p0 < 0 */
	long sel_p0, sel_o0, sel_p1, sel_o1;
} DOCVIEW;

int dv_init(DOCVIEW *v, TSTORE *ts, long npages, int cols, int lines_mode);
void dv_free(DOCVIEW *v);
/* page n's text came (ts_put): its rows counted again */
void dv_page_ready(DOCVIEW *v, long n);
/* a new width or mode: the pages' rows guessed from the old width, then
   counted again by dv_recount, a few pages at a time */
void dv_set_cols(DOCVIEW *v, int cols, int lines_mode);
/* up to max pages counted again, those nearest the top first; 1 while
   more are to count */
int dv_recount(DOCVIEW *v, int max);
/* the pages a window of that many rows shows from the top counted again,
   the rest left to when they come into view; 1 while more are to count */
int dv_recount_view(DOCVIEW *v, long window);

/* the pictures' room: at most w pixels wide, rows ch pixels high, a pixel
   aspect / 16 as tall as it is wide (16 square, 32 an ST's medium mode);
   the pages laid out again as dv_set_cols does. Without it a picture is
   one row */
void dv_set_pics(DOCVIEW *v, int w, int ch, int aspect);
/* the row r rows after the top (r >= 0): its kind, text and length, the
   page it is in and where its text starts in the page's (-1 for a head or
   a page to come); 0 past the end */
int dv_row(DOCVIEW *v, long r, int *kind, const char **s, long *n, long *page, long *off);
/* the row of page n that holds offset off of its text (1: the first
   after its head), or 0 when the page isn't read */
long dv_row_of(DOCVIEW *v, long n, long off);
/* the top put so that page n's row `row` shows, `above` rows under the top
   where there is room */
void dv_show_row(DOCVIEW *v, long n, long row, long above, long window);
/* the selection: from page p0 offset o0 to p1 o1 (either way round); none
   with p0 < 0. dv_sel_cols: the columns [a, b) of a row (page, its text's
   start off, n characters) that are selected; 0 when none */
void dv_select(DOCVIEW *v, long p0, long o0, long p1, long o1);
int dv_sel_cols(const DOCVIEW *v, long page, long off, long n, long *a, long *b);
/* the first row shown, moved by d rows: down no further than a full
   window from the end (a page jump may have put a later page at the top:
   then not down at all); the rows it really moved */
long dv_scroll(DOCVIEW *v, long d, long window);
/* the top at page n's head */
void dv_goto_page(DOCVIEW *v, long n);
/* the first row shown counted from the start, and the last top there is */
long dv_top_abs(const DOCVIEW *v);
void dv_set_top_abs(DOCVIEW *v, long r, long window);
/* the page the top row is in */
long dv_page_at_top(const DOCVIEW *v);
/* page n's next picture from *pos in its stored text (0 to start): its
   object, flip and the size it shows at in *pic; 0 when none is left, the
   page has none or its text isn't in memory. Nothing is laid out (the
   pages laid out for the view stay), nothing read from the cache file */
int dv_page_pic(DOCVIEW *v, long n, long *pos, DVPIC *pic);

#endif

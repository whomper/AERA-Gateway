/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * The page strip (decided 15 Nov 2026; his #95: a column of page miniatures at the window's
 * left, View > Page strip, kept in the INF): a cell a page, the page's own
 * preview where it has one (its /Thumb: EConvert's and the Gateway's
 * converted copies carry one for every page, #97; on a Falcon or TT with
 * View > Pages the page drawn small by render.ovl), else a box with its
 * first line, its number under it. A click goes to the page; the page at
 * the view's top is framed twice, and the column follows it. Its own bar
 * (common's panebar) moves along the pages without moving the view.
 *
 * Shaped to move to common the day a second program asks (the hub, 15 Nov
 * 2026): the column below is cells, their count, the first one shown, a
 * draw callback, a click giving the cell, and the cells due; the pages'
 * part is after it (page_cell, its preview and words).
 *
 * Drawn only where it changed, and never in the text's stretch (the speed
 * rig, 15 Nov 2026: with the cells drawn with the page an 8 MHz ST's next
 * page went 190 -> 360 ms, a cell some 60 ms): the frame moved from one
 * cell to another is drawn at once, frames only (a few ms); cells that
 * changed (the column moved on, a page's first line or preview came) are
 * due, and strip_turn() draws one part of one in a turn of the event loop
 * of its own, after the text's (its ground, box, frame and number; then
 * its words or preview: some 30 ms each on an ST). The window's own redraw
 * draws what it uncovers at once, as ever.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "gemui.h"
#include "text.h"
#include "click.h"
#include "panebar.h"
#include "pics.h"
#include "docview.h"
#include "epdf.h"
#include "strip.h"
#include "bidi.h"

#define PIC_W 96			/* a preview's width (#97: EConvert's /Thumb) */
#define PAD 6				/* the box's place in its cell, across */
#define TOPGAP 4			/* above the box: the marked cell's second frame */

/* ---------------------------------------------------------------- the column */

typedef struct {
	GRECT area;			/* the column, the bar included (w 0: not shown) */
	long count;			/* cells */
	long top;			/* the first shown */
	long mark;			/* the one framed twice (-1 none) */
	short cell_h;
	PANEBAR bar;			/* at its right */
	/* cell i's parts (C_*) drawn in r; marked: the one in view */
	void (*draw)(long i, const GRECT *r, int marked, int parts);
	void (*redraw)(const GRECT *a);	/* the window draws a over (its rectangles) */
	/* due: the shown cells (bit i - top) whose parts are to draw in turns
	   of their own (strip_turn), and the bar */
	unsigned long due_a, due_b;
	unsigned char line[32];		/* a due inside's next line of words (0: its first, or its picture) */
	int due_bar;			/* the bar's pieces due: B_* */
	GRECT moved;			/* B_MOVED's: its slider's old and new places (pb_moved) */
} COLUMN;

/* the bar's pieces, a turn each (the bar whole by objc_draw, 100 ms of an
   8 MHz ST): its top arrow, its track, its foot arrow, and where its
   slider moved */
#define B_TOP		1
#define B_TRACK		2
#define B_FOOT		4
#define B_MOVED		8

/* a cell's parts: its white ground; its box, frames and number; what is
   in its box; its outer frame alone (the mark moved) */
#define C_GROUND	1
#define C_FRAME		2
#define C_INSIDE	4
#define C_MARK		8
#define C_ALL		(C_GROUND | C_FRAME | C_INSIDE)

static COLUMN col;
static int col_parts = C_ALL;		/* what col_draw draws (the redraw callback's) */
static int col_line = -1;		/* C_INSIDE: the line of words to draw (-1 all); */
static int col_more;			/* set: more lines after it */

static short cells_w(void)
{
	return (short)(col.area.w - col.bar.r.w);
}

static long col_cells(void)
{
	return col.area.h > 0 && col.cell_h > 0 ? (col.area.h + col.cell_h - 1) / col.cell_h : 0;	/* (the last maybe in part) */
}

static long col_whole(void)
{
	long n = col.area.h > 0 && col.cell_h > 0 ? col.area.h / col.cell_h : 0;

	return n > 0 ? n : 1;
}

static void col_rect(long i, GRECT *r)
{
	r->x = col.area.x;
	r->y = (short)(col.area.y + (i - col.top) * col.cell_h);
	r->w = (short)(cells_w() - 1);	/* (the line between the cells and the bar) */
	r->h = col.cell_h;
}

static void col_bar(void)
{
	col.bar.r.w = pb_width();
	col.bar.r.x = (short)(col.area.x + col.area.w - col.bar.r.w);
	col.bar.r.y = col.area.y;
	col.bar.r.h = col.area.h;
	col.bar.total = col.count;
	col.bar.shown = col_whole();
	col.bar.top = col.top;
	col.bar.across = 0;
}

static void col_top(long t)
{
	long most = col.count - col_whole();

	if (t > most)
		t = most;
	col.top = t < 0 ? 0 : t;
	col.bar.top = col.top;
}

static short fill_pen = -1;		/* the fill's pen as set (-1: its attributes to set) */

static void fill_rect(short x0, short y0, short x1, short y1, short colr)
{
	short p[4];

	if (fill_pen < 0) {
		vsf_interior(vdi_h, 1);
		vsf_perimeter(vdi_h, 0);
	}
	if (colr != fill_pen)
		vsf_color(vdi_h, colr);
	fill_pen = colr;
	vswr_mode(vdi_h, 1);
	p[0] = x0;
	p[1] = y0;
	p[2] = x1;
	p[3] = y1;
	vr_recfl(vdi_h, p);
}

static void col_draw(const GRECT *c)
{
	short p[4];
	long i, n = col_cells();
	short x1 = (short)(col.area.x + cells_w() - 1);

	if (col.area.w <= 0)
		return;
	p[0] = c->x;
	p[1] = c->y;
	p[2] = (short)(c->x + c->w - 1);
	p[3] = (short)(c->y + c->h - 1);
	fill_pen = -1;
	vsl_color(vdi_h, 1);
	for (i = 0; i < n; i++) {
		GRECT r;
		int inside;

		col_rect(col.top + i, &r);
		if (r.y + r.h <= c->y || r.y >= c->y + c->h || r.x + r.w <= c->x || r.x >= c->x + c->w)
			continue;
		/* a cell wholly inside c drawn with no clip (TOS's VDI is slow
		   with one round text); one drawn whole is no longer due */
		inside = r.x >= c->x && r.y >= c->y && r.x + r.w <= c->x + c->w && r.y + r.h <= c->y + c->h;
		if (inside) {
			vs_clip(vdi_h, 0, p);
			if (col_parts == C_ALL && i < 32) {
				col.due_a &= ~(1UL << i);
				col.due_b &= ~(1UL << i);
			}
		} else
			vs_clip(vdi_h, 1, p);
		if (col_parts & C_GROUND)
			fill_rect(r.x, r.y, (short)(r.x + r.w - 1), (short)(r.y + r.h - 1), 0);
		if (col.top + i < col.count)
			col.draw(col.top + i, &r, col.top + i == col.mark, col_parts);
	}
	vs_clip(vdi_h, 1, p);
	if (col_parts & C_GROUND)
		fill_rect(x1, col.area.y, x1, (short)(col.area.y + col.area.h - 1), 1);	/* the line before the bar */
	vs_clip(vdi_h, 0, p);
	if (col_parts == C_ALL)
		pb_draw(&col.bar, c);
}

/* the cells shown, their parts p, due (C_GROUND: all of each; else what
   is in their boxes) */
static void col_all_due(void)
{
	long n = col_cells();
	unsigned long m = n >= 32 ? ~0UL : (1UL << n) - 1;

	col.due_a = m;
	col.due_b = m;
	memset(col.line, 0, sizeof col.line);
	col.due_bar = B_TOP | B_TRACK | B_FOOT;
}

/* cell i drawn again in parts: what is in its box (C_INSIDE), or all of it */
static void col_due(long i, int parts)
{
	long k = i - col.top;

	if (k < 0 || k >= col_cells() || k >= 32)
		return;
	if (parts & (C_GROUND | C_FRAME))
		col.due_a |= 1UL << k;
	col.due_b |= 1UL << k;
	col.line[k] = 0;
}

/* the cell at mx, my, -1 none (the bar's not a cell) */
static long col_hit(short mx, short my)
{
	long i;

	if (col.area.w <= 0 || mx < col.area.x || mx >= col.area.x + cells_w() - 1 || my < col.area.y ||
	    my >= col.area.y + col.area.h)
		return -1;
	i = col.top + (my - col.area.y) / col.cell_h;
	return i >= 0 && i < col.count ? i : -1;
}

static int col_scroll(long n)
{
	long was = col.top;

	col_top(col.top + n);
	if (col.top == was)
		return 0;
	/* the bar's slider at once (the user moves it); the cells in turns,
	   their ground first */
	{
		GRECT g = pb_moved(&col.bar, was);

		col.redraw(&g);
	}
	col_all_due();
	col.due_bar = 0;
	return 1;
}

static int col_goto(long t)
{
	return col_scroll(t - col.top);
}

/* ---------------------------------------------------------------- the pages */

static int on;
static short box_h, lab_h;		/* a page's box, the number's line under it */

/* the small font on the VDI only: told to text.c (font_small), its CPU
   path for the text view's font was set up again after each cell, and the
   text view's next drawing went the slow way (Select all on an 8 MHz ST 40
   -> 80 ms); font_back() puts the VDI's back, text.c's unchanged */
static void small_font(short *w, short *h)
{
	vst_font(vdi_h, 1);
	vst_height(vdi_h, 4, w, h);
}

static void say(short x, short y, const char *s)
{
	vswr_mode(vdi_h, 2);		/* (transparent: on the box's white) */
	v_gtext_n(vdi_h, x, y, s, (short)strlen(s));
}

/* the box's words: the page's first line, a word to a line where it
   doesn't fit (the small font); its number is under the box. Hebrew as
   the text view shows it (decided 24 Nov 2026): a line whose first
   strong letter is Hebrew is broken from its start as read, each piece
   shown right to left (common's bidi) and set at the box's right */
static void box_words(long page, short x, short y, short w, short h, short fw, short fh)
{
	const char *s = doc_page_first(page);
	int per = (w - 4) / fw, lines = (h - 4) / (fh + 1), k = 0, heb = 0, rtl = 0;
	short ty = (short)(y + 3);

	{
		size_t n = strlen(s);
		short m = (short)(n < BIDI_MAX ? n : BIDI_MAX);

		if (m > 0 && bidi_has_rtl(s, m)) {
			heb = 1;
			rtl = bidi_is_rtl(s, m);
		}
	}

	col_more = 0;
	while (*s && k < lines && per > 0) {
		char ln[32];
		int n = (int)strlen(s), cut = n;

		if (n > per) {
			for (cut = per; cut > 0 && s[cut] != ' '; cut--)
				;
			if (cut == 0)
				cut = per;
		}
		if (cut > (int)sizeof ln - 1)
			cut = (int)sizeof ln - 1;
		memcpy(ln, s, (size_t)cut);
		ln[cut] = '\0';
		if (col_line < 0 || k == col_line) {
			short lx = (short)(x + 3);

			if (heb && cut > 0) {
				char vis[32];

				bidi_visual(ln, (short)cut, (short)rtl, vis);
				memcpy(ln, vis, (size_t)cut);
				if (rtl)	/* (trailing spaces aren't drawn: the words end at the right) */
					lx = (short)(x + w - 3 - cut * fw);
			}
			say(lx, (short)(ty + k * (fh + 1)), ln);
		}
		s += cut;
		while (*s == ' ')
			s++;
		k++;
		if (k > col_line && col_line >= 0) {
			col_more = *s && k < lines;	/* (one line a turn: some 8 ms of an ST's VDI each) */
			return;
		}
	}
}

/* the page's preview's picture and its size in the box: its /Thumb, or in
   View > Pages on a Falcon or TT the page drawn small by render.ovl; -1
   none */
static long preview_obj(long page, short *pw, short *ph)
{
	long obj = doc_page_thumb(page), tw, th;

	if (obj < 0 && doc_drawn() && doc_pages_mode()) {
		obj = PICS_PAGE + page;
		tw = 1000;
		th = 1414;
	} else if (obj < 0 || !doc_thumb_size(page, &tw, &th) || tw <= 0 || th <= 0)
		return -1;
	*pw = PIC_W;
	*ph = (short)(PIC_W * th / tw * 16 / (pix_aspect > 0 ? pix_aspect : 16));
	if (*ph > box_h) {
		*pw = (short)(*pw * (long)box_h / *ph);
		*ph = box_h;
	}
	return *pw >= 8 && *ph >= 8 ? obj : -1;
}

/* its picture, made (pics.c): NULL while it is being made, or none */
static PICE *preview(long page, short *pw, short *ph)
{
	long obj = preview_obj(page, pw, ph);
	PICE *e;

	if (obj < 0)
		return NULL;
	e = pics_get(obj, *pw, *ph, 0);
	return e && e->state == PICS_READY && e->bm.fd_addr ? e : NULL;
}

/* a frame, one pixel, in pen (one polyline: four fills each set their
   attributes, 25 ms of an 8 MHz ST's cell) */
static void box_line(short x0, short y0, short x1, short y1, short pen)
{
	short p[10];

	p[0] = x0;
	p[1] = y0;
	p[2] = x1;
	p[3] = y0;
	p[4] = x1;
	p[5] = y1;
	p[6] = x0;
	p[7] = y1;
	p[8] = x0;
	p[9] = y0;
	vsl_color(vdi_h, pen);
	vswr_mode(vdi_h, 1);
	v_pline(vdi_h, 5, p);
}

static void page_cell(long page, const GRECT *r, int marked, int parts)
{
	short bx = (short)(r->x + PAD), by = (short)(r->y + TOPGAP);
	short bx1 = (short)(bx + PIC_W + 1), by1 = (short)(by + box_h + 1), fw, fh, pw = 0, ph = 0;
	char num[12];
	PICE *e;

	if (parts & (C_FRAME | C_MARK))
		box_line((short)(bx - 2), (short)(by - 2), (short)(bx1 + 2), (short)(by1 + 2), marked ? 1 : 0);
	if (!(parts & (C_FRAME | C_INSIDE)))
		return;
	small_font(&fw, &fh);
	if (parts & C_INSIDE) {
		col_more = 0;
		e = col_line <= 0 ? preview(page, &pw, &ph) : NULL;
		if (e) {
			MFDB scr;
			short pxy[8], px = (short)(bx + 1 + (PIC_W - pw) / 2), py = (short)(by + 1 + (box_h - ph) / 2);

			memset(&scr, 0, sizeof scr);
			pxy[0] = 0;
			pxy[1] = 0;
			pxy[2] = (short)(pw - 1);
			pxy[3] = (short)(ph - 1);
			pxy[4] = px;
			pxy[5] = py;
			pxy[6] = (short)(px + pw - 1);
			pxy[7] = (short)(py + ph - 1);
			vro_cpyfm(vdi_h, 3, pxy, &e->bm, &scr);
		} else
			box_words(page, (short)(bx + 1), (short)(by + 1), PIC_W, box_h, fw, fh);
	}
	if (parts & C_FRAME) {
		box_line(bx, by, bx1, by1, 1);
		snprintf(num, sizeof num, "%ld", page + 1);
		say((short)(bx + (PIC_W - (short)strlen(num) * fw) / 2), (short)(by1 + 3), num);
	}
	font_back();
}

void strip_set(int o)
{
	on = o;
	col.mark = -1;			/* (framed again at the next move) */
}

int strip_on(void)
{
	return on;
}

short strip_width(short whole_w)
{
	/* the cells and the bar, a multiple of 16 (the text after it starts
	   on a word); not in a window too narrow for text beside it */
	short w;

	if (!on || !doc_is_open())
		return 0;		/* (before pb_width: its first call is four AES calls, 10 ms of an ST's start) */
	w = (short)((PIC_W + 2 * PAD + 1 + pb_width() + 15) & ~15);
	return whole_w >= w + 30 * 8 ? w : 0;
}

int strip_layout(short x, short y, short w, short h, void (*redraw)(const GRECT *a))
{
	short sw, sh;
	long count = doc_is_open() ? doc_pages() : 0;

	/* the same place, size and pages (View > The page's lines, a font):
	   nothing to draw again (its ground blanked each time, an 8 MHz ST's
	   View > The page's lines 225 -> 280 ms) */
	if (w > 0 && col.area.w == w && col.area.x == x && col.area.y == y && col.area.h == h && col.count == count &&
	    col.redraw == redraw)
		return 0;
	col.area.x = x;
	col.area.y = y;
	col.area.w = w;
	col.area.h = h;
	col.due_a = col.due_b = 0;
	col.due_bar = 0;
	if (w <= 0)
		return 1;		/* (off: no fonts asked, no bar laid: the speed rig's rows, 15 Nov 2026) */
	col.draw = page_cell;
	col.redraw = redraw;
	col.count = count;
	/* an A4 page's box, 96 wide, the pixels' shape taken in (half as
	   high in ST medium) */
	box_h = (short)(PIC_W * 1414L / 1000 * 16 / (pix_aspect > 0 ? pix_aspect : 16));
	small_font(&sw, &sh);
	font_back();
	lab_h = (short)(sh + 4);
	col.cell_h = (short)(TOPGAP + box_h + 2 + lab_h);
	col_bar();
	col_top(col.top);
	return 1;
}

/* what the cells show changed (View > Pages: render.ovl's pages for their
   first lines): all of them again, in turns */
void strip_refresh(void)
{
	if (col.area.w <= 0)
		return;
	col_all_due();
	col.due_bar = 0;
}

void strip_draw(const GRECT *c)
{
	col.count = doc_is_open() ? doc_pages() : 0;	/* (pages counted as the document opens) */
	col.bar.total = col.count;
	col_draw(c);
}

/* a press in the strip, its button still down: the bar's (pb_click: its
   arrows, track and slider), or a cell's, which goes to its page when it is
   let go over it (press_track, 5.8). The page, else -1 */
long strip_press(short mx, short my, void (*held)(short *msg))
{
	long i;
	GRECT r;

	if (col.area.w <= 0)
		return -1;
	if (mx >= col.bar.r.x && mx < col.bar.r.x + col.bar.r.w && my >= col.bar.r.y && my < col.bar.r.y + col.bar.r.h) {
		pb_click(&col.bar, mx, my, col_scroll, col_goto);
		return -1;
	}
	if ((i = col_hit(mx, my)) < 0)
		return -1;
	col_rect(i, &r);
	return press_track(&r, mx, my, 0, held, 0, 0) == PRESS_IN ? i : -1;
}

int strip_in(short mx, short my)
{
	return col.area.w > 0 && mx >= col.area.x && mx < col.area.x + col.area.w && my >= col.area.y &&
	       my < col.area.y + col.area.h;
}

/* the view's top page became page: the old frame and the new one drawn at
   once (frames only), or, when the cells move on, their ground at once and
   the cells due */
void strip_follow(long page)
{
	long old = col.mark, n = col_whole();
	GRECT a;

	if (col.area.w <= 0 || page == col.mark)
		return;
	col.mark = page;
	col.count = doc_pages();
	col.bar.total = col.count;
	if (page < col.top || page >= col.top + n) {
		long was = col.top;
		int bar = col.due_bar;

		col_top(page - (n > 2 ? 1 : 0));
		/* each cell cleared in its own first turn (blanked here, the
		   column took an ST's the start 145 -> 180 ms) */
		col_all_due();
		/* the bar: only where its slider moved, unless more was due */
		col.due_bar = bar;
		if (!(bar & B_TRACK)) {
			col.moved = pb_moved(&col.bar, was);
			col.due_bar |= B_MOVED;
		}
		return;
	}
	/* the old frame and the new in one redraw (the cells between get
	   theirs again: one rectangle list, not two) */
	col_parts = C_MARK;
	col_rect(page, &a);
	if (old >= col.top && old < col.top + col_cells()) {
		GRECT b;

		col_rect(old, &b);
		if (b.y < a.y) {
			a.h = (short)(a.y + a.h - b.y);
			a.y = b.y;
		} else
			a.h = (short)(b.y + b.h - a.y);
	}
	col.redraw(&a);
	col_parts = C_ALL;
}

/* the first due part: the cells' grounds, boxes, frames and numbers, then
   the bar, then what is in their boxes; one a turn. 1 if one was drawn */
int strip_turn(void)
{
	long k, n = col_cells();
	GRECT a;

	if (col.area.w <= 0) {
		col.due_a = col.due_b = 0;
		col.due_bar = 0;
		return 0;
	}
	for (k = 0; k < n && k < 32; k++)
		if (col.due_a & (1UL << k)) {
			col.due_a &= ~(1UL << k);
			col_rect(col.top + k, &a);
			col_parts = C_GROUND | C_FRAME;
			col.redraw(&a);
			col_parts = C_ALL;
			return 1;
		}
	if (col.due_bar) {
		/* a piece of the bar */
		short ah = pb_arrow_h();

		a = col.bar.r;
		if (col.due_bar & B_TOP) {
			col.due_bar &= ~B_TOP;
			a.h = ah;
		} else if (col.due_bar & B_FOOT) {
			col.due_bar &= ~B_FOOT;
			a.y = (short)(a.y + a.h - ah);
			a.h = ah;
		} else if (col.due_bar & B_TRACK) {
			col.due_bar &= ~(B_TRACK | B_MOVED);
			a.y = (short)(a.y + ah);
			a.h = (short)(a.h - 2 * ah);
		} else {
			col.due_bar = 0;
			a = col.moved;
		}
		if (a.h > 0)
			col.redraw(&a);
		return 1;
	}
	for (k = 0; k < n && k < 32; k++)
		if (col.due_b & (1UL << k)) {
			col_rect(col.top + k, &a);
			col_parts = C_INSIDE;
			col_line = col.line[k];
			col.redraw(&a);
			col_line = -1;
			col_parts = C_ALL;
			if (col_more && col.line[k] < 255)
				col.line[k]++;
			else
				col.due_b &= ~(1UL << k);
			return 1;
		}
	return 0;
}

int strip_turn_due(void)
{
	return col.area.w > 0 && (col.due_a || col.due_b || col.due_bar);
}

/* the column laid out again (the window opened, resized, a view changed):
   its ground at once, its cells in turns */
/* common's panebar fills its slider's moves with the VDI (pb_vdi: 30 ms
   less a move on an 8 MHz ST), once the strip is shown */
static void vdi_bars(void)
{
	static int set;

	if (!set) {
		pb_vdi(vdi_h);
		set = 1;
	}
}

void strip_blank(void)
{
	if (col.area.w <= 0)
		return;
	col_parts = C_GROUND;
	col.redraw(&col.area);
	col_parts = C_ALL;
	col_all_due();
	vdi_bars();
}

/* a preview made (pics.c's obj): its cell's inside due */
void strip_pic_ready(long obj)
{
	long i, n = col_cells();
	short pw, ph;

	if (col.area.w <= 0 || obj < 0)
		return;
	for (i = 0; i < n && col.top + i < col.count; i++)
		if (preview_obj(col.top + i, &pw, &ph) == obj)
			col_due(col.top + i, C_INSIDE);
}

/* a page read (its first line, its /Thumb known): its cell's inside due */
void strip_page_ready(long page)
{
	if (col.area.w > 0)
		col_due(page, C_INSIDE);
}

void strip_forget(void)
{
	col.top = 0;
	col.mark = -1;
	col.count = -1;			/* (the next document's laid out anew, the same pages or not) */
}

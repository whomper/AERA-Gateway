/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * textwin.c - the text view's window: the document's rows (docview.c)
 * drawn in the system font, a head row for each page.
 *
 * The guidelines' Speed: the work area sits on a multiple of 8 across
 * (wind_x8) with an 8-pixel margin, so text starts on a byte; only the
 * AES's rectangles are drawn; a scroll moves what stays with vro_cpyfm
 * and draws only the rows that come in; the sliders are set after a
 * scroll only when no key waits (each wind_set draws them again).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "gemui.h"
#include "text.h"
#include "iconify.h"
#include "docview.h"
#include "bidi.h"
#include "pic.h"
#include "click.h"
#include "trview.h"
#include "pics.h"
#include "enlarge.h"
#include "epdf.h"
#include "progbar.h"
#include "badge.h"

/* the speed build (make speed-test) times a scroll's parts */
#ifdef EPDF_TIMES
static unsigned long t_mark;
#define T_START() (t_mark = tos_ms())
#define T_MARK(what) do { char t_[24]; unsigned long n_ = tos_ms(); \
	snprintf(t_, sizeof t_, "%lu", n_ - t_mark); log_event("t", what, t_); t_mark = tos_ms(); } while (0)
#else
#define T_START() ((void)0)
#define T_MARK(what) ((void)0)
#endif

/* no info line: the status bar at the foot says what it said (as EWeb's) */
#define WKIND (NAME | CLOSER | FULLER | MOVER | SIZER | UPARROW | DNARROW | VSLIDE)
#define MARGIN 8

static short win = -1;
static int iconified;
static GRECT whole;			/* the work area: the text's and the status bar's */
static short bar_h;			/* the status bar's height, at the foot (0: too low a window) */
static GRECT work;			/* the text's area, as last laid out (whole, less the bar) */
static short cols, rows;		/* the text's columns, the rows that show whole */
static int sliders_due;
static char wname[96];
static char bar_base[112];		/* the status bar's first words: the page, and the status after it */
static void bar_compose(void);
static void bar_laid(void);
static void ahead_again(void);

short win_handle(void)
{
	return win;
}

int win_iconified(void)
{
	return iconified;
}

int win_rows(void)
{
	return rows;
}

int win_cols(void)
{
	return cols;
}

static void get_work(void)
{
	wind_get(win, WF_WORKXYWH, &whole.x, &whole.y, &whole.w, &whole.h);
	bar_h = (short)(ch + (ch >= 16 ? 7 : 5));	/* (EWeb's: a line, the words, the progress line) */
	if (whole.h < bar_h + 2 * ch)
		bar_h = 0;			/* too low a window for it */
	work = whole;
	work.h = (short)(work.h - bar_h);
	bar_laid();
	cols = (short)((work.w - 2 * MARGIN) / cw);
	if (cols < 1)
		cols = 1;
	rows = (short)(work.h / ch);
	if (rows < 1)
		rows = 1;
}

/* the window's place, its work area moved onto a multiple of 8 across */
static void place(short *x, short *y, short *w, short *h)
{
	short nx = wind_x8(WKIND, *x, *y, *w, *h);

	if (nx + *w > desk_x + desk_w)
		*w = (short)(desk_x + desk_w - nx);
	*x = nx;
	if (*w < 12 * 8)
		*w = 12 * 8;
	if (*h < 6 * 8)
		*h = 6 * 8;
	(void)y;
}

void win_title(void)
{
	if (win < 0)
		return;
	if (doc_is_open())
		snprintf(wname, sizeof wname, " EPDF - %s ", doc_name());
	else
		snprintf(wname, sizeof wname, " EPDF ");
	wind_set_str(win, WF_NAME, wname);
}

/* the status bar's first words: the page at the top, then a status */
static char status[64];				/* (after the page, until the next key) */

void win_status(const char *st)
{
	const char *now = st ? st : "";

	if (!strcmp(now, status))
		return;
	strncpy(status, now, sizeof status - 1);
	win_info();
	/* a status is the answer to a key (Copy's "copying...", Find's
	   "finding..."): drawn at once, the letters that changed, so the key's
	   own turn shows it (the speed rig's Copy was the copy's first slice,
	   70 ms on an ST, when it waited for a turn of its own) */
	if (win_progress_due())
		win_progress_draw();
}

void win_info(void)
{
	DOCVIEW *v = doc_view();
	static long was_page = -2, was_pages = -2;
	static int was_opening = -1;
	static char was_status[sizeof status] = "\377";
	long p = v ? dv_page_at_top(v) : -1;

	if (win < 0)
		return;
	/* (a line scrolled asks each time: the words made again only when
	   they change) */
	if (p == was_page && doc_pages() == was_pages && doc_opening() == was_opening && !strcmp(status, was_status) && bar_base[0])
		return;
	was_page = p;
	was_pages = doc_pages();
	was_opening = doc_opening();
	strcpy(was_status, status);
	if (!v && doc_opening())
		snprintf(bar_base, sizeof bar_base, "Opening %s...", doc_name());	/* (the open goes on in slices) */
	else if (!v)
		snprintf(bar_base, sizeof bar_base, "Open a PDF with File > Open (^O), or drop one here.");
	else
		snprintf(bar_base, sizeof bar_base, "Viewing page %ld / %ld%s%s", dv_page_at_top(v) + 1, doc_pages(),
			 status[0] ? " \xFA " : "", status);
	bar_compose();			/* (drawn in a turn of its own: win_progress_draw) */
}

/* what the sliders were set to: TOS 1.04's AES draws the slider at each
   wind_set, 40 ms on an ST, so each is set only when it changes (a line
   scrolled set both again: the 80 ms turn after the key) */
static short sl_size = -1, sl_pos = -1, sl_px = -1;	/* sl_px: the place in the bar's pixels */
extern short gl_hchar;

static void sliders(void)
{
	DOCVIEW *v = doc_view();
	long total = v ? v->total : 0, size = 1000, pos = 0;

	if (v && total > rows) {
		size = (long)rows * 1000 / total;
		pos = dv_top_abs(v) * 1000 / (total - rows);
		if (pos > 1000)
			pos = 1000;
	}
	if (size < 1)
		size = 1;
	if (size != sl_size)
		wind_set(win, WF_VSLSIZE, (short)size, 0, 0, 0);
	{
		/* the place set only when the slider would move a pixel (or reach
		   an end): the bar is the work area's height less its two arrows,
		   the slider at least a cell high. A long book's line moves it
		   less than a pixel: no wind_set, 36 ms on TOS 1.04, after most */
		long track = work.h - 2L * (gl_hchar + 3), sl, px;

		if (track < 1)
			track = 1;
		sl = size * track / 1000;
		if (sl < gl_hchar)
			sl = gl_hchar;
		px = track > sl ? pos * (track - sl) / 1000 : 0;
		if (pos != sl_pos && (size != sl_size || pos == 0 || pos == 1000 || px != sl_px || sl_pos < 0)) {
			wind_set(win, WF_VSLIDE, (short)pos, 0, 0, 0);
			sl_pos = (short)pos;
			sl_px = (short)px;
		}
		sl_size = (short)size;
	}
	sliders_due = 0;
}

int win_sliders_due(void)
{
	return win >= 0 && sliders_due;
}

void win_sliders(void)
{
	if (win >= 0 && !iconified)
		sliders();
}

/* the rows from the top laid out for the window's width */
void win_layout(void)
{
	DOCVIEW *v = doc_view();

	if (win < 0 || iconified)
		return;
	get_work();
	if (v) {
		dv_set_cols(v, cols, settings_lines());
		dv_set_pics(v, cols * cw, ch, pix_aspect);
		dv_recount_view(v, rows + 1);	/* the pages in view at once, the rest as they come into view */
		dv_scroll(v, 0, rows);
	}
	/* the sliders in a turn of their own, as after a scroll: their
	   wind_set is 40 ms on an ST (View > The page's lines moved them:
	   EmuTOS 215 -> 260 ms in the turn, gemprof, 2 Nov 2026) */
	sliders_due = 1;
	win_info();
}

static int fill_set;

/* a page's head: a line across, "Page n" on it; a converted PDF's
   (EConvert's) with the badge "Optimised" at its right end, green on a
   screen with colours, black in mono (Erez, 31 Oct 2026); a dark green
   shaded from the top down, the text white with a shadow (Erez, 4 Nov) */
static void draw_head(short x, short y, short w, long page)
{
	static const char badge[] = "Optimised";
	char s[64];
	short n, my = (short)(y + ch / 2), tx, end = (short)(x + w - MARGIN - 1), bx = 0, bw = 0, bh = 0, sw = 0, sh = 0, small = 0;

	if (tr_on() && doc_tr_state(page) == TR_DONE && doc_tr_from(page)[0])
		/* the page shown translated: where it came from (Erez, 8 Nov 2026:
		   "Page 3 \xFA from Hebrew") */
		n = (short)snprintf(s, sizeof s, " Page %ld \xFA from %s ", page + 1, lang_name(doc_tr_from(page)));
	else
		n = (short)snprintf(s, sizeof s, " Page %ld ", page + 1);
	if (n > (short)sizeof s - 1)
		n = (short)sizeof s - 1;
	tx = (short)(x + MARGIN + 2 * cw);
	if (doc_prepared()) {
		/* the text centred in the system's small font, 3 pixels each side
		   and its shadow's pixel, the box 0.7 of the line high (Erez, 4
		   Nov: "the text is centered and the bounding box isn't too
		   wide"; 5 Nov: "smaller by 30%"): a 16-pixel font's badge 81 x
		   16 -> 61 x 11. An 8-pixel line (ST low and medium) keeps its
		   font and the box its height: the small font there is 3 pixels
		   of a monitor high, past reading */
		static short sw0, sh0;		/* (its cell, asked once) */

		small = ch >= 12;
		if (small && !sw0) {
			font_small(&sw0, &sh0);
			font_back();
		}
		sw = small ? sw0 : cw;
		sh = small ? sh0 : ch;
		bh = small ? (short)((ch * 7 + 5) / 10) : ch;
		if (bh < sh + 2)
			bh = (short)(sh + 2 < ch ? sh + 2 : ch);
		bw = (short)((sizeof badge - 1) * sw + (small ? 2 * 3 : 2 * (cw / 2)) + 1);
		bx = (short)(end + 1 - bw);
		if (bx > tx + (n + 2) * cw)
			end = (short)(bx - cw - 1);
		else
			bw = 0;			/* (no room beside "Page n") */
	}
	line_col(x + MARGIN, my, tx - 1, my, 1);
	line_col(tx + n * cw, my, end, my, 1);
	vswr_mode(vdi_h, MD_TRANS);
	vst_color(vdi_h, 1);
	text_n(vdi_h, tx, y, s, n);
	if (bw) {
		short by = (short)(y + (ch - bh) / 2), ty, tx2, shadow;

		shadow = badge_draw(bx, by, bw, bh);	/* (badge.c: shaded green, black in mono) */
		fill_set = 0;
		if (small)
			font_small(&sw, &sh);
		ty = (short)(by + (bh - sh - (shadow >= 0)) / 2);
		tx2 = (short)(bx + (bw - 1 - (sizeof badge - 1) * sw) / 2);
		vswr_mode(vdi_h, MD_TRANS);
		if (shadow >= 0) {
			vst_color(vdi_h, shadow);
			text_n(vdi_h, (short)(tx2 + 1), (short)(ty + 1), badge, (short)(sizeof badge - 1));
		}
		vst_color(vdi_h, 0);
		text_n(vdi_h, tx2, ty, badge, (short)(sizeof badge - 1));
		vst_color(vdi_h, 1);
		if (small)
			font_back();
	}
}

/* the fill's attributes are white, solid, MD_REPLACE: set once a draw,
   not five VDI calls a blank (draw_rows clears it, and so does whatever
   may change them between: a picture's frame, an inversion; fill_set is
   above) */

/* white over x1..x2-1 (screen x) of the row at y, cut to the clip c */
static void blank(const GRECT *c, short x1, short x2, short y, short h)
{
	short p[4];

	if (x1 < c->x)
		x1 = c->x;
	if (x2 > c->x + c->w)
		x2 = (short)(c->x + c->w);
	if (y < c->y) {
		h = (short)(h - (c->y - y));
		y = c->y;
	}
	if (y + h > c->y + c->h)
		h = (short)(c->y + c->h - y);
	if (x2 <= x1 || h <= 0)
		return;
	p[0] = x1;
	p[1] = y;
	p[2] = (short)(x2 - 1);
	p[3] = (short)(y + h - 1);
	if (scr_cpu_wins()) {
		/* TOS's screen on a 68000: pen 0 by the CPU (common's scr_fill,
		   cut to the clip); elsewhere (a 68030's TOS 4 fills as fast, NVDI,
		   GDOS, a card) the VDI's fill as before, set once (scr_fill's VDI
		   way would ask and set it each time) */
		scr_fill(vdi_h, p, 0);
		return;
	}
	if (!fill_set) {
		vswr_mode(vdi_h, MD_REPLACE);
		vsf_interior(vdi_h, FIS_SOLID);
		vsf_color(vdi_h, 0);
		vsf_perimeter(vdi_h, 0);
		fill_set = 1;
	}
	vr_recfl(vdi_h, p);
}

/*
 * A row as it shows: Hebrew in display order (common's bidi, only where
 * the row has Hebrew), a row whose first strong letter is Hebrew put at
 * the right. *vis is what to draw (s itself, or vbuf), *x where it starts;
 * pos[i] (when the row has Hebrew) the column logical letter i shows in.
 */
static char vbuf[BIDI_MAX];
static short vpos[BIDI_MAX];
static unsigned char vodd[BIDI_MAX];

/* a byte from 0xC2 on (where Hebrew is) in s's n: one compare a byte
   with dbhi (18 cycles), where bidi_has_rtl's two were 10 ms of a window
   of rows on an ST (gemprof.py, 1 Nov 2026); then bidi_has_rtl itself */
static int maybe_rtl(const char *s, short n)
{
	short f = 0;

	if (n <= 0)
		return 0;
	__asm__("subq.w #1,%1\n\t"
		"moveq #-63,%%d1\n"		/* 0xC1 */
		"1:\tcmp.b (%0)+,%%d1\n\t"
		"dbcs %1,1b\n\t"
		"scs %2"
		: "+a"(s), "+d"(n), "+d"(f)
		:
		: "d1", "cc");
	return f != 0;
}

static int row_visual(const char *s, long n, const char **vis, short *x)
{
	int rtl;

	*vis = s;
	*x = (short)(work.x + MARGIN);
	if (n <= 0 || n > BIDI_MAX || !maybe_rtl(s, (short)n) || !bidi_has_rtl(s, (short)n))
		return 0;
	rtl = bidi_is_rtl(s, (short)n);
	bidi_visual_map(s, (short)n, (short)rtl, vbuf, vpos, vodd);
	*vis = vbuf;
	if (rtl)
		*x = (short)(work.x + MARGIN + (cols - n) * cw);
	return 1;
}

/* where a picture of width w stands: in the middle of the text's width,
   on a word (a copy that starts inside one is slow on TOS 1.04) */
static short pic_x(int w)
{
	short x = (short)(work.x + MARGIN + (cols * cw - w) / 2);

	if (x < work.x + MARGIN)
		x = (short)(work.x + MARGIN);
	return (short)(x & ~15) < work.x + MARGIN ? x : (short)(x & ~15);
}

/* a row of a picture: its bitmap's band, or, until it is decoded, its
   frame with a word in it */
/* a picture shown with fewer pixels than it has: a click shows it whole
   (Erez, 5 Nov 2026) */
static int enlargeable(const PICE *e, const DVPIC *pr)
{
	long d, w, h;

	if (pr->w < 32 || pr->h < 24)
		return 0;
	if (doc_double_of(pr->obj, &d, &w, &h))
		return 1;		/* (a page at double size: View > Pages) */
	return e && e->sw > pr->w + 4;
}

/* the enlarge mark at (x, y), a watermark at a picture's top right
   (common's enlarge_mark), where the clip c allows */
static void enlarge_icon(const GRECT *c, short x, short y)
{
	if (x + ENLARGE_SIZE <= c->x || y + ENLARGE_SIZE <= c->y || x >= c->x + c->w || y >= c->y + c->h)
		return;
	enlarge_mark(vdi_h, x, y);
	fill_set = 0;
}

static void draw_pic(const GRECT *c, short y, const DVPIC *pr)
{
	PICE *e = pics_get(pr->obj, pr->w, pr->h, pr->flip);
	short x = pic_x(pr->w), right = (short)(c->x + c->w), tx = (short)(work.x + MARGIN);
	long y0 = pr->row * ch, y1 = y0 + ch > pr->h ? pr->h : y0 + ch;
	/* the rows made: all of it, or (made a row at a time) those so far,
	   from the top down */
	long made = e && e->bm.fd_addr ? (e->state == PICS_READY ? pr->h : e->ready) : 0, m1 = made < y1 ? made : y1;

	if (e)
		e->page = pr->page;
	blank(c, tx, x, y, ch);
	blank(c, (short)(x + pr->w), right, y, ch);
	if (m1 > y0) {
		MFDB scr;
		short pxy[8];

		memset(&scr, 0, sizeof scr);
		pxy[0] = 0;
		pxy[1] = (short)y0;
		pxy[2] = (short)(pr->w - 1);
		pxy[3] = (short)(m1 - 1);
		pxy[4] = x;
		pxy[5] = y;
		pxy[6] = (short)(x + pr->w - 1);
		pxy[7] = (short)(y + (m1 - y0) - 1);
		vro_cpyfm(vdi_h, 3, pxy, &e->bm, &scr);
		if (m1 < y1) {
			/* the rest of the row: still the frame's */
			short yy = (short)(y + (m1 - y0)), hh = (short)(y1 - m1);

			blank(c, x, (short)(x + pr->w), yy, hh);
			line_col(x, yy, x, (short)(yy + hh - 1), 1);
			line_col((short)(x + pr->w - 1), yy, (short)(x + pr->w - 1), (short)(yy + hh - 1), 1);
			if (pr->row == pr->rows - 1)
				line_col(x, (short)(yy + hh - 1), (short)(x + pr->w - 1), (short)(yy + hh - 1), 1);
		}
		if (y1 - y0 < ch)
			blank(c, x, (short)(x + pr->w), (short)(y + (y1 - y0)), (short)(ch - (y1 - y0)));
		if (y0 < 15 && enlargeable(e, pr))
			enlarge_icon(c, (short)(x + pr->w - 14), (short)(y - y0 + 3));
		vswr_mode(vdi_h, MD_REPLACE);
		vst_color(vdi_h, 1);
		return;
	}
	/* the frame, a part of it a row */
	blank(c, x, (short)(x + pr->w), y, ch);
	line_col(x, y, x, (short)(y + ch - 1), 1);
	line_col((short)(x + pr->w - 1), y, (short)(x + pr->w - 1), (short)(y + ch - 1), 1);
	if (pr->row == 0)
		line_col(x, y, (short)(x + pr->w - 1), y, 1);
	if (pr->row == pr->rows - 1)
		line_col(x, (short)(y + ch - 1), (short)(x + pr->w - 1), (short)(y + ch - 1), 1);
	if (pr->row == pr->rows / 2) {
		const char *say = !e ? "Picture" : e->state == PICS_FAILED ? (e->why == PIC_UNSUPPORTED ? "A picture EPDF can't show" :
			e->why == PIC_MEMORY || e->why == PIC_TOO_BIG ? "A picture too big for the memory" : "A damaged picture") : "Picture";
		short n = (short)strlen(say);

		if (n * cw < pr->w - 2 * cw) {
			vswr_mode(vdi_h, MD_TRANS);
			vst_color(vdi_h, 1);
			vst_effects(vdi_h, 2);
			text_n(vdi_h, (short)(x + (pr->w - n * cw) / 2), y, say, n);
			vst_effects(vdi_h, 0);
		}
	}
	vswr_mode(vdi_h, MD_REPLACE);
	vst_color(vdi_h, 1);
}

/* the selected part of a row shown inverted (XOR), within the clip c */
static void invert(const GRECT *c, short x1, short x2, short y)
{
	short p[4];

	if (x1 < c->x)
		x1 = c->x;
	if (x2 > c->x + c->w)
		x2 = (short)(c->x + c->w);
	if (x2 <= x1)
		return;
	p[0] = x1;
	p[1] = y < c->y ? c->y : y;
	p[2] = (short)(x2 - 1);
	p[3] = (short)(y + ch - 1 > c->y + c->h - 1 ? c->y + c->h - 1 : y + ch - 1);
	fill_set = 0;			/* the VDI's way leaves the fill black: set it again */
	scr_xor(vdi_h, p);		/* TOS's screen by the CPU, else MD_XOR's vr_recfl */
}

/*
 * A row's selection as it shows under the span (p0, o0)-(p1, o1), in order
 * (p0 < 0: none): 0 none; 1 its cells [a, b); 2 the whole row, every letter
 * of it selected, shown to the text's right edge (Erez, 31 Oct 2026), so a
 * run of such rows is one box to invert (Select all on an ST).
 */
static int sel_shape(long p0, long o0, long p1, long o1, long page, long off, long n, long *a, long *b)
{
	long s, e;

	if (p0 < 0 || off < 0 || n <= 0 || page < p0 || page > p1)
		return 0;
	s = page == p0 ? o0 - off : 0;
	e = page == p1 ? o1 - off : n;
	if (s < 0)
		s = 0;
	if (e > n)
		e = n;
	if (e <= s)
		return 0;
	*a = s;
	*b = e;
	return s == 0 && e == n ? 2 : 1;
}

/* a row's cells [a, b) inverted: in a row of Hebrew letter by letter (it runs back) */
static void cells_xor(const GRECT *c, const char *s, long n, long a, long b, short y)
{
	const char *vis;
	short x;
	long i;

	if (!row_visual(s, n, &vis, &x))
		invert(c, (short)(x + a * cw), (short)(x + b * cw), y);
	else
		for (i = a; i < b; i++)
			invert(c, (short)(x + vpos[i] * cw), (short)(x + (vpos[i] + 1) * cw), y);
}

/* whole rows y0..y1 (screen) inverted from the text's left edge to its right: one box */
static void rows_xor(const GRECT *c, short y0, short y1)
{
	short p[4], x1 = (short)(work.x + MARGIN), x2 = (short)(work.x + MARGIN + cols * cw);

	if (x1 < c->x)
		x1 = c->x;
	if (x2 > c->x + c->w)
		x2 = (short)(c->x + c->w);
	if (y0 < c->y)
		y0 = c->y;
	if (y1 > c->y + c->h - 1)
		y1 = (short)(c->y + c->h - 1);
	if (x2 <= x1 || y1 < y0)
		return;
	p[0] = x1;
	p[1] = y0;
	p[2] = (short)(x2 - 1);
	p[3] = y1;
	fill_set = 0;
	T_MARK("sel-shape");
	scr_xor(vdi_h, p);
	T_MARK("sel-xor");
}

/*
 * Rows [r0, r1) of the view, within clip c (the AES's rectangle). Text is
 * drawn in MD_REPLACE, which paints its own cells white: only what no
 * text covers is filled (the margin, a row's end, empty rows), not the
 * whole area first (a whole window's fill took EmuTOS 60 ms on an ST).
 */
/*
 * The left margin and the strip right of the text's columns are white
 * and nothing draws there; what is drawn again for new content in the
 * same layout (a scroll's rows, a page that came in, a page jump) needn't
 * clear them: edges_clean. A redraw the AES asks for, or one after the
 * layout changed (a font, a size, the page's lines), clears them. They
 * were most of a window's clearing on an ST, a narrow box a pixel row at
 * a time (box_op: 34 ms of a page scrolled, gemprof.py, 1 Nov 2026).
 */
static int edges_clean;

/* a row's text with its tail as spaces, drawn with it (a tail of up to
   PAD_MAX cells costs less than clearing it, a pixel row at a time) */
#define PAD_MAX 16
static char padded[512];

static void draw_rows(const GRECT *c, long r0, long r1)
{
	DOCVIEW *v = doc_view();
	short clip[4], tx = (short)(work.x + MARGIN), right = (short)(c->x + c->w);
	short text_right = (short)(tx + cols * cw);
	short y = (short)(work.y + r0 * ch), box_y0 = -1, box_y1 = 0;	/* whole selected rows to invert as one box */
	long r;

	clip[0] = c->x;
	clip[1] = c->y;
	clip[2] = (short)(c->x + c->w - 1);
	clip[3] = (short)(c->y + c->h - 1);
	text_clip(vdi_h, clip);
	T_START();
	fill_set = 0;				/* others may have changed the fill since */
	if (edges_clean) {
		if (right > text_right)
			right = text_right;	/* the strip after the columns is white */
	} else
		blank(c, c->x, tx, c->y, c->h);	/* the left margin */
	vswr_mode(vdi_h, MD_REPLACE);		/* once for the rows of text */
	vst_color(vdi_h, 1);
	for (r = r0; v && r < r1; r++, y = (short)(y + ch)) {
		int kind;
		const char *s;
		long n, page, off;

		if (y + ch <= c->y || y >= c->y + c->h)
			continue;
		if (!dv_row(v, r, &kind, &s, &n, &page, &off))
			break;
		if (kind == DV_PIC) {
			draw_pic(c, y, &v->pic);
			continue;
		}
		if (n > cols)
			n = cols;
		if (kind == DV_HEAD || n <= 0 || kind == DV_WAIT) {
			blank(c, tx, right, y, ch);
			if (kind == DV_HEAD)
				draw_head(work.x, y, (short)(cols * cw + 2 * MARGIN), page);	/* to the text's right edge: nothing in the strip after it */
			else if (n > 0) {
				vswr_mode(vdi_h, MD_TRANS);
				vst_color(vdi_h, 1);
				vst_effects(vdi_h, 2);
				text_n(vdi_h, tx, y, s, n);
				vst_effects(vdi_h, 0);
			}
			vswr_mode(vdi_h, MD_REPLACE);	/* back for the rows of text */
			vst_color(vdi_h, 1);
			continue;
		}
		{
			const char *vis;
			short x;
			int bi = row_visual(s, n, &vis, &x), k;
			long a, b, i;

			if (x > tx)
				blank(c, tx, x, y, ch);
			{
				short end = (short)(x + n * cw), pad = end < right ? (short)((right - end + cw - 1) / cw) : 0;

				/* (spaces after the text where the CPU draws it, cheaper than
				   a fill; through the VDI, a GDOS font under NVDI, each is a
				   glyph to draw: the fill, the hub's ST:mono with Courier 14) */
				if (pad > 0 && pad <= PAD_MAX && n + pad <= (long)sizeof padded && text_plain_state() >= 0) {
					memcpy(padded, vis, (size_t)n);
					memset(padded + n, ' ', (size_t)pad);
					text_plain(vdi_h, x, y, padded, n + pad);	/* MD_REPLACE, pen 1, no effects: set above */
				} else {
					text_plain(vdi_h, x, y, vis, n);
					blank(c, end, right, y, ch);
				}
			}
			k = sel_shape(v->sel_p0, v->sel_o0, v->sel_p1, v->sel_o1, page, off, n, &a, &b);
			if (k == 2) {
				/* a whole row: with the rows of it just above, one box */
				if (box_y0 >= 0 && y != box_y1 + ch) {
					rows_xor(c, box_y0, (short)(box_y1 + ch - 1));
					box_y0 = -1;
				}
				if (box_y0 < 0)
					box_y0 = y;
				box_y1 = y;
			} else if (k == 1) {
				if (!bi)
					invert(c, (short)(x + a * cw), (short)(x + b * cw), y);
				else
					for (i = a; i < b; i++)	/* letter by letter: Hebrew runs back */
						invert(c, (short)(x + vpos[i] * cw), (short)(x + (vpos[i] + 1) * cw), y);
			}
		}
	}
	if (box_y0 >= 0)
		rows_xor(c, box_y0, (short)(box_y1 + ch - 1));
	/* below the last row */
	if (y < c->y + c->h)
		blank(c, tx, right, y, (short)(c->y + c->h - y));
	T_MARK("rows");
	text_noclip(vdi_h);
}

static int overlap(const GRECT *a, const GRECT *b, GRECT *o)
{
	short x1 = a->x > b->x ? a->x : b->x, y1 = a->y > b->y ? a->y : b->y;
	short x2 = a->x + a->w < b->x + b->w ? a->x + a->w : b->x + b->w;
	short y2 = a->y + a->h < b->y + b->h ? a->y + a->h : b->y + b->h;

	if (x2 <= x1 || y2 <= y1)
		return 0;
	o->x = x1;
	o->y = y1;
	o->w = (short)(x2 - x1);
	o->h = (short)(y2 - y1);
	return 1;
}


/*
 * The status bar (Erez, 2 Nov 2026: "Like EWeb, placed at the bottom,
 * says viewing page x / y and then if there is activity going on, such as
 * opening a page, picture of whatever, it is stated after that text.
 * Progress is marked with a thin line at the bottom of that text area that
 * is filled from left to right"): at the foot of the window and always
 * there, as EWeb's (the window has no info line). A line along its top,
 * its words ("Viewing page 3 / 8 * Reading page 5 of 8 * 30% * 2 min
 * left"), and along its foot a line 2 pixels high (3 in a tall font) that
 * fills from the left as the job goes on. The job is the one the user
 * waits on (epdf.c's progress()); its words come once it has run half a
 * second and go a third of a second after it ends.
 *
 * Drawn in turns of its own, at most every 250 ms while a job runs (the
 * guidelines' progress), and then only what changed, each pixel written
 * once (no flicker: Erez, 1 Nov 2026, "the progress displays flicker
 * every second"): the letters that changed by text_plain on the white
 * ground, the line's new part.
 */
#define BAR_CELLS 160
static char bar_now[BAR_CELLS + 1], bar_want[BAR_CELLS + 1];	/* its words as drawn, as wanted, padded */
static short bar_rib_now, bar_rib_want;	/* the bar's part filled, in PB_FULLths (progbar.h) */
static int bar_due;			/* something to draw */
static char bar_job[112];		/* the job's words, "" none */
static long bar_p = -1;			/* the job's thousandths, -1 not known */
static int job_kind, job_on;		/* the job running; its words shown */
static unsigned long job_t0, job_since, idle_since, bar_drawn, eta_at;
static long job_p0, eta_s = -1;

static short bar_y(void) { return (short)(whole.y + whole.h - bar_h); }
static short bar_tx(void) { return (short)(whole.x + MARGIN); }
static short bar_rib(void) { return ch >= 16 ? 3 : 2; }
static short bar_cells(void)
{
	short n = (short)((whole.w - 2 * MARGIN) / cw);

	return n < 0 ? 0 : n > BAR_CELLS ? BAR_CELLS : n;
}

/* a box [x1, x2) x [y, y + h) in pen 1, cut to c */
static void bar_ink(const GRECT *c, short x1, short x2, short y, short h)
{
	short p[4];

	if (x1 < c->x)
		x1 = c->x;
	if (x2 > c->x + c->w)
		x2 = (short)(c->x + c->w);
	if (y < c->y) {
		h = (short)(h - (c->y - y));
		y = c->y;
	}
	if (y + h > c->y + c->h)
		h = (short)(c->y + c->h - y);
	if (x2 <= x1 || h <= 0)
		return;
	p[0] = x1;
	p[1] = y;
	p[2] = (short)(x2 - 1);
	p[3] = (short)(y + h - 1);
	fill_set = 0;
	if (scr_cpu_wins()) {
		scr_fill(vdi_h, p, 1);
		return;
	}
	vswr_mode(vdi_h, MD_REPLACE);
	vsf_interior(vdi_h, FIS_SOLID);
	vsf_color(vdi_h, 1);
	vsf_perimeter(vdi_h, 0);
	vr_recfl(vdi_h, p);
}

/* the progress bar (common's progbar, the guidelines 5.9) along the
   status bar's foot, from..to in PB_FULLths, in c (the VDI's clip, set by
   the caller); its ground (white) from..to where it shrank */
static short bar_x0(void) { return (short)(whole.x + 1); }
static short bar_w(void) { return (short)(whole.w - 2); }
static void bar_fill(const GRECT *c, short from, short to)
{
	(void)c;
	progbar_fill(vdi_h, bar_x0(), (short)(bar_y() + bar_h - bar_rib()), bar_w(), bar_rib(), from, to);
	fill_set = 0;
}
static void bar_unfill(const GRECT *c, short from, short to)
{
	short w = bar_w();

	fill_set = 0;
	blank(c, (short)(bar_x0() + (long)w * from / PB_FULL), (short)(bar_x0() + (long)w * to / PB_FULL),
	      (short)(bar_y() + bar_h - bar_rib()), bar_rib());
}

/* its words and its line as they should be */
static void bar_compose(void)
{
	short nb = bar_cells(), rib;
	char w[BAR_CELLS + 1];
	size_t k;

	if (job_on && bar_job[0])
		snprintf(w, sizeof w, "%s \xFA %s", bar_base, bar_job);
	else
		snprintf(w, sizeof w, "%s", bar_base);
	for (k = strlen(w); k < (size_t)nb; k++)
		w[k] = ' ';
	w[nb] = 0;
	rib = job_on && bar_p > 0 ? (short)(bar_p >= 1000 ? PB_FULL : bar_p * PB_FULL / 1000) : 0;
	if (memcmp(w, bar_want, (size_t)nb + 1) || rib != bar_rib_want) {
		memcpy(bar_want, w, (size_t)nb + 1);
		bar_rib_want = rib;
		bar_due = 1;
	}
}

/* the window laid out (opened, sized, its font changed): it is drawn
   whole next, the bar as it should be */
static void bar_laid(void)
{
	bar_compose();
	memcpy(bar_now, bar_want, sizeof bar_now);
	bar_rib_now = bar_rib_want;
	bar_due = 0;
}

/* the bar as it was last drawn (bar_now), whole, in c (an expose) */
static void draw_bar(const GRECT *c)
{
	short y = bar_y(), clip[4];

	clip[0] = c->x;
	clip[1] = c->y;
	clip[2] = (short)(c->x + c->w - 1);
	clip[3] = (short)(c->y + c->h - 1);
	text_clip(vdi_h, clip);
	fill_set = 0;
	blank(c, whole.x, (short)(whole.x + whole.w), y, bar_h);
	bar_ink(c, whole.x, (short)(whole.x + whole.w), y, 1);
	vswr_mode(vdi_h, MD_REPLACE);
	vst_color(vdi_h, 1);
	text_plain(vdi_h, bar_tx(), (short)(y + 2), bar_now, bar_cells());
	if (bar_rib_now > 0)
		bar_fill(c, 0, bar_rib_now);
	text_noclip(vdi_h);
}

/* from bar_now to bar_want in c: the letters that changed, the line's change */
static void draw_bar_change(const GRECT *c)
{
	short y = bar_y(), nb = bar_cells(), clip[4], i, j;

	clip[0] = c->x;
	clip[1] = c->y;
	clip[2] = (short)(c->x + c->w - 1);
	clip[3] = (short)(c->y + c->h - 1);
	text_clip(vdi_h, clip);
	vswr_mode(vdi_h, MD_REPLACE);
	vst_color(vdi_h, 1);
	for (i = 0; i < nb; i = j) {
		j = (short)(i + 1);
		if (bar_now[i] == bar_want[i])
			continue;
		while (j < nb && bar_now[j] != bar_want[j])
			j++;
		text_plain(vdi_h, (short)(bar_tx() + i * cw), (short)(y + 2), bar_want + i, j - i);
	}
	if (bar_rib_want > bar_rib_now)
		bar_fill(c, bar_rib_now, bar_rib_want);	/* only its new part (5.9, the Speed) */
	else if (bar_rib_want < bar_rib_now)
		bar_unfill(c, bar_rib_want, bar_rib_now);
	text_noclip(vdi_h);
}

/* the time left, said roughly: "20 s", "2 min", "3 h" */
static void eta_words(long s, char *o, size_t n)
{
	if (s < 60)
		snprintf(o, n, "%ld s left", s < 5 ? 5 : (s + 4) / 5 * 5);
	else if (s < 3600)
		snprintf(o, n, "%ld min left", (s + 30) / 60);
	else
		snprintf(o, n, "%ld h left", (s + 1800) / 3600);
}

void win_progress(int kind, const char *what, long done, long total)
{
	unsigned long now = tos_ms();
	long p = -1;
	char eta[24];

	if (win < 0 || iconified)
		return;
	if (!kind) {
		job_kind = 0;
		if (job_on) {
			if (!idle_since)
				idle_since = now;
			else if (now - idle_since >= 300) {
				job_on = 0;
				bar_job[0] = 0;
				bar_p = -1;
				bar_compose();
				log_event("progress", "gone", NULL);
			}
		}
		return;
	}
	idle_since = 0;
	if (total > 0) {
		while (total > 2000000L)		/* (1000 x it in a long) */
			done >>= 1, total >>= 1;
		p = done >= total ? 1000 : done < 0 ? 0 : done * 1000L / total;
	}
	if (kind != job_kind) {
		job_kind = kind;
		job_t0 = job_since = now;
		job_p0 = p;
		eta_s = -1;
		eta_at = 0;
	}
	if (!job_on) {
		if (now - job_since < 500)
			return;			/* a short job says nothing */
		job_on = 1;
		bar_drawn = 0;
		log_event("progress", "shown", NULL);
	}
	if (now - bar_drawn < 250)
		return;
	/* the time left: by how fast this job has gone since it began, said
	   again at most every second, from its second second */
	if (p > job_p0 && job_p0 >= 0 && now - job_t0 >= 2000 && now - eta_at >= 1000) {
		unsigned long el = (now - job_t0) / 100;	/* tenths of a second */

		eta_s = (long)(el * (unsigned long)(1000 - p) / (unsigned long)(p - job_p0) / 10);
		eta_at = now;
	}
	if (p < 0 || p >= 1000)
		eta_s = -1;
	eta[0] = 0;
	if (eta_s >= 0)
		eta_words(eta_s, eta, sizeof eta);
	if (p >= 0)
		snprintf(bar_job, sizeof bar_job, "%s \xFA %ld%%%s%s", what, p / 10, eta[0] ? " \xFA " : "", eta);
	else
		snprintf(bar_job, sizeof bar_job, "%s", what);
	bar_p = p;
	bar_compose();
}

int win_progress_due(void)
{
	return bar_due && win >= 0 && !iconified && bar_h;
}

int win_progress_shown(void)
{
	return job_on;
}

/* the pages the window shows, first and last (-1 none) */
void win_pages_shown(long *p0, long *p1)
{
	DOCVIEW *v = doc_view();
	int kind;
	const char *s;
	long n, off, r;

	*p0 = *p1 = -1;
	if (!v || win < 0)
		return;
	*p0 = dv_page_at_top(v);
	*p1 = *p0;
	for (r = rows; r >= 0; r--)
		if (dv_row(v, r, &kind, &s, &n, p1, &off))
			break;
}

long win_progress_wait(void)
{
	unsigned long now = tos_ms();

	if (win < 0 || iconified || !job_on || job_kind)
		return -1;
	if (!idle_since)
		return 0;
	return now - idle_since >= 300 ? 0 : (long)(300 - (now - idle_since));
}

void win_progress_draw(void)
{
	GRECT r, b, c;

	bar_due = 0;
	if (win < 0 || iconified || !bar_h)
		return;
	b = whole;
	b.y = bar_y();
	b.h = bar_h;
	wind_update(BEG_UPDATE);
	graf_mouse(M_OFF, NULL);
	wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	while (r.w > 0 && r.h > 0) {
		if (overlap(&r, &b, &c))
			draw_bar_change(&c);
		wind_get(win, WF_NEXTXYWH, &r.x, &r.y, &r.w, &r.h);
	}
	graf_mouse(M_ON, NULL);
	wind_update(END_UPDATE);
	memcpy(bar_now, bar_want, sizeof bar_now);
	bar_rib_now = bar_rib_want;
	bar_drawn = tos_ms();
}

/* the window's rectangles over area, each drawn */
/*
 * A picture whole (Erez, 5 Nov 2026: clicked, "it expands to full
 * glory"): in the one window (3.2), the picture alone at its own pixels,
 * at most the text column's width, made like any other (pics.c, its rows
 * as they come); Up, Down and the page keys move it, Esc, Backspace,
 * Return or a click give the text back as it was.
 */
void (*has_menu_states)(void);		/* (epdf.c's menu_states: Zoom in and out greyed or not) */

static struct {
	int on;
	DVPIC p;			/* the picture at the size shown whole */
	long top, left;			/* its first row and column in view */
	int pct;			/* a page of View > Pages zoomed: 150 to 400 (0: a picture) */
	DVPIC base;			/* that page's line as View > Pages shows it */
	/* a converted PDF's page: its own picture enlarged at once (made in a
	   second or so from what is stored), then the page drawn sharp at that
	   size (render.ovl, seconds to tens of seconds) in its place when it
	   is ready (Erez, 8 Nov 2026: "tens of seconds of wait time") */
	MFDB pre;			/* the page's picture as View > Pages shows it, enlarged */
	int pre_heap;
} zoom;

/* View > Zoom in and Zoom out's steps (Erez, 8 Nov 2026: keypad + and -,
   100 to 400 %; a click on a page still goes to 200 % and back) */
static const short zsteps[] = { 100, 150, 200, 300, 400 };
static int zoom_failed_at;		/* the size that couldn't be made (this document): not offered again */
#define NZSTEPS (int)(sizeof zsteps / sizeof zsteps[0])

static void zoom_pre_free(void)
{
	pics_enlarge_free(&zoom.pre, zoom.pre_heap);
}

int win_zoomed(void)
{
	return zoom.on;
}

void win_zoom_end(void)
{
	if (!zoom.on)
		return;
	zoom.on = 0;
	zoom.pct = 0;
	zoom_pre_free();
	doc_pic_stop_page_unless(zoom.base.obj, zoom.base.w, zoom.base.h);	/* (a zoomed page still drawing gives way) */
	if (has_menu_states)
		has_menu_states();
	win_draw_all();
}

static void draw_zoom(const GRECT *c)
{
	PICE *e = pics_get(zoom.p.obj, zoom.p.w, zoom.p.h, zoom.p.flip);
	const MFDB *from;
	short x = (short)(work.x + (work.w - zoom.p.w) / 2), y = work.y;
	long made = e && e->bm.fd_addr ? (e->state == PICS_READY ? zoom.p.h : e->ready) : 0;

	if (e && e->state == PICS_READY && zoom.pre.fd_addr) {
		zoom_pre_free();	/* (the sharp one is here) */
		log_event("zoom-sharp", NULL, NULL);
	}
	if (zoom.pre.fd_addr && (!e || e->state != PICS_READY)) {
		from = &zoom.pre;	/* the enlarged one till the sharp one is made */
		made = zoom.p.h;
	} else
		from = e ? &e->bm : NULL;
	long y1 = zoom.top + work.h < zoom.p.h ? zoom.top + work.h : zoom.p.h;
	short vw = (short)(zoom.p.w - zoom.left < work.w ? zoom.p.w - zoom.left : work.w);	/* its columns in view */
	long m1 = made < y1 ? made : y1;

	if (x < work.x)
		x = work.x;
	/* only what the picture doesn't cover blanked first: a drag moves it
	   without the window flashing white */
	blank(c, work.x, x, work.y, work.h);
	blank(c, (short)(x + vw), (short)(work.x + work.w), work.y, work.h);
	if (made > zoom.top)
		blank(c, x, (short)(x + vw), (short)(y + (m1 - zoom.top)), (short)(work.h - (m1 - zoom.top)));
	else
		blank(c, x, (short)(x + vw), work.y, work.h);
	if (e)
		e->page = zoom.p.page;
	if (made > zoom.top) {
		MFDB scr;
		short pxy[8];

		/* the part in c (the VDI's raster copy takes no clipping: a
		   window over this one, or a redraw of a piece of it) */
		short dx0 = x, dy0 = y, dx1 = (short)(x + vw - 1), dy1 = (short)(y + (m1 - zoom.top) - 1);

		if (dx0 < c->x) dx0 = c->x;
		if (dy0 < c->y) dy0 = c->y;
		if (dx1 > c->x + c->w - 1) dx1 = (short)(c->x + c->w - 1);
		if (dy1 > c->y + c->h - 1) dy1 = (short)(c->y + c->h - 1);
		if (dx1 >= dx0 && dy1 >= dy0) {
			memset(&scr, 0, sizeof scr);
			pxy[0] = (short)(zoom.left + (dx0 - x));
			pxy[1] = (short)(zoom.top + (dy0 - y));
			pxy[2] = (short)(zoom.left + (dx1 - x));
			pxy[3] = (short)(zoom.top + (dy1 - y));
			pxy[4] = dx0;
			pxy[5] = dy0;
			pxy[6] = dx1;
			pxy[7] = dy1;
			vro_cpyfm(vdi_h, 3, pxy, (MFDB *)from, &scr);
		}
	} else {
		/* its frame till its rows come */
		short yb = (short)(y + (y1 - zoom.top) - 1);

		line_col(x, y, (short)(x + vw - 1), y, 1);
		line_col(x, yb, (short)(x + vw - 1), yb, 1);
		line_col(x, y, x, yb, 1);
		line_col((short)(x + vw - 1), y, (short)(x + vw - 1), yb, 1);
	}
	vswr_mode(vdi_h, MD_REPLACE);
	vst_color(vdi_h, 1);
}

/* w x h at the screen's depth fits in memory, with room to spare */
static int zoom_fits(long w, long h)
{
	long bytes = (w + 15) / 16 * 2 * (planes > 8 ? 16 : planes) * h;

	return (long)Malloc(-1L) >= bytes + 128 * 1024L;
}

/* a page of View > Pages at pct of its line's size: its picture (drawn
   bigger by render.ovl on a Falcon or TT, in colour; else EConvert's
   double, at 200 % only), *w and *h; 0 when there is none at that size */
static int zoom_page_pic(const DVPIC *b, int pct, DVPIC *out)
{
	long w = (long)b->w * pct / 100, h = (long)b->h * pct / 100, d, dw, dh;

	*out = *b;
	if (doc_page_drawable() && !(zoom_failed_at && pct >= zoom_failed_at)) {
		out->obj = PICS_PAGE + b->page;
		out->flip = 0;
	} else if (pct == 200 && doc_double_of(b->obj, &d, &dw, &dh) && dw > 0 && dh > 0) {
		out->obj = d;
		w = dw < 2L * cols * cw ? dw : 2L * cols * cw;
		h = dh * w / dw * 16 / (pix_aspect > 0 ? pix_aspect : 16);
	} else
		return 0;
	if (h > 8 * 1024)
		return 0;
	if (!zoom_fits(w, h))
		return 0;
	out->w = (int)w;
	out->h = (int)h;
	return 1;
}

/* p (a page's zoomed picture) shown, and until it is made (render.ovl:
   seconds to tens of seconds) the page's picture as View > Pages has it
   enlarged at once, its pixels nearest (Erez, 8 Nov 2026: "tens of
   seconds of wait time ... the user may think there is an error"), where
   memory holds both */
static void zoom_set_pic(const DVPIC *p)
{
	long bytes = (p->w + 15) / 16 * 2 * (planes > 8 ? 16 : planes) * (long)p->h;
	PICE *b;

	zoom_pre_free();
	zoom.p = *p;
	doc_pic_stop_page_unless(p->obj, p->w, p->h);	/* (the page drawing at another size gives way at once) */
	b = pics_get(zoom.base.obj, zoom.base.w, zoom.base.h, zoom.base.flip);
	if (p->obj >= PICS_PAGE && b && b->state == PICS_READY && (long)Malloc(-1L) >= 2 * bytes + 128 * 1024L) {
		unsigned long t0 = tos_ms();

		graf_mouse(BUSYBEE, NULL);
		if (pics_enlarge(b, p->w, p->h, &zoom.pre, &zoom.pre_heap) == 0) {
			char a[24];

			snprintf(a, sizeof a, "%lu ms", tos_ms() - t0);
			log_event("zoom-enlarged", a, NULL);
		} else
			memset(&zoom.pre, 0, sizeof zoom.pre);
		graf_mouse(ARROW, NULL);
	}
}

/* the page line in view (View > Pages): the first row of a page's picture
   from the top, or the one at row r */
static int page_line_at(long r0, long r1, DVPIC *out)
{
	DOCVIEW *v = doc_view();
	long r, n, page, off;
	int kind;
	const char *s;

	if (!v || !doc_pages_mode())
		return 0;
	for (r = r0; r <= r1 && dv_row(v, r, &kind, &s, &n, &page, &off); r++)
		if (kind == DV_PIC && v->pic.w >= 32) {
			*out = v->pic;
			out->page = page;
			out->row = 0;
			return 1;
		}
	return 0;
}

/* the zoom to step k of zsteps (0: View > Pages again), the point
   (fx, fy) of the window kept where it is */
static void zoom_to(int k, short fx, short fy)
{
	DVPIC p;
	long cx, cy, ow, oh;

	if (k <= 0) {
		win_zoom_end();
		return;
	}
	if (!zoom_page_pic(&zoom.base, zsteps[k], &p))
		return;
	ow = zoom.on ? zoom.p.w : zoom.base.w;
	oh = zoom.on ? zoom.p.h : zoom.base.h;
	/* the point under (fx, fy) in the old picture, then in the new */
	cx = (zoom.on ? zoom.left : 0) + (fx - work.x - (work.w - (ow < work.w ? ow : work.w)) / 2);
	cy = (zoom.on ? zoom.top : zoom.base.row) + (fy - work.y);
	cx = cx * p.w / (ow > 0 ? ow : 1);
	cy = cy * p.h / (oh > 0 ? oh : 1);
	zoom_set_pic(&p);
	zoom.pct = zsteps[k];
	zoom.left = cx - (fx - work.x);
	zoom.top = cy - (fy - work.y);
	{
		long mx = zoom.p.w > work.w ? zoom.p.w - work.w : 0, my = zoom.p.h > work.h ? zoom.p.h - work.h : 0;

		zoom.left = zoom.left < 0 ? 0 : zoom.left > mx ? mx : zoom.left;
		zoom.top = zoom.top < 0 ? 0 : zoom.top > my ? my : zoom.top;
	}
	zoom.on = 1;
	{
		char a[16];

		snprintf(a, sizeof a, "%d", zoom.pct);
		log_event("zoom", a, NULL);
	}
	if (has_menu_states)
		has_menu_states();
	win_draw_all();
}

static int zoom_step_now(void)
{
	int k;

	for (k = 0; k < NZSTEPS; k++)
		if (zsteps[k] >= (zoom.pct ? zoom.pct : 100))
			return k;
	return NZSTEPS - 1;
}

void win_zoom_forget(void)
{
	zoom_failed_at = 0;
}

/* the zoom a step smaller after the bigger page couldn't be made */
static void zoom_back(void)
{
	int k = zoom_step_now();

	zoom_failed_at = zoom.pct;
	zoom_to(k - 1, (short)(work.x + work.w / 2), (short)(work.y + work.h / 2));
}

int win_zoom_can(int dir)
{
	DVPIC b, p;
	int k;

	if (zoom.on && !zoom.pct)
		return 0;			/* (a picture shown whole, not a page) */
	if (dir < 0)
		return zoom.on;
	if (zoom.on)
		b = zoom.base;
	else if (!page_line_at(0, rows, &b))
		return 0;
	for (k = zoom_step_now() + 1; k < NZSTEPS; k++)
		if (zoom_page_pic(&b, zsteps[k], &p))
			return 1;
	return 0;
}

void win_zoom_step(int dir)
{
	int k = zoom_step_now();
	DVPIC p;

	if (zoom.on && !zoom.pct)
		return;
	if (!zoom.on) {
		long r;

		if (dir < 0 || !page_line_at(0, rows, &zoom.base))
			return;
		/* the page's first row in view: where its picture starts */
		for (r = 0; r <= rows; r++) {
			DVPIC q;

			if (page_line_at(r, r, &q) && q.page == zoom.base.page) {
				DOCVIEW *v = doc_view();
				long n, page, off;
				int kind;
				const char *s;

				dv_row(v, r, &kind, &s, &n, &page, &off);
				zoom.base.row = (int)(v->pic.row * ch - r * ch);	/* (rows of it above the window: negative) */
				break;
			}
		}
		zoom.base.row = zoom.base.row < 0 ? -zoom.base.row : 0;
	}
	if (dir < 0) {
		zoom_to(k - 1, (short)(work.x + work.w / 2), (short)(work.y + work.h / 2));
		return;
	}
	for (k = k + 1; k < NZSTEPS; k++)
		if (zoom_page_pic(&zoom.base, zsteps[k], &p)) {
			zoom_to(k, (short)(work.x + work.w / 2), (short)(work.y + work.h / 2));
			return;
		}
}

/* View > Actual size: View > Pages' page as it was */
void win_zoom_actual(void)
{
	if (zoom.on && zoom.pct)
		zoom_to(0, (short)(work.x + work.w / 2), (short)(work.y + work.h / 2));
}

/* a click at (mx, my) on a picture with more pixels than it shows: shown
   whole (a page of View > Pages: at 200 %); 1 when it was one */
int win_zoom_at(short mx, short my)
{
	DOCVIEW *v = doc_view();
	long r, n, page, off;
	int kind;
	const char *s;
	PICE *e;
	short x;
	long w, h;

	if (!v || win < 0 || iconified || zoom.on || my < work.y || my >= work.y + work.h)
		return 0;
	r = (my - work.y) / ch;
	if (!dv_row(v, r, &kind, &s, &n, &page, &off) || kind != DV_PIC)
		return 0;
	x = pic_x(v->pic.w);
	if (mx < x || mx >= x + v->pic.w)
		return 0;
	if (doc_pages_mode() && v->pic.w >= 32) {
		/* a page: 200 %, the point clicked kept under the mouse */
		DVPIC p;
		int k;

		zoom.base = v->pic;
		zoom.base.page = page;
		zoom.base.row = 0;
		for (k = 0; k < NZSTEPS && zsteps[k] != 200; k++)
			;
		if (zoom_page_pic(&zoom.base, 200, &p)) {
			short fx = mx, fy = my;

			/* (the point's place in the page's line: its row and column) */
			zoom.on = 0;
			zoom.pct = 0;
			{
				long cx = mx - x, cy = (long)v->pic.row * ch + (my - work.y - r * ch);

				zoom_set_pic(&p);
				zoom.pct = 200;
				zoom.left = cx * p.w / v->pic.w - (fx - work.x);
				zoom.top = cy * p.h / v->pic.h - (fy - work.y);
			}
			{
				long mxl = zoom.p.w > work.w ? zoom.p.w - work.w : 0, myl = zoom.p.h > work.h ? zoom.p.h - work.h : 0;

				zoom.left = zoom.left < 0 ? 0 : zoom.left > mxl ? mxl : zoom.left;
				zoom.top = zoom.top < 0 ? 0 : zoom.top > myl ? myl : zoom.top;
			}
			zoom.on = 1;
			log_event("zoom", "200", NULL);
			if (has_menu_states)
				has_menu_states();
			win_draw_all();
			return 1;
		}
	}
	e = pics_get(v->pic.obj, v->pic.w, v->pic.h, v->pic.flip);
	if (!enlargeable(e, &v->pic))
		return 0;
	zoom.p.obj = v->pic.obj;
	/* its own pixels, up to twice the text column (wider than the window:
	   Left and Right move it), as much of that as memory holds: the
	   machine's own (Erez, 5 Nov) */
	w = e->sw < 2L * cols * cw ? e->sw : 2L * cols * cw;
	for (;;) {
		h = e->sh * w / e->sw * 16 / (pix_aspect > 0 ? pix_aspect : 16);
		if (h < 1)
			h = 1;
		if (h > 8 * 1024)
			h = 8 * 1024;
		if (w <= v->pic.w + 4)
			return 0;	/* (no room for more than is shown) */
		if (zoom_fits(w, h))
			break;
		w = w * 3 / 4;
	}
	{
		long o = zoom.p.obj;

		zoom.p = v->pic;
		zoom.p.obj = o;
	}
	zoom.p.w = (int)w;
	zoom.p.h = (int)h;
	zoom.top = 0;
	zoom.left = 0;
	zoom.pct = 0;
	zoom.on = 1;
	log_event("zoom", NULL, NULL);
	win_draw_all();
	return 1;
}

/* the zoomed picture moved by (dx, dy) pixels (a drag): 1 when it moved */
int win_zoom_pan(long dx, long dy)
{
	long l = zoom.left - dx, t = zoom.top - dy;
	long mx = zoom.p.w > work.w ? zoom.p.w - work.w : 0, my = zoom.p.h > work.h ? zoom.p.h - work.h : 0;

	if (!zoom.on)
		return 0;
	l = l < 0 ? 0 : l > mx ? mx : l;
	t = t < 0 ? 0 : t > my ? my : t;
	if (l == zoom.left && t == zoom.top)
		return 0;
	zoom.left = l;
	zoom.top = t;
	{
		/* drawn while the button is held: not BEG_UPDATE of our own,
		   which TOS's AES holds back till the release (5.8, click.h) */
		GRECT r, c;
		int took = press_update_begin();

		graf_mouse(M_OFF, NULL);
		wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
		while (r.w > 0 && r.h > 0) {
			if (overlap(&r, &work, &c))
				draw_zoom(&c);
			wind_get(win, WF_NEXTXYWH, &r.x, &r.y, &r.w, &r.h);
		}
		graf_mouse(M_ON, NULL);
		press_update_end(took);
	}
	return 1;
}

/* a key while a picture is shown whole: 1 taken */
int win_zoom_key(short scan)
{
	long step = 0, most;

	if (!zoom.on)
		return 0;
	if (scan == 0x4b || scan == 0x4d) {
		/* Left, Right: its columns, where it is wider than the window */
		long l = zoom.left + (scan == 0x4b ? -work.w / 2 : work.w / 2), mx = zoom.p.w > work.w ? zoom.p.w - work.w : 0;

		l = l < 0 ? 0 : l > mx ? mx : l;
		l &= ~15L;		/* (on a word of the bitmap: the copy's fast way) */
		if (l != zoom.left) {
			zoom.left = l;
			win_redraw(&work);
		}
		return 1;
	}
	switch (scan) {
	case 0x01: case 0x0e: case 0x1c: case 0x72: case 0x61:	/* Esc, Backspace, Return, Enter, Undo */
		win_zoom_end();
		return 1;
	case 0x4a: case 0x4e:					/* keypad - and +: Zoom out, in */
		return 0;
	case 0x48: step = -2L * ch; break;			/* Up */
	case 0x50: step = 2L * ch; break;			/* Down */
	case 0x49: step = -(long)(work.h - ch); break;		/* Page Up */
	case 0x51: step = work.h - ch; break;			/* Page Down */
	case 0x47: step = -zoom.p.h; break;			/* Clr/Home */
	default:
		win_zoom_end();		/* any other key: the text back, the key its own */
		return 0;
	}
	most = zoom.p.h > work.h ? zoom.p.h - work.h : 0;
	step += zoom.top;
	if (step < 0)
		step = 0;
	if (step > most)
		step = most;
	if (step != zoom.top) {
		zoom.top = step;
		win_redraw(&work);
	}
	return 1;
}

void win_redraw(const GRECT *area)
{
	GRECT r, c, a = *area, ba;
	long r0, r1;
	int has_bar, has_text;

	if (win < 0)
		return;
	if (iconified) {
		static char st[16];

		if (doc_is_open())
			snprintf(st, sizeof st, "%ld/%ld", doc_view() ? dv_page_at_top(doc_view()) + 1 : 0, doc_pages());
		else
			st[0] = '\0';
		app_icon_redraw(win, area, st);
		return;
	}
	{
		GRECT b = whole;

		b.y = (short)(whole.y + whole.h - bar_h);
		b.h = bar_h;
		has_bar = bar_h && overlap(area, &b, &ba);	/* the status bar's part */
	}
	has_text = overlap(&a, &work, &a);
	if (!has_text && !has_bar)
		return;
	r0 = (a.y - work.y) / ch;
	r1 = (a.y + a.h - work.y + ch - 1) / ch;
	if (has_text && doc_view() && doc_view()->nstale)
		dv_recount_view(doc_view(), rows + 1);	/* those in view counted before they are drawn */
	wind_update(BEG_UPDATE);
	graf_mouse(M_OFF, NULL);
	wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	while (r.w > 0 && r.h > 0) {
		if (has_text && overlap(&r, &a, &c)) {
			if (zoom.on)
				draw_zoom(&c);
			else
				draw_rows(&c, r0, r1);
		}
		if (has_bar && overlap(&r, &ba, &c))
			draw_bar(&c);
		wind_get(win, WF_NEXTXYWH, &r.x, &r.y, &r.w, &r.h);
	}
	graf_mouse(M_ON, NULL);
	wind_update(END_UPDATE);
	log_event("drawn", NULL, NULL);
}

/*
 * The whole window drawn again (next page, View > The page's lines, a
 * new font), in one piece: drawing is not sliced (Erez, 4 Nov 2026, the
 * guidelines' "Drawing is not such a job"; from 0.5.1 to 1.1 a VDI font's
 * text was owed to later turns in bands of about 60 ms, and a scroll's
 * rows to the next turn).
 */
void win_draw_all(void)
{
	ahead_again();
	if (win < 0)
		return;
	win_redraw(&whole);		/* the progress row too */
}

/* rows [r0, r1) drawn again, where they show (a page that came in) */
void win_draw_rows(long r0, long r1)
{
	GRECT a;

	if (win < 0 || iconified || r1 <= 0 || r0 >= rows + 1)
		return;
	if (r0 < 0)
		r0 = 0;
	a.x = work.x;
	a.w = work.w;
	a.y = (short)(work.y + r0 * ch);
	a.h = (short)((r1 - r0) * ch);
	if (a.y + a.h > work.y + work.h)
		a.h = (short)(work.y + work.h - a.y);
	if (a.h > 0) {
		edges_clean = 1;	/* a page that came in: the same layout */
		win_redraw(&a);
		edges_clean = 0;
	}
}

/*
 * The view moved d rows (down the document when d > 0): what stays in
 * view is copied on the screen when the whole work area shows, and only
 * the rows that come in are drawn; otherwise all of it.
 */
/* after a scroll's rows, in the same turn: the status bar's page; the
   sliders (the AES's own, 40 ms of an ST's page scrolled when one moves)
   when no key waits, the window's text drawn just as soon and the next
   key taken that much sooner */
static void moved_rest(void)
{
	sliders_due = 1;
	ahead_again();
	if (win_progress_due())
		win_progress_draw();
}

/*
 * A scroll's copy on a 256-colour screen by the CPU, whole rows of
 * 16-pixel groups at a time (common's scr_copy_rows): the VDI's copy of a
 * window's rows was 65-70 ms of Falcon-4's line, the CPU's 60-65 (the
 * memory's floor either way; scr_rows leaves a 68030's to the VDI, as fast
 * in 16 colours). The box starts on a group and is whole groups wide (the
 * caller's); 0 when it isn't for here.
 */
static int copy_wide(const short *p)
{
	long bpl, n;
	short np, sw, sh, rows_n;
	unsigned char *base;

	if (planes != 8 || (p[0] & 15) || ((p[2] + 1) & 15) || p[4] != p[0] || p[6] != p[2] ||
	    p[3] - p[1] != p[7] - p[5] || !(base = scr_at(&bpl, &np, &sw, &sh)) || np != 8 || p[2] >= sw ||
	    p[1] < 0 || p[5] < 0 || p[3] >= sh || p[7] >= sh)
		return 0;
	n = (long)(p[2] + 1 - p[0]) / 16 * 2 * np;
	rows_n = (short)(p[3] - p[1] + 1);
	base += (long)p[0] / 16 * 2 * np;
	if (p[5] <= p[1])	/* up the screen: from the top row on */
		scr_copy_rows(base + (long)p[5] * bpl, bpl, base + (long)p[1] * bpl, bpl, rows_n, (short)n);
	else			/* down it: from the last row back */
		scr_copy_rows(base + (long)p[7] * bpl, -bpl, base + (long)p[3] * bpl, -bpl, rows_n, (short)n);
	return 1;
}

static void moved(long d)
{
	GRECT r;
	short keep, cx, cwid;

	if (d == 0)
		return;
	T_START();
	win_info();
	T_MARK("info");
	keep = (short)(rows - (d < 0 ? -d : d));
	/* the block copied starts on a word (TOS 1.04 copies one that starts
	   inside a word a bit at a time, the guidelines' Speed): the text
	   starts 8 pixels in, and the margin needs no copy */
	{
		short x0 = (work.x & 15) ? work.x + MARGIN : work.x;
		short x1 = (short)(work.x + MARGIN + cols * cw);

		cx = (short)(x0 & ~15) < work.x ? x0 : (short)(x0 & ~15);
		cwid = (short)(((x1 - cx) + 15) & ~15);
		if (cx + cwid > work.x + work.w)
			cwid = (short)(work.x + work.w - cx);
	}
	wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	if (keep <= 0 || r.x > work.x || r.y > work.y || r.x + r.w < work.x + work.w || r.y + r.h < work.y + work.h ||
	    work.x < desk_x || work.y < desk_y || work.x + work.w > desk_x + desk_w || work.y + work.h > desk_y + desk_h) {
		edges_clean = 1;	/* scrolled: the same layout */
		win_draw_all();
		edges_clean = 0;
		moved_rest();
		return;
	}
	{
		short pxy[8], clip[4];
		short ad = (short)((d < 0 ? -d : d) * ch);

		pxy[0] = pxy[4] = cx;
		pxy[2] = pxy[6] = (short)(cx + cwid - 1);
		pxy[1] = (short)(work.y + (d > 0 ? ad : 0));
		pxy[3] = (short)(pxy[1] + keep * ch - 1);
		pxy[5] = (short)(work.y + (d > 0 ? 0 : ad));
		pxy[7] = (short)(pxy[5] + keep * ch - 1);
		if (d < 0) {
			/* down the screen: what shows of the row after the last
			   whole one moves too, the top of the cut row at the foot */
			pxy[3] = (short)(work.y + work.h - 1 - ad);
			pxy[7] = (short)(work.y + work.h - 1);
		}
		clip[0] = work.x;
		clip[1] = work.y;
		clip[2] = (short)(work.x + work.w - 1);
		clip[3] = (short)(work.y + work.h - 1);
		wind_update(BEG_UPDATE);
		graf_mouse(M_OFF, NULL);
		text_clip(vdi_h, clip);		/* the VDI clips raster copies too */
		if (!copy_wide(pxy))
			scr_rows(vdi_h, pxy);	/* TOS's screen by the CPU, else vro_cpyfm */
		text_noclip(vdi_h);
		T_MARK("copy");
		{
			/* the rows that came in, drawn while the screen and the
			   mouse are still held (the whole work area shows: one
			   rectangle): at the foot the cut row and the one above it,
			   at the top those that came in */
			long r0 = d > 0 ? keep : 0, r1 = d > 0 ? rows + 1 : rows - keep;
			GRECT a;

			a.x = work.x;
			a.w = work.w;
			a.y = (short)(work.y + r0 * ch);
			a.h = (short)((r1 - r0) * ch);
			if (a.y + a.h > work.y + work.h)
				a.h = (short)(work.y + work.h - a.y);
			if (a.h > 0) {
				/* drawn at once, in the copy's own hold of the screen and
				   the mouse (the whole work area shows: one rectangle; a
				   second walk of the AES's list was 10 ms of an ST's line) */
				edges_clean = 1;	/* scrolled: the same layout */
				draw_rows(&a, r0, r1);
				edges_clean = 0;
				text_noclip(vdi_h);
			}
		}
		graf_mouse(M_ON, NULL);
		wind_update(END_UPDATE);
		log_event("drawn", NULL, NULL);
	}
	T_MARK("rows");
	moved_rest();
}

void win_scroll(long d)
{
	DOCVIEW *v = doc_view();

	if (!v || win < 0 || iconified)
		return;
	moved(dv_scroll(v, d, rows));
}

void win_scroll_to_abs(long r)
{
	DOCVIEW *v = doc_view();
	long before;

	if (!v || win < 0 || iconified)
		return;
	before = dv_top_abs(v);
	dv_set_top_abs(v, r, rows);
	moved(dv_top_abs(v) - before);
}

void win_slider(short pos)
{
	DOCVIEW *v = doc_view();

	if (v && v->total > rows)
		win_scroll_to_abs((long)pos * (v->total - rows) / 1000);
}

void win_goto_page(long n)
{
	DOCVIEW *v = doc_view();

	if (!v || win < 0)
		return;
	dv_goto_page(v, n);		/* its head at the top, even near the end */
	win_info();
	edges_clean = 1;		/* the same layout */
	win_draw_all();
	edges_clean = 0;
	moved_rest();			/* the sliders and the bar's page, in the same turn */
}

/*
 * Pictures ahead of the view (Erez, 4 Nov 2026: "can the images even
 * though they are not yet displayed get silently converted behind the
 * scene in a way that will not block the user reading and navigating"):
 * the view's page and AHEAD_PAGES after it, a page a turn when nothing else
 * waits, each picture queued to be made quietly (pics_ahead, which takes
 * two at a time). The pictures are found in the pages' stored text
 * (dv_page_pic): a page the reader found none on is passed over at once,
 * nothing is laid out (laying the pages ahead out pushed the view's own
 * out: a line scrolled on an ST 50 -> 80 ms with none to make) and nothing
 * read from the cache file. Looked at again after the view moves, a page
 * comes or a picture is made.
 */
#define AHEAD_PAGES 3
static int ahead_due;			/* the view moved: look ahead again */
static long ahead_page = -1, ahead_pos;	/* the page looked at, and where in its text; -1 from the view's */

int win_ahead_due(void)
{
	return ahead_due && win >= 0 && !iconified && doc_view();
}

void win_ahead(void)
{
	DOCVIEW *v = doc_view();
	long top;
	DVPIC p;

	if (!v || win < 0 || iconified) {
		ahead_due = 0;
		return;
	}
	top = dv_page_at_top(v);
	if (ahead_page < top) {
		ahead_page = top;
		ahead_pos = 0;
	}
	for (; ahead_page <= top + AHEAD_PAGES && ahead_page < v->npages; ahead_page++, ahead_pos = 0) {
		if (!ts_pics(v->ts, ahead_page))
			continue;		/* (none on it, or not read yet: looked at when it comes) */
		while (dv_page_pic(v, ahead_page, &ahead_pos, &p))
			if (pics_ahead(p.obj, p.w, p.h, p.flip, p.page) < 0) {
				ahead_due = 0;	/* enough queued, or no room: again when the view moves */
				return;
			}
		ahead_page++;
		ahead_pos = 0;
		return;			/* (a page with pictures a turn) */
	}
	ahead_due = 0;
}

static void ahead_again(void)
{
	ahead_due = 1;
	ahead_page = -1;
}

/* picture obj is ready (or can't be shown): its rows drawn again where they show */
void win_pic_ready(long obj)
{
	DOCVIEW *v = doc_view();
	long r, first = -1, last = -1, n, page, off;
	int kind;
	const char *s;

	if (!v || win < 0 || iconified)
		return;
	if (zoom.on) {
		if (obj == zoom.p.obj) {
			PICE *e = pics_get(zoom.p.obj, zoom.p.w, zoom.p.h, zoom.p.flip);

			if (zoom.pct && e && e->state == PICS_FAILED) {
				/* no memory for the page this big: a step back */
				log_event("zoom-failed", NULL, NULL);
				zoom_back();
				return;
			}
			win_redraw(&work);
		}
		return;
	}
	for (r = 0; r <= rows && dv_row(v, r, &kind, &s, &n, &page, &off); r++)
		if (kind == DV_PIC && v->pic.obj == obj) {
			if (first < 0)
				first = r;
			last = r;
		}
	if (first >= 0)
		win_draw_rows(first, last + 1);
	ahead_again();			/* (room for the next one ahead) */
}

/* pixel rows from, to of picture obj made (it is made from the top
   down): only the window's rows they are in drawn again */
void win_pic_rows(long obj, long from, long to)
{
	DOCVIEW *v = doc_view();
	long r, first = -1, last = -1, n, page, off;
	int kind;
	const char *s;

	if (!v || win < 0 || iconified || to <= from)
		return;
	if (zoom.on) {
		if (obj == zoom.p.obj && from < zoom.top + work.h && to > zoom.top)
			win_redraw(&work);
		return;
	}
	for (r = 0; r <= rows && dv_row(v, r, &kind, &s, &n, &page, &off); r++)
		if (kind == DV_PIC && v->pic.obj == obj && (v->pic.row + 1) * ch > from && v->pic.row * ch < to) {
			if (first < 0)
				first = r;
			last = r;
		}
	if (first >= 0)
		win_draw_rows(first, last + 1);
}

/* page n's text came: its rows drawn again where it shows; the rest of
   the window too when the rows before the top changed (nothing moves:
   the top is a page and a row) */
void win_page_ready(long n)
{
	DOCVIEW *v = doc_view();
	long r, first = -1;

	if (!v || win < 0 || iconified)
		return;
	sliders_due = 1;
	for (r = 0; r <= rows; r++) {
		int kind;
		const char *s;
		long len, page, off;

		if (!dv_row(v, r, &kind, &s, &len, &page, &off))
			break;
		if (page == n && first < 0)
			first = r;
		if (page > n)
			break;
	}
	if (first >= 0)
		win_draw_rows(first, rows + 1);	/* the rows after it moved too */
	ahead_again();			/* (its pictures, if ahead) */
}

/* the place in the text at screen point (mx, my): its page and offset in
   the page's text (a row's end past its last letter); 0 outside the text */
int win_hit(short mx, short my, long *page, long *off)
{
	DOCVIEW *v = doc_view();
	long r, n, col;
	int kind;
	const char *s;

	if (!v || win < 0 || iconified || mx < work.x || my < work.y || mx >= work.x + work.w || my >= work.y + work.h)
		return 0;
	r = (my - work.y) / ch;
	if (!dv_row(v, r, &kind, &s, &n, page, off) || *off < 0)
		return 0;
	{
		const char *vis;
		short x;
		int bi = row_visual(s, n, &vis, &x);

		col = (mx - x + cw / 2) / cw;
		if (col < 0)
			col = 0;
		if (col > n)
			col = n;
		if (bi && col < n) {
			/* the logical letter shown in that column */
			long i;

			for (i = 0; i < n; i++)
				if (vpos[i] == col) {
					col = vodd[i] ? i + 1 : i;
					break;
				}
		}
	}
	*off += col;
	return 1;
}

/* the rows from the top that show page p0 offset o0 to p1 o1 drawn again */
void win_draw_span(long p0, long o0, long p1, long o1)
{
	DOCVIEW *v = doc_view();
	long r, first = -1, last = -1, n, page, off;
	int kind;
	const char *s;

	if (!v || win < 0 || iconified)
		return;
	if (p1 < p0 || (p1 == p0 && o1 < o0)) {
		long t;

		t = p0, p0 = p1, p1 = t;
		t = o0, o0 = o1, o1 = t;
	}
	for (r = 0; r <= rows && dv_row(v, r, &kind, &s, &n, &page, &off); r++) {
		if (off < 0)
			continue;
		if ((page > p0 || (page == p0 && off + n >= o0)) && (page < p1 || (page == p1 && off <= o1))) {
			if (first < 0)
				first = r;
			last = r;
		}
	}
	if (first >= 0)
		win_draw_rows(first, last + 1);
}

/*
 * The cells of page p0 offset o0 to p1 o1 (either way round) inverted on
 * the screen as they stand (XOR), through the window's rectangles: a
 * selection made, let go or changed by that span without its text drawn
 * again (Select all on a whole window: 445 ms drawn again on an ST, a few
 * filled boxes this way). The screen must show the view as it is now.
 */
/* a span in order (p0 < 0 stays none) */
static void in_order(long *p0, long *o0, long *p1, long *o1)
{
	if (*p0 >= 0 && (*p1 < *p0 || (*p1 == *p0 && *o1 < *o0))) {
		long t;

		t = *p0, *p0 = *p1, *p1 = t;
		t = *o0, *o0 = *o1, *o1 = t;
	}
}

/*
 * The selection changed from the span q to the span p (either may be none,
 * q0 or p0 < 0): only the rows whose shape changed are drawn, by inverting
 * the old shape off and the new one on; whole rows to invert one after
 * another go as one box (Select all: a window's rows in one).
 */
void win_sel_change(long q0, long r0, long q1, long r1, long p0, long o0, long p1, long o1)
{
	DOCVIEW *v = doc_view();
	GRECT r, c;
	long row, n, page, off;
	int kind, took;
	const char *s;

	T_START();

	if (!v || win < 0 || iconified || (q0 < 0 && p0 < 0))
		return;
	in_order(&q0, &r0, &q1, &r1);
	in_order(&p0, &o0, &p1, &o1);
	/* not BEG_UPDATE of our own: drawn on a press (the old selection off,
	   Shift's taken on) or while dragging, TOS's AES would hold it back
	   while the button is held still (5.8, common's click.h) */
	took = press_update_begin();
	graf_mouse(M_OFF, NULL);
	T_MARK("sel-begin");
	wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	T_MARK("sel-first");
	while (r.w > 0 && r.h > 0) {
		if (overlap(&r, &work, &c)) {
			short clip[4], box = -1, y = work.y;	/* box: the first row of a run of whole rows, -1 none */

			clip[0] = c.x;
			clip[1] = c.y;
			clip[2] = (short)(c.x + c.w - 1);
			clip[3] = (short)(c.y + c.h - 1);
			text_clip(vdi_h, clip);		/* scr_xor cuts to it */
			for (row = 0; row <= rows; row++, y = (short)(y + ch)) {
				long oa = 0, ob = 0, na = 0, nb = 0;
				int ko = 0, kn = 0;

				if (dv_row(v, row, &kind, &s, &n, &page, &off) && kind == DV_TEXT) {
					if (n > cols)
						n = cols;	/* as draw_rows shows it */
					ko = sel_shape(q0, r0, q1, r1, page, off, n, &oa, &ob);
					kn = sel_shape(p0, o0, p1, o1, page, off, n, &na, &nb);
				}
				if (ko == kn && (ko != 1 || (oa == na && ob == nb))) {
					if (box >= 0)
						rows_xor(&c, box, (short)(y - 1));
					box = -1;
					continue;
				}
				if (ko == 1)
					cells_xor(&c, s, n, oa, ob, y);
				if (kn == 1)
					cells_xor(&c, s, n, na, nb, y);
				if ((ko == 2) != (kn == 2)) {
					if (box < 0)
						box = y;	/* the whole row on or off: in a box with the next */
				} else if (box >= 0) {
					rows_xor(&c, box, (short)(y - 1));
					box = -1;
				}
			}
			if (box >= 0)
				rows_xor(&c, box, (short)(y - 1));
			T_MARK("sel-rows");
			text_noclip(vdi_h);
		}
		wind_get(win, WF_NEXTXYWH, &r.x, &r.y, &r.w, &r.h);
	}
	T_MARK("sel-next");
	graf_mouse(M_ON, NULL);
	press_update_end(took);
	T_MARK("sel-end");
}

/* the selection's start shown in the window, a few rows under the top */
void win_show(long page, long off)
{
	DOCVIEW *v = doc_view();
	long row;

	if (!v || win < 0)
		return;
	row = dv_row_of(v, page, off);
	if (row <= 0)
		return;
	dv_show_row(v, page, row, rows / 3, rows);
	win_info();
	edges_clean = 1;		/* the same layout */
	win_draw_all();
	edges_clean = 0;
	moved_rest();
}

int win_open(short x, short y, short w, short h)
{
	if (w <= 0 || h <= 0 || x < desk_x || y < desk_y || x + w > desk_x + desk_w || y + h > desk_y + desk_h) {
		x = desk_x;
		y = desk_y;
		w = desk_w;
		h = desk_h;
	}
	place(&x, &y, &w, &h);
	win = wind_create(icon_kind(WKIND), desk_x, desk_y, desk_w, desk_h);
	if (win < 0)
		return -1;
	sl_size = sl_pos = sl_px = -1;	/* a new window: its sliders set */
	win_title();
	wind_open(win, x, y, w, h);
	iconified = 0;
	get_work();
	sliders();
	win_info();
	return 0;
}

void win_close(void)
{
	if (win < 0)
		return;
	wind_close(win);
	wind_delete(win);
	win = -1;
	iconified = 0;
	bar_h = 0;
}

/* where the window is, for the INF */
void win_where(short *x, short *y, short *w, short *h)
{
	if (win < 0 || iconified) {
		*x = *y = *w = *h = 0;
		return;
	}
	wind_get(win, WF_CURRXYWH, x, y, w, h);
}

/* the window's own messages; 1 when it was one */
int win_message(const short *msg)
{
	short x, y, w, h;

	if (win < 0 || msg[3] != win)
		return 0;
	switch (msg[0]) {
	case WM_REDRAW: {
		GRECT a;

		a.x = msg[4];
		a.y = msg[5];
		a.w = msg[6];
		a.h = msg[7];
		win_redraw(&a);
		return 1;
	}
	case WM_TOPPED:
		wind_set(win, WF_TOP, win, 0, 0, 0);
		return 1;
	case WM_BOTTOMED:
		wind_set(win, WF_BOTTOM, win, 0, 0, 0);
		return 1;
	case WM_FULLED:
		if (iconified)
			return 1;
		wind_get(win, WF_CURRXYWH, &x, &y, &w, &h);
		{
			short fx, fy, fw, fh;

			wind_get(win, WF_FULLXYWH, &fx, &fy, &fw, &fh);
			if (x == fx && y == fy && w == fw && h == fh)
				wind_get(win, WF_PREVXYWH, &fx, &fy, &fw, &fh);
			place(&fx, &fy, &fw, &fh);
			wind_set(win, WF_CURRXYWH, fx, fy, fw, fh);
		}
		win_layout();
		win_draw_all();
		return 1;
	case WM_MOVED:
	case WM_SIZED:
		if (iconified) {
			wind_set(win, WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
			return 1;
		}
		x = msg[4];
		y = msg[5];
		w = msg[6];
		h = msg[7];
		place(&x, &y, &w, &h);
		{
			short ow = work.w, oh = work.h;

			wind_set(win, WF_CURRXYWH, x, y, w, h);
			get_work();
			/* a move: the AES sends what comes into view; a size: laid out again */
			if (work.w != ow || work.h != oh) {
				win_layout();
				if (work.w != ow)
					win_draw_all();
			}
		}
		return 1;
	case WM_ARROWED:
		if (iconified)
			return 1;
		switch (msg[4]) {
		case WA_UPPAGE: win_scroll(-(rows - 1)); break;
		case WA_DNPAGE: win_scroll(rows - 1); break;
		case WA_UPLINE: win_scroll(-1); break;
		case WA_DNLINE: win_scroll(1); break;
		}
		return 1;
	case WM_VSLID:
		if (!iconified)
			win_slider(msg[4]);
		return 1;
	case WM_ICONIFY:
	case WM_ALLICONIFY:
		if (!iconified) {
			wind_set(win, WF_ICONIFY, msg[4], msg[5], msg[6], msg[7]);
			iconified = 1;
			log_event("iconified", NULL, NULL);
		}
		return 1;
	case WM_UNICONIFY:
		if (iconified) {
			wind_set(win, WF_UNICONIFY, msg[4], msg[5], msg[6], msg[7]);
			iconified = 0;
			win_layout();
			log_event("uniconified", NULL, NULL);
		}
		return 1;
	}
	return 0;
}

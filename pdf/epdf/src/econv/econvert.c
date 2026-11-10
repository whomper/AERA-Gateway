/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * econvert.c - EConvert, the AERA PDF converter: PDFs made converted PDFs
 * (docs/CONVERTED-PDF.md) for EPDF, on a Falcon or a TT (a 68030; an ST
 * is told so).
 *
 * One window holds the batch (Erez, 31 Oct 2026): the files (Add files,
 * Add folder, Clear), the folder they are written to, the batch's
 * settings (the whole pages: none, mono, mono and colour; the photos: 256
 * colours or JPEG), Convert; and, while it works, two bars, the file and
 * the batch, each with its percentage and the time left, and Stop.
 *
 * The converting runs on a stack of its own (src/gem/coro.S): the
 * converter's turns (its reader's stop hook, its long loops) hand the
 * machine back to the event loop when the slice is up, so the window, the
 * menus and Stop answer while a page is drawn.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "gem.h"
#include "gemui.h"
#include "dialog.h"
#include "alert.h"
#include "aboutbox.h"
#include "aboutline.h"
#include "mbar.h"
#include "help.h"
#include "av.h"
#include "dd.h"
#include "progbar.h"
#include "fsel.h"
#include "scrsize.h"
#include "text.h"
#include "crash.h"
#include "inf.h"
#include "util.h"
#include "plat.h"
#include "coro.h"
#include "conv.h"
#include "pigate.h"
#include "conn.h"
#include "gatedlg.h"
#include "popup.h"
#include "panebar.h"
#include "iconify.h"
#include "tooltip.h"
#include "offdraw.h"
#include "icon.h"
#include "icon16.h"
#include "icon256.h"

extern const char app_version[];
short vdi_h, cw, ch, desk_x, desk_y, desk_w, desk_h, planes, gl_wchar, gl_hchar;	/* (scr_w, scr_h: src/gem/gemui.c) */
static short ap_id, win = -1, quit_now, has_menu;
static char start_dir[160], inf_path[180], font_dir[180];
static INF inf;
static MBAR menu;

void log_event(const char *what, const char *a, const char *b)
{
	char line[200];

	snprintf(line, sizeof line, "ECONVERT %s %s %s\n", what, a ? a : "", b ? b : "");
	nf_debug(line);
}

/* the system font is the only one (gemui.h's: the About box asks) */
void font_sys_push(void)
{
}

void font_sys_pop(void)
{
}

/* ---------------------------------------------------------------- the settings (econvert.inf) */

static const char inf_head[] =
	"; EConvert's settings. EConvert writes this file itself; a line it doesn't know is kept.\r\n\r\n";
static int pages_set = CV_PAGES_MONO, photos_set = CV_PHOTOS_INDEXED;
static char out_dir[160], in_dir[160];
/* Gateway mode (Erez, 7 Nov 2026): the PDFs converted by the Gateway
   (AERA-Gateway's pdfgate) instead of here. A Falcon or a TT converts here
   by default; an ST can only send (its converter needs a 68030) */
static int has_030, on_gateway;
static char gate_host[64];
static unsigned short gate_port = GATE_PORT;

/* no Gateway of its own yet: EPDF's, from epdf.inf in the folder above
   (the release's epdf\econvert\), or beside it (Erez, 8 Nov 2026: the
   converter takes the Gateway EPDF has) */
static void epdf_gateway(void)
{
	char p[200], dir[170];
	const char *where[2];
	int k;

	snprintf(dir, sizeof dir, "%s", start_dir);
	{
		char *b = strrchr(dir, '\\');

		if (b)
			*b = '\0';
	}
	where[0] = dir;
	where[1] = start_dir;
	for (k = 0; k < 2 && !gate_host[0]; k++) {
		INF e;
		long n;
		char *t;
		const char *v;

		path_join(p, sizeof p, where[k], "epdf.inf");
		if (!(t = pf_load(p, &n)))
			continue;
		inf_init(&e);
		inf_parse(&e, t, n);
		free(t);
		if ((v = inf_get(&e, "GATE")) != NULL && v[0] && strlen(v) < sizeof gate_host) {
			strcpy(gate_host, v);
			if ((v = inf_get(&e, "GATEPORT")) != NULL && atol(v) > 0 && atol(v) < 65536)
				gate_port = (unsigned short)atol(v);
			log_event("gateway-from-epdf", gate_host, NULL);
		}
		inf_free(&e);
	}
}

static void settings_read(void)
{
	long n;
	char *t = pf_load(inf_path, &n);
	const char *v;

	inf_init(&inf);
	if (t) {
		inf_parse(&inf, t, n);
		free(t);
	} else
		inf_parse(&inf, inf_head, (long)strlen(inf_head));
	if ((v = inf_get(&inf, "PAGES")) != NULL)
		pages_set = !strcmp(v, "none") ? CV_PAGES_NONE : !strcmp(v, "colour") ? CV_PAGES_COLOUR :
			    !strcmp(v, "colouronly") ? CV_PAGES_COLOUR_ONLY : CV_PAGES_MONO;
	if ((v = inf_get(&inf, "PHOTOS")) != NULL)
		photos_set = !strcmp(v, "jpeg") ? CV_PHOTOS_JPEG : CV_PHOTOS_INDEXED;
	if ((v = inf_get(&inf, "WRITETO")) != NULL && strlen(v) < sizeof out_dir)
		strcpy(out_dir, v);
	if ((v = inf_get(&inf, "FOLDER")) != NULL && strlen(v) < sizeof in_dir)
		strcpy(in_dir, v);
	on_gateway = !has_030;
	if (has_030 && (v = inf_get(&inf, "CONVERT")) != NULL)
		on_gateway = !strcmp(v, "gateway");
	if ((v = inf_get(&inf, "GATE")) != NULL && strlen(v) < sizeof gate_host)
		strcpy(gate_host, v);
	if ((v = inf_get(&inf, "GATEPORT")) != NULL && atol(v) > 0 && atol(v) < 65536)
		gate_port = (unsigned short)atol(v);
	if (!gate_host[0])
		epdf_gateway();
}

static void setting(const char *name, const char *value, const char *note)
{
	const char *old = inf_get(&inf, name);
	long n;
	char *t;

	if (old && !strcmp(old, value))
		return;
	inf_set(&inf, name, value, note);
	if ((t = inf_text(&inf, &n)) != NULL) {
		if (pf_save(inf_path, t, n) != 0)
			log_event("inf-not-written", inf_path, NULL);
		free(t);
	}
}

/* ---------------------------------------------------------------- the batch */

/* GEMDOS's largest free block: the converter's bands' size when a page
   doesn't fit with its canvas whole */
static long mem_free(void)
{
	return (long)Malloc(-1L);
}

/* the sizes this machine shows (Erez, 5 Nov 2026: "the 640 pixel
   limitation is based on the actual user's machine ... figure out
   available display area yourself and adapt to it"): the whole page's
   picture as wide as EPDF's text column in a window the desktop's size
   (its kind and margins: textwin.c's WKIND and MARGIN), and a picture at
   most that window's inside (common's scr_work_size) */
static int sizes_page = 640, sizes_pic = 640;

/* asked of the AES on the main stack when a batch begins (screen_sizes
   is called on the job's stack, where nothing may call the AES) */
static void screen_sizes_measure(void)
{
	short w, h;

	scr_work_size(NAME | CLOSER | FULLER | MOVER | SIZER | UPARROW | DNARROW | VSLIDE, 8, &w, &h);
	sizes_page = w;
	scr_work_size(NAME | CLOSER | FULLER | MOVER | SIZER | UPARROW | DNARROW | VSLIDE, 0, &w, &h);
	sizes_pic = w;
}

static void screen_sizes(int *page_w, int *pic_w)
{
	*page_w = sizes_page;
	*pic_w = sizes_pic;
}

#define MAXF 200
static struct {
	char path[160];
	long bytes;
	int state;			/* 0 to do, 1 done, 2 failed, 3 stopped */
	long out_bytes;			/* the converted PDF's size (done) */
	char why[120];
} files[MAXF];
static int nfiles;
/* how the last batch went, under the bars until the list changes (Erez,
   10 Nov 2026: the result in the window, not an alert; the list keeps every
   file with its mark until Clear) */
static char s_result[80];

static const char *base_of(const char *p)
{
	const char *b = strrchr(p, '\\');

	return b ? b + 1 : p;
}

static long file_size(const char *p)
{
	long h = Fopen(p, 0), n;

	if (h < 0)
		return -1;
	n = Fseek(0L, (short)h, 2);
	Fclose((short)h);
	return n;
}

static int add_file(const char *p)
{
	int i;
	long n;

	for (i = 0; i < nfiles; i++)
		if (!strcmp(files[i].path, p))
			return 0;
	if (nfiles >= MAXF || strlen(p) >= sizeof files[0].path || (n = file_size(p)) < 0)
		return 0;
	s_result[0] = '\0';	/* (the list changed: the last batch's line goes) */
	strcpy(files[nfiles].path, p);
	files[nfiles].bytes = n;
	files[nfiles].state = 0;
	files[nfiles].out_bytes = 0;
	files[nfiles].why[0] = '\0';
	nfiles++;
	return 1;
}

/* every PDF in the folder dir (no trailing \) */
static int add_folder(const char *dir)
{
	char pat[180], p[200];
	DTA *old = Fgetdta(), dta;
	int n = 0;

	Fsetdta(&dta);
	snprintf(pat, sizeof pat, "%s\\*.PDF", dir);
	if (Fsfirst(pat, 0) == 0)
		do {
			snprintf(p, sizeof p, "%s\\%s", dir, dta.d_fname);
			n += add_file(p);
		} while (Fsnext() == 0);
	Fsetdta(old);
	return n;
}

/* ---------------------------------------------------------------- the window's tree */

/* The window (Erez, 9 Nov 2026: "Go with B but use the list management
   of A ... a visual separator just beneath that area ... the same
   separator as the one at the bottom"): a black band with the icon, the
   name and what the batch is; the files in a list, its buttons at the
   right; a line; the settings as pop-up buttons (the family's, 5.7: over
   the button, the choice ticked); a line; the file's and the batch's
   bars under their words; Convert (Stop). */
enum {
	BOX, BAND1, BAND2, ICON, LIST, L0, L1, L2, L3, L4, L5, L6,
	M0, M1, M2, M3, M4, M5, M6, G0, G1, G2, G3, G4, G5, G6,
	B_ADD, B_FOLDER, B_REMOVE, B_CLEAR, B_OUT, SEP1, SEP1W,
	T_PAGES, U_PAGES, T_PHOTOS, U_PHOTOS, T_ON, U_ON, SEP2, SEP2W,
	T_FILE, BAR1, T_BATCH, BAR2, B_GO, NOBJ
};
/* a row of the list: L (its ground and the name), M (the mark), G (the size
   and what came of it, in grey) */
#define NAME_W 15			/* the mark and the name's columns */
#define LROWS 7				/* the list's rows */
#define COLS 66
#define ROWS 21
#define LW (COLS - 19)			/* the list's width */
#define LTEXT (LW - 3)			/* a row's characters (the pane bar at its right) */

static OBJECT tree[NOBJ];
static TEDINFO ted_b1, ted_b2, ted_u[3], ted_row[7], ted_mark[7], ted_rest[7];
static BITBLK icon_blk;
static char s_band1[COLS + 1], s_band2[COLS + 1], s_file[COLS + 1], s_batch[COLS + 1];
static char s_row[LROWS][NAME_W + 1], s_mark[LROWS][2], s_rest[LROWS][LTEXT + 1], s_u[3][28];

/* the mock-up's colours (Erez's B2, 9 Nov 2026: "the color coding is not
   like the mockup"), each the nearest of the first 16 pens as this
   screen has them (pen_nearest: the pens an object can name); on a mono
   screen black and white */
static short p_grey, p_navy, p_row, p_dark, p_green, p_blue, p_red;

/* In 256 colours the mock-up's own navy and light blue: two of the high pens
   set to exactly those while EConvert runs and given back as they were when
   it ends, the crash's way too (the guidelines' 5.2: an app's own theme
   pens; the hub, 10 Nov 2026). Pens 0-15, which objects name, have neither;
   the band and the file under way's row are painted over the tree with them
   (paint256). */
#define PEN_NAVY 250
#define PEN_ROW 251
static short own_pens, pens_was[2][3];
static const short pens_want[2][3] = { { 0, 0, 392 }, { 824, 882, 1000 } };

static void pens_take(void)
{
	if (planes != 8 || own_pens)
		return;
	vq_color(vdi_h, PEN_NAVY, pens_was[0]);
	vq_color(vdi_h, PEN_ROW, pens_was[1]);
	vs_color(vdi_h, PEN_NAVY, pens_want[0]);
	vs_color(vdi_h, PEN_ROW, pens_want[1]);
	own_pens = 1;
}

static void pens_give(void)
{
	if (!own_pens)
		return;
	vs_color(vdi_h, PEN_NAVY, pens_was[0]);
	vs_color(vdi_h, PEN_ROW, pens_was[1]);
	own_pens = 0;
}

static void colours(void)
{
	if (planes < 2) {
		p_grey = p_row = 0;
		p_navy = p_dark = p_green = p_blue = p_red = 1;
		return;
	}
	p_grey = pen_nearest(vdi_h, 16, 753, 753, 753, 0);	/* the window's ground (192) */
	p_navy = pen_nearest(vdi_h, 16, 0, 0, 392, 1);		/* the band (0, 0, 100) */
	p_row = pen_nearest(vdi_h, 16, 824, 882, 1000, 0);	/* the file under way (210, 225, 255) */
	p_dark = pen_nearest(vdi_h, 16, 502, 502, 502, 1);	/* sizes, lines' shadow (128) */
	p_green = pen_nearest(vdi_h, 16, 0, 667, 0, 1);	/* done */
	p_blue = pen_nearest(vdi_h, 16, 0, 0, 784, 1);		/* under way */
	p_red = pen_nearest(vdi_h, 16, 784, 0, 0, 1);		/* failed */
	if (p_row == p_grey)
		p_row = 0;		/* (no light blue near: the row stays white, its mark says it) */
	pens_take();
}

/* a TEDINFO's colour word: its frame and ground pen, its text's pen (the
   text drawn over the ground) */
static short tcolour(short ground, short ink)
{
	return (short)((ground << 12) | (ink << 8) | 0x0070 | ground);
}
static int ltop, lsel = -1;		/* the list's first row shown; the file chosen (Remove) */
static PANEBAR lbar;

static void obj(short i, short parent, short type, short flags, long spec, short x, short y, short w, short h)
{
	OBJECT *o = &tree[i];

	memset(o, 0, sizeof *o);
	o->ob_next = parent;		/* (linked below) */
	o->ob_head = o->ob_tail = -1;
	o->ob_type = type;
	o->ob_flags = flags;
	o->ob_spec = spec;
	o->ob_x = x;
	o->ob_y = y;
	o->ob_width = w;
	o->ob_height = h;
}

/* the children of each parent linked in their order */
static void link_tree(void)
{
	short i, last[NOBJ];

	for (i = 0; i < NOBJ; i++)
		last[i] = -1;
	for (i = 1; i < NOBJ; i++) {
		short p = tree[i].ob_next;

		if (last[p] < 0)
			tree[p].ob_head = i;
		else
			tree[last[p]].ob_next = i;
		last[p] = i;
		tree[p].ob_tail = i;
		tree[i].ob_next = p;	/* (the last child points back to its parent) */
	}
	tree[BOX].ob_next = -1;
	tree[NOBJ - 1].ob_flags |= LASTOB;
}

static void tedi(TEDINFO *t, char *s, short color)
{
	memset(t, 0, sizeof *t);
	t->te_ptext = s;
	t->te_ptmplt = "";
	t->te_pvalid = "";
	t->te_font = 3;
	t->te_color = color;
	t->te_txtlen = (short)(strlen(s) + 1);
	t->te_tmplen = 1;
}

/* a pop-up button: its choice and the system font's down arrow */
static void popup_button(short i, short u, short x, short y, short w)
{
	tedi(&ted_u[u], s_u[u], 0x1180);
	ted_u[u].te_thickness = -1;
	obj(i, BOX, G_BOXTEXT, TOUCHEXIT, (long)&ted_u[u], x, y, w, 1);
}

static void build(void)
{
	short k, line, half = (short)(gl_hchar / 2);

	colours();
	line = (short)(0x1070 | p_dark);	/* (a separator's grey line; black in mono) */
	obj(BOX, 0, G_BOX, 0, 0x00001070L | p_grey, 0, 0, COLS, ROWS);
	/* the band, two rows of navy (black in mono), white words */
	tedi(&ted_b1, s_band1, tcolour(p_navy, 0));
	tedi(&ted_b2, s_band2, tcolour(p_navy, 0));
	obj(BAND1, BOX, G_BOXTEXT, 0, (long)&ted_b1, 0, 0, COLS, 1);
	obj(BAND2, BOX, G_BOXTEXT, 0, (long)&ted_b2, 0, 1, COLS, 1);
	icon_blk.bi_pdata = (void *)icon_bits;
	icon_blk.bi_wb = ICON_W / 8;
	icon_blk.bi_hl = gl_hchar >= 16 ? ICON_H : ICON_H / 2;	/* (8-pixel rows: its top half) */
	icon_blk.bi_color = 0;		/* (white on the band) */
	obj(ICON, BOX, G_IMAGE, 0, (long)&icon_blk, 1, 0, ICON_W / 8, 2);
	if (planes >= 4)
		tree[ICON].ob_flags |= HIDETREE;	/* (the colour icon drawn over the band: draw()) */
	/* the list: white, framed in grey, a row a file */
	obj(LIST, BOX, G_BOX, 0, 0x00FF0070L | ((long)p_dark << 12), 1, (short)(2 | (half << 8)), LW, LROWS);
	for (k = 0; k < LROWS; k++) {
		tedi(&ted_row[k], s_row[k], tcolour(0, 1));
		obj((short)(L0 + k), LIST, G_BOXTEXT, 0, (long)&ted_row[k], 0, k, LTEXT, 1);
	}
	for (k = 0; k < LROWS; k++) {
		tedi(&ted_mark[k], s_mark[k], (short)(1 << 8));	/* (transparent: the row's ground under it) */
		obj((short)(M0 + k), LIST, G_TEXT, 0, (long)&ted_mark[k], 1, k, 1, 1);
	}
	for (k = 0; k < LROWS; k++) {
		tedi(&ted_rest[k], s_rest[k], (short)(p_dark << 8));
		obj((short)(G0 + k), LIST, G_TEXT, 0, (long)&ted_rest[k], NAME_W, k, LTEXT - NAME_W, 1);
	}
	obj(B_ADD, BOX, G_BUTTON, SELECTABLE | EXIT, (long)"Add files...", COLS - 16, (short)(2 | (half << 8)), 15, 1);
	obj(B_FOLDER, BOX, G_BUTTON, SELECTABLE | EXIT, (long)"Add folder...", COLS - 16, (short)(4 | (0 << 8)), 15, 1);
	obj(B_REMOVE, BOX, G_BUTTON, SELECTABLE | EXIT, (long)"Remove", COLS - 16, (short)(5 | (half << 8)), 15, 1);
	obj(B_CLEAR, BOX, G_BUTTON, SELECTABLE | EXIT, (long)"Clear", COLS - 16, 7, 15, 1);
	obj(B_OUT, BOX, G_BUTTON, SELECTABLE | EXIT, (long)"Write to...", COLS - 16, (short)(8 | (half << 8)), 15, 1);
	/* the separators etched: a grey line, a white one under it (the mock-up's) */
	obj(SEP1, BOX, G_BOX, 0, line, 1, (short)(10 | ((half + 2) << 8)), COLS - 2, (short)(0 | (1 << 8)));
	obj(SEP1W, BOX, G_BOX, 0, 0x00000070L, 1, (short)(10 | ((half + 3) << 8)), COLS - 2, (short)(0 | (1 << 8)));
	obj(T_PAGES, BOX, G_STRING, 0, (long)"Whole pages:", 2, 11 | (half << 8), 12, 1);
	popup_button(U_PAGES, 0, 15, (short)(11 | (half << 8)), 18);
	obj(T_PHOTOS, BOX, G_STRING, 0, (long)"Photos:", 36, 11 | (half << 8), 7, 1);
	popup_button(U_PHOTOS, 1, 44, (short)(11 | (half << 8)), 21);
	obj(T_ON, BOX, G_STRING, 0, (long)"Convert on:", 2, 13, 11, 1);
	popup_button(U_ON, 2, 15, 13, 18);
	obj(SEP2, BOX, G_BOX, 0, line, 1, (short)(14 | ((half + 2) << 8)), COLS - 2, (short)(0 | (1 << 8)));
	obj(SEP2W, BOX, G_BOX, 0, 0x00000070L, 1, (short)(14 | ((half + 3) << 8)), COLS - 2, (short)(0 | (1 << 8)));
	obj(T_FILE, BOX, G_STRING, 0, (long)s_file, 2, (short)(15 | (half << 8)), COLS - 4, 1);
	obj(BAR1, BOX, G_BOX, 0, 0x00FF0070L | ((long)p_dark << 12), 2, (short)(16 | (half << 8)), COLS - 4, 1);
	obj(T_BATCH, BOX, G_STRING, 0, (long)s_batch, 2, (short)(17 | ((half + 4) << 8)), COLS - 4, 1);
	obj(BAR2, BOX, G_BOX, 0, 0x00FF0070L | ((long)p_dark << 12), 2, (short)(18 | ((half + 4) << 8)), COLS - 4, 1);
	obj(B_GO, BOX, G_BUTTON, SELECTABLE | EXIT | DEFAULT, (long)"Convert", COLS - 15, ROWS - 1, 13, 1);
	link_tree();
	rsrc_obfix_all(tree, NOBJ);
	tree[BOX].ob_state = 0;
	/* the icon in the band's middle, its words after it */
	tree[ICON].ob_y = (short)((2 * gl_hchar - icon_blk.bi_hl) / 2);
	tree[ICON].ob_width = ICON_W;
	tree[ICON].ob_height = icon_blk.bi_hl;
	tree[LIST].ob_height += 2;	/* (a pixel of white above and below the rows) */
	tree[BAR1].ob_height = tree[BAR2].ob_height = (short)(gl_hchar * 5 / 8);
	tree[BOX].ob_height += (short)(gl_hchar / 2);	/* (air under Convert) */
	for (k = 0; k < LROWS; k++)
		tree[L0 + k].ob_y++, tree[M0 + k].ob_y++, tree[G0 + k].ob_y++;
	dlg_3d(tree);			/* (the buttons the AES's 3D where it has one: 5.6) */
}

/* ---------------------------------------------------------------- the work */

#define CONV_STACK (64L * 1024)		/* FreeType's rasteriser and the reader's interpreter */
static CORO main_co, work_co;
static char *work_stack;
static int working;			/* the batch under way */
static int fresh;			/* just begun: its first slice in the next turn */
static int stop_now;			/* Stop: the next turn says stop */
static int work_done;			/* the batch has finished (its stack is idle) */
static unsigned long slice_end;
static int cur;				/* the file being converted */
static long cur_done, cur_total;	/* its thousandths */
static unsigned long t_file, t_batch;	/* when they began */
static long batch_bytes, bytes_done;
static int n_ok, n_failed;

#ifdef TURN_DEBUG
static unsigned long n_turns;
#endif

static int turn(void *u)
{
#ifdef TURN_DEBUG
	n_turns++;
	static unsigned long last;
	static const char *lastph = "";
	unsigned long now = tos_ms();

	if (last && now - last > 25) {
		char a[60];

		snprintf(a, sizeof a, "%lu ms %s-%s", now - last, lastph, cv_phase);
		log_event("long-turn", a, NULL);
	}
#endif
	(void)u;
	if (tos_ms() >= slice_end)
		coro_switch(&work_co, &main_co);
#ifdef TURN_DEBUG
	last = tos_ms();
	lastph = cv_phase;
#endif
	return stop_now;
}

static int progress_cb(void *u, long done, long total)
{
	(void)u;
	cur_done = done;
	cur_total = total;
	return turn(NULL);
}

/* the folder's own name for a file: the original's name, in out_dir */
static void out_path(const char *in, char *out, long n)
{
	snprintf(out, (size_t)n, "%s\\%s", out_dir, base_of(in));
}

/* ---- Gateway mode: the file sent, the Gateway's progress as the file's
   (sending its first tenth, the Gateway's work the next eight, the
   converted PDF the last), conn.c's waits a turn each (pf_idle) */
static int gate_waiting;		/* the Gateway hasn't begun this one yet */
static int gate_phase = -1;		/* PG_SEND, PG_WORK, PG_RECV; -1 none */
static long gate_part, gate_whole;		/* the bytes of the phase so far, and in all */
static int net_ready;

static void gate_idle(void)
{
	if (turn(NULL))
		conn_stop = 1;
}

static int gate_progress(void *u, int phase, long done, long total)
{
	(void)u;
	if (total <= 0)
		total = 1;
	gate_waiting = phase == PG_WORK && done == 0;
	gate_phase = phase;
	if (phase != PG_WORK) {
		gate_part = done;
		gate_whole = total;
	}
	cur_total = 1000;
	cur_done = phase == PG_SEND ? done * 100 / total : phase == PG_WORK ? 100 + done * 800 / total :
		   900 + done * 100 / total;
	return turn(NULL);
}

static int gate_off;			/* the Gateway didn't answer: the batch stops */
static char gate_why[80];

static int on_the_gateway(const char *in, const char *out, char *err, int errlen)
{
	PGOPT po;
	int pw, iw, r;
	/* (colouronly: AERA-Gateway 1.42 and later) */
	static const char *pages[] = { "none", "mono", "colour", "colouronly" };

	memset(&po, 0, sizeof po);
	po.pages = pages[pages_set];
	po.photos = photos_set == CV_PHOTOS_JPEG ? "jpeg" : "indexed";
	po.pictures = "screen";
	screen_sizes(&pw, &iw);
	po.page_w = pw;
	po.pic_w = iw;
	po.page_double = -1;
	{
		char q[40];

		snprintf(q, sizeof q, "%d %d", pw, iw);	/* (the rigs' direct conversion takes the same) */
		log_event("gateway-sizes", q, NULL);
	}
	conn_stop = 0;
	pf_idle = gate_idle;
	r = pg_convert(gate_host, gate_port, &po, in, out, gate_progress, NULL, err, errlen);
	pf_idle = NULL;
	gate_waiting = 0;
	gate_phase = -1;
	if (r == -3) {
		gate_off = 1;
		snprintf(gate_why, sizeof gate_why, "%s", err);
	}
	return r == -3 ? -1 : r;
}

static void worker(void)
{
	for (;;) {
		int i;

		for (i = 0; i < nfiles && !stop_now; i++) {
			CVOPT o;
			CVSTAT st;
			char outp[200], err[120];
			int r;

			if (files[i].state == 1)
				continue;
			cur = i;
			cur_done = 0;
			cur_total = 1000;
			t_file = tos_ms();
			if (on_gateway) {
				out_path(files[i].path, outp, sizeof outp);
				memset(&st, 0, sizeof st);
				r = on_the_gateway(files[i].path, outp, err, sizeof err);
				goto done_one;
			}
			cv_defaults(&o);	/* (a 68030's: the window allows This Atari only there) */
			o.pages = pages_set;
			o.photos = photos_set;
			o.mem_free = mem_free;	/* (a page refused memory: made again in bands) */
			screen_sizes(&o.page_w, &o.pic_w);
			o.fontdir = font_dir;
			o.progress = progress_cb;
			o.turn = turn;
			out_path(files[i].path, outp, sizeof outp);
			r = cv_convert(files[i].path, outp, &o, &st, err, sizeof err);
done_one:
			files[i].state = r == 0 ? 1 : r == -2 ? 3 : 2;
			files[i].out_bytes = r == 0 ? file_size(outp) : 0;
			if (r == 0)
				n_ok++;
			else if (r == -1) {
				n_failed++;
				strncpy(files[i].why, err, sizeof files[i].why - 1);
			}
			{
				char a[120];

				snprintf(a, sizeof a, "%d %ld ms %ld bytes stack %ld%s%s", r, st.ms, st.out_bytes,
					 coro_used(work_stack, CONV_STACK), r == -1 ? " why " : "", r == -1 ? err : "");
				log_event("converted", base_of(files[i].path), a);
				{
					/* the job's stack, deepest so far, in crash.log's "last
					   status" (Erez's bombs left no record of it) */
					static long most;
					long u = coro_used(work_stack, CONV_STACK);

					if (u > most) {
						char q[64];

						most = u;
						snprintf(q, sizeof q, "the job's stack: %ld of %ld", most, CONV_STACK);
						crash_note(q);
					}
				}
			}
			bytes_done += files[i].bytes;
			if (gate_off)
				break;
		}
		work_done = 1;
		coro_switch(&work_co, &main_co);
	}
}

/* ---------------------------------------------------------------- drawing */

static GRECT work_area(void)
{
	GRECT r;

	wind_get(win, WF_WORKXYWH, &r.x, &r.y, &r.w, &r.h);
	return r;
}

/* "45%, 2 min left" (the family's form: Erez, 9 Nov 2026, D35) from the share done (thousandths) and the time it took */
static void left(char *s, long n, long share, unsigned long since)
{
	unsigned long t = tos_ms() - since, rest;
	int pc = (int)(share / 10);

	if (share < 20 || t < 3000) {
		snprintf(s, (size_t)n, "%d%%", pc);
		return;
	}
	rest = (t / 1000UL) * (1000UL - (unsigned long)share) / (unsigned long)share;
	if (rest >= 120)
		snprintf(s, (size_t)n, "%d%%, %lu min left", pc, (rest + 30) / 60);
	else
		snprintf(s, (size_t)n, "%d%%, %lu s left", pc, rest < 5 ? 5UL : rest);
}

static long share_file(void)
{
	return cur_total > 0 ? cur_done * 1000L / cur_total : 0;
}

static long share_batch(void)
{
	long cur_bytes = working && cur < nfiles ? files[cur].bytes : 0;

	if (batch_bytes <= 0)
		return 0;
	/* (in KB of the files, the one under way by its share) */
	return ((bytes_done >> 10) + (cur_bytes >> 10) * share_file() / 1000) * 1000L / ((batch_bytes >> 10) + 1);
}

/* "4.7 MB", "820 KB" */
static void size_words(char *s, size_t n, long bytes)
{
	if (bytes >= 1024L * 1024L)
		snprintf(s, n, "%ld.%ld MB", bytes / (1024L * 1024L), bytes % (1024L * 1024L) * 10 / (1024L * 1024L));
	else
		snprintf(s, n, "%ld KB", (bytes + 1023) / 1024);
}

/* by CV_PAGES_*; the pop-up's order: None, Mono, Colour, Mono and colour */
static const char *const pages_names[] = { "None", "Mono", "Mono and colour", "Colour" };
static const short pages_order[] = { CV_PAGES_NONE, CV_PAGES_MONO, CV_PAGES_COLOUR_ONLY, CV_PAGES_COLOUR };
static const char *const photos_names[] = { "256 colours (fast)", "JPEG (small)" };
static const char *const on_names[] = { "This Atari", "The Gateway" };

/* a file's row: its mark, name, size and what came of it */
/* row k shows file i: its mark (coloured), name, size and what came of it
   (grey); the file under way on the mock-up's light blue */
static void row_text(int k, int i)
{
	char size[16], what[40] = "";
	char mark = '\xFA';		/* (to do: the middle dot) */
	short ink = p_dark, ground = 0;

	size_words(size, sizeof size, files[i].bytes);
	if (files[i].state == 1) {
		char a[16], b[16];

		mark = '\x08';		/* (the check mark) */
		ink = p_green;
		size_words(a, sizeof a, files[i].bytes);
		size_words(b, sizeof b, files[i].out_bytes);
		snprintf(what, sizeof what, "%s \x03 %s", a, b);
	} else if (files[i].state == 2) {
		mark = 'x';
		ink = p_red;
		snprintf(what, sizeof what, "%.30s", files[i].why[0] ? files[i].why : "not converted");
	} else if (files[i].state == 3)
		snprintf(what, sizeof what, "stopped");
	else if (working && i == cur) {
		mark = '\x03';		/* (under way: the arrow) */
		ink = p_blue;
		ground = p_row;
		if (gate_phase == PG_SEND)
			snprintf(what, sizeof what, "sending");
		else if (gate_phase == PG_WORK)
			snprintf(what, sizeof what, gate_waiting ? "waiting" : "on the Gateway");
		else if (gate_phase == PG_RECV)
			snprintf(what, sizeof what, "receiving");
		else if (cur_total >= 1000 && !on_gateway)
			snprintf(what, sizeof what, "page %ld of %ld", cur_done / 1000 + 1, cur_total / 1000);
	}
	snprintf(s_row[k], sizeof s_row[k], "   %-12.12s", base_of(files[i].path));
	s_mark[k][0] = mark;
	s_mark[k][1] = '\0';
	snprintf(s_rest[k], sizeof s_rest[k], "%8s  %-*.*s", size, LTEXT - NAME_W - 10, LTEXT - NAME_W - 10, what);
	ted_row[k].te_color = tcolour(ground, 1);
	ted_mark[k].te_color = (short)(ink << 8);
}

/* row k with no file: empty, white */
static void row_empty(int k)
{
	s_row[k][0] = s_mark[k][0] = s_rest[k][0] = '\0';
	ted_row[k].te_color = tcolour(0, 1);
}

static void rows_text(void)
{
	int k;

	if (ltop > nfiles - LROWS)
		ltop = nfiles - LROWS;
	if (ltop < 0)
		ltop = 0;
	for (k = 0; k < LROWS; k++) {
		int i = ltop + k;

		if (i < nfiles)
			row_text(k, i);
		else
			row_empty(k);
		tree[L0 + k].ob_state = (short)(i == lsel && i < nfiles ? SELECTED : 0);
		if (i == lsel && i < nfiles) {
			ted_mark[k].te_color = 0;	/* (white on the chosen row's black bar) */
			ted_rest[k].te_color = 0;
		} else
			ted_rest[k].te_color = (short)(p_dark << 8);
	}
}

static void texts(void)
{
	long bytes = 0;
	int i;
	char a[16];

	for (i = 0; i < nfiles; i++)
		bytes += files[i].bytes;
	{
		static int logged = -1;	/* the list's count for the rigs, when it changes */

		if (nfiles != logged) {
			char q[8];

			snprintf(q, sizeof q, "%d", nfiles);
			log_event("files", q, NULL);
			logged = nfiles;
		}
	}
	snprintf(s_band1, sizeof s_band1, "      EConvert");
	size_words(a, sizeof a, bytes);
	if (!nfiles)
		snprintf(s_band2, sizeof s_band2, "      No PDFs yet \xFA %s", on_gateway ? "on the Gateway" : "on this Atari");
	else
		snprintf(s_band2, sizeof s_band2, "      %d PDF%s, %s \xFA into %.24s \xFA %s", nfiles, nfiles == 1 ? "" : "s", a,
			 out_dir[0] ? out_dir : "(Write to...)", on_gateway ? "on the Gateway" : "on this Atari");
	ted_b2.te_txtlen = (short)(strlen(s_band2) + 1);
	rows_text();
	if (lsel >= nfiles)
		lsel = -1;
	if (working || lsel < 0)
		tree[B_REMOVE].ob_state |= DISABLED;
	else
		tree[B_REMOVE].ob_state &= ~DISABLED;
	snprintf(s_u[0], sizeof s_u[0], " %-16s\x02", pages_names[pages_set]);
	snprintf(s_u[1], sizeof s_u[1], " %-19s\x02", photos_names[photos_set]);
	snprintf(s_u[2], sizeof s_u[2], " %-16s\x02", on_names[on_gateway]);
	if (!working) {
		snprintf(s_file, sizeof s_file, "The file");
		snprintf(s_batch, sizeof s_batch, "%s", s_result[0] ? s_result : "The batch");
		tree[B_GO].ob_spec = (long)"Convert";
	} else
		tree[B_GO].ob_spec = (long)"Stop";
}

static void progress_texts(void)
{
	char a[40], l[COLS + 1];
	int w = COLS - 4, n;

	if (!working)
		return;
	left(a, sizeof a, share_file(), t_file);
	/* the three phases (guideline 7.3, Erez 7 Nov 2026) */
	if (gate_phase == PG_SEND || gate_phase == PG_RECV) {
		char p1[16], p2[16];	/* "12 KB of 3.4 MB" (D35) */

		size_words(p1, sizeof p1, gate_part);
		size_words(p2, sizeof p2, gate_whole);
		snprintf(l, sizeof l, "%.12s  %s the Gateway... %s of %s", base_of(files[cur].path),
			 gate_phase == PG_SEND ? "Sending to" : "Receiving from", p1, p2);
	}
	else if (gate_phase == PG_WORK)
		snprintf(l, sizeof l, "%.12s  %s", base_of(files[cur].path),
			 gate_waiting ? "Waiting for the Gateway" : "The Gateway is converting...");
	else if (cur_total >= 1000)
		snprintf(l, sizeof l, "%.12s  page %ld of %ld", base_of(files[cur].path), cur_done / 1000 + 1,
			 cur_total / 1000);
	else
		snprintf(l, sizeof l, "%.12s", base_of(files[cur].path));
	n = w - (int)strlen(a) - 1;
	snprintf(s_file, sizeof s_file, "%-*.*s %s", n, n, l, a);
	left(a, sizeof a, share_batch(), t_batch);
	snprintf(l, sizeof l, "The batch  %d of %d", cur + 1, nfiles);
	n = w - (int)strlen(a) - 1;
	snprintf(s_batch, sizeof s_batch, "%-*.*s %s", n, n, l, a);
	/* the file's row says the same (its page) */
	if (cur >= ltop && cur < ltop + LROWS)
		row_text(cur - ltop, cur);
}

/* the file under way changed: the one before it is done (its row says
   so), the new one in view */
static int shown_cur = -1;

static int list_follow(void)
{
	if (!working || cur == shown_cur)
		return 0;
	shown_cur = cur;
	if (cur >= ltop + LROWS)
		ltop = cur - LROWS + 1;
	rows_text();
	return 1;
}

static void bar(short o, long share, const GRECT *clip)
{
	short x, y;
	long to = share > 1000 ? PB_FULL : share * PB_FULL / 1000;

	(void)clip;
	objc_offset(tree, o, &x, &y);
	if (to > 0)
		progbar_fill(vdi_h, x, y, tree[o].ob_width, tree[o].ob_height, 0, (short)to);
}

static int meet(const GRECT *a, GRECT *b)
{
	short x0 = a->x > b->x ? a->x : b->x, y0 = a->y > b->y ? a->y : b->y;
	short x1 = a->x + a->w < b->x + b->w ? a->x + a->w : b->x + b->w;
	short y1 = a->y + a->h < b->y + b->h ? a->y + a->h : b->y + b->h;

	if (x1 <= x0 || y1 <= y0)
		return 0;
	b->x = x0;
	b->y = y0;
	b->w = (short)(x1 - x0);
	b->h = (short)(y1 - y0);
	return 1;
}

/* the list's pane bar, at its right, when it has more files than rows */
static int list_bar(void)
{
	short x, y;

	objc_offset(tree, LIST, &x, &y);
	lbar.r.w = pb_width();
	lbar.r.x = (short)(x + tree[LIST].ob_width - lbar.r.w);
	lbar.r.y = y;
	lbar.r.h = tree[LIST].ob_height;
	lbar.total = nfiles;
	lbar.shown = LROWS;
	lbar.top = ltop;
	lbar.across = 0;
	return nfiles > LROWS;
}

/* iconified (5.1): the icon, and how far the batch has got under it */
static int iconified;

static const char *icon_status(void)
{
	static char st[8];

	if (!working)
		return nfiles ? "ready" : "";
	snprintf(st, sizeof st, "%ld%%", share_batch() / 10);
	return st;
}

/* words at x, y (the system font, from their top), pen ink, over what is there */
static void words(short x, short y, const char *t, short ink)
{
	if (!*t)
		return;
	vswr_mode(vdi_h, 2);
	vst_color(vdi_h, ink);
	vst_effects(vdi_h, 0);
	vst_alignment(vdi_h, 0, 5);
	v_gtext_n(vdi_h, x, y, t, (short)strlen(t));
	vswr_mode(vdi_h, 1);
}

static void box256(short x, short y, short w, short h, short pen)
{
	short p[4];

	p[0] = x;
	p[1] = y;
	p[2] = (short)(x + w - 1);
	p[3] = (short)(y + h - 1);
	vswr_mode(vdi_h, 1);
	vsf_interior(vdi_h, 1);
	vsf_color(vdi_h, pen);
	vsf_perimeter(vdi_h, 0);
	vr_recfl(vdi_h, p);
}

/* 256 colours: the band in the mock-up's navy and the file under way's row
   in its light blue, over what the AES drew (within the clip set) */
static void paint256(void)
{
	short x, y, k;

	if (!own_pens)
		return;
	objc_offset(tree, BAND1, &x, &y);
	box256(x, y, tree[BAND1].ob_width, (short)(2 * tree[BAND1].ob_height), PEN_NAVY);
	words(x, y, s_band1, 0);
	words(x, (short)(y + tree[BAND1].ob_height), s_band2, 0);
	if (!working || cur < ltop || cur >= ltop + LROWS || lsel == cur)
		return;
	k = (short)(cur - ltop);
	objc_offset(tree, (short)(L0 + k), &x, &y);
	box256(x, y, tree[L0 + k].ob_width, tree[L0 + k].ob_height, PEN_ROW);
	words(x, y, s_row[k], 1);
	objc_offset(tree, (short)(M0 + k), &x, &y);
	words(x, y, s_mark[k], p_blue);
	objc_offset(tree, (short)(G0 + k), &x, &y);
	words(x, y, s_rest[k], p_dark);
}

/* 0 while a piece is drawn off the screen (offdraw.h): the caller has the
   mouse hidden from off_begin to off_end, so draw() leaves it alone */
static int draw_mouse = 1;

/* a holds b whole; a and b meet */
static int holds(const GRECT *a, const GRECT *b)
{
	return a->x <= b->x && a->y <= b->y && a->x + a->w >= b->x + b->w && a->y + a->h >= b->y + b->h;
}

static int meets(const GRECT *a, const GRECT *b)
{
	return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

/* set: draw() draws only these objects, each without its children (the
   window's whole tree walked for a strip of the progress was 35-40 ms of
   the AES's on Falcon-4, three strips a second) */
static const short *draw_only;
static short draw_only_n;

/* the file's and the batch's strips as last drawn: their words, padded,
   and how much of their bar (PB_FULLths; -1 not known) */
static char shown_txt[2][COLS + 1];
static short strip_now(int k, char *now, short *to, GRECT *g, short *bx, short *by);
static short shown_to[2] = { -1, -1 };
static char shown_rest[LTEXT + 1];	/* the file under way's row's words as last drawn */

/* a line of the system font's letters at x, y (n of them, now) drawn within
   clip: was takes the letters drawn whole, a letter drawn in part is
   marked not known (0xFF); 0 the line's height not drawn whole */
static int note_line(const GRECT *clip, short x, short y, const char *now, char *was, short n)
{
	short c0, c1, i;

	if (clip->y > y || clip->y + clip->h < y + gl_hchar)
		return clip->y >= y + gl_hchar || clip->y + clip->h <= y;	/* (not met: as it was) */
	c0 = clip->x <= x ? 0 : (short)((clip->x - x) / gl_wchar);
	c1 = (short)((clip->x + clip->w - x + gl_wchar - 1) / gl_wchar);
	if (c1 > n)
		c1 = n;
	for (i = c0; i < c1; i++) {
		short l = (short)(x + i * gl_wchar);

		was[i] = l >= clip->x && l + gl_wchar <= clip->x + clip->w ? now[i] : (char)0xFF;
	}
	return 1;
}

/* what a redraw within clip leaves shown: the strips' words and bars, the
   row under way's words (strip() and redraw_progress() draw from these
   only what changed) */
static void note_drawn(const GRECT *clip)
{
	int k;

	for (k = 0; k < 2; k++) {
		short to, bx, by, n;
		char now[COLS + 1];
		GRECT g, b;

		n = strip_now(k, now, &to, &g, &bx, &by);
		if (!meets(clip, &g) || shown_to[k] < 0) {
			if (holds(clip, &g)) {
				memcpy(shown_txt[k], now, sizeof now);
				shown_to[k] = to;
			}
			continue;
		}
		b.x = (short)(bx - 1);
		b.y = (short)(by - 1);
		b.w = (short)(tree[k ? BAR2 : BAR1].ob_width + 2);
		b.h = (short)(tree[k ? BAR2 : BAR1].ob_height + 2);
		if (!note_line(clip, g.x, g.y, now, shown_txt[k], n) || (to != shown_to[k] && meets(clip, &b)))
			shown_to[k] = -1;
	}
	if (working && cur >= ltop && cur < ltop + LROWS && shown_rest[0]) {
		short n = (short)strlen(s_rest[cur - ltop]), x, y;

		objc_offset(tree, (short)(G0 + cur - ltop), &x, &y);
		if ((short)strlen(shown_rest) != n || !note_line(clip, x, y, s_rest[cur - ltop], shown_rest, n))
			shown_rest[0] = '\0';
	} else if (working && cur >= ltop && cur < ltop + LROWS) {
		GRECT g;

		objc_offset(tree, (short)(G0 + cur - ltop), &g.x, &g.y);
		g.w = tree[G0 + cur - ltop].ob_width;
		g.h = tree[G0 + cur - ltop].ob_height;
		if (holds(clip, &g))
			snprintf(shown_rest, sizeof shown_rest, "%s", s_rest[cur - ltop]);
	}
}

static void draw(const GRECT *clip, short start)
{
	if (iconified) {
		icon_redraw(win, clip, icon_bits, icon16_px, icon_status());
		return;
	}
	GRECT a = work_area(), r;

	if (!draw_only)
		note_drawn(clip);

	tree[BOX].ob_x = a.x;
	tree[BOX].ob_y = a.y;
	if (own_pens && start == BOX && clip->w >= a.w && clip->h >= a.h) {
		/* a whole redraw: our two pens set again, should another program
		   have set the palette meanwhile (the hub, 10 Nov 2026; the values
		   to give back are the ones read once, at pens_take) */
		vs_color(vdi_h, PEN_NAVY, pens_want[0]);
		vs_color(vdi_h, PEN_ROW, pens_want[1]);
	}
	wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	if (draw_mouse)
		graf_mouse(M_OFF, 0);
	while (r.w && r.h) {
		GRECT c = r;

		if (meet(clip, &c)) {
			short p[4];

			if (draw_only) {
				short i;

				for (i = 0; i < draw_only_n; i++)
					objc_draw(tree, draw_only[i], 0, c.x, c.y, c.w, c.h);
			} else
				objc_draw(tree, start, 8, c.x, c.y, c.w, c.h);
			if (own_pens) {
				short p[4];

				p[0] = c.x;
				p[1] = c.y;
				p[2] = (short)(c.x + c.w - 1);
				p[3] = (short)(c.y + c.h - 1);
				vs_clip(vdi_h, 1, p);
				paint256();
				vs_clip(vdi_h, 0, p);
			}
			if (planes >= 4 && gl_hchar >= 16) {
				/* the colour icon over the band (a raster copy keeps to no
				   clip: drawn where its whole square is in this one) */
				short ix, iy;

				objc_offset(tree, ICON, &ix, &iy);
				iy = (short)(tree[BOX].ob_y + (2 * gl_hchar - ICON_H) / 2);
				if (ix >= c.x && iy >= c.y && ix + ICON_W <= c.x + c.w && iy + ICON_H <= c.y + c.h)
					about_icon16(icon16_px, ix, iy, 1, 1, 0);
			}
			p[0] = c.x;
			p[1] = c.y;
			p[2] = (short)(c.x + c.w - 1);
			p[3] = (short)(c.y + c.h - 1);
			vs_clip(vdi_h, 1, p);
			if (working) {
				bar(BAR1, share_file(), &c);
				bar(BAR2, share_batch(), &c);
			}
			vs_clip(vdi_h, 0, p);
			if (list_bar())
				pb_draw(&lbar, &c);

		}
		wind_get(win, WF_NEXTXYWH, &r.x, &r.y, &r.w, &r.h);
	}
	if (draw_mouse)
		graf_mouse(M_ON, 0);
}

static void redraw_obj(short o)
{
	GRECT g;

	if (iconified)
		return;		/* (its icon: redraw_progress) */
	objc_offset(tree, o, &g.x, &g.y);
	/* 3 pixels more at the right and the foot: TOS's VDI draws text the
	   slow way where the clip ends less than 2 past it (text.h; the list
	   45 -> ... ms on Falcon-4), and what is there is the tree's too */
	g.w = (short)(tree[o].ob_width + 3);
	g.h = (short)(tree[o].ob_height + 3);
	wind_update(BEG_UPDATE);
	draw(&g, BOX);
	wind_update(END_UPDATE);
}

/* the file's line and bar to the batch's, one rectangle drawn once
   (four objects drawn one by one were 80 ms of a Falcon's turn) */

/* g drawn off the screen and shown in one copy (offdraw.h: Erez, 10 Nov
   2026, "The converter UI statistics flicker every second"): the ground,
   the words and the bar of a row all come at once. Where TOS's own screen
   can't be borrowed (MiNT, MagiC, NVDI that won't follow it, a piece too
   tall) it is drawn on the screen as before */
static void draw_steady(const GRECT *g, const short *objs, short n)
{
	draw_only = objs;
	draw_only_n = n;
	wind_update(BEG_UPDATE);
	graf_mouse(M_OFF, 0);
	draw_mouse = 0;
	if (g->h <= OFF_ROWS && off_begin(vdi_h, g->x, g->y, g->w, g->h)) {
		draw(g, BOX);
		off_end();
	} else
		draw(g, BOX);
	draw_mouse = 1;
	draw_only = NULL;
	graf_mouse(M_ON, 0);
	wind_update(END_UPDATE);
}

/* where now and was (n letters each) differ: [*a, *e) */
static void span(const char *now, const char *was, short n, short *a, short *e)
{
	short i = 0, j = n;

	while (i < n && now[i] == was[i])
		i++;
	while (j > i && now[j - 1] == was[j - 1])
		j--;
	*a = i;
	*e = j;
}

/* the letters a..e of a line of the system font at x, y on ground in ink,
   drawn off the screen and shown in one copy; was takes them. The caller
   holds wind_update and has the mouse hidden */
static void cells(short x, short y, char *now, char *was, short a, short e, short ground, short ink)
{
	short w = (short)((e - a) * gl_wchar), p[4];
	int off;

	if (a >= e)
		return;
	x = (short)(x + a * gl_wchar);
	off = off_begin(vdi_h, x, y, w, gl_hchar);
	p[0] = x;
	p[1] = y;
	p[2] = (short)(x + w - 1);
	p[3] = (short)(y + gl_hchar - 1);
	box256(x, y, w, gl_hchar, ground);
	/* (text.c's: TOS's VDI draws text the slow way with a clip tight round
	   it, 25 ms a line here) */
	text_clip(vdi_h, p);
	vswr_mode(vdi_h, 2);
	vst_color(vdi_h, ink);
	vst_effects(vdi_h, 0);
	vst_alignment(vdi_h, 0, 5);
	text_n(vdi_h, x, y, now + a, e - a);
	vswr_mode(vdi_h, 1);
	text_noclip(vdi_h);
	if (off)
		off_end();
	memcpy(was + a, now + a, (size_t)(e - a));
}

/* the window wholly on top over g (its first rectangle holds it) */
static int on_top(const GRECT *g)
{
	GRECT r;

	wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	return holds(&r, g);
}

/* a strip, the words over their bar: only the letters that changed (the
   system font's cells, on the window's ground, drawn off the screen and
   shown in one copy) and the bar's new part. The AES's G_STRING drawn
   whole was 25-30 ms of a Falcon-4's turn each, two strips a second. The
   whole strip (draw_steady) when it isn't known or the window isn't
   wholly on top there */
/* strip k's words padded to its width (n letters), its share in
   PB_FULLths, and its rectangle with the bar's place */
static short strip_now(int k, char *now, short *to, GRECT *g, short *bx, short *by)
{
	short t = k ? T_BATCH : T_FILE, b = k ? BAR2 : BAR1, n = tree[t].ob_width / gl_wchar, i;
	long share = k ? share_batch() : share_file();
	const char *s = (const char *)tree[t].ob_spec;

	if (n > COLS)
		n = COLS;
	for (i = 0; i < n && s[i]; i++)
		now[i] = s[i];
	for (; i < n; i++)
		now[i] = ' ';
	now[n] = '\0';
	*to = (short)(share > 1000 ? PB_FULL : share < 0 ? 0 : share * PB_FULL / 1000);
	if (!working)
		*to = 0;
	objc_offset(tree, t, &g->x, &g->y);
	objc_offset(tree, b, bx, by);
	g->w = tree[t].ob_width;
	g->h = (short)(*by + tree[b].ob_height + 2 - g->y);
	return n;
}

static void strip(int k)
{
	static short objs[2][3] = { { BOX, T_FILE, BAR1 }, { BOX, T_BATCH, BAR2 } };
	short b = k ? BAR2 : BAR1, to, a, e, bx, by, n;
	char now[COLS + 1];
	GRECT g;

	n = strip_now(k, now, &to, &g, &bx, &by);
	if (shown_to[k] < 0 || !on_top(&g)) {
		draw_steady(&g, objs[k], 3);
		memcpy(shown_txt[k], now, (size_t)n + 1);
		shown_to[k] = to;
		return;
	}
	span(now, shown_txt[k], n, &a, &e);
	if (a == e && to == shown_to[k])
		return;
	wind_update(BEG_UPDATE);
	graf_mouse(M_OFF, 0);
	cells(g.x, g.y, now, shown_txt[k], a, e, p_grey, 1);
	if (to < shown_to[k]) {
		/* (a new file, or the batch's end: the bar emptied, and filled
		   again, in one copy) */
		short bw = tree[b].ob_width, bh = tree[b].ob_height;
		int off = off_begin(vdi_h, bx, by, bw, bh);

		box256(bx, by, bw, bh, 0);
		if (to > 0)
			progbar_fill(vdi_h, bx, by, bw, bh, 0, to);
		if (off)
			off_end();
		shown_to[k] = to;
	}
	if (working && to > shown_to[k]) {
		/* (growing: drawn over its white, no ground first) */
		progbar_fill(vdi_h, bx, by, tree[b].ob_width, tree[b].ob_height, shown_to[k], to);
		shown_to[k] = to;
	}
	graf_mouse(M_ON, 0);
	wind_update(END_UPDATE);
}

static void redraw_progress(void)
{
	short row_objs[3];

	if (iconified) {
		wind_update(BEG_UPDATE);
		icon_redraw(win, NULL, icon_bits, icon16_px, icon_status());
		wind_update(END_UPDATE);
		return;
	}
	/* the file's words and bar, then the batch's */
	strip(0);
	strip(1);
	if (list_follow())
		redraw_obj(LIST);	/* (the file under way changed; draw() notes its row) */
	else if (cur >= ltop && cur < ltop + LROWS && strcmp(s_rest[cur - ltop], shown_rest)) {
		/* the row only when its words changed (its page): drawn at every
		   tick it made a batch's turns 135 ms on Falcon-4; and of it only
		   the letters that changed (the AES's three objects were 70-80 ms) */
		short k = (short)(cur - ltop), n = (short)strlen(s_rest[k]), a, e;
		GRECT rg;

		objc_offset(tree, (short)(L0 + k), &rg.x, &rg.y);
		rg.w = tree[L0 + k].ob_width;
		rg.h = tree[L0 + k].ob_height;
		if (shown_rest[0] && (short)strlen(shown_rest) == n && lsel != cur && rg.h == gl_hchar && on_top(&rg)) {
			short gx, gy;

			objc_offset(tree, (short)(G0 + k), &gx, &gy);
			span(s_rest[k], shown_rest, n, &a, &e);
			wind_update(BEG_UPDATE);
			graf_mouse(M_OFF, 0);
			cells(gx, gy, s_rest[k], shown_rest, a, e, own_pens ? PEN_ROW : p_row, p_dark);
			graf_mouse(M_ON, 0);
			wind_update(END_UPDATE);
		} else {
			row_objs[0] = (short)(L0 + k);
			row_objs[1] = (short)(M0 + k);
			row_objs[2] = (short)(G0 + k);
			draw_steady(&rg, row_objs, 3);
			snprintf(shown_rest, sizeof shown_rest, "%s", s_rest[k]);
		}
	}
}

/* ---------------------------------------------------------------- the commands */

static void begin(void);

/* the list, its buttons and the band (what a change to the files moves) */
static void redraw_list(void)
{
	redraw_obj(BAND2);
	redraw_obj(LIST);
	redraw_obj(B_REMOVE);
}

static int pick(const char *title, char *dir, long ndir, char *name, long nname)
{
	char path[200];
	short button;

	snprintf(path, sizeof path, "%s\\*.PDF", dir[0] ? dir : "C:");
	if (!fsel_exinput(path, name, &button, title) || (fsel_settle(), !button))	/* (its clicks left: fsel.h) */
		return 0;
	{
		char *s = strrchr(path, '\\');

		if (!s)
			return 0;
		*s = '\0';
	}
	strncpy(dir, path, (size_t)ndir - 1);
	dir[ndir - 1] = '\0';
	(void)nname;
	return 1;
}

static void cmd_add(void)
{
	static char name[16];
	char p[200];

	if (working || !pick("Add a PDF to the batch", in_dir, sizeof in_dir, name, sizeof name) || !name[0])
		return;
	setting("FOLDER", in_dir, "the folder the file selector shows first");
	snprintf(p, sizeof p, "%s\\%s", in_dir, name);
	if (add_file(p)) {
		texts();
		redraw_list();
	}
}

static void cmd_folder(void)
{
	char name[16] = "";

	if (working || !pick("Add every PDF in a folder", in_dir, sizeof in_dir, name, sizeof name))
		return;
	setting("FOLDER", in_dir, "the folder the file selector shows first");
	if (!add_folder(in_dir))
		alert(1, "[1][No PDF in that folder|could be added.][ OK ]");
	texts();
	redraw_list();
}

static void cmd_out(void)
{
	char name[16] = "";
	char d[160];

	if (working)
		return;
	strcpy(d, out_dir[0] ? out_dir : in_dir);
	if (!pick("The folder to write to", d, sizeof d, name, sizeof name))
		return;
	strcpy(out_dir, d);
	setting("WRITETO", out_dir, "the folder the converted PDFs are written to");
	texts();
	redraw_obj(BAND2);
}

static int settings_dialog(void);

static void begin(void)
{
	int i, there = 0;
	char p[200];

	s_result[0] = '\0';
	if (!nfiles) {
		alert(1, "[1][Add the PDFs to convert first:|Add files... or Add folder...][ OK ]");
		return;
	}
	if (!out_dir[0]) {
		cmd_out();
		if (!out_dir[0])
			return;
	}
	if (on_gateway) {
		if (!gate_host[0] && !settings_dialog())
			return;
		if (!gate_host[0])
			return;
		if (!net_ready && !(net_ready = net_init())) {
			alert(1, "[1][The Gateway is reached through STinG, MiNTnet or MagiC-Net, and none of them is running.][ OK ]");
			return;
		}
	}
	gate_off = 0;
	for (i = 0; i < nfiles; i++) {
		char d[160];

		strncpy(d, files[i].path, sizeof d - 1);
		d[sizeof d - 1] = '\0';
		if (strrchr(d, '\\'))
			*strrchr(d, '\\') = '\0';
		if (!strcmp(d, out_dir)) {
			alert(1, "[1][The converted PDFs need a folder|of their own, not the PDFs' own.|Choose another with Write to...][ OK ]");
			return;
		}
		out_path(files[i].path, p, sizeof p);
		if (files[i].state != 1 && file_size(p) >= 0)
			there++;
	}
	if (there && alert(2, "[2][%d of the files are in the folder|already. Replace them?][ Replace |Cancel]", there) != 1)
		return;
	if (!work_stack && !(work_stack = malloc(CONV_STACK))) {
		alert(1, "[3][Not enough memory to start.][ OK ]");
		return;
	}
	screen_sizes_measure();
	coro_start(&work_co, work_stack, CONV_STACK, worker);
	batch_bytes = bytes_done = 0;
	for (i = 0; i < nfiles; i++)
		if (files[i].state != 1)
			batch_bytes += files[i].bytes;
	n_ok = n_failed = 0;
	stop_now = 0;
	work_done = 0;
	working = 1;
	cur = 0;
	t_batch = tos_ms();
	log_event("batch", NULL, NULL);
	texts();
	redraw_obj(B_GO);		/* (Stop; the bars come in the turns after: the key's turn short) */
	fresh = 2;
}

/* the batch's end, its first turn: the button says Convert again */
static void finish_button(void)
{
	tree[B_GO].ob_spec = (long)"Convert";
	redraw_obj(B_GO);
}

/* its second: the progress lines and bars cleared (only what changed:
   the whole window was 135 ms of a Falcon's turn) */
static void finish_progress(void)
{
	int i, n_bad = 0;
	unsigned long t = (tos_ms() - t_batch) / 1000UL;
	char took[16];

	working = 0;
	shown_cur = -1;
	shown_rest[0] = '\0';
	for (i = 0; i < nfiles; i++)
		if (files[i].state == 2)
			n_bad++;
	if (gate_off) {
		log_event("gateway-off", gate_why, NULL);
		for (i = 0; i < nfiles; i++)
			if (files[i].state == 2 && !strcmp(files[i].why, gate_why)) {
				files[i].state = 0;	/* (not its fault: kept, to try again) */
				n_bad--;
			}
	}
	snprintf(took, sizeof took, "%lu:%02lu", t / 60, t % 60);
	if (stop_now)
		snprintf(s_result, sizeof s_result, "Stopped: %d converted in %s", n_ok, took);
	else if (n_bad > 0)
		snprintf(s_result, sizeof s_result, "%d converted, %d failed in %s", n_ok, n_bad, took);
	else
		snprintf(s_result, sizeof s_result, "%d converted in %s", n_ok, took);
	log_event("result", s_result, NULL);
	texts();
	redraw_progress();
	log_event("batch-done", stop_now ? "stopped" : "done", NULL);
#ifdef TURN_DEBUG
	{
		char q[24];

		snprintf(q, sizeof q, "%lu", n_turns);
		log_event("turns", q, NULL);
	}
#endif
}

/* its third: what came of it, said under the bars ("3 converted, 1 failed
   in 4:12"; Erez, 10 Nov 2026: in the window, no alert), every file left in
   the list with its mark. Only a Gateway that doesn't answer is an alert:
   the user has something to do */
static void finished(void)
{
	if (gate_off)
		redraw_list();
	else {
		/* only the file that was under way has a new row (the ones
		   before it had theirs as the batch went: 110 -> ... ms of the
		   end's one turn on Falcon-4); Remove and the band as they are
		   now */
		int i;

		for (i = cur - 1; i <= cur; i++)
			if (i >= ltop && i < ltop + LROWS && i < nfiles) {
				short o[3] = { (short)(L0 + i - ltop), (short)(M0 + i - ltop), (short)(G0 + i - ltop) };
				GRECT g;

				objc_offset(tree, o[0], &g.x, &g.y);
				g.w = tree[o[0]].ob_width;
				g.h = tree[o[0]].ob_height;
				draw_steady(&g, o, 3);
			}
		redraw_obj(B_REMOVE);
	}
	if (gate_off)
		alert(1, "[1][The Gateway at %.24s doesn't answer. Check that it is on and that the PDF converter is installed on it.][ OK ]", gate_host);
}

static void about_box(void)
{
	static char mem[100];
	static char net[100];
	ABOUT a = { "EConvert", app_version, "PDFs prepared for EPDF on every Atari",
		    { net, mem, NULL }, { 0, 0, 0 }, icon_bits, icon16_px, icon16_rgb, icon256_px, icon256_rgb };

	about_mem_line(mem, (int)sizeof mem);
	/* the connection (1.3), when the batch goes to the Gateway */
	net[0] = '\0';
	if (on_gateway) {
		if (!net_ready)
			net_ready = net_init();
		about_net_line(net, (int)sizeof net, net_stack(), gate_host[0] ? ABOUT_GATEWAY : ABOUT_NO_GATEWAY,
			       working ? ABOUT_ONLINE : ABOUT_READY);
	}
	if (!net[0]) {
		a.status[0] = mem;
		a.status[1] = NULL;
	}
	if (!a.status[0][0])
		a.status[0] = NULL;
	log_event("dialog", "about", app_version);
	about_show(&a);
}

enum { CMD_NONE, CMD_ABOUT, CMD_ADD, CMD_FOLDER, CMD_REMOVE, CMD_CLEAR, CMD_OUT, CMD_GO, CMD_QUIT, CMD_GUIDE, CMD_SETTINGS };

static const MBAR_ITEM desk_items[] = { { "About EConvert...", NULL, CMD_ABOUT }, { NULL, NULL, CMD_NONE } };
static const MBAR_ITEM file_items[] = {
	{ "Add files...", "^O", CMD_ADD },
	{ "Add folder...", NULL, CMD_FOLDER },
	{ "Remove", NULL, CMD_REMOVE },
	{ "Clear the list", NULL, CMD_CLEAR },
	{ "-", NULL, CMD_NONE },
	{ "Write to...", NULL, CMD_OUT },
	{ "Convert", "^R", CMD_GO },
	{ "-", NULL, CMD_NONE },
	{ "Quit", "^Q", CMD_QUIT },
	{ NULL, NULL, CMD_NONE } };
/* Options as the family's: Settings... (^,) first (the hub's review, 9 Nov 2026) */
static const MBAR_ITEM options_items[] = { { "Settings...", "^,", CMD_SETTINGS }, { NULL, NULL, CMD_NONE } };
static const MBAR_ITEM help_items[] = { { "EConvert help", "Help", CMD_GUIDE }, { NULL, NULL, CMD_NONE } };
static const MBAR_TITLE titles[] = { { " EConvert ", desk_items }, { " File ", file_items }, { " Options ", options_items },
				     { " Help ", help_items }, { NULL, NULL } };

/* Options > Settings... (^,): where the Gateway is; 1 when OK */
static int settings_dialog(void)
{
	char port[8];

	if (working)
		return 0;
	if (!gate_dialog(gate_host, (int)sizeof gate_host, &gate_port))
		return 0;
	setting("GATE", gate_host, "the Gateway's address (Through the Gateway)");
	snprintf(port, sizeof port, "%u", (unsigned)gate_port);
	setting("GATEPORT", port, "the Gateway's port for PDFs (8380)");
	return 1;
}

static void command(int c)
{
	switch (c) {
	case CMD_SETTINGS:
		settings_dialog();
		break;
	case CMD_ABOUT:
		about_box();
		break;
	case CMD_ADD:
		cmd_add();
		break;
	case CMD_FOLDER:
		cmd_folder();
		break;
	case CMD_REMOVE:
		if (!working && lsel >= 0 && lsel < nfiles) {
			int i;

			s_result[0] = '\0';
			for (i = lsel; i < nfiles - 1; i++)
				files[i] = files[i + 1];
			nfiles--;
			if (lsel >= nfiles)
				lsel = nfiles - 1;
			texts();
			redraw_list();
		}
		break;
	case CMD_CLEAR:
		if (!working) {
			nfiles = 0;
			lsel = -1;
			s_result[0] = '\0';
			texts();
			redraw_list();
		}
		break;
	case CMD_OUT:
		cmd_out();
		break;
	case CMD_GO:
		if (working)
			stop_now = 1;
		else
			begin();
		break;
	case CMD_QUIT:
		if (working && alert(2, "[2][A batch is under way.|Stop it and quit?][ Quit |Go on]") != 1)
			break;
		stop_now = 1;
		quit_now = 1;
		break;
	case CMD_GUIDE:
		/* (one guide for both: EPDF's, its page on EConvert; in the
		   EPDF folder above econvert\ or in C:\GUIDES) */
		{
			char up[160], p[180];

			strcpy(up, start_dir);
			path_join(p, sizeof p, start_dir, "epdf.hyp");
			if (Fattrib(p, 0, 0) < 0 && strrchr(up, '\\') && strrchr(up, '\\') > up + 2)
				*strrchr(up, '\\') = '\0';	/* (the folder above: EPDF's) */
			if (help_key(up, "epdf.hyp", "EConvert", "About"))
				about_box();
		}
		break;
	}
}

/* the list scrolled by n rows (its pane bar) */
static int list_scroll(long n)
{
	int was = ltop;

	ltop += (int)n;
	texts();
	if (ltop == was)
		return 0;
	redraw_obj(LIST);
	return 1;
}

static int list_goto(long t)
{
	return list_scroll(t - ltop);
}

/* a pop-up button's choices, over it with the one chosen on it and
   ticked (5.7, as EPDF's Settings and EHypView's) */
static void choose(short o)
{
	static char lab[4][28];
	const char *l[4];
	const char *const *names = o == U_PAGES ? pages_names : o == U_PHOTOS ? photos_names : on_names;
	short n = o == U_PAGES ? 4 : 2, sel = (short)(o == U_PHOTOS ? photos_set : on_gateway);
	short x, y, k, r;

	if (o == U_PAGES)
		for (k = 0; k < n; k++)
			if (pages_order[k] == pages_set)
				sel = k;
	for (k = 0; k < n; k++) {
		/* (an ST converts only on the Gateway: This Atari greyed) */
		snprintf(lab[k], sizeof lab[k], "%s%s", o == U_ON && k == 0 && !has_030 ? "~" :
			 k == sel ? "\x08" : "", names[o == U_PAGES ? pages_order[k] : k]);	/* (popup_m: the box adds the margin, D15) */
		l[k] = lab[k];
	}
	objc_offset(tree, o, &x, &y);
	r = popup_m(x, (short)(y - 3 - sel * (ch + 2)), l, n);
	if (!popup_put_back) {
		GRECT g = { popup_area.x, popup_area.y, popup_area.w, popup_area.h };

		wind_update(BEG_UPDATE);
		draw(&g, BOX);
		wind_update(END_UPDATE);
	}
	if (r < 0 || r == sel)
		return;
	if (o == U_PAGES) {
		static const char *const inf[] = { "none", "mono", "colour", "colouronly" };

		pages_set = pages_order[r];
		setting("PAGES", inf[pages_set],
			"the whole pages a converted PDF carries: none, mono, colour (mono and colour), colouronly");
	} else if (o == U_PHOTOS) {
		photos_set = r;
		setting("PHOTOS", photos_set == CV_PHOTOS_JPEG ? "jpeg" : "indexed",
			"the photos: indexed (256 colours, fast on every Atari) or jpeg (small)");
	} else {
		on_gateway = r == 1;
		setting("CONVERT", on_gateway ? "gateway" : "atari",
			"where the PDFs are converted: atari (this Falcon or TT) or gateway");
		redraw_obj(BAND2);
	}
	texts();
	redraw_obj(o);
}

/* the family's tooltip (D18) on the window's buttons: what each does, its key */
static int win_tip(short x, short y, char *out, int size)
{
	short o, top, d;
	const char *t = NULL;

	wind_get(0, WF_TOP, &top, &d, &d, &d);
	if (win < 0 || iconified || top != win || wind_find(x, y) != win)
		return 0;
	o = objc_find(tree, BOX, 8, x, y);
	switch (o) {
	case B_ADD: t = "Add PDFs to the list (^O)"; break;
	case B_FOLDER: t = "Add every PDF in a folder"; break;
	case B_REMOVE: t = "Take the file chosen out of the list (Delete)"; break;
	case B_CLEAR: t = "Empty the list"; break;
	case B_OUT: t = "The folder the converted PDFs are written to"; break;
	case B_GO: t = working ? "Stop (Esc)" : "Convert the PDFs in the list (Return)"; break;
	case U_PAGES: t = "Each page whole as a picture: none, mono, or mono and colour"; break;
	case U_PHOTOS: t = "Photos in 256 colours (fast to show) or as JPEG (small)"; break;
	case U_ON: t = "Convert on this Atari, or send the PDFs to the Gateway"; break;
	}
	if (!t)
		return 0;
	snprintf(out, (size_t)size, "%s", t);
	return 1;
}

static void tip_repaint(const GRECT *r)
{
	wind_update(BEG_UPDATE);
	draw(r, BOX);
	wind_update(END_UPDATE);
}

static void click(short mx, short my)
{
	short o;

	if (iconified)
		return;
	o = objc_find(tree, BOX, 8, mx, my);

	/* a button as GEM's: shown pressed, and taken only when let go on it
	   (a click that wasn't the user's, or one moved off, does nothing:
	   common's dlg_win_button) */
	if (o >= 0 && (tree[o].ob_flags & EXIT)) {
		int took = dlg_win_button(tree, o);

		redraw_obj(o);
		if (!took)
			return;
	}
	switch (o) {
	case B_ADD: command(CMD_ADD); break;
	case B_FOLDER: command(CMD_FOLDER); break;
	case B_CLEAR: command(CMD_CLEAR); break;
	case B_OUT: command(CMD_OUT); break;
	case B_GO: command(CMD_GO); break;
	case B_REMOVE: command(CMD_REMOVE); break;
	case U_PAGES: case U_PHOTOS: case U_ON:
		if (!working)
			choose(o);
		break;
	default:
		if (o >= M0 && o < M0 + LROWS)
			o = (short)(L0 + o - M0);	/* (a row's mark or its grey words: the row) */
		else if (o >= G0 && o < G0 + LROWS)
			o = (short)(L0 + o - G0);
		if (o >= L0 && o < L0 + LROWS && ltop + o - L0 < nfiles) {
			int was = lsel;

			lsel = lsel == ltop + o - L0 ? -1 : ltop + o - L0;	/* (a click again: none) */
			texts();
			if (was >= ltop && was < ltop + LROWS)
				redraw_obj((short)(L0 + was - ltop));
			if (lsel >= 0)
				redraw_obj(o);
			redraw_obj(B_REMOVE);
		} else if (list_bar() && mx >= lbar.r.x && mx < lbar.r.x + lbar.r.w && my >= lbar.r.y &&
			   my < lbar.r.y + lbar.r.h)
			pb_click(&lbar, mx, my, list_scroll, list_goto);
		break;
	}
}

static void key(short kstate, short k)
{
	short ascii = (short)KEY_ASCII(k), scan = (short)KEY_SCAN(k);

	if (scan == 0x62) {		/* Help */
		command(CMD_GUIDE);
		return;
	}
	if ((kstate & K_CTRL) && scan == 0x33) {	/* ^, : Settings (the comma's key) */
		command(CMD_SETTINGS);
		return;
	}
	if (key_ctrl(kstate, k)) {
		switch (ascii | 0x40) {
		case 'O': command(CMD_ADD); return;
		case 'Q': command(CMD_QUIT); return;
		case 'R': command(CMD_GO); return;
		case 'U': command(CMD_QUIT); return;
		}
	}
	if (scan == 0x53 && !working) {	/* Delete: Remove */
		command(CMD_REMOVE);
		return;
	}
	if (ascii == 27 && working)
		stop_now = 1;		/* Esc: Stop */
	else if ((ascii == 13 || scan == 0x72) && !working)
		begin();
}

static void add_args(const char *s)
{
	char f[256];
	int n = 0;

	while ((s = av_arg(s, f, sizeof f)) != NULL)
		n += add_file(f);
	if (n && win >= 0) {
		texts();
		redraw_list();
	}
}

static void message(const short *msg)
{
	const char *s;

	switch (av_message(msg, &s)) {
	case AV_START:
		add_args(s);
		wind_set(win, WF_TOP, win, 0, 0, 0);
		return;
	case AV_NONE:
		break;
	default:
		return;
	}
	{
		DROP d;
		int r = dd_message(msg, DD_WANT_FILES, &d);

		if (r != DD_NONE) {
			if (r == DD_FILES)
				add_args(d.data);
			dd_done(&d);
			return;
		}
	}
	switch (msg[0]) {
	case MN_SELECTED:
		command(mbar_command(&menu, msg[4]));
		menu_tnormal(menu.tree, msg[3], 1);
		break;
	case WM_REDRAW: {
		GRECT g = { msg[4], msg[5], msg[6], msg[7] };

		wind_update(BEG_UPDATE);
		draw(&g, BOX);
		wind_update(END_UPDATE);
		break;
	}
	case WM_TOPPED:
		wind_set(win, WF_TOP, win, 0, 0, 0);
		break;
	case WM_BOTTOMED:
		wind_set(win, WF_BOTTOM, win, 0, 0, 0);	/* (to the back: 5.1) */
		break;
	case WM_ICONIFY:
	case WM_ALLICONIFY:
		if (!iconified) {
			wind_set(win, WF_ICONIFY, msg[4], msg[5], msg[6], msg[7]);
			iconified = 1;
			log_event("iconified", NULL, NULL);
		}
		break;
	case WM_UNICONIFY:
		if (iconified) {
			wind_set(win, WF_UNICONIFY, msg[4], msg[5], msg[6], msg[7]);
			iconified = 0;
			log_event("uniconified", NULL, NULL);
		}
		break;
	case WM_MOVED:
		wind_set(win, WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
		{
			char v[32];

			snprintf(v, sizeof v, "%d,%d", msg[4], msg[5]);
			setting("WINDOW", v, "the window's place: x,y in pixels");
		}
		break;
	case WM_CLOSED:
	case AP_TERM:
		command(CMD_QUIT);
		break;
	}
}

static void crash_quit(const char *what)
{
	(void)what;
	if (win >= 0) {
		wind_close(win);
		wind_delete(win);
	}
	if (has_menu)
		menu_bar(menu.tree, 0);
	pens_give();		/* (the two pens as they were) */
	v_clsvwk(vdi_h);
	appl_exit();
}

int main(void)
{
	short work_out[57], msg[8], d, x, y, w, h;
	AEVENT ev;
	unsigned long last_bar = 0;
	long last1 = -1, last2 = -1;

	ap_id = appl_init();
	if (ap_id < 0)
		return 1;
	key_state_init();	/* (keys typed while busy or starting keep their Shift and Control: common's gem.c) */
	tip_init(tip_repaint);	/* (the buttons' tooltips: D18) */
	dlg_title = "EConvert";
	vdi_h = graf_handle(&gl_wchar, &gl_hchar, &d, &d);
	cw = gl_wchar;
	ch = gl_hchar;
	vdi_h = v_opnvwk_(vdi_h, work_out);
	if (vdi_h <= 0) {
		form_alert(1, "[3][EConvert can't open a VDI|workstation.][ OK ]");
		appl_exit();
		return 1;
	}
	scr_w = work_out[0] + 1;
	scr_h = work_out[1] + 1;
	vq_extnd(vdi_h, 1, work_out);
	planes = work_out[4];
	progbar_pens(vdi_h, 0);
	vst_alignment(vdi_h, 0, 5);
	text_font(vdi_h, 1, cw, ch);
	wind_get(0, WF_WORKXYWH, &desk_x, &desk_y, &desk_w, &desk_h);
	graf_mouse(ARROW, 0);
	/* _CPU: a 68030 converts here; an ST only through the Gateway (its
	   converter is built for the 68030: Makefile, build/m68030cv) */
	has_030 = get_cookie(0x5F435055L) >= 20;
	if (!screen_check("EConvert", scr_w)) {
		v_clsvwk(vdi_h);
		appl_exit();
		return 1;
	}
	strncpy(start_dir, app_dir(), sizeof start_dir - 1);
	{
		size_t n = strlen(start_dir);

		if (n > 3 && start_dir[n - 1] == '\\')
			start_dir[n - 1] = '\0';
	}
	path_join(inf_path, sizeof inf_path, start_dir, "econvert.inf");
	path_join(font_dir, sizeof font_dir, start_dir, "fonts");
	crash_install(start_dir, "EConvert", app_version);
	crash_cleanup = crash_quit;
	help_init();
	av_init("ECONVERT");
	settings_read();
	build();
	texts();
	if (mbar_build(&menu, titles, gl_wchar, gl_hchar, scr_w, scr_h) == 0)
		has_menu = menu_bar(menu.tree, 1) != 0;
	wind_calc(0, icon_kind(NAME | CLOSER | MOVER), 0, 0, tree[BOX].ob_width, tree[BOX].ob_height, &d, &d, &w, &h);
	x = (short)(desk_x + (desk_w - w) / 2);
	y = (short)(desk_y + (desk_h - h) / 2);
	{
		const char *v = inf_get(&inf, "WINDOW");

		if (v) {
			char *e;
			short a = (short)strtol(v, &e, 10);

			if (*e == ',') {
				short b = (short)atoi(e + 1);

				if (a >= desk_x && b >= desk_y && a + w <= desk_x + desk_w && b + h <= desk_y + desk_h) {
					x = a;
					y = b;
				}
			}
		}
	}
	win = wind_create(icon_kind(NAME | CLOSER | MOVER), x, y, w, h);
	if (win < 0) {
		alert(1, "[3][No window is free.|Close one and start EConvert again.][ OK ]");
		quit_now = 1;
	} else {
		static char title[40];

		snprintf(title, sizeof title, " EConvert %s ", app_version);
		wind_set(win, WF_NAME, (short)((long)title >> 16), (short)(long)title, 0, 0);
		wind_open(win, x, y, w, h);
		av_window(win, 1);
	}
	add_args(av_command());
	crash_offer("EConvert", "next to ECONVERT.PRG");
	{
		char m[48];

		snprintf(m, sizeof m, "%ld", (long)Malloc(-1L) / 1024);
		log_event("started", app_version, m);
	}
	while (!quit_now || working) {
		short which, flags = MU_MESAG | MU_KEYBD | MU_BUTTON | MU_M1 | MU_TIMER;
		static short hx = -1, hy = -1;
		short kd;

		/* (a still pointer costs nothing: woken when it leaves its pixel; the
		   timer a tip's wait when idle, at once while a batch runs) */
		evnt_set_m1(1, hx, hy, 1, 1);
		which = evnt_multi_(flags, 1, 1, 1, working ? 0 : tip_wait(1000), msg, &ev);
		if (which & MU_M1)
			graf_mkstate(&hx, &hy, &kd, &kd);
		tip_tick(which, iconified ? -1 : hx, hy, win_tip);
		if (which & MU_KEYBD)
			key(ev.kstate, ev.kreturn);
		if ((which & MU_BUTTON) && wind_find(ev.mx, ev.my) == win)
			click(ev.mx, ev.my);
		if (which & MU_MESAG)
			message(msg);
		if (working) {
			/* a slice of the batch, then its bars where they moved (at
			   most four times a second, each the part that grew) */
			int drawn = 0;

			if (fresh == 2) {
				fresh = 1;	/* (the key's turn: nothing more in it) */
				continue;
			}
			if (fresh == 3) {
				/* the batch's end drawn in one piece: the button, the
				   progress, the alert and the list (drawing is not sliced:
				   Erez, 4 Nov 2026; to 1.1 three turns) */
				fresh = 0;
				finish_button();
				finish_progress();
				last1 = last2 = -1;
				finished();
				continue;
			}
			if (tos_ms() - last_bar >= 250 || fresh) {
				long a = share_file(), b = share_batch();

				last_bar = tos_ms();
				if (a != last1 || b != last2 || fresh) {
					progress_texts();
					redraw_progress();
					last1 = a;
					last2 = b;
					drawn = 1;
				}
				fresh = 0;
			}
			/* a slice, in a turn of its own (the bars drawn and a slice
			   in one turn were 150 ms on a Falcon) */
			if (!drawn) {
				slice_end = tos_ms() + 15;	/* (the turns' own granularity takes it to some 40) */
				coro_switch(&main_co, &work_co);
				if (work_done)
					fresh = 3;	/* (the window and its alert in a turn of their own) */
				continue;
			}
		}
	}
	/* each step of the way out into the log at once (the hub, 10 Nov 2026:
	   bombs as EFTP and EMail quit, no CRASH.LOG); the settings are saved
	   as they change */
	log_event("quit", NULL, NULL);
	if (win >= 0) {
		av_window(win, 0);
		wind_close(win);
		wind_delete(win);
	}
	log_event("quit", "the window closed", NULL);
	if (has_menu)
		menu_bar(menu.tree, 0);
	av_exit();
	pens_give();		/* (the two pens as they were) */
	log_event("quit", "colours and menu bar given back", NULL);
	/* the catcher off here, as EPDF's, with the VDI and the AES still
	   there (it had been left to crash_term at Pterm) */
	crash_remove();
	log_event("quit", "crash catcher off; hooked over it:", crash_over());
	v_clsvwk(vdi_h);
	log_event("quit", "the VDI given back", NULL);
	appl_exit();
	log_event("quit", "out of the AES", NULL);
	return 0;
}

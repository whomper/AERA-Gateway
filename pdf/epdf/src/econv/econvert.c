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
		pages_set = !strcmp(v, "none") ? CV_PAGES_NONE : !strcmp(v, "colour") ? CV_PAGES_COLOUR : CV_PAGES_MONO;
	if ((v = inf_get(&inf, "PHOTOS")) != NULL)
		photos_set = !strcmp(v, "jpeg") ? CV_PHOTOS_JPEG : CV_PHOTOS_INDEXED;
	if ((v = inf_get(&inf, "WRITETO")) != NULL && strlen(v) < sizeof out_dir)
		strcpy(out_dir, v);
	if ((v = inf_get(&inf, "FOLDER")) != NULL && strlen(v) < sizeof in_dir)
		strcpy(in_dir, v);
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
static void screen_sizes(int *page_w, int *pic_w)
{
	short w, h;

	scr_work_size(NAME | CLOSER | FULLER | MOVER | SIZER | UPARROW | DNARROW | VSLIDE, 8, &w, &h);
	*page_w = w;
	scr_work_size(NAME | CLOSER | FULLER | MOVER | SIZER | UPARROW | DNARROW | VSLIDE, 0, &w, &h);
	*pic_w = w;
}

#define MAXF 200
static struct {
	char path[160];
	long bytes;
	int state;			/* 0 to do, 1 done, 2 failed, 3 stopped */
	char why[60];
} files[MAXF];
static int nfiles;

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
	strcpy(files[nfiles].path, p);
	files[nfiles].bytes = n;
	files[nfiles].state = 0;
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

enum {
	BOX, T_FILES, B_ADD, B_FOLDER, B_CLEAR, T_OUT, B_OUT,
	T_PAGES, G_PAGES, P_NONE, P_MONO, P_COLOUR,
	T_PHOTOS, G_PHOTOS, F_256, F_JPEG,
	T_FILE, BAR1, T_BATCH, BAR2, B_GO, NOBJ
};
#define COLS 58
#define ROWS 19

static OBJECT tree[NOBJ];
static char s_files[COLS + 1], s_out[COLS + 1], s_file[COLS + 1], s_batch[COLS + 1];

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

static void build(void)
{
	obj(BOX, 0, G_BOX, 0, 0x00001100L, 0, 0, COLS, ROWS);
	obj(T_FILES, BOX, G_STRING, 0, (long)s_files, 2, 1, COLS - 4, 1);
	obj(B_ADD, BOX, G_BUTTON, SELECTABLE | EXIT, (long)"Add files...", 2, 3, 14, 1);
	obj(B_FOLDER, BOX, G_BUTTON, SELECTABLE | EXIT, (long)"Add folder...", 18, 3, 15, 1);
	obj(B_CLEAR, BOX, G_BUTTON, SELECTABLE | EXIT, (long)"Clear", 35, 3, 9, 1);
	obj(T_OUT, BOX, G_STRING, 0, (long)s_out, 2, 5, COLS - 16, 1);
	obj(B_OUT, BOX, G_BUTTON, SELECTABLE | EXIT, (long)"Write to...", COLS - 15, 5, 13, 1);
	obj(T_PAGES, BOX, G_STRING, 0, (long)"Whole pages:", 2, 7, 13, 1);
	obj(G_PAGES, BOX, G_IBOX, 0, 0, 16, 7, COLS - 18, 1);
	obj(P_NONE, G_PAGES, G_BUTTON, SELECTABLE | RBUTTON, (long)"None", 0, 0, 8, 1);
	obj(P_MONO, G_PAGES, G_BUTTON, SELECTABLE | RBUTTON, (long)"Mono", 10, 0, 8, 1);
	obj(P_COLOUR, G_PAGES, G_BUTTON, SELECTABLE | RBUTTON, (long)"Mono and colour", 20, 0, 18, 1);
	obj(T_PHOTOS, BOX, G_STRING, 0, (long)"Photos:", 2, 9, 13, 1);
	obj(G_PHOTOS, BOX, G_IBOX, 0, 0, 16, 9, COLS - 18, 1);
	obj(F_256, G_PHOTOS, G_BUTTON, SELECTABLE | RBUTTON, (long)"256 colours (fast)", 0, 0, 20, 1);
	obj(F_JPEG, G_PHOTOS, G_BUTTON, SELECTABLE | RBUTTON, (long)"JPEG (small)", 22, 0, 16, 1);
	obj(T_FILE, BOX, G_STRING, 0, (long)s_file, 2, 11, COLS - 4, 1);
	obj(BAR1, BOX, G_IBOX, 0, 0, 2, 12, COLS - 4, 1);
	obj(T_BATCH, BOX, G_STRING, 0, (long)s_batch, 2, 14, COLS - 4, 1);
	obj(BAR2, BOX, G_IBOX, 0, 0, 2, 15, COLS - 4, 1);
	obj(B_GO, BOX, G_BUTTON, SELECTABLE | EXIT | DEFAULT, (long)"Convert", COLS - 14, 17, 12, 1);
	link_tree();
	rsrc_obfix_all(tree, NOBJ);
	tree[BOX].ob_state = 0;
}

static void set_radio(short first, short last, short on)
{
	short i;

	for (i = first; i <= last; i++)
		tree[i].ob_state = (short)(i == on ? tree[i].ob_state | SELECTED : tree[i].ob_state & ~SELECTED);
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

static void worker(void)
{
	for (;;) {
		int i;

		for (i = 0; i < nfiles && !stop_now; i++) {
			CVOPT o;
			CVSTAT st;
			char outp[200], err[60];
			int r;

			if (files[i].state == 1)
				continue;
			cur = i;
			cur_done = 0;
			cur_total = 1000;
			t_file = tos_ms();
			cv_defaults(&o);
			o.pages = pages_set;
			o.photos = photos_set;
			o.mem_free = mem_free;	/* (a page refused memory: made again in bands) */
			screen_sizes(&o.page_w, &o.pic_w);
			o.fontdir = font_dir;
			o.progress = progress_cb;
			o.turn = turn;
			out_path(files[i].path, outp, sizeof outp);
			r = cv_convert(files[i].path, outp, &o, &st, err, sizeof err);
			files[i].state = r == 0 ? 1 : r == -2 ? 3 : 2;
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
			}
			bytes_done += files[i].bytes;
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

static void texts(void)
{
	long kb = 0;
	int i;

	for (i = 0; i < nfiles; i++)
		kb += files[i].bytes / 1024;
	{
		static int logged = -1;	/* the list's count for the rigs, when it changes */

		if (nfiles != logged) {
			char q[8];

			snprintf(q, sizeof q, "%d", nfiles);
			log_event("files", q, NULL);
			logged = nfiles;
		}
	}
	if (!nfiles)
		snprintf(s_files, sizeof s_files, "Files to convert: none yet");
	else
		snprintf(s_files, sizeof s_files, "Files to convert: %d (%ld KB)", nfiles, kb);
	snprintf(s_out, sizeof s_out, "Write to: %.40s", out_dir[0] ? out_dir : "(a folder to choose)");
	if (!working) {
		s_file[0] = s_batch[0] = '\0';
		tree[B_GO].ob_spec = (long)"Convert";
	} else
		tree[B_GO].ob_spec = (long)"Stop";
	set_radio(P_NONE, P_COLOUR, (short)(P_NONE + pages_set));
	set_radio(F_256, F_JPEG, (short)(F_256 + photos_set));
}

/* "45 %, 2 min left" from the share done (thousandths) and the time it took */
static void left(char *s, long n, long share, unsigned long since)
{
	unsigned long t = tos_ms() - since, rest;
	int pc = (int)(share / 10);

	if (share < 20 || t < 3000) {
		snprintf(s, (size_t)n, "%d %%", pc);
		return;
	}
	rest = (t / 1000UL) * (1000UL - (unsigned long)share) / (unsigned long)share;
	if (rest >= 120)
		snprintf(s, (size_t)n, "%d %%, %lu min left", pc, (rest + 30) / 60);
	else
		snprintf(s, (size_t)n, "%d %%, %lu s left", pc, rest < 5 ? 5UL : rest);
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

static void progress_texts(void)
{
	char a[40];

	if (!working)
		return;
	left(a, sizeof a, share_file(), t_file);
	snprintf(s_file, sizeof s_file, "File %d of %d: %.24s  %s", cur + 1, nfiles, base_of(files[cur].path), a);
	left(a, sizeof a, share_batch(), t_batch);
	snprintf(s_batch, sizeof s_batch, "The batch: %s", a);
}

static void bar(short o, long share, const GRECT *clip)
{
	short x, y;
	long to = share > 1000 ? PB_FULL : share * PB_FULL / 1000;

	(void)clip;
	objc_offset(tree, o, &x, &y);
	if (to > 0)
		progbar_fill(vdi_h, x, (short)(y + tree[o].ob_height / 4), tree[o].ob_width, (short)(tree[o].ob_height / 2), 0, (short)to);
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

static void draw(const GRECT *clip, short start)
{
	GRECT a = work_area(), r;

	tree[BOX].ob_x = a.x;
	tree[BOX].ob_y = a.y;
	wind_get(win, WF_FIRSTXYWH, &r.x, &r.y, &r.w, &r.h);
	graf_mouse(M_OFF, 0);
	while (r.w && r.h) {
		GRECT c = r;

		if (meet(clip, &c)) {
			short p[4];

			objc_draw(tree, start, 8, c.x, c.y, c.w, c.h);
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
		}
		wind_get(win, WF_NEXTXYWH, &r.x, &r.y, &r.w, &r.h);
	}
	graf_mouse(M_ON, 0);
}

static void redraw_obj(short o)
{
	GRECT g;

	objc_offset(tree, o, &g.x, &g.y);
	g.w = tree[o].ob_width;
	g.h = tree[o].ob_height;
	wind_update(BEG_UPDATE);
	draw(&g, BOX);
	wind_update(END_UPDATE);
}

/* the file's line and bar to the batch's, one rectangle drawn once
   (four objects drawn one by one were 80 ms of a Falcon's turn) */
static void redraw_progress(void)
{
	GRECT g;
	short y2;

	objc_offset(tree, T_FILE, &g.x, &g.y);
	objc_offset(tree, BAR2, &g.w, &y2);
	g.w = tree[T_FILE].ob_width;
	g.h = (short)(y2 + tree[BAR2].ob_height - g.y);
	wind_update(BEG_UPDATE);
#ifdef TURN_DEBUG
	{
		unsigned long t0 = tos_ms();
		char q[24];

		draw(&g, BOX);
		snprintf(q, sizeof q, "%lu ms", tos_ms() - t0);
		log_event("bar-draw", q, NULL);
	}
#else
	draw(&g, BOX);
#endif
	wind_update(END_UPDATE);
}

/* ---------------------------------------------------------------- the commands */

static void begin(void);

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
		redraw_obj(T_FILES);
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
	redraw_obj(T_FILES);
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
	redraw_obj(T_OUT);
}

static void begin(void)
{
	int i, there = 0;
	char p[200];

	if (!nfiles) {
		alert(1, "[1][Add the PDFs to convert first:|Add files... or Add folder...][ OK ]");
		return;
	}
	if (!out_dir[0]) {
		cmd_out();
		if (!out_dir[0])
			return;
	}
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
	working = 0;
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

/* its third: what came of it, and the files done off the list (the
   failed ones stay) */
static void finished(void)
{
	int i, j, failed = -1;

	for (i = 0; i < nfiles && failed < 0; i++)
		if (files[i].state == 2)
			failed = i;
	if (failed >= 0)
		alert(1, "[1][%d converted.|%.30s couldn't be:|%.30s][ OK ]", n_ok, base_of(files[failed].path),
		      files[failed].why);
	else if (stop_now)
		alert(1, "[1][Stopped. %d converted;|the one under way was left out.][ OK ]", n_ok);
	else
		alert(1, "[1][%d converted.][ OK ]", n_ok);
	for (i = j = 0; i < nfiles; i++)
		if (files[i].state != 1)
			files[j++] = files[i];
	nfiles = j;
	texts();
	redraw_obj(T_FILES);
}

static void about_box(void)
{
	static char mem[100];
	ABOUT a = { "EConvert", app_version, "PDFs prepared for EPDF on every Atari",
		    { mem, NULL, NULL }, { 0, 0, 0 }, icon_bits, icon16_px, icon16_rgb, icon256_px, icon256_rgb };

	about_mem_line(mem, (int)sizeof mem);
	if (!mem[0])
		a.status[0] = NULL;
	log_event("dialog", "about", app_version);
	about_show(&a);
}

enum { CMD_NONE, CMD_ABOUT, CMD_ADD, CMD_FOLDER, CMD_CLEAR, CMD_OUT, CMD_GO, CMD_QUIT, CMD_GUIDE };

static const MBAR_ITEM desk_items[] = { { "About EConvert...", NULL, CMD_ABOUT }, { NULL, NULL, CMD_NONE } };
static const MBAR_ITEM file_items[] = {
	{ "Add files...", "^O", CMD_ADD },
	{ "Add folder...", NULL, CMD_FOLDER },
	{ "Clear the list", NULL, CMD_CLEAR },
	{ "-", NULL, CMD_NONE },
	{ "Write to...", NULL, CMD_OUT },
	{ "Convert", "^R", CMD_GO },
	{ "-", NULL, CMD_NONE },
	{ "Quit", "^Q", CMD_QUIT },
	{ NULL, NULL, CMD_NONE } };
static const MBAR_ITEM help_items[] = { { "EConvert in the EPDF guide", NULL, CMD_GUIDE }, { NULL, NULL, CMD_NONE } };
static const MBAR_TITLE titles[] = { { " EConvert ", desk_items }, { " File ", file_items }, { " Help ", help_items },
				     { NULL, NULL } };

static void command(int c)
{
	switch (c) {
	case CMD_ABOUT:
		about_box();
		break;
	case CMD_ADD:
		cmd_add();
		break;
	case CMD_FOLDER:
		cmd_folder();
		break;
	case CMD_CLEAR:
		if (!working) {
			nfiles = 0;
			texts();
			redraw_obj(T_FILES);
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

static void click(short mx, short my)
{
	short o = objc_find(tree, BOX, 8, mx, my);

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
	case P_NONE: case P_MONO: case P_COLOUR:
		if (!working) {
			pages_set = o - P_NONE;
			setting("PAGES", pages_set == CV_PAGES_NONE ? "none" : pages_set == CV_PAGES_COLOUR ? "colour" : "mono",
				"the whole pages a converted PDF carries: none, mono, colour (mono and colour)");
			texts();
			redraw_obj(G_PAGES);
		}
		break;
	case F_256: case F_JPEG:
		if (!working) {
			photos_set = o - F_256;
			setting("PHOTOS", photos_set == CV_PHOTOS_JPEG ? "jpeg" : "indexed",
				"the photos: indexed (256 colours, fast on every Atari) or jpeg (small)");
			texts();
			redraw_obj(G_PHOTOS);
		}
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
	if (key_ctrl(kstate, k)) {
		switch (ascii | 0x40) {
		case 'O': command(CMD_ADD); return;
		case 'Q': command(CMD_QUIT); return;
		case 'R': command(CMD_GO); return;
		case 'U': command(CMD_QUIT); return;
		}
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
		redraw_obj(T_FILES);
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
	if (get_cookie(0x5F435055L) < 20) {	/* _CPU: a 68000 */
		alert(1, "[1][EConvert needs a Falcon or a TT|(a 68030). On an ST, EPDF reads|the PDFs EConvert made elsewhere.][ Quit ]");
		v_clsvwk(vdi_h);
		appl_exit();
		return 0;
	}
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
	wind_calc(0, NAME | CLOSER | MOVER, 0, 0, tree[BOX].ob_width, tree[BOX].ob_height, &d, &d, &w, &h);
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
	win = wind_create(NAME | CLOSER | MOVER, x, y, w, h);
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
		short which, flags = MU_MESAG | MU_KEYBD | MU_BUTTON;

		if (working)
			flags |= MU_TIMER;
		which = evnt_multi_(flags, 1, 1, 1, 0, msg, &ev);
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
	if (win >= 0) {
		av_window(win, 0);
		wind_close(win);
		wind_delete(win);
	}
	if (has_menu)
		menu_bar(menu.tree, 0);
	av_exit();
	v_clsvwk(vdi_h);
	appl_exit();
	return 0;
}

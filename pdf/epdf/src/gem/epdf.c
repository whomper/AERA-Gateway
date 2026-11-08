/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * EPDF - the PDF viewer's GEM program. Stage 1: the text view. One
 * window shows the document's text, every page one after another, read
 * a page at a time between event calls (doc.c) and drawn by textwin.c.
 *
 * EPDF is a program only, so EPDF.INF is kept beside it (the
 * guidelines, 6.1), read once at the start and written when a setting
 * changes and at the end (the window's place).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "gemui.h"
#include "dialog.h"
#include "mbar.h"
#include "text.h"
#include "alert.h"
#include "av.h"
#include "dd.h"
#include "help.h"
#include "aboutbox.h"
#include "bgwork.h"
#include "twin.h"
#include "crash.h"
#include "plat.h"
#include "util.h"
#include "inf.h"
#include "search.h"
#include "clip.h"
#include "popup.h"
#include "menu.h"
#include "pics.h"
#include "dspjpeg.h"
#include "rdraw.h"
#include "click.h"
#include "epdf.h"
#include "progbar.h"
#include "badge.h"
#include "gwopen.h"
#include "gatedlg.h"

short ap_id;
short pix_aspect = 16;
short vdi_h, cw, ch, planes, gl_wchar, gl_hchar;
short desk_x, desk_y, desk_w, desk_h;
extern short scr_w, scr_h;

static MBAR menu;
static int has_menu, quit_now;
static char start_dir[160];		/* EPDF.PRG's folder: EPDF.INF, CRASH.LOG, EPDF.TXC */
static char inf_path[180], cache_path[180];
static INF inf;
static int lines_mode;
/* File > Open on the Gateway (Erez, 7 Nov 2026): a PDF that isn't
   converted is converted by the Gateway while it is shown (gwopen.c) */
static int gw_on;
static char gate_host[64];
static unsigned short gate_port = GATE_PORT;
static char open_dir[160] = "";		/* where Open looks first (FOLDER=) */
static int find_dir;			/* a search runs: 1 forwards, -1 back; 0 none */
static void menu_states(void);
static int menus_due;			/* menu_states() in the next idle turn */
static void command(int c);

#ifndef BEG_MCTRL
#define BEG_MCTRL 3			/* wind_update: the mouse is ours while a selection is dragged */
#define END_MCTRL 2
#endif

void log_event(const char *what, const char *a, const char *b)
{
	char line[200];

	snprintf(line, sizeof line, "EPDF %s %s %s\n", what, a ? a : "", b ? b : "");
	nf_debug(line);
}

int settings_lines(void)
{
	return lines_mode;
}

/* ---------------------------------------------------------------- EPDF.INF */

static const char inf_head[] =
	"; EPDF's settings. EPDF writes this file itself; a line it doesn't know is kept.\r\n\r\n";

static void settings_read(void)
{
	long n;
	char *t = pf_load(inf_path, &n);
	const char *v;

	inf_init(&inf);
	if (t) {
		inf_parse(&inf, t, n);
		free(t);
	} else {
		inf_parse(&inf, inf_head, (long)strlen(inf_head));
	}
	if ((v = inf_get(&inf, "LINES")) != NULL)
		lines_mode = v[0] == '1';
	if ((v = inf_get(&inf, "FOLDER")) != NULL && strlen(v) < sizeof open_dir)
		strcpy(open_dir, v);
	if ((v = inf_get(&inf, "GATEWAY")) != NULL)
		gw_on = v[0] == '1';
	if ((v = inf_get(&inf, "GATE")) != NULL && strlen(v) < sizeof gate_host)
		strcpy(gate_host, v);
	if ((v = inf_get(&inf, "GATEPORT")) != NULL && atol(v) > 0 && atol(v) < 65536)
		gate_port = (unsigned short)atol(v);
}

static void settings_write(void)
{
	long n;
	char *t = inf_text(&inf, &n);

	if (t) {
		if (pf_save(inf_path, t, n) != 0)
			log_event("inf-not-written", inf_path, NULL);
		free(t);
	}
}

/* NAME=value, saved at once (6.5: what the user does goes to disk then) */
static void setting(const char *name, const char *value, const char *note)
{
	const char *old = inf_get(&inf, name);

	if (old && !strcmp(old, value))
		return;
	inf_set(&inf, name, value, note);
	settings_write();
}

static void window_place_save(void)
{
	short x, y, w, h;
	char v[32];

	win_where(&x, &y, &w, &h);
	if (w <= 0)
		return;
	snprintf(v, sizeof v, "%d,%d,%d,%d", x, y, w, h);
	setting("WINDOW", v, "the window's place and size: x,y,width,height in pixels");
}

/* ---------------------------------------------------------------- the document */

static void copy_end(const char *say);

/* the document couldn't be read (or opened at all) */
static void open_failed(void)
{
	alert(1, "[3][EPDF can't read this file as a PDF.][ OK ]");
	win_title();
	win_info();
	win_layout();
	win_draw_all();
}

/* the open begun: it goes on in the reader's slices (doc_slice), the
   window saying so, and opened() follows when it is done */
static void open_doc(const char *p)
{
	char copy[256];

	if (gw_on && !gw_in_copy_folder(p) && gw_copy(p, copy, sizeof copy)) {
		log_event("gateway-copy", NULL, NULL);
		p = copy;		/* converted before: that copy, at once */
	}
	gw_stop();			/* (another document's conversion: given up) */
	copy_end(NULL);			/* a copy of the old document's text: given up */
	if (doc_open(p, win_cols(), lines_mode, cache_path) != 0) {
		open_failed();
		return;
	}
	find_dir = 0;
	win_status(NULL);
	win_title();
	menu_states();
	win_layout();
	win_draw_all();
}

static int settings_dialog(void);

/* the document open isn't converted: the Gateway converts it while it is
   shown (File > Open on the Gateway ticked) */
static void gateway_begin(void)
{
	int r;

	if (!gw_on || !doc_is_open() || doc_prepared() || gw_busy() || gw_in_copy_folder(doc_path()))
		return;
	r = gw_start(doc_path(), gate_host, gate_port);
	if (r == GW_NO_ADDRESS && settings_dialog())
		r = gw_start(doc_path(), gate_host, gate_port);
	if (r == GW_NO_FOLDER)
		alert(1, "[1][The folder atari can't be made beside %.20s: the disk is write-protected.][ OK ]", doc_name());
	else if (r == GW_NO_NETWORK)
		alert(1, "[1][The Gateway is reached through STinG, MiNTnet or MagiC-Net, and none of them is running.][ OK ]");
	else if (r == GW_NO_MEMORY)
		alert(1, "[3][Not enough memory to send it to the Gateway.][ OK ]");
	log_event("gateway", r == GW_STARTED ? "started" : "not-started", NULL);
}

/* the Gateway has done: its copy opened in place of the PDF, or why not */
static void gateway_end(void)
{
	char copy[256], why[120];
	int r = gw_result(copy, sizeof copy, why, sizeof why);
	char q[8];

	snprintf(q, sizeof q, "%d", r);
	log_event("gateway-done", q, NULL);
	if (r == 0)
		open_doc(copy);
	else if (r == -3)
		alert(1, "[1][The Gateway at %.24s doesn't answer. Check that it is on and that the PDF converter is installed on it.][ OK ]",
		      gw_host());
	else if (r == -1)
		alert(1, "[1][%.110s][ OK ]", why);
}

static void opened(void)
{
	bg_opened();
	gateway_begin();
	win_title();
	menu_states();
	win_layout();
	win_draw_all();
}

static void open_dialog(void)
{
	static char name[16];
	char dir[200], full[256], *s;
	short button;

	if (open_dir[0])
		snprintf(dir, sizeof dir, "%s\\*.PDF", open_dir);
	else
		snprintf(dir, sizeof dir, "%c:\\*.PDF", (char)('A' + Dgetdrv()));
	if (!fsel_exinput(dir, name, &button, "Open a PDF") || !button || !name[0])
		return;
	strcpy(full, dir);
	s = strrchr(full, '\\');
	if (!s)
		return;
	*s = '\0';
	if (strcmp(full, open_dir)) {
		strncpy(open_dir, full, sizeof open_dir - 1);
		setting("FOLDER", open_dir, "the folder File > Open shows first");
	}
	strcat(full, "\\");
	strcat(full, name);
	open_doc(full);
}

/* ---------------------------------------------------------------- the selection, Find, Copy */

static char find_text[65];
static long find_page, find_from;	/* where it goes on */
static long find_left;			/* pages it may still look at */

/* Copy and Find next greyed where they can't be used (the guidelines, 4.4);
   an item's menu_ienable only when its state changes (an AES call each, on
   every step of a selection dragged) */
static void menu_states(void)
{
	static const short cmd[6] = { CMD_COPY, CMD_SELALL, CMD_FIND, CMD_FINDNEXT, CMD_FINDPREV, CMD_PAGES };
	static signed char was[6] = { -1, -1, -1, -1, -1, -1 };
	static signed char tick = -1;
	DOCVIEW *v = doc_view();
	signed char now[6];
	int i;

	menus_due = 0;
	if (!has_menu)
		return;
	now[0] = v && v->sel_p0 >= 0;
	now[1] = now[2] = v != NULL;
	now[3] = now[4] = v && find_text[0];
	now[5] = doc_has_pages() != 0;		/* (greyed for a file without its pages whole) */
	for (i = 0; i < 6; i++)
		if (now[i] != was[i]) {
			menu_ienable(menu.tree, mbar_object(&menu, cmd[i]), now[i]);
			was[i] = now[i];
		}
	if (doc_pages_mode() != tick) {
		tick = (signed char)doc_pages_mode();
		menu_icheck(menu.tree, mbar_object(&menu, CMD_PAGES), tick);
	}
}

static void select_span(long p0, long o0, long p1, long o1)
{
	DOCVIEW *v = doc_view();
	long q0, r0, q1, r1;

	if (!v)
		return;
	q0 = v->sel_p0, r0 = v->sel_o0, q1 = v->sel_p1, r1 = v->sel_o1;
	if (p0 >= 0 && (p1 < p0 || (p1 == p0 && o1 < o0))) {
		long t;		/* in order, as dv_select keeps it */

		t = p0, p0 = p1, p1 = t;
		t = o0, o0 = o1, o1 = t;
	}
	if (q0 == p0 && r0 == o0 && q1 == p1 && r1 == o1)
		return;			/* the same again (^A twice): nothing to turn over */
	dv_select(v, p0, o0, p1, o1);
	/* the rows whose look changed: the old shape off, the new one on */
	win_sel_change(q0, r0, q1, r1, p0, o0, p1, o1);
	menus_due = 1;		/* Copy's state in the next turn: an AES call off the key's */
}

/* the search starts after the selection (before it, going back), or at the
   top of the window */
static void find_start(int dir)
{
	DOCVIEW *v = doc_view();

	if (!v || !find_text[0])
		return;
	if (v->sel_p0 >= 0) {
		find_page = dir > 0 ? v->sel_p1 : v->sel_p0;
		find_from = dir > 0 ? v->sel_o1 : v->sel_o0;
	} else {
		find_page = dv_page_at_top(v);
		find_from = dir > 0 ? 0 : 0x7fffffffL;
	}
	find_dir = dir;
	find_left = doc_pages() + 1;
	win_status("finding...");
	log_event("find", find_text, dir > 0 ? "next" : "previous");
}

/* the search on through the pages read, a page a step; a page not read yet
   is read first. 1 while it goes on */
static int find_step(void)
{
	long len, at, n;
	const char *t;
	char msg[96];

	if (!find_dir)
		return 0;
	if (find_left-- <= 0) {
		find_dir = 0;
		snprintf(msg, sizeof msg, "\"%.30s\" isn't in the document", find_text);
		win_status(msg);
		log_event("find-none", find_text, NULL);
		return 0;
	}
	t = doc_page_text(find_page, &len);
	if (!t) {
		find_left++;			/* that page doesn't count yet */
		doc_want(find_page);
		return 1;
	}
	while (find_dir > 0 ? search_text(t, len, find_text, find_from, &at, &n)
			    : search_text_back(t, len, find_text, find_from, &at, &n)) {
		char p[16];
		long ls = at;

		while (ls > 0 && t[ls - 1] != '\n')
			ls--;
		if (t[ls] == 0x7F) {
			/* inside a picture's line ("\x7F<object> <w>x<h>"): not text */
			find_from = find_dir > 0 ? at + 1 : at;
			if (find_dir < 0 && at == 0)
				break;
			continue;
		}

		find_dir = 0;
		select_span(find_page, at, find_page, at + n);
		win_show(find_page, at);
		snprintf(p, sizeof p, "%ld", find_page + 1);
		snprintf(msg, sizeof msg, "found on page %s", p);
		win_status(msg);
		log_event("found", p, NULL);
		return 0;
	}
	/* the next page round, from its start (its end, going back) */
	if (find_dir > 0) {
		find_page = find_page + 1 < doc_pages() ? find_page + 1 : 0;
		find_from = 0;
	} else {
		find_page = find_page > 0 ? find_page - 1 : doc_pages() - 1;
		find_from = 0x7fffffffL;
	}
	return 1;
}

/*
 * Copy: the selection's text to the clipboard, lines ending CR LF; the
 * soft breaks as spaces (or line ends, showing the page's lines). A job
 * of its own between event calls (the guidelines' Speed: a whole book is
 * a normal thing to copy, 8.5 s on an ST in one go): 1 KB of the text a
 * step, handed to common's sliced clipboard writer (clip_put_step, which
 * writes it 16 KB at a time), its progress on the info line, Esc or Undo
 * to stop it (the clipboard as it was, or no half-written one).
 */
#define COPY_STEP 1024
static struct {
	int on;
	long p, at;			/* the page and offset it goes on from */
	long p0, o0, p1, o1;		/* the selection, as it was when Copy began */
	long len, missing;
	int skip;			/* inside a picture's line (pdftext.c's "\x7F..."): not copied */
	unsigned long said;		/* tos_ms() when the progress was last shown */
	char *piece;			/* a step's text, line ends made CR LF: COPY_STEP * 2 + 2 bytes,
					   taken while a copy goes on (not resident: 1 Nov 2026) */
} cp;

static void copy_end(const char *say)
{
	if (cp.on)
		clip_put_end(0);	/* given up: the old clipboard kept, or this one removed */
	free(cp.piece);
	memset(&cp, 0, sizeof cp);
	if (say)
		win_status(say);
}

static void copy(void)
{
	DOCVIEW *v = doc_view();

	if (!v || v->sel_p0 < 0 || cp.on)
		return;
	if (!clip_put_begin()) {
		win_status("no clipboard to copy to");
		return;
	}
	memset(&cp, 0, sizeof cp);
	if (!(cp.piece = malloc(COPY_STEP * 2 + 2))) {
		clip_put_end(0);
		win_status("too little memory to copy");
		return;
	}
	cp.on = 1;
	cp.p0 = v->sel_p0;
	cp.o0 = v->sel_o0;
	cp.p1 = v->sel_p1;
	cp.o1 = v->sel_o1;
	cp.p = cp.p0;
	cp.at = cp.o0;
	win_status("copying...");
}

/* the copy on by up to 1 KB of text; 1 while it goes on */
static int copy_step(void)
{
	long len, e, end;
	const char *t;
	char *q;

	if (!cp.on)
		return 0;
	if (cp.p > cp.p1) {
		char n[16];
		int ok = cp.len > 0;

		cp.on = 0;		/* clip_put_end is called here, not by copy_end */
		if (clip_put_end(ok) && ok) {
			snprintf(n, sizeof n, "%ld", cp.len);
			copy_end(cp.missing ? "copied; pages not read yet left out" : "copied");
			log_event("copied", n, NULL);	/* the copy done, its status shown */
		} else {
			copy_end("nothing copied");
		}
		return 0;
	}
	t = doc_page_text(cp.p, &len);
	if (!t) {
		cp.missing++;
		cp.p++;
		cp.at = 0;
		return 1;
	}
	end = cp.p == cp.p1 && cp.o1 < len ? cp.o1 : len;
	e = end - cp.at > COPY_STEP ? cp.at + COPY_STEP : end;
	q = cp.piece;
	if (cp.at == 0 && cp.p > cp.p0 && cp.len) {
		*q++ = '\r';
		*q++ = '\n';
	}
	{
		/* the step's text, its state in locals: the loop in registers,
		   not in cp's fields (40 ms for 2 KB on an ST with them) */
		const char *sp = t + cp.at, *stop = t + e;
		int skip = cp.skip, lm = lines_mode;
		char prev = cp.at > 0 ? t[cp.at - 1] : '\n';

		while (sp < stop) {
			char c = *sp++;

			if (skip)			/* a picture's line, and its line end */
				skip = c != '\n';
			else if (c == 0x7F && prev == '\n')
				skip = 1;
			else if (c == '\n' || (c == '\r' && lm)) {
				*q++ = '\r';
				*q++ = '\n';
			} else
				*q++ = c == '\r' ? ' ' : c;
			prev = c;
		}
		cp.skip = skip;
	}
	if (q > cp.piece && !clip_put_step(cp.piece, (long)(q - cp.piece))) {
		cp.on = 0;
		clip_put_end(0);
		copy_end("the clipboard couldn't be written");
		return 0;
	}
	cp.len += (long)(q - cp.piece);
	if (e >= end) {
		cp.p++;
		cp.at = 0;
		/* the progress every quarter of a second (the guidelines' Speed);
		   the AES draws the info line again at each one */
		if (cp.p1 > cp.p0 && tos_ms() - cp.said >= 250 && !win_progress_shown()) {
			char msg[48];

			snprintf(msg, sizeof msg, "copying... %ld of %ld pages", cp.p - cp.p0, cp.p1 - cp.p0 + 1);
			win_status(msg);
			cp.said = tos_ms();
		}
	} else {
		cp.at = e;
	}
	return 1;
}

/* a click in the text: the selection made by dragging; with Shift, the
   selection there is taken on to where the button is let go */
/* messages that come while a press is held (press_track): as the loop's */
static void message(const short *msg);
static void select_word(short mx, short my);

static void held_msg(short *m)
{
	message(m);
}

/* a left press in the text (the guidelines, 5.8): the selection's first
   cell at once; let go where it was, nothing (or, the second press of a
   double click, the word); moved while held, the selection follows the
   mouse until it is let go */
static void click(short mx, short my, short kstate, int dbl)
{
	DOCVIEW *v = doc_view();
	long ap, ao, p, o, lp, lo;
	short mb, ks;
	static const GRECT anywhere = { 0, 0, 32767, 32767 };
	int how;

	if (win_zoomed()) {
		win_zoom_end();		/* (a click gives the text back) */
		return;
	}
	if (!dbl && !(kstate & (K_LSHIFT | K_RSHIFT)) && win_zoom_at(mx, my))
		return;			/* a picture with more pixels: shown whole */
	if (!v || !win_hit(mx, my, &p, &o)) {
		if (v && v->sel_p0 >= 0)
			select_span(-1, 0, 0, 0);
		return;
	}
	if ((kstate & (K_LSHIFT | K_RSHIFT)) && v->sel_p0 >= 0) {
		ap = v->sel_p0;
		ao = v->sel_o0;
	} else {
		ap = p;
		ao = o;
	}
	select_span(ap, ao, p, o);
	lp = p;
	lo = o;
	how = press_track(&anywhere, mx, my, 0, held_msg, 0, 0);
	if (how != PRESS_DRAG) {
		if (dbl)
			select_word(mx, my);	/* the double click's word, on its second release */
		else if (v->sel_p0 == v->sel_p1 && v->sel_o0 == v->sel_o1)
			select_span(-1, 0, 0, 0);	/* a click alone: nothing selected */
		menu_states();
		return;
	}
	wind_update(BEG_MCTRL);
	for (;;) {
		graf_mkstate(&mx, &my, &mb, &ks);
		if (!(mb & 1))
			break;
		if (win_hit(mx, my, &p, &o) && (p != lp || o != lo)) {
			/* the selection's end moved: the rows whose look changed */
			dv_select(v, ap, ao, p, o);
			win_sel_change(ap, ao, lp, lo, ap, ao, p, o);
			lp = p;
			lo = o;
		}
		evnt_timer_(20);
	}
	wind_update(END_MCTRL);
	if (v->sel_p0 == v->sel_p1 && v->sel_o0 == v->sel_o1)
		select_span(-1, 0, 0, 0);	/* a click alone: nothing selected */
	menu_states();
}

/* a double-click: the word there selected (letters, digits and the
   Atari's accented letters up to the next blank or mark) */
static void select_word(short mx, short my)
{
	long p, o, len, a, e;
	const char *t;

	if (!win_hit(mx, my, &p, &o) || !(t = doc_page_text(p, &len)))
		return;
	a = e = o;
#define WORDCH(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') || ((c) >= '0' && (c) <= '9') || \
		   (unsigned char)(c) >= 0x80 || (c) == '\'' || (c) == '-')
	while (a > 0 && WORDCH(t[a - 1]))
		a--;
	while (e < len && WORDCH(t[e]))
		e++;
#undef WORDCH
	if (e > a)
		select_span(p, a, p, e);
}

/*
 * The context menu (the guidelines, 3.3): the right button, or Insert for
 * the selection. Only the menu bar's commands, with their keys; Copy
 * first, as a double-click is a selection's.
 */
static void context_menu(short mx, short my)
{
	static const struct { const char *label; int cmd; } all[] = {
		{ "  Copy                   ^C ", CMD_COPY },
		{ "  Select all             ^A ", CMD_SELALL },
		{ "-", CMD_NONE },
		{ "  Find...                ^F ", CMD_FIND },
		{ "  Find next              ^G ", CMD_FINDNEXT },
		{ "-", CMD_NONE },
		{ "  Previous page       ^Left ", CMD_PREV },
		{ "  Next page          ^Right ", CMD_NEXT },
		{ "-", CMD_NONE },
		{ "  The page's lines          ", CMD_LINES }
	};
	enum { N = sizeof all / sizeof all[0] };
	static char alt[N][40];
	const char *lab[N];
	DOCVIEW *v = doc_view();
	short i, r;

	for (i = 0; i < N; i++) {
		int c = all[i].cmd, grey = !v && c != CMD_LINES;

		if (c == CMD_COPY && (!v || v->sel_p0 < 0))
			grey = 1;
		if (c == CMD_FINDNEXT && !find_text[0])
			grey = 1;
		lab[i] = all[i].label;
		if (grey || (c == CMD_LINES && lines_mode)) {
			alt[i][0] = grey ? '~' : '\x08';	/* greyed, or ticked (popup.h) */
			strncpy(alt[i] + 1, all[i].label + (grey ? 0 : 1), sizeof alt[i] - 2);
			lab[i] = alt[i];
		}
	}
	log_event("menu", "popup", NULL);
	r = popup(mx, my, lab, N);
	if (r >= 0 && all[r].cmd != CMD_NONE)
		command(all[r].cmd);
}

/* ---------------------------------------------------------------- commands and keys */

static void page_step(long d)
{
	DOCVIEW *v = doc_view();
	long p;

	if (!v)
		return;
	p = dv_page_at_top(v) + d;
	if (p < 0)
		p = 0;
	if (p >= doc_pages())
		p = doc_pages() - 1;
	doc_want(p);
	win_goto_page(p);
	{
		char n[16];

		snprintf(n, sizeof n, "%ld", p + 1);
		log_event("top", n, NULL);
	}
}

static void command(int c)
{
	char n[8];

	if (c != CMD_ABOUT)
		win_zoom_end();		/* (a picture shown whole: the text back first) */
	switch (c) {
	case CMD_ABOUT:
		about_box();
		break;
	case CMD_OPEN:
		open_dialog();
		break;
	case CMD_GATEWAY:
		gw_on = !gw_on;
		setting("GATEWAY", gw_on ? "1" : "0", "1: a PDF opened that isn't converted is converted by the Gateway (Open on the Gateway)");
		if (has_menu)
			menu_icheck(menu.tree, mbar_object(&menu, CMD_GATEWAY), gw_on);
		if (gw_on)
			gateway_begin();
		else
			gw_stop();
		break;
	case CMD_SETTINGS:
		settings_dialog();
		break;
	case CMD_CLOSE:
	case CMD_QUIT:
		quit_now = 1;
		break;
	case CMD_GUIDE:
		if (help_key(start_dir, "epdf.hyp", NULL, "About"))
			about_box();
		break;
	case CMD_PREV:
		page_step(-1);
		break;
	case CMD_NEXT:
		page_step(1);
		break;
	case CMD_FIRST:
		page_step(-doc_pages());
		break;
	case CMD_LAST:
		page_step(doc_pages());
		break;
	case CMD_COPY:
		copy();
		break;
	case CMD_SELALL:
		if (doc_view())
			select_span(0, 0, doc_pages() - 1, 0x7fffffffL);
		break;
	case CMD_FIND:
		if (doc_view() && find_dialog(find_text))
			find_start(1);
		menu_states();
		break;
	case CMD_FINDNEXT:
	case CMD_FINDPREV:
		if (find_text[0])
			find_start(c == CMD_FINDNEXT ? 1 : -1);
		else if (doc_view() && find_dialog(find_text))
			find_start(1);
		break;
	case CMD_FONT:
		log_event("dialog", "font", NULL);	/* the rigs type into it once it is open */
		if (font_dialog()) {
			setting("FONT", font_setting(), "the text's font and its size: \"System font,16\", or a fixed-width GDOS font's name and points");
			win_layout();
			win_draw_all();
		}
		break;
	case CMD_PAGES:
		/* a converted PDF's pages whole, or their text (a tick: the pages) */
		if (!doc_has_pages())
			break;
		doc_set_pages(!doc_pages_mode());
		if (has_menu)
			menu_icheck(menu.tree, mbar_object(&menu, CMD_PAGES), doc_pages_mode());
		log_event("pages", doc_pages_mode() ? "1" : "0", NULL);
		win_layout();
		win_draw_all();
		break;
	case CMD_LINES:
		lines_mode = !lines_mode;
		if (has_menu)
			menu_icheck(menu.tree, mbar_object(&menu, CMD_LINES), lines_mode);
		snprintf(n, sizeof n, "%d", lines_mode);
		setting("LINES", n, "1: each line as it is on the page; 0: the paragraphs joined and wrapped to the window");
		win_layout();
		win_draw_all();
		break;
	default:
		break;
	}
}

/* File > Settings... (^,): where the Gateway is; 1 when OK */
static int settings_dialog(void)
{
	char port[8];

	if (!gate_dialog(gate_host, (int)sizeof gate_host, &gate_port))
		return 0;
	setting("GATE", gate_host, "the Gateway's address (Through the Gateway)");
	snprintf(port, sizeof port, "%u", (unsigned)gate_port);
	setting("GATEPORT", port, "the Gateway's port for PDFs (8380)");
	return 1;
}

static int shift(short kstate)
{
	return (kstate & (K_LSHIFT | K_RSHIFT)) != 0;
}

static void key(short kstate, short k)
{
	short scan = KEY_SCAN(k), rows = (short)win_rows();

	if (win_iconified())
		return;			/* an iconified window takes no keys (5.1) */
	if (win_zoom_key(scan))
		return;			/* a picture shown whole takes them */
	if (gw_busy() && scan == 0x01 && !cp.on && !find_dir) {
		gw_stop();		/* Esc: the Gateway's conversion given up */
		log_event("gateway-stopped", NULL, NULL);
		return;
	}
	if (cp.on && (scan == 0x01 || scan == 0x61)) {
		copy_end("copy stopped");	/* Esc or Undo: the copy given up */
		log_event("copy-stopped", NULL, NULL);
		return;
	}
	/* the AES makes these itself from Control with the arrows and
	   Clr/Home, and gives them without Control in kstate; Shift may be gone
	   too when EPDF was busy, so Shift+^Home and ^End both mean the last */
	if (KEY_ASCII(k) == 0) {
		switch (scan) {
		case 0x73: command(CMD_PREV); return;		/* ^Left */
		case 0x74: command(CMD_NEXT); return;		/* ^Right */
		case 0x77: command(shift(kstate) ? CMD_LAST : CMD_FIRST); return;	/* ^Clr/Home */
		case 0x75: command(CMD_LAST); return;		/* ^End, where a keyboard has it */
		}
	}
	if (key_ctrl(kstate, k)) {
		switch (scan) {
		case 0x18: command(CMD_OPEN); return;		/* ^O */
		case 0x33: command(CMD_SETTINGS); return;	/* ^, */
		case 0x16: command(CMD_CLOSE); return;		/* ^U: one window, so quit (3.2) */
		case 0x11: return;				/* ^W: one window, so nothing */
		case 0x10: command(CMD_QUIT); return;		/* ^Q */
		case 0x2e: command(CMD_COPY); return;		/* ^C */
		case 0x1e: command(CMD_SELALL); return;		/* ^A */
		case 0x21: command(shift(kstate) ? CMD_FONT : CMD_FIND); return;	/* ^F; Shift+^F: Font... */
		case 0x22: command(shift(kstate) ? CMD_FINDPREV : CMD_FINDNEXT); return;	/* ^G, Shift+^G */
		}
		return;
	}
	win_status(NULL);		/* a key: the last message has been read */
	switch (scan) {
	case 0x62:			/* Help: EPDF.HYP in the viewer that runs (4.3) */
		command(CMD_GUIDE);
		return;
	case 0x52: {			/* Insert: the context menu, for the selection (3.3) */
		short x, y, w, h;

		wind_get(win_handle(), WF_WORKXYWH, &x, &y, &w, &h);
		context_menu((short)(x + w / 3), (short)(y + h / 3));
		return;
	}
	case 0x48:			/* Up; Shift: a window up */
		win_scroll(shift(kstate) ? -(rows - 1) : -1);
		return;
	case 0x50:			/* Down; Shift: a window down */
		win_scroll(shift(kstate) ? rows - 1 : 1);
		return;
	case 0x47:			/* Clr/Home: the start; Shift: the end */
		win_scroll_to_abs(shift(kstate) ? 0x7fffffffL : 0);
		return;
	case 0x39:			/* Space: a window down; Shift: up */
		win_scroll(shift(kstate) ? -(rows - 1) : rows - 1);
		return;
	}
}

/* ---------------------------------------------------------------- messages */

static void message(const short *msg)
{
	const char *s;
	char f[256];

	switch (av_message(msg, &s)) {
	case AV_START:			/* a file, or nothing: the window to the front */
		if (av_arg(s, f, sizeof f))
			open_doc(f);
		else if (win_handle() >= 0)
			wind_set(win_handle(), WF_TOP, win_handle(), 0, 0, 0);
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
			if (r == DD_FILES && av_arg(d.data, f, sizeof f))
				open_doc(f);
			dd_done(&d);
			return;
		}
	}
	if (win_message(msg)) {
		if (msg[0] == WM_CLOSED)
			quit_now = 1;
		return;
	}
	switch (msg[0]) {
	case MN_SELECTED:
		command(mbar_command(&menu, msg[4]));
		menu_tnormal(menu.tree, msg[3], 1);
		break;
	case WM_CLOSED:
		quit_now = 1;
		break;
	case AP_TERM:
		quit_now = 1;
		break;
	}
}

/* the progress row's job (Erez, 2 Nov 2026: the one the user waits on):
   a copy, a search, the document being opened, a page in the window being
   read (within a slow page too), a picture being made, the rest of the
   pages being read */
static void progress(void)
{
	char w[64];
	long pg, made, of, n = doc_pages(), p0, p1, rp;
	int pm = 0, pic;

	rp = doc_reading(&pm);
	win_pages_shown(&p0, &p1);
	pic = doc_pic_progress(&pg, &made, &of);
	if (cp.on) {
		snprintf(w, sizeof w, "Copying page %ld of %ld", cp.p - cp.p0 + 1 > cp.p1 - cp.p0 + 1 ? cp.p1 - cp.p0 + 1 : cp.p - cp.p0 + 1,
			 cp.p1 - cp.p0 + 1);
		win_progress(4, w, cp.p - cp.p0, cp.p1 - cp.p0 + 1);
	} else if (find_dir) {
		snprintf(w, sizeof w, "Searching page %ld of %ld", find_page + 1, n);
		win_progress(3, w, n + 1 - find_left, n);
	} else if (doc_opening()) {
		win_progress(5, "Opening the document", 0, 0);
	} else if (gw_busy()) {
		long d, t;

		gw_words(w, sizeof w, &d, &t);	/* (the three phases: guideline 7.3) */
		win_progress(6, w, d, t);
	} else if (rp >= 0 && (!pic || (rp >= p0 && rp <= p1))) {
		snprintf(w, sizeof w, "Reading page %ld of %ld", rp + 1, n);
		win_progress(1, w, doc_read() * 1000L + pm, n * 1000L);
	} else if (pic) {
		if (doc_pic_drawing())
			snprintf(w, sizeof w, "Drawing page %ld / %ld", pg + 1, n);
		else
			snprintf(w, sizeof w, "Picture on page %ld", pg + 1);	/* the one in view, before pages out of it */
		win_progress(2, w, made, of);
	} else
		win_progress(0, NULL, 0, 0);
}

/* after a crash (common/atari/crash.c): GEM closed down before it quits */
static void crash_quit(const char *what)
{
	(void)what;
	win_close();
	if (has_menu)
		menu_bar(menu.tree, 0);
	v_clsvwk(vdi_h);
	appl_exit();
}

/* the reading on for a piece of the slice; a page it finished shown */
static long arrived = -1;		/* a page read, its rows still to draw */
static long arrived_pic = -1;		/* a picture made, its rows still to draw */
static long rows_pic = -1, rows_from, rows_to;	/* a picture being made: its new rows, to draw */
static int pic_turn;			/* pictures and pages take turns */
static int gw_turn;			/* the Gateway's exchange and the reading take turns */

/* the reading on for a piece of the slice; 1 when a page came, whose rows
   are drawn in a turn of their own (a page's rows and the slice that read
   it, together, made one stretch of 400 ms on an ST with TOS 1.04) */
static int read_on(void)
{
	long n = doc_slice(40);

	if (n == DOC_OPENED || n == DOC_FAILED) {
		/* the window laid out and drawn: the turn ends there, the
		   reading goes on in the next (its first page, read in the same
		   turn, made it 880 ms on an ST) */
		if (n == DOC_OPENED)
			opened();
		else
			open_failed();
		return 1;
	}
	if (n < 0)
		return 0;
	arrived = n;
	if (doc_prepared() || doc_drawn())
		menus_due = 1;		/* (View > Pages greyed until a page whole has come; no AES call unless it changes) */
	return 1;
}

int main(void)
{
	short work_out[57], msg[8], d, x = 0, y = 0, w = 0, h = 0;
	AEVENT ev;
	const char *cmd, *v;

	/* the heap takes from GEMDOS what is needed, in 64 KB steps, not a
	   quarter of the memory free at its first malloc (221 KB of a Falcon
	   with four accessories, 1 KB of it used with no document): an AES
	   with NVDI needs room to save the screen under a menu's drop-down,
	   and 0.5's first candidate left too little (the hub's acc_all) */
	heap_grain = 64L * 1024;
	ap_id = appl_init();
	if (ap_id < 0)
		return 1;
	if (dspjpeg_present())
		pic_dsp = &dspjpeg_port_jpeg;	/* a Falcon: its DSP decodes the JPEGs it takes (EPDF dithers on the CPU) */
	dlg_title = "EPDF";			/* dialogs in a window (MiNT, MagiC): 3.5 */
	vdi_h = graf_handle(&gl_wchar, &gl_hchar, &d, &d);
	cw = gl_wchar;
	ch = gl_hchar;
	vdi_h = v_opnvwk_(vdi_h, work_out);
	if (vdi_h <= 0) {
		form_alert(1, "[3][EPDF can't open a VDI|workstation.][ OK ]");
		appl_exit();
		return 1;
	}
	scr_w = work_out[0] + 1;
	scr_h = work_out[1] + 1;
	if (work_out[3] > 0 && work_out[4] > 0)	/* a pixel's width and height, microns */
		pix_aspect = (short)(work_out[4] * 16L / work_out[3]);
	vq_extnd(vdi_h, 1, work_out);
	planes = work_out[4];
	progbar_pens(vdi_h, 0);		/* the status bar's progress bar on white (5.9); EPDF sets no pens */
	badge_pens();			/* the Optimised badge's greens, the same way */
	vst_alignment(vdi_h, 0, 5);
	text_font(vdi_h, 1, cw, ch);	/* until the INF's font is set */
	wind_get(0, WF_WORKXYWH, &desk_x, &desk_y, &desk_w, &desk_h);
	if (!screen_check("EPDF", scr_w)) {	/* not in ST low (5.2) */
		v_clsvwk(vdi_h);
		appl_exit();
		return 1;
	}
	/* EPDF's own folder from the command it was started with, not the
	   current one (6.1) */
	strncpy(start_dir, app_dir(), sizeof start_dir - 1);
	{
		size_t n = strlen(start_dir);

		if (n > 3 && start_dir[n - 1] == '\\')
			start_dir[n - 1] = '\0';
	}
	path_join(inf_path, sizeof inf_path, start_dir, "epdf.inf");
	path_join(cache_path, sizeof cache_path, start_dir, "epdf.txc");
	/* stage 2: render.ovl and the standard fonts beside the program */
	path_join(rdraw_path, sizeof rdraw_path, start_dir, "render.ovl");
	path_join(doc_font_dir, sizeof doc_font_dir, start_dir, "fonts");
	{
		/* (else EConvert's, as the release's zip carries them: econvert\fonts) */
		char t[220];

		path_join(t, sizeof t, doc_font_dir, "sans.ttf");
		if (pf_size(t) < 0) {
			path_join(t, sizeof t, start_dir, "econvert");
			path_join(doc_font_dir, sizeof doc_font_dir, t, "fonts");
		}
	}
	crash_install(start_dir, "EPDF", app_version);
	crash_cleanup = crash_quit;
	help_init();
	twin_init("EPDF");		/* bgwork asks it; EPDF has no accessory */
	av_init("EPDF");
	bg_init(BG_NOW);
	settings_read();
	font_init(inf_get(&inf, "FONT"), start_dir);
	if ((v = inf_get(&inf, "WINDOW")) != NULL) {
		short *f[4];
		char *end;
		int i;

		f[0] = &x;
		f[1] = &y;
		f[2] = &w;
		f[3] = &h;
		for (i = 0; i < 4; i++) {
			*f[i] = (short)strtol(v, &end, 10);
			if (end == v || (i < 3 && *end != ','))
				break;
			v = end + 1;
		}
		if (i < 4)
			x = y = w = h = 0;
	}

	/* the AES's cell, not the view's: font_init has set cw and ch to the
	   document's font (Courier 14 spread the titles and cut their tops,
	   and the bar took no clicks where its titles belong; the hub, 1 Nov
	   2026) */
	if (menu_build(&menu, gl_wchar, gl_hchar, scr_w, scr_h) == 0) {
		has_menu = menu_bar(menu.tree, 1) != 0;
		if (!has_menu) {
			/* (the AES had no room for it: NVDI with four accessories
			   on a 4 MB Falcon, the hub's acc_all; the keys still work) */
			char m[16];

			snprintf(m, sizeof m, "%ld", (long)Malloc(-1L) / 1024);
			log_event("menu-bar-failed", m, NULL);
		}
		if (has_menu) {
			menu_icheck(menu.tree, mbar_object(&menu, CMD_LINES), lines_mode);
			menu_icheck(menu.tree, mbar_object(&menu, CMD_GATEWAY), gw_on);
		}
		menu_states();
	}
	/* the arrow: the desktop leaves the busy bee it showed while EPDF
	   loaded (Erez's TOS, 2 Nov 2026: the bee stayed the whole time) */
	graf_mouse(ARROW, 0);
	if (win_open(x, y, w, h) != 0) {
		alert(1, "[3][No window is free.|Close one and start EPDF again.][ OK ]");
		quit_now = 1;
	}
	cmd = av_command();
	if (!quit_now && cmd[0]) {
		char f[256];

		if (av_arg(cmd, f, sizeof f))
			open_doc(f);
	}
	crash_offer("EPDF", "next to EPDF.PRG");
	log_event("started", app_version, NULL);
	{
		/* what is left once the window is open (the guidelines, 2.1: an
		   AES with NVDI needs room for a menu's drop-down; 0.5's first
		   candidate left too little for it on a Falcon with four
		   accessories): the largest free block, the heap taken, in use */
		char m[48];

		snprintf(m, sizeof m, "%ld %ld %ld", (long)Malloc(-1L) / 1024, (heap_st + heap_tt) / 1024, heap_use / 1024);
		log_event("free-kb", m, NULL);
	}
	while (!quit_now) {
		short which, flags = MU_MESAG | MU_KEYBD | MU_BUTTON;
		unsigned long t;
		int pending = doc_pending() || find_dir || doc_pic_busy() || pics_waiting() || cp.on || gw_busy();

		progress();

		/* the sliders after a scroll, once no key waits; the reading
		   on while nothing else does */
		if (win_sliders_due() || menus_due || pending || arrived >= 0 || arrived_pic >= 0 || rows_pic >= 0 ||
		    win_progress_due() || win_ahead_due())
			flags |= MU_TIMER;
		t = win_sliders_due() || menus_due || arrived >= 0 || arrived_pic >= 0 || rows_pic >= 0 ||
		    win_progress_due() ? 0 : bg_timer(pending || win_ahead_due(), 1000);
		{
			long pw = win_progress_wait();	/* the row to go once nothing has run for a while */

			if (pw >= 0) {
				if (!(flags & MU_TIMER) || (unsigned long)pw < t)
					t = (unsigned long)pw;
				flags |= MU_TIMER;
			}
		}
		/* either button going down (0x100: the AES's "any of them"), one
		   click (5.8: the press at once, not held back for the
		   double-click time); a double click timed by press_double */
		which = evnt_multi_(flags, 0x101, 3, 0, t, msg, &ev);
		bg_tick_at(ev.mx, ev.my, ev.mbutton, ev.kstate);	/* the event's mouse: no graf_mkstate (4.9 ms on TOS 1.04) */
		if (which & MU_KEYBD)
			key(ev.kstate, ev.kreturn);
		if ((which & MU_BUTTON) && !win_iconified() && wind_find(ev.mx, ev.my) == win_handle()) {
			if (ev.mbutton & 2)
				context_menu(ev.mx, ev.my);	/* (on press, as ever) */
			else
				click(ev.mx, ev.my, ev.kstate, press_double(1, ev.mx, ev.my));
		}
		if (which & MU_MESAG)
			message(msg);
		if (which == MU_TIMER) {
			if (arrived >= 0) {
				win_page_ready(arrived);	/* the page read last turn */
				win_info();
				arrived = -1;
			} else if (arrived_pic >= 0) {
				win_pic_ready(arrived_pic);	/* the picture made last turn */
				arrived_pic = -1;
				rows_pic = -1;
			} else if (rows_pic >= 0) {
				win_pic_rows(rows_pic, rows_from, rows_to);	/* its rows made last turn */
				rows_pic = -1;
			} else if (win_progress_due()) {
				win_progress_draw();	/* the progress row, a turn of its own */
			} else if (win_sliders_due()) {
				win_sliders();
			} else if (menus_due) {
				menu_states();
			} else if (cp.on) {
				/* a copy the user asked for: before the reading and the
				   pictures; a step starts only if one as long as the last
				   still fits in the slice (bg_room: the slice not overrun) */
				unsigned long t0 = tos_ms();

				bg_begin();
				while (copy_step()) {
					unsigned long t1 = tos_ms();

					if (!bg_room((int)(t1 - t0)))
						break;
					t0 = t1;
				}
			} else if ((doc_pic_busy() || pics_waiting()) && bg_may_work() && ((pic_turn ^= 1) || !doc_pending())) {
				long obj = -1;

				doc_pic_drop_ahead();	/* (one shown waits: the one ahead gives way) */

				/* a picture the window shows, every other turn (it has a
				   stack of its own): the user sees its frame, and the text goes
				   on being read in the turns between (a big JPEG takes minutes
				   on an ST) */
				bg_begin();
				if (doc_pic_busy() || doc_pic_start())
					while (!doc_pic_slice(40, &obj) && bg_more())
						;
				if (!doc_pic_busy() && obj >= 0)
					arrived_pic = obj;
				else {
					long o, f, t2;

					/* its rows so far, drawn in the next turn (from the top down) */
					if (doc_pic_rows(&o, &f, &t2)) {
						if (rows_pic == o)
							rows_to = t2;
						else {
							rows_pic = o;
							rows_from = f;
							rows_to = t2;
						}
					}
				}
			} else if (gw_busy() && bg_may_work() && ((gw_turn ^= 1) || !doc_pending())) {
				/* the Gateway's exchange, every other turn with the reading */
				bg_begin();
				if (gw_slice(40))
					gateway_end();
			} else if (pending && bg_may_work()) {
				bg_begin();
				/* a search first: the user waits for it; the reading after */
				while (find_dir && arrived < 0 && find_step() && bg_more()) {
					long len;

					if (!doc_page_text(find_page, &len))
						read_on();	/* the page it waits for */
				}
				if (!find_dir)
					while (!read_on() && doc_pending() && bg_more())
						;
			} else if (win_ahead_due() && bg_may_work()) {
				win_ahead();	/* pictures ahead of the view, queued quietly */
			}
		}
	}
	log_event("quit", NULL, NULL);
	window_place_save();
	win_close();
	if (has_menu)
		menu_bar(menu.tree, 0);
	mbar_free(&menu);
	copy_end(NULL);
	doc_close();
	pics_exit();
	font_exit();
	inf_free(&inf);
	av_exit();
	crash_remove();
	v_clsvwk(vdi_h);
	appl_exit();
	return 0;
}

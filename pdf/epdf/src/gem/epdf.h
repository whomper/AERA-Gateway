/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * EPDF - what its GEM files share. ui.h (GRECT) comes first.
 */
#ifndef EPDF_H
#define EPDF_H

struct DOCVIEW_s;

extern const char app_version[];
extern short ap_id;
extern short pix_aspect;			/* a pixel's height / its width x 16 (16 square) */

/* epdf.c */
void log_event(const char *what, const char *a, const char *b);
int settings_lines(void);		/* View > The page's lines: 1, else paragraphs joined */

/* about.c */
void about_box(void);
/* an iconified window drawn as the icon, status under it */
void app_icon_redraw(short h, const GRECT *area, const char *status);

/* doc.c: the open document */
int doc_open(const char *path, int cols, int lines_mode, const char *cache);
/* the open goes on in doc_slice, on the reader's stack (an AES-256 file's
   key: a minute on a 68000): DOC_OPENED when it is done, DOC_FAILED when
   the file can't be read; doc_opening() 1 meanwhile */
#define DOC_OPENED (-2)
#define DOC_FAILED (-3)
int doc_opening(void);
void doc_close(void);
int doc_is_open(void);
const char *doc_path(void);
const char *doc_name(void);
/* View > Translate: each page's translation (doc.c keeps them, in memory
   only), the page's state */
enum { TR_NONE, TR_DONE, TR_SAME, TR_FAILED, TR_NOTHING, TR_PARTIAL };
int doc_tr_begin(const char *to);	/* the translations' store for language to: 0, -1 */
void doc_tr_forget(void);
void doc_set_translate(int on);		/* the view shows the translations, or the pages as read */
int doc_tr_state(long n);
const char *doc_tr_from(long n);
/* page n's translation (Atari text; NULL with TR_SAME, TR_FAILED, TR_NOTHING) */
int doc_tr_put(long n, int state, const char *text, long len, const char *from);
const char *doc_page_original(long n, long *len);	/* the page as read */
int doc_tr_from_file(void);
const char *lang_name(const char *code);	/* epdf.c: "he" -> "Hebrew" */		/* a converted PDF translated into it: nothing to ask */
const char *doc_tr_file_to(void);
/* the document open is a copy made from path (its name shown) */
void doc_set_original(const char *path);
/* that PDF, or the document's own path */
const char *doc_original(void);
long doc_pages(void);
long doc_read(void);			/* pages whose text is read */
int doc_pending(void);			/* pages still to read */
void doc_want(long page);		/* read from this page on next */
long doc_slice(unsigned long ms);	/* the reading on for ms: the page it finished, or -1 */
int doc_pic_start(void);		/* a picture the window wants: its decoding begun */
int doc_pic_slice(unsigned long ms, long *obj);	/* 1 when it is done: *obj */
int doc_pic_rows(long *obj, long *from, long *to);	/* rows made since asked: from, to */
int doc_pic_busy(void);
int win_ahead_due(void);		/* pictures ahead of the view to look for */
void win_ahead(void);			/* (a few rows a turn) */
void doc_pic_drop_ahead(void);	/* a picture ahead stopped for one shown */
/* the progress row's: the page being read (-1 none), *prog how far
   through it in thousandths; the picture being made, its page and
   rows made of all (*of 0: not made a row at a time) */
long doc_reading(int *prog);
int doc_pic_progress(long *page, long *made, long *of);
const char *doc_status(void);
void epdf_net_line(char *out, int n);	/* the About box's connection line, or "" */		/* the About box's line */
const char *doc_page_text(long n, long *len);	/* NULL when not read yet */
long doc_page_len(long n);		/* -1 when not read yet; nothing read */
#include "docview.h"
DOCVIEW *doc_view(void);		/* NULL without a document */
int doc_prepared(void);			/* a converted PDF (EConvert's) */
/* a page can be drawn at any size here (render.ovl on a Falcon or TT: a
   PDF's own pages, or a converted copy's from its original) */
int doc_page_drawable(void);
int doc_drawn(void);			/* its pages drawn as printed by render.ovl (stage 2) */
int doc_pic_drawing(void);		/* the picture being made is a page drawn */
void doc_pic_stop_page_unless(long obj, int w, int h);	/* a zoom: another page's drawing stopped */
extern char doc_font_dir[200];
int doc_has_pages(void);		/* ... with its pages whole (View > Pages) */
int doc_pages_mode(void);		/* the view shows them */
void doc_set_pages(int on);

/* textwin.c: the window */
int win_open(short x, short y, short w, short h);
void win_close(void);
short win_handle(void);
int win_iconified(void);
int win_rows(void);
int win_cols(void);
void win_title(void);
void win_info(void);
void win_layout(void);
void win_redraw(const GRECT *area);
void win_draw_all(void);
void win_draw_rows(long r0, long r1);
void win_scroll(long d);
void win_scroll_to_abs(long r);
void win_goto_page(long n);
void win_page_ready(long n);
int win_sliders_due(void);
void win_sliders(void);
void win_where(short *x, short *y, short *w, short *h);
int win_message(const short *msg);
void win_status(const char *s);		/* after the info line's page, until cleared (NULL) */
/* the progress row (Erez, 2 Nov 2026): the job running now, kind > 0, its
   words and how far (done of total; total 0: not known), or kind 0
   none; called every turn. The row shows once a job has run half a
   second and goes when none has for a third; it is drawn in a turn of
   its own (win_progress_due, win_progress_draw), at most every 250 ms */
void win_progress(int kind, const char *what, long done, long total);
int win_progress_due(void);
void win_progress_draw(void);
int win_progress_shown(void);
long win_progress_wait(void);
void win_pages_shown(long *p0, long *p1);	/* the first and last page in the window */		/* ms until the row may go (-1: not waiting to) */
int win_hit(short mx, short my, long *page, long *off);
void win_draw_span(long p0, long o0, long p1, long o1);
/* the selection changed from span q to span p (p0 or q0 < 0: none): the rows whose look changed drawn */
void win_sel_change(long q0, long r0, long q1, long r1, long p0, long o0, long p1, long o1);
void win_show(long page, long off);		/* that place in view */
void win_pic_ready(long obj);		/* a picture decoded: its rows drawn */
void win_pic_rows(long obj, long from, long to);	/* its pixel rows from, to made: drawn */
/* a picture shown whole in the window (clicked: win_zoom_at, 1 when it was
   one); win_zoom_key takes the keys while it is (1 taken) */
int win_zoom_at(short mx, short my);
int win_zoomed(void);
void win_zoom_end(void);
int win_zoom_key(short scan);
/* View > Zoom in (dir 1) and Zoom out (-1) on View > Pages' page in view
   (100 to 400 %): can it, and do it */
int win_zoom_can(int dir);
void win_zoom_step(int dir);
void win_zoom_actual(void);		/* View > Actual size */
/* the zoomed picture dragged by (dx, dy): 1 when it moved */
int win_zoom_pan(long dx, long dy);
extern void (*has_menu_states)(void);
/* a new document: every zoom offered again */
void win_zoom_forget(void);
/* doc.c: a page's picture (View > Pages) and its double: 1, its object and size */
int doc_double_of(long obj, long *dbl, long *w, long *h);

/* font.c: the text view's font (FONT= in EPDF.INF) */
void font_init(const char *setting, const char *dir);
const char *font_setting(void);
int font_dialog(void);			/* View > Font...: 1 when another was chosen */
void font_exit(void);
/* the system's small font (6x6, the desktop's icon labels) on vdi_h and
   the text drawing told its cell (*w, *h), then the view's font back */
void font_small(short *w, short *h);
void font_back(void);

/* find.c */
int find_dialog(char *text);		/* text: 65 bytes; 1 when Find was chosen */

#endif

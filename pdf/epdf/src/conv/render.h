/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * render.h - a page drawn by EConvert: the reader's interpreter (PDRAW's
 * hooks) onto raster.c's canvas, glyphs through FreeType; and what on the
 * page is a drawing (a chart, a diagram, a map), for the converted file
 * to carry as a picture.
 */
#ifndef RENDER_H
#define RENDER_H

#include "pdf.h"
#include "raster.h"

/* a part of the page painted with paths: a drawing when it is big and
   busy enough (render_drawings) */
typedef struct {
	RCLIP box;			/* device pixels */
	long paths, curves;		/* painted, and their curves and slanted lines */
	long colours;			/* fills in a colour that isn't white, black or grey */
	long rules;			/* thin straight lines and rectangles (a table's) */
} RAREA;

typedef struct RFONTS RFONTS;

typedef struct {
	PDOC *d;
	RCANVAS c;
	int width;			/* the canvas's width in pixels */
	fx scale;			/* pixels a point */
	fx base[6];			/* page points to device pixels */
	fx pw, ph;			/* the page's size in points (after its turn) */
	RAREA *area;			/* what was painted, merged as it comes */
	long narea, caparea;
	RFONTS *fonts;
	const char *fontdir;		/* the standard fonts' files (Liberation*.ttf), or NULL */
	long glyphs, glyphs_missed, images, images_missed;
	int mono_glyphs;		/* 1: glyphs a bit a pixel (the mono page's: grey edges
					   dithered broke 10-pixel text into dots, decided 4 Nov 2026);
					   2: those under 20 pixels, a screen's of 16 colours or fewer
					   (render.ovl's, ROVL_HOST's crisp) */
	long image_ms;			/* the page's pictures' decode, in render_clock's ms */
	long canvas_max;		/* set by the caller: the canvas's most bytes (0 any): a page
				   taller is drawn a band at a time (render_page_from) */
	int whole_only;			/* set by the caller: a page whose content met a memory
					   refusal (its text or drawing cut short) is a failure, -1,
					   not a page with parts missing (render.ovl: EPDF keeps its
					   stored page, 13 Nov 2026); 0: drawn as far as it went */
	/* how far through the page (render_progress): its content streams'
	   and its pictures' stored bytes, as the page starts, and the
	   pictures' read so far (a page that is a scan sat at its content's
	   end, 40 %, while the picture took most of the time: decided 13 Nov
	   2026) */
	long p_ctot, p_itot, p_idone, p_icur, p_polls, p_ih;
	int p_inimg;
	/* a page drawn in bands (item 3, 13 Nov 2026): the picture the last
	   band decoded, kept for the next (one picture spanning several
	   bands was decoded in each: page 3 of the showcase 18 times) */
	void *ic_pic;			/* (a PIC: pic.h) */
	long ic_obj;
	int ic_w, ic_h;
} RPAGE;

/* the fonts are kept from page to page: render_init once, render_done at
   the end */
int render_init(RPAGE *r, PDOC *d, const char *fontdir);
/* the font programs' bytes the renderer keeps at most between pages, the
   least lately used let go first; 0 (the default): any */
extern long render_font_budget;
void render_done(RPAGE *r);
/* the caller's clock in ms (NULL: image_ms isn't kept) */
extern unsigned long (*render_clock)(void);
/* page n drawn width pixels wide (its height from its shape): r->c
   holds it; 0 ok */
int render_page(RPAGE *r, long n, int width);
/* the same page drawn again into the band starting at row y0 (a multiple
   of 8), when the canvas holds a band (r->c.rows < r->c.h); 0 ok */
int render_page_from(RPAGE *r, long n, int width, int y0);
/* the drawings found on the page drawn: their boxes in page points
   (x0, y0 bottom left, x1, y1 top right, as PDF's), at most max */
typedef struct {
	fx x0, y0, x1, y1;
	RCLIP px;			/* the same in the canvas's pixels */
} RDRAWING;
int render_drawings(const RPAGE *r, RDRAWING *out, int max);
/* the page being drawn: how far, 0 to 1000, its content and its pictures
   by their stored bytes (render.ovl's progress) */
int render_progress(const RPAGE *r);
/* pic.h's pic_poll for a renderer that turns (render.ovl's): the picture
   being decoded counted for render_progress, then cv_turn's turn */
int render_pic_poll(void);

#endif

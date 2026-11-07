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
	int mono_glyphs;		/* glyphs hinted, a bit a pixel (the mono page's: grey edges
					   dithered broke 10-pixel text into dots, Erez, 4 Nov 2026) */
	long canvas_max;		/* set by the caller: the canvas's most bytes (0 any): a page
				   taller is drawn a band at a time (render_page_from) */
} RPAGE;

/* the fonts are kept from page to page: render_init once, render_done at
   the end */
int render_init(RPAGE *r, PDOC *d, const char *fontdir);
void render_done(RPAGE *r);
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

#endif

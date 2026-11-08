/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * rovl.h - render.ovl, the page as printed drawn on a Falcon or TT (stage
 * 2): EConvert's renderer (src/conv/render.c, raster.c, FreeType,
 * libjpeg-turbo, the DSP's JPEGs) and its own copy of the reader, built
 * for the 68030, in a file of its own beside EPDF.PRG. An ST never loads
 * it and keeps its memory; EPDF.PRG stays one program for every Atari.
 * Loaded with common's ovl.c, as TLS.OVL is.
 *
 * The program and the overlay meet through two tables, as TLS.OVL's do:
 * what the program lends (ROVL_HOST) and what the overlay offers (ROVL,
 * at the start of its text). Each starts with its size and only grows at
 * the end; a change that can't be made by adding raises ROVL_MAJOR.
 */
#ifndef ROVL_H
#define ROVL_H

#include <stddef.h>

#define ROVL_MAGIC 0x4550524FUL	/* "EPRO" */
#define ROVL_MAJOR 1

/* what the program lends: its memory and its turns. Files, the clock and
   the DSP the overlay reaches through TOS itself */
typedef struct {
	long size;			/* sizeof(ROVL_HOST) as the program knows it */
	void *(*malloc)(size_t n);
	void *(*calloc)(size_t n, size_t m);
	void *(*realloc)(void *p, size_t n);
	void (*free)(void *p);
	/* asked often while a page is drawn (every few operators, glyphs,
	   rows): it may hand the machine back for a while; 1 stops the page */
	int (*turn)(void);
} ROVL_HOST;

/* a page drawn: rows y0 .. y0 + rows - 1 of a w x h page, 5-6-5, w a row;
   the overlay's memory, good until the next call */
typedef struct {
	int w, h;
	int y0, rows;
	const unsigned short *px;
	long glyphs, images;		/* drawn */
	long glyphs_missed, images_missed;
	long image_ms;			/* the pictures' decode */
} ROVL_PAGE;

typedef struct {
	unsigned long magic;		/* ROVL_MAGIC */
	short major;			/* ROVL_MAJOR */
	short size;			/* sizeof(ROVL) as the overlay knows it */
	char version[8];		/* EPDF's release it came with */
	/* 1 when it can work with this program's ROVL_HOST; first, once */
	int (*init)(const ROVL_HOST *h);
	/* the PDF opened for drawing (fontdir: the standard fonts' folder,
	   font_budget: the font programs' bytes kept at most, 0 any;
	   canvas_max: the canvas's most bytes, a taller page drawn a band at
	   a time); NULL and why */
	void *(*open)(const char *path, const char *fontdir, long font_budget, long canvas_max,
		      char *err, int errlen);
	long (*pages)(void *doc);
	/* page n (from 0) drawn width pixels wide, its band from row y0 (0,
	   or a multiple of 8 below out->h when out->rows < out->h): 0 drawn,
	   -1 failed, -2 stopped (turn said so) */
	int (*page)(void *doc, long n, int width, int y0, ROVL_PAGE *out);
	void (*close)(void *doc);
	/* the JPEGs on the DSP where there is one (1, as it starts), or the
	   68030's; and where they went: "DSP", "68030", "-" */
	void (*use_dsp)(int on);
	const char *(*dsp_state)(void);
	/* the page being drawn: how far through its content, in thousandths
	   (the progress bar's) */
	int (*progress)(void *doc);
	/* rows y .. y + rows - 1 of the band last drawn (y from the band's
	   first) dithered to the n (2 to 16) colours of pal (src/rovl/pens.c:
	   error diffusion, white kept white), on the 68030: each row into out
	   as `planes` rows of (w + 15) / 16 words, pen bit p in plane p (the
	   VDI's standard format). 0 ok */
	int (*pens)(void *doc, const unsigned char (*pal)[3], int n, int planes, int y, int rows, unsigned char *out);
} ROVL;

#endif

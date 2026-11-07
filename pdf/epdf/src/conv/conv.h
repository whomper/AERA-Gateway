/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * conv.h - EConvert's converter: a PDF in, a converted PDF out
 * (docs/CONVERTED-PDF.md). Portable C: EConvert on a Falcon or TT, and
 * this computer's tools (and AERA-Gateway's later).
 */
#ifndef CONV_H
#define CONV_H

enum { CV_PAGES_NONE, CV_PAGES_MONO, CV_PAGES_COLOUR };
enum { CV_PHOTOS_INDEXED, CV_PHOTOS_JPEG };
/* a picture's stored size: its size on the page (a point a pixel), or its
   own pixels (a drawing: the page drawn's) up to pic_w, for EPDF to show
   whole when it is clicked; its line keeps its size on the page */
enum { CV_PIC_PAGE, CV_PIC_SCREEN };

typedef struct {
	int pages;			/* whole pages: CV_PAGES_* (mono) */
	int photos;			/* CV_PHOTOS_* (indexed) */
	int pic_size;			/* CV_PIC_* (screen) */
	/* the screen's: the whole page's pictures' width (EPDF's text column
	   in a window the size of the desktop) and a picture's most; 0: 640 */
	int page_w, pic_w;
	/* the mono page at twice page_w where there is memory to draw it in
	   one piece (EPDF shows it at page_w, and whole when clicked); 1 */
	int page_double;
	long canvas_max;		/* the page canvas's most bytes, 0 any: a page drawn in
				   bands under it (slower) */
	/* the memory free now (EConvert: GEMDOS's), or NULL: a page refused
	   memory with the canvas whole is made again in bands of an eighth of
	   it (256 KB without it), and the rest of the file so */
	long (*mem_free)(void);
	const char *fontdir;		/* the standard fonts' files (Liberation*.ttf), or NULL */
	/* asked between pieces of work: done and total for this file, in
	   thousandths of its pages; 1 stops the conversion */
	int (*progress)(void *u, long done, long total);
	void *u;
	/* asked often while a page is worked (every few operators, rows,
	   symbols): it may hand the machine back for a while (EConvert's
	   window: the work on a stack of its own); 1 stops the conversion */
	int (*turn)(void *u);
} CVOPT;

/* the turn the long loops (dithers, deflate, fills) ask for; set by
   cv_convert while it runs (NULL none) */
extern int (*cv_turn)(void);
/* what the converter is at ("render", "text", "picture", "mono"...), for a log */
extern const char *cv_phase;

typedef struct {
	long pages, in_bytes, out_bytes, ms;
	long pictures, drawings;
} CVSTAT;

void cv_defaults(CVOPT *o);
/* 0 done; -1 failed (err says why), -2 stopped (out removed) */
int cv_convert(const char *in, const char *out, const CVOPT *o, CVSTAT *st, char *err, int errlen);

#endif

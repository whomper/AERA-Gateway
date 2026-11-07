/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * raster.h - EConvert's rasteriser: paths filled and stroked, glyph and
 * picture bitmaps laid onto a canvas of 5-6-5 pixels (pic.h's), all in
 * fixed point (a Falcon has no FPU). Coordinates are device pixels in
 * 16.16, y down.
 */
#ifndef RASTER_H
#define RASTER_H

#include <stdint.h>

typedef struct {
	int32_t x0, y0, x1, y1;		/* whole pixels, [x0, x1) x [y0, y1) */
} RCLIP;

typedef struct {
	int32_t x, y;			/* 16.16 */
} RPT;

/* a path flattened to straight lines: subpaths one after another, each
   starting where start[] says */
typedef struct {
	RPT *pt;
	long n, cap;
	long *start;			/* each subpath's first point */
	char *closed;			/* each subpath closed (h, re) */
	long ns, caps;
	int32_t cx, cy;			/* the current point */
	int open;			/* a subpath under way */
	int bad;			/* memory refused: drawn as far as it got */
	long curves;			/* curves and slanted lines in it (the drawings' test) */
} RPATH;

typedef struct {
	int w, h;			/* the page's size */
	int y0, rows;			/* the rows held: y0 .. y0 + rows - 1 (all of them, or a band) */
	uint16_t *px;			/* w * rows, 5-6-5 */
	int16_t *acc;			/* a row's coverage, w + 2 */
	struct REDGE *e;		/* a fill's edges */
	long ne, cape;
	int32_t *xs;			/* a sub-scanline's crossings: x and winding */
	long capx;
} RCANVAS;

int rc_init(RCANVAS *c, int w, int h);	/* white; 0 ok */
/* a band of rows of a w x h canvas (y0 0), white; 0 ok. Everything drawn
   lands only where it is held */
int rc_init_rows(RCANVAS *c, int w, int h, int rows);
/* the band moved to start at row y0, white */
void rc_band(RCANVAS *c, int y0);
void rc_free(RCANVAS *c);
void rc_clear(RCANVAS *c, uint16_t px);

static inline uint16_t rgb565(int r, int g, int b)
{
	return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void rp_init(RPATH *p);
void rp_free(RPATH *p);
void rp_reset(RPATH *p);
void rp_move(RPATH *p, int32_t x, int32_t y);
void rp_line(RPATH *p, int32_t x, int32_t y);
void rp_curve(RPATH *p, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3);
void rp_close(RPATH *p);
/* its bounds in whole pixels; 0 when it has no points */
int rp_bounds(const RPATH *p, RCLIP *b);

/* filled with one colour, nonzero or even-odd, inside clip */
void rc_fill(RCANVAS *c, const RPATH *p, int evenodd, uint16_t col, const RCLIP *clip);
/* stroked: width in pixels (16.16; thinner than one is drawn one wide),
   dash: n lengths in pixels (16.16) from phase, n 0 none */
void rc_stroke(RCANVAS *c, const RPATH *p, int32_t width, const int32_t *dash, int ndash, int32_t phase,
	       uint16_t col, const RCLIP *clip);
/* a coverage bitmap (a glyph: 0..255 a pixel, pitch bytes a row) laid at
   (x, y) in whole pixels in col */
void rc_mask(RCANVAS *c, int x, int y, const unsigned char *a, int w, int h, int pitch, uint16_t col,
	     const RCLIP *clip);
/* a picture of pw x ph pixels (5-6-5) drawn through m, the unit square to
   the device (16.16: a b c d e f, the picture's first row at the top of
   the square, as PDF's images); alpha (pw x ph, 0 transparent) or NULL;
   stencil: alpha only, in col */
void rc_image(RCANVAS *c, const uint16_t *px, const unsigned char *alpha, int pw, int ph, const int32_t *m,
	      int stencil, uint16_t col, const RCLIP *clip);

#endif

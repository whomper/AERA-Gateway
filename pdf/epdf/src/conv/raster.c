/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * raster.c - EConvert's rasteriser (raster.h).
 *
 * A fill is scanned a pixel row at a time, four sub-scanlines a row: each
 * one's crossings with the path's edges, sorted, give the spans inside
 * (nonzero or even-odd), and the spans add their coverage to the row (a
 * pixel partly covered by its share), so edges come out smoothed. A
 * stroke is made a fill: a quadrilateral along each segment and a square
 * at each joint. Fixed point throughout (16.16 pixels; 64-bit products
 * where a crossing is first placed).
 */
#include <stdlib.h>
#include <string.h>
#include "raster.h"
#include "conv.h"

typedef struct REDGE {
	int32_t y0, y1;			/* top and bottom, 16.16 */
	int32_t x0;			/* x at y0 */
	int32_t slope;			/* x per y, 16.16 */
	int dir;			/* +1 down, -1 up */
} REDGE;

#define SUB 4				/* sub-scanlines a row */
#define FULL (256 / SUB)		/* a pixel's coverage from one sub-scanline */

int rc_init(RCANVAS *c, int w, int h)
{
	return rc_init_rows(c, w, h, h);
}

int rc_init_rows(RCANVAS *c, int w, int h, int rows)
{
	memset(c, 0, sizeof *c);
	c->w = w;
	c->h = h;
	c->rows = rows < h ? rows : h;
	c->px = malloc((size_t)w * c->rows * 2);
	c->acc = calloc((size_t)w + 4, sizeof *c->acc);
	if (!c->px || !c->acc) {
		rc_free(c);
		return -1;
	}
	rc_clear(c, 0xFFFF);
	return 0;
}

void rc_free(RCANVAS *c)
{
	free(c->px);
	free(c->acc);
	free(c->e);
	free(c->xs);
	memset(c, 0, sizeof *c);
}

void rc_band(RCANVAS *c, int y0)
{
	c->y0 = y0;
	rc_clear(c, 0xFFFF);
}

void rc_clear(RCANVAS *c, uint16_t px)
{
	long i, n = (long)c->w * c->rows;

	/* in bands of 32 rows, a turn after each: a page of 640 is a MB,
	   145 ms on a Falcon in one piece */
	long band = (long)c->w * 32;

	for (i = 0; i < n; i += band) {
		long k = n - i < band ? n - i : band, j;

		if ((px >> 8) == (px & 255))
			memset(c->px + i, px & 255, (size_t)k * 2);	/* (white: a long at a time) */
		else
			for (j = 0; j < k; j++)
				c->px[i + j] = px;
		if (cv_turn)
			cv_turn();
	}
}

/* ------------------------------------------------------------------ paths */
void rp_init(RPATH *p)
{
	memset(p, 0, sizeof *p);
}

void rp_free(RPATH *p)
{
	free(p->pt);
	free(p->start);
	free(p->closed);
	memset(p, 0, sizeof *p);
}

void rp_reset(RPATH *p)
{
	p->n = 0;
	p->ns = 0;
	p->open = 0;
	p->bad = 0;
	p->curves = 0;
}

static int pt_add(RPATH *p, int32_t x, int32_t y)
{
	if (p->n == p->cap) {
		long nc = p->cap ? p->cap * 2 : 256;
		RPT *q = realloc(p->pt, (size_t)nc * sizeof *q);

		if (!q) {
			p->bad = 1;
			return 0;
		}
		p->pt = q;
		p->cap = nc;
	}
	p->pt[p->n].x = x;
	p->pt[p->n].y = y;
	p->n++;
	return 1;
}

static int sub_add(RPATH *p)
{
	if (p->ns == p->caps) {
		long nc = p->caps ? p->caps * 2 : 32;
		long *q = realloc(p->start, (size_t)nc * sizeof *q);
		char *k;

		if (!q) {
			p->bad = 1;
			return 0;
		}
		p->start = q;
		k = realloc(p->closed, (size_t)nc);
		if (!k) {
			p->bad = 1;
			return 0;
		}
		p->closed = k;
		p->caps = nc;
	}
	p->start[p->ns] = p->n;
	p->closed[p->ns] = 0;
	p->ns++;
	return 1;
}

void rp_move(RPATH *p, int32_t x, int32_t y)
{
	p->cx = x;
	p->cy = y;
	p->open = 0;
}

static void begin(RPATH *p)
{
	if (!p->open) {
		if (sub_add(p) && pt_add(p, p->cx, p->cy))
			p->open = 1;
	}
}

void rp_line(RPATH *p, int32_t x, int32_t y)
{
	begin(p);
	if (!p->open)
		return;
	if (x != p->cx && y != p->cy)
		p->curves++;
	pt_add(p, x, y);
	p->cx = x;
	p->cy = y;
}

static int32_t iabs32(int32_t v)
{
	return v < 0 ? -v : v;
}

/* a cubic Bezier as straight lines, as many as its size wants (a quarter
   pixel off the curve at most, roughly) */
void rp_curve(RPATH *p, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3)
{
	int32_t x0 = p->cx, y0 = p->cy, d;
	int n, i;

	begin(p);
	if (!p->open)
		return;
	p->curves++;
	/* the control polygon's deviation from the chord, in pixels */
	d = iabs32(x0 - 2 * x1 + x2) + iabs32(y0 - 2 * y1 + y2);
	{
		int32_t e = iabs32(x1 - 2 * x2 + x3) + iabs32(y1 - 2 * y2 + y3);

		if (e > d)
			d = e;
	}
	d >>= 16;
	for (n = 1; n < 64 && (long)n * n * 2 < (long)d * 3; n++)
		;
	if (d > 2 && n < 4)
		n = 4;
	for (i = 1; i <= n; i++) {
		/* t = i / n in 16.16 */
		int64_t t = ((int64_t)i << 16) / n, s = 65536 - t;
		int64_t a = s * s >> 16, b = t * t >> 16;
		int64_t c0 = a * s >> 16, c1 = 3 * a * t >> 16, c2 = 3 * s * b >> 16, c3 = b * t >> 16;
		int32_t x = (int32_t)((c0 * x0 + c1 * x1 + c2 * x2 + c3 * x3) >> 16);
		int32_t y = (int32_t)((c0 * y0 + c1 * y1 + c2 * y2 + c3 * y3) >> 16);

		pt_add(p, x, y);
	}
	p->cx = x3;
	p->cy = y3;
}

void rp_close(RPATH *p)
{
	if (p->open && p->ns > 0) {
		RPT *s = &p->pt[p->start[p->ns - 1]];

		p->closed[p->ns - 1] = 1;
		p->cx = s->x;
		p->cy = s->y;
	}
	p->open = 0;
}

int rp_bounds(const RPATH *p, RCLIP *b)
{
	long i;
	int32_t x0, y0, x1, y1;

	if (p->n <= 0)
		return 0;
	x0 = x1 = p->pt[0].x;
	y0 = y1 = p->pt[0].y;
	for (i = 1; i < p->n; i++) {
		if (p->pt[i].x < x0) x0 = p->pt[i].x;
		if (p->pt[i].x > x1) x1 = p->pt[i].x;
		if (p->pt[i].y < y0) y0 = p->pt[i].y;
		if (p->pt[i].y > y1) y1 = p->pt[i].y;
	}
	b->x0 = x0 >> 16;
	b->y0 = y0 >> 16;
	b->x1 = (x1 >> 16) + 1;
	b->y1 = (y1 >> 16) + 1;
	return 1;
}

/* ------------------------------------------------------------------ fill */
static int edge_add(RCANVAS *c, const RPT *a, const RPT *b)
{
	REDGE *e;
	int64_t dx, dy;

	if (a->y == b->y)
		return 1;
	if (c->ne == c->cape) {
		long nc = c->cape ? c->cape * 2 : 512;
		REDGE *q = realloc(c->e, (size_t)nc * sizeof *q);

		if (!q)
			return 0;
		c->e = q;
		c->cape = nc;
	}
	e = &c->e[c->ne++];
	if (a->y < b->y) {
		e->y0 = a->y; e->x0 = a->x; e->y1 = b->y; e->dir = 1;
		dx = (int64_t)b->x - a->x;
	} else {
		e->y0 = b->y; e->x0 = b->x; e->y1 = a->y; e->dir = -1;
		dx = (int64_t)a->x - b->x;
	}
	dy = (int64_t)e->y1 - e->y0;
	e->slope = (int32_t)((dx << 16) / dy);
	return 1;
}

static int edge_cmp(const void *a, const void *b)
{
	int32_t x = ((const REDGE *)a)->y0, y = ((const REDGE *)b)->y0;

	return x < y ? -1 : x > y;
}

static void span(int16_t *acc, int32_t xa, int32_t xb)
{
	int32_t ia, ib, v;

	if (xb <= xa)
		return;
	ia = xa >> 16;
	ib = xb >> 16;
	if (ia == ib) {
		v = (xb - xa) >> 10;
		acc[ia] += (int16_t)v;
		acc[ia + 1] -= (int16_t)v;
		return;
	}
	v = (65536 - (xa & 0xFFFF)) >> 10;
	acc[ia] += (int16_t)v;
	acc[ia + 1] -= (int16_t)v;
	acc[ia + 1] += FULL;
	acc[ib] -= FULL;
	v = (xb & 0xFFFF) >> 10;
	acc[ib] += (int16_t)v;
	acc[ib + 1] -= (int16_t)v;
}

static inline uint16_t mix(uint16_t d, uint16_t s, int c)
{
	int r, g, b;

	if (c >= 256)
		return s;
	r = ((d >> 11) * (256 - c) + (s >> 11) * c) >> 8;
	g = (((d >> 5) & 63) * (256 - c) + ((s >> 5) & 63) * c) >> 8;
	b = ((d & 31) * (256 - c) + (s & 31) * c) >> 8;
	return (uint16_t)(r << 11 | g << 5 | b);
}

static void clip_to(const RCANVAS *c, const RCLIP *in, RCLIP *out)
{
	out->x0 = 0;
	out->y0 = c->y0;		/* (the rows held) */
	out->x1 = c->w;
	out->y1 = c->y0 + c->rows < c->h ? c->y0 + c->rows : c->h;
	if (in) {
		if (in->x0 > out->x0) out->x0 = in->x0;
		if (in->y0 > out->y0) out->y0 = in->y0;
		if (in->x1 < out->x1) out->x1 = in->x1;
		if (in->y1 < out->y1) out->y1 = in->y1;
	}
}

void rc_fill(RCANVAS *c, const RPATH *p, int evenodd, uint16_t col, const RCLIP *clip)
{
	RCLIP k, b;
	long s, i, first = 0, nact = 0, *act = NULL;
	int32_t y;

	if (p->n < 2 || !rp_bounds(p, &b))
		return;
	clip_to(c, clip, &k);
	if (b.x0 > k.x0) k.x0 = b.x0;
	if (b.y0 > k.y0) k.y0 = b.y0;
	if (b.x1 < k.x1) k.x1 = b.x1;
	if (b.y1 < k.y1) k.y1 = b.y1;
	if (k.x0 >= k.x1 || k.y0 >= k.y1)
		return;
	c->ne = 0;
	for (s = 0; s < p->ns; s++) {
		long a = p->start[s], z = s + 1 < p->ns ? p->start[s + 1] : p->n;

		for (i = a; i + 1 < z; i++)
			if (!edge_add(c, &p->pt[i], &p->pt[i + 1]))
				return;
		if (z - a > 1 && !edge_add(c, &p->pt[z - 1], &p->pt[a]))	/* (a fill closes every subpath) */
			return;
	}
	if (!c->ne)
		return;
	qsort(c->e, (size_t)c->ne, sizeof *c->e, edge_cmp);
	if (c->capx < c->ne * 2) {
		int32_t *q = realloc(c->xs, (size_t)c->ne * 2 * sizeof *q);

		if (!q)
			return;
		c->xs = q;
		c->capx = c->ne * 2;
	}
	act = malloc((size_t)c->ne * sizeof *act);
	if (!act)
		return;
	for (y = k.y0; y < k.y1; y++) {
		int sub, any = 0;

		if ((y & 7) == 7 && cv_turn)
			cv_turn();
		int32_t lo = k.x0 << 16, hi = k.x1 << 16;

		for (sub = 0; sub < SUB; sub++) {
			int32_t yy = (y << 16) + (int32_t)((2 * sub + 1) << 16) / (2 * SUB);
			long nx = 0, j;

			while (first < c->ne && c->e[first].y0 <= yy)
				act[nact++] = first++;
			for (j = 0; j < nact;) {
				REDGE *e = &c->e[act[j]];

				if (e->y1 <= yy) {
					act[j] = act[--nact];
					continue;
				}
				if (e->y0 <= yy) {
					int32_t x = e->x0 + (int32_t)(((int64_t)(yy - e->y0) * e->slope) >> 16);

					/* inserted in order (x, then its winding in the low bits' place) */
					long m = nx;

					while (m > 0 && c->xs[2 * (m - 1)] > x) {
						c->xs[2 * m] = c->xs[2 * (m - 1)];
						c->xs[2 * m + 1] = c->xs[2 * m - 1];
						m--;
					}
					c->xs[2 * m] = x;
					c->xs[2 * m + 1] = e->dir;
					nx++;
				}
				j++;
			}
			{
				int w = 0;

				for (j = 0; j < nx; j++) {
					int in0 = evenodd ? (w & 1) : w != 0, in1;

					w += c->xs[2 * j + 1];
					in1 = evenodd ? (w & 1) : w != 0;
					if (!in0 && in1 && j + 1 < nx) {
						/* a span from here to where it is out again */
						int32_t xa = c->xs[2 * j], xb;
						long m = j + 1;

						for (; m < nx; m++) {
							w += c->xs[2 * m + 1];
							if (!(evenodd ? (w & 1) : w != 0))
								break;
						}
						xb = m < nx ? c->xs[2 * m] : xa;
						j = m;
						if (xa < lo) xa = lo;
						if (xb > hi) xb = hi;
						if (xb > xa) {
							span(c->acc, xa, xb);
							any = 1;
						}
					}
				}
			}
		}
		if (any) {
			int32_t x, run = 0;
			uint16_t *row = c->px + (long)(y - c->y0) * c->w;

			for (x = k.x0; x <= k.x1; x++) {
				run += c->acc[x];
				c->acc[x] = 0;
				if (run > 0 && x < k.x1)
					row[x] = mix(row[x], col, run);
			}
			c->acc[k.x1 + 1] = 0;
		}
	}
	free(act);
}

/* ------------------------------------------------------------------ stroke */
static uint32_t isqrt64(uint64_t v)
{
	uint64_t r = 0, b = (uint64_t)1 << 62;

	while (b > v)
		b >>= 2;
	while (b) {
		if (v >= r + b) {
			v -= r + b;
			r = (r >> 1) + b;
		} else
			r >>= 1;
		b >>= 2;
	}
	return (uint32_t)r;
}

/* a convex polygon added as a subpath turning the same way as every other
   (so a nonzero fill is their union) */
static void poly(RPATH *o, const RPT *q, int n)
{
	int64_t area = 0;
	int i;

	for (i = 0; i < n; i++) {
		const RPT *a = &q[i], *b = &q[(i + 1) % n];

		area += ((int64_t)a->x * b->y - (int64_t)b->x * a->y) >> 16;
	}
	if (area >= 0) {
		rp_move(o, q[0].x, q[0].y);
		for (i = 1; i < n; i++)
			rp_line(o, q[i].x, q[i].y);
	} else {
		rp_move(o, q[n - 1].x, q[n - 1].y);
		for (i = n - 2; i >= 0; i--)
			rp_line(o, q[i].x, q[i].y);
	}
	rp_close(o);
}

static void seg(RPATH *o, const RPT *a, const RPT *b, int32_t hw, int joint)
{
	int64_t dx = (int64_t)b->x - a->x, dy = (int64_t)b->y - a->y;
	uint32_t len = isqrt64((uint64_t)(dx * dx + dy * dy));
	RPT q[4];
	int32_t nx, ny;

	if (joint) {
		q[0].x = b->x - hw; q[0].y = b->y - hw;
		q[1].x = b->x + hw; q[1].y = b->y - hw;
		q[2].x = b->x + hw; q[2].y = b->y + hw;
		q[3].x = b->x - hw; q[3].y = b->y + hw;
		poly(o, q, 4);
	}
	if (!len)
		return;
	nx = (int32_t)(-dy * hw / len);
	ny = (int32_t)(dx * hw / len);
	q[0].x = a->x + nx; q[0].y = a->y + ny;
	q[1].x = b->x + nx; q[1].y = b->y + ny;
	q[2].x = b->x - nx; q[2].y = b->y - ny;
	q[3].x = a->x - nx; q[3].y = a->y - ny;
	poly(o, q, 4);
}

/* the dash's state along a subpath */
typedef struct {
	const int32_t *d;
	int n, i, on;
	int32_t left;
} DASH;

static void dash_start(DASH *s, const int32_t *d, int n, int32_t phase)
{
	int32_t total = 0;
	int k;

	s->d = d;
	s->n = n;
	s->i = 0;
	s->on = 1;
	for (k = 0; k < n; k++)
		total += d[k];
	if (n <= 0 || total <= 0) {
		s->n = 0;
		return;
	}
	if (n & 1)
		total *= 2;
	phase %= total;
	if (phase < 0)
		phase += total;
	s->left = d[0];
	while (phase >= s->left) {
		phase -= s->left;
		s->i = (s->i + 1) % n;
		s->on = !s->on;
		s->left = d[s->i];
		if (s->left <= 0)
			s->left = 1;
	}
	s->left -= phase;
}

void rc_stroke(RCANVAS *c, const RPATH *p, int32_t width, const int32_t *dash, int ndash, int32_t phase,
	       uint16_t col, const RCLIP *clip)
{
	RPATH o;
	long s, i;
	int32_t hw;

	if (p->n < 2)
		return;
	if (width < 65536)
		width = 65536;
	hw = width / 2;
	rp_init(&o);
	for (s = 0; s < p->ns && !o.bad; s++) {
		long a = p->start[s], z = s + 1 < p->ns ? p->start[s + 1] : p->n, cnt = z - a;
		int closed = p->closed[s];
		DASH ds;

		if (cnt < 2)
			continue;
		memset(&ds, 0, sizeof ds);
		dash_start(&ds, dash, ndash, phase);
		for (i = 0; i < cnt - 1 + closed; i++) {
			RPT pa = p->pt[a + i], pb = p->pt[a + (i + 1) % cnt];
			int joint = i < cnt - 2 + closed;

			if (!ds.n) {
				seg(&o, &pa, &pb, hw, joint);
				continue;
			}
			{
				/* along the segment, a dash at a time */
				int64_t dx = (int64_t)pb.x - pa.x, dy = (int64_t)pb.y - pa.y;
				int32_t len = (int32_t)isqrt64((uint64_t)(dx * dx + dy * dy)), at = 0;

				while (at < len) {
					int32_t step = ds.left < len - at ? ds.left : len - at;
					RPT q0, q1;

					q0.x = pa.x + (int32_t)(dx * at / len);
					q0.y = pa.y + (int32_t)(dy * at / len);
					q1.x = pa.x + (int32_t)(dx * (at + step) / len);
					q1.y = pa.y + (int32_t)(dy * (at + step) / len);
					if (ds.on)
						seg(&o, &q0, &q1, hw, 0);
					at += step;
					ds.left -= step;
					if (ds.left <= 0) {
						ds.i = (ds.i + 1) % ds.n;
						ds.on = !ds.on;
						ds.left = ds.d[ds.i] > 0 ? ds.d[ds.i] : 65536;
					}
				}
			}
		}
	}
	rc_fill(c, &o, 0, col, clip);
	rp_free(&o);
}

/* ------------------------------------------------------------------ bitmaps */
void rc_mask(RCANVAS *c, int x, int y, const unsigned char *a, int w, int h, int pitch, uint16_t col,
	     const RCLIP *clip)
{
	RCLIP k;
	int i, j;

	clip_to(c, clip, &k);
	for (j = 0; j < h; j++) {
		int yy = y + j;
		uint16_t *row;

		if (yy < k.y0 || yy >= k.y1)
			continue;
		row = c->px + (long)(yy - c->y0) * c->w;
		for (i = 0; i < w; i++) {
			int xx = x + i, v = a[(long)j * pitch + i];

			if (v && xx >= k.x0 && xx < k.x1)
				row[xx] = mix(row[xx], col, v + (v >> 7));
		}
	}
}

/* where along one axis of the picture a device pixel's centre falls:
   (pos + 0.5 - org) / ext of the n pixels, 0 .. n - 1, -1 outside */
static int along(int32_t pos, int32_t org, int32_t ext, int n)
{
	int64_t t;

	if (!ext)
		return -1;
	t = (((int64_t)pos << 16) + 32768 - org) * n / ext;
	if (t < 0 || t >= n)
		return -1;
	return (int)t;
}

void rc_image(RCANVAS *c, const uint16_t *px, const unsigned char *alpha, int pw, int ph, const int32_t *m,
	      int stencil, uint16_t col, const RCLIP *clip)
{
	RCLIP k;
	int32_t xs[4], ys[4], x0, x1, y0, y1;
	int i, X, Y, swap;
	int *cmap;

	/* the square's corners on the device */
	xs[0] = m[4]; ys[0] = m[5];
	xs[1] = m[4] + m[0]; ys[1] = m[5] + m[1];
	xs[2] = m[4] + m[2]; ys[2] = m[5] + m[3];
	xs[3] = m[4] + m[0] + m[2]; ys[3] = m[5] + m[1] + m[3];
	x0 = x1 = xs[0];
	y0 = y1 = ys[0];
	for (i = 1; i < 4; i++) {
		if (xs[i] < x0) x0 = xs[i];
		if (xs[i] > x1) x1 = xs[i];
		if (ys[i] < y0) y0 = ys[i];
		if (ys[i] > y1) y1 = ys[i];
	}
	clip_to(c, clip, &k);
	if ((x0 >> 16) > k.x0) k.x0 = x0 >> 16;
	if ((y0 >> 16) > k.y0) k.y0 = y0 >> 16;
	if ((x1 >> 16) + 1 < k.x1) k.x1 = (x1 >> 16) + 1;
	if ((y1 >> 16) + 1 < k.y1) k.y1 = (y1 >> 16) + 1;
	if (k.x0 >= k.x1 || k.y0 >= k.y1 || pw <= 0 || ph <= 0)
		return;
	/* a quarter turn (a, d small against b, c): the picture's columns run
	   down the device. A slant is drawn as the nearer of the two */
	swap = iabs32(m[0]) + iabs32(m[3]) < iabs32(m[1]) + iabs32(m[2]);
	cmap = malloc((size_t)(k.x1 - k.x0) * sizeof *cmap);
	if (!cmap)
		return;
	/* u from the device x (no turn: u along a; turned: v along b) */
	for (X = k.x0; X < k.x1; X++)
		cmap[X - k.x0] = swap ? along(X, m[4], m[2], ph) : along(X, m[4], m[0], pw);
	for (Y = k.y0; Y < k.y1; Y++) {
		uint16_t *row = c->px + (long)(Y - c->y0) * c->w;
		int r = swap ? along(Y, m[5], m[1], pw) : along(Y, m[5], m[3], ph);

		if (r < 0)
			continue;
		for (X = k.x0; X < k.x1; X++) {
			int q = cmap[X - k.x0], ix, iy;
			long at;

			if (q < 0)
				continue;
			if (swap) {
				ix = r;
				iy = ph - 1 - q;
			} else {
				ix = q;
				iy = ph - 1 - r;
			}
			at = (long)iy * pw + ix;
			if (stencil) {
				if (alpha[at])
					row[X] = mix(row[X], col, alpha[at] + (alpha[at] >> 7));
			} else if (!alpha)
				row[X] = px[at];
			else if (alpha[at])
				row[X] = mix(row[X], px[at], alpha[at] + (alpha[at] >> 7));
		}
	}
	free(cmap);
}

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * render.c - a page drawn by EConvert (render.h).
 *
 * The reader's interpreter runs the page with PDRAW's hooks: a path is
 * built on the device as its operators come (through the CTM and the
 * page's own matrix), painted by raster.c; colours are taken in the colour
 * space set (Gray, RGB, CMYK; ICC by its components; a separation's tint
 * as grey; Indexed through its table; a pattern or a shading by its middle
 * colour); a clip is the box of its path (most PDFs clip with rectangles);
 * glyphs come from the embedded font programs through FreeType, the
 * standard 14 from Liberation's files. Each painting's box is kept, merged
 * with its neighbours: the drawings are the busy ones (render_drawings).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "render.h"
#include "pdfimg.h"
#include "cvjpeg.h"
#include "conv.h"

/* ------------------------------------------------------------------ fixed point */
static void mat_mul6(const fx *a, const fx *b, fx *o)
{
	fx r[6];

	r[0] = fxmul(a[0], b[0]) + fxmul(a[1], b[2]);
	r[1] = fxmul(a[0], b[1]) + fxmul(a[1], b[3]);
	r[2] = fxmul(a[2], b[0]) + fxmul(a[3], b[2]);
	r[3] = fxmul(a[2], b[1]) + fxmul(a[3], b[3]);
	r[4] = fxmul(a[4], b[0]) + fxmul(a[5], b[2]) + b[4];
	r[5] = fxmul(a[4], b[1]) + fxmul(a[5], b[3]) + b[5];
	memcpy(o, r, sizeof r);
}

static fx fxa(fx v)
{
	return v < 0 ? -v : v;
}

/* a matrix's scale: the length a unit becomes, roughly */
static fx mat_scale(const fx *m)
{
	fx a = fxa(m[0]) + fxa(m[1]), b = fxa(m[2]) + fxa(m[3]);

	return (a + b) / 2;
}

static fx to_fx(const POBJ *o)
{
	if (!o)
		return 0;
	if (o->t == PO_INT)
		return o->a > 32767 ? FX_INT(32767) : o->a < -32767 ? FX_INT(-32767) : FX_INT(o->a);
	if (o->t == PO_REAL)
		return o->a;
	return 0;
}

/* ------------------------------------------------------------------ colours */
enum { CS_GRAY = 1, CS_RGB = 3, CS_CMYK = 4, CS_PATTERN = 0, CS_SEP = -1, CS_INDEXED = -2, CS_LAB = -3 };

#define NPAL 8
typedef struct {
	POBJ *key;			/* the colour space's object it came from */
	int base;			/* the base's components */
	int hival;
	unsigned char t[256 * 4];
} RPALETTE;

typedef struct {
	uint16_t col;
	int cs;				/* CS_* */
	int pal;			/* CS_INDEXED: which palette */
} RCOL;

typedef struct {
	RCOL fill, stroke;
	fx lw;				/* user units */
	RCLIP clip;
	fx dash[8];
	int ndash;
	fx phase;
} RGS;

/* a glyph's bitmap kept: its face, code and character, the matrix and
   size it was made at (a page of a novel draws some 80 glyphs 2,000
   times: FreeType a glyph at a time was 80 s of a page on a Falcon) */
typedef struct GENT {
	FT_Face face;
	uint32_t code, uni;
	int32_t k[5];
	short left, top, w, h;
	unsigned char *bm;		/* w x h, a byte a pixel */
	struct GENT *next;
} GENT;
#define GC_HASH 1021
#define GC_MAX 1200
#define GC_BYTES (384L * 1024)

struct RFONTS {
	GENT *gh[GC_HASH];
	GENT *pool;			/* GC_MAX of them, given out in turn */
	int npool;
	long gbytes;
	FT_Library lib;
	struct {
		long prog;		/* the program's object, or -1 a standard font */
		char key[48];		/* a standard font's file */
		FT_Face face;
		unsigned char *buf;	/* the program, or (disk) its RSTREAM */
		long len;		/* the program's bytes held (a stream's: 0) */
		unsigned long used;
		int bad;		/* FreeType refused it */
		int disk;		/* read from its file as FreeType asks */
	} e[24];
	unsigned long clock;
};

/* the font programs' bytes kept at most (render_font_budget; 0: any) */
long render_font_budget;
unsigned long (*render_clock)(void);

typedef struct {
	RPAGE *r;
	RGS gs[32];
	int sp;
	RPATH path;
	int clip_next;			/* W or W* seen: the path clips when it is painted */
	RPALETTE pal[NPAL];
	int npal;
} RS;

static uint16_t rgb_of(int nc, const fx *v)
{
	int r, g, b, k;

#define C8(f) ((f) <= 0 ? 0 : (f) >= FX_ONE ? 255 : (int)((f) >> 8) & 255)
	switch (nc) {
	case 1:
		r = g = b = C8(v[0]);
		break;
	case 3:
		r = C8(v[0]);
		g = C8(v[1]);
		b = C8(v[2]);
		break;
	case 4:
		k = 255 - C8(v[3]);
		r = (255 - C8(v[0])) * k / 255;
		g = (255 - C8(v[1])) * k / 255;
		b = (255 - C8(v[2])) * k / 255;
		break;
	default:
		r = g = b = 128;
	}
#undef C8
	return rgb565(r, g, b);
}

/* a function's middle colour: an exponential's C0 and C1 averaged, a
   stitching one's first and last; else grey */
static int fn_mid(PDOC *d, POBJ *fn, int nc, fx *out)
{
	POBJ *c0, *c1;
	int k, t;

	if (fn && fn->t == PO_ARR && fn->a > 0)
		fn = pdf_res(d, &fn->u.v[0]);
	if (!fn || fn->t != PO_DICT)
		return 0;
	t = (int)pd_int(d, fn, "FunctionType", -1);
	if (t == 3) {
		POBJ *fs = pd_get(d, fn, "Functions");
		fx a[4], b[4];

		if (!fs || fs->t != PO_ARR || fs->a < 1)
			return 0;
		if (!fn_mid(d, pdf_res(d, &fs->u.v[0]), nc, a) || !fn_mid(d, pdf_res(d, &fs->u.v[fs->a - 1]), nc, b))
			return 0;
		for (k = 0; k < nc; k++)
			out[k] = (a[k] + b[k]) / 2;
		return 1;
	}
	if (t != 2)
		return 0;
	c0 = pd_get(d, fn, "C0");
	c1 = pd_get(d, fn, "C1");
	for (k = 0; k < nc; k++) {
		fx a = c0 && c0->t == PO_ARR ? to_fx(pa_at(c0, k)) : 0;
		fx b = c1 && c1->t == PO_ARR ? to_fx(pa_at(c1, k)) : FX_ONE;

		out[k] = (a + b) / 2;
	}
	return 1;
}

static int cs_comps(PDOC *d, POBJ *cs);

/* a shading's middle colour */
static uint16_t shading_mid(PDOC *d, POBJ *sh)
{
	fx v[4] = { FX_ONE / 2, FX_ONE / 2, FX_ONE / 2, 0 };
	int nc;

	if (!sh || sh->t != PO_DICT)
		return rgb565(192, 192, 192);
	nc = cs_comps(d, pd_get(d, sh, "ColorSpace"));
	if (nc != 1 && nc != 3 && nc != 4)
		nc = 3;
	if (!fn_mid(d, pd_get(d, sh, "Function"), nc, v)) {
		POBJ *bg = pd_get(d, sh, "Background");
		int k;

		if (!bg || bg->t != PO_ARR)
			return rgb565(192, 192, 192);
		for (k = 0; k < nc; k++)
			v[k] = to_fx(pa_at(bg, k));
	}
	return rgb_of(nc, v);
}

/* the components a colour space takes, as the CS_* above (Indexed and
   separations aside) */
static int cs_comps(PDOC *d, POBJ *cs)
{
	const char *n;

	if (!cs)
		return CS_GRAY;
	if (cs->t == PO_NAME) {
		n = cs->u.p;
		if (!strcmp(n, "DeviceGray") || !strcmp(n, "G") || !strcmp(n, "CalGray"))
			return CS_GRAY;
		if (!strcmp(n, "DeviceRGB") || !strcmp(n, "RGB") || !strcmp(n, "CalRGB"))
			return CS_RGB;
		if (!strcmp(n, "DeviceCMYK") || !strcmp(n, "CMYK"))
			return CS_CMYK;
		if (!strcmp(n, "Pattern"))
			return CS_PATTERN;
		return CS_GRAY;
	}
	if (cs->t == PO_ARR && cs->a > 0) {
		POBJ *k = pdf_res(d, &cs->u.v[0]);

		if (!k || k->t != PO_NAME)
			return CS_GRAY;
		n = k->u.p;
		if (!strcmp(n, "ICCBased")) {
			POBJ *s = cs->a > 1 ? pdf_res(d, &cs->u.v[1]) : NULL;
			long c = s && s->t == PO_DICT ? pd_int(d, s, "N", 3) : 3;

			return c == 1 ? CS_GRAY : c == 4 ? CS_CMYK : CS_RGB;
		}
		if (!strcmp(n, "Separation") || !strcmp(n, "DeviceN"))
			return CS_SEP;
		if (!strcmp(n, "Indexed") || !strcmp(n, "I"))
			return CS_INDEXED;
		if (!strcmp(n, "Lab"))
			return CS_LAB;
		if (!strcmp(n, "Pattern"))
			return CS_PATTERN;
		return cs_comps(d, k);
	}
	return CS_GRAY;
}

/* an Indexed space's table, kept (NPAL of them) */
static int palette(RS *s, POBJ *cs)
{
	PDOC *d = s->r->d;
	RPALETTE *p;
	POBJ *lk;
	int i;

	for (i = 0; i < s->npal; i++)
		if (s->pal[i].key == cs)
			return i;
	i = s->npal < NPAL ? s->npal++ : NPAL - 1;
	p = &s->pal[i];
	memset(p, 0, sizeof *p);
	p->key = cs;
	p->base = cs->a > 1 ? cs_comps(d, pdf_res(d, &cs->u.v[1])) : CS_RGB;
	if (p->base != 1 && p->base != 3 && p->base != 4)
		p->base = 3;
	p->hival = cs->a > 2 ? (int)(to_fx(pdf_res(d, &cs->u.v[2])) >> 16) : 0;
	if (p->hival > 255)
		p->hival = 255;
	lk = cs->a > 3 ? pdf_res(d, &cs->u.v[3]) : NULL;
	if (lk && lk->t == PO_STR) {
		long n = lk->a < 256 * 4 ? lk->a : 256 * 4;

		memcpy(p->t, lk->u.p, (size_t)n);
	} else if (lk && lk->t == PO_DICT) {
		SBUF b;
		const char *term = NULL;

		if (pdf_stream(d, lk, cs->u.v[3].t == PO_REF ? cs->u.v[3].a : -1, &b, &term, 4096) == 0 && !term)
			memcpy(p->t, b.s, (size_t)(b.len < 256 * 4 ? b.len : 256 * 4));
		sb_free(&b);
	}
	return i;
}

static POBJ *res_get(PDOC *d, POBJ *res, const char *cat, const char *name)
{
	POBJ *c = res ? pd_get(d, res, cat) : NULL;

	return c && c->t == PO_DICT && name ? pd_get(d, c, name) : NULL;
}

static void set_space(RS *s, RCOL *c, const char *name, POBJ *res)
{
	PDOC *d = s->r->d;
	POBJ nm, *cs;

	nm.t = PO_NAME;
	nm.u.p = name ? name : "DeviceGray";
	cs = cs_comps(d, &nm) == CS_GRAY && name && strcmp(name, "DeviceGray") && strcmp(name, "G") && strcmp(name, "CalGray")
		? res_get(d, res, "ColorSpace", name) : &nm;
	if (!cs)
		cs = &nm;
	c->cs = cs_comps(d, cs);
	if (c->cs == CS_INDEXED)
		c->pal = palette(s, cs);
	/* the space's initial colour: black (a separation's: no ink) */
	c->col = c->cs == CS_SEP ? 0xFFFF : 0;
}

static void set_colour(RS *s, RCOL *c, const fx *v, int n, const char *name, POBJ *res)
{
	PDOC *d = s->r->d;

	switch (c->cs) {
	case CS_PATTERN: {
		POBJ *pt = res_get(d, res, "Pattern", name);

		if (pt && pd_int(d, pt, "PatternType", 1) == 2)
			c->col = shading_mid(d, pd_get(d, pt, "Shading"));
		else if (n >= 3)
			c->col = rgb_of(n >= 4 ? 4 : 3, v);
		else
			c->col = rgb565(192, 192, 192);
		break;
	}
	case CS_SEP: {
		fx t = 0;
		int k;

		for (k = 0; k < n; k++)
			if (v[k] > t)
				t = v[k];
		t = FX_ONE - t;
		c->col = rgb_of(1, &t);
		break;
	}
	case CS_INDEXED: {
		RPALETTE *p = &s->pal[c->pal];
		int i = n > 0 ? (int)(v[0] >> 16) : 0, k;
		fx q[4];

		if (i < 0) i = 0;
		if (i > p->hival) i = p->hival;
		for (k = 0; k < p->base; k++)
			q[k] = (fx)p->t[i * p->base + k] << 8;
		c->col = rgb_of(p->base, q);
		break;
	}
	case CS_LAB:
		c->col = n > 0 ? rgb_of(1, (fx[]){ v[0] / 100 }) : 0;
		break;
	default:
		c->col = rgb_of(n == 1 || n == 3 || n == 4 ? n : c->cs, v);
	}
}

/* ------------------------------------------------------------------ areas */
#define NEAR 6				/* pixels apart that still merge */

static int touches(const RCLIP *a, const RCLIP *b)
{
	return a->x0 - NEAR < b->x1 && b->x0 - NEAR < a->x1 && a->y0 - NEAR < b->y1 && b->y0 - NEAR < a->y1;
}

static void grow(RCLIP *a, const RCLIP *b)
{
	if (b->x0 < a->x0) a->x0 = b->x0;
	if (b->y0 < a->y0) a->y0 = b->y0;
	if (b->x1 > a->x1) a->x1 = b->x1;
	if (b->y1 > a->y1) a->y1 = b->y1;
}

static void area_add(RPAGE *r, const RCLIP *b, long curves, int colour, int rule)
{
	RAREA *a;
	long i;

	for (i = 0; i < r->narea; i++)
		if (touches(&r->area[i].box, b)) {
			a = &r->area[i];
			grow(&a->box, b);
			a->paths++;
			a->curves += curves;
			a->colours += colour;
			a->rules += rule;
			return;
		}
	if (r->narea == r->caparea) {
		long nc = r->caparea ? r->caparea * 2 : 64;
		RAREA *q = realloc(r->area, (size_t)nc * sizeof *q);

		if (!q)
			return;
		r->area = q;
		r->caparea = nc;
	}
	a = &r->area[r->narea++];
	a->box = *b;
	a->paths = 1;
	a->curves = curves;
	a->colours = colour;
	a->rules = rule;
}

/* areas that came to touch as they grew, merged */
static void areas_merge(RPAGE *r)
{
	int again = 1;

	while (again) {
		long i, j;

		again = 0;
		for (i = 0; i < r->narea; i++)
			for (j = i + 1; j < r->narea; j++)
				if (touches(&r->area[i].box, &r->area[j].box)) {
					RAREA *a = &r->area[i], *b = &r->area[j];

					grow(&a->box, &b->box);
					a->paths += b->paths;
					a->curves += b->curves;
					a->colours += b->colours;
					a->rules += b->rules;
					*b = r->area[--r->narea];
					again = 1;
					j--;
				}
	}
}

static int is_colour(uint16_t c)
{
	int r = (c >> 11) << 3, g = ((c >> 5) & 63) << 2, b = (c & 31) << 3, mx, mn;

	mx = r > g ? r : g;
	mx = b > mx ? b : mx;
	mn = r < g ? r : g;
	mn = b < mn ? b : mn;
	return mx - mn > 40;
}

/* ------------------------------------------------------------------ painting */
static void paint(RS *s, int fill, int evenodd, int stroke, int close)
{
	if (cv_turn)
		cv_turn();		/* (each painting a turn: a map's are many) */
	RPAGE *r = s->r;
	RGS *g = &s->gs[s->sp];
	RCLIP b;

	if (close)
		rp_close(&s->path);
	if (rp_bounds(&s->path, &b)) {
		long page = (long)r->c.w * r->c.h, area;
		RCLIP k = b;

		if (k.x0 < g->clip.x0) k.x0 = g->clip.x0;
		if (k.y0 < g->clip.y0) k.y0 = g->clip.y0;
		if (k.x1 > g->clip.x1) k.x1 = g->clip.x1;
		if (k.y1 > g->clip.y1) k.y1 = g->clip.y1;
		area = (long)(k.x1 - k.x0) * (k.y1 - k.y0);
		if (fill)
			rc_fill(&r->c, &s->path, evenodd, g->fill.col, &g->clip);
		if (stroke) {
			int32_t dash[8];
			int i;

			for (i = 0; i < g->ndash; i++)
				dash[i] = g->dash[i];
			rc_stroke(&r->c, &s->path, g->lw, dash, g->ndash, g->phase, g->stroke.col, &g->clip);
		}
		/* a background (most of the page) isn't a drawing's part */
		if (k.x1 > k.x0 && k.y1 > k.y0 && area < page * 7 / 10) {
			int rule = !s->path.curves && (k.x1 - k.x0 < 4 || k.y1 - k.y0 < 4 || s->path.n <= 5);
			int colour = (fill && is_colour(g->fill.col)) || (stroke && is_colour(g->stroke.col));

			area_add(r, &k, s->path.curves, colour, rule);
		}
	}
	if (s->clip_next && rp_bounds(&s->path, &b)) {
		if (b.x0 > g->clip.x0) g->clip.x0 = b.x0;
		if (b.y0 > g->clip.y0) g->clip.y0 = b.y0;
		if (b.x1 < g->clip.x1) g->clip.x1 = b.x1;
		if (b.y1 < g->clip.y1) g->clip.y1 = b.y1;
	} else if (s->clip_next)
		g->clip.x1 = g->clip.x0;	/* an empty clip */
	s->clip_next = 0;
	rp_reset(&s->path);
}

static void dev(const RS *s, const fx *m, fx x, fx y, int32_t *dx, int32_t *dy)
{
	(void)s;
	*dx = fxmul(x, m[0]) + fxmul(y, m[2]) + m[4];
	*dy = fxmul(x, m[1]) + fxmul(y, m[3]) + m[5];
}

static void op_hook(void *u, const char *op, const fx *v, int n, const char *name, const fx *ctm, POBJ *res)
{
	RS *s = u;
	RPAGE *r = s->r;
	RGS *g = &s->gs[s->sp];
	fx m[6];
	int32_t x, y, x1, y1, x2, y2;

	mat_mul6(ctm, r->base, m);
	switch (op[0]) {
	case 'q':
		if (!op[1] && s->sp < 31) {
			s->gs[s->sp + 1] = *g;
			s->sp++;
		}
		return;
	case 'Q':
		if (!op[1] && s->sp > 0)
			s->sp--;
		return;
	case 'm':
		if (!op[1] && n >= 2) {
			dev(s, m, v[n - 2], v[n - 1], &x, &y);
			rp_move(&s->path, x, y);
		}
		return;
	case 'l':
		if (!op[1] && n >= 2) {
			dev(s, m, v[n - 2], v[n - 1], &x, &y);
			rp_line(&s->path, x, y);
		}
		return;
	case 'c':
		if (!op[1] && n >= 6) {
			v += n - 6;
			dev(s, m, v[0], v[1], &x, &y);
			dev(s, m, v[2], v[3], &x1, &y1);
			dev(s, m, v[4], v[5], &x2, &y2);
			rp_curve(&s->path, x, y, x1, y1, x2, y2);
		} else if (op[1] == 's' && !op[2]) {		/* cs */
			set_space(s, &g->fill, name, res);
		}
		return;
	case 'v':
		if (!op[1] && n >= 4) {
			v += n - 4;
			dev(s, m, v[0], v[1], &x1, &y1);
			dev(s, m, v[2], v[3], &x2, &y2);
			rp_curve(&s->path, s->path.cx, s->path.cy, x1, y1, x2, y2);
		}
		return;
	case 'y':
		if (!op[1] && n >= 4) {
			v += n - 4;
			dev(s, m, v[0], v[1], &x1, &y1);
			dev(s, m, v[2], v[3], &x2, &y2);
			rp_curve(&s->path, x1, y1, x2, y2, x2, y2);
		}
		return;
	case 'h':
		if (!op[1])
			rp_close(&s->path);
		return;
	case 'r':
		if (op[1] == 'e' && !op[2] && n >= 4) {
			fx a = v[n - 4], b = v[n - 3], w = v[n - 2], h = v[n - 1];

			dev(s, m, a, b, &x, &y);
			rp_move(&s->path, x, y);
			dev(s, m, a + w, b, &x, &y);
			rp_line(&s->path, x, y);
			dev(s, m, a + w, b + h, &x, &y);
			rp_line(&s->path, x, y);
			dev(s, m, a, b + h, &x, &y);
			rp_line(&s->path, x, y);
			rp_close(&s->path);
		} else if (op[1] == 'g' && !op[2]) {		/* rg */
			if (n >= 3) {
				g->fill.cs = CS_RGB;
				g->fill.col = rgb_of(3, v + n - 3);
			}
		}
		return;
	case 'R':
		if (op[1] == 'G' && !op[2] && n >= 3) {
			g->stroke.cs = CS_RGB;
			g->stroke.col = rgb_of(3, v + n - 3);
		}
		return;
	case 'g':
		if (!op[1] && n >= 1) {
			g->fill.cs = CS_GRAY;
			g->fill.col = rgb_of(1, v + n - 1);
		}
		return;
	case 'G':
		if (!op[1] && n >= 1) {
			g->stroke.cs = CS_GRAY;
			g->stroke.col = rgb_of(1, v + n - 1);
		}
		return;
	case 'k':
		if (!op[1] && n >= 4) {
			g->fill.cs = CS_CMYK;
			g->fill.col = rgb_of(4, v + n - 4);
		}
		return;
	case 'K':
		if (!op[1] && n >= 4) {
			g->stroke.cs = CS_CMYK;
			g->stroke.col = rgb_of(4, v + n - 4);
		}
		return;
	case 'C':
		if (op[1] == 'S' && !op[2])
			set_space(s, &g->stroke, name, res);
		return;
	case 's':
		if (!strcmp(op, "sc") || !strcmp(op, "scn"))
			set_colour(s, &g->fill, v, n, name, res);
		else if (!strcmp(op, "sh")) {
			RPATH p;
			int32_t a0 = g->clip.x0 << 16, b0 = g->clip.y0 << 16, a1 = g->clip.x1 << 16, b1 = g->clip.y1 << 16;

			rp_init(&p);
			rp_move(&p, a0, b0);
			rp_line(&p, a1, b0);
			rp_line(&p, a1, b1);
			rp_line(&p, a0, b1);
			rp_close(&p);
			rc_fill(&r->c, &p, 0, shading_mid(r->d, res_get(r->d, res, "Shading", name)), &g->clip);
			rp_free(&p);
		} else if (!op[1])
			paint(s, 0, 0, 1, 1);
		return;
	case 'S':
		if (!strcmp(op, "SC") || !strcmp(op, "SCN"))
			set_colour(s, &g->stroke, v, n, name, res);
		else if (!op[1])
			paint(s, 0, 0, 1, 0);
		return;
	case 'f':
	case 'F':
		if (!op[1] || (op[1] == '*' && !op[2]))
			paint(s, 1, op[1] == '*', 0, 0);
		return;
	case 'B':			/* (not BI, BDC, BMC, BX) */
		if (!op[1] || (op[1] == '*' && !op[2]))
			paint(s, 1, op[1] == '*', 1, 0);
		return;
	case 'b':
		if (!op[1] || (op[1] == '*' && !op[2]))
			paint(s, 1, op[1] == '*', 1, 1);
		return;
	case 'n':
		if (!op[1])
			paint(s, 0, 0, 0, 0);
		return;
	case 'W':
		if (!op[1] || (op[1] == '*' && !op[2]))
			s->clip_next = 1;
		return;
	case 'w':
		if (!op[1] && n >= 1)
			g->lw = fxmul(v[n - 1], mat_scale(m));
		return;
	case 'd':
		if (!op[1]) {
			int k;
			fx sc = mat_scale(m);

			g->ndash = n - 1 < 8 ? n - 1 : 8;
			if (g->ndash < 0)
				g->ndash = 0;
			for (k = 0; k < g->ndash; k++)
				g->dash[k] = fxmul(v[k], sc);
			g->phase = n > 0 ? fxmul(v[n - 1], sc) : 0;
		}
		return;
	default:
		return;
	}
}

/* ------------------------------------------------------------------ glyphs */
static const char *std_file(const char *base)
{
	static char f[48];
	const char *fam = "Sans";
	int bold, ital;

	if (!base)
		base = "";
	if (strstr(base, "Symbol") || strstr(base, "Dingbats"))
		return NULL;
	if (strstr(base, "Times") || strstr(base, "Serif") || strstr(base, "Roman") || strstr(base, "Georgia") ||
	    strstr(base, "Palatino") || strstr(base, "Book") || strstr(base, "Garamond"))
		fam = "Serif";
	if (strstr(base, "Courier") || strstr(base, "Mono"))
		fam = "Mono";
	bold = strstr(base, "Bold") != NULL || strstr(base, "Black") != NULL || strstr(base, "Heavy") != NULL;
	ital = strstr(base, "Italic") != NULL || strstr(base, "Oblique") != NULL;
	/* the file's 8.3 name, as EConvert's release carries it in fonts\
	   (sans.ttf, sansb.ttf, sansi.ttf, sansz.ttf: bold and italic) */
	snprintf(f, sizeof f, "%s%s.ttf", fam[0] == 'S' && fam[1] == 'a' ? "sans" : fam[0] == 'S' ? "serif" : "mono",
		 bold && ital ? "z" : bold ? "b" : ital ? "i" : "");
	return f;
}

/* a standard font read from its file as FreeType asks for its parts,
   through a window of 4 KB, never held whole: Liberation's files are some
   400 KB each, and a 4 MB Falcon drawing a page beside render.ovl had no
   room for two (the Times on the showcase's last page wasn't drawn,
   7 Nov 2026) */
#define RS_WIN 4096
typedef struct {
	FT_StreamRec st;
	void *h;
	long woff, wlen;
	unsigned char win[RS_WIN];
} RSTREAM;

static unsigned long rs_read(FT_Stream st, unsigned long off, unsigned char *buf, unsigned long n)
{
	RSTREAM *s = (RSTREAM *)st;

	if (n == 0)
		return off <= st->size ? 0 : 1;	/* a seek: 0 ok */
	if (off >= st->size)
		return 0;
	if (n > st->size - off)
		n = st->size - off;
	if (n > RS_WIN)
		return (unsigned long)(pdf_os_read(s->h, (long)off, buf, (long)n) == (long)n ? n : 0);
	if ((long)off < s->woff || (long)(off + n) > s->woff + s->wlen) {
		long k = (long)(st->size - off) < RS_WIN ? (long)(st->size - off) : RS_WIN;

		s->wlen = 0;
		if (pdf_os_read(s->h, (long)off, s->win, k) != k)
			return 0;
		s->woff = (long)off;
		s->wlen = k;
	}
	memcpy(buf, s->win + (off - (unsigned long)s->woff), n);
	return n;
}

static void rs_close(FT_Stream st)
{
	RSTREAM *s = (RSTREAM *)st;

	if (s->h)
		pdf_os_close(s->h);
	s->h = NULL;
}

static RSTREAM *rs_open(const char *path)
{
	long size = 0;
	void *h = pdf_os_open(path, &size);
	RSTREAM *s;

	if (!h)
		return NULL;
	if (size <= 0 || !(s = calloc(1, sizeof *s))) {
		pdf_os_close(h);
		return NULL;
	}
	s->h = h;
	s->st.size = (unsigned long)size;
	s->st.read = rs_read;
	s->st.close = rs_close;
	s->woff = -RS_WIN - 1;
	return s;
}

static void gc_flush(RFONTS *f)
{
	int i;

	for (i = 0; i < f->npool; i++) {
		free(f->pool[i].bm);
		if ((i & 63) == 63 && cv_turn)
			cv_turn();	/* (1,200 blocks freed: some 100 ms of a Falcon) */
	}
	f->npool = 0;
	f->gbytes = 0;
	memset(f->gh, 0, sizeof f->gh);
}

static FT_Face face_for(RPAGE *r, long prog, const char *base)
{
	RFONTS *f = r->fonts;
	const char *file = prog < 0 ? std_file(base) : NULL;
	int i, v = 0;

	if (prog < 0 && (!file || !r->fontdir))
		return NULL;
	f->clock++;
	for (i = 0; i < 24; i++)
		if ((f->e[i].face || f->e[i].bad) && f->e[i].prog == prog && (prog >= 0 || !strcmp(f->e[i].key, file))) {
			f->e[i].used = f->clock;
			return f->e[i].face;
		}
	for (i = 1; i < 24; i++)
		if (f->e[i].used < f->e[v].used)
			v = i;
	if (f->e[v].face) {
		gc_flush(f);		/* (its glyphs: a new face may get its address) */
		FT_Done_Face(f->e[v].face);
	}
	free(f->e[v].buf);
	memset(&f->e[v], 0, sizeof f->e[v]);
	f->e[v].prog = prog;
	f->e[v].used = f->clock;
	if (prog >= 0) {
		POBJ *o = pdf_get(r->d, prog);
		SBUF b;
		const char *term = NULL;
		PCSRC *src = o && o->t == PO_DICT ? pdf_stream_open(r->d, o, prog) : NULL;

		sb_init(&b);
		if (src) {
			/* its size as the font's dictionary gives it, room for that
			   once (doubling from 4 KB made a 300 KB font 512 KB, a
			   quarter of a 4 MB Falcon's room while a page is drawn) */
			long want = pd_int(r->d, o, "Length1", 0) + pd_int(r->d, o, "Length2", 0) +
				    pd_int(r->d, o, "Length3", 0);

			if (want > 0 && want < (8L << 20))
				sb_reserve(&b, want + 16);
		}
		if (src) {
			/* a piece at a time, a turn between (a font unpacked at once
			   was a stretch of 150 ms on a Falcon) */
			unsigned char piece[4096];
			long k;

			while ((k = pdf_stream_read(src, piece, sizeof piece)) > 0 && b.len < (8L << 20)) {
				if (!sb_add(&b, (const char *)piece, k))
					break;
				if (cv_turn)
					cv_turn();
			}
			pdf_stream_close(src);
			if (k < 0)
				sb_free(&b);
		}
		if (src ? b.len > 0 : (o && o->t == PO_DICT && pdf_stream(r->d, o, prog, &b, &term, 8L << 20) == 0 && !term && b.len > 0)) {
			/* (the room past it given back: only a buffer read here,
			   pdf_stream's may not be the heap's own) */
			char *q = src ? realloc(b.s, (size_t)b.len + 1) : NULL;

			f->e[v].buf = (unsigned char *)(q ? q : b.s);
			f->e[v].len = b.len;
		} else
			sb_free(&b);
	} else {
		char path[300];

		/* (the folder's own separator: GEMDOS's \, a Unix /) */
		char sep = strchr(r->fontdir, '\\') ? '\\' : '/';

		snprintf(f->e[v].key, sizeof f->e[v].key, "%s", file);
		snprintf(path, sizeof path, "%s%c%s", r->fontdir, sep, file);
		f->e[v].disk = 1;
		f->e[v].buf = (unsigned char *)rs_open(path);
		if (!f->e[v].buf) {
			/* Liberation's own names (a Linux system's fonts) */
			static const char *const fam[3] = { "Sans", "Serif", "Mono" };
			const char *st = strchr(file, '.')[-1] == 'z' ? "BoldItalic" : strchr(file, '.')[-1] == 'b' ? "Bold" :
					 strchr(file, '.')[-1] == 'i' ? "Italic" : "Regular";

			snprintf(path, sizeof path, "%s%cLiberation%s-%s.ttf", r->fontdir, sep,
				 fam[file[1] == 'a' ? 0 : file[1] == 'e' ? 1 : 2], st);
			f->e[v].buf = (unsigned char *)rs_open(path);
		}
		if (!f->e[v].buf) {
			/* an italic face the release doesn't carry: the upright one
			   (sansi.ttf -> sans.ttf, sansz.ttf -> sansb.ttf) */
			char up[48], *d;

			snprintf(up, sizeof up, "%s", file);
			d = strchr(up, '.');
			if (d && (d[-1] == 'i' || d[-1] == 'z')) {
				if (d[-1] == 'z')
					d[-1] = 'b';
				else
					memmove(d - 1, d, strlen(d) + 1);
				snprintf(path, sizeof path, "%s%c%s", r->fontdir, sep, up);
				f->e[v].buf = (unsigned char *)rs_open(path);
			}
		}
	}
	if (cv_turn)
		cv_turn();
	cv_phase = "face";
	if (f->e[v].buf && f->e[v].disk) {
		FT_Open_Args a;

		memset(&a, 0, sizeof a);
		a.flags = FT_OPEN_STREAM;
		a.stream = &((RSTREAM *)f->e[v].buf)->st;
		if (FT_Open_Face(f->lib, &a, 0, &f->e[v].face)) {
			rs_close(a.stream);
			f->e[v].face = NULL;
		}
	} else if (f->e[v].buf && FT_New_Memory_Face(f->lib, f->e[v].buf, f->e[v].len, 0, &f->e[v].face))
		f->e[v].face = NULL;
	if (!f->e[v].face) {
		cv_phase = "render";
		f->e[v].bad = 1;
		return NULL;
	}
	cv_phase = "render";
	if (cv_turn)
		cv_turn();
	{
		/* the font programs kept within render_font_budget, the least
		   lately used let go first (each standard font is a Liberation
		   file of some 400 KB: the rigs' showcase, six of them, held 2.4
		   MB and a 4 MB Falcon's page found no room) */
		long total = 0;

		for (i = 0; i < 24; i++)
			total += f->e[i].buf ? f->e[i].len : 0;
		while (render_font_budget > 0 && total > render_font_budget) {
			int lru = -1;

			for (i = 0; i < 24; i++)
				if (i != v && f->e[i].buf && f->e[i].len > 0 && (lru < 0 || f->e[i].used < f->e[lru].used))
					lru = i;
			if (lru < 0)
				break;
			total -= f->e[lru].len;
			gc_flush(f);		/* (its glyphs go too: their face is gone) */
			FT_Done_Face(f->e[lru].face);
			free(f->e[lru].buf);
			memset(&f->e[lru], 0, sizeof f->e[lru]);
		}
	}
	return f->e[v].face;
}

static FT_UInt glyph_index(FT_Face face, int comp, uint32_t code, uint32_t uni)
{
	FT_UInt gid = 0;
	int i;

	if (comp)
		return code;
	if (uni && !FT_Select_Charmap(face, FT_ENCODING_UNICODE))
		gid = FT_Get_Char_Index(face, uni);
	for (i = 0; !gid && i < face->num_charmaps; i++) {
		FT_CharMap cm = face->charmaps[i];

		if (FT_Set_Charmap(face, cm))
			continue;
		if (cm->platform_id == 3 && cm->encoding_id == 0) {
			gid = FT_Get_Char_Index(face, 0xF000u + (code & 255));
			if (!gid)
				gid = FT_Get_Char_Index(face, code);
		} else if (cm->platform_id != 3 || cm->encoding_id != 1)
			gid = FT_Get_Char_Index(face, code);
	}
	return gid;
}

#define MONO_INK 64		/* of 255: a mono glyph's pixel inked from a quarter */
static void glyph_hook(void *u, long prog, int kind, const char *base, int comp, uint32_t code, uint32_t uni,
		       const fx *gm, int mode)
{
	RS *s = u;
	RPAGE *r = s->r;
	RGS *g = &s->gs[s->sp];
	FT_Face face;
	FT_UInt gid;
	fx m[6], sz;
	FT_Matrix t;
	FT_Vector dl;
	int32_t ox, oy;

	(void)kind;
	if ((mode & 3) == 3)
		return;			/* invisible (a scan's text layer), or a clip only */
	r->glyphs++;
	mat_mul6(gm, r->base, m);
	sz = mat_scale(m);
	if (sz < FX_ONE || sz > FX_INT(2000)) {
		if (sz >= FX_ONE)
			r->glyphs_missed++;
		return;
	}
	face = face_for(r, prog >= 0 ? prog : -1, base);
	if (!face) {
		r->glyphs_missed++;
		return;
	}
	/* em to device, over its size: y up in FreeType's space, down on the device */
	t.xx = (FT_Fixed)((int64_t)m[0] * 65536 / sz);
	t.xy = (FT_Fixed)((int64_t)m[2] * 65536 / sz);
	t.yx = (FT_Fixed)(-(int64_t)m[1] * 65536 / sz);
	t.yy = (FT_Fixed)(-(int64_t)m[3] * 65536 / sz);
	/* the origin at a whole pixel (half a pixel off at most, on a page of
	   640): one bitmap does for every place the glyph comes */
	ox = (m[4] + 0x8000) >> 16;
	oy = (m[5] + 0x8000) >> 16;
	{
		RFONTS *f = r->fonts;
		int32_t k[5];
		unsigned long hv;
		GENT *e;

		k[0] = t.xx >> 6;
		k[1] = t.xy >> 6;
		k[2] = t.yx >> 6;
		k[3] = t.yy >> 6;
		k[4] = (sz >> 10) * 2 + (r->mono_glyphs != 0);	/* (a mono glyph isn't a grey one) */
		hv = ((unsigned long)(uintptr_t)face >> 4) ^ code * 31u ^ uni * 131u ^ (unsigned long)k[4] * 7u ^
		     (unsigned long)k[0] * 3u;
		for (e = f->gh[hv % GC_HASH]; e; e = e->next)
			if (e->face == face && e->code == code && e->uni == uni && !memcmp(e->k, k, sizeof k))
				break;
		if (!e) {
			FT_Bitmap *bm;
			int y;

			cv_phase = "glyph";
			if (!(gid = glyph_index(face, comp, code, uni)) || FT_Set_Char_Size(face, 0, (FT_F26Dot6)(sz >> 10), 72, 72)) {
				r->glyphs_missed++;
				cv_phase = "render";
				return;
			}
			dl.x = dl.y = 0;
			FT_Set_Transform(face, &t, &dl);
			/* made as ever (unhinted, grey-edged); the mono page's then a
			   bit a pixel below, every pixel a quarter inked (MONO_INK)
			   black: crisp at 10 pixels with every stroke. FreeType's own
			   mono rendering lost an r's arm and a t's bar at that size
			   (EDOS_01.PDF), and its light hinting (the auto-hinter) read
			   no better and held a Falcon 305 ms at a font's first glyph */
			if (FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING) ||
			    FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) {
				r->glyphs_missed++;
				cv_phase = "render";
				return;
			}
			bm = &face->glyph->bitmap;
			cv_phase = "render";
			if (cv_turn)
				cv_turn();	/* (a glyph made: some ms of FreeType on a Falcon) */
			if (bm->pixel_mode != FT_PIXEL_MODE_GRAY)
				return;
			if (!f->pool && !(f->pool = malloc(GC_MAX * sizeof *f->pool)))
				return;
			if (f->npool == GC_MAX || f->gbytes > GC_BYTES)
				gc_flush(f);
			e = &f->pool[f->npool];
			e->face = face;
			e->code = code;
			e->uni = uni;
			memcpy(e->k, k, sizeof k);
			e->left = (short)face->glyph->bitmap_left;
			e->top = (short)face->glyph->bitmap_top;
			e->w = (short)bm->width;
			e->h = (short)bm->rows;
			e->bm = e->w * e->h > 0 ? malloc((size_t)e->w * e->h) : NULL;
			if (e->w * e->h > 0 && !e->bm)
				return;
			if (r->mono_glyphs) {
				int x;

				for (y = 0; y < e->h; y++)
					for (x = 0; x < e->w; x++)
						e->bm[(long)y * e->w + x] = bm->buffer[(long)y * bm->pitch + x] > MONO_INK ? 255 : 0;
			} else
				for (y = 0; y < e->h; y++)
					memcpy(e->bm + (long)y * e->w, bm->buffer + (long)y * bm->pitch, (size_t)e->w);
			f->gbytes += (long)e->w * e->h + (long)sizeof *e;
			e->next = f->gh[hv % GC_HASH];
			f->gh[hv % GC_HASH] = e;
			f->npool++;
		}
		/* stroked only (Tr 1, a label's halo): in the stroke's colour */
		if (e->bm)
			rc_mask(&r->c, ox + e->left, oy - e->top, e->bm, e->w, e->h, e->w,
				(mode & 3) == 1 ? g->stroke.col : g->fill.col, &g->clip);
	}
	/* (a line's glyphs from the cache: some ms of the Falcon's every 4) */
	if ((r->glyphs & 3) == 0 && cv_turn)
		cv_turn();
}

/* ------------------------------------------------------------------ pictures */
static void image_hook(void *u, long obj, const fx *ctm)
{
	RS *s = u;
	RPAGE *r = s->r;
	RGS *g = &s->gs[s->sp];
	POBJ *o = obj >= 0 ? pdf_get(r->d, obj) : NULL;
	long sw, sh, w, h;
	fx m[6];
	int why = 0, mask;
	PIC *p;
	unsigned long t0;

	if (!o || o->t != PO_DICT)
		return;
	r->images++;
	sw = pd_int(r->d, o, "Width", 0);
	sh = pd_int(r->d, o, "Height", 0);
	if (sw <= 0 || sh <= 0)
		return;
	{
		POBJ *im = pd_get(r->d, o, "ImageMask");

		mask = im && im->t == PO_BOOL && im->a;
	}
	mat_mul6(ctm, r->base, m);
	/* decoded no bigger than the device shows it, nor than it is */
	w = (fxa(m[0]) + fxa(m[2])) >> 16;
	h = (fxa(m[1]) + fxa(m[3])) >> 16;
	if (fxa(m[0]) + fxa(m[3]) < fxa(m[1]) + fxa(m[2])) {
		long t = w;

		w = h;
		h = t;
	}
	if (w > sw) w = sw;
	if (h > sh) h = sh;
	if (w < 1) w = 1;
	if (h < 1) h = 1;
	if (w > 2048) w = 2048;
	if (h > 2048) h = 2048;
	if (cv_turn)
		cv_turn();
	t0 = render_clock ? render_clock() : 0;
	p = cv_image(r->d, obj, (int)w, (int)h, &why);
	if (render_clock)
		r->image_ms += (long)(render_clock() - t0);
	if (cv_turn)
		cv_turn();
	if (!p) {
		r->images_missed++;
		return;
	}
#ifdef RENDER_TRACE
	fprintf(stderr, "image %ld %ldx%ld -> %dx%d px[mid] %04x m %d %d %d %d %d %d\n", obj, sw, sh, p->w, p->h,
		p->px[(long)p->w * p->h / 2], m[0] >> 16, m[1] >> 16, m[2] >> 16, m[3] >> 16, m[4] >> 16, m[5] >> 16);
#endif
	if (mask) {
		unsigned char *a = malloc((size_t)p->w * p->h);
		long i;

		if (a) {
			for (i = 0; i < (long)p->w * p->h; i++) {
				uint16_t c = p->px[i];

				a[i] = (unsigned char)(255 - (((c >> 11) * 8 + ((c >> 5) & 63) * 4 * 2 + (c & 31) * 8) / 4));
			}
			rc_image(&r->c, NULL, a, p->w, p->h, m, 1, g->fill.col, &g->clip);
			free(a);
		}
	} else
		rc_image(&r->c, p->px, NULL, p->w, p->h, m, 0, 0, &g->clip);
	pic_free(p);
}

/* ------------------------------------------------------------------ the page */
int render_init(RPAGE *r, PDOC *d, const char *fontdir)
{
	memset(r, 0, sizeof *r);
	r->d = d;
	r->fontdir = fontdir;
	r->fonts = calloc(1, sizeof *r->fonts);
	if (!r->fonts || FT_Init_FreeType(&r->fonts->lib)) {
		free(r->fonts);
		r->fonts = NULL;
		return -1;
	}
	return 0;
}

void render_done(RPAGE *r)
{
	int i;

	if (r->fonts) {
		gc_flush(r->fonts);
		free(r->fonts->pool);
		for (i = 0; i < 24; i++) {
			if (r->fonts->e[i].face)
				FT_Done_Face(r->fonts->e[i].face);
			free(r->fonts->e[i].buf);
			if (r->fonts->e[i].face && cv_turn)
				cv_turn();
		}
		FT_Done_FreeType(r->fonts->lib);
		free(r->fonts);
	}
	rc_free(&r->c);
	free(r->area);
	memset(r, 0, sizeof *r);
}

int render_page(RPAGE *r, long n, int width)
{
	return render_page_from(r, n, width, 0);
}

int render_page_from(RPAGE *r, long n, int width, int y0)
{
	POBJ *res, *box, *pg;
	long rot;
	fx bx[4] = { 0, 0, FX_INT(612), FX_INT(792) }, s, pw, ph;
	int k, h;
	RS *st;
	PTEXT t;
	PDRAW dr;

	pdf_page_reset(r->d);
	pg = pdf_page(r->d, n, &res, &box, &rot);
	if (!pg)
		return -1;
	if (box && box->t == PO_ARR && box->a >= 4)
		for (k = 0; k < 4; k++)
			bx[k] = to_fx(pa_at(box, k));
	if (bx[2] < bx[0]) { fx q = bx[0]; bx[0] = bx[2]; bx[2] = q; }
	if (bx[3] < bx[1]) { fx q = bx[1]; bx[1] = bx[3]; bx[3] = q; }
	rot = ((rot % 360) + 360) % 360;
	pw = bx[2] - bx[0];
	ph = bx[3] - bx[1];
	if (rot == 90 || rot == 270) {
		fx q = pw;

		pw = ph;
		ph = q;
	}
	if (pw < FX_ONE || ph < FX_ONE)
		return -1;
	s = (fx)(((int64_t)width << 32) / pw);	/* pixels a point (16.16) */
	h = (int)(((int64_t)ph * s) >> 32);
	if (h < 1)
		h = 1;
	if (h > 8192)
		h = 8192;
	r->pw = pw;
	r->ph = ph;
	r->scale = s;
	switch (rot) {
	case 90:  r->base[0] = 0;  r->base[1] = s;  r->base[2] = s;  r->base[3] = 0;
		  r->base[4] = -fxmul(bx[1], s); r->base[5] = -fxmul(bx[0], s); break;
	case 180: r->base[0] = -s; r->base[1] = 0;  r->base[2] = 0;  r->base[3] = s;
		  r->base[4] = fxmul(bx[2], s);  r->base[5] = -fxmul(bx[1], s); break;
	case 270: r->base[0] = 0;  r->base[1] = -s; r->base[2] = -s; r->base[3] = 0;
		  r->base[4] = fxmul(bx[3], s);  r->base[5] = fxmul(bx[2], s); break;
	default:  r->base[0] = s;  r->base[1] = 0;  r->base[2] = 0;  r->base[3] = -s;
		  r->base[4] = -fxmul(bx[0], s); r->base[5] = fxmul(bx[3], s); break;
	}
	if (r->c.w != width || r->c.h != h) {
		/* all of it, or the band canvas_max allows (8 rows at least, a
		   multiple of 8: the dithers' patterns are 8 rows) */
		long rows = h;

		if (r->canvas_max > 0 && (long)width * h * 2 > r->canvas_max) {
			rows = r->canvas_max / ((long)width * 2) & ~7L;
			if (rows < 8)
				rows = 8;
		}
		rc_free(&r->c);
		if (rc_init_rows(&r->c, width, h, (int)rows))
			return -1;
	}
	rc_band(&r->c, y0);
	r->width = width;
	r->narea = 0;
	r->glyphs = r->glyphs_missed = r->images = r->images_missed = 0;
	r->image_ms = 0;
	st = calloc(1, sizeof *st);
	if (!st)
		return -1;
	st->r = r;
	st->gs[0].lw = s;
	st->gs[0].clip.x1 = width;
	st->gs[0].clip.y1 = h;
	rp_init(&st->path);
	dr.u = st;
	dr.op = op_hook;
	dr.glyph = glyph_hook;
	dr.image = image_hook;
	ptext_init(&t);
	t.draw = &dr;
	k = pdf_page_text(r->d, n, &t);
	ptext_free(&t);
	rp_free(&st->path);
	free(st);
	areas_merge(r);
	return k < 0 ? -1 : 0;
}

int render_drawings(const RPAGE *r, RDRAWING *out, int max)
{
	long i;
	int n = 0;

	for (i = 0; i < r->narea && n < max; i++) {
		const RAREA *a = &r->area[i];
		int w = a->box.x1 - a->box.x0, h = a->box.y1 - a->box.y0;

		if (w < 40 || h < 30)
			continue;
		/* a chart, a diagram, a map: curves, or colour beyond a table's
		   rules; a table's lines and boxes alone are not one */
		if (a->curves < 12 && !(a->colours >= 3 && a->paths - a->rules >= 3))
			continue;
		out[n].px = a->box;
		/* back to page points (the canvas's turn undone: no turn here) */
		{
			fx inv = (fx)(((int64_t)1 << 32) / r->scale);
			const fx *b = r->base;
			fx X0 = FX_INT(a->box.x0), X1 = FX_INT(a->box.x1), Y0 = FX_INT(a->box.y0), Y1 = FX_INT(a->box.y1);

			if (b[1] == 0 && b[0] > 0) {
				out[n].x0 = fxmul(X0 - b[4], inv);
				out[n].x1 = fxmul(X1 - b[4], inv);
				out[n].y1 = fxmul(b[5] - Y0, inv);
				out[n].y0 = fxmul(b[5] - Y1, inv);
			} else {
				out[n].x0 = out[n].y0 = 0;
				out[n].x1 = r->pw;
				out[n].y1 = r->ph;
			}
		}
		n++;
	}
	return n;
}

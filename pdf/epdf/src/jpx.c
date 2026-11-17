/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * jpx.c - JPEG 2000 (jpx.h), written from ITU-T T.800 (2002): a JP2
 * file's code-stream or a bare one, decoded only as fine as asked.
 *
 *   the code-stream    SIZ, COD/COC, QCD/QCC, SOT tiles and tile-parts
 *                      (PPM, PPT and POC: JPX_UNSUPPORTED)
 *   tier 2 (B.10)      packets in all five orders (the position-led
 *                      three where each resolution has one precinct),
 *                      tag trees, SOP and EPH markers
 *   tier 1 (D)         the three passes on mq.c, a code-block's
 *                      contexts (styles: none, or a vertically causal
 *                      one; bypass, reset and termall: JPX_UNSUPPORTED)
 *   dequantisation     none (5/3) or scalar, derived and expounded
 *   inverse wavelet    5/3 in integers, 9/7 in fixed point (no FPU: a
 *                      68030's EPDF), levels only up to the resolution
 *                      kept
 *   colour             RCT and ICT, the DC shift, 8 bits
 *
 * Reduced by r, the r finest resolutions' code-blocks are passed over
 * (their packets' headers read, their data not) and the inverse wavelet
 * stops r levels early. Every PDF measured (16 Nov 2026: the corpus's and
 * archive.org's) has one tile, 5 levels, the 9/7, 64 x 64 code-blocks, one
 * layer, LRCP or RLCP, 8 bits, 1 or 3 components.
 */
#include <stdlib.h>
#include <string.h>
#include "mq.h"
#include "jpx.h"

#define FRAC 5			/* the samples' fixed point: 1/32 (a band's values, an
				   8-bit sample's, reach 670: in 16 bits) */

#define IKK97 2707		/* the 9/7's 1 / K^2, 0.660795, in 1/4096 (synth_level) */

/* a product 16 bits by 16: a 68000's one muls.w (GCC makes many of them a
   call to __mulsi3, 32 bits by 32, an int being 32); a 68030's own (its
   shifts and adds beat muls.w there) */
static inline int32_t muls16(int16_t a, int16_t b)
{
#if defined(__GNUC__) && defined(__m68k__) && !defined(__mc68020__)
	int32_t r = a;

	__asm__("muls.w %1,%0" : "+d"(r) : "dmi"(b));
	return r;
#else
	return (int32_t)a * b;
#endif
}
#define MULS(a, k) muls16((int16_t)(a), (int16_t)(k))

/* built for size (the Makefile's -Os), the inner loops for speed */
#if defined(__GNUC__) && !defined(__clang__) && defined(__m68k__)
#define HOT __attribute__((optimize("O2")))
#else
#define HOT
#endif

/* ------------------------------------------------------------------ the code-stream's parameters */

typedef struct {
	int levels, xcb, ycb, style, rev;	/* rev: the 5/3 */
	uint8_t ppx[33], ppy[33];		/* precinct exponents by resolution */
	int qstyle, guard;
	uint16_t q[97];				/* expounded: e << 11 | mu; none: e << 11 */
	int nq;
} TCC;					/* a tile-component's coding */

typedef struct {
	int prec, sgnd, dx, dy;
} CINFO;

typedef struct {
	const uint8_t *d;
	long n;
	/* SIZ */
	int X1, Y1, X0, Y0, TW, TH, TX0, TY0, nc;
	CINFO c[16];
	/* COD */
	int prog, layers, mct, sop, eph;
	TCC tcc[16];
	int ntx, nty;
	int sycc;			/* the JP2's colr: sYCC (enumerated 18) */
	int (*poll)(void);		/* (tier 2's turns, every 32 code-blocks) */
	long turn;
} CS;

static unsigned rd16(const uint8_t *p)
{
	return (unsigned)p[0] << 8 | p[1];
}

static uint32_t rd32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static int ceildiv(int a, int b)
{
	return (int)(((long)a + b - 1) / b);
}

static int ceilpow(int a, int e)
{
	return (int)(((long)a + (1L << e) - 1) >> e);
}

/* the code-stream in a JP2 file (its jp2c box), or the data itself; *pal
   1 where the file maps its samples through a palette (pclr), *sycc where
   its colours are sYCC */
static int codestream(const uint8_t *d, long n, const uint8_t **cs, long *cn, int *pal, int *sycc)
{
	long p = 0;

	if (n >= 4 && d[0] == 0xFF && d[1] == 0x4F) {
		*cs = d;
		*cn = n;
		return 1;
	}
	while (p + 8 <= n) {
		uint32_t len = rd32(d + p), type = rd32(d + p + 4);
		long hl = 8, bl;

		if (len == 1) {
			if (p + 16 > n || rd32(d + p + 8))
				return 0;
			len = rd32(d + p + 12);
			hl = 16;
		}
		bl = len == 0 ? n - p : (long)len;
		if (bl < hl || p + bl > n)
			bl = n - p;
		if (type == 0x6A703268) {	/* 'jp2h': the boxes within */
			long q = p + hl;

			while (q + 8 <= p + bl) {
				uint32_t l2 = rd32(d + q), t2 = rd32(d + q + 4);

				if (l2 < 8 || q + (long)l2 > p + bl)
					break;
				if (t2 == 0x70636C72)	/* 'pclr' */
					*pal = 1;
				else if (t2 == 0x636F6C72 && l2 >= 15 && d[q + 8] == 1 && rd32(d + q + 11) == 18)
					*sycc = 1;	/* 'colr', enumerated sYCC */
				q += l2;
			}
		}
		if (type == 0x6A703263) {	/* 'jp2c' */
			*cs = d + p + hl;
			*cn = bl - hl;
			return *cn >= 4 && (*cs)[0] == 0xFF && (*cs)[1] == 0x4F;
		}
		p += bl;
	}
	return 0;
}

static void cod_spcod(TCC *t, const uint8_t *s, long n, int precincts)
{
	int r;

	if (n < 5)
		return;
	t->levels = s[0];
	t->xcb = (s[1] & 15) + 2;
	t->ycb = (s[2] & 15) + 2;
	t->style = s[3];
	t->rev = s[4] == 1;
	for (r = 0; r <= t->levels && r < 33; r++) {
		if (precincts && 5 + r < n) {
			t->ppx[r] = s[5 + r] & 15;
			t->ppy[r] = s[5 + r] >> 4;
		} else
			t->ppx[r] = t->ppy[r] = 15;
	}
}

static void qcd_parse(TCC *t, const uint8_t *s, long n)
{
	int i;

	if (n < 1)
		return;
	t->qstyle = s[0] & 31;
	t->guard = s[0] >> 5;
	t->nq = 0;
	if (t->qstyle == 0)
		for (i = 1; i < n && t->nq < 97; i++)
			t->q[t->nq++] = (uint16_t)((s[i] >> 3) << 11);
	else
		for (i = 1; i + 1 < n && t->nq < 97; i += 2)
			t->q[t->nq++] = (uint16_t)rd16(s + i);
}

/* a main or tile header's COD/COC/QCD/QCC into tcc (nc components) */
static int header_marker(CS *c, TCC *tcc, unsigned m, const uint8_t *s, long n, int *precincts_main)
{
	int i, ci;

	switch (m) {
	case 0xFF52:		/* COD */
		if (n < 10)
			return JPX_DAMAGED;
		c->sop = s[0] >> 1 & 1;
		c->eph = s[0] >> 2 & 1;
		c->prog = s[1];
		c->layers = (int)rd16(s + 2);
		c->mct = s[4];
		*precincts_main = s[0] & 1;
		for (i = 0; i < c->nc; i++)
			cod_spcod(&tcc[i], s + 5, n - 5, s[0] & 1);
		break;
	case 0xFF53:		/* COC */
		ci = c->nc < 257 ? s[0] : (int)rd16(s);
		s += c->nc < 257 ? 1 : 2;
		n -= c->nc < 257 ? 1 : 2;
		if (ci < c->nc && n >= 6)
			cod_spcod(&tcc[ci], s + 1, n - 1, s[0] & 1);
		break;
	case 0xFF5C:		/* QCD */
		for (i = 0; i < c->nc; i++)
			qcd_parse(&tcc[i], s, n);
		break;
	case 0xFF5D:		/* QCC */
		ci = c->nc < 257 ? s[0] : (int)rd16(s);
		if (ci < c->nc)
			qcd_parse(&tcc[ci], s + (c->nc < 257 ? 1 : 2), n - (c->nc < 257 ? 1 : 2));
		break;
	case 0xFF5F:		/* POC */
	case 0xFF60:		/* PPM */
	case 0xFF61:		/* PPT */
		return JPX_UNSUPPORTED;
	default:
		break;		/* COM, TLM, PLM, PLT, CRG, RGN... */
	}
	return JPX_OK;
}

/* the main header: c filled, *p at the first SOT */
static int main_header(CS *c, long *p)
{
	const uint8_t *d = c->d;
	long q = 2;
	int got_siz = 0, pm = 0;

	while (q + 4 <= c->n) {
		unsigned m = rd16(d + q), L;

		if (m == 0xFF90) {
			*p = q;
			return got_siz ? JPX_OK : JPX_DAMAGED;
		}
		L = rd16(d + q + 2);
		if (L < 2 || q + 2 + L > c->n)
			return JPX_DAMAGED;
		if (m == 0xFF51) {	/* SIZ */
			const uint8_t *s = d + q + 4;
			int i;

			if (L < 38)
				return JPX_DAMAGED;
			c->X1 = (int)rd32(s + 2);
			c->Y1 = (int)rd32(s + 6);
			c->X0 = (int)rd32(s + 10);
			c->Y0 = (int)rd32(s + 14);
			c->TW = (int)rd32(s + 18);
			c->TH = (int)rd32(s + 22);
			c->TX0 = (int)rd32(s + 26);
			c->TY0 = (int)rd32(s + 30);
			c->nc = (int)rd16(s + 34);
			if (c->nc < 1 || c->nc > 16 || L < 38u + 3u * c->nc || c->X1 <= c->X0 || c->Y1 <= c->Y0 || c->TW <= 0 ||
			    c->TH <= 0 || c->X1 - c->X0 > 65535 || c->Y1 - c->Y0 > 65535)
				return c->nc > 16 ? JPX_UNSUPPORTED : JPX_DAMAGED;
			for (i = 0; i < c->nc; i++) {
				c->c[i].prec = (s[36 + 3 * i] & 0x7F) + 1;
				c->c[i].sgnd = s[36 + 3 * i] >> 7;
				c->c[i].dx = s[37 + 3 * i];
				c->c[i].dy = s[38 + 3 * i];
				if (c->c[i].dx < 1 || c->c[i].dy < 1 || c->c[i].prec > 16)
					return JPX_UNSUPPORTED;
			}
			/* the tiles' grid as T.800 has it (offsets from 0 up, the first
			   tile over the picture's corner) and at most 65,535 tiles: their
			   numbers are 16 bits (fuzz_jpx: a product past an int) */
			if (c->X0 < 0 || c->Y0 < 0 || c->TX0 < 0 || c->TY0 < 0 || c->TX0 > c->X0 || c->TY0 > c->Y0 ||
			    (long)c->TX0 + c->TW <= c->X0 || (long)c->TY0 + c->TH <= c->Y0)
				return JPX_DAMAGED;
			c->ntx = ceildiv(c->X1 - c->TX0, c->TW);
			c->nty = ceildiv(c->Y1 - c->TY0, c->TH);
			if (c->ntx < 1 || c->nty < 1 || (long)c->ntx * c->nty > 65535L)
				return JPX_DAMAGED;
			got_siz = 1;
		} else if (got_siz) {
			int r = header_marker(c, c->tcc, m, d + q + 4, (long)L - 2, &pm);

			if (r != JPX_OK)
				return r;
		}
		q += 2 + L;
	}
	return JPX_DAMAGED;
}

/* ------------------------------------------------------------------ tiles: their geometry */

typedef struct TT TT;
struct TT {
	int w, h;
	int *v;				/* value (999: not known) */
	int *low;
	int nl;
	int lw[32], lh[32];
	long off[32];
};

static int tt_new(TT *t, int w, int h)
{
	long tot = 0;
	int l = 0, W = w, H = h;

	memset(t, 0, sizeof *t);
	t->w = w;
	t->h = h;
	for (;;) {
		t->lw[l] = W;
		t->lh[l] = H;
		t->off[l] = tot;
		tot += (long)W * H;
		l++;
		if (W <= 1 && H <= 1)
			break;
		W = (W + 1) / 2;
		H = (H + 1) / 2;
		if (l >= 32)
			return 0;
	}
	t->nl = l;
	t->v = malloc((size_t)(tot ? tot : 1) * sizeof *t->v);
	t->low = calloc((size_t)(tot ? tot : 1), sizeof *t->low);
	if (!t->v || !t->low)
		return 0;
	while (tot--)
		t->v[tot] = 999;
	return 1;
}

static void tt_free(TT *t)
{
	free(t->v);
	free(t->low);
}

typedef struct {
	const uint8_t *p, *end;
	unsigned buf;
	int ct;
} BIO;

static void bio_in(BIO *b)
{
	b->buf = (b->buf << 8) & 0xFFFF;
	b->ct = b->buf == 0xFF00 ? 7 : 8;
	if (b->p < b->end)
		b->buf |= *b->p++;
}

static int bio_bit(BIO *b)
{
	if (b->ct == 0)
		bio_in(b);
	b->ct--;
	return (int)(b->buf >> b->ct) & 1;
}

static unsigned bio_bits(BIO *b, int n)
{
	unsigned v = 0;

	while (n--)
		v = v << 1 | (unsigned)bio_bit(b);
	return v;
}

static void bio_align(BIO *b)
{
	if ((b->buf & 0xFF) == 0xFF)
		bio_in(b);
	b->ct = 0;
}

/* B.10.2: 1 when leaf (x, y)'s value is below thresh */
static int tt_decode(TT *t, BIO *b, int x, int y, int thresh)
{
	int path[32], l, low = 0;

	for (l = 0; l < t->nl; l++)
		path[l] = (int)(t->off[l] + (long)(y >> l) * t->lw[l] + (x >> l));
	for (l = t->nl - 1; l >= 0; l--) {
		int k = path[l];

		if (low > t->low[k])
			t->low[k] = low;
		else
			low = t->low[k];
		while (low < thresh && low < t->v[k]) {
			if (bio_bit(b))
				t->v[k] = low;
			else
				low++;
		}
		t->low[k] = low;
	}
	return t->v[path[0]] < thresh;
}

typedef struct {
	int x0, y0, x1, y1;
	const uint8_t **seg;		/* its data, a piece a layer */
	long *seglen;
	int nseg, aseg;
	int incl, numbps, passes, lblock;
} CB;

typedef struct {
	int cx0, cy0, ncx, ncy;		/* its code-blocks: grid index and count */
	CB *cb;
	TT incl, zbp;
} PB;					/* a precinct's part of a band */

typedef struct {
	int orient;			/* 0 LL, 1 HL, 2 LH, 3 HH */
	int x0, y0, x1, y1;
	int numbps;			/* Mb */
	int e, mu;
} BAND;

typedef struct {
	int x0, y0, x1, y1;
	int nb;
	BAND b[3];
	int pw, ph, ppx, ppy, xcb, ycb;
	PB *pb;				/* pw * ph * nb */
} RES;

typedef struct {
	int x0, y0, x1, y1;		/* the tile-component */
	int nres;			/* resolutions kept (levels + 1 - reduce) */
	RES r[33];
	TCC *t;
} TC;

static void res_free(RES *r)
{
	long i, k;

	if (!r->pb)
		return;
	for (i = 0; i < (long)r->pw * r->ph * r->nb; i++) {
		PB *p = &r->pb[i];

		for (k = 0; k < (long)p->ncx * p->ncy; k++) {
			free(p->cb[k].seg);
			free(p->cb[k].seglen);
		}
		free(p->cb);
		tt_free(&p->incl);
		tt_free(&p->zbp);
	}
	free(r->pb);
	r->pb = NULL;
}

/* resolution r of tc (all its levels' worth: also those passed over,
   whose packets are read to get past them) */
static int res_init(TC *tc, int r, int guard, int prec)
{
	TCC *t = tc->t;
	RES *R = &tc->r[r];
	int NL = t->levels, sh = NL - r, b, px, py;

	memset(R, 0, sizeof *R);
	R->x0 = ceilpow(tc->x0, sh);
	R->y0 = ceilpow(tc->y0, sh);
	R->x1 = ceilpow(tc->x1, sh);
	R->y1 = ceilpow(tc->y1, sh);
	R->ppx = t->ppx[r];
	R->ppy = t->ppy[r];
	R->nb = r == 0 ? 1 : 3;
	R->pw = R->x1 > R->x0 ? ceilpow(R->x1, R->ppx) - (R->x0 >> R->ppx) : 0;
	R->ph = R->y1 > R->y0 ? ceilpow(R->y1, R->ppy) - (R->y0 >> R->ppy) : 0;
	R->xcb = t->xcb < (r ? R->ppx - 1 : R->ppx) ? t->xcb : (r ? R->ppx - 1 : R->ppx);
	R->ycb = t->ycb < (r ? R->ppy - 1 : R->ppy) ? t->ycb : (r ? R->ppy - 1 : R->ppy);
	for (b = 0; b < R->nb; b++) {
		BAND *B = &R->b[b];
		int nb = r == 0 ? NL : NL - r + 1, o = r == 0 ? 0 : b + 1;
		int xob = o == 1 || o == 3, yob = o == 2 || o == 3, qi, gain;

		B->orient = o;
		if (r == 0) {
			B->x0 = ceilpow(tc->x0, nb);
			B->y0 = ceilpow(tc->y0, nb);
			B->x1 = ceilpow(tc->x1, nb);
			B->y1 = ceilpow(tc->y1, nb);
		} else {
			long h = 1L << (nb - 1);

			B->x0 = ceilpow((int)(tc->x0 - h * xob), nb);
			B->y0 = ceilpow((int)(tc->y0 - h * yob), nb);
			B->x1 = ceilpow((int)(tc->x1 - h * xob), nb);
			B->y1 = ceilpow((int)(tc->y1 - h * yob), nb);
		}
		qi = r == 0 ? 0 : 3 * (r - 1) + 1 + b;
		if (t->qstyle == 1) {		/* derived: from the LL's */
			int e0 = t->nq ? t->q[0] >> 11 : 0;

			B->e = e0 - NL + nb;
			if (r == 0)
				B->e = e0;
			B->mu = t->nq ? t->q[0] & 0x7FF : 0;
		} else {
			if (qi >= t->nq)
				return JPX_DAMAGED;
			B->e = t->q[qi] >> 11;
			B->mu = t->q[qi] & 0x7FF;
		}
		gain = o == 0 ? 0 : o == 3 ? 2 : 1;
		B->numbps = guard + B->e - 1;
		(void)prec;
		(void)gain;
	}
	if (R->pw <= 0 || R->ph <= 0)
		return JPX_OK;
	if ((long)R->pw * R->ph > 65536)
		return JPX_UNSUPPORTED;
	R->pb = calloc((size_t)R->pw * R->ph * R->nb, sizeof *R->pb);
	if (!R->pb)
		return JPX_MEMORY;
	for (py = 0; py < R->ph; py++)
		for (px = 0; px < R->pw; px++) {
			/* the precinct in the resolution's coordinates, then each band's */
			long gx = ((long)(R->x0 >> R->ppx) + px) << R->ppx, gy = ((long)(R->y0 >> R->ppy) + py) << R->ppy;

			for (b = 0; b < R->nb; b++) {
				PB *P = &R->pb[((long)py * R->pw + px) * R->nb + b];
				BAND *B = &R->b[b];
				long bx0, by0, bx1, by1;
				int i, k;

				if (r == 0) {
					bx0 = gx;
					by0 = gy;
					bx1 = gx + (1L << R->ppx);
					by1 = gy + (1L << R->ppy);
				} else {
					bx0 = gx >> 1;
					by0 = gy >> 1;
					bx1 = bx0 + (1L << (R->ppx - 1));
					by1 = by0 + (1L << (R->ppy - 1));
				}
				if (bx0 < B->x0)
					bx0 = B->x0;
				if (by0 < B->y0)
					by0 = B->y0;
				if (bx1 > B->x1)
					bx1 = B->x1;
				if (by1 > B->y1)
					by1 = B->y1;
				if (bx1 <= bx0 || by1 <= by0)
					continue;
				P->cx0 = (int)(bx0 >> R->xcb);
				P->cy0 = (int)(by0 >> R->ycb);
				P->ncx = (int)(((bx1 + (1L << R->xcb) - 1) >> R->xcb) - P->cx0);
				P->ncy = (int)(((by1 + (1L << R->ycb) - 1) >> R->ycb) - P->cy0);
				P->cb = calloc((size_t)P->ncx * P->ncy, sizeof *P->cb);
				if (!P->cb || !tt_new(&P->incl, P->ncx, P->ncy) || !tt_new(&P->zbp, P->ncx, P->ncy))
					return JPX_MEMORY;
				for (k = 0; k < P->ncy; k++)
					for (i = 0; i < P->ncx; i++) {
						CB *c = &P->cb[k * P->ncx + i];
						long x0 = ((long)P->cx0 + i) << R->xcb, y0 = ((long)P->cy0 + k) << R->ycb;

						c->x0 = (int)(x0 > bx0 ? x0 : bx0);
						c->y0 = (int)(y0 > by0 ? y0 : by0);
						c->x1 = (int)(x0 + (1L << R->xcb) < bx1 ? x0 + (1L << R->xcb) : bx1);
						c->y1 = (int)(y0 + (1L << R->ycb) < by1 ? y0 + (1L << R->ycb) : by1);
						c->lblock = 3;
					}
			}
		}
	return JPX_OK;
}

/* ------------------------------------------------------------------ tier 2: packets */

static int cb_add(CB *c, const uint8_t *p, long n)
{
	if (c->nseg == c->aseg) {
		int na = c->aseg ? c->aseg * 2 : 2;
		const uint8_t **ns = realloc(c->seg, (size_t)na * sizeof *ns);
		long *nl;

		if (!ns)
			return 0;
		c->seg = ns;
		nl = realloc(c->seglen, (size_t)na * sizeof *nl);
		if (!nl)
			return 0;
		c->seglen = nl;
		c->aseg = na;
	}
	c->seg[c->nseg] = p;
	c->seglen[c->nseg++] = n;
	return 1;
}

static int floorlog2(unsigned v)
{
	int l = 0;

	while (v >>= 1)
		l++;
	return l;
}

typedef struct {
	CB *c;
	int passes;
	long len;
} INC;

/* one packet (layer l of resolution R's precinct k) at *pp: its header read,
   its code-blocks' data noted (where keep), *pp past it */
static int packet(CS *cs, RES *R, long k, int l, const uint8_t **pp, const uint8_t *end, INC *inc)
{
	BIO bio;
	const uint8_t *p = *pp;
	int b, ninc = 0, i;

	if (R->pw <= 0 || R->ph <= 0)
		return JPX_OK;		/* (an empty resolution: no packets) */
	if (cs->sop && p + 6 <= end && p[0] == 0xFF && p[1] == 0x91)
		p += 6;
	bio.p = p;
	bio.end = end;
	bio.buf = 0;
	bio.ct = 0;
	if (!bio_bit(&bio)) {
		bio_align(&bio);
		p = bio.p;
		if (cs->eph && p + 2 <= end && p[0] == 0xFF && p[1] == 0x92)
			p += 2;
		*pp = p;
		return JPX_OK;
	}
	for (b = 0; b < R->nb; b++) {
		PB *P = &R->pb[k * R->nb + b];
		int x, y;

		for (y = 0; y < P->ncy; y++)
			for (x = 0; x < P->ncx; x++) {
				CB *c = &P->cb[y * P->ncx + x];
				int in, np;
				unsigned n;

				if (cs->poll && (++cs->turn & 31) == 0 && !cs->poll())
					return JPX_STOPPED;
				if (!c->incl)
					in = tt_decode(&P->incl, &bio, x, y, l + 1);
				else
					in = bio_bit(&bio);
				if (!in)
					continue;
				if (!c->incl) {
					int z = 0;

					while (!tt_decode(&P->zbp, &bio, x, y, z))
						z++;
					c->numbps = R->b[b].numbps + 1 - z;
					c->incl = 1;
				}
				if (!bio_bit(&bio))
					np = 1;
				else if (!bio_bit(&bio))
					np = 2;
				else if ((n = bio_bits(&bio, 2)) != 3)
					np = 3 + (int)n;
				else if ((n = bio_bits(&bio, 5)) != 31)
					np = 6 + (int)n;
				else
					np = 37 + (int)bio_bits(&bio, 7);
				while (bio_bit(&bio))
					c->lblock++;
				if (c->lblock + floorlog2((unsigned)np) > 31)
					return JPX_DAMAGED;
				inc[ninc].c = c;
				inc[ninc].passes = np;
				inc[ninc].len = (long)bio_bits(&bio, c->lblock + floorlog2((unsigned)np));
				ninc++;
			}
	}
	bio_align(&bio);
	p = bio.p;
	if (cs->eph && p + 2 <= end && p[0] == 0xFF && p[1] == 0x92)
		p += 2;
	for (i = 0; i < ninc; i++) {
		if (p + inc[i].len > end)
			return JPX_DAMAGED;
		if (inc[i].len && !cb_add(inc[i].c, p, inc[i].len))
			return JPX_MEMORY;
		inc[i].c->passes += inc[i].passes;
		p += inc[i].len;
	}
	*pp = p;
	return JPX_OK;
}

/* ------------------------------------------------------------------ tier 1: a code-block */

/* a coefficient's flags (T.800 annex D's state, kept as OpenJPEG keeps
   it): its eight neighbours' significance and the four straight ones'
   signs, set once as each becomes significant, so a context is one look-up
   in a table; then its own state */
#define N_N 0x0001
#define N_S 0x0002
#define N_W 0x0004
#define N_E 0x0008
#define N_NW 0x0010
#define N_NE 0x0020
#define N_SW 0x0040
#define N_SE 0x0080
#define S_N 0x0100			/* that neighbour negative */
#define S_S 0x0200
#define S_W 0x0400
#define S_E 0x0800
#define F_SIG 0x1000
#define F_VIS 0x2000			/* coded in this bitplane's significance pass */
#define F_REF 0x4000			/* refined before */
#define F_NEG 0x8000
#define N_ALL 0x00FF
#define N_BELOW (N_S | N_SW | N_SE)	/* (masked in a stripe's last row when causal) */

enum { CX_ZC = 0, CX_SC = 9, CX_MR = 14, CX_RL = 17, CX_UNI = 18 };

/* the zero coding's context by the neighbours (N_ALL bits), for the LL
   and LH bands, the HL (horizontal and vertical the other way round) and
   the HH (T.800 table D.1); the sign's context and its turn by the
   straight four (table D.3: N_N..N_E | S_N..S_E >> 4) */
static uint8_t zc_lut[3][256], sc_lut[256];

static void t1_tables(void)
{
	int k, o;

	if (zc_lut[0][N_W])
		return;
	for (o = 0; o < 3; o++)
		for (k = 0; k < 256; k++) {
			int h = !!(k & N_W) + !!(k & N_E), v = !!(k & N_N) + !!(k & N_S);
			int d = !!(k & N_NW) + !!(k & N_NE) + !!(k & N_SW) + !!(k & N_SE), c;

			if (o == 2) {
				int hv = h + v;

				c = d >= 3 ? 8 : d == 2 ? (hv >= 1 ? 7 : 6) : d == 1 ? (hv >= 2 ? 5 : hv == 1 ? 4 : 3) : (hv >= 2 ? 2 : hv);
			} else {
				if (o == 1) {
					int t = h;

					h = v;
					v = t;
				}
				c = h == 2 ? 8 : h == 1 ? (v >= 1 ? 7 : d >= 1 ? 6 : 5) : v == 2 ? 4 : v == 1 ? 3 : d >= 2 ? 2 : d;
			}
			zc_lut[o][k] = (uint8_t)c;
		}
	for (k = 0; k < 256; k++) {
		/* a neighbour's part: +1 significant and positive, -1 negative */
#define PART(sig, neg) ((k & (sig)) ? ((k & (neg)) ? -1 : 1) : 0)
		int H = PART(N_W, S_W >> 4) + PART(N_E, S_E >> 4), V = PART(N_N, S_N >> 4) + PART(N_S, S_S >> 4), c, x;
#undef PART

		H = H > 1 ? 1 : H < -1 ? -1 : H;
		V = V > 1 ? 1 : V < -1 ? -1 : V;
		if (H == 0) {
			x = V < 0;
			c = V == 0 ? 0 : 1;
		} else {
			x = H < 0;
			if (H < 0)
				V = -V;
			c = V == 1 ? 4 : V == 0 ? 3 : 2;
		}
		sc_lut[k] = (uint8_t)((CX_SC + c) | x << 7);
	}
}

typedef struct {
	int w, h, stride;		/* stride: the flags' row, w + 2 */
	int32_t *v;			/* magnitude, twice (half a step's room) */
	uint16_t *f;			/* (w + 2) x (h + 2), a border round */
	const uint8_t *zc;		/* the band's zc_lut */
	int causal;
	uint8_t cx[19];
	MQ m;
	int (*poll)(void);		/* asked every stripe: 0 stops */
} T1;

/* the coefficient at f (its value at v) significant from bitplane bp,
   its sign decoded: its neighbours told */
#define SIGNIFICANT(t, fp, vp, bp, ctxmask)                                                                 \
	do {                                                                                                \
		unsigned sl_ = sc_lut[((fp)[0] & (ctxmask) & 0x0F) | (((fp)[0] & (ctxmask)) >> 4 & 0xF0)];   \
		int s_, st_ = st;                                                                           \
		MQ_DECODE(&(t)->m, c, a, ct, &cx[sl_ & 0x7F], s_);                                          \
		s_ ^= sl_ >> 7;                                                                             \
		*(vp) = (int32_t)(3L << (bp));	/* (1.5 steps of the bitplane, twice) */                     \
		(fp)[0] |= (uint16_t)(F_SIG | (s_ ? F_NEG : 0));                                            \
		(fp)[-st_ - 1] |= N_SE;                                                                     \
		(fp)[-st_ + 1] |= N_SW;                                                                     \
		(fp)[st_ - 1] |= N_NE;                                                                      \
		(fp)[st_ + 1] |= N_NW;                                                                      \
		(fp)[-st_] |= (uint16_t)(N_S | (s_ ? S_S : 0));                                             \
		(fp)[st_] |= (uint16_t)(N_N | (s_ ? S_N : 0));                                              \
		(fp)[-1] |= (uint16_t)(N_E | (s_ ? S_E : 0));                                               \
		(fp)[1] |= (uint16_t)(N_W | (s_ ? S_W : 0));                                                \
	} while (0)

/* (the mask for row y of a stripe: its neighbours below hidden in the
   vertically causal mode's last row) */
#define ROWMASK(t, k) (causal && (k) == 3 ? (unsigned)~(N_BELOW | S_S) : 0xFFFFu)

HOT static int pass_sig(T1 *t, int bp)
{
	int y0, x, k, st = t->stride, w = t->w, h = t->h, causal = t->causal;
	uint8_t *cx = t->cx;
	const uint8_t *zc = t->zc;
	uint16_t *fb = t->f;
	int32_t *vb = t->v;
	uint32_t c, a;
	int ct;

	MQ_LOAD(&t->m, c, a, ct);
	for (y0 = 0; y0 < h; y0 += 4) {
		int rows = h - y0 < 4 ? h - y0 : 4;

		if (t->poll && !t->poll()) {
			MQ_SAVE(&t->m, c, a, ct);
			return 0;
		}
		uint16_t *f0 = fb + (long)(y0 + 1) * st + 1;
		int32_t *v0 = vb + (long)y0 * w;

		for (x = 0; x < w; x++) {
			uint16_t *fp = f0 + x;
			int32_t *vp = v0 + x;

			/* (a column of four none of whose neighbours is significant:
			   nothing here) */
			if (rows == 4 && !((fp[0] | fp[st] | fp[2 * st] | fp[3 * st]) & N_ALL))
				continue;
			for (k = 0; k < rows; k++, fp += st, vp += w) {
				unsigned m = ROWMASK(t, k), f = *fp & m;
				int d;

				if ((f & F_SIG) || !(f & N_ALL))
					continue;
				*fp |= F_VIS;
				MQ_DECODE(&t->m, c, a, ct, &cx[CX_ZC + zc[f & N_ALL]], d);
				if (d)
					SIGNIFICANT(t, fp, vp, bp, m);
			}
		}
	}
	MQ_SAVE(&t->m, c, a, ct);
	return 1;
}

HOT static int pass_ref(T1 *t, int bp)
{
	int y0, x, k, st = t->stride, w = t->w, h = t->h, causal = t->causal;
	uint8_t *cx = t->cx;
	uint16_t *fb = t->f;
	int32_t *vb = t->v;
	int32_t one = (int32_t)(1L << bp);
	uint32_t c, a;
	int ct;

	MQ_LOAD(&t->m, c, a, ct);
	for (y0 = 0; y0 < h; y0 += 4) {
		int rows = h - y0 < 4 ? h - y0 : 4;

		if (t->poll && !t->poll()) {
			MQ_SAVE(&t->m, c, a, ct);
			return 0;
		}
		uint16_t *f0 = fb + (long)(y0 + 1) * st + 1;
		int32_t *v0 = vb + (long)y0 * w;

		for (x = 0; x < w; x++) {
			uint16_t *fp = f0 + x;
			int32_t *vp = v0 + x;

			if (rows == 4 && !((fp[0] | fp[st] | fp[2 * st] | fp[3 * st]) & F_SIG))
				continue;	/* (none significant: none refined) */
			for (k = 0; k < rows; k++, fp += st, vp += w) {
				unsigned f = *fp;
				int d, ctx;

				if ((f & (F_SIG | F_VIS)) != F_SIG)
					continue;
				ctx = f & F_REF ? CX_MR + 2 : CX_MR + ((f & ROWMASK(t, k) & N_ALL) ? 1 : 0);
				MQ_DECODE(&t->m, c, a, ct, &cx[ctx], d);
				*vp += d ? one : -one;
				*fp = (uint16_t)(f | F_REF);
			}
		}
	}
	MQ_SAVE(&t->m, c, a, ct);
	return 1;
}

/* the cleanup: every coefficient the significance pass didn't code (its
   mark cleared as it goes), four at once by the run-length mode where a
   stripe's column is quiet */
HOT static int pass_clean(T1 *t, int bp)
{
	int y0, x, k, st = t->stride, w = t->w, h = t->h, causal = t->causal;
	uint8_t *cx = t->cx;
	const uint8_t *zc = t->zc;
	uint16_t *fb = t->f;
	int32_t *vb = t->v;
	uint32_t c, a;
	int ct;

	MQ_LOAD(&t->m, c, a, ct);
	for (y0 = 0; y0 < h; y0 += 4) {
		int rows = h - y0 < 4 ? h - y0 : 4;

		if (t->poll && !t->poll()) {
			MQ_SAVE(&t->m, c, a, ct);
			return 0;
		}
		uint16_t *f0 = fb + (long)(y0 + 1) * st + 1;
		int32_t *v0 = vb + (long)y0 * w;

		for (x = 0; x < w; x++) {
			uint16_t *fp = f0 + x;
			int32_t *vp = v0 + x;

			k = 0;
			if (rows == 4 && !((fp[0] | fp[st] | fp[2 * st] | (fp[3 * st] & ROWMASK(t, 3))) & (F_SIG | F_VIS | N_ALL))) {
				int d;

				MQ_DECODE(&t->m, c, a, ct, &cx[CX_RL], d);
				if (!d)
					continue;
				MQ_DECODE(&t->m, c, a, ct, &cx[CX_UNI], d);
				k = d << 1;
				MQ_DECODE(&t->m, c, a, ct, &cx[CX_UNI], d);
				k |= d;
				for (d = 0; d < k; d++) {	/* (no multiply: a 68000's is a call) */
					fp += st;
					vp += w;
				}
				SIGNIFICANT(t, fp, vp, bp, ROWMASK(t, k));
				k++;
				fp += st;
				vp += w;
			}
			for (; k < rows; k++, fp += st, vp += w) {
				unsigned m = ROWMASK(t, k), f = *fp;
				int d;

				if (f & (F_SIG | F_VIS)) {
					*fp = (uint16_t)(f & ~F_VIS);
					continue;
				}
				MQ_DECODE(&t->m, c, a, ct, &cx[CX_ZC + zc[f & m & N_ALL]], d);
				if (d)
					SIGNIFICANT(t, fp, vp, bp, m);
			}
		}
	}
	MQ_SAVE(&t->m, c, a, ct);
	return 1;
}

/* code-block c of band B decoded, its coefficients to their samples'
   places (out its first, xs apart, its rows os apart): dequantised, FRAC
   fixed point; fold: an HH band's 1/K^2 in it (synth_level). poll every
   stripe of a pass (a dense block's pass is 75 ms on Falcon-4) */
static int t1_block(CB *c, const BAND *B, int style, int rev, int fold, int prec, int16_t *out, long xs, long os, int32_t *v,
		    uint16_t *f, int (*poll)(void))
{
	T1 t;
	int w = c->x1 - c->x0, h = c->y1 - c->y0, x, y, pass, bp, type;
	long n = 0, i;
	uint8_t *buf = NULL;
	const uint8_t *data;
	int gain = B->orient == 0 ? 0 : B->orient == 3 ? 2 : 1;

	if (w <= 0 || h <= 0)
		return JPX_OK;
	if (style & 0x27)		/* bypass, reset, termall, segmentation symbols */
		return JPX_UNSUPPORTED;
	for (i = 0; i < c->nseg; i++)
		n += c->seglen[i];
	if (!c->passes || n <= 0)
		return JPX_OK;		/* (the buffer's zeros) */
	if (c->nseg == 1)
		data = c->seg[0];
	else {
		buf = malloc((size_t)n + 2);
		if (!buf)
			return JPX_MEMORY;
		for (i = 0, n = 0; i < c->nseg; i++) {
			memcpy(buf + n, c->seg[i], (size_t)c->seglen[i]);
			n += c->seglen[i];
		}
		data = buf;
	}
	t1_tables();
	t.w = w;
	t.h = h;
	t.stride = w + 2;
	t.v = v;
	t.f = f;
	t.zc = zc_lut[B->orient == 3 ? 2 : B->orient == 1 ? 1 : 0];
	t.causal = style >> 3 & 1;
	memset(v, 0, (size_t)w * h * sizeof *v);
	memset(f, 0, (size_t)(w + 2) * (h + 2) * sizeof *f);
	memset(t.cx, 0, sizeof t.cx);
	t.cx[CX_UNI] = 46 << 1;
	t.cx[CX_RL] = 3 << 1;
	t.cx[CX_ZC] = 4 << 1;
	mq_init(&t.m, data, n);
	bp = c->numbps - 1;
	type = 2;
	t.poll = poll;
	for (pass = 0; pass < c->passes && bp >= 0; pass++) {
		int go = type == 0 ? pass_sig(&t, bp) : type == 1 ? pass_ref(&t, bp) : pass_clean(&t, bp);

		if (!go) {
			free(buf);
			return JPX_STOPPED;
		}
		if (type == 2) {
			type = 0;
			bp--;
		} else
			type++;
	}
	free(buf);
	/* out: the value (twice its magnitude, the sign) to the samples' fixed
	   point, every component's brought to 8 bits; the 9/7's step
	   2^(Rb - e) (1 + mu / 2^11), Rb the bits (8 here) and the band's gain */
	{
		int sh = 8 + gain - B->e - 12 + FRAC, sc = FRAC - (prec - 8), mult = 2048 + B->mu;

		if (fold && B->orient == 3)
			mult = (mult * IKK97 + 2048) >> 12;
		for (y = 0; y < h; y++) {
			const int32_t *vp = v + (long)y * w;
			const uint16_t *fp = f + (long)(y + 1) * t.stride + 1;
			int16_t *op = out + (long)y * os;

			if ((y & 15) == 15 && poll && !poll())
				return JPX_STOPPED;

			for (x = 0; x < w; x++, op += xs) {
				int32_t m = vp[x];
				long q;

				if (!m)
					continue;	/* (the buffer's zeros) */
				if (rev) {
					q = m >> 1;
					q = sc >= 0 ? q << sc : (q + (1L << (-sc - 1))) >> -sc;
				} else {
					/* (16 bits by 16 where the value fits: a 68000's muls.w) */
					q = m < 32768 ? MULS(m, mult) : (long)m * mult;
					q = sh >= 0 ? q << sh : (q + (1L << (-sh - 1))) >> -sh;
				}
				if (q > 32767)
					q = 32767;
				*op = (int16_t)(fp[x] & F_NEG ? -q : q);
			}
		}
	}
	return JPX_OK;
}

/* ------------------------------------------------------------------ the inverse wavelet */

/* lifting constants, 1/4096; each step's product 16 bits by 16 (a
   68000's one muls.w: the sum of two neighbours halved first, a 1/64th
   of a sample given up) */
#define A97 (-6497)		/* alpha -1.586134342 */
#define B97 (-217)		/* beta -0.052980119 */
#define G97 3616		/* gamma 0.882911076 */
#define D97 1817		/* delta 0.443506852 */
#define K97 5039		/* K 1.230174105 */
#define IK97 3330		/* 1 / K 0.812893066 */
#define KK97 6198		/* K^2 1.513328 */
#define LIFT(s, k) ((MULS((s) >> 1, (k)) + 1024) >> 11)
#define SCALE(a, k) ((MULS((a), (k)) + 2048) >> 12)

/*
 * The synthesis in place (T.800 F.3, lifting): every coefficient decoded
 * straight to its sample's place in the kept resolution's buffer, a level
 * j resolution's samples 2^(L - j) apart, so a level's lines are the
 * buffer's rows and columns at that step and the vertical steps go row
 * against row: nothing copied. The 9/7's scalings (K on the low samples,
 * 1/K on the high, each way) are taken out: the HH bands' 1/K^2 in their
 * dequantisation, the HL and LH bands' K / K none, and K^2 on the low
 * band's samples as a level begins (a level a sample wide or high scales
 * as it goes instead: its HH band's dequantised plain).
 */
enum { L_D, L_G, L_B, L_A, L_53LO, L_53HI, L_KLO, L_KHI };

/* one lifting step on a line's samples of one parity: p the line's first,
   s apart, n of them, the first of the step's at i0 (0 or 1) */
HOT static void lift(int16_t *p, long s, int n, int i0, int kind)
{
	int i = i0;
	int16_t *q = p + i0 * s;
	const int32_t frac = ~((1 << FRAC) - 1);

#define ONE(l, r)                                                                       \
	do {                                                                            \
		int32_t sum_ = (int32_t)(l) + (r);                                      \
		switch (kind) {                                                         \
		case L_D: *q = (int16_t)(*q - LIFT(sum_, D97)); break;                   \
		case L_G: *q = (int16_t)(*q - LIFT(sum_, G97)); break;                   \
		case L_B: *q = (int16_t)(*q - LIFT(sum_, B97)); break;                   \
		case L_A: *q = (int16_t)(*q - LIFT(sum_, A97)); break;                   \
		case L_53LO: *q = (int16_t)(*q - ((sum_ + 2 * (1 << FRAC)) >> 2 & frac)); break; \
		default: *q = (int16_t)(*q + (sum_ >> 1 & frac)); break;                \
		}                                                                       \
	} while (0)
	if (i == 0) {			/* (x[-1] = x[1]) */
		ONE(q[s], q[s]);
		i += 2;
		q += 2 * s;
	}
	/* the inner ones: a loop each, the step's kind fixed in it */
#define RUN(expr)                                               \
	for (; i + 1 < n; i += 2, q += 2 * s) {                 \
		int32_t sum = (int32_t)q[-s] + q[s];            \
		*q = (int16_t)(expr);                           \
	}
	switch (kind) {
	case L_D: RUN(*q - LIFT(sum, D97)); break;
	case L_G: RUN(*q - LIFT(sum, G97)); break;
	case L_B: RUN(*q - LIFT(sum, B97)); break;
	case L_A: RUN(*q - LIFT(sum, A97)); break;
	case L_53LO: RUN(*q - ((sum + 2 * (1 << FRAC)) >> 2 & frac)); break;
	default: RUN(*q + (sum >> 1 & frac)); break;
	}
#undef RUN
	if (i < n)			/* (x[n] = x[n - 2]) */
		ONE(q[-s], q[-s]);
#undef ONE
}

/* the same step down a level's columns: row r's samples (w of them, s
   apart) from the rows above and below it (rs apart), n rows; 0 when
   poll (asked every 4 rows) says stop */
HOT static int lift_rows(int16_t *p, long s, int w, long rs, int n, int i0, int kind, int (*poll)(void))
{
	int i, j;
	const int32_t frac = ~((1 << FRAC) - 1);

	for (i = i0; i < n; i += 2) {
		if ((i & 6) == 6 && poll && !poll())
			return 0;
		int16_t *q = p + i * rs;
		const int16_t *u = i > 0 ? q - rs : q + rs, *d = i + 1 < n ? q + rs : q - rs;

#define RUN(expr)                                                       \
	for (j = 0; j < w; j++, q += s, u += s, d += s) {               \
		int32_t sum = (int32_t)*u + *d;                         \
		*q = (int16_t)(expr);                                   \
	}
		switch (kind) {
		case L_D: RUN(*q - LIFT(sum, D97)); break;
		case L_G: RUN(*q - LIFT(sum, G97)); break;
		case L_B: RUN(*q - LIFT(sum, B97)); break;
		case L_A: RUN(*q - LIFT(sum, A97)); break;
		case L_53LO: RUN(*q - ((sum + 2 * (1 << FRAC)) >> 2 & frac)); break;
		default: RUN(*q + (sum >> 1 & frac)); break;
		}
#undef RUN
	}
	return 1;
}

/* samples of one parity (n of them from i0, s apart) times k / 4096 */
HOT static void scale(int16_t *p, long s, int n, int i0, int k)
{
	int16_t *q = p + i0 * s;
	int i;

	for (i = i0; i < n; i += 2, q += 2 * s)
		*q = (int16_t)SCALE(*q, k);
}

/* a level synthesised in a (rows W apart): its w x h samples s apart from
   (ox, oy); lo_x, lo_y its low samples' parity (its first sample's place's:
   x0 & 1). fold: the 9/7's scalings taken out (see above), else made here */
static void synth_level(int16_t *a, long W, long s, int ox, int oy, int w, int h, int lo_x, int lo_y, int rev, int fold,
			int (*poll)(void), int *stop)
{
	int16_t *p = a + (long)oy * W + ox;
	long rs = W * s;
	int k;

	if (fold) {
		/* the low band's samples times K^2 */
		for (k = lo_y; k < h; k += 2) {
			scale(p + k * rs, s, w, lo_x, KK97);
			if (((k - lo_y) & 14) == 14 && poll && !poll()) {	/* (every 8 rows of these) */
				*stop = 1;
				return;
			}
		}
	}
	/* the rows */
	for (k = 0; k < h; k++) {
		int16_t *q = p + k * rs;

		if (w == 1) {
			if (lo_x)
				*q = (int16_t)(*q / 2);	/* (a lone high sample: T.800 F.3.7) */
		} else if (rev) {
			lift(q, s, w, lo_x, L_53LO);
			lift(q, s, w, 1 - lo_x, L_53HI);
		} else {
			if (!fold) {
				scale(q, s, w, lo_x, K97);
				scale(q, s, w, 1 - lo_x, IK97);
			}
			lift(q, s, w, lo_x, L_D);
			lift(q, s, w, 1 - lo_x, L_G);
			lift(q, s, w, lo_x, L_B);
			lift(q, s, w, 1 - lo_x, L_A);
		}
		if ((k & 3) == 3 && poll && !poll()) {
			*stop = 1;
			return;
		}
	}
	/* the columns, row against row */
	if (h == 1) {
		if (lo_y)
			for (k = 0; k < w; k++)
				p[k * s] = (int16_t)(p[k * s] / 2);
	} else if (rev) {
		if (!lift_rows(p, s, w, rs, h, lo_y, L_53LO, poll) || !lift_rows(p, s, w, rs, h, 1 - lo_y, L_53HI, poll)) {
			*stop = 1;
			return;
		}
	} else {
		if (!fold)
			for (k = 0; k < h; k++) {
				int16_t *q = p + k * rs;
				int j, f = (k & 1) == lo_y ? K97 : IK97;

				for (j = 0; j < w; j++, q += s)
					*q = (int16_t)SCALE(*q, f);
			}
		if (!lift_rows(p, s, w, rs, h, lo_y, L_D, poll) || !lift_rows(p, s, w, rs, h, 1 - lo_y, L_G, poll) ||
		    !lift_rows(p, s, w, rs, h, lo_y, L_B, poll) || !lift_rows(p, s, w, rs, h, 1 - lo_y, L_A, poll)) {
			*stop = 1;
			return;
		}
	}
	if (poll && !poll())
		*stop = 1;
}

/* ------------------------------------------------------------------ a tile */

typedef struct {
	CS *cs;
	int reduce;
	uint8_t *plane[3];		/* each component at the reduced size, 8 bits
					   (the colour transform's two: 128 their 0) */
	int ow, oh;			/* the reduced picture's size */
	int outc, ycc;			/* components shown; YCbCr's (mct or sYCC) */
	int direct;			/* one tile: component 0's rows straight out */
	JPX_ROW row;
	void *u;
	uint8_t *line;
	int (*poll)(void);
	long done, of;			/* progress: code-blocks (jpx_progress) */
} DEC;

void (*jpx_progress)(long done, long of);

static void progress(DEC *dc, long n)
{
	dc->done += n;
	if (jpx_progress)
		jpx_progress(dc->done, dc->of > dc->done ? dc->of : dc->done);
}

static uint8_t clamp8(long v)
{
	return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
}

/* row y's pixels from component 0's values s (8 bits, FRAC fixed point,
   no DC shift) and the planes' others */
#define CL8(v) ((v) < 0 ? 0 : (v) > 255 ? 255 : (v))
#define RND (1 << (FRAC - 1))

HOT static int emit(DEC *dc, long y, const int16_t *s, int sgnd)
{
	int x, w = dc->ow;
	long half = (sgnd && !dc->ycc ? 0 : 128L) << FRAC;
	uint8_t *o = dc->line;
	const uint8_t *p1, *p2;

	/* (each kind its own loop: nothing of dc read again in it, a byte
	   stored may be anything to the compiler) */
	if (dc->outc == 1) {
		for (x = 0; x < w; x++) {
			long v = ((long)s[x] + half + RND) >> FRAC;

			o[x] = (uint8_t)CL8(v);
		}
		dc->row(dc->u, y, o);
		return (y & 3) != 3 || !dc->poll || dc->poll();
	}
	p1 = dc->plane[1] + y * w;
	p2 = dc->plane[2] + y * w;
	if (!dc->ycc)
		for (x = 0; x < w; x++, o += 3) {
			long R = ((long)s[x] + half + RND) >> FRAC;

			o[0] = (uint8_t)CL8(R);
			o[1] = p1[x];
			o[2] = p2[x];
		}
	else if (dc->cs->mct && dc->cs->tcc[0].rev)
		for (x = 0; x < w; x++, o += 3) {
			/* the RCT: G = Y - floor((U + V) / 4), R = V + G, B = U + G */
			int Y = (int)(((long)s[x] + RND) >> FRAC), U = p1[x] - 128, V = p2[x] - 128;
			int G = Y - ((U + V) >> 2) + 128, R = V + G, B = U + G;

			o[0] = (uint8_t)CL8(R);
			o[1] = (uint8_t)CL8(G);
			o[2] = (uint8_t)CL8(B);
		}
	else
		for (x = 0; x < w; x++, o += 3) {
			/* the ICT: R = Y + 1.402 V, G = Y - 0.34413 U - 0.71414 V,
			   B = Y + 1.772 U; Y to 1/32 */
			long Y = (long)s[x] + half + RND;
			int U = p1[x] - 128, V = p2[x] - 128;
			long R = (Y + (MULS(V, 5743) >> (12 - FRAC))) >> FRAC;
			long G = (Y - ((MULS(U, 1410) + MULS(V, 2925)) >> (12 - FRAC))) >> FRAC;
			long B = (Y + (MULS(U, 7258) >> (12 - FRAC))) >> FRAC;

			o[0] = (uint8_t)CL8(R);
			o[1] = (uint8_t)CL8(G);
			o[2] = (uint8_t)CL8(B);
		}
	dc->row(dc->u, y, dc->line);
	return (y & 3) != 3 || !dc->poll || dc->poll();	/* (a turn every 4 rows) */
}

/* the tile's tile-component c: its resolutions kept decoded into a buffer
   of its own, then synthesised and its samples (DC shifted, 8 bits) into
   the component's plane */
static int tile_comp(DEC *dc, TC *tc, int ci, int keep)
{
	CS *cs = dc->cs;
	RES *top = &tc->r[keep - 1];
	int W = top->x1 - top->x0, H = top->y1 - top->y0, r, b, x, y;
	int16_t *a;
	int32_t *cv = NULL;
	uint16_t *cf = NULL;
	int rc = JPX_OK, prec = cs->c[ci].prec, L = keep - 1, rev = tc->t->rev, stop = 0;

	if (W <= 0 || H <= 0)
		return JPX_OK;
	a = malloc((size_t)W * H * sizeof *a);
	if (a) {
		/* (cleared in pieces with turns: 2 MB whole is 120 ms on Falcon-4) */
		long done, n = (long)W * H;

		for (done = 0; done < n && rc == JPX_OK; done += 32768L) {
			memset(a + done, 0, (size_t)(n - done < 32768L ? n - done : 32768L) * sizeof *a);
			if (dc->poll && !dc->poll())
				rc = JPX_STOPPED;
		}
	}
	{
		int mw = 1 << tc->t->xcb, mh = 1 << tc->t->ycb;

		cv = malloc((size_t)mw * mh * sizeof *cv);
		cf = malloc((size_t)(mw + 2) * (mh + 2) * sizeof *cf);
	}
	if (!a || !cv || !cf) {
		rc = JPX_MEMORY;
		goto out;
	}
	if (rc != JPX_OK)
		goto out;
	/* each code-block's coefficients to their samples' places: resolution
	   r's 2^(L - r) apart, a band's every other one of them */
	for (r = 0; r < keep && rc == JPX_OK; r++) {
		RES *R = &tc->r[r];
		long k, sr = 1L << (L - r);
		int fold = !rev && R->x1 - R->x0 > 1 && R->y1 - R->y0 > 1;

		for (k = 0; k < (long)R->pw * R->ph && rc == JPX_OK; k++)
			for (b = 0; b < R->nb && rc == JPX_OK; b++) {
				PB *P = &R->pb[k * R->nb + b];
				BAND *B = &R->b[b];
				long i, xs = r ? 2 * sr : sr;
				int xo = B->orient & 1, yo = B->orient >> 1 & 1;

				for (i = 0; i < (long)P->ncx * P->ncy && rc == JPX_OK; i++) {
					CB *c = &P->cb[i];
					long px = (r ? sr * (2L * c->x0 + xo) : sr * c->x0) - top->x0;
					long py = (r ? sr * (2L * c->y0 + yo) : sr * c->y0) - top->y0;

					rc = t1_block(c, B, tc->t->style, rev, fold, prec, a + py * W + px, xs, xs * W, cv, cf, dc->poll);
					progress(dc, 1);
				}
			}
	}
	/* the synthesis, level by level, in place */
	for (r = 1; r < keep && rc == JPX_OK; r++) {
		RES *R = &tc->r[r];
		long sr = 1L << (L - r);
		int w = R->x1 - R->x0, h = R->y1 - R->y0;

		if (w <= 0 || h <= 0)
			continue;
		synth_level(a, W, sr, (int)(sr * R->x0 - top->x0), (int)(sr * R->y0 - top->y0), w, h, R->x0 & 1, R->y0 & 1, rev,
			    !rev && w > 1 && h > 1, dc->poll, &stop);
		if (stop)
			rc = JPX_STOPPED;
	}
	if (rc != JPX_OK)
		goto out;
	if (dc->direct && ci == 0) {
		/* one tile, the picture's whole: its rows out (8 bits: the
		   precision shift done in emit, the colour transform there too) */
		if (W != dc->ow || H != dc->oh)
			rc = JPX_DAMAGED;
		for (y = 0; y < H && rc == JPX_OK; y++)
			if (!emit(dc, y, a + (long)y * W, cs->c[0].sgnd))
				rc = JPX_STOPPED;
		goto out;
	}
	/* into the component's plane at the tile's place: 8 bits, DC shifted
	   (the colour transform's chroma 128 their 0) */
	{
		int ox = top->x0 - ceilpow(cs->X0, dc->reduce);
		int oy = top->y0 - ceilpow(cs->Y0, dc->reduce);
		int dcs = cs->c[ci].sgnd && !dc->ycc ? 0 : 128, pw = dc->ow;
		int x0 = ox < 0 ? -ox : 0, x1 = ox + W > pw ? pw - ox : W;
		const int16_t *s = a;

		for (y = 0; y < H; y++, s += W) {
			uint8_t *o;

			if ((y & 15) == 15 && dc->poll && !dc->poll()) {
				rc = JPX_STOPPED;
				break;
			}
			if (oy + y < 0 || oy + y >= dc->oh)
				continue;
			o = dc->plane[ci] + (long)(oy + y) * pw + ox;
			for (x = x0; x < x1; x++)
				o[x] = clamp8((((long)s[x] + (1 << (FRAC - 1))) >> FRAC) + dcs);
		}
	}
out:
	free(a);
	free(cv);
	free(cf);
	return rc;
}

/* tile t: its tile-parts' bodies (concatenated where more than one) */
static int tile(DEC *dc, int t, const uint8_t *body, long blen, TCC *tcc)
{
	CS *cs = dc->cs;
	TC *tcs = calloc((size_t)cs->nc, sizeof *tcs);	/* (5 KB each: never on a picture's stack) */
	int ci, r, l, rc = JPX_OK, maxres = 0, maxkeep = 0, need, keep[16];
	int tx = t % cs->ntx, ty = t / cs->ntx;
	int tx0 = cs->TX0 + tx * cs->TW, ty0 = cs->TY0 + ty * cs->TH;
	int tx1 = tx0 + cs->TW, ty1 = ty0 + cs->TH;
	const uint8_t *p = body, *end = body + blen;
	INC *inc = NULL;
	long most = 0;

	if (tx0 < cs->X0)
		tx0 = cs->X0;
	if (ty0 < cs->Y0)
		ty0 = cs->Y0;
	if (tx1 > cs->X1)
		tx1 = cs->X1;
	if (ty1 > cs->Y1)
		ty1 = cs->Y1;
	if (!tcs)
		return JPX_MEMORY;
	for (ci = 0; ci < cs->nc && rc == JPX_OK; ci++) {
		TC *tc = &tcs[ci];

		tc->t = &tcc[ci];
		tc->x0 = ceildiv(tx0, cs->c[ci].dx);
		tc->y0 = ceildiv(ty0, cs->c[ci].dy);
		tc->x1 = ceildiv(tx1, cs->c[ci].dx);
		tc->y1 = ceildiv(ty1, cs->c[ci].dy);
		if (tc->t->levels > 32 || tc->t->xcb + tc->t->ycb > 12) {
			rc = JPX_DAMAGED;
			break;
		}
		tc->nres = tc->t->levels + 1;
		keep[ci] = tc->nres - dc->reduce;
		if (keep[ci] < 1)
			keep[ci] = 1;
		if (tc->nres > maxres)
			maxres = tc->nres;
		if (keep[ci] > maxkeep)
			maxkeep = keep[ci];
	}
	/* the resolutions whose packets are read: where the finer ones all come
	   after the kept ones' (resolution first, RLCP and RPCL, or LRCP with
	   one layer), only those kept; else every one, to get past them */
	need = cs->prog == 1 || cs->prog == 2 || (cs->prog == 0 && cs->layers == 1) ? maxkeep : maxres;
	for (ci = 0; ci < cs->nc && rc == JPX_OK; ci++) {
		TC *tc = &tcs[ci];

		for (r = 0; r < tc->nres && r < need && rc == JPX_OK; r++) {
			RES *R;
			long k, b, n = 0;

			rc = res_init(tc, r, tc->t->guard, cs->c[ci].prec);
			if (rc == JPX_OK && dc->poll && !dc->poll())
				rc = JPX_STOPPED;
			R = &tc->r[r];
			if (R->pb)
				for (k = 0; k < (long)R->pw * R->ph; k++)
					for (b = 0; b < R->nb; b++)
						n += (long)R->pb[k * R->nb + b].ncx * R->pb[k * R->nb + b].ncy;
			if (n > most)
				most = n;
		}
	}
	if (rc == JPX_OK && !(inc = malloc((size_t)(most ? most : 1) * sizeof *inc)))
		rc = JPX_MEMORY;
	if (rc == JPX_OK && t == 0) {
		/* the work ahead: the shown components' code-blocks kept, every
		   tile's as many as the first's */
		long nb = 0, k, b;

		for (ci = 0; ci < dc->outc; ci++)
			for (r = 0; r < keep[ci]; r++) {
				RES *R = &tcs[ci].r[r];

				if (R->pb)
					for (k = 0; k < (long)R->pw * R->ph; k++)
						for (b = 0; b < R->nb; b++)
							nb += (long)R->pb[k * R->nb + b].ncx * R->pb[k * R->nb + b].ncy;
			}
		dc->of = nb * cs->ntx * cs->nty;
	}
	/* tier 2: every packet read in the tile's order (a turn every 64) */
	if (rc == JPX_OK) {
		int L = cs->layers, prog = cs->prog;
		long npk = 0;

#define PACKET(cs_, R_, k_, l_)                                                              \
	((rc = packet((cs_), (R_), (k_), (l_), &p, end, inc)) == JPX_OK && (++npk & 63) == 0 && \
		 dc->poll && !dc->poll() ? JPX_STOPPED : rc)

		if (prog == 0 || prog == 1) {
			int o1, o2, nl1 = prog == 0 ? L : need, nl2 = prog == 0 ? need : L;

			for (o1 = 0; o1 < nl1 && rc == JPX_OK && p < end; o1++)
				for (o2 = 0; o2 < nl2 && rc == JPX_OK && p < end; o2++) {
					int lay = prog == 0 ? o1 : o2, res = prog == 0 ? o2 : o1;

					for (ci = 0; ci < cs->nc && rc == JPX_OK; ci++) {
						RES *R = &tcs[ci].r[res];
						long k;

						if (res >= tcs[ci].nres)
							continue;
						for (k = 0; k < (long)R->pw * R->ph && rc == JPX_OK; k++)
							rc = PACKET(cs, R, k, lay);
					}
				}
		} else if (prog >= 2 && prog <= 4) {
			/* RPCL, PCRL, CPRL: where every resolution has one precinct */
			for (ci = 0; ci < cs->nc; ci++)
				for (r = 0; r < tcs[ci].nres; r++)
					if ((long)tcs[ci].r[r].pw * tcs[ci].r[r].ph > 1)
						rc = JPX_UNSUPPORTED;
			if (prog == 2) {
				for (r = 0; r < need && rc == JPX_OK; r++)
					for (ci = 0; ci < cs->nc && rc == JPX_OK; ci++)
						for (l = 0; l < L && rc == JPX_OK && r < tcs[ci].nres; l++)
							rc = PACKET(cs, &tcs[ci].r[r], 0, l);
			} else {
				for (ci = 0; ci < cs->nc && rc == JPX_OK; ci++)
					for (r = 0; r < tcs[ci].nres && rc == JPX_OK; r++)
						for (l = 0; l < L && rc == JPX_OK; l++)
							rc = PACKET(cs, &tcs[ci].r[r], 0, l);
			}
		} else
			rc = JPX_UNSUPPORTED;
#undef PACKET
	}
	/* tier 1 and the synthesis, a component at a time: those in the planes
	   first, component 0 last (one tile: its rows go straight out); those
	   not shown (an alpha) not at all */
	for (ci = 1; ci < dc->outc && rc == JPX_OK; ci++)
		rc = tile_comp(dc, &tcs[ci], ci, keep[ci]);
	if (rc == JPX_OK)
		rc = tile_comp(dc, &tcs[0], 0, keep[0]);
	for (ci = 0; ci < cs->nc; ci++)
		for (r = 0; r < 33; r++)
			res_free(&tcs[ci].r[r]);
	free(tcs);
	free(inc);
	return rc;
}

/* ------------------------------------------------------------------ the picture */

/* the code-stream's main header read (on the heap: 5 KB); NULL and *err */
static CS *cs_open(const uint8_t *d, long n, long *p, int *err)
{
	CS *cs = calloc(1, sizeof *cs);
	int pal = 0;

	*err = JPX_MEMORY;
	if (!cs)
		return NULL;
	if (!codestream(d, n, &cs->d, &cs->n, &pal, &cs->sycc))
		*err = JPX_DAMAGED;
	else if (pal)
		*err = JPX_UNSUPPORTED;
	else if ((*err = main_header(cs, p)) == JPX_OK)
		return cs;
	free(cs);
	return NULL;
}

int jpx_info(const uint8_t *d, long n, int reduce, int *w, int *h, int *comps, int *levels)
{
	long p;
	int r;
	CS *cs = cs_open(d, n, &p, &r);

	if (!cs)
		return r;
	*levels = cs->tcc[0].levels;
	for (r = 1; r < cs->nc; r++)
		if (cs->tcc[r].levels < *levels)
			*levels = cs->tcc[r].levels;
	if (reduce > *levels)
		reduce = *levels;
	if (reduce < 0)
		reduce = 0;
	*w = ceilpow(cs->X1, reduce) - ceilpow(cs->X0, reduce);
	*h = ceilpow(cs->Y1, reduce) - ceilpow(cs->Y0, reduce);
	*comps = cs->nc;
	free(cs);
	return JPX_OK;
}

int jpx_decode(const uint8_t *d, long n, int reduce, JPX_ROW row, void *u, int (*poll)(void), int *ow, int *oh, int *oc)
{
	CS *cs;
	DEC dc;
	TCC *tcc;
	long p;
	int r, ci, t, nt;

	memset(&dc, 0, sizeof dc);
	*ow = *oh = *oc = 0;
	if (!(cs = cs_open(d, n, &p, &r)))
		return r;
	if (!(tcc = malloc(sizeof cs->tcc))) {
		free(cs);
		return JPX_MEMORY;
	}
	for (ci = 0; ci < cs->nc; ci++)
		if (reduce > cs->tcc[ci].levels)
			reduce = cs->tcc[ci].levels;
	if (reduce < 0)
		reduce = 0;
	dc.cs = cs;
	dc.reduce = reduce;
	dc.poll = poll;
	cs->poll = poll;
	dc.row = row;
	dc.u = u;
	dc.ow = ceilpow(cs->X1, reduce) - ceilpow(cs->X0, reduce);
	dc.oh = ceilpow(cs->Y1, reduce) - ceilpow(cs->Y0, reduce);
	dc.outc = cs->nc >= 3 ? 3 : 1;
	dc.ycc = dc.outc == 3 && (cs->mct || cs->sycc);
	nt = cs->ntx * cs->nty;
	dc.direct = nt == 1;
	/* (the components shown at the picture's size: subsampled ones not yet) */
	for (ci = 0; ci < dc.outc; ci++)
		if (cs->c[ci].dx != 1 || cs->c[ci].dy != 1)
			r = JPX_UNSUPPORTED;
	if (r == JPX_OK && !(dc.line = malloc((size_t)dc.ow * dc.outc)))
		r = JPX_MEMORY;
	for (ci = dc.direct ? 1 : 0; ci < dc.outc && r == JPX_OK; ci++)
		if (!(dc.plane[ci] = malloc((size_t)dc.ow * dc.oh)))
			r = JPX_MEMORY;
	nt = cs->ntx * cs->nty;
	/* each tile: its tile-parts gathered, then decoded */
	for (t = 0; t < nt && r == JPX_OK; t++) {
		long q = p, blen = 0;
		uint8_t *body = NULL;
		const uint8_t *one = NULL;
		int parts = 0, pm = 0;

		memcpy(tcc, cs->tcc, sizeof cs->tcc);
		while (q + 12 <= cs->n && rd16(cs->d + q) == 0xFF90) {
			unsigned isot = rd16(cs->d + q + 4);
			long psot = (long)rd32(cs->d + q + 6), hdr = q + 2 + rd16(cs->d + q + 2), e;

			if (psot == 0)
				psot = cs->n - q;
			if (q + psot > cs->n || hdr > cs->n)
				break;
			e = q + psot;
			if ((int)isot == t) {
				/* its header's markers, then its body (after SOD) */
				long m = hdr;

				while (m + 4 <= e && rd16(cs->d + m) != 0xFF93) {
					unsigned L = rd16(cs->d + m + 2);

					if (!parts && (r = header_marker(cs, tcc, rd16(cs->d + m), cs->d + m + 4, (long)L - 2, &pm)) != JPX_OK)
						break;
					m += 2 + L;
				}
				if (r != JPX_OK)
					break;
				m += 2;
				if (m < e) {
					if (!parts)
						one = cs->d + m;
					else {
						uint8_t *nb = realloc(body, (size_t)(blen + e - m));

						if (!nb) {
							r = JPX_MEMORY;
							break;
						}
						if (!body)
							memcpy(nb, one, (size_t)blen);
						body = nb;
						memcpy(body + blen, cs->d + m, (size_t)(e - m));
					}
					blen += e - m;
					parts++;
				}
			}
			q = e;
		}
		if (r == JPX_OK && parts)
			r = tile(&dc, t, body ? body : one, blen, tcc);
		free(body);
	}
	/* more than one tile: the rows from the planes (component 0's
	   back to the values emit takes) */
	if (r == JPX_OK && !dc.direct) {
		int16_t *v = malloc((size_t)dc.ow * sizeof *v);
		long y, x, half = cs->c[0].sgnd && !dc.ycc ? 0 : 128;

		if (!v)
			r = JPX_MEMORY;
		for (y = 0; y < dc.oh && r == JPX_OK; y++) {
			for (x = 0; x < dc.ow; x++)
				v[x] = (int16_t)(((long)dc.plane[0][y * dc.ow + x] - half) * (1 << FRAC));
			if (!emit(&dc, y, v, cs->c[0].sgnd))
				r = JPX_STOPPED;
		}
		free(v);
	}
	free(dc.line);
	for (ci = 0; ci < 3; ci++)
		free(dc.plane[ci]);
	*ow = dc.ow;
	*oh = dc.oh;
	*oc = dc.outc;
	free(tcc);
	free(cs);
	return r;
}

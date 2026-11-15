/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pdftext.c - fonts (encodings, ToUnicode, widths), the content-stream
 * interpreter and the text of a page in reading order.
 *
 * The interpreter keeps its numbers in 16.16 fixed point and follows the
 * text state and the transformation matrices; it draws nothing, it counts
 * what drawing would take (paths, pictures, glyphs) for the measurement.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pdfint.h"
#include "lexnum.h"
#include "pdftab.h"

static void pause_point(PDOC *d);
static void pd_sort(PDOC *d, void *base, long n, long size, int (*cmp)(const void *, const void *));

/* ------------------------------------------------------------------ fonts */
typedef struct { uint32_t lo, hi, dst; } TUR;	/* dst: index into the pool (a length, then UTF-16 units) */
typedef struct { uint32_t lo, hi; int16_t w; } WR;

typedef struct {
	long objnum;
	int composite, nbytes;
	uint16_t uni[256];		/* simple: code -> Unicode (0: none) */
	uint16_t mpos[256];		/* simple: pool index + 1 of a code that is several characters (fi, ffl) */
	int32_t wfx[256];		/* simple: advance, em fractions in fx */
	int32_t dwfx;			/* default advance */
	int has_map;			/* the font says what its characters are */
	TUR *tu;
	long ntu, captu;
	uint16_t *pool;
	long npool, cappool;
	WR *wr;
	long nwr;
	int dw;				/* composite: default width (1/1000) */
	int ucs2;			/* the codes are Unicode (a Uni*-UCS2 / UTF16 encoding) */
	long prog;			/* its font program's object number, -1: none embedded */
	int pkind;			/* 1 Type 1, 2 TrueType, 3 CFF / OpenType, 4 none */
	char base[40];		/* BaseFont */
	long mem;
	long serial;			/* this font's number, never reused (show_simple's table) */
} PFONT;

#define FC_MAX 24
typedef struct {
	struct { long obj; PFONT *f; long age; } e[FC_MAX];
	long clock;
	int32_t adv[256];		/* show_simple's advances (IX.adv): here, with the
					   document, not taken and given back a page at a time
					   (a 1 MB ST's heap, big.py: 13 Nov 2026) */
} FCACHE;

static void font_free(PDOC *d, PFONT *f)
{
	if (!f)
		return;
	pdf_free(d, f->tu, f->captu * (long)sizeof(TUR));
	pdf_free(d, f->pool, f->cappool * 2);
	pdf_free(d, f->wr, f->nwr * (long)sizeof(WR));
	pdf_free(d, f, (long)sizeof *f);
}

/* glyph name -> Unicode */
/* a block's first name (pdftab.h's agl_names, ended by the next byte
   under 0x20) against n: <0, 0, >0 as strcmp */
static int agl_head_cmp(const unsigned char *h, const unsigned char *n)
{
	while (*h >= 0x20 && *h == *n)
		h++, n++;
	return (*h >= 0x20 ? *h : 0) - *n;
}

/* a glyph name's character from the Adobe Glyph List, 0 none: the last
   block whose first name isn't after it, then that block's names made
   one after another */
static int agl_find(const char *n)
{
	const unsigned char *s = (const unsigned char *)n, *p;
	char buf[64];
	int lo = 0, hi = AGL_NBLK - 1, i, k;

	while (lo < hi) {
		int mid = (lo + hi + 1) / 2;

		if (agl_head_cmp((const unsigned char *)agl_names + agl_blk[mid] + 1, s) <= 0)
			lo = mid;
		else
			hi = mid - 1;
	}
	p = (const unsigned char *)agl_names + agl_blk[lo];
	for (i = lo * AGL_BLK; i < AGL_N && i < (lo + 1) * AGL_BLK; i++) {
		int c;

		k = *p++;
		while (*p >= 0x20)
			buf[k++] = (char)*p++;
		buf[k] = 0;
		c = strcmp(buf, n);
		if (!c)
			return agl_uc[i];
		if (c > 0)
			break;
	}
	return 0;
}

static int hexn(const char *s, int n)
{
	int v = 0;

	while (n-- > 0) {
		int c = *s++;

		v <<= 4;
		if (c >= '0' && c <= '9') v |= c - '0';
		else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
		else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
		else return -1;
	}
	return v;
}

static int glyph_unicode(const char *n)
{
	int u = agl_find(n);
	const char *dot;
	char base[64];

	if (u)
		return u;
	if (n[0] == 'u' && n[1] == 'n' && n[2] == 'i' && strlen(n) >= 7) {
		u = hexn(n + 3, 4);
		return u > 0 ? u : 0;
	}
	if (n[0] == 'u' && strlen(n) >= 5 && strlen(n) <= 7) {
		u = hexn(n + 1, (int)strlen(n) - 1);
		return u > 0 && u < 0x10000 ? u : 0;
	}
	dot = strchr(n, '.');			/* "a.sc": the glyph a */
	if (dot && dot > n && dot - n < 63) {
		memcpy(base, n, (size_t)(dot - n));
		base[dot - n] = 0;
		return agl_find(base);
	}
	return 0;
}

static int tu_cmp(const void *a, const void *b)
{
	const TUR *x = a, *y = b;

	return x->lo < y->lo ? -1 : x->lo > y->lo;
}

static int pool_add(PDOC *d, PFONT *f, const uint16_t *u, int n)
{
	long at = f->npool;

	if (f->npool + n + 1 > f->cappool) {
		long cap = f->cappool ? f->cappool * 2 : 256;
		uint16_t *q;

		while (cap < f->npool + n + 1)
			cap *= 2;
		q = pdf_realloc(d, f->pool, f->cappool * 2, cap * 2);
		if (!q)
			return -1;
		f->pool = q;
		f->cappool = cap;
	}
	f->pool[f->npool++] = (uint16_t)n;
	memcpy(f->pool + f->npool, u, (size_t)n * 2);
	f->npool += n;
	return (int)at;
}

static int tu_add(PDOC *d, PFONT *f, uint32_t lo, uint32_t hi, int pooli)
{
	if (f->ntu == f->captu) {
		long cap = f->captu ? f->captu * 2 : 128;
		TUR *q = pdf_realloc(d, f->tu, f->captu * (long)sizeof(TUR), cap * (long)sizeof(TUR));

		if (!q)
			return -1;
		f->tu = q;
		f->captu = cap;
	}
	f->tu[f->ntu].lo = lo;
	f->tu[f->ntu].hi = hi;
	f->tu[f->ntu].dst = (uint32_t)pooli;
	f->ntu++;
	return 0;
}

/* one <hex> token: bytes into out (up to max), count; p after the '>' */
static int cm_hex(const unsigned char **pp, const unsigned char *e, unsigned char *out, int max)
{
	const unsigned char *p = *pp;
	int n = 0, hi = -1;

	while (p < e && *p != '>') {
		int v = *p >= '0' && *p <= '9' ? *p - '0' : *p >= 'a' && *p <= 'f' ? *p - 'a' + 10 :
			*p >= 'A' && *p <= 'F' ? *p - 'A' + 10 : -1;

		p++;
		if (v < 0)
			continue;
		if (hi < 0)
			hi = v;
		else {
			if (n < max)
				out[n++] = (unsigned char)(hi << 4 | v);
			hi = -1;
		}
	}
	if (hi >= 0 && n < max)
		out[n++] = (unsigned char)(hi << 4);
	*pp = p < e ? p + 1 : p;
	return n;
}

static uint32_t be_n(const unsigned char *b, int n)
{
	uint32_t v = 0;

	while (n-- > 0)
		v = (v << 8) | *b++;
	return v;
}

static int utf16_of(const unsigned char *b, int n, uint16_t *u)
{
	int i, k = 0;

	for (i = 0; i + 1 < n && k < 8; i += 2)
		u[k++] = (uint16_t)(b[i] << 8 | b[i + 1]);
	if (n == 1)
		u[k++] = b[0];
	return k;
}

/* the ToUnicode CMap */
static void tounicode_parse(PDOC *d, PFONT *f, const unsigned char *s, long n)
{
	const unsigned char *p = s, *e = s + n;
	int mode = 0;			/* 1 bfchar, 2 bfrange, 3 codespace */
	unsigned char a[16], b[16], c[64];
	unsigned turn = 0;

	while (p < e) {
		if (!(++turn & 63))
			pause_point(d);		/* a big CMap takes a while on an ST */
		while (p < e && is_white_c(*p))
			p++;
		if (p >= e)
			break;
		if (*p == '<' && p + 1 < e && p[1] != '<') {
			int na, nb, nc;

			p++;
			na = cm_hex(&p, e, a, 16);
			if (mode == 3) {
				const unsigned char *q = p;

				while (q < e && is_white_c(*q))
					q++;
				if (q < e && *q == '<') {
					q++;
					nb = cm_hex(&q, e, b, 16);
					p = q;
					if (na > f->nbytes && f->composite)
						f->nbytes = na;
				}
				continue;
			}
			if (mode == 1) {
				const unsigned char *q = p;
				uint16_t u[8];
				int k, pi;

				while (q < e && is_white_c(*q))
					q++;
				if (q < e && *q == '<') {
					q++;
					nc = cm_hex(&q, e, c, 64);
					p = q;
					k = utf16_of(c, nc, u);
					pi = pool_add(d, f, u, k);
					if (pi >= 0)
						tu_add(d, f, be_n(a, na), be_n(a, na), pi);
					d->st.tounicode_ent++;
				}
				continue;
			}
			if (mode == 2) {
				const unsigned char *q = p;
				uint32_t lo = be_n(a, na), hi;

				while (q < e && is_white_c(*q))
					q++;
				if (q < e && *q == '<') {
					q++;
					nb = cm_hex(&q, e, b, 16);
					hi = be_n(b, nb);
					while (q < e && is_white_c(*q))
						q++;
					if (q < e && *q == '<') {
						uint16_t u[8];
						int k, pi;

						q++;
						nc = cm_hex(&q, e, c, 64);
						k = utf16_of(c, nc, u);
						pi = pool_add(d, f, u, k);
						if (pi >= 0)
							tu_add(d, f, lo, hi, pi);
						d->st.tounicode_ent++;
					} else if (q < e && *q == '[') {
						uint32_t code = lo;

						q++;
						for (;;) {
							uint16_t u[8];
							int k, pi;

							while (q < e && is_white_c(*q))
								q++;
							if (q >= e || *q == ']') {
								q++;
								break;
							}
							if (*q != '<') {
								q++;
								continue;
							}
							q++;
							nc = cm_hex(&q, e, c, 64);
							k = utf16_of(c, nc, u);
							pi = pool_add(d, f, u, k);
							if (pi >= 0 && code <= hi)
								tu_add(d, f, code, code, pi);
							code++;
							d->st.tounicode_ent++;
						}
					}
					p = q;
				}
				continue;
			}
			continue;
		}
		if (*p == '/' || *p == '<' || *p == '(' || *p == '[' || *p == ']' || *p == '>') {
			if (*p == '<' || *p == '>')
				p++;
			p++;
			continue;
		}
		{
			const unsigned char *k = p;

			while (p < e && !is_white_c(*p) && *p != '<' && *p != '/' && *p != '[' && *p != '(')
				p++;
			if (p - k == 11 && !memcmp(k, "beginbfchar", 11))
				mode = 1;
			else if (p - k == 12 && !memcmp(k, "beginbfrange", 12))
				mode = 2;
			else if (p - k == 19 && !memcmp(k, "begincodespacerange", 19))
				mode = 3;
			else if (p - k >= 3 && !memcmp(k, "end", 3))
				mode = 0;
			if (p == k)
				p++;
		}
	}
	if (f->ntu > 1)
		pd_sort(d, f->tu, f->ntu, (long)sizeof(TUR), tu_cmp);
}

/* ToUnicode lookup: the UTF-16 units into u, their count (0: none) */
static int tu_lookup(const PFONT *f, uint32_t code, uint16_t *u)
{
	long lo = 0, hi = f->ntu - 1;

	while (lo <= hi) {
		long mid = (lo + hi) / 2;
		const TUR *t = &f->tu[mid];

		if (code < t->lo)
			hi = mid - 1;
		else if (code > t->hi) {
			/* ranges may nest or overlap a little: look at the nearest before */
			lo = mid + 1;
		} else {
			const uint16_t *src = f->pool + t->dst;
			int n = src[0], i;

			for (i = 0; i < n && i < 8; i++)
				u[i] = src[1 + i];
			if (code > t->lo && n > 0)
				u[n - 1] = (uint16_t)(u[n - 1] + (code - t->lo));
			return n;
		}
	}
	/* a code just under lo of the next range may sit in an earlier, wider range */
	if (lo > 0) {
		long k;

		for (k = lo - 1; k >= 0 && k >= lo - 4; k--) {
			const TUR *t = &f->tu[k];

			if (code >= t->lo && code <= t->hi) {
				const uint16_t *src = f->pool + t->dst;
				int n = src[0], i;

				for (i = 0; i < n && i < 8; i++)
					u[i] = src[1 + i];
				if (code > t->lo && n > 0)
					u[n - 1] = (uint16_t)(u[n - 1] + (code - t->lo));
				return n;
			}
		}
	}
	return 0;
}

static int32_t w2fx(int w)
{
	return (int32_t)(((int32_t)w * 8389) >> 7);		/* w / 1000 as 16.16 */
}

static const unsigned short *std_widths(const char *base, int *courier)
{
	int bold, ital;

	*courier = 0;
	if (!base)
		return NULL;
	if (strchr(base, '+') && strchr(base, '+') - base == 6)
		base = strchr(base, '+') + 1;
	bold = strstr(base, "Bold") != NULL || strstr(base, "bold") != NULL;
	ital = strstr(base, "Italic") != NULL || strstr(base, "Oblique") != NULL || strstr(base, "italic") != NULL;
	if (!strncmp(base, "Courier", 7) || !strncmp(base, "CourierNew", 10)) {
		*courier = 1;
		return NULL;
	}
	if (!strncmp(base, "Helvetica", 9) || !strncmp(base, "Arial", 5))
		return bold ? stdw_helvb : stdw_helv;
	if (!strncmp(base, "Times", 5)) {
		if (bold && ital) return stdw_timesbi;
		if (bold) return stdw_timesb;
		if (ital) return stdw_timesi;
		return stdw_times;
	}
	return NULL;
}


/* ------------------------------------------------------------------ font loading */
static void fill_simple_from_tu(PFONT *f)
{
	int c;
	uint16_t u[8];

	for (c = 0; c < 256; c++) {
		int n = tu_lookup(f, (uint32_t)c, u);

		if (n == 1) {
			f->uni[c] = u[0];
			f->mpos[c] = 0;
		} else if (n > 1) {
			/* find its pool entry again: the table points at it */
			long lo = 0, hi = f->ntu - 1;

			while (lo <= hi) {
				long mid = (lo + hi) / 2;

				if ((uint32_t)c < f->tu[mid].lo)
					hi = mid - 1;
				else if ((uint32_t)c > f->tu[mid].hi)
					lo = mid + 1;
				else {
					if (f->tu[mid].dst < 65535 && f->tu[mid].lo == f->tu[mid].hi)
						f->mpos[c] = (uint16_t)(f->tu[mid].dst + 1);
					else
						f->uni[c] = u[0];
					break;
				}
			}
		}
	}
}

static int w_of(PDOC *d, POBJ *o)
{
	if (!o)
		return 0;
	o = pdf_res(d, o);
	if (!o)
		return 0;
	if (o->t == PO_INT)
		return o->a;
	if (o->t == PO_REAL)
		return (o->a + 32768) >> 16;
	return 0;
}

static int wr_cmp(const void *a, const void *b)
{
	const WR *x = a, *y = b;

	return x->lo < y->lo ? -1 : x->lo > y->lo;
}

static int wr_add(PDOC *d, PFONT *f, long *cap, uint32_t lo, uint32_t hi, int w)
{
	if (f->nwr == *cap) {
		long nc = *cap ? *cap * 2 : 64;
		WR *q = pdf_realloc(d, f->wr, *cap * (long)sizeof(WR), nc * (long)sizeof(WR));

		if (!q)
			return -1;
		f->wr = q;
		*cap = nc;
	}
	f->wr[f->nwr].lo = lo;
	f->wr[f->nwr].hi = hi;
	f->wr[f->nwr].w = (int16_t)w;
	f->nwr++;
	return 0;
}

static void note_prog(PDOC *d, PTEXT *t, POBJ *desc, const char *base, PFONT *f)
{
	static const char *const keys[3] = { "FontFile", "FontFile2", "FontFile3" };
	int k;

	for (k = 0; k < 3; k++) {
		POBJ *r = pd_getraw(desc, keys[k]), *so;
		int i, dup = 0;

		if (!r || r->t != PO_REF)
			continue;
		for (i = 0; i < t->nfp; i++)
			if (t->fp[i].obj == r->a)
				dup = 1;
		if (dup || t->nfp >= 24)
			return;
		so = pdf_get(d, r->a);
		f->prog = r->a;
		f->pkind = k == 0 ? 1 : k == 1 ? 2 : 3;
		t->fp[t->nfp].obj = r->a;
		t->fp[t->nfp].kind = k == 0 ? 1 : k == 1 ? 2 : 3;
		t->fp[t->nfp].len = so ? pdf_stream_len(d, so) : 0;
		t->nfp++;
		d->st.font_prog++;
		d->st.font_prog_bytes += so ? pdf_stream_len(d, so) : 0;
		return;
	}
	f->prog = -1;
	f->pkind = 4;
	if (t->nfp < 24) {
		int i;

		for (i = 0; i < t->nfp; i++)
			if (t->fp[i].obj == -1)
				return;
		t->fp[t->nfp].obj = -1;		/* not embedded: the standard font of that name */
		t->fp[t->nfp].kind = 4;
		t->fp[t->nfp].len = 0;
		t->nfp++;
	}
	(void)base;
}


/* The built-in encoding of an embedded Type 1 font: the clear-text part of the font
   program says "/Encoding 256 array ... dup 65 /A put ... readonly def". 1: found
   (uni filled), 0: it says StandardEncoding or isn't there. */
static int t1_encoding(PDOC *d, const char *s, long n, uint16_t *uni)
{
	const char *p, *e = s + n, *q;
	int found = 0;

	p = NULL;
	for (q = s; q + 10 < e && q < s + 16384; q++)
		if (q[0] == '/' && !memcmp(q, "/Encoding", 9)) {
			p = q + 9;
			break;
		}
	if (p) {
		while (p < e && is_white_c((unsigned char)*p))
			p++;
		if (e - p >= 16 && !memcmp(p, "StandardEncoding", 16)) {
			found = 0;
		} else {
			/* "dup 65 /A put" until "def"; a code it leaves out keeps
			   the standard one (Calamus SL's fonts have no space, and
			   their text is set with code 32 between words) */
			while (p + 4 < e) {
				if (!memcmp(p, "dup", 3) && is_white_c((unsigned char)p[3])) {
					const char *s = p + 3;
					long code = 0;
					char name[64];
					int n = 0;

					while (s < e && is_white_c((unsigned char)*s))
						s++;
					if (s < e && *s == '8' && s + 1 < e && s[1] == '#') {		/* 8#101: octal */
						s += 2;
						while (s < e && *s >= '0' && *s <= '7')
							code = code * 8 + (*s++ - '0');
					} else
						while (s < e && *s >= '0' && *s <= '9')
							code = code * 10 + (*s++ - '0');
					while (s < e && is_white_c((unsigned char)*s))
						s++;
					if (s < e && *s == '/') {
						s++;
						while (s < e && !is_white_c((unsigned char)*s) && n < 63)
							name[n++] = *s++;
						name[n] = 0;
						if (code >= 0 && code < 256) {
							uni[code] = (uint16_t)glyph_unicode(name);
							if (!(++found & 31))
								pause_point(d);
						}
					}
					p = s;
					continue;
				}
				if (!memcmp(p, "readonly", 8) || (p[0] == 'd' && p[1] == 'e' && p[2] == 'f' && is_white_c((unsigned char)p[3]) && found))
					break;
				if (found == 0 && p > s + 16384)
					break;
				p++;
			}
		}
	}
	return found > 0;
}

/*
 * A font program's encoding is near its start: its first HEAD_FIRST bytes
 * are read, and more only where they don't hold it. A Type 1 program's
 * clear text ends at "eexec" (Calamus SL's are 4 KB of a 40 KB font).
 */
#define HEAD_FIRST 4096L

static int has_eexec(const char *s, long n)
{
	const char *e = s + n - 5;

	for (; s <= e; s++)
		if (*s == 'e' && !memcmp(s, "eexec", 5))
			return 1;
	return 0;
}

/* b: the program's first bytes (whole: all of it); the clear text, up to
   cap bytes, read again when they don't hold it all. 0, or -1 */
static int t1_clear(PDOC *d, POBJ *so, long onum, long cap, SBUF *b, int whole)
{
	const unsigned char *u = (const unsigned char *)b->s;

	if (whole || has_eexec(b->s, b->len) || b->len >= cap)
		return 0;
	if (b->len >= 6 && u[0] == 0x80 && u[1] == 1) {
		/* a PFB: the first segment's length */
		long seg = (long)u[2] | (long)u[3] << 8 | (long)u[4] << 16 | (long)u[5] << 24;

		if (seg > 0 && seg + 6 <= b->len)
			return 0;
		if (seg > 0 && seg + 6 < cap)
			cap = seg + 6;
	}
	sb_free(b);
	return pdf_stream_head(d, so, onum, cap, b, &whole);
}

static int t1_builtin(PDOC *d, POBJ *desc, uint16_t *uni)
{
	POBJ *r = pd_getraw(desc, "FontFile"), *so;
	SBUF b;
	int found, whole;
	long clear;

	if (!r || r->t != PO_REF)
		return 0;
	so = pdf_get(d, r->a);
	if (!so)
		return 0;
	/* the clear-text part (Length1) holds the encoding: no more is read */
	clear = pd_int(d, so, "Length1", 0);
	if (clear <= 0 || clear > 65536L)
		clear = clear > 0 ? 65536L : 32768L;
	if (pdf_stream_head(d, so, r->a, clear < HEAD_FIRST ? clear : HEAD_FIRST, &b, &whole) != 0)
		return 0;
	if (t1_clear(d, so, r->a, clear, &b, whole) != 0)
		return 0;
	found = t1_encoding(d, b.s, b.len, uni);
	sb_free(&b);
	return found;
}


/* ---- CFF: the built-in encoding of an embedded Type1C font ---- */
typedef struct { const unsigned char *b; long n; } CBUF;

static long cff_u(const unsigned char *p, int size)
{
	long v = 0;

	while (size-- > 0)
		v = (v << 8) | *p++;
	return v;
}

/* INDEX at off: count; *dataoff the start of the object data, offsets at off+3 */
static long cff_index(const CBUF *c, long off, long *count, int *osz, long *dataoff)
{
	long cnt;
	int sz;

	if (off < 0 || off + 2 > c->n)
		return -1;
	cnt = cff_u(c->b + off, 2);
	*count = cnt;
	if (!cnt) {
		*osz = 0;
		*dataoff = off + 2;
		return off + 2;
	}
	if (off + 3 > c->n)
		return -1;
	sz = c->b[off + 2];
	if (sz < 1 || sz > 4 || off + 3 + (cnt + 1) * sz > c->n)
		return -1;
	*osz = sz;
	*dataoff = off + 3 + (cnt + 1) * sz - 1;
	{
		long end = cff_u(c->b + off + 3 + cnt * sz, sz);

		return *dataoff + end;
	}
}

static int cff_entry(const CBUF *c, long off, long cnt, int sz, long dataoff, long i, const unsigned char **p, long *n)
{
	long a, b;

	if (i < 0 || i >= cnt)
		return 0;
	a = cff_u(c->b + off + 3 + i * sz, sz);
	b = cff_u(c->b + off + 3 + (i + 1) * sz, sz);
	if (dataoff + b > c->n || b < a)
		return 0;
	*p = c->b + dataoff + a;
	*n = b - a;
	return 1;
}

/* a DICT's operand of one operator (the first operand): -1 if absent */
static long cff_dict_get(const unsigned char *p, long n, int op, int esc)
{
	long st[48], ns = 0, i = 0;

	while (i < n) {
		int b0 = p[i++];

		if (b0 <= 21) {
			int o = b0;

			if (b0 == 12 && i < n)
				o = 0x100 | p[i++];
			if (o == (esc ? 0x100 | op : op))
				return ns ? st[0] : -1;
			ns = 0;
		} else if (b0 == 28 && i + 1 < n) {
			if (ns < 48) st[ns++] = (short)(p[i] << 8 | p[i + 1]);
			i += 2;
		} else if (b0 == 29 && i + 3 < n) {
			if (ns < 48) st[ns++] = (long)((unsigned long)p[i] << 24 | p[i + 1] << 16 | p[i + 2] << 8 | p[i + 3]);
			i += 4;
		} else if (b0 == 30) {
			while (i < n && (p[i] & 0x0F) != 0x0F && (p[i] >> 4) != 0x0F)
				i++;
			i++;
			if (ns < 48) st[ns++] = 0;
		} else if (b0 >= 32 && b0 <= 246) {
			if (ns < 48) st[ns++] = b0 - 139;
		} else if (b0 >= 247 && b0 <= 250 && i < n) {
			if (ns < 48) st[ns++] = (b0 - 247) * 256 + p[i] + 108;
			i++;
		} else if (b0 >= 251 && b0 <= 254 && i < n) {
			if (ns < 48) st[ns++] = -(b0 - 251) * 256 - p[i] - 108;
			i++;
		}
	}
	return -1;
}

/* part: data is the font's first n bytes only; the charset and encoding
   are taken only when they lie before the CharStrings (as they are put)
   and so wholly in it */
static int cff_encoding_in(const unsigned char *data, long n, uint16_t *uni, int part)
{
	CBUF c;
	long off, cnt, dat, top_n, i, enc_off, cs_off, chs_off, nglyphs, scnt, sdat;
	int osz, sosz, hdr;
	const unsigned char *top;
	long sidx;
	uint16_t *sid = NULL;
	int ok = 0, code;

	c.b = data;
	c.n = n;
	if (n < 8)
		return 0;
	if (!memcmp(data, "OTTO", 4)) {			/* OpenType: find the CFF table */
		int nt = data[4] << 8 | data[5], k;

		for (k = 0; k < nt && 12 + k * 16 + 16 <= n; k++)
			if (!memcmp(data + 12 + k * 16, "CFF ", 4)) {
				long o = cff_u(data + 12 + k * 16 + 8, 4), l = cff_u(data + 12 + k * 16 + 12, 4);

				if (o < n && o + l <= n) {
					c.b = data + o;
					c.n = l;
				} else if (part && o < n) {
					c.b = data + o;
					c.n = n - o;
				} else if (part)
					return 0;
				break;
			}
	}
	hdr = c.b[2];
	off = hdr;
	off = cff_index(&c, off, &cnt, &osz, &dat);			/* names */
	if (off < 0)
		return 0;
	off = cff_index(&c, off, &cnt, &osz, &dat);			/* top dicts */
	if (off < 0 || cnt < 1 || !cff_entry(&c, off - 0, 0, 0, 0, 0, &top, &top_n)) {
		/* re-read the entry from the INDEX we just passed */
	}
	{
		long toff;
		long cnt2, dat2;
		int osz2;

		toff = hdr;
		toff = cff_index(&c, toff, &cnt2, &osz2, &dat2);	/* names again, to find the top dict index start */
		if (toff < 0)
			return 0;
		{
			long t2 = cff_index(&c, toff, &cnt2, &osz2, &dat2);

			if (t2 < 0 || cnt2 < 1 || !cff_entry(&c, toff, cnt2, osz2, dat2, 0, &top, &top_n))
				return 0;
			sidx = t2;
		}
	}
	if (cff_dict_get(top, top_n, 30, 1) >= 0)		/* ROS: CID-keyed, no encoding */
		return 0;
	enc_off = cff_dict_get(top, top_n, 16, 0);
	chs_off = cff_dict_get(top, top_n, 15, 0);
	cs_off = cff_dict_get(top, top_n, 17, 0);
	if (enc_off < 0)
		enc_off = 0;
	if (chs_off < 0)
		chs_off = 0;
	if (enc_off == 0) {
		for (code = 0; code < 256; code++)
			uni[code] = enc_std[code];
		return 1;
	}
	if (enc_off == 1 || cs_off <= 0)
		return 0;
	if (cs_off + 2 > c.n || (part && (enc_off >= cs_off || chs_off >= cs_off)))
		return 0;
	nglyphs = cff_u(c.b + cs_off, 2);
	/* the string index, for the glyph names that are not standard */
	{
		long so;

		so = cff_index(&c, sidx, &scnt, &sosz, &sdat);		/* string index starts after the top dict index */
		(void)so;
	}
	sid = calloc((size_t)nglyphs + 1, sizeof *sid);
	if (!sid)
		return 0;
	/* charset: glyph -> SID */
	if (chs_off <= 2) {
		for (i = 0; i < nglyphs; i++)
			sid[i] = (uint16_t)i;		/* ISOAdobe; the expert sets are not told apart */
	} else if (chs_off + 1 < c.n) {
		int fmt = c.b[chs_off];
		long p = chs_off + 1, g = 1;

		if (fmt == 0) {
			for (; g < nglyphs && p + 2 <= c.n; g++, p += 2)
				sid[g] = (uint16_t)cff_u(c.b + p, 2);
		} else if (fmt == 1 || fmt == 2) {
			int ls = fmt == 1 ? 1 : 2;

			while (g < nglyphs && p + 2 + ls <= c.n) {
				long first = cff_u(c.b + p, 2), left = cff_u(c.b + p + 2, ls), k;

				p += 2 + ls;
				for (k = 0; k <= left && g < nglyphs; k++, g++)
					sid[g] = (uint16_t)(first + k);
			}
		}
	}
	/* encoding */
	for (code = 0; code < 256; code++)
		uni[code] = 0;
	{
		long p = enc_off;
		int fmt, gl = 1;

		if (p >= c.n)
			goto out;
		fmt = c.b[p++];
		if ((fmt & 0x7F) == 0) {
			int nc = c.b[p++];

			for (i = 0; i < nc && p < c.n; i++, gl++)
				if (gl < nglyphs) {
					uint16_t s = sid[gl];
					const char *nm = NULL;
					char tmp[64];

					if (s < 391)
						nm = cff_std[s];
					else {
						const unsigned char *sp;
						long sn;

						if (cff_entry(&c, sidx, scnt, sosz, sdat, s - 391, &sp, &sn) && sn < 63) {
							memcpy(tmp, sp, (size_t)sn);
							tmp[sn] = 0;
							nm = tmp;
						}
					}
					if (nm)
						uni[c.b[p + 0 - 0]] = (uint16_t)glyph_unicode(nm);
					p++;
				}
		} else if ((fmt & 0x7F) == 1) {
			int nr = c.b[p++];

			for (i = 0; i < nr && p + 1 < c.n; i++) {
				int first = c.b[p], left = c.b[p + 1], k;

				p += 2;
				for (k = 0; k <= left; k++, gl++) {
					uint16_t s;
					const char *nm = NULL;
					char tmp[64];

					if (gl >= nglyphs || first + k > 255)
						continue;
					s = sid[gl];
					if (s < 391)
						nm = cff_std[s];
					else {
						const unsigned char *sp;
						long sn;

						if (cff_entry(&c, sidx, scnt, sosz, sdat, s - 391, &sp, &sn) && sn < 63) {
							memcpy(tmp, sp, (size_t)sn);
							tmp[sn] = 0;
							nm = tmp;
						}
					}
					if (nm)
						uni[first + k] = (uint16_t)glyph_unicode(nm);
				}
			}
		}
		if (fmt & 0x80 && p < c.n) {
			int ns = c.b[p++];

			for (i = 0; i < ns && p + 2 < c.n; i++, p += 3) {
				int cd = c.b[p];
				uint16_t s = (uint16_t)cff_u(c.b + p + 1, 2);
				const char *nm = NULL;
				char tmp[64];

				if (s < 391)
					nm = cff_std[s];
				else {
					const unsigned char *sp;
					long sn;

					if (cff_entry(&c, sidx, scnt, sosz, sdat, s - 391, &sp, &sn) && sn < 63) {
						memcpy(tmp, sp, (size_t)sn);
						tmp[sn] = 0;
						nm = tmp;
					}
				}
				if (nm)
					uni[cd] = (uint16_t)glyph_unicode(nm);
			}
		}
		ok = 1;
	}
out:
	free(sid);
	return ok;
}

/* 1 found, 0 not; -1: a head (part) didn't hold it, all of it may */
static int cff_encoding(const unsigned char *data, long n, uint16_t *uni, int part)
{
	int r = cff_encoding_in(data, n, uni, part);

	return part && !r && n >= 4 && (data[0] == 1 || !memcmp(data, "OTTO", 4)) ? -1 : r;
}

#define CFF_HEAD 32768L		/* the header, its INDEXes, encoding and charset come first */

static int cff_builtin(PDOC *d, POBJ *desc, uint16_t *uni)
{
	POBJ *r = pd_getraw(desc, "FontFile3"), *so;
	SBUF b;
	int ok, whole;

	if (!r || r->t != PO_REF)
		return 0;
	so = pdf_get(d, r->a);
	if (!so || pdf_stream_head(d, so, r->a, HEAD_FIRST, &b, &whole) != 0)
		return 0;
	if (b.len >= 2 && ((b.s[0] == (char)0x80 && b.s[1] == 1) || (b.s[0] == '%' && b.s[1] == '!'))) {
		/* a Type 1 program labelled Type1C (Calamus SL's Bridge) */
		if (t1_clear(d, so, r->a, 65536L, &b, whole) != 0)
			return 0;
		ok = t1_encoding(d, b.s, b.len, uni);
		sb_free(&b);
		return ok;
	}
	ok = cff_encoding((const unsigned char *)b.s, b.len, uni, !whole);
	sb_free(&b);
	if (ok < 0) {
		/* the encoding or charset lies past the first bytes: more */
		if (pdf_stream_head(d, so, r->a, CFF_HEAD, &b, &whole) != 0)
			return 0;
		ok = cff_encoding((const unsigned char *)b.s, b.len, uni, !whole);
		sb_free(&b);
	}
	if (ok < 0) {
		/* and past those: all of it */
		if (pdf_stream_head(d, so, r->a, 4L << 20, &b, &whole) != 0)
			return 0;
		ok = cff_encoding((const unsigned char *)b.s, b.len, uni, 0);
		sb_free(&b);
	}
	return ok > 0;
}

static PFONT *font_load_in(PDOC *d, POBJ *fd, long objnum, PTEXT *t);

static PFONT *font_load(PDOC *d, POBJ *fd, long objnum, PTEXT *t)
{
	long t0 = d->clock ? d->clock() : 0, s0 = d->st.t_stream;
	PFONT *f = font_load_in(d, fd, objnum, t);

	if (d->clock)
		d->st.t_font += d->clock() - t0 - (d->st.t_stream - s0);
	return f;
}

static PFONT *font_load_in(PDOC *d, POBJ *fd, long objnum, PTEXT *t)
{
	PFONT *f = pdf_malloc(d, (long)sizeof *f);
	const char *sub = pd_name(d, fd, "Subtype"), *base = pd_name(d, fd, "BaseFont");
	POBJ *desc = NULL, *tu;
	int c;

	if (!f)
		return NULL;
	memset(f, 0, sizeof *f);
	d->st.fonts++;
	{
		static long serials;

		f->serial = ++serials;
	}
	f->objnum = objnum;
	if (base)
		strncpy(f->base, base, sizeof f->base - 1);
	f->prog = -1;
	f->pkind = 4;
	f->composite = sub && !strcmp(sub, "Type0");
	if (f->composite) {
		POBJ *enc = pd_get(d, fd, "Encoding"), *dfs = pd_get(d, fd, "DescendantFonts"), *df, *w;
		long cap = 0, i;

		if (enc && enc->t == PO_NAME && (strstr(enc->u.p, "UCS2") || strstr(enc->u.p, "UTF16")))
			f->ucs2 = 1;
		df = dfs && dfs->t == PO_ARR ? pdf_res(d, pa_at(dfs, 0)) : NULL;
		f->dw = df ? (int)pd_int(d, df, "DW", 1000) : 1000;
		f->dwfx = w2fx(f->dw);
		desc = df ? pd_get(d, df, "FontDescriptor") : NULL;
		w = df ? pd_get(d, df, "W") : NULL;
		for (i = 0; w && w->t == PO_ARR && i < w->a;) {
			long c1 = w_of(d, &w->u.v[i]);
			POBJ *nx = i + 1 < w->a ? pdf_res(d, &w->u.v[i + 1]) : NULL;

			if (!nx)
				break;
			if (nx->t == PO_ARR) {
				long k;

				for (k = 0; k < nx->a; k++)
					wr_add(d, f, &cap, (uint32_t)(c1 + k), (uint32_t)(c1 + k), w_of(d, &nx->u.v[k]));
				i += 2;
			} else {
				long c2 = w_of(d, nx);

				if (i + 2 >= w->a)
					break;
				wr_add(d, f, &cap, (uint32_t)c1, (uint32_t)c2, w_of(d, &w->u.v[i + 2]));
				i += 3;
			}
		}
		if (f->nwr > 1)
			pd_sort(d, f->wr, f->nwr, (long)sizeof(WR), wr_cmp);
	} else {
		POBJ *enc = pd_get(d, fd, "Encoding"), *wa = pd_get(d, fd, "Widths"), *diff = NULL;
		const unsigned short *base_tab = NULL;
		const unsigned short *sw;
		int courier, first = (int)pd_int(d, fd, "FirstChar", 0), miss = 0, istype3 = sub && !strcmp(sub, "Type3");
		const char *ename = NULL;

		f->nbytes = 1;
		desc = pd_get(d, fd, "FontDescriptor");
		if (desc)
			miss = (int)pd_int(d, desc, "MissingWidth", 0);
		if (enc && enc->t == PO_NAME)
			ename = enc->u.p;
		else if (enc && enc->t == PO_DICT) {
			ename = pd_name(d, enc, "BaseEncoding");
			diff = pd_get(d, enc, "Differences");
		}
		if (ename && !strcmp(ename, "WinAnsiEncoding"))
			base_tab = enc_win;
		else if (ename && !strcmp(ename, "MacRomanEncoding"))
			base_tab = enc_mac;
		else if (ename && !strcmp(ename, "StandardEncoding"))
			base_tab = enc_std;
		else if (!ename || strcmp(ename, "MacExpertEncoding"))
			base_tab = (sub && !strcmp(sub, "TrueType")) ? enc_win : (istype3 ? NULL : enc_std);
		if (base_tab)
			for (c = 0; c < 256; c++)
				f->uni[c] = base_tab[c];
		if (desc && !ename && sub && (!strcmp(sub, "Type1") || !strcmp(sub, "MMType1"))) {
			pause_point(d);
			if (!t1_builtin(d, desc, f->uni))
				cff_builtin(d, desc, f->uni);
		}
		if (diff && diff->t == PO_ARR) {
			long i, code = 0;

			for (i = 0; i < diff->a; i++) {
				POBJ *e = pdf_res(d, &diff->u.v[i]);

				if (!e)
					continue;
				if (e->t == PO_INT)
					code = e->a;
				else if (e->t == PO_NAME && code >= 0 && code < 256)
					f->uni[code++] = (uint16_t)glyph_unicode(e->u.p);
			}
		}
		/* widths */
		sw = std_widths(base, &courier);
		for (c = 0; c < 256; c++) {
			int w = miss;

			if (wa && wa->t == PO_ARR && c >= first && c - first < wa->a)
				w = w_of(d, &wa->u.v[c - first]);
			else if (!wa || wa->t != PO_ARR) {
				if (courier)
					w = 600;
				else if (sw && c >= 32)
					w = sw[c - 32];
				else if (!miss)
					w = 500;
			}
			if (istype3) {
				POBJ *fm = pd_get(d, fd, "FontMatrix");
				fx m0 = fm && fm->t == PO_ARR && pa_at(fm, 0) ? pa_at(fm, 0)->a : 65;

				if (pa_at(fm, 0) && pa_at(fm, 0)->t == PO_INT)
					m0 = pa_at(fm, 0)->a << 16;
				f->wfx[c] = (m0 >= 60 && m0 <= 72) ? w2fx(w) : fxmul(FX_INT(w), m0);
			} else
				f->wfx[c] = w2fx(w);
		}
		f->dwfx = w2fx(miss ? miss : 500);
	}
	if (desc)
		note_prog(d, t, desc, base, f);
	else if (!f->composite) {
		/* no descriptor: one of the standard fonts */
		POBJ none;

		memset(&none, 0, sizeof none);
		note_prog(d, t, &none, base, f);
	}
	pause_point(d);			/* a font's parts are read between the stop hook's calls */
	tu = pd_get(d, fd, "ToUnicode");
	if (tu && tu->t == PO_DICT && tu->flag) {
		SBUF b;
		int whole;
		long onum = pd_getraw(fd, "ToUnicode") && pd_getraw(fd, "ToUnicode")->t == PO_REF ? pd_getraw(fd, "ToUnicode")->a : -1;

		/* (all of it, a piece at a time between the stop hook) */
		if (pdf_stream_head(d, tu, onum, 8L << 20, &b, &whole) == 0) {
			tounicode_parse(d, f, (const unsigned char *)b.s, b.len);
			if (!f->composite)
				fill_simple_from_tu(f);
			sb_free(&b);
		}
	}
	if (f->composite) {
		if (!f->nbytes)
			f->nbytes = 2;
		f->has_map = f->ntu > 0 || f->ucs2;
	} else {
		for (c = 0; c < 256; c++)
			if (f->uni[c] || f->mpos[c])
				f->has_map = 1;
	}
	return f;
}

static PFONT *font_get(PDOC *d, POBJ *fref, PTEXT *t)
{
	FCACHE *fc = d->fc;
	POBJ *fd;
	long obj = fref && fref->t == PO_REF ? fref->a : -1, i, victim = 0;
	PFONT *f;

	if (!fc) {
		fc = d->fc = pdf_malloc(d, (long)sizeof *fc);
		if (!fc)
			return NULL;
		memset(fc, 0, sizeof *fc);
	}
	fc->clock++;
	if (obj >= 0)
		for (i = 0; i < FC_MAX; i++)
			if (fc->e[i].f && fc->e[i].obj == obj) {
				fc->e[i].age = fc->clock;
				/* a font from an earlier page is still in this page's font list */
				return fc->e[i].f;
			}
	pause_point(d);
	fd = pdf_res(d, fref);
	if (!fd || fd->t != PO_DICT)
		return NULL;
	pause_point(d);
	f = font_load(d, fd, obj, t);
	if (!f)
		return NULL;
	for (i = 0; i < FC_MAX; i++) {
		if (!fc->e[i].f) {
			victim = i;
			break;
		}
		if (fc->e[i].age < fc->e[victim].age)
			victim = i;
	}
	font_free(d, fc->e[victim].f);
	fc->e[victim].f = f;
	fc->e[victim].obj = obj;
	fc->e[victim].age = fc->clock;
	return f;
}

int pdf_helv_width(int code)
{
	return code >= 32 && code < 256 ? stdw_helv[code - 32] : 0;
}

void pdf_text_cleanup(PDOC *d)
{
	FCACHE *fc = d->fc;
	int i;

	if (!fc)
		return;
	for (i = 0; i < FC_MAX; i++)
		font_free(d, fc->e[i].f);
	pdf_free(d, fc, (long)sizeof *fc);
	d->fc = NULL;
}

/* ------------------------------------------------------------------ the interpreter */
typedef struct {
	fx ctm[6];
	fx tc, tw, th, tl, fs, rise;
	PFONT *font;
	int mode;
} GS;

typedef struct {
	fx x, y, fs, dx, dy;		/* origin, size, advance (device units) */
	long off;			/* in the chunk text */
	short len;
	char sp;			/* the chunk starts with a space */
	char plain;			/* its text is ASCII: the next may join it */
} CH;
#define CH_MAX 30000			/* a chunk's bytes at most (len) */

enum { K_NUM, K_STR, K_NAME, K_ARR0, K_ARR1, K_DICT };
typedef struct {
	int k;
	fx f;
	const unsigned char *s;
	long n;
} OP;
static fx num_val(OP *o);	/* a number operand's value (read when wanted) */

typedef struct {
	PDOC *d;
	PTEXT *t;
	GS gs, stack[32];
	int sp;
	fx tm[6], tlm[6];
	CH *ch;
	long nch, capch;
	SBUF txt;
	OP *ops;			/* the operands of the operator being read: a TJ array can have thousands */
	long nops, opcap;
	int all_ops;			/* every operator's operands made (a renderer's); else only
					   those the text reads (run_content_in) */
	int in_bt;			/* in a text object (BT ... ET) */
	unsigned char *sb;
	long sbcap, sblen;
	/* the page's content a piece at a time (run_content_pieces): the
	   window the interpreter reads, topped up between tokens; NULL while
	   a whole buffer is run (a form's) */
	struct CPIECES *src;
	unsigned char *win;
	long wcap;
	int src_end;
	int depth;
	POBJ *res;
	long forms_run;
	fx rm[4];			/* the chunk being shown: origin x, y and the advance direction */
	fx rfs;
	fx gm[4];			/* with a renderer: a glyph's em unit to the page (PDRAW.glyph) */
	fx run0;			/* the advance before the string being shown (a TJ's earlier items) */
	fx page_area;			/* the page's width x height / 65536 (points squared, 16.16) */
	int glyphwise;			/* the page set a glyph at a time with its spaces as characters (emit_line) */
	/* show_simple's advances: a code's advance in text space for the font
	   and the state they were made for, each made the first time the code
	   is shown (13 Nov 2026, the speed phase) */
	fx *adv;			/* 256: the font cache's (FCACHE.adv; the reader's stack is small) */
	unsigned char adv_has[32];	/* the codes made */
	long adv_font;			/* the font's serial; 0: none yet */
	fx adv_fs, adv_tc, adv_tw, adv_th;
} IX;

static const fx IDENT[6] = { FX_ONE, 0, 0, FX_ONE, 0, 0 };

/* six numbers copied as six longs (libc's memcpy goes a byte at a time) */
static void mat_copy(fx *o, const fx *a)
{
	o[0] = a[0];
	o[1] = a[1];
	o[2] = a[2];
	o[3] = a[3];
	o[4] = a[4];
	o[5] = a[5];
}

static void mat_mul(const fx *a, const fx *b, fx *o)		/* o = a x b */
{
	fx r[6];

	if (b[1] == 0 && b[2] == 0) {
		/* b scales and moves, without turning: a page's matrices nearly always */
		r[0] = fxmul(a[0], b[0]);
		r[1] = fxmul(a[1], b[3]);
		r[2] = fxmul(a[2], b[0]);
		r[3] = fxmul(a[3], b[3]);
		r[4] = fxmul(a[4], b[0]) + b[4];
		r[5] = fxmul(a[5], b[3]) + b[5];
	} else {
		r[0] = fxmul(a[0], b[0]) + fxmul(a[1], b[2]);
		r[1] = fxmul(a[0], b[1]) + fxmul(a[1], b[3]);
		r[2] = fxmul(a[2], b[0]) + fxmul(a[3], b[2]);
		r[3] = fxmul(a[2], b[1]) + fxmul(a[3], b[3]);
		r[4] = fxmul(a[4], b[0]) + fxmul(a[5], b[2]) + b[4];
		r[5] = fxmul(a[4], b[1]) + fxmul(a[5], b[3]) + b[5];
	}
	mat_copy(o, r);
}

static fx fxabs(fx v) { return v < 0 ? -v : v; }

static int utf8_put(SBUF *b, unsigned u)
{
	char t[4];
	int n;

	if (u < 0x80 && b->len + 2 <= b->cap) {
		/* most letters: one byte where there is room, no call */
		b->s[b->len++] = (char)u;
		b->s[b->len] = 0;
		return 1;
	}
	if (u < 0x80) { t[0] = (char)u; n = 1; }
	else if (u < 0x800) { t[0] = (char)(0xC0 | u >> 6); t[1] = (char)(0x80 | (u & 63)); n = 2; }
	else if (u < 0x10000) { t[0] = (char)(0xE0 | u >> 12); t[1] = (char)(0x80 | ((u >> 6) & 63)); t[2] = (char)(0x80 | (u & 63)); n = 3; }
	else { t[0] = (char)(0xF0 | u >> 18); t[1] = (char)(0x80 | ((u >> 12) & 63)); t[2] = (char)(0x80 | ((u >> 6) & 63)); t[3] = (char)(0x80 | (u & 63)); n = 4; }
	return sb_add(b, t, n);
}

static void put_units(SBUF *b, const uint16_t *u, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		unsigned c = u[i];

		if (c >= 0xD800 && c < 0xDC00 && i + 1 < n && u[i + 1] >= 0xDC00 && u[i + 1] < 0xE000) {
			c = 0x10000 + ((c - 0xD800) << 10) + (unsigned)(u[i + 1] - 0xDC00);
			i++;
		}
		if (c == 0xA0)
			c = ' ';
		if (c >= 0xFB00 && c <= 0xFB06) {		/* ligatures: the letters, for search and copy */
			static const char *const lig[7] = { "ff", "fi", "fl", "ffi", "ffl", "st", "st" };

			sb_adds(b, lig[c - 0xFB00]);
			continue;
		}
		utf8_put(b, c);
	}
}

static int wr_lookup(const PFONT *f, uint32_t code)
{
	long lo = 0, hi = f->nwr - 1;

	while (lo <= hi) {
		long mid = (lo + hi) / 2;

		if (code < f->wr[mid].lo)
			hi = mid - 1;
		else if (code > f->wr[mid].hi)
			lo = mid + 1;
		else
			return f->wr[mid].w;
	}
	return -1;
}

/* show the bytes s[0..n) in the current font; tj: the adjustments of a TJ array are applied
   by the caller between calls. Returns the advance in text space. */
/* the caller's hook (PDOC.stop), every few operators and glyphs: it may
   hand the machine back for a while (EPDF's reading on its own stack), or
   say stop; then the page is given up as soon as the interpreter sees it */
static void pause_point(PDOC *d)
{
	if (d->stop && !d->stopping && d->stop(d->stop_ctx))
		d->stopping = 1;
}

/* show_run for the common case: a simple font of one byte a code, no
   glyphs recorded and no renderer. The same work as show_run's, in the same
   order and with the same arithmetic (each code's advance made by
   show_run's formula once, then looked up), so the same text and advance;
   a letter of one ASCII character goes straight into the text.
   tools/textgold.py checks the corpus against the code before it. */
static fx show_simple(IX *x, const unsigned char *s, long n, long *glyphs)
{
	GS *g = &x->gs;
	PFONT *f = g->font;
	SBUF *b = &x->txt;
	fx adv = 0, *tab = x->adv;
	long i;

	if (x->adv_font != f->serial || x->adv_fs != g->fs || x->adv_tc != g->tc || x->adv_tw != g->tw ||
	    x->adv_th != g->th) {
		memset(x->adv_has, 0, sizeof x->adv_has);
		x->adv_font = f->serial;
		x->adv_fs = g->fs;
		x->adv_tc = g->tc;
		x->adv_tw = g->tw;
		x->adv_th = g->th;
	}
	for (i = 0; i < n; i++) {
		unsigned code = s[i];
		unsigned bit = 1u << (code & 7);

		if ((++x->d->pause_tick & 31) == 0)
			pause_point(x->d);
		(*glyphs)++;
		if (f->mpos[code]) {
			const uint16_t *src = f->pool + f->mpos[code] - 1;
			uint16_t u[8];
			int m = src[0] > 8 ? 8 : src[0];

			memcpy(u, src + 1, (size_t)m * 2);
			if (m)
				put_units(b, u, m);
			else
				x->t->unmapped++;
		} else {
			unsigned c = f->uni[code];

			if (!c)
				x->t->unmapped++;
			else if (c < 0x80 && b->len + 2 <= b->cap)
				b->s[b->len++] = (char)c;	/* (the 0 after it: once, below) */
			else {
				uint16_t u = (uint16_t)c;

				put_units(b, &u, 1);
			}
		}
		if (!(x->adv_has[code >> 3] & bit)) {
			fx a = fxmul(f->wfx[code], g->fs) + g->tc;

			if (code == 32)
				a += g->tw;
			if (g->th != FX_ONE)
				a = fxmul(a, g->th);
			tab[code] = a;
			x->adv_has[code >> 3] |= (unsigned char)bit;
		}
		adv += tab[code];
	}
	if (b->s)
		b->s[b->len] = 0;
	return adv;
}

static fx show_run(IX *x, const unsigned char *s, long n, long *glyphs)
{
	GS *g = &x->gs;
	PFONT *f = g->font;
	fx adv = 0;
	long i = 0;
	uint16_t u[8];

	if (f && !f->composite && f->nbytes == 1 && !x->t->rec
#ifdef PDF_DRAW
	    && !(x->t->draw && x->t->draw->glyph)
#endif
	    ) {
		if (!x->adv && x->d->fc)
			x->adv = ((FCACHE *)x->d->fc)->adv;
		if (x->adv)
			return show_simple(x, s, n, glyphs);
	}

	while (i < n) {
		if ((++x->d->pause_tick & 31) == 0)
			pause_point(x->d);
		uint32_t code;
		fx w;
		int m;

		if (!f) {
			code = s[i++];
			w = w2fx(500);
			m = code >= 32 && code < 127 ? 1 : 0;
			u[0] = (uint16_t)code;
		} else {
			if (f->nbytes == 2 && i + 1 < n) {
				code = (uint32_t)(s[i] << 8 | s[i + 1]);
				i += 2;
			} else if (f->nbytes >= 3 && i + 2 < n) {
				code = be_n(s + i, f->nbytes > 4 ? 4 : f->nbytes);
				i += f->nbytes;
			} else
				code = s[i++];
			if (f->composite) {
				int ww = wr_lookup(f, code);

				w = ww >= 0 ? w2fx(ww) : f->dwfx;
				m = tu_lookup(f, code, u);
				if (!m && f->ucs2) {
					u[0] = (uint16_t)code;
					m = 1;
				}
			} else {
				w = f->wfx[code & 255];
				if (f->mpos[code & 255]) {
					const uint16_t *src = f->pool + f->mpos[code & 255] - 1;

					m = src[0] > 8 ? 8 : src[0];
					memcpy(u, src + 1, (size_t)m * 2);
				} else if (f->uni[code & 255]) {
					u[0] = f->uni[code & 255];
					m = 1;
				} else
					m = 0;
			}
		}
		(*glyphs)++;
#ifdef PDF_DRAW
		if (x->t->draw && x->t->draw->glyph) {
			fx gmx[6];

			gmx[0] = x->gm[0];
			gmx[1] = x->gm[1];
			gmx[2] = x->gm[2];
			gmx[3] = x->gm[3];
			gmx[4] = x->rm[0] + fxmul(x->run0 + adv, x->rm[2]);
			gmx[5] = x->rm[1] + fxmul(x->run0 + adv, x->rm[3]);
			x->t->draw->glyph(x->t->draw->u, f ? f->prog : -1, f ? f->pkind : 4, f ? f->base : NULL,
					  f ? f->composite : 0, code, m ? u[0] : 0, gmx, g->mode);
		}
#endif
		if (x->t->rec && m && f) {
			PTEXT *pt = x->t;

			if (pt->ngl == pt->capgl) {
				long nc = pt->capgl ? pt->capgl * 2 : 1024;
				PGLYPH *q = realloc(pt->gl, (size_t)nc * sizeof *q);

				if (q) {
					pt->gl = q;
					pt->capgl = nc;
				}
			}
			if (pt->ngl < pt->capgl) {
				PGLYPH *gp = &pt->gl[pt->ngl++];

				gp->x = x->rm[0] + fxmul(x->run0 + adv, x->rm[2]);
				gp->y = x->rm[1] + fxmul(x->run0 + adv, x->rm[3]);
				gp->size = x->rfs;
				gp->uni = f->composite ? code : u[0];
				gp->comp = f->composite;
				gp->prog = f->prog;
				gp->kind = f->pkind;
				gp->base = f->base;
			}
		}
		if (m)
			put_units(&x->txt, u, m);
		else
			x->t->unmapped++;
		{
			fx a = fxmul(w, g->fs) + g->tc;

			if (code == 32 && (!f || f->nbytes == 1))
				a += g->tw;
			if (g->th != FX_ONE)
				a = fxmul(a, g->th);
			adv += a;
		}
	}
	return adv;
}

static int ch_room(IX *x)
{
	if (x->nch == x->capch) {
		long cap = x->capch ? x->capch * 2 : 256;
		CH *q = pdf_realloc(x->d, x->ch, x->capch * (long)sizeof(CH), cap * (long)sizeof(CH));

		/* refused: by half again (doubling holds the old and the new, three
		   times what was in it, as it grows: a page set a glyph at a time
		   came to 344 KB there on a 1 MB ST). Only then: growing by half
		   copies the glyphs about twice as often */
		if (!q && x->capch >= 256) {
			cap = x->capch + (x->capch >> 1);
			if ((q = pdf_realloc(x->d, x->ch, x->capch * (long)sizeof(CH), cap * (long)sizeof(CH))) != NULL)
				x->d->st.oom--;	/* (taken in the end: no refusal for the page's log) */
		}
		if (!q)
			return 0;
		x->ch = q;
		x->capch = cap;
	}
	return 1;
}

/* a Tj / TJ / ' / ": items from ops[from..to) */
static void show_text(IX *x, int from, int to)
{
	GS *g = &x->gs;
	fx m[6], tadv = 0, org_x, org_y, fsz;
	long start = x->txt.len, glyphs = 0;
	int i;
	CH *c;

	if (from < 0)			/* an operator without its operand (damaged content) */
		from = 0;
	if (to > x->nops)
		to = (int)x->nops;
	if (!ch_room(x))
		return;
	mat_mul(x->tm, g->ctm, m);
	org_x = m[4] + fxmul(g->rise, m[2]);
	org_y = m[5] + fxmul(g->rise, m[3]);
	{
		fx a = fxabs(m[3]), b = fxabs(m[2]);

		fsz = fxmul(g->fs < 0 ? -g->fs : g->fs, a > b ? a : b);
	}
	x->rm[0] = org_x;
	x->rm[1] = org_y;
	x->rm[2] = m[0];
	x->rm[3] = m[1];
	x->rfs = fsz;
#ifdef PDF_DRAW
	if (x->t->draw) {
		fx h = fxmul(g->fs, g->th);

		x->gm[0] = fxmul(h, m[0]);
		x->gm[1] = fxmul(h, m[1]);
		x->gm[2] = fxmul(g->fs, m[2]);
		x->gm[3] = fxmul(g->fs, m[3]);
	}
#endif
	for (i = from; i < to; i++) {
		OP *o = &x->ops[i];

		if (o->k == K_STR) {
			x->run0 = tadv;
			tadv += show_run(x, o->s, o->n, &glyphs);
		}
		else if (o->k == K_NUM) {
			/* thousandths of an em, subtracted; a wide one is a space between words */
			int32_t n1000 = num_val(o) >> 16;
			fx adj = -fxmul(w2fx((int)n1000), g->fs);

			if (g->th != FX_ONE)
				adj = fxmul(adj, g->th);
			if (n1000 <= -180 && x->txt.len > start && x->txt.s[x->txt.len - 1] != ' ')
				utf8_put(&x->txt, ' ');
			tadv += adj;
		}
	}
	x->t->glyphs += glyphs;
	x->d->st.glyphs += glyphs;
	x->d->st.ops_text++;
	if (x->txt.len > start) {
		long len = x->txt.len - start, k;
		int plain = 1;
		fx dx = fxmul(tadv, m[0]), dy = fxmul(tadv, m[1]);

		for (k = start; k < x->txt.len; k++)
			if ((unsigned char)x->txt.s[k] >= 0x80) {
				plain = 0;
				break;
			}
		/* a piece that goes on where the last one ended, on its line in its
		   size, joins it (a book's text set a letter at a time: a chunk a
		   letter took its memory, 0.3); within 1/16 em, where assemble
		   would put no space between them either */
		if (plain && x->nch > 0) {
			c = &x->ch[x->nch - 1];
			if (c->plain && c->y == org_y && c->fs == fsz && !c->dy && !dy && dx >= 0 && c->dx >= 0 &&
			    c->off + c->len == start && c->len + len <= CH_MAX &&
			    fxabs(org_x - (c->x + c->dx)) <= (fsz >> 4)) {
				c->dx = org_x + dx - c->x;
				c->len = (short)(c->len + len);
				goto moved;
			}
		}
		if (len > CH_MAX) {
			len = CH_MAX;		/* the rest isn't shown */
			x->txt.len = start + len;
		}
		c = &x->ch[x->nch++];
		c->x = org_x;
		c->y = org_y;
		c->fs = fsz;
		c->dx = dx;
		c->dy = dy;
		c->off = start;
		c->len = (short)len;
		c->sp = x->txt.s[start] == ' ';
		c->plain = (char)plain;
		x->d->st.chunks++;
	}
moved:
	/* the text matrix moves on by the advance */
	x->tm[4] += fxmul(tadv, x->tm[0]);
	x->tm[5] += fxmul(tadv, x->tm[1]);
}

static void tm_translate(IX *x, fx tx, fx ty)
{
	fx *l = x->tlm;

	if (l[1] == 0 && l[2] == 0) {
		/* [1 0 0 1 tx ty] x tlm with tlm unturned, a page's text nearly
		   always (Td a word): mat_mul's own sums for it, the rest of tlm as
		   it is (13 Nov 2026, the hub's item 2) */
		l[4] = fxmul(tx, l[0]) + l[4];
		l[5] = fxmul(ty, l[3]) + l[5];
	} else {
		fx t[6] = { FX_ONE, 0, 0, FX_ONE, tx, ty };

		mat_mul(t, l, l);
	}
	mat_copy(x->tm, l);
}

static int op_grow(IX *x);
static int run_content(IX *x, const unsigned char *s, long n);
static int run_content_in(IX *x, const unsigned char *s0, long n0, const unsigned char *p, const unsigned char *e, char *nm);

static void do_xobject(IX *x, const char *name)
{
	PDOC *d = x->d;
	POBJ *xo = pd_get(d, x->res, "XObject"), *o;
	const char *sub;

	xo = xo && xo->t == PO_DICT ? pd_get(d, xo, name) : NULL;
	o = xo;
	if (!o || o->t != PO_DICT)
		return;
	{
		POBJ *r = pd_getraw(pd_get(d, x->res, "XObject"), name);
		long onum = r && r->t == PO_REF ? r->a : -1;

		sub = pd_name(d, o, "Subtype");
		if (!sub)
			return;
		if (!strcmp(sub, "Image")) {
			long w = pd_int(d, o, "Width", 0), h = pd_int(d, o, "Height", 0), len = pdf_stream_len(d, o);
			POBJ *fl = pd_get(d, o, "Filter");
			const char *fn = NULL;

			if (fl && fl->t == PO_ARR && fl->a)
				fl = pdf_res(d, &fl->u.v[fl->a - 1]);
			if (fl && fl->t == PO_NAME)
				fn = fl->u.p;
			if ((x->t->rec || x->t->pics) && x->t->nimg < 64) {
				PIMG *im = &x->t->img[x->t->nimg++];
				const fx *c = x->gs.ctm;
				fx y0 = c[5], y1 = c[5] + c[1], y2 = c[5] + c[3], y3 = c[5] + c[1] + c[3];

				im->obj = onum;
				im->w = w;
				im->h = h;
				im->dw = fxabs(c[0]) + fxabs(c[1]);
				im->dh = fxabs(c[2]) + fxabs(c[3]);
				/* the image's first row is at the unit square's top: a
				   placing that turns the square over shows it upside
				   down or mirrored (EDOS_01.PDF's menu, 2 Nov 2026); a
				   quarter turn isn't followed */
				im->flip = 0;
				if (!c[1] && !c[2]) {
					if (c[3] < 0)
						im->flip |= PIMG_V;
					if (c[0] < 0)
						im->flip |= PIMG_H;
				}
				{
					fx x0 = c[4], x1 = c[4] + c[0], x2 = c[4] + c[2], x3 = c[4] + c[0] + c[2];

					im->left = x0 < x1 ? x0 : x1;
					if (x2 < im->left)
						im->left = x2;
					if (x3 < im->left)
						im->left = x3;
				}
				im->cut = 0;
				/* the unit square's highest corner */
				im->top = y0 > y1 ? y0 : y1;
				if (y2 > im->top)
					im->top = y2;
				if (y3 > im->top)
					im->top = y3;
				strncpy(im->filter, fn ? fn : "", sizeof im->filter - 1);
				im->filter[sizeof im->filter - 1] = 0;
			}
#ifdef PDF_DRAW
			if (x->t->draw && x->t->draw->image)
				x->t->draw->image(x->t->draw->u, onum, x->gs.ctm);
#endif
			d->st.images++;
			d->st.image_px += w * h;
			d->st.image_raw += len > 0 ? len : 0;
			if (!fn) d->st.img_other++;
			else if (!strcmp(fn, "DCTDecode")) d->st.img_dct++;
			else if (!strcmp(fn, "JPXDecode")) d->st.img_jpx++;
			else if (!strcmp(fn, "CCITTFaxDecode")) d->st.img_ccitt++;
			else if (!strcmp(fn, "JBIG2Decode")) d->st.img_jbig2++;
			else if (!strcmp(fn, "FlateDecode")) d->st.img_flate++;
			else d->st.img_other++;
			return;
		}
		if (!strcmp(sub, "Form") && x->depth < 8 && x->forms_run < 400) {
			SBUF b;
			const char *term = NULL;
			POBJ *fm = pd_get(d, o, "Matrix"), *fres = pd_get(d, o, "Resources");
			GS save = x->gs;
			POBJ *save_res = x->res;
			fx m[6];
			int k;

			x->forms_run++;
			d->st.forms++;
			memcpy(m, IDENT, sizeof m);
			if (fm && fm->t == PO_ARR && fm->a >= 6)
				for (k = 0; k < 6; k++) {
					POBJ *e = pa_at(fm, k);

					m[k] = e->t == PO_INT ? e->a << 16 : e->a;
				}
			mat_mul(m, x->gs.ctm, x->gs.ctm);
			if (fres && fres->t == PO_DICT)
				x->res = fres;
			if (pdf_stream(d, o, onum, &b, &term, 16L << 20) == 0 && !term) {
				x->depth++;
#ifdef PDF_DRAW
				if (x->t->draw)
					x->t->draw->op(x->t->draw->u, "q", NULL, 0, NULL, x->gs.ctm, x->res);
#endif
				run_content(x, (const unsigned char *)b.s, b.len);
#ifdef PDF_DRAW
				if (x->t->draw)
					x->t->draw->op(x->t->draw->u, "Q", NULL, 0, NULL, x->gs.ctm, save_res);
#endif
				x->depth--;
			}
			sb_free(&b);
			x->gs = save;
			x->res = save_res;
		}
	}
}

static int op_grow(IX *x)
{
	long nc = x->opcap ? x->opcap * 2 : 256;
	OP *q;

	if (nc > (1L << 20))
		return 0;
	q = pdf_realloc(x->d, x->ops, x->opcap * (long)sizeof(OP), nc * (long)sizeof(OP));
	if (!q)
		return 0;
	x->ops = q;
	x->opcap = nc;
	return 1;
}

/*
 * A number operand is kept as its bytes in the window (s, n) until an
 * operator wants its value: a map's content is mostly path operators,
 * whose numbers the text never needs (lexing each was 40 % of a map's
 * page on an ST, 3 Nov 2026). Its value then, as lex_number_at gives it;
 * s is NULL once it is read.
 */
static fx num_val(OP *o)
{
	if (o->s) {
		const unsigned char *t = o->s;
		fx f;
		int32_t iv;
		int real, ok;

		lex_number_at(t, t + o->n, &f, &iv, &real, &ok);
		if (!ok)
			f = 0;
		else if (!real && iv > 32767)
			f = FX_INT(32767);
		o->f = f;
		o->s = NULL;
	}
	return o->f;
}

/* the numbers still in the window read, before it moves (refill) */
static void nums_out(IX *x)
{
	long i;

	for (i = 0; i < x->nops; i++)
		if (x->ops[i].k == K_NUM)
			num_val(&x->ops[i]);
}

/* operand access */
static fx opnum(IX *x, int i)
{
	return i >= 0 && i < x->nops && x->ops[i].k == K_NUM ? num_val(&x->ops[i]) : 0;
}

static void set_font(IX *x, const char *name, fx size)
{
	PDOC *d = x->d;
	POBJ *fl = pd_get(d, x->res, "Font"), *r;

	x->gs.fs = size;
	x->gs.font = NULL;
	if (!fl || fl->t != PO_DICT)
		return;
	r = pd_getraw(fl, name);
	if (!r)
		return;
	x->gs.font = font_get(d, r, x->t);
	if (x->gs.font && !x->gs.font->has_map)
		x->t->fonts_nomap++;
}

#define OPK(a, b) (((a) << 8) | (b))

/* a string's byte into the scratch, while it has room (lim) */
#define PUT(v) do { if (len < lim) o[len++] = (unsigned char)(v); } while (0)

static int run_content(IX *x, const unsigned char *s, long n)
{
	const unsigned char *p = s, *e = s + n;
	PDOC *d = x->d;
	char nm[64];
	unsigned char *save_sb = x->sb;
	long save_cap = x->sbcap, save_len = x->sblen;
	int rc;

	/* the strings and names of one operator fit in the stream: one buffer for this stream, so that
	   an operand never moves (a form run from here has its own) */
	x->sb = pdf_malloc(d, n + 64);
	x->sbcap = n + 64;
	x->sblen = 0;
	if (!x->sb) {
		x->sb = save_sb;
		x->sbcap = save_cap;
		return -1;
	}
	x->nops = 0;
	{
		/* a form run from a page read a piece at a time: its own buffer, the page's window kept */
		struct CPIECES *save_src = x->src;
		int save_end = x->src_end;

		x->src = NULL;
		rc = run_content_in(x, s, n, p, e, nm);
		x->src = save_src;
		x->src_end = save_end;
	}
	pdf_free(d, x->sb, x->sbcap);
	x->sb = save_sb;
	x->sbcap = save_cap;
	x->sblen = save_len;
	x->nops = 0;
	return rc;
}

/*
 * A page's content a piece at a time (phase 0.3: a map's is MBs unpacked,
 * more than a 1 MB ST has): its streams one after another, each through
 * pdf_stream_open where it can be (stored, or Flate), else decoded whole
 * as before; a line end between two. The interpreter reads a window of
 * CWIN bytes, topped up between tokens when less than CMARGIN is left (so
 * a token up to CMARGIN long never straddles); its operands are copied out
 * of it as they are read, so the window may move under them.
 */
#define CWIN 16384L
#define CMARGIN 4096L
#define REFILL_PIECE 2048L		/* the content unpacked between stop hooks */
#define CSENT 'x'			/* after the window's bytes: no class (ccls), ends run_of's runs */

typedef struct CPIECES {
	PDOC *d;
	POBJ *cont, *ref;		/* Contents: a stream (ref: its reference) or an array of them */
	long k, n;			/* the next piece, how many */
	PCSRC *s;			/* the piece read a piece at a time */
	SBUF whole;			/* or decoded whole */
	long wpos;
	int nl;				/* a line end owed after a piece */
	long slen;			/* the piece's stored bytes, when it was opened */
} CPIECES;

/* how far through the page's content: the pieces done and this one's
   stored bytes read (PDOC.page_prog, the progress row's) */
static void pieces_prog(CPIECES *c)
{
	long in = 0, a = 0, b = 0;

	if (c->s && c->slen > 0) {
		a = c->slen - pdf_stream_left(c->s);
		b = c->slen;
	} else if (c->whole.s && c->whole.len > 0) {
		a = c->wpos;
		b = c->whole.len;
	}
	while (b > (1L << 20))		/* (1000 x the bytes in a long) */
		a >>= 1, b >>= 1;
	if (b > 0)
		in = a * 1000L / b;
	if (in > 1000)
		in = 1000;
	if (c->n > 0)
		c->d->page_prog = (int)(((c->k > 0 ? c->k - 1 : 0) * 1000L + in) / c->n);
}

static long pieces_read(CPIECES *c, unsigned char *out, long n)
{
	PDOC *d = c->d;
	long got = 0;

	while (got < n) {
		if (c->s) {
			long k = pdf_stream_read(c->s, out + got, n - got);

			if (k > 0) {
				got += k;
				continue;
			}
			pdf_stream_close(c->s);
			c->s = NULL;
			c->nl = 1;
			continue;
		}
		if (c->whole.s) {
			long k = c->whole.len - c->wpos;

			if (k > n - got)
				k = n - got;
			memcpy(out + got, c->whole.s + c->wpos, (size_t)k);
			got += k;
			c->wpos += k;
			if (c->wpos >= c->whole.len) {
				sb_free(&c->whole);
				memset(&c->whole, 0, sizeof c->whole);
				c->nl = 1;
			}
			continue;
		}
		if (c->nl) {
			out[got++] = '\n';
			c->nl = 0;
			continue;
		}
		if (c->k >= c->n)
			break;
		{
			POBJ *r = c->cont->t == PO_ARR ? &c->cont->u.v[c->k] : c->ref, *so = c->cont->t == PO_ARR ? pdf_res(d, r) : c->cont;
			long onum = r && r->t == PO_REF ? r->a : -1;
			const char *term = NULL;

			c->k++;
			if (!so || so->t != PO_DICT)
				continue;
			c->s = pdf_stream_open(d, so, onum);
			if (c->s) {
				c->slen = pdf_stream_left(c->s);
				continue;
			}
			c->wpos = 0;
			if (pdf_stream(d, so, onum, &c->whole, &term, 32L << 20) != 0 || !c->whole.len) {
				sb_free(&c->whole);
				memset(&c->whole, 0, sizeof c->whole);
			}
		}
	}
	return got;
}

/* the window topped up: what is left of it from *pp moved to its start */
/* the parity of where the content's next byte comes from, -1 not known */
static int pieces_parity(const CPIECES *c)
{
	if (!c)
		return -1;
	if (c->s)
		return pdf_stream_parity(c->s);
	if (c->whole.s && c->wpos < c->whole.len)
		return (int)((unsigned long)(c->whole.s + c->wpos) & 1);
	return -1;
}

static void refill(IX *x, const unsigned char **pp, const unsigned char **pe)
{
	long keep = *pe - *pp, got, cap;
	unsigned char *w = x->win;
	int par = pieces_parity(x->src);

	nums_out(x);
	/* the window from its first or second byte, so the bytes to come land
	   at the parity they come from: the unpacked content copied a long at
	   a time, not a byte (1.2 MB of a map's page, 3 s on an ST) */
	if (par >= 0 && ((par ^ (int)keep) & 1))
		w++;
	cap = x->wcap - (w - x->win);
	if (keep > 0 && *pp != w)
		memmove(w, *pp, (size_t)keep);
	/* 2 KB at a time, the stop hook between: unpacking 12 KB at once was
	   a stretch of 400 ms on an ST */
	got = 0;
	while (keep + got < cap) {
		long k = cap - keep - got, r;

		if (k > REFILL_PIECE)
			k = REFILL_PIECE;
		if (got)
			pause_point(x->d);
		r = pieces_read(x->src, w + keep + got, k);
		pieces_prog(x->src);
		if (r <= 0) {
			if (!got)
				x->src_end = 1;
			break;
		}
		got += r;
		if (x->d->stopping)
			break;
	}
	*pp = w;
	*pe = w + keep + got;
	w[keep + got] = CSENT;		/* run_of's sentinel (the window has a byte for it) */
}

static int run_content_pieces(IX *x, POBJ *cont, POBJ *ref)
{
	PDOC *d = x->d;
	CPIECES c;
	char nm[64];
	int rc = -1;
	long w;

	memset(&c, 0, sizeof c);
	c.d = d;
	c.cont = cont;
	c.ref = ref;
	c.n = cont->t == PO_ARR ? cont->a : 1;
	/* the window and the scratch: 16 KB each, or 4 KB where memory is short */
	for (w = CWIN; w >= CWIN / 4; w /= 4) {
		x->win = pdf_malloc(d, w + 1);	/* (+ 1: the sentinel after the bytes) */
		x->sb = pdf_malloc(d, w + 64);
		if (x->win && x->sb)
			break;
		pdf_free(d, x->win, w + 1);
		pdf_free(d, x->sb, w + 64);
		x->win = x->sb = NULL;
	}
	x->wcap = w;
	x->sbcap = w + 64;
	x->sblen = 0;
	x->nops = 0;
	if (x->win && x->sb) {
		x->src = &c;
		x->src_end = 0;
		pause_point(d);
		rc = run_content_in(x, x->win, 0, x->win, x->win, nm);
		x->src = NULL;
	} else
		x->t->failed = 1;	/* no memory to read it in: the page isn't read, not read empty */
	if (c.s)
		pdf_stream_close(c.s);
	sb_free(&c.whole);
	pdf_free(d, x->win, w + 1);
	pdf_free(d, x->sb, w + 64);
	x->win = x->sb = NULL;
	x->sbcap = x->sblen = 0;
	x->nops = 0;
	return rc;
}

/* the content's characters by class, a byte each: one look-up where the
   tests were up to a dozen compares a character (white space: is_white_c's
   six; an operator's end: twelve) */
#define CC_WHITE 1			/* is_white_c */
#define CC_NUM 2			/* 0-9 . + -: a number's run */
#define CC_OPEND 4			/* ends an operator: white, / [ ] ( < % */
#define CC_PATH 8			/* a path operator of one letter: m l c v y h */
#define CC_PAINT 16			/* a painting one: S s f F B b n */
static unsigned char ccls[256];
/* the same, one class a table (nonzero in it): what run_of looks up */
static unsigned char cwhite[256], cnum[256], cwn[256];

static void ccls_init(void)
{
	int c;

	for (c = 0; c < 256; c++) {
		unsigned char k = 0;

		if (is_white_c(c))
			k |= CC_WHITE | CC_OPEND;
		if ((c >= '0' && c <= '9') || c == '.' || c == '+' || c == '-')
			k |= CC_NUM;
		if (c == '/' || c == '[' || c == ']' || c == '(' || c == '<' || c == '%')
			k |= CC_OPEND;
		if (c && strchr("mlcvyh", c))
			k |= CC_PATH;
		if (c && strchr("SsfFBbn", c))
			k |= CC_PAINT;
		ccls[c] = k;
		cwhite[c] = (k & CC_WHITE) != 0;
		cnum[c] = (k & CC_NUM) != 0;
		cwn[c] = (k & (CC_WHITE | CC_NUM)) != 0;
	}
}

/* past the run of bytes from q that tab has nonzero, which a byte of none
   ends (the window's sentinel, CSENT, at the latest): 32 cycles a byte on
   a 68000, where the C's loop with its bound test was some 48 */
static inline const unsigned char *run_of(const unsigned char *q, const unsigned char *tab)
{
#if defined(__mc68000__) || defined(__m68k__)
	__asm__("moveq #0,%%d0\n"
		"1:\tmove.b (%0)+,%%d0\n\t"
		"tst.b 0(%1,%%d0.w)\n\t"
		"bne.s 1b\n\t"
		"subq.l #1,%0"
		: "+a"(q)
		: "a"(tab)
		: "d0", "cc", "memory");
	return q;
#else
	while (tab[*q])
		q++;
	return q;
#endif
}

/*
 * The numbers in [a, b) (with white space between: b is at the token after
 * them) made operands, kept as their bytes (num_val reads one when wanted).
 * run_content_in passes over a run of numbers and white space in one scan
 * (run_of) and makes its operands only when what follows reads them: a
 * map's content is mostly path operators ("12.5 300.25 l"), whose numbers
 * the text never needs; an operand made for each number was a third of a
 * map's page on an ST (3 Nov 2026).
 */
#ifdef PDF_DRAW
/* an operator handed to the renderer (PDRAW.op), but the text's own */
static void draw_op(IX *x, const unsigned char *op, int len, unsigned code)
{
	fx num[64];
	const char *name = NULL;
	char nm[4];
	int n = 0;
	long i;

	switch (code) {
	case OPK('B', 'T'): case OPK('E', 'T'): case OPK('T', '*'): case OPK('T', 'j'): case OPK('T', 'J'):
	case OPK(0, '\''): case OPK(0, '"'): case OPK('T', 'f'): case OPK('T', 'c'): case OPK('T', 'w'):
	case OPK('T', 'z'): case OPK('T', 'L'): case OPK('T', 'r'): case OPK('T', 's'): case OPK('T', 'd'):
	case OPK('T', 'D'): case OPK('T', 'm'): case OPK('D', 'o'):
		return;
	default:
		break;
	}
	for (i = 0; i < x->nops; i++) {
		OP *o = &x->ops[i];

		if (o->k == K_NUM && n < 64)
			num[n++] = num_val(o);
		else if (o->k == K_NAME)
			name = (const char *)o->s;
	}
	memcpy(nm, op, (size_t)len);
	nm[len] = 0;
	x->t->draw->op(x->t->draw->u, nm, num, n, name, x->gs.ctm, x->res);
}
#endif

static void nums_push(IX *x, const unsigned char *a, const unsigned char *b)
{
	const unsigned char *cls = ccls;

	while (a < b) {
		const unsigned char *q;

		if (cls[*a] & CC_WHITE) {
			a++;
			continue;
		}
		for (q = a + 1; q < b && (cls[*q] & CC_NUM); q++)
			;
		if (x->nops < x->opcap || op_grow(x)) {
			OP *o = &x->ops[x->nops++];

			o->k = K_NUM;	/* (f: num_val sets it when it reads s) */
			o->s = a;
			o->n = q - a;
		}
		a = q;
	}
}

/*
 * Numbers and the white space between them, from p, made operands as they
 * are passed over: in a text object (BT ... ET), where most numbers are
 * read (Td, Tm, TJ's), one scan, not run_of's and then nums_push's (that
 * was 3 % of a text page on an ST). A turn of run_content_in's loop, with
 * its refill, slice and stop tests, was some 200 cycles a token on a
 * 68000. Up to 32 numbers, and not past lim (a token never straddles a
 * refill); where it stopped, *cc the class of the last character looked at
 * (white or a number's: it moved on; else a token of another kind is at
 * the place returned). sent: a byte of no class at e (CSENT, the window's),
 * so the runs need no bound test. Its own copies of p and the operands'
 * count, in registers (run_content_in's p is in memory: refill takes its
 * address).
 */
static inline __attribute__((always_inline)) const unsigned char *
nums_run(IX *x, const unsigned char *p, const unsigned char *e, const unsigned char *lim, int sent, unsigned char *ccp)
{
	const unsigned char *cls = ccls;
	long n = x->nops, cap = x->opcap;
	OP *ops = x->ops;
	int nums = 32;
	unsigned char cc;

	for (;;) {
		cc = cls[*p];
		if (cc & CC_WHITE) {
			if (sent)
				p = run_of(p + 1, cwhite);
			else
				do
					p++;
				while (p < e && (cls[*p] & CC_WHITE));
		} else if (cc & CC_NUM) {
			const unsigned char *q;

			/* its run of 0-9 . + -, read only if an operator wants it */
			if (sent)
				q = run_of(p + 1, cnum);
			else
				for (q = p + 1; q < e && (cls[*q] & CC_NUM); q++)
					;
			if (n >= cap) {
				x->nops = n;
				if (op_grow(x)) {
					ops = x->ops;
					cap = x->opcap;
				}
			}
			if (n < cap) {
				OP *o = &ops[n++];

				o->k = K_NUM;	/* (f: num_val sets it when it reads s) */
				o->s = p;
				o->n = q - p;
			}
			p = q;
			if (--nums == 0)
				break;
		} else
			break;
		if (p >= lim)
			break;
	}
	x->nops = n;
	*ccp = cc;
	return p;
}

static int run_content_in(IX *x, const unsigned char *s0, long n0, const unsigned char *p, const unsigned char *e, char *nm)
{
	PDOC *d = x->d;
	int more = x->src && !x->src_end;	/* a window to top up (x->src_end changes only in refill) */
	unsigned tick = 0;

	(void)s0;
	(void)n0;
	if (!ccls[' '])
		ccls_init();
	for (;;) {
		unsigned char c;
		const unsigned char *run;

		if (more && e - p < CMARGIN) {
			refill(x, &p, &e);
			more = !x->src_end;
		}
		if (p >= e)
			break;

		/* the hook every 8 turns (a turn: an operator, or up to 32
		   numbers): it reads the clock through a trap (Supexec, and
		   every driver hooked on the XBIOS), which every 8 tokens was
		   as much again as the tokens with their numbers left unread */
		if ((++tick & 7) == 0)
			pause_point(d);
		if (d->stopping)
			return -1;

		/* a run of numbers and white space, passed over; then the token
		   after it. Its numbers are made operands (nums_push) only before
		   a token that isn't an operator, or an operator that reads them */
		if (x->in_bt || x->all_ops) {
			unsigned char cc;
			const unsigned char *lim = more ? e - CMARGIN : e;

			p = x->src ? nums_run(x, p, e, lim, 1, &cc) : nums_run(x, p, e, lim, 0, &cc);
			if (cc & (CC_WHITE | CC_NUM))
				continue;	/* (the top: refill, the end, the hook) */
			run = p;
		} else {
			run = p;
			if (x->src)
				p = run_of(p, cwn);	/* (stops at the window's sentinel at e) */
			else
				while (p < e && cwn[*p])
					p++;
			if (p >= e) {
				if (!more) {
					if (x->all_ops)
						nums_push(x, run, e);
					break;
				}
				/* the window's end in the run: its last token may go on
				   in the bytes to come; the rest made operands, then the
				   window topped up from that token */
				while (p > run && !cwhite[p[-1]])
					p--;
				if (p == run && e - run > CMARGIN)
					p = e;		/* (a number longer than the margin: cut) */
				nums_push(x, run, p);
				refill(x, &p, &e);
				more = !x->src_end;
				continue;
			}
		}
		c = *p;
		if (p > run) {
			int reads = 1;

			if (!x->all_ops && !(ccls[c] & CC_OPEND) && c != ')' && c != '>' && c != '{' && c != '}')
				/* an operator: of those the text reads, only cm and
				   the text operators (T., ") take numbers */
				reads = c == 'T' || c == '"' || (c == 'c' && p[1] == 'm' && (ccls[p[2]] & CC_OPEND));
			if (reads)
				nums_push(x, run, p);
		}
		/* a path's operator of one letter, the most of a map's: no
		   operands to read, nothing for the text (the switch below
		   tests its codes one after another) */
		if ((ccls[c] & (CC_PATH | CC_PAINT)) && !x->all_ops && p + 1 < e && (ccls[p[1]] & CC_OPEND)) {
			if (ccls[c] & CC_PATH)
				d->st.ops_path++;
			else
				d->st.ops_paint++;
			p++;
			x->nops = 0;
			x->sblen = 0;
			continue;
		}
		if (c == '%') {
			while (p < e && *p != '\n' && *p != '\r')
				p++;
			continue;
		}
		if (c == '(') {
			long depth = 1;
			unsigned char *o;

			p++;
			o = x->sb + x->sblen;
			{
				long len = 0, lim = x->sbcap - x->sblen - 2;	/* what the buffer has left: a longer string is cut */

				while (p < e) {
					unsigned char ch = *p++;

					if (ch == '\\' && p < e) {
						ch = *p++;
						switch (ch) {
						case 'n': PUT('\n'); break;
						case 'r': PUT('\r'); break;
						case 't': PUT('\t'); break;
						case 'b': PUT('\b'); break;
						case 'f': PUT('\f'); break;
						case '\r': if (p < e && *p == '\n') p++; break;
						case '\n': break;
						default:
							if (ch >= '0' && ch <= '7') {
								int v = ch - '0', k;

								for (k = 0; k < 2 && p < e && *p >= '0' && *p <= '7'; k++)
									v = v * 8 + (*p++ - '0');
								PUT((unsigned char)v);
							} else
								PUT(ch);
						}
					} else if (ch == '(') {
						depth++;
						PUT(ch);
					} else if (ch == ')') {
						if (--depth == 0)
							break;
						PUT(ch);
					} else
						PUT(ch);
				}
				if (x->nops < x->opcap || op_grow(x)) {
					x->ops[x->nops].k = K_STR;
					x->ops[x->nops].s = o;
					x->ops[x->nops].n = len;
					x->nops++;
				}
				x->sblen += len;
			}
			continue;
		}
		if (c == '<') {
			if (p + 1 < e && p[1] == '<') {	/* an inline dict: skip it */
				int lvl = 0;

				while (p + 1 < e) {
					if (p[0] == '<' && p[1] == '<') { lvl++; p += 2; }
					else if (p[0] == '>' && p[1] == '>') { lvl--; p += 2; if (!lvl) break; }
					else p++;
				}
				if (x->nops < x->opcap || op_grow(x)) {
					x->ops[x->nops].k = K_DICT;
					x->nops++;
				}
				continue;
			}
			p++;
			{
				unsigned char *o;
				long len = 0, lim = x->sbcap - x->sblen - 2;
				int hi = -1;

				o = x->sb + x->sblen;
				while (p < e && *p != '>') {
					int ch = *p++, v = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 :
						ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;

					if (v < 0)
						continue;
					if (hi < 0)
						hi = v;
					else {
						PUT((unsigned char)(hi << 4 | v));
						hi = -1;
					}
				}
				if (hi >= 0)
					PUT((unsigned char)(hi << 4));
				if (p < e)
					p++;
				if (x->nops < x->opcap || op_grow(x)) {
					x->ops[x->nops].k = K_STR;
					x->ops[x->nops].s = o;
					x->ops[x->nops].n = len;
					x->nops++;
				}
				x->sblen += len;
			}
			continue;
		}
		if (c == '[') {
			p++;
			if (x->nops < x->opcap || op_grow(x))
				x->ops[x->nops++].k = K_ARR0;
			continue;
		}
		if (c == ']') {
			p++;
			if (x->nops < x->opcap || op_grow(x))
				x->ops[x->nops++].k = K_ARR1;
			continue;
		}
		if (c == '/') {
			const unsigned char *q = ++p;
			int len = 0;

			while (q < e && !is_white_c(*q) && *q != '/' && *q != '[' && *q != ']' && *q != '(' && *q != '<' && *q != '>' && *q != '%')
				q++;
			for (; p < q && len < 63; p++) {
				if (*p == '#' && p + 2 < q + 0 && hexn((const char *)p + 1, 2) >= 0) {
					nm[len++] = (char)hexn((const char *)p + 1, 2);
					p += 2;
				} else
					nm[len++] = (char)*p;
			}
			p = q;
			nm[len] = 0;
			if (x->sblen + len + 1 <= x->sbcap && (x->nops < x->opcap || op_grow(x))) {
				/* keep the name in the scratch (only Tf and Do read it) */
				unsigned char *o;

				o = x->sb + x->sblen;
				memcpy(o, nm, (size_t)len + 1);
				x->sblen += len + 1;
				x->ops[x->nops].k = K_NAME;
				x->ops[x->nops].s = o;
				x->ops[x->nops].n = len;
				x->nops++;
			}
			continue;
		}
		if (c == '>' || c == ')' || c == '{' || c == '}') {
			p++;
			continue;
		}
		/* an operator */
		{
			const unsigned char *q = p;
			unsigned code;
			int len;

			while (q < e && !(ccls[*q] & CC_OPEND))
				q++;
			len = (int)(q - p);
			code = len == 1 ? OPK(0, p[0]) : len == 2 ? OPK(p[0], p[1]) : 0xFFFFFF;
			p = q;
			switch (code) {
			case OPK(0, 'q'):
				if (x->sp < 31) {
					x->stack[x->sp++] = x->gs;
				}
				d->st.ops_other++;
				break;
			case OPK(0, 'Q'):
				if (x->sp > 0)
					x->gs = x->stack[--x->sp];
				d->st.ops_other++;
				break;
			case OPK('c', 'm'):
				if (x->nops >= 6) {
					fx m[6];
					int k;

					for (k = 0; k < 6; k++)
						m[k] = opnum(x, x->nops - 6 + k);
					mat_mul(m, x->gs.ctm, x->gs.ctm);
				}
				d->st.ops_other++;
				break;
			case OPK('B', 'T'):
				mat_copy(x->tm, IDENT);
				mat_copy(x->tlm, IDENT);
				x->in_bt = 1;
				break;
			case OPK('E', 'T'):
				x->in_bt = 0;
				break;
			case OPK('T', 'c'): x->gs.tc = opnum(x, x->nops - 1); break;
			case OPK('T', 'w'): x->gs.tw = opnum(x, x->nops - 1); break;
			case OPK('T', 'z'): {
				fx v = opnum(x, x->nops - 1);

				x->gs.th = v ? v / 100 : FX_ONE;	/* percent -> fraction */
				break;
			}
			case OPK('T', 'L'): x->gs.tl = opnum(x, x->nops - 1); break;
			case OPK('T', 'r'): x->gs.mode = (int)(opnum(x, x->nops - 1) >> 16); break;
			case OPK('T', 's'): x->gs.rise = opnum(x, x->nops - 1); break;
			case OPK('T', 'f'):
				if (x->nops >= 2 && x->ops[x->nops - 2].k == K_NAME)
					set_font(x, (const char *)x->ops[x->nops - 2].s, opnum(x, x->nops - 1));
				break;
			case OPK('T', 'd'):
				tm_translate(x, opnum(x, x->nops - 2), opnum(x, x->nops - 1));
				break;
			case OPK('T', 'D'):
				x->gs.tl = -opnum(x, x->nops - 1);
				tm_translate(x, opnum(x, x->nops - 2), opnum(x, x->nops - 1));
				break;
			case OPK('T', 'm'):
				if (x->nops >= 6) {
					int k;

					for (k = 0; k < 6; k++)
						x->tlm[k] = opnum(x, x->nops - 6 + k);
					mat_copy(x->tm, x->tlm);
				}
				break;
			case OPK('T', '*'):
				tm_translate(x, 0, -x->gs.tl);
				break;
			case OPK('T', 'j'):
				show_text(x, x->nops - 1, x->nops);
				break;
			case OPK(0, '\''):
				tm_translate(x, 0, -x->gs.tl);
				show_text(x, x->nops - 1, x->nops);
				break;
			case OPK(0, '"'):
				if (x->nops >= 3) {
					x->gs.tw = opnum(x, x->nops - 3);
					x->gs.tc = opnum(x, x->nops - 2);
				}
				tm_translate(x, 0, -x->gs.tl);
				show_text(x, x->nops - 1, x->nops);
				break;
			case OPK('T', 'J'): {
				int k;

				for (k = x->nops - 1; k >= 0 && x->ops[k].k != K_ARR0; k--)
					;
				if (k >= 0)
					show_text(x, k + 1, x->nops);
				break;
			}
			case OPK('D', 'o'):
				if (x->nops >= 1 && x->ops[x->nops - 1].k == K_NAME)
					do_xobject(x, (const char *)x->ops[x->nops - 1].s);
				break;
			/* counted only (the measurement's): a path, and its painting
			   (in this switch, not a second test of every operator) */
			case OPK(0, 'm'): case OPK(0, 'l'): case OPK(0, 'c'): case OPK(0, 'v'):
			case OPK(0, 'y'): case OPK(0, 'h'): case OPK('r', 'e'):
				d->st.ops_path++;
				break;
			case OPK(0, 'S'): case OPK(0, 's'): case OPK(0, 'f'): case OPK(0, 'F'):
			case OPK(0, 'B'): case OPK(0, 'b'): case OPK(0, 'n'):
			case OPK('f', '*'): case OPK('B', '*'): case OPK('b', '*'): case OPK('s', 'h'):
				d->st.ops_paint++;
				break;
			default:
				break;
			}
#ifdef PDF_DRAW
			if (x->t->draw && len > 0 && len < 4)
				draw_op(x, p - len, len, code);
#endif
			if (len == 2 && p[-2] == 'B' && p[-1] == 'I') {
				/* inline image: skip to EI (read a piece at a time, the
				   window topped up as the search runs out of it, the last
				   few bytes kept so a mark across the edge is still seen) */
				const unsigned char *q2 = p;
				int found = 0, stage = 0;

				for (;;) {
					while (q2 + 2 < e) {
						if (!stage && is_white_c(q2[0]) && q2[1] == 'I' && q2[2] == 'D' &&
						    (q2 + 3 >= e || is_white_c(q2[3]))) {
							stage = 1;
							q2 += 4;
							continue;
						}
						if (stage && is_white_c(q2[0]) && q2[1] == 'E' && q2[2] == 'I' &&
						    (q2 + 3 >= e || is_white_c(q2[3]))) {
							found = 1;
							break;
						}
						q2++;
					}
					if (found || !x->src || x->src_end)
						break;
					p = q2;
					refill(x, &p, &e);
					more = !x->src_end;
					q2 = p;
					if (e - p < 4)
						break;
				}
				d->st.images++;
				d->st.img_other++;
				p = found ? q2 + 3 : e;
			}
			x->nops = 0;
			x->sblen = 0;
		}
	}
	return 0;
}

/* ------------------------------------------------------------------ the page's text */
void ptext_init(PTEXT *t)
{
	memset(t, 0, sizeof *t);
	sb_init(&t->text);
}

void ptext_free(PTEXT *t)
{
	free(t->gl);
	sb_free(&t->text);
}

/* Chunks come in nearly the order they are read: an insertion sort takes
   them in a pass, and gives up for a merge sort of their numbers when the
   order is far from it (a page of two columns, glyph by glyph: EDOS_01.PDF's
   4,500 chunks). The libc's qsort on an Atari is a heap sort that swaps a
   byte at a time: 11 s of such a page on an ST. Stable, as glibc's is, so
   chunks at one place keep the stream's order on both. */
static void ch_msort(PDOC *d, CH *c, long n, int (*cmp)(const void *, const void *))
{
	/* pointers, not numbers: a number times sizeof(CH) is a multiply
	   a merge step on a 68000 */
	CH **m = pdf_malloc(d, 2 * n * (long)sizeof(CH *)), **a = m, **b;
	long w, i, moved = 0;

	if (!m) {
		qsort(c, (size_t)n, sizeof(CH), cmp);
		return;
	}
	b = a + n;
	for (i = 0; i < n; i++)
		a[i] = c + i;
	for (w = 1; w < n; w <<= 1) {
		CH **t;

		for (i = 0; i < n; i += 2 * w) {
			long mid = i + w < n ? i + w : n, hi = i + 2 * w < n ? i + 2 * w : n;
			CH **p = a + i, **pe = a + mid, **q = a + mid, **qe = a + hi, **k = b + i;

			while (p < pe && q < qe)
				*k++ = cmp(*q, *p) < 0 ? *q++ : *p++;
			while (p < pe)
				*k++ = *p++;
			while (q < qe)
				*k++ = *q++;
			if (!((i + 2 * w) & 1023))
				pause_point(d);
		}
		t = a;
		a = b;
		b = t;
		pause_point(d);
	}
	/* a[k]: the chunk that goes to k; moved there a cycle at a time (each
	   place's number found once, by its pointer) */
	for (i = 0; i < n; i++) {
		long j = i, s;
		CH t;

		if (a[i] == c + i)
			continue;
		t = c[i];
		for (;;) {
			s = a[j] - c;
			a[j] = c + j;
			if (s == i) {
				c[j] = t;
				break;
			}
			c[j] = c[s];
			j = s;
			if (!(++moved & 255))
				pause_point(d);
		}
	}
	pdf_free(d, m, 2 * n * (long)sizeof(CH *));
}

static void ch_sort(PDOC *d, CH *c, long n, int (*cmp)(const void *, const void *))
{
	long i, moves = 0, limit = n * 2 + 64, down = 0;

	/* how far from the order: the pairs out of it */
	for (i = 1; i < n; i++) {
		if (cmp(&c[i - 1], &c[i]) > 0)
			down++;
		if (!(i & 511))
			pause_point(d);
	}
	if (!down)
		return;
	if (down > 16 + n / 64) {
		ch_msort(d, c, n, cmp);
		return;
	}
	for (i = 1; i < n; i++) {
		CH t = c[i];
		long j = i;

		while (j > 0 && cmp(&c[j - 1], &t) > 0) {
			c[j] = c[j - 1];
			j--;
			if (!(++moves & 255))
				pause_point(d);
			if (moves > limit) {
				c[j] = t;
				ch_msort(d, c, n, cmp);
				return;
			}
		}
		c[j] = t;
	}
}

/*
 * The same for a font's ToUnicode ranges and widths (elements of up to
 * SORT_MAX bytes, moved by memcpy): mostly in order already; a CID font's
 * thousands of ranges went to the libc's heap sort (hebrew-idf-guide's
 * first page: 170 ms of a stretch of 320 on an ST).
 */
#define SORT_MAX 32			/* an element's bytes at most */

static void pd_msort(PDOC *d, void *base, long n, long size, int (*cmp)(const void *, const void *))
{
	/* pointers, not numbers: a number times the size is a multiply a
	   merge step on a 68000 */
	char *c = base, **m = pdf_malloc(d, 2 * n * (long)sizeof(char *)), **a = m, **b, *at;
	long w, i, moved = 0;
	unsigned char t[SORT_MAX];

	if (!m) {
		qsort(base, (size_t)n, (size_t)size, cmp);
		return;
	}
	b = a + n;
	for (i = 0, at = c; i < n; i++, at += size)
		a[i] = at;
	for (w = 1; w < n; w <<= 1) {
		char **x;

		for (i = 0; i < n; i += 2 * w) {
			long mid = i + w < n ? i + w : n, hi = i + 2 * w < n ? i + 2 * w : n;
			char **p = a + i, **pe = a + mid, **q = a + mid, **qe = a + hi, **k = b + i;

			while (p < pe && q < qe)
				*k++ = cmp(*q, *p) < 0 ? *q++ : *p++;
			while (p < pe)
				*k++ = *p++;
			while (q < qe)
				*k++ = *q++;
			if (!((i + 2 * w) & 1023))
				pause_point(d);
		}
		x = a;
		a = b;
		b = x;
		pause_point(d);
	}
	/* a[k]: the element that goes to place k; moved there a cycle at a
	   time (each element's place found once, from its pointer) */
	for (i = 0, at = c; i < n; i++, at += size) {
		long j = i;
		char *jp = at, *src;

		if (a[i] == at)
			continue;
		memcpy(t, at, (size_t)size);
		for (;;) {
			src = a[j];
			a[j] = jp;
			if (src == at) {
				memcpy(jp, t, (size_t)size);
				break;
			}
			memcpy(jp, src, (size_t)size);
			j = (src - c) / size;
			jp = src;
			if (!(++moved & 255))
				pause_point(d);
		}
	}
	pdf_free(d, m, 2 * n * (long)sizeof(char *));
}

static void pd_sort(PDOC *d, void *base, long n, long size, int (*cmp)(const void *, const void *))
{
	char *c = base, *e, *f;
	long i, moves = 0, limit = n * 2 + 64, down = 0;
	unsigned char t[SORT_MAX];

	if (size > SORT_MAX) {
		qsort(base, (size_t)n, (size_t)size, cmp);
		return;
	}
	/* how far from the order: the pairs out of it */
	for (i = 1, e = c; i < n; i++, e += size) {
		if (cmp(e, e + size) > 0)
			down++;
		if (!(i & 511))
			pause_point(d);
	}
	if (!down)
		return;
	if (down > 16 + n / 64) {
		pd_msort(d, base, n, size, cmp);
		return;
	}
	for (i = 1, f = c + size; i < n; i++, f += size) {
		e = f;
		if (cmp(e - size, e) <= 0)
			continue;
		memcpy(t, e, (size_t)size);
		do {
			memcpy(e, e - size, (size_t)size);
			e -= size;
			if (!(++moves & 255))
				pause_point(d);
			if (moves > limit) {
				memcpy(e, t, (size_t)size);
				pd_msort(d, base, n, size, cmp);
				return;
			}
		} while (e > c && cmp(e - size, t) > 0);
		memcpy(e, t, (size_t)size);
	}
}

static int ch_cmp_y(const void *a, const void *b)
{
	const CH *x = a, *y = b;

	if (x->y != y->y)
		return x->y > y->y ? -1 : 1;
	return x->x < y->x ? -1 : x->x > y->x;
}

static int ch_cmp_x(const void *a, const void *b)
{
	const CH *x = a, *y = b;

	return x->x < y->x ? -1 : x->x > y->x;
}

/* a UTF-8 text: the number of Hebrew / Arabic letters and of letters at all */
static int is_rtl_text(const char *s, int len)
{
	int i, rtl = 0, ltr = 0;

	for (i = 0; i < len;) {
		unsigned c = (unsigned char)s[i];
		unsigned u;
		int n;

		if (c < 0x80) { u = c; n = 1; }
		else if ((c & 0xE0) == 0xC0 && i + 1 < len) { u = (c & 31) << 6 | (s[i + 1] & 63); n = 2; }
		else if ((c & 0xF0) == 0xE0 && i + 2 < len) { u = (c & 15) << 12 | (s[i + 1] & 63) << 6 | (s[i + 2] & 63); n = 3; }
		else { u = 0xFFFD; n = 1; }
		i += n;
		if ((u >= 0x590 && u <= 0x8FF) || (u >= 0xFB1D && u <= 0xFDFF) || (u >= 0xFE70 && u <= 0xFEFF))
			rtl++;
		else if ((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= 0xC0 && u < 0x590))
			ltr++;
	}
	return rtl > ltr;
}

/* emit chunks [a, b) of one line, in visual order left to right */
static void emit_line(IX *x, CH *c, long a, long b, PTEXT *t)
{
	long i, runstart;
	SBUF *o = &t->text;
	fx gapmin = 9830;		/* 0.15 em: a gap that is a space between words */
	long chars = 0;
	int spaces = 0;

	/* a line set a glyph at a time (its pieces under 3 characters on
	   average) that has its spaces as characters takes only a wide gap
	   (0.8 em: a tab, a column) as one more: its glyphs may be narrower
	   than the line sets them (Calamus SL's PDFs, through its Bridge: each
	   glyph placed alone, the font's widths its ink, "t h e di ffer e n
	   ce", Erez's EDOS_01.PDF, 1 Nov 2026) */
	for (i = a; i < b; i++) {
		chars += c[i].len;
		if (!spaces && memchr(x->txt.s + c[i].off, ' ', (size_t)c[i].len))
			spaces = 1;
	}
	/* (a line of one word, a heading's last, has no space of its own: the
	   page's tell) */
	if ((spaces || x->glyphwise) && chars < 3 * (b - a))
		gapmin = 52429;
	for (i = a; i < b;) {
		int rtl = is_rtl_text(x->txt.s + c[i].off, c[i].len);

		if (!rtl) {
			/* the same piece again where it stood (printed twice, a bold
			   made by a small offset: EDOS_01.PDF's running title) once */
			if (i > a && c[i].len == c[i - 1].len && fxabs(c[i].x - c[i - 1].x) < (c[i].fs >> 3) &&
			    fxabs(c[i].y - c[i - 1].y) < (c[i].fs >> 3) &&
			    !memcmp(x->txt.s + c[i].off, x->txt.s + c[i - 1].off, (size_t)c[i].len)) {
				i++;
				continue;
			}
			/* a space between pieces when there is a gap */
			if (i > a) {
				fx gap = c[i].x - (c[i - 1].x + c[i - 1].dx);
				fx fs = c[i].fs > c[i - 1].fs ? c[i].fs : c[i - 1].fs;

				if (gap > fxmul(fs, gapmin) && o->len && o->s[o->len - 1] != ' ' && !c[i].sp)
					sb_addc(o, ' ');
			}
			sb_add(o, x->txt.s + c[i].off, c[i].len);
			i++;
			continue;
		}
		/* a run of RTL pieces: in logical order they come from the right */
		runstart = i;
		while (i < b && is_rtl_text(x->txt.s + c[i].off, c[i].len))
			i++;
		{
			long k;
			int first = 1;

			for (k = i - 1; k >= runstart; k--) {
				SBUF tmp;
				const char *s = x->txt.s + c[k].off;
				int len = c[k].len, j;

				if (!first || runstart > a) {
					if (o->len && o->s[o->len - 1] != ' ' && !c[k].sp)
						sb_addc(o, ' ');
				}
				first = 0;
				/* characters of the piece, last to first (by code point) */
				sb_init(&tmp);
				for (j = len; j > 0;) {
					int st = j - 1;

					while (st > 0 && ((unsigned char)s[st] & 0xC0) == 0x80)
						st--;
					sb_add(&tmp, s + st, j - st);
					j = st;
				}
				sb_add(o, tmp.s, tmp.len);
				sb_free(&tmp);
			}
		}
	}
}

/* the line just written (it ends at text[len]) ends in a number after a
   row of dots: a contents line, which never goes on in the next */
static int leader_line(const char *text, long len)
{
	long i = len, dots = 0;

	while (i > 0 && text[i - 1] == ' ')
		i--;
	if (i == 0 || text[i - 1] < '0' || text[i - 1] > '9')
		return 0;
	while (i > 0 && text[i - 1] != '\n' && text[i - 1] != '\v') {
		dots = text[i - 1] == '.' ? dots + 1 : 0;
		if (dots >= 4)
			return 1;
		i--;
	}
	return 0;
}

/* the pictures sorted by their top, highest first (a page has few) */
static void pic_order(PTEXT *t)
{
	int i, j;

	for (i = 1; i < t->nimg; i++)
		for (j = i; j > 0 && t->img[j].top > t->img[j - 1].top; j--) {
			PIMG s = t->img[j];

			t->img[j] = t->img[j - 1];
			t->img[j - 1] = s;
		}
	t->pnext = 0;
}

/* a picture's line, a paragraph of its own: "\x7F<object> <w>x<h>", then
   'v' when it shows upside down, 'h' mirrored */
static void pic_line(PTEXT *t, const PIMG *im)
{
	char b[48];
	SBUF *o = &t->text;

	if (im->obj < 0 || im->dw < FX_ONE * 8 || im->dh < FX_ONE * 8)
		return;			/* an inline image, or a speck */
	if (o->len && o->s[o->len - 1] != '\n')
		sb_addc(o, '\n');
	snprintf(b, sizeof b, "\x7F%ld %ldx%ld%s%s\n", im->obj, (long)(im->dw >> 16), (long)(im->dh >> 16),
		 im->flip & PIMG_V ? "v" : "", im->flip & PIMG_H ? "h" : "");
	sb_adds(o, b);
	t->piclines++;
}

/* within one of the caller's boxes (PTEXT.cut) */
static int in_cut(const PTEXT *t, fx px, fx py)
{
	int i;

	for (i = 0; i < t->ncut; i++) {
		const fx *b = t->cut + 4 * i;

		if (px >= b[0] && px <= b[2] && py >= b[1] && py <= b[3])
			return 1;
	}
	return 0;
}

/* the text and pictures in the caller's boxes left out, a picture's line
   for each box in their place (EConvert: a drawing made a picture, its
   labels in it) */
static void cut_out(IX *x, PTEXT *t)
{
	long i, j = 0;
	int k, m = 0;

	for (i = 0; i < x->nch; i++)
		if (!in_cut(t, x->ch[i].x, x->ch[i].y))
			x->ch[j++] = x->ch[i];
	x->nch = j;
	for (k = 0; k < t->nimg; k++) {
		const PIMG *im = &t->img[k];

		if (!in_cut(t, im->left + im->dw / 2, im->top - im->dh / 2))	/* (by its middle) */
			t->img[m++] = *im;
	}
	t->nimg = m;
	for (k = 0; k < t->ncut && t->nimg < 64; k++) {
		PIMG *im = &t->img[t->nimg++];
		const fx *b = t->cut + 4 * k;

		memset(im, 0, sizeof *im);
		im->obj = t->cut_obj[k];
		im->dw = b[2] - b[0];
		im->dh = b[3] - b[1];
		im->top = b[3];
		im->left = b[0];
		im->cut = 1;
	}
}

/* a line of the page: chunks [a, b), its baseline, size and extent */
typedef struct {
	long a, b;
	fx y, fs, x0, x1;
} LN;

#define GBIN (8L << 16)		/* the gutter's bins: 8 points */
#define GBINS 128

#define VLINE (-(4000L << 16))	/* below it: the lines of text set up or down the page */

/*
 * The gutter where too few lines have a gap in it for the vote (lines
 * wholly in their column: the columns' baselines apart, so one isn't set
 * beside the other; Calamus SL's EDOS_01.PDF, 1 Nov 2026): a strip near
 * the middle of the text where at most a tenth of the lines in the body's
 * size have ink (a gap of 1.5 em is none: a line's two parts, one in each
 * column), with at least a fifth of them having text on each side of it,
 * as wide on average as half the left side and two thirds of the right
 * (columns of text: a contents page's page numbers or a listing's
 * comments make none, The ST Internals), and most of the left's lines
 * without text beside them (a table's rows have it). Headings, in a bigger
 * size, may cross it: they are above the columns (EDOS_01.PDF's title,
 * page 1). Its middle, or 0.
 */
static fx clear_strip(IX *x, const LN *ln, long nl, fx lo0, fx width, long nb)
{
	fx lo = lo0;		/* lo0: the bins' start */
	short ink[GBINS], sizes[128];
	long k, c, b, from = -1, run = 0, best = -1, bestrun = 0, nleft, nright, nbody = 0;
	fx g0, g1, body = 0, blo = 0x7fffffff, bhi = -0x7fffffff;	/* the body's lines' extent */
	int top = 0;

	/* the body's size: the one most lines are in (to a quarter point), and
	   the lines in it (up to an eighth bigger: ASK Magazine's 9-point line
	   of ornaments over its 7-point text is not one) */
	memset(sizes, 0, sizeof sizes);
	for (k = 0; k < nl; k++)
		if (ln[k].y > VLINE && ln[k].fs >> 14 < 128)
			sizes[ln[k].fs >> 14]++;
	for (k = 1; k < 128; k++)
		if (sizes[k] > sizes[top])
			top = (int)k;
	body = ((fx)top + 1) << 14;		/* the top of its quarter point */
	memset(ink, 0, sizeof ink);
	for (k = 0; k < nl; k++) {
		fx r0;

		if (ln[k].y <= VLINE || ln[k].fs > body + (body >> 3))
			continue;
		nbody++;
		if (ln[k].x0 < blo)
			blo = ln[k].x0;
		if (ln[k].x1 > bhi)
			bhi = ln[k].x1;
		/* each run of glyphs without a gap of 1.5 em: the bins it covers */
		r0 = x->ch[ln[k].a].x;
		for (c = ln[k].a + 1; c <= ln[k].b; c++) {
			fx end = x->ch[c - 1].x + x->ch[c - 1].dx;

			if (c < ln[k].b) {
				fx fs = x->ch[c].fs < x->ch[c - 1].fs ? x->ch[c].fs : x->ch[c - 1].fs;

				if (x->ch[c].x - end <= fs + (fs >> 1))
					continue;
			}
			{
				long b0 = (r0 - lo) / GBIN, b1 = (end - lo) / GBIN;

				for (b = b0 < 0 ? 0 : b0; b <= b1 && b < nb; b++)
					ink[b]++;
			}
			if (c < ln[k].b)
				r0 = x->ch[c].x;
		}
	}
	if (nbody < 8)
		return 0;
	/* the widest run of such bins within the middle 25-75 % of the body's
	   lines (a heading's, or a line of ornaments', may run far wider; ASK
	   Magazine's sidebar beside its text: 73 %) */
	lo = blo;
	width = bhi - blo;
	for (b = 0; b <= nb; b++) {
		fx at = lo0 + b * GBIN + GBIN / 2;
		int ok = b < nb && ink[b] * 10L <= nbody && at >= lo + width / 4 && at <= lo + width * 3 / 4;

		if (ok) {
			if (!run++)
				from = b;
		} else if (run) {
			if (run > bestrun)
				best = from, bestrun = run;
			run = 0;
		}
	}
	if (best < 0)
		return 0;
	g0 = lo0 + best * GBIN;
	g1 = lo0 + (best + bestrun) * GBIN;
	nleft = nright = 0;
	{
		long wl = 0, wr = 0, npair = 0;	/* (in 1/16 points: a page's lines in points would overflow) */

		for (k = 0; k < nl; k++)
			if (ln[k].y > VLINE && ln[k].fs <= body + (body >> 3)) {
				fx le = ln[k].x0, rs = ln[k].x1;	/* the left part's end, the right's start */

				for (c = ln[k].a; c < ln[k].b; c++) {
					if (x->ch[c].x + x->ch[c].dx <= g0)
						le = x->ch[c].x + x->ch[c].dx;
					else if (x->ch[c].x >= g1 && rs == ln[k].x1)
						rs = x->ch[c].x;
				}
				npair += ln[k].x0 < g0 && ln[k].x1 > g1;
				if (ln[k].x0 < g0) {
					nleft++;
					wl += (le - ln[k].x0) >> 12;
				}
				if (ln[k].x1 > g1) {
					nright++;
					wr += (ln[k].x1 - rs) >> 12;
				}
			}
		/* (and most of the left's lines alone on their line: a table's
		   names each have their row's text beside them, its wrapped
		   lines alone on the right, The ST Internals' line-A variables,
		   the Compendium's modes) */
		if (nleft * 5 < nbody || nright * 5 < nbody || wl * 2 < nleft * ((g0 - lo) >> 12) ||
		    wr * 3 < nright * 2 * ((lo + width - g1) >> 12) || nleft - npair <= npair)
			return 0;
	}
	return (g0 + g1) / 2;
}

/*
 * Two columns side by side read one after the other (with t->soft, the
 * text view's): where many lines have a gap wider than 1.5 em at the same
 * place, near the middle of the text, that is a gutter. The lines are cut
 * there, and between lines that cross it (a heading over both) the left
 * column's lines come first, then the right's. Room: ln holds 2 x nl.
 */
static long columns(IX *x, LN *ln, long nl)
{
	fx lo = 0x7fffffff, hi = -0x7fffffff, width, gutter;
	short votes[GBINS];
	long k, c, best = -1, nb, nout = 0, nl_ = 0, nr = 0;
	LN *left, *right, *out;

	if (nl < 8)
		return nl;
	for (k = 0; k < nl; k++) {
		if (ln[k].y <= VLINE)
			continue;
		if (ln[k].x0 < lo)
			lo = ln[k].x0;
		if (ln[k].x1 > hi)
			hi = ln[k].x1;
	}
	width = hi - lo;
	if (width <= GBIN * 4)
		return nl;
	nb = width / GBIN + 1;
	if (nb > GBINS)
		nb = GBINS;
	memset(votes, 0, sizeof votes);
	for (k = 0; k < nl; k++) {
		long last = -1;

		if (ln[k].y <= VLINE)
			continue;
		for (c = ln[k].a + 1; c < ln[k].b; c++) {
			fx end = x->ch[c - 1].x + x->ch[c - 1].dx, gap = x->ch[c].x - end;
			/* the smaller of the two sides' sizes: a heading beside the
			   other column's text (EDOS_01.PDF's "Next time") */
			fx fs = x->ch[c].fs < x->ch[c - 1].fs ? x->ch[c].fs : x->ch[c - 1].fs;

			if (gap > fs + (fs >> 1)) {
				/* each bin whose middle the gap covers (a ragged column's
				   lines end anywhere: their gaps' middles spread over
				   several bins, EDOS_01.PDF), else the one at its middle;
				   a gap over 8 em (a map's open sea) its middle alone */
				long b0 = (end - lo + GBIN / 2 + GBIN - 1) / GBIN, b1 = (x->ch[c].x - lo - GBIN / 2) / GBIN, b;

				if (b1 < b0 || gap > 8 * fs)
					b0 = b1 = (end + gap / 2 - lo) / GBIN;
				for (b = b0 > last ? b0 : last + 1; b <= b1; b++)
					if (b >= 0 && b < nb)
						votes[b]++, last = b;
			}
		}
	}
	for (k = 0; k < nb; k++)
		if (best < 0 || votes[k] > votes[best])
			best = k;
	/* the middle of the run of bins with that many votes */
	if (best >= 0) {
		long e = best;

		while (e + 1 < nb && votes[e + 1] == votes[best])
			e++;
		gutter = lo + (best + e + 1) * GBIN / 2;
	} else
		gutter = 0;
	if ((best < 0 || votes[best] < 6 || votes[best] * 10L < nl * 3 ||
	     gutter < lo + width * 3 / 10 || gutter > lo + width * 7 / 10) && (gutter = clear_strip(x, ln, nl, lo, width, nb)) == 0)
		return nl;
	left = ln + nl;			/* the room after the lines: the left column, then the right */
	/* out: the lines and those cut in two, 2 x nl at most; right after it
	   (where out ran on into right before: a page with lines cut at the
	   gutter lost lines and doubled others, EDOS_01.PDF page 1) */
	out = pdf_malloc(x->d, 3 * nl * (long)sizeof(LN));
	if (!out)
		return nl;
	right = out + 2 * nl;
	for (k = 0; k <= nl; k++) {
		LN *l = k < nl ? &ln[k] : NULL;
		long m = -1;

		if (l && l->y > VLINE) {
			if (l->x1 <= gutter + l->fs) {
				left[nl_++] = *l;
				continue;
			}
			if (l->x0 >= gutter - l->fs) {
				right[nr++] = *l;
				continue;
			}
			/* a gap round the gutter, as wide as the vote's, the line cut
			   there (a narrower one is between a heading's letters) */
			for (c = l->a + 1; c < l->b; c++)
				if (x->ch[c - 1].x + x->ch[c - 1].dx <= gutter && x->ch[c].x >= gutter &&
				    x->ch[c].x - (x->ch[c - 1].x + x->ch[c - 1].dx) >
					    (x->ch[c].fs < x->ch[c - 1].fs ? x->ch[c].fs : x->ch[c - 1].fs) * 3 / 2) {
					m = c;
					break;
				}
			if (m > 0) {
				LN a = *l, b = *l;

				a.b = m;
				a.x1 = x->ch[m - 1].x + x->ch[m - 1].dx;
				b.a = m;
				b.x0 = x->ch[m].x;
				left[nl_++] = a;
				right[nr++] = b;
				continue;
			}
		}
		/* a line across both columns, or the page's end: the band so far,
		   the left column first */
		for (c = 0; c < nl_; c++)
			out[nout++] = left[c];
		for (c = 0; c < nr; c++)
			out[nout++] = right[c];
		nl_ = nr = 0;
		if (l)
			out[nout++] = *l;
	}
	memcpy(ln, out, (size_t)nout * sizeof(LN));
	pdf_free(x->d, out, 3 * nl * (long)sizeof(LN));
	return nout;
}

/*
 * The lines in reading order. Between two lines comes '\n', or with
 * t->soft '\v' where the paragraph goes on: the next line's first word
 * would not have fitted at the end of the line before (half an em a
 * letter), the next starts where it did (within 1 em, or up to 3 em left
 * of an indented first line) and is in the same size. A gap of more than 1.875 lines
 * is a paragraph's end and gets an empty line. A word cut by a hyphen at a
 * soft break, or at any break without t->soft, is joined to its rest.
 */
static void assemble(IX *x, PTEXT *t)
{
	long i, j, nl = 0, k, turn = 0, nln;
	LN *q;
	LN *ln;
	int pstart = 1;		/* the line before began a paragraph */

	if (t->ncut > 0)
		cut_out(x, t);
	if (x->nch == 0) {
		/* a page of pictures alone (a scan): their lines only */
		if (t->pics) {
			pic_order(t);
			while (t->pnext < t->nimg)
				pic_line(t, &t->img[t->pnext++]);
		}
		return;
	}
	/* a picture over most of a page that has text is its background, or
	   the scan behind its text layer: the text is what is read */
	if (t->pics && x->page_area > 0) {
		int i, j = 0;

		for (i = 0; i < t->nimg; i++)
			if (t->img[i].cut || (fxmul(t->img[i].dw, t->img[i].dh >> 8) >> 8) * 5 < x->page_area * 3)
				t->img[j++] = t->img[i];
		t->nimg = j;
	}
	/* a space the stream puts before a glyph that stands a little left of
	   it (a heading set tight: "The" space "EDOS", the space at 107.9, the
	   E at 105.0, EDOS_01.PDF) sorted just before that glyph */
	for (i = 1; i + 1 < x->nch; i++) {
		CH *p = &x->ch[i - 1], *s = &x->ch[i], *n = &x->ch[i + 1];

		/* (left to right: the glyph before it on its left; in Hebrew the
		   next glyph is on the left anyway) */
		if (s->len == 1 && x->txt.s[s->off] == ' ' && fxabs(n->y - s->y) < (s->fs >> 1) && n->x < s->x &&
		    n->x > s->x - (s->fs >> 1) && fxabs(p->y - s->y) < (s->fs >> 1) && p->x < n->x)
			s->x = n->x - 1;
	}
	/* text set up or down the page (a running title along its edge,
	   EDOS_01.PDF's "SOFTWARE") would be cut into a letter a line and
	   glued to the ends of the lines beside it: turned to lie along a line
	   of its own after the page's text, one for each place it stands */
	{
		fx vx = 0, vfs = 0;
		int up = 0;

		for (i = 0; i < x->nch; i++) {
			CH *c = &x->ch[i];
			int k, blank = 1;

			for (k = 0; k < c->len && blank; k++)
				blank = x->txt.s[c->off + k] == ' ';
			/* (a space between its words where its letters stand, with
			   no advance of its own, goes with them) */
			/* (upright within 7 degrees: a map's slanted labels stay
			   as they were) */
			if (fxabs(c->dy) > 8 * fxabs(c->dx) || (blank && vfs && !c->dy && fxabs(c->x - vx) < vfs)) {
				fx along;

				if (c->dy)
					up = c->dy > 0;
				along = up ? c->y : -c->y;
				vx = c->x;
				vfs = c->fs;
				c->y = -c->x - (8000L << 16);
				c->x = along;
				c->dx = fxabs(c->dy);
				c->dy = 0;
			} else
				vfs = 0;
		}
	}
	/* a big initial letter (a drop cap: EDOS_01.PDF's "I started", the I
	   at 14.7 points beside lines of 7.2, its baseline a line below the
	   first's): taken into the line the text after it starts, as its first
	   letter, not into the line its baseline is on */
	for (i = 0; i + 1 < x->nch; i++) {
		CH *s = &x->ch[i], *n = &x->ch[i + 1];

		if (s->len == 1 && !s->dy && !n->dy && n->fs > 0 && s->fs >= n->fs + (n->fs >> 1) && n->y > s->y &&
		    n->y - s->y < s->fs && n->x > s->x && n->x - (s->x + s->dx) < n->fs) {
			s->y = n->y;
			s->fs = n->fs;
			if (s->x + s->dx > n->x)
				s->dx = n->x - s->x;
		}
	}
	{
		long chars = 0, spaces = 0;

		for (i = 0; i < x->nch; i++) {
			chars += x->ch[i].len;
			spaces += x->ch[i].len == 1 && x->txt.s[x->ch[i].off] == ' ';
		}
		x->glyphwise = chars < 3 * x->nch && spaces * 20 >= x->nch;	/* a space a word or so */
	}
	ch_sort(x->d, x->ch, x->nch, ch_cmp_y);
	/* the lines' table grown as they are found, then to twice them (split
	   at a gutter: two at most): twice the glyphs, as it was, was 217 KB of
	   EDOS_01.PDF's page 3, a page set a glyph at a time; no pass of its
	   own to count them (a line is tens of glyphs: few grow steps) */
	nln = x->nch / 32 + 16;
	ln = pdf_malloc(x->d, nln * (long)sizeof(LN));
	if (!ln)
		return;
	for (i = 0; i < x->nch; i = j) {
		fx y0 = x->ch[i].y, fsm = x->ch[i].fs;

		j = i + 1;
		while (j < x->nch && fxabs(x->ch[j].y - y0) < (fsm >> 1) + 1) {
			if (x->ch[j].fs > fsm)
				fsm = x->ch[j].fs;
			j++;
		}
		if (j - i > 1)
			ch_sort(x->d, x->ch + i, j - i, ch_cmp_x);
		if (j - turn >= 256) {
			/* a line's glyphs sorted left to right take a while, a page's
			   lines more: EDOS_01.PDF's glyph by glyph */
			pause_point(x->d);
			turn = j;
		}
		if (nl == nln) {
			if (!(q = pdf_realloc(x->d, ln, nln * (long)sizeof(LN), 2 * nln * (long)sizeof(LN)))) {
				pdf_free(x->d, ln, nln * (long)sizeof(LN));
				return;
			}
			ln = q;
			nln *= 2;
		}
		ln[nl].a = i;
		ln[nl].b = j;
		ln[nl].y = y0;
		ln[nl].fs = fsm;
		ln[nl].x0 = x->ch[i].x;
		ln[nl].x1 = x->ch[j - 1].x + x->ch[j - 1].dx;
		nl++;
	}
	if (t->soft && nl > 0) {
		if (nln < 2 * nl) {
			if (!(q = pdf_realloc(x->d, ln, nln * (long)sizeof(LN), 2 * nl * (long)sizeof(LN)))) {
				pdf_free(x->d, ln, nln * (long)sizeof(LN));
				return;
			}
			ln = q;
			nln = 2 * nl;
		}
		nl = columns(x, ln, nl);
	}
	/* the pictures in the order they stand on the page, top first */
	if (t->pics)
		pic_order(t);
	for (k = 0; k < nl; k++) {
		if (t->pics)
			while (t->pnext < t->nimg && t->img[t->pnext].top > ln[k].y + ln[k].fs)
				pic_line(t, &t->img[t->pnext++]);
		if (k > 0) {
			LN *p = &ln[k - 1], *c = &ln[k];
			fx fs = p->fs > c->fs ? p->fs : c->fs;
			int para = p->y - c->y > fxmul(fs, 0x1E000);	/* 1.875 lines of its size */
			/* where the next line may start: where this one did, or up to
			   3 em left of a paragraph's first line (its indent) */
			fx left = p->x0 - (pstart ? 3 * fs : fs);
			const char *w = x->txt.s + x->ch[c->a].off;
			int wl = 0, wmax = x->ch[c->a].len, soft;
			SBUF *o = &t->text;
			long l = o->len, j;
			fx edge = 0;

			/* the right edge of this text block: the widest of the lines
			   near it that start where it does (not of the whole page) */
			for (j = k - 7 < 0 ? 0 : k - 7; j < nl && j <= k + 6; j++)
				if (fxabs(ln[j].x0 - p->x0) <= 3 * fs && ln[j].x1 > edge)
					edge = ln[j].x1;

			/* the line ended where it did because the next line's first word
			   wouldn't fit after it (5/8 em a letter and a space: generous, so
			   ragged lines join and a paragraph's short last line still ends it) */
			while (wl < wmax && w[wl] != ' ')
				wl++;
			soft = t->soft && !para && p->x1 + ((fs >> 1) + (fs >> 3)) * (wl + 1) > edge &&
			       c->x0 >= left && c->x0 <= p->x0 + fs && fxabs(c->fs - p->fs) <= (fs >> 3) &&
			       !leader_line(o->s, l);
			if ((soft || !t->soft) && l >= 2 && o->s[l - 1] == '-' &&
			    ((o->s[l - 2] >= 'a' && o->s[l - 2] <= 'z') || (unsigned char)o->s[l - 2] >= 0x80) &&
			    w[0] >= 'a' && w[0] <= 'z')
				o->len--;
			else
				sb_addc(o, soft ? '\v' : '\n');
			if (para)
				sb_addc(o, '\n');
			pstart = !soft;
		}
		emit_line(x, x->ch, ln[k].a, ln[k].b, t);
		t->lines++;
		if ((k & 7) == 7)
			pause_point(x->d);	/* a page's lines take time too: the caller may want the machine */
	}
	sb_addc(&t->text, '\n');
	while (t->pics && t->pnext < t->nimg)
		pic_line(t, &t->img[t->pnext++]);
	pdf_free(x->d, ln, nln * (long)sizeof(LN));
}

int pdf_page_text(PDOC *d, long n, PTEXT *t)
{
	IX x;
	POBJ *pg, *res, *box, *cont;
	long rot;

	memset(&x, 0, sizeof x);
#ifdef PDF_DRAW
	x.all_ops = t->draw != NULL;
#endif
	x.d = d;
	x.t = t;
	d->stopping = 0;
	d->page_prog = 0;
	sb_init(&x.txt);
	pg = pdf_page(d, n, &res, &box, &rot);
	if (!pg) {
		t->failed = 1;
		return -1;
	}
	x.res = res;
	if (box && box->t == PO_ARR && box->a >= 4) {
		fx v[4];
		int k;

		for (k = 0; k < 4; k++) {
			POBJ *e = pa_at(box, k);

			v[k] = e->t == PO_INT ? e->a << 16 : e->a;
		}
		x.page_area = fxmul(fxabs(v[2] - v[0]), fxabs(v[3] - v[1]) >> 8) >> 8;	/* points squared / 65536 */
	}
	memcpy(x.gs.ctm, IDENT, sizeof x.gs.ctm);
	rot = ((rot % 360) + 360) % 360;
	if (rot == 90) { x.gs.ctm[0] = 0; x.gs.ctm[1] = -FX_ONE; x.gs.ctm[2] = FX_ONE; x.gs.ctm[3] = 0; }
	else if (rot == 180) { x.gs.ctm[0] = -FX_ONE; x.gs.ctm[3] = -FX_ONE; }
	else if (rot == 270) { x.gs.ctm[0] = 0; x.gs.ctm[1] = FX_ONE; x.gs.ctm[2] = -FX_ONE; x.gs.ctm[3] = 0; }
	x.gs.th = FX_ONE;
	x.gs.fs = 0;
	memcpy(x.tm, IDENT, sizeof x.tm);
	memcpy(x.tlm, IDENT, sizeof x.tlm);
	pause_point(d);			/* the page found: its content next */
	cont = pd_get(d, pg, "Contents");
	if (cont && (cont->t == PO_DICT || cont->t == PO_ARR))
		/* a stream, or the pieces of one (an operator doesn't straddle two),
		   read a piece at a time */
		run_content_pieces(&x, cont, pd_getraw(pg, "Contents"));
	if (d->stopping) {
		t->stopped = 1;
		d->stopping = 0;
	} else {
		long t0 = d->clock ? d->clock() : 0;

		assemble(&x, t);
		if (d->clock)
			d->st.t_asm += d->clock() - t0;
	}
#ifdef PDF_MEMDEBUG
	fprintf(stderr, "page %ld: chunks %ld (%ld KB), ops %ld KB, sb %ld KB, txt %ld KB, mem %ld KB\n", n, x.nch,
		x.capch * (long)sizeof(CH) / 1024, x.opcap * (long)sizeof(OP) / 1024, x.sbcap / 1024, (long)x.txt.cap / 1024, d->st.mem / 1024);
#endif
	pdf_free(d, x.ch, x.capch * (long)sizeof(CH));
	pdf_free(d, x.sb, x.sbcap);
	pdf_free(d, x.ops, x.opcap * (long)sizeof(OP));
	sb_free(&x.txt);
	return 0;
}

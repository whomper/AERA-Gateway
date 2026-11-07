/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pdfxref.c - the cross-reference table: classic tables, xref streams,
 * hybrid files, /Prev chains; and a rebuild by scanning the file when
 * they are damaged.
 */
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pdfint.h"

static int xe_room(PDOC *d, long n)
{
	PXE *q;

	if (n <= d->nxe)
		return 1;
	if (n > 8000000)
		return 0;
	if (n < d->nxe * 2 && d->nxe * 2 <= 8000000)		/* grow by doubling, not an entry at a time */
		n = d->nxe * 2;
	q = pdf_realloc(d, d->xe, d->nxe * (long)sizeof(PXE), n * (long)sizeof(PXE));
	if (!q)
		return 0;
	memset(q + d->nxe, 0, (size_t)(n - d->nxe) * sizeof(PXE));
	d->xe = q;
	d->nxe = n;
	return 1;
}

static void xe_set(PDOC *d, long num, int t, uint32_t a, uint16_t b)
{
	if (num < 0 || !xe_room(d, num + 1))
		return;
	if (xe_kind(d, num))			/* a newer section has it */
		return;
	xe_put(d, num, t, a, b);
}

/* a dict (or any object) at file offset off, parsed into the document arena */
static POBJ *obj_at(PDOC *d, long off, const char *kw, long *after, long *numout)
{
	long want = 2048, got;
	unsigned char *buf;
	POBJ *o;

	for (;;) {
		LEX x;
		TOK t;
		const unsigned char *p;

		buf = pdf_malloc(d, want);
		if (!buf)
			return NULL;
		got = pdf_rd(d, off, buf, want);
		lex_init(&x, d, &d->doc_a, buf, got, off + got >= d->size);
		x.nocrypt = 1;
		if (kw) {			/* "trailer" */
			if (lex_token(&x, &t) != T_KEY || t.n != (long)strlen(kw) || memcmp(t.s, kw, (size_t)t.n)) {
				lex_done(&x);
				pdf_free(d, buf, want);
				return NULL;
			}
		} else {			/* N G obj */
			TOK a, b, c;

			if (lex_token(&x, &a) != T_INT || lex_token(&x, &b) != T_INT || lex_token(&x, &c) != T_KEY ||
			    c.n < 3 || memcmp(c.s, "obj", 3) != 0) {
				lex_done(&x);
				pdf_free(d, buf, want);
				return NULL;
			}
			if (numout)
				*numout = a.i;
			if (c.n > 3)
				x.p = c.s + 3;
		}
		o = pa_alloc(d, &d->doc_a, sizeof(POBJ));
		if (o && parse_obj(&x, o)) {
			p = x.p;
			if (after) {
				/* the stream data, if there is one */
				TOK t2;

				*after = -1;
				if (o->t == PO_DICT && lex_token(&x, &t2) == T_KEY && t2.n == 6 && !memcmp(t2.s, "stream", 6)) {
					const unsigned char *s = t2.s + 6;

					if (s < x.e && *s == '\r')
						s++;
					if (s < x.e && *s == '\n')
						s++;
					*after = off + (s - buf);
					{
						PENT *e = pa_alloc(d, &d->doc_a, (o->a + 1) * (long)sizeof(PENT));

						if (e) {
							memcpy(e, o->u.e, (size_t)o->a * sizeof(PENT));
							e[o->a].key = "$soff";
							e[o->a].v.t = PO_INT;
							e[o->a].v.a = (int32_t)*after;
							o->u.e = e;
							o->a++;
							o->flag = 1;
						}
					}
				}
			}
			(void)p;
			lex_done(&x);
			pdf_free(d, buf, want);
			return o;
		}
		{
			int trunc = x.trunc;

			lex_done(&x);
			pdf_free(d, buf, want);
			if (!trunc || off + got >= d->size || want >= (4L << 20))
				return NULL;
			want *= 4;
		}
	}
}

static int xref_table(PDOC *d, long off, long *next_pos_after)
{
	/* off points at "xref" */
	long pos = off + 4;
	unsigned char *buf;
	long cap = 16384, got, i, start, count;

	buf = pdf_malloc(d, cap);
	if (!buf)
		return -1;
	for (;;) {
		const unsigned char *p, *e;

		got = pdf_rd(d, pos, buf, cap);
		p = buf;
		e = buf + got;
		/* subsection header or "trailer" */
		while (p < e && is_white_c(*p))
			p++;
		if (e - p >= 7 && memcmp(p, "trailer", 7) == 0) {
			*next_pos_after = pos + (p - buf);
			pdf_free(d, buf, cap);
			return 0;
		}
		{
			fx f;
			int32_t a, b;
			int r;
			const unsigned char *q = p;

			while (q < e && *q >= '0' && *q <= '9')
				q++;
			if (q == p || !lex_number(p, q, &f, &a, &r))
				goto bad;
			start = a;
			while (q < e && is_white_c(*q))
				q++;
			p = q;
			while (q < e && *q >= '0' && *q <= '9')
				q++;
			if (q == p || !lex_number(p, q, &f, &b, &r))
				goto bad;
			count = b;
			p = q;
		}
		if (count < 0 || count > 8000000 || start < 0 || count > d->size / 18 + 1)
			goto bad;
		if (!xe_room(d, start + count))	/* once, for the whole subsection */
			goto bad;
		pos += p - buf;
		for (i = 0; i < count;) {
			long k;

			got = pdf_rd(d, pos, buf, cap);
			p = buf;
			e = buf + got;
			for (k = 0; i < count && e - p >= 21; k++) {
				const unsigned char *q;

				while (p < e && is_white_c(*p))
					p++;
				if (e - p < 21)
					break;
				/* the usual entry: "oooooooooo ggggg n", then the end of line */
				if (p[10] == ' ' && p[16] == ' ' && (p[17] == 'n' || p[17] == 'f')) {
					uint32_t o2 = 0, g2 = 0;
					int ok = 1, z;

					for (z = 0; z < 10; z++) {
						unsigned dg = (unsigned)(p[z] - '0');

						if (dg > 9) { ok = 0; break; }
						o2 = o2 * 10 + dg;
					}
					for (z = 11; ok && z < 16; z++) {
						unsigned dg = (unsigned)(p[z] - '0');

						if (dg > 9) { ok = 0; break; }
						g2 = g2 * 10 + dg;
					}
					if (ok) {
						long num = start + i;

						if (num >= d->nxe && !xe_room(d, num + 1))
							goto bad;
						if (!xe_kind(d, num))
							xe_put(d, num, p[17] == 'n' ? 1 : 3, (uint32_t)o2, (uint16_t)g2);
						p += 18;
						i++;
						continue;
					}
				}
				uint32_t off2 = 0, gen = 0;
				int n = 0;

				while (p < e && is_white_c(*p))
					p++;
				if (e - p < 18)
					break;
				q = p;
				while (q < e && *q >= '0' && *q <= '9' && n < 11) {
					off2 = off2 * 10 + (uint32_t)(*q - '0');
					q++;
					n++;
				}
				if (!n)
					goto bad;
				while (q < e && is_white_c(*q))
					q++;
				n = 0;
				while (q < e && *q >= '0' && *q <= '9' && n < 6) {
					gen = gen * 10 + (uint32_t)(*q - '0');
					q++;
					n++;
				}
				while (q < e && is_white_c(*q))
					q++;
				if (q >= e || (*q != 'n' && *q != 'f'))
					goto bad;
				xe_set(d, start + i, *q == 'n' ? 1 : 3, off2, (uint16_t)gen);
				q++;
				p = q;
				i++;
			}
			if (i < count && p == buf)
				goto bad;
			pos += p - buf;
			if (!got)
				goto bad;
		}
	}
bad:
	pdf_free(d, buf, cap);
	return -1;
}

static long be(const unsigned char *p, int w)
{
	long v = 0;

	while (w-- > 0)
		v = (v << 8) | *p++;
	return v;
}

static int xref_stream(PDOC *d, POBJ *o, long dataoff, long onum)
{
	POBJ *w, *idx;
	SBUF b;
	int w0, w1, w2, rec;
	long size, i, n, pos = 0;
	const char *term = NULL;

	w = pd_get(d, o, "W");
	if (!w || w->t != PO_ARR || w->a < 3)
		return -1;
	w0 = pa_at(w, 0)->a;
	w1 = pa_at(w, 1)->a;
	w2 = pa_at(w, 2)->a;
	if (w0 < 0 || w1 < 0 || w2 < 0 || w0 > 8 || w1 > 8 || w2 > 8 || w1 == 0)
		return -1;
	rec = w0 + w1 + w2;
	size = pd_int(d, o, "Size", 0);
	sb_init(&b);
	{
		POBJ *so = o;
		POBJ tmp;

		/* the stream's data offset is in "$soff" */
		(void)tmp;
		(void)dataoff;
		if (pdf_stream(d, so, onum, &b, &term, 64L << 20) != 0 || term) {
			sb_free(&b);
			return -1;
		}
	}
	idx = pd_get(d, o, "Index");
	if (!idx) {
		if (!xe_room(d, size))
			goto bad;
		n = b.len / rec;
		for (i = 0; i < n && i < size; i++) {
			const unsigned char *p = (const unsigned char *)b.s + i * rec;
			long t = w0 ? be(p, w0) : 1;

			xe_set(d, i, t == 1 ? 1 : t == 2 ? 2 : t == 0 ? 3 : 0, (uint32_t)be(p + w0, w1), (uint16_t)be(p + w0 + w1, w2));
		}
	} else {
		long k;

		for (k = 0; k + 1 < idx->a; k += 2) {
			long start = pa_at(idx, k)->a, cnt = pa_at(idx, k + 1)->a;

			if (start < 0 || cnt < 0 || !xe_room(d, start + cnt))
				goto bad;
			for (i = 0; i < cnt && pos + rec <= b.len; i++, pos += rec) {
				const unsigned char *p = (const unsigned char *)b.s + pos;
				long t = w0 ? be(p, w0) : 1;

				xe_set(d, start + i, t == 1 ? 1 : t == 2 ? 2 : t == 0 ? 3 : 0, (uint32_t)be(p + w0, w1),
				       (uint16_t)be(p + w0 + w1, w2));
			}
		}
	}
	sb_free(&b);
	return 0;
bad:
	sb_free(&b);
	return -1;
}

/* Read the section at off (and the ones before it). */
static int xref_section(PDOC *d, long off, int first, long *prev_seen, int depth)
{
	unsigned char head[32];
	long got, i, tpos, onum = 0, data = -1;
	POBJ *trailer = NULL;

	if (depth > 64)
		return -1;
	for (i = 0; i < depth; i++)
		if (prev_seen[i] == off)
			return -1;
	prev_seen[depth] = off;
	got = pdf_rd(d, off, head, sizeof head);
	for (i = 0; i < got && is_white_c(head[i]); i++)
		;
	if (got - i >= 4 && memcmp(head + i, "xref", 4) == 0) {
		if (xref_table(d, off + i, &tpos) != 0)
			return -1;
		trailer = obj_at(d, tpos, "trailer", NULL, NULL);
		if (!trailer || trailer->t != PO_DICT)
			return -1;
		if (first)
			d->trailer = trailer;
		{
			POBJ *xs = pd_getraw(trailer, "XRefStm");

			if (xs && xs->t == PO_INT) {
				long xo = xs->a, n2 = 0, d2 = -1;
				POBJ *so = obj_at(d, xo, NULL, &d2, &n2);

				if (so && so->t == PO_DICT && d2 >= 0)
					xref_stream(d, so, d2, n2);	/* a damaged one: the table stands */
			}
		}
	} else {
		POBJ *so = obj_at(d, off, NULL, &data, &onum);

		if (!so || so->t != PO_DICT || data < 0 || !pdf_is_name(pd_getraw(so, "Type"), "XRef"))
			return -1;
		if (xref_stream(d, so, data, onum) != 0)
			return -1;
		trailer = so;
		if (first)
			d->trailer = so;
	}
	{
		POBJ *pv = pd_getraw(trailer, "Prev");

		if (pv && (pv->t == PO_INT || pv->t == PO_REAL)) {
			long po = pv->t == PO_INT ? pv->a : pv->a >> 16;

			if (po > 0 && po < d->size)
				xref_section(d, po, 0, prev_seen, depth + 1);	/* a broken older section: keep what we have */
		}
	}
	return 0;
}

int xref_load(PDOC *d)
{
	unsigned char tail[1032];
	long n, i, off = -1, seen[66];
	POBJ *sz, *root;

	n = d->size < (long)sizeof tail ? d->size : (long)sizeof tail;
	if (pdf_rd(d, d->size - n, tail, n) != n)
		return -1;
	for (i = n - 9; i >= 0; i--)
		if (memcmp(tail + i, "startxref", 9) == 0) {
			fx f;
			int32_t v;
			int r;
			const unsigned char *p = tail + i + 9, *q;

			while (p < tail + n && is_white_c(*p))
				p++;
			q = p;
			while (q < tail + n && *q >= '0' && *q <= '9')
				q++;
			if (q > p && lex_number(p, q, &f, &v, &r))
				off = v;
			break;
		}
	if (off <= 0 || off >= d->size)
		return -1;
	if (xref_section(d, off, 1, seen, 0) != 0 || !d->trailer)
		return -1;
	sz = pd_get(d, d->trailer, "Size");
	if (sz && sz->t == PO_INT)
		xe_room(d, sz->a);
	root = pd_getraw(d->trailer, "Root");
	if (!root || root->t != PO_REF || root->a >= d->nxe || xe_kind(d, root->a) == 0)
		return -1;
	return 0;
}

/* ------------------------------------------------------------------ rebuild */
/* Scan the file for "N G obj", the trailers and the catalogue, and make the
   table from what is found: the last definition of an object wins. */
int xref_rebuild(PDOC *d)
{
	long chunk = 65536, pos = 0, cur = -1, cur_off = 0, root_cand = -1, xref_cand = -1;
	long nstm = 0, capstm = 0, *stm = NULL;
	unsigned char *buf;
	POBJ *tr = NULL;
	long i;

	buf = pdf_malloc(d, chunk + 64);
	if (!buf)
		return -1;
	xe_clear(d);
	d->trailer = NULL;
	while (pos < d->size) {
		long got = pdf_rd(d, pos, buf, chunk + 32), lim, j;

		if (got <= 0)
			break;
		lim = pos + got >= d->size ? got : chunk;
		for (j = 0; j < lim; j++) {
			unsigned char c = buf[j];

			if (c == 'o' && j + 3 <= got && buf[j + 1] == 'b' && buf[j + 2] == 'j' && j > 0 &&
			    (j + 3 >= got || !(buf[j + 3] >= 'a' && buf[j + 3] <= 'z')) && buf[j - 1] != 'd') {
				/* back over " G " and "N" */
				long k = j - 1, gend, nend, g = 0, num = 0, m = 1;

				while (k >= 0 && is_white_c(buf[k]))
					k--;
				gend = k;
				while (k >= 0 && buf[k] >= '0' && buf[k] <= '9')
					k--;
				if (k == gend || k < 0)
					continue;
				for (nend = gend; nend > k; nend--, m *= 10)
					g += (buf[nend] - '0') * m;
				if (!is_white_c(buf[k]))
					continue;
				while (k >= 0 && is_white_c(buf[k]))
					k--;
				nend = k;
				while (k >= 0 && buf[k] >= '0' && buf[k] <= '9')
					k--;
				if (k == nend)
					continue;
				for (m = 1; nend > k; nend--, m *= 10)
					num += (buf[nend] - '0') * m;
				if (num > 8000000 || num < 0 || (k >= 0 && !is_white_c(buf[k]) && buf[k] != '>' && buf[k] != ']'))
					continue;
				if (!xe_room(d, num + 1))
					continue;
				xe_put(d, num, 1, (uint32_t)(pos + k + 1), (uint16_t)g);
				cur = num;
				cur_off = pos + k + 1;
				(void)cur_off;
			} else if (c == '/' && j + 8 < got) {
				if (memcmp(buf + j, "/Catalog", 8) == 0 && cur >= 0)
					root_cand = cur;
				else if (memcmp(buf + j, "/XRef", 5) == 0 && cur >= 0 && !(buf[j + 5] >= 'A' && buf[j + 5] <= 'Z') && buf[j + 5] != 'S')
					xref_cand = cur;
				else if (memcmp(buf + j, "/ObjStm", 7) == 0 && cur >= 0) {
					if (nstm == capstm) {
						long ncap = capstm ? capstm * 2 : 16;
						long *q = pdf_realloc(d, stm, capstm * (long)sizeof(long), ncap * (long)sizeof(long));

						if (q) {
							stm = q;
							capstm = ncap;
						}
					}
					if (nstm < capstm)
						stm[nstm++] = cur;
				}
			} else if (c == 't' && j + 7 <= got && memcmp(buf + j, "trailer", 7) == 0) {
				POBJ *t = obj_at(d, pos + j, "trailer", NULL, NULL);

				if (t && t->t == PO_DICT && pd_getraw(t, "Root"))
					tr = t;
			}
		}
		pos += lim;
	}
	pdf_free(d, buf, chunk + 64);
	/* an encrypted file needs its key before an object stream can be read */
	if (!tr && xref_cand >= 0 && xe_kind(d, xref_cand) == 1) {
		POBJ *t = pdf_get(d, xref_cand);

		if (t && pd_getraw(t, "Root"))
			tr = t;
	}
	if (!tr && root_cand >= 0) {
		/* no trailer at all: make one */
		PENT *e = pa_alloc(d, &d->doc_a, sizeof(PENT));
		POBJ *t = pa_alloc(d, &d->doc_a, sizeof(POBJ));

		if (e && t) {
			e->key = "Root";
			e->v.t = PO_REF;
			e->v.a = (int32_t)root_cand;
			t->t = PO_DICT;
			t->a = 1;
			t->u.e = e;
			tr = t;
		}
	}
	d->trailer = tr;
	if (tr && pd_getraw(tr, "Encrypt") && !d->crypt) {
		POBJ *enc = pd_get(d, tr, "Encrypt"), *id = pd_get(d, tr, "ID");

		if (enc)
			d->encrypted = pc_open(d, enc, id) == 0 ? 1 : 2;
	}
	/* objects inside object streams: set where nothing else defines them */
	for (i = 0; i < nstm; i++) {
		long k;

		if (!objstm_register(d, stm[i]))
			continue;
		(void)k;
	}
	pdf_free(d, stm, capstm * (long)sizeof(long));
	if (!tr)
		return -1;
	if (!root_cand) {
		root_cand = -1;
	}
	if (root_cand >= 0) {
		POBJ *r = pd_getraw(tr, "Root");

		if (!r || r->t != PO_REF || r->a >= d->nxe || xe_kind(d, r->a) == 0) {
			if (r)
				r->a = (int32_t)root_cand;
		}
	}
	d->repaired = 1;
	return 0;
}

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pdfobj.c - memory, the file window, the arena, the lexer and parser,
 * and objects (from the file and from object streams).
 */
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pdfint.h"

/* ------------------------------------------------------------------ memory */
#ifndef PDF_TOS
/* the host's tools: PDF_MEMCAP=KB refuses what would take the reader past
   it, as a 1 MB ST would, and says each refusal and who asked
   (addr2line -f -e TOOL ADDR) */
#include <stdio.h>
#include <execinfo.h>
static long memcap = -1;

static int over_cap(PDOC *d, long n)
{
	if (memcap < 0) {
		const char *c = getenv("PDF_MEMCAP");

		memcap = c ? atol(c) * 1024 : 0;
	}
	if (memcap > 0 && d->st.mem + n > memcap) {
		void *bt[10];
		int k = backtrace(bt, 10), i;

		fprintf(stderr, "pdf: refused %ld bytes, %ld in use (arenas: page %ld, document %ld; fonts %ld, os %ld); from", n, d->st.mem,
			d->a.total, d->doc_a.total, d->st.fonts, d->os_len);
		for (i = 1; i < k; i++)
			fprintf(stderr, " %p", bt[i]);
		fprintf(stderr, "\n");
		return 1;
	}
	return 0;
}
#else
#define over_cap(d, n) 0
#endif

void *pdf_malloc(PDOC *d, long n)
{
	void *p = over_cap(d, n) ? NULL : malloc((size_t)n);

	if (p) {
		d->st.mem += n;
		if (d->st.mem > d->st.mem_peak)
			d->st.mem_peak = d->st.mem;
	} else if (n > 0)
		d->st.oom++;
	return p;
}

void *pdf_realloc(PDOC *d, void *p, long oldn, long n)
{
	void *q = over_cap(d, n - oldn) ? NULL : realloc(p, (size_t)n);

	if (q) {
		d->st.mem += n - oldn;
		if (d->st.mem > d->st.mem_peak)
			d->st.mem_peak = d->st.mem;
	} else if (n > 0)
		d->st.oom++;
	return q;
}

void pdf_free(PDOC *d, void *p, long n)
{
	if (p) {
		free(p);
		d->st.mem -= n;
	}
}

/* ------------------------------------------------------------------ arena */
#define ACH_SIZE 4096
#define AL ((long)sizeof(void *) > 4 ? 8 : 4)	/* the structs' alignment (a 68000 needs 2) */

void *pa_alloc(PDOC *d, PARENA *a, long n)
{
	ACH *c;
	void *p;

	n = (n + AL - 1) & ~(long)(AL - 1);
	c = a->head;
	if (!c || c->size - c->used < n) {
		long sz = n + (long)sizeof(ACH) > ACH_SIZE ? n + (long)sizeof(ACH) : ACH_SIZE;

		c = pdf_malloc(d, sz);
		if (!c)
			return NULL;
		c->size = sz - (long)sizeof(ACH);
		c->used = 0;
		c->next = a->head;
		a->head = c;
		a->total += sz;
	}
	p = (char *)(c + 1) + c->used;
	c->used += n;
	return p;
}

void pa_free(PDOC *d, PARENA *a)
{
	ACH *c, *nx;

	for (c = a->head; c; c = nx) {
		nx = c->next;
		pdf_free(d, c, c->size + (long)sizeof(ACH));
	}
	a->head = NULL;
	a->total = 0;
}

/* the arena back to where it was (head, and head's use then): what was
   built since dropped */
static void pa_back(PDOC *d, PARENA *a, ACH *head, long used)
{
	while (a->head && a->head != head) {
		ACH *c = a->head;

		a->head = c->next;
		a->total -= c->size + (long)sizeof(ACH);
		pdf_free(d, c, c->size + (long)sizeof(ACH));
	}
	if (a->head)
		a->head->used = used;
}

/* ------------------------------------------------------------------ file windows */
/* A window is what a seek and a read costs once: its bytes at the place
   asked for, kept for the next read near it. Two: a stream's bytes, read
   in turn (PWIN, or twice that on a 68020 and up), and the objects', a few
   at a time (POWIN). With one, each object read between two pieces of a
   page's content read 16 KB at the object and 16 KB at the content again:
   EDOS_01.PDF read 768 KB from the disk for a page of 220 KB. */
static long rd_win(PDOC *d, unsigned char *w, long *base, long *len, long cap, long off, void *buf, long n)
{
	long got = 0, k;
	unsigned char *b = buf;

	if (off < 0 || off >= d->size)
		return 0;
	if (n > d->size - off)
		n = d->size - off;
	while (got < n) {
		long o = off + got;

		if (o >= *base && o < *base + *len) {
			k = *base + *len - o;
			if (k > n - got)
				k = n - got;
			memcpy(b + got, w + (o - *base), (size_t)k);
			got += k;
			continue;
		}
		if (n - got >= cap) {	/* big: straight into the caller's buffer */
			k = pdf_os_read(d->h, o, b + got, n - got);
			d->st.reads++;
			d->st.bytes_read += k > 0 ? k : 0;
			if (k <= 0)
				break;
			got += k;
			continue;
		}
		k = pdf_os_read(d->h, o, w, cap);
		d->st.reads++;
		d->st.bytes_read += k > 0 ? k : 0;
		if (k <= 0)
			break;
		*base = o;
		*len = k;
	}
	return got;
}

long pdf_rd(PDOC *d, long off, void *buf, long n)
{
	return rd_win(d, d->owin, &d->obase, &d->olen, POWIN, off, buf, n);
}

long pdf_rd_seq(PDOC *d, long off, void *buf, long n)
{
	return rd_win(d, d->win, &d->wbase, &d->wlen, d->wcap, off, buf, n);
}

/* ------------------------------------------------------------------ lexer */
static const unsigned char cls[256] = {
	/* 1 white, 2 delimiter, 0 regular */
	[0] = 1, [9] = 1, [10] = 1, [12] = 1, [13] = 1, [32] = 1,
	['('] = 2, [')'] = 2, ['<'] = 2, ['>'] = 2, ['['] = 2, [']'] = 2, ['{'] = 2, ['}'] = 2, ['/'] = 2, ['%'] = 2,
};
#define IS_WHITE(c) (cls[(unsigned char)(c)] == 1)
#define IS_REG(c) (cls[(unsigned char)(c)] == 0)

void lex_init(LEX *x, PDOC *d, PARENA *arena, const unsigned char *buf, long n, int final)
{
	memset(x, 0, sizeof *x);
	x->d = d;
	x->arena = arena;
	x->p = buf;
	x->e = buf + n;
	x->final = final;
	x->num = x->gen = -1;
}

void lex_done(LEX *x)
{
	pdf_free(x->d, x->scr, x->scrcap);
	pdf_free(x->d, x->tmp, x->captmp * (long)sizeof(POBJ));
	x->scr = NULL;
	x->tmp = NULL;
}

void lex_skip(LEX *x)
{
	while (x->p < x->e) {
		unsigned char c = *x->p;

		if (IS_WHITE(c))
			x->p++;
		else if (c == '%') {
			while (x->p < x->e && *x->p != '\n' && *x->p != '\r')
				x->p++;
		} else
			break;
	}
}

#include "lexnum.h"

/* "12", "-3", "4.25", ".5", "+7": the value as fx and as an integer */
int lex_number(const unsigned char *s, const unsigned char *e, fx *fxv, int32_t *iv, int *isreal)
{
	int neg = 0, digits = 0, fd = 0, real = 0;
	uint32_t ip = 0, fp = 0;

	if (s < e && (*s == '+' || *s == '-')) {
		neg = *s == '-';
		s++;
	}
	while (s < e && *s == '-')		/* "--5": Acrobat reads 5 */
		s++;
	while (s < e && *s >= '0' && *s <= '9') {
		ip = ip < 429496729u ? ip * 10 + (uint32_t)(*s - '0') : 0xFFFFFFFFu;
		s++;
		digits++;
	}
	if (s < e && *s == '.') {
		real = 1;
		s++;
		while (s < e && *s >= '0' && *s <= '9') {
			if (fd < 5) {
				fp = fp * 10 + (uint32_t)(*s - '0');
				fd++;
			}
			s++;
			digits++;
		}
	}
	if (!digits || s != e)
		return 0;
	*isreal = real;
	*iv = neg ? -(int32_t)ip : (int32_t)ip;
	if (ip > 32767)
		ip = 32767;
	{
		uint32_t v = ip << 16;

		if (real && fd)
			v += frac_fx(fp, fd);
		*fxv = neg ? -(fx)v : (fx)v;
	}
	return 1;
}


static int hexv(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int scr_room(LEX *x, long n)
{
	if (n <= x->scrcap)
		return 1;
	{
		long cap = x->scrcap ? x->scrcap : 256;
		unsigned char *q;

		while (cap < n)
			cap *= 2;
		q = pdf_realloc(x->d, x->scr, x->scrcap, cap);
		if (!q)
			return 0;
		x->scr = q;
		x->scrcap = cap;
	}
	return 1;
}

static const unsigned char *arena_copy(LEX *x, const unsigned char *s, long n)
{
	unsigned char *o = pa_alloc(x->d, x->arena, n + 1);

	if (!o)
		return NULL;
	memcpy(o, s, (size_t)n);
	o[n] = 0;
	return o;
}

/* a (string): x->p is after the '(' */
static int lex_litstring(LEX *x, TOK *t)
{
	const unsigned char *s = x->p;
	long n = 0, depth = 1;

	while (s < x->e) {
		unsigned char c = *s++;

		if (n + 2 > x->scrcap && !scr_room(x, n + 2))
			return T_BAD;
		if (c == '\\') {
			if (s >= x->e)
				break;
			c = *s++;
			switch (c) {
			case 'n': x->scr[n++] = '\n'; break;
			case 'r': x->scr[n++] = '\r'; break;
			case 't': x->scr[n++] = '\t'; break;
			case 'b': x->scr[n++] = '\b'; break;
			case 'f': x->scr[n++] = '\f'; break;
			case '\r':
				if (s < x->e && *s == '\n')
					s++;
				break;
			case '\n':
				break;
			default:
				if (c >= '0' && c <= '7') {
					int v = c - '0', k;

					for (k = 0; k < 2 && s < x->e && *s >= '0' && *s <= '7'; k++)
						v = v * 8 + (*s++ - '0');
					x->scr[n++] = (unsigned char)v;
				} else
					x->scr[n++] = c;
			}
		} else if (c == '(') {
			depth++;
			x->scr[n++] = c;
		} else if (c == ')') {
			if (--depth == 0) {
				x->p = s;
				goto done;
			}
			x->scr[n++] = c;
		} else if (c == '\r') {
			if (s < x->e && *s == '\n')
				s++;
			x->scr[n++] = '\n';
		} else
			x->scr[n++] = c;
	}
	x->trunc = 1;
	return T_BAD;
done:
	if (x->d->crypt && x->num >= 0 && !x->nocrypt)
		n = pc_decrypt(x->d, x->num, x->gen, 0, x->scr, n);
	t->s = arena_copy(x, x->scr, n);
	t->n = n;
	return t->s ? T_STR : T_BAD;
}

/* a <hex string>: x->p is after the '<' */
static int lex_hexstring(LEX *x, TOK *t)
{
	long n = 0;
	int hi = -1;

	while (x->p < x->e) {
		unsigned char c = *x->p++;
		int v;

		if (c == '>')
			goto done;
		v = hexv(c);
		if (v < 0)
			continue;
		if (hi < 0)
			hi = v;
		else {
			if (n + 2 > x->scrcap && !scr_room(x, n + 2))
				return T_BAD;
			x->scr[n++] = (unsigned char)(hi << 4 | v);
			hi = -1;
		}
	}
	x->trunc = 1;
	return T_BAD;
done:
	if (hi >= 0) {
		if (n + 2 > x->scrcap && !scr_room(x, n + 2))
			return T_BAD;
		x->scr[n++] = (unsigned char)(hi << 4);
	}
	if (x->d->crypt && x->num >= 0 && !x->nocrypt)
		n = pc_decrypt(x->d, x->num, x->gen, 0, x->scr, n);
	t->s = arena_copy(x, x->scr, n);
	t->n = n;
	return t->s ? T_STR : T_BAD;
}

int lex_token(LEX *x, TOK *t)
{
	unsigned char c;

	lex_skip(x);
	if (x->p >= x->e) {
		x->trunc = !x->final;
		return t->kind = T_EOF;
	}
	c = *x->p;
	switch (c) {
	case '[': x->p++; return t->kind = T_ARR0;
	case ']': x->p++; return t->kind = T_ARR1;
	case '(':
		x->p++;
		return t->kind = lex_litstring(x, t);
	case '<':
		if (x->p + 1 < x->e && x->p[1] == '<') {
			x->p += 2;
			return t->kind = T_DICT0;
		}
		x->p++;
		return t->kind = lex_hexstring(x, t);
	case '>':
		if (x->p + 1 < x->e && x->p[1] == '>') {
			x->p += 2;
			return t->kind = T_DICT1;
		}
		x->p++;
		return t->kind = T_BAD;
	case ')': case '{': case '}':
		x->p++;
		return t->kind = T_BAD;
	case '/': {
		long n = 0;

		x->p++;
		while (x->p < x->e && IS_REG(*x->p)) {
			unsigned char ch = *x->p++;

			if (n + 2 > x->scrcap && !scr_room(x, n + 2))
				return t->kind = T_BAD;
			if (ch == '#' && x->p + 1 < x->e && hexv(x->p[0]) >= 0 && hexv(x->p[1]) >= 0) {
				ch = (unsigned char)(hexv(x->p[0]) << 4 | hexv(x->p[1]));
				x->p += 2;
			}
			x->scr[n++] = ch;
		}
		if (x->p >= x->e && !x->final)
			x->trunc = 1;
		t->s = arena_copy(x, x->scr ? x->scr : (const unsigned char *)"", n);
		t->n = n;
		return t->kind = t->s ? T_NAME : T_BAD;
	}
	}
	{
		const unsigned char *s = x->p;
		int isreal;

		while (x->p < x->e && IS_REG(*x->p))
			x->p++;
		if (x->p >= x->e && !x->final)
			x->trunc = 1;
		if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.') {
			if (lex_number(s, x->p, &t->f, &t->i, &isreal))
				return t->kind = isreal ? T_REAL : T_INT;
		}
		t->s = s;
		t->n = x->p - s;
		return t->kind = T_KEY;
	}
}

/* ------------------------------------------------------------------ parser */
static int tmp_push(LEX *x, const POBJ *o)
{
	if (x->ntmp == x->captmp) {
		long cap = x->captmp ? x->captmp * 2 : 64;
		POBJ *q = pdf_realloc(x->d, x->tmp, x->captmp * (long)sizeof(POBJ), cap * (long)sizeof(POBJ));

		if (!q)
			return 0;
		x->tmp = q;
		x->captmp = cap;
	}
	x->tmp[x->ntmp++] = *o;
	return 1;
}

static void set_null(POBJ *o)
{
	memset(o, 0, sizeof *o);
}

/* An integer, or a reference "n g R", straight from the bytes: arrays of references (/Kids) and of
   numbers (/Widths) are most of what is parsed in a big file. 1 when taken. */
static int fast_int_or_ref(LEX *x, POBJ *out)
{
	const unsigned char *p = x->p, *e = x->e, *q;
	uint32_t n1 = 0, n2 = 0;
	int d1 = 0, d2 = 0;

	while (p < e && is_white_c(*p))
		p++;
	if (p >= e || *p < '0' || *p > '9')
		return 0;
	q = p;
	while (q < e && *q >= '0' && *q <= '9' && d1 < 9) {
		n1 = n1 * 10 + (uint32_t)(*q++ - '0');
		d1++;
	}
	if (q >= e || !(is_white_c(*q) || *q == ']' || *q == '/' || *q == '>' || *q == '<' || *q == '[' || *q == '(')) {
		if (q >= e && !x->final)
			x->trunc = 1;
		return 0;				/* a real, a long number, a keyword: the general way */
	}
	if (is_white_c(*q)) {
		const unsigned char *r = q;

		while (r < e && is_white_c(*r))
			r++;
		if (r >= e && !x->final)
			return 0;			/* what follows is past the buffer: the general way, which says so */
		if (r < e && *r >= '0' && *r <= '9') {
			const unsigned char *s = r;

			while (s < e && *s >= '0' && *s <= '9' && d2 < 6) {
				n2 = n2 * 10 + (uint32_t)(*s++ - '0');
				d2++;
			}
			while (s < e && is_white_c(*s))
				s++;
			if (s + 1 >= e && !x->final)
				return 0;		/* "n g" and maybe "R" at the buffer's end: the same */
			if (d2 < 6 && s + 1 <= e && s < e && *s == 'R' && (s + 1 >= e || !(*(s + 1) >= '!' && *(s + 1) <= '~') ||
			    *(s + 1) == '/' || *(s + 1) == ']' || *(s + 1) == '>' || *(s + 1) == '<' || *(s + 1) == '[')) {
				out->t = PO_REF;
				out->a = (int32_t)n1;
				out->gen = (uint16_t)n2;
				x->p = s + 1;
				return 1;
			}
		}
	}
	out->t = PO_INT;
	out->a = (int32_t)n1;
	x->p = q;
	return 1;
}

int parse_obj(LEX *x, POBJ *out)
{
	TOK t;
	int k;

	set_null(out);
	if (fast_int_or_ref(x, out))
		return 1;
	k = lex_token(x, &t);
	switch (k) {
	case T_INT: {
		/* "n g R" ? */
		const unsigned char *save = x->p;
		TOK t2, t3;

		if (t.i >= 0 && lex_token(x, &t2) == T_INT && t2.i >= 0 && lex_token(x, &t3) == T_KEY &&
		    t3.n == 1 && t3.s[0] == 'R') {
			out->t = PO_REF;
			out->a = t.i;
			out->gen = (uint16_t)t2.i;
			return 1;
		}
		x->p = save;
		if (x->trunc)
			return 0;	/* the look past it ran off a buffer that isn't the file's end: the
					   number may be cut, or begin a reference ("12 0 R"): read more
					   (it was taken as it stood, and a font's /Widths lost) */
		out->t = PO_INT;
		out->a = t.i;
		return 1;
	}
	case T_REAL:
		out->t = PO_REAL;
		out->a = t.f;
		return 1;
	case T_STR:
		out->t = PO_STR;
		out->a = (int32_t)t.n;
		out->u.p = (const char *)t.s;
		return 1;
	case T_NAME:
		out->t = PO_NAME;
		out->a = (int32_t)t.n;
		out->u.p = (const char *)t.s;
		return 1;
	case T_KEY:
		if (t.n == 4 && memcmp(t.s, "true", 4) == 0) {
			out->t = PO_BOOL;
			out->a = 1;
			return 1;
		}
		if (t.n == 5 && memcmp(t.s, "false", 5) == 0) {
			out->t = PO_BOOL;
			out->a = 0;
			return 1;
		}
		if (t.n == 4 && memcmp(t.s, "null", 4) == 0)
			return 1;
		x->p = t.s;			/* a keyword the caller reads (endobj, stream) */
		return 0;
	case T_ARR0: {
		long base = x->ntmp, n;
		POBJ el;

		if (++x->depth > 24) {
			x->depth--;
			return 0;
		}
		for (;;) {
			const unsigned char *save = x->p;
			TOK t2;

			lex_skip(x);
			if (x->p < x->e && *x->p == ']') {
				x->p++;
				break;
			}
			(void)t2;
			if (!parse_obj(x, &el)) {
				if (x->p == save || x->trunc) {	/* a stray token: skip it */
					if (x->trunc || x->p >= x->e) {
						/* the buffer ended inside the array: cut off (read
						   more), as a dict's end says (a font's /Widths past
						   load_file_obj's first 1024 bytes was lost, and
						   tried again at each use: 468 KB on a page) */
						if (x->p >= x->e)
							x->trunc = !x->final;
						goto bad;
					}
					lex_token(x, &t2);
					if (t2.kind == T_ARR1)
						break;
					if (t2.kind == T_EOF)
						goto bad;
					continue;
				}
				goto bad;
			}
			if (!tmp_push(x, &el))
				goto bad;
		}
		n = x->ntmp - base;
		out->t = PO_ARR;
		out->a = (int32_t)n;
		out->u.v = n ? pa_alloc(x->d, x->arena, n * (long)sizeof(POBJ)) : NULL;
		if (n && !out->u.v)
			goto bad;
		if (n)
			memcpy(out->u.v, x->tmp + base, (size_t)n * sizeof(POBJ));
		x->ntmp = base;
		x->depth--;
		return 1;
	bad:
		x->ntmp = base;
		x->depth--;
		return 0;
	}
	case T_DICT0: {
		long base = x->ntmp, n;
		POBJ key, val;
		PENT *e;
		long i;

		if (++x->depth > 24) {
			x->depth--;
			return 0;
		}
		for (;;) {
			TOK t2;

			lex_skip(x);
			if (x->p + 1 < x->e && x->p[0] == '>' && x->p[1] == '>') {
				x->p += 2;
				break;
			}
			if (x->p >= x->e) {
				x->trunc = !x->final;
				goto dbad;
			}
			if (lex_token(x, &t2) != T_NAME)
				goto dbad;
			key.t = PO_NAME;
			key.a = (int32_t)t2.n;
			key.u.p = (const char *)t2.s;
			key.flag = 0;
			key.gen = 0;
			lex_skip(x);
			if (x->p + 1 < x->e && x->p[0] == '>' && x->p[1] == '>') {
				x->p += 2;		/* a key with no value: dropped */
				break;
			}
			if (x->d->skip_kids && t2.n == 4 && memcmp(t2.s, "Kids", 4) == 0 && x->p < x->e && *x->p == '[') {
				/* the list of a big page tree's pages is read once (pdf_pages), not for each page */
				while (x->p < x->e && *x->p != ']')
					x->p++;
				if (x->p >= x->e) {
					x->trunc = !x->final;
					goto dbad;
				}
				x->p++;
				memset(&val, 0, sizeof val);
			} else if (!parse_obj(x, &val))
				goto dbad;
			if (!tmp_push(x, &key) || !tmp_push(x, &val))
				goto dbad;
		}
		n = (x->ntmp - base) / 2;
		e = n ? pa_alloc(x->d, x->arena, n * (long)sizeof(PENT)) : NULL;
		if (n && !e)
			goto dbad;
		for (i = 0; i < n; i++) {
			e[i].key = x->tmp[base + 2 * i].u.p;
			e[i].v = x->tmp[base + 2 * i + 1];
		}
		out->t = PO_DICT;
		out->a = (int32_t)n;
		out->u.e = e;
		x->ntmp = base;
		x->depth--;
		return 1;
	dbad:
		x->ntmp = base;
		x->depth--;
		return 0;
	}
	default:
		return 0;
	}
}

/* ------------------------------------------------------------------ dict helpers */
POBJ *pd_getraw(POBJ *dict, const char *key)
{
	long i;

	if (!dict || dict->t != PO_DICT)
		return NULL;
	for (i = 0; i < dict->a; i++)
		if (strcmp(dict->u.e[i].key, key) == 0)
			return &dict->u.e[i].v;
	return NULL;
}

POBJ *pdf_res(PDOC *d, POBJ *o)
{
	int k;

	for (k = 0; o && o->t == PO_REF && k < 8; k++)
		o = pdf_get(d, o->a);
	return o && o->t != PO_REF ? o : NULL;
}

POBJ *pd_get(PDOC *d, POBJ *dict, const char *key)
{
	POBJ *o = pdf_res(d, pd_getraw(dict, key));

	return o && o->t != PO_NULL ? o : NULL;
}

long pd_int(PDOC *d, POBJ *dict, const char *key, long def)
{
	POBJ *o = pd_get(d, dict, key);

	if (!o)
		return def;
	if (o->t == PO_INT || o->t == PO_BOOL)
		return o->a;
	if (o->t == PO_REAL)
		return o->a >> 16;
	return def;
}

const char *pd_name(PDOC *d, POBJ *dict, const char *key)
{
	POBJ *o = pd_get(d, dict, key);

	return o && o->t == PO_NAME ? o->u.p : NULL;
}

POBJ *pa_at(POBJ *arr, long i)
{
	return arr && arr->t == PO_ARR && i >= 0 && i < arr->a ? &arr->u.v[i] : NULL;
}

int pdf_is_name(const POBJ *o, const char *name)
{
	return o && o->t == PO_NAME && strcmp(o->u.p, name) == 0;
}

/* ------------------------------------------------------------------ objects */
static POBJ unloadable;			/* the cache's mark for an object that can't be loaded */

static POBJ *cache_find(PDOC *d, long num)
{
	PCENT *c = &d->cache[num & (PCACHE - 1)];

	return c->num == num + 1 && c->o ? c->o : NULL;
}

static void cache_put(PDOC *d, long num, POBJ *o)
{
	PCENT *c = &d->cache[num & (PCACHE - 1)];

	c->num = (int32_t)(num + 1);
	c->o = o;
}

/* Parse an object whose text starts at buf. For a stream the dict gets an
   extra entry "$soff" (where the data starts). Returns NULL if the text is
   cut off (*trunc) or is not an object. */
static POBJ *parse_indirect(PDOC *d, const unsigned char *buf, long n, int final, long num, long gen,
			    long base, int in_stm, int *trunc)
{
	LEX x;
	POBJ *o;
	TOK t;

	lex_init(&x, d, d->cur, buf, n, final);
	x.num = num;
	x.gen = gen;
	x.nocrypt = in_stm;			/* an object in an object stream is not encrypted on its own */
	o = pa_alloc(d, d->cur, sizeof(POBJ));
	if (!o) {
		lex_done(&x);
		return NULL;
	}
	if (!in_stm) {
		/* N G obj */
		TOK a, b, c;

		if (lex_token(&x, &a) != T_INT || lex_token(&x, &b) != T_INT || lex_token(&x, &c) != T_KEY ||
		    c.n < 3 || memcmp(c.s, "obj", 3) != 0 || a.i != num) {
			*trunc = x.trunc;
			lex_done(&x);
			return NULL;
		}
		if (c.n > 3)
			x.p = c.s + 3;
	}
	if (!parse_obj(&x, o)) {
		*trunc = x.trunc;
		lex_done(&x);
		return NULL;
	}
	if (!in_stm && o->t == PO_DICT) {
		const unsigned char *save = x.p;

		if (lex_token(&x, &t) == T_KEY && t.n == 6 && memcmp(t.s, "stream", 6) == 0) {
			const unsigned char *s = t.s + 6;
			PENT *e;

			if (s < x.e && *s == '\r')
				s++;
			if (s < x.e && *s == '\n')
				s++;
			e = pa_alloc(d, d->cur, (o->a + 1) * (long)sizeof(PENT));
			if (e) {
				memcpy(e, o->u.e, (size_t)o->a * sizeof(PENT));
				e[o->a].key = "$soff";
				e[o->a].v.t = PO_INT;
				e[o->a].v.a = (int32_t)(base + (s - buf));
				o->u.e = e;
				o->a++;
				o->flag = 1;
			}
		} else {
			x.p = save;
			x.trunc = 0;
		}
	}
	*trunc = 0;
	lex_done(&x);
	d->st.objs++;
	return o;
}

/* the most the kept objects may take in the document's arena */
#define KEPT_MAX (64L * 1024)

static POBJ *load_file_obj(PDOC *d, long num, long off, long gen)
{
	/* 2 KB first: a font with its widths is just over 1 KB, and parsed
	   twice from a first read of 1 KB took 270 ms on an ST (EDOS_01.PDF) */
	long want = 2048, got, a0 = 0;
	unsigned char *buf = NULL;
	POBJ *o = NULL;
	int trunc = 0;
	PARENA *page = NULL;
	ACH *k0 = NULL;
	long ku0 = 0;

	for (;;) {
		PARENA *a = d->cur;
		ACH *h0 = a->head;
		long u0 = h0 ? h0->used : 0;

		buf = pdf_malloc(d, want);
		if (!buf)
			break;
		got = pdf_rd(d, off, buf, want);
		o = parse_indirect(d, buf, got, off + got >= d->size, num, gen, off, 0, &trunc);
		pdf_free(d, buf, want);
		if (o || !trunc || off + got >= d->size || want >= (8L << 20))
			break;
		/* cut off: what it built dropped, and more read. One past 4 KB is
		   parsed into the document's arena and kept (pdf_get), while
		   there is room */
		pa_back(d, a, h0, u0);
		want = want == 2048 ? 4096 : want * 4;	/* 4 KB, 16 KB, ... as before */
		if (want >= 16384 && !page && d->cur == &d->a && !d->skip_kids && d->nkept < 8 && d->kept_bytes < KEPT_MAX) {
			page = d->cur;
			d->cur = &d->doc_a;
			a0 = d->doc_a.total;
			k0 = d->doc_a.head;
			ku0 = k0 ? k0->used : 0;
		}
	}
	if (page) {
		d->cur = page;
		if (!o)
			pa_back(d, &d->doc_a, k0, ku0);	/* damaged: nothing of it stays */
		else {
			d->kept[d->nkept].num = (int32_t)num;
			d->kept[d->nkept++].o = o;
			d->kept_bytes += d->doc_a.total - a0;
		}
	}
	return o;
}

/* the object stream n: decoded, with its table of (number, offset) pairs */
static int objstm_load(PDOC *d, long snum)
{
	POBJ *so;
	SBUF b;
	long n, first, i;
	const char *term = NULL;
	LEX x;
	TOK t;

	if (d->os_num == snum && d->os_buf)
		return 1;
	pdf_free(d, d->os_buf, d->os_len);
	pdf_free(d, d->os_off, d->os_n * 2 * (long)sizeof(long));
	d->os_buf = NULL;
	d->os_off = NULL;
	d->os_num = -1;
	so = pdf_get(d, snum);
	if (!so || so->t != PO_DICT || !so->flag)
		return 0;
	n = pd_int(d, so, "N", 0);
	first = pd_int(d, so, "First", 0);
	if (n <= 0 || n > 100000 || first <= 0)
		return 0;
	sb_init(&b);
	if (pdf_stream(d, so, snum, &b, &term, 64L << 20) != 0 || term) {
		sb_free(&b);
		return 0;
	}
	d->st.objstm_loaded++;
	d->st.objstm_bytes += b.len;
	d->os_buf = pdf_malloc(d, b.len + 1);
	if (!d->os_buf) {
		sb_free(&b);
		return 0;
	}
	memcpy(d->os_buf, b.s, (size_t)b.len);
	d->os_buf[b.len] = 0;
	d->os_len = b.len + 1;
	sb_free(&b);
	d->os_off = pdf_malloc(d, n * 2 * (long)sizeof(long));
	if (!d->os_off)
		return 0;
	lex_init(&x, d, d->cur, d->os_buf, b.len, 1);
	for (i = 0; i < n; i++) {
		TOK a, o;

		if (lex_token(&x, &a) != T_INT || lex_token(&x, &o) != T_INT)
			break;
		d->os_off[2 * i] = a.i;
		d->os_off[2 * i + 1] = first + o.i;
	}
	(void)t;
	lex_done(&x);
	d->os_n = i;
	d->os_first = first;
	d->os_num = snum;
	return 1;
}

/* ------------------------------------------------------------------ the table's entries */
#define XE_BIG 0x3FFFFFFFUL		/* below the kind: the entry is in xx */

static int xx_cmp(const void *a, const void *b)
{
	const PXX *x = a, *y = b;

	return x->num < y->num ? -1 : x->num > y->num;
}

/* num's entry in the sorted side list, or NULL */
static PXX *xx_find(PDOC *d, long num)
{
	long lo = 0, hi = d->nxx - 1;

	while (lo <= hi) {
		long m = (lo + hi) >> 1;

		if (d->xx[m].num == num)
			return &d->xx[m];
		if (d->xx[m].num < num)
			lo = m + 1;
		else
			hi = m - 1;
	}
	return NULL;
}

void xe_put(PDOC *d, long num, int t, uint32_t a, uint16_t b)
{
	uint32_t v = (uint32_t)t << 30;

	if (t == 1 && b == 0 && a < XE_BIG)
		v |= a;
	else if (t == 2 && a < (1UL << 18))
		v |= a << 12 | (b < 4095 ? b : 4095);	/* the index a hint: past 4094, found by search */
	else if (t == 1 || t == 2) {
		long k;

		/* a number already there takes its place (a newer section, a rebuild) */
		for (k = d->nxx - 1; k >= 0 && d->xx_sorted == 0; k--)
			if (d->xx[k].num == num)
				break;
		if (d->xx_sorted || k < 0) {
			PXX *f = d->xx_sorted ? xx_find(d, num) : NULL;

			if (f)
				k = f - d->xx;
			else {
				if (d->nxx == d->capxx) {
					long cap = d->capxx ? d->capxx * 2 : 16;
					PXX *q = pdf_realloc(d, d->xx, d->capxx * (long)sizeof(PXX), cap * (long)sizeof(PXX));

					if (!q)
						return;
					d->xx = q;
					d->capxx = cap;
				}
				k = d->nxx++;
				d->xx_sorted = 0;
			}
		}
		d->xx[k].num = (int32_t)num;
		d->xx[k].a = a;
		d->xx[k].b = b;
		d->xx[k].t = (uint8_t)t;
		v |= XE_BIG;
	}
	d->xe[num] = v;
}

int xe_get(PDOC *d, long num, uint32_t *a, uint16_t *b)
{
	uint32_t v = d->xe[num];
	int t = (int)(v >> 30);

	if ((v & XE_BIG) == XE_BIG && (t == 1 || t == 2)) {
		PXX *f;

		if (!d->xx_sorted) {
			/* a number set twice before the sort: the later one is what
			   counts, and xe_put replaced it in place */
			qsort(d->xx, (size_t)d->nxx, sizeof *d->xx, xx_cmp);
			d->xx_sorted = 1;
		}
		f = xx_find(d, num);
		if (!f)
			return 0;
		*a = f->a;
		*b = f->b;
		return t;
	}
	if (t == 1) {
		*a = v & XE_BIG;
		*b = 0;
	} else if (t == 2) {
		*a = (v & XE_BIG) >> 12;
		*b = (uint16_t)(v & 4095);
	} else {
		*a = 0;
		*b = 0;
	}
	return t;
}

void xe_clear(PDOC *d)
{
	if (d->xe)
		memset(d->xe, 0, (size_t)d->nxe * sizeof(PXE));
	d->nxx = 0;
	d->xx_sorted = 0;
}

POBJ *pdf_get(PDOC *d, long num)
{
	POBJ *o;
	uint32_t ea;
	uint16_t eb;
	int et;

	if (num < 0 || num >= d->nxe)
		return NULL;
	o = cache_find(d, num);
	if (o)
		return o == &unloadable ? NULL : o;
	{
		int k;

		for (k = 0; k < d->nkept; k++)
			if (d->kept[k].num == num)
				return d->kept[k].o;
	}
	et = xe_get(d, num, &ea, &eb);
	if (et == 1) {
		o = load_file_obj(d, num, ea, eb);
	} else if (et == 2) {
		long i, idx = eb;
		int trunc = 0;

		if (!objstm_load(d, ea))
			return NULL;
		if (idx >= d->os_n || d->os_off[2 * idx] != num) {	/* the index is a hint */
			for (idx = 0; idx < d->os_n; idx++)
				if (d->os_off[2 * idx] == num)
					break;
			if (idx >= d->os_n)
				return NULL;
		}
		i = d->os_off[2 * idx + 1];
		if (i < 0 || i >= d->os_len - 1)
			return NULL;
		o = parse_indirect(d, d->os_buf + i, d->os_len - 1 - i, 1, num, 0, 0, 1, &trunc);
	}
	/* one that can't be loaded is remembered too (for the page): parsed
	   again at each use, its half-built pieces stayed in the arena each time */
	cache_put(d, num, o ? o : &unloadable);
	return o;
}

void pdf_page_reset(PDOC *d)
{
	pa_free(d, &d->a);
	memset(d->cache, 0, sizeof d->cache);
	d->cur = &d->a;
}

/* every object of object stream snum, where nothing else says what it is
   (the rebuild of a damaged table) */
int objstm_register(PDOC *d, long snum)
{
	long i;

	if (!objstm_load(d, snum))
		return 0;
	for (i = 0; i < d->os_n; i++) {
		long num = d->os_off[2 * i];

		if (num < 0 || num > 8000000)
			continue;
		if (num >= d->nxe) {
			PXE *q = pdf_realloc(d, d->xe, d->nxe * (long)sizeof(PXE), (num + 1) * (long)sizeof(PXE));

			if (!q)
				continue;
			memset(q + d->nxe, 0, (size_t)(num + 1 - d->nxe) * sizeof(PXE));
			d->xe = q;
			d->nxe = num + 1;
		}
		if (xe_kind(d, num) == 0)
			xe_put(d, num, 2, (uint32_t)snum, (uint16_t)i);
	}
	return 1;
}

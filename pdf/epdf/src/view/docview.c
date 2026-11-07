/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#include <stdlib.h>
#include <string.h>
#include "docview.h"

/* a picture's line: its object and size in points, and how it is turned
   (*flip, DV_FLIPV / DV_FLIPH: 'v' and 'h' after the size); 0 when it
   isn't one */
static int pic_of(const char *s, long n, long *obj, long *w, long *h, int *flip)
{
	long v[3] = { 0, 0, 0 };
	int k = 0;
	long i;

	if (n < 4 || s[0] != 0x7F)
		return 0;
	*flip = 0;
	for (i = 1; i < n && k < 3; i++) {
		if (s[i] >= '0' && s[i] <= '9' && !*flip)
			v[k] = v[k] * 10 + (s[i] - '0');
		else if ((s[i] == ' ' || s[i] == 'x') && !*flip)
			k++;
		else if (k == 2 && s[i] == 'v')
			*flip |= DV_FLIPV;
		else if (k == 2 && s[i] == 'h')
			*flip |= DV_FLIPH;
		else
			return 0;
	}
	if (k < 2 || v[1] <= 0 || v[2] <= 0)
		return 0;
	*obj = v[0];
	*w = v[1];
	*h = v[2];
	return 1;
}

/* the size a picture shows at, and the rows it takes */
static long pic_rows(const DOCVIEW *v, long w, long h, int *pw, int *ph)
{
	long x, y;

	if (v->pic_w <= 0 || v->pic_ch <= 0)
		return 1;
	x = w < v->pic_w ? w : v->pic_w;	/* a point a pixel, at most the width */
	y = x * h / w * 16 / (v->pic_aspect > 0 ? v->pic_aspect : 16);
	if (y < 1)
		y = 1;
	*pw = (int)x;
	*ph = (int)y;
	return (y + v->pic_ch - 1) / v->pic_ch;
}

/* page n laid out (its text copied: the store's pointer lasts only till
   its next call), or NULL when it isn't extracted or memory is short */
static TV *laid(DOCVIEW *v, long n)
{
	int i, best = 0;
	const char *s;
	long len, k;
	char *t, *m;

	v->tick++;
	for (i = 0; i < DV_LAID; i++)
		if (v->lay[i].page == n) {
			v->lay[i].used = v->tick;
			return &v->lay[i].tv;
		}
	if (!(s = ts_get(v->ts, n, &len)))
		return NULL;
	for (i = 1; i < DV_LAID; i++)
		if (v->lay[i].used < v->lay[best].used)
			best = i;
	/* the copy at the stored text's byte parity: memcpy moves longs
	   between two even addresses or two odd ones, else a byte at a time
	   (13 ms a page on an ST, gemprof.py) */
	if (!(m = malloc((size_t)(len > 0 ? len : 1) + 1)))
		return NULL;
	t = m + ((unsigned long)s & 1);
	/* the soft breaks: a space, or a line end showing the page's lines
	   (copied whole, then each CR found and changed: a character at a
	   time with a long index was 54 ms a page on an ST, gemprof.py) */
	if (len > 0)
		memcpy(t, s, (size_t)len);
	for (k = 0; k < len; k++) {
		long f = tv_find(t + k, len - k, '\r');

		if (f < 0)
			break;
		k += f;
		t[k] = v->lines_mode ? '\n' : ' ';
	}
	/* the page's last line end makes no empty row */
	while (len > 0 && t[len - 1] == '\n')
		len--;
	free(v->lay[best].mem);
	free(v->lay[best].lrow);
	v->lay[best].lrow = NULL;
	tv_free(&v->lay[best].tv);
	v->lay[best].mem = m;
	v->lay[best].text = t;
	v->lay[best].len = len;
	v->lay[best].page = n;
	v->lay[best].used = v->tick;
	tv_init(&v->lay[best].tv);
	if (tv_layout(&v->lay[best].tv, t, len, v->cols)) {
		v->lay[best].page = -1;
		return NULL;
	}
	/* where each line's rows start: a picture's line takes its rows */
	{
		TV *tv = &v->lay[best].tv;
		long *lr = malloc((size_t)(tv->nlines + 1) * sizeof *lr), i, ln, o, w, h;
		const char *ls;
		int pw, ph, fl;

		if (!lr) {
			v->lay[best].page = -1;
			return NULL;
		}
		lr[0] = 0;
		for (i = 0; i < tv->nlines; i++) {
			ls = tv_line(tv, i, &ln);
			lr[i + 1] = lr[i] + (ls && pic_of(ls, ln, &o, &w, &h, &fl) ? pic_rows(v, w, h, &pw, &ph) : 1);
		}
		v->lay[best].lrow = lr;
	}
	return &v->lay[best].tv;
}

/* the laid page's slot (after laid() gave its TV) */
static long *lrow_of(DOCVIEW *v, TV *tv)
{
	int i;

	for (i = 0; i < DV_LAID; i++)
		if (&v->lay[i].tv == tv)
			return v->lay[i].lrow;
	return NULL;
}

static long count(DOCVIEW *v, long n)
{
	TV *tv = ts_have(v->ts, n) ? laid(v, n) : NULL;
	long *lr = tv ? lrow_of(v, tv) : NULL;

	return 1 + (tv && lr && lr[tv->nlines] > 0 ? lr[tv->nlines] : 1);
}

int dv_init(DOCVIEW *v, TSTORE *ts, long npages, int cols, int lines_mode)
{
	long i;

	memset(v, 0, sizeof *v);
	v->ts = ts;
	v->npages = npages;
	v->cols = cols < 1 ? 1 : cols;
	v->lines_mode = lines_mode;
	for (i = 0; i < DV_LAID; i++)
		v->lay[i].page = -1;
	v->sel_p0 = -1;
	v->rows = malloc((size_t)(npages > 0 ? npages : 1) * sizeof *v->rows);
	v->stale = calloc((size_t)(npages > 0 ? npages : 1), 1);
	if (!v->rows || !v->stale) {
		free(v->rows);
		free(v->stale);
		v->rows = NULL;
		v->stale = NULL;
		return -1;
	}
	for (i = 0; i < npages; i++)
		v->rows[i] = count(v, i);
	v->total = 0;
	for (i = 0; i < npages; i++)
		v->total += v->rows[i];
	return 0;
}

void dv_free(DOCVIEW *v)
{
	int i;

	for (i = 0; i < DV_LAID; i++) {
		free(v->lay[i].mem);
		free(v->lay[i].lrow);
		tv_free(&v->lay[i].tv);
	}
	free(v->rows);
	free(v->stale);
	memset(v, 0, sizeof *v);
}

static void forget(DOCVIEW *v, long n)
{
	int i;

	for (i = 0; i < DV_LAID; i++)
		if (v->lay[i].page == n || n < 0)
			v->lay[i].page = -1;
}

void dv_page_ready(DOCVIEW *v, long n)
{
	long r;

	if (n < 0 || n >= v->npages)
		return;
	forget(v, n);
	if (v->stale[n]) {
		v->stale[n] = 0;
		v->nstale--;
	}
	r = count(v, n);
	v->total += r - v->rows[n];
	v->rows[n] = r;
	if (n == v->top_page && v->top_row >= r)
		v->top_row = r - 1;
}

void dv_set_cols(DOCVIEW *v, int cols, int lines_mode)
{
	long i, old = v->cols;

	cols = cols < 1 ? 1 : cols;
	if (cols == v->cols && lines_mode == v->lines_mode)
		return;
	v->cols = cols;
	v->lines_mode = lines_mode;
	forget(v, -1);
	v->total = 0;
	v->nstale = 0;
	for (i = 0; i < v->npages; i++) {
		if (ts_have(v->ts, i)) {
			/* text the old width took r rows takes about r * old / cols */
			v->rows[i] = 1 + ((v->rows[i] - 1) * old + cols - 1) / cols;
			v->stale[i] = 1;
			v->nstale++;
		}
		v->total += v->rows[i];
	}
	if (v->top_page < v->npages && v->top_row >= v->rows[v->top_page])
		v->top_row = v->rows[v->top_page] - 1;
}

void dv_set_pics(DOCVIEW *v, int w, int ch, int aspect)
{
	long i;

	if (w == v->pic_w && ch == v->pic_ch && aspect == v->pic_aspect)
		return;
	v->pic_w = w;
	v->pic_ch = ch;
	v->pic_aspect = aspect;
	forget(v, -1);
	for (i = 0; i < v->npages; i++)
		if (ts_have(v->ts, i) && !v->stale[i]) {
			v->stale[i] = 1;
			v->nstale++;
		}
}

int dv_recount(DOCVIEW *v, int max)
{
	long d, n;

	/* outwards from the top: what shows first */
	for (d = 0; v->nstale > 0 && max > 0 && d < v->npages; d++) {
		long cand[2];
		int k;

		cand[0] = v->top_page + d;
		cand[1] = d ? v->top_page - d : -1;
		for (k = 0; k < 2; k++) {
			n = cand[k];
			if (n < 0 || n >= v->npages || !v->stale[n])
				continue;
			dv_page_ready(v, n);
			max--;
		}
	}
	return v->nstale > 0;
}

int dv_recount_view(DOCVIEW *v, long window)
{
	long n = v->top_page, have;

	/* the top page, then those after it while the window isn't full: a
	   page each, not rows + 2 pages (24 of a book on an ST's high mode,
	   175 ms each: View > The page's lines 4.3 s on ST, 1 Nov 2026) */
	if (n < 0 || n >= v->npages)
		return v->nstale > 0;
	if (v->stale[n])
		dv_page_ready(v, n);
	have = v->rows[n] - v->top_row;
	while (have < window && ++n < v->npages) {
		if (v->stale[n])
			dv_page_ready(v, n);
		have += v->rows[n];
	}
	return v->nstale > 0;
}

int dv_row(DOCVIEW *v, long r, int *kind, const char **s, long *n, long *page, long *off)
{
	long p = v->top_page, row = v->top_row + r;
	TV *tv;

	while (p < v->npages && row >= v->rows[p]) {
		row -= v->rows[p];
		p++;
	}
	if (p >= v->npages)
		return 0;
	*page = p;
	*off = -1;
	if (row == 0) {
		*kind = DV_HEAD;
		*s = "";
		*n = 0;
		return 1;
	}
	tv = ts_have(v->ts, p) ? laid(v, p) : NULL;
	if (!tv) {
		*kind = DV_WAIT;
		*s = "...";
		*n = 3;
		return 1;
	}
	{
		long *lr = lrow_of(v, tv), lo = 0, hi = tv->nlines - 1, r = row - 1, obj, w, h;
		int fl;

		*kind = DV_TEXT;
		if (lr && tv->nlines > 0) {
			/* the line whose rows hold r */
			while (lo < hi) {
				long mid = (lo + hi + 1) / 2;

				if (lr[mid] <= r)
					lo = mid;
				else
					hi = mid - 1;
			}
			*s = tv_line(tv, lo, n);
			if (*s && pic_of(*s, *n, &obj, &w, &h, &fl)) {
				*kind = DV_PIC;
				v->pic.obj = obj;
				v->pic.flip = fl;
				v->pic.page = p;
				v->pic.rows = pic_rows(v, w, h, &v->pic.w, &v->pic.h);
				v->pic.row = r - lr[lo];
				*off = (long)(*s - (const char *)tv->text);
				*n = 0;
				return 1;
			}
			if (r != lr[lo])
				*s = NULL;
		} else {
			*s = tv_line(tv, r, n);
		}
	}
	if (!*s) {
		*s = "";
		*n = 0;
	} else {
		*off = (long)(*s - (const char *)tv->text);
	}
	return 1;
}

long dv_row_of(DOCVIEW *v, long n, long off)
{
	TV *tv = n >= 0 && n < v->npages && ts_have(v->ts, n) ? laid(v, n) : NULL;
	long lo = 0, hi;

	long *lr;

	if (!tv || tv->nlines == 0)
		return tv ? 1 : 0;
	/* the last line that starts at or before off */
	hi = tv->nlines - 1;
	while (lo < hi) {
		long mid = (lo + hi + 1) / 2;

		if ((long)tv->start[mid] <= off)
			lo = mid;
		else
			hi = mid - 1;
	}
	lr = lrow_of(v, tv);
	return (lr ? lr[lo] : lo) + 1;
}

void dv_show_row(DOCVIEW *v, long n, long row, long above, long window)
{
	long r = 0, i;

	for (i = 0; i < n && i < v->npages; i++)
		r += v->rows[i];
	r += row - above;
	if (r < 0)
		r = 0;
	dv_set_top_abs(v, r, window);
}

void dv_select(DOCVIEW *v, long p0, long o0, long p1, long o1)
{
	if (p0 >= 0 && (p1 < p0 || (p1 == p0 && o1 < o0))) {
		long t;

		t = p0, p0 = p1, p1 = t;
		t = o0, o0 = o1, o1 = t;
	}
	v->sel_p0 = p0;
	v->sel_o0 = o0;
	v->sel_p1 = p1;
	v->sel_o1 = o1;
}

int dv_sel_cols(const DOCVIEW *v, long page, long off, long n, long *a, long *b)
{
	long s, e;

	if (v->sel_p0 < 0 || off < 0 || page < v->sel_p0 || page > v->sel_p1)
		return 0;
	s = page == v->sel_p0 ? v->sel_o0 - off : 0;
	e = page == v->sel_p1 ? v->sel_o1 - off : n;
	if (s < 0)
		s = 0;
	if (e > n)
		e = n;
	if (e <= s)
		return 0;
	*a = s;
	*b = e;
	return 1;
}

long dv_top_abs(const DOCVIEW *v)
{
	long i, r = 0;

	for (i = 0; i < v->top_page && i < v->npages; i++)
		r += v->rows[i];
	return r + v->top_row;
}

void dv_set_top_abs(DOCVIEW *v, long r, long window)
{
	long p = 0;

	if (r > v->total - window)
		r = v->total - window;
	if (r < 0)
		r = 0;
	while (p < v->npages - 1 && r >= v->rows[p]) {
		r -= v->rows[p];
		p++;
	}
	v->top_page = p;
	v->top_row = r;
}

long dv_scroll(DOCVIEW *v, long d, long window)
{
	long before = dv_top_abs(v), to = before + d;

	/* down no further than a full window from the end; a page jump may
	   have gone past that, and the view then stays */
	if (d > 0 && to > v->total - window)
		to = before > v->total - window ? before : v->total - window;
	if (d <= 0 && to > before)
		to = before;
	if (to < 0)
		to = 0;
	if (to == before)
		return 0;
	dv_set_top_abs(v, to, d > 0 || to <= v->total - window ? window : 0);
	return dv_top_abs(v) - before;
}

void dv_goto_page(DOCVIEW *v, long n)
{
	if (n < 0)
		n = 0;
	if (n >= v->npages)
		n = v->npages - 1;
	v->top_page = n;
	v->top_row = 0;
}

long dv_page_at_top(const DOCVIEW *v)
{
	return v->top_page;
}

int dv_page_pic(DOCVIEW *v, long n, long *pos, DVPIC *pic)
{
	const char *s;
	long len, p, e, obj, w, h;
	int fl;

	if (!ts_pics(v->ts, n) || !(s = ts_peek(v->ts, n, &len)))
		return 0;
	for (p = *pos; p < len; p += e + 1) {
		long f = tv_find(s + p, len - p, 0x7F);

		if (f < 0)
			break;
		p += f;
		e = tv_find(s + p, len - p, '\n');
		if (e < 0)
			e = len - p;
		if ((p == 0 || s[p - 1] == '\n') && pic_of(s + p, e, &obj, &w, &h, &fl)) {
			*pos = p + e + 1;
			pic->obj = obj;
			pic->flip = fl;
			pic->page = n;
			pic->rows = pic_rows(v, w, h, &pic->w, &pic->h);
			pic->row = 0;
			return 1;
		}
	}
	*pos = len;
	return 0;
}

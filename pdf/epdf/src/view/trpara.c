/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * trpara.c - a page's text cut into paragraphs for a translation, and the
 * translation put back (trpara.h). A paragraph is lines joined by soft
 * breaks (CR) up to a line ending in LF, sent as one line, its soft
 * breaks as spaces. A picture's line, an empty line and a line with a Tab
 * (a table's) are kept where they are, never sent. The translation comes
 * back a paragraph a line (or, for a paragraph of short lines, its lines
 * as they were: the Gateway decides), each put back ending in LF.
 */
#include <stdlib.h>
#include <string.h>
#include "charset.h"
#include "trpara.h"

void (*tp_turn)(void);

static int add_seg(TPARA *p, int *cap, long a, long b, int send)
{
	if (p->nseg == *cap) {
		int nc = *cap ? *cap * 2 : 32;
		TPSEG *s = realloc(p->seg, (size_t)nc * sizeof *s);

		if (!s)
			return -1;
		p->seg = s;
		*cap = nc;
	}
	if (p->nseg > 0 && !send && !p->seg[p->nseg - 1].send && p->seg[p->nseg - 1].b == a)
		p->seg[p->nseg - 1].b = b;	/* (kept lines one after another: one piece) */
	else {
		p->seg[p->nseg].a = a;
		p->seg[p->nseg].b = b;
		p->seg[p->nseg].send = send;
		p->nseg++;
	}
	p->nsend += send;
	return 0;
}

/* the line from i: its end (the CR or LF, or n) */
static long line_end(const char *t, long n, long i)
{
	while (i < n && t[i] != '\r' && t[i] != '\n')
		i++;
	return i;
}

static int kept_line(const char *t, long a, long e)
{
	long i;

	if (a == e || (unsigned char)t[a] == 0x7F)
		return 1;
	for (i = a; i < e; i++)
		if (t[i] == '\t')
			return 1;
	for (i = a; i < e; i++)
		if (t[i] != ' ')
			return 0;
	return 1;			/* (spaces only) */
}

static int grow(char **buf, long *cap, long need)
{
	if (need <= *cap)
		return 0;
	{
		long nc = *cap ? *cap : 1024;
		char *b;

		while (nc < need)
			nc *= 2;
		if (!(b = realloc(*buf, (size_t)nc)))
			return -1;
		*buf = b;
		*cap = nc;
	}
	return 0;
}

int tp_split(const char *t, long n, TPARA *p)
{
	long i = 0, cap = 0;
	int scap = 0;

	memset(p, 0, sizeof *p);
	while (i < n) {
		long a = i, e = line_end(t, n, i);

		if (tp_turn && p->nseg && (p->nseg & 3) == 0)
			tp_turn();

		if (kept_line(t, a, e)) {
			/* a line kept as it is, its end too (a soft break after one
			   such: kept also) */
			i = e < n ? e + 1 : e;
			if (add_seg(p, &scap, a, i, 0))
				goto bad;
			continue;
		}
		/* a paragraph: on through soft breaks, to a line's end, a kept
		   line or the text's end */
		for (;;) {
			if (e >= n || t[e] == '\n') {
				i = e < n ? e + 1 : e;
				break;
			}
			/* a soft break: the next line is the paragraph's, unless it
			   is one kept */
			{
				long na = e + 1, ne = line_end(t, n, na);

				if (na >= n || kept_line(t, na, ne)) {
					i = na;
					break;
				}
				e = ne;
			}
		}
		if (add_seg(p, &scap, a, i, 1))
			goto bad;
		{
			/* its words in UTF-8, the soft breaks spaces, its end left off */
			long b = i, k, ul;
			char *u;

			while (b > a && (t[b - 1] == '\n' || t[b - 1] == '\r'))
				b--;
			if (!(u = atari_to_utf8(t + a, b - a, &ul)))
				goto bad;
			if (grow(&p->utf8, &cap, p->ulen + ul + 3)) {
				free(u);
				goto bad;
			}
			if (p->ulen)
				p->utf8[p->ulen++] = '\n', p->utf8[p->ulen++] = '\n';	/* (an empty line between) */
			for (k = 0; k < ul; k++)
				p->utf8[p->ulen++] = u[k] == '\r' ? ' ' : u[k];
			free(u);
		}
	}
	if (p->utf8)
		p->utf8[p->ulen] = 0;
	return 0;
bad:
	tp_free(p);
	return -1;
}

char *tp_join(const char *t, long n, const TPARA *p, const char *u, long ulen, long *outlen)
{
	char *out = NULL;
	long cap = 0, len = 0, pos = 0;
	int s;

	(void)n;
	for (s = 0; s < p->nseg; s++) {
		const TPSEG *g = &p->seg[s];

		if (tp_turn && (s & 3) == 3)
			tp_turn();

		if (!g->send) {
			if (grow(&out, &cap, len + (g->b - g->a) + 1))
				goto bad;
			memcpy(out + len, t + g->a, (size_t)(g->b - g->a));
			len += g->b - g->a;
			continue;
		}
		{
			/* the next paragraph of the answer: to an empty line, or its end */
			long e = pos, k, al;
			char *at;

			while (pos < ulen && u[pos] == '\n')
				pos++;			/* (empty lines more than one) */
			if (pos >= ulen)
				goto bad;		/* fewer paragraphs than sent */
			e = pos;
			while (e < ulen && !(u[e] == '\n' && e + 1 < ulen && u[e + 1] == '\n'))
				e++;
			while (e > pos && (u[e - 1] == '\n' || u[e - 1] == '\r'))
				e--;
			if (!(at = cs_to_atari(u + pos, e - pos, CS_UTF8, &al)))
				goto bad;
			if (grow(&out, &cap, len + al + 2)) {
				free(at);
				goto bad;
			}
			for (k = 0; k < al; k++)
				if (at[k] != '\r')
					out[len++] = at[k];	/* (a short line's own LF kept) */
			out[len++] = '\n';
			free(at);
			pos = e;
		}
	}
	while (pos < ulen && (u[pos] == '\n' || u[pos] == '\r' || u[pos] == ' '))
		pos++;
	if (pos < ulen)
		goto bad;			/* more paragraphs than sent */
	if (!out && !(out = malloc(1)))
		return NULL;
	*outlen = len;
	return out;
bad:
	free(out);
	return NULL;
}

void tp_free(TPARA *p)
{
	free(p->seg);
	free(p->utf8);
	memset(p, 0, sizeof *p);
}

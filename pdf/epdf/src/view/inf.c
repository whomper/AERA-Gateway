/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#include <stdlib.h>
#include <string.h>
#include "inf.h"

void inf_init(INF *f)
{
	memset(f, 0, sizeof *f);
}

void inf_free(INF *f)
{
	long i;

	for (i = 0; i < f->n; i++)
		free(f->line[i]);
	free(f->line);
	inf_init(f);
}

static char *dup_n(const char *s, long n)
{
	char *p = malloc((size_t)n + 1);

	if (p) {
		memcpy(p, s, (size_t)n);
		p[n] = '\0';
	}
	return p;
}

/* line s inserted before line at */
static int insert(INF *f, long at, const char *s, long n)
{
	char *p;

	if (f->n == f->cap) {
		long cap = f->cap ? f->cap * 2 : 32;
		char **l = realloc(f->line, (size_t)cap * sizeof *l);

		if (!l)
			return -1;
		f->line = l;
		f->cap = cap;
	}
	if (!(p = dup_n(s, n)))
		return -1;
	memmove(f->line + at + 1, f->line + at, (size_t)(f->n - at) * sizeof *f->line);
	f->line[at] = p;
	f->n++;
	return 0;
}

int inf_parse(INF *f, const char *text, long len)
{
	long a = 0, b;

	inf_free(f);
	while (a < len) {
		for (b = a; b < len && text[b] != '\n' && text[b] != '\r'; b++)
			;
		if (insert(f, f->n, text + a, b - a))
			return -1;
		if (b < len && text[b] == '\r')
			b++;
		if (b < len && text[b] == '\n')
			b++;
		a = b;
	}
	return 0;
}

/* the line holding NAME=, or -1 */
static long find(const INF *f, const char *name)
{
	size_t k = strlen(name);
	long i;

	for (i = 0; i < f->n; i++)
		if (!strncmp(f->line[i], name, k) && f->line[i][k] == '=')
			return i;
	return -1;
}

const char *inf_get(const INF *f, const char *name)
{
	long i = find(f, name);

	return i < 0 ? NULL : f->line[i] + strlen(name) + 1;
}

int inf_set(INF *f, const char *name, const char *value, const char *note)
{
	long i = find(f, name), kn = (long)strlen(name), vn = (long)strlen(value);
	char *p = malloc((size_t)(kn + vn) + 2);

	if (!p)
		return -1;
	memcpy(p, name, (size_t)kn);
	p[kn] = '=';
	memcpy(p + kn + 1, value, (size_t)vn + 1);
	if (i >= 0) {
		free(f->line[i]);
		f->line[i] = p;
		return 0;
	}
	/* a new paragraph: an empty line before it unless there is one */
	if (f->n > 0 && f->line[f->n - 1][0] != '\0' && insert(f, f->n, "", 0))
		goto fail;
	if (note) {
		char *t = malloc(strlen(note) + 3);

		if (!t)
			goto fail;
		strcpy(t, "; ");
		strcat(t, note);
		i = insert(f, f->n, t, (long)strlen(t));
		free(t);
		if (i)
			goto fail;
	}
	if (insert(f, f->n, p, kn + vn + 1) || insert(f, f->n, "", 0))
		goto fail;
	free(p);
	return 0;
fail:
	free(p);
	return -1;
}

char *inf_text(const INF *f, long *len)
{
	long i, n = 0;
	char *t, *q;

	for (i = 0; i < f->n; i++)
		n += (long)strlen(f->line[i]) + 2;
	if (!(t = malloc((size_t)n + 1)))
		return NULL;
	q = t;
	for (i = 0; i < f->n; i++) {
		size_t k = strlen(f->line[i]);

		memcpy(q, f->line[i], k);
		q += k;
		*q++ = '\r';
		*q++ = '\n';
	}
	*q = '\0';
	*len = n;
	return t;
}

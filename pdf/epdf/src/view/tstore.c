/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#include <stdlib.h>
#include <string.h>
#include "plat.h"
#include "tstore.h"

int ts_open(TSTORE *s, long npages, long budget, const char *path)
{
	long i;

	memset(s, 0, sizeof *s);
	s->pg = malloc((size_t)(npages > 0 ? npages : 1) * sizeof *s->pg);
	s->pool = malloc((size_t)(budget > 0 ? budget : 1));
	if (!s->pg || !s->pool) {
		free(s->pg);
		free(s->pool);
		memset(s, 0, sizeof *s);
		return -1;
	}
	for (i = 0; i < npages; i++) {
		s->pg[i].len = -1;
		s->pg[i].in_ram = 0;
		s->pg[i].pics = 0;
	}
	s->npages = npages;
	s->budget = budget;
	for (i = 0; i < TS_SLOTS; i++)
		s->slot[i].page = -1;
	s->fh = -1;
	if (path && strlen(path) < sizeof s->path)
		strcpy(s->path, path);
	return 0;
}

void ts_close(TSTORE *s)
{
	int i;

	for (i = 0; i < TS_SLOTS; i++)
		free(s->slot[i].s);
	if (s->fh >= 0)
		pf_close(s->fh);
	if (s->path[0] && s->fh >= 0)
		pf_remove(s->path);
	free(s->pg);
	free(s->pool);
	memset(s, 0, sizeof *s);
}

/* the cache file, made and opened for reading and writing the first
   time it is wanted, then kept open until the document closes; -1 when
   it can't be */
static int file(TSTORE *s)
{
	int h;

	if (s->fh >= 0 || !s->path[0])
		return s->fh;
	h = pf_open(s->path, PF_WRITE);		/* made empty */
	if (h < 0)
		return -1;
	pf_close(h);
	s->fh = pf_open(s->path, PF_UPDATE);
	s->opens++;
	if (s->fh < 0)
		pf_remove(s->path);
	return s->fh;
}

/* len bytes at the end of the cache file: 0, or -1 */
static int append(TSTORE *s, const void *p, long len)
{
	int h = file(s);

	if (h < 0)
		return -1;
	s->writes++;
	if (pf_seek(h, s->file_len) != s->file_len || pf_write(h, p, len) != len)
		return -1;
	return 0;
}

/* the pool written at the end of the cache file in one write; its pages
   then live there */
static int spill(TSTORE *s)
{
	long i;

	if (!s->path[0])
		return -1;
	if (s->pool_len == 0)
		return 0;
	if (append(s, s->pool, s->pool_len) != 0)
		return -1;
	for (i = 0; i < s->npages; i++)
		if (s->pg[i].in_ram && s->pg[i].len >= 0) {
			s->pg[i].off += s->file_len;
			s->pg[i].in_ram = 0;
		}
	s->file_len += s->pool_len;
	s->pool_len = 0;
	return 0;
}

long ts_shrink(TSTORE *s, long keep)
{
	long back = 0;
	int i;

	if (!s->path[0] || (s->pool_len && spill(s) != 0))
		return 0;
	for (i = 0; i < TS_SLOTS; i++)
		if (s->slot[i].s) {
			back += s->slot[i].len;
			free(s->slot[i].s);
			s->slot[i].s = NULL;
			s->slot[i].page = -1;
		}
	if (keep < 1)
		keep = 1;
	if (s->budget > keep) {
		char *p;

		free(s->pool);
		p = malloc((size_t)keep);
		back += s->budget - (p ? keep : 0);
		s->pool = p;
		s->budget = p ? keep : 0;
	}
	return back;
}

int ts_put(TSTORE *s, long n, const char *text, long len)
{
	int i;

	if (n < 0 || n >= s->npages || len < 0)
		return -1;
	for (i = 0; i < TS_SLOTS; i++)
		if (s->slot[i].page == n)
			s->slot[i].page = -1;
	if (s->pool_len + len > s->budget && spill(s) != 0 && s->pool_len + len > s->budget) {
		/* no file: as much as memory takes */
		char *p = realloc(s->pool, (size_t)(s->pool_len + len));

		if (!p)
			return -1;
		s->pool = p;
		s->budget = s->pool_len + len;
	}
	if (len > s->budget || !s->pool) {
		/* bigger than the pool: straight to the file */
		if (append(s, text, len) != 0)
			return -1;
		s->pg[n].off = s->file_len;
		s->pg[n].len = len;
		s->pg[n].in_ram = 0;
		s->pg[n].pics = 0;
		s->file_len += len;
		return 0;
	}
	memcpy(s->pool + s->pool_len, text, (size_t)len);
	s->pg[n].off = s->pool_len;
	s->pg[n].len = len;
	s->pg[n].in_ram = 1;
	s->pg[n].pics = 0;
	s->pool_len += len;
	return 0;
}

void ts_set_pics(TSTORE *s, long n, int pics)
{
	if (n >= 0 && n < s->npages)
		s->pg[n].pics = pics != 0;
}

static int own(const TSTORE *s, long n)
{
	return n >= 0 && n < s->npages && s->pg[n].len >= 0;
}

int ts_pics(const TSTORE *s, long n)
{
	if (!own(s, n) && s->under)
		return ts_pics(s->under, n);
	return n >= 0 && n < s->npages && s->pg[n].len > 0 && s->pg[n].pics;
}

const char *ts_peek(const TSTORE *s, long n, long *len)
{
	int i;

	if (!own(s, n))
		return s->under ? ts_peek(s->under, n, len) : NULL;
	*len = s->pg[n].len;
	if (s->pg[n].in_ram)
		return s->pool + s->pg[n].off;
	for (i = 0; i < TS_SLOTS; i++)
		if (s->slot[i].page == n)
			return s->slot[i].s;
	return NULL;
}

int ts_have(const TSTORE *s, long n)
{
	return own(s, n) || (s->under && ts_have(s->under, n));
}

const char *ts_get(TSTORE *s, long n, long *len)
{
	int i, best = 0, h;
	char *p;

	if (!own(s, n))
		return s->under ? ts_get(s->under, n, len) : NULL;
	*len = s->pg[n].len;
	if (s->pg[n].in_ram)
		return s->pool + s->pg[n].off;
	s->tick++;
	for (i = 0; i < TS_SLOTS; i++)
		if (s->slot[i].page == n) {
			s->slot[i].used = s->tick;
			return s->slot[i].s;
		}
	for (i = 1; i < TS_SLOTS; i++)
		if (s->slot[i].page < 0 || s->slot[i].used < s->slot[best].used)
			best = i;
	if (s->slot[best].page < 0)
		for (i = 0; i < TS_SLOTS; i++)
			if (s->slot[i].page < 0) {
				best = i;
				break;
			}
	p = realloc(s->slot[best].s, (size_t)(*len > 0 ? *len : 1));
	if (!p)
		return NULL;
	s->slot[best].s = p;
	s->slot[best].page = -1;
	h = file(s);
	if (h < 0)
		return NULL;
	s->reads++;
	if (pf_seek(h, s->pg[n].off) != s->pg[n].off || pf_read(h, p, *len) != *len)
		return NULL;
	s->slot[best].page = n;
	s->slot[best].len = *len;
	s->slot[best].used = s->tick;
	return p;
}

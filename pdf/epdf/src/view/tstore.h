/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * tstore.h - the text of a document's pages, as they are extracted, in
 * memory up to a budget and beyond it in a cache file (the guidelines'
 * 6.5: gathered in memory, written a buffer at a time). The cache file
 * lasts as long as the document is open, and stays open as long: one
 * open for all its writes and reads (a program's one big file, 6.5),
 * not one a page read back.
 *
 * A page read back from the file is kept in a few slots, so drawing the
 * pages in view reads each once.
 */
#ifndef TSTORE_H
#define TSTORE_H

#define TS_SLOTS 4

typedef struct {
	long off;			/* in the pool when < pool_len and in_ram, else in the file */
	long len;			/* -1: not extracted yet */
	char in_ram;
	char pics;			/* its text holds a picture's line (ts_set_pics) */
} TSPAGE;

typedef struct tstore {
	TSPAGE *pg;
	long npages;
	char *pool;			/* the pages in memory, one after another */
	long pool_len, budget;
	long file_len;			/* what the cache file holds */
	char path[160];			/* the cache file; "" for none: everything stays in memory */
	struct { long page; char *s; long len; unsigned long used; } slot[TS_SLOTS];
	unsigned long tick;
	int fh;				/* the cache file, open; -1 not yet */
	long writes, reads, opens;	/* file writes, reads and opens, for the tests */
	/* a page this store hasn't got is taken from under's (EPDF's
	   translations over the pages as read: View > Translate) */
	struct tstore *under;
} TSTORE;

/* budget: the bytes kept in memory (the pool), path: the cache file */
int ts_open(TSTORE *s, long npages, long budget, const char *path);
void ts_close(TSTORE *s);		/* the cache file removed */
/* page n's text (len bytes, copied); 0, or -1 when it can't be kept */
int ts_put(TSTORE *s, long n, const char *text, long len);
/* page n's text, or NULL when it isn't extracted yet (or can't be read);
   valid until the next ts_get or ts_put */
const char *ts_get(TSTORE *s, long n, long *len);
int ts_have(const TSTORE *s, long n);
/* page n's text where it is in memory (the pool or a slot), else NULL:
   no file read, no slot taken (pictures ahead look only there) */
const char *ts_peek(const TSTORE *s, long n, long *len);
/* page n holds pictures (the reader counted their lines), and asked */
void ts_set_pics(TSTORE *s, long n, int pics);
int ts_pics(const TSTORE *s, long n);
/* memory given back when the reader or a picture is refused it: the pool
   written to the cache file and cut to keep bytes, the slots freed (what
   ts_get gave is then gone). The bytes given back; 0 with no cache file */
long ts_shrink(TSTORE *s, long keep);

#endif

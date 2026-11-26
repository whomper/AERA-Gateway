/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pigate.c - a PDF converted by the Pi (docs/PI-GATEWAY.md): sent from
 * disk a piece at a time, the Pi's 102 answers taken as progress, the
 * converted PDF written to disk a piece at a time. conn.c does the
 * waiting (pf_idle's turns, conn_stop, its timeouts); the files are
 * pdf_os_* (read) and cv_* (written), EConvert's and the host's.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "conn.h"
#include "pdf.h"
#include "conv/cvio.h"
#include "pigate.h"
#include "plat.h"
#include "pgshare.c"	/* (in this object: the programs' layout) */

#define PIECE 8192
/* the Pi converts for up to 600 s (pdfgate's --time) and says how far it
   is when it moves on: this long with no word from it is too long */
#define WORK_MS 660000L

/* how far the exchange is by time */
static PGSHARE share_s;

long pg_share(void)
{
	return pgs_now(&share_s, pf_ms());
}

/* the progress call, the share brought up to date first */
static int tell(pg_progress progress, void *u, int phase, long done, long total)
{
	pgs_share(&share_s, phase, done, total, pf_ms());
	return progress && progress(u, phase, done, total);
}

static void say(char *err, int errlen, const char *what)
{
	snprintf(err, errlen, "%s", what);
}

int pg_query(const PGOPT *o, const char *name, char *q, int qlen)
{
	char nm[65], *p;
	int n = 0, k = 0;
	const char *base = name;

	for (p = (char *)name; *p; p++)
		if (*p == '\\' || *p == '/' || *p == ':')
			base = p + 1;
	/* letters, digits, '_', '.', '-' and spaces, not starting with a dot
	   (pdfgate's NAME); anything else '_' */
	for (; base[k] && k < 64; k++) {
		char c = base[k];
		nm[k] = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
			((c == '.' || c == '-' || c == ' ') && k) ? c : '_';
	}
	nm[k] = 0;
	q[0] = 0;
#define ADD(...) do { if (n < qlen) n += snprintf(q + n, qlen - n, __VA_ARGS__); } while (0)
	if (o && o->pages)
		ADD("&pages=%s", o->pages);
	if (o && o->photos)
		ADD("&photos=%s", o->photos);
	if (o && o->pictures)
		ADD("&pictures=%s", o->pictures);
	if (o && o->page_w > 0)
		ADD("&page_width=%d", o->page_w);
	if (o && o->pic_w > 0)
		ADD("&picture_width=%d", o->pic_w);
	if (o && o->page_double >= 0)
		ADD("&double=%d", o->page_double ? 1 : 0);
	if (o && o->thumbs)
		ADD("&thumbs=1");
	if (o && o->pen_n >= 2 && o->pen_n <= 256 && o->pen_rgb && o->pen_col > 0) {
		static const char hx[] = "0123456789abcdef";
		int i, j;

		ADD("&pens=");
		if (n + o->pen_n * 6 >= qlen)
			return -1;
		for (i = 0; i < o->pen_n; i++)
			for (j = 0; j < 3; j++) {
				q[n++] = hx[o->pen_rgb[i][j] >> 4];
				q[n++] = hx[o->pen_rgb[i][j] & 15];
			}
		q[n] = 0;
		ADD("&column=%d&aspect=%d", o->pen_col, o->pen_aspect > 0 ? o->pen_aspect : 16);
	}
	if (o && o->translate && o->translate[0]) {
		const char *t;

		for (t = o->translate; *t; t++)
			if (*t < 'a' || *t > 'z')
				return -1;	/* (only a code: never anything else in the request line) */
		ADD("&translate=%s", o->translate);
	}
	if (k) {
		/* spaces as %20 */
		ADD("&name=");
		for (p = nm; *p; p++)
			if (*p == ' ')
				ADD("%%20");
			else
				ADD("%c", *p);
	}
#undef ADD
	if (n >= qlen)
		return -1;
	if (n)
		q[0] = '?';
	return n;
}

/* one answer's head: the status, and the headers wanted (Content-Length,
   X-Progress); -1 when the connection failed */
static int head(CONN *c, SBUF *b, long *length, long *done, long *total)
{
	int status;

	*length = -1;
	if (conn_getline(c, b) < 0)
		return -1;
	if (b->len < 12 || strncmp(b->s, "HTTP/1.", 7))
		return 0;
	status = atoi(b->s + 9);
	for (;;) {
		if (conn_getline(c, b) < 0)
			return -1;
		if (!b->len)
			return status;
		if (!strncmp(b->s, "Content-Length:", 15) || !strncmp(b->s, "content-length:", 15))
			*length = atol(b->s + 15);
		else if (!strncmp(b->s, "X-Progress:", 11)) {
			char *sl = strchr(b->s, '/');

			*done = atol(b->s + 11);
			*total = sl ? atol(sl + 1) : 0;
		}
	}
}

/* the copy's last bytes as they come (TAIL of them), for the update that
   adds the original's path */
#define TAIL 4096

static void keep_tail(char *tail, long *kept, const char *p, long n)
{
	if (n >= TAIL) {
		memcpy(tail, p + n - TAIL, TAIL);
		*kept = TAIL;
		return;
	}
	if (*kept + n > TAIL) {
		long drop = *kept + n - TAIL;

		memmove(tail, tail + drop, (size_t)(*kept - drop));
		*kept -= drop;
	}
	memcpy(tail + *kept, p, (size_t)n);
	*kept += n;
}

/* the update after the copy's length bytes: its catalog (object 1, the
   last object conv.c writes) again with /Path in its /Source, a one-entry
   xref and a trailer with /Prev (EPDF's reader and any other follow it).
   Into out (outlen); its length, or 0 when the tail isn't the converter's
   (the copy is left as it came) */
static long path_update(const char *tail, long kept, long length, const char *path, char *out, long outlen)
{
	const char *cat = NULL, *end, *src, *sx, *p;
	long prev, k = 0, size, xref;
	const char *w;

	for (p = tail; p + 8 <= tail + kept; p++)	/* (the last "1 0 obj" at a line's start) */
		if (!memcmp(p, "1 0 obj\n", 8) && (p == tail || p[-1] == '\n'))
			cat = p;
	if (!cat)
		return 0;
	end = NULL;
	for (p = cat; p + 7 <= tail + kept; p++)
		if (!memcmp(p, "\nendobj", 7)) {
			end = p;
			break;
		}
	src = NULL;
	for (p = cat; end && p + 18 <= end; p++)
		if (!memcmp(p, "/Source << /Name (", 18)) {
			src = p;
			break;
		}
	sx = NULL;
	for (p = end ? end : tail; p + 10 <= tail + kept; p++)
		if (!memcmp(p, "startxref\n", 10))
			sx = p;
	if (!end || !src || !sx)
		return 0;
	prev = atol(sx + 10);
	size = 0;
	for (p = end; p + 7 <= sx; p++)		/* (the xref's "0 N": the trailer's /Size) */
		if (!memcmp(p, "\nxref\n0 ", 8)) {
			size = atol(p + 8);
			break;
		}
	{
		/* the catalog to the end of /Source's /Name string, then /Path */
		const char *q = src + 18;
		int depth = 1;

		while (q < end && depth) {
			if (*q == '\\')
				q++;
			else if (*q == '(')
				depth++;
			else if (*q == ')')
				depth--;
			q++;
		}
		if (depth || prev <= 0 || size <= 1 || (long)(q - cat) + 200 + 2 * (long)strlen(path) > outlen)
			return 0;
		memcpy(out, cat, (size_t)(q - cat));
		k = (long)(q - cat);
		k += snprintf(out + k, (size_t)(outlen - k), " /Path (");
		for (w = path; *w; w++) {
			if (*w == '\\' || *w == '(' || *w == ')')
				out[k++] = '\\';
			out[k++] = *w;
		}
		out[k++] = ')';
		memcpy(out + k, q, (size_t)(end - q));
		k += (long)(end - q);
	}
	k += snprintf(out + k, (size_t)(outlen - k), "\nendobj\n");
	xref = length + k;	/* (the update's xref, where it begins in the file) */
	k += snprintf(out + k, (size_t)(outlen - k), "xref\n0 1\n0000000000 65535 f \n1 1\n%010ld 00000 n \n"
		      "trailer\n<< /Size %ld /Root 1 0 R /Info 3 0 R /Prev %ld >>\nstartxref\n%ld\n%%%%EOF\n",
		      length, size, prev, xref);
	return k;
}

static int convert(const char *host, unsigned short port, const PGOPT *o, const char *in, const char *out,
		   pg_progress progress, void *u, char *err, int errlen)
{
	/* (the query and request line on the heap while the request is made:
	   the pens' 1,536 hex digits are too many for the job's stack, and
	   3.8 KB held all the time a 1 MB ST's pictures miss) */
	enum { QLEN = 1800, LLEN = 2000 };
	char *q = NULL, *line = NULL, *piece = NULL, *tail = NULL;
	long kept = 0;
	long size = 0, off, length, done = 0, total = 0;
	void *src = NULL, *dst = NULL;
	CONN *c = NULL;
	SBUF b;
	int rc = -1, status;

	sb_init(&b);
	err[0] = 0;
	pgs_start(&share_s, pf_ms());
	q = malloc(QLEN);
	line = malloc(LLEN);
	if (!q || !line) {
		say(err, errlen, "not enough memory");
		free(q);
		free(line);
		return -1;
	}
	if (pg_query(o, in, q, QLEN) < 0) {
		say(err, errlen, "the settings are too long");
		free(q);
		free(line);
		return -1;
	}
	src = pdf_os_open(in, &size);
	if (!src) {
		say(err, errlen, "cannot open the file");
		free(q);
		free(line);
		return -1;
	}
	piece = malloc(PIECE);
	if (!piece) {
		say(err, errlen, "not enough memory");
		goto done;
	}
	c = conn_open("PDF", host, port, PIECE, SEC_PLAIN, err, errlen);
	if (!c) {
		rc = conn_stop ? -2 : -3;	/* (no Gateway there: the batch can't go on) */
		goto done;
	}
	snprintf(line, LLEN, "POST /convert%s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/pdf\r\n"
		 "Content-Length: %ld\r\nConnection: close\r\n\r\n", q, host, size);
	if (conn_phase)
		conn_phase(GATE_SEND, (long)strlen(line) + size);	/* (guideline 7.3: the three phases) */
	if (!conn_write(c, line, (long)strlen(line)))
		goto lost;
	for (off = 0; off < size; off += PIECE) {
		long n = size - off < PIECE ? size - off : PIECE;

		if (pdf_os_read(src, off, piece, n) != n) {
			say(err, errlen, "cannot read the file");
			goto done;
		}
		if (!conn_write(c, piece, n))
			goto lost;
		if (tell(progress, u, PG_SEND, off + n, size)) {
			conn_stop = 1;
			goto lost;
		}
	}
	pdf_os_close(src);
	src = NULL;
	if (conn_phase)
		conn_phase(GATE_WAIT, 0);
	/* the Gateway's answers: 102s while it works (still GATE_WAIT: they are
	   no part of the answer), then the last one, after the work */
	c->timeout_ms = WORK_MS;
	for (;;) {
		status = head(c, &b, &length, &done, &total);
		if (status < 0)
			goto lost;
		if (status != 102)
			break;
		if (tell(progress, u, PG_WORK, done, total)) {
			conn_stop = 1;
			goto lost;
		}
	}
	c->timeout_ms = 60000;
	if (status != 200) {
		/* its one line, "pdfgate: why", as it is */
		long n = length > 0 && length < LLEN ? length : LLEN - 1;
		long got = 0;

		while (got < n) {
			long k = conn_read(c, line + got, n - got);

			if (k <= 0)
				break;
			got += k;
		}
		line[got] = 0;
		while (got && (line[got - 1] == '\n' || line[got - 1] == '\r'))
			line[--got] = 0;
		if (got)
			say(err, errlen, line);
		else
			snprintf(err, errlen, "the Pi answered %d", status);
		goto done;
	}
	if (length <= 0) {
		say(err, errlen, "the Pi sent nothing");
		goto done;
	}
	if (conn_phase)
		conn_phase(GATE_RECEIVE, length);
	if (o && o->source_path && o->source_path[0] && strlen(o->source_path) < 1000)
		tail = malloc(TAIL);	/* (none: the copy as it comes) */
	dst = cv_create(out);
	if (!dst) {
		say(err, errlen, "cannot write the converted file");
		goto done;
	}
	for (off = 0; off < length;) {
		long k = conn_read(c, piece, length - off < PIECE ? length - off : PIECE);

		if (k <= 0)
			goto lost;
		if (cv_write(dst, piece, k) != k) {
			say(err, errlen, "the disk is full");
			goto done;
		}
		off += k;
		if (tail)
			keep_tail(tail, &kept, piece, k);
		if (tell(progress, u, PG_RECV, off, length)) {
			conn_stop = 1;
			goto lost;
		}
	}
	if (tail) {
		/* the original's path, for EPDF (its piece buffer holds the update) */
		long n = path_update(tail, kept, length, o->source_path, piece, PIECE);

		if (n > 0 && cv_write(dst, piece, n) != n) {
			say(err, errlen, "the disk is full");
			goto done;
		}
	}
	if (cv_close(dst)) {
		dst = NULL;
		say(err, errlen, "the disk is full");
		cv_remove(out);
		goto done;
	}
	dst = NULL;
	rc = 0;
	goto done;
lost:
	if (conn_stop)
		rc = -2;
	else if (c && c->err[0])
		say(err, errlen, c->err);
	else if (!err[0])
		say(err, errlen, "the connection to the Pi was lost");
done:
	if (conn_phase)
		conn_phase(GATE_DONE, 0);
	if (c)
		conn_close(c);
	if (src)
		pdf_os_close(src);
	if (dst) {
		cv_close(dst);
		cv_remove(out);
	}
	free(piece);
	free(tail);
	free(q);
	free(line);
	sb_free(&b);
	return rc;
}

int pg_convert(const char *host, unsigned short port, const PGOPT *o, const char *in, const char *out,
	       pg_progress progress, void *u, char *err, int errlen)
{
	int rc = convert(host, port, o, in, out, progress, u, err, errlen);

	if (rc == -1 && o && o->pen_n && strstr(err, "no option pens")) {
		/* a Gateway before Faster display: the same without the pens
		   (the pictures as before) */
		PGOPT p = *o;

		p.pen_n = 0;
		rc = convert(host, port, &p, in, out, progress, u, err, errlen);
	}
	return rc;
}

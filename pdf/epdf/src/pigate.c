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

#define PIECE 8192
/* the Pi converts for up to 600 s (pdfgate's --time) and says how far it
   is when it moves on: this long with no word from it is too long */
#define WORK_MS 660000L

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

int pg_convert(const char *host, unsigned short port, const PGOPT *o, const char *in, const char *out,
	       pg_progress progress, void *u, char *err, int errlen)
{
	char q[256], line[400], *piece = NULL;
	long size = 0, off, length, done = 0, total = 0;
	void *src = NULL, *dst = NULL;
	CONN *c = NULL;
	SBUF b;
	int rc = -1, status;

	sb_init(&b);
	err[0] = 0;
	if (pg_query(o, in, q, sizeof q) < 0) {
		say(err, errlen, "the settings are too long");
		return -1;
	}
	src = pdf_os_open(in, &size);
	if (!src) {
		say(err, errlen, "cannot open the file");
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
	snprintf(line, sizeof line, "POST /convert%s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/pdf\r\n"
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
		if (progress && progress(u, PG_SEND, off + n, size)) {
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
		if (progress && progress(u, PG_WORK, done, total)) {
			conn_stop = 1;
			goto lost;
		}
	}
	c->timeout_ms = 60000;
	if (status != 200) {
		/* its one line, "pdfgate: why", as it is */
		long n = length > 0 && length < (long)sizeof line ? length : (long)sizeof line - 1;
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
		if (progress && progress(u, PG_RECV, off, length)) {
			conn_stop = 1;
			goto lost;
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
	sb_free(&b);
	return rc;
}

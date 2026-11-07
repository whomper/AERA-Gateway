/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* pdfmeas.h - the measurement run: what a page costs to read, shared by the host tool and the Atari program */
#ifndef PDFMEAS_H
#define PDFMEAS_H

/* pages: "all", "first:N", "sample:K" (K pages spread over the file), "N" (one, 1-based) */
typedef struct {
	void (*emit)(const char *line);			/* a JSON line */
	void (*text)(long page, const char *s, long n);	/* a page's text (NULL: not wanted) */
	long (*ms)(void);				/* a clock; 1000 ticks a second */
	long (*us)(void);				/* a finer clock, or NULL */
	long (*heap_peak)(void);			/* bytes the heap has had in use, or NULL */
	void (*heap_mark)(void);			/* start a new peak (heap_peak = heap_use) */
} PMEAS;

int pdf_measure(const PMEAS *m, const char *path, const char *name, const char *pages);

#endif

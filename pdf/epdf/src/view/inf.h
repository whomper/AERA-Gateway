/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * inf.h - EPDF.INF (the guidelines, 6.1): plain text, one NAME=value a
 * line, ';' notes above each setting and an empty line after it, CR/LF.
 * Written back with every line the program doesn't know kept as it was.
 */
#ifndef INF_H
#define INF_H

typedef struct {
	char **line;			/* without their line ends */
	long n, cap;
} INF;

void inf_init(INF *f);
void inf_free(INF *f);
/* the text of a file; 0, or -1 out of memory */
int inf_parse(INF *f, const char *text, long len);
/* the value of NAME, or NULL */
const char *inf_get(const INF *f, const char *name);
/* NAME=value: changed where it is, else added at the end after its note
   (";" and the note; NULL for none) and followed by an empty line.
   0, or -1 out of memory */
int inf_set(INF *f, const char *name, const char *value, const char *note);
/* the file's text, CR/LF (malloc'ed), its length in *len */
char *inf_text(const INF *f, long *len);

#endif

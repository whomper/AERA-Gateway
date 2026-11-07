/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* pdfint.h - what the reader's files share and the program doesn't see */
#ifndef PDFINT_H
#define PDFINT_H

#include "pdf.h"

void *pa_alloc(PDOC *d, PARENA *a, long n);
void pa_free(PDOC *d, PARENA *a);
long pdf_rd(PDOC *d, long off, void *buf, long n);
long pdf_rd_seq(PDOC *d, long off, void *buf, long n);
/* the cross-reference table's entries (pdf.h's PXE): the kind (0 not set,
   1 in use, 2 compressed, 3 free), and a, b as PXX's; xe_put sets one
   (as it is: whatever was there goes), xe_clear empties the table */
int xe_get(PDOC *d, long num, uint32_t *a, uint16_t *b);
#define xe_kind(d, num) ((int)((d)->xe[num] >> 30))
void xe_put(PDOC *d, long num, int t, uint32_t a, uint16_t b);
void xe_clear(PDOC *d);

/* ---- lexer ---- */
typedef struct {
	const unsigned char *p, *e;
	PDOC *d;
	PARENA *arena;
	unsigned char *scr;		/* scratch for strings and names */
	long scrcap;
	POBJ *tmp;			/* the stack of what is being built */
	long ntmp, captmp;
	long num, gen;			/* the object being read (for decryption); -1: none */
	int depth;
	int trunc;			/* ran into the end of the buffer */
	int final;			/* the buffer reaches the end of the file */
	int nocrypt;			/* leave strings as they are */
} LEX;

typedef struct {
	int kind;
	fx f;
	int32_t i;
	const unsigned char *s;		/* string/name (in the arena), keyword (in the buffer) */
	long n;
} TOK;

enum { T_EOF, T_INT, T_REAL, T_STR, T_NAME, T_ARR0, T_ARR1, T_DICT0, T_DICT1, T_KEY, T_BAD };

void lex_init(LEX *x, PDOC *d, PARENA *arena, const unsigned char *buf, long n, int final);
void lex_done(LEX *x);
void lex_skip(LEX *x);
int lex_token(LEX *x, TOK *t);
int lex_number(const unsigned char *s, const unsigned char *e, fx *fxv, int32_t *iv, int *isreal);
int parse_obj(LEX *x, POBJ *out);

/* ---- xref ---- */
int xref_load(PDOC *d);
int xref_rebuild(PDOC *d);
int objstm_register(PDOC *d, long snum);

/* ---- crypt ---- */
int pc_open(PDOC *d, POBJ *enc, POBJ *id);		/* 0 ok, 1 needs a password or unsupported */
long pc_decrypt(PDOC *d, long num, long gen, int is_stream, unsigned char *buf, long n);	/* new length */
/* a stream decrypted as it is read: NULL when it isn't encrypted (or no
   memory); run decrypts n bytes in place (AES: whole blocks, the first 16
   of the stream its IV, not given back) and gives how many came out */
typedef struct pcstream PCSTREAM;
PCSTREAM *pc_stream_open(PDOC *d, long num, long gen);
int pc_stream_is_aes(const PCSTREAM *x);
long pc_stream_run(PCSTREAM *x, unsigned char *buf, long n);
void pc_stream_close(PCSTREAM *x);
void pc_close(PDOC *d);

void pdf_text_cleanup(PDOC *d);

/* ---- filters ---- */
int filt_apply(PDOC *d, const char *name, POBJ *parms, SBUF *in_out, long max);
int pdf_filter_is_image(const char *name);

static inline int is_white_c(int c) { return c == 0 || c == 9 || c == 10 || c == 12 || c == 13 || c == 32; }

#endif

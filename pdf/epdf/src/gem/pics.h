/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef PICS_H
#define PICS_H

#include "pic.h"

enum { PICS_FREE, PICS_WANTED, PICS_DECODING, PICS_READY, PICS_FAILED };

typedef struct {
	long obj;
	int w, h;
	int flip;			/* DV_FLIPV: made upside down, then turned; DV_FLIPH: mirrored */
	long page;			/* the page it is on (the progress row's words) */
	long made;			/* its rows made so far, shown or not (the progress row's) */
	int state, why;
	MFDB bm;			/* the screen's own bitmap, in ST RAM, when bm.fd_addr */
	long ready;			/* its rows made, drawn from the top down while it is made a
					   row at a time; h when PICS_READY */
	int heap;			/* bm's memory is malloc's (else GEMDOS's) */
	unsigned long used;
	int ahead;			/* wanted ahead of the view, not yet shown (Erez, 4 Nov 2026:
					   pictures made quietly before they come into view) */
	long sw, sh;			/* the picture's own pixels, once it is being made (0: not known) */
} PICE;

long pics_st_free(void);		/* the largest block of ST RAM free (the bitmaps'), Malloc(-1)'s where there is only ST RAM */
void pics_room(void);			/* how many pictures are kept decoded, from the memory free (at the start) */
PICE *pics_get(long obj, int w, int h, int flip);	/* NULL: every slot busy decoding */
int pics_waiting(void);
PICE *pics_next(void);			/* one to decode, marked decoding (one shown first) */
/* a picture ahead of the view wanted, quietly: 1 queued, 0 there already,
   -1 no room (every slot taken, or the memory too short to spare) */
int pics_ahead(long obj, int w, int h, int flip, long page);
int pics_shown_waiting(void);		/* a picture the window shows waits to be decoded */
/* the page in focus (the window's top page): its pictures made first, and
   while one is wanted or being made nothing else takes turns from it */
extern long pics_focus;
int pics_focus_busy(void);
void pics_release(PICE *e);		/* its slot let go (an ahead one stopped) */
/* the decoder's answer for a picture made whole: std (the standard
   bitmap) or NULL (why) */
void pics_done(PICE *e, unsigned char *std, long bytes, int why);
PIC_PAL *pics_palette(void);		/* NULL on a 16-bit screen */
unsigned char *pics_standard(PIC *p, int nplanes, PIC_PAL *pl, int flip, long *bytes);
/* e's picture made a row at a time straight into e->bm, its rows shown
   as they come (e->ready): 1 made, 0 not (*why), -1 not this way
   (pdf_image, pics_standard, pics_done). On the decoder's stack */
struct pdoc;
int pics_rows(struct pdoc *d, PICE *e, int nplanes, PIC_PAL *pl, int *why);
/* while a band of colour rows waits for the screen's format, the decoder
   hands the machine back through pics_wait (doc.c's); pics_band, on the
   main stack, turns it (vr_trnfm): 1 when there was one */
extern void (*pics_wait)(void);
int pics_band(void);
/* pics_rows' picture ended: made, or not (its bitmap let go; logged
   unless it was stopped) */
void pics_made(PICE *e, int ok, int why);
/* a page drawn as printed by render.ovl (View > Pages for any PDF on a
   Falcon or TT): its picture's object is PICS_PAGE + the page (above any
   PDF's objects), its rows given by pics_page_rows (doc.c's): 1 made, 0
   not (*why). On the decoder's stack */
#define PICS_PAGE 0x40000000L
/* (pal not NULL: a screen of 2 to 16 colours, `planes` of them; then
   planes_row takes each row as its planes' rows, the standard format's, in
   place of row's 5-6-5) */
extern int (*pics_page_rows)(long page, int w, int h, void (*row)(void *u, long y, const unsigned short *px),
			     void (*planes_row)(void *u, long y, const unsigned char *planes), const PIC_PAL *pal,
			     int planes, void *u, int *why);
void pics_forget(void);
/* src (made) enlarged to w x h at once, nearest pixels, into a bitmap of
   its own (out, device format; heap for pics_enlarge_free): 0, -1 */
int pics_enlarge(const PICE *src, int w, int h, MFDB *out, int *heap);
void pics_enlarge_free(MFDB *m, int heap);
void pics_exit(void);

#endif

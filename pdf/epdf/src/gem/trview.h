/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* trview.h - View > Translate: the text view's pages translated (trview.c) */
#ifndef TRVIEW_H
#define TRVIEW_H

#define TR_PORT 8480			/* translategate's (AERA-Gateway) */

/* where translategate is: the Gateway's address (Settings), its port */
void tr_setup(const char *host, unsigned short port);
/* View > Translate on or off, into lang ("he"; NULL: as it was) */
void tr_set(int on, const char *lang);
int tr_on(void);
const char *tr_to(void);
void tr_new_document(void);		/* a document opened: its view as Translate has it */
void tr_document(void);
int tr_busy(void);			/* a page to translate, or one under way */
/* a slice of about ms: the page whose translation came (doc.c has it), or -1 */
long tr_slice(int ms);
void tr_stop(void);			/* the page under way given up */
void tr_halt(void);			/* and the document's (Esc) */
int tr_page(long *n);			/* the page under way */
/* why the document's translation stopped, once (the one alert), else NULL */
const char *tr_failed(void);
long tr_pieces(void);			/* requests made for the document (the rigs) */

#endif

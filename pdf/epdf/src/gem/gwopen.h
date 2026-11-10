/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* gwopen.h - File > Open on the Gateway: a PDF converted by the Gateway
   while it is shown, its copy in a file of EPDF's own (gwopen.c) */
#ifndef GWOPEN_H
#define GWOPEN_H

enum { GW_STARTED, GW_BUSY, GW_NO_ADDRESS, GW_NO_FOLDER, GW_NO_NETWORK, GW_NO_MEMORY };

/* where the copies go (EPDF.PRG's folder), once at the start */
void gw_set_temp_dir(const char *dir);
/* path is one of those copies */
int gw_is_temp(const char *path);
/* the copies deleted, but open_now (the document open; NULL: both) */
void gw_forget(const char *open_now);
/* the PDF last sent (the copy's original) */
const char *gw_source(void);
/* the conversion begun (GW_STARTED), or why not; open_now: the document
   open, which the copy mustn't overwrite */
int gw_start(const char *pdf, const char *open_now, const char *host, unsigned short port);
int gw_busy(void);
/* a slice of it, about ms; 1 when it has ended (gw_result) */
int gw_slice(int ms);
void gw_stop(void);
/* how far (thousandths); 1 while the Gateway hasn't begun it */
int gw_progress(long *done, long *total);
/* the status line's words for where it is, and the phase's bytes */
const char *gw_words(char *w, int n, long *done, long *total);
/* how it ended: pg_convert's 0, -1 (why), -2 stopped, -3 no answer; the copy */
int gw_result(char *copy, int ncopy, char *why, int nwhy);
const char *gw_host(void);
/* the language the next conversions are translated into ("" or NULL: none;
   View > Translate) */
void gw_set_translate(const char *to);

#endif

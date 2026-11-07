/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* gwopen.h - File > Open on the Gateway: a PDF converted by the Gateway
   while it is shown, its copy in "atari" beside it (gwopen.c) */
#ifndef GWOPEN_H
#define GWOPEN_H

enum { GW_STARTED, GW_BUSY, GW_NO_ADDRESS, GW_NO_FOLDER, GW_NO_NETWORK, GW_NO_MEMORY };

/* pdf is in a folder "atari" (a copy: never sent again) */
int gw_in_copy_folder(const char *pdf);
/* the copy's path for pdf into copy; 1 when it is there and not older */
int gw_copy(const char *pdf, char *copy, int n);
/* the conversion begun (GW_STARTED), or why not */
int gw_start(const char *pdf, const char *host, unsigned short port);
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

#endif

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* prjob.h - File > Print... (prjob.c) */
#ifndef PRJOB_H
#define PRJOB_H

int prjob_begin(void);		/* the dialog, the printer opened, the job begun: 1 */
int prjob_on(void);		/* a job under way */
void prjob_stop(void);		/* Esc: stopped at the next band */
void prjob_slice(unsigned long ms);	/* the job on for up to ms */
int prjob_progress(char *w, int n, long *done, long *total);	/* the status bar's: 1 while printing */

/* the rigs' (epdf.inf's PRINTTO= "DRIVER DEVICE", PRINTRANGE= "FIRST LAST"):
   NVDI's picture driver to a file, the pages the dialog would give */
extern const char *prjob_device;
extern long prjob_first, prjob_last;

#endif

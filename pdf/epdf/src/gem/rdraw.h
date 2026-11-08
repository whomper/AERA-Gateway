/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * rdraw.h - EPDF's side of render.ovl (src/rovl/rovl.h): the page as
 * printed, drawn on a Falcon or TT by the overlay beside the program,
 * loaded the first time a page is drawn.
 */
#ifndef RDRAW_H
#define RDRAW_H

#include "rovl.h"

/* where render.ovl is (set by the program: beside it) */
extern char rdraw_path[200];

/* this machine can draw pages: a 68020 or better (an ST never loads it) */
int rdraw_machine(void);
/* render.ovl loaded (once), its table checked and started with the
   program's memory and turn; 1, or 0 and why */
int rdraw_load(int (*turn)(void), char *err, int errlen);
const ROVL *rdraw_ovl(void);		/* NULL until loaded */
long rdraw_taken(void);			/* its bytes in memory */
void rdraw_unload(void);

#endif

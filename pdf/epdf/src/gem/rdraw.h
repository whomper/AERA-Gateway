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

/* where render.ovl is (set by the program: beside it); in the FireBee's
   package (decided 22 Nov 2026: the same names) it is the ColdFire's build */
extern char rdraw_path[200];
#define RDRAW_FILE "render.ovl"
#define RDRAW_NAME "RENDER.OVL"
#define RDRAW_PRG "EPDF.PRG"

/* this machine can draw pages: a 68020 or better (an ST never loads it) */
int rdraw_machine(void);
/* render.ovl loaded (once), its table checked and started with the
   program's memory and turn; 1, or 0 and why */
void rdraw_crisp(int on);
int rdraw_load(int (*turn)(void), char *err, int errlen);
const ROVL *rdraw_ovl(void);		/* NULL until loaded */
long rdraw_taken(void);			/* its bytes in memory */
void rdraw_unload(void);
/* render.ovl's room set aside before a document's heap grows (a short
   machine: rdraw.c), 1 when it was; its size (0 none); given back to TOS
   (render.ovl not loaded in it) */
int rdraw_reserve(void);
long rdraw_reserved(void);
long rdraw_spare(void);		/* (that part of it a refusal gives back) */
void rdraw_unreserve(void);

#endif

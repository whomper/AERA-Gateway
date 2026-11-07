/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* Read before every FreeType file on the Atari (-include): what the freestanding C library lacks. */
#ifndef FTPREP_H
#define FTPREP_H
char *epdf_getenv(const char *name);
#define getenv epdf_getenv
#endif

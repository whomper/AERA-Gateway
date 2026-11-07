/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#ifndef EPDF_MENU_H
#define EPDF_MENU_H

#include "mbar.h"

enum {
	CMD_NONE, CMD_ABOUT, CMD_OPEN, CMD_CLOSE, CMD_QUIT, CMD_PREV, CMD_NEXT, CMD_FIRST, CMD_LAST,
	CMD_COPY, CMD_GUIDE, CMD_LINES, CMD_SELALL, CMD_FIND, CMD_FINDNEXT, CMD_FINDPREV, CMD_FONT, CMD_PAGES, CMD_GATEWAY, CMD_SETTINGS, CMD_COUNT
};

int menu_build(MBAR *m, short cw, short ch, short screen_w, short screen_h);

#endif

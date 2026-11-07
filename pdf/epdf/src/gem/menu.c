/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * The menu bar in the guidelines' order (3.1): the Desk menu under the
 * program's name, File, Edit, View, Page, Help; built by common/ui/mbar.c.
 */
#include <stddef.h>
#include "menu.h"

static const MBAR_ITEM desk_items[] = {
	{ "About EPDF...", NULL, CMD_ABOUT },
	{ NULL, NULL, CMD_NONE }
};

static const MBAR_ITEM file_items[] = {
	{ "Open...", "^O", CMD_OPEN },
	{ "-", NULL, CMD_NONE },
	{ "Quit", "^Q", CMD_QUIT },
	{ NULL, NULL, CMD_NONE }
};

/* Find next and previous: the Mac's keys (the guidelines, 3.2: an app
   that copies a Mac app takes its Command key as Control) */
static const MBAR_ITEM edit_items[] = {
	{ "Copy", "^C", CMD_COPY },
	{ "Select all", "^A", CMD_SELALL },
	{ "-", NULL, CMD_NONE },
	{ "Find...", "^F", CMD_FIND },
	{ "Find next", "^G", CMD_FINDNEXT },
	{ "Find previous", "\x01^G", CMD_FINDPREV },
	{ NULL, NULL, CMD_NONE }
};

/* a tick: each line as on the page, else the paragraphs joined and wrapped to the window */
static const MBAR_ITEM view_items[] = {
	{ "Font...", "\x01^F", CMD_FONT },
	{ "-", NULL, CMD_NONE },
	{ "The page's lines", NULL, CMD_LINES },
	{ "Pages", NULL, CMD_PAGES },
	{ NULL, NULL, CMD_NONE }
};

static const MBAR_ITEM page_items[] = {
	{ "Previous page", "^Left", CMD_PREV },
	{ "Next page", "^Right", CMD_NEXT },
	{ "-", NULL, CMD_NONE },
	{ "First page", "^Home", CMD_FIRST },
	{ "Last page", "\x01^Home", CMD_LAST },
	{ NULL, NULL, CMD_NONE }
};

static const MBAR_ITEM help_items[] = {
	{ "EPDF guide", NULL, CMD_GUIDE },
	{ NULL, NULL, CMD_NONE }
};

static const MBAR_TITLE titles[] = {
	{ " EPDF ", desk_items },
	{ " File ", file_items },
	{ " Edit ", edit_items },
	{ " View ", view_items },
	{ " Page ", page_items },
	{ " Help ", help_items },
	{ NULL, NULL }
};

int menu_build(MBAR *m, short cw, short ch, short screen_w, short screen_h)
{
	return mbar_build(m, titles, cw, ch, screen_w, screen_h);
}

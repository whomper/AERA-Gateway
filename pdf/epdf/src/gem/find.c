/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * find.c - Edit > Find... (^F): the text to look for, in the family's
 * dialog (common/ui/dialog.c: Esc empties the field, Undo cancels; the
 * field holds more than it shows, common/ui/field.c), made with common's
 * builder (common/ui/dlgbuild.c).
 */
#include <stdio.h>
#include <string.h>
#include "gemui.h"
#include "dialog.h"
#include "dlgbuild.h"
#include "epdf.h"

#define SHOW 32				/* the field's width on the screen */
#define CAP 64				/* what it holds */
#define W (SHOW + 10)

/* the text to look for, in text (what was looked for last shows in the
   field; text changes only when Find is chosen); 1 when Find was chosen
   with some. Common's builder (dlgbuild.h): a black band " Find", the
   field framed, Cancel then Find, 10 wide (Erez, 9 Nov 2026: D22, D26) */
int find_dialog(char *text)
{
	static char edit[CAP + 1];
	short x, y, w, h, r, field, find, cancel;
	DLGB b;

	if (!db_alloc(&b, 8, 4, 0, 1, 0))
		return 0;
	snprintf(edit, sizeof edit, "%s", text);
	db_begin(&b, W, 7);
	db_band(&b, W, " Find");
	db_text(&b, 2, 2, "Text:");
	field = db_framed(&b, db_long(&b, 8, 2, edit, SHOW, CAP, 0));
	cancel = db_button(&b, W - 24, 5, 10, "Cancel", EXIT);
	find = db_button(&b, W - 12, 5, 10, "Find", EXIT | DEFAULT);
	db_end(&b);
	dlg_name("Find");			/* its window's name, where it has one */
	dlg_band(b.band);
	dlg_open(b.tree, &x, &y, &w, &h);
	r = (short)(dlg_do(b.tree, field, cancel) & 0x7fff);
	dlg_close();
	db_free(&b);
	log_event("dialog-closed", "find", r == find ? "find" : "cancel");
	if (r != find)
		return 0;
	snprintf(text, CAP + 1, "%s", edit);
	return text[0] != '\0';
}

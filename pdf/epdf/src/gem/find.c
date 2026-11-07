/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * find.c - Edit > Find... (^F): the text to look for, in the family's
 * dialog (common/ui/dialog.c: Esc empties the field, Undo cancels; the
 * field holds more than it shows, common/ui/field.c), built here at run
 * time as the menu bar is.
 */
#include <string.h>
#include "gemui.h"
#include "dialog.h"
#include "field.h"
#include "epdf.h"

#define SHOW 32				/* the field's width on the screen */
#define CAP 64				/* what it holds */

enum { BOX, TITLE, LABEL, FIELD, FIND, CANCEL, NOBJ };

static OBJECT tree[NOBJ];
static TEDINFO ted;
static char tmpl[SHOW + 1], valid[SHOW + 1], view[SHOW + 1];
static LFIELD lf;

static void obj(short i, short type, short flags, long spec, short x, short y, short w, short h)
{
	OBJECT *o = &tree[i];

	memset(o, 0, sizeof *o);
	o->ob_next = (short)(i + 1 < NOBJ ? i + 1 : 0);
	o->ob_head = o->ob_tail = -1;
	o->ob_type = type;
	o->ob_flags = flags;
	o->ob_spec = spec;
	o->ob_x = x;
	o->ob_y = y;
	o->ob_width = w;
	o->ob_height = h;
}

static void build(void)
{
	memset(tmpl, '_', SHOW);
	tmpl[SHOW] = '\0';
	memset(valid, 'X', SHOW);
	valid[SHOW] = '\0';
	view[0] = '\0';
	memset(&ted, 0, sizeof ted);
	ted.te_ptext = view;
	ted.te_ptmplt = tmpl;
	ted.te_pvalid = valid;
	ted.te_font = 3;
	ted.te_color = 0x1180;
	ted.te_thickness = -1;
	ted.te_txtlen = SHOW + 1;
	ted.te_tmplen = SHOW + 1;
	obj(BOX, G_BOX, 0, 0x00021100L, 0, 0, SHOW + 10, 8);
	tree[BOX].ob_next = -1;
	tree[BOX].ob_head = TITLE;
	tree[BOX].ob_tail = CANCEL;
	tree[BOX].ob_state = OUTLINED;
	obj(TITLE, G_STRING, 0, (long)"Find in the document", 2, 1, 20, 1);
	obj(LABEL, G_STRING, 0, (long)"Text:", 2, 3, 5, 1);
	obj(FIELD, G_FBOXTEXT, EDITABLE, (long)&ted, 8, 3, SHOW, 1);
	obj(FIND, G_BUTTON, SELECTABLE | EXIT | DEFAULT, (long)"Find", SHOW - 12, 6, 9, 1);
	obj(CANCEL, G_BUTTON, SELECTABLE | EXIT | LASTOB, (long)"Cancel", SHOW - 1, 6, 9, 1);
	tree[CANCEL].ob_next = BOX;
	rsrc_obfix_all(tree, NOBJ);
}

/* the text to look for, in text (what was looked for last shows in the
   field); 1 when Find was chosen with some */
int find_dialog(char *text)
{
	static int built;
	short r;

	if (!built) {
		build();
		built = 1;
	}
	lf_init(&lf, tree, FIELD, text, CAP, 0);
	lf_end(&lf);
	dlg_name("Find");			/* its window's name, where it has one */
	r = dlg_modal(tree, FIELD, CANCEL);
	lf_forget(tree);
	log_event("dialog-closed", "find", r == FIND ? "find" : "cancel");
	return r == FIND && text[0] != '\0';
}

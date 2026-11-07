/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * gatedlg.c - Settings... (^,) in EPDF and EConvert: where the Gateway is,
 * in the family's words for it (guideline 6.1's Secure page and 7.3:
 * "Through the Gateway: address, port"; Erez, 7 Nov 2026). Built at run
 * time as the Find dialog is; the address holds more than it shows
 * (common/ui/field.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gemui.h"
#include "dialog.h"
#include "field.h"
#include "gatedlg.h"

#define SHOW 24				/* the address's width on the screen */

enum { BOX, TITLE, LABEL, ALABEL, ADDR, PLABEL, PORT, OK, CANCEL, NOBJ };

static OBJECT tree[NOBJ];
static TEDINFO ted_a, ted_p;
static char tmpl_a[SHOW + 1], valid_a[SHOW + 1], view_a[SHOW + 1];
static char tmpl_p[6], valid_p[6], text_p[6];
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

static void ted(TEDINFO *t, char *text, char *tmpl, char *valid, int n, char v)
{
	memset(tmpl, '_', (size_t)n);
	tmpl[n] = '\0';
	memset(valid, v, (size_t)n);
	valid[n] = '\0';
	memset(t, 0, sizeof *t);
	t->te_ptext = text;
	t->te_ptmplt = tmpl;
	t->te_pvalid = valid;
	t->te_font = 3;
	t->te_color = 0x1180;
	t->te_thickness = -1;
	t->te_txtlen = (short)(n + 1);
	t->te_tmplen = (short)(n + 1);
}

static void build(void)
{
	ted(&ted_a, view_a, tmpl_a, valid_a, SHOW, 'X');
	ted(&ted_p, text_p, tmpl_p, valid_p, 5, '9');
	obj(BOX, G_BOX, 0, 0x00021100L, 0, 0, 44, 10);
	tree[BOX].ob_next = -1;
	tree[BOX].ob_head = TITLE;
	tree[BOX].ob_tail = CANCEL;
	tree[BOX].ob_state = OUTLINED;
	obj(TITLE, G_STRING, 0, (long)"Settings", 2, 1, 8, 1);
	obj(LABEL, G_STRING, 0, (long)"Through the Gateway:", 2, 3, 20, 1);
	obj(ALABEL, G_STRING, 0, (long)"Address:", 3, 5, 8, 1);
	obj(ADDR, G_FBOXTEXT, EDITABLE, (long)&ted_a, 12, 5, SHOW, 1);
	obj(PLABEL, G_STRING, 0, (long)"Port:", 6, 6, 5, 1);
	obj(PORT, G_FBOXTEXT, EDITABLE, (long)&ted_p, 12, 6, 5, 1);
	obj(OK, G_BUTTON, SELECTABLE | EXIT | DEFAULT, (long)"OK", 22, 8, 9, 1);
	obj(CANCEL, G_BUTTON, SELECTABLE | EXIT | LASTOB, (long)"Cancel", 33, 8, 9, 1);
	tree[CANCEL].ob_next = BOX;
	rsrc_obfix_all(tree, NOBJ);
}

int gate_dialog(char *host, int hostcap, unsigned short *port)
{
	static int built;
	short r;
	long p;

	if (!built) {
		build();
		built = 1;
	}
	snprintf(text_p, sizeof text_p, "%u", (unsigned)*port);
	lf_init(&lf, tree, ADDR, host, hostcap, 0);
	lf_end(&lf);
	dlg_name("Settings");
	r = dlg_modal(tree, ADDR, CANCEL);
	lf_forget(tree);
	if (r != OK)
		return 0;
	p = atol(text_p);
	*port = (unsigned short)(p > 0 && p < 65536 ? p : GATE_PORT);
	return 1;
}

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * gatedlg.c - Settings... (^,) in EPDF and EConvert: the family's dialog
 * (common/ui/settings.c, the guidelines' 6.1), with EPDF's pages:
 *
 *   General  (EPDF) the language View > Translate translates into: a
 *            button that opens the Gateway's installed languages over
 *            itself, the one chosen ticked (5.7); Languages...: the
 *            family's dialog (common's langs.c)
 *   Secure   the Gateway's address and port, in EWeb's words (6.1, 7.4)
 *
 * The address holds more than it shows (5.4). Cancel, Esc and Undo put
 * every value back (settings.c); the caller's own only change on OK.
 * Help > Keys... is here too, made with common's builder (dlgbuild.h).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gemui.h"
#include "dialog.h"
#include "settings.h"
#include "dlgbuild.h"
#include "gatedlg.h"

void log_event(const char *what, const char *a, const char *b);	/* (the program's) */

static char edit[128];			/* the address as typed (the caller's only on OK) */
static char port_s[6];
static short edit_cap;
static char lang_label[28];		/* " Hebrew              \x02" */
static short lang_obj;
static char *tr_to_buf;
static int tr_to_cap;
static int (*tr_pick)(char *to, int tocap, char *name, int namecap, short x, short y);
static void (*tr_languages)(void);

static void lang_show(const char *name)
{
	snprintf(lang_label, sizeof lang_label, " %-21.21s\x02", name && name[0] ? name : "Choose...");
}

/* "Translate into": the Gateway's languages, over the button (epdf.c's pick) */
static void lang_act(SETTINGS *s, void *user)
{
	char name[24];
	short x, y;

	(void)user;
	if (!tr_pick)
		return;
	objc_offset(set_tree(s), lang_obj, &x, &y);
	if (tr_pick(tr_to_buf, tr_to_cap, name, (int)sizeof name, x, y))
		lang_show(name);
}

static void langs_act(SETTINGS *s, void *user)
{
	(void)s;
	(void)user;
	if (tr_languages)
		tr_languages();
}

static void page_general(SETTINGS *s, short x, short y, void *user)
{
	short n = set_y(s, 5);

	(void)user;
	set_text(s, x, y, "View > Translate translates into:");
	lang_obj = set_value(s, x + 2, set_y(s, 1), 23, lang_label, lang_act, NULL);
	set_button(s, x + 2, set_y(s, 3), 14, "Languages...", langs_act, NULL);
	set_text(s, x, n, "The languages are the Gateway's. On it:");
	set_text(s, x + 2, (short)(n + 1), "aera-gateway install translate");
	set_text(s, x, (short)(n + 2), "Translations are kept in memory only.");
}

static void page_secure(SETTINGS *s, short x, short y, void *user)
{
	short n = set_y(s, 2);

	set_text(s, x, y, "Gateway");
	set_long(s, x + 8, y, edit, 18, edit_cap, 0);
	set_text(s, x + 28, y, "Port");
	set_field(s, x + 33, y, port_s, 5, '9');
	if (user) {
		set_text(s, x, n, "Options > Open on the Gateway sends a");
		set_text(s, x, (short)(n + 1), "PDF there to be converted for this");
		set_text(s, x, (short)(n + 2), "Atari (and translated, with Translate).");
	} else {
		set_text(s, x, n, "Through the Gateway, the PDFs are");
		set_text(s, x, (short)(n + 1), "converted there and sent back.");
	}
	set_text(s, x, (short)(n + 4), "On the Gateway:");
	set_text(s, x + 2, (short)(n + 5), "aera-gateway install pdf");
}

/* where the dialog is, once a time it opens (the rigs click its pages) */
static int placed;

static void drawn(OBJECT *tree, short band)
{
	char at[16];

	(void)band;
	if (placed)
		return;
	placed = 1;
	snprintf(at, sizeof at, "%d %d", tree[0].ob_x, tree[0].ob_y);
	log_event("dialog", "settings", at);
}

static int run(int with_tr, char *host, int hostcap, unsigned short *port)
{
	static const SET_PAGE both[] = { { "General", page_general }, { "Secure", page_secure } };
	static const SET_PAGE secure[] = { { "Secure", page_secure } };
	long p;
	int ok;

	if (hostcap > (int)sizeof edit)
		hostcap = (int)sizeof edit;
	edit_cap = (short)(hostcap - 1);
	snprintf(edit, sizeof edit, "%s", host);
	snprintf(port_s, sizeof port_s, "%u", (unsigned)*port);
	placed = 0;
	settings_drawn = drawn;
	ok = with_tr ? settings_run("Settings", both, 2, 0, 17, (void *)1) :
		       settings_run("Settings", secure, 1, 0, 16, NULL);
	settings_drawn = NULL;
	log_event("dialog-closed", "settings", ok ? "ok" : "cancel");
	if (!ok)
		return 0;
	snprintf(host, (size_t)hostcap, "%s", gate_dialog_host());
	p = atol(port_s);
	*port = (unsigned short)(p > 0 && p < 65536 ? p : GATE_PORT);
	return 1;
}

const char *gate_dialog_host(void)
{
	char *e;

	while ((e = strrchr(edit, ' ')) != NULL && e[1] == '\0')
		*e = '\0';	/* (a field's trailing blanks) */
	return edit;
}

int gate_dialog(char *host, int hostcap, unsigned short *port)
{
	tr_pick = NULL;
	tr_languages = NULL;
	return run(0, host, hostcap, port);
}

int gate_dialog_tr(char *host, int hostcap, unsigned short *port, char *to, int tocap, const char *to_name,
		   int (*pick)(char *to, int tocap, char *name, int namecap, short x, short y),
		   void (*languages)(void))
{
	char was[16];
	int ok;

	snprintf(was, sizeof was, "%s", to);
	tr_to_buf = to;
	tr_to_cap = tocap;
	tr_pick = pick;
	tr_languages = languages;
	lang_show(to_name);
	ok = run(1, host, hostcap, port);
	if (!ok)
		snprintf(to, (size_t)tocap, "%s", was);	/* (the language chosen meanwhile: back too) */
	return ok;
}

/* Help > Keys...: EPDF's keys in two columns, as EMail's Keys dialog */
void keys_dialog(void)
{
	static const char *const lines[] = {
		"^O  open a PDF          ^Q  quit",
		"^C  copy                ^A  select all",
		"^F  find                ^G  find next",
		"\x01^G find previous      ^,  settings",
		"^Left, ^Right  the page before, after",
		"^Home  the first page   \x01^Home  the last",
		"Up, Down, Space  scroll (\x01: a window)",
		"^+  ^-  ^0  a page larger, smaller, as it was",
		"           (View > Pages; the keypad's + and -)",
		"Insert  the context menu",
		"Esc  stops what EPDF is doing",
		"Help  EPDF's help",
		"^U  quit (one window)   Undo cancels a dialog"
	};
	short n = (short)(sizeof lines / sizeof lines[0]), i, x, y, w, h, ok;
	DLGB b;

	if (!db_alloc(&b, (short)(n + 4), 2, 0, 0, 0))
		return;
	db_begin(&b, 52, (short)(n + 5));
	db_band(&b, 52, " Keys");
	for (i = 0; i < n; i++)
		db_text(&b, 3, (short)(2 + i), lines[i]);
	ok = db_button(&b, 40, (short)(n + 3), 10, "OK", EXIT | DEFAULT);
	db_end(&b);
	dlg_name("Keys");
	dlg_band(b.band);
	dlg_open(b.tree, &x, &y, &w, &h);
	log_event("dialog", "keys", NULL);
	dlg_do(b.tree, 0, ok);
	dlg_close();
	db_free(&b);
	log_event("dialog-closed", "keys", NULL);
}

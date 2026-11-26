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
 *   General  (EConvert) Optimise for: Smaller files or Faster display
 *            (pictures also made for this screen's pens, the default);
 *            Page previews: each converted page carries a small picture
 *            of itself, for EPDF's page strip
 *   Gateway  (EPDF) how Open on the Gateway has a PDF converted: Whole
 *            pages, Photos, Optimise for, Page previews (EConvert's)
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
#include "alert.h"

void log_event(const char *what, const char *a, const char *b);	/* (the program's) */

static char edit[128];			/* the address as typed (the caller's only on OK) */
static char port_s[6];
static short port_obj = -1, port_page;	/* the port's field and its page: where a refused port is typed again */
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

/* the log's tick, last on General (the guidelines' 6.5, D8: "Write a
   log" in Options and in Settings; decided 22 Nov 2026): the program sets
   it before Settings and takes it after OK */
short settings_log;
/* "Use the system file selector" (guidelines 5.12, decided 25 Nov 2026; off
   by default): File > Save Optimised... through the AES's selector, not the
   family's save dialog; EPDF's General only (EConvert saves nothing) */
short settings_fsel;

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
	/* (the ticks after a blank row, in the page: set_y(s, 9) put the log's
	   under the dialog's foot, 2.44 and 2.45) */
	set_tick(s, x, (short)(n + 4), "Write a log", &settings_log);
	set_tick(s, x, (short)(n + 5), "Use the system file selector", &settings_fsel);
}

/* EConvert's General: Optimise for (Faster display the default: decided 17
   Nov 2026) and the pages' previews (off by default: decided 15 Nov 2026) */
static short previews_edit, optimise_edit;

static void page_conv(SETTINGS *s, short x, short y, void *user)
{
	static const char *const optimise[] = { "Smaller files", "Faster display" };
	short n = set_y(s, 3), m = set_y(s, 4);

	(void)user;
	set_text(s, x, y, "Optimise for");
	set_popup(s, x + 13, y, 18, optimise, 2, &optimise_edit);
	set_text(s, x, set_y(s, 1), "Faster display: pictures made ready");
	set_text(s, x, set_y(s, 2), "for this screen; bigger files.");
	set_tick(s, x, n, "Page previews", &previews_edit);
	set_text(s, x, m, "Each page carries a small picture of");
	set_text(s, x, (short)(m + 1), "itself, for EPDF's page strip. On a");
	set_text(s, x, (short)(m + 2), "Falcon, 2 seconds more a page.");
	set_tick(s, x, (short)(m + 4), "Write a log", &settings_log);	/* (set_y(s, 8) lay on the buttons) */
}

/* EPDF's Gateway page: how Open on the Gateway has a PDF converted (decided
   20 Nov 2026: EConvert's choices; by default as before: Automatic, 256
   colours, Faster display, previews with the page strip) */
static GWCONV gw_edit;

static void page_gateway(SETTINGS *s, short x, short y, void *user)
{
	static const char *const pages[] = { "Automatic", "None", "Mono", "Colour", "Mono and colour" };
	static const char *const photos[] = { "256 colours (fast)", "JPEG (small)" };
	static const char *const optimise[] = { "Smaller files", "Faster display" };
	static const char *const previews[] = { "With the page strip", "Always", "Never" };

	(void)user;
	set_text(s, x, y, "Whole pages");
	set_popup(s, x + 15, y, 23, pages, 5, &gw_edit.pages);
	set_text(s, x, set_y(s, 1), "Photos");
	set_popup(s, x + 15, set_y(s, 1), 23, photos, 2, &gw_edit.photos);
	set_text(s, x, set_y(s, 2), "Optimise for");
	set_popup(s, x + 15, set_y(s, 2), 23, optimise, 2, &gw_edit.optimise);
	set_text(s, x, set_y(s, 3), "Page previews");
	set_popup(s, x + 15, set_y(s, 3), 23, previews, 3, &gw_edit.previews);
	{
		short n = set_y(s, 5);

		set_text(s, x, n, "How a PDF opened on the Gateway is");
		set_text(s, x, (short)(n + 1), "converted. Automatic: in colour on 16");
		set_text(s, x, (short)(n + 2), "colours or more, else in mono.");
	}
}

static void page_secure(SETTINGS *s, short x, short y, void *user)
{
	short n = set_y(s, 2);

	set_text(s, x, y, "Gateway");
	set_long(s, x + 8, y, edit, 18, edit_cap, 0);
	set_text(s, x + 28, y, "Port");
	port_obj = set_field(s, x + 33, y, port_s, 5, '9');
	port_page = set_page_now(s);
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

/* OK: a port outside 1 to 65535 is refused with what to type, the field
   kept as typed and the text cursor in it (D30, decided 9 Nov 2026: nothing
   changed silently) */
static int check(SETTINGS *s, void *user)
{
	long p = atol(port_s);

	(void)user;
	if (p >= 1 && p <= 65535)
		return 1;
	alert(1, "[1][The Gateway's port is a number.|Please type a number|from 1 to 65535.][ OK ]");
	set_show(s, port_page);
	if (port_obj >= 0)
		set_focus(s, port_obj);
	log_event("settings-refused", "port", port_s);
	return 0;
}

static int run(int with_tr, char *host, int hostcap, unsigned short *port)
{
	static const SET_PAGE both[] = { { "General", page_general }, { "Gateway", page_gateway }, { "Secure", page_secure } };
	static const SET_PAGE conv[] = { { "General", page_conv }, { "Secure", page_secure } };
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
	{
		SET_DEF d;

		memset(&d, 0, sizeof d);
		d.title = "Settings";
		d.pages = with_tr == 1 ? both : with_tr == 2 ? conv : secure;
		d.npages = with_tr == 1 ? 3 : with_tr ? 2 : 1;
		d.height = with_tr == 1 ? 20 : with_tr ? 17 : 16;	/* (General's ticks: 2.46) */
		d.user = with_tr == 1 ? (void *)1 : NULL;
		d.check = check;
		port_obj = -1;
		ok = settings_dialog(&d);
	}
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

int gate_dialog_conv(char *host, int hostcap, unsigned short *port, short *previews, short *optimise)
{
	int ok;

	tr_pick = NULL;
	tr_languages = NULL;
	previews_edit = *previews ? 1 : 0;
	optimise_edit = *optimise ? 1 : 0;
	ok = run(2, host, hostcap, port);
	if (ok) {
		*previews = previews_edit;
		*optimise = optimise_edit;
	}
	return ok;
}

int gate_dialog_tr(char *host, int hostcap, unsigned short *port, char *to, int tocap, const char *to_name,
		   int (*pick)(char *to, int tocap, char *name, int namecap, short x, short y),
		   void (*languages)(void), GWCONV *gw)
{
	char was[16];
	int ok;

	gw_edit = *gw;
	snprintf(was, sizeof was, "%s", to);
	tr_to_buf = to;
	tr_to_cap = tocap;
	tr_pick = pick;
	tr_languages = languages;
	lang_show(to_name);
	ok = run(1, host, hostcap, port);
	if (!ok)
		snprintf(to, (size_t)tocap, "%s", was);	/* (the language chosen meanwhile: back too) */
	else
		*gw = gw_edit;
	return ok;
}

/* Help > Keys...: EPDF's keys in two columns, as EMail's Keys dialog */
void keys_dialog(void)
{
	static const char *const lines[] = {
		"^O  open a PDF          ^Q  quit",
		"^P  print",
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

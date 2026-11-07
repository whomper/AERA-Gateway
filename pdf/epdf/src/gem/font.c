/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * font.c - the text view's font (the guidelines, 5.3): the system font
 * in its two sizes, or a fixed-width GDOS font, chosen in the family's
 * Font dialog (common/ui/fontsel.c) and kept in EPDF.INF as FONT=name,size.
 * GDOS fonts are loaded only when the INF names one or the dialog is
 * opened (EPDF is a program: it may load them at any time). Dialogs and
 * menus keep the system font (font_sys_push, font_sys_pop).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tos.h"
#include "gemui.h"
#include "text.h"
#include "fontsel.h"
#include "epdf.h"

static short font_id = 1, font_pt;	/* in use: VDI id, points (system font: 8 or 16) */
static short gdos = FS_UNLOADED;	/* GDOS fonts loaded on vdi_h */
static char font_nm[FS_NAME] = "System font";
static char fnt_path[180];		/* EPDF.FNT: the fonts measured once */
static short saved_cw, saved_ch, pushed;

/* the system font at a cell height of 8 or 16 */
static void sys_size(short pt, short *w, short *h)
{
	vst_font(vdi_h, 1);
	vst_height(vdi_h, pt >= 16 ? 13 : 6, w, h);
}

/* the font set on vdi_h, and the text drawing told its cell */
static void set(short id, short pt)
{
	short w, h;

	if (id == 1) {
		sys_size(pt, &w, &h);
	} else {
		vst_font(vdi_h, id);
		vst_point(vdi_h, pt, &w, &h);
	}
	cw = w;
	ch = h;
	text_font(vdi_h, id, cw, ch);
}

static void load_gdos(void)
{
	if (gdos == FS_UNLOADED)
		gdos = vq_gdos() ? vst_load_fonts(vdi_h, 0) : 0;
}

/* the font named in EPDF.INF ("System font,16", "Courier 10 Pitch,10"),
   or the system font at the AES's size */
void font_init(const char *setting, const char *dir)
{
	char name[FS_NAME];
	const char *comma;
	short i, id = 1, pt = gl_hchar >= 16 ? 16 : 8;

	snprintf(fnt_path, sizeof fnt_path, "%s\\EPDF.FNT", dir);
	if (setting && (comma = strrchr(setting, ',')) != NULL && comma - setting < FS_NAME) {
		memcpy(name, setting, (size_t)(comma - setting));
		name[comma - setting] = '\0';
		pt = (short)atoi(comma + 1);
		if (strcmp(name, "System font")) {
			load_gdos();
			for (i = 2; i <= gdos + 1; i++) {
				char n[FS_NAME];
				short fid = vqt_name(vdi_h, i, n);

				n[32] = '\0';
				while (strlen(n) && n[strlen(n) - 1] == ' ')
					n[strlen(n) - 1] = '\0';
				if (!strcmp(n, name)) {
					id = fid;
					strcpy(font_nm, name);
					break;
				}
			}
		}
		if (id == 1 && pt != 8 && pt != 16)
			pt = gl_hchar >= 16 ? 16 : 8;
	}
	font_id = id;
	font_pt = pt;
	set(font_id, font_pt);
}

const char *font_setting(void)
{
	static char s[FS_NAME + 8];

	snprintf(s, sizeof s, "%s,%d", font_id == 1 ? "System font" : font_nm, font_pt);
	return s;
}

static int chosen;

static void apply(fontsel *s)
{
	font_id = s->id;
	font_pt = s->pt;
	strcpy(font_nm, s->id == 1 ? "System font" : s->font);
	chosen = 1;
}

/* View > Font...: 1 when another font was chosen (now set) */
int font_dialog(void)
{
	static fontsel fs;

	memset(&fs, 0, sizeof fs);
	fs.app = "EPDF";
	fs.fnt = fnt_path;
	fs.id = font_id;
	fs.pt = font_pt;
	fs.gdos = gdos;
	fs.apply = apply;
	chosen = 0;
	fontsel_run(&fs);
	gdos = fs.gdos;
	set(font_id, font_pt);		/* the dialog left the system font set */
	log_event("dialog-closed", "font", chosen ? font_setting() : "cancel");
	return chosen;
}

/* the system font for a dialog or a popup (gemui.h), then the view's again */
void font_sys_push(void)
{
	short w, h;

	if (pushed++)
		return;
	saved_cw = cw;
	saved_ch = ch;
	sys_size(gl_hchar >= 16 ? 16 : 8, &w, &h);
	cw = gl_wchar;
	ch = gl_hchar;
	text_font(vdi_h, 1, cw, ch);
}

void font_sys_pop(void)
{
	if (!pushed || --pushed)
		return;
	cw = saved_cw;
	ch = saved_ch;
	set(font_id, font_pt);
}

void font_small(short *w, short *h)
{
	vst_font(vdi_h, 1);
	vst_height(vdi_h, 4, w, h);
	text_font(vdi_h, 1, *w, *h);
}

void font_back(void)
{
	set(font_id, font_pt);
}

void font_exit(void)
{
	if (gdos > 0)
		vst_unload_fonts(vdi_h, 0);
}

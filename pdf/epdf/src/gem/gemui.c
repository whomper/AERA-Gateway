/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * gemui.c - the drawing the shared building blocks (About box, popups,
 * dialogs) take from the app (common/ui/gemui.h), on EPDF's workstation.
 * Text goes through common/ui/text.c, the fast way TOS allows.
 */
#include <string.h>
#include "tos.h"
#include "gemui.h"
#include "text.h"

short scr_w, scr_h;

void fill(GRECT *r, short color)
{
	short pxy[4];

	if (r->w <= 0 || r->h <= 0)
		return;
	pxy[0] = r->x;
	pxy[1] = r->y;
	pxy[2] = r->x + r->w - 1;
	pxy[3] = r->y + r->h - 1;
	vswr_mode(vdi_h, MD_REPLACE);
	vsf_interior(vdi_h, FIS_SOLID);
	vsf_color(vdi_h, color);
	vsf_perimeter(vdi_h, 0);
	vr_recfl(vdi_h, pxy);
}

void line_col(short x1, short y1, short x2, short y2, short color)
{
	short p[4] = { x1, y1, x2, y2 };

	vswr_mode(vdi_h, MD_REPLACE);
	vsl_color(vdi_h, color);
	v_pline(vdi_h, 2, p);
}

void hline(short x1, short x2, short y)
{
	line_col(x1, y, x2, y, 1);
}

void frame(GRECT *r, short color)
{
	short p[10];

	p[0] = p[6] = p[8] = r->x;
	p[1] = p[3] = p[9] = r->y;
	p[2] = p[4] = r->x + r->w - 1;
	p[5] = p[7] = r->y + r->h - 1;
	vswr_mode(vdi_h, MD_REPLACE);
	vsl_color(vdi_h, color);
	v_pline(vdi_h, 5, p);
}

/* at most cols characters, in the flags' style: drawn plain, bold added
   by common/ui/text.c (never vst_effects' bold for text in fixed cells) */
void text_at(short x, short y, const char *s, long n, short cols, short flags)
{
	short colour = (flags & TX_INVERSE) ? 0 : 1;

	if (n > cols)
		n = cols;
	if (n <= 0)
		return;
	vswr_mode(vdi_h, MD_TRANS);
	vst_color(vdi_h, colour);
	vst_effects(vdi_h, (flags & TX_DIM) ? 2 : 0);
	text_n(vdi_h, x, y, s, n);
	vst_effects(vdi_h, 0);
	if (flags & TX_BOLD) {
		if (text_fx_fast(1, MD_TRANS))
			text_fx(vdi_h, x, y, s, n, 1, colour);
		else
			text_bold_strike(vdi_h, x, y, s, n);
	}
}

void clip_on(GRECT *r)
{
	short pxy[4];

	pxy[0] = r->x;
	pxy[1] = r->y;
	pxy[2] = r->x + r->w - 1;
	pxy[3] = r->y + r->h - 1;
	text_clip(vdi_h, pxy);
}

void clip_off(void)
{
	text_noclip(vdi_h);
}

/* ST RAM to blit from: the screen can't be fed from TT RAM, where the
   program may be loaded */
void *st_ram(long n)
{
	static unsigned short fallback[256];
	static void *buf;

	if (buf == NULL) {
		void *p = Mxalloc(sizeof fallback, 0);

		buf = (long)p > 0 ? p : (void *)fallback;
	}
	return n <= (long)sizeof fallback ? buf : NULL;
}

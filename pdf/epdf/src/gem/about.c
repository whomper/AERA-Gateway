/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * EPDF's About box: the family's (common/ui/aboutbox.c), with the
 * document and the memory as its status lines (the guidelines, 1.3).
 * There is no network, so no first line.
 */
#include <stdio.h>
#include <string.h>
#include "tos.h"
#include "gem.h"
#include "aboutbox.h"
#include "aboutline.h"
#include "gemui.h"
#include "iconify.h"
#include "epdf.h"
#include "icon.h"
#include "icon16.h"
#include "icon256.h"

void about_box(void)
{
	static char status[100], mem[100];
	ABOUT a = { "EPDF", app_version, "PDF viewer for the Atari ST, TT and Falcon",
		    { status, mem, NULL }, { 0, 0, 0 }, icon_bits, icon16_px, icon16_rgb,
		    icon256_px, icon256_rgb };

	snprintf(status, sizeof status, "%s", doc_status());
	about_mem_line(mem, (int)sizeof mem);
	if (mem[0] == '\0')
		a.status[1] = NULL;
	log_event("dialog", "about", app_version);
	about_show(&a);
}

/* an iconified window: the icon (common/ui/iconify.c), the page under it */
void app_icon_redraw(short h, const GRECT *area, const char *status)
{
	icon_redraw(h, area, icon_bits, icon16_px, status);
}

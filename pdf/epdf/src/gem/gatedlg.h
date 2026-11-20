/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* gatedlg.h - Settings... (^,): where the Gateway is (EPDF, EConvert) */
#ifndef GATEDLG_H
#define GATEDLG_H

#define GATE_PORT 8380			/* pdfgate's (Erez's OK, 7 Nov 2026) */

/* EConvert's: the Gateway's address (host, hostcap bytes) and port, as they
   are shown and as the user leaves them, and its General page's "Page
   previews" (*previews, 0 or 1); all the caller's only on OK, then 1 */
int gate_dialog_conv(char *host, int hostcap, unsigned short *port, short *previews, short *optimise);
/* EPDF's: the same with "Translate into" (to, a language code; its button
   shows to_name and calls pick with its place, x and y: the popup over
   it; pick may change to and the name) and
   "Languages..." (languages: the family's dialog, common's langs.c) */
int gate_dialog_tr(char *host, int hostcap, unsigned short *port, char *to, int tocap, const char *to_name,
		   int (*pick)(char *to, int tocap, char *name, int namecap, short x, short y),
		   void (*languages)(void));
/* while the dialog is open: the address as typed so far (pick and
   languages ask the Gateway there; the caller's own only after OK) */
const char *gate_dialog_host(void);
/* Help > Keys... (EPDF) */
void keys_dialog(void);

#endif

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* gatedlg.h - Settings... (^,): where the Gateway is (EPDF, EConvert) */
#ifndef GATEDLG_H
#define GATEDLG_H

#define GATE_PORT 8380			/* pdfgate's (Erez's OK, 7 Nov 2026) */

/* the Gateway's address (host, hostcap bytes) and port, as they are shown
   and as the user leaves them; 1 when OK was chosen */
int gate_dialog(char *host, int hostcap, unsigned short *port);

#endif

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * fbalert.c - EConvert's two packages name each other (the guidelines' 7.4,
 * decided 22 Nov 2026: the standard package and the FireBee's, the same names):
 * common's cpucheck.c (ECONVERT.PRG, any 68000 and up) shows cpu_cf_alert on a
 * FireBee, cpucheck_cf.S (the FireBee package's build, common's mkfirebee.py)
 * cpu_alert on a machine without the _CF_ cookie. Linked last, so nothing that was
 * there moves off its cache places.
 */
#ifdef __mcoldfire__
const char cpu_alert[] = "[3][ECONVERT.PRG is for the|FireBee. On an ST, a TT or|a Falcon, install EPDF's|standard package.][ Quit ]";
#else
const long cpu_need = 0;		/* any 68000 */
const char cpu_alert[] = "";		/* (never shown: cpu_need is 0) */
const char cpu_cf_alert[] = "[3][ECONVERT.PRG is for the ST,|TT and Falcon. On the|FireBee, install EPDF's|FireBee package.][ Quit ]";
#endif

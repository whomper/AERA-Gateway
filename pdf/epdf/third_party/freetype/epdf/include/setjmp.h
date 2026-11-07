/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* setjmp / longjmp for the Atari's freestanding C library (FreeType's error handling needs them);
 * the host builds use the system's. */
#ifndef _SETJMP_H
#define _SETJMP_H
typedef long jmp_buf[14];	/* d2-d7, a2-a6, sp, return address */
int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val) __attribute__((noreturn));
#endif

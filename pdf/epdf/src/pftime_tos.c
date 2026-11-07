/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pftime_tos.c - what common's conn.c and net_tos.c take beyond the file
 * layer (pdfio_tos.c), for the Pi converter: the family's clock
 * (tos_ms(), corrected for a fast _hz_200), the date and time from
 * GEMDOS, the turn pf_idle gives the program while they wait, and the
 * bytes net_tos.c's DNS question takes as its id.
 */
#include <string.h>
#include "tos.h"
#include "plat.h"
#include "bearssl_hash.h"

void (*pf_idle)(void);

unsigned long pf_ms(void)
{
	return tos_ms();
}

void pf_now(PFTIME *t)
{
	unsigned short d = (unsigned short)Tgetdate(), tm = (unsigned short)Tgettime();

	t->year = (short)(1980 + (d >> 9));
	t->mon = (short)((d >> 5) & 15);
	t->day = (short)(d & 31);
	t->hour = (short)(tm >> 11);
	t->min = (short)((tm >> 5) & 63);
	t->sec = (short)((tm & 31) * 2);
}

/* bytes no one on the network can guess ahead (a DNS question's id: a
   fixed one lets anyone answer for the server). Not for keys: EPDF has no
   TLS. The clock to the millisecond, XBIOS Random() (a 24-bit generator
   seeded at boot), a count and where the stack is, mixed by SHA-256 */
void pf_entropy(unsigned char *buf, int n)
{
	static unsigned long count, pool[8];
	br_sha256_context h;
	unsigned char out[32];
	unsigned long mix[4];

	while (n > 0) {
		mix[0] = tos_ms();
		mix[1] = (unsigned long)trap14_w(17);	/* Random() */
		mix[2] = ++count;
		mix[3] = (unsigned long)&mix;
		br_sha256_init(&h);
		br_sha256_update(&h, pool, sizeof pool);
		br_sha256_update(&h, mix, sizeof mix);
		br_sha256_out(&h, out);
		memcpy(pool, out, sizeof pool);
		memcpy(buf, out, n < 16 ? (size_t)n : 16);
		buf += 16;
		n -= 16;
	}
}

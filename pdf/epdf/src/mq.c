/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * mq.c - the MQ arithmetic decoder (mq.h): T.800's conventions (the C
 * register's high half against Qe), which decode T.88's streams the same.
 */
#include "mq.h"

/* T.88 table E.1 / T.800 table C.2, as mq.h's mq_tab says */
#define E(q, nm, nl, sw) ((uint32_t)(q) << 16 | (uint32_t)(nm) << 9 | (uint32_t)(nl) << 1 | (sw))
const uint32_t mq_tab[47] = {
	E(0x5601, 1, 1, 1),   E(0x3401, 2, 6, 0),   E(0x1801, 3, 9, 0),   E(0x0AC1, 4, 12, 0),  E(0x0521, 5, 29, 0),
	E(0x0221, 38, 33, 0), E(0x5601, 7, 6, 1),   E(0x5401, 8, 14, 0),  E(0x4801, 9, 14, 0),  E(0x3801, 10, 14, 0),
	E(0x3001, 11, 17, 0), E(0x2401, 12, 18, 0), E(0x1C01, 13, 20, 0), E(0x1601, 29, 21, 0), E(0x5601, 15, 14, 1),
	E(0x5401, 16, 14, 0), E(0x5101, 17, 15, 0), E(0x4801, 18, 16, 0), E(0x3801, 19, 17, 0), E(0x3401, 20, 18, 0),
	E(0x3001, 21, 19, 0), E(0x2801, 22, 19, 0), E(0x2401, 23, 20, 0), E(0x2201, 24, 21, 0), E(0x1C01, 25, 22, 0),
	E(0x1801, 26, 23, 0), E(0x1601, 27, 24, 0), E(0x1401, 28, 25, 0), E(0x1201, 29, 26, 0), E(0x1101, 30, 27, 0),
	E(0x0AC1, 31, 28, 0), E(0x09C1, 32, 29, 0), E(0x08A1, 33, 30, 0), E(0x0521, 34, 31, 0), E(0x0441, 35, 32, 0),
	E(0x02A1, 36, 33, 0), E(0x0221, 37, 34, 0), E(0x0141, 38, 35, 0), E(0x0111, 39, 36, 0), E(0x0085, 40, 37, 0),
	E(0x0049, 41, 38, 0), E(0x0025, 42, 39, 0), E(0x0015, 43, 40, 0), E(0x0009, 44, 41, 0), E(0x0005, 45, 42, 0),
	E(0x0001, 45, 43, 0), E(0x5601, 46, 46, 0)
};
#undef E

void mq_byte_in(MQ *m)
{
	unsigned b = m->p < m->end ? *m->p : 0xFF;
	unsigned b1 = m->p + 1 < m->end ? m->p[1] : 0xFF;

	if (b == 0xFF) {
		if (b1 > 0x8F) {
			m->c += 0xFF00;		/* a marker, or the end: 1 bits from here */
			m->ct = 8;
		} else {
			m->p++;
			m->c += (uint32_t)b1 << 9;
			m->ct = 7;
		}
	} else {
		m->p++;
		m->c += (uint32_t)(m->p < m->end ? *m->p : 0xFF) << 8;
		m->ct = 8;
	}
}

void mq_init(MQ *m, const uint8_t *data, long n)
{
	m->p = data;
	m->end = data + (n > 0 ? n : 0);
	m->c = (uint32_t)(n > 0 ? *data : 0xFF) << 16;
	mq_byte_in(m);
	m->c <<= 7;
	m->ct -= 7;
	m->a = 0x8000;
}

int mq_decode(MQ *m, uint8_t *cx)
{
	uint32_t c, a;
	int ct, d;

	MQ_LOAD(m, c, a, ct);
	MQ_DECODE(m, c, a, ct, cx, d);
	MQ_SAVE(m, c, a, ct);
	return d;
}

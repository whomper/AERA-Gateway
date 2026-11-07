/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#include <stdlib.h>
#include <string.h>
#include "rscale.h"

/* the bits' counts, taken at the first scan (1.8 KB not resident) */
static uint8_t *pc8, *pc4h, *pc4l, (*pc2)[256];
static int ready;

static int tables(void)
{
	int b, i;
	uint8_t *t = malloc(256 * 7);

	if (!t)
		return -1;
	pc8 = t;
	pc4h = t + 256;
	pc4l = t + 512;
	pc2 = (uint8_t (*)[256])(t + 768);

	for (b = 0; b < 256; b++) {
		int n = 0;

		for (i = 0; i < 8; i++)
			n += (b >> i) & 1;
		pc8[b] = (uint8_t)n;
		pc4h[b] = (uint8_t)(((b >> 7) & 1) + ((b >> 6) & 1) + ((b >> 5) & 1) + ((b >> 4) & 1));
		pc4l[b] = (uint8_t)(((b >> 3) & 1) + ((b >> 2) & 1) + ((b >> 1) & 1) + (b & 1));
		for (i = 0; i < 4; i++)
			pc2[i][b] = (uint8_t)(((b >> (7 - 2 * i)) & 1) + ((b >> (6 - 2 * i)) & 1));
	}
	ready = 1;
	return 0;
}

int rs_init(RSCALE *r, int cols, int f)
{
	if (f != 2 && f != 4 && f != 8)
		return -1;
	if (!ready && tables() != 0)
		return -1;
	r->f = f;
	r->cols = cols;
	r->outw = (cols + f - 1) / f;
	r->rows_in = 0;
	r->acc = calloc((size_t)r->outw + 8, 2);
	return r->acc ? 0 : -1;
}

void rs_close(RSCALE *r)
{
	free(r->acc);
	r->acc = NULL;
}

int rs_row(RSCALE *r, const uint8_t *row, uint8_t *out)
{
	int nb = (r->cols + 7) >> 3, i;
	uint16_t *a = r->acc;

	if (r->f == 8) {
		for (i = 0; i < nb; i++)
			a[i] = (uint16_t)(a[i] + pc8[row[i]]);
	} else if (r->f == 4) {
		for (i = 0; i < nb; i++) {
			a[2 * i] = (uint16_t)(a[2 * i] + pc4h[row[i]]);
			a[2 * i + 1] = (uint16_t)(a[2 * i + 1] + pc4l[row[i]]);
		}
	} else {
		for (i = 0; i < nb; i++) {
			uint8_t b = row[i];

			a[4 * i] = (uint16_t)(a[4 * i] + pc2[0][b]);
			a[4 * i + 1] = (uint16_t)(a[4 * i + 1] + pc2[1][b]);
			a[4 * i + 2] = (uint16_t)(a[4 * i + 2] + pc2[2][b]);
			a[4 * i + 3] = (uint16_t)(a[4 * i + 3] + pc2[3][b]);
		}
	}
	if (++r->rows_in < r->f)
		return 0;
	{
		int n = r->f * r->f, x;

		/* ink 0 .. n -> 255 .. 0: a table of n + 1 entries */
		uint8_t lut[65];

		for (x = 0; x <= n; x++)
			lut[x] = (uint8_t)(255 - (x * 255) / n);
		for (x = 0; x < r->outw; x++) {
			out[x] = lut[a[x] > n ? n : a[x]];
			a[x] = 0;
		}
	}
	r->rows_in = 0;
	return 1;
}

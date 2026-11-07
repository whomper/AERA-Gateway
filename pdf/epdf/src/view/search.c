/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
#include <string.h>
#include "search.h"

static unsigned char fold[256];
static int folded;

/* each character as it is compared: small letters, and a blank for every
   space, line end and soft break */
static void fold_init(void)
{
	static const unsigned char pairs[] = {
		0x80, 0x87, 0x8E, 0x84, 0x8F, 0x86, 0x90, 0x82, 0x92, 0x91, 0x99, 0x94,
		0x9A, 0x81, 0xA5, 0xA4, 0xB6, 0xB0, 0xB7, 0xB1, 0xB2, 0xB3, 0xB5, 0xB4
	};
	int i;

	for (i = 0; i < 256; i++)
		fold[i] = (unsigned char)i;
	for (i = 'A'; i <= 'Z'; i++)
		fold[i] = (unsigned char)(i + 32);
	for (i = 0; i < (int)sizeof pairs; i += 2)
		fold[pairs[i]] = pairs[i + 1];
	fold['\r'] = fold['\n'] = fold['\t'] = ' ';
	folded = 1;
}

static int blank(unsigned char c)
{
	return fold[c] == ' ';
}

/* needle matches at text[p]: the length it covers there, or 0 */
static long match_at(const unsigned char *t, long len, const unsigned char *nd, long p)
{
	long q = p;

	while (*nd) {
		if (blank(*nd)) {
			const unsigned char *e = nd;

			while (*e && blank(*e))
				e++;
			if (!*e)
				break;		/* blanks at the needle's end ask for nothing */
			if (q >= len || !blank(t[q]))
				return 0;
			while (*nd && blank(*nd))
				nd++;
			while (q < len && blank(t[q]))
				q++;
			continue;
		}
		if (q >= len || fold[t[q]] != fold[*nd])
			return 0;
		q++;
		nd++;
	}
	return q - p;
}

int search_text(const char *text, long len, const char *needle, long from, long *at, long *n)
{
	const unsigned char *t = (const unsigned char *)text, *nd = (const unsigned char *)needle;
	unsigned char first;
	long p;

	if (!folded)
		fold_init();
	while (*nd && blank(*nd))
		nd++;
	if (!*nd)
		return 0;
	first = fold[*nd];
	for (p = from < 0 ? 0 : from; p < len; p++) {
		long m;

		if (fold[t[p]] != first)
			continue;
		if ((m = match_at(t, len, nd, p)) > 0) {
			/* a blank at the needle's end isn't taken from the text */
			while (m > 0 && blank(t[p + m - 1]))
				m--;
			*at = p;
			*n = m;
			return 1;
		}
	}
	return 0;
}

int search_text_back(const char *text, long len, const char *needle, long before, long *at, long *n)
{
	long p = 0, a, m, best = -1, bn = 0;

	while (search_text(text, len, needle, p, &a, &m) && a < before) {
		best = a;
		bn = m;
		p = a + 1;
	}
	if (best < 0)
		return 0;
	*at = best;
	*n = bn;
	return 1;
}

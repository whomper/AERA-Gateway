/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * deflate.c - a zlib stream (RFC 1950/1951) made, for EConvert's
 * FlateDecode streams: LZ77 over a 32 KB window (hash chains of three
 * bytes, the longest match of the first few tried, one step of lazy
 * matching), each block of up to 64 K symbols coded with its own Huffman
 * codes (dynamic blocks), lengths limited as the format wants.
 */
#include <stdlib.h>
#include <string.h>
#include "deflate.h"
#include "conv.h"

#define WSIZE 32768
#define WMASK (WSIZE - 1)
#define HBITS 15
#define HSIZE (1 << HBITS)
#define MINM 3
#define MAXM 258
#define CHAIN 48			/* matches tried at most */
#define NICE 128			/* a match this long is taken at once */
#define BLOCK 65536			/* symbols a block */

typedef struct {
	SBUF *out;
	unsigned long bits;
	int nb;
	int bad;
} BW;

static void put(BW *w, unsigned v, int n)
{
	w->bits |= (unsigned long)v << w->nb;
	w->nb += n;
	while (w->nb >= 8) {
		if (!sb_addc(w->out, (char)(w->bits & 255)))
			w->bad = 1;
		w->bits >>= 8;
		w->nb -= 8;
	}
}

static void flush(BW *w)
{
	if (w->nb > 0)
		put(w, 0, 8 - w->nb);
}

/* a Huffman code's lengths for n symbols of freq f, none longer than lim */
static void lengths(const unsigned long *f, int n, int lim, unsigned char *len)
{
	int i, m = 0, *sym, *par;
	unsigned long *w;

	memset(len, 0, (size_t)n);
	sym = malloc((size_t)n * 2 * sizeof *sym);
	par = malloc((size_t)n * 2 * sizeof *par);
	w = malloc((size_t)n * 2 * sizeof *w);
	if (!sym || !par || !w) {
		/* (no memory: every symbol the same length) */
		for (i = 0; i < n; i++)
			len[i] = f[i] ? (unsigned char)lim : 0;
		free(sym);
		free(par);
		free(w);
		return;
	}
	for (i = 0; i < n; i++)
		if (f[i])
			sym[m++] = i;
	if (m == 0)
		goto done;
	if (m == 1) {
		len[sym[0]] = 1;
		goto done;
	}
	{
		/* Huffman's tree by two sorted queues: the leaves by weight, then
		   the nodes as they are made (they come in order) */
		int nl = m, li = 0, ni = 0, nn = 0, k;
		int *node = malloc((size_t)m * sizeof *node);

		if (!node) {
			for (i = 0; i < m; i++)
				len[sym[i]] = (unsigned char)lim;
			goto done;
		}
		/* the leaves sorted by weight, equal ones in their order (a
		   merge, bottom up, through par before it is used: an insertion
		   sort of the 286 literals was 40 ms of a Falcon's turn) */
		{
			int run, *from = sym, *to = par;

			for (run = 1; run < nl; run *= 2) {
				int lo;

				for (lo = 0; lo < nl; lo += 2 * run) {
					int mid = lo + run < nl ? lo + run : nl, hi = lo + 2 * run < nl ? lo + 2 * run : nl;
					int a = lo, b = mid, o = lo;

					while (a < mid && b < hi)
						to[o++] = f[from[b]] < f[from[a]] ? from[b++] : from[a++];
					while (a < mid)
						to[o++] = from[a++];
					while (b < hi)
						to[o++] = from[b++];
				}
				{
					int *t = from;

					from = to;
					to = t;
				}
			}
			if (from != sym)
				memcpy(sym, from, (size_t)nl * sizeof *sym);
		}
		for (i = 0; i < nl; i++) {
			w[i] = f[sym[i]];
			par[i] = -1;
		}
		for (k = 0; k < nl - 1; k++) {
			int a, b;

			a = (li < nl && (ni >= nn || w[li] <= w[nl + node[ni] - nl])) ? li++ : node[ni++];
			b = (li < nl && (ni >= nn || w[li] <= w[nl + node[ni] - nl])) ? li++ : node[ni++];
			w[nl + k] = w[a] + w[b];
			par[nl + k] = -1;
			par[a] = nl + k;
			par[b] = nl + k;
			node[nn++] = nl + k;
		}
		free(node);
		{
			/* each leaf's depth; then the deepest cut to lim (moving
			   leaves up, zlib's way: count by length, fix the overflow) */
			int bl[32] = { 0 }, over = 0, d;
			unsigned char *dep = nl >= 2 ? malloc((size_t)nl * 2) : NULL;

			if (!dep) {
				for (i = 0; i < m; i++)
					len[sym[i]] = (unsigned char)lim;
				goto done;
			}
			dep[2 * nl - 2] = 0;
			for (k = 2 * nl - 3; k >= 0; k--)
				dep[k] = (unsigned char)(dep[par[k]] + 1);
			for (i = 0; i < nl; i++) {
				d = dep[i];
				if (d > lim) {
					d = lim;
					over++;
				}
				bl[d]++;
			}
			free(dep);
			while (over > 0) {
				int b2 = lim - 1;

				while (bl[b2] == 0)
					b2--;
				bl[b2]--;
				bl[b2 + 1] += 2;
				bl[lim]--;
				over -= 2;
			}
			/* lengths given out again: the lightest leaves the longest */
			k = 0;
			for (d = lim; d >= 1; d--)
				for (i = 0; i < bl[d]; i++)
					len[sym[k++]] = (unsigned char)d;
		}
	}
done:
	free(sym);
	free(par);
	free(w);
}

/* canonical codes from lengths, bit-reversed for the stream */
static void codes(const unsigned char *len, int n, unsigned short *code)
{
	int bl[16] = { 0 }, next[16], i, c = 0, b;

	for (i = 0; i < n; i++)
		bl[len[i]]++;
	bl[0] = 0;
	for (b = 1; b < 16; b++) {
		c = (c + bl[b - 1]) << 1;
		next[b] = c;
	}
	for (i = 0; i < n; i++)
		if (len[i]) {
			unsigned v = (unsigned)next[len[i]]++, r = 0;
			int k;

			for (k = 0; k < len[i]; k++) {
				r = (r << 1) | (v & 1);
				v >>= 1;
			}
			code[i] = (unsigned short)r;
		}
}

static const unsigned short lbase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
					  67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const unsigned char lext[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4,
					5, 5, 5, 5, 0 };
static const unsigned short dbase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513,
					  769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const unsigned char dext[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10,
					11, 11, 12, 12, 13, 13 };

/* a length's code (3..258) and a distance's (1..32768), from tables made
   once: the searches were most of a block's time on a Falcon */
static unsigned char ltab[259], dtab_lo[257], dtab_hi[256];
static int tabs_made;

static void make_tabs(void)
{
	int i, c;

	for (i = 3, c = 0; i <= 258; i++) {
		while (c < 28 && lbase[c + 1] <= i)
			c++;
		ltab[i] = (unsigned char)c;
	}
	for (i = 1, c = 0; i <= 256; i++) {
		while (c < 29 && dbase[c + 1] <= i)
			c++;
		dtab_lo[i] = (unsigned char)c;
	}
	for (i = 0, c = 0; i < 256; i++) {	/* distances 257..32768 by (d - 1) >> 7 */
		int d = (i << 7) + 1;

		while (c < 29 && dbase[c + 1] <= d)
			c++;
		dtab_hi[i] = (unsigned char)c;
	}
	tabs_made = 1;
}

static int lcode(int l)
{
	return ltab[l];
}

static int dcode(int d)
{
	return d <= 256 ? dtab_lo[d] : dtab_hi[(d - 1) >> 7];
}

/* a symbol: a literal (len 0) or a match (len, dist) */
typedef struct {
	unsigned short len, dist;	/* len 0: dist is the literal */
} SYM;

static const unsigned char clorder[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

static void block(BW *w, const SYM *s, long n, int last)
{
	unsigned long lf[286] = { 0 }, df[30] = { 0 }, cf[19] = { 0 };
	unsigned char ll[286], dl[30], cl[19], all[316];
	unsigned short lc[286], dc[30], cc[19];
	unsigned char rl[316], rx[316];
	int nl = 286, nd = 30, nc = 19, na, nr = 0, i;
	long k;

	for (k = 0; k < n; k++) {
		if ((k & 255) == 255 && cv_turn)
			cv_turn();
		if (!s[k].len)
			lf[s[k].dist]++;
		else {
			lf[257 + lcode(s[k].len)]++;
			df[dcode(s[k].dist)]++;
		}
	}
	lf[256] = 1;
	cv_phase = "deflate-codes";
	if (cv_turn)
		cv_turn();
	lengths(lf, 286, 15, ll);
	if (cv_turn)
		cv_turn();	/* (the two lengths() in one turn: 40 ms on a Falcon) */
	lengths(df, 30, 15, dl);
	if (cv_turn)
		cv_turn();
	/* a code of one symbol made two (a complete code: some decoders
	   refuse one that isn't) */
	{
		int a = 0, b = 0, j;

		for (j = 0; j < 30; j++)
			a += dl[j] != 0;
		for (j = 0; j < 286; j++)
			b += ll[j] != 0;
		if (a < 2) {
			if (!dl[0])
				dl[0] = 1;
			else
				dl[1] = 1;
			if (a == 0)
				dl[1] = 1;
		}
		if (b < 2)
			ll[ll[0] ? 1 : 0] = 1;	/* (256 has 1) */
	}
	while (nl > 257 && !ll[nl - 1])
		nl--;
	while (nd > 1 && !dl[nd - 1])
		nd--;
	memcpy(all, ll, (size_t)nl);
	memcpy(all + nl, dl, (size_t)nd);
	na = nl + nd;
	/* the lengths run-length coded (16 repeat, 17/18 zeros) */
	for (i = 0; i < na;) {
		int v = all[i], r = 1;

		while (i + r < na && all[i + r] == v)
			r++;
		if (v == 0 && r >= 3) {
			int t = r > 138 ? 138 : r;

			rl[nr] = t >= 11 ? 18 : 17;
			rx[nr++] = (unsigned char)(t >= 11 ? t - 11 : t - 3);
			i += t;
		} else if (v != 0 && r >= 4) {
			int t = r - 1 > 6 ? 6 : r - 1;

			rl[nr] = (unsigned char)v;
			rx[nr++] = 0;
			rl[nr] = 16;
			rx[nr++] = (unsigned char)(t - 3);
			i += t + 1;
		} else {
			rl[nr] = (unsigned char)v;
			rx[nr++] = 0;
			i++;
		}
	}
	for (i = 0; i < nr; i++)
		cf[rl[i]]++;
	lengths(cf, 19, 7, cl);
	while (nc > 4 && !cl[clorder[nc - 1]])
		nc--;
	if (cv_turn)
		cv_turn();
	codes(ll, 286, lc);
	codes(dl, 30, dc);
	codes(cl, 19, cc);
	cv_phase = "deflate-emit";
	put(w, last ? 1 : 0, 1);
	put(w, 2, 2);			/* dynamic */
	put(w, (unsigned)(nl - 257), 5);
	put(w, (unsigned)(nd - 1), 5);
	put(w, (unsigned)(nc - 4), 4);
	for (i = 0; i < nc; i++)
		put(w, cl[clorder[i]], 3);
	for (i = 0; i < nr; i++) {
		put(w, cc[rl[i]], cl[rl[i]]);
		if (rl[i] == 16)
			put(w, rx[i], 2);
		else if (rl[i] == 17)
			put(w, rx[i], 3);
		else if (rl[i] == 18)
			put(w, rx[i], 7);
	}
	for (k = 0; k < n; k++) {
		if ((k & 127) == 127 && cv_turn)
			cv_turn();	/* (a block's 64 K symbols written: 300 ms on a Falcon) */
		if (!s[k].len)
			put(w, lc[s[k].dist], ll[s[k].dist]);
		else {
			int a = lcode(s[k].len), b = dcode(s[k].dist);

			put(w, lc[257 + a], ll[257 + a]);
			if (lext[a])
				put(w, (unsigned)(s[k].len - lbase[a]), lext[a]);
			put(w, dc[b], dl[b]);
			if (dext[b])
				put(w, (unsigned)(s[k].dist - dbase[b]), dext[b]);
		}
	}
	put(w, lc[256], ll[256]);
}

static unsigned long adler(const unsigned char *d, long n)
{
	unsigned long a = 1, b = 0;

	while (n > 0) {
		long k = n > 5552 ? 5552 : n;

		n -= k;
		while (k--) {
			a += *d++;
			b += a;
		}
		if (cv_turn)
			cv_turn();	/* (a colour page's 265 KB: 300 ms on a Falcon) */
		a %= 65521;
		b %= 65521;
	}
	return b << 16 | a;
}

/* padding in a branch only refused memory takes: the code after this file
   at the same places in a 68030's instruction cache (addresses mod 256) as
   before the turns in block() and lengths()'s merge (tools/rpad.py) */
#ifndef DPAD_A
#define DPAD_A 80
#endif
#define DPAD_S2(n) #n
#define DPAD_S(n) DPAD_S2(n)
#if defined(__m68k__) || defined(__mc68000__)
#define DPAD(n) __asm__ volatile("bra.w 1f\n\t.skip " DPAD_S(n) "\n1:")
#else
#define DPAD(n) ((void)0)
#endif

int zdeflate(const unsigned char *d, long n, SBUF *out)
{
	BW w;
	SYM *sy;
	long *head, *prev, i = 0, ns = 0;
	unsigned long ad;

	memset(&w, 0, sizeof w);
	w.out = out;
	if (!tabs_made)
		make_tabs();
	cv_phase = "deflate-init";
	sy = malloc(BLOCK * sizeof *sy);
	head = malloc(HSIZE * sizeof *head);
	prev = malloc(WSIZE * sizeof *prev);
	if (!sy || !head || !prev) {
		DPAD(DPAD_A);
		free(sy);
		free(head);
		free(prev);
		return -1;
	}
	/* each is written before block() reads it; the first cleared for
	   GCC's -Wmaybe-uninitialized on the host */
	memset(sy, 0, sizeof *sy);
	for (i = 0; i < HSIZE; i++)
		head[i] = -1;
	if (cv_turn)
		cv_turn();
	cv_phase = "deflate-parse";
	sb_addc(out, 0x78);
	sb_addc(out, (char)0x9C);
#define HASH(p) ((((unsigned long)(p)[0] << 10) ^ ((unsigned long)(p)[1] << 5) ^ (p)[2]) & (HSIZE - 1))
	i = 0;
	while (i < n) {
		int best = 0, bd = 0;

		if (i + MINM <= n) {
			unsigned long h = HASH(d + i);
			long c = head[h];
			int tries = CHAIN, lim = n - i < MAXM ? (int)(n - i) : MAXM;

			while (c >= 0 && i - c <= WSIZE - 1 && tries--) {
				if (d[c + best] == d[i + best] && d[c] == d[i]) {
					int l = 0;

					while (l < lim && d[c + l] == d[i + l])
						l++;
					if (l > best) {
						best = l;
						bd = (int)(i - c);
						/* good enough (zlib's nice length): a dither's
						   patterns match long at every candidate, 48
						   compares of 250 bytes a symbol */
						if (l >= lim || l >= NICE)
							break;
					}
				}
				/* a long one found: fewer candidates after it (zlib's
				   good length) */
				if (best >= 32 && tries > 8)
					tries = 8;
				c = prev[c & WMASK];
			}
			prev[i & WMASK] = head[h];
			head[h] = i;
		}
		if (best >= MINM) {
			long j;

			sy[ns].len = (unsigned short)best;
			sy[ns].dist = (unsigned short)bd;
			ns++;
			/* a long match's bytes aren't looked up later (white runs
			   of a page: each byte hashed was most of the time) */
			for (j = i + 1; j < i + best && j < i + 32 && j + MINM <= n; j++) {
				unsigned long h = HASH(d + j);

				prev[j & WMASK] = head[h];
				head[h] = j;
			}
			i += best;
		} else {
			sy[ns].len = 0;
			sy[ns].dist = d[i];
			ns++;
			i++;
		}
		if ((ns & 15) == 15 && cv_turn)
			cv_turn();
		if (ns == BLOCK) {
			block(&w, sy, ns, 0);
			ns = 0;
		}
	}
	block(&w, sy, ns, 1);
	flush(&w);
	cv_phase = "deflate-adler";
	ad = adler(d, n);
	sb_addc(out, (char)(ad >> 24));
	sb_addc(out, (char)(ad >> 16));
	sb_addc(out, (char)(ad >> 8));
	sb_addc(out, (char)ad);
	free(sy);
	free(head);
	free(prev);
	return w.bad ? -1 : 0;
}

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * jpegenc.c - a baseline JPEG made (EConvert's photos with the batch's
 * "JPEG (small)"): Y Cb Cr with the colour halved each way (4:2:0), or
 * grey; the DCT in fixed point (the separable one, 13-bit cosines); the
 * standard tables of the JPEG specification's annex K, scaled to the
 * quality as libjpeg scales them. What TJpgDec and libjpeg both read.
 */
#include <stdlib.h>
#include <string.h>
#include "jpegenc.h"

static const unsigned char zz[64] = {
	0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21,
	28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61,
	54, 47, 55, 62, 63 };
static const unsigned char qlum[64] = {
	16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55, 14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51,
	87, 80, 62, 18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92, 49, 64, 78, 87, 103, 121, 120,
	101, 72, 92, 95, 98, 112, 100, 103, 99 };
static const unsigned char qchr[64] = {
	17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99, 24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99,
	99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
	99, 99, 99, 99, 99, 99 };
/* annex K's Huffman tables: the counts of each length, then the symbols */
static const unsigned char dc_l_bits[16] = { 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
static const unsigned char dc_c_bits[16] = { 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
static const unsigned char dc_val[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
static const unsigned char ac_l_bits[16] = { 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
static const unsigned char ac_l_val[162] = {
	0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14,
	0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09,
	0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a,
	0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65,
	0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
	0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
	0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca,
	0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
	0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa };
static const unsigned char ac_c_bits[16] = { 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
static const unsigned char ac_c_val[162] = {
	0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32,
	0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16,
	0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39,
	0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64,
	0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86,
	0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
	0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
	0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
	0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa };

typedef struct {
	unsigned short code[256];
	unsigned char len[256];
} HT;

typedef struct {
	SBUF *o;
	unsigned long acc;
	int nb, bad;
	HT dcl, acl, dcc, acc_c;
	unsigned char ql[64], qc[64];
} JE;

static void ht_make(HT *h, const unsigned char *bits, const unsigned char *val)
{
	int l, i, k = 0;
	unsigned c = 0;

	memset(h, 0, sizeof *h);
	for (l = 1; l <= 16; l++) {
		for (i = 0; i < bits[l - 1]; i++) {
			h->code[val[k]] = (unsigned short)c;
			h->len[val[k]] = (unsigned char)l;
			k++;
			c++;
		}
		c <<= 1;
	}
}

static void byte(JE *j, int b)
{
	if (!sb_addc(j->o, (char)b))
		j->bad = 1;
}

static void put(JE *j, unsigned v, int n)
{
	j->acc = (j->acc << n) | (v & ((1u << n) - 1));
	j->nb += n;
	while (j->nb >= 8) {
		int b = (int)(j->acc >> (j->nb - 8)) & 255;

		byte(j, b);
		if (b == 0xFF)
			byte(j, 0);	/* (a stuffed zero) */
		j->nb -= 8;
	}
}

static void marker(JE *j, int m, int len)
{
	byte(j, 0xFF);
	byte(j, m);
	byte(j, len >> 8);
	byte(j, len & 255);
}

static void quality(unsigned char *q, const unsigned char *base, int qual)
{
	int s = qual < 50 ? 5000 / qual : 200 - 2 * qual, i;

	for (i = 0; i < 64; i++) {
		int v = (base[i] * s + 50) / 100;

		q[i] = (unsigned char)(v < 1 ? 1 : v > 255 ? 255 : v);
	}
}

/* the 8 x 8 DCT of b (samples less 128), quantised by q, in zigzag order:
   separable, cosines in 13 bits */
static void fdct(const int *b, const unsigned char *q, int *out)
{
	static int cs[8][8], made;
	long t[64], o[64];
	int u, v, x;

	if (!made) {
		/* round(cos((2x + 1) u pi / 16) * c(u) / 2 * 8192), c(0) = 1/sqrt 2 */
		static const short c[8][8] = {
			{ 2896, 2896, 2896, 2896, 2896, 2896, 2896, 2896 },
			{ 4017, 3406, 2276, 799, -799, -2276, -3406, -4017 },
			{ 3784, 1567, -1567, -3784, -3784, -1567, 1567, 3784 },
			{ 3406, -799, -4017, -2276, 2276, 4017, 799, -3406 },
			{ 2896, -2896, -2896, 2896, 2896, -2896, -2896, 2896 },
			{ 2276, -4017, 799, 3406, -3406, -799, 4017, -2276 },
			{ 1567, -3784, 3784, -1567, -1567, 3784, -3784, 1567 },
			{ 799, -2276, 3406, -4017, 4017, -3406, 2276, -799 } };

		for (u = 0; u < 8; u++)
			for (x = 0; x < 8; x++)
				cs[u][x] = c[u][x];
		made = 1;
	}
	for (v = 0; v < 8; v++)		/* rows */
		for (u = 0; u < 8; u++) {
			long s = 0;

			for (x = 0; x < 8; x++)
				s += (long)cs[u][x] * b[v * 8 + x];
			t[v * 8 + u] = s;	/* 13 bits up */
		}
	for (u = 0; u < 8; u++)		/* columns */
		for (v = 0; v < 8; v++) {
			long s = 0, d;
			int k;

			for (x = 0; x < 8; x++)
				s += (long)cs[v][x] * (t[x * 8 + u] >> 7);	/* (32 bits at most on the Atari) */
			s >>= 19;		/* (13 + 13 - 7 bits up) */
			k = v * 8 + u;
			d = q[k];
			o[k] = s >= 0 ? (s + d / 2) / d : -((-s + d / 2) / d);	/* (t is still read: a column's rows) */
		}
	for (x = 0; x < 64; x++)
		out[x] = (int)o[zz[x]];
}

static int bits_of(int v)
{
	int n = 0;

	if (v < 0)
		v = -v;
	while (v) {
		n++;
		v >>= 1;
	}
	return n;
}

static void block(JE *j, const int *b, const unsigned char *q, const HT *dc, const HT *ac, int *pred)
{
	int c[64], d, n, i, run = 0;

	fdct(b, q, c);
	d = c[0] - *pred;
	*pred = c[0];
	n = bits_of(d);
	put(j, dc->code[n], dc->len[n]);
	if (n)
		put(j, (unsigned)(d < 0 ? d - 1 : d), n);
	for (i = 1; i < 64; i++) {
		if (!c[i]) {
			run++;
			continue;
		}
		while (run > 15) {
			put(j, ac->code[0xF0], ac->len[0xF0]);
			run -= 16;
		}
		n = bits_of(c[i]);
		put(j, ac->code[run << 4 | n], ac->len[run << 4 | n]);
		put(j, (unsigned)(c[i] < 0 ? c[i] - 1 : c[i]), n);
		run = 0;
	}
	if (run)
		put(j, ac->code[0], ac->len[0]);
}

static void dqt(JE *j, int id, const unsigned char *q)
{
	int i;

	marker(j, 0xDB, 67);
	byte(j, id);
	for (i = 0; i < 64; i++)
		byte(j, q[zz[i]]);
}

static void dht(JE *j, int cls_id, const unsigned char *bits, const unsigned char *val, int nval)
{
	int i;

	marker(j, 0xC4, 19 + nval);
	byte(j, cls_id);
	for (i = 0; i < 16; i++)
		byte(j, bits[i]);
	for (i = 0; i < nval; i++)
		byte(j, val[i]);
}

#define R8(c) ((((c) >> 11) << 3) | ((c) >> 13))
#define G8(c) (((((c) >> 5) & 63) << 2) | (((c) >> 9) & 3))
#define B8(c) ((((c) & 31) << 3) | (((c) >> 2) & 7))

/* a pixel's Y, Cb or Cr (0 .. 255), the edge's pixel past the picture */
static int sample(const uint16_t *px, int w, int h, int x, int y, int ch)
{
	int c, r, g, b;

	if (x >= w) x = w - 1;
	if (y >= h) y = h - 1;
	c = px[(long)y * w + x];
	r = R8(c);
	g = G8(c);
	b = B8(c);
	if (ch == 0)
		return (r * 19595 + g * 38470 + b * 7471 + 32768) >> 16;
	if (ch == 1)
		return ((-r * 11059 - g * 21709 + b * 32768) >> 16) + 128;
	return ((r * 32768 - g * 27439 - b * 5329) >> 16) + 128;
}

int jpeg_encode(const uint16_t *px, int w, int h, int grey, int qual, SBUF *out)
{
	JE j;
	int mx, my, pred[3] = { 0, 0, 0 }, b[64];

	memset(&j, 0, sizeof j);
	j.o = out;
	quality(j.ql, qlum, qual);
	quality(j.qc, qchr, qual);
	ht_make(&j.dcl, dc_l_bits, dc_val);
	ht_make(&j.acl, ac_l_bits, ac_l_val);
	ht_make(&j.dcc, dc_c_bits, dc_val);
	ht_make(&j.acc_c, ac_c_bits, ac_c_val);
	byte(&j, 0xFF);
	byte(&j, 0xD8);
	dqt(&j, 0, j.ql);
	if (!grey)
		dqt(&j, 1, j.qc);
	marker(&j, 0xC0, grey ? 11 : 17);
	byte(&j, 8);
	byte(&j, h >> 8);
	byte(&j, h & 255);
	byte(&j, w >> 8);
	byte(&j, w & 255);
	byte(&j, grey ? 1 : 3);
	byte(&j, 1);
	byte(&j, grey ? 0x11 : 0x22);
	byte(&j, 0);
	if (!grey) {
		byte(&j, 2); byte(&j, 0x11); byte(&j, 1);
		byte(&j, 3); byte(&j, 0x11); byte(&j, 1);
	}
	dht(&j, 0x00, dc_l_bits, dc_val, 12);
	dht(&j, 0x10, ac_l_bits, ac_l_val, 162);
	if (!grey) {
		dht(&j, 0x01, dc_c_bits, dc_val, 12);
		dht(&j, 0x11, ac_c_bits, ac_c_val, 162);
	}
	marker(&j, 0xDA, grey ? 8 : 12);
	byte(&j, grey ? 1 : 3);
	byte(&j, 1);
	byte(&j, 0x00);
	if (!grey) {
		byte(&j, 2); byte(&j, 0x11);
		byte(&j, 3); byte(&j, 0x11);
	}
	byte(&j, 0);
	byte(&j, 63);
	byte(&j, 0);
	{
		int step = grey ? 8 : 16;

		for (my = 0; my < h; my += step)
			for (mx = 0; mx < w; mx += step) {
				int bx, by, x, y, ch;

				for (by = 0; by < step; by += 8)
					for (bx = 0; bx < step; bx += 8) {
						for (y = 0; y < 8; y++)
							for (x = 0; x < 8; x++)
								b[y * 8 + x] = sample(px, w, h, mx + bx + x, my + by + y, 0) - 128;
						block(&j, b, j.ql, &j.dcl, &j.acl, &pred[0]);
					}
				if (grey)
					continue;
				for (ch = 1; ch < 3; ch++) {
					/* the colour of each 2 x 2, averaged */
					for (y = 0; y < 8; y++)
						for (x = 0; x < 8; x++) {
							int px0 = mx + 2 * x, py0 = my + 2 * y;

							b[y * 8 + x] = ((sample(px, w, h, px0, py0, ch) + sample(px, w, h, px0 + 1, py0, ch) +
									 sample(px, w, h, px0, py0 + 1, ch) +
									 sample(px, w, h, px0 + 1, py0 + 1, ch) + 2) >> 2) - 128;
						}
					block(&j, b, j.qc, &j.dcc, &j.acc_c, &pred[ch]);
				}
			}
	}
	if (j.nb > 0)
		put(&j, 0x7F, 8 - j.nb);	/* (padded with ones) */
	byte(&j, 0xFF);
	byte(&j, 0xD9);
	return j.bad ? -1 : 0;
}

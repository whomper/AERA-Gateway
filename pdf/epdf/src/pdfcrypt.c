/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pdfcrypt.c - the standard security handler with the empty user password:
 * RC4 40 and 128 bit (V1, V2), AES-128 (V4) and AES-256 (V5, revisions 5
 * and 6). MD5, SHA-2 and AES are BearSSL's (common/third_party/bearssl);
 * RC4 is here (BearSSL has none).
 */
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pdfint.h"
#include "bearssl_hash.h"
#include "bearssl_block.h"

enum { M_NONE, M_RC4, M_AES128, M_AES256 };

struct pcrypt {
	int v, r;
	int stm, str;			/* methods */
	unsigned char key[32];
	int klen;
};

static const unsigned char PAD[32] = {
	0x28, 0xBF, 0x4E, 0x5E, 0x4E, 0x75, 0x8A, 0x41, 0x64, 0x00, 0x4E, 0x56, 0xFF, 0xFA, 0x01, 0x08,
	0x2E, 0x2E, 0x00, 0xB6, 0xD0, 0x68, 0x3E, 0x80, 0x2F, 0x0C, 0xA9, 0xFE, 0x64, 0x53, 0x69, 0x7A,
};

static void rc4(const unsigned char *key, int klen, unsigned char *buf, long n)
{
	unsigned char s[256];
	int i, j = 0, a, b = 0;
	long k;

	for (i = 0; i < 256; i++)
		s[i] = (unsigned char)i;
	for (i = 0; i < 256; i++) {
		j = (j + s[i] + key[i % klen]) & 255;
		a = s[i]; s[i] = s[j]; s[j] = (unsigned char)a;
	}
	i = j = 0;
	for (k = 0; k < n; k++) {
		i = (i + 1) & 255;
		j = (j + s[i]) & 255;
		a = s[i]; s[i] = s[j]; s[j] = (unsigned char)a;
		b = (s[i] + s[j]) & 255;
		buf[k] ^= s[b];
	}
}

static void md5(unsigned char *out, const void *a, long an, const void *b, long bn, const void *c, long cn)
{
	br_md5_context x;

	br_md5_init(&x);
	if (an) br_md5_update(&x, a, (size_t)an);
	if (bn) br_md5_update(&x, b, (size_t)bn);
	if (cn) br_md5_update(&x, c, (size_t)cn);
	br_md5_out(&x, out);
}

static const unsigned char *str_of(PDOC *d, POBJ *dict, const char *key, long *n)
{
	POBJ *o = pd_get(d, dict, key);

	if (!o || o->t != PO_STR)
		return NULL;
	*n = o->a;
	return (const unsigned char *)o->u.p;
}

static void aes_cbc_dec(unsigned char *key, int klen, unsigned char *iv, unsigned char *data, long n)
{
	br_aes_big_cbcdec_keys c;

	br_aes_big_cbcdec_init(&c, key, (size_t)klen);
	br_aes_big_cbcdec_run(&c, iv, data, (size_t)n);
}

/* d's stop hook between pieces of a long hash (an AES-256 file's key: a
   minute and more on a 68000): 1 when the document is closing */
static int crypt_pause(PDOC *d)
{
	if (d->stop && !d->stopping && d->stop(d->stop_ctx))
		d->stopping = 1;
	return d->stopping;
}

/* the pieces: about 30 ms each on an 8 MHz ST (AES some 900 cycles a
   byte, SHA-384/512 some 2000) */
#define PIECE_AES 256
#define PIECE_SHA 128

/* ISO 32000-2, algorithm 2.B (revision 6) / 2.A (revision 5): the hash of
   the (empty) password; -1 out of memory or the document closing (its
   stop hook, between pieces) */
static int hash_r56(PDOC *d, int r, const unsigned char *salt, const unsigned char *udata, long un, unsigned char *out)
{
	unsigned char k[64], e_last = 0;
	int klen = 32, i = 0;

	{
		br_sha256_context c;

		br_sha256_init(&c);
		br_sha256_update(&c, salt, 8);
		if (un)
			br_sha256_update(&c, udata, (size_t)un);
		br_sha256_out(&c, k);
	}
	if (r == 5) {
		memcpy(out, k, 32);
		return 0;
	}
	do {
		long m = klen + un, rep = 64 * m, j;
		unsigned char *k1 = pdf_malloc(d, rep + 16);
		unsigned char iv[16];
		int mod = 0;
		union {
			br_aes_big_cbcenc_keys a;
			br_sha256_context s256;
			br_sha384_context s384;
			br_sha512_context s512;
		} c;

		if (!k1)
			return -1;
		for (j = 0; j < 64; j++) {
			memcpy(k1 + j * m, k, (size_t)klen);
			if (un)
				memcpy(k1 + j * m + klen, udata, (size_t)un);
		}
		memcpy(iv, k + 16, 16);
		br_aes_big_cbcenc_init(&c.a, k, 16);
		for (j = 0; j < rep; j += PIECE_AES) {
			if (crypt_pause(d)) {
				pdf_free(d, k1, rep + 16);
				return -1;
			}
			br_aes_big_cbcenc_run(&c.a, iv, k1 + j, (size_t)(rep - j < PIECE_AES ? rep - j : PIECE_AES));
		}
		for (j = 0; j < 16; j++)
			mod += k1[j];
		mod %= 3;
		e_last = k1[rep - 1];
		if (mod == 0)
			br_sha256_init(&c.s256);
		else if (mod == 1)
			br_sha384_init(&c.s384);
		else
			br_sha512_init(&c.s512);
		for (j = 0; j < rep; j += PIECE_SHA) {
			size_t n = (size_t)(rep - j < PIECE_SHA ? rep - j : PIECE_SHA);

			if (crypt_pause(d)) {
				pdf_free(d, k1, rep + 16);
				return -1;
			}
			if (mod == 0)
				br_sha256_update(&c.s256, k1 + j, n);
			else if (mod == 1)
				br_sha384_update(&c.s384, k1 + j, n);
			else
				br_sha512_update(&c.s512, k1 + j, n);
		}
		if (mod == 0) {
			br_sha256_out(&c.s256, k);
			klen = 32;
		} else if (mod == 1) {
			br_sha384_out(&c.s384, k);
			klen = 48;
		} else {
			br_sha512_out(&c.s512, k);
			klen = 64;
		}
		pdf_free(d, k1, rep + 16);
		i++;
	} while (i < 64 || e_last > i - 32);
	memcpy(out, k, 32);
	return 0;
}

static int cf_method(PDOC *d, POBJ *enc, const char *which, int v)
{
	const char *nm = pd_name(d, enc, which);
	POBJ *cf, *f;
	const char *cfm;

	if (v < 4)
		return M_RC4;
	if (!nm || !strcmp(nm, "Identity"))
		return M_NONE;
	cf = pd_get(d, enc, "CF");
	f = cf ? pd_get(d, cf, nm) : NULL;
	cfm = f ? pd_name(d, f, "CFM") : NULL;
	if (!cfm || !strcmp(cfm, "None"))
		return M_NONE;
	if (!strcmp(cfm, "V2"))
		return M_RC4;
	if (!strcmp(cfm, "AESV2"))
		return M_AES128;
	if (!strcmp(cfm, "AESV3"))
		return M_AES256;
	return -1;
}

int pc_open(PDOC *d, POBJ *enc, POBJ *id)
{
	PCRYPT *c;
	const char *filt;
	long on = 0, un = 0, idn = 0, oen = 0, uen = 0;
	const unsigned char *O, *U, *ID = NULL, *OE, *UE;
	long P;
	POBJ *idel;
	int v, r, n, i;

	if (!enc || enc->t != PO_DICT)
		return 1;
	filt = pd_name(d, enc, "Filter");
	if (!filt || strcmp(filt, "Standard"))
		return 1;
	v = (int)pd_int(d, enc, "V", 0);
	r = (int)pd_int(d, enc, "R", 2);
	if (v < 1 || v == 3 || v > 5 || r < 2 || r > 6)
		return 1;
	c = calloc(1, sizeof *c);
	if (!c)
		return 1;
	c->v = v;
	c->r = r;
	c->stm = cf_method(d, enc, "StmF", v);
	c->str = cf_method(d, enc, "StrF", v);
	if (c->stm < 0 || c->str < 0) {
		free(c);
		return 1;
	}
	O = str_of(d, enc, "O", &on);
	U = str_of(d, enc, "U", &un);
	OE = str_of(d, enc, "OE", &oen);
	UE = str_of(d, enc, "UE", &uen);
	P = pd_int(d, enc, "P", 0);
	if (!O || !U) {
		free(c);
		return 1;
	}
	idel = id && id->t == PO_ARR ? pdf_res(d, pa_at(id, 0)) : NULL;
	if (idel && idel->t == PO_STR) {
		ID = (const unsigned char *)idel->u.p;
		idn = idel->a;
	}
	if (v >= 5) {
		unsigned char h[32], ik[32], iv[16];

		if (un < 48 || uen < 32 || (r != 5 && r != 6)) {
			free(c);
			return 1;
		}
		/* the file key from the empty password (its key salt), then
		   checked by /Perms (ISO 32000-2, algorithm 13: "adb" in its
		   bytes 9 to 11 when the key is right): one hash where the
		   password's own check took another, the most of an open on a
		   68000 (revision 6: 165 s on an 8 MHz ST, 1 Nov 2026). Without
		   /Perms, the password's hash as before */
		long pn;
		const unsigned char *Pm = str_of(d, enc, "Perms", &pn);

		if (!Pm || pn < 16) {
			if (hash_r56(d, r, U + 32, NULL, 0, h) != 0 || memcmp(h, U, 32) != 0) {
				free(c);
				return 1;	/* the user password is not empty */
			}
		}
		if (hash_r56(d, r, U + 40, NULL, 0, ik) != 0) {
			free(c);
			return 1;
		}
		memcpy(c->key, UE, 32);
		memset(iv, 0, 16);
		aes_cbc_dec(ik, 32, iv, c->key, 32);
		if (Pm && pn >= 16) {
			unsigned char pm[16];

			memcpy(pm, Pm, 16);
			memset(iv, 0, 16);
			aes_cbc_dec(c->key, 32, iv, pm, 16);	/* one block, a zero IV: ECB */
			if (pm[9] != 'a' || pm[10] != 'd' || pm[11] != 'b') {
				free(c);
				return 1;	/* the user password is not empty */
			}
		}
		c->klen = 32;
		(void)OE;
		d->crypt = c;
		return 0;
	}
	/* revisions 2 to 4 */
	n = 5;
	if (v >= 2)
		n = (int)pd_int(d, enc, "Length", 40) / 8;
	if (v == 4) {
		POBJ *cf = pd_get(d, enc, "CF"), *std = cf ? pd_get(d, cf, "StdCF") : NULL;

		n = 16;
		if (std) {
			long l = pd_int(d, std, "Length", 16);

			n = (int)(l > 32 ? l / 8 : l);
		}
	}
	if (n < 5 || n > 16)
		n = r == 2 ? 5 : 16;
	{
		br_md5_context x;
		unsigned char pb[4], h[16], ff[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
		POBJ *em = pd_get(d, enc, "EncryptMetadata");

		pb[0] = (unsigned char)P; pb[1] = (unsigned char)(P >> 8); pb[2] = (unsigned char)(P >> 16); pb[3] = (unsigned char)(P >> 24);
		br_md5_init(&x);
		br_md5_update(&x, PAD, 32);
		br_md5_update(&x, O, 32);
		br_md5_update(&x, pb, 4);
		if (ID)
			br_md5_update(&x, ID, (size_t)idn);
		if (r >= 4 && em && em->t == PO_BOOL && !em->a)
			br_md5_update(&x, ff, 4);
		br_md5_out(&x, h);
		if (r >= 3)
			for (i = 0; i < 50; i++)
				md5(h, h, n, NULL, 0, NULL, 0);
		memcpy(c->key, h, (size_t)n);
		c->klen = n;
	}
	/* does the empty password open it? */
	{
		unsigned char t[32];

		if (r == 2) {
			memcpy(t, PAD, 32);
			rc4(c->key, c->klen, t, 32);
			if (un < 32 || memcmp(t, U, 32) != 0) {
				free(c);
				return 1;
			}
		} else {
			unsigned char h[16];
			int k;

			md5(h, PAD, 32, ID, idn, NULL, 0);
			memcpy(t, h, 16);
			for (k = 0; k < 20; k++) {
				unsigned char kk[16];
				int j;

				for (j = 0; j < c->klen; j++)
					kk[j] = (unsigned char)(c->key[j] ^ k);
				rc4(kk, c->klen, t, 16);
			}
			if (un < 16 || memcmp(t, U, 16) != 0) {
				free(c);
				return 1;
			}
		}
	}
	d->crypt = c;
	return 0;
}

/* the object's own key (algorithm 1; revision 5 and 6: the file's) */
static int obj_key(PCRYPT *c, int method, long num, long gen, unsigned char *k)
{
	int kl;

	if (c->v >= 5) {
		memcpy(k, c->key, 32);
		kl = 32;
	} else {
		unsigned char ext[9];
		unsigned char h[16];
		int el = 5;

		ext[0] = (unsigned char)num; ext[1] = (unsigned char)(num >> 8); ext[2] = (unsigned char)(num >> 16);
		ext[3] = (unsigned char)gen; ext[4] = (unsigned char)(gen >> 8);
		if (method == M_AES128) {
			memcpy(ext + 5, "sAlT", 4);
			el = 9;
		}
		{
			br_md5_context x;

			br_md5_init(&x);
			br_md5_update(&x, c->key, (size_t)c->klen);
			br_md5_update(&x, ext, (size_t)el);
			br_md5_out(&x, h);
		}
		kl = c->klen + 5 > 16 ? 16 : c->klen + 5;
		memcpy(k, h, (size_t)kl);
	}
	return kl;
}

long pc_decrypt(PDOC *d, long num, long gen, int is_stream, unsigned char *buf, long n)
{
	PCRYPT *c = d->crypt;
	int method = is_stream ? c->stm : c->str;
	unsigned char k[32];
	int kl;

	if (method == M_NONE || n <= 0)
		return n;
	kl = obj_key(c, method, num, gen, k);
	if (method == M_RC4) {
		rc4(k, kl, buf, n);
		return n;
	}
	/* AES-CBC: 16 bytes of IV, the data, PKCS#5 padding */
	if (n < 32 || (n & 15))
		return n >= 16 ? (n - 16) & ~15L : 0;
	{
		unsigned char iv[16];
		long m = n - 16;

		memcpy(iv, buf, 16);
		memmove(buf, buf + 16, (size_t)m);
		if (!is_stream)
			aes_cbc_dec(k, kl, iv, buf, m);
		else {
			/* a stream in pieces, the stop hook between them: a page's
			   content of 13 KB was 1.5 s of AES-256 on an ST in one go */
			br_aes_big_cbcdec_keys ck;
			long j;

			br_aes_big_cbcdec_init(&ck, k, (size_t)kl);
			for (j = 0; j < m; j += PIECE_AES) {
				if (j && crypt_pause(d))
					return 0;	/* (the document closing) */
				br_aes_big_cbcdec_run(&ck, iv, buf + j, (size_t)(m - j < PIECE_AES ? m - j : PIECE_AES));
			}
		}
		if (buf[m - 1] >= 1 && buf[m - 1] <= 16)
			m -= buf[m - 1];
		return m;
	}
}

void pc_close(PDOC *d)
{
	free(d->crypt);
	d->crypt = NULL;
}

/*
 * A stream decrypted as it is read (pdffilt.c's pieces): RC4 keeps its
 * state from call to call; AES takes the first 16 bytes as its IV and
 * chains the blocks (the caller gives whole blocks and strips the
 * padding at the end). A page's content was decrypted whole, then
 * unpacked whole: seconds on an ST without a turn.
 */
struct pcstream {
	int method;
	unsigned char s[256];		/* RC4 */
	int si, sj;
	br_aes_big_cbcdec_keys ck;	/* AES */
	unsigned char iv[16];
	int ivn;			/* the IV's bytes taken */
};

PCSTREAM *pc_stream_open(PDOC *d, long num, long gen)
{
	PCRYPT *c = d->crypt;
	PCSTREAM *x;
	unsigned char k[32];
	int kl, i, j = 0;

	if (!c || c->stm == M_NONE || !(x = calloc(1, sizeof *x)))
		return NULL;
	x->method = c->stm;
	kl = obj_key(c, c->stm, num, gen, k);
	if (x->method == M_RC4) {
		for (i = 0; i < 256; i++)
			x->s[i] = (unsigned char)i;
		for (i = 0; i < 256; i++) {
			unsigned char a = x->s[i];

			j = (j + a + k[i % kl]) & 255;
			x->s[i] = x->s[j];
			x->s[j] = a;
		}
	} else
		br_aes_big_cbcdec_init(&x->ck, k, (size_t)kl);
	return x;
}

int pc_stream_is_aes(const PCSTREAM *x)
{
	return x->method != M_RC4;
}

long pc_stream_run(PCSTREAM *x, unsigned char *buf, long n)
{
	long k;

	if (x->method == M_RC4) {
		int i = x->si, j = x->sj;

		for (k = 0; k < n; k++) {
			unsigned char a;

			i = (i + 1) & 255;
			a = x->s[i];
			j = (j + a) & 255;
			x->s[i] = x->s[j];
			x->s[j] = a;
			buf[k] ^= x->s[(a + x->s[i]) & 255];
		}
		x->si = i;
		x->sj = j;
		return n;
	}
	k = 0;
	while (x->ivn < 16 && k < n)
		x->iv[x->ivn++] = buf[k++];
	if (k) {
		memmove(buf, buf + k, (size_t)(n - k));
		n -= k;
	}
	n &= ~15L;
	if (n > 0)
		br_aes_big_cbcdec_run(&x->ck, x->iv, buf, (size_t)n);
	return n;
}

void pc_stream_close(PCSTREAM *x)
{
	free(x);
}

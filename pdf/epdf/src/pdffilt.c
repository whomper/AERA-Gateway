/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * pdffilt.c - streams: reading the stored bytes, decrypting them and
 * the filters (Flate through common's inflate, LZW, ASCII85, ASCIIHex,
 * RunLength, the PNG and TIFF predictors). The picture filters (DCT,
 * JPX, CCITT, JBIG2) end the chain; whoever draws pictures takes it from
 * there.
 */
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "pdfint.h"
#include "inflate.h"

long pdf_stream_len(PDOC *d, POBJ *o)
{
	return pd_int(d, o, "Length", -1);
}

/* where a stream's stored bytes are: its /Length, checked against
   "endstream" (found by looking where it is wrong or missing); 0, or -1 */
static int stream_span(PDOC *d, POBJ *o, long *offp, long *lenp)
{
	long off = pd_int(d, o, "$soff", -1), len = pdf_stream_len(d, o), k;
	unsigned char tail[16];
	int ok = 0;

	if (off < 0)
		return -1;
	if (len >= 0 && off + len <= d->size) {
		long n = pdf_rd(d, off + len, tail, sizeof tail);

		for (k = 0; k < n && is_white_c(tail[k]); k++)
			;
		ok = n - k >= 9 && memcmp(tail + k, "endstream", 9) == 0;
	}
	if (!ok) {
		/* wrong or missing /Length: find "endstream" */
		unsigned char *blk = pdf_malloc(d, 8192 + 16);
		long pos = off, found = -1;

		if (!blk)
			return -1;
		while (pos < d->size && found < 0) {
			long got = pdf_rd(d, pos, blk, 8192 + 16), j;

			if (got <= 0)
				break;
			for (j = 0; j + 9 <= got && j < 8192; j++)
				if (blk[j] == 'e' && memcmp(blk + j, "endstream", 9) == 0) {
					found = pos + j;
					break;
				}
			pos += 8192;
		}
		pdf_free(d, blk, 8192 + 16);
		if (found < 0)
			found = d->size;
		len = found - off;
		/* the EOL before endstream is not data */
		if (len > 0) {
			unsigned char c[2];

			pdf_rd(d, off + len - 2 > off ? off + len - 2 : off, c, 2);
			if (len >= 2 && c[0] == '\r' && c[1] == '\n')
				len -= 2;
			else if (c[1] == '\n' || c[1] == '\r')
				len -= 1;
		}
	}
	if (len < 0 || len > (256L << 20))
		return -1;
	*offp = off;
	*lenp = len;
	return 0;
}

/* the stored bytes of a stream, read whole */
static unsigned char *stream_raw(PDOC *d, POBJ *o, long *lenp)
{
	long off, len;
	unsigned char *buf;

	if (stream_span(d, o, &off, &len))
		return NULL;
	buf = pdf_malloc(d, len + 1);
	if (!buf)
		return NULL;
	if (pdf_rd(d, off, buf, len) != len) {
		pdf_free(d, buf, len + 1);
		return NULL;
	}
	d->st.raw_in += len;
	*lenp = len;
	return buf;
}

int pdf_filter_is_image(const char *n)
{
	return !strcmp(n, "DCTDecode") || !strcmp(n, "DCT") || !strcmp(n, "JPXDecode") ||
	       !strcmp(n, "CCITTFaxDecode") || !strcmp(n, "CCF") || !strcmp(n, "JBIG2Decode");
}

static const char *image_name(const char *n)
{
	if (!strcmp(n, "DCTDecode") || !strcmp(n, "DCT"))
		return "DCT";
	if (!strcmp(n, "JPXDecode"))
		return "JPX";
	if (!strcmp(n, "JBIG2Decode"))
		return "JBIG2";
	return "CCITT";
}

/* ------------------------------------------------------------------ predictors */
static int predictor(PDOC *d, SBUF *b, long pred, long colors, long bpc, long columns)
{
	long bpp, row, rows, i, y;
	unsigned char *p, *prev;

	(void)d;
	if (pred <= 1)
		return 0;
	if (colors < 1)
		colors = 1;
	if (bpc < 1)
		bpc = 8;
	if (columns < 1)
		columns = 1;
	row = (colors * bpc * columns + 7) / 8;
	bpp = (colors * bpc + 7) / 8;
	if (pred == 2) {			/* TIFF */
		rows = b->len / row;
		for (y = 0; y < rows; y++) {
			p = (unsigned char *)b->s + y * row;
			if (bpc == 8) {
				for (i = colors; i < row; i++)
					p[i] = (unsigned char)(p[i] + p[i - colors]);
			} else if (bpc == 16) {
				for (i = colors * 2; i + 1 < row; i += 2) {
					unsigned v = (unsigned)((p[i] << 8) | p[i + 1]) + (unsigned)((p[i - colors * 2] << 8) | p[i - colors * 2 + 1]);

					p[i] = (unsigned char)(v >> 8);
					p[i + 1] = (unsigned char)v;
				}
			}
		}
		return 0;
	}
	/* PNG: each row has a tag byte */
	rows = (b->len + row) / (row + 1);
	prev = NULL;
	{
		unsigned char *out = (unsigned char *)b->s;
		unsigned char *in = (unsigned char *)b->s;
		long o = 0;

		for (y = 0; y < rows; y++) {
			long avail = b->len - y * (row + 1) - 1;
			int tag;
			unsigned char *cur = out + o;

			if (avail <= 0)
				break;
			tag = in[y * (row + 1)];
			if (avail > row)
				avail = row;
			memmove(cur, in + y * (row + 1) + 1, (size_t)avail);	/* in place: out never passes in */
			if (avail < row)
				memset(cur + avail, 0, (size_t)(row - avail));
			switch (tag) {
			case 1:
				for (i = bpp; i < row; i++)
					cur[i] = (unsigned char)(cur[i] + cur[i - bpp]);
				break;
			case 2:
				if (prev)
					for (i = 0; i < row; i++)
						cur[i] = (unsigned char)(cur[i] + prev[i]);
				break;
			case 3:
				for (i = 0; i < row; i++) {
					int l = i >= bpp ? cur[i - bpp] : 0, u = prev ? prev[i] : 0;

					cur[i] = (unsigned char)(cur[i] + ((l + u) >> 1));
				}
				break;
			case 4:
				for (i = 0; i < row; i++) {
					int a = i >= bpp ? cur[i - bpp] : 0, bb = prev ? prev[i] : 0, c = (i >= bpp && prev) ? prev[i - bpp] : 0;
					int pp = a + bb - c, pa = pp > a ? pp - a : a - pp, pb = pp > bb ? pp - bb : bb - pp,
					    pc = pp > c ? pp - c : c - pp;
					int pr = (pa <= pb && pa <= pc) ? a : (pb <= pc ? bb : c);

					cur[i] = (unsigned char)(cur[i] + pr);
				}
				break;
			default:
				break;
			}
			prev = cur;
			o += row;
		}
		b->len = o;
	}
	return 0;
}

/* ------------------------------------------------------------------ ASCII filters */
static int f_hex(SBUF *b)
{
	long i, n = 0;
	int hi = -1;
	unsigned char *s = (unsigned char *)b->s;

	for (i = 0; i < b->len; i++) {
		int c = s[i], v;

		if (c == '>')
			break;
		v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
		if (v < 0)
			continue;
		if (hi < 0)
			hi = v;
		else {
			s[n++] = (unsigned char)(hi << 4 | v);
			hi = -1;
		}
	}
	if (hi >= 0)
		s[n++] = (unsigned char)(hi << 4);
	b->len = n;
	return 0;
}

static int f_a85(SBUF *b)
{
	unsigned char *s = (unsigned char *)b->s;
	long i = 0, n = 0, k = 0;
	uint32_t v = 0;
	int cnt = 0;

	if (b->len >= 2 && s[0] == '<' && s[1] == '~')
		i = 2;
	for (; i < b->len; i++) {
		int c = s[i];

		if (is_white_c(c))
			continue;
		if (c == '~')
			break;
		if (c == 'z' && cnt == 0) {
			s[n++] = 0; s[n++] = 0; s[n++] = 0; s[n++] = 0;	/* n <= i always: 1 byte of input gives 4 of output only if... */
			continue;
		}
		if (c < '!' || c > 'u')
			continue;
		v = v * 85 + (uint32_t)(c - '!');
		if (++cnt == 5) {
			s[n++] = (unsigned char)(v >> 24);
			s[n++] = (unsigned char)(v >> 16);
			s[n++] = (unsigned char)(v >> 8);
			s[n++] = (unsigned char)v;
			v = 0;
			cnt = 0;
		}
	}
	(void)k;
	if (cnt > 1) {
		int j;

		for (j = cnt; j < 5; j++)
			v = v * 85 + 84;
		for (j = 0; j < cnt - 1; j++)
			s[n++] = (unsigned char)(v >> (24 - 8 * j));
	}
	b->len = n;
	return 0;
}

static int f_rl(PDOC *d, SBUF *in)
{
	SBUF out;
	const unsigned char *s = (const unsigned char *)in->s;
	long i = 0;

	(void)d;
	sb_init(&out);
	while (i < in->len) {
		int c = s[i++];

		if (c == 128)
			break;
		if (c < 128) {
			long n = c + 1;

			if (n > in->len - i)
				n = in->len - i;
			sb_add(&out, (const char *)s + i, n);
			i += n;
		} else if (i < in->len) {
			long n = 257 - c;

			sb_room(&out, n);
			memset(out.s + out.len, s[i++], (size_t)n);
			out.len += n;
		}
	}
	sb_free(in);
	*in = out;
	return 0;
}

static int f_lzw(PDOC *d, SBUF *in, int early)
{
	uint16_t *prefix;
	uint16_t *len;
	unsigned char *suffix;
	SBUF out;
	const unsigned char *s = (const unsigned char *)in->s;
	long pos = 0, nbits = in->len * 8;
	int bits = 9, next = 258, prev = -1, code;
	uint32_t acc = 0;
	int have = 0;

	prefix = pdf_malloc(d, 4096 * 2);
	len = pdf_malloc(d, 4096 * 2);
	suffix = pdf_malloc(d, 4096);
	if (!prefix || !len || !suffix) {
		pdf_free(d, prefix, 4096 * 2);
		pdf_free(d, len, 4096 * 2);
		pdf_free(d, suffix, 4096);
		return -1;
	}
	sb_init(&out);
	for (;;) {
		long n;
		int c, first;

		while (have < bits && pos < nbits) {
			acc = (acc << 8) | s[pos >> 3];
			pos += 8;
			have += 8;
		}
		if (have < bits)
			break;
		code = (int)((acc >> (have - bits)) & ((1u << bits) - 1));
		have -= bits;
		if (code == 256) {
			bits = 9;
			next = 258;
			prev = -1;
			continue;
		}
		if (code == 257)
			break;
		if (prev < 0) {
			if (code > 255)
				break;
			sb_addc(&out, (char)code);
			prev = code;
			continue;
		}
		if (code < next) {
			/* emit code's string */
			n = code < 256 ? 1 : len[code];
			sb_room(&out, n);
			c = code;
			{
				long k = n;

				while (k > 0) {
					out.s[out.len + --k] = c < 256 ? (char)c : (char)suffix[c];
					c = c < 256 ? 0 : prefix[c];
				}
			}
			first = (unsigned char)out.s[out.len];
			out.len += n;
		} else if (code == next && next < 4096) {
			/* prev's string + its first byte */
			long pl = prev < 256 ? 1 : len[prev];
			int fc;

			n = pl + 1;
			sb_room(&out, n);
			c = prev;
			{
				long k = pl;

				while (k > 0) {
					out.s[out.len + --k] = c < 256 ? (char)c : (char)suffix[c];
					c = c < 256 ? 0 : prefix[c];
				}
			}
			fc = (unsigned char)out.s[out.len];
			out.s[out.len + pl] = (char)fc;
			first = fc;
			out.len += n;
		} else
			break;
		if (next < 4096) {
			prefix[next] = (uint16_t)prev;
			suffix[next] = (unsigned char)first;
			len[next] = (uint16_t)((prev < 256 ? 1 : len[prev]) + 1);
			next++;
		}
		prev = code;
		if (next + early >= 4096)
			bits = 12;
		else if (next + early >= 2048)
			bits = 12;
		else if (next + early >= 1024)
			bits = 11;
		else if (next + early >= 512)
			bits = 10;
		else
			bits = 9;
	}
	pdf_free(d, prefix, 4096 * 2);
	pdf_free(d, len, 4096 * 2);
	pdf_free(d, suffix, 4096);
	sb_free(in);
	*in = out;
	return 0;
}

static int f_flate(SBUF *in, long max)
{
	SBUF out;
	int r;

	sb_init(&out);
	r = inflate_zlib((const unsigned char *)in->s, in->len, &out, max);
	sb_free(in);
	*in = out;
	return r < 0 ? -1 : 0;			/* damaged data: what came before the damage stays */
}

int filt_apply(PDOC *d, const char *name, POBJ *parms, SBUF *io, long max)
{
	int r = 0;

	if (!strcmp(name, "FlateDecode") || !strcmp(name, "Fl"))
		r = f_flate(io, max);
	else if (!strcmp(name, "ASCIIHexDecode") || !strcmp(name, "AHx"))
		r = f_hex(io);
	else if (!strcmp(name, "ASCII85Decode") || !strcmp(name, "A85"))
		r = f_a85(io);
	else if (!strcmp(name, "RunLengthDecode") || !strcmp(name, "RL"))
		r = f_rl(d, io);
	else if (!strcmp(name, "LZWDecode") || !strcmp(name, "LZW"))
		r = f_lzw(d, io, (int)pd_int(d, parms, "EarlyChange", 1));
	else if (!strcmp(name, "Crypt"))
		return 0;
	else
		return -2;			/* unknown */
	if (r == 0 && parms && parms->t == PO_DICT &&
	    (!strcmp(name, "FlateDecode") || !strcmp(name, "Fl") || !strcmp(name, "LZWDecode") || !strcmp(name, "LZW")))
		predictor(d, io, pd_int(d, parms, "Predictor", 1), pd_int(d, parms, "Colors", 1),
			  pd_int(d, parms, "BitsPerComponent", 8), pd_int(d, parms, "Columns", 1));
	return r;
}

/* a stream with the Identity crypt filter is stored as it is */
static int crypt_identity(PDOC *d, POBJ *filter, POBJ *parms)
{
	POBJ *f0 = filter && filter->t == PO_ARR ? pdf_res(d, pa_at(filter, 0)) : filter;
	POBJ *p0 = parms && parms->t == PO_ARR ? pdf_res(d, pa_at(parms, 0)) : parms;

	if (f0 && pdf_is_name(f0, "Crypt")) {
		const char *n = pd_name(d, p0, "Name");

		return !n || !strcmp(n, "Identity");
	}
	return 0;
}

static int pdf_stream_in(PDOC *d, POBJ *o, long onum, SBUF *out, const char **terminal, long max);

int pdf_stream(PDOC *d, POBJ *o, long onum, SBUF *out, const char **terminal, long max)
{
	long t0 = d->clock ? d->clock() : 0;
	int r = pdf_stream_in(d, o, onum, out, terminal, max);

	if (d->clock)
		d->st.t_stream += d->clock() - t0;
	return r;
}

static int pdf_stream_in(PDOC *d, POBJ *o, long onum, SBUF *out, const char **terminal, long max)
{
	long len = 0, nf, i;
	unsigned char *raw;
	POBJ *filter, *parms;
	SBUF b;
	int replaced = 0;

	if (terminal)
		*terminal = NULL;
	sb_init(out);
	if (!o || o->t != PO_DICT || !o->flag)
		return -1;
	raw = stream_raw(d, o, &len);
	if (!raw)
		return -1;
	d->st.streams++;
	filter = pd_get(d, o, "Filter");
	if (!filter)
		filter = pd_get(d, o, "F");
	parms = pd_get(d, o, "DecodeParms");
	if (!parms)
		parms = pd_get(d, o, "DP");
	if (d->crypt && onum >= 0 && !pdf_is_name(pd_getraw(o, "Type"), "XRef") && !crypt_identity(d, filter, parms)) {
		uint32_t ea = 0;
		uint16_t eb = 0;

		if (d->xe && onum < d->nxe && xe_get(d, onum, &ea, &eb) != 1)
			eb = 0;
		len = pc_decrypt(d, onum, eb, 1, raw, len);
		d->st.crypt_bytes += len;
	}
	b.s = (char *)raw;
	b.len = len;
	b.cap = len + 1;
	nf = !filter ? 0 : filter->t == PO_ARR ? filter->a : 1;
	for (i = 0; i < nf; i++) {
		char *before = b.s;
		POBJ *fn = filter->t == PO_ARR ? pdf_res(d, pa_at(filter, i)) : filter;
		POBJ *fp = parms ? (parms->t == PO_ARR ? pdf_res(d, pa_at(parms, i)) : parms) : NULL;
		int r;

		if (!fn || fn->t != PO_NAME)
			break;
		if (pdf_filter_is_image(fn->u.p)) {
			if (terminal)
				*terminal = image_name(fn->u.p);
			break;
		}
		r = filt_apply(d, fn->u.p, fp, &b, max);
		if (b.s != before)
			replaced = 1;
		if (r == -2) {
			if (terminal)
				*terminal = "unknown filter";
			break;
		}
		if (r < 0) {
			sb_free(&b);
			return -1;
		}
	}
	/* b's buffer came from pdf_malloc'd raw, or from util.c's: hand it over as an SBUF */
	if (!replaced) {
		/* untouched: copy into an SBUF the caller can sb_free (raw is ours) */
		*out = (SBUF){0};
		sb_init(out);
		sb_add(out, b.s, b.len);
		pdf_free(d, raw, len + 1);
	} else {
		*out = b;
		/* raw was freed by the filter that replaced it?  No: the first filter left raw alone */
	}
	d->st.out += out->len;
	return 0;
}

/* ------------------------------------------------------------------ a stream a piece at a time */
/*
 * A content stream needn't fit in memory (phase 0.3: a map's page is MBs
 * unpacked): a stream stored as it is, or packed by Flate alone (no
 * predictor), not encrypted, is read through the file's window and
 * unpacked by common's inflate_open/inflate_read (40 KB). Anything else
 * gives NULL, and the caller decodes it whole (pdf_stream) as before.
 */
#define PCSRC_INFLATE 73000L		/* inflate_open_piece's state, about: counted in st.mem while open */

struct PCSRC {
	PDOC *d;
	long off, left;			/* the stored bytes still to read, in the file */
	INFLATE *z;			/* NULL: stored as it is */
	const unsigned char *pp;	/* inflate_piece's piece, and what is left of it */
	long pn;
	int pend;			/* inflate_piece said the end (0) or damage (-1) */
	PCSTREAM *cs;			/* encrypted: decrypted as it is read, else NULL */
	long ask;			/* bytes the caller will take in all (0: all of it), less those given */
	unsigned char *q;		/* AES: what came out, the last block held back */
	int qh, qn, qfin;		/* until the end (its padding) */
};

long pdf_stream_left(const PCSRC *s)
{
	return s ? s->left : 0;
}

#define PCQ 512				/* AES: the raw bytes taken at a time, about 60 ms on an ST */

static int raw_get(PCSRC *s, unsigned char *buf, long n)
{
	long k;

	if (n > s->left)
		n = s->left;
	if (n <= 0)
		return 0;
	k = pdf_rd_seq(s->d, s->off, buf, n);
	if (k <= 0)
		return 0;
	s->off += k;
	s->left -= k;
	s->d->st.raw_in += k;
	return (int)k;
}

/* the reader's stop hook between pieces of decryption (as the content's
   interpreter has it) */
static int src_pause(PDOC *d)
{
	if (d->stop && !d->stopping && d->stop(d->stop_ctx))
		d->stopping = 1;
	return d->stopping;
}

/* the stream's bytes, decrypted when it is encrypted */
static int pcsrc_get(void *u, unsigned char *buf, long n)
{
	PCSRC *s = u;
	long out = 0;

	if (!s->cs)
		return raw_get(s, buf, n);
	if (!pc_stream_is_aes(s->cs)) {
		long k = raw_get(s, buf, n);

		if (k > 0)
			pc_stream_run(s->cs, buf, k);
		return (int)k;
	}
	while (out < n) {
		/* the last block is its padding's: held until the end */
		long rel = s->qfin ? s->qn : s->qn - 16, k;

		if (rel > 0) {
			k = n - out < rel ? n - out : rel;
			memcpy(buf + out, s->q + s->qh, (size_t)k);
			s->qh += (int)k;
			s->qn -= (int)k;
			out += k;
			continue;
		}
		if (s->left <= 0) {
			if (s->qfin)
				break;
			s->qfin = 1;
			if (s->qn > 0) {
				int pad = s->q[s->qh + s->qn - 1];

				if (pad >= 1 && pad <= 16 && pad <= s->qn)
					s->qn -= pad;
			}
			continue;
		}
		if (out && src_pause(s->d))
			break;
		memmove(s->q, s->q + s->qh, (size_t)s->qn);
		s->qh = 0;
		k = raw_get(s, s->q + s->qn, s->left < PCQ ? s->left : PCQ);
		if (k <= 0) {
			s->left = 0;
			continue;
		}
		s->qn += (int)pc_stream_run(s->cs, s->q + s->qn, k);
	}
	return (int)out;
}

/* a source given up before it is open (its decryption only) */
static void src_free(PCSRC *s)
{
	if (s->cs)
		pc_stream_close(s->cs);
	free(s->q);
	pdf_free(s->d, s, (long)sizeof *s);
}

/* a picture's own filter, which the caller decodes: a scan's or a JPEG's */
static int is_coded(const POBJ *f)
{
	return f && f->t == PO_NAME && (!strcmp(f->u.p, "CCITTFaxDecode") || !strcmp(f->u.p, "CCF") ||
					!strcmp(f->u.p, "DCTDecode") || !strcmp(f->u.p, "DCT"));
}

/* image: the stream's last filter is a scan's or a JPEG's, left to the
   caller: the bytes before it */
static PCSRC *src_open(PDOC *d, POBJ *o, int image, long onum)
{
	POBJ *filter, *parms;
	PCSRC *s;
	long off, len;
	int flate = 0, crypt = 0;

	if (!o || o->t != PO_DICT || !o->flag)
		return NULL;
	filter = pd_get(d, o, "Filter");
	if (!filter)
		filter = pd_get(d, o, "F");
	parms = pd_get(d, o, "DecodeParms");
	if (!parms)
		parms = pd_get(d, o, "DP");
	if (image) {
		/* /CCITTFaxDecode, [/CCITTFaxDecode] or [/FlateDecode /CCITTFaxDecode]; DCTDecode the same */
		if (is_coded(filter))
			filter = NULL;
		else if (filter && filter->t == PO_ARR && filter->a >= 1 && filter->a <= 2
			 && is_coded(pdf_res(d, pa_at(filter, filter->a - 1)))) {
			filter = filter->a == 2 ? pdf_res(d, pa_at(filter, 0)) : NULL;
			parms = parms && parms->t == PO_ARR && parms->a == 2 ? pdf_res(d, pa_at(parms, 0)) : NULL;
		} else
			return NULL;
	}
	if (filter && filter->t == PO_ARR && filter->a == 1)
		filter = pdf_res(d, pa_at(filter, 0));
	if (filter) {
		if (filter->t != PO_NAME || (strcmp(filter->u.p, "FlateDecode") && strcmp(filter->u.p, "Fl")))
			return NULL;
		if (parms && parms->t == PO_ARR && parms->a == 1)
			parms = pdf_res(d, pa_at(parms, 0));
		if (parms && parms->t == PO_DICT && pd_int(d, parms, "Predictor", 1) > 1)
			return NULL;
		flate = 1;
	}
	if (stream_span(d, o, &off, &len))
		return NULL;
	if (d->crypt) {
		/* as pdf_stream_in decrypts: not an xref stream, not Identity */
		if (onum < 0)
			return NULL;
		crypt = !pdf_is_name(pd_getraw(o, "Type"), "XRef") && !crypt_identity(d, pd_get(d, o, "Filter"), pd_get(d, o, "DecodeParms"));
	}
	s = pdf_malloc(d, (long)sizeof *s);
	if (!s)
		return NULL;
	memset(s, 0, sizeof *s);
	s->d = d;
	s->off = off;
	s->left = len;
	if (crypt) {
		uint32_t ea = 0;
		uint16_t eb = 0;

		if (d->xe && onum < d->nxe && xe_get(d, onum, &ea, &eb) != 1)
			eb = 0;
		s->cs = pc_stream_open(d, onum, eb);
		if (s->cs && pc_stream_is_aes(s->cs) && !(s->q = malloc(PCQ + 32))) {
			pc_stream_close(s->cs);
			s->cs = NULL;
		}
		if (!s->cs) {
			pdf_free(d, s, (long)sizeof *s);
			return NULL;	/* (the caller decodes it whole) */
		}
	}
	if (flate) {
		/* zlib's two bytes before the DEFLATE data (no preset dictionary) */
		unsigned char h[2];

		if (pcsrc_get(s, h, 2) != 2 || (h[0] & 0x0F) != 8 || ((h[0] << 8) | h[1]) % 31 || (h[1] & 0x20)) {
			src_free(s);
			return NULL;
		}
	}
	if (flate) {
		s->z = inflate_open_piece(pcsrc_get, s);
		s->pn = 0;
		s->pend = 1;
		if (!s->z) {
			d->st.oom++;
			src_free(s);
			return NULL;
		}
		d->st.mem += PCSRC_INFLATE;
		if (d->st.mem > d->st.mem_peak)
			d->st.mem_peak = d->st.mem;
	}
	d->st.streams++;
	return s;
}

PCSRC *pdf_stream_open(PDOC *d, POBJ *o, long onum)
{
	return src_open(d, o, 0, onum);
}

PCSRC *pdf_stream_open_coded(PDOC *d, POBJ *o, long onum)
{
	return src_open(d, o, 1, onum);
}

/*
 * A stream's bytes from inflate_piece's pieces: its hot loop is common's
 * assembly on the Atari (atari/inflate_fast.S, INFLATE_ASM), where
 * inflate_read's C took a map's page 89 s of 232 on an ST; 46 s now. Its
 * buffer is the window too: 72 KB a stream open, inflate_open's 40.
 *
 * A piece is 32 KB (or what the caller will take, less), with the
 * reader's stop hook after each 1 KB of it (inflate_piece's turn): each
 * piece moves the 32 KB window down first, and pieces the size of the
 * reader's 2 KB refills did that 16 times as often (an EDOS_01.PDF page 3 s
 * longer on an ST).
 */
static PDOC *turn_doc;			/* the document whose stream is being unpacked */

static void piece_turn(void)
{
	if (turn_doc)
		src_pause(turn_doc);
}

static long piece_read(PCSRC *s, unsigned char *out, long n)
{
	long k = 0;

	while (k < n) {
		long m;

		if (s->pn <= 0) {
			long ask = s->ask > 0 && s->ask < 32768L ? s->ask : 32768L;

			if (s->pend <= 0)
				return k ? k : s->pend;
			if (ask < n - k)
				ask = n - k < 32768L ? n - k : 32768L;
			turn_doc = s->d;	/* (one document at a time: left set, as a stream on
						   another stack may be unpacked in a turn) */
			s->pn = inflate_piece(s->z, &s->pp, ask, piece_turn);
			if (s->pn <= 0) {
				s->pend = (int)s->pn;
				s->pn = 0;
				return k ? k : s->pend;
			}
		}
		m = n - k < s->pn ? n - k : s->pn;
		memcpy(out + k, s->pp, (size_t)m);
		s->pp += m;
		s->pn -= m;
		k += m;
	}
	return k;
}

long pdf_stream_read(PCSRC *s, unsigned char *out, long n)
{
	long k = s->z ? piece_read(s, out, n) : pcsrc_get(s, out, n);

	if (k > 0) {
		s->d->st.out += k;
		if (s->ask > 0)
			s->ask = s->ask > k ? s->ask - k : 1;
	}
	return k;
}

int pdf_stream_parity(const PCSRC *s)
{
	return s && s->z && s->pn > 0 ? (int)((unsigned long)s->pp & 1) : -1;
}

void pdf_stream_close(PCSRC *s)
{
	if (!s)
		return;
	if (s->z) {
		inflate_close(s->z);
		s->d->st.mem -= PCSRC_INFLATE;
	}
	src_free(s);
}

/*
 * The first want bytes of a stream (all of it when it is shorter): read a
 * piece at a time with the reader's stop hook between pieces, where the
 * stream can be (pdf_stream_open), else decoded whole. *whole: 1 when out
 * holds all of the stream. 0, or -1 (damaged, out of memory, the document
 * closing). A font program's encoding is in its first few KB: decoding all
 * of a font was a stretch of 600 ms on an ST.
 */
#define HEAD_PIECE 2048L

int pdf_stream_head(PDOC *d, POBJ *o, long onum, long want, SBUF *out, int *whole)
{
	PCSRC *s = pdf_stream_open(d, o, onum);

	*whole = 1;
	if (s)
		s->ask = want + 1;	/* (the byte that says whether there is more) */
	if (!s) {
		const char *term = NULL;

		if (pdf_stream(d, o, onum, out, &term, 4L << 20) != 0 || term) {
			sb_free(out);
			return -1;
		}
		return 0;
	}
	sb_init(out);
	while (out->len < want) {
		long n = want - out->len < HEAD_PIECE ? want - out->len : HEAD_PIECE, k;
		/* the buffer doubles, up to want */
		long room = out->len > n ? out->len : n;

		if (room > want - out->len)
			room = want - out->len;
		if ((out->len + n + 1 > out->cap && !sb_reserve(out, room)) || src_pause(d)) {
			pdf_stream_close(s);
			sb_free(out);
			return -1;
		}
		k = pdf_stream_read(s, (unsigned char *)out->s + out->len, n);
		if (k < 0) {
			pdf_stream_close(s);
			sb_free(out);
			return -1;
		}
		if (k == 0)
			break;
		out->len += k;
	}
	if (out->len >= want) {
		unsigned char c;

		/* is there more? */
		*whole = pdf_stream_read(s, &c, 1) == 0;
	}
	pdf_stream_close(s);
	if (!out->s && !sb_reserve(out, 0))
		return -1;
	out->s[out->len] = 0;
	return 0;
}

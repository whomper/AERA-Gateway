/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * econvert - EConvert's converter on this computer (the tests, and the
 * gateway's later).
 *
 *   econvert [options] IN.pdf OUT.pdf      a converted PDF (docs/CONVERTED-PDF.md)
 *   econvert --render PAGE OUT.ppm IN.pdf  page PAGE (1-based) drawn 640 wide, and its drawings listed
 *
 * options: --pages none|mono|colour (whole pages: none, mono, mono and colour; mono),
 *          --photos indexed|jpeg (indexed), --pictures page|screen (screen), --page-width N, --picture-width N (640: a screen's), --canvas KB (any), --progress ("progress DONE TOTAL" lines on stderr), --fonts DIR (the standard fonts' files,
 *          Liberation*.ttf; /usr/share/fonts/truetype/liberation), --width N (640)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdf.h"
#include "render.h"
#include "conv.h"
#include "pic.h"
#include "charset.h"

static int ppm(const RCANVAS *c, const char *path)
{
	FILE *f = fopen(path, "wb");
	long i;

	if (!f)
		return -1;
	fprintf(f, "P6\n%d %d\n255\n", c->w, c->h);
	for (i = 0; i < (long)c->w * c->h; i++) {
		uint16_t p = c->px[i];

		fputc((p >> 11) << 3 | (p >> 13), f);
		fputc(((p >> 5) & 63) << 2 | ((p >> 9) & 3), f);
		fputc((p & 31) << 3 | ((p >> 2) & 7), f);
	}
	return fclose(f);
}

/* --progress: the conversion's thousandths on stderr, a line each time it
   moves by a hundredth ("progress DONE TOTAL"), for pdfgate's 102 answers */
static long last_done = -1;
static int show_progress(void *u, long done, long total)
{
	long pc = total > 0 ? done * 100 / total : 0;

	if (pc != last_done) {
		last_done = pc;
		fprintf(stderr, "progress %ld %ld\n", done, total);
		fflush(stderr);
	}
	return 0;
}

int main(int argc, char **argv)
{
	const char *fonts = "/usr/share/fonts/truetype/liberation", *in = NULL, *out = NULL, *img = NULL;
	long page = 0;
	int width = 640, i;
	CVOPT o;

	cv_defaults(&o);
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--render") && i + 2 < argc) {
			page = atol(argv[++i]);
			img = argv[++i];
		} else if (!strcmp(argv[i], "--fonts") && i + 1 < argc)
			fonts = argv[++i];
		else if (!strcmp(argv[i], "--width") && i + 1 < argc)
			width = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--pages") && i + 1 < argc) {
			i++;
			o.pages = !strcmp(argv[i], "none") ? CV_PAGES_NONE : !strcmp(argv[i], "colour") ? CV_PAGES_COLOUR : CV_PAGES_MONO;
		} else if (!strcmp(argv[i], "--progress")) {
			o.progress = show_progress;
		} else if (!strcmp(argv[i], "--normal-pages")) {
			o.page_double = 0;
		} else if (!strcmp(argv[i], "--page-width") && i + 1 < argc) {
			o.page_w = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--picture-width") && i + 1 < argc) {
			o.pic_w = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--canvas") && i + 1 < argc) {
			o.canvas_max = atol(argv[++i]) * 1024L;	/* KB: a page drawn in bands under it */
		} else if (!strcmp(argv[i], "--pictures") && i + 1 < argc) {
			i++;
			o.pic_size = !strcmp(argv[i], "screen") ? CV_PIC_SCREEN : CV_PIC_PAGE;
		} else if (!strcmp(argv[i], "--photos") && i + 1 < argc) {
			i++;
			o.photos = !strcmp(argv[i], "jpeg") ? CV_PHOTOS_JPEG : CV_PHOTOS_INDEXED;
		} else if (!in)
			in = argv[i];
		else if (!out)
			out = argv[i];
		else {
			fprintf(stderr, "econvert: %s?\n", argv[i]);
			return 2;
		}
	}
	o.fontdir = fonts;
	if (argc == 4 && !strcmp(argv[1], "--store")) {
		/* a page's text as EPDF's store keeps it: a converted file's as it
		   is, another's read and made so (the tests: the two the same) */
		PDOC d;
		PTEXT t;
		PPREP pp;
		long n = atol(argv[3]) - 1, len = 0, i;
		char *a = NULL;

		if (pdf_open(&d, argv[2]))
			return 1;
		ptext_init(&t);
		t.soft = 1;
		t.pics = 1;
		if (d.prepared) {
			if (pdf_prepared_page(&d, n, &t.text, &pp))
				return 1;
			fwrite(t.text.s, 1, (size_t)t.text.len, stdout);
		} else {
			pdf_page_text(&d, n, &t);
			for (i = 0; i < t.text.len; i++)
				if (t.text.s[i] == '\v')
					t.text.s[i] = '\r';
			if (t.text.len)
				a = cs_to_atari(t.text.s, t.text.len, CS_UTF8, &len);
			if (a)
				fwrite(a, 1, (size_t)len, stdout);
		}
		return 0;
	}
	if (getenv("ECONV_JPEG")) {	/* (a test: a JPEG file through pic_decode) */
		long size = 0;
		void *h = pdf_os_open(getenv("ECONV_JPEG"), &size);
		unsigned char *b = malloc((size_t)size);
		int why = 0;
		PIC *p;

		pdf_os_read(h, 0, b, size);
		p = pic_decode(b, size, 200, 200, 64L << 20, &why);
		printf("why %d px %04x %04x\n", why, p ? p->px[100 * 200 + 100] : 0, p ? p->px[150 * 200 + 50] : 0);
		return 0;
	}
	if (img) {
		PDOC d;
		RPAGE r;
		RDRAWING dw[32];
		int n, k;
		long t0;

		if (!in || pdf_open(&d, in)) {
			fprintf(stderr, "econvert: %s: %s\n", in ? in : "no file", in ? d.err : "");
			return 1;
		}
		if (render_init(&r, &d, fonts)) {
			fprintf(stderr, "econvert: no FreeType\n");
			return 1;
		}
		t0 = pdf_ms();
		if (render_page(&r, page - 1, width)) {
			fprintf(stderr, "econvert: page %ld not drawn\n", page);
			return 1;
		}
		n = render_drawings(&r, dw, 32);
		printf("{\"page\":%ld,\"w\":%d,\"h\":%d,\"ms\":%ld,\"glyphs\":%ld,\"glyphs_missed\":%ld,\"images\":%ld,\"images_missed\":%ld,\"areas\":%ld,\"drawings\":[",
		       page, r.c.w, r.c.h, pdf_ms() - t0, r.glyphs, r.glyphs_missed, r.images, r.images_missed, r.narea);
		for (k = 0; k < n; k++)
			printf("%s[%d,%d,%d,%d]", k ? "," : "", dw[k].px.x0, dw[k].px.y0, dw[k].px.x1, dw[k].px.y1);
		printf("]}\n");
		ppm(&r.c, img);
		render_done(&r);
		pdf_close(&d);
		return 0;
	}
	if (!in || !out) {
		fprintf(stderr, "usage: econvert [options] IN.pdf OUT.pdf | econvert --render PAGE OUT.ppm IN.pdf\n");
		return 2;
	}
	{
		char err[128];
		CVSTAT st;

		if (cv_convert(in, out, &o, &st, err, sizeof err)) {
			fprintf(stderr, "econvert: %s\n", err);
			return 1;
		}
		printf("{\"in\":\"%s\",\"pages\":%ld,\"in_bytes\":%ld,\"out_bytes\":%ld,\"ms\":%ld,\"pictures\":%ld,\"drawings\":%ld}\n",
		       in, st.pages, st.in_bytes, st.out_bytes, st.ms, st.pictures, st.drawings);
	}
	return 0;
}

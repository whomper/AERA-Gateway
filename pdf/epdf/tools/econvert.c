/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * econvert - EConvert's converter on this computer (the tests, and the
 * gateway's later).
 *
 *   econvert [options] IN.pdf OUT.pdf      a converted PDF (docs/CONVERTED-PDF.md)
 *   econvert --render PAGE OUT.ppm IN.pdf  page PAGE (1-based) drawn 640 wide, and its drawings listed
 *            (--width W, --jpeg-pic: its JPEGs through pic.c, as render.ovl draws them)
 *
 * options: --pages none|mono|colour|colouronly (whole pages: none, mono, mono and colour,
 *          colour only; mono),
 *          --photos indexed|jpeg (indexed), --pictures page|screen (screen), --page-width N, --picture-width N (640: a screen's), --canvas KB (any), --progress ("progress DONE TOTAL" lines on stderr), --fonts DIR (the standard fonts' files,
 *          Liberation*.ttf; /usr/share/fonts/truetype/liberation), --width N (640)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "pdf.h"
#include "render.h"
#include "conv.h"
#include "pic.h"
#include "cvjpeg.h"
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

/* --translate CODE --translator PATH (docs/PI-GATEWAY.md, "aerapdf and the
   translator"): PATH --to CODE with the page's paragraphs on its standard
   input, the translation on its output, "from CODE" or "same CODE" first
   on its error output; its exit status as CVOPT's translate gives it */
static const char *translator;

static char *slurp(const char *path, long *len)
{
	FILE *f = fopen(path, "rb");
	char *b = NULL;
	long n = 0, cap = 0;
	size_t k;

	if (!f)
		return NULL;
	for (;;) {
		if (n + 4096 > cap) {
			char *nb = realloc(b, (size_t)(cap = cap ? cap * 2 : 8192));

			if (!nb) {
				free(b);
				fclose(f);
				return NULL;
			}
			b = nb;
		}
		k = fread(b + n, 1, 4096, f);
		if (!k)
			break;
		n += (long)k;
	}
	fclose(f);
	b[n] = 0;
	*len = n;
	return b;
}

static int run_translator(void *u, const char *to, const char *utf8, long len, char **out, long *outlen, char from[8])
{
	char dir[] = "/tmp/aerapdf-XXXXXX", pin[64], pout[64], perr[64];
	FILE *f;
	pid_t pid;
	int status = 0, rc;
	char *e;
	long el;

	(void)u;
	*out = NULL;
	*outlen = 0;
	if (!mkdtemp(dir))
		return 1;
	snprintf(pin, sizeof pin, "%s/in", dir);
	snprintf(pout, sizeof pout, "%s/out", dir);
	snprintf(perr, sizeof perr, "%s/err", dir);
	if (!(f = fopen(pin, "wb")) || fwrite(utf8, 1, (size_t)len, f) != (size_t)len || fclose(f)) {
		rc = 1;
		goto done;
	}
	pid = fork();
	if (pid == 0) {
		int fi = open(pin, O_RDONLY), fo = open(pout, O_WRONLY | O_CREAT | O_TRUNC, 0600),
		    fe = open(perr, O_WRONLY | O_CREAT | O_TRUNC, 0600);

		if (fi < 0 || fo < 0 || fe < 0)
			_exit(1);
		dup2(fi, 0);
		dup2(fo, 1);
		dup2(fe, 2);
		execl(translator, translator, "--to", to, (char *)NULL);
		_exit(1);
	}
	if (pid < 0 || waitpid(pid, &status, 0) != pid || !WIFEXITED(status)) {
		rc = 1;
		goto done;
	}
	rc = WEXITSTATUS(status);
	if (rc == 0) {
		*out = slurp(pout, outlen);
		e = slurp(perr, &el);
		from[0] = 0;
		if (e && !strncmp(e, "from ", 5))
			sscanf(e + 5, "%7[a-z]", from);
		else if (e && !strncmp(e, "same", 4))
			strcpy(from, "same");
		free(e);
		if (!*out || !from[0]) {
			free(*out);
			*out = NULL;
			rc = 1;
		}
	}
done:
	unlink(pin);
	unlink(pout);
	unlink(perr);
	rmdir(dir);
	return rc;
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
			o.pages = !strcmp(argv[i], "none") ? CV_PAGES_NONE : !strcmp(argv[i], "colour") ? CV_PAGES_COLOUR :
				  !strcmp(argv[i], "colouronly") ? CV_PAGES_COLOUR_ONLY : CV_PAGES_MONO;
		} else if (!strcmp(argv[i], "--font-kb") && i + 1 < argc) {
			o.font_budget = atol(argv[++i]) * 1024L;	/* (as EConvert's quarter of what is free) */
		} else if (!strcmp(argv[i], "--jpeg-pic")) {
			cv_jpeg_pic = 1;	/* (the JPEGs as render.ovl takes them: through pic.c) */
		} else if (!strcmp(argv[i], "--translate") && i + 1 < argc) {
			o.translate_to = argv[++i];
		} else if (!strcmp(argv[i], "--translator") && i + 1 < argc) {
			translator = argv[++i];
			o.translate = run_translator;
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

		if (o.translate_to && (!o.translate || strlen(o.translate_to) < 2 || strlen(o.translate_to) > 7 ||
				       strspn(o.translate_to, "abcdefghijklmnopqrstuvwxyz") != strlen(o.translate_to))) {
			fprintf(stderr, "econvert: --translate needs a language (en, he, ...) and --translator\n");
			return 2;
		}
		if (cv_convert(in, out, &o, &st, err, sizeof err)) {
			fprintf(stderr, "econvert: %s\n", err);
			/* the translator's 2 (no such pair), 3 (busy), 4 (too long) passed on */
			return st.tr_fail >= 2 && st.tr_fail <= 4 ? st.tr_fail : 1;
		}
		if (o.translate_to) {
			if (st.tr_same)
				fprintf(stderr, "same %s\n", o.translate_to);
			else if (st.tr_from[0])
				fprintf(stderr, "from %s\n", st.tr_from);
		}
		printf("{\"in\":\"%s\",\"pages\":%ld,\"in_bytes\":%ld,\"out_bytes\":%ld,\"ms\":%ld,\"pictures\":%ld,\"drawings\":%ld}\n",
		       in, st.pages, st.in_bytes, st.out_bytes, st.ms, st.pictures, st.drawings);
	}
	return 0;
}

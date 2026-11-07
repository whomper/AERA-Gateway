/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* pdfio_host.c - the file layer on stdio, for the tests and the host measurement */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "pdf.h"

void *pdf_os_open(const char *path, long *size)
{
	FILE *f = fopen(path, "rb");

	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	return f;
}

long pdf_os_read(void *h, long off, void *buf, long n)
{
	FILE *f = h;

	if (fseek(f, off, SEEK_SET) != 0)
		return -1;
	return (long)fread(buf, 1, (size_t)n, f);
}

long pdf_os_window(void)
{
	return PWIN;
}

void pdf_os_close(void *h)
{
	fclose(h);
}

long pdf_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}

/* ---------------------------------------------------------------- plat.h on POSIX, for the tests */
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "plat.h"

int pf_open(const char *path, int mode)
{
	int fl = mode == PF_WRITE ? O_WRONLY | O_CREAT | O_TRUNC : mode == PF_UPDATE ? O_RDWR : O_RDONLY;

	return open(path, fl, 0644);
}

long pf_read(int h, void *buf, long n)
{
	return (long)read(h, buf, (size_t)n);
}

long pf_write(int h, const void *buf, long n)
{
	return (long)write(h, buf, (size_t)n);
}

long pf_seek(int h, long off)
{
	return (long)lseek(h, off, SEEK_SET);
}

void pf_close(int h)
{
	close(h);
}

long pf_size(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 ? (long)st.st_size : -1;
}

int pf_remove(const char *path)
{
	return unlink(path) == 0 ? 0 : -1;
}

int pf_rename(const char *from, const char *to)
{
	return rename(from, to) == 0 ? 0 : -1;
}

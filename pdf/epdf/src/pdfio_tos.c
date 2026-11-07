/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* pdfio_tos.c - the file layer on GEMDOS: Fopen, Fseek, Fread */
#include "tos.h"
#include "pdf.h"

void *pdf_os_open(const char *path, long *size)
{
	long h = Fopen(path, 0);

	if (h < 0)
		return NULL;
	*size = Fseek(0L, (short)h, 2);
	if (*size < 0) {
		Fclose((short)h);
		return NULL;
	}
	return (void *)(h + 1);			/* handle 0 is a handle, not NULL */
}

long pdf_os_read(void *h, long off, void *buf, long n)
{
	short fh = (short)((long)h - 1);

	if (Fseek(off, fh, 0) != off)
		return -1;
	return Fread(fh, n, buf);
}

void pdf_os_close(void *h)
{
	Fclose((short)((long)h - 1));
}

long pdf_os_window(void)
{
	static long w;

	if (!w)
		w = get_cookie(0x5F435055L) >= 20 ? 2 * PWIN : PWIN;	/* _CPU */
	return w;
}

long pdf_ms(void)
{
	return (long)tos_ms();
}

/* Microseconds: the 200 Hz tick count and MFP timer C's down-counter (192 steps of 26 us in a tick)
   give 26 us where the tick alone gives 5 ms. Hatari and the ST, STE, TT and Falcon count alike. */
static long us_hz, us_c;

static void sup_us(void)
{
	volatile u32 *hz = (volatile u32 *)0x4baL;
	volatile u8 *tcdr = (volatile u8 *)0xfffffa23L;
	u32 h;

	__asm__ volatile("" : "+r"(hz), "+r"(tcdr));		/* an address GCC may not reason about */

	do {
		h = *hz;
		us_c = *tcdr;
	} while (h != *hz);
	us_hz = (long)h;
}

long pdf_us(void)
{
	Supexec(sup_us);
	return us_hz * 5000L + (192L - us_c) * 26L;
}

/* ---------------------------------------------------------------- plat.h on GEMDOS */
#include "plat.h"

int pf_open(const char *path, int mode)
{
	long h;

	if (mode == PF_WRITE)
		h = Fcreate(path, 0);
	else
		h = Fopen(path, mode == PF_UPDATE ? 2 : 0);
	return h < 0 ? -1 : (int)h;
}

long pf_read(int h, void *buf, long n)
{
	return Fread((short)h, n, buf);
}

long pf_write(int h, const void *buf, long n)
{
	return Fwrite((short)h, n, (void *)buf);
}

long pf_seek(int h, long off)
{
	return Fseek(off, (short)h, 0);
}

void pf_close(int h)
{
	Fclose((short)h);
}

long pf_size(const char *path)
{
	DTA dta, *old = Fgetdta();
	long r;

	Fsetdta(&dta);
	r = Fsfirst(path, 0) == 0 ? (long)dta.d_length : -1;
	Fsetdta(old);
	return r;
}

int pf_remove(const char *path)
{
	return Fdelete(path) == 0 ? 0 : -1;
}

int pf_rename(const char *from, const char *to)
{
	return Frename(from, to) == 0 ? 0 : -1;
}

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * coro.h - a second stack for work that gives the machine back in the
 * middle (doc.c's reading of a page): coro_switch saves the registers a C
 * function keeps on the stack it leaves and takes up the other where it
 * left it. coro_start makes a stack whose first switch to it calls fn().
 * Nothing in the work may call the AES: it runs between event calls.
 */
#ifndef CORO_H
#define CORO_H

typedef struct {
	void *sp;			/* the stack pointer, saved */
} CORO;

void coro_switch(CORO *from, CORO *to);

/* the stack (size bytes at mem) set up so that the first switch to c
   calls fn, which must never return: it switches away for good */
static inline void coro_start(CORO *c, void *mem, long size, void (*fn)(void))
{
	unsigned long *sp = (unsigned long *)(((unsigned long)mem + (unsigned long)size) & ~3UL);
	unsigned char *m = (unsigned char *)mem;
	long k;
	int i;

	for (k = 0; k < size; k++)
		m[k] = 0xA5;			/* to see how much of it was used (coro_used) */
	*--sp = 0;				/* fn's return address: never used */
	*--sp = (unsigned long)fn;		/* where coro_switch's rts goes */
	for (i = 0; i < 11; i++)
		*--sp = 0;			/* d2-d7, a2-a6 */
	c->sp = sp;
}

/* the bytes of a stack that were used: those past the 0xA5 still left
   at its bottom */
static inline long coro_used(const void *mem, long size)
{
	const unsigned char *m = (const unsigned char *)mem;
	long k = 0;

	while (k < size && m[k] == 0xA5)
		k++;
	return size - k;
}

#endif

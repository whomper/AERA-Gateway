/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * search.h - finding text in a page's text (the Atari character set),
 * whatever the case: A-Z and the accented capitals the Atari has match
 * their small letters. Spaces, line ends and the paragraphs' soft breaks
 * (CR) all match one another, and a run of them counts as one, so a
 * phrase is found across the lines it was set on.
 */
#ifndef SEARCH_H
#define SEARCH_H

/* the first match of needle in text[from..len): its start (*at) and
   length (*n) in text; 0 when there is none */
int search_text(const char *text, long len, const char *needle, long from, long *at, long *n);
/* the last match that starts before `before` (searching back) */
int search_text_back(const char *text, long len, const char *needle, long before, long *at, long *n);

#endif

/* SPDX-License-Identifier: FTL OR GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/*
 * ftsystem.c - FreeType's system layer for EPDF: memory from malloc,
 * fonts only from memory (the PDF's embedded font programs): no files.
 */
#include <ft2build.h>
#include FT_CONFIG_CONFIG_H
#include <freetype/internal/ftdebug.h>
#include <freetype/ftsystem.h>
#include <freetype/fterrors.h>
#include <freetype/fttypes.h>
#include <stdlib.h>

static void *ft_alloc(FT_Memory memory, long size)
{
	(void)memory;
	return malloc((size_t)size);
}

static void *ft_realloc(FT_Memory memory, long cur_size, long new_size, void *block)
{
	(void)memory;
	(void)cur_size;
	return realloc(block, (size_t)new_size);
}

static void ft_free(FT_Memory memory, void *block)
{
	(void)memory;
	free(block);
}

FT_BASE_DEF(FT_Memory) FT_New_Memory(void)
{
	FT_Memory memory = (FT_Memory)malloc(sizeof *memory);

	if (memory) {
		memory->user = 0;
		memory->alloc = ft_alloc;
		memory->realloc = ft_realloc;
		memory->free = ft_free;
	}
	return memory;
}

FT_BASE_DEF(void) FT_Done_Memory(FT_Memory memory)
{
	free(memory);
}

FT_BASE_DEF(FT_Error) FT_Stream_Open(FT_Stream stream, const char *filepathname)
{
	(void)stream;
	(void)filepathname;
	return FT_THROW(Cannot_Open_Resource);
}

/* FreeType reads FREETYPE_PROPERTIES from the environment: there is none here */
char *epdf_getenv(const char *name)
{
	(void)name;
	return 0;
}

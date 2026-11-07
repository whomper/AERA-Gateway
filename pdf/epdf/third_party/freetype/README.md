# FreeType in EPDF

[FreeType](https://freetype.org) 2.13.3, tag `VER-2-13-3`, commit `42608f77f20749dd6ddc9e0536788eaad70ea4b5`
(github.com/freetype/freetype), used under the **FreeType Licence** (`docs/FTL.TXT`) or, at our choice,
the **GNU GPL version 2** (`docs/GPLv2.TXT`): EPDF is GPL-2.0-or-later, so it takes the GPLv2 option.
FreeType works in fixed point (26.6 and 16.16), so it runs on a 68000 without an FPU.

## What is here

Unchanged from the tag (no patches yet, so no `patches/`):

- `include/`, `LICENSE.TXT`, `docs/FTL.TXT`, `docs/GPLv2.TXT`
- `src/base`, `src/sfnt`, `src/truetype`, `src/cff`, `src/type1`, `src/psaux`, `src/pshinter`, `src/psnames`,
  `src/raster`, `src/smooth`, `src/autofit`: the modules a PDF's embedded fonts need (TrueType, CFF / OpenType,
  Type 1), the smooth and the monochrome rasterisers, the PostScript hinter and the auto-hinter.
  Left out: bzip2, gzip, lzw, pcf, bdf, pfr, winfonts, type42, cid, sdf, svg, cache, gxvalid, otvalid, dlg, tools.

EPDF's own files, in `epdf/` (the Makefile points FreeType at them with `-DFT_CONFIG_OPTIONS_H` and
`-DFT_CONFIG_MODULES_H`):

- `ftoption.h`: the stock `ftoption.h` with LZW, zlib, Mac fonts, incremental loading, SVG, colour layers,
  variable fonts, BDF properties and subpixel hinting switched off (made by sed; the header says so);
  `-DEPDF_NO_BYTECODE` also leaves out the TrueType bytecode interpreter and `-DEPDF_NO_AUTOFIT` the auto-hinter;
- `ftmodule.h`: the modules built;
- `ftsystem.c`: FreeType's system layer on `malloc`, fonts from memory only (a PDF's fonts are streams);
- `setjmp.S`, `include/setjmp.h`: `setjmp` and `longjmp` for the 68000 (FreeType's error handling needs them);
- `ftprep.h`: `getenv` for the freestanding C library.

## Fonts

The standard 14 PDF fonts are drawn with metric-compatible free fonts. The measurement run used Liberation
2.x (SIL Open Font Licence 1.1), read from the build machine and not part of the repository: its licence
is to be checked before a font is shipped with EPDF (the OFL allows bundling with software; the
reserved font name and the licence text must go with it).

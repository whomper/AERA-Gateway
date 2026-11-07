/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 AERA | Erez Yaary */
/* Written by tools/gen-tables.py: do not edit. */
#ifndef PDFTAB_H
#define PDFTAB_H

static const unsigned short enc_std[256] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0021, 0x0022, 0x0023,
	0x0024, 0x0025, 0x0026, 0x2019, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B,
	0x003C, 0x003D, 0x003E, 0x003F, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
	0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, 0x0050, 0x0051, 0x0052, 0x0053,
	0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
	0x2018, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B,
	0x006C, 0x006D, 0x006E, 0x006F, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
	0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00A1, 0x00A2, 0x00A3, 0x2044, 0x00A5, 0x0192, 0x00A7,
	0x00A4, 0x0027, 0x201C, 0x00AB, 0x2039, 0x203A, 0xFB01, 0xFB02, 0x0000, 0x2013, 0x2020, 0x2021,
	0x00B7, 0x0000, 0x00B6, 0x2022, 0x201A, 0x201E, 0x201D, 0x00BB, 0x2026, 0x2030, 0x0000, 0x00BF,
	0x0000, 0x0060, 0x00B4, 0x02C6, 0x02DC, 0x00AF, 0x02D8, 0x02D9, 0x00A8, 0x0000, 0x02DA, 0x00B8,
	0x0000, 0x02DD, 0x02DB, 0x02C7, 0x2014, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00C6, 0x0000, 0x00AA,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0141, 0x00D8, 0x0152, 0x00BA, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x00E6, 0x0000, 0x0000, 0x0000, 0x0131, 0x0000, 0x0000, 0x0142, 0x00F8, 0x0153, 0x00DF,
	0x0000, 0x0000, 0x0000, 0x0000,
};
static const unsigned short enc_win[256] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0021, 0x0022, 0x0023,
	0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B,
	0x003C, 0x003D, 0x003E, 0x003F, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
	0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, 0x0050, 0x0051, 0x0052, 0x0053,
	0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B,
	0x006C, 0x006D, 0x006E, 0x006F, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
	0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x0000, 0x20AC, 0x0000, 0x201A, 0x0192,
	0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x0000, 0x017D, 0x0000,
	0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A,
	0x0153, 0x0000, 0x017E, 0x0178, 0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,
	0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF, 0x00B0, 0x00B1, 0x00B2, 0x00B3,
	0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
	0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7, 0x00C8, 0x00C9, 0x00CA, 0x00CB,
	0x00CC, 0x00CD, 0x00CE, 0x00CF, 0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
	0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF, 0x00E0, 0x00E1, 0x00E2, 0x00E3,
	0x00E4, 0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
	0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x00F8, 0x00F9, 0x00FA, 0x00FB,
	0x00FC, 0x00FD, 0x00FE, 0x00FF,
};
static const unsigned short enc_mac[256] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0021, 0x0022, 0x0023,
	0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B,
	0x003C, 0x003D, 0x003E, 0x003F, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
	0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, 0x0050, 0x0051, 0x0052, 0x0053,
	0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B,
	0x006C, 0x006D, 0x006E, 0x006F, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
	0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x0000, 0x00C4, 0x00C5, 0x00C7, 0x00C9,
	0x00D1, 0x00D6, 0x00DC, 0x00E1, 0x00E0, 0x00E2, 0x00E4, 0x00E3, 0x00E5, 0x00E7, 0x00E9, 0x00E8,
	0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3, 0x00F2, 0x00F4, 0x00F6, 0x00F5,
	0x00FA, 0x00F9, 0x00FB, 0x00FC, 0x2020, 0x00B0, 0x00A2, 0x00A3, 0x00A7, 0x2022, 0x00B6, 0x00DF,
	0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8, 0x221E, 0x00B1, 0x2264, 0x2265,
	0x00A5, 0x00B5, 0x2202, 0x2211, 0x220F, 0x03C0, 0x222B, 0x00AA, 0x00BA, 0x03A9, 0x00E6, 0x00F8,
	0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB, 0x00BB, 0x2026, 0x00A0, 0x00C0,
	0x00C3, 0x00D5, 0x0152, 0x0153, 0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x25CA,
	0x00FF, 0x0178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02, 0x2021, 0x00B7, 0x201A, 0x201E,
	0x2030, 0x00C2, 0x00CA, 0x00C1, 0x00CB, 0x00C8, 0x00CD, 0x00CE, 0x00CF, 0x00CC, 0x00D3, 0x00D4,
	0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC, 0x00AF, 0x02D8, 0x02D9, 0x02DA,
	0x00B8, 0x02DD, 0x02DB, 0x02C7,
};

#define AGL_N 4200
#define AGL_BLK 16
#define AGL_NBLK 263
static const char agl_names[35428] =
	"\000A\001E\002acute\002macron\002small\001acute\006small\001breve\006acute\006cyrillic\006dotbelow\006grave\006hookabove\006tilde\001caron\002ircle"
	"\000Acircumflex\013acute\013dotbelow\013grave\013hookabove\013small\013tilde\002ute\005small\002yrillic\001dblgrave\002ieresis\011cyrillic\011macron\011small\002otbelow"
	"\000Adotmacron\001grave\006small\001hookabove\001iecyrillic\002nvertedbreve\001lpha\005tonos\001macron\002onospace\001ogonek\001ring\005acute\005below\005small\001small"
	"\000Atilde\006small\001ybarmenian\000B\001circle\001dotaccent\004below\001ecyrillic\002narmenian\002ta\001hook\001linebelow\001monospace\001revesmall\001small\001topbar"
	"\000C\001aarmenian\002cute\002ron\005small\001caron\002edilla\010acute\010small\002ircle\005umflex\001dot\004accent\001edillasmall\001haarmenian\002eabkhasiancyrillic"
	"\000Checyrillic\003descenderabkhasiancyrillic\014cyrillic\004ieresiscyrillic\003harmenian\003khakassiancyrillic\003verticalstrokecyrillic\002i\002ook\001ircumflexsmall\001monospace\001oarmenian\001small\000D\001Z\002caron"
	"\000Daarmenian\002frican\001caron\002edilla\002ircle\005umflexbelow\002roat\001dotaccent\004below\001ecyrillic\002icoptic\002lta\005greek\001hook\001ieresis\010Acute"
	"\000DieresisGrave\010small\002gammagreek\001jecyrillic\001linebelow\001monospace\001otaccentsmall\001slash\002mall\001topbar\001z\002caron\002eabkhasiancyrillic\003cyrillic\002hecyrillic\000E"
	"\000Eacute\006small\001breve\001caron\002edillabreve\002harmenian\002ircle\005umflex\013acute\013below\013dotbelow\013grave\013hookabove\013small\013tilde\002yrillic"
	"\000Edblgrave\002ieresis\011small\002ot\004accent\004below\001fcyrillic\001grave\006small\001harmenian\002ookabove\001ightroman\002nvertedbreve\002otifiedcyrillic\001lcyrillic\002evenroman"
	"\000Emacron\007acute\007grave\002cyrillic\002onospace\001ncyrillic\002descendercyrillic\002g\003hecyrillic\002hookcyrillic\001ogonek\002pen\001psilon\007tonos\001rcyrillic\002eversed"
	"\000Ereversedcyrillic\001scyrillic\002descendercyrillic\002h\002mall\001ta\003rmenian\003tonos\002h\003small\002ilde\006below\001uro\001zh\003caron\003reversed"
	"\000F\001circle\001dotaccent\001eharmenian\002icoptic\001hook\001itacyrillic\002veroman\001monospace\001ourroman\001small\000G\001Bsquare\001acute\002mma\005african"
	"\000Gangiacoptic\001breve\001caron\002edilla\002ircle\005umflex\002ommaaccent\001dot\004accent\001ecyrillic\001hadarmenian\002emiddlehookcyrillic\003strokecyrillic\003upturncyrillic\002ook\001imarmenian"
	"\000Gjecyrillic\001macron\002onospace\001rave\005small\001small\006hook\002troke\000H\00118533\00443\00451\00122073\001Psquare\001aabkhasiancyrillic\002descendercyrillic"
	"\000Hardsigncyrillic\001bar\002revebelow\001cedilla\002ircle\005umflex\001dieresis\002otaccent\004below\001monospace\001oarmenian\002ricoptic\001small\001ungarumlaut\014small\001zsquare"
	"\000I\001Acyrillic\001J\001Ucyrillic\001acute\006small\001breve\001caron\002ircle\005umflex\013small\002yrillic\001dblgrave\002ieresis\011acute\011cyrillic"
	"\000Idieresissmall\002ot\004accent\004below\001ebrevecyrillic\002cyrillic\001fraktur\001grave\006small\001hookabove\001icyrillic\002nvertedbreve\002shortcyrillic\001macron\007cyrillic\002onospace"
	"\000Iniarmenian\001ocyrillic\002gonek\002ta\004african\004dieresis\004tonos\001small\002troke\001tilde\006below\001zhitsacyrillic\007dblgravecyrillic\000J\001aarmenian\001circle"
	"\000Jcircumflex\001ecyrillic\001heharmenian\001monospace\001small\000K\001Bsquare\001Ksquare\001abashkircyrillic\002cute\003yrillic\002descendercyrillic\002hookcyrillic\002ppa\002strokecyrillic\002verticalstrokecyrillic"
	"\000Kcaron\002edilla\002ircle\002ommaaccent\001dotbelow\001eharmenian\002narmenian\001hacyrillic\002eicoptic\002ook\001jecyrillic\001linebelow\001monospace\001oppacyrillic\005greek\001sicyrillic"
	"\000Ksmall\000L\001J\001L\001acute\002mbda\001caron\002edilla\002ircle\005umflexbelow\002ommaaccent\001dot\004accent\004below\011macron\001iwnarmenian"
	"\000Lj\002ecyrillic\001linebelow\001monospace\001slash\006small\002mall\000M\001Bsquare\001acron\006small\003ute\001circle\001dotaccent\004below\001enarmenian"
	"\000Mmonospace\001small\001turned\001u\000N\001J\001acute\001caron\002edilla\002ircle\005umflexbelow\002ommaaccent\001dotaccent\004below\001hookleft\001ineroman"
	"\000Nj\002ecyrillic\001linebelow\001monospace\001owarmenian\001small\001tilde\006small\001u\000O\001E\002small\001acute\006small\001barredcyrillic\007dieresiscyrillic"
	"\000Obreve\001caron\002enteredtilde\002ircle\005umflex\013acute\013dotbelow\013grave\013hookabove\013small\013tilde\002yrillic\001dblacute\004grave\002ieresis\011cyrillic"
	"\000Odieresissmall\002otbelow\001goneksmall\002rave\006small\001harmenian\002m\002ookabove\003rn\005acute\005dotbelow\005grave\005hookabove\005tilde\002ungarumlaut\001i"
	"\000Oinvertedbreve\001macron\007acute\007grave\002ega\005cyrillic\005greek\005roundcyrillic\005titlocyrillic\006onos\002icron\007tonos\002onospace\001neroman\001ogonek\007macron"
	"\000Oopen\001slash\006acute\006small\002mall\002trokeacute\001tcyrillic\002ilde\006acute\006dieresis\006small\000P\001acute\001circle\001dotaccent\001ecyrillic"
	"\000Peharmenian\002middlehookcyrillic\001hi\002ook\001i\002wrarmenian\001monospace\001si\003cyrillic\002mall\000Q\001circle\001monospace\001small\000R\001aarmenian"
	"\000Racute\001caron\002edilla\002ircle\002ommaaccent\001dblgrave\002otaccent\004below\011macron\001eharmenian\001fraktur\001ho\001ingsmall\003vertedbreve\001linebelow\001monospace"
	"\000Rsmall\006inverted\016superior\000S\001F010000\00320000\00330000\00340000\00350000\00360000\00370000\00380000\00390000\002100000\00310000\00390000"
	"\000SF200000\00310000\00320000\00330000\00340000\00350000\00360000\00370000\00380000\002360000\00370000\00380000\00390000\002400000\00310000\00320000"
	"\000SF430000\00340000\00350000\00360000\00370000\00380000\00390000\002500000\00310000\00320000\00330000\00340000\001acute\006dotaccent\002mpigreek\001caron"
	"\000Scarondotaccent\006small\002edilla\002hwa\005cyrillic\005dieresiscyrillic\002ircle\005umflex\002ommaaccent\001dotaccent\004below\011dotaccent\001eharmenian\002venroman\001haarmenian\003cyrillic"
	"\000Shchacyrillic\002eicoptic\002hacyrillic\002imacoptic\001igma\002xroman\001monospace\001oftsigncyrillic\001small\001tigmagreek\000T\001au\001bar\001caron\002edilla\002ircle"
	"\000Tcircumflexbelow\002ommaaccent\001dotaccent\004below\001ecyrillic\002descendercyrillic\002nroman\002tsecyrillic\001heta\002ook\003rn\005small\002reeroman\001ildesmall\002wnarmenian\001linebelow"
	"\000Tmonospace\001oarmenian\002nefive\004six\004two\001retroflexhook\001secyrillic\002hecyrillic\002mall\001welveroman\002oroman\000U\001acute\006small\001breve\001caron"
	"\000Ucircle\005umflex\013below\013small\002yrillic\001dblacute\004grave\002ieresis\011acute\011below\011caron\012yrillic\011grave\011macron\011small\002otbelow"
	"\000Ugrave\006small\001hookabove\003rn\005acute\005dotbelow\005grave\005hookabove\005tilde\002ungarumlaut\015cyrillic\001invertedbreve\001kcyrillic\001macron\007cyrillic\007dieresis"
	"\000Umonospace\001ogonek\001psilon\0071\007acutehooksymbolgreek\010frican\007dieresis\017hooksymbolgreek\007hooksymbol\007tonos\001ring\001shortcyrillic\002mall\002traightcyrillic\011strokecyrillic\001tilde"
	"\000Utildeacute\006below\000V\001circle\001dotbelow\001ecyrillic\002warmenian\001hook\001monospace\001oarmenian\001small\001tilde\000W\001acute\001circle\005umflex"
	"\000Wdieresis\002otaccent\004below\001grave\001monospace\001small\000X\001circle\001dieresis\002otaccent\001eharmenian\001i\001monospace\001small\000Y\001acute"
	"\000Yacutesmall\002tcyrillic\001circle\005umflex\001dieresis\011small\002otaccent\004below\001ericyrillic\003udieresiscyrillic\001grave\001hook\005above\001iarmenian\002cyrillic\002wnarmenian"
	"\000Ymonospace\001small\001tilde\001usbigcyrillic\006iotifiedcyrillic\003littlecyrillic\011iotifiedcyrillic\000Z\001aarmenian\002cute\001caron\006small\002ircle\005umflex\001dot\004accent"
	"\000Zdotbelow\001ecyrillic\002descendercyrillic\003ieresiscyrillic\002ta\001hearmenian\003brevecyrillic\003cyrillic\003descendercyrillic\004ieresiscyrillic\001linebelow\001monospace\001small\002troke\000a\001abengali"
	"\000aacute\002deva\002gujarati\004rmukhi\002matragurmukhi\002rusquare\002vowelsignbengali\013deva\013gujarati\001bbreviationmarkarmenian\014signdeva\002engali\002opomofo\002reve\006acute\006cyrillic"
	"\000abrevedotbelow\006grave\006hookabove\006tilde\001caron\002ircle\005umflex\013acute\013dotbelow\013grave\013hookabove\013tilde\002ute\005belowcmb\005cmb\006omb"
	"\000acutedeva\005lowmod\005tonecmb\002yrillic\001dblgrave\002dakgurmukhi\002eva\002ieresis\011cyrillic\011macron\002otbelow\004macron\001e\002acute\002korean\002macron"
	"\000afii00208\0058941\00410017\0108\0109\00720\0101\0102\0103\0104\0105\0106\0107\0108\0109\00730"
	"\000afii10031\0102\0103\0104\0105\0106\0107\0108\0109\00740\0101\0102\0103\0104\0105\0106"
	"\000afii10047\0108\0109\00750\0101\0102\0103\0104\0105\0106\0107\0108\0109\00760\0101\0102"
	"\000afii10063\0104\0105\0106\0107\0108\0109\00770\0101\0102\0103\0104\0105\0106\0107\0108"
	"\000afii10079\00780\0101\0102\0103\0104\0105\0106\0107\0108\0109\00790\0101\0102\0103\0104"
	"\000afii10095\0106\0107\0108\0109\006100\0101\0102\0103\0104\0105\0106\0107\0108\0109\00710"
	"\000afii10145\0106\0107\0108\00792\0103\0104\0105\0106\006831\0102\00746\004299\004300\0061\00457381"
	"\000afii57388\00792\0103\0104\0105\0106\0107\0108\0109\006400\0101\0103\0107\0109\00710\0101"
	"\000afii57412\0103\0104\0105\0106\0107\0108\0109\00720\0101\0102\0103\0104\0105\0106\0107"
	"\000afii57428\0109\00730\0101\0102\0103\0104\00740\0101\0102\0103\0104\0105\0106\0108\0109"
	"\000afii57450\0101\0102\0103\0104\0105\0106\0107\0108\00770\006505\0106\0107\0108\0109\00711"
	"\000afii57512\0103\0104\0109\00734\006636\00745\00758\00764\0105\0106\0107\0108\0109\00770\0101"
	"\000afii57672\0103\0104\0105\0106\0107\0108\0109\00780\0101\0102\0103\0104\0105\0106\0107"
	"\000afii57688\0109\00790\0104\0105\006700\0105\00716\0107\0108\00723\00793\0104\0105\0106\0107"
	"\000afii57798\0109\006800\0101\0102\0103\0104\0106\0107\00739\00741\0102\006929\00461248\00789\006352"
	"\000afii61573\0104\0105\006664\0053167\0054937\001grave\002ujarati\003rmukhi\001hiragana\002ookabove\001ibengali\003opomofo\002deva\002ecyrillic\002gujarati"
	"\000aigurmukhi\002matragurmukhi\002narabic\003finalarabic\003initialarabic\003medialarabic\003vertedbreve\002vowelsignbengali\013deva\013gujarati\001katakana\011halfwidth\002orean\001lef\004arabic\004dageshhebrew"
	"\000aleffinalarabic\004hamzaabovearabic\016finalarabic\011belowarabic\016finalarabic\005ebrew\004lamedhebrew\004maddaabovearabic\016finalarabic\006ksuraarabic\013finalarabic\013initialarabic\013medialarabic\004patahhebrew\004qamatshebrew\003ph"
	"\000allequal\002pha\005tonos\001macron\002onospace\002persand\011monospace\011small\002square\001nbopomofo\002gbopomofo\003khankhuthai\003le\005bracketleft\020vertical\014right"
	"\000anglebracketrightvertical\005left\005right\003strom\002oteleia\002udattadeva\003svarabengali\010deva\010gujarati\001ogonek\001paatosquare\003ren\002ostrophearmenian\012mod\002ple\003roaches"
	"\000approxequal\013orimage\006imatelyequal\001raeaekorean\005korean\002c\002ighthalfring\003ng\005acute\005below\002rowboth\005dashdown\011left\011right\011up\006blboth"
	"\000arrowdbldown\010left\010right\010up\006own\011left\011right\011white\005headdownmod\011leftmod\011rightmod\011upmod\006orizex\005left\011dbl\014stroke"
	"\000arrowleftoverright\011white\005right\012dblstroke\012heavy\012overleft\012white\005tableft\010right\005up\007dn\011bse\010ownbase\007left\013ofdown\007right"
	"\000arrowupwhite\005vertex\001sciicircum\013monospace\005tilde\012monospace\003ript\007turned\002mallhiragana\006katakana\016halfwidth\002terisk\010altonearabic\011rabic\010math\011onospace"
	"\000asterisksmall\007m\002uperior\002ymptoticallyequal\001t\002ilde\002monospace\002small\002urned\001ubengali\003opomofo\002deva\002gujarati\004rmukhi\002lengthmarkbengali\002matragurmukhi"
	"\000auvowelsignbengali\013deva\013gujarati\001vagrahadeva\001ybarmenian\002in\004altonehebrew\004hebrew\000b\001abengali\002ckslash\011monospace\002deva\002gujarati\004rmukhi\002hiragana"
	"\000bahtthai\002katakana\002r\003monospace\001bopomofo\001circle\001dotaccent\004below\001eamedsixteenthnotes\002cause\003yrillic\002harabic\003finalarabic\003initialarabic\004ragana\003medialarabic"
	"\000behmeeminitialarabic\010solatedarabic\003noonfinalarabic\002katakana\002narmenian\002t\003a\004symbolgreek\003dagesh\011hebrew\003hebrew\003rafehebrew\001habengali\003deva\003gujarati\005rmukhi"
	"\000bhook\001ihiragana\002katakana\002labialclick\002ndigurmukhi\002rusquare\001lackcircle\005diamond\006ownpointingtriangle\005leftpointingpointer\021triangle\007nticularbracketleft\032vertical\026right\033vertical\006owerlefttriangle"
	"\000blacklowerrighttriangle\005rectangle\006ightpointingpointer\022triangle\005smallsquare\007ilingface\006quare\006tar\005upperlefttriangle\012righttriangle\010ointingsmalltriangle\017triangle\003nk\002inebelow\002ock\001monospace"
	"\000bobaimaithai\002hiragana\002katakana\001paren\001qsquare\001raceex\005left\011bt\011mid\012onospace\011small\011tp\011vertical\005right\012bt\012mid"
	"\000bracerightmonospace\012small\012tp\012vertical\004ketleft\013bt\013ex\013monospace\013tp\007right\014bt\014ex\014monospace\014tp\002eve\005belowcmb"
	"\000brevecmb\005invertedbelowcmb\015cmb\015doublecmb\002idgebelowcmb\006invertedbelowcmb\002okenbar\001stroke\002uperior\001topbar\001uhiragana\002katakana\002llet\006inverse\006operator\004seye"
	"\000c\001aarmenian\002bengali\002cute\002deva\002gujarati\004rmukhi\002lsquare\002ndrabindubengali\013cmb\013deva\013gujarati\002pslock\002reof\003on\005belowcmb"
	"\000caroncmb\003riagereturn\001bopomofo\001caron\002edilla\010acute\002ircle\005umflex\002url\001dot\004accent\002square\001edilla\007cmb\002nt\004igrade"
	"\000centinferior\004monospace\004oldstyle\004superior\001haarmenian\003bengali\003deva\003gujarati\005rmukhi\002bopomofo\002eabkhasiancyrillic\003ckmark\004yrillic\003descenderabkhasiancyrillic\014cyrillic\004ieresiscyrillic"
	"\000cheharmenian\003khakassiancyrillic\003verticalstrokecyrillic\002i\003euchacirclekorean\010parenkorean\007circlekorean\007korean\007parenkorean\002ochangthai\007thai\005ingthai\005oethai\003ok\001ieucacirclekorean\006parenkorean"
	"\000cieuccirclekorean\005korean\005parenkorean\005uparenkorean\002rcle\006multiply\006ot\006plus\007ostalmark\006withlefthalfblack\012righthalfblack\004umflex\012belowcmb\012cmb\001lear\002ickalveolar"
	"\000clickdental\005lateral\005retroflex\002ub\004suitblack\010white\001mcubedsquare\002onospace\002squaredsquare\001oarmenian\002lon\005monetary\010ospace\005sign\006mall\005triangularhalfmod"
	"\000colontriangularmod\002mma\005abovecmb\012rightcmb\006ccent\006rabic\007menian\005inferior\005monospace\005reversedabovecmb\015mod\005small\006uperior\005turnedabovecmb\013mod\003pass"
	"\000congruent\003tourintegral\004rol\007ACK\007BEL\010S\007CAN\010R\007DC1\0112\0113\0114\010EL\010LE\007EM\010NQ"
	"\000controlEOT\010SC\010TB\011X\007FF\010S\007GS\007HT\007LF\007NAK\007RS\007SI\010O\011T\010TX\010UB"
	"\000controlSYN\007US\007VT\002pyright\011sans\012erif\002rnerbracketleft\021halfwidth\021vertical\015right\022halfwidth\022vertical\003porationsquare\002square\002verkgsquare\001paren"
	"\000cruzeiro\001stretched\001urlyand\005or\003rency\001yrBreve\003Flex\003breve\003flex\000d\001aarmenian\002bengali\002darabic\003eva\003finalarabic\003initialarabic"
	"\000dadmedialarabic\002gesh\006hebrew\003ger\006dbl\003ujarati\004rmukhi\002hiragana\002katakana\002larabic\003et\005dagesh\013hebrew\005hebrew\003finalarabic\002mmaarabic"
	"\000dammalowarabic\005tanaltonearabic\011rabic\002nda\002rgahebrew\005lefthebrew\002siapneumatacyrilliccmb\001blGrave\003anglebracketleft\023vertical\017right\024vertical\004rchinvertedbelowcmb\005rowleft\010right\003danda"
	"\000dblgrave\010cmb\003integral\003lowline\012cmb\003overlinecmb\003primemod\003verticalbar\013lineabovecmb\002opomofo\002square\001caron\002edilla\002ircle\005umflexbelow\002roat"
	"\000ddabengali\003deva\003gujarati\005rmukhi\003larabic\004finalarabic\002dhadeva\002habengali\004deva\004gujarati\006rmukhi\002otaccent\004below\001ecimalseparatorarabic\020persian\003yrillic"
	"\000degree\002hihebrew\004ragana\002icoptic\002katakana\002leteleft\006right\003ta\005turned\002nominatorminusonenumeratorbengali\002zh\001habengali\003deva\003gujarati\005rmukhi\002ook"
	"\000dialytikatonos\016cmb\003mond\007suitwhite\002eresis\010acute\010belowcmb\010cmb\010grave\010tonos\002hiragana\002katakana\002ttomark\002vide\006s\004sionslash"
	"\000djecyrillic\001kshade\001linebelow\002square\001macron\002onospace\001nblock\001ochadathai\002dekthai\002hiragana\002katakana\002llar\006inferior\006monospace\006oldstyle\006small"
	"\000dollarsuperior\002ng\002rusquare\002taccent\011cmb\003belowcmb\011omb\003katakana\003lessi\007j\010strokehook\003math\003tedcircle\002ubleyodpatah\016hebrew\002wntackbelowcmb"
	"\000downtackmod\001paren\001superior\001tail\002opbar\001uhiragana\002katakana\001z\002altone\002caron\003url\002eabkhasiancyrillic\003cyrillic\002hecyrillic\000e\001acute"
	"\000earth\001bengali\002opomofo\002reve\001candradeva\007gujarati\007vowelsigndeva\020gujarati\003ron\002edillabreve\002harmenian\003yiwnarmenian\002ircle\005umflex\013acute\013below"
	"\000ecircumflexdotbelow\013grave\013hookabove\013tilde\002yrillic\001dblgrave\002eva\002ieresis\002ot\004accent\004below\001egurmukhi\002matragurmukhi\001fcyrillic\001grave\002ujarati"
	"\000eharmenian\002bopomofo\002iragana\002ookabove\001ibopomofo\002ght\005arabic\005bengali\005circle\013inversesansserif\005deva\005eencircle\010paren\011eriod\005gujarati\007rmukhi"
	"\000eighthackarabic\007ngzhou\006notebeamed\005ideographicparen\006nferior\005monospace\005oldstyle\005paren\006eriod\010sian\005roman\005superior\005thai\002nvertedbreve\002otifiedcyrillic\001katakana"
	"\000ekatakanahalfwidth\002onkargurmukhi\003rean\001lcyrillic\002ement\003vencircle\006paren\007eriod\006roman\002lipsis\010vertical\001macron\007acute\007grave\002cyrillic\002dash"
	"\000emdashvertical\002onospace\002phasismarkarmenian\003tyset\001nbopomofo\002cyrillic\002dash\006vertical\003escendercyrillic\002g\003bopomofo\003hecyrillic\002hookcyrillic\002space\001ogonek\002korean"
	"\000eopen\005closed\005reversed\015closed\015hook\001paren\002silon\007tonos\001qual\005monospace\005small\006uperior\003ivalence\001rbopomofo\002cyrillic\002eversed"
	"\000ereversedcyrillic\001scyrillic\002descendercyrillic\002h\003curl\003ortdeva\006vowelsigndeva\003reversedloop\003squatreversed\002mallhiragana\006katakana\016halfwidth\002timated\002uperior\001ta\003rmenian"
	"\000etatonos\002h\002ilde\006below\002nahtafoukhhebrew\014lefthebrew\007hebrew\007lefthebrew\002urned\001ukorean\002ro\001vowelsignbengali\012deva\012gujarati\001xclam\006armenian"
	"\000exclamdbl\007own\012small\006monospace\006small\002istential\001zh\003caron\004url\003reversed\003tail\000f\001adeva\002gurmukhi\002hrenheit\002thaarabic"
	"\000fathalowarabic\005tanarabic\001bopomofo\001circle\001dotaccent\001eharabic\005menian\003finalarabic\003initialarabic\003medialarabic\002icoptic\002male\001f\002i\002l\001i"
	"\000fifteencircle\007paren\010eriod\002guredash\002lledbox\006rect\002nalkaf\010dagesh\016hebrew\010hebrew\005mem\010hebrew\005nun\010hebrew\005pe\007hebrew"
	"\000finaltsadi\012hebrew\002rsttonechinese\002sheye\002tacyrillic\002ve\004arabic\004bengali\004circle\012inversesansserif\004deva\004eighths\004gujarati\006rmukhi\004hackarabic\006ngzhou"
	"\000fiveideographicparen\005nferior\004monospace\004oldstyle\004paren\005eriod\007sian\004roman\004superior\004thai\001l\002orin\001monospace\002square\001ofanthai\004thai"
	"\000fongmanthai\002rall\002ur\004arabic\004bengali\004circle\012inversesansserif\004deva\004gujarati\006rmukhi\004hackarabic\006ngzhou\004ideographicparen\005nferior\004monospace\004numeratorbengali"
	"\000fouroldstyle\004paren\005eriod\007sian\004roman\004superior\004teencircle\010paren\011eriod\005hai\006tonechinese\001paren\001raction\003nc\000g\001abengali"
	"\000gacute\002deva\002farabic\003finalarabic\003initialarabic\003medialarabic\002gujarati\004rmukhi\002hiragana\002katakana\002mma\005latinsmall\005superior\002ngiacoptic\001bopomofo\002reve"
	"\000gcaron\002edilla\002ircle\005umflex\002ommaaccent\001dot\004accent\001ecyrillic\002hiragana\002katakana\002ometricallyequal\002reshaccenthebrew\006hebrew\006muqdamhebrew\003mandbls\003shayimaccenthebrew"
	"\000gershayimhebrew\002tamark\001habengali\003darmenian\004eva\003gujarati\005rmukhi\003inarabic\005finalarabic\005initialarabic\005medialarabic\002emiddlehookcyrillic\003strokecyrillic\003upturncyrillic\002hadeva\004gurmukhi"
	"\000ghook\002zsquare\001ihiragana\002katakana\002marmenian\003el\005dagesh\013hebrew\005hebrew\001jecyrillic\001lottalinvertedstroke\007stop\013inverted\013mod\013reversed\023mod"
	"\000glottalstopreversedsuperior\013stroke\021reversed\001macron\002onospace\001ohiragana\002katakana\001paren\003square\001radient\003ve\005belowcmb\005cmb\006omb\005deva\005lowmod"
	"\000gravemonospace\005tonecmb\002eater\007equal\014orless\007monospace\007orequivalent\011less\010verequal\007small\001script\002troke\001uhiragana\002illemotleft\011right\004singlleft"
	"\000guilsinglright\002katakana\002ramusquare\001ysquare\000h\001aabkhasiancyrillic\003ltonearabic\002bengali\002descendercyrillic\004va\002gujarati\004rmukhi\002harabic\003finalarabic\003initialarabic\004ragana"
	"\000hahmedialarabic\002itusquare\002katakana\012halfwidth\002lantgurmukhi\002mzaarabic\005lowarabic\002ngulfiller\002rdsigncyrillic\003poonleftbarbup\007rightbarbup\002square\002tafpatah\01216\01223\013f"
	"\000hatafpatahhebrew\012narrowhebrew\012quarterhebrew\012widehebrew\005qamats\0131b\01328\01334\013hebrew\013narrowhebrew\013quarterhebrew\013widehebrew\005segol\01217\01224\01230"
	"\000hatafsegolhebrew\012narrowhebrew\012quarterhebrew\012widehebrew\001bar\002opomofo\002revebelow\001cedilla\002ircle\005umflex\001dieresis\002otaccent\004below\001e\002art\005suitblack"
	"\000heartsuitwhite\002dagesh\010hebrew\002haltonearabic\004rabic\003ebrew\003finalaltonearabic\013twoarabic\011rabic\003hamzaabovefinalarabic\015isolatedarabic\003initialaltonearabic\013rabic\004ragana\003medialaltonearabic\012rabic"
	"\000heiseierasquare\002katakana\012halfwidth\003utaarusquare\002nghook\002rutusquare\002t\003hebrew\001hook\005superior\001ieuhacirclekorean\006parenkorean\005circlekorean\005korean\005parenkorean\002hiragana"
	"\000hikatakana\012halfwidth\002riq\00514\00521\006d\005hebrew\005narrowhebrew\005quarterhebrew\005widehebrew\001linebelow\001monospace\001oarmenian\002hipthai\004ragana\002katakana"
	"\000hokatakanahalfwidth\002lam\00519\00526\00532\005hebrew\005narrowhebrew\005quarterhebrew\005widehebrew\002nokhukthai\002okabovecomb\004cmb\004palatalizedbelowcmb\004retroflexbelowcmb\003nsquare\002ricoptic"
	"\000horizontalbar\003ncmb\002tsprings\002use\001paren\001superior\001turned\001uhiragana\002iitosquare\002katakana\012halfwidth\002ngarumlaut\014cmb\001v\001yphen\006inferior"
	"\000hyphenmonospace\006small\007uperior\006two\000i\001acute\003yrillic\001bengali\002opomofo\002reve\001caron\002ircle\005umflex\002yrillic\001dblgrave\002eographearthcircle"
	"\000ideographfirecircle\011icallianceparen\013callparen\014entrecircle\014lose\014omma\020left\015ngratulationparen\015rrectcircle\013earthparen\014nterpriseparen\014xcellentcircle\013festivalparen\014inancialcircle\024paren\015reparen"
	"\000ideographichaveparen\014ighcircle\013iterationmark\013laborcircle\020paren\014eftcircle\014owcircle\013medicinecircle\015talparen\014oonparen\013nameparen\013period\014rintcircle\013reachparen\015presentparen\015sourceparen"
	"\000ideographicrightcircle\013secretcircle\015lfparen\014ocietyparen\014pace\015ecialparen\014tockparen\015udyparen\014unparen\015perviseparen\013waterparen\014oodparen\013zero\011metalcircle\012ooncircle\011namecircle"
	"\000ideographsuncircle\011watercircle\012oodcircle\003va\002ieresis\011acute\011cyrillic\002otbelow\001ebrevecyrillic\002cyrillic\002ungacirclekorean\006parenkorean\005circlekorean\005korean\005parenkorean\001grave"
	"\000igujarati\003rmukhi\001hiragana\002ookabove\001ibengali\002cyrillic\002deva\002gujarati\004rmukhi\002matragurmukhi\002nvertedbreve\002shortcyrillic\002vowelsignbengali\013deva\013gujarati\001j"
	"\000ikatakana\011halfwidth\002orean\001lde\002uyhebrew\001macron\007cyrillic\003georapproximatelyequal\003tragurmukhi\002onospace\001ncrement\002finity\002iarmenian\002tegral\010bottom\011t"
	"\000integralex\010top\011p\004rsection\003isquare\002vbullet\003circle\003smileface\001ocyrillic\002gonek\002ta\004dieresis\014tonos\004latin\004tonos\001paren"
	"\000irigurmukhi\001smallhiragana\006katakana\016halfwidth\002sharbengali\002troke\002uperior\001terationhiragana\011katakana\002ilde\006below\001ubopomofo\002cyrillic\001vowelsignbengali\012deva\012gujarati"
	"\000izhitsacyrillic\007dblgravecyrillic\000j\001aarmenian\002bengali\002deva\002gujarati\004rmukhi\001bopomofo\001caron\002ircle\005umflex\002rossedtail\001dotlessstroke\001ecyrillic\002emarabic"
	"\000jeemfinalarabic\004initialarabic\004medialarabic\002harabic\003finalarabic\001habengali\003deva\003gujarati\005rmukhi\002eharmenian\001is\001monospace\001paren\001superior\000k\001abashkircyrillic"
	"\000kabengali\002cute\003yrillic\002descendercyrillic\004va\002f\003arabic\003dagesh\011hebrew\003finalarabic\003hebrew\003initialarabic\003medialarabic\003rafehebrew\002gujarati\004rmukhi"
	"\000kahiragana\003ookcyrillic\002katakana\012halfwidth\002ppa\005symbolgreek\003yeounmieumkorean\010phieuphkorean\011ieupkorean\010ssangpieupkorean\002roriisquare\002shidaautoarabic\013nosidebearingarabic\003mallkatakana\003quare\003raarabic"
	"\000kasratanarabic\003trokecyrillic\002tahiraprolongmarkhalfwidth\002verticalstrokecyrillic\001bopomofo\001calsquare\003ron\002edilla\002ircle\002ommaaccent\001dotbelow\001eharmenian\003iragana\002katakana\012halfwidth\002narmenian"
	"\000kesmallkatakana\001greenlandic\001habengali\003cyrillic\003deva\003gujarati\005rmukhi\003harabic\004finalarabic\004initialarabic\004medialarabic\002eicoptic\002hadeva\004gurmukhi\002ieukhacirclekorean\010parenkorean"
	"\000khieukhcirclekorean\007korean\007parenkorean\002okhaithai\005onthai\005uatthai\005waithai\003mutthai\003ok\003rakhangthai\002zsquare\001ihiragana\002katakana\012halfwidth\002roguramusquare\004meetorusquare"
	"\000kirosquare\002yeokacirclekorean\007parenkorean\006circlekorean\006korean\006parenkorean\006sioskorean\001jecyrillic\001linebelow\002square\001mcubedsquare\002onospace\002squaredsquare\001ohiragana\003msquare\002kaithai"
	"\000kokatakana\012halfwidth\002oposquare\002ppacyrillic\002reanstandardsymbol\003oniscmb\001paren\003square\001sicyrillic\001tsquare\002urned\001uhiragana\002katakana\012halfwidth\001vsquare\001wsquare"
	"\000l\001abengali\002cute\002deva\002gujarati\004rmukhi\002kkhangyaothai\002maleffinalarabic\007hamzaabovefinalarabic\021isolatedarabic\014belowfinalarabic\021isolatedarabic\007isolatedarabic\007maddaabovefinalarabic\021isolatedarabic\004rabic"
	"\000lambda\006stroke\003ed\005dagesh\013hebrew\005hebrew\003finalarabic\003hahinitialarabic\003initialarabic\003jeeminitialarabic\003khahinitialarabic\003lamhehisolatedarabic\003medialarabic\005emhahinitialarabic\007initialarabic\002rgecircle"
	"\000lbar\002elt\002opomofo\001caron\002edilla\002ircle\005umflexbelow\002ommaaccent\001dot\004accent\004below\011macron\001eftangleabovecmb\004tackbelowcmb\002ss\004equal"
	"\000lessequalorgreater\004monospace\004orequivalent\006greater\005verequal\004small\002zh\001fblock\001hookretroflex\001ira\002wnarmenian\001j\002ecyrillic\001l\002adeva\003gujarati"
	"\000llinebelow\002ladeva\002vocalicbengali\011deva\011vowelsignbengali\022deva\001middletilde\002onospace\002square\001ochulathai\002gicaland\007not\012reversed\007or\002lingthai\002ngs"
	"\000lowlinecenterline\010mb\007dashed\002zenge\001paren\001slash\002quare\002uperior\001tshade\001uthai\001vocalicbengali\010deva\010vowelsignbengali\021deva\001xsquare\000m"
	"\000mabengali\002cron\006belowcmb\006cmb\006lowmod\006monospace\003ute\002deva\002gujarati\004rmukhi\002hapakhhebrew\010lefthebrew\003iragana\002ichattawalowleftthai\016rightthai\013thai"
	"\000maichattawaupperleftthai\003eklowleftthai\010rightthai\005thai\005upperleftthai\003hanakatleftthai\012thai\003taikhuleftthai\011thai\004holowleftthai\011rightthai\006thai\006upperleftthai\004rilowleftthai\011rightthai\006thai"
	"\000maitriupperleftthai\003yamokthai\002katakana\012halfwidth\002le\002nsyonsquare\002qafhebrew\002rs\002soracirclehebrew\003quare\001bopomofo\002square\001circle\002ubedsquare\001dotaccent\004below"
	"\000meemarabic\004finalarabic\004initialarabic\004medialarabic\006eminitialarabic\011solatedarabic\003torusquare\002hiragana\002izierasquare\002katakana\012halfwidth\002m\003dagesh\011hebrew\003hebrew\002narmenian"
	"\000merkhahebrew\006kefulahebrew\014lefthebrew\006lefthebrew\001hook\002zsquare\001iddledotkatakanahalfwidth\004ot\002eumacirclekorean\006parenkorean\005circlekorean\005korean\005pansioskorean\007renkorean\006ieupkorean\005sioskorean"
	"\000mihiragana\002katakana\012halfwidth\002nus\005belowcmb\005circle\005mod\005plus\004te\002ribaarusquare\004square\001longlegturned\002square\001mcubedsquare\002onospace\002squaredsquare"
	"\000mohiragana\003msquare\002katakana\012halfwidth\002lsquare\002mathai\002verssquare\014dsquare\001paren\003square\001ssquare\002uperior\001turned\001u\0021\002asquare"
	"\000muchgreater\004less\002fsquare\002greek\003square\002hiragana\002katakana\012halfwidth\002lsquare\003tiply\002msquare\002nahhebrew\005lefthebrew\002sicalnote\013dbl\005flatsign"
	"\000musicsharpsign\003square\002vsquare\002wsquare\001vmegasquare\002square\001wmegasquare\002square\000n\001abengali\003la\002cute\002deva\002gujarati\004rmukhi\002hiragana"
	"\000nakatakana\012halfwidth\002postrophe\002square\001bopomofo\002space\001caron\002edilla\002ircle\005umflexbelow\002ommaaccent\001dotaccent\004below\001ehiragana\002katakana\012halfwidth"
	"\000newsheqelsign\001fsquare\001gabengali\003deva\003gujarati\005rmukhi\002onguthai\001hiragana\002ookleft\005retroflex\001ieunacirclekorean\006parenkorean\005cieuckorean\007rclekorean\005hieuhkorean\005korean"
	"\000nieunpansioskorean\007renkorean\005sioskorean\005tikeutkorean\002hiragana\002katakana\012halfwidth\003hahitleftthai\010thai\002ne\004arabic\004bengali\004circle\012inversesansserif\004deva\004gujarati"
	"\000ninegurmukhi\004hackarabic\006ngzhou\004ideographicparen\005nferior\004monospace\004oldstyle\004paren\005eriod\007sian\004roman\004superior\004teencircle\010paren\011eriod\005hai"
	"\000nj\002ecyrillic\001katakana\011halfwidth\001legrightlong\002inebelow\001monospace\002square\001nabengali\003deva\003gujarati\005rmukhi\002nadeva\001ohiragana\002katakana\012halfwidth"
	"\000nonbreakingspace\003enthai\003uthai\002onarabic\004finalarabic\004ghunnaarabic\012finalarabic\004initialarabic\004jeeminitialarabic\011solatedarabic\004medialarabic\006eminitialarabic\011solatedarabic\004noonfinalarabic\002tcontains\003element"
	"\000notelementof\004qual\003greater\012norequal\015less\003identical\003less\007norequal\003parallel\004recedes\003subset\005cceeds\005perset\002warmenian\001paren\001ssquare"
	"\000nsuperior\001tilde\001u\002hiragana\002katakana\012halfwidth\003tabengali\005deva\005gujarati\007rmukhi\002mbersign\012monospace\012small\003eralsigngreek\013lowergreek\005o"
	"\000nun\003dagesh\011hebrew\003hebrew\001vsquare\001wsquare\001yabengali\003deva\003gujarati\005rmukhi\000o\001acute\002ngthai\001barred\007cyrillic\007dieresiscyrillic"
	"\000obengali\002opomofo\002reve\001candradeva\007gujarati\007vowelsigndeva\020gujarati\003ron\002ircle\005umflex\013acute\013dotbelow\013grave\013hookabove\013tilde\002yrillic"
	"\000odblacute\004grave\002eva\002ieresis\011cyrillic\002otbelow\001e\002korean\001gonek\006cmb\002rave\002ujarati\001harmenian\002iragana\002ookabove\003rn"
	"\000ohornacute\005dotbelow\005grave\005hookabove\005tilde\002ungarumlaut\001i\002nvertedbreve\001katakana\011halfwidth\002orean\001lehebrew\001macron\007acute\007grave\002deva"
	"\000omega\0051\005cyrillic\005latinclosed\005roundcyrillic\005titlocyrillic\006onos\002gujarati\002icron\007tonos\002onospace\001ne\003arabic\003bengali\003circle\011inversesansserif"
	"\000onedeva\004otenleader\003eighth\003fitted\003gujarati\005rmukhi\003hackarabic\005lf\005ngzhou\003ideographicparen\004nferior\003monospace\003numeratorbengali\003oldstyle\003paren\004eriod"
	"\000onepersian\003quarter\003roman\003superior\003thai\005ird\001ogonek\007macron\003urmukhi\002matragurmukhi\002pen\001paren\002enbullet\002tion\001rdfeminine\003masculine"
	"\000orthogonal\001shortdeva\006vowelsigndeva\002lash\006acute\002mallhiragana\006katakana\016halfwidth\002trokeacute\002uperior\001tcyrillic\002ilde\006acute\006dieresis\001ubopomofo\001verline"
	"\000overlinecenterline\011mb\010dashed\011blwavy\010wavy\004score\002owelsignbengali\012deva\012gujarati\000p\001aampssquare\003sentosquare\002bengali\002cute\002deva\002gedown"
	"\000pageup\003ujarati\004rmukhi\002hiragana\002iyannoithai\002katakana\002latalizationcyrilliccmb\003ochkacyrillic\002nsioskorean\002ragraph\004llel\003enleft\011altonearabic\011bt\011ex\011inferior"
	"\000parenleftmonospace\011small\012uperior\011tp\011vertical\005right\012altonearabic\012bt\012ex\012inferior\012monospace\012small\013uperior\012tp\012vertical\003tialdiff"
	"\000paseqhebrew\003htahebrew\003quare\002tah\00511\006d\0052a\005hebrew\005narrowhebrew\005quarterhebrew\005widehebrew\002zerhebrew\001bopomofo\001circle\001dotaccent\001e"
	"\000pecyrillic\002dagesh\010hebrew\002ezisquare\002finaldageshhebrew\002harabic\005menian\003ebrew\003finalarabic\003initialarabic\004ragana\003medialarabic\002katakana\002middlehookcyrillic\002rafehebrew\003cent"
	"\000percentarabic\007monospace\007small\003iod\006armenian\006centered\006halfwidth\006inferior\006monospace\006small\007uperior\004spomenigreekcmb\003pendicular\003thousand\002seta\001fsquare"
	"\000phabengali\003deva\003gujarati\005rmukhi\002i\0031\003euphacirclekorean\010parenkorean\007circlekorean\007korean\007parenkorean\003latin\003nthuthai\003symbolgreek\002ook\003phanthai"
	"\000phophungthai\003samphaothai\001i\002eupacirclekorean\006parenkorean\005cieuckorean\007rclekorean\005kiyeokkorean\006orean\005parenkorean\005sioskiyeokkorean\012orean\011tikeutkorean\005thieuthkorean\006ikeutkorean\002hiragana"
	"\000pikatakana\002symbolgreek\002wrarmenian\001lus\004belowcmb\004circle\004minus\005od\006nospace\004small\005uperior\001monospace\002square\001ohiragana\002intingindexdownwhite\015leftwhite"
	"\000pointingindexrightwhite\015upwhite\002katakana\002plathai\002stalmark\012face\001paren\001recedes\003scription\002imemod\005reversed\002oduct\003jective\003longedkana\003pellor\005rsubset"
	"\000propersuperset\004ortion\012al\001si\003cyrillic\003lipneumatacyrilliccmb\002square\001uhiragana\002katakana\001vsquare\001wsquare\000q\001adeva\003mahebrew\002farabic\003finalarabic"
	"\000qafinitialarabic\003medialarabic\002mats\00610\007a\007c\00627\0079\00633\006de\006hebrew\006narrowhebrew\006qatanhebrew\013narrowhebrew\013quarterhebrew\013widehebrew"
	"\000qamatsquarterhebrew\006widehebrew\002rneyparahebrew\001bopomofo\001circle\001hook\001monospace\001of\003dagesh\011hebrew\003hebrew\001paren\001uarternote\002buts\00618\00625"
	"\000qubuts31\006hebrew\006narrowhebrew\006quarterhebrew\006widehebrew\002estion\010arabic\012menian\010down\014small\010greek\010monospace\010small\002otedbl\010base\010left"
	"\000quotedblmonospace\010prime\015reversed\010right\005left\011reversed\005reversed\006ight\012n\005singlbase\012e\013monospace\000r\001aarmenian\002bengali\002cute"
	"\000radeva\003ical\007ex\003overssquare\016dsquare\003square\002fe\004hebrew\002gujarati\004rmukhi\002hiragana\002katakana\012halfwidth\002lowerdiagonalbengali\002middlediagonalbengali\003shorn"
	"\000ratio\001bopomofo\001caron\002edilla\002ircle\002ommaaccent\001dblgrave\002otaccent\004below\011macron\001eferencemark\003lexsubset\010perset\002gistered\010sans\011erif"
	"\000reharabic\005menian\003finalarabic\003iragana\002katakana\012halfwidth\002sh\004dageshhebrew\004hebrew\002versedtilde\003iahebrew\005mugrashhebrew\003logicalnot\001fishhook\011reversed\001habengali"
	"\000rhadeva\002o\003ok\005turned\013superior\003symbolgreek\003tichookmod\001ieulacirclekorean\006parenkorean\005circlekorean\005hieuhkorean\005kiyeokkorean\013sioskorean\006orean\005mieumkorean\005pansioskorean"
	"\000rieulparenkorean\006hieuphkorean\006ieupkorean\012sioskorean\005sioskorean\005thieuthkorean\006ikeutkorean\005yeorinhieuhkorean\002ghtangle\005tackbelowcmb\006riangle\002hiragana\002katakana\012halfwidth\002ng\004belowcmb"
	"\000ringcmb\004halfleft\014armenian\014belowcmb\014centered\010right\015belowcmb\015centered\003vertedbreve\002ttorusquare\001linebelow\002ongleg\010turned\001monospace\001ohiragana\002katakana"
	"\000rokatakanahalfwidth\002ruathai\001paren\001rabengali\003deva\003gurmukhi\002eharabic\004finalarabic\002vocalicbengali\011deva\011gujarati\011vowelsignbengali\022deva\022gujarati\001superior\001tblock"
	"\000rturned\007superior\001uhiragana\002katakana\012halfwidth\002peemarkbengali\005signbengali\003iah\002thai\001vocalicbengali\010deva\010gujarati\010vowelsignbengali\021deva\021gujarati\000s"
	"\000sabengali\002cute\006dotaccent\002darabic\003eva\003finalarabic\003initialarabic\003medialarabic\002gujarati\004rmukhi\002hiragana\002katakana\012halfwidth\002llallahoualayhewasallamarabic\002mekh\006dagesh"
	"\000samekhdageshhebrew\006hebrew\002raaathai\005ethai\005imaimalaithai\012uanthai\005mthai\005thai\004ethai\004iileftthai\006thai\005leftthai\005thai\004othai\004ueeleftthai\007thai"
	"\000saraueleftthai\006thai\005thai\005uthai\001bopomofo\001caron\006dotaccent\002edilla\002hwa\005cyrillic\005dieresiscyrillic\005hook\002ircle\005umflex\002ommaaccent\001dotaccent"
	"\000sdotbelow\011dotaccent\001eagullbelowcmb\002cond\006tonechinese\003tion\002enarabic\004finalarabic\004initialarabic\004medialarabic\002gol\00513\006f\0052c\005hebrew\005narrowhebrew"
	"\000segolquarterhebrew\005tahebrew\005widehebrew\002harmenian\003iragana\002katakana\012halfwidth\002micolon\011arabic\011monospace\011small\004voicedmarkkana\022halfwidth\002ntisquare\004osquare\002ven"
	"\000sevenarabic\005bengali\005circle\013inversesansserif\005deva\005eighths\005gujarati\007rmukhi\005hackarabic\007ngzhou\005ideographicparen\006nferior\005monospace\005oldstyle\005paren\006eriod"
	"\000sevenpersian\005roman\005superior\005teencircle\011paren\012eriod\006hai\001fthyphen\001haarmenian\003bengali\003cyrillic\003ddaarabic\006dammaarabic\013tanarabic\006fathaarabic\006kasraarabic"
	"\000shaddakasratanarabic\004e\005dark\005light\005medium\005va\003gujarati\005rmukhi\003lshelethebrew\002bopomofo\002chacyrillic\002eenarabic\005finalarabic\005initialarabic\005medialarabic\003icoptic"
	"\000sheqel\006hebrew\003va\005115\0065\00522\006e\005hebrew\005narrowhebrew\005quarterhebrew\005widehebrew\002hacyrillic\002imacoptic\003n\004dagesh\012hebrew"
	"\000shindageshshindot\021hebrew\013indot\020hebrew\005othebrew\004hebrew\004shindot\013hebrew\005indot\012hebrew\002ook\001igma\0051\005final\005lunatesymbolgreek\002hiragana"
	"\000sikatakana\012halfwidth\002luqhebrew\005lefthebrew\002milar\002ndothebrew\002osacirclekorean\005parenkorean\004cieuckorean\006rclekorean\004kiyeokkorean\005orean\004nieunkorean\004parenkorean\005ieupkorean\004tikeutkorean"
	"\000six\003arabic\003bengali\003circle\011inversesansserif\003deva\003gujarati\005rmukhi\003hackarabic\005ngzhou\003ideographicparen\004nferior\003monospace\003oldstyle\003paren\004eriod"
	"\000sixpersian\003roman\003superior\003teencircle\010urrencydenominatorbengali\007paren\010eriod\004hai\001lash\005monospace\002ong\005dotaccent\001mileface\002onospace\001ofpasuqhebrew\003thyphen"
	"\000softsigncyrillic\002hiragana\002katakana\012halfwidth\002liduslongoverlaycmb\007shortoverlaycmb\002rusithai\002salathai\003othai\003uathai\001pace\005hackarabic\003de\005suitblack\011white\003ren"
	"\000squarebelowcmb\006cc\007m\006diagonalcrosshatchfill\006horizontalfill\006kg\007m\010capital\006ln\007og\006mg\007il\007m\007squared\006orthogonalcrosshatchfill\006upperlefttolowerrightfill"
	"\000squareupperrighttolowerleftfill\006verticalfill\006whitewithsmallblack\001rsquare\001sabengali\003deva\003gujarati\003ngcieuckorean\005hieuhkorean\005ieungkorean\005kiyeokkorean\005nieunkorean\005pieupkorean\005sioskorean\005tikeutkorean\002uperior"
	"\000sterling\010monospace\002rokelongoverlaycmb\006shortoverlaycmb\001ubset\006notequal\006orequal\002cceeds\003hthat\002hiragana\002katakana\012halfwidth\003unarabic\002mmation\002n\002perset"
	"\000supersetnotequal\010orequal\001vsquare\001youwaerasquare\000t\001abengali\002ckdown\004left\002deva\002gujarati\004rmukhi\002harabic\003finalarabic\003initialarabic\004ragana\003medialarabic"
	"\000taisyouerasquare\002katakana\012halfwidth\002tweelarabic\002u\002v\003dages\010h\011hebrew\003hebrew\001bar\002opomofo\001caron\002curl\002edilla\002heharabic"
	"\000tchehfinalarabic\005initialarabic\005medialarabic\002ircle\005umflexbelow\002ommaaccent\001dieresis\002otaccent\004below\001ecyrillic\002descendercyrillic\002harabic\003finalarabic\003hahinitialarabic\007solatedarabic\003initialarabic"
	"\000tehiragana\003jeeminitialarabic\010solatedarabic\003marbutaarabic\012finalarabic\004edialarabic\005eminitialarabic\010solatedarabic\003noonfinalarabic\002katakana\012halfwidth\002lephone\011black\003ishagedolahebrew\007qetanahebrew\002ncircle"
	"\000tenideographicparen\003paren\004eriod\003roman\002sh\002t\003dagesh\011hebrew\003hebrew\003secyrillic\002virhebrew\005lefthebrew\001habengali\003deva\003gujarati\005rmukhi"
	"\000thalarabic\004finalarabic\003nthakhatlowleftthai\016rightthai\013thai\013upperleftthai\002eharabic\004finalarabic\004initialarabic\004medialarabic\003reexists\005fore\003ta\0051\005symbolgreek\002ieuthacirclekorean"
	"\000thieuthaparenkorean\007circlekorean\007korean\007parenkorean\003rteencircle\010paren\011eriod\002onangmonthothai\003ok\003phuthaothai\003rn\003thahanthai\006nthai\005ongthai\005ungthai\003usandcyrillic"
	"\000thousandsseparatorarabic\022persian\002ree\005arabic\005bengali\005circle\013inversesansserif\005deva\005eighths\005gujarati\007rmukhi\005hackarabic\007ngzhou\005ideographicparen\006nferior\005monospace"
	"\000threenumeratorbengali\005oldstyle\005paren\006eriod\010sian\005quarters\015emdash\005roman\005superior\005thai\002zsquare\001ihiragana\002katakana\012halfwidth\003eutacirclekorean\007parenkorean"
	"\000tikeutcirclekorean\006korean\006parenkorean\002lde\005belowcmb\005cmb\006omb\005doublecmb\005operator\006verlaycmb\005verticalcmb\002mescircle\002pehahebrew\006lefthebrew\003pigurmukhi\002tlocyrilliccmb"
	"\000tiwnarmenian\001linebelow\001monospace\001oarmenian\002hiragana\002katakana\012halfwidth\002nebarextrahighmod\014lowmod\007highmod\007lowmod\007midmod\004five\004six\004two\003os"
	"\000tonsquare\002patakthai\002rtoiseshellbracketleft\030small\030vertical\024right\031small\031vertical\002taothai\001palatalhook\003ren\001rademark\011sans\012erif\002etroflexhook\002iagdn"
	"\000triaglf\005rt\005up\001s\002adi\005dagesh\013hebrew\005hebrew\002ecyrillic\003re\00512\006e\0052b\005hebrew\005narrowhebrew\005quarterhebrew"
	"\000tserewidehebrew\002hecyrillic\002uperior\001tabengali\003deva\003gujarati\005rmukhi\002eharabic\004finalarabic\004initialarabic\004medialarabic\002habengali\004deva\004gujarati\006rmukhi\002urned"
	"\000tuhiragana\002katakana\012halfwidth\002smallhiragana\007katakana\017halfwidth\001welvecircle\006paren\007eriod\006roman\003ntycircle\006hangzhou\006paren\007eriod\002o\003arabic"
	"\000twobengali\003circle\011inversesansserif\003deva\004otenleader\006leader\014vertical\003gujarati\005rmukhi\003hackarabic\005ngzhou\003ideographicparen\004nferior\003monospace\003numeratorbengali\003oldstyle"
	"\000twoparen\004eriod\006sian\003roman\003stroke\004uperior\003thai\005irds\000u\001acute\001bar\002engali\002opomofo\002reve\001caron\002ircle"
	"\000ucircumflex\013below\002yrillic\001dattadeva\002blacute\004grave\002eva\002ieresis\011acute\011below\011caron\012yrillic\011grave\011macron\002otbelow\001grave"
	"\000ugujarati\003rmukhi\001hiragana\002ookabove\003rn\005acute\005dotbelow\005grave\005hookabove\005tilde\002ungarumlaut\015cyrillic\001invertedbreve\001katakana\011halfwidth\002cyrillic"
	"\000ukorean\001macron\007cyrillic\007dieresis\003tragurmukhi\002onospace\001nderscore\012dbl\012monospace\012vertical\012wavy\002ion\003versal\001ogonek\001paren\002block"
	"\000upperdothebrew\002silon\007dieresis\017tonos\007latin\007tonos\002tackbelowcmb\006mod\001ragurmukhi\002ing\001shortcyrillic\002mallhiragana\006katakana\016halfwidth\002traightcyrillic\011strokecyrillic"
	"\000utilde\006acute\006below\001ubengali\002deva\002gujarati\004rmukhi\002matragurmukhi\002vowelsignbengali\013deva\013gujarati\001vowelsignbengali\012deva\012gujarati\000v\001adeva"
	"\000vagujarati\004rmukhi\002katakana\002v\003dagesh\01165\011hebrew\003hebrew\004olam\010hebrew\003vavhebrew\003yodhebrew\001circle\001dotbelow\001ecyrillic\002harabic"
	"\000vehfinalarabic\003initialarabic\003medialarabic\002katakana\002nus\002rticalbar\010lineabovecmb\014belowcmb\014lowmod\014mod\002warmenian\001hook\001ikatakana\002ramabengali\006deva\006gujarati"
	"\000visargabengali\007deva\007gujarati\001monospace\001oarmenian\002icediterationhiragana\017katakana\006markkana\016halfwidth\002katakana\001paren\001tilde\002urned\001uhiragana\002katakana\000w"
	"\000wacute\002ekorean\002hiragana\002katakana\012halfwidth\003orean\002smallhiragana\007katakana\002ttosquare\002vedash\003yunderscorevertical\002warabic\003finalarabic\003hamzaabovearabic\015finalarabic\001bsquare"
	"\000wcircle\005umflex\001dieresis\002otaccent\004below\001ehiragana\002ierstrass\002katakana\003orean\002okorean\001grave\001hitebullet\005circle\013inverse\006ornerbracketleft\026vertical"
	"\000whitecornerbracketright\027vertical\005diamond\014containingblacksmalldiamond\006ownpointingsmalltriangle\021triangle\005leftpointingsmalltriangle\021triangle\007nticularbracketleft\026right\005rightpointingsmalltriangle\022triangle\005smallsquare\007ilingface\006quare\006tar"
	"\000whitetelephone\006ortoiseshellbracketleft\031right\005uppointingsmalltriangle\017triangle\001ihiragana\002katakana\003orean\001monospace\001ohiragana\002katakana\012halfwidth\002n\003monospace\002waenthai\001paren"
	"\000wring\001superior\001turned\001ynn\000x\001abovecmb\001bopomofo\001circle\001dieresis\002otaccent\001eharmenian\001i\001monospace\001paren\001superior\000y"
	"\000yaadosquare\002bengali\002cute\002deva\002ekorean\002gujarati\004rmukhi\002hiragana\002katakana\012halfwidth\003orean\002makkanthai\002smallhiragana\007katakana\017halfwidth\002tcyrillic"
	"\000ycircle\005umflex\001dieresis\002otaccent\004below\001eharabic\003barreearabic\011finalarabic\003finalarabic\003hamzaabovearabic\015finalarabic\015initialarabic\015medialarabic\003initialarabic\003medialarabic\005eminitialarabic"
	"\000yehmeemisolatedarabic\003noonfinalarabic\003threedotsbelowarabic\002korean\002n\003monospace\002okorean\003rinhieuhkorean\002rahbenyomohebrew\014lefthebrew\003icyrillic\003udieresiscyrillic\002sieungkorean\010pansioskorean\010sioskorean\002tivhebrew"
	"\000ygrave\001hook\005above\001iarmenian\002cyrillic\002korean\002nyang\002wnarmenian\001monospace\001od\003dagesh\011hebrew\003hebrew\003yodhebrew\006patahhebrew\002hiragana"
	"\000yoikorean\002katakana\012halfwidth\003orean\002smallhiragana\007katakana\017halfwidth\002tgreek\002yaekorean\004korean\005thai\003ingthai\001paren\002ogegrammeni\015greekcmb\001r"
	"\000yring\001superior\001tilde\002urned\001uhiragana\002ikorean\002katakana\012halfwidth\003orean\002sbigcyrillic\006iotifiedcyrillic\003littlecyrillic\011iotifiedcyrillic\003mallhiragana\007katakana\017halfwidth"
	"\000yuyekorean\004okorean\001yabengali\003deva\000z\001aarmenian\002cute\002deva\002gurmukhi\002harabic\003finalarabic\003initialarabic\004ragana\003medialarabic\002inarabic\004finalarabic"
	"\000zakatakana\002qefgadolhebrew\005qatanhebrew\002rqahebrew\002yin\005dagesh\013hebrew\005hebrew\001bopomofo\001caron\002ircle\005umflex\002url\001dot\004accent\004below"
	"\000zecyrillic\002descendercyrillic\003ieresiscyrillic\002hiragana\002katakana\002ro\004arabic\004bengali\004deva\004gujarati\006rmukhi\004hackarabic\004inferior\004monospace\004oldstyle\004persian"
	"\000zerosuperior\004thai\004widthjoiner\011nonjoiner\011space\002ta\001hbopomofo\002earmenian\003brevecyrillic\003cyrillic\003descendercyrillic\004ieresiscyrillic\001ihiragana\002katakana\002norhebrew\001linebelow"
	"\000zmonospace\001ohiragana\002katakana\001paren\001retroflexhook\001stroke\001uhiragana\002katakana"
	;
static const unsigned short agl_blk[263] = {
	0, 99, 219, 339, 459, 577, 750, 867, 1010, 1127, 1267, 1401,
	1512, 1628, 1782, 1903, 2040, 2147, 2294, 2423, 2594, 2740, 2842, 2952,
	3064, 3182, 3304, 3424, 3553, 3664, 3779, 3910, 4012, 4113, 4220, 4369,
	4496, 4645, 4766, 4877, 5010, 5168, 5283, 5394, 5537, 5676, 5844, 5994,
	6108, 6227, 6276, 6317, 6359, 6400, 6442, 6485, 6537, 6581, 6622, 6664,
	6708, 6755, 6796, 6842, 6895, 7003, 7167, 7362, 7490, 7635, 7755, 7861,
	7973, 8114, 8255, 8393, 8543, 8692, 8879, 9062, 9166, 9269, 9415, 9528,
	9633, 9809, 9992, 10144, 10287, 10425, 10490, 10543, 10678, 10796, 10919, 11093,
	11224, 11368, 11505, 11620, 11755, 11885, 12008, 12142, 12277, 12410, 12563, 12694,
	12848, 12965, 13126, 13257, 13366, 13493, 13601, 13740, 13863, 14013, 14125, 14267,
	14425, 14602, 14732, 14864, 15000, 15162, 15317, 15448, 15586, 15770, 15930, 16062,
	16222, 16348, 16474, 16661, 16840, 17010, 17176, 17324, 17465, 17576, 17734, 17880,
	18032, 18172, 18365, 18548, 18719, 18880, 19054, 19207, 19402, 19596, 19718, 19851,
	19988, 20113, 20251, 20432, 20582, 20747, 20932, 21072, 21192, 21324, 21449, 21589,
	21750, 21906, 22040, 22178, 22377, 22503, 22632, 22752, 22879, 22986, 23113, 23247,
	23390, 23514, 23659, 23787, 23937, 24061, 24184, 24348, 24492, 24630, 24811, 24954,
	25097, 25235, 25358, 25487, 25612, 25728, 25873, 25998, 26151, 26314, 26491, 26636,
	26791, 26935, 27097, 27230, 27362, 27499, 27653, 27800, 27948, 28100, 28210, 28336,
	28513, 28651, 28799, 28950, 29094, 29298, 29442, 29588, 29702, 29892, 30090, 30217,
	30387, 30544, 30702, 30852, 31010, 31144, 31285, 31381, 31532, 31665, 31827, 31922,
	32037, 32175, 32297, 32454, 32587, 32706, 32855, 33002, 33168, 33308, 33540, 33709,
	33820, 33962, 34146, 34351, 34477, 34611, 34775, 34915, 35037, 35186, 35351,
};
static const unsigned short agl_uc[4200] = {
	0x0041, 0x00C6, 0x01FC, 0x01E2, 0xF7E6, 0x00C1, 0xF7E1, 0x0102, 0x1EAE, 0x04D0, 0x1EB6, 0x1EB0,
	0x1EB2, 0x1EB4, 0x01CD, 0x24B6, 0x00C2, 0x1EA4, 0x1EAC, 0x1EA6, 0x1EA8, 0xF7E2, 0x1EAA, 0xF6C9,
	0xF7B4, 0x0410, 0x0200, 0x00C4, 0x04D2, 0x01DE, 0xF7E4, 0x1EA0, 0x01E0, 0x00C0, 0xF7E0, 0x1EA2,
	0x04D4, 0x0202, 0x0391, 0x0386, 0x0100, 0xFF21, 0x0104, 0x00C5, 0x01FA, 0x1E00, 0xF7E5, 0xF761,
	0x00C3, 0xF7E3, 0x0531, 0x0042, 0x24B7, 0x1E02, 0x1E04, 0x0411, 0x0532, 0x0392, 0x0181, 0x1E06,
	0xFF22, 0xF6F4, 0xF762, 0x0182, 0x0043, 0x053E, 0x0106, 0xF6CA, 0xF6F5, 0x010C, 0x00C7, 0x1E08,
	0xF7E7, 0x24B8, 0x0108, 0x010A, 0x010A, 0xF7B8, 0x0549, 0x04BC, 0x0427, 0x04BE, 0x04B6, 0x04F4,
	0x0543, 0x04CB, 0x04B8, 0x03A7, 0x0187, 0xF6F6, 0xFF23, 0x0551, 0xF763, 0x0044, 0x01F1, 0x01C4,
	0x0534, 0x0189, 0x010E, 0x1E10, 0x24B9, 0x1E12, 0x0110, 0x1E0A, 0x1E0C, 0x0414, 0x03EE, 0x2206,
	0x0394, 0x018A, 0xF6CB, 0xF6CC, 0xF6CD, 0xF7A8, 0x03DC, 0x0402, 0x1E0E, 0xFF24, 0xF6F7, 0x0110,
	0xF764, 0x018B, 0x01F2, 0x01C5, 0x04E0, 0x0405, 0x040F, 0x0045, 0x00C9, 0xF7E9, 0x0114, 0x011A,
	0x1E1C, 0x0535, 0x24BA, 0x00CA, 0x1EBE, 0x1E18, 0x1EC6, 0x1EC0, 0x1EC2, 0xF7EA, 0x1EC4, 0x0404,
	0x0204, 0x00CB, 0xF7EB, 0x0116, 0x0116, 0x1EB8, 0x0424, 0x00C8, 0xF7E8, 0x0537, 0x1EBA, 0x2167,
	0x0206, 0x0464, 0x041B, 0x216A, 0x0112, 0x1E16, 0x1E14, 0x041C, 0xFF25, 0x041D, 0x04A2, 0x014A,
	0x04A4, 0x04C7, 0x0118, 0x0190, 0x0395, 0x0388, 0x0420, 0x018E, 0x042D, 0x0421, 0x04AA, 0x01A9,
	0xF765, 0x0397, 0x0538, 0x0389, 0x00D0, 0xF7F0, 0x1EBC, 0x1E1A, 0x20AC, 0x01B7, 0x01EE, 0x01B8,
	0x0046, 0x24BB, 0x1E1E, 0x0556, 0x03E4, 0x0191, 0x0472, 0x2164, 0xFF26, 0x2163, 0xF766, 0x0047,
	0x3387, 0x01F4, 0x0393, 0x0194, 0x03EA, 0x011E, 0x01E6, 0x0122, 0x24BC, 0x011C, 0x0122, 0x0120,
	0x0120, 0x0413, 0x0542, 0x0494, 0x0492, 0x0490, 0x0193, 0x0533, 0x0403, 0x1E20, 0xFF27, 0xF6CE,
	0xF760, 0xF767, 0x029B, 0x01E4, 0x0048, 0x25CF, 0x25AA, 0x25AB, 0x25A1, 0x33CB, 0x04A8, 0x04B2,
	0x042A, 0x0126, 0x1E2A, 0x1E28, 0x24BD, 0x0124, 0x1E26, 0x1E22, 0x1E24, 0xFF28, 0x0540, 0x03E8,
	0xF768, 0xF6CF, 0xF6F8, 0x3390, 0x0049, 0x042F, 0x0132, 0x042E, 0x00CD, 0xF7ED, 0x012C, 0x01CF,
	0x24BE, 0x00CE, 0xF7EE, 0x0406, 0x0208, 0x00CF, 0x1E2E, 0x04E4, 0xF7EF, 0x0130, 0x0130, 0x1ECA,
	0x04D6, 0x0415, 0x2111, 0x00CC, 0xF7EC, 0x1EC8, 0x0418, 0x020A, 0x0419, 0x012A, 0x04E2, 0xFF29,
	0x053B, 0x0401, 0x012E, 0x0399, 0x0196, 0x03AA, 0x038A, 0xF769, 0x0197, 0x0128, 0x1E2C, 0x0474,
	0x0476, 0x004A, 0x0541, 0x24BF, 0x0134, 0x0408, 0x054B, 0xFF2A, 0xF76A, 0x004B, 0x3385, 0x33CD,
	0x04A0, 0x1E30, 0x041A, 0x049A, 0x04C3, 0x039A, 0x049E, 0x049C, 0x01E8, 0x0136, 0x24C0, 0x0136,
	0x1E32, 0x0554, 0x053F, 0x0425, 0x03E6, 0x0198, 0x040C, 0x1E34, 0xFF2B, 0x0480, 0x03DE, 0x046E,
	0xF76B, 0x004C, 0x01C7, 0xF6BF, 0x0139, 0x039B, 0x013D, 0x013B, 0x24C1, 0x1E3C, 0x013B, 0x013F,
	0x013F, 0x1E36, 0x1E38, 0x053C, 0x01C8, 0x0409, 0x1E3A, 0xFF2C, 0x0141, 0xF6F9, 0xF76C, 0x004D,
	0x3386, 0xF6D0, 0xF7AF, 0x1E3E, 0x24C2, 0x1E40, 0x1E42, 0x0544, 0xFF2D, 0xF76D, 0x019C, 0x039C,
	0x004E, 0x01CA, 0x0143, 0x0147, 0x0145, 0x24C3, 0x1E4A, 0x0145, 0x1E44, 0x1E46, 0x019D, 0x2168,
	0x01CB, 0x040A, 0x1E48, 0xFF2E, 0x0546, 0xF76E, 0x00D1, 0xF7F1, 0x039D, 0x004F, 0x0152, 0xF6FA,
	0x00D3, 0xF7F3, 0x04E8, 0x04EA, 0x014E, 0x01D1, 0x019F, 0x24C4, 0x00D4, 0x1ED0, 0x1ED8, 0x1ED2,
	0x1ED4, 0xF7F4, 0x1ED6, 0x041E, 0x0150, 0x020C, 0x00D6, 0x04E6, 0xF7F6, 0x1ECC, 0xF6FB, 0x00D2,
	0xF7F2, 0x0555, 0x2126, 0x1ECE, 0x01A0, 0x1EDA, 0x1EE2, 0x1EDC, 0x1EDE, 0x1EE0, 0x0150, 0x01A2,
	0x020E, 0x014C, 0x1E52, 0x1E50, 0x2126, 0x0460, 0x03A9, 0x047A, 0x047C, 0x038F, 0x039F, 0x038C,
	0xFF2F, 0x2160, 0x01EA, 0x01EC, 0x0186, 0x00D8, 0x01FE, 0xF7F8, 0xF76F, 0x01FE, 0x047E, 0x00D5,
	0x1E4C, 0x1E4E, 0xF7F5, 0x0050, 0x1E54, 0x24C5, 0x1E56, 0x041F, 0x054A, 0x04A6, 0x03A6, 0x01A4,
	0x03A0, 0x0553, 0xFF30, 0x03A8, 0x0470, 0xF770, 0x0051, 0x24C6, 0xFF31, 0xF771, 0x0052, 0x054C,
	0x0154, 0x0158, 0x0156, 0x24C7, 0x0156, 0x0210, 0x1E58, 0x1E5A, 0x1E5C, 0x0550, 0x211C, 0x03A1,
	0xF6FC, 0x0212, 0x1E5E, 0xFF32, 0xF772, 0x0281, 0x02B6, 0x0053, 0x250C, 0x2514, 0x2510, 0x2518,
	0x253C, 0x252C, 0x2534, 0x251C, 0x2524, 0x2500, 0x2502, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563,
	0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560,
	0x2550, 0x256C, 0x2567, 0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A,
	0x015A, 0x1E64, 0x03E0, 0x0160, 0x1E66, 0xF6FD, 0x015E, 0x018F, 0x04D8, 0x04DA, 0x24C8, 0x015C,
	0x0218, 0x1E60, 0x1E62, 0x1E68, 0x054D, 0x2166, 0x0547, 0x0428, 0x0429, 0x03E2, 0x04BA, 0x03EC,
	0x03A3, 0x2165, 0xFF33, 0x042C, 0xF773, 0x03DA, 0x0054, 0x03A4, 0x0166, 0x0164, 0x0162, 0x24C9,
	0x1E70, 0x0162, 0x1E6A, 0x1E6C, 0x0422, 0x04AC, 0x2169, 0x04B4, 0x0398, 0x01AC, 0x00DE, 0xF7FE,
	0x2162, 0xF6FE, 0x054F, 0x1E6E, 0xFF34, 0x0539, 0x01BC, 0x0184, 0x01A7, 0x01AE, 0x0426, 0x040B,
	0xF774, 0x216B, 0x2161, 0x0055, 0x00DA, 0xF7FA, 0x016C, 0x01D3, 0x24CA, 0x00DB, 0x1E76, 0xF7FB,
	0x0423, 0x0170, 0x0214, 0x00DC, 0x01D7, 0x1E72, 0x01D9, 0x04F0, 0x01DB, 0x01D5, 0xF7FC, 0x1EE4,
	0x00D9, 0xF7F9, 0x1EE6, 0x01AF, 0x1EE8, 0x1EF0, 0x1EEA, 0x1EEC, 0x1EEE, 0x0170, 0x04F2, 0x0216,
	0x0478, 0x016A, 0x04EE, 0x1E7A, 0xFF35, 0x0172, 0x03A5, 0x03D2, 0x03D3, 0x01B1, 0x03AB, 0x03D4,
	0x03D2, 0x038E, 0x016E, 0x040E, 0xF775, 0x04AE, 0x04B0, 0x0168, 0x1E78, 0x1E74, 0x0056, 0x24CB,
	0x1E7E, 0x0412, 0x054E, 0x01B2, 0xFF36, 0x0548, 0xF776, 0x1E7C, 0x0057, 0x1E82, 0x24CC, 0x0174,
	0x1E84, 0x1E86, 0x1E88, 0x1E80, 0xFF37, 0xF777, 0x0058, 0x24CD, 0x1E8C, 0x1E8A, 0x053D, 0x039E,
	0xFF38, 0xF778, 0x0059, 0x00DD, 0xF7FD, 0x0462, 0x24CE, 0x0176, 0x0178, 0xF7FF, 0x1E8E, 0x1EF4,
	0x042B, 0x04F8, 0x1EF2, 0x01B3, 0x1EF6, 0x0545, 0x0407, 0x0552, 0xFF39, 0xF779, 0x1EF8, 0x046A,
	0x046C, 0x0466, 0x0468, 0x005A, 0x0536, 0x0179, 0x017D, 0xF6FF, 0x24CF, 0x1E90, 0x017B, 0x017B,
	0x1E92, 0x0417, 0x0498, 0x04DE, 0x0396, 0x053A, 0x04C1, 0x0416, 0x0496, 0x04DC, 0x1E94, 0xFF3A,
	0xF77A, 0x01B5, 0x0061, 0x0986, 0x00E1, 0x0906, 0x0A86, 0x0A06, 0x0A3E, 0x3303, 0x09BE, 0x093E,
	0x0ABE, 0x055F, 0x0970, 0x0985, 0x311A, 0x0103, 0x1EAF, 0x04D1, 0x1EB7, 0x1EB1, 0x1EB3, 0x1EB5,
	0x01CE, 0x24D0, 0x00E2, 0x1EA5, 0x1EAD, 0x1EA7, 0x1EA9, 0x1EAB, 0x00B4, 0x0317, 0x0301, 0x0301,
	0x0954, 0x02CF, 0x0341, 0x0430, 0x0201, 0x0A71, 0x0905, 0x00E4, 0x04D3, 0x01DF, 0x1EA1, 0x01E1,
	0x00E6, 0x01FD, 0x3150, 0x01E3, 0x2015, 0x20A4, 0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415,
	0x0401, 0x0416, 0x0417, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F, 0x0420,
	0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C,
	0x042D, 0x042E, 0x042F, 0x0490, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407, 0x0408, 0x0409,
	0x040A, 0x040B, 0x040C, 0x040E, 0xF6C4, 0xF6C5, 0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435,
	0x0451, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F, 0x0440,
	0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044A, 0x044B, 0x044C,
	0x044D, 0x044E, 0x044F, 0x0491, 0x0452, 0x0453, 0x0454, 0x0455, 0x0456, 0x0457, 0x0458, 0x0459,
	0x045A, 0x045B, 0x045C, 0x045E, 0x040F, 0x0462, 0x0472, 0x0474, 0xF6C6, 0x045F, 0x0463, 0x0473,
	0x0475, 0xF6C7, 0xF6C8, 0x04D9, 0x200E, 0x200F, 0x200D, 0x066A, 0x060C, 0x0660, 0x0661, 0x0662,
	0x0663, 0x0664, 0x0665, 0x0666, 0x0667, 0x0668, 0x0669, 0x061B, 0x061F, 0x0621, 0x0622, 0x0623,
	0x0624, 0x0625, 0x0626, 0x0627, 0x0628, 0x0629, 0x062A, 0x062B, 0x062C, 0x062D, 0x062E, 0x062F,
	0x0630, 0x0631, 0x0632, 0x0633, 0x0634, 0x0635, 0x0636, 0x0637, 0x0638, 0x0639, 0x063A, 0x0640,
	0x0641, 0x0642, 0x0643, 0x0644, 0x0645, 0x0646, 0x0648, 0x0649, 0x064A, 0x064B, 0x064C, 0x064D,
	0x064E, 0x064F, 0x0650, 0x0651, 0x0652, 0x0647, 0x06A4, 0x067E, 0x0686, 0x0698, 0x06AF, 0x0679,
	0x0688, 0x0691, 0x06BA, 0x06D2, 0x06D5, 0x20AA, 0x05BE, 0x05C3, 0x05D0, 0x05D1, 0x05D2, 0x05D3,
	0x05D4, 0x05D5, 0x05D6, 0x05D7, 0x05D8, 0x05D9, 0x05DA, 0x05DB, 0x05DC, 0x05DD, 0x05DE, 0x05DF,
	0x05E0, 0x05E1, 0x05E2, 0x05E3, 0x05E4, 0x05E5, 0x05E6, 0x05E7, 0x05E8, 0x05E9, 0x05EA, 0xFB2A,
	0xFB2B, 0xFB4B, 0xFB1F, 0x05F0, 0x05F1, 0x05F2, 0xFB35, 0x05B4, 0x05B5, 0x05B6, 0x05BB, 0x05B8,
	0x05B7, 0x05B0, 0x05B2, 0x05B1, 0x05B3, 0x05C2, 0x05C1, 0x05B9, 0x05BC, 0x05BD, 0x05BF, 0x05C0,
	0x02BC, 0x2105, 0x2113, 0x2116, 0x202C, 0x202D, 0x202E, 0x200C, 0x066D, 0x02BD, 0x00E0, 0x0A85,
	0x0A05, 0x3042, 0x1EA3, 0x0990, 0x311E, 0x0910, 0x04D5, 0x0A90, 0x0A10, 0x0A48, 0x0639, 0xFECA,
	0xFECB, 0xFECC, 0x0203, 0x09C8, 0x0948, 0x0AC8, 0x30A2, 0xFF71, 0x314F, 0x05D0, 0x0627, 0xFB30,
	0xFE8E, 0x0623, 0xFE84, 0x0625, 0xFE88, 0x05D0, 0xFB4F, 0x0622, 0xFE82, 0x0649, 0xFEF0, 0xFEF3,
	0xFEF4, 0xFB2E, 0xFB2F, 0x2135, 0x224C, 0x03B1, 0x03AC, 0x0101, 0xFF41, 0x0026, 0xFF06, 0xF726,
	0x33C2, 0x3122, 0x3124, 0x0E5A, 0x2220, 0x3008, 0xFE3F, 0x3009, 0xFE40, 0x2329, 0x232A, 0x212B,
	0x0387, 0x0952, 0x0982, 0x0902, 0x0A82, 0x0105, 0x3300, 0x249C, 0x055A, 0x02BC, 0xF8FF, 0x2250,
	0x2248, 0x2252, 0x2245, 0x318E, 0x318D, 0x2312, 0x1E9A, 0x00E5, 0x01FB, 0x1E01, 0x2194, 0x21E3,
	0x21E0, 0x21E2, 0x21E1, 0x21D4, 0x21D3, 0x21D0, 0x21D2, 0x21D1, 0x2193, 0x2199, 0x2198, 0x21E9,
	0x02C5, 0x02C2, 0x02C3, 0x02C4, 0xF8E7, 0x2190, 0x21D0, 0x21CD, 0x21C6, 0x21E6, 0x2192, 0x21CF,
	0x279E, 0x21C4, 0x21E8, 0x21E4, 0x21E5, 0x2191, 0x2195, 0x21A8, 0x21A8, 0x2196, 0x21C5, 0x2197,
	0x21E7, 0xF8E6, 0x005E, 0xFF3E, 0x007E, 0xFF5E, 0x0251, 0x0252, 0x3041, 0x30A1, 0xFF67, 0x002A,
	0x066D, 0x066D, 0x2217, 0xFF0A, 0xFE61, 0x2042, 0xF6E9, 0x2243, 0x0040, 0x00E3, 0xFF20, 0xFE6B,
	0x0250, 0x0994, 0x3120, 0x0914, 0x0A94, 0x0A14, 0x09D7, 0x0A4C, 0x09CC, 0x094C, 0x0ACC, 0x093D,
	0x0561, 0x05E2, 0xFB20, 0x05E2, 0x0062, 0x09AC, 0x005C, 0xFF3C, 0x092C, 0x0AAC, 0x0A2C, 0x3070,
	0x0E3F, 0x30D0, 0x007C, 0xFF5C, 0x3105, 0x24D1, 0x1E03, 0x1E05, 0x266C, 0x2235, 0x0431, 0x0628,
	0xFE90, 0xFE91, 0x3079, 0xFE92, 0xFC9F, 0xFC08, 0xFC6D, 0x30D9, 0x0562, 0x05D1, 0x03B2, 0x03D0,
	0xFB31, 0xFB31, 0x05D1, 0xFB4C, 0x09AD, 0x092D, 0x0AAD, 0x0A2D, 0x0253, 0x3073, 0x30D3, 0x0298,
	0x0A02, 0x3331, 0x25CF, 0x25C6, 0x25BC, 0x25C4, 0x25C0, 0x3010, 0xFE3B, 0x3011, 0xFE3C, 0x25E3,
	0x25E2, 0x25AC, 0x25BA, 0x25B6, 0x25AA, 0x263B, 0x25A0, 0x2605, 0x25E4, 0x25E5, 0x25B4, 0x25B2,
	0x2423, 0x1E07, 0x2588, 0xFF42, 0x0E1A, 0x307C, 0x30DC, 0x249D, 0x33C3, 0xF8F4, 0x007B, 0xF8F3,
	0xF8F2, 0xFF5B, 0xFE5B, 0xF8F1, 0xFE37, 0x007D, 0xF8FE, 0xF8FD, 0xFF5D, 0xFE5C, 0xF8FC, 0xFE38,
	0x005B, 0xF8F0, 0xF8EF, 0xFF3B, 0xF8EE, 0x005D, 0xF8FB, 0xF8FA, 0xFF3D, 0xF8F9, 0x02D8, 0x032E,
	0x0306, 0x032F, 0x0311, 0x0361, 0x032A, 0x033A, 0x00A6, 0x0180, 0xF6EA, 0x0183, 0x3076, 0x30D6,
	0x2022, 0x25D8, 0x2219, 0x25CE, 0x0063, 0x056E, 0x099A, 0x0107, 0x091A, 0x0A9A, 0x0A1A, 0x3388,
	0x0981, 0x0310, 0x0901, 0x0A81, 0x21EA, 0x2105, 0x02C7, 0x032C, 0x030C, 0x21B5, 0x3118, 0x010D,
	0x00E7, 0x1E09, 0x24D2, 0x0109, 0x0255, 0x010B, 0x010B, 0x33C5, 0x00B8, 0x0327, 0x00A2, 0x2103,
	0xF6DF, 0xFFE0, 0xF7A2, 0xF6E0, 0x0579, 0x099B, 0x091B, 0x0A9B, 0x0A1B, 0x3114, 0x04BD, 0x2713,
	0x0447, 0x04BF, 0x04B7, 0x04F5, 0x0573, 0x04CC, 0x04B9, 0x03C7, 0x3277, 0x3217, 0x3269, 0x314A,
	0x3209, 0x0E0A, 0x0E08, 0x0E09, 0x0E0C, 0x0188, 0x3276, 0x3216, 0x3268, 0x3148, 0x3208, 0x321C,
	0x25CB, 0x2297, 0x2299, 0x2295, 0x3036, 0x25D0, 0x25D1, 0x02C6, 0x032D, 0x0302, 0x2327, 0x01C2,
	0x01C0, 0x01C1, 0x01C3, 0x2663, 0x2663, 0x2667, 0x33A4, 0xFF43, 0x33A0, 0x0581, 0x003A, 0x20A1,
	0xFF1A, 0x20A1, 0xFE55, 0x02D1, 0x02D0, 0x002C, 0x0313, 0x0315, 0xF6C3, 0x060C, 0x055D, 0xF6E1,
	0xFF0C, 0x0314, 0x02BD, 0xFE50, 0xF6E2, 0x0312, 0x02BB, 0x263C, 0x2245, 0x222E, 0x2303, 0x0006,
	0x0007, 0x0008, 0x0018, 0x000D, 0x0011, 0x0012, 0x0013, 0x0014, 0x007F, 0x0010, 0x0019, 0x0005,
	0x0004, 0x001B, 0x0017, 0x0003, 0x000C, 0x001C, 0x001D, 0x0009, 0x000A, 0x0015, 0x001E, 0x000F,
	0x000E, 0x0002, 0x0001, 0x001A, 0x0016, 0x001F, 0x000B, 0x00A9, 0xF8E9, 0xF6D9, 0x300C, 0xFF62,
	0xFE41, 0x300D, 0xFF63, 0xFE42, 0x337F, 0x33C7, 0x33C6, 0x249E, 0x20A2, 0x0297, 0x22CF, 0x22CE,
	0x00A4, 0xF6D1, 0xF6D2, 0xF6D4, 0xF6D5, 0x0064, 0x0564, 0x09A6, 0x0636, 0x0926, 0xFEBE, 0xFEBF,
	0xFEC0, 0x05BC, 0x05BC, 0x2020, 0x2021, 0x0AA6, 0x0A26, 0x3060, 0x30C0, 0x062F, 0x05D3, 0xFB33,
	0xFB33, 0x05D3, 0xFEAA, 0x064F, 0x064F, 0x064C, 0x064C, 0x0964, 0x05A7, 0x05A7, 0x0485, 0xF6D3,
	0x300A, 0xFE3D, 0x300B, 0xFE3E, 0x032B, 0x21D4, 0x21D2, 0x0965, 0xF6D6, 0x030F, 0x222C, 0x2017,
	0x0333, 0x033F, 0x02BA, 0x2016, 0x030E, 0x3109, 0x33C8, 0x010F, 0x1E11, 0x24D3, 0x1E13, 0x0111,
	0x09A1, 0x0921, 0x0AA1, 0x0A21, 0x0688, 0xFB89, 0x095C, 0x09A2, 0x0922, 0x0AA2, 0x0A22, 0x1E0B,
	0x1E0D, 0x066B, 0x066B, 0x0434, 0x00B0, 0x05AD, 0x3067, 0x03EF, 0x30C7, 0x232B, 0x2326, 0x03B4,
	0x018D, 0x09F8, 0x02A4, 0x09A7, 0x0927, 0x0AA7, 0x0A27, 0x0257, 0x0385, 0x0344, 0x2666, 0x2662,
	0x00A8, 0xF6D7, 0x0324, 0x0308, 0xF6D8, 0x0385, 0x3062, 0x30C2, 0x3003, 0x00F7, 0x2223, 0x2215,
	0x0452, 0x2593, 0x1E0F, 0x3397, 0x0111, 0xFF44, 0x2584, 0x0E0E, 0x0E14, 0x3069, 0x30C9, 0x0024,
	0xF6E3, 0xFF04, 0xF724, 0xFE69, 0xF6E4, 0x20AB, 0x3326, 0x02D9, 0x0307, 0x0323, 0x0323, 0x30FB,
	0x0131, 0xF6BE, 0x0284, 0x22C5, 0x25CC, 0xFB1F, 0xFB1F, 0x031E, 0x02D5, 0x249F, 0xF6EB, 0x0256,
	0x018C, 0x3065, 0x30C5, 0x01F3, 0x02A3, 0x01C6, 0x02A5, 0x04E1, 0x0455, 0x045F, 0x0065, 0x00E9,
	0x2641, 0x098F, 0x311C, 0x0115, 0x090D, 0x0A8D, 0x0945, 0x0AC5, 0x011B, 0x1E1D, 0x0565, 0x0587,
	0x24D4, 0x00EA, 0x1EBF, 0x1E19, 0x1EC7, 0x1EC1, 0x1EC3, 0x1EC5, 0x0454, 0x0205, 0x090F, 0x00EB,
	0x0117, 0x0117, 0x1EB9, 0x0A0F, 0x0A47, 0x0444, 0x00E8, 0x0A8F, 0x0567, 0x311D, 0x3048, 0x1EBB,
	0x311F, 0x0038, 0x0668, 0x09EE, 0x2467, 0x2791, 0x096E, 0x2471, 0x2485, 0x2499, 0x0AEE, 0x0A6E,
	0x0668, 0x3028, 0x266B, 0x3227, 0x2088, 0xFF18, 0xF738, 0x247B, 0x248F, 0x06F8, 0x2177, 0x2078,
	0x0E58, 0x0207, 0x0465, 0x30A8, 0xFF74, 0x0A74, 0x3154, 0x043B, 0x2208, 0x246A, 0x247E, 0x2492,
	0x217A, 0x2026, 0x22EE, 0x0113, 0x1E17, 0x1E15, 0x043C, 0x2014, 0xFE31, 0xFF45, 0x055B, 0x2205,
	0x3123, 0x043D, 0x2013, 0xFE32, 0x04A3, 0x014B, 0x3125, 0x04A5, 0x04C8, 0x2002, 0x0119, 0x3153,
	0x025B, 0x029A, 0x025C, 0x025E, 0x025D, 0x24A0, 0x03B5, 0x03AD, 0x003D, 0xFF1D, 0xFE66, 0x207C,
	0x2261, 0x3126, 0x0440, 0x0258, 0x044D, 0x0441, 0x04AB, 0x0283, 0x0286, 0x090E, 0x0946, 0x01AA,
	0x0285, 0x3047, 0x30A7, 0xFF6A, 0x212E, 0xF6EC, 0x03B7, 0x0568, 0x03AE, 0x00F0, 0x1EBD, 0x1E1B,
	0x0591, 0x0591, 0x0591, 0x0591, 0x01DD, 0x3161, 0x20AC, 0x09C7, 0x0947, 0x0AC7, 0x0021, 0x055C,
	0x203C, 0x00A1, 0xF7A1, 0xFF01, 0xF721, 0x2203, 0x0292, 0x01EF, 0x0293, 0x01B9, 0x01BA, 0x0066,
	0x095E, 0x0A5E, 0x2109, 0x064E, 0x064E, 0x064B, 0x3108, 0x24D5, 0x1E1F, 0x0641, 0x0586, 0xFED2,
	0xFED3, 0xFED4, 0x03E5, 0x2640, 0xFB00, 0xFB03, 0xFB04, 0xFB01, 0x246E, 0x2482, 0x2496, 0x2012,
	0x25A0, 0x25AC, 0x05DA, 0xFB3A, 0xFB3A, 0x05DA, 0x05DD, 0x05DD, 0x05DF, 0x05DF, 0x05E3, 0x05E3,
	0x05E5, 0x05E5, 0x02C9, 0x25C9, 0x0473, 0x0035, 0x0665, 0x09EB, 0x2464, 0x278E, 0x096B, 0x215D,
	0x0AEB, 0x0A6B, 0x0665, 0x3025, 0x3224, 0x2085, 0xFF15, 0xF735, 0x2478, 0x248C, 0x06F5, 0x2174,
	0x2075, 0x0E55, 0xFB02, 0x0192, 0xFF46, 0x3399, 0x0E1F, 0x0E1D, 0x0E4F, 0x2200, 0x0034, 0x0664,
	0x09EA, 0x2463, 0x278D, 0x096A, 0x0AEA, 0x0A6A, 0x0664, 0x3024, 0x3223, 0x2084, 0xFF14, 0x09F7,
	0xF734, 0x2477, 0x248B, 0x06F4, 0x2173, 0x2074, 0x246D, 0x2481, 0x2495, 0x0E54, 0x02CB, 0x24A1,
	0x2044, 0x20A3, 0x0067, 0x0997, 0x01F5, 0x0917, 0x06AF, 0xFB93, 0xFB94, 0xFB95, 0x0A97, 0x0A17,
	0x304C, 0x30AC, 0x03B3, 0x0263, 0x02E0, 0x03EB, 0x310D, 0x011F, 0x01E7, 0x0123, 0x24D6, 0x011D,
	0x0123, 0x0121, 0x0121, 0x0433, 0x3052, 0x30B2, 0x2251, 0x059C, 0x05F3, 0x059D, 0x00DF, 0x059E,
	0x05F4, 0x3013, 0x0998, 0x0572, 0x0918, 0x0A98, 0x0A18, 0x063A, 0xFECE, 0xFECF, 0xFED0, 0x0495,
	0x0493, 0x0491, 0x095A, 0x0A5A, 0x0260, 0x3393, 0x304E, 0x30AE, 0x0563, 0x05D2, 0xFB32, 0xFB32,
	0x05D2, 0x0453, 0x01BE, 0x0294, 0x0296, 0x02C0, 0x0295, 0x02C1, 0x02E4, 0x02A1, 0x02A2, 0x1E21,
	0xFF47, 0x3054, 0x30B4, 0x24A2, 0x33AC, 0x2207, 0x0060, 0x0316, 0x0300, 0x0300, 0x0953, 0x02CE,
	0xFF40, 0x0340, 0x003E, 0x2265, 0x22DB, 0xFF1E, 0x2273, 0x2277, 0x2267, 0xFE65, 0x0261, 0x01E5,
	0x3050, 0x00AB, 0x00BB, 0x2039, 0x203A, 0x30B0, 0x3318, 0x33C9, 0x0068, 0x04A9, 0x06C1, 0x09B9,
	0x04B3, 0x0939, 0x0AB9, 0x0A39, 0x062D, 0xFEA2, 0xFEA3, 0x306F, 0xFEA4, 0x332A, 0x30CF, 0xFF8A,
	0x0A4D, 0x0621, 0x0621, 0x3164, 0x044A, 0x21BC, 0x21C0, 0x33CA, 0x05B2, 0x05B2, 0x05B2, 0x05B2,
	0x05B2, 0x05B2, 0x05B2, 0x05B2, 0x05B3, 0x05B3, 0x05B3, 0x05B3, 0x05B3, 0x05B3, 0x05B3, 0x05B3,
	0x05B1, 0x05B1, 0x05B1, 0x05B1, 0x05B1, 0x05B1, 0x05B1, 0x05B1, 0x0127, 0x310F, 0x1E2B, 0x1E29,
	0x24D7, 0x0125, 0x1E27, 0x1E23, 0x1E25, 0x05D4, 0x2665, 0x2665, 0x2661, 0xFB34, 0xFB34, 0x06C1,
	0x0647, 0x05D4, 0xFBA7, 0xFEEA, 0xFEEA, 0xFBA5, 0xFBA4, 0xFBA8, 0xFEEB, 0x3078, 0xFBA9, 0xFEEC,
	0x337B, 0x30D8, 0xFF8D, 0x3336, 0x0267, 0x3339, 0x05D7, 0x05D7, 0x0266, 0x02B1, 0x327B, 0x321B,
	0x326D, 0x314E, 0x320D, 0x3072, 0x30D2, 0xFF8B, 0x05B4, 0x05B4, 0x05B4, 0x05B4, 0x05B4, 0x05B4,
	0x05B4, 0x05B4, 0x1E96, 0xFF48, 0x0570, 0x0E2B, 0x307B, 0x30DB, 0xFF8E, 0x05B9, 0x05B9, 0x05B9,
	0x05B9, 0x05B9, 0x05B9, 0x05B9, 0x05B9, 0x0E2E, 0x0309, 0x0309, 0x0321, 0x0322, 0x3342, 0x03E9,
	0x2015, 0x031B, 0x2668, 0x2302, 0x24A3, 0x02B0, 0x0265, 0x3075, 0x3333, 0x30D5, 0xFF8C, 0x02DD,
	0x030B, 0x0195, 0x002D, 0xF6E5, 0xFF0D, 0xFE63, 0xF6E6, 0x2010, 0x0069, 0x00ED, 0x044F, 0x0987,
	0x3127, 0x012D, 0x01D0, 0x24D8, 0x00EE, 0x0456, 0x0209, 0x328F, 0x328B, 0x323F, 0x323A, 0x32A5,
	0x3006, 0x3001, 0xFF64, 0x3237, 0x32A3, 0x322F, 0x323D, 0x329D, 0x3240, 0x3296, 0x3236, 0x322B,
	0x3232, 0x32A4, 0x3005, 0x3298, 0x3238, 0x32A7, 0x32A6, 0x32A9, 0x322E, 0x322A, 0x3234, 0x3002,
	0x329E, 0x3243, 0x3239, 0x323E, 0x32A8, 0x3299, 0x3242, 0x3233, 0x3000, 0x3235, 0x3231, 0x323B,
	0x3230, 0x323C, 0x322C, 0x322D, 0x3007, 0x328E, 0x328A, 0x3294, 0x3290, 0x328C, 0x328D, 0x0907,
	0x00EF, 0x1E2F, 0x04E5, 0x1ECB, 0x04D7, 0x0435, 0x3275, 0x3215, 0x3267, 0x3147, 0x3207, 0x00EC,
	0x0A87, 0x0A07, 0x3044, 0x1EC9, 0x0988, 0x0438, 0x0908, 0x0A88, 0x0A08, 0x0A40, 0x020B, 0x0439,
	0x09C0, 0x0940, 0x0AC0, 0x0133, 0x30A4, 0xFF72, 0x3163, 0x02DC, 0x05AC, 0x012B, 0x04E3, 0x2253,
	0x0A3F, 0xFF49, 0x2206, 0x221E, 0x056B, 0x222B, 0x2321, 0x2321, 0xF8F5, 0x2320, 0x2320, 0x2229,
	0x3305, 0x25D8, 0x25D9, 0x263B, 0x0451, 0x012F, 0x03B9, 0x03CA, 0x0390, 0x0269, 0x03AF, 0x24A4,
	0x0A72, 0x3043, 0x30A3, 0xFF68, 0x09FA, 0x0268, 0xF6ED, 0x309D, 0x30FD, 0x0129, 0x1E2D, 0x3129,
	0x044E, 0x09BF, 0x093F, 0x0ABF, 0x0475, 0x0477, 0x006A, 0x0571, 0x099C, 0x091C, 0x0A9C, 0x0A1C,
	0x3110, 0x01F0, 0x24D9, 0x0135, 0x029D, 0x025F, 0x0458, 0x062C, 0xFE9E, 0xFE9F, 0xFEA0, 0x0698,
	0xFB8B, 0x099D, 0x091D, 0x0A9D, 0x0A1D, 0x057B, 0x3004, 0xFF4A, 0x24A5, 0x02B2, 0x006B, 0x04A1,
	0x0995, 0x1E31, 0x043A, 0x049B, 0x0915, 0x05DB, 0x0643, 0xFB3B, 0xFB3B, 0xFEDA, 0x05DB, 0xFEDB,
	0xFEDC, 0xFB4D, 0x0A95, 0x0A15, 0x304B, 0x04C4, 0x30AB, 0xFF76, 0x03BA, 0x03F0, 0x3171, 0x3184,
	0x3178, 0x3179, 0x330D, 0x0640, 0x0640, 0x30F5, 0x3384, 0x0650, 0x064D, 0x049F, 0xFF70, 0x049D,
	0x310E, 0x3389, 0x01E9, 0x0137, 0x24DA, 0x0137, 0x1E33, 0x0584, 0x3051, 0x30B1, 0xFF79, 0x056F,
	0x30F6, 0x0138, 0x0996, 0x0445, 0x0916, 0x0A96, 0x0A16, 0x062E, 0xFEA6, 0xFEA7, 0xFEA8, 0x03E7,
	0x0959, 0x0A59, 0x3278, 0x3218, 0x326A, 0x314B, 0x320A, 0x0E02, 0x0E05, 0x0E03, 0x0E04, 0x0E5B,
	0x0199, 0x0E06, 0x3391, 0x304D, 0x30AD, 0xFF77, 0x3315, 0x3316, 0x3314, 0x326E, 0x320E, 0x3260,
	0x3131, 0x3200, 0x3133, 0x045C, 0x1E35, 0x3398, 0x33A6, 0xFF4B, 0x33A2, 0x3053, 0x33C0, 0x0E01,
	0x30B3, 0xFF7A, 0x331E, 0x0481, 0x327F, 0x0343, 0x24A6, 0x33AA, 0x046F, 0x33CF, 0x029E, 0x304F,
	0x30AF, 0xFF78, 0x33B8, 0x33BE, 0x006C, 0x09B2, 0x013A, 0x0932, 0x0AB2, 0x0A32, 0x0E45, 0xFEFC,
	0xFEF8, 0xFEF7, 0xFEFA, 0xFEF9, 0xFEFB, 0xFEF6, 0xFEF5, 0x0644, 0x03BB, 0x019B, 0x05DC, 0xFB3C,
	0xFB3C, 0x05DC, 0xFEDE, 0xFCCA, 0xFEDF, 0xFCC9, 0xFCCB, 0xFDF2, 0xFEE0, 0xFD88, 0xFCCC, 0x25EF,
	0x019A, 0x026C, 0x310C, 0x013E, 0x013C, 0x24DB, 0x1E3D, 0x013C, 0x0140, 0x0140, 0x1E37, 0x1E39,
	0x031A, 0x0318, 0x003C, 0x2264, 0x22DA, 0xFF1C, 0x2272, 0x2276, 0x2266, 0xFE64, 0x026E, 0x258C,
	0x026D, 0x20A4, 0x056C, 0x01C9, 0x0459, 0xF6C0, 0x0933, 0x0AB3, 0x1E3B, 0x0934, 0x09E1, 0x0961,
	0x09E3, 0x0963, 0x026B, 0xFF4C, 0x33D0, 0x0E2C, 0x2227, 0x00AC, 0x2310, 0x2228, 0x0E25, 0x017F,
	0xFE4E, 0x0332, 0xFE4D, 0x25CA, 0x24A7, 0x0142, 0x2113, 0xF6EE, 0x2591, 0x0E26, 0x098C, 0x090C,
	0x09E2, 0x0962, 0x33D3, 0x006D, 0x09AE, 0x00AF, 0x0331, 0x0304, 0x02CD, 0xFFE3, 0x1E3F, 0x092E,
	0x0AAE, 0x0A2E, 0x05A4, 0x05A4, 0x307E, 0xF895, 0xF894, 0x0E4B, 0xF893, 0xF88C, 0xF88B, 0x0E48,
	0xF88A, 0xF884, 0x0E31, 0xF889, 0x0E47, 0xF88F, 0xF88E, 0x0E49, 0xF88D, 0xF892, 0xF891, 0x0E4A,
	0xF890, 0x0E46, 0x30DE, 0xFF8F, 0x2642, 0x3347, 0x05BE, 0x2642, 0x05AF, 0x3383, 0x3107, 0x33D4,
	0x24DC, 0x33A5, 0x1E41, 0x1E43, 0x0645, 0xFEE2, 0xFEE3, 0xFEE4, 0xFCD1, 0xFC48, 0x334D, 0x3081,
	0x337E, 0x30E1, 0xFF92, 0x05DE, 0xFB3E, 0xFB3E, 0x05DE, 0x0574, 0x05A5, 0x05A6, 0x05A6, 0x05A5,
	0x0271, 0x3392, 0xFF65, 0x00B7, 0x3272, 0x3212, 0x3264, 0x3141, 0x3170, 0x3204, 0x316E, 0x316F,
	0x307F, 0x30DF, 0xFF90, 0x2212, 0x0320, 0x2296, 0x02D7, 0x2213, 0x2032, 0x334A, 0x3349, 0x0270,
	0x3396, 0x33A3, 0xFF4D, 0x339F, 0x3082, 0x33C1, 0x30E2, 0xFF93, 0x33D6, 0x0E21, 0x33A7, 0x33A8,
	0x24A8, 0x33AB, 0x33B3, 0xF6EF, 0x026F, 0x00B5, 0x00B5, 0x3382, 0x226B, 0x226A, 0x338C, 0x03BC,
	0x338D, 0x3080, 0x30E0, 0xFF91, 0x3395, 0x00D7, 0x339B, 0x05A3, 0x05A3, 0x266A, 0x266B, 0x266D,
	0x266F, 0x33B2, 0x33B6, 0x33BC, 0x33B9, 0x33B7, 0x33BF, 0x33BD, 0x006E, 0x09A8, 0x2207, 0x0144,
	0x0928, 0x0AA8, 0x0A28, 0x306A, 0x30CA, 0xFF85, 0x0149, 0x3381, 0x310B, 0x00A0, 0x0148, 0x0146,
	0x24DD, 0x1E4B, 0x0146, 0x1E45, 0x1E47, 0x306D, 0x30CD, 0xFF88, 0x20AA, 0x338B, 0x0999, 0x0919,
	0x0A99, 0x0A19, 0x0E07, 0x3093, 0x0272, 0x0273, 0x326F, 0x320F, 0x3135, 0x3261, 0x3136, 0x3134,
	0x3168, 0x3201, 0x3167, 0x3166, 0x306B, 0x30CB, 0xFF86, 0xF899, 0x0E4D, 0x0039, 0x0669, 0x09EF,
	0x2468, 0x2792, 0x096F, 0x0AEF, 0x0A6F, 0x0669, 0x3029, 0x3228, 0x2089, 0xFF19, 0xF739, 0x247C,
	0x2490, 0x06F9, 0x2178, 0x2079, 0x2472, 0x2486, 0x249A, 0x0E59, 0x01CC, 0x045A, 0x30F3, 0xFF9D,
	0x019E, 0x1E49, 0xFF4E, 0x339A, 0x09A3, 0x0923, 0x0AA3, 0x0A23, 0x0929, 0x306E, 0x30CE, 0xFF89,
	0x00A0, 0x0E13, 0x0E19, 0x0646, 0xFEE6, 0x06BA, 0xFB9F, 0xFEE7, 0xFCD2, 0xFC4B, 0xFEE8, 0xFCD5,
	0xFC4E, 0xFC8D, 0x220C, 0x2209, 0x2209, 0x2260, 0x226F, 0x2271, 0x2279, 0x2262, 0x226E, 0x2270,
	0x2226, 0x2280, 0x2284, 0x2281, 0x2285, 0x0576, 0x24A9, 0x33B1, 0x207F, 0x00F1, 0x03BD, 0x306C,
	0x30CC, 0xFF87, 0x09BC, 0x093C, 0x0ABC, 0x0A3C, 0x0023, 0xFF03, 0xFE5F, 0x0374, 0x0375, 0x2116,
	0x05E0, 0xFB40, 0xFB40, 0x05E0, 0x33B5, 0x33BB, 0x099E, 0x091E, 0x0A9E, 0x0A1E, 0x006F, 0x00F3,
	0x0E2D, 0x0275, 0x04E9, 0x04EB, 0x0993, 0x311B, 0x014F, 0x0911, 0x0A91, 0x0949, 0x0AC9, 0x01D2,
	0x24DE, 0x00F4, 0x1ED1, 0x1ED9, 0x1ED3, 0x1ED5, 0x1ED7, 0x043E, 0x0151, 0x020D, 0x0913, 0x00F6,
	0x04E7, 0x1ECD, 0x0153, 0x315A, 0x02DB, 0x0328, 0x00F2, 0x0A93, 0x0585, 0x304A, 0x1ECF, 0x01A1,
	0x1EDB, 0x1EE3, 0x1EDD, 0x1EDF, 0x1EE1, 0x0151, 0x01A3, 0x020F, 0x30AA, 0xFF75, 0x3157, 0x05AB,
	0x014D, 0x1E53, 0x1E51, 0x0950, 0x03C9, 0x03D6, 0x0461, 0x0277, 0x047B, 0x047D, 0x03CE, 0x0AD0,
	0x03BF, 0x03CC, 0xFF4F, 0x0031, 0x0661, 0x09E7, 0x2460, 0x278A, 0x0967, 0x2024, 0x215B, 0xF6DC,
	0x0AE7, 0x0A67, 0x0661, 0x00BD, 0x3021, 0x3220, 0x2081, 0xFF11, 0x09F4, 0xF731, 0x2474, 0x2488,
	0x06F1, 0x00BC, 0x2170, 0x00B9, 0x0E51, 0x2153, 0x01EB, 0x01ED, 0x0A13, 0x0A4B, 0x0254, 0x24AA,
	0x25E6, 0x2325, 0x00AA, 0x00BA, 0x221F, 0x0912, 0x094A, 0x00F8, 0x01FF, 0x3049, 0x30A9, 0xFF6B,
	0x01FF, 0xF6F0, 0x047F, 0x00F5, 0x1E4D, 0x1E4F, 0x3121, 0x203E, 0xFE4A, 0x0305, 0xFE49, 0xFE4C,
	0xFE4B, 0x00AF, 0x09CB, 0x094B, 0x0ACB, 0x0070, 0x3380, 0x332B, 0x09AA, 0x1E55, 0x092A, 0x21DF,
	0x21DE, 0x0AAA, 0x0A2A, 0x3071, 0x0E2F, 0x30D1, 0x0484, 0x04C0, 0x317F, 0x00B6, 0x2225, 0x0028,
	0xFD3E, 0xF8ED, 0xF8EC, 0x208D, 0xFF08, 0xFE59, 0x207D, 0xF8EB, 0xFE35, 0x0029, 0xFD3F, 0xF8F8,
	0xF8F7, 0x208E, 0xFF09, 0xFE5A, 0x207E, 0xF8F6, 0xFE36, 0x2202, 0x05C0, 0x0599, 0x33A9, 0x05B7,
	0x05B7, 0x05B7, 0x05B7, 0x05B7, 0x05B7, 0x05B7, 0x05B7, 0x05A1, 0x3106, 0x24DF, 0x1E57, 0x05E4,
	0x043F, 0xFB44, 0xFB44, 0x333B, 0xFB43, 0x067E, 0x057A, 0x05E4, 0xFB57, 0xFB58, 0x307A, 0xFB59,
	0x30DA, 0x04A7, 0xFB4E, 0x0025, 0x066A, 0xFF05, 0xFE6A, 0x002E, 0x0589, 0x00B7, 0xFF61, 0xF6E7,
	0xFF0E, 0xFE52, 0xF6E8, 0x0342, 0x22A5, 0x2030, 0x20A7, 0x338A, 0x09AB, 0x092B, 0x0AAB, 0x0A2B,
	0x03C6, 0x03D5, 0x327A, 0x321A, 0x326C, 0x314D, 0x320C, 0x0278, 0x0E3A, 0x03D5, 0x01A5, 0x0E1E,
	0x0E1C, 0x0E20, 0x03C0, 0x3273, 0x3213, 0x3176, 0x3265, 0x3172, 0x3142, 0x3205, 0x3174, 0x3144,
	0x3175, 0x3177, 0x3173, 0x3074, 0x30D4, 0x03D6, 0x0583, 0x002B, 0x031F, 0x2295, 0x00B1, 0x02D6,
	0xFF0B, 0xFE62, 0x207A, 0xFF50, 0x33D8, 0x307D, 0x261F, 0x261C, 0x261E, 0x261D, 0x30DD, 0x0E1B,
	0x3012, 0x3020, 0x24AB, 0x227A, 0x211E, 0x02B9, 0x2035, 0x220F, 0x2305, 0x30FC, 0x2318, 0x2282,
	0x2283, 0x2237, 0x221D, 0x03C8, 0x0471, 0x0486, 0x33B0, 0x3077, 0x30D7, 0x33B4, 0x33BA, 0x0071,
	0x0958, 0x05A8, 0x0642, 0xFED6, 0xFED7, 0xFED8, 0x05B8, 0x05B8, 0x05B8, 0x05B8, 0x05B8, 0x05B8,
	0x05B8, 0x05B8, 0x05B8, 0x05B8, 0x05B8, 0x05B8, 0x05B8, 0x05B8, 0x05B8, 0x05B8, 0x059F, 0x3111,
	0x24E0, 0x02A0, 0xFF51, 0x05E7, 0xFB47, 0xFB47, 0x05E7, 0x24AC, 0x2669, 0x05BB, 0x05BB, 0x05BB,
	0x05BB, 0x05BB, 0x05BB, 0x05BB, 0x05BB, 0x003F, 0x061F, 0x055E, 0x00BF, 0xF7BF, 0x037E, 0xFF1F,
	0xF73F, 0x0022, 0x201E, 0x201C, 0xFF02, 0x301E, 0x301D, 0x201D, 0x2018, 0x201B, 0x201B, 0x2019,
	0x0149, 0x201A, 0x0027, 0xFF07, 0x0072, 0x057C, 0x09B0, 0x0155, 0x0930, 0x221A, 0xF8E5, 0x33AE,
	0x33AF, 0x33AD, 0x05BF, 0x05BF, 0x0AB0, 0x0A30, 0x3089, 0x30E9, 0xFF97, 0x09F1, 0x09F0, 0x0264,
	0x2236, 0x3116, 0x0159, 0x0157, 0x24E1, 0x0157, 0x0211, 0x1E59, 0x1E5B, 0x1E5D, 0x203B, 0x2286,
	0x2287, 0x00AE, 0xF8E8, 0xF6DA, 0x0631, 0x0580, 0xFEAE, 0x308C, 0x30EC, 0xFF9A, 0x05E8, 0xFB48,
	0x05E8, 0x223D, 0x0597, 0x0597, 0x2310, 0x027E, 0x027F, 0x09DD, 0x095D, 0x03C1, 0x027D, 0x027B,
	0x02B5, 0x03F1, 0x02DE, 0x3271, 0x3211, 0x3263, 0x3140, 0x313A, 0x3169, 0x3139, 0x313B, 0x316C,
	0x3203, 0x313F, 0x313C, 0x316B, 0x313D, 0x313E, 0x316A, 0x316D, 0x221F, 0x0319, 0x22BF, 0x308A,
	0x30EA, 0xFF98, 0x02DA, 0x0325, 0x030A, 0x02BF, 0x0559, 0x031C, 0x02D3, 0x02BE, 0x0339, 0x02D2,
	0x0213, 0x3351, 0x1E5F, 0x027C, 0x027A, 0xFF52, 0x308D, 0x30ED, 0xFF9B, 0x0E23, 0x24AD, 0x09DC,
	0x0931, 0x0A5C, 0x0691, 0xFB8D, 0x09E0, 0x0960, 0x0AE0, 0x09C4, 0x0944, 0x0AC4, 0xF6F1, 0x2590,
	0x0279, 0x02B4, 0x308B, 0x30EB, 0xFF99, 0x09F2, 0x09F3, 0xF6DD, 0x0E24, 0x098B, 0x090B, 0x0A8B,
	0x09C3, 0x0943, 0x0AC3, 0x0073, 0x09B8, 0x015B, 0x1E65, 0x0635, 0x0938, 0xFEBA, 0xFEBB, 0xFEBC,
	0x0AB8, 0x0A38, 0x3055, 0x30B5, 0xFF7B, 0xFDFA, 0x05E1, 0xFB41, 0xFB41, 0x05E1, 0x0E32, 0x0E41,
	0x0E44, 0x0E43, 0x0E33, 0x0E30, 0x0E40, 0xF886, 0x0E35, 0xF885, 0x0E34, 0x0E42, 0xF888, 0x0E37,
	0xF887, 0x0E36, 0x0E38, 0x0E39, 0x3119, 0x0161, 0x1E67, 0x015F, 0x0259, 0x04D9, 0x04DB, 0x025A,
	0x24E2, 0x015D, 0x0219, 0x1E61, 0x1E63, 0x1E69, 0x033C, 0x2033, 0x02CA, 0x00A7, 0x0633, 0xFEB2,
	0xFEB3, 0xFEB4, 0x05B6, 0x05B6, 0x05B6, 0x05B6, 0x05B6, 0x05B6, 0x05B6, 0x0592, 0x05B6, 0x057D,
	0x305B, 0x30BB, 0xFF7E, 0x003B, 0x061B, 0xFF1B, 0xFE54, 0x309C, 0xFF9F, 0x3322, 0x3323, 0x0037,
	0x0667, 0x09ED, 0x2466, 0x2790, 0x096D, 0x215E, 0x0AED, 0x0A6D, 0x0667, 0x3027, 0x3226, 0x2087,
	0xFF17, 0xF737, 0x247A, 0x248E, 0x06F7, 0x2176, 0x2077, 0x2470, 0x2484, 0x2498, 0x0E57, 0x00AD,
	0x0577, 0x09B6, 0x0448, 0x0651, 0xFC61, 0xFC5E, 0xFC60, 0xFC62, 0xFC5F, 0x2592, 0x2593, 0x2591,
	0x2592, 0x0936, 0x0AB6, 0x0A36, 0x0593, 0x3115, 0x0449, 0x0634, 0xFEB6, 0xFEB7, 0xFEB8, 0x03E3,
	0x20AA, 0x20AA, 0x05B0, 0x05B0, 0x05B0, 0x05B0, 0x05B0, 0x05B0, 0x05B0, 0x05B0, 0x05B0, 0x04BB,
	0x03ED, 0x05E9, 0xFB49, 0xFB49, 0xFB2C, 0xFB2C, 0xFB2D, 0xFB2D, 0x05C1, 0x05E9, 0xFB2A, 0xFB2A,
	0xFB2B, 0xFB2B, 0x0282, 0x03C3, 0x03C2, 0x03C2, 0x03F2, 0x3057, 0x30B7, 0xFF7C, 0x05BD, 0x05BD,
	0x223C, 0x05C2, 0x3274, 0x3214, 0x317E, 0x3266, 0x317A, 0x3145, 0x317B, 0x3206, 0x317D, 0x317C,
	0x0036, 0x0666, 0x09EC, 0x2465, 0x278F, 0x096C, 0x0AEC, 0x0A6C, 0x0666, 0x3026, 0x3225, 0x2086,
	0xFF16, 0xF736, 0x2479, 0x248D, 0x06F6, 0x2175, 0x2076, 0x246F, 0x09F9, 0x2483, 0x2497, 0x0E56,
	0x002F, 0xFF0F, 0x017F, 0x1E9B, 0x263A, 0xFF53, 0x05C3, 0x00AD, 0x044C, 0x305D, 0x30BD, 0xFF7F,
	0x0338, 0x0337, 0x0E29, 0x0E28, 0x0E0B, 0x0E2A, 0x0020, 0x0020, 0x2660, 0x2660, 0x2664, 0x24AE,
	0x033B, 0x33C4, 0x339D, 0x25A9, 0x25A4, 0x338F, 0x339E, 0x33CE, 0x33D1, 0x33D2, 0x338E, 0x33D5,
	0x339C, 0x33A1, 0x25A6, 0x25A7, 0x25A8, 0x25A5, 0x25A3, 0x33DB, 0x09B7, 0x0937, 0x0AB7, 0x3149,
	0x3185, 0x3180, 0x3132, 0x3165, 0x3143, 0x3146, 0x3138, 0xF6F2, 0x00A3, 0xFFE1, 0x0336, 0x0335,
	0x2282, 0x228A, 0x2286, 0x227B, 0x220B, 0x3059, 0x30B9, 0xFF7D, 0x0652, 0x2211, 0x263C, 0x2283,
	0x228B, 0x2287, 0x33DC, 0x337C, 0x0074, 0x09A4, 0x22A4, 0x22A3, 0x0924, 0x0AA4, 0x0A24, 0x0637,
	0xFEC2, 0xFEC3, 0x305F, 0xFEC4, 0x337D, 0x30BF, 0xFF80, 0x0640, 0x03C4, 0x05EA, 0xFB4A, 0xFB4A,
	0xFB4A, 0x05EA, 0x0167, 0x310A, 0x0165, 0x02A8, 0x0163, 0x0686, 0xFB7B, 0xFB7C, 0xFB7D, 0x24E3,
	0x1E71, 0x0163, 0x1E97, 0x1E6B, 0x1E6D, 0x0442, 0x04AD, 0x062A, 0xFE96, 0xFCA2, 0xFC0C, 0xFE97,
	0x3066, 0xFCA1, 0xFC0B, 0x0629, 0xFE94, 0xFE98, 0xFCA4, 0xFC0E, 0xFC73, 0x30C6, 0xFF83, 0x2121,
	0x260E, 0x05A0, 0x05A9, 0x2469, 0x3229, 0x247D, 0x2491, 0x2179, 0x02A7, 0x05D8, 0xFB38, 0xFB38,
	0x05D8, 0x04B5, 0x059B, 0x059B, 0x09A5, 0x0925, 0x0AA5, 0x0A25, 0x0630, 0xFEAC, 0xF898, 0xF897,
	0x0E4C, 0xF896, 0x062B, 0xFE9A, 0xFE9B, 0xFE9C, 0x2203, 0x2234, 0x03B8, 0x03D1, 0x03D1, 0x3279,
	0x3219, 0x326B, 0x314C, 0x320B, 0x246C, 0x2480, 0x2494, 0x0E11, 0x01AD, 0x0E12, 0x00FE, 0x0E17,
	0x0E10, 0x0E18, 0x0E16, 0x0482, 0x066C, 0x066C, 0x0033, 0x0663, 0x09E9, 0x2462, 0x278C, 0x0969,
	0x215C, 0x0AE9, 0x0A69, 0x0663, 0x3023, 0x3222, 0x2083, 0xFF13, 0x09F6, 0xF733, 0x2476, 0x248A,
	0x06F3, 0x00BE, 0xF6DE, 0x2172, 0x00B3, 0x0E53, 0x3394, 0x3061, 0x30C1, 0xFF81, 0x3270, 0x3210,
	0x3262, 0x3137, 0x3202, 0x02DC, 0x0330, 0x0303, 0x0303, 0x0360, 0x223C, 0x0334, 0x033E, 0x2297,
	0x0596, 0x0596, 0x0A70, 0x0483, 0x057F, 0x1E6F, 0xFF54, 0x0569, 0x3068, 0x30C8, 0xFF84, 0x02E5,
	0x02E9, 0x02E6, 0x02E8, 0x02E7, 0x01BD, 0x0185, 0x01A8, 0x0384, 0x3327, 0x0E0F, 0x3014, 0xFE5D,
	0xFE39, 0x3015, 0xFE5E, 0xFE3A, 0x0E15, 0x01AB, 0x24AF, 0x2122, 0xF8EA, 0xF6DB, 0x0288, 0x25BC,
	0x25C4, 0x25BA, 0x25B2, 0x02A6, 0x05E6, 0xFB46, 0xFB46, 0x05E6, 0x0446, 0x05B5, 0x05B5, 0x05B5,
	0x05B5, 0x05B5, 0x05B5, 0x05B5, 0x05B5, 0x045B, 0xF6F3, 0x099F, 0x091F, 0x0A9F, 0x0A1F, 0x0679,
	0xFB67, 0xFB68, 0xFB69, 0x09A0, 0x0920, 0x0AA0, 0x0A20, 0x0287, 0x3064, 0x30C4, 0xFF82, 0x3063,
	0x30C3, 0xFF6F, 0x246B, 0x247F, 0x2493, 0x217B, 0x2473, 0x5344, 0x2487, 0x249B, 0x0032, 0x0662,
	0x09E8, 0x2461, 0x278B, 0x0968, 0x2025, 0x2025, 0xFE30, 0x0AE8, 0x0A68, 0x0662, 0x3022, 0x3221,
	0x2082, 0xFF12, 0x09F5, 0xF732, 0x2475, 0x2489, 0x06F2, 0x2171, 0x01BB, 0x00B2, 0x0E52, 0x2154,
	0x0075, 0x00FA, 0x0289, 0x0989, 0x3128, 0x016D, 0x01D4, 0x24E4, 0x00FB, 0x1E77, 0x0443, 0x0951,
	0x0171, 0x0215, 0x0909, 0x00FC, 0x01D8, 0x1E73, 0x01DA, 0x04F1, 0x01DC, 0x01D6, 0x1EE5, 0x00F9,
	0x0A89, 0x0A09, 0x3046, 0x1EE7, 0x01B0, 0x1EE9, 0x1EF1, 0x1EEB, 0x1EED, 0x1EEF, 0x0171, 0x04F3,
	0x0217, 0x30A6, 0xFF73, 0x0479, 0x315C, 0x016B, 0x04EF, 0x1E7B, 0x0A41, 0xFF55, 0x005F, 0x2017,
	0xFF3F, 0xFE33, 0xFE4F, 0x222A, 0x2200, 0x0173, 0x24B0, 0x2580, 0x05C4, 0x03C5, 0x03CB, 0x03B0,
	0x028A, 0x03CD, 0x031D, 0x02D4, 0x0A73, 0x016F, 0x045E, 0x3045, 0x30A5, 0xFF69, 0x04AF, 0x04B1,
	0x0169, 0x1E79, 0x1E75, 0x098A, 0x090A, 0x0A8A, 0x0A0A, 0x0A42, 0x09C2, 0x0942, 0x0AC2, 0x09C1,
	0x0941, 0x0AC1, 0x0076, 0x0935, 0x0AB5, 0x0A35, 0x30F7, 0x05D5, 0xFB35, 0xFB35, 0xFB35, 0x05D5,
	0xFB4B, 0xFB4B, 0x05F0, 0x05F1, 0x24E5, 0x1E7F, 0x0432, 0x06A4, 0xFB6B, 0xFB6C, 0xFB6D, 0x30F9,
	0x2640, 0x007C, 0x030D, 0x0329, 0x02CC, 0x02C8, 0x057E, 0x028B, 0x30F8, 0x09CD, 0x094D, 0x0ACD,
	0x0983, 0x0903, 0x0A83, 0xFF56, 0x0578, 0x309E, 0x30FE, 0x309B, 0xFF9E, 0x30FA, 0x24B1, 0x1E7D,
	0x028C, 0x3094, 0x30F4, 0x0077, 0x1E83, 0x3159, 0x308F, 0x30EF, 0xFF9C, 0x3158, 0x308E, 0x30EE,
	0x3357, 0x301C, 0xFE34, 0x0648, 0xFEEE, 0x0624, 0xFE86, 0x33DD, 0x24E6, 0x0175, 0x1E85, 0x1E87,
	0x1E89, 0x3091, 0x2118, 0x30F1, 0x315E, 0x315D, 0x1E81, 0x25E6, 0x25CB, 0x25D9, 0x300E, 0xFE43,
	0x300F, 0xFE44, 0x25C7, 0x25C8, 0x25BF, 0x25BD, 0x25C3, 0x25C1, 0x3016, 0x3017, 0x25B9, 0x25B7,
	0x25AB, 0x263A, 0x25A1, 0x2606, 0x260F, 0x3018, 0x3019, 0x25B5, 0x25B3, 0x3090, 0x30F0, 0x315F,
	0xFF57, 0x3092, 0x30F2, 0xFF66, 0x20A9, 0xFFE6, 0x0E27, 0x24B2, 0x1E98, 0x02B7, 0x028D, 0x01BF,
	0x0078, 0x033D, 0x3112, 0x24E7, 0x1E8D, 0x1E8B, 0x056D, 0x03BE, 0xFF58, 0x24B3, 0x02E3, 0x0079,
	0x334E, 0x09AF, 0x00FD, 0x092F, 0x3152, 0x0AAF, 0x0A2F, 0x3084, 0x30E4, 0xFF94, 0x3151, 0x0E4E,
	0x3083, 0x30E3, 0xFF6C, 0x0463, 0x24E8, 0x0177, 0x00FF, 0x1E8F, 0x1EF5, 0x064A, 0x06D2, 0xFBAF,
	0xFEF2, 0x0626, 0xFE8A, 0xFE8B, 0xFE8C, 0xFEF3, 0xFEF4, 0xFCDD, 0xFC58, 0xFC94, 0x06D1, 0x3156,
	0x00A5, 0xFFE5, 0x3155, 0x3186, 0x05AA, 0x05AA, 0x044B, 0x04F9, 0x3181, 0x3183, 0x3182, 0x059A,
	0x1EF3, 0x01B4, 0x1EF7, 0x0575, 0x0457, 0x3162, 0x262F, 0x0582, 0xFF59, 0x05D9, 0xFB39, 0xFB39,
	0x05D9, 0x05F2, 0xFB1F, 0x3088, 0x3189, 0x30E8, 0xFF96, 0x315B, 0x3087, 0x30E7, 0xFF6E, 0x03F3,
	0x3188, 0x3187, 0x0E22, 0x0E0D, 0x24B4, 0x037A, 0x0345, 0x01A6, 0x1E99, 0x02B8, 0x1EF9, 0x028E,
	0x3086, 0x318C, 0x30E6, 0xFF95, 0x3160, 0x046B, 0x046D, 0x0467, 0x0469, 0x3085, 0x30E5, 0xFF6D,
	0x318B, 0x318A, 0x09DF, 0x095F, 0x007A, 0x0566, 0x017A, 0x095B, 0x0A5B, 0x0638, 0xFEC6, 0xFEC7,
	0x3056, 0xFEC8, 0x0632, 0xFEB0, 0x30B6, 0x0595, 0x0594, 0x0598, 0x05D6, 0xFB36, 0xFB36, 0x05D6,
	0x3117, 0x017E, 0x24E9, 0x1E91, 0x0291, 0x017C, 0x017C, 0x1E93, 0x0437, 0x0499, 0x04DF, 0x305C,
	0x30BC, 0x0030, 0x0660, 0x09E6, 0x0966, 0x0AE6, 0x0A66, 0x0660, 0x2080, 0xFF10, 0xF730, 0x06F0,
	0x2070, 0x0E50, 0xFEFF, 0x200C, 0x200B, 0x03B6, 0x3113, 0x056A, 0x04C2, 0x0436, 0x0497, 0x04DD,
	0x3058, 0x30B8, 0x05AE, 0x1E95, 0xFF5A, 0x305E, 0x30BE, 0x24B5, 0x0290, 0x01B6, 0x305A, 0x30BA,
};
static const unsigned short stdw_helv[224] = {
	278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278, 333, 278, 278,
	556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 278, 278, 584, 584, 584, 556,
	1015, 667, 667, 722, 722, 667, 611, 778, 722, 278, 500, 667, 556, 833, 722, 778,
	667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278, 278, 278, 469, 556,
	333, 556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833, 556, 556,
	556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584, 0,
	556, 0, 222, 556, 333, 1000, 556, 556, 333, 1000, 667, 333, 1000, 0, 611, 0,
	0, 222, 222, 333, 333, 350, 556, 1000, 333, 1000, 500, 333, 944, 0, 500, 667,
	0, 333, 556, 556, 556, 556, 260, 556, 333, 737, 370, 556, 584, 0, 737, 333,
	400, 584, 0, 0, 333, 556, 537, 278, 333, 0, 365, 556, 834, 834, 834, 611,
	667, 667, 667, 667, 667, 667, 1000, 722, 667, 667, 667, 667, 278, 278, 278, 278,
	722, 722, 778, 778, 778, 778, 778, 584, 778, 722, 722, 722, 722, 667, 667, 611,
	556, 556, 556, 556, 556, 556, 889, 500, 556, 556, 556, 556, 278, 278, 278, 278,
	556, 556, 556, 556, 556, 556, 556, 584, 611, 556, 556, 556, 556, 500, 556, 500,
};
static const unsigned short stdw_helvb[224] = {
	278, 333, 474, 556, 556, 889, 722, 238, 333, 333, 389, 584, 278, 333, 278, 278,
	556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 333, 333, 584, 584, 584, 611,
	975, 722, 722, 722, 722, 667, 611, 778, 722, 278, 556, 722, 611, 833, 722, 778,
	667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 333, 278, 333, 584, 556,
	333, 556, 611, 556, 611, 556, 333, 611, 611, 278, 278, 556, 278, 889, 611, 611,
	611, 611, 389, 556, 333, 611, 556, 778, 556, 556, 500, 389, 280, 389, 584, 0,
	556, 0, 278, 556, 500, 1000, 556, 556, 333, 1000, 667, 333, 1000, 0, 611, 0,
	0, 278, 278, 500, 500, 350, 556, 1000, 333, 1000, 556, 333, 944, 0, 500, 667,
	0, 333, 556, 556, 556, 556, 280, 556, 333, 737, 370, 556, 584, 0, 737, 333,
	400, 584, 0, 0, 333, 611, 556, 278, 333, 0, 365, 556, 834, 834, 834, 611,
	722, 722, 722, 722, 722, 722, 1000, 722, 667, 667, 667, 667, 278, 278, 278, 278,
	722, 722, 778, 778, 778, 778, 778, 584, 778, 722, 722, 722, 722, 667, 667, 611,
	556, 556, 556, 556, 556, 556, 889, 556, 556, 556, 556, 556, 278, 278, 278, 278,
	611, 611, 611, 611, 611, 611, 611, 584, 611, 611, 611, 611, 611, 556, 611, 556,
};
static const unsigned short stdw_times[224] = {
	250, 333, 408, 500, 500, 833, 778, 180, 333, 333, 500, 564, 250, 333, 250, 278,
	500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 278, 278, 564, 564, 564, 444,
	921, 722, 667, 667, 722, 611, 556, 722, 722, 333, 389, 722, 611, 889, 722, 722,
	556, 722, 667, 556, 611, 722, 722, 944, 722, 722, 611, 333, 278, 333, 469, 500,
	333, 444, 500, 444, 500, 444, 333, 500, 500, 278, 278, 500, 278, 778, 500, 500,
	500, 500, 333, 389, 278, 500, 500, 722, 500, 500, 444, 480, 200, 480, 541, 0,
	500, 0, 333, 500, 444, 1000, 500, 500, 333, 1000, 556, 333, 889, 0, 611, 0,
	0, 333, 333, 444, 444, 350, 500, 1000, 333, 980, 389, 333, 722, 0, 444, 722,
	0, 333, 500, 500, 500, 500, 200, 500, 333, 760, 276, 500, 564, 0, 760, 333,
	400, 564, 0, 0, 333, 500, 453, 250, 333, 0, 310, 500, 750, 750, 750, 444,
	722, 722, 722, 722, 722, 722, 889, 667, 611, 611, 611, 611, 333, 333, 333, 333,
	722, 722, 722, 722, 722, 722, 722, 564, 722, 722, 722, 722, 722, 722, 556, 500,
	444, 444, 444, 444, 444, 444, 667, 444, 444, 444, 444, 444, 278, 278, 278, 278,
	500, 500, 500, 500, 500, 500, 500, 564, 500, 500, 500, 500, 500, 500, 500, 500,
};
static const unsigned short stdw_timesb[224] = {
	250, 333, 555, 500, 500, 1000, 833, 278, 333, 333, 500, 570, 250, 333, 250, 278,
	500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 333, 333, 570, 570, 570, 500,
	930, 722, 667, 722, 722, 667, 611, 778, 778, 389, 500, 778, 667, 944, 722, 778,
	611, 778, 722, 556, 667, 722, 722, 1000, 722, 722, 667, 333, 278, 333, 581, 500,
	333, 500, 556, 444, 556, 444, 333, 500, 556, 278, 333, 556, 278, 833, 556, 500,
	556, 556, 444, 389, 333, 556, 500, 722, 500, 500, 444, 394, 220, 394, 520, 0,
	500, 0, 333, 500, 500, 1000, 500, 500, 333, 1000, 556, 333, 1000, 0, 667, 0,
	0, 333, 333, 500, 500, 350, 500, 1000, 333, 1000, 389, 333, 722, 0, 444, 722,
	0, 333, 500, 500, 500, 500, 220, 500, 333, 747, 300, 500, 570, 0, 747, 333,
	400, 570, 0, 0, 333, 556, 540, 250, 333, 0, 330, 500, 750, 750, 750, 500,
	722, 722, 722, 722, 722, 722, 1000, 722, 667, 667, 667, 667, 389, 389, 389, 389,
	722, 722, 778, 778, 778, 778, 778, 570, 778, 722, 722, 722, 722, 722, 611, 556,
	500, 500, 500, 500, 500, 500, 722, 444, 444, 444, 444, 444, 278, 278, 278, 278,
	500, 556, 500, 500, 500, 500, 500, 570, 500, 556, 556, 556, 556, 500, 556, 500,
};
static const unsigned short stdw_timesi[224] = {
	250, 333, 420, 500, 500, 833, 778, 214, 333, 333, 500, 675, 250, 333, 250, 278,
	500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 333, 333, 675, 675, 675, 500,
	920, 611, 611, 667, 722, 611, 611, 722, 722, 333, 444, 667, 556, 833, 667, 722,
	611, 722, 611, 500, 556, 722, 611, 833, 611, 556, 556, 389, 278, 389, 422, 500,
	333, 500, 500, 444, 500, 444, 278, 500, 500, 278, 278, 444, 278, 722, 500, 500,
	500, 500, 389, 389, 278, 500, 444, 667, 444, 444, 389, 400, 275, 400, 541, 0,
	500, 0, 333, 500, 556, 889, 500, 500, 333, 1000, 500, 333, 944, 0, 556, 0,
	0, 333, 333, 556, 556, 350, 500, 889, 333, 980, 389, 333, 667, 0, 389, 556,
	0, 389, 500, 500, 500, 500, 275, 500, 333, 760, 276, 500, 675, 0, 760, 333,
	400, 675, 0, 0, 333, 500, 523, 250, 333, 0, 310, 500, 750, 750, 750, 500,
	611, 611, 611, 611, 611, 611, 889, 667, 611, 611, 611, 611, 333, 333, 333, 333,
	722, 667, 722, 722, 722, 722, 722, 675, 722, 722, 722, 722, 722, 556, 611, 500,
	500, 500, 500, 500, 500, 500, 667, 444, 444, 444, 444, 444, 278, 278, 278, 278,
	500, 500, 500, 500, 500, 500, 500, 675, 500, 500, 500, 500, 500, 444, 500, 444,
};
static const unsigned short stdw_timesbi[224] = {
	250, 389, 555, 500, 500, 833, 778, 278, 333, 333, 500, 570, 250, 333, 250, 278,
	500, 500, 500, 500, 500, 500, 500, 500, 500, 500, 333, 333, 570, 570, 570, 500,
	832, 667, 667, 667, 722, 667, 667, 722, 778, 389, 500, 667, 611, 889, 722, 722,
	611, 722, 667, 556, 611, 722, 667, 889, 667, 611, 611, 333, 278, 333, 570, 500,
	333, 500, 500, 444, 500, 444, 333, 500, 556, 278, 278, 500, 278, 778, 556, 500,
	500, 500, 389, 389, 278, 556, 444, 667, 500, 444, 389, 348, 220, 348, 570, 0,
	500, 0, 333, 500, 500, 1000, 500, 500, 333, 1000, 556, 333, 944, 0, 611, 0,
	0, 333, 333, 500, 500, 350, 500, 1000, 333, 1000, 389, 333, 722, 0, 389, 611,
	0, 389, 500, 500, 500, 500, 220, 500, 333, 747, 266, 500, 606, 0, 747, 333,
	400, 570, 0, 0, 333, 576, 500, 250, 333, 0, 300, 500, 750, 750, 750, 500,
	667, 667, 667, 667, 667, 667, 944, 667, 667, 667, 667, 667, 389, 389, 389, 389,
	722, 722, 722, 722, 722, 722, 722, 570, 722, 722, 722, 722, 722, 611, 611, 500,
	500, 500, 500, 500, 500, 500, 722, 444, 444, 444, 444, 444, 278, 278, 278, 278,
	500, 556, 500, 500, 500, 500, 500, 570, 500, 556, 556, 556, 556, 444, 500, 444,
};

static const char *const cff_std[391] = {
	".notdef", "space", "exclam", "quotedbl", "numbersign", "dollar",
	"percent", "ampersand", "quoteright", "parenleft", "parenright", "asterisk",
	"plus", "comma", "hyphen", "period", "slash", "zero",
	"one", "two", "three", "four", "five", "six",
	"seven", "eight", "nine", "colon", "semicolon", "less",
	"equal", "greater", "question", "at", "A", "B",
	"C", "D", "E", "F", "G", "H",
	"I", "J", "K", "L", "M", "N",
	"O", "P", "Q", "R", "S", "T",
	"U", "V", "W", "X", "Y", "Z",
	"bracketleft", "backslash", "bracketright", "asciicircum", "underscore", "quoteleft",
	"a", "b", "c", "d", "e", "f",
	"g", "h", "i", "j", "k", "l",
	"m", "n", "o", "p", "q", "r",
	"s", "t", "u", "v", "w", "x",
	"y", "z", "braceleft", "bar", "braceright", "asciitilde",
	"exclamdown", "cent", "sterling", "fraction", "yen", "florin",
	"section", "currency", "quotesingle", "quotedblleft", "guillemotleft", "guilsinglleft",
	"guilsinglright", "fi", "fl", "endash", "dagger", "daggerdbl",
	"periodcentered", "paragraph", "bullet", "quotesinglbase", "quotedblbase", "quotedblright",
	"guillemotright", "ellipsis", "perthousand", "questiondown", "grave", "acute",
	"circumflex", "tilde", "macron", "breve", "dotaccent", "dieresis",
	"ring", "cedilla", "hungarumlaut", "ogonek", "caron", "emdash",
	"AE", "ordfeminine", "Lslash", "Oslash", "OE", "ordmasculine",
	"ae", "dotlessi", "lslash", "oslash", "oe", "germandbls",
	"onesuperior", "logicalnot", "mu", "trademark", "Eth", "onehalf",
	"plusminus", "Thorn", "onequarter", "divide", "brokenbar", "degree",
	"thorn", "threequarters", "twosuperior", "registered", "minus", "eth",
	"multiply", "threesuperior", "copyright", "Aacute", "Acircumflex", "Adieresis",
	"Agrave", "Aring", "Atilde", "Ccedilla", "Eacute", "Ecircumflex",
	"Edieresis", "Egrave", "Iacute", "Icircumflex", "Idieresis", "Igrave",
	"Ntilde", "Oacute", "Ocircumflex", "Odieresis", "Ograve", "Otilde",
	"Scaron", "Uacute", "Ucircumflex", "Udieresis", "Ugrave", "Yacute",
	"Ydieresis", "Zcaron", "aacute", "acircumflex", "adieresis", "agrave",
	"aring", "atilde", "ccedilla", "eacute", "ecircumflex", "edieresis",
	"egrave", "iacute", "icircumflex", "idieresis", "igrave", "ntilde",
	"oacute", "ocircumflex", "odieresis", "ograve", "otilde", "scaron",
	"uacute", "ucircumflex", "udieresis", "ugrave", "yacute", "ydieresis",
	"zcaron", "exclamsmall", "Hungarumlautsmall", "dollaroldstyle", "dollarsuperior", "ampersandsmall",
	"Acutesmall", "parenleftsuperior", "parenrightsuperior", "twodotenleader", "onedotenleader", "zerooldstyle",
	"oneoldstyle", "twooldstyle", "threeoldstyle", "fouroldstyle", "fiveoldstyle", "sixoldstyle",
	"sevenoldstyle", "eightoldstyle", "nineoldstyle", "commasuperior", "threequartersemdash", "periodsuperior",
	"questionsmall", "asuperior", "bsuperior", "centsuperior", "dsuperior", "esuperior",
	"isuperior", "lsuperior", "msuperior", "nsuperior", "osuperior", "rsuperior",
	"ssuperior", "tsuperior", "ff", "ffi", "ffl", "parenleftinferior",
	"parenrightinferior", "Circumflexsmall", "hyphensuperior", "Gravesmall", "Asmall", "Bsmall",
	"Csmall", "Dsmall", "Esmall", "Fsmall", "Gsmall", "Hsmall",
	"Ismall", "Jsmall", "Ksmall", "Lsmall", "Msmall", "Nsmall",
	"Osmall", "Psmall", "Qsmall", "Rsmall", "Ssmall", "Tsmall",
	"Usmall", "Vsmall", "Wsmall", "Xsmall", "Ysmall", "Zsmall",
	"colonmonetary", "onefitted", "rupiah", "Tildesmall", "exclamdownsmall", "centoldstyle",
	"Lslashsmall", "Scaronsmall", "Zcaronsmall", "Dieresissmall", "Brevesmall", "Caronsmall",
	"Dotaccentsmall", "Macronsmall", "figuredash", "hypheninferior", "Ogoneksmall", "Ringsmall",
	"Cedillasmall", "questiondownsmall", "oneeighth", "threeeighths", "fiveeighths", "seveneighths",
	"onethird", "twothirds", "zerosuperior", "foursuperior", "fivesuperior", "sixsuperior",
	"sevensuperior", "eightsuperior", "ninesuperior", "zeroinferior", "oneinferior", "twoinferior",
	"threeinferior", "fourinferior", "fiveinferior", "sixinferior", "seveninferior", "eightinferior",
	"nineinferior", "centinferior", "dollarinferior", "periodinferior", "commainferior", "Agravesmall",
	"Aacutesmall", "Acircumflexsmall", "Atildesmall", "Adieresissmall", "Aringsmall", "AEsmall",
	"Ccedillasmall", "Egravesmall", "Eacutesmall", "Ecircumflexsmall", "Edieresissmall", "Igravesmall",
	"Iacutesmall", "Icircumflexsmall", "Idieresissmall", "Ethsmall", "Ntildesmall", "Ogravesmall",
	"Oacutesmall", "Ocircumflexsmall", "Otildesmall", "Odieresissmall", "OEsmall", "Oslashsmall",
	"Ugravesmall", "Uacutesmall", "Ucircumflexsmall", "Udieresissmall", "Yacutesmall", "Thornsmall",
	"Ydieresissmall", "001.000", "001.001", "001.002", "001.003", "Black",
	"Bold", "Book", "Light", "Medium", "Regular", "Roman",
	"Semibold",
};

#endif

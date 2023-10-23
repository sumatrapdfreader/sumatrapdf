// Copyright (C) 2004-2022 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"
#include "mupdf/ucdn.h"

#include <string.h>

/*
	Base 14 PDF fonts from URW.
	Noto fonts from Google.
	Source Han Serif from Adobe for CJK.
	DroidSansFallback from Android for CJK.
	Charis SIL from SIL.

	Define TOFU to only include the Base14 and CJK fonts.

	Define TOFU_CJK_LANG to skip Source Han Serif per-language fonts.
	Define TOFU_CJK_EXT to skip DroidSansFallbackFull (and the above).
	Define TOFU_CJK to skip DroidSansFallback (and the above).

	Define TOFU_NOTO to skip ALL non-CJK noto fonts.
	Define TOFU_SYMBOL to skip symbol fonts.
	Define TOFU_EMOJI to skip emoji/extended symbol font.

	Define TOFU_SIL to skip the SIL fonts (warning: makes EPUB documents ugly).
	Define TOFU_BASE14 to skip the Base 14 fonts (warning: makes PDF unusable).
*/

#ifdef NOTO_SMALL
#define TOFU_CJK_EXT
#define TOFU_SYMBOL
#define TOFU_EMOJI
#define TOFU_SIL
#endif

#ifdef NO_CJK
#define TOFU_CJK
#endif

#ifdef TOFU
#define TOFU_NOTO
#define TOFU_SIL
#endif

#ifdef TOFU_NOTO
#define TOFU_SYMBOL
#define TOFU_EMOJI
#endif

/* This historic script has an unusually large font (2MB), so we skip it by default. */
#ifndef NOTO_TANGUT
#define NOTO_TANGUT 0
#endif

/* Define some extra scripts for special fonts. */
enum
{
	MUPDF_SCRIPT_MUSIC = UCDN_LAST_SCRIPT+1,
	MUPDF_SCRIPT_MATH,
	MUPDF_SCRIPT_SYMBOLS,
	MUPDF_SCRIPT_SYMBOLS2,
	MUPDF_SCRIPT_EMOJI,
	MUPDF_SCRIPT_CJKV
};

enum
{
	BOLD = 1,
	ITALIC = 2
};

typedef struct
{
	const unsigned char *data;
#ifdef HAVE_OBJCOPY
	const unsigned char *start;
	const unsigned char *end;
#define INBUILT_SIZE(e) (e->end - e->start)
#else
	const unsigned int *size;
#define INBUILT_SIZE(e) (*e->size)
#endif
	char family[48];
	int script;
	int lang;
	int subfont;
	int attr;
} font_entry;

#define END_OF_DATA -2
#define ANY_SCRIPT -1
#define NO_SUBFONT 0
#define REGULAR 0

/* First, declare all the fonts. */
#ifdef HAVE_OBJCOPY
#define FONT(FORGE,NAME,NAME2,SCRIPT,LANG,SUBFONT,ATTR) \
extern const unsigned char _binary_resources_fonts_##FORGE##_##NAME##_start; \
extern const unsigned char _binary_resources_fonts_##FORGE##_##NAME##_end;
#else
#define FONT(FORGE,NAME,NAME2,SCRIPT,LANG,SUBFONT,ATTR) \
extern const unsigned char _binary_##NAME[];\
extern const unsigned int _binary_##NAME##_size;
#endif
#define ALIAS(FORGE,NAME,NAME2,SCRIPT,LANG,SUBFONT,ATTR)
#define EMPTY(SCRIPT)

#include "font-table.h"

#undef FONT
#undef ALIAS
#undef EMPTY

/* Now the actual list. */
#ifdef HAVE_OBJCOPY
#define FONT_DATA(FORGE,NAME) &_binary_resources_fonts_##FORGE##_##NAME##_start
#define FONT_SIZE(FORGE,NAME) &_binary_resources_fonts_##FORGE##_##NAME##_start, &_binary_resources_fonts_##FORGE##_##NAME##_end
#define EMPTY(SCRIPT) { NULL, NULL, NULL, "", SCRIPT, FZ_LANG_UNSET, NO_SUBFONT, REGULAR },
#else
#define FONT_DATA(FORGE,NAME) _binary_##NAME
#define FONT_SIZE(FORCE,NAME) &_binary_##NAME##_size
#define EMPTY(SCRIPT) { NULL, 0, "", SCRIPT, FZ_LANG_UNSET, NO_SUBFONT, REGULAR },
#endif

#define FONT(FORGE,NAME,NAME2,SCRIPT,LANG,SUBFONT,ATTR) { FONT_DATA(FORGE, NAME), FONT_SIZE(FORGE, NAME), NAME2, SCRIPT, LANG, SUBFONT, ATTR },
#define ALIAS(FORGE,NAME,NAME2,SCRIPT,LANG,SUBFONT,ATTR) { FONT_DATA(FORGE, NAME), FONT_SIZE(FORGE, NAME), NAME2, SCRIPT, LANG, SUBFONT, ATTR },
static font_entry inbuilt_fonts[] =
{
#include "font-table.h"
	{ NULL,
#ifdef HAVE_OBJCOPY
	NULL, NULL,
#else
	0,
#endif
	"", END_OF_DATA, FZ_LANG_UNSET, NO_SUBFONT, REGULAR }
};

#undef FONT
#undef ALIAS
#undef EMPTY
#undef FONT_DATA
#undef FONT_SIZE

static const unsigned char *
search_by_script_lang_strict(int *size, int *subfont, int script, int language)
{
	/* Search in the inbuilt font table. */
	font_entry *e;

	if (subfont)
		*subfont = 0;

	for (e = inbuilt_fonts; e->script != END_OF_DATA; e++)
	{
		if (script != ANY_SCRIPT && e->script != script)
			continue;
		if (e->lang != language)
			continue;
		*size = INBUILT_SIZE(e);
		if (subfont)
			*subfont = e->subfont;
		return e->data;
	}

	return *size = 0, NULL;
}

static const unsigned char *
search_by_script_lang(int *size, int *subfont, int script, int language)
{
	const unsigned char *result;
	result = search_by_script_lang_strict(size, subfont, script, language);
	if (!result && language != FZ_LANG_UNSET)
		result = search_by_script_lang_strict(size, subfont, script, FZ_LANG_UNSET);
	return result;
}

static const unsigned char *
search_by_family(int *size, const char *family, int attr)
{
	/* Search in the inbuilt font table. */
	font_entry *e;

	for (e = inbuilt_fonts; e->script != END_OF_DATA; e++)
	{
		if (e->family[0] == '\0')
			continue;
		if (attr != e->attr)
			continue;
		if (!fz_strcasecmp(e->family, family))
		{
			*size = INBUILT_SIZE(e);
			return e->data;
		}
	}

	return *size = 0, NULL;
}

const unsigned char *
fz_lookup_base14_font(fz_context *ctx, const char *name, int *size)
{
	/* We want to insist on the base14 name matching exactly,
	 * so we check that here first, before we look in the font table
	 * to see if we actually have data. */

	if (!strcmp(name, "Courier"))
		return search_by_family(size, "Courier", REGULAR);
	if (!strcmp(name, "Courier-Oblique"))
		return search_by_family(size, "Courier", ITALIC);
	if (!strcmp(name, "Courier-Bold"))
		return search_by_family(size, "Courier", BOLD);
	if (!strcmp(name, "Courier-BoldOblique"))
		return search_by_family(size, "Courier", BOLD|ITALIC);

	if (!strcmp(name, "Helvetica"))
		return search_by_family(size, "Helvetica", REGULAR);
	if (!strcmp(name, "Helvetica-Oblique"))
		return search_by_family(size, "Helvetica", ITALIC);
	if (!strcmp(name, "Helvetica-Bold"))
		return search_by_family(size, "Helvetica", BOLD);
	if (!strcmp(name, "Helvetica-BoldOblique"))
		return search_by_family(size, "Helvetica", BOLD|ITALIC);

	if (!strcmp(name, "Times-Roman"))
		return search_by_family(size, "Times", REGULAR);
	if (!strcmp(name, "Times-Italic"))
		return search_by_family(size, "Times", ITALIC);
	if (!strcmp(name, "Times-Bold"))
		return search_by_family(size, "Times", BOLD);
	if (!strcmp(name, "Times-BoldItalic"))
		return search_by_family(size, "Times", BOLD|ITALIC);

	if (!strcmp(name, "Symbol"))
		return search_by_family(size, "Symbol", REGULAR);
	if (!strcmp(name, "ZapfDingbats"))
		return search_by_family(size, "ZapfDingbats", REGULAR);

	*size = 0;
	return NULL;
}

const unsigned char *
fz_lookup_builtin_font(fz_context *ctx, const char *family, int is_bold, int is_italic, int *size)
{
	return search_by_family(size, family, (is_bold ? BOLD : 0) | (is_italic ? ITALIC : 0));
}

const unsigned char *
fz_lookup_cjk_font(fz_context *ctx, int ordering, int *size, int *subfont)
{
	int lang = FZ_LANG_UNSET;
	switch (ordering)
	{
	case FZ_ADOBE_JAPAN: lang = FZ_LANG_ja; break;
	case FZ_ADOBE_KOREA: lang = FZ_LANG_ko; break;
	case FZ_ADOBE_GB: lang = FZ_LANG_zh_Hans; break;
	case FZ_ADOBE_CNS: lang = FZ_LANG_zh_Hant; break;
	}
	return search_by_script_lang(size, subfont, UCDN_SCRIPT_HAN, lang);
}

int
fz_lookup_cjk_ordering_by_language(const char *lang)
{
	if (!strcmp(lang, "zh-Hant")) return FZ_ADOBE_CNS;
	if (!strcmp(lang, "zh-TW")) return FZ_ADOBE_CNS;
	if (!strcmp(lang, "zh-HK")) return FZ_ADOBE_CNS;
	if (!strcmp(lang, "zh-Hans")) return FZ_ADOBE_GB;
	if (!strcmp(lang, "zh-CN")) return FZ_ADOBE_GB;
	if (!strcmp(lang, "ja")) return FZ_ADOBE_JAPAN;
	if (!strcmp(lang, "ko")) return FZ_ADOBE_KOREA;
	return -1;
}

static int
fz_lookup_cjk_language(const char *lang)
{
	if (!strcmp(lang, "zh-Hant")) return FZ_LANG_zh_Hant;
	if (!strcmp(lang, "zh-TW")) return FZ_LANG_zh_Hant;
	if (!strcmp(lang, "zh-HK")) return FZ_LANG_zh_Hant;
	if (!strcmp(lang, "zh-Hans")) return FZ_LANG_zh_Hans;
	if (!strcmp(lang, "zh-CN")) return FZ_LANG_zh_Hans;
	if (!strcmp(lang, "ja")) return FZ_LANG_ja;
	if (!strcmp(lang, "ko")) return FZ_LANG_ko;
	return FZ_LANG_UNSET;
}

const unsigned char *
fz_lookup_cjk_font_by_language(fz_context *ctx, const char *lang, int *size, int *subfont)
{
	return search_by_script_lang(size, subfont, UCDN_SCRIPT_HAN, fz_lookup_cjk_language(lang));
}

const unsigned char *
fz_lookup_noto_font(fz_context *ctx, int script, int language, int *size, int *subfont)
{
	return search_by_script_lang(size, subfont, script, language);
}

const unsigned char *
fz_lookup_noto_math_font(fz_context *ctx, int *size)
{
	return search_by_script_lang(size, NULL, MUPDF_SCRIPT_MATH, FZ_LANG_UNSET);
}

const unsigned char *
fz_lookup_noto_music_font(fz_context *ctx, int *size)
{
	return search_by_script_lang(size, NULL, MUPDF_SCRIPT_MUSIC, FZ_LANG_UNSET);
}

const unsigned char *
fz_lookup_noto_symbol1_font(fz_context *ctx, int *size)
{
	return search_by_script_lang(size, NULL, MUPDF_SCRIPT_SYMBOLS, FZ_LANG_UNSET);
}

const unsigned char *
fz_lookup_noto_symbol2_font(fz_context *ctx, int *size)
{
	return search_by_script_lang(size, NULL, MUPDF_SCRIPT_SYMBOLS2, FZ_LANG_UNSET);
}

const unsigned char *
fz_lookup_noto_emoji_font(fz_context *ctx, int *size)
{
	return search_by_script_lang(size, NULL, MUPDF_SCRIPT_EMOJI, FZ_LANG_UNSET);
}

const unsigned char *
fz_lookup_noto_boxes_font(fz_context *ctx, int *size)
{
	return search_by_family(size, "Nimbus Boxes", REGULAR);
}

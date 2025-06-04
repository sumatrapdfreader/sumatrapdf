// Copyright (C) 2004-2025 Artifex Software, Inc.
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
#define INBUILT_SIZE(e) (e->size ? *e->size : 0)
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


const char *
fz_lookup_noto_stem_from_script(fz_context *ctx, int script, int language)
{
	switch (script)
	{
	default:
	case UCDN_SCRIPT_COMMON:
	case UCDN_SCRIPT_INHERITED:
	case UCDN_SCRIPT_UNKNOWN:
		return NULL;

	case UCDN_SCRIPT_HANGUL: return "KR";
	case UCDN_SCRIPT_HIRAGANA: return "JP";
	case UCDN_SCRIPT_KATAKANA: return "JP";
	case UCDN_SCRIPT_BOPOMOFO: return "TC";
	case UCDN_SCRIPT_HAN:
		switch (language)
		{
		case FZ_LANG_ja: return "JP";
		case FZ_LANG_ko: return "KR";
		case FZ_LANG_zh_Hans: return "SC";
		default:
		case FZ_LANG_zh_Hant: return "TC";
		}

	case UCDN_SCRIPT_LATIN: return "";
	case UCDN_SCRIPT_GREEK: return "";
	case UCDN_SCRIPT_CYRILLIC: return "";
	case UCDN_SCRIPT_ARABIC: return "Naskh";

	case UCDN_SCRIPT_ARMENIAN: return "Armenian";
	case UCDN_SCRIPT_HEBREW: return "Hebrew";
	case UCDN_SCRIPT_SYRIAC: return "Syriac";
	case UCDN_SCRIPT_THAANA: return "Thaana";
	case UCDN_SCRIPT_DEVANAGARI: return "Devanagari";
	case UCDN_SCRIPT_BENGALI: return "Bengali";
	case UCDN_SCRIPT_GURMUKHI: return "Gurmukhi";
	case UCDN_SCRIPT_GUJARATI: return "Gujarati";
	case UCDN_SCRIPT_ORIYA: return "Oriya";
	case UCDN_SCRIPT_TAMIL: return "Tamil";
	case UCDN_SCRIPT_TELUGU: return "Telugu";
	case UCDN_SCRIPT_KANNADA: return "Kannada";
	case UCDN_SCRIPT_MALAYALAM: return "Malayalam";
	case UCDN_SCRIPT_SINHALA: return "Sinhala";
	case UCDN_SCRIPT_THAI: return "Thai";
	case UCDN_SCRIPT_LAO: return "Lao";
	case UCDN_SCRIPT_TIBETAN: return "Tibetan";
	case UCDN_SCRIPT_MYANMAR: return "Myanmar";
	case UCDN_SCRIPT_GEORGIAN: return "Georgian";
	case UCDN_SCRIPT_ETHIOPIC: return "Ethiopic";
	case UCDN_SCRIPT_CHEROKEE: return "Cherokee";
	case UCDN_SCRIPT_CANADIAN_ABORIGINAL: return "CanadianAboriginal";
	case UCDN_SCRIPT_OGHAM: return "Ogham";
	case UCDN_SCRIPT_RUNIC: return "Runic";
	case UCDN_SCRIPT_KHMER: return "Khmer";
	case UCDN_SCRIPT_MONGOLIAN: return "Mongolian";
	case UCDN_SCRIPT_YI: return "Yi";
	case UCDN_SCRIPT_OLD_ITALIC: return "OldItalic";
	case UCDN_SCRIPT_GOTHIC: return "Gothic";
	case UCDN_SCRIPT_DESERET: return "Deseret";
	case UCDN_SCRIPT_TAGALOG: return "Tagalog";
	case UCDN_SCRIPT_HANUNOO: return "Hanunoo";
	case UCDN_SCRIPT_BUHID: return "Buhid";
	case UCDN_SCRIPT_TAGBANWA: return "Tagbanwa";
	case UCDN_SCRIPT_LIMBU: return "Limbu";
	case UCDN_SCRIPT_TAI_LE: return "TaiLe";
	case UCDN_SCRIPT_LINEAR_B: return "LinearB";
	case UCDN_SCRIPT_UGARITIC: return "Ugaritic";
	case UCDN_SCRIPT_SHAVIAN: return "Shavian";
	case UCDN_SCRIPT_OSMANYA: return "Osmanya";
	case UCDN_SCRIPT_CYPRIOT: return "Cypriot";
	case UCDN_SCRIPT_BUGINESE: return "Buginese";
	case UCDN_SCRIPT_COPTIC: return "Coptic";
	case UCDN_SCRIPT_NEW_TAI_LUE: return "NewTaiLue";
	case UCDN_SCRIPT_GLAGOLITIC: return "Glagolitic";
	case UCDN_SCRIPT_TIFINAGH: return "Tifinagh";
	case UCDN_SCRIPT_SYLOTI_NAGRI: return "SylotiNagri";
	case UCDN_SCRIPT_OLD_PERSIAN: return "OldPersian";
	case UCDN_SCRIPT_KHAROSHTHI: return "Kharoshthi";
	case UCDN_SCRIPT_BALINESE: return "Balinese";
	case UCDN_SCRIPT_CUNEIFORM: return "Cuneiform";
	case UCDN_SCRIPT_PHOENICIAN: return "Phoenician";
	case UCDN_SCRIPT_PHAGS_PA: return "PhagsPa";
	case UCDN_SCRIPT_NKO: return "NKo";
	case UCDN_SCRIPT_SUNDANESE: return "Sundanese";
	case UCDN_SCRIPT_LEPCHA: return "Lepcha";
	case UCDN_SCRIPT_OL_CHIKI: return "OlChiki";
	case UCDN_SCRIPT_VAI: return "Vai";
	case UCDN_SCRIPT_SAURASHTRA: return "Saurashtra";
	case UCDN_SCRIPT_KAYAH_LI: return "KayahLi";
	case UCDN_SCRIPT_REJANG: return "Rejang";
	case UCDN_SCRIPT_LYCIAN: return "Lycian";
	case UCDN_SCRIPT_CARIAN: return "Carian";
	case UCDN_SCRIPT_LYDIAN: return "Lydian";
	case UCDN_SCRIPT_CHAM: return "Cham";
	case UCDN_SCRIPT_TAI_THAM: return "TaiTham";
	case UCDN_SCRIPT_TAI_VIET: return "TaiViet";
	case UCDN_SCRIPT_AVESTAN: return "Avestan";
	case UCDN_SCRIPT_EGYPTIAN_HIEROGLYPHS: return "EgyptianHieroglyphs";
	case UCDN_SCRIPT_SAMARITAN: return "Samaritan";
	case UCDN_SCRIPT_LISU: return "Lisu";
	case UCDN_SCRIPT_BAMUM: return "Bamum";
	case UCDN_SCRIPT_JAVANESE: return "Javanese";
	case UCDN_SCRIPT_MEETEI_MAYEK: return "MeeteiMayek";
	case UCDN_SCRIPT_IMPERIAL_ARAMAIC: return "ImperialAramaic";
	case UCDN_SCRIPT_OLD_SOUTH_ARABIAN: return "OldSouthArabian";
	case UCDN_SCRIPT_INSCRIPTIONAL_PARTHIAN: return "InscriptionalParthian";
	case UCDN_SCRIPT_INSCRIPTIONAL_PAHLAVI: return "InscriptionalPahlavi";
	case UCDN_SCRIPT_OLD_TURKIC: return "OldTurkic";
	case UCDN_SCRIPT_KAITHI: return "Kaithi";
	case UCDN_SCRIPT_BATAK: return "Batak";
	case UCDN_SCRIPT_BRAHMI: return "Brahmi";
	case UCDN_SCRIPT_MANDAIC: return "Mandaic";
	case UCDN_SCRIPT_CHAKMA: return "Chakma";
	case UCDN_SCRIPT_MEROITIC_CURSIVE: return "Meroitic";
	case UCDN_SCRIPT_MEROITIC_HIEROGLYPHS: return "Meroitic";
	case UCDN_SCRIPT_MIAO: return "Miao";
	case UCDN_SCRIPT_SHARADA: return "Sharada";
	case UCDN_SCRIPT_SORA_SOMPENG: return "SoraSompeng";
	case UCDN_SCRIPT_TAKRI: return "Takri";
	case UCDN_SCRIPT_BASSA_VAH: return "BassaVah";
	case UCDN_SCRIPT_CAUCASIAN_ALBANIAN: return "CaucasianAlbanian";
	case UCDN_SCRIPT_DUPLOYAN: return "Duployan";
	case UCDN_SCRIPT_ELBASAN: return "Elbasan";
	case UCDN_SCRIPT_GRANTHA: return "Grantha";
	case UCDN_SCRIPT_KHOJKI: return "Khojki";
	case UCDN_SCRIPT_KHUDAWADI: return "Khudawadi";
	case UCDN_SCRIPT_LINEAR_A: return "LinearA";
	case UCDN_SCRIPT_MAHAJANI: return "Mahajani";
	case UCDN_SCRIPT_MANICHAEAN: return "Manichaean";
	case UCDN_SCRIPT_MENDE_KIKAKUI: return "MendeKikakui";
	case UCDN_SCRIPT_MODI: return "Modi";
	case UCDN_SCRIPT_MRO: return "Mro";
	case UCDN_SCRIPT_NABATAEAN: return "Nabataean";
	case UCDN_SCRIPT_OLD_NORTH_ARABIAN: return "OldNorthArabian";
	case UCDN_SCRIPT_OLD_PERMIC: return "OldPermic";
	case UCDN_SCRIPT_PAHAWH_HMONG: return "PahawhHmong";
	case UCDN_SCRIPT_PALMYRENE: return "Palmyrene";
	case UCDN_SCRIPT_PAU_CIN_HAU: return "PauCinHau";
	case UCDN_SCRIPT_PSALTER_PAHLAVI: return "PsalterPahlavi";
	case UCDN_SCRIPT_SIDDHAM: return "Siddham";
	case UCDN_SCRIPT_TIRHUTA: return "Tirhuta";
	case UCDN_SCRIPT_WARANG_CITI: return "WarangCiti";
	case UCDN_SCRIPT_AHOM: return "Ahom";
	case UCDN_SCRIPT_ANATOLIAN_HIEROGLYPHS: return "AnatolianHieroglyphs";
	case UCDN_SCRIPT_HATRAN: return "Hatran";
	case UCDN_SCRIPT_MULTANI: return "Multani";
	case UCDN_SCRIPT_OLD_HUNGARIAN: return "OldHungarian";
	case UCDN_SCRIPT_SIGNWRITING: return "SignWriting";
	case UCDN_SCRIPT_ADLAM: return "Adlam";
	case UCDN_SCRIPT_BHAIKSUKI: return "Bhaiksuki";
	case UCDN_SCRIPT_MARCHEN: return "Marchen";
	case UCDN_SCRIPT_NEWA: return "Newa";
	case UCDN_SCRIPT_OSAGE: return "Osage";
	case UCDN_SCRIPT_TANGUT: return "Tangut";
	case UCDN_SCRIPT_MASARAM_GONDI: return "MasaramGondi";
	case UCDN_SCRIPT_NUSHU: return "Nushu";
	case UCDN_SCRIPT_SOYOMBO: return "Soyombo";
	case UCDN_SCRIPT_ZANABAZAR_SQUARE: return "ZanabazarSquare";
	case UCDN_SCRIPT_DOGRA: return "Dogra";
	case UCDN_SCRIPT_GUNJALA_GONDI: return "GunjalaGondi";
	case UCDN_SCRIPT_HANIFI_ROHINGYA: return "HanifiRohingya";
	case UCDN_SCRIPT_MAKASAR: return "Makasar";
	case UCDN_SCRIPT_MEDEFAIDRIN: return "Medefaidrin";
	case UCDN_SCRIPT_OLD_SOGDIAN: return "OldSogdian";
	case UCDN_SCRIPT_SOGDIAN: return "Sogdian";
	case UCDN_SCRIPT_ELYMAIC: return "Elymaic";
	case UCDN_SCRIPT_NANDINAGARI: return "Nandinagari";
	case UCDN_SCRIPT_NYIAKENG_PUACHUE_HMONG: return "NyiakengPuachueHmong";
	case UCDN_SCRIPT_WANCHO: return "Wancho";
	case UCDN_SCRIPT_CHORASMIAN: return "Chorasmian";
	case UCDN_SCRIPT_DIVES_AKURU: return "DivesAkuru";
	case UCDN_SCRIPT_KHITAN_SMALL_SCRIPT: return "KhitanSmallScript";
	case UCDN_SCRIPT_YEZIDI: return "Yezidi";
	case UCDN_SCRIPT_VITHKUQI: return "Vithkuqi";
	case UCDN_SCRIPT_OLD_UYGHUR: return "OldUyghur";
	case UCDN_SCRIPT_CYPRO_MINOAN: return "CyproMinoan";
	case UCDN_SCRIPT_TANGSA: return "Tangsa";
	case UCDN_SCRIPT_TOTO: return "Toto";
	case UCDN_SCRIPT_KAWI: return "Kawi";
	case UCDN_SCRIPT_NAG_MUNDARI: return "NagMundari";
	}
}

const char *
fz_lookup_script_name(fz_context *ctx, int script, int language)
{
	switch (script) {
	case UCDN_SCRIPT_COMMON:
	case UCDN_SCRIPT_INHERITED:
	case UCDN_SCRIPT_UNKNOWN:
		return "Common";
	case UCDN_SCRIPT_LATIN: return "Latin";
	case UCDN_SCRIPT_GREEK: return "Greek";
	case UCDN_SCRIPT_CYRILLIC: return "Cyrillic";
	case UCDN_SCRIPT_ARABIC: return "Arabic";
	default: return fz_lookup_noto_stem_from_script(ctx, script, language);
	}
}

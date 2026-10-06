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

// SumatraPDF: compiled instead of source/fitz/noto.c. Same font table
// (font-table.h), but no font data is linked into the binary: it comes from a
// loader set with fz_set_builtin_font_loader(), which SumatraPDF serves from the
// fonts\ entries of its embedded archive (src/EmbeddedResources.cpp). A table
// entry the loader has no file for is skipped, so the set of built-in fonts is
// whatever cmd/pack-embedded-prebuild.cmd packs, not a TOFU_* define.

#include "mupdf/fitz.h"
#include "mupdf/ucdn.h"

#include "noto_sumatra.h"

#include <string.h>

/* This historic script has an unusually large font (2MB), so we skip it by default. */
#ifndef NOTO_TANGUT
#define NOTO_TANGUT 0
#endif

/* Define some extra scripts for special fonts. */
enum {
    MUPDF_SCRIPT_MUSIC = UCDN_LAST_SCRIPT + 1,
    MUPDF_SCRIPT_MATH,
    MUPDF_SCRIPT_SYMBOLS,
    MUPDF_SCRIPT_SYMBOLS2,
    MUPDF_SCRIPT_EMOJI,
    MUPDF_SCRIPT_CJKV
};

enum {
    BOLD = 1,
    ITALIC = 2
};

typedef struct {
    const char* symbol; /* font-table.h name, e.g. NimbusRoman_Regular_cff; NULL for EMPTY */
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

#define FONT(FORGE, NAME, NAME2, SCRIPT, LANG, SUBFONT, ATTR) {#NAME, NAME2, SCRIPT, LANG, SUBFONT, ATTR},
#define ALIAS(FORGE, NAME, NAME2, SCRIPT, LANG, SUBFONT, ATTR) {#NAME, NAME2, SCRIPT, LANG, SUBFONT, ATTR},
#define EMPTY(SCRIPT) {NULL, "", SCRIPT, FZ_LANG_UNSET, NO_SUBFONT, REGULAR},
static const font_entry inbuilt_fonts[] = {
#include "../../../ext/mupdf/source/fitz/font-table.h"
    {NULL, "", END_OF_DATA, FZ_LANG_UNSET, NO_SUBFONT, REGULAR}};
#undef FONT
#undef ALIAS
#undef EMPTY

static fz_builtin_font_loader font_loader;

void fz_set_builtin_font_loader(fz_builtin_font_loader loader) {
    font_loader = loader;
}

/* The entry's font data, or NULL when the loader has no such file. The symbol
 * is the file name with '-' and '.' replaced by '_':
 * NimbusRoman_Regular_cff is resources/fonts/urw/NimbusRoman-Regular.cff */
static const unsigned char* load_font(const font_entry* e, int* size) {
    char name[64];
    char *p, *ext;

    *size = 0;
    if (!font_loader || !e->symbol || strlen(e->symbol) >= sizeof(name)) return NULL;
    fz_strlcpy(name, e->symbol, sizeof(name));
    ext = strrchr(name, '_');
    if (!ext) return NULL;
    *ext = '.';
    for (p = name; p < ext; p++)
        if (*p == '_') *p = '-';
    return font_loader(name, size);
}

static const unsigned char* search_by_script_lang_strict(int* size, int* subfont, int script, int language) {
    /* Search in the inbuilt font table. */
    const font_entry* e;
    const unsigned char* data;

    if (subfont) *subfont = 0;

    for (e = inbuilt_fonts; e->script != END_OF_DATA; e++) {
        if (script != ANY_SCRIPT && e->script != script) continue;
        if (e->lang != language) continue;
        if (!e->symbol) break; /* EMPTY: deliberately no font for this script */
        data = load_font(e, size);
        if (!data) continue; /* not packed: try the next font for this script */
        if (subfont) *subfont = e->subfont;
        return data;
    }

    return *size = 0, NULL;
}

static const unsigned char* search_by_script_lang(int* size, int* subfont, int script, int language) {
    const unsigned char* result;
    result = search_by_script_lang_strict(size, subfont, script, language);
    if (!result && language != FZ_LANG_UNSET)
        result = search_by_script_lang_strict(size, subfont, script, FZ_LANG_UNSET);
    return result;
}

static const unsigned char* search_by_family(int* size, const char* family, int attr) {
    /* Search in the inbuilt font table. */
    const font_entry* e;
    const unsigned char* data;

    for (e = inbuilt_fonts; e->script != END_OF_DATA; e++) {
        if (e->family[0] == '\0') continue;
        if (attr != e->attr) continue;
        if (fz_strcasecmp(e->family, family)) continue;
        data = load_font(e, size);
        if (data) return data;
    }

    return *size = 0, NULL;
}

const unsigned char* fz_lookup_base14_font(fz_context* ctx, const char* name, int* size) {
    /* We want to insist on the base14 name matching exactly,
     * so we check that here first, before we look in the font table
     * to see if we actually have data. */

    if (!strcmp(name, "Courier")) return search_by_family(size, "Courier", REGULAR);
    if (!strcmp(name, "Courier-Oblique")) return search_by_family(size, "Courier", ITALIC);
    if (!strcmp(name, "Courier-Bold")) return search_by_family(size, "Courier", BOLD);
    if (!strcmp(name, "Courier-BoldOblique")) return search_by_family(size, "Courier", BOLD | ITALIC);

    if (!strcmp(name, "Helvetica")) return search_by_family(size, "Helvetica", REGULAR);
    if (!strcmp(name, "Helvetica-Oblique")) return search_by_family(size, "Helvetica", ITALIC);
    if (!strcmp(name, "Helvetica-Bold")) return search_by_family(size, "Helvetica", BOLD);
    if (!strcmp(name, "Helvetica-BoldOblique")) return search_by_family(size, "Helvetica", BOLD | ITALIC);

    if (!strcmp(name, "Times-Roman")) return search_by_family(size, "Times", REGULAR);
    if (!strcmp(name, "Times-Italic")) return search_by_family(size, "Times", ITALIC);
    if (!strcmp(name, "Times-Bold")) return search_by_family(size, "Times", BOLD);
    if (!strcmp(name, "Times-BoldItalic")) return search_by_family(size, "Times", BOLD | ITALIC);

    if (!strcmp(name, "Symbol")) return search_by_family(size, "Symbol", REGULAR);
    if (!strcmp(name, "ZapfDingbats")) return search_by_family(size, "ZapfDingbats", REGULAR);

    *size = 0;
    return NULL;
}

const unsigned char* fz_lookup_builtin_font(fz_context* ctx, const char* family, int is_bold, int is_italic,
                                            int* size) {
    return search_by_family(size, family, (is_bold ? BOLD : 0) | (is_italic ? ITALIC : 0));
}

const unsigned char* fz_lookup_cjk_font(fz_context* ctx, int ordering, int* size, int* subfont) {
    int lang = FZ_LANG_UNSET;
    switch (ordering) {
        case FZ_ADOBE_JAPAN:
            lang = FZ_LANG_ja;
            break;
        case FZ_ADOBE_KOREA:
            lang = FZ_LANG_ko;
            break;
        case FZ_ADOBE_GB:
            lang = FZ_LANG_zh_Hans;
            break;
        case FZ_ADOBE_CNS:
            lang = FZ_LANG_zh_Hant;
            break;
    }
    return search_by_script_lang(size, subfont, UCDN_SCRIPT_HAN, lang);
}

int fz_lookup_cjk_ordering_by_language(const char* lang) {
    if (!strcmp(lang, "zh-Hant")) return FZ_ADOBE_CNS;
    if (!strcmp(lang, "zh-TW")) return FZ_ADOBE_CNS;
    if (!strcmp(lang, "zh-HK")) return FZ_ADOBE_CNS;
    if (!strcmp(lang, "zh-Hans")) return FZ_ADOBE_GB;
    if (!strcmp(lang, "zh-CN")) return FZ_ADOBE_GB;
    if (!strcmp(lang, "ja")) return FZ_ADOBE_JAPAN;
    if (!strcmp(lang, "ko")) return FZ_ADOBE_KOREA;
    return -1;
}

static int fz_lookup_cjk_language(const char* lang) {
    if (!strcmp(lang, "zh-Hant")) return FZ_LANG_zh_Hant;
    if (!strcmp(lang, "zh-TW")) return FZ_LANG_zh_Hant;
    if (!strcmp(lang, "zh-HK")) return FZ_LANG_zh_Hant;
    if (!strcmp(lang, "zh-Hans")) return FZ_LANG_zh_Hans;
    if (!strcmp(lang, "zh-CN")) return FZ_LANG_zh_Hans;
    if (!strcmp(lang, "ja")) return FZ_LANG_ja;
    if (!strcmp(lang, "ko")) return FZ_LANG_ko;
    return FZ_LANG_UNSET;
}

const unsigned char* fz_lookup_cjk_font_by_language(fz_context* ctx, const char* lang, int* size, int* subfont) {
    return search_by_script_lang(size, subfont, UCDN_SCRIPT_HAN, fz_lookup_cjk_language(lang));
}

const unsigned char* fz_lookup_noto_font(fz_context* ctx, int script, int language, int* size, int* subfont) {
    return search_by_script_lang(size, subfont, script, language);
}

const unsigned char* fz_lookup_noto_math_font(fz_context* ctx, int* size) {
    return search_by_script_lang(size, NULL, MUPDF_SCRIPT_MATH, FZ_LANG_UNSET);
}

const unsigned char* fz_lookup_noto_music_font(fz_context* ctx, int* size) {
    return search_by_script_lang(size, NULL, MUPDF_SCRIPT_MUSIC, FZ_LANG_UNSET);
}

const unsigned char* fz_lookup_noto_symbol1_font(fz_context* ctx, int* size) {
    return search_by_script_lang(size, NULL, MUPDF_SCRIPT_SYMBOLS, FZ_LANG_UNSET);
}

const unsigned char* fz_lookup_noto_symbol2_font(fz_context* ctx, int* size) {
    return search_by_script_lang(size, NULL, MUPDF_SCRIPT_SYMBOLS2, FZ_LANG_UNSET);
}

const unsigned char* fz_lookup_noto_emoji_font(fz_context* ctx, int* size) {
    return search_by_script_lang(size, NULL, MUPDF_SCRIPT_EMOJI, FZ_LANG_UNSET);
}

const unsigned char* fz_lookup_noto_boxes_font(fz_context* ctx, int* size) {
    return search_by_family(size, "Nimbus Boxes", REGULAR);
}

const char* fz_lookup_noto_stem_from_script(fz_context* ctx, int script, int language) {
    static const struct {
        int script;
        const char* stem;
    } stems[] = {
        {UCDN_SCRIPT_HANGUL, "KR"},
        {UCDN_SCRIPT_HIRAGANA, "JP"},
        {UCDN_SCRIPT_KATAKANA, "JP"},
        {UCDN_SCRIPT_BOPOMOFO, "TC"},
        {UCDN_SCRIPT_LATIN, ""},
        {UCDN_SCRIPT_GREEK, ""},
        {UCDN_SCRIPT_CYRILLIC, ""},
        {UCDN_SCRIPT_ARABIC, "Naskh"},
        {UCDN_SCRIPT_ARMENIAN, "Armenian"},
        {UCDN_SCRIPT_HEBREW, "Hebrew"},
        {UCDN_SCRIPT_SYRIAC, "Syriac"},
        {UCDN_SCRIPT_THAANA, "Thaana"},
        {UCDN_SCRIPT_DEVANAGARI, "Devanagari"},
        {UCDN_SCRIPT_BENGALI, "Bengali"},
        {UCDN_SCRIPT_GURMUKHI, "Gurmukhi"},
        {UCDN_SCRIPT_GUJARATI, "Gujarati"},
        {UCDN_SCRIPT_ORIYA, "Oriya"},
        {UCDN_SCRIPT_TAMIL, "Tamil"},
        {UCDN_SCRIPT_TELUGU, "Telugu"},
        {UCDN_SCRIPT_KANNADA, "Kannada"},
        {UCDN_SCRIPT_MALAYALAM, "Malayalam"},
        {UCDN_SCRIPT_SINHALA, "Sinhala"},
        {UCDN_SCRIPT_THAI, "Thai"},
        {UCDN_SCRIPT_LAO, "Lao"},
        {UCDN_SCRIPT_TIBETAN, "Tibetan"},
        {UCDN_SCRIPT_MYANMAR, "Myanmar"},
        {UCDN_SCRIPT_GEORGIAN, "Georgian"},
        {UCDN_SCRIPT_ETHIOPIC, "Ethiopic"},
        {UCDN_SCRIPT_CHEROKEE, "Cherokee"},
        {UCDN_SCRIPT_CANADIAN_ABORIGINAL, "CanadianAboriginal"},
        {UCDN_SCRIPT_OGHAM, "Ogham"},
        {UCDN_SCRIPT_RUNIC, "Runic"},
        {UCDN_SCRIPT_KHMER, "Khmer"},
        {UCDN_SCRIPT_MONGOLIAN, "Mongolian"},
        {UCDN_SCRIPT_YI, "Yi"},
        {UCDN_SCRIPT_OLD_ITALIC, "OldItalic"},
        {UCDN_SCRIPT_GOTHIC, "Gothic"},
        {UCDN_SCRIPT_DESERET, "Deseret"},
        {UCDN_SCRIPT_TAGALOG, "Tagalog"},
        {UCDN_SCRIPT_HANUNOO, "Hanunoo"},
        {UCDN_SCRIPT_BUHID, "Buhid"},
        {UCDN_SCRIPT_TAGBANWA, "Tagbanwa"},
        {UCDN_SCRIPT_LIMBU, "Limbu"},
        {UCDN_SCRIPT_TAI_LE, "TaiLe"},
        {UCDN_SCRIPT_LINEAR_B, "LinearB"},
        {UCDN_SCRIPT_UGARITIC, "Ugaritic"},
        {UCDN_SCRIPT_SHAVIAN, "Shavian"},
        {UCDN_SCRIPT_OSMANYA, "Osmanya"},
        {UCDN_SCRIPT_CYPRIOT, "Cypriot"},
        {UCDN_SCRIPT_BUGINESE, "Buginese"},
        {UCDN_SCRIPT_COPTIC, "Coptic"},
        {UCDN_SCRIPT_NEW_TAI_LUE, "NewTaiLue"},
        {UCDN_SCRIPT_GLAGOLITIC, "Glagolitic"},
        {UCDN_SCRIPT_TIFINAGH, "Tifinagh"},
        {UCDN_SCRIPT_SYLOTI_NAGRI, "SylotiNagri"},
        {UCDN_SCRIPT_OLD_PERSIAN, "OldPersian"},
        {UCDN_SCRIPT_KHAROSHTHI, "Kharoshthi"},
        {UCDN_SCRIPT_BALINESE, "Balinese"},
        {UCDN_SCRIPT_CUNEIFORM, "Cuneiform"},
        {UCDN_SCRIPT_PHOENICIAN, "Phoenician"},
        {UCDN_SCRIPT_PHAGS_PA, "PhagsPa"},
        {UCDN_SCRIPT_NKO, "NKo"},
        {UCDN_SCRIPT_SUNDANESE, "Sundanese"},
        {UCDN_SCRIPT_LEPCHA, "Lepcha"},
        {UCDN_SCRIPT_OL_CHIKI, "OlChiki"},
        {UCDN_SCRIPT_VAI, "Vai"},
        {UCDN_SCRIPT_SAURASHTRA, "Saurashtra"},
        {UCDN_SCRIPT_KAYAH_LI, "KayahLi"},
        {UCDN_SCRIPT_REJANG, "Rejang"},
        {UCDN_SCRIPT_LYCIAN, "Lycian"},
        {UCDN_SCRIPT_CARIAN, "Carian"},
        {UCDN_SCRIPT_LYDIAN, "Lydian"},
        {UCDN_SCRIPT_CHAM, "Cham"},
        {UCDN_SCRIPT_TAI_THAM, "TaiTham"},
        {UCDN_SCRIPT_TAI_VIET, "TaiViet"},
        {UCDN_SCRIPT_AVESTAN, "Avestan"},
        {UCDN_SCRIPT_EGYPTIAN_HIEROGLYPHS, "EgyptianHieroglyphs"},
        {UCDN_SCRIPT_SAMARITAN, "Samaritan"},
        {UCDN_SCRIPT_LISU, "Lisu"},
        {UCDN_SCRIPT_BAMUM, "Bamum"},
        {UCDN_SCRIPT_JAVANESE, "Javanese"},
        {UCDN_SCRIPT_MEETEI_MAYEK, "MeeteiMayek"},
        {UCDN_SCRIPT_IMPERIAL_ARAMAIC, "ImperialAramaic"},
        {UCDN_SCRIPT_OLD_SOUTH_ARABIAN, "OldSouthArabian"},
        {UCDN_SCRIPT_INSCRIPTIONAL_PARTHIAN, "InscriptionalParthian"},
        {UCDN_SCRIPT_INSCRIPTIONAL_PAHLAVI, "InscriptionalPahlavi"},
        {UCDN_SCRIPT_OLD_TURKIC, "OldTurkic"},
        {UCDN_SCRIPT_KAITHI, "Kaithi"},
        {UCDN_SCRIPT_BATAK, "Batak"},
        {UCDN_SCRIPT_BRAHMI, "Brahmi"},
        {UCDN_SCRIPT_MANDAIC, "Mandaic"},
        {UCDN_SCRIPT_CHAKMA, "Chakma"},
        {UCDN_SCRIPT_MEROITIC_CURSIVE, "Meroitic"},
        {UCDN_SCRIPT_MEROITIC_HIEROGLYPHS, "Meroitic"},
        {UCDN_SCRIPT_MIAO, "Miao"},
        {UCDN_SCRIPT_SHARADA, "Sharada"},
        {UCDN_SCRIPT_SORA_SOMPENG, "SoraSompeng"},
        {UCDN_SCRIPT_TAKRI, "Takri"},
        {UCDN_SCRIPT_BASSA_VAH, "BassaVah"},
        {UCDN_SCRIPT_CAUCASIAN_ALBANIAN, "CaucasianAlbanian"},
        {UCDN_SCRIPT_DUPLOYAN, "Duployan"},
        {UCDN_SCRIPT_ELBASAN, "Elbasan"},
        {UCDN_SCRIPT_GRANTHA, "Grantha"},
        {UCDN_SCRIPT_KHOJKI, "Khojki"},
        {UCDN_SCRIPT_KHUDAWADI, "Khudawadi"},
        {UCDN_SCRIPT_LINEAR_A, "LinearA"},
        {UCDN_SCRIPT_MAHAJANI, "Mahajani"},
        {UCDN_SCRIPT_MANICHAEAN, "Manichaean"},
        {UCDN_SCRIPT_MENDE_KIKAKUI, "MendeKikakui"},
        {UCDN_SCRIPT_MODI, "Modi"},
        {UCDN_SCRIPT_MRO, "Mro"},
        {UCDN_SCRIPT_NABATAEAN, "Nabataean"},
        {UCDN_SCRIPT_OLD_NORTH_ARABIAN, "OldNorthArabian"},
        {UCDN_SCRIPT_OLD_PERMIC, "OldPermic"},
        {UCDN_SCRIPT_PAHAWH_HMONG, "PahawhHmong"},
        {UCDN_SCRIPT_PALMYRENE, "Palmyrene"},
        {UCDN_SCRIPT_PAU_CIN_HAU, "PauCinHau"},
        {UCDN_SCRIPT_PSALTER_PAHLAVI, "PsalterPahlavi"},
        {UCDN_SCRIPT_SIDDHAM, "Siddham"},
        {UCDN_SCRIPT_TIRHUTA, "Tirhuta"},
        {UCDN_SCRIPT_WARANG_CITI, "WarangCiti"},
        {UCDN_SCRIPT_AHOM, "Ahom"},
        {UCDN_SCRIPT_ANATOLIAN_HIEROGLYPHS, "AnatolianHieroglyphs"},
        {UCDN_SCRIPT_HATRAN, "Hatran"},
        {UCDN_SCRIPT_MULTANI, "Multani"},
        {UCDN_SCRIPT_OLD_HUNGARIAN, "OldHungarian"},
        {UCDN_SCRIPT_SIGNWRITING, "SignWriting"},
        {UCDN_SCRIPT_ADLAM, "Adlam"},
        {UCDN_SCRIPT_BHAIKSUKI, "Bhaiksuki"},
        {UCDN_SCRIPT_MARCHEN, "Marchen"},
        {UCDN_SCRIPT_NEWA, "Newa"},
        {UCDN_SCRIPT_OSAGE, "Osage"},
        {UCDN_SCRIPT_TANGUT, "Tangut"},
        {UCDN_SCRIPT_MASARAM_GONDI, "MasaramGondi"},
        {UCDN_SCRIPT_NUSHU, "Nushu"},
        {UCDN_SCRIPT_SOYOMBO, "Soyombo"},
        {UCDN_SCRIPT_ZANABAZAR_SQUARE, "ZanabazarSquare"},
        {UCDN_SCRIPT_DOGRA, "Dogra"},
        {UCDN_SCRIPT_GUNJALA_GONDI, "GunjalaGondi"},
        {UCDN_SCRIPT_HANIFI_ROHINGYA, "HanifiRohingya"},
        {UCDN_SCRIPT_MAKASAR, "Makasar"},
        {UCDN_SCRIPT_MEDEFAIDRIN, "Medefaidrin"},
        {UCDN_SCRIPT_OLD_SOGDIAN, "OldSogdian"},
        {UCDN_SCRIPT_SOGDIAN, "Sogdian"},
        {UCDN_SCRIPT_ELYMAIC, "Elymaic"},
        {UCDN_SCRIPT_NANDINAGARI, "Nandinagari"},
        {UCDN_SCRIPT_NYIAKENG_PUACHUE_HMONG, "NyiakengPuachueHmong"},
        {UCDN_SCRIPT_WANCHO, "Wancho"},
        {UCDN_SCRIPT_CHORASMIAN, "Chorasmian"},
        {UCDN_SCRIPT_DIVES_AKURU, "DivesAkuru"},
        {UCDN_SCRIPT_KHITAN_SMALL_SCRIPT, "KhitanSmallScript"},
        {UCDN_SCRIPT_YEZIDI, "Yezidi"},
        {UCDN_SCRIPT_VITHKUQI, "Vithkuqi"},
        {UCDN_SCRIPT_OLD_UYGHUR, "OldUyghur"},
        {UCDN_SCRIPT_CYPRO_MINOAN, "CyproMinoan"},
        {UCDN_SCRIPT_TANGSA, "Tangsa"},
        {UCDN_SCRIPT_TOTO, "Toto"},
        {UCDN_SCRIPT_KAWI, "Kawi"},
        {UCDN_SCRIPT_NAG_MUNDARI, "NagMundari"},
    };

    if (script == UCDN_SCRIPT_HAN) {
        switch (language) {
            case FZ_LANG_ja:
                return "JP";
            case FZ_LANG_ko:
                return "KR";
            case FZ_LANG_zh_Hans:
                return "SC";
            default:
                return "TC";
        }
    }
    for (size_t i = 0; i < sizeof(stems) / sizeof(stems[0]); i++) {
        if (stems[i].script == script) {
            return stems[i].stem;
        }
    }
    return NULL;
}

const char* fz_lookup_script_name(fz_context* ctx, int script, int language) {
    switch (script) {
        case UCDN_SCRIPT_COMMON:
        case UCDN_SCRIPT_INHERITED:
        case UCDN_SCRIPT_UNKNOWN:
            return "Common";
        case UCDN_SCRIPT_LATIN:
            return "Latin";
        case UCDN_SCRIPT_GREEK:
            return "Greek";
        case UCDN_SCRIPT_CYRILLIC:
            return "Cyrillic";
        case UCDN_SCRIPT_ARABIC:
            return "Arabic";
        default:
            return fz_lookup_noto_stem_from_script(ctx, script, language);
    }
}

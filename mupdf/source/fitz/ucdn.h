/*
 * Copyright (C) 2012 Grigori Goronzy <greg@kinoho.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef UCDN_H
#define UCDN_H

#define UCDN_EAST_ASIAN_F 0
#define UCDN_EAST_ASIAN_H 1
#define UCDN_EAST_ASIAN_W 2
#define UCDN_EAST_ASIAN_NA 3
#define UCDN_EAST_ASIAN_A 4
#define UCDN_EAST_ASIAN_N 5

#define UCDN_SCRIPT_COMMON 0
#define UCDN_SCRIPT_LATIN 1
#define UCDN_SCRIPT_GREEK 2
#define UCDN_SCRIPT_CYRILLIC 3
#define UCDN_SCRIPT_ARMENIAN 4
#define UCDN_SCRIPT_HEBREW 5
#define UCDN_SCRIPT_ARABIC 6
#define UCDN_SCRIPT_SYRIAC 7
#define UCDN_SCRIPT_THAANA 8
#define UCDN_SCRIPT_DEVANAGARI 9
#define UCDN_SCRIPT_BENGALI 10
#define UCDN_SCRIPT_GURMUKHI 11
#define UCDN_SCRIPT_GUJARATI 12
#define UCDN_SCRIPT_ORIYA 13
#define UCDN_SCRIPT_TAMIL 14
#define UCDN_SCRIPT_TELUGU 15
#define UCDN_SCRIPT_KANNADA 16
#define UCDN_SCRIPT_MALAYALAM 17
#define UCDN_SCRIPT_SINHALA 18
#define UCDN_SCRIPT_THAI 19
#define UCDN_SCRIPT_LAO 20
#define UCDN_SCRIPT_TIBETAN 21
#define UCDN_SCRIPT_MYANMAR 22
#define UCDN_SCRIPT_GEORGIAN 23
#define UCDN_SCRIPT_HANGUL 24
#define UCDN_SCRIPT_ETHIOPIC 25
#define UCDN_SCRIPT_CHEROKEE 26
#define UCDN_SCRIPT_CANADIAN_ABORIGINAL 27
#define UCDN_SCRIPT_OGHAM 28
#define UCDN_SCRIPT_RUNIC 29
#define UCDN_SCRIPT_KHMER 30
#define UCDN_SCRIPT_MONGOLIAN 31
#define UCDN_SCRIPT_HIRAGANA 32
#define UCDN_SCRIPT_KATAKANA 33
#define UCDN_SCRIPT_BOPOMOFO 34
#define UCDN_SCRIPT_HAN 35
#define UCDN_SCRIPT_YI 36
#define UCDN_SCRIPT_OLD_ITALIC 37
#define UCDN_SCRIPT_GOTHIC 38
#define UCDN_SCRIPT_DESERET 39
#define UCDN_SCRIPT_INHERITED 40
#define UCDN_SCRIPT_TAGALOG 41
#define UCDN_SCRIPT_HANUNOO 42
#define UCDN_SCRIPT_BUHID 43
#define UCDN_SCRIPT_TAGBANWA 44
#define UCDN_SCRIPT_LIMBU 45
#define UCDN_SCRIPT_TAI_LE 46
#define UCDN_SCRIPT_LINEAR_B 47
#define UCDN_SCRIPT_UGARITIC 48
#define UCDN_SCRIPT_SHAVIAN 49
#define UCDN_SCRIPT_OSMANYA 50
#define UCDN_SCRIPT_CYPRIOT 51
#define UCDN_SCRIPT_BRAILLE 52
#define UCDN_SCRIPT_BUGINESE 53
#define UCDN_SCRIPT_COPTIC 54
#define UCDN_SCRIPT_NEW_TAI_LUE 55
#define UCDN_SCRIPT_GLAGOLITIC 56
#define UCDN_SCRIPT_TIFINAGH 57
#define UCDN_SCRIPT_SYLOTI_NAGRI 58
#define UCDN_SCRIPT_OLD_PERSIAN 59
#define UCDN_SCRIPT_KHAROSHTHI 60
#define UCDN_SCRIPT_BALINESE 61
#define UCDN_SCRIPT_CUNEIFORM 62
#define UCDN_SCRIPT_PHOENICIAN 63
#define UCDN_SCRIPT_PHAGS_PA 64
#define UCDN_SCRIPT_NKO 65
#define UCDN_SCRIPT_SUNDANESE 66
#define UCDN_SCRIPT_LEPCHA 67
#define UCDN_SCRIPT_OL_CHIKI 68
#define UCDN_SCRIPT_VAI 69
#define UCDN_SCRIPT_SAURASHTRA 70
#define UCDN_SCRIPT_KAYAH_LI 71
#define UCDN_SCRIPT_REJANG 72
#define UCDN_SCRIPT_LYCIAN 73
#define UCDN_SCRIPT_CARIAN 74
#define UCDN_SCRIPT_LYDIAN 75
#define UCDN_SCRIPT_CHAM 76
#define UCDN_SCRIPT_TAI_THAM 77
#define UCDN_SCRIPT_TAI_VIET 78
#define UCDN_SCRIPT_AVESTAN 79
#define UCDN_SCRIPT_EGYPTIAN_HIEROGLYPHS 80
#define UCDN_SCRIPT_SAMARITAN 81
#define UCDN_SCRIPT_LISU 82
#define UCDN_SCRIPT_BAMUM 83
#define UCDN_SCRIPT_JAVANESE 84
#define UCDN_SCRIPT_MEETEI_MAYEK 85
#define UCDN_SCRIPT_IMPERIAL_ARAMAIC 86
#define UCDN_SCRIPT_OLD_SOUTH_ARABIAN 87
#define UCDN_SCRIPT_INSCRIPTIONAL_PARTHIAN 88
#define UCDN_SCRIPT_INSCRIPTIONAL_PAHLAVI 89
#define UCDN_SCRIPT_OLD_TURKIC 90
#define UCDN_SCRIPT_KAITHI 91
#define UCDN_SCRIPT_BATAK 92
#define UCDN_SCRIPT_BRAHMI 93
#define UCDN_SCRIPT_MANDAIC 94
#define UCDN_SCRIPT_CHAKMA 95
#define UCDN_SCRIPT_MEROITIC_CURSIVE 96
#define UCDN_SCRIPT_MEROITIC_HIEROGLYPHS 97
#define UCDN_SCRIPT_MIAO 98
#define UCDN_SCRIPT_SHARADA 99
#define UCDN_SCRIPT_SORA_SOMPENG 100
#define UCDN_SCRIPT_TAKRI 101
#define UCDN_SCRIPT_UNKNOWN 102

#define UCDN_GENERAL_CATEGORY_CC 0
#define UCDN_GENERAL_CATEGORY_CF 1
#define UCDN_GENERAL_CATEGORY_CN 2
#define UCDN_GENERAL_CATEGORY_CO 3
#define UCDN_GENERAL_CATEGORY_CS 4
#define UCDN_GENERAL_CATEGORY_LL 5
#define UCDN_GENERAL_CATEGORY_LM 6
#define UCDN_GENERAL_CATEGORY_LO 7
#define UCDN_GENERAL_CATEGORY_LT 8
#define UCDN_GENERAL_CATEGORY_LU 9
#define UCDN_GENERAL_CATEGORY_MC 10
#define UCDN_GENERAL_CATEGORY_ME 11
#define UCDN_GENERAL_CATEGORY_MN 12
#define UCDN_GENERAL_CATEGORY_ND 13
#define UCDN_GENERAL_CATEGORY_NL 14
#define UCDN_GENERAL_CATEGORY_NO 15
#define UCDN_GENERAL_CATEGORY_PC 16
#define UCDN_GENERAL_CATEGORY_PD 17
#define UCDN_GENERAL_CATEGORY_PE 18
#define UCDN_GENERAL_CATEGORY_PF 19
#define UCDN_GENERAL_CATEGORY_PI 20
#define UCDN_GENERAL_CATEGORY_PO 21
#define UCDN_GENERAL_CATEGORY_PS 22
#define UCDN_GENERAL_CATEGORY_SC 23
#define UCDN_GENERAL_CATEGORY_SK 24
#define UCDN_GENERAL_CATEGORY_SM 25
#define UCDN_GENERAL_CATEGORY_SO 26
#define UCDN_GENERAL_CATEGORY_ZL 27
#define UCDN_GENERAL_CATEGORY_ZP 28
#define UCDN_GENERAL_CATEGORY_ZS 29

#define UCDN_BIDI_CLASS_L 0
#define UCDN_BIDI_CLASS_LRE 1
#define UCDN_BIDI_CLASS_LRO 2
#define UCDN_BIDI_CLASS_R 3
#define UCDN_BIDI_CLASS_AL 4
#define UCDN_BIDI_CLASS_RLE 5
#define UCDN_BIDI_CLASS_RLO 6
#define UCDN_BIDI_CLASS_PDF 7
#define UCDN_BIDI_CLASS_EN 8
#define UCDN_BIDI_CLASS_ES 9
#define UCDN_BIDI_CLASS_ET 10
#define UCDN_BIDI_CLASS_AN 11
#define UCDN_BIDI_CLASS_CS 12
#define UCDN_BIDI_CLASS_NSM 13
#define UCDN_BIDI_CLASS_BN 14
#define UCDN_BIDI_CLASS_B 15
#define UCDN_BIDI_CLASS_S 16
#define UCDN_BIDI_CLASS_WS 17
#define UCDN_BIDI_CLASS_ON 18

/**
 * Return version of the Unicode database.
 *
 * @return Unicode database version
 */
const char *ucdn_get_unicode_version(void);

/**
 * Get combining class of a codepoint.
 *
 * @param code Unicode codepoint
 * @return combining class value, as defined in UAX#44
 */
int ucdn_get_combining_class(unsigned int code);

/**
 * Get east-asian width of a codepoint.
 *
 * @param code Unicode codepoint
 * @return value according to UCDN_EAST_ASIAN_* and as defined in UAX#11.
 */
int ucdn_get_east_asian_width(unsigned int code);

/**
 * Get general category of a codepoint.
 *
 * @param code Unicode codepoint
 * @return value according to UCDN_GENERAL_CATEGORY_* and as defined in
 * UAX#44.
 */
int ucdn_get_general_category(unsigned int code);

/**
 * Get bidirectional class of a codepoint.
 *
 * @param code Unicode codepoint
 * @return value according to UCDN_BIDI_CLASS_* and as defined in UAX#44.
 */
int ucdn_get_bidi_class(unsigned int code);

/**
 * Get script of a codepoint.
 *
 * @param code Unicode codepoint
 * @return value according to UCDN_SCRIPT_* and as defined in UAX#24.
 */
int ucdn_get_script(unsigned int code);

/**
 * Check if codepoint can be mirrored.
 *
 * @param code Unicode codepoint
 * @return 1 if mirrored character exists, otherwise 0
 */
int ucdn_get_mirrored(unsigned int code);

/**
 * Mirror a codepoint.
 *
 * @param code Unicode codepoint
 * @return mirrored codepoint or the original codepoint if no
 * mirrored character exists
 */
unsigned int ucdn_mirror(unsigned int code);

/**
 * Pairwise canonical decomposition of a codepoint. This includes
 * Hangul Jamo decomposition (see chapter 3.12 of the Unicode core
 * specification).
 *
 * Hangul is decomposed into L and V jamos for LV forms, and an
 * LV precomposed syllable and a T jamo for LVT forms.
 *
 * @param code Unicode codepoint
 * @param a filled with first codepoint of decomposition
 * @param b filled with second codepoint of decomposition, or 0
 * @return success
 */
int ucdn_decompose(unsigned int code, unsigned int *a, unsigned int *b);

/**
 * Compatibility decomposition of a codepoint.
 *
 * @param code Unicode codepoint
 * @param decomposed filled with decomposition, must be able to hold 18
 * characters
 * @return length of decomposition or 0 in case none exists
 */
int ucdn_compat_decompose(unsigned int code, unsigned int *decomposed);

/**
 * Pairwise canonical composition of two codepoints. This includes
 * Hangul Jamo composition (see chapter 3.12 of the Unicode core
 * specification).
 *
 * Hangul composition expects either L and V jamos, or an LV
 * precomposed syllable and a T jamo. This is exactly the inverse
 * of pairwise Hangul decomposition.
 *
 * @param code filled with composition
 * @param a first codepoint
 * @param b second codepoint
 * @return success
 */
int ucdn_compose(unsigned int *code, unsigned int a, unsigned int b);

#endif

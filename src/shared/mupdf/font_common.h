/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* font lookup helpers shared by the linux and mac system font loaders */

#include "mupdf/fitz.h"
#include "mupdf/ucdn.h"

// "zw" (正文, body text): Chinese EPUB tools point it at e-reader font paths
// (#6500). Windows maps it to Microsoft YaHei; here to the system's CJK font
static inline int is_cjk_body_font_alias(const char* name, int* bold) {
    if (!name) {
        return 0;
    }
    if (!strcmp(name, "zw-Bold")) {
        *bold = 1;
        return 1;
    }
    return !strcmp(name, "zw");
}

static inline unsigned int fallback_rune(int script) {
    switch (script) {
        case UCDN_SCRIPT_ARABIC:
            return 0x0627;
        case UCDN_SCRIPT_ARMENIAN:
            return 0x0531;
        case UCDN_SCRIPT_BENGALI:
            return 0x0985;
        case UCDN_SCRIPT_CYRILLIC:
            return 0x0410;
        case UCDN_SCRIPT_DEVANAGARI:
            return 0x0905;
        case UCDN_SCRIPT_ETHIOPIC:
            return 0x1200;
        case UCDN_SCRIPT_GEORGIAN:
            return 0x10d0;
        case UCDN_SCRIPT_GREEK:
            return 0x0391;
        case UCDN_SCRIPT_GUJARATI:
            return 0x0a85;
        case UCDN_SCRIPT_GURMUKHI:
            return 0x0a05;
        case UCDN_SCRIPT_HANGUL:
            return 0xac00;
        case UCDN_SCRIPT_HEBREW:
            return 0x05d0;
        case UCDN_SCRIPT_HIRAGANA:
        case UCDN_SCRIPT_KATAKANA:
            return 0x3042;
        case UCDN_SCRIPT_KANNADA:
            return 0x0c85;
        case UCDN_SCRIPT_KHMER:
            return 0x1780;
        case UCDN_SCRIPT_LAO:
            return 0x0e81;
        case UCDN_SCRIPT_MALAYALAM:
            return 0x0d05;
        case UCDN_SCRIPT_MYANMAR:
            return 0x1000;
        case UCDN_SCRIPT_ORIYA:
            return 0x0b05;
        case UCDN_SCRIPT_SINHALA:
            return 0x0d85;
        case UCDN_SCRIPT_TAMIL:
            return 0x0b85;
        case UCDN_SCRIPT_TELUGU:
            return 0x0c05;
        case UCDN_SCRIPT_THAI:
            return 0x0e01;
        case UCDN_SCRIPT_TIBETAN:
            return 0x0f40;
        case UCDN_SCRIPT_YI:
            return 0xa000;
        case UCDN_SCRIPT_HAN:
        case UCDN_SCRIPT_BOPOMOFO:
            return 0x4e00;
        case UCDN_SCRIPT_LATIN:
        case UCDN_SCRIPT_COMMON:
        case UCDN_SCRIPT_INHERITED:
        case UCDN_SCRIPT_UNKNOWN:
            return 0x0041;
        default:
            // a script we have no sample for: a Latin font wouldn't have it
            // either, and mupdf's own font for it is better than boxes
            return 0;
    }
}

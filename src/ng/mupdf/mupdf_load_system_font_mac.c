/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "mupdf/fitz.h"
#include "mupdf/ucdn.h"

#include <CoreText/CoreText.h>

typedef struct mac_font_table {
    uint32_t tag;
    uint32_t offset;
    uint32_t length;
    CFDataRef data;
} mac_font_table;

static void write_be16(unsigned char* p, uint16_t value) {
    p[0] = (unsigned char)(value >> 8);
    p[1] = (unsigned char)value;
}

static void write_be32(unsigned char* p, uint32_t value) {
    p[0] = (unsigned char)(value >> 24);
    p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8);
    p[3] = (unsigned char)value;
}

static uint32_t table_checksum(const unsigned char* data, size_t size) {
    uint32_t sum = 0;
    for (size_t i = 0; i < size; i += 4) {
        uint32_t word = (uint32_t)data[i] << 24;
        if (i + 1 < size) {
            word |= (uint32_t)data[i + 1] << 16;
        }
        if (i + 2 < size) {
            word |= (uint32_t)data[i + 2] << 8;
        }
        if (i + 3 < size) {
            word |= data[i + 3];
        }
        sum += word;
    }
    return sum;
}

static int compare_tables(const void* a, const void* b) {
    uint32_t ta = ((const mac_font_table*)a)->tag;
    uint32_t tb = ((const mac_font_table*)b)->tag;
    return ta < tb ? -1 : ta > tb;
}

static void free_tables(mac_font_table* tables, int count) {
    for (int i = 0; i < count; i++) {
        CFRelease(tables[i].data);
    }
    free(tables);
}

static fz_font* load_ct_font_data(fz_context* ctx, const char* name, CTFontRef ct_font) {
    CFArrayRef tags = CTFontCopyAvailableTables(ct_font, kCTFontTableOptionNoOptions);
    if (!tags) {
        return NULL;
    }
    CFIndex tag_count = CFArrayGetCount(tags);
    if (tag_count <= 0 || tag_count > UINT16_MAX) {
        CFRelease(tags);
        return NULL;
    }

    mac_font_table* tables = calloc((size_t)tag_count, sizeof(*tables));
    if (!tables) {
        CFRelease(tags);
        return NULL;
    }
    int count = 0;
    size_t table_data_size = 0;
    for (CFIndex i = 0; i < tag_count; i++) {
        // CTFontCopyAvailableTables stores raw FourCharCodes, not CFNumbers.
        CTFontTableTag tag = (CTFontTableTag)(uintptr_t)CFArrayGetValueAtIndex(tags, i);
        if (!tag) {
            continue;
        }
        CFDataRef data = CTFontCopyTable(ct_font, tag, kCTFontTableOptionNoOptions);
        if (!data) {
            continue;
        }
        CFIndex length = CFDataGetLength(data);
        size_t padded = length > 0 ? ((size_t)length + 3) & ~(size_t)3 : 0;
        if (length <= 0 || length > UINT32_MAX || padded > SIZE_MAX - table_data_size) {
            CFRelease(data);
            continue;
        }
        tables[count].tag = tag;
        tables[count].length = (uint32_t)length;
        tables[count].data = data;
        table_data_size += padded;
        count++;
    }
    CFRelease(tags);
    size_t header_size = 12 + (size_t)count * 16;
    if (count == 0 || table_data_size > UINT32_MAX - header_size) {
        free_tables(tables, count);
        return NULL;
    }
    size_t total = header_size + table_data_size;

    qsort(tables, (size_t)count, sizeof(*tables), compare_tables);
    unsigned char* font_data = calloc(1, total);
    if (!font_data) {
        free_tables(tables, count);
        return NULL;
    }

    uint32_t scaler = 0x00010000;
    for (int i = 0; i < count; i++) {
        if (tables[i].tag == 0x43464620 || tables[i].tag == 0x43464632) {
            scaler = 0x4f54544f;
            break;
        }
    }
    write_be32(font_data, scaler);
    write_be16(font_data + 4, (uint16_t)count);
    int selector = 0;
    int power = 1;
    while (power * 2 <= count) {
        power *= 2;
        selector++;
    }
    write_be16(font_data + 6, (uint16_t)(power * 16));
    write_be16(font_data + 8, (uint16_t)selector);
    write_be16(font_data + 10, (uint16_t)(count * 16 - power * 16));

    size_t offset = header_size;
    size_t head_offset = 0;
    for (int i = 0; i < count; i++) {
        mac_font_table* table = &tables[i];
        table->offset = (uint32_t)offset;
        memcpy(font_data + offset, CFDataGetBytePtr(table->data), table->length);
        if (table->tag == 0x68656164 && table->length >= 12) {
            head_offset = offset;
            memset(font_data + offset + 8, 0, 4);
        }
        unsigned char* entry = font_data + 12 + (size_t)i * 16;
        write_be32(entry, table->tag);
        write_be32(entry + 4, table_checksum(font_data + offset, table->length));
        write_be32(entry + 8, table->offset);
        write_be32(entry + 12, table->length);
        offset += ((size_t)table->length + 3) & ~(size_t)3;
    }
    if (head_offset) {
        write_be32(font_data + head_offset + 8, 0xb1b0afba - table_checksum(font_data, total));
    }

    fz_buffer* buffer = NULL;
    fz_font* font = NULL;
    fz_try(ctx) {
        buffer = fz_new_buffer_from_copied_data(ctx, font_data, total);
        font = fz_new_font_from_buffer(ctx, name, buffer, 0, 0);
    }
    fz_always(ctx) {
        fz_drop_buffer(ctx, buffer);
    }
    fz_catch(ctx) {
        free(font_data);
        free_tables(tables, count);
        fz_rethrow(ctx);
    }
    free(font_data);
    free_tables(tables, count);
    return font;
}

static int font_name_matches(CTFontRef font, CFStringRef wanted) {
    CFStringRef names[3];
    names[0] = CTFontCopyPostScriptName(font);
    names[1] = CTFontCopyFullName(font);
    names[2] = CTFontCopyFamilyName(font);
    int matches = 0;
    for (int i = 0; i < 3; i++) {
        if (names[i] && CFStringCompare(names[i], wanted, kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
            matches = 1;
        }
        if (names[i]) {
            CFRelease(names[i]);
        }
    }
    return matches;
}

static CTFontRef create_named_font(const char* name, int bold, int italic, int exact) {
    const char* wanted = name && *name ? name : "Helvetica";
    if (!strcmp(wanted, "sans-serif")) {
        wanted = "Helvetica";
    } else if (!strcmp(wanted, "serif")) {
        wanted = "Times";
    } else if (!strcmp(wanted, "monospace")) {
        wanted = "Courier";
    }
    CFStringRef font_name = CFStringCreateWithCString(NULL, wanted, kCFStringEncodingUTF8);
    if (!font_name) {
        return NULL;
    }
    CTFontRef font = CTFontCreateWithName(font_name, 12, NULL);
    if (font && exact && !font_name_matches(font, font_name)) {
        CFRelease(font);
        font = NULL;
    }
    CFRelease(font_name);
    if (!font) {
        return NULL;
    }
    CTFontSymbolicTraits traits = 0;
    if (bold) {
        traits |= kCTFontBoldTrait;
    }
    if (italic) {
        traits |= kCTFontItalicTrait;
    }
    if (traits) {
        CTFontRef styled = CTFontCreateCopyWithSymbolicTraits(font, 0, NULL, traits, traits);
        if (styled) {
            CFRelease(font);
            font = styled;
        }
    }
    return font;
}

static CFStringRef string_for_rune(unsigned int rune) {
    UniChar chars[2];
    CFIndex count = 1;
    if (rune <= 0xffff) {
        chars[0] = (UniChar)rune;
    } else {
        rune -= 0x10000;
        chars[0] = (UniChar)(0xd800 + (rune >> 10));
        chars[1] = (UniChar)(0xdc00 + (rune & 0x3ff));
        count = 2;
    }
    return CFStringCreateWithCharacters(NULL, chars, count);
}

static CTFontRef create_fallback_font(int serif, int bold, int italic, unsigned int rune) {
    CTFontRef base = create_named_font(serif ? "Times" : "Helvetica", bold, italic, 0);
    if (!base || !rune) {
        return base;
    }
    CFStringRef text = string_for_rune(rune);
    CTFontRef font = text ? CTFontCreateForString(base, text, CFRangeMake(0, CFStringGetLength(text))) : NULL;
    if (text) {
        CFRelease(text);
    }
    if (!font) {
        return base;
    }
    CFRelease(base);
    return font;
}

static fz_font* load_mac_font(fz_context* ctx, const char* name, int bold, int italic, int exact) {
    CTFontRef font = create_named_font(name, bold, italic, exact);
    if (!font) {
        return NULL;
    }
    fz_font* result = load_ct_font_data(ctx, name, font);
    CFRelease(font);
    return result;
}

static fz_font* load_mac_cjk_font(fz_context* ctx, const char* name, int ordering, int serif) {
    unsigned int rune = 0x4e00;
    if (ordering == FZ_ADOBE_JAPAN) {
        rune = 0x3042;
    } else if (ordering == FZ_ADOBE_KOREA) {
        rune = 0xac00;
    }
    CTFontRef font = name && *name ? create_named_font(name, 0, 0, 0) : create_fallback_font(serif, 0, 0, rune);
    if (!font) {
        return NULL;
    }
    fz_font* result = load_ct_font_data(ctx, name, font);
    CFRelease(font);
    return result;
}

static unsigned int fallback_rune(int script) {
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
        default:
            return 0x0041;
    }
}

static fz_font* load_mac_fallback_font(fz_context* ctx, int script, int language, int serif, int bold, int italic) {
    (void)language;
    CTFontRef font = create_fallback_font(serif, bold, italic, fallback_rune(script));
    if (!font) {
        return NULL;
    }
    fz_font* result = load_ct_font_data(ctx, NULL, font);
    CFRelease(font);
    return result;
}

void install_load_mac_font_funcs(fz_context* ctx) {
    fz_install_load_system_font_funcs(ctx, load_mac_font, load_mac_cjk_font, load_mac_fallback_font);
}

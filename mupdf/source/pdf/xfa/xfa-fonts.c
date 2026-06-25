// Copyright (C) 2004-2026 Artifex Software, Inc.
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

#include "xfa-imp.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define PDF_XFA_MAX_HELD_FONTS 256
#define PDF_XFA_PSMAP_INIT 32

typedef struct pdf_xfa_font_slot pdf_xfa_font_slot;

typedef struct {
    char typeface[FZ_HASH_TABLE_KEY_LENGTH];
    int bold;
    int italic;
    char psname[FZ_HASH_TABLE_KEY_LENGTH];
} pdf_xfa_psmap_entry;

struct pdf_xfa_font_slot {
    fz_font* regular;
    fz_font* bold;
    fz_font* italic;
    fz_font* bold_italic;
};

struct pdf_xfa_fonts {
    fz_hash_table* families;
    fz_hash_table* equates;
    fz_hash_table* embedded;
    pdf_xfa_psmap_entry* psmap;
    int psmap_n;
    int psmap_cap;
    char default_typeface[FZ_HASH_TABLE_KEY_LENGTH];
    fz_font* held[PDF_XFA_MAX_HELD_FONTS];
    int held_n;
};

static void pdf_xfa_fonts_drop_slot(fz_context* ctx, void* val) {
    (void)ctx;
    fz_free(ctx, val);
}

static void pdf_xfa_fonts_hold(fz_context* ctx, pdf_xfa_fonts* fonts, fz_font* font) {
    if (!fonts || !font) return;
    if (fonts->held_n >= PDF_XFA_MAX_HELD_FONTS) return;
    fonts->held[fonts->held_n++] = fz_keep_font(ctx, font);
}

static void pdf_xfa_strip_typeface_quotes(const char* src, char* dst, size_t dstsz) {
    const char* p;
    size_t len;

    if (!dst || dstsz == 0) return;
    dst[0] = 0;
    if (!src) return;

    p = src;
    while (*p == ' ' || *p == '\t') p++;
    if ((*p == '\'' && strchr(p + 1, '\'')) || (*p == '"' && strchr(p + 1, '"'))) {
        char quote = *p++;
        const char* end = strchr(p, quote);
        if (!end) return;
        len = (size_t)(end - p);
        if (len >= dstsz) len = dstsz - 1;
        memcpy(dst, p, len);
        dst[len] = 0;
        return;
    }

    strncpy(dst, p, dstsz - 1);
    dst[dstsz - 1] = 0;
}

static void pdf_xfa_normalize_news_with_key(char* key) {
    char* p;

    if (!key || !key[0]) return;
    p = strstr(key, "newswcomm");
    if (!p) return;
    memmove(p + 10, p + 7, strlen(p + 7) + 1);
    memcpy(p, "newswith", 8);
    memcpy(p + 8, "comm", 4);
}

static void pdf_xfa_family_key(const char* family, char* key, size_t keysz) {
    const char* p;
    size_t n = 0;
    int c;

    if (!key || keysz == 0) return;
    key[0] = 0;
    if (!family) return;

    for (p = family; *p && n + 1 < keysz; p++) {
        if (*p == ',' || *p == '-' || *p == '_' || *p == ' ' || *p == '\t') continue;
        c = tolower((unsigned char)*p);
        key[n++] = (char)c;
    }
    key[n] = 0;
    pdf_xfa_normalize_news_with_key(key);
}

static int pdf_xfa_font_attr_bold(const char* weight) {
    return weight && strcmp(weight, "bold") == 0;
}

static int pdf_xfa_font_attr_italic(const char* posture) {
    return posture && strcmp(posture, "italic") == 0;
}

static int pdf_xfa_typeface_keys_match(const char* a, const char* b) {
    char keya[FZ_HASH_TABLE_KEY_LENGTH];
    char keyb[FZ_HASH_TABLE_KEY_LENGTH];
    char cleana[FZ_HASH_TABLE_KEY_LENGTH];
    char cleanb[FZ_HASH_TABLE_KEY_LENGTH];

    pdf_xfa_strip_typeface_quotes(a, cleana, sizeof cleana);
    pdf_xfa_strip_typeface_quotes(b, cleanb, sizeof cleanb);
    pdf_xfa_family_key(cleana, keya, sizeof keya);
    pdf_xfa_family_key(cleanb, keyb, sizeof keyb);
    return keya[0] && strcmp(keya, keyb) == 0;
}

static pdf_xfa_object* pdf_xfa_fonts_find_child(pdf_xfa_object* node, const char* name) {
    pdf_xfa_object* child;

    if (!node || !name) return NULL;
    for (child = node->first_child; child; child = child->next_sibling) {
        if (child->name && strcmp(child->name, name) == 0) return child;
    }
    return NULL;
}

static void pdf_xfa_fonts_add_equate(fz_context* ctx, fz_pool* pool, pdf_xfa_fonts* fonts, const char* from,
                                     const char* to) {
    char clean_from[FZ_HASH_TABLE_KEY_LENGTH];
    char clean_to[FZ_HASH_TABLE_KEY_LENGTH];
    char key_from[FZ_HASH_TABLE_KEY_LENGTH];
    char* key_to;

    if (!fonts || !fonts->equates || !from || !from[0] || !to || !to[0]) return;

    pdf_xfa_strip_typeface_quotes(from, clean_from, sizeof clean_from);
    pdf_xfa_strip_typeface_quotes(to, clean_to, sizeof clean_to);
    pdf_xfa_family_key(clean_from, key_from, sizeof key_from);
    if (!key_from[0]) return;

    key_to = fz_pool_strdup(ctx, pool, clean_to);
    fz_hash_insert(ctx, fonts->equates, key_from, key_to);
}

static void pdf_xfa_fonts_add_psmap(fz_context* ctx, pdf_xfa_fonts* fonts, const char* typeface, int bold, int italic,
                                    const char* psname) {
    char clean[FZ_HASH_TABLE_KEY_LENGTH];
    pdf_xfa_psmap_entry* entry;

    if (!fonts || !typeface || !typeface[0] || !psname || !psname[0]) return;

    if (fonts->psmap_n == fonts->psmap_cap) {
        int newcap = fonts->psmap_cap ? fonts->psmap_cap * 2 : PDF_XFA_PSMAP_INIT;
        pdf_xfa_psmap_entry* newmap = fz_malloc_array(ctx, newcap, pdf_xfa_psmap_entry);
        if (fonts->psmap) {
            memcpy(newmap, fonts->psmap, sizeof(*newmap) * fonts->psmap_n);
            fz_free(ctx, fonts->psmap);
        }
        fonts->psmap = newmap;
        fonts->psmap_cap = newcap;
    }

    entry = &fonts->psmap[fonts->psmap_n++];
    memset(entry, 0, sizeof(*entry));
    pdf_xfa_strip_typeface_quotes(typeface, clean, sizeof clean);
    strncpy(entry->typeface, clean, sizeof(entry->typeface) - 1);
    entry->bold = bold;
    entry->italic = italic;
    strncpy(entry->psname, psname, sizeof(entry->psname) - 1);
}

static void pdf_xfa_fonts_set_default_typeface(pdf_xfa_fonts* fonts, const char* typeface, int wildcard) {
    if (!fonts || !typeface || !typeface[0]) return;
    if (wildcard || !fonts->default_typeface[0])
        pdf_xfa_strip_typeface_quotes(typeface, fonts->default_typeface, sizeof(fonts->default_typeface));
}

static void pdf_xfa_fonts_walk_default_typefaces(fz_context* ctx, pdf_xfa_fonts* fonts, pdf_xfa_object* node) {
    pdf_xfa_object* child;
    char* script;
    const char* face;
    int wildcard;

    if (!node) return;

    if (node->name && strcmp(node->name, "defaultTypeface") == 0) {
        script = pdf_xfa_object_get_attr(ctx, node, "writingScript");
        wildcard = !script || !script[0] || strcmp(script, "*") == 0;
        face = node->content;
        if (!face || !face[0]) face = pdf_xfa_object_text(ctx, node);
        pdf_xfa_fonts_set_default_typeface(fonts, face, wildcard);
    }

    for (child = node->first_child; child; child = child->next_sibling)
        pdf_xfa_fonts_walk_default_typefaces(ctx, fonts, child);
}

static void pdf_xfa_fonts_walk_equates(fz_context* ctx, fz_pool* pool, pdf_xfa_fonts* fonts, pdf_xfa_object* node) {
    pdf_xfa_object* child;
    char* from;
    char* to;

    if (!node) return;

    if (node->name && strcmp(node->name, "equate") == 0) {
        from = pdf_xfa_object_get_attr(ctx, node, "from");
        to = pdf_xfa_object_get_attr(ctx, node, "to");
        pdf_xfa_fonts_add_equate(ctx, pool, fonts, from, to);
    }

    for (child = node->first_child; child; child = child->next_sibling)
        pdf_xfa_fonts_walk_equates(ctx, pool, fonts, child);
}

static void pdf_xfa_fonts_parse_psmap(fz_context* ctx, pdf_xfa_fonts* fonts, pdf_xfa_object* config) {
    pdf_xfa_object* psmap;
    pdf_xfa_object* child;
    char* typeface;
    char* psname;
    char* weight;
    char* posture;

    psmap = pdf_xfa_fonts_find_child(config, "psMap");
    if (!psmap) return;

    for (child = psmap->first_child; child; child = child->next_sibling) {
        if (!child->name || strcmp(child->name, "font") != 0) continue;
        typeface = pdf_xfa_object_get_attr(ctx, child, "typeface");
        psname = pdf_xfa_object_get_attr(ctx, child, "psName");
        if (!psname || !psname[0]) psname = pdf_xfa_object_get_attr(ctx, child, "psname");
        if (!typeface || !typeface[0] || !psname || !psname[0]) continue;
        weight = pdf_xfa_object_get_attr(ctx, child, "weight");
        posture = pdf_xfa_object_get_attr(ctx, child, "posture");
        pdf_xfa_fonts_add_psmap(ctx, fonts, typeface, pdf_xfa_font_attr_bold(weight), pdf_xfa_font_attr_italic(posture),
                                psname);
    }
}

static void pdf_xfa_fonts_parse_config_packet(fz_context* ctx, fz_pool* pool, pdf_xfa_fonts* fonts,
                                              pdf_xfa_packet* packets) {
    pdf_xfa_packet* packet;
    pdf_xfa_object* config = NULL;

    if (!fonts || !packets) return;

    for (packet = packets; packet; packet = packet->next) {
        if (!packet->name || !packet->data || strcmp(packet->name, "config") != 0) continue;

        fz_try(ctx) {
            config = pdf_xfa_parse_xml(ctx, pool, packet->data, PDF_XFA_NS_CONFIG, 0, NULL, NULL);
            if (config) {
                pdf_xfa_fonts_walk_equates(ctx, pool, fonts, config);
                pdf_xfa_fonts_walk_default_typefaces(ctx, fonts, config);
                pdf_xfa_fonts_parse_psmap(ctx, fonts, config);
            }
        }
        fz_catch(ctx) fz_warn(ctx, "XFA: could not parse config fontInfo");
        break;
    }
}

static void pdf_xfa_fonts_map_typeface(fz_context* ctx, pdf_xfa_fonts* fonts, const char* typeface, char* out,
                                       size_t outsz) {
    char clean[FZ_HASH_TABLE_KEY_LENGTH];
    char key[FZ_HASH_TABLE_KEY_LENGTH];
    const char* mapped;

    if (!out || outsz == 0) return;
    out[0] = 0;
    if (!typeface || !typeface[0]) return;

    pdf_xfa_strip_typeface_quotes(typeface, clean, sizeof clean);
    if (fonts && fonts->equates) {
        pdf_xfa_family_key(clean, key, sizeof key);
        if (key[0]) {
            mapped = (const char*)fz_hash_find(ctx, fonts->equates, key);
            if (mapped && mapped[0]) {
                strncpy(out, mapped, outsz - 1);
                out[outsz - 1] = 0;
                return;
            }
        }
    }

    strncpy(out, clean, outsz - 1);
    out[outsz - 1] = 0;
}

static const char* pdf_xfa_fonts_psmap_lookup(pdf_xfa_fonts* fonts, const char* typeface, int bold, int italic) {
    int i;

    if (!fonts || !typeface || !typeface[0]) return NULL;
    for (i = 0; i < fonts->psmap_n; i++) {
        pdf_xfa_psmap_entry* entry = &fonts->psmap[i];
        if (entry->bold != bold || entry->italic != italic) continue;
        if (pdf_xfa_typeface_keys_match(entry->typeface, typeface)) return entry->psname;
    }
    return NULL;
}

static pdf_xfa_font_slot* pdf_xfa_fonts_slot_for_family(fz_context* ctx, pdf_xfa_fonts* fonts, const char* family) {
    char key[FZ_HASH_TABLE_KEY_LENGTH];
    pdf_xfa_font_slot* slot;

    if (!fonts || !family || !family[0]) return NULL;

    pdf_xfa_family_key(family, key, sizeof key);
    if (!key[0]) return NULL;

    slot = fz_hash_find(ctx, fonts->families, key);
    if (slot) return slot;

    slot = fz_malloc_struct(ctx, pdf_xfa_font_slot);
    fz_try(ctx) fz_hash_insert(ctx, fonts->families, key, slot);
    fz_catch(ctx) {
        fz_free(ctx, slot);
        fz_rethrow(ctx);
    }
    return slot;
}

static void pdf_xfa_fonts_add_variant(fz_context* ctx, pdf_xfa_fonts* fonts, const char* family, int bold, int italic,
                                      fz_font* font) {
    pdf_xfa_font_slot* slot;
    fz_font** target;

    if (!fonts || !font || !family || !family[0]) return;

    slot = pdf_xfa_fonts_slot_for_family(ctx, fonts, family);
    if (!slot) return;

    if (bold && italic)
        target = &slot->bold_italic;
    else if (bold)
        target = &slot->bold;
    else if (italic)
        target = &slot->italic;
    else
        target = &slot->regular;

    if (!*target) {
        *target = fz_keep_font(ctx, font);
        pdf_xfa_fonts_hold(ctx, fonts, font);
    }
}

static int pdf_xfa_font_name_has_suffix(const char* name, const char* suffix) {
    size_t nlen, slen;
    const char* p;

    if (!name || !suffix) return 0;
    nlen = strlen(name);
    slen = strlen(suffix);
    if (nlen < slen) return 0;
    p = name + nlen - slen;
    if (strcmp(p, suffix) != 0) return 0;
    return p == name || *(p - 1) == '-';
}

static int pdf_xfa_font_name_is_bold(const char* name) {
    if (!name || !name[0]) return 0;
    if (strstr(name, "Bold") || strstr(name, "bold")) return 1;
    if (strstr(name, "-Bd") || strstr(name, "-Blk") || strstr(name, "-Demi")) return 1;
    if (pdf_xfa_font_name_has_suffix(name, "Bd") || pdf_xfa_font_name_has_suffix(name, "Blk") ||
        pdf_xfa_font_name_has_suffix(name, "Demi") || pdf_xfa_font_name_has_suffix(name, "Heavy") ||
        pdf_xfa_font_name_has_suffix(name, "Black"))
        return 1;
    return 0;
}

static int pdf_xfa_font_name_is_italic(const char* name) {
    if (!name || !name[0]) return 0;
    if (strstr(name, "Italic") || strstr(name, "Oblique")) return 1;
    if (pdf_xfa_font_name_has_suffix(name, "It")) return 1;
    return 0;
}

static int pdf_xfa_font_is_bold(fz_context* ctx, pdf_obj* fontdict, pdf_font_desc* fontdesc) {
    pdf_obj* descriptor;
    int weight;
    const char* name;

    descriptor = pdf_dict_get(ctx, fontdict, PDF_NAME(FontDescriptor));
    if (descriptor) {
        pdf_obj* weightobj = pdf_dict_getp(ctx, descriptor, "FontWeight");
        if (weightobj) {
            weight = pdf_to_int(ctx, weightobj);
            if (weight >= 700) return 1;
        }
    }

    name = fontdesc && fontdesc->font && fontdesc->font->name ? fontdesc->font->name : NULL;
    if (pdf_xfa_font_name_is_bold(name)) return 1;

    if (fontdict) {
        pdf_obj* basefontobj = pdf_dict_get(ctx, fontdict, PDF_NAME(BaseFont));
        if (basefontobj) {
            name = pdf_to_name(ctx, basefontobj);
            if (pdf_xfa_font_name_is_bold(name)) return 1;
        }
    }
    return 0;
}

static int pdf_xfa_font_is_italic(fz_context* ctx, pdf_obj* fontdict, pdf_font_desc* fontdesc) {
    pdf_obj* descriptor;
    int angle;
    const char* name;

    descriptor = pdf_dict_get(ctx, fontdict, PDF_NAME(FontDescriptor));
    if (descriptor) {
        angle = pdf_dict_get_int(ctx, descriptor, PDF_NAME(ItalicAngle));
        if (angle != 0) return 1;
    }

    name = fontdesc && fontdesc->font && fontdesc->font->name ? fontdesc->font->name : NULL;
    if (pdf_xfa_font_name_is_italic(name)) return 1;

    if (fontdict) {
        pdf_obj* basefontobj = pdf_dict_get(ctx, fontdict, PDF_NAME(BaseFont));
        if (basefontobj) {
            name = pdf_to_name(ctx, basefontobj);
            if (pdf_xfa_font_name_is_italic(name)) return 1;
        }
    }
    return 0;
}

static void pdf_xfa_fonts_strip_subset_prefix(const char* name, char* out, size_t outsz) {
    const char* plus;

    if (!out || outsz == 0) return;
    out[0] = 0;
    if (!name || !name[0]) return;

    plus = strchr(name, '+');
    if (plus && plus[1])
        strncpy(out, plus + 1, outsz - 1);
    else
        strncpy(out, name, outsz - 1);
    out[outsz - 1] = 0;
}

static char* pdf_xfa_font_family_name(fz_context* ctx, fz_pool* pool, pdf_obj* fontdict, pdf_font_desc* fontdesc) {
    pdf_obj* descriptor;
    const char* family;
    pdf_obj* basefont;

    descriptor = pdf_dict_get(ctx, fontdict, PDF_NAME(FontDescriptor));
    if (descriptor) {
        pdf_obj* familyobj = pdf_dict_getp(ctx, descriptor, "FontFamily");
        if (familyobj) {
            family = pdf_to_text_string(ctx, familyobj);
            if (family && family[0]) return fz_pool_strdup(ctx, pool, family);
        }
    }

    basefont = pdf_dict_get(ctx, fontdict, PDF_NAME(BaseFont));
    if (basefont) {
        const char* name = pdf_to_name(ctx, basefont);
        if (name && name[0]) return fz_pool_strdup(ctx, pool, name);
    }

    if (fontdesc && fontdesc->font && fontdesc->font->name && fontdesc->font->name[0])
        return fz_pool_strdup(ctx, pool, fontdesc->font->name);

    return NULL;
}

static void pdf_xfa_fonts_variant_key(const char* family, int bold, int italic, char* variant_key, size_t keysz) {
    char key[FZ_HASH_TABLE_KEY_LENGTH];

    memset(variant_key, 0, keysz);
    memset(key, 0, sizeof key);
    pdf_xfa_family_key(family, key, sizeof key);
    if (!key[0]) {
        variant_key[0] = 0;
        return;
    }
    snprintf(variant_key, keysz, "%s:%d:%d", key, bold, italic);
}

static void pdf_xfa_fonts_mark_embedded_variant(fz_context* ctx, pdf_xfa_fonts* fonts, const char* typeface, int bold,
                                                int italic) {
    char clean[FZ_HASH_TABLE_KEY_LENGTH];
    char variant_key[FZ_HASH_TABLE_KEY_LENGTH];

    if (!fonts || !fonts->embedded || !typeface || !typeface[0]) return;

    pdf_xfa_strip_typeface_quotes(typeface, clean, sizeof clean);
    pdf_xfa_fonts_variant_key(clean, bold, italic, variant_key, sizeof variant_key);
    if (!variant_key[0]) return;
    fz_hash_insert(ctx, fonts->embedded, variant_key, (void*)1);
}

static int pdf_xfa_fonts_psname_match(const char* a, const char* b) {
    char clean_a[FZ_HASH_TABLE_KEY_LENGTH];
    char clean_b[FZ_HASH_TABLE_KEY_LENGTH];

    if (!a || !a[0] || !b || !b[0]) return 0;
    pdf_xfa_strip_typeface_quotes(a, clean_a, sizeof clean_a);
    pdf_xfa_strip_typeface_quotes(b, clean_b, sizeof clean_b);
    if (strcmp(clean_a, clean_b) == 0) return 1;
    return pdf_xfa_typeface_keys_match(clean_a, clean_b);
}

static void pdf_xfa_fonts_mark_psmap_embedded(fz_context* ctx, pdf_xfa_fonts* fonts, const char* stripped) {
    int i;

    if (!fonts || !stripped || !stripped[0]) return;
    for (i = 0; i < fonts->psmap_n; i++) {
        pdf_xfa_psmap_entry* entry = &fonts->psmap[i];
        if (pdf_xfa_fonts_psname_match(entry->psname, stripped))
            pdf_xfa_fonts_mark_embedded_variant(ctx, fonts, entry->typeface, entry->bold, entry->italic);
    }
}

static void pdf_xfa_fonts_note_pdf_fontobj(fz_context* ctx, pdf_xfa_fonts* fonts, pdf_obj* fontobj) {
    char stripped[FZ_HASH_TABLE_KEY_LENGTH];
    const char* basefont_name;
    pdf_obj* basefontobj;
    pdf_obj* dfonts;
    int bold, italic;

    if (!fonts || !fontobj) return;

    fontobj = pdf_resolve_indirect(ctx, fontobj);
    basefontobj = pdf_dict_get(ctx, fontobj, PDF_NAME(BaseFont));
    if (!basefontobj) {
        dfonts = pdf_dict_get(ctx, fontobj, PDF_NAME(DescendantFonts));
        if (dfonts && pdf_array_len(ctx, dfonts) > 0)
            basefontobj = pdf_dict_get(ctx, pdf_array_get(ctx, dfonts, 0), PDF_NAME(BaseFont));
    }
    if (!basefontobj) return;

    basefont_name = pdf_to_name(ctx, basefontobj);
    if (!basefont_name || !basefont_name[0]) return;

    bold = pdf_xfa_font_is_bold(ctx, fontobj, NULL);
    italic = pdf_xfa_font_is_italic(ctx, fontobj, NULL);
    pdf_xfa_fonts_strip_subset_prefix(basefont_name, stripped, sizeof stripped);
    if (stripped[0]) {
        pdf_xfa_fonts_mark_embedded_variant(ctx, fonts, stripped, bold, italic);
        pdf_xfa_fonts_mark_psmap_embedded(ctx, fonts, stripped);
    }
}

static void pdf_xfa_fonts_register_psmap_aliases(fz_context* ctx, pdf_xfa_fonts* fonts, const char* stripped, int bold,
                                                 int italic, fz_font* font) {
    int i;

    if (!fonts || !stripped || !stripped[0] || !font) return;
    for (i = 0; i < fonts->psmap_n; i++) {
        pdf_xfa_psmap_entry* entry = &fonts->psmap[i];
        if (pdf_xfa_typeface_keys_match(entry->psname, stripped))
            pdf_xfa_fonts_add_variant(ctx, fonts, entry->typeface, entry->bold, entry->italic, font);
    }
}

static void pdf_xfa_fonts_register_loaded(fz_context* ctx, fz_pool* pool, pdf_xfa_fonts* fonts, pdf_obj* fontobj,
                                          pdf_font_desc* fontdesc) {
    char* family;
    char stripped[FZ_HASH_TABLE_KEY_LENGTH];
    const char* basefont_name;
    pdf_obj* basefontobj;
    pdf_obj* descriptor;
    int bold, italic;

    if (!fonts || !fontobj || !fontdesc || !fontdesc->font) return;

    family = pdf_xfa_font_family_name(ctx, pool, fontobj, fontdesc);
    bold = pdf_xfa_font_is_bold(ctx, fontobj, fontdesc);
    italic = pdf_xfa_font_is_italic(ctx, fontobj, fontdesc);

    if (family && family[0]) pdf_xfa_fonts_add_variant(ctx, fonts, family, bold, italic, fontdesc->font);

    basefontobj = pdf_dict_get(ctx, fontobj, PDF_NAME(BaseFont));
    if (basefontobj) {
        basefont_name = pdf_to_name(ctx, basefontobj);
        if (basefont_name && basefont_name[0]) {
            pdf_xfa_fonts_add_variant(ctx, fonts, basefont_name, bold, italic, fontdesc->font);
            pdf_xfa_fonts_strip_subset_prefix(basefont_name, stripped, sizeof stripped);
            if (stripped[0] && !pdf_xfa_typeface_keys_match(stripped, basefont_name))
                pdf_xfa_fonts_add_variant(ctx, fonts, stripped, bold, italic, fontdesc->font);
            if (stripped[0]) pdf_xfa_fonts_register_psmap_aliases(ctx, fonts, stripped, bold, italic, fontdesc->font);
        }
    }

    descriptor = pdf_dict_get(ctx, fontobj, PDF_NAME(FontDescriptor));
    if (descriptor) {
        pdf_obj* fontnameobj = pdf_dict_get(ctx, descriptor, PDF_NAME(FontName));
        if (fontnameobj) {
            const char* fontname = pdf_to_name(ctx, fontnameobj);
            if (fontname && fontname[0]) {
                pdf_xfa_fonts_strip_subset_prefix(fontname, stripped, sizeof stripped);
                pdf_xfa_fonts_add_variant(ctx, fonts, stripped, bold, italic, fontdesc->font);
                pdf_xfa_fonts_register_psmap_aliases(ctx, fonts, stripped, bold, italic, fontdesc->font);
            }
        }
    }
}

static void pdf_xfa_fonts_load_font_dict(fz_context* ctx, pdf_document* doc, fz_pool* pool, pdf_xfa_fonts* fonts,
                                         pdf_obj* fontdict, const char* warn) {
    int i, n;

    if (!fontdict) return;

    n = pdf_dict_len(ctx, fontdict);
    for (i = 0; i < n; i++) {
        pdf_obj* fontobj = pdf_dict_get_val(ctx, fontdict, i);
        pdf_font_desc* fontdesc = NULL;

        pdf_xfa_fonts_note_pdf_fontobj(ctx, fonts, fontobj);
        fz_try(ctx) {
            fontdesc = pdf_load_font(ctx, doc, NULL, fontobj);
            pdf_xfa_fonts_register_loaded(ctx, pool, fonts, fontobj, fontdesc);
        }
        fz_always(ctx) pdf_drop_font(ctx, fontdesc);
        fz_catch(ctx) {
            if (warn) fz_warn(ctx, "%s", warn);
        }
    }
}

static void pdf_xfa_fonts_load_dr(fz_context* ctx, pdf_document* doc, fz_pool* pool, pdf_xfa_fonts* fonts) {
    pdf_obj* root;
    pdf_obj* acro;
    pdf_obj* dr;
    pdf_obj* fontdict;

    root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
    acro = pdf_dict_get(ctx, root, PDF_NAME(AcroForm));
    dr = pdf_dict_get(ctx, acro, PDF_NAME(DR));
    fontdict = pdf_dict_get(ctx, dr, PDF_NAME(Font));
    pdf_xfa_fonts_load_font_dict(ctx, doc, pool, fonts, fontdict, "XFA: could not load AcroForm DR font");
}

static void pdf_xfa_fonts_load_page_node(fz_context* ctx, pdf_document* doc, fz_pool* pool, pdf_xfa_fonts* fonts,
                                         pdf_obj* node) {
    pdf_obj* type;
    pdf_obj* kids;
    pdf_obj* resources;
    pdf_obj* fontdict;
    int i, n;

    if (!node) return;

    type = pdf_dict_get(ctx, node, PDF_NAME(Type));
    if (pdf_name_eq(ctx, type, PDF_NAME(Pages))) {
        kids = pdf_dict_get(ctx, node, PDF_NAME(Kids));
        n = pdf_array_len(ctx, kids);
        for (i = 0; i < n; i++) pdf_xfa_fonts_load_page_node(ctx, doc, pool, fonts, pdf_array_get(ctx, kids, i));
        return;
    }

    if (pdf_name_eq(ctx, type, PDF_NAME(Page))) {
        resources = pdf_dict_get_inheritable(ctx, node, PDF_NAME(Resources));
        fontdict = pdf_dict_get(ctx, resources, PDF_NAME(Font));
        pdf_xfa_fonts_load_font_dict(ctx, doc, pool, fonts, fontdict, "XFA: could not load page resource font");
    }
}

static void pdf_xfa_fonts_scan_embedded_font_dict(fz_context* ctx, pdf_xfa_fonts* fonts, pdf_obj* fontdict) {
    int i, n;

    if (!fontdict) return;

    n = pdf_dict_len(ctx, fontdict);
    for (i = 0; i < n; i++) pdf_xfa_fonts_note_pdf_fontobj(ctx, fonts, pdf_dict_get_val(ctx, fontdict, i));
}

static void pdf_xfa_fonts_scan_embedded_page_node(fz_context* ctx, pdf_xfa_fonts* fonts, pdf_obj* node) {
    pdf_obj* type;
    pdf_obj* kids;
    pdf_obj* resources;
    pdf_obj* fontdict;
    int i, n;

    if (!node) return;

    type = pdf_dict_get(ctx, node, PDF_NAME(Type));
    if (pdf_name_eq(ctx, type, PDF_NAME(Pages))) {
        kids = pdf_dict_get(ctx, node, PDF_NAME(Kids));
        n = pdf_array_len(ctx, kids);
        for (i = 0; i < n; i++) pdf_xfa_fonts_scan_embedded_page_node(ctx, fonts, pdf_array_get(ctx, kids, i));
        return;
    }

    if (pdf_name_eq(ctx, type, PDF_NAME(Page))) {
        resources = pdf_dict_get_inheritable(ctx, node, PDF_NAME(Resources));
        fontdict = pdf_dict_get(ctx, resources, PDF_NAME(Font));
        pdf_xfa_fonts_scan_embedded_font_dict(ctx, fonts, fontdict);
    }
}

static void pdf_xfa_fonts_scan_embedded_dr(fz_context* ctx, pdf_xfa_fonts* fonts, pdf_document* doc) {
    pdf_obj* root;
    pdf_obj* acro;
    pdf_obj* dr;
    pdf_obj* fontdict;

    root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
    acro = pdf_dict_get(ctx, root, PDF_NAME(AcroForm));
    dr = pdf_dict_get(ctx, acro, PDF_NAME(DR));
    fontdict = pdf_dict_get(ctx, dr, PDF_NAME(Font));
    pdf_xfa_fonts_scan_embedded_font_dict(ctx, fonts, fontdict);
}

static int pdf_xfa_fonts_fontdict_has_psname(fz_context* ctx, pdf_xfa_fonts* fonts, pdf_obj* fontdict,
                                             const char* psname) {
    char stripped[FZ_HASH_TABLE_KEY_LENGTH];
    const char* basefont_name;
    pdf_obj* basefontobj;
    pdf_obj* dfonts;
    int i, n;

    if (!fontdict || !psname || !psname[0]) return 0;

    n = pdf_dict_len(ctx, fontdict);
    for (i = 0; i < n; i++) {
        pdf_obj* fontobj = pdf_resolve_indirect(ctx, pdf_dict_get_val(ctx, fontdict, i));
        basefontobj = pdf_dict_get(ctx, fontobj, PDF_NAME(BaseFont));
        if (!basefontobj) {
            dfonts = pdf_dict_get(ctx, fontobj, PDF_NAME(DescendantFonts));
            if (dfonts && pdf_array_len(ctx, dfonts) > 0)
                basefontobj = pdf_dict_get(ctx, pdf_array_get(ctx, dfonts, 0), PDF_NAME(BaseFont));
        }
        if (!basefontobj) continue;
        basefont_name = pdf_to_name(ctx, basefontobj);
        pdf_xfa_fonts_strip_subset_prefix(basefont_name, stripped, sizeof stripped);
        if (pdf_xfa_fonts_psname_match(psname, stripped)) return 1;
        if (strstr(basefont_name, psname) || (stripped[0] && strstr(stripped, psname))) return 1;
    }
    return 0;
}

static int pdf_xfa_fonts_page_node_has_psname(fz_context* ctx, pdf_xfa_fonts* fonts, pdf_obj* node,
                                              const char* psname) {
    pdf_obj* type;
    pdf_obj* kids;
    pdf_obj* resources;
    pdf_obj* fontdict;
    int i, n;

    if (!node) return 0;

    type = pdf_dict_get(ctx, node, PDF_NAME(Type));
    if (pdf_name_eq(ctx, type, PDF_NAME(Pages))) {
        kids = pdf_dict_get(ctx, node, PDF_NAME(Kids));
        n = pdf_array_len(ctx, kids);
        for (i = 0; i < n; i++)
            if (pdf_xfa_fonts_page_node_has_psname(ctx, fonts, pdf_array_get(ctx, kids, i), psname)) return 1;
        return 0;
    }

    if (pdf_name_eq(ctx, type, PDF_NAME(Page))) {
        resources = pdf_dict_get_inheritable(ctx, node, PDF_NAME(Resources));
        fontdict = pdf_dict_get(ctx, resources, PDF_NAME(Font));
        if (pdf_xfa_fonts_fontdict_has_psname(ctx, fonts, fontdict, psname)) return 1;
    }
    return 0;
}

static void pdf_xfa_fonts_ensure_psmap_embedded(fz_context* ctx, pdf_document* doc, pdf_xfa_fonts* fonts) {
    pdf_obj* root;
    pdf_obj* acro;
    pdf_obj* dr;
    pdf_obj* fontdict;
    pdf_obj* pages;
    int i;

    if (!fonts || !fonts->embedded) return;

    root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
    acro = pdf_dict_get(ctx, root, PDF_NAME(AcroForm));
    dr = pdf_dict_get(ctx, acro, PDF_NAME(DR));
    fontdict = pdf_dict_get(ctx, dr, PDF_NAME(Font));
    pages = pdf_dict_get(ctx, root, PDF_NAME(Pages));

    for (i = 0; i < fonts->psmap_n; i++) {
        pdf_xfa_psmap_entry* entry = &fonts->psmap[i];
        char variant_key[FZ_HASH_TABLE_KEY_LENGTH];
        int found;

        pdf_xfa_fonts_variant_key(entry->typeface, entry->bold, entry->italic, variant_key, sizeof variant_key);
        if (variant_key[0] && fz_hash_find(ctx, fonts->embedded, variant_key)) continue;

        found = pdf_xfa_fonts_fontdict_has_psname(ctx, fonts, fontdict, entry->psname);
        if (!found && pages) found = pdf_xfa_fonts_page_node_has_psname(ctx, fonts, pages, entry->psname);
        if (found) pdf_xfa_fonts_mark_embedded_variant(ctx, fonts, entry->typeface, entry->bold, entry->italic);
    }
}

static void pdf_xfa_fonts_scan_embedded_resources(fz_context* ctx, pdf_document* doc, pdf_xfa_fonts* fonts) {
    pdf_obj* root;
    pdf_obj* pages;

    pdf_xfa_fonts_scan_embedded_dr(ctx, fonts, doc);
    root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
    pages = pdf_dict_get(ctx, root, PDF_NAME(Pages));
    if (!pages) return;

    fz_try(ctx) pdf_xfa_fonts_scan_embedded_page_node(ctx, fonts, pages);
    fz_catch(ctx) fz_warn(ctx, "XFA: could not scan embedded fonts from page tree");
}

static void pdf_xfa_fonts_load_page_resources(fz_context* ctx, pdf_document* doc, fz_pool* pool, pdf_xfa_fonts* fonts) {
    pdf_obj* root;
    pdf_obj* pages;

    root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
    pages = pdf_dict_get(ctx, root, PDF_NAME(Pages));
    if (!pages) return;

    fz_try(ctx) pdf_xfa_fonts_load_page_node(ctx, doc, pool, fonts, pages);
    fz_catch(ctx) fz_warn(ctx, "XFA: could not load fonts from page tree");
}

static fz_font* pdf_xfa_fonts_base14(fz_context* ctx, pdf_xfa_fonts* fonts, const char* typeface, int bold,
                                     int italic) {
    char clean[FZ_HASH_TABLE_KEY_LENGTH];
    const char* face;

    pdf_xfa_strip_typeface_quotes(typeface, clean, sizeof clean);
    pdf_xfa_family_key(clean, clean, sizeof clean);

    if (strstr(clean, "courier") == clean) {
        if (bold && italic)
            face = "Courier-BoldOblique";
        else if (bold)
            face = "Courier-Bold";
        else if (italic)
            face = "Courier-Oblique";
        else
            face = "Courier";
    } else if (strstr(clean, "times") == clean) {
        if (bold && italic)
            face = "Times-BoldItalic";
        else if (bold)
            face = "Times-Bold";
        else if (italic)
            face = "Times-Italic";
        else
            face = "Times-Roman";
    } else if (strstr(clean, "helvetica") == clean || strstr(clean, "arial") == clean ||
               strstr(clean, "myriad") == clean || strstr(clean, "segoe") == clean ||
               strstr(clean, "calibri") == clean) {
        if (bold && italic)
            face = "Helvetica-BoldOblique";
        else if (bold)
            face = "Helvetica-Bold";
        else if (italic)
            face = "Helvetica-Oblique";
        else
            face = "Helvetica";
    } else
        return NULL;

    {
        fz_font* font = fz_new_base14_font(ctx, face);
        pdf_xfa_fonts_hold(ctx, fonts, font);
        return font;
    }
}

void pdf_xfa_fonts_register_typeface(fz_context* ctx, pdf_xfa_global_data* global, const char* typeface) {
    char clean[FZ_HASH_TABLE_KEY_LENGTH];
    char key[FZ_HASH_TABLE_KEY_LENGTH];

    if (!global || !global->used_typefaces || !typeface || !typeface[0]) return;

    pdf_xfa_strip_typeface_quotes(typeface, clean, sizeof clean);
    pdf_xfa_family_key(clean, key, sizeof key);
    if (!key[0]) return;
    fz_hash_insert(ctx, global->used_typefaces, key, (void*)1);
}

pdf_xfa_fonts* pdf_xfa_fonts_load(fz_context* ctx, pdf_document* doc, fz_pool* pool, pdf_xfa_packet* packets) {
    pdf_xfa_fonts* fonts = fz_malloc_struct(ctx, pdf_xfa_fonts);

    fz_try(ctx) {
        fonts->families = fz_new_hash_table(ctx, 64, FZ_HASH_TABLE_KEY_LENGTH, -1, pdf_xfa_fonts_drop_slot);
        fonts->equates = fz_new_hash_table(ctx, 32, FZ_HASH_TABLE_KEY_LENGTH, -1, NULL);
        fonts->embedded = fz_new_hash_table(ctx, 64, FZ_HASH_TABLE_KEY_LENGTH, -1, NULL);
        pdf_xfa_fonts_parse_config_packet(ctx, pool, fonts, packets);
        pdf_xfa_fonts_load_dr(ctx, doc, pool, fonts);
        pdf_xfa_fonts_load_page_resources(ctx, doc, pool, fonts);
        pdf_xfa_fonts_scan_embedded_resources(ctx, doc, fonts);
        fz_try(ctx) pdf_xfa_fonts_ensure_psmap_embedded(ctx, doc, fonts);
        fz_catch(ctx) fz_warn(ctx, "XFA: could not finalize psMap embedded fonts");
    }
    fz_catch(ctx) {
        pdf_xfa_fonts_drop(ctx, fonts);
        fz_rethrow(ctx);
    }

    return fonts;
}

void pdf_xfa_fonts_drop(fz_context* ctx, pdf_xfa_fonts* fonts) {
    int i;

    if (!fonts) return;
    for (i = 0; i < fonts->held_n; i++) fz_drop_font(ctx, fonts->held[i]);
    fz_drop_hash_table(ctx, fonts->embedded);
    fz_drop_hash_table(ctx, fonts->equates);
    fz_drop_hash_table(ctx, fonts->families);
    fz_free(ctx, fonts->psmap);
    fz_free(ctx, fonts);
}

typedef struct {
    const char* key;
    pdf_xfa_font_slot* slot;
} pdf_xfa_font_find_state;

static void pdf_xfa_fonts_find_prefix_cb(fz_context* ctx, void* state, void* key, int keylen, void* val) {
    pdf_xfa_font_find_state* st = (pdf_xfa_font_find_state*)state;
    size_t want;

    (void)ctx;
    if (st->slot || !st->key) return;
    want = strlen(st->key);
    if ((size_t)keylen >= want && memcmp(key, st->key, want) == 0) st->slot = (pdf_xfa_font_slot*)val;
}

static pdf_xfa_font_slot* pdf_xfa_fonts_find_slot(fz_context* ctx, pdf_xfa_fonts* fonts, const char* typeface) {
    char clean[FZ_HASH_TABLE_KEY_LENGTH];
    char key[FZ_HASH_TABLE_KEY_LENGTH];
    pdf_xfa_font_slot* slot;
    pdf_xfa_font_find_state state;

    if (!fonts || !typeface || !typeface[0]) return NULL;

    pdf_xfa_strip_typeface_quotes(typeface, clean, sizeof clean);
    pdf_xfa_family_key(clean, key, sizeof key);
    if (!key[0]) return NULL;

    slot = fz_hash_find(ctx, fonts->families, key);
    if (slot) return slot;

    state.key = key;
    state.slot = NULL;
    fz_hash_for_each(ctx, fonts->families, &state, pdf_xfa_fonts_find_prefix_cb);
    return state.slot;
}

typedef struct {
    int count;
} pdf_xfa_font_count_state;

static void pdf_xfa_fonts_count_cb(fz_context* ctx, void* state, void* key, int keylen, void* val) {
    pdf_xfa_font_count_state* st = (pdf_xfa_font_count_state*)state;
    (void)ctx;
    (void)key;
    (void)keylen;
    (void)val;
    st->count++;
}

static void pdf_xfa_fonts_check_used_walk(fz_context* ctx, pdf_xfa_fonts* fonts, fz_hash_table* warned,
                                          pdf_xfa_object* node, int* missing_out, fz_buffer* missing_names);

void pdf_xfa_fonts_stats(fz_context* ctx, pdf_xfa_fonts* fonts, pdf_xfa_object* form, int* families_out, int* held_out,
                         int* missing_out, fz_buffer* missing_names) {
    pdf_xfa_font_count_state state = {0};
    int missing = 0;
    fz_hash_table* warned = NULL;

    if (families_out) *families_out = 0;
    if (held_out) *held_out = 0;
    if (missing_out) *missing_out = 0;
    if (!fonts) return;

    if (families_out && fonts->families && ctx) {
        fz_hash_for_each(ctx, fonts->families, &state, pdf_xfa_fonts_count_cb);
        *families_out = state.count;
    }
    if (held_out) *held_out = fonts->held_n;

    if (!missing_out || !form || !ctx) return;

    fz_var(warned);
    fz_try(ctx) {
        warned = fz_new_hash_table(ctx, 32, FZ_HASH_TABLE_KEY_LENGTH, -1, NULL);
        pdf_xfa_fonts_check_used_walk(ctx, fonts, warned, form, &missing, missing_names);
        *missing_out = missing;
    }
    fz_always(ctx) fz_drop_hash_table(ctx, warned);
    fz_catch(ctx) fz_rethrow(ctx);
}

static fz_font* pdf_xfa_fonts_pick_variant(pdf_xfa_font_slot* slot, int bold, int italic) {
    fz_font* font = NULL;

    if (!slot) return NULL;
    if (bold && italic && slot->bold_italic)
        font = slot->bold_italic;
    else if (bold && slot->bold)
        font = slot->bold;
    else if (italic && slot->italic)
        font = slot->italic;
    else if (slot->regular)
        font = slot->regular;
    else if (slot->bold)
        font = slot->bold;
    else if (slot->italic)
        font = slot->italic;
    else if (slot->bold_italic)
        font = slot->bold_italic;
    return font;
}

static int pdf_xfa_fonts_has_embedded(fz_context* ctx, pdf_xfa_fonts* fonts, const char* typeface, int bold,
                                      int italic) {
    char mapped[FZ_HASH_TABLE_KEY_LENGTH];
    const char* psname;
    pdf_xfa_font_slot* slot;
    fz_font* font;

    if (!fonts || !typeface || !typeface[0]) return 0;

    pdf_xfa_fonts_map_typeface(ctx, fonts, typeface, mapped, sizeof mapped);
    if (!mapped[0]) return 0;

    psname = pdf_xfa_fonts_psmap_lookup(fonts, mapped, bold, italic);
    if (psname) {
        slot = pdf_xfa_fonts_find_slot(ctx, fonts, psname);
        font = pdf_xfa_fonts_pick_variant(slot, bold, italic);
        if (font) return 1;
    }

    slot = pdf_xfa_fonts_find_slot(ctx, fonts, mapped);
    if (slot) {
        font = pdf_xfa_fonts_pick_variant(slot, bold, italic);
        if (font) return 1;
    }

    if (fonts->embedded) {
        char variant_key[FZ_HASH_TABLE_KEY_LENGTH];
        pdf_xfa_fonts_variant_key(mapped, bold, italic, variant_key, sizeof variant_key);
        if (variant_key[0] && fz_hash_find(ctx, fonts->embedded, variant_key)) return 1;
        if (psname) {
            pdf_xfa_fonts_variant_key(psname, bold, italic, variant_key, sizeof variant_key);
            if (variant_key[0] && fz_hash_find(ctx, fonts->embedded, variant_key)) return 1;
        }
    }
    return 0;
}

fz_font* pdf_xfa_fonts_resolve(fz_context* ctx, pdf_xfa_fonts* fonts, const char* typeface, int bold, int italic) {
    pdf_xfa_font_slot* slot;
    fz_font* font = NULL;
    char mapped[FZ_HASH_TABLE_KEY_LENGTH];
    const char* psname;

    pdf_xfa_fonts_map_typeface(ctx, fonts, typeface, mapped, sizeof mapped);
    if (!mapped[0]) return pdf_xfa_fonts_base14(ctx, fonts, typeface, bold, italic);

    psname = pdf_xfa_fonts_psmap_lookup(fonts, mapped, bold, italic);
    if (psname) {
        slot = pdf_xfa_fonts_find_slot(ctx, fonts, psname);
        font = pdf_xfa_fonts_pick_variant(slot, bold, italic);
        if (font) return font;
    }

    slot = pdf_xfa_fonts_find_slot(ctx, fonts, mapped);
    if (slot) {
        font = pdf_xfa_fonts_pick_variant(slot, bold, italic);
        if (font) return font;
    }
    font = pdf_xfa_fonts_base14(ctx, fonts, mapped, bold, italic);
    if (font) return font;
    return pdf_xfa_fonts_base14(ctx, fonts, "Helvetica", bold, italic);
}

const char* pdf_xfa_fonts_default_typeface(pdf_xfa_fonts* fonts) {
    if (!fonts || !fonts->default_typeface[0]) return NULL;
    return fonts->default_typeface;
}

static void pdf_xfa_fonts_check_used_walk(fz_context* ctx, pdf_xfa_fonts* fonts, fz_hash_table* warned,
                                          pdf_xfa_object* node, int* missing_out, fz_buffer* missing_names) {
    pdf_xfa_object* child;
    char* typeface;
    char clean[FZ_HASH_TABLE_KEY_LENGTH];
    char key[FZ_HASH_TABLE_KEY_LENGTH];
    char variant_key[FZ_HASH_TABLE_KEY_LENGTH];

    if (!node) return;

    if (node->name && strcmp(node->name, "font") == 0) {
        char* weight;
        char* posture;
        int bold;
        int italic;

        typeface = pdf_xfa_object_get_attr(ctx, node, "typeface");
        if (typeface && typeface[0]) {
            weight = pdf_xfa_object_get_attr(ctx, node, "weight");
            posture = pdf_xfa_object_get_attr(ctx, node, "posture");
            bold = pdf_xfa_font_attr_bold(weight);
            italic = pdf_xfa_font_attr_italic(posture);
            pdf_xfa_strip_typeface_quotes(typeface, clean, sizeof clean);
            pdf_xfa_family_key(clean, key, sizeof key);
            if (!key[0]) return;
            pdf_xfa_fonts_variant_key(clean, bold, italic, variant_key, sizeof variant_key);
            if (!variant_key[0]) return;
            if (!fz_hash_find(ctx, warned, variant_key)) {
                fz_hash_insert(ctx, warned, variant_key, (void*)1);
                if (!pdf_xfa_fonts_has_embedded(ctx, fonts, typeface, bold, italic)) {
                    if (missing_out) (*missing_out)++;
                    if (missing_names) {
                        size_t prev = fz_buffer_storage(ctx, missing_names, NULL);
                        fz_append_printf(ctx, missing_names, "%s%s:%d:%d", prev ? ";" : "", clean, bold, italic);
                    }
                    if (!pdf_xfa_fonts_resolve(ctx, fonts, typeface, bold, italic))
                        fz_warn(ctx, "XFA: cannot find the font: %s", clean);
                }
            }
        }
    }

    for (child = node->first_child; child; child = child->next_sibling)
        pdf_xfa_fonts_check_used_walk(ctx, fonts, warned, child, missing_out, missing_names);
}

void pdf_xfa_fonts_check_used(fz_context* ctx, pdf_xfa_fonts* fonts, pdf_xfa_object* form) {
    fz_hash_table* warned = NULL;

    if (!fonts || !form) return;

    fz_var(warned);
    fz_try(ctx) {
        warned = fz_new_hash_table(ctx, 32, FZ_HASH_TABLE_KEY_LENGTH, -1, NULL);
        pdf_xfa_fonts_check_used_walk(ctx, fonts, warned, form, NULL, NULL);
    }
    fz_always(ctx) fz_drop_hash_table(ctx, warned);
    fz_catch(ctx) fz_rethrow(ctx);
}
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
    pdf_xfa_psmap_entry* psmap;
    int psmap_n;
    int psmap_cap;
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
        pdf_xfa_fonts_add_psmap(ctx, fonts, typeface, pdf_xfa_font_attr_bold(weight),
                                pdf_xfa_font_attr_italic(posture), psname);
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
            config = pdf_xfa_parse_xml(ctx, pool, packet->data, PDF_XFA_NS_CONFIG, 0, NULL);
            if (config) {
                pdf_xfa_fonts_walk_equates(ctx, pool, fonts, config);
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
    if (name && (strstr(name, "Bold") || strstr(name, "bold"))) return 1;
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
    if (name && (strstr(name, "Italic") || strstr(name, "Oblique") || strstr(name, "It"))) return 1;
    return 0;
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

static void pdf_xfa_fonts_load_dr(fz_context* ctx, pdf_document* doc, fz_pool* pool, pdf_xfa_fonts* fonts) {
    pdf_obj* root;
    pdf_obj* acro;
    pdf_obj* dr;
    pdf_obj* fontdict;
    int i, n;

    root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
    acro = pdf_dict_get(ctx, root, PDF_NAME(AcroForm));
    dr = pdf_dict_get(ctx, acro, PDF_NAME(DR));
    fontdict = pdf_dict_get(ctx, dr, PDF_NAME(Font));
    if (!fontdict) return;

    n = pdf_dict_len(ctx, fontdict);
    for (i = 0; i < n; i++) {
        pdf_obj* fontobj = pdf_dict_get_val(ctx, fontdict, i);
        pdf_font_desc* fontdesc = NULL;
        char* family = NULL;
        int bold, italic;

        fz_try(ctx) {
            fontdesc = pdf_load_font(ctx, doc, NULL, fontobj);
            family = pdf_xfa_font_family_name(ctx, pool, fontobj, fontdesc);
            if (family && family[0]) {
                const char* basefont_name = NULL;
                pdf_obj* basefontobj;

                bold = pdf_xfa_font_is_bold(ctx, fontobj, fontdesc);
                italic = pdf_xfa_font_is_italic(ctx, fontobj, fontdesc);
                pdf_xfa_fonts_add_variant(ctx, fonts, family, bold, italic, fontdesc->font);
                basefontobj = pdf_dict_get(ctx, fontobj, PDF_NAME(BaseFont));
                if (basefontobj) {
                    basefont_name = pdf_to_name(ctx, basefontobj);
                    if (basefont_name && basefont_name[0] &&
                        !pdf_xfa_typeface_keys_match(basefont_name, family))
                        pdf_xfa_fonts_add_variant(ctx, fonts, basefont_name, bold, italic, fontdesc->font);
                }
            }
        }
        fz_always(ctx) pdf_drop_font(ctx, fontdesc);
        fz_catch(ctx) {
            fz_warn(ctx, "XFA: could not load AcroForm DR font");
        }
    }
}

static fz_font* pdf_xfa_fonts_base14(fz_context* ctx, pdf_xfa_fonts* fonts, const char* typeface, int bold, int italic) {
    char clean[FZ_HASH_TABLE_KEY_LENGTH];
    const char* face;

    pdf_xfa_strip_typeface_quotes(typeface, clean, sizeof clean);
    pdf_xfa_family_key(clean, clean, sizeof clean);

    if (strstr(clean, "courier") == clean) {
        if (bold && italic) face = "Courier-BoldOblique";
        else if (bold)
            face = "Courier-Bold";
        else if (italic)
            face = "Courier-Oblique";
        else
            face = "Courier";
    } else if (strstr(clean, "times") == clean) {
        if (bold && italic) face = "Times-BoldItalic";
        else if (bold)
            face = "Times-Bold";
        else if (italic)
            face = "Times-Italic";
        else
            face = "Times-Roman";
    } else if (strstr(clean, "helvetica") == clean || strstr(clean, "arial") == clean ||
               strstr(clean, "myriad") == clean || strstr(clean, "segoe") == clean ||
               strstr(clean, "calibri") == clean) {
        if (bold && italic) face = "Helvetica-BoldOblique";
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
        fonts->families =
            fz_new_hash_table(ctx, 64, FZ_HASH_TABLE_KEY_LENGTH, -1, pdf_xfa_fonts_drop_slot);
        fonts->equates = fz_new_hash_table(ctx, 32, FZ_HASH_TABLE_KEY_LENGTH, -1, NULL);
        pdf_xfa_fonts_parse_config_packet(ctx, pool, fonts, packets);
        pdf_xfa_fonts_load_dr(ctx, doc, pool, fonts);
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
    }

    if (font) return font;
    return pdf_xfa_fonts_base14(ctx, fonts, mapped, bold, italic);
}
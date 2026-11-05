/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "mupdf/fitz.h"
#include <ctype.h>
#include "ofd-seal.h"
#include "ofd.h"

/* GB/T 33190 uses millimetres; MuPDF uses points. */
static const float ofd_points_per_mm = 72.0f / 25.4f;
enum {
    OFD_PATH_SIZE = 4096,
    OFD_MAX_DEPTH = 32,
    OFD_MAX_GLYPHS = 1000000,
    OFD_MAX_DASHES = 128
};

typedef struct ofd_resource {
    fz_xml* xml;
    char path[OFD_PATH_SIZE];
    struct ofd_resource* next;
} ofd_resource;

typedef struct ofd_font_entry {
    fz_xml* definition;
    fz_font* font;
    int embedded;
    int system;
    struct ofd_font_entry* next;
} ofd_font_entry;

typedef struct {
    fz_document super;
    fz_archive* archive;
    fz_xml* xml;
    fz_xml* common;
    ofd_resource* resources;
    ofd_font_entry* fonts;
    char path[OFD_PATH_SIZE];
    char signatures[OFD_PATH_SIZE];
    int count;
    int seal_depth;
} ofd_document;

static fz_document* ofd_open_document(fz_context* ctx, const fz_document_handler* handler, fz_stream* stream,
                                      fz_stream* accel, fz_archive* dir, void* state);

typedef struct {
    fz_page super;
    fz_rect bounds;
    fz_display_list* list;
} ofd_page;

typedef struct {
    const char* text;
    int repeats;
    float value;
} ofd_delta;

static int ofd_is(fz_xml* node, const char* name) {
    const char* tag = fz_xml_tag(node);
    const char* colon = tag ? strchr(tag, ':') : NULL;
    return tag && !strcmp(colon ? colon + 1 : tag, name);
}

static fz_xml* ofd_child(fz_xml* node, const char* name) {
    for (node = fz_xml_down(node); node; node = fz_xml_next(node)) {
        if (ofd_is(node, name)) return node;
    }
    return NULL;
}

static const char* ofd_text(fz_xml* node) {
    const char* text = fz_xml_text(fz_xml_down(node));
    return text ? text : "";
}

static float ofd_number(const char** input) {
    char* end;
    const char* s = *input;
    float value;
    if (!s) return 0;
    while (*s && (isspace((unsigned char)*s) || *s == ',')) ++s;
    value = fz_strtof(s, &end);
    *input = end;
    return isfinite(value) ? value : 0;
}

static float ofd_attr(fz_xml* node, const char* name, float fallback) {
    const char* s = fz_xml_att(node, name);
    return s ? ofd_number(&s) : fallback;
}

static fz_rect ofd_box(const char* s) {
    float x = ofd_number(&s), y = ofd_number(&s);
    float w = ofd_number(&s), h = ofd_number(&s);
    return fz_make_rect(x, y, x + w, y + h);
}

/* Resolve only ZIP member names, never filesystem paths. */
static void ofd_resolve(fz_context* ctx, char* out, const char* base, const char* loc) {
    char dir[OFD_PATH_SIZE];
    if (!loc || !*loc) fz_throw(ctx, FZ_ERROR_FORMAT, "Missing OFD resource path");
    fz_dirname(dir, base, sizeof(dir));
    if (fz_snprintf(out, OFD_PATH_SIZE, "%s/%s", loc[0] == '/' ? "" : dir, loc) >= OFD_PATH_SIZE)
        fz_throw(ctx, FZ_ERROR_LIMIT, "OFD resource path too long");
    for (char* p = out; *p; ++p) {
        if (*p == '\\') *p = '/';
    }
    while (out[0] == '/') memmove(out, out + 1, strlen(out));
    fz_cleanname(out);
    if (!strcmp(out, "..") || !strncmp(out, "../", 3) || strchr(out, ':'))
        fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD resource path");
}

static fz_xml* ofd_read_xml(fz_context* ctx, ofd_document* doc, const char* path) {
    return fz_parse_xml_archive_entry(ctx, doc->archive, path, 0);
}

// Recover a bad resource directory only when the document has one matching member.
static void ofd_resource_path(fz_context* ctx, ofd_document* doc, char* out, const char* base, const char* loc) {
    ofd_resolve(ctx, out, base, loc);
    if (fz_has_archive_entry(ctx, doc->archive, out)) return;
    const char* leaf = strrchr(out, '/');
    leaf = leaf ? leaf + 1 : out;
    char directory[OFD_PATH_SIZE];
    fz_dirname(directory, doc->path, sizeof(directory));
    size_t prefix = strlen(directory);
    const char* found = NULL;
    int count = fz_count_archive_entries(ctx, doc->archive);
    for (int i = 0; i < count; ++i) {
        const char* name = fz_list_archive_entry(ctx, doc->archive, i);
        if (!name || (strcmp(directory, ".") && (strncmp(name, directory, prefix) || name[prefix] != '/'))) continue;
        const char* candidate = strrchr(name, '/');
        candidate = candidate ? candidate + 1 : name;
        if (strcmp(candidate, leaf)) continue;
        if (found) return;
        found = name;
    }
    if (found) {
        if (strlen(found) >= OFD_PATH_SIZE) fz_throw(ctx, FZ_ERROR_LIMIT, "OFD resource path too long");
        fz_warn(ctx, "Recovered OFD resource path %s as %s", out, found);
        fz_strlcpy(out, found, OFD_PATH_SIZE);
    }
}

static void ofd_load_resources(fz_context* ctx, ofd_document* doc, fz_xml* parent, const char* path, const char* tag) {
    for (fz_xml* node = fz_xml_down(parent); node; node = fz_xml_next(node)) {
        if (!ofd_is(node, tag)) continue;
        char path_name[OFD_PATH_SIZE];
        ofd_resolve(ctx, path_name, path, ofd_text(node));
        if (!fz_has_archive_entry(ctx, doc->archive, path_name)) {
            fz_warn(ctx, "Missing OFD resource dictionary: %s", path_name);
            continue;
        }
        ofd_resource* res = fz_malloc_struct(ctx, ofd_resource);
        res->next = doc->resources;
        doc->resources = res;
        fz_strlcpy(res->path, path_name, sizeof(res->path));
        res->xml = ofd_read_xml(ctx, doc, res->path);
        const char* base = fz_xml_att(fz_xml_root(res->xml), "BaseLoc");
        if (base) {
            char resolved[OFD_PATH_SIZE];
            ofd_resolve(ctx, resolved, res->path, base);
            if (fz_snprintf(res->path, sizeof(res->path), "%s/", resolved) >= sizeof(res->path))
                fz_throw(ctx, FZ_ERROR_LIMIT, "OFD resource path too long");
        }
    }
}

static fz_xml* ofd_find_id(fz_xml* node, const char* tag, const char* id, int depth) {
    if (!id || depth > OFD_MAX_DEPTH) return NULL;
    for (; node; node = fz_xml_next(node)) {
        if (ofd_is(node, tag) && fz_xml_att_eq(node, "ID", id)) return node;
        fz_xml* found = ofd_find_id(fz_xml_down(node), tag, id, depth + 1);
        if (found) return found;
    }
    return NULL;
}

static fz_xml* ofd_resource_id(ofd_document* doc, const char* tag, const char* id, const char** path) {
    for (ofd_resource* res = doc->resources; res; res = res->next) {
        fz_xml* node = ofd_find_id(fz_xml_root(res->xml), tag, id, 0);
        if (node) {
            if (path) *path = res->path;
            return node;
        }
    }
    return NULL;
}

static float ofd_next_delta(fz_context* ctx, ofd_delta* delta, float fallback) {
    if (delta->repeats > 0) {
        --delta->repeats;
        return delta->value;
    }
    const char* s = delta->text;
    if (!s) return fallback;
    while (isspace((unsigned char)*s)) ++s;
    if (!*s) return delta->value;
    if (*s == 'g') {
        ++s;
        float count = ofd_number(&s);
        if (count < 1 || count > 1000000) fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD delta repeat");
        delta->repeats = (int)count - 1;
    }
    const char* before = s;
    delta->value = ofd_number(&s);
    if (before == s) fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD delta");
    delta->text = s;
    return delta->value;
}

static fz_matrix ofd_matrix(fz_xml* node, fz_matrix parent) {
    fz_rect box = ofd_box(fz_xml_att(node, "Boundary"));
    fz_matrix m = fz_identity;
    const char* s = fz_xml_att(node, "CTM");
    if (s) {
        m.a = ofd_number(&s);
        m.b = ofd_number(&s);
        m.c = ofd_number(&s);
        m.d = ofd_number(&s);
        m.e = ofd_number(&s);
        m.f = ofd_number(&s);
    }
    m.e += box.x0;
    m.f += box.y0;
    return fz_concat(m, parent);
}

typedef struct {
    fz_xml* fill;
    fz_xml* stroke;
    float width;
    const char* cap;
    const char* join;
    const char* dash;
    float dash_offset;
    float miter;
} ofd_paint;

static void ofd_draw_tree(fz_context* ctx, ofd_document* doc, fz_xml* node, fz_device* dev, fz_matrix parent, int depth,
                          ofd_paint inherited);
static void ofd_brush(fz_context* ctx, ofd_document* doc, fz_xml* color, fz_device* dev, fz_rect area, fz_matrix ctm,
                      float alpha, int depth);

static void ofd_apply_paint(fz_context* ctx, ofd_document* doc, fz_xml* node, ofd_paint* paint, int depth) {
    if (depth > OFD_MAX_DEPTH) fz_throw(ctx, FZ_ERROR_LIMIT, "OFD draw parameter cycle");
    const char* id = fz_xml_att(node, "DrawParam");
    if (!id) id = fz_xml_att(node, "Relative");
    fz_xml* param = ofd_resource_id(doc, "DrawParam", id, NULL);
    if (param) ofd_apply_paint(ctx, doc, param, paint, depth + 1);
    fz_xml* value = ofd_child(node, "FillColor");
    if (value) paint->fill = value;
    value = ofd_child(node, "StrokeColor");
    if (value) paint->stroke = value;
    paint->width = ofd_attr(node, "LineWidth", paint->width);
    const char* cap = fz_xml_att(node, "Cap");
    const char* join = fz_xml_att(node, "Join");
    const char* dash = fz_xml_att(node, "DashPattern");
    if (cap) paint->cap = cap;
    if (join) paint->join = join;
    if (dash) paint->dash = dash;
    paint->dash_offset = ofd_attr(node, "DashOffset", paint->dash_offset);
    paint->miter = ofd_attr(node, "MiterLimit", paint->miter);
}

static fz_stroke_state* ofd_make_stroke(fz_context* ctx, ofd_paint paint) {
    float values[OFD_MAX_DASHES];
    int count = 0;
    float total = 0;
    const char* s = paint.dash;
    while (s && *s) {
        while (isspace((unsigned char)*s)) ++s;
        if (!*s) break;
        const char* before = s;
        float value = ofd_number(&s);
        if (s == before || value < 0 || count == OFD_MAX_DASHES)
            fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD dash pattern");
        values[count++] = value;
        total += value;
    }
    if (count && total <= 0) fz_throw(ctx, FZ_ERROR_FORMAT, "Empty OFD dash pattern");
    fz_stroke_state* stroke = fz_new_stroke_state_with_dash_len(ctx, count);
    stroke->linewidth = paint.width;
    if (paint.cap && !strcmp(paint.cap, "Round"))
        stroke->start_cap = stroke->dash_cap = stroke->end_cap = FZ_LINECAP_ROUND;
    else if (paint.cap && !strcmp(paint.cap, "Square"))
        stroke->start_cap = stroke->dash_cap = stroke->end_cap = FZ_LINECAP_SQUARE;
    if (paint.join && !strcmp(paint.join, "Round"))
        stroke->linejoin = FZ_LINEJOIN_ROUND;
    else if (paint.join && !strcmp(paint.join, "Bevel"))
        stroke->linejoin = FZ_LINEJOIN_BEVEL;
    if (paint.miter > 0) stroke->miterlimit = paint.miter;
    stroke->dash_len = count;
    stroke->dash_phase = paint.dash_offset;
    for (int i = 0; i < count; ++i) stroke->dash_list[i] = values[i];
    return stroke;
}

static float ofd_color(fz_context* ctx, ofd_document* doc, fz_xml* node, fz_xml* value, fz_colorspace** cs,
                       float* color) {
    const char* s = fz_xml_att(value, "Value");
    if (value && !s) fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "OFD indexed and gradient colors are not supported yet");
    fz_xml* space = ofd_resource_id(doc, "ColorSpace", fz_xml_att(value, "ColorSpace"), NULL);
    const char* type = fz_xml_att(space, "Type");
    int n = 3;
    *cs = fz_device_rgb(ctx);
    if (type && !strcmp(type, "GRAY")) {
        *cs = fz_device_gray(ctx);
        n = 1;
    } else if (type && !strcmp(type, "CMYK")) {
        *cs = fz_device_cmyk(ctx);
        n = 4;
    }
    float bits = ofd_attr(space, "BitsPerComponent", 8);
    if (bits != 1 && bits != 2 && bits != 4 && bits != 8 && bits != 16)
        fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD color depth");
    float max = powf(2, bits) - 1;
    for (int i = 0; i < n; ++i) color[i] = fz_clamp(ofd_number(&s) / max, 0, 1);
    return ofd_attr(node, "Alpha", 255) / 255 * ofd_attr(value, "Alpha", 255) / 255;
}

static ofd_font_entry* ofd_font(fz_context* ctx, ofd_document* doc, fz_xml* node) {
    const char* base = NULL;
    fz_xml* definition = ofd_resource_id(doc, "Font", fz_xml_att(node, "Font"), &base);
    for (ofd_font_entry* entry = doc->fonts; entry; entry = entry->next) {
        if (entry->definition == definition && entry->font) return entry;
    }
    ofd_font_entry* entry = fz_malloc_struct(ctx, ofd_font_entry);
    entry->definition = definition;
    entry->next = doc->fonts;
    doc->fonts = entry;
    const char* name = fz_xml_att(definition, "FontName");
    fz_xml* file = ofd_child(definition, "FontFile");
    if (file) {
        char path[OFD_PATH_SIZE];
        fz_buffer* buffer = NULL;
        fz_var(buffer);
        fz_try(ctx) {
            ofd_resource_path(ctx, doc, path, base, ofd_text(file));
            buffer = fz_read_archive_entry(ctx, doc->archive, path);
            entry->font = fz_new_font_from_buffer(ctx, name, buffer, 0, 0);
            entry->embedded = 1;
        }
        fz_always(ctx) {
            fz_drop_buffer(ctx, buffer);
        }
        fz_catch(ctx) {
            fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
            fz_report_error(ctx);
            fz_warn(ctx, "Cannot load OFD font; using a substitute");
        }
    }
    if (!entry->font) {
        int bold = fz_xml_att_eq(definition, "Bold", "true");
        int italic = fz_xml_att_eq(definition, "Italic", "true");
        if (name) entry->font = fz_load_system_font(ctx, name, bold, italic, 0);
        const char* family = fz_xml_att(definition, "FamilyName");
        if (!entry->font && family) entry->font = fz_load_system_font(ctx, family, bold, italic, 0);
        entry->system = entry->font && !file;
        if (!entry->font) entry->font = fz_new_base14_font(ctx, "Helvetica");
    }
    return entry;
}

static int ofd_text_count(fz_context* ctx, fz_xml* node, const char* name, int fallback) {
    float value = ofd_attr(node, name, (float)fallback);
    if (value < 0 || value > OFD_MAX_GLYPHS || value != floorf(value))
        fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD text count");
    return (int)value;
}

static fz_xml* ofd_glyph_mapping(fz_context* ctx, fz_xml* node, int position) {
    for (fz_xml* mapping = fz_xml_down(node); mapping; mapping = fz_xml_next(mapping)) {
        if (!ofd_is(mapping, "CGTransform")) continue;
        int start = ofd_text_count(ctx, mapping, "CodePosition", 0);
        int count = ofd_text_count(ctx, mapping, "CodeCount", 1);
        if (position >= start && position - start < count) return mapping;
    }
    return NULL;
}

static int ofd_mapped_glyph(fz_context* ctx, fz_xml* mapping, int index) {
    const char* s = ofd_text(ofd_child(mapping, "Glyphs"));
    float glyph = 0;
    for (int i = 0; i <= index; ++i) {
        const char* before = s;
        glyph = ofd_number(&s);
        if (s == before || glyph < 0 || glyph > OFD_MAX_GLYPHS || glyph != floorf(glyph))
            fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD glyph mapping");
    }
    return (int)glyph;
}

static fz_text* ofd_make_text(fz_context* ctx, ofd_document* doc, fz_xml* node) {
    ofd_font_entry* entry = ofd_font(ctx, doc, node);
    fz_text* text = fz_new_text(ctx);
    fz_try(ctx) {
        float size = ofd_attr(node, "Size", 0);
        float x = 0, y = 0;
        float cluster_x = 0, cluster_y = 0;
        int position = 0;
        for (fz_xml* code = fz_xml_down(node); code; code = fz_xml_next(code)) {
            if (!ofd_is(code, "TextCode")) continue;
            x = ofd_attr(code, "X", x);
            y = ofd_attr(code, "Y", y);
            ofd_delta dx = {fz_xml_att(code, "DeltaX"), 0, 0};
            ofd_delta dy = {fz_xml_att(code, "DeltaY"), 0, 0};
            const char* s = ofd_text(code);
            const char* end = s + strlen(s);
            if (strpbrk(s, "\r\n")) {
                while (s < end && isspace((unsigned char)*s)) ++s;
                while (end > s && isspace((unsigned char)end[-1])) --end;
            }
            while (s < end) {
                int rune;
                s += fz_chartorune(&rune, s);
                if (rune == '\r' || rune == '\n' || rune == '\t') continue;
                fz_font* font = entry->font;
                fz_xml* mapping = entry->embedded || entry->system ? ofd_glyph_mapping(ctx, node, position) : NULL;
                int index = mapping ? position - ofd_text_count(ctx, mapping, "CodePosition", 0) : 0;
                int glyphs = ofd_text_count(ctx, mapping, "GlyphCount", 1);
                int codes = ofd_text_count(ctx, mapping, "CodeCount", 1);
                if (glyphs <= 0 || codes <= 0) fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD glyph count");
                // Subset glyph IDs cannot address a substituted full font; retain real ligatures.
                if (!entry->embedded && codes == glyphs) {
                    mapping = NULL;
                    index = 0;
                    glyphs = codes = 1;
                }
                int glyph = mapping ? (index < glyphs ? ofd_mapped_glyph(ctx, mapping, index) : -1)
                                    : fz_encode_character_with_fallback(ctx, font, rune, 0, FZ_LANG_UNSET, &font);
                int outputs = mapping && codes == 1 ? glyphs : 1;
                for (int output = 0; output < outputs; ++output) {
                    if (output) glyph = ofd_mapped_glyph(ctx, mapping, output);
                    if (glyph >= font->glyph_count) glyph = 0;
                    fz_matrix trm = fz_scale(size * ofd_attr(node, "HScale", 1), -size);
                    trm = fz_concat(trm, fz_rotate(ofd_attr(node, "CharDirection", 0)));
                    trm.e = x;
                    trm.f = y;
                    if (!index && !output) {
                        cluster_x = x;
                        cluster_y = y;
                    }
                    if (glyph < 0) {
                        trm.e = cluster_x;
                        trm.f = cluster_y;
                        fz_show_glyph_aux(ctx, text, font, trm, 0, -1, rune, -1, 0, 0, FZ_BIDI_LTR, FZ_LANG_UNSET);
                        continue;
                    }
                    fz_show_glyph(ctx, text, font, trm, glyph, output ? -1 : rune, 0, 0, FZ_BIDI_LTR, FZ_LANG_UNSET);
                    // Delta arrays position glyphs, including one-to-many substitutions.
                    if (output + 1 < outputs || s < end) {
                        float advance = fz_advance_glyph(ctx, font, glyph, 0) * size;
                        x += ofd_next_delta(ctx, &dx, advance);
                        y += ofd_next_delta(ctx, &dy, 0);
                    }
                }
                ++position;
            }
        }
    }
    fz_catch(ctx) {
        fz_drop_text(ctx, text);
        fz_rethrow(ctx);
    }
    return text;
}

static fz_xml* ofd_complex_color(fz_xml* color);

static void ofd_draw_text(fz_context* ctx, ofd_document* doc, fz_xml* node, fz_device* dev, fz_matrix ctm,
                          ofd_paint paint, int depth) {
    fz_text* text = ofd_make_text(ctx, doc, node);
    fz_stroke_state* stroke = NULL;
    fz_var(stroke);
    fz_try(ctx) {
        float color[4] = {0};
        fz_colorspace* cs;
        if (!fz_xml_att_eq(node, "Fill", "false")) {
            if (ofd_complex_color(paint.fill)) {
                float alpha = ofd_attr(node, "Alpha", 255) / 255 * ofd_attr(paint.fill, "Alpha", 255) / 255;
                fz_clip_text(ctx, dev, text, ctm, fz_infinite_rect);
                ofd_brush(ctx, doc, paint.fill, dev, fz_bound_text(ctx, text, NULL, fz_identity), ctm, alpha,
                          depth + 1);
                fz_pop_clip(ctx, dev);
            } else {
                float alpha = ofd_color(ctx, doc, node, paint.fill, &cs, color);
                fz_fill_text(ctx, dev, text, ctm, cs, color, alpha, fz_default_color_params);
            }
        }
        if (fz_xml_att_eq(node, "Stroke", "true")) {
            stroke = ofd_make_stroke(ctx, paint);
            if (ofd_complex_color(paint.stroke)) {
                float alpha = ofd_attr(node, "Alpha", 255) / 255 * ofd_attr(paint.stroke, "Alpha", 255) / 255;
                fz_clip_stroke_text(ctx, dev, text, stroke, ctm, fz_infinite_rect);
                ofd_brush(ctx, doc, paint.stroke, dev, fz_bound_text(ctx, text, stroke, fz_identity), ctm, alpha,
                          depth + 1);
                fz_pop_clip(ctx, dev);
            } else {
                float alpha = ofd_color(ctx, doc, node, paint.stroke, &cs, color);
                fz_stroke_text(ctx, dev, text, stroke, ctm, cs, color, alpha, fz_default_color_params);
            }
        }
    }
    fz_always(ctx) {
        fz_drop_stroke_state(ctx, stroke);
        fz_drop_text(ctx, text);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

static fz_path* ofd_make_path(fz_context* ctx, fz_xml* node) {
    fz_path* path = fz_new_path(ctx);
    fz_try(ctx) {
        const char* s = ofd_text(ofd_child(node, "AbbreviatedData"));
        while (*s) {
            while (isspace((unsigned char)*s)) ++s;
            if (!*s) break;
            char command = *s++;
            float v[6];
            int n = command == 'B'                                         ? 6
                    : command == 'Q'                                       ? 4
                    : (command == 'M' || command == 'S' || command == 'L') ? 2
                                                                           : 0;
            for (int i = 0; i < n; ++i) {
                const char* before = s;
                v[i] = ofd_number(&s);
                if (before == s) fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD path");
            }
            switch (command) {
                case 'M':
                case 'S':
                    fz_moveto(ctx, path, v[0], v[1]);
                    break;
                case 'L':
                    fz_lineto(ctx, path, v[0], v[1]);
                    break;
                case 'B':
                    fz_curveto(ctx, path, v[0], v[1], v[2], v[3], v[4], v[5]);
                    break;
                case 'Q':
                    fz_quadto(ctx, path, v[0], v[1], v[2], v[3]);
                    break;
                case 'C':
                    fz_closepath(ctx, path);
                    break;
                default:
                    fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "Unsupported OFD path command '%c'", command);
            }
        }
    }
    fz_catch(ctx) {
        fz_drop_path(ctx, path);
        fz_rethrow(ctx);
    }
    return path;
}

enum {
    OFD_SHADE_SAMPLES = 256,
    OFD_RGB_ALPHA = 4
};

static fz_xml* ofd_complex_color(fz_xml* color) {
    fz_xml* node = ofd_child(color, "Pattern");
    if (!node) node = ofd_child(color, "AxialShd");
    if (!node) node = ofd_child(color, "RadialShd");
    return node;
}

static void ofd_shade_color(fz_context* ctx, ofd_document* doc, fz_xml* segment, float* rgba) {
    fz_colorspace* cs;
    float color[FZ_MAX_COLORS] = {0};
    rgba[3] = ofd_color(ctx, doc, NULL, ofd_child(segment, "Color"), &cs, color);
    fz_convert_color(ctx, cs, color, fz_device_rgb(ctx), rgba, NULL, fz_default_color_params);
}

static void ofd_gradient(fz_context* ctx, ofd_document* doc, fz_xml* node, fz_device* dev, fz_matrix ctm, float alpha) {
    fz_shade* shade = fz_malloc_struct(ctx, fz_shade);
    FZ_INIT_STORABLE(shade, 1, fz_drop_shade_imp);
    fz_try(ctx) {
        shade->bbox = fz_infinite_rect;
        shade->matrix = fz_identity;
        shade->colorspace = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
        shade->type = ofd_is(node, "RadialShd") ? FZ_RADIAL : FZ_LINEAR;
        int extend = (int)ofd_attr(node, "Extend", 0);
        shade->u.l_or_r.extend[0] = (extend & 1) != 0;
        shade->u.l_or_r.extend[1] = (extend & 2) != 0;
        const char* start = fz_xml_att(node, "StartPoint");
        const char* end = fz_xml_att(node, "EndPoint");
        shade->u.l_or_r.coords[0][0] = ofd_number(&start);
        shade->u.l_or_r.coords[0][1] = ofd_number(&start);
        shade->u.l_or_r.coords[0][2] = ofd_attr(node, "StartRadius", 0);
        shade->u.l_or_r.coords[1][0] = ofd_number(&end);
        shade->u.l_or_r.coords[1][1] = ofd_number(&end);
        shade->u.l_or_r.coords[1][2] = ofd_attr(node, "EndRadius", 0);
        int count = 0;
        for (fz_xml* segment = fz_xml_down(node); segment; segment = fz_xml_next(segment))
            if (ofd_is(segment, "Segment")) ++count;
        if (count < 2) fz_throw(ctx, FZ_ERROR_FORMAT, "OFD gradient needs two segments");
        shade->function_stride = OFD_RGB_ALPHA;
        shade->function = fz_malloc_array(ctx, OFD_SHADE_SAMPLES * OFD_RGB_ALPHA, float);
        for (int i = 0; i < OFD_SHADE_SAMPLES; ++i) {
            float t = (float)i / (OFD_SHADE_SAMPLES - 1);
            fz_xml* lower = ofd_child(node, "Segment");
            fz_xml* upper = lower;
            float lo = 0, hi = 0;
            int index = 0;
            for (fz_xml* segment = fz_xml_down(node); segment; segment = fz_xml_next(segment)) {
                if (!ofd_is(segment, "Segment")) continue;
                float position = ofd_attr(segment, "Position", (float)index++ / (count - 1));
                if (position <= t) {
                    lower = segment;
                    lo = position;
                }
                upper = segment;
                hi = position;
                if (position >= t) break;
            }
            float a[OFD_RGB_ALPHA], b[OFD_RGB_ALPHA];
            ofd_shade_color(ctx, doc, lower, a);
            ofd_shade_color(ctx, doc, upper, b);
            float mix = hi > lo ? fz_clamp((t - lo) / (hi - lo), 0, 1) : 0;
            for (int k = 0; k < OFD_RGB_ALPHA; ++k) shade->function[i * OFD_RGB_ALPHA + k] = a[k] + (b[k] - a[k]) * mix;
        }
        fz_fill_shade(ctx, dev, shade, ctm, alpha, fz_default_color_params);
    }
    fz_always(ctx) {
        fz_drop_shade(ctx, shade);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

static void ofd_brush(fz_context* ctx, ofd_document* doc, fz_xml* color, fz_device* dev, fz_rect area, fz_matrix ctm,
                      float alpha, int depth) {
    if (depth > OFD_MAX_DEPTH) fz_throw(ctx, FZ_ERROR_LIMIT, "OFD pattern nesting too deep");
    fz_xml* node = ofd_complex_color(color);
    if (!ofd_is(node, "Pattern")) {
        ofd_gradient(ctx, doc, node, dev, ctm, alpha);
        return;
    }
    float w = ofd_attr(node, "Width", 0), h = ofd_attr(node, "Height", 0);
    float xstep = ofd_attr(node, "XStep", w), ystep = ofd_attr(node, "YStep", h);
    if (w <= 0 || h <= 0 || xstep <= 0 || ystep <= 0) fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD pattern dimensions");
    fz_matrix matrix = ofd_matrix(node, ctm);
    fz_rect coverage = fz_transform_rect(area, fz_invert_matrix(ofd_matrix(node, fz_identity)));
    fz_begin_group(ctx, dev, fz_transform_rect(area, ctm), NULL, 0, 0, FZ_BLEND_NORMAL, alpha);
    fz_begin_tile(ctx, dev, coverage, fz_make_rect(0, 0, w, h), xstep, ystep, matrix);
    ofd_paint paint = {NULL, NULL, 0.353f};
    ofd_draw_tree(ctx, doc, fz_xml_down(ofd_child(node, "CellContent")), dev, matrix, depth + 1, paint);
    fz_end_tile(ctx, dev);
    fz_end_group(ctx, dev);
}

static void ofd_draw_path(fz_context* ctx, ofd_document* doc, fz_xml* node, fz_device* dev, fz_matrix ctm,
                          ofd_paint paint, int depth) {
    fz_path* path = ofd_make_path(ctx, node);
    fz_stroke_state* stroke = NULL;
    fz_var(stroke);
    fz_try(ctx) {
        float color[4] = {0};
        fz_colorspace* cs;
        // Unlike text, paths without a fill color are transparent.
        if (paint.fill && fz_xml_att_eq(node, "Fill", "true")) {
            if (ofd_complex_color(paint.fill)) {
                float alpha = ofd_attr(node, "Alpha", 255) / 255 * ofd_attr(paint.fill, "Alpha", 255) / 255;
                fz_clip_path(ctx, dev, path, fz_xml_att_eq(node, "Rule", "Even-Odd"), ctm, fz_infinite_rect);
                ofd_brush(ctx, doc, paint.fill, dev, fz_bound_path(ctx, path, NULL, fz_identity), ctm, alpha,
                          depth + 1);
                fz_pop_clip(ctx, dev);
            } else {
                float alpha = ofd_color(ctx, doc, node, paint.fill, &cs, color);
                fz_fill_path(ctx, dev, path, fz_xml_att_eq(node, "Rule", "Even-Odd"), ctm, cs, color, alpha,
                             fz_default_color_params);
            }
        }
        if (!fz_xml_att_eq(node, "Stroke", "false")) {
            stroke = ofd_make_stroke(ctx, paint);
            if (ofd_complex_color(paint.stroke)) {
                float alpha = ofd_attr(node, "Alpha", 255) / 255 * ofd_attr(paint.stroke, "Alpha", 255) / 255;
                fz_clip_stroke_path(ctx, dev, path, stroke, ctm, fz_infinite_rect);
                ofd_brush(ctx, doc, paint.stroke, dev, fz_bound_path(ctx, path, stroke, fz_identity), ctm, alpha,
                          depth + 1);
                fz_pop_clip(ctx, dev);
            } else {
                float alpha = ofd_color(ctx, doc, node, paint.stroke, &cs, color);
                fz_stroke_path(ctx, dev, path, stroke, ctm, cs, color, alpha, fz_default_color_params);
            }
        }
    }
    fz_always(ctx) {
        fz_drop_stroke_state(ctx, stroke);
        fz_drop_path(ctx, path);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

static void ofd_draw_image(fz_context* ctx, ofd_document* doc, fz_xml* node, fz_device* dev, fz_matrix ctm) {
    const char* base = NULL;
    fz_xml* res = ofd_resource_id(doc, "MultiMedia", fz_xml_att(node, "ResourceID"), &base);
    if (!res) fz_throw(ctx, FZ_ERROR_FORMAT, "Missing OFD image resource");
    char path[OFD_PATH_SIZE];
    ofd_resource_path(ctx, doc, path, base, ofd_text(ofd_child(res, "MediaFile")));
    fz_buffer* buffer = NULL;
    fz_image* image = NULL;
    fz_var(buffer);
    fz_var(image);
    fz_try(ctx) {
        buffer = fz_read_archive_entry(ctx, doc->archive, path);
        image = fz_new_image_from_buffer(ctx, buffer);
        fz_fill_image(ctx, dev, image, ctm, ofd_attr(node, "Alpha", 255) / 255, fz_default_color_params);
    }
    fz_always(ctx) {
        fz_drop_image(ctx, image);
        fz_drop_buffer(ctx, buffer);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

static int ofd_apply_clips(fz_context* ctx, ofd_document* doc, fz_xml* node, fz_device* dev, fz_matrix parent,
                           fz_matrix ctm, ofd_paint paint) {
    fz_xml* clips = ofd_child(node, "Clips");
    fz_rect box = ofd_box(fz_xml_att(node, "Boundary"));
    fz_matrix base = fz_xml_att_eq(clips, "TransFlag", "true") ? ctm : fz_concat(fz_translate(box.x0, box.y0), parent);
    fz_rect bounds = fz_xml_att(node, "Boundary") ? fz_transform_rect(box, parent) : fz_infinite_rect;
    int count = 0;
    const float black[] = {0}, white[] = {1};
    for (fz_xml* clip = fz_xml_down(clips); clip; clip = fz_xml_next(clip)) {
        if (!ofd_is(clip, "Clip")) continue;
        // Areas within a Clip form a union; successive Clips intersect.
        fz_begin_mask(ctx, dev, bounds, 1, fz_device_gray(ctx), black, fz_default_color_params);
        for (fz_xml* area = fz_xml_down(clip); area; area = fz_xml_next(area)) {
            if (!ofd_is(area, "Area")) continue;
            ofd_paint local = paint;
            ofd_apply_paint(ctx, doc, area, &local, 0);
            fz_matrix matrix = ofd_matrix(area, base);
            fz_xml* shape = ofd_child(area, "Path");
            if (shape) {
                fz_path* path = ofd_make_path(ctx, shape);
                fz_stroke_state* stroke = NULL;
                fz_var(stroke);
                fz_try(ctx) {
                    fz_matrix transform = ofd_matrix(shape, matrix);
                    if (fz_xml_att_eq(shape, "Fill", "true"))
                        fz_fill_path(ctx, dev, path, fz_xml_att_eq(shape, "Rule", "Even-Odd"), transform,
                                     fz_device_gray(ctx), white, 1, fz_default_color_params);
                    if (!fz_xml_att_eq(shape, "Stroke", "false")) {
                        ofd_apply_paint(ctx, doc, shape, &local, 0);
                        stroke = ofd_make_stroke(ctx, local);
                        fz_stroke_path(ctx, dev, path, stroke, transform, fz_device_gray(ctx), white, 1,
                                       fz_default_color_params);
                    }
                }
                fz_always(ctx) {
                    fz_drop_stroke_state(ctx, stroke);
                    fz_drop_path(ctx, path);
                }
                fz_catch(ctx) {
                    fz_rethrow(ctx);
                }
            }
            shape = ofd_child(area, "Text");
            if (shape) {
                fz_text* text = ofd_make_text(ctx, doc, shape);
                fz_try(ctx) {
                    fz_fill_text(ctx, dev, text, ofd_matrix(shape, matrix), fz_device_gray(ctx), white, 1,
                                 fz_default_color_params);
                }
                fz_always(ctx) {
                    fz_drop_text(ctx, text);
                }
                fz_catch(ctx) {
                    fz_rethrow(ctx);
                }
            }
        }
        fz_end_mask(ctx, dev);
        ++count;
    }
    return count;
}

static void ofd_draw_tree(fz_context* ctx, ofd_document* doc, fz_xml* node, fz_device* dev, fz_matrix parent, int depth,
                          ofd_paint inherited) {
    if (depth > OFD_MAX_DEPTH) fz_throw(ctx, FZ_ERROR_LIMIT, "OFD nesting too deep");
    for (; node; node = fz_xml_next(node)) {
        if (fz_xml_att_eq(node, "Visible", "false") || ofd_attr(node, "Alpha", 255) <= 0) continue;
        fz_matrix ctm = ofd_matrix(node, parent);
        ofd_paint paint = inherited;
        ofd_apply_paint(ctx, doc, node, &paint, 0);
        int container = ofd_is(node, "Content") || ofd_is(node, "Layer") || ofd_is(node, "PageBlock") ||
                        ofd_is(node, "Appearance") || ofd_is(node, "CompositeObject");
        int clips = 0, grouped = 0;
        fz_var(clips);
        fz_var(grouped);
        fz_try(ctx) {
            clips = ofd_apply_clips(ctx, doc, node, dev, parent, ctm, paint);
            const char* blend = fz_xml_att(node, "BlendMode");
            float alpha = container ? ofd_attr(node, "Alpha", 255) / 255 : 1;
            if (blend || alpha < 1) {
                fz_rect box = fz_xml_att(node, "Boundary")
                                  ? fz_transform_rect(ofd_box(fz_xml_att(node, "Boundary")), parent)
                                  : fz_infinite_rect;
                fz_begin_group(ctx, dev, box, NULL, 0, 0, blend ? fz_lookup_blendmode(blend) : FZ_BLEND_NORMAL, alpha);
                grouped = 1;
            }
            if (ofd_is(node, "TextObject"))
                ofd_draw_text(ctx, doc, node, dev, ctm, paint, depth);
            else if (ofd_is(node, "PathObject"))
                ofd_draw_path(ctx, doc, node, dev, ctm, paint, depth);
            else if (ofd_is(node, "ImageObject")) {
                fz_try(ctx) {
                    ofd_draw_image(ctx, doc, node, dev, ctm);
                }
                fz_catch(ctx) {
                    fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
                    fz_report_error(ctx);
                    fz_warn(ctx, "Skipping unavailable OFD image");
                }
            } else if (ofd_is(node, "CompositeObject")) {
                fz_xml* unit = ofd_resource_id(doc, "CompositeGraphicUnit", fz_xml_att(node, "ResourceID"), NULL);
                if (!unit)
                    fz_warn(ctx, "Missing OFD composite resource");
                else
                    ofd_draw_tree(ctx, doc, ofd_child(unit, "Content"), dev, ctm, depth + 1, paint);
            } else if (container)
                ofd_draw_tree(ctx, doc, fz_xml_down(node), dev, ctm, depth + 1, paint);
        }
        fz_always(ctx) {
            if (grouped) fz_end_group(ctx, dev);
            while (clips-- > 0) fz_pop_clip(ctx, dev);
        }
        fz_catch(ctx) {
            fz_rethrow(ctx);
        }
    }
}

static void ofd_draw_nodes(fz_context* ctx, ofd_document* doc, fz_xml* node, fz_device* dev, fz_matrix ctm, int depth) {
    ofd_paint paint = {NULL, NULL, 0.353f};
    ofd_draw_tree(ctx, doc, node, dev, ctm, depth, paint);
}

static void ofd_draw_file(fz_context* ctx, ofd_document* doc, const char* path, fz_device* dev, fz_matrix ctm) {
    fz_xml* xml = ofd_read_xml(ctx, doc, path);
    fz_try(ctx) {
        if (ofd_child(fz_xml_root(xml), "PageRes") || ofd_child(fz_xml_root(xml), "Template"))
            fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "OFD page resources and nested templates are not supported yet");
        ofd_draw_nodes(ctx, doc, ofd_child(fz_xml_root(xml), "Content"), dev, ctm, 0);
    }
    fz_always(ctx) {
        fz_drop_xml(ctx, xml);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

static void ofd_templates(fz_context* ctx, ofd_document* doc, fz_xml* page, fz_device* dev, fz_matrix ctm,
                          const char* order) {
    for (fz_xml* node = fz_xml_down(page); node; node = fz_xml_next(node)) {
        if (!ofd_is(node, "Template")) continue;
        const char* z = fz_xml_att(node, "ZOrder");
        if (strcmp(z ? z : "Background", order)) continue;
        fz_xml* tpl = ofd_find_id(doc->common, "TemplatePage", fz_xml_att(node, "TemplateID"), 0);
        char path[OFD_PATH_SIZE];
        ofd_resolve(ctx, path, doc->path, fz_xml_att(tpl, "BaseLoc"));
        ofd_draw_file(ctx, doc, path, dev, ctm);
    }
}

static void ofd_drop_page(fz_context* ctx, fz_page* page) {
    fz_drop_display_list(ctx, ((ofd_page*)page)->list);
}

static fz_rect ofd_bound_page(fz_context* ctx, fz_page* page, fz_box_type box) {
    return ((ofd_page*)page)->bounds;
}

static void ofd_run_page(fz_context* ctx, fz_page* page, fz_device* dev, fz_matrix ctm, fz_cookie* cookie) {
    fz_run_display_list(ctx, ((ofd_page*)page)->list, dev, ctm, fz_infinite_rect, cookie);
}

static void ofd_annots(fz_context* ctx, ofd_document* doc, const char* page_id, fz_device* dev, fz_matrix ctm) {
    fz_xml* node = ofd_child(fz_xml_root(doc->xml), "Annotations");
    if (!node) return;
    char path[OFD_PATH_SIZE];
    ofd_resolve(ctx, path, doc->path, ofd_text(node));
    fz_xml* index = NULL;
    fz_xml* annots = NULL;
    fz_var(index);
    fz_var(annots);
    fz_try(ctx) {
        index = ofd_read_xml(ctx, doc, path);
        for (node = fz_xml_down(fz_xml_root(index)); node; node = fz_xml_next(node)) {
            if (!ofd_is(node, "Page") || !fz_xml_att_eq(node, "PageID", page_id)) continue;
            char entry[OFD_PATH_SIZE];
            ofd_resolve(ctx, entry, path, ofd_text(ofd_child(node, "FileLoc")));
            annots = ofd_read_xml(ctx, doc, entry);
            for (fz_xml* annot = fz_xml_down(fz_xml_root(annots)); annot; annot = fz_xml_next(annot)) {
                ofd_draw_nodes(ctx, doc, ofd_child(annot, "Appearance"), dev, ctm, 0);
            }
            fz_drop_xml(ctx, annots);
            annots = NULL;
        }
    }
    fz_always(ctx) {
        fz_drop_xml(ctx, annots);
        fz_drop_xml(ctx, index);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

static void ofd_draw_seal(fz_context* ctx, ofd_document* doc, fz_xml* stamp, const char* value_path, fz_device* dev,
                          fz_matrix ctm) {
    if (doc->seal_depth >= OFD_MAX_DEPTH) fz_throw(ctx, FZ_ERROR_LIMIT, "OFD seal nesting too deep");
    fz_rect target = ofd_box(fz_xml_att(stamp, "Boundary"));
    if (fz_is_empty_rect(target)) fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD seal boundary");
    fz_buffer* signed_value = NULL;
    fz_buffer* picture = NULL;
    fz_image* image = NULL;
    fz_stream* stream = NULL;
    fz_document* inner = NULL;
    fz_page* page = NULL;
    fz_path* clip = NULL;
    int clipped = 0;
    int grouped = 0;
    fz_var(signed_value);
    fz_var(picture);
    fz_var(image);
    fz_var(stream);
    fz_var(inner);
    fz_var(page);
    fz_var(clip);
    fz_var(clipped);
    fz_var(grouped);
    fz_try(ctx) {
        signed_value = fz_read_archive_entry(ctx, doc->archive, value_path);
        ofd_seal_type type;
        picture = ofd_seal_picture(ctx, signed_value, &type);
        clip = fz_new_path(ctx);
        fz_rect visible = target;
        if (fz_xml_att(stamp, "Clip")) {
            visible = ofd_box(fz_xml_att(stamp, "Clip"));
            visible = fz_transform_rect(visible, fz_translate(target.x0, target.y0));
            visible = fz_intersect_rect(visible, target);
        }
        fz_rectto(ctx, clip, visible.x0, visible.y0, visible.x1, visible.y1);
        fz_clip_path(ctx, dev, clip, 0, ctm, fz_infinite_rect);
        clipped = 1;
        // Multiply keeps opaque white stamp backgrounds from covering the document.
        fz_begin_group(ctx, dev, fz_transform_rect(visible, ctm), fz_device_rgb(ctx), 1, 0, FZ_BLEND_MULTIPLY, 1);
        grouped = 1;
        fz_matrix placement =
            fz_concat(fz_scale(target.x1 - target.x0, target.y1 - target.y0), fz_translate(target.x0, target.y0));
        if (type == OFD_SEAL_OFD) {
            stream = fz_open_buffer(ctx, picture);
            inner = ofd_open_document(ctx, NULL, stream, NULL, NULL, NULL);
            ((ofd_document*)inner)->seal_depth = doc->seal_depth + 1;
            page = fz_load_page(ctx, inner, 0);
            fz_rect bounds = fz_bound_page(ctx, page);
            if (fz_is_empty_rect(bounds)) fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD seal page dimensions");
            // Fit the actual seal page, not its document's default paper size.
            fz_matrix unit = fz_concat(fz_translate(-bounds.x0, -bounds.y0),
                                       fz_scale(1 / (bounds.x1 - bounds.x0), 1 / (bounds.y1 - bounds.y0)));
            fz_run_page_contents(ctx, page, dev, fz_concat(unit, fz_concat(placement, ctm)), NULL);
        } else {
            image = fz_new_image_from_buffer(ctx, picture);
            fz_fill_image(ctx, dev, image, fz_concat(placement, ctm), 1, fz_default_color_params);
        }
    }
    fz_always(ctx) {
        if (grouped) fz_end_group(ctx, dev);
        if (clipped) fz_pop_clip(ctx, dev);
        fz_drop_path(ctx, clip);
        fz_drop_page(ctx, page);
        fz_drop_document(ctx, inner);
        fz_drop_stream(ctx, stream);
        fz_drop_image(ctx, image);
        fz_drop_buffer(ctx, picture);
        fz_drop_buffer(ctx, signed_value);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

static void ofd_signature(fz_context* ctx, ofd_document* doc, const char* signature_path, const char* page_id,
                          fz_device* dev, fz_matrix ctm) {
    fz_xml* xml = ofd_read_xml(ctx, doc, signature_path);
    fz_try(ctx) {
        fz_xml* root = fz_xml_root(xml);
        fz_xml* info = ofd_child(root, "SignedInfo");
        for (fz_xml* stamp = fz_xml_down(info); stamp; stamp = fz_xml_next(stamp)) {
            if (!ofd_is(stamp, "StampAnnot") || !fz_xml_att_eq(stamp, "PageRef", page_id)) continue;
            char value_path[OFD_PATH_SIZE];
            ofd_resolve(ctx, value_path, signature_path, ofd_text(ofd_child(root, "SignedValue")));
            ofd_draw_seal(ctx, doc, stamp, value_path, dev, ctm);
        }
    }
    fz_always(ctx) {
        fz_drop_xml(ctx, xml);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

static void ofd_signatures(fz_context* ctx, ofd_document* doc, const char* page_id, fz_device* dev, fz_matrix ctm) {
    if (!doc->signatures[0]) return;
    fz_xml* index = ofd_read_xml(ctx, doc, doc->signatures);
    fz_try(ctx) {
        for (fz_xml* node = fz_xml_down(fz_xml_root(index)); node; node = fz_xml_next(node)) {
            if (!ofd_is(node, "Signature")) continue;
            fz_try(ctx) {
                char path[OFD_PATH_SIZE];
                ofd_resolve(ctx, path, doc->signatures, fz_xml_att(node, "BaseLoc"));
                ofd_signature(ctx, doc, path, page_id, dev, ctm);
            }
            fz_catch(ctx) {
                fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
                fz_report_error(ctx);
                fz_warn(ctx, "Cannot draw OFD signature appearance");
            }
        }
    }
    fz_always(ctx) {
        fz_drop_xml(ctx, index);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

static fz_page* ofd_load_page(fz_context* ctx, fz_document* doc_, int chapter, int number) {
    ofd_document* doc = (ofd_document*)doc_;
    if (chapter != 0 || number < 0 || number >= doc->count) fz_throw(ctx, FZ_ERROR_ARGUMENT, "OFD page out of range");
    fz_xml* entry = fz_xml_down(ofd_child(fz_xml_root(doc->xml), "Pages"));
    for (int i = 0; entry; entry = fz_xml_next(entry)) {
        if (ofd_is(entry, "Page") && i++ == number) break;
    }
    ofd_page* page = fz_new_derived_page(ctx, ofd_page, doc_);
    page->super.drop_page = ofd_drop_page;
    page->super.bound_page = ofd_bound_page;
    page->super.run_page_contents = ofd_run_page;
    fz_xml* xml = NULL;
    fz_device* dev = NULL;
    fz_var(xml);
    fz_var(dev);
    fz_try(ctx) {
        char path[OFD_PATH_SIZE];
        ofd_resolve(ctx, path, doc->path, fz_xml_att(entry, "BaseLoc"));
        xml = ofd_read_xml(ctx, doc, path);
        fz_xml* root = fz_xml_root(xml);
        if (ofd_child(root, "PageRes")) fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "OFD page resources are not supported yet");
        fz_xml* area = ofd_child(root, "Area");
        fz_xml* physical = ofd_child(area, "PhysicalBox");
        if (!physical) physical = ofd_child(ofd_child(doc->common, "PageArea"), "PhysicalBox");
        fz_rect box = ofd_box(ofd_text(physical));
        if (fz_is_empty_rect(box)) fz_throw(ctx, FZ_ERROR_FORMAT, "Missing OFD page dimensions");
        fz_matrix ctm = fz_concat(fz_translate(-box.x0, -box.y0), fz_scale(ofd_points_per_mm, ofd_points_per_mm));
        page->bounds = fz_transform_rect(box, ctm);
        page->list = fz_new_display_list(ctx, page->bounds);
        dev = fz_new_list_device(ctx, page->list);
        ofd_templates(ctx, doc, root, dev, ctm, "Background");
        ofd_draw_nodes(ctx, doc, ofd_child(root, "Content"), dev, ctm, 0);
        ofd_templates(ctx, doc, root, dev, ctm, "Foreground");
        ofd_annots(ctx, doc, fz_xml_att(entry, "ID"), dev, ctm);
        ofd_signatures(ctx, doc, fz_xml_att(entry, "ID"), dev, ctm);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
        fz_drop_xml(ctx, xml);
    }
    fz_catch(ctx) {
        fz_drop_page(ctx, &page->super);
        fz_rethrow(ctx);
    }
    return &page->super;
}

static int ofd_count_pages(fz_context* ctx, fz_document* doc, int chapter) {
    return chapter == 0 ? ((ofd_document*)doc)->count : 0;
}

static int ofd_metadata(fz_context* ctx, fz_document* doc, const char* key, char* buf, size_t size) {
    if (strcmp(key, FZ_META_FORMAT)) return -1;
    return 1 + (int)fz_strlcpy(buf, "OFD", size);
}

static void ofd_drop_document(fz_context* ctx, fz_document* doc_) {
    ofd_document* doc = (ofd_document*)doc_;
    while (doc->fonts) {
        ofd_font_entry* entry = doc->fonts;
        doc->fonts = entry->next;
        fz_drop_font(ctx, entry->font);
        fz_free(ctx, entry);
    }
    while (doc->resources) {
        ofd_resource* res = doc->resources;
        doc->resources = res->next;
        fz_drop_xml(ctx, res->xml);
        fz_free(ctx, res);
    }
    fz_drop_xml(ctx, doc->xml);
    fz_drop_archive(ctx, doc->archive);
}

/* Open OFD directly so rendering, text extraction and printing share the same display list. */
static fz_document* ofd_open_document(fz_context* ctx, const fz_document_handler* handler, fz_stream* stream,
                                      fz_stream* accel, fz_archive* dir, void* state) {
    ofd_document* doc = fz_new_derived_document(ctx, ofd_document);
    doc->super.drop_document = ofd_drop_document;
    doc->super.count_pages = ofd_count_pages;
    doc->super.load_page = ofd_load_page;
    doc->super.lookup_metadata = ofd_metadata;
    fz_xml* index = NULL;
    fz_var(index);
    fz_try(ctx) {
        doc->archive = fz_open_archive_with_stream(ctx, stream);
        index = ofd_read_xml(ctx, doc, "OFD.xml");
        fz_xml* body = ofd_child(fz_xml_root(index), "DocBody");
        fz_xml* signatures = ofd_child(body, "Signatures");
        if (signatures) ofd_resolve(ctx, doc->signatures, "OFD.xml", ofd_text(signatures));
        ofd_resolve(ctx, doc->path, "OFD.xml", ofd_text(ofd_child(body, "DocRoot")));
        doc->xml = ofd_read_xml(ctx, doc, doc->path);
        doc->common = ofd_child(fz_xml_root(doc->xml), "CommonData");
        ofd_load_resources(ctx, doc, doc->common, doc->path, "PublicRes");
        ofd_load_resources(ctx, doc, doc->common, doc->path, "DocumentRes");
        for (fz_xml* node = fz_xml_down(ofd_child(fz_xml_root(doc->xml), "Pages")); node; node = fz_xml_next(node)) {
            if (ofd_is(node, "Page")) ++doc->count;
        }
        if (!doc->count) fz_throw(ctx, FZ_ERROR_FORMAT, "OFD contains no pages");
    }
    fz_always(ctx) {
        fz_drop_xml(ctx, index);
    }
    fz_catch(ctx) {
        fz_drop_document(ctx, &doc->super);
        fz_rethrow(ctx);
    }
    return &doc->super;
}

static int ofd_recognize(fz_context* ctx, const fz_document_handler* handler, fz_stream* stream, fz_archive* dir,
                         void** state, fz_document_recognize_state_free_fn** free_state) {
    fz_archive* archive = NULL;
    int score = 0;
    if (!stream || !fz_is_zip_archive(ctx, stream)) return 0;
    fz_var(archive);
    fz_var(score);
    fz_try(ctx) {
        archive = fz_open_archive_with_stream(ctx, stream);
        score = fz_has_archive_entry(ctx, archive, "OFD.xml") ? 100 : 0;
    }
    fz_always(ctx) {
        fz_drop_archive(ctx, archive);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
    return score;
}

void fz_register_ofd_handler(fz_context* ctx) {
    static const char* extensions[] = {"ofd", NULL};
    static const char* mimetypes[] = {"application/ofd", NULL};
    static const fz_document_handler handler = {NULL, ofd_open_document, extensions, mimetypes, ofd_recognize};
    fz_register_document_handler(ctx, &handler);
}

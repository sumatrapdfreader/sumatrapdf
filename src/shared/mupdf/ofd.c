/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "mupdf/fitz.h"
#include <ctype.h>
#include "ofd-crypto.h"
#include "ofd-seal.h"
#include "ofd.h"

/* GB/T 33190 uses millimetres; MuPDF uses points. */
static const float ofd_points_per_mm = 72.0f / 25.4f;
enum {
    OFD_PATH_SIZE = 4096,
    OFD_MAX_DEPTH = 32,
    OFD_MAX_SEAL_DEPTH = 4,
    OFD_MAX_GLYPHS = 1000000,
    OFD_MAX_DASHES = 128,
    OFD_ID_TABLE_SIZE = 1024
};

typedef struct ofd_resource {
    fz_xml* xml;
    char file[OFD_PATH_SIZE]; // the resource dictionary's own member name
    char path[OFD_PATH_SIZE]; // base for its relative paths (BaseLoc)
    struct ofd_resource* next;
} ofd_resource;

// what ids (unique within a document) map to
typedef struct {
    fz_xml* node;
    ofd_resource* res;
} ofd_id_entry;

typedef struct ofd_font_entry {
    fz_xml* definition;
    fz_font* font;
    int embedded;
    int system;
    struct ofd_font_entry* next;
} ofd_font_entry;

// One DocBody of a package: ids, resources and paths are scoped to it.
typedef struct {
    fz_archive* archive; // owned by the package
    fz_xml* info;        // DocBody/DocInfo in the package's OFD.xml
    fz_xml* xml;
    fz_xml* common;
    ofd_resource* resources;
    fz_hash_table* ids;
    ofd_font_entry* fonts;
    char path[OFD_PATH_SIZE];
    char signatures[OFD_PATH_SIZE];
    int count;
    int seal_depth;
} ofd_document;

// The fz_document: an OFD package, its documents' pages one after another.
typedef struct {
    fz_document super;
    fz_archive* archive;
    fz_xml* index; // OFD.xml
    ofd_document* docs;
    int ndocs;
    int seal_depth;
} ofd_package;

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

// A color component: decimal, or hex with a leading '#' ("#FF").
static float ofd_component(const char** input) {
    const char* s = *input;
    if (!s) return 0;
    while (*s && (isspace((unsigned char)*s) || *s == ',')) ++s;
    if (*s != '#') return ofd_number(input);
    char* end;
    long value = strtol(s + 1, &end, 16);
    *input = end;
    return (float)value;
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

static int ofd_parse_id(const char* id, unsigned int* out) {
    char* end;
    if (!id || !isdigit((unsigned char)*id)) return 0;
    unsigned long n = strtoul(id, &end, 10);
    if (*end || n > 0xffffffffUL) return 0;
    *out = (unsigned int)n;
    return 1;
}

static void ofd_drop_id_entry(fz_context* ctx, void* entry) {
    fz_free(ctx, entry);
}

// Index elements with an ID so lookups don't walk every resource tree.
static void ofd_index_ids(fz_context* ctx, ofd_document* doc, ofd_resource* res, fz_xml* node, int depth) {
    if (depth > OFD_MAX_DEPTH) return;
    for (; node; node = fz_xml_next(node)) {
        unsigned int key;
        if (ofd_parse_id(fz_xml_att(node, "ID"), &key) && !fz_hash_find(ctx, doc->ids, &key)) {
            ofd_id_entry* entry = fz_malloc_struct(ctx, ofd_id_entry);
            entry->node = node;
            entry->res = res;
            fz_try(ctx) {
                fz_hash_insert(ctx, doc->ids, &key, entry);
            }
            fz_catch(ctx) {
                fz_free(ctx, entry);
                fz_rethrow(ctx);
            }
        }
        ofd_index_ids(ctx, doc, res, fz_xml_down(node), depth + 1);
    }
}

// Loads each resource dictionary once; page and template resources accumulate as pages load.
static void ofd_load_resources(fz_context* ctx, ofd_document* doc, fz_xml* parent, const char* path, const char* tag) {
    for (fz_xml* node = fz_xml_down(parent); node; node = fz_xml_next(node)) {
        if (!ofd_is(node, tag)) continue;
        char file[OFD_PATH_SIZE];
        ofd_resolve(ctx, file, path, ofd_text(node));
        if (!fz_has_archive_entry(ctx, doc->archive, file)) {
            fz_warn(ctx, "Missing OFD resource dictionary: %s", file);
            continue;
        }
        int loaded = 0;
        for (ofd_resource* res = doc->resources; res && !loaded; res = res->next) loaded = !strcmp(res->file, file);
        if (loaded) continue;

        fz_xml* xml = ofd_read_xml(ctx, doc, file);
        ofd_resource* res = NULL;
        fz_try(ctx) {
            res = fz_malloc_struct(ctx, ofd_resource);
        }
        fz_catch(ctx) {
            fz_drop_xml(ctx, xml);
            fz_rethrow(ctx);
        }
        res->xml = xml;
        res->next = doc->resources;
        doc->resources = res;
        fz_strlcpy(res->file, file, sizeof(res->file));
        fz_strlcpy(res->path, file, sizeof(res->path));
        const char* base = fz_xml_att(fz_xml_root(res->xml), "BaseLoc");
        if (base) {
            char resolved[OFD_PATH_SIZE];
            ofd_resolve(ctx, resolved, res->path, base);
            if (fz_snprintf(res->path, sizeof(res->path), "%s/", resolved) >= sizeof(res->path))
                fz_throw(ctx, FZ_ERROR_LIMIT, "OFD resource path too long");
        }
        ofd_index_ids(ctx, doc, res, fz_xml_root(res->xml), 0);
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

static fz_xml* ofd_resource_id(fz_context* ctx, ofd_document* doc, const char* tag, const char* id, const char** path) {
    unsigned int key;
    if (!id) return NULL;
    ofd_id_entry* entry = ofd_parse_id(id, &key) ? fz_hash_find(ctx, doc->ids, &key) : NULL;
    if (entry && ofd_is(entry->node, tag)) {
        if (path) *path = entry->res->path;
        return entry->node;
    }
    // non-numeric or duplicate ids
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
static void ofd_brush_clipped(fz_context* ctx, ofd_document* doc, fz_xml* color, fz_device* dev, fz_rect area,
                              fz_matrix ctm, float alpha, int depth);

static void ofd_apply_paint(fz_context* ctx, ofd_document* doc, fz_xml* node, ofd_paint* paint, int depth) {
    if (depth > OFD_MAX_DEPTH) fz_throw(ctx, FZ_ERROR_LIMIT, "OFD draw parameter cycle");
    const char* id = fz_xml_att(node, "DrawParam");
    if (!id) id = fz_xml_att(node, "Relative");
    fz_xml* param = ofd_resource_id(ctx, doc, "DrawParam", id, NULL);
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
    if (ofd_child(value, "GouraudShd") || ofd_child(value, "LaGouraudShd"))
        fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "OFD mesh gradients are not supported yet");
    const char* s = fz_xml_att(value, "Value");
    fz_xml* space = ofd_resource_id(ctx, doc, "ColorSpace", fz_xml_att(value, "ColorSpace"), NULL);
    // Index selects an entry of the color space's palette, e.g. <CV>#FF #00 #00</CV>
    const char* index = fz_xml_att(value, "Index");
    if (!s && index) {
        int wanted = (int)ofd_attr(value, "Index", 0);
        int i = 0;
        for (fz_xml* cv = fz_xml_down(ofd_child(space, "Palette")); cv; cv = fz_xml_next(cv)) {
            if (ofd_is(cv, "CV") && i++ == wanted) s = ofd_text(cv);
        }
        if (!s) fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD palette index");
    }
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
    for (int i = 0; i < n; ++i) color[i] = fz_clamp(ofd_component(&s) / max, 0, 1);
    return ofd_attr(node, "Alpha", 255) / 255 * ofd_attr(value, "Alpha", 255) / 255;
}

static ofd_font_entry* ofd_font(fz_context* ctx, ofd_document* doc, fz_xml* node) {
    const char* base = NULL;
    fz_xml* definition = ofd_resource_id(ctx, doc, "Font", fz_xml_att(node, "Font"), &base);
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

// A CGTransform: codes [start, start + codes) of a TextObject map to glyphs.
typedef struct {
    int start;
    int codes;
    int count;
    int* glyphs;
} ofd_cgt;

static void ofd_drop_cgts(fz_context* ctx, ofd_cgt* cgts, int n) {
    for (int i = 0; i < n; ++i) fz_free(ctx, cgts[i].glyphs);
    fz_free(ctx, cgts);
}

// Parse a TextObject's glyph mappings once instead of rescanning them for every character.
static ofd_cgt* ofd_load_cgts(fz_context* ctx, fz_xml* node, int* count_out) {
    int n = 0;
    for (fz_xml* m = fz_xml_down(node); m; m = fz_xml_next(m)) n += ofd_is(m, "CGTransform");
    *count_out = 0;
    if (!n) return NULL;
    ofd_cgt* cgts = fz_calloc(ctx, n, sizeof(ofd_cgt));
    int i = 0;
    fz_try(ctx) {
        for (fz_xml* m = fz_xml_down(node); m; m = fz_xml_next(m)) {
            if (!ofd_is(m, "CGTransform")) continue;
            ofd_cgt* c = &cgts[i++];
            c->start = ofd_text_count(ctx, m, "CodePosition", 0);
            c->codes = ofd_text_count(ctx, m, "CodeCount", 1);
            c->count = ofd_text_count(ctx, m, "GlyphCount", 1);
            if (c->codes <= 0 || c->count <= 0) fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid OFD glyph count");
            c->glyphs = fz_malloc_array(ctx, c->count, int);
            const char* s = ofd_text(ofd_child(m, "Glyphs"));
            for (int k = 0; k < c->count; ++k) {
                const char* before = s;
                float glyph = ofd_number(&s);
                int valid = s != before && glyph >= 0 && glyph <= OFD_MAX_GLYPHS && glyph == floorf(glyph);
                c->glyphs[k] = valid ? (int)glyph : -1;
            }
        }
    }
    fz_catch(ctx) {
        ofd_drop_cgts(ctx, cgts, n);
        fz_rethrow(ctx);
    }
    *count_out = n;
    return cgts;
}

static ofd_cgt* ofd_glyph_mapping(ofd_cgt* cgts, int n, int position) {
    for (int i = 0; i < n; ++i) {
        if (position >= cgts[i].start && position - cgts[i].start < cgts[i].codes) return &cgts[i];
    }
    return NULL;
}

static fz_text* ofd_make_text(fz_context* ctx, ofd_document* doc, fz_xml* node) {
    ofd_font_entry* entry = ofd_font(ctx, doc, node);
    fz_text* text = fz_new_text(ctx);
    ofd_cgt* cgts = NULL;
    int ncgts = 0;
    fz_var(cgts);
    fz_var(ncgts);
    fz_try(ctx) {
        if (entry->embedded || entry->system) cgts = ofd_load_cgts(ctx, node, &ncgts);
        float size = ofd_attr(node, "Size", 0);
        fz_matrix glyph_trm = fz_scale(size * ofd_attr(node, "HScale", 1), -size);
        glyph_trm = fz_concat(glyph_trm, fz_rotate(ofd_attr(node, "CharDirection", 0)));
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
                ofd_cgt* mapping = ofd_glyph_mapping(cgts, ncgts, position);
                int index = mapping ? position - mapping->start : 0;
                int glyphs = mapping ? mapping->count : 1;
                int codes = mapping ? mapping->codes : 1;
                // Subset glyph IDs cannot address a substituted full font; retain real ligatures.
                if (!entry->embedded && codes == glyphs) {
                    mapping = NULL;
                    index = 0;
                    glyphs = codes = 1;
                }
                int glyph = mapping ? (index < glyphs ? mapping->glyphs[index] : -1)
                                    : fz_encode_character_with_fallback(ctx, font, rune, 0, FZ_LANG_UNSET, &font);
                int outputs = mapping && codes == 1 ? glyphs : 1;
                for (int output = 0; output < outputs; ++output) {
                    if (output) glyph = mapping->glyphs[output];
                    if (glyph >= font->glyph_count) glyph = 0;
                    fz_matrix trm = glyph_trm;
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
    fz_always(ctx) {
        ofd_drop_cgts(ctx, cgts, ncgts);
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
                ofd_brush_clipped(ctx, doc, paint.fill, dev, fz_bound_text(ctx, text, NULL, fz_identity), ctm, alpha,
                                  depth + 1);
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
                ofd_brush_clipped(ctx, doc, paint.stroke, dev, fz_bound_text(ctx, text, stroke, fz_identity), ctm,
                                  alpha, depth + 1);
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

// SVG-style elliptical arc from the current point to (x, y), as cubic Beziers of at most 90 degrees.
static void ofd_arc(fz_context* ctx, fz_path* path, const float* v) {
    float rx = fabsf(v[0]), ry = fabsf(v[1]);
    int large = v[3] != 0, sweep = v[4] != 0;
    float x = v[5], y = v[6];
    fz_point p0 = fz_currentpoint(ctx, path);
    if (rx == 0 || ry == 0 || (p0.x == x && p0.y == y)) {
        fz_lineto(ctx, path, x, y);
        return;
    }

    // endpoint to center parameterization (SVG 1.1, appendix F.6.5)
    float phi = v[2] * FZ_PI / 180;
    float cosp = cosf(phi), sinp = sinf(phi);
    float dx = (p0.x - x) / 2, dy = (p0.y - y) / 2;
    float x1 = cosp * dx + sinp * dy;
    float y1 = -sinp * dx + cosp * dy;
    float lambda = (x1 * x1) / (rx * rx) + (y1 * y1) / (ry * ry);
    if (lambda > 1) {
        rx *= sqrtf(lambda);
        ry *= sqrtf(lambda);
    }
    float num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
    float den = rx * rx * y1 * y1 + ry * ry * x1 * x1;
    float coef = num > 0 && den > 0 ? sqrtf(num / den) : 0;
    if (large == sweep) coef = -coef;
    float cx1 = coef * rx * y1 / ry, cy1 = -coef * ry * x1 / rx;
    float cx = cosp * cx1 - sinp * cy1 + (p0.x + x) / 2;
    float cy = sinp * cx1 + cosp * cy1 + (p0.y + y) / 2;
    float theta = atan2f((y1 - cy1) / ry, (x1 - cx1) / rx);
    float dtheta = atan2f((-y1 - cy1) / ry, (-x1 - cx1) / rx) - theta;
    if (sweep && dtheta < 0) dtheta += 2 * FZ_PI;
    if (!sweep && dtheta > 0) dtheta -= 2 * FZ_PI;

    int n = (int)ceilf(fabsf(dtheta) / (FZ_PI / 2) - 0.001f);
    if (n < 1) n = 1;
    float delta = dtheta / n;
    float t = 4.0f / 3.0f * tanf(delta / 4);
    for (int i = 0; i < n; ++i) {
        float a1 = theta + i * delta, a2 = a1 + delta;
        float c1 = cosf(a1), s1 = sinf(a1), c2 = cosf(a2), s2 = sinf(a2);
        float u[3] = {c1 - t * s1, c2 + t * s2, c2};
        float w[3] = {s1 + t * c1, s2 - t * c2, s2};
        fz_point p[3];
        for (int k = 0; k < 3; ++k) {
            p[k].x = cx + rx * u[k] * cosp - ry * w[k] * sinp;
            p[k].y = cy + rx * u[k] * sinp + ry * w[k] * cosp;
        }
        if (i == n - 1) p[2] = fz_make_point(x, y);
        fz_curveto(ctx, path, p[0].x, p[0].y, p[1].x, p[1].y, p[2].x, p[2].y);
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
            float v[7];
            int n = command == 'A'                                         ? 7
                    : command == 'B'                                       ? 6
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
                case 'A':
                    ofd_arc(ctx, path, v);
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
    fz_try(ctx) {
        fz_begin_tile(ctx, dev, coverage, fz_make_rect(0, 0, w, h), xstep, ystep, matrix);
        fz_try(ctx) {
            ofd_paint paint = {NULL, NULL, 0.353f};
            ofd_draw_tree(ctx, doc, fz_xml_down(ofd_child(node, "CellContent")), dev, matrix, depth + 1, paint);
        }
        fz_always(ctx) {
            fz_end_tile(ctx, dev);
        }
        fz_catch(ctx) {
            fz_rethrow(ctx);
        }
    }
    fz_always(ctx) {
        fz_end_group(ctx, dev);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

// Paints a brush inside a clip the caller pushed, popping it even on errors.
static void ofd_brush_clipped(fz_context* ctx, ofd_document* doc, fz_xml* color, fz_device* dev, fz_rect area,
                              fz_matrix ctm, float alpha, int depth) {
    fz_try(ctx) {
        ofd_brush(ctx, doc, color, dev, area, ctm, alpha, depth);
    }
    fz_always(ctx) {
        fz_pop_clip(ctx, dev);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
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
                ofd_brush_clipped(ctx, doc, paint.fill, dev, fz_bound_path(ctx, path, NULL, fz_identity), ctm, alpha,
                                  depth + 1);
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
                ofd_brush_clipped(ctx, doc, paint.stroke, dev, fz_bound_path(ctx, path, stroke, fz_identity), ctm,
                                  alpha, depth + 1);
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
    fz_xml* res = ofd_resource_id(ctx, doc, "MultiMedia", fz_xml_att(node, "ResourceID"), &base);
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

static void ofd_clip_area(fz_context* ctx, ofd_document* doc, fz_xml* area, fz_device* dev, fz_matrix base,
                          ofd_paint paint) {
    const float white[] = {1};
    ofd_apply_paint(ctx, doc, area, &paint, 0);
    fz_matrix matrix = ofd_matrix(area, base);
    fz_xml* shape = ofd_child(area, "Path");
    if (shape) {
        fz_path* path = ofd_make_path(ctx, shape);
        fz_stroke_state* stroke = NULL;
        fz_var(stroke);
        fz_try(ctx) {
            fz_matrix transform = ofd_matrix(shape, matrix);
            if (fz_xml_att_eq(shape, "Fill", "true"))
                fz_fill_path(ctx, dev, path, fz_xml_att_eq(shape, "Rule", "Even-Odd"), transform, fz_device_gray(ctx),
                             white, 1, fz_default_color_params);
            if (!fz_xml_att_eq(shape, "Stroke", "false")) {
                ofd_apply_paint(ctx, doc, shape, &paint, 0);
                stroke = ofd_make_stroke(ctx, paint);
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

// Pushes one mask per Clip; *count tracks pushed masks so the caller can pop them even on errors.
static void ofd_apply_clips(fz_context* ctx, ofd_document* doc, fz_xml* node, fz_device* dev, fz_matrix parent,
                            fz_matrix ctm, ofd_paint paint, int* count) {
    fz_xml* clips = ofd_child(node, "Clips");
    fz_rect box = ofd_box(fz_xml_att(node, "Boundary"));
    fz_matrix base = fz_xml_att_eq(clips, "TransFlag", "true") ? ctm : fz_concat(fz_translate(box.x0, box.y0), parent);
    fz_rect bounds = fz_xml_att(node, "Boundary") ? fz_transform_rect(box, parent) : fz_infinite_rect;
    const float black[] = {0};
    for (fz_xml* clip = fz_xml_down(clips); clip; clip = fz_xml_next(clip)) {
        if (!ofd_is(clip, "Clip")) continue;
        // Areas within a Clip form a union; successive Clips intersect.
        fz_begin_mask(ctx, dev, bounds, 1, fz_device_gray(ctx), black, fz_default_color_params);
        for (fz_xml* area = fz_xml_down(clip); area; area = fz_xml_next(area)) {
            if (!ofd_is(area, "Area")) continue;
            fz_try(ctx) {
                ofd_clip_area(ctx, doc, area, dev, base, paint);
            }
            fz_catch(ctx) {
                fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
                fz_report_error(ctx);
                fz_warn(ctx, "Skipping OFD clip area");
            }
        }
        fz_end_mask(ctx, dev);
        ++*count;
    }
}

static void ofd_draw_tree(fz_context* ctx, ofd_document* doc, fz_xml* node, fz_device* dev, fz_matrix parent, int depth,
                          ofd_paint inherited) {
    if (depth > OFD_MAX_DEPTH) fz_throw(ctx, FZ_ERROR_LIMIT, "OFD nesting too deep");
    for (; node; node = fz_xml_next(node)) {
        if (fz_xml_att_eq(node, "Visible", "false") || ofd_attr(node, "Alpha", 255) <= 0) continue;
        fz_matrix ctm = ofd_matrix(node, parent);
        int container = ofd_is(node, "Content") || ofd_is(node, "Layer") || ofd_is(node, "PageBlock") ||
                        ofd_is(node, "Appearance") || ofd_is(node, "CompositeObject");
        int clips = 0, grouped = 0;
        fz_var(clips);
        fz_var(grouped);
        // An object that fails (unsupported feature, bad data) is skipped, not the whole page.
        fz_try(ctx) {
            ofd_paint paint = inherited;
            ofd_apply_paint(ctx, doc, node, &paint, 0);
            ofd_apply_clips(ctx, doc, node, dev, parent, ctm, paint, &clips);
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
            else if (ofd_is(node, "ImageObject"))
                ofd_draw_image(ctx, doc, node, dev, ctm);
            else if (ofd_is(node, "CompositeObject")) {
                fz_xml* unit = ofd_resource_id(ctx, doc, "CompositeGraphicUnit", fz_xml_att(node, "ResourceID"), NULL);
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
            fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
            fz_report_error(ctx);
            fz_warn(ctx, "Skipping OFD %s", fz_xml_tag(node));
        }
    }
}

static void ofd_draw_nodes(fz_context* ctx, ofd_document* doc, fz_xml* node, fz_device* dev, fz_matrix ctm, int depth) {
    ofd_paint paint = {NULL, NULL, 0.353f};
    ofd_draw_tree(ctx, doc, node, dev, ctm, depth, paint);
}

// Logs a non-fatal error so an optional part (template, annotation, seal) doesn't fail the page.
static void ofd_skip(fz_context* ctx, const char* what) {
    fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
    fz_report_error(ctx);
    fz_warn(ctx, "Skipping OFD %s", what);
}

static void ofd_draw_page_xml(fz_context* ctx, ofd_document* doc, fz_xml* root, const char* path, fz_device* dev,
                              fz_matrix ctm, int depth);

static void ofd_templates(fz_context* ctx, ofd_document* doc, fz_xml* page, fz_device* dev, fz_matrix ctm,
                          const char* order, int depth) {
    for (fz_xml* node = fz_xml_down(page); node; node = fz_xml_next(node)) {
        if (!ofd_is(node, "Template")) continue;
        fz_xml* tpl = ofd_find_id(doc->common, "TemplatePage", fz_xml_att(node, "TemplateID"), 0);
        const char* z = fz_xml_att(node, "ZOrder");
        if (!z) z = fz_xml_att(tpl, "ZOrder");
        if (strcmp(z ? z : "Background", order)) continue;
        fz_xml* xml = NULL;
        fz_var(xml);
        fz_try(ctx) {
            if (!tpl) fz_throw(ctx, FZ_ERROR_FORMAT, "Missing OFD template %s", fz_xml_att(node, "TemplateID"));
            char path[OFD_PATH_SIZE];
            ofd_resolve(ctx, path, doc->path, fz_xml_att(tpl, "BaseLoc"));
            xml = ofd_read_xml(ctx, doc, path);
            ofd_draw_page_xml(ctx, doc, fz_xml_root(xml), path, dev, ctm, depth + 1);
        }
        fz_always(ctx) {
            fz_drop_xml(ctx, xml);
        }
        fz_catch(ctx) {
            ofd_skip(ctx, "template");
        }
    }
}

// Draws a page or template page: its resources, background templates, content, foreground templates.
static void ofd_draw_page_xml(fz_context* ctx, ofd_document* doc, fz_xml* root, const char* path, fz_device* dev,
                              fz_matrix ctm, int depth) {
    if (depth > OFD_MAX_DEPTH) fz_throw(ctx, FZ_ERROR_LIMIT, "OFD templates nested too deep");
    ofd_load_resources(ctx, doc, root, path, "PageRes");
    ofd_templates(ctx, doc, root, dev, ctm, "Background", depth);
    ofd_draw_nodes(ctx, doc, ofd_child(root, "Content"), dev, ctm, 0);
    ofd_templates(ctx, doc, root, dev, ctm, "Foreground", depth);
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
                if (fz_xml_att_eq(annot, "Visible", "false")) continue;
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
    if (doc->seal_depth >= OFD_MAX_SEAL_DEPTH) fz_throw(ctx, FZ_ERROR_LIMIT, "OFD seal nesting too deep");
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
            ((ofd_package*)inner)->seal_depth = doc->seal_depth + 1;
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

static fz_xml* ofd_page_entry(ofd_document* doc, int number) {
    fz_xml* entry = fz_xml_down(ofd_child(fz_xml_root(doc->xml), "Pages"));
    for (int i = 0; entry; entry = fz_xml_next(entry)) {
        if (ofd_is(entry, "Page") && i++ == number) break;
    }
    return entry;
}

// The package's pages are its documents' pages in order, in one chapter.
static int ofd_first_page(ofd_package* pkg, int doc_index) {
    int n = 0;
    for (int i = 0; i < doc_index; ++i) n += pkg->docs[i].count;
    return n;
}

static fz_page* ofd_load_page(fz_context* ctx, fz_document* doc_, int chapter, int number) {
    ofd_package* pkg = (ofd_package*)doc_;
    int i = 0;
    while (i < pkg->ndocs && number >= pkg->docs[i].count) number -= pkg->docs[i++].count;
    if (chapter != 0 || number < 0 || i == pkg->ndocs) fz_throw(ctx, FZ_ERROR_ARGUMENT, "OFD page out of range");
    ofd_document* doc = &pkg->docs[i];
    doc->seal_depth = pkg->seal_depth;
    fz_xml* entry = ofd_page_entry(doc, number);
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
        fz_xml* area = ofd_child(root, "Area");
        fz_xml* physical = ofd_child(area, "PhysicalBox");
        if (!physical) physical = ofd_child(ofd_child(doc->common, "PageArea"), "PhysicalBox");
        fz_rect box = ofd_box(ofd_text(physical));
        if (fz_is_empty_rect(box)) fz_throw(ctx, FZ_ERROR_FORMAT, "Missing OFD page dimensions");
        fz_matrix ctm = fz_concat(fz_translate(-box.x0, -box.y0), fz_scale(ofd_points_per_mm, ofd_points_per_mm));
        page->bounds = fz_transform_rect(box, ctm);
        page->list = fz_new_display_list(ctx, page->bounds);
        dev = fz_new_list_device(ctx, page->list);
        ofd_draw_page_xml(ctx, doc, root, path, dev, ctm, 0);
        fz_try(ctx) {
            ofd_annots(ctx, doc, fz_xml_att(entry, "ID"), dev, ctm);
        }
        fz_catch(ctx) {
            ofd_skip(ctx, "annotations");
        }
        fz_try(ctx) {
            ofd_signatures(ctx, doc, fz_xml_att(entry, "ID"), dev, ctm);
        }
        fz_catch(ctx) {
            ofd_skip(ctx, "signatures");
        }
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

static int ofd_count_pages(fz_context* ctx, fz_document* doc_, int chapter) {
    ofd_package* pkg = (ofd_package*)doc_;
    return chapter == 0 ? ofd_first_page(pkg, pkg->ndocs) : 0;
}

static int ofd_metadata(fz_context* ctx, fz_document* doc_, const char* key, char* buf, size_t size) {
    static const struct {
        const char* key;
        const char* tag;
    } info_keys[] = {
        {FZ_META_INFO_TITLE, "Title"},
        {FZ_META_INFO_AUTHOR, "Author"},
        {FZ_META_INFO_SUBJECT, "Subject"},
        {FZ_META_INFO_KEYWORDS, "Keywords"},
        {FZ_META_INFO_CREATOR, "Creator"},
        {FZ_META_INFO_CREATIONDATE, "CreationDate"},
        {FZ_META_INFO_MODIFICATIONDATE, "ModDate"},
    };
    ofd_package* pkg = (ofd_package*)doc_;
    if (!strcmp(key, FZ_META_FORMAT)) return 1 + (int)fz_strlcpy(buf, "OFD", size);
    for (size_t i = 0; i < nelem(info_keys); ++i) {
        if (strcmp(key, info_keys[i].key)) continue;
        fz_xml* node = ofd_child(pkg->docs[0].info, info_keys[i].tag);
        if (!node) return -1;
        return 1 + (int)fz_strlcpy(buf, ofd_text(node), size);
    }
    return -1;
}

// 0-based index of the document's page with this ID, -1 if none
static int ofd_page_by_id(ofd_document* doc, const char* id) {
    int i = 0;
    for (fz_xml* entry = fz_xml_down(ofd_child(fz_xml_root(doc->xml), "Pages")); entry; entry = fz_xml_next(entry)) {
        if (!ofd_is(entry, "Page")) continue;
        if (fz_xml_att_eq(entry, "ID", id)) return i;
        ++i;
    }
    return -1;
}

// page: 0-based, across all documents
static fz_outline* ofd_new_outline(fz_context* ctx, const char* title, int page) {
    fz_outline* item = fz_new_outline(ctx);
    fz_try(ctx) {
        item->title = fz_strdup(ctx, title);
        item->page = fz_make_location(0, page);
        char uri[32];
        fz_snprintf(uri, sizeof(uri), "#page=%d", page + 1);
        item->uri = fz_strdup(ctx, uri);
    }
    fz_catch(ctx) {
        fz_drop_outline(ctx, item);
        fz_rethrow(ctx);
    }
    return item;
}

// <OutlineElem Title=".."><Actions><Action><Goto><Dest PageID=".."/> with nested OutlineElems.
static fz_outline* ofd_outline_elems(fz_context* ctx, ofd_package* pkg, int doc_index, fz_xml* node, int depth) {
    if (depth > OFD_MAX_DEPTH) return NULL;
    ofd_document* doc = &pkg->docs[doc_index];
    int first = ofd_first_page(pkg, doc_index);
    fz_outline* head = NULL;
    fz_outline** tail = &head;
    fz_try(ctx) {
        for (; node; node = fz_xml_next(node)) {
            if (!ofd_is(node, "OutlineElem")) continue;
            fz_xml* dest = ofd_child(ofd_child(ofd_child(ofd_child(node, "Actions"), "Action"), "Goto"), "Dest");
            int page = ofd_page_by_id(doc, fz_xml_att(dest, "PageID"));
            const char* title = fz_xml_att(node, "Title");
            *tail = ofd_new_outline(ctx, title ? title : "", first + (page < 0 ? 0 : page));
            (*tail)->is_open = !fz_xml_att_eq(node, "Expanded", "false");
            (*tail)->down = ofd_outline_elems(ctx, pkg, doc_index, fz_xml_down(node), depth + 1);
            tail = &(*tail)->next;
        }
    }
    fz_catch(ctx) {
        fz_drop_outline(ctx, head);
        fz_rethrow(ctx);
    }
    return head;
}

// The documents' own outlines; with several documents, an entry per document over its outline.
static fz_outline* ofd_load_outline(fz_context* ctx, fz_document* doc_) {
    ofd_package* pkg = (ofd_package*)doc_;
    if (pkg->ndocs == 1) {
        fz_xml* outlines = ofd_child(fz_xml_root(pkg->docs[0].xml), "Outlines");
        return ofd_outline_elems(ctx, pkg, 0, fz_xml_down(outlines), 0);
    }
    fz_outline* head = NULL;
    fz_outline** tail = &head;
    fz_try(ctx) {
        for (int i = 0; i < pkg->ndocs; ++i) {
            char title[64];
            const char* name = ofd_text(ofd_child(pkg->docs[i].info, "Title"));
            if (!*name) {
                fz_snprintf(title, sizeof(title), "Document %d", i + 1);
                name = title;
            }
            *tail = ofd_new_outline(ctx, name, ofd_first_page(pkg, i));
            (*tail)->is_open = 1;
            fz_xml* outlines = ofd_child(fz_xml_root(pkg->docs[i].xml), "Outlines");
            (*tail)->down = ofd_outline_elems(ctx, pkg, i, fz_xml_down(outlines), 0);
            tail = &(*tail)->next;
        }
    }
    fz_catch(ctx) {
        fz_drop_outline(ctx, head);
        fz_rethrow(ctx);
    }
    return head;
}

// Only "#page=N" (1-based, across all documents), as written by ofd_new_outline().
static fz_link_dest ofd_resolve_link(fz_context* ctx, fz_document* doc, const char* uri) {
    if (!uri || strncmp(uri, "#page=", 6)) return fz_make_link_dest_none();
    int number = atoi(uri + 6) - 1;
    if (number < 0 || number >= fz_count_pages(ctx, doc)) return fz_make_link_dest_none();
    return fz_make_link_dest_xyz(0, number, 0, 0, 0);
}

static void ofd_drop_body(fz_context* ctx, ofd_document* doc) {
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
    fz_drop_hash_table(ctx, doc->ids);
    fz_drop_xml(ctx, doc->xml);
}

static void ofd_drop_document(fz_context* ctx, fz_document* doc_) {
    ofd_package* pkg = (ofd_package*)doc_;
    for (int i = 0; i < pkg->ndocs; ++i) ofd_drop_body(ctx, &pkg->docs[i]);
    fz_free(ctx, pkg->docs);
    fz_drop_xml(ctx, pkg->index);
    fz_drop_archive(ctx, pkg->archive);
}

static void ofd_open_body(fz_context* ctx, ofd_document* doc, fz_xml* body) {
    doc->ids = fz_new_hash_table(ctx, OFD_ID_TABLE_SIZE, sizeof(unsigned int), -1, ofd_drop_id_entry);
    doc->info = ofd_child(body, "DocInfo");
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
    if (!doc->count) fz_throw(ctx, FZ_ERROR_FORMAT, "OFD document contains no pages");
}

/* Open OFD directly so rendering, text extraction and printing share the same display list. */
static fz_document* ofd_open_document(fz_context* ctx, const fz_document_handler* handler, fz_stream* stream,
                                      fz_stream* accel, fz_archive* dir, void* state) {
    ofd_package* pkg = fz_new_derived_document(ctx, ofd_package);
    pkg->super.drop_document = ofd_drop_document;
    pkg->super.count_pages = ofd_count_pages;
    pkg->super.load_page = ofd_load_page;
    pkg->super.lookup_metadata = ofd_metadata;
    pkg->super.load_outline = ofd_load_outline;
    pkg->super.resolve_link_dest = ofd_resolve_link;
    fz_try(ctx) {
        pkg->archive = fz_open_archive_with_stream(ctx, stream);
        pkg->index = fz_parse_xml_archive_entry(ctx, pkg->archive, "OFD.xml", 0);
        fz_xml* root = fz_xml_root(pkg->index);
        int n = 0;
        for (fz_xml* body = fz_xml_down(root); body; body = fz_xml_next(body)) n += ofd_is(body, "DocBody");
        if (!n) fz_throw(ctx, FZ_ERROR_FORMAT, "OFD contains no documents");
        pkg->docs = fz_calloc(ctx, n, sizeof(ofd_document));
        // a document that can't be opened is skipped, unless it is the only one
        for (fz_xml* body = fz_xml_down(root); body; body = fz_xml_next(body)) {
            if (!ofd_is(body, "DocBody")) continue;
            ofd_document* doc = &pkg->docs[pkg->ndocs];
            doc->archive = pkg->archive;
            fz_try(ctx) {
                ofd_open_body(ctx, doc, body);
                ++pkg->ndocs;
            }
            fz_catch(ctx) {
                ofd_drop_body(ctx, doc);
                memset(doc, 0, sizeof(*doc));
                if (n == 1) fz_rethrow(ctx);
                ofd_skip(ctx, "document");
            }
        }
        if (!pkg->ndocs) fz_throw(ctx, FZ_ERROR_FORMAT, "OFD contains no readable documents");
    }
    fz_catch(ctx) {
        fz_drop_document(ctx, &pkg->super);
        fz_rethrow(ctx);
    }
    return &pkg->super;
}

//--- signature verification

static int ofd_is_package(fz_document* doc) {
    return doc && doc->load_page == ofd_load_page;
}

int ofd_count_signatures(fz_context* ctx, fz_document* doc_) {
    if (!ofd_is_package(doc_)) return -1;
    ofd_package* pkg = (ofd_package*)doc_;
    int n = 0;
    for (int i = 0; i < pkg->ndocs; ++i) {
        ofd_document* doc = &pkg->docs[i];
        if (!doc->signatures[0]) continue;
        fz_xml* index = NULL;
        fz_var(index);
        fz_try(ctx) {
            index = ofd_read_xml(ctx, doc, doc->signatures);
            for (fz_xml* node = fz_xml_down(fz_xml_root(index)); node; node = fz_xml_next(node)) {
                n += ofd_is(node, "Signature");
            }
        }
        fz_always(ctx) {
            fz_drop_xml(ctx, index);
        }
        fz_catch(ctx) {
            ofd_skip(ctx, "signatures");
        }
    }
    return n;
}

typedef enum {
    OFD_DIGEST_UNSUPPORTED,
    OFD_DIGEST_SM3,
    OFD_DIGEST_SM3_WITH_KEY,
    OFD_DIGEST_SHA256
} ofd_digest;

// CheckMethod: "SM3", the SM3 OID (1.2.156.10197.1.401[.1]), SM3 with the signer's key (.401.2) or SHA-256
static ofd_digest ofd_digest_method(const char* method) {
    if (!method || !*method) return OFD_DIGEST_SM3;
    if (!fz_strcasecmp(method, "SM3") || !strcmp(method, "1.2.156.10197.1.401") ||
        !strcmp(method, "1.2.156.10197.1.401.1") || fz_strstrcase(method, "#sm3"))
        return OFD_DIGEST_SM3;
    if (!strcmp(method, "1.2.156.10197.1.401.2")) return OFD_DIGEST_SM3_WITH_KEY;
    if (!fz_strcasecmp(method, "SHA256") || !fz_strcasecmp(method, "SHA-256") ||
        !strcmp(method, "2.16.840.1.101.3.4.2.1"))
        return OFD_DIGEST_SHA256;
    return OFD_DIGEST_UNSUPPORTED;
}

// 1 if the member's digest equals the base64 check value
static int ofd_reference_matches(fz_context* ctx, ofd_document* doc, const char* path, ofd_digest method,
                                 const unsigned char* key, const char* check_value) {
    fz_buffer* data = NULL;
    fz_buffer* expected = NULL;
    int ok = 0;
    fz_var(data);
    fz_var(expected);
    fz_try(ctx) {
        if (!fz_has_archive_entry(ctx, doc->archive, path)) break;
        data = fz_read_archive_entry(ctx, doc->archive, path);
        expected = fz_new_buffer_from_base64(ctx, check_value, 0);
        unsigned char* bytes;
        size_t len = fz_buffer_storage(ctx, data, &bytes);
        unsigned char* want;
        size_t want_len = fz_buffer_storage(ctx, expected, &want);
        unsigned char digest[32];
        if (method == OFD_DIGEST_SM3) {
            ofd_sm3(bytes, len, digest);
        } else if (method == OFD_DIGEST_SM3_WITH_KEY) {
            ofd_sm3_with_key(key, bytes, len, digest);
        } else {
            fz_sha256 sha;
            fz_sha256_init(&sha);
            fz_sha256_update(&sha, bytes, len);
            fz_sha256_final(&sha, digest);
        }
        ok = want_len == sizeof(digest) && !memcmp(want, digest, sizeof(digest));
    }
    fz_always(ctx) {
        fz_drop_buffer(ctx, expected);
        fz_drop_buffer(ctx, data);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }
    return ok;
}

// The files a signature protects must still have the digests it recorded.
static void ofd_check_references(fz_context* ctx, ofd_document* doc, fz_xml* refs, const char* sig_path,
                                 ofd_signature_info* info) {
    ofd_digest method = ofd_digest_method(fz_xml_att(refs, "CheckMethod"));
    // the keyed digest needs the key from the signature's certificate
    if (method == OFD_DIGEST_SM3_WITH_KEY && !info->check.has_key) method = OFD_DIGEST_UNSUPPORTED;
    for (fz_xml* ref = fz_xml_down(refs); ref; ref = fz_xml_next(ref)) {
        if (!ofd_is(ref, "Reference")) continue;
        ++info->files_total;
        if (method == OFD_DIGEST_UNSUPPORTED) continue;
        char path[OFD_PATH_SIZE];
        const char* file_ref = fz_xml_att(ref, "FileRef");
        ofd_resolve(ctx, path, sig_path, file_ref);
        if (ofd_reference_matches(ctx, doc, path, method, info->check.key, ofd_text(ofd_child(ref, "CheckValue")))) {
            ++info->files_ok;
        } else if (!info->bad_file[0]) {
            fz_strlcpy(info->bad_file, path, sizeof(info->bad_file));
        }
    }
}

// Where the signature's first stamp is: page across all documents, rect in page points.
static void ofd_stamp_location(fz_context* ctx, ofd_package* pkg, int chapter, fz_xml* signed_info,
                               ofd_signature_info* info) {
    ofd_document* doc = &pkg->docs[chapter];
    fz_xml* stamp = ofd_child(signed_info, "StampAnnot");
    info->page = -1;
    if (!stamp) return;
    int page = ofd_page_by_id(doc, fz_xml_att(stamp, "PageRef"));
    if (page < 0) return;
    page += ofd_first_page(pkg, chapter);
    info->page = page;
    fz_rect box = ofd_box(ofd_text(ofd_child(ofd_child(doc->common, "PageArea"), "PhysicalBox")));
    fz_matrix ctm = fz_concat(fz_translate(-box.x0, -box.y0), fz_scale(ofd_points_per_mm, ofd_points_per_mm));
    info->rect = fz_transform_rect(ofd_box(fz_xml_att(stamp, "Boundary")), ctm);
}

static void ofd_check_signature(fz_context* ctx, ofd_package* pkg, int chapter, fz_xml* entry, const char* index_path,
                                ofd_signature_info* info) {
    ofd_document* doc = &pkg->docs[chapter];
    fz_buffer* tbs = NULL;
    fz_buffer* value = NULL;
    fz_xml* xml = NULL;
    fz_var(tbs);
    fz_var(value);
    fz_var(xml);
    fz_try(ctx) {
        char path[OFD_PATH_SIZE];
        ofd_resolve(ctx, path, index_path, fz_xml_att(entry, "BaseLoc"));
        tbs = fz_read_archive_entry(ctx, doc->archive, path);
        xml = fz_parse_xml(ctx, tbs, 0);
        fz_xml* root = fz_xml_root(xml);
        fz_xml* signed_info = ofd_child(root, "SignedInfo");
        ofd_stamp_location(ctx, pkg, chapter, signed_info, info);

        // first: the references' digests may need the signer's key
        char value_path[OFD_PATH_SIZE];
        ofd_resolve(ctx, value_path, path, ofd_text(ofd_child(root, "SignedValue")));
        value = fz_read_archive_entry(ctx, doc->archive, value_path);
        unsigned char* bytes;
        size_t len = fz_buffer_storage(ctx, tbs, &bytes);
        int is_seal = !fz_xml_att_eq(entry, "Type", "Sign");
        ofd_verify_signed_value(ctx, value, is_seal, bytes, len, &info->check);

        ofd_check_references(ctx, doc, ofd_child(signed_info, "References"), path, info);
    }
    fz_always(ctx) {
        fz_drop_xml(ctx, xml);
        fz_drop_buffer(ctx, value);
        fz_drop_buffer(ctx, tbs);
    }
    fz_catch(ctx) {
        fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
        fz_report_error(ctx);
        if (!info->check.problem) info->check.problem = "the signature couldn't be read";
    }
}

void ofd_drop_signature_infos(fz_context* ctx, ofd_signature_info* info) {
    while (info) {
        ofd_signature_info* next = info->next;
        fz_drop_buffer(ctx, info->check.cert);
        fz_free(ctx, info);
        info = next;
    }
}

// Every signature of every document, in order; NULL when there are none.
ofd_signature_info* ofd_check_signatures(fz_context* ctx, fz_document* doc_) {
    if (!ofd_is_package(doc_)) return NULL;
    ofd_package* pkg = (ofd_package*)doc_;
    ofd_signature_info* head = NULL;
    ofd_signature_info** tail = &head;
    fz_xml* index = NULL;
    fz_var(index);
    fz_try(ctx) {
        for (int i = 0; i < pkg->ndocs; ++i) {
            ofd_document* doc = &pkg->docs[i];
            if (!doc->signatures[0] || !fz_has_archive_entry(ctx, doc->archive, doc->signatures)) continue;
            index = ofd_read_xml(ctx, doc, doc->signatures);
            for (fz_xml* node = fz_xml_down(fz_xml_root(index)); node; node = fz_xml_next(node)) {
                if (!ofd_is(node, "Signature")) continue;
                *tail = fz_malloc_struct(ctx, ofd_signature_info);
                ofd_check_signature(ctx, pkg, i, node, doc->signatures, *tail);
                tail = &(*tail)->next;
            }
            fz_drop_xml(ctx, index);
            index = NULL;
        }
    }
    fz_always(ctx) {
        fz_drop_xml(ctx, index);
    }
    fz_catch(ctx) {
        ofd_drop_signature_infos(ctx, head);
        fz_rethrow(ctx);
    }
    return head;
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

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

#include <stdlib.h>
#include <string.h>

#define PDF_XFA_FIELD_PAD 2.0f
#define PDF_XFA_DEFAULT_DRAW_H 14.0f
#define PDF_XFA_DEFAULT_FIELD_H 18.0f
#define PDF_XFA_MAX_COLUMNS 32
#define PDF_XFA_LINE_LIMIT 64
#define PDF_XFA_DEFAULT_LINE_HEIGHT_MUL 1.2f

typedef struct {
    float ox;
    float oy;
} pdf_xfa_render_pos;

typedef struct {
    int page_index;
    int fields_only;
    pdf_xfa_object* target_page_area;
    fz_font* font_bold;
    fz_font* default_font;
    pdf_xfa_fonts* fonts;
} pdf_xfa_render_ctx;

typedef struct {
    float size_pt;
    int bold;
    float rgb[3];
    int has_color;
    char v_align[16];
    char h_align[16];
    float margin_left;
    float margin_right;
    float space_above;
    float space_below;
    float text_indent;
    float line_height;
    char typeface[128];
    int italic;
} pdf_xfa_text_style;

typedef struct {
    const char* a;
    const char* b;
    int paragraph_end;
} pdf_xfa_text_line;

typedef struct {
    fz_context* ctx;
    fz_font* font;
    float fontsize;
} pdf_xfa_font_info;

typedef struct {
    float widths[PDF_XFA_MAX_COLUMNS];
    int count;
    int current_column;
    float row_x;
    int in_row;
    int in_table;
} pdf_xfa_column_ctx;

static void pdf_xfa_render_fill_rect(fz_context* ctx, fz_device* dev, fz_matrix ctm, fz_rect rect, float rgb[3]) {
    fz_path* path = fz_new_path(ctx);

    fz_try(ctx) {
        fz_moveto(ctx, path, rect.x0, rect.y0);
        fz_lineto(ctx, path, rect.x1, rect.y0);
        fz_lineto(ctx, path, rect.x1, rect.y1);
        fz_lineto(ctx, path, rect.x0, rect.y1);
        fz_closepath(ctx, path);
        fz_fill_path(ctx, dev, path, 0, ctm, fz_device_rgb(ctx), rgb, 1.0f, fz_default_color_params);
    }
    fz_always(ctx) fz_drop_path(ctx, path);
    fz_catch(ctx) fz_rethrow(ctx);
}

static void pdf_xfa_render_stroke_line(fz_context* ctx, fz_device* dev, fz_matrix ctm, float x0, float y0, float x1,
                                       float y1, float rgb[3], float line_width) {
    fz_path* path = NULL;
    fz_stroke_state* stroke = NULL;

    fz_var(path);
    fz_var(stroke);
    fz_try(ctx) {
        path = fz_new_path(ctx);
        stroke = fz_new_stroke_state(ctx);
        stroke->linewidth = line_width;

        fz_moveto(ctx, path, x0, y0);
        fz_lineto(ctx, path, x1, y1);
        fz_stroke_path(ctx, dev, path, stroke, ctm, fz_device_rgb(ctx), rgb, 1.0f, fz_default_color_params);
    }
    fz_always(ctx) {
        fz_drop_stroke_state(ctx, stroke);
        fz_drop_path(ctx, path);
    }
    fz_catch(ctx) fz_rethrow(ctx);
}

static int pdf_xfa_parse_rgb_color(const char* text, float rgb[3]) {
    const char* p;
    int i;

    if (!text || !text[0]) return 0;

    p = text;
    for (i = 0; i < 3; i++) {
        char* end;
        double v = strtod(p, &end);
        if (end == p) return 0;
        rgb[i] = (float)(v / 255.0);
        p = end;
        while (*p == ' ' || *p == '\t') p++;
        if (i < 2) {
            if (*p != ',') return 0;
            p++;
        }
    }
    return 1;
}

static pdf_xfa_object* pdf_xfa_find_descendant(pdf_xfa_object* node, const char* name) {
    pdf_xfa_object* child;
    pdf_xfa_object* hit;

    if (!node || !name) return NULL;

    for (child = node->first_child; child; child = child->next_sibling) {
        if (child->name && strcmp(child->name, name) == 0) return child;
        hit = pdf_xfa_find_descendant(child, name);
        if (hit) return hit;
    }
    return NULL;
}

static int pdf_xfa_object_fill_color(fz_context* ctx, pdf_xfa_object* node, float rgb[3]) {
    pdf_xfa_object* color;

    color = pdf_xfa_find_descendant(node, "color");
    if (!color) return 0;
    if (!pdf_xfa_parse_rgb_color(pdf_xfa_object_get_attr(ctx, color, "value"), rgb)) return 0;
    return 1;
}

static void pdf_xfa_render_stroke_rect(fz_context* ctx, fz_device* dev, fz_matrix ctm, fz_rect rect, float rgb[3],
                                       float line_width) {
    fz_path* path = NULL;
    fz_stroke_state* stroke = NULL;

    fz_var(path);
    fz_var(stroke);
    fz_try(ctx) {
        path = fz_new_path(ctx);
        stroke = fz_new_stroke_state(ctx);
        stroke->linewidth = line_width;

        fz_moveto(ctx, path, rect.x0, rect.y0);
        fz_lineto(ctx, path, rect.x1, rect.y0);
        fz_lineto(ctx, path, rect.x1, rect.y1);
        fz_lineto(ctx, path, rect.x0, rect.y1);
        fz_closepath(ctx, path);
        fz_stroke_path(ctx, dev, path, stroke, ctm, fz_device_rgb(ctx), rgb, 1.0f, fz_default_color_params);
    }
    fz_always(ctx) {
        fz_drop_stroke_state(ctx, stroke);
        fz_drop_path(ctx, path);
    }
    fz_catch(ctx) fz_rethrow(ctx);
}

static void pdf_xfa_render_pos_add_attrs(fz_context* ctx, pdf_xfa_object* node, pdf_xfa_render_pos* pos) {
    char *x, *y;

    x = pdf_xfa_object_get_attr(ctx, node, "x");
    y = pdf_xfa_object_get_attr(ctx, node, "y");
    if (x && x[0]) pos->ox += pdf_xfa_parse_measurement(x, 0);
    if (y && y[0]) pos->oy += pdf_xfa_parse_measurement(y, 0);
}

static int pdf_xfa_node_shifts_children(const char* name) {
    if (!name) return 0;
    return strcmp(name, "subform") == 0 || strcmp(name, "contentArea") == 0 || strcmp(name, "pageArea") == 0 ||
           strcmp(name, "area") == 0;
}

static int pdf_xfa_node_is_field_or_draw(const char* name) {
    if (!name) return 0;
    return strcmp(name, "field") == 0 || strcmp(name, "draw") == 0;
}

static int pdf_xfa_layout_is_row(const char* layout) {
    return layout && (strcmp(layout, "row") == 0 || strcmp(layout, "rl-row") == 0);
}

static int pdf_xfa_layout_is_table(const char* layout) {
    return layout && strcmp(layout, "table") == 0;
}

static int pdf_xfa_layout_is_tb(const char* layout) {
    return layout && strcmp(layout, "tb") == 0;
}

static int pdf_xfa_layout_is_lr_tb(const char* layout) {
    return layout && (strcmp(layout, "lr-tb") == 0 || strcmp(layout, "rl-tb") == 0);
}

static int pdf_xfa_layout_is_position(fz_context* ctx, pdf_xfa_object* subform) {
    char* layout;

    if (!subform || !subform->name || strcmp(subform->name, "subform") != 0) return 0;
    layout = pdf_xfa_object_get_attr(ctx, subform, "layout");
    if (!layout || !layout[0]) return 1;
    return strcmp(layout, "position") == 0;
}

static int pdf_xfa_presence_visible(fz_context* ctx, pdf_xfa_object* node) {
    char* presence;

    if (!node) return 0;
    presence = pdf_xfa_object_get_attr(ctx, node, "presence");
    if (!presence || !presence[0]) return 1;
    if (strcmp(presence, "hidden") == 0 || strcmp(presence, "inactive") == 0) return 0;
    if (strcmp(presence, "invisible") == 0) return 0;
    return 1;
}

static int pdf_xfa_subform_has_layout(fz_context* ctx, pdf_xfa_object* subform, int (*match)(const char*)) {
    char* layout;

    if (!subform || !subform->name || strcmp(subform->name, "subform") != 0) return 0;
    layout = pdf_xfa_object_get_attr(ctx, subform, "layout");
    return match(layout);
}

static float pdf_xfa_node_content_height(fz_context* ctx, pdf_xfa_object* node) {
    char* h_attr;
    float h;

    if (!node || !node->name) return 0;
    h_attr = pdf_xfa_object_get_attr(ctx, node, "h");
    if (strcmp(node->name, "field") == 0)
        h = pdf_xfa_parse_measurement(h_attr, PDF_XFA_DEFAULT_FIELD_H);
    else if (strcmp(node->name, "draw") == 0)
        h = pdf_xfa_parse_measurement(h_attr, PDF_XFA_DEFAULT_DRAW_H);
    else
        return 0;
    return h > 0 ? h : 0;
}

static float pdf_xfa_row_content_height(fz_context* ctx, pdf_xfa_object* row) {
    pdf_xfa_object* child;
    float max_h = 0;
    float h;

    if (!row) return 0;
    for (child = row->first_child; child; child = child->next_sibling) {
        h = pdf_xfa_node_content_height(ctx, child);
        if (h > max_h) max_h = h;
    }
    return max_h;
}

static int pdf_xfa_tb_should_stack_child(fz_context* ctx, pdf_xfa_object* child) {
    if (!child || !child->name) return 0;
    if (pdf_xfa_object_is_prototype_def(ctx, child)) return 0;
    if (strcmp(child->name, "pageSet") == 0) return 0;
    if (child->flow_page >= 0) return 0;
    if (strcmp(child->name, "subform") == 0 && pdf_xfa_page_subform_index(ctx, child) >= 0) return 0;
    return strcmp(child->name, "field") == 0 || strcmp(child->name, "draw") == 0 || strcmp(child->name, "subform") == 0;
}

static float pdf_xfa_tb_stack_advance(fz_context* ctx, pdf_xfa_object* child) {
    char* layout;
    float y;

    if (!pdf_xfa_tb_should_stack_child(ctx, child)) return 0;

    y = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, child, "y"), 0);

    if (strcmp(child->name, "field") == 0 || strcmp(child->name, "draw") == 0)
        return pdf_xfa_node_content_height(ctx, child) + y;

    if (strcmp(child->name, "subform") == 0) {
        pdf_xfa_object* row;
        float h = 0;

        layout = pdf_xfa_object_get_attr(ctx, child, "layout");
        if (pdf_xfa_layout_is_table(layout)) {
            for (row = child->first_child; row; row = row->next_sibling) {
                if (!pdf_xfa_subform_has_layout(ctx, row, pdf_xfa_layout_is_row)) continue;
                h += pdf_xfa_row_content_height(ctx, row) +
                     pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, row, "y"), 0);
            }
            return h + y;
        }
        if (pdf_xfa_layout_is_tb(layout)) {
            for (row = child->first_child; row; row = row->next_sibling) h += pdf_xfa_tb_stack_advance(ctx, row);
            return h + y;
        }
        if (pdf_xfa_layout_is_row(layout)) return pdf_xfa_row_content_height(ctx, child) + y;

        for (row = child->first_child; row; row = row->next_sibling) h += pdf_xfa_tb_stack_advance(ctx, row);
        return h + y;
    }

    return 0;
}

static float pdf_xfa_subform_layout_width(fz_context* ctx, pdf_xfa_object* subform) {
    if (!subform) return 0;
    return pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, subform, "w"), 0);
}

static float pdf_xfa_lr_tb_child_width(fz_context* ctx, pdf_xfa_object* child, pdf_xfa_column_ctx* col) {
    float w;

    if (!pdf_xfa_tb_should_stack_child(ctx, child)) return 0;

    w = pdf_xfa_node_layout_width(ctx, child, col);
    return w > 0 ? w : 0;
}

static float pdf_xfa_lr_tb_child_height(fz_context* ctx, pdf_xfa_object* child) {
    float y;

    if (!child || !child->name) return 0;

    y = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, child, "y"), 0);

    if (strcmp(child->name, "field") == 0 || strcmp(child->name, "draw") == 0)
        return pdf_xfa_node_content_height(ctx, child) + y;

    if (strcmp(child->name, "subform") == 0) {
        if (pdf_xfa_subform_has_layout(ctx, child, pdf_xfa_layout_is_row))
            return pdf_xfa_row_content_height(ctx, child) + y;
        return pdf_xfa_tb_stack_advance(ctx, child) + y;
    }

    return 0;
}

static int pdf_xfa_parent_is_row(fz_context* ctx, pdf_xfa_object* node) {
    pdf_xfa_object* parent = node ? node->parent : NULL;
    char* layout;

    if (!parent || !parent->name || strcmp(parent->name, "subform") != 0) return 0;
    layout = pdf_xfa_object_get_attr(ctx, parent, "layout");
    return pdf_xfa_layout_is_row(layout);
}

static int pdf_xfa_parse_column_widths(fz_context* ctx, pdf_xfa_object* node, float* widths, int max) {
    char* text;
    char* p;
    int n = 0;

    text = pdf_xfa_object_get_attr(ctx, node, "columnWidths");
    if (!text || !text[0]) return 0;

    p = text;
    while (n < max) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        widths[n++] = pdf_xfa_parse_measurement(p, 0);
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    return n;
}

static int pdf_xfa_parse_col_span(fz_context* ctx, pdf_xfa_object* node) {
    char* col_span;
    int span;

    col_span = pdf_xfa_object_get_attr(ctx, node, "colSpan");
    if (!col_span || !col_span[0]) return 1;
    span = atoi(col_span);
    if (span == 0) return 1;
    return span;
}

static float pdf_xfa_column_span_width(pdf_xfa_column_ctx* col, int col_span, int start_col) {
    int i, end;
    float w = 0;

    if (!col || col->count <= 0 || start_col < 0 || start_col >= col->count) return 0;

    if (col_span < 0) {
        for (i = start_col; i < col->count; i++) w += col->widths[i];
        return w;
    }

    end = start_col + col_span;
    if (end > col->count) end = col->count;
    for (i = start_col; i < end; i++) w += col->widths[i];
    return w;
}

static float pdf_xfa_node_layout_width(fz_context* ctx, pdf_xfa_object* node, pdf_xfa_column_ctx* col) {
    char* w;
    float wf;
    int col_span;

    w = pdf_xfa_object_get_attr(ctx, node, "w");
    wf = pdf_xfa_parse_measurement(w, 0);
    if (wf > 0) return wf;
    if (!col || col->count <= 0) return 0;

    col_span = pdf_xfa_parse_col_span(ctx, node);
    return pdf_xfa_column_span_width(col, col_span, col->current_column);
}

static void pdf_xfa_row_advance(fz_context* ctx, pdf_xfa_object* node, pdf_xfa_column_ctx* col, float width) {
    int col_span;

    if (!col || width <= 0) return;

    col->row_x += width;
    col_span = pdf_xfa_parse_col_span(ctx, node);
    if (col_span < 0)
        col->current_column = 0;
    else if (col->count > 0)
        col->current_column = (col->current_column + col_span) % col->count;
}

static pdf_xfa_object* pdf_xfa_find_content_area(pdf_xfa_object* page_area) {
    pdf_xfa_object* child;

    for (child = page_area ? page_area->first_child : NULL; child; child = child->next_sibling) {
        if (child->name && strcmp(child->name, "contentArea") == 0) return child;
    }
    return NULL;
}

static pdf_xfa_object* pdf_xfa_find_pagearea_ancestor(pdf_xfa_object* node) {
    while (node) {
        if (node->name && strcmp(node->name, "pageArea") == 0) return node;
        node = node->parent;
    }
    return NULL;
}

static pdf_xfa_object* pdf_xfa_find_page_subform_ancestor(fz_context* ctx, pdf_xfa_object* node) {
    while (node) {
        if (node->name && strcmp(node->name, "subform") == 0 && pdf_xfa_page_subform_index(ctx, node) >= 0) return node;
        node = node->parent;
    }
    return NULL;
}

static int pdf_xfa_page_subform_is_target(fz_context* ctx, pdf_xfa* xfa, pdf_xfa_object* subform, int page_index) {
    int idx;

    if (!subform || !xfa->page_subforms || page_index < 0 || page_index >= xfa->page_subform_count) return 0;
    if (subform == xfa->page_subforms[page_index]) return 1;
    idx = pdf_xfa_page_subform_index(ctx, subform);
    return idx >= 0 && idx == page_index;
}

static int pdf_xfa_pagearea_is_target(fz_context* ctx, pdf_xfa_object* node, pdf_xfa_object* target) {
    char* node_name;
    char* target_name;

    if (!node || !target) return 0;
    if (node == target) return 1;
    node_name = pdf_xfa_object_get_attr(ctx, node, "name");
    target_name = pdf_xfa_object_get_attr(ctx, target, "name");
    if (node_name && target_name && node_name[0] && target_name[0] && strcmp(node_name, target_name) == 0) return 1;
    return 0;
}

static fz_rect pdf_xfa_object_rect(fz_context* ctx, pdf_xfa_object* node, float page_h, pdf_xfa_render_pos* pos,
                                   float default_w, float default_h, int ignore_node_xy) {
    char *x, *y, *w, *h;
    float xf, yf, wf, hf;
    fz_rect rect = fz_empty_rect;

    x = pdf_xfa_object_get_attr(ctx, node, "x");
    y = pdf_xfa_object_get_attr(ctx, node, "y");
    w = pdf_xfa_object_get_attr(ctx, node, "w");
    h = pdf_xfa_object_get_attr(ctx, node, "h");

    xf = pos->ox + (ignore_node_xy ? 0 : pdf_xfa_parse_measurement(x, 0));
    yf = pos->oy + (ignore_node_xy ? 0 : pdf_xfa_parse_measurement(y, 0));
    wf = pdf_xfa_parse_measurement(w, default_w);
    hf = pdf_xfa_parse_measurement(h, default_h);
    if (wf <= 0 && default_w > 0) wf = default_w;
    if (hf <= 0 && default_h > 0) hf = default_h;

    if (wf <= 0 || hf <= 0) return rect;

    rect.x0 = xf;
    rect.x1 = xf + wf;
    rect.y1 = page_h - yf;
    rect.y0 = rect.y1 - hf;
    return rect;
}

static void pdf_xfa_add_ui_textedit_margins(fz_context* ctx, pdf_xfa_object* node, pdf_xfa_text_style* style) {
    pdf_xfa_object* ui;
    pdf_xfa_object* text_edit;
    pdf_xfa_object* margin;

    if (!style) return;

    ui = pdf_xfa_object_find_child(node, "ui");
    if (!ui) return;
    text_edit = pdf_xfa_object_find_child(ui, "textEdit");
    if (!text_edit) return;
    margin = pdf_xfa_object_find_child(text_edit, "margin");
    if (!margin) return;

    style->margin_left += pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, margin, "leftInset"), 0);
    style->margin_right += pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, margin, "rightInset"), 0);
    style->space_above += pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, margin, "topInset"), 0);
    style->space_below += pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, margin, "bottomInset"), 0);
}

static void pdf_xfa_strip_typeface_attr(const char* src, char* dst, size_t dstsz) {
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

static void pdf_xfa_parse_text_style(fz_context* ctx, pdf_xfa_object* node, pdf_xfa_text_style* style) {
    pdf_xfa_object* font;
    pdf_xfa_object* fill;
    pdf_xfa_object* color;
    pdf_xfa_object* para;
    char* size;
    char* weight;
    char* posture;
    char* typeface;
    char* align;

    if (!style) return;

    memset(style, 0, sizeof(*style));
    style->rgb[0] = style->rgb[1] = style->rgb[2] = 0.0f;

    font = pdf_xfa_find_descendant(node, "font");
    if (font) {
        size = pdf_xfa_object_get_attr(ctx, font, "size");
        style->size_pt = pdf_xfa_parse_measurement(size, 0);
        weight = pdf_xfa_object_get_attr(ctx, font, "weight");
        if (weight && strcmp(weight, "bold") == 0) style->bold = 1;
        posture = pdf_xfa_object_get_attr(ctx, font, "posture");
        if (posture && strcmp(posture, "italic") == 0) style->italic = 1;
        typeface = pdf_xfa_object_get_attr(ctx, font, "typeface");
        if (typeface && typeface[0]) pdf_xfa_strip_typeface_attr(typeface, style->typeface, sizeof(style->typeface));
        fill = pdf_xfa_object_find_child(font, "fill");
        if (!fill) fill = pdf_xfa_find_descendant(font, "fill");
        if (fill) {
            color = pdf_xfa_object_find_child(fill, "color");
            if (!color) color = pdf_xfa_find_descendant(fill, "color");
            if (color && pdf_xfa_parse_rgb_color(pdf_xfa_object_get_attr(ctx, color, "value"), style->rgb))
                style->has_color = 1;
        }
    }

    if (!style->has_color) {
        fill = pdf_xfa_object_find_child(node, "fill");
        if (!fill) fill = pdf_xfa_find_descendant(node, "fill");
        if (fill) {
            color = pdf_xfa_object_find_child(fill, "color");
            if (!color) color = pdf_xfa_find_descendant(fill, "color");
            if (color && pdf_xfa_parse_rgb_color(pdf_xfa_object_get_attr(ctx, color, "value"), style->rgb))
                style->has_color = 1;
        }
    }

    para = pdf_xfa_object_find_child(node, "para");
    if (!para) para = pdf_xfa_find_descendant(node, "para");
    if (para) {
        align = pdf_xfa_object_get_attr(ctx, para, "vAlign");
        if (align && align[0]) {
            strncpy(style->v_align, align, sizeof(style->v_align) - 1);
            style->v_align[sizeof(style->v_align) - 1] = 0;
        }
        align = pdf_xfa_object_get_attr(ctx, para, "hAlign");
        if (align && align[0]) {
            strncpy(style->h_align, align, sizeof(style->h_align) - 1);
            style->h_align[sizeof(style->h_align) - 1] = 0;
        }
        style->margin_left = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, para, "marginLeft"), 0);
        style->margin_right = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, para, "marginRight"), 0);
        style->space_above = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, para, "spaceAbove"), 0);
        style->space_below = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, para, "spaceBelow"), 0);
        style->text_indent = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, para, "textIndent"), 0);
        if (style->text_indent < 0) {
            style->margin_left -= style->text_indent;
            style->text_indent = 0;
        }
        style->line_height = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, para, "lineHeight"), 0);
    }

    pdf_xfa_add_ui_textedit_margins(ctx, node, style);
}

static fz_font* pdf_xfa_font_for_style(fz_context* ctx, pdf_xfa_render_ctx* rctx, pdf_xfa_text_style* style) {
    fz_font* font;
    const char* typeface = NULL;
    int bold = 0;
    int italic = 0;

    if (style) {
        bold = style->bold;
        italic = style->italic;
        if (style->typeface[0])
            typeface = style->typeface;
        else if (rctx && rctx->fonts)
            typeface = pdf_xfa_fonts_default_typeface(rctx->fonts);
    }

    if (rctx && rctx->fonts && typeface && typeface[0]) {
        font = pdf_xfa_fonts_resolve(ctx, rctx->fonts, typeface, bold, italic);
        if (font) return font;
    }

    if (style && style->bold && rctx && rctx->font_bold) return rctx->font_bold;
    if (rctx && rctx->default_font) return rctx->default_font;
    return NULL;
}

static const char* pdf_xfa_node_text(fz_context* ctx, pdf_xfa_object* node) {
    pdf_xfa_object* value;

    if (node->content && node->content[0]) return node->content;

    value = pdf_xfa_object_find_child(node, "value");
    if (value) {
        const char* text = pdf_xfa_object_text(ctx, value);
        if (text && text[0]) return text;
    }

    return NULL;
}

static float pdf_xfa_measure_text_width(fz_context* ctx, fz_font* font, float fontsize, const char* text) {
    fz_matrix trm;
    fz_matrix adv;

    trm = fz_scale(fontsize, -fontsize);
    adv = fz_measure_string(ctx, font, trm, text, 0, 0, FZ_BIDI_LTR, FZ_LANG_UNSET);
    return adv.e - trm.e;
}

static float pdf_xfa_measure_character(pdf_xfa_font_info* info, int c) {
    fz_font* font;
    int gid = fz_encode_character_with_fallback(info->ctx, info->font, c, 0, 0, &font);
    return fz_advance_glyph(info->ctx, font, gid, 0) * info->fontsize;
}

static float pdf_xfa_measure_text_substring(fz_context* ctx, fz_font* font, float fontsize, const char* text, int len) {
    pdf_xfa_font_info info;
    const char* end;
    float x = 0;
    int c;

    if (!text || len <= 0) return 0;

    info.ctx = ctx;
    info.font = font;
    info.fontsize = fontsize;

    end = text + len;
    while (text < end) {
        text += fz_chartorune(&c, text);
        if (c == '\r' || c == '\n') continue;
        x += pdf_xfa_measure_character(&info, c);
    }
    return x;
}

static void pdf_xfa_set_text_line(pdf_xfa_text_line* line, const char* a, const char* b, int paragraph_end) {
    if (!line) return;
    line->a = a;
    line->b = b;
    line->paragraph_end = paragraph_end;
}

static int pdf_xfa_break_text_lines(pdf_xfa_font_info* info, const char* text, pdf_xfa_text_line* lines, int maxlines,
                                    float width) {
    const char* next;
    const char* space = NULL;
    const char* a = text;
    const char* b = text;
    int c, n = 0;
    float x = 0, w = 0;

    if (!text) return 0;

    while (*b) {
        next = b + fz_chartorune(&c, b);
        if (c == '\r' || c == '\n') {
            if (lines && n < maxlines) pdf_xfa_set_text_line(&lines[n], a, b, 1);
            ++n;
            if (c == '\r' && *next == '\n') next++;
            a = next;
            x = 0;
            space = NULL;
        } else {
            if (c == ' ') space = b;

            w = pdf_xfa_measure_character(info, c);
            if (width > 0 && x + w > width) {
                if (space) {
                    if (lines && n < maxlines) pdf_xfa_set_text_line(&lines[n], a, space, 0);
                    ++n;
                    a = next = space + 1;
                    x = 0;
                    space = NULL;
                } else {
                    if (lines && n < maxlines) pdf_xfa_set_text_line(&lines[n], a, b, 0);
                    ++n;
                    a = b;
                    x = w;
                    space = NULL;
                }
            } else
                x += w;
        }
        b = next;
    }

    if (lines && n < maxlines) pdf_xfa_set_text_line(&lines[n], a, b, 1);
    ++n;
    return n < maxlines ? n : maxlines;
}

static int pdf_xfa_trim_line_end(const char* text, int len) {
    while (len > 0) {
        const char* p = text;
        const char* end = text + len;
        const char* last_start = text;
        int last_rune = 0;
        int rune_len;

        while (p < end) {
            last_start = p;
            rune_len = fz_chartorune(&last_rune, p);
            p += rune_len;
        }
        if (last_rune != ' ' && last_rune != '\t') break;
        len = (int)(last_start - text);
    }
    return len;
}

static int pdf_xfa_count_interior_spaces(const char* text, int len) {
    const char* end = text + len;
    const char* p = text;
    int count = 0;

    while (p < end) {
        int c;
        const char* next = p + fz_chartorune(&c, p);
        if (c == ' ') {
            const char* q = next;
            int has_more = 0;
            while (q < end) {
                int c2;
                q += fz_chartorune(&c2, q);
                if (c2 != ' ' && c2 != '\t' && c2 != '\r' && c2 != '\n') {
                    has_more = 1;
                    break;
                }
            }
            if (has_more) count++;
        }
        p = next;
    }
    return count;
}

static int pdf_xfa_is_interior_space(const char* text, int len, const char* space) {
    const char* end = text + len;
    const char* q = space + 1;
    while (q < end) {
        int c;
        q += fz_chartorune(&c, q);
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') return 1;
    }
    return 0;
}

static fz_matrix pdf_xfa_show_glyph_at(fz_context* ctx, fz_text* fztext, fz_font* font, fz_matrix trm, int ucs) {
    fz_font* glyph_font;
    int gid;
    float adv;

    gid = fz_encode_character_with_fallback(ctx, font, ucs, 0, FZ_LANG_UNSET, &glyph_font);
    fz_show_glyph(ctx, fztext, glyph_font, trm, gid, ucs, 0, 0, FZ_BIDI_LTR, FZ_LANG_UNSET);
    adv = fz_advance_glyph(ctx, glyph_font, gid, 0);
    return fz_pre_translate(trm, adv, 0);
}

static fz_matrix pdf_xfa_show_substring(fz_context* ctx, fz_text* fztext, fz_font* font, fz_matrix trm, const char* s,
                                        int len) {
    int ucs;
    int i = 0;

    while (i < len) {
        i += fz_chartorune(&ucs, s + i);
        if (ucs == '\r' || ucs == '\n') continue;
        trm = pdf_xfa_show_glyph_at(ctx, fztext, font, trm, ucs);
    }

    return trm;
}

static fz_matrix pdf_xfa_show_substring_justified(fz_context* ctx, fz_text* fztext, fz_font* font, fz_matrix trm,
                                                  const char* s, int len, float extra_space) {
    const char* end = s + len;
    const char* p = s;
    int ucs;

    while (p < end) {
        const char* next = p + fz_chartorune(&ucs, p);
        if (ucs == '\r' || ucs == '\n') {
            p = next;
            continue;
        }
        if (ucs == ' ' && pdf_xfa_is_interior_space(s, len, p)) {
            trm = pdf_xfa_show_glyph_at(ctx, fztext, font, trm, ucs);
            trm = fz_pre_translate(trm, extra_space, 0);
        } else
            trm = pdf_xfa_show_glyph_at(ctx, fztext, font, trm, ucs);
        p = next;
    }

    return trm;
}

static int pdf_xfa_h_align_mode(pdf_xfa_text_style* style) {
    if (!style || !style->h_align[0]) return 0;
    if (strcmp(style->h_align, "center") == 0) return 1;
    if (strcmp(style->h_align, "right") == 0) return 2;
    if (strcmp(style->h_align, "justify") == 0) return 3;
    if (strcmp(style->h_align, "justifyAll") == 0) return 4;
    return 0;
}

static void pdf_xfa_font_vertical_metrics(fz_context* ctx, fz_font* font, float fontsize, float* asc_pt,
                                          float* desc_pt) {
    float asc = 0.8f;
    float desc = -0.2f;

    if (font) {
        fz_calculate_font_ascender_descender(ctx, font);
        asc = fz_font_ascender(ctx, font);
        desc = fz_font_descender(ctx, font);
        if (asc <= 0) asc = 0.8f;
        if (desc >= 0) desc = -0.2f;
    }
    *asc_pt = fontsize * asc;
    *desc_pt = fontsize * desc;
}

static float pdf_xfa_line_start_y(fz_context* ctx, fz_font* font, fz_rect rect, float fontsize, float line_height,
                                  int line_count, pdf_xfa_text_style* style) {
    float asc_pt, desc_pt;
    float block_h;

    pdf_xfa_font_vertical_metrics(ctx, font, fontsize, &asc_pt, &desc_pt);

    if (style && style->v_align[0]) {
        if (strcmp(style->v_align, "bottom") == 0) return rect.y0 - desc_pt + (line_count - 1) * line_height;
        if (strcmp(style->v_align, "middle") == 0) {
            block_h = (line_count - 1) * line_height + asc_pt - desc_pt;
            return (rect.y0 + rect.y1) * 0.5f + block_h * 0.5f - asc_pt;
        }
    }
    return rect.y1 - asc_pt;
}

static float pdf_xfa_line_start_x(fz_rect rect, float line_width, int line_index, pdf_xfa_text_style* style) {
    float x0 = rect.x0;

    if (line_index == 0 && style && style->text_indent > 0) x0 += style->text_indent;

    if (style && style->h_align[0]) {
        if (strcmp(style->h_align, "center") == 0) return x0 + (rect.x1 - x0 - line_width) * 0.5f;
        if (strcmp(style->h_align, "right") == 0) return rect.x1 - line_width;
    }
    return x0;
}

static void pdf_xfa_render_text_in_rect(fz_context* ctx, fz_device* dev, fz_matrix ctm, fz_font* font, const char* text,
                                        fz_rect rect, float pad, pdf_xfa_text_style* style) {
    fz_text* fztext = NULL;
    pdf_xfa_text_line lines[PDF_XFA_LINE_LIMIT];
    pdf_xfa_font_info info;
    float fontsize;
    float line_height;
    float rgb[3];
    float text_width;
    float max_width;
    fz_matrix trm;
    int line_count;
    int l;

    if (!text || !text[0] || fz_is_empty_rect(rect)) return;

    rect.x0 += pad;
    rect.x1 -= pad;
    rect.y0 += pad;
    rect.y1 -= pad;
    if (style) {
        rect.x0 += style->margin_left;
        rect.x1 -= style->margin_right;
        rect.y0 += style->space_below;
        rect.y1 -= style->space_above;
    }
    if (rect.x1 <= rect.x0 || rect.y1 <= rect.y0) return;

    if (style && style->size_pt > 0)
        fontsize = style->size_pt;
    else {
        fontsize = rect.y1 - rect.y0;
        if (fontsize > 12.0f) fontsize = 12.0f;
        if (fontsize < 6.0f) fontsize = 6.0f;
    }

    if (style && style->line_height > 0)
        line_height = style->line_height;
    else
        line_height = fontsize * PDF_XFA_DEFAULT_LINE_HEIGHT_MUL;

    if (style && style->has_color) {
        rgb[0] = style->rgb[0];
        rgb[1] = style->rgb[1];
        rgb[2] = style->rgb[2];
    } else {
        rgb[0] = rgb[1] = rgb[2] = 0.0f;
    }

    info.ctx = ctx;
    info.font = font;
    info.fontsize = fontsize;
    max_width = rect.x1 - rect.x0;
    line_count = pdf_xfa_break_text_lines(&info, text, lines, PDF_XFA_LINE_LIMIT, max_width);
    if (line_count <= 0) return;

    fz_var(fztext);
    fz_try(ctx) {
        fztext = fz_new_text(ctx);
        trm = fz_scale(fontsize, -fontsize);
        trm.f = pdf_xfa_line_start_y(ctx, font, rect, fontsize, line_height, line_count, style);

        for (l = 0; l < line_count; l++) {
            int len = (int)(lines[l].b - lines[l].a);
            int h_align = pdf_xfa_h_align_mode(style);
            int justify_line = 0;
            float line_x0;
            float avail_width;
            int space_count;
            int draw_len;

            if (len <= 0) {
                if (l + 1 < line_count) trm.f -= line_height;
                continue;
            }

            draw_len = pdf_xfa_trim_line_end(lines[l].a, len);
            if (draw_len <= 0) {
                if (l + 1 < line_count) trm.f -= line_height;
                continue;
            }

            line_x0 = rect.x0;
            if (l == 0 && style && style->text_indent > 0) line_x0 += style->text_indent;
            avail_width = rect.x1 - line_x0;

            if (h_align == 3 || h_align == 4) {
                if (h_align == 4 || !lines[l].paragraph_end) justify_line = 1;
            }

            if (justify_line) {
                space_count = pdf_xfa_count_interior_spaces(lines[l].a, draw_len);
                text_width = pdf_xfa_measure_text_substring(ctx, font, fontsize, lines[l].a, draw_len);
                if (space_count > 0 && text_width < avail_width) {
                    float extra_space = (avail_width - text_width) / space_count;
                    trm.e = line_x0;
                    pdf_xfa_show_substring_justified(ctx, fztext, font, trm, lines[l].a, draw_len, extra_space);
                } else {
                    trm.e = line_x0;
                    pdf_xfa_show_substring(ctx, fztext, font, trm, lines[l].a, draw_len);
                }
            } else {
                text_width = pdf_xfa_measure_text_substring(ctx, font, fontsize, lines[l].a, draw_len);
                trm.e = pdf_xfa_line_start_x(rect, text_width, l, style);
                pdf_xfa_show_substring(ctx, fztext, font, trm, lines[l].a, draw_len);
            }

            if (l + 1 < line_count) trm.f -= line_height;
        }

        fz_fill_text(ctx, dev, fztext, ctm, fz_device_rgb(ctx), rgb, 1.0f, fz_default_color_params);
    }
    fz_always(ctx) fz_drop_text(ctx, fztext);
    fz_catch(ctx) fz_rethrow(ctx);
}

static pdf_xfa_object* pdf_xfa_find_ui_widget(fz_context* ctx, pdf_xfa_object* field, const char* widget_name) {
    pdf_xfa_object* ui;
    pdf_xfa_object* child;

    ui = pdf_xfa_object_find_child(field, "ui");
    if (!ui) return NULL;
    for (child = ui->first_child; child; child = child->next_sibling) {
        if (child->name && strcmp(child->name, widget_name) == 0) return child;
    }
    return NULL;
}

static pdf_xfa_object* pdf_xfa_find_check_button(fz_context* ctx, pdf_xfa_object* field) {
    return pdf_xfa_find_ui_widget(ctx, field, "checkButton");
}

static pdf_xfa_object* pdf_xfa_find_radio_button(fz_context* ctx, pdf_xfa_object* field) {
    return pdf_xfa_find_ui_widget(ctx, field, "radioButton");
}

static int pdf_xfa_value_is_checked(const char* text) {
    if (!text || !text[0]) return 0;
    if (strcmp(text, "1") == 0) return 1;
    if (strcmp(text, "true") == 0 || strcmp(text, "on") == 0 || strcmp(text, "yes") == 0) return 1;
    return 0;
}

static fz_rect pdf_xfa_checkbox_rect(fz_rect cell, float size) {
    float w, h, box, cx, cy;

    w = cell.x1 - cell.x0;
    h = cell.y1 - cell.y0;
    box = size;
    if (box > w) box = w;
    if (box > h) box = h;
    cx = (cell.x0 + cell.x1) * 0.5f;
    cy = (cell.y0 + cell.y1) * 0.5f;
    return fz_make_rect(cx - box * 0.5f, cy - box * 0.5f, cx + box * 0.5f, cy + box * 0.5f);
}

#define PDF_XFA_CIRCLE_K 0.552284f

static void pdf_xfa_path_circle(fz_context* ctx, fz_path* path, float cx, float cy, float r) {
    float k = r * PDF_XFA_CIRCLE_K;

    fz_moveto(ctx, path, cx + r, cy);
    fz_curveto(ctx, path, cx + r, cy + k, cx + k, cy + r, cx, cy + r);
    fz_curveto(ctx, path, cx - k, cy + r, cx - r, cy + k, cx - r, cy);
    fz_curveto(ctx, path, cx - r, cy - k, cx - k, cy - r, cx, cy - r);
    fz_curveto(ctx, path, cx + k, cy - r, cx + r, cy - k, cx + r, cy);
    fz_closepath(ctx, path);
}

static void pdf_xfa_render_circle(fz_context* ctx, fz_device* dev, fz_matrix ctm, float cx, float cy, float r,
                                  float fill_rgb[3], float stroke_rgb[3], float line_width, int fill) {
    fz_path* path = NULL;

    fz_var(path);
    fz_try(ctx) {
        path = fz_new_path(ctx);
        pdf_xfa_path_circle(ctx, path, cx, cy, r);
        if (fill) fz_fill_path(ctx, dev, path, 0, ctm, fz_device_rgb(ctx), fill_rgb, 1.0f, fz_default_color_params);
        if (stroke_rgb && line_width > 0) {
            fz_stroke_state* stroke = fz_new_stroke_state(ctx);
            stroke->linewidth = line_width;
            fz_stroke_path(ctx, dev, path, stroke, ctm, fz_device_rgb(ctx), stroke_rgb, 1.0f, fz_default_color_params);
            fz_drop_stroke_state(ctx, stroke);
        }
    }
    fz_always(ctx) fz_drop_path(ctx, path);
    fz_catch(ctx) fz_rethrow(ctx);
}

static void pdf_xfa_render_check_mark(fz_context* ctx, fz_device* dev, fz_matrix ctm, fz_rect box, float rgb[3]) {
    float s, x1, y1, x2, y2, x3, y3;

    s = box.x1 - box.x0;
    if (s <= 0) return;

    x1 = box.x0 + s * 0.22f;
    y1 = box.y0 + s * 0.45f;
    x2 = box.x0 + s * 0.42f;
    y2 = box.y0 + s * 0.28f;
    x3 = box.x1 - s * 0.18f;
    y3 = box.y1 - s * 0.22f;

    pdf_xfa_render_stroke_line(ctx, dev, ctm, x1, y1, x2, y2, rgb, 1.2f);
    pdf_xfa_render_stroke_line(ctx, dev, ctm, x2, y2, x3, y3, rgb, 1.2f);
}

static int pdf_xfa_render_border(fz_context* ctx, fz_device* dev, fz_matrix ctm, pdf_xfa* xfa, pdf_xfa_object* node,
                                 fz_rect rect) {
    pdf_xfa_object* border;
    float fill[3] = {1.0f, 1.0f, 1.0f};
    float stroke[3] = {0.5f, 0.5f, 0.5f};

    border = pdf_xfa_object_find_child(node, "border");
    if (!border) return 0;

    pdf_xfa_object_fill_color(ctx, border, fill);
    pdf_xfa_render_fill_rect(ctx, dev, ctm, rect, fill);

    if (pdf_xfa_find_descendant(border, "edge")) pdf_xfa_render_stroke_rect(ctx, dev, ctm, rect, stroke, 1.0f);

    xfa->render_borders++;
    return 1;
}

static void pdf_xfa_render_radio_button(fz_context* ctx, fz_device* dev, fz_matrix ctm, pdf_xfa* xfa,
                                        pdf_xfa_object* field, pdf_xfa_object* radio_btn, fz_rect rect,
                                        const char* text) {
    fz_rect box;
    float white[3] = {1.0f, 1.0f, 1.0f};
    float black[3] = {0.0f, 0.0f, 0.0f};
    float size, cx, cy, r;

    size = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, radio_btn, "size"), 10.0f);
    box = pdf_xfa_checkbox_rect(rect, size);
    if (!pdf_xfa_render_border(ctx, dev, ctm, xfa, radio_btn, box)) {
        if (!pdf_xfa_render_border(ctx, dev, ctm, xfa, field, box)) {
            cx = (box.x0 + box.x1) * 0.5f;
            cy = (box.y0 + box.y1) * 0.5f;
            r = (box.x1 - box.x0) * 0.5f;
            pdf_xfa_render_circle(ctx, dev, ctm, cx, cy, r, white, black, 1.0f, 1);
        }
    }
    if (pdf_xfa_value_is_checked(text)) {
        cx = (box.x0 + box.x1) * 0.5f;
        cy = (box.y0 + box.y1) * 0.5f;
        r = (box.x1 - box.x0) * 0.2f;
        pdf_xfa_render_circle(ctx, dev, ctm, cx, cy, r, black, NULL, 0, 1);
    }
}

static void pdf_xfa_render_field(fz_context* ctx, fz_device* dev, fz_matrix ctm, pdf_xfa* xfa, pdf_xfa_object* field,
                                 float page_h, fz_font* font, pdf_xfa_render_pos* pos, int ignore_node_xy,
                                 float layout_w, pdf_xfa_render_ctx* rctx) {
    fz_rect rect, box;
    float white[3] = {1.0f, 1.0f, 1.0f};
    float gray[3] = {0.75f, 0.75f, 0.75f};
    float black[3] = {0.0f, 0.0f, 0.0f};
    const char* text;
    pdf_xfa_object* check_btn;
    pdf_xfa_object* radio_btn;
    pdf_xfa_text_style style;
    fz_font* text_font;
    float check_size;

    rect = pdf_xfa_object_rect(ctx, field, page_h, pos, layout_w, PDF_XFA_DEFAULT_FIELD_H, ignore_node_xy);
    if (fz_is_empty_rect(rect)) return;

    xfa->render_fields++;
    text = pdf_xfa_node_text(ctx, field);

    check_btn = pdf_xfa_find_check_button(ctx, field);
    if (check_btn) {
        check_size = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, check_btn, "size"), 10.0f);
        box = pdf_xfa_checkbox_rect(rect, check_size);
        if (!pdf_xfa_render_border(ctx, dev, ctm, xfa, check_btn, box)) {
            if (!pdf_xfa_render_border(ctx, dev, ctm, xfa, field, box)) {
                pdf_xfa_render_fill_rect(ctx, dev, ctm, box, white);
                pdf_xfa_render_stroke_rect(ctx, dev, ctm, box, black, 1.0f);
            }
        }
        if (pdf_xfa_value_is_checked(text)) pdf_xfa_render_check_mark(ctx, dev, ctm, box, black);
        return;
    }

    radio_btn = pdf_xfa_find_radio_button(ctx, field);
    if (radio_btn) {
        pdf_xfa_render_radio_button(ctx, dev, ctm, xfa, field, radio_btn, rect, text);
        return;
    }

    if (!pdf_xfa_render_border(ctx, dev, ctm, xfa, field, rect)) {
        pdf_xfa_render_fill_rect(ctx, dev, ctm, rect, white);
        pdf_xfa_render_stroke_rect(ctx, dev, ctm, rect, gray, 1.0f);
    }

    pdf_xfa_parse_text_style(ctx, field, &style);
    text_font = pdf_xfa_font_for_style(ctx, rctx, &style);
    if (!text_font) text_font = font;
    pdf_xfa_render_text_in_rect(ctx, dev, ctm, text_font, text, rect, PDF_XFA_FIELD_PAD, &style);
}

static int pdf_xfa_render_draw_line(fz_context* ctx, fz_device* dev, fz_matrix ctm, pdf_xfa* xfa, pdf_xfa_object* draw,
                                    float page_h, pdf_xfa_render_pos* pos, int ignore_node_xy) {
    pdf_xfa_object* line;
    char *x1, *y1, *x2, *y2, *dx, *dy;
    float ox, oy, lx0, ly0, lx1, ly1, px0, py0, px1, py1;
    float black[3] = {0.0f, 0.0f, 0.0f};

    line = pdf_xfa_object_find_child(draw, "line");
    if (!line) return 0;

    dx = pdf_xfa_object_get_attr(ctx, draw, "x");
    dy = pdf_xfa_object_get_attr(ctx, draw, "y");
    ox = pos->ox + (ignore_node_xy ? 0 : pdf_xfa_parse_measurement(dx, 0));
    oy = pos->oy + (ignore_node_xy ? 0 : pdf_xfa_parse_measurement(dy, 0));

    x1 = pdf_xfa_object_get_attr(ctx, line, "x1");
    y1 = pdf_xfa_object_get_attr(ctx, line, "y1");
    x2 = pdf_xfa_object_get_attr(ctx, line, "x2");
    y2 = pdf_xfa_object_get_attr(ctx, line, "y2");

    lx0 = ox + pdf_xfa_parse_measurement(x1, 0);
    ly0 = oy + pdf_xfa_parse_measurement(y1, 0);
    lx1 = ox + pdf_xfa_parse_measurement(x2, 0);
    ly1 = oy + pdf_xfa_parse_measurement(y2, 0);

    px0 = lx0;
    px1 = lx1;
    py0 = page_h - ly0;
    py1 = page_h - ly1;

    pdf_xfa_render_stroke_line(ctx, dev, ctm, px0, py0, px1, py1, black, 1.0f);
    xfa->render_lines++;
    return 1;
}

static void pdf_xfa_render_draw(fz_context* ctx, fz_device* dev, fz_matrix ctm, pdf_xfa* xfa, pdf_xfa_object* draw,
                                float page_h, fz_font* font, pdf_xfa_render_pos* pos, int ignore_node_xy,
                                float layout_w, pdf_xfa_render_ctx* rctx) {
    fz_rect rect;
    float black[3] = {0.0f, 0.0f, 0.0f};
    const char* text;
    pdf_xfa_text_style style;
    fz_font* text_font;
    int drew_line;

    drew_line = pdf_xfa_render_draw_line(ctx, dev, ctm, xfa, draw, page_h, pos, ignore_node_xy);
    if (drew_line) {
        xfa->render_draws++;
        return;
    }

    rect = pdf_xfa_object_rect(ctx, draw, page_h, pos, layout_w, PDF_XFA_DEFAULT_DRAW_H, ignore_node_xy);
    if (fz_is_empty_rect(rect)) return;

    xfa->render_draws++;

    pdf_xfa_render_border(ctx, dev, ctm, xfa, draw, rect);

    text = pdf_xfa_node_text(ctx, draw);
    pdf_xfa_parse_text_style(ctx, draw, &style);
    text_font = pdf_xfa_font_for_style(ctx, rctx, &style);
    if (!text_font) text_font = font;
    pdf_xfa_render_text_in_rect(ctx, dev, ctm, text_font, text, rect, 0, &style);
}

static void pdf_xfa_render_tree(fz_context* ctx, fz_device* dev, fz_matrix ctm, pdf_xfa* xfa, pdf_xfa_object* node,
                                float page_h, fz_font* font, pdf_xfa_render_pos* pos, pdf_xfa_render_ctx* rctx,
                                int under_pageset, pdf_xfa_column_ctx* col_ctx) {
    pdf_xfa_object* child;
    pdf_xfa_render_pos child_pos;
    pdf_xfa_column_ctx subform_col_ctx;
    pdf_xfa_column_ctx* active_col_ctx = col_ctx;
    int render_node = 1;

    if (!node) return;

    if (!pdf_xfa_presence_visible(ctx, node)) return;

    if (node->name && strcmp(node->name, "subform") == 0) {
        char* layout = pdf_xfa_object_get_attr(ctx, node, "layout");

        if ((layout && strcmp(layout, "table") == 0) || pdf_xfa_layout_is_row(layout)) {
            memset(&subform_col_ctx, 0, sizeof(subform_col_ctx));
            if (col_ctx) subform_col_ctx = *col_ctx;

            if (layout && strcmp(layout, "table") == 0) {
                subform_col_ctx.count =
                    pdf_xfa_parse_column_widths(ctx, node, subform_col_ctx.widths, PDF_XFA_MAX_COLUMNS);
                subform_col_ctx.current_column = 0;
                subform_col_ctx.row_x = 0;
                subform_col_ctx.in_row = 0;
                subform_col_ctx.in_table = 1;
            } else {
                subform_col_ctx.current_column = 0;
                subform_col_ctx.row_x = 0;
                subform_col_ctx.in_row = 1;
            }
            active_col_ctx = &subform_col_ctx;
        }
    }

    if (node->name && strcmp(node->name, "pageSet") == 0) {
        for (child = node->first_child; child; child = child->next_sibling) {
            if (rctx->target_page_area) {
                if (!child->name || strcmp(child->name, "pageArea") != 0) continue;
                if (!pdf_xfa_pagearea_is_target(ctx, child, rctx->target_page_area)) continue;
            }
            pdf_xfa_render_tree(ctx, dev, ctm, xfa, child, page_h, font, pos, rctx, 1, col_ctx);
        }
        return;
    }

    if (under_pageset && rctx->target_page_area && node->name && strcmp(node->name, "pageArea") == 0 &&
        !pdf_xfa_pagearea_is_target(ctx, node, rctx->target_page_area))
        return;

    if (!under_pageset && pdf_xfa_node_is_field_or_draw(node->name)) {
        pdf_xfa_object* area;
        pdf_xfa_object* page_subform;

        if (pdf_xfa_object_is_prototype_def(ctx, node))
            render_node = 0;
        else if (node->flow_page >= 0)
            render_node = (node->flow_page == rctx->page_index);
        else {
            area = pdf_xfa_find_pagearea_ancestor(node);
            if (area && xfa->page_areas && xfa->page_count > 1 && rctx->target_page_area)
                render_node = pdf_xfa_pagearea_is_target(ctx, area, rctx->target_page_area);
            else if (xfa->page_subforms && xfa->page_subform_count > 1) {
                page_subform = pdf_xfa_find_page_subform_ancestor(ctx, node);
                if (page_subform)
                    render_node = pdf_xfa_page_subform_is_target(ctx, xfa, page_subform, rctx->page_index);
                else
                    render_node = (rctx->page_index == 0);
            } else
                render_node = (rctx->page_index == 0);
        }
    }

    if (render_node) {
        pdf_xfa_render_pos node_pos = *pos;
        int ignore_node_xy = 0;
        float layout_w = 0;

        if (!under_pageset && node->flow_page >= 0 && rctx->target_page_area) {
            pdf_xfa_object* content_area = pdf_xfa_find_content_area(rctx->target_page_area);
            if (content_area) pdf_xfa_render_pos_add_attrs(ctx, content_area, &node_pos);
            node_pos.ox += node->flow_x;
            node_pos.oy += node->flow_y;
            ignore_node_xy = 1;
        }

        if (active_col_ctx && active_col_ctx->in_row && pdf_xfa_parent_is_row(ctx, node)) {
            char* x_attr = pdf_xfa_object_get_attr(ctx, node, "x");

            layout_w = pdf_xfa_node_layout_width(ctx, node, active_col_ctx);
            if (!x_attr || !x_attr[0]) node_pos.ox += active_col_ctx->row_x;
        }

        if (node->name && strcmp(node->name, "field") == 0)
            pdf_xfa_render_field(ctx, dev, ctm, xfa, node, page_h, font, &node_pos, ignore_node_xy, layout_w, rctx);
        else if (!rctx->fields_only && node->name && strcmp(node->name, "draw") == 0)
            pdf_xfa_render_draw(ctx, dev, ctm, xfa, node, page_h, font, &node_pos, ignore_node_xy, layout_w, rctx);

        if (active_col_ctx && active_col_ctx->in_row && pdf_xfa_parent_is_row(ctx, node))
            pdf_xfa_row_advance(ctx, node, active_col_ctx, layout_w);
    }

    child_pos = *pos;
    if (pdf_xfa_node_shifts_children(node->name)) pdf_xfa_render_pos_add_attrs(ctx, node, &child_pos);

    if (pdf_xfa_subform_has_layout(ctx, node, pdf_xfa_layout_is_table)) {
        float tb_y = 0;

        for (child = node->first_child; child; child = child->next_sibling) {
            if (pdf_xfa_subform_has_layout(ctx, child, pdf_xfa_layout_is_row)) {
                pdf_xfa_render_pos row_pos = child_pos;

                row_pos.oy += tb_y;
                pdf_xfa_render_tree(ctx, dev, ctm, xfa, child, page_h, font, &row_pos, rctx, under_pageset,
                                    active_col_ctx);
                tb_y += pdf_xfa_row_content_height(ctx, child) +
                        pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, child, "y"), 0);
            } else
                pdf_xfa_render_tree(ctx, dev, ctm, xfa, child, page_h, font, &child_pos, rctx, under_pageset,
                                    active_col_ctx);
        }
    } else if (pdf_xfa_subform_has_layout(ctx, node, pdf_xfa_layout_is_tb)) {
        float tb_y = 0;

        for (child = node->first_child; child; child = child->next_sibling) {
            if (!pdf_xfa_tb_should_stack_child(ctx, child)) {
                pdf_xfa_render_tree(ctx, dev, ctm, xfa, child, page_h, font, &child_pos, rctx, under_pageset,
                                    active_col_ctx);
                continue;
            }

            {
                pdf_xfa_render_pos stack_pos = child_pos;

                stack_pos.oy += tb_y;
                pdf_xfa_render_tree(ctx, dev, ctm, xfa, child, page_h, font, &stack_pos, rctx, under_pageset,
                                    active_col_ctx);
                tb_y += pdf_xfa_tb_stack_advance(ctx, child);
            }
        }
    } else if (pdf_xfa_layout_is_position(ctx, node)) {
        for (child = node->first_child; child; child = child->next_sibling)
            pdf_xfa_render_tree(ctx, dev, ctm, xfa, child, page_h, font, &child_pos, rctx, under_pageset,
                                active_col_ctx);
    } else if (pdf_xfa_subform_has_layout(ctx, node, pdf_xfa_layout_is_lr_tb)) {
        float lr_x = 0;
        float lr_y = 0;
        float line_max_h = 0;
        float avail_w = pdf_xfa_subform_layout_width(ctx, node);

        for (child = node->first_child; child; child = child->next_sibling) {
            float child_w;
            float child_h;

            if (!pdf_xfa_tb_should_stack_child(ctx, child)) {
                pdf_xfa_render_tree(ctx, dev, ctm, xfa, child, page_h, font, &child_pos, rctx, under_pageset,
                                    active_col_ctx);
                continue;
            }

            child_w = pdf_xfa_lr_tb_child_width(ctx, child, active_col_ctx);
            child_h = pdf_xfa_lr_tb_child_height(ctx, child);

            if (avail_w > 0 && child_w > 0 && lr_x > 0 && lr_x + child_w > avail_w) {
                lr_y += line_max_h;
                lr_x = 0;
                line_max_h = 0;
            }

            {
                pdf_xfa_render_pos stack_pos = child_pos;

                stack_pos.ox += lr_x;
                stack_pos.oy += lr_y;
                pdf_xfa_render_tree(ctx, dev, ctm, xfa, child, page_h, font, &stack_pos, rctx, under_pageset,
                                    active_col_ctx);
            }

            if (child_w > 0) lr_x += child_w;
            if (child_h > line_max_h) line_max_h = child_h;
        }
    } else {
        for (child = node->first_child; child; child = child->next_sibling)
            pdf_xfa_render_tree(ctx, dev, ctm, xfa, child, page_h, font, &child_pos, rctx, under_pageset,
                                active_col_ctx);
    }
}

fz_display_list* pdf_xfa_factory_render_page(fz_context* ctx, pdf_xfa* xfa, int page_index, fz_matrix ctm) {
    fz_display_list* list = NULL;
    fz_device* dev = NULL;
    fz_font* font = NULL;
    fz_font* font_bold = NULL;
    fz_rect page_bbox;
    float page_h;
    float white[3] = {1.0f, 1.0f, 1.0f};
    pdf_xfa_render_pos pos = {0, 0};
    pdf_xfa_render_ctx rctx;

    memset(&rctx, 0, sizeof(rctx));

    if (!xfa || !xfa->valid || !xfa->form) return NULL;
    if (page_index < 0 || page_index >= pdf_xfa_factory_layout(ctx, xfa)) return NULL;

    page_bbox = xfa->page_bboxes[page_index];
    page_h = page_bbox.y1 - page_bbox.y0;
    xfa->render_fields = 0;
    xfa->render_draws = 0;
    xfa->render_borders = 0;
    xfa->render_lines = 0;

    rctx.page_index = page_index;
    rctx.fields_only = (xfa->render_flags & PDF_XFA_RENDER_FIELDS_ONLY) != 0;
    if (xfa->page_areas && page_index < xfa->page_count) rctx.target_page_area = xfa->page_areas[page_index];

    fz_var(list);
    fz_var(dev);
    fz_var(font);
    fz_var(font_bold);
    fz_try(ctx) {
        font = fz_new_base14_font(ctx, "Helvetica");
        font_bold = fz_new_base14_font(ctx, "Helvetica-Bold");
        rctx.default_font = font;
        rctx.font_bold = font_bold;
        rctx.fonts = xfa->fonts;

        list = fz_new_display_list(ctx, page_bbox);
        dev = fz_new_list_device(ctx, list);

        if (!(xfa->render_flags & PDF_XFA_RENDER_NO_BACKGROUND))
            pdf_xfa_render_fill_rect(ctx, dev, ctm, page_bbox, white);
        pdf_xfa_render_tree(ctx, dev, ctm, xfa, xfa->form, page_h, font, &pos, &rctx, 0, NULL);

        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
        fz_drop_font(ctx, font_bold);
        fz_drop_font(ctx, font);
    }
    fz_catch(ctx) {
        fz_drop_display_list(ctx, list);
        fz_rethrow(ctx);
    }

    return list;
}
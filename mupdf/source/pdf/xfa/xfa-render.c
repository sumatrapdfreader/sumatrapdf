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

typedef struct {
    float ox;
    float oy;
} pdf_xfa_render_pos;

typedef struct {
    int page_index;
    pdf_xfa_object* target_page_area;
} pdf_xfa_render_ctx;

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
    if (strcmp(node->name, "field") == 0) h = pdf_xfa_parse_measurement(h_attr, PDF_XFA_DEFAULT_FIELD_H);
    else if (strcmp(node->name, "draw") == 0) h = pdf_xfa_parse_measurement(h_attr, PDF_XFA_DEFAULT_DRAW_H);
    else return 0;
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

static void pdf_xfa_render_text_in_rect(fz_context* ctx, fz_device* dev, fz_matrix ctm, fz_font* font, const char* text,
                                        fz_rect rect, float pad, float rgb[3]) {
    fz_text* fztext = NULL;
    float fontsize;
    fz_matrix trm;

    if (!text || !text[0] || fz_is_empty_rect(rect)) return;

    rect.x0 += pad;
    rect.x1 -= pad;
    rect.y0 += pad;
    rect.y1 -= pad;
    if (rect.x1 <= rect.x0 || rect.y1 <= rect.y0) return;

    fontsize = rect.y1 - rect.y0 - pad;
    if (fontsize > 12.0f) fontsize = 12.0f;
    if (fontsize < 6.0f) fontsize = 6.0f;

    fz_var(fztext);
    fz_try(ctx) {
        fztext = fz_new_text(ctx);
        trm = fz_scale(fontsize, -fontsize);
        trm.e = rect.x0;
        trm.f = rect.y1 - pad;
        fz_show_string(ctx, fztext, font, trm, text, 0, 0, FZ_BIDI_LTR, FZ_LANG_UNSET);
        fz_fill_text(ctx, dev, fztext, ctm, fz_device_rgb(ctx), rgb, 1.0f, fz_default_color_params);
    }
    fz_always(ctx) fz_drop_text(ctx, fztext);
    fz_catch(ctx) fz_rethrow(ctx);
}

static pdf_xfa_object* pdf_xfa_find_check_button(fz_context* ctx, pdf_xfa_object* field) {
    pdf_xfa_object* ui;
    pdf_xfa_object* child;

    ui = pdf_xfa_object_find_child(field, "ui");
    if (!ui) return NULL;
    for (child = ui->first_child; child; child = child->next_sibling) {
        if (child->name && strcmp(child->name, "checkButton") == 0) return child;
    }
    return NULL;
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

static void pdf_xfa_render_field(fz_context* ctx, fz_device* dev, fz_matrix ctm, pdf_xfa* xfa, pdf_xfa_object* field,
                                 float page_h, fz_font* font, pdf_xfa_render_pos* pos, int ignore_node_xy,
                                 float layout_w) {
    fz_rect rect, box;
    float white[3] = {1.0f, 1.0f, 1.0f};
    float gray[3] = {0.75f, 0.75f, 0.75f};
    float black[3] = {0.0f, 0.0f, 0.0f};
    const char* text;
    pdf_xfa_object* check_btn;
    float check_size;

    rect = pdf_xfa_object_rect(ctx, field, page_h, pos, layout_w, PDF_XFA_DEFAULT_FIELD_H, ignore_node_xy);
    if (fz_is_empty_rect(rect)) return;

    xfa->render_fields++;

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
        text = pdf_xfa_node_text(ctx, field);
        if (pdf_xfa_value_is_checked(text)) pdf_xfa_render_check_mark(ctx, dev, ctm, box, black);
        return;
    }

    if (!pdf_xfa_render_border(ctx, dev, ctm, xfa, field, rect)) {
        pdf_xfa_render_fill_rect(ctx, dev, ctm, rect, white);
        pdf_xfa_render_stroke_rect(ctx, dev, ctm, rect, gray, 1.0f);
    }

    text = pdf_xfa_node_text(ctx, field);
    pdf_xfa_render_text_in_rect(ctx, dev, ctm, font, text, rect, PDF_XFA_FIELD_PAD, black);
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
                                float layout_w) {
    fz_rect rect;
    float black[3] = {0.0f, 0.0f, 0.0f};
    const char* text;
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
    pdf_xfa_render_text_in_rect(ctx, dev, ctm, font, text, rect, 0, black);
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
            pdf_xfa_render_field(ctx, dev, ctm, xfa, node, page_h, font, &node_pos, ignore_node_xy, layout_w);
        else if (node->name && strcmp(node->name, "draw") == 0)
            pdf_xfa_render_draw(ctx, dev, ctm, xfa, node, page_h, font, &node_pos, ignore_node_xy, layout_w);

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
                tb_y += pdf_xfa_row_content_height(ctx, child);
            } else
                pdf_xfa_render_tree(ctx, dev, ctm, xfa, child, page_h, font, &child_pos, rctx, under_pageset,
                                    active_col_ctx);
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
    fz_rect page_bbox;
    float page_h;
    float white[3] = {1.0f, 1.0f, 1.0f};
    pdf_xfa_render_pos pos = {0, 0};
    pdf_xfa_render_ctx rctx = {0, NULL};

    if (!xfa || !xfa->valid || !xfa->form) return NULL;
    if (page_index < 0 || page_index >= pdf_xfa_factory_layout(ctx, xfa)) return NULL;

    page_bbox = xfa->page_bboxes[page_index];
    page_h = page_bbox.y1 - page_bbox.y0;
    xfa->render_fields = 0;
    xfa->render_draws = 0;
    xfa->render_borders = 0;
    xfa->render_lines = 0;

    rctx.page_index = page_index;
    if (xfa->page_areas && page_index < xfa->page_count) rctx.target_page_area = xfa->page_areas[page_index];

    fz_var(list);
    fz_var(dev);
    fz_var(font);
    fz_try(ctx) {
        font = fz_new_base14_font(ctx, "Helvetica");

        list = fz_new_display_list(ctx, page_bbox);
        dev = fz_new_list_device(ctx, list);

        pdf_xfa_render_fill_rect(ctx, dev, ctm, page_bbox, white);
        pdf_xfa_render_tree(ctx, dev, ctm, xfa, xfa->form, page_h, font, &pos, &rctx, 0, NULL);

        fz_close_device(ctx, dev);
    }
    fz_always(ctx) {
        fz_drop_device(ctx, dev);
        fz_drop_font(ctx, font);
    }
    fz_catch(ctx) {
        fz_drop_display_list(ctx, list);
        fz_rethrow(ctx);
    }

    return list;
}
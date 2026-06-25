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

#include <string.h>

#define PDF_XFA_FIELD_PAD 2.0f
#define PDF_XFA_DEFAULT_DRAW_H 14.0f
typedef struct {
    float ox;
    float oy;
} pdf_xfa_render_pos;

typedef struct {
    int page_index;
    pdf_xfa_object* target_page_area;
} pdf_xfa_render_ctx;

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

static pdf_xfa_object* pdf_xfa_find_content_area(pdf_xfa_object* page_area) {
    pdf_xfa_object* child;

    for (child = page_area ? page_area->first_child : NULL; child; child = child->next_sibling) {
        if (child->name && strcmp(child->name, "contentArea") == 0) return child;
    }
    return NULL;
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
                                 float page_h, fz_font* font, pdf_xfa_render_pos* pos, int ignore_node_xy) {
    fz_rect rect;
    float white[3] = {1.0f, 1.0f, 1.0f};
    float gray[3] = {0.75f, 0.75f, 0.75f};
    float black[3] = {0.0f, 0.0f, 0.0f};
    const char* text;

    rect = pdf_xfa_object_rect(ctx, field, page_h, pos, 0, 0, ignore_node_xy);
    if (fz_is_empty_rect(rect)) return;

    xfa->render_fields++;

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
                                float page_h, fz_font* font, pdf_xfa_render_pos* pos, int ignore_node_xy) {
    fz_rect rect;
    float black[3] = {0.0f, 0.0f, 0.0f};
    const char* text;
    int drew_line;

    drew_line = pdf_xfa_render_draw_line(ctx, dev, ctm, xfa, draw, page_h, pos, ignore_node_xy);
    if (drew_line) {
        xfa->render_draws++;
        return;
    }

    rect = pdf_xfa_object_rect(ctx, draw, page_h, pos, 0, PDF_XFA_DEFAULT_DRAW_H, ignore_node_xy);
    if (fz_is_empty_rect(rect)) return;

    xfa->render_draws++;

    pdf_xfa_render_border(ctx, dev, ctm, xfa, draw, rect);

    text = pdf_xfa_node_text(ctx, draw);
    pdf_xfa_render_text_in_rect(ctx, dev, ctm, font, text, rect, 0, black);
}

static void pdf_xfa_render_tree(fz_context* ctx, fz_device* dev, fz_matrix ctm, pdf_xfa* xfa, pdf_xfa_object* node,
                                float page_h, fz_font* font, pdf_xfa_render_pos* pos, pdf_xfa_render_ctx* rctx,
                                int under_pageset) {
    pdf_xfa_object* child;
    pdf_xfa_render_pos child_pos;
    int render_node = 1;

    if (!node) return;

    if (node->name && strcmp(node->name, "pageSet") == 0) {
        for (child = node->first_child; child; child = child->next_sibling) {
            if (rctx->target_page_area) {
                if (!child->name || strcmp(child->name, "pageArea") != 0) continue;
                if (!pdf_xfa_pagearea_is_target(ctx, child, rctx->target_page_area)) continue;
            }
            pdf_xfa_render_tree(ctx, dev, ctm, xfa, child, page_h, font, pos, rctx, 1);
        }
        return;
    }

    if (under_pageset && rctx->target_page_area && node->name && strcmp(node->name, "pageArea") == 0 &&
        !pdf_xfa_pagearea_is_target(ctx, node, rctx->target_page_area))
        return;

    if (!under_pageset && pdf_xfa_node_is_field_or_draw(node->name)) {
        if (pdf_xfa_object_is_prototype_def(ctx, node))
            render_node = 0;
        else if (node->flow_page >= 0)
            render_node = (node->flow_page == rctx->page_index);
        else
            render_node = (rctx->page_index == 0);
    }

    if (render_node) {
        pdf_xfa_render_pos node_pos = *pos;
        int ignore_node_xy = 0;

        if (!under_pageset && node->flow_page >= 0 && rctx->target_page_area) {
            pdf_xfa_object* content_area = pdf_xfa_find_content_area(rctx->target_page_area);
            if (content_area) pdf_xfa_render_pos_add_attrs(ctx, content_area, &node_pos);
            node_pos.ox += node->flow_x;
            node_pos.oy += node->flow_y;
            ignore_node_xy = 1;
        }

        if (node->name && strcmp(node->name, "field") == 0)
            pdf_xfa_render_field(ctx, dev, ctm, xfa, node, page_h, font, &node_pos, ignore_node_xy);
        else if (node->name && strcmp(node->name, "draw") == 0)
            pdf_xfa_render_draw(ctx, dev, ctm, xfa, node, page_h, font, &node_pos, ignore_node_xy);
    }

    child_pos = *pos;
    if (pdf_xfa_node_shifts_children(node->name)) pdf_xfa_render_pos_add_attrs(ctx, node, &child_pos);

    for (child = node->first_child; child; child = child->next_sibling)
        pdf_xfa_render_tree(ctx, dev, ctm, xfa, child, page_h, font, &child_pos, rctx, under_pageset);
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
        pdf_xfa_render_tree(ctx, dev, ctm, xfa, xfa->form, page_h, font, &pos, &rctx, 0);

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
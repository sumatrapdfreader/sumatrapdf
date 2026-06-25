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

#include "mupdf/pdf/annot.h"

#include <string.h>

#define PDF_XFA_FIELD_PAD 2.0f

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

static fz_rect pdf_xfa_object_rect(fz_context* ctx, pdf_xfa_object* node, float page_h) {
    char *x, *y, *w, *h;
    float xf, yf, wf, hf;
    fz_rect rect = fz_empty_rect;

    x = pdf_xfa_object_get_attr(ctx, node, "x");
    y = pdf_xfa_object_get_attr(ctx, node, "y");
    w = pdf_xfa_object_get_attr(ctx, node, "w");
    h = pdf_xfa_object_get_attr(ctx, node, "h");

    xf = pdf_xfa_parse_measurement(x, 0);
    yf = pdf_xfa_parse_measurement(y, 0);
    wf = pdf_xfa_parse_measurement(w, 0);
    hf = pdf_xfa_parse_measurement(h, 0);

    if (wf <= 0 || hf <= 0) return rect;

    rect.x0 = xf;
    rect.x1 = xf + wf;
    rect.y1 = page_h - yf;
    rect.y0 = rect.y1 - hf;
    return rect;
}

static const char* pdf_xfa_field_text(fz_context* ctx, pdf_xfa_object* field) {
    pdf_xfa_object* value;

    if (field->content && field->content[0]) return field->content;

    value = pdf_xfa_object_find_child(field, "value");
    if (value) {
        const char* text = pdf_xfa_object_text(ctx, value);
        if (text && text[0]) return text;
    }

    return NULL;
}

static void pdf_xfa_render_field(fz_context* ctx, fz_device* dev, fz_matrix ctm, pdf_xfa_object* field, float page_h,
                                 fz_font* font) {
    fz_rect rect, text_rect;
    float white[3] = {1.0f, 1.0f, 1.0f};
    float gray[3] = {0.75f, 0.75f, 0.75f};
    float black[3] = {0.0f, 0.0f, 0.0f};
    const char* text;
    fz_text* fztext = NULL;

    rect = pdf_xfa_object_rect(ctx, field, page_h);
    if (fz_is_empty_rect(rect)) return;

    pdf_xfa_render_fill_rect(ctx, dev, ctm, rect, white);
    pdf_xfa_render_stroke_rect(ctx, dev, ctm, rect, gray, 1.0f);

    text = pdf_xfa_field_text(ctx, field);
    if (!text) return;

    text_rect = rect;
    text_rect.x0 += PDF_XFA_FIELD_PAD;
    text_rect.x1 -= PDF_XFA_FIELD_PAD;
    text_rect.y0 += PDF_XFA_FIELD_PAD;
    text_rect.y1 -= PDF_XFA_FIELD_PAD;

    fz_var(fztext);
    fz_try(ctx) {
        fztext = pdf_layout_fit_text(ctx, font, FZ_LANG_UNSET, text, text_rect);
        fz_fill_text(ctx, dev, fztext, ctm, fz_device_rgb(ctx), black, 1.0f, fz_default_color_params);
    }
    fz_always(ctx) fz_drop_text(ctx, fztext);
    fz_catch(ctx) fz_rethrow(ctx);
}

static void pdf_xfa_render_tree(fz_context* ctx, fz_device* dev, fz_matrix ctm, pdf_xfa_object* node, float page_h,
                                fz_font* font) {
    pdf_xfa_object* child;

    if (!node) return;

    if (node->name && strcmp(node->name, "field") == 0) pdf_xfa_render_field(ctx, dev, ctm, node, page_h, font);

    for (child = node->first_child; child; child = child->next_sibling)
        pdf_xfa_render_tree(ctx, dev, ctm, child, page_h, font);
}

fz_display_list* pdf_xfa_factory_render_page(fz_context* ctx, pdf_xfa* xfa, int page_index, fz_matrix ctm) {
    fz_display_list* list = NULL;
    fz_device* dev = NULL;
    fz_font* font = NULL;
    fz_rect page_bbox;
    float page_h;
    float white[3] = {1.0f, 1.0f, 1.0f};

    if (!xfa || !xfa->valid || !xfa->form) return NULL;
    if (page_index < 0 || page_index >= pdf_xfa_factory_layout(ctx, xfa)) return NULL;

    page_bbox = xfa->page_bboxes[page_index];
    page_h = page_bbox.y1 - page_bbox.y0;

    fz_var(list);
    fz_var(dev);
    fz_var(font);
    fz_try(ctx) {
        font = fz_new_base14_font(ctx, "Helvetica");

        list = fz_new_display_list(ctx, page_bbox);
        dev = fz_new_list_device(ctx, list);

        pdf_xfa_render_fill_rect(ctx, dev, ctm, page_bbox, white);
        pdf_xfa_render_tree(ctx, dev, ctm, xfa->form, page_h, font);

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
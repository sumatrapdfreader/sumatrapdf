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

#define PDF_XFA_DEFAULT_PAGE_W 612.0f
#define PDF_XFA_DEFAULT_PAGE_H 792.0f
#define PDF_XFA_DEFAULT_FIELD_H 18.0f
#define PDF_XFA_DEFAULT_DRAW_H 14.0f

typedef struct {
    float x;
    float y;
    float w;
    float h;
} pdf_xfa_content_metrics;

static pdf_xfa_object* pdf_xfa_find_content_area(pdf_xfa_object* page_area) {
    pdf_xfa_object* child;

    for (child = page_area ? page_area->first_child : NULL; child; child = child->next_sibling) {
        if (child->name && strcmp(child->name, "contentArea") == 0) return child;
    }
    return NULL;
}

static pdf_xfa_content_metrics pdf_xfa_get_content_metrics(fz_context* ctx, pdf_xfa_object* content_area, float page_w,
                                                           float page_h) {
    pdf_xfa_content_metrics m;

    m.x = 0;
    m.y = 0;
    m.w = page_w;
    m.h = page_h;

    if (!content_area) return m;

    m.x = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, content_area, "x"), 0);
    m.y = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, content_area, "y"), 0);
    m.w = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, content_area, "w"), m.w);
    m.h = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, content_area, "h"), m.h);
    return m;
}

static int pdf_xfa_is_flow_body_child(fz_context* ctx, pdf_xfa_object* node) {
    if (!node || !node->name) return 0;
    if (strcmp(node->name, "pageSet") == 0) return 0;
    if (pdf_xfa_object_is_prototype_def(ctx, node)) return 0;
    return strcmp(node->name, "field") == 0 || strcmp(node->name, "draw") == 0;
}

static float pdf_xfa_flow_node_height(fz_context* ctx, pdf_xfa_object* node) {
    char* h;

    if (!node || !node->name) return 0;

    h = pdf_xfa_object_get_attr(ctx, node, "h");
    if (strcmp(node->name, "field") == 0) return pdf_xfa_parse_measurement(h, PDF_XFA_DEFAULT_FIELD_H);
    if (strcmp(node->name, "draw") == 0) return pdf_xfa_parse_measurement(h, PDF_XFA_DEFAULT_DRAW_H);
    return 0;
}

static float pdf_xfa_flow_node_width(fz_context* ctx, pdf_xfa_object* node) {
    return pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, node, "w"), 0);
}

static void pdf_xfa_layout_flowed_tb(fz_context* ctx, pdf_xfa* xfa, pdf_xfa_object* subform) {
    pdf_xfa_object* child;
    char* layout;
    int page_idx;
    float flow_y;
    pdf_xfa_content_metrics area;

    layout = pdf_xfa_object_get_attr(ctx, subform, "layout");
    if (!layout || strcmp(layout, "tb") != 0) return;
    if (!xfa->page_areas || xfa->page_count <= 0) return;

    page_idx = 0;
    flow_y = 0;
    area = pdf_xfa_get_content_metrics(ctx, pdf_xfa_find_content_area(xfa->page_areas[0]), xfa->page_bboxes[0].x1,
                                       xfa->page_bboxes[0].y1);

    for (child = subform->first_child; child; child = child->next_sibling) {
        float xf, yf, hf, wf;

        if (!pdf_xfa_is_flow_body_child(ctx, child)) continue;

        hf = pdf_xfa_flow_node_height(ctx, child);
        wf = pdf_xfa_flow_node_width(ctx, child);
        if (hf <= 0 || wf <= 0) continue;

        if (flow_y > 0 && flow_y + hf > area.h && page_idx + 1 < xfa->page_count) {
            page_idx++;
            flow_y = 0;
            area = pdf_xfa_get_content_metrics(ctx, pdf_xfa_find_content_area(xfa->page_areas[page_idx]),
                                               xfa->page_bboxes[page_idx].x1, xfa->page_bboxes[page_idx].y1);
        }

        xf = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, child, "x"), 0);
        yf = pdf_xfa_parse_measurement(pdf_xfa_object_get_attr(ctx, child, "y"), 0);

        child->flow_page = page_idx;
        child->flow_x = xf;
        child->flow_y = flow_y + yf;
        flow_y += hf + yf;
    }
}

static void pdf_xfa_layout_flow_subforms(fz_context* ctx, pdf_xfa* xfa, pdf_xfa_object* node) {
    pdf_xfa_object* child;

    if (!node) return;

    if (node->name && strcmp(node->name, "subform") == 0) pdf_xfa_layout_flowed_tb(ctx, xfa, node);

    for (child = node->first_child; child; child = child->next_sibling) pdf_xfa_layout_flow_subforms(ctx, xfa, child);
}

float pdf_xfa_parse_measurement(const char* text, float default_pt) {
    char* end;
    double v;

    if (!text || !text[0]) return default_pt;

    v = strtod(text, &end);
    if (end == text) return default_pt;

    while (*end == ' ' || *end == '\t') end++;

    if (*end == 'i' && end[1] == 'n') return (float)(v * 72.0);
    if (*end == 'p' && end[1] == 't') return (float)v;
    if (*end == 'm' && end[1] == 'm') return (float)(v * 72.0 / 25.4);
    if (*end == 'c' && end[1] == 'm') return (float)(v * 72.0 / 2.54);

    return (float)v;
}

static fz_rect pdf_xfa_pagearea_bbox(fz_context* ctx, pdf_xfa_object* page_area) {
    pdf_xfa_object* child;
    char *w, *h, *short_dim, *long_dim;
    float width = PDF_XFA_DEFAULT_PAGE_W;
    float height = PDF_XFA_DEFAULT_PAGE_H;

    w = pdf_xfa_object_get_attr(ctx, page_area, "w");
    h = pdf_xfa_object_get_attr(ctx, page_area, "h");
    if (w && h) {
        width = pdf_xfa_parse_measurement(w, width);
        height = pdf_xfa_parse_measurement(h, height);
    } else {
        for (child = page_area->first_child; child; child = child->next_sibling) {
            if (!child->name || strcmp(child->name, "medium") != 0) continue;
            short_dim = pdf_xfa_object_get_attr(ctx, child, "short");
            long_dim = pdf_xfa_object_get_attr(ctx, child, "long");
            if (short_dim && long_dim) {
                width = pdf_xfa_parse_measurement(short_dim, width);
                height = pdf_xfa_parse_measurement(long_dim, height);
                if (width > height) {
                    float tmp = width;
                    width = height;
                    height = tmp;
                }
            }
            break;
        }
    }

    return fz_make_rect(0, 0, width, height);
}

static pdf_xfa_object* pdf_xfa_find_pageset(pdf_xfa_object* node) {
    pdf_xfa_object* child;

    if (!node) return NULL;
    if (node->name && strcmp(node->name, "pageSet") == 0) return node;
    for (child = node->first_child; child; child = child->next_sibling) {
        pdf_xfa_object* hit = pdf_xfa_find_pageset(child);
        if (hit) return hit;
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

static void pdf_xfa_count_fields_imp(pdf_xfa_object* node, int under_pageset, int* in_pageset, int* outside_pageset,
                                     int* with_pagearea) {
    pdf_xfa_object* child;

    if (!node) return;

    if (node->name && strcmp(node->name, "pageSet") == 0) under_pageset = 1;

    if (node->name && strcmp(node->name, "field") == 0) {
        if (under_pageset) (*in_pageset)++;
        else (*outside_pageset)++;
        if (pdf_xfa_find_pagearea_ancestor(node)) (*with_pagearea)++;
    }

    for (child = node->first_child; child; child = child->next_sibling)
        pdf_xfa_count_fields_imp(child, under_pageset, in_pageset, outside_pageset, with_pagearea);
}

void pdf_xfa_count_field_stats(pdf_xfa_object* root, int* in_pageset, int* outside_pageset, int* with_pagearea) {
    int in_ps = 0;
    int out_ps = 0;
    int with_pa = 0;

    if (in_pageset) *in_pageset = 0;
    if (outside_pageset) *outside_pageset = 0;
    if (with_pagearea) *with_pagearea = 0;

    pdf_xfa_count_fields_imp(root, 0, &in_ps, &out_ps, &with_pa);

    if (in_pageset) *in_pageset = in_ps;
    if (outside_pageset) *outside_pageset = out_ps;
    if (with_pagearea) *with_pagearea = with_pa;
}

static int pdf_xfa_is_page_subform_name(const char* name) {
    int i;

    if (!name || strncmp(name, "Page", 4) != 0) return 0;
    if (!name[4]) return 0;
    for (i = 4; name[i]; i++)
        if (name[i] < '0' || name[i] > '9') return 0;
    return 1;
}

int pdf_xfa_page_subform_index(fz_context* ctx, pdf_xfa_object* subform) {
    char* name;
    char* end;
    long n;

    (void)ctx;

    if (!subform || !subform->name || strcmp(subform->name, "subform") != 0) return -1;
    name = pdf_xfa_object_get_attr(ctx, subform, "name");
    if (!pdf_xfa_is_page_subform_name(name)) return -1;
    n = strtol(name + 4, &end, 10);
    if (end == name + 4 || n <= 0) return -1;
    return (int)(n - 1);
}

static pdf_xfa_object* pdf_xfa_layout_body(pdf_xfa_object* form) {
    if (!form) return NULL;
    if (form->name && strcmp(form->name, "subform") == 0) return form;
    return pdf_xfa_object_find_child(form, "subform");
}

static int pdf_xfa_collect_page_subforms(fz_context* ctx, pdf_xfa_object* form, pdf_xfa_object** subs, int max_subforms) {
    pdf_xfa_object* body;
    pdf_xfa_object* child;
    pdf_xfa_object* found[64];
    int found_n = 0;
    int i, j, n;

    (void)ctx;

    body = pdf_xfa_layout_body(form);
    if (!body) return 0;

    for (child = body->first_child; child; child = child->next_sibling) {
        char* name;

        if (!child->name || strcmp(child->name, "subform") != 0) continue;
        name = pdf_xfa_object_get_attr(ctx, child, "name");
        if (!pdf_xfa_is_page_subform_name(name)) continue;
        if (found_n < 64) found[found_n++] = child;
    }

    for (i = 0; i < found_n; i++) {
        for (j = i + 1; j < found_n; j++) {
            int ai = pdf_xfa_page_subform_index(ctx, found[i]);
            int aj = pdf_xfa_page_subform_index(ctx, found[j]);
            pdf_xfa_object* tmp;

            if (aj < ai) {
                tmp = found[i];
                found[i] = found[j];
                found[j] = tmp;
            }
        }
    }

    n = 0;
    for (i = 0; i < found_n && n < max_subforms; i++) subs[n++] = found[i];
    return n;
}

static int pdf_xfa_collect_pageareas(fz_context* ctx, pdf_xfa_object* node, pdf_xfa_object** page_areas, int max_pages,
                                     int n) {
    pdf_xfa_object* pageset;
    pdf_xfa_object* child;

    (void)ctx;

    if (!node || n >= max_pages) return n;

    pageset = pdf_xfa_find_pageset(node);
    if (pageset) {
        for (child = pageset->first_child; child && n < max_pages; child = child->next_sibling) {
            if (child->name && strcmp(child->name, "pageArea") == 0) page_areas[n++] = child;
        }
        return n;
    }

    if (node->name && strcmp(node->name, "pageArea") == 0) page_areas[n++] = node;

    for (child = node->first_child; child && n < max_pages; child = child->next_sibling)
        n = pdf_xfa_collect_pageareas(ctx, child, page_areas, max_pages, n);

    return n;
}

int pdf_xfa_factory_layout(fz_context* ctx, pdf_xfa* xfa) {
    pdf_xfa_object* page_areas[256];
    pdf_xfa_object* page_subforms[64];
    int n, sn, i;

    if (!xfa || !xfa->valid) return 0;

    if (xfa->layout_done) return xfa->page_count;

    xfa->layout_done = 1;
    xfa->pages = NULL;

    if (!xfa->form) {
        xfa->page_count = 0;
        xfa->page_bboxes = NULL;
        return 0;
    }

    n = pdf_xfa_collect_pageareas(ctx, xfa->form, page_areas, 256, 0);

    fz_try(ctx) {
        if (n == 0) {
            xfa->page_count = 1;
            xfa->page_bboxes = fz_malloc_array(ctx, 1, fz_rect);
            xfa->page_areas = NULL;
            xfa->page_bboxes[0] = fz_make_rect(0, 0, PDF_XFA_DEFAULT_PAGE_W, PDF_XFA_DEFAULT_PAGE_H);
        } else {
            xfa->page_count = n;
            xfa->page_bboxes = fz_malloc_array(ctx, n, fz_rect);
            xfa->page_areas = fz_malloc_array(ctx, n, pdf_xfa_object*);
            for (i = 0; i < n; i++) {
                xfa->page_areas[i] = page_areas[i];
                xfa->page_bboxes[i] = pdf_xfa_pagearea_bbox(ctx, page_areas[i]);
            }
            sn = pdf_xfa_collect_page_subforms(ctx, xfa->form, page_subforms, 64);
            if (sn > 0) {
                xfa->page_subform_count = sn;
                xfa->page_subforms = fz_malloc_array(ctx, sn, pdf_xfa_object*);
                for (i = 0; i < sn; i++) xfa->page_subforms[i] = page_subforms[i];
            } else {
                xfa->page_subform_count = 0;
                xfa->page_subforms = NULL;
            }
            pdf_xfa_layout_flow_subforms(ctx, xfa, xfa->form);
            pdf_xfa_count_field_stats(xfa->form, &xfa->fields_in_pageset, &xfa->fields_outside_pageset,
                                      &xfa->fields_with_pagearea);
        }
    }
    fz_catch(ctx) {
        fz_free(ctx, xfa->page_bboxes);
        fz_free(ctx, xfa->page_areas);
        fz_free(ctx, xfa->page_subforms);
        xfa->page_bboxes = NULL;
        xfa->page_areas = NULL;
        xfa->page_subforms = NULL;
        xfa->page_subform_count = 0;
        xfa->page_count = 0;
        fz_rethrow(ctx);
    }

    return xfa->page_count;
}
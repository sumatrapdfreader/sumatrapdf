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

static int pdf_xfa_collect_pageareas(fz_context* ctx, pdf_xfa_object* node, pdf_xfa_object** page_areas, int max_pages,
                                     int n) {
    pdf_xfa_object* child;

    (void)ctx;

    if (!node || n >= max_pages) return n;

    if (node->name && strcmp(node->name, "pageArea") == 0) page_areas[n++] = node;

    for (child = node->first_child; child && n < max_pages; child = child->next_sibling)
        n = pdf_xfa_collect_pageareas(ctx, child, page_areas, max_pages, n);

    return n;
}

int pdf_xfa_factory_layout(fz_context* ctx, pdf_xfa* xfa) {
    pdf_xfa_object* page_areas[256];
    int n, i;

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
        }
    }
    fz_catch(ctx) {
        fz_free(ctx, xfa->page_bboxes);
        fz_free(ctx, xfa->page_areas);
        xfa->page_bboxes = NULL;
        xfa->page_areas = NULL;
        xfa->page_count = 0;
        fz_rethrow(ctx);
    }

    return xfa->page_count;
}
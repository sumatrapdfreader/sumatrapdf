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

static void pdf_xfa_bind_value(fz_context* ctx, fz_pool* pool, pdf_xfa_object* form, pdf_xfa_object* data) {
    const char* value;

    form->data_node = data;
    if (!pdf_xfa_object_is_data_value(data)) return;

    value = pdf_xfa_object_get_data_value(ctx, data);
    if (!value) return;

    /* Phase 2: store value as content until typed Field nodes exist */
    if (!form->content || !form->content[0]) form->content = fz_pool_strdup(ctx, pool, value);
}

static pdf_xfa_object* pdf_xfa_find_data_child(pdf_xfa_object* data, const char* name, int skip_consumed) {
    pdf_xfa_object* child;
    char* named;

    for (child = data ? data->first_child : NULL; child; child = child->next_sibling) {
        if (skip_consumed && child->consumed) continue;
        named = pdf_xfa_object_get_attr(NULL, child, "name");
        if ((child->name && strcmp(child->name, name) == 0) || (named && strcmp(named, name) == 0)) return child;
    }
    return NULL;
}

static void pdf_xfa_bind_element(fz_context* ctx, fz_pool* pool, pdf_xfa* xfa, pdf_xfa_object* form,
                                 pdf_xfa_object* data, int merge_mode);

static void pdf_xfa_bind_element(fz_context* ctx, fz_pool* pool, pdf_xfa* xfa, pdf_xfa_object* form,
                                 pdf_xfa_object* data, int merge_mode) {
    pdf_xfa_object *child, *match, *next;
    char *name, *ref;
    int n;

    for (child = form->first_child; child; child = next) {
        next = child->next_sibling;

        if (child->data_node) continue;

        /* Highest-level subform binds to first data child (bind.js). */
        if (strcmp(child->name, "subform") == 0 && merge_mode < 0) {
            if (data && data->first_child) pdf_xfa_bind_element(ctx, pool, xfa, child, data->first_child, 0);
            continue;
        }

        name = pdf_xfa_object_get_attr(ctx, child, "name");
        if (!name || !name[0]) {
            pdf_xfa_bind_element(ctx, pool, xfa, child, data, merge_mode);
            continue;
        }

        ref = pdf_xfa_object_get_attr(ctx, child, "ref");
        if (ref && ref[0]) {
            pdf_xfa_object* hits[8];
            n = pdf_xfa_som_search(ctx, xfa->root, data, ref, 1, hits, 8);
            if (n > 0) {
                pdf_xfa_bind_element(ctx, pool, xfa, child, hits[0], merge_mode);
                continue;
            }
        }

        match = pdf_xfa_find_data_child(data, name, merge_mode > 0);
        if (match) {
            if (merge_mode > 0) match->consumed = 1;
            if (pdf_xfa_object_is_data_value(match))
                pdf_xfa_bind_value(ctx, pool, child, match);
            else
                pdf_xfa_bind_element(ctx, pool, xfa, child, match, merge_mode);
        } else
            pdf_xfa_bind_element(ctx, pool, xfa, child, data, merge_mode);
    }
}

pdf_xfa_object* pdf_xfa_bind(fz_context* ctx, fz_pool* pool, pdf_xfa* xfa) {
    pdf_xfa_object *xdp, *template_node, *datasets, *data, *form;
    int merge_mode = -1;
    int empty_merge;

    if (!xfa || !xfa->root) return NULL;

    xdp = xfa->root;
    if (!pdf_xfa_object_find_child(xdp, "template") && !pdf_xfa_object_find_child(xdp, "datasets")) {
        pdf_xfa_object* nested = pdf_xfa_object_find_child(xdp, "xdp");
        if (nested) xdp = nested;
    }
    template_node = pdf_xfa_object_find_child(xdp, "template");
    datasets = pdf_xfa_object_find_child(xdp, "datasets");
    data = datasets ? pdf_xfa_object_find_child(datasets, "data") : NULL;

    if (!template_node) fz_throw(ctx, FZ_ERROR_FORMAT, "XFA: missing template packet");

    if (!data) {
        data = pdf_xfa_new_object(ctx, pool, PDF_XFA_NS_DATASETS, "data", 1);
        if (datasets) pdf_xfa_object_append_child(ctx, pool, datasets, data);
    }

    xfa->template_node = template_node;
    xfa->datasets_node = datasets;
    xfa->data_node = data;

    empty_merge = !data->first_child;
    form = pdf_xfa_object_clone(ctx, pool, template_node);
    if (!form) fz_throw(ctx, FZ_ERROR_GENERIC, "XFA: failed to clone template");

    form->global = &xfa->global;
    pdf_xfa_bind_element(ctx, pool, xfa, form, data, empty_merge ? 0 : merge_mode);

    return form;
}
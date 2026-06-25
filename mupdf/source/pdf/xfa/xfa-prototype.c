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

#define PDF_XFA_PROTO_MAX_ANCESTORS 32

static int pdf_xfa_ancestor_has(pdf_xfa_object** ancestors, int n, pdf_xfa_object* node) {
    int i;

    for (i = 0; i < n; i++)
        if (ancestors[i] == node) return 1;
    return 0;
}

static int pdf_xfa_extract_som_ref(const char* ref, char* buf, size_t buflen) {
    const char *start, *end;
    size_t len;

    if (strncmp(ref, "#som(", 5) == 0) start = ref + 5;
    else if (strncmp(ref, ".#som(", 6) == 0) start = ref + 6;
    else return 0;

    end = strrchr(start, ')');
    if (!end || end <= start) return 0;

    len = (size_t)(end - start);
    if (len >= buflen) len = buflen - 1;
    memcpy(buf, start, len);
    buf[len] = 0;
    return 1;
}

static void pdf_xfa_clear_attr(fz_context* ctx, fz_pool* pool, pdf_xfa_object* node, const char* name) {
    pdf_xfa_attr* attr;

    (void)ctx;
    for (attr = node->first_attr; attr; attr = attr->next) {
        if (strcmp(attr->name, name) == 0) {
            attr->value = fz_pool_strdup(ctx, pool, "");
            return;
        }
    }
}

static pdf_xfa_object* pdf_xfa_lookup_prototype(fz_context* ctx, pdf_xfa* xfa, pdf_xfa_object* node, const char* use,
                                                const char* usehref, pdf_xfa_object** ancestors, int n_ancestors) {
    const char* id = NULL;
    char som_buf[256];
    pdf_xfa_object* proto = NULL;

    if (usehref && usehref[0]) {
        if (pdf_xfa_extract_som_ref(usehref, som_buf, sizeof som_buf)) {
            pdf_xfa_object* hits[4];
            int n = pdf_xfa_som_search(ctx, xfa->root, node, som_buf, 1, hits, 4);
            if (n > 0) proto = hits[0];
        } else if (strncmp(usehref, ".#", 2) == 0) id = usehref + 2;
        else if (usehref[0] == '#') id = usehref + 1;
    } else if (use && use[0]) {
        if (use[0] == '#') id = use + 1;
        else {
            pdf_xfa_object* hits[4];
            int n = pdf_xfa_som_search(ctx, xfa->root, node, use, 1, hits, 4);
            if (n > 0) proto = hits[0];
        }
    }

    if (!proto && id && xfa->ids) {
        char key[FZ_HASH_TABLE_KEY_LENGTH];

        pdf_xfa_ids_make_key(key, id);
        proto = fz_hash_find(ctx, xfa->ids, key);
    }

    if (!proto) return NULL;
    if (proto->name && node->name && strcmp(proto->name, node->name) != 0) return NULL;
    if (pdf_xfa_ancestor_has(ancestors, n_ancestors, proto)) return NULL;

    return proto;
}

static void pdf_xfa_copy_missing_attrs(fz_context* ctx, fz_pool* pool, pdf_xfa_object* dst, pdf_xfa_object* src) {
    pdf_xfa_attr* attr;

    for (attr = src->first_attr; attr; attr = attr->next) {
        if (!pdf_xfa_object_get_attr(ctx, dst, attr->name))
            pdf_xfa_object_add_attr(ctx, pool, dst, attr->name, attr->value);
    }
}

static pdf_xfa_object* pdf_xfa_find_child_by_name(pdf_xfa_object* node, const char* name) {
    pdf_xfa_object* child;

    for (child = node ? node->first_child : NULL; child; child = child->next_sibling) {
        if (child->name && name && strcmp(child->name, name) == 0) return child;
    }
    return NULL;
}

static void pdf_xfa_apply_prototype(fz_context* ctx, fz_pool* pool, pdf_xfa* xfa, pdf_xfa_object* node,
                                    pdf_xfa_object* proto, pdf_xfa_object** ancestors, int n_ancestors);

static void pdf_xfa_resolve_prototypes_imp(fz_context* ctx, fz_pool* pool, pdf_xfa* xfa, pdf_xfa_object* node,
                                           pdf_xfa_object** ancestors, int n_ancestors) {
    pdf_xfa_object* proto;
    pdf_xfa_object* child;
    char *use, *usehref;
    pdf_xfa_object* next_ancestors[PDF_XFA_PROTO_MAX_ANCESTORS];
    int next_n;

    if (!node) return;

    use = pdf_xfa_object_get_attr(ctx, node, "use");
    usehref = pdf_xfa_object_get_attr(ctx, node, "usehref");
    proto = pdf_xfa_lookup_prototype(ctx, xfa, node, use, usehref, ancestors, n_ancestors);

    if (proto) {
        if (n_ancestors >= PDF_XFA_PROTO_MAX_ANCESTORS) return;
        next_ancestors[n_ancestors] = proto;
        next_n = n_ancestors + 1;

        pdf_xfa_resolve_prototypes_imp(ctx, pool, xfa, proto, next_ancestors, next_n);
        pdf_xfa_apply_prototype(ctx, pool, xfa, node, proto, ancestors, n_ancestors);
        pdf_xfa_clear_attr(ctx, pool, node, "use");
        pdf_xfa_clear_attr(ctx, pool, node, "usehref");
    }

    for (child = node->first_child; child; child = child->next_sibling)
        pdf_xfa_resolve_prototypes_imp(ctx, pool, xfa, child, ancestors, n_ancestors);
}

static void pdf_xfa_apply_prototype(fz_context* ctx, fz_pool* pool, pdf_xfa* xfa, pdf_xfa_object* node,
                                    pdf_xfa_object* proto, pdf_xfa_object** ancestors, int n_ancestors) {
    pdf_xfa_object *pchild, *match, *clone;

    (void)xfa;
    (void)ancestors;
    (void)n_ancestors;

    if (!node || !proto) return;

    if ((!node->content || !node->content[0]) && proto->content && proto->content[0])
        node->content = fz_pool_strdup(ctx, pool, proto->content);

    pdf_xfa_copy_missing_attrs(ctx, pool, node, proto);

    for (pchild = proto->first_child; pchild; pchild = pchild->next_sibling) {
        if (!pchild->name) continue;

        match = pdf_xfa_find_child_by_name(node, pchild->name);
        if (match) {
            pdf_xfa_apply_prototype(ctx, pool, xfa, match, pchild, ancestors, n_ancestors);
        } else {
            clone = pdf_xfa_object_clone(ctx, pool, pchild);
            if (clone) pdf_xfa_object_append_child(ctx, pool, node, clone);
        }
    }
}

void pdf_xfa_resolve_prototypes(fz_context* ctx, fz_pool* pool, pdf_xfa* xfa, pdf_xfa_object* node) {
    pdf_xfa_object* ancestors[PDF_XFA_PROTO_MAX_ANCESTORS];
    pdf_xfa_resolve_prototypes_imp(ctx, pool, xfa, node, ancestors, 0);
}
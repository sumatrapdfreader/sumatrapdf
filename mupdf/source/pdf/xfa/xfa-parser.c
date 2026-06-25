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
#include "../../fitz/xml-imp.h"

#include <ctype.h>
#include <string.h>

typedef struct {
    fz_pool* pool;
    fz_hash_table* ids;
    pdf_xfa_builder* builder;
    pdf_xfa_global_data* global;
    pdf_xfa_object** stack;
    int stack_len;
    int stack_cap;
    pdf_xfa_object* root;
    int rich_text;
} pdf_xfa_parser;

static void pdf_xfa_parser_push(fz_context* ctx, pdf_xfa_parser* parser, pdf_xfa_object* node) {
    if (parser->stack_len == parser->stack_cap) {
        int newcap = parser->stack_cap ? parser->stack_cap * 2 : 8;
        pdf_xfa_object** newstack = fz_pool_alloc(ctx, parser->pool, sizeof(pdf_xfa_object*) * newcap);
        if (parser->stack) memcpy(newstack, parser->stack, sizeof(pdf_xfa_object*) * parser->stack_len);
        parser->stack = newstack;
        parser->stack_cap = newcap;
    }
    parser->stack[parser->stack_len++] = node;
}

static pdf_xfa_object* pdf_xfa_parser_pop(pdf_xfa_parser* parser) {
    if (parser->stack_len == 0) return NULL;
    return parser->stack[--parser->stack_len];
}

static pdf_xfa_object* pdf_xfa_parser_current(pdf_xfa_parser* parser) {
    if (parser->stack_len == 0) return NULL;
    return parser->stack[parser->stack_len - 1];
}

static int pdf_xfa_is_whitespace(const char* text) {
    while (*text) {
        if (!isspace((unsigned char)*text)) return 0;
        text++;
    }
    return 1;
}

static void pdf_xfa_on_text(fz_context* ctx, pdf_xfa_parser* parser, const char* text) {
    pdf_xfa_object* current = pdf_xfa_parser_current(parser);
    char* merged;
    size_t len;

    if (!current) return;

    if (!parser->rich_text && pdf_xfa_is_whitespace(text)) return;

    len = strlen(text);
    merged = fz_pool_alloc(ctx, parser->pool, len + 1);
    memcpy(merged, text, len + 1);

    if (!parser->rich_text) {
        char* d = merged;
        char* s = merged;
        while (isspace((unsigned char)*s)) s++;
        while (*s) *d++ = *s++;
        *d = 0;
        while (d > merged && isspace((unsigned char)d[-1])) *--d = 0;
    }

    if (!current->content)
        current->content = merged;
    else {
        size_t oldlen = strlen(current->content);
        char* buf = fz_pool_alloc(ctx, parser->pool, oldlen + len + 1);
        memcpy(buf, current->content, oldlen);
        memcpy(buf + oldlen, merged, len + 1);
        current->content = buf;
    }
}

static void pdf_xfa_split_name(const char* tag, int ns_agnostic, char* prefix, char* local) {
    const char* colon = strchr(tag, ':');
    if (!colon || ns_agnostic) {
        prefix[0] = 0;
        fz_strlcpy(local, colon && ns_agnostic ? colon + 1 : tag, 128);
    } else {
        size_t plen = (size_t)(colon - tag);
        if (plen >= 128) plen = 127;
        memcpy(prefix, tag, plen);
        prefix[plen] = 0;
        fz_strlcpy(local, colon + 1, 128);
    }
}

static void pdf_xfa_collect_attrs(fz_context* ctx, fz_pool* pool, fz_xml* item, pdf_xfa_attr** attrs_out,
                                  pdf_xfa_attr** prefixes_out, char** xmlns_out) {
    pdf_xfa_attr* attrs = NULL;
    pdf_xfa_attr* prefixes = NULL;
    pdf_xfa_attr** tail = &attrs;
    pdf_xfa_attr** ptail = &prefixes;
    char* xmlns = NULL;
    struct attribute* att;
    char* colon;
    char prefix_name[128];

    if (!item || !fz_xml_tag(item)) {
        *attrs_out = NULL;
        *prefixes_out = NULL;
        *xmlns_out = NULL;
        return;
    }

    for (att = item->u.node.u.d.atts; att; att = att->next) {
        if (!att->name[0]) continue;
        if (strcmp(att->name, "xmlns") == 0) {
            xmlns = fz_pool_strdup(ctx, pool, att->value ? att->value : "");
            continue;
        }
        colon = strchr(att->name, ':');
        if (colon && strcmp(att->name, "xmlns") != 0 && strncmp(att->name, "xmlns:", 6) == 0) {
            fz_strlcpy(prefix_name, att->name + 6, sizeof prefix_name);
            *ptail = fz_pool_alloc(ctx, pool, sizeof(pdf_xfa_attr));
            (*ptail)->name = fz_pool_strdup(ctx, pool, prefix_name);
            (*ptail)->value = fz_pool_strdup(ctx, pool, att->value ? att->value : "");
            (*ptail)->next = NULL;
            ptail = &(*ptail)->next;
            continue;
        }
        if (colon) continue; /* xfa:attr namespace attrs handled in later phases */
        *tail = fz_pool_alloc(ctx, pool, sizeof(pdf_xfa_attr));
        (*tail)->name = fz_pool_strdup(ctx, pool, att->name);
        (*tail)->value = fz_pool_strdup(ctx, pool, att->value ? att->value : "");
        (*tail)->next = NULL;
        tail = &(*tail)->next;
    }

    *attrs_out = attrs;
    *prefixes_out = prefixes;
    *xmlns_out = xmlns;
}

static pdf_xfa_object* pdf_xfa_resolve_xdp_root(pdf_xfa_object* node) {
    pdf_xfa_object* child;

    if (!node) return NULL;
    if (node->name && strcmp(node->name, "xdp") == 0) return node;
    for (child = node->first_child; child; child = child->next_sibling)
        if (child->name && strcmp(child->name, "xdp") == 0) return child;
    return node;
}

static void pdf_xfa_walk_xml(fz_context* ctx, pdf_xfa_parser* parser, fz_xml* item) {
    char* text = fz_xml_text(item);
    char* tag = fz_xml_tag(item);
    fz_xml* child;

    if (text) {
        pdf_xfa_on_text(ctx, parser, text);
        return;
    }

    if (tag) {
        pdf_xfa_attr* attrs = NULL;
        pdf_xfa_attr* prefixes = NULL;
        char* xmlns = NULL;
        char prefix[128];
        char local[128];
        pdf_xfa_object* node;
        pdf_xfa_object* parent;
        int ns_agnostic;

        pdf_xfa_collect_attrs(ctx, parser->pool, item, &attrs, &prefixes, &xmlns);
        ns_agnostic = pdf_xfa_builder_is_ns_agnostic(ctx, parser->builder);
        pdf_xfa_split_name(tag, ns_agnostic, prefix, local);

        node = pdf_xfa_builder_build(ctx, parser->pool, parser->builder, prefix[0] ? prefix : NULL, local, attrs, xmlns,
                                     prefixes);

        node->global = parser->global;

        parent = pdf_xfa_parser_current(parser);
        if (!parent)
            pdf_xfa_object_append_child(ctx, parser->pool, parser->root, node);
        else
            pdf_xfa_object_append_child(ctx, parser->pool, parent, node);

        pdf_xfa_parser_push(ctx, parser, node);

        for (child = fz_xml_down(item); child; child = fz_xml_next(child)) pdf_xfa_walk_xml(ctx, parser, child);

        pdf_xfa_object_finalize(ctx, parser->pool, node);
        pdf_xfa_object_set_id(ctx, parser->ids, node);
        pdf_xfa_parser_pop(parser);
        pdf_xfa_builder_clean(ctx, parser->builder, xmlns != NULL, prefixes, 0);
        return;
    }

    for (child = fz_xml_down(item); child; child = fz_xml_next(child)) pdf_xfa_walk_xml(ctx, parser, child);
}

pdf_xfa_object* pdf_xfa_parse_xml(fz_context* ctx, fz_pool* pool, fz_buffer* buf, pdf_xfa_ns_id root_ns,
                                  int rich_text) {
    fz_xml* xml = NULL;
    pdf_xfa_parser parser;
    pdf_xfa_object* result = NULL;

    fz_var(xml);

    memset(&parser, 0, sizeof parser);
    parser.pool = pool;
    parser.rich_text = rich_text;
    parser.builder = pdf_xfa_builder_new(ctx, pool);
    parser.ids = fz_new_hash_table(ctx, 256, FZ_HASH_TABLE_KEY_LENGTH, -1, NULL);
    parser.global = fz_pool_alloc(ctx, pool, sizeof(pdf_xfa_global_data));
    memset(parser.global, 0, sizeof(*parser.global));
    parser.global->used_typefaces = fz_new_hash_table(ctx, 32, FZ_HASH_TABLE_KEY_LENGTH, -1, NULL);
    parser.root = pdf_xfa_builder_build_root(ctx, pool, parser.ids);

    fz_try(ctx) {
        xml = fz_parse_xml(ctx, buf, rich_text);
        if (!xml) fz_throw(ctx, FZ_ERROR_FORMAT, "cannot parse XFA xml");

        if (root_ns != PDF_XFA_NS_UNKNOWN) parser.builder->current_ns = root_ns;

        pdf_xfa_walk_xml(ctx, &parser, xml);

        result = pdf_xfa_resolve_xdp_root(parser.root->first_child);
    }
    fz_always(ctx) {
        fz_drop_xml(ctx, xml);
        pdf_xfa_builder_drop(ctx, parser.builder);
        fz_drop_hash_table(ctx, parser.ids);
    }
    fz_catch(ctx) fz_rethrow(ctx);

    return result;
}

pdf_xfa_object* pdf_xfa_parse_packets(fz_context* ctx, fz_pool* pool, pdf_xfa_packet* packets) {
    fz_buffer* xdp = pdf_xfa_packets_to_xdp(ctx, packets);
    pdf_xfa_object* root = NULL;

    fz_try(ctx) root = pdf_xfa_parse_xml(ctx, pool, xdp, PDF_XFA_NS_XDP, 0);
    fz_always(ctx) fz_drop_buffer(ctx, xdp);
    fz_catch(ctx) fz_rethrow(ctx);

    return root;
}
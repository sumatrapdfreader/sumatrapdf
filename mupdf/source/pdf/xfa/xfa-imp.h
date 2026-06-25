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

#ifndef PDF_XFA_IMP_H
#define PDF_XFA_IMP_H

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

/*
        Internal XFA engine types.  Module layout mirrors pdf.js src/core/xfa/:

        namespaces.c  -> namespaces.js + setup.js
        object.c      -> xfa_object.js
        builder.c     -> builder.js
        parser.c      -> parser.js (+ xml_parser.js)
        factory.c     -> factory.js (+ bind.js, data.js, layout.js, template.js, …)
*/

typedef enum {
    PDF_XFA_NS_CONFIG = 0,
    PDF_XFA_NS_CONNECTION_SET,
    PDF_XFA_NS_DATASETS,
    PDF_XFA_NS_FORM,
    PDF_XFA_NS_LOCALE_SET,
    PDF_XFA_NS_PDF,
    PDF_XFA_NS_SIGNATURE,
    PDF_XFA_NS_SOURCE_SET,
    PDF_XFA_NS_STYLESHEET,
    PDF_XFA_NS_TEMPLATE,
    PDF_XFA_NS_XDC,
    PDF_XFA_NS_XDP,
    PDF_XFA_NS_XFDF,
    PDF_XFA_NS_XHTML,
    PDF_XFA_NS_XMPMETA,
    PDF_XFA_NS_UNKNOWN,
    PDF_XFA_NS_COUNT
} pdf_xfa_ns_id;

typedef struct pdf_xfa_attr pdf_xfa_attr;
typedef struct pdf_xfa_object pdf_xfa_object;
typedef struct pdf_xfa_global_data pdf_xfa_global_data;
typedef struct pdf_xfa_builder pdf_xfa_builder;
typedef struct pdf_xfa_packet pdf_xfa_packet;

struct pdf_xfa_attr {
    char* name;
    char* value;
    pdf_xfa_attr* next;
};

struct pdf_xfa_object {
    pdf_xfa_ns_id ns;
    char* name;
    char* content;
    char* uid;
    char* template_id;
    pdf_xfa_object* parent;
    pdf_xfa_object* first_child;
    pdf_xfa_object* next_sibling;
    pdf_xfa_attr* first_attr;
    pdf_xfa_global_data* global;
    pdf_xfa_object* data_node; /* bound datasets node (bind.js: $data) */
    int is_data_value;         /* -1 unknown, 0 group, 1 value */
    int has_children;
    int consumed;
    int flow_page; /* -1: not flowed; else target page index */
    float flow_x;
    float flow_y;
};

struct pdf_xfa_global_data {
    fz_hash_table* used_typefaces;
    pdf_xfa_object* template_root;
    /* future: images, font_finder, para_stack, … */
};

struct pdf_xfa_packet {
    char* name;
    fz_buffer* data;
    pdf_xfa_packet* next;
};

typedef struct pdf_xfa_fonts pdf_xfa_fonts;

struct pdf_xfa {
    int refs;
    pdf_document* doc;
    fz_pool* pool;
    fz_hash_table* ids;
    pdf_xfa_global_data global;
    pdf_xfa_fonts* fonts;
    pdf_xfa_object* root; /* parsed xdp */
    pdf_xfa_object* template_node;
    pdf_xfa_object* datasets_node;
    pdf_xfa_object* data_node;
    pdf_xfa_object* form; /* bound template clone */
    pdf_xfa_packet* packets;
    int valid;
    int layout_done;
    int page_count;
    fz_rect* page_bboxes;
    pdf_xfa_object** page_areas; /* parallel to page_bboxes */
    pdf_xfa_object** page_subforms; /* Page1/Page2 content subforms (IRS-style forms) */
    int page_subform_count;
    pdf_xfa_html_node* pages;    /* laid-out page tree roots */
    int render_fields;
    int render_draws;
    int render_borders;
    int render_lines;
    int fields_in_pageset;
    int fields_outside_pageset;
    int fields_with_pagearea;
    int fields_with_pagearea_template;
};

struct pdf_xfa_builder {
    pdf_xfa_ns_id* namespace_stack;
    int namespace_stack_len;
    int namespace_stack_cap;
    pdf_xfa_ns_id current_ns;
    int next_unknown_ns;
    int ns_agnostic_level;
    /* prefix -> stack of namespaces */
    fz_hash_table* prefix_stacks;
    fz_hash_table* uri_namespaces;
};

/* namespaces.c */
pdf_xfa_ns_id pdf_xfa_ns_from_uri(fz_context* ctx, pdf_xfa_builder* builder, const char* uri);
const char* pdf_xfa_ns_uri(pdf_xfa_ns_id ns);
int pdf_xfa_ns_check_uri(pdf_xfa_ns_id ns, const char* uri);

/* object.c */
pdf_xfa_object* pdf_xfa_new_object(fz_context* ctx, fz_pool* pool, pdf_xfa_ns_id ns, const char* name,
                                   int has_children);
void pdf_xfa_object_append_child(fz_context* ctx, fz_pool* pool, pdf_xfa_object* parent, pdf_xfa_object* child);
void pdf_xfa_object_add_attr(fz_context* ctx, fz_pool* pool, pdf_xfa_object* node, const char* name, const char* value);
void pdf_xfa_ids_make_key(char* buf, const char* id);
void pdf_xfa_object_set_id(fz_context* ctx, fz_hash_table* ids, pdf_xfa_object* node);
void pdf_xfa_object_finalize(fz_context* ctx, fz_pool* pool, pdf_xfa_object* node);
char* pdf_xfa_object_get_attr(fz_context* ctx, pdf_xfa_object* node, const char* name);
int pdf_xfa_object_is_prototype_def(fz_context* ctx, pdf_xfa_object* node);
int pdf_xfa_object_is_data_value(pdf_xfa_object* node);
const char* pdf_xfa_object_get_data_value(fz_context* ctx, pdf_xfa_object* node);
const char* pdf_xfa_object_text(fz_context* ctx, pdf_xfa_object* node);
pdf_xfa_object* pdf_xfa_object_find_child(pdf_xfa_object* node, const char* name);
pdf_xfa_object* pdf_xfa_object_clone(fz_context* ctx, fz_pool* pool, pdf_xfa_object* node);

/* som.c */
int pdf_xfa_som_search(fz_context* ctx, pdf_xfa_object* root, pdf_xfa_object* container, const char* expr,
                       int dot_dot_allowed, pdf_xfa_object** out, int out_max);

/* bind.c */
pdf_xfa_object* pdf_xfa_bind(fz_context* ctx, fz_pool* pool, pdf_xfa* xfa);

/* prototype.c */
void pdf_xfa_resolve_prototypes(fz_context* ctx, fz_pool* pool, pdf_xfa* xfa, pdf_xfa_object* node);

/* builder.c */
pdf_xfa_builder* pdf_xfa_builder_new(fz_context* ctx, fz_pool* pool);
void pdf_xfa_builder_drop(fz_context* ctx, pdf_xfa_builder* builder);
pdf_xfa_object* pdf_xfa_builder_build_root(fz_context* ctx, fz_pool* pool, fz_hash_table* ids);
pdf_xfa_object* pdf_xfa_builder_build(fz_context* ctx, fz_pool* pool, pdf_xfa_builder* builder, const char* ns_prefix,
                                      const char* name, pdf_xfa_attr* attrs, const char* xmlns,
                                      pdf_xfa_attr* xmlns_prefixes);
void pdf_xfa_builder_clean(fz_context* ctx, pdf_xfa_builder* builder, int has_namespace, pdf_xfa_attr* prefixes,
                           int ns_agnostic);
int pdf_xfa_builder_is_ns_agnostic(fz_context* ctx, pdf_xfa_builder* builder);

/* parser.c */
pdf_xfa_object* pdf_xfa_parse_xml(fz_context* ctx, fz_pool* pool, fz_buffer* buf, pdf_xfa_ns_id root_ns, int rich_text,
                                  fz_hash_table* ids, pdf_xfa_global_data* global);
pdf_xfa_object* pdf_xfa_parse_packets(fz_context* ctx, fz_pool* pool, pdf_xfa_packet* packets, fz_hash_table* ids,
                                      pdf_xfa_global_data* global);

/* layout.c */
float pdf_xfa_parse_measurement(const char* text, float default_pt);
void pdf_xfa_count_field_stats(pdf_xfa_object* root, int* in_pageset, int* outside_pageset, int* with_pagearea);
int pdf_xfa_page_subform_index(fz_context* ctx, pdf_xfa_object* subform);

/* factory.c */
pdf_xfa* pdf_xfa_new_from_packets(fz_context* ctx, pdf_document* doc, pdf_xfa_packet* packets);
int pdf_xfa_factory_layout(fz_context* ctx, pdf_xfa* xfa);

/* fonts.c */
void pdf_xfa_fonts_register_typeface(fz_context* ctx, pdf_xfa_global_data* global, const char* typeface);
pdf_xfa_fonts* pdf_xfa_fonts_load(fz_context* ctx, pdf_document* doc, fz_pool* pool, pdf_xfa_packet* packets);
void pdf_xfa_fonts_drop(fz_context* ctx, pdf_xfa_fonts* fonts);
const char* pdf_xfa_fonts_default_typeface(pdf_xfa_fonts* fonts);
void pdf_xfa_fonts_check_used(fz_context* ctx, pdf_xfa_fonts* fonts, pdf_xfa_object* form);
fz_font* pdf_xfa_fonts_resolve(fz_context* ctx, pdf_xfa_fonts* fonts, const char* typeface, int bold, int italic);

/* render.c */
fz_display_list* pdf_xfa_factory_render_page(fz_context* ctx, pdf_xfa* xfa, int page_index, fz_matrix ctm);

/* data.c */
void pdf_xfa_sync_form_to_data(fz_context* ctx, fz_pool* pool, pdf_xfa* xfa);
fz_buffer* pdf_xfa_factory_serialize_data(fz_context* ctx, pdf_xfa* xfa);

/* pdf-xfa.c helpers used by factory */
pdf_xfa_packet* pdf_xfa_load_packets(fz_context* ctx, pdf_document* doc);
void pdf_xfa_drop_packets(fz_context* ctx, pdf_xfa_packet* packets);
fz_buffer* pdf_xfa_packets_to_xdp(fz_context* ctx, pdf_xfa_packet* packets);

#endif
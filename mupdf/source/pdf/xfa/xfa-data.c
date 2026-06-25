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

#define PDF_XFA_DATASETS_NS "http://www.xfa.org/schema/xfa-data/1.0/"

static const char* pdf_xfa_form_value(fz_context* ctx, pdf_xfa_object* form) {
    pdf_xfa_object* value;

    if (!form) return "";

    if (form->content && form->content[0]) return form->content;

    value = pdf_xfa_object_find_child(form, "value");
    if (value) {
        const char* text = pdf_xfa_object_text(ctx, value);
        if (text && text[0]) return text;
    }

    return "";
}

static void pdf_xfa_set_data_value(fz_context* ctx, fz_pool* pool, pdf_xfa_object* data, const char* value) {
    pdf_xfa_object* child;
    const char* v = value ? value : "";

    if (!data) return;

    if (!data->first_child) {
        data->content = fz_pool_strdup(ctx, pool, v);
        return;
    }

    child = data->first_child;
    if (child->name && strcmp(child->name, "#text") == 0 && !child->next_sibling) {
        child->content = fz_pool_strdup(ctx, pool, v);
        return;
    }

    data->first_child = NULL;
    data->content = fz_pool_strdup(ctx, pool, v);
    data->is_data_value = 1;
}

static void pdf_xfa_sync_form_to_data_imp(fz_context* ctx, fz_pool* pool, pdf_xfa_object* form) {
    pdf_xfa_object* child;
    const char* value;

    if (!form) return;

    if (form->data_node && pdf_xfa_object_is_data_value(form->data_node)) {
        value = pdf_xfa_form_value(ctx, form);
        pdf_xfa_set_data_value(ctx, pool, form->data_node, value);
    }

    for (child = form->first_child; child; child = child->next_sibling) pdf_xfa_sync_form_to_data_imp(ctx, pool, child);
}

void pdf_xfa_sync_form_to_data(fz_context* ctx, fz_pool* pool, pdf_xfa* xfa) {
    if (!xfa || !xfa->form) return;
    pdf_xfa_sync_form_to_data_imp(ctx, pool, xfa->form);
}

static void pdf_xfa_append_xml_escaped(fz_context* ctx, fz_buffer* buf, const char* text) {
    const char* p;

    if (!text) return;

    for (p = text; *p; p++) {
        switch (*p) {
            case '&':
                fz_append_string(ctx, buf, "&amp;");
                break;
            case '<':
                fz_append_string(ctx, buf, "&lt;");
                break;
            case '>':
                fz_append_string(ctx, buf, "&gt;");
                break;
            case '"':
                fz_append_string(ctx, buf, "&quot;");
                break;
            default:
                fz_append_byte(ctx, buf, *p);
                break;
        }
    }
}

static void pdf_xfa_serialize_node(fz_context* ctx, fz_buffer* buf, pdf_xfa_object* node) {
    pdf_xfa_object* child;
    pdf_xfa_attr* attr;

    if (!node || !node->name) return;

    if (strcmp(node->name, "#text") == 0) {
        pdf_xfa_append_xml_escaped(ctx, buf, node->content);
        return;
    }

    fz_append_byte(ctx, buf, '<');
    fz_append_string(ctx, buf, node->name);

    for (attr = node->first_attr; attr; attr = attr->next) {
        fz_append_byte(ctx, buf, ' ');
        fz_append_string(ctx, buf, attr->name);
        fz_append_string(ctx, buf, "=\"");
        pdf_xfa_append_xml_escaped(ctx, buf, attr->value);
        fz_append_byte(ctx, buf, '"');
    }

    if (!node->first_child && (!node->content || !node->content[0])) {
        fz_append_string(ctx, buf, "/>");
        return;
    }

    fz_append_byte(ctx, buf, '>');

    if (node->content && node->content[0]) pdf_xfa_append_xml_escaped(ctx, buf, node->content);

    for (child = node->first_child; child; child = child->next_sibling) pdf_xfa_serialize_node(ctx, buf, child);

    fz_append_string(ctx, buf, "</");
    fz_append_string(ctx, buf, node->name);
    fz_append_byte(ctx, buf, '>');
}

fz_buffer* pdf_xfa_factory_serialize_data(fz_context* ctx, pdf_xfa* xfa) {
    fz_buffer* buf = NULL;

    if (!xfa || !xfa->valid || !xfa->data_node) return fz_new_buffer(ctx, 0);

    fz_var(buf);
    fz_try(ctx) {
        pdf_xfa_sync_form_to_data(ctx, xfa->pool, xfa);

        buf = fz_new_buffer(ctx, 256);
        fz_append_string(ctx, buf, "<datasets xmlns=\"");
        fz_append_string(ctx, buf, PDF_XFA_DATASETS_NS);
        fz_append_string(ctx, buf, "\">");
        pdf_xfa_serialize_node(ctx, buf, xfa->data_node);
        fz_append_string(ctx, buf, "</datasets>");
    }
    fz_catch(ctx) {
        fz_drop_buffer(ctx, buf);
        fz_rethrow(ctx);
    }

    return buf;
}
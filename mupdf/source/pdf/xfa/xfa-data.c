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

static pdf_xfa_object* pdf_xfa_find_named_field(pdf_xfa_object* node, const char* field_name) {
    pdf_xfa_object* child;
    char* name;
    pdf_xfa_object* hit;

    if (!node || !field_name || !field_name[0]) return NULL;

    if (node->name && strcmp(node->name, "field") == 0) {
        name = pdf_xfa_object_get_attr(NULL, node, "name");
        if (name && strcmp(name, field_name) == 0) return node;
    }

    for (child = node->first_child; child; child = child->next_sibling) {
        hit = pdf_xfa_find_named_field(child, field_name);
        if (hit) return hit;
    }
    return NULL;
}

static pdf_xfa_object* pdf_xfa_find_ui_widget(pdf_xfa_object* field, const char* widget_name) {
    pdf_xfa_object* ui;

    ui = pdf_xfa_object_find_child(field, "ui");
    if (!ui) return NULL;
    return pdf_xfa_object_find_child(ui, widget_name);
}

int pdf_xfa_factory_set_field_content(fz_context* ctx, pdf_xfa* xfa, const char* field_name, const char* value) {
    pdf_xfa_object* field;

    if (!xfa || !xfa->form || !field_name || !field_name[0]) return 0;

    field = pdf_xfa_find_named_field(xfa->form, field_name);
    if (!field) return 0;

    field->content = fz_pool_strdup(ctx, xfa->pool, value ? value : "");
    return 1;
}

int pdf_xfa_factory_get_field_kind(fz_context* ctx, pdf_xfa* xfa, const char* field_name) {
    pdf_xfa_object* field;

    (void)ctx;
    if (!xfa || !xfa->form || !field_name || !field_name[0]) return PDF_XFA_FIELD_UNKNOWN;

    field = pdf_xfa_find_named_field(xfa->form, field_name);
    if (!field) return PDF_XFA_FIELD_UNKNOWN;
    if (pdf_xfa_find_ui_widget(field, "checkButton")) return PDF_XFA_FIELD_CHECKBOX;
    if (pdf_xfa_find_ui_widget(field, "radioButton")) return PDF_XFA_FIELD_RADIO;
    return PDF_XFA_FIELD_TEXT;
}

int pdf_xfa_factory_get_field_content(fz_context* ctx, pdf_xfa* xfa, const char* field_name, char* buf, int buflen) {
    pdf_xfa_object* field;
    const char* value;
    size_t len;

    if (!buf || buflen <= 0) return 0;
    buf[0] = 0;
    if (!xfa || !xfa->form || !field_name || !field_name[0]) return 0;

    field = pdf_xfa_find_named_field(xfa->form, field_name);
    if (!field) return 0;

    value = pdf_xfa_form_value(ctx, field);
    if (!value) value = "";
    len = strlen(value);
    if ((int)len >= buflen) len = (size_t)buflen - 1;
    memcpy(buf, value, len);
    buf[len] = 0;
    return 1;
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

static int pdf_xfa_packet_is_datasets(const char* name) {
    size_t n;

    if (!name) return 0;
    if (strcmp(name, "datasets") == 0) return 1;
    n = strlen(name);
    if (n > 9 && strcmp(name + n - 8, "datasets") == 0 && name[n - 9] == ':') return 1;
    return 0;
}

static int pdf_xfa_is_element_start(const char* p, const char* local_name) {
    size_t n = strlen(local_name);
    char c;

    if (p[0] != '<') return 0;
    if (strncmp(p + 1, local_name, n) == 0) {
        c = p[1 + n];
        return c == '>' || c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '/';
    }
    {
        const char* colon = strchr(p + 1, ':');
        if (colon && strncmp(colon + 1, local_name, n) == 0) {
            c = colon[1 + n];
            return c == '>' || c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '/';
        }
    }
    return 0;
}

static const char* pdf_xfa_find_element_close(const char* start, const char* local_name) {
    char close[64];
    const char* end;
    const char* p;

    snprintf(close, sizeof close, "</%s>", local_name);
    end = strstr(start, close);
    if (end) return end + strlen(close);

    for (p = start; (p = strstr(p, "</")) != NULL; p += 2) {
        const char* q = p + 2;
        const char* colon = strchr(q, ':');
        const char* name = colon ? colon + 1 : q;
        if (strncmp(name, local_name, strlen(local_name)) == 0 && name[strlen(local_name)] == '>') return name + strlen(local_name) + 1;
    }
    return NULL;
}

static int pdf_xfa_find_element_span(const char* text, size_t len, const char* local_name, const char** out_start,
                                     const char** out_end) {
    const char* start = NULL;
    const char* end = NULL;
    const char* p;

    if (!text || len == 0) return 0;

    for (p = text; p + strlen(local_name) + 1 < text + len; p++) {
        if (pdf_xfa_is_element_start(p, local_name)) {
            start = p;
            break;
        }
    }
    if (!start) return 0;

    end = pdf_xfa_find_element_close(start, local_name);
    if (!end) return 0;

    *out_start = start;
    *out_end = end;
    return 1;
}

static fz_buffer* pdf_xfa_serialize_data_node_xml(fz_context* ctx, pdf_xfa* xfa) {
    fz_buffer* buf = NULL;

    if (!xfa || !xfa->valid || !xfa->data_node) return fz_new_buffer(ctx, 0);

    fz_var(buf);
    fz_try(ctx) {
        pdf_xfa_sync_form_to_data(ctx, xfa->pool, xfa);
        buf = fz_new_buffer(ctx, 256);
        pdf_xfa_serialize_node(ctx, buf, xfa->data_node);
    }
    fz_catch(ctx) {
        fz_drop_buffer(ctx, buf);
        fz_rethrow(ctx);
    }

    return buf;
}

static fz_buffer* pdf_xfa_replace_element_in_buffer(fz_context* ctx, fz_buffer* orig, const char* local_name,
                                                    fz_buffer* replacement) {
    unsigned char* orig_data;
    size_t orig_len;
    const char* start;
    const char* end;
    const char* o;
    fz_buffer* out = NULL;

    if (!replacement) return NULL;
    if (!orig) return fz_keep_buffer(ctx, replacement);

    orig_len = fz_buffer_storage(ctx, orig, &orig_data);
    o = (const char*)orig_data;
    if (!pdf_xfa_find_element_span(o, orig_len, local_name, &start, &end)) return NULL;

    fz_var(out);
    fz_try(ctx) {
        out = fz_new_buffer(ctx, orig_len + fz_buffer_storage(ctx, replacement, NULL));
        fz_append_data(ctx, out, orig_data, (size_t)(start - o));
        fz_append_buffer(ctx, out, replacement);
        fz_append_data(ctx, out, end, orig_len - (size_t)(end - o));
    }
    fz_catch(ctx) {
        fz_drop_buffer(ctx, out);
        fz_rethrow(ctx);
    }

    return out;
}

static fz_buffer* pdf_xfa_replace_datasets_in_buffer(fz_context* ctx, pdf_xfa* xfa, fz_buffer* orig,
                                                    fz_buffer* datasets_xml) {
    fz_buffer* data_xml = NULL;
    fz_buffer* out = NULL;

    if (!datasets_xml) return NULL;

    fz_var(data_xml);
    fz_var(out);
    fz_try(ctx) {
        out = pdf_xfa_replace_element_in_buffer(ctx, orig, "datasets", datasets_xml);
        if (!out && xfa) {
            data_xml = pdf_xfa_serialize_data_node_xml(ctx, xfa);
            out = pdf_xfa_replace_element_in_buffer(ctx, orig, "data", data_xml);
        }
        if (!out) out = fz_keep_buffer(ctx, datasets_xml);
    }
    fz_always(ctx) fz_drop_buffer(ctx, data_xml);
    fz_catch(ctx) {
        fz_drop_buffer(ctx, out);
        fz_rethrow(ctx);
    }

    return out;
}

static pdf_xfa_packet* pdf_xfa_find_packet(pdf_xfa* xfa, const char* name) {
    pdf_xfa_packet* p;

    if (!xfa || !name) return NULL;
    for (p = xfa->packets; p; p = p->next) {
        if (p->name && strcmp(p->name, name) == 0) return p;
    }
    return NULL;
}

static int pdf_xfa_update_packet_data(fz_context* ctx, pdf_xfa_packet* packet, fz_buffer* buf) {
    if (!packet || !buf) return 0;
    fz_drop_buffer(ctx, packet->data);
    packet->data = fz_keep_buffer(ctx, buf);
    return 1;
}

static int pdf_xfa_write_datasets_stream(fz_context* ctx, pdf_document* doc, pdf_obj* stm, pdf_xfa* xfa,
                                         const char* packet_name, fz_buffer* datasets_xml) {
    pdf_xfa_packet* packet;
    fz_buffer* packet_buf = NULL;
    int ok = 0;

    fz_var(packet_buf);
    fz_try(ctx) {
        packet = pdf_xfa_find_packet(xfa, packet_name);
        if (packet && packet->data)
            packet_buf = pdf_xfa_replace_datasets_in_buffer(ctx, xfa, packet->data, datasets_xml);
        else
            packet_buf = fz_keep_buffer(ctx, datasets_xml);

        pdf_update_stream(ctx, doc, stm, packet_buf, 0);
        if (packet) pdf_xfa_update_packet_data(ctx, packet, packet_buf);
        ok = 1;
    }
    fz_always(ctx) fz_drop_buffer(ctx, packet_buf);
    fz_catch(ctx) fz_rethrow(ctx);

    return ok;
}

int pdf_xfa_factory_write_datasets(fz_context* ctx, pdf_xfa* xfa) {
    pdf_document* doc;
    pdf_obj* xfa_obj;
    fz_buffer* datasets_xml = NULL;
    int wrote = 0;

    if (!xfa || !xfa->valid || !xfa->doc) return 0;
    doc = xfa->doc;

    fz_var(datasets_xml);
    fz_try(ctx) {
        datasets_xml = pdf_xfa_factory_serialize_data(ctx, xfa);
        if (!datasets_xml || fz_buffer_storage(ctx, datasets_xml, NULL) == 0)
            fz_throw(ctx, FZ_ERROR_GENERIC, "XFA: empty datasets");

        xfa_obj = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm/XFA");

        if (pdf_is_array(ctx, xfa_obj)) {
            int n = pdf_array_len(ctx, xfa_obj);
            int i;
            for (i = 0; i + 1 < n; i += 2) {
                pdf_obj* nameobj = pdf_array_get(ctx, xfa_obj, i);
                pdf_obj* stm;
                char* name;

                if (!pdf_is_string(ctx, nameobj)) continue;
                name = pdf_load_stream_or_string_as_utf8(ctx, nameobj);
                if (!pdf_xfa_packet_is_datasets(name)) {
                    fz_free(ctx, name);
                    continue;
                }
                stm = pdf_array_get(ctx, xfa_obj, i + 1);
                if (!pdf_is_stream(ctx, stm)) {
                    fz_free(ctx, name);
                    continue;
                }
                if (pdf_xfa_write_datasets_stream(ctx, doc, stm, xfa, name, datasets_xml)) wrote = 1;
                fz_free(ctx, name);
                break;
            }
        } else if (pdf_is_stream(ctx, xfa_obj)) {
            pdf_xfa_packet* p;
            const char* packet_name = "xdp:xdp";

            for (p = xfa->packets; p; p = p->next) {
                if (p->name && p->data) {
                    packet_name = p->name;
                    break;
                }
            }
            if (pdf_xfa_write_datasets_stream(ctx, doc, xfa_obj, xfa, packet_name, datasets_xml)) wrote = 1;
        }
    }
    fz_always(ctx) fz_drop_buffer(ctx, datasets_xml);
    fz_catch(ctx) fz_rethrow(ctx);

    return wrote;
}
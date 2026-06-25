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

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include "xfa/xfa-imp.h"

#include <string.h>

static char pdf_xfa_load_error[256];

const char* pdf_xfa_last_load_error(fz_context* ctx) {
    (void)ctx;
    return pdf_xfa_load_error;
}

int pdf_document_has_xfa(fz_context* ctx, pdf_document* doc) {
    pdf_obj* xfa = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm/XFA");
    return pdf_is_array(ctx, xfa) || pdf_is_stream(ctx, xfa);
}

int pdf_document_is_pure_xfa(fz_context* ctx, pdf_document* doc) {
    return pdf_was_pure_xfa(ctx, doc);
}

void pdf_xfa_drop_packets(fz_context* ctx, pdf_xfa_packet* packets) {
    while (packets) {
        pdf_xfa_packet* next = packets->next;
        fz_free(ctx, packets->name);
        fz_drop_buffer(ctx, packets->data);
        fz_free(ctx, packets);
        packets = next;
    }
}

pdf_xfa_packet* pdf_xfa_load_packets(fz_context* ctx, pdf_document* doc) {
    pdf_obj* xfa;
    pdf_xfa_packet* head = NULL;
    pdf_xfa_packet** tail = &head;
    pdf_xfa_packet* last_named = NULL;
    int i, n;

    xfa = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm/XFA");
    if (!pdf_is_array(ctx, xfa) && !pdf_is_stream(ctx, xfa)) return NULL;

    fz_try(ctx) {
        if (pdf_is_stream(ctx, xfa)) {
            pdf_xfa_packet* packet = fz_malloc_struct(ctx, pdf_xfa_packet);
            packet->name = fz_strdup(ctx, "xdp:xdp");
            packet->data = pdf_load_stream(ctx, xfa);
            packet->next = NULL;
            *tail = packet;
        } else {
            n = pdf_array_len(ctx, xfa);
            for (i = 0; i < n; i++) {
                pdf_obj* item = pdf_array_get(ctx, xfa, i);
                if (pdf_is_string(ctx, item)) {
                    pdf_xfa_packet* packet = fz_malloc_struct(ctx, pdf_xfa_packet);
                    packet->name = pdf_load_stream_or_string_as_utf8(ctx, item);
                    packet->data = NULL;
                    packet->next = NULL;
                    *tail = packet;
                    tail = &packet->next;
                    last_named = packet;
                } else if (pdf_is_stream(ctx, item) && last_named && !last_named->data) {
                    last_named->data = pdf_load_stream(ctx, item);
                }
            }
        }
    }
    fz_catch(ctx) {
        pdf_xfa_drop_packets(ctx, head);
        fz_rethrow(ctx);
    }

    return head;
}

static int pdf_xfa_skip_packet(const char* name) {
    /* Metadata packets are not used for bind/layout; xmpmeta may contain tag names
     * with characters our XML parser rejects (see ad-hoc-f1040.pdf). */
    return name && (strcmp(name, "xmpmeta") == 0 || strcmp(name, "xfdf") == 0);
}

fz_buffer* pdf_xfa_packets_to_xdp(fz_context* ctx, pdf_xfa_packet* packets) {
    fz_buffer* buf = fz_new_buffer(ctx, 1024);
    pdf_xfa_packet* p;

    /* Concatenate packet streams in array order (pdf.js XFAFactory._createDocument). */
    for (p = packets; p; p = p->next) {
        if (p->data && !pdf_xfa_skip_packet(p->name)) fz_append_buffer(ctx, buf, p->data);
    }

    return buf;
}

pdf_xfa* pdf_load_xfa(fz_context* ctx, pdf_document* doc) {
    if (doc->xfa_ctx) return doc->xfa_ctx;

    if (!pdf_document_has_xfa(ctx, doc)) return NULL;

    pdf_xfa_load_error[0] = 0;

    fz_try(ctx) {
        pdf_xfa_packet* packets = pdf_xfa_load_packets(ctx, doc);
        if (packets) {
            doc->xfa_ctx = pdf_xfa_new_from_packets(ctx, doc, packets);
            if (!doc->xfa_ctx || !doc->xfa_ctx->valid) {
                pdf_drop_xfa(ctx, doc->xfa_ctx);
                doc->xfa_ctx = NULL;
                fz_strlcpy(pdf_xfa_load_error, "XFA: bind failed", sizeof pdf_xfa_load_error);
            }
        }
    }
    fz_catch(ctx) {
        fz_strlcpy(pdf_xfa_load_error, fz_caught_message(ctx), sizeof pdf_xfa_load_error);
        fz_warn(ctx, "XFA: parse/bind failed: %s", pdf_xfa_load_error);
        doc->xfa_ctx = NULL;
    }

    return doc->xfa_ctx;
}

int pdf_xfa_is_valid(fz_context* ctx, pdf_xfa* xfa) {
    (void)ctx;
    return xfa && xfa->valid;
}

int pdf_xfa_layout(fz_context* ctx, pdf_xfa* xfa) {
    return pdf_xfa_factory_layout(ctx, xfa);
}

int pdf_xfa_page_count(fz_context* ctx, pdf_xfa* xfa) {
    if (!xfa) return 0;
    if (!xfa->layout_done) pdf_xfa_layout(ctx, xfa);
    return xfa->page_count;
}

fz_rect pdf_xfa_page_bbox(fz_context* ctx, pdf_xfa* xfa, int page_index) {
    fz_rect empty = fz_empty_rect;
    if (!xfa || page_index < 0 || page_index >= pdf_xfa_page_count(ctx, xfa)) return empty;
    return xfa->page_bboxes[page_index];
}

fz_display_list* pdf_xfa_run_page(fz_context* ctx, pdf_xfa* xfa, int page_index, fz_matrix ctm) {
    if (!xfa || !xfa->valid) return NULL;
    return pdf_xfa_factory_render_page(ctx, xfa, page_index, ctm);
}

int pdf_xfa_last_render_fields(fz_context* ctx, pdf_xfa* xfa) {
    (void)ctx;
    return xfa ? xfa->render_fields : 0;
}

int pdf_xfa_last_render_draws(fz_context* ctx, pdf_xfa* xfa) {
    (void)ctx;
    return xfa ? xfa->render_draws : 0;
}

int pdf_xfa_last_render_borders(fz_context* ctx, pdf_xfa* xfa) {
    (void)ctx;
    return xfa ? xfa->render_borders : 0;
}

int pdf_xfa_last_render_lines(fz_context* ctx, pdf_xfa* xfa) {
    (void)ctx;
    return xfa ? xfa->render_lines : 0;
}

int pdf_xfa_fields_in_pageset(fz_context* ctx, pdf_xfa* xfa) {
    if (!xfa) return 0;
    pdf_xfa_layout(ctx, xfa);
    return xfa->fields_in_pageset;
}

int pdf_xfa_fields_outside_pageset(fz_context* ctx, pdf_xfa* xfa) {
    if (!xfa) return 0;
    pdf_xfa_layout(ctx, xfa);
    return xfa->fields_outside_pageset;
}

int pdf_xfa_fields_with_pagearea(fz_context* ctx, pdf_xfa* xfa) {
    if (!xfa) return 0;
    pdf_xfa_layout(ctx, xfa);
    return xfa->fields_with_pagearea;
}

int pdf_xfa_fields_with_pagearea_template(fz_context* ctx, pdf_xfa* xfa) {
    (void)ctx;
    return xfa ? xfa->fields_with_pagearea_template : 0;
}

const char* pdf_xfa_page_area_name(fz_context* ctx, pdf_xfa* xfa, int page_index) {
    char* name;

    if (!xfa || page_index < 0 || page_index >= pdf_xfa_page_count(ctx, xfa)) return "";
    if (!xfa->page_areas) return "";
    name = pdf_xfa_object_get_attr(ctx, xfa->page_areas[page_index], "name");
    return name ? name : "";
}

void pdf_xfa_font_stats(fz_context* ctx, pdf_xfa* xfa, int* families_out, int* held_out) {
    if (!xfa || !xfa->fonts) {
        if (families_out) *families_out = 0;
        if (held_out) *held_out = 0;
        return;
    }
    pdf_xfa_fonts_stats(ctx, xfa->fonts, families_out, held_out);
}

fz_buffer* pdf_xfa_serialize_data(fz_context* ctx, pdf_xfa* xfa) {
    if (!xfa || !xfa->valid) return fz_new_buffer(ctx, 0);
    return pdf_xfa_factory_serialize_data(ctx, xfa);
}
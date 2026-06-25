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

static void pdf_xfa_drop_imp(fz_context* ctx, pdf_xfa* xfa);

static void pdf_xfa_ids_drop(fz_context* ctx, void* val) {
    (void)ctx;
    (void)val;
}

pdf_xfa* pdf_xfa_new_from_packets(fz_context* ctx, pdf_document* doc, pdf_xfa_packet* packets) {
    pdf_xfa* xfa = fz_malloc_struct(ctx, pdf_xfa);

    xfa->refs = 1;
    xfa->doc = doc;
    xfa->pool = fz_new_pool(ctx);
    xfa->ids = fz_new_hash_table(ctx, 256, FZ_HASH_TABLE_KEY_LENGTH, -1, pdf_xfa_ids_drop);
    xfa->packets = packets;

    fz_try(ctx) {
        xfa->root = pdf_xfa_parse_packets(ctx, xfa->pool, packets, xfa->ids);
        if (!xfa->root) fz_throw(ctx, FZ_ERROR_FORMAT, "XFA: parse failed");

        xfa->form = pdf_xfa_bind(ctx, xfa->pool, xfa);
        if (xfa->form) {
            xfa->global.template_root = xfa->form;
            xfa->valid = 1;
        }
    }
    fz_catch(ctx) {
        pdf_xfa_drop_imp(ctx, xfa);
        fz_rethrow(ctx);
    }

    return xfa;
}

static void pdf_xfa_drop_imp(fz_context* ctx, pdf_xfa* xfa) {
    if (!xfa) return;
    pdf_xfa_drop_packets(ctx, xfa->packets);
    fz_free(ctx, xfa->page_bboxes);
    fz_free(ctx, xfa->page_areas);
    fz_free(ctx, xfa->page_subforms);
    fz_drop_hash_table(ctx, xfa->ids);
    fz_drop_pool(ctx, xfa->pool);
    fz_free(ctx, xfa);
}

pdf_xfa* pdf_keep_xfa(fz_context* ctx, pdf_xfa* xfa) {
    return fz_keep_imp(ctx, xfa, &xfa->refs);
}

void pdf_drop_xfa(fz_context* ctx, pdf_xfa* xfa) {
    if (fz_drop_imp(ctx, xfa, &xfa->refs)) pdf_xfa_drop_imp(ctx, xfa);
}
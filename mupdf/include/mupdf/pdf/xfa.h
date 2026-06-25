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

#ifndef MUPDF_PDF_XFA_H
#define MUPDF_PDF_XFA_H

#include "mupdf/fitz/display-list.h"
#include "mupdf/pdf/document.h"

/*
        XFA (XML Forms Architecture) support.

        This API mirrors the pdf.js XFAFactory / XfaLayer surface, adapted for
        MuPDF's C rendering pipeline.  Parsing, binding, layout, and rendering are
        staged; see the implementation plan in the xfa/ sources.
*/

typedef struct pdf_xfa pdf_xfa;

typedef struct pdf_xfa_html_node pdf_xfa_html_node;

struct pdf_xfa_html_node {
    char* name;
    char* value; /* for #text nodes */
    pdf_xfa_html_node* parent;
    pdf_xfa_html_node* first_child;
    pdf_xfa_html_node* next_sibling;
    /* attribute list: parallel arrays terminated by NULL name */
    char** attr_names;
    char** attr_values;
};

/*
        Return non-zero if the document has an AcroForm/XFA entry.
*/
int pdf_document_has_xfa(fz_context* ctx, pdf_document* doc);

/*
        Return non-zero if this is (or was) a pure XFA document: empty AcroForm
        Fields with an XFA packet present.  Wraps pdf_was_pure_xfa.
*/
int pdf_document_is_pure_xfa(fz_context* ctx, pdf_document* doc);

/*
        Lazily load and parse the XFA resource.  Returns NULL when no XFA is
        present or parsing failed.  The returned pointer is owned by the document.
*/
pdf_xfa* pdf_load_xfa(fz_context* ctx, pdf_document* doc);

/*
        After a failed pdf_load_xfa, returns the last error message (empty string if none).
        Valid until the next pdf_load_xfa call on any context in this thread.
*/
const char* pdf_xfa_last_load_error(fz_context* ctx);

/*
        Return non-zero when parse + bind succeeded (pdf.js: XFAFactory.isValid).
*/
int pdf_xfa_is_valid(fz_context* ctx, pdf_xfa* xfa);

/*
        Page geometry after layout (pdf.js: getNumPages / getBoundingBox).
*/
int pdf_xfa_page_count(fz_context* ctx, pdf_xfa* xfa);
fz_rect pdf_xfa_page_bbox(fz_context* ctx, pdf_xfa* xfa, int page_index);

/*
        Run layout if not done yet.  Returns the page count (may be 0 until
        template/layout modules are ported).
*/
int pdf_xfa_layout(fz_context* ctx, pdf_xfa* xfa);

/*
        Return how many field/draw nodes were painted by the last pdf_xfa_run_page
        call (0 before any render).
*/
int pdf_xfa_last_render_fields(fz_context* ctx, pdf_xfa* xfa);
int pdf_xfa_last_render_draws(fz_context* ctx, pdf_xfa* xfa);
int pdf_xfa_last_render_borders(fz_context* ctx, pdf_xfa* xfa);
int pdf_xfa_last_render_lines(fz_context* ctx, pdf_xfa* xfa);

/*
        Field counts in the bound form tree (after layout). fields_outside_pageset are
        not under any pageSet ancestor.
*/
int pdf_xfa_fields_in_pageset(fz_context* ctx, pdf_xfa* xfa);
int pdf_xfa_fields_outside_pageset(fz_context* ctx, pdf_xfa* xfa);
int pdf_xfa_fields_with_pagearea(fz_context* ctx, pdf_xfa* xfa);
int pdf_xfa_fields_with_pagearea_template(fz_context* ctx, pdf_xfa* xfa);

/*
        Return the pageArea name attribute for a laid-out page (empty string if none).
*/
const char* pdf_xfa_page_area_name(fz_context* ctx, pdf_xfa* xfa, int page_index);

/*
        Count loaded font family slots and held fz_font references after XFA init.
*/
void pdf_xfa_font_stats(fz_context* ctx, pdf_xfa* xfa, int* families_out, int* held_out, int* missing_out);

/*
        Render flags for pdf_xfa_run_page (reset to 0 after hybrid overlay use).
*/
enum {
    PDF_XFA_RENDER_FIELDS_ONLY = 1,
    PDF_XFA_RENDER_NO_BACKGROUND = 2,
    PDF_XFA_RENDER_PROBE_FIELDS = 4,
};

#define PDF_XFA_FIELD_PROBE_MAX 256

typedef struct {
    char name[64];
    fz_rect rect;
} pdf_xfa_field_probe;

void pdf_xfa_set_render_flags(fz_context* ctx, pdf_xfa* xfa, int flags);

int pdf_xfa_field_probe_count(fz_context* ctx, pdf_xfa* xfa);
const pdf_xfa_field_probe* pdf_xfa_field_probe_entry(fz_context* ctx, pdf_xfa* xfa, int index);

/*
        Render a laid-out XFA page into a display list (replaces pdf.js htmlForXfa
        + XfaLayer for MuPDF consumers).
*/
fz_display_list* pdf_xfa_run_page(fz_context* ctx, pdf_xfa* xfa, int page_index, fz_matrix ctm);

/*
        Serialize bound datasets back to XML (pdf.js: serializeData).
*/
fz_buffer* pdf_xfa_serialize_data(fz_context* ctx, pdf_xfa* xfa);

/*
        Drop a reference obtained via pdf_keep_xfa.  Document-owned instances are
        freed by pdf_invalidate_xfa / pdf_drop_document.
*/
pdf_xfa* pdf_keep_xfa(fz_context* ctx, pdf_xfa* xfa);
void pdf_drop_xfa(fz_context* ctx, pdf_xfa* xfa);

#endif
// Copyright (C) 2004-2023 Artifex Software, Inc.
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

#ifndef MUPDF_PDF_ANNOT_IMP_H
#define MUPDF_PDF_ANNOT_IMP_H

#include "mupdf/pdf.h"

struct pdf_annot
{
	int refs;

	pdf_page *page;
	pdf_obj *obj;

	int is_hot;
	int is_active;

	int needs_new_local_ap; /* True for widgets when AcroForm/NeedAppearances is set */
	int needs_new_ap; /* If set, then a resynthesis of this annotation has been requested. (-1 to request local only) */
	int has_new_ap; /* If set, then the appearance stream has changed since last queried. */
	int ignore_trigger_events; /* Avoids triggering events during editing of e.g. text field widgets. */
	int hidden_editing; /* Hides annotation from rendering e.g. during editing. */

	pdf_annot *next;
};

typedef struct
{
	fz_link super;
	pdf_page *page;
	pdf_obj *obj;
} pdf_link;

void pdf_load_annots(fz_context *ctx, pdf_page *page);
void pdf_drop_annots(fz_context *ctx, pdf_annot *annot_list);
void pdf_drop_widgets(fz_context *ctx, pdf_annot *widget_list);

void pdf_set_annot_has_changed(fz_context *ctx, pdf_annot *annot);

/*
	Create a destination object given an internal link URI.
*/
pdf_obj *pdf_add_filespec(fz_context *ctx, pdf_document *doc, const char *file, pdf_obj *embedded_file);
pdf_obj *pdf_add_url_filespec(fz_context *ctx, pdf_document *doc, const char *url);
char *pdf_parse_link_dest(fz_context *ctx, pdf_document *doc, pdf_obj *dest);
char *pdf_parse_link_action(fz_context *ctx, pdf_document *doc, pdf_obj *obj, int pagenum);

/* Rect diff is not really a rect! Just returned as such for convenience. */
fz_rect pdf_annot_rect_diff(fz_context *ctx, pdf_annot *annot);

#endif

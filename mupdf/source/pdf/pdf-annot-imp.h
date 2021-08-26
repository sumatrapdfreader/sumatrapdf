// Copyright (C) 2004-2021 Artifex Software, Inc.
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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

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

	int needs_new_ap; /* If set, then a resynthesis of this annotation has been requested. */
	int has_new_ap; /* If set, then the appearance stream has changed since last queried. */
	int ignore_trigger_events;

	pdf_annot *next;
};

void pdf_load_annots(fz_context *ctx, pdf_page *page, pdf_obj *annots);
void pdf_drop_annots(fz_context *ctx, pdf_annot *annot_list);
void pdf_drop_widgets(fz_context *ctx, pdf_annot *widget_list);

void pdf_set_annot_has_changed(fz_context *ctx, pdf_annot *annot);

#endif

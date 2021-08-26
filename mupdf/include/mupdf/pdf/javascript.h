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

#ifndef MUPDF_PDF_JAVASCRIPT_H
#define MUPDF_PDF_JAVASCRIPT_H

void pdf_enable_js(fz_context *ctx, pdf_document *doc);
void pdf_disable_js(fz_context *ctx, pdf_document *doc);
int pdf_js_supported(fz_context *ctx, pdf_document *doc);
void pdf_drop_js(fz_context *ctx, pdf_js *js);

void pdf_js_event_init(pdf_js *js, pdf_obj *target, const char *value, int willCommit);
int pdf_js_event_result(pdf_js *js);
int pdf_js_event_result_validate(pdf_js *js, char **newvalue);
char *pdf_js_event_value(pdf_js *js);
void pdf_js_event_init_keystroke(pdf_js *js, pdf_obj *target, pdf_keystroke_event *evt);
int pdf_js_event_result_keystroke(pdf_js *js, pdf_keystroke_event *evt);

void pdf_js_execute(pdf_js *js, const char *name, const char *code);

#endif

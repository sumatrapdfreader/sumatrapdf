// Copyright (C) 2023 Artifex Software, Inc.
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

// This header allows people to easily build HTML-based document handlers.

#ifndef MUPDF_HTML_HTML_H
#define MUPDF_HTML_HTML_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/document.h"

/*
	HTML types required
*/
typedef struct fz_html_s fz_html;
typedef struct fz_html_font_set_s fz_html_font_set;

typedef struct
{
	const char *format_name;
	fz_buffer *(*convert_to_html)(fz_context *ctx, fz_html_font_set *set, fz_buffer *buf, const char *user_css);
	int try_xml;
	int try_html5;
	int patch_mobi;
} fz_htdoc_format_t;

fz_document *fz_htdoc_open_document_with_buffer(fz_context *ctx, fz_archive *zip, fz_buffer *buf, const fz_htdoc_format_t *format);

fz_document *fz_htdoc_open_document_with_file_and_dir(fz_context *ctx, const char *dirname, const char *filename, const fz_htdoc_format_t *format);

fz_document *fz_htdoc_open_document_with_file(fz_context *ctx, const char *filename, const fz_htdoc_format_t *format);

fz_document *fz_htdoc_open_document_with_stream_and_dir(fz_context *ctx, const char *dirname, fz_stream *stm, const fz_htdoc_format_t *format);

fz_document *fz_htdoc_open_document_with_stream(fz_context *ctx, fz_stream *file, const fz_htdoc_format_t *format);



#endif /* MUPDF_HTML_HTML_H */

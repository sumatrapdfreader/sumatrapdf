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
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#ifndef SOURCE_SVG_IMP_H
#define SOURCE_SVG_IMP_H

typedef struct svg_cycle_list_s svg_cycle_list;
struct svg_cycle_list_s {
	svg_cycle_list *up;
	fz_xml *symbol;
};

typedef struct svg_document_s svg_document;

struct svg_document_s
{
	fz_document super;
	fz_xml_doc *xml;
	fz_xml *root;
	fz_tree *idmap;
	float width;
	float height;
	svg_cycle_list *cycle; /* for detecting mutual recursive <use> invocations */
	fz_archive *zip; /* for locating external resources */
	char base_uri[2048];
};

const char *svg_lex_number(float *fp, const char *str);
float svg_parse_number(const char *str, float min, float max, float inherit);
float svg_parse_number_from_style(fz_context *ctx, svg_document *doc, const char *string, const char *name, float number);
int svg_parse_enum_from_style(fz_context *ctx, svg_document *doc, const char *style, const char *att,
	int ecount, const char *etable[], int value);
char *svg_parse_string_from_style(fz_context *ctx, svg_document *doc, const char *style, const char *att,
	char *buf, int buf_size, const char *value);

/*
	Return length/coordinate in points.
*/
float svg_parse_length(const char *str, float percent, float font_size);
float svg_parse_angle(const char *str);

void svg_parse_color_from_style(fz_context *ctx, svg_document *doc, const char *str,
	int *fill_is_set, float fill[3], int *stroke_is_set, float stroke[3]);
void svg_parse_color(fz_context *ctx, svg_document *doc, const char *str, float *rgb);
fz_matrix svg_parse_transform(fz_context *ctx, svg_document *doc, const char *str, fz_matrix transform);

int svg_is_whitespace_or_comma(int c);
int svg_is_whitespace(int c);
int svg_is_alpha(int c);
int svg_is_digit(int c);

void svg_parse_document_bounds(fz_context *ctx, svg_document *doc, fz_xml *root);
void svg_run_document(fz_context *ctx, svg_document *doc, fz_xml *root, fz_device *dev, fz_matrix ctm);

#endif

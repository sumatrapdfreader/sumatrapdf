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

#ifndef MUPDF_PDF_ANNOT_H
#define MUPDF_PDF_ANNOT_H

#include "mupdf/fitz/display-list.h"
#include "mupdf/fitz/stream.h"
#include "mupdf/fitz/structured-text.h"
#include "mupdf/pdf/object.h"
#include "mupdf/pdf/page.h"

typedef struct pdf_annot pdf_annot;

enum pdf_annot_type
{
	PDF_ANNOT_TEXT,
	PDF_ANNOT_LINK,
	PDF_ANNOT_FREE_TEXT,
	PDF_ANNOT_LINE,
	PDF_ANNOT_SQUARE,
	PDF_ANNOT_CIRCLE,
	PDF_ANNOT_POLYGON,
	PDF_ANNOT_POLY_LINE,
	PDF_ANNOT_HIGHLIGHT,
	PDF_ANNOT_UNDERLINE,
	PDF_ANNOT_SQUIGGLY,
	PDF_ANNOT_STRIKE_OUT,
	PDF_ANNOT_REDACT,
	PDF_ANNOT_STAMP,
	PDF_ANNOT_CARET,
	PDF_ANNOT_INK,
	PDF_ANNOT_POPUP,
	PDF_ANNOT_FILE_ATTACHMENT,
	PDF_ANNOT_SOUND,
	PDF_ANNOT_MOVIE,
	PDF_ANNOT_RICH_MEDIA,
	PDF_ANNOT_WIDGET,
	PDF_ANNOT_SCREEN,
	PDF_ANNOT_PRINTER_MARK,
	PDF_ANNOT_TRAP_NET,
	PDF_ANNOT_WATERMARK,
	PDF_ANNOT_3D,
	PDF_ANNOT_PROJECTION,
	PDF_ANNOT_UNKNOWN = -1
};

/*
	Map an annotation type to a (static) string.

	The returned string must not be freed by the caller.
*/
const char *pdf_string_from_annot_type(fz_context *ctx, enum pdf_annot_type type);

/*
	Map from a (non-NULL, case sensitive) string to an annotation
	type.
*/
enum pdf_annot_type pdf_annot_type_from_string(fz_context *ctx, const char *subtype);

enum
{
	PDF_ANNOT_IS_INVISIBLE = 1 << (1-1),
	PDF_ANNOT_IS_HIDDEN = 1 << (2-1),
	PDF_ANNOT_IS_PRINT = 1 << (3-1),
	PDF_ANNOT_IS_NO_ZOOM = 1 << (4-1),
	PDF_ANNOT_IS_NO_ROTATE = 1 << (5-1),
	PDF_ANNOT_IS_NO_VIEW = 1 << (6-1),
	PDF_ANNOT_IS_READ_ONLY = 1 << (7-1),
	PDF_ANNOT_IS_LOCKED = 1 << (8-1),
	PDF_ANNOT_IS_TOGGLE_NO_VIEW = 1 << (9-1),
	PDF_ANNOT_IS_LOCKED_CONTENTS = 1 << (10-1)
};

enum pdf_line_ending
{
	PDF_ANNOT_LE_NONE = 0,
	PDF_ANNOT_LE_SQUARE,
	PDF_ANNOT_LE_CIRCLE,
	PDF_ANNOT_LE_DIAMOND,
	PDF_ANNOT_LE_OPEN_ARROW,
	PDF_ANNOT_LE_CLOSED_ARROW,
	PDF_ANNOT_LE_BUTT,
	PDF_ANNOT_LE_R_OPEN_ARROW,
	PDF_ANNOT_LE_R_CLOSED_ARROW,
	PDF_ANNOT_LE_SLASH
};

enum
{
	PDF_ANNOT_Q_LEFT = 0,
	PDF_ANNOT_Q_CENTER = 1,
	PDF_ANNOT_Q_RIGHT = 2
};

/*
	Map from a PDF name specifying an annotation line ending
	to an enumerated line ending value.
*/
enum pdf_line_ending pdf_line_ending_from_name(fz_context *ctx, pdf_obj *end);

/*
	Map from a (non-NULL, case sensitive) C string specifying
	an annotation line ending to an enumerated line ending value.
*/
enum pdf_line_ending pdf_line_ending_from_string(fz_context *ctx, const char *end);

/*
	Map from an enumerated line ending to a pdf name object that
	specifies it.
*/
pdf_obj *pdf_name_from_line_ending(fz_context *ctx, enum pdf_line_ending end);

/*
	Map from an enumerated line ending to a C string that specifies
	it.

	The caller must not free the returned string.
*/
const char *pdf_string_from_line_ending(fz_context *ctx, enum pdf_line_ending end);

/*
	Increment the reference count for an annotation.

	Never throws exceptions. Returns the same pointer.
*/
pdf_annot *pdf_keep_annot(fz_context *ctx, pdf_annot *annot);

/*
	Drop the reference count for an annotation.

	When the reference count reaches zero, the annotation will
	be destroyed. Never throws exceptions.
*/
void pdf_drop_annot(fz_context *ctx, pdf_annot *annot);

/*
	Returns a borrowed reference to the first annotation on
	a page, or NULL if none.

	The caller should fz_keep this if it intends to hold the
	pointer. Unless it fz_keeps it, it must not fz_drop it.
*/
pdf_annot *pdf_first_annot(fz_context *ctx, pdf_page *page);

/*
	Returns a borrowed reference to the next annotation
	on a page, or NULL if none.

	The caller should fz_keep this if it intends to hold the
	pointer. Unless it fz_keeps it, it must not fz_drop it.
*/
pdf_annot *pdf_next_annot(fz_context *ctx, pdf_annot *annot);

/*
	Returns a borrowed reference to the object underlying
	an annotation.

	The caller should fz_keep this if it intends to hold the
	pointer. Unless it fz_keeps it, it must not fz_drop it.
*/
pdf_obj *pdf_annot_obj(fz_context *ctx, pdf_annot *annot);

/*
	Returns a borrowed reference to the page to which
	an annotation belongs.

	The caller should fz_keep this if it intends to hold the
	pointer. Unless it fz_keeps it, it must not fz_drop it.
*/
pdf_page *pdf_annot_page(fz_context *ctx, pdf_annot *annot);

/*
	Return the rectangle for an annotation on a page.
*/
fz_rect pdf_bound_annot(fz_context *ctx, pdf_annot *annot);

enum pdf_annot_type pdf_annot_type(fz_context *ctx, pdf_annot *annot);

/*
	Interpret an annotation and render it on a device.

	page: A page loaded by pdf_load_page.

	annot: an annotation.

	dev: Device used for rendering, obtained from fz_new_*_device.

	ctm: A transformation matrix applied to the objects on the page,
	e.g. to scale or rotate the page contents as desired.
*/
void pdf_run_annot(fz_context *ctx, pdf_annot *annot, fz_device *dev, fz_matrix ctm, fz_cookie *cookie);

/*
	Lookup needle in the nametree of the document given by which.

	The returned reference is borrowed, and should not be dropped,
	unless it is kept first.
*/
pdf_obj *pdf_lookup_name(fz_context *ctx, pdf_document *doc, pdf_obj *which, pdf_obj *needle);

/*
	Load a nametree, flattening it into a single dictionary.

	The caller is responsible for pdf_dropping the returned
	reference.
*/
pdf_obj *pdf_load_name_tree(fz_context *ctx, pdf_document *doc, pdf_obj *which);

/*
	Lookup needle in the given number tree.

	The returned reference is borrowed, and should not be dropped,
	unless it is kept first.
*/
pdf_obj *pdf_lookup_number(fz_context *ctx, pdf_obj *root, int needle);

/*
	Perform a depth first traversal of a tree.

	Start at tree, looking for children in the array named
	kid_name at each level.

	The arrive callback is called when we arrive at a node (i.e.
	before all the children are walked), and then the leave callback
	is called as we leave it (after all the children have been
	walked).

	names and values are (matching) null terminated arrays of
	names and values to be carried down the tree, to implement
	inheritance. NULL is a permissible value.
*/
void pdf_walk_tree(fz_context *ctx, pdf_obj *tree, pdf_obj *kid_name,
			void (*arrive)(fz_context *, pdf_obj *, void *, pdf_obj **),
			void (*leave)(fz_context *, pdf_obj *, void *),
			void *arg,
			pdf_obj **names,
			pdf_obj **values);

/*
	Resolve a link within a document.
*/
int pdf_resolve_link(fz_context *ctx, pdf_document *doc, const char *uri, float *xp, float *yp);
fz_link_dest pdf_resolve_link_dest(fz_context *ctx, pdf_document *doc, const char *uri);

/*
	Create an action object given a link URI. The action will
	be a GoTo or URI action depending on whether the link URI
	specifies a document internal or external destination.
*/
pdf_obj *pdf_new_action_from_link(fz_context *ctx, pdf_document *doc, const char *uri);

/*
	Create a destination object given a link URI expected to adhere
	to the Adobe specification "Parameters for Opening PDF files"
	from the Adobe Acrobat SDK. The resulting destination object
	will either be a PDF string, or a PDF array referring to a page
	and suitable zoom level settings. In the latter case the page
	can be referred to by PDF object number or by page number, this
	is controlled by the is_remote argument. For remote destinations
	it is not possible to refer to the page by object number, so
	page numbers are used instead.
*/
pdf_obj *pdf_new_dest_from_link(fz_context *ctx, pdf_document *doc, const char *uri, int is_remote);

/*
	Create a link URI string according to the Adobe specification
	"Parameters for Opening PDF files" from the Adobe Acrobat SDK,
	version 8.1, which can, at the time of writing, be found here:

	https://web.archive.org/web/20170921000830/http://www.adobe.com/content/dam/Adobe/en/devnet/acrobat/pdfs/pdf_open_parameters.pdf

	The resulting string must be freed by the caller.
*/
char *pdf_new_uri_from_explicit_dest(fz_context *ctx, fz_link_dest dest);

/*
	Create a remote link URI string according to the Adobe specification
	"Parameters for Opening PDF files" from the Adobe Acrobat SDK,
	version 8.1, which can, at the time of writing, be found here:

	https://web.archive.org/web/20170921000830/http://www.adobe.com/content/dam/Adobe/en/devnet/acrobat/pdfs/pdf_open_parameters.pdf

	The file: URI scheme is used in the resulting URI if the remote document
	is specified by a system independent path (already taking the recommendations
	in table 3.40 of the PDF 1.7 specification into account), and either a
	destination name or a page number and zoom level are appended:
	file:///path/doc.pdf#page=42&view=FitV,100
	file:///path/doc.pdf#nameddest=G42.123456

	If a URL is used to specify the remote document, then its scheme takes
	precedence and either a destination name or a page number and zoom level
	are appended:
	ftp://example.com/alpha.pdf#page=42&view=Fit
	https://example.com/bravo.pdf?query=parameter#page=42&view=Fit

	The resulting string must be freed by the caller.
*/
char *pdf_append_named_dest_to_uri(fz_context *ctx, const char *url, const char *name);
char *pdf_append_explicit_dest_to_uri(fz_context *ctx, const char *url, fz_link_dest dest);
char *pdf_new_uri_from_path_and_named_dest(fz_context *ctx, const char *path, const char *name);
char *pdf_new_uri_from_path_and_explicit_dest(fz_context *ctx, const char *path, fz_link_dest dest);

/*
	Create transform to fit appearance stream to annotation Rect
*/
fz_matrix pdf_annot_transform(fz_context *ctx, pdf_annot *annot);

/*
	Create a new link object.
*/
fz_link *pdf_new_link(fz_context *ctx, pdf_page *page, fz_rect rect, const char *uri, pdf_obj *obj);

/*
	create a new annotation of the specified type on the
	specified page. The returned pdf_annot structure is owned by the
	page and does not need to be freed.
*/
pdf_annot *pdf_create_annot_raw(fz_context *ctx, pdf_page *page, enum pdf_annot_type type);

/*
	create a new link on the specified page. The returned fz_link
	structure is owned by the page and does not need to be freed.
*/
fz_link *pdf_create_link(fz_context *ctx, pdf_page *page, fz_rect bbox, const char *uri);

/*
	delete an existing link from the specified page.
*/
void pdf_delete_link(fz_context *ctx, pdf_page *page, fz_link *link);

enum pdf_border_style
{
	PDF_BORDER_STYLE_SOLID = 0,
	PDF_BORDER_STYLE_DASHED,
	PDF_BORDER_STYLE_BEVELED,
	PDF_BORDER_STYLE_INSET,
	PDF_BORDER_STYLE_UNDERLINE,
};

enum pdf_border_effect
{
	PDF_BORDER_EFFECT_NONE = 0,
	PDF_BORDER_EFFECT_CLOUDY,
};

/*
	create a new annotation of the specified type on the
	specified page. Populate it with sensible defaults per the type.

	Currently this returns a reference that the caller owns, and
	must drop when finished with it. Up until release 1.18, the
	returned reference was owned by the page and did not need to
	be freed.
*/
pdf_annot *pdf_create_annot(fz_context *ctx, pdf_page *page, enum pdf_annot_type type);

/*
	Delete an annoation from the page.

	This unlinks the annotation from the page structure and drops
	the pages reference to it. Any reference held by the caller
	will not be dropped automatically, so this can safely be used
	on a borrowed reference.
*/
void pdf_delete_annot(fz_context *ctx, pdf_page *page, pdf_annot *annot);

/*
	Edit the associated Popup annotation rectangle.

	Popup annotations are used to store the size and position of the
	popup box that is used to edit the contents of the markup annotation.
*/
void pdf_set_annot_popup(fz_context *ctx, pdf_annot *annot, fz_rect rect);
fz_rect pdf_annot_popup(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has a rect.
*/
int pdf_annot_has_rect(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has an ink list.
*/
int pdf_annot_has_ink_list(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has quad points data.
*/
int pdf_annot_has_quad_points(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has vertex data.
*/
int pdf_annot_has_vertices(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has line data.
*/
int pdf_annot_has_line(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has an interior color.
*/
int pdf_annot_has_interior_color(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has line ending styles.
*/
int pdf_annot_has_line_ending_styles(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has a border.
*/
int pdf_annot_has_border(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has a border effect.
*/
int pdf_annot_has_border_effect(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has an icon name.
*/
int pdf_annot_has_icon_name(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has an open action.
*/
int pdf_annot_has_open(fz_context *ctx, pdf_annot *annot);

/*
	Check to see if an annotation has author data.
*/
int pdf_annot_has_author(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the annotation flags.
*/
int pdf_annot_flags(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the annotation bounds in doc space.
*/
fz_rect pdf_annot_rect(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the annotation border line width in points.
*/
float pdf_annot_border(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the annotation border style.
 */
enum pdf_border_style pdf_annot_border_style(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the annotation border width in points.
 */
float pdf_annot_border_width(fz_context *ctx, pdf_annot *annot);

/*
	How many items does the annotation border dash pattern have?
 */
int pdf_annot_border_dash_count(fz_context *ctx, pdf_annot *annot);

/*
	How long is dash item i in the annotation border dash pattern?
 */
float pdf_annot_border_dash_item(fz_context *ctx, pdf_annot *annot, int i);

/*
	Retrieve the annotation border effect.
 */
enum pdf_border_effect pdf_annot_border_effect(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the annotation border effect intensity.
 */
float pdf_annot_border_effect_intensity(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the annotation opacity. (0 transparent, 1 solid).
*/
float pdf_annot_opacity(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the annotation color.

	n components, each between 0 and 1.
	n = 1 (grey), 3 (rgb) or 4 (cmyk).
*/
void pdf_annot_color(fz_context *ctx, pdf_annot *annot, int *n, float color[4]);

/*
	Retrieve the annotation interior color.

	n components, each between 0 and 1.
	n = 1 (grey), 3 (rgb) or 4 (cmyk).
*/
void pdf_annot_interior_color(fz_context *ctx, pdf_annot *annot, int *n, float color[4]);

/*
	Retrieve the annotation quadding (justification) to use.
		0 = Left-justified
		1 = Centered
		2 = Right-justified
*/
int pdf_annot_quadding(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the annotations text language (either from the
	annotation, or from the document).
*/
fz_text_language pdf_annot_language(fz_context *ctx, pdf_annot *annot);

/*
	How many quad points does an annotation have?
*/
int pdf_annot_quad_point_count(fz_context *ctx, pdf_annot *annot);

/*
	Get quadpoint i for an annotation.
*/
fz_quad pdf_annot_quad_point(fz_context *ctx, pdf_annot *annot, int i);

/*
	How many strokes in the ink list for an annotation?
*/
int pdf_annot_ink_list_count(fz_context *ctx, pdf_annot *annot);

/*
	How many vertexes in stroke i of the ink list for an annotation?
*/
int pdf_annot_ink_list_stroke_count(fz_context *ctx, pdf_annot *annot, int i);

/*
	Get vertex k from stroke i of the ink list for an annoation, in
	doc space.
*/
fz_point pdf_annot_ink_list_stroke_vertex(fz_context *ctx, pdf_annot *annot, int i, int k);

/*
	Set the flags for an annotation.
*/
void pdf_set_annot_flags(fz_context *ctx, pdf_annot *annot, int flags);

/*
	Set the stamp appearance stream to a custom image.
	Fits the image to the current Rect, and shrinks the Rect
	to fit the image aspect ratio.
*/
void pdf_set_annot_stamp_image(fz_context *ctx, pdf_annot *annot, fz_image *image);

/*
	Set the bounding box for an annotation, in doc space.
*/
void pdf_set_annot_rect(fz_context *ctx, pdf_annot *annot, fz_rect rect);

/*
	Set the border width for an annotation, in points and remove any border effect.
*/
void pdf_set_annot_border(fz_context *ctx, pdf_annot *annot, float width);

/*
	Set the border style for an annotation.
*/
void pdf_set_annot_border_style(fz_context *ctx, pdf_annot *annot, enum pdf_border_style style);

/*
	Set the border width for an annotation in points;
*/
void pdf_set_annot_border_width(fz_context *ctx, pdf_annot *annot, float width);

/*
	Clear the entire border dash pattern for an annotation.
*/
void pdf_clear_annot_border_dash(fz_context *ctx, pdf_annot *annot);

/*
	Add an item to the end of the border dash pattern for an annotation.
*/
void pdf_add_annot_border_dash_item(fz_context *ctx, pdf_annot *annot, float length);

/*
	Set the border effect for an annotation.
*/
void pdf_set_annot_border_effect(fz_context *ctx, pdf_annot *annot, enum pdf_border_effect effect);

/*
	Set the border effect intensity for an annotation.
*/
void pdf_set_annot_border_effect_intensity(fz_context *ctx, pdf_annot *annot, float intensity);

/*
	Set the opacity for an annotation, between 0 (transparent) and 1
	(solid).
*/
void pdf_set_annot_opacity(fz_context *ctx, pdf_annot *annot, float opacity);

/*
	Set the annotation color.

	n components, each between 0 and 1.
	n = 1 (grey), 3 (rgb) or 4 (cmyk).
*/
void pdf_set_annot_color(fz_context *ctx, pdf_annot *annot, int n, const float *color);

/*
	Set the annotation interior color.

	n components, each between 0 and 1.
	n = 1 (grey), 3 (rgb) or 4 (cmyk).
*/
void pdf_set_annot_interior_color(fz_context *ctx, pdf_annot *annot, int n, const float *color);

/*
	Set the quadding (justification) to use for the annotation.
		0 = Left-justified
		1 = Centered
		2 = Right-justified
*/
void pdf_set_annot_quadding(fz_context *ctx, pdf_annot *annot, int q);

/*
	Set the language for the annotation.
*/
void pdf_set_annot_language(fz_context *ctx, pdf_annot *annot, fz_text_language lang);

/*
	Set the quad points for an annotation to those in the qv array
	of length n.
*/
void pdf_set_annot_quad_points(fz_context *ctx, pdf_annot *annot, int n, const fz_quad *qv);

/*
	Clear the quadpoint data for an annotation.
*/
void pdf_clear_annot_quad_points(fz_context *ctx, pdf_annot *annot);

/*
	Append a new quad point to the quad point data in an annotation.
*/
void pdf_add_annot_quad_point(fz_context *ctx, pdf_annot *annot, fz_quad quad);

/*
	Set the ink list for an annotation.

	n strokes. For 0 <= i < n, stroke i has count[i] points,
	The vertexes for all the strokes are packed into a single
	array, pointed to by v.
*/
void pdf_set_annot_ink_list(fz_context *ctx, pdf_annot *annot, int n, const int *count, const fz_point *v);

/*
	Clear the ink list for an annotation.
*/
void pdf_clear_annot_ink_list(fz_context *ctx, pdf_annot *annot);

/*
	Add a new stroke (initially empty) to the ink list for an
	annotation.
*/
void pdf_add_annot_ink_list_stroke(fz_context *ctx, pdf_annot *annot);

/*
	Add a new vertex to the last stroke in the ink list for an
	annotation.
*/
void pdf_add_annot_ink_list_stroke_vertex(fz_context *ctx, pdf_annot *annot, fz_point p);

/*
	Add a new stroke to the ink list for an annotation, and
	populate it with the n points from stroke[].
*/
void pdf_add_annot_ink_list(fz_context *ctx, pdf_annot *annot, int n, fz_point stroke[]);

/*

*/
void pdf_set_annot_icon_name(fz_context *ctx, pdf_annot *annot, const char *name);
void pdf_set_annot_is_open(fz_context *ctx, pdf_annot *annot, int is_open);

enum pdf_line_ending pdf_annot_line_start_style(fz_context *ctx, pdf_annot *annot);
enum pdf_line_ending pdf_annot_line_end_style(fz_context *ctx, pdf_annot *annot);
void pdf_annot_line_ending_styles(fz_context *ctx, pdf_annot *annot, enum pdf_line_ending *start_style, enum pdf_line_ending *end_style);
void pdf_set_annot_line_start_style(fz_context *ctx, pdf_annot *annot, enum pdf_line_ending s);
void pdf_set_annot_line_end_style(fz_context *ctx, pdf_annot *annot, enum pdf_line_ending e);
void pdf_set_annot_line_ending_styles(fz_context *ctx, pdf_annot *annot, enum pdf_line_ending start_style, enum pdf_line_ending end_style);

const char *pdf_annot_icon_name(fz_context *ctx, pdf_annot *annot);
int pdf_annot_is_open(fz_context *ctx, pdf_annot *annot);
int pdf_annot_is_standard_stamp(fz_context *ctx, pdf_annot *annot);

void pdf_annot_line(fz_context *ctx, pdf_annot *annot, fz_point *a, fz_point *b);
void pdf_set_annot_line(fz_context *ctx, pdf_annot *annot, fz_point a, fz_point b);

int pdf_annot_vertex_count(fz_context *ctx, pdf_annot *annot);
fz_point pdf_annot_vertex(fz_context *ctx, pdf_annot *annot, int i);

void pdf_set_annot_vertices(fz_context *ctx, pdf_annot *annot, int n, const fz_point *v);
void pdf_clear_annot_vertices(fz_context *ctx, pdf_annot *annot);
void pdf_add_annot_vertex(fz_context *ctx, pdf_annot *annot, fz_point p);
void pdf_set_annot_vertex(fz_context *ctx, pdf_annot *annot, int i, fz_point p);

const char *pdf_annot_contents(fz_context *ctx, pdf_annot *annot);
void pdf_set_annot_contents(fz_context *ctx, pdf_annot *annot, const char *text);

const char *pdf_annot_author(fz_context *ctx, pdf_annot *annot);
void pdf_set_annot_author(fz_context *ctx, pdf_annot *annot, const char *author);

int64_t pdf_annot_modification_date(fz_context *ctx, pdf_annot *annot);
void pdf_set_annot_modification_date(fz_context *ctx, pdf_annot *annot, int64_t time);
int64_t pdf_annot_creation_date(fz_context *ctx, pdf_annot *annot);
void pdf_set_annot_creation_date(fz_context *ctx, pdf_annot *annot, int64_t time);

void pdf_parse_default_appearance(fz_context *ctx, const char *da, const char **font, float *size, int *n, float color[4]);
void pdf_print_default_appearance(fz_context *ctx, char *buf, int nbuf, const char *font, float size, int n, const float *color);
void pdf_annot_default_appearance(fz_context *ctx, pdf_annot *annot, const char **font, float *size, int *n, float color[4]);
void pdf_set_annot_default_appearance(fz_context *ctx, pdf_annot *annot, const char *font, float size, int n, const float *color);

/*
 * Request that an appearance stream should be generated for an annotation if none is present.
 * It will be created in future calls to pdf_update_annot or pdf_update_page.
 */
void pdf_annot_request_synthesis(fz_context *ctx, pdf_annot *annot);

/*
 * Request that an appearance stream should be re-generated for an annotation
 * the next time pdf_annot_update or pdf_page_update is called.
 * You usually won't need to call this, because changing any annotation attributes
 * via the pdf_annot functions will do so automatically.
 * It will be created in future calls to pdf_update_annot or pdf_update_page.
 */
void pdf_annot_request_resynthesis(fz_context *ctx, pdf_annot *annot);

int pdf_annot_needs_resynthesis(fz_context *ctx, pdf_annot *annot);
void pdf_set_annot_resynthesised(fz_context *ctx, pdf_annot *annot);
void pdf_dirty_annot(fz_context *ctx, pdf_annot *annot);

int pdf_annot_field_flags(fz_context *ctx, pdf_annot *annot);
const char *pdf_annot_field_value(fz_context *ctx, pdf_annot *annot);
const char *pdf_annot_field_label(fz_context *ctx, pdf_annot *widget);

int pdf_set_annot_field_value(fz_context *ctx, pdf_document *doc, pdf_annot *widget, const char *text, int ignore_trigger_events);

/*
	Recreate the appearance stream for an annotation, if necessary.
*/
fz_text *pdf_layout_fit_text(fz_context *ctx, fz_font *font, fz_text_language lang, const char *str, fz_rect bounds);

/*
	Start/Stop using the annotation-local xref. This allows us to
	generate appearance streams that don't actually hit the underlying
	document.
*/
void pdf_annot_push_local_xref(fz_context *ctx, pdf_annot *annot);
void pdf_annot_pop_local_xref(fz_context *ctx, pdf_annot *annot);
void pdf_annot_ensure_local_xref(fz_context *ctx, pdf_annot *annot);
void pdf_annot_pop_and_discard_local_xref(fz_context *ctx, pdf_annot *annot);

/*
	Regenerate any appearance streams that are out of date and check for
	cases where a different appearance stream should be selected because of
	state changes.

	Note that a call to pdf_pass_event for one page may lead to changes on
	any other, so an app should call pdf_update_annot for every annotation
	it currently displays. Also it is important that the pdf_annot object
	is the one used to last render the annotation. If instead the app were
	to drop the page or annotations and reload them then a call to
	pdf_update_annot would not reliably be able to report all changed
	annotations.

	Returns true if the annotation appearance has changed since the last time
	pdf_update_annot was called or the annotation was first loaded.
*/
int pdf_update_annot(fz_context *ctx, pdf_annot *annot);

/*
	Recalculate form fields if necessary.

	Loop through all annotations on the page and update them. Return true
	if any of them were changed (by either event or javascript actions, or
	by annotation editing) and need re-rendering.

	If you need more granularity, loop through the annotations and call
	pdf_update_annot for each one to detect changes on a per-annotation
	basis.
*/
int pdf_update_page(fz_context *ctx, pdf_page *page);

/*
	Update internal state appropriate for editing this field. When editing
	is true, updating the text of the text widget will not have any
	side-effects such as changing other widgets or running javascript.
	This state is intended for the period when a text widget is having
	characters typed into it. The state should be reverted at the end of
	the edit sequence and the text newly updated.
*/
void pdf_set_widget_editing_state(fz_context *ctx, pdf_annot *widget, int editing);

int pdf_get_widget_editing_state(fz_context *ctx, pdf_annot *widget);

/*
	Toggle the state of a specified annotation. Applies only to check-box
	and radio-button widgets.
*/
int pdf_toggle_widget(fz_context *ctx, pdf_annot *widget);

fz_display_list *pdf_new_display_list_from_annot(fz_context *ctx, pdf_annot *annot);

/*
	Render an annotation suitable for blending on top of the opaque
	pixmap returned by fz_new_pixmap_from_page_contents.
*/
fz_pixmap *pdf_new_pixmap_from_annot(fz_context *ctx, pdf_annot *annot, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha);
fz_stext_page *pdf_new_stext_page_from_annot(fz_context *ctx, pdf_annot *annot, const fz_stext_options *options);

fz_layout_block *pdf_layout_text_widget(fz_context *ctx, pdf_annot *annot);

typedef struct pdf_embedded_file_params pdf_embedded_file_params;

/*
	Parameters for and embedded file. Obtained through
	pdf_get_embedded_file_params(). The creation and
	modification date fields are < 0 if unknown.
*/
struct pdf_embedded_file_params {
	const char *filename;
	const char *mimetype;
	int size;
	int64_t created;
	int64_t modified;
};

/*
	Check if pdf object is a file specification.
*/
int pdf_is_embedded_file(fz_context *ctx, pdf_obj *fs);

/*
	Add an embedded file to the document. This can later
	be passed e.g. to pdf_annot_set_filespec(). If unknown,
	supply NULL for MIME type and -1 for the date arguments.
	If a checksum is added it can later be verified by calling
	pdf_verify_embedded_file_checksum().
*/
pdf_obj *pdf_add_embedded_file(fz_context *ctx, pdf_document *doc, const char *filename, const char *mimetype, fz_buffer *contents, int64_t created, int64_t modifed, int add_checksum);

/*
	Obtain parameters for embedded file: name, size,
	creation and modification dates cnad MIME type.
*/
void pdf_get_embedded_file_params(fz_context *ctx, pdf_obj *fs, pdf_embedded_file_params *out);

/*
	Load embedded file contents in a buffer which
	needs to be dropped by the called after use.
*/
fz_buffer *pdf_load_embedded_file_contents(fz_context *ctx, pdf_obj *fs);

/*
	Verifies the embedded file checksum. Returns 1
	if the verifiction is successful or there is no
	checksum to be verified, or 0 if verification fails.
*/
int pdf_verify_embedded_file_checksum(fz_context *ctx, pdf_obj *fs);

pdf_obj *pdf_lookup_dest(fz_context *ctx, pdf_document *doc, pdf_obj *needle);
fz_link *pdf_load_link_annots(fz_context *ctx, pdf_document *, pdf_page *, pdf_obj *annots, int pagenum, fz_matrix page_ctm);

void pdf_annot_MK_BG(fz_context *ctx, pdf_annot *annot, int *n, float color[4]);
void pdf_annot_MK_BC(fz_context *ctx, pdf_annot *annot, int *n, float color[4]);
int pdf_annot_MK_BG_rgb(fz_context *ctx, pdf_annot *annot, float rgb[3]);
int pdf_annot_MK_BC_rgb(fz_context *ctx, pdf_annot *annot, float rgb[3]);

pdf_obj *pdf_annot_ap(fz_context *ctx, pdf_annot *annot);

int pdf_annot_active(fz_context *ctx, pdf_annot *annot);
void pdf_set_annot_active(fz_context *ctx, pdf_annot *annot, int active);
int pdf_annot_hot(fz_context *ctx, pdf_annot *annot);
void pdf_set_annot_hot(fz_context *ctx, pdf_annot *annot, int hot);

void pdf_set_annot_appearance(fz_context *ctx, pdf_annot *annot, const char *appearance, const char *state, fz_matrix ctm, fz_rect bbox, pdf_obj *res, fz_buffer *contents);
void pdf_set_annot_appearance_from_display_list(fz_context *ctx, pdf_annot *annot, const char *appearance, const char *state, fz_matrix ctm, fz_display_list *list);

/*
	Check to see if an annotation has a file specification.
*/
int pdf_annot_has_filespec(fz_context *ctx, pdf_annot *annot);

/*
	Retrieve the file specification for the given annotation.
*/
pdf_obj *pdf_annot_filespec(fz_context *ctx, pdf_annot *annot);

/*
	Set the annotation file specification.
*/
void pdf_set_annot_filespec(fz_context *ctx, pdf_annot *annot, pdf_obj *obj);

/*
	Get/set a hidden flag preventing the annotation from being
	rendered when it is being edited. This flag is independent
	of the hidden flag in the PDF annotation object described in the PDF specification.
*/
int pdf_annot_hidden_for_editing(fz_context *ctx, pdf_annot *annot);
void pdf_set_annot_hidden_for_editing(fz_context *ctx, pdf_annot *annot, int hidden);

/*
 * Apply Redaction annotation by redacting page underneath and removing the annotation.
 */
int pdf_apply_redaction(fz_context *ctx, pdf_annot *annot, pdf_redact_options *opts);

#endif

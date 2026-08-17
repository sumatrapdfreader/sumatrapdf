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

#ifndef MUPDF_FITZ_DISPLAY_LIST_H
#define MUPDF_FITZ_DISPLAY_LIST_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/device.h"

/**
	Display list device -- record and play back device commands.
*/

/**
	fz_display_list is a list containing drawing commands (text,
	images, etc.). The intent is two-fold: as a caching-mechanism
	to reduce parsing of a page, and to be used as a data
	structure in multi-threading where one thread parses the page
	and another renders pages.

	Create a display list with fz_new_display_list, hand it over to
	fz_new_list_device to have it populated, and later replay the
	list (once or many times) by calling fz_run_display_list. When
	the list is no longer needed drop it with fz_drop_display_list.
*/
typedef struct fz_display_list fz_display_list;

/**
	Create an empty display list.

	A display list contains drawing commands (text, images, etc.).
	Use fz_new_list_device for populating the list.

	mediabox: Bounds of the page (in points) represented by the
	display list.
*/
fz_display_list *fz_new_display_list(fz_context *ctx, fz_rect mediabox);

/**
	Create a rendering device for a display list.

	When the device is rendering a page it will populate the
	display list with drawing commands (text, images, etc.). The
	display list can later be reused to render a page many times
	without having to re-interpret the page from the document file
	for each rendering. Once the device is no longer needed, free
	it with fz_drop_device.

	list: A display list that the list device takes a reference to.
*/
fz_device *fz_new_list_device(fz_context *ctx, fz_display_list *list);

/**
	(Re)-run a display list through a device.

	list: A display list, created by fz_new_display_list and
	populated with objects from a page by running fz_run_page on a
	device obtained from fz_new_list_device.

	ctm: Transform to apply to display list contents. May include
	for example scaling and rotation, see fz_scale, fz_rotate and
	fz_concat. Set to fz_identity if no transformation is desired.

	scissor: Only the part of the contents of the display list
	visible within this area will be considered when the list is
	run through the device. This does not imply for tile objects
	contained in the display list.

	cookie: Communication mechanism between caller and library
	running the page. Intended for multi-threaded applications,
	while single-threaded applications set cookie to NULL. The
	caller may abort an ongoing page run. Cookie also communicates
	progress information back to the caller. The fields inside
	cookie are continually updated while the page is being run.
*/
void fz_run_display_list(fz_context *ctx, fz_display_list *list, fz_device *dev, fz_matrix ctm, fz_rect scissor, fz_cookie *cookie);

/**
	Increment the reference count for a display list. Returns the
	same pointer.

	Never throws exceptions.
*/
fz_display_list *fz_keep_display_list(fz_context *ctx, fz_display_list *list);

/**
	Decrement the reference count for a display list. When the
	reference count reaches zero, all the references in the display
	list itself are dropped, and the display list is freed.

	Never throws exceptions.
*/
void fz_drop_display_list(fz_context *ctx, fz_display_list *list);

/**
	Return the bounding box of the page recorded in a display list.
*/
fz_rect fz_bound_display_list(fz_context *ctx, fz_display_list *list);

/**
	Create a new image from a display list.

	w, h: The conceptual width/height of the image.

	transform: The matrix that needs to be applied to the given
	list to make it render to the unit square.

	list: The display list.
*/
fz_image *fz_new_image_from_display_list(fz_context *ctx, float w, float h, fz_display_list *list);

/**
	Check for a display list being empty

	list: The list to check.

	Returns true if empty, false otherwise.
*/
int fz_display_list_is_empty(fz_context *ctx, const fz_display_list *list);

#endif

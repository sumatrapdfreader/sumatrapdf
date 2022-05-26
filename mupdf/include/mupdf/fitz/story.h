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

#ifndef MUPDF_FITZ_STORY_H
#define MUPDF_FITZ_STORY_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"

/*
	This header file provides an API for laying out and placing styled
	text on a page, or pages.

	First a text story is created from some styled HTML.

	Next, this story can be laid out into a given rectangle (possibly
	retrying several times with updated rectangles as required).

	Next, the laid out story can be drawn to a given device.

	In the case where the text story cannot be fitted into the given
	areas all at once, these two steps can be repeated multiple
	times until the text story is completely consumed.

	Finally, the text story can be dropped in the usual fashion.
*/


typedef struct fz_html_story_s fz_html_story;

/*
	Create a text story using styled html.
*/
fz_html_story *fz_new_html_story(fz_context *ctx, fz_buffer *buf, const char *user_css, float em);

/*
	Place (or continue placing) a story into the supplied rectangle
	'where', updating 'filled' with the actual area that was used.
	Returns zero if all the content fitted, non-zero if there is
	more to fit.

	Subsequent calls will attempt to place the same section of story
	again and again, until the placed story is drawn using fz_draw_story,
	whereupon subsequent calls to fz_place_story will attempt to place
	the unused remainder of the story.
*/
int fz_place_story(fz_context *ctx, fz_html_story *story, fz_rect where, fz_rect *filled);

/*
	Draw the placed story to the given device.

	This moves the point at which subsequent calls to fz_place_story
	will restart placing to the end of what has just been output.
*/
void fz_draw_story(fz_context *ctx, fz_html_story *story, fz_device *dev, fz_matrix ctm);

/*
	Drop the html story.
*/
void fz_drop_html_story(fz_context *ctx, fz_html_story *story);

/*
	Get a borrowed reference to the DOM document pointer for this
	story. Do not destroy this reference, it will be destroyed
	when the story is laid out.

	This only makes sense before the first placement of the story.
	Once the story is placed, the DOM representation is destroyed.
*/
fz_xml *fz_html_story_document(fz_context *ctx, fz_html_story *story);

#endif

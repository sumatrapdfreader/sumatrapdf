// Copyright (C) 2022-2024 Artifex Software, Inc.
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

#ifndef MUPDF_FITZ_STORY_WRITER_H
#define MUPDF_FITZ_STORY_WRITER_H

#include "mupdf/fitz/story.h"
#include "mupdf/fitz/writer.h"

/*
 * A fz_story_element_position plus page number information; used with
 * fz_write_story() and fz_write_stabilized_story().
 */
typedef struct
{
	fz_story_element_position element;
	int page_num;
} fz_write_story_position;

/*
 * A set of fz_write_story_position items; used with
 * fz_write_stabilized_story().
 */
typedef struct
{
	fz_write_story_position *positions;
	int num;
} fz_write_story_positions;


/*
 * Callback type used by fz_write_story() and fz_write_stabilized_story().
 *
 * Should set *rect to rect number <num>. If this is on a new page should also
 * set *mediabox and return 1, otherwise return 0.
 *
 *  ref:
 *      As passed to fz_write_story() or fz_write_stabilized_story().
 *  num:
 *      The rect number. Will typically increment by one each time, being reset
 *      to zero when fz_write_stabilized_story() starts a new iteration.
 *  filled:
 *      From earlier internal call to fz_place_story().
 *  rect:
 *      Out param.
 *  ctm:
 *      Out param, defaults to fz_identity.
 *  mediabox:
 *      Out param, only used if we return 1.
 */
typedef int (fz_write_story_rectfn)(fz_context *ctx, void *ref, int num, fz_rect filled, fz_rect *rect, fz_matrix *ctm, fz_rect *mediabox);

/*
 * Callback used by fz_write_story() to report information about element
 * positions. Slightly different from fz_story_position_callback() because
 * <position> also includes the page number.
 *
 *  ref:
 *      As passed to fz_write_story() or fz_write_stabilized_story().
 *  position:
 *      Called via internal call to fz_story_position_callback().
 */
typedef void (fz_write_story_positionfn)(fz_context *ctx, void *ref, const fz_write_story_position *position);

/*
 * Callback for fz_write_story(), called twice for each page, before (after=0)
 * and after (after=1) the story is written.
 *
 *  ref:
 *      As passed to fz_write_story() or fz_write_stabilized_story().
 *  page_num:
 *      Page number, starting from 1.
 *  mediabox:
 *      As returned from fz_write_story_rectfn().
 *  dev:
 *      Created from the fz_writer passed to fz_write_story() or
 *      fz_write_stabilized_story().
 *  after:
 *      0 - before writing the story.
 *      1 - after writing the story.
 */
typedef void (fz_write_story_pagefn)(fz_context *ctx, void *ref, int page_num, fz_rect mediabox, fz_device *dev, int after);

/*
 * Callback type for fz_write_stabilized_story().
 *
 * Should populate the supplied buffer with html content for use with internal
 * calls to fz_new_story(). This may include extra content derived from
 * information in <positions>, for example a table of contents.
 *
 *  ref:
 *      As passed to fz_write_stabilized_story().
 *  positions:
 *      Information from previous iteration.
 *  buffer:
 *      Where to write the new content. Will be initially empty.
 */
typedef void (fz_write_story_contentfn)(fz_context *ctx, void *ref, const fz_write_story_positions *positions, fz_buffer *buffer);


/*
 * Places and writes a story to a fz_document_writer. Avoids the need
 * for calling code to implement a loop that calls fz_place_story()
 * and fz_draw_story() etc, at the expense of having to provide a
 * fz_write_story_rectfn() callback.
 *
 *  story:
 *      The story to place and write.
 *  writer:
 *      Where to write the story; can be NULL.
 *  rectfn:
 *      Should return information about the rect to be used in the next
 *      internal call to fz_place_story().
 *  rectfn_ref:
 *      Passed to rectfn().
 *  positionfn:
 *      If not NULL, is called via internal calls to fz_story_positions().
 *  positionfn_ref:
 *      Passed to positionfn().
 *  pagefn:
 *      If not NULL, called at start and end of each page (before and after all
 *      story content has been written to the device).
 *  pagefn_ref:
 *      Passed to pagefn().
 */
void fz_write_story(
		fz_context *ctx,
		fz_document_writer *writer,
		fz_story *story,
		fz_write_story_rectfn rectfn,
		void *rectfn_ref,
		fz_write_story_positionfn positionfn,
		void *positionfn_ref,
		fz_write_story_pagefn pagefn,
		void *pagefn_ref
		);


/*
 * Does iterative layout of html content to a fz_document_writer. For example
 * this allows one to add a table of contents section while ensuring that page
 * numbers are patched up until stable.
 *
 * Repeatedly creates new story from (contentfn(), contentfn_ref, user_css, em)
 * and lays it out with internal call to fz_write_story(); uses a NULL writer
 * and populates a fz_write_story_positions which is passed to the next call of
 * contentfn().
 *
 * When the html from contentfn() becomes unchanged, we do a final iteration
 * using <writer>.
 *
 *  writer:
 *      Where to write in the final iteration.
 *  user_css:
 *      Used in internal calls to fz_new_story().
 *  em:
 *      Used in internal calls to fz_new_story().
 *  contentfn:
 *      Should return html content for use with fz_new_story(), possibly
 *      including extra content such as a table-of-contents.
 *  contentfn_ref:
 *      Passed to contentfn().
 *  rectfn:
 *      Should return information about the rect to be used in the next
 *      internal call to fz_place_story().
 *  rectfn_ref:
 *      Passed to rectfn().
 *  fz_write_story_pagefn:
 *      If not NULL, called at start and end of each page (before and after all
 *      story content has been written to the device).
 *  pagefn_ref:
 *      Passed to pagefn().
 *  dir:
 *      NULL, or a directory context to load images etc from.
 */
void fz_write_stabilized_story(
		fz_context *ctx,
		fz_document_writer *writer,
		const char *user_css,
		float em,
		fz_write_story_contentfn contentfn,
		void *contentfn_ref,
		fz_write_story_rectfn rectfn,
		void *rectfn_ref,
		fz_write_story_pagefn pagefn,
		void *pagefn_ref,
		fz_archive *dir
		);

#endif

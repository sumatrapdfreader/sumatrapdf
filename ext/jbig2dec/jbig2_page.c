/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <stdlib.h>

#ifdef OUTPUT_PBM
#include <stdio.h>
#endif

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_image.h"
#include "jbig2_page.h"
#include "jbig2_segment.h"

/* dump the page struct info */
static void
dump_page_info(Jbig2Ctx *ctx, Jbig2Segment *segment, Jbig2Page *page)
{
    if (page->x_resolution == 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "page %d image is %dx%d (unknown res)", page->number, page->width, page->height);
    } else if (page->x_resolution == page->y_resolution) {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "page %d image is %dx%d (%d ppm)", page->number, page->width, page->height, page->x_resolution);
    } else {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
                    "page %d image is %dx%d (%dx%d ppm)", page->number, page->width, page->height, page->x_resolution, page->y_resolution);
    }
    if (page->striped) {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "\tmaximum stripe size: %d", page->stripe_size);
    }
}

/**
 * jbig2_page_info: parse page info segment
 *
 * Parse the page info segment data and fill out a corresponding
 * Jbig2Page struct and ready it for subsequent rendered data,
 * including allocating an image buffer for the page (or the first stripe)
 **/
int
jbig2_page_info(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data)
{
    Jbig2Page *page, *pages;

    /* a new page info segment implies the previous page is finished */
    page = &(ctx->pages[ctx->current_page]);
    if (page->number != 0 && (page->state == JBIG2_PAGE_NEW || page->state == JBIG2_PAGE_FREE)) {
        page->state = JBIG2_PAGE_COMPLETE;
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "unexpected page info segment, marking previous page finished");
    }

    /* find a free page */
    {
        int index, j;

        index = ctx->current_page;
        while (ctx->pages[index].state != JBIG2_PAGE_FREE) {
            index++;
            if (index >= ctx->max_page_index) {
                /* grow the list */
                pages = jbig2_renew(ctx, ctx->pages, Jbig2Page, (ctx->max_page_index <<= 2));
                if (pages == NULL) {
                    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to reallocate pages");
                }
                ctx->pages = pages;
                for (j = index; j < ctx->max_page_index; j++) {
                    ctx->pages[j].state = JBIG2_PAGE_FREE;
                    ctx->pages[j].number = 0;
                    ctx->pages[j].image = NULL;
                }
            }
        }
        page = &(ctx->pages[index]);
        ctx->current_page = index;
        page->state = JBIG2_PAGE_NEW;
        page->number = segment->page_association;
    }

    /* FIXME: would be nice if we tried to work around this */
    if (segment->data_length < 19) {
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
    }

    /* 7.4.8.x */
    page->width = jbig2_get_uint32(segment_data);
    page->height = jbig2_get_uint32(segment_data + 4);

    page->x_resolution = jbig2_get_uint32(segment_data + 8);
    page->y_resolution = jbig2_get_uint32(segment_data + 12);
    page->flags = segment_data[16];
    /* Check for T.88 amendment 3 */
    if (page->flags & 0x80)
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "page segment indicates use of color segments (NYI)");

    /* 7.4.8.6 */
    {
        int16_t striping = jbig2_get_int16(segment_data + 17);

        if (striping & 0x8000) {
            page->striped = TRUE;
            page->stripe_size = striping & 0x7FFF;
        } else {
            page->striped = FALSE;
            page->stripe_size = 0;      /* would page->height be better? */
        }
    }
    if (page->height == 0xFFFFFFFF && page->striped == FALSE) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "height is unspecified but page is not marked as striped, assuming striped with maximum strip size");
        page->striped = TRUE;
        page->stripe_size = 0x7FFF;
    }
    page->end_row = 0;

    if (segment->data_length > 19) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "extra data in segment");
    }

    dump_page_info(ctx, segment, page);

    /* allocate an appropriate page image buffer */
    /* 7.4.8.2 */
    if (page->height == 0xFFFFFFFF) {
        page->image = jbig2_image_new(ctx, page->width, page->stripe_size);
    } else {
        page->image = jbig2_image_new(ctx, page->width, page->height);
    }
    if (page->image == NULL) {
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate buffer for page image");
    } else {
        /* 8.2 (3) fill the page with the default pixel value */
        jbig2_image_clear(ctx, page->image, (page->flags & 4));
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
                    "allocated %dx%d page image (%d bytes)", page->image->width, page->image->height, page->image->stride * page->image->height);
    }

    return 0;
}

/**
 * jbig2_end_of_stripe: parse and implement an end of stripe segment
 **/
int
jbig2_end_of_stripe(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data)
{
    Jbig2Page *page = &ctx->pages[ctx->current_page];
    uint32_t end_row;

    if (segment->data_length < 4)
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
    end_row = jbig2_get_uint32(segment_data);
    if (end_row < page->end_row) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
                    "end of stripe segment with non-positive end row advance (new end row %d vs current end row %d)", end_row, page->end_row);
    } else {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "end of stripe: advancing end row from %u to %u", page->end_row, end_row);
    }

    page->end_row = end_row;

    return 0;
}

/**
 * jbig2_complete_page: complete a page image
 *
 * called upon seeing an 'end of page' segment, this routine
 * marks a page as completed so it can be returned.
 * compositing will have already happened in the previous
 * segment handlers.
 **/
int
jbig2_complete_page(Jbig2Ctx *ctx)
{
    int code;

    /* check for unfinished segments */
    if (ctx->segment_index != ctx->n_segments) {
        Jbig2Segment *segment = ctx->segments[ctx->segment_index];

        /* Some versions of Xerox Workcentre generate PDF files
           with the segment data length field of the last segment
           set to -1. Try to cope with this here. */
        if ((segment->data_length & 0xffffffff) == 0xffffffff) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "file has an invalid segment data length; trying to decode using the available data");
            segment->data_length = ctx->buf_wr_ix - ctx->buf_rd_ix;
            code = jbig2_parse_segment(ctx, segment, ctx->buf + ctx->buf_rd_ix);
            ctx->buf_rd_ix += segment->data_length;
            ctx->segment_index++;
            if (code < 0) {
                return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to parse segment");
            }
        }
    }

    /* ensure image exists before marking page as complete */
    if (ctx->pages[ctx->current_page].image == NULL) {
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "page has no image, cannot be completed");
    }

    ctx->pages[ctx->current_page].state = JBIG2_PAGE_COMPLETE;
    return 0;
}

/**
 * jbig2_end_of_page: parse and implement an end of page segment
 **/
int
jbig2_end_of_page(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data)
{
    uint32_t page_number = ctx->pages[ctx->current_page].number;
    int code;

    if (segment->page_association != page_number) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
                    "end of page marker for page %d doesn't match current page number %d", segment->page_association, page_number);
    }

    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "end of page %d", page_number);

    code = jbig2_complete_page(ctx);
    if (code < 0)
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to complete page");

#ifdef OUTPUT_PBM
    code = jbig2_image_write_pbm(ctx->pages[ctx->current_page].image, stdout);
    if (code < 0)
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to write page image");
#endif

    return 0;
}

/**
 * jbig2_add_page_result: composite a decoding result onto a page
 *
 * this is called to add the results of segment decode (when it
 * is an image) to a page image buffer
 **/
int
jbig2_page_add_result(Jbig2Ctx *ctx, Jbig2Page *page, Jbig2Image *image, uint32_t x, uint32_t y, Jbig2ComposeOp op)
{
    int code;

    /* ensure image exists first */
    if (page->image == NULL)
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, -1, "page info possibly missing, no image defined");

    /* grow the page to accommodate a new stripe if necessary */
    if (page->striped && page->height == 0xFFFFFFFF) {
        uint32_t new_height = y + image->height;
        if (page->image->height < new_height) {
            Jbig2Image *resized_image = NULL;

            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, -1, "growing page buffer to %u rows to accommodate new stripe", new_height);
            resized_image = jbig2_image_resize(ctx, page->image, page->image->width, new_height, page->flags & 4);
            if (resized_image == NULL) {
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "unable to resize image to accommodate new stripe");
            }
            page->image = resized_image;
        }
    }

    code = jbig2_image_compose(ctx, page->image, image, x, y, op);
    if (code < 0)
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, -1, "failed to compose image with page");

    return 0;
}

/**
 * jbig2_get_page: return the next available page image buffer
 *
 * the client can call this at any time to check if any pages
 * have been decoded. If so, it returns the first available
 * one. The client should then call jbig2_release_page() when
 * it no longer needs to refer to the image buffer.
 *
 * since this is a public routine for the library clients, we
 * return an image structure pointer, even though the function
 * name refers to a page; the page structure is private.
 **/
Jbig2Image *
jbig2_page_out(Jbig2Ctx *ctx)
{
    int index;

    /* search for a completed page */
    for (index = 0; index < ctx->max_page_index; index++) {
        if (ctx->pages[index].state == JBIG2_PAGE_COMPLETE) {
            Jbig2Image *img = ctx->pages[index].image;
            uint32_t page_number = ctx->pages[index].number;

            if (img == NULL) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, -1, "page %d returned with no associated image", page_number);
                continue;
            }

            ctx->pages[index].state = JBIG2_PAGE_RETURNED;
            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, -1, "page %d returned to the client", page_number);
            return jbig2_image_reference(ctx, img);
        }
    }

    /* no pages available */
    return NULL;
}

/**
 * jbig2_release_page: tell the library a page can be freed
 **/
void
jbig2_release_page(Jbig2Ctx *ctx, Jbig2Image *image)
{
    int index;

    if (image == NULL)
        return;

    /* find the matching page struct and mark it released */
    for (index = 0; index < ctx->max_page_index; index++) {
        if (ctx->pages[index].image == image) {
            jbig2_image_release(ctx, image);
            ctx->pages[index].state = JBIG2_PAGE_RELEASED;
            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, -1, "page %d released by the client", ctx->pages[index].number);
            return;
        }
    }

    /* no matching pages */
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, -1, "failed to release unknown page");
}

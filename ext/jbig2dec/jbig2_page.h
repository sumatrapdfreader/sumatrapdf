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

#ifndef _JBIG2_PAGE_H
#define _JBIG2_PAGE_H

/* the page structure handles decoded page
   results. it's allocated by a 'page info'
   segment and marked complete by an 'end of page'
   segment.
*/
typedef enum {
    JBIG2_PAGE_FREE,
    JBIG2_PAGE_NEW,
    JBIG2_PAGE_COMPLETE,
    JBIG2_PAGE_RETURNED,
    JBIG2_PAGE_RELEASED
} Jbig2PageState;

struct _Jbig2Page {
    Jbig2PageState state;
    uint32_t number;
    uint32_t height, width;     /* in pixels */
    uint32_t x_resolution, y_resolution;        /* in pixels per meter */
    uint16_t stripe_size;
    bool striped;
    uint32_t end_row;
    uint8_t flags;
    Jbig2Image *image;
};

int jbig2_page_info(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);
int jbig2_end_of_stripe(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);
int jbig2_end_of_page(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);
int jbig2_page_add_result(Jbig2Ctx *ctx, Jbig2Page *page, Jbig2Image *src, uint32_t x, uint32_t y, Jbig2ComposeOp op);

#endif /* _JBIG2_PAGE_H */

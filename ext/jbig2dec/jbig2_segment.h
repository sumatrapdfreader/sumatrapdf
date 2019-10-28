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

#ifndef _JBIG2_SEGMENT_H
#define _JBIG2_SEGMENT_H

/* segment header routines */

struct _Jbig2Segment {
    uint32_t number;
    uint8_t flags;
    uint32_t page_association;
    size_t data_length;
    int referred_to_segment_count;
    uint32_t *referred_to_segments;
    uint32_t rows;
    void *result;
};

Jbig2Segment *jbig2_parse_segment_header(Jbig2Ctx *ctx, uint8_t *buf, size_t buf_size, size_t *p_header_size);
int jbig2_parse_segment(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);
void jbig2_free_segment(Jbig2Ctx *ctx, Jbig2Segment *segment);
Jbig2Segment *jbig2_find_segment(Jbig2Ctx *ctx, uint32_t number);

/* region segment info */

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    Jbig2ComposeOp op;
    uint8_t flags;
} Jbig2RegionSegmentInfo;

void jbig2_get_region_segment_info(Jbig2RegionSegmentInfo *info, const uint8_t *segment_data);

#endif /* _JBIG2_SEGMENT_H */

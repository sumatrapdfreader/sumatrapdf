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

#ifndef _JBIG2_REFINEMENT_H
#define _JBIG2_REFINEMENT_H

/* 6.3 Table 6 */
typedef struct {
    /* GRW */
    /* GRH */
    bool GRTEMPLATE;
    Jbig2Image *GRREFERENCE;
    int32_t GRREFERENCEDX, GRREFERENCEDY;
    bool TPGRON;
    int8_t grat[4];
} Jbig2RefinementRegionParams;

int
jbig2_decode_refinement_region(Jbig2Ctx *ctx,
                               Jbig2Segment *segment,
                               const Jbig2RefinementRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats);

/* 7.4 */
int jbig2_refinement_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data);

#endif /* _JBIG2_REFINEMENT_H */


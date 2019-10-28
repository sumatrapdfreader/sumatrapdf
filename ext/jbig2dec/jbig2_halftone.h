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

#ifndef _JBIG2_HALFTONE_H
#define _JBIG2_HALFTONE_H

typedef struct {
    int n_patterns;
    Jbig2Image **patterns;
    int HPW, HPH;
} Jbig2PatternDict;

/* Table 24 */
typedef struct {
    bool HDMMR;
    uint32_t HDPW;
    uint32_t HDPH;
    uint32_t GRAYMAX;
    int HDTEMPLATE;
} Jbig2PatternDictParams;

/* Table 33 */
typedef struct {
    byte flags;
    uint32_t HGW;
    uint32_t HGH;
    int32_t HGX;
    int32_t HGY;
    uint16_t HRX;
    uint16_t HRY;
    bool HMMR;
    int HTEMPLATE;
    bool HENABLESKIP;
    Jbig2ComposeOp HCOMBOP;
    bool HDEFPIXEL;
} Jbig2HalftoneRegionParams;

void jbig2_hd_release(Jbig2Ctx *ctx, Jbig2PatternDict *dict);

int jbig2_halftone_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data);
int jbig2_pattern_dictionary(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data);

#endif /* _JBIG2_HALFTONE_H */

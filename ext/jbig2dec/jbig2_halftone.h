/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
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
  int32_t  HGX;
  int32_t  HGY;
  uint16_t HRX;
  uint16_t HRY;
  bool HMMR;
  int HTEMPLATE;
  bool HENABLESKIP;
  Jbig2ComposeOp op;
  bool HDEFPIXEL;
} Jbig2HalftoneRegionParams;

Jbig2PatternDict* 
jbig2_hd_new(Jbig2Ctx *ctx, const Jbig2PatternDictParams *params, 
             Jbig2Image *image);

void
jbig2_hd_release(Jbig2Ctx *ctx, Jbig2PatternDict *dict);

uint8_t **
jbig2_decode_gray_scale_image(Jbig2Ctx *ctx, Jbig2Segment* segment,
                              const byte *data, const size_t size,
                              bool GSMMR, uint32_t GSW, uint32_t GSH,
                              uint32_t GSBPP, bool GSUSESKIP,
                              Jbig2Image *GSKIP, int GSTEMPLATE,
                              Jbig2ArithCx *GB_stats);

Jbig2PatternDict *
jbig2_decode_ht_region_get_hpats(Jbig2Ctx *ctx, Jbig2Segment *segment);

int
jbig2_decode_halftone_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
                             Jbig2HalftoneRegionParams *params,
                             const byte *data, const size_t size,
                             Jbig2Image *image,
                             Jbig2ArithCx *GB_stats);

#endif /* _JBIG2_HALFTONE_H */

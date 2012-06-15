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


/**
 * Headers for Generic and Generic Refinement region handling
 **/

/* 6.4 Table 2 */
typedef struct {
  bool MMR;
  /* GBW */
  /* GBH */
  int GBTEMPLATE;
  bool TPGDON;
  bool USESKIP;
  /* SKIP */
  int8_t gbat[8];
} Jbig2GenericRegionParams;

/* return the appropriate context size for the given template */
int
jbig2_generic_stats_size(Jbig2Ctx *ctx, int template);

int
jbig2_decode_generic_region(Jbig2Ctx *ctx,
			    Jbig2Segment *segment,
			    const Jbig2GenericRegionParams *params,
			    Jbig2ArithState *as,
			    Jbig2Image *image,
			    Jbig2ArithCx *GB_stats);


/* 6.3 Table 6 */
typedef struct {
  /* GRW */
  /* GRH */
  bool GRTEMPLATE;
  Jbig2Image *reference;
  int32_t DX, DY;
  bool TPGRON;
  int8_t grat[4];
} Jbig2RefinementRegionParams;

int
jbig2_decode_refinement_region(Jbig2Ctx *ctx,
                            Jbig2Segment *segment,
                            const Jbig2RefinementRegionParams *params,
                            Jbig2ArithState *as,
                            Jbig2Image *image,
                            Jbig2ArithCx *GB_stats);

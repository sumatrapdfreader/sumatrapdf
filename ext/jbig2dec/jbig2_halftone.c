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


/* JBIG2 Pattern Dictionary and Halftone Region decoding */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <string.h> /* memset() */

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_generic.h"
#include "jbig2_mmr.h"
#include "jbig2_image.h"
#include "jbig2_halftone.h"

/**
 * jbig2_hd_new: create a new dictionary from a collective bitmap
 */
Jbig2PatternDict *
jbig2_hd_new(Jbig2Ctx *ctx,
		const Jbig2PatternDictParams *params,
		Jbig2Image *image)
{
  Jbig2PatternDict *new;
  const int N = params->GRAYMAX + 1;
  const int HPW = params->HDPW;
  const int HPH = params->HDPH;
  int i;

  /* allocate a new struct */
  new = jbig2_new(ctx, Jbig2PatternDict, 1);
  if (new != NULL) {
    new->patterns = jbig2_new(ctx, Jbig2Image*, N);
    if (new->patterns == NULL) {
      jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
          "failed to allocate pattern in collective bitmap dictionary");
      jbig2_free(ctx->allocator, new);
      return NULL;
    }
    new->n_patterns = N;
    new->HPW = HPW;
    new->HPH = HPH;

    /* 6.7.5(4) - copy out the individual pattern images */
    for (i = 0; i < N; i++) {
      new->patterns[i] = jbig2_image_new(ctx, HPW, HPH);
      if (new->patterns[i] == NULL) {
        int j;
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, -1,
            "failed to allocate pattern element image");
        for (j = 0; j < i; j++)
          jbig2_free(ctx->allocator, new->patterns[j]);
        jbig2_free(ctx->allocator, new);
        return NULL;
      }
      /* compose with the REPLACE operator; the source
         will be clipped to the destintion, selecting the
         proper sub image */
      jbig2_image_compose(ctx, new->patterns[i], image,
			  -i * HPW, 0, JBIG2_COMPOSE_REPLACE);
    }
  }
  else
  {
      jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
          "failed to allocate collective bitmap dictionary");
  }

  return new;
}

/**
 * jbig2_hd_release: release a pattern dictionary
 */
void
jbig2_hd_release(Jbig2Ctx *ctx, Jbig2PatternDict *dict)
{
  int i;

  if (dict == NULL) return;
  for (i = 0; i < dict->n_patterns; i++)
    if (dict->patterns[i]) jbig2_image_release(ctx, dict->patterns[i]);
  jbig2_free(ctx->allocator, dict->patterns);
  jbig2_free(ctx->allocator, dict);
}

/**
 * jbig2_decode_pattern_dict: decode pattern dictionary data
 *
 * @ctx: jbig2 decoder context
 * @segment: jbig2 segment (header) structure
 * @params: parameters from the pattern dictionary header
 * @data: pointer to text region data to be decoded
 * @size: length of text region data
 * @GB_stats: artimetic coding context to use
 *
 * Implements the patten dictionary decoding proceedure
 * described in section 6.7 of the JBIG2 spec.
 *
 * returns: a pointer to the resulting dictionary on success
 * returns: 0 on failure
 **/
static Jbig2PatternDict *
jbig2_decode_pattern_dict(Jbig2Ctx *ctx, Jbig2Segment *segment,
                             const Jbig2PatternDictParams *params,
                             const byte *data, const size_t size,
			     Jbig2ArithCx *GB_stats)
{
  Jbig2PatternDict *hd = NULL;
  Jbig2Image *image = NULL;
  Jbig2GenericRegionParams rparams;
  int code = 0;

  /* allocate the collective image */
  image = jbig2_image_new(ctx,
	params->HDPW * (params->GRAYMAX + 1), params->HDPH);
  if (image == NULL) {
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "failed to allocate collective bitmap for halftone dict!");
    return NULL;
  }

  /* fill out the generic region decoder parameters */
  rparams.MMR = params->HDMMR;
  rparams.GBTEMPLATE = params->HDTEMPLATE;
  rparams.TPGDON = 0;	/* not used if HDMMR = 1 */
  rparams.USESKIP = 0;
  rparams.gbat[0] = -(int8_t)params->HDPW;
  rparams.gbat[1] = 0;
  rparams.gbat[2] = -3;
  rparams.gbat[3] = -1;
  rparams.gbat[4] = 2;
  rparams.gbat[5] = -2;
  rparams.gbat[6] = -2;
  rparams.gbat[7] = -2;

  if (params->HDMMR) {
    code = jbig2_decode_generic_mmr(ctx, segment, &rparams, data, size, image);
  } else {
    Jbig2WordStream *ws = jbig2_word_stream_buf_new(ctx, data, size);
    if (ws != NULL)
    {
      Jbig2ArithState *as = jbig2_arith_new(ctx, ws);
      if (as != NULL)
      {
        code = jbig2_decode_generic_region(ctx, segment, &rparams,
            as, image, GB_stats);
      }
      else
      {
        code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
            "failed to allocate storage for as in halftone dict!");
      }

      jbig2_free(ctx->allocator, as);
      jbig2_word_stream_buf_free(ctx, ws);
    }
    else
    {
      code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
          "failed to allocate storage for ws in halftone dict!");
    }
  }

  if (code == 0) hd = jbig2_hd_new(ctx, params, image);
  jbig2_image_release(ctx, image);

  return hd;
}

/* 7.4.4 */
int
jbig2_pattern_dictionary(Jbig2Ctx *ctx, Jbig2Segment *segment,
			 const byte *segment_data)
{
  Jbig2PatternDictParams params;
  Jbig2ArithCx *GB_stats = NULL;
  byte flags;
  int offset = 0;

  /* 7.4.4.1 - Data header */
  if (segment->data_length < 7) {
    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		       "Segment too short");
  }
  flags = segment_data[0];
  params.HDMMR = flags & 1;
  params.HDTEMPLATE = (flags & 6) >> 1;
  params.HDPW = segment_data[1];
  params.HDPH = segment_data[2];
  params.GRAYMAX = jbig2_get_uint32(segment_data + 3);
  offset += 7;

  jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
	"pattern dictionary, flags=%02x, %d grays (%dx%d cell)",
	flags, params.GRAYMAX + 1, params.HDPW, params.HDPH);

  if (params.HDMMR && params.HDTEMPLATE) {
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
	"HDTEMPLATE is %d when HDMMR is %d, contrary to spec",
	params.HDTEMPLATE, params.HDMMR);
  }
  if (flags & 0xf8) {
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
	"Reserved flag bits non-zero");
  }

  /* 7.4.4.2 */
  if (!params.HDMMR) {
    /* allocate and zero arithmetic coding stats */
    int stats_size = jbig2_generic_stats_size(ctx, params.HDTEMPLATE);
    GB_stats = jbig2_new(ctx, Jbig2ArithCx, stats_size);
    if (GB_stats == NULL)
    {
      jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
          "failed to allocate GB_stats in pattern dictionary");
      return 0;
    }
    memset(GB_stats, 0, stats_size);
  }

  segment->result = jbig2_decode_pattern_dict(ctx, segment, &params,
			segment_data + offset,
			segment->data_length - offset, GB_stats);

  /* todo: retain GB_stats? */
  if (!params.HDMMR) {
    jbig2_free(ctx->allocator, GB_stats);
  }

  return (segment->result != NULL) ? 0 : -1;
}

/**
 * jbig2_decode_gray_scale_image: decode gray-scale image
 *
 * @ctx: jbig2 decoder context
 * @segment: jbig2 segment (header) structure
 * @data: pointer to text region data to be decoded
 * @size: length of text region data
 * @GSMMR: if MMR is used
 * @GSW: width of gray-scale image
 * @GSH: height of gray-scale image
 * @GSBPP: number of bitplanes/Jbig2Images to use 
 * @GSKIP: mask indicating which values should be skipped
 * @GSTEMPLATE: template used to code the gray-scale bitplanes
 * @GB_stats: artimetic coding context to use
 *
 * Implements the decoding a gray-scale image described in
 * annex C.5. This is part of the halftone region decoding.
 *
 * returns: array of gray-scale values with GSW x GSH width/height
 *          0 on failure
 **/
uint8_t **
jbig2_decode_gray_scale_image(Jbig2Ctx *ctx, Jbig2Segment* segment,
                              const byte *data, const size_t size,
                              bool GSMMR, uint32_t GSW, uint32_t GSH, 
                              uint32_t GSBPP, bool GSUSESKIP,
                              Jbig2Image *GSKIP, int GSTEMPLATE,
                              Jbig2ArithCx *GB_stats)
{
  uint8_t **GSVALS = NULL;
  size_t consumed_bytes = 0;
  int i, j, code, stride;
  int x, y;
  Jbig2Image **GSPLANES;
  Jbig2GenericRegionParams rparams;
  Jbig2WordStream *ws = NULL;
  Jbig2ArithState *as = NULL;

  /* allocate GSPLANES */
  GSPLANES = jbig2_new(ctx, Jbig2Image*, GSBPP);
  if (GSPLANES == NULL) {
    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                "failed to allocate %d bytes for GSPLANES", GSBPP);
    return NULL;
  }

  for (i = 0; i < GSBPP; ++i) {
    GSPLANES[i] = jbig2_image_new(ctx, GSW, GSH);
    if (GSPLANES[i] == NULL) {
      jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                  "failed to allocate %dx%d image for GSPLANES", GSW, GSH);
      /* free already allocated */
      for (j = i-1; j >= 0; --j) {
        jbig2_image_release(ctx, GSPLANES[j]);
      }
      jbig2_free(ctx->allocator, GSPLANES);
      return NULL;
    }
  }

  /* C.5 step 1. Decode GSPLANES[GSBPP-1] */ 
  /* fill generic region decoder parameters */
  rparams.MMR = GSMMR;
  rparams.GBTEMPLATE = GSTEMPLATE;
  rparams.TPGDON = 0;
  rparams.USESKIP = GSUSESKIP;
  rparams.gbat[0] = (GSTEMPLATE <= 1? 3 : 2);
  rparams.gbat[1] = -1;
  rparams.gbat[2] = -3;
  rparams.gbat[3] = -1;
  rparams.gbat[4] = 2;
  rparams.gbat[5] = -2;
  rparams.gbat[6] = -2;
  rparams.gbat[7] = -2;

  if (GSMMR) {
    code = jbig2_decode_halftone_mmr(ctx, &rparams, data, size,
                                     GSPLANES[GSBPP-1], &consumed_bytes);
  } else {
    ws = jbig2_word_stream_buf_new(ctx, data, size);
    if (ws == NULL)
    {
      jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
        "failed to allocate ws in jbig2_decode_gray_scale_image");
      goto cleanup;
    }

    as = jbig2_arith_new(ctx, ws);
    if (as == NULL)
    {
      jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
        "failed to allocate as in jbig2_decode_gray_scale_image");
      goto cleanup;
    }

    code = jbig2_decode_generic_region(ctx, segment, &rparams, as,
                                       GSPLANES[GSBPP-1], GB_stats);

  }
  if (code != 0) {
    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                "error decoding GSPLANES for halftone image");
    goto cleanup;
  }

  /* C.5 step 2. Set j = GSBPP-2 */ 
  j = GSBPP - 2;
  /* C.5 step 3. decode loop */ 
  while(j >= 0) {
    /*  C.5 step 3. (a) */
    if (GSMMR) {
      code = jbig2_decode_halftone_mmr(ctx, &rparams, data + consumed_bytes,
                                       size - consumed_bytes, GSPLANES[j],
                                       &consumed_bytes);
    } else {
      code = jbig2_decode_generic_region(ctx, segment, &rparams, as,
                                         GSPLANES[j], GB_stats);
    }
    if (code != 0) {
      jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                  "error decoding GSPLANES for halftone image");
      goto cleanup;
    }

    /* C.5 step 3. (b):
     * for each [x,y]
     * GSPLANES[j][x][y] = GSPLANES[j+1][x][y] XOR GSPLANES[j][x][y] */
    stride = GSPLANES[0]->stride;
    for (i=0; i < stride * GSH; ++i)
      GSPLANES[j]->data[i] ^= GSPLANES[j+1]->data[i];

    /*  C.5 step 3. (c) */
    --j;
  }

  /* allocate GSVALS */
  GSVALS = jbig2_new(ctx, uint8_t* , GSW);
  if (GSVALS == NULL) {
    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                "failed to allocate GSVALS: %d bytes", GSW);
    goto cleanup;
  }
  for (i=0; i<GSW; ++i) {
    GSVALS[i] = jbig2_new(ctx, uint8_t , GSH);
    if (GSVALS[i] == NULL) {
      jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                  "failed to allocate GSVALS: %d bytes", GSH * GSW);
      /* free already allocated */
      for (j = i-1; j >= 0; --j) {
        jbig2_free(ctx->allocator, GSVALS[j]);
      }
      jbig2_free(ctx->allocator, GSVALS);
      GSVALS = NULL;
      goto cleanup;
    }
  }

  /*  C.5 step 4.  */
  for (x = 0; x < GSW; ++x) {
    for (y = 0; y < GSH; ++y) {
      GSVALS[x][y] = 0;

      for (j = 0; j < GSBPP; ++j)
        GSVALS[x][y] += jbig2_image_get_pixel(GSPLANES[j], x, y) << j;
    }
  }

cleanup:
  /* free memory */
  if (!GSMMR) {
    jbig2_free(ctx->allocator, as);
    jbig2_word_stream_buf_free(ctx, ws);
  }
  for (i=0; i< GSBPP; ++i)
    jbig2_image_release(ctx, GSPLANES[i]);

  jbig2_free(ctx->allocator, GSPLANES);

  return GSVALS;
}

/**
 * jbig2_decode_ht_region_get_hpats: get pattern dictionary 
 *
 * @ctx: jbig2 decoder context
 * @segment: jbig2 halftone region segment 
 *
 * Returns the first referred pattern dictionary of segment 
 *
 * returns: pattern dictionary
 *          0 if search failed
 **/
Jbig2PatternDict * 
jbig2_decode_ht_region_get_hpats(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
  int index = 0;
  Jbig2PatternDict *pattern_dict = NULL;
  Jbig2Segment *rsegment = NULL;

  /* loop through all referred segments */
  while (!pattern_dict && segment->referred_to_segment_count > index) {
    rsegment = jbig2_find_segment(ctx, segment->referred_to_segments[index]);
    if (rsegment) {
      /* segment type is pattern dictionary and result is not empty */
      if ((rsegment->flags & 0x3f) == 16 && rsegment->result) {
        pattern_dict = (Jbig2PatternDict *) rsegment->result;
        return pattern_dict;
      }
    }
    index++;
  }
  return pattern_dict;
}

/**
 * jbig2_decode_halftone_region: decode a halftone region
 *
 * @ctx: jbig2 decoder context
 * @segment: jbig2 halftone region segment 
 * @params: parameters 
 * @data: pointer to halftone region data to be decoded
 * @size: length of halftone region data
 * @GB_stats: artimetic coding context to use
 *
 * Implements the halftone region decoding proceedure
 * described in section 6.6.5 of the JBIG2 spec.
 *
 * returns: 0 on success
 *         <0 on failure
 **/
int
jbig2_decode_halftone_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
			     Jbig2HalftoneRegionParams *params,
			     const byte *data, const size_t size,
			     Jbig2Image *image,
			     Jbig2ArithCx *GB_stats)
{
  uint32_t HBPP;
  uint32_t HNUMPATS;
  uint8_t **GI;
  Jbig2Image *HSKIP = NULL;
  Jbig2PatternDict * HPATS;
  int i;
  uint32_t mg, ng;
  int32_t x, y;
  uint8_t gray_val;

  /* 6.6.5 point 1. Fill bitmap with HDEFPIXEL */
  memset(image->data, params->HDEFPIXEL, image->stride * image->height);

  /* 6.6.5 point 2. compute HSKIP */
  if (params->HENABLESKIP == 1) {
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
                "unhandled option HENABLESKIP");
  }

  /* 6.6.5 point 3. set HBPP to ceil(log2(HNUMPATS)): 
   * we need the number of patterns used in this region (HNUMPATS)
   * get it from referred pattern dictionary */

  HPATS = jbig2_decode_ht_region_get_hpats(ctx, segment);
  if (!HPATS) {
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
    "no pattern dictionary found, skipping halftone image");
    return -1;
  }
  HNUMPATS = HPATS->n_patterns;

  /* calculate ceil(log2(HNUMPATS)) */
  HBPP = 0; 
  while(HNUMPATS > (1 << ++HBPP));

  /* 6.6.5 point 4. decode gray-scale image as mentioned in annex C */
  GI = jbig2_decode_gray_scale_image(ctx, segment, data, size,
                                     params->HMMR, params->HGW,
                                     params->HGH, HBPP, params->HENABLESKIP,
                                     HSKIP, params->HTEMPLATE,
                                     GB_stats); 

  if (!GI) {
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
    "unable to acquire gray-scale image, skipping halftone image");
    return -1;
  }

  /* 6.6.5 point 5. place patterns with procedure mentioned in 6.6.5.2 */
  for (mg = 0 ; mg < params->HGH ; ++mg) {
    for (ng = 0 ; ng < params->HGW ; ++ng ) {
      x = (params->HGX + mg * params->HRY + ng * params->HRX) >> 8;
      y = (params->HGY + mg * params->HRX - ng * params->HRY) >> 8;

      /* prevent pattern index >= HNUMPATS */
      gray_val = GI[ng][mg];
      if (gray_val >= HNUMPATS) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
                    "gray-scale image uses value %d which larger than pattern dictionary",
                    gray_val);
        /* use highest aviable pattern */
        gray_val = HNUMPATS - 1;
      }
      jbig2_image_compose(ctx, image, HPATS->patterns[gray_val], x, y, params->op);
    }
  }

  /* free GI */
  for (i = 0; i < params->HGW; ++i) {
    jbig2_free(ctx->allocator, GI[i]);
  }
  jbig2_free(ctx->allocator, GI);

  return 0;
}

/**
 * jbig2_halftone_region: read a halftone region segment header
 **/
int
jbig2_halftone_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
  int offset = 0;
  Jbig2RegionSegmentInfo region_info;
  Jbig2HalftoneRegionParams params;
  Jbig2Image *image = NULL;
  Jbig2ArithCx *GB_stats = NULL;
  int code = 0;

  /* 7.4.5.1 */
  if (segment->data_length < 17) goto too_short;
  jbig2_get_region_segment_info(&region_info, segment_data);
  offset += 17;

  if (segment->data_length < 18) goto too_short;

  /* 7.4.5.1.1 */
  params.flags = segment_data[offset];
  params.HMMR = params.flags & 1;
  params.HTEMPLATE = (params.flags & 6) >> 1;
  params.HENABLESKIP = (params.flags & 8) >> 3;
  params.op = (Jbig2ComposeOp)((params.flags & 0x70) >> 4);
  params.HDEFPIXEL = (params.flags &0x80) >> 7;
  offset += 1;

  jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
	"halftone region: %d x %d @ (%x,%d) flags=%02x",
	region_info.width, region_info.height,
        region_info.x, region_info.y, params.flags);

  if (params.HMMR && params.HTEMPLATE) {
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
	"HTEMPLATE is %d when HMMR is %d, contrary to spec",
	params.HTEMPLATE, params.HMMR);
  }
  if (params.HMMR && params.HENABLESKIP) {
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
	"HENABLESKIP is %d when HMMR is %d, contrary to spec",
	params.HENABLESKIP, params.HMMR);
  }

  /* Figure 43 */
  if (segment->data_length - offset < 16) goto too_short;
  params.HGW = jbig2_get_uint32(segment_data + offset);
  params.HGH = jbig2_get_uint32(segment_data + offset + 4);
  params.HGX = jbig2_get_int32(segment_data + offset + 8);
  params.HGY = jbig2_get_int32(segment_data + offset + 12);
  offset += 16;

  /* Figure 44 */
  if (segment->data_length - offset < 4) goto too_short;
  params.HRX = jbig2_get_uint16(segment_data + offset);
  params.HRY = jbig2_get_uint16(segment_data + offset + 2);
  offset += 4;

  jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
	" grid %d x %d @ (%d.%d,%d.%d) vector (%d.%d,%d.%d)",
	params.HGW, params.HGH,
	params.HGX >> 8, params.HGX & 0xff,
	params.HGY >> 8, params.HGY & 0xff,
	params.HRX >> 8, params.HRX & 0xff,
	params.HRY >> 8, params.HRY & 0xff);

  /* 7.4.5.2.2 */
  if (!params.HMMR) {
    /* allocate and zero arithmetic coding stats */
    int stats_size = jbig2_generic_stats_size(ctx, params.HTEMPLATE);
    GB_stats = jbig2_new(ctx, Jbig2ArithCx, stats_size);
    if (GB_stats == NULL)
    {
      return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
          "failed to allocate GB_stats in halftone region");
    }
    memset(GB_stats, 0, stats_size);
  }

  image = jbig2_image_new(ctx, region_info.width, region_info.height);
  if (image == NULL)
  {
    jbig2_free(ctx->allocator, GB_stats);
    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "unable to allocate halftone image");
  }

  code = jbig2_decode_halftone_region(ctx, segment, &params,
		segment_data + offset, segment->data_length - offset,
		image, GB_stats);

  /* todo: retain GB_stats? */
  if (!params.HMMR) {
    jbig2_free(ctx->allocator, GB_stats);
  }

  jbig2_page_add_result(ctx, &ctx->pages[ctx->current_page],
			image, region_info.x, region_info.y, region_info.op);
  jbig2_image_release(ctx, image);

  return code;

too_short:
    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                       "Segment too short");
}

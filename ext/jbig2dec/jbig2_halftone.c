/*
    jbig2dec

    Copyright (C) 2005 Artifex Software, Inc.

    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.

    For further licensing information refer to http://artifex.com/ or
    contact Artifex Software, Inc., 7 Mt. Lassen Drive - Suite A-134,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.
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
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
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
  Jbig2Image *image;
  Jbig2GenericRegionParams rparams;
  int code;

  /* allocate the collective image */
  image = jbig2_image_new(ctx,
	params->HDPW * (params->GRAYMAX + 1), params->HDPH);
  if (image == NULL) {
    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
	"failed to allocate collective bitmap for halftone dict!");
    return NULL;
  }

  /* fill out the generic region decoder parameters */
  rparams.MMR = params->HDMMR;
  rparams.GBTEMPLATE = params->HDTEMPLATE;
  rparams.TPGDON = 0;	/* not used if HDMMR = 1 */
  rparams.USESKIP = 0;
  rparams.gbat[0] = -params->HDPW;
  rparams.gbat[1] = 0;
  rparams.gbat[2] = -3;
  rparams.gbat[3] = -1;
  rparams.gbat[4] = 2;
  rparams.gbat[5] = -2;
  rparams.gbat[6] = -2;
  rparams.gbat[7] = -2;

  if (params->HDMMR) {
    code = jbig2_decode_generic_mmr(ctx, segment, &rparams,
		data, size, image);
  } else {
    Jbig2WordStream *ws = jbig2_word_stream_buf_new(ctx, data, size);
    Jbig2ArithState *as = jbig2_arith_new(ctx, ws);

    code = jbig2_decode_generic_region(ctx, segment, &rparams,
		as, image, GB_stats);

    jbig2_free(ctx->allocator, as);
    jbig2_word_stream_buf_free(ctx, ws);
  }
  if (code != 0) {
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
	"error decoding collective pattern dictionary bitmap!");
  }

  hd = jbig2_hd_new(ctx, params, image);
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
  params.GRAYMAX = jbig2_get_int32(segment_data + 3);
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
    memset(GB_stats, 0, stats_size);
  }

  segment->result = jbig2_decode_pattern_dict(ctx, segment, &params,
			segment_data + offset,
			segment->data_length - offset, GB_stats);

  /* todo: retain GB_stats? */
  if (!params.HDMMR) {
    jbig2_free(ctx->allocator, GB_stats);
  }

  return (segment->result != NULL) ? 0 : 1;
}



/**
 * jbig2_decode_halftone_region: decode a halftone region
 **/
int
jbig2_decode_halftone_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
			     Jbig2HalftoneRegionParams *params,
			     const byte *data, const size_t size,
			     Jbig2Image *image,
			     Jbig2ArithCx *GB_stats)
{
  int code = 0;

  /* todo: implement */
  return code;
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
  Jbig2Image *image;
  Jbig2ArithCx *GB_stats;
  int code;

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
  params.HGW = jbig2_get_int32(segment_data + offset);
  params.HGH = jbig2_get_int32(segment_data + offset + 4);
  params.HGX = jbig2_get_int32(segment_data + offset + 8);
  params.HGY = jbig2_get_int32(segment_data + offset + 12);
  offset += 16;

  /* Figure 44 */
  if (segment->data_length - offset < 4) goto too_short;
  params.HRX = jbig2_get_int16(segment_data + offset);
  params.HRY = jbig2_get_int16(segment_data + offset + 2);
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
    memset(GB_stats, 0, stats_size);
  }

  image = jbig2_image_new(ctx, region_info.width, region_info.height);
  if (image == NULL)
    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
             "unable to allocate halftone image");

  code = jbig2_decode_halftone_region(ctx, segment, &params,
		segment_data + offset, segment->data_length - offset,
		image, GB_stats);

  /* todo: retain GB_stats? */
  if (!params.HMMR) {
    jbig2_free(ctx->allocator, GB_stats);
  }

  return code;

too_short:
    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                       "Segment too short");
}

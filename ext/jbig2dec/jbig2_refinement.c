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
 * Generic Refinement region handlers.
 **/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <stddef.h>
#include <string.h> /* memcpy(), memset() */

#include <stdio.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_generic.h"
#include "jbig2_image.h"

static int
jbig2_decode_refinement_template0(Jbig2Ctx *ctx,
                              Jbig2Segment *segment,
                              const Jbig2RefinementRegionParams *params,
                              Jbig2ArithState *as,
                              Jbig2Image *image,
                              Jbig2ArithCx *GR_stats)
{
  return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
    "refinement region template 0 NYI");
}

static int
jbig2_decode_refinement_template0_unopt(Jbig2Ctx *ctx,
                              Jbig2Segment *segment,
                              const Jbig2RefinementRegionParams *params,
                              Jbig2ArithState *as,
                              Jbig2Image *image,
                              Jbig2ArithCx *GR_stats)
{
  const int GRW = image->width;
  const int GRH = image->height;
  const int dx = params->DX;
  const int dy = params->DY;
  Jbig2Image *ref = params->reference;
  uint32_t CONTEXT;
  int x,y;
  bool bit;

  for (y = 0; y < GRH; y++) {
    for (x = 0; x < GRW; x++) {
      CONTEXT = 0;
      CONTEXT |= jbig2_image_get_pixel(image, x - 1, y + 0) << 0;
      CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 1;
      CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 1) << 2;
      CONTEXT |= jbig2_image_get_pixel(image, x + params->grat[0],
	y + params->grat[1]) << 3;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+1, y-dy+1) << 4;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+0, y-dy+1) << 5;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx-1, y-dy+1) << 6;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+1, y-dy+0) << 7;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+0, y-dy+0) << 8;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx-1, y-dy+0) << 9;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+1, y-dy-1) << 10;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+0, y-dy-1) << 11;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+params->grat[2],
	y-dy+params->grat[3]) << 12;
      bit = jbig2_arith_decode(as, &GR_stats[CONTEXT]);
      jbig2_image_set_pixel(image, x, y, bit);
    }
  }
#ifdef JBIG2_DEBUG_DUMP
  {
    static count = 0;
    char name[32];
    snprintf(name, 32, "refin-%d.pbm", count);
    jbig2_image_write_pbm_file(ref, name);
    snprintf(name, 32, "refout-%d.pbm", count);
    jbig2_image_write_pbm_file(image, name);
    count++;
  }
#endif

  return 0;
}


static int
jbig2_decode_refinement_template1_unopt(Jbig2Ctx *ctx,
                              Jbig2Segment *segment,
                              const Jbig2RefinementRegionParams *params,
                              Jbig2ArithState *as,
                              Jbig2Image *image,
                              Jbig2ArithCx *GR_stats)
{
  const int GRW = image->width;
  const int GRH = image->height;
  const int dx = params->DX;
  const int dy = params->DY;
  Jbig2Image *ref = params->reference;
  uint32_t CONTEXT;
  int x,y;
  bool bit;

  for (y = 0; y < GRH; y++) {
    for (x = 0; x < GRW; x++) {
      CONTEXT = 0;
      CONTEXT |= jbig2_image_get_pixel(image, x - 1, y + 0) << 0;
      CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 1;
      CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 1) << 2;
      CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 3;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+1, y-dy+1) << 4;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+0, y-dy+1) << 5;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+1, y-dy+0) << 6;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+0, y-dy+0) << 7;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx-1, y-dy+0) << 8;
      CONTEXT |= jbig2_image_get_pixel(ref, x-dx+0, y-dy-1) << 9;
      bit = jbig2_arith_decode(as, &GR_stats[CONTEXT]);
      jbig2_image_set_pixel(image, x, y, bit);
    }
  }

#ifdef JBIG2_DEBUG_DUMP
  {
    static count = 0;
    char name[32];
    snprintf(name, 32, "refin-%d.pbm", count);
    jbig2_image_write_pbm_file(ref, name);
    snprintf(name, 32, "refout-%d.pbm", count);
    jbig2_image_write_pbm_file(image, name);
    count++;
  }
#endif

  return 0;
}

static int
jbig2_decode_refinement_template1(Jbig2Ctx *ctx,
                              Jbig2Segment *segment,
                              const Jbig2RefinementRegionParams *params,
                              Jbig2ArithState *as,
                              Jbig2Image *image,
                              Jbig2ArithCx *GR_stats)
{
  const int GRW = image->width;
  const int GRH = image->height;
  const int stride = image->stride;
  const int refstride = params->reference->stride;
  const int dy = params->DY;
  byte *grreg_line = (byte *)image->data;
  byte *grref_line = (byte *)params->reference->data;
  int x,y;

  for (y = 0; y < GRH; y++) {
    const int padded_width = (GRW + 7) & -8;
    uint32_t CONTEXT;
    uint32_t refline_m1; /* previous line of the reference bitmap */
    uint32_t refline_0;  /* current line of the reference bitmap */
    uint32_t refline_1;  /* next line of the reference bitmap */
    uint32_t line_m1;    /* previous line of the decoded bitmap */

    line_m1 = (y >= 1) ? grreg_line[-stride] : 0;
    refline_m1 = ((y-dy) >= 1) ? grref_line[(-1-dy)*stride] << 2: 0;
    refline_0  = (((y-dy) > 0) && ((y-dy) < GRH)) ? grref_line[(0-dy)*stride] << 4 : 0;
    refline_1  = (y < GRH - 1) ? grref_line[(+1-dy)*stride] << 7 : 0;
    CONTEXT = ((line_m1 >> 5) & 0x00e) |
	      ((refline_1 >> 5) & 0x030) |
	      ((refline_0 >> 5) & 0x1c0) |
	      ((refline_m1 >> 5) & 0x200);

    for (x = 0; x < padded_width; x += 8) {
      byte result = 0;
      int x_minor;
      const int minor_width = GRW - x > 8 ? 8 : GRW - x;

      if (y >= 1) {
	line_m1 = (line_m1 << 8) |
	  (x + 8 < GRW ? grreg_line[-stride + (x >> 3) + 1] : 0);
	refline_m1 = (refline_m1 << 8) |
	  (x + 8 < GRW ? grref_line[-refstride + (x >> 3) + 1] << 2 : 0);
      }

      refline_0 = (refline_0 << 8) |
	  (x + 8 < GRW ? grref_line[(x >> 3) + 1] << 4 : 0);

      if (y < GRH - 1)
	refline_1 = (refline_1 << 8) |
	  (x + 8 < GRW ? grref_line[+refstride + (x >> 3) + 1] << 7 : 0);
      else
	refline_1 = 0;

      /* this is the speed critical inner-loop */
      for (x_minor = 0; x_minor < minor_width; x_minor++) {
	bool bit;

	bit = jbig2_arith_decode(as, &GR_stats[CONTEXT]);
	result |= bit << (7 - x_minor);
	CONTEXT = ((CONTEXT & 0x0d6) << 1) | bit |
	  ((line_m1 >> (9 - x_minor)) & 0x002) |
	  ((refline_1 >> (9 - x_minor)) & 0x010) |
	  ((refline_0 >> (9 - x_minor)) & 0x040) |
	  ((refline_m1 >> (9 - x_minor)) & 0x200);
      }

      grreg_line[x>>3] = result;

    }

    grreg_line += stride;
    grref_line += refstride;

  }

  return 0;

}


typedef uint32_t (*ContextBuilder)(const Jbig2RefinementRegionParams *,
Jbig2Image *, int, int);

static int implicit_value( const Jbig2RefinementRegionParams *params, Jbig2Image
*image, int x, int y )
{
  Jbig2Image *ref = params->reference;
  int i = x - params->DX;
  int j = y - params->DY;
  int m = jbig2_image_get_pixel(ref, i, j);
  return (
          (jbig2_image_get_pixel(ref, i - 1, j - 1) == m) &&
          (jbig2_image_get_pixel(ref, i    , j - 1) == m) &&
          (jbig2_image_get_pixel(ref, i + 1, j - 1) == m) &&
          (jbig2_image_get_pixel(ref, i - 1, j    ) == m) &&
          (jbig2_image_get_pixel(ref, i + 1, j    ) == m) &&
          (jbig2_image_get_pixel(ref, i - 1, j + 1) == m) &&
          (jbig2_image_get_pixel(ref, i    , j + 1) == m) &&
          (jbig2_image_get_pixel(ref, i + 1, j + 1) == m)
         )? m : -1;
}

static uint32_t mkctx0( const Jbig2RefinementRegionParams *params, Jbig2Image
*image, int x, int y )
{
  const int dx = params->DX;
  const int dy = params->DY;
  Jbig2Image *ref = params->reference;
  uint32_t CONTEXT;
  CONTEXT  = jbig2_image_get_pixel(image, x - 1, y + 0);
  CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 1;
  CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 1) << 2;
  CONTEXT |= jbig2_image_get_pixel(image, x + params->grat[0], y +
params->grat[1]) << 3;
  CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 1, y - dy + 1) << 4;
  CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy + 1) << 5;
  CONTEXT |= jbig2_image_get_pixel(ref, x - dx - 1, y - dy + 1) << 6;
  CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 1, y - dy + 0) << 7;
  CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy + 0) << 8;
  CONTEXT |= jbig2_image_get_pixel(ref, x - dx - 1, y - dy + 0) << 9;
  CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 1, y - dy - 1) << 10;
  CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy - 1) << 11;
  CONTEXT |= jbig2_image_get_pixel(ref, x - dx + params->grat[2], y - dy +
params->grat[3]) << 12;
  return CONTEXT;
}

static uint32_t mkctx1( const Jbig2RefinementRegionParams *params, Jbig2Image
*image, int x, int y )
{
  const int dx = params->DX;
  const int dy = params->DY;
  Jbig2Image *ref = params->reference;
  uint32_t CONTEXT;
  CONTEXT  = jbig2_image_get_pixel(image, x - 1, y + 0);
  CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 1;
  CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 1) << 2;
  CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 3;
  CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 1, y - dy + 1) << 4;
  CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy + 1) << 5;
  CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 1, y - dy + 0) << 6;
  CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy + 0) << 7;
  CONTEXT |= jbig2_image_get_pixel(ref, x - dx - 1, y - dy + 0) << 8;
  CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy - 1) << 9;
  return CONTEXT;
}

static int jbig2_decode_refinement_TPGRON(const Jbig2RefinementRegionParams
*params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GR_stats)
{
  const int GRW = image->width;
  const int GRH = image->height;
  int x, y, iv, bit, LTP = 0;
  uint32_t start_context = (params->GRTEMPLATE? 0x40   : 0x100);
  ContextBuilder mkctx   = (params->GRTEMPLATE? mkctx1 : mkctx0);

  for (y = 0; y < GRH; y++)
  {
    bit = jbig2_arith_decode(as, &GR_stats[start_context]);
    if (bit < 0) return -1;
    LTP = LTP ^ bit;
    if (!LTP)
    {
      for (x = 0; x < GRW; x++)
      {
        bit = jbig2_arith_decode(as, &GR_stats[mkctx(params, image, x, y)]);
        if (bit < 0) return -1;
        jbig2_image_set_pixel(image, x, y, bit);
      }
    }
    else
    {
      for (x = 0; x < GRW; x++)
      {
        iv = implicit_value(params, image, x, y);
        if (iv < 0)
        {
          bit = jbig2_arith_decode(as, &GR_stats[mkctx(params, image, x, y)]);
          if (bit < 0) return -1;
          jbig2_image_set_pixel(image, x, y, bit);
        }
        else jbig2_image_set_pixel(image, x, y, iv);
      }
    }
  }

  return 0;
}


/**
 * jbig2_decode_refinement_region: Decode a generic refinement region.
 * @ctx: The context for allocation and error reporting.
 * @segment: A segment reference for error reporting.
 * @params: Decoding parameter set.
 * @as: Arithmetic decoder state.
 * @image: Where to store the decoded image.
 * @GR_stats: Arithmetic stats.
 *
 * Decodes a generic refinement region, according to section 6.3.
 * an already allocated Jbig2Image object in @image for the result.
 *
 * Because this API is based on an arithmetic decoding state, it is
 * not suitable for MMR decoding.
 *
 * Return code: 0 on success.
 **/
int
jbig2_decode_refinement_region(Jbig2Ctx *ctx,
			    Jbig2Segment *segment,
			    const Jbig2RefinementRegionParams *params,
			    Jbig2ArithState *as,
			    Jbig2Image *image,
			    Jbig2ArithCx *GR_stats)
{
  {
    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
      "decoding generic refinement region with offset %d,%x, GRTEMPLATE=%d, TPGRON=%d",
      params->DX, params->DY, params->GRTEMPLATE, params->TPGRON);
  }

  if (params->TPGRON)
    return jbig2_decode_refinement_TPGRON(params, as, image, GR_stats);

  if (params->GRTEMPLATE)
    return jbig2_decode_refinement_template1_unopt(ctx, segment, params,
                                             as, image, GR_stats);
  else
    return jbig2_decode_refinement_template0_unopt(ctx, segment, params,
                                             as, image, GR_stats);
}

/**
 * Find the first referred-to intermediate region segment
 * with a non-NULL result for use as a reference image
 */
Jbig2Segment *
jbig2_region_find_referred(Jbig2Ctx *ctx,Jbig2Segment *segment)
{
  const int nsegments = segment->referred_to_segment_count;
  Jbig2Segment *rsegment;
  int index;

  for (index = 0; index < nsegments; index++) {
    rsegment = jbig2_find_segment(ctx,
      segment->referred_to_segments[index]);
    if (rsegment == NULL) {
      jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
        "could not find referred to segment %d",
        segment->referred_to_segments[index]);
      continue;
    }
    switch (rsegment->flags & 63) {
      case 4:  /* intermediate text region */
      case 20: /* intermediate halftone region */
      case 36: /* intermediate generic region */
      case 40: /* intermediate generic refinement region */
        if (rsegment->result) return rsegment;
	break;
      default: /* keep looking */
        break;
    }
  }
  /* no appropriate reference was found. */
  return NULL;
}

/**
 * Handler for generic refinement region segments
 */
int
jbig2_refinement_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
                               const byte *segment_data)
{
  Jbig2RefinementRegionParams params;
  Jbig2RegionSegmentInfo rsi;
  int offset = 0;
  byte seg_flags;
  int code = 0;

  /* 7.4.7 */
  if (segment->data_length < 18)
    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                       "Segment too short");

  jbig2_get_region_segment_info(&rsi, segment_data);
  jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
              "generic region: %d x %d @ (%d, %d), flags = %02x",
              rsi.width, rsi.height, rsi.x, rsi.y, rsi.flags);

  /* 7.4.7.2 */
  seg_flags = segment_data[17];
  params.GRTEMPLATE = seg_flags & 0x01;
  params.TPGRON = seg_flags & 0x02 ? 1 : 0;
  jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
              "segment flags = %02x %s%s", seg_flags,
              params.GRTEMPLATE ? " GRTEMPLATE" :"",
              params.TPGRON ? " TPGRON" : "" );
  if (seg_flags & 0xFC)
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
                "reserved segment flag bits are non-zero");
  offset += 18;

  /* 7.4.7.3 */
  if (!params.GRTEMPLATE) {
    if (segment->data_length < 22)
      return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                         "Segment too short");
    params.grat[0] = segment_data[offset + 0];
    params.grat[1] = segment_data[offset + 1];
    params.grat[2] = segment_data[offset + 2];
    params.grat[3] = segment_data[offset + 3];
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
                   "grat1: (%d, %d) grat2: (%d, %d)",
                   params.grat[0], params.grat[1],
                   params.grat[2], params.grat[3]);
    offset += 4;
  }

  /* 7.4.7.4 - set up the reference image */
  if (segment->referred_to_segment_count) {
    Jbig2Segment *ref;

    ref = jbig2_region_find_referred(ctx, segment);
    if (ref == NULL)
      return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
        "could not find reference bitmap!");
    /* the reference bitmap is the result of a previous
       intermediate region segment; the reference selection
       rules say to use the first one available, and not to
       reuse any intermediate result, so we simply clone it
       and free the original to keep track of this. */
    params.reference = jbig2_image_clone(ctx, (Jbig2Image*)ref->result);
    jbig2_image_release(ctx, (Jbig2Image*)ref->result);
    ref->result = NULL;
    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
      "found reference bitmap in segment %d", ref->number);
  } else {
    /* the reference is just (a subset of) the page buffer */
    params.reference = jbig2_image_clone(ctx,
      ctx->pages[ctx->current_page].image);
    /* TODO: subset the image if appropriate */
  }

  /* 7.4.7.5 */
  params.DX = 0;
  params.DY = 0;
  {
    Jbig2WordStream *ws = NULL;
    Jbig2ArithState *as = NULL;
    Jbig2ArithCx *GR_stats = NULL;
    int stats_size;
    Jbig2Image *image = NULL;

    image = jbig2_image_new(ctx, rsi.width, rsi.height);
    if (image == NULL)
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
            "unable to allocate refinement image");
        goto cleanup;
    }
    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
      "allocated %d x %d image buffer for region decode results",
          rsi.width, rsi.height);

    stats_size = params.GRTEMPLATE ? 1 << 10 : 1 << 13;
    GR_stats = jbig2_new(ctx, Jbig2ArithCx, stats_size);
    if (GR_stats == NULL)
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
            "failed to allocate GR-stats in jbig2_refinement_region");
        goto cleanup;
    }
    memset(GR_stats, 0, stats_size);

    ws = jbig2_word_stream_buf_new(ctx, segment_data + offset,
           segment->data_length - offset);
    if (ws == NULL)
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
            "failed to allocate ws in jbig2_refinement_region");
        goto cleanup;
    }

    as = jbig2_arith_new(ctx, ws);
    if (as == NULL)
    {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1,
            "failed to allocate as in jbig2_refinement_region");
        goto cleanup;
    }

    code = jbig2_decode_refinement_region(ctx, segment, &params,
                              as, image, GR_stats);

    if ((segment->flags & 63) == 40) {
        /* intermediate region. save the result for later */
        segment->result = jbig2_image_clone(ctx, image);
    } else {
	/* immediate region. composite onto the page */
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
            "composing %dx%d decoded refinement region onto page at (%d, %d)",
            rsi.width, rsi.height, rsi.x, rsi.y);
	jbig2_page_add_result(ctx, &ctx->pages[ctx->current_page],
          image, rsi.x, rsi.y, rsi.op);
    }

cleanup:
    jbig2_image_release(ctx, image);
    jbig2_image_release(ctx, params.reference);
    jbig2_free(ctx->allocator, as);
    jbig2_word_stream_buf_free(ctx, ws);
    jbig2_free(ctx->allocator, GR_stats);
  }

  return code;
}

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
 * Generic region handlers.
 **/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <stddef.h>
#include <string.h> /* memcpy(), memset() */

#ifdef OUTPUT_PBM
#include <stdio.h>
#endif

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_image.h"
#include "jbig2_arith.h"
#include "jbig2_generic.h"
#include "jbig2_mmr.h"

/* return the appropriate context size for the given template */
int
jbig2_generic_stats_size(Jbig2Ctx *ctx, int template)
{
  int stats_size = template == 0 ? 1 << 16 :
        template == 1 ? 1 << 1 << 13 : 1 << 10;
  return stats_size;
}


static int
jbig2_decode_generic_template0(Jbig2Ctx *ctx,
			       Jbig2Segment *segment,
			       const Jbig2GenericRegionParams *params,
			       Jbig2ArithState *as,
			       Jbig2Image *image,
			       Jbig2ArithCx *GB_stats)
{
  const int GBW = image->width;
  const int GBH = image->height;
  const int rowstride = image->stride;
  int x, y;
  byte *gbreg_line = (byte *)image->data;

  /* todo: currently we only handle the nominal gbat location */

#ifdef OUTPUT_PBM
  printf("P4\n%d %d\n", GBW, GBH);
#endif

  for (y = 0; y < GBH; y++)
    {
      uint32_t CONTEXT;
      uint32_t line_m1;
      uint32_t line_m2;
      int padded_width = (GBW + 7) & -8;

      line_m1 = (y >= 1) ? gbreg_line[-rowstride] : 0;
      line_m2 = (y >= 2) ? gbreg_line[-(rowstride << 1)] << 6 : 0;
      CONTEXT = (line_m1 & 0x7f0) | (line_m2 & 0xf800);

      /* 6.2.5.7 3d */
      for (x = 0; x < padded_width; x += 8)
	{
	  byte result = 0;
	  int x_minor;
	  int minor_width = GBW - x > 8 ? 8 : GBW - x;

	  if (y >= 1)
	    line_m1 = (line_m1 << 8) |
	      (x + 8 < GBW ? gbreg_line[-rowstride + (x >> 3) + 1] : 0);

	  if (y >= 2)
	    line_m2 = (line_m2 << 8) |
	      (x + 8 < GBW ? gbreg_line[-(rowstride << 1) + (x >> 3) + 1] << 6: 0);

	  /* This is the speed-critical inner loop. */
	  for (x_minor = 0; x_minor < minor_width; x_minor++)
	    {
	      bool bit;

	      bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
	      result |= bit << (7 - x_minor);
	      CONTEXT = ((CONTEXT & 0x7bf7) << 1) | bit |
		((line_m1 >> (7 - x_minor)) & 0x10) |
		((line_m2 >> (7 - x_minor)) & 0x800);
	    }
	  gbreg_line[x >> 3] = result;
	}
#ifdef OUTPUT_PBM
      fwrite(gbreg_line, 1, rowstride, stdout);
#endif
      gbreg_line += rowstride;
    }

  return 0;
}

static int
jbig2_decode_generic_template0_unopt(Jbig2Ctx *ctx,
                               Jbig2Segment *segment,
                               const Jbig2GenericRegionParams *params,
                               Jbig2ArithState *as,
                               Jbig2Image *image,
                               Jbig2ArithCx *GB_stats)
{
  const int GBW = image->width;
  const int GBH = image->height;
  uint32_t CONTEXT;
  int x,y;
  bool bit;

  /* this version is generic and easy to understand, but very slow */

  for (y = 0; y < GBH; y++) {
    for (x = 0; x < GBW; x++) {
      CONTEXT = 0;
      CONTEXT |= jbig2_image_get_pixel(image, x - 1, y) << 0;
      CONTEXT |= jbig2_image_get_pixel(image, x - 2, y) << 1;
      CONTEXT |= jbig2_image_get_pixel(image, x - 3, y) << 2;
      CONTEXT |= jbig2_image_get_pixel(image, x - 4, y) << 3;
      CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0],
	y + params->gbat[1]) << 4;
      CONTEXT |= jbig2_image_get_pixel(image, x + 2, y - 1) << 5;
      CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 6;
      CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 1) << 7;
      CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 8;
      CONTEXT |= jbig2_image_get_pixel(image, x - 2, y - 1) << 9;
      CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[2],
	y + params->gbat[3]) << 10;
      CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[4],
	y + params->gbat[5]) << 11;
      CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 2) << 12;
      CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 2) << 13;
      CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 2) << 14;
      CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[6],
	y + params->gbat[7]) << 15;
      bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
      jbig2_image_set_pixel(image, x, y, bit);
    }
  }
  return 0;
}

static int
jbig2_decode_generic_template1(Jbig2Ctx *ctx,
			       Jbig2Segment *segment,
			       const Jbig2GenericRegionParams *params,
			       Jbig2ArithState *as,
			       Jbig2Image *image,
			       Jbig2ArithCx *GB_stats)
{
  const int GBW = image->width;
  const int GBH = image->height;
  const int rowstride = image->stride;
  int x, y;
  byte *gbreg_line = (byte *)image->data;

  /* todo: currently we only handle the nominal gbat location */

#ifdef OUTPUT_PBM
  printf("P4\n%d %d\n", GBW, GBH);
#endif

  for (y = 0; y < GBH; y++)
    {
      uint32_t CONTEXT;
      uint32_t line_m1;
      uint32_t line_m2;
      int padded_width = (GBW + 7) & -8;

      line_m1 = (y >= 1) ? gbreg_line[-rowstride] : 0;
      line_m2 = (y >= 2) ? gbreg_line[-(rowstride << 1)] << 5 : 0;
      CONTEXT = ((line_m1 >> 1) & 0x1f8) | ((line_m2 >> 1) & 0x1e00);

      /* 6.2.5.7 3d */
      for (x = 0; x < padded_width; x += 8)
	{
	  byte result = 0;
	  int x_minor;
	  int minor_width = GBW - x > 8 ? 8 : GBW - x;

	  if (y >= 1)
	    line_m1 = (line_m1 << 8) |
	      (x + 8 < GBW ? gbreg_line[-rowstride + (x >> 3) + 1] : 0);

	  if (y >= 2)
	    line_m2 = (line_m2 << 8) |
	      (x + 8 < GBW ? gbreg_line[-(rowstride << 1) + (x >> 3) + 1] << 5: 0);

	  /* This is the speed-critical inner loop. */
	  for (x_minor = 0; x_minor < minor_width; x_minor++)
	    {
	      bool bit;

	      bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
	      result |= bit << (7 - x_minor);
	      CONTEXT = ((CONTEXT & 0xefb) << 1) | bit |
		((line_m1 >> (8 - x_minor)) & 0x8) |
		((line_m2 >> (8 - x_minor)) & 0x200);
	    }
	  gbreg_line[x >> 3] = result;
	}
#ifdef OUTPUT_PBM
      fwrite(gbreg_line, 1, rowstride, stdout);
#endif
      gbreg_line += rowstride;
    }

  return 0;
}

static int
jbig2_decode_generic_template2(Jbig2Ctx *ctx,
			       Jbig2Segment *segment,
			       const Jbig2GenericRegionParams *params,
			       Jbig2ArithState *as,
			       Jbig2Image *image,
			       Jbig2ArithCx *GB_stats)
{
  const int GBW = image->width;
  const int GBH = image->height;
  const int rowstride = image->stride;
  int x, y;
  byte *gbreg_line = (byte *)image->data;

  /* todo: currently we only handle the nominal gbat location */

#ifdef OUTPUT_PBM
  printf("P4\n%d %d\n", GBW, GBH);
#endif

  for (y = 0; y < GBH; y++)
    {
      uint32_t CONTEXT;
      uint32_t line_m1;
      uint32_t line_m2;
      int padded_width = (GBW + 7) & -8;

      line_m1 = (y >= 1) ? gbreg_line[-rowstride] : 0;
      line_m2 = (y >= 2) ? gbreg_line[-(rowstride << 1)] << 4 : 0;
      CONTEXT = ((line_m1 >> 3) & 0x7c) | ((line_m2 >> 3) & 0x380);

      /* 6.2.5.7 3d */
      for (x = 0; x < padded_width; x += 8)
	{
	  byte result = 0;
	  int x_minor;
	  int minor_width = GBW - x > 8 ? 8 : GBW - x;

	  if (y >= 1)
	    line_m1 = (line_m1 << 8) |
	      (x + 8 < GBW ? gbreg_line[-rowstride + (x >> 3) + 1] : 0);

	  if (y >= 2)
	    line_m2 = (line_m2 << 8) |
	      (x + 8 < GBW ? gbreg_line[-(rowstride << 1) + (x >> 3) + 1] << 4: 0);

	  /* This is the speed-critical inner loop. */
	  for (x_minor = 0; x_minor < minor_width; x_minor++)
	    {
	      bool bit;

	      bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
	      result |= bit << (7 - x_minor);
	      CONTEXT = ((CONTEXT & 0x1bd) << 1) | bit |
		((line_m1 >> (10 - x_minor)) & 0x4) |
		((line_m2 >> (10 - x_minor)) & 0x80);
	    }
	  gbreg_line[x >> 3] = result;
	}
#ifdef OUTPUT_PBM
      fwrite(gbreg_line, 1, rowstride, stdout);
#endif
      gbreg_line += rowstride;
    }

  return 0;
}

static int
jbig2_decode_generic_template2a(Jbig2Ctx *ctx,
			       Jbig2Segment *segment,
			       const Jbig2GenericRegionParams *params,
			       Jbig2ArithState *as,
			       Jbig2Image *image,
			       Jbig2ArithCx *GB_stats)
{
  const int GBW = image->width;
  const int GBH = image->height;
  const int rowstride = image->stride;
  int x, y;
  byte *gbreg_line = (byte *)image->data;

  /* This is a special case for GBATX1 = 3, GBATY1 = -1 */

#ifdef OUTPUT_PBM
  printf("P4\n%d %d\n", GBW, GBH);
#endif

  for (y = 0; y < GBH; y++)
    {
      uint32_t CONTEXT;
      uint32_t line_m1;
      uint32_t line_m2;
      int padded_width = (GBW + 7) & -8;

      line_m1 = (y >= 1) ? gbreg_line[-rowstride] : 0;
      line_m2 = (y >= 2) ? gbreg_line[-(rowstride << 1)] << 4 : 0;
      CONTEXT = ((line_m1 >> 3) & 0x78) | ((line_m1 >> 2) & 0x4) | ((line_m2 >> 3) & 0x380);

      /* 6.2.5.7 3d */
      for (x = 0; x < padded_width; x += 8)
	{
	  byte result = 0;
	  int x_minor;
	  int minor_width = GBW - x > 8 ? 8 : GBW - x;

	  if (y >= 1)
	    line_m1 = (line_m1 << 8) |
	      (x + 8 < GBW ? gbreg_line[-rowstride + (x >> 3) + 1] : 0);

	  if (y >= 2)
	    line_m2 = (line_m2 << 8) |
	      (x + 8 < GBW ? gbreg_line[-(rowstride << 1) + (x >> 3) + 1] << 4: 0);

	  /* This is the speed-critical inner loop. */
	  for (x_minor = 0; x_minor < minor_width; x_minor++)
	    {
	      bool bit;

	      bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
	      result |= bit << (7 - x_minor);
	      CONTEXT = ((CONTEXT & 0x1b9) << 1) | bit |
		((line_m1 >> (10 - x_minor)) & 0x8) |
		((line_m1 >> (9 - x_minor)) & 0x4) |
		((line_m2 >> (10 - x_minor)) & 0x80);
	    }
	  gbreg_line[x >> 3] = result;
	}
#ifdef OUTPUT_PBM
      fwrite(gbreg_line, 1, rowstride, stdout);
#endif
      gbreg_line += rowstride;
    }

  return 0;
}

static int
jbig2_decode_generic_template3(Jbig2Ctx *ctx,
			       Jbig2Segment *segment,
			       const Jbig2GenericRegionParams *params,
			       Jbig2ArithState *as,
			       Jbig2Image *image,
			       Jbig2ArithCx *GB_stats)
{
  const int GBW = image->width;
  const int GBH = image->height;
  const int rowstride = image->stride;
  byte *gbreg_line = (byte *)image->data;
  int x, y;

  /* this routine only handles the nominal AT location */

#ifdef OUTPUT_PBM
  printf("P4\n%d %d\n", GBW, GBH);
#endif

  for (y = 0; y < GBH; y++)
    {
      uint32_t CONTEXT;
      uint32_t line_m1;
      int padded_width = (GBW + 7) & -8;

      line_m1 = (y >= 1) ? gbreg_line[-rowstride] : 0;
      CONTEXT = (line_m1 >> 1) & 0x3f0;

      /* 6.2.5.7 3d */
      for (x = 0; x < padded_width; x += 8)
	{
	  byte result = 0;
	  int x_minor;
	  int minor_width = GBW - x > 8 ? 8 : GBW - x;

	  if (y >= 1)
	    line_m1 = (line_m1 << 8) |
	      (x + 8 < GBW ? gbreg_line[-rowstride + (x >> 3) + 1] : 0);

	  /* This is the speed-critical inner loop. */
	  for (x_minor = 0; x_minor < minor_width; x_minor++)
	    {
	      bool bit;

	      bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
	      result |= bit << (7 - x_minor);
	      CONTEXT = ((CONTEXT & 0x1f7) << 1) | bit |
		((line_m1 >> (10 - x_minor)) & 0x010);
	    }
	  gbreg_line[x >> 3] = result;
	}
#ifdef OUTPUT_PBM
      fwrite(gbreg_line, 1, rowstride, stdout);
#endif
      gbreg_line += rowstride;
    }

  return 0;
}

static int
jbig2_decode_generic_template3_unopt(Jbig2Ctx *ctx,
                               Jbig2Segment *segment,
                               const Jbig2GenericRegionParams *params,
                               Jbig2ArithState *as,
                               Jbig2Image *image,
                               Jbig2ArithCx *GB_stats)
{
  const int GBW = image->width;
  const int GBH = image->height;
  uint32_t CONTEXT;
  int x,y;
  bool bit;

  /* this version is generic and easy to understand, but very slow */

  for (y = 0; y < GBH; y++) {
    for (x = 0; x < GBW; x++) {
      CONTEXT = 0;
      CONTEXT |= jbig2_image_get_pixel(image, x - 1, y) << 0;
      CONTEXT |= jbig2_image_get_pixel(image, x - 2, y) << 1;
      CONTEXT |= jbig2_image_get_pixel(image, x - 3, y) << 2;
      CONTEXT |= jbig2_image_get_pixel(image, x - 4, y) << 3;
      CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0],
	y + params->gbat[1]) << 4;
      CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 5;
      CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 1) << 6;
      CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 7;
      CONTEXT |= jbig2_image_get_pixel(image, x - 2, y - 1) << 8;
      CONTEXT |= jbig2_image_get_pixel(image, x - 3, y - 1) << 9;
      bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
      jbig2_image_set_pixel(image, x, y, bit);
    }
  }
  return 0;
}

static void
copy_prev_row(Jbig2Image *image, int row)
{
  if (!row) {
    /* no previous row */
    memset( image->data, 0, image->stride );
  } else {
    /* duplicate data from the previous row */
    uint8_t *src = image->data + (row - 1) * image->stride;
    memcpy( src + image->stride, src, image->stride );
  }
}

static int
jbig2_decode_generic_template0_TPGDON(Jbig2Ctx *ctx,
				Jbig2Segment *segment,
				const Jbig2GenericRegionParams *params, 
				Jbig2ArithState *as,
				Jbig2Image *image,
				Jbig2ArithCx *GB_stats)
{
  const int GBW = image->width;
  const int GBH = image->height;
  uint32_t CONTEXT;
  int x, y;
  bool bit;
  int LTP = 0;

  for (y = 0; y < GBH; y++)
  {
    LTP ^= jbig2_arith_decode(as, &GB_stats[0x9B25]);
    if (!LTP) {
      for (x = 0; x < GBW; x++) {
        CONTEXT  = jbig2_image_get_pixel(image, x - 1, y);
        CONTEXT |= jbig2_image_get_pixel(image, x - 2, y) << 1;
        CONTEXT |= jbig2_image_get_pixel(image, x - 3, y) << 2;
        CONTEXT |= jbig2_image_get_pixel(image, x - 4, y) << 3;
        CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0],
					y + params->gbat[1]) << 4;
        CONTEXT |= jbig2_image_get_pixel(image, x + 2, y - 1) << 5;
        CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 6;
        CONTEXT |= jbig2_image_get_pixel(image, x    , y - 1) << 7;
        CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 8;
        CONTEXT |= jbig2_image_get_pixel(image, x - 2, y - 1) << 9;
        CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[2],
					y + params->gbat[3]) << 10;
        CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[4],
					y + params->gbat[5]) << 11;
        CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 2) << 12;
        CONTEXT |= jbig2_image_get_pixel(image, x    , y - 2) << 13;
        CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 2) << 14;
        CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[6],
					y + params->gbat[7]) << 15;
        bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
        jbig2_image_set_pixel(image, x, y, bit);
      }
    } else {
      copy_prev_row(image, y);
    }
  }

  return 0;
}

static int
jbig2_decode_generic_template1_TPGDON(Jbig2Ctx *ctx, 
				Jbig2Segment *segment,
				const Jbig2GenericRegionParams *params, 
				Jbig2ArithState *as,
				Jbig2Image *image,
				Jbig2ArithCx *GB_stats)
{
  const int GBW = image->width;
  const int GBH = image->height;
  uint32_t CONTEXT;
  int x, y;
  bool bit;
  int LTP = 0;

  for (y = 0; y < GBH; y++) {
    LTP ^= jbig2_arith_decode(as, &GB_stats[0x0795]);
    if (!LTP) {
      for (x = 0; x < GBW; x++) {
        CONTEXT  = jbig2_image_get_pixel(image, x - 1, y);
        CONTEXT |= jbig2_image_get_pixel(image, x - 2, y) << 1;
        CONTEXT |= jbig2_image_get_pixel(image, x - 3, y) << 2;
        CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0],
					y + params->gbat[1]) << 3;
        CONTEXT |= jbig2_image_get_pixel(image, x + 2, y - 1) << 4;
        CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 5;
        CONTEXT |= jbig2_image_get_pixel(image, x    , y - 1) << 6;
        CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 7;
        CONTEXT |= jbig2_image_get_pixel(image, x - 2, y - 1) << 8;
        CONTEXT |= jbig2_image_get_pixel(image, x + 2, y - 2) << 9;
        CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 2) << 10;
        CONTEXT |= jbig2_image_get_pixel(image, x    , y - 2) << 11;
        CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 2) << 12;
        bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
        jbig2_image_set_pixel(image, x, y, bit);
      }
    } else {
      copy_prev_row(image, y);
    }
  }

  return 0;
}

static int
jbig2_decode_generic_template2_TPGDON(Jbig2Ctx *ctx, 
				Jbig2Segment *segment,
				const Jbig2GenericRegionParams *params,
				Jbig2ArithState *as,
				Jbig2Image *image,
				Jbig2ArithCx *GB_stats)
{
  const int GBW = image->width;
  const int GBH = image->height;
  uint32_t CONTEXT;
  int x, y;
  bool bit;
  int LTP = 0;

  for (y = 0; y < GBH; y++) {
    LTP ^= jbig2_arith_decode(as, &GB_stats[0xE5]);
    if (!LTP) {
      for (x = 0; x < GBW; x++) {
        CONTEXT  = jbig2_image_get_pixel(image, x - 1, y);
        CONTEXT |= jbig2_image_get_pixel(image, x - 2, y) << 1;
        CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0],
					y + params->gbat[1]) << 2;
        CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 3;
        CONTEXT |= jbig2_image_get_pixel(image, x    , y - 1) << 4;
        CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 5;
        CONTEXT |= jbig2_image_get_pixel(image, x - 2, y - 1) << 6;
        CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 2) << 7;
        CONTEXT |= jbig2_image_get_pixel(image, x    , y - 2) << 8;
        CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 2) << 9;
        bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
        jbig2_image_set_pixel(image, x, y, bit);
      }
    } else {
      copy_prev_row(image, y);
    }
  }

  return 0;
}

static int
jbig2_decode_generic_template3_TPGDON(Jbig2Ctx *ctx, 
				Jbig2Segment *segment,
				const Jbig2GenericRegionParams *params,
				Jbig2ArithState *as,
				Jbig2Image *image,
				Jbig2ArithCx *GB_stats)
{
  const int GBW = image->width;
  const int GBH = image->height;
  uint32_t CONTEXT;
  int x, y;
  bool bit;
  int LTP = 0;

  for (y = 0; y < GBH; y++) {
    LTP ^= jbig2_arith_decode(as, &GB_stats[0x0195]);
    if (!LTP) {
      for (x = 0; x < GBW; x++) {
        CONTEXT  = jbig2_image_get_pixel(image, x - 1, y);
        CONTEXT |= jbig2_image_get_pixel(image, x - 2, y) << 1;
        CONTEXT |= jbig2_image_get_pixel(image, x - 3, y) << 2;
        CONTEXT |= jbig2_image_get_pixel(image, x - 4, y) << 3;
        CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0],
					y + params->gbat[1]) << 4;
        CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 5;
        CONTEXT |= jbig2_image_get_pixel(image, x    , y - 1) << 6;
        CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 7;
        CONTEXT |= jbig2_image_get_pixel(image, x - 2, y - 1) << 8;
        CONTEXT |= jbig2_image_get_pixel(image, x - 3, y - 1) << 9;
        bit = jbig2_arith_decode(as, &GB_stats[CONTEXT]);
        jbig2_image_set_pixel(image, x, y, bit);
      }
    } else {
      copy_prev_row(image, y);
    }
  }

  return 0;
}

static int
jbig2_decode_generic_region_TPGDON(Jbig2Ctx *ctx,
				Jbig2Segment *segment,
				const Jbig2GenericRegionParams *params, 
				Jbig2ArithState *as,
				Jbig2Image *image,
				Jbig2ArithCx *GB_stats)
{
  switch (params->GBTEMPLATE) {
    case 0:
      return jbig2_decode_generic_template0_TPGDON(ctx, segment, 
			params, as, image, GB_stats);
    case 1:
      return jbig2_decode_generic_template1_TPGDON(ctx, segment, 
			params, as, image, GB_stats);
    case 2:
      return jbig2_decode_generic_template2_TPGDON(ctx, segment, 
			params, as, image, GB_stats);
    case 3:
      return jbig2_decode_generic_template3_TPGDON(ctx, segment, 
			params, as, image, GB_stats);
  }

  return -1;
}

/**
 * jbig2_decode_generic_region: Decode a generic region.
 * @ctx: The context for allocation and error reporting.
 * @segment: A segment reference for error reporting.
 * @params: Decoding parameter set.
 * @as: Arithmetic decoder state.
 * @image: Where to store the decoded data.
 * @GB_stats: Arithmetic stats.
 *
 * Decodes a generic region, according to section 6.2. The caller should
 * pass an already allocated Jbig2Image object for @image
 *
 * Because this API is based on an arithmetic decoding state, it is
 * not suitable for MMR decoding.
 *
 * Return code: 0 on success.
 **/
int
jbig2_decode_generic_region(Jbig2Ctx *ctx,
			    Jbig2Segment *segment,
			    const Jbig2GenericRegionParams *params,
			    Jbig2ArithState *as,
			    Jbig2Image *image,
			    Jbig2ArithCx *GB_stats)
{
  const int8_t *gbat = params->gbat;

  if (!params->MMR && params->TPGDON) 
     return jbig2_decode_generic_region_TPGDON(ctx, segment, params, 
		as, image, GB_stats);

  if (!params->MMR && params->GBTEMPLATE == 0) {
    if (gbat[0] == +3 && gbat[1] == -1 &&
        gbat[2] == -3 && gbat[3] == -1 &&
        gbat[4] == +2 && gbat[5] == -2 &&
        gbat[6] == -2 && gbat[7] == -2)
      return jbig2_decode_generic_template0(ctx, segment, params,
                                          as, image, GB_stats);
    else
      return jbig2_decode_generic_template0_unopt(ctx, segment, params,
                                          as, image, GB_stats);
  } else if (!params->MMR && params->GBTEMPLATE == 1)
    return jbig2_decode_generic_template1(ctx, segment, params,
					  as, image, GB_stats);
  else if (!params->MMR && params->GBTEMPLATE == 2)
    {
      if (gbat[0] == 3 && gbat[1] == -1)
	return jbig2_decode_generic_template2a(ctx, segment, params,
					       as, image, GB_stats);
      else
	return jbig2_decode_generic_template2(ctx, segment, params,
                                              as, image, GB_stats);
    }
  else if (!params->MMR && params->GBTEMPLATE == 3) {
   if (gbat[0] == 2 && gbat[1] == -1)
     return jbig2_decode_generic_template3_unopt(ctx, segment, params,
                                         as, image, GB_stats);
   else
     return jbig2_decode_generic_template3_unopt(ctx, segment, params,
                                         as, image, GB_stats);
  }

  {
    int i;
    for (i = 0; i < 8; i++)
      jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
        "gbat[%d] = %d", i, params->gbat[i]);
  }
  jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
	      "decode_generic_region: MMR=%d, GBTEMPLATE=%d NYI",
	      params->MMR, params->GBTEMPLATE);
  return -1;
}

/**
 * Handler for immediate generic region segments
 */
int
jbig2_immediate_generic_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
			       const byte *segment_data)
{
  Jbig2RegionSegmentInfo rsi;
  byte seg_flags;
  int8_t gbat[8];
  int offset;
  int gbat_bytes = 0;
  Jbig2GenericRegionParams params;
  int code = 0;
  Jbig2Image *image = NULL;
  Jbig2WordStream *ws = NULL;
  Jbig2ArithState *as = NULL;
  Jbig2ArithCx *GB_stats = NULL;

  /* 7.4.6 */
  if (segment->data_length < 18)
    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
		       "Segment too short");

  jbig2_get_region_segment_info(&rsi, segment_data);
  jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
	      "generic region: %d x %d @ (%d, %d), flags = %02x",
	      rsi.width, rsi.height, rsi.x, rsi.y, rsi.flags);

  /* 7.4.6.2 */
  seg_flags = segment_data[17];
  jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
	      "segment flags = %02x", seg_flags);
  if ((seg_flags & 1) && (seg_flags & 6))
    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
		"MMR is 1, but GBTEMPLATE is not 0");

  /* 7.4.6.3 */
  if (!(seg_flags & 1))
    {
      gbat_bytes = (seg_flags & 6) ? 2 : 8;
      if (18 + gbat_bytes > segment->data_length)
	return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
			   "Segment too short");
      memcpy(gbat, segment_data + 18, gbat_bytes);
      jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
		  "gbat: %d, %d", gbat[0], gbat[1]);
    }

  offset = 18 + gbat_bytes;

  /* Table 34 */
  params.MMR = seg_flags & 1;
  params.GBTEMPLATE = (seg_flags & 6) >> 1;
  params.TPGDON = (seg_flags & 8) >> 3;
  params.USESKIP = 0;
  memcpy (params.gbat, gbat, gbat_bytes);

  image = jbig2_image_new(ctx, rsi.width, rsi.height);
  if (image == NULL)
    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
             "unable to allocate generic image");
  jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
    "allocated %d x %d image buffer for region decode results",
        rsi.width, rsi.height);

  if (params.MMR)
    {
      code = jbig2_decode_generic_mmr(ctx, segment, &params,
          segment_data + offset, segment->data_length - offset, image);
    }
  else
    {
      int stats_size = jbig2_generic_stats_size(ctx, params.GBTEMPLATE);
      GB_stats = jbig2_new(ctx, Jbig2ArithCx, stats_size);
      if (GB_stats == NULL)
      {
          code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
              "unable to allocate GB_stats in jbig2_immediate_generic_region");
          goto cleanup;
      }
      memset(GB_stats, 0, stats_size);

      ws = jbig2_word_stream_buf_new(ctx, segment_data + offset,
          segment->data_length - offset);
      if (ws == NULL)
      {
          code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
              "unable to allocate ws in jbig2_immediate_generic_region");
          goto cleanup;
      }
      as = jbig2_arith_new(ctx, ws);
      if (as == NULL)
      {
          code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
              "unable to allocate as in jbig2_immediate_generic_region");
          goto cleanup;
      }
      code = jbig2_decode_generic_region(ctx, segment, &params,
					 as, image, GB_stats);
    }

  jbig2_page_add_result(ctx, &ctx->pages[ctx->current_page],
			image, rsi.x, rsi.y, JBIG2_COMPOSE_OR);

cleanup:
  jbig2_free(ctx->allocator, as);
  jbig2_word_stream_buf_free(ctx, ws);
  jbig2_free(ctx->allocator, GB_stats);
  jbig2_image_release(ctx, image);

  return code;
}

/*
 * jdmerge.c
 *
 * This file was part of the Independent JPEG Group's software:
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Modifications:
 * Copyright (C) 2009, 2011, D. R. Commander.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains code for merged upsampling/color conversion.
 *
 * This file combines functions from jdsample.c and jdcolor.c;
 * read those files first to understand what's going on.
 *
 * When the chroma components are to be upsampled by simple replication
 * (ie, box filtering), we can save some work in color conversion by
 * calculating all the output pixels corresponding to a pair of chroma
 * samples at one time.  In the conversion equations
 *	R = Y           + K1 * Cr
 *	G = Y + K2 * Cb + K3 * Cr
 *	B = Y + K4 * Cb
 * only the Y term varies among the group of pixels corresponding to a pair
 * of chroma samples, so the rest of the terms can be calculated just once.
 * At typical sampling ratios, this eliminates half or three-quarters of the
 * multiplications needed for color conversion.
 *
 * This file currently provides implementations for the following cases:
 *	YCbCr => RGB color conversion only.
 *	Sampling ratios of 2h1v or 2h2v.
 *	No scaling needed at upsample time.
 *	Corner-aligned (non-CCIR601) sampling alignment.
 * Other special cases could be added, but in most applications these are
 * the only common cases.  (For uncommon cases we fall back on the more
 * general code in jdsample.c and jdcolor.c.)
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "jsimd.h"
#include "config.h"

#ifdef UPSAMPLE_MERGING_SUPPORTED


/* Private subobject */

typedef struct {
  struct jpeg_upsampler pub;	/* public fields */

  /* Pointer to routine to do actual upsampling/conversion of one row group */
  JMETHOD(void, upmethod, (j_decompress_ptr cinfo,
			   JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
			   JSAMPARRAY output_buf));

  /* Private state for YCC->RGB conversion */
  int * Cr_r_tab;		/* => table for Cr to R conversion */
  int * Cb_b_tab;		/* => table for Cb to B conversion */
  INT32 * Cr_g_tab;		/* => table for Cr to G conversion */
  INT32 * Cb_g_tab;		/* => table for Cb to G conversion */

  /* For 2:1 vertical sampling, we produce two output rows at a time.
   * We need a "spare" row buffer to hold the second output row if the
   * application provides just a one-row buffer; we also use the spare
   * to discard the dummy last row if the image height is odd.
   */
  JSAMPROW spare_row;
  boolean spare_full;		/* T if spare buffer is occupied */

  JDIMENSION out_row_width;	/* samples per output row */
  JDIMENSION rows_to_go;	/* counts rows remaining in image */
} my_upsampler;

typedef my_upsampler * my_upsample_ptr;

#define SCALEBITS	16	/* speediest right-shift on some machines */
#define ONE_HALF	((INT32) 1 << (SCALEBITS-1))
#define FIX(x)		((INT32) ((x) * (1L<<SCALEBITS) + 0.5))


/* Include inline routines for colorspace extensions */

#include "jdmrgext.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE

#define RGB_RED EXT_RGB_RED
#define RGB_GREEN EXT_RGB_GREEN
#define RGB_BLUE EXT_RGB_BLUE
#define RGB_PIXELSIZE EXT_RGB_PIXELSIZE
#define h2v1_merged_upsample_internal extrgb_h2v1_merged_upsample_internal
#define h2v2_merged_upsample_internal extrgb_h2v2_merged_upsample_internal
#include "jdmrgext.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef h2v1_merged_upsample_internal
#undef h2v2_merged_upsample_internal

#define RGB_RED EXT_RGBX_RED
#define RGB_GREEN EXT_RGBX_GREEN
#define RGB_BLUE EXT_RGBX_BLUE
#define RGB_ALPHA 3
#define RGB_PIXELSIZE EXT_RGBX_PIXELSIZE
#define h2v1_merged_upsample_internal extrgbx_h2v1_merged_upsample_internal
#define h2v2_merged_upsample_internal extrgbx_h2v2_merged_upsample_internal
#include "jdmrgext.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef h2v1_merged_upsample_internal
#undef h2v2_merged_upsample_internal

#define RGB_RED EXT_BGR_RED
#define RGB_GREEN EXT_BGR_GREEN
#define RGB_BLUE EXT_BGR_BLUE
#define RGB_PIXELSIZE EXT_BGR_PIXELSIZE
#define h2v1_merged_upsample_internal extbgr_h2v1_merged_upsample_internal
#define h2v2_merged_upsample_internal extbgr_h2v2_merged_upsample_internal
#include "jdmrgext.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_PIXELSIZE
#undef h2v1_merged_upsample_internal
#undef h2v2_merged_upsample_internal

#define RGB_RED EXT_BGRX_RED
#define RGB_GREEN EXT_BGRX_GREEN
#define RGB_BLUE EXT_BGRX_BLUE
#define RGB_ALPHA 3
#define RGB_PIXELSIZE EXT_BGRX_PIXELSIZE
#define h2v1_merged_upsample_internal extbgrx_h2v1_merged_upsample_internal
#define h2v2_merged_upsample_internal extbgrx_h2v2_merged_upsample_internal
#include "jdmrgext.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef h2v1_merged_upsample_internal
#undef h2v2_merged_upsample_internal

#define RGB_RED EXT_XBGR_RED
#define RGB_GREEN EXT_XBGR_GREEN
#define RGB_BLUE EXT_XBGR_BLUE
#define RGB_ALPHA 0
#define RGB_PIXELSIZE EXT_XBGR_PIXELSIZE
#define h2v1_merged_upsample_internal extxbgr_h2v1_merged_upsample_internal
#define h2v2_merged_upsample_internal extxbgr_h2v2_merged_upsample_internal
#include "jdmrgext.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef h2v1_merged_upsample_internal
#undef h2v2_merged_upsample_internal

#define RGB_RED EXT_XRGB_RED
#define RGB_GREEN EXT_XRGB_GREEN
#define RGB_BLUE EXT_XRGB_BLUE
#define RGB_ALPHA 0
#define RGB_PIXELSIZE EXT_XRGB_PIXELSIZE
#define h2v1_merged_upsample_internal extxrgb_h2v1_merged_upsample_internal
#define h2v2_merged_upsample_internal extxrgb_h2v2_merged_upsample_internal
#include "jdmrgext.c"
#undef RGB_RED
#undef RGB_GREEN
#undef RGB_BLUE
#undef RGB_ALPHA
#undef RGB_PIXELSIZE
#undef h2v1_merged_upsample_internal
#undef h2v2_merged_upsample_internal


/*
 * Initialize tables for YCC->RGB colorspace conversion.
 * This is taken directly from jdcolor.c; see that file for more info.
 */

LOCAL(void)
build_ycc_rgb_table (j_decompress_ptr cinfo)
{
  my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
  int i;
  INT32 x;
  SHIFT_TEMPS

  upsample->Cr_r_tab = (int *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				(MAXJSAMPLE+1) * SIZEOF(int));
  upsample->Cb_b_tab = (int *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				(MAXJSAMPLE+1) * SIZEOF(int));
  upsample->Cr_g_tab = (INT32 *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				(MAXJSAMPLE+1) * SIZEOF(INT32));
  upsample->Cb_g_tab = (INT32 *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				(MAXJSAMPLE+1) * SIZEOF(INT32));

  for (i = 0, x = -CENTERJSAMPLE; i <= MAXJSAMPLE; i++, x++) {
    /* i is the actual input pixel value, in the range 0..MAXJSAMPLE */
    /* The Cb or Cr value we are thinking of is x = i - CENTERJSAMPLE */
    /* Cr=>R value is nearest int to 1.40200 * x */
    upsample->Cr_r_tab[i] = (int)
		    RIGHT_SHIFT(FIX(1.40200) * x + ONE_HALF, SCALEBITS);
    /* Cb=>B value is nearest int to 1.77200 * x */
    upsample->Cb_b_tab[i] = (int)
		    RIGHT_SHIFT(FIX(1.77200) * x + ONE_HALF, SCALEBITS);
    /* Cr=>G value is scaled-up -0.71414 * x */
    upsample->Cr_g_tab[i] = (- FIX(0.71414)) * x;
    /* Cb=>G value is scaled-up -0.34414 * x */
    /* We also add in ONE_HALF so that need not do it in inner loop */
    upsample->Cb_g_tab[i] = (- FIX(0.34414)) * x + ONE_HALF;
  }
}


/*
 * Initialize for an upsampling pass.
 */

METHODDEF(void)
start_pass_merged_upsample (j_decompress_ptr cinfo)
{
  my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;

  /* Mark the spare buffer empty */
  upsample->spare_full = FALSE;
  /* Initialize total-height counter for detecting bottom of image */
  upsample->rows_to_go = cinfo->output_height;
}


/*
 * Control routine to do upsampling (and color conversion).
 *
 * The control routine just handles the row buffering considerations.
 */

METHODDEF(void)
merged_2v_upsample (j_decompress_ptr cinfo,
		    JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
		    JDIMENSION in_row_groups_avail,
		    JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
		    JDIMENSION out_rows_avail)
/* 2:1 vertical sampling case: may need a spare row. */
{
  my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;
  JSAMPROW work_ptrs[2];
  JDIMENSION num_rows;		/* number of rows returned to caller */

  if (upsample->spare_full) {
    /* If we have a spare row saved from a previous cycle, just return it. */
    jcopy_sample_rows(& upsample->spare_row, 0, output_buf + *out_row_ctr, 0,
		      1, upsample->out_row_width);
    num_rows = 1;
    upsample->spare_full = FALSE;
  } else {
    /* Figure number of rows to return to caller. */
    num_rows = 2;
    /* Not more than the distance to the end of the image. */
    if (num_rows > upsample->rows_to_go)
      num_rows = upsample->rows_to_go;
    /* And not more than what the client can accept: */
    out_rows_avail -= *out_row_ctr;
    if (num_rows > out_rows_avail)
      num_rows = out_rows_avail;
    /* Create output pointer array for upsampler. */
    work_ptrs[0] = output_buf[*out_row_ctr];
    if (num_rows > 1) {
      work_ptrs[1] = output_buf[*out_row_ctr + 1];
    } else {
      work_ptrs[1] = upsample->spare_row;
      upsample->spare_full = TRUE;
    }
    /* Now do the upsampling. */
    (*upsample->upmethod) (cinfo, input_buf, *in_row_group_ctr, work_ptrs);
  }

  /* Adjust counts */
  *out_row_ctr += num_rows;
  upsample->rows_to_go -= num_rows;
  /* When the buffer is emptied, declare this input row group consumed */
  if (! upsample->spare_full)
    (*in_row_group_ctr)++;
}


METHODDEF(void)
merged_1v_upsample (j_decompress_ptr cinfo,
		    JSAMPIMAGE input_buf, JDIMENSION *in_row_group_ctr,
		    JDIMENSION in_row_groups_avail,
		    JSAMPARRAY output_buf, JDIMENSION *out_row_ctr,
		    JDIMENSION out_rows_avail)
/* 1:1 vertical sampling case: much easier, never need a spare row. */
{
  my_upsample_ptr upsample = (my_upsample_ptr) cinfo->upsample;

  /* Just do the upsampling. */
  (*upsample->upmethod) (cinfo, input_buf, *in_row_group_ctr,
			 output_buf + *out_row_ctr);
  /* Adjust counts */
  (*out_row_ctr)++;
  (*in_row_group_ctr)++;
}


/*
 * These are the routines invoked by the control routines to do
 * the actual upsampling/conversion.  One row group is processed per call.
 *
 * Note: since we may be writing directly into application-supplied buffers,
 * we have to be honest about the output width; we can't assume the buffer
 * has been rounded up to an even width.
 */


/*
 * Upsample and color convert for the case of 2:1 horizontal and 1:1 vertical.
 */

METHODDEF(void)
h2v1_merged_upsample (j_decompress_ptr cinfo,
		      JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
		      JSAMPARRAY output_buf)
{
  switch (cinfo->out_color_space) {
    case JCS_EXT_RGB:
      extrgb_h2v1_merged_upsample_internal(cinfo, input_buf, in_row_group_ctr,
                                           output_buf);
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      extrgbx_h2v1_merged_upsample_internal(cinfo, input_buf, in_row_group_ctr,
                                            output_buf);
      break;
    case JCS_EXT_BGR:
      extbgr_h2v1_merged_upsample_internal(cinfo, input_buf, in_row_group_ctr,
                                           output_buf);
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      extbgrx_h2v1_merged_upsample_internal(cinfo, input_buf, in_row_group_ctr,
                                            output_buf);
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      extxbgr_h2v1_merged_upsample_internal(cinfo, input_buf, in_row_group_ctr,
                                            output_buf);
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      extxrgb_h2v1_merged_upsample_internal(cinfo, input_buf, in_row_group_ctr,
                                            output_buf);
      break;
    default:
      h2v1_merged_upsample_internal(cinfo, input_buf, in_row_group_ctr,
                                    output_buf);
      break;
  }
}


/*
 * Upsample and color convert for the case of 2:1 horizontal and 2:1 vertical.
 */

METHODDEF(void)
h2v2_merged_upsample (j_decompress_ptr cinfo,
		      JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
		      JSAMPARRAY output_buf)
{
  switch (cinfo->out_color_space) {
    case JCS_EXT_RGB:
      extrgb_h2v2_merged_upsample_internal(cinfo, input_buf, in_row_group_ctr,
                                           output_buf);
      break;
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      extrgbx_h2v2_merged_upsample_internal(cinfo, input_buf, in_row_group_ctr,
                                            output_buf);
      break;
    case JCS_EXT_BGR:
      extbgr_h2v2_merged_upsample_internal(cinfo, input_buf, in_row_group_ctr,
                                           output_buf);
      break;
    case JCS_EXT_BGRX:
    case JCS_EXT_BGRA:
      extbgrx_h2v2_merged_upsample_internal(cinfo, input_buf, in_row_group_ctr,
                                            output_buf);
      break;
    case JCS_EXT_XBGR:
    case JCS_EXT_ABGR:
      extxbgr_h2v2_merged_upsample_internal(cinfo, input_buf, in_row_group_ctr,
                                            output_buf);
      break;
    case JCS_EXT_XRGB:
    case JCS_EXT_ARGB:
      extxrgb_h2v2_merged_upsample_internal(cinfo, input_buf, in_row_group_ctr,
                                            output_buf);
      break;
    default:
      h2v2_merged_upsample_internal(cinfo, input_buf, in_row_group_ctr,
                                    output_buf);
      break;
  }
}


/*
 * Module initialization routine for merged upsampling/color conversion.
 *
 * NB: this is called under the conditions determined by use_merged_upsample()
 * in jdmaster.c.  That routine MUST correspond to the actual capabilities
 * of this module; no safety checks are made here.
 */

GLOBAL(void)
jinit_merged_upsampler (j_decompress_ptr cinfo)
{
  my_upsample_ptr upsample;

  upsample = (my_upsample_ptr)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				SIZEOF(my_upsampler));
  cinfo->upsample = (struct jpeg_upsampler *) upsample;
  upsample->pub.start_pass = start_pass_merged_upsample;
  upsample->pub.need_context_rows = FALSE;

  upsample->out_row_width = cinfo->output_width * cinfo->out_color_components;

  if (cinfo->max_v_samp_factor == 2) {
    upsample->pub.upsample = merged_2v_upsample;
    if (jsimd_can_h2v2_merged_upsample())
      upsample->upmethod = jsimd_h2v2_merged_upsample;
    else
      upsample->upmethod = h2v2_merged_upsample;
    /* Allocate a spare row buffer */
    upsample->spare_row = (JSAMPROW)
      (*cinfo->mem->alloc_large) ((j_common_ptr) cinfo, JPOOL_IMAGE,
		(size_t) (upsample->out_row_width * SIZEOF(JSAMPLE)));
  } else {
    upsample->pub.upsample = merged_1v_upsample;
    if (jsimd_can_h2v1_merged_upsample())
      upsample->upmethod = jsimd_h2v1_merged_upsample;
    else
      upsample->upmethod = h2v1_merged_upsample;
    /* No spare row needed */
    upsample->spare_row = NULL;
  }

  build_ycc_rgb_table(cinfo);
}

#endif /* UPSAMPLE_MERGING_SUPPORTED */

/*
 * jccolor.c
 *
 * Copyright (C) 1991-1996, Thomas G. Lane.
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright 2009-2011 D. R. Commander
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains input colorspace conversion routines.
 */

#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "jsimd.h"


/* Private subobject */

typedef struct {
  struct jpeg_color_converter pub; /* public fields */

  /* Private state for RGB->YCC conversion */
  INT32 * rgb_ycc_tab;		/* => table for RGB to YCbCr conversion */
} my_color_converter;

typedef my_color_converter * my_cconvert_ptr;


/**************** RGB -> YCbCr conversion: most common case **************/

/*
 * YCbCr is defined per CCIR 601-1, except that Cb and Cr are
 * normalized to the range 0..MAXJSAMPLE rather than -0.5 .. 0.5.
 * The conversion equations to be implemented are therefore
 *	Y  =  0.29900 * R + 0.58700 * G + 0.11400 * B
 *	Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B  + CENTERJSAMPLE
 *	Cr =  0.50000 * R - 0.41869 * G - 0.08131 * B  + CENTERJSAMPLE
 * (These numbers are derived from TIFF 6.0 section 21, dated 3-June-92.)
 * Note: older versions of the IJG code used a zero offset of MAXJSAMPLE/2,
 * rather than CENTERJSAMPLE, for Cb and Cr.  This gave equal positive and
 * negative swings for Cb/Cr, but meant that grayscale values (Cb=Cr=0)
 * were not represented exactly.  Now we sacrifice exact representation of
 * maximum red and maximum blue in order to get exact grayscales.
 *
 * To avoid floating-point arithmetic, we represent the fractional constants
 * as integers scaled up by 2^16 (about 4 digits precision); we have to divide
 * the products by 2^16, with appropriate rounding, to get the correct answer.
 *
 * For even more speed, we avoid doing any multiplications in the inner loop
 * by precalculating the constants times R,G,B for all possible values.
 * For 8-bit JSAMPLEs this is very reasonable (only 256 entries per table);
 * for 12-bit samples it is still acceptable.  It's not very reasonable for
 * 16-bit samples, but if you want lossless storage you shouldn't be changing
 * colorspace anyway.
 * The CENTERJSAMPLE offsets and the rounding fudge-factor of 0.5 are included
 * in the tables to save adding them separately in the inner loop.
 */

#define SCALEBITS	16	/* speediest right-shift on some machines */
#define CBCR_OFFSET	((INT32) CENTERJSAMPLE << SCALEBITS)
#define ONE_HALF	((INT32) 1 << (SCALEBITS-1))
#define FIX(x)		((INT32) ((x) * (1L<<SCALEBITS) + 0.5))

/* We allocate one big table and divide it up into eight parts, instead of
 * doing eight alloc_small requests.  This lets us use a single table base
 * address, which can be held in a register in the inner loops on many
 * machines (more than can hold all eight addresses, anyway).
 */

#define R_Y_OFF		0			/* offset to R => Y section */
#define G_Y_OFF		(1*(MAXJSAMPLE+1))	/* offset to G => Y section */
#define B_Y_OFF		(2*(MAXJSAMPLE+1))	/* etc. */
#define R_CB_OFF	(3*(MAXJSAMPLE+1))
#define G_CB_OFF	(4*(MAXJSAMPLE+1))
#define B_CB_OFF	(5*(MAXJSAMPLE+1))
#define R_CR_OFF	B_CB_OFF		/* B=>Cb, R=>Cr are the same */
#define G_CR_OFF	(6*(MAXJSAMPLE+1))
#define B_CR_OFF	(7*(MAXJSAMPLE+1))
#define TABLE_SIZE	(8*(MAXJSAMPLE+1))


#if BITS_IN_JSAMPLE == 8

static const unsigned char red_lut[256] = {
  0 , 0 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 3 , 3 , 3 , 4 , 4 , 4 , 4 ,
  5 , 5 , 5 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 8 , 8 , 8 , 9 , 9 , 9 ,
  10, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 13, 14, 14,
  14, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19,
  19, 19, 20, 20, 20, 21, 21, 21, 22, 22, 22, 22, 23, 23, 23, 24,
  24, 24, 25, 25, 25, 25, 26, 26, 26, 27, 27, 27, 28, 28, 28, 28,
  29, 29, 29, 30, 30, 30, 30, 31, 31, 31, 32, 32, 32, 33, 33, 33,
  33, 34, 34, 34, 35, 35, 35, 36, 36, 36, 36, 37, 37, 37, 38, 38,
  38, 39, 39, 39, 39, 40, 40, 40, 41, 41, 41, 42, 42, 42, 42, 43,
  43, 43, 44, 44, 44, 45, 45, 45, 45, 46, 46, 46, 47, 47, 47, 48,
  48, 48, 48, 49, 49, 49, 50, 50, 50, 51, 51, 51, 51, 52, 52, 52,
  53, 53, 53, 54, 54, 54, 54, 55, 55, 55, 56, 56, 56, 57, 57, 57,
  57, 58, 58, 58, 59, 59, 59, 60, 60, 60, 60, 61, 61, 61, 62, 62,
  62, 62, 63, 63, 63, 64, 64, 64, 65, 65, 65, 65, 66, 66, 66, 67,
  67, 67, 68, 68, 68, 68, 69, 69, 69, 70, 70, 70, 71, 71, 71, 71,
  72, 72, 72, 73, 73, 73, 74, 74, 74, 74, 75, 75, 75, 76, 76, 76
};

static const unsigned char green_lut[256] = {
  0  , 1  , 1  , 2  , 2  , 3  , 4  , 4  , 5  , 5  , 6  , 6  ,
  7  , 8  , 8  , 9  , 9  , 10 , 11 , 11 , 12 , 12 , 13 , 14 ,
  14 , 15 , 15 , 16 , 16 , 17 , 18 , 18 , 19 , 19 , 20 , 21 ,
  21 , 22 , 22 , 23 , 23 , 24 , 25 , 25 , 26 , 26 , 27 , 28 ,
  28 , 29 , 29 , 30 , 31 , 31 , 32 , 32 , 33 , 33 , 34 , 35 ,
  35 , 36 , 36 , 37 , 38 , 38 , 39 , 39 , 40 , 41 , 41 , 42 ,
  42 , 43 , 43 , 44 , 45 , 45 , 46 , 46 , 47 , 48 , 48 , 49 ,
  49 , 50 , 50 , 51 , 52 , 52 , 53 , 53 , 54 , 55 , 55 , 56 ,
  56 , 57 , 58 , 58 , 59 , 59 , 60 , 60 , 61 , 62 , 62 , 63 ,
  63 , 64 , 65 , 65 , 66 , 66 , 67 , 68 , 68 , 69 , 69 , 70 ,
  70 , 71 , 72 , 72 , 73 , 73 , 74 , 75 , 75 , 76 , 76 , 77 ,
  77 , 78 , 79 , 79 , 80 , 80 , 81 , 82 , 82 , 83 , 83 , 84 ,
  85 , 85 , 86 , 86 , 87 , 87 , 88 , 89 , 89 , 90 , 90 , 91 ,
  92 , 92 , 93 , 93 , 94 , 95 , 95 , 96 , 96 , 97 , 97 , 98 ,
  99 , 99 , 100, 100, 101, 102, 102, 103, 103, 104, 104, 105,
  106, 106, 107, 107, 108, 109, 109, 110, 110, 111, 112, 112,
  113, 113, 114, 114, 115, 116, 116, 117, 117, 118, 119, 119,
  120, 120, 121, 122, 122, 123, 123, 124, 124, 125, 126, 126,
  127, 127, 128, 129, 129, 130, 130, 131, 131, 132, 133, 133,
  134, 134, 135, 136, 136, 137, 137, 138, 139, 139, 140, 140,
  141, 141, 142, 143, 143, 144, 144, 145, 146, 146, 147, 147,
  148, 149, 149, 150
};

static const unsigned char blue_lut[256] = {
  0 , 0 , 0 , 0 , 0 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 1 , 2 , 2 ,
  2 , 2 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 3 , 3 , 3 , 3 , 3 , 4 ,
  4 , 4 , 4 , 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 5 , 5 , 5 , 5 ,
  5 , 6 , 6 , 6 , 6 , 6 , 6 , 6 , 6 , 6 , 7 , 7 , 7 , 7 , 7 , 7 ,
  7 , 7 , 8 , 8 , 8 , 8 , 8 , 8 , 8 , 8 , 8 , 9 , 9 , 9 , 9 , 9 ,
  9 , 9 , 9 , 9 , 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11,
  11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13,
  13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14,
  15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16,
  16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18,
  18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20,
  20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22,
  22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 23, 24,
  24, 24, 24, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 25,
  26, 26, 26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27,
  27, 27, 28, 28, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29
};

#endif


/*
 * Initialize for RGB->YCC colorspace conversion.
 */

METHODDEF(void)
rgb_ycc_start (j_compress_ptr cinfo)
{
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  INT32 * rgb_ycc_tab;
  INT32 i;

  /* Allocate and fill in the conversion tables. */
  cconvert->rgb_ycc_tab = rgb_ycc_tab = (INT32 *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				(TABLE_SIZE * SIZEOF(INT32)));

  for (i = 0; i <= MAXJSAMPLE; i++) {
    rgb_ycc_tab[i+R_Y_OFF] = FIX(0.29900) * i;
    rgb_ycc_tab[i+G_Y_OFF] = FIX(0.58700) * i;
    rgb_ycc_tab[i+B_Y_OFF] = FIX(0.11400) * i     + ONE_HALF;
    rgb_ycc_tab[i+R_CB_OFF] = (-FIX(0.16874)) * i;
    rgb_ycc_tab[i+G_CB_OFF] = (-FIX(0.33126)) * i;
    /* We use a rounding fudge-factor of 0.5-epsilon for Cb and Cr.
     * This ensures that the maximum output will round to MAXJSAMPLE
     * not MAXJSAMPLE+1, and thus that we don't have to range-limit.
     */
    rgb_ycc_tab[i+B_CB_OFF] = FIX(0.50000) * i    + CBCR_OFFSET + ONE_HALF-1;
/*  B=>Cb and R=>Cr tables are the same
    rgb_ycc_tab[i+R_CR_OFF] = FIX(0.50000) * i    + CBCR_OFFSET + ONE_HALF-1;
*/
    rgb_ycc_tab[i+G_CR_OFF] = (-FIX(0.41869)) * i;
    rgb_ycc_tab[i+B_CR_OFF] = (-FIX(0.08131)) * i;
  }
}


/*
 * Convert some rows of samples to the JPEG colorspace.
 *
 * Note that we change from the application's interleaved-pixel format
 * to our internal noninterleaved, one-plane-per-component format.
 * The input buffer is therefore three times as wide as the output buffer.
 *
 * A starting row offset is provided only for the output buffer.  The caller
 * can easily adjust the passed input_buf value to accommodate any row
 * offset required on that side.
 */

METHODDEF(void)
rgb_ycc_convert (j_compress_ptr cinfo,
		 JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
		 JDIMENSION output_row, int num_rows)
{
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  register int r, g, b;
  register INT32 * ctab = cconvert->rgb_ycc_tab;
  register JSAMPROW inptr;
  register JSAMPROW outptr0, outptr1, outptr2;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->image_width;

  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr0 = output_buf[0][output_row];
    outptr1 = output_buf[1][output_row];
    outptr2 = output_buf[2][output_row];
    output_row++;
    for (col = 0; col < num_cols; col++) {
      r = GETJSAMPLE(inptr[rgb_red[cinfo->in_color_space]]);
      g = GETJSAMPLE(inptr[rgb_green[cinfo->in_color_space]]);
      b = GETJSAMPLE(inptr[rgb_blue[cinfo->in_color_space]]);
      inptr += rgb_pixelsize[cinfo->in_color_space];
      /* If the inputs are 0..MAXJSAMPLE, the outputs of these equations
       * must be too; we do not need an explicit range-limiting operation.
       * Hence the value being shifted is never negative, and we don't
       * need the general RIGHT_SHIFT macro.
       */
      /* Y */
      outptr0[col] = (JSAMPLE)
		((ctab[r+R_Y_OFF] + ctab[g+G_Y_OFF] + ctab[b+B_Y_OFF])
		 >> SCALEBITS);
      /* Cb */
      outptr1[col] = (JSAMPLE)
		((ctab[r+R_CB_OFF] + ctab[g+G_CB_OFF] + ctab[b+B_CB_OFF])
		 >> SCALEBITS);
      /* Cr */
      outptr2[col] = (JSAMPLE)
		((ctab[r+R_CR_OFF] + ctab[g+G_CR_OFF] + ctab[b+B_CR_OFF])
		 >> SCALEBITS);
    }
  }
}


/**************** Cases other than RGB -> YCbCr **************/


/*
 * Convert some rows of samples to the JPEG colorspace.
 * This version handles RGB->grayscale conversion, which is the same
 * as the RGB->Y portion of RGB->YCbCr.
 * We assume rgb_ycc_start has been called (we only use the Y tables).
 */

METHODDEF(void)
rgb_gray_convert (j_compress_ptr cinfo,
		  JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
		  JDIMENSION output_row, int num_rows)
{
  #if BITS_IN_JSAMPLE != 8
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  register INT32 * ctab = cconvert->rgb_ycc_tab;
  #endif
  register JSAMPROW inptr;
  register JSAMPROW outptr;
  JSAMPLE *maxoutptr;
  JDIMENSION num_cols = cinfo->image_width;
  int rindex = rgb_red[cinfo->in_color_space];
  int gindex = rgb_green[cinfo->in_color_space];
  int bindex = rgb_blue[cinfo->in_color_space];
  int rgbstride = rgb_pixelsize[cinfo->in_color_space];

  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr = output_buf[0][output_row];
    maxoutptr = &outptr[num_cols];
    output_row++;
    for (; outptr < maxoutptr; outptr++, inptr += rgbstride) {
      /* Y */
      #if BITS_IN_JSAMPLE == 8
      *outptr = red_lut[inptr[rindex]] + green_lut[inptr[gindex]]
	    + blue_lut[inptr[bindex]];
      #else
      *outptr = (JSAMPLE)
	    ((ctab[GETJSAMPLE(inptr[rindex])+R_Y_OFF]
	     + ctab[GETJSAMPLE(inptr[gindex])+G_Y_OFF]
	     + ctab[GETJSAMPLE(inptr[bindex])+B_Y_OFF])
	     >> SCALEBITS);
      #endif
    }
  }
}


/*
 * Convert some rows of samples to the JPEG colorspace.
 * This version handles Adobe-style CMYK->YCCK conversion,
 * where we convert R=1-C, G=1-M, and B=1-Y to YCbCr using the same
 * conversion as above, while passing K (black) unchanged.
 * We assume rgb_ycc_start has been called.
 */

METHODDEF(void)
cmyk_ycck_convert (j_compress_ptr cinfo,
		   JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
		   JDIMENSION output_row, int num_rows)
{
  my_cconvert_ptr cconvert = (my_cconvert_ptr) cinfo->cconvert;
  register int r, g, b;
  register INT32 * ctab = cconvert->rgb_ycc_tab;
  register JSAMPROW inptr;
  register JSAMPROW outptr0, outptr1, outptr2, outptr3;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->image_width;

  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr0 = output_buf[0][output_row];
    outptr1 = output_buf[1][output_row];
    outptr2 = output_buf[2][output_row];
    outptr3 = output_buf[3][output_row];
    output_row++;
    for (col = 0; col < num_cols; col++) {
      r = MAXJSAMPLE - GETJSAMPLE(inptr[0]);
      g = MAXJSAMPLE - GETJSAMPLE(inptr[1]);
      b = MAXJSAMPLE - GETJSAMPLE(inptr[2]);
      /* K passes through as-is */
      outptr3[col] = inptr[3];	/* don't need GETJSAMPLE here */
      inptr += 4;
      /* If the inputs are 0..MAXJSAMPLE, the outputs of these equations
       * must be too; we do not need an explicit range-limiting operation.
       * Hence the value being shifted is never negative, and we don't
       * need the general RIGHT_SHIFT macro.
       */
      /* Y */
      outptr0[col] = (JSAMPLE)
		((ctab[r+R_Y_OFF] + ctab[g+G_Y_OFF] + ctab[b+B_Y_OFF])
		 >> SCALEBITS);
      /* Cb */
      outptr1[col] = (JSAMPLE)
		((ctab[r+R_CB_OFF] + ctab[g+G_CB_OFF] + ctab[b+B_CB_OFF])
		 >> SCALEBITS);
      /* Cr */
      outptr2[col] = (JSAMPLE)
		((ctab[r+R_CR_OFF] + ctab[g+G_CR_OFF] + ctab[b+B_CR_OFF])
		 >> SCALEBITS);
    }
  }
}


/*
 * Convert some rows of samples to the JPEG colorspace.
 * This version handles grayscale output with no conversion.
 * The source can be either plain grayscale or YCbCr (since Y == gray).
 */

METHODDEF(void)
grayscale_convert (j_compress_ptr cinfo,
		   JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
		   JDIMENSION output_row, int num_rows)
{
  register JSAMPROW inptr;
  register JSAMPROW outptr;
  register JDIMENSION col;
  JDIMENSION num_cols = cinfo->image_width;
  int instride = cinfo->input_components;

  while (--num_rows >= 0) {
    inptr = *input_buf++;
    outptr = output_buf[0][output_row];
    output_row++;
    for (col = 0; col < num_cols; col++) {
      outptr[col] = inptr[0];	/* don't need GETJSAMPLE() here */
      inptr += instride;
    }
  }
}


/*
 * Convert some rows of samples to the JPEG colorspace.
 * This version handles multi-component colorspaces without conversion.
 * We assume input_components == num_components.
 */

METHODDEF(void)
null_convert (j_compress_ptr cinfo,
	      JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
	      JDIMENSION output_row, int num_rows)
{
  register JSAMPROW inptr;
  register JSAMPROW outptr;
  register JDIMENSION col;
  register int ci;
  int nc = cinfo->num_components;
  JDIMENSION num_cols = cinfo->image_width;

  while (--num_rows >= 0) {
    /* It seems fastest to make a separate pass for each component. */
    for (ci = 0; ci < nc; ci++) {
      inptr = *input_buf;
      outptr = output_buf[ci][output_row];
      for (col = 0; col < num_cols; col++) {
	outptr[col] = inptr[ci]; /* don't need GETJSAMPLE() here */
	inptr += nc;
      }
    }
    input_buf++;
    output_row++;
  }
}


/*
 * Empty method for start_pass.
 */

METHODDEF(void)
null_method (j_compress_ptr cinfo)
{
  /* no work needed */
}


/*
 * Module initialization routine for input colorspace conversion.
 */

GLOBAL(void)
jinit_color_converter (j_compress_ptr cinfo)
{
  my_cconvert_ptr cconvert;

  cconvert = (my_cconvert_ptr)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
				SIZEOF(my_color_converter));
  cinfo->cconvert = (struct jpeg_color_converter *) cconvert;
  /* set start_pass to null method until we find out differently */
  cconvert->pub.start_pass = null_method;

  /* Make sure input_components agrees with in_color_space */
  switch (cinfo->in_color_space) {
  case JCS_GRAYSCALE:
    if (cinfo->input_components != 1)
      ERREXIT(cinfo, JERR_BAD_IN_COLORSPACE);
    break;

  case JCS_RGB:
  case JCS_EXT_RGB:
  case JCS_EXT_RGBX:
  case JCS_EXT_BGR:
  case JCS_EXT_BGRX:
  case JCS_EXT_XBGR:
  case JCS_EXT_XRGB:
    if (cinfo->input_components != rgb_pixelsize[cinfo->in_color_space])
      ERREXIT(cinfo, JERR_BAD_IN_COLORSPACE);
    break;

  case JCS_YCbCr:
    if (cinfo->input_components != 3)
      ERREXIT(cinfo, JERR_BAD_IN_COLORSPACE);
    break;

  case JCS_CMYK:
  case JCS_YCCK:
    if (cinfo->input_components != 4)
      ERREXIT(cinfo, JERR_BAD_IN_COLORSPACE);
    break;

  default:			/* JCS_UNKNOWN can be anything */
    if (cinfo->input_components < 1)
      ERREXIT(cinfo, JERR_BAD_IN_COLORSPACE);
    break;
  }

  /* Check num_components, set conversion method based on requested space */
  switch (cinfo->jpeg_color_space) {
  case JCS_GRAYSCALE:
    if (cinfo->num_components != 1)
      ERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    if (cinfo->in_color_space == JCS_GRAYSCALE)
      cconvert->pub.color_convert = grayscale_convert;
    else if (cinfo->in_color_space == JCS_RGB ||
             cinfo->in_color_space == JCS_EXT_RGB ||
             cinfo->in_color_space == JCS_EXT_RGBX ||
             cinfo->in_color_space == JCS_EXT_BGR ||
             cinfo->in_color_space == JCS_EXT_BGRX ||
             cinfo->in_color_space == JCS_EXT_XBGR ||
             cinfo->in_color_space == JCS_EXT_XRGB) {
      cconvert->pub.start_pass = rgb_ycc_start;
      cconvert->pub.color_convert = rgb_gray_convert;
    } else if (cinfo->in_color_space == JCS_YCbCr)
      cconvert->pub.color_convert = grayscale_convert;
    else
      ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  case JCS_RGB:
  case JCS_EXT_RGB:
  case JCS_EXT_RGBX:
  case JCS_EXT_BGR:
  case JCS_EXT_BGRX:
  case JCS_EXT_XBGR:
  case JCS_EXT_XRGB:
    if (cinfo->num_components != 3)
      ERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    if (cinfo->in_color_space == cinfo->jpeg_color_space &&
      rgb_pixelsize[cinfo->in_color_space] == 3)
      cconvert->pub.color_convert = null_convert;
    else
      ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  case JCS_YCbCr:
    if (cinfo->num_components != 3)
      ERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    if (cinfo->in_color_space == JCS_RGB ||
        cinfo->in_color_space == JCS_EXT_RGB ||
        cinfo->in_color_space == JCS_EXT_RGBX ||
        cinfo->in_color_space == JCS_EXT_BGR ||
        cinfo->in_color_space == JCS_EXT_BGRX ||
        cinfo->in_color_space == JCS_EXT_XBGR ||
        cinfo->in_color_space == JCS_EXT_XRGB) {
      if (jsimd_can_rgb_ycc())
        cconvert->pub.color_convert = jsimd_rgb_ycc_convert;
      else {
        cconvert->pub.start_pass = rgb_ycc_start;
        cconvert->pub.color_convert = rgb_ycc_convert;
      }
    } else if (cinfo->in_color_space == JCS_YCbCr)
      cconvert->pub.color_convert = null_convert;
    else
      ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  case JCS_CMYK:
    if (cinfo->num_components != 4)
      ERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    if (cinfo->in_color_space == JCS_CMYK)
      cconvert->pub.color_convert = null_convert;
    else
      ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  case JCS_YCCK:
    if (cinfo->num_components != 4)
      ERREXIT(cinfo, JERR_BAD_J_COLORSPACE);
    if (cinfo->in_color_space == JCS_CMYK) {
      cconvert->pub.start_pass = rgb_ycc_start;
      cconvert->pub.color_convert = cmyk_ycck_convert;
    } else if (cinfo->in_color_space == JCS_YCCK)
      cconvert->pub.color_convert = null_convert;
    else
      ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    break;

  default:			/* allow null conversion of JCS_UNKNOWN */
    if (cinfo->jpeg_color_space != cinfo->in_color_space ||
	cinfo->num_components != cinfo->input_components)
      ERREXIT(cinfo, JERR_CONVERSION_NOTIMPL);
    cconvert->pub.color_convert = null_convert;
    break;
  }
}

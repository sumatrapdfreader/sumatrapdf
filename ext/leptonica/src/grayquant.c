/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -
 -  Redistribution and use in source and binary forms, with or without
 -  modification, are permitted provided that the following conditions
 -  are met:
 -  1. Redistributions of source code must retain the above copyright
 -     notice, this list of conditions and the following disclaimer.
 -  2. Redistributions in binary form must reproduce the above
 -     copyright notice, this list of conditions and the following
 -     disclaimer in the documentation and/or other materials
 -     provided with the distribution.
 -
 -  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 -  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 -  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 -  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ANY
 -  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 -  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 -  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 -  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 -  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 -  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 -  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *====================================================================*/

/*!
 * \file grayquant.c
 * <pre>
 *
 *      Thresholding from 8 bpp to 1 bpp
 *
 *          Floyd-Steinberg dithering to binary
 *              PIX         *pixDitherToBinary()
 *              PIX         *pixDitherToBinarySpec()
 *              static void  ditherToBinaryLow()
 *              void         ditherToBinaryLineLow()
 *
 *          Simple (pixelwise) binarization with fixed threshold
 *              PIX         *pixThresholdToBinary()
 *              static void  thresholdToBinaryLow()
 *              void         thresholdToBinaryLineLow()
 *
 *          Binarization with variable threshold
 *              PIX         *pixVarThresholdToBinary()
 *
 *          Binarization by adaptive mapping
 *              PIX         *pixAdaptThresholdToBinary()
 *              PIX         *pixAdaptThresholdToBinaryGen()
 *
 *          Generate a binary mask from pixels of particular values
 *              PIX         *pixGenerateMaskByValue()
 *              PIX         *pixGenerateMaskByBand()
 *
 *      Thresholding from 8 bpp to 2 bpp
 *
 *          Floyd-Steinberg-like dithering to 2 bpp
 *              PIX         *pixDitherTo2bpp()
 *              PIX         *pixDitherTo2bppSpec()
 *              static void  ditherTo2bppLow()
 *              static void  ditherTo2bppLineLow()
 *              static l_int32  make8To2DitherTables()
 *
 *          Simple (pixelwise) thresholding to 2 bpp with optional cmap
 *              PIX         *pixThresholdTo2bpp()
 *              static void  thresholdTo2bppLow()
 *
 *      Simple (pixelwise) thresholding from 8 bpp to 4 bpp
 *              PIX         *pixThresholdTo4bpp()
 *              static void  thresholdTo4bppLow()
 *
 *      Simple (pixelwise) quantization on 8 bpp grayscale
 *              PIX         *pixThresholdOn8bpp()
 *
 *      Arbitrary (pixelwise) thresholding from 8 bpp to 2, 4 or 8 bpp
 *              PIX         *pixThresholdGrayArb()
 *
 *      Quantization tables for linear thresholds of grayscale images
 *              l_int32     *makeGrayQuantIndexTable()
 *              static l_int32  *makeGrayQuantTargetTable()
 *
 *      Quantization table for arbitrary thresholding of grayscale images
 *              l_int32      makeGrayQuantTableArb()
 *              static l_int32   makeGrayQuantColormapArb()
 *
 *      Thresholding from 32 bpp rgb to 1 bpp
 *      (really color quantization, but it's better placed in this file)
 *              PIX         *pixGenerateMaskByBand32()
 *              PIX         *pixGenerateMaskByDiscr32()
 *
 *      Histogram-based grayscale quantization
 *              PIX         *pixGrayQuantFromHisto()
 *              static l_int32  numaFillCmapFromHisto()
 *
 *      Color quantize grayscale image using existing colormap
 *              PIX         *pixGrayQuantFromCmap()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>
#include "allheaders.h"

static void ditherToBinaryLow(l_uint32 *datad, l_int32 w, l_int32 h,
                              l_int32 wpld, l_uint32 *datas, l_int32 wpls,
                              l_uint32 *bufs1, l_uint32 *bufs2,
                              l_int32 lowerclip, l_int32 upperclip);
static void thresholdToBinaryLow(l_uint32 *datad, l_int32 w, l_int32 h,
                                 l_int32 wpld, l_uint32 *datas, l_int32 d,
                                 l_int32 wpls, l_int32 thresh);
static void ditherTo2bppLow(l_uint32 *datad, l_int32 w, l_int32 h, l_int32 wpld,
                            l_uint32 *datas, l_int32 wpls, l_uint32 *bufs1,
                            l_uint32 *bufs2, l_int32 *tabval, l_int32 *tab38,
                            l_int32   *tab14);
static void ditherTo2bppLineLow(l_uint32 *lined, l_int32 w, l_uint32 *bufs1,
                                l_uint32 *bufs2, l_int32 *tabval,
                                l_int32 *tab38, l_int32 *tab14,
                                l_int32 lastlineflag);
static l_int32 make8To2DitherTables(l_int32 **ptabval, l_int32 **ptab38,
                                    l_int32 **ptab14, l_int32 cliptoblack,
                                    l_int32 cliptowhite);
static void thresholdTo2bppLow(l_uint32 *datad, l_int32 h, l_int32 wpld,
                               l_uint32 *datas, l_int32 wpls, l_int32 *tab);
static void thresholdTo4bppLow(l_uint32 *datad, l_int32 h, l_int32 wpld,
                               l_uint32 *datas, l_int32 wpls, l_int32 *tab);
static l_int32 *makeGrayQuantTargetTable(l_int32 nlevels, l_int32 depth);
static l_int32 makeGrayQuantColormapArb(PIX *pixs, l_int32 *tab,
                                        l_int32 outdepth, PIXCMAP **pcmap);
static l_int32 numaFillCmapFromHisto(NUMA *na, PIXCMAP *cmap,
                                     l_float32 minfract, l_int32 maxsize,
                                     l_int32 **plut);

#ifndef  NO_CONSOLE_IO
#define DEBUG_UNROLLING 0
#endif   /* ~NO_CONSOLE_IO */

/*------------------------------------------------------------------*
 *             Binarization by Floyd-Steinberg dithering            *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixDitherToBinary()
 *
 * \param[in]    pixs
 * \return  pixd dithered binary, or NULL on error
 *
 *  The Floyd-Steinberg error diffusion dithering algorithm
 *  binarizes an 8 bpp grayscale image to a threshold of 128.
 *  If a pixel has a value above 127, it is binarized to white
 *  and the excess below 255 is subtracted from three
 *  neighboring pixels in the fractions 3/8 to i, j+1,
 *  3/8 to i+1, j) and 1/4 to (i+1,j+1, truncating to 0
 *  if necessary.  Likewise, if it the pixel has a value
 *  below 128, it is binarized to black and the excess above 0
 *  is added to the neighboring pixels, truncating to 255 if necessary.
 *
 *  This function differs from straight dithering in that it allows
 *  clipping of grayscale to 0 or 255 if the values are
 *  sufficiently close, without distribution of the excess.
 *  This uses default values to specify the range of lower
 *  and upper values near 0 and 255, rsp that are clipped
 *  to black and white without propagating the excess.
 *  Not propagating the excess has the effect of reducing the
 *  snake patterns in parts of the image that are nearly black or white;
 *  however, it also prevents the attempt to reproduce gray for those values.
 *
 *  The implementation is straightforward.  It uses a pair of
 *  line buffers to avoid changing pixs.  It is about the same speed
 *  as pixDitherToBinaryLUT(), which uses three LUTs.
 */
PIX *
pixDitherToBinary(PIX  *pixs)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("must be 8 bpp for dithering", __func__, NULL);

    return pixDitherToBinarySpec(pixs, DEFAULT_CLIP_LOWER_1,
                                 DEFAULT_CLIP_UPPER_1);
}


/*!
 * \brief   pixDitherToBinarySpec()
 *
 * \param[in]    pixs
 * \param[in]    lowerclip   lower clip distance to black; use 0 for default
 * \param[in]    upperclip   upper clip distance to white; use 0 for default
 * \return  pixd dithered binary, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See comments above in pixDitherToBinary() for details.
 *      (2) The input parameters lowerclip and upperclip specify the range
 *          of lower and upper values (near 0 and 255, rsp) that are
 *          clipped to black and white without propagating the excess.
 *          For that reason, lowerclip and upperclip should be small numbers.
 * </pre>
 */
PIX *
pixDitherToBinarySpec(PIX     *pixs,
                      l_int32  lowerclip,
                      l_int32  upperclip)
{
l_int32    w, h, d, wplt, wpld;
l_uint32  *datat, *datad;
l_uint32  *bufs1, *bufs2;
PIX       *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8)
        return (PIX *)ERROR_PTR("must be 8 bpp for dithering", __func__, NULL);
    if (lowerclip < 0 || lowerclip > 255)
        return (PIX *)ERROR_PTR("invalid value for lowerclip", __func__, NULL);
    if (upperclip < 0 || upperclip > 255)
        return (PIX *)ERROR_PTR("invalid value for upperclip", __func__, NULL);

    if ((pixd = pixCreate(w, h, 1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

        /* Remove colormap if it exists */
    if ((pixt = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE)) == NULL) {
        pixDestroy(&pixd);
        return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);
    }
    datat = pixGetData(pixt);
    wplt = pixGetWpl(pixt);

        /* Two line buffers, 1 for current line and 2 for next line */
    bufs1 = (l_uint32 *)LEPT_CALLOC(wplt, sizeof(l_uint32));
    bufs2 = (l_uint32 *)LEPT_CALLOC(wplt, sizeof(l_uint32));
    if (!bufs1 || !bufs2) {
        LEPT_FREE(bufs1);
        LEPT_FREE(bufs2);
        pixDestroy(&pixd);
        pixDestroy(&pixt);
        return (PIX *)ERROR_PTR("bufs1, bufs2 not both made", __func__, NULL);
    }

    ditherToBinaryLow(datad, w, h, wpld, datat, wplt, bufs1, bufs2,
                      lowerclip, upperclip);

    LEPT_FREE(bufs1);
    LEPT_FREE(bufs2);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   ditherToBinaryLow()
 *
 *  See comments in pixDitherToBinary() in binarize.c
 */
static void
ditherToBinaryLow(l_uint32  *datad,
                  l_int32    w,
                  l_int32    h,
                  l_int32    wpld,
                  l_uint32  *datas,
                  l_int32    wpls,
                  l_uint32  *bufs1,
                  l_uint32  *bufs2,
                  l_int32    lowerclip,
                  l_int32    upperclip)
{
l_int32    i;
l_uint32  *lined;

        /* do all lines except last line */
    memcpy(bufs2, datas, 4 * wpls);  /* prime the buffer */
    for (i = 0; i < h - 1; i++) {
        memcpy(bufs1, bufs2, 4 * wpls);
        memcpy(bufs2, datas + (i + 1) * wpls, 4 * wpls);
        lined = datad + i * wpld;
        ditherToBinaryLineLow(lined, w, bufs1, bufs2, lowerclip, upperclip, 0);
    }

        /* do last line */
    memcpy(bufs1, bufs2, 4 * wpls);
    lined = datad + (h - 1) * wpld;
    ditherToBinaryLineLow(lined, w, bufs1, bufs2, lowerclip, upperclip, 1);
}


/*!
 * \brief   ditherToBinaryLineLow()
 *
 * \param[in]    lined         ptr to beginning of dest line
 * \param[in]    w             width of image in pixels
 * \param[in]    bufs1         buffer of current source line
 * \param[in]    bufs2         buffer of next source line
 * \param[in]    lowerclip     lower clip distance to black
 * \param[in]    upperclip     upper clip distance to white
 * \param[in]    lastlineflag  0 if not last dest line, 1 if last dest line
 * \return  void
 *
 *  Dispatches FS error diffusion dithering for
 *  a single line of the image.  If lastlineflag == 0,
 *  both source buffers are used; otherwise, only bufs1
 *  is used.  We use source buffers because the error
 *  is propagated into them, and we don't want to change
 *  the input src image.
 *
 *  We break dithering out line by line to make it
 *  easier to combine functions like interpolative
 *  scaling and error diffusion dithering, as such a
 *  combination of operations obviates the need to
 *  generate a 2x grayscale image as an intermediary.
 */
void
ditherToBinaryLineLow(l_uint32  *lined,
                      l_int32    w,
                      l_uint32  *bufs1,
                      l_uint32  *bufs2,
                      l_int32    lowerclip,
                      l_int32    upperclip,
                      l_int32    lastlineflag)
{
l_int32   j;
l_int32   oval, eval;
l_uint8   fval1, fval2, rval, bval, dval;

    if (lastlineflag == 0) {
        for (j = 0; j < w - 1; j++) {
            oval = GET_DATA_BYTE(bufs1, j);
            if (oval > 127) {   /* binarize to OFF */
                if ((eval = 255 - oval) > upperclip) {
                        /* subtract from neighbors */
                    fval1 = (3 * eval) / 8;
                    fval2 = eval / 4;
                    rval = GET_DATA_BYTE(bufs1, j + 1);
                    rval = L_MAX(0, rval - fval1);
                    SET_DATA_BYTE(bufs1, j + 1, rval);
                    bval = GET_DATA_BYTE(bufs2, j);
                    bval = L_MAX(0, bval - fval1);
                    SET_DATA_BYTE(bufs2, j, bval);
                    dval = GET_DATA_BYTE(bufs2, j + 1);
                    dval = L_MAX(0, dval - fval2);
                    SET_DATA_BYTE(bufs2, j + 1, dval);
                }
            } else {   /* oval <= 127; binarize to ON  */
                SET_DATA_BIT(lined, j);   /* ON pixel */
                if (oval > lowerclip) {
                        /* add to neighbors */
                    fval1 = (3 * oval) / 8;
                    fval2 = oval / 4;
                    rval = GET_DATA_BYTE(bufs1, j + 1);
                    rval = L_MIN(255, rval + fval1);
                    SET_DATA_BYTE(bufs1, j + 1, rval);
                    bval = GET_DATA_BYTE(bufs2, j);
                    bval = L_MIN(255, bval + fval1);
                    SET_DATA_BYTE(bufs2, j, bval);
                    dval = GET_DATA_BYTE(bufs2, j + 1);
                    dval = L_MIN(255, dval + fval2);
                    SET_DATA_BYTE(bufs2, j + 1, dval);
                }
            }
        }

            /* do last column: j = w - 1 */
        oval = GET_DATA_BYTE(bufs1, j);
        if (oval > 127) {  /* binarize to OFF */
            if ((eval = 255 - oval) > upperclip) {
                    /* subtract from neighbors */
                fval1 = (3 * eval) / 8;
                bval = GET_DATA_BYTE(bufs2, j);
                bval = L_MAX(0, bval - fval1);
                SET_DATA_BYTE(bufs2, j, bval);
            }
        } else {  /*oval <= 127; binarize to ON */
            SET_DATA_BIT(lined, j);   /* ON pixel */
            if (oval > lowerclip) {
                    /* add to neighbors */
                fval1 = (3 * oval) / 8;
                bval = GET_DATA_BYTE(bufs2, j);
                bval = L_MIN(255, bval + fval1);
                SET_DATA_BYTE(bufs2, j, bval);
            }
        }
    } else {   /* lastlineflag == 1 */
        for (j = 0; j < w - 1; j++) {
            oval = GET_DATA_BYTE(bufs1, j);
            if (oval > 127) {   /* binarize to OFF */
                if ((eval = 255 - oval) > upperclip) {
                        /* subtract from neighbors */
                    fval1 = (3 * eval) / 8;
                    rval = GET_DATA_BYTE(bufs1, j + 1);
                    rval = L_MAX(0, rval - fval1);
                    SET_DATA_BYTE(bufs1, j + 1, rval);
                }
            } else {   /* oval <= 127; binarize to ON  */
                SET_DATA_BIT(lined, j);   /* ON pixel */
                if (oval > lowerclip) {
                        /* add to neighbors */
                    fval1 = (3 * oval) / 8;
                    rval = GET_DATA_BYTE(bufs1, j + 1);
                    rval = L_MIN(255, rval + fval1);
                    SET_DATA_BYTE(bufs1, j + 1, rval);
                }
            }
        }

            /* do last pixel: (i, j) = (h - 1, w - 1) */
        oval = GET_DATA_BYTE(bufs1, j);
        if (oval < 128)
            SET_DATA_BIT(lined, j);   /* ON pixel */
    }
}


/*------------------------------------------------------------------*
 *       Simple (pixelwise) binarization with fixed threshold       *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixThresholdToBinary()
 *
 * \param[in]    pixs     4 or 8 bpp
 * \param[in]    thresh   threshold value
 * \return  pixd 1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If the source pixel is less than the threshold value,
 *          the dest will be 1; otherwise, it will be 0.
 *      (2) For example, for 8 bpp src pix, if %thresh == 256, the dest
 *          1 bpp pix is all ones (fg), and if %thresh == 0, the dest
 *          pix is all zeros (bg).
 *
 * </pre>
 */
PIX *
pixThresholdToBinary(PIX     *pixs,
                     l_int32  thresh)
{
l_int32    d, w, h, wplt, wpld;
l_uint32  *datat, *datad;
PIX       *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 4 && d != 8)
        return (PIX *)ERROR_PTR("pixs must be 4 or 8 bpp", __func__, NULL);
    if (thresh < 0)
        return (PIX *)ERROR_PTR("thresh must be non-negative", __func__, NULL);
    if (d == 4 && thresh > 16)
        return (PIX *)ERROR_PTR("4 bpp thresh not in {0-16}", __func__, NULL);
    if (d == 8 && thresh > 256)
        return (PIX *)ERROR_PTR("8 bpp thresh not in {0-256}", __func__, NULL);

    if ((pixd = pixCreate(w, h, 1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

        /* Remove colormap if it exists.  If there is a colormap,
         * pixt will be 8 bpp regardless of the depth of pixs. */
    pixt = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    datat = pixGetData(pixt);
    wplt = pixGetWpl(pixt);
    if (pixGetColormap(pixs) && d == 4) {  /* promoted to 8 bpp */
        d = 8;
        thresh *= 16;
    }

    thresholdToBinaryLow(datad, w, h, wpld, datat, d, wplt, thresh);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   thresholdToBinaryLow()
 *
 *  If the source pixel is less than thresh,
 *  the dest will be 1; otherwise, it will be 0
 */
static void
thresholdToBinaryLow(l_uint32  *datad,
                     l_int32    w,
                     l_int32    h,
                     l_int32    wpld,
                     l_uint32  *datas,
                     l_int32    d,
                     l_int32    wpls,
                     l_int32    thresh)
{
l_int32    i;
l_uint32  *lines, *lined;

    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        thresholdToBinaryLineLow(lined, w, lines, d, thresh);
    }
}


/*
 *  thresholdToBinaryLineLow()
 *
 */
void
thresholdToBinaryLineLow(l_uint32  *lined,
                         l_int32    w,
                         l_uint32  *lines,
                         l_int32    d,
                         l_int32    thresh)
{
l_int32  j, k, gval, scount, dcount;
l_uint32 sword, dword;

    switch (d)
    {
    case 4:
            /* Unrolled as 4 source words, 1 dest word */
        for (j = 0, scount = 0, dcount = 0; j + 31 < w; j += 32) {
            dword = 0;
            for (k = 0; k < 4; k++) {
                sword = lines[scount++];
                dword <<= 8;
                gval = (sword >> 28) & 0xf;
                    /* Trick used here and below: if gval < thresh then
                     * gval - thresh < 0, so its high-order bit is 1, and
                     * ((gval - thresh) >> 31) & 1 == 1; likewise, if
                     * gval >= thresh, then ((gval - thresh) >> 31) & 1 == 0
                     * Doing it this way avoids a random (and thus easily
                     * mispredicted) branch on each pixel. */
                dword |= ((gval - thresh) >> 24) & 128;
                gval = (sword >> 24) & 0xf;
                dword |= ((gval - thresh) >> 25) & 64;
                gval = (sword >> 20) & 0xf;
                dword |= ((gval - thresh) >> 26) & 32;
                gval = (sword >> 16) & 0xf;
                dword |= ((gval - thresh) >> 27) & 16;
                gval = (sword >> 12) & 0xf;
                dword |= ((gval - thresh) >> 28) & 8;
                gval = (sword >> 8) & 0xf;
                dword |= ((gval - thresh) >> 29) & 4;
                gval = (sword >> 4) & 0xf;
                dword |= ((gval - thresh) >> 30) & 2;
                gval = sword & 0xf;
                dword |= ((gval - thresh) >> 31) & 1;
            }
            lined[dcount++] = dword;
        }

        if (j < w) {
          dword = 0;
          for (; j < w; j++) {
              if ((j & 7) == 0) {
                  sword = lines[scount++];
              }
              gval = (sword >> 28) & 0xf;
              sword <<= 4;
              dword |= (((gval - thresh) >> 31) & 1) << (31 - (j & 31));
          }
          lined[dcount] = dword;
        }
#if DEBUG_UNROLLING
#define CHECK_BIT(a, b, c) if (GET_DATA_BIT(a, b) != c) { \
    lept_stderr("Error: mismatch at %d/%d(%d), %d vs %d\n", \
                j, w, d, GET_DATA_BIT(a, b), c); }
        for (j = 0; j < w; j++) {
            gval = GET_DATA_QBIT(lines, j);
            CHECK_BIT(lined, j, gval < thresh ? 1 : 0);
        }
#endif
        break;
    case 8:
            /* Unrolled as 8 source words, 1 dest word */
        for (j = 0, scount = 0, dcount = 0; j + 31 < w; j += 32) {
            dword = 0;
            for (k = 0; k < 8; k++) {
                sword = lines[scount++];
                dword <<= 4;
                gval = (sword >> 24) & 0xff;
                dword |= ((gval - thresh) >> 28) & 8;
                gval = (sword >> 16) & 0xff;
                dword |= ((gval - thresh) >> 29) & 4;
                gval = (sword >> 8) & 0xff;
                dword |= ((gval - thresh) >> 30) & 2;
                gval = sword & 0xff;
                dword |= ((gval - thresh) >> 31) & 1;
            }
            lined[dcount++] = dword;
        }

        if (j < w) {
            dword = 0;
            for (; j < w; j++) {
                if ((j & 3) == 0) {
                    sword = lines[scount++];
                }
                gval = (sword >> 24) & 0xff;
                sword <<= 8;
                dword |= (l_uint64)(((gval - thresh) >> 31) & 1)
                             << (31 - (j & 31));
            }
            lined[dcount] = dword;
        }
#if DEBUG_UNROLLING
        for (j = 0; j < w; j++) {
            gval = GET_DATA_BYTE(lines, j);
            CHECK_BIT(lined, j, gval < thresh ? 1 : 0);
        }
#undef CHECK_BIT
#endif
        break;
    default:
        L_ERROR("src depth not 4 or 8 bpp\n", __func__);
        break;
    }
}


/*------------------------------------------------------------------*
 *                Binarization with variable threshold              *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixVarThresholdToBinary()
 *
 * \param[in]    pixs    8 bpp
 * \param[in]    pixg    8 bpp; contains threshold values for each pixel
 * \return  pixd 1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If the pixel in pixs is less than the corresponding pixel
 *          in pixg, the dest will be 1; otherwise it will be 0.
 * </pre>
 */
PIX *
pixVarThresholdToBinary(PIX  *pixs,
                        PIX  *pixg)
{
l_int32    i, j, vals, valg, w, h, d, wpls, wplg, wpld;
l_uint32  *datas, *datag, *datad, *lines, *lineg, *lined;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (!pixg)
        return (PIX *)ERROR_PTR("pixg not defined", __func__, NULL);
    if (!pixSizesEqual(pixs, pixg))
        return (PIX *)ERROR_PTR("pix sizes not equal", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8)
        return (PIX *)ERROR_PTR("pixs must be 8 bpp", __func__, NULL);

    pixd = pixCreate(w, h, 1);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datag = pixGetData(pixg);
    wplg = pixGetWpl(pixg);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lineg = datag + i * wplg;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            vals = GET_DATA_BYTE(lines, j);
            valg = GET_DATA_BYTE(lineg, j);
            if (vals < valg)
                SET_DATA_BIT(lined, j);
        }
    }

    return pixd;
}


/*------------------------------------------------------------------*
 *                  Binarization by adaptive mapping                *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixAdaptThresholdToBinary()
 *
 * \param[in]    pixs    8 bpp
 * \param[in]    pixm    [optional] 1 bpp image mask; can be null
 * \param[in]    gamma   gamma correction; must be > 0.0; typically ~1.0
 * \return  pixd 1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a simple convenience function for doing adaptive
 *          thresholding on a grayscale image with variable background.
 *          It uses default parameters appropriate for typical text images.
 *          Other high-level adaptive thresholding functions are
 *          pixConvertTo1Adaptive() and pixCleanImage().
 *      (2) %pixm is a 1 bpp mask over "image" regions, which are not
 *          expected to have a white background.  The mask inhibits
 *          background finding under the fg pixels of the mask.  For
 *          images with both text and image, the image regions would
 *          be binarized (or quantized) by a different set of operations.
 *      (3) As %gamma is increased, the foreground pixels are reduced.
 *      (4) Under the covers:  The default background value for normalization
 *          is 200, so we choose 170 for 'maxval' in pixGammaTRC.  Likewise,
 *          the default foreground threshold for normalization is 60,
 *          so we choose 50 for 'minval' in pixGammaTRC.  Because
 *          170 was mapped to 255, choosing 200 for the threshold is
 *          quite safe for avoiding speckle noise from the background.
 * </pre>
 */
PIX *
pixAdaptThresholdToBinary(PIX       *pixs,
                          PIX       *pixm,
                          l_float32  gamma)
{
    if (!pixs || pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs undefined or not 8 bpp", __func__, NULL);

    return pixAdaptThresholdToBinaryGen(pixs, pixm, gamma, 50, 170, 200);
}


/*!
 * \brief   pixAdaptThresholdToBinaryGen()
 *
 * \param[in]    pixs       8 bpp
 * \param[in]    pixm       [optional] 1 bpp image mask; can be null
 * \param[in]    gamma      gamma correction; must be > 0.0; typically ~1.0
 * \param[in]    blackval   dark value to set to black (0)
 * \param[in]    whiteval   light value to set to white (255)
 * \param[in]    thresh     final threshold for binarization
 * \return  pixd 1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a convenience function for doing adaptive thresholding
 *          on a grayscale image with variable background.  Also see notes
 *          in pixAdaptThresholdToBinary().
 *      (2) Reducing %gamma increases the foreground (text) pixels.
 *          Use a low value (e.g., 0.5) for images with light text.
 *      (3) For normal images, see default args in pixAdaptThresholdToBinary().
 *          For images with very light text, these values are appropriate:
 *             gamma     ~0.5
 *             blackval  ~70
 *             whiteval  ~190
 *             thresh    ~200
 * </pre>
 */
PIX *
pixAdaptThresholdToBinaryGen(PIX       *pixs,
                             PIX       *pixm,
                             l_float32  gamma,
                             l_int32    blackval,
                             l_int32    whiteval,
                             l_int32    thresh)
{
PIX  *pix1, *pixd;

    if (!pixs || pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs undefined or not 8 bpp", __func__, NULL);

    if ((pix1 = pixBackgroundNormSimple(pixs, pixm, NULL)) == NULL)
        return (PIX *)ERROR_PTR("pix1 not made", __func__, NULL);
    pixGammaTRC(pix1, pix1, gamma, blackval, whiteval);
    pixd = pixThresholdToBinary(pix1, thresh);
    pixDestroy(&pix1);
    return pixd;
}


/*--------------------------------------------------------------------*
 *       Generate a binary mask from pixels of particular value(s)    *
 *--------------------------------------------------------------------*/
/*!
 * \brief   pixGenerateMaskByValue()
 *
 * \param[in]    pixs      2, 4 or 8 bpp, or colormapped
 * \param[in]    val       of pixels for which we set 1 in dest
 * \param[in]    usecmap   1 to retain cmap values; 0 to convert to gray
 * \return  pixd 1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) %val is the pixel value that we are selecting.  It can be
 *          either a gray value or a colormap index.
 *      (2) If pixs is colormapped, %usecmap determines if the colormap
 *          index values are used, or if the colormap is removed to gray and
 *          the gray values are used.  For the latter, it generates
 *          an approximate grayscale value for each pixel, and then looks
 *          for gray pixels with the value %val.
 * </pre>
 */
PIX *
pixGenerateMaskByValue(PIX     *pixs,
                       l_int32  val,
                       l_int32  usecmap)
{
l_int32    i, j, w, h, d, wplg, wpld;
l_uint32  *datag, *datad, *lineg, *lined;
PIX       *pixg, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    d = pixGetDepth(pixs);
    if (d != 2 && d != 4 && d != 8)
        return (PIX *)ERROR_PTR("not 2, 4 or 8 bpp", __func__, NULL);

    if (!usecmap && pixGetColormap(pixs))
        pixg = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    else
        pixg = pixClone(pixs);
    pixGetDimensions(pixg, &w, &h, &d);
    if (d == 8 && (val < 0 || val > 255)) {
        pixDestroy(&pixg);
        return (PIX *)ERROR_PTR("val out of 8 bpp range", __func__, NULL);
    }
    if (d == 4 && (val < 0 || val > 15)) {
        pixDestroy(&pixg);
        return (PIX *)ERROR_PTR("val out of 4 bpp range", __func__, NULL);
    }
    if (d == 2 && (val < 0 || val > 3)) {
        pixDestroy(&pixg);
        return (PIX *)ERROR_PTR("val out of 2 bpp range", __func__, NULL);
    }

    pixd = pixCreate(w, h, 1);
    pixCopyResolution(pixd, pixg);
    pixCopyInputFormat(pixd, pixs);
    datag = pixGetData(pixg);
    wplg = pixGetWpl(pixg);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lineg = datag + i * wplg;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            if (d == 8) {
                if (GET_DATA_BYTE(lineg, j) == val)
                    SET_DATA_BIT(lined, j);
            } else if (d == 4) {
                if (GET_DATA_QBIT(lineg, j) == val)
                    SET_DATA_BIT(lined, j);
            } else {  /* d == 2 */
                if (GET_DATA_DIBIT(lineg, j) == val)
                    SET_DATA_BIT(lined, j);
            }
        }
    }

    pixDestroy(&pixg);
    return pixd;
}


/*!
 * \brief   pixGenerateMaskByBand()
 *
 * \param[in]    pixs           2, 4 or 8 bpp, or colormapped
 * \param[in]    lower, upper   two pixel values from which a range, either
 *                              between (inband) or outside of (!inband),
 *                              determines which pixels in pixs cause us to
 *                              set a 1 in the dest mask
 * \param[in]    inband         1 for finding pixels in [lower, upper];
 *                              0 for finding pixels in
 *                              [0, lower) union (upper, 255]
 * \param[in]    usecmap        1 to retain cmap values; 0 to convert to gray
 * \return  pixd 1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Generates a 1 bpp mask pixd, the same size as pixs, where
 *          the fg pixels in the mask are those either within the specified
 *          band (for inband == 1) or outside the specified band
 *          (for inband == 0).
 *      (2) If pixs is colormapped, %usecmap determines if the colormap
 *          values are used, or if the colormap is removed to gray and
 *          the gray values are used.  For the latter, it generates
 *          an approximate grayscale value for each pixel, and then looks
 *          for gray pixels with the value %val.
 * </pre>
 */
PIX *
pixGenerateMaskByBand(PIX     *pixs,
                      l_int32  lower,
                      l_int32  upper,
                      l_int32  inband,
                      l_int32  usecmap)
{
l_int32    i, j, w, h, d, wplg, wpld, val;
l_uint32  *datag, *datad, *lineg, *lined;
PIX       *pixg, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    d = pixGetDepth(pixs);
    if (d != 2 && d != 4 && d != 8)
        return (PIX *)ERROR_PTR("not 2, 4 or 8 bpp", __func__, NULL);
    if (lower < 0 || lower > upper)
        return (PIX *)ERROR_PTR("lower < 0 or lower > upper!", __func__, NULL);

    if (!usecmap && pixGetColormap(pixs))
        pixg = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    else
        pixg = pixClone(pixs);
    pixGetDimensions(pixg, &w, &h, &d);
    if (d == 8 && upper > 255) {
        pixDestroy(&pixg);
        return (PIX *)ERROR_PTR("d == 8 and upper > 255", __func__, NULL);
    }
    if (d == 4 && upper > 15) {
        pixDestroy(&pixg);
        return (PIX *)ERROR_PTR("d == 4 and upper > 15", __func__, NULL);
    }
    if (d == 2 && upper > 3) {
        pixDestroy(&pixg);
        return (PIX *)ERROR_PTR("d == 2 and upper > 3", __func__, NULL);
    }

    pixd = pixCreate(w, h, 1);
    pixCopyResolution(pixd, pixg);
    pixCopyInputFormat(pixd, pixs);
    datag = pixGetData(pixg);
    wplg = pixGetWpl(pixg);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lineg = datag + i * wplg;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            if (d == 8)
                val = GET_DATA_BYTE(lineg, j);
            else if (d == 4)
                val = GET_DATA_QBIT(lineg, j);
            else  /* d == 2 */
                val = GET_DATA_DIBIT(lineg, j);
            if (inband) {
                if (val >= lower && val <= upper)
                    SET_DATA_BIT(lined, j);
            } else {  /* out of band */
                if (val < lower || val > upper)
                    SET_DATA_BIT(lined, j);
            }
        }
    }

    pixDestroy(&pixg);
    return pixd;
}


/*------------------------------------------------------------------*
 *                Thresholding to 2 bpp by dithering                *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixDitherTo2bpp()
 *
 * \param[in]    pixs       8 bpp
 * \param[in]    cmapflag   1 to generate a colormap
 * \return  pixd dithered   2 bpp, or NULL on error
 *
 *  An analog of the Floyd-Steinberg error diffusion dithering
 *  algorithm is used to "dibitize" an 8 bpp grayscale image
 *  to 2 bpp, using equally spaced gray values of 0, 85, 170, and 255,
 *  which are served by thresholds of 43, 128 and 213.
 *  If cmapflag == 1, the colormap values are set to 0, 85, 170 and 255.
 *  If a pixel has a value between 0 and 42, it is dibitized
 *  to 0, and the excess above 0 is added to the
 *  three neighboring pixels, in the fractions 3/8 to i, j+1,
 *  3/8 to i+1, j) and 1/4 to (i+1, j+1, truncating to 255 if
 *  necessary.  If a pixel has a value between 43 and 127, it is
 *  dibitized to 1, and the excess above 85 is added to the three
 *  neighboring pixels as before.  If the value is below 85, the
 *  excess is subtracted.  With a value between 128
 *  and 212, it is dibitized to 2, with the excess on either side
 *  of 170 distributed as before.  Finally, with a value between
 *  213 and 255, it is dibitized to 3, with the excess below 255
 *  subtracted from the neighbors.  We always truncate to 0 or 255.
 *  The details can be seen in the lookup table generation.
 *
 *  This function differs from straight dithering in that it allows
 *  clipping of grayscale to 0 or 255 if the values are
 *  sufficiently close, without distribution of the excess.
 *  This uses default values from pix.h to specify the range of lower
 *  and upper values near 0 and 255, rsp that are clipped to black
 *  and white without propagating the excess.
 *  Not propagating the excess has the effect of reducing the snake
 *  patterns in parts of the image that are nearly black or white;
 *  however, it also prevents any attempt to reproduce gray for those values.
 *
 *  The implementation uses 3 lookup tables for simplicity, and
 *  a pair of line buffers to avoid modifying pixs.
 */
PIX *
pixDitherTo2bpp(PIX     *pixs,
                l_int32  cmapflag)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("must be 8 bpp for dithering", __func__, NULL);

    return pixDitherTo2bppSpec(pixs, DEFAULT_CLIP_LOWER_2,
                               DEFAULT_CLIP_UPPER_2, cmapflag);
}


/*!
 * \brief   pixDitherTo2bppSpec()
 *
 * \param[in]    pixs        8 bpp
 * \param[in]    lowerclip   lower clip distance to black; use 0 for default
 * \param[in]    upperclip   upper clip distance to white; use 0 for default
 * \param[in]    cmapflag    1 to generate a colormap
 * \return  pixd dithered    2 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See comments above in pixDitherTo2bpp() for details.
 *      (2) The input parameters lowerclip and upperclip specify the range
 *          of lower and upper values (near 0 and 255, rsp) that are
 *          clipped to black and white without propagating the excess.
 *          For that reason, lowerclip and upperclip should be small numbers.
 * </pre>
 */
PIX *
pixDitherTo2bppSpec(PIX     *pixs,
                    l_int32  lowerclip,
                    l_int32  upperclip,
                    l_int32  cmapflag)
{
l_int32    w, h, d, wplt, wpld;
l_int32   *tabval, *tab38, *tab14;
l_uint32  *datat, *datad;
l_uint32  *bufs1, *bufs2;
PIX       *pixt, *pixd;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8)
        return (PIX *)ERROR_PTR("must be 8 bpp for dithering", __func__, NULL);
    if (lowerclip < 0 || lowerclip > 255)
        return (PIX *)ERROR_PTR("invalid value for lowerclip", __func__, NULL);
    if (upperclip < 0 || upperclip > 255)
        return (PIX *)ERROR_PTR("invalid value for upperclip", __func__, NULL);

    if ((pixd = pixCreate(w, h, 2)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

        /* If there is a colormap, remove it */
    pixt = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    datat = pixGetData(pixt);
    wplt = pixGetWpl(pixt);

        /* Two line buffers, 1 for current line and 2 for next line */
    bufs1 = (l_uint32 *)LEPT_CALLOC(wplt, sizeof(l_uint32));
    bufs2 = (l_uint32 *)LEPT_CALLOC(wplt, sizeof(l_uint32));
    if (!bufs1 || !bufs2) {
        LEPT_FREE(bufs1);
        LEPT_FREE(bufs2);
        pixDestroy(&pixd);
        pixDestroy(&pixt);
        return (PIX *)ERROR_PTR("bufs1, bufs2 not both made", __func__, NULL);
    }

        /* 3 lookup tables: 2-bit value, (3/8)excess, and (1/4)excess */
    make8To2DitherTables(&tabval, &tab38, &tab14, lowerclip, upperclip);

    ditherTo2bppLow(datad, w, h, wpld, datat, wplt, bufs1, bufs2,
                    tabval, tab38, tab14);

    if (cmapflag) {
        cmap = pixcmapCreateLinear(2, 4);
        pixSetColormap(pixd, cmap);
    }

    LEPT_FREE(bufs1);
    LEPT_FREE(bufs2);
    LEPT_FREE(tabval);
    LEPT_FREE(tab38);
    LEPT_FREE(tab14);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   ditherTo2bppLow()
 *
 *  Low-level function for doing Floyd-Steinberg error diffusion
 *  dithering from 8 bpp (datas) to 2 bpp (datad).  Two source
 *  line buffers, bufs1 and bufs2, are provided, along with three
 *  256-entry lookup tables: tabval gives the output pixel value,
 *  tab38 gives the extra (plus or minus) transferred to the pixels
 *  directly to the left and below, and tab14 gives the extra
 *  transferred to the diagonal below.  The choice of 3/8 and 1/4
 *  is traditional but arbitrary when you use a lookup table; the
 *  only constraint is that the sum is 1.  See other comments
 *  below and in grayquant.c.
 */
static void
ditherTo2bppLow(l_uint32  *datad,
                l_int32    w,
                l_int32    h,
                l_int32    wpld,
                l_uint32  *datas,
                l_int32    wpls,
                l_uint32  *bufs1,
                l_uint32  *bufs2,
                l_int32   *tabval,
                l_int32   *tab38,
                l_int32   *tab14)
{
l_int32      i;
l_uint32    *lined;

        /* do all lines except last line */
    memcpy(bufs2, datas, 4 * wpls);  /* prime the buffer */
    for (i = 0; i < h - 1; i++) {
        memcpy(bufs1, bufs2, 4 * wpls);
        memcpy(bufs2, datas + (i + 1) * wpls, 4 * wpls);
        lined = datad + i * wpld;
        ditherTo2bppLineLow(lined, w, bufs1, bufs2, tabval, tab38, tab14, 0);
    }

        /* do last line */
    memcpy(bufs1, bufs2, 4 * wpls);
    lined = datad + (h - 1) * wpld;
    ditherTo2bppLineLow(lined, w, bufs1, bufs2, tabval, tab38, tab14, 1);
}


/*!
 * \brief   ditherTo2bppLineLow()
 *
 * \param[in]    lined          ptr to beginning of dest line
 * \param[in]    w              width of image in pixels
 * \param[in]    bufs1          buffer of current source line
 * \param[in]    bufs2          buffer of next source line
 * \param[in]    tabval         value to assign for current pixel
 * \param[in]    tab38          excess value to give to neighboring 3/8 pixels
 * \param[in]    tab14          excess value to give to neighboring 1/4 pixel
 * \param[in]    lastlineflag   0 if not last dest line, 1 if last dest line
 * \return  void
 *
 *  Dispatches error diffusion dithering for
 *  a single line of the image.  If lastlineflag == 0,
 *  both source buffers are used; otherwise, only bufs1
 *  is used.  We use source buffers because the error
 *  is propagated into them, and we don't want to change
 *  the input src image.
 *
 *  We break dithering out line by line to make it
 *  easier to combine functions like interpolative
 *  scaling and error diffusion dithering, as such a
 *  combination of operations obviates the need to
 *  generate a 2x grayscale image as an intermediary.
 */
static void
ditherTo2bppLineLow(l_uint32  *lined,
                    l_int32    w,
                    l_uint32  *bufs1,
                    l_uint32  *bufs2,
                    l_int32   *tabval,
                    l_int32   *tab38,
                    l_int32   *tab14,
                    l_int32    lastlineflag)
{
l_int32  j;
l_int32  oval, tab38val, tab14val;
l_uint8  rval, bval, dval;

    if (lastlineflag == 0) {
        for (j = 0; j < w - 1; j++) {
            oval = GET_DATA_BYTE(bufs1, j);
            SET_DATA_DIBIT(lined, j, tabval[oval]);
            rval = GET_DATA_BYTE(bufs1, j + 1);
            bval = GET_DATA_BYTE(bufs2, j);
            dval = GET_DATA_BYTE(bufs2, j + 1);
            tab38val = tab38[oval];
            tab14val = tab14[oval];
            if (tab38val < 0) {
                rval = L_MAX(0, rval + tab38val);
                bval = L_MAX(0, bval + tab38val);
                dval = L_MAX(0, dval + tab14val);
            } else {
                rval = L_MIN(255, rval + tab38val);
                bval = L_MIN(255, bval + tab38val);
                dval = L_MIN(255, dval + tab14val);
            }
            SET_DATA_BYTE(bufs1, j + 1, rval);
            SET_DATA_BYTE(bufs2, j, bval);
            SET_DATA_BYTE(bufs2, j + 1, dval);
        }

            /* do last column: j = w - 1 */
        oval = GET_DATA_BYTE(bufs1, j);
        SET_DATA_DIBIT(lined, j, tabval[oval]);
        bval = GET_DATA_BYTE(bufs2, j);
        tab38val = tab38[oval];
        if (tab38val < 0)
            bval = L_MAX(0, bval + tab38val);
        else
            bval = L_MIN(255, bval + tab38val);
        SET_DATA_BYTE(bufs2, j, bval);
    } else {   /* lastlineflag == 1 */
        for (j = 0; j < w - 1; j++) {
            oval = GET_DATA_BYTE(bufs1, j);
            SET_DATA_DIBIT(lined, j, tabval[oval]);
            rval = GET_DATA_BYTE(bufs1, j + 1);
            tab38val = tab38[oval];
            if (tab38val < 0)
                rval = L_MAX(0, rval + tab38val);
            else
                rval = L_MIN(255, rval + tab38val);
            SET_DATA_BYTE(bufs1, j + 1, rval);
        }

            /* do last pixel: (i, j) = (h - 1, w - 1) */
        oval = GET_DATA_BYTE(bufs1, j);
        SET_DATA_DIBIT(lined, j, tabval[oval]);
    }
}


/*!
 * \brief   make8To2DitherTables()
 *
 * \param[out]  ptabval      value assigned to output pixel; 0, 1, 2 or 3
 * \param[out]  ptab38       amount propagated to pixels left and below
 * \param[out]  ptab14       amount propagated to pixel to left and down
 * \param[in]   cliptoblack  values near 0 where the excess is not propagated
 * \param[in]   cliptowhite  values near 255 where the deficit is not propagated
 *
 * \return  0 if OK, 1 on error
 */
static l_int32
make8To2DitherTables(l_int32 **ptabval,
                     l_int32 **ptab38,
                     l_int32 **ptab14,
                     l_int32   cliptoblack,
                     l_int32   cliptowhite)
{
l_int32   i;
l_int32  *tabval, *tab38, *tab14;

        /* 3 lookup tables: 2-bit value, (3/8)excess, and (1/4)excess */
    tabval = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    tab38 = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    tab14 = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    *ptabval = tabval;
    *ptab38 = tab38;
    *ptab14 = tab14;

    for (i = 0; i < 256; i++) {
        if (i <= cliptoblack) {
            tabval[i] = 0;
            tab38[i] = 0;
            tab14[i] = 0;
        } else if (i < 43) {
            tabval[i] = 0;
            tab38[i] = (3 * i + 4) / 8;
            tab14[i] = (i + 2) / 4;
        } else if (i < 85) {
            tabval[i] = 1;
            tab38[i] = (3 * (i - 85) - 4) / 8;
            tab14[i] = ((i - 85) - 2) / 4;
        } else if (i < 128) {
            tabval[i] = 1;
            tab38[i] = (3 * (i - 85) + 4) / 8;
            tab14[i] = ((i - 85) + 2) / 4;
        } else if (i < 170) {
            tabval[i] = 2;
            tab38[i] = (3 * (i - 170) - 4) / 8;
            tab14[i] = ((i - 170) - 2) / 4;
        } else if (i < 213) {
            tabval[i] = 2;
            tab38[i] = (3 * (i - 170) + 4) / 8;
            tab14[i] = ((i - 170) + 2) / 4;
        } else if (i < 255 - cliptowhite) {
            tabval[i] = 3;
            tab38[i] = (3 * (i - 255) - 4) / 8;
            tab14[i] = ((i - 255) - 2) / 4;
        } else {  /* i >= 255 - cliptowhite */
            tabval[i] = 3;
            tab38[i] = 0;
            tab14[i] = 0;
        }
    }

    return 0;
}


/*--------------------------------------------------------------------*
 *  Simple (pixelwise) thresholding to 2 bpp with optional colormap   *
 *--------------------------------------------------------------------*/
/*!
 * \brief   pixThresholdTo2bpp()
 *
 * \param[in]    pixs       8 bpp
 * \param[in]    nlevels    equally spaced; must be between 2 and 4
 * \param[in]    cmapflag   1 to build colormap; 0 otherwise
 * \return  pixd 2 bpp, optionally with colormap, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Valid values for nlevels is the set {2, 3, 4}.
 *      (2) Any colormap on the input pixs is removed to 8 bpp grayscale.
 *      (3) This function is typically invoked with cmapflag == 1.
 *          In the situation where no colormap is desired, nlevels is
 *          ignored and pixs is thresholded to 4 levels.
 *      (4) The target output colors are equally spaced, with the
 *          darkest at 0 and the lightest at 255.  The thresholds are
 *          chosen halfway between adjacent output values.  A table
 *          is built that specifies the mapping from src to dest.
 *      (5) If cmapflag == 1, a colormap of size 'nlevels' is made,
 *          and the pixel values in pixs are replaced by their
 *          appropriate color indices.  The number of holdouts,
 *          4 - nlevels, will be between 0 and 2.
 *      (6) If you don't want the thresholding to be equally spaced,
 *          either first transform the 8 bpp src using pixGammaTRC().
 *          or, if cmapflag == 1, after calling this function you can use
 *          pixcmapResetColor() to change any individual colors.
 *      (7) If a colormap is generated, it will specify (to display
 *          programs) exactly how each level is to be represented in RGB
 *          space.  When representing text, 3 levels is far better than
 *          2 because of the antialiasing of the single gray level,
 *          and 4 levels (black, white and 2 gray levels) is getting
 *          close to the perceptual quality of a (nearly continuous)
 *          grayscale image.  With 2 bpp, you can set up a colormap
 *          and allocate from 2 to 4 levels to represent antialiased text.
 *          Any left over colormap entries can be used for coloring regions.
 *          For the same number of levels, the file size of a 2 bpp image
 *          is about 10% smaller than that of a 4 bpp result for the same
 *          number of levels.  For both 2 bpp and 4 bpp, using 4 levels you
 *          get compression far better than that of jpeg, because the
 *          quantization to 4 levels will remove the jpeg ringing in the
 *          background near character edges.
 * </pre>
 */
PIX *
pixThresholdTo2bpp(PIX     *pixs,
                   l_int32  nlevels,
                   l_int32  cmapflag)
{
l_int32   *qtab;
l_int32    w, h, d, wplt, wpld;
l_uint32  *datat, *datad;
PIX       *pixt, *pixd;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (nlevels < 2 || nlevels > 4)
        return (PIX *)ERROR_PTR("nlevels not in {2, 3, 4}", __func__, NULL);

    if ((pixd = pixCreate(w, h, 2)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    if (cmapflag) {   /* hold out (4 - nlevels) cmap entries */
        cmap = pixcmapCreateLinear(2, nlevels);
        pixSetColormap(pixd, cmap);
    }

        /* If there is a colormap in the src, remove it */
    pixt = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    datat = pixGetData(pixt);
    wplt = pixGetWpl(pixt);

        /* Make the appropriate table */
    if (cmapflag)
        qtab = makeGrayQuantIndexTable(nlevels);
    else
        qtab = makeGrayQuantTargetTable(4, 2);

    thresholdTo2bppLow(datad, h, wpld, datat, wplt, qtab);

    LEPT_FREE(qtab);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   thresholdTo2bppLow()
 *
 *  Low-level function for thresholding from 8 bpp (datas) to
 *  2 bpp (datad), using thresholds implicitly defined through %tab,
 *  a 256-entry lookup table that gives a 2-bit output value
 *  for each possible input.
 *
 *  For each line, unroll the loop so that for each 32 bit src word,
 *  representing four consecutive 8-bit pixels, we compose one byte
 *  of output consisiting of four 2-bit pixels.
 */
static void
thresholdTo2bppLow(l_uint32  *datad,
                   l_int32    h,
                   l_int32    wpld,
                   l_uint32  *datas,
                   l_int32    wpls,
                   l_int32   *tab)
{
l_uint8    sval1, sval2, sval3, sval4, dval;
l_int32    i, j, k;
l_uint32  *lines, *lined;

    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < wpls; j++) {
            k = 4 * j;
            sval1 = GET_DATA_BYTE(lines, k);
            sval2 = GET_DATA_BYTE(lines, k + 1);
            sval3 = GET_DATA_BYTE(lines, k + 2);
            sval4 = GET_DATA_BYTE(lines, k + 3);
            dval = (tab[sval1] << 6) | (tab[sval2] << 4) |
                   (tab[sval3] << 2) | tab[sval4];
            SET_DATA_BYTE(lined, j, dval);
        }
    }
}


/*----------------------------------------------------------------------*
 *               Simple (pixelwise) thresholding to 4 bpp               *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixThresholdTo4bpp()
 *
 * \param[in]    pixs      8 bpp, can have colormap
 * \param[in]    nlevels   equally spaced; must be between 2 and 16
 * \param[in]    cmapflag  1 to build colormap; 0 otherwise
 * \return  pixd 4 bpp, optionally with colormap, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Valid values for nlevels is the set {2, ... 16}.
 *      (2) Any colormap on the input pixs is removed to 8 bpp grayscale.
 *      (3) This function is typically invoked with cmapflag == 1.
 *          In the situation where no colormap is desired, nlevels is
 *          ignored and pixs is thresholded to 16 levels.
 *      (4) The target output colors are equally spaced, with the
 *          darkest at 0 and the lightest at 255.  The thresholds are
 *          chosen halfway between adjacent output values.  A table
 *          is built that specifies the mapping from src to dest.
 *      (5) If cmapflag == 1, a colormap of size 'nlevels' is made,
 *          and the pixel values in pixs are replaced by their
 *          appropriate color indices.  The number of holdouts,
 *          16 - nlevels, will be between 0 and 14.
 *      (6) If you don't want the thresholding to be equally spaced,
 *          either first transform the 8 bpp src using pixGammaTRC().
 *          or, if cmapflag == 1, after calling this function you can use
 *          pixcmapResetColor() to change any individual colors.
 *      (7) If a colormap is generated, it will specify, to display
 *          programs, exactly how each level is to be represented in RGB
 *          space.  When representing text, 3 levels is far better than
 *          2 because of the antialiasing of the single gray level,
 *          and 4 levels (black, white and 2 gray levels) is getting
 *          close to the perceptual quality of a (nearly continuous)
 *          grayscale image.  Therefore, with 4 bpp, you can set up a
 *          colormap, allocate a relatively small fraction of the 16
 *          possible values to represent antialiased text, and use the
 *          other colormap entries for other things, such as coloring
 *          text or background.  Two other reasons for using a small number
 *          of gray values for antialiased text are (1) PNG compression
 *          gets worse as the number of levels that are used is increased,
 *          and (2) using a small number of levels will filter out most of
 *          the jpeg ringing that is typically introduced near sharp edges
 *          of text.  This filtering is partly responsible for the improved
 *          compression.
 * </pre>
 */
PIX *
pixThresholdTo4bpp(PIX     *pixs,
                   l_int32  nlevels,
                   l_int32  cmapflag)
{
l_int32   *qtab;
l_int32    w, h, d, wplt, wpld;
l_uint32  *datat, *datad;
PIX       *pixt, *pixd;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (nlevels < 2 || nlevels > 16)
        return (PIX *)ERROR_PTR("nlevels not in [2,...,16]", __func__, NULL);

    if ((pixd = pixCreate(w, h, 4)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    if (cmapflag) {   /* hold out (16 - nlevels) cmap entries */
        cmap = pixcmapCreateLinear(4, nlevels);
        pixSetColormap(pixd, cmap);
    }

        /* If there is a colormap in the src, remove it */
    pixt = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    datat = pixGetData(pixt);
    wplt = pixGetWpl(pixt);

        /* Make the appropriate table */
    if (cmapflag)
        qtab = makeGrayQuantIndexTable(nlevels);
    else
        qtab = makeGrayQuantTargetTable(16, 4);

    thresholdTo4bppLow(datad, h, wpld, datat, wplt, qtab);

    LEPT_FREE(qtab);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   thresholdTo4bppLow()
 *
 *  Low-level function for thresholding from 8 bpp (datas) to
 *  4 bpp (datad), using thresholds implicitly defined through %tab,
 *  a 256-entry lookup table that gives a 4-bit output value
 *  for each possible input.
 *
 *  For each line, unroll the loop so that for each 32 bit src word,
 *  representing four consecutive 8-bit pixels, we compose two bytes
 *  of output consisiting of four 4-bit pixels.
 */
static void
thresholdTo4bppLow(l_uint32  *datad,
                   l_int32    h,
                   l_int32    wpld,
                   l_uint32  *datas,
                   l_int32    wpls,
                   l_int32   *tab)
{
l_uint8    sval1, sval2, sval3, sval4;
l_uint16   dval;
l_int32    i, j, k;
l_uint32  *lines, *lined;

    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < wpls; j++) {
            k = 4 * j;
            sval1 = GET_DATA_BYTE(lines, k);
            sval2 = GET_DATA_BYTE(lines, k + 1);
            sval3 = GET_DATA_BYTE(lines, k + 2);
            sval4 = GET_DATA_BYTE(lines, k + 3);
            dval = (tab[sval1] << 12) | (tab[sval2] << 8) |
                   (tab[sval3] << 4) | tab[sval4];
            SET_DATA_TWO_BYTES(lined, j, dval);
        }
    }
}


/*----------------------------------------------------------------------*
 *    Simple (pixelwise) thresholding on 8 bpp with optional colormap   *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixThresholdOn8bpp()
 *
 * \param[in]    pixs       8 bpp, can have colormap
 * \param[in]    nlevels    equally spaced; must be between 2 and 256
 * \param[in]    cmapflag   1 to build colormap; 0 otherwise
 * \return  pixd 8 bpp, optionally with colormap, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Valid values for nlevels is the set {2,...,256}.
 *      (2) Any colormap on the input pixs is removed to 8 bpp grayscale.
 *      (3) If cmapflag == 1, a colormap of size 'nlevels' is made,
 *          and the pixel values in pixs are replaced by their
 *          appropriate color indices.  Otherwise, the pixel values
 *          are the actual thresholded (i.e., quantized) grayscale values.
 *      (4) If you don't want the thresholding to be equally spaced,
 *          first transform the input 8 bpp src using pixGammaTRC().
 * </pre>
 */
PIX *
pixThresholdOn8bpp(PIX     *pixs,
                   l_int32  nlevels,
                   l_int32  cmapflag)
{
l_int32   *qtab;  /* quantization table */
l_int32    i, j, w, h, wpld, val, newval;
l_uint32  *datad, *lined;
PIX       *pixd;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (nlevels < 2 || nlevels > 256)
        return (PIX *)ERROR_PTR("nlevels not in [2,...,256]", __func__, NULL);

        /* Get a new pixd; if there is a colormap in the src, remove it */
    if (pixGetColormap(pixs))
        pixd = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    else
        pixd = pixCopy(NULL, pixs);

    if (cmapflag) {   /* hold out (256 - nlevels) cmap entries */
        cmap = pixcmapCreateLinear(8, nlevels);
        pixSetColormap(pixd, cmap);
    }

    if (cmapflag)
        qtab = makeGrayQuantIndexTable(nlevels);
    else
        qtab = makeGrayQuantTargetTable(nlevels, 8);

    pixGetDimensions(pixd, &w, &h, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            val = GET_DATA_BYTE(lined, j);
            newval = qtab[val];
            SET_DATA_BYTE(lined, j, newval);
        }
    }

    LEPT_FREE(qtab);
    return pixd;
}


/*----------------------------------------------------------------------*
 *    Arbitrary (pixelwise) thresholding from 8 bpp to 2, 4 or 8 bpp    *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixThresholdGrayArb()
 *
 * \param[in]    pixs          8 bpp grayscale; can have colormap
 * \param[in]    edgevals      string giving edge value of each bin
 * \param[in]    outdepth      0, 2, 4 or 8 bpp; 0 is default for min depth
 * \param[in]    use_average   1 if use the average pixel value in colormap
 * \param[in]    setblack      1 if darkest color is set to black
 * \param[in]    setwhite      1 if lightest color is set to white
 * \return  pixd 2, 4 or 8 bpp quantized image with colormap,
 *                    or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This function allows exact specification of the quantization bins.
 *          The string %edgevals is a space-separated set of values
 *          specifying the dividing points between output quantization bins.
 *          These threshold values are assigned to the bin with higher
 *          values, so that each of them is the smallest value in their bin.
 *      (2) The output image (pixd) depth is specified by %outdepth.  The
 *          number of bins is the number of edgevals + 1.  The
 *          relation between outdepth and the number of bins is:
 *               outdepth = 2       nbins <= 4
 *               outdepth = 4       nbins <= 16
 *               outdepth = 8       nbins <= 256
 *          With %outdepth == 0, the minimum required depth for the
 *          given number of bins is used.
 *          The output pixd has a colormap.
 *      (3) The last 3 args determine the specific values that go into
 *          the colormap.
 *      (4) For %use_average:
 *            ~ if TRUE, the average value of pixels falling in the bin is
 *              chosen as the representative gray value.  Otherwise,
 *            ~ if FALSE, the central value of each bin is chosen as
 *              the representative value.
 *          The colormap holds the representative value.
 *      (5) For %setblack, if TRUE the darkest color is set to (0,0,0).
 *      (6) For %setwhite, if TRUE the lightest color is set to (255,255,255).
 *      (7) An alternative to using this function to quantize to
 *          unequally-spaced bins is to first transform the 8 bpp pixs
 *          using pixGammaTRC(), and follow this with pixThresholdTo4bpp().
 * </pre>
 */
PIX *
pixThresholdGrayArb(PIX         *pixs,
                    const char  *edgevals,
                    l_int32      outdepth,
                    l_int32      use_average,
                    l_int32      setblack,
                    l_int32      setwhite)
{
l_int32   *qtab;
l_int32    w, h, d, i, j, n, wplt, wpld, val, newval;
l_uint32  *datat, *datad, *linet, *lined;
NUMA      *na;
PIX       *pixt, *pixd;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (!edgevals)
        return (PIX *)ERROR_PTR("edgevals not defined", __func__, NULL);
    if (outdepth != 0 && outdepth != 2 && outdepth != 4 && outdepth != 8)
        return (PIX *)ERROR_PTR("invalid outdepth", __func__, NULL);

        /* Parse and sort (if required) the bin edge values */
    na = parseStringForNumbers(edgevals, " \t\n,");
    n = numaGetCount(na);
    if (n > 255) {
        numaDestroy(&na);
        return (PIX *)ERROR_PTR("more than 256 levels", __func__, NULL);
    }
    if (outdepth == 0) {
        if (n <= 3)
            outdepth = 2;
        else if (n <= 15)
            outdepth = 4;
        else
            outdepth = 8;
    } else if (n + 1 > (1 << outdepth)) {
        L_WARNING("outdepth too small; setting to 8 bpp\n", __func__);
        outdepth = 8;
    }
    numaSort(na, na, L_SORT_INCREASING);

        /* Make the quantization LUT and the colormap */
    makeGrayQuantTableArb(na, outdepth, &qtab, &cmap);
    if (use_average) {  /* use the average value in each bin */
        pixcmapDestroy(&cmap);
        makeGrayQuantColormapArb(pixs, qtab, outdepth, &cmap);
    }
    pixcmapSetBlackAndWhite(cmap, setblack, setwhite);
    numaDestroy(&na);

    if ((pixd = pixCreate(w, h, outdepth)) == NULL) {
        LEPT_FREE(qtab);
        pixcmapDestroy(&cmap);
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    }
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    pixSetColormap(pixd, cmap);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

        /* If there is a colormap in the src, remove it */
    pixt = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    datat = pixGetData(pixt);
    wplt = pixGetWpl(pixt);

    if (outdepth == 2) {
        thresholdTo2bppLow(datad, h, wpld, datat, wplt, qtab);
    } else if (outdepth == 4) {
        thresholdTo4bppLow(datad, h, wpld, datat, wplt, qtab);
    } else {
        for (i = 0; i < h; i++) {
            lined = datad + i * wpld;
            linet = datat + i * wplt;
            for (j = 0; j < w; j++) {
                val = GET_DATA_BYTE(linet, j);
                newval = qtab[val];
                SET_DATA_BYTE(lined, j, newval);
            }
        }
    }

    LEPT_FREE(qtab);
    pixDestroy(&pixt);
    return pixd;
}


/*----------------------------------------------------------------------*
 *     Quantization tables for linear thresholds of grayscale images    *
 *----------------------------------------------------------------------*/
/*!
 * \brief   makeGrayQuantIndexTable()
 *
 * \param[in]    nlevels    number of output levels
 * \return  table maps input gray level to colormap index,
 *                     or NULL on error
 * <pre>
 * Notes:
 *      (1) 'nlevels' is some number between 2 and 256 (typically 8 or less).
 *      (2) The table is typically used for quantizing 2, 4 and 8 bpp
 *          grayscale src pix, and generating a colormapped dest pix.
 * </pre>
 */
l_int32 *
makeGrayQuantIndexTable(l_int32  nlevels)
{
l_int32  *tab;
l_int32   i, j, thresh;

    tab = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    for (i = 0; i < 256; i++) {
        for (j = 0; j < nlevels; j++) {
            thresh = 255 * (2 * j + 1) / (2 * nlevels - 2);
            if (i <= thresh) {
                tab[i] = j;
/*                lept_stderr("tab[%d] = %d\n", i, j); */
                break;
            }
        }
    }
    return tab;
}


/*!
 * \brief   makeGrayQuantTargetTable()
 *
 * \param[in]    nlevels    number of output levels
 * \param[in]    depth      of dest pix, in bpp; 2, 4 or 8 bpp
 * \return  table maps input gray level to thresholded gray level,
 *                     or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) nlevels is some number between 2 and 2^(depth)
 *      (2) The table is used in two similar ways:
 *           ~ for 8 bpp, it quantizes to a given number of target levels
 *           ~ for 2 and 4 bpp, it thresholds to appropriate target values
 *             that will use the full dynamic range of the dest pix.
 *      (3) For depth = 8, the number of thresholds chosen is
 *          ('nlevels' - 1), and the 'nlevels' values stored in the
 *          table are at the two at the extreme ends, (0, 255), plus
 *          plus ('nlevels' - 2) values chosen at equal intervals between.
 *          For example, for depth = 8 and 'nlevels' = 3, the two
 *          threshold values are 3f and bf, and the three target pixel
 *          values are 0, 7f and ff.
 *      (4) For depth < 8, we ignore nlevels, and always use the maximum
 *          number of levels, which is 2^(depth).
 *          If you want nlevels < the maximum number, you should always
 *          use a colormap.
 * </pre>
 */
static l_int32 *
makeGrayQuantTargetTable(l_int32  nlevels,
                         l_int32  depth)
{
l_int32  *tab;
l_int32   i, j, thresh, maxval, quantval;

    tab = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    maxval = (1 << depth) - 1;
    if (depth < 8)
        nlevels = 1 << depth;
    for (i = 0; i < 256; i++) {
        for (j = 0; j < nlevels; j++) {
            thresh = 255 * (2 * j + 1) / (2 * nlevels - 2);
            if (i <= thresh) {
                quantval = maxval * j / (nlevels - 1);
                tab[i] = quantval;
/*                lept_stderr("tab[%d] = %d\n", i, tab[i]); */
                break;
            }
        }
    }
    return tab;
}


/*----------------------------------------------------------------------*
 *   Quantization table for arbitrary thresholding of grayscale images  *
 *----------------------------------------------------------------------*/
/*!
 * \brief   makeGrayQuantTableArb()
 *
 * \param[in]    na         numa of bin boundaries
 * \param[in]    outdepth   of colormap: 1, 2, 4 or 8
 * \param[out]   ptab       table mapping input gray level to cmap index
 * \param[out]   pcmap      colormap
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The number of bins is the count of %na + 1.
 *      (2) The bin boundaries in na must be sorted in increasing order.
 *      (3) The table is an inverse colormap: it maps input gray level
 *          to colormap index (the bin number).
 *      (4) The colormap generated here has quantized values at the
 *          center of each bin.  If you want to use the average gray
 *          value of pixels within the bin, discard the colormap and
 *          compute it using makeGrayQuantColormapArb().
 *      (5) Returns an error if there are not enough levels in the
 *          output colormap for the number of bins.  The number
 *          of bins must not exceed 2^outdepth.
 * </pre>
 */
l_ok
makeGrayQuantTableArb(NUMA      *na,
                      l_int32    outdepth,
                      l_int32  **ptab,
                      PIXCMAP  **pcmap)
{
l_int32   i, j, n, jstart, ave, val;
l_int32  *tab;
PIXCMAP  *cmap;

    if (!ptab)
        return ERROR_INT("&tab not defined", __func__, 1);
    *ptab = NULL;
    if (!pcmap)
        return ERROR_INT("&cmap not defined", __func__, 1);
    *pcmap = NULL;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    n = numaGetCount(na);
    if (n + 1 > (1 << outdepth))
        return ERROR_INT("more bins than cmap levels", __func__, 1);

    if ((cmap = pixcmapCreate(outdepth)) == NULL)
        return ERROR_INT("cmap not made", __func__, 1);
    tab = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    *ptab = tab;
    *pcmap = cmap;

        /* First n bins */
    jstart = 0;
    for (i = 0; i < n; i++) {
        numaGetIValue(na, i, &val);
        ave = (jstart + val) / 2;
        pixcmapAddColor(cmap, ave, ave, ave);
        for (j = jstart; j < val; j++)
            tab[j] = i;
        jstart = val;
    }

        /* Last bin */
    ave = (jstart + 255) / 2;
    pixcmapAddColor(cmap, ave, ave, ave);
    for (j = jstart; j < 256; j++)
        tab[j] = n;

    return 0;
}


/*!
 * \brief   makeGrayQuantColormapArb()
 *
 * \param[in]    pixs       8 bpp
 * \param[in]    tab        table mapping input gray level to cmap index
 * \param[in]    outdepth   of colormap: 1, 2, 4 or 8
 * \param[out]   pcmap      colormap
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The table is a 256-entry inverse colormap: it maps input gray
 *          level to colormap index (the bin number).  It is computed
 *          using makeGrayQuantTableArb().
 *      (2) The colormap generated here has quantized values at the
 *          average gray value of the pixels that are in each bin.
 *      (3) Returns an error if there are not enough levels in the
 *          output colormap for the number of bins.  The number
 *          of bins must not exceed 2^outdepth.
 * </pre>
 */
static l_int32
makeGrayQuantColormapArb(PIX       *pixs,
                         l_int32   *tab,
                         l_int32    outdepth,
                         PIXCMAP  **pcmap)
{
l_int32    i, j, index, w, h, d, nbins, wpl, factor, val;
l_int32   *bincount, *binave, *binstart;
l_uint32  *line, *data;

    if (!pcmap)
        return ERROR_INT("&cmap not defined", __func__, 1);
    *pcmap = NULL;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8)
        return ERROR_INT("pixs not 8 bpp", __func__, 1);
    if (!tab)
        return ERROR_INT("tab not defined", __func__, 1);
    nbins = tab[255] + 1;
    if (nbins > (1 << outdepth))
        return ERROR_INT("more bins than cmap levels", __func__, 1);

        /* Find the count and weighted count for each bin */
    if ((bincount = (l_int32 *)LEPT_CALLOC(nbins, sizeof(l_int32))) == NULL)
        return ERROR_INT("calloc fail for bincount", __func__, 1);
    if ((binave = (l_int32 *)LEPT_CALLOC(nbins, sizeof(l_int32))) == NULL) {
        LEPT_FREE(bincount);
        return ERROR_INT("calloc fail for binave", __func__, 1);
    }
    factor = (l_int32)(sqrt((l_float64)(w * h) / 30000.) + 0.5);
    factor = L_MAX(1, factor);
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    for (i = 0; i < h; i += factor) {
        line = data + i * wpl;
        for (j = 0; j < w; j += factor) {
            val = GET_DATA_BYTE(line, j);
            bincount[tab[val]]++;
            binave[tab[val]] += val;
        }
    }

        /* Find the smallest gray values in each bin */
    binstart = (l_int32 *)LEPT_CALLOC(nbins, sizeof(l_int32));
    for (i = 1, index = 1; i < 256; i++) {
        if (tab[i] < index) continue;
        if (tab[i] == index)
            binstart[index++] = i;
    }

        /* Get the averages.  If there are no samples in a bin, use
         * the center value of the bin. */
    *pcmap = pixcmapCreate(outdepth);
    for (i = 0; i < nbins; i++) {
        if (bincount[i]) {
            val = binave[i] / bincount[i];
        } else {  /* no samples in the bin */
            if (i < nbins - 1)
                val = (binstart[i] + binstart[i + 1]) / 2;
            else  /* last bin */
                val = (binstart[i] + 255) / 2;
        }
        pixcmapAddColor(*pcmap, val, val, val);
    }

    LEPT_FREE(bincount);
    LEPT_FREE(binave);
    LEPT_FREE(binstart);
    return 0;
}


/*--------------------------------------------------------------------*
 *                 Thresholding from 32 bpp rgb to 1 bpp              *
 *--------------------------------------------------------------------*/
/*!
 * \brief   pixGenerateMaskByBand32()
 *
 * \param[in]    pixs     32 bpp
 * \param[in]    refval   reference rgb value
 * \param[in]    delm     max amount below the ref value for any component
 * \param[in]    delp     max amount above the ref value for any component
 * \param[in]    fractm   fractional amount below ref value for all components
 * \param[in]    fractp   fractional amount above ref value for all components
 * \return  pixd 1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Generates a 1 bpp mask pixd, the same size as pixs, where
 *          the fg pixels in the mask within a band of rgb values
 *          surrounding %refval.  The band can be chosen in two ways
 *          for each component:
 *          (a) Use (%delm, %delp) to specify how many levels down and up
 *          (b) Use (%fractm, %fractp) to specify the fractional
 *              distance toward 0 and 255, respectively.
 *          Note that %delm and %delp must be in [0 ... 255], whereas
 *          %fractm and %fractp must be in [0.0 - 1.0].
 *      (2) Either (%delm, %delp) or (%fractm, %fractp) can be used.
 *          Set each value in the other pair to 0.
 * </pre>
 */
PIX *
pixGenerateMaskByBand32(PIX       *pixs,
                        l_uint32   refval,
                        l_int32    delm,
                        l_int32    delp,
                        l_float32  fractm,
                        l_float32  fractp)
{
l_int32    i, j, w, h, d, wpls, wpld;
l_int32    rref, gref, bref, rval, gval, bval;
l_int32    rmin, gmin, bmin, rmax, gmax, bmax;
l_uint32   pixel;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 32)
        return (PIX *)ERROR_PTR("not 32 bpp", __func__, NULL);
    if (delm < 0 || delp < 0)
        return (PIX *)ERROR_PTR("delm and delp must be >= 0", __func__, NULL);
    if (fractm < 0.0 || fractm > 1.0 || fractp < 0.0 || fractp > 1.0)
        return (PIX *)ERROR_PTR("fractm and/or fractp invalid", __func__, NULL);

    extractRGBValues(refval, &rref, &gref, &bref);
    if (fractm == 0.0 && fractp == 0.0) {
        rmin = rref - delm;
        gmin = gref - delm;
        bmin = bref - delm;
        rmax = rref + delm;
        gmax = gref + delm;
        bmax = bref + delm;
    } else if (delm == 0 && delp == 0) {
        rmin = (l_int32)((1.0 - fractm) * rref);
        gmin = (l_int32)((1.0 - fractm) * gref);
        bmin = (l_int32)((1.0 - fractm) * bref);
        rmax = rref + (l_int32)(fractp * (255 - rref));
        gmax = gref + (l_int32)(fractp * (255 - gref));
        bmax = bref + (l_int32)(fractp * (255 - bref));
    } else {
        L_ERROR("bad input: either (delm, delp) or (fractm, fractp) "
                "must be 0\n", __func__);
        return NULL;
    }

    pixd = pixCreate(w, h, 1);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            pixel = lines[j];
            rval = (pixel >> L_RED_SHIFT) & 0xff;
            if (rval < rmin || rval > rmax)
                continue;
            gval = (pixel >> L_GREEN_SHIFT) & 0xff;
            if (gval < gmin || gval > gmax)
                continue;
            bval = (pixel >> L_BLUE_SHIFT) & 0xff;
            if (bval < bmin || bval > bmax)
                continue;
            SET_DATA_BIT(lined, j);
        }
    }

    return pixd;
}


/*!
 * \brief   pixGenerateMaskByDiscr32()
 *
 * \param[in]    pixs       32 bpp
 * \param[in]    refval1    reference rgb value
 * \param[in]    refval2    reference rgb value
 * \param[in]    distflag   L_MANHATTAN_DISTANCE, L_EUCLIDEAN_DISTANCE
 * \return  pixd 1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Generates a 1 bpp mask pixd, the same size as pixs, where
 *          the fg pixels in the mask are those where the pixel in pixs
 *          is "closer" to refval1 than to refval2.
 *      (2) "Closer" can be defined in several ways, such as:
 *            ~ manhattan distance (L1)
 *            ~ euclidean distance (L2)
 *            ~ majority vote of the individual components
 *          Here, we have a choice of L1 or L2.
 * </pre>
 */
PIX *
pixGenerateMaskByDiscr32(PIX      *pixs,
                         l_uint32  refval1,
                         l_uint32  refval2,
                         l_int32   distflag)
{
l_int32    i, j, w, h, d, wpls, wpld;
l_int32    rref1, gref1, bref1, rref2, gref2, bref2, rval, gval, bval;
l_uint32   pixel, dist1, dist2;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 32)
        return (PIX *)ERROR_PTR("not 32 bpp", __func__, NULL);
    if (distflag != L_MANHATTAN_DISTANCE && distflag != L_EUCLIDEAN_DISTANCE)
        return (PIX *)ERROR_PTR("invalid distflag", __func__, NULL);

    extractRGBValues(refval1, &rref1, &gref1, &bref1);
    extractRGBValues(refval2, &rref2, &gref2, &bref2);
    pixd = pixCreate(w, h, 1);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            pixel = lines[j];
            extractRGBValues(pixel, &rval, &gval, &bval);
            if (distflag == L_MANHATTAN_DISTANCE) {
                dist1 = L_ABS(rref1 - rval);
                dist2 = L_ABS(rref2 - rval);
                dist1 += L_ABS(gref1 - gval);
                dist2 += L_ABS(gref2 - gval);
                dist1 += L_ABS(bref1 - bval);
                dist2 += L_ABS(bref2 - bval);
            } else {
                dist1 = (rref1 - rval) * (rref1 - rval);
                dist2 = (rref2 - rval) * (rref2 - rval);
                dist1 += (gref1 - gval) * (gref1 - gval);
                dist2 += (gref2 - gval) * (gref2 - gval);
                dist1 += (bref1 - bval) * (bref1 - bval);
                dist2 += (bref2 - bval) * (bref2 - bval);
            }
            if (dist1 < dist2)
                SET_DATA_BIT(lined, j);
        }
    }

    return pixd;
}


/*----------------------------------------------------------------------*
 *                Histogram-based grayscale quantization                *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixGrayQuantFromHisto()
 *
 * \param[in]    pixd       [optional] quantized pix with cmap; can be null
 * \param[in]    pixs       8 bpp gray input pix; not cmapped
 * \param[in]    pixm       [optional] mask over pixels in pixs to quantize
 * \param[in]    minfract   minimum fraction of pixels in a set of adjacent
 *                          histo bins that causes the set to be automatically
 *                          set aside as a color in the colormap; must be
 *                          at least 0.01
 * \param[in]    maxsize    maximum number of adjacent bins allowed to represent
 *                          a color, regardless of the population of pixels
 *                          in the bins; must be at least 2
 * \return  pixd 8 bpp, cmapped, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is useful for quantizing images with relatively few
 *          colors, but which may have both color and gray pixels.
 *          If there are color pixels, it is assumed that an input
 *          rgb image has been color quantized first so that:
 *            ~ pixd has a colormap describing the color pixels
 *            ~ pixm is a mask over the non-color pixels in pixd
 *            ~ the colormap in pixd, and the color pixels in pixd,
 *              have been repacked to go from 0 to n-1 (n colors)
 *          If there are no color pixels, pixd and pixm are both null,
 *          and all pixels in pixs are quantized to gray.
 *      (2) A 256-entry histogram is built of the gray values in pixs.
 *          If pixm exists, the pixels contributing to the histogram are
 *          restricted to the fg of pixm.  A colormap and LUT are generated
 *          from this histogram.  We break up the array into a set
 *          of intervals, each one constituting a color in the colormap:
 *          An interval is identified by summing histogram bins until
 *          either the sum equals or exceeds the %minfract of the total
 *          number of pixels, or the span itself equals or exceeds %maxsize.
 *          The color of each bin is always an average of the pixels
 *          that constitute it.
 *      (3) Note that we do not specify the number of gray colors in
 *          the colormap.  Instead, we specify two parameters that
 *          describe the accuracy of the color assignments; this and
 *          the actual image determine the number of resulting colors.
 *      (4) If a mask exists and it is not the same size as pixs, make
 *          a new mask the same size as pixs, with the original mask
 *          aligned at the UL corners.  Set all additional pixels
 *          in the (larger) new mask set to 1, causing those pixels
 *          in pixd to be set as gray.
 *      (5) We estimate the total number of colors (color plus gray);
 *          if it exceeds 255, return null.
 * </pre>
 */
PIX *
pixGrayQuantFromHisto(PIX       *pixd,
                      PIX       *pixs,
                      PIX       *pixm,
                      l_float32  minfract,
                      l_int32    maxsize)
{
l_int32    w, h, wd, hd, wm, hm, wpls, wplm, wpld;
l_int32    nc, nestim, i, j, vals, vald;
l_int32   *lut;
l_uint32  *datas, *datam, *datad, *lines, *linem, *lined;
NUMA      *na;
PIX       *pixmr = NULL;  /* resized mask */
PIXCMAP   *cmap;

    if (!pixs || pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs undefined or not 8 bpp", __func__, NULL);
    if (minfract < 0.01) {
        L_WARNING("minfract < 0.01; setting to 0.05\n", __func__);
        minfract = 0.05f;
    }
    if (maxsize < 2) {
        L_WARNING("maxsize < 2; setting to 10\n", __func__);
        maxsize = 10;
    }
    if ((pixd && !pixm) || (!pixd && pixm))
        return (PIX *)ERROR_PTR("(pixd,pixm) not defined together",
                                __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (pixd) {
        if (pixGetDepth(pixm) != 1)
            return (PIX *)ERROR_PTR("pixm not 1 bpp", __func__, NULL);
        if ((cmap = pixGetColormap(pixd)) == NULL)
            return (PIX *)ERROR_PTR("pixd not cmapped", __func__, NULL);
        pixGetDimensions(pixd, &wd, &hd, NULL);
        if (w != wd || h != hd)
            return (PIX *)ERROR_PTR("pixs, pixd sizes differ", __func__, NULL);
        nc = pixcmapGetCount(cmap);
        nestim = nc + (l_int32)(1.5 * 255 / maxsize);
        lept_stderr( "nestim = %d\n", nestim);
        if (nestim > 255) {
            L_ERROR("Estimate %d colors!\n", __func__, nestim);
            return (PIX *)ERROR_PTR("probably too many colors", __func__, NULL);
        }
        pixGetDimensions(pixm, &wm, &hm, NULL);
        if (w != wm || h != hm) {  /* resize the mask */
            L_WARNING("mask and dest sizes not equal\n", __func__);
            pixmr = pixCreate(w, h, 1);
            pixRasterop(pixmr, 0, 0, wm, hm, PIX_SRC, pixm, 0, 0);
            pixRasterop(pixmr, wm, 0, w - wm, h, PIX_SET, NULL, 0, 0);
            pixRasterop(pixmr, 0, hm, wm, h - hm, PIX_SET, NULL, 0, 0);
        } else {
            pixmr = pixClone(pixm);
        }
    } else {
        pixd = pixCreateTemplate(pixs);
        cmap = pixcmapCreate(8);
        pixSetColormap(pixd, cmap);
    }
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);

        /* Use original mask, if it exists, to select gray pixels */
    na = pixGetGrayHistogramMasked(pixs, pixm, 0, 0, 1);

        /* Fill out the cmap with gray colors, and generate the lut
         * for pixel assignment.  Issue a warning on failure.  */
    if (numaFillCmapFromHisto(na, cmap, minfract, maxsize, &lut))
        L_ERROR("ran out of colors in cmap!\n", __func__);
    numaDestroy(&na);

        /* Assign the gray pixels to their cmap indices */
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);
    if (!pixm) {
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {
                vals = GET_DATA_BYTE(lines, j);
                vald = lut[vals];
                SET_DATA_BYTE(lined, j, vald);
            }
        }
        LEPT_FREE(lut);
        return pixd;
    }

    datam = pixGetData(pixmr);
    wplm = pixGetWpl(pixmr);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        linem = datam + i * wplm;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            if (!GET_DATA_BIT(linem, j))
                continue;
            vals = GET_DATA_BYTE(lines, j);
            vald = lut[vals];
            SET_DATA_BYTE(lined, j, vald);
        }
    }
    pixDestroy(&pixmr);
    LEPT_FREE(lut);
    return pixd;
}


/*!
 * \brief   numaFillCmapFromHisto()
 *
 * \param[in]    na         histogram of gray values
 * \param[in]    cmap       8 bpp cmap, possibly initialized with color value
 * \param[in]    minfract   minimum fraction of pixels in a set of adjacent
 *                          histo bins that causes the set to be automatically
 *                          set aside as a color in the colormap; must be
 *                          at least 0.01
 * \param[in]    maxsize    maximum number of adjacent bins allowed to represent
 *                          a color, regardless of the population of pixels
 *                          in the bins; must be at least 2
 * \param[out]  plut        lookup table from gray value to colormap index
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This static function must be called from pixGrayQuantFromHisto()
 * </pre>
 */
static l_int32
numaFillCmapFromHisto(NUMA      *na,
                      PIXCMAP   *cmap,
                      l_float32  minfract,
                      l_int32    maxsize,
                      l_int32  **plut)
{
l_int32    mincount, index, sum, wtsum, span, istart, i, val, ret;
l_int32   *iahisto, *lut;
l_float32  total;

    if (!plut)
        return ERROR_INT("&lut not defined", __func__, 1);
    *plut = NULL;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if (!cmap)
        return ERROR_INT("cmap not defined", __func__, 1);

    numaGetSum(na, &total);
    mincount = (l_int32)(minfract * total);
    iahisto = numaGetIArray(na);
    lut = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    *plut = lut;
    index = pixcmapGetCount(cmap);  /* start with number of colors
                                     * already reserved */

        /* March through, associating colors with sets of adjacent
         * gray levels.  During the process, the LUT that gives
         * the colormap index for each gray level is computed.
         * To complete a color, either the total count must equal
         * or exceed %mincount, or the current span of colors must
         * equal or exceed %maxsize.  An empty span is not converted
         * into a color; it is simply ignored.  When a span is completed for a
         * color, the weighted color in the span is added to the colormap. */
    sum = 0;
    wtsum = 0;
    istart = 0;
    ret = 0;
    for (i = 0; i < 256; i++) {
        lut[i] = index;
        sum += iahisto[i];
        wtsum += i * iahisto[i];
        span = i - istart + 1;
        if (sum < mincount && span < maxsize)
            continue;

        if (sum == 0) {  /* empty span; don't save */
            istart = i + 1;
            continue;
        }

            /* Found new color; sum > 0 */
        val = (l_int32)((l_float32)wtsum / (l_float32)sum + 0.5);
        ret = pixcmapAddColor(cmap, val, val, val);
        istart = i + 1;
        sum = 0;
        wtsum = 0;
        index++;
    }
    if (istart < 256 && sum > 0) {  /* last one */
        span = 256 - istart;
        val = (l_int32)((l_float32)wtsum / (l_float32)sum + 0.5);
        ret = pixcmapAddColor(cmap, val, val, val);
    }

    LEPT_FREE(iahisto);
    return ret;
}


/*----------------------------------------------------------------------*
 *        Color quantize grayscale image using existing colormap        *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixGrayQuantFromCmap()
 *
 * \param[in]    pixs       8 bpp grayscale without cmap
 * \param[in]    cmap       to quantize to; of dest pix
 * \param[in]    mindepth   minimum depth of pixd: can be 2, 4 or 8 bpp
 * \return  pixd 2, 4 or 8 bpp, colormapped, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) In use, pixs is an 8 bpp grayscale image without a colormap.
 *          If there is an existing colormap, a warning is issued and
 *          a copy of the input pixs is returned.
 * </pre>
 */
PIX *
pixGrayQuantFromCmap(PIX      *pixs,
                     PIXCMAP  *cmap,
                     l_int32   mindepth)
{
l_int32    i, j, index, w, h, d, depth, wpls, wpld;
l_int32    hascolor, vals, vald;
l_int32   *tab;
l_uint32  *datas, *datad, *lines, *lined;
PIXCMAP   *cmapd;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetColormap(pixs) != NULL) {
        L_WARNING("pixs already has a colormap; returning a copy\n", __func__);
        return pixCopy(NULL, pixs);
    }
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (!cmap)
        return (PIX *)ERROR_PTR("cmap not defined", __func__, NULL);
    if (mindepth != 2 && mindepth != 4 && mindepth != 8)
        return (PIX *)ERROR_PTR("invalid mindepth", __func__, NULL);

        /* Make sure the colormap is gray */
    pixcmapHasColor(cmap, &hascolor);
    if (hascolor) {
        L_WARNING("Converting colormap colors to gray\n", __func__);
        cmapd = pixcmapColorToGray(cmap, 0.3f, 0.5f, 0.2f);
    } else {
        cmapd = pixcmapCopy(cmap);
    }

        /* Make LUT into colormap */
    tab = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    for (i = 0; i < 256; i++) {
        pixcmapGetNearestGrayIndex(cmapd, i, &index);
        tab[i] = index;
    }

    pixcmapGetMinDepth(cmap, &depth);
    depth = L_MAX(depth, mindepth);
    pixd = pixCreate(w, h, depth);
    pixSetColormap(pixd, cmapd);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            vals = GET_DATA_BYTE(lines, j);
            vald = tab[vals];
            if (depth == 2)
                SET_DATA_DIBIT(lined, j, vald);
            else if (depth == 4)
                SET_DATA_QBIT(lined, j, vald);
            else  /* depth == 8 */
                SET_DATA_BYTE(lined, j, vald);
        }
    }

    LEPT_FREE(tab);
    return pixd;
}


#if 0   /* Documentation */
/*--------------------------------------------------------------------*
 *       Implementation of binarization by dithering using LUTs       *
 *                        It is archived here.                        *
 *--------------------------------------------------------------------*/
/*!
 * \brief   pixDitherToBinaryLUT()
 *
 * \param[in]    pixs
 * \param[in]    lowerclip  lower clip distance to black; use -1 for default
 * \param[in]    upperclip  upper clip distance to white; use -1 for default
 * \return  pixd dithered binary, or NULL on error
 *
 *  We don't need two implementations of Floyd-Steinberg dithering,
 *  and this one with LUTs is a little more complicated than
 *  pixDitherToBinary().  It uses three lookup tables to generate the
 *  output pixel value and the excess or deficit carried over to the
 *  neighboring pixels.  It's here for pedagogical reasons only.
 */
PIX *
pixDitherToBinaryLUT(PIX     *pixs,
                     l_int32  lowerclip,
                     l_int32  upperclip)
{
l_int32    w, h, d, wplt, wpld;
l_int32   *tabval, *tab38, *tab14;
l_uint32  *datat, *datad;
l_uint32  *bufs1, *bufs2;
PIX       *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8)
        return (PIX *)ERROR_PTR("must be 8 bpp for dithering", __func__, NULL);
    if (lowerclip < 0)
        lowerclip = DEFAULT_CLIP_LOWER_1;
    if (upperclip < 0)
        upperclip = DEFAULT_CLIP_UPPER_1;

    if ((pixd = pixCreate(w, h, 1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

        /* Remove colormap if it exists */
    pixt = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    datat = pixGetData(pixt);
    wplt = pixGetWpl(pixt);

        /* Two line buffers, 1 for current line and 2 for next line */
    bufs1 = (l_uint32 *)LEPT_CALLOC(wplt, sizeof(l_uint32));
    bufs2 = (l_uint32 *)LEPT_CALLOC(wplt, sizeof(l_uint32));
    if (!bufs1 || !bufs2) {
        LEPT_FREE(bufs1);
        LEPT_FREE(bufs2);
        pixDestroy(&pixd);
        pixDestroy(&pixt);
        return (PIX *)ERROR_PTR("bufs1, bufs2 not both made", __func__, NULL);
    }

        /* 3 lookup tables: 1-bit value, (3/8)excess, and (1/4)excess */
    make8To1DitherTables(&tabval, &tab38, &tab14, lowerclip, upperclip);

    ditherToBinaryLUTLow(datad, w, h, wpld, datat, wplt, bufs1, bufs2,
                         tabval, tab38, tab14);

    LEPT_FREE(bufs1);
    LEPT_FREE(bufs2);
    LEPT_FREE(tabval);
    LEPT_FREE(tab38);
    LEPT_FREE(tab14);
    pixDestroy(&pixt);
    return pixd;
}

/*!
 * \brief   ditherToBinaryLUTLow()
 *
 *  Low-level function for doing Floyd-Steinberg error diffusion
 *  dithering from 8 bpp (datas) to 1 bpp (datad).  Two source
 *  line buffers, bufs1 and bufs2, are provided, along with three
 *  256-entry lookup tables: tabval gives the output pixel value,
 *  tab38 gives the extra (plus or minus) transferred to the pixels
 *  directly to the left and below, and tab14 gives the extra
 *  transferred to the diagonal below.  The choice of 3/8 and 1/4
 *  is traditional but arbitrary when you use a lookup table; the
 *  only constraint is that the sum is 1.  See other comments below.
 */
void
ditherToBinaryLUTLow(l_uint32  *datad,
                     l_int32    w,
                     l_int32    h,
                     l_int32    wpld,
                     l_uint32  *datas,
                     l_int32    wpls,
                     l_uint32  *bufs1,
                     l_uint32  *bufs2,
                     l_int32   *tabval,
                     l_int32   *tab38,
                     l_int32   *tab14)
{
l_int32      i;
l_uint32    *lined;

        /* do all lines except last line */
    memcpy(bufs2, datas, 4 * wpls);  /* prime the buffer */
    for (i = 0; i < h - 1; i++) {
        memcpy(bufs1, bufs2, 4 * wpls);
        memcpy(bufs2, datas + (i + 1) * wpls, 4 * wpls);
        lined = datad + i * wpld;
        ditherToBinaryLineLUTLow(lined, w, bufs1, bufs2,
                                 tabval, tab38, tab14, 0);
    }

        /* do last line */
    memcpy(bufs1, bufs2, 4 * wpls);
    lined = datad + (h - 1) * wpld;
    ditherToBinaryLineLUTLow(lined, w, bufs1, bufs2, tabval, tab38, tab14,  1);
    return;
}

/*!
 * \brief   ditherToBinaryLineLUTLow()
 *
 * \param[in]    lined          ptr to beginning of dest line
 * \param[in]    w              width of image in pixels
 * \param[in]    bufs1          buffer of current source line
 * \param[in]    bufs2          buffer of next source line
 * \param[in]    tabval         value to assign for current pixel
 * \param[in]    tab38          excess value to give to neighboring 3/8 pixels
 * \param[in]    tab14          excess value to give to neighboring 1/4 pixel
 * \param[in]    lastlineflag   0 if not last dest line, 1 if last dest line
 * \return  void
 */
void
ditherToBinaryLineLUTLow(l_uint32  *lined,
                         l_int32    w,
                         l_uint32  *bufs1,
                         l_uint32  *bufs2,
                         l_int32   *tabval,
                         l_int32   *tab38,
                         l_int32   *tab14,
                         l_int32    lastlineflag)
{
l_int32  j;
l_int32  oval, tab38val, tab14val;
l_uint8  rval, bval, dval;

    if (lastlineflag == 0) {
        for (j = 0; j < w - 1; j++) {
            oval = GET_DATA_BYTE(bufs1, j);
            if (tabval[oval])
                SET_DATA_BIT(lined, j);
            rval = GET_DATA_BYTE(bufs1, j + 1);
            bval = GET_DATA_BYTE(bufs2, j);
            dval = GET_DATA_BYTE(bufs2, j + 1);
            tab38val = tab38[oval];
            if (tab38val == 0)
                continue;
            tab14val = tab14[oval];
            if (tab38val < 0) {
                rval = L_MAX(0, rval + tab38val);
                bval = L_MAX(0, bval + tab38val);
                dval = L_MAX(0, dval + tab14val);
            } else {
                rval = L_MIN(255, rval + tab38val);
                bval = L_MIN(255, bval + tab38val);
                dval = L_MIN(255, dval + tab14val);
            }
            SET_DATA_BYTE(bufs1, j + 1, rval);
            SET_DATA_BYTE(bufs2, j, bval);
            SET_DATA_BYTE(bufs2, j + 1, dval);
        }

            /* do last column: j = w - 1 */
        oval = GET_DATA_BYTE(bufs1, j);
        if (tabval[oval])
            SET_DATA_BIT(lined, j);
        bval = GET_DATA_BYTE(bufs2, j);
        tab38val = tab38[oval];
        if (tab38val < 0) {
            bval = L_MAX(0, bval + tab38val);
            SET_DATA_BYTE(bufs2, j, bval);
        } else if (tab38val > 0 ) {
            bval = L_MIN(255, bval + tab38val);
            SET_DATA_BYTE(bufs2, j, bval);
        }
    } else {   /* lastlineflag == 1 */
        for (j = 0; j < w - 1; j++) {
            oval = GET_DATA_BYTE(bufs1, j);
            if (tabval[oval])
                SET_DATA_BIT(lined, j);
            rval = GET_DATA_BYTE(bufs1, j + 1);
            tab38val = tab38[oval];
            if (tab38val == 0)
                continue;
            if (tab38val < 0)
                rval = L_MAX(0, rval + tab38val);
            else
                rval = L_MIN(255, rval + tab38val);
            SET_DATA_BYTE(bufs1, j + 1, rval);
        }

            /* do last pixel: (i, j) = (h - 1, w - 1) */
        oval = GET_DATA_BYTE(bufs1, j);
        if (tabval[oval])
            SET_DATA_BIT(lined, j);
    }

    return;
}

/*!
 * \brief   make8To1DitherTables()
 *
 * \param[out]  ptabval     value assigned to output pixel; 0 or 1
 * \param[out]  ptab38      amount propagated to pixels left and below
 * \param[out]  ptab14      amount propagated to pixel to left and down
 * \param[in]   lowerclip   values near 0 where the excess is not propagated
 * \param[in]   upperclip   values near 255 where the deficit is not propagated
 *
 * \return  0 if OK, 1 on error
 */
l_ok
make8To1DitherTables(l_int32 **ptabval,
                     l_int32 **ptab38,
                     l_int32 **ptab14,
                     l_int32   lowerclip,
                     l_int32   upperclip)
{
l_int32   i;
l_int32  *tabval, *tab38, *tab14;

    if (ptabval) *ptabval = NULL;
    if (ptab38) *ptab38 = NULL;
    if (ptab14) *ptab14 = NULL;
    if (!ptabval || !ptab38 || !ptab14)
        return ERROR_INT("table ptrs not all defined", __func__, 1);

        /* 3 lookup tables: 1-bit value, (3/8)excess, and (1/4)excess */
    tabval = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    tab38 = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    tab14 = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    if (!tabval || !tab38 || !tab14)
        return ERROR_INT("calloc failure to make small table", __func__, 1);
    *ptabval = tabval;
    *ptab38 = tab38;
    *ptab14 = tab14;

    for (i = 0; i < 256; i++) {
        if (i <= lowerclip) {
            tabval[i] = 1;
            tab38[i] = 0;
            tab14[i] = 0;
        } else if (i < 128) {
            tabval[i] = 1;
            tab38[i] = (3 * i + 4) / 8;
            tab14[i] = (i + 2) / 4;
        } else if (i < 255 - upperclip) {
            tabval[i] = 0;
            tab38[i] = (3 * (i - 255) + 4) / 8;
            tab14[i] = ((i - 255) + 2) / 4;
        } else {  /* i >= 255 - upperclip */
            tabval[i] = 0;
            tab38[i] = 0;
            tab14[i] = 0;
        }
    }

    return 0;
}
#endif   /* Documentation */

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
 * \file pix3.c
 * <pre>
 *
 *    This file has these operations:
 *
 *      (1) Mask-directed operations
 *      (2) Full-image bit-logical operations
 *      (3) Foreground pixel counting operations on 1 bpp images
 *      (4) Average and variance of pixel values
 *      (5) Mirrored tiling of a smaller image
 *
 *
 *    Masked operations
 *           l_int32     pixSetMasked()
 *           l_int32     pixSetMaskedGeneral()
 *           l_int32     pixCombineMasked()
 *           l_int32     pixCombineMaskedGeneral()
 *           l_int32     pixPaintThroughMask()
 *           l_int32     pixCopyWithBoxa()  -- this is boxa-directed
 *           PIX        *pixPaintSelfThroughMask()
 *           PIX        *pixMakeMaskFromVal()
 *           PIX        *pixMakeMaskFromLUT()
 *           PIX        *pixMakeArbMaskFromRGB()
 *           PIX        *pixSetUnderTransparency()
 *           PIX        *pixMakeAlphaFromMask()
 *           l_int32     pixGetColorNearMaskBoundary()
 *           PIX        *pixDisplaySelectedPixels()  -- for debugging
 *
 *    One and two-image boolean operations on arbitrary depth images
 *           PIX        *pixInvert()
 *           PIX        *pixOr()
 *           PIX        *pixAnd()
 *           PIX        *pixXor()
 *           PIX        *pixSubtract()
 *
 *    Foreground pixel counting in 1 bpp images
 *           l_int32     pixZero()
 *           l_int32     pixForegroundFraction()
 *           NUMA       *pixaCountPixels()
 *           l_int32     pixCountPixels()
 *           l_int32     pixCountPixelsInRect()
 *           NUMA       *pixCountByRow()
 *           NUMA       *pixCountByColumn()
 *           NUMA       *pixCountPixelsByRow()
 *           NUMA       *pixCountPixelsByColumn()
 *           l_int32     pixCountPixelsInRow()
 *           NUMA       *pixGetMomentByColumn()
 *           l_int32     pixThresholdPixelSum()
 *           l_int32    *makePixelSumTab8()
 *           l_int32    *makePixelCentroidTab8()
 *
 *    Average of pixel values in gray images
 *           NUMA       *pixAverageByRow()
 *           NUMA       *pixAverageByColumn()
 *           l_int32     pixAverageInRect()
 *
 *    Average of pixel values in RGB images
 *           l_int32     pixAverageInRectRGB()
 *
 *    Variance of pixel values in gray images
 *           NUMA       *pixVarianceByRow()
 *           NUMA       *pixVarianceByColumn()
 *           l_int32     pixVarianceInRect()
 *
 *    Average of absolute value of pixel differences in gray images
 *           NUMA       *pixAbsDiffByRow()
 *           NUMA       *pixAbsDiffByColumn()
 *           l_int32     pixAbsDiffInRect()
 *           l_int32     pixAbsDiffOnLine()
 *
 *    Count of pixels with specific value
 *           l_int32     pixCountArbInRect()
 *
 *    Mirrored tiling
 *           PIX        *pixMirroredTiling()
 *
 *    Representative tile near but outside region
 *           l_int32     pixFindRepCloseTile()
 *
 *    Static helper function
 *           static BOXA    *findTileRegionsForSearch()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>
#include "allheaders.h"

static BOXA *findTileRegionsForSearch(BOX *box, l_int32 w, l_int32 h,
                                      l_int32 searchdir, l_int32 mindist,
                                      l_int32 tsize, l_int32 ntiles);

#ifndef  NO_CONSOLE_IO
#define   EQUAL_SIZE_WARNING      0
#endif  /* ~NO_CONSOLE_IO */

/*-------------------------------------------------------------*
 *                        Masked operations                    *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixSetMasked()
 *
 * \param[in]   pixd   1, 2, 4, 8, 16 or 32 bpp; or colormapped
 * \param[in]   pixm   [optional] 1 bpp mask; no operation if NULL
 * \param[in]   val    value to set at each masked pixel
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) In-place operation.
 *      (2) NOTE: For cmapped images, this calls pixSetMaskedCmap().
 *          %val must be the 32-bit color representation of the RGB pixel.
 *          It is not the index into the colormap!
 *      (2) If pixm == NULL, a warning is given.
 *      (3) This is an implicitly aligned operation, where the UL
 *          corners of pixd and pixm coincide.  A warning is
 *          issued if the two image sizes differ significantly,
 *          but the operation proceeds.
 *      (4) Each pixel in pixd that co-locates with an ON pixel
 *          in pixm is set to the specified input value.
 *          Other pixels in pixd are not changed.
 *      (5) You can visualize this as painting the color through
 *          the mask, as a stencil.
 *      (6) If you do not want to have the UL corners aligned,
 *          use the function pixSetMaskedGeneral(), which requires
 *          you to input the UL corner of pixm relative to pixd.
 *      (7) Implementation details: see comments in pixPaintThroughMask()
 *          for when we use rasterop to do the painting.
 * </pre>
 */
l_ok
pixSetMasked(PIX      *pixd,
             PIX      *pixm,
             l_uint32  val)
{
l_int32    wd, hd, wm, hm, w, h, d, wpld, wplm;
l_int32    i, j, rval, gval, bval;
l_uint32  *datad, *datam, *lined, *linem;

    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (!pixm) {
        L_WARNING("no mask; nothing to do\n", __func__);
        return 0;
    }
    if (pixGetColormap(pixd)) {
        extractRGBValues(val, &rval, &gval, &bval);
        return pixSetMaskedCmap(pixd, pixm, 0, 0, rval, gval, bval);
    }

    if (pixGetDepth(pixm) != 1)
        return ERROR_INT("pixm not 1 bpp", __func__, 1);
    d = pixGetDepth(pixd);
    if (d == 1)
        val &= 1;
    else if (d == 2)
        val &= 3;
    else if (d == 4)
        val &= 0x0f;
    else if (d == 8)
        val &= 0xff;
    else if (d == 16)
        val &= 0xffff;
    else if (d != 32)
        return ERROR_INT("pixd not 1, 2, 4, 8, 16 or 32 bpp", __func__, 1);
    pixGetDimensions(pixm, &wm, &hm, NULL);

        /* If d == 1, use rasterop; it's about 25x faster */
    if (d == 1) {
        if (val == 0) {
            PIX *pixmi = pixInvert(NULL, pixm);
            pixRasterop(pixd, 0, 0, wm, hm, PIX_MASK, pixmi, 0, 0);
            pixDestroy(&pixmi);
        } else {  /* val == 1 */
            pixRasterop(pixd, 0, 0, wm, hm, PIX_PAINT, pixm, 0, 0);
        }
        return 0;
    }

        /* For d < 32, use rasterop for val == 0 (black); ~3x faster. */
    if (d < 32 && val == 0) {
        PIX *pixmd = pixUnpackBinary(pixm, d, 1);
        pixRasterop(pixd, 0, 0, wm, hm, PIX_MASK, pixmd, 0, 0);
        pixDestroy(&pixmd);
        return 0;
    }

        /* For d < 32, use rasterop for val == maxval (white); ~3x faster. */
    if (d < 32 && val == ((1 << d) - 1)) {
        PIX *pixmd = pixUnpackBinary(pixm, d, 0);
        pixRasterop(pixd, 0, 0, wm, hm, PIX_PAINT, pixmd, 0, 0);
        pixDestroy(&pixmd);
        return 0;
    }

    pixGetDimensions(pixd, &wd, &hd, &d);
    w = L_MIN(wd, wm);
    h = L_MIN(hd, hm);
    if (L_ABS(wd - wm) > 7 || L_ABS(hd - hm) > 7)  /* allow a small tolerance */
        L_WARNING("pixd and pixm sizes differ\n", __func__);

    datad = pixGetData(pixd);
    datam = pixGetData(pixm);
    wpld = pixGetWpl(pixd);
    wplm = pixGetWpl(pixm);
    for (i = 0; i < h; i++) {
        lined = datad + i * wpld;
        linem = datam + i * wplm;
        for (j = 0; j < w; j++) {
            if (GET_DATA_BIT(linem, j)) {
                switch(d)
                {
                case 2:
                    SET_DATA_DIBIT(lined, j, val);
                    break;
                case 4:
                    SET_DATA_QBIT(lined, j, val);
                    break;
                case 8:
                    SET_DATA_BYTE(lined, j, val);
                    break;
                case 16:
                    SET_DATA_TWO_BYTES(lined, j, val);
                    break;
                case 32:
                    *(lined + j) = val;
                    break;
                default:
                    return ERROR_INT("shouldn't get here", __func__, 1);
                }
            }
        }
    }

    return 0;
}


/*!
 * \brief   pixSetMaskedGeneral()
 *
 * \param[in]   pixd    8, 16 or 32 bpp
 * \param[in]   pixm    [optional] 1 bpp mask; no operation if null
 * \param[in]   val     value to set at each masked pixel
 * \param[in]   x, y    location of UL corner of pixm relative to pixd;
 *                      can be negative
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is an in-place operation.
 *      (2) Alignment is explicit.  If you want the UL corners of
 *          the two images to be aligned, use pixSetMasked().
 *      (3) A typical use would be painting through the foreground
 *          of a small binary mask pixm, located somewhere on a
 *          larger pixd.  Other pixels in pixd are not changed.
 *      (4) You can visualize this as painting the color through
 *          the mask, as a stencil.
 *      (5) This uses rasterop to handle clipping and different depths of pixd.
 *      (6) If pixd has a colormap, you should call pixPaintThroughMask().
 *      (7) Why is this function here, if pixPaintThroughMask() does the
 *          same thing, and does it more generally?  I've retained it here
 *          to show how one can paint through a mask using only full
 *          image rasterops, rather than pixel peeking in pixm and poking
 *          in pixd.  It's somewhat baroque, but I found it amusing.
 * </pre>
 */
l_ok
pixSetMaskedGeneral(PIX      *pixd,
                    PIX      *pixm,
                    l_uint32  val,
                    l_int32   x,
                    l_int32   y)
{
l_int32    wm, hm, d;
PIX       *pixmu, *pixc;

    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (!pixm)  /* nothing to do */
        return 0;

    d = pixGetDepth(pixd);
    if (d != 8 && d != 16 && d != 32)
        return ERROR_INT("pixd not 8, 16 or 32 bpp", __func__, 1);
    if (pixGetDepth(pixm) != 1)
        return ERROR_INT("pixm not 1 bpp", __func__, 1);

        /* Unpack binary to depth d, with inversion:  1 --> 0, 0 --> 0xff... */
    if ((pixmu = pixUnpackBinary(pixm, d, 1)) == NULL)
        return ERROR_INT("pixmu not made", __func__, 1);

        /* Clear stenciled pixels in pixd */
    pixGetDimensions(pixm, &wm, &hm, NULL);
    pixRasterop(pixd, x, y, wm, hm, PIX_SRC & PIX_DST, pixmu, 0, 0);

        /* Generate image with requisite color */
    if ((pixc = pixCreateTemplate(pixmu)) == NULL) {
        pixDestroy(&pixmu);
        return ERROR_INT("pixc not made", __func__, 1);
    }
    pixSetAllArbitrary(pixc, val);

        /* Invert stencil mask, and paint color color into stencil */
    pixInvert(pixmu, pixmu);
    pixAnd(pixmu, pixmu, pixc);

        /* Finally, repaint stenciled pixels, with val, in pixd */
    pixRasterop(pixd, x, y, wm, hm, PIX_SRC | PIX_DST, pixmu, 0, 0);

    pixDestroy(&pixmu);
    pixDestroy(&pixc);
    return 0;
}


/*!
 * \brief   pixCombineMasked()
 *
 * \param[in]   pixd   1 bpp, 8 bpp gray or 32 bpp rgb; no cmap
 * \param[in]   pixs   1 bpp, 8 bpp gray or 32 bpp rgb; no cmap
 * \param[in]   pixm   [optional] 1 bpp mask; no operation if NULL
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) In-place operation; pixd is changed.
 *      (2) This sets each pixel in pixd that co-locates with an ON
 *          pixel in pixm to the corresponding value of pixs.
 *      (3) pixs and pixd must be the same depth and not colormapped.
 *      (4) All three input pix are aligned at the UL corner, and the
 *          operation is clipped to the intersection of all three images.
 *      (5) If pixm == NULL, it's a no-op.
 *      (6) Implementation: see notes in pixCombineMaskedGeneral().
 *          For 8 bpp selective masking, you might guess that it
 *          would be faster to generate an 8 bpp version of pixm,
 *          using pixConvert1To8(pixm, 0, 255), and then use a
 *          general combine operation
 *               d = (d & ~m) | (s & m)
 *          on a word-by-word basis.  Not always.  The word-by-word
 *          combine takes a time that is independent of the mask data.
 *          If the mask is relatively sparse, the byte-check method
 *          is actually faster!
 * </pre>
 */
l_ok
pixCombineMasked(PIX  *pixd,
                 PIX  *pixs,
                 PIX  *pixm)
{
l_int32    w, h, d, ws, hs, ds, wm, hm, dm, wmin, hmin;
l_int32    wpl, wpls, wplm, i, j, val;
l_uint32  *data, *datas, *datam, *line, *lines, *linem;
PIX       *pixt;

    if (!pixm)  /* nothing to do */
        return 0;
    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixd, &w, &h, &d);
    pixGetDimensions(pixs, &ws, &hs, &ds);
    pixGetDimensions(pixm, &wm, &hm, &dm);
    if (d != ds)
        return ERROR_INT("pixs and pixd depths differ", __func__, 1);
    if (dm != 1)
        return ERROR_INT("pixm not 1 bpp", __func__, 1);
    if (d != 1 && d != 8 && d != 32)
        return ERROR_INT("pixd not 1, 8 or 32 bpp", __func__, 1);
    if (pixGetColormap(pixd) || pixGetColormap(pixs))
        return ERROR_INT("pixs and/or pixd is cmapped", __func__, 1);

        /* For d = 1, use rasterop.  pixt is the part from pixs, under
         * the fg of pixm, that is to be combined with pixd.  We also
         * use pixt to remove all fg of pixd that is under the fg of pixm.
         * Then pixt and pixd are combined by ORing. */
    wmin = L_MIN(w, L_MIN(ws, wm));
    hmin = L_MIN(h, L_MIN(hs, hm));
    if (d == 1) {
        pixt = pixAnd(NULL, pixs, pixm);
        pixRasterop(pixd, 0, 0, wmin, hmin, PIX_DST & PIX_NOT(PIX_SRC),
                    pixm, 0, 0);
        pixRasterop(pixd, 0, 0, wmin, hmin, PIX_SRC | PIX_DST, pixt, 0, 0);
        pixDestroy(&pixt);
        return 0;
    }

    data = pixGetData(pixd);
    datas = pixGetData(pixs);
    datam = pixGetData(pixm);
    wpl = pixGetWpl(pixd);
    wpls = pixGetWpl(pixs);
    wplm = pixGetWpl(pixm);
    if (d == 8) {
        for (i = 0; i < hmin; i++) {
            line = data + i * wpl;
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = 0; j < wmin; j++) {
                if (GET_DATA_BIT(linem, j)) {
                   val = GET_DATA_BYTE(lines, j);
                   SET_DATA_BYTE(line, j, val);
                }
            }
        }
    } else {  /* d == 32 */
        for (i = 0; i < hmin; i++) {
            line = data + i * wpl;
            lines = datas + i * wpls;
            linem = datam + i * wplm;
            for (j = 0; j < wmin; j++) {
                if (GET_DATA_BIT(linem, j))
                   line[j] = lines[j];
            }
        }
    }

    return 0;
}


/*!
 * \brief   pixCombineMaskedGeneral()
 *
 * \param[in]   pixd   1 bpp, 8 bpp gray or 32 bpp rgb
 * \param[in]   pixs   1 bpp, 8 bpp gray or 32 bpp rgb
 * \param[in]   pixm   [optional] 1 bpp mask
 * \param[in]   x, y   origin of pixs and pixm relative to pixd; can be negative
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) In-place operation; pixd is changed.
 *      (2) This is a generalized version of pixCombinedMasked(), where
 *          the source and mask can be placed at the same (arbitrary)
 *          location relative to pixd.
 *      (3) pixs and pixd must be the same depth and not colormapped.
 *      (4) The UL corners of both pixs and pixm are aligned with
 *          the point (x, y) of pixd, and the operation is clipped to
 *          the intersection of all three images.
 *      (5) If pixm == NULL, it's a no-op.
 *      (6) Implementation.  There are two ways to do these.  In the first,
 *          we use rasterop, ORing the part of pixs under the mask
 *          with pixd (which has been appropriately cleared there first).
 *          In the second, the mask is used one pixel at a time to
 *          selectively replace pixels of pixd with those of pixs.
 *          Here, we use rasterop for 1 bpp and pixel-wise replacement
 *          for 8 and 32 bpp.  To use rasterop for 8 bpp, for example,
 *          we must first generate an 8 bpp version of the mask.
 *          The code is simple:
 *
 *             Pix *pixm8 = pixConvert1To8(NULL, pixm, 0, 255);
 *             Pix *pixt = pixAnd(NULL, pixs, pixm8);
 *             pixRasterop(pixd, x, y, wmin, hmin, PIX_DST & PIX_NOT(PIX_SRC),
 *                         pixm8, 0, 0);
 *             pixRasterop(pixd, x, y, wmin, hmin, PIX_SRC | PIX_DST,
 *                         pixt, 0, 0);
 *             pixDestroy(&pixt);
 *             pixDestroy(&pixm8);
 * </pre>
 */
l_ok
pixCombineMaskedGeneral(PIX      *pixd,
                        PIX      *pixs,
                        PIX      *pixm,
                        l_int32   x,
                        l_int32   y)
{
l_int32    d, w, h, ws, hs, ds, wm, hm, dm, wmin, hmin;
l_int32    wpl, wpls, wplm, i, j, val;
l_uint32  *data, *datas, *datam, *line, *lines, *linem;
PIX       *pixt;

    if (!pixm)  /* nothing to do */
        return 0;
    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixd, &w, &h, &d);
    pixGetDimensions(pixs, &ws, &hs, &ds);
    pixGetDimensions(pixm, &wm, &hm, &dm);
    if (d != ds)
        return ERROR_INT("pixs and pixd depths differ", __func__, 1);
    if (dm != 1)
        return ERROR_INT("pixm not 1 bpp", __func__, 1);
    if (d != 1 && d != 8 && d != 32)
        return ERROR_INT("pixd not 1, 8 or 32 bpp", __func__, 1);
    if (pixGetColormap(pixd) || pixGetColormap(pixs))
        return ERROR_INT("pixs and/or pixd is cmapped", __func__, 1);

        /* For d = 1, use rasterop.  pixt is the part from pixs, under
         * the fg of pixm, that is to be combined with pixd.  We also
         * use pixt to remove all fg of pixd that is under the fg of pixm.
         * Then pixt and pixd are combined by ORing. */
    wmin = L_MIN(ws, wm);
    hmin = L_MIN(hs, hm);
    if (d == 1) {
        pixt = pixAnd(NULL, pixs, pixm);
        pixRasterop(pixd, x, y, wmin, hmin, PIX_DST & PIX_NOT(PIX_SRC),
                    pixm, 0, 0);
        pixRasterop(pixd, x, y, wmin, hmin, PIX_SRC | PIX_DST, pixt, 0, 0);
        pixDestroy(&pixt);
        return 0;
    }

    wpl = pixGetWpl(pixd);
    data = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    datas = pixGetData(pixs);
    wplm = pixGetWpl(pixm);
    datam = pixGetData(pixm);

    for (i = 0; i < hmin; i++) {
        if (y + i < 0 || y + i >= h) continue;
        line = data + (y + i) * wpl;
        lines = datas + i * wpls;
        linem = datam + i * wplm;
        for (j = 0; j < wmin; j++) {
            if (x + j < 0 || x + j >= w) continue;
            if (GET_DATA_BIT(linem, j)) {
                switch (d)
                {
                case 8:
                    val = GET_DATA_BYTE(lines, j);
                    SET_DATA_BYTE(line, x + j, val);
                    break;
                case 32:
                    *(line + x + j) = *(lines + j);
                    break;
                default:
                    return ERROR_INT("shouldn't get here", __func__, 1);
                }
            }
        }
    }

    return 0;
}


/*!
 * \brief   pixPaintThroughMask()
 *
 * \param[in]   pixd   1, 2, 4, 8, 16 or 32 bpp; or colormapped
 * \param[in]   pixm   [optional] 1 bpp mask
 * \param[in]   x, y   origin of pixm relative to pixd; can be negative
 * \param[in]   val    pixel value to set at each masked pixel
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) In-place operation.  Calls pixSetMaskedCmap() for colormapped
 *          images.
 *      (2) For 1, 2, 4, 8 and 16 bpp gray, we take the appropriate
 *          number of least significant bits of val.
 *      (3) If pixm == NULL, it's a no-op.
 *      (4) The mask origin is placed at (x,y) on pixd, and the
 *          operation is clipped to the intersection of rectangles.
 *      (5) For rgb, the components in val are in the canonical locations,
 *          with red in location COLOR_RED, etc.
 *      (6) Implementation detail 1:
 *          For painting with val == 0 or val == maxval, you can use rasterop.
 *          If val == 0, invert the mask so that it's 0 over the region
 *          into which you want to write, and use PIX_SRC & PIX_DST to
 *          clear those pixels.  To write with val = maxval (all 1's),
 *          use PIX_SRC | PIX_DST to set all bits under the mask.
 *      (7) Implementation detail 2:
 *          The rasterop trick can be used for depth > 1 as well.
 *          For val == 0, generate the mask for depth d from the binary
 *          mask using
 *              pixmd = pixUnpackBinary(pixm, d, 1);
 *          and use pixRasterop() with PIX_MASK.  For val == maxval,
 *              pixmd = pixUnpackBinary(pixm, d, 0);
 *          and use pixRasterop() with PIX_PAINT.
 *          But note that if d == 32 bpp, it is about 3x faster to use
 *          the general implementation (not pixRasterop()).
 *      (8) Implementation detail 3:
 *          It might be expected that the switch in the inner loop will
 *          cause large branching delays and should be avoided.
 *          This is not the case, because the entrance is always the
 *          same and the compiler can correctly predict the jump.
 * </pre>
 */
l_ok
pixPaintThroughMask(PIX      *pixd,
                    PIX      *pixm,
                    l_int32   x,
                    l_int32   y,
                    l_uint32  val)
{
l_int32    d, w, h, wm, hm, wpl, wplm, i, j, rval, gval, bval;
l_uint32  *data, *datam, *line, *linem;

    if (!pixm)  /* nothing to do */
        return 0;
    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (pixGetColormap(pixd)) {
        extractRGBValues(val, &rval, &gval, &bval);
        return pixSetMaskedCmap(pixd, pixm, x, y, rval, gval, bval);
    }

    if (pixGetDepth(pixm) != 1)
        return ERROR_INT("pixm not 1 bpp", __func__, 1);
    d = pixGetDepth(pixd);
    if (d == 1)
        val &= 1;
    else if (d == 2)
        val &= 3;
    else if (d == 4)
        val &= 0x0f;
    else if (d == 8)
        val &= 0xff;
    else if (d == 16)
        val &= 0xffff;
    else if (d != 32)
        return ERROR_INT("pixd not 1, 2, 4, 8, 16 or 32 bpp", __func__, 1);
    pixGetDimensions(pixm, &wm, &hm, NULL);

        /* If d == 1, use rasterop; it's about 25x faster. */
    if (d == 1) {
        if (val == 0) {
            PIX *pixmi = pixInvert(NULL, pixm);
            pixRasterop(pixd, x, y, wm, hm, PIX_MASK, pixmi, 0, 0);
            pixDestroy(&pixmi);
        } else {  /* val == 1 */
            pixRasterop(pixd, x, y, wm, hm, PIX_PAINT, pixm, 0, 0);
        }
        return 0;
    }

        /* For d < 32, use rasterop if val == 0 (black); ~3x faster. */
    if (d < 32 && val == 0) {
        PIX *pixmd = pixUnpackBinary(pixm, d, 1);
        pixRasterop(pixd, x, y, wm, hm, PIX_MASK, pixmd, 0, 0);
        pixDestroy(&pixmd);
        return 0;
    }

        /* For d < 32, use rasterop if val == maxval (white); ~3x faster. */
    if (d < 32 && val == ((1 << d) - 1)) {
        PIX *pixmd = pixUnpackBinary(pixm, d, 0);
        pixRasterop(pixd, x, y, wm, hm, PIX_PAINT, pixmd, 0, 0);
        pixDestroy(&pixmd);
        return 0;
    }

        /* All other cases */
    pixGetDimensions(pixd, &w, &h, NULL);
    wpl = pixGetWpl(pixd);
    data = pixGetData(pixd);
    wplm = pixGetWpl(pixm);
    datam = pixGetData(pixm);
    for (i = 0; i < hm; i++) {
        if (y + i < 0 || y + i >= h) continue;
        line = data + (y + i) * wpl;
        linem = datam + i * wplm;
        for (j = 0; j < wm; j++) {
            if (x + j < 0 || x + j >= w) continue;
            if (GET_DATA_BIT(linem, j)) {
                switch (d)
                {
                case 2:
                    SET_DATA_DIBIT(line, x + j, val);
                    break;
                case 4:
                    SET_DATA_QBIT(line, x + j, val);
                    break;
                case 8:
                    SET_DATA_BYTE(line, x + j, val);
                    break;
                case 16:
                    SET_DATA_TWO_BYTES(line, x + j, val);
                    break;
                case 32:
                    *(line + x + j) = val;
                    break;
                default:
                    return ERROR_INT("shouldn't get here", __func__, 1);
                }
            }
        }
    }

    return 0;
}


/*!
 * \brief   pixCopyWithBoxa()
 *
 * \param[in]   pixs         all depths; cmap ok
 * \param[in]   boxa         e.g., from components of a photomask
 * \param[in]   background   L_SET_WHITE or L_SET_BLACK
 * \return  pixd or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Pixels from pixs are copied ("blitted") through each box into pixd.
 *      (2) Pixels not copied are preset to either white or black.
 *      (3) This fast and simple implementation can use rasterop because
 *          each region to be copied is rectangular.
 *      (4) A much slower implementation that doesn't use rasterop would make
 *          a 1 bpp mask from the boxa and then copy, pixel by pixel,
 *          through the mask:
 *             pixGetDimensions(pixs, &w, &h, NULL);
 *             pixm = pixCreate(w, h, 1);
 *             pixm = pixMaskBoxa(pixm, pixm, boxa);
 *             pixd = pixCreateTemplate(pixs);
 *             pixSetBlackOrWhite(pixd, background);
 *             pixCombineMasked(pixd, pixs, pixm);
 *             pixDestroy(&pixm);
 * </pre>
 */
PIX *
pixCopyWithBoxa(PIX     *pixs,
                BOXA    *boxa,
                l_int32  background)
{
l_int32  i, n, x, y, w, h;
PIX     *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (!boxa)
        return (PIX *)ERROR_PTR("boxa not defined", __func__, NULL);
    if (background != L_SET_WHITE && background != L_SET_BLACK)
        return (PIX *)ERROR_PTR("invalid background", __func__, NULL);

    pixd = pixCreateTemplate(pixs);
    pixSetBlackOrWhite(pixd, background);
    n = boxaGetCount(boxa);
    for (i = 0; i < n; i++) {
        boxaGetBoxGeometry(boxa, i, &x, &y, &w, &h);
        pixRasterop(pixd, x, y, w, h, PIX_SRC, pixs, x, y);
    }
    return pixd;
}


/*!
 * \brief   pixPaintSelfThroughMask()
 *
 * \param[in]   pixd       8 bpp gray or 32 bpp rgb; not colormapped
 * \param[in]   pixm       1 bpp mask
 * \param[in]   x, y       origin of pixm relative to pixd; must not be negative
 * \param[in]   searchdir  L_HORIZ, L_VERT or L_BOTH_DIRECTIONS
 * \param[in]   mindist    min distance of nearest tile edge to box; >= 0
 * \param[in]   tilesize   requested size for tiling; may be reduced
 * \param[in]   ntiles     number of tiles tested in each row/column
 * \param[in]   distblend  distance outside the fg used for blending with pixs
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) In-place operation; pixd is changed.
 *      (2) If pixm == NULL, it's a no-op.
 *      (3) The mask origin is placed at (x,y) on pixd, and the
 *          operation is clipped to the intersection of pixd and the
 *          fg of the mask.
 *      (4) %tsize is the the requested size for tiling.  The actual
 *          actual size for each c.c. will be bounded by the minimum
 *          dimension of the c.c.
 *      (5) For %mindist, %searchdir and %ntiles, see pixFindRepCloseTile().
 *          They determine the set of possible tiles that can be used
 *          to build a larger mirrored tile to paint onto pixd through
 *          the c.c. of pixm.
 *      (6) %distblend is used for alpha blending.  It is only applied
 *          if there is exactly one c.c. in the mask.  Use distblend == 0
 *          to skip blending and just paint through the 1 bpp mask.
 *      (7) To apply blending to more than 1 component, call this function
 *          repeatedly with %pixm, %x and %y representing one component of
 *          the mask each time.  This would be done as follows, for an
 *          underlying image pixs and mask pixm of components to fill:
 *              Boxa *boxa = pixConnComp(pixm, &pixa, 8);
 *              n = boxaGetCount(boxa);
 *              for (i = 0; i < n; i++) {
 *                  Pix *pix = pixaGetPix(pixa, i, L_CLONE);
 *                  Box *box = pixaGetBox(pixa, i, L_CLONE);
 *                  boxGetGeometry(box, &bx, &by, &bw, &bh);
 *                  pixPaintSelfThroughMask(pixs, pix, bx, by, searchdir,
 *                                     mindist, tilesize, ntiles, distblend);
 *                  pixDestroy(&pix);
 *                  boxDestroy(&box);
 *              }
 *              pixaDestroy(&pixa);
 *              boxaDestroy(&boxa);
 *      (8) If no tiles can be found, this falls back to estimating the
 *          color near the boundary of the region to be textured.
 *      (9) This can be used to replace the pixels in some regions of
 *          an image by selected neighboring pixels.  The mask represents
 *          the pixels to be replaced.  For each connected component in
 *          the mask, this function selects up to two tiles of neighboring
 *          pixels to be used for replacement of pixels represented by
 *          the component (i.e., under the FG of that component in the mask).
 *          After selection, mirror replication is used to generate an
 *          image that is large enough to cover the component.  Alpha
 *          blending can also be used outside of the component, but near the
 *          edge, to blur the transition between painted and original pixels.
 * </pre>
 */
l_ok
pixPaintSelfThroughMask(PIX      *pixd,
                        PIX      *pixm,
                        l_int32   x,
                        l_int32   y,
                        l_int32   searchdir,
                        l_int32   mindist,
                        l_int32   tilesize,
                        l_int32   ntiles,
                        l_int32   distblend)
{
l_int32   w, h, d, wm, hm, dm, i, n, bx, by, bw, bh, edgeblend, retval, minside;
l_uint32  pixval;
BOX      *box, *boxv, *boxh;
BOXA     *boxa;
PIX      *pixf, *pixv, *pixh, *pix1, *pix2, *pix3, *pix4, *pix5;
PIXA     *pixa;

    if (!pixm)  /* nothing to do */
        return 0;
    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (pixGetColormap(pixd) != NULL)
        return ERROR_INT("pixd has colormap", __func__, 1);
    pixGetDimensions(pixd, &w, &h, &d);
    if (d != 8 && d != 32)
        return ERROR_INT("pixd not 8 or 32 bpp", __func__, 1);
    pixGetDimensions(pixm, &wm, &hm, &dm);
    if (dm != 1)
        return ERROR_INT("pixm not 1 bpp", __func__, 1);
    if (x < 0 || y < 0)
        return ERROR_INT("x and y must be non-negative", __func__, 1);
    if (searchdir != L_HORIZ && searchdir != L_VERT &&
        searchdir != L_BOTH_DIRECTIONS)
        return ERROR_INT("invalid searchdir", __func__, 1);
    if (tilesize < 2)
        return ERROR_INT("tilesize must be >= 2", __func__, 1);
    if (distblend < 0)
        return ERROR_INT("distblend must be >= 0", __func__, 1);

        /* Embed mask in full sized mask */
    if (wm < w || hm < h) {
        pixf = pixCreate(w, h, 1);
        pixRasterop(pixf, x, y, wm, hm, PIX_SRC, pixm, 0, 0);
    } else {
        pixf = pixCopy(NULL, pixm);
    }

        /* Get connected components of mask */
    boxa = pixConnComp(pixf, &pixa, 8);
    if ((n = pixaGetCount(pixa)) == 0) {
        L_WARNING("no fg in mask\n", __func__);
        pixDestroy(&pixf);
        pixaDestroy(&pixa);
        boxaDestroy(&boxa);
        return 1;
    }
    boxaDestroy(&boxa);

        /* For each c.c., generate one or two representative tiles for
         * texturizing and apply through the mask.  The input 'tilesize'
         * is the requested value.  Note that if there is exactly one
         * component, and blending at the edge is requested, an alpha mask
         * is generated, which is larger than the bounding box of the c.c. */
    edgeblend = (n == 1 && distblend > 0) ? 1 : 0;
    if (distblend > 0 && n > 1)
        L_WARNING("%d components; can not blend at edges\n", __func__, n);
    retval = 0;
    for (i = 0; i < n; i++) {
        if (edgeblend) {
            pix1 = pixMakeAlphaFromMask(pixf, distblend, &box);
        } else {
            pix1 = pixaGetPix(pixa, i, L_CLONE);
            box = pixaGetBox(pixa, i, L_CLONE);
        }
        boxGetGeometry(box, &bx, &by, &bw, &bh);
        minside = L_MIN(bw, bh);

        boxh = boxv = NULL;
        if (searchdir == L_HORIZ || searchdir == L_BOTH_DIRECTIONS) {
            pixFindRepCloseTile(pixd, box, L_HORIZ, mindist,
                                L_MIN(minside, tilesize), ntiles, &boxh, 0);
        }
        if (searchdir == L_VERT || searchdir == L_BOTH_DIRECTIONS) {
            pixFindRepCloseTile(pixd, box, L_VERT, mindist,
                                L_MIN(minside, tilesize), ntiles, &boxv, 0);
        }
        if (!boxh && !boxv) {
            L_WARNING("tile region not selected; paint color near boundary\n",
                      __func__);
            pixDestroy(&pix1);
            pix1 = pixaGetPix(pixa, i, L_CLONE);
            pixaGetBoxGeometry(pixa, i, &bx, &by, NULL, NULL);
            retval = pixGetColorNearMaskBoundary(pixd, pixm, box, distblend,
                                                 &pixval, 0);
            pixSetMaskedGeneral(pixd, pix1, pixval, bx, by);
            pixDestroy(&pix1);
            boxDestroy(&box);
            continue;
        }

            /* Extract the selected squares from pixd */
        pixh = (boxh) ? pixClipRectangle(pixd, boxh, NULL) : NULL;
        pixv = (boxv) ? pixClipRectangle(pixd, boxv, NULL) : NULL;
        if (pixh && pixv)
            pix2 = pixBlend(pixh, pixv, 0, 0, 0.5);
        else if (pixh)
            pix2 = pixClone(pixh);
        else  /* pixv */
            pix2 = pixClone(pixv);
        pixDestroy(&pixh);
        pixDestroy(&pixv);
        boxDestroy(&boxh);
        boxDestroy(&boxv);

            /* Generate an image the size of the b.b. of the c.c.,
             * possibly extended by the blending distance, which
             * is then either painted through the c.c. mask or
             * blended using the alpha mask for that c.c.  */
        pix3 = pixMirroredTiling(pix2, bw, bh);
        if (edgeblend) {
            pix4 = pixClipRectangle(pixd, box, NULL);
            pix5 = pixBlendWithGrayMask(pix4, pix3, pix1, 0, 0);
            pixRasterop(pixd, bx, by, bw, bh, PIX_SRC, pix5, 0, 0);
            pixDestroy(&pix4);
            pixDestroy(&pix5);
        } else {
            pixCombineMaskedGeneral(pixd, pix3, pix1, bx, by);
        }
        pixDestroy(&pix1);
        pixDestroy(&pix2);
        pixDestroy(&pix3);
        boxDestroy(&box);
    }

    pixaDestroy(&pixa);
    pixDestroy(&pixf);
    return retval;
}


/*!
 * \brief   pixMakeMaskFromVal()
 *
 * \param[in]   pixs   2, 4 or 8 bpp; can be colormapped
 * \param[in]   val    pixel value
 * \return  pixd 1 bpp mask, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a 1 bpp mask image, where a 1 is written in
 *          the mask for each pixel in pixs that has a value %val.
 *      (2) If no pixels have the value, an empty mask is generated.
 * </pre>
 */
PIX *
pixMakeMaskFromVal(PIX     *pixs,
                   l_int32  val)
{
l_int32    w, h, d, i, j, sval, wpls, wpld;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 2 && d != 4 && d != 8)
        return (PIX *)ERROR_PTR("pix not 2, 4 or 8 bpp", __func__, NULL);

    pixd = pixCreate(w, h, 1);
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
            if (d == 2)
                sval = GET_DATA_DIBIT(lines, j);
            else if (d == 4)
                sval = GET_DATA_QBIT(lines, j);
            else  /* d == 8 */
                sval = GET_DATA_BYTE(lines, j);
            if (sval == val)
                SET_DATA_BIT(lined, j);
        }
    }

    return pixd;
}


/*!
 * \brief   pixMakeMaskFromLUT()
 *
 * \param[in]   pixs   2, 4 or 8 bpp; can be colormapped
 * \param[in]   tab    256-entry LUT; 1 means to write to mask
 * \return  pixd 1 bpp mask, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a 1 bpp mask image, where a 1 is written in
 *          the mask for each pixel in pixs that has a value corresponding
 *          to a 1 in the LUT.
 *      (2) The LUT should be of size 256.
 * </pre>
 */
PIX *
pixMakeMaskFromLUT(PIX      *pixs,
                   l_int32  *tab)
{
l_int32    w, h, d, i, j, val, wpls, wpld;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (!tab)
        return (PIX *)ERROR_PTR("tab not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 2 && d != 4 && d != 8)
        return (PIX *)ERROR_PTR("pix not 2, 4 or 8 bpp", __func__, NULL);

    pixd = pixCreate(w, h, 1);
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
            if (d == 2)
                val = GET_DATA_DIBIT(lines, j);
            else if (d == 4)
                val = GET_DATA_QBIT(lines, j);
            else  /* d == 8 */
                val = GET_DATA_BYTE(lines, j);
            if (tab[val] == 1)
                SET_DATA_BIT(lined, j);
        }
    }

    return pixd;
}


/*!
 * \brief   pixMakeArbMaskFromRGB()
 *
 * \param[in]   pixs         32 bpp RGB
 * \param[in]   rc, gc, bc   arithmetic factors; can be negative
 * \param[in]   thresh       lower threshold on weighted sum of components
 * \return  pixd 1 bpp mask, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a 1 bpp mask image, where a 1 is written in
 *          the mask for each pixel in pixs that satisfies
 *               rc * rval + gc * gval + bc * bval > thresh
 *          where rval is the red component, etc.
 *      (2) Unlike with pixConvertToGray(), there are no constraints
 *          on the color coefficients, which can be negative.  For
 *          example, a mask that discriminates against red and in favor
 *          of blue will have rc < 0.0 and bc > 0.0.
 *      (3) To make the result independent of intensity (the 'V' in HSV),
 *          select coefficients so that %thresh = 0.  Then the result
 *          is not changed when all components are multiplied by the
 *          same constant (as long as nothing saturates).  This can be
 *          useful if, for example, the illumination is not uniform.
 * </pre>
 */
PIX *
pixMakeArbMaskFromRGB(PIX       *pixs,
                      l_float32  rc,
                      l_float32  gc,
                      l_float32  bc,
                      l_float32  thresh)
{
PIX  *pix1, *pix2;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);
    if (thresh >= 255.0) thresh = 254.0;  /* avoid 8 bit overflow */

    if ((pix1 = pixConvertRGBToGrayArb(pixs, rc, gc, bc)) == NULL)
        return (PIX *)ERROR_PTR("pix1 not made", __func__, NULL);
    pix2 = pixThresholdToBinary(pix1, thresh + 1);
    pixInvert(pix2, pix2);
    pixDestroy(&pix1);
    return pix2;
}


/*!
 * \brief   pixSetUnderTransparency()
 *
 * \param[in]   pixs    32 bpp rgba
 * \param[in]   val     32 bit unsigned color to use where alpha == 0
 * \param[in]   debug   displays layers of pixs
 * \return  pixd 32 bpp rgba, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This sets the r, g and b components under every fully
 *          transparent alpha component to %val.  The alpha components
 *          are unchanged.
 *      (2) Full transparency is denoted by alpha == 0.  Setting
 *          all pixels to a constant %val where alpha is transparent
 *          can improve compressibility by reducing the entropy.
 *      (3) The visual result depends on how the image is displayed.
 *          (a) For display devices that respect the use of the alpha
 *              layer, this will not affect the appearance.
 *          (b) For typical leptonica operations, alpha is ignored,
 *              so there will be a change in appearance because this
 *              resets the rgb values in the fully transparent region.
 *      (4) pixRead() and pixWrite() will, by default, read and write
 *          4-component (rgba) pix in png format.  To ignore the alpha
 *          component after reading, or omit it on writing, pixSetSpp(..., 3).
 *      (5) Here are some examples:
 *          * To convert all fully transparent pixels in a 4 component
 *            (rgba) png file to white:
 *              pixs = pixRead(<infile>);
 *              pixd = pixSetUnderTransparency(pixs, 0xffffff00, 0);
 *          * To write pixd with the alpha component:
 *              pixWrite(<outfile>, pixd, IFF_PNG);
 *          * To write and rgba image without the alpha component, first do:
 *              pixSetSpp(pixd, 3);
 *            If you later want to use the alpha, spp must be reset to 4.
 *          * (fancier) To remove the alpha by blending the image over
 *            a white background:
 *              pixRemoveAlpha()
 *            This changes all pixel values where the alpha component is
 *            not opaque (255).
 *      (6) Caution.  rgb images in leptonica typically have value 0 in
 *          the alpha channel, which is fully transparent.  If spp for
 *          such an image were changed from 3 to 4, the image becomes
 *          fully transparent, and this function will set each pixel to %val.
 *          If you really want to set every pixel to the same value,
 *          use pixSetAllArbitrary().
 *      (7) This is useful for compressing an RGBA image where the part
 *          of the image that is fully transparent is random junk; compression
 *          is typically improved by setting that region to a constant.
 *          For rendering as a 3 component RGB image over a uniform
 *          background of arbitrary color, use pixAlphaBlendUniform().
 * </pre>
 */
PIX *
pixSetUnderTransparency(PIX      *pixs,
                        l_uint32  val,
                        l_int32   debug)
{
PIX  *pixg, *pixm, *pixt, *pixd;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not defined or not 32 bpp",
                                __func__, NULL);

    if (pixGetSpp(pixs) != 4) {
        L_WARNING("no alpha channel; returning a copy\n", __func__);
        return pixCopy(NULL, pixs);
    }

        /* Make a mask from the alpha component with ON pixels
         * wherever the alpha component is fully transparent (0).
         * The hard way:
         *     l_int32 *lut = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
         *     lut[0] = 1;
         *     pixg = pixGetRGBComponent(pixs, L_ALPHA_CHANNEL);
         *     pixm = pixMakeMaskFromLUT(pixg, lut);
         *     LEPT_FREE(lut);
         * But there's an easier way to set pixels in a mask where
         * the alpha component is 0 ...  */
    pixg = pixGetRGBComponent(pixs, L_ALPHA_CHANNEL);
    pixm = pixThresholdToBinary(pixg, 1);

    if (debug) {
        pixt = pixDisplayLayersRGBA(pixs, 0xffffff00, 600);
        pixDisplay(pixt, 0, 0);
        pixDestroy(&pixt);
    }

    pixd = pixCopy(NULL, pixs);
    pixSetMasked(pixd, pixm, (val & 0xffffff00));
    pixDestroy(&pixg);
    pixDestroy(&pixm);
    return pixd;
}


/*!
 * \brief   pixMakeAlphaFromMask()
 *
 * \param[in]    pixs   1 bpp
 * \param[in]    dist   blending distance; typically 10 - 30
 * \param[out]   pbox   [optional] use NULL to get the full size
 * \return  pixd (8 bpp gray, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a 8 bpp alpha layer that is opaque (256)
 *          over the FG of pixs, and goes transparent linearly away
 *          from the FG pixels, decaying to 0 (transparent) is an
 *          8-connected distance given by %dist.  If %dist == 0,
 *          this does a simple conversion from 1 to 8 bpp.
 *      (2) If &box == NULL, this returns an alpha mask that is the
 *          full size of pixs.  Otherwise, the returned mask pixd covers
 *          just the FG pixels of pixs, expanded by %dist in each
 *          direction (if possible), and the returned box gives the
 *          location of the returned mask relative to pixs.
 *      (3) This is useful for painting through a mask and allowing
 *          blending of the painted image with an underlying image
 *          in the mask background for pixels near foreground mask pixels.
 *          For example, with an underlying rgb image pix1, an overlaying
 *          image rgb pix2, binary mask pixm, and dist > 0, this
 *          blending is achieved with:
 *              pix3 = pixMakeAlphaFromMask(pixm, dist, &box);
 *              boxGetGeometry(box, &x, &y, NULL, NULL);
 *              pix4 = pixBlendWithGrayMask(pix1, pix2, pix3, x, y);
 * </pre>
 */
PIX *
pixMakeAlphaFromMask(PIX     *pixs,
                     l_int32  dist,
                     BOX    **pbox)
{
l_int32  w, h;
BOX     *box1, *box2;
PIX     *pix1, *pixd;

    if (pbox) *pbox = NULL;
    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (dist < 0)
        return (PIX *)ERROR_PTR("dist must be >= 0", __func__, NULL);

        /* If requested, extract just the region to be affected by the mask */
    if (pbox) {
        pixClipToForeground(pixs, NULL, &box1);
        if (!box1) {
            L_WARNING("no ON pixels in mask\n", __func__);
            return pixCreateTemplate(pixs);  /* all background (0) */
        }

        boxAdjustSides(box1, box1, -dist, dist, -dist, dist);
        pixGetDimensions(pixs, &w, &h, NULL);
        box2 = boxClipToRectangle(box1, w, h);
        *pbox = box2;
        pix1 = pixClipRectangle(pixs, box2, NULL);
        boxDestroy(&box1);
    } else {
        pix1 = pixCopy(NULL, pixs);
    }

    if (dist == 0) {
        pixd = pixConvert1To8(NULL, pix1, 0, 255);
        pixDestroy(&pix1);
        return pixd;
    }

        /* Blur the boundary of the input mask */
    pixInvert(pix1, pix1);
    pixd = pixDistanceFunction(pix1, 8, 8, L_BOUNDARY_FG);
    pixMultConstantGray(pixd, 256.0f / dist);
    pixInvert(pixd, pixd);
    pixDestroy(&pix1);
    return pixd;
}


/*!
 * \brief   pixGetColorNearMaskBoundary()
 *
 * \param[in]    pixs    32 bpp rgb
 * \param[in]    pixm    1 bpp mask, full image
 * \param[in]    box     region of mask; typically b.b. of a component
 * \param[in]    dist    distance into BG from mask boundary to use
 * \param[out]   pval    average pixel value
 * \param[in]    debug   1 to output mask images
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) This finds the average color in a set of pixels that are
 *          roughly a distance %dist from the c.c. boundary and in the
 *          background of the mask image.
 * </pre>
 */
l_ok
pixGetColorNearMaskBoundary(PIX       *pixs,
                            PIX       *pixm,
                            BOX       *box,
                            l_int32    dist,
                            l_uint32  *pval,
                            l_int32    debug)
{
char       op[64];
l_int32    empty, bx, by;
l_float32  rval, gval, bval;
BOX       *box1, *box2;
PIX       *pix1, *pix2, *pix3;

    if (!pval)
        return ERROR_INT("&pval not defined", __func__, 1);
    *pval = 0xffffff00;  /* white */
    if (!pixs || pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs undefined or not 32 bpp", __func__, 1);
    if (!pixm || pixGetDepth(pixm) != 1)
        return ERROR_INT("pixm undefined or not 1 bpp", __func__, 1);
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);
    if (dist < 0)
        return ERROR_INT("dist must be >= 0", __func__, 1);

        /* Clip mask piece, expanded beyond %box by (%dist + 5) on each side.
         * box1 is the region requested; box2 is the actual region retrieved,
         * which is clipped to %pixm */
    box1 = boxAdjustSides(NULL, box, -dist - 5, dist + 5, -dist - 5, dist + 5);
    pix1 = pixClipRectangle(pixm, box1, &box2);

        /* Expand FG by %dist into the BG */
    if (dist == 0) {
        pix2 = pixCopy(NULL, pix1);
    } else {
        snprintf(op, sizeof(op), "d%d.%d", 2 * dist, 2 * dist);
        pix2 = pixMorphSequence(pix1, op, 0);
    }

        /* Expand again by 5 pixels on all sides (dilate 11x11) and XOR,
         * getting the annulus of FG pixels between %dist and %dist + 5 */
    pix3 = pixCopy(NULL, pix2);
    pixDilateBrick(pix3, pix3, 11, 11);
    pixXor(pix3, pix3, pix2);
    pixZero(pix3, &empty);
    if (!empty) {
            /* Scan the same region in %pixs, to get average under FG in pix3 */
        boxGetGeometry(box2, &bx, &by, NULL, NULL);
        pixGetAverageMaskedRGB(pixs, pix3, bx, by, 1, L_MEAN_ABSVAL,
                               &rval, &gval, &bval);
        composeRGBPixel((l_int32)(rval + 0.5), (l_int32)(gval + 0.5),
                        (l_int32)(bval + 0.5), pval);
    } else {
        L_WARNING("no pixels found\n", __func__);
    }

    if (debug) {
        lept_rmdir("masknear");  /* erase previous images */
        lept_mkdir("masknear");
        pixWriteDebug("/tmp/masknear/input.png", pix1, IFF_PNG);
        pixWriteDebug("/tmp/masknear/adjusted.png", pix2, IFF_PNG);
        pixWriteDebug("/tmp/masknear/outerfive.png", pix3, IFF_PNG);
        lept_stderr("Input box; with adjusted sides; clipped\n");
        boxPrintStreamInfo(stderr, box);
        boxPrintStreamInfo(stderr, box1);
        boxPrintStreamInfo(stderr, box2);
    }

    pixDestroy(&pix1);
    pixDestroy(&pix2);
    pixDestroy(&pix3);
    boxDestroy(&box1);
    boxDestroy(&box2);
    return 0;
}


/*!
 * \brief   pixDisplaySelectedPixels()
 *
 * \param[in]    pixs    [optional] any depth
 * \param[in]    pixm    1 bpp mask, aligned UL corner with %pixs
 * \param[in]    sel     [optional] pattern to paint at each pixel in pixm
 * \param[in]    val     rgb rendering of pattern
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For every fg pixel in %pixm, this paints the pattern in %sel
 *          in color %val on a copy of %pixs.
 *      (2) The implementation is to dilate %pixm by %sel, and then
 *          paint through the dilated mask onto %pixs.
 *      (3) If %pixs == NULL, it paints on a white image.
 *      (4) If %sel == NULL, it paints only the pixels in the input %pixm.
 *      (5) This visualization would typically be used in debugging.
 * </pre>
 */
PIX *
pixDisplaySelectedPixels(PIX      *pixs,
                         PIX      *pixm,
                         SEL      *sel,
                         l_uint32  val)
{
l_int32  w, h;
PIX     *pix1, *pix2;

    if (!pixm || pixGetDepth(pixm) != 1)
        return (PIX *)ERROR_PTR("pixm undefined or not 1 bpp", __func__, NULL);

    if (pixs) {
        pix1 = pixConvertTo32(pixs);
    } else {
        pixGetDimensions(pixm, &w, &h, NULL);
        pix1 = pixCreate(w, h, 32);
        pixSetAll(pix1);
    }

    if (sel)
       pix2 = pixDilate(NULL, pixm, sel);
    else
       pix2 = pixClone(pixm);
    pixSetMasked(pix1, pix2, val);
    pixDestroy(&pix2);
    return pix1;
}


/*-------------------------------------------------------------*
 *    One and two-image boolean ops on arbitrary depth images  *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixInvert()
 *
 * \param[in]   pixd  [optional]; this can be null, equal to pixs,
 *                    or different from pixs
 * \param[in]   pixs
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This inverts pixs, for all pixel depths.
 *      (2) There are 3 cases:
 *           (a) pixd == null,   ~src --> new pixd
 *           (b) pixd == pixs,   ~src --> src  (in-place)
 *           (c) pixd != pixs,   ~src --> input pixd
 *      (3) For clarity, if the case is known, use these patterns:
 *           (a) pixd = pixInvert(NULL, pixs);
 *           (b) pixInvert(pixs, pixs);
 *           (c) pixInvert(pixd, pixs);
 * </pre>
 */
PIX *
pixInvert(PIX  *pixd,
          PIX  *pixs)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

        /* Prepare pixd for in-place operation */
    if ((pixd = pixCopy(pixd, pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);

    pixRasterop(pixd, 0, 0, pixGetWidth(pixd), pixGetHeight(pixd),
                PIX_NOT(PIX_DST), NULL, 0, 0);   /* invert pixd */

    return pixd;
}


/*!
 * \brief   pixOr()
 *
 * \param[in]   pixd    [optional]; this can be null, equal to pixs1,
 *                      different from pixs1
 * \param[in]   pixs1   can be == pixd
 * \param[in]   pixs2   must be != pixd
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *      (1) This gives the union of two images with equal depth,
 *          aligning them to the UL corner.  pixs1 and pixs2
 *          need not have the same width and height.
 *      (2) There are 3 cases:
 *            (a) pixd == null,   (src1 | src2) --> new pixd
 *            (b) pixd == pixs1,  (src1 | src2) --> src1  (in-place)
 *            (c) pixd != pixs1,  (src1 | src2) --> input pixd
 *      (3) For clarity, if the case is known, use these patterns:
 *            (a) pixd = pixOr(NULL, pixs1, pixs2);
 *            (b) pixOr(pixs1, pixs1, pixs2);
 *            (c) pixOr(pixd, pixs1, pixs2);
 *      (4) The size of the result is determined by pixs1.
 *      (5) The depths of pixs1 and pixs2 must be equal.
 *      (6) Note carefully that the order of pixs1 and pixs2 only matters
 *          for the in-place case.  For in-place, you must have
 *          pixd == pixs1.  Setting pixd == pixs2 gives an incorrect
 *          result: the copy puts pixs1 image data in pixs2, and
 *          the rasterop is then between pixs2 and pixs2 (a no-op).
 * </pre>
 */
PIX *
pixOr(PIX  *pixd,
      PIX  *pixs1,
      PIX  *pixs2)
{
    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, pixd);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, pixd);
    if (pixd == pixs2)
        return (PIX *)ERROR_PTR("cannot have pixs2 == pixd", __func__, pixd);
    if (pixGetDepth(pixs1) != pixGetDepth(pixs2))
        return (PIX *)ERROR_PTR("depths of pixs* unequal", __func__, pixd);

#if  EQUAL_SIZE_WARNING
    if (!pixSizesEqual(pixs1, pixs2))
        L_WARNING("pixs1 and pixs2 not equal sizes\n", __func__);
#endif  /* EQUAL_SIZE_WARNING */

        /* Prepare pixd to be a copy of pixs1 */
    if ((pixd = pixCopy(pixd, pixs1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, pixd);

        /* src1 | src2 --> dest */
    pixRasterop(pixd, 0, 0, pixGetWidth(pixd), pixGetHeight(pixd),
                PIX_SRC | PIX_DST, pixs2, 0, 0);

    return pixd;
}


/*!
 * \brief   pixAnd()
 *
 * \param[in]   pixd    [optional]; this can be null, equal to pixs1,
 *                      different from pixs1
 * \param[in]   pixs1   can be == pixd
 * \param[in]   pixs2   must be != pixd
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *      (1) This gives the intersection of two images with equal depth,
 *          aligning them to the the UL corner.  pixs1 and pixs2
 *          need not have the same width and height.
 *      (2) There are 3 cases:
 *            (a) pixd == null,   (src1 & src2) --> new pixd
 *            (b) pixd == pixs1,  (src1 & src2) --> src1  (in-place)
 *            (c) pixd != pixs1,  (src1 & src2) --> input pixd
 *      (3) For clarity, if the case is known, use these patterns:
 *            (a) pixd = pixAnd(NULL, pixs1, pixs2);
 *            (b) pixAnd(pixs1, pixs1, pixs2);
 *            (c) pixAnd(pixd, pixs1, pixs2);
 *      (4) The size of the result is determined by pixs1.
 *      (5) The depths of pixs1 and pixs2 must be equal.
 *      (6) Note carefully that the order of pixs1 and pixs2 only matters
 *          for the in-place case.  For in-place, you must have
 *          pixd == pixs1.  Setting pixd == pixs2 gives an incorrect
 *          result: the copy puts pixs1 image data in pixs2, and
 *          the rasterop is then between pixs2 and pixs2 (a no-op).
 * </pre>
 */
PIX *
pixAnd(PIX  *pixd,
       PIX  *pixs1,
       PIX  *pixs2)
{
    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, pixd);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, pixd);
    if (pixd == pixs2)
        return (PIX *)ERROR_PTR("cannot have pixs2 == pixd", __func__, pixd);
    if (pixGetDepth(pixs1) != pixGetDepth(pixs2))
        return (PIX *)ERROR_PTR("depths of pixs* unequal", __func__, pixd);

#if  EQUAL_SIZE_WARNING
    if (!pixSizesEqual(pixs1, pixs2))
        L_WARNING("pixs1 and pixs2 not equal sizes\n", __func__);
#endif  /* EQUAL_SIZE_WARNING */

        /* Prepare pixd to be a copy of pixs1 */
    if ((pixd = pixCopy(pixd, pixs1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, pixd);

        /* src1 & src2 --> dest */
    pixRasterop(pixd, 0, 0, pixGetWidth(pixd), pixGetHeight(pixd),
                PIX_SRC & PIX_DST, pixs2, 0, 0);

    return pixd;
}


/*!
 * \brief   pixXor()
 *
 * \param[in]   pixd    [optional]; this can be null, equal to pixs1,
 *                      different from pixs1
 * \param[in]   pixs1   can be == pixd
 * \param[in]   pixs2   must be != pixd
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *      (1) This gives the XOR of two images with equal depth,
 *          aligning them to the the UL corner.  pixs1 and pixs2
 *          need not have the same width and height.
 *      (2) There are 3 cases:
 *            (a) pixd == null,   (src1 ^ src2) --> new pixd
 *            (b) pixd == pixs1,  (src1 ^ src2) --> src1  (in-place)
 *            (c) pixd != pixs1,  (src1 ^ src2) --> input pixd
 *      (3) For clarity, if the case is known, use these patterns:
 *            (a) pixd = pixXor(NULL, pixs1, pixs2);
 *            (b) pixXor(pixs1, pixs1, pixs2);
 *            (c) pixXor(pixd, pixs1, pixs2);
 *      (4) The size of the result is determined by pixs1.
 *      (5) The depths of pixs1 and pixs2 must be equal.
 *      (6) Note carefully that the order of pixs1 and pixs2 only matters
 *          for the in-place case.  For in-place, you must have
 *          pixd == pixs1.  Setting pixd == pixs2 gives an incorrect
 *          result: the copy puts pixs1 image data in pixs2, and
 *          the rasterop is then between pixs2 and pixs2 (a no-op).
 * </pre>
 */
PIX *
pixXor(PIX  *pixd,
       PIX  *pixs1,
       PIX  *pixs2)
{
    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, pixd);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, pixd);
    if (pixd == pixs2)
        return (PIX *)ERROR_PTR("cannot have pixs2 == pixd", __func__, pixd);
    if (pixGetDepth(pixs1) != pixGetDepth(pixs2))
        return (PIX *)ERROR_PTR("depths of pixs* unequal", __func__, pixd);

#if  EQUAL_SIZE_WARNING
    if (!pixSizesEqual(pixs1, pixs2))
        L_WARNING("pixs1 and pixs2 not equal sizes\n", __func__);
#endif  /* EQUAL_SIZE_WARNING */

        /* Prepare pixd to be a copy of pixs1 */
    if ((pixd = pixCopy(pixd, pixs1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, pixd);

        /* src1 ^ src2 --> dest */
    pixRasterop(pixd, 0, 0, pixGetWidth(pixd), pixGetHeight(pixd),
                PIX_SRC ^ PIX_DST, pixs2, 0, 0);

    return pixd;
}


/*!
 * \brief   pixSubtract()
 *
 * \param[in]   pixd    [optional]; this can be null, equal to pixs1,
 *                      equal to pixs2, or different from both pixs1 and pixs2
 * \param[in]   pixs1   can be == pixd
 * \param[in]   pixs2   can be == pixd
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *      (1) This gives the set subtraction of two images with equal depth,
 *          aligning them to the the UL corner.  pixs1 and pixs2
 *          need not have the same width and height.
 *      (2) Source pixs2 is always subtracted from source pixs1.
 *          The result is
 *                  pixs1 \ pixs2 = pixs1 & (~pixs2)
 *      (3) There are 4 cases:
 *            (a) pixd == null,   (src1 - src2) --> new pixd
 *            (b) pixd == pixs1,  (src1 - src2) --> src1  (in-place)
 *            (c) pixd == pixs2,  (src1 - src2) --> src2  (in-place)
 *            (d) pixd != pixs1 && pixd != pixs2),
 *                                 (src1 - src2) --> input pixd
 *      (4) For clarity, if the case is known, use these patterns:
 *            (a) pixd = pixSubtract(NULL, pixs1, pixs2);
 *            (b) pixSubtract(pixs1, pixs1, pixs2);
 *            (c) pixSubtract(pixs2, pixs1, pixs2);
 *            (d) pixSubtract(pixd, pixs1, pixs2);
 *      (5) The size of the result is determined by pixs1.
 *      (6) The depths of pixs1 and pixs2 must be equal.
 * </pre>
 */
PIX *
pixSubtract(PIX  *pixd,
            PIX  *pixs1,
            PIX  *pixs2)
{
l_int32  w, h;

    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, pixd);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, pixd);
    if (pixGetDepth(pixs1) != pixGetDepth(pixs2))
        return (PIX *)ERROR_PTR("depths of pixs* unequal", __func__, pixd);

#if  EQUAL_SIZE_WARNING
    if (!pixSizesEqual(pixs1, pixs2))
        L_WARNING("pixs1 and pixs2 not equal sizes\n", __func__);
#endif  /* EQUAL_SIZE_WARNING */

    pixGetDimensions(pixs1, &w, &h, NULL);
    if (!pixd) {
        pixd = pixCopy(NULL, pixs1);
        pixRasterop(pixd, 0, 0, w, h, PIX_DST & PIX_NOT(PIX_SRC),
            pixs2, 0, 0);   /* src1 & (~src2)  */
    } else if (pixd == pixs1) {
        pixRasterop(pixd, 0, 0, w, h, PIX_DST & PIX_NOT(PIX_SRC),
            pixs2, 0, 0);   /* src1 & (~src2)  */
    } else if (pixd == pixs2) {
        pixRasterop(pixd, 0, 0, w, h, PIX_NOT(PIX_DST) & PIX_SRC,
            pixs1, 0, 0);   /* src1 & (~src2)  */
    } else  { /* pixd != pixs1 && pixd != pixs2 */
        pixCopy(pixd, pixs1);  /* sizes pixd to pixs1 if unequal */
        pixRasterop(pixd, 0, 0, w, h, PIX_DST & PIX_NOT(PIX_SRC),
            pixs2, 0, 0);   /* src1 & (~src2)  */
    }

    return pixd;
}


/*-------------------------------------------------------------*
 *                         Pixel counting                      *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixZero()
 *
 * \param[in]    pix     all depths; colormap OK
 * \param[out]   pempty  1 if all bits in image data field are 0; 0 otherwise
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) For a binary image, if there are no fg (black) pixels, empty = 1.
 *      (2) For a grayscale image, if all pixels are black (0), empty = 1.
 *      (3) For an RGB image, if all 4 components in every pixel is 0
 *          (i.e. opaque black), empty = 1.
 *      (4) For a colormapped image, pixel values are 0.  The colormap
 *          is ignored.
 * </pre>
 */
l_ok
pixZero(PIX      *pix,
        l_int32  *pempty)
{
l_int32    w, h, wpl, i, j, fullwords, endbits;
l_uint32   endmask;
l_uint32  *data, *line;

    if (!pempty)
        return ERROR_INT("&empty not defined", __func__, 1);
    *pempty = 1;
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    w = pixGetWidth(pix) * pixGetDepth(pix);  /* in bits */
    h = pixGetHeight(pix);
    wpl = pixGetWpl(pix);
    data = pixGetData(pix);
    fullwords = w / 32;
    endbits = w & 31;
    endmask = (endbits == 0) ? 0 : (0xffffffffU << (32 - endbits));

    for (i = 0; i < h; i++) {
        line = data + wpl * i;
        for (j = 0; j < fullwords; j++)
            if (*line++) {
                *pempty = 0;
                return 0;
            }
        if (endbits) {
            if (*line & endmask) {
                *pempty = 0;
                return 0;
            }
        }
    }

    return 0;
}


/*!
 * \brief   pixForegroundFraction()
 *
 * \param[in]    pix      1 bpp
 * \param[out]   pfract   fraction of ON pixels
 * \return  0 if OK; 1 on error
 */
l_ok
pixForegroundFraction(PIX        *pix,
                      l_float32  *pfract)
{
l_int32  w, h, count;

    if (!pfract)
        return ERROR_INT("&fract not defined", __func__, 1);
    *pfract = 0.0;
    if (!pix || pixGetDepth(pix) != 1)
        return ERROR_INT("pix not defined or not 1 bpp", __func__, 1);

    pixCountPixels(pix, &count, NULL);
    pixGetDimensions(pix, &w, &h, NULL);
    *pfract = (l_float32)count / (l_float32)(w * h);
    return 0;
}


/*!
 * \brief   pixaCountPixels()
 *
 * \param[in]    pixa    array of 1 bpp pix
 * \return  na of ON pixels in each pix, or NULL on error
 */
NUMA *
pixaCountPixels(PIXA  *pixa)
{
l_int32   d, i, n, count;
l_int32  *tab;
NUMA     *na;
PIX      *pix;

    if (!pixa)
        return (NUMA *)ERROR_PTR("pix not defined", __func__, NULL);

    if ((n = pixaGetCount(pixa)) == 0)
        return numaCreate(1);

    pix = pixaGetPix(pixa, 0, L_CLONE);
    d = pixGetDepth(pix);
    pixDestroy(&pix);
    if (d != 1)
        return (NUMA *)ERROR_PTR("pixa not 1 bpp", __func__, NULL);

    if ((na = numaCreate(n)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    tab = makePixelSumTab8();
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixa, i, L_CLONE);
        pixCountPixels(pix, &count, tab);
        numaAddNumber(na, count);
        pixDestroy(&pix);
    }

    LEPT_FREE(tab);
    return na;
}


/*!
 * \brief   pixCountPixels()
 *
 * \param[in]    pixs     1 bpp
 * \param[out]   pcount   count of ON pixels
 * \param[in]    tab8     [optional] 8-bit pixel lookup table
 * \return  0 if OK; 1 on error
 */
l_ok
pixCountPixels(PIX      *pixs,
               l_int32  *pcount,
               l_int32  *tab8)
{
l_uint32   endmask;
l_int32    w, h, wpl, i, j;
l_int32    fullwords, endbits, sum;
l_int32   *tab;
l_uint32  *data;

    if (!pcount)
        return ERROR_INT("&count not defined", __func__, 1);
    *pcount = 0;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);

    tab = (tab8) ? tab8 : makePixelSumTab8();
    pixGetDimensions(pixs, &w, &h, NULL);
    wpl = pixGetWpl(pixs);
    data = pixGetData(pixs);
    fullwords = w >> 5;
    endbits = w & 31;
    endmask = (endbits == 0) ? 0 : (0xffffffffU << (32 - endbits));

    sum = 0;
    for (i = 0; i < h; i++, data += wpl) {
        for (j = 0; j < fullwords; j++) {
            l_uint32 word = data[j];
            if (word) {
                sum += tab[word & 0xff] +
                       tab[(word >> 8) & 0xff] +
                       tab[(word >> 16) & 0xff] +
                       tab[(word >> 24) & 0xff];
            }
        }
        if (endbits) {
            l_uint32 word = data[j] & endmask;
            if (word) {
                sum += tab[word & 0xff] +
                       tab[(word >> 8) & 0xff] +
                       tab[(word >> 16) & 0xff] +
                       tab[(word >> 24) & 0xff];
            }
        }
    }
    *pcount = sum;

    if (!tab8) LEPT_FREE(tab);
    return 0;
}


/*!
 * \brief   pixCountPixelsInRect()
 *
 * \param[in]    pixs     1 bpp
 * \param[in]    box      (can be null)
 * \param[out]   pcount   count of ON pixels
 * \param[in]    tab8     [optional] 8-bit pixel lookup table
 * \return  0 if OK; 1 on error
 */
l_ok
pixCountPixelsInRect(PIX      *pixs,
                     BOX      *box,
                     l_int32  *pcount,
                     l_int32  *tab8)
{
l_int32  w, h, bx, by, bw, bh;
BOX     *box1;
PIX     *pix1;

    if (!pcount)
        return ERROR_INT("&count not defined", __func__, 1);
    *pcount = 0;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);

    if (box) {
        pixGetDimensions(pixs, &w, &h, NULL);
        if ((box1 = boxClipToRectangle(box, w, h)) == NULL)
            return ERROR_INT("box1 not made", __func__, 1);
        boxGetGeometry(box1, &bx, &by, &bw, &bh);
        pix1 = pixCreate(bw, bh, 1);
        pixRasterop(pix1, 0, 0, bw, bh, PIX_SRC, pixs, bx, by);
        pixCountPixels(pix1, pcount, tab8);
        pixDestroy(&pix1);
        boxDestroy(&box1);
    } else {
        pixCountPixels(pixs, pcount, tab8);
    }

    return 0;
}


/*!
 * \brief   pixCountByRow()
 *
 * \param[in]   pix   1 bpp
 * \param[in]   box   [optional] clipping box for count; can be null
 * \return  na of number of ON pixels by row, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) To resample for a bin size different from 1, use
 *          numaUniformSampling() on the result of this function.
 * </pre>
 */
NUMA *
pixCountByRow(PIX      *pix,
              BOX      *box)
{
l_int32    i, j, w, h, wpl, count, xstart, xend, ystart, yend, bw, bh;
l_uint32  *line, *data;
NUMA      *na;

    if (!pix || pixGetDepth(pix) != 1)
        return (NUMA *)ERROR_PTR("pix undefined or not 1 bpp", __func__, NULL);
    if (!box)
        return pixCountPixelsByRow(pix, NULL);

    pixGetDimensions(pix, &w, &h, NULL);
    if (boxClipToRectangleParams(box, w, h, &xstart, &ystart, &xend, &yend,
                                 &bw, &bh) == 1)
        return (NUMA *)ERROR_PTR("invalid clipping box", __func__, NULL);

    if ((na = numaCreate(bh)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    numaSetParameters(na, ystart, 1);
    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    for (i = ystart; i < yend; i++) {
        count = 0;
        line = data + i * wpl;
        for (j = xstart; j < xend; j++) {
            if (GET_DATA_BIT(line, j))
                count++;
        }
        numaAddNumber(na, count);
    }

    return na;
}


/*!
 * \brief   pixCountByColumn()
 *
 * \param[in]   pix   1 bpp
 * \param[in]   box   [optional] clipping box for count; can be null
 * \return  na of number of ON pixels by column, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) To resample for a bin size different from 1, use
 *          numaUniformSampling() on the result of this function.
 * </pre>
 */
NUMA *
pixCountByColumn(PIX      *pix,
                 BOX      *box)
{
l_int32    i, j, w, h, wpl, count, xstart, xend, ystart, yend, bw, bh;
l_uint32  *line, *data;
NUMA      *na;

    if (!pix || pixGetDepth(pix) != 1)
        return (NUMA *)ERROR_PTR("pix undefined or not 1 bpp", __func__, NULL);
    if (!box)
        return pixCountPixelsByColumn(pix);

    pixGetDimensions(pix, &w, &h, NULL);
    if (boxClipToRectangleParams(box, w, h, &xstart, &ystart, &xend, &yend,
                                 &bw, &bh) == 1)
        return (NUMA *)ERROR_PTR("invalid clipping box", __func__, NULL);

    if ((na = numaCreate(bw)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    numaSetParameters(na, xstart, 1);
    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    for (j = xstart; j < xend; j++) {
        count = 0;
        for (i = ystart; i < yend; i++) {
            line = data + i * wpl;
            if (GET_DATA_BIT(line, j))
                count++;
        }
        numaAddNumber(na, count);
    }

    return na;
}


/*!
 * \brief   pixCountPixelsByRow()
 *
 * \param[in]   pix   1 bpp
 * \param[in]   tab8  [optional] 8-bit pixel lookup table
 * \return  na of counts, or NULL on error
 */
NUMA *
pixCountPixelsByRow(PIX      *pix,
                    l_int32  *tab8)
{
l_int32   h, i, count;
l_int32  *tab;
NUMA     *na;

    if (!pix || pixGetDepth(pix) != 1)
        return (NUMA *)ERROR_PTR("pix undefined or not 1 bpp", __func__, NULL);

    h = pixGetHeight(pix);
    if ((na = numaCreate(h)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);

    tab = (tab8) ? tab8 : makePixelSumTab8();
    for (i = 0; i < h; i++) {
        pixCountPixelsInRow(pix, i, &count, tab);
        numaAddNumber(na, count);
    }

    if (!tab8) LEPT_FREE(tab);
    return na;
}


/*!
 * \brief   pixCountPixelsByColumn()
 *
 * \param[in]   pix   1 bpp
 * \return  na of counts in each column, or NULL on error
 */
NUMA *
pixCountPixelsByColumn(PIX  *pix)
{
l_int32     i, j, w, h, wpl;
l_uint32   *line, *data;
l_float32  *array;
NUMA       *na;

    if (!pix || pixGetDepth(pix) != 1)
        return (NUMA *)ERROR_PTR("pix undefined or not 1 bpp", __func__, NULL);

    pixGetDimensions(pix, &w, &h, NULL);
    if ((na = numaCreate(w)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    numaSetCount(na, w);
    array = numaGetFArray(na, L_NOCOPY);
    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    for (i = 0; i < h; i++) {
        line = data + wpl * i;
        for (j = 0; j < w; j++) {
            if (GET_DATA_BIT(line, j))
                array[j] += 1.0;
        }
    }

    return na;
}


/*!
 * \brief   pixCountPixelsInRow()
 *
 * \param[in]    pix     1 bpp
 * \param[in]    row     number
 * \param[out]   pcount  sum of ON pixels in raster line
 * \param[in]    tab8    [optional] 8-bit pixel lookup table
 * \return  0 if OK; 1 on error
 */
l_ok
pixCountPixelsInRow(PIX      *pix,
                    l_int32   row,
                    l_int32  *pcount,
                    l_int32  *tab8)
{
l_uint32   word, endmask;
l_int32    j, w, h, wpl;
l_int32    fullwords, endbits, sum;
l_int32   *tab;
l_uint32  *line;

    if (!pcount)
        return ERROR_INT("&count not defined", __func__, 1);
    *pcount = 0;
    if (!pix || pixGetDepth(pix) != 1)
        return ERROR_INT("pix not defined or not 1 bpp", __func__, 1);

    pixGetDimensions(pix, &w, &h, NULL);
    if (row < 0 || row >= h)
        return ERROR_INT("row out of bounds", __func__, 1);
    wpl = pixGetWpl(pix);
    line = pixGetData(pix) + row * wpl;
    fullwords = w >> 5;
    endbits = w & 31;
    endmask = (endbits == 0) ? 0 : (0xffffffffU << (32 - endbits));

    tab = (tab8) ? tab8 : makePixelSumTab8();
    sum = 0;
    for (j = 0; j < fullwords; j++) {
        word = line[j];
        if (word) {
            sum += tab[word & 0xff] +
                   tab[(word >> 8) & 0xff] +
                   tab[(word >> 16) & 0xff] +
                   tab[(word >> 24) & 0xff];
        }
    }
    if (endbits) {
        word = line[j] & endmask;
        if (word) {
            sum += tab[word & 0xff] +
                   tab[(word >> 8) & 0xff] +
                   tab[(word >> 16) & 0xff] +
                   tab[(word >> 24) & 0xff];
        }
    }
    *pcount = sum;

    if (!tab8) LEPT_FREE(tab);
    return 0;
}


/*!
 * \brief   pixGetMomentByColumn()
 *
 * \param[in]   pix     1 bpp
 * \param[in]   order   of moment, either 1 or 2
 * \return  na of first moment of fg pixels, by column, or NULL on error
 */
NUMA *
pixGetMomentByColumn(PIX     *pix,
                     l_int32  order)
{
l_int32     i, j, w, h, wpl;
l_uint32   *line, *data;
l_float32  *array;
NUMA       *na;

    if (!pix || pixGetDepth(pix) != 1)
        return (NUMA *)ERROR_PTR("pix undefined or not 1 bpp", __func__, NULL);
    if (order != 1 && order != 2)
        return (NUMA *)ERROR_PTR("order of moment not 1 or 2", __func__, NULL);

    pixGetDimensions(pix, &w, &h, NULL);
    if ((na = numaCreate(w)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    numaSetCount(na, w);
    array = numaGetFArray(na, L_NOCOPY);
    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    for (i = 0; i < h; i++) {
        line = data + wpl * i;
        for (j = 0; j < w; j++) {
            if (GET_DATA_BIT(line, j)) {
                if (order == 1)
                    array[j] += i;
                else  /* order == 2 */
                    array[j] += i * i;
            }
        }
    }

    return na;
}


/*!
 * \brief   pixThresholdPixelSum()
 *
 * \param[in]    pix      1 bpp
 * \param[in]    thresh   threshold
 * \param[out]   pabove   1 if above threshold;
 *                        0 if equal to or less than threshold
 * \param[in]    tab8     [optional] 8-bit pixel lookup table
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This sums the ON pixels and returns immediately if the count
 *          goes above threshold.  It is therefore more efficient
 *          for matching images (by running this function on the xor of
 *          the 2 images) than using pixCountPixels(), which counts all
 *          pixels before returning.
 * </pre>
 */
l_ok
pixThresholdPixelSum(PIX      *pix,
                     l_int32   thresh,
                     l_int32  *pabove,
                     l_int32  *tab8)
{
l_uint32   word, endmask;
l_int32   *tab;
l_int32    w, h, wpl, i, j;
l_int32    fullwords, endbits, sum;
l_uint32  *line, *data;

    if (!pabove)
        return ERROR_INT("&above not defined", __func__, 1);
    *pabove = 0;
    if (!pix || pixGetDepth(pix) != 1)
        return ERROR_INT("pix not defined or not 1 bpp", __func__, 1);

    tab = (tab8) ? tab8 : makePixelSumTab8();
    pixGetDimensions(pix, &w, &h, NULL);
    wpl = pixGetWpl(pix);
    data = pixGetData(pix);
    fullwords = w >> 5;
    endbits = w & 31;
    endmask = 0xffffffff << (32 - endbits);

    sum = 0;
    for (i = 0; i < h; i++) {
        line = data + wpl * i;
        for (j = 0; j < fullwords; j++) {
            word = line[j];
            if (word) {
                sum += tab[word & 0xff] +
                       tab[(word >> 8) & 0xff] +
                       tab[(word >> 16) & 0xff] +
                       tab[(word >> 24) & 0xff];
            }
        }
        if (endbits) {
            word = line[j] & endmask;
            if (word) {
                sum += tab[word & 0xff] +
                       tab[(word >> 8) & 0xff] +
                       tab[(word >> 16) & 0xff] +
                       tab[(word >> 24) & 0xff];
            }
        }
        if (sum > thresh) {
            *pabove = 1;
            if (!tab8) LEPT_FREE(tab);
            return 0;
        }
    }

    if (!tab8) LEPT_FREE(tab);
    return 0;
}


/*!
 * \brief   makePixelSumTab8()
 *
 * \return  table of 256 l_int32.
 *
 * <pre>
 * Notes:
 *      (1) This table of integers gives the number of 1 bits
 *          in the 8 bit index.
 * </pre>
 */
l_int32 *
makePixelSumTab8(void)
{
l_uint8   byte;
l_int32   i;
l_int32  *tab;

    tab = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    for (i = 0; i < 256; i++) {
        byte = (l_uint8)i;
        tab[i] = (byte & 0x1) +
                 ((byte >> 1) & 0x1) +
                 ((byte >> 2) & 0x1) +
                 ((byte >> 3) & 0x1) +
                 ((byte >> 4) & 0x1) +
                 ((byte >> 5) & 0x1) +
                 ((byte >> 6) & 0x1) +
                 ((byte >> 7) & 0x1);
    }
    return tab;
}


/*!
 * \brief   makePixelCentroidTab8()
 *
 * \return  table of 256 l_int32.
 *
 * <pre>
 * Notes:
 *      (1) This table of integers gives the centroid weight of the 1 bits
 *          in the 8 bit index.  In other words, if sumtab is obtained by
 *          makePixelSumTab8, and centroidtab is obtained by
 *          makePixelCentroidTab8, then, for 1 <= i <= 255,
 *          centroidtab[i] / (float)sumtab[i]
 *          is the centroid of the 1 bits in the 8-bit index i, where the
 *          MSB is considered to have position 0 and the LSB is considered
 *          to have position 7.
 * </pre>
 */
l_int32 *
makePixelCentroidTab8(void)
{
l_int32   i;
l_int32  *tab;

    tab = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    tab[0] = 0;
    tab[1] = 7;
    for (i = 2; i < 4; i++) {
        tab[i] = tab[i - 2] + 6;
    }
    for (i = 4; i < 8; i++) {
        tab[i] = tab[i - 4] + 5;
    }
    for (i = 8; i < 16; i++) {
        tab[i] = tab[i - 8] + 4;
    }
    for (i = 16; i < 32; i++) {
        tab[i] = tab[i - 16] + 3;
    }
    for (i = 32; i < 64; i++) {
        tab[i] = tab[i - 32] + 2;
    }
    for (i = 64; i < 128; i++) {
        tab[i] = tab[i - 64] + 1;
    }
    for (i = 128; i < 256; i++) {
        tab[i] = tab[i - 128];
    }
    return tab;
}


/*-------------------------------------------------------------*
 *             Average of pixel values in gray images          *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixAverageByRow()
 *
 * \param[in]   pix    8 or 16 bpp; no colormap
 * \param[in]   box    [optional] clipping box for sum; can be null
 * \param[in]   type   L_WHITE_IS_MAX, L_BLACK_IS_MAX
 * \return  na of pixel averages by row, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) To resample for a bin size different from 1, use
 *          numaUniformSampling() on the result of this function.
 *      (2) If type == L_BLACK_IS_MAX, black pixels get the maximum
 *          value (0xff for 8 bpp, 0xffff for 16 bpp) and white get 0.
 * </pre>
 */
NUMA *
pixAverageByRow(PIX     *pix,
                BOX     *box,
                l_int32  type)
{
l_int32    i, j, w, h, d, wpl, xstart, xend, ystart, yend, bw, bh;
l_uint32  *line, *data;
l_float64  norm, sum;
NUMA      *na;

    if (!pix)
        return (NUMA *)ERROR_PTR("pix not defined", __func__, NULL);
    pixGetDimensions(pix, &w, &h, &d);
    if (d != 8 && d != 16)
        return (NUMA *)ERROR_PTR("pix not 8 or 16 bpp", __func__, NULL);
    if (type != L_WHITE_IS_MAX && type != L_BLACK_IS_MAX)
        return (NUMA *)ERROR_PTR("invalid type", __func__, NULL);
    if (pixGetColormap(pix) != NULL)
        return (NUMA *)ERROR_PTR("pix colormapped", __func__, NULL);

    if (boxClipToRectangleParams(box, w, h, &xstart, &ystart, &xend, &yend,
                                 &bw, &bh) == 1)
        return (NUMA *)ERROR_PTR("invalid clipping box", __func__, NULL);

    norm = 1. / (l_float32)bw;
    if ((na = numaCreate(bh)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    numaSetParameters(na, ystart, 1);
    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    for (i = ystart; i < yend; i++) {
        sum = 0.0;
        line = data + i * wpl;
        if (d == 8) {
            for (j = xstart; j < xend; j++)
                sum += GET_DATA_BYTE(line, j);
            if (type == L_BLACK_IS_MAX)
                sum = bw * 255 - sum;
        } else {  /* d == 16 */
            for (j = xstart; j < xend; j++)
                sum += GET_DATA_TWO_BYTES(line, j);
            if (type == L_BLACK_IS_MAX)
                sum = bw * 0xffff - sum;
        }
        numaAddNumber(na, (l_float32)(norm * sum));
    }

    return na;
}


/*!
 * \brief   pixAverageByColumn()
 *
 * \param[in]   pix   8 or 16 bpp; no colormap
 * \param[in]   box   [optional] clipping box for sum; can be null
 * \param[in]   type  L_WHITE_IS_MAX, L_BLACK_IS_MAX
 * \return  na of pixel averages by column, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) To resample for a bin size different from 1, use
 *          numaUniformSampling() on the result of this function.
 *      (2) If type == L_BLACK_IS_MAX, black pixels get the maximum
 *          value (0xff for 8 bpp, 0xffff for 16 bpp) and white get 0.
 * </pre>
 */
NUMA *
pixAverageByColumn(PIX     *pix,
                   BOX     *box,
                   l_int32  type)
{
l_int32     i, j, w, h, d, wpl, xstart, xend, ystart, yend, bw, bh;
l_uint32   *line, *data;
l_float32   norm, sum;
NUMA       *na;

    if (!pix)
        return (NUMA *)ERROR_PTR("pix not defined", __func__, NULL);
    pixGetDimensions(pix, &w, &h, &d);

    if (d != 8 && d != 16)
        return (NUMA *)ERROR_PTR("pix not 8 or 16 bpp", __func__, NULL);
    if (type != L_WHITE_IS_MAX && type != L_BLACK_IS_MAX)
        return (NUMA *)ERROR_PTR("invalid type", __func__, NULL);
    if (pixGetColormap(pix) != NULL)
        return (NUMA *)ERROR_PTR("pix colormapped", __func__, NULL);

    if (boxClipToRectangleParams(box, w, h, &xstart, &ystart, &xend, &yend,
                                 &bw, &bh) == 1)
        return (NUMA *)ERROR_PTR("invalid clipping box", __func__, NULL);

    if ((na = numaCreate(bw)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    numaSetParameters(na, xstart, 1);
    norm = 1.f / (l_float32)bh;
    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    for (j = xstart; j < xend; j++) {
        sum = 0.0;
        if (d == 8) {
            for (i = ystart; i < yend; i++) {
                line = data + i * wpl;
                sum += GET_DATA_BYTE(line, j);
            }
            if (type == L_BLACK_IS_MAX)
                sum = bh * 255 - sum;
        } else {  /* d == 16 */
            for (i = ystart; i < yend; i++) {
                line = data + i * wpl;
                sum += GET_DATA_TWO_BYTES(line, j);
            }
            if (type == L_BLACK_IS_MAX)
                sum = bh * 0xffff - sum;
        }
        numaAddNumber(na, (l_float32)(norm * sum));
    }

    return na;
}


/*!
 * \brief   pixAverageInRect()
 *
 * \param[in]    pixs     1, 2, 4, 8 bpp; not cmapped
 * \param[in]    pixm     [optional] 1 bpp mask; if null, use all pixels
 * \param[in]    box      [optional] if null, use entire image
 * \param[in]    minval   ignore values less than this
 * \param[in]    maxval   ignore values greater than this
 * \param[in]    subsamp  subsample factor: integer; use 1 for all pixels
 * \param[out]   pave     average of pixel values under consideration
 * \return  0 if OK; 1 on error; 2 if all pixels are filtered out
 *
 * <pre>
 * Notes:
 *      (1) The average is computed with 4 optional filters: a rectangle,
 *          a mask, a contiguous set of range values, and subsampling.
 *          In practice you might use only one or two of these.
 *      (2) The mask %pixm is a blocking mask: only count pixels in the bg.
 *          If it exists, alignment is assumed at UL corner and computation
 *          is over the minimum intersection of %pixs and %pixm.
 *          If you want the average of pixels under the mask fg, invert it.
 *      (3) Set the range limits %minval = 0 and %maxval = 255 to use
 *          all non-masked pixels (regardless of value) in the average.
 *      (4) If no pixels are used in the averaging, the returned average
 *          value is 0 and the function returns 2.  This is not an error,
 *          but it says to disregard the returned average value.
 *      (5) For example, to average all pixels in a given clipping rect %box,
 *              pixAverageInRect(pixs, NULL, box, 0, 255, 1, &aveval);
 * </pre>
 */
l_ok
pixAverageInRect(PIX        *pixs,
                 PIX        *pixm,
                 BOX        *box,
                 l_int32     minval,
                 l_int32     maxval,
                 l_int32     subsamp,
                 l_float32  *pave)
{
l_int32    w, h, d, wpls, wm, hm, dm, wplm, val, count;
l_int32    i, j, xstart, xend, ystart, yend;
l_uint32  *datas, *datam = NULL, *lines, *linem = NULL;
l_float64  sum;

    if (!pave)
        return ERROR_INT("&ave not defined", __func__, 1);
    *pave = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (pixGetColormap(pixs) != NULL)
        return ERROR_INT("pixs is colormapped", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 1 && d != 2 && d != 4 && d != 8)
        return ERROR_INT("pixs not 1, 2, 4 or 8 bpp", __func__, 1);
    if (pixm) {
        pixGetDimensions(pixm, &wm, &hm, &dm);
        if (dm != 1)
            return ERROR_INT("pixm not 1 bpp", __func__, 1);
        w = L_MIN(w, wm);
        h = L_MIN(h, hm);
    }
    if (subsamp < 1)
        return ERROR_INT("subsamp must be >= 1", __func__, 1);

    if (boxClipToRectangleParams(box, w, h, &xstart, &ystart, &xend, &yend,
                                 NULL, NULL) == 1)
        return ERROR_INT("invalid clipping box", __func__, 1);

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if (pixm) {
        datam = pixGetData(pixm);
        wplm = pixGetWpl(pixm);
    }
    sum = 0.0;
    count = 0;
    for (i = ystart; i < yend; i += subsamp) {
        lines = datas + i * wpls;
        if (pixm)
            linem = datam + i * wplm;
        for (j = xstart; j < xend; j += subsamp) {
            if (pixm && (GET_DATA_BIT(linem, j) == 1))
                continue;
            if (d == 1)
                val = GET_DATA_BIT(lines, j);
            else if (d == 2)
                val = GET_DATA_DIBIT(lines, j);
            else if (d == 4)
                val = GET_DATA_QBIT(lines, j);
            else  /* d == 8 */
                val = GET_DATA_BYTE(lines, j);
            if (val >= minval && val <= maxval) {
                sum += val;
                count++;
            }
        }
    }

    if (count == 0)
        return 2;  /* not an error; don't use the average value (0.0) */
    *pave = sum / (l_float32)count;
    return 0;
}


/*-------------------------------------------------------------*
 *             Average of pixel values in RGB images           *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixAverageInRectRGB()
 *
 * \param[in]    pixs     rgb; not cmapped
 * \param[in]    pixm     [optional] 1 bpp mask; if null, use all pixels
 * \param[in]    box      [optional] if null, use entire image
 * \param[in]    subsamp  subsample factor: integer; use 1 for all pixels
 * \param[out]   pave     average color of pixel values under consideration,
 *                        in format 0xrrggbb00.
 * \return  0 if OK; 1 on error; 2 if all pixels are filtered out
 *
 * <pre>
 * Notes:
 *      (1) The average is computed with 3 optional filters: a rectangle,
 *          a mask, and subsampling.
 *          In practice you might use only one or two of these.
 *      (2) The mask %pixm is a blocking mask: only count pixels in the bg.
 *          If it exists, alignment is assumed at UL corner and computation
 *          is over the minimum intersection of %pixs and %pixm.
 *          If you want the average of pixels under the mask fg, invert it.
 *      (3) If no pixels are used in the averaging, the returned average
 *          value is 0 and the function returns 2.  This is not an error,
 *          but it says to disregard the returned average value.
 *      (4) For example, to average all pixels in a given clipping rect %box,
 *              pixAverageInRectRGB(pixs, NULL, box, 1, &aveval);
 * </pre>
 */
l_ok
pixAverageInRectRGB(PIX       *pixs,
                    PIX       *pixm,
                    BOX       *box,
                    l_int32    subsamp,
                    l_uint32  *pave)
{
l_int32    w, h, wpls, wm, hm, dm, wplm, i, j, xstart, xend, ystart, yend;
l_int32    rval, gval, bval, rave, gave, bave, count;
l_uint32  *datas, *datam = NULL, *lines, *linem = NULL;
l_uint32   pixel;
l_float64  rsum, gsum, bsum;

    if (!pave)
        return ERROR_INT("&ave not defined", __func__, 1);
    *pave = 0;
    if (!pixs || pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs undefined or not 32 bpp", __func__, 1);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (pixm) {
        pixGetDimensions(pixm, &wm, &hm, &dm);
        if (dm != 1)
            return ERROR_INT("pixm not 1 bpp", __func__, 1);
        w = L_MIN(w, wm);
        h = L_MIN(h, hm);
    }
    if (subsamp < 1)
        return ERROR_INT("subsamp must be >= 1", __func__, 1);

    if (boxClipToRectangleParams(box, w, h, &xstart, &ystart, &xend, &yend,
                                 NULL, NULL) == 1)
        return ERROR_INT("invalid clipping box", __func__, 1);

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if (pixm) {
        datam = pixGetData(pixm);
        wplm = pixGetWpl(pixm);
    }
    rsum = gsum = bsum = 0.0;
    count = 0;
    for (i = ystart; i < yend; i += subsamp) {
        lines = datas + i * wpls;
        if (pixm)
            linem = datam + i * wplm;
        for (j = xstart; j < xend; j += subsamp) {
            if (pixm && (GET_DATA_BIT(linem, j) == 1))
                continue;
            pixel = *(lines + j);
            extractRGBValues(pixel, &rval, &gval, &bval);
            rsum += rval;
            gsum += gval;
            bsum += bval;
            count++;
        }
    }

    if (count == 0)
        return 2;  /* not an error */
    rave = (l_uint32)(rsum / (l_float64)count);
    gave = (l_uint32)(gsum / (l_float64)count);
    bave = (l_uint32)(bsum / (l_float64)count);
    composeRGBPixel(rave, gave, bave, pave);
    return 0;
}


/*------------------------------------------------------------------*
 *               Variance of pixel values in gray images            *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixVarianceByRow()
 *
 * \param[in]   pix   8 or 16 bpp; no colormap
 * \param[in]   box   [optional] clipping box for variance; can be null
 * \return  na of rmsdev by row, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) To resample for a bin size different from 1, use
 *          numaUniformSampling() on the result of this function.
 *      (2) We are actually computing the RMS deviation in each row.
 *          This is the square root of the variance.
 * </pre>
 */
NUMA *
pixVarianceByRow(PIX     *pix,
                 BOX     *box)
{
l_int32     i, j, w, h, d, wpl, xstart, xend, ystart, yend, bw, bh, val;
l_uint32   *line, *data;
l_float64   sum1, sum2, norm, ave, var, rootvar;
NUMA       *na;

    if (!pix)
        return (NUMA *)ERROR_PTR("pix not defined", __func__, NULL);
    pixGetDimensions(pix, &w, &h, &d);
    if (d != 8 && d != 16)
        return (NUMA *)ERROR_PTR("pix not 8 or 16 bpp", __func__, NULL);
    if (pixGetColormap(pix) != NULL)
        return (NUMA *)ERROR_PTR("pix colormapped", __func__, NULL);

    if (boxClipToRectangleParams(box, w, h, &xstart, &ystart, &xend, &yend,
                                 &bw, &bh) == 1)
        return (NUMA *)ERROR_PTR("invalid clipping box", __func__, NULL);

    if ((na = numaCreate(bh)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    numaSetParameters(na, ystart, 1);
    norm = 1. / (l_float32)bw;
    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    for (i = ystart; i < yend; i++) {
        sum1 = sum2 = 0.0;
        line = data + i * wpl;
        for (j = xstart; j < xend; j++) {
            if (d == 8)
                val = GET_DATA_BYTE(line, j);
            else  /* d == 16 */
                val = GET_DATA_TWO_BYTES(line, j);
            sum1 += val;
            sum2 += (l_float64)(val) * val;
        }
        ave = norm * sum1;
        var = norm * sum2 - ave * ave;
        rootvar = sqrt(var);
        numaAddNumber(na, (l_float32)rootvar);
    }

    return na;
}


/*!
 * \brief   pixVarianceByColumn()
 *
 * \param[in]   pix   8 or 16 bpp; no colormap
 * \param[in]   box   [optional] clipping box for variance; can be null
 * \return  na of rmsdev by column, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) To resample for a bin size different from 1, use
 *          numaUniformSampling() on the result of this function.
 *      (2) We are actually computing the RMS deviation in each row.
 *          This is the square root of the variance.
 * </pre>
 */
NUMA *
pixVarianceByColumn(PIX     *pix,
                    BOX     *box)
{
l_int32     i, j, w, h, d, wpl, xstart, xend, ystart, yend, bw, bh, val;
l_uint32   *line, *data;
l_float64   sum1, sum2, norm, ave, var, rootvar;
NUMA       *na;

    if (!pix)
        return (NUMA *)ERROR_PTR("pix not defined", __func__, NULL);
    pixGetDimensions(pix, &w, &h, &d);
    if (d != 8 && d != 16)
        return (NUMA *)ERROR_PTR("pix not 8 or 16 bpp", __func__, NULL);
    if (pixGetColormap(pix) != NULL)
        return (NUMA *)ERROR_PTR("pix colormapped", __func__, NULL);

    if (boxClipToRectangleParams(box, w, h, &xstart, &ystart, &xend, &yend,
                                 &bw, &bh) == 1)
        return (NUMA *)ERROR_PTR("invalid clipping box", __func__, NULL);

    if ((na = numaCreate(bw)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    numaSetParameters(na, xstart, 1);
    norm = 1. / (l_float32)bh;
    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    for (j = xstart; j < xend; j++) {
        sum1 = sum2 = 0.0;
        for (i = ystart; i < yend; i++) {
            line = data + wpl * i;
            if (d == 8)
                val = GET_DATA_BYTE(line, j);
            else  /* d == 16 */
                val = GET_DATA_TWO_BYTES(line, j);
            sum1 += val;
            sum2 += (l_float64)(val) * val;
        }
        ave = norm * sum1;
        var = norm * sum2 - ave * ave;
        rootvar = sqrt(var);
        numaAddNumber(na, (l_float32)rootvar);
    }

    return na;
}


/*!
 * \brief   pixVarianceInRect()
 *
 * \param[in]    pix       1, 2, 4, 8 bpp; not cmapped
 * \param[in]    box       [optional] if null, use entire image
 * \param[out]   prootvar  sqrt variance of pixel values in region
 * \return  0 if OK; 1 on error
 */
l_ok
pixVarianceInRect(PIX        *pix,
                  BOX        *box,
                  l_float32  *prootvar)
{
l_int32    w, h, d, wpl, i, j, xstart, xend, ystart, yend, bw, bh, val;
l_uint32  *data, *line;
l_float64  sum1, sum2, norm, ave, var;

    if (!prootvar)
        return ERROR_INT("&rootvar not defined", __func__, 1);
    *prootvar = 0.0;
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    pixGetDimensions(pix, &w, &h, &d);
    if (d != 1 && d != 2 && d != 4 && d != 8)
        return ERROR_INT("pix not 1, 2, 4 or 8 bpp", __func__, 1);
    if (pixGetColormap(pix) != NULL)
        return ERROR_INT("pix is colormapped", __func__, 1);

    if (boxClipToRectangleParams(box, w, h, &xstart, &ystart, &xend, &yend,
                                 &bw, &bh) == 1)
        return ERROR_INT("invalid clipping box", __func__, 1);

    wpl = pixGetWpl(pix);
    data = pixGetData(pix);
    sum1 = sum2 = 0.0;
    for (i = ystart; i < yend; i++) {
        line = data + i * wpl;
        for (j = xstart; j < xend; j++) {
            if (d == 1) {
                val = GET_DATA_BIT(line, j);
                sum1 += val;
                sum2 += (l_float64)(val) * val;
            } else if (d == 2) {
                val = GET_DATA_DIBIT(line, j);
                sum1 += val;
                sum2 += (l_float64)(val) * val;
            } else if (d == 4) {
                val = GET_DATA_QBIT(line, j);
                sum1 += val;
                sum2 += (l_float64)(val) * val;
            } else {  /* d == 8 */
                val = GET_DATA_BYTE(line, j);
                sum1 += val;
                sum2 += (l_float64)(val) * val;
            }
        }
    }
    norm = 1.0 / ((l_float64)(bw) * bh);
    ave = norm * sum1;
    var = norm * sum2 - ave * ave;
    *prootvar = (l_float32)sqrt(var);
    return 0;
}


/*---------------------------------------------------------------------*
 *    Average of absolute value of pixel differences in gray images    *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixAbsDiffByRow()
 *
 * \param[in]   pix   8 bpp; no colormap
 * \param[in]   box   [optional] clipping box for region; can be null
 * \return  na of abs val pixel difference averages by row, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is an average over differences of adjacent pixels along
 *          each row.
 *      (2) To resample for a bin size different from 1, use
 *          numaUniformSampling() on the result of this function.
 * </pre>
 */
NUMA *
pixAbsDiffByRow(PIX  *pix,
                BOX  *box)
{
l_int32    i, j, w, h, wpl, xstart, xend, ystart, yend, bw, bh, val0, val1;
l_uint32  *line, *data;
l_float64  norm, sum;
NUMA      *na;

    if (!pix || pixGetDepth(pix) != 8)
        return (NUMA *)ERROR_PTR("pix undefined or not 8 bpp", __func__, NULL);
    if (pixGetColormap(pix) != NULL)
        return (NUMA *)ERROR_PTR("pix colormapped", __func__, NULL);

    pixGetDimensions(pix, &w, &h, NULL);
    if (boxClipToRectangleParams(box, w, h, &xstart, &ystart, &xend, &yend,
                                 &bw, &bh) == 1)
        return (NUMA *)ERROR_PTR("invalid clipping box", __func__, NULL);
    if (bw < 2)
        return (NUMA *)ERROR_PTR("row width must be >= 2", __func__, NULL);

    norm = 1. / (l_float32)(bw - 1);
    if ((na = numaCreate(bh)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    numaSetParameters(na, ystart, 1);
    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    for (i = ystart; i < yend; i++) {
        sum = 0.0;
        line = data + i * wpl;
        val0 = GET_DATA_BYTE(line, xstart);
        for (j = xstart + 1; j < xend; j++) {
            val1 = GET_DATA_BYTE(line, j);
            sum += L_ABS(val1 - val0);
            val0 = val1;
        }
        numaAddNumber(na, (l_float32)(norm * sum));
    }

    return na;
}


/*!
 * \brief   pixAbsDiffByColumn()
 *
 * \param[in]   pix   8 bpp; no colormap
 * \param[in]   box   [optional] clipping box for region; can be null
 * \return  na of abs val pixel difference averages by column,
 *              or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is an average over differences of adjacent pixels along
 *          each column.
 *      (2) To resample for a bin size different from 1, use
 *          numaUniformSampling() on the result of this function.
 * </pre>
 */
NUMA *
pixAbsDiffByColumn(PIX  *pix,
                   BOX  *box)
{
l_int32    i, j, w, h, wpl, xstart, xend, ystart, yend, bw, bh, val0, val1;
l_uint32  *line, *data;
l_float64  norm, sum;
NUMA      *na;

    if (!pix || pixGetDepth(pix) != 8)
        return (NUMA *)ERROR_PTR("pix undefined or not 8 bpp", __func__, NULL);
    if (pixGetColormap(pix) != NULL)
        return (NUMA *)ERROR_PTR("pix colormapped", __func__, NULL);

    pixGetDimensions(pix, &w, &h, NULL);
    if (boxClipToRectangleParams(box, w, h, &xstart, &ystart, &xend, &yend,
                                 &bw, &bh) == 1)
        return (NUMA *)ERROR_PTR("invalid clipping box", __func__, NULL);
    if (bh < 2)
        return (NUMA *)ERROR_PTR("column height must be >= 2", __func__, NULL);

    norm = 1. / (l_float32)(bh - 1);
    if ((na = numaCreate(bw)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    numaSetParameters(na, xstart, 1);
    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    for (j = xstart; j < xend; j++) {
        sum = 0.0;
        line = data + ystart * wpl;
        val0 = GET_DATA_BYTE(line, j);
        for (i = ystart + 1; i < yend; i++) {
            line = data + i * wpl;
            val1 = GET_DATA_BYTE(line, j);
            sum += L_ABS(val1 - val0);
            val0 = val1;
        }
        numaAddNumber(na, (l_float32)(norm * sum));
    }

    return na;
}


/*!
 * \brief   pixAbsDiffInRect()
 *
 * \param[in]   pix       8 bpp; not cmapped
 * \param[in]   box       [optional] if null, use entire image
 * \param[in]   dir       differences along L_HORIZONTAL_LINE or L_VERTICAL_LINE
 * \param[out]  pabsdiff  average of abs diff pixel values in region
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This gives the average over the abs val of differences of
 *          adjacent pixels values, along either each
 *             row:     dir == L_HORIZONTAL_LINE
 *             column:  dir == L_VERTICAL_LINE
 * </pre>
 */
l_ok
pixAbsDiffInRect(PIX        *pix,
                 BOX        *box,
                 l_int32     dir,
                 l_float32  *pabsdiff)
{
l_int32    w, h, wpl, i, j, xstart, xend, ystart, yend, bw, bh, val0, val1;
l_uint32  *data, *line;
l_float64  norm, sum;

    if (!pabsdiff)
        return ERROR_INT("&absdiff not defined", __func__, 1);
    *pabsdiff = 0.0;
    if (!pix || pixGetDepth(pix) != 8)
        return ERROR_INT("pix undefined or not 8 bpp", __func__, 1);
    if (dir != L_HORIZONTAL_LINE && dir != L_VERTICAL_LINE)
        return ERROR_INT("invalid direction", __func__, 1);
    if (pixGetColormap(pix) != NULL)
        return ERROR_INT("pix is colormapped", __func__, 1);

    pixGetDimensions(pix, &w, &h, NULL);
    if (boxClipToRectangleParams(box, w, h, &xstart, &ystart, &xend, &yend,
                                 &bw, &bh) == 1)
        return ERROR_INT("invalid clipping box", __func__, 1);

    wpl = pixGetWpl(pix);
    data = pixGetData(pix);
    if (dir == L_HORIZONTAL_LINE) {
        norm = 1. / (l_float32)(bh * (bw - 1));
        sum = 0.0;
        for (i = ystart; i < yend; i++) {
            line = data + i * wpl;
            val0 = GET_DATA_BYTE(line, xstart);
            for (j = xstart + 1; j < xend; j++) {
                val1 = GET_DATA_BYTE(line, j);
                sum += L_ABS(val1 - val0);
                val0 = val1;
            }
        }
    } else {  /* vertical line */
        norm = 1. / (l_float32)(bw * (bh - 1));
        sum = 0.0;
        for (j = xstart; j < xend; j++) {
            line = data + ystart * wpl;
            val0 = GET_DATA_BYTE(line, j);
            for (i = ystart + 1; i < yend; i++) {
                line = data + i * wpl;
                val1 = GET_DATA_BYTE(line, j);
                sum += L_ABS(val1 - val0);
                val0 = val1;
            }
        }
    }
    *pabsdiff = (l_float32)(norm * sum);
    return 0;
}


/*!
 * \brief   pixAbsDiffOnLine()
 *
 * \param[in]    pix        8 bpp; not cmapped
 * \param[in]    x1, y1     first point; x1 <= x2, y1 <= y2
 * \param[in]    x2, y2     first point
 * \param[out]   pabsdiff   average of abs diff pixel values on line
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This gives the average over the abs val of differences of
 *          adjacent pixels values, along a line that is either horizontal
 *          or vertical.
 *      (2) If horizontal, require x1 < x2; if vertical, require y1 < y2.
 * </pre>
 */
l_ok
pixAbsDiffOnLine(PIX        *pix,
                 l_int32     x1,
                 l_int32     y1,
                 l_int32     x2,
                 l_int32     y2,
                 l_float32  *pabsdiff)
{
l_int32    w, h, i, j, dir, size, sum;
l_uint32   val0, val1;

    if (!pabsdiff)
        return ERROR_INT("&absdiff not defined", __func__, 1);
    *pabsdiff = 0.0;
    if (!pix || pixGetDepth(pix) != 8)
        return ERROR_INT("pix undefined or not 8 bpp", __func__, 1);
    if (y1 == y2) {
        dir = L_HORIZONTAL_LINE;
    } else if (x1 == x2) {
        dir = L_VERTICAL_LINE;
    } else {
        return ERROR_INT("line is neither horiz nor vert", __func__, 1);
    }
    if (pixGetColormap(pix) != NULL)
        return ERROR_INT("pix is colormapped", __func__, 1);

    pixGetDimensions(pix, &w, &h, NULL);
    sum = 0;
    if (dir == L_HORIZONTAL_LINE) {
        x1 = L_MAX(x1, 0);
        x2 = L_MIN(x2, w - 1);
        if (x1 >= x2)
            return ERROR_INT("x1 >= x2", __func__, 1);
        size = x2 - x1;
        pixGetPixel(pix, x1, y1, &val0);
        for (j = x1 + 1; j <= x2; j++) {
            pixGetPixel(pix, j, y1, &val1);
            sum += L_ABS((l_int32)val1 - (l_int32)val0);
            val0 = val1;
        }
    } else {  /* vertical */
        y1 = L_MAX(y1, 0);
        y2 = L_MIN(y2, h - 1);
        if (y1 >= y2)
            return ERROR_INT("y1 >= y2", __func__, 1);
        size = y2 - y1;
        pixGetPixel(pix, x1, y1, &val0);
        for (i = y1 + 1; i <= y2; i++) {
            pixGetPixel(pix, x1, i, &val1);
            sum += L_ABS((l_int32)val1 - (l_int32)val0);
            val0 = val1;
        }
    }
    *pabsdiff = (l_float32)sum / (l_float32)size;
    return 0;
}


/*-------------------------------------------------------------*
 *              Count of pixels with specific value            *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixCountArbInRect()
 *
 * \param[in]    pixs     1,2,4,8 bpp; can be colormapped
 * \param[in]    box      [optional] over which count is made;
 *                        use entire image if NULL
 * \param[in]    val      pixel value to count
 * \param[in]    factor   subsampling factor; integer >= 1
 * \param[out]   pcount   count; estimate it if factor > 1
 * \return  na histogram, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If pixs is cmapped, %val is compared to the colormap index;
 *          otherwise, %val is compared to the grayscale value.
 *      (2) Set the subsampling %factor > 1 to reduce the amount of computation.
 *          If %factor > 1, multiply the count by %factor * %factor.
 * </pre>
 */
l_int32
pixCountArbInRect(PIX      *pixs,
                  BOX      *box,
                  l_int32   val,
                  l_int32   factor,
                  l_int32  *pcount)
{
l_int32    i, j, bx, by, bw, bh, w, h, d, wpl, pixval;
l_uint32  *data, *line;

    if (!pcount)
        return ERROR_INT("&count not defined", __func__, 1);
    *pcount = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    d = pixGetDepth(pixs);
    if (d != 1 && d != 2 && d != 4 && d != 8)
        return ERROR_INT("pixs not 1, 2, 4 or 8 bpp", __func__, 1);
    if (val < 0)
        return ERROR_INT("val < 0", __func__, 1);
    if (val > (1 << d) - 1) {
        L_ERROR("invalid val = %d for depth %d\n", __func__, val, d);
        return 1;
    }
    if (factor < 1)
        return ERROR_INT("sampling factor < 1", __func__, 1);

    pixGetDimensions(pixs, &w, &h, NULL);
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    if (!box) {
        for (i = 0; i < h; i += factor) {
            line = data + i * wpl;
            for (j = 0; j < w; j += factor) {
                if (d == 8) {
                    pixval = GET_DATA_BYTE(line, j);
                } else if (d == 1) {
                    pixval = GET_DATA_BIT(line, j);
                } else if (d == 2) {
                    pixval = GET_DATA_DIBIT(line, j);
                } else  /* d == 4 */  {
                    pixval = GET_DATA_QBIT(line, j);
                }
                if (pixval == val) (*pcount)++;
            }
        }
    } else {
        boxGetGeometry(box, &bx, &by, &bw, &bh);
        for (i = 0; i < bh; i += factor) {
            if (by + i < 0 || by + i >= h) continue;
            line = data + (by + i) * wpl;
            for (j = 0; j < bw; j += factor) {
                if (bx + j < 0 || bx + j >= w) continue;
                if (d == 8) {
                    pixval = GET_DATA_BYTE(line, bx + j);
                } else if (d == 1) {
                    pixval = GET_DATA_BIT(line, bx + j);
                } else if (d == 2) {
                    pixval = GET_DATA_DIBIT(line, bx + j);
                } else  /* d == 4 */  {
                    pixval = GET_DATA_QBIT(line, bx + j);
                }
                if (pixval == val) (*pcount)++;
            }
        }
    }

    if (factor > 1)  /* assume pixel color is randomly distributed */
        *pcount = *pcount * factor * factor;
    return 0;
}


/*-------------------------------------------------------------*
 *              Mirrored tiling of a smaller image             *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixMirroredTiling()
 *
 * \param[in]   pixs   8 or 32 bpp, small tile; to be replicated
 * \param[in]   w, h   dimensions of output pix
 * \return  pixd usually larger pix, mirror-tiled with pixs,
 *              or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This uses mirrored tiling, where each row alternates
 *          with LR flips and every column alternates with TB
 *          flips, such that the result is a tiling with identical
 *          2 x 2 tiles, each of which is composed of these transforms:
 *                  -----------------
 *                  | 1    |  LR    |
 *                  -----------------
 *                  | TB   |  LR/TB |
 *                  -----------------
 * </pre>
 */
PIX *
pixMirroredTiling(PIX     *pixs,
                  l_int32  w,
                  l_int32  h)
{
l_int32   wt, ht, d, i, j, nx, ny;
PIX      *pixd, *pixsfx, *pixsfy, *pixsfxy, *pix;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &wt, &ht, &d);
    if (wt <= 0 || ht <= 0)
        return (PIX *)ERROR_PTR("pixs size illegal", __func__, NULL);
    if (d != 8 && d != 32)
        return (PIX *)ERROR_PTR("depth not 32 bpp", __func__, NULL);

    if ((pixd = pixCreate(w, h, d)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopySpp(pixd, pixs);

    nx = (w + wt - 1) / wt;
    ny = (h + ht - 1) / ht;
    pixsfx = pixFlipLR(NULL, pixs);
    pixsfy = pixFlipTB(NULL, pixs);
    pixsfxy = pixFlipTB(NULL, pixsfx);
    for (i = 0; i < ny; i++) {
        for (j = 0; j < nx; j++) {
            pix = pixs;
            if ((i & 1) && !(j & 1))
                pix = pixsfy;
            else if (!(i & 1) && (j & 1))
                pix = pixsfx;
            else if ((i & 1) && (j & 1))
                pix = pixsfxy;
            pixRasterop(pixd, j * wt, i * ht, wt, ht, PIX_SRC, pix, 0, 0);
        }
    }

    pixDestroy(&pixsfx);
    pixDestroy(&pixsfy);
    pixDestroy(&pixsfxy);
    return pixd;
}


/*!
 * \brief   pixFindRepCloseTile()
 *
 * \param[in]    pixs       32 bpp rgb
 * \param[in]    box        region of pixs to search around
 * \param[in]    searchdir  L_HORIZ or L_VERT; direction to search
 * \param[in]    mindist    min distance of selected tile edge from box; >= 0
 * \param[in]    tsize      tile size; > 1; even; typically ~50
 * \param[in]    ntiles     number of tiles tested in each row/column
 * \param[out]   pboxtile   region of best tile
 * \param[in]    debug 1    for debug output
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This looks for one or two square tiles with conforming median
 *          intensity and low variance, that is outside but near the input box.
 *      (2) %mindist specifies the gap between the box and the
 *          potential tiles.  The tiles are given an overlap of 50%.
 *          %ntiles specifies the number of tiles that are tested
 *          beyond %mindist for each row or column.
 *      (3) For example, if %mindist = 20, %tilesize = 50 and %ntiles = 3,
 *          a horizontal search to the right will have 3 tiles in each row,
 *          with left edges at 20, 45 and 70 from the right edge of the
 *          input %box.  The number of rows of tiles is determined by
 *          the height of %box and %tsize, with the 50% overlap..
 * </pre>
 */
l_ok
pixFindRepCloseTile(PIX     *pixs,
                    BOX     *box,
                    l_int32  searchdir,
                    l_int32  mindist,
                    l_int32  tsize,
                    l_int32  ntiles,
                    BOX    **pboxtile,
                    l_int32  debug)
{
l_int32    w, h, i, n, bestindex;
l_float32  var_of_mean, median_of_mean, median_of_stdev, mean_val, stdev_val;
l_float32  mindels, bestdelm, delm, dels, mean, stdev;
BOXA      *boxa;
NUMA      *namean, *nastdev;
PIX       *pix, *pixg;
PIXA      *pixa;

    if (!pboxtile)
        return ERROR_INT("&boxtile not defined", __func__, 1);
    *pboxtile = NULL;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);
    if (searchdir != L_HORIZ && searchdir != L_VERT)
        return ERROR_INT("invalid searchdir", __func__, 1);
    if (mindist < 0)
        return ERROR_INT("mindist must be >= 0", __func__, 1);
    if (tsize < 2)
        return ERROR_INT("tsize must be > 1", __func__, 1);
    if (ntiles > 7) {
        L_WARNING("ntiles = %d; larger than suggested max of 7\n",
                  __func__, ntiles);
    }

        /* Locate tile regions */
    pixGetDimensions(pixs, &w, &h, NULL);
    boxa = findTileRegionsForSearch(box, w, h, searchdir, mindist,
                                    tsize, ntiles);
    if (!boxa)
        return ERROR_INT("no tiles found", __func__, 1);

        /* Generate the tiles and the mean and stdev of intensity */
    pixa = pixClipRectangles(pixs, boxa);
    n = pixaGetCount(pixa);
    namean = numaCreate(n);
    nastdev = numaCreate(n);
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixa, i, L_CLONE);
        pixg = pixConvertRGBToGray(pix, 0.33f, 0.34f, 0.33f);
        pixGetAverageMasked(pixg, NULL, 0, 0, 1, L_MEAN_ABSVAL, &mean);
        pixGetAverageMasked(pixg, NULL, 0, 0, 1, L_STANDARD_DEVIATION, &stdev);
        numaAddNumber(namean, mean);
        numaAddNumber(nastdev, stdev);
        pixDestroy(&pix);
        pixDestroy(&pixg);
    }

        /* Find the median and variance of the averages.  We require
         * the best tile to have a mean pixel intensity within a standard
         * deviation of the median of mean intensities, and choose the
         * tile in that set with the smallest stdev of pixel intensities
         * (as a proxy for the tile with least visible structure).
         * The median of the stdev is used, for debugging, as a normalizing
         * factor for the stdev of intensities within a tile. */
    numaGetStatsUsingHistogram(namean, 256, NULL, NULL, NULL, &var_of_mean,
                               &median_of_mean, 0.0, NULL, NULL);
    numaGetStatsUsingHistogram(nastdev, 256, NULL, NULL, NULL, NULL,
                               &median_of_stdev, 0.0, NULL, NULL);
    mindels = 1000.0;
    bestdelm = 1000.0;
    bestindex = 0;
    for (i = 0; i < n; i++) {
        numaGetFValue(namean, i, &mean_val);
        numaGetFValue(nastdev, i, &stdev_val);
        if (var_of_mean == 0.0) {  /* uniform color; any box will do */
            delm = 0.0;  /* any value < 1.01 */
            dels = 1.0;  /* n'importe quoi */
        } else {
            delm = L_ABS(mean_val - median_of_mean) / sqrt(var_of_mean);
            dels = stdev_val / median_of_stdev;
        }
        if (delm < 1.01) {
            if (dels < mindels) {
                if (debug) {
                    lept_stderr("i = %d, mean = %7.3f, delm = %7.3f,"
                                " stdev = %7.3f, dels = %7.3f\n",
                                i, mean_val, delm, stdev_val, dels);
                }
                mindels = dels;
                bestdelm = delm;
                bestindex = i;
            }
        }
    }
    *pboxtile = boxaGetBox(boxa, bestindex, L_COPY);

    if (debug) {
        L_INFO("median of mean = %7.3f\n", __func__, median_of_mean);
        L_INFO("standard dev of mean = %7.3f\n", __func__, sqrt(var_of_mean));
        L_INFO("median of stdev = %7.3f\n", __func__, median_of_stdev);
        L_INFO("best tile: index = %d\n", __func__, bestindex);
        L_INFO("delta from median in units of stdev = %5.3f\n",
               __func__, bestdelm);
        L_INFO("stdev as fraction of median stdev = %5.3f\n",
               __func__, mindels);
    }

    numaDestroy(&namean);
    numaDestroy(&nastdev);
    pixaDestroy(&pixa);
    boxaDestroy(&boxa);
    return 0;
}


/*!
 * \brief   findTileRegionsForSearch()
 *
 * \param[in]   box        region of Pix to search around
 * \param[in]   w, h       dimensions of Pix
 * \param[in]   searchdir  L_HORIZ or L_VERT; direction to search
 * \param[in]   mindist    min distance of selected tile edge from box; >= 0
 * \param[in]   tsize      tile size; > 1; even; typically ~50
 * \param[in]   ntiles     number of tiles tested in each row/column
 * \return  boxa if OK, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See calling function pixfindRepCloseTile().
 * </pre>
 */
static BOXA *
findTileRegionsForSearch(BOX     *box,
                         l_int32  w,
                         l_int32  h,
                         l_int32  searchdir,
                         l_int32  mindist,
                         l_int32  tsize,
                         l_int32  ntiles)
{
l_int32  bx, by, bw, bh, left, right, top, bot, i, j, nrows, ncols;
l_int32  x0, y0, x, y, w_avail, w_needed, h_avail, h_needed, t_avail;
BOX     *box1;
BOXA    *boxa;

    if (!box)
        return (BOXA *)ERROR_PTR("box not defined", __func__, NULL);
    if (ntiles == 0)
        return (BOXA *)ERROR_PTR("no tiles requested", __func__, NULL);

    boxGetGeometry(box, &bx, &by, &bw, &bh);
    if (searchdir == L_HORIZ) {
            /* Find the tile parameters for the search.  Note that the
             * tiles are overlapping by 50% in each direction. */
        left = bx;   /* distance to left of box */
        right = w - bx - bw + 1;   /* distance to right of box */
        w_avail = L_MAX(left, right) - mindist;
        if (tsize & 1) tsize++;  /* be sure it's even */
        if (w_avail < tsize) {
            L_ERROR("tsize = %d, w_avail = %d\n", __func__, tsize, w_avail);
            return NULL;
        }
        w_needed = tsize + (ntiles - 1) * (tsize / 2);
        if (w_needed > w_avail) {
            t_avail = 1 + 2 * (w_avail - tsize) / tsize;
            L_WARNING("ntiles = %d; room for only %d\n", __func__,
                      ntiles, t_avail);
            ntiles = t_avail;
            w_needed = tsize + (ntiles - 1) * (tsize / 2);
        }
        nrows = L_MAX(1, 1 + 2 * (bh - tsize) / tsize);

            /* Generate the tile regions to search */
        boxa = boxaCreate(0);
        if (left > right)  /* search to left */
            x0 = bx - w_needed;
        else  /* search to right */
            x0 = bx + bw + mindist;
        for (i = 0; i < nrows; i++) {
            y = by + i * tsize / 2;
            for (j = 0; j < ntiles; j++) {
                x = x0 + j * tsize / 2;
                box1 = boxCreate(x, y, tsize, tsize);
                boxaAddBox(boxa, box1, L_INSERT);
            }
        }
    } else {  /* L_VERT */
            /* Find the tile parameters for the search */
        top = by;   /* distance above box */
        bot = h - by - bh + 1;   /* distance below box */
        h_avail = L_MAX(top, bot) - mindist;
        if (h_avail < tsize) {
            L_ERROR("tsize = %d, h_avail = %d\n", __func__, tsize, h_avail);
            return NULL;
        }
        h_needed = tsize + (ntiles - 1) * (tsize / 2);
        if (h_needed > h_avail) {
            t_avail = 1 + 2 * (h_avail - tsize) / tsize;
            L_WARNING("ntiles = %d; room for only %d\n", __func__,
                      ntiles, t_avail);
            ntiles = t_avail;
            h_needed = tsize + (ntiles - 1) * (tsize / 2);
        }
        ncols = L_MAX(1, 1 + 2 * (bw - tsize) / tsize);

            /* Generate the tile regions to search */
        boxa = boxaCreate(0);
        if (top > bot)  /* search above */
            y0 = by - h_needed;
        else  /* search below */
            y0 = by + bh + mindist;
        for (j = 0; j < ncols; j++) {
            x = bx + j * tsize / 2;
            for (i = 0; i < ntiles; i++) {
                y = y0 + i * tsize / 2;
                box1 = boxCreate(x, y, tsize, tsize);
                boxaAddBox(boxa, box1, L_INSERT);
            }
        }
    }
    return boxa;
}

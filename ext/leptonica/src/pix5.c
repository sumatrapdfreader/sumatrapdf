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
 * \file pix5.c
 * <pre>
 *
 *    This file has these operations:
 *
 *      (1) Measurement of 1 bpp image properties
 *      (2) Extract rectangular regions
 *      (3) Clip to foreground
 *      (4) Extract pixel averages, reversals and variance along lines
 *      (5) Rank row and column transforms
 *
 *    Measurement of properties
 *           l_int32     pixaFindDimensions()
 *           l_int32     pixFindAreaPerimRatio()
 *           NUMA       *pixaFindPerimToAreaRatio()
 *           l_int32     pixFindPerimToAreaRatio()
 *           NUMA       *pixaFindPerimSizeRatio()
 *           l_int32     pixFindPerimSizeRatio()
 *           NUMA       *pixaFindAreaFraction()
 *           l_int32     pixFindAreaFraction()
 *           NUMA       *pixaFindAreaFractionMasked()
 *           l_int32     pixFindAreaFractionMasked()
 *           NUMA       *pixaFindWidthHeightRatio()
 *           NUMA       *pixaFindWidthHeightProduct()
 *           l_int32     pixFindOverlapFraction()
 *           BOXA       *pixFindRectangleComps()
 *           l_int32     pixConformsToRectangle()
 *
 *    Extract rectangular regions
 *           PIX        *pixExtractRectangularRegions()
 *           PIXA       *pixClipRectangles()
 *           PIX        *pixClipRectangle()
 *           PIX        *pixClipRectangleWithBorder()
 *           PIX        *pixClipMasked()
 *           l_int32     pixCropToMatch()
 *           PIX        *pixCropToSize()
 *           PIX        *pixResizeToMatch()
 *
 *    Select a connected component by size
 *           PIX        *pixSelectComponentBySize()
 *           PIX        *pixFilterComponentBySize()
 *
 *    Make special masks
 *           PIX        *pixMakeSymmetricMask()
 *           PIX        *pixMakeFrameMask()
 *
 *    Generate a covering of rectangles over connected components
 *           PIX        * pixMakeCoveringOfRectangles()
 *
 *    Fraction of Fg pixels under a mask
 *           l_int32     pixFractionFgInMask()
 *
 *    Clip to foreground
 *           PIX        *pixClipToForeground()
 *           l_int32     pixTestClipToForeground()
 *           l_int32     pixClipBoxToForeground()
 *           l_int32     pixScanForForeground()
 *           l_int32     pixClipBoxToEdges()
 *           l_int32     pixScanForEdge()
 *
 *    Extract pixel averages and reversals along lines
 *           NUMA       *pixExtractOnLine()
 *           l_float32   pixAverageOnLine()
 *           NUMA       *pixAverageIntensityProfile()
 *           NUMA       *pixReversalProfile()
 *
 *    Extract windowed variance along a line
 *           NUMA       *pixWindowedVarianceOnLine()
 *
 *    Extract min/max of pixel values near lines
 *           l_int32     pixMinMaxNearLine()
 *
 *    Rank row and column transforms
 *           PIX        *pixRankRowTransform()
 *           PIX        *pixRankColumnTransform()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>
#include "allheaders.h"

static const l_uint32 rmask32[] = {0x0,
    0x00000001, 0x00000003, 0x00000007, 0x0000000f,
    0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
    0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff,
    0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff,
    0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff,
    0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff,
    0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff,
    0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff};

#ifndef  NO_CONSOLE_IO
#define  DEBUG_EDGES         0
#endif  /* ~NO_CONSOLE_IO */


/*-------------------------------------------------------------*
 *                 Measurement of properties                   *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixaFindDimensions()
 *
 * \param[in]    pixa
 * \param[out]   pnaw [optional] numa of pix widths
 * \param[out]   pnah [optional] numa of pix heights
 * \return  0 if OK, 1 on error
 */
l_ok
pixaFindDimensions(PIXA   *pixa,
                   NUMA  **pnaw,
                   NUMA  **pnah)
{
l_int32  i, n, w, h;
PIX     *pixt;

    if (pnaw) *pnaw = NULL;
    if (pnah) *pnah = NULL;
    if (!pnaw && !pnah)
        return ERROR_INT("no output requested", __func__, 1);
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    n = pixaGetCount(pixa);
    if (pnaw) *pnaw = numaCreate(n);
    if (pnah) *pnah = numaCreate(n);
    for (i = 0; i < n; i++) {
        pixt = pixaGetPix(pixa, i, L_CLONE);
        pixGetDimensions(pixt, &w, &h, NULL);
        if (pnaw)
            numaAddNumber(*pnaw, w);
        if (pnah)
            numaAddNumber(*pnah, h);
        pixDestroy(&pixt);
    }
    return 0;
}


/*!
 * \brief   pixFindAreaPerimRatio()
 *
 * \param[in]    pixs    1 bpp
 * \param[in]    tab     [optional] pixel sum table, can be NULL
 * \param[out]   pfract  area/perimeter ratio
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The area is the number of fg pixels that are not on the
 *          boundary (i.e., are not 8-connected to a bg pixel), and the
 *          perimeter is the number of fg boundary pixels.  Returns
 *          0.0 if there are no fg pixels.
 *      (2) This function is retained because clients are using it.
 * </pre>
 */
l_ok
pixFindAreaPerimRatio(PIX        *pixs,
                      l_int32    *tab,
                      l_float32  *pfract)
{
l_int32  *tab8;
l_int32   nfg, nbound;
PIX      *pixt;

    if (!pfract)
        return ERROR_INT("&fract not defined", __func__, 1);
    *pfract = 0.0;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);

    if (!tab)
        tab8 = makePixelSumTab8();
    else
        tab8 = tab;

    pixt = pixErodeBrick(NULL, pixs, 3, 3);
    pixCountPixels(pixt, &nfg, tab8);
    if (nfg == 0) {
        pixDestroy(&pixt);
        if (!tab) LEPT_FREE(tab8);
        return 0;
    }
    pixXor(pixt, pixt, pixs);
    pixCountPixels(pixt, &nbound, tab8);
    *pfract = (l_float32)nfg / (l_float32)nbound;
    pixDestroy(&pixt);

    if (!tab) LEPT_FREE(tab8);
    return 0;
}


/*!
 * \brief   pixaFindPerimToAreaRatio()
 *
 * \param[in]    pixa   of 1 bpp pix
 * \return  na   of perimeter/arear ratio for each pix, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is typically used for a pixa consisting of
 *          1 bpp connected components.
 * </pre>
 */
NUMA *
pixaFindPerimToAreaRatio(PIXA  *pixa)
{
l_int32    i, n;
l_int32   *tab;
l_float32  fract;
NUMA      *na;
PIX       *pixt;

    if (!pixa)
        return (NUMA *)ERROR_PTR("pixa not defined", __func__, NULL);

    n = pixaGetCount(pixa);
    na = numaCreate(n);
    tab = makePixelSumTab8();
    for (i = 0; i < n; i++) {
        pixt = pixaGetPix(pixa, i, L_CLONE);
        pixFindPerimToAreaRatio(pixt, tab, &fract);
        numaAddNumber(na, fract);
        pixDestroy(&pixt);
    }
    LEPT_FREE(tab);
    return na;
}


/*!
 * \brief   pixFindPerimToAreaRatio()
 *
 * \param[in]    pixs    1 bpp
 * \param[in]    tab     [optional] pixel sum table, can be NULL
 * \param[out]   pfract  perimeter/area ratio
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The perimeter is the number of fg boundary pixels, and the
 *          area is the number of fg pixels.  This returns 0.0 if
 *          there are no fg pixels.
 *      (2) Unlike pixFindAreaPerimRatio(), this uses the full set of
 *          fg pixels for the area, and the ratio is taken in the opposite
 *          order.
 *      (3) This is typically used for a single connected component.
 *          This always has a value <= 1.0, and if the average distance
 *          of a fg pixel from the nearest bg pixel is d, this has
 *          a value ~1/d.
 * </pre>
 */
l_ok
pixFindPerimToAreaRatio(PIX        *pixs,
                        l_int32    *tab,
                        l_float32  *pfract)
{
l_int32  *tab8;
l_int32   nfg, nbound;
PIX      *pixt;

    if (!pfract)
        return ERROR_INT("&fract not defined", __func__, 1);
    *pfract = 0.0;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);

    if (!tab)
        tab8 = makePixelSumTab8();
    else
        tab8 = tab;

    pixCountPixels(pixs, &nfg, tab8);
    if (nfg == 0) {
        if (!tab) LEPT_FREE(tab8);
        return 0;
    }
    pixt = pixErodeBrick(NULL, pixs, 3, 3);
    pixXor(pixt, pixt, pixs);
    pixCountPixels(pixt, &nbound, tab8);
    *pfract = (l_float32)nbound / (l_float32)nfg;
    pixDestroy(&pixt);

    if (!tab) LEPT_FREE(tab8);
    return 0;
}


/*!
 * \brief   pixaFindPerimSizeRatio()
 *
 * \param[in]    pixa   of 1 bpp pix
 * \return  na   of fg perimeter/(2*(w+h)) ratio for each pix,
 *                  or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is typically used for a pixa consisting of
 *          1 bpp connected components.
 *      (2) This has a minimum value for a circle of pi/4; a value for
 *          a rectangle component of approx. 1.0; and a value much larger
 *          than 1.0 for a component with a highly irregular boundary.
 * </pre>
 */
NUMA *
pixaFindPerimSizeRatio(PIXA  *pixa)
{
l_int32    i, n;
l_int32   *tab;
l_float32  ratio;
NUMA      *na;
PIX       *pixt;

    if (!pixa)
        return (NUMA *)ERROR_PTR("pixa not defined", __func__, NULL);

    n = pixaGetCount(pixa);
    na = numaCreate(n);
    tab = makePixelSumTab8();
    for (i = 0; i < n; i++) {
        pixt = pixaGetPix(pixa, i, L_CLONE);
        pixFindPerimSizeRatio(pixt, tab, &ratio);
        numaAddNumber(na, ratio);
        pixDestroy(&pixt);
    }
    LEPT_FREE(tab);
    return na;
}


/*!
 * \brief   pixFindPerimSizeRatio()
 *
 * \param[in]    pixs    1 bpp
 * \param[in]    tab     [optional] pixel sum table, can be NULL
 * \param[out]   pratio  perimeter/size ratio
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) We take the 'size' as twice the sum of the width and
 *          height of pixs, and the perimeter is the number of fg
 *          boundary pixels.  We use the fg pixels of the boundary
 *          because the pix may be clipped to the boundary, so an
 *          erosion is required to count all boundary pixels.
 *      (2) This has a large value for dendritic, fractal-like components
 *          with highly irregular boundaries.
 *      (3) This is typically used for a single connected component.
 *          It has a value of about 1.0 for rectangular components with
 *          relatively smooth boundaries.
 * </pre>
 */
l_ok
pixFindPerimSizeRatio(PIX        *pixs,
                      l_int32    *tab,
                      l_float32  *pratio)
{
l_int32  *tab8;
l_int32   w, h, nbound;
PIX      *pixt;

    if (!pratio)
        return ERROR_INT("&ratio not defined", __func__, 1);
    *pratio = 0.0;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);

    if (!tab)
        tab8 = makePixelSumTab8();
    else
        tab8 = tab;

    pixt = pixErodeBrick(NULL, pixs, 3, 3);
    pixXor(pixt, pixt, pixs);
    pixCountPixels(pixt, &nbound, tab8);
    pixGetDimensions(pixs, &w, &h, NULL);
    *pratio = (0.5f * nbound) / (l_float32)(w + h);
    pixDestroy(&pixt);

    if (!tab) LEPT_FREE(tab8);
    return 0;
}


/*!
 * \brief   pixaFindAreaFraction()
 *
 * \param[in]    pixa   of 1 bpp pix
 * \return  na  of area fractions for each pix, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is typically used for a pixa consisting of
 *          1 bpp connected components.
 * </pre>
 */
NUMA *
pixaFindAreaFraction(PIXA  *pixa)
{
l_int32    i, n;
l_int32   *tab;
l_float32  fract;
NUMA      *na;
PIX       *pixt;

    if (!pixa)
        return (NUMA *)ERROR_PTR("pixa not defined", __func__, NULL);

    n = pixaGetCount(pixa);
    na = numaCreate(n);
    tab = makePixelSumTab8();
    for (i = 0; i < n; i++) {
        pixt = pixaGetPix(pixa, i, L_CLONE);
        pixFindAreaFraction(pixt, tab, &fract);
        numaAddNumber(na, fract);
        pixDestroy(&pixt);
    }
    LEPT_FREE(tab);
    return na;
}


/*!
 * \brief   pixFindAreaFraction()
 *
 * \param[in]    pixs    1 bpp
 * \param[in]    tab     [optional] pixel sum table, can be NULL
 * \param[out]   pfract  fg area/size ratio
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This finds the ratio of the number of fg pixels to the
 *          size of the pix (w * h).  It is typically used for a
 *          single connected component.
 * </pre>
 */
l_ok
pixFindAreaFraction(PIX        *pixs,
                    l_int32    *tab,
                    l_float32  *pfract)
{
l_int32   w, h, sum;
l_int32  *tab8;

    if (!pfract)
        return ERROR_INT("&fract not defined", __func__, 1);
    *pfract = 0.0;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);

    if (!tab)
        tab8 = makePixelSumTab8();
    else
        tab8 = tab;
    pixGetDimensions(pixs, &w, &h, NULL);
    pixCountPixels(pixs, &sum, tab8);
    *pfract = (l_float32)sum / (l_float32)(w * h);

    if (!tab) LEPT_FREE(tab8);
    return 0;
}


/*!
 * \brief   pixaFindAreaFractionMasked()
 *
 * \param[in]    pixa    of 1 bpp pix
 * \param[in]    pixm    mask image
 * \param[in]    debug   1 for output, 0 to suppress
 * \return  na of ratio masked/total fractions for each pix,
 *                  or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is typically used for a pixa consisting of
 *          1 bpp connected components, which has an associated
 *          boxa giving the location of the components relative
 *          to the mask origin.
 *      (2) The debug flag displays in green and red the masked and
 *          unmasked parts of the image from which pixa was derived.
 * </pre>
 */
NUMA *
pixaFindAreaFractionMasked(PIXA    *pixa,
                           PIX     *pixm,
                           l_int32  debug)
{
l_int32    i, n, full;
l_int32   *tab;
l_float32  fract;
BOX       *box;
NUMA      *na;
PIX       *pix;

    if (!pixa)
        return (NUMA *)ERROR_PTR("pixa not defined", __func__, NULL);
    if (!pixm || pixGetDepth(pixm) != 1)
        return (NUMA *)ERROR_PTR("pixm undefined or not 1 bpp", __func__, NULL);

    n = pixaGetCount(pixa);
    na = numaCreate(n);
    tab = makePixelSumTab8();
    pixaIsFull(pixa, NULL, &full);  /* check boxa */
    box = NULL;
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixa, i, L_CLONE);
        if (full)
            box = pixaGetBox(pixa, i, L_CLONE);
        pixFindAreaFractionMasked(pix, box, pixm, tab, &fract);
        numaAddNumber(na, fract);
        boxDestroy(&box);
        pixDestroy(&pix);
    }
    LEPT_FREE(tab);

    if (debug) {
        l_int32  w, h;
        PIX     *pix1, *pix2;
        pixGetDimensions(pixm, &w, &h, NULL);
        pix1 = pixaDisplay(pixa, w, h);  /* recover original image */
        pix2 = pixCreate(w, h, 8);  /* make an 8 bpp white image ... */
        pixSetColormap(pix2, pixcmapCreate(8));  /* that's cmapped ... */
        pixSetBlackOrWhite(pix2, L_SET_WHITE);  /* and init to white */
        pixSetMaskedCmap(pix2, pix1, 0, 0, 255, 0, 0);  /* color all fg red */
        pixRasterop(pix1, 0, 0, w, h, PIX_MASK, pixm, 0, 0);
        pixSetMaskedCmap(pix2, pix1, 0, 0, 0, 255, 0);  /* turn masked green */
        pixDisplay(pix2, 100, 100);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }

    return na;
}


/*!
 * \brief   pixFindAreaFractionMasked()
 *
 * \param[in]    pixs    1 bpp, typically a single component
 * \param[in]    box     [optional] for pixs relative to pixm
 * \param[in]    pixm    1 bpp mask, typically over the entire image from
 *                       which the component pixs was extracted
 * \param[in]    tab     [optional] pixel sum table, can be NULL
 * \param[out]   pfract  fg area/size ratio
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This finds the ratio of the number of masked fg pixels
 *          in pixs to the total number of fg pixels in pixs.
 *          It is typically used for a single connected component.
 *          If there are no fg pixels, this returns a ratio of 0.0.
 *      (2) The box gives the location of the pix relative to that
 *          of the UL corner of the mask.  Therefore, the rasterop
 *          is performed with the pix translated to its location
 *          (x, y) in the mask before ANDing.
 *          If box == NULL, the UL corners of pixs and pixm are aligned.
 * </pre>
 */
l_ok
pixFindAreaFractionMasked(PIX        *pixs,
                          BOX        *box,
                          PIX        *pixm,
                          l_int32    *tab,
                          l_float32  *pfract)
{
l_int32   x, y, w, h, sum, masksum;
l_int32  *tab8;
PIX      *pix1;

    if (!pfract)
        return ERROR_INT("&fract not defined", __func__, 1);
    *pfract = 0.0;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);
    if (!pixm || pixGetDepth(pixm) != 1)
        return ERROR_INT("pixm not defined or not 1 bpp", __func__, 1);

    if (!tab)
        tab8 = makePixelSumTab8();
    else
        tab8 = tab;
    x = y = 0;
    if (box)
        boxGetGeometry(box, &x, &y, NULL, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);

    pix1 = pixCopy(NULL, pixs);
    pixRasterop(pix1, 0, 0, w, h, PIX_MASK, pixm, x, y);
    pixCountPixels(pixs, &sum, tab8);
    if (sum == 0) {
        pixDestroy(&pix1);
        if (!tab) LEPT_FREE(tab8);
        return 0;
    }
    pixCountPixels(pix1, &masksum, tab8);
    *pfract = (l_float32)masksum / (l_float32)sum;

    if (!tab) LEPT_FREE(tab8);
    pixDestroy(&pix1);
    return 0;
}


/*!
 * \brief   pixaFindWidthHeightRatio()
 *
 * \param[in]    pixa   of 1 bpp pix
 * \return  na of width/height ratios for each pix, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is typically used for a pixa consisting of
 *          1 bpp connected components.
 * </pre>
 */
NUMA *
pixaFindWidthHeightRatio(PIXA  *pixa)
{
l_int32  i, n, w, h;
NUMA    *na;
PIX     *pixt;

    if (!pixa)
        return (NUMA *)ERROR_PTR("pixa not defined", __func__, NULL);

    n = pixaGetCount(pixa);
    na = numaCreate(n);
    for (i = 0; i < n; i++) {
        pixt = pixaGetPix(pixa, i, L_CLONE);
        pixGetDimensions(pixt, &w, &h, NULL);
        numaAddNumber(na, (l_float32)w / (l_float32)h);
        pixDestroy(&pixt);
    }
    return na;
}


/*!
 * \brief   pixaFindWidthHeightProduct()
 *
 * \param[in]    pixa   of 1 bpp pix
 * \return  na of width*height products for each pix, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is typically used for a pixa consisting of
 *          1 bpp connected components.
 * </pre>
 */
NUMA *
pixaFindWidthHeightProduct(PIXA  *pixa)
{
l_int32  i, n, w, h;
NUMA    *na;
PIX     *pixt;

    if (!pixa)
        return (NUMA *)ERROR_PTR("pixa not defined", __func__, NULL);

    n = pixaGetCount(pixa);
    na = numaCreate(n);
    for (i = 0; i < n; i++) {
        pixt = pixaGetPix(pixa, i, L_CLONE);
        pixGetDimensions(pixt, &w, &h, NULL);
        numaAddNumber(na, w * h);
        pixDestroy(&pixt);
    }
    return na;
}


/*!
 * \brief   pixFindOverlapFraction()
 *
 * \param[in]    pixs1, pixs2   1 bpp
 * \param[in]    x2, y2         location in pixs1 of UL corner of pixs2
 * \param[in]    tab            [optional] pixel sum table, can be null
 * \param[out]   pratio         ratio fg intersection to fg union
 * \param[out]   pnoverlap      [optional] number of overlapping pixels
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The UL corner of pixs2 is placed at (x2, y2) in pixs1.
 *      (2) This measure is similar to the correlation.
 * </pre>
 */
l_ok
pixFindOverlapFraction(PIX        *pixs1,
                       PIX        *pixs2,
                       l_int32     x2,
                       l_int32     y2,
                       l_int32    *tab,
                       l_float32  *pratio,
                       l_int32    *pnoverlap)
{
l_int32  *tab8;
l_int32   w, h, nintersect, nunion;
PIX      *pixt;

    if (pnoverlap) *pnoverlap = 0;
    if (!pratio)
        return ERROR_INT("&ratio not defined", __func__, 1);
    *pratio = 0.0;
    if (!pixs1 || pixGetDepth(pixs1) != 1)
        return ERROR_INT("pixs1 not defined or not 1 bpp", __func__, 1);
    if (!pixs2 || pixGetDepth(pixs2) != 1)
        return ERROR_INT("pixs2 not defined or not 1 bpp", __func__, 1);

    if (!tab)
        tab8 = makePixelSumTab8();
    else
        tab8 = tab;

    pixGetDimensions(pixs2, &w, &h, NULL);
    pixt = pixCopy(NULL, pixs1);
    pixRasterop(pixt, x2, y2, w, h, PIX_MASK, pixs2, 0, 0);  /* AND */
    pixCountPixels(pixt, &nintersect, tab8);
    if (pnoverlap)
        *pnoverlap = nintersect;
    pixCopy(pixt, pixs1);
    pixRasterop(pixt, x2, y2, w, h, PIX_PAINT, pixs2, 0, 0);  /* OR */
    pixCountPixels(pixt, &nunion, tab8);
    if (!tab) LEPT_FREE(tab8);
    pixDestroy(&pixt);

    if (nunion > 0)
        *pratio = (l_float32)nintersect / (l_float32)nunion;
    return 0;
}


/*!
 * \brief   pixFindRectangleComps()
 *
 * \param[in]    pixs        1 bpp
 * \param[in]    dist        max distance allowed between bounding box
 *                           and nearest foreground pixel within it
 * \param[in]    minw, minh  minimum size in each direction as a requirement
 *                           for a conforming rectangle
 * \return  boxa of components that conform, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This applies the function pixConformsToRectangle() to
 *          each 8-c.c. in pixs, and returns a boxa containing the
 *          regions of all components that are conforming.
 *      (2) Conforming components must satisfy both the size constraint
 *          given by %minsize and the slop in conforming to a rectangle
 *          determined by %dist.
 * </pre>
 */
BOXA *
pixFindRectangleComps(PIX     *pixs,
                      l_int32  dist,
                      l_int32  minw,
                      l_int32  minh)
{
l_int32  w, h, i, n, conforms;
BOX     *box;
BOXA    *boxa, *boxad;
PIX     *pix;
PIXA    *pixa;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (BOXA *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (dist < 0)
        return (BOXA *)ERROR_PTR("dist must be >= 0", __func__, NULL);
    if (minw <= 2 * dist && minh <= 2 * dist)
        return (BOXA *)ERROR_PTR("invalid parameters", __func__, NULL);

    boxa = pixConnComp(pixs, &pixa, 8);
    boxad = boxaCreate(0);
    n = pixaGetCount(pixa);
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixa, i, L_CLONE);
        pixGetDimensions(pix, &w, &h, NULL);
        if (w < minw || h < minh) {
            pixDestroy(&pix);
            continue;
        }
        pixConformsToRectangle(pix, NULL, dist, &conforms);
        if (conforms) {
            box = boxaGetBox(boxa, i, L_COPY);
            boxaAddBox(boxad, box, L_INSERT);
        }
        pixDestroy(&pix);
    }
    boxaDestroy(&boxa);
    pixaDestroy(&pixa);
    return boxad;
}


/*!
 * \brief   pixConformsToRectangle()
 *
 * \param[in]    pixs       1 bpp
 * \param[in]    box        [optional] if null, use the entire pixs
 * \param[in]    dist       max distance allowed between bounding box and
 *                          nearest foreground pixel within it
 * \param[out]   pconforms  0 (false) if not conforming;
 *                          1 (true) if conforming
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) There are several ways to test if a connected component has
 *          an essentially rectangular boundary, such as:
 *           a. Fraction of fill into the bounding box
 *           b. Max-min distance of fg pixel from periphery of bounding box
 *           c. Max depth of bg intrusions into component within bounding box
 *          The weakness of (a) is that it is highly sensitive to holes
 *          within the c.c.  The weakness of (b) is that it can have
 *          arbitrarily large intrusions into the c.c.  Method (c) tests
 *          the integrity of the outer boundary of the c.c., with respect
 *          to the enclosing bounding box, so we use it.
 *      (2) This tests if the connected component within the box conforms
 *          to the box at all points on the periphery within %dist.
 *          Inside, at a distance from the box boundary that is greater
 *          than %dist, we don't care about the pixels in the c.c.
 *      (3) We can think of the conforming condition as follows:
 *          No pixel inside a distance %dist from the boundary
 *          can connect to the boundary through a path through the bg.
 *          To implement this, we need to do a flood fill.  We can go
 *          either from inside toward the boundary, or the other direction.
 *          It's easiest to fill from the boundary, and then verify that
 *          there are no filled pixels farther than %dist from the boundary.
 * </pre>
 */
l_ok
pixConformsToRectangle(PIX      *pixs,
                       BOX      *box,
                       l_int32   dist,
                       l_int32  *pconforms)
{
l_int32  w, h, empty;
PIX     *pix1, *pix2;

    if (!pconforms)
        return ERROR_INT("&conforms not defined", __func__, 1);
    *pconforms = 0;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);
    if (dist < 0)
        return ERROR_INT("dist must be >= 0", __func__, 1);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (w <= 2 * dist || h <= 2 * dist) {
        L_WARNING("automatic conformation: distance too large\n", __func__);
        *pconforms = 1;
        return 0;
    }

        /* Extract the region, if necessary */
    if (box)
        pix1 = pixClipRectangle(pixs, box, NULL);
    else
        pix1 = pixCopy(NULL, pixs);

        /* Invert and fill from the boundary into the interior.
         * Because we're considering the connected component in an
         * 8-connected sense, we do the background filling as 4 c.c. */
    pixInvert(pix1, pix1);
    pix2 = pixExtractBorderConnComps(pix1, 4);

        /* Mask out all pixels within a distance %dist from the box
         * boundary.  Any remaining pixels are from filling that goes
         * more than %dist from the boundary.  If no pixels remain,
         * the component conforms to the bounding rectangle within
         * a distance %dist. */
    pixSetOrClearBorder(pix2, dist, dist, dist, dist, PIX_CLR);
    pixZero(pix2, &empty);
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    *pconforms = (empty) ? 1 : 0;
    return 0;
}


/*-----------------------------------------------------------------------*
 *                      Extract rectangular regions                      *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixExtractRectangularRegions()
 *
 * \param[in]    pixs
 * \param[in]    boxa  regions to extract
 * \return  pix  with extracted regions, or NULL on error
 *
 * <pre>
 * Notes:
 *     (1) The returned pix has the rectangular regions clipped from
 *         the input pixs.
 *     (2) We could equally well do this operation using a mask of 1's over
 *         the regions determined by the boxa:
 *           pix1 = pixCreateTemplate(pixs);
 *           pixMaskBoxa(pix1, pix1, boxa, L_SET_PIXELS);
 *           pixAnd(pix1, pix1, pixs);
 * </pre>
 */
PIX *
pixExtractRectangularRegions(PIX   *pixs,
                             BOXA  *boxa)
{
l_int32  w, h;
PIX     *pix1;
PIXA    *pixa1;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (!boxa)
        return (PIX *)ERROR_PTR("boxa not defined", __func__, NULL);

    if ((pixa1 = pixClipRectangles(pixs, boxa)) == NULL)
        return (PIX *)ERROR_PTR("pixa1 not made", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    pix1 = pixaDisplay(pixa1, w, h);
    pixaDestroy(&pixa1);
    return pix1;
}


/*!
 * \brief   pixClipRectangles()
 *
 * \param[in]    pixs
 * \param[in]    boxa  requested clipping regions
 * \return  pixa consisting of requested regions, or NULL on error
 *
 * <pre>
 * Notes:
 *     (1) The boxa in the returned pixa has the regions clipped from
 *         the input pixs.
 * </pre>
 */
PIXA *
pixClipRectangles(PIX   *pixs,
                  BOXA  *boxa)
{
l_int32  i, n;
BOX     *box, *boxc;
PIX     *pix;
PIXA    *pixa;

    if (!pixs)
        return (PIXA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (!boxa)
        return (PIXA *)ERROR_PTR("boxa not defined", __func__, NULL);

    n = boxaGetCount(boxa);
    pixa = pixaCreate(n);
    for (i = 0; i < n; i++) {
        box = boxaGetBox(boxa, i, L_CLONE);
        pix = pixClipRectangle(pixs, box, &boxc);
        pixaAddPix(pixa, pix, L_INSERT);
        pixaAddBox(pixa, boxc, L_INSERT);
        boxDestroy(&box);
    }

    return pixa;
}


/*!
 * \brief   pixClipRectangle()
 *
 * \param[in]    pixs
 * \param[in]    box    requested clipping region; const
 * \param[out]   pboxc  [optional] actual box of clipped region
 * \return  clipped pix, or NULL on error or if rectangle
 *              doesn't intersect pixs
 *
 * <pre>
 * Notes:
 *
 *  This should be simple, but there are choices to be made.
 *  The box is defined relative to the pix coordinates.  However,
 *  if the box is not contained within the pix, we have two choices:
 *
 *      (1) clip the box to the pix
 *      (2) make a new pix equal to the full box dimensions,
 *          but let rasterop do the clipping and positioning
 *          of the src with respect to the dest
 *
 *  Choice (2) immediately brings up the problem of what pixel values
 *  to use that were not taken from the src.  For example, on a grayscale
 *  image, do you want the pixels not taken from the src to be black
 *  or white or something else?  To implement choice 2, one needs to
 *  specify the color of these extra pixels.
 *
 *  So we adopt (1), and clip the box first, if necessary,
 *  before making the dest pix and doing the rasterop.  But there
 *  is another issue to consider.  If you want to paste the
 *  clipped pix back into pixs, it must be properly aligned, and
 *  it is necessary to use the clipped box for alignment.
 *  Accordingly, this function has a third (optional) argument, which is
 *  the input box clipped to the src pix.
 * </pre>
 */
PIX *
pixClipRectangle(PIX   *pixs,
                 BOX   *box,
                 BOX  **pboxc)
{
l_int32  w, h, d, bx, by, bw, bh;
BOX     *boxc;
PIX     *pixd;

    if (pboxc) *pboxc = NULL;
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (!box)
        return (PIX *)ERROR_PTR("box not defined", __func__, NULL);

        /* Clip the input box to the pix */
    pixGetDimensions(pixs, &w, &h, &d);
    if ((boxc = boxClipToRectangle(box, w, h)) == NULL) {
        L_WARNING("box doesn't overlap pix\n", __func__);
        return NULL;
    }
    boxGetGeometry(boxc, &bx, &by, &bw, &bh);

        /* Extract the block */
    if ((pixd = pixCreate(bw, bh, d)) == NULL) {
        boxDestroy(&boxc);
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    }
    pixCopyResolution(pixd, pixs);
    pixCopyColormap(pixd, pixs);
    pixCopyText(pixd, pixs);
    pixRasterop(pixd, 0, 0, bw, bh, PIX_SRC, pixs, bx, by);

    if (pboxc)
        *pboxc = boxc;
    else
        boxDestroy(&boxc);

    return pixd;
}


/*!
 * \brief   pixClipRectangleWithBorder()
 *
 * \param[in]    pixs
 * \param[in]    box       requested clipping region; const
 * \param[in]    maxbord   maximum amount of border to include
 * \param[out]   pboxn     box in coordinates of returned pix
 * \return  under-clipped pix, or NULL on error or if rectangle
 *              doesn't intersect pixs
 *
 * <pre>
 * Notes:
 *      (1) This underclips by an amount determined by the minimum of
 *          %maxbord and the amount of border that can be included
 *          equally on all 4 sides.
 *      (2) If part of the rectangle lies outside the pix, no border
 *          is included on any side.
 * </pre>
 */
PIX *
pixClipRectangleWithBorder(PIX     *pixs,
                           BOX     *box,
                           l_int32  maxbord,
                           BOX    **pboxn)
{
l_int32  w, h, bx, by, bw, bh, bord;
BOX     *box1;
PIX     *pix1;

    if (!pboxn)
        return (PIX *)ERROR_PTR("&boxn not defined", __func__, NULL);
    *pboxn = NULL;
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (!box)
        return (PIX *)ERROR_PTR("box not defined", __func__, NULL);

        /* Determine the border width */
    pixGetDimensions(pixs, &w, &h, NULL);
    boxGetGeometry(box, &bx, &by, &bw, &bh);
    bord = L_MIN(bx, by);
    bord = L_MIN(bord, w - bx - bw);
    bord = L_MIN(bord, h - by - bh);
    bord = L_MIN(bord, maxbord);

    if (bord <= 0) {  /* standard clipping */
        pix1 = pixClipRectangle(pixs, box, NULL);
        pixGetDimensions(pix1, &w, &h, NULL);
        *pboxn = boxCreate(0, 0, w, h);
        return pix1;
    }

        /* There is a positive border */
    box1 = boxAdjustSides(NULL, box, -bord, bord, -bord, bord);
    pix1 = pixClipRectangle(pixs, box1, NULL);
    boxDestroy(&box1);
    *pboxn = boxCreate(bord, bord, bw, bh);
    return pix1;
}


/*!
 * \brief   pixClipMasked()
 *
 * \param[in]    pixs    1, 2, 4, 8, 16, 32 bpp; colormap ok
 * \param[in]    pixm    clipping mask, 1 bpp
 * \param[in]    x, y    origin of clipping mask relative to pixs
 * \param[in]    outval  val to use for pixels that are outside the mask
 * \return  pixd, clipped pix or NULL on error or if pixm doesn't
 *              intersect pixs
 *
 * <pre>
 * Notes:
 *      (1) If pixs has a colormap, it is preserved in pixd.
 *      (2) The depth of pixd is the same as that of pixs.
 *      (3) If the depth of pixs is 1, use %outval = 0 for white background
 *          and 1 for black; otherwise, use the max value for white
 *          and 0 for black.  If pixs has a colormap, the max value for
 *          %outval is 0xffffffff; otherwise, it is 2^d - 1.
 *      (4) When using 1 bpp pixs, this is a simple clip and
 *          blend operation.  For example, if both pix1 and pix2 are
 *          black text on white background, and you want to OR the
 *          fg on the two images, let pixm be the inverse of pix2.
 *          Then the operation takes all of pix1 that's in the bg of
 *          pix2, and for the remainder (which are the pixels
 *          corresponding to the fg of the pix2), paint them black
 *          (1) in pix1.  The function call looks like
 *             pixClipMasked(pix2, pixInvert(pix1, pix1), x, y, 1);
 * </pre>
 */
PIX *
pixClipMasked(PIX      *pixs,
              PIX      *pixm,
              l_int32   x,
              l_int32   y,
              l_uint32  outval)
{
l_int32   wm, hm, index, rval, gval, bval;
l_uint32  pixel;
BOX      *box;
PIX      *pixmi, *pixd;
PIXCMAP  *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (!pixm || pixGetDepth(pixm) != 1)
        return (PIX *)ERROR_PTR("pixm undefined or not 1 bpp", __func__, NULL);

        /* Clip out the region specified by pixm and (x,y) */
    pixGetDimensions(pixm, &wm, &hm, NULL);
    box = boxCreate(x, y, wm, hm);
    pixd = pixClipRectangle(pixs, box, NULL);

        /* Paint 'outval' (or something close to it if cmapped) through
         * the pixels not masked by pixm */
    cmap = pixGetColormap(pixd);
    pixmi = pixInvert(NULL, pixm);
    if (cmap) {
        extractRGBValues(outval, &rval, &gval, &bval);
        pixcmapGetNearestIndex(cmap, rval, gval, bval, &index);
        pixcmapGetColor(cmap, index, &rval, &gval, &bval);
        composeRGBPixel(rval, gval, bval, &pixel);
        pixPaintThroughMask(pixd, pixmi, 0, 0, pixel);
    } else {
        pixPaintThroughMask(pixd, pixmi, 0, 0, outval);
    }

    boxDestroy(&box);
    pixDestroy(&pixmi);
    return pixd;
}


/*!
 * \brief   pixCropToMatch()
 *
 * \param[in]    pixs1   any depth, colormap OK
 * \param[in]    pixs2   any depth, colormap OK
 * \param[out]   ppixd1  may be a clone
 * \param[out]   ppixd2  may be a clone
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This resizes pixs1 and/or pixs2 by cropping at the right
 *          and bottom, so that they're the same size.
 *      (2) If a pix doesn't need to be cropped, a clone is returned.
 *      (3) Note: the images are implicitly aligned to the UL corner.
 * </pre>
 */
l_ok
pixCropToMatch(PIX   *pixs1,
               PIX   *pixs2,
               PIX  **ppixd1,
               PIX  **ppixd2)
{
l_int32  w1, h1, w2, h2, w, h;

    if (!ppixd1 || !ppixd2)
        return ERROR_INT("&pixd1 and &pixd2 not both defined", __func__, 1);
    *ppixd1 = *ppixd2 = NULL;
    if (!pixs1 || !pixs2)
        return ERROR_INT("pixs1 and pixs2 not defined", __func__, 1);

    pixGetDimensions(pixs1, &w1, &h1, NULL);
    pixGetDimensions(pixs2, &w2, &h2, NULL);
    w = L_MIN(w1, w2);
    h = L_MIN(h1, h2);

    *ppixd1 = pixCropToSize(pixs1, w, h);
    *ppixd2 = pixCropToSize(pixs2, w, h);
    if (*ppixd1 == NULL || *ppixd2 == NULL)
        return ERROR_INT("cropped image failure", __func__, 1);
    return 0;
}


/*!
 * \brief   pixCropToSize()
 *
 * \param[in]    pixs   any depth, colormap OK
 * \param[in]    w, h   max dimensions of cropped image
 * \return  pixd cropped if necessary or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) If either w or h is smaller than the corresponding dimension
 *          of pixs, this returns a cropped image; otherwise it returns
 *          a clone of pixs.
 * </pre>
 */
PIX *
pixCropToSize(PIX     *pixs,
              l_int32  w,
              l_int32  h)
{
l_int32  ws, hs, wd, hd, d;
PIX     *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, &d);
    if (ws <= w && hs <= h)  /* no cropping necessary */
        return pixClone(pixs);

    wd = L_MIN(ws, w);
    hd = L_MIN(hs, h);
    if ((pixd = pixCreate(wd, hd, d)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyColormap(pixd, pixs);
    pixCopyText(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    pixRasterop(pixd, 0, 0, wd, hd, PIX_SRC, pixs, 0, 0);
    return pixd;
}


/*!
 * \brief   pixResizeToMatch()
 *
 * \param[in]    pixs   1, 2, 4, 8, 16, 32 bpp; colormap ok
 * \param[in]    pixt   can be null; we use only the size
 * \param[in]    w, h   ignored if pixt is defined
 * \return  pixd resized to match or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This resizes pixs to make pixd, without scaling, by either
 *          cropping or extending separately in both width and height.
 *          Extension is done by replicating the last row or column.
 *          This is useful in a situation where, due to scaling
 *          operations, two images that are expected to be the
 *          same size can differ slightly in each dimension.
 *      (2) You can use either an existing pixt or specify
 *          both %w and %h.  If pixt is defined, the values
 *          in %w and %h are ignored.
 *      (3) If pixt is larger than pixs (or if w and/or d is larger
 *          than the dimension of pixs, replicate the outer row and
 *          column of pixels in pixs into pixd.
 * </pre>
 */
PIX *
pixResizeToMatch(PIX     *pixs,
                 PIX     *pixt,
                 l_int32  w,
                 l_int32  h)
{
l_int32  i, j, ws, hs, d;
PIX     *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (!pixt && (w <= 0 || h <= 0))
        return (PIX *)ERROR_PTR("both w and h not > 0", __func__, NULL);

    if (pixt)  /* redefine w, h */
        pixGetDimensions(pixt, &w, &h, NULL);
    pixGetDimensions(pixs, &ws, &hs, &d);
    if (ws == w && hs == h)
        return pixCopy(NULL, pixs);

    if ((pixd = pixCreate(w, h, d)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyColormap(pixd, pixs);
    pixCopyText(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    pixRasterop(pixd, 0, 0, ws, hs, PIX_SRC, pixs, 0, 0);
    if (ws >= w && hs >= h)
        return pixd;

        /* Replicate the last column and then the last row */
    if (ws < w) {
        for (j = ws; j < w; j++)
            pixRasterop(pixd, j, 0, 1, h, PIX_SRC, pixd, ws - 1, 0);
    }
    if (hs < h) {
        for (i = hs; i < h; i++)
            pixRasterop(pixd, 0, i, w, 1, PIX_SRC, pixd, 0, hs - 1);
    }

    return pixd;
}


/*---------------------------------------------------------------------*
 *                Select a connected component by size                 *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixSelectComponentBySize()
 *
 * \param[in]    pixs          1 bpp
 * \param[in]    rankorder     in decreasing size: 0 for largest.
 * \param[in]    type          L_SELECT_BY_WIDTH, L_SELECT_BY_HEIGHT,
 *                             L_SELECT_BY_MAX_DIMENSION,
 *                             L_SELECT_BY_AREA, L_SELECT_BY_PERIMETER
 * \param[in]    connectivity  4 or 8
 * \param[out]   pbox          [optional] location of returned component
 * \return  pix of rank order connected component, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) This selects the Nth largest connected component, based on
 *          the selection type and connectivity.
 *      (2) Note that %rankorder is an integer.  Use %rankorder = 0 for
 *          the largest component and %rankorder = -1 for the smallest.
 *          If %rankorder >= number of components, select the smallest.
 */
PIX *
pixSelectComponentBySize(PIX     *pixs,
                         l_int32  rankorder,
                         l_int32  type,
                         l_int32  connectivity,
                         BOX    **pbox)
{
l_int32  n, empty, sorttype, index;
BOXA    *boxa1;
NUMA    *naindex;
PIX     *pixd;
PIXA    *pixa1, *pixa2;

    if (pbox) *pbox = NULL;
    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (type == L_SELECT_BY_WIDTH)
        sorttype = L_SORT_BY_WIDTH;
    else if (type == L_SELECT_BY_HEIGHT)
        sorttype = L_SORT_BY_HEIGHT;
    else if (type == L_SELECT_BY_MAX_DIMENSION)
        sorttype = L_SORT_BY_MAX_DIMENSION;
    else if (type == L_SELECT_BY_AREA)
        sorttype = L_SORT_BY_AREA;
    else if (type == L_SELECT_BY_PERIMETER)
        sorttype = L_SORT_BY_PERIMETER;
    else
        return (PIX *)ERROR_PTR("invalid selection type", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);
    pixZero(pixs, &empty);
    if (empty)
        return (PIX *)ERROR_PTR("no foreground pixels", __func__, NULL);

    boxa1 = pixConnComp(pixs, &pixa1, connectivity);
    n = boxaGetCount(boxa1);
    if (rankorder < 0 || rankorder >= n)
        rankorder = n - 1;  /* smallest */
    pixa2 = pixaSort(pixa1, sorttype, L_SORT_DECREASING, &naindex, L_CLONE);
    pixd = pixaGetPix(pixa2, rankorder, L_COPY);
    if (pbox) {
        numaGetIValue(naindex, rankorder, &index);
        *pbox = boxaGetBox(boxa1, index, L_COPY);
    }

    numaDestroy(&naindex);
    boxaDestroy(&boxa1);
    pixaDestroy(&pixa1);
    pixaDestroy(&pixa2);
    return pixd;
}


/*!
 * \brief   pixFilterComponentBySize()
 *
 * \param[in]    pixs          1 bpp
 * \param[in]    rankorder     in decreasing size: 0 for largest.
 * \param[in]    type          L_SELECT_BY_WIDTH, L_SELECT_BY_HEIGHT,
 *                             L_SELECT_BY_MAX_DIMENSION,
 *                             L_SELECT_BY_AREA, L_SELECT_BY_PERIMETER
 * \param[in]    connectivity  4 or 8
 * \param[out]   pbox          [optional] location of returned component
 * \return  pix with all other components removed, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) See notes in pixSelectComponentBySize().
 *      (2) This returns a copy of %pixs, with all components removed
 *          except for the selected one.
 */
PIX *
pixFilterComponentBySize(PIX     *pixs,
                         l_int32  rankorder,
                         l_int32  type,
                         l_int32  connectivity,
                         BOX    **pbox)
{
l_int32  x, y, w, h;
BOX     *box;
PIX     *pix1, *pix2;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);

    pix1 = pixSelectComponentBySize(pixs, rankorder, type, connectivity, &box);
    if (!pix1) {
        boxDestroy(&box);
        return (PIX *)ERROR_PTR("pix1 not made", __func__, NULL);
    }

        /* Put the selected component in a new pix at the same
         * location as it had in %pixs */
    boxGetGeometry(box, &x, &y, &w, &h);
    pix2 = pixCreateTemplate(pixs);
    pixRasterop(pix2, x, y, w, h, PIX_SRC, pix1, 0, 0);
    if (pbox)
        *pbox = box;
    else
        boxDestroy(&box);
    pixDestroy(&pix1);
    return pix2;
}


/*---------------------------------------------------------------------*
 *                         Make special masks                          *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixMakeSymmetricMask()
 *
 * \param[in]    w, h    dimensions of output 1 bpp pix
 * \param[in]    hf      horizontal fraction of half-width
 * \param[in]    vf      vertical fraction of half-height
 * \param[in]    type    L_USE_INNER, L_USE_OUTER
 * \return  pixd 1 bpp, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) This is a convenience function for generating masks with
 *          horizontal and vertical reflection symmetry, over either
 *          the inner or outer parts of an image.
 *      (2) Using L_USE_INNER to generate a mask over the inner part
 *          of the image, the mask is a solid rectangle, and the fractions
 *          describe the distance between the boundary of the image and
 *          the rectangle boundary.  For example, with hf == vf == 0.0,
 *          the mask covers the full image.
 *      (3) Using L_USE_OUTER to generate a mask over an outer frame
 *          of the image, the mask touches the boundary of the image,
 *          and the fractions describe the location of the inner
 *          boundary of the frame.  For example, with hf == vf == 1.0,
 *          the inner boundary is at the center of the image, so the
 *          mask covers the full image.
 *      (4) More examples:
 *           * mask covering the inner 70%: hf = vf = 0.3, type = L_USE_INNER
 *           * frame covering the outer 30%: hf = vf = 0.3, type = L_USE_OUTER
 * </pre>
 */
PIX *
pixMakeSymmetricMask(l_int32    w,
                     l_int32    h,
                     l_float32  hf,
                     l_float32  vf,
                     l_int32    type)
{
    if (w <= 0 || h <= 0)
        return (PIX *)ERROR_PTR("mask size 0", __func__, NULL);
    if (hf < 0.0 || hf > 1.0)
        return (PIX *)ERROR_PTR("invalid horiz fractions", __func__, NULL);
    if (vf < 0.0 || vf > 1.0)
        return (PIX *)ERROR_PTR("invalid vert fractions", __func__, NULL);

    if (type == L_USE_INNER)
        return pixMakeFrameMask(w, h, hf, 1.0, vf, 1.0);
    else if (type == L_USE_OUTER)
        return pixMakeFrameMask(w, h, 0.0, hf, 0.0, vf);
    else
        return (PIX *)ERROR_PTR("invalid type", __func__, NULL);
}


/*!
 * \brief   pixMakeFrameMask()
 *
 * \param[in]    w, h  dimensions of output 1 bpp pix
 * \param[in]    hf1   horizontal fraction of half-width at outer frame bdry
 * \param[in]    hf2   horizontal fraction of half-width at inner frame bdry
 * \param[in]    vf1   vertical fraction of half-width at outer frame bdry
 * \param[in]    vf2   vertical fraction of half-width at inner frame bdry
 * \return  pixd 1 bpp, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) This makes an arbitrary 1-component mask with a centered fg
 *          frame, which can have both an inner and an outer boundary.
 *          All input fractional distances are measured from the image
 *          border to the frame boundary, in units of the image half-width
 *          for hf1 and hf2 and the image half-height for vf1 and vf2.
 *          The distances to the outer frame boundary are given by hf1
 *          and vf1; to the inner frame boundary, by hf2 and vf2.
 *          Input fractions are thus in [0.0 ... 1.0], with hf1 <= hf2
 *          and vf1 <= vf2.  Horizontal and vertical frame widths are
 *          thus independently specified.
 *      (2) Special cases:
 *           * full fg mask: hf1 = vf1 = 0.0, hf2 = vf2 = 1.0.
 *           * empty fg (zero width) mask: set  hf1 = hf2  and vf1 = vf2.
 *           * fg rectangle with no hole: set hf2 = vf2 = 1.0.
 *           * frame touching outer boundary: set hf1 = vf1 = 0.0.
 *      (3) The vertical thickness of the horizontal mask parts
 *          is 0.5 * (vf2 - vf1) * h.  The horizontal thickness of the
 *          vertical mask parts is 0.5 * (hf2 - hf1) * w.
 * </pre>
 */
PIX *
pixMakeFrameMask(l_int32    w,
                 l_int32    h,
                 l_float32  hf1,
                 l_float32  hf2,
                 l_float32  vf1,
                 l_float32  vf2)
{
l_int32  h1, h2, v1, v2;
PIX     *pixd;

    if (w <= 0 || h <= 0)
        return (PIX *)ERROR_PTR("mask size 0", __func__, NULL);
    if (hf1 < 0.0 || hf1 > 1.0 || hf2 < 0.0 || hf2 > 1.0)
        return (PIX *)ERROR_PTR("invalid horiz fractions", __func__, NULL);
    if (vf1 < 0.0 || vf1 > 1.0 || vf2 < 0.0 || vf2 > 1.0)
        return (PIX *)ERROR_PTR("invalid vert fractions", __func__, NULL);
    if (hf1 > hf2 || vf1 > vf2)
        return (PIX *)ERROR_PTR("invalid relative sizes", __func__, NULL);

    pixd = pixCreate(w, h, 1);

        /* Special cases */
    if (hf1 == 0.0 && vf1 == 0.0 && hf2 == 1.0 && vf2 == 1.0) {  /* full */
        pixSetAll(pixd);
        return pixd;
    }
    if (hf1 == hf2 && vf1 == vf2) {  /* empty */
        return pixd;
    }

        /* General case */
    h1 = 0.5f * hf1 * w;
    h2 = 0.5f * hf2 * w;
    v1 = 0.5f * vf1 * h;
    v2 = 0.5f * vf2 * h;
    pixRasterop(pixd, h1, v1, w - 2 * h1, h - 2 * v1, PIX_SET, NULL, 0, 0);
    if (hf2 < 1.0 && vf2 < 1.0)
        pixRasterop(pixd, h2, v2, w - 2 * h2, h - 2 * v2, PIX_CLR, NULL, 0, 0);
    return pixd;
}


/*---------------------------------------------------------------------*
 *     Generate a covering of rectangles over connected components     *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixMakeCoveringOfRectangles()
 *
 * \param[in]   pixs       1 bpp
 * \param[in]   maxiters   max iterations: use 0 to iterate to completion
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This iteratively finds the bounding boxes of the connected
 *          components and generates a mask from them.  Two iterations
 *          should suffice for most situations.
 *      (2) Returns an empty pix if %pixs is empty.
 *      (3) If there are many small components in proximity, it may
 *          be useful to merge them with a morphological closing before
 *          calling this one.
 * </pre>
 */
PIX *
pixMakeCoveringOfRectangles(PIX     *pixs,
                            l_int32  maxiters)
{
l_int32  empty, same, niters;
BOXA    *boxa;
PIX     *pix1, *pix2;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (maxiters < 0)
        return (PIX *)ERROR_PTR("maxiters must be >= 0", __func__, NULL);
    if (maxiters == 0) maxiters = 50;  /* ridiculously large number */

    pixZero(pixs, &empty);
    pix1 = pixCreateTemplate(pixs);
    if (empty) return pix1;

        /* Do first iteration */
    boxa = pixConnCompBB(pixs, 8);
    pixMaskBoxa(pix1, pix1, boxa, L_SET_PIXELS);
    boxaDestroy(&boxa);
    if (maxiters == 1) return pix1;

    niters = 1;
    while (niters < maxiters) {  /* continue to add pixels to pix1 */
        niters++;
        boxa = pixConnCompBB(pix1, 8);
        pix2 = pixCopy(NULL, pix1);
        pixMaskBoxa(pix1, pix1, boxa, L_SET_PIXELS);
        boxaDestroy(&boxa);
        pixEqual(pix1, pix2, &same);
        pixDestroy(&pix2);
        if (same) {
            L_INFO("%d iterations\n", __func__, niters - 1);
            return pix1;
        }
    }
    L_INFO("maxiters = %d reached\n", __func__, niters);
    return pix1;
}


/*---------------------------------------------------------------------*
 *                 Fraction of Fg pixels under a mask                  *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixFractionFgInMask()
 *
 * \param[in]    pix1    1 bpp
 * \param[in]    pix2    1 bpp
 * \param[out]   pfract  fraction of fg pixels in 1 that are
 *                       aligned with the fg of 2
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) This gives the fraction of fg pixels in pix1 that are in
 *          the intersection (i.e., under the fg) of pix2:
 *          |1 & 2|/|1|, where |...| means the number of fg pixels.
 *          Note that this is different from the situation where
 *          pix1 and pix2 are reversed.
 *      (2) Both pix1 and pix2 are registered to the UL corners.  A warning
 *          is issued if pix1 and pix2 have different sizes.
 *      (3) This can also be used to find the fraction of fg pixels in pix1
 *          that are NOT under the fg of pix2: 1.0 - |1 & 2|/|1|
 *      (4) If pix1 or pix2 are empty, this returns %fract = 0.0.
 *      (5) For example, pix2 could be a frame around the outside of the
 *          image, made from pixMakeFrameMask().
 * </pre>
 */
l_ok
pixFractionFgInMask(PIX        *pix1,
                    PIX        *pix2,
                    l_float32  *pfract)
{
l_int32  w1, h1, w2, h2, empty, count1, count3;
PIX     *pix3;

    if (!pfract)
        return ERROR_INT("&fract not defined", __func__, 1);
    *pfract = 0.0;
    if (!pix1 || pixGetDepth(pix1) != 1)
        return ERROR_INT("pix1 not defined or not 1 bpp", __func__, 1);
    if (!pix2 || pixGetDepth(pix2) != 1)
        return ERROR_INT("pix2 not defined or not 1 bpp", __func__, 1);

    pixGetDimensions(pix1, &w1, &h1, NULL);
    pixGetDimensions(pix2, &w2, &h2, NULL);
    if (w1 != w2 || h1 != h2) {
        L_INFO("sizes unequal: (w1,w2) = (%d,%d), (h1,h2) = (%d,%d)\n",
               __func__, w1, w2, h1, h2);
    }
    pixZero(pix1, &empty);
    if (empty) return 0;
    pixZero(pix2, &empty);
    if (empty) return 0;

    pix3 = pixCopy(NULL, pix1);
    pixAnd(pix3, pix3, pix2);
    pixCountPixels(pix1, &count1, NULL);  /* |1| */
    pixCountPixels(pix3, &count3, NULL);  /* |1 & 2| */
    *pfract = (l_float32)count3 / (l_float32)count1;
    pixDestroy(&pix3);
    return 0;
}


/*---------------------------------------------------------------------*
 *                           Clip to Foreground                        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixClipToForeground()
 *
 * \param[in]    pixs   1 bpp
 * \param[out]   ppixd  [optional] clipped pix returned
 * \param[out]   pbox   [optional] bounding box
 * \return  0 if OK; 1 on error or if there are no fg pixels
 *
 * <pre>
 * Notes:
 *      (1) At least one of {&pixd, &box} must be specified.
 *      (2) If there are no fg pixels, the returned ptrs are null.
 * </pre>
 */
l_ok
pixClipToForeground(PIX   *pixs,
                    PIX  **ppixd,
                    BOX  **pbox)
{
l_int32    w, h, wpl, nfullwords, extra, i, j;
l_int32    minx, miny, maxx, maxy;
l_uint32   result, mask;
l_uint32  *data, *line;
BOX       *box;

    if (ppixd) *ppixd = NULL;
    if (pbox) *pbox = NULL;
    if (!ppixd && !pbox)
        return ERROR_INT("no output requested", __func__, 1);
    if (!pixs || (pixGetDepth(pixs) != 1))
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);

    pixGetDimensions(pixs, &w, &h, NULL);
    nfullwords = w / 32;
    extra = w & 31;
    mask = ~rmask32[32 - extra];
    wpl = pixGetWpl(pixs);
    data = pixGetData(pixs);

    result = 0;
    for (i = 0, miny = 0; i < h; i++, miny++) {
        line = data + i * wpl;
        for (j = 0; j < nfullwords; j++)
            result |= line[j];
        if (extra)
            result |= (line[j] & mask);
        if (result)
            break;
    }
    if (miny == h)  /* no ON pixels */
        return 1;

    result = 0;
    for (i = h - 1, maxy = h - 1; i >= 0; i--, maxy--) {
        line = data + i * wpl;
        for (j = 0; j < nfullwords; j++)
            result |= line[j];
        if (extra)
            result |= (line[j] & mask);
        if (result)
            break;
    }

    minx = 0;
    for (j = 0, minx = 0; j < w; j++, minx++) {
        for (i = 0; i < h; i++) {
            line = data + i * wpl;
            if (GET_DATA_BIT(line, j))
                goto minx_found;
        }
    }

minx_found:
    for (j = w - 1, maxx = w - 1; j >= 0; j--, maxx--) {
        for (i = 0; i < h; i++) {
            line = data + i * wpl;
            if (GET_DATA_BIT(line, j))
                goto maxx_found;
        }
    }

maxx_found:
    box = boxCreate(minx, miny, maxx - minx + 1, maxy - miny + 1);

    if (ppixd)
        *ppixd = pixClipRectangle(pixs, box, NULL);
    if (pbox)
        *pbox = box;
    else
        boxDestroy(&box);

    return 0;
}


/*!
 * \brief   pixTestClipToForeground()
 *
 * \param[in]    pixs      1 bpp
 * \param[out]   pcanclip  1 if fg does not extend to all four edges
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is a lightweight test to determine if a 1 bpp image
 *          can be further cropped without loss of fg pixels.
 *          If it cannot, canclip is set to 0.
 *      (2) It does not test for the existence of any fg pixels.
 *          If there are no fg pixels, it will return %canclip = 1.
 *          Check the output of the subsequent call to pixClipToForeground().
 * </pre>
 */
l_ok
pixTestClipToForeground(PIX      *pixs,
                        l_int32  *pcanclip)
{
l_int32    i, j, w, h, wpl, found;
l_uint32  *data, *line;

    if (!pcanclip)
        return ERROR_INT("&canclip not defined", __func__, 1);
    *pcanclip = 0;
    if (!pixs || (pixGetDepth(pixs) != 1))
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);

        /* Check top and bottom raster lines */
    pixGetDimensions(pixs, &w, &h, NULL);
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    found = FALSE;
    for (j = 0; found == FALSE && j < w; j++)
        found = GET_DATA_BIT(data, j);
    if (!found) {
        *pcanclip = 1;
        return 0;
    }

    line = data + (h - 1) * wpl;
    found = FALSE;
    for (j = 0; found == FALSE && j < w; j++)
        found = GET_DATA_BIT(data, j);
    if (!found) {
        *pcanclip = 1;
        return 0;
    }

        /* Check left and right edges */
    found = FALSE;
    for (i = 0, line = data; found == FALSE && i < h; line += wpl, i++)
        found = GET_DATA_BIT(line, 0);
    if (!found) {
        *pcanclip = 1;
        return 0;
    }

    found = FALSE;
    for (i = 0, line = data; found == FALSE && i < h; line += wpl, i++)
        found = GET_DATA_BIT(line, w - 1);
    if (!found)
        *pcanclip = 1;

    return 0;  /* fg pixels found on all edges */
}


/*!
 * \brief   pixClipBoxToForeground()
 *
 * \param[in]    pixs   1 bpp
 * \param[in]    boxs   [optional] use full image if null
 * \param[out]   ppixd  [optional] clipped pix returned
 * \param[out]   pboxd  [optional] bounding box
 * \return  0 if OK; 1 on error or if there are no fg pixels
 *
 * <pre>
 * Notes:
 *      (1) At least one of {&pixd, &boxd} must be specified.
 *      (2) If there are no fg pixels, the returned ptrs are null.
 *      (3) Do not use &pixs for the 3rd arg or &boxs for the 4th arg;
 *          this will leak memory.
 * </pre>
 */
l_ok
pixClipBoxToForeground(PIX   *pixs,
                       BOX   *boxs,
                       PIX  **ppixd,
                       BOX  **pboxd)
{
l_int32  w, h, bx, by, bw, bh, cbw, cbh, left, right, top, bottom;
BOX     *boxt, *boxd;

    if (ppixd) *ppixd = NULL;
    if (pboxd) *pboxd = NULL;
    if (!ppixd && !pboxd)
        return ERROR_INT("no output requested", __func__, 1);
    if (!pixs || (pixGetDepth(pixs) != 1))
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);

    if (!boxs)
        return pixClipToForeground(pixs, ppixd, pboxd);

    pixGetDimensions(pixs, &w, &h, NULL);
    boxGetGeometry(boxs, &bx, &by, &bw, &bh);
    cbw = L_MIN(bw, w - bx);
    cbh = L_MIN(bh, h - by);
    if (cbw < 0 || cbh < 0)
        return ERROR_INT("box not within image", __func__, 1);
    boxt = boxCreate(bx, by, cbw, cbh);

    if (pixScanForForeground(pixs, boxt, L_FROM_LEFT, &left)) {
        boxDestroy(&boxt);
        return 1;
    }
    pixScanForForeground(pixs, boxt, L_FROM_RIGHT, &right);
    pixScanForForeground(pixs, boxt, L_FROM_TOP, &top);
    pixScanForForeground(pixs, boxt, L_FROM_BOT, &bottom);

    boxd = boxCreate(left, top, right - left + 1, bottom - top + 1);
    if (ppixd)
        *ppixd = pixClipRectangle(pixs, boxd, NULL);
    if (pboxd)
        *pboxd = boxd;
    else
        boxDestroy(&boxd);

    boxDestroy(&boxt);
    return 0;
}


/*!
 * \brief   pixScanForForeground()
 *
 * \param[in]    pixs      1 bpp
 * \param[in]    box       [optional] within which the search is conducted
 * \param[in]    scanflag  direction of scan; e.g., L_FROM_LEFT
 * \param[out]   ploc      location in scan direction of first black pixel
 * \return  0 if OK; 1 on error or if no fg pixels are found
 *
 * <pre>
 * Notes:
 *      (1) If there are no fg pixels, the position is set to 0.
 *          Caller must check the return value!
 *      (2) Use %box == NULL to scan from edge of pixs
 * </pre>
 */
l_ok
pixScanForForeground(PIX      *pixs,
                     BOX      *box,
                     l_int32   scanflag,
                     l_int32  *ploc)
{
l_int32    bx, by, bw, bh, x, xstart, xend, y, ystart, yend, wpl;
l_uint32  *data, *line;
BOX       *boxt;

    if (!ploc)
        return ERROR_INT("&loc not defined", __func__, 1);
    *ploc = 0;
    if (!pixs || (pixGetDepth(pixs) != 1))
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);

        /* Clip box to pixs if it exists */
    pixGetDimensions(pixs, &bw, &bh, NULL);
    if (box) {
        if ((boxt = boxClipToRectangle(box, bw, bh)) == NULL)
            return ERROR_INT("invalid box", __func__, 1);
        boxGetGeometry(boxt, &bx, &by, &bw, &bh);
        boxDestroy(&boxt);
    } else {
        bx = by = 0;
    }
    xstart = bx;
    ystart = by;
    xend = bx + bw - 1;
    yend = by + bh - 1;

    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    if (scanflag == L_FROM_LEFT) {
        for (x = xstart; x <= xend; x++) {
            for (y = ystart; y <= yend; y++) {
                line = data + y * wpl;
                if (GET_DATA_BIT(line, x)) {
                    *ploc = x;
                    return 0;
                }
            }
        }
    } else if (scanflag == L_FROM_RIGHT) {
        for (x = xend; x >= xstart; x--) {
            for (y = ystart; y <= yend; y++) {
                line = data + y * wpl;
                if (GET_DATA_BIT(line, x)) {
                    *ploc = x;
                    return 0;
                }
            }
        }
    } else if (scanflag == L_FROM_TOP) {
        for (y = ystart; y <= yend; y++) {
            line = data + y * wpl;
            for (x = xstart; x <= xend; x++) {
                if (GET_DATA_BIT(line, x)) {
                    *ploc = y;
                    return 0;
                }
            }
        }
    } else if (scanflag == L_FROM_BOT) {
        for (y = yend; y >= ystart; y--) {
            line = data + y * wpl;
            for (x = xstart; x <= xend; x++) {
                if (GET_DATA_BIT(line, x)) {
                    *ploc = y;
                    return 0;
                }
            }
        }
    } else {
        return ERROR_INT("invalid scanflag", __func__, 1);
    }

    return 1;  /* no fg found */
}


/*!
 * \brief   pixClipBoxToEdges()
 *
 * \param[in]    pixs        1 bpp
 * \param[in]    boxs        [optional] ; use full image if null
 * \param[in]    lowthresh   threshold to choose clipping location
 * \param[in]    highthresh  threshold required to find an edge
 * \param[in]    maxwidth    max allowed width between low and high thresh locs
 * \param[in]    factor      sampling factor along pixel counting direction
 * \param[out]   ppixd       [optional] clipped pix returned
 * \param[out]   pboxd       [optional] bounding box
 * \return  0 if OK; 1 on error or if a fg edge is not found from
 *              all four sides.
 *
 * <pre>
 * Notes:
 *      (1) At least one of {&pixd, &boxd} must be specified.
 *      (2) If there are no fg pixels, the returned ptrs are null.
 *      (3) This function attempts to locate rectangular "image" regions
 *          of high-density fg pixels, that have well-defined edges
 *          on the four sides.
 *      (4) Edges are searched for on each side, iterating in order
 *          from left, right, top and bottom.  As each new edge is
 *          found, the search box is resized to use that location.
 *          Once an edge is found, it is held.  If no more edges
 *          are found in one iteration, the search fails.
 *      (5) See pixScanForEdge() for usage of the thresholds and %maxwidth.
 *      (6) The thresholds must be at least 1, and the low threshold
 *          cannot be larger than the high threshold.
 *      (7) If the low and high thresholds are both 1, this is equivalent
 *          to pixClipBoxToForeground().
 * </pre>
 */
l_ok
pixClipBoxToEdges(PIX     *pixs,
                  BOX     *boxs,
                  l_int32  lowthresh,
                  l_int32  highthresh,
                  l_int32  maxwidth,
                  l_int32  factor,
                  PIX    **ppixd,
                  BOX    **pboxd)
{
l_int32  w, h, bx, by, bw, bh, cbw, cbh, left, right, top, bottom;
l_int32  lfound, rfound, tfound, bfound, change;
BOX     *boxt, *boxd;

    if (ppixd) *ppixd = NULL;
    if (pboxd) *pboxd = NULL;
    if (!ppixd && !pboxd)
        return ERROR_INT("no output requested", __func__, 1);
    if (!pixs || (pixGetDepth(pixs) != 1))
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);
    if (lowthresh < 1 || highthresh < 1 ||
        lowthresh > highthresh || maxwidth < 1)
        return ERROR_INT("invalid thresholds", __func__, 1);
    factor = L_MIN(1, factor);

    if (lowthresh == 1 && highthresh == 1)
        return pixClipBoxToForeground(pixs, boxs, ppixd, pboxd);

    pixGetDimensions(pixs, &w, &h, NULL);
    if (boxs) {
        boxGetGeometry(boxs, &bx, &by, &bw, &bh);
        cbw = L_MIN(bw, w - bx);
        cbh = L_MIN(bh, h - by);
        if (cbw < 0 || cbh < 0)
            return ERROR_INT("box not within image", __func__, 1);
        boxt = boxCreate(bx, by, cbw, cbh);
    } else {
        boxt = boxCreate(0, 0, w, h);
    }

    lfound = rfound = tfound = bfound = 0;
    while (!lfound || !rfound || !tfound || !bfound) {
        change = 0;
        if (!lfound) {
            if (!pixScanForEdge(pixs, boxt, lowthresh, highthresh, maxwidth,
                                factor, L_FROM_LEFT, &left)) {
                lfound = 1;
                change = 1;
                boxRelocateOneSide(boxt, boxt, left, L_FROM_LEFT);
            }
        }
        if (!rfound) {
            if (!pixScanForEdge(pixs, boxt, lowthresh, highthresh, maxwidth,
                                factor, L_FROM_RIGHT, &right)) {
                rfound = 1;
                change = 1;
                boxRelocateOneSide(boxt, boxt, right, L_FROM_RIGHT);
            }
        }
        if (!tfound) {
            if (!pixScanForEdge(pixs, boxt, lowthresh, highthresh, maxwidth,
                                factor, L_FROM_TOP, &top)) {
                tfound = 1;
                change = 1;
                boxRelocateOneSide(boxt, boxt, top, L_FROM_TOP);
            }
        }
        if (!bfound) {
            if (!pixScanForEdge(pixs, boxt, lowthresh, highthresh, maxwidth,
                                factor, L_FROM_BOT, &bottom)) {
                bfound = 1;
                change = 1;
                boxRelocateOneSide(boxt, boxt, bottom, L_FROM_BOT);
            }
        }

#if DEBUG_EDGES
        lept_stderr("iter: %d %d %d %d\n", lfound, rfound, tfound, bfound);
#endif  /* DEBUG_EDGES */

        if (change == 0) break;
    }
    boxDestroy(&boxt);

    if (change == 0)
        return ERROR_INT("not all edges found", __func__, 1);

    boxd = boxCreate(left, top, right - left + 1, bottom - top + 1);
    if (ppixd)
        *ppixd = pixClipRectangle(pixs, boxd, NULL);
    if (pboxd)
        *pboxd = boxd;
    else
        boxDestroy(&boxd);

    return 0;
}


/*!
 * \brief   pixScanForEdge()
 *
 * \param[in]    pixs        1 bpp
 * \param[in]    box         [optional] within which the search is conducted
 * \param[in]    lowthresh   threshold to choose clipping location
 * \param[in]    highthresh  threshold required to find an edge
 * \param[in]    maxwidth    max allowed width between low and high thresh locs
 * \param[in]    factor      sampling factor along pixel counting direction
 * \param[in]    scanflag    direction of scan; e.g., L_FROM_LEFT
 * \param[out]   ploc        location in scan direction of first black pixel
 * \return  0 if OK; 1 on error or if the edge is not found
 *
 * <pre>
 * Notes:
 *      (1) If there are no fg pixels, the position is set to 0.
 *          Caller must check the return value!
 *      (2) Use %box == NULL to scan from edge of pixs
 *      (3) As the scan progresses, the location where the sum of
 *          pixels equals or excees %lowthresh is noted (loc).  The
 *          scan is stopped when the sum of pixels equals or exceeds
 *          %highthresh.  If the scan distance between loc and that
 *          point does not exceed %maxwidth, an edge is found and
 *          its position is taken to be loc.  %maxwidth implicitly
 *          sets a minimum on the required gradient of the edge.
 *      (4) The thresholds must be at least 1, and the low threshold
 *          cannot be larger than the high threshold.
 * </pre>
 */
l_ok
pixScanForEdge(PIX      *pixs,
               BOX      *box,
               l_int32   lowthresh,
               l_int32   highthresh,
               l_int32   maxwidth,
               l_int32   factor,
               l_int32   scanflag,
               l_int32  *ploc)
{
l_int32    bx, by, bw, bh, foundmin, loc, sum, wpl;
l_int32    x, xstart, xend, y, ystart, yend;
l_uint32  *data, *line;
BOX       *boxt;

    if (!ploc)
        return ERROR_INT("&ploc not defined", __func__, 1);
    *ploc = 0;
    if (!pixs || (pixGetDepth(pixs) != 1))
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);
    if (lowthresh < 1 || highthresh < 1 ||
        lowthresh > highthresh || maxwidth < 1)
        return ERROR_INT("invalid thresholds", __func__, 1);
    factor = L_MIN(1, factor);

        /* Clip box to pixs if it exists */
    pixGetDimensions(pixs, &bw, &bh, NULL);
    if (box) {
        if ((boxt = boxClipToRectangle(box, bw, bh)) == NULL)
            return ERROR_INT("invalid box", __func__, 1);
        boxGetGeometry(boxt, &bx, &by, &bw, &bh);
        boxDestroy(&boxt);
    } else {
        bx = by = 0;
    }
    xstart = bx;
    ystart = by;
    xend = bx + bw - 1;
    yend = by + bh - 1;

    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    foundmin = 0;
    if (scanflag == L_FROM_LEFT) {
        for (x = xstart; x <= xend; x++) {
            sum = 0;
            for (y = ystart; y <= yend; y += factor) {
                line = data + y * wpl;
                if (GET_DATA_BIT(line, x))
                    sum++;
            }
            if (!foundmin && sum < lowthresh)
                continue;
            if (!foundmin) {  /* save the loc of the beginning of the edge */
                foundmin = 1;
                loc = x;
            }
            if (sum >= highthresh) {
#if DEBUG_EDGES
                lept_stderr("Left: x = %d, loc = %d\n", x, loc);
#endif  /* DEBUG_EDGES */
                if (x - loc < maxwidth) {
                    *ploc = loc;
                    return 0;
                } else {
                  return 1;
                }
            }
        }
    } else if (scanflag == L_FROM_RIGHT) {
        for (x = xend; x >= xstart; x--) {
            sum = 0;
            for (y = ystart; y <= yend; y += factor) {
                line = data + y * wpl;
                if (GET_DATA_BIT(line, x))
                    sum++;
            }
            if (!foundmin && sum < lowthresh)
                continue;
            if (!foundmin) {
                foundmin = 1;
                loc = x;
            }
            if (sum >= highthresh) {
#if DEBUG_EDGES
                lept_stderr("Right: x = %d, loc = %d\n", x, loc);
#endif  /* DEBUG_EDGES */
                if (loc - x < maxwidth) {
                    *ploc = loc;
                    return 0;
                } else {
                  return 1;
                }
            }
        }
    } else if (scanflag == L_FROM_TOP) {
        for (y = ystart; y <= yend; y++) {
            sum = 0;
            line = data + y * wpl;
            for (x = xstart; x <= xend; x += factor) {
                if (GET_DATA_BIT(line, x))
                    sum++;
            }
            if (!foundmin && sum < lowthresh)
                continue;
            if (!foundmin) {
                foundmin = 1;
                loc = y;
            }
            if (sum >= highthresh) {
#if DEBUG_EDGES
                lept_stderr("Top: y = %d, loc = %d\n", y, loc);
#endif  /* DEBUG_EDGES */
                if (y - loc < maxwidth) {
                    *ploc = loc;
                    return 0;
                } else {
                  return 1;
                }
            }
        }
    } else if (scanflag == L_FROM_BOT) {
        for (y = yend; y >= ystart; y--) {
            sum = 0;
            line = data + y * wpl;
            for (x = xstart; x <= xend; x += factor) {
                if (GET_DATA_BIT(line, x))
                    sum++;
            }
            if (!foundmin && sum < lowthresh)
                continue;
            if (!foundmin) {
                foundmin = 1;
                loc = y;
            }
            if (sum >= highthresh) {
#if DEBUG_EDGES
                lept_stderr("Bottom: y = %d, loc = %d\n", y, loc);
#endif  /* DEBUG_EDGES */
                if (loc - y < maxwidth) {
                    *ploc = loc;
                    return 0;
                } else {
                  return 1;
                }
            }
        }
    } else {
        return ERROR_INT("invalid scanflag", __func__, 1);
    }

    return 1;  /* edge not found */
}


/*---------------------------------------------------------------------*
 *           Extract pixel averages and reversals along lines          *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixExtractOnLine()
 *
 * \param[in]    pixs     1 bpp or 8 bpp; no colormap
 * \param[in]    x1, y1   one end point for line
 * \param[in]    x2, y2   another end pt for line
 * \param[in]    factor   sampling; >= 1
 * \return  na of pixel values along line, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) Input end points are clipped to the pix.
 *      (2) If the line is either horizontal, or closer to horizontal
 *          than to vertical, the points will be extracted from left
 *          to right in the pix.  Likewise, if the line is vertical,
 *          or closer to vertical than to horizontal, the points will
 *          be extracted from top to bottom.
 *      (3) Can be used with numaCountReverals(), for example, to
 *          characterize the intensity smoothness along a line.
 * </pre>
 */
NUMA *
pixExtractOnLine(PIX     *pixs,
                 l_int32  x1,
                 l_int32  y1,
                 l_int32  x2,
                 l_int32  y2,
                 l_int32  factor)
{
l_int32    i, w, h, d, xmin, ymin, xmax, ymax, npts, direction;
l_uint32   val;
l_float32  x, y;
l_float64  slope;
NUMA      *na;
PTA       *pta;

    if (!pixs)
        return (NUMA *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 1 && d != 8)
        return (NUMA *)ERROR_PTR("d not 1 or 8 bpp", __func__, NULL);
    if (pixGetColormap(pixs))
        return (NUMA *)ERROR_PTR("pixs has a colormap", __func__, NULL);
    if (factor < 1) {
        L_WARNING("factor must be >= 1; setting to 1\n", __func__);
        factor = 1;
    }

        /* Clip line to the image */
    x1 = L_MAX(0, L_MIN(x1, w - 1));
    x2 = L_MAX(0, L_MIN(x2, w - 1));
    y1 = L_MAX(0, L_MIN(y1, h - 1));
    y2 = L_MAX(0, L_MIN(y2, h - 1));

    if (x1 == x2 && y1 == y2) {
        pixGetPixel(pixs, x1, y1, &val);
        na = numaCreate(1);
        numaAddNumber(na, val);
        return na;
    }

    if (y1 == y2)
        direction = L_HORIZONTAL_LINE;
    else if (x1 == x2)
        direction = L_VERTICAL_LINE;
    else
        direction = L_OBLIQUE_LINE;

    na = numaCreate(0);
    if (direction == L_HORIZONTAL_LINE) {  /* plot against x */
        xmin = L_MIN(x1, x2);
        xmax = L_MAX(x1, x2);
        numaSetParameters(na, xmin, factor);
        for (i = xmin; i <= xmax; i += factor) {
            pixGetPixel(pixs, i, y1, &val);
            numaAddNumber(na, val);
        }
    } else if (direction == L_VERTICAL_LINE) {  /* plot against y */
        ymin = L_MIN(y1, y2);
        ymax = L_MAX(y1, y2);
        numaSetParameters(na, ymin, factor);
        for (i = ymin; i <= ymax; i += factor) {
            pixGetPixel(pixs, x1, i, &val);
            numaAddNumber(na, val);
        }
    } else {  /* direction == L_OBLIQUE_LINE */
        slope = (l_float64)((y2 - y1) / (x2 - x1));
        if (L_ABS(slope) < 1.0) {  /* quasi-horizontal */
            xmin = L_MIN(x1, x2);
            xmax = L_MAX(x1, x2);
            ymin = (xmin == x1) ? y1 : y2;  /* pt that goes with xmin */
            ymax = (ymin == y1) ? y2 : y1;  /* pt that goes with xmax */
            pta = generatePtaLine(xmin, ymin, xmax, ymax);
            numaSetParameters(na, xmin, (l_float32)factor);
        } else {  /* quasi-vertical */
            ymin = L_MIN(y1, y2);
            ymax = L_MAX(y1, y2);
            xmin = (ymin == y1) ? x1 : x2;  /* pt that goes with ymin */
            xmax = (xmin == x1) ? x2 : x1;  /* pt that goes with ymax */
            pta = generatePtaLine(xmin, ymin, xmax, ymax);
            numaSetParameters(na, ymin, (l_float32)factor);
        }
        npts = ptaGetCount(pta);
        for (i = 0; i < npts; i += factor) {
            ptaGetPt(pta, i, &x, &y);
            pixGetPixel(pixs, (l_int32)x, (l_int32)y, &val);
            numaAddNumber(na, val);
        }

#if 0  /* debugging */
        pixPlotAlongPta(pixs, pta, GPLOT_PNG, NULL);
#endif

        ptaDestroy(&pta);
    }

    return na;
}


/*!
 * \brief   pixAverageOnLine()
 *
 * \param[in]    pixs     1 bpp or 8 bpp; no colormap
 * \param[in]    x1, y1   starting pt for line
 * \param[in]    x2, y2   end pt for line
 * \param[in]    factor   sampling; >= 1
 * \return  average of pixel values along line, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) The line must be either horizontal or vertical, so either
 *          y1 == y2 (horizontal) or x1 == x2 (vertical).
 *      (2) If horizontal, x1 must be <= x2.
 *          If vertical, y1 must be <= y2.
 *          characterize the intensity smoothness along a line.
 *      (3) Input end points are clipped to the pix.
 * </pre>
 */
l_float32
pixAverageOnLine(PIX     *pixs,
                 l_int32  x1,
                 l_int32  y1,
                 l_int32  x2,
                 l_int32  y2,
                 l_int32  factor)
{
l_int32    i, j, w, h, d, direction, count, wpl;
l_uint32  *data, *line;
l_float32  sum;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 1 && d != 8)
        return ERROR_INT("d not 1 or 8 bpp", __func__, 1);
    if (pixGetColormap(pixs))
        return ERROR_INT("pixs has a colormap", __func__, 1);
    if (x1 > x2 || y1 > y2)
        return ERROR_INT("x1 > x2 or y1 > y2", __func__, 1);

    if (y1 == y2) {
        x1 = L_MAX(0, x1);
        x2 = L_MIN(w - 1, x2);
        y1 = L_MAX(0, L_MIN(y1, h - 1));
        direction = L_HORIZONTAL_LINE;
    } else if (x1 == x2) {
        y1 = L_MAX(0, y1);
        y2 = L_MIN(h - 1, y2);
        x1 = L_MAX(0, L_MIN(x1, w - 1));
        direction = L_VERTICAL_LINE;
    } else {
        return ERROR_INT("line neither horiz nor vert", __func__, 1);
    }

    if (factor < 1) {
        L_WARNING("factor must be >= 1; setting to 1\n", __func__);
        factor = 1;
    }

    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    sum = 0;
    count = 0;
    if (direction == L_HORIZONTAL_LINE) {
        line = data + y1 * wpl;
        for (j = x1, count = 0; j <= x2; count++, j += factor) {
            if (d == 1)
                sum += GET_DATA_BIT(line, j);
            else  /* d == 8 */
                sum += GET_DATA_BYTE(line, j);
        }
    } else if (direction == L_VERTICAL_LINE) {
        for (i = y1, count = 0; i <= y2; count++, i += factor) {
            line = data + i * wpl;
            if (d == 1)
                sum += GET_DATA_BIT(line, x1);
            else  /* d == 8 */
                sum += GET_DATA_BYTE(line, x1);
        }
    }

    return sum / (l_float32)count;
}


/*!
 * \brief   pixAverageIntensityProfile()
 *
 * \param[in]    pixs      any depth; colormap OK
 * \param[in]    fract     fraction of image width or height to be used
 * \param[in]    dir       averaging direction: L_HORIZONTAL_LINE or
 *                         L_VERTICAL_LINE
 * \param[in]    first,    last span of rows or columns to measure
 * \param[in]    factor1   sampling along fast scan direction; >= 1
 * \param[in]    factor2   sampling along slow scan direction; >= 1
 * \return  na of reversal profile, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) If d != 1 bpp, colormaps are removed and the result
 *          is converted to 8 bpp.
 *      (2) If %dir == L_HORIZONTAL_LINE, the intensity is averaged
 *          along each horizontal raster line (sampled by %factor1),
 *          and the profile is the array of these averages in the
 *          vertical direction between %first and %last raster lines,
 *          and sampled by %factor2.
 *      (3) If %dir == L_VERTICAL_LINE, the intensity is averaged
 *          along each vertical line (sampled by %factor1),
 *          and the profile is the array of these averages in the
 *          horizontal direction between %first and %last columns,
 *          and sampled by %factor2.
 *      (4) The averages are measured over the central %fract of the image.
 *          Use %fract == 1.0 to average across the entire width or height.
 * </pre>
 */
NUMA *
pixAverageIntensityProfile(PIX       *pixs,
                           l_float32  fract,
                           l_int32    dir,
                           l_int32    first,
                           l_int32    last,
                           l_int32    factor1,
                           l_int32    factor2)
{
l_int32    i, j, w, h, d, start, end;
l_float32  ave;
NUMA      *nad;
PIX       *pixr, *pixg;

    if (!pixs)
        return (NUMA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (fract < 0.0 || fract > 1.0)
        return (NUMA *)ERROR_PTR("fract < 0.0 or > 1.0", __func__, NULL);
    if (dir != L_HORIZONTAL_LINE && dir != L_VERTICAL_LINE)
        return (NUMA *)ERROR_PTR("invalid direction", __func__, NULL);
    if (first < 0) first = 0;
    if (last < first)
        return (NUMA *)ERROR_PTR("last must be >= first", __func__, NULL);
    if (factor1 < 1) {
        L_WARNING("factor1 must be >= 1; setting to 1\n", __func__);
        factor1 = 1;
    }
    if (factor2 < 1) {
        L_WARNING("factor2 must be >= 1; setting to 1\n", __func__);
        factor2 = 1;
    }

        /* Use 1 or 8 bpp, without colormap */
    if (pixGetColormap(pixs))
        pixr = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    else
        pixr = pixClone(pixs);
    pixGetDimensions(pixr, &w, &h, &d);
    if (d == 1)
        pixg = pixClone(pixr);
    else
        pixg = pixConvertTo8(pixr, 0);

    nad = numaCreate(0);  /* output: samples in slow scan direction */
    numaSetParameters(nad, 0, factor2);
    if (dir == L_HORIZONTAL_LINE) {
        start = (l_int32)(0.5 * (1.0 - fract) * (l_float32)w);
        end = w - start;
        if (last > h - 1) {
            L_WARNING("last > h - 1; clipping\n", __func__);
            last = h - 1;
        }
        for (i = first; i <= last; i += factor2) {
            ave = pixAverageOnLine(pixg, start, i, end, i, factor1);
            numaAddNumber(nad, ave);
        }
    } else if (dir == L_VERTICAL_LINE) {
        start = (l_int32)(0.5 * (1.0 - fract) * (l_float32)h);
        end = h - start;
        if (last > w - 1) {
            L_WARNING("last > w - 1; clipping\n", __func__);
            last = w - 1;
        }
        for (j = first; j <= last; j += factor2) {
            ave = pixAverageOnLine(pixg, j, start, j, end, factor1);
            numaAddNumber(nad, ave);
        }
    }

    pixDestroy(&pixr);
    pixDestroy(&pixg);
    return nad;
}


/*!
 * \brief   pixReversalProfile()
 *
 * \param[in]    pixs          any depth; colormap OK
 * \param[in]    fract         fraction of image width or height to be used
 * \param[in]    dir           profile direction: L_HORIZONTAL_LINE or
 *                             L_VERTICAL_LINE
 * \param[in]    first, last   span of rows or columns to measure
 * \param[in]    minreversal   minimum change in intensity to trigger a reversal
 * \param[in]    factor1       sampling along raster line (fast scan); >= 1
 * \param[in]    factor2       sampling of raster lines (slow scan); >= 1
 * \return  na of reversal profile, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) If d != 1 bpp, colormaps are removed and the result
 *          is converted to 8 bpp.
 *      (2) If %dir == L_HORIZONTAL_LINE, the the reversals are counted
 *          along each horizontal raster line (sampled by %factor1),
 *          and the profile is the array of these sums in the
 *          vertical direction between %first and %last raster lines,
 *          and sampled by %factor2.
 *      (3) If %dir == L_VERTICAL_LINE, the the reversals are counted
 *          along each vertical column (sampled by %factor1),
 *          and the profile is the array of these sums in the
 *          horizontal direction between %first and %last columns,
 *          and sampled by %factor2.
 *      (4) For each row or column, the reversals are summed over the
 *          central %fract of the image.  Use %fract == 1.0 to sum
 *          across the entire width (of row) or height (of column).
 *      (5) %minreversal is the relative change in intensity that is
 *          required to resolve peaks and valleys.  A typical number for
 *          locating text in 8 bpp might be 50.  For 1 bpp, minreversal
 *          must be 1.
 *      (6) The reversal profile is simply the number of reversals
 *          in a row or column, vs the row or column index.
 * </pre>
 */
NUMA *
pixReversalProfile(PIX       *pixs,
                   l_float32  fract,
                   l_int32    dir,
                   l_int32    first,
                   l_int32    last,
                   l_int32    minreversal,
                   l_int32    factor1,
                   l_int32    factor2)
{
l_int32   i, j, w, h, d, start, end, nr;
NUMA     *naline, *nad;
PIX      *pixr, *pixg;

    if (!pixs)
        return (NUMA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (fract < 0.0 || fract > 1.0)
        return (NUMA *)ERROR_PTR("fract < 0.0 or > 1.0", __func__, NULL);
    if (dir != L_HORIZONTAL_LINE && dir != L_VERTICAL_LINE)
        return (NUMA *)ERROR_PTR("invalid direction", __func__, NULL);
    if (first < 0) first = 0;
    if (last < first)
        return (NUMA *)ERROR_PTR("last must be >= first", __func__, NULL);
    if (factor1 < 1) {
        L_WARNING("factor1 must be >= 1; setting to 1\n", __func__);
        factor1 = 1;
    }
    if (factor2 < 1) {
        L_WARNING("factor2 must be >= 1; setting to 1\n", __func__);
        factor2 = 1;
    }

        /* Use 1 or 8 bpp, without colormap */
    if (pixGetColormap(pixs))
        pixr = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    else
        pixr = pixClone(pixs);
    pixGetDimensions(pixr, &w, &h, &d);
    if (d == 1) {
        pixg = pixClone(pixr);
        minreversal = 1;  /* enforce this */
    } else {
        pixg = pixConvertTo8(pixr, 0);
    }

    nad = numaCreate(0);  /* output: samples in slow scan direction */
    numaSetParameters(nad, 0, factor2);
    if (dir == L_HORIZONTAL_LINE) {
        start = (l_int32)(0.5 * (1.0 - fract) * (l_float32)w);
        end = w - start;
        if (last > h - 1) {
            L_WARNING("last > h - 1; clipping\n", __func__);
            last = h - 1;
        }
        for (i = first; i <= last; i += factor2) {
            naline = pixExtractOnLine(pixg, start, i, end, i, factor1);
            numaCountReversals(naline, minreversal, &nr, NULL);
            numaAddNumber(nad, nr);
            numaDestroy(&naline);
        }
    } else if (dir == L_VERTICAL_LINE) {
        start = (l_int32)(0.5 * (1.0 - fract) * (l_float32)h);
        end = h - start;
        if (last > w - 1) {
            L_WARNING("last > w - 1; clipping\n", __func__);
            last = w - 1;
        }
        for (j = first; j <= last; j += factor2) {
            naline = pixExtractOnLine(pixg, j, start, j, end, factor1);
            numaCountReversals(naline, minreversal, &nr, NULL);
            numaAddNumber(nad, nr);
            numaDestroy(&naline);
        }
    }

    pixDestroy(&pixr);
    pixDestroy(&pixg);
    return nad;
}


/*---------------------------------------------------------------------*
 *                 Extract windowed variance along a line              *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixWindowedVarianceOnLine()
 *
 * \param[in]    pixs     8 bpp; no colormap
 * \param[in]    dir      L_HORIZONTAL_LINE or L_VERTICAL_LINE
 * \param[in]    loc      location of the constant coordinate for the line
 * \param[in]    c1, c2   end point coordinates for the line
 * \param[in]    size     window size; must be > 1
 * \param[out]   pnad     windowed square root of variance
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The returned variance array traverses the line starting
 *          from the smallest coordinate, min(c1,c2).
 *      (2) Line end points are clipped to pixs.
 *      (3) The reference point for the variance calculation is the center of
 *          the window.  Therefore, the numa start parameter from
 *          pixExtractOnLine() is incremented by %size/2,
 *          to align the variance values with the pixel coordinate.
 *      (4) The square root of the variance is the RMS deviation from the mean.
 * </pre>
 */
l_ok
pixWindowedVarianceOnLine(PIX     *pixs,
                          l_int32  dir,
                          l_int32  loc,
                          l_int32  c1,
                          l_int32  c2,
                          l_int32  size,
                          NUMA   **pnad)
{
l_int32     i, j, w, h, cmin, cmax, maxloc, n, x, y;
l_uint32    val;
l_float32   norm, rootvar;
l_float32  *array;
l_float64   sum1, sum2, ave, var;
NUMA       *na1, *nad;
PTA        *pta;

    if (!pnad)
        return ERROR_INT("&nad not defined", __func__, 1);
    *pnad = NULL;
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8bpp", __func__, 1);
    if (size < 2)
        return ERROR_INT("window size must be > 1", __func__, 1);
    if (dir != L_HORIZONTAL_LINE && dir != L_VERTICAL_LINE)
        return ERROR_INT("invalid direction", __func__, 1);
    pixGetDimensions(pixs, &w, &h, NULL);
    maxloc = (dir == L_HORIZONTAL_LINE) ? h - 1 : w - 1;
    if (loc < 0 || loc > maxloc)
        return ERROR_INT("invalid line position", __func__, 1);

        /* Clip line to the image */
    cmin = L_MIN(c1, c2);
    cmax = L_MAX(c1, c2);
    maxloc = (dir == L_HORIZONTAL_LINE) ? w - 1 : h - 1;
    cmin = L_MAX(0, L_MIN(cmin, maxloc));
    cmax = L_MAX(0, L_MIN(cmax, maxloc));
    n = cmax - cmin + 1;

        /* Generate pta along the line */
    pta = ptaCreate(n);
    if (dir == L_HORIZONTAL_LINE) {
        for (i = cmin; i <= cmax; i++)
            ptaAddPt(pta, i, loc);
    } else {  /* vertical line */
        for (i = cmin; i <= cmax; i++)
            ptaAddPt(pta, loc, i);
    }

        /* Get numa of pixel values on the line */
    na1 = numaCreate(n);
    numaSetParameters(na1, cmin, 1);
    for (i = 0; i < n; i++) {
        ptaGetIPt(pta, i, &x, &y);
        pixGetPixel(pixs, x, y, &val);
        numaAddNumber(na1, val);
    }
    array = numaGetFArray(na1, L_NOCOPY);
    ptaDestroy(&pta);

        /* Compute root variance on overlapping windows */
    nad = numaCreate(n);
    *pnad = nad;
    numaSetParameters(nad, cmin + size / 2, 1);
    norm = 1.0f / (l_float32)size;
    for (i = 0; i < n - size; i++) {  /* along the line */
        sum1 = sum2 = 0;
        for (j = 0; j < size; j++) {  /* over the window */
            val = array[i + j];
            sum1 += val;
            sum2 += (l_float64)(val) * val;
        }
        ave = norm * sum1;
        var = norm * sum2 - ave * ave;
        if (var < 0)  /* avoid small negative values from rounding effects */
            var = 0.0;
        rootvar = (l_float32)sqrt(var);
        numaAddNumber(nad, rootvar);
    }

    numaDestroy(&na1);
    return 0;
}


/*---------------------------------------------------------------------*
 *              Extract min/max of pixel values near lines             *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixMinMaxNearLine()
 *
 * \param[in]    pixs        8 bpp; no colormap
 * \param[in]    x1, y1      starting pt for line
 * \param[in]    x2, y2      end pt for line
 * \param[in]    dist        distance to search from line in each direction
 * \param[in]    direction   L_SCAN_NEGATIVE, L_SCAN_POSITIVE, L_SCAN_BOTH
 * \param[out]   pnamin      [optional] minimum values
 * \param[out]   pnamax      [optional] maximum values
 * \param[out]   pminave     [optional] average of minimum values
 * \param[out]   pmaxave     [optional] average of maximum values
 * \return  0 if OK; 1 on error or if there are no sampled points
 *              within the image.
 *
 * <pre>
 * Notes:
 *      (1) If the line is more horizontal than vertical, the values
 *          are computed for [x1, x2], and the pixels are taken
 *          below and/or above the local y-value.  Otherwise, the
 *          values are computed for [y1, y2] and the pixels are taken
 *          to the left and/or right of the local x value.
 *      (2) %direction specifies which side (or both sides) of the
 *          line are scanned for min and max values.
 *      (3) There are two ways to tell if the returned values of min
 *          and max averages are valid: the returned values cannot be
 *          negative and the function must return 0.
 *      (4) All accessed pixels are clipped to the pix.
 * </pre>
 */
l_ok
pixMinMaxNearLine(PIX        *pixs,
                  l_int32     x1,
                  l_int32     y1,
                  l_int32     x2,
                  l_int32     y2,
                  l_int32     dist,
                  l_int32     direction,
                  NUMA      **pnamin,
                  NUMA      **pnamax,
                  l_float32  *pminave,
                  l_float32  *pmaxave)
{
l_int32    i, j, w, h, d, x, y, n, dir, found, minval, maxval, negloc, posloc;
l_uint32   val;
l_float32  sum;
NUMA      *namin, *namax;
PTA       *pta;

    if (pnamin) *pnamin = NULL;
    if (pnamax) *pnamax = NULL;
    if (pminave) *pminave = UNDEF;
    if (pmaxave) *pmaxave = UNDEF;
    if (!pnamin && !pnamax && !pminave && !pmaxave)
        return ERROR_INT("no output requested", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8 || pixGetColormap(pixs))
        return ERROR_INT("pixs not 8 bpp or has colormap", __func__, 1);
    dist = L_ABS(dist);
    if (direction != L_SCAN_NEGATIVE && direction != L_SCAN_POSITIVE &&
        direction != L_SCAN_BOTH)
        return ERROR_INT("invalid direction", __func__, 1);

    pta = generatePtaLine(x1, y1, x2, y2);
    n = ptaGetCount(pta);
    dir = (L_ABS(x1 - x2) == n - 1) ? L_HORIZ : L_VERT;
    namin = numaCreate(n);
    namax = numaCreate(n);
    negloc = -dist;
    posloc = dist;
    if (direction == L_SCAN_NEGATIVE)
        posloc = 0;
    else if (direction == L_SCAN_POSITIVE)
        negloc = 0;
    for (i = 0; i < n; i++) {
        ptaGetIPt(pta, i, &x, &y);
        minval = 255;
        maxval = 0;
        found = FALSE;
        if (dir == L_HORIZ) {
            if (x < 0 || x >= w) continue;
            for (j = negloc; j <= posloc; j++) {
                if (y + j < 0 || y + j >= h) continue;
                pixGetPixel(pixs, x, y + j, &val);
                found = TRUE;
                if (val < minval) minval = val;
                if (val > maxval) maxval = val;
            }
        } else {  /* dir == L_VERT */
            if (y < 0 || y >= h) continue;
            for (j = negloc; j <= posloc; j++) {
                if (x + j < 0 || x + j >= w) continue;
                pixGetPixel(pixs, x + j, y, &val);
                found = TRUE;
                if (val < minval) minval = val;
                if (val > maxval) maxval = val;
            }
        }
        if (found) {
            numaAddNumber(namin, minval);
            numaAddNumber(namax, maxval);
        }
    }

    n = numaGetCount(namin);
    if (n == 0) {
        numaDestroy(&namin);
        numaDestroy(&namax);
        ptaDestroy(&pta);
        return ERROR_INT("no output from this line", __func__, 1);
    }

    if (pminave) {
        numaGetSum(namin, &sum);
        *pminave = sum / n;
    }
    if (pmaxave) {
        numaGetSum(namax, &sum);
        *pmaxave = sum / n;
    }
    if (pnamin)
        *pnamin = namin;
    else
        numaDestroy(&namin);
    if (pnamax)
        *pnamax = namax;
    else
        numaDestroy(&namax);
    ptaDestroy(&pta);
    return 0;
}


/*---------------------------------------------------------------------*
 *                     Rank row and column transforms                  *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixRankRowTransform()
 *
 * \param[in]    pixs   8 bpp; no colormap
 * \return  pixd with pixels sorted in each row, from
 *                    min to max value
 *
 * <pre>
 * Notes:
 *     (1) The time is O(n) in the number of pixels and runs about
 *         100 Mpixels/sec on a 3 GHz machine.
 * </pre>
 */
PIX *
pixRankRowTransform(PIX  *pixs)
{
l_int32    i, j, k, m, w, h, wpl, val;
l_int32    histo[256];
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs has a colormap", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    pixd = pixCreateTemplate(pixs);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpl = pixGetWpl(pixs);
    for (i = 0; i < h; i++) {
        memset(histo, 0, 1024);
        lines = datas + i * wpl;
        lined = datad + i * wpl;
        for (j = 0; j < w; j++) {
            val = GET_DATA_BYTE(lines, j);
            histo[val]++;
        }
        for (m = 0, j = 0; m < 256; m++) {
            for (k = 0; k < histo[m]; k++, j++)
                SET_DATA_BYTE(lined, j, m);
        }
    }

    return pixd;
}


/*!
 * \brief   pixRankColumnTransform()
 *
 * \param[in]    pixs   8 bpp; no colormap
 * \return  pixd with pixels sorted in each column, from
 *                    min to max value
 *
 * <pre>
 * Notes:
 *     (1) The time is O(n) in the number of pixels and runs about
 *         50 Mpixels/sec on a 3 GHz machine.
 * </pre>
 */
PIX *
pixRankColumnTransform(PIX  *pixs)
{
l_int32    i, j, k, m, w, h, val;
l_int32    histo[256];
void     **lines8, **lined8;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs has a colormap", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    pixd = pixCreateTemplate(pixs);
    lines8 = pixGetLinePtrs(pixs, NULL);
    lined8 = pixGetLinePtrs(pixd, NULL);
    for (j = 0; j < w; j++) {
        memset(histo, 0, 1024);
        for (i = 0; i < h; i++) {
            val = GET_DATA_BYTE(lines8[i], j);
            histo[val]++;
        }
        for (m = 0, i = 0; m < 256; m++) {
            for (k = 0; k < histo[m]; k++, i++)
                SET_DATA_BYTE(lined8[i], j, m);
        }
    }

    LEPT_FREE(lines8);
    LEPT_FREE(lined8);
    return pixd;
}

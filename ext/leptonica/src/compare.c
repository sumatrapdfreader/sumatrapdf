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
 * \file compare.c
 * <pre>
 *
 *      Test for pix equality
 *           l_int32     pixEqual()
 *           l_int32     pixEqualWithAlpha()
 *           l_int32     pixEqualWithCmap()
 *           l_int32     cmapEqual()
 *           l_int32     pixUsesCmapColor()
 *
 *      Binary correlation
 *           l_int32     pixCorrelationBinary()
 *
 *      Difference of two images of same size
 *           l_int32     pixDisplayDiff()
 *           l_int32     pixDisplayDiffBinary()
 *           l_int32     pixCompareBinary()
 *           l_int32     pixCompareGrayOrRGB()
 *           l_int32     pixCompareGray()
 *           l_int32     pixCompareRGB()
 *           l_int32     pixCompareTiled()
 *
 *      Other measures of the difference of two images of the same size
 *           NUMA       *pixCompareRankDifference()
 *           l_int32     pixTestForSimilarity()
 *           l_int32     pixGetDifferenceStats()
 *           NUMA       *pixGetDifferenceHistogram()
 *           l_int32     pixGetPerceptualDiff()
 *           l_int32     pixGetPSNR()
 *
 *      Comparison of photo regions by histogram
 *           l_int32     pixaComparePhotoRegionsByHisto()  -- top-level
 *           l_int32     pixComparePhotoRegionsByHisto()  -- top-level for 2
 *           l_int32     pixGenPhotoHistos()
 *           PIX        *pixPadToCenterCentroid()
 *           l_int32     pixCentroid8()
 *           l_int32     pixDecideIfPhotoImage()
 *       static l_int32  findHistoGridDimensions()
 *           l_int32     compareTilesByHisto()
 *
 *           l_int32     pixCompareGrayByHisto()  -- top-level for 2
 *       static l_int32  pixCompareTilesByHisto()
 *           l_int32     pixCropAlignedToCentroid()
 *
 *           l_uint8    *l_compressGrayHistograms()
 *           NUMAA      *l_uncompressGrayHistograms()
 *
 *      Translated images at the same resolution
 *           l_int32     pixCompareWithTranslation()
 *           l_int32     pixBestCorrelation()
 *
 *  For comparing images using tiled histograms, essentially all the
 *  computation goes into deciding if a region of an image is a photo,
 *  whether that photo region is amenable to similarity measurements
 *  using histograms, and finally the calculation of the gray histograms
 *  for each of the tiled regions.  The actual comparison is essentially
 *  instantaneous.  Therefore, with a large number of images to compare
 *  with each other, it is important to first calculate the histograms
 *  for each image.  Then the comparisons, which go as the square of the
 *  number of images, actually takes no time.
 *
 *  A high level function that takes a pixa of images and does
 *  all comparisons, pixaComparePhotosByHisto(), uses this split
 *  approach.  It pads the images so that the centroid is in the center,
 *  which will allow the tiles to be better aligned.
 *
 *  For testing purposes, two functions are given that do all the work
 *  to compare just two photo regions:
 *    *  pixComparePhotoRegionsByHisto() uses the split approach, qualifying
 *       the images first with pixGenPhotoHistos(), and then comparing
 *       with compareTilesByHisto().
 *    *  pixCompareGrayByHisto() aligns the two images by centroid
 *       and calls pixCompareTilesByHisto() to generate the histograms
 *       and do the comparison.
 *
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>
#include "allheaders.h"

    /* Small enough to consider equal to 0.0, for plot output */
static const l_float32  TINY = 0.00001f;

static l_ok findHistoGridDimensions(l_int32 n, l_int32 w, l_int32 h,
                                    l_int32 *pnx, l_int32 *pny, l_int32 debug);
static l_ok pixCompareTilesByHisto(PIX *pix1, PIX *pix2, l_int32 maxgray,
                                   l_int32 factor, l_int32 n,
                                   l_float32 *pscore, PIXA *pixadebug);

/*------------------------------------------------------------------*
 *                        Test for pix equality                     *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixEqual()
 *
 * \param[in]    pix1
 * \param[in]    pix2
 * \param[out]   psame  1 if same; 0 if different
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Equality is defined as having the same pixel values for
 *          each respective image pixel.
 *      (2) This works on two pix of any depth.  If one or both pix
 *          have a colormap, the depths can be different and the
 *          two pix can still be equal.
 *      (3) This ignores the alpha component for 32 bpp images.
 *      (4) If both pix have colormaps and the depths are equal,
 *          use the pixEqualWithCmap() function, which does a fast
 *          comparison if the colormaps are identical and a relatively
 *          slow comparison otherwise.
 *      (5) In all other cases, any existing colormaps must first be
 *          removed before doing pixel comparison.  After the colormaps
 *          are removed, the resulting two images must have the same depth.
 *          The "lowest common denominator" is RGB, but this is only
 *          chosen when necessary, or when both have colormaps but
 *          different depths.
 *      (6) For images without colormaps that are not 32 bpp, all bits
 *          in the image part of the data array must be identical.
 * </pre>
 */
l_ok
pixEqual(PIX      *pix1,
         PIX      *pix2,
         l_int32  *psame)
{
    return pixEqualWithAlpha(pix1, pix2, 0, psame);
}


/*!
 * \brief   pixEqualWithAlpha()
 *
 * \param[in]    pix1
 * \param[in]    pix2
 * \param[in]    use_alpha   1 to compare alpha in RGBA; 0 to ignore
 * \param[out]   psame       1 if same; 0 if different
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See notes in pixEqual().
 *      (2) This is more general than pixEqual(), in that for 32 bpp
 *          RGBA images, where spp = 4, you can optionally include
 *          the alpha component in the comparison.
 * </pre>
 */
l_ok
pixEqualWithAlpha(PIX      *pix1,
                  PIX      *pix2,
                  l_int32   use_alpha,
                  l_int32  *psame)
{
l_int32    w1, h1, d1, w2, h2, d2, wpl1, wpl2;
l_int32    spp1, spp2, i, j, color, mismatch, opaque;
l_int32    fullwords, linebits, endbits;
l_uint32   endmask, wordmask;
l_uint32  *data1, *data2, *line1, *line2;
PIX       *pixs1, *pixs2, *pixt1, *pixt2, *pixalpha;
PIXCMAP   *cmap1, *cmap2;

    if (!psame)
        return ERROR_INT("psame not defined", __func__, 1);
    *psame = 0;  /* init to not equal */
    if (!pix1 || !pix2)
        return ERROR_INT("pix1 and pix2 not both defined", __func__, 1);
    pixGetDimensions(pix1, &w1, &h1, &d1);
    pixGetDimensions(pix2, &w2, &h2, &d2);
    if (w1 != w2 || h1 != h2) {
        L_INFO("pix sizes differ\n", __func__);
        return 0;
    }

        /* Suppose the use_alpha flag is true.
         * If only one of two 32 bpp images has spp == 4, we call that
         * a "mismatch" of the alpha component.  In the case of a mismatch,
         * if the 4 bpp pix does not have all alpha components opaque (255),
         * the images are not-equal.  However if they are all opaque,
         * this image is equivalent to spp == 3, so we allow the
         * comparison to go forward, testing only for the RGB equality. */
    spp1 = pixGetSpp(pix1);
    spp2 = pixGetSpp(pix2);
    mismatch = 0;
    if (use_alpha && d1 == 32 && d2 == 32) {
        mismatch = ((spp1 == 4 && spp2 != 4) || (spp1 != 4 && spp2 == 4));
        if (mismatch) {
            pixalpha = (spp1 == 4) ? pix1 : pix2;
            pixAlphaIsOpaque(pixalpha, &opaque);
            if (!opaque) {
                L_INFO("just one pix has a non-opaque alpha layer\n", __func__);
                return 0;
            }
        }
    }

    cmap1 = pixGetColormap(pix1);
    cmap2 = pixGetColormap(pix2);
    if (!cmap1 && !cmap2 && (d1 != d2) && (d1 == 32 || d2 == 32)) {
        L_INFO("no colormaps, pix depths unequal, and one of them is RGB\n",
               __func__);
        return 0;
    }

    if (cmap1 && cmap2 && (d1 == d2))   /* use special function */
        return pixEqualWithCmap(pix1, pix2, psame);

        /* Must remove colormaps if they exist, and in the process
         * end up with the resulting images having the same depth. */
    if (cmap1 && !cmap2) {
        pixUsesCmapColor(pix1, &color);
        if (color && d2 <= 8)  /* can't be equal */
            return 0;
        if (d2 < 8)
            pixs2 = pixConvertTo8(pix2, FALSE);
        else
            pixs2 = pixClone(pix2);
        if (d2 <= 8)
            pixs1 = pixRemoveColormap(pix1, REMOVE_CMAP_TO_GRAYSCALE);
        else
            pixs1 = pixRemoveColormap(pix1, REMOVE_CMAP_TO_FULL_COLOR);
    } else if (!cmap1 && cmap2) {
        pixUsesCmapColor(pix2, &color);
        if (color && d1 <= 8)  /* can't be equal */
            return 0;
        if (d1 < 8)
            pixs1 = pixConvertTo8(pix1, FALSE);
        else
            pixs1 = pixClone(pix1);
        if (d1 <= 8)
            pixs2 = pixRemoveColormap(pix2, REMOVE_CMAP_TO_GRAYSCALE);
        else
            pixs2 = pixRemoveColormap(pix2, REMOVE_CMAP_TO_FULL_COLOR);
    } else if (cmap1 && cmap2) {  /* depths not equal; use rgb */
        pixs1 = pixRemoveColormap(pix1, REMOVE_CMAP_TO_FULL_COLOR);
        pixs2 = pixRemoveColormap(pix2, REMOVE_CMAP_TO_FULL_COLOR);
    } else {  /* no colormaps */
        pixs1 = pixClone(pix1);
        pixs2 = pixClone(pix2);
    }

        /* OK, we have no colormaps, but the depths may still be different */
    d1 = pixGetDepth(pixs1);
    d2 = pixGetDepth(pixs2);
    if (d1 != d2) {
        if (d1 == 16 || d2 == 16) {
            L_INFO("one pix is 16 bpp\n", __func__);
            pixDestroy(&pixs1);
            pixDestroy(&pixs2);
            return 0;
        }
        pixt1 = pixConvertLossless(pixs1, 8);
        pixt2 = pixConvertLossless(pixs2, 8);
        if (!pixt1 || !pixt2) {
            L_INFO("failure to convert to 8 bpp\n", __func__);
            pixDestroy(&pixs1);
            pixDestroy(&pixs2);
            pixDestroy(&pixt1);
            pixDestroy(&pixt2);
            return 0;
        }
    } else {
        pixt1 = pixClone(pixs1);
        pixt2 = pixClone(pixs2);
    }
    pixDestroy(&pixs1);
    pixDestroy(&pixs2);

        /* No colormaps, equal depths; do pixel comparisons */
    d1 = pixGetDepth(pixt1);
    d2 = pixGetDepth(pixt2);
    wpl1 = pixGetWpl(pixt1);
    wpl2 = pixGetWpl(pixt2);
    data1 = pixGetData(pixt1);
    data2 = pixGetData(pixt2);

    if (d1 == 32) {  /* test either RGB or RGBA pixels */
        if (use_alpha && !mismatch)
            wordmask = (spp1 == 3) ? 0xffffff00 : 0xffffffff;
        else
            wordmask = 0xffffff00;
        for (i = 0; i < h1; i++) {
            line1 = data1 + wpl1 * i;
            line2 = data2 + wpl2 * i;
            for (j = 0; j < wpl1; j++) {
                if ((*line1 ^ *line2) & wordmask) {
                    pixDestroy(&pixt1);
                    pixDestroy(&pixt2);
                    return 0;
                }
                line1++;
                line2++;
            }
        }
    } else {  /* all bits count */
        linebits = d1 * w1;
        fullwords = linebits / 32;
        endbits = linebits & 31;
        endmask = (endbits == 0) ? 0 : (0xffffffff << (32 - endbits));
        for (i = 0; i < h1; i++) {
            line1 = data1 + wpl1 * i;
            line2 = data2 + wpl2 * i;
            for (j = 0; j < fullwords; j++) {
                if (*line1 ^ *line2) {
                    pixDestroy(&pixt1);
                    pixDestroy(&pixt2);
                    return 0;
                }
                line1++;
                line2++;
            }
            if (endbits) {
                if ((*line1 ^ *line2) & endmask) {
                    pixDestroy(&pixt1);
                    pixDestroy(&pixt2);
                    return 0;
                }
            }
        }
    }

    pixDestroy(&pixt1);
    pixDestroy(&pixt2);
    *psame = 1;
    return 0;
}


/*!
 * \brief   pixEqualWithCmap()
 *
 * \param[in]    pix1
 * \param[in]    pix2
 * \param[out]   psame
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This returns same = TRUE if the images have identical content.
 *      (2) Both pix must have a colormap, and be of equal size and depth.
 *          If these conditions are not satisfied, it is not an error;
 *          the returned result is same = FALSE.
 *      (3) We then check whether the colormaps are the same; if so,
 *          the comparison proceeds 32 bits at a time.
 *      (4) If the colormaps are different, the comparison is done by
 *          slow brute force.
 * </pre>
 */
l_ok
pixEqualWithCmap(PIX      *pix1,
                 PIX      *pix2,
                 l_int32  *psame)
{
l_int32    d, w, h, wpl1, wpl2, i, j, linebits, fullwords, endbits;
l_int32    rval1, rval2, gval1, gval2, bval1, bval2, samecmaps;
l_uint32   endmask, val1, val2;
l_uint32  *data1, *data2, *line1, *line2;
PIXCMAP   *cmap1, *cmap2;

    if (!psame)
        return ERROR_INT("&same not defined", __func__, 1);
    *psame = 0;
    if (!pix1)
        return ERROR_INT("pix1 not defined", __func__, 1);
    if (!pix2)
        return ERROR_INT("pix2 not defined", __func__, 1);

    if (pixSizesEqual(pix1, pix2) == 0)
        return 0;
    cmap1 = pixGetColormap(pix1);
    cmap2 = pixGetColormap(pix2);
    if (!cmap1 || !cmap2) {
        L_INFO("both images don't have colormap\n", __func__);
        return 0;
    }
    pixGetDimensions(pix1, &w, &h, &d);
    if (d != 1 && d != 2 && d != 4 && d != 8) {
        L_INFO("pix depth not in {1, 2, 4, 8}\n", __func__);
        return 0;
    }

    cmapEqual(cmap1, cmap2, 3, &samecmaps);
    if (samecmaps == TRUE) {  /* colormaps are identical; compare by words */
        linebits = d * w;
        wpl1 = pixGetWpl(pix1);
        wpl2 = pixGetWpl(pix2);
        data1 = pixGetData(pix1);
        data2 = pixGetData(pix2);
        fullwords = linebits / 32;
        endbits = linebits & 31;
        endmask = (endbits == 0) ? 0 : (0xffffffff << (32 - endbits));
        for (i = 0; i < h; i++) {
            line1 = data1 + wpl1 * i;
            line2 = data2 + wpl2 * i;
            for (j = 0; j < fullwords; j++) {
                if (*line1 ^ *line2)
                    return 0;
                line1++;
                line2++;
            }
            if (endbits) {
                if ((*line1 ^ *line2) & endmask)
                    return 0;
            }
        }
        *psame = 1;
        return 0;
    }

        /* Colormaps aren't identical; compare pixel by pixel */
    for (i = 0; i < h; i++) {
        for (j = 0; j < w; j++) {
            pixGetPixel(pix1, j, i, &val1);
            pixGetPixel(pix2, j, i, &val2);
            pixcmapGetColor(cmap1, val1, &rval1, &gval1, &bval1);
            pixcmapGetColor(cmap2, val2, &rval2, &gval2, &bval2);
            if (rval1 != rval2 || gval1 != gval2 || bval1 != bval2)
                return 0;
        }
    }

    *psame = 1;
    return 0;
}


/*!
 * \brief   cmapEqual()
 *
 * \param[in]    cmap1
 * \param[in]    cmap2
 * \param[in]    ncomps  3 for RGB, 4 for RGBA
 * \param[out]   psame
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This returns %same = TRUE if the colormaps have identical entries.
 *      (2) If %ncomps == 4, the alpha components of the colormaps are also
 *          compared.
 * </pre>
 */
l_ok
cmapEqual(PIXCMAP  *cmap1,
          PIXCMAP  *cmap2,
          l_int32   ncomps,
          l_int32  *psame)
{
l_int32  n1, n2, i, rval1, rval2, gval1, gval2, bval1, bval2, aval1, aval2;

    if (!psame)
        return ERROR_INT("&same not defined", __func__, 1);
    *psame = FALSE;
    if (!cmap1)
        return ERROR_INT("cmap1 not defined", __func__, 1);
    if (!cmap2)
        return ERROR_INT("cmap2 not defined", __func__, 1);
    if (ncomps != 3 && ncomps != 4)
        return ERROR_INT("ncomps not 3 or 4", __func__, 1);

    n1 = pixcmapGetCount(cmap1);
    n2 = pixcmapGetCount(cmap2);
    if (n1 != n2) {
        L_INFO("colormap sizes are different\n", __func__);
        return 0;
    }

    for (i = 0; i < n1; i++) {
        pixcmapGetRGBA(cmap1, i, &rval1, &gval1, &bval1, &aval1);
        pixcmapGetRGBA(cmap2, i, &rval2, &gval2, &bval2, &aval2);
        if (rval1 != rval2 || gval1 != gval2 || bval1 != bval2)
            return 0;
        if (ncomps == 4 && aval1 != aval2)
            return 0;
    }
    *psame = TRUE;
    return 0;
}


/*!
 * \brief   pixUsesCmapColor()
 *
 * \param[in]    pixs     any depth, colormap
 * \param[out]   pcolor   TRUE if color found
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This returns color = TRUE if three things are obtained:
 *          (a) the pix has a colormap
 *          (b) the colormap has at least one color entry
 *          (c) a color entry is actually used
 *      (2) It is used in pixEqual() for comparing two images, in a
 *          situation where it is required to know if the colormap
 *          has color entries that are actually used in the image.
 * </pre>
 */
l_ok
pixUsesCmapColor(PIX      *pixs,
                 l_int32  *pcolor)
{
l_int32   n, i, rval, gval, bval, numpix;
NUMA     *na;
PIXCMAP  *cmap;

    if (!pcolor)
        return ERROR_INT("&color not defined", __func__, 1);
    *pcolor = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

    if ((cmap = pixGetColormap(pixs)) == NULL)
        return 0;

    pixcmapHasColor(cmap, pcolor);
    if (*pcolor == 0)  /* no color */
        return 0;

        /* The cmap has color entries.  Are they used? */
    na = pixGetGrayHistogram(pixs, 1);
    n = pixcmapGetCount(cmap);
    for (i = 0; i < n; i++) {
        pixcmapGetColor(cmap, i, &rval, &gval, &bval);
        numaGetIValue(na, i, &numpix);
        if ((rval != gval || rval != bval) && numpix) {  /* color found! */
            *pcolor = 1;
            break;
        }
    }
    numaDestroy(&na);

    return 0;
}


/*------------------------------------------------------------------*
 *                          Binary correlation                      *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixCorrelationBinary()
 *
 * \param[in]    pix1    1 bpp
 * \param[in]    pix2    1 bpp
 * \param[out]   pval    correlation
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The correlation is a number between 0.0 and 1.0,
 *          based on foreground similarity:
 *                           (|1 AND 2|)**2
 *            correlation =  --------------
 *                             |1| * |2|
 *          where |x| is the count of foreground pixels in image x.
 *          If the images are identical, this is 1.0.
 *          If they have no fg pixels in common, this is 0.0.
 *          If one or both images have no fg pixels, the correlation is 0.0.
 *      (2) Typically the two images are of equal size, but this
 *          is not enforced.  Instead, the UL corners are aligned.
 * </pre>
 */
l_ok
pixCorrelationBinary(PIX        *pix1,
                     PIX        *pix2,
                     l_float32  *pval)
{
l_int32   count1, count2, countn;
l_int32  *tab8;
PIX      *pixn;

    if (!pval)
        return ERROR_INT("&pval not defined", __func__, 1);
    *pval = 0.0;
    if (!pix1)
        return ERROR_INT("pix1 not defined", __func__, 1);
    if (!pix2)
        return ERROR_INT("pix2 not defined", __func__, 1);

    tab8 = makePixelSumTab8();
    pixCountPixels(pix1, &count1, tab8);
    pixCountPixels(pix2, &count2, tab8);
    if (count1 == 0 || count2 == 0) {
        LEPT_FREE(tab8);
        return 0;
    }
    pixn = pixAnd(NULL, pix1, pix2);
    pixCountPixels(pixn, &countn, tab8);
    *pval = (l_float32)countn * (l_float32)countn /
              ((l_float32)count1 * (l_float32)count2);
    LEPT_FREE(tab8);
    pixDestroy(&pixn);
    return 0;
}


/*------------------------------------------------------------------*
 *                   Difference of two images                       *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixDisplayDiff()
 *
 * \param[in]    pix1       any depth
 * \param[in]    pix2       any depth
 * \param[in]    showall    1 to display input images; 0 to only display result
 * \param[in]    mindiff    min difference to identify pixel
 * \param[in]    diffcolor  color of pixel indicating difference >= mindiff
 * \return  pixd  32 bpp rgb, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This aligns the UL corners of pix1 and pix2, crops to the
 *          overlapping pixels, and shows which pixels have a significant
 *          difference in value.
 *      (2) Requires %pix1 and %pix2 to have the same depth.
 *      (3) If rgb, a pixel is identified as different if any component
 *          values of the corresponding pixels equals or exceeds %mindiff.
 *      (4) %diffcolor is in format 0xrrggbbaa.
 *      (5) If %pix1 and %pix2 are 1 bpp, ignores %mindiff and %diffcolor,
 *          and uses the result of pixDisplayDiffBinary().
 * </pre>
 */
PIX *
pixDisplayDiff(PIX      *pix1,
               PIX      *pix2,
               l_int32   showall,
               l_int32   mindiff,
               l_uint32  diffcolor)
{
l_int32    i, j, w1, h1, d1, w2, h2, d2, minw, minh, wpl1, wpl2, wpl3;
l_int32    rval1, gval1, bval1, rval2, gval2, bval2;
l_uint32   val1, val2;
l_uint32  *data1, *data2, *data3, *line1, *line2, *line3;
PIX       *pix3 = NULL, *pix4 = NULL, *pixd;
PIXA      *pixa1;

    if (!pix1 || !pix2)
        return (PIX *)ERROR_PTR("pix1, pix2 not both defined", __func__, NULL);
    pixGetDimensions(pix1, &w1, &h1, &d1);
    pixGetDimensions(pix2, &w2, &h2, &d2);
    if (d1 != d2)
        return (PIX *)ERROR_PTR("unequal depths", __func__, NULL);
    if (mindiff <= 0)
        return (PIX *)ERROR_PTR("mindiff must be > 0", __func__, NULL);

    if (d1 == 1) {
        pix3 = pixDisplayDiffBinary(pix1, pix2);
        pixd = pixConvertTo32(pix3); 
        pixDestroy(&pix3);
    } else {
        minw = L_MIN(w1, w2);
        minh = L_MIN(h1, h2);
        pix3 = pixConvertTo32(pix1);
        pix4 = pixConvertTo32(pix2);
        pixd = pixCreate(minw, minh, 32);
        pixRasterop(pixd, 0, 0, minw, minh, PIX_SRC, pix3, 0, 0);
        data1 = pixGetData(pix3);
        wpl1 = pixGetWpl(pix3);
        data2 = pixGetData(pix4);
        wpl2 = pixGetWpl(pix4);
        data3 = pixGetData(pixd);
        wpl3 = pixGetWpl(pixd);
        for (i = 0; i < minh; i++) {
            line1 = data1 + i * wpl1;
            line2 = data2 + i * wpl2;
            line3 = data3 + i * wpl3;
            for (j = 0; j < minw; j++) {
                val1 = GET_DATA_FOUR_BYTES(line1, j);
                val2 = GET_DATA_FOUR_BYTES(line2, j);
                extractRGBValues(val1, &rval1, &gval1, &bval1);
                extractRGBValues(val2, &rval2, &gval2, &bval2);
                if (L_ABS(rval1 - rval2) >= mindiff ||
                    L_ABS(gval1 - gval2) >= mindiff ||
                    L_ABS(bval1 - bval2) >= mindiff)
                    SET_DATA_FOUR_BYTES(line3, j, diffcolor);
            }
        }
    }
                
    if (showall) {
        pixa1 = pixaCreate(3);
        if (d1 == 1) {
            pixaAddPix(pixa1, pix1, L_COPY);
            pixaAddPix(pixa1, pix2, L_COPY);
        } else {
            pixaAddPix(pixa1, pix3, L_INSERT);
            pixaAddPix(pixa1, pix4, L_INSERT);
        }
        pixaAddPix(pixa1, pixd, L_INSERT);  /* save diff image */
        pixd = pixaDisplayTiledInColumns(pixa1, 2, 1.0, 30, 2);  /* all 3 */
        pixaDestroy(&pixa1);
    } else if (d1 != 1) {
        pixDestroy(&pix3);
        pixDestroy(&pix4);
    }
    return pixd;
}


/*!
 * \brief   pixDisplayDiffBinary()
 *
 * \param[in]    pix1    1 bpp
 * \param[in]    pix2    1 bpp
 * \return  pixd 4 bpp cmapped, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This gives a color representation of the difference between
 *          pix1 and pix2.  The color difference depends on the order.
 *          The pixels in pixd have 4 colors:
 *           * unchanged:  black (on), white (off)
 *           * on in pix1, off in pix2: red
 *           * on in pix2, off in pix1: green
 *      (2) This aligns the UL corners of pix1 and pix2, and crops
 *          to the overlapping pixels.
 * </pre>
 */
PIX *
pixDisplayDiffBinary(PIX  *pix1,
                     PIX  *pix2)
{
l_int32   w1, h1, d1, w2, h2, d2, minw, minh;
PIX      *pixt, *pixd;
PIXCMAP  *cmap;

    if (!pix1 || !pix2)
        return (PIX *)ERROR_PTR("pix1, pix2 not both defined", __func__, NULL);
    pixGetDimensions(pix1, &w1, &h1, &d1);
    pixGetDimensions(pix2, &w2, &h2, &d2);
    if (d1 != 1 || d2 != 1)
        return (PIX *)ERROR_PTR("pix1 and pix2 not 1 bpp", __func__, NULL);
    minw = L_MIN(w1, w2);
    minh = L_MIN(h1, h2);

    pixd = pixCreate(minw, minh, 4);
    cmap = pixcmapCreate(4);
    pixcmapAddColor(cmap, 255, 255, 255);  /* initialized to white */
    pixcmapAddColor(cmap, 0, 0, 0);
    pixcmapAddColor(cmap, 255, 0, 0);
    pixcmapAddColor(cmap, 0, 255, 0);
    pixSetColormap(pixd, cmap);

    pixt = pixAnd(NULL, pix1, pix2);
    pixPaintThroughMask(pixd, pixt, 0, 0, 0x0);  /* black */
    pixSubtract(pixt, pix1, pix2);
    pixPaintThroughMask(pixd, pixt, 0, 0, 0xff000000);  /* red */
    pixSubtract(pixt, pix2, pix1);
    pixPaintThroughMask(pixd, pixt, 0, 0, 0x00ff0000);  /* green */
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixCompareBinary()
 *
 * \param[in]    pix1       1 bpp
 * \param[in]    pix2       1 bpp
 * \param[in]    comptype   L_COMPARE_XOR, L_COMPARE_SUBTRACT
 * \param[out]   pfract     fraction of pixels that are different
 * \param[out]   ppixdiff   [optional] pix of difference
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The two images are aligned at the UL corner, and do not
 *          need to be the same size.
 *      (2) If using L_COMPARE_SUBTRACT, pix2 is subtracted from pix1.
 *      (3) The total number of pixels is determined by pix1.
 *      (4) On error, the returned fraction is 1.0.
 * </pre>
 */
l_ok
pixCompareBinary(PIX        *pix1,
                 PIX        *pix2,
                 l_int32     comptype,
                 l_float32  *pfract,
                 PIX       **ppixdiff)
{
l_int32   w, h, count;
PIX      *pixt;

    if (ppixdiff) *ppixdiff = NULL;
    if (!pfract)
        return ERROR_INT("&pfract not defined", __func__, 1);
    *pfract = 1.0;  /* initialize to max difference */
    if (!pix1 || pixGetDepth(pix1) != 1)
        return ERROR_INT("pix1 not defined or not 1 bpp", __func__, 1);
    if (!pix2 || pixGetDepth(pix2) != 1)
        return ERROR_INT("pix2 not defined or not 1 bpp", __func__, 1);
    if (comptype != L_COMPARE_XOR && comptype != L_COMPARE_SUBTRACT)
        return ERROR_INT("invalid comptype", __func__, 1);

    if (comptype == L_COMPARE_XOR)
        pixt = pixXor(NULL, pix1, pix2);
    else  /* comptype == L_COMPARE_SUBTRACT) */
        pixt = pixSubtract(NULL, pix1, pix2);
    pixCountPixels(pixt, &count, NULL);
    pixGetDimensions(pix1, &w, &h, NULL);
    *pfract = (l_float32)(count) / (l_float32)(w * h);

    if (ppixdiff)
        *ppixdiff = pixt;
    else
        pixDestroy(&pixt);
    return 0;
}


/*!
 * \brief   pixCompareGrayOrRGB()
 *
 * \param[in]    pix1      2,4,8,16 bpp gray, 32 bpp rgb, or colormapped
 * \param[in]    pix2      2,4,8,16 bpp gray, 32 bpp rgb, or colormapped
 * \param[in]    comptype  L_COMPARE_SUBTRACT, L_COMPARE_ABS_DIFF
 * \param[in]    plottype  gplot plot output type, or 0 for no plot
 * \param[out]   psame     [optional] 1 if pixel values are identical
 * \param[out]   pdiff     [optional] average difference
 * \param[out]   prmsdiff  [optional] rms of difference
 * \param[out]   ppixdiff  [optional] pix of difference
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The two images are aligned at the UL corner, and do not
 *          need to be the same size.  If they are not the same size,
 *          the comparison will be made over overlapping pixels.
 *      (2) If there is a colormap, it is removed and the result
 *          is either gray or RGB depending on the colormap.
 *      (3) If RGB, each component is compared separately.
 *      (4) If type is L_COMPARE_ABS_DIFF, pix2 is subtracted from pix1
 *          and the absolute value is taken.
 *      (5) If type is L_COMPARE_SUBTRACT, pix2 is subtracted from pix1
 *          and the result is clipped to 0.
 *      (6) The plot output types are specified in gplot.h.
 *          Use 0 if no difference plot is to be made.
 *      (7) If the images are pixelwise identical, no difference
 *          plot is made, even if requested.  The result (TRUE or FALSE)
 *          is optionally returned in the parameter 'same'.
 *      (8) The average difference (either subtracting or absolute value)
 *          is optionally returned in the parameter 'diff'.
 *      (9) The RMS difference is optionally returned in the
 *          parameter 'rmsdiff'.  For RGB, we return the average of
 *          the RMS differences for each of the components.
 *     (10) Because pixel values are compared, pix1 and pix2 can be equal when:
 *          * they are both gray with different depth
 *          * one is colormapped and the other is not
 *          * they are both colormapped and have different size colormaps
 * </pre>
 */
l_ok
pixCompareGrayOrRGB(PIX        *pix1,
                    PIX        *pix2,
                    l_int32     comptype,
                    l_int32     plottype,
                    l_int32    *psame,
                    l_float32  *pdiff,
                    l_float32  *prmsdiff,
                    PIX       **ppixdiff)
{
l_int32  retval, d1, d2;
PIX     *pixt1, *pixt2, *pixs1, *pixs2;

    if (psame) *psame = 0;
    if (pdiff) *pdiff = 255.0;
    if (prmsdiff) *prmsdiff = 255.0;
    if (ppixdiff) *ppixdiff = NULL;
    if (!pix1 || pixGetDepth(pix1) == 1)
        return ERROR_INT("pix1 not defined or 1 bpp", __func__, 1);
    if (!pix2 || pixGetDepth(pix2) == 1)
        return ERROR_INT("pix2 not defined or 1 bpp", __func__, 1);
    if (comptype != L_COMPARE_SUBTRACT && comptype != L_COMPARE_ABS_DIFF)
        return ERROR_INT("invalid comptype", __func__, 1);
    if (plottype < 0 || plottype >= NUM_GPLOT_OUTPUTS)
        return ERROR_INT("invalid plottype", __func__, 1);

    pixt1 = pixRemoveColormap(pix1, REMOVE_CMAP_BASED_ON_SRC);
    pixt2 = pixRemoveColormap(pix2, REMOVE_CMAP_BASED_ON_SRC);
    d1 = pixGetDepth(pixt1);
    d2 = pixGetDepth(pixt2);
    if (d1 < 8)
        pixs1 = pixConvertTo8(pixt1, FALSE);
    else
        pixs1 = pixClone(pixt1);
    if (d2 < 8)
        pixs2 = pixConvertTo8(pixt2, FALSE);
    else
        pixs2 = pixClone(pixt2);
    pixDestroy(&pixt1);
    pixDestroy(&pixt2);
    d1 = pixGetDepth(pixs1);
    d2 = pixGetDepth(pixs2);
    if (d1 != d2) {
        pixDestroy(&pixs1);
        pixDestroy(&pixs2);
        return ERROR_INT("intrinsic depths are not equal", __func__, 1);
    }

    if (d1 == 8 || d1 == 16)
        retval = pixCompareGray(pixs1, pixs2, comptype, plottype, psame,
                                pdiff, prmsdiff, ppixdiff);
    else  /* d1 == 32 */
        retval = pixCompareRGB(pixs1, pixs2, comptype, plottype, psame,
                               pdiff, prmsdiff, ppixdiff);
    pixDestroy(&pixs1);
    pixDestroy(&pixs2);
    return retval;
}


/*!
 * \brief   pixCompareGray()
 *
 * \param[in]    pix1       8 or 16 bpp, not cmapped
 * \param[in]    pix2       8 or 16 bpp, not cmapped
 * \param[in]    comptype   L_COMPARE_SUBTRACT, L_COMPARE_ABS_DIFF
 * \param[in]    plottype   gplot plot output type, or 0 for no plot
 * \param[out]   psame      [optional] 1 if pixel values are identical
 * \param[out]   pdiff      [optional] average difference
 * \param[out]   prmsdiff   [optional] rms of difference
 * \param[out]   ppixdiff   [optional] pix of difference
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See pixCompareGrayOrRGB() for details.
 *      (2) Use pixCompareGrayOrRGB() if the input pix are colormapped.
 *      (3) Note: setting %plottype > 0 can result in writing named
 *                output files.
 * </pre>
 */
l_ok
pixCompareGray(PIX        *pix1,
               PIX        *pix2,
               l_int32     comptype,
               l_int32     plottype,
               l_int32    *psame,
               l_float32  *pdiff,
               l_float32  *prmsdiff,
               PIX       **ppixdiff)
{
char            buf[64];
static l_atomic index = 0;
l_int32         d1, d2, same, first, last;
GPLOT          *gplot;
NUMA           *na, *nac;
PIX            *pixt;

    if (psame) *psame = 0;
    if (pdiff) *pdiff = 255.0;
    if (prmsdiff) *prmsdiff = 255.0;
    if (ppixdiff) *ppixdiff = NULL;
    if (!pix1)
        return ERROR_INT("pix1 not defined", __func__, 1);
    if (!pix2)
        return ERROR_INT("pix2 not defined", __func__, 1);
    d1 = pixGetDepth(pix1);
    d2 = pixGetDepth(pix2);
    if ((d1 != d2) || (d1 != 8 && d1 != 16))
        return ERROR_INT("depths unequal or not 8 or 16 bpp", __func__, 1);
    if (pixGetColormap(pix1) || pixGetColormap(pix2))
        return ERROR_INT("pix1 and/or pix2 are colormapped", __func__, 1);
    if (comptype != L_COMPARE_SUBTRACT && comptype != L_COMPARE_ABS_DIFF)
        return ERROR_INT("invalid comptype", __func__, 1);
    if (plottype < 0 || plottype >= NUM_GPLOT_OUTPUTS)
        return ERROR_INT("invalid plottype", __func__, 1);

    lept_mkdir("lept/comp");

    if (comptype == L_COMPARE_SUBTRACT)
        pixt = pixSubtractGray(NULL, pix1, pix2);
    else  /* comptype == L_COMPARE_ABS_DIFF) */
        pixt = pixAbsDifference(pix1, pix2);

    pixZero(pixt, &same);
    if (same)
        L_INFO("Images are pixel-wise identical\n", __func__);
    if (psame) *psame = same;

    if (pdiff)
        pixGetAverageMasked(pixt, NULL, 0, 0, 1, L_MEAN_ABSVAL, pdiff);

        /* Don't bother to plot if the images are the same */
    if (plottype && !same) {
        L_INFO("Images differ: output plots will be generated\n", __func__);
        na = pixGetGrayHistogram(pixt, 1);
        numaGetNonzeroRange(na, TINY, &first, &last);
        nac = numaClipToInterval(na, 0, last);
        snprintf(buf, sizeof(buf), "/tmp/lept/comp/compare_gray%d", index);
        gplot = gplotCreate(buf, plottype,
                            "Pixel Difference Histogram", "diff val",
                            "number of pixels");
        gplotAddPlot(gplot, NULL, nac, GPLOT_LINES, "gray");
        gplotMakeOutput(gplot);
        gplotDestroy(&gplot);
        snprintf(buf, sizeof(buf), "/tmp/lept/comp/compare_gray%d.png",
                 index++);
        l_fileDisplay(buf, 100, 100, 1.0);
        numaDestroy(&na);
        numaDestroy(&nac);
    }

    if (ppixdiff)
        *ppixdiff = pixCopy(NULL, pixt);

    if (prmsdiff) {
        if (comptype == L_COMPARE_SUBTRACT) {  /* wrong type for rms diff */
            pixDestroy(&pixt);
            pixt = pixAbsDifference(pix1, pix2);
        }
        pixGetAverageMasked(pixt, NULL, 0, 0, 1, L_ROOT_MEAN_SQUARE, prmsdiff);
    }

    pixDestroy(&pixt);
    return 0;
}


/*!
 * \brief   pixCompareRGB()
 *
 * \param[in]    pix1       32 bpp rgb
 * \param[in]    pix2       32 bpp rgb
 * \param[in]    comptype   L_COMPARE_SUBTRACT, L_COMPARE_ABS_DIFF
 * \param[in]    plottype   gplot plot output type, or 0 for no plot
 * \param[out]   psame      [optional] 1 if pixel values are identical
 * \param[out]   pdiff      [optional] average difference
 * \param[out]   prmsdiff   [optional] rms of difference
 * \param[out]   ppixdiff   [optional] pix of difference
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See pixCompareGrayOrRGB() for details.
 *      (2) Note: setting %plottype > 0 can result in writing named
 *                output files.
 * </pre>
 */
l_ok
pixCompareRGB(PIX        *pix1,
              PIX        *pix2,
              l_int32     comptype,
              l_int32     plottype,
              l_int32    *psame,
              l_float32  *pdiff,
              l_float32  *prmsdiff,
              PIX       **ppixdiff)
{
char            buf[64];
static l_atomic index = 0;
l_int32         rsame, gsame, bsame, same, first, rlast, glast, blast, last;
l_float32       rdiff, gdiff, bdiff;
GPLOT          *gplot;
NUMA           *nar, *nag, *nab, *narc, *nagc, *nabc;
PIX            *pixr1, *pixr2, *pixg1, *pixg2, *pixb1, *pixb2;
PIX            *pixr, *pixg, *pixb;

    if (psame) *psame = 0;
    if (pdiff) *pdiff = 0.0;
    if (prmsdiff) *prmsdiff = 0.0;
    if (ppixdiff) *ppixdiff = NULL;
    if (!pix1 || pixGetDepth(pix1) != 32)
        return ERROR_INT("pix1 not defined or not 32 bpp", __func__, 1);
    if (!pix2 || pixGetDepth(pix2) != 32)
        return ERROR_INT("pix2 not defined or not ew bpp", __func__, 1);
    if (comptype != L_COMPARE_SUBTRACT && comptype != L_COMPARE_ABS_DIFF)
        return ERROR_INT("invalid comptype", __func__, 1);
    if (plottype < 0 || plottype >= NUM_GPLOT_OUTPUTS)
        return ERROR_INT("invalid plottype", __func__, 1);

    lept_mkdir("lept/comp");

    pixr1 = pixGetRGBComponent(pix1, COLOR_RED);
    pixr2 = pixGetRGBComponent(pix2, COLOR_RED);
    pixg1 = pixGetRGBComponent(pix1, COLOR_GREEN);
    pixg2 = pixGetRGBComponent(pix2, COLOR_GREEN);
    pixb1 = pixGetRGBComponent(pix1, COLOR_BLUE);
    pixb2 = pixGetRGBComponent(pix2, COLOR_BLUE);
    if (comptype == L_COMPARE_SUBTRACT) {
        pixr = pixSubtractGray(NULL, pixr1, pixr2);
        pixg = pixSubtractGray(NULL, pixg1, pixg2);
        pixb = pixSubtractGray(NULL, pixb1, pixb2);
    } else { /* comptype == L_COMPARE_ABS_DIFF) */
        pixr = pixAbsDifference(pixr1, pixr2);
        pixg = pixAbsDifference(pixg1, pixg2);
        pixb = pixAbsDifference(pixb1, pixb2);
    }

    pixZero(pixr, &rsame);
    pixZero(pixg, &gsame);
    pixZero(pixb, &bsame);
    same = rsame && gsame && bsame;
    if (same)
        L_INFO("Images are pixel-wise identical\n", __func__);
    if (psame) *psame = same;

    if (pdiff) {
        pixGetAverageMasked(pixr, NULL, 0, 0, 1, L_MEAN_ABSVAL, &rdiff);
        pixGetAverageMasked(pixg, NULL, 0, 0, 1, L_MEAN_ABSVAL, &gdiff);
        pixGetAverageMasked(pixb, NULL, 0, 0, 1, L_MEAN_ABSVAL, &bdiff);
        *pdiff = (rdiff + gdiff + bdiff) / 3.0;
    }

        /* Don't bother to plot if the images are the same */
    if (plottype && !same) {
        L_INFO("Images differ: output plots will be generated\n", __func__);
        nar = pixGetGrayHistogram(pixr, 1);
        nag = pixGetGrayHistogram(pixg, 1);
        nab = pixGetGrayHistogram(pixb, 1);
        numaGetNonzeroRange(nar, TINY, &first, &rlast);
        numaGetNonzeroRange(nag, TINY, &first, &glast);
        numaGetNonzeroRange(nab, TINY, &first, &blast);
        last = L_MAX(rlast, glast);
        last = L_MAX(last, blast);
        narc = numaClipToInterval(nar, 0, last);
        nagc = numaClipToInterval(nag, 0, last);
        nabc = numaClipToInterval(nab, 0, last);
        snprintf(buf, sizeof(buf), "/tmp/lept/comp/compare_rgb%d", index);
        gplot = gplotCreate(buf, plottype,
                            "Pixel Difference Histogram", "diff val",
                            "number of pixels");
        gplotAddPlot(gplot, NULL, narc, GPLOT_LINES, "red");
        gplotAddPlot(gplot, NULL, nagc, GPLOT_LINES, "green");
        gplotAddPlot(gplot, NULL, nabc, GPLOT_LINES, "blue");
        gplotMakeOutput(gplot);
        gplotDestroy(&gplot);
        snprintf(buf, sizeof(buf), "/tmp/lept/comp/compare_rgb%d.png",
                 index++);
        l_fileDisplay(buf, 100, 100, 1.0);
        numaDestroy(&nar);
        numaDestroy(&nag);
        numaDestroy(&nab);
        numaDestroy(&narc);
        numaDestroy(&nagc);
        numaDestroy(&nabc);
    }

    if (ppixdiff)
        *ppixdiff = pixCreateRGBImage(pixr, pixg, pixb);

    if (prmsdiff) {
        if (comptype == L_COMPARE_SUBTRACT) {
            pixDestroy(&pixr);
            pixDestroy(&pixg);
            pixDestroy(&pixb);
            pixr = pixAbsDifference(pixr1, pixr2);
            pixg = pixAbsDifference(pixg1, pixg2);
            pixb = pixAbsDifference(pixb1, pixb2);
        }
        pixGetAverageMasked(pixr, NULL, 0, 0, 1, L_ROOT_MEAN_SQUARE, &rdiff);
        pixGetAverageMasked(pixg, NULL, 0, 0, 1, L_ROOT_MEAN_SQUARE, &gdiff);
        pixGetAverageMasked(pixb, NULL, 0, 0, 1, L_ROOT_MEAN_SQUARE, &bdiff);
        *prmsdiff = (rdiff + gdiff + bdiff) / 3.0;
    }

    pixDestroy(&pixr1);
    pixDestroy(&pixr2);
    pixDestroy(&pixg1);
    pixDestroy(&pixg2);
    pixDestroy(&pixb1);
    pixDestroy(&pixb2);
    pixDestroy(&pixr);
    pixDestroy(&pixg);
    pixDestroy(&pixb);
    return 0;
}


/*!
 * \brief   pixCompareTiled()
 *
 * \param[in]    pix1       8 bpp or 32 bpp rgb
 * \param[in]    pix2       8 bpp 32 bpp rgb
 * \param[in]    sx, sy     tile size; must be > 1 in each dimension
 * \param[in]    type       L_MEAN_ABSVAL or L_ROOT_MEAN_SQUARE
 * \param[out]   ppixdiff   pix of difference
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) With L_MEAN_ABSVAL, we compute for each tile the
 *          average abs value of the pixel component difference between
 *          the two (aligned) images.  With L_ROOT_MEAN_SQUARE, we
 *          compute instead the rms difference over all components.
 *      (2) The two input pix must be the same depth.  Comparison is made
 *          using UL corner alignment.
 *      (3) For 32 bpp, the distance between corresponding tiles
 *          is found by averaging the measured difference over all three
 *          components of each pixel in the tile.
 *      (4) The result, pixdiff, contains one pixel for each source tile.
 * </pre>
 */
l_ok
pixCompareTiled(PIX     *pix1,
                PIX     *pix2,
                l_int32  sx,
                l_int32  sy,
                l_int32  type,
                PIX    **ppixdiff)
{
l_int32    d1, d2, w, h;
PIX       *pixt, *pixr, *pixg, *pixb;
PIX       *pixrdiff, *pixgdiff, *pixbdiff;
PIXACC    *pixacc;

    if (!ppixdiff)
        return ERROR_INT("&pixdiff not defined", __func__, 1);
    *ppixdiff = NULL;
    if (!pix1)
        return ERROR_INT("pix1 not defined", __func__, 1);
    if (!pix2)
        return ERROR_INT("pix2 not defined", __func__, 1);
    d1 = pixGetDepth(pix1);
    d2 = pixGetDepth(pix2);
    if (d1 != d2)
        return ERROR_INT("depths not equal", __func__, 1);
    if (d1 != 8 && d1 != 32)
        return ERROR_INT("pix1 not 8 or 32 bpp", __func__, 1);
    if (d2 != 8 && d2 != 32)
        return ERROR_INT("pix2 not 8 or 32 bpp", __func__, 1);
    if (sx < 2 || sy < 2)
        return ERROR_INT("sx and sy not both > 1", __func__, 1);
    if (type != L_MEAN_ABSVAL && type != L_ROOT_MEAN_SQUARE)
        return ERROR_INT("invalid type", __func__, 1);

    pixt = pixAbsDifference(pix1, pix2);
    if (d1 == 8) {
        *ppixdiff = pixGetAverageTiled(pixt, sx, sy, type);
    } else {  /* d1 == 32 */
        pixr = pixGetRGBComponent(pixt, COLOR_RED);
        pixg = pixGetRGBComponent(pixt, COLOR_GREEN);
        pixb = pixGetRGBComponent(pixt, COLOR_BLUE);
        pixrdiff = pixGetAverageTiled(pixr, sx, sy, type);
        pixgdiff = pixGetAverageTiled(pixg, sx, sy, type);
        pixbdiff = pixGetAverageTiled(pixb, sx, sy, type);
        pixGetDimensions(pixrdiff, &w, &h, NULL);
        pixacc = pixaccCreate(w, h, 0);
        pixaccAdd(pixacc, pixrdiff);
        pixaccAdd(pixacc, pixgdiff);
        pixaccAdd(pixacc, pixbdiff);
        pixaccMultConst(pixacc, 1.f / 3.f);
        *ppixdiff = pixaccFinal(pixacc, 8);
        pixDestroy(&pixr);
        pixDestroy(&pixg);
        pixDestroy(&pixb);
        pixDestroy(&pixrdiff);
        pixDestroy(&pixgdiff);
        pixDestroy(&pixbdiff);
        pixaccDestroy(&pixacc);
    }
    pixDestroy(&pixt);
    return 0;
}


/*------------------------------------------------------------------*
 *            Other measures of the difference of two images        *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixCompareRankDifference()
 *
 * \param[in]    pix1      8 bpp gray or 32 bpp rgb, or colormapped
 * \param[in]    pix2      8 bpp gray or 32 bpp rgb, or colormapped
 * \param[in]    factor    subsampling factor; use 0 or 1 for no subsampling
 * \return  narank      numa of rank difference, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This answers the question: if the pixel values in each
 *          component are compared by absolute difference, for
 *          any value of difference, what is the fraction of
 *          pixel pairs that have a difference of this magnitude
 *          or greater.  For a difference of 0, the fraction is 1.0.
 *          In this sense, it is a mapping from pixel difference to
 *          rank order of difference.
 *      (2) The two images are aligned at the UL corner, and do not
 *          need to be the same size.  If they are not the same size,
 *          the comparison will be made over overlapping pixels.
 *      (3) If there is a colormap, it is removed and the result
 *          is either gray or RGB depending on the colormap.
 *      (4) If RGB, pixel differences for each component are aggregated
 *          into a single histogram.
 * </pre>
 */
NUMA *
pixCompareRankDifference(PIX     *pix1,
                         PIX     *pix2,
                         l_int32  factor)
{
l_int32     i;
l_float32  *array1, *array2;
NUMA       *nah, *nan, *nad;

    if (!pix1)
        return (NUMA *)ERROR_PTR("pix1 not defined", __func__, NULL);
    if (!pix2)
        return (NUMA *)ERROR_PTR("pix2 not defined", __func__, NULL);

    if ((nah = pixGetDifferenceHistogram(pix1, pix2, factor)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);

    nan = numaNormalizeHistogram(nah, 1.0);
    array1 = numaGetFArray(nan, L_NOCOPY);

    nad = numaCreate(256);
    numaSetCount(nad, 256);  /* all initialized to 0.0 */
    array2 = numaGetFArray(nad, L_NOCOPY);

        /* Do rank accumulation on normalized histo of diffs */
    array2[0] = 1.0;
    for (i = 1; i < 256; i++)
        array2[i] = array2[i - 1] - array1[i - 1];

    numaDestroy(&nah);
    numaDestroy(&nan);
    return nad;
}


/*!
 * \brief   pixTestForSimilarity()
 *
 * \param[in]    pix1         8 bpp gray or 32 bpp rgb, or colormapped
 * \param[in]    pix2         8 bpp gray or 32 bpp rgb, or colormapped
 * \param[in]    factor       subsampling factor; use 0 or 1 for no subsampling
 * \param[in]    mindiff      minimum pixel difference to be counted; > 0
 * \param[in]    maxfract     maximum fraction of pixels allowed to have
 *                            diff greater than or equal to mindiff
 * \param[in]    maxave       maximum average difference of pixels allowed for
 *                            pixels with diff greater than or equal to
 *                            mindiff, after subtracting mindiff
 * \param[out]   psimilar     1 if similar, 0 otherwise
 * \param[in]    details      use 1 to give normalized histogram and other data
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This takes 2 pix that are the same size and determines using
 *          3 input parameters if they are "similar".  The first parameter
 *          %mindiff establishes a criterion of pixel-to-pixel similarity:
 *          two pixels are not similar if their difference in value is
 *          at least mindiff.  Then %maxfract and %maxave are thresholds
 *          on the number and distribution of dissimilar pixels
 *          allowed for the two pix to be similar.   If the pix are
 *          to be similar, neither threshold can be exceeded.
 *      (2) In setting the %maxfract and %maxave thresholds, you have
 *          these options:
 *            (a) Base the comparison only on %maxfract.  Then set
 *                %maxave = 0.0 or 256.0.  (If 0, we always ignore it.)
 *            (b) Base the comparison only on %maxave.  Then set
 *                %maxfract = 1.0.
 *            (c) Base the comparison on both thresholds.
 *      (3) Example of values that can be expected at mindiff = 15 when
 *          comparing lossless png encoding with jpeg encoding, q=75:
 *             (smoothish bg)       fractdiff = 0.01, avediff = 2.5
 *             (natural scene)      fractdiff = 0.13, avediff = 3.5
 *          To identify these images as 'similar', select maxfract
 *          and maxave to be upper bounds of what you expect.
 *      (4) See pixGetDifferenceStats() for a discussion of why we subtract
 *          mindiff from the computed average diff of the nonsimilar pixels
 *          to get the 'avediff' returned by that function.
 *      (5) If there is a colormap, it is removed and the result
 *          is either gray or RGB depending on the colormap.
 *      (6) If RGB, the maximum difference between pixel components is
 *          saved in the histogram.
 * </pre>
 */
l_ok
pixTestForSimilarity(PIX       *pix1,
                     PIX       *pix2,
                     l_int32    factor,
                     l_int32    mindiff,
                     l_float32  maxfract,
                     l_float32  maxave,
                     l_int32   *psimilar,
                     l_int32    details)
{
l_float32   fractdiff, avediff;

    if (!psimilar)
        return ERROR_INT("&similar not defined", __func__, 1);
    *psimilar = 0;
    if (!pix1)
        return ERROR_INT("pix1 not defined", __func__, 1);
    if (!pix2)
        return ERROR_INT("pix2 not defined", __func__, 1);
    if (pixSizesEqual(pix1, pix2) == 0)
        return ERROR_INT("pix sizes not equal", __func__, 1);
    if (mindiff <= 0)
        return ERROR_INT("mindiff must be > 0", __func__, 1);

    if (pixGetDifferenceStats(pix1, pix2, factor, mindiff,
                              &fractdiff, &avediff, details))
        return ERROR_INT("diff stats not found", __func__, 1);

    if (maxave <= 0.0) maxave = 256.0;
    if (fractdiff <= maxfract && avediff <= maxave)
        *psimilar = 1;
    return 0;
}


/*!
 * \brief   pixGetDifferenceStats()
 *
 * \param[in]    pix1        8 bpp gray or 32 bpp rgb, or colormapped
 * \param[in]    pix2        8 bpp gray or 32 bpp rgb, or colormapped
 * \param[in]    factor      subsampling factor; use 0 or 1 for no subsampling
 * \param[in]    mindiff     minimum pixel difference to be counted; > 0
 * \param[out]   pfractdiff  fraction of pixels with diff greater than or
 *                           equal to mindiff
 * \param[out]   pavediff    average difference of pixels with diff greater
 *                           than or equal to mindiff, less mindiff
 * \param[in]    details     use 1 to give normalized histogram and other data
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This takes a threshold %mindiff and describes the difference
 *          between two images in terms of two numbers:
 *            (a) the fraction of pixels, %fractdiff, whose difference
 *                equals or exceeds the threshold %mindiff, and
 *            (b) the average value %avediff of the difference in pixel value
 *                for the pixels in the set given by (a), after you subtract
 *                %mindiff.  The reason for subtracting %mindiff is that
 *                you then get a useful measure for the rate of falloff
 *                of the distribution for larger differences.  For example,
 *                if %mindiff = 10 and you find that %avediff = 2.5, it
 *                says that of the pixels with diff > 10, the average of
 *                their diffs is just mindiff + 2.5 = 12.5.  This is a
 *                fast falloff in the histogram with increasing difference.
 *      (2) The two images are aligned at the UL corner, and do not
 *          need to be the same size.  If they are not the same size,
 *          the comparison will be made over overlapping pixels.
 *      (3) If there is a colormap, it is removed and the result
 *          is either gray or RGB depending on the colormap.
 *      (4) If RGB, the maximum difference between pixel components is
 *          saved in the histogram.
 *      (5) Set %details == 1 to see the difference histogram and get
 *          an output that shows for each value of %mindiff, what are the
 *          minimum values required for fractdiff and avediff in order
 *          that the two pix will be considered similar.
 * </pre>
 */
l_ok
pixGetDifferenceStats(PIX        *pix1,
                      PIX        *pix2,
                      l_int32     factor,
                      l_int32     mindiff,
                      l_float32  *pfractdiff,
                      l_float32  *pavediff,
                      l_int32     details)
{
l_int32     i, first, last, diff;
l_float32   fract, ave;
l_float32  *array;
NUMA       *nah, *nan, *nac;

    if (pfractdiff) *pfractdiff = 0.0;
    if (pavediff) *pavediff = 0.0;
    if (!pfractdiff)
        return ERROR_INT("&fractdiff not defined", __func__, 1);
    if (!pavediff)
        return ERROR_INT("&avediff not defined", __func__, 1);
    if (!pix1)
        return ERROR_INT("pix1 not defined", __func__, 1);
    if (!pix2)
        return ERROR_INT("pix2 not defined", __func__, 1);
    if (mindiff <= 0)
        return ERROR_INT("mindiff must be > 0", __func__, 1);

    if ((nah = pixGetDifferenceHistogram(pix1, pix2, factor)) == NULL)
        return ERROR_INT("na not made", __func__, 1);

    if ((nan = numaNormalizeHistogram(nah, 1.0)) == NULL) {
        numaDestroy(&nah);
        return ERROR_INT("nan not made", __func__, 1);
    }
    array = numaGetFArray(nan, L_NOCOPY);

    if (details) {
        lept_mkdir("lept/comp");
        numaGetNonzeroRange(nan, 0.0, &first, &last);
        nac = numaClipToInterval(nan, first, last);
        gplotSimple1(nac, GPLOT_PNG, "/tmp/lept/comp/histo",
                     "Difference histogram");
        l_fileDisplay("/tmp/lept/comp/histo.png", 500, 0, 1.0);
        lept_stderr("\nNonzero values in normalized histogram:");
        numaWriteStderr(nac);
        numaDestroy(&nac);
        lept_stderr(" Mindiff      fractdiff      avediff\n");
        lept_stderr(" -----------------------------------\n");
        for (diff = 1; diff < L_MIN(2 * mindiff, last); diff++) {
            fract = 0.0;
            ave = 0.0;
            for (i = diff; i <= last; i++) {
                fract += array[i];
                ave += (l_float32)i * array[i];
            }
            ave = (fract == 0.0) ? 0.0 : ave / fract;
            ave -= diff;
            lept_stderr("%5d         %7.4f        %7.4f\n",
                        diff, fract, ave);
        }
        lept_stderr(" -----------------------------------\n");
    }

    fract = 0.0;
    ave = 0.0;
    for (i = mindiff; i < 256; i++) {
      fract += array[i];
      ave += (l_float32)i * array[i];
    }
    ave = (fract == 0.0) ? 0.0 : ave / fract;
    ave -= mindiff;

    *pfractdiff = fract;
    *pavediff = ave;

    numaDestroy(&nah);
    numaDestroy(&nan);
    return 0;
}


/*!
 * \brief   pixGetDifferenceHistogram()
 *
 * \param[in]    pix1      8 bpp gray or 32 bpp rgb, or colormapped
 * \param[in]    pix2      8 bpp gray or 32 bpp rgb, or colormapped
 * \param[in]    factor    subsampling factor; use 0 or 1 for no subsampling
 * \return  na     Numa of histogram of differences, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The two images are aligned at the UL corner, and do not
 *          need to be the same size.  If they are not the same size,
 *          the comparison will be made over overlapping pixels.
 *      (2) If there is a colormap, it is removed and the result
 *          is either gray or RGB depending on the colormap.
 *      (3) If RGB, the maximum difference between pixel components is
 *          saved in the histogram.
 * </pre>
 */
NUMA *
pixGetDifferenceHistogram(PIX     *pix1,
                          PIX     *pix2,
                          l_int32  factor)
{
l_int32     w1, h1, d1, w2, h2, d2, w, h, wpl1, wpl2;
l_int32     i, j, val, val1, val2;
l_int32     rval1, rval2, gval1, gval2, bval1, bval2;
l_int32     rdiff, gdiff, bdiff, maxdiff;
l_uint32   *data1, *data2, *line1, *line2;
l_float32  *array;
NUMA       *na;
PIX        *pixt1, *pixt2;

    if (!pix1)
        return (NUMA *)ERROR_PTR("pix1 not defined", __func__, NULL);
    if (!pix2)
        return (NUMA *)ERROR_PTR("pix2 not defined", __func__, NULL);
    d1 = pixGetDepth(pix1);
    d2 = pixGetDepth(pix2);
    if (d1 == 16 || d2 == 16)
        return (NUMA *)ERROR_PTR("d == 16 not supported", __func__, NULL);
    if (d1 < 8 && !pixGetColormap(pix1))
        return (NUMA *)ERROR_PTR("pix1 depth < 8 bpp and not cmapped",
                                 __func__, NULL);
    if (d2 < 8 && !pixGetColormap(pix2))
        return (NUMA *)ERROR_PTR("pix2 depth < 8 bpp and not cmapped",
                                 __func__, NULL);
    pixt1 = pixRemoveColormap(pix1, REMOVE_CMAP_BASED_ON_SRC);
    pixt2 = pixRemoveColormap(pix2, REMOVE_CMAP_BASED_ON_SRC);
    pixGetDimensions(pixt1, &w1, &h1, &d1);
    pixGetDimensions(pixt2, &w2, &h2, &d2);
    if (d1 != d2) {
        pixDestroy(&pixt1);
        pixDestroy(&pixt2);
        return (NUMA *)ERROR_PTR("pix depths not equal", __func__, NULL);
    }
    if (factor < 1) factor = 1;

    na = numaCreate(256);
    numaSetCount(na, 256);  /* all initialized to 0.0 */
    array = numaGetFArray(na, L_NOCOPY);
    w = L_MIN(w1, w2);
    h = L_MIN(h1, h2);
    data1 = pixGetData(pixt1);
    data2 = pixGetData(pixt2);
    wpl1 = pixGetWpl(pixt1);
    wpl2 = pixGetWpl(pixt2);
    if (d1 == 8) {
        for (i = 0; i < h; i += factor) {
            line1 = data1 + i * wpl1;
            line2 = data2 + i * wpl2;
            for (j = 0; j < w; j += factor) {
                val1 = GET_DATA_BYTE(line1, j);
                val2 = GET_DATA_BYTE(line2, j);
                val = L_ABS(val1 - val2);
                array[val]++;
            }
        }
    } else {  /* d1 == 32 */
        for (i = 0; i < h; i += factor) {
            line1 = data1 + i * wpl1;
            line2 = data2 + i * wpl2;
            for (j = 0; j < w; j += factor) {
                extractRGBValues(line1[j], &rval1, &gval1, &bval1);
                extractRGBValues(line2[j], &rval2, &gval2, &bval2);
                rdiff = L_ABS(rval1 - rval2);
                gdiff = L_ABS(gval1 - gval2);
                bdiff = L_ABS(bval1 - bval2);
                maxdiff = L_MAX(rdiff, gdiff);
                maxdiff = L_MAX(maxdiff, bdiff);
                array[maxdiff]++;
            }
        }
    }

    pixDestroy(&pixt1);
    pixDestroy(&pixt2);
    return na;
}


/*!
 * \brief   pixGetPerceptualDiff()
 *
 * \param[in]    pixs1       8 bpp gray or 32 bpp rgb, or colormapped
 * \param[in]    pixs2       8 bpp gray or 32 bpp rgb, or colormapped
 * \param[in]    sampling    subsampling factor; use 0 or 1 for no subsampling
 * \param[in]    dilation    size of grayscale or color Sel; odd
 * \param[in]    mindiff     minimum pixel difference to be counted; > 0
 * \param[out]   pfract      fraction of pixels with diff greater than mindiff
 * \param[out]   ppixdiff1   [optional] showing difference (gray or color)
 * \param[out]   ppixdiff2   [optional] showing pixels of sufficient diff
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This takes 2 pix and determines, using 2 input parameters:
 *           * %dilation specifies the amount of grayscale or color
 *             dilation to apply to the images, to compensate for
 *             a small amount of misregistration.  A typical number might
 *             be 5, which uses a 5x5 Sel.  Grayscale dilation expands
 *             lighter pixels into darker pixel regions.
 *           * %mindiff determines the threshold on the difference in
 *             pixel values to be counted -- two pixels are not similar
 *             if their difference in value is at least %mindiff.  For
 *             color pixels, we use the maximum component difference.
 *      (2) The pixelwise comparison is always done with the UL corners
 *          aligned.  The sizes of pix1 and pix2 need not be the same,
 *          although in practice it can be useful to scale to the same size.
 *      (3) If there is a colormap, it is removed and the result
 *          is either gray or RGB depending on the colormap.
 *      (4) Two optional diff images can be retrieved (typ. for debugging):
 *           pixdiff1: the gray or color difference
 *           pixdiff2: thresholded to 1 bpp for pixels exceeding %mindiff
 *      (5) The returned value of fract can be compared to some threshold,
 *          which is application dependent.
 *      (6) This method is in analogy to the two-sided hausdorff transform,
 *          except here it is for d > 1.  For d == 1 (see pixRankHaustest()),
 *          we verify that when one pix1 is dilated, it covers at least a
 *          given fraction of the pixels in pix2, and v.v.; in that
 *          case, the two pix are sufficiently similar.  Here, we
 *          do an analogous thing: subtract the dilated pix1 from pix2 to
 *          get a 1-sided hausdorff-like transform.  Then do it the
 *          other way.  Take the component-wise max of the two results,
 *          and threshold to get the fraction of pixels with a difference
 *          below the threshold.
 * </pre>
 */
l_ok
pixGetPerceptualDiff(PIX        *pixs1,
                     PIX        *pixs2,
                     l_int32     sampling,
                     l_int32     dilation,
                     l_int32     mindiff,
                     l_float32  *pfract,
                     PIX       **ppixdiff1,
                     PIX       **ppixdiff2)
{
l_int32  d1, d2, w, h, count;
PIX     *pix1, *pix2, *pix3, *pix4, *pix5, *pix6, *pix7, *pix8, *pix9;
PIX     *pix10, *pix11;

    if (ppixdiff1) *ppixdiff1 = NULL;
    if (ppixdiff2) *ppixdiff2 = NULL;
    if (!pfract)
        return ERROR_INT("&fract not defined", __func__, 1);
    *pfract = 1.0;  /* init to completely different */
    if ((dilation & 1) == 0)
        return ERROR_INT("dilation must be odd", __func__, 1);
    if (!pixs1)
        return ERROR_INT("pixs1 not defined", __func__, 1);
    if (!pixs2)
        return ERROR_INT("pixs2 not defined", __func__, 1);
    d1 = pixGetDepth(pixs1);
    d2 = pixGetDepth(pixs2);
    if (!pixGetColormap(pixs1) && d1 < 8)
        return ERROR_INT("pixs1 not cmapped and < 8 bpp", __func__, 1);
    if (!pixGetColormap(pixs2) && d2 < 8)
        return ERROR_INT("pixs2 not cmapped and < 8 bpp", __func__, 1);

        /* Integer downsample if requested */
    if (sampling > 1) {
        pix1 = pixScaleByIntSampling(pixs1, sampling);
        pix2 = pixScaleByIntSampling(pixs2, sampling);
    } else {
        pix1 = pixClone(pixs1);
        pix2 = pixClone(pixs2);
    }

        /* Remove colormaps */
    if (pixGetColormap(pix1)) {
        pix3 = pixRemoveColormap(pix1, REMOVE_CMAP_BASED_ON_SRC);
        d1 = pixGetDepth(pix3);
    } else {
        pix3 = pixClone(pix1);
    }
    if (pixGetColormap(pix2)) {
        pix4 = pixRemoveColormap(pix2, REMOVE_CMAP_BASED_ON_SRC);
        d2 = pixGetDepth(pix4);
    } else {
        pix4 = pixClone(pix2);
    }
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    if (d1 != d2 || (d1 != 8 && d1 != 32)) {
        pixDestroy(&pix3);
        pixDestroy(&pix4);
        L_INFO("depths unequal or not in {8,32}: d1 = %d, d2 = %d\n",
               __func__, d1, d2);
        return 1;
    }

        /* In each direction, do a small dilation and subtract the dilated
         * image from the other image to get a one-sided difference.
         * Then take the max of the differences for each direction
         * and clipping each component to 255 if necessary.  Note that
         * for RGB images, the dilations and max selection are done
         * component-wise, and the conversion to grayscale also uses the
         * maximum component.  The resulting grayscale images are
         * thresholded using %mindiff. */
    if (d1 == 8) {
        pix5 = pixDilateGray(pix3, dilation, dilation);
        pixCompareGray(pix4, pix5, L_COMPARE_SUBTRACT, 0, NULL, NULL, NULL,
                       &pix7);
        pix6 = pixDilateGray(pix4, dilation, dilation);
        pixCompareGray(pix3, pix6, L_COMPARE_SUBTRACT, 0, NULL, NULL, NULL,
                       &pix8);
        pix9 = pixMinOrMax(NULL, pix7, pix8, L_CHOOSE_MAX);
        pix10 = pixThresholdToBinary(pix9, mindiff);
        pixInvert(pix10, pix10);
        pixCountPixels(pix10, &count, NULL);
        pixGetDimensions(pix10, &w, &h, NULL);
        *pfract = (w <= 0 || h <= 0) ? 0.0 :
                                       (l_float32)count / (l_float32)(w * h);
        pixDestroy(&pix5);
        pixDestroy(&pix6);
        pixDestroy(&pix7);
        pixDestroy(&pix8);
        if (ppixdiff1)
            *ppixdiff1 = pix9;
        else
            pixDestroy(&pix9);
        if (ppixdiff2)
            *ppixdiff2 = pix10;
        else
            pixDestroy(&pix10);
    } else {  /* d1 == 32 */
        pix5 = pixColorMorph(pix3, L_MORPH_DILATE, dilation, dilation);
        pixCompareRGB(pix4, pix5, L_COMPARE_SUBTRACT, 0, NULL, NULL, NULL,
                       &pix7);
        pix6 = pixColorMorph(pix4, L_MORPH_DILATE, dilation, dilation);
        pixCompareRGB(pix3, pix6, L_COMPARE_SUBTRACT, 0, NULL, NULL, NULL,
                      &pix8);
        pix9 = pixMinOrMax(NULL, pix7, pix8, L_CHOOSE_MAX);
        pix10 = pixConvertRGBToGrayMinMax(pix9, L_CHOOSE_MAX);
        pix11 = pixThresholdToBinary(pix10, mindiff);
        pixInvert(pix11, pix11);
        pixCountPixels(pix11, &count, NULL);
        pixGetDimensions(pix11, &w, &h, NULL);
        *pfract = (w <= 0 || h <= 0) ? 0.0 :
                                       (l_float32)count / (l_float32)(w * h);
        pixDestroy(&pix5);
        pixDestroy(&pix6);
        pixDestroy(&pix7);
        pixDestroy(&pix8);
        pixDestroy(&pix10);
        if (ppixdiff1)
            *ppixdiff1 = pix9;
        else
            pixDestroy(&pix9);
        if (ppixdiff2)
            *ppixdiff2 = pix11;
        else
            pixDestroy(&pix11);

    }
    pixDestroy(&pix3);
    pixDestroy(&pix4);
    return 0;
}


/*!
 * \brief   pixGetPSNR()
 *
 * \param[in]    pix1, pix2     8 or 32 bpp; no colormap
 * \param[in]    factor         sampling factor; >= 1
 * \param[out]   ppsnr          power signal/noise ratio difference
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This computes the power S/N ratio, in dB, for the difference
 *          between two images.  By convention, the power S/N
 *          for a grayscale image is ('log' == log base 10,
 *          and 'ln == log base e):
 *            PSNR = 10 * log((255/MSE)^2)
 *                 = 4.3429 * ln((255/MSE)^2)
 *                 = -4.3429 * ln((MSE/255)^2)
 *          where MSE is the mean squared error.
 *          Here are some examples:
 *             MSE             PSNR
 *             ---             ----
 *             10              28.1
 *             3               38.6
 *             1               48.1
 *             0.1             68.1
 *      (2) If pix1 and pix2 have the same pixel values, the MSE = 0.0
 *          and the PSNR is infinity.  For that case, this returns
 *          PSNR = 1000, which corresponds to the very small MSE of
 *          about 10^(-48).
 * </pre>
 */
l_ok
pixGetPSNR(PIX        *pix1,
           PIX        *pix2,
           l_int32     factor,
           l_float32  *ppsnr)
{
l_int32    same, i, j, w, h, d, wpl1, wpl2, v1, v2, r1, g1, b1, r2, g2, b2;
l_uint32  *data1, *data2, *line1, *line2;
l_float32  mse;  /* mean squared error */

    if (!ppsnr)
        return ERROR_INT("&psnr not defined", __func__, 1);
    *ppsnr = 0.0;
    if (!pix1 || !pix2)
        return ERROR_INT("empty input pix", __func__, 1);
    if (!pixSizesEqual(pix1, pix2))
        return ERROR_INT("pix sizes unequal", __func__, 1);
    if (pixGetColormap(pix1))
        return ERROR_INT("pix1 has colormap", __func__, 1);
    if (pixGetColormap(pix2))
        return ERROR_INT("pix2 has colormap", __func__, 1);
    pixGetDimensions(pix1, &w, &h, &d);
    if (d != 8 && d != 32)
        return ERROR_INT("pix not 8 or 32 bpp", __func__, 1);
    if (factor < 1)
        return ERROR_INT("invalid sampling factor", __func__, 1);

    pixEqual(pix1, pix2, &same);
    if (same) {
        *ppsnr = 1000.0;  /* crazy big exponent */
        return 0;
    }

    data1 = pixGetData(pix1);
    data2 = pixGetData(pix2);
    wpl1 = pixGetWpl(pix1);
    wpl2 = pixGetWpl(pix2);
    mse = 0.0;
    if (d == 8) {
        for (i = 0; i < h; i += factor) {
            line1 = data1 + i * wpl1;
            line2 = data2 + i * wpl2;
            for (j = 0; j < w; j += factor) {
                v1 = GET_DATA_BYTE(line1, j);
                v2 = GET_DATA_BYTE(line2, j);
                mse += (l_float32)(v1 - v2) * (v1 - v2);
            }
        }
    } else {  /* d == 32 */
        for (i = 0; i < h; i += factor) {
            line1 = data1 + i * wpl1;
            line2 = data2 + i * wpl2;
            for (j = 0; j < w; j += factor) {
                extractRGBValues(line1[j], &r1, &g1, &b1);
                extractRGBValues(line2[j], &r2, &g2, &b2);
                mse += ((l_float32)(r1 - r2) * (r1 - r2) +
                        (g1 - g2) * (g1 - g2) +
                        (b1 - b2) * (b1 - b2)) / 3.0;
            }
        }
    }
    mse = mse / ((l_float32)(w) * h);

    *ppsnr = -4.3429448 * log(mse / (255 * 255));
    return 0;
}


/*------------------------------------------------------------------*
 *             Comparison of photo regions by histogram             *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixaComparePhotoRegionsByHisto()
 *
 * \param[in]    pixa        any depth; colormap OK
 * \param[in]    minratio    requiring sizes be compatible; < 1.0
 * \param[in]    textthresh  threshold for text/photo; use 0 for default
 * \param[in]    factor      subsampling; >= 1
 * \param[in]    n           in range {1, ... 7}. n^2 is the maximum number
 *                           of subregions for histograms; typ. n = 3.
 * \param[in]    simthresh   threshold for similarity; use 0 for default
 * \param[out]   pnai array  giving similarity class indices
 * \param[out]   pscores     [optional] score matrix as 1-D array of size N^2
 * \param[out]   ppixd       [optional] pix of similarity classes
 * \param[in]    debug       1 to output histograms; 0 otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function takes a pixa of cropped photo images and
 *          compares each one to the others for similarity.
 *          Each image is first tested to see if it is a photo that can
 *          be compared by tiled histograms.  If so, it is padded to put
 *          the centroid in the center of the image, and the histograms
 *          are generated.  The final step of comparing each histogram
 *          with all the others is very fast.
 *      (2) To make the histograms, each image is subdivided in a maximum
 *          of n^2 subimages.  The parameter %n specifies the "side" of
 *          an n x n grid of such subimages.  If the subimages have an
 *          aspect ratio larger than 2, the grid will change, again using n^2
 *          as a maximum for the number of subimages.  For example,
 *          if n == 3, but the image is 600 x 200 pixels, a 3x3 grid
 *          would have subimages of 200 x 67 pixels, which is more
 *          than 2:1, so we change to a 4x2 grid where each subimage
 *          has 150 x 100 pixels.
 *      (3) An initial filter gives %score = 0 if the ratio of widths
 *          and heights (smallest / largest) does not exceed a
 *          threshold %minratio.  If set at 1.0, both images must be
 *          exactly the same size.  A typical value for %minratio is 0.9.
 *      (4) The comparison score between two images is a value in [0.0 .. 1.0].
 *          If the comparison score >= %simthresh, the images are placed in
 *          the same similarity class.  Default value for %simthresh is 0.25.
 *      (5) An array %nai of similarity class indices for pix in the
 *          input pixa is returned.
 *      (6) There are two debugging options:
 *          * An optional 2D matrix of scores is returned as a 1D array.
 *            A visualization of this is written to a temp file.
 *          * An optional pix showing the similarity classes can be
 *            returned.  Text in each input pix is reproduced.
 *      (7) See the notes in pixComparePhotoRegionsByHisto() for details
 *          on the implementation.
 * </pre>
 */
l_ok
pixaComparePhotoRegionsByHisto(PIXA        *pixa,
                               l_float32    minratio,
                               l_float32    textthresh,
                               l_int32      factor,
                               l_int32      n,
                               l_float32    simthresh,
                               NUMA       **pnai,
                               l_float32  **pscores,
                               PIX        **ppixd,
                               l_int32      debug)
{
char       *text;
l_int32     i, j, nim, w, h, w1, h1, w2, h2, ival, index, classid;
l_float32   score;
l_float32  *scores;
NUMA       *nai, *naw, *nah;
NUMAA      *naa;
NUMAA     **n3a;  /* array of naa */
PIX        *pix;

    if (pscores) *pscores = NULL;
    if (ppixd) *ppixd = NULL;
    if (!pnai)
        return ERROR_INT("&na not defined", __func__, 1);
    *pnai = NULL;
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (minratio < 0.0 || minratio > 1.0)
        return ERROR_INT("minratio not in [0.0 ... 1.0]", __func__, 1);
    if (textthresh <= 0.0) textthresh = 1.3f;
    if (factor < 1)
        return ERROR_INT("subsampling factor must be >= 1", __func__, 1);
    if (n < 1 || n > 7) {
        L_WARNING("n = %d is invalid; setting to 4\n", __func__, n);
        n = 4;
    }
    if (simthresh <= 0.0) simthresh = 0.25;
    if (simthresh > 1.0)
        return ERROR_INT("simthresh invalid; should be near 0.25", __func__, 1);

        /* Prepare the histograms */
    nim = pixaGetCount(pixa);
    if ((n3a = (NUMAA **)LEPT_CALLOC(nim, sizeof(NUMAA *))) == NULL)
        return ERROR_INT("calloc fail for n3a", __func__, 1);
    naw = numaCreate(0);
    nah = numaCreate(0);
    for (i = 0; i < nim; i++) {
        pix = pixaGetPix(pixa, i, L_CLONE);
        text = pixGetText(pix);
        pixSetResolution(pix, 150, 150);
        index = (debug) ? i : 0;
        pixGenPhotoHistos(pix, NULL, factor, textthresh, n,
                          &naa, &w, &h, index);
        n3a[i] = naa;
        numaAddNumber(naw, w);
        numaAddNumber(nah, h);
        if (naa)
            lept_stderr("Image %s is photo\n", text);
        else
            lept_stderr("Image %s is NOT photo\n", text);
        pixDestroy(&pix);
    }

        /* Do the comparisons.  We are making a set of classes, where
         * all similar images are placed in the same class.  There are
         * 'nim' input images.  The classes are labeled by 'classid' (all
         * similar images get the same 'classid' value), and 'nai' maps
         * the classid of the image in the input array to the classid
         * of the similarity class.  */
    if ((scores =
               (l_float32 *)LEPT_CALLOC((size_t)nim * nim, sizeof(l_float32)))
                == NULL) {
        L_ERROR("calloc fail for scores\n", __func__);
        goto cleanup;
    }
    nai = numaMakeConstant(-1, nim);  /* classid array */
    for (i = 0, classid = 0; i < nim; i++) {
        scores[nim * i + i] = 1.0;
        numaGetIValue(nai, i, &ival);
        if (ival != -1)  /* already set */
            continue;
        numaSetValue(nai, i, classid);
        if (n3a[i] == NULL) {  /* not a photo */
            classid++;
            continue;
        }
        numaGetIValue(naw, i, &w1);
        numaGetIValue(nah, i, &h1);
        for (j = i + 1; j < nim; j++) {
            numaGetIValue(nai, j, &ival);
            if (ival != -1)  /* already set */
                continue;
            if (n3a[j] == NULL)  /* not a photo */
                continue;
            numaGetIValue(naw, j, &w2);
            numaGetIValue(nah, j, &h2);
            compareTilesByHisto(n3a[i], n3a[j], minratio, w1, h1, w2, h2,
                                &score, NULL);
            scores[nim * i + j] = score;
            scores[nim * j + i] = score;  /* the score array is symmetric */
/*            lept_stderr("score = %5.3f\n", score); */
            if (score > simthresh) {
                numaSetValue(nai, j, classid);
                lept_stderr(
                        "Setting %d similar to %d, in class %d; score %5.3f\n",
                        j, i, classid, score);
            }
        }
        classid++;
    }
    *pnai = nai;

        /* Debug: optionally save and display the score array.
         * All images that are photos are represented by a point on
         * the diagonal. Other images in the same similarity class
         * are on the same horizontal raster line to the right.
         * The array has been symmetrized, so images in the same
         * same similarity class also appear on the same column below. */
    if (pscores) {
        l_int32    wpl, fact;
        l_uint32  *line, *data;
        PIX       *pix2, *pix3;
        pix2 = pixCreate(nim, nim, 8);
        data = pixGetData(pix2);
        wpl = pixGetWpl(pix2);
        for (i = 0; i < nim; i++) {
            line = data + i * wpl;
            for (j = 0; j < nim; j++) {
                SET_DATA_BYTE(line, j,
                              L_MIN(255, 4.0 * 255 * scores[nim * i + j]));
            }
        }
        fact = L_MAX(2, 1000 / nim);
        pix3 = pixExpandReplicate(pix2, fact);
        lept_stderr("Writing to /tmp/lept/comp/scorearray.png\n");
        lept_mkdir("lept/comp");
        pixWrite("/tmp/lept/comp/scorearray.png", pix3, IFF_PNG);
        pixDestroy(&pix2);
        pixDestroy(&pix3);
        *pscores = scores;
    } else {
        LEPT_FREE(scores);
    }

        /* Debug: optionally display and save the image comparisons.
         * Image similarity classes are displayed by column; similar
         * images are displayed in the same column. */
    if (ppixd)
        *ppixd = pixaDisplayTiledByIndex(pixa, nai, 200, 20, 2, 6, 0x0000ff00);

cleanup:
    numaDestroy(&naw);
    numaDestroy(&nah);
    for (i = 0; i < nim; i++)
        numaaDestroy(&n3a[i]);
    LEPT_FREE(n3a);
    return 0;
}


/*!
 * \brief   pixComparePhotoRegionsByHisto()
 *
 * \param[in]    pix1, pix2    any depth; colormap OK
 * \param[in]    box1, box2    [optional] photo regions from each; can be null
 * \param[in]    minratio      requiring sizes be compatible; < 1.0
 * \param[in]    factor        subsampling factor; >= 1
 * \param[in]    n             in range {1, ... 7}. n^2 is the maximum number
 *                             of subregions for histograms; typ. n = 3.
 * \param[out]   pscore        similarity score of histograms
 * \param[in]    debugflag     1 for debug output; 0 for no debugging
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function compares two grayscale photo regions.  If a
 *          box is given, the region is clipped; otherwise assume
 *          the entire images are photo regions.  This is done with a
 *          set of not more than n^2 spatially aligned histograms, which are
 *          aligned using the centroid of the inverse image.
 *      (2) The parameter %n specifies the "side" of an n x n grid
 *          of subimages.  If the subimages have an aspect ratio larger
 *          than 2, the grid will change, using n^2 as a maximum for
 *          the number of subimages.  For example, if n == 3, but the
 *          image is 600 x 200 pixels, a 3x3 grid would have subimages
 *          of 200 x 67 pixels, which is more than 2:1, so we change
 *          to a 4x2 grid where each subimage has 150 x 100 pixels.
 *      (3) An initial filter gives %score = 0 if the ratio of widths
 *          and heights (smallest / largest) does not exceed a
 *          threshold %minratio.  This must be between 0.5 and 1.0.
 *          If set at 1.0, both images must be exactly the same size.
 *          A typical value for %minratio is 0.9.
 *      (4) Because this function should not be used on text or
 *          line graphics, which can give false positive results
 *          (i.e., high scores for different images), filter the images
 *          using pixGenPhotoHistos(), which returns tiled histograms
 *          only if an image is not text and comparison is expected
 *          to work with histograms.  If either image fails the test,
 *          the comparison returns a score of 0.0.
 *      (5) The white value counts in the histograms are removed; they
 *          are typically pixels that were padded to achieve alignment.
 *      (6) For an efficient representation of the histogram, normalize
 *          using a multiplicative factor so that the number in the
 *          maximum bucket is 255.  It then takes 256 bytes to store.
 *      (7) When comparing the histograms of two regions, use the
 *          Earth Mover distance (EMD), with the histograms normalized
 *          so that the sum over bins is the same.  Further normalize
 *          by dividing by 255, so that the result is in [0.0 ... 1.0].
 *      (8) Get a similarity score S = 1.0 - k * D, where
 *            k is a constant, say in the range 5-10
 *            D = normalized EMD
 *          and for multiple tiles, take the Min(S) to be the final score.
 *          Using aligned tiles gives protection against accidental
 *          similarity of the overall grayscale histograms.
 *          A small number of aligned tiles works well.
 *      (9) With debug on, you get a pdf that shows, for each tile,
 *          the images, histograms and score.
 * </pre>
 */
l_ok
pixComparePhotoRegionsByHisto(PIX        *pix1,
                              PIX        *pix2,
                              BOX        *box1,
                              BOX        *box2,
                              l_float32   minratio,
                              l_int32     factor,
                              l_int32     n,
                              l_float32  *pscore,
                              l_int32     debugflag)
{
l_int32    w1, h1, w2, h2, w1c, h1c, w2c, h2c, debugindex;
l_float32  wratio, hratio;
NUMAA     *naa1, *naa2;
PIX       *pix3, *pix4;
PIXA      *pixa;

    if (!pscore)
        return ERROR_INT("&score not defined", __func__, 1);
    *pscore = 0.0;
    if (!pix1 || !pix2)
        return ERROR_INT("pix1 and pix2 not both defined", __func__, 1);
    if (minratio < 0.5 || minratio > 1.0)
        return ERROR_INT("minratio not in [0.5 ... 1.0]", __func__, 1);
    if (factor < 1)
        return ERROR_INT("subsampling factor must be >= 1", __func__, 1);
    if (n < 1 || n > 7) {
        L_WARNING("n = %d is invalid; setting to 4\n", __func__, n);
        n = 4;
    }

    debugindex = 0;
    if (debugflag) {
        lept_mkdir("lept/comp");
        debugindex = 666;  /* arbitrary number used for naming output */
    }

        /* Initial filter by size */
    if (box1)
        boxGetGeometry(box1, NULL, NULL, &w1, &h1);
    else
        pixGetDimensions(pix1, &w1, &h1, NULL);
    if (box2)
        boxGetGeometry(box2, NULL, NULL, &w2, &h2);
    else
        pixGetDimensions(pix1, &w2, &h2, NULL);
    wratio = (w1 < w2) ? (l_float32)w1 / (l_float32)w2 :
             (l_float32)w2 / (l_float32)w1;
    hratio = (h1 < h2) ? (l_float32)h1 / (l_float32)h2 :
             (l_float32)h2 / (l_float32)h1;
    if (wratio < minratio || hratio < minratio)
        return 0;

        /* Initial crop, if necessary, and make histos */
    if (box1)
        pix3 = pixClipRectangle(pix1, box1, NULL);
    else
        pix3 = pixClone(pix1);
    pixGenPhotoHistos(pix3, NULL, factor, 0, n, &naa1, &w1c, &h1c, debugindex);
    pixDestroy(&pix3);
    if (!naa1) return 0;
    if (box2)
        pix4 = pixClipRectangle(pix2, box2, NULL);
    else
        pix4 = pixClone(pix2);
    pixGenPhotoHistos(pix4, NULL, factor, 0, n, &naa2, &w2c, &h2c, debugindex);
    pixDestroy(&pix4);
    if (!naa2) return 0;

        /* Compare histograms */
    pixa = (debugflag) ? pixaCreate(0) : NULL;
    compareTilesByHisto(naa1, naa2, minratio, w1c, h1c, w2c, h2c, pscore, pixa);
    pixaDestroy(&pixa);
    return 0;
}


/*!
 * \brief   pixGenPhotoHistos()
 *
 * \param[in]    pixs      depth > 1 bpp; colormap OK
 * \param[in]    box       [optional] region to be selected; can be null
 * \param[in]    factor    subsampling; >= 1
 * \param[in]    thresh    threshold for photo/text; use 0 for default
 * \param[in]    n         in range {1, ... 7}. n^2 is the maximum number
 *                         of subregions for histograms; typ. n = 3.
 * \param[out]   pnaa      nx * ny 256-entry gray histograms
 * \param[out]   pw        width of image used to make histograms
 * \param[out]   ph        height of image used to make histograms
 * \param[in]    debugindex  0 for no debugging; positive integer otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This crops and converts to 8 bpp if necessary.  It adds a
 *          minimal white boundary such that the centroid of the
 *          photo-inverted image is in the center. This allows
 *          automatic alignment with histograms of other image regions.
 *      (2) The parameter %n specifies the "side" of the n x n grid
 *          of subimages.  If the subimages have an aspect ratio larger
 *          than 2, the grid will change, using n^2 as a maximum for
 *          the number of subimages.  For example, if n == 3, but the
 *          image is 600 x 200 pixels, a 3x3 grid would have subimages
 *          of 200 x 67 pixels, which is more than 2:1, so we change
 *          to a 4x2 grid where each subimage has 150 x 100 pixels.
 *      (3) The white value in the histogram is removed, because of
 *          the padding.
 *      (4) Use 0 for conservative default (1.3) for thresh.
 *      (5) For an efficient representation of the histogram, normalize
 *          using a multiplicative factor so that the number in the
 *          maximum bucket is 255.  It then takes 256 bytes to store.
 *      (6) With %debugindex > 0, this makes a pdf that shows, for each tile,
 *          the images and histograms.
 * </pre>
 */
l_ok
pixGenPhotoHistos(PIX        *pixs,
                  BOX        *box,
                  l_int32     factor,
                  l_float32   thresh,
                  l_int32     n,
                  NUMAA     **pnaa,
                  l_int32    *pw,
                  l_int32    *ph,
                  l_int32     debugindex)
{
char    buf[64];
NUMAA  *naa;
PIX    *pix1, *pix2, *pix3, *pixm;
PIXA   *pixa;

    if (pnaa) *pnaa = NULL;
    if (pw) *pw = 0;
    if (ph) *ph = 0;
    if (!pnaa)
        return ERROR_INT("&naa not defined", __func__, 1);
    if (!pw || !ph)
        return ERROR_INT("&w and &h not both defined", __func__, 1);
    if (!pixs || pixGetDepth(pixs) == 1)
        return ERROR_INT("pixs not defined or 1 bpp", __func__, 1);
    if (factor < 1)
        return ERROR_INT("subsampling factor must be >= 1", __func__, 1);
    if (thresh <= 0.0) thresh = 1.3f;  /* default */
    if (n < 1 || n > 7) {
        L_WARNING("n = %d is invalid; setting to 4\n", __func__, n);
        n = 4;
    }

    pixa = NULL;
    if (debugindex > 0) {
        pixa = pixaCreate(0);
        lept_mkdir("lept/comp");
    }

        /* Initial crop, if necessary */
    if (box)
        pix1 = pixClipRectangle(pixs, box, NULL);
    else
        pix1 = pixClone(pixs);

        /* Convert to 8 bpp and pad to center the centroid */
    pix2 = pixConvertTo8(pix1, FALSE);
    pix3 = pixPadToCenterCentroid(pix2, factor);

        /* Set to 255 all pixels above 230.  Do this so that light gray
         * pixels do not enter into the comparison. */
    pixm = pixThresholdToBinary(pix3, 230);
    pixInvert(pixm, pixm);
    pixSetMaskedGeneral(pix3, pixm, 255, 0, 0);
    pixDestroy(&pixm);

    if (debugindex > 0) {
        PIX   *pix4, *pix5, *pix6, *pix7, *pix8;
        PIXA  *pixa2;
        pix4 = pixConvertTo32(pix2);
        pix5 = pixConvertTo32(pix3);
        pix6 = pixScaleToSize(pix4, 400, 0);
        pix7 = pixScaleToSize(pix5, 400, 0);
        pixa2 = pixaCreate(2);
        pixaAddPix(pixa2, pix6, L_INSERT);
        pixaAddPix(pixa2, pix7, L_INSERT);
        pix8 = pixaDisplayTiledInRows(pixa2, 32, 1000, 1.0, 0, 50, 3);
        pixaAddPix(pixa, pix8, L_INSERT);
        pixDestroy(&pix4);
        pixDestroy(&pix5);
        pixaDestroy(&pixa2);
    }
    pixDestroy(&pix1);
    pixDestroy(&pix2);

        /* Test if this is a photoimage */
    pixDecideIfPhotoImage(pix3, factor, thresh, n, &naa, pixa);
    if (naa) {
        *pnaa = naa;
        *pw = pixGetWidth(pix3);
        *ph = pixGetHeight(pix3);
    }

    if (pixa) {
        snprintf(buf, sizeof(buf), "/tmp/lept/comp/tiledhistos.%d.pdf",
                 debugindex);
        lept_stderr("Writing to %s\n", buf);
        pixaConvertToPdf(pixa, 300, 1.0, L_FLATE_ENCODE, 0, NULL, buf);
        pixaDestroy(&pixa);
    }

    pixDestroy(&pix3);
    return 0;
}


/*!
 * \brief   pixPadToCenterCentroid()
 *
 * \param[in]    pixs     any depth, colormap OK
 * \param[in]    factor   subsampling for centroid; >= 1
 * \return  pixd padded with white pixels, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) This add minimum white padding to an 8 bpp pix, such that
 *          the centroid of the photometric inverse is in the center of
 *          the resulting image.  Thus in computing the centroid,
 *          black pixels have weight 255, and white pixels have weight 0.
 * </pre>
 */
PIX *
pixPadToCenterCentroid(PIX     *pixs,
                       l_int32  factor)

{
l_float32  cx, cy;
l_int32    xs, ys, delx, dely, icx, icy, ws, hs, wd, hd;
PIX       *pix1, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (factor < 1)
        return (PIX *)ERROR_PTR("invalid sampling factor", __func__, NULL);

    pix1 = pixConvertTo8(pixs, FALSE);
    pixCentroid8(pix1, factor, &cx, &cy);
    icx = (l_int32)(cx + 0.5);
    icy = (l_int32)(cy + 0.5);
    pixGetDimensions(pix1, &ws, &hs, NULL);
    delx = ws - 2 * icx;
    dely = hs - 2 * icy;
    xs = L_MAX(0, delx);
    ys = L_MAX(0, dely);
    wd = 2 * L_MAX(icx, ws - icx);
    hd = 2 * L_MAX(icy, hs - icy);
    pixd = pixCreate(wd, hd, 8);
    pixSetAll(pixd);  /* to white */
    pixCopyResolution(pixd, pixs);
    pixRasterop(pixd, xs, ys, ws, hs, PIX_SRC, pix1, 0, 0);
    pixDestroy(&pix1);
    return pixd;
}


/*!
 * \brief   pixCentroid8()
 *
 * \param[in]    pixs    8 bpp
 * \param[in]    factor  subsampling factor; >= 1
 * \param[out]   pcx     x value of centroid
 * \param[out]   pcy     y value of centroid
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This first does a photometric inversion (black = 255, white = 0).
 *          It then finds the centroid of the result.  The inversion is
 *          done because white is usually background, so the centroid
 *          is computed based on the "foreground" gray pixels, and the
 *          darker the pixel, the more weight it is given.
 * </pre>
 */
l_ok
pixCentroid8(PIX        *pixs,
             l_int32     factor,
             l_float32  *pcx,
             l_float32  *pcy)
{
l_int32    i, j, w, h, wpl, val;
l_float32  sumx, sumy, sumv;
l_uint32  *data, *line;
PIX       *pix1;

    if (pcx) *pcx = 0.0;
    if (pcy) *pcy = 0.0;
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs undefined or not 8 bpp", __func__, 1);
    if (factor < 1)
        return ERROR_INT("subsampling factor must be >= 1", __func__, 1);
    if (!pcx || !pcy)
        return ERROR_INT("&cx and &cy not both defined", __func__, 1);

    pix1 = pixInvert(NULL, pixs);
    pixGetDimensions(pix1, &w, &h, NULL);
    data = pixGetData(pix1);
    wpl = pixGetWpl(pix1);
    sumx = sumy = sumv = 0.0;
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        for (j = 0; j < w; j++) {
            val = GET_DATA_BYTE(line, j);
            sumx += val * j;
            sumy += val * i;
            sumv += val;
        }
    }
    pixDestroy(&pix1);

    if (sumv == 0) {
        L_INFO("input image is white\n", __func__);
        *pcx = (l_float32)(w) / 2;
        *pcy = (l_float32)(h) / 2;
    } else {
        *pcx = sumx / sumv;
        *pcy = sumy / sumv;
    }

    return 0;
}


/*!
 * \brief   pixDecideIfPhotoImage()
 *
 * \param[in]    pix         8 bpp, centroid in center
 * \param[in]    factor      subsampling for histograms; >= 1
 * \param[in]    thresh      threshold for photo/text; use 0 for default
 * \param[in]    n           in range {1, ... 7}. n^2 is the maximum number
 *                           of subregions for histograms; typ. n = 3.
 * \param[out]   pnaa        array of normalized histograms
 * \param[in]    pixadebug   [optional] use only for debug output
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The input image must be 8 bpp (no colormap), and padded with
 *          white pixels so the centroid of photo-inverted pixels is at
 *          the center of the image.
 *      (2) The parameter %n specifies the "side" of the n x n grid
 *          of subimages.  If the subimages have an aspect ratio larger
 *          than 2, the grid will change, using n^2 as a maximum for
 *          the number of subimages.  For example, if n == 3, but the
 *          image is 600 x 200 pixels, a 3x3 grid would have subimages
 *          of 200 x 67 pixels, which is more than 2:1, so we change
 *          to a 4x2 grid where each subimage has 150 x 100 pixels.
 *      (3) If the pix is not almost certainly a photoimage, the returned
 *          histograms (%naa) are null.
 *      (4) If histograms are generated, the white (255) count is set
 *          to 0.  This removes all pixels values above 230, including
 *          white padding from the centroid matching operation, from
 *          consideration.  The resulting histograms are then normalized
 *          so the maximum count is 255.
 *      (5) Default for %thresh is 1.3; this seems sufficiently conservative.
 *      (6) Use %pixadebug == NULL unless debug output is requested.
 * </pre>
 */
l_ok
pixDecideIfPhotoImage(PIX       *pix,
                      l_int32    factor,
                      l_float32  thresh,
                      l_int32    n,
                      NUMAA    **pnaa,
                      PIXA      *pixadebug)
{
char       buf[64];
l_int32    i, w, h, nx, ny, ngrids, istext, isphoto;
l_float32  maxval, sum1, sum2, ratio;
L_BMF     *bmf;
NUMA      *na1, *na2, *na3, *narv;
NUMAA     *naa;
PIX       *pix1;
PIXA      *pixa1, *pixa2, *pixa3;

    if (!pnaa)
        return ERROR_INT("&naa not defined", __func__, 1);
    *pnaa = NULL;
    if (!pix || pixGetDepth(pix) != 8 || pixGetColormap(pix))
        return ERROR_INT("pix undefined or invalid", __func__, 1);
    if (n < 1 || n > 7) {
        L_WARNING("n = %d is invalid; setting to 4\n", __func__, n);
        n = 4;
    }
    if (thresh <= 0.0) thresh = 1.3f;  /* default */

        /* Look for text lines */
    pixDecideIfText(pix, NULL, &istext, pixadebug);
    if (istext) {
        L_INFO("Image is text\n", __func__);
        return 0;
    }

        /* Determine grid from n */
    pixGetDimensions(pix, &w, &h, NULL);
    if (w == 0 || h == 0)
        return ERROR_INT("invalid pix dimension", __func__, 1);
    findHistoGridDimensions(n, w, h, &nx, &ny, 1);

        /* Evaluate histograms in each tile */
    pixa1 = pixaSplitPix(pix, nx, ny, 0, 0);
    ngrids = nx * ny;
    bmf = (pixadebug) ? bmfCreate(NULL, 6) : NULL;
    naa = numaaCreate(ngrids);
    if (pixadebug) {
        lept_rmdir("lept/compplot");
        lept_mkdir("lept/compplot");
    }
    for (i = 0; i < ngrids; i++) {
        pix1 = pixaGetPix(pixa1, i, L_CLONE);

            /* Get histograms, set white count to 0, normalize max to 255 */
        na1 = pixGetGrayHistogram(pix1, factor);
        numaSetValue(na1, 255, 0);
        na2 = numaWindowedMean(na1, 5);  /* do some smoothing */
        numaGetMax(na2, &maxval, NULL);
        na3 = numaTransform(na2, 0, 255.0 / maxval);
        if (pixadebug) {
            snprintf(buf, sizeof(buf), "/tmp/lept/compplot/plot.%d", i);
            gplotSimple1(na3, GPLOT_PNG, buf, "Histos");
        }

        numaaAddNuma(naa, na3, L_INSERT);
        numaDestroy(&na1);
        numaDestroy(&na2);
        pixDestroy(&pix1);
    }
    if (pixadebug) {
        pix1 = pixaDisplayTiledInColumns(pixa1, nx, 1.0, 30, 2);
        pixaAddPix(pixadebug, pix1, L_INSERT);
        pixa2 = pixaReadFiles("/tmp/lept/compplot", ".png");
        pixa3 = pixaScale(pixa2, 0.4f, 0.4f);
        pix1 = pixaDisplayTiledInColumns(pixa3, nx, 1.0, 30, 2);
        pixaAddPix(pixadebug, pix1, L_INSERT);
        pixaDestroy(&pixa2);
        pixaDestroy(&pixa3);
    }

        /* Compute the standard deviation between these histos to decide
         * if the image is photo or something more like line art,
         * which does not support good comparison by tiled histograms.  */
    grayInterHistogramStats(naa, 5, NULL, NULL, NULL, &narv);

        /* For photos, the root variance has a larger weight of
         * values in the range [50 ... 150] compared to [200 ... 230],
         * than text or line art.  For the latter, most of the variance
         * between tiles is in the lightest parts of the image, well
         * above 150.  */
    numaGetSumOnInterval(narv, 50, 150, &sum1);
    numaGetSumOnInterval(narv, 200, 230, &sum2);
    if (sum2 == 0.0) {  /* shouldn't happen */
        ratio = 0.001f;  /* anything very small for debug output */
        isphoto = 0;  /* be conservative */
    } else {
        ratio = sum1 / sum2;
        isphoto = (ratio > thresh) ? 1 : 0;
    }
    if (pixadebug) {
        if (isphoto)
            L_INFO("ratio %f > %f; isphoto is true\n",
                   __func__, ratio, thresh);
        else
            L_INFO("ratio %f < %f; isphoto is false\n",
                   __func__, ratio, thresh);
    }
    if (isphoto)
        *pnaa = naa;
    else
        numaaDestroy(&naa);
    bmfDestroy(&bmf);
    numaDestroy(&narv);
    pixaDestroy(&pixa1);
    return 0;
}


/*!
 * \brief   findHistoGridDimensions()
 *
 * \param[in]    n         max number of grid elements is n^2; typ. n = 3
 * \param[in]    w         width of image to be subdivided
 * \param[in]    h         height of image to be subdivided
 * \param[out]   pnx       number of grid elements in x direction
 * \param[out]   pny       number of grid elements in y direction
 * \param[in]    debug     1 for debug output to stderr
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This determines the number of subdivisions to be used on
 *          the image in each direction.  A histogram will be built
 *          for each subimage.
 *      (2) The parameter %n specifies the "side" of the n x n grid
 *          of subimages.  If the subimages have an aspect ratio larger
 *          than 2, the grid will change, using n^2 as a maximum for
 *          the number of subimages.  For example, if n == 3, but the
 *          image is 600 x 200 pixels, a 3x3 grid would have subimages
 *          of 200 x 67 pixels, which is more than 2:1, so we change
 *          to a 4x2 grid where each subimage has 150 x 100 pixels.
 * </pre>
 */
static l_ok
findHistoGridDimensions(l_int32   n,
                        l_int32   w,
                        l_int32   h,
                        l_int32  *pnx,
                        l_int32  *pny,
                        l_int32   debug)
{
l_int32    nx, ny, max;
l_float32  ratio;

    ratio = (l_float32)w / (l_float32)h;
    max = n * n;
    nx = ny = n;
    while (nx > 1 && ny > 1) {
        if (ratio > 2.0) {  /* reduce ny */
            ny--;
            nx = max / ny;
            if (debug)
                lept_stderr("nx = %d, ny = %d, ratio w/h = %4.2f\n",
                            nx, ny, ratio);
        } else if (ratio < 0.5) {  /* reduce nx */
            nx--;
            ny = max / nx;
            if (debug)
                lept_stderr("nx = %d, ny = %d, ratio w/h = %4.2f\n",
                            nx, ny, ratio);
        } else {  /* we're ok */
            if (debug)
                lept_stderr("nx = %d, ny = %d, ratio w/h = %4.2f\n",
                            nx, ny, ratio);
            break;
        }
        ratio = (l_float32)(ny * w) / (l_float32)(nx * h);
    }
    *pnx = nx;
    *pny = ny;
    return 0;
}


/*!
 * \brief   compareTilesByHisto()
 *
 * \param[in]    naa1, naa2      each is a set of 256 entry histograms
 * \param[in]    minratio        requiring image sizes be compatible; < 1.0
 * \param[in]    w1, h1, w2, h2  image sizes from which histograms were made
 * \param[out]   pscore          similarity score of histograms
 * \param[in]    pixadebug       [optional] use only for debug output
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) naa1 and naa2 must be generated using pixGenPhotoHistos(),
 *          using the same tile sizes.
 *      (2) The image dimensions must be similar.  The score is 0.0
 *          if the ratio of widths and heights (smallest / largest)
 *          exceeds a threshold %minratio, which must be between
 *          0.5 and 1.0.  If set at 1.0, both images must be exactly
 *          the same size.  A typical value for %minratio is 0.9.
 *      (3) The input pixadebug is null unless debug output is requested.
 * </pre>
 */
l_ok
compareTilesByHisto(NUMAA      *naa1,
                    NUMAA      *naa2,
                    l_float32   minratio,
                    l_int32     w1,
                    l_int32     h1,
                    l_int32     w2,
                    l_int32     h2,
                    l_float32  *pscore,
                    PIXA       *pixadebug)
{
char       buf1[128], buf2[128];
l_int32    i, n;
l_float32  wratio, hratio, score, minscore, dist;
L_BMF     *bmf;
NUMA      *na1, *na2, *nadist, *nascore;

    if (!pscore)
        return ERROR_INT("&score not defined", __func__, 1);
    *pscore = 0.0;
    if (!naa1 || !naa2)
        return ERROR_INT("naa1 and naa2 not both defined", __func__, 1);

        /* Filter for different sizes */
    wratio = (w1 < w2) ? (l_float32)w1 / (l_float32)w2 :
             (l_float32)w2 / (l_float32)w1;
    hratio = (h1 < h2) ? (l_float32)h1 / (l_float32)h2 :
             (l_float32)h2 / (l_float32)h1;
    if (wratio < minratio || hratio < minratio) {
        if (pixadebug)
            L_INFO("Sizes differ: wratio = %f, hratio = %f\n",
                   __func__, wratio, hratio);
        return 0;
    }
    n = numaaGetCount(naa1);
    if (n != numaaGetCount(naa2)) {  /* due to differing w/h ratio */
        L_INFO("naa1 and naa2 sizes are different\n", __func__);
        return 0;
    }

    if (pixadebug) {
        lept_rmdir("lept/comptile");
        lept_mkdir("lept/comptile");
    }


        /* Evaluate histograms in each tile.  Remove white before
         * computing EMD, because there are may be a lot of white
         * pixels due to padding, and we don't want to include them.
         * This also makes the debug histo plots more informative. */
    minscore = 1.0;
    nadist = numaCreate(n);
    nascore = numaCreate(n);
    bmf = (pixadebug) ? bmfCreate(NULL, 6) : NULL;
    for (i = 0; i < n; i++) {
        na1 = numaaGetNuma(naa1, i, L_CLONE);
        na2 = numaaGetNuma(naa2, i, L_CLONE);
        numaSetValue(na1, 255, 0.0);
        numaSetValue(na2, 255, 0.0);

            /* To compare histograms, use the normalized earthmover distance.
             * Further normalize to get the EM distance as a fraction of the
             * maximum distance in the histogram (255).  Finally, scale this
             * up by 10.0, and subtract from 1.0 to get a similarity score. */
        numaEarthMoverDistance(na1, na2, &dist);
        score = L_MAX(0.0, 1.0 - 10.0 * (dist / 255.));
        numaAddNumber(nadist, dist);
        numaAddNumber(nascore, score);
        minscore = L_MIN(minscore, score);
        if (pixadebug) {
            snprintf(buf1, sizeof(buf1), "/tmp/lept/comptile/plot.%d", i);
            gplotSimple2(na1, na2, GPLOT_PNG, buf1, "Histos");
        }
        numaDestroy(&na1);
        numaDestroy(&na2);
    }
    *pscore = minscore;

    if (pixadebug) {
        for (i = 0; i < n; i++) {
            PIX  *pix1, *pix2;
            snprintf(buf1, sizeof(buf1), "/tmp/lept/comptile/plot.%d.png", i);
            pix1 = pixRead(buf1);
            numaGetFValue(nadist, i, &dist);
            numaGetFValue(nascore, i, &score);
            snprintf(buf2, sizeof(buf2),
                 "Image %d\ndist = %5.3f, score = %5.3f", i, dist, score);
            pix2 = pixAddTextlines(pix1, bmf, buf2, 0x0000ff00, L_ADD_BELOW);
            pixaAddPix(pixadebug, pix2, L_INSERT);
            pixDestroy(&pix1);
        }
        lept_stderr("Writing to /tmp/lept/comptile/comparegray.pdf\n");
        pixaConvertToPdf(pixadebug, 300, 1.0, L_FLATE_ENCODE, 0, NULL,
                         "/tmp/lept/comptile/comparegray.pdf");
        numaWriteDebug("/tmp/lept/comptile/scores.na", nascore);
        numaWriteDebug("/tmp/lept/comptile/dists.na", nadist);
    }

    bmfDestroy(&bmf);
    numaDestroy(&nadist);
    numaDestroy(&nascore);
    return 0;
}


/*!
 * \brief   pixCompareGrayByHisto()
 *
 * \param[in]    pix1, pix2  any depth; colormap OK
 * \param[in]    box1, box2  [optional] region selected from each; can be null
 * \param[in]    minratio    requiring sizes be compatible; < 1.0
 * \param[in]    maxgray     max value to keep in histo; >= 200, 255 to keep all
 * \param[in]    factor      subsampling factor; >= 1
 * \param[in]    n           in range {1, ... 7}. n^2 is the maximum number
 *                           of subregions for histograms; typ. n = 3.
 * \param[out]   pscore      similarity score of histograms
 * \param[in]    debugflag   1 for debug output; 0 for no debugging
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function compares two grayscale photo regions.  It can
 *          do it with a single histogram from each region, or with a
 *          set of spatially aligned histograms.  For both cases,
 *          align the regions using the centroid of the inverse image,
 *          and crop to the smallest of the two.
 *      (2) The parameter %n specifies the "side" of an n x n grid
 *          of subimages.  If the subimages have an aspect ratio larger
 *          than 2, the grid will change, using n^2 as a maximum for
 *          the number of subimages.  For example, if n == 3, but the
 *          image is 600 x 200 pixels, a 3x3 grid would have subimages
 *          of 200 x 67 pixels, which is more than 2:1, so we change
 *          to a 4x2 grid where each subimage has 150 x 100 pixels.
 *      (3) An initial filter gives %score = 0 if the ratio of widths
 *          and heights (smallest / largest) does not exceed a
 *          threshold %minratio.  This must be between 0.5 and 1.0.
 *          If set at 1.0, both images must be exactly the same size.
 *          A typical value for %minratio is 0.9.
 *      (4) The lightest values in the histogram can be disregarded.
 *          Set %maxgray to the lightest value to be kept.  For example,
 *          to eliminate white (255), set %maxgray = 254.  %maxgray must
 *          be >= 200.
 *      (5) For an efficient representation of the histogram, normalize
 *          using a multiplicative factor so that the number in the
 *          maximum bucket is 255.  It then takes 256 bytes to store.
 *      (6) When comparing the histograms of two regions:
 *          ~ Use %maxgray = 254 to ignore the white pixels, the number
 *            of which may be sensitive to the crop region if the pixels
 *            outside that region are white.
 *          ~ Use the Earth Mover distance (EMD), with the histograms
 *            normalized so that the sum over bins is the same.
 *            Further normalize by dividing by 255, so that the result
 *            is in [0.0 ... 1.0].
 *      (7) Get a similarity score S = 1.0 - k * D, where
 *            k is a constant, say in the range 5-10
 *            D = normalized EMD
 *          and for multiple tiles, take the Min(S) to be the final score.
 *          Using aligned tiles gives protection against accidental
 *          similarity of the overall grayscale histograms.
 *          A small number of aligned tiles works well.
 *      (8) With debug on, you get a pdf that shows, for each tile,
 *          the images, histograms and score.
 *      (9) When to use:
 *          (a) Because this function should not be used on text or
 *              line graphics, which can give false positive results
 *              (i.e., high scores for different images), the input
 *              images should be filtered.
 *          (b) To filter, first use pixDecideIfText().  If that function
 *              says the image is text, do not use it.  If the function
 *              says it is not text, it still may be line graphics, and
 *              in that case, use:
 *                 pixGetGrayHistogramTiled()
 *                 grayInterHistogramStats()
 *              to determine whether it is photo or line graphics.
 * </pre>
 */
l_ok
pixCompareGrayByHisto(PIX        *pix1,
                      PIX        *pix2,
                      BOX        *box1,
                      BOX        *box2,
                      l_float32   minratio,
                      l_int32     maxgray,
                      l_int32     factor,
                      l_int32     n,
                      l_float32  *pscore,
                      l_int32     debugflag)
{
l_int32    w1, h1, w2, h2;
l_float32  wratio, hratio;
BOX       *box3, *box4;
PIX       *pix3, *pix4, *pix5, *pix6, *pix7, *pix8;
PIXA      *pixa;

    if (!pscore)
        return ERROR_INT("&score not defined", __func__, 1);
    *pscore = 0.0;
    if (!pix1 || !pix2)
        return ERROR_INT("pix1 and pix2 not both defined", __func__, 1);
    if (minratio < 0.5 || minratio > 1.0)
        return ERROR_INT("minratio not in [0.5 ... 1.0]", __func__, 1);
    if (maxgray < 200)
        return ERROR_INT("invalid maxgray; should be >= 200", __func__, 1);
    maxgray = L_MIN(255, maxgray);
    if (factor < 1)
        return ERROR_INT("subsampling factor must be >= 1", __func__, 1);
    if (n < 1 || n > 7) {
        L_WARNING("n = %d is invalid; setting to 4\n", __func__, n);
        n = 4;
    }

    if (debugflag)
        lept_mkdir("lept/comp");

        /* Initial filter by size */
    if (box1)
        boxGetGeometry(box1, NULL, NULL, &w1, &h1);
    else
        pixGetDimensions(pix1, &w1, &h1, NULL);
    if (box2)
        boxGetGeometry(box2, NULL, NULL, &w2, &h2);
    else
        pixGetDimensions(pix1, &w2, &h2, NULL);
    wratio = (w1 < w2) ? (l_float32)w1 / (l_float32)w2 :
             (l_float32)w2 / (l_float32)w1;
    hratio = (h1 < h2) ? (l_float32)h1 / (l_float32)h2 :
             (l_float32)h2 / (l_float32)h1;
    if (wratio < minratio || hratio < minratio)
        return 0;

        /* Initial crop, if necessary */
    if (box1)
        pix3 = pixClipRectangle(pix1, box1, NULL);
    else
        pix3 = pixClone(pix1);
    if (box2)
        pix4 = pixClipRectangle(pix2, box2, NULL);
    else
        pix4 = pixClone(pix2);

        /* Convert to 8 bpp, align centroids and do maximal crop */
    pix5 = pixConvertTo8(pix3, FALSE);
    pix6 = pixConvertTo8(pix4, FALSE);
    pixCropAlignedToCentroid(pix5, pix6, factor, &box3, &box4);
    pix7 = pixClipRectangle(pix5, box3, NULL);
    pix8 = pixClipRectangle(pix6, box4, NULL);
    pixa = (debugflag) ? pixaCreate(0) : NULL;
    if (debugflag) {
        PIX     *pix9, *pix10, *pix11, *pix12, *pix13;
        PIXA    *pixa2;
        pix9 = pixConvertTo32(pix5);
        pix10 = pixConvertTo32(pix6);
        pixRenderBoxArb(pix9, box3, 2, 255, 0, 0);
        pixRenderBoxArb(pix10, box4, 2, 255, 0, 0);
        pix11 = pixScaleToSize(pix9, 400, 0);
        pix12 = pixScaleToSize(pix10, 400, 0);
        pixa2 = pixaCreate(2);
        pixaAddPix(pixa2, pix11, L_INSERT);
        pixaAddPix(pixa2, pix12, L_INSERT);
        pix13 = pixaDisplayTiledInRows(pixa2, 32, 1000, 1.0, 0, 50, 0);
        pixaAddPix(pixa, pix13, L_INSERT);
        pixDestroy(&pix9);
        pixDestroy(&pix10);
        pixaDestroy(&pixa2);
    }
    pixDestroy(&pix3);
    pixDestroy(&pix4);
    pixDestroy(&pix5);
    pixDestroy(&pix6);
    boxDestroy(&box3);
    boxDestroy(&box4);

        /* Tile and compare histograms */
    pixCompareTilesByHisto(pix7, pix8, maxgray, factor, n, pscore, pixa);
    pixaDestroy(&pixa);
    pixDestroy(&pix7);
    pixDestroy(&pix8);
    return 0;
}


/*!
 * \brief   pixCompareTilesByHisto()
 *
 * \param[in]    pix1, pix2     8 bpp
 * \param[in]    maxgray        max value to keep in histo; 255 to keep all
 * \param[in]    factor         subsampling factor; >= 1
 * \param[in]    n              see pixCompareGrayByHisto()
 * \param[out]   pscore         similarity score of histograms
 * \param[in]    pixadebug      [optional] use only for debug output
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This static function is only called from pixCompareGrayByHisto().
 *          The input images have been converted to 8 bpp if necessary,
 *          aligned and cropped.
 *      (2) The input pixadebug is null unless debug output is requested.
 *      (3) See pixCompareGrayByHisto() for details.
 * </pre>
 */
static l_ok
pixCompareTilesByHisto(PIX        *pix1,
                       PIX        *pix2,
                       l_int32     maxgray,
                       l_int32     factor,
                       l_int32     n,
                       l_float32  *pscore,
                       PIXA       *pixadebug)
{
char       buf[64];
l_int32    w, h, i, j, nx, ny, ngr;
l_float32  score, minscore, maxval1, maxval2, dist;
L_BMF     *bmf;
NUMA      *na1, *na2, *na3, *na4, *na5, *na6, *na7;
PIX       *pix3, *pix4;
PIXA      *pixa1, *pixa2;

    if (!pscore)
        return ERROR_INT("&score not defined", __func__, 1);
    *pscore = 0.0;
    if (!pix1 || !pix2)
        return ERROR_INT("pix1 and pix2 not both defined", __func__, 1);

        /* Determine grid from n */
    pixGetDimensions(pix1, &w, &h, NULL);
    findHistoGridDimensions(n, w, h, &nx, &ny, 1);
    ngr = nx * ny;

        /* Evaluate histograms in each tile */
    pixa1 = pixaSplitPix(pix1, nx, ny, 0, 0);
    pixa2 = pixaSplitPix(pix2, nx, ny, 0, 0);
    na7 = (pixadebug) ? numaCreate(ngr) : NULL;
    bmf = (pixadebug) ? bmfCreate(NULL, 6) : NULL;
    minscore = 1.0;
    for (i = 0; i < ngr; i++) {
        pix3 = pixaGetPix(pixa1, i, L_CLONE);
        pix4 = pixaGetPix(pixa2, i, L_CLONE);

            /* Get histograms, set white count to 0, normalize max to 255 */
        na1 = pixGetGrayHistogram(pix3, factor);
        na2 = pixGetGrayHistogram(pix4, factor);
        if (maxgray < 255) {
            for (j = maxgray + 1; j <= 255; j++) {
                numaSetValue(na1, j, 0);
                numaSetValue(na2, j, 0);
            }
        }
        na3 = numaWindowedMean(na1, 5);
        na4 = numaWindowedMean(na2, 5);
        numaGetMax(na3, &maxval1, NULL);
        numaGetMax(na4, &maxval2, NULL);
        na5 = numaTransform(na3, 0, 255.0 / maxval1);
        na6 = numaTransform(na4, 0, 255.0 / maxval2);
        if (pixadebug) {
            gplotSimple2(na5, na6, GPLOT_PNG, "/tmp/lept/comp/plot1", "Histos");
        }

            /* To compare histograms, use the normalized earthmover distance.
             * Further normalize to get the EM distance as a fraction of the
             * maximum distance in the histogram (255).  Finally, scale this
             * up by 10.0, and subtract from 1.0 to get a similarity score. */
        numaEarthMoverDistance(na5, na6, &dist);
        score = L_MAX(0.0, 1.0 - 8.0 * (dist / 255.));
        if (pixadebug) numaAddNumber(na7, score);
        minscore = L_MIN(minscore, score);
        if (pixadebug) {
            PIX     *pix5, *pix6, *pix7, *pix8, *pix9, *pix10;
            PIXA    *pixa3;
            l_int32  w, h, wscale;
            pixa3 = pixaCreate(3);
            pixGetDimensions(pix3, &w, &h, NULL);
            wscale = (w > h) ? 700 : 400;
            pix5 = pixScaleToSize(pix3, wscale, 0);
            pix6 = pixScaleToSize(pix4, wscale, 0);
            pixaAddPix(pixa3, pix5, L_INSERT);
            pixaAddPix(pixa3, pix6, L_INSERT);
            pix7 = pixRead("/tmp/lept/comp/plot1.png");
            pix8 = pixScaleToSize(pix7, 700, 0);
            snprintf(buf, sizeof(buf), "%5.3f", score);
            pix9 = pixAddTextlines(pix8, bmf, buf, 0x0000ff00, L_ADD_RIGHT);
            pixaAddPix(pixa3, pix9, L_INSERT);
            pix10 = pixaDisplayTiledInRows(pixa3, 32, 1000, 1.0, 0, 50, 0);
            pixaAddPix(pixadebug, pix10, L_INSERT);
            pixDestroy(&pix7);
            pixDestroy(&pix8);
            pixaDestroy(&pixa3);
        }
        numaDestroy(&na1);
        numaDestroy(&na2);
        numaDestroy(&na3);
        numaDestroy(&na4);
        numaDestroy(&na5);
        numaDestroy(&na6);
        pixDestroy(&pix3);
        pixDestroy(&pix4);
    }
    *pscore = minscore;

    if (pixadebug) {
        pixaConvertToPdf(pixadebug, 300, 1.0, L_FLATE_ENCODE, 0, NULL,
                         "/tmp/lept/comp/comparegray.pdf");
        numaWriteDebug("/tmp/lept/comp/tilescores.na", na7);
    }

    bmfDestroy(&bmf);
    numaDestroy(&na7);
    pixaDestroy(&pixa1);
    pixaDestroy(&pixa2);
    return 0;
}


/*!
 * \brief   pixCropAlignedToCentroid()
 *
 * \param[in]    pix1, pix2   any depth; colormap OK
 * \param[in]    factor       subsampling; >= 1
 * \param[out]   pbox1        crop box for pix1
 * \param[out]   pbox2        crop box for pix2
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This finds the maximum crop boxes for two 8 bpp images when
 *          their centroids of their photometric inverses are aligned.
 *          Black pixels have weight 255; white pixels have weight 0.
 * </pre>
 */
l_ok
pixCropAlignedToCentroid(PIX     *pix1,
                         PIX     *pix2,
                         l_int32  factor,
                         BOX    **pbox1,
                         BOX    **pbox2)
{
l_float32  cx1, cy1, cx2, cy2;
l_int32    w1, h1, w2, h2, icx1, icy1, icx2, icy2;
l_int32    xm, xm1, xm2, xp, xp1, xp2, ym, ym1, ym2, yp, yp1, yp2;
PIX       *pix3, *pix4;

    if (pbox1) *pbox1 = NULL;
    if (pbox2) *pbox2 = NULL;
    if (!pix1 || !pix2)
        return ERROR_INT("pix1 and pix2 not both defined", __func__, 1);
    if (factor < 1)
        return ERROR_INT("subsampling factor must be >= 1", __func__, 1);
    if (!pbox1 || !pbox2)
        return ERROR_INT("&box1 and &box2 not both defined", __func__, 1);

    pix3 = pixConvertTo8(pix1, FALSE);
    pix4 = pixConvertTo8(pix2, FALSE);
    pixCentroid8(pix3, factor, &cx1, &cy1);
    pixCentroid8(pix4, factor, &cx2, &cy2);
    pixGetDimensions(pix3, &w1, &h1, NULL);
    pixGetDimensions(pix4, &w2, &h2, NULL);
    pixDestroy(&pix3);
    pixDestroy(&pix4);

    icx1 = (l_int32)(cx1 + 0.5);
    icy1 = (l_int32)(cy1 + 0.5);
    icx2 = (l_int32)(cx2 + 0.5);
    icy2 = (l_int32)(cy2 + 0.5);
    xm = L_MIN(icx1, icx2);
    xm1 = icx1 - xm;
    xm2 = icx2 - xm;
    xp = L_MIN(w1 - icx1, w2 - icx2);  /* one pixel beyond to the right */
    xp1 = icx1 + xp;
    xp2 = icx2 + xp;
    ym = L_MIN(icy1, icy2);
    ym1 = icy1 - ym;
    ym2 = icy2 - ym;
    yp = L_MIN(h1 - icy1, h2 - icy2);  /* one pixel below the bottom */
    yp1 = icy1 + yp;
    yp2 = icy2 + yp;
    *pbox1 = boxCreate(xm1, ym1, xp1 - xm1, yp1 - ym1);
    *pbox2 = boxCreate(xm2, ym2, xp2 - xm2, yp2 - ym2);
    return 0;
}


/*!
 * \brief   l_compressGrayHistograms()
 *
 * \param[in]    naa     set of 256-entry histograms
 * \param[in]    w, h    size of image
 * \param[out]   psize   size of byte array
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This first writes w and h to the byte array as 4 byte ints.
 *      (2) Then it normalizes each histogram to a max value of 255,
 *          and saves each value as a byte.  If there are
 *          N histograms, the output bytearray has 8 + 256 * N bytes.
 *      (3) Further compression of the array with zlib yields only about
 *          a 25% decrease in size, so we don't bother.  If size reduction
 *          were important, a lossy transform using a 1-dimensional DCT
 *          would be effective, because we don't care about the fine
 *          details of these histograms.
 * </pre>
 */
l_uint8 *
l_compressGrayHistograms(NUMAA   *naa,
                         l_int32  w,
                         l_int32  h,
                         size_t  *psize)
{
l_uint8   *bytea;
l_int32    i, j, n, nn, ival;
l_float32  maxval;
NUMA      *na1, *na2;

    if (!psize)
        return (l_uint8 *)ERROR_PTR("&size not defined", __func__, NULL);
    *psize = 0;
    if (!naa)
        return (l_uint8 *)ERROR_PTR("naa not defined", __func__, NULL);
    n = numaaGetCount(naa);
    for (i = 0; i < n; i++) {
        nn = numaaGetNumaCount(naa, i);
        if (nn != 256) {
            L_ERROR("%d numbers in numa[%d]\n", __func__, nn, i);
            return NULL;
        }
    }

    if ((bytea = (l_uint8 *)LEPT_CALLOC(8 + 256 * n, sizeof(l_uint8))) == NULL)
        return (l_uint8 *)ERROR_PTR("bytea not made", __func__, NULL);
    *psize = 8 + 256 * n;
    l_setDataFourBytes(bytea, 0, w);
    l_setDataFourBytes(bytea, 1, h);
    for (i = 0; i < n; i++) {
        na1 = numaaGetNuma(naa, i, L_COPY);
        numaGetMax(na1, &maxval, NULL);
        na2 = numaTransform(na1, 0, 255.0 / maxval);
        for (j = 0; j < 256; j++) {
            numaGetIValue(na2, j, &ival);
            bytea[8 + 256 * i + j] = ival;
        }
        numaDestroy(&na1);
        numaDestroy(&na2);
    }

    return bytea;
}


/*!
 * \brief   l_uncompressGrayHistograms()
 *
 * \param[in]    bytea    byte array of size 8 + 256 * N, N an integer
 * \param[in]    size     size of byte array
 * \param[out]   pw       width of the image that generated the histograms
 * \param[out]   ph       height of the image
 * \return  numaa     representing N histograms, each with 256 bins,
 *                    or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) The first 8 bytes are read as two 32-bit ints.
 *      (2) Then this constructs a numaa representing some number of
 *          gray histograms that are normalized such that the max value
 *          in each histogram is 255.  The data is stored as a byte
 *          array, with 256 bytes holding the data for each histogram.
 *          Each gray histogram was computed from a tile of a grayscale image.
 * </pre>
 */
NUMAA *
l_uncompressGrayHistograms(l_uint8  *bytea,
                           size_t    size,
                           l_int32  *pw,
                           l_int32  *ph)
{
l_int32  i, j, n;
NUMA    *na;
NUMAA   *naa;

    if (pw) *pw = 0;
    if (ph) *ph = 0;
    if (!pw || !ph)
        return (NUMAA *)ERROR_PTR("&w and &h not both defined", __func__, NULL);
    if (!bytea)
        return (NUMAA *)ERROR_PTR("bytea not defined", __func__, NULL);
    n = (size - 8) / 256;
    if ((size - 8) % 256 != 0)
        return (NUMAA *)ERROR_PTR("bytea size is invalid", __func__, NULL);

    *pw = l_getDataFourBytes(bytea, 0);
    *ph = l_getDataFourBytes(bytea, 1);
    naa = numaaCreate(n);
    for (i = 0; i < n; i++) {
        na = numaCreate(256);
        for (j = 0; j < 256; j++)
            numaAddNumber(na, bytea[8 + 256 * i + j]);
        numaaAddNuma(naa, na, L_INSERT);
    }

    return naa;
}


/*------------------------------------------------------------------*
 *             Translated images at the same resolution             *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixCompareWithTranslation()
 *
 * \param[in]    pix1, pix2    any depth; colormap OK
 * \param[in]    thresh        threshold for converting to 1 bpp
 * \param[out]   pdelx         x translation on pix2 to align with pix1
 * \param[out]   pdely         y translation on pix2 to align with pix1
 * \param[out]   pscore        correlation score at best alignment
 * \param[in]    debugflag     1 for debug output; 0 for no debugging
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This does a coarse-to-fine search for best translational
 *          alignment of two images, measured by a scoring function
 *          that is the correlation between the fg pixels.
 *      (2) The threshold is used if the images aren't 1 bpp.
 *      (3) With debug on, you get a pdf that shows, as a grayscale
 *          image, the score as a function of shift from the initial
 *          estimate, for each of the four levels.  The shift is 0 at
 *          the center of the image.
 *      (4) With debug on, you also get a pdf that shows the
 *          difference at the best alignment between the two images,
 *          at each of the four levels.  The red and green pixels
 *          show locations where one image has a fg pixel and the
 *          other doesn't.  The black pixels are where both images
 *          have fg pixels, and white pixels are where neither image
 *          has fg pixels.
 * </pre>
 */
l_ok
pixCompareWithTranslation(PIX        *pix1,
                          PIX        *pix2,
                          l_int32     thresh,
                          l_int32    *pdelx,
                          l_int32    *pdely,
                          l_float32  *pscore,
                          l_int32     debugflag)
{
l_uint8   *subtab;
l_int32    i, level, area1, area2, delx, dely;
l_int32    etransx, etransy, maxshift, dbint;
l_int32   *stab, *ctab;
l_float32  cx1, cx2, cy1, cy2, score;
PIX       *pixb1, *pixb2, *pixt1, *pixt2, *pixt3, *pixt4;
PIXA      *pixa1, *pixa2, *pixadb = NULL;

    if (pdelx) *pdelx = 0;
    if (pdely) *pdely = 0;
    if (pscore) *pscore = 0.0;
    if (!pdelx || !pdely)
        return ERROR_INT("&delx and &dely not defined", __func__, 1);
    if (!pscore)
        return ERROR_INT("&score not defined", __func__, 1);
    if (!pix1)
        return ERROR_INT("pix1 not defined", __func__, 1);
    if (!pix2)
        return ERROR_INT("pix2 not defined", __func__, 1);

        /* Make tables */
    subtab = makeSubsampleTab2x();
    stab = makePixelSumTab8();
    ctab = makePixelCentroidTab8();

        /* Binarize each image */
    pixb1 = pixConvertTo1(pix1, thresh);
    pixb2 = pixConvertTo1(pix2, thresh);

        /* Make a cascade of 2x reduced images for each, thresholding
         * with level 2 (neutral), down to 8x reduction */
    pixa1 = pixaCreate(4);
    pixa2 = pixaCreate(4);
    if (debugflag)
        pixadb = pixaCreate(4);
    pixaAddPix(pixa1, pixb1, L_INSERT);
    pixaAddPix(pixa2, pixb2, L_INSERT);
    for (i = 0; i < 3; i++) {
        pixt1 = pixReduceRankBinary2(pixb1, 2, subtab);
        pixt2 = pixReduceRankBinary2(pixb2, 2, subtab);
        pixaAddPix(pixa1, pixt1, L_INSERT);
        pixaAddPix(pixa2, pixt2, L_INSERT);
        pixb1 = pixt1;
        pixb2 = pixt2;
    }

        /* At the lowest level, use the centroids with a maxshift of 6
         * to search for the best alignment.  Then at higher levels,
         * use the result from the level below as the initial approximation
         * for the alignment, and search with a maxshift of 2. */
    for (level = 3; level >= 0; level--) {
        pixt1 = pixaGetPix(pixa1, level, L_CLONE);
        pixt2 = pixaGetPix(pixa2, level, L_CLONE);
        pixCountPixels(pixt1, &area1, stab);
        pixCountPixels(pixt2, &area2, stab);
        if (level == 3) {
            pixCentroid(pixt1, ctab, stab, &cx1, &cy1);
            pixCentroid(pixt2, ctab, stab, &cx2, &cy2);
            etransx = lept_roundftoi(cx1 - cx2);
            etransy = lept_roundftoi(cy1 - cy2);
            maxshift = 6;
        } else {
            etransx = 2 * delx;
            etransy = 2 * dely;
            maxshift = 2;
        }
        dbint = (debugflag) ? level + 1 : 0;
        pixBestCorrelation(pixt1, pixt2, area1, area2, etransx, etransy,
                           maxshift, stab, &delx, &dely, &score, dbint);
        if (debugflag) {
            lept_stderr("Level %d: delx = %d, dely = %d, score = %7.4f\n",
                        level, delx, dely, score);
            pixRasteropIP(pixt2, delx, dely, L_BRING_IN_WHITE);
            pixt3 = pixDisplayDiffBinary(pixt1, pixt2);
            pixt4 = pixExpandReplicate(pixt3, 8 / (1 << (3 - level)));
            pixaAddPix(pixadb, pixt4, L_INSERT);
            pixDestroy(&pixt3);
        }
        pixDestroy(&pixt1);
        pixDestroy(&pixt2);
    }

    if (debugflag) {
        pixaConvertToPdf(pixadb, 300, 1.0, L_FLATE_ENCODE, 0, NULL,
                         "/tmp/lept/comp/compare.pdf");
        convertFilesToPdf("/tmp/lept/comp", "correl_", 30, 1.0, L_FLATE_ENCODE,
                          0, "Correlation scores at levels 1 through 5",
                          "/tmp/lept/comp/correl.pdf");
        pixaDestroy(&pixadb);
    }

    *pdelx = delx;
    *pdely = dely;
    *pscore = score;
    pixaDestroy(&pixa1);
    pixaDestroy(&pixa2);
    LEPT_FREE(subtab);
    LEPT_FREE(stab);
    LEPT_FREE(ctab);
    return 0;
}


/*!
 * \brief   pixBestCorrelation()
 *
 * \param[in]    pix1      1 bpp
 * \param[in]    pix2      1 bpp
 * \param[in]    area1     number of on pixels in pix1
 * \param[in]    area2     number of on pixels in pix2
 * \param[in]    etransx   estimated x translation of pix2 to align with pix1
 * \param[in]    etransy   estimated y translation of pix2 to align with pix1
 * \param[in]    maxshift  max x and y shift of pix2, around the estimated
 *                         alignment location, relative to pix1
 * \param[in]    tab8      [optional] sum tab for ON pixels in byte; can be NULL
 * \param[out]   pdelx     [optional] best x shift of pix2 relative to pix1
 * \param[out]   pdely     [optional] best y shift of pix2 relative to pix1
 * \param[out]   pscore    [optional] maximum score found; can be NULL
 * \param[in]    debugflag   <= 0 to skip; positive to generate output.
 *                           The integer is used to label the debug image.
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This maximizes the correlation score between two 1 bpp images,
 *          by starting with an estimate of the alignment
 *          (%etransx, %etransy) and computing the correlation around this.
 *          It optionally returns the shift (%delx, %dely) that maximizes
 *          the correlation score when pix2 is shifted by this amount
 *          relative to pix1.
 *      (2) Get the centroids of pix1 and pix2, using pixCentroid(),
 *          to compute (%etransx, %etransy).  Get the areas using
 *          pixCountPixels().
 *      (3) The centroid of pix2 is shifted with respect to the centroid
 *          of pix1 by all values between -maxshiftx and maxshiftx,
 *          and likewise for the y shifts.  Therefore, the number of
 *          correlations computed is:
 *               (2 * maxshiftx + 1) * (2 * maxshifty + 1)
 *          Consequently, if pix1 and pix2 are large, you should do this
 *          in a coarse-to-fine sequence.  See the use of this function
 *          in pixCompareWithTranslation().
 * </pre>
 */
l_ok
pixBestCorrelation(PIX        *pix1,
                   PIX        *pix2,
                   l_int32     area1,
                   l_int32     area2,
                   l_int32     etransx,
                   l_int32     etransy,
                   l_int32     maxshift,
                   l_int32    *tab8,
                   l_int32    *pdelx,
                   l_int32    *pdely,
                   l_float32  *pscore,
                   l_int32     debugflag)
{
l_int32    shiftx, shifty, delx, dely;
l_int32   *tab;
l_float32  maxscore, score;
FPIX      *fpix = NULL;
PIX       *pix3, *pix4;

    if (pdelx) *pdelx = 0;
    if (pdely) *pdely = 0;
    if (pscore) *pscore = 0.0;
    if (!pix1 || pixGetDepth(pix1) != 1)
        return ERROR_INT("pix1 not defined or not 1 bpp", __func__, 1);
    if (!pix2 || pixGetDepth(pix2) != 1)
        return ERROR_INT("pix2 not defined or not 1 bpp", __func__, 1);
    if (!area1 || !area2)
        return ERROR_INT("areas must be > 0", __func__, 1);

    if (debugflag > 0)
        fpix = fpixCreate(2 * maxshift + 1, 2 * maxshift + 1);

    if (!tab8)
        tab = makePixelSumTab8();
    else
        tab = tab8;

        /* Search over a set of {shiftx, shifty} for the max */
    maxscore = 0;
    delx = etransx;
    dely = etransy;
    for (shifty = -maxshift; shifty <= maxshift; shifty++) {
        for (shiftx = -maxshift; shiftx <= maxshift; shiftx++) {
            pixCorrelationScoreShifted(pix1, pix2, area1, area2,
                                       etransx + shiftx,
                                       etransy + shifty, tab, &score);
            if (debugflag > 0) {
                fpixSetPixel(fpix, maxshift + shiftx, maxshift + shifty,
                             1000.0 * score);
/*                lept_stderr("(sx, sy) = (%d, %d): score = %6.4f\n",
                              shiftx, shifty, score); */
            }
            if (score > maxscore) {
                maxscore = score;
                delx = etransx + shiftx;
                dely = etransy + shifty;
            }
        }
    }

    if (debugflag > 0) {
        char  buf[128];
        lept_mkdir("lept/comp");
        pix3 = fpixDisplayMaxDynamicRange(fpix);
        pix4 = pixExpandReplicate(pix3, 20);
        snprintf(buf, sizeof(buf), "/tmp/lept/comp/correl_%d.png",
                 debugflag);
        pixWrite(buf, pix4, IFF_PNG);
        pixDestroy(&pix3);
        pixDestroy(&pix4);
        fpixDestroy(&fpix);
    }

    if (pdelx) *pdelx = delx;
    if (pdely) *pdely = dely;
    if (pscore) *pscore = maxscore;
    if (!tab8) LEPT_FREE(tab);
    return 0;
}

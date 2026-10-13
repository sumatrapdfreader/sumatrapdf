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
 * \file pix4.c
 * <pre>
 *
 *    This file has these operations:
 *
 *      (1) Pixel histograms
 *      (2) Pixel row/column statistics
 *      (3) Foreground/background estimation
 *
 *    Pixel histogram, rank val, averaging and min/max
 *           NUMA       *pixGetGrayHistogram()
 *           NUMA       *pixGetGrayHistogramMasked()
 *           NUMA       *pixGetGrayHistogramInRect()
 *           NUMAA      *pixGetGrayHistogramTiled()
 *           l_int32     pixGetColorHistogram()
 *           l_int32     pixGetColorHistogramMasked()
 *           NUMA       *pixGetCmapHistogram()
 *           NUMA       *pixGetCmapHistogramMasked()
 *           NUMA       *pixGetCmapHistogramInRect()
 *           l_int32     pixCountRGBColorsByHash()
 *           l_int32     pixCountRGBColors()
 *           L_AMAP     *pixGetColorAmapHistogram()
 *           l_int32     amapGetCountForColor()
 *           l_int32     pixGetRankValue()
 *           l_int32     pixGetRankValueMaskedRGB()
 *           l_int32     pixGetRankValueMasked()
 *           l_int32     pixGetPixelAverage()
 *           l_int32     pixGetPixelStats()
 *           l_int32     pixGetAverageMaskedRGB()
 *           l_int32     pixGetAverageMasked()
 *           l_int32     pixGetAverageTiledRGB()
 *           PIX        *pixGetAverageTiled()
 *           NUMA       *pixRowStats()
 *           NUMA       *pixColumnStats()
 *           l_int32     pixGetRangeValues()
 *           l_int32     pixGetExtremeValue()
 *           l_int32     pixGetMaxValueInRect()
 *           l_int32     pixGetMaxColorIndex()
 *           l_int32     pixGetBinnedComponentRange()
 *           l_int32     pixGetRankColorArray()
 *           l_int32     pixGetBinnedColor()
 *           PIX        *pixDisplayColorArray()
 *           PIX        *pixRankBinByStrip()
 *
 *    Pixelwise aligned statistics
 *           PIX        *pixaGetAlignedStats()
 *           l_int32     pixaExtractColumnFromEachPix()
 *           l_int32     pixGetRowStats()
 *           l_int32     pixGetColumnStats()
 *           l_int32     pixSetPixelColumn()
 *
 *    Foreground/background estimation
 *           l_int32     pixThresholdForFgBg()
 *           l_int32     pixSplitDistributionFgBg()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>
#include "allheaders.h"


/*------------------------------------------------------------------*
 *                  Pixel histogram and averaging                   *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixGetGrayHistogram()
 *
 * \param[in]   pixs     1, 2, 4, 8, 16 bpp; can be colormapped
 * \param[in]   factor   subsampling factor; integer >= 1
 * \return  na histogram, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If pixs has a colormap, it is converted to 8 bpp gray.
 *          If you want a histogram of the colormap indices, use
 *          pixGetCmapHistogram().
 *      (2) If pixs does not have a colormap, the output histogram is
 *          of size 2^d, where d is the depth of pixs.
 *      (3) Set the subsampling factor > 1 to reduce the amount of computation.
 * </pre>
 */
NUMA *
pixGetGrayHistogram(PIX     *pixs,
                    l_int32  factor)
{
l_int32     i, j, w, h, d, wpl, val, size, count;
l_uint32   *data, *line;
l_float32  *array;
NUMA       *na;
PIX        *pixg;

    if (!pixs)
        return (NUMA *)ERROR_PTR("pixs not defined", __func__, NULL);
    d = pixGetDepth(pixs);
    if (d > 16)
        return (NUMA *)ERROR_PTR("depth not in {1,2,4,8,16}", __func__, NULL);
    if (factor < 1)
        return (NUMA *)ERROR_PTR("sampling must be >= 1", __func__, NULL);

    if (pixGetColormap(pixs))
        pixg = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    else
        pixg = pixClone(pixs);

    pixGetDimensions(pixg, &w, &h, &d);
    size = 1 << d;
    if ((na = numaCreate(size)) == NULL) {
        pixDestroy(&pixg);
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    }
    numaSetCount(na, size);  /* all initialized to 0.0 */
    array = numaGetFArray(na, L_NOCOPY);

    if (d == 1) {  /* special case */
        pixCountPixels(pixg, &count, NULL);
        array[0] = w * h - count;
        array[1] = count;
        pixDestroy(&pixg);
        return na;
    }

    wpl = pixGetWpl(pixg);
    data = pixGetData(pixg);
    for (i = 0; i < h; i += factor) {
        line = data + i * wpl;
        if (d == 2) {
            for (j = 0; j < w; j += factor) {
                val = GET_DATA_DIBIT(line, j);
                array[val] += 1.0;
            }
        } else if (d == 4) {
            for (j = 0; j < w; j += factor) {
                val = GET_DATA_QBIT(line, j);
                array[val] += 1.0;
            }
        } else if (d == 8) {
            for (j = 0; j < w; j += factor) {
                val = GET_DATA_BYTE(line, j);
                array[val] += 1.0;
            }
        } else {  /* d == 16 */
            for (j = 0; j < w; j += factor) {
                val = GET_DATA_TWO_BYTES(line, j);
                array[val] += 1.0;
            }
        }
    }

    pixDestroy(&pixg);
    return na;
}


/*!
 * \brief   pixGetGrayHistogramMasked()
 *
 * \param[in]   pixs     8 bpp, or colormapped
 * \param[in]   pixm     [optional] 1 bpp mask over which histogram is
 *                       to be computed; use all pixels if null
 * \param[in]   x, y     UL corner of pixm relative to the UL corner of pixs;
 *                       can be < 0; these values are ignored if pixm is null
 * \param[in]   factor   subsampling factor; integer >= 1
 * \return  na histogram, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If pixs is cmapped, it is converted to 8 bpp gray.
 *          If you want a histogram of the colormap indices, use
 *          pixGetCmapHistogramMasked().
 *      (2) This always returns a 256-value histogram of pixel values.
 *      (3) Set the subsampling factor > 1 to reduce the amount of computation.
 *      (4) Clipping of pixm (if it exists) to pixs is done in the inner loop.
 *      (5) Input x,y are ignored unless pixm exists.
 * </pre>
 */
NUMA *
pixGetGrayHistogramMasked(PIX        *pixs,
                          PIX        *pixm,
                          l_int32     x,
                          l_int32     y,
                          l_int32     factor)
{
l_int32     i, j, w, h, wm, hm, dm, wplg, wplm, val;
l_uint32   *datag, *datam, *lineg, *linem;
l_float32  *array;
NUMA       *na;
PIX        *pixg;

    if (!pixm)
        return pixGetGrayHistogram(pixs, factor);
    if (!pixs)
        return (NUMA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8 && !pixGetColormap(pixs))
        return (NUMA *)ERROR_PTR("pixs neither 8 bpp nor colormapped",
                                 __func__, NULL);
    pixGetDimensions(pixm, &wm, &hm, &dm);
    if (dm != 1)
        return (NUMA *)ERROR_PTR("pixm not 1 bpp", __func__, NULL);
    if (factor < 1)
        return (NUMA *)ERROR_PTR("sampling must be >= 1", __func__, NULL);

    if ((na = numaCreate(256)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    numaSetCount(na, 256);  /* all initialized to 0.0 */
    array = numaGetFArray(na, L_NOCOPY);

    if (pixGetColormap(pixs))
        pixg = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    else
        pixg = pixClone(pixs);
    pixGetDimensions(pixg, &w, &h, NULL);
    datag = pixGetData(pixg);
    wplg = pixGetWpl(pixg);
    datam = pixGetData(pixm);
    wplm = pixGetWpl(pixm);

        /* Generate the histogram */
    for (i = 0; i < hm; i += factor) {
        if (y + i < 0 || y + i >= h) continue;
        lineg = datag + (y + i) * wplg;
        linem = datam + i * wplm;
        for (j = 0; j < wm; j += factor) {
            if (x + j < 0 || x + j >= w) continue;
            if (GET_DATA_BIT(linem, j)) {
                val = GET_DATA_BYTE(lineg, x + j);
                array[val] += 1.0;
            }
        }
    }

    pixDestroy(&pixg);
    return na;
}


/*!
 * \brief   pixGetGrayHistogramInRect()
 *
 * \param[in]   pixs    8 bpp, or colormapped
 * \param[in]   box     [optional] over which histogram is to be computed;
 *                      use full image if NULL
 * \param[in]   factor  subsampling factor; integer >= 1
 * \return  na histogram, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If pixs is cmapped, it is converted to 8 bpp gray.
 *          If you want a histogram of the colormap indices, use
 *          pixGetCmapHistogramInRect().
 *      (2) This always returns a 256-value histogram of pixel values.
 *      (3) Set the subsampling %factor > 1 to reduce the amount of computation.
 * </pre>
 */
NUMA *
pixGetGrayHistogramInRect(PIX     *pixs,
                          BOX     *box,
                          l_int32  factor)
{
l_int32     i, j, bx, by, bw, bh, w, h, wplg, val;
l_uint32   *datag, *lineg;
l_float32  *array;
NUMA       *na;
PIX        *pixg;

    if (!box)
        return pixGetGrayHistogram(pixs, factor);
    if (!pixs)
        return (NUMA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8 && !pixGetColormap(pixs))
        return (NUMA *)ERROR_PTR("pixs neither 8 bpp nor colormapped",
                                 __func__, NULL);
    if (factor < 1)
        return (NUMA *)ERROR_PTR("sampling must be >= 1", __func__, NULL);

    if ((na = numaCreate(256)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    numaSetCount(na, 256);  /* all initialized to 0.0 */
    array = numaGetFArray(na, L_NOCOPY);

    if (pixGetColormap(pixs))
        pixg = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    else
        pixg = pixClone(pixs);
    pixGetDimensions(pixg, &w, &h, NULL);
    datag = pixGetData(pixg);
    wplg = pixGetWpl(pixg);
    boxGetGeometry(box, &bx, &by, &bw, &bh);

        /* Generate the histogram */
    for (i = 0; i < bh; i += factor) {
        if (by + i < 0 || by + i >= h) continue;
        lineg = datag + (by + i) * wplg;
        for (j = 0; j < bw; j += factor) {
            if (bx + j < 0 || bx + j >= w) continue;
            val = GET_DATA_BYTE(lineg, bx + j);
            array[val] += 1.0;
        }
    }

    pixDestroy(&pixg);
    return na;
}


/*!
 * \brief   pixGetGrayHistogramTiled()
 *
 * \param[in]   pixs     any depth, colormap OK
 * \param[in]   factor   subsampling factor; integer >= 1
 * \param[in]   nx, ny   tiling; >= 1; typically small
 * \return  naa set of histograms, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If pixs is cmapped, it is converted to 8 bpp gray.
 *      (2) This returns a set of 256-value histograms of pixel values.
 *      (3) Set the subsampling factor > 1 to reduce the amount of computation.
 * </pre>
 */
NUMAA *
pixGetGrayHistogramTiled(PIX     *pixs,
                         l_int32  factor,
                         l_int32  nx,
                         l_int32  ny)
{
l_int32  i, n;
NUMA    *na;
NUMAA   *naa;
PIX     *pix1, *pix2;
PIXA    *pixa;

    if (!pixs)
        return (NUMAA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (factor < 1)
        return (NUMAA *)ERROR_PTR("sampling must be >= 1", __func__, NULL);
    if (nx < 1 || ny < 1)
        return (NUMAA *)ERROR_PTR("nx and ny must both be > 0", __func__, NULL);

    n = nx * ny;
    if ((naa = numaaCreate(n)) == NULL)
        return (NUMAA *)ERROR_PTR("naa not made", __func__, NULL);

    pix1 = pixConvertTo8(pixs, FALSE);
    pixa = pixaSplitPix(pix1, nx, ny, 0, 0);
    for (i = 0; i < n; i++) {
        pix2 = pixaGetPix(pixa, i, L_CLONE);
        na = pixGetGrayHistogram(pix2, factor);
        numaaAddNuma(naa, na, L_INSERT);
        pixDestroy(&pix2);
    }

    pixDestroy(&pix1);
    pixaDestroy(&pixa);
    return naa;
}


/*!
 * \brief   pixGetColorHistogram()
 *
 * \param[in]    pixs     rgb or colormapped
 * \param[in]    factor   subsampling factor; integer >= 1
 * \param[out]   pnar     red histogram
 * \param[out]   pnag     green histogram
 * \param[out]   pnab     blue histogram
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a set of three 256 entry histograms,
 *          one for each color component (r,g,b).
 *      (2) Set the subsampling %factor > 1 to reduce the amount of computation.
 * </pre>
 */
l_ok
pixGetColorHistogram(PIX     *pixs,
                     l_int32  factor,
                     NUMA   **pnar,
                     NUMA   **pnag,
                     NUMA   **pnab)
{
l_int32     i, j, w, h, d, wpl, index, rval, gval, bval;
l_uint32   *data, *line;
l_float32  *rarray, *garray, *barray;
NUMA       *nar, *nag, *nab;
PIXCMAP    *cmap;

    if (pnar) *pnar = NULL;
    if (pnag) *pnag = NULL;
    if (pnab) *pnab = NULL;
    if (!pnar || !pnag || !pnab)
        return ERROR_INT("&nar, &nag, &nab not all defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    cmap = pixGetColormap(pixs);
    if (cmap && (d != 2 && d != 4 && d != 8))
        return ERROR_INT("colormap and not 2, 4, or 8 bpp", __func__, 1);
    if (!cmap && d != 32)
        return ERROR_INT("no colormap and not rgb", __func__, 1);
    if (factor < 1)
        return ERROR_INT("sampling factor must be >= 1", __func__, 1);

        /* Set up the histogram arrays */
    nar = numaCreate(256);
    nag = numaCreate(256);
    nab = numaCreate(256);
    numaSetCount(nar, 256);
    numaSetCount(nag, 256);
    numaSetCount(nab, 256);
    rarray = numaGetFArray(nar, L_NOCOPY);
    garray = numaGetFArray(nag, L_NOCOPY);
    barray = numaGetFArray(nab, L_NOCOPY);
    *pnar = nar;
    *pnag = nag;
    *pnab = nab;

        /* Generate the color histograms */
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    if (cmap) {
        for (i = 0; i < h; i += factor) {
            line = data + i * wpl;
            for (j = 0; j < w; j += factor) {
                if (d == 8)
                    index = GET_DATA_BYTE(line, j);
                else if (d == 4)
                    index = GET_DATA_QBIT(line, j);
                else   /* 2 bpp */
                    index = GET_DATA_DIBIT(line, j);
                pixcmapGetColor(cmap, index, &rval, &gval, &bval);
                rarray[rval] += 1.0;
                garray[gval] += 1.0;
                barray[bval] += 1.0;
            }
        }
    } else {  /* 32 bpp rgb */
        for (i = 0; i < h; i += factor) {
            line = data + i * wpl;
            for (j = 0; j < w; j += factor) {
                extractRGBValues(line[j], &rval, &gval, &bval);
                rarray[rval] += 1.0;
                garray[gval] += 1.0;
                barray[bval] += 1.0;
            }
        }
    }

    return 0;
}


/*!
 * \brief   pixGetColorHistogramMasked()
 *
 * \param[in]    pixs     32 bpp rgb, or colormapped
 * \param[in]    pixm     [optional] 1 bpp mask over which histogram is
 *                        to be computed; use all pixels if null
 * \param[in]    x, y     UL corner of pixm relative to the UL corner of pixs;
 *                        can be < 0; these values are ignored if pixm is null
 * \param[in]    factor   subsampling factor; integer >= 1
 * \param[out]   pnar     red histogram
 * \param[out]   pnag     green histogram
 * \param[out]   pnab     blue histogram
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a set of three 256 entry histograms,
 *      (2) Set the subsampling %factor > 1 to reduce the amount of computation.
 *      (3) Clipping of pixm (if it exists) to pixs is done in the inner loop.
 *      (4) Input x,y are ignored unless pixm exists.
 * </pre>
 */
l_ok
pixGetColorHistogramMasked(PIX        *pixs,
                           PIX        *pixm,
                           l_int32     x,
                           l_int32     y,
                           l_int32     factor,
                           NUMA      **pnar,
                           NUMA      **pnag,
                           NUMA      **pnab)
{
l_int32     i, j, w, h, d, wm, hm, dm, wpls, wplm, index, rval, gval, bval;
l_uint32   *datas, *datam, *lines, *linem;
l_float32  *rarray, *garray, *barray;
NUMA       *nar, *nag, *nab;
PIXCMAP    *cmap;

    if (!pixm)
        return pixGetColorHistogram(pixs, factor, pnar, pnag, pnab);

    if (pnar) *pnar = NULL;
    if (pnag) *pnag = NULL;
    if (pnab) *pnab = NULL;
    if (!pnar || !pnag || !pnab)
        return ERROR_INT("&nar, &nag, &nab not all defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    cmap = pixGetColormap(pixs);
    if (cmap && (d != 2 && d != 4 && d != 8))
        return ERROR_INT("colormap and not 2, 4, or 8 bpp", __func__, 1);
    if (!cmap && d != 32)
        return ERROR_INT("no colormap and not rgb", __func__, 1);
    pixGetDimensions(pixm, &wm, &hm, &dm);
    if (dm != 1)
        return ERROR_INT("pixm not 1 bpp", __func__, 1);
    if (factor < 1)
        return ERROR_INT("sampling factor must be >= 1", __func__, 1);

        /* Set up the histogram arrays */
    nar = numaCreate(256);
    nag = numaCreate(256);
    nab = numaCreate(256);
    numaSetCount(nar, 256);
    numaSetCount(nag, 256);
    numaSetCount(nab, 256);
    rarray = numaGetFArray(nar, L_NOCOPY);
    garray = numaGetFArray(nag, L_NOCOPY);
    barray = numaGetFArray(nab, L_NOCOPY);
    *pnar = nar;
    *pnag = nag;
    *pnab = nab;

        /* Generate the color histograms */
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datam = pixGetData(pixm);
    wplm = pixGetWpl(pixm);
    if (cmap) {
        for (i = 0; i < hm; i += factor) {
            if (y + i < 0 || y + i >= h) continue;
            lines = datas + (y + i) * wpls;
            linem = datam + i * wplm;
            for (j = 0; j < wm; j += factor) {
                if (x + j < 0 || x + j >= w) continue;
                if (GET_DATA_BIT(linem, j)) {
                    if (d == 8)
                        index = GET_DATA_BYTE(lines, x + j);
                    else if (d == 4)
                        index = GET_DATA_QBIT(lines, x + j);
                    else   /* 2 bpp */
                        index = GET_DATA_DIBIT(lines, x + j);
                    pixcmapGetColor(cmap, index, &rval, &gval, &bval);
                    rarray[rval] += 1.0;
                    garray[gval] += 1.0;
                    barray[bval] += 1.0;
                }
            }
        }
    } else {  /* 32 bpp rgb */
        for (i = 0; i < hm; i += factor) {
            if (y + i < 0 || y + i >= h) continue;
            lines = datas + (y + i) * wpls;
            linem = datam + i * wplm;
            for (j = 0; j < wm; j += factor) {
                if (x + j < 0 || x + j >= w) continue;
                if (GET_DATA_BIT(linem, j)) {
                    extractRGBValues(lines[x + j], &rval, &gval, &bval);
                    rarray[rval] += 1.0;
                    garray[gval] += 1.0;
                    barray[bval] += 1.0;
                }
            }
        }
    }

    return 0;
}


/*!
 * \brief   pixGetCmapHistogram()
 *
 * \param[in]   pixs    colormapped: d = 2, 4 or 8
 * \param[in]   factor  subsampling factor; integer >= 1
 * \return  na histogram of cmap indices, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a histogram of colormap pixel indices,
 *          and is of size 2^d.
 *      (2) Set the subsampling %factor > 1 to reduce the amount of computation.
 * </pre>
 */
NUMA *
pixGetCmapHistogram(PIX     *pixs,
                    l_int32  factor)
{
l_int32     i, j, w, h, d, wpl, val, size;
l_uint32   *data, *line;
l_float32  *array;
NUMA       *na;

    if (!pixs)
        return (NUMA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetColormap(pixs) == NULL)
        return (NUMA *)ERROR_PTR("pixs not cmapped", __func__, NULL);
    if (factor < 1)
        return (NUMA *)ERROR_PTR("sampling must be >= 1", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 2 && d != 4 && d != 8)
        return (NUMA *)ERROR_PTR("d not 2, 4 or 8", __func__, NULL);

    size = 1 << d;
    if ((na = numaCreate(size)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    numaSetCount(na, size);  /* all initialized to 0.0 */
    array = numaGetFArray(na, L_NOCOPY);

    wpl = pixGetWpl(pixs);
    data = pixGetData(pixs);
    for (i = 0; i < h; i += factor) {
        line = data + i * wpl;
        for (j = 0; j < w; j += factor) {
            if (d == 8)
                val = GET_DATA_BYTE(line, j);
            else if (d == 4)
                val = GET_DATA_QBIT(line, j);
            else  /* d == 2 */
                val = GET_DATA_DIBIT(line, j);
            array[val] += 1.0;
        }
    }

    return na;
}


/*!
 * \brief   pixGetCmapHistogramMasked()
 *
 * \param[in]   pixs     colormapped: d = 2, 4 or 8
 * \param[in]   pixm     [optional] 1 bpp mask over which histogram is
 *                       to be computed; use all pixels if null
 * \param[in]   x, y     UL corner of pixm relative to the UL corner of pixs;
 *                       can be < 0; these values are ignored if pixm is null
 * \param[in]   factor   subsampling factor; integer >= 1
 * \return  na histogram, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a histogram of colormap pixel indices,
 *          and is of size 2^d.
 *      (2) Set the subsampling %factor > 1 to reduce the amount of computation.
 *      (3) Clipping of pixm to pixs is done in the inner loop.
 * </pre>
 */
NUMA *
pixGetCmapHistogramMasked(PIX     *pixs,
                          PIX     *pixm,
                          l_int32  x,
                          l_int32  y,
                          l_int32  factor)
{
l_int32     i, j, w, h, d, wm, hm, dm, wpls, wplm, val, size;
l_uint32   *datas, *datam, *lines, *linem;
l_float32  *array;
NUMA       *na;

    if (!pixm)
        return pixGetCmapHistogram(pixs, factor);

    if (!pixs)
        return (NUMA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetColormap(pixs) == NULL)
        return (NUMA *)ERROR_PTR("pixs not cmapped", __func__, NULL);
    pixGetDimensions(pixm, &wm, &hm, &dm);
    if (dm != 1)
        return (NUMA *)ERROR_PTR("pixm not 1 bpp", __func__, NULL);
    if (factor < 1)
        return (NUMA *)ERROR_PTR("sampling must be >= 1", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 2 && d != 4 && d != 8)
        return (NUMA *)ERROR_PTR("d not 2, 4 or 8", __func__, NULL);

    size = 1 << d;
    if ((na = numaCreate(size)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    numaSetCount(na, size);  /* all initialized to 0.0 */
    array = numaGetFArray(na, L_NOCOPY);

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datam = pixGetData(pixm);
    wplm = pixGetWpl(pixm);

    for (i = 0; i < hm; i += factor) {
        if (y + i < 0 || y + i >= h) continue;
        lines = datas + (y + i) * wpls;
        linem = datam + i * wplm;
        for (j = 0; j < wm; j += factor) {
            if (x + j < 0 || x + j >= w) continue;
            if (GET_DATA_BIT(linem, j)) {
                if (d == 8)
                    val = GET_DATA_BYTE(lines, x + j);
                else if (d == 4)
                    val = GET_DATA_QBIT(lines, x + j);
                else  /* d == 2 */
                    val = GET_DATA_DIBIT(lines, x + j);
                array[val] += 1.0;
            }
        }
    }

    return na;
}


/*!
 * \brief   pixGetCmapHistogramInRect()
 *
 * \param[in]   pixs     colormapped: d = 2, 4 or 8
 * \param[in]   box      [optional] over which histogram is to be computed;
 *                       use full image if NULL
 * \param[in]   factor   subsampling factor; integer >= 1
 * \return  na histogram, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a histogram of colormap pixel indices,
 *          and is of size 2^d.
 *      (2) Set the subsampling %factor > 1 to reduce the amount of computation.
 *      (3) Clipping to the box is done in the inner loop.
 * </pre>
 */
NUMA *
pixGetCmapHistogramInRect(PIX     *pixs,
                          BOX     *box,
                          l_int32  factor)
{
l_int32     i, j, bx, by, bw, bh, w, h, d, wpls, val, size;
l_uint32   *datas, *lines;
l_float32  *array;
NUMA       *na;

    if (!box)
        return pixGetCmapHistogram(pixs, factor);
    if (!pixs)
        return (NUMA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetColormap(pixs) == NULL)
        return (NUMA *)ERROR_PTR("pixs not cmapped", __func__, NULL);
    if (factor < 1)
        return (NUMA *)ERROR_PTR("sampling must be >= 1", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 2 && d != 4 && d != 8)
        return (NUMA *)ERROR_PTR("d not 2, 4 or 8", __func__, NULL);

    size = 1 << d;
    if ((na = numaCreate(size)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);
    numaSetCount(na, size);  /* all initialized to 0.0 */
    array = numaGetFArray(na, L_NOCOPY);

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    boxGetGeometry(box, &bx, &by, &bw, &bh);

    for (i = 0; i < bh; i += factor) {
        if (by + i < 0 || by + i >= h) continue;
        lines = datas + (by + i) * wpls;
        for (j = 0; j < bw; j += factor) {
            if (bx + j < 0 || bx + j >= w) continue;
            if (d == 8)
                val = GET_DATA_BYTE(lines, bx + j);
            else if (d == 4)
                val = GET_DATA_QBIT(lines, bx + j);
            else  /* d == 2 */
                val = GET_DATA_DIBIT(lines, bx + j);
            array[val] += 1.0;
        }
    }

    return na;
}


/*!
 * \brief   pixCountRGBColorsByHash()
 *
 * \param[in]    pixs       rgb or rgba
 * \param[out]   pncolors   number of colors found
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is about 4x faster than pixCountRGBColors(),
 *          which uses an ordered map.
 * </pre>
 */
l_ok
pixCountRGBColorsByHash(PIX      *pixs,
                        l_int32  *pncolors)
{
L_DNA  *da1, *da2;

    if (!pncolors)
        return ERROR_INT("&ncolors not defined", __func__, 1);
    *pncolors = 0;
    if (!pixs || pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs not defined or not 32 bpp", __func__, 1);
    da1 = pixConvertDataToDna(pixs);
    l_dnaRemoveDupsByHmap(da1, &da2, NULL);
    *pncolors = l_dnaGetCount(da2);
    l_dnaDestroy(&da1);
    l_dnaDestroy(&da2);
    return 0;
}


/*!
 * \brief   pixCountRGBColors()
 *
 * \param[in]    pixs       rgb or rgba
 * \param[in]    factor     subsampling factor; integer >= 1
 * \param[out]   pncolors   number of colors found
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If %factor == 1, this gives the exact number of colors.
 *      (2) This is about 4x slower than pixCountRGBColorsByHash().
 * </pre>
 */
l_ok
pixCountRGBColors(PIX      *pixs,
                  l_int32   factor,
                  l_int32  *pncolors)
{
L_AMAP  *amap;

    if (!pncolors)
        return ERROR_INT("&ncolors not defined", __func__, 1);
    *pncolors = 0;
    if (!pixs || pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs not defined or not 32 bpp", __func__, 1);
    if (factor <= 0)
        return ERROR_INT("factor must be > 0", __func__, 1);
    amap = pixGetColorAmapHistogram(pixs, factor);
    *pncolors = l_amapSize(amap);
    l_amapDestroy(&amap);
    return 0;
}


/*!
 * \brief   pixGetColorAmapHistogram()
 *
 * \param[in]   pixs    rgb or rgba
 * \param[in]   factor  subsampling factor; integer >= 1
 * \return  amap, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This generates an ordered map from pixel value to histogram count.
 *      (2) Use amapGetCountForColor() to use the map to look up a count.
 * </pre>
 */
L_AMAP  *
pixGetColorAmapHistogram(PIX     *pixs,
                         l_int32  factor)
{
l_int32    i, j, w, h, wpl;
l_uint32  *data, *line;
L_AMAP    *amap;
RB_TYPE    key, value;
RB_TYPE   *pval;

    if (!pixs)
        return (L_AMAP *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (L_AMAP *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (factor <= 0)
        return (L_AMAP *)ERROR_PTR("factor must be > 0", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    amap = l_amapCreate(L_UINT_TYPE);
    for (i = 0; i < h; i += factor) {
        line = data + i * wpl;
        for (j = 0; j < w; j += factor) {
            key.utype = line[j];
            pval = l_amapFind(amap, key);
            if (!pval)
                value.itype = 1;
            else
                value.itype = 1 + pval->itype;
            l_amapInsert(amap, key, value);
        }
    }

    return amap;
}


/*!
 * \brief   amapGetCountForColor()
 *
 * \param[in]   amap   map from pixel value to count
 * \param[in]   val    rgb or rgba pixel value
 * \return  count, or -1 on error
 *
 * <pre>
 * Notes:
 *      (1) The ordered map is made by pixGetColorAmapHistogram().
 * </pre>
 */
l_int32
amapGetCountForColor(L_AMAP   *amap,
                     l_uint32  val)
{
RB_TYPE   key;
RB_TYPE  *pval;

    if (!amap)
        return ERROR_INT("amap not defined", __func__, -1);

    key.utype = val;
    pval = l_amapFind(amap, key);
    return (pval) ? pval->itype : 0;
}


/*!
 * \brief   pixGetRankValue()
 *
 * \param[in]    pixs     8 bpp, 32 bpp or colormapped
 * \param[in]    factor   subsampling factor; integer >= 1
 * \param[in]    rank     between 0.0 and 1.0; 1.0 is brightest, 0.0 is darkest
 * \param[out]   pvalue   pixel value corresponding to input rank
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Simple function to get a rank value (color) of an image.
 *          For a color image, the median value (rank = 0.5) can be
 *          used to linearly remap the colors based on the median
 *          of a target image, using pixLinearMapToTargetColor().
 *      (2) For RGB, this treats each color component independently.
 *          It calls pixGetGrayHistogramMasked() on each component, and
 *          uses the returned gray histogram to get the rank value.
 *          It then combines the 3 rank values into a color pixel.
 * </pre>
 */
l_ok
pixGetRankValue(PIX       *pixs,
                l_int32    factor,
                l_float32  rank,
                l_uint32  *pvalue)
{
l_int32    d;
l_float32  val, rval, gval, bval;
PIX       *pixt;
PIXCMAP   *cmap;

    if (!pvalue)
        return ERROR_INT("&value not defined", __func__, 1);
    *pvalue = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    d = pixGetDepth(pixs);
    cmap = pixGetColormap(pixs);
    if (d != 8 && d != 32 && !cmap)
        return ERROR_INT("pixs not 8 or 32 bpp, or cmapped", __func__, 1);
    if (cmap)
        pixt = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
    else
        pixt = pixClone(pixs);
    d = pixGetDepth(pixt);

    if (d == 8) {
        pixGetRankValueMasked(pixt, NULL, 0, 0, factor, rank, &val, NULL);
        *pvalue = lept_roundftoi(val);
    } else {
        pixGetRankValueMaskedRGB(pixt, NULL, 0, 0, factor, rank,
                                 &rval, &gval, &bval);
        composeRGBPixel(lept_roundftoi(rval), lept_roundftoi(gval),
                        lept_roundftoi(bval), pvalue);
    }

    pixDestroy(&pixt);
    return 0;
}


/*!
 * \brief   pixGetRankValueMaskedRGB()
 *
 * \param[in]    pixs     32 bpp
 * \param[in]    pixm     [optional] 1 bpp mask over which rank val is to be taken;
 *                        use all pixels if null
 * \param[in]    x, y     UL corner of pixm relative to the UL corner of pixs;
 *                        can be < 0; these values are ignored if pixm is null
 * \param[in]    factor   subsampling factor; integer >= 1
 * \param[in]    rank     between 0.0 and 1.0; 1.0 is brightest, 0.0 is darkest
 * \param[out]   prval    [optional] red component val for input rank
 * \param[out]   pgval    [optional] green component val for input rank
 * \param[out]   pbval    [optional] blue component val for input rank
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Computes the rank component values of pixels in pixs that
 *          are under the fg of the optional mask.  If the mask is null, it
 *          computes the average of the pixels in pixs.
 *      (2) Set the subsampling %factor > 1 to reduce the amount of
 *          computation.
 *      (4) Input x,y are ignored unless pixm exists.
 *      (5) The rank must be in [0.0 ... 1.0], where the brightest pixel
 *          has rank 1.0.  For the median pixel value, use 0.5.
 * </pre>
 */
l_ok
pixGetRankValueMaskedRGB(PIX        *pixs,
                         PIX        *pixm,
                         l_int32     x,
                         l_int32     y,
                         l_int32     factor,
                         l_float32   rank,
                         l_float32  *prval,
                         l_float32  *pgval,
                         l_float32  *pbval)
{
l_float32  scale;
PIX       *pixmt, *pixt;

    if (prval) *prval = 0.0;
    if (pgval) *pgval = 0.0;
    if (pbval) *pbval = 0.0;
    if (!prval && !pgval && !pbval)
        return ERROR_INT("no results requested", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs not 32 bpp", __func__, 1);
    if (pixm && pixGetDepth(pixm) != 1)
        return ERROR_INT("pixm not 1 bpp", __func__, 1);
    if (factor < 1)
        return ERROR_INT("sampling factor must be >= 1", __func__, 1);
    if (rank < 0.0 || rank > 1.0)
        return ERROR_INT("rank not in [0.0 ... 1.0]", __func__, 1);

    pixmt = NULL;
    if (pixm) {
        scale = 1.0f / (l_float32)factor;
        pixmt = pixScale(pixm, scale, scale);
    }
    if (prval) {
        pixt = pixScaleRGBToGrayFast(pixs, factor, COLOR_RED);
        pixGetRankValueMasked(pixt, pixmt, x / factor, y / factor,
                              factor, rank, prval, NULL);
        pixDestroy(&pixt);
    }
    if (pgval) {
        pixt = pixScaleRGBToGrayFast(pixs, factor, COLOR_GREEN);
        pixGetRankValueMasked(pixt, pixmt, x / factor, y / factor,
                              factor, rank, pgval, NULL);
        pixDestroy(&pixt);
    }
    if (pbval) {
        pixt = pixScaleRGBToGrayFast(pixs, factor, COLOR_BLUE);
        pixGetRankValueMasked(pixt, pixmt, x / factor, y / factor,
                              factor, rank, pbval, NULL);
        pixDestroy(&pixt);
    }
    pixDestroy(&pixmt);
    return 0;
}


/*!
 * \brief   pixGetRankValueMasked()
 *
 * \param[in]    pixs     8 bpp, or colormapped
 * \param[in]    pixm     [optional] 1 bpp mask, over which the rank val
 *                        is to be taken; use all pixels if null
 * \param[in]    x, y     UL corner of pixm relative to the UL corner of pixs;
 *                        can be < 0; these values are ignored if pixm is null
 * \param[in]    factor   subsampling factor; integer >= 1
 * \param[in]    rank     between 0.0 and 1.0; 1.0 is brightest, 0.0 is darkest
 * \param[out]   pval     pixel value corresponding to input rank
 * \param[out]   pna     [optional] of histogram
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Computes the rank value of pixels in pixs that are under
 *          the fg of the optional mask.  If the mask is null, it
 *          computes the average of the pixels in pixs.
 *      (2) Set the subsampling %factor > 1 to reduce the amount of
 *          computation.
 *      (3) Clipping of pixm (if it exists) to pixs is done in the inner loop.
 *      (4) Input x,y are ignored unless pixm exists.
 *      (5) The rank must be in [0.0 ... 1.0], where the brightest pixel
 *          has rank 1.0.  For the median pixel value, use 0.5.
 *      (6) The histogram can optionally be returned, so that other rank
 *          values can be extracted without recomputing the histogram.
 *          In that case, just use
 *              numaHistogramGetValFromRank(na, rank, &val);
 *          on the returned Numa for additional rank values.
 * </pre>
 */
l_ok
pixGetRankValueMasked(PIX        *pixs,
                      PIX        *pixm,
                      l_int32     x,
                      l_int32     y,
                      l_int32     factor,
                      l_float32   rank,
                      l_float32  *pval,
                      NUMA      **pna)
{
NUMA  *na;

    if (pna) *pna = NULL;
    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0.0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (pixGetDepth(pixs) != 8 && !pixGetColormap(pixs))
        return ERROR_INT("pixs neither 8 bpp nor colormapped", __func__, 1);
    if (pixm && pixGetDepth(pixm) != 1)
        return ERROR_INT("pixm not 1 bpp", __func__, 1);
    if (factor < 1)
        return ERROR_INT("sampling factor must be >= 1", __func__, 1);
    if (rank < 0.0 || rank > 1.0)
        return ERROR_INT("rank not in [0.0 ... 1.0]", __func__, 1);

    if ((na = pixGetGrayHistogramMasked(pixs, pixm, x, y, factor)) == NULL)
        return ERROR_INT("na not made", __func__, 1);
    numaHistogramGetValFromRank(na, rank, pval);
    if (pna)
        *pna = na;
    else
        numaDestroy(&na);

    return 0;
}


/*!
 * \brief   pixGetPixelAverage()
 *
 * \param[in]    pixs     8 or 32 bpp, or colormapped
 * \param[in]    pixm     [optional] 1 bpp mask over which average is
 *                        to be taken; use all pixels if null
 * \param[in]    x, y     UL corner of pixm relative to the UL corner of pixs;
 *                        can be < 0
 * \param[in]    factor   subsampling factor; >= 1
 * \param[out]   pval     average pixel value
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) For rgb pix, this is a more direct computation of the
 *          average value of the pixels in %pixs that are under the
 *          mask %pixm. It is faster than pixGetPixelStats(), which
 *          calls pixGetAverageMaskedRGB() and has the overhead of
 *          generating a temporary pix of each of the three components;
 *          this can take most of the time if %factor > 1.
 *      (2) If %pixm is null, this gives the average value of all
 *          pixels in %pixs.  The returned value is an integer.
 *      (3) For color %pixs, the returned pixel value is in the standard
 *          uint32 RGBA packing.
 *      (4) Clipping of pixm (if it exists) to pixs is done in the inner loop.
 *      (5) Input x,y are ignored if %pixm does not exist.
 *      (6) For general averaging of 1, 2, 4 or 8 bpp grayscale, use
 *          pixAverageInRect().
 * </pre>
 */
l_ok
pixGetPixelAverage(PIX       *pixs,
                   PIX       *pixm,
                   l_int32    x,
                   l_int32    y,
                   l_int32    factor,
                   l_uint32  *pval)
{
l_int32    i, j, w, h, d, wm, hm, wpl1, wplm, val, rval, gval, bval, count;
l_uint32  *data1, *datam, *line1, *linem;
l_float64  sum, rsum, gsum, bsum;
PIX       *pix1;

    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    d = pixGetDepth(pixs);
    if (d != 32 && !pixGetColormap(pixs))
        return ERROR_INT("pixs not rgb or colormapped", __func__, 1);
    if (pixm && pixGetDepth(pixm) != 1)
        return ERROR_INT("pixm not 1 bpp", __func__, 1);
    if (factor < 1)
        return ERROR_INT("sampling factor must be >= 1", __func__, 1);

    if (pixGetColormap(pixs))
        pix1 = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
    else
        pix1 = pixClone(pixs);
    pixGetDimensions(pix1, &w, &h, &d);
    if (d == 1) {
        pixDestroy(&pix1);
        return ERROR_INT("pix1 is just 1 bpp", __func__, 1);
    }
    data1 = pixGetData(pix1);
    wpl1 = pixGetWpl(pix1);

    sum = rsum = gsum = bsum = 0.0;
    count = 0;
    if (!pixm) {
        for (i = 0; i < h; i += factor) {
            line1 = data1 + i * wpl1;
            for (j = 0; j < w; j += factor) {
                if (d == 8) {
                    val = GET_DATA_BYTE(line1, j);
                    sum += val;
                } else {  /* rgb */
                    extractRGBValues(*(line1 + j), &rval, &gval, &bval);
                    rsum += rval;
                    gsum += gval;
                    bsum += bval;
                }
                count++;
            }
        }
    } else {  /* masked */
        pixGetDimensions(pixm, &wm, &hm, NULL);
        datam = pixGetData(pixm);
        wplm = pixGetWpl(pixm);
        for (i = 0; i < hm; i += factor) {
            if (y + i < 0 || y + i >= h) continue;
            line1 = data1 + (y + i) * wpl1;
            linem = datam + i * wplm;
            for (j = 0; j < wm; j += factor) {
                if (x + j < 0 || x + j >= w) continue;
                if (GET_DATA_BIT(linem, j)) {
                    if (d == 8) {
                        val = GET_DATA_BYTE(line1, x + j);
                        sum += val;
                    } else {  /* rgb */
                        extractRGBValues(*(line1 + x + j), &rval, &gval, &bval);
                        rsum += rval;
                        gsum += gval;
                        bsum += bval;
                    }
                    count++;
                }
            }
        }
    }

    pixDestroy(&pix1);
    if (count == 0)
        return ERROR_INT("no pixels sampled", __func__, 1);
    if (d == 8) {
        *pval = (l_uint32)(sum / (l_float64)count);
    } else {  /* d == 32 */
        rval = (l_uint32)(rsum / (l_float64)count);
        gval = (l_uint32)(gsum / (l_float64)count);
        bval = (l_uint32)(bsum / (l_float64)count);
        composeRGBPixel(rval, gval, bval, pval);
    }

    return 0;
}


/*!
 * \brief   pixGetPixelStats()
 *
 * \param[in]    pixs     8 bpp, 32 bpp or colormapped
 * \param[in]    factor   subsampling factor; integer >= 1
 * \param[in]    type     L_MEAN_ABSVAL, L_ROOT_MEAN_SQUARE,
 *                        L_STANDARD_DEVIATION, L_VARIANCE
 * \param[out]   pvalue   pixel value corresponding to input type
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Simple function to get one of four statistical values of an image.
 *      (2) It does not take a mask: it uses the entire image.
 *      (3) To get the average pixel value of an RGB image, suggest using
 *          pixGetPixelAverage(), which is considerably faster.
 * </pre>
 */
l_ok
pixGetPixelStats(PIX       *pixs,
                 l_int32    factor,
                 l_int32    type,
                 l_uint32  *pvalue)
{
l_int32    d;
l_float32  val, rval, gval, bval;
PIX       *pixt;
PIXCMAP   *cmap;

    if (!pvalue)
        return ERROR_INT("&value not defined", __func__, 1);
    *pvalue = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    d = pixGetDepth(pixs);
    cmap = pixGetColormap(pixs);
    if (d != 8 && d != 32 && !cmap)
        return ERROR_INT("pixs not 8 or 32 bpp, or cmapped", __func__, 1);
    if (cmap)
        pixt = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
    else
        pixt = pixClone(pixs);
    d = pixGetDepth(pixt);

    if (d == 8) {
        pixGetAverageMasked(pixt, NULL, 0, 0, factor, type, &val);
        *pvalue = lept_roundftoi(val);
    } else {
        pixGetAverageMaskedRGB(pixt, NULL, 0, 0, factor, type,
                               &rval, &gval, &bval);
        composeRGBPixel(lept_roundftoi(rval), lept_roundftoi(gval),
                        lept_roundftoi(bval), pvalue);
    }

    pixDestroy(&pixt);
    return 0;
}


/*!
 * \brief   pixGetAverageMaskedRGB()
 *
 * \param[in]    pixs     32 bpp, or colormapped
 * \param[in]    pixm     [optional] 1 bpp mask over which average is
 *                        to be taken; use all pixels if null
 * \param[in]    x, y     UL corner of pixm relative to the UL corner of pixs;
 *                        can be < 0
 * \param[in]    factor   subsampling factor; >= 1
 * \param[in]    type     L_MEAN_ABSVAL, L_ROOT_MEAN_SQUARE,
 *                        L_STANDARD_DEVIATION, L_VARIANCE
 * \param[out]   prval    [optional] measured red value of given 'type'
 * \param[out]   pgval    [optional] measured green value of given 'type'
 * \param[out]   pbval    [optional] measured blue value of given 'type'
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) For usage, see pixGetAverageMasked().
 *      (2) If there is a colormap, it is removed before the 8 bpp
 *          component images are extracted.
 *      (3) A better name for this would be: pixGetPixelStatsRGB()
 * </pre>
 */
l_ok
pixGetAverageMaskedRGB(PIX        *pixs,
                       PIX        *pixm,
                       l_int32     x,
                       l_int32     y,
                       l_int32     factor,
                       l_int32     type,
                       l_float32  *prval,
                       l_float32  *pgval,
                       l_float32  *pbval)
{
l_int32   empty;
PIX      *pixt;
PIXCMAP  *cmap;

    if (prval) *prval = 0.0;
    if (pgval) *pgval = 0.0;
    if (pbval) *pbval = 0.0;
    if (!prval && !pgval && !pbval)
        return ERROR_INT("no values requested", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    cmap = pixGetColormap(pixs);
    if (pixGetDepth(pixs) != 32 && !cmap)
        return ERROR_INT("pixs neither 32 bpp nor colormapped", __func__, 1);
    if (pixm && pixGetDepth(pixm) != 1)
        return ERROR_INT("pixm not 1 bpp", __func__, 1);
    if (factor < 1)
        return ERROR_INT("sampling factor must be >= 1", __func__, 1);
    if (type != L_MEAN_ABSVAL && type != L_ROOT_MEAN_SQUARE &&
        type != L_STANDARD_DEVIATION && type != L_VARIANCE)
        return ERROR_INT("invalid measure type", __func__, 1);
    if (pixm) {
        pixZero(pixm, &empty);
        if (empty)
            return ERROR_INT("empty mask", __func__, 1);
    }

    if (prval) {
        if (cmap)
            pixt = pixGetRGBComponentCmap(pixs, COLOR_RED);
        else
            pixt = pixGetRGBComponent(pixs, COLOR_RED);
        pixGetAverageMasked(pixt, pixm, x, y, factor, type, prval);
        pixDestroy(&pixt);
    }
    if (pgval) {
        if (cmap)
            pixt = pixGetRGBComponentCmap(pixs, COLOR_GREEN);
        else
            pixt = pixGetRGBComponent(pixs, COLOR_GREEN);
        pixGetAverageMasked(pixt, pixm, x, y, factor, type, pgval);
        pixDestroy(&pixt);
    }
    if (pbval) {
        if (cmap)
            pixt = pixGetRGBComponentCmap(pixs, COLOR_BLUE);
        else
            pixt = pixGetRGBComponent(pixs, COLOR_BLUE);
        pixGetAverageMasked(pixt, pixm, x, y, factor, type, pbval);
        pixDestroy(&pixt);
    }

    return 0;
}


/*!
 * \brief   pixGetAverageMasked()
 *
 * \param[in]   pixs     8 or 16 bpp, or colormapped
 * \param[in]   pixm     [optional] 1 bpp mask over which average is
 *                       to be taken; use all pixels if null
 * \param[in]   x, y     UL corner of pixm relative to the UL corner of pixs;
 *                       can be < 0
 * \param[in]   factor   subsampling factor; >= 1
 * \param[in]   type     L_MEAN_ABSVAL, L_ROOT_MEAN_SQUARE,
 *                       L_STANDARD_DEVIATION, L_VARIANCE
 * \param[out]  pval     measured value of given 'type'
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Use L_MEAN_ABSVAL to get the average value of pixels in pixs
 *          that are under the fg of the optional mask.  If the mask
 *          is null, it finds the average of the pixels in pixs.
 *      (2) Likewise, use L_ROOT_MEAN_SQUARE to get the rms value of
 *          pixels in pixs, either masked or not; L_STANDARD_DEVIATION
 *          to get the standard deviation from the mean of the pixels;
 *          L_VARIANCE to get the average squared difference from the
 *          expected value.  The variance is the square of the stdev.
 *          For the standard deviation, we use
 *              sqrt([([x] - x)]^2) = sqrt([x^2] - [x]^2)
 *      (3) Set the subsampling %factor > 1 to reduce the amount of
 *          computation.
 *      (4) Clipping of pixm (if it exists) to pixs is done in the inner loop.
 *      (5) Input x,y are ignored unless pixm exists.
 *      (6) A better name for this would be: pixGetPixelStatsGray()
 * </pre>
 */
l_ok
pixGetAverageMasked(PIX        *pixs,
                    PIX        *pixm,
                    l_int32     x,
                    l_int32     y,
                    l_int32     factor,
                    l_int32     type,
                    l_float32  *pval)
{
l_int32    i, j, w, h, d, wm, hm, wplg, wplm, val, count, empty;
l_uint32  *datag, *datam, *lineg, *linem;
l_float64  sumave, summs, ave, meansq, var;
PIX       *pixg;

    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0.0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    d = pixGetDepth(pixs);
    if (d != 8 && d != 16 && !pixGetColormap(pixs))
        return ERROR_INT("pixs not 8 or 16 bpp or colormapped", __func__, 1);
    if (pixm && pixGetDepth(pixm) != 1)
        return ERROR_INT("pixm not 1 bpp", __func__, 1);
    if (factor < 1)
        return ERROR_INT("sampling factor must be >= 1", __func__, 1);
    if (type != L_MEAN_ABSVAL && type != L_ROOT_MEAN_SQUARE &&
        type != L_STANDARD_DEVIATION && type != L_VARIANCE)
        return ERROR_INT("invalid measure type", __func__, 1);
    if (pixm) {
        pixZero(pixm, &empty);
        if (empty)
            return ERROR_INT("empty mask", __func__, 1);
    }

    if (pixGetColormap(pixs))
        pixg = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    else
        pixg = pixClone(pixs);
    pixGetDimensions(pixg, &w, &h, &d);
    datag = pixGetData(pixg);
    wplg = pixGetWpl(pixg);

    sumave = summs = 0.0;
    count = 0;
    if (!pixm) {
        for (i = 0; i < h; i += factor) {
            lineg = datag + i * wplg;
            for (j = 0; j < w; j += factor) {
                if (d == 8)
                    val = GET_DATA_BYTE(lineg, j);
                else  /* d == 16 */
                    val = GET_DATA_TWO_BYTES(lineg, j);
                if (type != L_ROOT_MEAN_SQUARE)
                    sumave += val;
                if (type != L_MEAN_ABSVAL)
                    summs += (l_float64)(val) * val;
                count++;
            }
        }
    } else {
        pixGetDimensions(pixm, &wm, &hm, NULL);
        datam = pixGetData(pixm);
        wplm = pixGetWpl(pixm);
        for (i = 0; i < hm; i += factor) {
            if (y + i < 0 || y + i >= h) continue;
            lineg = datag + (y + i) * wplg;
            linem = datam + i * wplm;
            for (j = 0; j < wm; j += factor) {
                if (x + j < 0 || x + j >= w) continue;
                if (GET_DATA_BIT(linem, j)) {
                    if (d == 8)
                        val = GET_DATA_BYTE(lineg, x + j);
                    else  /* d == 16 */
                        val = GET_DATA_TWO_BYTES(lineg, x + j);
                    if (type != L_ROOT_MEAN_SQUARE)
                        sumave += val;
                    if (type != L_MEAN_ABSVAL)
                        summs += (l_float64)(val) * val;
                    count++;
                }
            }
        }
    }

    pixDestroy(&pixg);
    if (count == 0)
        return ERROR_INT("no pixels sampled", __func__, 1);
    ave = sumave / (l_float64)count;
    meansq = summs / (l_float64)count;
    var = meansq - ave * ave;
    if (type == L_MEAN_ABSVAL)
        *pval = (l_float32)ave;
    else if (type == L_ROOT_MEAN_SQUARE)
        *pval = (l_float32)sqrt(meansq);
    else if (type == L_STANDARD_DEVIATION)
        *pval = (l_float32)sqrt(var);
    else  /* type == L_VARIANCE */
        *pval = (l_float32)var;

    return 0;
}


/*!
 * \brief   pixGetAverageTiledRGB()
 *
 * \param[in]   pixs     32 bpp, or colormapped
 * \param[in]   sx, sy   tile size; must be at least 2 x 2
 * \param[in]   type     L_MEAN_ABSVAL, L_ROOT_MEAN_SQUARE, L_STANDARD_DEVIATION
 * \param[out]  ppixr    [optional] tiled 'average' of red component
 * \param[out]  ppixg    [optional] tiled 'average' of green component
 * \param[out]  ppixb    [optional] tiled 'average' of blue component
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) For usage, see pixGetAverageTiled().
 *      (2) If there is a colormap, it is removed before the 8 bpp
 *          component images are extracted.
 * </pre>
 */
l_ok
pixGetAverageTiledRGB(PIX     *pixs,
                      l_int32  sx,
                      l_int32  sy,
                      l_int32  type,
                      PIX    **ppixr,
                      PIX    **ppixg,
                      PIX    **ppixb)
{
PIX      *pixt;
PIXCMAP  *cmap;

    if (ppixr) *ppixr = NULL;
    if (ppixg) *ppixg = NULL;
    if (ppixb) *ppixb = NULL;
    if (!ppixr && !ppixg && !ppixb)
        return ERROR_INT("no data requested", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    cmap = pixGetColormap(pixs);
    if (pixGetDepth(pixs) != 32 && !cmap)
        return ERROR_INT("pixs neither 32 bpp nor colormapped", __func__, 1);
    if (sx < 2 || sy < 2)
        return ERROR_INT("sx and sy not both > 1", __func__, 1);
    if (type != L_MEAN_ABSVAL && type != L_ROOT_MEAN_SQUARE &&
        type != L_STANDARD_DEVIATION)
        return ERROR_INT("invalid measure type", __func__, 1);

    if (ppixr) {
        if (cmap)
            pixt = pixGetRGBComponentCmap(pixs, COLOR_RED);
        else
            pixt = pixGetRGBComponent(pixs, COLOR_RED);
        *ppixr = pixGetAverageTiled(pixt, sx, sy, type);
        pixDestroy(&pixt);
    }
    if (ppixg) {
        if (cmap)
            pixt = pixGetRGBComponentCmap(pixs, COLOR_GREEN);
        else
            pixt = pixGetRGBComponent(pixs, COLOR_GREEN);
        *ppixg = pixGetAverageTiled(pixt, sx, sy, type);
        pixDestroy(&pixt);
    }
    if (ppixb) {
        if (cmap)
            pixt = pixGetRGBComponentCmap(pixs, COLOR_BLUE);
        else
            pixt = pixGetRGBComponent(pixs, COLOR_BLUE);
        *ppixb = pixGetAverageTiled(pixt, sx, sy, type);
        pixDestroy(&pixt);
    }

    return 0;
}


/*!
 * \brief   pixGetAverageTiled()
 *
 * \param[in]   pixs    8 bpp, or colormapped
 * \param[in]   sx, sy  tile size; must be at least 2 x 2
 * \param[in]   type    L_MEAN_ABSVAL, L_ROOT_MEAN_SQUARE, L_STANDARD_DEVIATION
 * \return  pixd average values in each tile, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Only computes for tiles that are entirely contained in pixs.
 *      (2) Use L_MEAN_ABSVAL to get the average abs value within the tile;
 *          L_ROOT_MEAN_SQUARE to get the rms value within each tile;
 *          L_STANDARD_DEVIATION to get the standard dev. from the average
 *          within each tile.
 *      (3) If colormapped, converts to 8 bpp gray.
 * </pre>
 */
PIX *
pixGetAverageTiled(PIX     *pixs,
                   l_int32  sx,
                   l_int32  sy,
                   l_int32  type)
{
l_int32    i, j, k, m, w, h, wd, hd, d, pos, wplt, wpld, valt;
l_uint32  *datat, *datad, *linet, *lined, *startt;
l_float64  sumave, summs, ave, meansq, normfact;
PIX       *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8 && !pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs not 8 bpp or cmapped", __func__, NULL);
    if (sx < 2 || sy < 2)
        return (PIX *)ERROR_PTR("sx and sy not both > 1", __func__, NULL);
    wd = w / sx;
    hd = h / sy;
    if (wd < 1 || hd < 1)
        return (PIX *)ERROR_PTR("wd or hd == 0", __func__, NULL);
    if (type != L_MEAN_ABSVAL && type != L_ROOT_MEAN_SQUARE &&
        type != L_STANDARD_DEVIATION)
        return (PIX *)ERROR_PTR("invalid measure type", __func__, NULL);

    pixt = pixRemoveColormap(pixs, REMOVE_CMAP_TO_GRAYSCALE);
    pixd = pixCreate(wd, hd, 8);
    datat = pixGetData(pixt);
    wplt = pixGetWpl(pixt);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    normfact = 1. / (l_float64)(sx * sy);
    for (i = 0; i < hd; i++) {
        lined = datad + i * wpld;
        linet = datat + i * sy * wplt;
        for (j = 0; j < wd; j++) {
            if (type == L_MEAN_ABSVAL || type == L_STANDARD_DEVIATION) {
                sumave = 0.0;
                for (k = 0; k < sy; k++) {
                    startt = linet + k * wplt;
                    for (m = 0; m < sx; m++) {
                        pos = j * sx + m;
                        valt = GET_DATA_BYTE(startt, pos);
                        sumave += valt;
                    }
                }
                ave = normfact * sumave;
            }
            if (type == L_ROOT_MEAN_SQUARE || type == L_STANDARD_DEVIATION) {
                summs = 0.0;
                for (k = 0; k < sy; k++) {
                    startt = linet + k * wplt;
                    for (m = 0; m < sx; m++) {
                        pos = j * sx + m;
                        valt = GET_DATA_BYTE(startt, pos);
                        summs += (l_float64)(valt) * valt;
                    }
                }
                meansq = normfact * summs;
            }
            if (type == L_MEAN_ABSVAL)
                valt = (l_int32)(ave + 0.5);
            else if (type == L_ROOT_MEAN_SQUARE)
                valt = (l_int32)(sqrt(meansq) + 0.5);
            else  /* type == L_STANDARD_DEVIATION */
                valt = (l_int32)(sqrt(meansq - ave * ave) + 0.5);
            SET_DATA_BYTE(lined, j, valt);
        }
    }

    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixRowStats()
 *
 * \param[in]    pixs          8 bpp; not cmapped
 * \param[in]    box           [optional] clipping box; can be null
 * \param[out]   pnamean       [optional] numa of mean values
 * \param[out]   pnamedian     [optional] numa of median values
 * \param[out]   pnamode       [optional] numa of mode intensity values
 * \param[out]   pnamodecount  [optional] numa of mode counts
 * \param[out]   pnavar        [optional] numa of variance
 * \param[out]   pnarootvar    [optional] numa of square root of variance
 * \return  na numa of requested statistic for each row, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This computes numas that represent column vectors of statistics,
 *          with each of its values derived from the corresponding row of a Pix.
 *      (2) Use NULL on input to prevent computation of any of the 5 numas.
 *      (3) Other functions that compute pixel row statistics are:
 *             pixCountPixelsByRow()
 *             pixAverageByRow()
 *             pixVarianceByRow()
 *             pixGetRowStats()
 * </pre>
 */
l_int32
pixRowStats(PIX    *pixs,
            BOX    *box,
            NUMA  **pnamean,
            NUMA  **pnamedian,
            NUMA  **pnamode,
            NUMA  **pnamodecount,
            NUMA  **pnavar,
            NUMA  **pnarootvar)
{
l_int32     i, j, k, w, h, val, wpls, sum, sumsq, target, max, modeval;
l_int32     xstart, xend, ystart, yend, bw, bh;
l_int32    *histo;
l_uint32   *lines, *datas;
l_float32   norm;
l_float32  *famean, *fameansq, *favar, *farootvar;
l_float32  *famedian, *famode, *famodecount;

    if (pnamean) *pnamean = NULL;
    if (pnamedian) *pnamedian = NULL;
    if (pnamode) *pnamode = NULL;
    if (pnamodecount) *pnamodecount = NULL;
    if (pnavar) *pnavar = NULL;
    if (pnarootvar) *pnarootvar = NULL;
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs undefined or not 8 bpp", __func__, 1);
    famean = fameansq = favar = farootvar = NULL;
    famedian = famode = famodecount = NULL;

    pixGetDimensions(pixs, &w, &h, NULL);
    if (boxClipToRectangleParams(box, w, h, &xstart, &ystart, &xend, &yend,
                                 &bw, &bh) == 1)
        return ERROR_INT("invalid clipping box", __func__, 1);

        /* We need the mean for variance and root variance */
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if (pnamean || pnavar || pnarootvar) {
        norm = 1.f / (l_float32)bw;
        famean = (l_float32 *)LEPT_CALLOC(bh, sizeof(l_float32));
        fameansq = (l_float32 *)LEPT_CALLOC(bh, sizeof(l_float32));
        if (pnavar || pnarootvar) {
            favar = (l_float32 *)LEPT_CALLOC(bh, sizeof(l_float32));
            if (pnarootvar)
                farootvar = (l_float32 *)LEPT_CALLOC(bh, sizeof(l_float32));
        }
        for (i = ystart; i < yend; i++) {
            sum = sumsq = 0;
            lines = datas + i * wpls;
            for (j = xstart; j < xend; j++) {
                val = GET_DATA_BYTE(lines, j);
                sum += val;
                sumsq += val * val;
            }
            famean[i] = norm * sum;
            fameansq[i] = norm * sumsq;
            if (pnavar || pnarootvar) {
                favar[i] = fameansq[i] - famean[i] * famean[i];
                if (pnarootvar)
                    farootvar[i] = sqrtf(favar[i]);
            }
        }
        LEPT_FREE(fameansq);
        if (pnamean)
            *pnamean = numaCreateFromFArray(famean, bh, L_INSERT);
        else
            LEPT_FREE(famean);
        if (pnavar)
            *pnavar = numaCreateFromFArray(favar, bh, L_INSERT);
        else
            LEPT_FREE(favar);
        if (pnarootvar)
            *pnarootvar = numaCreateFromFArray(farootvar, bh, L_INSERT);
    }

        /* We need a histogram to find the median and/or mode values */
    if (pnamedian || pnamode || pnamodecount) {
        histo = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
        if (pnamedian) {
            *pnamedian = numaMakeConstant(0, bh);
            famedian = numaGetFArray(*pnamedian, L_NOCOPY);
        }
        if (pnamode) {
            *pnamode = numaMakeConstant(0, bh);
            famode = numaGetFArray(*pnamode, L_NOCOPY);
        }
        if (pnamodecount) {
            *pnamodecount = numaMakeConstant(0, bh);
            famodecount = numaGetFArray(*pnamodecount, L_NOCOPY);
        }
        for (i = ystart; i < yend; i++) {
            lines = datas + i * wpls;
            memset(histo, 0, 1024);
            for (j = xstart; j < xend; j++) {
                val = GET_DATA_BYTE(lines, j);
                histo[val]++;
            }

            if (pnamedian) {
                sum = 0;
                target = (bw + 1) / 2;
                for (k = 0; k < 256; k++) {
                    sum += histo[k];
                    if (sum >= target) {
                        famedian[i] = k;
                        break;
                    }
                }
            }

            if (pnamode || pnamodecount) {
                max = 0;
                modeval = 0;
                for (k = 0; k < 256; k++) {
                    if (histo[k] > max) {
                        max = histo[k];
                        modeval = k;
                    }
                }
                if (pnamode)
                    famode[i] = modeval;
                if (pnamodecount)
                    famodecount[i] = max;
            }
        }
        LEPT_FREE(histo);
    }

    return 0;
}


/*!
 * \brief   pixColumnStats()
 *
 * \param[in]    pixs          8 bpp; not cmapped
 * \param[in]    box           [optional] clipping box; can be null
 * \param[out]   pnamean       [optional] numa of mean values
 * \param[out]   pnamedian     [optional] numa of median values
 * \param[out]   pnamode       [optional] numa of mode intensity values
 * \param[out]   pnamodecount  [optional] numa of mode counts
 * \param[out]   pnavar        [optional] numa of variance
 * \param[out]   pnarootvar    [optional] numa of square root of variance
 * \return  na numa of requested statistic for each column,
 *                  or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This computes numas that represent row vectors of statistics,
 *          with each of its values derived from the corresponding col of a Pix.
 *      (2) Use NULL on input to prevent computation of any of the 5 numas.
 *      (3) Other functions that compute pixel column statistics are:
 *             pixCountPixelsByColumn()
 *             pixAverageByColumn()
 *             pixVarianceByColumn()
 *             pixGetColumnStats()
 * </pre>
 */
l_int32
pixColumnStats(PIX    *pixs,
               BOX    *box,
               NUMA  **pnamean,
               NUMA  **pnamedian,
               NUMA  **pnamode,
               NUMA  **pnamodecount,
               NUMA  **pnavar,
               NUMA  **pnarootvar)
{
l_int32     i, j, k, w, h, val, wpls, sum, sumsq, target, max, modeval;
l_int32     xstart, xend, ystart, yend, bw, bh;
l_int32    *histo;
l_uint32   *lines, *datas;
l_float32   norm;
l_float32  *famean, *fameansq, *favar, *farootvar;
l_float32  *famedian, *famode, *famodecount;

    if (pnamean) *pnamean = NULL;
    if (pnamedian) *pnamedian = NULL;
    if (pnamode) *pnamode = NULL;
    if (pnamodecount) *pnamodecount = NULL;
    if (pnavar) *pnavar = NULL;
    if (pnarootvar) *pnarootvar = NULL;
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs undefined or not 8 bpp", __func__, 1);
    famean = fameansq = favar = farootvar = NULL;
    famedian = famode = famodecount = NULL;

    pixGetDimensions(pixs, &w, &h, NULL);
    if (boxClipToRectangleParams(box, w, h, &xstart, &ystart, &xend, &yend,
                                 &bw, &bh) == 1)
        return ERROR_INT("invalid clipping box", __func__, 1);

        /* We need the mean for variance and root variance */
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if (pnamean || pnavar || pnarootvar) {
        norm = 1.f / (l_float32)bh;
        famean = (l_float32 *)LEPT_CALLOC(bw, sizeof(l_float32));
        fameansq = (l_float32 *)LEPT_CALLOC(bw, sizeof(l_float32));
        if (pnavar || pnarootvar) {
            favar = (l_float32 *)LEPT_CALLOC(bw, sizeof(l_float32));
            if (pnarootvar)
                farootvar = (l_float32 *)LEPT_CALLOC(bw, sizeof(l_float32));
        }
        for (j = xstart; j < xend; j++) {
            sum = sumsq = 0;
            for (i = ystart, lines = datas; i < yend; lines += wpls, i++) {
                val = GET_DATA_BYTE(lines, j);
                sum += val;
                sumsq += val * val;
            }
            famean[j] = norm * sum;
            fameansq[j] = norm * sumsq;
            if (pnavar || pnarootvar) {
                favar[j] = fameansq[j] - famean[j] * famean[j];
                if (pnarootvar)
                    farootvar[j] = sqrtf(favar[j]);
            }
        }
        LEPT_FREE(fameansq);
        if (pnamean)
            *pnamean = numaCreateFromFArray(famean, bw, L_INSERT);
        else
            LEPT_FREE(famean);
        if (pnavar)
            *pnavar = numaCreateFromFArray(favar, bw, L_INSERT);
        else
            LEPT_FREE(favar);
        if (pnarootvar)
            *pnarootvar = numaCreateFromFArray(farootvar, bw, L_INSERT);
    }

        /* We need a histogram to find the median and/or mode values */
    if (pnamedian || pnamode || pnamodecount) {
        histo = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
        if (pnamedian) {
            *pnamedian = numaMakeConstant(0, bw);
            famedian = numaGetFArray(*pnamedian, L_NOCOPY);
        }
        if (pnamode) {
            *pnamode = numaMakeConstant(0, bw);
            famode = numaGetFArray(*pnamode, L_NOCOPY);
        }
        if (pnamodecount) {
            *pnamodecount = numaMakeConstant(0, bw);
            famodecount = numaGetFArray(*pnamodecount, L_NOCOPY);
        }
        for (j = xstart; j < xend; j++) {
            memset(histo, 0, 1024);
            for (i = ystart, lines = datas; i < yend; lines += wpls, i++) {
                val = GET_DATA_BYTE(lines, j);
                histo[val]++;
            }

            if (pnamedian) {
                sum = 0;
                target = (bh + 1) / 2;
                for (k = 0; k < 256; k++) {
                    sum += histo[k];
                    if (sum >= target) {
                        famedian[j] = k;
                        break;
                    }
                }
            }

            if (pnamode || pnamodecount) {
                max = 0;
                modeval = 0;
                for (k = 0; k < 256; k++) {
                    if (histo[k] > max) {
                        max = histo[k];
                        modeval = k;
                    }
                }
                if (pnamode)
                    famode[j] = modeval;
                if (pnamodecount)
                    famodecount[j] = max;
            }
        }
        LEPT_FREE(histo);
    }

    return 0;
}


/*!
 * \brief   pixGetRangeValues()
 *
 * \param[in]    pixs     8 bpp grayscale, 32 bpp rgb, or colormapped
 * \param[in]    factor   subsampling factor; >= 1; ignored if colormapped
 * \param[in]    color    L_SELECT_RED, L_SELECT_GREEN or L_SELECT_BLUE
 * \param[out]   pminval  [optional] minimum value of component
 * \param[out]   pmaxval  [optional] maximum value of component
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If pixs is 8 bpp grayscale, the color selection type is ignored.
 * </pre>
 */
l_ok
pixGetRangeValues(PIX      *pixs,
                  l_int32   factor,
                  l_int32   color,
                  l_int32  *pminval,
                  l_int32  *pmaxval)
{
l_int32   d;
PIXCMAP  *cmap;

    if (pminval) *pminval = 0;
    if (pmaxval) *pmaxval = 0;
    if (!pminval && !pmaxval)
        return ERROR_INT("no result requested", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

    cmap = pixGetColormap(pixs);
    if (cmap)
        return pixcmapGetRangeValues(cmap, color, pminval, pmaxval,
                                     NULL, NULL);

    if (factor < 1)
        return ERROR_INT("sampling factor must be >= 1", __func__, 1);
    d = pixGetDepth(pixs);
    if (d != 8 && d != 32)
        return ERROR_INT("pixs not 8 or 32 bpp", __func__, 1);

    if (d == 8) {
        pixGetExtremeValue(pixs, factor, L_SELECT_MIN,
                           NULL, NULL, NULL, pminval);
        pixGetExtremeValue(pixs, factor, L_SELECT_MAX,
                           NULL, NULL, NULL, pmaxval);
    } else if (color == L_SELECT_RED) {
        pixGetExtremeValue(pixs, factor, L_SELECT_MIN,
                           pminval, NULL, NULL, NULL);
        pixGetExtremeValue(pixs, factor, L_SELECT_MAX,
                           pmaxval, NULL, NULL, NULL);
    } else if (color == L_SELECT_GREEN) {
        pixGetExtremeValue(pixs, factor, L_SELECT_MIN,
                           NULL, pminval, NULL, NULL);
        pixGetExtremeValue(pixs, factor, L_SELECT_MAX,
                           NULL, pmaxval, NULL, NULL);
    } else if (color == L_SELECT_BLUE) {
        pixGetExtremeValue(pixs, factor, L_SELECT_MIN,
                           NULL, NULL, pminval, NULL);
        pixGetExtremeValue(pixs, factor, L_SELECT_MAX,
                           NULL, NULL, pmaxval, NULL);
    } else {
        return ERROR_INT("invalid color", __func__, 1);
    }

    return 0;
}


/*!
 * \brief   pixGetExtremeValue()
 *
 * \param[in]    pixs      8 bpp grayscale, 32 bpp rgb, or colormapped
 * \param[in]    factor    subsampling factor; >= 1; ignored if colormapped
 * \param[in]    type      L_SELECT_MIN or L_SELECT_MAX
 * \param[out]   prval     [optional] red component
 * \param[out]   pgval     [optional] green component
 * \param[out]   pbval     [optional] blue component
 * \param[out]   pgrayval  [optional] min or max gray value
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If pixs is grayscale, the result is returned in &grayval.
 *          Otherwise, if there is a colormap or d == 32,
 *          each requested color component is returned.  At least
 *          one color component (address) must be input.
 * </pre>
 */
l_ok
pixGetExtremeValue(PIX      *pixs,
                   l_int32   factor,
                   l_int32   type,
                   l_int32  *prval,
                   l_int32  *pgval,
                   l_int32  *pbval,
                   l_int32  *pgrayval)
{
l_int32    i, j, w, h, d, wpl;
l_int32    val, extval, rval, gval, bval, extrval, extgval, extbval;
l_uint32   pixel;
l_uint32  *data, *line;
PIXCMAP   *cmap;

    if (prval) *prval = -1;
    if (pgval) *pgval = -1;
    if (pbval) *pbval = -1;
    if (pgrayval) *pgrayval = -1;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (type != L_SELECT_MIN && type != L_SELECT_MAX)
        return ERROR_INT("invalid type", __func__, 1);

    cmap = pixGetColormap(pixs);
    if (cmap) {
        if (type == L_SELECT_MIN) {
            if (prval) pixcmapGetRangeValues(cmap, L_SELECT_RED, prval, NULL,
                                             NULL, NULL);
            if (pgval) pixcmapGetRangeValues(cmap, L_SELECT_GREEN, pgval, NULL,
                                             NULL, NULL);
            if (pbval) pixcmapGetRangeValues(cmap, L_SELECT_BLUE, pbval, NULL,
                                             NULL, NULL);
        } else {  /* type == L_SELECT_MAX */
            if (prval) pixcmapGetRangeValues(cmap, L_SELECT_RED, NULL, prval,
                                             NULL, NULL);
            if (pgval) pixcmapGetRangeValues(cmap, L_SELECT_GREEN, NULL, pgval,
                                             NULL, NULL);
            if (pbval) pixcmapGetRangeValues(cmap, L_SELECT_BLUE, NULL, pbval,
                                             NULL, NULL);
        }
        return 0;
    }

    pixGetDimensions(pixs, &w, &h, &d);
    if (factor < 1)
        return ERROR_INT("sampling factor must be >= 1", __func__, 1);
    if (d != 8 && d != 32)
        return ERROR_INT("pixs not 8 or 32 bpp", __func__, 1);
    if (d == 8 && !pgrayval)
        return ERROR_INT("can't return result in grayval", __func__, 1);
    if (d == 32 && !prval && !pgval && !pbval)
        return ERROR_INT("can't return result in r/g/b-val", __func__, 1);

    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    if (d == 8) {
        if (type == L_SELECT_MIN)
            extval = 100000;
        else  /* get max */
            extval = -1;

        for (i = 0; i < h; i += factor) {
            line = data + i * wpl;
            for (j = 0; j < w; j += factor) {
                val = GET_DATA_BYTE(line, j);
                if ((type == L_SELECT_MIN && val < extval) ||
                    (type == L_SELECT_MAX && val > extval))
                    extval = val;
            }
        }
        *pgrayval = extval;
        return 0;
    }

        /* 32 bpp rgb */
    if (type == L_SELECT_MIN) {
        extrval = 100000;
        extgval = 100000;
        extbval = 100000;
    } else {
        extrval = -1;
        extgval = -1;
        extbval = -1;
    }
    for (i = 0; i < h; i += factor) {
        line = data + i * wpl;
        for (j = 0; j < w; j += factor) {
            pixel = line[j];
            if (prval) {
                rval = (pixel >> L_RED_SHIFT) & 0xff;
                if ((type == L_SELECT_MIN && rval < extrval) ||
                    (type == L_SELECT_MAX && rval > extrval))
                    extrval = rval;
            }
            if (pgval) {
                gval = (pixel >> L_GREEN_SHIFT) & 0xff;
                if ((type == L_SELECT_MIN && gval < extgval) ||
                    (type == L_SELECT_MAX && gval > extgval))
                    extgval = gval;
            }
            if (pbval) {
                bval = (pixel >> L_BLUE_SHIFT) & 0xff;
                if ((type == L_SELECT_MIN && bval < extbval) ||
                    (type == L_SELECT_MAX && bval > extbval))
                    extbval = bval;
            }
        }
    }
    if (prval) *prval = extrval;
    if (pgval) *pgval = extgval;
    if (pbval) *pbval = extbval;
    return 0;
}


/*!
 * \brief   pixGetMaxValueInRect()
 *
 * \param[in]    pixs     8, 16 or 32 bpp grayscale; no color space components
 * \param[in]    box      [optional] region; set box = NULL to use entire pixs
 * \param[out]   pmaxval  [optional] max value in region
 * \param[out]   pxmax    [optional] x location of max value
 * \param[out]   pymax    [optional] y location of max value
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This can be used to find the maximum and its location
 *          in a 2-dimensional histogram, where the x and y directions
 *          represent two color components (e.g., saturation and hue).
 *      (2) Note that here a 32 bpp pixs has pixel values that are simply
 *          numbers.  They are not 8 bpp components in a colorspace.
 * </pre>
 */
l_ok
pixGetMaxValueInRect(PIX       *pixs,
                     BOX       *box,
                     l_uint32  *pmaxval,
                     l_int32   *pxmax,
                     l_int32   *pymax)
{
l_int32    i, j, w, h, d, wpl, bw, bh;
l_int32    xstart, ystart, xend, yend, xmax, ymax;
l_uint32   val, maxval;
l_uint32  *data, *line;

    if (pmaxval) *pmaxval = 0;
    if (pxmax) *pxmax = 0;
    if (pymax) *pymax = 0;
    if (!pmaxval && !pxmax && !pymax)
        return ERROR_INT("no data requested", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (pixGetColormap(pixs) != NULL)
        return ERROR_INT("pixs has colormap", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8 && d != 16 && d != 32)
        return ERROR_INT("pixs not 8, 16 or 32 bpp", __func__, 1);

    xstart = ystart = 0;
    xend = w - 1;
    yend = h - 1;
    if (box) {
        boxGetGeometry(box, &xstart, &ystart, &bw, &bh);
        xend = xstart + bw - 1;
        yend = ystart + bh - 1;
    }

    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    maxval = 0;
    xmax = ymax = 0;
    for (i = ystart; i <= yend; i++) {
        line = data + i * wpl;
        for (j = xstart; j <= xend; j++) {
            if (d == 8)
                val = GET_DATA_BYTE(line, j);
            else if (d == 16)
                val = GET_DATA_TWO_BYTES(line, j);
            else  /* d == 32 */
                val = line[j];
            if (val > maxval) {
                maxval = val;
                xmax = j;
                ymax = i;
            }
        }
    }
    if (maxval == 0) {  /* no counts; pick the center of the rectangle */
        xmax = (xstart + xend) / 2;
        ymax = (ystart + yend) / 2;
    }

    if (pmaxval) *pmaxval = maxval;
    if (pxmax) *pxmax = xmax;
    if (pymax) *pymax = ymax;
    return 0;
}


/*!
 * \brief   pixGetMaxColorIndex()
 *
 * \param[in]    pixs          1, 2, 4 or 8 bpp colormapped
 * \param[out]   pmaxindex     max colormap index value
 * \return  0 if OK, 1 on error
 */
l_ok
pixGetMaxColorIndex(PIX      *pixs,
                    l_int32  *pmaxindex)
{
l_int32    i, j, w, h, d, wpl, val, max, maxval, empty;
l_uint32  *data, *line;

    if (!pmaxindex)
        return ERROR_INT("&maxindex not defined", __func__, 1);
    *pmaxindex = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 1 && d != 2 && d != 4 && d != 8)
        return ERROR_INT("invalid pixs depth; not in (1,2,4,8}", __func__, 1);

    wpl = pixGetWpl(pixs);
    data = pixGetData(pixs);
    max = 0;
    maxval = (1 << d) - 1;
    if (d == 1) {
        pixZero(pixs, &empty);
        if (!empty) max = 1;
        *pmaxindex = max;
        return 0;
    }
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        if (d == 2) {
            for (j = 0; j < w; j++) {
                val = GET_DATA_DIBIT(line, j);
                if (val > max) max = val;
            }
        } else if (d == 4) {
            for (j = 0; j < w; j++) {
                val = GET_DATA_QBIT(line, j);
                if (val > max) max = val;
            }
        } else if (d == 8) {
            for (j = 0; j < w; j++) {
                val = GET_DATA_BYTE(line, j);
                if (val > max) max = val;
            }
        }
        if (max == maxval) break;
    }
    *pmaxindex = max;
    return 0;
}


/*!
 * \brief   pixGetBinnedComponentRange()
 *
 * \param[in]    pixs      32 bpp rgb
 * \param[in]    nbins     number of equal population bins; must be > 1
 * \param[in]    factor    subsampling factor; >= 1
 * \param[in]    color     L_SELECT_RED, L_SELECT_GREEN or L_SELECT_BLUE
 * \param[out]   pminval   [optional] minimum value of component
 * \param[out]   pmaxval   [optional] maximum value of component
 * \param[out]   pcarray   [optional] color array of bins
 * \param[in]    fontsize  [optional] 0 for no debug; for debug, valid set
 *                         is {4,6,8,10,12,14,16,18,20}.
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This returns the min and max average values of the
 *          selected color component in the set of rank bins,
 *          where the ranking is done using the specified component.
 * </pre>
 */
l_ok
pixGetBinnedComponentRange(PIX        *pixs,
                           l_int32     nbins,
                           l_int32     factor,
                           l_int32     color,
                           l_int32    *pminval,
                           l_int32    *pmaxval,
                           l_uint32  **pcarray,
                           l_int32     fontsize)
{
l_int32    i, minval, maxval, rval, gval, bval;
l_uint32  *carray;
PIX       *pixt;

    if (pminval) *pminval = 0;
    if (pmaxval) *pmaxval = 0;
    if (pcarray) *pcarray = NULL;
    if (!pminval && !pmaxval)
        return ERROR_INT("no result requested", __func__, 1);
    if (!pixs || pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs not defined or not 32 bpp", __func__, 1);
    if (factor < 1)
        return ERROR_INT("sampling factor must be >= 1", __func__, 1);
    if (color != L_SELECT_RED && color != L_SELECT_GREEN &&
        color != L_SELECT_BLUE)
        return ERROR_INT("invalid color", __func__, 1);
    if (fontsize < 0 || fontsize > 20 || fontsize & 1 || fontsize == 2)
        return ERROR_INT("invalid fontsize", __func__, 1);

    pixGetRankColorArray(pixs, nbins, color, factor, &carray, NULL, 0);
    if (!carray)
        return ERROR_INT("carray not made", __func__, 1);

    if (fontsize > 0) {
        for (i = 0; i < nbins; i++)
            L_INFO("c[%d] = %x\n", __func__, i, carray[i]);
        pixt = pixDisplayColorArray(carray, nbins, 200, 5, fontsize);
        pixDisplay(pixt, 100, 100);
        pixDestroy(&pixt);
    }

    extractRGBValues(carray[0], &rval, &gval, &bval);
    minval = rval;
    if (color == L_SELECT_GREEN)
        minval = gval;
    else if (color == L_SELECT_BLUE)
        minval = bval;
    extractRGBValues(carray[nbins - 1], &rval, &gval, &bval);
    maxval = rval;
    if (color == L_SELECT_GREEN)
        maxval = gval;
    else if (color == L_SELECT_BLUE)
        maxval = bval;

    if (pminval) *pminval = minval;
    if (pmaxval) *pmaxval = maxval;
    if (pcarray)
        *pcarray = carray;
    else
        LEPT_FREE(carray);
    return 0;
}


/*!
 * \brief   pixGetRankColorArray()
 *
 * \param[in]    pixs       32 bpp or cmapped
 * \param[in]    nbins      number of equal population bins; must be > 1
 * \param[in]    type       color selection flag
 * \param[in]    factor     subsampling factor; integer >= 1
 * \param[out]   pcarray    array of colors, ranked by intensity
 * \param[in]    pixadb     [optional] debug: caller passes this in.
 *                          Use to display color squares and to
 *                          capture plots of color components
 * \param[in]    fontsize   [optional] debug: only used if pixadb exists.
 *                          Valid set is {4,6,8,10,12,14,16,18,20}.
 *                          fontsize == 6 is typical.
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The color selection flag is one of: L_SELECT_RED, L_SELECT_GREEN,
 *          L_SELECT_BLUE, L_SELECT_MIN, L_SELECT_MAX, L_SELECT_AVERAGE,
 *          L_SELECT_HUE, L_SELECT_SATURATION.
 *      (2) The pixels are ordered by the value of the selected color
            value, and an equal number are placed in %nbins.  The average
 *          color in each bin is returned in a color array with %nbins colors.
 *      (3) Set the subsampling factor > 1 to reduce the amount of
 *          computation.  Typically you want at least 10,000 pixels
 *          for reasonable statistics.  Must be at least 10 samples/bin.
 *      (4) A crude "rank color" as a function of rank can be found from
 *             rankint = (l_int32)(rank * (nbins - 1) + 0.5);
 *             extractRGBValues(array[rankint], &rval, &gval, &bval);
 *          where the rank is in [0.0 ... 1.0].
 * </pre>
 */
l_ok
pixGetRankColorArray(PIX        *pixs,
                     l_int32     nbins,
                     l_int32     type,
                     l_int32     factor,
                     l_uint32  **pcarray,
                     PIXA       *pixadb,
                     l_int32     fontsize)
{
l_int32    ret, w, h, samplesperbin;
l_uint32  *array;
PIX       *pix1, *pixc, *pixg, *pixd;
PIXCMAP   *cmap;

    if (!pcarray)
        return ERROR_INT("&carray not defined", __func__, 1);
    *pcarray = NULL;
    if (factor < 1)
        return ERROR_INT("sampling factor must be >= 1", __func__, 1);
    if (nbins < 2)
        return ERROR_INT("nbins must be at least 2", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    cmap = pixGetColormap(pixs);
    if (pixGetDepth(pixs) != 32 && !cmap)
        return ERROR_INT("pixs neither 32 bpp nor cmapped", __func__, 1);
    if (type != L_SELECT_RED && type != L_SELECT_GREEN &&
        type != L_SELECT_BLUE && type != L_SELECT_MIN &&
        type != L_SELECT_MAX && type != L_SELECT_AVERAGE &&
        type != L_SELECT_HUE && type != L_SELECT_SATURATION)
        return ERROR_INT("invalid type", __func__, 1);
    if (pixadb) {
        if (fontsize < 0 || fontsize > 20 || fontsize & 1 || fontsize == 2) {
            L_WARNING("invalid fontsize %d; setting to 6\n", __func__,
                      fontsize);
            fontsize = 6;
        }
    }
    pixGetDimensions(pixs, &w, &h, NULL);
    samplesperbin = (w * h) / (factor * factor * nbins);
    if (samplesperbin < 10) {
        L_ERROR("samplesperbin = %d < 10\n", __func__, samplesperbin);
        return 1;
    }

        /* Downscale by factor and remove colormap if it exists */
    pix1 = pixScaleByIntSampling(pixs, factor);
    if (cmap)
        pixc = pixRemoveColormap(pix1, REMOVE_CMAP_TO_FULL_COLOR);
    else
        pixc = pixClone(pix1);
    pixDestroy(&pix1);

        /* Convert to an 8 bit version for ordering the pixels */
    pixg = pixConvertRGBToGrayGeneral(pixc, type, 0.0, 0.0, 0.0);

        /* Get the average color in each bin for pixels whose grayscale
         * values are in the range for that bin. */
    pixGetBinnedColor(pixc, pixg, 1, nbins, pcarray, pixadb);
    ret = 0;
    if ((array = *pcarray) == NULL) {
        L_ERROR("color array not returned\n", __func__);
        ret = 1;
    }
    if (array && pixadb) {
        pixd = pixDisplayColorArray(array, nbins, 200, 5, fontsize);
        pixWriteDebug("/tmp/lept/regout/rankhisto.png", pixd, IFF_PNG);
        pixDestroy(&pixd);
    }

    pixDestroy(&pixc);
    pixDestroy(&pixg);
    return ret;
}


/*!
 * \brief   pixGetBinnedColor()
 *
 * \param[in]    pixs       32 bpp
 * \param[in]    pixg       8 bpp grayscale version of pixs
 * \param[in]    factor     sampling factor along pixel counting direction
 * \param[in]    nbins      number of bins based on grayscale value {1,...,100}
 * \param[out]   pcarray    array of average color values in each bin
 * \param[in]    pixadb     [optional] debug: caller passes this in.
 *                          Use to display output color squares and plots of
 *                          color components.
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This takes a color image, a grayscale version, and the number
 *          of requested bins.  The pixels are ordered by the corresponding
 *          gray value and an equal number of pixels are put in each bin.
 *          The average color for each bin is returned as an array
 *          of l_uint32 colors in our standard RGBA ordering.  We require
 *          at least 5 pixels in each bin.
 *      (2) This is used by pixGetRankColorArray(), which generates the
 *          grayscale image %pixg from the color image %pixs.
 *      (3) Arrays of float64 are used for intermediate storage, without
 *          loss of precision, of the sampled uint32 pixel values.
 * </pre>
 */
l_ok
pixGetBinnedColor(PIX        *pixs,
                  PIX        *pixg,
                  l_int32     factor,
                  l_int32     nbins,
                  l_uint32  **pcarray,
                  PIXA       *pixadb)
{
l_int32     i, j, w, h, wpls, wplg;
l_int32     count, bincount, binindex, binsize, npts, avepts, ntot;
l_int32     rval, gval, bval, grayval, rave, gave, bave;
l_uint32   *datas, *datag, *lines, *lineg, *carray;
l_float64   val64, rsum, gsum, bsum;
L_DNAA     *daa;
NUMA       *naeach;
PIX        *pix1;

    if (!pcarray)
        return ERROR_INT("&carray not defined", __func__, 1);
    *pcarray = NULL;
    if (!pixs || pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs undefined or not 32 bpp", __func__, 1);
    if (!pixg || pixGetDepth(pixg) != 8)
        return ERROR_INT("pixg undefined or not 8 bpp", __func__, 1);
    if (factor < 1) {
        L_WARNING("sampling factor less than 1; setting to 1\n", __func__);
        factor = 1;
    }
    if (nbins < 1 || nbins > 100)
        return ERROR_INT("nbins not in [1,100]", __func__, 1);

        /* Require that each bin has at least 5 pixels. */
    pixGetDimensions(pixs, &w, &h, NULL);
    npts = (w + factor - 1) * (h + factor - 1) / (factor * factor);
    avepts = (npts + nbins - 1) / nbins;  /* average number of pts in a bin */
    if (avepts < 5) {
        L_ERROR("avepts = %d; must be >= 5\n", __func__, avepts);
        return 1;
    }

    /* ------------------------------------------------------------ *
     * Find the average color for each bin.  The colors are ordered *
     * by the gray value in the corresponding pixel in %pixg.       *
     * The bins have equal numbers of pixels (within 1).            *
     * ------------------------------------------------------------ */

        /* Generate a dnaa, where each dna has the colors corresponding
         * to the grayscale value given by the index of the dna in the dnaa */
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datag = pixGetData(pixg);
    wplg = pixGetWpl(pixg);
    daa = l_dnaaCreateFull(256, 0);
    for (i = 0; i < h; i += factor) {
        lines = datas + i * wpls;
        lineg = datag + i * wplg;
        for (j = 0; j < w; j += factor) {
            grayval = GET_DATA_BYTE(lineg, j);
            l_dnaaAddNumber(daa, grayval, lines[j]);
        }
    }

    if (pixadb) {
        NUMA  *na, *nabinval, *narank;
        na = numaCreate(256);  /* grayscale histogram */
        for (i = 0; i < 256; i++)
            numaAddNumber(na, l_dnaaGetDnaCount(daa, i));

            /* Plot the gray bin value and the rank(gray) values */
        numaDiscretizeHistoInBins(na, nbins, &nabinval, &narank);
        pix1 = gplotSimplePix1(nabinval, "Gray value in each bin");
        pixaAddPix(pixadb, pix1, L_INSERT);
        pix1 = gplotSimplePix1(narank, "rank as function of gray value");
        pixaAddPix(pixadb, pix1, L_INSERT);
        numaDestroy(&na);
        numaDestroy(&nabinval);
        numaDestroy(&narank);
    }

        /* Get the number of items in each bin */
    ntot = l_dnaaGetNumberCount(daa);
    if ((naeach = numaGetUniformBinSizes(ntot, nbins)) == NULL) {
        l_dnaaDestroy(&daa);
        return ERROR_INT("naeach not made", __func__, 1);
    }

        /* Get the average color in each bin.  This algorithm is
         * esssentially the same as in numaDiscretizeHistoInBins() */
    carray = (l_uint32 *)LEPT_CALLOC(nbins, sizeof(l_uint32));
    rsum = gsum = bsum = 0.0;
    bincount = 0;
    binindex = 0;
    numaGetIValue(naeach, 0, &binsize);
    for (i = 0; i < 256; i++) {
        count = l_dnaaGetDnaCount(daa, i);
        for (j = 0; j < count; j++) {
            bincount++;
            l_dnaaGetValue(daa, i, j, &val64);
            extractRGBValues((l_uint32)val64, &rval, &gval, &bval);
            rsum += rval;
            gsum += gval;
            bsum += bval;
            if (bincount == binsize) {  /* add bin entry */
                rave = (l_int32)(rsum / binsize + 0.5);
                gave = (l_int32)(gsum / binsize + 0.5);
                bave = (l_int32)(bsum / binsize + 0.5);
                composeRGBPixel(rave, gave, bave, carray + binindex);
                rsum = gsum = bsum = 0.0;
                bincount = 0;
                binindex++;
                if (binindex == nbins) break;
                numaGetIValue(naeach, binindex, &binsize);
            }
        }
        if (binindex == nbins) break;
    }
    if (binindex != nbins)
        L_ERROR("binindex = %d != nbins = %d\n", __func__, binindex, nbins);

    if (pixadb) {
        NUMA  *nared, *nagreen, *nablue;
        nared = numaCreate(nbins);
        nagreen = numaCreate(nbins);
        nablue = numaCreate(nbins);
        for (i = 0; i < nbins; i++) {
            extractRGBValues(carray[i], &rval, &gval, &bval);
            numaAddNumber(nared, rval);
            numaAddNumber(nagreen, gval);
            numaAddNumber(nablue, bval);
        }
        lept_mkdir("lept/regout");
        pix1 = gplotSimplePix1(nared, "Average red val vs. rank bin");
        pixaAddPix(pixadb, pix1, L_INSERT);
        pix1 = gplotSimplePix1(nagreen, "Average green val vs. rank bin");
        pixaAddPix(pixadb, pix1, L_INSERT);
        pix1 = gplotSimplePix1(nablue, "Average blue val vs. rank bin");
        pixaAddPix(pixadb, pix1, L_INSERT);
        numaDestroy(&nared);
        numaDestroy(&nagreen);
        numaDestroy(&nablue);
    }

    *pcarray = carray;
    numaDestroy(&naeach);
    l_dnaaDestroy(&daa);
    return 0;
}


/*!
 * \brief   pixDisplayColorArray()
 *
 * \param[in]   carray    array of colors: 0xrrggbb00
 * \param[in]   ncolors   size of array
 * \param[in]   side      size of each color square; suggest 200
 * \param[in]   ncols     number of columns in output color matrix
 * \param[in]   fontsize  to label each square with text.
 *                        Valid set is {4,6,8,10,12,14,16,18,20}.
 *                        Suggest 6 for 200x200 square. Use 0 to disable.
 * \return  pixd color array, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This generates an array of labeled color squares from an
 *          array of color values.
 *      (2) To make a single color square, use pixMakeColorSquare().
 * </pre>
 */
PIX *
pixDisplayColorArray(l_uint32  *carray,
                     l_int32    ncolors,
                     l_int32    side,
                     l_int32    ncols,
                     l_int32    fontsize)
{
char     textstr[256];
l_int32  i, rval, gval, bval;
L_BMF   *bmf;
PIX     *pix1, *pix2, *pix3, *pix4;
PIXA    *pixa;

    if (!carray)
        return (PIX *)ERROR_PTR("carray not defined", __func__, NULL);
    if (fontsize < 0 || fontsize > 20 || fontsize & 1 || fontsize == 2)
        return (PIX *)ERROR_PTR("invalid fontsize", __func__, NULL);

    bmf = (fontsize == 0) ? NULL : bmfCreate(NULL, fontsize);
    pixa = pixaCreate(ncolors);
    for (i = 0; i < ncolors; i++) {
        pix1 = pixCreate(side, side, 32);
        pixSetAllArbitrary(pix1, carray[i]);
        pix2 = pixAddBorder(pix1, 2, 1);
        if (bmf) {
            extractRGBValues(carray[i], &rval, &gval, &bval);
            snprintf(textstr, sizeof(textstr),
                     "%d: (%d %d %d)", i, rval, gval, bval);
            pix3 = pixAddSingleTextblock(pix2, bmf, textstr, 0xff000000,
                                         L_ADD_BELOW, NULL);
        } else {
            pix3 = pixClone(pix2);
        }
        pixaAddPix(pixa, pix3, L_INSERT);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }
    pix4 = pixaDisplayTiledInColumns(pixa, ncols, 1.0, 20, 2);
    pixaDestroy(&pixa);
    bmfDestroy(&bmf);
    return pix4;
}


/*!
 * \brief   pixRankBinByStrip()
 *
 * \param[in]   pixs       32 bpp or cmapped
 * \param[in]   direction  L_SCAN_HORIZONTAL or L_SCAN_VERTICAL
 * \param[in]   size       of strips in scan direction
 * \param[in]   nbins      number of equal population bins; must be > 1
 * \param[in]   type       color selection flag
 * \return  pixd result, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a pix of height %nbins, where each column
 *          represents a horizontal or vertical strip of the input image.
 *          If %direction == L_SCAN_HORIZONTAL, the input image is
 *          tiled into vertical strips of width %size, where %size is
 *          chosen as a compromise between getting better spatial
 *          columnwise resolution (small %size) and getting better
 *          columnwise statistical information (larger %size).  Likewise
 *          with rows of the image if %direction == L_SCAN_VERTICAL.
 *      (2) For L_HORIZONTAL_SCAN, the output pix contains rank binned
 *          median colors in each column that correspond to a vertical
 *          strip of width %size in the input image.
 *      (3) The color selection flag is one of: L_SELECT_RED, L_SELECT_GREEN,
 *          L_SELECT_BLUE, L_SELECT_MIN, L_SELECT_MAX, L_SELECT_AVERAGE,
 *          L_SELECT_HUE, L_SELECT_SATURATION.
 *          It determines how the rank ordering is done.
 *      (4) Typical input values might be %size = 5, %nbins = 10.
 * </pre>
 */
PIX *
pixRankBinByStrip(PIX     *pixs,
                  l_int32  direction,
                  l_int32  size,
                  l_int32  nbins,
                  l_int32  type)
{
l_int32    i, j, w, h, mindim, nstrips;
l_uint32  *array;
BOXA      *boxa;
PIX       *pix1, *pix2, *pixd;
PIXA      *pixa;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    cmap = pixGetColormap(pixs);
    if (pixGetDepth(pixs) != 32 && !cmap)
        return (PIX *)ERROR_PTR("pixs neither 32 bpp nor cmapped",
                                __func__, NULL);
    if (direction != L_SCAN_HORIZONTAL && direction != L_SCAN_VERTICAL)
        return (PIX *)ERROR_PTR("invalid direction", __func__, NULL);
    if (size < 1)
        return (PIX *)ERROR_PTR("size < 1", __func__, NULL);
    if (nbins < 2)
        return (PIX *)ERROR_PTR("nbins must be at least 2", __func__, NULL);
    if (type != L_SELECT_RED && type != L_SELECT_GREEN &&
        type != L_SELECT_BLUE && type != L_SELECT_MIN &&
        type != L_SELECT_MAX && type != L_SELECT_AVERAGE &&
        type != L_SELECT_HUE && type != L_SELECT_SATURATION)
        return (PIX *)ERROR_PTR("invalid type", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    mindim = L_MIN(w, h);
    if (mindim < 20 || nbins > mindim)
        return (PIX *)ERROR_PTR("pix too small and/or too many bins",
                                __func__, NULL);

        /* Remove colormap if it exists */
    if (cmap)
        pix1 = pixRemoveColormap(pixs, REMOVE_CMAP_TO_FULL_COLOR);
    else
        pix1 = pixClone(pixs);
    pixGetDimensions(pixs, &w, &h, NULL);

    pixd = NULL;
    boxa = makeMosaicStrips(w, h, direction, size);
    pixa = pixClipRectangles(pix1, boxa);
    nstrips = pixaGetCount(pixa);
    if (direction == L_SCAN_HORIZONTAL) {
        pixd = pixCreate(nstrips, nbins, 32);
        for (i = 0; i < nstrips; i++) {
            pix2 = pixaGetPix(pixa, i, L_CLONE);
            pixGetRankColorArray(pix2, nbins, type, 1, &array, NULL, 0);
            if (array) {
                for (j = 0; j < nbins; j++)
                    pixSetPixel(pixd, i, j, array[j]);
                LEPT_FREE(array);
            }
            pixDestroy(&pix2);
        }
    } else {  /* L_SCAN_VERTICAL */
        pixd = pixCreate(nbins, nstrips, 32);
        for (i = 0; i < nstrips; i++) {
            pix2 = pixaGetPix(pixa, i, L_CLONE);
            pixGetRankColorArray(pix2, nbins, type, 1, &array, NULL, 0);
            if (array) {
                for (j = 0; j < nbins; j++)
                    pixSetPixel(pixd, j, i, array[j]);
                LEPT_FREE(array);
            }
            pixDestroy(&pix2);
        }
    }
    pixDestroy(&pix1);
    boxaDestroy(&boxa);
    pixaDestroy(&pixa);
    return pixd;
}



/*-------------------------------------------------------------*
 *                 Pixelwise aligned statistics                *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixaGetAlignedStats()
 *
 * \param[in]   pixa    of identically sized, 8 bpp pix; not cmapped
 * \param[in]   type    L_MEAN_ABSVAL, L_MEDIAN_VAL, L_MODE_VAL, L_MODE_COUNT
 * \param[in]   nbins   of histogram for median and mode; ignored for mean
 * \param[in]   thresh  on histogram for mode val; ignored for all other types
 * \return  pix with pixelwise aligned stats, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) Each pixel in the returned pix represents an average
 *          (or median, or mode) over the corresponding pixels in each
 *          pix in the pixa.
 *      (2) The %thresh parameter works with L_MODE_VAL only, and
 *          sets a minimum occupancy of the mode bin.
 *          If the occupancy of the mode bin is less than %thresh, the
 *          mode value is returned as 0.  To always return the actual
 *          mode value, set %thresh = 0.  See pixGetRowStats().
 * </pre>
 */
PIX *
pixaGetAlignedStats(PIXA     *pixa,
                    l_int32   type,
                    l_int32   nbins,
                    l_int32   thresh)
{
l_int32     j, n, w, h, d;
l_float32  *colvect;
PIX        *pixt, *pixd;

    if (!pixa)
        return (PIX *)ERROR_PTR("pixa not defined", __func__, NULL);
    if (type != L_MEAN_ABSVAL && type != L_MEDIAN_VAL &&
        type != L_MODE_VAL && type != L_MODE_COUNT)
        return (PIX *)ERROR_PTR("invalid type", __func__, NULL);
    n = pixaGetCount(pixa);
    if (n == 0)
        return (PIX *)ERROR_PTR("no pix in pixa", __func__, NULL);
    pixaGetPixDimensions(pixa, 0, &w, &h, &d);
    if (d != 8)
        return (PIX *)ERROR_PTR("pix not 8 bpp", __func__, NULL);

    pixd = pixCreate(w, h, 8);
    pixt = pixCreate(n, h, 8);
    colvect = (l_float32 *)LEPT_CALLOC(h, sizeof(l_float32));
    for (j = 0; j < w; j++) {
        pixaExtractColumnFromEachPix(pixa, j, pixt);
        pixGetRowStats(pixt, type, nbins, thresh, colvect);
        pixSetPixelColumn(pixd, j, colvect);
    }

    LEPT_FREE(colvect);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixaExtractColumnFromEachPix()
 *
 * \param[in]   pixa   of identically sized, 8 bpp; not cmapped
 * \param[in]   col    column index
 * \param[in]   pixd   pix into which each column is inserted
 * \return  0 if OK, 1 on error
 */
l_ok
pixaExtractColumnFromEachPix(PIXA    *pixa,
                             l_int32  col,
                             PIX     *pixd)
{
l_int32    i, k, n, w, h, ht, val, wplt, wpld;
l_uint32  *datad, *datat;
PIX       *pixt;

    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (!pixd || pixGetDepth(pixd) != 8)
        return ERROR_INT("pixd not defined or not 8 bpp", __func__, 1);
    n = pixaGetCount(pixa);
    pixGetDimensions(pixd, &w, &h, NULL);
    if (n != w)
        return ERROR_INT("pix width != n", __func__, 1);
    pixt = pixaGetPix(pixa, 0, L_CLONE);
    wplt = pixGetWpl(pixt);
    pixGetDimensions(pixt, NULL, &ht, NULL);
    pixDestroy(&pixt);
    if (h != ht)
        return ERROR_INT("pixd height != column height", __func__, 1);

    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    for (k = 0; k < n; k++) {
        pixt = pixaGetPix(pixa, k, L_CLONE);
        datat = pixGetData(pixt);
        for (i = 0; i < h; i++) {
            val = GET_DATA_BYTE(datat, col);
            SET_DATA_BYTE(datad + i * wpld, k, val);
            datat += wplt;
        }
        pixDestroy(&pixt);
    }

    return 0;
}


/*!
 * \brief   pixGetRowStats()
 *
 * \param[in]   pixs     8 bpp; not cmapped
 * \param[in]   type     L_MEAN_ABSVAL, L_MEDIAN_VAL, L_MODE_VAL, L_MODE_COUNT
 * \param[in]   nbins    of histogram for median and mode; ignored for mean
 * \param[in]   thresh   on histogram for mode; ignored for mean and median
 * \param[in]   colvect  vector of results gathered across the rows of pixs
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This computes a column vector of statistics using each
 *          row of a Pix.  The result is put in %colvect.
 *      (2) The %thresh parameter works with L_MODE_VAL only, and
 *          sets a minimum occupancy of the mode bin.
 *          If the occupancy of the mode bin is less than %thresh, the
 *          mode value is returned as 0.  To always return the actual
 *          mode value, set %thresh = 0.
 *      (3) What is the meaning of this %thresh parameter?
 *          For each row, the total count in the histogram is w, the
 *          image width.  So %thresh, relative to w, gives a measure
 *          of the ratio of the bin width to the width of the distribution.
 *          The larger %thresh, the narrower the distribution must be
 *          for the mode value to be returned (instead of returning 0).
 *      (4) If the Pix consists of a set of corresponding columns,
 *          one for each Pix in a Pixa, the width of the Pix is the
 *          number of Pix in the Pixa and the column vector can
 *          be stored as a column in a Pix of the same size as
 *          each Pix in the Pixa.
 * </pre>
 */
l_ok
pixGetRowStats(PIX        *pixs,
               l_int32     type,
               l_int32     nbins,
               l_int32     thresh,
               l_float32  *colvect)
{
l_int32    i, j, k, w, h, val, wpls, sum, target, max, modeval;
l_int32   *histo, *gray2bin, *bin2gray;
l_uint32  *lines, *datas;

    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8 bpp", __func__, 1);
    if (!colvect)
        return ERROR_INT("colvect not defined", __func__, 1);
    if (type != L_MEAN_ABSVAL && type != L_MEDIAN_VAL &&
        type != L_MODE_VAL && type != L_MODE_COUNT)
        return ERROR_INT("invalid type", __func__, 1);
    if (type != L_MEAN_ABSVAL && (nbins < 1 || nbins > 256))
        return ERROR_INT("invalid nbins", __func__, 1);
    pixGetDimensions(pixs, &w, &h, NULL);

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if (type == L_MEAN_ABSVAL) {
        for (i = 0; i < h; i++) {
            sum = 0;
            lines = datas + i * wpls;
            for (j = 0; j < w; j++)
                sum += GET_DATA_BYTE(lines, j);
            colvect[i] = (l_float32)sum / (l_float32)w;
        }
        return 0;
    }

        /* We need a histogram; binwidth ~ 256 / nbins */
    histo = (l_int32 *)LEPT_CALLOC(nbins, sizeof(l_int32));
    gray2bin = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    bin2gray = (l_int32 *)LEPT_CALLOC(nbins, sizeof(l_int32));
    for (i = 0; i < 256; i++)  /* gray value --> histo bin */
        gray2bin[i] = (i * nbins) / 256;
    for (i = 0; i < nbins; i++)  /* histo bin --> gray value */
        bin2gray[i] = (i * 256 + 128) / nbins;

    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        for (k = 0; k < nbins; k++)
            histo[k] = 0;
        for (j = 0; j < w; j++) {
            val = GET_DATA_BYTE(lines, j);
            histo[gray2bin[val]]++;
        }

        if (type == L_MEDIAN_VAL) {
            sum = 0;
            target = (w + 1) / 2;
            for (k = 0; k < nbins; k++) {
                sum += histo[k];
                if (sum >= target) {
                    colvect[i] = bin2gray[k];
                    break;
                }
            }
        } else if (type == L_MODE_VAL) {
            max = 0;
            modeval = 0;
            for (k = 0; k < nbins; k++) {
                if (histo[k] > max) {
                    max = histo[k];
                    modeval = k;
                }
            }
            if (max < thresh)
                colvect[i] = 0;
            else
                colvect[i] = bin2gray[modeval];
        } else {  /* type == L_MODE_COUNT */
            max = 0;
            for (k = 0; k < nbins; k++) {
                if (histo[k] > max)
                    max = histo[k];
            }
            colvect[i] = max;
        }
    }

    LEPT_FREE(histo);
    LEPT_FREE(gray2bin);
    LEPT_FREE(bin2gray);
    return 0;
}


/*!
 * \brief   pixGetColumnStats()
 *
 * \param[in]   pixs     8 bpp; not cmapped
 * \param[in]   type     L_MEAN_ABSVAL, L_MEDIAN_VAL, L_MODE_VAL, L_MODE_COUNT
 * \param[in]   nbins    of histogram for median and mode; ignored for mean
 * \param[in]   thresh   on histogram for mode val; ignored for all other types
 * \param[in]   rowvect  vector of results gathered down the columns of pixs
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This computes a row vector of statistics using each
 *          column of a Pix.  The result is put in %rowvect.
 *      (2) The %thresh parameter works with L_MODE_VAL only, and
 *          sets a minimum occupancy of the mode bin.
 *          If the occupancy of the mode bin is less than %thresh, the
 *          mode value is returned as 0.  To always return the actual
 *          mode value, set %thresh = 0.
 *      (3) What is the meaning of this %thresh parameter?
 *          For each column, the total count in the histogram is h, the
 *          image height.  So %thresh, relative to h, gives a measure
 *          of the ratio of the bin width to the width of the distribution.
 *          The larger %thresh, the narrower the distribution must be
 *          for the mode value to be returned (instead of returning 0).
 * </pre>
 */
l_ok
pixGetColumnStats(PIX        *pixs,
                  l_int32     type,
                  l_int32     nbins,
                  l_int32     thresh,
                  l_float32  *rowvect)
{
l_int32    i, j, k, w, h, val, wpls, sum, target, max, modeval;
l_int32   *histo, *gray2bin, *bin2gray;
l_uint32  *datas;

    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8 bpp", __func__, 1);
    if (!rowvect)
        return ERROR_INT("rowvect not defined", __func__, 1);
    if (type != L_MEAN_ABSVAL && type != L_MEDIAN_VAL &&
        type != L_MODE_VAL && type != L_MODE_COUNT)
        return ERROR_INT("invalid type", __func__, 1);
    if (type != L_MEAN_ABSVAL && (nbins < 1 || nbins > 256))
        return ERROR_INT("invalid nbins", __func__, 1);
    pixGetDimensions(pixs, &w, &h, NULL);

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if (type == L_MEAN_ABSVAL) {
        for (j = 0; j < w; j++) {
            sum = 0;
            for (i = 0; i < h; i++)
                sum += GET_DATA_BYTE(datas + i * wpls, j);
            rowvect[j] = (l_float32)sum / (l_float32)h;
        }
        return 0;
    }

        /* We need a histogram; binwidth ~ 256 / nbins */
    histo = (l_int32 *)LEPT_CALLOC(nbins, sizeof(l_int32));
    gray2bin = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    bin2gray = (l_int32 *)LEPT_CALLOC(nbins, sizeof(l_int32));
    for (i = 0; i < 256; i++)  /* gray value --> histo bin */
        gray2bin[i] = (i * nbins) / 256;
    for (i = 0; i < nbins; i++)  /* histo bin --> gray value */
        bin2gray[i] = (i * 256 + 128) / nbins;

    for (j = 0; j < w; j++) {
        for (i = 0; i < h; i++) {
            val = GET_DATA_BYTE(datas + i * wpls, j);
            histo[gray2bin[val]]++;
        }

        if (type == L_MEDIAN_VAL) {
            sum = 0;
            target = (h + 1) / 2;
            for (k = 0; k < nbins; k++) {
                sum += histo[k];
                if (sum >= target) {
                    rowvect[j] = bin2gray[k];
                    break;
                }
            }
        } else if (type == L_MODE_VAL) {
            max = 0;
            modeval = 0;
            for (k = 0; k < nbins; k++) {
                if (histo[k] > max) {
                    max = histo[k];
                    modeval = k;
                }
            }
            if (max < thresh)
                rowvect[j] = 0;
            else
                rowvect[j] = bin2gray[modeval];
        } else {  /* type == L_MODE_COUNT */
            max = 0;
            for (k = 0; k < nbins; k++) {
                if (histo[k] > max)
                    max = histo[k];
            }
            rowvect[j] = max;
        }
        for (k = 0; k < nbins; k++)
            histo[k] = 0;
    }

    LEPT_FREE(histo);
    LEPT_FREE(gray2bin);
    LEPT_FREE(bin2gray);
    return 0;
}


/*!
 * \brief   pixSetPixelColumn()
 *
 * \param[in]   pix      8 bpp; not cmapped
 * \param[in]   col      column index
 * \param[in]   colvect  vector of floats
 * \return  0 if OK, 1 on error
 */
l_ok
pixSetPixelColumn(PIX        *pix,
                  l_int32     col,
                  l_float32  *colvect)
{
l_int32    i, w, h, wpl;
l_uint32  *data;

    if (!pix || pixGetDepth(pix) != 8)
        return ERROR_INT("pix not defined or not 8 bpp", __func__, 1);
    if (!colvect)
        return ERROR_INT("colvect not defined", __func__, 1);
    pixGetDimensions(pix, &w, &h, NULL);
    if (col < 0 || col > w)
        return ERROR_INT("invalid col", __func__, 1);

    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    for (i = 0; i < h; i++)
        SET_DATA_BYTE(data + i * wpl, col, (l_int32)colvect[i]);

    return 0;
}


/*-------------------------------------------------------------*
 *              Foreground/background estimation               *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixThresholdForFgBg()
 *
 * \param[in]    pixs    any depth; cmapped ok
 * \param[in]    factor  subsampling factor; integer >= 1
 * \param[in]    thresh  threshold for generating foreground mask
 * \param[out]   pfgval  [optional] average foreground value
 * \param[out]   pbgval  [optional] average background value
 * \return  0 if OK, 1 on error
 */
l_ok
pixThresholdForFgBg(PIX      *pixs,
                    l_int32   factor,
                    l_int32   thresh,
                    l_int32  *pfgval,
                    l_int32  *pbgval)
{
l_float32  fval;
PIX       *pixg, *pixm;

    if (pfgval) *pfgval = 0;
    if (pbgval) *pbgval = 0;
    if (!pfgval && !pbgval)
        return ERROR_INT("no data requested", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

        /* Generate a subsampled 8 bpp version and a mask over the fg */
    pixg = pixConvertTo8BySampling(pixs, factor, 0);
    pixm = pixThresholdToBinary(pixg, thresh);

    if (pfgval) {
        pixGetAverageMasked(pixg, pixm, 0, 0, 1, L_MEAN_ABSVAL, &fval);
        *pfgval = (l_int32)(fval + 0.5);
    }

    if (pbgval) {
        pixInvert(pixm, pixm);
        pixGetAverageMasked(pixg, pixm, 0, 0, 1, L_MEAN_ABSVAL, &fval);
        *pbgval = (l_int32)(fval + 0.5);
    }

    pixDestroy(&pixg);
    pixDestroy(&pixm);
    return 0;
}


/*!
 * \brief   pixSplitDistributionFgBg()
 *
 * \param[in]    pixs        any depth; cmapped ok
 * \param[in]    scorefract  fraction of the max score, used to determine
 *                           the range over which the histogram min is searched
 * \param[in]    factor      subsampling factor; integer >= 1
 * \param[out]   pthresh     [optional] best threshold for separating
 * \param[out]   pfgval      [optional] average foreground value
 * \param[out]   pbgval      [optional] average background value
 * \param[out]   ppixdb      [optional] plot of distribution and split point
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See numaSplitDistribution() for details on the underlying
 *          method of choosing a threshold.
 * </pre>
 */
l_ok
pixSplitDistributionFgBg(PIX       *pixs,
                         l_float32  scorefract,
                         l_int32    factor,
                         l_int32   *pthresh,
                         l_int32   *pfgval,
                         l_int32   *pbgval,
                         PIX      **ppixdb)
{
char       buf[256];
l_int32    thresh;
l_float32  avefg, avebg, maxnum;
GPLOT     *gplot;
NUMA      *na, *nascore, *nax, *nay;
PIX       *pixg;

    if (pthresh) *pthresh = 0;
    if (pfgval) *pfgval = 0;
    if (pbgval) *pbgval = 0;
    if (ppixdb) *ppixdb = NULL;
    if (!pthresh && !pfgval && !pbgval)
        return ERROR_INT("no data requested", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

        /* Generate a subsampled 8 bpp version */
    pixg = pixConvertTo8BySampling(pixs, factor, 0);

        /* Make the fg/bg estimates */
    na = pixGetGrayHistogram(pixg, 1);
    if (ppixdb) {
        numaSplitDistribution(na, scorefract, &thresh, &avefg, &avebg,
                              NULL, NULL, &nascore);
        numaDestroy(&nascore);
    } else {
        numaSplitDistribution(na, scorefract, &thresh, &avefg, &avebg,
                              NULL, NULL, NULL);
    }

    if (pthresh) *pthresh = thresh;
    if (pfgval) *pfgval = (l_int32)(avefg + 0.5);
    if (pbgval) *pbgval = (l_int32)(avebg + 0.5);

    if (ppixdb) {
        lept_mkdir("lept/redout");
        gplot = gplotCreate("/tmp/lept/redout/histplot", GPLOT_PNG, "Histogram",
                            "Grayscale value", "Number of pixels");
        gplotAddPlot(gplot, NULL, na, GPLOT_LINES, NULL);
        nax = numaMakeConstant(thresh, 2);
        numaGetMax(na, &maxnum, NULL);
        nay = numaMakeConstant(0, 2);
        numaReplaceNumber(nay, 1, (l_int32)(0.5 * maxnum));
        snprintf(buf, sizeof(buf), "score fract = %3.1f", scorefract);
        gplotAddPlot(gplot, nax, nay, GPLOT_LINES, buf);
        *ppixdb = gplotMakeOutputPix(gplot);
        gplotDestroy(&gplot);
        numaDestroy(&nax);
        numaDestroy(&nay);
    }

    pixDestroy(&pixg);
    numaDestroy(&na);
    return 0;
}

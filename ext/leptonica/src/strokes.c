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
 * \file strokes.c
 * <pre>
 *
 *      Operations on 1 bpp images to:
 *      (1) measure stroke parameters, such as length and average width
 *      (2) change the average stroke width to a given value by eroding
 *          or dilating the image.
 *
 *      These operations are intended to operate on a single text
 *      character, to regularize the stroke width. It is expected
 *      that character matching by correlation, as used in the recog
 *      application, can often be improved by pre-processing both
 *      template and character images to a fixed stroke width.
 *
 *      Stroke parameter measurement
 *            l_int32      pixFindStrokeLength()
 *            l_int32      pixFindStrokeWidth()
 *            NUMA        *pixaFindStrokeWidth()
 *
 *      Stroke width regulation
 *            PIXA        *pixaModifyStrokeWidth()
 *            PIX         *pixModifyStrokeWidth()
 *            PIXA        *pixaSetStrokeWidth()
 *            PIX         *pixSetStrokeWidth()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

/*-----------------------------------------------------------------*
 *                   Stroke parameter measurement                  *
 *-----------------------------------------------------------------*/
/*!
 * \brief   pixFindStrokeLength()
 *
 * \param[in]    pixs      1 bpp
 * \param[in]    tab8      [optional] table for counting fg pixels; can be NULL
 * \param[out]   plength   estimated length of the strokes
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Returns half the number of fg boundary pixels.
 * </pre>
 */
l_ok
pixFindStrokeLength(PIX      *pixs,
                    l_int32  *tab8,
                    l_int32  *plength)
{
l_int32   n;
l_int32  *tab;
PIX      *pix1;

    if (!plength)
        return ERROR_INT("&length not defined", __func__, 1);
    *plength = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

    pix1 = pixExtractBoundary(pixs, 1);
    tab = (tab8) ? tab8 : makePixelSumTab8();
    pixCountPixels(pix1, &n, tab);
    *plength = n / 2;
    if (!tab8) LEPT_FREE(tab);
    pixDestroy(&pix1);
    return 0;
}


/*!
 * \brief   pixFindStrokeWidth()
 *
 * \param[in]    pixs       1 bpp
 * \param[in]    thresh     fractional count threshold relative to distance 1
 * \param[in]    tab8       [optional] table for counting fg pixels; can be NULL
 * \param[out]   pwidth     estimated width of the strokes
 * \param[out]   pnahisto   [optional] histo of pixel distances from bg
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This uses two methods to estimate the stroke width:
 *          (a) half the fg boundary length
 *          (b) a value derived from the histogram of the fg distance transform
 *      (2) Distance is measured in 8-connected
 *      (3) %thresh is the minimum fraction N(dist=d)/N(dist=1) of pixels
 *          required to determine if the pixels at distance d are above
 *          the noise. It is typically about 0.15.
 * </pre>
 */
l_ok
pixFindStrokeWidth(PIX        *pixs,
                   l_float32   thresh,
                   l_int32    *tab8,
                   l_float32  *pwidth,
                   NUMA      **pnahisto)
{
l_int32     i, n, count, length, first, last;
l_int32    *tab;
l_float32   width1, width2, ratio, extra;
l_float32  *fa;
NUMA       *na1, *na2;
PIX        *pix1;

    if (!pwidth)
        return ERROR_INT("&width not defined", __func__, 1);
    *pwidth = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

    tab = (tab8) ? tab8 : makePixelSumTab8();

    /* ------- Method 1: via boundary length ------- */
        /* The computed stroke length is a bit larger than that actual
         * length, because of the addition of the 'caps' at the
         * stroke ends.  Therefore the computed width is a bit
         * smaller than the average width. */
    pixFindStrokeLength(pixs, tab8, &length);
    pixCountPixels(pixs, &count, tab8);
    width1 = (l_float32)count / (l_float32)length;

    /* ------- Method 2: via distance transform ------- */
        /* First get the histogram of distances */
    pix1 = pixDistanceFunction(pixs, 8, 8, L_BOUNDARY_BG);
    na1 = pixGetGrayHistogram(pix1, 1);
    pixDestroy(&pix1);
    numaGetNonzeroRange(na1, 0.1f, &first, &last);
    na2 = numaClipToInterval(na1, 0, last);
    numaWriteStderr(na2);

        /* Find the bucket with the largest distance whose contents
         * exceed the threshold. */
    fa = numaGetFArray(na2, L_NOCOPY);
    n = numaGetCount(na2);
    for (i = n - 1; i > 0; i--) {
        ratio = fa[i] / fa[1];
        if (ratio > thresh) break;
    }
        /* Let the last skipped bucket contribute to the stop bucket.
         * This is the 'extra' term below.  The result may be a slight
         * over-correction, so the computed width may be a bit larger
         * than the average width. */
    extra = (i < n - 1) ? fa[i + 1] / fa[1] : 0;
    width2 = 2.0f * (i - 1.0f + ratio + extra);
    lept_stderr("width1 = %5.2f, width2 = %5.2f\n", width1, width2);

        /* Average the two results */
    *pwidth = (width1 + width2) / 2.0f;

    if (!tab8) LEPT_FREE(tab);
    numaDestroy(&na1);
    if (pnahisto)
        *pnahisto = na2;
    else
        numaDestroy(&na2);
    return 0;
}


/*!
 * \brief   pixaFindStrokeWidth()
 *
 * \param[in]    pixa     of 1 bpp images
 * \param[in]    thresh   fractional count threshold relative to distance 1
 * \param[in]    tab8     [optional] table for counting fg pixels; can be NULL
 * \param[in]    debug    1 for debug output; 0 to skip
 * \return  na  array of stroke widths for each pix in %pixa; NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See pixFindStrokeWidth() for details.
 * </pre>
 */
NUMA *
pixaFindStrokeWidth(PIXA     *pixa,
                   l_float32  thresh,
                   l_int32   *tab8,
                   l_int32    debug)
{
l_int32    i, n, same, maxd;
l_int32   *tab;
l_float32  width;
NUMA      *na;
PIX       *pix;

    if (!pixa)
        return (NUMA *)ERROR_PTR("pixa not defined", __func__, NULL);
    pixaVerifyDepth(pixa, &same, &maxd);
    if (maxd > 1)
        return (NUMA *)ERROR_PTR("pix not all 1 bpp", __func__, NULL);

    tab = (tab8) ? tab8 : makePixelSumTab8();

    n = pixaGetCount(pixa);
    na = numaCreate(n);
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixa, i, L_CLONE);
        pixFindStrokeWidth(pix, thresh, tab8, &width, NULL);
        numaAddNumber(na, width);
        pixDestroy(&pix);
    }

    if (!tab8) LEPT_FREE(tab);
    return na;
}


/*-----------------------------------------------------------------*
 *                       Change stroke width                       *
 *-----------------------------------------------------------------*/
/*!
 * \brief   pixaModifyStrokeWidth()
 *
 * \param[in]     pixas      of 1 bpp pix
 * \param[out]    targetw    desired width for strokes in each pix
 * \return  pixa  with modified stroke widths, or NULL on error
 */
PIXA *
pixaModifyStrokeWidth(PIXA      *pixas,
                      l_float32  targetw)
{
l_int32    i, n, same, maxd;
l_float32  width;
NUMA      *na;
PIX       *pix1, *pix2;
PIXA      *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (targetw < 1)
        return (PIXA *)ERROR_PTR("target width < 1", __func__, NULL);
    pixaVerifyDepth(pixas, &same, &maxd);
    if (maxd > 1)
        return (PIXA *)ERROR_PTR("pix not all 1 bpp", __func__, NULL);

    na = pixaFindStrokeWidth(pixas, 0.1f, NULL, 0);
    n = pixaGetCount(pixas);
    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        numaGetFValue(na, i, &width);
        pix2 = pixModifyStrokeWidth(pix1, width, targetw);
        pixaAddPix(pixad, pix2, L_INSERT);
        pixDestroy(&pix1);
    }

    numaDestroy(&na);
    return pixad;
}


/*!
 * \brief   pixModifyStrokeWidth()
 *
 * \param[in]   pixs      of 1 bpp pix
 * \param[in]   width     measured average stroke width
 * \param[in]   targetw   desired stroke width
 * \return  pix  with modified stroke width, or NULL on error
 */
PIX *
pixModifyStrokeWidth(PIX       *pixs,
                     l_float32  width,
                     l_float32  targetw)
{
char     buf[32];
l_int32  diff, size;

    if (!pixs || (pixGetDepth(pixs) != 1))
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (targetw < 1)
        return (PIX *)ERROR_PTR("target width < 1", __func__, NULL);

    diff = lept_roundftoi(targetw - width);
    if (diff == 0) return pixCopy(NULL, pixs);

    size = L_ABS(diff) + 1;
    if (diff < 0)  /* erode */
        snprintf(buf, sizeof(buf), "e%d.%d", size, size);
    else  /* diff > 0; dilate */
        snprintf(buf, sizeof(buf), "d%d.%d", size, size);
    return pixMorphSequence(pixs, buf, 0);
}


/*!
 * \brief   pixaSetStrokeWidth()
 *
 * \param[in]   pixas          of 1 bpp pix
 * \param[in]   width          set stroke width to this value, in [1 ... 100].
 * \param[in]   thinfirst      1 to thin all pix to a skeleton first; 0 to skip
 * \param[in]   connectivity   4 or 8, to be used if %thinfirst == 1
 * \return  pixa  with all stroke widths being %width, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If %thinfirst == 1, thin to a skeleton using the specified
 *          %connectivity.  Use %thinfirst == 0 if all pix in pixas
 *          have already been thinned as far as possible.
 *      (2) The image is dilated to the required %width.  This dilation
 *          is not connectivity preserving, so this is typically
 *          used in a situation where merging of c.c. in the individual
 *          pix is not a problem; e.g., where each pix is a single c.c.
 * </pre>
 */
PIXA *
pixaSetStrokeWidth(PIXA    *pixas,
                   l_int32  width,
                   l_int32  thinfirst,
                   l_int32  connectivity)
{
l_int32  i, n, maxd, same;
PIX     *pix1, *pix2;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (width < 1 || width > 100)
        return (PIXA *)ERROR_PTR("width not in [1 ... 100]", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIXA *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);
    pixaVerifyDepth(pixas, &same, &maxd);
    if (maxd > 1)
        return (PIXA *)ERROR_PTR("pix are not all 1 bpp", __func__, NULL);

    n = pixaGetCount(pixas);
    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        pix2 = pixSetStrokeWidth(pix1, width, thinfirst, connectivity);
        pixaAddPix(pixad, pix2, L_INSERT);
        pixDestroy(&pix1);
    }

    return pixad;
}


/*!
 * \brief   pixSetStrokeWidth()
 *
 * \param[in]   pixs           1 bpp
 * \param[in]   width          set stroke width to this value, in [1 ... 100].
 * \param[in]   thinfirst      1 to thin all pix to a skeleton first; 0 to skip
 * \param[in]   connectivity   4 or 8, to be used if %thinfirst == 1
 * \return  pixd  with stroke width set to %width, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See notes in pixaSetStrokeWidth().
 *      (2) A white border of sufficient width to avoid boundary
 *          artifacts in the thickening step is added before thinning.
 *      (3) %connectivity == 8 usually gives a slightly smoother result.
 * </pre>
 */
PIX *
pixSetStrokeWidth(PIX     *pixs,
                  l_int32  width,
                  l_int32  thinfirst,
                  l_int32  connectivity)
{
char     buf[16];
l_int32  border;
PIX     *pix1, *pix2, *pixd;

    if (!pixs || (pixGetDepth(pixs) != 1))
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (width < 1 || width > 100)
        return (PIX *)ERROR_PTR("width not in [1 ... 100]", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);

    if (!thinfirst && width == 1)  /* nothing to do */
        return pixCopy(NULL, pixs);

        /* Add a white border */
    border = width / 2;
    pix1 = pixAddBorder(pixs, border, 0);

        /* Thin to a skeleton */
    if (thinfirst)
        pix2 = pixThinConnected(pix1, L_THIN_FG, connectivity, 0);
    else
        pix2 = pixClone(pix1);
    pixDestroy(&pix1);

        /* Dilate */
    snprintf(buf, sizeof(buf), "D%d.%d", width, width);
    pixd = pixMorphSequence(pix2, buf, 0);
    pixCopyText(pixd, pixs);
    pixDestroy(&pix2);
    return pixd;
}

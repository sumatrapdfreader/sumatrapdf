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
 * \file binarize.c
 * <pre>
 *
 *  ===================================================================
 *  Image binarization algorithms are found in:
 *    grayquant.c:   standard, simple, general grayscale quantization
 *    adaptmap.c:    local adaptive; mostly gray-to-gray in preparation
 *                   for binarization
 *    binarize.c:    special binarization methods, locally adaptive and
 *                   global.
 *  ===================================================================
 *
 *      Adaptive Otsu-based thresholding
 *          l_int32       pixOtsuAdaptiveThreshold()       8 bpp
 *
 *      Otsu thresholding on adaptive background normalization
 *          PIX          *pixOtsuThreshOnBackgroundNorm()  8 bpp
 *
 *      Masking and Otsu estimate on adaptive background normalization
 *          PIX          *pixMaskedThreshOnBackgroundNorm()  8 bpp
 *
 *      Sauvola local thresholding
 *          l_int32       pixSauvolaBinarizeTiled()
 *          l_int32       pixSauvolaBinarize()
 *          static PIX   *pixSauvolaGetThreshold()
 *          static PIX   *pixApplyLocalThreshold();
 *
 *      Sauvola binarization on contrast normalization
 *          PIX          *pixSauvolaOnContrastNorm()  8 bpp
 *
 *      Contrast normalization followed by bg normalization and thresholding
 *          PIX          *pixThreshOnDoubleNorm()
 *
 *      Global thresholding using connected components
 *          PIX          *pixThresholdByConnComp()
 *
 *      Global thresholding by histogram
 *          PIX          *pixThresholdByHisto()
 *
 *  Notes:
 *      (1) pixOtsuAdaptiveThreshold() computes a global threshold over each
 *          tile and performs the threshold operation, resulting in a
 *          binary image for each tile.  These are stitched into the
 *          final result.
 *      (2) pixOtsuThreshOnBackgroundNorm() and
 *          pixMaskedThreshOnBackgroundNorm() are binarization functions
 *          that use background normalization with other techniques.
 *      (3) Sauvola binarization computes a local threshold based on
 *          the local average and square average.  It takes two constants:
 *          the window size for the measurement at each pixel and a
 *          parameter that determines the amount of normalized local
 *          standard deviation to subtract from the local average value.
 *      (4) pixThresholdByConnComp() uses the numbers of 4 and 8 connected
 *          components at different thresholding to determine if a
 *          global threshold can be used (for text or line-art) and the
 *          value it should have.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"

static PIX *pixSauvolaGetThreshold(PIX *pixm, PIX *pixms, l_float32 factor,
                                   PIX **ppixsd);
static PIX *pixApplyLocalThreshold(PIX *pixs, PIX *pixth);

/*------------------------------------------------------------------*
 *                 Adaptive Otsu-based thresholding                 *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixOtsuAdaptiveThreshold()
 *
 * \param[in]    pixs              8 bpp
 * \param[in]    sx, sy            desired tile dimensions; actual size may vary
 * \param[in]    smoothx, smoothy  half-width of convolution kernel applied to
 *                                 threshold array: use 0 for no smoothing
 * \param[in]    scorefract        fraction of the max Otsu score; typ. 0.1;
 *                                 use 0.0 for standard Otsu
 * \param[out]   ppixth            [optional] array of threshold values
 *                                 found for each tile
 * \param[out]   ppixd             [optional] thresholded input pixs,
 *                                 based on the threshold array
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The Otsu method finds a single global threshold for an image.
 *          This function allows a locally adapted threshold to be
 *          found for each tile into which the image is broken up.
 *      (2) The array of threshold values, one for each tile, constitutes
 *          a highly downscaled image.  This array is optionally
 *          smoothed using a convolution.  The full width and height of the
 *          convolution kernel are (2 * %smoothx + 1) and (2 * %smoothy + 1).
 *      (3) The minimum tile dimension allowed is 16.  If such small
 *          tiles are used, it is recommended to use smoothing, because
 *          without smoothing, each small tile determines the splitting
 *          threshold independently.  A tile that is entirely in the
 *          image bg will then hallucinate fg, resulting in a very noisy
 *          binarization.  The smoothing should be large enough that no
 *          tile is only influenced by one type (fg or bg) of pixels,
 *          because it will force a split of its pixels.
 *      (4) To get a single global threshold for the entire image, use
 *          input values of %sx and %sy that are larger than the image.
 *          For this situation, the smoothing parameters are ignored.
 *      (5) The threshold values partition the image pixels into two classes:
 *          one whose values are less than the threshold and another
 *          whose values are greater than or equal to the threshold.
 *          This is the same use of 'threshold' as in pixThresholdToBinary().
 *      (6) The scorefract is the fraction of the maximum Otsu score, which
 *          is used to determine the range over which the histogram minimum
 *          is searched.  See numaSplitDistribution() for details on the
 *          underlying method of choosing a threshold.
 *      (7) This uses enables a modified version of the Otsu criterion for
 *          splitting the distribution of pixels in each tile into a
 *          fg and bg part.  The modification consists of searching for
 *          a minimum in the histogram over a range of pixel values where
 *          the Otsu score is within a defined fraction, %scorefract,
 *          of the max score.  To get the original Otsu algorithm, set
 *          %scorefract == 0.
 *      (8) N.B. This method is NOT recommended for images with weak text
 *          and significant background noise, such as bleedthrough, because
 *          of the problem noted in (3) above for tiling.  Use Sauvola.
 * </pre>
 */
l_ok
pixOtsuAdaptiveThreshold(PIX       *pixs,
                         l_int32    sx,
                         l_int32    sy,
                         l_int32    smoothx,
                         l_int32    smoothy,
                         l_float32  scorefract,
                         PIX      **ppixth,
                         PIX      **ppixd)
{
l_int32     w, h, nx, ny, i, j, thresh;
l_uint32    val;
PIX        *pixt, *pixb, *pixthresh, *pixth, *pixd;
PIXTILING  *pt;

    if (!ppixth && !ppixd)
        return ERROR_INT("neither &pixth nor &pixd defined", __func__, 1);
    if (ppixth) *ppixth = NULL;
    if (ppixd) *ppixd = NULL;
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8 bpp", __func__, 1);
    if (sx < 16 || sy < 16)
        return ERROR_INT("sx and sy must be >= 16", __func__, 1);

        /* Compute the threshold array for the tiles */
    pixGetDimensions(pixs, &w, &h, NULL);
    nx = L_MAX(1, w / sx);
    ny = L_MAX(1, h / sy);
    smoothx = L_MIN(smoothx, (nx - 1) / 2);
    smoothy = L_MIN(smoothy, (ny - 1) / 2);
    pt = pixTilingCreate(pixs, nx, ny, 0, 0, 0, 0);
    pixthresh = pixCreate(nx, ny, 8);
    for (i = 0; i < ny; i++) {
        for (j = 0; j < nx; j++) {
            pixt = pixTilingGetTile(pt, i, j);
            pixSplitDistributionFgBg(pixt, scorefract, 1, &thresh,
                                     NULL, NULL, NULL);
            pixSetPixel(pixthresh, j, i, thresh);  /* see note (4) */
            pixDestroy(&pixt);
        }
    }

        /* Optionally smooth the threshold array */
    if (smoothx > 0 || smoothy > 0)
        pixth = pixBlockconv(pixthresh, smoothx, smoothy);
    else
        pixth = pixClone(pixthresh);
    pixDestroy(&pixthresh);

        /* Optionally apply the threshold array to binarize pixs */
    if (ppixd) {
        pixd = pixCreate(w, h, 1);
        pixCopyResolution(pixd, pixs);
        for (i = 0; i < ny; i++) {
            for (j = 0; j < nx; j++) {
                pixt = pixTilingGetTile(pt, i, j);
                pixGetPixel(pixth, j, i, &val);
                pixb = pixThresholdToBinary(pixt, val);
                pixTilingPaintTile(pixd, i, j, pixb, pt);
                pixDestroy(&pixt);
                pixDestroy(&pixb);
            }
        }
        *ppixd = pixd;
    }

    if (ppixth)
        *ppixth = pixth;
    else
        pixDestroy(&pixth);

    pixTilingDestroy(&pt);
    return 0;
}


/*------------------------------------------------------------------*
 *      Otsu thresholding on adaptive background normalization      *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixOtsuThreshOnBackgroundNorm()
 *
 * \param[in]    pixs         8 bpp grayscale; not colormapped
 * \param[in]    pixim        [optional] 1 bpp 'image' mask; can be null
 * \param[in]    sx, sy       tile size in pixels
 * \param[in]    thresh       threshold for determining foreground
 * \param[in]    mincount     min threshold on counts in a tile
 * \param[in]    bgval        target bg val; typ. > 128
 * \param[in]    smoothx      half-width of block convolution kernel width
 * \param[in]    smoothy      half-width of block convolution kernel height
 * \param[in]    scorefract   fraction of the max Otsu score; typ. 0.1
 * \param[out]   pthresh      [optional] threshold value that was
 *                            used on the normalized image
 * \return  pixd 1 bpp thresholded image, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does background normalization followed by Otsu
 *          thresholding.  Otsu binarization attempts to split the
 *          image into two roughly equal sets of pixels, and it does
 *          a very poor job when there are large amounts of dark
 *          background.  By doing a background normalization first,
 *          to get the background near 255, we remove this problem.
 *          Then we use a modified Otsu to estimate the best global
 *          threshold on the normalized image.
 *      (2) See pixBackgroundNorm() for meaning and typical values
 *          of input parameters.  For a start, you can try:
 *            sx, sy = 10, 15
 *            thresh = 100
 *            mincount = 50
 *            bgval = 255
 *            smoothx, smoothy = 2
 * </pre>
 */
PIX *
pixOtsuThreshOnBackgroundNorm(PIX       *pixs,
                              PIX       *pixim,
                              l_int32    sx,
                              l_int32    sy,
                              l_int32    thresh,
                              l_int32    mincount,
                              l_int32    bgval,
                              l_int32    smoothx,
                              l_int32    smoothy,
                              l_float32  scorefract,
                              l_int32   *pthresh)
{
l_int32   w, h;
l_uint32  val;
PIX      *pixn, *pixt, *pixd;

    if (pthresh) *pthresh = 0;
    if (!pixs || pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs undefined or not 8 bpp", __func__, NULL);
    if (pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs is colormapped", __func__, NULL);
    if (sx < 4 || sy < 4)
        return (PIX *)ERROR_PTR("sx and sy must be >= 4", __func__, NULL);
    if (mincount > sx * sy) {
        L_WARNING("mincount too large for tile size\n", __func__);
        mincount = (sx * sy) / 3;
    }

    pixn = pixBackgroundNorm(pixs, pixim, NULL, sx, sy, thresh,
                             mincount, bgval, smoothx, smoothy);
    if (!pixn)
        return (PIX *)ERROR_PTR("pixn not made", __func__, NULL);

        /* Just use 1 tile for a global threshold, which is stored
         * as a single pixel in pixt. */
    pixGetDimensions(pixn, &w, &h, NULL);
    pixOtsuAdaptiveThreshold(pixn, w, h, 0, 0, scorefract, &pixt, &pixd);
    pixDestroy(&pixn);

    if (pixt && pthresh) {
        pixGetPixel(pixt, 0, 0, &val);
        *pthresh = val;
    }
    pixDestroy(&pixt);

    if (!pixd)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    else
        return pixd;
}



/*----------------------------------------------------------------------*
 *    Masking and Otsu estimate on adaptive background normalization    *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixMaskedThreshOnBackgroundNorm()
 *
 * \param[in]    pixs         8 bpp grayscale; not colormapped
 * \param[in]    pixim        [optional] 1 bpp 'image' mask; can be null
 * \param[in]    sx, sy       tile size in pixels
 * \param[in]    thresh       threshold for determining foreground
 * \param[in]    mincount     min threshold on counts in a tile
 * \param[in]    smoothx      half-width of block convolution kernel width
 * \param[in]    smoothy      half-width of block convolution kernel height
 * \param[in]    scorefract   fraction of the max Otsu score; typ. ~ 0.1
 * \param[out]   pthresh      [optional] threshold value that was
 *                            used on the normalized image
 * \return  pixd 1 bpp thresholded image, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This begins with a standard background normalization.
 *          Additionally, there is a flexible background norm, that
 *          will adapt to a rapidly varying background, and this
 *          puts white pixels in the background near regions with
 *          significant foreground.  The white pixels are turned into
 *          a 1 bpp selection mask by binarization followed by dilation.
 *          Otsu thresholding is performed on the input image to get an
 *          estimate of the threshold in the non-mask regions.
 *          The background normalized image is thresholded with two
 *          different values, and the result is combined using
 *          the selection mask.
 *      (2) Note that the numbers 255 (for bgval target) and 190 (for
 *          thresholding on pixn) are tied together, and explicitly
 *          defined in this function.
 *      (3) See pixBackgroundNorm() for meaning and typical values
 *          of input parameters.  For a start, you can try:
 *            sx, sy = 10, 15
 *            thresh = 100
 *            mincount = 50
 *            smoothx, smoothy = 2
 * </pre>
 */
PIX *
pixMaskedThreshOnBackgroundNorm(PIX       *pixs,
                                PIX       *pixim,
                                l_int32    sx,
                                l_int32    sy,
                                l_int32    thresh,
                                l_int32    mincount,
                                l_int32    smoothx,
                                l_int32    smoothy,
                                l_float32  scorefract,
                                l_int32   *pthresh)
{
l_int32   w, h, highthresh;
l_uint32  val;
PIX      *pixn, *pixm, *pixd, *pix1, *pix2, *pix3, *pix4;

    if (pthresh) *pthresh = 0;
    if (!pixs || pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs undefined or not 8 bpp", __func__, NULL);
    if (pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs is colormapped", __func__, NULL);
    if (sx < 4 || sy < 4)
        return (PIX *)ERROR_PTR("sx and sy must be >= 4", __func__, NULL);
    if (mincount > sx * sy) {
        L_WARNING("mincount too large for tile size\n", __func__);
        mincount = (sx * sy) / 3;
    }

        /* Standard background normalization */
    pixn = pixBackgroundNorm(pixs, pixim, NULL, sx, sy, thresh,
                             mincount, 255, smoothx, smoothy);
    if (!pixn)
        return (PIX *)ERROR_PTR("pixn not made", __func__, NULL);

        /* Special background normalization for adaptation to quickly
         * varying background.  Threshold on the very light parts,
         * which tend to be near significant edges, and dilate to
         * form a mask over regions that are typically text.  The
         * dilation size is chosen to cover the text completely,
         * except for very thick fonts. */
    pix1 = pixBackgroundNormFlex(pixs, 7, 7, 1, 1, 20);
    pix2 = pixThresholdToBinary(pix1, 240);
    pixInvert(pix2, pix2);
    pixm = pixMorphSequence(pix2, "d21.21", 0);
    pixDestroy(&pix1);
    pixDestroy(&pix2);

        /* Use Otsu to get a global threshold estimate for the image,
         * which is stored as a single pixel in pix3. */
    pixGetDimensions(pixs, &w, &h, NULL);
    pixOtsuAdaptiveThreshold(pixs, w, h, 0, 0, scorefract, &pix3, NULL);
    pixGetPixel(pix3, 0, 0, &val);
    if (pthresh) *pthresh = val;
    pixDestroy(&pix3);

        /* Threshold the background normalized images differentially,
         * using a high value correlated with the background normalization
         * for the part of the image under the mask (i.e., near the
         * darker, thicker foreground), and a value that depends on the Otsu
         * threshold for the rest of the image.  This gives a solid
         * (high) thresholding for the foreground parts of the image,
         * while allowing the background and light foreground to be
         * reasonably well cleaned using a threshold adapted to the
         * input image. */
    highthresh = L_MIN(256, val + 30);
    pixd = pixThresholdToBinary(pixn, highthresh);  /* for bg and light fg */
    pix4 = pixThresholdToBinary(pixn, 190);  /* for heavier fg */
    pixCombineMasked(pixd, pix4, pixm);
    pixDestroy(&pix4);
    pixDestroy(&pixm);
    pixDestroy(&pixn);

    if (!pixd)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    else
        return pixd;
}


/*----------------------------------------------------------------------*
 *                           Sauvola binarization                       *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixSauvolaBinarizeTiled()
 *
 * \param[in]    pixs      8 bpp grayscale, not colormapped
 * \param[in]    whsize    window half-width for measuring local statistics
 * \param[in]    factor    factor for reducing threshold due to variance; >= 0
 * \param[in]    nx, ny    subdivision into tiles; >= 1
 * \param[out]   ppixth    [optional] Sauvola threshold values
 * \param[out]   ppixd     [optional] thresholded image
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The window width and height are 2 * %whsize + 1.  The minimum
 *          value for %whsize is 2; typically it is >= 7.
 *      (2) For nx == ny == 1, this defaults to pixSauvolaBinarize().
 *      (3) Why a tiled version?
 *          (a) A uint32 is used for the mean value accumulator, so
 *              overflow can occur for an image with more than 16M pixels.
 *          (b) A dpix is used to accumulate mean square values, and it
 *              can only accommodate images with less than 2^28 pixels.
 *              Using tiles reduces the size of all the arrays.
 *          (c) Each tile can be processed independently, in parallel,
 *              on a multicore processor.
 *      (4) The Sauvola threshold is determined from the formula:
 *              t = m * (1 - k * (1 - s / 128))
 *          See pixSauvolaBinarize() for details.
 * </pre>
 */
l_ok
pixSauvolaBinarizeTiled(PIX       *pixs,
                        l_int32    whsize,
                        l_float32  factor,
                        l_int32    nx,
                        l_int32    ny,
                        PIX      **ppixth,
                        PIX      **ppixd)
{
l_int32     i, j, w, h, xrat, yrat;
PIX        *pixth = NULL, *pixd = NULL, *tileth = NULL, *tiled = NULL, *pixt;
PIX       **ptileth, **ptiled;
PIXTILING  *pt;

    if (!ppixth && !ppixd)
        return ERROR_INT("no outputs", __func__, 1);
    if (ppixth) *ppixth = NULL;
    if (ppixd) *ppixd = NULL;
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs undefined or not 8 bpp", __func__, 1);
    if (pixGetColormap(pixs))
        return ERROR_INT("pixs is cmapped", __func__, 1);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (whsize < 2)
        return ERROR_INT("whsize must be >= 2", __func__, 1);
    if (w < 2 * whsize + 3 || h < 2 * whsize + 3)
        return ERROR_INT("whsize too large for image", __func__, 1);
    if (factor < 0.0)
        return ERROR_INT("factor must be >= 0", __func__, 1);

    if (nx <= 1 && ny <= 1)
        return pixSauvolaBinarize(pixs, whsize, factor, 1, NULL, NULL,
                                  ppixth, ppixd);

        /* Test to see if the tiles are too small.  The required
         * condition is that the tile dimensions must be at least
         * (whsize + 2) x (whsize + 2).  */
    xrat = w / nx;
    yrat = h / ny;
    if (xrat < whsize + 2) {
        nx = w / (whsize + 2);
        L_WARNING("tile width too small; nx reduced to %d\n", __func__, nx);
    }
    if (yrat < whsize + 2) {
        ny = h / (whsize + 2);
        L_WARNING("tile height too small; ny reduced to %d\n", __func__, ny);
    }
    if (nx <= 1 && ny <= 1)
        return pixSauvolaBinarize(pixs, whsize, factor, 1, NULL, NULL,
                                  ppixth, ppixd);

        /* We can use pixtiling for painting both outputs, if requested */
    if (ppixth) {
        pixth = pixCreate(w, h, 8);
        *ppixth = pixth;
    }
    if (ppixd) {
        pixd = pixCreate(w, h, 1);
        *ppixd = pixd;
    }
    pt = pixTilingCreate(pixs, nx, ny, 0, 0, whsize + 1, whsize + 1);
    pixTilingNoStripOnPaint(pt);  /* pixSauvolaBinarize() does the stripping */

    for (i = 0; i < ny; i++) {
        for (j = 0; j < nx; j++) {
            pixt = pixTilingGetTile(pt, i, j);
            ptileth = (ppixth) ? &tileth : NULL;
            ptiled = (ppixd) ? &tiled : NULL;
            pixSauvolaBinarize(pixt, whsize, factor, 0, NULL, NULL,
                               ptileth, ptiled);
            if (ppixth) {  /* do not strip */
                pixTilingPaintTile(pixth, i, j, tileth, pt);
                pixDestroy(&tileth);
            }
            if (ppixd) {
                pixTilingPaintTile(pixd, i, j, tiled, pt);
                pixDestroy(&tiled);
            }
            pixDestroy(&pixt);
        }
    }

    pixTilingDestroy(&pt);
    return 0;
}


/*!
 * \brief   pixSauvolaBinarize()
 *
 * \param[in]    pixs       8 bpp grayscale; not colormapped
 * \param[in]    whsize     window half-width for measuring local statistics
 * \param[in]    factor     factor for reducing threshold due to variance; >= 0
 * \param[in]    addborder  1 to add border of width (%whsize + 1) on all sides
 * \param[out]   ppixm      [optional] local mean values
 * \param[out]   ppixsd     [optional] local standard deviation values
 * \param[out]   ppixth     [optional] threshold values
 * \param[out]   ppixd      [optional] thresholded image
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The window width and height are 2 * %whsize + 1.  The minimum
 *          value for %whsize is 2; typically it is >= 7..
 *      (2) The local statistics, measured over the window, are the
 *          average and standard deviation.
 *      (3) The measurements of the mean and standard deviation are
 *          performed inside a border of (%whsize + 1) pixels.  If pixs does
 *          not have these added border pixels, use %addborder = 1 to add
 *          it here; otherwise use %addborder = 0.
 *      (4) The Sauvola threshold is determined from the formula:
 *            t = m * (1 - k * (1 - s / 128))
 *          where:
 *            t = local threshold
 *            m = local mean
 *            k = %factor (>= 0)   [ typ. 0.35 ]
 *            s = local standard deviation, which is maximized at
 *                127.5 when half the samples are 0 and half are 255.
 *      (5) The basic idea of Niblack and Sauvola binarization is that
 *          the local threshold should be less than the median value,
 *          and the larger the variance, the closer to the median
 *          it should be chosen.  Typical values for k are between
 *          0.2 and 0.5.
 * </pre>
 */
l_ok
pixSauvolaBinarize(PIX       *pixs,
                   l_int32    whsize,
                   l_float32  factor,
                   l_int32    addborder,
                   PIX      **ppixm,
                   PIX      **ppixsd,
                   PIX      **ppixth,
                   PIX      **ppixd)
{
l_int32  w, h;
PIX     *pixg, *pixsc, *pixm = NULL, *pixms = NULL, *pixth = NULL, *pixd = NULL;

    if (ppixm) *ppixm = NULL;
    if (ppixsd) *ppixsd = NULL;
    if (ppixth) *ppixth = NULL;
    if (ppixd) *ppixd = NULL;
    if (!ppixm && !ppixsd && !ppixth && !ppixd)
        return ERROR_INT("no outputs", __func__, 1);
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs undefined or not 8 bpp", __func__, 1);
    if (pixGetColormap(pixs))
        return ERROR_INT("pixs is cmapped", __func__, 1);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (whsize < 2)
        return ERROR_INT("whsize must be >= 2", __func__, 1);
    if (w < 2 * whsize + 3 || h < 2 * whsize + 3)
        return ERROR_INT("whsize too large for image", __func__, 1);
    if (factor < 0.0)
        return ERROR_INT("factor must be >= 0", __func__, 1);

    if (addborder) {
        pixg = pixAddMirroredBorder(pixs, whsize + 1, whsize + 1,
                                    whsize + 1, whsize + 1);
        pixsc = pixClone(pixs);
    } else {
        pixg = pixClone(pixs);
        pixsc = pixRemoveBorder(pixs, whsize + 1);
    }
    if (!pixg || !pixsc)
        return ERROR_INT("pixg and pixsc not made", __func__, 1);

        /* All these functions strip off the border pixels. */
    if (ppixm || ppixth || ppixd)
        pixm = pixWindowedMean(pixg, whsize, whsize, 1, 1);
    if (ppixsd || ppixth || ppixd)
        pixms = pixWindowedMeanSquare(pixg, whsize, whsize, 1);
    if (ppixth || ppixd)
        pixth = pixSauvolaGetThreshold(pixm, pixms, factor, ppixsd);
    if (ppixd) {
        pixd = pixApplyLocalThreshold(pixsc, pixth);
        pixCopyResolution(pixd, pixs);
    }

    if (ppixm)
        *ppixm = pixm;
    else
        pixDestroy(&pixm);
    pixDestroy(&pixms);
    if (ppixth)
        *ppixth = pixth;
    else
        pixDestroy(&pixth);
    if (ppixd)
        *ppixd = pixd;
    pixDestroy(&pixg);
    pixDestroy(&pixsc);
    return 0;
}


/*!
 * \brief   pixSauvolaGetThreshold()
 *
 * \param[in]    pixm     8 bpp grayscale; not colormapped
 * \param[in]    pixms    32 bpp
 * \param[in]    factor   factor for reducing threshold due to variance; >= 0
 * \param[out]   ppixsd   [optional] local standard deviation
 * \return  pixd   8 bpp, sauvola threshold values, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The Sauvola threshold is determined from the formula:
 *            t = m * (1 - k * (1 - s / 128))
 *          where:
 *            t = local threshold
 *            m = local mean
 *            k = %factor (>= 0)   [ typ. 0.35 ]
 *            s = local standard deviation, which is maximized at
 *                127.5 when half the samples are 0 and half are 255.
 *      (2) See pixSauvolaBinarize() for other details.
 *      (3) Important definitions and relations for computing averages:
 *            v == pixel value
 *            E(p) == expected value of p == average of p over some pixel set
 *            S(v) == square of v == v * v
 *            mv == E(v) == expected pixel value == mean value
 *            ms == E(S(v)) == expected square of pixel values
 *               == mean square value
 *            var == variance == expected square of deviation from mean
 *                == E(S(v - mv)) = E(S(v) - 2 * S(v * mv) + S(mv))
 *                                = E(S(v)) - S(mv)
 *                                = ms - mv * mv
 *            s == standard deviation = sqrt(var)
 *          So for evaluating the standard deviation in the Sauvola
 *          threshold, we take
 *            s = sqrt(ms - mv * mv)
 * </pre>
 */
static PIX *
pixSauvolaGetThreshold(PIX       *pixm,
                       PIX       *pixms,
                       l_float32  factor,
                       PIX      **ppixsd)
{
l_int32     i, j, w, h, tabsize, wplm, wplms, wplsd, wpld, usetab;
l_int32     mv, ms, var, thresh;
l_uint32   *datam, *datams, *datasd = NULL, *datad;
l_uint32   *linem, *linems, *linesd = NULL, *lined;
l_float32   sd;
l_float32  *tab = NULL;  /* of 2^16 square roots */
PIX        *pixsd = NULL, *pixd = NULL;

    if (ppixsd) *ppixsd = NULL;
    if (!pixm || pixGetDepth(pixm) != 8)
        return (PIX *)ERROR_PTR("pixm undefined or not 8 bpp", __func__, NULL);
    if (pixGetColormap(pixm))
        return (PIX *)ERROR_PTR("pixm is colormapped", __func__, NULL);
    if (!pixms || pixGetDepth(pixms) != 32)
        return (PIX *)ERROR_PTR("pixms undefined or not 32 bpp",
                                __func__, NULL);
    if (factor < 0.0)
        return (PIX *)ERROR_PTR("factor must be >= 0", __func__, NULL);

        /* Only make a table of 2^16 square roots if there
         * are enough pixels to justify it. */
    pixGetDimensions(pixm, &w, &h, NULL);
    usetab = (w * h > 100000) ? 1 : 0;
    if (usetab) {
        tabsize = 1 << 16;
        tab = (l_float32 *)LEPT_CALLOC(tabsize, sizeof(l_float32));
        for (i = 0; i < tabsize; i++)
            tab[i] = sqrtf((l_float32)i);
    }

    pixd = pixCreate(w, h, 8);
    if (ppixsd) {
        pixsd = pixCreate(w, h, 8);
        *ppixsd = pixsd;
    }
    datam = pixGetData(pixm);
    datams = pixGetData(pixms);
    if (ppixsd) datasd = pixGetData(pixsd);
    datad = pixGetData(pixd);
    wplm = pixGetWpl(pixm);
    wplms = pixGetWpl(pixms);
    if (ppixsd) wplsd = pixGetWpl(pixsd);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        linem = datam + i * wplm;
        linems = datams + i * wplms;
        if (ppixsd) linesd = datasd + i * wplsd;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            mv = GET_DATA_BYTE(linem, j);
            ms = linems[j];
            var = ms - mv * mv;
            if (usetab)
                sd = tab[var];
            else
                sd = sqrtf((l_float32)var);
            if (ppixsd) SET_DATA_BYTE(linesd, j, (l_int32)sd);
            thresh = (l_int32)(mv * (1.0 - factor * (1.0 - sd / 128.)));
            SET_DATA_BYTE(lined, j, thresh);
        }
    }

    if (usetab) LEPT_FREE(tab);
    return pixd;
}


/*!
 * \brief   pixApplyLocalThreshold()
 *
 * \param[in]    pixs     8 bpp grayscale; not colormapped
 * \param[in]    pixth    8 bpp array of local thresholds
 * \return  pixd   1 bpp, thresholded image, or NULL on error
 */
static PIX *
pixApplyLocalThreshold(PIX     *pixs,
                       PIX     *pixth)
{
l_int32    i, j, w, h, wpls, wplt, wpld, vals, valt;
l_uint32  *datas, *datat, *datad, *lines, *linet, *lined;
PIX       *pixd;

    if (!pixs || pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs undefined or not 8 bpp", __func__, NULL);
    if (pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs is colormapped", __func__, NULL);
    if (!pixth || pixGetDepth(pixth) != 8)
        return (PIX *)ERROR_PTR("pixth undefined or not 8 bpp", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    pixd = pixCreate(w, h, 1);
    datas = pixGetData(pixs);
    datat = pixGetData(pixth);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wplt = pixGetWpl(pixth);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        linet = datat + i * wplt;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            vals = GET_DATA_BYTE(lines, j);
            valt = GET_DATA_BYTE(linet, j);
            if (vals < valt)
                SET_DATA_BIT(lined, j);
        }
    }

    return pixd;
}


/*----------------------------------------------------------------------*
 *      Contrast normalization followed by Sauvola binarization         *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixSauvolaOnContrastNorm()
 *
 * \param[in]    pixs          8 or 32 bpp
 * \param[in]    mindiff       minimum diff to accept as valid in contrast
 *                             normalization.  Use ~130 for noisy images.
 * \param[out]   ppixn         [optional] intermediate output from contrast
 *                             normalization
 * \param[out]   ppixth        [optional] threshold array for binarization
 * \return  pixd    1 bpp thresholded image, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This composite operation is good for adaptively removing
 *          dark background.
 * </pre>
 */
PIX  *
pixSauvolaOnContrastNorm(PIX     *pixs,
                         l_int32  mindiff,
                         PIX    **ppixn,
                         PIX    **ppixth)
{
l_int32  w, h, d, nx, ny;
PIX     *pixg, *pix1, *pixd;

    if (ppixn) *ppixn = NULL;
    if (ppixth) *ppixth = NULL;
    if (!pixs || (d = pixGetDepth(pixs)) < 8)
        return (PIX *)ERROR_PTR("pixs undefined or d < 8 bpp", __func__, NULL);
    if (d == 32)
        pixg = pixConvertRGBToGray(pixs, 0.3f, 0.4f, 0.3f);
    else
        pixg = pixConvertTo8(pixs, 0);

    pix1 = pixContrastNorm(NULL, pixg, 50, 50, mindiff, 2, 2);

        /* Use tiles of size approximately 250 x 250 */
    pixGetDimensions(pix1, &w, &h, NULL);
    nx = L_MAX(1, (w + 125) / 250);
    ny = L_MAX(1, (h + 125) / 250);
    pixSauvolaBinarizeTiled(pix1, 25, 0.40f, nx, ny, ppixth, &pixd);
    pixDestroy(&pixg);
    if (ppixn)
        *ppixn = pix1;
    else
        pixDestroy(&pix1);
    return pixd;
}


/*----------------------------------------------------------------------*
 *      Contrast normalization followed by background normalization     *
 *                            and thresholding                          *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixThreshOnDoubleNorm()
 *
 * \param[in]    pixs          8 or 32 bpp
 * \param[in]    mindiff       minimum diff to accept as valid in contrast
 *                             normalization.  Use ~130 for noisy images.
 * \return  pixd    1 bpp thresholded image, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This composite operation is good for adaptively removing
 *          dark background.
 *      (2) The threshold for the binarization uses an estimate based
 *          on Otsu adaptive thresholding.
 * </pre>
 */
PIX  *
pixThreshOnDoubleNorm(PIX     *pixs,
                      l_int32  mindiff)
{
l_int32    d, ival;
l_uint32   val;
PIX       *pixg, *pix1, *pixd;

    if (!pixs || (d = pixGetDepth(pixs)) < 8)
        return (PIX *)ERROR_PTR("pixs undefined or d < 8 bpp", __func__, NULL);
    if (d == 32)
        pixg = pixConvertRGBToGray(pixs, 0.3f, 0.4f, 0.3f);
    else
        pixg = pixConvertTo8(pixs, 0);

        /* Use the entire image for the estimate; pix1 is 1x1 */
    pixOtsuAdaptiveThreshold(pixg, 5000, 5000, 0, 0, 0.1f, &pix1, NULL);
    pixGetPixel(pix1, 0, 0, &val);
    ival = (l_int32)val;
    ival = L_MIN(ival, 110);
    pixDestroy(&pix1);

        /* Double normalization */
    pixContrastNorm(pixg, pixg, 50, 50, mindiff, 2, 2);
    pix1 = pixBackgroundNormSimple(pixg, NULL, NULL);
    pixDestroy(&pixg);

/*    lept_stderr("ival = %d\n", ival); */
    pixd = pixThresholdToBinary(pix1, ival);
    pixDestroy(&pix1);
    return pixd;
}


/*----------------------------------------------------------------------*
 *            Global thresholding using connected components            *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixThresholdByConnComp()
 *
 * \param[in]    pixs          depth > 1, colormap OK
 * \param[in]    pixm          [optional] 1 bpp mask giving region to ignore
 *                             by setting pixels to white; use NULL if no mask
 * \param[in]    start, end, incr   binarization threshold levels to test
 * \param[in]    thresh48      threshold on normalized difference between the
 *                             numbers of 4 and 8 connected components
 * \param[in]    threshdiff    threshold on normalized difference between the
 *                             number of 4 cc at successive iterations
 * \param[out]   pglobthresh   [optional] best global threshold; 0
 *                             if no threshold is found
 * \param[out]   ppixd         [optional] image thresholded to binary, or
 *                             null if no threshold is found
 * \param[in]    debugflag     1 for plotted results
 * \return  0 if OK, 1 on error or if no threshold is found
 *
 * <pre>
 * Notes:
 *      (1) This finds a global threshold based on connected components.
 *          Although slow, it is reasonable to use it in a situation where
 *          (a) the background in the image is relatively uniform, and
 *          (b) the result will be fed to an OCR program that accepts 1 bpp
 *              images and works best with easily segmented characters.
 *          The reason for (b) is that this selects a threshold with a
 *          minimum number of both broken characters and merged characters.
 *      (2) If the pix has color, it is converted to gray using the
 *          max component.
 *      (3) Input 0 to use default values for any of these inputs:
 *          %start, %end, %incr, %thresh48, %threshdiff.
 *      (4) This approach can be understood as follows.  When the
 *          binarization threshold is varied, the numbers of c.c. identify
 *          four regimes:
 *          (a) For low thresholds, text is broken into small pieces, and
 *              the number of c.c. is large, with the 4 c.c. significantly
 *              exceeding the 8 c.c.
 *          (b) As the threshold rises toward the optimum value, the text
 *              characters coalesce and there is very little difference
 *              between the numbers of 4 and 8 c.c, which both go
 *              through a minimum.
 *          (c) Above this, the image background gets noisy because some
 *              pixels are(thresholded to foreground, and the numbers
 *              of c.c. quickly increase, with the 4 c.c. significantly
 *              larger than the 8 c.c.
 *          (d) At even higher thresholds, the image background noise
 *              coalesces as it becomes mostly foreground, and the
 *              number of c.c. drops quickly.
 *      (5) If there is no global threshold that distinguishes foreground
 *          text from background (e.g., weak text over a background that
 *          has significant variation and/or bleedthrough), this returns 1,
 *          which the caller should check.
 * </pre>
 */
l_ok
pixThresholdByConnComp(PIX       *pixs,
                       PIX       *pixm,
                       l_int32    start,
                       l_int32    end,
                       l_int32    incr,
                       l_float32  thresh48,
                       l_float32  threshdiff,
                       l_int32   *pglobthresh,
                       PIX      **ppixd,
                       l_int32    debugflag)
{
l_int32    i, thresh, n, n4, n8, mincounts, found, globthresh;
l_float32  count4, count8, firstcount4, prevcount4, diff48, diff4;
GPLOT     *gplot;
NUMA      *na4, *na8;
PIX       *pix1, *pix2, *pix3;

    if (pglobthresh) *pglobthresh = 0;
    if (ppixd) *ppixd = NULL;
    if (!pixs || pixGetDepth(pixs) == 1)
        return ERROR_INT("pixs undefined or 1 bpp", __func__, 1);
    if (pixm && pixGetDepth(pixm) != 1)
        return ERROR_INT("pixm must be 1 bpp", __func__, 1);

        /* Assign default values if requested */
    if (start <= 0) start = 80;
    if (end <= 0) end = 200;
    if (incr <= 0) incr = 10;
    if (thresh48 <= 0.0) thresh48 = 0.01f;
    if (threshdiff <= 0.0) threshdiff = 0.01f;
    if (start > end)
        return ERROR_INT("invalid start,end", __func__, 1);

        /* Make 8 bpp, using the max component if color. */
    if (pixGetColormap(pixs))
        pix1 = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
    else
        pix1 = pixClone(pixs);
    if (pixGetDepth(pix1) == 32)
        pix2 = pixConvertRGBToGrayMinMax(pix1, L_CHOOSE_MAX);
    else
        pix2 = pixConvertTo8(pix1, 0);
    pixDestroy(&pix1);

        /* Mask out any non-text regions.  Do this in-place, because pix2
         * can never be the same pix as pixs. */
    if (pixm)
        pixSetMasked(pix2, pixm, 255);

        /* Make sure there are enough components to get a valid signal */
    pix3 = pixConvertTo1(pix2, start);
    pixCountConnComp(pix3, 4, &n4);
    pixDestroy(&pix3);
    mincounts = 500;
    if (n4 < mincounts) {
        L_INFO("Insufficient component count: %d\n", __func__, n4);
        pixDestroy(&pix2);
        return 1;
    }

        /* Compute the c.c. data */
    na4 = numaCreate(0);
    na8 = numaCreate(0);
    numaSetParameters(na4, start, incr);
    numaSetParameters(na8, start, incr);
    for (thresh = start, i = 0; thresh <= end; thresh += incr, i++) {
        pix3 = pixConvertTo1(pix2, thresh);
        pixCountConnComp(pix3, 4, &n4);
        pixCountConnComp(pix3, 8, &n8);
        numaAddNumber(na4, n4);
        numaAddNumber(na8, n8);
        pixDestroy(&pix3);
    }
    if (debugflag) {
        lept_mkdir("lept/binarize");
        gplot = gplotCreate("/tmp/lept/binarize", GPLOT_PNG,
                            "number of cc vs. threshold",
                            "threshold", "number of cc");
        gplotAddPlot(gplot, NULL, na4, GPLOT_LINES, "plot 4cc");
        gplotAddPlot(gplot, NULL, na8, GPLOT_LINES, "plot 8cc");
        gplotMakeOutput(gplot);
        gplotDestroy(&gplot);
    }

    n = numaGetCount(na4);
    found = FALSE;
    for (i = 0; i < n; i++) {
        if (i == 0) {
            numaGetFValue(na4, i, &firstcount4);
            prevcount4 = firstcount4;
        } else {
            numaGetFValue(na4, i, &count4);
            numaGetFValue(na8, i, &count8);
            diff48 = (count4 - count8) / firstcount4;
            diff4 = L_ABS(prevcount4 - count4) / firstcount4;
            if (debugflag) {
                lept_stderr("diff48 = %7.3f, diff4 = %7.3f\n",
                            diff48, diff4);
            }
            if (diff48 < thresh48 && diff4 < threshdiff) {
                found = TRUE;
                break;
            }
            prevcount4 = count4;
        }
    }
    numaDestroy(&na4);
    numaDestroy(&na8);

    if (found) {
        globthresh = start + i * incr;
        if (pglobthresh) *pglobthresh = globthresh;
        if (ppixd) {
            *ppixd = pixConvertTo1(pix2, globthresh);
            pixCopyResolution(*ppixd, pixs);
        }
        if (debugflag) lept_stderr("global threshold = %d\n", globthresh);
        pixDestroy(&pix2);
        return 0;
    }

    if (debugflag) lept_stderr("no global threshold found\n");
    pixDestroy(&pix2);
    return 1;
}

/*----------------------------------------------------------------------*
 *                   Global thresholding by histogram                   *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixThresholdByHisto()
 *
 * \param[in]    pixs        gray 8 bpp, no colormap
 * \param[in]    factor      subsampling factor >= 1
 * \param[in]    halfw       half of window width for smoothing;
 *                           use 0 for default
 * \param[in]    skip        look-ahead distance to avoid false minima;
 *                           use 0 for default
 * \param[out]   pthresh     best global threshold; 0 if no threshold is found
 * \param[out]   ppixd       [optional] thresholded 1 bpp pix
 * \param[out]   pnahisto    [optional] rescaled histogram of gray values
 * \param[out]   ppixhisto   [optional] plot of rescaled histogram
 * \return  0 if OK, 1 on error or if no threshold is found
 *
 * <pre>
 * Notes:
 *      (1) This finds a global threshold.  It is best for an image that
 *          has a fairly well-defined fg and bg.
 *      (2) If it finds a good threshold and %ppixd is defined, the binarized
 *          image is returned in &pixd; otherwise it return null.
 *      (3) See numaFindLocForThreshold() for use of %skip.
 *      (4) Suggest using default values (20) for %half and %skip.
 *      (5) Returns 0 in %pthresh if it can't find a good threshold.
 * </pre>
 */
l_ok
pixThresholdByHisto(PIX       *pixs,
                    l_int32    factor,
                    l_int32    halfw,
                    l_int32    skip,
                    l_int32   *pthresh,
                    PIX      **ppixd,
                    NUMA     **pnahisto,
                    PIX      **ppixhisto)
{
l_float32  maxval, fract;
NUMA      *na1, *na2, *na3;

    if (ppixd) *ppixd = NULL;
    if (pnahisto) *pnahisto = NULL;
    if (ppixhisto) *ppixhisto = NULL;
    if (!pthresh)
        return ERROR_INT("&thresh not defined", __func__, 1);
    *pthresh = 0;
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs undefined or not 8 bpp", __func__, 1);
    if (pixGetColormap(pixs))
        return ERROR_INT("pixs has colormap", __func__, 1);
    if (factor < 1)
        return ERROR_INT("sampling must be >= 1", __func__, 1);
    if (halfw <= 0) halfw = 20;
    if (skip <= 0) skip = 20;

        /* Make a histogram of pixel values where the largest peak
         * is normalized to a value of 1.0. */
    na1 = pixGetGrayHistogram(pixs, factor);
    na2 = numaWindowedMean(na1, halfw);  /* smoothing */
    numaGetMax(na2, &maxval, NULL);
    na3 = numaTransform(na2, 0.0, 1.0 / maxval);  /* rescale to max of 1.0 */
    numaDestroy(&na1);
    numaDestroy(&na2);

    if (numaFindLocForThreshold(na3, skip, pthresh, &fract) == 1) {
        numaDestroy(&na3);
        return ERROR_INT("failure to find threshold", __func__, 1);
    }
    L_INFO("fractional area under first peak: %5.3f\n", __func__, fract);

    if (ppixhisto) {
        lept_mkdir("lept/histo");
        gplotSimple1(na3, GPLOT_PNG, "/tmp/lept/histo/histo", NULL);
        *ppixhisto = pixRead("/tmp/lept/histo/histo.png");
    }
    if (pnahisto)
        *pnahisto = na3;
    else
        numaDestroy(&na3);
    if (*pthresh > 0 && ppixd)
        *ppixd = pixThresholdToBinary(pixs, *pthresh);
    return 0;
}


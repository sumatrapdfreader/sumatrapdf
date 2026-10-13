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
 * \file colorcontent.c
 * <pre>
 *
 *      Build an image of the color content, on a per-pixel basis,
 *      as a measure of the amount of divergence of each color
 *      component (R,G,B) from gray.
 *         l_int32    pixColorContent()
 *
 *      Find the 'amount' of color in an image, on a per-pixel basis,
 *      as a measure of the difference of the pixel color from gray.
 *         PIX       *pixColorMagnitude()
 *
 *      Find the fraction of pixels with "color" that are not close to black
 *         l_int32    pixColorFraction()
 *
 *      Do a linear TRC to map colors so that the three input reference
 *      values go to white.  These three numbers are typically the median
 *      or average background values.
 *         PIX       *pixColorShiftWhitePoint()
 *
 *      Generate a mask over pixels that have sufficient color and
 *      are not too close to gray pixels.
 *         PIX       *pixMaskOverColorPixels()
 *
 *      Generate a mask over dark pixels with little color
 *         PIX       *pixMaskOverGrayPixels()
 *
 *      Generate mask over pixels within a prescribed cube in RGB space
 *         PIX       *pixMaskOverColorRange()
 *
 *      Determine if there are significant color regions that are
 *      not background in a page image
 *         l_int32    pixFindColorRegions()
 *
 *      Find the number of perceptually significant gray intensities
 *      in a grayscale image.
 *         l_int32    pixNumSignificantGrayColors()
 *
 *      Identify images where color quantization will cause posterization
 *      due to the existence of many colors in low-gradient regions.
 *         l_int32    pixColorsForQuantization()
 *
 *      Find the number of unique colors in an image
 *         l_int32    pixNumColors()
 *
 *      Lossless conversion of RGB image to colormapped
 *         PIX       *pixConvertRGBToCmapLossless()
 *
 *      Find the most "populated" colors in the image (and quantize)
 *         l_int32    pixGetMostPopulatedColors()
 *         PIX       *pixSimpleColorQuantize()
 *
 *      Construct a color histogram based on rgb indices
 *         NUMA      *pixGetRGBHistogram()
 *         l_int32    makeRGBIndexTables()
 *         l_int32    getRGBFromIndex()
 *
 *      Identify images that have highlight (red) color
 *         l_int32    pixHasHighlightRed()
 *
 *  Color is tricky.  If we consider gray (r = g = b) to have no color
 *  content, how should we define the color content in each component
 *  of an arbitrary pixel, as well as the overall color magnitude?
 *
 *  I can think of three ways to define the color content in each component:
 *
 *  (1) Linear.  For each component, take the difference from the average
 *      of all three.
 *  (2) Linear.  For each component, take the difference from the average
 *      of the other two.
 *  (3) Nonlinear.  For each component, take the minimum of the differences
 *      from the other two.
 *
 *  How might one choose from among these?  Consider two different situations:
 *  (a) r = g = 0, b = 255            {255}   /255/   <255>
 *  (b) r = 0, g = 127, b = 255       {191}   /128/   <255>
 *  How much g is in each of these?  The three methods above give:
 *  (a)  1: 85   2: 127   3: 0        [85]
 *  (b)  1: 0    2: 0     3: 127      [0]
 *  How much b is in each of these?
 *  (a)  1: 170  2: 255   3: 255      [255]
 *  (b)  1: 127  2: 191   3: 127      [191]
 *  The number I'd "like" to give is in [].  (Please don't ask why, it's
 *  just a feeling.
 *
 *  So my preferences seem to be somewhere between (1) and (2).
 *  (3) is just too "decisive!"  Let's pick (2).
 *
 *  We also allow compensation for white imbalance.  For each
 *  component, we do a linear TRC (gamma = 1.0), where the black
 *  point remains at 0 and the white point is given by the input
 *  parameter.  This is equivalent to doing a global remapping,
 *  as with pixGlobalNormRGB(), followed by color content (or magnitude)
 *  computation, but without the overhead of first creating the
 *  white point normalized image.
 *
 *  Another useful property is the overall color magnitude in the pixel.
 *  For this there are again several choices, such as:
 *      (a) rms deviation from the mean
 *      (b) the average L1 deviation from the mean
 *      (c) the maximum (over components) of one of the color
 *          content measures given above.
 *
 *  For now, we will consider three of the methods in (c):
 *    L_INTERMED_DIFF
 *        Define the color magnitude as the intermediate value of the
 *        three differences between the three components.
 *        For (a) and (b) above, this value is in /../.
 *     L_AVE_MAX_DIFF_2
 *        Define the color magnitude as the maximum over components
 *        of the difference between the component value and the
 *        average of the other two.  It is easy to show that
 *        this is equivalent to selecting the two component values
 *        that are closest to each other, averaging them, and
 *        using the distance from that average to the third component.
 *        For (a) and (b) above, this value is in {..}.
 *    L_MAX_DIFF
 *        Define the color magnitude as the maximum value of the
 *        three differences between the three components.
 *        For (a) and (b) above, this value is in <..>.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

/* ----------------------------------------------------------------------- *
 *      Build an image of the color content, on a per-pixel basis,         *
 *      as a measure of the amount of divergence of each color             *
 *      component (R,G,B) from gray.                                       *
 * ----------------------------------------------------------------------- */
/*!
 * \brief   pixColorContent()
 *
 * \param[in]    pixs      32 bpp rgb or 8 bpp colormapped
 * \param[in]    rref      reference value for red component
 * \param[in]    gref      reference value for green component
 * \param[in]    bref      reference value for blue component
 * \param[in]    mingray   min gray value for which color is measured
 * \param[out]   ppixr     [optional] 8 bpp red 'content'
 * \param[out]   ppixg     [optional] 8 bpp green 'content'
 * \param[out]   ppixb     [optional] 8 bpp blue 'content'
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This returns the color content in each component, which is
 *          a measure of the deviation from gray, and is defined
 *          as the difference between the component and the average of
 *          the other two components.  See the discussion at the
 *          top of this file.
 *      (2) The three numbers (rref, gref and bref) can be thought
 *          of in two ways:
 *            (a) as the values in the image corresponding to white,
 *                to compensate for an unbalanced color white point.
 *            (b) the median or mean values of the background color of
 *                a scan.
 *          The gamma TRC transformation is used to modify all colors so that
 *          these reference values become white.
 *          These three numbers must either be all 0 or all non-zero.
 *          To skip the TRC transform, set them all to 0.
 *      (3) If the maximum component after white point correction,
 *          max(r,g,b), is less than mingray, all color components
 *          for that pixel are set to zero.
 *          Use mingray = 0 to turn off this filtering of dark pixels.
 *      (4) Therefore, use 0 for all four input parameters if the color
 *          magnitude is to be calculated without either white balance
 *          correction or dark filtering.
 * </pre>
 */
l_ok
pixColorContent(PIX     *pixs,
                l_int32  rref,
                l_int32  gref,
                l_int32  bref,
                l_int32  mingray,
                PIX    **ppixr,
                PIX    **ppixg,
                PIX    **ppixb)
{
l_int32    w, h, i, j, wpl1, wplr, wplg, wplb;
l_int32    rval, gval, bval, rgdiff, rbdiff, gbdiff, maxval, colorval;
l_uint32   pixel;
l_uint32  *data1, *datar, *datag, *datab, *line1, *liner, *lineg, *lineb;
PIX       *pix1, *pixr, *pixg, *pixb;

    if (!ppixr && !ppixg && !ppixb)
        return ERROR_INT("no return val requested", __func__, 1);
    if (ppixr) *ppixr = NULL;
    if (ppixg) *ppixg = NULL;
    if (ppixb) *ppixb = NULL;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (mingray < 0) mingray = 0;
    if (mingray > 255)
        return ERROR_INT("mingray > 255", __func__, 1);

        /* Do the optional linear color map; this checks the ref vals */
    if ((pix1 = pixColorShiftWhitePoint(pixs, rref, gref, bref)) == NULL)
        return ERROR_INT("pix1 not returned", __func__, 1);

    pixr = pixg = pixb = NULL;
    datar = datag = datab = NULL;
    liner = lineg = lineb = NULL;

	pixGetDimensions(pix1, &w, &h, NULL);
    if (ppixr) {
        pixr = pixCreate(w, h, 8);
        datar = pixGetData(pixr);
        wplr = pixGetWpl(pixr);
        *ppixr = pixr;
    }
    if (ppixg) {
        pixg = pixCreate(w, h, 8);
        datag = pixGetData(pixg);
        wplg = pixGetWpl(pixg);
        *ppixg = pixg;
    }
    if (ppixb) {
        pixb = pixCreate(w, h, 8);
        datab = pixGetData(pixb);
        wplb = pixGetWpl(pixb);
        *ppixb = pixb;
    }

    data1 = pixGetData(pix1);
    wpl1 = pixGetWpl(pix1);
    for (i = 0; i < h; i++) {
        line1 = data1 + i * wpl1;
        if (pixr)
            liner = datar + i * wplr;
        if (pixg)
            lineg = datag + i * wplg;
        if (pixb)
            lineb = datab + i * wplb;
        for (j = 0; j < w; j++) {
            pixel = line1[j];
            extractRGBValues(pixel, &rval, &gval, &bval);
            if (mingray > 0) {  /* dark pixels have no color value */
                maxval = L_MAX(rval, gval);
                maxval = L_MAX(maxval, bval);
                if (maxval < mingray)
                    continue;  /* colorval = 0 for each component */
            }
            rgdiff = L_ABS(rval - gval);
            rbdiff = L_ABS(rval - bval);
            gbdiff = L_ABS(gval - bval);
            if (pixr) {
                colorval = (rgdiff + rbdiff) / 2;
                SET_DATA_BYTE(liner, j, colorval);
            }
            if (pixg) {
                colorval = (rgdiff + gbdiff) / 2;
                SET_DATA_BYTE(lineg, j, colorval);
            }
            if (pixb) {
                colorval = (rbdiff + gbdiff) / 2;
                SET_DATA_BYTE(lineb, j, colorval);
            }
        }
    }

    pixDestroy(&pix1);
    return 0;
}


/* ----------------------------------------------------------------------- *
 *      Find the 'amount' of color in an image, on a per-pixel basis,      *
 *      as a measure of the difference of the pixel color from gray.       *
 * ----------------------------------------------------------------------- */
/*!
 * \brief   pixColorMagnitude()
 *
 * \param[in]    pixs    32 bpp rgb or 8 bpp colormapped
 * \param[in]    rref    reference value for red component
 * \param[in]    gref    reference value for green component
 * \param[in]    bref    reference value for blue component
 * \param[in]    type    chooses the method for calculating the color magnitude:
 *                       L_INTERMED_DIFF, L_AVE_MAX_DIFF_2, L_MAX_DIFF
 * \return  pixd 8 bpp, amount of color in each source pixel,
 *                    or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For an RGB image, a gray pixel is one where all three components
 *          are equal.  We define the amount of color in an RGB pixel as
 *          a function depending on the absolute value of the differences
 *          between the three color components.  Consider the two largest
 *          of these differences.  The pixel component in common to these
 *          two differences is the color farthest from the other two.
 *          The color magnitude in an RGB pixel can be taken as one
 *          of these three definitions:
 *            (a) The minimum value of these two differences.  This is
 *                the intermediate value of the three distances between
 *                component values.
 *            (b) The average of these two differences.  This is the
 *                average distance from the two components that are
 *                nearest to each other to the third component.
 *            (c) The maximum difference between component values.
 *      (2) As an example, suppose that R and G are the closest in
 *          magnitude.  Then the color is determined as either:
 *            (a) The minimum distance of B from these two:
 *                   min(|B - R|, |B - G|).
 *            (b) The average distance of B from these two:
 *                   (|B - R| + |B - G|) / 2
 *            (c) The maximum distance of B from these two:
 *                   max(|B - R|, |B - G|)
 *      (3) This example can be visualized graphically.  Put the R,G and B
 *          component values on a line; e.g.,
 *                G...R...........B
 *            (a) B - R
 *            (b) B - (R + G) / 2
 *            (c) B - G
 *      (4) The three methods for choosing the color magnitude from
 *          the components are selected with these flags:
 *            (a) L_INTERMED_DIFF
 *            (b) L_AVE_MAX_DIFF_2
 *            (c) L_MAX_DIFF
 *      (5) The three numbers (rref, gref and bref) can be thought
 *          of in two ways:
 *            (a) as the values in the image corresponding to white,
 *                to compensate for an unbalanced color white point.
 *            (b) the median or mean values of the background color of
 *                a scan.
 *          The gamma TRC transformation is used to modify all colors so that
 *          these reference values become white.
 *          These three numbers must either be all 0 or all non-zero.
 *          To skip the TRC transform, set them all to 0.
 * </pre>
 */
PIX *
pixColorMagnitude(PIX     *pixs,
                  l_int32  rref,
                  l_int32  gref,
                  l_int32  bref,
                  l_int32  type)
{
l_int32    w, h, i, j, wpl1, wpld;
l_int32    rval, gval, bval, rdist, gdist, bdist, colorval;
l_int32    rgdist, rbdist, gbdist, mindist, maxdist, minval, maxval;
l_uint32   pixel;
l_uint32  *data1, *datad, *line1, *lined;
PIX       *pix1, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (type != L_INTERMED_DIFF && type != L_AVE_MAX_DIFF_2 &&
        type != L_MAX_DIFF)
        return (PIX *)ERROR_PTR("invalid type", __func__, NULL);

        /* Do the optional linear color map; this checks the ref vals */
    if ((pix1 = pixColorShiftWhitePoint(pixs, rref, gref, bref)) == NULL)
        return (PIX *)ERROR_PTR("pix1 not returned", __func__, NULL);

    pixGetDimensions(pix1, &w, &h, NULL);
    pixd = pixCreate(w, h, 8);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    data1 = pixGetData(pix1);
    wpl1 = pixGetWpl(pix1);
    for (i = 0; i < h; i++) {
        line1 = data1 + i * wpl1;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            pixel = line1[j];
            extractRGBValues(pixel, &rval, &gval, &bval);
            if (type == L_INTERMED_DIFF) {
                rgdist = L_ABS(rval - gval);
                rbdist = L_ABS(rval - bval);
                gbdist = L_ABS(gval - bval);
                maxdist = L_MAX(rgdist, rbdist);
                if (gbdist >= maxdist) {
                    colorval = maxdist;
                } else {  /* gbdist is smallest or intermediate */
                    mindist = L_MIN(rgdist, rbdist);
                    colorval = L_MAX(mindist, gbdist);
                }
            } else if (type == L_AVE_MAX_DIFF_2) {
                rdist = ((gval + bval ) / 2 - rval);
                rdist = L_ABS(rdist);
                gdist = ((rval + bval ) / 2 - gval);
                gdist = L_ABS(gdist);
                bdist = ((rval + gval ) / 2 - bval);
                bdist = L_ABS(bdist);
                colorval = L_MAX(rdist, gdist);
                colorval = L_MAX(colorval, bdist);
            } else {  /* type == L_MAX_DIFF */
                minval = L_MIN(rval, gval);
                minval = L_MIN(minval, bval);
                maxval = L_MAX(rval, gval);
                maxval = L_MAX(maxval, bval);
                colorval = maxval - minval;
            }
            SET_DATA_BYTE(lined, j, colorval);
        }
    }

    pixDestroy(&pix1);
    return pixd;
}


/* ----------------------------------------------------------------------- *
 *   Find the fraction of pixels with "color" that are not close to black  *
 * ----------------------------------------------------------------------- */
/*!
 * \brief   pixColorFraction()
 *
 * \param[in]    pixs  32 bpp rgb
 * \param[in]    darkthresh    threshold near black; if the largest (lightest)
 *                             component is below this, the pixel is not
 *                             considered in the statistics; typ. 20
 * \param[in]    lightthresh   threshold near white; if the smallest (darkest)
 *                             component is above this, the pixel is not
 *                             considered in the statistics; typ. 244
 * \param[in]    diffthresh    thresh for the maximum difference between
 *                             component values; below this the pixel is not
 *                             considered to have sufficient color; typ. 60.
 * \param[in]    factor        subsampling factor
 * \param[out]   ppixfract     [optional] fraction of pixels in intermediate
 *                             brightness range that were considered
 *                             for color content
 * \param[out]   pcolorfract   [optional] fraction of pixels that meet the
 *                             criterion for sufficient color; 0.0 on error
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function is asking the question: to what extent does the
 *          image appear to have color?   The amount of color a pixel
 *          appears to have depends on both the deviation of the
 *          individual components from their average and on the average
 *          intensity itself.  For example, the color will be much more
 *          obvious with a small deviation from white than the same
 *          deviation from black.
 *      (2) Any pixel that meets these three tests is considered a
 *          colorful pixel:
 *            (a) the lightest component must equal or exceed %darkthresh
 *            (b) the darkest component must not exceed %lightthresh
 *            (c) the max difference between components must equal or
 *                exceed %diffthresh.
 *      (3) The dark pixels are removed from consideration because
 *          they don't appear to have color.
 *      (4) The very lightest pixels are removed because if an image
 *          has a lot of "white", the color fraction will be artificially
 *          low, even if all the other pixels are colorful.
 *      (5) If pixfract is very small, there are few pixels that are neither
 *          black nor white.  If colorfract is very small, the pixels
 *          that are neither black nor white have very little color
 *          content.  The product 'pixfract * colorfract' gives the
 *          fraction of pixels with significant color content.
 *      (6) One use of this function is as a preprocessing step for median
 *          cut quantization (colorquant2.c), which does a very poor job
 *          splitting the color space into rectangular volume elements when
 *          all the pixels are near the diagonal of the color cube.  For
 *          octree quantization of an image with only gray values, the
 *          2^(level) octcubes on the diagonal are the only ones
 *          that can be occupied.
 * </pre>
 */
l_ok
pixColorFraction(PIX        *pixs,
                 l_int32     darkthresh,
                 l_int32     lightthresh,
                 l_int32     diffthresh,
                 l_int32     factor,
                 l_float32  *ppixfract,
                 l_float32  *pcolorfract)
{
l_int32    i, j, w, h, wpl, rval, gval, bval, minval, maxval;
l_int32    total, npix, ncolor;
l_uint32   pixel;
l_uint32  *data, *line;

    if (ppixfract) *ppixfract = 0.0;
    if (pcolorfract) *pcolorfract = 0.0;
    if (!ppixfract && !pcolorfract)
        return ERROR_INT("neither &pixfract nor &colorfract are defined",
                         __func__, 1);
    if (!pixs || pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs not defined or not 32 bpp", __func__, 1);

    pixGetDimensions(pixs, &w, &h, NULL);
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    npix = ncolor = total = 0;
    for (i = 0; i < h; i += factor) {
        line = data + i * wpl;
        for (j = 0; j < w; j += factor) {
            total++;
            pixel = line[j];
            extractRGBValues(pixel, &rval, &gval, &bval);
            minval = L_MIN(rval, gval);
            minval = L_MIN(minval, bval);
            if (minval > lightthresh)  /* near white */
                continue;
            maxval = L_MAX(rval, gval);
            maxval = L_MAX(maxval, bval);
            if (maxval < darkthresh)  /* near black */
                continue;

            npix++;
            if (maxval - minval >= diffthresh)
                ncolor++;
        }
    }

    if (npix == 0) {
        L_WARNING("No pixels found for consideration\n", __func__);
        return 0;
    }
    if (ppixfract) *ppixfract = (l_float32)npix / (l_float32)total;
    if (pcolorfract) *pcolorfract = (l_float32)ncolor / (l_float32)npix;
    return 0;
}


/* ----------------------------------------------------------------------- *
 *      Do a linear TRC to map colors so that the three input reference    *
 *      values go to white.  These three numbers are typically the median  *
 *      or average background values.                                      *
 * ----------------------------------------------------------------------- */
/*!
 * \brief   pixColorShiftWhitePoint()
 *
 * \param[in]    pixs      32 bpp rgb or 8 bpp colormapped
 * \param[in]    rref      reference value for red component
 * \param[in]    gref      reference value for green component
 * \param[in]    bref      reference value for blue component
 * \return  pix2  32 bpp if OK, NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This returns a pix where the colors are linearly mapped to
 *          so that the components go to 255 at the input reference values.
 *      (2) These three numbers (rref, gref and bref) can be thought
 *          of in two ways:
 *            (a) as the values in the image corresponding to white,
 *                to compensate for an unbalanced color white point.
 *            (b) the median or mean values of the background color of
 *                an image.
 *          A linear (gamma = 1) TRC transformation is used.
 *      (3) Any existing colormap is removed and a 32 bpp rgb pix is returned.
 *      (4) No transformation is applied if any of the three numbers are <= 0.
 *          If any are < 0, or if some but not all are 0, a warning is given.
 * </pre>
 */
PIX *
pixColorShiftWhitePoint(PIX     *pixs,
                        l_int32  rref,
                        l_int32  gref,
                        l_int32  bref)
{
l_int32    w, h, i, j, wpl1, wpl2, rval, gval, bval;
l_int32   *rtab, *gtab, *btab;
l_uint32   pixel;
l_uint32  *data1, *data2, *line1, *line2;
NUMA      *nar, *nag, *nab;
PIX       *pix1, *pix2;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

    cmap = pixGetColormap(pixs);
    if (!cmap && pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs neither cmapped nor 32 bpp",
                                __func__, NULL);
    if (cmap)
        pix1 = pixRemoveColormap(pixs, REMOVE_CMAP_TO_FULL_COLOR);
    else
        pix1 = pixClone(pixs);

    if (!rref && !gref && !bref)  /* all 0; no transform requested */
        return pix1;

        /* Some ref values are < 0, or some (but not all) are 0 */
    if ((rref < 0 || gref < 0 || bref < 0) || (rref * gref * bref == 0)) {
        L_WARNING("invalid set of ref values\n", __func__);
        return pix1;
    }

        /* All white point ref values > 0; do transformation */
    pixGetDimensions(pix1, &w, &h, NULL);
    pix2 = pixCreate(w, h, 32);
    data1 = pixGetData(pix1);
    wpl1 = pixGetWpl(pix1);
    data2 = pixGetData(pix2);
    wpl2 = pixGetWpl(pix2);
    nar = numaGammaTRC(1.0, 0, rref);
    rtab = numaGetIArray(nar);
    nag = numaGammaTRC(1.0, 0, gref);
    gtab = numaGetIArray(nag);
    nab = numaGammaTRC(1.0, 0, bref);
    btab = numaGetIArray(nab);
    for (i = 0; i < h; i++) {
        line1 = data1 + i * wpl1;
        line2 = data2 + i * wpl2;
        for (j = 0; j < w; j++) {
            pixel = line1[j];
            extractRGBValues(pixel, &rval, &gval, &bval);
            rval = rtab[rval];
            gval = gtab[gval];
            bval = btab[bval];
            composeRGBPixel(rval, gval, bval, line2 + j);
        }
    }
    numaDestroy(&nar);
    numaDestroy(&nag);
    numaDestroy(&nab);
    LEPT_FREE(rtab);
    LEPT_FREE(gtab);
    LEPT_FREE(btab);
    pixDestroy(&pix1);
    return pix2;
}


/* ----------------------------------------------------------------------- *
 *      Generate a mask over pixels that have sufficient color and         *
 *      are not too close to gray pixels.                                  *
 * ----------------------------------------------------------------------- */
/*!
 * \brief   pixMaskOverColorPixels()
 *
 * \param[in]    pixs         32 bpp rgb or 8 bpp colormapped
 * \param[in]    threshdiff   threshold for minimum of the max difference
 *                            between components
 * \param[in]    mindist      min allowed distance from nearest non-color pixel
 * \return  pixd 1 bpp, mask over color pixels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The generated mask identifies each pixel as either color or
 *          non-color.  For a pixel to be color, it must satisfy two
 *          constraints:
 *            (a) The max difference between the r,g and b components must
 *                equal or exceed a threshold %threshdiff.
 *            (b) It must be at least %mindist (in an 8-connected way)
 *                from the nearest non-color pixel.
 *      (2) The distance constraint (b) is only applied if %mindist > 1.
 *          For example, if %mindist == 2, the color pixels identified
 *          by (a) are eroded by a 3x3 Sel.  In general, the Sel size
 *          for erosion is 2 * (%mindist - 1) + 1.
 *          Why have this constraint?  In scanned images that are
 *          essentially gray, color artifacts are typically introduced
 *          in transition regions near sharp edges that go from dark
 *          to light, so this allows these transition regions to be removed.
 * </pre>
 */
PIX *
pixMaskOverColorPixels(PIX     *pixs,
                       l_int32  threshdiff,
                       l_int32  mindist)
{
l_int32    w, h, d, i, j, wpls, wpld, size;
l_int32    rval, gval, bval, minval, maxval;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixc, *pixd;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);

    cmap = pixGetColormap(pixs);
    if (!cmap && d != 32)
        return (PIX *)ERROR_PTR("pixs not cmapped or 32 bpp", __func__, NULL);
    if (cmap)
        pixc = pixRemoveColormap(pixs, REMOVE_CMAP_TO_FULL_COLOR);
    else
        pixc = pixClone(pixs);
    if (!pixc || pixGetDepth(pixc) != 32) {
        pixDestroy(&pixc);
        return (PIX *)ERROR_PTR("rgb pix not made", __func__, NULL);
    }

    pixd = pixCreate(w, h, 1);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    datas = pixGetData(pixc);
    wpls = pixGetWpl(pixc);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            extractRGBValues(lines[j], &rval, &gval, &bval);
            minval = L_MIN(rval, gval);
            minval = L_MIN(minval, bval);
            maxval = L_MAX(rval, gval);
            maxval = L_MAX(maxval, bval);
            if (maxval - minval >= threshdiff)
                SET_DATA_BIT(lined, j);
        }
    }

    if (mindist > 1) {
        size = 2 * (mindist - 1) + 1;
        pixErodeBrick(pixd, pixd, size, size);
    }

    pixDestroy(&pixc);
    return pixd;
}


/* ----------------------------------------------------------------------- *
 *          Generate a mask over dark pixels with little color             *
 * ----------------------------------------------------------------------- */
/*!
 * \brief   pixMaskOverGrayPixels()
 *
 * \param[in]    pixs      32 bpp rgb
 * \param[in]    maxlimit  only consider pixels with max component <= %maxlimit
 * \param[in]    satlimit  only consider pixels with saturation <= %satlimit
 * \return  pixd (1 bpp), or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a mask over rgb pixels that are gray (i.e.,
 *          have low saturation) and are not too bright.  For example, if
 *          we know that the gray pixels in %pixs have saturation
 *          (max - min) less than 10, and brightness (max) less than 200,
 *             pixMaskOverGrayPixels(pixs, 220, 10)
 *          will generate a mask over the gray pixels.  Other pixels that
 *          are not too dark and have a relatively large saturation will
 *          be little affected.
 *      (2) The algorithm is related to pixDarkenGray().
 * </pre>
 */
PIX *
pixMaskOverGrayPixels(PIX     *pixs,
                      l_int32  maxlimit,
                      l_int32  satlimit)
{
l_int32    w, h, i, j, wpls, wpld;
l_int32    rval, gval, bval, minrg, min, maxrg, max, sat;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);
    if (maxlimit < 0 || maxlimit > 255)
        return (PIX *)ERROR_PTR("invalid maxlimit", __func__, NULL);
    if (satlimit < 1)
        return (PIX *)ERROR_PTR("invalid satlimit", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if ((pixd = pixCreate(w, h, 1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            extractRGBValues(lines[j], &rval, &gval, &bval);
            minrg = L_MIN(rval, gval);
            min = L_MIN(minrg, bval);
            maxrg = L_MAX(rval, gval);
            max = L_MAX(maxrg, bval);
            sat = max - min;
            if (max <= maxlimit && sat <= satlimit)
                SET_DATA_BIT(lined, j);
        }
    }
    return pixd;
}


/* ----------------------------------------------------------------------- *
 *      Generate a mask over pixels that have RGB color components         *
 *      within the prescribed range (a cube in RGB color space)            *
 * ----------------------------------------------------------------------- */
/*!
 * \brief   pixMaskOverColorRange()
 *
 * \param[in]    pixs          32 bpp rgb or 8 bpp colormapped
 * \param[in]    rmin, rmax    min and max allowed values for red component
 * \param[in]    gmin, gmax    ditto for green
 * \param[in]    bmin, bmax    ditto for blue
 * \return  pixd 1 bpp, mask over color pixels, or NULL on error
 */
PIX *
pixMaskOverColorRange(PIX     *pixs,
                      l_int32  rmin,
                      l_int32  rmax,
                      l_int32  gmin,
                      l_int32  gmax,
                      l_int32  bmin,
                      l_int32  bmax)
{
l_int32    w, h, d, i, j, wpls, wpld;
l_int32    rval, gval, bval;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixc, *pixd;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);

    cmap = pixGetColormap(pixs);
    if (!cmap && d != 32)
        return (PIX *)ERROR_PTR("pixs not cmapped or 32 bpp", __func__, NULL);
    if (cmap)
        pixc = pixRemoveColormap(pixs, REMOVE_CMAP_TO_FULL_COLOR);
    else
        pixc = pixClone(pixs);

    pixd = pixCreate(w, h, 1);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    datas = pixGetData(pixc);
    wpls = pixGetWpl(pixc);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            extractRGBValues(lines[j], &rval, &gval, &bval);
            if (rval < rmin || rval > rmax) continue;
            if (gval < gmin || gval > gmax) continue;
            if (bval < bmin || bval > bmax) continue;
            SET_DATA_BIT(lined, j);
        }
    }

    pixDestroy(&pixc);
    return pixd;
}


/* ----------------------------------------------------------------------- *
 *     Determine if there are significant color regions in a page image    *
 * ----------------------------------------------------------------------- */
/*!
 * \brief   pixFindColorRegions()
 *
 * \param[in]    pixs         32 bpp rgb
 * \param[in]    pixm         [optional] 1 bpp mask image
 * \param[in]    factor       subsample factor; integer >= 1
 * \param[in]    lightthresh  threshold for component average in lightest
 *                            of 10 buckets; typ. 210; -1 for default
 * \param[in]    darkthresh   threshold to eliminate dark pixels (e.g., text)
 *                            from consideration; typ. 70; -1 for default.
 * \param[in]    mindiff      minimum difference (b - r) and (g - r), used to
 *                            find blue or green pixels; typ. 10; -1 for default
 * \param[in]    colordiff    minimum difference in (max - min) component to
 *                            qualify as a color pixel; typ. 90; -1 for default
 * \param[in]    edgefract    fraction of image half-width and half-height
 *                            for which color pixels are ignored; typ. 0.05.
 * \param[out]   pcolorfract  fraction of 'color' pixels found
 * \param[out]   pcolormask1  [optional] mask over background color, if any
 * \param[out]   pcolormask2  [optional] filtered mask over background color
 * \param[out]   pixadb       [optional] debug intermediate results
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function tries to determine if there is a significant
 *          color or darker region on a scanned page image, where part
 *          of the image is background that is either white or reddish.
 *          This also allows extraction of regions of colored pixels that
 *          have a smaller red component than blue or green components.
 *      (2) If %pixm exists, pixels under its fg are combined with
 *          dark pixels to make a mask of pixels not to be considered
 *          as color candidates.
 *      (3) There are four thresholds.
 *          * %lightthresh: compute the average value of each rgb pixel,
 *            and make 10 buckets by value.  If the lightest bucket gray
 *            value is below %lightthresh, the image is not considered
 *            to have a light bg, and this returns 0.0 for %colorfract.
 *          * %darkthresh: ignore pixels darker than this (typ. fg text).
 *            We make a 1 bpp mask of these pixels, and then dilate it to
 *            remove all vestiges of fg from their vicinity.
 *          * %mindiff: consider pixels with either (b - r) or (g - r)
 *            being at least this value, as having color.
 *          * %colordiff: consider pixels where the (max - min) difference
 *            of the pixel components exceeds this value, as having color.
 *      (4) All components of color pixels that are touching the image
 *          border are removed.  Additionally, all pixels within some
 *          normalized distance %edgefract from the image border can
 *          be removed.  This insures that dark pixels near the edge
 *          of the image are not included.
 *      (5) This returns in %pcolorfract the fraction of pixels that have
 *          color and are not in the set consisting of an OR between
 *          %pixm and the dilated dark pixel mask.
 *      (6) No masks are returned unless light color pixels are found.
 *          If colorfract > 0.0 and %pcolormask1 is defined, this returns
 *          a 1 bpp mask with fg pixels over the color background.
 *          This mask may have some holes in it.
 *      (7) If colorfract > 0.0 and %pcolormask2 is defined, this returns
 *          a version of colormask1 where small holes have been filled.
 *      (8) To generate a boxa of rectangular regions from the overlap
 *          of components in the filtered mask:
 *                boxa1 = pixConnCompBB(colormask2, 8);
 *                boxa2 = boxaCombineOverlaps(boxa1, NULL);
 *          This is done here in debug mode.
 * </pre>
 */
l_ok
pixFindColorRegions(PIX        *pixs,
                    PIX        *pixm,
                    l_int32     factor,
                    l_int32     lightthresh,
                    l_int32     darkthresh,
                    l_int32     mindiff,
                    l_int32     colordiff,
                    l_float32   edgefract,
                    l_float32  *pcolorfract,
                    PIX       **pcolormask1,
                    PIX       **pcolormask2,
                    PIXA       *pixadb)
{
l_int32    w, h, count, rval, gval, bval, aveval, proceed;
l_float32  ratio;
l_uint32  *carray;
BOXA      *boxa1, *boxa2;
PIX       *pix1, *pix2, *pix3, *pix4, *pix5, *pixm1, *pixm2, *pixm3;

    if (pcolormask1) *pcolormask1 = NULL;
    if (pcolormask2) *pcolormask2 = NULL;
    if (!pcolorfract)
        return ERROR_INT("&colorfract not defined", __func__, 1);
    *pcolorfract = 0.0;
    if (!pixs || pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs not defined or not 32 bpp", __func__, 1);
    if (factor < 1) factor = 1;
    if (lightthresh < 0) lightthresh = 210;  /* defaults */
    if (darkthresh < 0) darkthresh = 70;
    if (mindiff < 0) mindiff = 10;
    if (colordiff < 0) colordiff = 90;
    if (edgefract < 0.0 || edgefract > 1.0) edgefract = 0.05f;

        /* Check if pixm covers most of the image.  If so, just return. */
    if (pixm) {
        pixForegroundFraction(pixm, &ratio);
        if (ratio > 0.7) {
            if (pixadb) L_INFO("pixm has big fg: %f5.2\n", __func__, ratio);
            return 0;
        }
    }

        /* Get the light background color.  Use the average component value
         * and select the lightest of 10 buckets.  Require that it is
         * reddish and, using lightthresh, not too dark. */
    pixGetRankColorArray(pixs, 10, L_SELECT_AVERAGE, factor, &carray, NULL, 0);
    if (!carray)
        return ERROR_INT("rank color array not made", __func__, 1);
    extractRGBValues(carray[9], &rval, &gval, &bval);
    if (pixadb) L_INFO("lightest background color: (r,g,b) = (%d,%d,%d)\n",
                       __func__, rval, gval, bval);
    proceed = TRUE;
    if ((rval < bval - 2) || (rval < gval - 2)) {
        if (pixadb) L_INFO("background not reddish\n", __func__);
        proceed = FALSE;
    }
    aveval = (rval + gval + bval) / 3;
    if (aveval < lightthresh) {
        if (pixadb) L_INFO("background too dark\n", __func__);
        proceed = FALSE;
    }
    if (pixadb) {
        pix1 = pixDisplayColorArray(carray, 10, 120, 3, 6);
        pixaAddPix(pixadb, pix1, L_INSERT);
    }
    LEPT_FREE(carray);
    if (proceed == FALSE) return 0;

        /* Make a mask pixm1 over the dark pixels in the image:
         * convert to gray using the average of the components;
         * threshold using darkthresh; do a small dilation;
         * combine with pixm. */
    pix1 = pixConvertRGBToGray(pixs, 0.33f, 0.34f, 0.33f);
    if (pixadb) pixaAddPix(pixadb, pix1, L_COPY);
    pixm1 = pixThresholdToBinary(pix1, darkthresh);
    pixDilateBrick(pixm1, pixm1, 7, 7);
    if (pixadb) pixaAddPix(pixadb, pixm1, L_COPY);
    if (pixm) {
        pixOr(pixm1, pixm1, pixm);
        if (pixadb) pixaAddPix(pixadb, pixm1, L_COPY);
    }
    pixDestroy(&pix1);

        /* Make masks over pixels that are bluish, or greenish, or
           have a very large color saturation (max - min) value. */
    pixm2 = pixConvertRGBToBinaryArb(pixs, -1.0, 0.0, 1.0, mindiff,
                                     L_SELECT_IF_GTE);  /* b - r */
    if (pixadb) pixaAddPix(pixadb, pixm2, L_COPY);
    pix1 = pixConvertRGBToBinaryArb(pixs, -1.0, 1.0, 0.0, mindiff,
                                    L_SELECT_IF_GTE);  /* g - r */
    if (pixadb) pixaAddPix(pixadb, pix1, L_COPY);
    pixOr(pixm2, pixm2, pix1);
    pixDestroy(&pix1);
    pix1 = pixConvertRGBToGrayMinMax(pixs, L_CHOOSE_MAXDIFF);
    pix2 = pixThresholdToBinary(pix1, colordiff);
    pixInvert(pix2, pix2);
    if (pixadb) pixaAddPix(pixadb, pix2, L_COPY);
    pixOr(pixm2, pixm2, pix2);
    if (pixadb) pixaAddPix(pixadb, pixm2, L_COPY);
    pixDestroy(&pix1);
    pixDestroy(&pix2);

        /* Subtract the dark pixels represented by pixm1.
         * pixm2 now holds all the color pixels of interest  */
    pixSubtract(pixm2, pixm2, pixm1);
    pixDestroy(&pixm1);
    if (pixadb) pixaAddPix(pixadb, pixm2, L_COPY);

        /* But we're not quite finished.  Remove pixels from any component
         * that is touching the image border.  False color pixels can
         * sometimes be found there if the image is much darker near
         * the border, due to oxidation or reduced illumination.  Also
         * remove any pixels within the normalized fraction %distfract
         * of the image border. */
    pixm3 = pixRemoveBorderConnComps(pixm2, 8);
    pixGetDimensions(pixm3, &w, &h, NULL);
    pixDestroy(&pixm2);
    if (edgefract > 0.0) {
        pix2 = pixMakeSymmetricMask(w, h, edgefract, edgefract, L_USE_INNER);
        pixAnd(pixm3, pixm3, pix2);
        pixDestroy(&pix2);
    }
    if (pixadb) pixaAddPix(pixadb, pixm3, L_COPY);

        /* Get the fraction of light color pixels */
    pixCountPixels(pixm3, &count, NULL);
    *pcolorfract = (l_float32)count / ((l_float32)(w) * h);
    if (pixadb) {
        if (count == 0)
            L_INFO("no light color pixels found\n", __func__);
        else
            L_INFO("fraction of light color pixels = %5.3f\n", __func__,
                   *pcolorfract);
    }

        /* Debug: extract the color pixels from pixs */
    if (pixadb && count > 0) {
            /* Use pixm3 to extract the color pixels */
        pix3 = pixCreateTemplate(pixs);
        pixSetAll(pix3);
        pixCombineMasked(pix3, pixs, pixm3);
        pixaAddPix(pixadb, pix3, L_INSERT);

            /* Use additional filtering to extract the color pixels */
        pix3 = pixCloseSafeBrick(NULL, pixm3, 15, 15);
        pixaAddPix(pixadb, pix3, L_INSERT);
        pix5 = pixCreateTemplate(pixs);
        pixSetAll(pix5);
        pixCombineMasked(pix5, pixs, pix3);
        pixaAddPix(pixadb, pix5, L_INSERT);

            /* Get the combined bounding boxes of the mask components
             * in pix3, and extract those pixels from pixs. */
        boxa1 = pixConnCompBB(pix3, 8);
        boxa2 = boxaCombineOverlaps(boxa1, NULL);
        pix4 = pixCreateTemplate(pix3);
        pixMaskBoxa(pix4, pix4, boxa2, L_SET_PIXELS);
        pixaAddPix(pixadb, pix4, L_INSERT);
        pix5 = pixCreateTemplate(pixs);
        pixSetAll(pix5);
        pixCombineMasked(pix5, pixs, pix4);
        pixaAddPix(pixadb, pix5, L_INSERT);
        boxaDestroy(&boxa1);
        boxaDestroy(&boxa2);
    }
    pixaAddPix(pixadb, pixs, L_COPY);

        /* Optional colormask returns */
    if (pcolormask2 && count > 0)
        *pcolormask2 = pixCloseSafeBrick(NULL, pixm3, 15, 15);
    if (pcolormask1 && count > 0)
        *pcolormask1 = pixm3;
    else
        pixDestroy(&pixm3);
    return 0;
}


/* ----------------------------------------------------------------------- *
 *      Find the number of perceptually significant gray intensities       *
 *      in a grayscale image.                                              *
 * ----------------------------------------------------------------------- */
/*!
 * \brief   pixNumSignificantGrayColors()
 *
 * \param[in]    pixs          8 bpp gray
 * \param[in]    darkthresh    dark threshold for minimum intensity to be
 *                             considered; typ. 20
 * \param[in]    lightthresh   threshold near white, for maximum intensity
 *                             to be considered; typ. 236
 * \param[in]    minfract      minimum fraction of all pixels to include a level
 *                             as significant; typ. 0.0001; should be < 0.001
 * \param[in]    factor        subsample factor; integer >= 1
 * \param[out]   pncolors      number of significant colors; 0 on error
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function is asking the question: how many perceptually
 *          significant gray color levels is in this pix?
 *          A color level must meet 3 criteria to be significant:
 *            ~ it can't be too close to black
 *            ~ it can't be too close to white
 *            ~ it must have at least some minimum fractional population
 *      (2) Use -1 for default values for darkthresh, lightthresh and minfract.
 *      (3) Choose default of darkthresh = 20, because variations in very
 *          dark pixels are not visually significant.
 *      (4) Choose default of lightthresh = 236, because document images
 *          that have been jpeg'd typically have near-white pixels in the
 *          8x8 jpeg blocks, and these should not be counted.  It is desirable
 *          to obtain a clean image by quantizing this noise away.
 * </pre>
 */
l_ok
pixNumSignificantGrayColors(PIX       *pixs,
                            l_int32    darkthresh,
                            l_int32    lightthresh,
                            l_float32  minfract,
                            l_int32    factor,
                            l_int32   *pncolors)
{
l_int32  i, w, h, count, mincount, ncolors;
NUMA    *na;

    if (!pncolors)
        return ERROR_INT("&ncolors not defined", __func__, 1);
    *pncolors = 0;
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8 bpp", __func__, 1);
    if (darkthresh < 0) darkthresh = 20;  /* defaults */
    if (lightthresh < 0) lightthresh = 236;
    if (minfract < 0.0)
		minfract = 0.0001f;
    if (minfract > 1.0)
        return ERROR_INT("minfract > 1.0", __func__, 1);
    if (minfract >= 0.001)
        L_WARNING("minfract too big; likely to underestimate ncolors\n",
                  __func__);
    if (lightthresh > 255 || darkthresh >= lightthresh)
        return ERROR_INT("invalid thresholds", __func__, 1);
    if (factor < 1) factor = 1;

    pixGetDimensions(pixs, &w, &h, NULL);
    mincount = (l_int32)(minfract * w * h * factor * factor);
    if ((na = pixGetGrayHistogram(pixs, factor)) == NULL)
        return ERROR_INT("na not made", __func__, 1);
    ncolors = 2;  /* add in black and white */
    for (i = darkthresh; i <= lightthresh; i++) {
        numaGetIValue(na, i, &count);
        if (count >= mincount)
            ncolors++;
    }

    *pncolors = ncolors;
    numaDestroy(&na);
    return 0;
}


/* ----------------------------------------------------------------------- *
 *   Identifies images where color quantization will cause posterization   *
 *   due to the existence of many colors in low-gradient regions.          *
 * ----------------------------------------------------------------------- */
/*!
 * \brief   pixColorsForQuantization()
 * \param[in]    pixs 8     bpp gray or 32 bpp rgb; with or without colormap
 * \param[in]    thresh     binary threshold on edge gradient; 0 for default
 * \param[out]   pncolors   the number of colors found
 * \param[out]   piscolor   [optional] 1 if significant color is found;
 *                          0 otherwise.  If pixs is 8 bpp, and does not have
 *                          a colormap with color entries, this is 0
 * \param[in]    debug      1 to output masked image that is tested for colors;
 *                          0 otherwise
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) This function finds a measure of the number of colors that are
 *          found in low-gradient regions of an image.  By its
 *          magnitude relative to some threshold (not specified in
 *          this function), it gives a good indication of whether
 *          quantization will generate posterization.   This number
 *          is larger for images with regions of slowly varying
 *          intensity (if 8 bpp) or color (if rgb). Such images, if
 *          quantized, may require dithering to avoid posterization,
 *          and lossless compression is then expected to be poor.
 *      (2) If pixs has a colormap, the number of colors returned is
 *          the number in the colormap.
 *      (3) It is recommended that document images be reduced to a width
 *          of 800 pixels before applying this function.  Then it can
 *          be expected that color detection will be fairly accurate
 *          and the number of colors will reflect both the content and
 *          the type of compression to be used.  For less than 15 colors,
 *          there is unlikely to be a halftone image, and lossless
 *          quantization should give both a good visual result and
 *          better compression.
 *      (4) When using the default threshold on the gradient (15),
 *          images (both gray and rgb) where ncolors is greater than
 *          about 15 will compress poorly with either lossless
 *          compression or dithered quantization, and they may be
 *          posterized with non-dithered quantization.
 *      (5) For grayscale images, or images without significant color,
 *          this returns the number of significant gray levels in
 *          the low-gradient regions.  The actual number of gray levels
 *          can be large due to jpeg compression noise in the background.
 *      (6) Similarly, for color images, the actual number of different
 *          (r,g,b) colors in the low-gradient regions (rather than the
 *          number of occupied level 4 octcubes) can be quite large, e.g.,
 *          due to jpeg compression noise, even for regions that appear
 *          to be of a single color.  By quantizing to level 4 octcubes,
 *          most of these superfluous colors are removed from the counting.
 *      (7) The image is tested for color.  If there is very little color,
 *          it is thresholded to gray and the number of gray levels in
 *          the low gradient regions is found.  If the image has color,
 *          the number of occupied level 4 octcubes is found.
 *      (8) The number of colors in the low-gradient regions increases
 *          monotonically with the threshold %thresh on the edge gradient.
 *      (9) Background: grayscale and color quantization is often useful
 *          to achieve highly compressed images with little visible
 *          distortion.  However, gray or color washes (regions of
 *          low gradient) can defeat this approach to high compression.
 *          How can one determine if an image is expected to compress
 *          well using gray or color quantization?  We use the fact that
 *            * gray washes, when quantized with less than 50 intensities,
 *              have posterization (visible boundaries between regions
 *              of uniform 'color') and poor lossless compression
 *            * color washes, when quantized with level 4 octcubes,
 *              typically result in both posterization and the occupancy
 *              of many level 4 octcubes.
 *          Images can have colors either intrinsically or as jpeg
 *          compression artifacts.  This function reduces but does not
 *          completely eliminate measurement of jpeg quantization noise
 *          in the white background of grayscale or color images.
 * </pre>
 */
l_ok
pixColorsForQuantization(PIX      *pixs,
                         l_int32   thresh,
                         l_int32  *pncolors,
                         l_int32  *piscolor,
                         l_int32   debug)
{
l_int32    w, h, d, minside, factor;
l_float32  pixfract, colorfract;
PIX       *pixt, *pixsc, *pixg, *pixe, *pixb, *pixm;
PIXCMAP   *cmap;

    if (piscolor) *piscolor = 0;
    if (!pncolors)
        return ERROR_INT("&ncolors not defined", __func__, 1);
    *pncolors = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if ((cmap = pixGetColormap(pixs)) != NULL) {
        *pncolors = pixcmapGetCount(cmap);
        if (piscolor)
            pixcmapHasColor(cmap, piscolor);
        return 0;
    }

    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8 && d != 32)
        return ERROR_INT("pixs not 8 or 32 bpp", __func__, 1);
    if (thresh <= 0)
        thresh = 15;

        /* First test if 32 bpp has any significant color; if not,
         * convert it to gray.  Colors whose average values are within
         * 20 of black or 8 of white are ignored because they're not
         * very 'colorful'.  If less than 2.5/10000 of the pixels have
         * significant color, consider the image to be gray. */
    minside = L_MIN(w, h);
    if (d == 8) {
        pixt = pixClone(pixs);
    } else {  /* d == 32 */
        factor = L_MAX(1, minside / 400);
        pixColorFraction(pixs, 20, 248, 30, factor, &pixfract, &colorfract);
        if (pixfract * colorfract < 0.00025) {
            pixt = pixGetRGBComponent(pixs, COLOR_RED);
            d = 8;
        } else {  /* d == 32 */
            pixt = pixClone(pixs);
            if (piscolor)
                *piscolor = 1;
        }
    }

        /* If the smallest side is less than 1000, do not downscale.
         * If it is in [1000 ... 2000), downscale by 2x.  If it is >= 2000,
         * downscale by 4x.  Factors of 2 are chosen for speed.  The
         * actual resolution at which subsequent calculations take place
         * is not strongly dependent on downscaling.  */
    factor = L_MAX(1, minside / 500);
    if (factor == 1)
        pixsc = pixCopy(NULL, pixt);  /* to be sure pixs is unchanged */
    else if (factor == 2 || factor == 3)
        pixsc = pixScaleAreaMap2(pixt);
    else
        pixsc = pixScaleAreaMap(pixt, 0.25, 0.25);

        /* Basic edge mask generation procedure:
         *   ~ work on a grayscale image
         *   ~ get a 1 bpp edge mask by using an edge filter and
         *     thresholding to get fg pixels at the edges
         *   ~ for gray, dilate with a 3x3 brick Sel to get mask over
         *     all pixels within a distance of 1 pixel from the nearest
         *     edge pixel
         *   ~ for color, dilate with a 7x7 brick Sel to get mask over
         *     all pixels within a distance of 3 pixels from the nearest
         *     edge pixel  */
    if (d == 8)
        pixg = pixClone(pixsc);
    else  /* d == 32 */
        pixg = pixConvertRGBToLuminance(pixsc);
    pixe = pixSobelEdgeFilter(pixg, L_ALL_EDGES);
    pixb = pixThresholdToBinary(pixe, thresh);
    pixInvert(pixb, pixb);
    if (d == 8)
        pixm = pixMorphSequence(pixb, "d3.3", 0);
    else
        pixm = pixMorphSequence(pixb, "d7.7", 0);

        /* Mask the near-edge pixels to white, and count the colors.
         * If grayscale, don't count colors within 20 levels of
         * black or white, and only count colors with a fraction
         * of at least 1/10000 of the image pixels.
         * If color, count the number of level 4 octcubes that
         * contain at least 20 pixels.  These magic numbers are guesses
         * as to what might work, based on a small data set.  Results
         * should not be overly sensitive to their actual values. */
    if (d == 8) {
        pixSetMasked(pixg, pixm, 0xff);
        if (debug) pixWrite("junkpix8.png", pixg, IFF_PNG);
        pixNumSignificantGrayColors(pixg, 20, 236, 0.0001f, 1, pncolors);
    } else {  /* d == 32 */
        pixSetMasked(pixsc, pixm, 0xffffffff);
        if (debug) pixWrite("junkpix32.png", pixsc, IFF_PNG);
        pixNumberOccupiedOctcubes(pixsc, 4, 20, -1, pncolors);
    }

    pixDestroy(&pixt);
    pixDestroy(&pixsc);
    pixDestroy(&pixg);
    pixDestroy(&pixe);
    pixDestroy(&pixb);
    pixDestroy(&pixm);
    return 0;
}


/* ----------------------------------------------------------------------- *
 *               Find the number of unique colors in an image              *
 * ----------------------------------------------------------------------- */
/*!
 * \brief   pixNumColors()
 * \param[in]    pixs       2, 4, 8, 32 bpp
 * \param[in]    factor     subsampling factor; integer
 * \param[out]   pncolors   the number of colors found in the pix
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) This returns the number of colors found in the image,
 *          even if there is a colormap.  If %factor == 1 and the
 *          number of colors differs from the number of entries
 *          in the colormap, a warning is issued.
 *      (2) Use %factor == 1 to find the actual number of colors.
 *          Use %factor > 1 to more efficiently find an approximate
 *          number of colors.
 *      (3) For d = 2, 4 or 8 bpp grayscale, this returns the number
 *          of colors found in the image in 'ncolors'.
 *      (4) For d = 32 bpp (rgb), if the number of colors is greater
 *          than 256, this uses a hash set with %factor == 1.
 * </pre>
 */
l_ok
pixNumColors(PIX      *pixs,
             l_int32   factor,
             l_int32  *pncolors)
{
l_int32    w, h, d, i, j, wpl, hashsize, sum, count, manycolors;
l_int32    rval, gval, bval, val;
l_int32   *inta;
l_uint32   pixel;
l_uint32  *data, *line;
PIXCMAP   *cmap;

    if (!pncolors)
        return ERROR_INT("&ncolors not defined", __func__, 1);
    *pncolors = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 2 && d != 4 && d != 8 && d != 32)
        return ERROR_INT("d not in {2, 4, 8, 32}", __func__, 1);
    if (factor < 1) factor = 1;

    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    sum = 0;
    if (d != 32) {  /* grayscale */
        inta = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
        for (i = 0; i < h; i += factor) {
            line = data + i * wpl;
            for (j = 0; j < w; j += factor) {
                if (d == 8)
                    val = GET_DATA_BYTE(line, j);
                else if (d == 4)
                    val = GET_DATA_QBIT(line, j);
                else  /* d == 2 */
                    val = GET_DATA_DIBIT(line, j);
                inta[val] = 1;
            }
        }
        for (i = 0; i < 256; i++)
            if (inta[i]) sum++;
        *pncolors = sum;
        LEPT_FREE(inta);

        cmap = pixGetColormap(pixs);
        if (cmap && factor == 1) {
            count = pixcmapGetCount(cmap);
            if (sum != count)
                L_WARNING("colormap size %d differs from actual colors\n",
                          __func__, count);
        }
        return 0;
    }

        /* 32 bpp rgb; quit if we get above 256 colors */
    hashsize = 5507;  /* big and prime; collisions are not likely */
    inta = (l_int32 *)LEPT_CALLOC(hashsize, sizeof(l_int32));
    manycolors = 0;
    for (i = 0; i < h && manycolors == 0; i += factor) {
        line = data + i * wpl;
        for (j = 0; j < w; j += factor) {
            pixel = line[j];
            extractRGBValues(pixel, &rval, &gval, &bval);
            val = (137 * rval + 269 * gval + 353 * bval) % hashsize;
            if (inta[val] == 0) {
                inta[val] = 1;
                sum++;
                if (sum > 256) {
                    manycolors = 1;
                    break;
                }
            }
        }
    }
    LEPT_FREE(inta);

    if (manycolors == 0) {
        *pncolors = sum;
        return 0;
    }

        /* More than 256 colors in RGB image; count all the pixels */
    return pixCountRGBColorsByHash(pixs, pncolors);
}


/* ----------------------------------------------------------------------- *
 *             Lossless conversion of RGB image to colormapped             *
 * ----------------------------------------------------------------------- */
/*!
 * \brief   pixConvertRGBToCmapLossless()
 * \param[in]    pixs     32 bpp RGB
 * \return  pixd   if num colors <= 256; null otherwise or on error
 *
 * <pre>
 * Notes:
 *      (1) If there are not more than 256 colors, this losslessly
 *          converts and RGB image to a colormapped one, with the
 *          smallest pixel depth required to hold all the colors.
 * </pre>
 */
PIX *
pixConvertRGBToCmapLossless(PIX  *pixs)
{
l_int32    w, h, d, i, j, wpls, wpld, hashsize, hashval, ncolors, index;
l_int32    rval, gval, bval, val;
l_int32   *hasha1, *hasha2;
l_uint32   pixel;
l_uint32  *datas, *lines, *datad, *lined;
PIX       *pixd;
PIXCMAP   *cmap;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);

    pixNumColors(pixs, 1, &ncolors);
    if (ncolors > 256) {
        L_ERROR("too many colors found: %d\n", __func__, ncolors);
        return NULL;
    }

    pixGetDimensions(pixs, &w, &h, NULL);
    if (ncolors <= 2)
        d = 1;
    else if (ncolors <= 4)
        d = 2;
    else if (ncolors <= 16)
        d = 4;
    else  /* ncolors <= 256 */
        d = 8;

    if ((pixd = pixCreate(w, h, d)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    cmap = pixcmapCreate(d);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

        /* hasha1 is a 1/0 indicator array for colors seen.
           hasha2 holds the index into the colormap that will be
           generated from the colors in the order seen. This is
           the value inserted into pixd.  */
    hashsize = 5507;  /* big and prime; collisions are not likely */
    hasha1 = (l_int32 *)LEPT_CALLOC(hashsize, sizeof(l_int32));
    hasha2 = (l_int32 *)LEPT_CALLOC(hashsize, sizeof(l_int32));
    index = -1;
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            pixel = lines[j];
            extractRGBValues(pixel, &rval, &gval, &bval);
            hashval = (137 * rval + 269 * gval + 353 * bval) % hashsize;
            if (hasha1[hashval] == 0) {  /* new color */
                hasha1[hashval] = 1;
                index++;
                hasha2[hashval] = index;
                pixcmapAddColor(cmap, rval, gval, bval);
            }
            val = hasha2[hashval];
            setLineDataVal(lined, j, d, val);
        }
    }
    pixSetColormap(pixd, cmap);

    LEPT_FREE(hasha1);
    LEPT_FREE(hasha2);
    return pixd;
}


/* ----------------------------------------------------------------------- *
 *       Find the most "populated" colors in the image (and quantize)      *
 * ----------------------------------------------------------------------- */
/*!
 * \brief   pixGetMostPopulatedColors()
 * \param[in]    pixs 32   bpp rgb
 * \param[in]    sigbits   2-6, significant bits retained in the quantizer
 *                         for each component of the input image
 * \param[in]    factor    subsampling factor; use 1 for no subsampling
 * \param[in]    ncolors   the number of most populated colors to select
 * \param[out]   parray    [optional] array of colors, each as 0xrrggbb00
 * \param[out]   pcmap     [optional] colormap of the colors
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This finds the %ncolors most populated cubes in rgb colorspace,
 *          where the cube size depends on %sigbits as
 *               cube side = (256 >> sigbits)
 *      (2) The rgb color components are found at the center of the cube.
 *      (3) The output array of colors can be displayed using
 *               pixDisplayColorArray(array, ncolors, ...);
 * </pre>
 */
l_ok
pixGetMostPopulatedColors(PIX        *pixs,
                          l_int32     sigbits,
                          l_int32     factor,
                          l_int32     ncolors,
                          l_uint32  **parray,
                          PIXCMAP   **pcmap)
{
l_int32  n, i, rgbindex, rval, gval, bval;
NUMA    *nahisto, *naindex;

    if (!parray && !pcmap)
        return ERROR_INT("no return val requested", __func__, 1);
    if (parray) *parray = NULL;
    if (pcmap) *pcmap = NULL;
    if (!pixs || pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (sigbits < 2 || sigbits > 6)
        return ERROR_INT("sigbits not in [2 ... 6]", __func__, 1);
    if (factor < 1 || ncolors < 1)
        return ERROR_INT("factor < 1 or ncolors < 1", __func__, 1);

    if ((nahisto = pixGetRGBHistogram(pixs, sigbits, factor)) == NULL)
        return ERROR_INT("nahisto not made", __func__, 1);

        /* naindex contains the index into nahisto, which is the rgbindex */
    naindex = numaSortIndexAutoSelect(nahisto, L_SORT_DECREASING);
    numaDestroy(&nahisto);
    if (!naindex)
        return ERROR_INT("naindex not made", __func__, 1);

    n = numaGetCount(naindex);
    ncolors = L_MIN(n, ncolors);
    if (parray) *parray = (l_uint32 *)LEPT_CALLOC(ncolors, sizeof(l_uint32));
    if (pcmap) *pcmap = pixcmapCreate(8);
    for (i = 0; i < ncolors; i++) {
        numaGetIValue(naindex, i, &rgbindex);  /* rgb index */
        getRGBFromIndex(rgbindex, sigbits, &rval, &gval, &bval);
        if (parray) composeRGBPixel(rval, gval, bval, *parray + i);
        if (pcmap) pixcmapAddColor(*pcmap, rval, gval, bval);
    }

    numaDestroy(&naindex);
    return 0;
}


/*!
 * \brief   pixSimpleColorQuantize()
 * \param[in]    pixs      32 bpp rgb
 * \param[in]    sigbits   2-4, significant bits retained in the quantizer
 *                         for each component of the input image
 * \param[in]    factor    subsampling factor; use 1 for no subsampling
 * \param[in]    ncolors   the number of most populated colors to select
 * \return  pixd 8 bpp cmapped or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If you want to do color quantization for real, use octcube
 *          or modified median cut.  This function shows that it is
 *          easy to make a simple quantizer based solely on the population
 *          in cells of a given size in rgb color space.
 *      (2) The %ncolors most populated cells at the %sigbits level form
 *          the colormap for quantizing, and this uses octcube indexing
 *          under the covers to assign each pixel to the nearest color.
 *      (3) %sigbits is restricted to 2, 3 and 4.  At the low end, the
 *          color discrimination is very crude; at the upper end, a set of
 *          similar colors can dominate the result.  Interesting results
 *          are generally found for %sigbits = 3 and ncolors ~ 20.
 *      (4) See also pixColorSegment() for a method of quantizing the
 *          colors to generate regions of similar color.
 *      (5) See also pixConvertRGBToCmapLossless() to losslessly convert
 *          an RGB image with not more than 256 colors.
 * </pre>
 */
PIX *
pixSimpleColorQuantize(PIX        *pixs,
                       l_int32     sigbits,
                       l_int32     factor,
                       l_int32     ncolors)
{
l_int32   w, h;
PIX      *pixd;
PIXCMAP  *cmap;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (sigbits < 2 || sigbits > 4)
        return (PIX *)ERROR_PTR("sigbits not in {2,3,4}", __func__, NULL);

    pixGetMostPopulatedColors(pixs, sigbits, factor, ncolors, NULL, &cmap);
    pixGetDimensions(pixs, &w, &h, NULL);
    pixd = pixCreate(w, h, 8);
    pixSetColormap(pixd, cmap);
    pixAssignToNearestColor(pixd, pixs, NULL, 4, NULL);
    return pixd;
}


/* ----------------------------------------------------------------------- *
 *            Constructs a color histogram based on rgb indices            *
 * ----------------------------------------------------------------------- */
/*!
 * \brief   pixGetRGBHistogram()
 * \param[in]    pixs      32 bpp rgb
 * \param[in]    sigbits   2-6, significant bits retained in the quantizer
 *                         for each component of the input image
 * \param[in]    factor    subsampling factor; use 1 for no subsampling
 * \return  numa histogram of colors, indexed by RGB
 *                    components, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This uses a simple, fast method of indexing into an rgb image.
 *      (2) The output is a 1D histogram of count vs. rgb-index, which
 *          uses red sigbits as the most significant and blue as the least.
 *      (3) This function produces the same result as pixMedianCutHisto().
 * </pre>
 */
NUMA *
pixGetRGBHistogram(PIX     *pixs,
                   l_int32  sigbits,
                   l_int32  factor)
{
l_int32     w, h, i, j, size, wpl, rval, gval, bval, npts;
l_uint32    val32, rgbindex;
l_float32  *array;
l_uint32   *data, *line, *rtab, *gtab, *btab;
NUMA       *na;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (NUMA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (sigbits < 2 || sigbits > 6)
        return (NUMA *)ERROR_PTR("sigbits not in [2 ... 6]", __func__, NULL);
    if (factor < 1)
        return (NUMA *)ERROR_PTR("factor < 1", __func__, NULL);

        /* Get histogram size: 2^(3 * sigbits) */
    size = 1 << (3 * sigbits);  /* 64, 512, 4096, 32768, 262144 */
    na = numaMakeConstant(0, size);  /* init to all 0 */
    array = numaGetFArray(na, L_NOCOPY);

    makeRGBIndexTables(&rtab, &gtab, &btab, sigbits);

        /* Check the number of sampled pixels */
    pixGetDimensions(pixs, &w, &h, NULL);
    npts = ((w + factor - 1) / factor) * ((h + factor - 1) / factor);
    if (npts < 1000)
        L_WARNING("only sampling %d pixels\n", __func__, npts);
    wpl = pixGetWpl(pixs);
    data = pixGetData(pixs);
    for (i = 0; i < h; i += factor) {
        line = data + i * wpl;
        for (j = 0; j < w; j += factor) {
            val32 = *(line + j);
            extractRGBValues(val32, &rval, &gval, &bval);
            rgbindex = rtab[rval] | gtab[gval] | btab[bval];
            array[rgbindex]++;
        }
    }

    LEPT_FREE(rtab);
    LEPT_FREE(gtab);
    LEPT_FREE(btab);
    return na;
}


/*!
 * \brief   makeRGBIndexTables()
 *
 * \param[out]   prtab, pgtab, pbtab    256-entry rgb index tables
 * \param[in]    sigbits   2-6, significant bits retained in the quantizer
 *                         for each component of the input image
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) These tables are used to map from rgb sample values to
 *          an rgb index, using
 *             rgbindex = rtab[rval] | gtab[gval] | btab[bval]
 *          where, e.g., if sigbits = 3, the index is a 9 bit integer:
 *             r7 r6 r5 g7 g6 g5 b7 b6 b5
 * </pre>
 */
l_ok
makeRGBIndexTables(l_uint32  **prtab,
                   l_uint32  **pgtab,
                   l_uint32  **pbtab,
                   l_int32     sigbits)
{
l_int32    i;
l_uint32  *rtab, *gtab, *btab;

    if (prtab) *prtab = NULL;
    if (pgtab) *pgtab = NULL;
    if (pbtab) *pbtab = NULL;
    if (!prtab || !pgtab || !pbtab)
        return ERROR_INT("not all table ptrs defined", __func__, 1);
    if (sigbits < 2 || sigbits > 6)
        return ERROR_INT("sigbits not in [2 ... 6]", __func__, 1);

    rtab = (l_uint32 *)LEPT_CALLOC(256, sizeof(l_uint32));
    gtab = (l_uint32 *)LEPT_CALLOC(256, sizeof(l_uint32));
    btab = (l_uint32 *)LEPT_CALLOC(256, sizeof(l_uint32));
    if (!rtab || !gtab || !btab) {
        LEPT_FREE(rtab);
        LEPT_FREE(gtab);
        LEPT_FREE(btab);
        return ERROR_INT("calloc fail for tab", __func__, 1);
    }
    *prtab = rtab;
    *pgtab = gtab;
    *pbtab = btab;
    switch (sigbits) {
    case 2:
        for (i = 0; i < 256; i++) {
            rtab[i] = (i & 0xc0) >> 2;
            gtab[i] = (i & 0xc0) >> 4;
            btab[i] = (i & 0xc0) >> 6;
        }
        break;
    case 3:
        for (i = 0; i < 256; i++) {
            rtab[i] = (i & 0xe0) << 1;
            gtab[i] = (i & 0xe0) >> 2;
            btab[i] = (i & 0xe0) >> 5;
        }
        break;
    case 4:
        for (i = 0; i < 256; i++) {
            rtab[i] = (i & 0xf0) << 4;
            gtab[i] = (i & 0xf0);
            btab[i] = (i & 0xf0) >> 4;
        }
        break;
    case 5:
        for (i = 0; i < 256; i++) {
          rtab[i] = (i & 0xf8) << 7;
          gtab[i] = (i & 0xf8) << 2;
          btab[i] = (i & 0xf8) >> 3;
        }
        break;
    case 6:
        for (i = 0; i < 256; i++) {
          rtab[i] = (i & 0xfc) << 10;
          gtab[i] = (i & 0xfc) << 4;
          btab[i] = (i & 0xfc) >> 2;
        }
        break;
    default:
        L_ERROR("Illegal sigbits = %d\n", __func__, sigbits);
        return ERROR_INT("sigbits not in [2 ... 6]", __func__, 1);
    }

    return 0;
}


/*!
 * \brief   getRGBFromIndex()
 *
 * \param[in]    index      rgbindex
 * \param[in]    sigbits    2-6, significant bits retained in the quantizer
 *                          for each component of the input image
 * \param[out]   prval, pgval, pbval   rgb values
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The %index is expressed in bits, based on the the
 *          %sigbits of the r, g and b components, as
 *             r7 r6 ... g7 g6 ... b7 b6 ...
 *      (2) The computed rgb values are in the center of the quantized cube.
 *          The extra bit that is OR'd accomplishes this.
 * </pre>
 */
l_ok
getRGBFromIndex(l_uint32  index,
                l_int32   sigbits,
                l_int32  *prval,
                l_int32  *pgval,
                l_int32  *pbval)
{
    if (prval) *prval = 0;
    if (pgval) *pgval = 0;
    if (pbval) *pbval = 0;
    if (!prval || !pgval || !pbval)
        return ERROR_INT("not all component ptrs defined", __func__, 1);
    if (sigbits < 2 || sigbits > 6)
        return ERROR_INT("sigbits not in [2 ... 6]", __func__, 1);

    switch (sigbits) {
    case 2:
        *prval = ((index << 2) & 0xc0) | 0x20;
        *pgval = ((index << 4) & 0xc0) | 0x20;
        *pbval = ((index << 6) & 0xc0) | 0x20;
        break;
    case 3:
        *prval = ((index >> 1) & 0xe0) | 0x10;
        *pgval = ((index << 2) & 0xe0) | 0x10;
        *pbval = ((index << 5) & 0xe0) | 0x10;
        break;
    case 4:
        *prval = ((index >> 4) & 0xf0) | 0x08;
        *pgval = (index & 0xf0) | 0x08;
        *pbval = ((index << 4) & 0xf0) | 0x08;
        break;
    case 5:
        *prval = ((index >> 7) & 0xf8) | 0x04;
        *pgval = ((index >> 2) & 0xf8) | 0x04;
        *pbval = ((index << 3) & 0xf8) | 0x04;
        break;
    case 6:
        *prval = ((index >> 10) & 0xfc) | 0x02;
        *pgval = ((index >> 4) & 0xfc) | 0x02;
        *pbval = ((index << 2) & 0xfc) | 0x02;
        break;
    default:
        L_ERROR("Illegal sigbits = %d\n", __func__, sigbits);
        return ERROR_INT("sigbits not in [2 ... 6]", __func__, 1);
    }

    return 0;
}


/* ----------------------------------------------------------------------- *
 *             Identify images that have highlight (red) color             *
 * ----------------------------------------------------------------------- */
/*!
 * \brief   pixHasHighlightRed()
 *
 * \param[in]    pixs      32 bpp rgb
 * \param[in]    factor    subsampling; an integer >= 1; use 1 for all pixels
 * \param[in]    minfract  threshold fraction of all image pixels; must be > 0.0
 * \param[in]    fthresh   threshold on a function of the components; typ. ~2.5
 * \param[out]   phasred   1 if red pixels are above threshold
 * \param[out]   pratio    [optional] normalized fraction of threshold
 *                         red pixels that is actually observed
 * \param[out]   ppixdb    [optional] seed pixel mask
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Pixels are identified as red if they satisfy two conditions:
 *          (a) The components satisfy (R-B)/B > %fthresh   (red or dark fg)
 *          (b) The red component satisfied R > 128  (red or light bg)
 *          Masks are generated for (a) and (b), and the intersection
 *          gives the pixels that are red but not either light bg or
 *          dark fg.
 *      (2) A typical value for minfract = 0.0001, which gives sensitivity
 *          to an image where a small fraction of the pixels are printed
 *          in red.
 *      (3) A typical value for fthresh = 2.5.  Higher values give less
 *          sensitivity to red, and fewer false positives.
 * </pre>
 */
l_ok
pixHasHighlightRed(PIX        *pixs,
                   l_int32     factor,
                   l_float32   minfract,
                   l_float32   fthresh,
                   l_int32    *phasred,
                   l_float32  *pratio,
                   PIX       **ppixdb)
{
l_float32  fract, ratio;
PIX       *pix1, *pix2, *pix3, *pix4;
FPIX      *fpix;

    if (pratio) *pratio = 0.0;
    if (ppixdb) *ppixdb = NULL;
    if (phasred) *phasred = 0;
    if (!pratio && !ppixdb)
        return ERROR_INT("no return val requested", __func__, 1);
    if (!phasred)
        return ERROR_INT("&hasred not defined", __func__, 1);
    if (!pixs || pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs not defined or not 32 bpp", __func__, 1);
    if (minfract <= 0.0)
        return ERROR_INT("minfract must be > 0.0", __func__, 1);
    if (fthresh < 1.5 || fthresh > 3.5)
        L_WARNING("fthresh = %f is out of normal bounds\n", __func__, fthresh);

    if (factor > 1)
        pix1 = pixScaleByIntSampling(pixs, factor);
    else
        pix1 = pixClone(pixs);

        /* Identify pixels that are either red or dark foreground */
    fpix = pixComponentFunction(pix1, 1.0, 0.0, -1.0, 0.0, 0.0, 1.0);
    pix2 = fpixThresholdToPix(fpix, fthresh);
    pixInvert(pix2, pix2);

        /* Identify pixels that are either red or light background */
    pix3 = pixGetRGBComponent(pix1, COLOR_RED);
    pix4 = pixThresholdToBinary(pix3, 130);
    pixInvert(pix4, pix4);

    pixAnd(pix4, pix4, pix2);
    pixForegroundFraction(pix4, &fract);
    ratio = fract / minfract;
    L_INFO("fract = %7.5f, ratio = %7.3f\n", __func__, fract, ratio);
    if (pratio) *pratio = ratio;
    if (ratio >= 1.0)
        *phasred = 1;
    if (ppixdb)
        *ppixdb = pix4;
    else
        pixDestroy(&pix4);
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    pixDestroy(&pix3);
    fpixDestroy(&fpix);
    return 0;
}

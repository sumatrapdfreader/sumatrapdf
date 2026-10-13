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
 * \file scale1.c
 * <pre>
 *         Top-level scaling
 *               PIX      *pixScale()
 *               PIX      *pixScaleToSizeRel()
 *               PIX      *pixScaleToSize()
 *               PIX      *pixScaleToResolution()
 *               PIX      *pixScaleGeneral()
 *
 *         Linearly interpreted (usually up-) scaling
 *               PIX      *pixScaleLI()
 *               PIX      *pixScaleColorLI()
 *               PIX      *pixScaleColor2xLI()
 *               PIX      *pixScaleColor4xLI()
 *               PIX      *pixScaleGrayLI()
 *               PIX      *pixScaleGray2xLI()
 *               PIX      *pixScaleGray4xLI()
 *
 *         Upscale 2x followed by binarization
 *               PIX      *pixScaleGray2xLIThresh()
 *               PIX      *pixScaleGray2xLIDither()
 *
 *         Upscale 4x followed by binarization
 *               PIX      *pixScaleGray4xLIThresh()
 *               PIX      *pixScaleGray4xLIDither()
 *
 *         Scaling by closest pixel sampling
 *               PIX      *pixScaleBySampling()
 *               PIX      *pixScaleBySamplingWithShift()
 *               PIX      *pixScaleBySamplingToSize()
 *               PIX      *pixScaleByIntSampling()
 *
 *         Fast integer factor subsampling RGB to gray and to binary
 *               PIX      *pixScaleRGBToGrayFast()
 *               PIX      *pixScaleRGBToBinaryFast()
 *               PIX      *pixScaleGrayToBinaryFast()
 *
 *         Downscaling with (antialias) smoothing
 *               PIX      *pixScaleSmooth()
 *               PIX      *pixScaleSmoothToSize()
 *               PIX      *pixScaleRGBToGray2()   [special 2x reduction to gray]
 *
 *         Downscaling with (antialias) area mapping
 *               PIX      *pixScaleAreaMap()
 *               PIX      *pixScaleAreaMap2()
 *               PIX      *pixScaleAreaMapToSize()
 *
 *         Binary scaling by closest pixel sampling
 *               PIX      *pixScaleBinary()
 *               PIX      *pixScaleBinaryWithShift()
 *
 *     Low-level static functions:
 *
 *         Color (interpolated) scaling: general case
 *               static void       scaleColorLILow()
 *
 *         Grayscale (interpolated) scaling: general case
 *               static void       scaleGrayLILow()
 *
 *         Color (interpolated) scaling: 2x upscaling
 *               static void       scaleColor2xLILow()
 *               static void       scaleColor2xLILineLow()
 *
 *         Grayscale (interpolated) scaling: 2x upscaling
 *               static void       scaleGray2xLILow()
 *               static void       scaleGray2xLILineLow()
 *
 *         Grayscale (interpolated) scaling: 4x upscaling
 *               static void       scaleGray4xLILow()
 *               static void       scaleGray4xLILineLow()
 *
 *         Grayscale and color scaling by closest pixel sampling
 *               static l_int32    scaleBySamplingLow()
 *
 *         Color and grayscale downsampling with (antialias) lowpass filter
 *               static l_int32    scaleSmoothLow()
 *               static void       scaleRGBToGray2Low()
 *
 *         Color and grayscale downsampling with (antialias) area mapping
 *               static l_int32    scaleColorAreaMapLow()
 *               static l_int32    scaleGrayAreaMapLow()
 *               static l_int32    scaleAreaMapLow2()
 *
 *         Binary scaling by closest pixel sampling
 *               static l_int32    scaleBinaryLow()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

static void scaleColorLILow(l_uint32 *datad, l_int32 wd, l_int32 hd,
                            l_int32 wpld, l_uint32 *datas, l_int32 ws,
                            l_int32 hs, l_int32 wpls);
static void scaleGrayLILow(l_uint32 *datad, l_int32 wd, l_int32 hd,
                           l_int32 wpld, l_uint32 *datas, l_int32 ws,
                           l_int32 hs, l_int32 wpls);
static void scaleColor2xLILow(l_uint32 *datad, l_int32 wpld, l_uint32 *datas,
                              l_int32 ws, l_int32 hs, l_int32 wpls);
static void scaleColor2xLILineLow(l_uint32 *lined, l_int32 wpld,
                                  l_uint32 *lines, l_int32 ws, l_int32 wpls,
                                  l_int32 lastlineflag);
static void scaleGray2xLILow(l_uint32 *datad, l_int32 wpld, l_uint32 *datas,
                             l_int32 ws, l_int32 hs, l_int32 wpls);
static void scaleGray2xLILineLow(l_uint32 *lined, l_int32 wpld,
                                 l_uint32 *lines, l_int32 ws, l_int32 wpls,
                                 l_int32 lastlineflag);
static void scaleGray4xLILow(l_uint32 *datad, l_int32 wpld, l_uint32 *datas,
                             l_int32 ws, l_int32 hs, l_int32 wpls);
static void scaleGray4xLILineLow(l_uint32 *lined, l_int32 wpld,
                                 l_uint32 *lines, l_int32 ws, l_int32 wpls,
                                 l_int32 lastlineflag);
static l_int32 scaleBySamplingLow(l_uint32 *datad, l_int32 wd, l_int32 hd,
                                  l_int32 wpld, l_uint32 *datas, l_int32 ws,
                                  l_int32 hs, l_int32 d, l_int32 wpls,
                                  l_float32 shiftx, l_float32 shifty);
static l_int32 scaleSmoothLow(l_uint32 *datad, l_int32 wd, l_int32 hd,
                              l_int32 wpld, l_uint32 *datas, l_int32 ws,
                              l_int32 hs, l_int32 d, l_int32 wpls,
                              l_int32 size);
static void scaleRGBToGray2Low(l_uint32 *datad, l_int32 wd, l_int32 hd,
                               l_int32 wpld, l_uint32 *datas, l_int32 wpls,
                               l_float32 rwt, l_float32 gwt, l_float32 bwt);
static void scaleColorAreaMapLow(l_uint32 *datad, l_int32 wd, l_int32 hd,
                                 l_int32 wpld, l_uint32 *datas, l_int32 ws,
                                 l_int32 hs, l_int32 wpls);
static void scaleGrayAreaMapLow(l_uint32 *datad, l_int32 wd, l_int32 hd,
                                l_int32 wpld, l_uint32 *datas, l_int32 ws,
                                l_int32 hs, l_int32 wpls);
static void scaleAreaMapLow2(l_uint32 *datad, l_int32 wd, l_int32 hd,
                             l_int32 wpld, l_uint32 *datas, l_int32 d,
                             l_int32 wpls);
static l_int32 scaleBinaryLow(l_uint32 *datad, l_int32 wd, l_int32 hd,
                              l_int32 wpld, l_uint32 *datas, l_int32 ws,
                              l_int32 hs, l_int32 wpls,
                              l_float32 shiftx, l_float32 shifty);

#ifndef  NO_CONSOLE_IO
#define  DEBUG_OVERFLOW   0
#define  DEBUG_UNROLLING  0
#endif  /* ~NO_CONSOLE_IO */

/*------------------------------------------------------------------*
 *                    Top level scaling dispatcher                  *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixScale()
 *
 * \param[in]    pixs       1, 2, 4, 8, 16 and 32 bpp
 * \param[in]    scalex, scaley
 * \return  pixd, or NULL on error
 *
 *  This function scales 32 bpp RGB; 2, 4 or 8 bpp palette color;
 *  2, 4, 8 or 16 bpp gray; and binary images.
 *
 *  When the input has palette color, the colormap is removed and
 *  the result is either 8 bpp gray or 32 bpp RGB, depending on whether
 *  the colormap has color entries.  Images with 2, 4 or 16 bpp are
 *  converted to 8 bpp.
 *
 *  Because pixScale is meant to be a very simple interface to a
 *  number of scaling functions, including the use of unsharp masking,
 *  the type of scaling and the sharpening parameters are chosen
 *  by default.  Grayscale and color images are scaled using one
 *  of five methods, depending on the scale factors:
 *   1. antialiased subsampling (lowpass filtering followed by
 *      subsampling, implemented by convolution, for tiny scale factors:
 *        min(scalex, scaley) < 0.02.
 *   2. antialiased subsampling (implemented by area mapping, for
 *      small scale factors:
 *        max(scalex, scaley) < 0.2 and min(scalex, scaley) >= 0.02.
 *   3. antialiased subsampling with sharpening, for scale factors
 *      between 0.2 and 0.7
 *   4. linear interpolation with sharpening, for scale factors between
 *      0.7 and 1.4
 *   5. linear interpolation without sharpening, for scale factors >= 1.4.
 *
 *  One could use subsampling for scale factors very close to 1.0,
 *  because it preserves sharp edges.  Linear interpolation blurs
 *  edges because the dest pixels will typically straddle two src edge
 *  pixels.  Subsmpling removes entire columns and rows, so the edge is
 *  not blurred.  However, there are two reasons for not doing this.
 *  First, it moves edges, so that a straight line at a large angle to
 *  both horizontal and vertical will have noticeable kinks where
 *  horizontal and vertical rasters are removed.  Second, although it
 *  is very fast, you get good results on sharp edges by applying
 *  a sharpening filter.
 *
 *  For images with sharp edges, sharpening substantially improves the
 *  image quality for scale factors between about 0.2 and about 2.0.
 *  pixScale uses a small amount of sharpening by default because
 *  it strengthens edge pixels that are weak due to anti-aliasing.
 *  The default sharpening factors are:
 *      * for scaling factors < 0.7:   sharpfract = 0.2    sharpwidth = 1
 *      * for scaling factors >= 0.7:  sharpfract = 0.4    sharpwidth = 2
 *  The cases where the sharpening halfwidth is 1 or 2 have special
 *  implementations and are about twice as fast as the general case.
 *
 *  However, sharpening is computationally expensive, and one needs
 *  to consider the speed-quality tradeoff:
 *      * For upscaling of RGB images, linear interpolation plus default
 *        sharpening is about 5 times slower than upscaling alone.
 *      * For downscaling, area mapping plus default sharpening is
 *        about 10 times slower than downscaling alone.
 *  When the scale factor is larger than 1.4, the cost of sharpening,
 *  which is proportional to image area, is very large compared to the
 *  incremental quality improvement, so we cut off the default use of
 *  sharpening at 1.4.  Thus, for scale factors greater than 1.4,
 *  pixScale only does linear interpolation.
 *
 *  In many situations you will get a satisfactory result by scaling
 *  without sharpening: call pixScaleGeneral with %sharpfract = 0.0.
 *  Alternatively, if you wish to sharpen but not use the default
 *  value, first call pixScaleGeneral with %sharpfract = 0.0, and
 *  then sharpen explicitly using pixUnsharpMasking.
 *
 *  Binary images are scaled to binary by sampling the closest pixel,
 *  without any low-pass filtering averaging of neighboring pixels.
 *  This will introduce aliasing for reductions.  Aliasing can be
 *  prevented by using pixScaleToGray instead.
 */
PIX *
pixScale(PIX       *pixs,
         l_float32  scalex,
         l_float32  scaley)
{
l_int32    sharpwidth;
l_float32  maxscale, sharpfract;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

        /* Reduce the default sharpening factors by 2 if maxscale < 0.7 */
    maxscale = L_MAX(scalex, scaley);
    sharpfract = (maxscale < 0.7) ? 0.2f : 0.4f;
    sharpwidth = (maxscale < 0.7) ? 1 : 2;

    return pixScaleGeneral(pixs, scalex, scaley, sharpfract, sharpwidth);
}


/*!
 * \brief   pixScaleToSizeRel()
 *
 * \param[in]    pixs
 * \param[in]    delw    change in width, in pixels; 0 means no change
 * \param[in]    delh    change in height, in pixels; 0 means no change
 * \return  pixd, or NULL on error
 */
PIX *
pixScaleToSizeRel(PIX     *pixs,
                  l_int32  delw,
                  l_int32  delh)
{
l_int32  w, h, wd, hd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

    if (delw == 0 && delh == 0)
        return pixCopy(NULL, pixs);

    pixGetDimensions(pixs, &w, &h, NULL);
    wd = w + delw;
    hd = h + delh;
    if (wd <= 0 || hd <= 0)
        return (PIX *)ERROR_PTR("pix dimension reduced to 0", __func__, NULL);

    return pixScaleToSize(pixs, wd, hd);
}


/*!
 * \brief   pixScaleToSize()
 *
 * \param[in]    pixs    1, 2, 4, 8, 16 and 32 bpp
 * \param[in]    wd      target width; use 0 if using height as target
 * \param[in]    hd      target height; use 0 if using width as target
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The output scaled image has the dimension(s) you specify:
 *          * To specify the width with isotropic scaling, set %hd = 0.
 *          * To specify the height with isotropic scaling, set %wd = 0.
 *          * If both %wd and %hd are specified, the image is scaled
 *             (in general, anisotropically) to that size.
 *          * It is an error to set both %wd and %hd to 0.
 * </pre>
 */
PIX *
pixScaleToSize(PIX     *pixs,
               l_int32  wd,
               l_int32  hd)
{
l_int32    w, h;
l_float32  scalex, scaley;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (wd <= 0 && hd <= 0)
        return (PIX *)ERROR_PTR("neither wd nor hd > 0", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    if (wd <= 0) {
        scaley = (l_float32)hd / (l_float32)h;
        scalex = scaley;
    } else if (hd <= 0) {
        scalex = (l_float32)wd / (l_float32)w;
        scaley = scalex;
    } else {
        scalex = (l_float32)wd / (l_float32)w;
        scaley = (l_float32)hd / (l_float32)h;
    }

    return pixScale(pixs, scalex, scaley);
}


/*!
 * \brief   pixScaleToResolution()
 *
 * \param[in]    pixs
 * \param[in]    target      desired resolution
 * \param[in]    assumed     assumed resolution if not defined; typ. 300.
 * \param[out]   pscalefact  [optional] actual scaling factor used
 * \return  pixd, or NULL on error
 */
PIX *
pixScaleToResolution(PIX        *pixs,
                     l_float32   target,
                     l_float32   assumed,
                     l_float32  *pscalefact)
{
l_int32    xres;
l_float32  factor;

    if (pscalefact) *pscalefact = 1.0;
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (target <= 0)
        return (PIX *)ERROR_PTR("target resolution <= 0", __func__, NULL);

    xres = pixGetXRes(pixs);
    if (xres <= 0) {
        if (assumed == 0)
            return pixCopy(NULL, pixs);
        xres = assumed;
    }
    factor = target / (l_float32)xres;
    if (pscalefact) *pscalefact = factor;

    return pixScale(pixs, factor, factor);
}


/*!
 * \brief   pixScaleGeneral()
 *
 * \param[in]    pixs         1, 2, 4, 8, 16 and 32 bpp
 * \param[in]    scalex       must be > 0.0
 * \param[in]    scaley       must be > 0.0
 * \param[in]    sharpfract   use 0.0 to skip sharpening
 * \param[in]    sharpwidth   halfwidth of low-pass filter; typ. 1 or 2
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See pixScale() for usage.
 *      (2) This interface may change in the future, as other special
 *          cases are added.
 *      (3) For tiny scaling factors
 *            minscale < 0.02:        use a simple lowpass filter
 *      (4) The actual sharpening factors used depend on the maximum
 *          of the two scale factors (maxscale):
 *            maxscale <= 0.2:        no sharpening
 *            0.2 < maxscale < 1.4:   uses the input parameters
 *            maxscale >= 1.4:        no sharpening
 *      (5) To avoid sharpening for grayscale and color images with
 *          scaling factors between 0.2 and 1.4, call this function
 *          with %sharpfract == 0.0.
 *      (6) To use arbitrary sharpening in conjunction with scaling,
 *          call this function with %sharpfract = 0.0, and follow this
 *          with a call to pixUnsharpMasking() with your chosen parameters.
 * </pre>
 */
PIX *
pixScaleGeneral(PIX       *pixs,
                l_float32  scalex,
                l_float32  scaley,
                l_float32  sharpfract,
                l_int32    sharpwidth)
{
l_int32    d;
l_float32  maxscale, minscale;
PIX       *pix1, *pix2, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    d = pixGetDepth(pixs);
    if (d != 1 && d != 2 && d != 4 && d != 8 && d != 16 && d != 32)
        return (PIX *)ERROR_PTR("pixs not {1,2,4,8,16,32} bpp", __func__, NULL);
    if (scalex <= 0.0 || scaley <= 0.0)
        return (PIX *)ERROR_PTR("scale factor <= 0", __func__, NULL);
    if (scalex == 1.0 && scaley == 1.0)
        return pixCopy(NULL, pixs);

    if (d == 1)
        return pixScaleBinary(pixs, scalex, scaley);

        /* Remove colormap; clone if possible; result is either 8 or 32 bpp */
    if ((pix1 = pixConvertTo8Or32(pixs, L_CLONE, 0)) == NULL)
        return (PIX *)ERROR_PTR("pix1 not made", __func__, NULL);

        /* Scale (up or down) */
    d = pixGetDepth(pix1);
    maxscale = L_MAX(scalex, scaley);
    minscale = L_MIN(scalex, scaley);
    if (maxscale < 0.7) {  /* use low-pass filter for anti-aliasing */
        if (minscale < 0.02) {  /* whole-pixel low-pass filter */
            pix2 = pixScaleSmooth(pix1, scalex, scaley);
        } else {  /* fractional pixel low-pass filter */
            pix2 = pixScaleAreaMap(pix1, scalex, scaley);
        }
        if (maxscale > 0.2 && sharpfract > 0.0 && sharpwidth > 0) {
            pixd = pixUnsharpMasking(pix2, sharpwidth, sharpfract);
        } else {
            pixd = pixClone(pix2);
        }
    } else {  /* use linear interpolation */
        if (d == 8) {
            pix2 = pixScaleGrayLI(pix1, scalex, scaley);
        } else {  /* d == 32 */
            pix2 = pixScaleColorLI(pix1, scalex, scaley);
        }
        if (maxscale < 1.4 && sharpfract > 0.0 && sharpwidth > 0) {
            pixd = pixUnsharpMasking(pix2, sharpwidth, sharpfract);
        } else {
            pixd = pixClone(pix2);
        }
    }

    pixDestroy(&pix1);
    pixDestroy(&pix2);
    pixCopyText(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    return pixd;
}


/*------------------------------------------------------------------*
 *                  Scaling by linear interpolation                 *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixScaleLI()
 *
 * \param[in]    pixs       2, 4, 8 or 32 bpp; with or without colormap
 * \param[in]    scalex     must be >= 0.7
 * \param[in]    scaley     must be >= 0.7
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This function should only be used when the scale factors are
 *          greater than or equal to 0.7, and typically greater than 1.
 *          If both scale factors are smaller than 0.7, we issue a warning
 *          and call pixScaleGeneral(), which will invoke area mapping
 *          without sharpening.
 *      (2) This works on 2, 4, 8, 16 and 32 bpp images, as well as on
 *          2, 4 and 8 bpp images that have a colormap.  If there is a
 *          colormap, it is removed to either gray or RGB, depending
 *          on the colormap.
 *      (3) This does a linear interpolation on the src image.
 *      (4) It dispatches to much faster implementations for
 *          the special cases of 2x and 4x expansion.
 * </pre>
 */
PIX *
pixScaleLI(PIX       *pixs,
           l_float32  scalex,
           l_float32  scaley)
{
l_int32    d;
l_float32  maxscale;
PIX       *pixt, *pixd;

    if (!pixs || (pixGetDepth(pixs) == 1))
        return (PIX *)ERROR_PTR("pixs not defined or 1 bpp", __func__, NULL);
    maxscale = L_MAX(scalex, scaley);
    if (maxscale < 0.7) {
        L_WARNING("scaling factors < 0.7; do regular scaling\n", __func__);
        return pixScaleGeneral(pixs, scalex, scaley, 0.0, 0);
    }
    d = pixGetDepth(pixs);
    if (d != 2 && d != 4 && d != 8 && d != 16 && d != 32)
        return (PIX *)ERROR_PTR("pixs not {2,4,8,16,32} bpp", __func__, NULL);

        /* Remove colormap; clone if possible; result is either 8 or 32 bpp */
    if ((pixt = pixConvertTo8Or32(pixs, L_CLONE, 0)) == NULL)
        return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);

    d = pixGetDepth(pixt);
    if (d == 8)
        pixd = pixScaleGrayLI(pixt, scalex, scaley);
    else  /* d == 32 */
        pixd = pixScaleColorLI(pixt, scalex, scaley);

    pixDestroy(&pixt);
    pixCopyInputFormat(pixd, pixs);
    return pixd;
}


/*!
 * \brief   pixScaleColorLI()
 *
 * \param[in]    pixs       32 bpp, representing rgb
 * \param[in]    scalex     must be >= 0.7
 * \param[in]    scaley     must be >= 0.7
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If both scale factors are smaller than 0.7, we issue a warning
 *          and call pixScaleGeneral(), which will invoke area mapping
 *          without sharpening.  This is particularly important for
 *          document images with sharp edges.
 *      (2) For the general case, it's about 4x faster to manipulate
 *          the color pixels directly, rather than to make images
 *          out of each of the 3 components, scale each component
 *          using the pixScaleGrayLI(), and combine the results back
 *          into an rgb image.
 * </pre>
 */
PIX *
pixScaleColorLI(PIX      *pixs,
               l_float32  scalex,
               l_float32  scaley)
{
l_int32    ws, hs, wpls, wd, hd, wpld;
l_uint32  *datas, *datad;
l_float32  maxscale;
PIX       *pixd;

    if (!pixs || (pixGetDepth(pixs) != 32))
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);
    maxscale = L_MAX(scalex, scaley);
    if (maxscale < 0.7) {
        L_WARNING("scaling factors < 0.7; do regular scaling\n", __func__);
        return pixScaleGeneral(pixs, scalex, scaley, 0.0, 0);
    }

        /* Do fast special cases if possible */
    if (scalex == 1.0 && scaley == 1.0)
        return pixCopy(NULL, pixs);
    if (scalex == 2.0 && scaley == 2.0)
        return pixScaleColor2xLI(pixs);
    if (scalex == 4.0 && scaley == 4.0)
        return pixScaleColor4xLI(pixs);

        /* General case */
    pixGetDimensions(pixs, &ws, &hs, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    wd = (l_int32)(scalex * (l_float32)ws + 0.5);
    hd = (l_int32)(scaley * (l_float32)hs + 0.5);
    if ((pixd = pixCreate(wd, hd, 32)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, scalex, scaley);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    scaleColorLILow(datad, wd, hd, wpld, datas, ws, hs, wpls);
    if (pixGetSpp(pixs) == 4)
        pixScaleAndTransferAlpha(pixd, pixs, scalex, scaley);

    pixCopyInputFormat(pixd, pixs);
    return pixd;
}


/*!
 * \brief   pixScaleColor2xLI()
 *
 * \param[in]    pixs    32 bpp, representing rgb
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a special case of linear interpolated scaling,
 *          for 2x upscaling.  It is about 8x faster than using
 *          the generic pixScaleColorLI(), and about 4x faster than
 *          using the special 2x scale function pixScaleGray2xLI()
 *          on each of the three components separately.
 * </pre>
 */
PIX *
pixScaleColor2xLI(PIX  *pixs)
{
l_int32    ws, hs, wpls, wpld;
l_uint32  *datas, *datad;
PIX       *pixd;

    if (!pixs || (pixGetDepth(pixs) != 32))
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if ((pixd = pixCreate(2 * ws, 2 * hs, 32)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, 2.0, 2.0);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    scaleColor2xLILow(datad, wpld, datas, ws, hs, wpls);
    if (pixGetSpp(pixs) == 4)
        pixScaleAndTransferAlpha(pixd, pixs, 2.0, 2.0);

    pixCopyInputFormat(pixd, pixs);
    return pixd;
}


/*!
 * \brief   pixScaleColor4xLI()
 *
 * \param[in]    pixs    32 bpp, representing rgb
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a special case of color linear interpolated scaling,
 *          for 4x upscaling.  It is about 3x faster than using
 *          the generic pixScaleColorLI().
 *      (2) This scales each component separately, using pixScaleGray4xLI().
 *          It would be about 4x faster to inline the color code properly,
 *          in analogy to scaleColor4xLILow(), and I leave this as
 *          an exercise for someone who really needs it.
 * </pre>
 */
PIX *
pixScaleColor4xLI(PIX  *pixs)
{
PIX  *pixr, *pixg, *pixb;
PIX  *pixrs, *pixgs, *pixbs;
PIX  *pixd;

    if (!pixs || (pixGetDepth(pixs) != 32))
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);

    pixr = pixGetRGBComponent(pixs, COLOR_RED);
    pixrs = pixScaleGray4xLI(pixr);
    pixDestroy(&pixr);
    pixg = pixGetRGBComponent(pixs, COLOR_GREEN);
    pixgs = pixScaleGray4xLI(pixg);
    pixDestroy(&pixg);
    pixb = pixGetRGBComponent(pixs, COLOR_BLUE);
    pixbs = pixScaleGray4xLI(pixb);
    pixDestroy(&pixb);

    if ((pixd = pixCreateRGBImage(pixrs, pixgs, pixbs)) == NULL) {
        L_ERROR("pixd not made\n", __func__);
    } else {
        if (pixGetSpp(pixs) == 4)
            pixScaleAndTransferAlpha(pixd, pixs, 4.0, 4.0);
        pixCopyInputFormat(pixd, pixs);
    }

    pixDestroy(&pixrs);
    pixDestroy(&pixgs);
    pixDestroy(&pixbs);
    return pixd;
}


/*!
 * \brief   pixScaleGrayLI()
 *
 * \param[in]    pixs       8 bpp grayscale, no cmap
 * \param[in]    scalex     must be >= 0.7
 * \param[in]    scaley     must be >= 0.7
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This function is appropriate for upscaling magnification, where the
 *          scale factor is > 1, as well as for a small amount of downscaling
 *          reduction, with scale factor >= 0.7.  If the scale factor is < 0.7,
 *          the best result is obtained by area mapping.
 *      (2) Here are some details:
 *          - For each pixel in the dest, this does a linear
 *            interpolation of 4 neighboring pixels in the src.
 *            Specifically, consider the UL corner of src and
 *            dest pixels.  The UL corner of the dest falls within
 *            a src pixel, whose four corners are the UL corners
 *            of 4 adjacent src pixels.  The value of the dest
 *            is taken by linear interpolation using the values of
 *            the four src pixels and the distance of the UL corner
 *            of the dest from each corner.
 *          - If the image is expanded so that the dest pixel is
 *            smaller than the src pixel, such interpolation
 *            is a reasonable approach.  This interpolation is
 *            also good for a small image reduction factor that
 *            is not more than a 2x reduction.
 *          - The linear interpolation algorithm for scaling is
 *            identical in form to the area-mapping algorithm
 *            for grayscale rotation.  The latter corresponds to a
 *            translation of each pixel without scaling.
 *          - This function is NOT optimal if the scaling involves
 *            a large reduction.  If the image is significantly
 *            reduced, so that the dest pixel is much larger than
 *            the src pixels, this interpolation, which is over src
 *            pixels only near the UL corner of the dest pixel,
 *            is not going to give a good area-mapping average.
 *            Because area mapping for image scaling is considerably
 *            more computationally intensive than linear interpolation,
 *            we choose not to use it.  For large image reduction,
 *            linear interpolation over adjacent src pixels
 *            degenerates asymptotically to subsampling.  But
 *            subsampling without a low-pass pre-filter causes
 *            aliasing by the nyquist theorem.  To avoid aliasing,
 *            a low-pass filter e.g., an averaging filter of
 *            size roughly equal to the dest pixel i.e., the reduction
 *            factor should be applied to the src before subsampling.
 *          - As an alternative to low-pass filtering and subsampling
 *            for large reduction factors, linear interpolation can
 *            also be done between the widely separated src pixels in
 *            which the corners of the dest pixel lie.  This also is
 *            not optimal, as it samples src pixels only near the
 *            corners of the dest pixel, and it is not implemented.
 * </pre>
 */
PIX *
pixScaleGrayLI(PIX       *pixs,
               l_float32  scalex,
               l_float32  scaley)
{
l_int32    ws, hs, wpls, wd, hd, wpld;
l_uint32  *datas, *datad;
l_float32  maxscale;
PIX       *pixd;

    if (!pixs || pixGetDepth(pixs) != 8 || pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs undefined, cmapped or not 8 bpp",
                                __func__, NULL);
    maxscale = L_MAX(scalex, scaley);
    if (maxscale < 0.7) {
        L_WARNING("scaling factors < 0.7; do regular scaling\n", __func__);
        return pixScaleGeneral(pixs, scalex, scaley, 0.0, 0);
    }

        /* Do fast special cases if possible */
    if (scalex == 1.0 && scaley == 1.0)
        return pixCopy(NULL, pixs);
    if (scalex == 2.0 && scaley == 2.0)
        return pixScaleGray2xLI(pixs);
    if (scalex == 4.0 && scaley == 4.0)
        return pixScaleGray4xLI(pixs);

        /* General case */
    pixGetDimensions(pixs, &ws, &hs, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    wd = (l_int32)(scalex * (l_float32)ws + 0.5);
    hd = (l_int32)(scaley * (l_float32)hs + 0.5);
    if ((pixd = pixCreate(wd, hd, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyText(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    pixScaleResolution(pixd, scalex, scaley);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    scaleGrayLILow(datad, wd, hd, wpld, datas, ws, hs, wpls);
    return pixd;
}


/*!
 * \brief   pixScaleGray2xLI()
 *
 * \param[in]    pixs    8 bpp grayscale, not cmapped
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a special case of gray linear interpolated scaling,
 *          for 2x upscaling.  It is about 6x faster than using
 *          the generic pixScaleGrayLI().
 * </pre>
 */
PIX *
pixScaleGray2xLI(PIX  *pixs)
{
l_int32    ws, hs, wpls, wpld;
l_uint32  *datas, *datad;
PIX       *pixd;

    if (!pixs || pixGetDepth(pixs) != 8 || pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs undefined, cmapped or not 8 bpp",
                                __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if ((pixd = pixCreate(2 * ws, 2 * hs, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    pixScaleResolution(pixd, 2.0, 2.0);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    scaleGray2xLILow(datad, wpld, datas, ws, hs, wpls);
    return pixd;
}


/*!
 * \brief   pixScaleGray4xLI()
 *
 * \param[in]    pixs    8 bpp grayscale, not cmapped
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a special case of gray linear interpolated scaling,
 *          for 4x upscaling.  It is about 12x faster than using
 *          the generic pixScaleGrayLI().
 * </pre>
 */
PIX *
pixScaleGray4xLI(PIX  *pixs)
{
l_int32    ws, hs, wpls, wpld;
l_uint32  *datas, *datad;
PIX       *pixd;

    if (!pixs || pixGetDepth(pixs) != 8 || pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs undefined, cmapped or not 8 bpp",
                                __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    if ((pixd = pixCreate(4 * ws, 4 * hs, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    pixScaleResolution(pixd, 4.0, 4.0);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    scaleGray4xLILow(datad, wpld, datas, ws, hs, wpls);
    return pixd;
}


/*------------------------------------------------------------------*
 *                Scale 2x followed by binarization                 *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixScaleGray2xLIThresh()
 *
 * \param[in]    pixs    8 bpp, not cmapped
 * \param[in]    thresh  between 0 and 256
 * \return  pixd 1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does 2x upscale on pixs, using linear interpolation,
 *          followed by thresholding to binary.
 *      (2) Buffers are used to avoid making a large grayscale image.
 * </pre>
 */
PIX *
pixScaleGray2xLIThresh(PIX     *pixs,
                       l_int32  thresh)
{
l_int32    i, ws, hs, hsm, wd, hd, wpls, wplb, wpld;
l_uint32  *datas, *datad, *lines, *lined, *lineb;
PIX       *pixd;

    if (!pixs || pixGetDepth(pixs) != 8 || pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs undefined, not 8 bpp, or cmapped",
                                __func__, NULL);
    if (thresh < 0 || thresh > 256)
        return (PIX *)ERROR_PTR("thresh must be in [0, ... 256]",
            __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    wd = 2 * ws;
    hd = 2 * hs;
    hsm = hs - 1;
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);

        /* Make line buffer for 2 lines of virtual intermediate image */
    wplb = (wd + 3) / 4;
    if ((lineb = (l_uint32 *)LEPT_CALLOC(2 * wplb, sizeof(l_uint32))) == NULL)
        return (PIX *)ERROR_PTR("lineb not made", __func__, NULL);

        /* Make dest binary image */
    if ((pixd = pixCreate(wd, hd, 1)) == NULL) {
        LEPT_FREE(lineb);
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    }
    pixCopyInputFormat(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, 2.0, 2.0);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);

        /* Do all but last src line */
    for (i = 0; i < hsm; i++) {
        lines = datas + i * wpls;
        lined = datad + 2 * i * wpld;  /* do 2 dest lines at a time */
        scaleGray2xLILineLow(lineb, wplb, lines, ws, wpls, 0);
        thresholdToBinaryLineLow(lined, wd, lineb, 8, thresh);
        thresholdToBinaryLineLow(lined + wpld, wd, lineb + wplb, 8, thresh);
    }

        /* Do last src line */
    lines = datas + hsm * wpls;
    lined = datad + 2 * hsm * wpld;
    scaleGray2xLILineLow(lineb, wplb, lines, ws, wpls, 1);
    thresholdToBinaryLineLow(lined, wd, lineb, 8, thresh);
    thresholdToBinaryLineLow(lined + wpld, wd, lineb + wplb, 8, thresh);

    LEPT_FREE(lineb);
    return pixd;
}


/*!
 * \brief   pixScaleGray2xLIDither()
 *
 * \param[in]    pixs    8 bpp, not cmapped
 * \return  pixd 1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does 2x upscale on pixs, using linear interpolation,
 *          followed by Floyd-Steinberg dithering to binary.
 *      (2) Buffers are used to avoid making a large grayscale image.
 *          ~ Two line buffers are used for the src, required for the 2x
 *            LI upscale.
 *          ~ Three line buffers are used for the intermediate image.
 *            Two are filled with each 2xLI row operation; the third is
 *            needed because the upscale and dithering ops are out of sync.
 * </pre>
 */
PIX *
pixScaleGray2xLIDither(PIX  *pixs)
{
l_int32    i, ws, hs, hsm, wd, hd, wpls, wplb, wpld;
l_uint32  *datas, *datad;
l_uint32  *lined;
l_uint32  *lineb = NULL;   /* 2 intermediate buffer lines */
l_uint32  *linebp = NULL;  /* 1 intermediate buffer line */
l_uint32  *bufs = NULL;    /* 2 source buffer lines */
PIX       *pixd = NULL;

    if (!pixs || pixGetDepth(pixs) != 8 || pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs undefined, not 8 bpp, or cmapped",
                                __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    wd = 2 * ws;
    hd = 2 * hs;
    hsm = hs - 1;
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);

        /* Make line buffers for 2 lines of src image */
    if ((bufs = (l_uint32 *)LEPT_CALLOC(2 * wpls, sizeof(l_uint32))) == NULL)
        return (PIX *)ERROR_PTR("bufs not made", __func__, NULL);

        /* Make line buffer for 2 lines of virtual intermediate image */
    wplb = (wd + 3) / 4;
    if ((lineb = (l_uint32 *)LEPT_CALLOC(2 * wplb, sizeof(l_uint32))) == NULL) {
        L_ERROR("lineb not made\n", __func__);
        goto cleanup;
    }

        /* Make line buffer for 1 line of virtual intermediate image */
    if ((linebp = (l_uint32 *)LEPT_CALLOC(wplb, sizeof(l_uint32))) == NULL) {
        L_ERROR("linebp not made\n", __func__);
        goto cleanup;
    }

        /* Make dest binary image */
    if ((pixd = pixCreate(wd, hd, 1)) == NULL) {
        L_ERROR("pixd not made\n", __func__);
        goto cleanup;
    }
    pixCopyInputFormat(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, 2.0, 2.0);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);

        /* Start with the first src and the first dest line */
    memcpy(bufs, datas, 4 * wpls);   /* first src line */
    memcpy(bufs + wpls, datas + wpls, 4 * wpls);  /* 2nd src line */
    scaleGray2xLILineLow(lineb, wplb, bufs, ws, wpls, 0);  /* 2 i lines */
    lined = datad;
    ditherToBinaryLineLow(lined, wd, lineb, lineb + wplb,
                          DEFAULT_CLIP_LOWER_1, DEFAULT_CLIP_UPPER_1, 0);
                                                    /* 1st d line */

        /* Do all but last src line */
    for (i = 1; i < hsm; i++) {
        memcpy(bufs, datas + i * wpls, 4 * wpls);  /* i-th src line */
        memcpy(bufs + wpls, datas + (i + 1) * wpls, 4 * wpls);
        memcpy(linebp, lineb + wplb, 4 * wplb);
        scaleGray2xLILineLow(lineb, wplb, bufs, ws, wpls, 0);  /* 2 i lines */
        lined = datad + 2 * i * wpld;
        ditherToBinaryLineLow(lined - wpld, wd, linebp, lineb,
                              DEFAULT_CLIP_LOWER_1, DEFAULT_CLIP_UPPER_1, 0);
                                                   /* odd dest line */
        ditherToBinaryLineLow(lined, wd, lineb, lineb + wplb,
                              DEFAULT_CLIP_LOWER_1, DEFAULT_CLIP_UPPER_1, 0);
                                                   /* even dest line */
    }

        /* Do the last src line and the last 3 dest lines */
    memcpy(bufs, datas + hsm * wpls, 4 * wpls);  /* hsm-th src line */
    memcpy(linebp, lineb + wplb, 4 * wplb);   /* 1 i line */
    scaleGray2xLILineLow(lineb, wplb, bufs, ws, wpls, 1);  /* 2 i lines */
    ditherToBinaryLineLow(lined + wpld, wd, linebp, lineb,
                          DEFAULT_CLIP_LOWER_1, DEFAULT_CLIP_UPPER_1, 0);
                                                   /* odd dest line */
    ditherToBinaryLineLow(lined + 2 * wpld, wd, lineb, lineb + wplb,
                          DEFAULT_CLIP_LOWER_1, DEFAULT_CLIP_UPPER_1, 0);
                                                   /* even dest line */
    ditherToBinaryLineLow(lined + 3 * wpld, wd, lineb + wplb, NULL,
                          DEFAULT_CLIP_LOWER_1, DEFAULT_CLIP_UPPER_1, 1);
                                                   /* last dest line */

cleanup:
    LEPT_FREE(bufs);
    LEPT_FREE(lineb);
    LEPT_FREE(linebp);
    return pixd;
}


/*------------------------------------------------------------------*
 *                Scale 4x followed by binarization                 *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixScaleGray4xLIThresh()
 *
 * \param[in]    pixs    8 bpp
 * \param[in]    thresh  between 0 and 256
 * \return  pixd 1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does 4x upscale on pixs, using linear interpolation,
 *          followed by thresholding to binary.
 *      (2) Buffers are used to avoid making a large grayscale image.
 *      (3) If a full 4x expanded grayscale image can be kept in memory,
 *          this function is only about 10% faster than separately doing
 *          a linear interpolation to a large grayscale image, followed
 *          by thresholding to binary.
 * </pre>
 */
PIX *
pixScaleGray4xLIThresh(PIX     *pixs,
                       l_int32  thresh)
{
l_int32    i, j, ws, hs, hsm, wd, hd, wpls, wplb, wpld;
l_uint32  *datas, *datad, *lines, *lined, *lineb;
PIX       *pixd;

    if (!pixs || pixGetDepth(pixs) != 8 || pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs undefined, not 8 bpp, or cmapped",
                                __func__, NULL);
    if (thresh < 0 || thresh > 256)
        return (PIX *)ERROR_PTR("thresh must be in [0, ... 256]",
            __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    wd = 4 * ws;
    hd = 4 * hs;
    hsm = hs - 1;
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);

        /* Make line buffer for 4 lines of virtual intermediate image */
    wplb = (wd + 3) / 4;
    if ((lineb = (l_uint32 *)LEPT_CALLOC(4 * wplb, sizeof(l_uint32))) == NULL)
        return (PIX *)ERROR_PTR("lineb not made", __func__, NULL);

        /* Make dest binary image */
    if ((pixd = pixCreate(wd, hd, 1)) == NULL) {
        LEPT_FREE(lineb);
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    }
    pixCopyInputFormat(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, 4.0, 4.0);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);

        /* Do all but last src line */
    for (i = 0; i < hsm; i++) {
        lines = datas + i * wpls;
        lined = datad + 4 * i * wpld;  /* do 4 dest lines at a time */
        scaleGray4xLILineLow(lineb, wplb, lines, ws, wpls, 0);
        for (j = 0; j < 4; j++) {
            thresholdToBinaryLineLow(lined + j * wpld, wd,
                                     lineb + j * wplb, 8, thresh);
        }
    }

        /* Do last src line */
    lines = datas + hsm * wpls;
    lined = datad + 4 * hsm * wpld;
    scaleGray4xLILineLow(lineb, wplb, lines, ws, wpls, 1);
    for (j = 0; j < 4; j++) {
        thresholdToBinaryLineLow(lined + j * wpld, wd,
                                 lineb + j * wplb, 8, thresh);
    }

    LEPT_FREE(lineb);
    return pixd;
}


/*!
 * \brief   pixScaleGray4xLIDither()
 *
 * \param[in]    pixs    8 bpp, not cmapped
 * \return  pixd 1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does 4x upscale on pixs, using linear interpolation,
 *          followed by Floyd-Steinberg dithering to binary.
 *      (2) Buffers are used to avoid making a large grayscale image.
 *          ~ Two line buffers are used for the src, required for the
 *            4xLI upscale.
 *          ~ Five line buffers are used for the intermediate image.
 *            Four are filled with each 4xLI row operation; the fifth
 *            is needed because the upscale and dithering ops are
 *            out of sync.
 *      (3) If a full 4x expanded grayscale image can be kept in memory,
 *          this function is only about 5% faster than separately doing
 *          a linear interpolation to a large grayscale image, followed
 *          by error-diffusion dithering to binary.
 * </pre>
 */
PIX *
pixScaleGray4xLIDither(PIX  *pixs)
{
l_int32    i, j, ws, hs, hsm, wd, hd, wpls, wplb, wpld;
l_uint32  *datas, *datad;
l_uint32  *lined;
l_uint32  *lineb = NULL;   /* 4 intermediate buffer lines */
l_uint32  *linebp = NULL;  /* 1 intermediate buffer line */
l_uint32  *bufs = NULL;    /* 2 source buffer lines */
PIX       *pixd = NULL;

    if (!pixs || pixGetDepth(pixs) != 8 || pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs undefined, not 8 bpp, or cmapped",
                                __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    wd = 4 * ws;
    hd = 4 * hs;
    hsm = hs - 1;
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);

        /* Make line buffers for 2 lines of src image */
    if ((bufs = (l_uint32 *)LEPT_CALLOC(2 * wpls, sizeof(l_uint32))) == NULL)
        return (PIX *)ERROR_PTR("bufs not made", __func__, NULL);

        /* Make line buffer for 4 lines of virtual intermediate image */
    wplb = (wd + 3) / 4;
    if ((lineb = (l_uint32 *)LEPT_CALLOC(4 * wplb, sizeof(l_uint32))) == NULL) {
        L_ERROR("lineb not made\n", __func__);
        goto cleanup;
    }

        /* Make line buffer for 1 line of virtual intermediate image */
    if ((linebp = (l_uint32 *)LEPT_CALLOC(wplb, sizeof(l_uint32))) == NULL) {
        L_ERROR("linebp not made\n", __func__);
        goto cleanup;
    }

        /* Make dest binary image */
    if ((pixd = pixCreate(wd, hd, 1)) == NULL) {
        L_ERROR("pixd not made\n", __func__);
        goto cleanup;
    }
    pixCopyInputFormat(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, 4.0, 4.0);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);

        /* Start with the first src and the first 3 dest lines */
    memcpy(bufs, datas, 4 * wpls);   /* first src line */
    memcpy(bufs + wpls, datas + wpls, 4 * wpls);  /* 2nd src line */
    scaleGray4xLILineLow(lineb, wplb, bufs, ws, wpls, 0);  /* 4 b lines */
    lined = datad;
    for (j = 0; j < 3; j++) {  /* first 3 d lines of Q */
        ditherToBinaryLineLow(lined + j * wpld, wd, lineb + j * wplb,
                              lineb + (j + 1) * wplb,
                              DEFAULT_CLIP_LOWER_1, DEFAULT_CLIP_UPPER_1, 0);
    }

        /* Do all but last src line */
    for (i = 1; i < hsm; i++) {
        memcpy(bufs, datas + i * wpls, 4 * wpls);  /* i-th src line */
        memcpy(bufs + wpls, datas + (i + 1) * wpls, 4 * wpls);
        memcpy(linebp, lineb + 3 * wplb, 4 * wplb);
        scaleGray4xLILineLow(lineb, wplb, bufs, ws, wpls, 0);  /* 4 b lines */
        lined = datad + 4 * i * wpld;
        ditherToBinaryLineLow(lined - wpld, wd, linebp, lineb,
                              DEFAULT_CLIP_LOWER_1, DEFAULT_CLIP_UPPER_1, 0);
                                                     /* 4th dest line of Q */
        for (j = 0; j < 3; j++) {  /* next 3 d lines of Quad */
            ditherToBinaryLineLow(lined + j * wpld, wd, lineb + j * wplb,
                                  lineb + (j + 1) * wplb,
                                 DEFAULT_CLIP_LOWER_1, DEFAULT_CLIP_UPPER_1, 0);
        }
    }

        /* Do the last src line and the last 5 dest lines */
    memcpy(bufs, datas + hsm * wpls, 4 * wpls);  /* hsm-th src line */
    memcpy(linebp, lineb + 3 * wplb, 4 * wplb);   /* 1 b line */
    scaleGray4xLILineLow(lineb, wplb, bufs, ws, wpls, 1);  /* 4 b lines */
    lined = datad + 4 * hsm * wpld;
    ditherToBinaryLineLow(lined - wpld, wd, linebp, lineb,
                          DEFAULT_CLIP_LOWER_1, DEFAULT_CLIP_UPPER_1, 0);
                                                   /* 4th dest line of Q */
    for (j = 0; j < 3; j++) {  /* next 3 d lines of Quad */
        ditherToBinaryLineLow(lined + j * wpld, wd, lineb + j * wplb,
                              lineb + (j + 1) * wplb,
                              DEFAULT_CLIP_LOWER_1, DEFAULT_CLIP_UPPER_1, 0);
    }
        /* And finally, the last dest line */
    ditherToBinaryLineLow(lined + 3 * wpld, wd, lineb + 3 * wplb, NULL,
                              DEFAULT_CLIP_LOWER_1, DEFAULT_CLIP_UPPER_1, 1);

cleanup:
    LEPT_FREE(bufs);
    LEPT_FREE(lineb);
    LEPT_FREE(linebp);
    return pixd;
}


/*------------------------------------------------------------------*
 *                  Scaling by closest pixel sampling               *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixScaleBySampling()
 *
 * \param[in]    pixs       1, 2, 4, 8, 16, 32 bpp
 * \param[in]    scalex     must be > 0.0
 * \param[in]    scaley     must be > 0.0
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This function samples from the source without
 *          filtering.  As a result, aliasing will occur for
 *          subsampling (%scalex and/or %scaley < 1.0).
 *      (2) If %scalex == 1.0 and %scaley == 1.0, returns a copy.
 *      (3) For upscaling by an integer, use pixExpandReplicate().
 *      (4) By default, indexing for the sampled source pixel is done
 *          by rounding.  This shifts the source pixel sampling down
 *          and to the right by half a pixel, which has the effect of
 *          shifting the destination image up and to the left by a
 *          number of pixels approximately equal to half the scaling
 *          factor.  To avoid this shift in the destination image,
 *          call pixScalebySamplingWithShift() using 0 for both shifts.
 * </pre>
 */
PIX *
pixScaleBySampling(PIX       *pixs,
                   l_float32  scalex,
                   l_float32  scaley)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    return pixScaleBySamplingWithShift(pixs, scalex, scaley, 0.5, 0.5);
}


/*!
 * \brief   pixScaleBySamplingWithShift()
 *
 * \param[in]    pixs      1, 2, 4, 8, 16, 32 bpp
 * \param[in]    scalex    must be > 0.0
 * \param[in]    scaley    must be > 0.0
 * \param[in]    shiftx    0.5 for default; 0.0 to mihimize edge effects
 * \param[in]    shifty    0.5 for default; 0.0 to mihimize edge effects
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The @shiftx and @shifty parameters are usually unimportant.
 *          Visible artifacts are minimized by using 0.0.
 *          Allowed values are 0.0 and 0.5.
 * </pre>
 */
PIX *
pixScaleBySamplingWithShift(PIX       *pixs,
                            l_float32  scalex,
                            l_float32  scaley,
                            l_float32  shiftx,
                            l_float32  shifty)
{
l_int32    ws, hs, d, wpls, wd, hd, wpld;
l_uint32  *datas, *datad;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (scalex <= 0.0 || scaley <= 0.0)
        return (PIX *)ERROR_PTR("scale factor <= 0", __func__, NULL);
    if (scalex == 1.0 && scaley == 1.0)
        return pixCopy(NULL, pixs);
    if (shiftx != 0.0 && shiftx != 0.5)
        return (PIX *)ERROR_PTR("shiftx != 0.0 or 0.5", __func__, NULL);
    if (shifty != 0.0 && shifty != 0.5)
        return (PIX *)ERROR_PTR("shifty != 0.0 or 0.5", __func__, NULL);
    if ((d = pixGetDepth(pixs)) == 1)
        return pixScaleBinaryWithShift(pixs, scalex, scaley, shiftx, shifty);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    wd = (l_int32)(scalex * (l_float32)ws + 0.5);
    hd = (l_int32)(scaley * (l_float32)hs + 0.5);
    if ((pixd = pixCreate(wd, hd, d)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, scalex, scaley);
    pixCopyColormap(pixd, pixs);
    pixCopyText(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    pixCopySpp(pixd, pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    scaleBySamplingLow(datad, wd, hd, wpld, datas, ws, hs, d, wpls,
                       shiftx, shifty);
    if (d == 32 && pixGetSpp(pixs) == 4)
        pixScaleAndTransferAlpha(pixd, pixs, scalex, scaley);

    return pixd;
}


/*!
 * \brief   pixScaleBySamplingToSize()
 *
 * \param[in]    pixs    1, 2, 4, 8, 16 and 32 bpp
 * \param[in]    wd      target width; use 0 if using height as target
 * \param[in]    hd      target height; use 0 if using width as target
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This guarantees that the output scaled image has the
 *          dimension(s) you specify.
 *          ~ To specify the width with isotropic scaling, set %hd = 0.
 *          ~ To specify the height with isotropic scaling, set %wd = 0.
 *          ~ If both %wd and %hd are specified, the image is scaled
 *            (in general, anisotropically) to that size.
 *          ~ It is an error to set both %wd and %hd to 0.
 * </pre>
 */
PIX *
pixScaleBySamplingToSize(PIX     *pixs,
                         l_int32  wd,
                         l_int32  hd)
{
l_int32    w, h;
l_float32  scalex, scaley;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (wd <= 0 && hd <= 0)
        return (PIX *)ERROR_PTR("neither wd nor hd > 0", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    if (wd <= 0) {
        scaley = (l_float32)hd / (l_float32)h;
        scalex = scaley;
    } else if (hd <= 0) {
        scalex = (l_float32)wd / (l_float32)w;
        scaley = scalex;
    } else {
        scalex = (l_float32)wd / (l_float32)w;
        scaley = (l_float32)hd / (l_float32)h;
    }

    return pixScaleBySampling(pixs, scalex, scaley);
}


/*!
 * \brief   pixScaleByIntSampling()
 *
 * \param[in]    pixs     1, 2, 4, 8, 16, 32 bpp  (all depths)
 * \param[in]    factor   integer subsampling; >= 1
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Simple interface to pixScaleBySampling(), for isotropic
 *          integer reduction.  If %factor == 1, returns a copy.
 * </pre>
 */
PIX *
pixScaleByIntSampling(PIX     *pixs,
                      l_int32  factor)
{
l_float32  scale;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (factor <= 1) {
        if (factor < 1)
            L_ERROR("factor must be >= 1; returning a copy\n", __func__);
        return pixCopy(NULL, pixs);
    }

    scale = 1.f / (l_float32)factor;
    return pixScaleBySampling(pixs, scale, scale);
}


/*------------------------------------------------------------------*
 *            Fast integer factor subsampling RGB to gray           *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixScaleRGBToGrayFast()
 *
 * \param[in]    pixs     32 bpp rgb
 * \param[in]    factor   integer reduction factor >= 1
 * \param[in]    color    one of COLOR_RED, COLOR_GREEN, COLOR_BLUE
 * \return  pixd 8 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does simultaneous subsampling by an integer factor and
 *          extraction of the color from the RGB pix.
 *      (2) It is designed for maximum speed, and is used for quickly
 *          generating a downsized grayscale image from a higher resolution
 *          RGB image.  This would typically be used for image analysis.
 *      (3) The standard color byte order (RGBA) is assumed.
 * </pre>
 */
PIX *
pixScaleRGBToGrayFast(PIX     *pixs,
                      l_int32  factor,
                      l_int32  color)
{
l_int32    byteval, shift;
l_int32    i, j, ws, hs, wd, hd, wpls, wpld;
l_uint32  *datas, *words, *datad, *lined;
l_float32  scale;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("depth not 32 bpp", __func__, NULL);
    if (factor < 1)
        return (PIX *)ERROR_PTR("factor must be >= 1", __func__, NULL);

    if (color == COLOR_RED)
        shift = L_RED_SHIFT;
    else if (color == COLOR_GREEN)
        shift = L_GREEN_SHIFT;
    else if (color == COLOR_BLUE)
        shift = L_BLUE_SHIFT;
    else
        return (PIX *)ERROR_PTR("invalid color", __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);

    wd = ws / factor;
    hd = hs / factor;
    if ((pixd = pixCreate(wd, hd, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    scale = 1.f / (l_float32) factor;
    pixScaleResolution(pixd, scale, scale);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    for (i = 0; i < hd; i++) {
        words = datas + i * factor * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < wd; j++, words += factor) {
            byteval = ((*words) >> shift) & 0xff;
            SET_DATA_BYTE(lined, j, byteval);
        }
    }

    return pixd;
}


/*!
 * \brief   pixScaleRGBToBinaryFast()
 *
 * \param[in]    pixs     32 bpp RGB
 * \param[in]    factor   integer reduction factor >= 1
 * \param[in]    thresh   binarization threshold
 * \return  pixd 1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does simultaneous subsampling by an integer factor and
 *          conversion from RGB to gray to binary.
 *      (2) It is designed for maximum speed, and is used for quickly
 *          generating a downsized binary image from a higher resolution
 *          RGB image.  This would typically be used for image analysis.
 *      (3) It uses the green channel to represent the RGB pixel intensity.
 * </pre>
 */
PIX *
pixScaleRGBToBinaryFast(PIX     *pixs,
                        l_int32  factor,
                        l_int32  thresh)
{
l_int32    byteval;
l_int32    i, j, ws, hs, wd, hd, wpls, wpld;
l_uint32  *datas, *words, *datad, *lined;
l_float32  scale;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (factor < 1)
        return (PIX *)ERROR_PTR("factor must be >= 1", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("depth not 32 bpp", __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);

    wd = ws / factor;
    hd = hs / factor;
    if ((pixd = pixCreate(wd, hd, 1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    scale = 1. / (l_float32) factor;
    pixScaleResolution(pixd, scale, scale);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    for (i = 0; i < hd; i++) {
        words = datas + i * factor * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < wd; j++, words += factor) {
            byteval = ((*words) >> L_GREEN_SHIFT) & 0xff;
            if (byteval < thresh)
                SET_DATA_BIT(lined, j);
        }
    }

    return pixd;
}


/*!
 * \brief   pixScaleGrayToBinaryFast()
 *
 * \param[in]    pixs     8 bpp grayscale
 * \param[in]    factor   integer reduction factor >= 1
 * \param[in]    thresh   binarization threshold
 * \return  pixd 1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does simultaneous subsampling by an integer factor and
 *          thresholding from gray to binary.
 *      (2) It is designed for maximum speed, and is used for quickly
 *          generating a downsized binary image from a higher resolution
 *          gray image.  This would typically be used for image analysis.
 * </pre>
 */
PIX *
pixScaleGrayToBinaryFast(PIX     *pixs,
                         l_int32  factor,
                         l_int32  thresh)
{
l_int32    byteval;
l_int32    i, j, ws, hs, wd, hd, wpls, wpld, sj;
l_uint32  *datas, *datad, *lines, *lined;
l_float32  scale;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (factor < 1)
        return (PIX *)ERROR_PTR("factor must be >= 1", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("depth not 8 bpp", __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);

    wd = ws / factor;
    hd = hs / factor;
    if ((pixd = pixCreate(wd, hd, 1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    scale = 1.f / (l_float32) factor;
    pixScaleResolution(pixd, scale, scale);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    for (i = 0; i < hd; i++) {
        lines = datas + i * factor * wpls;
        lined = datad + i * wpld;
        for (j = 0, sj = 0; j < wd; j++, sj += factor) {
            byteval = GET_DATA_BYTE(lines, sj);
            if (byteval < thresh)
                SET_DATA_BIT(lined, j);
        }
    }

    return pixd;
}


/*------------------------------------------------------------------*
 *               Downscaling with (antialias) smoothing             *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixScaleSmooth()
 *
 * \param[in]    pix       2, 4, 8 or 32 bpp; and 2, 4, 8 bpp with colormap
 * \param[in]    scalex    must be < 0.7
 * \param[in]    scaley    must be < 0.7
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This function should only be used when the scale factors are less
 *          than 0.7.  If either scale factor is >= 0.7, issue a warning
 *          and call pixScaleGeneral(), which will invoke linear interpolation
 *          without sharpening.
 *      (2) This works only on 2, 4, 8 and 32 bpp images, and if there is
 *          a colormap, it is removed by converting to RGB.
 *      (3) It does simple (flat filter) convolution, with a filter size
 *          commensurate with the amount of reduction, to avoid antialiasing.
 *      (4) It does simple subsampling after smoothing, which is appropriate
 *          for this range of scaling.  Linear interpolation gives essentially
 *          the same result with more computation for these scale factors,
 *          so we don't use it.
 *      (5) The result is the same as doing a full block convolution followed by
 *          subsampling, but this is faster because the results of the block
 *          convolution are only computed at the subsampling locations.
 *          In fact, the computation time is approximately independent of
 *          the scale factor, because the convolution kernel is adjusted
 *          so that each source pixel is summed approximately once.
 * </pre>
 */
PIX *
pixScaleSmooth(PIX       *pix,
               l_float32  scalex,
               l_float32  scaley)
{
l_int32    ws, hs, d, wd, hd, wpls, wpld, isize;
l_uint32   val;
l_uint32  *datas, *datad;
l_float32  minscale, size;
PIX       *pixs, *pixd;

    if (!pix)
        return (PIX *)ERROR_PTR("pix not defined", __func__, NULL);
    if (scalex >= 0.7 || scaley >= 0.7) {
        L_WARNING("scaling factor not < 0.7; do regular scaling\n", __func__);
        return pixScaleGeneral(pix, scalex, scaley, 0.0, 0);
    }
    d = pixGetDepth(pix);
    if (d != 2 && d != 4 && d !=8 && d != 32)
        return (PIX *)ERROR_PTR("pix not 2, 4, 8 or 32 bpp", __func__, NULL);

        /* Remove colormap; clone if possible; result is either 8 or 32 bpp */
    if ((pixs = pixConvertTo8Or32(pix, L_CLONE, 0)) == NULL)
        return (PIX *)ERROR_PTR("pixs not made", __func__, NULL);
    d = pixGetDepth(pixs);

        /* If 1.42 < 1/minscale < 2.5, use isize = 2
         * If 2.5 =< 1/minscale < 3.5, use isize = 3, etc.
         * Under no conditions use isize < 2  */
    minscale = L_MIN(scalex, scaley);
    size = 1.0f / minscale;   /* ideal filter full width */
    isize = L_MIN(10000, L_MAX(2, (l_int32)(size + 0.5)));

    pixGetDimensions(pixs, &ws, &hs, NULL);
    if ((ws < isize) || (hs < isize)) {
        pixd = pixCreate(1, 1, d);
        pixGetPixel(pixs, ws / 2, hs / 2, &val);
        pixSetPixel(pixd, 0, 0, val);
        L_WARNING("ridiculously small scaling factor %f\n", __func__, minscale);
        pixDestroy(&pixs);
        return pixd;
    }

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    wd = L_MAX(1, (l_int32)(scalex * (l_float32)ws + 0.5));
    hd = L_MAX(1, (l_int32)(scaley * (l_float32)hs + 0.5));
    if ((pixd = pixCreate(wd, hd, d)) == NULL) {
        pixDestroy(&pixs);
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    }
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    pixScaleResolution(pixd, scalex, scaley);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    scaleSmoothLow(datad, wd, hd, wpld, datas, ws, hs, d, wpls, isize);
    if (d == 32 && pixGetSpp(pixs) == 4)
        pixScaleAndTransferAlpha(pixd, pixs, scalex, scaley);

    pixDestroy(&pixs);
    return pixd;
}


/*!
 * \brief   pixScaleSmoothToSize()
 *
 * \param[in]    pixs   2, 4, 8 or 32 bpp; and 2, 4, 8 bpp with colormap
 * \param[in]    wd     target width; use 0 if using height as target
 * \param[in]    hd     target height; use 0 if using width as target
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See notes in pixScaleSmooth().
 *      (2) The output scaled image has the dimension(s) you specify:
 *          - To specify the width with isotropic scaling, set %hd = 0.
 *          - To specify the height with isotropic scaling, set %wd = 0.
 *          - If both %wd and %hd are specified, the image is scaled
 *             (in general, anisotropically) to that size.
 *          - It is an error to set both %wd and %hd to 0.
 * </pre>
 */
PIX *
pixScaleSmoothToSize(PIX     *pixs,
                     l_int32  wd,
                     l_int32  hd)
{
l_int32    w, h;
l_float32  scalex, scaley;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (wd <= 0 && hd <= 0)
        return (PIX *)ERROR_PTR("neither wd nor hd > 0", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    if (wd <= 0) {
        scaley = (l_float32)hd / (l_float32)h;
        scalex = scaley;
    } else if (hd <= 0) {
        scalex = (l_float32)wd / (l_float32)w;
        scaley = scalex;
    } else {
        scalex = (l_float32)wd / (l_float32)w;
        scaley = (l_float32)hd / (l_float32)h;
    }

    return pixScaleSmooth(pixs, scalex, scaley);
}


/*!
 * \brief   pixScaleRGBToGray2()
 *
 * \param[in]    pixs            32 bpp rgb
 * \param[in]    rwt, gwt, bwt   must sum to 1.0
 * \return  pixd, 8 bpp, 2x reduced, or NULL on error
 */
PIX *
pixScaleRGBToGray2(PIX       *pixs,
                   l_float32  rwt,
                   l_float32  gwt,
                   l_float32  bwt)
{
l_int32    wd, hd, wpls, wpld;
l_uint32  *datas, *datad;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (rwt + gwt + bwt < 0.98 || rwt + gwt + bwt > 1.02)
        return (PIX *)ERROR_PTR("sum of wts should be 1.0", __func__, NULL);

    wd = pixGetWidth(pixs) / 2;
    hd = pixGetHeight(pixs) / 2;
    wpls = pixGetWpl(pixs);
    datas = pixGetData(pixs);
    if ((pixd = pixCreate(wd, hd, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    pixScaleResolution(pixd, 0.5, 0.5);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);
    scaleRGBToGray2Low(datad, wd, hd, wpld, datas, wpls, rwt, gwt, bwt);
    return pixd;
}


/*------------------------------------------------------------------*
 *             Downscaling with (antialias) area mapping            *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixScaleAreaMap()
 *
 * \param[in]    pix       2, 4, 8 or 32 bpp; and 2, 4, 8 bpp with colormap
 * \param[in]    scalex    must be < 0.7; minimum is 0.02
 * \param[in]    scaley    must be < 0.7; minimum is 0.02
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a low-pass filter that averages over fractional pixels.
 *          It should only be used when the scale factors are less than 0.7.
 *          If either scale factor is greater than or equal to 0.7, we
 *          issue a warning and call pixScaleGeneral(), which will invoke
 *          linear interpolation without sharpening.
 *      (2) The minimum scale factor allowed for area mapping reduction
 *          is 0.02.  Various overflows will occur when scale factors are
 *          less than about 1/256.  If a scale factor smaller than 0.02
 *          is given, we use pixScaleSmooth(), which is a low-pass filter
 *          that averages over entire pixels.
 *      (3) This works only on 2, 4, 8 and 32 bpp images.  If there is
 *          a colormap, it is removed by converting to RGB.  In other
 *          cases, we issue a warning and call pixScaleGeneral().
 *      (4) This is faster than pixScale() because it does not do sharpening.
 *      (5) It does a relatively expensive area mapping computation, to
 *          avoid antialiasing.  It is about 2x slower than pixScaleSmooth(),
 *          but the results are much better on fine text.
 *      (6) pixScaleAreaMap2() is typically about 7x faster for the special
 *          case of 2x reduction for color images, and about 9x faster
 *          for grayscale images.  Surprisingly, the improvement in speed
 *          when using a cascade of 2x reductions for small scale factors is
 *          less than one might expect, and in most situations gives
 *          poorer image quality.  But see (6).
 *      (7) For reductions between 0.35 and 0.5, a 2x area map reduction
 *          followed by using pixScaleGeneral() on a 2x larger scalefactor
 *          (which further reduces the image size using bilinear interpolation)
 *          would give a significant speed increase, with little loss of
 *          quality, but this is not enabled as it would break too many tests.
 *          For scaling factors below 0.35, scaling atomically is nearly
 *          as fast as using a cascade of 2x scalings, and gives
 *          better results.
 * </pre>
 */
PIX *
pixScaleAreaMap(PIX       *pix,
                l_float32  scalex,
                l_float32  scaley)
{
l_int32    ws, hs, d, wd, hd, wpls, wpld;
l_uint32  *datas, *datad;
l_float32  maxscale, minscale;
PIX       *pixs, *pixd, *pix1, *pix2, *pix3;

    if (!pix)
        return (PIX *)ERROR_PTR("pix not defined", __func__, NULL);
    d = pixGetDepth(pix);
    if (d != 2 && d != 4 && d != 8 && d != 32)
        return (PIX *)ERROR_PTR("pix not 2, 4, 8 or 32 bpp", __func__, NULL);

    minscale = L_MIN(scalex, scaley);
    if (minscale < 0.02) {  /* too small for area mapping */
        L_WARNING("tiny scaling factor; using pixScaleSmooth()\n", __func__);
        return pixScaleSmooth(pix, scalex, scaley);
    }

    maxscale = L_MAX(scalex, scaley);
    if (maxscale >= 0.7) {  /* too large for area mapping */
        L_WARNING("scaling factor >= 0.7; do regular scaling\n", __func__);
        return pixScaleGeneral(pix, scalex, scaley, 0.0, 0);
    }

        /* Special cases: 2x, 4x, 8x, 16x reduction */
    if (scalex == 0.5 && scaley == 0.5)
        return pixScaleAreaMap2(pix);
    if (scalex == 0.25 && scaley == 0.25) {
        pix1 = pixScaleAreaMap2(pix);
        pixd = pixScaleAreaMap2(pix1);
        pixDestroy(&pix1);
        return pixd;
    }
    if (scalex == 0.125 && scaley == 0.125) {
        pix1 = pixScaleAreaMap2(pix);
        pix2 = pixScaleAreaMap2(pix1);
        pixd = pixScaleAreaMap2(pix2);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
        return pixd;
    }
    if (scalex == 0.0625 && scaley == 0.0625) {
        pix1 = pixScaleAreaMap2(pix);
        pix2 = pixScaleAreaMap2(pix1);
        pix3 = pixScaleAreaMap2(pix2);
        pixd = pixScaleAreaMap2(pix3);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
        pixDestroy(&pix3);
        return pixd;
    }

#if 0  /* Not enabled because it breaks too many tests that rely on exact
        * pixel matches.  */
        /* Special case where it is significantly faster to downscale first
         * by 2x, with relatively little degradation in image quality.  */
    if (scalex > 0.35 && scalex < 0.5) {
        pix1 = pixScaleAreaMap2(pix);
        pixd = pixScaleAreaMap(pix1, 2.0 * scalex, 2.0 * scaley);
        pixDestroy(&pix1);
        return pixd;
    }
#endif

        /* Remove colormap if necessary.
         * If 2 bpp or 4 bpp gray, convert to 8 bpp */
    if ((d == 2 || d == 4 || d == 8) && pixGetColormap(pix)) {
        L_WARNING("pix has colormap; removing\n", __func__);
        pixs = pixRemoveColormap(pix, REMOVE_CMAP_BASED_ON_SRC);
        d = pixGetDepth(pixs);
    } else if (d == 2 || d == 4) {
        pixs = pixConvertTo8(pix, FALSE);
        d = 8;
    } else {
        pixs = pixClone(pix);
    }

    pixGetDimensions(pixs, &ws, &hs, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    wd = (l_int32)(scalex * (l_float32)ws + 0.5);
    hd = (l_int32)(scaley * (l_float32)hs + 0.5);
    if (wd < 1 || hd < 1) {
        pixDestroy(&pixs);
        return (PIX *)ERROR_PTR("pixd too small", __func__, NULL);
    }
    if ((pixd = pixCreate(wd, hd, d)) == NULL) {
        pixDestroy(&pixs);
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    }
    pixCopyInputFormat(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, scalex, scaley);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    if (d == 8) {
        scaleGrayAreaMapLow(datad, wd, hd, wpld, datas, ws, hs, wpls);
    } else {  /* RGB, d == 32 */
        scaleColorAreaMapLow(datad, wd, hd, wpld, datas, ws, hs, wpls);
        if (pixGetSpp(pixs) == 4)
            pixScaleAndTransferAlpha(pixd, pixs, scalex, scaley);
    }

    pixDestroy(&pixs);
    return pixd;
}


/*!
 * \brief   pixScaleAreaMap2()
 *
 * \param[in]    pix     2, 4, 8 or 32 bpp; and 2, 4, 8 bpp with colormap
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This function does an area mapping (average) for 2x
 *          reduction.
 *      (2) This works only on 2, 4, 8 and 32 bpp images.  If there is
 *          a colormap, it is removed by converting to RGB.
 *      (3) Compared to the general pixScaleAreaMap(), for this function
 *          gray processing is about 14x faster and color processing
 *          is about 4x faster.  Consequently, pixScaleAreaMap2() is
 *          incorporated into the general area map scaling function,
 *          for the special cases of 2x, 4x, 8x and 16x reduction.
 * </pre>
 */
PIX *
pixScaleAreaMap2(PIX  *pix)
{
l_int32    wd, hd, d, wpls, wpld;
l_uint32  *datas, *datad;
PIX       *pixs, *pixd;

    if (!pix)
        return (PIX *)ERROR_PTR("pix not defined", __func__, NULL);
    d = pixGetDepth(pix);
    if (d != 2 && d != 4 && d != 8 && d != 32)
        return (PIX *)ERROR_PTR("pix not 2, 4, 8 or 32 bpp", __func__, NULL);

        /* Remove colormap if necessary.
         * If 2 bpp or 4 bpp gray, convert to 8 bpp */
    if ((d == 2 || d == 4 || d == 8) && pixGetColormap(pix)) {
        L_WARNING("pix has colormap; removing\n", __func__);
        pixs = pixRemoveColormap(pix, REMOVE_CMAP_BASED_ON_SRC);
        d = pixGetDepth(pixs);
    } else if (d == 2 || d == 4) {
        pixs = pixConvertTo8(pix, FALSE);
        d = 8;
    } else {
        pixs = pixClone(pix);
    }

    wd = pixGetWidth(pixs) / 2;
    hd = pixGetHeight(pixs) / 2;
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    pixd = pixCreate(wd, hd, d);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    pixCopyInputFormat(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, 0.5, 0.5);
    scaleAreaMapLow2(datad, wd, hd, wpld, datas, d, wpls);
    if (pixGetSpp(pixs) == 4)
        pixScaleAndTransferAlpha(pixd, pixs, 0.5, 0.5);
    pixDestroy(&pixs);
    return pixd;
}


/*!
 * \brief   pixScaleAreaMapToSize()
 *
 * \param[in]    pixs    2, 4, 8 or 32 bpp; and 2, 4, 8 bpp with colormap
 * \param[in]    wd      target width; use 0 if using height as target
 * \param[in]    hd      target height; use 0 if using width as target
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See notes in pixScaleAreaMap().
 *      (2) The output scaled image has the dimension(s) you specify:
 *          - To specify the width with isotropic scaling, set %hd = 0.
 *          - To specify the height with isotropic scaling, set %wd = 0.
 *          - If both %wd and %hd are specified, the image is scaled
 *             (in general, anisotropically) to that size.
 *          - It is an error to set both %wd and %hd to 0.
 * </pre>
 */
PIX *
pixScaleAreaMapToSize(PIX     *pixs,
                      l_int32  wd,
                      l_int32  hd)
{
l_int32    w, h;
l_float32  scalex, scaley;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (wd <= 0 && hd <= 0)
        return (PIX *)ERROR_PTR("neither wd nor hd > 0", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    if (wd <= 0) {
        scaley = (l_float32)hd / (l_float32)h;
        scalex = scaley;
    } else if (hd <= 0) {
        scalex = (l_float32)wd / (l_float32)w;
        scaley = scalex;
    } else {
        scalex = (l_float32)wd / (l_float32)w;
        scaley = (l_float32)hd / (l_float32)h;
    }

    return pixScaleAreaMap(pixs, scalex, scaley);
}


/*------------------------------------------------------------------*
 *               Binary scaling by closest pixel sampling           *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixScaleBinary()
 *
 * \param[in]    pixs      1 bpp
 * \param[in]    scalex    must be > 0.0
 * \param[in]    scaley    must be > 0.0
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This function samples from the source without
 *          filtering.  As a result, aliasing will occur for
 *          subsampling (scalex and scaley < 1.0).
 *      (2) By default, indexing for the sampled source pixel is done
 *          by rounding.  This shifts the source pixel sampling down
 *          and to the right by half a pixel, which has the effect of
 *          shifting the destination image up and to the left by a
 *          number of pixels approximately equal to half the scaling
 *          factor.  To avoid this shift in the destination image,
 *          call pixScalebySamplingWithShift() using 0 for both shifts.
 * </pre>
 */
PIX *
pixScaleBinary(PIX       *pixs,
               l_float32  scalex,
               l_float32  scaley)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs must be 1 bpp", __func__, NULL);
    return pixScaleBinaryWithShift(pixs, scalex, scaley, 0.5, 0.5);
}


/*!
 * \brief   pixScaleBinaryWithShift()
 *
 * \param[in]    pixs      1 bpp
 * \param[in]    scalex    must be > 0.0
 * \param[in]    scaley    must be > 0.0
 * \param[in]    shiftx    0.5 for default; 0.0 to mihimize edge effects
 * \param[in]    shifty    0.5 for default; 0.0 to mihimize edge effects
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The @shiftx and @shifty parameters are usually unimportant.
 *          Visible artifacts are minimized by using 0.0.
 *          Allowed values are 0.0 and 0.5.
 * </pre>
 */
PIX *
pixScaleBinaryWithShift(PIX       *pixs,
                        l_float32  scalex,
                        l_float32  scaley,
                        l_float32  shiftx,
                        l_float32  shifty)
{
l_int32    ws, hs, wpls, wd, hd, wpld;
l_uint32  *datas, *datad;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs must be 1 bpp", __func__, NULL);
    if (scalex <= 0.0 || scaley <= 0.0)
        return (PIX *)ERROR_PTR("scale factor <= 0", __func__, NULL);
    if (scalex == 1.0 && scaley == 1.0)
        return pixCopy(NULL, pixs);
    if (shiftx != 0.0 && shiftx != 0.5)
        return (PIX *)ERROR_PTR("shiftx != 0.0 or 0.5", __func__, NULL);
    if (shifty != 0.0 && shifty != 0.5)
        return (PIX *)ERROR_PTR("shifty != 0.0 or 0.5", __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    wd = (l_int32)(scalex * (l_float32)ws + 0.5);
    hd = (l_int32)(scaley * (l_float32)hs + 0.5);
    if ((pixd = pixCreate(wd, hd, 1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyColormap(pixd, pixs);
    pixCopyText(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, scalex, scaley);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    scaleBinaryLow(datad, wd, hd, wpld, datas, ws, hs, wpls, shiftx, shifty);
    return pixd;
}


/* ================================================================ *
 *                    Low level static functions                    *
 * ================================================================ */

/*------------------------------------------------------------------*
 *            General linear interpolated color scaling             *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleColorLILow()
 *
 * <pre>
 * Notes:
 *      (1) We choose to divide each pixel into 16 x 16 sub-pixels.
 *          Linear interpolation is equivalent to finding the
 *          fractional area (i.e., number of sub-pixels divided
 *          by 256) associated with each of the four nearest src pixels,
 *          and weighting each pixel value by this fractional area.
 * </pre>
 */
static void
scaleColorLILow(l_uint32  *datad,
               l_int32    wd,
               l_int32    hd,
               l_int32    wpld,
               l_uint32  *datas,
               l_int32    ws,
               l_int32    hs,
               l_int32    wpls)
{
l_int32    i, j, wm2, hm2;
l_int32    xpm, ypm;  /* location in src image, to 1/16 of a pixel */
l_int32    xp, yp, xf, yf;  /* src pixel and pixel fraction coordinates */
l_uint32   v00r, v01r, v10r, v11r, v00g, v01g, v10g, v11g;
l_uint32   v00b, v01b, v10b, v11b, area00, area01, area10, area11;
l_uint32   pixels1, pixels2, pixels3, pixels4, pixel;
l_uint32  *lines, *lined;
l_float32  scx, scy;

        /* (scx, scy) are scaling factors that are applied to the
         * dest coords to get the corresponding src coords.
         * We need them because we iterate over dest pixels
         * and must find the corresponding set of src pixels. */
    scx = 16.f * (l_float32)ws / (l_float32)wd;
    scy = 16.f * (l_float32)hs / (l_float32)hd;
    wm2 = ws - 2;
    hm2 = hs - 2;

        /* Iterate over the destination pixels */
    for (i = 0; i < hd; i++) {
        ypm = (l_int32)(scy * (l_float32)i);
        yp = ypm >> 4;
        yf = ypm & 0x0f;
        lined = datad + i * wpld;
        lines = datas + yp * wpls;
        for (j = 0; j < wd; j++) {
            xpm = (l_int32)(scx * (l_float32)j);
            xp = xpm >> 4;
            xf = xpm & 0x0f;

                /* Do bilinear interpolation.  This is a simple
                 * generalization of the calculation in scaleGrayLILow().
                 * Without this, we could simply subsample:
                 *     *(lined + j) = *(lines + xp);
                 * which is faster but gives lousy results!  */
            pixels1 = *(lines + xp);

            if (xp > wm2 || yp > hm2) {
                if (yp > hm2 && xp <= wm2) {  /* pixels near bottom */
                    pixels2 = *(lines + xp + 1);
                    pixels3 = pixels1;
                    pixels4 = pixels2;
                } else if (xp > wm2 && yp <= hm2) {  /* pixels near rt side */
                    pixels2 = pixels1;
                    pixels3 = *(lines + wpls + xp);
                    pixels4 = pixels3;
                } else {  /* pixels at LR corner */
                    pixels4 = pixels3 = pixels2 = pixels1;
                }
            } else {
                pixels2 = *(lines + xp + 1);
                pixels3 = *(lines + wpls + xp);
                pixels4 = *(lines + wpls + xp + 1);
            }

            area00 = (16 - xf) * (16 - yf);
            area10 = xf * (16 - yf);
            area01 = (16 - xf) * yf;
            area11 = xf * yf;
            v00r = area00 * ((pixels1 >> L_RED_SHIFT) & 0xff);
            v00g = area00 * ((pixels1 >> L_GREEN_SHIFT) & 0xff);
            v00b = area00 * ((pixels1 >> L_BLUE_SHIFT) & 0xff);
            v10r = area10 * ((pixels2 >> L_RED_SHIFT) & 0xff);
            v10g = area10 * ((pixels2 >> L_GREEN_SHIFT) & 0xff);
            v10b = area10 * ((pixels2 >> L_BLUE_SHIFT) & 0xff);
            v01r = area01 * ((pixels3 >> L_RED_SHIFT) & 0xff);
            v01g = area01 * ((pixels3 >> L_GREEN_SHIFT) & 0xff);
            v01b = area01 * ((pixels3 >> L_BLUE_SHIFT) & 0xff);
            v11r = area11 * ((pixels4 >> L_RED_SHIFT) & 0xff);
            v11g = area11 * ((pixels4 >> L_GREEN_SHIFT) & 0xff);
            v11b = area11 * ((pixels4 >> L_BLUE_SHIFT) & 0xff);
            pixel = (((v00r + v10r + v01r + v11r + 128) << 16) & 0xff000000) |
                    (((v00g + v10g + v01g + v11g + 128) << 8) & 0x00ff0000) |
                    ((v00b + v10b + v01b + v11b + 128) & 0x0000ff00);
            *(lined + j) = pixel;
        }
    }
}


/*------------------------------------------------------------------*
 *            General linear interpolated gray scaling              *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleGrayLILow()
 *
 * <pre>
 * Notes:
 *      (1) We choose to divide each pixel into 16 x 16 sub-pixels.
 *          Linear interpolation is equivalent to finding the
 *          fractional area (i.e., number of sub-pixels divided
 *          by 256) associated with each of the four nearest src pixels,
 *          and weighting each pixel value by this fractional area.
 * </pre>
 */
static void
scaleGrayLILow(l_uint32  *datad,
               l_int32    wd,
               l_int32    hd,
               l_int32    wpld,
               l_uint32  *datas,
               l_int32    ws,
               l_int32    hs,
               l_int32    wpls)
{
l_int32    i, j, wm2, hm2;
l_int32    xpm, ypm;  /* location in src image, to 1/16 of a pixel */
l_int32    xp, yp, xf, yf;  /* src pixel and pixel fraction coordinates */
l_int32    v00, v01, v10, v11, v00_val, v01_val, v10_val, v11_val;
l_uint8    val;
l_uint32  *lines, *lined;
l_float32  scx, scy;

        /* (scx, scy) are scaling factors that are applied to the
         * dest coords to get the corresponding src coords.
         * We need them because we iterate over dest pixels
         * and must find the corresponding set of src pixels. */
    scx = 16.f * (l_float32)ws / (l_float32)wd;
    scy = 16.f * (l_float32)hs / (l_float32)hd;
    wm2 = ws - 2;
    hm2 = hs - 2;

        /* Iterate over the destination pixels */
    for (i = 0; i < hd; i++) {
        ypm = (l_int32)(scy * (l_float32)i);
        yp = ypm >> 4;
        yf = ypm & 0x0f;
        lined = datad + i * wpld;
        lines = datas + yp * wpls;
        for (j = 0; j < wd; j++) {
            xpm = (l_int32)(scx * (l_float32)j);
            xp = xpm >> 4;
            xf = xpm & 0x0f;

                /* Do bilinear interpolation.  Without this, we could
                 * simply subsample:
                 *   SET_DATA_BYTE(lined, j, GET_DATA_BYTE(lines, xp));
                 * which is faster but gives lousy results!  */
            v00_val = GET_DATA_BYTE(lines, xp);
            if (xp > wm2 || yp > hm2) {
                if (yp > hm2 && xp <= wm2) {  /* pixels near bottom */
                    v01_val = v00_val;
                    v10_val = GET_DATA_BYTE(lines, xp + 1);
                    v11_val = v10_val;
                } else if (xp > wm2 && yp <= hm2) {  /* pixels near rt side */
                    v01_val = GET_DATA_BYTE(lines + wpls, xp);
                    v10_val = v00_val;
                    v11_val = v01_val;
                } else {  /* pixels at LR corner */
                    v10_val = v01_val = v11_val = v00_val;
                }
            } else {
                v10_val = GET_DATA_BYTE(lines, xp + 1);
                v01_val = GET_DATA_BYTE(lines + wpls, xp);
                v11_val = GET_DATA_BYTE(lines + wpls, xp + 1);
            }

            v00 = (16 - xf) * (16 - yf) * v00_val;
            v10 = xf * (16 - yf) * v10_val;
            v01 = (16 - xf) * yf * v01_val;
            v11 = xf * yf * v11_val;

            val = (l_uint8)((v00 + v01 + v10 + v11 + 128) / 256);
            SET_DATA_BYTE(lined, j, val);
        }
    }
}


/*------------------------------------------------------------------*
 *                2x linear interpolated color scaling              *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleColor2xLILow()
 *
 * <pre>
 * Notes:
 *      (1) This is a special case of 2x expansion by linear
 *          interpolation.  Each src pixel contains 4 dest pixels.
 *          The 4 dest pixels in src pixel 1 are numbered at
 *          their UL corners.  The 4 dest pixels in src pixel 1
 *          are related to that src pixel and its 3 neighboring
 *          src pixels as follows:
 *
 *             1-----2-----|-----|-----|
 *             |     |     |     |     |
 *             |     |     |     |     |
 *  src 1 -->  3-----4-----|     |     |  <-- src 2
 *             |     |     |     |     |
 *             |     |     |     |     |
 *             |-----|-----|-----|-----|
 *             |     |     |     |     |
 *             |     |     |     |     |
 *  src 3 -->  |     |     |     |     |  <-- src 4
 *             |     |     |     |     |
 *             |     |     |     |     |
 *             |-----|-----|-----|-----|
 *
 *           dest      src
 *           ----      ---
 *           dp1    =  sp1
 *           dp2    =  (sp1 + sp2) / 2
 *           dp3    =  (sp1 + sp3) / 2
 *           dp4    =  (sp1 + sp2 + sp3 + sp4) / 4
 *
 *      (2) We iterate over the src pixels, and unroll the calculation
 *          for each set of 4 dest pixels corresponding to that src
 *          pixel, caching pixels for the next src pixel whenever possible.
 *          The method is exactly analogous to the one we use for
 *          scaleGray2xLILow() and its line version.
 * </pre>
 */
static void
scaleColor2xLILow(l_uint32  *datad,
                  l_int32    wpld,
                  l_uint32  *datas,
                  l_int32    ws,
                  l_int32    hs,
                  l_int32    wpls)
{
l_int32    i, hsm;
l_uint32  *lines, *lined;

    hsm = hs - 1;

        /* We're taking 2 src and 2 dest lines at a time,
         * and for each src line, we're computing 2 dest lines.
         * Call these 2 dest lines:  destline1 and destline2.
         * The first src line is used for destline 1.
         * On all but the last src line, both src lines are
         * used in the linear interpolation for destline2.
         * On the last src line, both destline1 and destline2
         * are computed using only that src line (because there
         * isn't a lower src line). */

        /* iterate over all but the last src line */
    for (i = 0; i < hsm; i++) {
        lines = datas + i * wpls;
        lined = datad + 2 * i * wpld;
        scaleColor2xLILineLow(lined, wpld, lines, ws, wpls, 0);
    }

        /* last src line */
    lines = datas + hsm * wpls;
    lined = datad + 2 * hsm * wpld;
    scaleColor2xLILineLow(lined, wpld, lines, ws, wpls, 1);
}


/*!
 * \brief   scaleColor2xLILineLow()
 *
 * \param[in]    lined   ptr to top destline, to be made from current src line
 * \param[in]    wpld
 * \param[in]    lines   ptr to current src line
 * \param[in]    ws
 * \param[in]    wpls
 * \param[in]    lastlineflag  1 if last src line; 0 otherwise
 * \return  void
 */
static void
scaleColor2xLILineLow(l_uint32  *lined,
                      l_int32    wpld,
                      l_uint32  *lines,
                      l_int32    ws,
                      l_int32    wpls,
                      l_int32    lastlineflag)
{
l_int32    j, jd, wsm;
l_uint32   rval1, rval2, rval3, rval4, gval1, gval2, gval3, gval4;
l_uint32   bval1, bval2, bval3, bval4;
l_uint32   pixels1, pixels2, pixels3, pixels4, pixel;
l_uint32  *linesp, *linedp;

    wsm = ws - 1;

    if (lastlineflag == 0) {
        linesp = lines + wpls;
        linedp = lined + wpld;
        pixels1 = *lines;
        pixels3 = *linesp;

            /* initialize with v(2) and v(4) */
        rval2 = pixels1 >> 24;
        gval2 = (pixels1 >> 16) & 0xff;
        bval2 = (pixels1 >> 8) & 0xff;
        rval4 = pixels3 >> 24;
        gval4 = (pixels3 >> 16) & 0xff;
        bval4 = (pixels3 >> 8) & 0xff;

        for (j = 0, jd = 0; j < wsm; j++, jd += 2) {
                /* shift in previous src values */
            rval1 = rval2;
            gval1 = gval2;
            bval1 = bval2;
            rval3 = rval4;
            gval3 = gval4;
            bval3 = bval4;
                /* get new src values */
            pixels2 = *(lines + j + 1);
            pixels4 = *(linesp + j + 1);
            rval2 = pixels2 >> 24;
            gval2 = (pixels2 >> 16) & 0xff;
            bval2 = (pixels2 >> 8) & 0xff;
            rval4 = pixels4 >> 24;
            gval4 = (pixels4 >> 16) & 0xff;
            bval4 = (pixels4 >> 8) & 0xff;
                /* save dest values */
            pixel = (rval1 << 24 | gval1 << 16 | bval1 << 8);
            *(lined + jd) = pixel;                               /* pix 1 */
            pixel = ((((rval1 + rval2) << 23) & 0xff000000) |
                     (((gval1 + gval2) << 15) & 0x00ff0000) |
                     (((bval1 + bval2) << 7) & 0x0000ff00));
            *(lined + jd + 1) = pixel;                           /* pix 2 */
            pixel = ((((rval1 + rval3) << 23) & 0xff000000) |
                     (((gval1 + gval3) << 15) & 0x00ff0000) |
                     (((bval1 + bval3) << 7) & 0x0000ff00));
            *(linedp + jd) = pixel;                              /* pix 3 */
            pixel = ((((rval1 + rval2 + rval3 + rval4) << 22) & 0xff000000) |
                     (((gval1 + gval2 + gval3 + gval4) << 14) & 0x00ff0000) |
                     (((bval1 + bval2 + bval3 + bval4) << 6) & 0x0000ff00));
            *(linedp + jd + 1) = pixel;                          /* pix 4 */
        }
            /* last src pixel on line */
        rval1 = rval2;
        gval1 = gval2;
        bval1 = bval2;
        rval3 = rval4;
        gval3 = gval4;
        bval3 = bval4;
        pixel = (rval1 << 24 | gval1 << 16 | bval1 << 8);
        *(lined + 2 * wsm) = pixel;                        /* pix 1 */
        *(lined + 2 * wsm + 1) = pixel;                    /* pix 2 */
        pixel = ((((rval1 + rval3) << 23) & 0xff000000) |
                 (((gval1 + gval3) << 15) & 0x00ff0000) |
                 (((bval1 + bval3) << 7) & 0x0000ff00));
        *(linedp + 2 * wsm) = pixel;                       /* pix 3 */
        *(linedp + 2 * wsm + 1) = pixel;                   /* pix 4 */
    } else {   /* last row of src pixels: lastlineflag == 1 */
        linedp = lined + wpld;
        pixels2 = *lines;
        rval2 = pixels2 >> 24;
        gval2 = (pixels2 >> 16) & 0xff;
        bval2 = (pixels2 >> 8) & 0xff;
        for (j = 0, jd = 0; j < wsm; j++, jd += 2) {
            rval1 = rval2;
            gval1 = gval2;
            bval1 = bval2;
            pixels2 = *(lines + j + 1);
            rval2 = pixels2 >> 24;
            gval2 = (pixels2 >> 16) & 0xff;
            bval2 = (pixels2 >> 8) & 0xff;
            pixel = (rval1 << 24 | gval1 << 16 | bval1 << 8);
            *(lined + jd) = pixel;                            /* pix 1 */
            *(linedp + jd) = pixel;                           /* pix 2 */
            pixel = ((((rval1 + rval2) << 23) & 0xff000000) |
                     (((gval1 + gval2) << 15) & 0x00ff0000) |
                     (((bval1 + bval2) << 7) & 0x0000ff00));
            *(lined + jd + 1) = pixel;                        /* pix 3 */
            *(linedp + jd + 1) = pixel;                       /* pix 4 */
        }
        rval1 = rval2;
        gval1 = gval2;
        bval1 = bval2;
        pixel = (rval1 << 24 | gval1 << 16 | bval1 << 8);
        *(lined + 2 * wsm) = pixel;                           /* pix 1 */
        *(lined + 2 * wsm + 1) = pixel;                       /* pix 2 */
        *(linedp + 2 * wsm) = pixel;                          /* pix 3 */
        *(linedp + 2 * wsm + 1) = pixel;                      /* pix 4 */
    }
}


/*------------------------------------------------------------------*
 *                2x linear interpolated gray scaling               *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleGray2xLILow()
 *
 * <pre>
 * Notes:
 *      (1) This is a special case of 2x expansion by linear
 *          interpolation.  Each src pixel contains 4 dest pixels.
 *          The 4 dest pixels in src pixel 1 are numbered at
 *          their UL corners.  The 4 dest pixels in src pixel 1
 *          are related to that src pixel and its 3 neighboring
 *          src pixels as follows:
 *
 *             1-----2-----|-----|-----|
 *             |     |     |     |     |
 *             |     |     |     |     |
 *  src 1 -->  3-----4-----|     |     |  <-- src 2
 *             |     |     |     |     |
 *             |     |     |     |     |
 *             |-----|-----|-----|-----|
 *             |     |     |     |     |
 *             |     |     |     |     |
 *  src 3 -->  |     |     |     |     |  <-- src 4
 *             |     |     |     |     |
 *             |     |     |     |     |
 *             |-----|-----|-----|-----|
 *
 *           dest      src
 *           ----      ---
 *           dp1    =  sp1
 *           dp2    =  (sp1 + sp2) / 2
 *           dp3    =  (sp1 + sp3) / 2
 *           dp4    =  (sp1 + sp2 + sp3 + sp4) / 4
 *
 *      (2) We iterate over the src pixels, and unroll the calculation
 *          for each set of 4 dest pixels corresponding to that src
 *          pixel, caching pixels for the next src pixel whenever possible.
 * </pre>
 */
static void
scaleGray2xLILow(l_uint32  *datad,
                 l_int32    wpld,
                 l_uint32  *datas,
                 l_int32    ws,
                 l_int32    hs,
                 l_int32    wpls)
{
l_int32    i, hsm;
l_uint32  *lines, *lined;

    hsm = hs - 1;

        /* We're taking 2 src and 2 dest lines at a time,
         * and for each src line, we're computing 2 dest lines.
         * Call these 2 dest lines:  destline1 and destline2.
         * The first src line is used for destline 1.
         * On all but the last src line, both src lines are
         * used in the linear interpolation for destline2.
         * On the last src line, both destline1 and destline2
         * are computed using only that src line (because there
         * isn't a lower src line). */

        /* iterate over all but the last src line */
    for (i = 0; i < hsm; i++) {
        lines = datas + i * wpls;
        lined = datad + 2 * i * wpld;
        scaleGray2xLILineLow(lined, wpld, lines, ws, wpls, 0);
    }

        /* last src line */
    lines = datas + hsm * wpls;
    lined = datad + 2 * hsm * wpld;
    scaleGray2xLILineLow(lined, wpld, lines, ws, wpls, 1);
}


/*!
 * \brief   scaleGray2xLILineLow()
 *
 * \param[in]    lined   ptr to top destline, to be made from current src line
 * \param[in]    wpld
 * \param[in]    lines   ptr to current src line
 * \param[in]    ws
 * \param[in]    wpls
 * \param[in]    lastlineflag  1 if last src line; 0 otherwise
 * \return  void
 */
static void
scaleGray2xLILineLow(l_uint32  *lined,
                     l_int32    wpld,
                     l_uint32  *lines,
                     l_int32    ws,
                     l_int32    wpls,
                     l_int32    lastlineflag)
{
l_int32    j, jd, wsm, w;
l_uint32   sval1, sval2, sval3, sval4;
l_uint32  *linesp, *linedp;
l_uint32   words, wordsp, wordd, worddp;

    wsm = ws - 1;

    if (lastlineflag == 0) {
        linesp = lines + wpls;
        linedp = lined + wpld;

            /* Unroll the loop 4x and work on full words */
        words = lines[0];
        wordsp = linesp[0];
        sval2 = (words >> 24) & 0xff;
        sval4 = (wordsp >> 24) & 0xff;
        for (j = 0, jd = 0, w = 0; j + 3 < wsm; j += 4, jd += 8, w++) {
                /* At the top of the loop,
                 * words == lines[w], wordsp == linesp[w]
                 * and the top bytes of those have been loaded into
                 * sval2 and sval4. */
            sval1 = sval2;
            sval2 = (words >> 16) & 0xff;
            sval3 = sval4;
            sval4 = (wordsp >> 16) & 0xff;
            wordd = (sval1 << 24) | (((sval1 + sval2) >> 1) << 16);
            worddp = (((sval1 + sval3) >> 1) << 24) |
                (((sval1 + sval2 + sval3 + sval4) >> 2) << 16);

            sval1 = sval2;
            sval2 = (words >> 8) & 0xff;
            sval3 = sval4;
            sval4 = (wordsp >> 8) & 0xff;
            wordd |= (sval1 << 8) | ((sval1 + sval2) >> 1);
            worddp |= (((sval1 + sval3) >> 1) << 8) |
                ((sval1 + sval2 + sval3 + sval4) >> 2);
            lined[w * 2] = wordd;
            linedp[w * 2] = worddp;

            sval1 = sval2;
            sval2 = words & 0xff;
            sval3 = sval4;
            sval4 = wordsp & 0xff;
            wordd = (sval1 << 24) |                              /* pix 1 */
                (((sval1 + sval2) >> 1) << 16);                  /* pix 2 */
            worddp = (((sval1 + sval3) >> 1) << 24) |            /* pix 3 */
                (((sval1 + sval2 + sval3 + sval4) >> 2) << 16);  /* pix 4 */

                /* Load the next word as we need its first byte */
            words = lines[w + 1];
            wordsp = linesp[w + 1];
            sval1 = sval2;
            sval2 = (words >> 24) & 0xff;
            sval3 = sval4;
            sval4 = (wordsp >> 24) & 0xff;
            wordd |= (sval1 << 8) |                              /* pix 1 */
                ((sval1 + sval2) >> 1);                          /* pix 2 */
            worddp |= (((sval1 + sval3) >> 1) << 8) |            /* pix 3 */
                ((sval1 + sval2 + sval3 + sval4) >> 2);          /* pix 4 */
            lined[w * 2 + 1] = wordd;
            linedp[w * 2 + 1] = worddp;
        }

            /* Finish up the last word */
        for (; j < wsm; j++, jd += 2) {
            sval1 = sval2;
            sval3 = sval4;
            sval2 = GET_DATA_BYTE(lines, j + 1);
            sval4 = GET_DATA_BYTE(linesp, j + 1);
            SET_DATA_BYTE(lined, jd, sval1);                     /* pix 1 */
            SET_DATA_BYTE(lined, jd + 1, (sval1 + sval2) / 2);   /* pix 2 */
            SET_DATA_BYTE(linedp, jd, (sval1 + sval3) / 2);      /* pix 3 */
            SET_DATA_BYTE(linedp, jd + 1,
                          (sval1 + sval2 + sval3 + sval4) / 4);  /* pix 4 */
        }
        sval1 = sval2;
        sval3 = sval4;
        SET_DATA_BYTE(lined, 2 * wsm, sval1);                     /* pix 1 */
        SET_DATA_BYTE(lined, 2 * wsm + 1, sval1);                 /* pix 2 */
        SET_DATA_BYTE(linedp, 2 * wsm, (sval1 + sval3) / 2);      /* pix 3 */
        SET_DATA_BYTE(linedp, 2 * wsm + 1, (sval1 + sval3) / 2);  /* pix 4 */

#if DEBUG_UNROLLING
#define CHECK_BYTE(a, b, c) if (GET_DATA_BYTE(a, b) != c) {\
     lept_stderr("Error: mismatch at %d, %d vs %d\n", \
             j, GET_DATA_BYTE(a, b), c); }

        sval2 = GET_DATA_BYTE(lines, 0);
        sval4 = GET_DATA_BYTE(linesp, 0);
        for (j = 0, jd = 0; j < wsm; j++, jd += 2) {
            sval1 = sval2;
            sval3 = sval4;
            sval2 = GET_DATA_BYTE(lines, j + 1);
            sval4 = GET_DATA_BYTE(linesp, j + 1);
            CHECK_BYTE(lined, jd, sval1);                     /* pix 1 */
            CHECK_BYTE(lined, jd + 1, (sval1 + sval2) / 2);   /* pix 2 */
            CHECK_BYTE(linedp, jd, (sval1 + sval3) / 2);      /* pix 3 */
            CHECK_BYTE(linedp, jd + 1,
                          (sval1 + sval2 + sval3 + sval4) / 4);  /* pix 4 */
        }
        sval1 = sval2;
        sval3 = sval4;
        CHECK_BYTE(lined, 2 * wsm, sval1);                     /* pix 1 */
        CHECK_BYTE(lined, 2 * wsm + 1, sval1);                 /* pix 2 */
        CHECK_BYTE(linedp, 2 * wsm, (sval1 + sval3) / 2);      /* pix 3 */
        CHECK_BYTE(linedp, 2 * wsm + 1, (sval1 + sval3) / 2);  /* pix 4 */
#undef CHECK_BYTE
#endif
    } else {  /* last row of src pixels: lastlineflag == 1 */
        linedp = lined + wpld;
        sval2 = GET_DATA_BYTE(lines, 0);
        for (j = 0, jd = 0; j < wsm; j++, jd += 2) {
            sval1 = sval2;
            sval2 = GET_DATA_BYTE(lines, j + 1);
            SET_DATA_BYTE(lined, jd, sval1);                       /* pix 1 */
            SET_DATA_BYTE(linedp, jd, sval1);                      /* pix 3 */
            SET_DATA_BYTE(lined, jd + 1, (sval1 + sval2) / 2);     /* pix 2 */
            SET_DATA_BYTE(linedp, jd + 1, (sval1 + sval2) / 2);    /* pix 4 */
        }
        sval1 = sval2;
        SET_DATA_BYTE(lined, 2 * wsm, sval1);                     /* pix 1 */
        SET_DATA_BYTE(lined, 2 * wsm + 1, sval1);                 /* pix 2 */
        SET_DATA_BYTE(linedp, 2 * wsm, sval1);                    /* pix 3 */
        SET_DATA_BYTE(linedp, 2 * wsm + 1, sval1);                /* pix 4 */
    }
}


/*------------------------------------------------------------------*
 *               4x linear interpolated gray scaling                *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleGray4xLILow()
 *
 * <pre>
 * Notes:
 *      (1) This is a special case of 4x expansion by linear
 *          interpolation.  Each src pixel contains 16 dest pixels.
 *          The 16 dest pixels in src pixel 1 are numbered at
 *          their UL corners.  The 16 dest pixels in src pixel 1
 *          are related to that src pixel and its 3 neighboring
 *          src pixels as follows:
 *
 *             1---2---3---4---|---|---|---|---|
 *             |   |   |   |   |   |   |   |   |
 *             5---6---7---8---|---|---|---|---|
 *             |   |   |   |   |   |   |   |   |
 *  src 1 -->  9---a---b---c---|---|---|---|---|  <-- src 2
 *             |   |   |   |   |   |   |   |   |
 *             d---e---f---g---|---|---|---|---|
 *             |   |   |   |   |   |   |   |   |
 *             |===|===|===|===|===|===|===|===|
 *             |   |   |   |   |   |   |   |   |
 *             |---|---|---|---|---|---|---|---|
 *             |   |   |   |   |   |   |   |   |
 *  src 3 -->  |---|---|---|---|---|---|---|---|  <-- src 4
 *             |   |   |   |   |   |   |   |   |
 *             |---|---|---|---|---|---|---|---|
 *             |   |   |   |   |   |   |   |   |
 *             |---|---|---|---|---|---|---|---|
 *
 *           dest      src
 *           ----      ---
 *           dp1    =  sp1
 *           dp2    =  (3 * sp1 + sp2) / 4
 *           dp3    =  (sp1 + sp2) / 2
 *           dp4    =  (sp1 + 3 * sp2) / 4
 *           dp5    =  (3 * sp1 + sp3) / 4
 *           dp6    =  (9 * sp1 + 3 * sp2 + 3 * sp3 + sp4) / 16
 *           dp7    =  (3 * sp1 + 3 * sp2 + sp3 + sp4) / 8
 *           dp8    =  (3 * sp1 + 9 * sp2 + 1 * sp3 + 3 * sp4) / 16
 *           dp9    =  (sp1 + sp3) / 2
 *           dp10   =  (3 * sp1 + sp2 + 3 * sp3 + sp4) / 8
 *           dp11   =  (sp1 + sp2 + sp3 + sp4) / 4
 *           dp12   =  (sp1 + 3 * sp2 + sp3 + 3 * sp4) / 8
 *           dp13   =  (sp1 + 3 * sp3) / 4
 *           dp14   =  (3 * sp1 + sp2 + 9 * sp3 + 3 * sp4) / 16
 *           dp15   =  (sp1 + sp2 + 3 * sp3 + 3 * sp4) / 8
 *           dp16   =  (sp1 + 3 * sp2 + 3 * sp3 + 9 * sp4) / 16
 *
 *      (2) We iterate over the src pixels, and unroll the calculation
 *          for each set of 16 dest pixels corresponding to that src
 *          pixel, caching pixels for the next src pixel whenever possible.
 * </pre>
 */
static void
scaleGray4xLILow(l_uint32  *datad,
                 l_int32    wpld,
                 l_uint32  *datas,
                 l_int32    ws,
                 l_int32    hs,
                 l_int32    wpls)
{
l_int32    i, hsm;
l_uint32  *lines, *lined;

    hsm = hs - 1;

        /* We're taking 2 src and 4 dest lines at a time,
         * and for each src line, we're computing 4 dest lines.
         * Call these 4 dest lines:  destline1 - destline4.
         * The first src line is used for destline 1.
         * Two src lines are used for all other dest lines,
         * except for the last 4 dest lines, which are computed
         * using only the last src line. */

        /* iterate over all but the last src line */
    for (i = 0; i < hsm; i++) {
        lines = datas + i * wpls;
        lined = datad + 4 * i * wpld;
        scaleGray4xLILineLow(lined, wpld, lines, ws, wpls, 0);
    }

        /* last src line */
    lines = datas + hsm * wpls;
    lined = datad + 4 * hsm * wpld;
    scaleGray4xLILineLow(lined, wpld, lines, ws, wpls, 1);
}


/*!
 * \brief   scaleGray4xLILineLow()
 *
 * \param[in]    lined   ptr to top destline, to be made from current src line
 * \param[in]    wpld
 * \param[in]    lines   ptr to current src line
 * \param[in]    ws
 * \param[in]    wpls
 * \param[in]    lastlineflag  1 if last src line; 0 otherwise
 * \return  void
 */
static void
scaleGray4xLILineLow(l_uint32  *lined,
                     l_int32    wpld,
                     l_uint32  *lines,
                     l_int32    ws,
                     l_int32    wpls,
                     l_int32    lastlineflag)
{
l_int32    j, jd, wsm, wsm4;
l_int32    s1, s2, s3, s4, s1t, s2t, s3t, s4t;
l_uint32  *linesp, *linedp1, *linedp2, *linedp3;

    wsm = ws - 1;
    wsm4 = 4 * wsm;

    if (lastlineflag == 0) {
        linesp = lines + wpls;
        linedp1 = lined + wpld;
        linedp2 = lined + 2 * wpld;
        linedp3 = lined + 3 * wpld;
        s2 = GET_DATA_BYTE(lines, 0);
        s4 = GET_DATA_BYTE(linesp, 0);
        for (j = 0, jd = 0; j < wsm; j++, jd += 4) {
            s1 = s2;
            s3 = s4;
            s2 = GET_DATA_BYTE(lines, j + 1);
            s4 = GET_DATA_BYTE(linesp, j + 1);
            s1t = 3 * s1;
            s2t = 3 * s2;
            s3t = 3 * s3;
            s4t = 3 * s4;
            SET_DATA_BYTE(lined, jd, s1);                             /* d1 */
            SET_DATA_BYTE(lined, jd + 1, (s1t + s2) / 4);             /* d2 */
            SET_DATA_BYTE(lined, jd + 2, (s1 + s2) / 2);              /* d3 */
            SET_DATA_BYTE(lined, jd + 3, (s1 + s2t) / 4);             /* d4 */
            SET_DATA_BYTE(linedp1, jd, (s1t + s3) / 4);                /* d5 */
            SET_DATA_BYTE(linedp1, jd + 1, (9*s1 + s2t + s3t + s4) / 16); /*d6*/
            SET_DATA_BYTE(linedp1, jd + 2, (s1t + s2t + s3 + s4) / 8); /* d7 */
            SET_DATA_BYTE(linedp1, jd + 3, (s1t + 9*s2 + s3 + s4t) / 16);/*d8*/
            SET_DATA_BYTE(linedp2, jd, (s1 + s3) / 2);                /* d9 */
            SET_DATA_BYTE(linedp2, jd + 1, (s1t + s2 + s3t + s4) / 8);/* d10 */
            SET_DATA_BYTE(linedp2, jd + 2, (s1 + s2 + s3 + s4) / 4);  /* d11 */
            SET_DATA_BYTE(linedp2, jd + 3, (s1 + s2t + s3 + s4t) / 8);/* d12 */
            SET_DATA_BYTE(linedp3, jd, (s1 + s3t) / 4);               /* d13 */
            SET_DATA_BYTE(linedp3, jd + 1, (s1t + s2 + 9*s3 + s4t) / 16);/*d14*/
            SET_DATA_BYTE(linedp3, jd + 2, (s1 + s2 + s3t + s4t) / 8); /* d15 */
            SET_DATA_BYTE(linedp3, jd + 3, (s1 + s2t + s3t + 9*s4) / 16);/*d16*/
        }
        s1 = s2;
        s3 = s4;
        s1t = 3 * s1;
        s3t = 3 * s3;
        SET_DATA_BYTE(lined, wsm4, s1);                               /* d1 */
        SET_DATA_BYTE(lined, wsm4 + 1, s1);                           /* d2 */
        SET_DATA_BYTE(lined, wsm4 + 2, s1);                           /* d3 */
        SET_DATA_BYTE(lined, wsm4 + 3, s1);                           /* d4 */
        SET_DATA_BYTE(linedp1, wsm4, (s1t + s3) / 4);                 /* d5 */
        SET_DATA_BYTE(linedp1, wsm4 + 1, (s1t + s3) / 4);             /* d6 */
        SET_DATA_BYTE(linedp1, wsm4 + 2, (s1t + s3) / 4);             /* d7 */
        SET_DATA_BYTE(linedp1, wsm4 + 3, (s1t + s3) / 4);             /* d8 */
        SET_DATA_BYTE(linedp2, wsm4, (s1 + s3) / 2);                  /* d9 */
        SET_DATA_BYTE(linedp2, wsm4 + 1, (s1 + s3) / 2);              /* d10 */
        SET_DATA_BYTE(linedp2, wsm4 + 2, (s1 + s3) / 2);              /* d11 */
        SET_DATA_BYTE(linedp2, wsm4 + 3, (s1 + s3) / 2);              /* d12 */
        SET_DATA_BYTE(linedp3, wsm4, (s1 + s3t) / 4);                 /* d13 */
        SET_DATA_BYTE(linedp3, wsm4 + 1, (s1 + s3t) / 4);             /* d14 */
        SET_DATA_BYTE(linedp3, wsm4 + 2, (s1 + s3t) / 4);             /* d15 */
        SET_DATA_BYTE(linedp3, wsm4 + 3, (s1 + s3t) / 4);             /* d16 */
    } else {   /* last row of src pixels: lastlineflag == 1 */
        linedp1 = lined + wpld;
        linedp2 = lined + 2 * wpld;
        linedp3 = lined + 3 * wpld;
        s2 = GET_DATA_BYTE(lines, 0);
        for (j = 0, jd = 0; j < wsm; j++, jd += 4) {
            s1 = s2;
            s2 = GET_DATA_BYTE(lines, j + 1);
            s1t = 3 * s1;
            s2t = 3 * s2;
            SET_DATA_BYTE(lined, jd, s1);                            /* d1 */
            SET_DATA_BYTE(lined, jd + 1, (s1t + s2) / 4 );           /* d2 */
            SET_DATA_BYTE(lined, jd + 2, (s1 + s2) / 2 );            /* d3 */
            SET_DATA_BYTE(lined, jd + 3, (s1 + s2t) / 4 );           /* d4 */
            SET_DATA_BYTE(linedp1, jd, s1);                          /* d5 */
            SET_DATA_BYTE(linedp1, jd + 1, (s1t + s2) / 4 );         /* d6 */
            SET_DATA_BYTE(linedp1, jd + 2, (s1 + s2) / 2 );          /* d7 */
            SET_DATA_BYTE(linedp1, jd + 3, (s1 + s2t) / 4 );         /* d8 */
            SET_DATA_BYTE(linedp2, jd, s1);                          /* d9 */
            SET_DATA_BYTE(linedp2, jd + 1, (s1t + s2) / 4 );         /* d10 */
            SET_DATA_BYTE(linedp2, jd + 2, (s1 + s2) / 2 );          /* d11 */
            SET_DATA_BYTE(linedp2, jd + 3, (s1 + s2t) / 4 );         /* d12 */
            SET_DATA_BYTE(linedp3, jd, s1);                          /* d13 */
            SET_DATA_BYTE(linedp3, jd + 1, (s1t + s2) / 4 );         /* d14 */
            SET_DATA_BYTE(linedp3, jd + 2, (s1 + s2) / 2 );          /* d15 */
            SET_DATA_BYTE(linedp3, jd + 3, (s1 + s2t) / 4 );         /* d16 */
        }
        s1 = s2;
        SET_DATA_BYTE(lined, wsm4, s1);                              /* d1 */
        SET_DATA_BYTE(lined, wsm4 + 1, s1);                          /* d2 */
        SET_DATA_BYTE(lined, wsm4 + 2, s1);                          /* d3 */
        SET_DATA_BYTE(lined, wsm4 + 3, s1);                          /* d4 */
        SET_DATA_BYTE(linedp1, wsm4, s1);                            /* d5 */
        SET_DATA_BYTE(linedp1, wsm4 + 1, s1);                        /* d6 */
        SET_DATA_BYTE(linedp1, wsm4 + 2, s1);                        /* d7 */
        SET_DATA_BYTE(linedp1, wsm4 + 3, s1);                        /* d8 */
        SET_DATA_BYTE(linedp2, wsm4, s1);                            /* d9 */
        SET_DATA_BYTE(linedp2, wsm4 + 1, s1);                        /* d10 */
        SET_DATA_BYTE(linedp2, wsm4 + 2, s1);                        /* d11 */
        SET_DATA_BYTE(linedp2, wsm4 + 3, s1);                        /* d12 */
        SET_DATA_BYTE(linedp3, wsm4, s1);                            /* d13 */
        SET_DATA_BYTE(linedp3, wsm4 + 1, s1);                        /* d14 */
        SET_DATA_BYTE(linedp3, wsm4 + 2, s1);                        /* d15 */
        SET_DATA_BYTE(linedp3, wsm4 + 3, s1);                        /* d16 */
    }
}


/*------------------------------------------------------------------*
 *       Grayscale and color scaling by closest pixel sampling      *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleBySamplingLow()
 *
 * <pre>
 * Notes:
 *      (1) The dest must be cleared prior to this operation,
 *          and we clear it here in the low-level code.
 *      (2) We reuse dest pixels and dest pixel rows whenever
 *          possible.  This speeds the upscaling; downscaling
 *          is done by strict subsampling and is unaffected.
 *      (3) Because we are sampling and not interpolating, this
 *          routine works directly, without conversion to full
 *          RGB color, for 2, 4 or 8 bpp palette color images.
 * </pre>
 */
static l_int32
scaleBySamplingLow(l_uint32  *datad,
                   l_int32    wd,
                   l_int32    hd,
                   l_int32    wpld,
                   l_uint32  *datas,
                   l_int32    ws,
                   l_int32    hs,
                   l_int32    d,
                   l_int32    wpls,
                   l_float32  shiftx,
                   l_float32  shifty)
{
l_int32    i, j;
l_int32    xs, prevxs, sval;
l_int32   *srow, *scol;
l_uint32   csval;
l_uint32  *lines, *prevlines, *lined, *prevlined;
l_float32  wratio, hratio;

    if (d != 2 && d != 4 && d !=8 && d != 16 && d != 32)
        return ERROR_INT("pixel depth not supported", __func__, 1);

        /* Clear dest */
    memset(datad, 0, 4LL * hd * wpld);

        /* the source row corresponding to dest row i ==> srow[i]
         * the source col corresponding to dest col j ==> scol[j]  */
    if ((srow = (l_int32 *)LEPT_CALLOC(hd, sizeof(l_int32))) == NULL)
        return ERROR_INT("srow not made", __func__, 1);
    if ((scol = (l_int32 *)LEPT_CALLOC(wd, sizeof(l_int32))) == NULL) {
        LEPT_FREE(srow);
        return ERROR_INT("scol not made", __func__, 1);
    }

    wratio = (l_float32)ws / (l_float32)wd;
    hratio = (l_float32)hs / (l_float32)hd;
    for (i = 0; i < hd; i++)
        srow[i] = L_MIN((l_int32)(hratio * i + shifty), hs - 1);
    for (j = 0; j < wd; j++)
        scol[j] = L_MIN((l_int32)(wratio * j + shiftx), ws - 1);

    prevlines = NULL;
    for (i = 0; i < hd; i++) {
        lines = datas + srow[i] * wpls;
        lined = datad + i * wpld;
        if (lines != prevlines) {  /* make dest from new source row */
            prevxs = -1;
            sval = 0;
            csval = 0;
            if (d == 2) {
                for (j = 0; j < wd; j++) {
                    xs = scol[j];
                    if (xs != prevxs) {  /* get dest pix from source col */
                        sval = GET_DATA_DIBIT(lines, xs);
                        SET_DATA_DIBIT(lined, j, sval);
                        prevxs = xs;
                    } else {  /* copy prev dest pix */
                        SET_DATA_DIBIT(lined, j, sval);
                    }
                }
            } else if (d == 4) {
                for (j = 0; j < wd; j++) {
                    xs = scol[j];
                    if (xs != prevxs) {  /* get dest pix from source col */
                        sval = GET_DATA_QBIT(lines, xs);
                        SET_DATA_QBIT(lined, j, sval);
                        prevxs = xs;
                    } else {  /* copy prev dest pix */
                        SET_DATA_QBIT(lined, j, sval);
                    }
                }
            } else if (d == 8) {
                for (j = 0; j < wd; j++) {
                    xs = scol[j];
                    if (xs != prevxs) {  /* get dest pix from source col */
                        sval = GET_DATA_BYTE(lines, xs);
                        SET_DATA_BYTE(lined, j, sval);
                        prevxs = xs;
                    } else {  /* copy prev dest pix */
                        SET_DATA_BYTE(lined, j, sval);
                    }
                }
            } else if (d == 16) {
                for (j = 0; j < wd; j++) {
                    xs = scol[j];
                    if (xs != prevxs) {  /* get dest pix from source col */
                        sval = GET_DATA_TWO_BYTES(lines, xs);
                        SET_DATA_TWO_BYTES(lined, j, sval);
                        prevxs = xs;
                    } else {  /* copy prev dest pix */
                        SET_DATA_TWO_BYTES(lined, j, sval);
                    }
                }
            } else {  /* d == 32 */
                for (j = 0; j < wd; j++) {
                    xs = scol[j];
                    if (xs != prevxs) {  /* get dest pix from source col */
                        csval = lines[xs];
                        lined[j] = csval;
                        prevxs = xs;
                    } else {  /* copy prev dest pix */
                        lined[j] = csval;
                    }
                }
            }
        } else {  /* lines == prevlines; copy prev dest row */
            prevlined = lined - wpld;
            memcpy(lined, prevlined, 4 * wpld);
        }
        prevlines = lines;
    }

    LEPT_FREE(srow);
    LEPT_FREE(scol);
    return 0;
}


/*------------------------------------------------------------------*
 *    Color and grayscale downsampling with (antialias) smoothing   *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleSmoothLow()
 *
 * <pre>
 * Notes:
 *      (1) This function is called on 8 or 32 bpp src and dest images.
 *      (2) size is the full width of the lowpass smoothing filter.
 *          It is correlated with the reduction ratio, being the
 *          nearest integer such that size is approximately equal to hs / hd.
 * </pre>
 */
static l_int32
scaleSmoothLow(l_uint32  *datad,
               l_int32    wd,
               l_int32    hd,
               l_int32    wpld,
               l_uint32  *datas,
               l_int32    ws,
               l_int32    hs,
               l_int32    d,
               l_int32    wpls,
               l_int32    size)
{
l_int32    i, j, m, n, xstart;
l_int32    val, rval, gval, bval;
l_int32   *srow, *scol;
l_uint32  *lines, *lined, *line, *ppixel;
l_uint32   pixel;
l_float32  wratio, hratio, norm;

        /* Clear dest */
    memset(datad, 0, 4LL * wpld * hd);

        /* Each dest pixel at (j,i) is computed as the average
           of size^2 corresponding src pixels.
           We store the UL corner location of the square of
           src pixels that correspond to dest pixel (j,i).
           The are labeled by the arrays srow[i] and scol[j]. */
    if ((srow = (l_int32 *)LEPT_CALLOC(hd, sizeof(l_int32))) == NULL)
        return ERROR_INT("srow not made", __func__, 1);
    if ((scol = (l_int32 *)LEPT_CALLOC(wd, sizeof(l_int32))) == NULL) {
        LEPT_FREE(srow);
        return ERROR_INT("scol not made", __func__, 1);
    }

    norm = 1.f / (l_float32)(size * size);
    wratio = (l_float32)ws / (l_float32)wd;
    hratio = (l_float32)hs / (l_float32)hd;
    for (i = 0; i < hd; i++)
        srow[i] = L_MIN((l_int32)(hratio * i), hs - size);
    for (j = 0; j < wd; j++)
        scol[j] = L_MIN((l_int32)(wratio * j), ws - size);

        /* For each dest pixel, compute average */
    if (d == 8) {
        for (i = 0; i < hd; i++) {
            lines = datas + srow[i] * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < wd; j++) {
                xstart = scol[j];
                val = 0;
                for (m = 0; m < size; m++) {
                    line = lines + m * wpls;
                    for (n = 0; n < size; n++) {
                        val += GET_DATA_BYTE(line, xstart + n);
                    }
                }
                val = (l_int32)((l_float32)val * norm);
                SET_DATA_BYTE(lined, j, val);
            }
        }
    } else {  /* d == 32 */
        for (i = 0; i < hd; i++) {
            lines = datas + srow[i] * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < wd; j++) {
                xstart = scol[j];
                rval = gval = bval = 0;
                for (m = 0; m < size; m++) {
                    ppixel = lines + m * wpls + xstart;
                    for (n = 0; n < size; n++) {
                        pixel = *(ppixel + n);
                        rval += (pixel >> L_RED_SHIFT) & 0xff;
                        gval += (pixel >> L_GREEN_SHIFT) & 0xff;
                        bval += (pixel >> L_BLUE_SHIFT) & 0xff;
                    }
                }
                rval = (l_int32)((l_float32)rval * norm);
                gval = (l_int32)((l_float32)gval * norm);
                bval = (l_int32)((l_float32)bval * norm);
                composeRGBPixel(rval, gval, bval, lined + j);
            }
        }
    }

    LEPT_FREE(srow);
    LEPT_FREE(scol);
    return 0;
}


/*!
 * \brief   scaleRGBToGray2Low()
 *
 * <pre>
 * Notes:
 *      (1) This function is called with 32 bpp RGB src and 8 bpp,
 *          half-resolution dest.  The weights should add to 1.0.
 * </pre>
 */
static void
scaleRGBToGray2Low(l_uint32  *datad,
                   l_int32    wd,
                   l_int32    hd,
                   l_int32    wpld,
                   l_uint32  *datas,
                   l_int32    wpls,
                   l_float32  rwt,
                   l_float32  gwt,
                   l_float32  bwt)
{
l_int32    i, j, val, rval, gval, bval;
l_uint32  *lines, *lined;
l_uint32   pixel;

    rwt *= 0.25;
    gwt *= 0.25;
    bwt *= 0.25;
    for (i = 0; i < hd; i++) {
        lines = datas + 2 * i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < wd; j++) {
                /* Sum each of the color components from 4 src pixels */
            pixel = *(lines + 2 * j);
            rval = (pixel >> L_RED_SHIFT) & 0xff;
            gval = (pixel >> L_GREEN_SHIFT) & 0xff;
            bval = (pixel >> L_BLUE_SHIFT) & 0xff;
            pixel = *(lines + 2 * j + 1);
            rval += (pixel >> L_RED_SHIFT) & 0xff;
            gval += (pixel >> L_GREEN_SHIFT) & 0xff;
            bval += (pixel >> L_BLUE_SHIFT) & 0xff;
            pixel = *(lines + wpls + 2 * j);
            rval += (pixel >> L_RED_SHIFT) & 0xff;
            gval += (pixel >> L_GREEN_SHIFT) & 0xff;
            bval += (pixel >> L_BLUE_SHIFT) & 0xff;
            pixel = *(lines + wpls + 2 * j + 1);
            rval += (pixel >> L_RED_SHIFT) & 0xff;
            gval += (pixel >> L_GREEN_SHIFT) & 0xff;
            bval += (pixel >> L_BLUE_SHIFT) & 0xff;
                /* Generate the dest byte as a weighted sum of the averages */
            val = (l_int32)(rwt * rval + gwt * gval + bwt * bval);
            SET_DATA_BYTE(lined, j, val);
        }
    }
}


/*------------------------------------------------------------------*
 *                  General area mapped gray scaling                *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleColorAreaMapLow()
 *
 * <pre>
 * Notes:
 *      (1) This should only be used for downscaling.
 *          We choose to divide each pixel into 16 x 16 sub-pixels.
 *          This is much slower than scaleSmoothLow(), but it gives a
 *          better representation, esp. for downscaling factors between
 *          1.5 and 5.  All src pixels are subdivided into 256 sub-pixels,
 *          and are weighted by the number of sub-pixels covered by
 *          the dest pixel.  This is about 2x slower than scaleSmoothLow(),
 *          but the results are significantly better on small text.
 * </pre>
 */
static void
scaleColorAreaMapLow(l_uint32  *datad,
                    l_int32    wd,
                    l_int32    hd,
                    l_int32    wpld,
                    l_uint32  *datas,
                    l_int32    ws,
                    l_int32    hs,
                    l_int32    wpls)
{
l_int32    i, j, k, m, wm2, hm2;
l_int32    area00, area10, area01, area11, areal, arear, areat, areab;
l_int32    xu, yu;  /* UL corner in src image, to 1/16 of a pixel */
l_int32    xl, yl;  /* LR corner in src image, to 1/16 of a pixel */
l_int32    xup, yup, xuf, yuf;  /* UL src pixel: integer and fraction */
l_int32    xlp, ylp, xlf, ylf;  /* LR src pixel: integer and fraction */
l_int32    delx, dely, area;
l_int32    v00r, v00g, v00b;  /* contrib. from UL src pixel */
l_int32    v01r, v01g, v01b;  /* contrib. from LL src pixel */
l_int32    v10r, v10g, v10b;  /* contrib from UR src pixel */
l_int32    v11r, v11g, v11b;  /* contrib from LR src pixel */
l_int32    vinr, ving, vinb;  /* contrib from all full interior src pixels */
l_int32    vmidr, vmidg, vmidb;  /* contrib from side parts */
l_int32    rval, gval, bval;
l_uint32   pixel00, pixel10, pixel01, pixel11, pixel;
l_uint32  *lines, *lined;
l_float32  scx, scy;

        /* (scx, scy) are scaling factors that are applied to the
         * dest coords to get the corresponding src coords.
         * We need them because we iterate over dest pixels
         * and must find the corresponding set of src pixels. */
    scx = 16.f * (l_float32)ws / (l_float32)wd;
    scy = 16.f * (l_float32)hs / (l_float32)hd;
    wm2 = ws - 2;
    hm2 = hs - 2;

        /* Iterate over the destination pixels */
    for (i = 0; i < hd; i++) {
        yu = (l_int32)(scy * i);
        yl = (l_int32)(scy * (i + 1.0));
        yup = yu >> 4;
        yuf = yu & 0x0f;
        ylp = yl >> 4;
        ylf = yl & 0x0f;
        dely = ylp - yup;
        lined = datad + i * wpld;
        lines = datas + yup * wpls;
        for (j = 0; j < wd; j++) {
            xu = (l_int32)(scx * j);
            xl = (l_int32)(scx * (j + 1.0));
            xup = xu >> 4;
            xuf = xu & 0x0f;
            xlp = xl >> 4;
            xlf = xl & 0x0f;
            delx = xlp - xup;

                /* If near the edge, just use a src pixel value */
            if (xlp > wm2 || ylp > hm2) {
                *(lined + j) = *(lines + xup);
                continue;
            }

                /* Area summed over, in subpixels.  This varies
                 * due to the quantization, so we can't simply take
                 * the area to be a constant: area = scx * scy. */
            area = ((16 - xuf) + 16 * (delx - 1) + xlf) *
                   ((16 - yuf) + 16 * (dely - 1) + ylf);

                /* Do area map summation */
            pixel00 = *(lines + xup);
            pixel10 = *(lines + xlp);
            pixel01 = *(lines + dely * wpls +  xup);
            pixel11 = *(lines + dely * wpls +  xlp);
            area00 = (16 - xuf) * (16 - yuf);
            area10 = xlf * (16 - yuf);
            area01 = (16 - xuf) * ylf;
            area11 = xlf * ylf;
            v00r = area00 * ((pixel00 >> L_RED_SHIFT) & 0xff);
            v00g = area00 * ((pixel00 >> L_GREEN_SHIFT) & 0xff);
            v00b = area00 * ((pixel00 >> L_BLUE_SHIFT) & 0xff);
            v10r = area10 * ((pixel10 >> L_RED_SHIFT) & 0xff);
            v10g = area10 * ((pixel10 >> L_GREEN_SHIFT) & 0xff);
            v10b = area10 * ((pixel10 >> L_BLUE_SHIFT) & 0xff);
            v01r = area01 * ((pixel01 >> L_RED_SHIFT) & 0xff);
            v01g = area01 * ((pixel01 >> L_GREEN_SHIFT) & 0xff);
            v01b = area01 * ((pixel01 >> L_BLUE_SHIFT) & 0xff);
            v11r = area11 * ((pixel11 >> L_RED_SHIFT) & 0xff);
            v11g = area11 * ((pixel11 >> L_GREEN_SHIFT) & 0xff);
            v11b = area11 * ((pixel11 >> L_BLUE_SHIFT) & 0xff);
            vinr = ving = vinb = 0;
            for (k = 1; k < dely; k++) {  /* for full src pixels */
                for (m = 1; m < delx; m++) {
                    pixel = *(lines + k * wpls + xup + m);
                    vinr += 256 * ((pixel >> L_RED_SHIFT) & 0xff);
                    ving += 256 * ((pixel >> L_GREEN_SHIFT) & 0xff);
                    vinb += 256 * ((pixel >> L_BLUE_SHIFT) & 0xff);
                }
            }
            vmidr = vmidg = vmidb = 0;
            areal = (16 - xuf) * 16;
            arear = xlf * 16;
            areat = 16 * (16 - yuf);
            areab = 16 * ylf;
            for (k = 1; k < dely; k++) {  /* for left side */
                pixel = *(lines + k * wpls + xup);
                vmidr += areal * ((pixel >> L_RED_SHIFT) & 0xff);
                vmidg += areal * ((pixel >> L_GREEN_SHIFT) & 0xff);
                vmidb += areal * ((pixel >> L_BLUE_SHIFT) & 0xff);
            }
            for (k = 1; k < dely; k++) {  /* for right side */
                pixel = *(lines + k * wpls + xlp);
                vmidr += arear * ((pixel >> L_RED_SHIFT) & 0xff);
                vmidg += arear * ((pixel >> L_GREEN_SHIFT) & 0xff);
                vmidb += arear * ((pixel >> L_BLUE_SHIFT) & 0xff);
            }
            for (m = 1; m < delx; m++) {  /* for top side */
                pixel = *(lines + xup + m);
                vmidr += areat * ((pixel >> L_RED_SHIFT) & 0xff);
                vmidg += areat * ((pixel >> L_GREEN_SHIFT) & 0xff);
                vmidb += areat * ((pixel >> L_BLUE_SHIFT) & 0xff);
            }
            for (m = 1; m < delx; m++) {  /* for bottom side */
                pixel = *(lines + dely * wpls + xup + m);
                vmidr += areab * ((pixel >> L_RED_SHIFT) & 0xff);
                vmidg += areab * ((pixel >> L_GREEN_SHIFT) & 0xff);
                vmidb += areab * ((pixel >> L_BLUE_SHIFT) & 0xff);
            }

                /* Sum all the contributions */
            rval = (v00r + v01r + v10r + v11r + vinr + vmidr + 128) / area;
            gval = (v00g + v01g + v10g + v11g + ving + vmidg + 128) / area;
            bval = (v00b + v01b + v10b + v11b + vinb + vmidb + 128) / area;
#if  DEBUG_OVERFLOW
            if (rval > 255) lept_stderr("rval ovfl: %d\n", rval);
            if (gval > 255) lept_stderr("gval ovfl: %d\n", gval);
            if (bval > 255) lept_stderr("bval ovfl: %d\n", bval);
#endif  /* DEBUG_OVERFLOW */
            composeRGBPixel(rval, gval, bval, lined + j);
        }
    }
}


/*!
 * \brief   scaleGrayAreaMapLow()
 *
 * <pre>
 * Notes:
 *      (1) This should only be used for downscaling.
 *          We choose to divide each pixel into 16 x 16 sub-pixels.
 *          This is about 2x slower than scaleSmoothLow(), but the results
 *          are significantly better on small text, esp. for downscaling
 *          factors between 1.5 and 5.  All src pixels are subdivided
 *          into 256 sub-pixels, and are weighted by the number of
 *          sub-pixels covered by the dest pixel.
 * </pre>
 */
static void
scaleGrayAreaMapLow(l_uint32  *datad,
                    l_int32    wd,
                    l_int32    hd,
                    l_int32    wpld,
                    l_uint32  *datas,
                    l_int32    ws,
                    l_int32    hs,
                    l_int32    wpls)
{
l_int32    i, j, k, m, wm2, hm2;
l_int32    xu, yu;  /* UL corner in src image, to 1/16 of a pixel */
l_int32    xl, yl;  /* LR corner in src image, to 1/16 of a pixel */
l_int32    xup, yup, xuf, yuf;  /* UL src pixel: integer and fraction */
l_int32    xlp, ylp, xlf, ylf;  /* LR src pixel: integer and fraction */
l_int32    delx, dely, area;
l_int32    v00;  /* contrib. from UL src pixel */
l_int32    v01;  /* contrib. from LL src pixel */
l_int32    v10;  /* contrib from UR src pixel */
l_int32    v11;  /* contrib from LR src pixel */
l_int32    vin;  /* contrib from all full interior src pixels */
l_int32    vmid;  /* contrib from side parts that are full in 1 direction */
l_int32    val;
l_uint32  *lines, *lined;
l_float32  scx, scy;

        /* (scx, scy) are scaling factors that are applied to the
         * dest coords to get the corresponding src coords.
         * We need them because we iterate over dest pixels
         * and must find the corresponding set of src pixels. */
    scx = 16.f * (l_float32)ws / (l_float32)wd;
    scy = 16.f * (l_float32)hs / (l_float32)hd;
    wm2 = ws - 2;
    hm2 = hs - 2;

        /* Iterate over the destination pixels */
    for (i = 0; i < hd; i++) {
        yu = (l_int32)(scy * i);
        yl = (l_int32)(scy * (i + 1.0));
        yup = yu >> 4;
        yuf = yu & 0x0f;
        ylp = yl >> 4;
        ylf = yl & 0x0f;
        dely = ylp - yup;
        lined = datad + i * wpld;
        lines = datas + yup * wpls;
        for (j = 0; j < wd; j++) {
            xu = (l_int32)(scx * j);
            xl = (l_int32)(scx * (j + 1.0));
            xup = xu >> 4;
            xuf = xu & 0x0f;
            xlp = xl >> 4;
            xlf = xl & 0x0f;
            delx = xlp - xup;

                /* If near the edge, just use a src pixel value */
            if (xlp > wm2 || ylp > hm2) {
                SET_DATA_BYTE(lined, j, GET_DATA_BYTE(lines, xup));
                continue;
            }

                /* Area summed over, in subpixels.  This varies
                 * due to the quantization, so we can't simply take
                 * the area to be a constant: area = scx * scy. */
            area = ((16 - xuf) + 16 * (delx - 1) + xlf) *
                   ((16 - yuf) + 16 * (dely - 1) + ylf);

                /* Do area map summation */
            v00 = (16 - xuf) * (16 - yuf) * GET_DATA_BYTE(lines, xup);
            v10 = xlf * (16 - yuf) * GET_DATA_BYTE(lines, xlp);
            v01 = (16 - xuf) * ylf * GET_DATA_BYTE(lines + dely * wpls, xup);
            v11 = xlf * ylf * GET_DATA_BYTE(lines + dely * wpls, xlp);
            for (vin = 0, k = 1; k < dely; k++) {  /* for full src pixels */
                 for (m = 1; m < delx; m++) {
                     vin += 256 * GET_DATA_BYTE(lines + k * wpls, xup + m);
                 }
            }
            for (vmid = 0, k = 1; k < dely; k++)  /* for left side */
                vmid += (16 - xuf) * 16 * GET_DATA_BYTE(lines + k * wpls, xup);
            for (k = 1; k < dely; k++)  /* for right side */
                vmid += xlf * 16 * GET_DATA_BYTE(lines + k * wpls, xlp);
            for (m = 1; m < delx; m++)  /* for top side */
                vmid += 16 * (16 - yuf) * GET_DATA_BYTE(lines, xup + m);
            for (m = 1; m < delx; m++)  /* for bottom side */
                vmid += 16 * ylf * GET_DATA_BYTE(lines + dely * wpls, xup + m);
            val = (v00 + v01 + v10 + v11 + vin + vmid + 128) / area;
#if  DEBUG_OVERFLOW
            if (val > 255) lept_stderr("val overflow: %d\n", val);
#endif  /* DEBUG_OVERFLOW */
            SET_DATA_BYTE(lined, j, val);
        }
    }
}


/*------------------------------------------------------------------*
 *                     2x area mapped downscaling                   *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleAreaMapLow2()
 *
 * <pre>
 * Notes:
 *      (1) This function is called with either 8 bpp gray or 32 bpp RGB.
 *          The result is a 2x reduced dest.
 * </pre>
 */
static void
scaleAreaMapLow2(l_uint32  *datad,
                 l_int32    wd,
                 l_int32    hd,
                 l_int32    wpld,
                 l_uint32  *datas,
                 l_int32    d,
                 l_int32    wpls)
{
l_int32    i, j, val, rval, gval, bval;
l_uint32  *lines, *lined;
l_uint32   pixel;

    if (d == 8) {
        for (i = 0; i < hd; i++) {
            lines = datas + 2 * i * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < wd; j++) {
                    /* Average each dest pixel using 4 src pixels */
                val = GET_DATA_BYTE(lines, 2 * j);
                val += GET_DATA_BYTE(lines, 2 * j + 1);
                val += GET_DATA_BYTE(lines + wpls, 2 * j);
                val += GET_DATA_BYTE(lines + wpls, 2 * j + 1);
                val >>= 2;
                SET_DATA_BYTE(lined, j, val);
            }
        }
    } else {  /* d == 32 */
        for (i = 0; i < hd; i++) {
            lines = datas + 2 * i * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < wd; j++) {
                    /* Average each of the color components from 4 src pixels */
                pixel = *(lines + 2 * j);
                rval = (pixel >> L_RED_SHIFT) & 0xff;
                gval = (pixel >> L_GREEN_SHIFT) & 0xff;
                bval = (pixel >> L_BLUE_SHIFT) & 0xff;
                pixel = *(lines + 2 * j + 1);
                rval += (pixel >> L_RED_SHIFT) & 0xff;
                gval += (pixel >> L_GREEN_SHIFT) & 0xff;
                bval += (pixel >> L_BLUE_SHIFT) & 0xff;
                pixel = *(lines + wpls + 2 * j);
                rval += (pixel >> L_RED_SHIFT) & 0xff;
                gval += (pixel >> L_GREEN_SHIFT) & 0xff;
                bval += (pixel >> L_BLUE_SHIFT) & 0xff;
                pixel = *(lines + wpls + 2 * j + 1);
                rval += (pixel >> L_RED_SHIFT) & 0xff;
                gval += (pixel >> L_GREEN_SHIFT) & 0xff;
                bval += (pixel >> L_BLUE_SHIFT) & 0xff;
                composeRGBPixel(rval >> 2, gval >> 2, bval >> 2, &pixel);
                *(lined + j) = pixel;
            }
        }
    }
}


/*------------------------------------------------------------------*
 *              Binary scaling by closest pixel sampling            *
 *------------------------------------------------------------------*/
/*
 * \brief   scaleBinaryLow()
 *
 * <pre>
 * Notes:
 *      (1) The dest must be cleared prior to this operation,
 *          and we clear it here in the low-level code.
 *      (2) We reuse dest pixels and dest pixel rows whenever
 *          possible for upscaling; downscaling is done by
 *          strict subsampling.
 * </pre>
 */
static l_int32
scaleBinaryLow(l_uint32  *datad,
               l_int32    wd,
               l_int32    hd,
               l_int32    wpld,
               l_uint32  *datas,
               l_int32    ws,
               l_int32    hs,
               l_int32    wpls,
               l_float32  shiftx,
               l_float32  shifty)
{
l_int32    i, j;
l_int32    xs, prevxs, sval;
l_int32   *srow, *scol;
l_uint32  *lines, *prevlines, *lined, *prevlined;
l_float32  wratio, hratio;

        /* Clear dest */
    memset(datad, 0, 4LL * hd * wpld);

        /* The source row corresponding to dest row i ==> srow[i]
         * The source col corresponding to dest col j ==> scol[j]  */
    if ((srow = (l_int32 *)LEPT_CALLOC(hd, sizeof(l_int32))) == NULL)
        return ERROR_INT("srow not made", __func__, 1);
    if ((scol = (l_int32 *)LEPT_CALLOC(wd, sizeof(l_int32))) == NULL) {
        LEPT_FREE(srow);
        return ERROR_INT("scol not made", __func__, 1);
    }

    wratio = (l_float32)ws / (l_float32)wd;
    hratio = (l_float32)hs / (l_float32)hd;
    for (i = 0; i < hd; i++)
        srow[i] = L_MIN((l_int32)(hratio * i + shifty), hs - 1);
    for (j = 0; j < wd; j++)
        scol[j] = L_MIN((l_int32)(wratio * j + shiftx), ws - 1);

    prevlines = NULL;
    prevxs = -1;
    sval = 0;
    for (i = 0; i < hd; i++) {
        lines = datas + srow[i] * wpls;
        lined = datad + i * wpld;
        if (lines != prevlines) {  /* make dest from new source row */
            for (j = 0; j < wd; j++) {
                xs = scol[j];
                if (xs != prevxs) {  /* get dest pix from source col */
                    if ((sval = GET_DATA_BIT(lines, xs)))
                        SET_DATA_BIT(lined, j);
                    prevxs = xs;
                } else {  /* copy prev dest pix, if set */
                    if (sval)
                        SET_DATA_BIT(lined, j);
                }
            }
        } else {  /* lines == prevlines; copy prev dest row */
            prevlined = lined - wpld;
            memcpy(lined, prevlined, 4 * wpld);
        }
        prevlines = lines;
    }

    LEPT_FREE(srow);
    LEPT_FREE(scol);
    return 0;
}

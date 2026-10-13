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
 * \file scale2.c
 * <pre>
 *         Scale-to-gray (1 bpp --> 8 bpp; arbitrary downscaling)
 *               PIX      *pixScaleToGray()
 *               PIX      *pixScaleToGrayFast()
 *
 *         Scale-to-gray (1 bpp --> 8 bpp; integer downscaling)
 *               PIX      *pixScaleToGray2()
 *               PIX      *pixScaleToGray3()
 *               PIX      *pixScaleToGray4()
 *               PIX      *pixScaleToGray6()
 *               PIX      *pixScaleToGray8()
 *               PIX      *pixScaleToGray16()
 *
 *         Scale-to-gray by mipmap(1 bpp --> 8 bpp, arbitrary reduction)
 *               PIX      *pixScaleToGrayMipmap()
 *
 *         Grayscale scaling using mipmap
 *               PIX      *pixScaleMipmap()
 *
 *         Replicated (integer) expansion (all depths)
 *               PIX      *pixExpandReplicate()
 *
 *         Grayscale downscaling using min and max
 *               PIX      *pixScaleGrayMinMax()
 *               PIX      *pixScaleGrayMinMax2()
 *
 *         Grayscale downscaling using rank value
 *               PIX      *pixScaleGrayRankCascade()
 *               PIX      *pixScaleGrayRank2()
 *
 *         Helper function for transferring alpha with scaling
 *               l_int32   pixScaleAndTransferAlpha()
 *
 *         RGB scaling including alpha (blend) component
 *               PIX      *pixScaleWithAlpha()
 *
 *     Low-level static functions:
 *
 *         Scale-to-gray 2x
 *                  static void       scaleToGray2Low()
 *                  static l_uint32  *makeSumTabSG2()
 *                  static l_uint8   *makeValTabSG2()
 *
 *         Scale-to-gray 3x
 *                  static void       scaleToGray3Low()
 *                  static l_uint32  *makeSumTabSG3()
 *                  static l_uint8   *makeValTabSG3()
 *
 *         Scale-to-gray 4x
 *                  static void       scaleToGray4Low()
 *                  static l_uint32  *makeSumTabSG4()
 *                  static l_uint8   *makeValTabSG4()
 *
 *         Scale-to-gray 6x
 *                  static void       scaleToGray6Low()
 *                  static l_uint8   *makeValTabSG6()
 *
 *         Scale-to-gray 8x
 *                  static void       scaleToGray8Low()
 *                  static l_uint8   *makeValTabSG8()
 *
 *         Scale-to-gray 16x
 *                  static void       scaleToGray16Low()
 *
 *         Grayscale mipmap
 *                  static l_int32    scaleMipmapLow()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

static void scaleToGray2Low(l_uint32 *datad, l_int32 wd, l_int32 hd,
                            l_int32 wpld, l_uint32 *datas, l_int32 wpls,
                            l_uint32 *sumtab, l_uint8 *valtab);
static l_uint32 *makeSumTabSG2(void);
static l_uint8 *makeValTabSG2(void);
static void scaleToGray3Low(l_uint32 *datad, l_int32 wd, l_int32 hd,
                            l_int32 wpld, l_uint32 *datas, l_int32 wpls,
                            l_uint32 *sumtab, l_uint8 *valtab);
static l_uint32 *makeSumTabSG3(void);
static l_uint8 *makeValTabSG3(void);
static void scaleToGray4Low(l_uint32 *datad, l_int32 wd, l_int32 hd,
                            l_int32 wpld, l_uint32 *datas, l_int32 wpls,
                            l_uint32 *sumtab, l_uint8 *valtab);
static l_uint32 *makeSumTabSG4(void);
static l_uint8 *makeValTabSG4(void);
static void scaleToGray6Low(l_uint32 *datad, l_int32 wd, l_int32 hd,
                            l_int32 wpld, l_uint32 *datas, l_int32 wpls,
                            l_int32 *tab8, l_uint8 *valtab);
static l_uint8 *makeValTabSG6(void);
static void scaleToGray8Low(l_uint32 *datad, l_int32 wd, l_int32 hd,
                            l_int32 wpld, l_uint32 *datas, l_int32 wpls,
                            l_int32 *tab8, l_uint8 *valtab);
static l_uint8 *makeValTabSG8(void);
static void scaleToGray16Low(l_uint32 *datad, l_int32 wd, l_int32 hd,
                             l_int32 wpld, l_uint32 *datas, l_int32 wpls,
                             l_int32 *tab8);
static l_int32 scaleMipmapLow(l_uint32 *datad, l_int32 wd, l_int32 hd,
                              l_int32 wpld, l_uint32 *datas1, l_int32 wpls1,
                              l_uint32 *datas2, l_int32 wpls2, l_float32 red);

extern l_float32  AlphaMaskBorderVals[2];


/*------------------------------------------------------------------*
 *      Scale-to-gray (1 bpp --> 8 bpp; arbitrary downscaling)      *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixScaleToGray()
 *
 * \param[in]    pixs          1 bpp
 * \param[in]    scalefactor   reduction: must be > 0.0 and < 1.0
 * \return  pixd 8 bpp, scaled down by scalefactor in each direction,
 *              or NULL on error.
 *
 * <pre>
 * Notes:
 *
 *  For faster scaling in the range of scalefactors from 0.0625 to 0.5,
 *  with very little difference in quality, use pixScaleToGrayFast().
 *
 *  Binary images have sharp edges, so they intrinsically have very
 *  high frequency content.  To avoid aliasing, they must be low-pass
 *  filtered, which tends to blur the edges.  How can we keep relatively
 *  crisp edges without aliasing?  The trick is to do binary upscaling
 *  followed by a power-of-2 scaleToGray.  For large reductions, where
 *  you don't end up with much detail, some corners can be cut.
 *
 *  The intent here is to get high quality reduced grayscale
 *  images with relatively little computation.  We do binary
 *  pre-scaling followed by scaleToGrayN() for best results,
 *  esp. to avoid excess blur when the scale factor is near
 *  an inverse power of 2.  Where a low-pass filter is required,
 *  we use simple convolution kernels: either the hat filter for
 *  linear interpolation or a flat filter for larger downscaling.
 *  Other choices, such as a perfect bandpass filter with infinite extent
 *  (the sinc) or various approximations to it (e.g., lanczos), are
 *  unnecessarily expensive.
 *
 *  The choices made are as follows:
 *      (1) Do binary upscaling before scaleToGrayN() for scalefactors > 1/8
 *      (2) Do binary downscaling before scaleToGray8() for scalefactors
 *          between 1/16 and 1/8.
 *      (3) Use scaleToGray16() before grayscale downscaling for
 *          scalefactors less than 1/16
 *  Another reasonable choice would be to start binary downscaling
 *  for scalefactors below 1/4, rather than below 1/8 as we do here.
 *
 *  The general scaling rules, not all of which are used here, go as follows:
 *      (1) For grayscale upscaling, use pixScaleGrayLI().  However,
 *          note that edges will be visibly blurred for scalefactors
 *          near (but above) 1.0.  Replication will avoid edge blur,
 *          and should be considered for factors very near 1.0.
 *      (2) For grayscale downscaling with a scale factor larger than
 *          about 0.7, use pixScaleGrayLI().  For scalefactors near
 *          (but below) 1.0, you tread between Scylla and Charybdis.
 *          pixScaleGrayLI() again gives edge blurring, but
 *          pixScaleBySampling() gives visible aliasing.
 *      (3) For grayscale downscaling with a scale factor smaller than
 *          about 0.7, use pixScaleSmooth()
 *      (4) For binary input images, do as much scale to gray as possible
 *          using the special integer functions (2, 3, 4, 8 and 16).
 *      (5) It is better to upscale in binary, followed by scaleToGrayN()
 *          than to do scaleToGrayN() followed by an upscale using either
 *          LI or oversampling.
 *      (6) It may be better to downscale in binary, followed by
 *          scaleToGrayN() than to first use scaleToGrayN() followed by
 *          downscaling.  For downscaling between 8x and 16x, this is
 *          a reasonable option.
 *      (7) For reductions greater than 16x, it's reasonable to use
 *          scaleToGray16() followed by further grayscale downscaling.
 * </pre>
 */
PIX *
pixScaleToGray(PIX       *pixs,
               l_float32  scalefactor)
{
l_int32    w, h, minsrc, mindest;
l_float32  mag, red;
PIX       *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, NULL);
    if (scalefactor <= 0.0f)
        return (PIX *)ERROR_PTR("scalefactor <= 0.0", __func__, NULL);
    if (scalefactor >= 1.0f)
        return (PIX *)ERROR_PTR("scalefactor >= 1.0", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    minsrc = L_MIN(w, h);
    mindest = (l_int32)((l_float32)minsrc * scalefactor);
    if (mindest < 2)
        return (PIX *)ERROR_PTR("scalefactor too small", __func__, NULL);

    if (scalefactor > 0.5f) {   /* see note (5) */
        mag = 2.0f * scalefactor;  /* will be < 2.0 */
/*        lept_stderr("2x with mag %7.3f\n", mag);  */
        if ((pixt = pixScaleBinary(pixs, mag, mag)) == NULL)
            return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);
        pixd = pixScaleToGray2(pixt);
    } else if (scalefactor == 0.5f) {
        return pixd = pixScaleToGray2(pixs);
    } else if (scalefactor > 0.33333f) {   /* see note (5) */
        mag = 3.0f * scalefactor;   /* will be < 1.5 */
/*        lept_stderr("3x with mag %7.3f\n", mag);  */
        if ((pixt = pixScaleBinary(pixs, mag, mag)) == NULL)
            return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);
        pixd = pixScaleToGray3(pixt);
    } else if (scalefactor > 0.25f) {  /* see note (5) */
        mag = 4.0f * scalefactor;   /* will be < 1.3333 */
/*        lept_stderr("4x with mag %7.3f\n", mag);  */
        if ((pixt = pixScaleBinary(pixs, mag, mag)) == NULL)
            return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);
        pixd = pixScaleToGray4(pixt);
    } else if (scalefactor == 0.25f) {
        return pixd = pixScaleToGray4(pixs);
    } else if (scalefactor > 0.16667f) {  /* see note (5) */
        mag = 6.0f * scalefactor;   /* will be < 1.5 */
/*        lept_stderr("6x with mag %7.3f\n", mag); */
        if ((pixt = pixScaleBinary(pixs, mag, mag)) == NULL)
            return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);
        pixd = pixScaleToGray6(pixt);
    } else if (scalefactor == 0.16667f) {
        return pixd = pixScaleToGray6(pixs);
    } else if (scalefactor > 0.125f) {  /* see note (5) */
        mag = 8.0f * scalefactor;   /*  will be < 1.3333  */
/*        lept_stderr("8x with mag %7.3f\n", mag);  */
        if ((pixt = pixScaleBinary(pixs, mag, mag)) == NULL)
            return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);
        pixd = pixScaleToGray8(pixt);
    } else if (scalefactor == 0.125f) {
        return pixd = pixScaleToGray8(pixs);
    } else if (scalefactor > 0.0625f) {  /* see note (6) */
        red = 8.0f * scalefactor;   /* will be > 0.5 */
/*        lept_stderr("8x with red %7.3f\n", red);  */
        if ((pixt = pixScaleBinary(pixs, red, red)) == NULL)
            return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);
        pixd = pixScaleToGray8(pixt);
    } else if (scalefactor == 0.0625f) {
        return pixd = pixScaleToGray16(pixs);
    } else {  /* see note (7) */
        red = 16.0f * scalefactor;  /* will be <= 1.0 */
/*        lept_stderr("16x with red %7.3f\n", red);  */
        if ((pixt = pixScaleToGray16(pixs)) == NULL)
            return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);
        if (red < 0.7f)
            pixd = pixScaleSmooth(pixt, red, red);  /* see note (3) */
        else
            pixd = pixScaleGrayLI(pixt, red, red);  /* see note (2) */
    }

    pixDestroy(&pixt);
    if (!pixd)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyInputFormat(pixd, pixs);
    return pixd;
}


/*!
 * \brief   pixScaleToGrayFast()
 *
 * \param[in]    pixs          1 bpp
 * \param[in]    scalefactor   reduction: must be > 0.0 and < 1.0
 * \return  pixd 8 bpp, scaled down by scalefactor in each direction,
 *              or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) See notes in pixScaleToGray() for the basic approach.
 *      (2) This function is considerably less expensive than pixScaleToGray()
 *          for scalefactor in the range (0.0625 ... 0.5), and the
 *          quality is nearly as good.
 *      (3) Unlike pixScaleToGray(), which does binary upscaling before
 *          downscaling for scale factors >= 0.0625, pixScaleToGrayFast()
 *          first downscales in binary for all scale factors < 0.5, and
 *          then does a 2x scale-to-gray as the final step.  For
 *          scale factors < 0.0625, both do a 16x scale-to-gray, followed
 *          by further grayscale reduction.
 * </pre>
 */
PIX *
pixScaleToGrayFast(PIX       *pixs,
                   l_float32  scalefactor)
{
l_int32    w, h, minsrc, mindest;
l_float32  eps, factor;
PIX       *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, NULL);
    if (scalefactor <= 0.0f)
        return (PIX *)ERROR_PTR("scalefactor <= 0.0", __func__, NULL);
    if (scalefactor >= 1.0f)
        return (PIX *)ERROR_PTR("scalefactor >= 1.0", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    minsrc = L_MIN(w, h);
    mindest = (l_int32)((l_float32)minsrc * scalefactor);
    if (mindest < 2)
        return (PIX *)ERROR_PTR("scalefactor too small", __func__, NULL);
    eps = 0.0001f;

        /* Handle the special cases */
    if (scalefactor > 0.5f - eps && scalefactor < 0.5f + eps)
        return pixScaleToGray2(pixs);
    else if (scalefactor > 0.33333f - eps && scalefactor < 0.33333f + eps)
        return pixScaleToGray3(pixs);
    else if (scalefactor > 0.25f - eps && scalefactor < 0.25f + eps)
        return pixScaleToGray4(pixs);
    else if (scalefactor > 0.16666f - eps && scalefactor < 0.16666f + eps)
        return pixScaleToGray6(pixs);
    else if (scalefactor > 0.125f - eps && scalefactor < 0.125f + eps)
        return pixScaleToGray8(pixs);
    else if (scalefactor > 0.0625f - eps && scalefactor < 0.0625f + eps)
        return pixScaleToGray16(pixs);

    if (scalefactor > 0.0625f) {  /* scale binary first */
        factor = 2.0f * scalefactor;
        if ((pixt = pixScaleBinary(pixs, factor, factor)) == NULL)
            return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);
        pixd = pixScaleToGray2(pixt);
    } else {  /* scalefactor < 0.0625; scale-to-gray first */
        factor = 16.0f * scalefactor;  /* will be < 1.0 */
        if ((pixt = pixScaleToGray16(pixs)) == NULL)
            return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);
        if (factor < 0.7f)
            pixd = pixScaleSmooth(pixt, factor, factor);
        else
            pixd = pixScaleGrayLI(pixt, factor, factor);
    }
    pixDestroy(&pixt);
    if (!pixd)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyInputFormat(pixd, pixs);
    return pixd;
}


/*-----------------------------------------------------------------------*
 *          Scale-to-gray (1 bpp --> 8 bpp; integer downscaling)         *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixScaleToGray2()
 *
 * \param[in]    pixs     1 bpp
 * \return  pixd 8 bpp, scaled down by 2x in each direction,
 *              or NULL on error.
 */
PIX *
pixScaleToGray2(PIX  *pixs)
{
l_uint8   *valtab;
l_int32    ws, hs, wd, hd;
l_int32    wpld, wpls;
l_uint32  *sumtab;
l_uint32  *datas, *datad;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs must be 1 bpp", __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    wd = ws / 2;
    hd = hs / 2;
    if (wd == 0 || hd == 0)
        return (PIX *)ERROR_PTR("pixs too small", __func__, NULL);

    if ((pixd = pixCreate(wd, hd, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixSetPadBits(pixs, 0);
    pixCopyInputFormat(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, 0.5, 0.5);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);

    sumtab = makeSumTabSG2();
    valtab = makeValTabSG2();
    scaleToGray2Low(datad, wd, hd, wpld, datas, wpls, sumtab, valtab);
    LEPT_FREE(sumtab);
    LEPT_FREE(valtab);
    return pixd;
}


/*!
 * \brief   pixScaleToGray3()
 *
 * \param[in]    pixs     1 bpp
 * \return  pixd 8 bpp, scaled down by 3x in each direction,
 *              or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) Speed is about 100 x 10^6 src-pixels/sec/GHz.
 *          Another way to express this is it processes 1 src pixel
 *          in about 10 cycles.
 *      (2) The width of pixd is truncated is truncated to a factor of 8.
 * </pre>
 */
PIX *
pixScaleToGray3(PIX  *pixs)
{
l_uint8   *valtab;
l_int32    ws, hs, wd, hd;
l_int32    wpld, wpls;
l_uint32  *sumtab;
l_uint32  *datas, *datad;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    wd = (ws / 3) & 0xfffffff8;    /* truncate to factor of 8 */
    hd = hs / 3;
    if (wd == 0 || hd == 0)
        return (PIX *)ERROR_PTR("pixs too small", __func__, NULL);

    if ((pixd = pixCreate(wd, hd, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyInputFormat(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, 0.33333f, 0.33333f);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);

    sumtab = makeSumTabSG3();
    valtab = makeValTabSG3();
    scaleToGray3Low(datad, wd, hd, wpld, datas, wpls, sumtab, valtab);
    LEPT_FREE(sumtab);
    LEPT_FREE(valtab);
    return pixd;
}


/*!
 * \brief   pixScaleToGray4()
 *
 * \param[in]    pixs     1 bpp
 * \return  pixd 8 bpp, scaled down by 4x in each direction,
 *              or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) The width of pixd is truncated is truncated to a factor of 2.
 * </pre>
 */
PIX *
pixScaleToGray4(PIX  *pixs)
{
l_uint8   *valtab;
l_int32    ws, hs, wd, hd;
l_int32    wpld, wpls;
l_uint32  *sumtab;
l_uint32  *datas, *datad;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs must be 1 bpp", __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    wd = (ws / 4) & 0xfffffffe;    /* truncate to factor of 2 */
    hd = hs / 4;
    if (wd == 0 || hd == 0)
        return (PIX *)ERROR_PTR("pixs too small", __func__, NULL);

    if ((pixd = pixCreate(wd, hd, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyInputFormat(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, 0.25, 0.25);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);

    sumtab = makeSumTabSG4();
    valtab = makeValTabSG4();
    scaleToGray4Low(datad, wd, hd, wpld, datas, wpls, sumtab, valtab);
    LEPT_FREE(sumtab);
    LEPT_FREE(valtab);
    return pixd;
}



/*!
 * \brief   pixScaleToGray6()
 *
 * \param[in]    pixs     1 bpp
 * \return  pixd 8 bpp, scaled down by 6x in each direction,
 *              or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) The width of pixd is truncated is truncated to a factor of 8.
 * </pre>
 */
PIX *
pixScaleToGray6(PIX  *pixs)
{
l_uint8   *valtab;
l_int32    ws, hs, wd, hd, wpld, wpls;
l_int32   *tab8;
l_uint32  *datas, *datad;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    wd = (ws / 6) & 0xfffffff8;    /* truncate to factor of 8 */
    hd = hs / 6;
    if (wd == 0 || hd == 0)
        return (PIX *)ERROR_PTR("pixs too small", __func__, NULL);

    if ((pixd = pixCreate(wd, hd, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyInputFormat(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, 0.16667f, 0.16667f);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);

    tab8 = makePixelSumTab8();
    valtab = makeValTabSG6();
    scaleToGray6Low(datad, wd, hd, wpld, datas, wpls, tab8, valtab);
    LEPT_FREE(tab8);
    LEPT_FREE(valtab);
    return pixd;
}


/*!
 * \brief   pixScaleToGray8()
 *
 * \param[in]    pixs     1 bpp
 * \return  pixd 8 bpp, scaled down by 8x in each direction,
 *              or NULL on error
 */
PIX *
pixScaleToGray8(PIX  *pixs)
{
l_uint8   *valtab;
l_int32    ws, hs, wd, hd;
l_int32    wpld, wpls;
l_int32   *tab8;
l_uint32  *datas, *datad;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs must be 1 bpp", __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    wd = ws / 8;  /* truncate to nearest dest byte */
    hd = hs / 8;
    if (wd == 0 || hd == 0)
        return (PIX *)ERROR_PTR("pixs too small", __func__, NULL);

    if ((pixd = pixCreate(wd, hd, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyInputFormat(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, 0.125, 0.125);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);

    tab8 = makePixelSumTab8();
    valtab = makeValTabSG8();
    scaleToGray8Low(datad, wd, hd, wpld, datas, wpls, tab8, valtab);
    LEPT_FREE(tab8);
    LEPT_FREE(valtab);
    return pixd;
}


/*!
 * \brief   pixScaleToGray16()
 *
 * \param[in]    pixs     1 bpp
 * \return  pixd 8 bpp, scaled down by 16x in each direction,
 *              or NULL on error.
 */
PIX *
pixScaleToGray16(PIX  *pixs)
{
l_int32    ws, hs, wd, hd;
l_int32    wpld, wpls;
l_int32   *tab8;
l_uint32  *datas, *datad;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs must be 1 bpp", __func__, NULL);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    wd = ws / 16;
    hd = hs / 16;
    if (wd == 0 || hd == 0)
        return (PIX *)ERROR_PTR("pixs too small", __func__, NULL);

    if ((pixd = pixCreate(wd, hd, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyInputFormat(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, 0.0625, 0.0625);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);

    tab8 = makePixelSumTab8();
    scaleToGray16Low(datad, wd, hd, wpld, datas, wpls, tab8);
    LEPT_FREE(tab8);
    return pixd;
}


/*------------------------------------------------------------------*
 *    Scale-to-gray mipmap(1 bpp --> 8 bpp, arbitrary reduction)    *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixScaleToGrayMipmap()
 *
 * \param[in]    pixs          1 bpp
 * \param[in]    scalefactor   reduction: must be > 0.0 and < 1.0
 * \return  pixd 8 bpp, scaled down by scalefactor in each direction,
 *              or NULL on error.
 *
 * <pre>
 * Notes:
 *
 *  This function is here mainly for pedagogical reasons.
 *  Mip-mapping is widely used in graphics for texture mapping, because
 *  the texture changes smoothly with scale.  This is accomplished by
 *  constructing a multiresolution pyramid and, for each pixel,
 *  doing a linear interpolation between corresponding pixels in
 *  the two planes of the pyramid that bracket the desired resolution.
 *  The computation is very efficient, and is implemented in hardware
 *  in high-end graphics cards.
 *
 *  We can use mip-mapping for scale-to-gray by using two scale-to-gray
 *  reduced images (we don't need the entire pyramid) selected from
 *  the set {2x, 4x, ... 16x}, and interpolating.  However, we get
 *  severe aliasing, probably because we are subsampling from the
 *  higher resolution image.  The method is very fast, but the result
 *  is very poor.  In fact, the results don't look any better than
 *  either subsampling off the higher-res grayscale image or oversampling
 *  on the lower-res image.  Consequently, this method should NOT be used
 *  for generating reduced images, scale-to-gray or otherwise.
 * </pre>
 */
PIX *
pixScaleToGrayMipmap(PIX       *pixs,
                     l_float32  scalefactor)
{
l_int32    w, h, minsrc, mindest;
l_float32  red;
PIX       *pixs1, *pixs2, *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, NULL);
    if (scalefactor <= 0.0f)
        return (PIX *)ERROR_PTR("scalefactor <= 0.0", __func__, NULL);
    if (scalefactor >= 1.0f)
        return (PIX *)ERROR_PTR("scalefactor >= 1.0", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    minsrc = L_MIN(w, h);
    mindest = (l_int32)((l_float32)minsrc * scalefactor);
    if (mindest < 2)
        return (PIX *)ERROR_PTR("scalefactor too small", __func__, NULL);

    if (scalefactor > 0.5f) {
        pixs1 = pixConvert1To8(NULL, pixs, 255, 0);
        pixs2 = pixScaleToGray2(pixs);
        red = scalefactor;
    } else if (scalefactor == 0.5f) {
        return pixScaleToGray2(pixs);
    } else if (scalefactor > 0.25f) {
        pixs1 = pixScaleToGray2(pixs);
        pixs2 = pixScaleToGray4(pixs);
        red = 2.f * scalefactor;
    } else if (scalefactor == 0.25f) {
        return pixScaleToGray4(pixs);
    } else if (scalefactor > 0.125f) {
        pixs1 = pixScaleToGray4(pixs);
        pixs2 = pixScaleToGray8(pixs);
        red = 4.f * scalefactor;
    } else if (scalefactor == 0.125f) {
        return pixScaleToGray8(pixs);
    } else if (scalefactor > 0.0625f) {
        pixs1 = pixScaleToGray8(pixs);
        pixs2 = pixScaleToGray16(pixs);
        red = 8.f * scalefactor;
    } else if (scalefactor == 0.0625f) {
        return pixScaleToGray16(pixs);
    } else {  /* end of the pyramid; just do it */
        red = 16.0f * scalefactor;  /* will be <= 1.0 */
        if ((pixt = pixScaleToGray16(pixs)) == NULL)
            return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);
        if (red < 0.7f)
            pixd = pixScaleSmooth(pixt, red, red);
        else
            pixd = pixScaleGrayLI(pixt, red, red);
        pixDestroy(&pixt);
        return pixd;
    }

    pixd = pixScaleMipmap(pixs1, pixs2, red);
    pixCopyInputFormat(pixd, pixs);

    pixDestroy(&pixs1);
    pixDestroy(&pixs2);
    return pixd;
}


/*------------------------------------------------------------------*
 *                  Grayscale scaling using mipmap                  *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixScaleMipmap()
 *
 * \param[in]    pixs1    high res 8 bpp, no cmap
 * \param[in]    pixs2    low res -- 2x reduced -- 8 bpp, no cmap
 * \param[in]    scale    reduction with respect to high res image, > 0.5
 * \return  8 bpp pix, scaled down by reduction in each direction,
 *              or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) See notes in pixScaleToGrayMipmap().
 *      (2) This function suffers from aliasing effects that are
 *          easily seen in document images.
 * </pre>
 */
PIX *
pixScaleMipmap(PIX       *pixs1,
               PIX       *pixs2,
               l_float32  scale)
{
l_int32    ws1, hs1, ws2, hs2, wd, hd, wpls1, wpls2, wpld;
l_uint32  *datas1, *datas2, *datad;
PIX       *pixd;

    if (!pixs1 || pixGetDepth(pixs1) != 8 || pixGetColormap(pixs1))
        return (PIX *)ERROR_PTR("pixs1 underdefined, not 8 bpp, or cmapped",
                                __func__, NULL);
    if (!pixs2 || pixGetDepth(pixs2) != 8 || pixGetColormap(pixs2))
        return (PIX *)ERROR_PTR("pixs2 underdefined, not 8 bpp, or cmapped",
                                __func__, NULL);
    pixGetDimensions(pixs1, &ws1, &hs1, NULL);
    pixGetDimensions(pixs2, &ws2, &hs2, NULL);
    if (scale > 1.0f || scale < 0.5f)
        return (PIX *)ERROR_PTR("scale not in [0.5, 1.0]", __func__, NULL);
    if (ws1 < 2 * ws2)
        return (PIX *)ERROR_PTR("invalid width ratio", __func__, NULL);
    if (hs1 < 2 * hs2)
        return (PIX *)ERROR_PTR("invalid height ratio", __func__, NULL);

        /* Generate wd and hd from the lower resolution dimensions,
         * to guarantee staying within both src images */
    datas1 = pixGetData(pixs1);
    wpls1 = pixGetWpl(pixs1);
    datas2 = pixGetData(pixs2);
    wpls2 = pixGetWpl(pixs2);
    wd = (l_int32)(2 * scale * pixGetWidth(pixs2));
    hd = (l_int32)(2 * scale * pixGetHeight(pixs2));
    if ((pixd = pixCreate(wd, hd, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyInputFormat(pixd, pixs1);
    pixCopyResolution(pixd, pixs1);
    pixScaleResolution(pixd, scale, scale);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    scaleMipmapLow(datad, wd, hd, wpld, datas1, wpls1, datas2, wpls2, scale);
    return pixd;
}


/*------------------------------------------------------------------*
 *                  Replicated (integer) expansion                  *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixExpandReplicate()
 *
 * \param[in]    pixs     1, 2, 4, 8, 16, 32 bpp
 * \param[in]    factor   integer scale factor for replicative expansion
 * \return  pixd scaled up, or NULL on error.
 */
PIX *
pixExpandReplicate(PIX     *pixs,
                   l_int32  factor)
{
l_int32    w, h, d, wd, hd, wpls, wpld, start, i, j, k;
l_uint8    sval;
l_uint16   sval16;
l_uint32   sval32;
l_uint32  *lines, *datas, *lined, *datad;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 1 && d != 2 && d != 4 && d != 8 && d != 16 && d != 32)
        return (PIX *)ERROR_PTR("depth not in {1,2,4,8,16,32}", __func__, NULL);
    if (factor <= 0)
        return (PIX *)ERROR_PTR("factor <= 0; invalid", __func__, NULL);
    if (factor == 1)
        return pixCopy(NULL, pixs);

    if (d == 1)
        return pixExpandBinaryReplicate(pixs, factor, factor);

    wd = factor * w;
    hd = factor * h;
    if ((pixd = pixCreate(wd, hd, d)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyColormap(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, (l_float32)factor, (l_float32)factor);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    switch (d) {
    case 2:
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + factor * i * wpld;
            for (j = 0; j < w; j++) {
                sval = GET_DATA_DIBIT(lines, j);
                start = factor * j;
                for (k = 0; k < factor; k++)
                    SET_DATA_DIBIT(lined, start + k, sval);
            }
            for (k = 1; k < factor; k++)
                memcpy(lined + k * wpld, lined, 4 * wpld);
        }
        break;
    case 4:
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + factor * i * wpld;
            for (j = 0; j < w; j++) {
                sval = GET_DATA_QBIT(lines, j);
                start = factor * j;
                for (k = 0; k < factor; k++)
                    SET_DATA_QBIT(lined, start + k, sval);
            }
            for (k = 1; k < factor; k++)
                memcpy(lined + k * wpld, lined, 4 * wpld);
        }
        break;
    case 8:
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + factor * i * wpld;
            for (j = 0; j < w; j++) {
                sval = GET_DATA_BYTE(lines, j);
                start = factor * j;
                for (k = 0; k < factor; k++)
                    SET_DATA_BYTE(lined, start + k, sval);
            }
            for (k = 1; k < factor; k++)
                memcpy(lined + k * wpld, lined, 4 * wpld);
        }
        break;
    case 16:
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + factor * i * wpld;
            for (j = 0; j < w; j++) {
                sval16 = GET_DATA_TWO_BYTES(lines, j);
                start = factor * j;
                for (k = 0; k < factor; k++)
                    SET_DATA_TWO_BYTES(lined, start + k, sval16);
            }
            for (k = 1; k < factor; k++)
                memcpy(lined + k * wpld, lined, 4 * wpld);
        }
        break;
    case 32:
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + factor * i * wpld;
            for (j = 0; j < w; j++) {
                sval32 = *(lines + j);
                start = factor * j;
                for (k = 0; k < factor; k++)
                    *(lined + start + k) = sval32;
            }
            for (k = 1; k < factor; k++)
                memcpy(lined + k * wpld, lined, 4 * wpld);
        }
        break;
    default:
        lept_stderr("invalid depth\n");
    }

    if (d == 32 && pixGetSpp(pixs) == 4)
        pixScaleAndTransferAlpha(pixd, pixs, (l_float32)factor,
                                 (l_float32)factor);
    return pixd;
}


/*-----------------------------------------------------------------------*
 *                    Downscaling using min or max                       *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixScaleGrayMinMax()
 *
 * \param[in]    pixs     8 bpp, not cmapped
 * \param[in]    xfact    x downscaling factor; integer
 * \param[in]    yfact    y downscaling factor; integer
 * \param[in]    type     L_CHOOSE_MIN, L_CHOOSE_MAX, L_CHOOSE_MAXDIFF
 * \return  pixd 8 bpp
 *
 * <pre>
 * Notes:
 *      (1) The downscaled pixels in pixd are the min, max or (max - min)
 *          of the corresponding set of xfact * yfact pixels in pixs.
 *      (2) Using L_CHOOSE_MIN is equivalent to a grayscale erosion,
 *          using a brick Sel of size (xfact * yfact), followed by
 *          subsampling within each (xfact * yfact) cell.  Using
 *          L_CHOOSE_MAX is equivalent to the corresponding dilation.
 *      (3) Using L_CHOOSE_MAXDIFF finds the difference between max
 *          and min values in each cell.
 *      (4) For the special case of downscaling by 2x in both directions,
 *          pixScaleGrayMinMax2() is about 2x more efficient.
 * </pre>
 */
PIX *
pixScaleGrayMinMax(PIX     *pixs,
                   l_int32  xfact,
                   l_int32  yfact,
                   l_int32  type)
{
l_int32    ws, hs, wd, hd, wpls, wpld, i, j, k, m;
l_int32    minval, maxval, val;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;

    if (!pixs || pixGetDepth(pixs) != 8 || pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs undefined, not 8 bpp, or cmapped",
                                __func__, NULL);
    pixGetDimensions(pixs, &ws, &hs, NULL);
    if (type != L_CHOOSE_MIN && type != L_CHOOSE_MAX &&
        type != L_CHOOSE_MAXDIFF)
        return (PIX *)ERROR_PTR("invalid type", __func__, NULL);
    if (xfact < 1 || yfact < 1)
        return (PIX *)ERROR_PTR("xfact and yfact must be >= 1", __func__, NULL);

    if (xfact == 2 && yfact == 2)
        return pixScaleGrayMinMax2(pixs, type);

    wd = ws / xfact;
    if (wd == 0) {  /* single tile */
        wd = 1;
        xfact = ws;
    }
    hd = hs / yfact;
    if (hd == 0) {  /* single tile */
        hd = 1;
        yfact = hs;
    }
    if ((pixd = pixCreate(wd, hd, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyInputFormat(pixd, pixs);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < hd; i++) {
        lined = datad + i * wpld;
        for (j = 0; j < wd; j++) {
            if (type == L_CHOOSE_MIN || type == L_CHOOSE_MAXDIFF) {
                minval = 255;
                for (k = 0; k < yfact; k++) {
                    lines = datas + (yfact * i + k) * wpls;
                    for (m = 0; m < xfact; m++) {
                        val = GET_DATA_BYTE(lines, xfact * j + m);
                        if (val < minval)
                            minval = val;
                    }
                }
            }
            if (type == L_CHOOSE_MAX || type == L_CHOOSE_MAXDIFF) {
                maxval = 0;
                for (k = 0; k < yfact; k++) {
                    lines = datas + (yfact * i + k) * wpls;
                    for (m = 0; m < xfact; m++) {
                        val = GET_DATA_BYTE(lines, xfact * j + m);
                        if (val > maxval)
                            maxval = val;
                    }
                }
            }
            if (type == L_CHOOSE_MIN)
                SET_DATA_BYTE(lined, j, minval);
            else if (type == L_CHOOSE_MAX)
                SET_DATA_BYTE(lined, j, maxval);
            else  /* type == L_CHOOSE_MAXDIFF */
                SET_DATA_BYTE(lined, j, maxval - minval);
        }
    }

    return pixd;
}


/*!
 * \brief   pixScaleGrayMinMax2()
 *
 * \param[in]    pixs    8 bpp, not cmapped
 * \param[in]    type    L_CHOOSE_MIN, L_CHOOSE_MAX, L_CHOOSE_MAXDIFF
 * \return  pixd 8 bpp downscaled by 2x
 *
 * <pre>
 * Notes:
 *      (1) Special version for 2x reduction.  The downscaled pixels
 *          in pixd are the min, max or (max - min) of the corresponding
 *          set of 4 pixels in pixs.
 *      (2) The max and min operations are a special case (for levels 1
 *          and 4) of grayscale analog to the binary rank scaling operation
 *          pixReduceRankBinary2().  Note, however, that because of
 *          the photometric definition that higher gray values are
 *          lighter, the erosion-like L_CHOOSE_MIN will darken
 *          the resulting image, corresponding to a threshold level 1
 *          in the binary case.  Likewise, L_CHOOSE_MAX will lighten
 *          the pixd, corresponding to a threshold level of 4.
 *      (3) To choose any of the four rank levels in a 2x grayscale
 *          reduction, use pixScaleGrayRank2().
 *      (4) This runs at about 70 MPix/sec/GHz of source data for
 *          erosion and dilation.
 * </pre>
 */
PIX *
pixScaleGrayMinMax2(PIX     *pixs,
                    l_int32  type)
{
l_int32    ws, hs, wd, hd, wpls, wpld, i, j, k;
l_int32    minval, maxval;
l_int32    val[4];
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;

    if (!pixs || pixGetDepth(pixs) != 8 || pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs undefined, not 8 bpp, or cmapped",
                                __func__, NULL);
    pixGetDimensions(pixs, &ws, &hs, NULL);
    if (ws < 2 || hs < 2)
        return (PIX *)ERROR_PTR("too small: ws < 2 or hs < 2", __func__, NULL);
    if (type != L_CHOOSE_MIN && type != L_CHOOSE_MAX &&
        type != L_CHOOSE_MAXDIFF)
        return (PIX *)ERROR_PTR("invalid type", __func__, NULL);

    wd = ws / 2;
    hd = hs / 2;
    if ((pixd = pixCreate(wd, hd, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyInputFormat(pixd, pixs);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < hd; i++) {
        lines = datas + 2 * i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < wd; j++) {
            val[0] = GET_DATA_BYTE(lines, 2 * j);
            val[1] = GET_DATA_BYTE(lines, 2 * j + 1);
            val[2] = GET_DATA_BYTE(lines + wpls, 2 * j);
            val[3] = GET_DATA_BYTE(lines + wpls, 2 * j + 1);
            if (type == L_CHOOSE_MIN || type == L_CHOOSE_MAXDIFF) {
                minval = 255;
                for (k = 0; k < 4; k++) {
                    if (val[k] < minval)
                        minval = val[k];
                }
            }
            if (type == L_CHOOSE_MAX || type == L_CHOOSE_MAXDIFF) {
                maxval = 0;
                for (k = 0; k < 4; k++) {
                    if (val[k] > maxval)
                        maxval = val[k];
                }
            }
            if (type == L_CHOOSE_MIN)
                SET_DATA_BYTE(lined, j, minval);
            else if (type == L_CHOOSE_MAX)
                SET_DATA_BYTE(lined, j, maxval);
            else  /* type == L_CHOOSE_MAXDIFF */
                SET_DATA_BYTE(lined, j, maxval - minval);
        }
    }

    return pixd;
}


/*-----------------------------------------------------------------------*
 *                  Grayscale downscaling using rank value               *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixScaleGrayRankCascade()
 *
 * \param[in]    pixs    8 bpp, not cmapped
 * \param[in]    level1, level2    ...
 * \param[in]    level3, level4    rank thresholds, in set {0, 1, 2, 3, 4}
 * \return  pixd 8 bpp, downscaled by up to 16x
 *
 * <pre>
 * Notes:
 *      (1) This performs up to four cascaded 2x rank reductions.
 *      (2) Use level = 0 to truncate the cascade.
 * </pre>
 */
PIX *
pixScaleGrayRankCascade(PIX     *pixs,
                        l_int32  level1,
                        l_int32  level2,
                        l_int32  level3,
                        l_int32  level4)
{
PIX  *pixt1, *pixt2, *pixt3, *pixt4;

    if (!pixs || pixGetDepth(pixs) != 8 || pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs undefined, not 8 bpp, or cmapped",
                                __func__, NULL);
    if (level1 > 4 || level2 > 4 || level3 > 4 || level4 > 4)
        return (PIX *)ERROR_PTR("levels must not exceed 4", __func__, NULL);

    if (level1 <= 0) {
        L_WARNING("no reduction because level1 not > 0\n", __func__);
        return pixCopy(NULL, pixs);
    }

    pixt1 = pixScaleGrayRank2(pixs, level1);
    if (level2 <= 0)
        return pixt1;

    pixt2 = pixScaleGrayRank2(pixt1, level2);
    pixDestroy(&pixt1);
    if (level3 <= 0)
        return pixt2;

    pixt3 = pixScaleGrayRank2(pixt2, level3);
    pixDestroy(&pixt2);
    if (level4 <= 0)
        return pixt3;

    pixt4 = pixScaleGrayRank2(pixt3, level4);
    pixDestroy(&pixt3);
    return pixt4;
}


/*!
 * \brief   pixScaleGrayRank2()
 *
 * \param[in]    pixs    8 bpp, no cmap
 * \param[in]    rank    1 (darkest), 2, 3, 4 (lightest)
 * \return  pixd 8 bpp, downscaled by 2x
 *
 * <pre>
 * Notes:
 *      (1) Rank 2x reduction.  If rank == 1(4), the downscaled pixels
 *          in pixd are the min(max) of the corresponding set of
 *          4 pixels in pixs.  Values 2 and 3 are intermediate.
 *      (2) This is the grayscale analog to the binary rank scaling operation
 *          pixReduceRankBinary2().  Here, because of the photometric
 *          definition that higher gray values are lighter, rank 1 gives
 *          the darkest pixel, whereas rank 4 gives the lightest pixel.
 *          This is opposite to the binary rank operation.
 *      (3) For rank = 1 and 4, this calls pixScaleGrayMinMax2(),
 *          which runs at about 70 MPix/sec/GHz of source data.
 *          For rank 2 and 3, this runs 3x slower, at about 25 MPix/sec/GHz.
 * </pre>
 */
PIX *
pixScaleGrayRank2(PIX     *pixs,
                  l_int32  rank)
{
l_int32    ws, hs, wd, hd, wpls, wpld, i, j, k, m;
l_int32    minval, maxval, rankval, minindex, maxindex;
l_int32    val[4];
l_int32    midval[4];  /* should only use 2 of these */
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;

    if (!pixs || pixGetDepth(pixs) != 8 || pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs undefined, not 8 bpp, or cmapped",
                                __func__, NULL);
    if (rank < 1 || rank > 4)
        return (PIX *)ERROR_PTR("invalid rank", __func__, NULL);

    if (rank == 1)
        return pixScaleGrayMinMax2(pixs, L_CHOOSE_MIN);
    if (rank == 4)
        return pixScaleGrayMinMax2(pixs, L_CHOOSE_MAX);

    pixGetDimensions(pixs, &ws, &hs, NULL);
    wd = ws / 2;
    hd = hs / 2;
    if ((pixd = pixCreate(wd, hd, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyInputFormat(pixd, pixs);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < hd; i++) {
        lines = datas + 2 * i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < wd; j++) {
            val[0] = GET_DATA_BYTE(lines, 2 * j);
            val[1] = GET_DATA_BYTE(lines, 2 * j + 1);
            val[2] = GET_DATA_BYTE(lines + wpls, 2 * j);
            val[3] = GET_DATA_BYTE(lines + wpls, 2 * j + 1);
            minval = maxval = val[0];
            minindex = maxindex = 0;
            for (k = 1; k < 4; k++) {
                if (val[k] < minval) {
                    minval = val[k];
                    minindex = k;
                    continue;
                }
                if (val[k] > maxval) {
                    maxval = val[k];
                    maxindex = k;
                }
            }
            for (k = 0, m = 0; k < 4; k++) {
                if (k == minindex || k == maxindex)
                    continue;
                midval[m++] = val[k];
            }
            if (m > 2)  /* minval == maxval; all val[k] are the same */
                rankval = minval;
            else if (rank == 2)
                rankval = L_MIN(midval[0], midval[1]);
            else  /* rank == 3 */
                rankval = L_MAX(midval[0], midval[1]);
            SET_DATA_BYTE(lined, j, rankval);
        }
    }

    return pixd;
}


/*------------------------------------------------------------------------*
 *           Helper function for transferring alpha with scaling          *
 *------------------------------------------------------------------------*/
/*!
 * \brief   pixScaleAndTransferAlpha()
 *
 * \param[in]    pixd      32 bpp, scaled image
 * \param[in]    pixs      32 bpp, original unscaled image
 * \param[in]    scalex    must be > 0.0
 * \param[in]    scaley    must be > 0.0
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This scales the alpha component of pixs and inserts into pixd.
 * </pre>
 */
l_ok
pixScaleAndTransferAlpha(PIX       *pixd,
                         PIX       *pixs,
                         l_float32  scalex,
                         l_float32  scaley)
{
PIX  *pix1, *pix2;

    if (!pixs || !pixd)
        return ERROR_INT("pixs and pixd not both defined", __func__, 1);
    if (pixGetDepth(pixs) != 32 || pixGetSpp(pixs) != 4)
        return ERROR_INT("pixs not 32 bpp and 4 spp", __func__, 1);
    if (pixGetDepth(pixd) != 32)
        return ERROR_INT("pixd not 32 bpp", __func__, 1);

    if (scalex == 1.0f && scaley == 1.0f) {
        pixCopyRGBComponent(pixd, pixs, L_ALPHA_CHANNEL);
        return 0;
    }

    pix1 = pixGetRGBComponent(pixs, L_ALPHA_CHANNEL);
    pix2 = pixScale(pix1, scalex, scaley);
    pixSetRGBComponent(pixd, pix2, L_ALPHA_CHANNEL);
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    return 0;
}


/*------------------------------------------------------------------------*
 *    RGB scaling including alpha (blend) component and gamma transform   *
 *------------------------------------------------------------------------*/
/*!
 * \brief   pixScaleWithAlpha()
 *
 * \param[in]    pixs      32 bpp rgb or cmapped
 * \param[in]    scalex    must be > 0.0
 * \param[in]    scaley    must be > 0.0
 * \param[in]    pixg      [optional] 8 bpp, can be null
 * \param[in]    fract     between 0.0 and 1.0, with 0.0 fully transparent
 *                         and 1.0 fully opaque
 * \return  pixd 32 bpp rgba, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The alpha channel is transformed separately from pixs,
 *          and aligns with it, being fully transparent outside the
 *          boundary of the transformed pixs.  For pixels that are fully
 *          transparent, a blending function like pixBlendWithGrayMask()
 *          will give zero weight to corresponding pixels in pixs.
 *      (2) Scaling is done with area mapping or linear interpolation,
 *          depending on the scale factors.  Default sharpening is done.
 *      (3) If pixg is NULL, it is generated as an alpha layer that is
 *          partially opaque, using %fract.  Otherwise, it is cropped
 *          to pixs if required, and %fract is ignored.  The alpha
 *          channel in pixs is never used.
 *      (4) Colormaps are removed to 32 bpp.
 *      (5) The default setting for the border values in the alpha channel
 *          is 0 (transparent) for the outermost ring of pixels and
 *          (0.5 * fract * 255) for the second ring.  When blended over
 *          a second image, this
 *          (a) shrinks the visible image to make a clean overlap edge
 *              with an image below, and
 *          (b) softens the edges by weakening the aliasing there.
 *          Use l_setAlphaMaskBorder() to change these values.
 *      (6) A subtle use of gamma correction is to remove gamma correction
 *          before scaling and restore it afterwards.  This is done
 *          by sandwiching this function between a gamma/inverse-gamma
 *          photometric transform:
 *              pixt = pixGammaTRCWithAlpha(NULL, pixs, 1.0 / gamma, 0, 255);
 *              pixd = pixScaleWithAlpha(pixt, scalex, scaley, NULL, fract);
 *              pixGammaTRCWithAlpha(pixd, pixd, gamma, 0, 255);
 *              pixDestroy(&pixt);
 *          This has the side-effect of producing artifacts in the very
 *          dark regions.
 * </pre>
 */
PIX *
pixScaleWithAlpha(PIX       *pixs,
                  l_float32  scalex,
                  l_float32  scaley,
                  PIX       *pixg,
                  l_float32  fract)
{
l_int32  ws, hs, d, spp;
PIX     *pixd, *pix32, *pixg2, *pixgs;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &ws, &hs, &d);
    if (d != 32 && !pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs not cmapped or 32 bpp", __func__, NULL);
    if (scalex <= 0.0f || scaley <= 0.0f)
        return (PIX *)ERROR_PTR("scale factor <= 0.0", __func__, NULL);
    if (pixg && pixGetDepth(pixg) != 8) {
        L_WARNING("pixg not 8 bpp; using 'fract' transparent alpha\n",
                  __func__);
        pixg = NULL;
    }
    if (!pixg && (fract < 0.0f || fract > 1.0f)) {
        L_WARNING("invalid fract; using fully opaque\n", __func__);
        fract = 1.0;
    }
    if (!pixg && fract == 0.0f)
        L_WARNING("transparent alpha; image will not be blended\n", __func__);

        /* Make sure input to scaling is 32 bpp rgb, and scale it */
    if (d != 32)
        pix32 = pixConvertTo32(pixs);
    else
        pix32 = pixClone(pixs);
    spp = pixGetSpp(pix32);
    pixSetSpp(pix32, 3);  /* ignore the alpha channel for scaling */
    pixd = pixScale(pix32, scalex, scaley);
    pixSetSpp(pix32, spp);  /* restore initial value in case it's a clone */
    pixDestroy(&pix32);

        /* Set up alpha layer with a fading border and scale it */
    if (!pixg) {
        pixg2 = pixCreate(ws, hs, 8);
        if (fract == 1.0f)
            pixSetAll(pixg2);
        else if (fract > 0.0f)
            pixSetAllArbitrary(pixg2, (l_int32)(255 * fract));
    } else {
        pixg2 = pixResizeToMatch(pixg, NULL, ws, hs);
    }
    if (ws > 10 && hs > 10) {  /* see note 4 */
        pixSetBorderRingVal(pixg2, 1,
                            (l_int32)(255 * fract * AlphaMaskBorderVals[0]));
        pixSetBorderRingVal(pixg2, 2,
                            (l_int32)(255 * fract * AlphaMaskBorderVals[1]));
    }
    pixgs = pixScaleGeneral(pixg2, scalex, scaley, 0.0f, 0);

        /* Combine into a 4 spp result */
    pixSetRGBComponent(pixd, pixgs, L_ALPHA_CHANNEL);
    pixCopyInputFormat(pixd, pixs);

    pixDestroy(&pixg2);
    pixDestroy(&pixgs);
    return pixd;
}


/* ================================================================ *
 *                    Low level static functions                    *
 * ================================================================ */

/*------------------------------------------------------------------*
 *                         Scale-to-gray 2x                         *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleToGray2Low()
 *
 * \param[in]    datad     dest data
 * \param[in]    wd, hd    dest width, height
 * \param[in]    wpld      dest words/line
 * \param[in]    datas     src data
 * \param[in]    wpls      src words/line
 * \param[in]    sumtab    made from makeSumTabSG2()
 * \param[in]    valtab    made from makeValTabSG2()
 * \return  0 if OK; 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) The output is processed in sets of 4 output bytes on a row,
 *          corresponding to 4 2x2 bit-blocks in the input image.
 *          Two lookup tables are used.  The first, sumtab, gets the
 *          sum of ON pixels in 4 sets of two adjacent bits,
 *          storing the result in 4 adjacent bytes.  After sums from
 *          two rows have been added, the second table, valtab,
 *          converts from the sum of ON pixels in the 2x2 block to
 *          an 8 bpp grayscale value between 0 for 4 bits ON
 *          and 255 for 0 bits ON.
 * </pre>
 */
static void
scaleToGray2Low(l_uint32  *datad,
                l_int32    wd,
                l_int32    hd,
                l_int32    wpld,
                l_uint32  *datas,
                l_int32    wpls,
                l_uint32  *sumtab,
                l_uint8   *valtab)
{
l_int32    i, j, l, k, m, wd4, extra;
l_uint32   sbyte1, sbyte2, sum;
l_uint32  *lines, *lined;

        /* i indexes the dest lines
         * l indexes the source lines
         * j indexes the dest bytes
         * k indexes the source bytes
         * We take two bytes from the source (in 2 lines of 8 pixels
         * each) and convert them into four 8 bpp bytes of the dest. */
    wd4 = wd & 0xfffffffc;
    extra = wd - wd4;
    for (i = 0, l = 0; i < hd; i++, l += 2) {
        lines = datas + l * wpls;
        lined = datad + i * wpld;
        for (j = 0, k = 0; j < wd4; j += 4, k++) {
            sbyte1 = GET_DATA_BYTE(lines, k);
            sbyte2 = GET_DATA_BYTE(lines + wpls, k);
            sum = sumtab[sbyte1] + sumtab[sbyte2];
            SET_DATA_BYTE(lined, j, valtab[sum >> 24]);
            SET_DATA_BYTE(lined, j + 1, valtab[(sum >> 16) & 0xff]);
            SET_DATA_BYTE(lined, j + 2, valtab[(sum >> 8) & 0xff]);
            SET_DATA_BYTE(lined, j + 3, valtab[sum & 0xff]);
        }
        if (extra > 0) {
            sbyte1 = GET_DATA_BYTE(lines, k);
            sbyte2 = GET_DATA_BYTE(lines + wpls, k);
            sum = sumtab[sbyte1] + sumtab[sbyte2];
            for (m = 0; m < extra; m++) {
                SET_DATA_BYTE(lined, j + m,
                              valtab[((sum >> (24 - 8 * m)) & 0xff)]);
            }
        }

    }
}


/*!
 * \brief   makeSumTabSG2()
 *
 * <pre>
 * Notes:
 *      (1) Returns a table of 256 l_uint32s, giving the four output
 *          8-bit grayscale sums corresponding to 8 input bits of a binary
 *          image, for a 2x scale-to-gray op.  The sums from two
 *          adjacent scanlines are then added and transformed to
 *          output four 8 bpp pixel values, using makeValTabSG2().
 * </pre>
 */
static l_uint32  *
makeSumTabSG2(void)
{
l_int32    i;
l_int32    sum[] = {0, 1, 1, 2};
l_uint32  *tab;

        /* Pack the four sums separately in four bytes */
    tab = (l_uint32 *)LEPT_CALLOC(256, sizeof(l_uint32));
    for (i = 0; i < 256; i++) {
        tab[i] = (sum[i & 0x3] | sum[(i >> 2) & 0x3] << 8 |
                  sum[(i >> 4) & 0x3] << 16 | sum[(i >> 6) & 0x3] << 24);
    }
    return tab;
}


/*!
 * \brief   makeValTabSG2()
 *
 * <pre>
 * Notes:
 *      (1) Returns an 8 bit value for the sum of ON pixels
 *          in a 2x2 square, according to
 *               val = 255 - (255 * sum)/4
 *          where sum is in set {0,1,2,3,4}
 * </pre>
 */
static l_uint8 *
makeValTabSG2(void)
{
l_int32   i;
l_uint8  *tab;

    tab = (l_uint8 *)LEPT_CALLOC(5, sizeof(l_uint8));
    for (i = 0; i < 5; i++)
        tab[i] = 255 - (i * 255) / 4;
    return tab;
}


/*------------------------------------------------------------------*
 *                         Scale-to-gray 3x                         *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleToGray3Low()
 *
 * \param[in]    datad     dest data
 * \param[in]    wd, hd    dest width, height
 * \param[in]    wpld      dest words/line
 * \param[in]    datas     src data
 * \param[in]    wpls      src words/line
 * \param[in]    sumtab    made from makeSumTabSG3()
 * \param[in]    valtab    made from makeValTabSG3()
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Each set of 8 3x3 bit-blocks in the source image, which
 *          consist of 72 pixels arranged 24 pixels wide by 3 scanlines,
 *          is converted to a row of 8 8-bit pixels in the dest image.
 *          These 72 pixels of the input image are runs of 24 pixels
 *          in three adjacent scanlines.  Each run of 24 pixels is
 *          stored in the 24 LSbits of a 32-bit word.  We use 2 LUTs.
 *          The first, sumtab, takes 6 of these bits and stores
 *          sum, taken 3 bits at a time, in two bytes.  (See
 *          makeSumTabSG3).  This is done for each of the 3 scanlines,
 *          and the results are added.  We now have the sum of ON pixels
 *          in the first two 3x3 blocks in two bytes.  The valtab LUT
 *          then converts these values (which go from 0 to 9) to
 *          grayscale values between between 255 and 0.  (See makeValTabSG3).
 *          This process is repeated for each of the other 3 sets of
 *          6x3 input pixels, giving 8 output pixels in total.
 *      (2) Note: because the input image is processed in groups of
 *           24 x 3 pixels, the process clips the input height to
 *           (h - h % 3) and the input width to (w - w % 24).
 * </pre>
 */
static void
scaleToGray3Low(l_uint32  *datad,
                l_int32    wd,
                l_int32    hd,
                l_int32    wpld,
                l_uint32  *datas,
                l_int32    wpls,
                l_uint32  *sumtab,
                l_uint8   *valtab)
{
l_int32    i, j, l, k;
l_uint32   threebytes1, threebytes2, threebytes3, sum;
l_uint32  *lines, *lined;

        /* i indexes the dest lines
         * l indexes the source lines
         * j indexes the dest bytes
         * k indexes the source bytes
         * We take 9 bytes from the source (72 binary pixels
         * in three lines of 24 pixels each) and convert it
         * into 8 bytes of the dest (8 8bpp pixels in one line)   */
    for (i = 0, l = 0; i < hd; i++, l += 3) {
        lines = datas + l * wpls;
        lined = datad + i * wpld;
        for (j = 0, k = 0; j < wd; j += 8, k += 3) {
            threebytes1 = (GET_DATA_BYTE(lines, k) << 16) |
                          (GET_DATA_BYTE(lines, k + 1) << 8) |
                          GET_DATA_BYTE(lines, k + 2);
            threebytes2 = (GET_DATA_BYTE(lines + wpls, k) << 16) |
                          (GET_DATA_BYTE(lines + wpls, k + 1) << 8) |
                          GET_DATA_BYTE(lines + wpls, k + 2);
            threebytes3 = (GET_DATA_BYTE(lines + 2 * wpls, k) << 16) |
                          (GET_DATA_BYTE(lines + 2 * wpls, k + 1) << 8) |
                          GET_DATA_BYTE(lines + 2 * wpls, k + 2);

            sum = sumtab[(threebytes1 >> 18)] +
                  sumtab[(threebytes2 >> 18)] +
                  sumtab[(threebytes3 >> 18)];
            SET_DATA_BYTE(lined, j, valtab[GET_DATA_BYTE(&sum, 2)]);
            SET_DATA_BYTE(lined, j + 1, valtab[GET_DATA_BYTE(&sum, 3)]);

            sum = sumtab[((threebytes1 >> 12) & 0x3f)] +
                  sumtab[((threebytes2 >> 12) & 0x3f)] +
                  sumtab[((threebytes3 >> 12) & 0x3f)];
            SET_DATA_BYTE(lined, j + 2, valtab[GET_DATA_BYTE(&sum, 2)]);
            SET_DATA_BYTE(lined, j + 3, valtab[GET_DATA_BYTE(&sum, 3)]);

            sum = sumtab[((threebytes1 >> 6) & 0x3f)] +
                  sumtab[((threebytes2 >> 6) & 0x3f)] +
                  sumtab[((threebytes3 >> 6) & 0x3f)];
            SET_DATA_BYTE(lined, j + 4, valtab[GET_DATA_BYTE(&sum, 2)]);
            SET_DATA_BYTE(lined, j + 5, valtab[GET_DATA_BYTE(&sum, 3)]);

            sum = sumtab[(threebytes1 & 0x3f)] +
                  sumtab[(threebytes2 & 0x3f)] +
                  sumtab[(threebytes3 & 0x3f)];
            SET_DATA_BYTE(lined, j + 6, valtab[GET_DATA_BYTE(&sum, 2)]);
            SET_DATA_BYTE(lined, j + 7, valtab[GET_DATA_BYTE(&sum, 3)]);
        }
    }
}



/*!
 * \brief   makeSumTabSG3()
 *
 * <pre>
 * Notes:
 *      (1) Returns a table of 64 l_uint32s, giving the two output
 *          8-bit grayscale sums corresponding to 6 input bits of a binary
 *          image, for a 3x scale-to-gray op.  In practice, this would
 *          be used three times (on adjacent scanlines), and the sums would
 *          be added and then transformed to output 8 bpp pixel values,
 *          using makeValTabSG3().
 * </pre>
 */
static l_uint32  *
makeSumTabSG3(void)
{
l_int32    i;
l_int32    sum[] = {0, 1, 1, 2, 1, 2, 2, 3};
l_uint32  *tab;

        /* Pack the two sums separately in two bytes */
    tab = (l_uint32 *)LEPT_CALLOC(64, sizeof(l_uint32));
    for (i = 0; i < 64; i++) {
        tab[i] = (sum[i & 0x07]) | (sum[(i >> 3) & 0x07] << 8);
    }
    return tab;
}


/*!
 * \brief   makeValTabSG3()
 *
 * <pre>
 * Notes:
 *      (1) Returns an 8 bit value for the sum of ON pixels
 *          in a 3x3 square, according to
 *               val = 255 - (255 * sum)/9
 *          where sum is in [0,...,9]
 * </pre>
 */
static l_uint8 *
makeValTabSG3(void)
{
l_int32   i;
l_uint8  *tab;

    tab = (l_uint8 *)LEPT_CALLOC(10, sizeof(l_uint8));
    for (i = 0; i < 10; i++)
        tab[i] = 0xff - (i * 255) / 9;
    return tab;
}


/*------------------------------------------------------------------*
 *                         Scale-to-gray 4x                         *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleToGray4Low()
 *
 * \param[in]    datad     dest data
 * \param[in]    wd, hd    dest width, height
 * \param[in]    wpld      dest words/line
 * \param[in]    datas     src data
 * \param[in]    wpls      src words/line
 * \param[in]    sumtab    made from makeSumTabSG4()
 * \param[in]    valtab    made from makeValTabSG4()
 * \return  0 if OK; 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) The output is processed in sets of 2 output bytes on a row,
 *          corresponding to 2 4x4 bit-blocks in the input image.
 *          Two lookup tables are used.  The first, sumtab, gets the
 *          sum of ON pixels in two sets of four adjacent bits,
 *          storing the result in 2 adjacent bytes.  After sums from
 *          four rows have been added, the second table, valtab,
 *          converts from the sum of ON pixels in the 4x4 block to
 *          an 8 bpp grayscale value between 0 for 16 bits ON
 *          and 255 for 0 bits ON.
 * </pre>
 */
static void
scaleToGray4Low(l_uint32  *datad,
                l_int32    wd,
                l_int32    hd,
                l_int32    wpld,
                l_uint32  *datas,
                l_int32    wpls,
                l_uint32  *sumtab,
                l_uint8   *valtab)
{
l_int32    i, j, l, k;
l_uint32   sbyte1, sbyte2, sbyte3, sbyte4, sum;
l_uint32  *lines, *lined;

        /* i indexes the dest lines
         * l indexes the source lines
         * j indexes the dest bytes
         * k indexes the source bytes
         * We take four bytes from the source (in 4 lines of 8 pixels
         * each) and convert it into two 8 bpp bytes of the dest. */
    for (i = 0, l = 0; i < hd; i++, l += 4) {
        lines = datas + l * wpls;
        lined = datad + i * wpld;
        for (j = 0, k = 0; j < wd; j += 2, k++) {
            sbyte1 = GET_DATA_BYTE(lines, k);
            sbyte2 = GET_DATA_BYTE(lines + wpls, k);
            sbyte3 = GET_DATA_BYTE(lines + 2 * wpls, k);
            sbyte4 = GET_DATA_BYTE(lines + 3 * wpls, k);
            sum = sumtab[sbyte1] + sumtab[sbyte2] +
                  sumtab[sbyte3] + sumtab[sbyte4];
            SET_DATA_BYTE(lined, j, valtab[GET_DATA_BYTE(&sum, 2)]);
            SET_DATA_BYTE(lined, j + 1, valtab[GET_DATA_BYTE(&sum, 3)]);
        }
    }
}


/*!
 * \brief   makeSumTabSG4()
 *
 * <pre>
 * Notes:
 *      (1) Returns a table of 256 l_uint32s, giving the two output
 *          8-bit grayscale sums corresponding to 8 input bits of a
 *          binary image, for a 4x scale-to-gray op.  The sums from
 *          four adjacent scanlines are then added and transformed to
 *          output 8 bpp pixel values, using makeValTabSG4().
 * </pre>
 */
static l_uint32  *
makeSumTabSG4(void)
{
l_int32    i;
l_int32    sum[] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
l_uint32  *tab;

        /* Pack the two sums separately in two bytes */
    tab = (l_uint32 *)LEPT_CALLOC(256, sizeof(l_uint32));
    for (i = 0; i < 256; i++) {
        tab[i] = (sum[i & 0xf]) | (sum[(i >> 4) & 0xf] << 8);
    }
    return tab;
}


/*!
 * \brief   makeValTabSG4()
 *
 * <pre>
 * Notes:
 *      (1) Returns an 8 bit value for the sum of ON pixels
 *          in a 4x4 square, according to
 *              val = 255 - (255 * sum)/16
 *          where sum is in [0,...,16]
 * </pre>
 */
static l_uint8 *
makeValTabSG4(void)
{
l_int32   i;
l_uint8  *tab;

    tab = (l_uint8 *)LEPT_CALLOC(17, sizeof(l_uint8));
    for (i = 0; i < 17; i++)
        tab[i] = 0xff - (i * 255) / 16;
    return tab;
}


/*------------------------------------------------------------------*
 *                         Scale-to-gray 6x                         *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleToGray6Low()
 *
 * \param[in]    datad     dest data
 * \param[in]    wd, hd    dest width, height
 * \param[in]    wpld      dest words/line
 * \param[in]    datas     src data
 * \param[in]    wpls      src words/line
 * \param[in]    tab8      made from makePixelSumTab8()
 * \param[in]    valtab    made from makeValTabSG6()
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Each set of 4 6x6 bit-blocks in the source image, which
 *          consist of 144 pixels arranged 24 pixels wide by 6 scanlines,
 *          is converted to a row of 4 8-bit pixels in the dest image.
 *          These 144 pixels of the input image are runs of 24 pixels
 *          in six adjacent scanlines.  Each run of 24 pixels is
 *          stored in the 24 LSbits of a 32-bit word.  We use 2 LUTs.
 *          The first, tab8, takes 6 of these bits and stores
 *          sum in one byte.  This is done for each of the 6 scanlines,
 *          and the results are added.
 *          We now have the sum of ON pixels in the first 6x6 block.  The
 *          valtab LUT then converts these values (which go from 0 to 36) to
 *          grayscale values between between 255 and 0.  (See makeValTabSG6).
 *          This process is repeated for each of the other 3 sets of
 *          6x6 input pixels, giving 4 output pixels in total.
 *      (2) Note: because the input image is processed in groups of
 *          24 x 6 pixels, the process clips the input height to
 *          (h - h % 6) and the input width to (w - w % 24).
 * </pre>
 */
static void
scaleToGray6Low(l_uint32  *datad,
                l_int32    wd,
                l_int32    hd,
                l_int32    wpld,
                l_uint32  *datas,
                l_int32    wpls,
                l_int32   *tab8,
                l_uint8   *valtab)
{
l_int32    i, j, l, k;
l_uint32   threebytes1, threebytes2, threebytes3;
l_uint32   threebytes4, threebytes5, threebytes6, sum;
l_uint32  *lines, *lined;

        /* i indexes the dest lines
         * l indexes the source lines
         * j indexes the dest bytes
         * k indexes the source bytes
         * We take 18 bytes from the source (144 binary pixels
         * in six lines of 24 pixels each) and convert it
         * into 4 bytes of the dest (four 8 bpp pixels in one line)   */
    for (i = 0, l = 0; i < hd; i++, l += 6) {
        lines = datas + l * wpls;
        lined = datad + i * wpld;
        for (j = 0, k = 0; j < wd; j += 4, k += 3) {
                /* First grab the 18 bytes, 3 at a time, and put each set
                 * of 3 bytes into the LS bytes of a 32-bit word. */
            threebytes1 = (GET_DATA_BYTE(lines, k) << 16) |
                          (GET_DATA_BYTE(lines, k + 1) << 8) |
                          GET_DATA_BYTE(lines, k + 2);
            threebytes2 = (GET_DATA_BYTE(lines + wpls, k) << 16) |
                          (GET_DATA_BYTE(lines + wpls, k + 1) << 8) |
                          GET_DATA_BYTE(lines + wpls, k + 2);
            threebytes3 = (GET_DATA_BYTE(lines + 2 * wpls, k) << 16) |
                          (GET_DATA_BYTE(lines + 2 * wpls, k + 1) << 8) |
                          GET_DATA_BYTE(lines + 2 * wpls, k + 2);
            threebytes4 = (GET_DATA_BYTE(lines + 3 * wpls, k) << 16) |
                          (GET_DATA_BYTE(lines + 3 * wpls, k + 1) << 8) |
                          GET_DATA_BYTE(lines + 3 * wpls, k + 2);
            threebytes5 = (GET_DATA_BYTE(lines + 4 * wpls, k) << 16) |
                          (GET_DATA_BYTE(lines + 4 * wpls, k + 1) << 8) |
                          GET_DATA_BYTE(lines + 4 * wpls, k + 2);
            threebytes6 = (GET_DATA_BYTE(lines + 5 * wpls, k) << 16) |
                          (GET_DATA_BYTE(lines + 5 * wpls, k + 1) << 8) |
                          GET_DATA_BYTE(lines + 5 * wpls, k + 2);

                /* Sum first set of 36 bits and convert to 0-255 */
            sum = tab8[(threebytes1 >> 18)] +
                  tab8[(threebytes2 >> 18)] +
                  tab8[(threebytes3 >> 18)] +
                  tab8[(threebytes4 >> 18)] +
                  tab8[(threebytes5 >> 18)] +
                   tab8[(threebytes6 >> 18)];
            SET_DATA_BYTE(lined, j, valtab[GET_DATA_BYTE(&sum, 3)]);

                /* Ditto for second set */
            sum = tab8[((threebytes1 >> 12) & 0x3f)] +
                  tab8[((threebytes2 >> 12) & 0x3f)] +
                  tab8[((threebytes3 >> 12) & 0x3f)] +
                  tab8[((threebytes4 >> 12) & 0x3f)] +
                  tab8[((threebytes5 >> 12) & 0x3f)] +
                  tab8[((threebytes6 >> 12) & 0x3f)];
            SET_DATA_BYTE(lined, j + 1, valtab[GET_DATA_BYTE(&sum, 3)]);

            sum = tab8[((threebytes1 >> 6) & 0x3f)] +
                  tab8[((threebytes2 >> 6) & 0x3f)] +
                  tab8[((threebytes3 >> 6) & 0x3f)] +
                  tab8[((threebytes4 >> 6) & 0x3f)] +
                  tab8[((threebytes5 >> 6) & 0x3f)] +
                  tab8[((threebytes6 >> 6) & 0x3f)];
            SET_DATA_BYTE(lined, j + 2, valtab[GET_DATA_BYTE(&sum, 3)]);

            sum = tab8[(threebytes1 & 0x3f)] +
                  tab8[(threebytes2 & 0x3f)] +
                  tab8[(threebytes3 & 0x3f)] +
                  tab8[(threebytes4 & 0x3f)] +
                  tab8[(threebytes5 & 0x3f)] +
                  tab8[(threebytes6 & 0x3f)];
            SET_DATA_BYTE(lined, j + 3, valtab[GET_DATA_BYTE(&sum, 3)]);
        }
    }
}


/*!
 * \brief   makeValTabSG6()
 *
 * <pre>
 * Notes:
 *      (1) Returns an 8 bit value for the sum of ON pixels
 *          in a 6x6 square, according to
 *              val = 255 - (255 * sum)/36
 *          where sum is in [0,...,36]
 * </pre>
 */
static l_uint8 *
makeValTabSG6(void)
{
l_int32   i;
l_uint8  *tab;

    tab = (l_uint8 *)LEPT_CALLOC(37, sizeof(l_uint8));
    for (i = 0; i < 37; i++)
        tab[i] = 0xff - (i * 255) / 36;
    return tab;
}


/*------------------------------------------------------------------*
 *                         Scale-to-gray 8x                         *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleToGray8Low()
 *
 * \param[in]    datad     dest data
 * \param[in]    wd, hd    dest width, height
 * \param[in]    wpld      dest words/line
 * \param[in]    datas     src data
 * \param[in]    wpls      src words/line
 * \param[in]    tab8      made from makePixelSumTab8()
 * \param[in]    valtab    made from makeValTabSG8()
 * \return  0 if OK; 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) The output is processed one dest byte at a time,
 *          corresponding to 8 rows of src bytes in the input image.
 *          Two lookup tables are used.  The first, %tab8, gets the
 *          sum of ON pixels in a byte.  After sums from 8 rows have
 *          been added, the second table, %valtab, converts from this
 *          value which is between 0 and 64 to an 8 bpp grayscale
 *          value between 0 and 255: 0 for all 64 bits ON and 255
 *          for all 64 bits OFF.
 * </pre>
 */
static void
scaleToGray8Low(l_uint32  *datad,
                l_int32    wd,
                l_int32    hd,
                l_int32    wpld,
                l_uint32  *datas,
                l_int32    wpls,
                l_int32   *tab8,
                l_uint8   *valtab)
{
l_int32    i, j, k;
l_int32    sbyte0, sbyte1, sbyte2, sbyte3, sbyte4, sbyte5, sbyte6, sbyte7, sum;
l_uint32  *lines, *lined;

        /* i indexes the dest lines
         * k indexes the source lines
         * j indexes the src and dest bytes
         * We take 8 bytes from the source (in 8 lines of 8 pixels
         * each) and convert it into one 8 bpp byte of the dest. */
    for (i = 0, k = 0; i < hd; i++, k += 8) {
        lines = datas + k * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < wd; j++) {
            sbyte0 = GET_DATA_BYTE(lines, j);
            sbyte1 = GET_DATA_BYTE(lines + wpls, j);
            sbyte2 = GET_DATA_BYTE(lines + 2 * wpls, j);
            sbyte3 = GET_DATA_BYTE(lines + 3 * wpls, j);
            sbyte4 = GET_DATA_BYTE(lines + 4 * wpls, j);
            sbyte5 = GET_DATA_BYTE(lines + 5 * wpls, j);
            sbyte6 = GET_DATA_BYTE(lines + 6 * wpls, j);
            sbyte7 = GET_DATA_BYTE(lines + 7 * wpls, j);
            sum = tab8[sbyte0] + tab8[sbyte1] +
                  tab8[sbyte2] + tab8[sbyte3] +
                  tab8[sbyte4] + tab8[sbyte5] +
                  tab8[sbyte6] + tab8[sbyte7];
            SET_DATA_BYTE(lined, j, valtab[sum]);
        }
    }
}


/*!
 * \brief   makeValTabSG8()
 *
 * <pre>
 * Notes:
 *      (1) Returns an 8 bit value for the sum of ON pixels
 *          in an 8x8 square, according to
 *              val = 255 - (255 * sum)/64
 *          where sum is in [0,...,64]
 * </pre>
 */
static l_uint8 *
makeValTabSG8(void)
{
l_int32   i;
l_uint8  *tab;

    tab = (l_uint8 *)LEPT_CALLOC(65, sizeof(l_uint8));
    for (i = 0; i < 65; i++)
        tab[i] = 0xff - (i * 255) / 64;
    return tab;
}


/*------------------------------------------------------------------*
 *                         Scale-to-gray 16x                        *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleToGray16Low()
 *
 * \param[in]    datad     dest data
 * \param[in]    wd, hd    dest width, height
 * \param[in]    wpld      dest words/line
 * \param[in]    datas     src data
 * \param[in]    wpls      src words/line
 * \param[in]    tab8      made from makePixelSumTab8()
 * \return  0 if OK; 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) The output is processed one dest byte at a time, corresponding
 *          to 16 rows consisting each of 2 src bytes in the input image.
 *          This uses one lookup table, tab8, which gives the sum of
 *          ON pixels in a byte.  After summing for all ON pixels in the
 *          32 src bytes, which is between 0 and 256, this is converted
 *          to an 8 bpp grayscale value between 0 for 255 or 256 bits ON
 *          and 255 for 0 bits ON.
 * </pre>
 */
static void
scaleToGray16Low(l_uint32  *datad,
                  l_int32    wd,
                 l_int32    hd,
                 l_int32    wpld,
                 l_uint32  *datas,
                 l_int32    wpls,
                 l_int32   *tab8)
{
l_int32    i, j, k, m;
l_int32    sum;
l_uint32  *lines, *lined;

        /* i indexes the dest lines
         * k indexes the source lines
         * j indexes the dest bytes
         * m indexes the src bytes
         * We take 32 bytes from the source (in 16 lines of 16 pixels
         * each) and convert it into one 8 bpp byte of the dest. */
    for (i = 0, k = 0; i < hd; i++, k += 16) {
        lines = datas + k * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < wd; j++) {
            m = 2 * j;
            sum = tab8[GET_DATA_BYTE(lines, m)];
            sum += tab8[GET_DATA_BYTE(lines, m + 1)];
            sum += tab8[GET_DATA_BYTE(lines + wpls, m)];
            sum += tab8[GET_DATA_BYTE(lines + wpls, m + 1)];
            sum += tab8[GET_DATA_BYTE(lines + 2 * wpls, m)];
            sum += tab8[GET_DATA_BYTE(lines + 2 * wpls, m + 1)];
            sum += tab8[GET_DATA_BYTE(lines + 3 * wpls, m)];
            sum += tab8[GET_DATA_BYTE(lines + 3 * wpls, m + 1)];
            sum += tab8[GET_DATA_BYTE(lines + 4 * wpls, m)];
            sum += tab8[GET_DATA_BYTE(lines + 4 * wpls, m + 1)];
            sum += tab8[GET_DATA_BYTE(lines + 5 * wpls, m)];
            sum += tab8[GET_DATA_BYTE(lines + 5 * wpls, m + 1)];
            sum += tab8[GET_DATA_BYTE(lines + 6 * wpls, m)];
            sum += tab8[GET_DATA_BYTE(lines + 6 * wpls, m + 1)];
            sum += tab8[GET_DATA_BYTE(lines + 7 * wpls, m)];
            sum += tab8[GET_DATA_BYTE(lines + 7 * wpls, m + 1)];
            sum += tab8[GET_DATA_BYTE(lines + 8 * wpls, m)];
            sum += tab8[GET_DATA_BYTE(lines + 8 * wpls, m + 1)];
            sum += tab8[GET_DATA_BYTE(lines + 9 * wpls, m)];
            sum += tab8[GET_DATA_BYTE(lines + 9 * wpls, m + 1)];
            sum += tab8[GET_DATA_BYTE(lines + 10 * wpls, m)];
            sum += tab8[GET_DATA_BYTE(lines + 10 * wpls, m + 1)];
            sum += tab8[GET_DATA_BYTE(lines + 11 * wpls, m)];
            sum += tab8[GET_DATA_BYTE(lines + 11 * wpls, m + 1)];
            sum += tab8[GET_DATA_BYTE(lines + 12 * wpls, m)];
            sum += tab8[GET_DATA_BYTE(lines + 12 * wpls, m + 1)];
            sum += tab8[GET_DATA_BYTE(lines + 13 * wpls, m)];
            sum += tab8[GET_DATA_BYTE(lines + 13 * wpls, m + 1)];
            sum += tab8[GET_DATA_BYTE(lines + 14 * wpls, m)];
            sum += tab8[GET_DATA_BYTE(lines + 14 * wpls, m + 1)];
            sum += tab8[GET_DATA_BYTE(lines + 15 * wpls, m)];
            sum += tab8[GET_DATA_BYTE(lines + 15 * wpls, m + 1)];
            sum = L_MIN(sum, 255);
            SET_DATA_BYTE(lined, j, 255 - sum);
        }
    }
}



/*------------------------------------------------------------------*
 *                         Grayscale mipmap                         *
 *------------------------------------------------------------------*/
/*!
 * \brief   scaleMipmapLow()
 *
 * <pre>
 * Notes:
 *      (1) See notes in scale.c for pixScaleToGrayMipmap().  This function
 *          is here for pedagogical reasons.  It gives poor results on document
 *          images because of aliasing.
 * </pre>
 */
static l_int32
scaleMipmapLow(l_uint32  *datad,
               l_int32    wd,
               l_int32    hd,
               l_int32    wpld,
               l_uint32  *datas1,
               l_int32    wpls1,
               l_uint32  *datas2,
               l_int32    wpls2,
               l_float32  red)
{
l_int32    i, j, val1, val2, val, row2, col2;
l_int32   *srow, *scol;
l_uint32  *lines1, *lines2, *lined;
l_float32  ratio, w1, w2;

        /* Clear dest */
    memset(datad, 0, 4LL * wpld * hd);

        /* Each dest pixel at (j,i) is computed by interpolating
           between the two src images at the corresponding location.
           We store the UL corner locations of the square of
           src pixels in thelower-resolution image that correspond
           to dest pixel (j,i).  The are labeled by the arrays
           srow[i], scol[j].  The UL corner locations of the higher
           resolution src pixels are obtained from these arrays
           by multiplying by 2. */
    if ((srow = (l_int32 *)LEPT_CALLOC(hd, sizeof(l_int32))) == NULL)
        return ERROR_INT("srow not made", __func__, 1);
    if ((scol = (l_int32 *)LEPT_CALLOC(wd, sizeof(l_int32))) == NULL) {
        LEPT_FREE(srow);
        return ERROR_INT("scol not made", __func__, 1);
    }
    ratio = 1.f / (2.f * red);  /* 0.5 for red = 1, 1 for red = 0.5 */
    for (i = 0; i < hd; i++)
        srow[i] = (l_int32)(ratio * i);
    for (j = 0; j < wd; j++)
        scol[j] = (l_int32)(ratio * j);

        /* Get weights for linear interpolation: these are the
         * 'distances' of the dest image plane from the two
         * src image planes. */
    w1 = 2.f * red - 1.f;   /* w1 --> 1 as red --> 1 */
    w2 = 1.f - w1;

        /* For each dest pixel, compute linear interpolation */
    for (i = 0; i < hd; i++) {
        row2 = srow[i];
        lines1 = datas1 + 2 * row2 * wpls1;
        lines2 = datas2 + row2 * wpls2;
        lined = datad + i * wpld;
        for (j = 0; j < wd; j++) {
            col2 = scol[j];
            val1 = GET_DATA_BYTE(lines1, 2 * col2);
            val2 = GET_DATA_BYTE(lines2, col2);
            val = (l_int32)(w1 * val1 + w2 * val2);
            SET_DATA_BYTE(lined, j, val);
        }
    }

    LEPT_FREE(srow);
    LEPT_FREE(scol);
    return 0;
}

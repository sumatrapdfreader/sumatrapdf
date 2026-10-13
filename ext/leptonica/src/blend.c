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
 * \file blend.c
 * <pre>
 *
 *      Blending two images that are not colormapped
 *           PIX             *pixBlend()
 *           PIX             *pixBlendMask()
 *           PIX             *pixBlendGray()
 *           PIX             *pixBlendGrayInverse()
 *           PIX             *pixBlendColor()
 *           PIX             *pixBlendColorByChannel()
 *           PIX             *pixBlendGrayAdapt()
 *           static l_int32   blendComponents()
 *           PIX             *pixFadeWithGray()
 *           PIX             *pixBlendHardLight()
 *           static l_int32   blendHardLightComponents()
 *
 *      Blending two colormapped images
 *           l_int32          pixBlendCmap()
 *
 *      Blending two images using a third (alpha mask)
 *           PIX             *pixBlendWithGrayMask()
 *
 *      Blending background to a specific color
 *           PIX             *pixBlendBackgroundToColor()
 *
 *      Multiplying by a specific color
 *           PIX             *pixMultiplyByColor()
 *
 *      Rendering with alpha blending over a uniform background
 *           PIX             *pixAlphaBlendUniform()
 *
 *      Adding an alpha layer for blending
 *           PIX             *pixAddAlphaToBlend()
 *
 *      Setting a transparent alpha component over a white background
 *           PIX             *pixSetAlphaOverWhite()
 *
 *      Fading from the edge
 *           l_int32          pixLinearEdgeFade()
 *
 *  In blending operations a new pix is produced where typically
 *  a subset of pixels in src1 are changed by the set of pixels
 *  in src2, when src2 is located in a given position relative
 *  to src1.  This is similar to rasterop, except that the
 *  blending operations we allow are more complex, and typically
 *  result in dest pixels that are a linear combination of two
 *  pixels, such as src1 and its inverse.  I find it convenient
 *  to think of src2 as the "blender" (the one that takes the action)
 *  and src1 as the "blendee" (the one that changes).
 *
 *  Blending works best when src1 is 8 or 32 bpp.  We also allow
 *  src1 to be colormapped, but the colormap is removed before blending,
 *  so if src1 is colormapped, we can't allow in-place blending.
 *
 *  Because src2 is typically smaller than src1, we can implement by
 *  clipping src2 to src1 and then transforming some of the dest
 *  pixels that are under the support of src2.  In practice, we
 *  do the clipping in the inner pixel loop.  For grayscale and
 *  color src2, we also allow a simple form of transparency, where
 *  pixels of a particular value in src2 are transparent; for those pixels,
 *  no blending is done.
 *
 *  The blending functions are categorized by the depth of src2,
 *  the blender, and not that of src1, the blendee.
 *
 *   ~ If src2 is 1 bpp, we can do one of three things:
 *     (1) L_BLEND_WITH_INVERSE: Blend a given fraction of src1 with its
 *         inverse color for those pixels in src2 that are fg (ON),
 *         and leave the dest pixels unchanged for pixels in src2 that
 *         are bg (OFF).
 *     (2) L_BLEND_TO_WHITE: Fade the src1 pixels toward white by a
 *         given fraction for those pixels in src2 that are fg (ON),
 *         and leave the dest pixels unchanged for pixels in src2 that
 *         are bg (OFF).
 *     (3) L_BLEND_TO_BLACK: Fade the src1 pixels toward black by a
 *         given fraction for those pixels in src2 that are fg (ON),
 *         and leave the dest pixels unchanged for pixels in src2 that
 *         are bg (OFF).
 *     The blending function is pixBlendMask().
 *
 *   ~ If src2 is 8 bpp grayscale, we can do one of two things
 *     (but see pixFadeWithGray() below):
 *     (1) L_BLEND_GRAY: If src1 is 8 bpp, mix the two values, using
 *         a fraction of src2 and (1 - fraction) of src1.
 *         If src1 is 32 bpp (rgb), mix the fraction of src2 with
 *         each of the color components in src1.
 *     (2) L_BLEND_GRAY_WITH_INVERSE: Use the grayscale value in src2
 *         to determine how much of the inverse of a src1 pixel is
 *         to be combined with the pixel value.  The input fraction
 *         further acts to scale the change in the src1 pixel.
 *     The blending function is pixBlendGray().
 *
 *   ~ If src2 is color, we blend a given fraction of src2 with
 *     src1.  If src1 is 8 bpp, the resulting image is 32 bpp.
 *     The blending function is pixBlendColor().
 *
 *   ~ For all three blending functions -- pixBlendMask(), pixBlendGray()
 *     and pixBlendColor() -- you can apply the blender to the blendee
 *     either in-place or generating a new pix.  For the in-place
 *     operation, this requires that the depth of the resulting pix
 *     must equal that of the input pixs1.
 *
 *   ~ We remove colormaps from src1 and src2 before blending.
 *     Any quantization would have to be done after blending.
 *
 *  We include another function, pixFadeWithGray(), that blends
 *  a gray or color src1 with a gray src2.  It does one of these things:
 *     (1) L_BLEND_TO_WHITE: Fade the src1 pixels toward white by
 *         a number times the value in src2.
 *     (2) L_BLEND_TO_BLACK: Fade the src1 pixels toward black by
 *         a number times the value in src2.
 *
 *  Also included is a generalization of the so-called "hard light"
 *  blending: pixBlendHardLight().  We generalize by allowing a fraction < 1.0
 *  of the blender to be admixed with the blendee.  The standard function
 *  does full mixing.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

static l_int32 blendComponents(l_int32 a, l_int32 b, l_float32 fract);
static l_int32 blendHardLightComponents(l_int32 a, l_int32 b, l_float32 fract);

/*-------------------------------------------------------------*
 *         Blending two images that are not colormapped        *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixBlend()
 *
 * \param[in]    pixs1    blendee
 * \param[in]    pixs2    blender; typ. smaller
 * \param[in]    x,y      origin [UL corner] of pixs2 relative to
 *                        the origin of pixs1; can be < 0
 * \param[in]    fract    blending fraction
 * \return  pixd blended image, or null on error
 *
 * <pre>
 * Notes:
 *      (1) This is a simple top-level interface.  For more flexibility,
 *          call directly into pixBlendMask(), etc.
 * </pre>
 */
PIX *
pixBlend(PIX       *pixs1,
         PIX       *pixs2,
         l_int32    x,
         l_int32    y,
         l_float32  fract)
{
l_int32    w1, h1, d1, d2;
BOX       *box;
PIX       *pixc, *pixt, *pixd;

    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, NULL);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, NULL);

        /* check relative depths */
    d1 = pixGetDepth(pixs1);
    d2 = pixGetDepth(pixs2);
    if (d1 == 1 && d2 > 1)
        return (PIX *)ERROR_PTR("mixing gray or color with 1 bpp",
                                __func__, NULL);

        /* remove colormap from pixs2 if necessary */
    pixt = pixRemoveColormap(pixs2, REMOVE_CMAP_BASED_ON_SRC);
    d2 = pixGetDepth(pixt);

        /* Check if pixs2 is clipped by its position with respect
         * to pixs1; if so, clip it and redefine x and y if necessary.
         * This actually isn't necessary, as the specific blending
         * functions do the clipping directly in the pixel loop
         * over pixs2, but it's included here to show how it can
         * easily be done on pixs2 first. */
    pixGetDimensions(pixs1, &w1, &h1, NULL);
    box = boxCreate(-x, -y, w1, h1);  /* box of pixs1 relative to pixs2 */
    pixc = pixClipRectangle(pixt, box, NULL);
    boxDestroy(&box);
    if (!pixc) {
        L_WARNING("box doesn't overlap pix\n", __func__);
        pixDestroy(&pixt);
        return NULL;
    }
    x = L_MAX(0, x);
    y = L_MAX(0, y);

    if (d2 == 1) {
        pixd = pixBlendMask(NULL, pixs1, pixc, x, y, fract,
                            L_BLEND_WITH_INVERSE);
    } else if (d2 == 8) {
        pixd = pixBlendGray(NULL, pixs1, pixc, x, y, fract,
                            L_BLEND_GRAY, 0, 0);
    } else {  /* d2 == 32 */
        pixd = pixBlendColor(NULL, pixs1, pixc, x, y, fract, 0, 0);
    }

    pixDestroy(&pixc);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixBlendMask()
 *
 * \param[in]    pixd    [optional]; either NULL or equal to pixs1 for in-place
 * \param[in]    pixs1   blendee, depth > 1
 * \param[in]    pixs2   blender, 1 bpp; typ. smaller in size than pixs1
 * \param[in]    x,y     origin [UL corner] of pixs2 relative to
 *                       the origin of pixs1; can be < 0
 * \param[in]    fract   blending fraction
 * \param[in]    type    L_BLEND_WITH_INVERSE, L_BLEND_TO_WHITE,
 *                       L_BLEND_TO_BLACK
 * \return  pixd if OK; null on error
 *
 * <pre>
 * Notes:
 *      (1) Clipping of pixs2 to pixs1 is done in the inner pixel loop.
 *      (2) If pixs1 has a colormap, it is removed.
 *      (3) For inplace operation (pixs1 not cmapped), call it this way:
 *            pixBlendMask(pixs1, pixs1, pixs2, ...)
 *      (4) For generating a new pixd:
 *            pixd = pixBlendMask(NULL, pixs1, pixs2, ...)
 *      (5) Only call in-place if pixs1 does not have a colormap.
 *      (6) Invalid %fract defaults to 0.5 with a warning.
 *          Invalid %type defaults to L_BLEND_WITH_INVERSE with a warning.
 * </pre>
 */
PIX *
pixBlendMask(PIX       *pixd,
             PIX       *pixs1,
             PIX       *pixs2,
             l_int32    x,
             l_int32    y,
             l_float32  fract,
             l_int32    type)
{
l_int32    i, j, d, wc, hc, w, h, wplc;
l_int32    val, rval, gval, bval;
l_uint32   pixval;
l_uint32  *linec, *datac;
PIX       *pixc, *pix1, *pix2;

    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, NULL);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, NULL);
    if (pixGetDepth(pixs1) == 1)
        return (PIX *)ERROR_PTR("pixs1 is 1 bpp", __func__, NULL);
    if (pixGetDepth(pixs2) != 1)
        return (PIX *)ERROR_PTR("pixs2 not 1 bpp", __func__, NULL);
    if (pixd == pixs1 && pixGetColormap(pixs1))
        return (PIX *)ERROR_PTR("inplace; pixs1 has colormap", __func__, NULL);
    if (pixd && (pixd != pixs1))
        return (PIX *)ERROR_PTR("pixd must be NULL or pixs1", __func__, NULL);
    if (fract < 0.0 || fract > 1.0) {
        L_WARNING("fract must be in [0.0, 1.0]; setting to 0.5\n", __func__);
        fract = 0.5;
    }
    if (type != L_BLEND_WITH_INVERSE && type != L_BLEND_TO_WHITE &&
        type != L_BLEND_TO_BLACK) {
        L_WARNING("invalid blend type; setting to L_BLEND_WITH_INVERSE\n",
                  __func__);
        type = L_BLEND_WITH_INVERSE;
    }

        /* If pixd != NULL, we know that it is equal to pixs1 and
         * that pixs1 does not have a colormap, so that an in-place operation
         * can be done.  Otherwise, remove colormap from pixs1 if
         * it exists and unpack to at least 8 bpp if necessary,
         * to do the blending on a new pix. */
    if (!pixd) {
        pix1 = pixRemoveColormap(pixs1, REMOVE_CMAP_BASED_ON_SRC);
        if (pixGetDepth(pix1) < 8)
            pix2 = pixConvertTo8(pix1, FALSE);
        else
            pix2 = pixClone(pix1);
        pixd = pixCopy(NULL, pix2);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }

    pixGetDimensions(pixd, &w, &h, &d);  /* d must be either 8 or 32 bpp */
    pixc = pixClone(pixs2);
    wc = pixGetWidth(pixc);
    hc = pixGetHeight(pixc);
    datac = pixGetData(pixc);
    wplc = pixGetWpl(pixc);

        /* Check limits for src1, in case clipping was not done. */
    switch (type)
    {
    case L_BLEND_WITH_INVERSE:
            /*
             * The basic logic for this blending is:
             *      p -->  (1 - f) * p + f * (1 - p)
             * where p is a normalized value: p = pixval / 255.
             * Thus,
             *      p -->  p + f * (1 - 2 * p)
             */
        for (i = 0; i < hc; i++) {
            if (i + y < 0  || i + y >= h) continue;
            linec = datac + i * wplc;
            for (j = 0; j < wc; j++) {
                if (j + x < 0  || j + x >= w) continue;
                bval = GET_DATA_BIT(linec, j);
                if (bval) {
                    switch (d)
                    {
                    case 8:
                        pixGetPixel(pixd, x + j, y + i, &pixval);
                        val = (l_int32)(pixval + fract * (255 - 2 * pixval));
                        pixSetPixel(pixd, x + j, y + i, val);
                        break;
                    case 32:
                        pixGetPixel(pixd, x + j, y + i, &pixval);
                        extractRGBValues(pixval, &rval, &gval, &bval);
                        rval = (l_int32)(rval + fract * (255 - 2 * rval));
                        gval = (l_int32)(gval + fract * (255 - 2 * gval));
                        bval = (l_int32)(bval + fract * (255 - 2 * bval));
                        composeRGBPixel(rval, gval, bval, &pixval);
                        pixSetPixel(pixd, x + j, y + i, pixval);
                        break;
                    default:
                        L_WARNING("d neither 8 nor 32 bpp; no blend\n",
                                  __func__);
                    }
                }
            }
        }
        break;
    case L_BLEND_TO_WHITE:
            /*
             * The basic logic for this blending is:
             *      p -->  p + f * (1 - p)    (p normalized to [0...1])
             */
        for (i = 0; i < hc; i++) {
            if (i + y < 0  || i + y >= h) continue;
            linec = datac + i * wplc;
            for (j = 0; j < wc; j++) {
                if (j + x < 0  || j + x >= w) continue;
                bval = GET_DATA_BIT(linec, j);
                if (bval) {
                    switch (d)
                    {
                    case 8:
                        pixGetPixel(pixd, x + j, y + i, &pixval);
                        val = (l_int32)(pixval + fract * (255 - pixval));
                        pixSetPixel(pixd, x + j, y + i, val);
                        break;
                    case 32:
                        pixGetPixel(pixd, x + j, y + i, &pixval);
                        extractRGBValues(pixval, &rval, &gval, &bval);
                        rval = (l_int32)(rval + fract * (255 - rval));
                        gval = (l_int32)(gval + fract * (255 - gval));
                        bval = (l_int32)(bval + fract * (255 - bval));
                        composeRGBPixel(rval, gval, bval, &pixval);
                        pixSetPixel(pixd, x + j, y + i, pixval);
                        break;
                    default:
                        L_WARNING("d neither 8 nor 32 bpp; no blend\n",
                                  __func__);
                    }
                }
            }
        }
        break;
    case L_BLEND_TO_BLACK:
            /*
             * The basic logic for this blending is:
             *      p -->  (1 - f) * p     (p normalized to [0...1])
             */
        for (i = 0; i < hc; i++) {
            if (i + y < 0  || i + y >= h) continue;
            linec = datac + i * wplc;
            for (j = 0; j < wc; j++) {
                if (j + x < 0  || j + x >= w) continue;
                bval = GET_DATA_BIT(linec, j);
                if (bval) {
                    switch (d)
                    {
                    case 8:
                        pixGetPixel(pixd, x + j, y + i, &pixval);
                        val = (l_int32)((1. - fract) * pixval);
                        pixSetPixel(pixd, x + j, y + i, val);
                        break;
                    case 32:
                        pixGetPixel(pixd, x + j, y + i, &pixval);
                        extractRGBValues(pixval, &rval, &gval, &bval);
                        rval = (l_int32)((1. - fract) * rval);
                        gval = (l_int32)((1. - fract) * gval);
                        bval = (l_int32)((1. - fract) * bval);
                        composeRGBPixel(rval, gval, bval, &pixval);
                        pixSetPixel(pixd, x + j, y + i, pixval);
                        break;
                    default:
                        L_WARNING("d neither 8 nor 32 bpp; no blend\n",
                                  __func__);
                    }
                }
            }
        }
        break;
    default:
        L_WARNING("invalid binary mask blend type\n", __func__);
        break;
    }

    pixDestroy(&pixc);
    return pixd;
}


/*!
 * \brief   pixBlendGray()
 *
 * \param[in]    pixd         [optional] either equal to pixs1 for in-place,
 *                            or NULL
 * \param[in]    pixs1        blendee, depth > 1
 * \param[in]    pixs2        blender, any depth; typically, the area of
 *                            pixs2 is smaller than pixs1
 * \param[in]    x,y          origin [UL corner] of pixs2 relative to
 *                            the origin of pixs1; can be < 0
 * \param[in]    fract        blending fraction
 * \param[in]    type         L_BLEND_GRAY, L_BLEND_GRAY_WITH_INVERSE
 * \param[in]    transparent  1 to use transparency; 0 otherwise
 * \param[in]    transpix     pixel grayval in pixs2 that is to be transparent
 * \return  pixd if OK; pixs1 on error
 *
 * <pre>
 * Notes:
 *      (1) For inplace operation (pixs1 not cmapped), call it this way:
 *            pixBlendGray(pixs1, pixs1, pixs2, ...)
 *      (2) For generating a new pixd:
 *            pixd = pixBlendGray(NULL, pixs1, pixs2, ...)
 *      (3) Clipping of pixs2 to pixs1 is done in the inner pixel loop.
 *      (4) If pixs1 has a colormap, it is removed; otherwise, if pixs1
 *          has depth < 8, it is unpacked to generate a 8 bpp pix.
 *      (5) If transparent = 0, the blending fraction (fract) is
 *          applied equally to all pixels.
 *      (6) If transparent = 1, all pixels of value transpix (typically
 *          either 0 or 0xff) in pixs2 are transparent in the blend.
 *      (7) After processing pixs1, it is either 8 bpp or 32 bpp:
 *          ~ if 8 bpp, the fraction of pixs2 is mixed with pixs1.
 *          ~ if 32 bpp, each component of pixs1 is mixed with
 *            the same fraction of pixs2.
 *      (8) For L_BLEND_GRAY_WITH_INVERSE, the white values of the blendee
 *          (cval == 255 in the code below) result in a delta of 0.
 *          Thus, these pixels are intrinsically transparent!
 *          The "pivot" value of the src, at which no blending occurs, is
 *          128.  Compare with the adaptive pivot in pixBlendGrayAdapt().
 *      (9) Invalid %fract defaults to 0.5 with a warning.
 *          Invalid %type defaults to L_BLEND_GRAY with a warning.
 * </pre>
 */
PIX *
pixBlendGray(PIX       *pixd,
             PIX       *pixs1,
             PIX       *pixs2,
             l_int32    x,
             l_int32    y,
             l_float32  fract,
             l_int32    type,
             l_int32    transparent,
             l_uint32   transpix)
{
l_int32    i, j, d, wc, hc, w, h, wplc, wpld, delta;
l_int32    ival, irval, igval, ibval, cval, dval;
l_uint32   val32;
l_uint32  *linec, *lined, *datac, *datad;
PIX       *pixc, *pix1, *pix2;

    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, pixd);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, pixd);
    if (pixGetDepth(pixs1) == 1)
        return (PIX *)ERROR_PTR("pixs1 is 1 bpp", __func__, pixd);
    if (pixd == pixs1 && pixGetColormap(pixs1))
        return (PIX *)ERROR_PTR("can't do in-place with cmap", __func__, pixd);
    if (pixd && (pixd != pixs1))
        return (PIX *)ERROR_PTR("pixd must be NULL or pixs1", __func__, pixd);
    if (fract < 0.0 || fract > 1.0) {
        L_WARNING("fract must be in [0.0, 1.0]; setting to 0.5\n", __func__);
        fract = 0.5;
    }
    if (type != L_BLEND_GRAY && type != L_BLEND_GRAY_WITH_INVERSE) {
        L_WARNING("invalid blend type; setting to L_BLEND_GRAY\n", __func__);
        type = L_BLEND_GRAY;
    }

        /* If pixd != NULL, we know that it is equal to pixs1 and
         * that pixs1 does not have a colormap, so that an in-place operation
         * can be done.  Otherwise, remove colormap from pixs1 if
         * it exists and unpack to at least 8 bpp if necessary,
         * to do the blending on a new pix. */
    if (!pixd) {
        pix1 = pixRemoveColormap(pixs1, REMOVE_CMAP_BASED_ON_SRC);
        if (pixGetDepth(pix1) < 8)
            pix2 = pixConvertTo8(pix1, FALSE);
        else
            pix2 = pixClone(pix1);
        pixd = pixCopy(NULL, pix2);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }

    pixGetDimensions(pixd, &w, &h, &d);  /* 8 or 32 bpp */
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);
    pixc = pixConvertTo8(pixs2, 0);
    pixGetDimensions(pixc, &wc, &hc, NULL);
    datac = pixGetData(pixc);
    wplc = pixGetWpl(pixc);

        /* Check limits for src1, in case clipping was not done */
    if (type == L_BLEND_GRAY) {
        /*
         * The basic logic for this blending is:
         *      p -->  (1 - f) * p + f * c
         * where c is the 8 bpp blender.  All values are normalized to [0...1].
         */
        for (i = 0; i < hc; i++) {
            if (i + y < 0  || i + y >= h) continue;
            linec = datac + i * wplc;
            lined = datad + (i + y) * wpld;
            switch (d)
            {
            case 8:
                for (j = 0; j < wc; j++) {
                    if (j + x < 0  || j + x >= w) continue;
                    cval = GET_DATA_BYTE(linec, j);
                    if (transparent == 0 || cval != transpix) {
                        dval = GET_DATA_BYTE(lined, j + x);
                        ival = (l_int32)((1. - fract) * dval + fract * cval);
                        SET_DATA_BYTE(lined, j + x, ival);
                    }
                }
                break;
            case 32:
                for (j = 0; j < wc; j++) {
                    if (j + x < 0  || j + x >= w) continue;
                    cval = GET_DATA_BYTE(linec, j);
                    if (transparent == 0 || cval != transpix) {
                        val32 = *(lined + j + x);
                        extractRGBValues(val32, &irval, &igval, &ibval);
                        irval = (l_int32)((1. - fract) * irval + fract * cval);
                        igval = (l_int32)((1. - fract) * igval + fract * cval);
                        ibval = (l_int32)((1. - fract) * ibval + fract * cval);
                        composeRGBPixel(irval, igval, ibval, &val32);
                        *(lined + j + x) = val32;
                    }
                }
                break;
            default:
                break;   /* shouldn't happen */
            }
        }
    } else {  /* L_BLEND_GRAY_WITH_INVERSE */
        for (i = 0; i < hc; i++) {
            if (i + y < 0  || i + y >= h) continue;
            linec = datac + i * wplc;
            lined = datad + (i + y) * wpld;
            switch (d)
            {
            case 8:
                /*
                 * For 8 bpp, the dest pix is shifted by a signed amount
                 * proportional to the distance from 128 (the pivot value),
                 * and to the darkness of src2.  If the dest is darker
                 * than 128, it becomes lighter, and v.v.
                 * The basic logic is:
                 *     d  -->  d + f * (0.5 - d) * (1 - c)
                 * where d and c are normalized pixel values for src1 and
                 * src2, respectively, with 8 bit normalization to [0...1].
                 */
                for (j = 0; j < wc; j++) {
                    if (j + x < 0  || j + x >= w) continue;
                    cval = GET_DATA_BYTE(linec, j);
                    if (transparent == 0 || cval != transpix) {
                        ival = GET_DATA_BYTE(lined, j + x);
                        delta = (128 - ival) * (255 - cval) / 256;
                        ival += (l_int32)(fract * delta + 0.5);
                        SET_DATA_BYTE(lined, j + x, ival);
                    }
                }
                break;
            case 32:
                /* Each component is shifted by the same formula for 8 bpp */
                for (j = 0; j < wc; j++) {
                    if (j + x < 0  || j + x >= w) continue;
                    cval = GET_DATA_BYTE(linec, j);
                    if (transparent == 0 || cval != transpix) {
                        val32 = *(lined + j + x);
                        extractRGBValues(val32, &irval, &igval, &ibval);
                        delta = (128 - irval) * (255 - cval) / 256;
                        irval += (l_int32)(fract * delta + 0.5);
                        delta = (128 - igval) * (255 - cval) / 256;
                        igval += (l_int32)(fract * delta + 0.5);
                        delta = (128 - ibval) * (255 - cval) / 256;
                        ibval += (l_int32)(fract * delta + 0.5);
                        composeRGBPixel(irval, igval, ibval, &val32);
                        *(lined + j + x) = val32;
                    }
                }
                break;
            default:
                break;   /* shouldn't happen */
            }
        }
    }

    pixDestroy(&pixc);
    return pixd;
}


/*!
 * \brief   pixBlendGrayInverse()
 *
 * \param[in]    pixd     [optional] either equal to pixs1 for in-place, or NULL
 * \param[in]    pixd     [optional] either NULL or equal to pixs1 for in-place
 * \param[in]    pixs1    blendee, depth > 1
 * \param[in]    pixs2    blender, any depth; typ. smaller in size than pixs1
 * \param[in]    x,y      origin [UL corner] of pixs2 relative to
 *                        the origin of pixs1; can be < 0
 * \param[in]    fract    blending fraction
 * \return  pixd if OK; pixs1 on error
 *
 * <pre>
 * Notes:
 *      (1) For inplace operation (pixs1 not cmapped), call it this way:
 *            pixBlendGrayInverse(pixs1, pixs1, pixs2, ...)
 *      (2) For generating a new pixd:
 *            pixd = pixBlendGrayInverse(NULL, pixs1, pixs2, ...)
 *      (3) Clipping of pixs2 to pixs1 is done in the inner pixel loop.
 *      (4) If pixs1 has a colormap, it is removed; otherwise if pixs1
 *          has depth < 8, it is unpacked to generate a 8 bpp pix.
 *      (5) This is a no-nonsense blender.  It changes the src1 pixel except
 *          when the src1 pixel is midlevel gray.  Use fract == 1 for the most
 *          aggressive blending, where, if the gray pixel in pixs2 is 0,
 *          we get a complete inversion of the color of the src pixel in pixs1.
 *      (6) The basic logic is that each component transforms by:
                 d  -->  c * d + (1 - c ) * (f * (1 - d) + d * (1 - f))
 *          where c is the blender pixel from pixs2,
 *                f is %fract,
 *                c and d are normalized to [0...1]
 *          This has the property that for f == 0 (no blend) or c == 1 (white):
 *               d  -->  d
 *          For c == 0 (black) we get maximum inversion:
 *               d  -->  f * (1 - d) + d * (1 - f)   [inversion by fraction f]
 * </pre>
 */
PIX *
pixBlendGrayInverse(PIX       *pixd,
                    PIX       *pixs1,
                    PIX       *pixs2,
                    l_int32    x,
                    l_int32    y,
                    l_float32  fract)
{
l_int32    i, j, d, wc, hc, w, h, wplc, wpld;
l_int32    irval, igval, ibval, cval, dval;
l_float32  a;
l_uint32   val32;
l_uint32  *linec, *lined, *datac, *datad;
PIX       *pixc, *pix1, *pix2;

    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, pixd);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, pixd);
    if (pixGetDepth(pixs1) == 1)
        return (PIX *)ERROR_PTR("pixs1 is 1 bpp", __func__, pixd);
    if (pixd == pixs1 && pixGetColormap(pixs1))
        return (PIX *)ERROR_PTR("can't do in-place with cmap", __func__, pixd);
    if (pixd && (pixd != pixs1))
        return (PIX *)ERROR_PTR("pixd must be NULL or pixs1", __func__, pixd);
    if (fract < 0.0 || fract > 1.0) {
        L_WARNING("fract must be in [0.0, 1.0]; setting to 0.5\n", __func__);
        fract = 0.5;
    }

        /* If pixd != NULL, we know that it is equal to pixs1 and
         * that pixs1 does not have a colormap, so that an in-place operation
         * can be done.  Otherwise, remove colormap from pixs1 if
         * it exists and unpack to at least 8 bpp if necessary,
         * to do the blending on a new pix. */
    if (!pixd) {
        pix1 = pixRemoveColormap(pixs1, REMOVE_CMAP_BASED_ON_SRC);
        if (pixGetDepth(pix1) < 8)
            pix2 = pixConvertTo8(pix1, FALSE);
        else
            pix2 = pixClone(pix1);
        pixd = pixCopy(NULL, pix2);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }

    pixGetDimensions(pixd, &w, &h, &d);  /* 8 or 32 bpp */
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);
    pixc = pixConvertTo8(pixs2, 0);
    pixGetDimensions(pixc, &wc, &hc, NULL);
    datac = pixGetData(pixc);
    wplc = pixGetWpl(pixc);

        /* Check limits for src1, in case clipping was not done */
    for (i = 0; i < hc; i++) {
        if (i + y < 0  || i + y >= h) continue;
        linec = datac + i * wplc;
        lined = datad + (i + y) * wpld;
        switch (d)
        {
        case 8:
            for (j = 0; j < wc; j++) {
                if (j + x < 0  || j + x >= w) continue;
                cval = GET_DATA_BYTE(linec, j);
                dval = GET_DATA_BYTE(lined, j + x);
                a = (1.0 - fract) * dval + fract * (255.0 - dval);
                dval = (l_int32)(cval * dval / 255.0 +
                                  a * (255.0 - cval) / 255.0);
                SET_DATA_BYTE(lined, j + x, dval);
            }
            break;
        case 32:
            for (j = 0; j < wc; j++) {
                if (j + x < 0  || j + x >= w) continue;
                cval = GET_DATA_BYTE(linec, j);
                val32 = *(lined + j + x);
                extractRGBValues(val32, &irval, &igval, &ibval);
                a = (1.0 - fract) * irval + fract * (255.0 - irval);
                irval = (l_int32)(cval * irval / 255.0 +
                                  a * (255.0 - cval) / 255.0);
                a = (1.0 - fract) * igval + fract * (255.0 - igval);
                igval = (l_int32)(cval * igval / 255.0 +
                                  a * (255.0 - cval) / 255.0);
                a = (1.0 - fract) * ibval + fract * (255.0 - ibval);
                ibval = (l_int32)(cval * ibval / 255.0 +
                                  a * (255.0 - cval) / 255.0);
                composeRGBPixel(irval, igval, ibval, &val32);
                *(lined + j + x) = val32;
            }
            break;
        default:
            break;   /* shouldn't happen */
        }
    }

    pixDestroy(&pixc);
    return pixd;
}


/*!
 * \brief   pixBlendColor()
 *
 * \param[in]    pixd          [optional] either equal to pixs1 for in-place,
 *                             or NULL
 * \param[in]    pixs1         blendee; depth > 1
 * \param[in]    pixs2         blender, any depth; typically, the area of
 *                             pixs2 is smaller than pixs1
 * \param[in]    x,y           origin [UL corner] of pixs2 relative to
 *                             the origin of pixs1
 * \param[in]    fract         blending fraction
 * \param[in]    transparent   1 to use transparency; 0 otherwise
 * \param[in]    transpix      pixel color in pixs2 that is to be transparent
 * \return  pixd, or null on error
 *
 * <pre>
 * Notes:
 *      (1) For inplace operation (pixs1 must be 32 bpp), call it this way:
 *            pixBlendColor(pixs1, pixs1, pixs2, ...)
 *      (2) For generating a new pixd:
 *            pixd = pixBlendColor(NULL, pixs1, pixs2, ...)
 *      (3) If pixs2 is not 32 bpp rgb, it is converted.
 *      (4) Clipping of pixs2 to pixs1 is done in the inner pixel loop.
 *      (5) If pixs1 has a colormap, it is removed to generate a 32 bpp pix.
 *      (6) If pixs1 has depth < 32, it is unpacked to generate a 32 bpp pix.
 *      (7) If transparent = 0, the blending fraction (fract) is
 *          applied equally to all pixels.
 *      (8) If transparent = 1, all pixels of value transpix (typically
 *          either 0 or 0xffffff00) in pixs2 are transparent in the blend.
 * </pre>
 */
PIX *
pixBlendColor(PIX       *pixd,
              PIX       *pixs1,
              PIX       *pixs2,
              l_int32    x,
              l_int32    y,
              l_float32  fract,
              l_int32    transparent,
              l_uint32   transpix)
{
l_int32    i, j, wc, hc, w, h, wplc, wpld;
l_int32    rval, gval, bval, rcval, gcval, bcval;
l_uint32   cval32, val32;
l_uint32  *linec, *lined, *datac, *datad;
PIX       *pixc;

    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, NULL);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, NULL);
    if (pixGetDepth(pixs1) == 1)
        return (PIX *)ERROR_PTR("pixs1 is 1 bpp", __func__, NULL);
    if (pixd == pixs1 && pixGetDepth(pixs1) != 32)
        return (PIX *)ERROR_PTR("inplace; pixs1 not 32 bpp", __func__, NULL);
    if (pixd && (pixd != pixs1))
        return (PIX *)ERROR_PTR("pixd must be NULL or pixs1", __func__, NULL);
    if (fract < 0.0 || fract > 1.0) {
        L_WARNING("fract must be in [0.0, 1.0]; setting to 0.5\n", __func__);
        fract = 0.5;
    }

        /* If pixd != null, we know that it is equal to pixs1 and
         * that pixs1 is 32 bpp rgb, so that an in-place operation
         * can be done.  Otherwise, pixConvertTo32() will remove a
         * colormap from pixs1 if it exists and unpack to 32 bpp
         * (if necessary) to do the blending on a new 32 bpp Pix. */
    if (!pixd)
        pixd = pixConvertTo32(pixs1);
    pixGetDimensions(pixd, &w, &h, NULL);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);
    pixc = pixConvertTo32(pixs2);  /* blend with 32 bpp rgb */
    pixGetDimensions(pixc, &wc, &hc, NULL);
    datac = pixGetData(pixc);
    wplc = pixGetWpl(pixc);

        /* Check limits for src1, in case clipping was not done */
    for (i = 0; i < hc; i++) {
        /*
         * The basic logic for this blending is:
         *      p -->  (1 - f) * p + f * c
         * for each color channel.  c is a color component of the blender.
         * All values are normalized to [0...1].
         */
        if (i + y < 0  || i + y >= h) continue;
        linec = datac + i * wplc;
        lined = datad + (i + y) * wpld;
        for (j = 0; j < wc; j++) {
            if (j + x < 0  || j + x >= w) continue;
            cval32 = *(linec + j);
            if (transparent == 0 ||
                ((cval32 & 0xffffff00) != (transpix & 0xffffff00))) {
                val32 = *(lined + j + x);
                extractRGBValues(cval32, &rcval, &gcval, &bcval);
                extractRGBValues(val32, &rval, &gval, &bval);
                rval = (l_int32)((1. - fract) * rval + fract * rcval);
                gval = (l_int32)((1. - fract) * gval + fract * gcval);
                bval = (l_int32)((1. - fract) * bval + fract * bcval);
                composeRGBPixel(rval, gval, bval, &val32);
                *(lined + j + x) = val32;
            }
        }
    }

    pixDestroy(&pixc);
    return pixd;
}


/*
 * \brief    pixBlendColorByChannel()
 *
 * \param[in]    pixd          [optional] either equal to pixs1 for in-place,
 *                             or NULL
 * \param[in]    pixs1         blendee; depth > 1
 * \param[in]    pixs2         blender, any depth; typically, the area of
 *                             pixs2 is smaller than pixs1
 * \param[in]    x,y           origin [UL corner] of pixs2 relative to
 *                             the origin of pixs1
 * \param[in]    rfract        blending fraction in red channel
 * \param[in]    gfract        blending fraction in green channel
 * \param[in]    bfract        blending fraction in blue channel
 * \param[in]    transparent   1 to use transparency; 0 otherwise
 * \param[in]    transpix      pixel color in pixs2 that is to be transparent
 * \return  pixd if OK; pixd on error
 *
 * <pre>
 * Notes:
 *      (1) This generalizes pixBlendColor() in two ways:
 *          (a) The mixing fraction is specified per channel.
 *          (b) The mixing fraction may be < 0 or > 1, in which case,
 *              the min or max of two images are taken, respectively.
 *      (2) Specifically,
 *          for p = pixs1[i], c = pixs2[i], f = fract[i], i = 1, 2, 3:
 *              f < 0.0:          p --> min(p, c)
 *              0.0 <= f <= 1.0:  p --> (1 - f) * p + f * c
 *              f > 1.0:          p --> max(a, c)
 *          Special cases:
 *              f = 0:   p --> p
 *              f = 1:   p --> c
 *      (3) See usage notes in pixBlendColor()
 *      (4) pixBlendColor() would be equivalent to
 *            pixBlendColorChannel(..., fract, fract, fract, ...);
 *          at a small cost of efficiency.
 * </pre>
 */
PIX *
pixBlendColorByChannel(PIX       *pixd,
                       PIX       *pixs1,
                       PIX       *pixs2,
                       l_int32    x,
                       l_int32    y,
                       l_float32  rfract,
                       l_float32  gfract,
                       l_float32  bfract,
                       l_int32    transparent,
                       l_uint32   transpix)
{
l_int32    i, j, wc, hc, w, h, wplc, wpld;
l_int32    rval, gval, bval, rcval, gcval, bcval;
l_uint32   cval32, val32;
l_uint32  *linec, *lined, *datac, *datad;
PIX       *pixc;

    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, pixd);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, pixd);
    if (pixGetDepth(pixs1) == 1)
        return (PIX *)ERROR_PTR("pixs1 is 1 bpp", __func__, pixd);
    if (pixd == pixs1 && pixGetDepth(pixs1) != 32)
        return (PIX *)ERROR_PTR("inplace; pixs1 not 32 bpp", __func__, pixd);
    if (pixd && (pixd != pixs1))
        return (PIX *)ERROR_PTR("pixd must be NULL or pixs1", __func__, pixd);

        /* If pixd != NULL, we know that it is equal to pixs1 and
         * that pixs1 is 32 bpp rgb, so that an in-place operation
         * can be done.  Otherwise, pixConvertTo32() will remove a
         * colormap from pixs1 if it exists and unpack to 32 bpp
         * (if necessary) to do the blending on a new 32 bpp Pix. */
    if (!pixd)
        pixd = pixConvertTo32(pixs1);
    pixGetDimensions(pixd, &w, &h, NULL);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);
    pixc = pixConvertTo32(pixs2);
    pixGetDimensions(pixc, &wc, &hc, NULL);
    datac = pixGetData(pixc);
    wplc = pixGetWpl(pixc);

        /* Check limits for src1, in case clipping was not done */
    for (i = 0; i < hc; i++) {
        if (i + y < 0  || i + y >= h) continue;
        linec = datac + i * wplc;
        lined = datad + (i + y) * wpld;
        for (j = 0; j < wc; j++) {
            if (j + x < 0  || j + x >= w) continue;
            cval32 = *(linec + j);
            if (transparent == 0 ||
                ((cval32 & 0xffffff00) != (transpix & 0xffffff00))) {
                val32 = *(lined + j + x);
                extractRGBValues(cval32, &rcval, &gcval, &bcval);
                extractRGBValues(val32, &rval, &gval, &bval);
                rval = blendComponents(rval, rcval, rfract);
                gval = blendComponents(gval, gcval, gfract);
                bval = blendComponents(bval, bcval, bfract);
                composeRGBPixel(rval, gval, bval, &val32);
                *(lined + j + x) = val32;
            }
        }
    }

    pixDestroy(&pixc);
    return pixd;
}


static l_int32
blendComponents(l_int32    a,
                l_int32    b,
                l_float32  fract)
{
    if (fract < 0.)
        return ((a < b) ? a : b);
    if (fract > 1.)
        return ((a > b) ? a : b);
    return (l_int32)((1. - fract) * a + fract * b);
}


/*!
 * \brief   pixBlendGrayAdapt()
 *
 * \param[in]    pixd    [optional] either equal to pixs1 for in-place, or NULL
 * \param[in]    pixs1   blendee; depth > 1
 * \param[in]    pixs2   blender, any depth; typically, the area of
 *                       pixs2 is smaller than pixs1
 * \param[in]    x,y     origin [UL corner] of pixs2 relative to
 *                       the origin of pixs1; can be < 0
 * \param[in]    fract   blending fraction
 * \param[in]    shift   >= 0 but <= 128: shift of zero blend value from
 *                       median source; use -1 for default value;
 * \return  pixd if OK; pixs1 on error
 *
 * <pre>
 * Notes:
 *      (1) For inplace operation (pixs1 not cmapped), call it this way:
 *            pixBlendGrayAdapt(pixs1, pixs1, pixs2, ...)
 *          For generating a new pixd:
 *            pixd = pixBlendGrayAdapt(NULL, pixs1, pixs2, ...)
 *      (2) Clipping of pixs2 to pixs1 is done in the inner pixel loop.
 *      (3) If pixs1 has a colormap, it is removed.
 *      (4) If pixs1 has depth < 8, it is unpacked to generate a 8 bpp pix.
 *      (5) This does a blend with inverse.  Whereas in pixGlendGray(), the
 *          zero blend point is where the blendee pixel is 128, here
 *          the zero blend point is found adaptively, with respect to the
 *          median of the blendee region.  If the median is < 128,
 *          the zero blend point is found from
 *              median + shift.
 *          Otherwise, if the median >= 128, the zero blend point is
 *              median - shift.
 *          The purpose of shifting the zero blend point away from the
 *          median is to prevent a situation in pixBlendGray() where
 *          the median is 128 and the blender is not visible.
 *          The default value of shift is 64.
 *      (6) After processing pixs1, it is either 8 bpp or 32 bpp:
 *          ~ if 8 bpp, the fraction of pixs2 is mixed with pixs1.
 *          ~ if 32 bpp, each component of pixs1 is mixed with
 *            the same fraction of pixs2.
 *      (7) The darker the blender, the more it mixes with the blendee.
 *          A blender value of 0 has maximum mixing; a value of 255
 *          has no mixing and hence is transparent.
 * </pre>
 */
PIX *
pixBlendGrayAdapt(PIX       *pixd,
                  PIX       *pixs1,
                  PIX       *pixs2,
                  l_int32    x,
                  l_int32    y,
                  l_float32  fract,
                  l_int32    shift)
{
l_int32    i, j, d, wc, hc, w, h, wplc, wpld, delta, overlap;
l_int32    rval, gval, bval, cval, dval, mval, median, pivot;
l_uint32   val32;
l_uint32  *linec, *lined, *datac, *datad;
l_float32  fmedian, factor;
BOX       *box, *boxt;
PIX       *pixc, *pix1, *pix2;

    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, pixd);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, pixd);
    if (pixGetDepth(pixs1) == 1)
        return (PIX *)ERROR_PTR("pixs1 is 1 bpp", __func__, pixd);
    if (pixd == pixs1 && pixGetColormap(pixs1))
        return (PIX *)ERROR_PTR("can't do in-place with cmap", __func__, pixd);
    if (pixd && (pixd != pixs1))
        return (PIX *)ERROR_PTR("pixd must be NULL or pixs1", __func__, pixd);
    if (fract < 0.0 || fract > 1.0) {
        L_WARNING("fract must be in [0.0, 1.0]; setting to 0.5\n", __func__);
        fract = 0.5;
    }
    if (shift == -1) shift = 64;   /* default value */
    if (shift < 0 || shift > 127) {
        L_WARNING("invalid shift; setting to 64\n", __func__);
        shift = 64;
    }

        /* Test for overlap */
    pixGetDimensions(pixs1, &w, &h, NULL);
    pixGetDimensions(pixs2, &wc, &hc, NULL);
    box = boxCreate(x, y, wc, hc);
    boxt = boxCreate(0, 0, w, h);
    boxIntersects(box, boxt, &overlap);
    boxDestroy(&boxt);
    if (!overlap) {
        boxDestroy(&box);
        return (PIX *)ERROR_PTR("no image overlap", __func__, pixd);
    }

        /* If pixd != NULL, we know that it is equal to pixs1 and
         * that pixs1 does not have a colormap, so that an in-place operation
         * can be done.  Otherwise, remove colormap from pixs1 if
         * it exists and unpack to at least 8 bpp if necessary,
         * to do the blending on a new pix. */
    if (!pixd) {
        pix1 = pixRemoveColormap(pixs1, REMOVE_CMAP_BASED_ON_SRC);
        if (pixGetDepth(pix1) < 8)
            pix2 = pixConvertTo8(pix1, FALSE);
        else
            pix2 = pixClone(pix1);
        pixd = pixCopy(NULL, pix2);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }

        /* Get the median value in the region of blending */
    pix1 = pixClipRectangle(pixd, box, NULL);
    pix2 = pixConvertTo8(pix1, 0);
    pixGetRankValueMasked(pix2, NULL, 0, 0, 1, 0.5, &fmedian, NULL);
    median = (l_int32)(fmedian + 0.5);
    if (median < 128)
        pivot = median + shift;
    else
        pivot = median - shift;
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    boxDestroy(&box);

        /* Process over src2; clip to src1. */
    d = pixGetDepth(pixd);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);
    pixc = pixConvertTo8(pixs2, 0);
    datac = pixGetData(pixc);
    wplc = pixGetWpl(pixc);
    for (i = 0; i < hc; i++) {
        if (i + y < 0  || i + y >= h) continue;
        linec = datac + i * wplc;
        lined = datad + (i + y) * wpld;
        switch (d)
        {
        case 8:
                /*
                 * For 8 bpp, the dest pix is shifted by an amount
                 * proportional to the distance from the pivot value,
                 * and to the darkness of src2.  In no situation will it
                 * pass the pivot value in intensity.
                 * The basic logic is:
                 *     d  -->  d + f * (np - d) * (1 - c)
                 * where np, d and c are normalized pixel values for
                 * the pivot, src1 and src2, respectively, with normalization
                 * to 255.
                 */
            for (j = 0; j < wc; j++) {
                if (j + x < 0  || j + x >= w) continue;
                dval = GET_DATA_BYTE(lined, j + x);
                cval = GET_DATA_BYTE(linec, j);
                delta = (pivot - dval) * (255 - cval) / 256;
                dval += (l_int32)(fract * delta + 0.5);
                SET_DATA_BYTE(lined, j + x, dval);
            }
            break;
        case 32:
                /*
                 * For 32 bpp, the dest pix is shifted by an amount
                 * proportional to the max component distance from the
                 * pivot value, and to the darkness of src2.  Each component
                 * is shifted by the same fraction, either up or down,
                 * depending on the shift direction (which is toward the
                 * pivot).   The basic logic for the red component is:
                 *     r  -->  r + f * (np - m) * (1 - c) * (r / m)
                 * where np, r, m and c are normalized pixel values for
                 * the pivot, the r component of src1, the max component
                 * of src1, and src2, respectively, again with normalization
                 * to 255.  Likewise for the green and blue components.
                 */
            for (j = 0; j < wc; j++) {
                if (j + x < 0  || j + x >= w) continue;
                cval = GET_DATA_BYTE(linec, j);
                val32 = *(lined + j + x);
                extractRGBValues(val32, &rval, &gval, &bval);
                mval = L_MAX(rval, gval);
                mval = L_MAX(mval, bval);
                mval = L_MAX(mval, 1);
                delta = (pivot - mval) * (255 - cval) / 256;
                factor = fract * delta / mval;
                rval += (l_int32)(factor * rval + 0.5);
                gval += (l_int32)(factor * gval + 0.5);
                bval += (l_int32)(factor * bval + 0.5);
                composeRGBPixel(rval, gval, bval, &val32);
                *(lined + j + x) = val32;
            }
            break;
        default:
            break;   /* shouldn't happen */
        }
    }

    pixDestroy(&pixc);
    return pixd;
}


/*!
 * \brief   pixFadeWithGray()
 *
 * \param[in]    pixs     colormapped or 8 bpp or 32 bpp
 * \param[in]    pixb     8 bpp blender
 * \param[in]    factor   multiplicative factor to apply to blender value
 * \param[in]    type     L_BLEND_TO_WHITE, L_BLEND_TO_BLACK
 * \return  pixd, or null on error
 *
 * <pre>
 * Notes:
 *      (1) This function combines two pix aligned to the UL corner; they
 *          need not be the same size.
 *      (2) Each pixel in pixb is multiplied by 'factor' divided by 255, and
 *          clipped to the range [0 ... 1].  This gives the fade fraction
 *          to be applied to pixs.  Fade either to white (L_BLEND_TO_WHITE)
 *          or to black (L_BLEND_TO_BLACK).
 * </pre>
 */
PIX *
pixFadeWithGray(PIX       *pixs,
                PIX       *pixb,
                l_float32  factor,
                l_int32    type)
{
l_int32    i, j, w, h, d, wb, hb, db, wd, hd, wplb, wpld;
l_int32    valb, vald, nvald, rval, gval, bval, nrval, ngval, nbval;
l_float32  nfactor, fract;
l_uint32   val32, nval32;
l_uint32  *lined, *datad, *lineb, *datab;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (!pixb)
        return (PIX *)ERROR_PTR("pixb not defined", __func__, NULL);
    if (pixGetDepth(pixs) == 1)
        return (PIX *)ERROR_PTR("pixs is 1 bpp", __func__, NULL);
    pixGetDimensions(pixb, &wb, &hb, &db);
    if (db != 8)
        return (PIX *)ERROR_PTR("pixb not 8 bpp", __func__, NULL);
    if (factor < 0.0 || factor > 255.0)
        return (PIX *)ERROR_PTR("factor not in [0.0...255.0]", __func__, NULL);
    if (type != L_BLEND_TO_WHITE && type != L_BLEND_TO_BLACK)
        return (PIX *)ERROR_PTR("invalid fade type", __func__, NULL);

        /* Remove colormap if it exists; otherwise copy */
    pixd = pixRemoveColormapGeneral(pixs, REMOVE_CMAP_BASED_ON_SRC, L_COPY);
    pixGetDimensions(pixd, &wd, &hd, &d);
    w = L_MIN(wb, wd);
    h = L_MIN(hb, hd);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    datab = pixGetData(pixb);
    wplb = pixGetWpl(pixb);

        /* The basic logic for this blending is, for each component p of pixs:
         *   fade-to-white:   p -->  p + (f * c) * (1 - p)
         *   fade-to-black:   p -->  p - (f * c) * p
         * with c being the 8 bpp blender pixel of pixb, and with both
         * p and c normalized to [0...1]. */
    nfactor = factor / 255.;
    for (i = 0; i < h; i++) {
        lineb = datab + i * wplb;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            valb = GET_DATA_BYTE(lineb, j);
            fract = nfactor * (l_float32)valb;
            fract = L_MIN(fract, 1.0);
            if (d == 8) {
                vald = GET_DATA_BYTE(lined, j);
                if (type == L_BLEND_TO_WHITE)
                    nvald = vald + (l_int32)(fract * (255. - (l_float32)vald));
                else  /* L_BLEND_TO_BLACK */
                    nvald = vald - (l_int32)(fract * (l_float32)vald);
                SET_DATA_BYTE(lined, j, nvald);
            } else {  /* d == 32 */
                val32 = lined[j];
                extractRGBValues(val32, &rval, &gval, &bval);
                if (type == L_BLEND_TO_WHITE) {
                    nrval = rval + (l_int32)(fract * (255. - (l_float32)rval));
                    ngval = gval + (l_int32)(fract * (255. - (l_float32)gval));
                    nbval = bval + (l_int32)(fract * (255. - (l_float32)bval));
                } else {
                    nrval = rval - (l_int32)(fract * (l_float32)rval);
                    ngval = gval - (l_int32)(fract * (l_float32)gval);
                    nbval = bval - (l_int32)(fract * (l_float32)bval);
                }
                composeRGBPixel(nrval, ngval, nbval, &nval32);
                lined[j] = nval32;
            }
        }
    }

    return pixd;
}


/*
 * \brief   pixBlendHardLight()
 *
 * \param[in]   pixd    either NULL or equal to pixs1 for in-place
 * \param[in]   pixs1   blendee; depth > 1, may be cmapped
 * \param[in]   pixs2   blender, 8 or 32 bpp; may be colormapped;
 *                      typ. smaller in size than pixs1
 * \param[in]   x,y     origin [UL corner] of pixs2 relative to
 *                      the origin of pixs1
 * \param[in]   fract   blending fraction, or 'opacity factor'
 * \return   pixd if OK; pixs1 on error
 *
 * <pre>
 * Notes:
 *      (1) pixs2 must be 8 or 32 bpp; either may have a colormap.
 *      (2) Clipping of pixs2 to pixs1 is done in the inner pixel loop.
 *      (3) Only call in-place if pixs1 is not colormapped.
 *      (4) If pixs1 has a colormap, it is removed to generate either an
 *          8 or 32 bpp pix, depending on the colormap.
 *      (5) For inplace operation, call it this way:
 *            pixBlendHardLight(pixs1, pixs1, pixs2, ...)
 *      (6) For generating a new pixd:
 *            pixd = pixBlendHardLight(NULL, pixs1, pixs2, ...)
 *      (7) This is a generalization of the usual hard light blending,
 *          where fract == 1.0.
 *      (8) "Overlay" blending is the same as hard light blending, with
 *          fract == 1.0, except that the components are switched
 *          in the test.  (Note that the result is symmetric in the
 *          two components.)
 *      (9) See, e.g.:
 *           http://www.pegtop.net/delphi/articles/blendmodes/hardlight.htm
 *           http://www.digitalartform.com/imageArithmetic.htm
 *      (10) This function was built by Paco Galanes.
 * </pre>
 */
PIX *
pixBlendHardLight(PIX       *pixd,
                  PIX       *pixs1,
                  PIX       *pixs2,
                  l_int32    x,
                  l_int32    y,
                  l_float32  fract)
{
l_int32    i, j, w, h, d, wc, hc, dc, wplc, wpld;
l_int32    cval, dval, rcval, gcval, bcval, rdval, gdval, bdval;
l_uint32   cval32, dval32;
l_uint32  *linec, *lined, *datac, *datad;
PIX       *pixc, *pixt;

    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, pixd);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, pixd);
    pixGetDimensions(pixs1, &w, &h, &d);
    pixGetDimensions(pixs2, &wc, &hc, &dc);
    if (d == 1)
        return (PIX *)ERROR_PTR("pixs1 is 1 bpp", __func__, pixd);
    if (dc != 8 && dc != 32)
        return (PIX *)ERROR_PTR("pixs2 not 8 or 32 bpp", __func__, pixd);
    if (pixd && (pixd != pixs1))
        return (PIX *)ERROR_PTR("inplace and pixd != pixs1", __func__, pixd);
    if (pixd == pixs1 && pixGetColormap(pixs1))
        return (PIX *)ERROR_PTR("inplace and pixs1 cmapped", __func__, pixd);
    if (pixd && d != 8 && d != 32)
        return (PIX *)ERROR_PTR("inplace and not 8 or 32 bpp", __func__, pixd);

    if (fract < 0.0 || fract > 1.0) {
        L_WARNING("fract must be in [0.0, 1.0]; setting to 0.5\n", __func__);
        fract = 0.5;
    }

        /* If pixs2 has a colormap, remove it */
    pixc = pixRemoveColormap(pixs2, REMOVE_CMAP_BASED_ON_SRC);  /* clone ok */
    dc = pixGetDepth(pixc);

        /* There are 4 cases:
         *    * pixs1 has or doesn't have a colormap
         *    * pixc is either 8 or 32 bpp
         * In all situations, if pixs has a colormap it must be removed,
         * and pixd must have a depth that is equal to or greater than pixc. */
    if (dc == 32) {
        if (pixGetColormap(pixs1)) {  /* pixd == NULL */
            pixd = pixRemoveColormap(pixs1, REMOVE_CMAP_TO_FULL_COLOR);
        } else {
            if (!pixd) {
                pixd = pixConvertTo32(pixs1);
            } else {
                pixt = pixConvertTo32(pixs1);
                pixCopy(pixd, pixt);
                pixDestroy(&pixt);
            }
        }
        d = 32;
    } else {  /* dc == 8 */
        if (pixGetColormap(pixs1))   /* pixd == NULL */
            pixd = pixRemoveColormap(pixs1, REMOVE_CMAP_BASED_ON_SRC);
        else
            pixd = pixCopy(pixd, pixs1);
        d = pixGetDepth(pixd);
    }

    if (!(d == 8 && dc == 8) &&   /* 3 cases only */
        !(d == 32 && dc == 8) &&
        !(d == 32 && dc == 32)) {
        pixDestroy(&pixc);
        return (PIX *)ERROR_PTR("bad! -- invalid depth combo!", __func__, pixd);
    }

    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);
    datac = pixGetData(pixc);
    wplc = pixGetWpl(pixc);
    for (i = 0; i < hc; i++) {
        if (i + y < 0  || i + y >= h) continue;
        linec = datac + i * wplc;
        lined = datad + (i + y) * wpld;
        for (j = 0; j < wc; j++) {
            if (j + x < 0  || j + x >= w) continue;
            if (d == 8 && dc == 8) {
                dval = GET_DATA_BYTE(lined, x + j);
                cval = GET_DATA_BYTE(linec, j);
                dval = blendHardLightComponents(dval, cval, fract);
                SET_DATA_BYTE(lined, x + j, dval);
            } else if (d == 32 && dc == 8) {
                dval32 = *(lined + x + j);
                extractRGBValues(dval32, &rdval, &gdval, &bdval);
                cval = GET_DATA_BYTE(linec, j);
                rdval = blendHardLightComponents(rdval, cval, fract);
                gdval = blendHardLightComponents(gdval, cval, fract);
                bdval = blendHardLightComponents(bdval, cval, fract);
                composeRGBPixel(rdval, gdval, bdval, &dval32);
                *(lined + x + j) = dval32;
            } else if (d == 32 && dc == 32) {
                dval32 = *(lined + x + j);
                extractRGBValues(dval32, &rdval, &gdval, &bdval);
                cval32 = *(linec + j);
                extractRGBValues(cval32, &rcval, &gcval, &bcval);
                rdval = blendHardLightComponents(rdval, rcval, fract);
                gdval = blendHardLightComponents(gdval, gcval, fract);
                bdval = blendHardLightComponents(bdval, bcval, fract);
                composeRGBPixel(rdval, gdval, bdval, &dval32);
                *(lined + x + j) = dval32;
            }
        }
    }

    pixDestroy(&pixc);
    return pixd;
}


/*
 * \brief   blendHardLightComponents()
 *
 * \param[in]   a        8 bpp blendee component
 * \param[in]   b        8 bpp blender component
 * \param[in]   fract    fraction of blending; use 1.0 for usual definition
 * \return   blended 8 bpp component
 *
 * <pre>
 * Notes:
 *
 *    The basic logic for this blending is:
 *      b < 0.5:
 *          a --> 2 * a * (0.5 - f * (0.5 - b))
 *      b >= 0.5:
 *          a --> 1 - 2 * (1 - a) * (1 - (0.5 - f * (0.5 - b)))
 *
 *    In the limit that f == 1 (standard hardlight blending):
 *      b < 0.5:   a --> 2 * a * b
 *                     or
 *                 a --> a - a * (1 - 2 * b)
 *      b >= 0.5:  a --> 1 - 2 * (1 - a) * (1 - b)
 *                     or
 *                 a --> a + (1 - a) * (2 * b - 1)
 *
 *    You can see that for standard hardlight blending:
 *      b < 0.5:   a is pushed linearly with b down to 0
 *      b >= 0.5:  a is pushed linearly with b up to 1
 *    a is unchanged if b = 0.5
 *
 *    Our opacity factor f reduces the deviation of b from 0.5:
 *      f == 0:  b -->  0.5, so no blending occurs
 *      f == 1:  b -->  b, so we get full conventional blending
 *
 *    There is a variant of hardlight blending called "softlight" blending:
 *    (e.g., http://jswidget.com/blog/tag/hard-light/)
 *      b < 0.5:
 *          a --> a - a * (0.5 - b) * (1 - Abs(2 * a - 1))
 *      b >= 0.5:
 *          a --> a + (1 - a) * (b - 0.5) * (1 - Abs(2 * a - 1))
 *    which limits the amount that 'a' can be moved to a maximum of
 *    halfway toward 0 or 1, and further reduces it as 'a' moves
 *    away from 0.5.
 *    As you can see, there are a nearly infinite number of different
 *    blending formulas that can be conjured up.
 * </pre>
 */
static l_int32 blendHardLightComponents(l_int32    a,
                                        l_int32    b,
                                        l_float32  fract)
{
    if (b < 0x80) {
        b = 0x80 - (l_int32)(fract * (0x80 - b));
        return (a * b) >> 7;
    } else {
        b = 0x80 + (l_int32)(fract * (b - 0x80));
        return  0xff - (((0xff - b) * (0xff - a)) >> 7);
    }
}


/*-------------------------------------------------------------*
 *               Blending two colormapped images               *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixBlendCmap()
 *
 * \param[in]    pixs     2, 4 or 8 bpp, with colormap
 * \param[in]    pixb     colormapped blender
 * \param[in]    x, y     UL corner of blender relative to pixs
 * \param[in]    sindex   colormap index of pixels in pixs to be changed
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This function combines two colormaps, and replaces the pixels
 *          in pixs that have a specified color value with those in pixb.
 *      (2) sindex must be in the existing colormap; otherwise an
 *          error is returned.  In use, sindex will typically be the index
 *          for white (255, 255, 255).
 *      (3) Blender colors that already exist in the colormap are used;
 *          others are added.  If any blender colors cannot be
 *          stored in the colormap, an error is returned.
 *      (4) In the implementation, a mapping is generated from each
 *          original blender colormap index to the corresponding index
 *          in the expanded colormap for pixs.  Then for each pixel in
 *          pixs with value sindex, and which is covered by a blender pixel,
 *          the new index corresponding to the blender pixel is substituted
 *          for sindex.
 * </pre>
 */
l_ok
pixBlendCmap(PIX     *pixs,
             PIX     *pixb,
             l_int32  x,
             l_int32  y,
             l_int32  sindex)
{
l_int32    rval, gval, bval;
l_int32    i, j, w, h, d, ncb, wb, hb, wpls;
l_int32    index, val, nadded;
l_int32    lut[256];
l_uint32   pval;
l_uint32  *lines, *datas;
PIXCMAP   *cmaps, *cmapb, *cmapsc;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!pixb)
        return ERROR_INT("pixb not defined", __func__, 1);
    if ((cmaps = pixGetColormap(pixs)) == NULL)
        return ERROR_INT("no colormap in pixs", __func__, 1);
    if ((cmapb = pixGetColormap(pixb)) == NULL)
        return ERROR_INT("no colormap in pixb", __func__, 1);
    ncb = pixcmapGetCount(cmapb);

    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 2 && d != 4 && d != 8)
        return ERROR_INT("depth not in {2,4,8}", __func__, 1);

        /* Make a copy of cmaps; we'll add to this if necessary
         * and substitute at the end if we found there was enough room
         * to hold all the new colors. */
    cmapsc = pixcmapCopy(cmaps);

        /* Add new colors if necessary; get mapping array between
         * cmaps and cmapb. */
    for (i = 0, nadded = 0; i < ncb; i++) {
        pixcmapGetColor(cmapb, i, &rval, &gval, &bval);
        if (pixcmapGetIndex(cmapsc, rval, gval, bval, &index)) { /* not found */
            if (pixcmapAddColor(cmapsc, rval, gval, bval)) {
                pixcmapDestroy(&cmapsc);
                return ERROR_INT("not enough room in cmaps", __func__, 1);
            }
            lut[i] = pixcmapGetCount(cmapsc) - 1;
            nadded++;
        } else {
            lut[i] = index;
        }
    }

        /* Replace cmaps if colors have been added. */
    if (nadded == 0)
        pixcmapDestroy(&cmapsc);
    else
        pixSetColormap(pixs, cmapsc);

        /* Replace each pixel value sindex by mapped colormap index when
         * a blender pixel in pixbc overlays it. */
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    pixGetDimensions(pixb, &wb, &hb, NULL);
    for (i = 0; i < hb; i++) {
        if (i + y < 0  || i + y >= h) continue;
        lines = datas + (y + i) * wpls;
        for (j = 0; j < wb; j++) {
            if (j + x < 0  || j + x >= w) continue;
            switch (d) {
            case 2:
                val = GET_DATA_DIBIT(lines, x + j);
                if (val == sindex) {
                    pixGetPixel(pixb, j, i, &pval);
                    SET_DATA_DIBIT(lines, x + j, lut[pval]);
                }
                break;
            case 4:
                val = GET_DATA_QBIT(lines, x + j);
                if (val == sindex) {
                    pixGetPixel(pixb, j, i, &pval);
                    SET_DATA_QBIT(lines, x + j, lut[pval]);
                }
                break;
            case 8:
                val = GET_DATA_BYTE(lines, x + j);
                if (val == sindex) {
                    pixGetPixel(pixb, j, i, &pval);
                    SET_DATA_BYTE(lines, x + j, lut[pval]);
                }
                break;
            default:
                return ERROR_INT("depth not in {2,4,8}", __func__, 1);
            }
        }
    }

    return 0;
}


/*---------------------------------------------------------------------*
 *                  Blending two images using a third                  *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixBlendWithGrayMask()
 *
 * \param[in]    pixs1   8 bpp gray, rgb, rgba or colormapped
 * \param[in]    pixs2   8 bpp gray, rgb, rgba or colormapped
 * \param[in]    pixg    [optional] 8 bpp gray, for transparency of pixs2;
 *                       can be null
 * \param[in]    x, y    UL corner of pixs2 and pixg with respect to pixs1
 * \return  pixd blended image, or null on error
 *
 * <pre>
 * Notes:
 *      (1) The result is 8 bpp grayscale if both pixs1 and pixs2 are
 *          8 bpp gray.  Otherwise, the result is 32 bpp rgb.
 *      (2) pixg is an 8 bpp transparency image, where 0 is transparent
 *          and 255 is opaque.  It determines the transparency of pixs2
 *          when applied over pixs1.  It can be null if pixs2 is rgba,
 *          in which case we use the alpha component of pixs2.
 *      (3) If pixg exists, it need not be the same size as pixs2.
 *          However, we assume their UL corners are aligned with each other,
 *          and placed at the location (x, y) in pixs1.
 *      (4) The pixels in pixd are a combination of those in pixs1
 *          and pixs2, where the amount from pixs2 is proportional to
 *          the value of the pixel (p) in pixg, and the amount from pixs1
 *          is proportional to (255 - p).  Thus pixg is a transparency
 *          image (usually called an alpha blender) where each pixel
 *          can be associated with a pixel in pixs2, and determines
 *          the amount of the pixs2 pixel in the final result.
 *          For example, if pixg is all 0, pixs2 is transparent and
 *          the result in pixd is simply pixs1.
 *      (5) A typical use is for the pixs2/pixg combination to be
 *          a small watermark that is applied to pixs1.
 * </pre>
 */
PIX *
pixBlendWithGrayMask(PIX     *pixs1,
                     PIX     *pixs2,
                     PIX     *pixg,
                     l_int32  x,
                     l_int32  y)
{
l_int32    w1, h1, d1, w2, h2, d2, spp, wg, hg, wmin, hmin, wpld, wpls, wplg;
l_int32    i, j, val, dval, sval;
l_int32    drval, dgval, dbval, srval, sgval, sbval;
l_uint32   dval32, sval32;
l_uint32  *datad, *datas, *datag, *lined, *lines, *lineg;
l_float32  fract;
PIX       *pixr1, *pixr2, *pix1, *pix2, *pixg2, *pixd;

    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, NULL);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, NULL);
    pixGetDimensions(pixs1, &w1, &h1, &d1);
    pixGetDimensions(pixs2, &w2, &h2, &d2);
    if (d1 == 1 || d2 == 1)
        return (PIX *)ERROR_PTR("pixs1 or pixs2 is 1 bpp", __func__, NULL);
    if (pixg) {
        if (pixGetDepth(pixg) != 8)
            return (PIX *)ERROR_PTR("pixg not 8 bpp", __func__, NULL);
        pixGetDimensions(pixg, &wg, &hg, NULL);
        wmin = L_MIN(w2, wg);
        hmin = L_MIN(h2, hg);
        pixg2 = pixClone(pixg);
    } else {  /* use the alpha component of pixs2 */
        spp = pixGetSpp(pixs2);
        if (d2 != 32 || spp != 4)
            return (PIX *)ERROR_PTR("no alpha; pixs2 not rgba", __func__, NULL);
        wmin = w2;
        hmin = h2;
        pixg2 = pixGetRGBComponent(pixs2, L_ALPHA_CHANNEL);
    }

        /* Remove colormaps if they exist; clones are OK */
    pixr1 = pixRemoveColormap(pixs1, REMOVE_CMAP_BASED_ON_SRC);
    pixr2 = pixRemoveColormap(pixs2, REMOVE_CMAP_BASED_ON_SRC);

        /* Regularize to the same depth if necessary */
    d1 = pixGetDepth(pixr1);
    d2 = pixGetDepth(pixr2);
    if (d1 == 32) {  /* convert d2 to rgb if necessary */
        pix1 = pixClone(pixr1);
        if (d2 != 32)
            pix2 = pixConvertTo32(pixr2);
        else
            pix2 = pixClone(pixr2);
    } else if (d2 == 32) {   /* and d1 != 32; convert to 32 */
        pix2 = pixClone(pixr2);
        pix1 = pixConvertTo32(pixr1);
    } else {  /* both are 8 bpp or less */
        pix1 = pixConvertTo8(pixr1, FALSE);
        pix2 = pixConvertTo8(pixr2, FALSE);
    }
    pixDestroy(&pixr1);
    pixDestroy(&pixr2);

        /* Output a copy of pix1 to avoid side-effecting input pixs1 */
    pixd = pixCopy(NULL, pix1);
    pixDestroy(&pix1);

        /* Sanity check: both either 8 or 32 bpp */
    d1 = pixGetDepth(pixd);
    d2 = pixGetDepth(pix2);
    if (!pixd || d1 != d2 || (d1 != 8 && d1 != 32)) {
        pixDestroy(&pixd);
        pixDestroy(&pix2);
        pixDestroy(&pixg2);
        return (PIX *)ERROR_PTR("depths not regularized! bad!", __func__, NULL);
    }

        /* Blend pix2 onto pixd, using pixg2.
         * Let the normalized pixel value of pixg2 be f = pixval / 255,
         * and the pixel values of pixd and pix2 be p1 and p2, rsp.
         * Then the blended value is:
         *      p = (1.0 - f) * p1 + f * p2
         * Blending is done component-wise if rgb.
         * Scan over pix2 and pixg2, clipping to pixd where necessary.  */
    datad = pixGetData(pixd);
    datas = pixGetData(pix2);
    datag = pixGetData(pixg2);
    wpld = pixGetWpl(pixd);
    wpls = pixGetWpl(pix2);
    wplg = pixGetWpl(pixg2);
    for (i = 0; i < hmin; i++) {
        if (i + y < 0  || i + y >= h1) continue;
        lined = datad + (i + y) * wpld;
        lines = datas + i * wpls;
        lineg = datag + i * wplg;
        for (j = 0; j < wmin; j++) {
            if (j + x < 0  || j + x >= w1) continue;
            val = GET_DATA_BYTE(lineg, j);
            if (val == 0) continue;  /* pix2 is transparent */
            fract = (l_float32)val / 255.;
            if (d1 == 8) {
                dval = GET_DATA_BYTE(lined, j + x);
                sval = GET_DATA_BYTE(lines, j);
                dval = (l_int32)((1.0 - fract) * dval + fract * sval);
                SET_DATA_BYTE(lined, j + x, dval);
            } else {  /* 32 */
                dval32 = *(lined + j + x);
                sval32 = *(lines + j);
                extractRGBValues(dval32, &drval, &dgval, &dbval);
                extractRGBValues(sval32, &srval, &sgval, &sbval);
                drval = (l_int32)((1.0 - fract) * drval + fract * srval);
                dgval = (l_int32)((1.0 - fract) * dgval + fract * sgval);
                dbval = (l_int32)((1.0 - fract) * dbval + fract * sbval);
                composeRGBPixel(drval, dgval, dbval, &dval32);
                *(lined + j + x) = dval32;
            }
        }
    }

    pixDestroy(&pixg2);
    pixDestroy(&pix2);
    return pixd;
}


/*---------------------------------------------------------------------*
 *                Blending background to a specific color              *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixBlendBackgroundToColor()
 *
 * \param[in]    pixd    can be NULL or pixs
 * \param[in]    pixs    32 bpp rgb
 * \param[in]    box     region for blending; can be NULL)
 * \param[in]    color   32 bit color in 0xrrggbb00 format
 * \param[in]    gamma, minval, maxval    args for grayscale TRC mapping
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *      (1) This in effect replaces light background pixels in pixs
 *          by the input color.  It does it by alpha blending so that
 *          there are no visible artifacts from hard cutoffs.
 *      (2) If pixd == pixs, this is done in-place.
 *      (3) If box == NULL, this is performed on all of pixs.
 *      (4) The alpha component for blending is derived from pixs,
 *          by converting to grayscale and enhancing with a TRC.
 *      (5) The last three arguments specify the TRC operation.
 *          Suggested values are: %gamma = 0.3, %minval = 50, %maxval = 200.
 *          To skip the TRC, use %gamma == 1, %minval = 0, %maxval = 255.
 *          See pixGammaTRC() for details.
 * </pre>
 */
PIX *
pixBlendBackgroundToColor(PIX       *pixd,
                          PIX       *pixs,
                          BOX       *box,
                          l_uint32   color,
                          l_float32  gamma,
                          l_int32    minval,
                          l_int32    maxval)
{
l_int32  x, y, w, h;
BOX     *boxt;
PIX     *pixt, *pixc, *pixr, *pixg;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, pixd);
    if (pixd && (pixd != pixs))
        return (PIX *)ERROR_PTR("pixd neither null nor pixs", __func__, pixd);

        /* Extract the (optionally cropped) region, pixr, and generate
         * an identically sized pixc with the uniform color. */
    if (!pixd)
        pixd = pixCopy(NULL, pixs);
    if (box) {
        pixr = pixClipRectangle(pixd, box, &boxt);
        boxGetGeometry(boxt, &x, &y, &w, &h);
        pixc = pixCreate(w, h, 32);
        boxDestroy(&boxt);
    } else {
        pixc = pixCreateTemplate(pixs);
        pixr = pixClone(pixd);
    }
    pixSetAllArbitrary(pixc, color);

        /* Set up the alpha channel */
    pixg = pixConvertTo8(pixr, 0);
    pixGammaTRC(pixg, pixg, gamma, minval, maxval);
    pixSetRGBComponent(pixc, pixg, L_ALPHA_CHANNEL);

        /* Blend and replace in pixd */
    pixt = pixBlendWithGrayMask(pixr, pixc, NULL, 0, 0);
    if (box) {
        pixRasterop(pixd, x, y, w, h, PIX_SRC, pixt, 0, 0);
        pixDestroy(&pixt);
    } else {
        pixTransferAllData(pixd, &pixt, 0, 0);
    }

    pixDestroy(&pixc);
    pixDestroy(&pixr);
    pixDestroy(&pixg);
    return pixd;
}


/*---------------------------------------------------------------------*
 *                     Multiplying by a specific color                 *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixMultiplyByColor()
 *
 * \param[in]    pixd    can be NULL or pixs
 * \param[in]    pixs    32 bpp rgb
 * \param[in]    box     region for filtering; can be NULL)
 * \param[in]    color   32 bit color in 0xrrggbb00 format
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *      (1) This filters all pixels in the specified region by
 *          multiplying each component by the input color.
 *          This leaves black invariant and transforms white to the
 *          input color.
 *      (2) If pixd == pixs, this is done in-place.
 *      (3) If box == NULL, this is performed on all of pixs.
 * </pre>
 */
PIX *
pixMultiplyByColor(PIX       *pixd,
                   PIX       *pixs,
                   BOX       *box,
                   l_uint32   color)
{
l_int32    i, j, bx, by, w, h, wpl;
l_int32    red, green, blue, rval, gval, bval, nrval, ngval, nbval;
l_float32  frval, fgval, fbval;
l_uint32  *data, *line;
PIX       *pixt;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, pixd);
    if (pixd && (pixd != pixs))
        return (PIX *)ERROR_PTR("pixd neither null nor pixs", __func__, pixd);

    if (!pixd)
        pixd = pixCopy(NULL, pixs);
    if (box) {
        boxGetGeometry(box, &bx, &by, NULL, NULL);
        pixt = pixClipRectangle(pixd, box, NULL);
    } else {
        pixt = pixClone(pixd);
    }

        /* Multiply each pixel in pixt by the color */
    extractRGBValues(color, &red, &green, &blue);
    frval = (1. / 255.) * red;
    fgval = (1. / 255.) * green;
    fbval = (1. / 255.) * blue;
    data = pixGetData(pixt);
    wpl = pixGetWpl(pixt);
    pixGetDimensions(pixt, &w, &h, NULL);
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        for (j = 0; j < w; j++) {
            extractRGBValues(line[j], &rval, &gval, &bval);
            nrval = (l_int32)(frval * rval + 0.5);
            ngval = (l_int32)(fgval * gval + 0.5);
            nbval = (l_int32)(fbval * bval + 0.5);
            composeRGBPixel(nrval, ngval, nbval, line + j);
        }
    }

        /* Replace */
    if (box)
        pixRasterop(pixd, bx, by, w, h, PIX_SRC, pixt, 0, 0);
    pixDestroy(&pixt);
    return pixd;
}


/*---------------------------------------------------------------------*
 *       Rendering with alpha blending over a uniform background       *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixAlphaBlendUniform()
 *
 * \param[in]    pixs    32 bpp rgba, with alpha
 * \param[in]    color   32 bit color in 0xrrggbb00 format
 * \return  pixd 32 bpp rgb: pixs blended over uniform color %color,
 *                    a clone of pixs if no alpha, and null on error
 *
 * <pre>
 * Notes:
 *      (1) This is a convenience function that renders 32 bpp RGBA images
 *          (with an alpha channel) over a uniform background of
 *          value %color.  To render over a white background,
 *          use %color = 0xffffff00.  The result is an RGB image.
 *      (2) If pixs does not have an alpha channel, it returns a clone
 *          of pixs.
 * </pre>
 */
PIX *
pixAlphaBlendUniform(PIX      *pixs,
                     l_uint32  color)
{
PIX  *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (pixGetSpp(pixs) != 4) {
        L_WARNING("no alpha channel; returning clone\n", __func__);
        return pixClone(pixs);
    }

    pixt = pixCreateTemplate(pixs);
    pixSetAllArbitrary(pixt, color);
    pixSetSpp(pixt, 3);  /* not required */
    pixd = pixBlendWithGrayMask(pixt, pixs, NULL, 0, 0);

    pixDestroy(&pixt);
    return pixd;
}


/*---------------------------------------------------------------------*
 *                   Adding an alpha layer for blending                *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixAddAlphaToBlend()
 *
 * \param[in]    pixs     any depth
 * \param[in]    fract    fade fraction in the alpha component
 * \param[in]    invert   1 to photometrically invert pixs
 * \return  pixd 32 bpp with alpha, or null on error
 *
 * <pre>
 * Notes:
 *      (1) This is a simple alpha layer generator, where typically white has
 *          maximum transparency and black has minimum.
 *      (2) If %invert == 1, generate the same alpha layer but invert
 *          the input image photometrically.  This is useful for blending
 *          over dark images, where you want dark regions in pixs, such
 *          as text, to be lighter in the blended image.
 *      (3) The fade %fract gives the minimum transparency (i.e.,
 *          maximum opacity).  A small fraction is useful for adding
 *          a watermark to an image.
 *      (4) If pixs has a colormap, it is removed to rgb.
 *      (5) If pixs already has an alpha layer, it is overwritten.
 * </pre>
 */
PIX *
pixAddAlphaToBlend(PIX       *pixs,
                   l_float32  fract,
                   l_int32    invert)
{
PIX  *pixd, *pix1, *pix2;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (fract < 0.0 || fract > 1.0)
        return (PIX *)ERROR_PTR("invalid fract", __func__, NULL);

        /* Convert to 32 bpp */
    if (pixGetColormap(pixs))
        pix1 = pixRemoveColormap(pixs, REMOVE_CMAP_TO_FULL_COLOR);
    else
        pix1 = pixClone(pixs);
    pixd = pixConvertTo32(pix1);  /* new */

        /* Use an inverted image if this will be blended with a dark image */
    if (invert) pixInvert(pixd, pixd);

        /* Generate alpha layer */
    pix2 = pixConvertTo8(pix1, 0);  /* new */
    pixInvert(pix2, pix2);
    pixMultConstantGray(pix2, fract);
    pixSetRGBComponent(pixd, pix2, L_ALPHA_CHANNEL);

    pixDestroy(&pix1);
    pixDestroy(&pix2);
    return pixd;
}



/*---------------------------------------------------------------------*
 *    Setting a transparent alpha component over a white background    *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixSetAlphaOverWhite()
 *
 * \param[in]    pixs    colormapped or 32 bpp rgb; no alpha
 * \return  pixd new pix with meaningful alpha component,
 *                   or null on error
 *
 * <pre>
 * Notes:
 *      (1) The generated alpha component is transparent over white
 *          (background) pixels in pixs, and quickly grades to opaque
 *          away from the transparent parts.  This is a cheap and
 *          dirty alpha generator.  The 2 pixel gradation is useful
 *          to blur the boundary between the transparent region
 *          (that will render entirely from a backing image) and
 *          the remainder which renders from pixs.
 *      (2) All alpha component bits in pixs are overwritten.
 * </pre>
 */
PIX *
pixSetAlphaOverWhite(PIX  *pixs)
{
PIX  *pixd, *pix1, *pix2, *pix3, *pix4;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (!(pixGetDepth(pixs) == 32 || pixGetColormap(pixs)))
        return (PIX *)ERROR_PTR("pixs not 32 bpp or cmapped", __func__, NULL);

        /* Remove colormap if it exists; otherwise copy */
    pixd = pixRemoveColormapGeneral(pixs, REMOVE_CMAP_TO_FULL_COLOR, L_COPY);

        /* Generate a 1 bpp image where a white pixel in pixd is 0.
         * In the comments below, a "white" pixel refers to pixd.
         * pix1 is rgb, pix2 is 8 bpp gray, pix3 is 1 bpp. */
    pix1 = pixInvert(NULL, pixd);  /* send white (255) to 0 for each sample */
    pix2 = pixConvertRGBToGrayMinMax(pix1, L_CHOOSE_MAX);  /* 0 if white */
    pix3 = pixThresholdToBinary(pix2, 1);  /* sets white pixels to 1 */
    pixInvert(pix3, pix3);  /* sets white pixels to 0 */

        /* Generate the alpha component using the distance transform,
         * which measures the distance to the nearest bg (0) pixel in pix3.
         * After multiplying by 128, its value is 0 (transparent)
         * over white pixels, and goes to opaque (255) two pixels away
         * from the nearest white pixel. */
    pix4 = pixDistanceFunction(pix3, 8, 8, L_BOUNDARY_FG);
    pixMultConstantGray(pix4, 128.0);
    pixSetRGBComponent(pixd, pix4, L_ALPHA_CHANNEL);

    pixDestroy(&pix1);
    pixDestroy(&pix2);
    pixDestroy(&pix3);
    pixDestroy(&pix4);
    return pixd;
}


/*---------------------------------------------------------------------*
 *                          Fading from the edge                       *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixLinearEdgeFade()
 *
 * \param[in]    pixs       8 or 32 bpp; no colormap
 * \param[in]    dir        L_FROM_LEFT, L_FROM_RIGHT, L_FROM_TOP, L_FROM_BOT
 * \param[in]    fadeto     L_BLEND_TO_WHITE, L_BLEND_TO_BLACK
 * \param[in]    distfract  fraction of width or height over which fading occurs
 * \param[in]    maxfade    fraction of fading at the edge, <= 1.0
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) In-place operation.
 *      (2) Maximum fading fraction %maxfade occurs at the edge of the image,
 *          and the fraction goes to 0 at the fractional distance %distfract
 *          from the edge.  %maxfade must be in [0, 1].
 *      (3) %distrfact must be in [0, 1], and typically it would be <= 0.5.
 * </pre>
 */
l_ok
pixLinearEdgeFade(PIX       *pixs,
                  l_int32    dir,
                  l_int32    fadeto,
                  l_float32  distfract,
                  l_float32  maxfade)
{
l_int32    i, j, w, h, d, wpl, xmin, ymin, range, val, rval, gval, bval;
l_float32  slope, limit, del;
l_uint32  *data, *line;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (pixGetColormap(pixs) != NULL)
        return ERROR_INT("pixs has a colormap", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8 && d != 32)
        return ERROR_INT("pixs not 8 or 32 bpp", __func__, 1);
    if (dir != L_FROM_LEFT && dir != L_FROM_RIGHT &&
        dir != L_FROM_TOP && dir != L_FROM_BOT)
        return ERROR_INT("invalid fade direction from edge", __func__, 1);
    if (fadeto != L_BLEND_TO_WHITE && fadeto != L_BLEND_TO_BLACK)
        return ERROR_INT("invalid fadeto photometry", __func__, 1);
    if (maxfade <= 0) return 0;
    if (maxfade > 1.0)
        return ERROR_INT("invalid maxfade", __func__, 1);
    if (distfract <= 0 || distfract * L_MIN(w, h) < 1.0) {
        L_INFO("distfract is too small\n", __func__);
        return 0;
    }
    if (distfract > 1.0)
        return ERROR_INT("invalid distfract", __func__, 1);

        /* Set up parameters */
    if (dir == L_FROM_LEFT) {
        range = (l_int32)(distfract * w);
        xmin = 0;
        slope = maxfade / (l_float32)range;
    } else if (dir == L_FROM_RIGHT) {
        range = (l_int32)(distfract * w);
        xmin = w - range;
        slope = maxfade / (l_float32)range;
    } else if (dir == L_FROM_TOP) {
        range = (l_int32)(distfract * h);
        ymin = 0;
        slope = maxfade / (l_float32)range;
    } else {  /* dir == L_FROM_BOT */
        range = (l_int32)(distfract * h);
        ymin = h - range;
        slope = maxfade / (l_float32)range;
    }

    limit = (fadeto == L_BLEND_TO_WHITE) ? 255.0 : 0.0;
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    if (dir == L_FROM_LEFT || dir == L_FROM_RIGHT) {
        for (j = 0; j < range; j++) {
            del = (dir == L_FROM_LEFT) ? maxfade - slope * j
                                       : maxfade - slope * (range - j);
            for (i = 0; i < h; i++) {
                line = data + i * wpl;
                if (d == 8) {
                    val = GET_DATA_BYTE(line, xmin + j);
                    val += (limit - val) * del + 0.5;
                    SET_DATA_BYTE(line, xmin + j, val);
                } else {  /* rgb */
                    extractRGBValues(*(line + xmin + j), &rval, &gval, &bval);
                    rval += (limit - rval) * del + 0.5;
                    gval += (limit - gval) * del + 0.5;
                    bval += (limit - bval) * del + 0.5;
                    composeRGBPixel(rval, gval, bval, line + xmin + j);
                }
            }
        }
    } else {  /* dir == L_FROM_TOP || L_FROM_BOT */
        for (i = 0; i < range; i++) {
            del = (dir == L_FROM_TOP) ? maxfade - slope * i
                                      : maxfade - slope * (range - i);
            line = data + (ymin + i) * wpl;
            for (j = 0; j < w; j++) {
                if (d == 8) {
                    val = GET_DATA_BYTE(line, j);
                    val += (limit - val) * del + 0.5;
                    SET_DATA_BYTE(line, j, val);
                } else {  /* rgb */
                    extractRGBValues(*(line + j), &rval, &gval, &bval);
                    rval += (limit - rval) * del + 0.5;
                    gval += (limit - gval) * del + 0.5;
                    bval += (limit - bval) * del + 0.5;
                    composeRGBPixel(rval, gval, bval, line + j);
                }
            }
        }
    }

    return 0;
}

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
 * \file rotate.c
 * <pre>
 *
 *     General rotation about image center
 *              PIX     *pixRotate()
 *              PIX     *pixEmbedForRotation()
 *
 *     General rotation by sampling
 *              PIX     *pixRotateBySampling()
 *
 *     Nice (slow) rotation of 1 bpp image
 *              PIX     *pixRotateBinaryNice()
 *
 *     Rotation including alpha (blend) component
 *              PIX     *pixRotateWithAlpha()
 *
 *     Rotations are measured in radians; clockwise is positive.
 *
 *     The general rotation pixRotate() does the best job for
 *     rotating about the image center.  For 1 bpp, it uses shear;
 *     for others, it uses either shear or area mapping.
 *     If requested, it expands the output image so that no pixels are lost
 *     in the rotation, and this can be done on multiple successive shears
 *     without expanding beyond the maximum necessary size.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"

extern l_float32  AlphaMaskBorderVals[2];
static const l_float32  MinAngleToRotate = 0.001f;  /* radians; ~0.06 deg */
static const l_float32  Max1BppShearAngle = 0.06f;  /* radians; ~3 deg    */
static const l_float32  LimitShearAngle = 0.35f;    /* radians; ~20 deg   */

/*------------------------------------------------------------------*
 *                  General rotation about the center               *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixRotate()
 *
 * \param[in]    pixs     1, 2, 4, 8, 32 bpp rgb
 * \param[in]    angle    radians; clockwise is positive
 * \param[in]    type     L_ROTATE_AREA_MAP, L_ROTATE_SHEAR, L_ROTATE_SAMPLING
 * \param[in]    incolor  L_BRING_IN_WHITE, L_BRING_IN_BLACK
 * \param[in]    width    original width; use 0 to avoid embedding
 * \param[in]    height   original height; use 0 to avoid embedding
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a high-level, simple interface for rotating images
 *          about their center.
 *      (2) For very small rotations, just return a clone.
 *      (3) Rotation brings either white or black pixels in
 *          from outside the image.
 *      (4) The rotation type is adjusted if necessary for the image
 *          depth and size of rotation angle.  For 1 bpp images, we
 *          rotate either by shear or sampling.
 *      (5) Colormaps are removed for rotation by area mapping.
 *      (6) The dest can be expanded so that no image pixels
 *          are lost.  To invoke expansion, input the original
 *          width and height.  For repeated rotation, use of the
 *          original width and height allows the expansion to
 *          stop at the maximum required size, which is a square
 *          with side = sqrt(w*w + h*h).
 * </pre>
 */
PIX *
pixRotate(PIX       *pixs,
          l_float32  angle,
          l_int32    type,
          l_int32    incolor,
          l_int32    width,
          l_int32    height)
{
l_int32    w, h, d;
l_uint32   fillval;
PIX       *pix1, *pix2, *pix3, *pixd;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (type != L_ROTATE_SHEAR && type != L_ROTATE_AREA_MAP &&
        type != L_ROTATE_SAMPLING)
        return (PIX *)ERROR_PTR("invalid type", __func__, NULL);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return (PIX *)ERROR_PTR("invalid incolor", __func__, NULL);

    if (L_ABS(angle) < MinAngleToRotate)
        return pixClone(pixs);

        /* Adjust rotation type if necessary:
         *  - If d == 1 bpp and the angle is more than about 6 degrees,
         *    rotate by sampling; otherwise rotate by shear.
         *  - If d > 1, only allow shear rotation up to about 20 degrees;
         *    beyond that, default a shear request to sampling. */
    if (pixGetDepth(pixs) == 1) {
        if (L_ABS(angle) > Max1BppShearAngle) {
            if (type != L_ROTATE_SAMPLING)
                L_INFO("1 bpp, large angle; rotate by sampling\n", __func__);
            type = L_ROTATE_SAMPLING;
        } else if (type != L_ROTATE_SHEAR) {
            L_INFO("1 bpp; rotate by shear\n", __func__);
            type = L_ROTATE_SHEAR;
        }
    } else if (L_ABS(angle) > LimitShearAngle && type == L_ROTATE_SHEAR) {
        L_INFO("large angle; rotate by sampling\n", __func__);
        type = L_ROTATE_SAMPLING;
    }

        /* Remove colormap if we rotate by area mapping. */
    cmap = pixGetColormap(pixs);
    if (cmap && type == L_ROTATE_AREA_MAP)
        pix1 = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
    else
        pix1 = pixClone(pixs);
    cmap = pixGetColormap(pix1);

        /* Otherwise, if there is a colormap and we're not embedding,
         * add white color if it doesn't exist. */
    if (cmap && width == 0) {  /* no embedding; generate %incolor */
        if (incolor == L_BRING_IN_BLACK)
            pixcmapAddBlackOrWhite(cmap, 0, NULL);
        else  /* L_BRING_IN_WHITE */
            pixcmapAddBlackOrWhite(cmap, 1, NULL);
    }

        /* Request to embed in a larger image; do if necessary */
    pix2 = pixEmbedForRotation(pix1, angle, incolor, width, height);

        /* Area mapping requires 8 or 32 bpp.  If less than 8 bpp and
         * area map rotation is requested, convert to 8 bpp. */
    d = pixGetDepth(pix2);
    if (type == L_ROTATE_AREA_MAP && d < 8)
        pix3 = pixConvertTo8(pix2, FALSE);
    else
        pix3 = pixClone(pix2);

        /* Do the rotation: shear, sampling or area mapping */
    pixGetDimensions(pix3, &w, &h, &d);
    if (type == L_ROTATE_SHEAR) {
        pixd = pixRotateShearCenter(pix3, angle, incolor);
    } else if (type == L_ROTATE_SAMPLING) {
        pixd = pixRotateBySampling(pix3, w / 2, h / 2, angle, incolor);
    } else {  /* rotate by area mapping */
        fillval = 0;
        if (incolor == L_BRING_IN_WHITE) {
            if (d == 8)
                fillval = 255;
            else  /* d == 32 */
                fillval = 0xffffff00;
        }
        if (d == 8)
            pixd = pixRotateAMGray(pix3, angle, fillval);
        else  /* d == 32 */
            pixd = pixRotateAMColor(pix3, angle, fillval);
    }

    pixDestroy(&pix1);
    pixDestroy(&pix2);
    pixDestroy(&pix3);
    return pixd;
}


/*!
 * \brief   pixEmbedForRotation()
 *
 * \param[in]    pixs      1, 2, 4, 8, 32 bpp rgb
 * \param[in]    angle     radians; clockwise is positive
 * \param[in]    incolor   L_BRING_IN_WHITE, L_BRING_IN_BLACK
 * \param[in]    width     original width; use 0 to avoid embedding
 * \param[in]    height    original height; use 0 to avoid embedding
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For very small rotations, just return a clone.
 *      (2) Generate larger image to embed pixs if necessary, and
 *          place the center of the input image in the center.
 *      (3) Rotation brings either white or black pixels in
 *          from outside the image.  For colormapped images where
 *          there is no white or black, a new color is added if
 *          possible for these pixels; otherwise, either the
 *          lightest or darkest color is used.  In most cases,
 *          the colormap will be removed prior to rotation.
 *      (4) The dest is to be expanded so that no image pixels
 *          are lost after rotation.  Input of the original width
 *          and height allows the expansion to stop at the maximum
 *          required size, which is a square with side equal to
 *          sqrt(w*w + h*h).
 *      (5) For an arbitrary angle, the expansion can be found by
 *          considering the UL and UR corners.  As the image is
 *          rotated, these move in an arc centered at the center of
 *          the image.  Normalize to a unit circle by dividing by half
 *          the image diagonal.  After a rotation of T radians, the UL
 *          and UR corners are at points T radians along the unit
 *          circle.  Compute the x and y coordinates of both these
 *          points and take the max of absolute values; these represent
 *          the half width and half height of the containing rectangle.
 *          The arithmetic is done using formulas for sin(a+b) and cos(a+b),
 *          where b = T.  For the UR corner, sin(a) = h/d and cos(a) = w/d.
 *          For the UL corner, replace a by (pi - a), and you have
 *          sin(pi - a) = h/d, cos(pi - a) = -w/d.  The equations
 *          given below follow directly.
 * </pre>
 */
PIX *
pixEmbedForRotation(PIX       *pixs,
                    l_float32  angle,
                    l_int32    incolor,
                    l_int32    width,
                    l_int32    height)
{
l_int32    w, h, d, w1, h1, w2, h2, maxside, wnew, hnew, xoff, yoff, setcolor;
l_float64  sina, cosa, fw, fh;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return (PIX *)ERROR_PTR("invalid incolor", __func__, NULL);
    if (L_ABS(angle) < MinAngleToRotate)
        return pixClone(pixs);

        /* Test if big enough to hold any rotation of the original image */
    pixGetDimensions(pixs, &w, &h, &d);
    maxside = (l_int32)(sqrt((l_float64)(width * width) +
                             (l_float64)(height * height)) + 0.5);
    if (w >= maxside && h >= maxside)  /* big enough */
        return pixClone(pixs);

        /* Find the new sizes required to hold the image after rotation.
         * Note that the new dimensions must be at least as large as those
         * of pixs, because we're rasterop-ing into it before rotation. */
    cosa = cos(angle);
    sina = sin(angle);
    fw = (l_float64)w;
    fh = (l_float64)h;
    w1 = (l_int32)(L_ABS(fw * cosa - fh * sina) + 0.5);
    w2 = (l_int32)(L_ABS(-fw * cosa - fh * sina) + 0.5);
    h1 = (l_int32)(L_ABS(fw * sina + fh * cosa) + 0.5);
    h2 = (l_int32)(L_ABS(-fw * sina + fh * cosa) + 0.5);
    wnew = L_MAX(w, L_MAX(w1, w2));
    hnew = L_MAX(h, L_MAX(h1, h2));

    if ((pixd = pixCreate(wnew, hnew, d)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixCopyColormap(pixd, pixs);
    pixCopySpp(pixd, pixs);
    pixCopyText(pixd, pixs);
    xoff = (wnew - w) / 2;
    yoff = (hnew - h) / 2;

        /* Set background to color to be rotated in */
    setcolor = (incolor == L_BRING_IN_BLACK) ? L_SET_BLACK : L_SET_WHITE;
    pixSetBlackOrWhite(pixd, setcolor);

        /* Rasterop automatically handles all 4 channels for rgba */
    pixRasterop(pixd, xoff, yoff, w, h, PIX_SRC, pixs, 0, 0);
    return pixd;
}


/*------------------------------------------------------------------*
 *                    General rotation by sampling                  *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixRotateBySampling()
 *
 * \param[in]    pixs     1, 2, 4, 8, 16, 32 bpp rgb; can be cmapped
 * \param[in]    xcen     x value of center of rotation
 * \param[in]    ycen     y value of center of rotation
 * \param[in]    angle    radians; clockwise is positive
 * \param[in]    incolor  L_BRING_IN_WHITE, L_BRING_IN_BLACK
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For very small rotations, just return a clone.
 *      (2) Rotation brings either white or black pixels in
 *          from outside the image.
 *      (3) Colormaps are retained.
 * </pre>
 */
PIX *
pixRotateBySampling(PIX       *pixs,
                    l_int32    xcen,
                    l_int32    ycen,
                    l_float32  angle,
                    l_int32    incolor)
{
l_int32    w, h, d, i, j, x, y, xdif, ydif, wm1, hm1, wpld;
l_uint32   val;
l_float32  sina, cosa;
l_uint32  *datad, *lined;
void     **lines;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return (PIX *)ERROR_PTR("invalid incolor", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 1 && d != 2 && d != 4 && d != 8 && d != 16 && d != 32)
        return (PIX *)ERROR_PTR("invalid depth", __func__, NULL);

    if (L_ABS(angle) < MinAngleToRotate)
        return pixClone(pixs);

    if ((pixd = pixCreateTemplate(pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixSetBlackOrWhite(pixd, incolor);

    sina = sin(angle);
    cosa = cos(angle);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    wm1 = w - 1;
    hm1 = h - 1;
    lines = pixGetLinePtrs(pixs, NULL);

        /* Treat 1 bpp case specially */
    if (d == 1) {
        for (i = 0; i < h; i++) {  /* scan over pixd */
            lined = datad + i * wpld;
            ydif = ycen - i;
            for (j = 0; j < w; j++) {
                xdif = xcen - j;
                x = xcen + (l_int32)(-xdif * cosa - ydif * sina);
                if (x < 0 || x > wm1) continue;
                y = ycen + (l_int32)(-ydif * cosa + xdif * sina);
                if (y < 0 || y > hm1) continue;
                if (incolor == L_BRING_IN_WHITE) {
                    if (GET_DATA_BIT(lines[y], x))
                        SET_DATA_BIT(lined, j);
                } else {
                    if (!GET_DATA_BIT(lines[y], x))
                        CLEAR_DATA_BIT(lined, j);
                }
            }
        }
        LEPT_FREE(lines);
        return pixd;
    }

    for (i = 0; i < h; i++) {  /* scan over pixd */
        lined = datad + i * wpld;
        ydif = ycen - i;
        for (j = 0; j < w; j++) {
            xdif = xcen - j;
            x = xcen + (l_int32)(-xdif * cosa - ydif * sina);
            if (x < 0 || x > wm1) continue;
            y = ycen + (l_int32)(-ydif * cosa + xdif * sina);
            if (y < 0 || y > hm1) continue;
            switch (d)
            {
            case 8:
                val = GET_DATA_BYTE(lines[y], x);
                SET_DATA_BYTE(lined, j, val);
                break;
            case 32:
                val = GET_DATA_FOUR_BYTES(lines[y], x);
                SET_DATA_FOUR_BYTES(lined, j, val);
                break;
            case 2:
                val = GET_DATA_DIBIT(lines[y], x);
                SET_DATA_DIBIT(lined, j, val);
                break;
            case 4:
                val = GET_DATA_QBIT(lines[y], x);
                SET_DATA_QBIT(lined, j, val);
                break;
            case 16:
                val = GET_DATA_TWO_BYTES(lines[y], x);
                SET_DATA_TWO_BYTES(lined, j, val);
                break;
            default:
                return (PIX *)ERROR_PTR("invalid depth", __func__, NULL);
            }
        }
    }

    LEPT_FREE(lines);
    return pixd;
}


/*------------------------------------------------------------------*
 *                 Nice (slow) rotation of 1 bpp image              *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixRotateBinaryNice()
 *
 * \param[in]    pixs     1 bpp
 * \param[in]    angle    radians; clockwise is positive; about the center
 * \param[in]    incolor  L_BRING_IN_WHITE, L_BRING_IN_BLACK
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For very small rotations, just return a clone.
 *      (2) This does a computationally expensive rotation of 1 bpp images.
 *          The fastest rotators (using shears or subsampling) leave
 *          visible horizontal and vertical shear lines across which
 *          the image shear changes by one pixel.  To ameliorate the
 *          visual effect one can introduce random dithering.  One
 *          way to do this in a not-too-random fashion is given here.
 *          We convert to 8 bpp, do a very small blur, rotate using
 *          linear interpolation (same as area mapping), do a
 *          small amount of sharpening to compensate for the initial
 *          blur, and threshold back to binary.  The shear lines
 *          are magically removed.
 *      (3) This operation is about 5x slower than rotation by sampling.
 * </pre>
 */
PIX *
pixRotateBinaryNice(PIX       *pixs,
                    l_float32  angle,
                    l_int32    incolor)
{
PIX  *pix1, *pix2, *pix3, *pix4, *pixd;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return (PIX *)ERROR_PTR("invalid incolor", __func__, NULL);

    pix1 = pixConvertTo8(pixs, 0);
    pix2 = pixBlockconv(pix1, 1, 1);  /* smallest blur allowed */
    pix3 = pixRotateAM(pix2, angle, incolor);
    pix4 = pixUnsharpMasking(pix3, 1, 1.0);  /* sharpen a bit */
    pixd = pixThresholdToBinary(pix4, 128);
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    pixDestroy(&pix3);
    pixDestroy(&pix4);
    return pixd;
}


/*------------------------------------------------------------------*
 *             Rotation including alpha (blend) component           *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixRotateWithAlpha()
 *
 * \param[in]    pixs     32 bpp rgb or cmapped
 * \param[in]    angle    radians; clockwise is positive
 * \param[in]    pixg     [optional] 8 bpp, can be null
 * \param[in]    fract    between 0.0 and 1.0, with 0.0 fully transparent
 *                        and 1.0 fully opaque
 * \return  pixd 32 bpp rgba, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The alpha channel is transformed separately from pixs,
 *          and aligns with it, being fully transparent outside the
 *          boundary of the transformed pixs.  For pixels that are fully
 *          transparent, a blending function like pixBlendWithGrayMask()
 *          will give zero weight to corresponding pixels in pixs.
 *      (2) Rotation is about the center of the image; for very small
 *          rotations, just return a clone.  The dest is automatically
 *          expanded so that no image pixels are lost.
 *      (3) Rotation is by area mapping.  It doesn't matter what
 *          color is brought in because the alpha channel will
 *          be transparent (black) there.
 *      (4) If pixg is NULL, it is generated as an alpha layer that is
 *          partially opaque, using %fract.  Otherwise, it is cropped
 *          to pixs if required and %fract is ignored.  The alpha
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
 *          before rotation and restore it afterwards.  This is done
 *          by sandwiching this function between a gamma/inverse-gamma
 *          photometric transform:
 *              pixt = pixGammaTRCWithAlpha(NULL, pixs, 1.0 / gamma, 0, 255);
 *              pixd = pixRotateWithAlpha(pixt, angle, NULL, fract);
 *              pixGammaTRCWithAlpha(pixd, pixd, gamma, 0, 255);
 *              pixDestroy(&pixt);
 *          This has the side-effect of producing artifacts in the very
 *          dark regions.
 * </pre>
 */
PIX *
pixRotateWithAlpha(PIX       *pixs,
                   l_float32  angle,
                   PIX       *pixg,
                   l_float32  fract)
{
l_int32  ws, hs, d, spp;
PIX     *pixd, *pix32, *pixg2, *pixgr;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &ws, &hs, &d);
    if (d != 32 && pixGetColormap(pixs) == NULL)
        return (PIX *)ERROR_PTR("pixs not cmapped or 32 bpp", __func__, NULL);
    if (pixg && pixGetDepth(pixg) != 8) {
        L_WARNING("pixg not 8 bpp; using 'fract' transparent alpha\n",
                  __func__);
        pixg = NULL;
    }
    if (!pixg && (fract < 0.0 || fract > 1.0)) {
        L_WARNING("invalid fract; using fully opaque\n", __func__);
        fract = 1.0;
    }
    if (!pixg && fract == 0.0)
        L_WARNING("transparent alpha; image will not be blended\n", __func__);

        /* Make sure input to rotation is 32 bpp rgb, and rotate it */
    if (d != 32)
        pix32 = pixConvertTo32(pixs);
    else
        pix32 = pixClone(pixs);
    spp = pixGetSpp(pix32);
    pixSetSpp(pix32, 3);  /* ignore the alpha channel for the rotation */
    pixd = pixRotate(pix32, angle, L_ROTATE_AREA_MAP, L_BRING_IN_WHITE, ws, hs);
    pixSetSpp(pix32, spp);  /* restore initial value in case it's a clone */
    pixDestroy(&pix32);

        /* Set up alpha layer with a fading border and rotate it */
    if (!pixg) {
        pixg2 = pixCreate(ws, hs, 8);
        if (fract == 1.0)
            pixSetAll(pixg2);
        else if (fract > 0.0)
            pixSetAllArbitrary(pixg2, (l_int32)(255.0 * fract));
    } else {
        pixg2 = pixResizeToMatch(pixg, NULL, ws, hs);
    }
    if (ws > 10 && hs > 10) {  /* see note 8 */
        pixSetBorderRingVal(pixg2, 1,
                            (l_int32)(255.0 * fract * AlphaMaskBorderVals[0]));
        pixSetBorderRingVal(pixg2, 2,
                            (l_int32)(255.0 * fract * AlphaMaskBorderVals[1]));
    }
    pixgr = pixRotate(pixg2, angle, L_ROTATE_AREA_MAP,
                      L_BRING_IN_BLACK, ws, hs);

        /* Combine into a 4 spp result */
    pixSetRGBComponent(pixd, pixgr, L_ALPHA_CHANNEL);

    pixDestroy(&pixg2);
    pixDestroy(&pixgr);
    return pixd;
}

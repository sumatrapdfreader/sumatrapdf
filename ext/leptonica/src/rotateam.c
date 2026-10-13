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
 * \file rotateam.c
 * <pre>
 *
 *     Grayscale and color rotation for area mapping (== interpolation)
 *
 *         Rotation about the image center
 *                PIX         *pixRotateAM()
 *                PIX         *pixRotateAMColor()
 *                PIX         *pixRotateAMGray()
 *                static void  rotateAMColorLow()
 *                static void  rotateAMGrayLow()
 *
 *         Rotation about the UL corner of the image
 *                PIX         *pixRotateAMCorner()
 *                PIX         *pixRotateAMColorCorner()
 *                PIX         *pixRotateAMGrayCorner()
 *                static void  rotateAMColorCornerLow()
 *                static void  rotateAMGrayCornerLow()
 *
 *         Faster color rotation about the image center
 *                PIX         *pixRotateAMColorFast()
 *                static void  rotateAMColorFastLow()
 *
 *     Rotations are measured in radians; clockwise is positive.
 *
 *     The basic area mapping grayscale rotation works on 8 bpp images.
 *     For color, the same method is applied to each color separately.
 *     This can be done in two ways: (1) as here, computing each dest
 *     rgb pixel from the appropriate four src rgb pixels, or (2) separating
 *     the color image into three 8 bpp images, rotate each of these,
 *     and then combine the result.  Method (1) is about 2.5x faster.
 *     We have also implemented a fast approximation for color area-mapping
 *     rotation (pixRotateAMColorFast()), which is about 25% faster
 *     than the standard color rotator.  If you need the extra speed,
 *     use it.
 *
 *     Area mapping works as follows.  For each dest
 *     pixel you find the 4 source pixels that it partially
 *     covers.  You then compute the dest pixel value as
 *     the area-weighted average of those 4 source pixels.
 *     We make two simplifying approximations:
 *
 *       ~  For simplicity, compute the areas as if the dest
 *          pixel were translated but not rotated.
 *
 *       ~  Compute area overlaps on a discrete sub-pixel grid.
 *          Because we are using 8 bpp images with 256 levels,
 *          it is convenient to break each pixel into a
 *          16x16 sub-pixel grid, and count the number of
 *          overlapped sub-pixels.
 *
 *     It is interesting to note that the digital filter that
 *     implements the area mapping algorithm for rotation
 *     is identical to the digital filter used for linear
 *     interpolation when arbitrarily scaling grayscale images.
 *
 *     The advantage of area mapping over pixel sampling
 *     in grayscale rotation is that the former naturally
 *     blurs sharp edges ("anti-aliasing"), so that stair-step
 *     artifacts are not introduced.  The disadvantage is that
 *     it is significantly slower.
 *
 *     But it is still pretty fast.  With standard 3 GHz hardware,
 *     the anti-aliased (area-mapped) color rotation speed is
 *     about 15 million pixels/sec.
 *
 *     The function pixRotateAMColorFast() is about 10-20% faster
 *     than pixRotateAMColor().  The quality is slightly worse,
 *     and if you make many successive small rotations, with a
 *     total angle of 360 degrees, it has been noted that the
 *     center wanders -- it seems to be doing a 1 pixel translation
 *     in addition to the rotation.
 *
 *     Consider again the comparison of image quality between sampling
 *     and area mapping.  With sampling, sharp edges such as found in
 *     text images remain sharp.  However, sampling artifacts such as
 *     characters randomly bouncing up and down by one pixel, or
 *     one pixel horizontal shear lines going through a line of text
 *     (causing the characters to look like badly rendered italic),
 *     are highly visible.  It does not help to sample the source pixel
 *     with the largest area covering each dest pixel; the result has
 *     the same ugly sampling artifacts.
 *
 *     With area mapping, these annoying artifacts are avoided, but the
 *     blurring of edges makes small text a bit more difficult to read.
 *     However, if you are willing to do more computation, you can have
 *     the best of both worlds: no sampling artifacts and sharp edges.
 *     Use area mapping to avoid sampling issues, and follow it with
 *     unsharp masking.  Experiment with the sharpening parameters.
 *     I have found that a small amount of sharpening is sufficient to
 *     restore the sharp edges in text; e.g.,
 *         pix2 = pixUnsharpMasking(pix1, 1, 0.3);
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>   /* required for sin and tan */
#include "allheaders.h"

static void rotateAMColorLow(l_uint32 *datad, l_int32 w, l_int32 h,
                             l_int32 wpld, l_uint32 *datas, l_int32 wpls,
                             l_float32 angle, l_uint32 colorval);
static void rotateAMGrayLow(l_uint32 *datad, l_int32 w, l_int32 h,
                            l_int32 wpld, l_uint32 *datas, l_int32 wpls,
                            l_float32 angle, l_uint8 grayval);
static void rotateAMColorCornerLow(l_uint32 *datad, l_int32 w, l_int32 h,
                                   l_int32 wpld, l_uint32 *datas,
                                   l_int32 wpls, l_float32 angle,
                                   l_uint32 colorval);
static void rotateAMGrayCornerLow(l_uint32 *datad, l_int32 w, l_int32 h,
                                  l_int32 wpld, l_uint32 *datas, l_int32 wpls,
                                  l_float32 angle, l_uint8 grayval);

static void rotateAMColorFastLow(l_uint32 *datad, l_int32 w, l_int32 h,
                                 l_int32 wpld, l_uint32 *datas, l_int32 wpls,
                                 l_float32 angle, l_uint32 colorval);

static const l_float32  MinAngleToRotate = 0.001f;  /* radians; ~0.06 deg */


/*------------------------------------------------------------------*
 *                     Rotation about the center                    *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixRotateAM()
 *
 * \param[in]    pixs 2, 4, 8 bpp gray or colormapped, or 32 bpp RGB
 * \param[in]    angle radians; clockwise is positive
 * \param[in]    incolor L_BRING_IN_WHITE, L_BRING_IN_BLACK
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Rotates about image center.
 *      (2) A positive angle gives a clockwise rotation.
 *      (3) Brings in either black or white pixels from the boundary.
 * </pre>
 */
PIX *
pixRotateAM(PIX       *pixs,
            l_float32  angle,
            l_int32    incolor)
{
l_int32   d;
l_uint32  fillval;
PIX      *pixt1, *pixt2, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) == 1)
        return (PIX *)ERROR_PTR("pixs is 1 bpp", __func__, NULL);

    if (L_ABS(angle) < MinAngleToRotate)
        return pixClone(pixs);

        /* Remove cmap if it exists, and unpack to 8 bpp if necessary */
    pixt1 = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
    d = pixGetDepth(pixt1);
    if (d < 8)
        pixt2 = pixConvertTo8(pixt1, FALSE);
    else
        pixt2 = pixClone(pixt1);
    d = pixGetDepth(pixt2);

        /* Compute actual incoming color */
    fillval = 0;
    if (incolor == L_BRING_IN_WHITE) {
        if (d == 8)
            fillval = 255;
        else  /* d == 32 */
            fillval = 0xffffff00;
    }

    if (d == 8)
        pixd = pixRotateAMGray(pixt2, angle, fillval);
    else   /* d == 32 */
        pixd = pixRotateAMColor(pixt2, angle, fillval);

    pixDestroy(&pixt1);
    pixDestroy(&pixt2);
    return pixd;
}


/*!
 * \brief   pixRotateAMColor()
 *
 * \param[in]    pixs 32 bpp
 * \param[in]    angle radians; clockwise is positive
 * \param[in]    colorval e.g., 0 to bring in BLACK, 0xffffff00 for WHITE
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Rotates about image center.
 *      (2) A positive angle gives a clockwise rotation.
 *      (3) Specify the color to be brought in from outside the image.
 * </pre>
 */
PIX *
pixRotateAMColor(PIX       *pixs,
                 l_float32  angle,
                 l_uint32   colorval)
{
l_int32    w, h, wpls, wpld;
l_uint32  *datas, *datad;
PIX       *pix1, *pix2, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs must be 32 bpp", __func__, NULL);

    if (L_ABS(angle) < MinAngleToRotate)
        return pixClone(pixs);

    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    pixd = pixCreateTemplate(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    rotateAMColorLow(datad, w, h, wpld, datas, wpls, angle, colorval);
    if (pixGetSpp(pixs) == 4) {
        pix1 = pixGetRGBComponent(pixs, L_ALPHA_CHANNEL);
        pix2 = pixRotateAMGray(pix1, angle, 255);  /* bring in opaque */
        pixSetRGBComponent(pixd, pix2, L_ALPHA_CHANNEL);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }

    return pixd;
}


/*!
 * \brief   pixRotateAMGray()
 *
 * \param[in]    pixs 8 bpp
 * \param[in]    angle radians; clockwise is positive
 * \param[in]    grayval 0 to bring in BLACK, 255 for WHITE
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Rotates about image center.
 *      (2) A positive angle gives a clockwise rotation.
 *      (3) Specify the grayvalue to be brought in from outside the image.
 * </pre>
 */
PIX *
pixRotateAMGray(PIX       *pixs,
                l_float32  angle,
                l_uint8    grayval)
{
l_int32    w, h, wpls, wpld;
l_uint32  *datas, *datad;
PIX        *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs must be 8 bpp", __func__, NULL);

    if (L_ABS(angle) < MinAngleToRotate)
        return pixClone(pixs);

    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    pixd = pixCreateTemplate(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    rotateAMGrayLow(datad, w, h, wpld, datas, wpls, angle, grayval);

    return pixd;
}


static void
rotateAMColorLow(l_uint32  *datad,
                 l_int32    w,
                 l_int32    h,
                 l_int32    wpld,
                 l_uint32  *datas,
                 l_int32    wpls,
                 l_float32  angle,
                 l_uint32   colorval)
{
l_int32    i, j, xcen, ycen, wm2, hm2;
l_int32    xdif, ydif, xpm, ypm, xp, yp, xf, yf;
l_int32    rval, gval, bval;
l_uint32   word00, word01, word10, word11;
l_uint32  *lines, *lined;
l_float32  sina, cosa;

    xcen = w / 2;
    wm2 = w - 2;
    ycen = h / 2;
    hm2 = h - 2;
    sina = 16.f * sin(angle);
    cosa = 16.f * cos(angle);

    for (i = 0; i < h; i++) {
        ydif = ycen - i;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            xdif = xcen - j;
            xpm = (l_int32)(-xdif * cosa - ydif * sina);
            ypm = (l_int32)(-ydif * cosa + xdif * sina);
            xp = xcen + (xpm >> 4);
            yp = ycen + (ypm >> 4);
            xf = xpm & 0x0f;
            yf = ypm & 0x0f;

                /* if off the edge, write input colorval */
            if (xp < 0 || yp < 0 || xp > wm2 || yp > hm2) {
                *(lined + j) = colorval;
                continue;
            }

            lines = datas + yp * wpls;

                /* do area weighting.  Without this, we would
                 * simply do:
                 *   *(lined + j) = *(lines + xp);
                 * which is faster but gives lousy results!
                 */
            word00 = *(lines + xp);
            word10 = *(lines + xp + 1);
            word01 = *(lines + wpls + xp);
            word11 = *(lines + wpls + xp + 1);
            rval = ((16 - xf) * (16 - yf) * ((word00 >> L_RED_SHIFT) & 0xff) +
                    xf * (16 - yf) * ((word10 >> L_RED_SHIFT) & 0xff) +
                    (16 - xf) * yf * ((word01 >> L_RED_SHIFT) & 0xff) +
                    xf * yf * ((word11 >> L_RED_SHIFT) & 0xff) + 128) / 256;
            gval = ((16 - xf) * (16 - yf) * ((word00 >> L_GREEN_SHIFT) & 0xff) +
                    xf * (16 - yf) * ((word10 >> L_GREEN_SHIFT) & 0xff) +
                    (16 - xf) * yf * ((word01 >> L_GREEN_SHIFT) & 0xff) +
                    xf * yf * ((word11 >> L_GREEN_SHIFT) & 0xff) + 128) / 256;
            bval = ((16 - xf) * (16 - yf) * ((word00 >> L_BLUE_SHIFT) & 0xff) +
                    xf * (16 - yf) * ((word10 >> L_BLUE_SHIFT) & 0xff) +
                    (16 - xf) * yf * ((word01 >> L_BLUE_SHIFT) & 0xff) +
                    xf * yf * ((word11 >> L_BLUE_SHIFT) & 0xff) + 128) / 256;
            composeRGBPixel(rval, gval, bval, lined + j);
        }
    }
}


static void
rotateAMGrayLow(l_uint32  *datad,
                l_int32    w,
                l_int32    h,
                l_int32    wpld,
                l_uint32  *datas,
                l_int32    wpls,
                l_float32  angle,
                l_uint8    grayval)
{
l_int32    i, j, xcen, ycen, wm2, hm2;
l_int32    xdif, ydif, xpm, ypm, xp, yp, xf, yf;
l_int32    v00, v01, v10, v11;
l_uint8    val;
l_uint32  *lines, *lined;
l_float32  sina, cosa;

    xcen = w / 2;
    wm2 = w - 2;
    ycen = h / 2;
    hm2 = h - 2;
    sina = 16.f * sin(angle);
    cosa = 16.f * cos(angle);

    for (i = 0; i < h; i++) {
        ydif = ycen - i;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            xdif = xcen - j;
            xpm = (l_int32)(-xdif * cosa - ydif * sina);
            ypm = (l_int32)(-ydif * cosa + xdif * sina);
            xp = xcen + (xpm >> 4);
            yp = ycen + (ypm >> 4);
            xf = xpm & 0x0f;
            yf = ypm & 0x0f;

                /* if off the edge, write input grayval */
            if (xp < 0 || yp < 0 || xp > wm2 || yp > hm2) {
                SET_DATA_BYTE(lined, j, grayval);
                continue;
            }

            lines = datas + yp * wpls;

                /* do area weighting.  Without this, we would
                 * simply do:
                 *   SET_DATA_BYTE(lined, j, GET_DATA_BYTE(lines, xp));
                 * which is faster but gives lousy results!
                 */
            v00 = (16 - xf) * (16 - yf) * GET_DATA_BYTE(lines, xp);
            v10 = xf * (16 - yf) * GET_DATA_BYTE(lines, xp + 1);
            v01 = (16 - xf) * yf * GET_DATA_BYTE(lines + wpls, xp);
            v11 = xf * yf * GET_DATA_BYTE(lines + wpls, xp + 1);
            val = (l_uint8)((v00 + v01 + v10 + v11 + 128) / 256);
            SET_DATA_BYTE(lined, j, val);
        }
    }
}


/*------------------------------------------------------------------*
 *                    Rotation about the UL corner                  *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixRotateAMCorner()
 *
 * \param[in]    pixs 1, 2, 4, 8 bpp gray or colormapped, or 32 bpp RGB
 * \param[in]    angle radians; clockwise is positive
 * \param[in]    incolor L_BRING_IN_WHITE, L_BRING_IN_BLACK
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Rotates about the UL corner of the image.
 *      (2) A positive angle gives a clockwise rotation.
 *      (3) Brings in either black or white pixels from the boundary.
 * </pre>
 */
PIX *
pixRotateAMCorner(PIX       *pixs,
                  l_float32  angle,
                  l_int32    incolor)
{
l_int32   d;
l_uint32  fillval;
PIX      *pixt1, *pixt2, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

    if (L_ABS(angle) < MinAngleToRotate)
        return pixClone(pixs);

        /* Remove cmap if it exists, and unpack to 8 bpp if necessary */
    pixt1 = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
    d = pixGetDepth(pixt1);
    if (d < 8)
        pixt2 = pixConvertTo8(pixt1, FALSE);
    else
        pixt2 = pixClone(pixt1);
    d = pixGetDepth(pixt2);

        /* Compute actual incoming color */
    fillval = 0;
    if (incolor == L_BRING_IN_WHITE) {
        if (d == 8)
            fillval = 255;
        else  /* d == 32 */
            fillval = 0xffffff00;
    }

    if (d == 8)
        pixd = pixRotateAMGrayCorner(pixt2, angle, fillval);
    else   /* d == 32 */
        pixd = pixRotateAMColorCorner(pixt2, angle, fillval);

    pixDestroy(&pixt1);
    pixDestroy(&pixt2);
    return pixd;
}


/*!
 * \brief   pixRotateAMColorCorner()
 *
 * \param[in]    pixs
 * \param[in]    angle radians; clockwise is positive
 * \param[in]    fillval e.g., 0 to bring in BLACK, 0xffffff00 for WHITE
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Rotates the image about the UL corner.
 *      (2) A positive angle gives a clockwise rotation.
 *      (3) Specify the color to be brought in from outside the image.
 * </pre>
 */
PIX *
pixRotateAMColorCorner(PIX       *pixs,
                       l_float32  angle,
                       l_uint32   fillval)
{
l_int32    w, h, wpls, wpld;
l_uint32  *datas, *datad;
PIX       *pix1, *pix2, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs must be 32 bpp", __func__, NULL);

    if (L_ABS(angle) < MinAngleToRotate)
        return pixClone(pixs);

    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    pixd = pixCreateTemplate(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    rotateAMColorCornerLow(datad, w, h, wpld, datas, wpls, angle, fillval);
    if (pixGetSpp(pixs) == 4) {
        pix1 = pixGetRGBComponent(pixs, L_ALPHA_CHANNEL);
        pix2 = pixRotateAMGrayCorner(pix1, angle, 255);  /* bring in opaque */
        pixSetRGBComponent(pixd, pix2, L_ALPHA_CHANNEL);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }

    return pixd;
}


/*!
 * \brief   pixRotateAMGrayCorner()
 *
 * \param[in]    pixs
 * \param[in]    angle radians; clockwise is positive
 * \param[in]    grayval 0 to bring in BLACK, 255 for WHITE
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Rotates the image about the UL corner.
 *      (2) A positive angle gives a clockwise rotation.
 *      (3) Specify the grayvalue to be brought in from outside the image.
 * </pre>
 */
PIX *
pixRotateAMGrayCorner(PIX       *pixs,
                      l_float32  angle,
                      l_uint8    grayval)
{
l_int32    w, h, wpls, wpld;
l_uint32  *datas, *datad;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs must be 8 bpp", __func__, NULL);

    if (L_ABS(angle) < MinAngleToRotate)
        return pixClone(pixs);

    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    pixd = pixCreateTemplate(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    rotateAMGrayCornerLow(datad, w, h, wpld, datas, wpls, angle, grayval);

    return pixd;
}


static void
rotateAMColorCornerLow(l_uint32  *datad,
                       l_int32    w,
                       l_int32    h,
                       l_int32    wpld,
                       l_uint32  *datas,
                       l_int32    wpls,
                       l_float32  angle,
                       l_uint32   colorval)
{
l_int32    i, j, wm2, hm2;
l_int32    xpm, ypm, xp, yp, xf, yf;
l_int32    rval, gval, bval;
l_uint32   word00, word01, word10, word11;
l_uint32  *lines, *lined;
l_float32  sina, cosa;

    wm2 = w - 2;
    hm2 = h - 2;
    sina = 16.f * sin(angle);
    cosa = 16.f * cos(angle);

    for (i = 0; i < h; i++) {
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            xpm = (l_int32)(j * cosa + i * sina);
            ypm = (l_int32)(i * cosa - j * sina);
            xp = xpm >> 4;
            yp = ypm >> 4;
            xf = xpm & 0x0f;
            yf = ypm & 0x0f;

                /* if off the edge, write input colorval */
            if (xp < 0 || yp < 0 || xp > wm2 || yp > hm2) {
                *(lined + j) = colorval;
                continue;
            }

            lines = datas + yp * wpls;

                /* do area weighting.  Without this, we would
                 * simply do:
                 *   *(lined + j) = *(lines + xp);
                 * which is faster but gives lousy results!
                 */
            word00 = *(lines + xp);
            word10 = *(lines + xp + 1);
            word01 = *(lines + wpls + xp);
            word11 = *(lines + wpls + xp + 1);
            rval = ((16 - xf) * (16 - yf) * ((word00 >> L_RED_SHIFT) & 0xff) +
                    xf * (16 - yf) * ((word10 >> L_RED_SHIFT) & 0xff) +
                    (16 - xf) * yf * ((word01 >> L_RED_SHIFT) & 0xff) +
                    xf * yf * ((word11 >> L_RED_SHIFT) & 0xff) + 128) / 256;
            gval = ((16 - xf) * (16 - yf) * ((word00 >> L_GREEN_SHIFT) & 0xff) +
                    xf * (16 - yf) * ((word10 >> L_GREEN_SHIFT) & 0xff) +
                    (16 - xf) * yf * ((word01 >> L_GREEN_SHIFT) & 0xff) +
                    xf * yf * ((word11 >> L_GREEN_SHIFT) & 0xff) + 128) / 256;
            bval = ((16 - xf) * (16 - yf) * ((word00 >> L_BLUE_SHIFT) & 0xff) +
                    xf * (16 - yf) * ((word10 >> L_BLUE_SHIFT) & 0xff) +
                    (16 - xf) * yf * ((word01 >> L_BLUE_SHIFT) & 0xff) +
                    xf * yf * ((word11 >> L_BLUE_SHIFT) & 0xff) + 128) / 256;
            composeRGBPixel(rval, gval, bval, lined + j);
        }
    }
}


static void
rotateAMGrayCornerLow(l_uint32  *datad,
                      l_int32    w,
                      l_int32    h,
                      l_int32    wpld,
                      l_uint32  *datas,
                      l_int32    wpls,
                      l_float32  angle,
                      l_uint8    grayval)
{
l_int32    i, j, wm2, hm2;
l_int32    xpm, ypm, xp, yp, xf, yf;
l_int32    v00, v01, v10, v11;
l_uint8    val;
l_uint32  *lines, *lined;
l_float32  sina, cosa;

    wm2 = w - 2;
    hm2 = h - 2;
    sina = 16.f * sin(angle);
    cosa = 16.f * cos(angle);

    for (i = 0; i < h; i++) {
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            xpm = (l_int32)(j * cosa + i * sina);
            ypm = (l_int32)(i * cosa - j * sina);
            xp = xpm >> 4;
            yp = ypm >> 4;
            xf = xpm & 0x0f;
            yf = ypm & 0x0f;

                /* if off the edge, write input grayval */
            if (xp < 0 || yp < 0 || xp > wm2 || yp > hm2) {
                SET_DATA_BYTE(lined, j, grayval);
                continue;
            }

            lines = datas + yp * wpls;

                /* do area weighting.  Without this, we would
                 * simply do:
                 *   SET_DATA_BYTE(lined, j, GET_DATA_BYTE(lines, xp));
                 * which is faster but gives lousy results!
                 */
            v00 = (16 - xf) * (16 - yf) * GET_DATA_BYTE(lines, xp);
            v10 = xf * (16 - yf) * GET_DATA_BYTE(lines, xp + 1);
            v01 = (16 - xf) * yf * GET_DATA_BYTE(lines + wpls, xp);
            v11 = xf * yf * GET_DATA_BYTE(lines + wpls, xp + 1);
            val = (l_uint8)((v00 + v01 + v10 + v11 + 128) / 256);
            SET_DATA_BYTE(lined, j, val);
        }
    }
}


/*------------------------------------------------------------------*
 *               Fast RGB color rotation about center               *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixRotateAMColorFast()
 *
 * \param[in]    pixs
 * \param[in]    angle radians; clockwise is positive
 * \param[in]    colorval e.g., 0 to bring in BLACK, 0xffffff00 for WHITE
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This rotates a color image about the image center.
 *      (2) A positive angle gives a clockwise rotation.
 *      (3) It uses area mapping, dividing each pixel into
 *          16 subpixels.
 *      (4) It is about 10% to 20% faster than the more accurate linear
 *          interpolation function pixRotateAMColor(),
 *          which uses 256 subpixels.
 *      (5) For some reason it shifts the image center.
 *          No attempt is made to rotate the alpha component.
 * </pre>
 */
PIX *
pixRotateAMColorFast(PIX       *pixs,
                     l_float32  angle,
                     l_uint32   colorval)
{
l_int32    w, h, wpls, wpld;
l_uint32  *datas, *datad;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs must be 32 bpp", __func__, NULL);

    if (L_ABS(angle) < MinAngleToRotate)
        return pixClone(pixs);

    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    pixd = pixCreateTemplate(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    rotateAMColorFastLow(datad, w, h, wpld, datas, wpls, angle, colorval);
    return pixd;
}


/*!
 * \brief   rotateAMColorFastLow()
 *
 *     This is a special simplification of area mapping with division
 *     of each pixel into 16 sub-pixels.  The exact coefficients that
 *     should be used are the same as for the 4x linear interpolation
 *     scaling case, and are given there.  I tried to approximate these
 *     as weighted coefficients with a maximum sum of 4, which
 *     allows us to do the arithmetic in parallel for the R, G and B
 *     components in a 32 bit pixel.  However, there are three reasons
 *     for not doing that:
 *        (1) the loss of accuracy in the parallel implementation
 *            is visually significant
 *        (2) the parallel implementation (described below) is slower
 *        (3) the parallel implementation requires allocation of
 *            a temporary color image
 *
 *     There are 16 cases for the choice of the subpixel, and
 *     for each, the mapping to the relevant source
 *     pixels is as follows:
 *
 *      subpixel      src pixel weights
 *      --------      -----------------
 *         0          sp1
 *         1          (3 * sp1 + sp2) / 4
 *         2          (sp1 + sp2) / 2
 *         3          (sp1 + 3 * sp2) / 4
 *         4          (3 * sp1 + sp3) / 4
 *         5          (9 * sp1 + 3 * sp2 + 3 * sp3 + sp4) / 16
 *         6          (3 * sp1 + 3 * sp2 + sp3 + sp4) / 8
 *         7          (3 * sp1 + 9 * sp2 + sp3 + 3 * sp4) / 16
 *         8          (sp1 + sp3) / 2
 *         9          (3 * sp1 + sp2 + 3 * sp3 + sp4) / 8
 *         10         (sp1 + sp2 + sp3 + sp4) / 4
 *         11         (sp1 + 3 * sp2 + sp3 + 3 * sp4) / 8
 *         12         (sp1 + 3 * sp3) / 4
 *         13         (3 * sp1 + sp2 + 9 * sp3 + 3 * sp4) / 16
 *         14         (sp1 + sp2 + 3 * sp3 + 3 * sp4) / 8
 *         15         (sp1 + 3 * sp2 + 3 * sp3 + 9 * sp4) / 16
 *
 *     Another way to visualize this is to consider the area mapping
 *     (or linear interpolation) coefficients  for the pixel sp1.
 *     Expressed in fourths, they can be written as asymmetric matrix:
 *
 *           4      3      2      1
 *           3      2.25   1.5    0.75
 *           2      1.5    1      0.5
 *           1      0.75   0.5    0.25
 *
 *     The coefficients for the three neighboring pixels can be
 *     similarly written.
 *
 *     This is implemented here, where, for each color component,
 *     we inline its extraction from each participating word,
 *     construct the linear combination, and combine the results
 *     into the destination 32 bit RGB pixel, using the appropriate shifts.
 *
 *     It is interesting to note that an alternative method, where
 *     we do the arithmetic on the 32 bit pixels directly (after
 *     shifting the components so they won't overflow into each other)
 *     is significantly inferior.  Because we have only 8 bits for
 *     internal overflows, which can be distributed as 2, 3, 3, it
 *     is impossible to add these with the correct linear
 *     interpolation coefficients, which require a sum of up to 16.
 *     Rounding off to a sum of 4 causes appreciable visual artifacts
 *     in the rotated image.  The code for the inferior method
 *     can be found in prog/rotatefastalt.c, for reference.
 */
static void
rotateAMColorFastLow(l_uint32  *datad,
                     l_int32    w,
                     l_int32    h,
                     l_int32    wpld,
                     l_uint32  *datas,
                     l_int32    wpls,
                     l_float32  angle,
                     l_uint32   colorval)
{
l_int32    i, j, xcen, ycen, wm2, hm2;
l_int32    xdif, ydif, xpm, ypm, xp, yp, xf, yf;
l_uint32   word1, word2, word3, word4, red, blue, green;
l_uint32  *pword, *lines, *lined;
l_float32  sina, cosa;

    xcen = w / 2;
    wm2 = w - 2;
    ycen = h / 2;
    hm2 = h - 2;
    sina = 4.f * sin(angle);
    cosa = 4.f * cos(angle);

    for (i = 0; i < h; i++) {
        ydif = ycen - i;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            xdif = xcen - j;
            xpm = (l_int32)(-xdif * cosa - ydif * sina);
            ypm = (l_int32)(-ydif * cosa + xdif * sina);
            xp = xcen + (xpm >> 2);
            yp = ycen + (ypm >> 2);
            xf = xpm & 0x03;
            yf = ypm & 0x03;

                /* if off the edge, write input grayval */
            if (xp < 0 || yp < 0 || xp > wm2 || yp > hm2) {
                *(lined + j) = colorval;
                continue;
            }

            lines = datas + yp * wpls;
            pword = lines + xp;

            switch (xf + 4 * yf)
            {
            case 0:
                *(lined + j) = *pword;
                break;
            case 1:
                word1 = *pword;
                word2 = *(pword + 1);
                red = 3 * (word1 >> 24) + (word2 >> 24);
                green = 3 * ((word1 >> 16) & 0xff) +
                            ((word2 >> 16) & 0xff);
                blue = 3 * ((word1 >> 8) & 0xff) +
                            ((word2 >> 8) & 0xff);
                *(lined + j) = ((red << 22) & 0xff000000) |
                               ((green << 14) & 0x00ff0000) |
                               ((blue << 6) & 0x0000ff00);
                break;
            case 2:
                word1 = *pword;
                word2 = *(pword + 1);
                red = (word1 >> 24) + (word2 >> 24);
                green = ((word1 >> 16) & 0xff) + ((word2 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) + ((word2 >> 8) & 0xff);
                *(lined + j) = ((red << 23) & 0xff000000) |
                               ((green << 15) & 0x00ff0000) |
                               ((blue << 7) & 0x0000ff00);
                break;
            case 3:
                word1 = *pword;
                word2 = *(pword + 1);
                red = (word1 >> 24) + 3 * (word2 >> 24);
                green = ((word1 >> 16) & 0xff) +
                          3 * ((word2 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) +
                          3 * ((word2 >> 8) & 0xff);
                *(lined + j) = ((red << 22) & 0xff000000) |
                               ((green << 14) & 0x00ff0000) |
                               ((blue << 6) & 0x0000ff00);
                break;
            case 4:
                word1 = *pword;
                word3 = *(pword + wpls);
                red = 3 * (word1 >> 24) + (word3 >> 24);
                green = 3 * ((word1 >> 16) & 0xff) +
                            ((word3 >> 16) & 0xff);
                blue = 3 * ((word1 >> 8) & 0xff) +
                            ((word3 >> 8) & 0xff);
                *(lined + j) = ((red << 22) & 0xff000000) |
                               ((green << 14) & 0x00ff0000) |
                               ((blue << 6) & 0x0000ff00);
                break;
            case 5:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = 9 * (word1 >> 24) + 3 * (word2 >> 24) +
                      3 * (word3 >> 24) + (word4 >> 24);
                green = 9 * ((word1 >> 16) & 0xff) +
                        3 * ((word2 >> 16) & 0xff) +
                        3 * ((word3 >> 16) & 0xff) +
                        ((word4 >> 16) & 0xff);
                blue = 9 * ((word1 >> 8) & 0xff) +
                       3 * ((word2 >> 8) & 0xff) +
                       3 * ((word3 >> 8) & 0xff) +
                       ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 20) & 0xff000000) |
                               ((green << 12) & 0x00ff0000) |
                               ((blue << 4) & 0x0000ff00);
                break;
            case 6:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = 3 * (word1 >> 24) +  3 * (word2 >> 24) +
                      (word3 >> 24) + (word4 >> 24);
                green = 3 * ((word1 >> 16) & 0xff) +
                        3 * ((word2 >> 16) & 0xff) +
                        ((word3 >> 16) & 0xff) +
                        ((word4 >> 16) & 0xff);
                blue = 3 * ((word1 >> 8) & 0xff) +
                       3 * ((word2 >> 8) & 0xff) +
                       ((word3 >> 8) & 0xff) +
                       ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 21) & 0xff000000) |
                               ((green << 13) & 0x00ff0000) |
                               ((blue << 5) & 0x0000ff00);
                break;
            case 7:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = 3 * (word1 >> 24) + 9 * (word2 >> 24) +
                      (word3 >> 24) + 3 * (word4 >> 24);
                green = 3 * ((word1 >> 16) & 0xff) +
                        9 * ((word2 >> 16) & 0xff) +
                        ((word3 >> 16) & 0xff) +
                        3 * ((word4 >> 16) & 0xff);
                blue = 3 * ((word1 >> 8) & 0xff) +
                       9 * ((word2 >> 8) & 0xff) +
                         ((word3 >> 8) & 0xff) +
                         3 * ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 20) & 0xff000000) |
                               ((green << 12) & 0x00ff0000) |
                               ((blue << 4) & 0x0000ff00);
                break;
            case 8:
                word1 = *pword;
                word3 = *(pword + wpls);
                red = (word1 >> 24) + (word3 >> 24);
                green = ((word1 >> 16) & 0xff) + ((word3 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) + ((word3 >> 8) & 0xff);
                *(lined + j) = ((red << 23) & 0xff000000) |
                               ((green << 15) & 0x00ff0000) |
                               ((blue << 7) & 0x0000ff00);
                break;
            case 9:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = 3 * (word1 >> 24) + (word2 >> 24) +
                      3 * (word3 >> 24) + (word4 >> 24);
                green = 3 * ((word1 >> 16) & 0xff) + ((word2 >> 16) & 0xff) +
                        3 * ((word3 >> 16) & 0xff) + ((word4 >> 16) & 0xff);
                blue = 3 * ((word1 >> 8) & 0xff) + ((word2 >> 8) & 0xff) +
                       3 * ((word3 >> 8) & 0xff) + ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 21) & 0xff000000) |
                               ((green << 13) & 0x00ff0000) |
                               ((blue << 5) & 0x0000ff00);
                break;
            case 10:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = (word1 >> 24) + (word2 >> 24) +
                      (word3 >> 24) + (word4 >> 24);
                green = ((word1 >> 16) & 0xff) + ((word2 >> 16) & 0xff) +
                        ((word3 >> 16) & 0xff) + ((word4 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) + ((word2 >> 8) & 0xff) +
                       ((word3 >> 8) & 0xff) + ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 22) & 0xff000000) |
                               ((green << 14) & 0x00ff0000) |
                               ((blue << 6) & 0x0000ff00);
                break;
            case 11:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = (word1 >> 24) + 3 * (word2 >> 24) +
                      (word3 >> 24) + 3 * (word4 >> 24);
                green = ((word1 >> 16) & 0xff) + 3 * ((word2 >> 16) & 0xff) +
                        ((word3 >> 16) & 0xff) + 3 * ((word4 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) + 3 * ((word2 >> 8) & 0xff) +
                       ((word3 >> 8) & 0xff) + 3 * ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 21) & 0xff000000) |
                               ((green << 13) & 0x00ff0000) |
                               ((blue << 5) & 0x0000ff00);
                break;
            case 12:
                word1 = *pword;
                word3 = *(pword + wpls);
                red = (word1 >> 24) + 3 * (word3 >> 24);
                green = ((word1 >> 16) & 0xff) +
                          3 * ((word3 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) +
                          3 * ((word3 >> 8) & 0xff);
                *(lined + j) = ((red << 22) & 0xff000000) |
                               ((green << 14) & 0x00ff0000) |
                               ((blue << 6) & 0x0000ff00);
                break;
            case 13:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = 3 * (word1 >> 24) + (word2 >> 24) +
                      9 * (word3 >> 24) + 3 * (word4 >> 24);
                green = 3 * ((word1 >> 16) & 0xff) + ((word2 >> 16) & 0xff) +
                        9 * ((word3 >> 16) & 0xff) + 3 * ((word4 >> 16) & 0xff);
                blue = 3 *((word1 >> 8) & 0xff) + ((word2 >> 8) & 0xff) +
                       9 * ((word3 >> 8) & 0xff) + 3 * ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 20) & 0xff000000) |
                               ((green << 12) & 0x00ff0000) |
                               ((blue << 4) & 0x0000ff00);
                break;
            case 14:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = (word1 >> 24) + (word2 >> 24) +
                      3 * (word3 >> 24) + 3 * (word4 >> 24);
                green = ((word1 >> 16) & 0xff) +((word2 >> 16) & 0xff) +
                        3 * ((word3 >> 16) & 0xff) + 3 * ((word4 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) + ((word2 >> 8) & 0xff) +
                       3 * ((word3 >> 8) & 0xff) + 3 * ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 21) & 0xff000000) |
                               ((green << 13) & 0x00ff0000) |
                               ((blue << 5) & 0x0000ff00);
                break;
            case 15:
                word1 = *pword;
                word2 = *(pword + 1);
                word3 = *(pword + wpls);
                word4 = *(pword + wpls + 1);
                red = (word1 >> 24) + 3 * (word2 >> 24) +
                      3 * (word3 >> 24) + 9 * (word4 >> 24);
                green = ((word1 >> 16) & 0xff) + 3 * ((word2 >> 16) & 0xff) +
                        3 * ((word3 >> 16) & 0xff) + 9 * ((word4 >> 16) & 0xff);
                blue = ((word1 >> 8) & 0xff) + 3 * ((word2 >> 8) & 0xff) +
                       3 * ((word3 >> 8) & 0xff) + 9 * ((word4 >> 8) & 0xff);
                *(lined + j) = ((red << 20) & 0xff000000) |
                               ((green << 12) & 0x00ff0000) |
                               ((blue << 4) & 0x0000ff00);
                break;
            default:
                lept_stderr("shouldn't get here\n");
                break;
            }
        }
    }
}

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
 * \file shear.c
 * <pre>
 *
 *    About arbitrary lines
 *           PIX      *pixHShear()
 *           PIX      *pixVShear()
 *
 *    About special 'points': UL corner and center
 *           PIX      *pixHShearCorner()
 *           PIX      *pixVShearCorner()
 *           PIX      *pixHShearCenter()
 *           PIX      *pixVShearCenter()
 *
 *    In place about arbitrary lines
 *           l_int32   pixHShearIP()
 *           l_int32   pixVShearIP()
 *
 *    Linear interpolated shear about arbitrary lines
 *           PIX      *pixHShearLI()
 *           PIX      *pixVShearLI()
 *
 *    Static helper
 *      static l_float32  normalizeAngleForShear()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>
#include "allheaders.h"

    /* Shear angle must not get too close to -pi/2 or pi/2 */
static const l_float32   MinDiffFromHalfPi = 0.04f;

static l_float32 normalizeAngleForShear(l_float32 radang, l_float32 mindif);


#ifndef  NO_CONSOLE_IO
#define  DEBUG     0
#endif  /* ~NO_CONSOLE_IO */


/*-------------------------------------------------------------*
 *                    About arbitrary lines                    *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixHShear()
 *
 * \param[in]    pixd      [optional] this can be null, equal to pixs,
 *                         or different from pixs
 * \param[in]    pixs      any depth; cmap ok
 * \param[in]    yloc      location of horizontal line, measured from origin
 * \param[in]    radang    angle in radians
 * \param[in]    incolor   L_BRING_IN_WHITE, L_BRING_IN_BLACK;
 * \return  pixd, always
 *
 * <pre>
 * Notes:
 *      (1) There are 3 cases:
 *            (a) pixd == null (make a new pixd)
 *            (b) pixd == pixs (in-place)
 *            (c) pixd != pixs
 *      (2) For these three cases, use these patterns, respectively:
 *              pixd = pixHShear(NULL, pixs, ...);
 *              pixHShear(pixs, pixs, ...);
 *              pixHShear(pixd, pixs, ...);
 *      (3) This shear leaves the horizontal line of pixels at y = yloc
 *          invariant.  For a positive shear angle, pixels above this
 *          line are shoved to the right, and pixels below this line
 *          move to the left.
 *      (4) With positive shear angle, this can be used, along with
 *          pixVShear(), to perform a cw rotation, either with 2 shears
 *          (for small angles) or in the general case with 3 shears.
 *      (5) Changing the value of yloc is equivalent to translating
 *          the result horizontally.
 *      (6) This brings in %incolor pixels from outside the image.
 *      (7) In-place shears do not work on cmapped pix, because the
 *          in-place operation cannot initialize to the requested %incolor,
 *          so we shear from a copy.
 *      (8) The angle is brought into the range [-pi, -pi].  It is
 *          not permitted to be within MinDiffFromHalfPi radians
 *          from either -pi/2 or pi/2.
 * </pre>
 */
PIX *
pixHShear(PIX       *pixd,
          PIX       *pixs,
          l_int32    yloc,
          l_float32  radang,
          l_int32    incolor)
{
l_int32    sign, w, h;
l_int32    y, yincr, inityincr, hshift;
l_float32  tanangle, invangle;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return (PIX *)ERROR_PTR("invalid incolor value", __func__, pixd);

    if (pixd == pixs) {  /* in place */
        if (!pixGetColormap(pixs)) {
            pixHShearIP(pixd, yloc, radang, incolor);
        } else {  /* can't do in-place with a colormap */
            PIX *pix1 = pixCopy(NULL, pixs);
            pixHShear(pixd, pix1, yloc, radang, incolor);
            pixDestroy(&pix1);
        }
        return pixd;
    }

        /* Make sure pixd exists and is same size as pixs */
    if (!pixd) {
        if ((pixd = pixCreateTemplate(pixs)) == NULL)
            return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    } else {  /* pixd != pixs */
        pixResizeImageData(pixd, pixs);
    }

        /* Normalize angle.  If no rotation, return a copy */
    radang = normalizeAngleForShear(radang, MinDiffFromHalfPi);
    if (radang == 0.0 || tan(radang) == 0.0)
        return pixCopy(pixd, pixs);

        /* Initialize to value of incoming pixels */
    pixSetBlackOrWhite(pixd, incolor);

    pixGetDimensions(pixs, &w, &h, NULL);
    sign = L_SIGN(radang);
    tanangle = tan(radang);
    invangle = L_ABS(1. / tanangle);
    inityincr = (l_int32)(invangle / 2.);
    yincr = (l_int32)invangle;
    pixRasterop(pixd, 0, yloc - inityincr, w, 2 * inityincr, PIX_SRC,
                pixs, 0, yloc - inityincr);

    for (hshift = 1, y = yloc + inityincr; y < h; hshift++) {
        yincr = (l_int32)(invangle * (hshift + 0.5) + 0.5) - (y - yloc);
        if (h - y < yincr)  /* reduce for last one if req'd */
            yincr = h - y;
        pixRasterop(pixd, -sign*hshift, y, w, yincr, PIX_SRC, pixs, 0, y);
#if DEBUG
        lept_stderr("y = %d, hshift = %d, yincr = %d\n", y, hshift, yincr);
#endif /* DEBUG */
        y += yincr;
    }

    for (hshift = -1, y = yloc - inityincr; y > 0; hshift--) {
        yincr = (y - yloc) - (l_int32)(invangle * (hshift - 0.5) + 0.5);
        if (y < yincr)  /* reduce for last one if req'd */
            yincr = y;
        pixRasterop(pixd, -sign*hshift, y - yincr, w, yincr, PIX_SRC,
            pixs, 0, y - yincr);
#if DEBUG
        lept_stderr("y = %d, hshift = %d, yincr = %d\n",
                y - yincr, hshift, yincr);
#endif /* DEBUG */
        y -= yincr;
    }

    return pixd;
}


/*!
 * \brief   pixVShear()
 *
 * \param[in]    pixd      [optional], this can be null, equal to pixs,
 *                         or different from pixs
 * \param[in]    pixs      any depth; cmap ok
 * \param[in]    xloc      location of vertical line, measured from origin
 * \param[in]    radang    angle in radians; not too close to +-(pi / 2)
 * \param[in]    incolor   L_BRING_IN_WHITE, L_BRING_IN_BLACK;
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) There are 3 cases:
 *            (a) pixd == null (make a new pixd)
 *            (b) pixd == pixs (in-place)
 *            (c) pixd != pixs
 *      (2) For these three cases, use these patterns, respectively:
 *              pixd = pixVShear(NULL, pixs, ...);
 *              pixVShear(pixs, pixs, ...);
 *              pixVShear(pixd, pixs, ...);
 *      (3) This shear leaves the vertical line of pixels at x = xloc
 *          invariant.  For a positive shear angle, pixels to the right
 *          of this line are shoved downward, and pixels to the left
 *          of the line move upward.
 *      (4) With positive shear angle, this can be used, along with
 *          pixHShear(), to perform a cw rotation, either with 2 shears
 *          (for small angles) or in the general case with 3 shears.
 *      (5) Changing the value of xloc is equivalent to translating
 *          the result vertically.
 *      (6) This brings in %incolor pixels from outside the image.
 *      (7) In-place shears do not work on cmapped pix, because the
 *          in-place operation cannot initialize to the requested %incolor,
 *          so we shear from a copy.
 *      (8) The angle is brought into the range [-pi, -pi].  It is
 *          not permitted to be within MinDiffFromHalfPi radians
 *          from either -pi/2 or pi/2.
 * </pre>
 */
PIX *
pixVShear(PIX       *pixd,
          PIX       *pixs,
          l_int32    xloc,
          l_float32  radang,
          l_int32    incolor)
{
l_int32    sign, w, h;
l_int32    x, xincr, initxincr, vshift;
l_float32  tanangle, invangle;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return (PIX *)ERROR_PTR("invalid incolor value", __func__, NULL);

    if (pixd == pixs) {  /* in place */
        if (!pixGetColormap(pixs)) {
            pixVShearIP(pixd, xloc, radang, incolor);
        } else {  /* can't do in-place with a colormap */
            PIX *pix1 = pixCopy(NULL, pixs);
            pixVShear(pixd, pix1, xloc, radang, incolor);
            pixDestroy(&pix1);
        }
        return pixd;
    }

        /* Make sure pixd exists and is same size as pixs */
    if (!pixd) {
        if ((pixd = pixCreateTemplate(pixs)) == NULL)
            return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    } else {  /* pixd != pixs */
        pixResizeImageData(pixd, pixs);
    }

        /* Normalize angle.  If no rotation, return a copy */
    radang = normalizeAngleForShear(radang, MinDiffFromHalfPi);
    if (radang == 0.0 || tan(radang) == 0.0)
        return pixCopy(pixd, pixs);

        /* Initialize to value of incoming pixels */
    pixSetBlackOrWhite(pixd, incolor);

    pixGetDimensions(pixs, &w, &h, NULL);
    sign = L_SIGN(radang);
    tanangle = tan(radang);
    invangle = L_ABS(1. / tanangle);
    initxincr = (l_int32)(invangle / 2.);
    xincr = (l_int32)invangle;
    pixRasterop(pixd, xloc - initxincr, 0, 2 * initxincr, h, PIX_SRC,
                pixs, xloc - initxincr, 0);

    for (vshift = 1, x = xloc + initxincr; x < w; vshift++) {
        xincr = (l_int32)(invangle * (vshift + 0.5) + 0.5) - (x - xloc);
        if (w - x < xincr)  /* reduce for last one if req'd */
            xincr = w - x;
        pixRasterop(pixd, x, sign*vshift, xincr, h, PIX_SRC, pixs, x, 0);
#if DEBUG
        lept_stderr("x = %d, vshift = %d, xincr = %d\n", x, vshift, xincr);
#endif /* DEBUG */
        x += xincr;
    }

    for (vshift = -1, x = xloc - initxincr; x > 0; vshift--) {
        xincr = (x - xloc) - (l_int32)(invangle * (vshift - 0.5) + 0.5);
        if (x < xincr)  /* reduce for last one if req'd */
            xincr = x;
        pixRasterop(pixd, x - xincr, sign*vshift, xincr, h, PIX_SRC,
            pixs, x - xincr, 0);
#if DEBUG
        lept_stderr("x = %d, vshift = %d, xincr = %d\n",
                x - xincr, vshift, xincr);
#endif /* DEBUG */
        x -= xincr;
    }

    return pixd;
}



/*-------------------------------------------------------------*
 *             Shears about UL corner and center               *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixHShearCorner()
 *
 * \param[in]    pixd      [optional], if not null, must be equal to pixs
 * \param[in]    pixs      any depth
 * \param[in]    radang    angle in radians
 * \param[in]    incolor   L_BRING_IN_WHITE, L_BRING_IN_BLACK;
 * \return  pixd, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) See pixHShear() for usage.
 *      (2) This does a horizontal shear about the UL corner, with (+) shear
 *          pushing increasingly leftward (-x) with increasing y.
 * </pre>
 */
PIX *
pixHShearCorner(PIX       *pixd,
                PIX       *pixs,
                l_float32  radang,
                l_int32    incolor)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);

    return pixHShear(pixd, pixs, 0, radang, incolor);
}


/*!
 * \brief   pixVShearCorner()
 *
 * \param[in]    pixd      [optional], if not null, must be equal to pixs
 * \param[in]    pixs      any depth
 * \param[in]    radang    angle in radians
 * \param[in]    incolor   L_BRING_IN_WHITE, L_BRING_IN_BLACK;
 * \return  pixd, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) See pixVShear() for usage.
 *      (2) This does a vertical shear about the UL corner, with (+) shear
 *          pushing increasingly downward (+y) with increasing x.
 * </pre>
 */
PIX *
pixVShearCorner(PIX       *pixd,
                PIX       *pixs,
                l_float32  radang,
                l_int32    incolor)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);

    return pixVShear(pixd, pixs, 0, radang, incolor);
}


/*!
 * \brief   pixHShearCenter()
 *
 * \param[in]    pixd      [optional] if not null, must be equal to pixs
 * \param[in]    pixs      any depth
 * \param[in]    radang    angle in radians
 * \param[in]    incolor   L_BRING_IN_WHITE, L_BRING_IN_BLACK;
 * \return  pixd, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) See pixHShear() for usage.
 *      (2) This does a horizontal shear about the center, with (+) shear
 *          pushing increasingly leftward (-x) with increasing y.
 * </pre>
 */
PIX *
pixHShearCenter(PIX       *pixd,
                PIX       *pixs,
                l_float32  radang,
                l_int32    incolor)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);

    return pixHShear(pixd, pixs, pixGetHeight(pixs) / 2, radang, incolor);
}


/*!
 * \brief   pixVShearCenter()
 *
 * \param[in]    pixd      [optional] if not null, must be equal to pixs
 * \param[in]    pixs      any depth
 * \param[in]    radang    angle in radians
 * \param[in]    incolor   L_BRING_IN_WHITE, L_BRING_IN_BLACK;
 * \return  pixd, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) See pixVShear() for usage.
 *      (2) This does a vertical shear about the center, with (+) shear
 *          pushing increasingly downward (+y) with increasing x.
 * </pre>
 */
PIX *
pixVShearCenter(PIX       *pixd,
                PIX       *pixs,
                l_float32  radang,
                l_int32    incolor)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);

    return pixVShear(pixd, pixs, pixGetWidth(pixs) / 2, radang, incolor);
}



/*--------------------------------------------------------------------------*
 *                       In place about arbitrary lines                     *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   pixHShearIP()
 *
 * \param[in]    pixs      any depth; no cmap
 * \param[in]    yloc      location of horizontal line, measured from origin
 * \param[in]    radang    angle in radians
 * \param[in]    incolor   L_BRING_IN_WHITE, L_BRING_IN_BLACK;
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is an in-place version of pixHShear(); see comments there.
 *      (2) This brings in 'incolor' pixels from outside the image.
 *      (3) pixs cannot be colormapped, because the in-place operation
 *          only blits in 0 or 1 bits, not an arbitrary colormap index.
 *      (4) Does a horizontal full-band shear about the line with (+) shear
 *          pushing increasingly leftward (-x) with increasing y.
 * </pre>
 */
l_ok
pixHShearIP(PIX       *pixs,
            l_int32    yloc,
            l_float32  radang,
            l_int32    incolor)
{
l_int32    sign, w, h;
l_int32    y, yincr, inityincr, hshift;
l_float32  tanangle, invangle;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return ERROR_INT("invalid incolor value", __func__, 1);
    if (pixGetColormap(pixs))
        return ERROR_INT("pixs is colormapped", __func__, 1);

        /* Normalize angle */
    radang = normalizeAngleForShear(radang, MinDiffFromHalfPi);
    if (radang == 0.0 || tan(radang) == 0.0)
        return 0;

    sign = L_SIGN(radang);
    pixGetDimensions(pixs, &w, &h, NULL);
    tanangle = tan(radang);
    invangle = L_ABS(1. / tanangle);
    inityincr = (l_int32)(invangle / 2.);
    yincr = (l_int32)invangle;

    if (inityincr > 0)
        pixRasteropHip(pixs, yloc - inityincr, 2 * inityincr, 0, incolor);

    for (hshift = 1, y = yloc + inityincr; y < h; hshift++) {
        yincr = (l_int32)(invangle * (hshift + 0.5) + 0.5) - (y - yloc);
        if (yincr == 0) continue;
        if (h - y < yincr)  /* reduce for last one if req'd */
            yincr = h - y;
        pixRasteropHip(pixs, y, yincr, -sign*hshift, incolor);
        y += yincr;
    }

    for (hshift = -1, y = yloc - inityincr; y > 0; hshift--) {
        yincr = (y - yloc) - (l_int32)(invangle * (hshift - 0.5) + 0.5);
        if (yincr == 0) continue;
        if (y < yincr)  /* reduce for last one if req'd */
            yincr = y;
        pixRasteropHip(pixs, y - yincr, yincr, -sign*hshift, incolor);
        y -= yincr;
    }

    return 0;
}


/*!
 * \brief   pixVShearIP()
 *
 * \param[in]    pixs      any depth; no cmap
 * \param[in]    xloc      location of vertical line, measured from origin
 * \param[in]    radang    angle in radians
 * \param[in]    incolor   L_BRING_IN_WHITE, L_BRING_IN_BLACK;
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is an in-place version of pixVShear(); see comments there.
 *      (2) This brings in 'incolor' pixels from outside the image.
 *      (3) pixs cannot be colormapped, because the in-place operation
 *          only blits in 0 or 1 bits, not an arbitrary colormap index.
 *      (4) Does a vertical full-band shear about the line with (+) shear
 *          pushing increasingly downward (+y) with increasing x.
 * </pre>
 */
l_ok
pixVShearIP(PIX       *pixs,
            l_int32    xloc,
            l_float32  radang,
            l_int32    incolor)
{
l_int32    sign, w, h;
l_int32    x, xincr, initxincr, vshift;
l_float32  tanangle, invangle;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return ERROR_INT("invalid incolor value", __func__, 1);
    if (pixGetColormap(pixs))
        return ERROR_INT("pixs is colormapped", __func__, 1);

        /* Normalize angle */
    radang = normalizeAngleForShear(radang, MinDiffFromHalfPi);
    if (radang == 0.0 || tan(radang) == 0.0)
        return 0;

    sign = L_SIGN(radang);
    pixGetDimensions(pixs, &w, &h, NULL);
    tanangle = tan(radang);
    invangle = L_ABS(1. / tanangle);
    initxincr = (l_int32)(invangle / 2.);
    xincr = (l_int32)invangle;

    if (initxincr > 0)
        pixRasteropVip(pixs, xloc - initxincr, 2 * initxincr, 0, incolor);

    for (vshift = 1, x = xloc + initxincr; x < w; vshift++) {
        xincr = (l_int32)(invangle * (vshift + 0.5) + 0.5) - (x - xloc);
        if (xincr == 0) continue;
        if (w - x < xincr)  /* reduce for last one if req'd */
            xincr = w - x;
        pixRasteropVip(pixs, x, xincr, sign*vshift, incolor);
        x += xincr;
    }

    for (vshift = -1, x = xloc - initxincr; x > 0; vshift--) {
        xincr = (x - xloc) - (l_int32)(invangle * (vshift - 0.5) + 0.5);
        if (xincr == 0) continue;
        if (x < xincr)  /* reduce for last one if req'd */
            xincr = x;
        pixRasteropVip(pixs, x - xincr, xincr, sign*vshift, incolor);
        x -= xincr;
    }

    return 0;
}


/*-------------------------------------------------------------------------*
 *              Linear interpolated shear about arbitrary lines            *
 *-------------------------------------------------------------------------*/
/*!
 * \brief   pixHShearLI()
 *
 * \param[in]    pixs      8 bpp or 32 bpp, or colormapped
 * \param[in]    yloc      location of horizontal line, measured from origin
 * \param[in]    radang    angle in radians, in range (-pi/2 ... pi/2)
 * \param[in]    incolor   L_BRING_IN_WHITE, L_BRING_IN_BLACK;
 * \return  pixd sheared, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does horizontal shear with linear interpolation for
 *          accurate results on 8 bpp gray, 32 bpp rgb, or cmapped images.
 *          It is relatively slow compared to the sampled version
 *          implemented by rasterop, but the result is much smoother.
 *      (2) This shear leaves the horizontal line of pixels at y = yloc
 *          invariant.  For a positive shear angle, pixels above this
 *          line are shoved to the right, and pixels below this line
 *          move to the left.
 *      (3) Any colormap is removed.
 *      (4) The angle is brought into the range [-pi/2 + del, pi/2 - del],
 *          where del == MinDiffFromHalfPi.
 * </pre>
 */
PIX *
pixHShearLI(PIX       *pixs,
            l_int32    yloc,
            l_float32  radang,
            l_int32    incolor)
{
l_int32    i, jd, x, xp, xf, w, h, d, wm, wpls, wpld, val, rval, gval, bval;
l_uint32   word0, word1;
l_uint32  *datas, *datad, *lines, *lined;
l_float32  tanangle, xshift;
PIX       *pix, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8 && d != 32 && !pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs not 8, 32 bpp, or cmap", __func__, NULL);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return (PIX *)ERROR_PTR("invalid incolor value", __func__, NULL);
    if (yloc < 0 || yloc >= h)
        return (PIX *)ERROR_PTR("yloc not in [0 ... h-1]", __func__, NULL);

    if (pixGetColormap(pixs))
        pix = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
    else
        pix = pixClone(pixs);

        /* Normalize angle.  If no rotation, return a copy */
    radang = normalizeAngleForShear(radang, MinDiffFromHalfPi);
    if (radang == 0.0 || tan(radang) == 0.0) {
        pixDestroy(&pix);
        return pixCopy(NULL, pixs);
    }

        /* Initialize to value of incoming pixels */
    pixd = pixCreateTemplate(pix);
    pixSetBlackOrWhite(pixd, incolor);

        /* Standard linear interp: subdivide each pixel into 64 parts */
    d = pixGetDepth(pixd);  /* 8 or 32 */
    datas = pixGetData(pix);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pix);
    wpld = pixGetWpl(pixd);
    tanangle = tan(radang);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        xshift = (yloc - i) * tanangle;
        for (jd = 0; jd < w; jd++) {
            x = (l_int32)(64.0 * (-xshift + jd) + 0.5);
            xp = x / 64;
            xf = x & 63;
            wm = w - 1;
            if (xp < 0 || xp > wm) continue;
            if (d == 8) {
                if (xp < wm) {
                    val = ((63 - xf) * GET_DATA_BYTE(lines, xp) +
                           xf * GET_DATA_BYTE(lines, xp + 1) + 31) / 63;
                } else {  /* xp == wm */
                    val = GET_DATA_BYTE(lines, xp);
                }
                SET_DATA_BYTE(lined, jd, val);
            } else {  /* d == 32 */
                if (xp < wm) {
                    word0 = *(lines + xp);
                    word1 = *(lines + xp + 1);
                    rval = ((63 - xf) * ((word0 >> L_RED_SHIFT) & 0xff) +
                           xf * ((word1 >> L_RED_SHIFT) & 0xff) + 31) / 63;
                    gval = ((63 - xf) * ((word0 >> L_GREEN_SHIFT) & 0xff) +
                           xf * ((word1 >> L_GREEN_SHIFT) & 0xff) + 31) / 63;
                    bval = ((63 - xf) * ((word0 >> L_BLUE_SHIFT) & 0xff) +
                           xf * ((word1 >> L_BLUE_SHIFT) & 0xff) + 31) / 63;
                    composeRGBPixel(rval, gval, bval, lined + jd);
                } else {  /* xp == wm */
                    lined[jd] = lines[xp];
                }
            }
        }
    }

    pixDestroy(&pix);
    return pixd;
}


/*!
 * \brief   pixVShearLI()
 *
 * \param[in]    pixs      8 bpp or 32 bpp, or colormapped
 * \param[in]    xloc      location of vertical line, measured from origin
 * \param[in]    radang    angle in radians, in range (-pi/2 ... pi/2)
 * \param[in]    incolor   L_BRING_IN_WHITE, L_BRING_IN_BLACK;
 * \return  pixd sheared, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does vertical shear with linear interpolation for
 *          accurate results on 8 bpp gray, 32 bpp rgb, or cmapped images.
 *          It is relatively slow compared to the sampled version
 *          implemented by rasterop, but the result is much smoother.
 *      (2) This shear leaves the vertical line of pixels at x = xloc
 *          invariant.  For a positive shear angle, pixels to the right
 *          of this line are shoved downward, and pixels to the left
 *          of the line move upward.
 *      (3) Any colormap is removed.
 *      (4) The angle is brought into the range [-pi/2 + del, pi/2 - del],
 *          where del == MinDiffFromHalfPi.
 * </pre>
 */
PIX *
pixVShearLI(PIX       *pixs,
            l_int32    xloc,
            l_float32  radang,
            l_int32    incolor)
{
l_int32    id, y, yp, yf, j, w, h, d, hm, wpls, wpld, val, rval, gval, bval;
l_uint32   word0, word1;
l_uint32  *datas, *datad, *lines, *lined;
l_float32  tanangle, yshift;
PIX       *pix, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8 && d != 32 && !pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pixs not 8, 32 bpp, or cmap", __func__, NULL);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return (PIX *)ERROR_PTR("invalid incolor value", __func__, NULL);
    if (xloc < 0 || xloc >= w)
        return (PIX *)ERROR_PTR("xloc not in [0 ... w-1]", __func__, NULL);

    if (pixGetColormap(pixs))
        pix = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
    else
        pix = pixClone(pixs);

        /* Normalize angle.  If no rotation, return a copy */
    radang = normalizeAngleForShear(radang, MinDiffFromHalfPi);
    if (radang == 0.0 || tan(radang) == 0.0) {
        pixDestroy(&pix);
        return pixCopy(NULL, pixs);
    }

        /* Initialize to value of incoming pixels */
    pixd = pixCreateTemplate(pix);
    pixSetBlackOrWhite(pixd, incolor);

        /* Standard linear interp: subdivide each pixel into 64 parts */
    d = pixGetDepth(pixd);  /* 8 or 32 */
    datas = pixGetData(pix);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pix);
    wpld = pixGetWpl(pixd);
    tanangle = tan(radang);
    for (j = 0; j < w; j++) {
        yshift = (j - xloc) * tanangle;
        for (id = 0; id < h; id++) {
            y = (l_int32)(64.0 * (-yshift + id) + 0.5);
            yp = y / 64;
            yf = y & 63;
            hm = h - 1;
            if (yp < 0 || yp > hm) continue;
            lines = datas + yp * wpls;
            lined = datad + id * wpld;
            if (d == 8) {
                if (yp < hm) {
                    val = ((63 - yf) * GET_DATA_BYTE(lines, j) +
                           yf * GET_DATA_BYTE(lines + wpls, j) + 31) / 63;
                } else {  /* yp == hm */
                    val = GET_DATA_BYTE(lines, j);
                }
                SET_DATA_BYTE(lined, j, val);
            } else {  /* d == 32 */
                if (yp < hm) {
                    word0 = *(lines + j);
                    word1 = *(lines + wpls + j);
                    rval = ((63 - yf) * ((word0 >> L_RED_SHIFT) & 0xff) +
                           yf * ((word1 >> L_RED_SHIFT) & 0xff) + 31) / 63;
                    gval = ((63 - yf) * ((word0 >> L_GREEN_SHIFT) & 0xff) +
                           yf * ((word1 >> L_GREEN_SHIFT) & 0xff) + 31) / 63;
                    bval = ((63 - yf) * ((word0 >> L_BLUE_SHIFT) & 0xff) +
                           yf * ((word1 >> L_BLUE_SHIFT) & 0xff) + 31) / 63;
                    composeRGBPixel(rval, gval, bval, lined + j);
                } else {  /* yp == hm */
                    lined[j] = lines[j];
                }
            }
        }
    }

    pixDestroy(&pix);
    return pixd;
}


/*-------------------------------------------------------------------------*
 *                           Angle normalization                           *
 *-------------------------------------------------------------------------*/
static l_float32
normalizeAngleForShear(l_float32  radang,
                       l_float32  mindif)
{
l_float32  pi2;

       /* Bring angle into range [-pi/2, pi/2] */
    pi2 = 3.14159265f / 2.0f;
    if (radang < -pi2 || radang > pi2)
        radang = radang - (l_int32)(radang / pi2) * pi2;

       /* If angle is too close to pi/2 or -pi/2, move it */
    if (radang > pi2 - mindif) {
        L_WARNING("angle close to pi/2; shifting away\n", __func__);
        radang = pi2 - mindif;
    } else if (radang < -pi2 + mindif) {
        L_WARNING("angle close to -pi/2; shifting away\n", __func__);
        radang = -pi2 + mindif;
    }

    return radang;
}

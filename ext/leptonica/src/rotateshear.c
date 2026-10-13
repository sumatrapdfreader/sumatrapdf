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
 * \file rotateshear.c
 * <pre>
 *
 *      Shear rotation about arbitrary point using 2 and 3 shears
 *
 *              PIX      *pixRotateShear()
 *              PIX      *pixRotate2Shear()
 *              PIX      *pixRotate3Shear()
 *
 *      Shear rotation in-place about arbitrary point using 3 shears
 *              l_int32   pixRotateShearIP()
 *
 *      Shear rotation around the image center
 *              PIX      *pixRotateShearCenter()    (2 or 3 shears)
 *              l_int32   pixRotateShearCenterIP()  (3 shears)
 *
 *  Rotation is measured in radians; clockwise rotations are positive.
 *
 *  Rotation by shear works on images of any depth,
 *  including 8 bpp color paletted images and 32 bpp
 *  rgb images.  It works by translating each src pixel
 *  value to the appropriate pixel in the rotated dest.
 *  For 8 bpp grayscale images, it is about 10-15x faster
 *  than rotation by area-mapping.
 *
 *  This speed and flexibility comes at the following cost,
 *  relative to area-mapped rotation:
 *
 *    ~  Jaggies are created on edges of straight lines
 *
 *    ~  For large angles, where you must use 3 shears,
 *       there is some extra clipping from the shears.
 *
 *  For small angles, typically less than 0.05 radians,
 *  rotation can be done with 2 orthogonal shears.
 *  Two such continuous shears (as opposed to the discrete
 *  shears on a pixel lattice that we have here) give
 *  a rotated image that has a distortion in the lengths
 *  of the two rotated and still-perpendicular axes.  The
 *  length/width ratio changes by a fraction
 *
 *       0.5 * (angle)**2
 *
 *  For an angle of 0.05 radians, this is about 1 part in
 *  a thousand.  This distortion is absent when you use
 *  3 continuous shears with the correct angles (see below).
 *
 *  Of course, the image is on a discrete pixel lattice.
 *  Rotation by shear gives an approximation to a continuous
 *  rotation, leaving pixel jaggies at sharp boundaries.
 *  For very small rotations, rotating from a corner gives
 *  better sensitivity than rotating from the image center.
 *  Here's why.  Define the shear "center" to be the line such
 *  that the image is sheared in opposite directions on
 *  each side of and parallel to the line.  For small
 *  rotations there is a "dead space" on each side of the
 *  shear center of width equal to half the shear angle,
 *  in radians.  Thus, when the image is sheared about the center,
 *  the dead space width equals the shear angle, but when
 *  the image is sheared from a corner, the dead space
 *  width is only half the shear angle.
 *
 *  All horizontal and vertical shears are implemented by
 *  rasterop.  The in-place rotation uses special in-place
 *  shears that copy rows sideways or columns vertically
 *  without buffering, and then rewrite old pixels that are
 *  no longer covered by sheared pixels.  For that rewriting,
 *  you have the choice of using white or black pixels.
 *  When not in-place, the new pix is initialized with white or black
 *  pixels by pixSetBlackOrWhite(), which also works for cmapped pix.
 *  But for in-place, this initialization is not possible, so
 *  in-place shear operations on cmapped pix are not allowed.
 *
 *  Rotation by shear is fast and depth-independent.  However, it
 *  does not work well for large rotation angles.  In fact, for
 *  rotation angles greater than about 7 degrees, more pixels are
 *  lost at the edges than when using pixRotationBySampling(), which
 *  only loses pixels because they are rotated out of the image.
 *  For larger rotations, use pixRotationBySampling() or, for
 *  more accuracy when d > 1 bpp, pixRotateAM().
 *
 *  For small angles, when comparing the quality of rotation by
 *  sampling and by shear, you can see that rotation by sampling
 *  is slightly more accurate.  However, the difference in
 *  accuracy of rotation by sampling when compared to 3-shear and
 *  (for angles less than 2 degrees, when compared to 2-shear) is
 *  less than 1 pixel at any point.  For very small angles, rotation by
 *  sampling is much slower than rotation by shear.  The speed difference
 *  depends on the pixel depth and the rotation angle.  Rotation
 *  by shear is very fast for small angles and for small depth (esp. 1 bpp).
 *  Rotation by sampling speed is independent of angle and relatively
 *  more efficient for 8 and 32 bpp images.  Here are some timings
 *  for the ratio of rotation times: (time for sampling)/ (time for shear)
  *
 *       depth (bpp)       ratio (2 deg)       ratio (10 deg)
 *       -----------------------------------------------------
 *          1                  25                  6
 *          8                   5                  2.6
 *          32                  1.6                1.0
 *
 *  In summary:
 *    * For d == 1 and small angles, use rotation by shear.  By default
 *      this will use 2-shear rotations, because 3-shears cause more
 *      visible artifacts in straight lines and, for small angles, the
 *      distortion in asperity ratio is small.
 *    * For d > 1, shear is faster than sampling, which is faster than
 *      area mapping.  However, area mapping gives the best results.
 *  These results are used in selecting the rotation methods in
 *  pixRotateShear().
 *
 *  There has been some work on what is called a "quasishear
 *  rotation" ("The Quasi-Shear Rotation, Eric Andres,
 *  DGCI 1996, pp. 307-314).  I believe they use a 3-shear
 *  approximation to the continuous rotation, exactly as
 *  we do here.  The approximation is due to being on
 *  a square pixel lattice.  They also use integers to specify
 *  the rotation angle and center offset, but that makes
 *  little sense on a machine where you have a few GFLOPS
 *  and only a few hundred floating point operations to do (!)
 *  They also allow subpixel specification of the center of
 *  rotation, which I haven't bothered with, and claim that
 *  better results are possible if each of the 4 quadrants is
 *  handled separately.
 *
 *  But the bottom line is that you are going to see shear lines when
 *  you rotate 1 bpp images.  Although the 3-shear rotation is
 *  mathematically exact in the limit of infinitesimal pixels, artifacts
 *  will be evident in real images.  One might imagine using dithering
 *  to break up the horizontal and vertical shear lines, but this
 *  is hard with block shears, where you need to dither on the block
 *  boundaries.  Dithering (by accumulation of 'error') with sampling
 *  makes more sense, but I haven't tried to do this.  There is only
 *  so much you can do with 1 bpp images!
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include <string.h>
#include "allheaders.h"

    /* Angle limits:
     *     angle < MinAngleToRotate    ==>  clone
     *     angle > MaxTwoShearAngle    ==>  warning for 2-angle shears
     *     angle > MaxThreeShearAngle  ==>  warning for 3-angle shears
     *     angle > MaxShearAngle       ==>  error
     */
static const l_float32  MinAngleToRotate = 0.001f;   /* radians; ~0.06 deg */
static const l_float32  MaxTwoShearAngle = 0.06f;    /* radians; ~3 deg    */
static const l_float32  MaxThreeShearAngle = 0.35f;  /* radians; ~20 deg   */
static const l_float32  MaxShearAngle = 0.50f;       /* radians; ~29 deg   */

/*------------------------------------------------------------------*
 *                Rotations about an arbitrary point                *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixRotateShear()
 *
 * \param[in]    pixs     any depth; cmap ok
 * \param[in]    xcen     x value for which there is no horizontal shear
 * \param[in]    ycen     y value for which there is no vertical shear
 * \param[in]    angle    radians
 * \param[in]    incolor  L_BRING_IN_WHITE, L_BRING_IN_BLACK;
 * \return  pixd, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) This rotates an image about the given point, using
 *          either 2 or 3 shears.
 *      (2) A positive angle gives a clockwise rotation.
 *      (3) This brings in 'incolor' pixels from outside the image.
 *      (4) For rotation angles larger than about 0.35 radians, we issue
 *          a warning because you should probably be using another method
 *          (either sampling or area mapping)
 * </pre>
 */
PIX *
pixRotateShear(PIX       *pixs,
               l_int32    xcen,
               l_int32    ycen,
               l_float32  angle,
               l_int32    incolor)
{
    if (!pixs)
        return (PIX *)(PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return (PIX *)(PIX *)ERROR_PTR("invalid incolor value", __func__, NULL);

    if (L_ABS(angle) > MaxShearAngle) {
        L_ERROR("%6.2f radians; too large for shear rotation\n", __func__,
                L_ABS(angle));
        return NULL;
    }
    if (L_ABS(angle) < MinAngleToRotate)
        return pixClone(pixs);

    if (L_ABS(angle) <= MaxTwoShearAngle)
        return pixRotate2Shear(pixs, xcen, ycen, angle, incolor);
    else
        return pixRotate3Shear(pixs, xcen, ycen, angle, incolor);
}


/*!
 * \brief   pixRotate2Shear()
 *
 * \param[in]    pixs         any depth; cmap ok
 * \param[in]    xcen, ycen   center of rotation
 * \param[in]    angle        radians
 * \param[in]    incolor      L_BRING_IN_WHITE, L_BRING_IN_BLACK;
 * \return  pixd, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) This rotates the image about the given point, using the 2-shear
 *          method.  It should only be used for angles no larger than
 *          MaxTwoShearAngle.  For larger angles, a warning is issued.
 *      (2) A positive angle gives a clockwise rotation.
 *      (3) 2-shear rotation by a specified angle is equivalent
 *          to the sequential transformations
 *             x' = x + tan(angle) * (y - ycen)     for x-shear
 *             y' = y + tan(angle) * (x - xcen)     for y-shear
 *      (4) Computation of tan(angle) is performed within the shear operation.
 *      (5) This brings in 'incolor' pixels from outside the image.
 *      (6) If the image has an alpha layer, it is rotated separately by
 *          two shears.
 * </pre>
 */
PIX *
pixRotate2Shear(PIX       *pixs,
                l_int32    xcen,
                l_int32    ycen,
                l_float32  angle,
                l_int32    incolor)
{
PIX  *pix1, *pix2, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return (PIX *)(PIX *)ERROR_PTR("invalid incolor value", __func__, NULL);

    if (L_ABS(angle) > MaxShearAngle) {
        L_ERROR("%6.2f radians; too large for shear rotation\n", __func__,
                L_ABS(angle));
        return NULL;
    }
    if (L_ABS(angle) < MinAngleToRotate)
        return pixClone(pixs);
    if (L_ABS(angle) > MaxTwoShearAngle)
        L_WARNING("%6.2f radians; large angle for 2-shear rotation\n",
                  __func__, L_ABS(angle));

    if ((pix1 = pixHShear(NULL, pixs, ycen, angle, incolor)) == NULL)
        return (PIX *)ERROR_PTR("pix1 not made", __func__, NULL);
    pixd = pixVShear(NULL, pix1, xcen, angle, incolor);
    pixDestroy(&pix1);
    if (!pixd)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);

    if (pixGetDepth(pixs) == 32 && pixGetSpp(pixs) == 4) {
        pix1 = pixGetRGBComponent(pixs, L_ALPHA_CHANNEL);
            /* L_BRING_IN_WHITE brings in opaque for the alpha component */
        pix2 = pixRotate2Shear(pix1, xcen, ycen, angle, L_BRING_IN_WHITE);
        pixSetRGBComponent(pixd, pix2, L_ALPHA_CHANNEL);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }
    return pixd;
}


/*!
 * \brief   pixRotate3Shear()
 *
 * \param[in]    pixs         any depth; cmap ok
 * \param[in]    xcen, ycen   center of rotation
 * \param[in]    angle        radians
 * \param[in]    incolor      L_BRING_IN_WHITE, L_BRING_IN_BLACK;
 * \return  pixd, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) This rotates the image about the given point, using the 3-shear
 *          method.  It should only be used for angles smaller than
 *          MaxThreeShearAngle.  For larger angles, a warning is issued.
 *      (2) A positive angle gives a clockwise rotation.
 *      (3) 3-shear rotation by a specified angle is equivalent
 *          to the sequential transformations
 *            y' = y + tan(angle/2) * (x - xcen)     for first y-shear
 *            x' = x + sin(angle) * (y - ycen)       for x-shear
 *            y' = y + tan(angle/2) * (x - xcen)     for second y-shear
 *      (4) Computation of tan(angle) is performed in the shear operations.
 *      (5) This brings in 'incolor' pixels from outside the image.
 *      (6) If the image has an alpha layer, it is rotated separately by
 *          two shears.
 *      (7) The algorithm was published by Alan Paeth: "A Fast Algorithm
 *          for General Raster Rotation," Graphics Interface '86,
 *          pp. 77-81, May 1986.  A description of the method, along with
 *          an implementation, can be found in Graphics Gems, p. 179,
 *          edited by Andrew Glassner, published by Academic Press, 1990.
 * </pre>
 */
PIX *
pixRotate3Shear(PIX       *pixs,
                l_int32    xcen,
                l_int32    ycen,
                l_float32  angle,
                l_int32    incolor)
{
l_float32  hangle;
PIX       *pix1, *pix2, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return (PIX *)(PIX *)ERROR_PTR("invalid incolor value", __func__, NULL);

    if (L_ABS(angle) > MaxShearAngle) {
        L_ERROR("%6.2f radians; too large for shear rotation\n", __func__,
                L_ABS(angle));
        return NULL;
    }
    if (L_ABS(angle) < MinAngleToRotate)
        return pixClone(pixs);
    if (L_ABS(angle) > MaxThreeShearAngle) {
        L_WARNING("%6.2f radians; large angle for 3-shear rotation\n",
                  __func__, L_ABS(angle));
    }

    hangle = atan(sin(angle));
    if ((pixd = pixVShear(NULL, pixs, xcen, angle / 2.f, incolor)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    if ((pix1 = pixHShear(NULL, pixd, ycen, hangle, incolor)) == NULL) {
        pixDestroy(&pixd);
        return (PIX *)ERROR_PTR("pix1 not made", __func__, NULL);
    }
    pixVShear(pixd, pix1, xcen, angle / 2.f, incolor);
    pixDestroy(&pix1);

    if (pixGetDepth(pixs) == 32 && pixGetSpp(pixs) == 4) {
        pix1 = pixGetRGBComponent(pixs, L_ALPHA_CHANNEL);
            /* L_BRING_IN_WHITE brings in opaque for the alpha component */
        pix2 = pixRotate3Shear(pix1, xcen, ycen, angle, L_BRING_IN_WHITE);
        pixSetRGBComponent(pixd, pix2, L_ALPHA_CHANNEL);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }
    return pixd;
}


/*------------------------------------------------------------------*
 *             Rotations in-place about an arbitrary point          *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixRotateShearIP()
 *
 * \param[in]    pixs         any depth; no cmap
 * \param[in]    xcen, ycen   center of rotation
 * \param[in]    angle        radians
 * \param[in]    incolor      L_BRING_IN_WHITE, L_BRING_IN_BLACK
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This does an in-place rotation of the image about the
 *          specified point, using the 3-shear method.  It should only
 *          be used for angles smaller than MaxThreeShearAngle.
 *          For larger angles, a warning is issued.
 *      (2) A positive angle gives a clockwise rotation.
 *      (3) 3-shear rotation by a specified angle is equivalent
 *          to the sequential transformations
 *            y' = y + tan(angle/2) * (x - xcen)      for first y-shear
 *            x' = x + sin(angle) * (y - ycen)        for x-shear
 *            y' = y + tan(angle/2) * (x - xcen)      for second y-shear
 *      (4) Computation of tan(angle) is performed in the shear operations.
 *      (5) This brings in 'incolor' pixels from outside the image.
 *      (6) The pix cannot be colormapped, because the in-place operation
 *          only blits in 0 or 1 bits, not an arbitrary colormap index.
 * </pre>
 */
l_ok
pixRotateShearIP(PIX       *pixs,
                 l_int32    xcen,
                 l_int32    ycen,
                 l_float32  angle,
                 l_int32    incolor)
{
l_float32  hangle;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return ERROR_INT("invalid value for incolor", __func__, 1);
    if (pixGetColormap(pixs) != NULL)
        return ERROR_INT("pixs is colormapped", __func__, 1);

    if (angle == 0.0)
        return 0;
    if (L_ABS(angle) > MaxThreeShearAngle) {
        L_WARNING("%6.2f radians; large angle for in-place 3-shear rotation\n",
                  __func__, L_ABS(angle));
    }

    hangle = atan(sin(angle));
    pixHShearIP(pixs, ycen, angle / 2.f, incolor);
    pixVShearIP(pixs, xcen, hangle, incolor);
    pixHShearIP(pixs, ycen, angle / 2.f, incolor);
    return 0;
}


/*------------------------------------------------------------------*
 *                    Rotations about the image center              *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixRotateShearCenter()
 *
 * \param[in]    pixs      any depth; cmap ok
 * \param[in]    angle     radians
 * \param[in]    incolor   L_BRING_IN_WHITE, L_BRING_IN_BLACK
 * \return  pixd, or NULL on error
 */
PIX *
pixRotateShearCenter(PIX       *pixs,
                     l_float32  angle,
                     l_int32    incolor)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

    return pixRotateShear(pixs, pixGetWidth(pixs) / 2,
                          pixGetHeight(pixs) / 2, angle, incolor);
}


/*!
 * \brief   pixRotateShearCenterIP()
 *
 * \param[in]    pixs      any depth; no cmap
 * \param[in]    angle     radians
 * \param[in]    incolor   L_BRING_IN_WHITE, L_BRING_IN_BLACK
 * \return  0 if OK, 1 on error
 */
l_ok
pixRotateShearCenterIP(PIX       *pixs,
                       l_float32  angle,
                       l_int32    incolor)
{
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

    return pixRotateShearIP(pixs, pixGetWidth(pixs) / 2,
                            pixGetHeight(pixs) / 2, angle, incolor);
}

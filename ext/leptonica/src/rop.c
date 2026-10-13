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
 * \file rop.c
 * <pre>
 *      General rasterop
 *           l_int32    pixRasterop()
 *
 *      In-place full band translation
 *           l_int32    pixRasteropVip()
 *           l_int32    pixRasteropHip()
 *
 *      Full image translation (general and in-place)
 *           l_int32    pixTranslate()
 *           l_int32    pixRasteropIP()
 *
 *      Full image rasterop with no translation
 *           l_int32    pixRasteropFullImage()
 *
 *      Checking for invalid crop box
 *           static l_int32   checkRasteropCrop()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

static l_int32 checkRasteropCrop(l_int32 pixw, l_int32 pixh, l_int32 dx,
                                 l_int32 dy, l_int32 dw, l_int32 dh);


/*--------------------------------------------------------------------*
 *                General rasterop (basic pix interface)              *
 *--------------------------------------------------------------------*/
/*!
 * \brief   pixRasterop()
 *
 * \param[in]    pixd   dest pix
 * \param[in]    dx     x val of UL corner of dest rectangle
 * \param[in]    dy     y val of UL corner of dest rectangle
 * \param[in]    dw     width of dest rectangle
 * \param[in]    dh     height of dest rectangle
 * \param[in]    op     op code
 * \param[in]    pixs   src pix
 * \param[in]    sx     x val of UL corner of src rectangle
 * \param[in]    sy     y val of UL corner of src rectangle
 * \return  0 if OK; 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) This has the standard set of 9 args for rasterop.
 *          This function is your friend; it is worth memorizing!
 *      (2) If the operation involves only dest, this calls
 *          rasteropUniLow().  Otherwise, checks depth of the
 *          src and dest, and if they match, calls rasteropLow().
 *      (3) For the two-image operation, where both pixs and pixd
 *          are defined, they are typically different images.  However
 *          there are cases, such as pixSetMirroredBorder(), where
 *          in-place operations can be done, blitting pixels from
 *          one part of pixd to another.  Consequently, we permit
 *          such operations.  If you use them, be sure that there
 *          is no overlap between the source and destination rectangles
 *          in pixd (!)
 *
 *  Background:
 *  -----------
 *
 *  There are 18 operations, described by the op codes in pix.h.
 *
 *  One, PIX_DST, is a no-op.
 *
 *  Three, PIX_CLR, PIX_SET, and PIX_NOT(PIX_DST) operate only on the dest.
 *  These are handled by the low-level rasteropUniLow().
 *
 *  The other 14 involve both the src and the dest, and depend on
 *  the bit values of either just the src or the bit values of both
 *  src and dest.  They are handled by rasteropLow():
 *
 *          PIX_SRC                             s
 *          PIX_NOT(PIX_SRC)                   ~s
 *          PIX_SRC | PIX_DST                   s | d
 *          PIX_SRC & PIX_DST                   s & d
 *          PIX_SRC ^ PIX_DST                   s ^ d
 *          PIX_NOT(PIX_SRC) | PIX_DST         ~s | d
 *          PIX_NOT(PIX_SRC) & PIX_DST         ~s & d
 *          PIX_NOT(PIX_SRC) ^ PIX_DST         ~s ^ d
 *          PIX_SRC | PIX_NOT(PIX_DST)          s | ~d
 *          PIX_SRC & PIX_NOT(PIX_DST)          s & ~d
 *          PIX_SRC ^ PIX_NOT(PIX_DST)          s ^ ~d
 *          PIX_NOT(PIX_SRC | PIX_DST)         ~(s | d)
 *          PIX_NOT(PIX_SRC & PIX_DST)         ~(s & d)
 *          PIX_NOT(PIX_SRC ^ PIX_DST)         ~(s ^ d)
 *
 *  Each of these is implemented with one of three low-level
 *  functions, depending on the alignment of the left edge
 *  of the src and dest rectangles:
 *      * a fastest implementation if both left edges are
 *        (32-bit) word aligned
 *      * a very slightly slower implementation if both left
 *        edges have the same relative (32-bit) word alignment
 *      * the general routine that is invoked when
 *        both left edges have different word alignment
 *
 *  Of the 14 binary rasterops above, only 12 are unique
 *  logical combinations (out of a possible 16) of src
 *  and dst bits:
 *
 *        (sd)         (11)   (10)   (01)   (00)
 *   -----------------------------------------------
 *         s            1      1      0      0
 *        ~s            0      1      0      1
 *       s | d          1      1      1      0
 *       s & d          1      0      0      0
 *       s ^ d          0      1      1      0
 *      ~s | d          1      0      1      1
 *      ~s & d          0      0      1      0
 *      ~s ^ d          1      0      0      1
 *       s | ~d         1      1      0      1
 *       s & ~d         0      1      0      0
 *       s ^ ~d         1      0      0      1
 *      ~(s | d)        0      0      0      1
 *      ~(s & d)        0      1      1      1
 *      ~(s ^ d)        1      0      0      1
 *
 *  Note that the following three operations are equivalent:
 *      ~(s ^ d)
 *      ~s ^ d
 *      s ^ ~d
 *  and in the implementation, we call them out with the first form;
 *  namely, ~(s ^ d).
 *
 *  Of the 16 possible binary combinations of src and dest bits,
 *  the remaining 4 unique ones are independent of the src bit.
 *  They depend on either just the dest bit or on neither
 *  the src nor dest bits:
 *
 *         d            1      0      1      0    (indep. of s)
 *        ~d            0      1      0      1    (indep. of s)
 *        CLR           0      0      0      0    (indep. of both s & d)
 *        SET           1      1      1      1    (indep. of both s & d)
 *
 *  As mentioned above, three of these are implemented by
 *  rasteropUniLow(), and one is a no-op.
 *
 *  How can these operation codes be represented by bits
 *  in such a way that when the basic operations are performed
 *  on the bits the results are unique for unique
 *  operations, and mimic the logic table given above?
 *
 *  The answer is to choose a particular order of the pairings:
 *         (sd)         (11)   (10)   (01)   (00)
 *  (which happens to be the same as in the above table)
 *  and to translate the result into 4-bit representations
 *  of s and d.  For example, the Sun rasterop choice
 *  (omitting the extra bit for clipping) is
 *
 *      PIX_SRC      0xc
 *      PIX_DST      0xa
 *
 *  This corresponds to our pairing order given above:
 *         (sd)         (11)   (10)   (01)   (00)
 *  where for s = 1 we get the bit pattern
 *       PIX_SRC:        1      1      0      0     (0xc)
 *  and for d = 1 we get the pattern
 *       PIX_DST:         1      0      1      0    (0xa)
 *
 *  OK, that's the pairing order that Sun chose.  How many different
 *  ways can we assign bit patterns to PIX_SRC and PIX_DST to get
 *  the boolean ops to work out?  Any of the 4 pairs can be put
 *  in the first position, any of the remaining 3 pairs can go
 *  in the second; and one of the remaining 2 pairs can go the the third.
 *  There is a total of 4*3*2 = 24 ways these pairs can be permuted.
 * </pre>
 */
l_ok
pixRasterop(PIX     *pixd,
            l_int32  dx,
            l_int32  dy,
            l_int32  dw,
            l_int32  dh,
            l_int32  op,
            PIX     *pixs,
            l_int32  sx,
            l_int32  sy)
{
l_int32  dpw, dph, dpd, spw, sph, spd;

    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);

    if (op == PIX_DST)   /* no-op */
        return 0;

    pixGetDimensions(pixd, &dpw, &dph, &dpd);
#if 0
    if (checkRasteropCrop(dpw, dph, dx, dy, dw, dh)) {
        L_WARNING("dest crop box out of bounds\n", __func__);
        return 1;
    }
#endif

        /* Check if operation is only on dest */
    if (op == PIX_CLR || op == PIX_SET || op == PIX_NOT(PIX_DST)) {
        rasteropUniLow(pixGetData(pixd), dpw, dph, dpd, pixGetWpl(pixd),
                       dx, dy, dw, dh, op);
        return 0;
    }

        /* Two-image rasterop; the depths must match */
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixs, &spw, &sph, &spd);
    if (dpd != spd)
        return ERROR_INT("depths of pixs and pixd differ", __func__, 1);
#if 0
    if (checkRasteropCrop(spw, sph, sx, sy, dw, dh)) {
        L_WARNING("source crop box out of bounds\n", __func__);
        return 1;
    }
#endif

    rasteropLow(pixGetData(pixd), dpw, dph, dpd, pixGetWpl(pixd),
                dx, dy, dw, dh, op,
                pixGetData(pixs), spw, sph, pixGetWpl(pixs), sx, sy);
    return 0;
}


/*--------------------------------------------------------------------*
 *                    In-place full band translation                  *
 *--------------------------------------------------------------------*/
/*!
 * \brief   pixRasteropVip()
 *
 * \param[in]    pixd     in-place
 * \param[in]    bx       left edge of vertical band
 * \param[in]    bw       width of vertical band
 * \param[in]    vshift   vertical shift of band; vshift > 0 is down
 * \param[in]    incolor  L_BRING_IN_WHITE, L_BRING_IN_BLACK
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This rasterop translates a vertical band of the
 *          image either up or down, bringing in either white
 *          or black pixels from outside the image.
 *      (2) The vertical band extends the full height of pixd.
 *      (3) If a colormap exists, the nearest color to white or black
 *          is brought in.
 * </pre>
 */
l_ok
pixRasteropVip(PIX     *pixd,
               l_int32  bx,
               l_int32  bw,
               l_int32  vshift,
               l_int32  incolor)
{
l_int32   w, h, d, index, op;
PIX      *pixt;
PIXCMAP  *cmap;

    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return ERROR_INT("invalid value for incolor", __func__, 1);
    if (bw <= 0)
        return ERROR_INT("bw must be > 0", __func__, 1);

    if (vshift == 0)
        return 0;

    pixGetDimensions(pixd, &w, &h, &d);
    rasteropVipLow(pixGetData(pixd), w, h, d, pixGetWpl(pixd), bx, bw, vshift);

    cmap = pixGetColormap(pixd);
    if (!cmap) {
        if ((d == 1 && incolor == L_BRING_IN_BLACK) ||
            (d > 1 && incolor == L_BRING_IN_WHITE))
            op = PIX_SET;
        else
            op = PIX_CLR;

            /* Set the pixels brought in at top or bottom */
        if (vshift > 0)
            pixRasterop(pixd, bx, 0, bw, vshift, op, NULL, 0, 0);
        else  /* vshift < 0 */
            pixRasterop(pixd, bx, h + vshift, bw, -vshift, op, NULL, 0, 0);
        return 0;
    }

        /* Get the nearest index and fill with that */
    if (incolor == L_BRING_IN_BLACK)
        pixcmapGetRankIntensity(cmap, 0.0, &index);
    else  /* white */
        pixcmapGetRankIntensity(cmap, 1.0, &index);
    pixt = pixCreate(bw, L_ABS(vshift), d);
    pixSetAllArbitrary(pixt, index);
    if (vshift > 0)
        pixRasterop(pixd, bx, 0, bw, vshift, PIX_SRC, pixt, 0, 0);
    else  /* vshift < 0 */
        pixRasterop(pixd, bx, h + vshift, bw, -vshift, PIX_SRC, pixt, 0, 0);
    pixDestroy(&pixt);
    return 0;
}


/*!
 * \brief   pixRasteropHip()
 *
 * \param[in]    pixd     in-place operation
 * \param[in]    by       top of horizontal band
 * \param[in]    bh       height of horizontal band
 * \param[in]    hshift   horizontal shift of band; hshift > 0 is to right
 * \param[in]    incolor  L_BRING_IN_WHITE, L_BRING_IN_BLACK
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This rasterop translates a horizontal band of the
 *          image either left or right, bringing in either white
 *          or black pixels from outside the image.
 *      (2) The horizontal band extends the full width of pixd.
 *      (3) If a colormap exists, the nearest color to white or black
 *          is brought in.
 * </pre>
 */
l_ok
pixRasteropHip(PIX     *pixd,
               l_int32  by,
               l_int32  bh,
               l_int32  hshift,
               l_int32  incolor)
{
l_int32   w, h, d, index, op;
PIX      *pixt;
PIXCMAP  *cmap;

    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return ERROR_INT("invalid value for incolor", __func__, 1);
    if (bh <= 0)
        return ERROR_INT("bh must be > 0", __func__, 1);

    if (hshift == 0)
        return 0;

    pixGetDimensions(pixd, &w, &h, &d);
    rasteropHipLow(pixGetData(pixd), h, d, pixGetWpl(pixd), by, bh, hshift);

    cmap = pixGetColormap(pixd);
    if (!cmap) {
        if ((d == 1 && incolor == L_BRING_IN_BLACK) ||
            (d > 1 && incolor == L_BRING_IN_WHITE))
            op = PIX_SET;
        else
            op = PIX_CLR;

            /* Set the pixels brought in at left or right */
        if (hshift > 0)
            pixRasterop(pixd, 0, by, hshift, bh, op, NULL, 0, 0);
        else  /* hshift < 0 */
            pixRasterop(pixd, w + hshift, by, -hshift, bh, op, NULL, 0, 0);
        return 0;
    }

        /* Get the nearest index and fill with that */
    if (incolor == L_BRING_IN_BLACK)
        pixcmapGetRankIntensity(cmap, 0.0, &index);
    else  /* white */
        pixcmapGetRankIntensity(cmap, 1.0, &index);
    pixt = pixCreate(L_ABS(hshift), bh, d);
    pixSetAllArbitrary(pixt, index);
    if (hshift > 0)
        pixRasterop(pixd, 0, by, hshift, bh, PIX_SRC, pixt, 0, 0);
    else  /* hshift < 0 */
        pixRasterop(pixd, w + hshift, by, -hshift, bh, PIX_SRC, pixt, 0, 0);
    pixDestroy(&pixt);
    return 0;
}


/*--------------------------------------------------------------------*
 *             Full image translation (general and in-place)          *
 *--------------------------------------------------------------------*/
/*!
 * \brief   pixTranslate()
 *
 * \param[in]    pixd    [optional] destination: this can be null,
 *                        equal to pixs, or different from pixs
 * \param[in]    pixs
 * \param[in]    hshift   horizontal shift; hshift > 0 is to right
 * \param[in]    vshift   vertical shift; vshift > 0 is down
 * \param[in]    incolor  L_BRING_IN_WHITE, L_BRING_IN_BLACK
 * \return  pixd, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) The general pattern is:
 *            pixd = pixTranslate(pixd, pixs, ...);
 *          For clarity, when you know the case, use one of these:
 *            pixd = pixTranslate(NULL, pixs, ...);  // new
 *            pixTranslate(pixs, pixs, ...);         // in-place
 *            pixTranslate(pixd, pixs, ...);         // to existing pixd
 *      (2) If an existing pixd is not the same size as pixs, the
 *          image data will be reallocated.
 * </pre>
 */
PIX *
pixTranslate(PIX     *pixd,
             PIX     *pixs,
             l_int32  hshift,
             l_int32  vshift,
             l_int32  incolor)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);

        /* Prepare pixd for in-place operation */
    if ((pixd = pixCopy(pixd, pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);

    pixRasteropIP(pixd, hshift, vshift, incolor);
    return pixd;
}


/*!
 * \brief   pixRasteropIP()
 *
 * \param[in]    pixd     in-place translation
 * \param[in]    hshift   horizontal shift; hshift > 0 is to right
 * \param[in]    vshift   vertical shift; vshift > 0 is down
 * \param[in]    incolor  L_BRING_IN_WHITE, L_BRING_IN_BLACK
 * \return  0 if OK; 1 on error
 */
l_ok
pixRasteropIP(PIX     *pixd,
              l_int32  hshift,
              l_int32  vshift,
              l_int32  incolor)
{
l_int32  w, h;

    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);

    pixGetDimensions(pixd, &w, &h, NULL);
    pixRasteropHip(pixd, 0, h, hshift, incolor);
    pixRasteropVip(pixd, 0, w, vshift, incolor);

    return 0;
}


/*--------------------------------------------------------------------*
 *                 Full image rasterop with no shifts                 *
 *--------------------------------------------------------------------*/
/*!
 * \brief   pixRasteropFullImage()
 *
 * \param[in]    pixd
 * \param[in]    pixs
 * \param[in]    op     any of the op-codes
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      ~ this is a wrapper for a common 2-image raster operation
 *      ~ both pixs and pixd must be defined
 *      ~ the operation is performed with aligned UL corners of pixs and pixd
 *      ~ the operation clips to the smallest pix; if the width or height
 *        of pixd is larger than pixs, some pixels in pixd will be unchanged
 * </pre>
 */
l_ok
pixRasteropFullImage(PIX     *pixd,
                     PIX     *pixs,
                     l_int32  op)
{
    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

    pixRasterop(pixd, 0, 0, pixGetWidth(pixd), pixGetHeight(pixd), op,
                pixs, 0, 0);
    return 0;
}


/*--------------------------------------------------------------------*
 *                    Checking for invalid crop box                   *
 *--------------------------------------------------------------------*/
/*!
 * \brief   checkRasteropCrop()
 *
 * \param[in]    pixw, pixh   pix dimensions
 * \param[in]    x, y, w, h   crop box parameters
 * \return  0 if OK, 1 if the crop box does not intersect with the pix.
 *
 * <pre>
 * Notes:
 *      (1) The widths and heights must all be positive, but %x and %y
 *          can take on any value.
 *      (2) This works for checking both the source and dest regions.
 *      (3) This has been used to verify rasteropLow() cropping is correct.
 *          It is not needed for pre-filtering in pixRasterop().
 * </pre>
 */
static l_int32
checkRasteropCrop(l_int32 pixw,
                  l_int32 pixh,
                  l_int32 x,
                  l_int32 y,
                  l_int32 w,
                  l_int32 h)
{
    if (pixw < 1 || pixh < 1 || w < 1 || h < 1)
        return ERROR_INT("dimension is <= 0", __func__, 1);

    if (x + w <= 0 || y + h <= 0)
        return ERROR_INT("box to left or above pix", __func__, 1);

    if (x >= pixw || y >= pixh)
        return ERROR_INT("box to right or below pix", __func__, 1);

    return 0;
}

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
 * \file graymorph.c
 * <pre>
 *
 *      Top-level grayscale morphological operations (van Herk / Gil-Werman)
 *            PIX           *pixErodeGray()
 *            PIX           *pixDilateGray()
 *            PIX           *pixOpenGray()
 *            PIX           *pixCloseGray()
 *
 *      Special operations for 1x3, 3x1 and 3x3 Sels  (direct)
 *            PIX           *pixErodeGray3()
 *            static PIX    *pixErodeGray3h()
 *            static PIX    *pixErodeGray3v()
 *            PIX           *pixDilateGray3()
 *            static PIX    *pixDilateGray3h()
 *            static PIX    *pixDilateGray3v()
 *            PIX           *pixOpenGray3()
 *            PIX           *pixCloseGray3()
 *
 *      Low-level grayscale morphological operations
 *            static void    dilateGrayLow()
 *            static void    erodeGrayLow()
 *
 *
 *      Method: Algorithm by van Herk and Gil and Werman, 1992
 *
 *      Measured speed of the vH/G-W implementation is about 1 output
 *      pixel per 120 PIII clock cycles, for a horizontal or vertical
 *      erosion or dilation.  The computation time doubles for opening
 *      or closing, or for a square SE, as expected, and is independent
 *      of the size of the SE.
 *
 *      A faster implementation can be made directly for brick Sels
 *      of maximum size 3.  We unroll the computation for sets of 8 bytes.
 *      It needs to be called explicitly; the general functions do not
 *      default for the size 3 brick Sels.
 *
 *      We use the van Herk/Gil-Werman (vHGW) algorithm, [van Herk,
 *      Patt. Recog. Let. 13, pp. 517-521, 1992; Gil and Werman,
 *      IEEE Trans PAMI 15(5), pp. 504-507, 1993.]
 *      This was the first grayscale morphology
 *      algorithm to compute dilation and erosion with
 *      complexity independent of the size of the structuring
 *      element.  It is simple and elegant, and surprising that
 *      it was discovered as recently as 1992.  It works for
 *      SEs composed of horizontal and/or vertical lines.  The
 *      general case requires finding the Min or Max over an
 *      arbitrary set of pixels, and this requires a number of
 *      pixel comparisons equal to the SE "size" at each pixel
 *      in the image.  The vHGW algorithm requires not
 *      more than 3 comparisons at each point.  The algorithm has been
 *      recently refined by Gil and Kimmel ("Efficient Dilation
 *      Erosion, Opening and Closing Algorithms", in "Mathematical
 *      Morphology and its Applications to Image and Signal Processing",
 *      the proceedings of the International Symposium on Mathematical
 *      Morphology, Palo Alto, CA, June 2000, Kluwer Academic
 *      Publishers, pp. 301-310).  They bring this number down below
 *      1.5 comparisons per output pixel but at a cost of significantly
 *      increased complexity, so I don't bother with that here.
 *
 *      In brief, the method is as follows.  We evaluate the dilation
 *      in groups of "size" pixels, equal to the size of the SE.
 *      For horizontal, we start at x = "size"/2 and go
 *      (w - 2 * ("size"/2))/"size" steps.  This means that
 *      we don't evaluate the first 0.5 * "size" pixels and, worst
 *      case, the last 1.5 * "size" pixels.  Thus we embed the
 *      image in a larger image with these augmented dimensions, where
 *      the new border pixels are appropriately initialized (0 for
 *      dilation; 255 for erosion), and remove the boundary at the end.
 *      (For vertical, use h instead of w.)   Then for each group
 *      of "size" pixels, we form an array of length 2 * "size" + 1,
 *      consisting of backward and forward partial maxima (for
 *      dilation) or minima (for erosion).  This represents a
 *      jumping window computed from the source image, over which
 *      the SE will slide.  The center of the array gets the source
 *      pixel at the center of the SE.  Call this the center pixel
 *      of the window.  Array values to left of center get
 *      the maxima(minima) of the pixels from the center
 *      one and going to the left an equal distance.  Array
 *      values to the right of center get the maxima(minima) to
 *      the pixels from the center one and going to the right
 *      an equal distance.  These are computed sequentially starting
 *      from the center one.  The SE (of length "size") can slide over this
 *      window (of length 2 * "size + 1) at "size" different places.
 *      At each place, the maxima(minima) of the values in the window
 *      that correspond to the end points of the SE give the extremal
 *      values over that interval, and these are stored at the dest
 *      pixel corresponding to the SE center.  A picture is worth
 *      at least this many words, so if this isn't clear, see the
 *      leptonica documentation on grayscale morphology.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

    /* Special static operations for 3x1, 1x3 and 3x3 structuring elements */
static PIX *pixErodeGray3h(PIX *pixs);
static PIX *pixErodeGray3v(PIX *pixs);
static PIX *pixDilateGray3h(PIX *pixs);
static PIX *pixDilateGray3v(PIX *pixs);

    /*  Low-level gray morphological operations */
static void dilateGrayLow(l_uint32 *datad, l_int32 w, l_int32 h,
                          l_int32 wpld, l_uint32 *datas, l_int32 wpls,
                          l_int32 size, l_int32 direction, l_uint8 *buffer,
                          l_uint8 *maxarray);
static void erodeGrayLow(l_uint32 *datad, l_int32 w, l_int32 h,
                         l_int32 wpld, l_uint32 *datas, l_int32 wpls,
                         l_int32 size, l_int32 direction, l_uint8 *buffer,
                         l_uint8 *minarray);

/*-----------------------------------------------------------------*
 *           Top-level grayscale morphological operations          *
 *-----------------------------------------------------------------*/
/*!
 * \brief   pixErodeGray()
 *
 * \param[in]    pixs
 * \param[in]    hsize  of Sel; must be odd; origin implicitly in center
 * \param[in]    vsize  ditto
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      (1) Sel is a brick with all elements being hits
 *      (2) If hsize = vsize = 1, just returns a copy.
 * </pre>
 */
PIX *
pixErodeGray(PIX     *pixs,
             l_int32  hsize,
             l_int32  vsize)
{
l_uint8   *buffer, *minarray;
l_int32    w, h, wplb, wplt;
l_int32    leftpix, rightpix, toppix, bottompix, maxsize;
l_uint32  *datab, *datat;
PIX       *pixb, *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize or vsize < 1", __func__, NULL);
    if ((hsize & 1) == 0 ) {
        L_WARNING("horiz sel size must be odd; increasing by 1\n", __func__);
        hsize++;
    }
    if ((vsize & 1) == 0 ) {
        L_WARNING("vert sel size must be odd; increasing by 1\n", __func__);
        vsize++;
    }

    pixb = pixt = pixd = NULL;
    buffer = minarray = NULL;

    if (hsize == 1 && vsize == 1)
        return pixCopy(NULL, pixs);

    if (vsize == 1) {  /* horizontal sel */
        leftpix = (hsize + 1) / 2;
        rightpix = (3 * hsize + 1) / 2;
        toppix = 0;
        bottompix = 0;
    } else if (hsize == 1) {  /* vertical sel */
        leftpix = 0;
        rightpix = 0;
        toppix = (vsize + 1) / 2;
        bottompix = (3 * vsize + 1) / 2;
    } else {
        leftpix = (hsize + 1) / 2;
        rightpix = (3 * hsize + 1) / 2;
        toppix = (vsize + 1) / 2;
        bottompix = (3 * vsize + 1) / 2;
    }

    pixb = pixAddBorderGeneral(pixs, leftpix, rightpix, toppix, bottompix, 255);
    pixt = pixCreateTemplate(pixb);
    if (!pixb || !pixt) {
        L_ERROR("pixb and pixt not made\n", __func__);
        goto cleanup;
    }

    pixGetDimensions(pixt, &w, &h, NULL);
    datab = pixGetData(pixb);
    datat = pixGetData(pixt);
    wplb = pixGetWpl(pixb);
    wplt = pixGetWpl(pixt);

    buffer = (l_uint8 *)LEPT_CALLOC(L_MAX(w, h), sizeof(l_uint8));
    maxsize = L_MAX(hsize, vsize);
    minarray = (l_uint8 *)LEPT_CALLOC(2 * maxsize, sizeof(l_uint8));
    if (!buffer || !minarray) {
        L_ERROR("buffer and minarray not made\n", __func__);
        goto cleanup;
    }

    if (vsize == 1) {
        erodeGrayLow(datat, w, h, wplt, datab, wplb, hsize, L_HORIZ,
                     buffer, minarray);
    } else if (hsize == 1) {
        erodeGrayLow(datat, w, h, wplt, datab, wplb, vsize, L_VERT,
                     buffer, minarray);
    } else {
        erodeGrayLow(datat, w, h, wplt, datab, wplb, hsize, L_HORIZ,
                     buffer, minarray);
        pixSetOrClearBorder(pixt, leftpix, rightpix, toppix, bottompix,
                            PIX_SET);
        erodeGrayLow(datab, w, h, wplb, datat, wplt, vsize, L_VERT,
                     buffer, minarray);
        pixDestroy(&pixt);
        pixt = pixClone(pixb);
    }

    pixd = pixRemoveBorderGeneral(pixt, leftpix, rightpix, toppix, bottompix);
    if (!pixd)
        L_ERROR("pixd not made\n", __func__);

cleanup:
    LEPT_FREE(buffer);
    LEPT_FREE(minarray);
    pixDestroy(&pixb);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixDilateGray()
 *
 * \param[in]    pixs
 * \param[in]    hsize  of Sel; must be odd; origin implicitly in center
 * \param[in]    vsize  ditto
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      (1) Sel is a brick with all elements being hits
 *      (2) If hsize = vsize = 1, just returns a copy.
 * </pre>
 */
PIX *
pixDilateGray(PIX     *pixs,
              l_int32  hsize,
              l_int32  vsize)
{
l_uint8   *buffer, *maxarray;
l_int32    w, h, wplb, wplt;
l_int32    leftpix, rightpix, toppix, bottompix, maxsize;
l_uint32  *datab, *datat;
PIX       *pixb, *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize or vsize < 1", __func__, NULL);
    if ((hsize & 1) == 0 ) {
        L_WARNING("horiz sel size must be odd; increasing by 1\n", __func__);
        hsize++;
    }
    if ((vsize & 1) == 0 ) {
        L_WARNING("vert sel size must be odd; increasing by 1\n", __func__);
        vsize++;
    }

    pixb = pixt = pixd = NULL;
    buffer = maxarray = NULL;

    if (hsize == 1 && vsize == 1)
        return pixCopy(NULL, pixs);

    if (vsize == 1) {  /* horizontal sel */
        leftpix = (hsize + 1) / 2;
        rightpix = (3 * hsize + 1) / 2;
        toppix = 0;
        bottompix = 0;
    } else if (hsize == 1) {  /* vertical sel */
        leftpix = 0;
        rightpix = 0;
        toppix = (vsize + 1) / 2;
        bottompix = (3 * vsize + 1) / 2;
    } else {
        leftpix = (hsize + 1) / 2;
        rightpix = (3 * hsize + 1) / 2;
        toppix = (vsize + 1) / 2;
        bottompix = (3 * vsize + 1) / 2;
    }

    pixb = pixAddBorderGeneral(pixs, leftpix, rightpix, toppix, bottompix, 0);
    pixt = pixCreateTemplate(pixb);
    if (!pixb || !pixt) {
        L_ERROR("pixb and pixt not made\n", __func__);
        goto cleanup;
    }

    pixGetDimensions(pixt, &w, &h, NULL);
    datab = pixGetData(pixb);
    datat = pixGetData(pixt);
    wplb = pixGetWpl(pixb);
    wplt = pixGetWpl(pixt);

    buffer = (l_uint8 *)LEPT_CALLOC(L_MAX(w, h), sizeof(l_uint8));
    maxsize = L_MAX(hsize, vsize);
    maxarray = (l_uint8 *)LEPT_CALLOC(2 * maxsize, sizeof(l_uint8));
    if (!buffer || !maxarray) {
        L_ERROR("buffer and maxarray not made\n", __func__);
        goto cleanup;
    }

    if (vsize == 1) {
        dilateGrayLow(datat, w, h, wplt, datab, wplb, hsize, L_HORIZ,
                      buffer, maxarray);
    } else if (hsize == 1) {
        dilateGrayLow(datat, w, h, wplt, datab, wplb, vsize, L_VERT,
                      buffer, maxarray);
    } else {
        dilateGrayLow(datat, w, h, wplt, datab, wplb, hsize, L_HORIZ,
                      buffer, maxarray);
        pixSetOrClearBorder(pixt, leftpix, rightpix, toppix, bottompix,
                            PIX_CLR);
        dilateGrayLow(datab, w, h, wplb, datat, wplt, vsize, L_VERT,
                      buffer, maxarray);
        pixDestroy(&pixt);
        pixt = pixClone(pixb);
    }

    pixd = pixRemoveBorderGeneral(pixt, leftpix, rightpix, toppix, bottompix);
    if (!pixd)
        L_ERROR("pixd not made\n", __func__);

cleanup:
    LEPT_FREE(buffer);
    LEPT_FREE(maxarray);
    pixDestroy(&pixb);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixOpenGray()
 *
 * \param[in]    pixs
 * \param[in]    hsize  of Sel; must be odd; origin implicitly in center
 * \param[in]    vsize  ditto
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      (1) Sel is a brick with all elements being hits
 *      (2) If hsize = vsize = 1, just returns a copy.
 * </pre>
 */
PIX *
pixOpenGray(PIX     *pixs,
            l_int32  hsize,
            l_int32  vsize)
{
l_uint8   *buffer;
l_uint8   *array;  /* used to find either min or max in interval */
l_int32    w, h, wplb, wplt;
l_int32    leftpix, rightpix, toppix, bottompix, maxsize;
l_uint32  *datab, *datat;
PIX       *pixb, *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize or vsize < 1", __func__, NULL);
    if ((hsize & 1) == 0 ) {
        L_WARNING("horiz sel size must be odd; increasing by 1\n", __func__);
        hsize++;
    }
    if ((vsize & 1) == 0 ) {
        L_WARNING("vert sel size must be odd; increasing by 1\n", __func__);
        vsize++;
    }

    pixb = pixt = pixd = NULL;
    buffer = array = NULL;

    if (hsize == 1 && vsize == 1)
        return pixCopy(NULL, pixs);

    if (vsize == 1) {  /* horizontal sel */
        leftpix = (hsize + 1) / 2;
        rightpix = (3 * hsize + 1) / 2;
        toppix = 0;
        bottompix = 0;
    } else if (hsize == 1) {  /* vertical sel */
        leftpix = 0;
        rightpix = 0;
        toppix = (vsize + 1) / 2;
        bottompix = (3 * vsize + 1) / 2;
    } else {
        leftpix = (hsize + 1) / 2;
        rightpix = (3 * hsize + 1) / 2;
        toppix = (vsize + 1) / 2;
        bottompix = (3 * vsize + 1) / 2;
    }

    pixb = pixAddBorderGeneral(pixs, leftpix, rightpix, toppix, bottompix, 255);
    pixt = pixCreateTemplate(pixb);
    if (!pixb || !pixt) {
        L_ERROR("pixb and pixt not made\n", __func__);
        goto cleanup;
    }

    pixGetDimensions(pixt, &w, &h, NULL);
    datab = pixGetData(pixb);
    datat = pixGetData(pixt);
    wplb = pixGetWpl(pixb);
    wplt = pixGetWpl(pixt);

    buffer = (l_uint8 *)LEPT_CALLOC(L_MAX(w, h), sizeof(l_uint8));
    maxsize = L_MAX(hsize, vsize);
    array = (l_uint8 *)LEPT_CALLOC(2 * maxsize, sizeof(l_uint8));
    if (!buffer || !array) {
        L_ERROR("buffer and array not made\n", __func__);
        goto cleanup;
    }

    if (vsize == 1) {
        erodeGrayLow(datat, w, h, wplt, datab, wplb, hsize, L_HORIZ,
                     buffer, array);
        pixSetOrClearBorder(pixt, leftpix, rightpix, toppix, bottompix,
                            PIX_CLR);
        dilateGrayLow(datab, w, h, wplb, datat, wplt, hsize, L_HORIZ,
                      buffer, array);
    }
    else if (hsize == 1) {
        erodeGrayLow(datat, w, h, wplt, datab, wplb, vsize, L_VERT,
                     buffer, array);
        pixSetOrClearBorder(pixt, leftpix, rightpix, toppix, bottompix,
                            PIX_CLR);
        dilateGrayLow(datab, w, h, wplb, datat, wplt, vsize, L_VERT,
                      buffer, array);
    } else {
        erodeGrayLow(datat, w, h, wplt, datab, wplb, hsize, L_HORIZ,
                     buffer, array);
        pixSetOrClearBorder(pixt, leftpix, rightpix, toppix, bottompix,
                            PIX_SET);
        erodeGrayLow(datab, w, h, wplb, datat, wplt, vsize, L_VERT,
                     buffer, array);
        pixSetOrClearBorder(pixb, leftpix, rightpix, toppix, bottompix,
                            PIX_CLR);
        dilateGrayLow(datat, w, h, wplt, datab, wplb, hsize, L_HORIZ,
                      buffer, array);
        pixSetOrClearBorder(pixt, leftpix, rightpix, toppix, bottompix,
                            PIX_CLR);
        dilateGrayLow(datab, w, h, wplb, datat, wplt, vsize, L_VERT,
                      buffer, array);
    }

    pixd = pixRemoveBorderGeneral(pixb, leftpix, rightpix, toppix, bottompix);
    if (!pixd)
        L_ERROR("pixd not made\n", __func__);

cleanup:
    LEPT_FREE(buffer);
    LEPT_FREE(array);
    pixDestroy(&pixb);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixCloseGray()
 *
 * \param[in]    pixs
 * \param[in]    hsize  of Sel; must be odd; origin implicitly in center
 * \param[in]    vsize  ditto
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      (1) Sel is a brick with all elements being hits
 *      (2) If hsize = vsize = 1, just returns a copy.
 * </pre>
 */
PIX *
pixCloseGray(PIX     *pixs,
             l_int32  hsize,
             l_int32  vsize)
{
l_uint8   *buffer;
l_uint8   *array;  /* used to find either min or max in interval */
l_int32    w, h, wplb, wplt;
l_int32    leftpix, rightpix, toppix, bottompix, maxsize;
l_uint32  *datab, *datat;
PIX       *pixb, *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize or vsize < 1", __func__, NULL);
    if ((hsize & 1) == 0 ) {
        L_WARNING("horiz sel size must be odd; increasing by 1\n", __func__);
        hsize++;
    }
    if ((vsize & 1) == 0 ) {
        L_WARNING("vert sel size must be odd; increasing by 1\n", __func__);
        vsize++;
    }

    pixb = pixt = pixd = NULL;
    buffer = array = NULL;

    if (hsize == 1 && vsize == 1)
        return pixCopy(NULL, pixs);

    if (vsize == 1) {  /* horizontal sel */
        leftpix = (hsize + 1) / 2;
        rightpix = (3 * hsize + 1) / 2;
        toppix = 0;
        bottompix = 0;
    } else if (hsize == 1) {  /* vertical sel */
        leftpix = 0;
        rightpix = 0;
        toppix = (vsize + 1) / 2;
        bottompix = (3 * vsize + 1) / 2;
    } else {
        leftpix = (hsize + 1) / 2;
        rightpix = (3 * hsize + 1) / 2;
        toppix = (vsize + 1) / 2;
        bottompix = (3 * vsize + 1) / 2;
    }

    pixb = pixAddBorderGeneral(pixs, leftpix, rightpix, toppix, bottompix, 0);
    pixt = pixCreateTemplate(pixb);
    if (!pixb || !pixt) {
        L_ERROR("pixb and pixt not made\n", __func__);
        goto cleanup;
    }

    pixGetDimensions(pixt, &w, &h, NULL);
    datab = pixGetData(pixb);
    datat = pixGetData(pixt);
    wplb = pixGetWpl(pixb);
    wplt = pixGetWpl(pixt);

    buffer = (l_uint8 *)LEPT_CALLOC(L_MAX(w, h), sizeof(l_uint8));
    maxsize = L_MAX(hsize, vsize);
    array = (l_uint8 *)LEPT_CALLOC(2 * maxsize, sizeof(l_uint8));
    if (!buffer || !array) {
        L_ERROR("buffer and array not made\n", __func__);
        goto cleanup;
    }

    if (vsize == 1) {
        dilateGrayLow(datat, w, h, wplt, datab, wplb, hsize, L_HORIZ,
                     buffer, array);
        pixSetOrClearBorder(pixt, leftpix, rightpix, toppix, bottompix,
                            PIX_SET);
        erodeGrayLow(datab, w, h, wplb, datat, wplt, hsize, L_HORIZ,
                      buffer, array);
    } else if (hsize == 1) {
        dilateGrayLow(datat, w, h, wplt, datab, wplb, vsize, L_VERT,
                     buffer, array);
        pixSetOrClearBorder(pixt, leftpix, rightpix, toppix, bottompix,
                            PIX_SET);
        erodeGrayLow(datab, w, h, wplb, datat, wplt, vsize, L_VERT,
                      buffer, array);
    } else {
        dilateGrayLow(datat, w, h, wplt, datab, wplb, hsize, L_HORIZ,
                      buffer, array);
        pixSetOrClearBorder(pixt, leftpix, rightpix, toppix, bottompix,
                            PIX_CLR);
        dilateGrayLow(datab, w, h, wplb, datat, wplt, vsize, L_VERT,
                      buffer, array);
        pixSetOrClearBorder(pixb, leftpix, rightpix, toppix, bottompix,
                            PIX_SET);
        erodeGrayLow(datat, w, h, wplt, datab, wplb, hsize, L_HORIZ,
                     buffer, array);
        pixSetOrClearBorder(pixt, leftpix, rightpix, toppix, bottompix,
                            PIX_SET);
        erodeGrayLow(datab, w, h, wplb, datat, wplt, vsize, L_VERT,
                     buffer, array);
    }

    pixd = pixRemoveBorderGeneral(pixb, leftpix, rightpix, toppix, bottompix);
    if (!pixd)
        L_ERROR("pixd not made\n", __func__);

cleanup:
    LEPT_FREE(buffer);
    LEPT_FREE(array);
    pixDestroy(&pixb);
    pixDestroy(&pixt);
    return pixd;
}


/*-----------------------------------------------------------------*
 *           Special operations for 1x3, 3x1 and 3x3 Sels          *
 *-----------------------------------------------------------------*/
/*!
 * \brief   pixErodeGray3()
 *
 * \param[in]    pixs    8 bpp, not cmapped
 * \param[in]    hsize   1 or 3
 * \param[in]    vsize   1 or 3
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Special case for 1x3, 3x1 or 3x3 brick sel (all hits)
 *      (2) If hsize = vsize = 1, just returns a copy.
 *      (3) It would be nice not to add a border, but it is required
 *          if we want the same results as from the general case.
 *          We add 4 bytes on the left to speed up the copying, and
 *          8 bytes at the right and bottom to allow unrolling of
 *          the computation of 8 pixels.
 * </pre>
 */
PIX *
pixErodeGray3(PIX     *pixs,
              l_int32  hsize,
              l_int32  vsize)
{
PIX  *pixt, *pixb, *pixbd, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pix has colormap", __func__, NULL);
    if ((hsize != 1 && hsize != 3) ||
        (vsize != 1 && vsize != 3))
        return (PIX *)ERROR_PTR("invalid size: must be 1 or 3", __func__, NULL);

    if (hsize == 1 && vsize == 1)
        return pixCopy(NULL, pixs);

    pixb = pixAddBorderGeneral(pixs, 4, 8, 2, 8, 255);

    if (vsize == 1)
        pixbd = pixErodeGray3h(pixb);
    else if (hsize == 1)
        pixbd = pixErodeGray3v(pixb);
    else {  /* vize == hsize == 3 */
        pixt = pixErodeGray3h(pixb);
        pixbd = pixErodeGray3v(pixt);
        pixDestroy(&pixt);
    }

    pixd = pixRemoveBorderGeneral(pixbd, 4, 8, 2, 8);
    pixDestroy(&pixb);
    pixDestroy(&pixbd);
    return pixd;
}


/*!
 * \brief   pixErodeGray3h()
 *
 * \param[in]    pixs    8 bpp, not cmapped
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Special case for horizontal 3x1 brick Sel;
 *          also used as the first step for the 3x3 brick Sel.
 * </pre>
 */
static PIX *
pixErodeGray3h(PIX  *pixs)
{
l_uint32  *datas, *datad, *lines, *lined;
l_int32    w, h, wpl, i, j;
l_int32    val0, val1, val2, val3, val4, val5, val6, val7, val8, val9, minval;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);

    pixd = pixCreateTemplate(pixs);
    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpl = pixGetWpl(pixs);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpl;
        lined = datad + i * wpl;
        for (j = 1; j < w - 8; j += 8) {
            val0 = GET_DATA_BYTE(lines, j - 1);
            val1 = GET_DATA_BYTE(lines, j);
            val2 = GET_DATA_BYTE(lines, j + 1);
            val3 = GET_DATA_BYTE(lines, j + 2);
            val4 = GET_DATA_BYTE(lines, j + 3);
            val5 = GET_DATA_BYTE(lines, j + 4);
            val6 = GET_DATA_BYTE(lines, j + 5);
            val7 = GET_DATA_BYTE(lines, j + 6);
            val8 = GET_DATA_BYTE(lines, j + 7);
            val9 = GET_DATA_BYTE(lines, j + 8);
            minval = L_MIN(val1, val2);
            SET_DATA_BYTE(lined, j, L_MIN(val0, minval));
            SET_DATA_BYTE(lined, j + 1, L_MIN(minval, val3));
            minval = L_MIN(val3, val4);
            SET_DATA_BYTE(lined, j + 2, L_MIN(val2, minval));
            SET_DATA_BYTE(lined, j + 3, L_MIN(minval, val5));
            minval = L_MIN(val5, val6);
            SET_DATA_BYTE(lined, j + 4, L_MIN(val4, minval));
            SET_DATA_BYTE(lined, j + 5, L_MIN(minval, val7));
            minval = L_MIN(val7, val8);
            SET_DATA_BYTE(lined, j + 6, L_MIN(val6, minval));
            SET_DATA_BYTE(lined, j + 7, L_MIN(minval, val9));
        }
    }
    return pixd;
}


/*!
 * \brief   pixErodeGray3v()
 *
 * \param[in]    pixs    8 bpp, not cmapped
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Special case for vertical 1x3 brick Sel;
 *          also used as the second step for the 3x3 brick Sel.
 *      (2) Surprisingly, this is faster than setting up the
 *          lineptrs array and accessing into it; e.g.,
 *              val4 = GET_DATA_BYTE(lines8[i + 3], j);
 * </pre>
 */
static PIX *
pixErodeGray3v(PIX  *pixs)
{
l_uint32  *datas, *datad, *linesi, *linedi;
l_int32    w, h, wpl, i, j;
l_int32    val0, val1, val2, val3, val4, val5, val6, val7, val8, val9, minval;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);

    pixd = pixCreateTemplate(pixs);
    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpl = pixGetWpl(pixs);
    for (j = 0; j < w; j++) {
        for (i = 1; i < h - 8; i += 8) {
            linesi = datas + i * wpl;
            linedi = datad + i * wpl;
            val0 = GET_DATA_BYTE(linesi - wpl, j);
            val1 = GET_DATA_BYTE(linesi, j);
            val2 = GET_DATA_BYTE(linesi + wpl, j);
            val3 = GET_DATA_BYTE(linesi + 2 * wpl, j);
            val4 = GET_DATA_BYTE(linesi + 3 * wpl, j);
            val5 = GET_DATA_BYTE(linesi + 4 * wpl, j);
            val6 = GET_DATA_BYTE(linesi + 5 * wpl, j);
            val7 = GET_DATA_BYTE(linesi + 6 * wpl, j);
            val8 = GET_DATA_BYTE(linesi + 7 * wpl, j);
            val9 = GET_DATA_BYTE(linesi + 8 * wpl, j);
            minval = L_MIN(val1, val2);
            SET_DATA_BYTE(linedi, j, L_MIN(val0, minval));
            SET_DATA_BYTE(linedi + wpl, j, L_MIN(minval, val3));
            minval = L_MIN(val3, val4);
            SET_DATA_BYTE(linedi + 2 * wpl, j, L_MIN(val2, minval));
            SET_DATA_BYTE(linedi + 3 * wpl, j, L_MIN(minval, val5));
            minval = L_MIN(val5, val6);
            SET_DATA_BYTE(linedi + 4 * wpl, j, L_MIN(val4, minval));
            SET_DATA_BYTE(linedi + 5 * wpl, j, L_MIN(minval, val7));
            minval = L_MIN(val7, val8);
            SET_DATA_BYTE(linedi + 6 * wpl, j, L_MIN(val6, minval));
            SET_DATA_BYTE(linedi + 7 * wpl, j, L_MIN(minval, val9));
        }
    }
    return pixd;
}


/*!
 * \brief   pixDilateGray3()
 *
 * \param[in]    pixs    8 bpp, not cmapped
 * \param[in]    hsize   1 or 3
 * \param[in]    vsize   1 or 3
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Special case for 1x3, 3x1 or 3x3 brick sel (all hits)
 *      (2) If hsize = vsize = 1, just returns a copy.
 * </pre>
 */
PIX *
pixDilateGray3(PIX     *pixs,
               l_int32  hsize,
               l_int32  vsize)
{
PIX  *pixt, *pixb, *pixbd, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pix has colormap", __func__, NULL);
    if ((hsize != 1 && hsize != 3) ||
        (vsize != 1 && vsize != 3))
        return (PIX *)ERROR_PTR("invalid size: must be 1 or 3", __func__, NULL);

    if (hsize == 1 && vsize == 1)
        return pixCopy(NULL, pixs);

    pixb = pixAddBorderGeneral(pixs, 4, 8, 2, 8, 0);

    if (vsize == 1)
        pixbd = pixDilateGray3h(pixb);
    else if (hsize == 1)
        pixbd = pixDilateGray3v(pixb);
    else {  /* vize == hsize == 3 */
        pixt = pixDilateGray3h(pixb);
        pixbd = pixDilateGray3v(pixt);
        pixDestroy(&pixt);
    }

    pixd = pixRemoveBorderGeneral(pixbd, 4, 8, 2, 8);
    pixDestroy(&pixb);
    pixDestroy(&pixbd);
    return pixd;
}


/*!
 * \brief   pixDilateGray3h()
 *
 * \param[in]    pixs    8 bpp, not cmapped
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Special case for horizontal 3x1 brick Sel;
 *          also used as the first step for the 3x3 brick Sel.
 * </pre>
 */
static PIX *
pixDilateGray3h(PIX  *pixs)
{
l_uint32  *datas, *datad, *lines, *lined;
l_int32    w, h, wpl, i, j;
l_int32    val0, val1, val2, val3, val4, val5, val6, val7, val8, val9, maxval;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);

    pixd = pixCreateTemplate(pixs);
    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpl = pixGetWpl(pixs);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpl;
        lined = datad + i * wpl;
        for (j = 1; j < w - 8; j += 8) {
            val0 = GET_DATA_BYTE(lines, j - 1);
            val1 = GET_DATA_BYTE(lines, j);
            val2 = GET_DATA_BYTE(lines, j + 1);
            val3 = GET_DATA_BYTE(lines, j + 2);
            val4 = GET_DATA_BYTE(lines, j + 3);
            val5 = GET_DATA_BYTE(lines, j + 4);
            val6 = GET_DATA_BYTE(lines, j + 5);
            val7 = GET_DATA_BYTE(lines, j + 6);
            val8 = GET_DATA_BYTE(lines, j + 7);
            val9 = GET_DATA_BYTE(lines, j + 8);
            maxval = L_MAX(val1, val2);
            SET_DATA_BYTE(lined, j, L_MAX(val0, maxval));
            SET_DATA_BYTE(lined, j + 1, L_MAX(maxval, val3));
            maxval = L_MAX(val3, val4);
            SET_DATA_BYTE(lined, j + 2, L_MAX(val2, maxval));
            SET_DATA_BYTE(lined, j + 3, L_MAX(maxval, val5));
            maxval = L_MAX(val5, val6);
            SET_DATA_BYTE(lined, j + 4, L_MAX(val4, maxval));
            SET_DATA_BYTE(lined, j + 5, L_MAX(maxval, val7));
            maxval = L_MAX(val7, val8);
            SET_DATA_BYTE(lined, j + 6, L_MAX(val6, maxval));
            SET_DATA_BYTE(lined, j + 7, L_MAX(maxval, val9));
        }
    }
    return pixd;
}


/*!
 * \brief   pixDilateGray3v()
 *
 * \param[in]    pixs    8 bpp, not cmapped
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Special case for vertical 1x3 brick Sel;
 *          also used as the second step for the 3x3 brick Sel.
 * </pre>
 */
static PIX *
pixDilateGray3v(PIX  *pixs)
{
l_uint32  *datas, *datad, *linesi, *linedi;
l_int32    w, h, wpl, i, j;
l_int32    val0, val1, val2, val3, val4, val5, val6, val7, val8, val9, maxval;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);

    pixd = pixCreateTemplate(pixs);
    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpl = pixGetWpl(pixs);
    for (j = 0; j < w; j++) {
        for (i = 1; i < h - 8; i += 8) {
            linesi = datas + i * wpl;
            linedi = datad + i * wpl;
            val0 = GET_DATA_BYTE(linesi - wpl, j);
            val1 = GET_DATA_BYTE(linesi, j);
            val2 = GET_DATA_BYTE(linesi + wpl, j);
            val3 = GET_DATA_BYTE(linesi + 2 * wpl, j);
            val4 = GET_DATA_BYTE(linesi + 3 * wpl, j);
            val5 = GET_DATA_BYTE(linesi + 4 * wpl, j);
            val6 = GET_DATA_BYTE(linesi + 5 * wpl, j);
            val7 = GET_DATA_BYTE(linesi + 6 * wpl, j);
            val8 = GET_DATA_BYTE(linesi + 7 * wpl, j);
            val9 = GET_DATA_BYTE(linesi + 8 * wpl, j);
            maxval = L_MAX(val1, val2);
            SET_DATA_BYTE(linedi, j, L_MAX(val0, maxval));
            SET_DATA_BYTE(linedi + wpl, j, L_MAX(maxval, val3));
            maxval = L_MAX(val3, val4);
            SET_DATA_BYTE(linedi + 2 * wpl, j, L_MAX(val2, maxval));
            SET_DATA_BYTE(linedi + 3 * wpl, j, L_MAX(maxval, val5));
            maxval = L_MAX(val5, val6);
            SET_DATA_BYTE(linedi + 4 * wpl, j, L_MAX(val4, maxval));
            SET_DATA_BYTE(linedi + 5 * wpl, j, L_MAX(maxval, val7));
            maxval = L_MAX(val7, val8);
            SET_DATA_BYTE(linedi + 6 * wpl, j, L_MAX(val6, maxval));
            SET_DATA_BYTE(linedi + 7 * wpl, j, L_MAX(maxval, val9));
        }
    }
    return pixd;
}


/*!
 * \brief   pixOpenGray3()
 *
 * \param[in]    pixs    8 bpp, not cmapped
 * \param[in]    hsize   1 or 3
 * \param[in]    vsize   1 or 3
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Special case for 1x3, 3x1 or 3x3 brick sel (all hits)
 *      (2) If hsize = vsize = 1, just returns a copy.
 *      (3) It would be nice not to add a border, but it is required
 *          to get the same results as for the general case.
 * </pre>
 */
PIX *
pixOpenGray3(PIX     *pixs,
             l_int32  hsize,
             l_int32  vsize)
{
PIX  *pixt, *pixb, *pixbd, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pix has colormap", __func__, NULL);
    if ((hsize != 1 && hsize != 3) ||
        (vsize != 1 && vsize != 3))
        return (PIX *)ERROR_PTR("invalid size: must be 1 or 3", __func__, NULL);

    if (hsize == 1 && vsize == 1)
        return pixCopy(NULL, pixs);

    pixb = pixAddBorderGeneral(pixs, 4, 8, 2, 8, 255);  /* set to max */

    if (vsize == 1) {
        pixt = pixErodeGray3h(pixb);
        pixSetBorderVal(pixt, 4, 8, 2, 8, 0);  /* set to min */
        pixbd = pixDilateGray3h(pixt);
        pixDestroy(&pixt);
    } else if (hsize == 1) {
        pixt = pixErodeGray3v(pixb);
        pixSetBorderVal(pixt, 4, 8, 2, 8, 0);
        pixbd = pixDilateGray3v(pixt);
        pixDestroy(&pixt);
    } else {  /* vize == hsize == 3 */
        pixt = pixErodeGray3h(pixb);
        pixbd = pixErodeGray3v(pixt);
        pixDestroy(&pixt);
        pixSetBorderVal(pixbd, 4, 8, 2, 8, 0);
        pixt = pixDilateGray3h(pixbd);
        pixDestroy(&pixbd);
        pixbd = pixDilateGray3v(pixt);
        pixDestroy(&pixt);
    }

    pixd = pixRemoveBorderGeneral(pixbd, 4, 8, 2, 8);
    pixDestroy(&pixb);
    pixDestroy(&pixbd);
    return pixd;
}


/*!
 * \brief   pixCloseGray3()
 *
 * \param[in]    pixs    8 bpp, not cmapped
 * \param[in]    hsize   1 or 3
 * \param[in]    vsize   1 or 3
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Special case for 1x3, 3x1 or 3x3 brick sel (all hits)
 *      (2) If hsize = vsize = 1, just returns a copy.
 * </pre>
 */
PIX *
pixCloseGray3(PIX     *pixs,
              l_int32  hsize,
              l_int32  vsize)
{
PIX  *pixt, *pixb, *pixbd, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (pixGetColormap(pixs))
        return (PIX *)ERROR_PTR("pix has colormap", __func__, NULL);
    if ((hsize != 1 && hsize != 3) ||
        (vsize != 1 && vsize != 3))
        return (PIX *)ERROR_PTR("invalid size: must be 1 or 3", __func__, NULL);

    if (hsize == 1 && vsize == 1)
        return pixCopy(NULL, pixs);

    pixb = pixAddBorderGeneral(pixs, 4, 8, 2, 8, 0);  /* set to min */

    if (vsize == 1) {
        pixt = pixDilateGray3h(pixb);
        pixSetBorderVal(pixt, 4, 8, 2, 8, 255);  /* set to max */
        pixbd = pixErodeGray3h(pixt);
        pixDestroy(&pixt);
    } else if (hsize == 1) {
        pixt = pixDilateGray3v(pixb);
        pixSetBorderVal(pixt, 4, 8, 2, 8, 255);
        pixbd = pixErodeGray3v(pixt);
        pixDestroy(&pixt);
    } else {  /* vize == hsize == 3 */
        pixt = pixDilateGray3h(pixb);
        pixbd = pixDilateGray3v(pixt);
        pixDestroy(&pixt);
        pixSetBorderVal(pixbd, 4, 8, 2, 8, 255);
        pixt = pixErodeGray3h(pixbd);
        pixDestroy(&pixbd);
        pixbd = pixErodeGray3v(pixt);
        pixDestroy(&pixt);
    }

    pixd = pixRemoveBorderGeneral(pixbd, 4, 8, 2, 8);
    pixDestroy(&pixb);
    pixDestroy(&pixbd);
    return pixd;
}


/*-----------------------------------------------------------------*
 *              Low-level gray morphological operations            *
 *-----------------------------------------------------------------*/
/*!
 * \brief   dilateGrayLow()
 *
 * \param[in]    datad       8 bpp dsst image
 * \param[in]    w, h        dimensions of src and dest
 * \param[in]    wpld        words/line of dest
 * \param[in]    datas       8 bpp src image
 * \param[in]    wpls        words/line of src
 * \param[in]    size        full length of SEL; restricted to odd numbers
 * \param[in]    direction   L_HORIZ or L_VERT
 * \param[in]    buffer      holds full line or column of src image pixels
 * \param[in]    maxarray    array of dimension 2*size+1
 * \return  void
 *
 * <pre>
 * Notes:
 *        (1) To eliminate border effects on the actual image, these images
 *            are prepared with an additional border of dimensions:
 *               leftpix = 0.5 * size
 *               rightpix = 1.5 * size
 *               toppix = 0.5 * size
 *               bottompix = 1.5 * size
 *            and we initialize the src border pixels to 0.
 *            This allows full processing over the actual image; at
 *            the end the border is removed.
 *        (2) Uses algorithm of van Herk, Gil and Werman
 * </pre>
 */
static void
dilateGrayLow(l_uint32  *datad,
              l_int32    w,
              l_int32    h,
              l_int32    wpld,
              l_uint32  *datas,
              l_int32    wpls,
              l_int32    size,
              l_int32    direction,
              l_uint8   *buffer,
              l_uint8   *maxarray)
{
l_int32    i, j, k;
l_int32    hsize, nsteps, startmax, startx, starty;
l_uint8    maxval;
l_uint32  *lines, *lined;

    if (direction == L_HORIZ) {
        hsize = size / 2;
        nsteps = (w - 2 * hsize) / size;
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;

                /* fill buffer with pixels in byte order */
            for (j = 0; j < w; j++)
                buffer[j] = GET_DATA_BYTE(lines, j);

            for (j = 0; j < nsteps; j++) {
                    /* refill the minarray */
                startmax = (j + 1) * size - 1;
                maxarray[size - 1] = buffer[startmax];
                for (k = 1; k < size; k++) {
                    maxarray[size - 1 - k] =
                        L_MAX(maxarray[size - k], buffer[startmax - k]);
                    maxarray[size - 1 + k] =
                        L_MAX(maxarray[size + k - 2], buffer[startmax + k]);
                }

                    /* compute dilation values */
                startx = hsize + j * size;
                SET_DATA_BYTE(lined, startx, maxarray[0]);
                SET_DATA_BYTE(lined, startx + size - 1, maxarray[2 * size - 2]);
                for (k = 1; k < size - 1; k++) {
                    maxval = L_MAX(maxarray[k], maxarray[k + size - 1]);
                    SET_DATA_BYTE(lined, startx + k, maxval);
                }
            }
        }
    } else {  /* direction == L_VERT */
        hsize = size / 2;
        nsteps = (h - 2 * hsize) / size;
        for (j = 0; j < w; j++) {
                /* fill buffer with pixels in byte order */
            for (i = 0; i < h; i++) {
                lines = datas + i * wpls;
                buffer[i] = GET_DATA_BYTE(lines, j);
            }

            for (i = 0; i < nsteps; i++) {
                    /* refill the minarray */
                startmax = (i + 1) * size - 1;
                maxarray[size - 1] = buffer[startmax];
                for (k = 1; k < size; k++) {
                    maxarray[size - 1 - k] =
                        L_MAX(maxarray[size - k], buffer[startmax - k]);
                    maxarray[size - 1 + k] =
                        L_MAX(maxarray[size + k - 2], buffer[startmax + k]);
                }

                    /* compute dilation values */
                starty = hsize + i * size;
                lined = datad + starty * wpld;
                SET_DATA_BYTE(lined, j, maxarray[0]);
                SET_DATA_BYTE(lined + (size - 1) * wpld, j,
                        maxarray[2 * size - 2]);
                for (k = 1; k < size - 1; k++) {
                    maxval = L_MAX(maxarray[k], maxarray[k + size - 1]);
                    SET_DATA_BYTE(lined + wpld * k, j, maxval);
                }
            }
        }
    }

    return;
}


/*!
 * \brief   erodeGrayLow()
 *
 * \param[in]    datad       8 bpp dsst image
 * \param[in]    w, h        dimensions of src and dest
 * \param[in]    wpld        words/line of dest
 * \param[in]    datas       8 bpp src image
 * \param[in]    wpls        words/line of src
 * \param[in]    size        full length of SEL; restricted to odd numbers
 * \param[in]    direction   L_HORIZ or L_VERT
 * \param[in]    buffer      holds full line or column of src image pixels
 * \param[in]    minarray    array of dimension 2*size+1
 * \return  void
 *
 * <pre>
 * Notes:
 *        (1) See notes in dilateGrayLow()
 * </pre>
 */
static void
erodeGrayLow(l_uint32  *datad,
             l_int32    w,
             l_int32    h,
             l_int32    wpld,
             l_uint32  *datas,
             l_int32    wpls,
             l_int32    size,
             l_int32    direction,
             l_uint8   *buffer,
             l_uint8   *minarray)
{
l_int32    i, j, k;
l_int32    hsize, nsteps, startmin, startx, starty;
l_uint8    minval;
l_uint32  *lines, *lined;

    if (direction == L_HORIZ) {
        hsize = size / 2;
        nsteps = (w - 2 * hsize) / size;
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;

                /* fill buffer with pixels in byte order */
            for (j = 0; j < w; j++)
                buffer[j] = GET_DATA_BYTE(lines, j);

            for (j = 0; j < nsteps; j++) {
                    /* refill the minarray */
                startmin = (j + 1) * size - 1;
                minarray[size - 1] = buffer[startmin];
                for (k = 1; k < size; k++) {
                    minarray[size - 1 - k] =
                        L_MIN(minarray[size - k], buffer[startmin - k]);
                    minarray[size - 1 + k] =
                        L_MIN(minarray[size + k - 2], buffer[startmin + k]);
                }

                    /* compute erosion values */
                startx = hsize + j * size;
                SET_DATA_BYTE(lined, startx, minarray[0]);
                SET_DATA_BYTE(lined, startx + size - 1, minarray[2 * size - 2]);
                for (k = 1; k < size - 1; k++) {
                    minval = L_MIN(minarray[k], minarray[k + size - 1]);
                    SET_DATA_BYTE(lined, startx + k, minval);
                }
            }
        }
    } else {  /* direction == L_VERT */
        hsize = size / 2;
        nsteps = (h - 2 * hsize) / size;
        for (j = 0; j < w; j++) {
                /* fill buffer with pixels in byte order */
            for (i = 0; i < h; i++) {
                lines = datas + i * wpls;
                buffer[i] = GET_DATA_BYTE(lines, j);
            }

            for (i = 0; i < nsteps; i++) {
                    /* refill the minarray */
                startmin = (i + 1) * size - 1;
                minarray[size - 1] = buffer[startmin];
                for (k = 1; k < size; k++) {
                    minarray[size - 1 - k] =
                        L_MIN(minarray[size - k], buffer[startmin - k]);
                    minarray[size - 1 + k] =
                        L_MIN(minarray[size + k - 2], buffer[startmin + k]);
                }

                    /* compute erosion values */
                starty = hsize + i * size;
                lined = datad + starty * wpld;
                SET_DATA_BYTE(lined, j, minarray[0]);
                SET_DATA_BYTE(lined + (size - 1) * wpld, j,
                        minarray[2 * size - 2]);
                for (k = 1; k < size - 1; k++) {
                    minval = L_MIN(minarray[k], minarray[k + size - 1]);
                    SET_DATA_BYTE(lined + wpld * k, j, minval);
                }
            }
        }
    }

    return;
}

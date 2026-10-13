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
 * \file pixarith.c
 * <pre>
 *
 *      One-image grayscale arithmetic operations (8, 16, 32 bpp)
 *           l_int32     pixAddConstantGray()
 *           l_int32     pixMultConstantGray()
 *
 *      Two-image grayscale arithmetic operations (8, 16, 32 bpp)
 *           PIX        *pixAddGray()
 *           PIX        *pixSubtractGray()
 *           PIX        *pixMultiplyGray()
 *
 *      Grayscale threshold operation (8, 16, 32 bpp)
 *           PIX        *pixThresholdToValue()
 *
 *      Image accumulator arithmetic operations
 *           PIX        *pixInitAccumulate()
 *           PIX        *pixFinalAccumulate()
 *           PIX        *pixFinalAccumulateThreshold()
 *           l_int32     pixAccumulate()
 *           l_int32     pixMultConstAccumulate()
 *
 *      Absolute value of difference
 *           PIX        *pixAbsDifference()
 *
 *      Sum of color images
 *           PIX        *pixAddRGB()
 *
 *      Two-image min and max operations (8 and 16 bpp)
 *           PIX        *pixMinOrMax()
 *
 *      Scale pix for maximum dynamic range
 *           PIX        *pixMaxDynamicRange()
 *           PIX        *pixMaxDynamicRangeRGB()
 *
 *      RGB pixel value scaling
 *           l_uint32    linearScaleRGBVal()
 *           l_uint32    logScaleRGBVal()
 *
 *      Log base2 lookup
 *           l_float32  *makeLogBase2Tab()
 *           l_float32   getLogBase2()
 *
 *      The image accumulator operations are used when you expect
 *      overflow from 8 bits on intermediate results.  For example,
 *      you might want a tophat contrast operator which is
 *         3*I - opening(I,S) - closing(I,S)
 *      To use these operations, first use the init to generate
 *      a 16 bpp image, use the accumulate to add or subtract 8 bpp
 *      images from that, or the multiply constant to multiply
 *      by a small constant (much less than 256 -- we don't want
 *      overflow from the 16 bit images!), and when you're finished
 *      use final to bring the result back to 8 bpp, clipped
 *      if necessary.  There is also a divide function, which
 *      can be used to divide one image by another, scaling the
 *      result for maximum dynamic range, and giving back the
 *      8 bpp result.
 *
 *      A simpler interface to the arithmetic operations is
 *      provided in pixacc.c.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>
#include "allheaders.h"

/*-------------------------------------------------------------*
 *          One-image grayscale arithmetic operations          *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixAddConstantGray()
 *
 * \param[in]    pixs   8, 16 or 32 bpp
 * \param[in]    val    amount to add to each pixel
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) In-place operation.
 *      (2) No clipping for 32 bpp.
 *      (3) For 8 and 16 bpp, if val > 0 the result is clipped
 *          to 0xff and 0xffff, rsp.
 *      (4) For 8 and 16 bpp, if val < 0 the result is clipped to 0.
 * </pre>
 */
l_ok
pixAddConstantGray(PIX      *pixs,
                   l_int32   val)
{
l_int32    i, j, w, h, d, wpl, pval;
l_uint32  *data, *line;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8 && d != 16 && d != 32)
        return ERROR_INT("pixs not 8, 16 or 32 bpp", __func__, 1);

    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        if (d == 8) {
            if (val < 0) {
                for (j = 0; j < w; j++) {
                    pval = GET_DATA_BYTE(line, j);
                    pval = L_MAX(0, pval + val);
                    SET_DATA_BYTE(line, j, pval);
                }
            } else {  /* val >= 0 */
                for (j = 0; j < w; j++) {
                    pval = GET_DATA_BYTE(line, j);
                    pval = L_MIN(255, pval + val);
                    SET_DATA_BYTE(line, j, pval);
                }
            }
        } else if (d == 16) {
            if (val < 0) {
                for (j = 0; j < w; j++) {
                    pval = GET_DATA_TWO_BYTES(line, j);
                    pval = L_MAX(0, pval + val);
                    SET_DATA_TWO_BYTES(line, j, pval);
                }
            } else {  /* val >= 0 */
                for (j = 0; j < w; j++) {
                    pval = GET_DATA_TWO_BYTES(line, j);
                    pval = L_MIN(0xffff, pval + val);
                    SET_DATA_TWO_BYTES(line, j, pval);
                }
            }
        } else {  /* d == 32; no check for overflow (< 0 or > 0xffffffff) */
            for (j = 0; j < w; j++)
                *(line + j) += val;
        }
    }

    return 0;
}


/*!
 * \brief   pixMultConstantGray()
 *
 * \param[in]    pixs   8, 16 or 32 bpp
 * \param[in]    val    >= 0.0; amount to multiply by each pixel
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) In-place operation; val must be >= 0.
 *      (2) No clipping for 32 bpp.
 *      (3) For 8 and 16 bpp, the result is clipped to 0xff and 0xffff, rsp.
 * </pre>
 */
l_ok
pixMultConstantGray(PIX       *pixs,
                    l_float32  val)
{
l_int32    i, j, w, h, d, wpl, pval;
l_uint32   upval;
l_uint32  *data, *line;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8 && d != 16 && d != 32)
        return ERROR_INT("pixs not 8, 16 or 32 bpp", __func__, 1);
    if (val < 0.0)
        return ERROR_INT("val < 0.0", __func__, 1);

    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        if (d == 8) {
            for (j = 0; j < w; j++) {
                pval = GET_DATA_BYTE(line, j);
                pval = (l_int32)(val * pval);
                pval = L_MIN(255, pval);
                SET_DATA_BYTE(line, j, pval);
            }
        } else if (d == 16) {
            for (j = 0; j < w; j++) {
                pval = GET_DATA_TWO_BYTES(line, j);
                pval = (l_int32)(val * pval);
                pval = L_MIN(0xffff, pval);
                SET_DATA_TWO_BYTES(line, j, pval);
            }
        } else {  /* d == 32; no clipping */
            for (j = 0; j < w; j++) {
                upval = *(line + j);
                upval = (l_uint32)(val * upval);
                *(line + j) = upval;
            }
        }
    }

    return 0;
}


/*-------------------------------------------------------------*
 *             Two-image grayscale arithmetic ops              *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixAddGray()
 *
 * \param[in]    pixd    [optional]; this can be null, equal to pixs1, or
 *                       different from pixs1
 * \param[in]    pixs1   can be equal to pixd
 * \param[in]    pixs2
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *      (1) Arithmetic addition of two 8, 16 or 32 bpp images.
 *      (2) For 8 and 16 bpp, we do explicit clipping to 0xff and 0xffff,
 *          respectively.
 *      (3) Alignment is to UL corner.
 *      (4) There are 3 cases.  The result can go to a new dest,
 *          in-place to pixs1, or to an existing input dest:
 *          * pixd == null:   (src1 + src2) --> new pixd
 *          * pixd == pixs1:  (src1 + src2) --> src1  (in-place)
 *          * pixd != pixs1:  (src1 + src2) --> input pixd
 *      (5) pixs2 must be different from both pixd and pixs1.
 * </pre>
 */
PIX *
pixAddGray(PIX  *pixd,
           PIX  *pixs1,
           PIX  *pixs2)
{
l_int32    i, j, d, ws, hs, w, h, wpls, wpld, val, sum;
l_uint32  *datas, *datad, *lines, *lined;

    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, pixd);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, pixd);
    if (pixs2 == pixs1)
        return (PIX *)ERROR_PTR("pixs2 and pixs1 must differ", __func__, pixd);
    if (pixs2 == pixd)
        return (PIX *)ERROR_PTR("pixs2 and pixd must differ", __func__, pixd);
    d = pixGetDepth(pixs1);
    if (d != 8 && d != 16 && d != 32)
        return (PIX *)ERROR_PTR("pix are not 8, 16 or 32 bpp", __func__, pixd);
    if (pixGetDepth(pixs2) != d)
        return (PIX *)ERROR_PTR("depths differ (pixs1, pixs2)", __func__, pixd);
    if (pixd && (pixGetDepth(pixd) != d))
        return (PIX *)ERROR_PTR("depths differ (pixs1, pixd)", __func__, pixd);

    if (!pixSizesEqual(pixs1, pixs2))
        L_WARNING("pixs1 and pixs2 not equal in size\n", __func__);
    if (pixd && !pixSizesEqual(pixs1, pixd))
        L_WARNING("pixs1 and pixd not equal in size\n", __func__);

    if (pixs1 != pixd)
        pixd = pixCopy(pixd, pixs1);

        /* pixd + pixs2 ==> pixd  */
    datas = pixGetData(pixs2);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs2);
    wpld = pixGetWpl(pixd);
    pixGetDimensions(pixs2, &ws, &hs, NULL);
    pixGetDimensions(pixd, &w, &h, NULL);
    w = L_MIN(ws, w);
    h = L_MIN(hs, h);
    for (i = 0; i < h; i++) {
        lined = datad + i * wpld;
        lines = datas + i * wpls;
        if (d == 8) {
            for (j = 0; j < w; j++) {
                sum = GET_DATA_BYTE(lines, j) + GET_DATA_BYTE(lined, j);
                val = L_MIN(sum, 255);
                SET_DATA_BYTE(lined, j, val);
            }
        } else if (d == 16) {
            for (j = 0; j < w; j++) {
                sum = GET_DATA_TWO_BYTES(lines, j)
                    + GET_DATA_TWO_BYTES(lined, j);
                val = L_MIN(sum, 0xffff);
                SET_DATA_TWO_BYTES(lined, j, val);
            }
        } else {   /* d == 32; no clipping */
            for (j = 0; j < w; j++)
                *(lined + j) += *(lines + j);
        }
    }

    return pixd;
}


/*!
 * \brief   pixSubtractGray()
 *
 * \param[in]    pixd     [optional]; this can be null, equal to pixs1, or
 *                        different from pixs1
 * \param[in]    pixs1    can be equal to pixd
 * \param[in]    pixs2
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *      (1) Arithmetic subtraction of two 8, 16 or 32 bpp images.
 *      (2) Source pixs2 is always subtracted from source pixs1.
 *      (3) Do explicit clipping to 0.
 *      (4) Alignment is to UL corner.
 *      (5) There are 3 cases.  The result can go to a new dest,
 *          in-place to pixs1, or to an existing input dest:
 *          (a) pixd == null   (src1 - src2) --> new pixd
 *          (b) pixd == pixs1  (src1 - src2) --> src1  (in-place)
 *          (d) pixd != pixs1  (src1 - src2) --> input pixd
 *      (6) pixs2 must be different from both pixd and pixs1.
 * </pre>
 */
PIX *
pixSubtractGray(PIX  *pixd,
                PIX  *pixs1,
                PIX  *pixs2)
{
l_int32    i, j, w, h, ws, hs, d, wpls, wpld, val, diff;
l_uint32  *datas, *datad, *lines, *lined;

    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, pixd);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, pixd);
    if (pixs2 == pixs1)
        return (PIX *)ERROR_PTR("pixs2 and pixs1 must differ", __func__, pixd);
    if (pixs2 == pixd)
        return (PIX *)ERROR_PTR("pixs2 and pixd must differ", __func__, pixd);
    d = pixGetDepth(pixs1);
    if (d != 8 && d != 16 && d != 32)
        return (PIX *)ERROR_PTR("pix are not 8, 16 or 32 bpp", __func__, pixd);
    if (pixGetDepth(pixs2) != d)
        return (PIX *)ERROR_PTR("depths differ (pixs1, pixs2)", __func__, pixd);
    if (pixd && (pixGetDepth(pixd) != d))
        return (PIX *)ERROR_PTR("depths differ (pixs1, pixd)", __func__, pixd);

    if (!pixSizesEqual(pixs1, pixs2))
        L_WARNING("pixs1 and pixs2 not equal in size\n", __func__);
    if (pixd && !pixSizesEqual(pixs1, pixd))
        L_WARNING("pixs1 and pixd not equal in size\n", __func__);

    if (pixs1 != pixd)
        pixd = pixCopy(pixd, pixs1);

        /* pixd - pixs2 ==> pixd  */
    datas = pixGetData(pixs2);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs2);
    wpld = pixGetWpl(pixd);
    pixGetDimensions(pixs2, &ws, &hs, NULL);
    pixGetDimensions(pixd, &w, &h, NULL);
    w = L_MIN(ws, w);
    h = L_MIN(hs, h);
    for (i = 0; i < h; i++) {
        lined = datad + i * wpld;
        lines = datas + i * wpls;
        if (d == 8) {
            for (j = 0; j < w; j++) {
                diff = GET_DATA_BYTE(lined, j) - GET_DATA_BYTE(lines, j);
                val = L_MAX(diff, 0);
                SET_DATA_BYTE(lined, j, val);
            }
        } else if (d == 16) {
            for (j = 0; j < w; j++) {
                diff = GET_DATA_TWO_BYTES(lined, j)
                       - GET_DATA_TWO_BYTES(lines, j);
                val = L_MAX(diff, 0);
                SET_DATA_TWO_BYTES(lined, j, val);
            }
        } else {  /* d == 32; no clipping */
            for (j = 0; j < w; j++)
                *(lined + j) -= *(lines + j);
        }
    }

    return pixd;
}


/*!
 * \brief   pixMultiplyGray()
 *
 * \param[in]    pixs    32 bpp rgb or 8 bpp gray
 * \param[in]    pixg    8 bpp gray
 * \param[in]    norm    multiplicative factor to avoid overflow; 0 for default
 * \return  pixd, or null on error
 *
 * <pre>
 * Notes:
 *      (1) This function can be used for correcting a scanned image
 *          under non-uniform illumination.  For that application,
 *          %pixs is the scanned image, %pixg is an image whose values
 *          are inversely related to light from a uniform (say, white)
 *          target, and %norm is typically the inverse of the maximum
 *          pixel value in %pixg.
 *      (2) Set norm = 0 to get the default value, which is the inverse
 *          of the max value in %pixg.  This avoids overflow in the product.
 *      (3) For 32 bpp %pixs, all 3 components are multiplied by the
 *          same number.
 *      (4) Alignment is to UL corner.
 * </pre>
 */
PIX *
pixMultiplyGray(PIX        *pixs,
                PIX        *pixg,
                l_float32   norm)
{
l_int32    i, j, w, h, d, ws, hs, ds, wpls, wplg, wpld;
l_int32    rval, gval, bval, rval2, gval2, bval2, vals, valg, val, maxgray;
l_uint32   val32;
l_uint32  *datas, *datag, *datad, *lines, *lineg, *lined;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &ws, &hs, &ds);
    if (ds != 8 && ds != 32)
        return (PIX *)ERROR_PTR("pixs not 8 or 32 bpp", __func__, NULL);
    if (!pixg)
        return (PIX *)ERROR_PTR("pixg not defined", __func__, NULL);
    pixGetDimensions(pixg, &w, &h, &d);
    if (d != 8)
        return (PIX *)ERROR_PTR("pixg not 8 bpp", __func__, NULL);

    if (norm <= 0.0) {
        pixGetExtremeValue(pixg, 1, L_SELECT_MAX, NULL, NULL, NULL, &maxgray);
        norm = (maxgray > 0) ? 1.0f / (l_float32)maxgray : 1.0f;
    }

    if ((pixd = pixCreateTemplate(pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    datas = pixGetData(pixs);
    datag = pixGetData(pixg);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wplg = pixGetWpl(pixg);
    wpld = pixGetWpl(pixd);
    w = L_MIN(ws, w);
    h = L_MIN(hs, h);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lineg = datag + i * wplg;
        lined = datad + i * wpld;
        if (ds == 8) {
            for (j = 0; j < w; j++) {
                vals = GET_DATA_BYTE(lines, j);
                valg = GET_DATA_BYTE(lineg, j);
                val = (l_int32)(vals * valg * norm + 0.5);
                val = L_MIN(255, val);
                SET_DATA_BYTE(lined, j, val);
            }
        } else {  /* ds == 32 */
            for (j = 0; j < w; j++) {
                val32 = *(lines + j);
                extractRGBValues(val32, &rval, &gval, &bval);
                valg = GET_DATA_BYTE(lineg, j);
                rval2 = (l_int32)(rval * valg * norm + 0.5);
                rval2 = L_MIN(255, rval2);
                gval2 = (l_int32)(gval * valg * norm + 0.5);
                gval2 = L_MIN(255, gval2);
                bval2 = (l_int32)(bval * valg * norm + 0.5);
                bval2 = L_MIN(255, bval2);
                composeRGBPixel(rval2, gval2, bval2, lined + j);
            }
        }
    }

    return pixd;
}


/*-------------------------------------------------------------*
 *                Grayscale threshold operation                *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixThresholdToValue()
 *
 * \param[in]    pixd       [optional]; if not null, must be equal to pixs
 * \param[in]    pixs       8, 16, 32 bpp
 * \param[in]    threshval
 * \param[in]    setval
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *    ~ operation can be in-place (pixs == pixd) or to a new pixd
 *    ~ if %setval > %threshval, sets pixels with a value >= threshval to setval
 *    ~ if %setval < %threshval, sets pixels with a value <= threshval to setval
 *    ~ if %setval == %threshval, no-op
 * </pre>
 */
PIX *
pixThresholdToValue(PIX      *pixd,
                    PIX      *pixs,
                    l_int32   threshval,
                    l_int32   setval)
{
l_int32    i, j, w, h, d, wpld, setabove;
l_uint32  *datad, *lined;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    d = pixGetDepth(pixs);
    if (d != 8 && d != 16 && d != 32)
        return (PIX *)ERROR_PTR("pixs not 8, 16 or 32 bpp", __func__, pixd);
    if (pixd && (pixs != pixd))
        return (PIX *)ERROR_PTR("pixd exists and is not pixs", __func__, pixd);
    if (threshval < 0 || setval < 0)
        return (PIX *)ERROR_PTR("threshval & setval not < 0", __func__, pixd);
    if (d == 8 && setval > 255)
        return (PIX *)ERROR_PTR("setval > 255 for 8 bpp", __func__, pixd);
    if (d == 16 && setval > 0xffff)
        return (PIX *)ERROR_PTR("setval > 0xffff for 16 bpp", __func__, pixd);

    if (!pixd)
        pixd = pixCopy(NULL, pixs);
    if (setval == threshval) {
        L_WARNING("setval == threshval; no operation\n", __func__);
        return pixd;
    }

    datad = pixGetData(pixd);
    pixGetDimensions(pixd, &w, &h, NULL);
    wpld = pixGetWpl(pixd);
    if (setval > threshval)
        setabove = TRUE;
    else
        setabove = FALSE;

    for (i = 0; i < h; i++) {
        lined = datad + i * wpld;
        if (setabove == TRUE) {
            if (d == 8) {
                for (j = 0; j < w; j++) {
                    if (GET_DATA_BYTE(lined, j) - threshval >= 0)
                        SET_DATA_BYTE(lined, j, setval);
                }
            } else if (d == 16) {
                for (j = 0; j < w; j++) {
                    if (GET_DATA_TWO_BYTES(lined, j) - threshval >= 0)
                        SET_DATA_TWO_BYTES(lined, j, setval);
                }
            } else {  /* d == 32 */
                for (j = 0; j < w; j++) {
                    if (*(lined + j) >= threshval)
                        *(lined + j) = setval;
                }
            }
        } else { /* set if below or at threshold */
            if (d == 8) {
                for (j = 0; j < w; j++) {
                    if (GET_DATA_BYTE(lined, j) - threshval <= 0)
                        SET_DATA_BYTE(lined, j, setval);
                }
            } else if (d == 16) {
                for (j = 0; j < w; j++) {
                    if (GET_DATA_TWO_BYTES(lined, j) - threshval <= 0)
                        SET_DATA_TWO_BYTES(lined, j, setval);
                }
            } else {  /* d == 32 */
                for (j = 0; j < w; j++) {
                    if (*(lined + j) <= threshval)
                        *(lined + j) = setval;
                }
            }
        }
    }

    return pixd;
}


/*-------------------------------------------------------------*
 *            Image accumulator arithmetic operations          *
 *-------------------------------------------------------------*/
/*!
 * \brief   pixInitAccumulate()
 *
 * \param[in]    w, h      of accumulate array
 * \param[in]    offset    initialize the 32 bpp to have this
 *                         value; not more than 0x40000000
 * \return  pixd   32 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) %offset must be >= 0.
 *      (2) %offset is used so that we can do arithmetic
 *          with negative number results on l_uint32 data; it
 *          prevents the l_uint32 data from going negative.
 *      (3) Because we use l_int32 intermediate data results,
 *          these should never exceed the max of l_int32 (0x7fffffff).
 *          We do not permit the offset to be above 0x40000000,
 *          which is half way between 0 and the max of l_int32.
 *      (4) The same offset should be used for initialization,
 *          multiplication by a constant, and final extraction!
 *      (5) If you're only adding positive values, %offset can be 0.
 * </pre>
 */
PIX *
pixInitAccumulate(l_int32   w,
                  l_int32   h,
                  l_uint32  offset)
{
PIX  *pixd;

    if ((pixd = pixCreate(w, h, 32)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    if (offset > 0x40000000)
        offset = 0x40000000;
    pixSetAllArbitrary(pixd, offset);
    return pixd;
}


/*!
 * \brief   pixFinalAccumulate()
 *
 * \param[in]    pixs     32 bpp
 * \param[in]    offset   same as used for initialization
 * \param[in]    depth    8, 16 or 32 bpp, of destination
 * \return  pixd   8, 16 or 32 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) %offset must be >= 0 and should not exceed 0x40000000.
 *      (2) %offset is subtracted from the src 32 bpp image
 *      (3) For 8 bpp dest, the result is clipped to [0, 0xff]
 *      (4) For 16 bpp dest, the result is clipped to [0, 0xffff]
 * </pre>
 */
PIX *
pixFinalAccumulate(PIX      *pixs,
                   l_uint32  offset,
                   l_int32   depth)
{
l_int32    i, j, w, h, wpls, wpld, val;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (depth != 8 && depth != 16 && depth != 32)
        return (PIX *)ERROR_PTR("dest depth not 8, 16, 32 bpp", __func__, NULL);
    if (offset > 0x40000000)
        offset = 0x40000000;

    pixGetDimensions(pixs, &w, &h, NULL);
    if ((pixd = pixCreate(w, h, depth)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);  /* but how did pixs get it initially? */
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);
    if (depth == 8) {
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {
                val = lines[j] - offset;
                val = L_MAX(0, val);
                val = L_MIN(255, val);
                SET_DATA_BYTE(lined, j, (l_uint8)val);
            }
        }
    } else if (depth == 16) {
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {
                val = lines[j] - offset;
                val = L_MAX(0, val);
                val = L_MIN(0xffff, val);
                SET_DATA_TWO_BYTES(lined, j, (l_uint16)val);
            }
        }
    } else {  /* depth == 32 */
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++)
                lined[j] = lines[j] - offset;
        }
    }

    return pixd;
}


/*!
 * \brief   pixFinalAccumulateThreshold()
 *
 * \param[in]    pixs        32 bpp
 * \param[in]    offset      same as used for initialization
 * \param[in]    threshold   values less than this are set in the destination
 * \return  pixd   1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) %offset must be >= 0 and should not exceed 0x40000000.
 *      (2) %offset is subtracted from the src 32 bpp image
 * </pre>
 */
PIX *
pixFinalAccumulateThreshold(PIX      *pixs,
                            l_uint32  offset,
                            l_uint32  threshold)
{
l_int32    i, j, w, h, wpls, wpld, val;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (offset > 0x40000000)
        offset = 0x40000000;

    pixGetDimensions(pixs, &w, &h, NULL);
    if ((pixd = pixCreate(w, h, 1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);  /* but how did pixs get it initially? */
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            val = lines[j] - offset;
            if (val >= threshold) {
                SET_DATA_BIT(lined, j);
            }
        }
    }

    return pixd;
}


/*!
 * \brief   pixAccumulate()
 *
 * \param[in]    pixd    32 bpp
 * \param[in]    pixs    1, 8, 16 or 32 bpp
 * \param[in]    op      L_ARITH_ADD or L_ARITH_SUBTRACT
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This adds or subtracts each pixs value from pixd.
 *      (2) This clips to the minimum of pixs and pixd, so they
 *          do not need to be the same size.
 *      (3) The alignment is to the origin [UL corner] of pixs & pixd.
 * </pre>
 */
l_ok
pixAccumulate(PIX     *pixd,
              PIX     *pixs,
              l_int32  op)
{
l_int32    i, j, w, h, d, wd, hd, wpls, wpld;
l_uint32  *datas, *datad, *lines, *lined;


    if (!pixd || (pixGetDepth(pixd) != 32))
        return ERROR_INT("pixd not defined or not 32 bpp", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    d = pixGetDepth(pixs);
    if (d != 1 && d != 8 && d != 16 && d != 32)
        return ERROR_INT("pixs not 1, 8, 16 or 32 bpp", __func__, 1);
    if (op != L_ARITH_ADD && op != L_ARITH_SUBTRACT)
        return ERROR_INT("op must be in {L_ARITH_ADD, L_ARITH_SUBTRACT}",
                         __func__, 1);

    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);
    pixGetDimensions(pixs, &w, &h, NULL);
    pixGetDimensions(pixd, &wd, &hd, NULL);
    w = L_MIN(w, wd);
    h = L_MIN(h, hd);
    if (d == 1) {
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            if (op == L_ARITH_ADD) {
                for (j = 0; j < w; j++)
                    lined[j] += GET_DATA_BIT(lines, j);
            } else {  /* op == L_ARITH_SUBTRACT */
                for (j = 0; j < w; j++)
                    lined[j] -= GET_DATA_BIT(lines, j);
            }
        }
    } else if (d == 8) {
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            if (op == L_ARITH_ADD) {
                for (j = 0; j < w; j++)
                    lined[j] += GET_DATA_BYTE(lines, j);
            } else {  /* op == L_ARITH_SUBTRACT */
                for (j = 0; j < w; j++)
                    lined[j] -= GET_DATA_BYTE(lines, j);
            }
        }
    } else if (d == 16) {
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            if (op == L_ARITH_ADD) {
                for (j = 0; j < w; j++)
                    lined[j] += GET_DATA_TWO_BYTES(lines, j);
            } else {  /* op == L_ARITH_SUBTRACT */
                for (j = 0; j < w; j++)
                    lined[j] -= GET_DATA_TWO_BYTES(lines, j);
            }
        }
    } else {  /* d == 32 */
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            if (op == L_ARITH_ADD) {
                for (j = 0; j < w; j++)
                    lined[j] += lines[j];
            } else {  /* op == L_ARITH_SUBTRACT */
                for (j = 0; j < w; j++)
                    lined[j] -= lines[j];
            }
        }
    }

    return 0;
}


/*!
 * \brief   pixMultConstAccumulate()
 *
 * \param[in]    pixs      32 bpp
 * \param[in]    factor
 * \param[in]    offset    same as used for initialization
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) %offset must be >= 0 and should not exceed 0x40000000.
 *      (2) This multiplies each pixel, relative to offset, by %factor.
 *      (3) The result is returned with %offset back in place.
 * </pre>
 */
l_ok
pixMultConstAccumulate(PIX       *pixs,
                       l_float32  factor,
                       l_uint32   offset)
{
l_int32    i, j, w, h, wpl, val;
l_uint32  *data, *line;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs not 32 bpp", __func__, 1);
    if (offset > 0x40000000)
        offset = 0x40000000;

    pixGetDimensions(pixs, &w, &h, NULL);
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        for (j = 0; j < w; j++) {
            val = line[j] - offset;
            val = (l_int32)(val * factor);
            val += offset;
            line[j] = (l_uint32)val;
        }
    }

    return 0;
}


/*-----------------------------------------------------------------------*
 *                      Absolute value of difference                     *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixAbsDifference()
 *
 * \param[in]    pixs1, pixs2    both either 8 or 16 bpp gray, or 32 bpp RGB
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The depth of pixs1 and pixs2 must be equal.
 *      (2) Clips computation to the min size, aligning the UL corners
 *      (3) For 8 and 16 bpp, assumes one gray component.
 *      (4) For 32 bpp, assumes 3 color components, and ignores the
 *          LSB of each word (the alpha channel)
 *      (5) Computes the absolute value of the difference between
 *          each component value.
 * </pre>
 */
PIX *
pixAbsDifference(PIX  *pixs1,
                 PIX  *pixs2)
{
l_int32    i, j, w, h, w2, h2, d, wpls1, wpls2, wpld, val1, val2, diff;
l_int32    rval1, gval1, bval1, rval2, gval2, bval2, rdiff, gdiff, bdiff;
l_uint32  *datas1, *datas2, *datad, *lines1, *lines2, *lined;
PIX       *pixd;

    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, NULL);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, NULL);
    d = pixGetDepth(pixs1);
    if (d != pixGetDepth(pixs2))
        return (PIX *)ERROR_PTR("src1 and src2 depths unequal", __func__, NULL);
    if (d != 8 && d != 16 && d != 32)
        return (PIX *)ERROR_PTR("depths not in {8, 16, 32}", __func__, NULL);

    pixGetDimensions(pixs1, &w, &h, NULL);
    pixGetDimensions(pixs2, &w2, &h2, NULL);
    w = L_MIN(w, w2);
    h = L_MIN(h, h2);
    if ((pixd = pixCreate(w, h, d)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs1);
    datas1 = pixGetData(pixs1);
    datas2 = pixGetData(pixs2);
    datad = pixGetData(pixd);
    wpls1 = pixGetWpl(pixs1);
    wpls2 = pixGetWpl(pixs2);
    wpld = pixGetWpl(pixd);
    if (d == 8) {
        for (i = 0; i < h; i++) {
            lines1 = datas1 + i * wpls1;
            lines2 = datas2 + i * wpls2;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {
                val1 = GET_DATA_BYTE(lines1, j);
                val2 = GET_DATA_BYTE(lines2, j);
                diff = L_ABS(val1 - val2);
                SET_DATA_BYTE(lined, j, diff);
            }
        }
    } else if (d == 16) {
        for (i = 0; i < h; i++) {
            lines1 = datas1 + i * wpls1;
            lines2 = datas2 + i * wpls2;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {
                val1 = GET_DATA_TWO_BYTES(lines1, j);
                val2 = GET_DATA_TWO_BYTES(lines2, j);
                diff = L_ABS(val1 - val2);
                SET_DATA_TWO_BYTES(lined, j, diff);
            }
        }
    } else {  /* d == 32 */
        for (i = 0; i < h; i++) {
            lines1 = datas1 + i * wpls1;
            lines2 = datas2 + i * wpls2;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {
                extractRGBValues(lines1[j], &rval1, &gval1, &bval1);
                extractRGBValues(lines2[j], &rval2, &gval2, &bval2);
                rdiff = L_ABS(rval1 - rval2);
                gdiff = L_ABS(gval1 - gval2);
                bdiff = L_ABS(bval1 - bval2);
                composeRGBPixel(rdiff, gdiff, bdiff, lined + j);
            }
        }
    }

    return pixd;
}


/*-----------------------------------------------------------------------*
 *                           Sum of color images                         *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixAddRGB()
 *
 * \param[in]    pixs1, pixs2    32 bpp RGB, or colormapped
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Clips computation to the minimum size, aligning the UL corners.
 *      (2) Removes any colormap to RGB, and ignores the LSB of each
 *          pixel word (the alpha channel).
 *      (3) Adds each component value, pixelwise, clipping to 255.
 *      (4) This is useful to combine two images where most of the
 *          pixels are essentially black, such as in pixPerceptualDiff().
 * </pre>
 */
PIX *
pixAddRGB(PIX  *pixs1,
          PIX  *pixs2)
{
l_int32    i, j, w, h, d, w2, h2, d2, wplc1, wplc2, wpld;
l_int32    rval1, gval1, bval1, rval2, gval2, bval2, rval, gval, bval;
l_uint32  *datac1, *datac2, *datad, *linec1, *linec2, *lined;
PIX       *pixc1, *pixc2, *pixd;

    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, NULL);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, NULL);
    pixGetDimensions(pixs1, &w, &h, &d);
    pixGetDimensions(pixs2, &w2, &h2, &d2);
    if (!pixGetColormap(pixs1) && d != 32)
        return (PIX *)ERROR_PTR("pixs1 not cmapped or rgb", __func__, NULL);
    if (!pixGetColormap(pixs2) && d2 != 32)
        return (PIX *)ERROR_PTR("pixs2 not cmapped or rgb", __func__, NULL);
    if (pixGetColormap(pixs1))
        pixc1 = pixRemoveColormap(pixs1, REMOVE_CMAP_TO_FULL_COLOR);
    else
        pixc1 = pixClone(pixs1);
    if (pixGetColormap(pixs2))
        pixc2 = pixRemoveColormap(pixs2, REMOVE_CMAP_TO_FULL_COLOR);
    else
        pixc2 = pixClone(pixs2);

    w = L_MIN(w, w2);
    h = L_MIN(h, h2);
    pixd = pixCreate(w, h, 32);
    pixCopyResolution(pixd, pixs1);
    datac1 = pixGetData(pixc1);
    datac2 = pixGetData(pixc2);
    datad = pixGetData(pixd);
    wplc1 = pixGetWpl(pixc1);
    wplc2 = pixGetWpl(pixc2);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        linec1 = datac1 + i * wplc1;
        linec2 = datac2 + i * wplc2;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            extractRGBValues(linec1[j], &rval1, &gval1, &bval1);
            extractRGBValues(linec2[j], &rval2, &gval2, &bval2);
            rval = L_MIN(255, rval1 + rval2);
            gval = L_MIN(255, gval1 + gval2);
            bval = L_MIN(255, bval1 + bval2);
            composeRGBPixel(rval, gval, bval, lined + j);
        }
    }

    pixDestroy(&pixc1);
    pixDestroy(&pixc2);
    return pixd;
}


/*-----------------------------------------------------------------------*
 *             Two-image min and max operations (8 and 16 bpp)           *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixMinOrMax()
 *
 * \param[in]    pixd     [optional] destination: this can be null,
 *                        equal to pixs1, or different from pixs1
 * \param[in]    pixs1    can be equal to pixd
 * \param[in]    pixs2
 * \param[in]    type     L_CHOOSE_MIN, L_CHOOSE_MAX
 * \return  pixd always
 *
 * <pre>
 * Notes:
 *      (1) This gives the min or max of two images, component-wise.
 *      (2) The depth can be 8 or 16 bpp for 1 component, and 32 bpp
 *          for a 3 component image.  For 32 bpp, ignore the LSB
 *          of each word (the alpha channel)
 *      (3) There are 3 cases:
 *          ~  if pixd == null,   MinOrMax(src1, src2) --> new pixd
 *          ~  if pixd == pixs1,  MinOrMax(src1, src2) --> src1  (in-place)
 *          ~  if pixd != pixs1,  MinOrMax(src1, src2) --> input pixd
 * </pre>
 */
PIX *
pixMinOrMax(PIX     *pixd,
            PIX     *pixs1,
            PIX     *pixs2,
            l_int32  type)
{
l_int32    d, ws, hs, w, h, wpls, wpld, i, j, vals, vald, val;
l_int32    rval1, gval1, bval1, rval2, gval2, bval2, rval, gval, bval;
l_uint32  *datas, *datad, *lines, *lined;

    if (!pixs1)
        return (PIX *)ERROR_PTR("pixs1 not defined", __func__, pixd);
    if (!pixs2)
        return (PIX *)ERROR_PTR("pixs2 not defined", __func__, pixd);
    if (pixs1 == pixs2)
        return (PIX *)ERROR_PTR("pixs1 and pixs2 must differ", __func__, pixd);
    if (type != L_CHOOSE_MIN && type != L_CHOOSE_MAX)
        return (PIX *)ERROR_PTR("invalid type", __func__, pixd);
    d = pixGetDepth(pixs1);
    if (pixGetDepth(pixs2) != d)
        return (PIX *)ERROR_PTR("depths unequal", __func__, pixd);
    if (d != 8 && d != 16 && d != 32)
        return (PIX *)ERROR_PTR("depth not 8, 16 or 32 bpp", __func__, pixd);

    if (pixs1 != pixd)
        pixd = pixCopy(pixd, pixs1);

    pixGetDimensions(pixs2, &ws, &hs, NULL);
    pixGetDimensions(pixd, &w, &h, NULL);
    w = L_MIN(w, ws);
    h = L_MIN(h, hs);
    datas = pixGetData(pixs2);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs2);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        if (d == 8) {
            for (j = 0; j < w; j++) {
                vals = GET_DATA_BYTE(lines, j);
                vald = GET_DATA_BYTE(lined, j);
                if (type == L_CHOOSE_MIN)
                    val = L_MIN(vals, vald);
                else  /* type == L_CHOOSE_MAX */
                    val = L_MAX(vals, vald);
                SET_DATA_BYTE(lined, j, val);
            }
        } else if (d == 16) {
            for (j = 0; j < w; j++) {
                vals = GET_DATA_TWO_BYTES(lines, j);
                vald = GET_DATA_TWO_BYTES(lined, j);
                if (type == L_CHOOSE_MIN)
                    val = L_MIN(vals, vald);
                else  /* type == L_CHOOSE_MAX */
                    val = L_MAX(vals, vald);
                SET_DATA_TWO_BYTES(lined, j, val);
            }
        } else {  /* d == 32 */
            for (j = 0; j < w; j++) {
                extractRGBValues(lines[j], &rval1, &gval1, &bval1);
                extractRGBValues(lined[j], &rval2, &gval2, &bval2);
                if (type == L_CHOOSE_MIN) {
                    rval = L_MIN(rval1, rval2);
                    gval = L_MIN(gval1, gval2);
                    bval = L_MIN(bval1, bval2);
                } else {  /* type == L_CHOOSE_MAX */
                    rval = L_MAX(rval1, rval2);
                    gval = L_MAX(gval1, gval2);
                    bval = L_MAX(bval1, bval2);
                }
                composeRGBPixel(rval, gval, bval, lined + j);
            }
        }
    }

    return pixd;
}


/*-----------------------------------------------------------------------*
 *                    Scale for maximum dynamic range                    *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixMaxDynamicRange()
 *
 * \param[in]    pixs    4, 8, 16 or 32 bpp source
 * \param[in]    type    L_LINEAR_SCALE or L_LOG_SCALE
 * \return  pixd    8 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Scales pixel values to fit maximally within the dest 8 bpp pixd
 *      (2) Assumes the source 'pixels' are a 1-component scalar.  For
 *          a 32 bpp source, each pixel is treated as a single number --
 *          not as a 3-component rgb pixel value.
 *      (3) Uses a LUT for log scaling.
 * </pre>
 */
PIX *
pixMaxDynamicRange(PIX     *pixs,
                   l_int32  type)
{
l_uint8     dval;
l_int32     i, j, w, h, d, wpls, wpld, max;
l_uint32   *datas, *datad;
l_uint32    word, sval;
l_uint32   *lines, *lined;
l_float32   factor;
l_float32  *tab;
PIX        *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 4 && d != 8 && d != 16 && d != 32)
        return (PIX *)ERROR_PTR("pixs not in {4,8,16,32} bpp", __func__, NULL);
    if (type != L_LINEAR_SCALE && type != L_LOG_SCALE)
        return (PIX *)ERROR_PTR("invalid type", __func__, NULL);

    if ((pixd = pixCreate(w, h, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);

        /* Get max */
    max = 0;
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        for (j = 0; j < wpls; j++) {
            word = *(lines + j);
            if (d == 4) {
                max = L_MAX(max, word >> 28);
                max = L_MAX(max, (word >> 24) & 0xf);
                max = L_MAX(max, (word >> 20) & 0xf);
                max = L_MAX(max, (word >> 16) & 0xf);
                max = L_MAX(max, (word >> 12) & 0xf);
                max = L_MAX(max, (word >> 8) & 0xf);
                max = L_MAX(max, (word >> 4) & 0xf);
                max = L_MAX(max, word & 0xf);
            } else if (d == 8) {
                max = L_MAX(max, word >> 24);
                max = L_MAX(max, (word >> 16) & 0xff);
                max = L_MAX(max, (word >> 8) & 0xff);
                max = L_MAX(max, word & 0xff);
            } else if (d == 16) {
                max = L_MAX(max, word >> 16);
                max = L_MAX(max, word & 0xffff);
            } else {  /* d == 32 (rgb) */
                max = L_MAX(max, word);
            }
        }
    }

        /* Map to the full dynamic range */
    if (d == 4) {
        if (type == L_LINEAR_SCALE) {
            factor = 255.f / (l_float32)max;
            for (i = 0; i < h; i++) {
                lines = datas + i * wpls;
                lined = datad + i * wpld;
                for (j = 0; j < w; j++) {
                    sval = GET_DATA_QBIT(lines, j);
                    dval = (l_uint8)(factor * (l_float32)sval + 0.5);
                    SET_DATA_QBIT(lined, j, dval);
                }
            }
        } else {  /* type == L_LOG_SCALE) */
            tab = makeLogBase2Tab();
            factor = 255.f / getLogBase2(max, tab);
            for (i = 0; i < h; i++) {
                lines = datas + i * wpls;
                lined = datad + i * wpld;
                for (j = 0; j < w; j++) {
                    sval = GET_DATA_QBIT(lines, j);
                    dval = (l_uint8)(factor * getLogBase2(sval, tab) + 0.5);
                    SET_DATA_BYTE(lined, j, dval);
                }
            }
            LEPT_FREE(tab);
        }
    } else if (d == 8) {
        if (type == L_LINEAR_SCALE) {
            factor = 255.f / (l_float32)max;
            for (i = 0; i < h; i++) {
                lines = datas + i * wpls;
                lined = datad + i * wpld;
                for (j = 0; j < w; j++) {
                    sval = GET_DATA_BYTE(lines, j);
                    dval = (l_uint8)(factor * (l_float32)sval + 0.5);
                    SET_DATA_BYTE(lined, j, dval);
                }
            }
        } else {  /* type == L_LOG_SCALE) */
            tab = makeLogBase2Tab();
            factor = 255.f / getLogBase2(max, tab);
            for (i = 0; i < h; i++) {
                lines = datas + i * wpls;
                lined = datad + i * wpld;
                for (j = 0; j < w; j++) {
                    sval = GET_DATA_BYTE(lines, j);
                    dval = (l_uint8)(factor * getLogBase2(sval, tab) + 0.5);
                    SET_DATA_BYTE(lined, j, dval);
                }
            }
            LEPT_FREE(tab);
        }
    } else if (d == 16) {
        if (type == L_LINEAR_SCALE) {
            factor = 255.f / (l_float32)max;
            for (i = 0; i < h; i++) {
                lines = datas + i * wpls;
                lined = datad + i * wpld;
                for (j = 0; j < w; j++) {
                    sval = GET_DATA_TWO_BYTES(lines, j);
                    dval = (l_uint8)(factor * (l_float32)sval + 0.5);
                    SET_DATA_BYTE(lined, j, dval);
                }
            }
        } else {  /* type == L_LOG_SCALE) */
            tab = makeLogBase2Tab();
            factor = 255.f / getLogBase2(max, tab);
            for (i = 0; i < h; i++) {
                lines = datas + i * wpls;
                lined = datad + i * wpld;
                for (j = 0; j < w; j++) {
                    sval = GET_DATA_TWO_BYTES(lines, j);
                    dval = (l_uint8)(factor * getLogBase2(sval, tab) + 0.5);
                    SET_DATA_BYTE(lined, j, dval);
                }
            }
            LEPT_FREE(tab);
        }
    } else {  /* d == 32 */
        if (type == L_LINEAR_SCALE) {
            factor = 255.f / (l_float32)max;
            for (i = 0; i < h; i++) {
                lines = datas + i * wpls;
                lined = datad + i * wpld;
                for (j = 0; j < w; j++) {
                    sval = lines[j];
                    dval = (l_uint8)(factor * (l_float32)sval + 0.5);
                    SET_DATA_BYTE(lined, j, dval);
                }
            }
        } else {  /* type == L_LOG_SCALE) */
            tab = makeLogBase2Tab();
            factor = 255.f / getLogBase2(max, tab);
            for (i = 0; i < h; i++) {
                lines = datas + i * wpls;
                lined = datad + i * wpld;
                for (j = 0; j < w; j++) {
                    sval = lines[j];
                    dval = (l_uint8)(factor * getLogBase2(sval, tab) + 0.5);
                    SET_DATA_BYTE(lined, j, dval);
                }
            }
            LEPT_FREE(tab);
        }
    }

    return pixd;
}


/*!
 * \brief   pixMaxDynamicRangeRGB()
 *
 * \param[in]    pixs    32 bpp rgb source
 * \param[in]    type    L_LINEAR_SCALE or L_LOG_SCALE
 * \return  pixd   32 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Scales pixel values to fit maximally within a 32 bpp dest pixd
 *      (2) All color components are scaled with the same factor, based
 *          on the maximum r, g or b component in the image.  This should
 *          not be used if the 32-bit value is a single number (e.g., a
 *          count in a histogram generated by pixMakeHistoHS()).
 *      (3) Uses a LUT for log scaling.
 * </pre>
 */
PIX *
pixMaxDynamicRangeRGB(PIX     *pixs,
                      l_int32  type)
{
l_int32     i, j, w, h, wpls, wpld, max;
l_uint32    sval, dval, word;
l_uint32   *datas, *datad;
l_uint32   *lines, *lined;
l_float32   factor;
l_float32  *tab;
PIX        *pixd;

    if (!pixs || pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs undefined or not 32 bpp", __func__, NULL);
    if (type != L_LINEAR_SCALE && type != L_LOG_SCALE)
        return (PIX *)ERROR_PTR("invalid type", __func__, NULL);

        /* Get max */
    pixd = pixCreateTemplate(pixs);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);
    pixGetDimensions(pixs, &w, &h, NULL);
    max = 0;
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        for (j = 0; j < wpls; j++) {
            word = lines[j];
            max = L_MAX(max, word >> 24);
            max = L_MAX(max, (word >> 16) & 0xff);
            max = L_MAX(max, (word >> 8) & 0xff);
        }
    }
    if (max == 0) {
        L_WARNING("max = 0; setting to 1\n", __func__);
        max = 1;
    }

        /* Map to the full dynamic range */
    if (type == L_LINEAR_SCALE) {
        factor = 255.f / (l_float32)max;
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {
                sval = lines[j];
                dval = linearScaleRGBVal(sval, factor);
                lined[j] = dval;
            }
        }
    } else {  /* type == L_LOG_SCALE) */
        tab = makeLogBase2Tab();
        factor = 255.f / getLogBase2(max, tab);
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {
                sval = lines[j];
                dval = logScaleRGBVal(sval, tab, factor);
                lined[j] = dval;
            }
        }
        LEPT_FREE(tab);
    }

    return pixd;
}


/*-----------------------------------------------------------------------*
 *                         RGB pixel value scaling                       *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   linearScaleRGBVal()
 *
 * \param[in]    sval     32-bit rgb pixel value
 * \param[in]    factor   multiplication factor on each component
 * \return  dval  linearly scaled version of %sval
 *
 * <pre>
 * Notes:
 *      (1) %factor must be chosen to be not greater than (255 / maxcomp),
 *          where maxcomp is the maximum value of the pixel components.
 *          Otherwise, the product will overflow a uint8.  In use, factor
 *          is the same for all pixels in a pix.
 *      (2) No scaling is performed on the transparency ("A") component.
 * </pre>
 */
l_uint32
linearScaleRGBVal(l_uint32   sval,
                  l_float32  factor)
{
l_uint32  dval;

    dval = ((l_uint8)(factor * (sval >> 24) + 0.5f) << 24) |
           ((l_uint8)(factor * ((sval >> 16) & 0xff) + 0.5f) << 16) |
           ((l_uint8)(factor * ((sval >> 8) & 0xff) + 0.5f) << 8) |
           (sval & 0xff);
    return dval;
}


/*!
 * \brief   logScaleRGBVal()
 *
 * \param[in]    sval     32-bit rgb pixel value
 * \param[in]    tab      256 entry log-base-2 table
 * \param[in]    factor   multiplication factor on each component
 * \return  dval  log scaled version of %sval
 *
 * <pre>
 * Notes:
 *      (1) %tab is made with makeLogBase2Tab().
 *      (2) %factor must be chosen to be not greater than
 *          255.0 / log[base2](maxcomp), where maxcomp is the maximum
 *          value of the pixel components.  Otherwise, the product
 *          will overflow a uint8.  In use, factor is the same for
 *          all pixels in a pix.
 *      (3) No scaling is performed on the transparency ("A") component.
 * </pre>
 */
l_uint32
logScaleRGBVal(l_uint32    sval,
               l_float32  *tab,
               l_float32   factor)
{
l_uint32  dval;

    dval = ((l_uint8)(factor * getLogBase2(sval >> 24, tab) + 0.5f) << 24) |
           ((l_uint8)(factor * getLogBase2(((sval >> 16) & 0xff), tab) + 0.5f)
                     << 16) |
           ((l_uint8)(factor * getLogBase2(((sval >> 8) & 0xff), tab) + 0.5f)
                     << 8) |
           (sval & 0xff);
    return dval;
}


/*-----------------------------------------------------------------------*
 *                            Log base2 lookup                           *
 *-----------------------------------------------------------------------*/
/*
 * \brief   makeLogBase2Tab()
 *
 * \return  tab   table giving the log[base2] of values from 1 to 255
 */
l_float32 *
makeLogBase2Tab(void)
{
l_int32     i;
l_float32   log2;
l_float32  *tab;

    if ((tab = (l_float32 *)LEPT_CALLOC(256, sizeof(l_float32))) == NULL)
        return (l_float32 *)ERROR_PTR("tab not made", __func__, NULL);

    log2 = (l_float32)log((l_float32)2);
    for (i = 0; i < 256; i++)
        tab[i] = (l_float32)log((l_float32)i) / log2;

    return tab;
}


/*
 * \brief   getLogBase2()
 *
 * \param[in]    val       in range [0 ... 255]
 * \param[in]    logtab    256-entry table of logs
 * \return       logval    log[base2] of %val, or 0 on error
 */
l_float32
getLogBase2(l_int32     val,
            l_float32  *logtab)
{
    if (!logtab)
        return ERROR_INT("logtab not defined", __func__, 0);

    if (val < 0x100)
        return logtab[val];
    else if (val < 0x10000)
        return 8.0f + logtab[val >> 8];
    else if (val < 0x1000000)
        return 16.0f + logtab[val >> 16];
    else
        return 24.0f + logtab[val >> 24];
}

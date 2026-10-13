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
 * \file rotateorth.c
 * <pre>
 *
 *      Top-level rotation by multiples of 90 degrees
 *            PIX             *pixRotateOrth()
 *
 *      180-degree rotation
 *            PIX             *pixRotate180()
 *
 *      90-degree rotation (both directions)
 *            PIX             *pixRotate90()
 *
 *      Left-right flip
 *            PIX             *pixFlipLR()
 *
 *      Top-bottom flip
 *            PIX             *pixFlipTB()
 *
 *      Byte reverse tables
 *            static l_uint8  *makeReverseByteTab1()
 *            static l_uint8  *makeReverseByteTab2()
 *            static l_uint8  *makeReverseByteTab4()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

static l_uint8 *makeReverseByteTab1(void);
static l_uint8 *makeReverseByteTab2(void);
static l_uint8 *makeReverseByteTab4(void);

/*------------------------------------------------------------------*
 *           Top-level rotation by multiples of 90 degrees          *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixRotateOrth()
 *
 * \param[in]    pixs       all depths
 * \param[in]    quads      0-3; number of 90 degree cw rotations
 * \return  pixd, or NULL on error
 */
PIX *
pixRotateOrth(PIX     *pixs,
              l_int32  quads)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (quads < 0 || quads > 3)
        return (PIX *)ERROR_PTR("quads not in {0,1,2,3}", __func__, NULL);

    if (quads == 0)
        return pixCopy(NULL, pixs);
    else if (quads == 1)
        return pixRotate90(pixs, 1);
    else if (quads == 2)
        return pixRotate180(NULL, pixs);
    else /* quads == 3 */
        return pixRotate90(pixs, -1);
}


/*------------------------------------------------------------------*
 *                          180 degree rotation                     *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixRotate180()
 *
 * \param[in]    pixd    [optional]; can be null, equal to pixs,
 *                       or different from pixs
 * \param[in]    pixs    all depths
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does a 180 rotation of the image about the center,
 *          which is equivalent to a left-right flip about a vertical
 *          line through the image center, followed by a top-bottom
 *          flip about a horizontal line through the image center.
 *      (2) There are 3 cases for input:
 *          (a) pixd == null (creates a new pixd)
 *          (b) pixd == pixs (in-place operation)
 *          (c) pixd != pixs (existing pixd)
 *      (3) For clarity, use these three patterns, respectively:
 *          (a) pixd = pixRotate180(NULL, pixs);
 *          (b) pixRotate180(pixs, pixs);
 *          (c) pixRotate180(pixd, pixs);
 * </pre>
 */
PIX *
pixRotate180(PIX  *pixd,
             PIX  *pixs)
{
l_int32  d;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    d = pixGetDepth(pixs);
    if (d != 1 && d != 2 && d != 4 && d != 8 && d != 16 && d != 32)
        return (PIX *)ERROR_PTR("pixs not in {1,2,4,8,16,32} bpp",
                                __func__, NULL);

        /* Prepare pixd for in-place operation */
    if ((pixd = pixCopy(pixd, pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);

    pixFlipLR(pixd, pixd);
    pixFlipTB(pixd, pixd);
    return pixd;
}


/*------------------------------------------------------------------*
 *                           90 degree rotation                     *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixRotate90()
 *
 * \param[in]    pixs         all depths
 * \param[in]    direction    clockwise = 1, counterclockwise = -1
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does a 90 degree rotation of the image about the center,
 *          either cw or ccw, returning a new pix.
 *      (2) The direction must be either 1 (cw) or -1 (ccw).
 * </pre>
 */
PIX *
pixRotate90(PIX     *pixs,
            l_int32  direction)
{
l_int32    wd, hd, d, wpls, wpld;
l_int32    i, j, k, m, iend, nswords;
l_uint32   val, word;
l_uint32  *lines, *datas, *lined, *datad;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &hd, &wd, &d);  /* note: reversed */
    if (d != 1 && d != 2 && d != 4 && d != 8 && d != 16 && d != 32)
        return (PIX *)ERROR_PTR("pixs not in {1,2,4,8,16,32} bpp",
                                __func__, NULL);
    if (direction != 1 && direction != -1)
        return (PIX *)ERROR_PTR("invalid direction", __func__, NULL);

    if ((pixd = pixCreate(wd, hd, d)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyColormap(pixd, pixs);
    pixCopyResolution(pixd, pixs);
    pixCopyInputFormat(pixd, pixs);
    pixCopySpp(pixd, pixs);

    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    if (direction == 1) {  /* clockwise */
        switch (d)
        {
            case 32:
                for (i = 0; i < hd; i++) {
                    lined = datad + i * wpld;
                    lines = datas + (wd - 1) * wpls;
                    for (j = 0; j < wd; j++) {
                        lined[j] = lines[i];
                        lines -= wpls;
                    }
                }
                break;
            case 16:
                for (i = 0; i < hd; i++) {
                    lined = datad + i * wpld;
                    lines = datas + (wd - 1) * wpls;
                    for (j = 0; j < wd; j++) {
                        if ((val = GET_DATA_TWO_BYTES(lines, i)))
                            SET_DATA_TWO_BYTES(lined, j, val);
                        lines -= wpls;
                    }
                }
                break;
            case 8:
                for (i = 0; i < hd; i++) {
                    lined = datad + i * wpld;
                    lines = datas + (wd - 1) * wpls;
                    for (j = 0; j < wd; j++) {
                        if ((val = GET_DATA_BYTE(lines, i)))
                            SET_DATA_BYTE(lined, j, val);
                        lines -= wpls;
                    }
                }
                break;
            case 4:
                for (i = 0; i < hd; i++) {
                    lined = datad + i * wpld;
                    lines = datas + (wd - 1) * wpls;
                    for (j = 0; j < wd; j++) {
                        if ((val = GET_DATA_QBIT(lines, i)))
                            SET_DATA_QBIT(lined, j, val);
                        lines -= wpls;
                    }
                }
                break;
            case 2:
                for (i = 0; i < hd; i++) {
                    lined = datad + i * wpld;
                    lines = datas + (wd - 1) * wpls;
                    for (j = 0; j < wd; j++) {
                        if ((val = GET_DATA_DIBIT(lines, i)))
                            SET_DATA_DIBIT(lined, j, val);
                        lines -= wpls;
                    }
                }
                break;
            case 1:
                nswords = hd / 32;
                for (j = 0; j < wd; j++) {
                    lined = datad;
                    lines = datas + (wd - 1 - j) * wpls;
                    for (k = 0; k < nswords; k++) {
                        word = lines[k];
                        if (!word) {
                            lined += 32 * wpld;
                            continue;
                        } else {
                            iend = 32 * (k + 1);
                            for (m = 0, i = 32 * k; i < iend; i++, m++) {
                                if ((word << m) & 0x80000000)
                                    SET_DATA_BIT(lined, j);
                                lined += wpld;
                            }
                        }
                    }
                    for (i = 32 * nswords; i < hd; i++) {
                        if (GET_DATA_BIT(lines, i))
                            SET_DATA_BIT(lined, j);
                        lined += wpld;
                    }
                }
                break;
            default:
                pixDestroy(&pixd);
                L_ERROR("illegal depth: %d\n", __func__, d);
                break;
        }
    } else  {     /* direction counter-clockwise */
        switch (d)
        {
            case 32:
                for (i = 0; i < hd; i++) {
                    lined = datad + i * wpld;
                    lines = datas;
                    for (j = 0; j < wd; j++) {
                        lined[j] = lines[hd - 1 - i];
                        lines += wpls;
                    }
                }
                break;
            case 16:
                for (i = 0; i < hd; i++) {
                    lined = datad + i * wpld;
                    lines = datas;
                    for (j = 0; j < wd; j++) {
                        if ((val = GET_DATA_TWO_BYTES(lines, hd - 1 - i)))
                            SET_DATA_TWO_BYTES(lined, j, val);
                        lines += wpls;
                    }
                }
                break;
            case 8:
                for (i = 0; i < hd; i++) {
                    lined = datad + i * wpld;
                    lines = datas;
                    for (j = 0; j < wd; j++) {
                        if ((val = GET_DATA_BYTE(lines, hd - 1 - i)))
                            SET_DATA_BYTE(lined, j, val);
                        lines += wpls;
                    }
                }
                break;
            case 4:
                for (i = 0; i < hd; i++) {
                    lined = datad + i * wpld;
                    lines = datas;
                    for (j = 0; j < wd; j++) {
                        if ((val = GET_DATA_QBIT(lines, hd - 1 - i)))
                            SET_DATA_QBIT(lined, j, val);
                        lines += wpls;
                    }
                }
                break;
            case 2:
                for (i = 0; i < hd; i++) {
                    lined = datad + i * wpld;
                    lines = datas;
                    for (j = 0; j < wd; j++) {
                        if ((val = GET_DATA_DIBIT(lines, hd - 1 - i)))
                            SET_DATA_DIBIT(lined, j, val);
                        lines += wpls;
                    }
                }
                break;
            case 1:
                nswords = hd / 32;
                for (j = 0; j < wd; j++) {
                    lined = datad + (hd - 1) * wpld;
                    lines = datas + (wd - 1 - j) * wpls;
                    for (k = 0; k < nswords; k++) {
                        word = lines[k];
                        if (!word) {
                            lined -= 32 * wpld;
                            continue;
                        } else {
                            iend = 32 * (k + 1);
                            for (m = 0, i = 32 * k; i < iend; i++, m++) {
                                if ((word << m) & 0x80000000)
                                    SET_DATA_BIT(lined, wd - 1 - j);
                                lined -= wpld;
                            }
                        }
                    }
                    for (i = 32 * nswords; i < hd; i++) {
                        if (GET_DATA_BIT(lines, i))
                            SET_DATA_BIT(lined, wd - 1 - j);
                        lined -= wpld;
                    }
                }
                break;
            default:
                pixDestroy(&pixd);
                L_ERROR("illegal depth: %d\n", __func__, d);
                break;
        }
    }

    return pixd;
}


/*------------------------------------------------------------------*
 *                            Left-right flip                       *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixFlipLR()
 *
 * \param[in]    pixd    [optional]; can be null, equal to pixs,
 *                       or different from pixs
 * \param[in]    pixs    all depths
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does a left-right flip of the image, which is
 *          equivalent to a rotation out of the plane about a
 *          vertical line through the image center.
 *      (2) There are 3 cases for input:
 *          (a) pixd == null (creates a new pixd)
 *          (b) pixd == pixs (in-place operation)
 *          (c) pixd != pixs (existing pixd)
 *      (3) For clarity, use these three patterns, respectively:
 *          (a) pixd = pixFlipLR(NULL, pixs);
 *          (b) pixFlipLR(pixs, pixs);
 *          (c) pixFlipLR(pixd, pixs);
 *      (4) If an existing pixd is not the same size as pixs, the
 *          image data will be reallocated.
 *      (5) The pixel access routines allow a trivial implementation.
 *          However, for d < 8, it is more efficient to right-justify
 *          each line to a 32-bit boundary and then extract bytes and
 *          do pixel reversing.   In those cases, as in the 180 degree
 *          rotation, we right-shift the data (if necessary) to
 *          right-justify on the 32 bit boundary, and then read the
 *          bytes off each raster line in reverse order, reversing
 *          the pixels in each byte using a table.  These functions
 *          for 1, 2 and 4 bpp were tested against the "trivial"
 *          version (shown here for 4 bpp):
 *              for (i = 0; i < h; i++) {
 *                  line = data + i * wpl;
 *                  memcpy(buffer, line, bpl);
 *                    for (j = 0; j < w; j++) {
 *                      val = GET_DATA_QBIT(buffer, w - 1 - j);
 *                        SET_DATA_QBIT(line, j, val);
 *                  }
 *              }
 * </pre>
 */
PIX *
pixFlipLR(PIX  *pixd,
          PIX  *pixs)
{
l_uint8   *tab;
l_int32    w, h, d, wpl;
l_int32    extra, shift, databpl, bpl, i, j;
l_uint32   val;
l_uint32  *line, *data, *buffer;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 1 && d != 2 && d != 4 && d != 8 && d != 16 && d != 32)
        return (PIX *)ERROR_PTR("pixs not in {1,2,4,8,16,32} bpp",
                                __func__, NULL);

        /* Prepare pixd for in-place operation */
    if ((pixd = pixCopy(pixd, pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);

    data = pixGetData(pixd);
    wpl = pixGetWpl(pixd);
    switch (d)
    {
    case 1:
        tab = makeReverseByteTab1();
        break;
    case 2:
        tab = makeReverseByteTab2();
        break;
    case 4:
        tab = makeReverseByteTab4();
        break;
    default:
        tab = NULL;
        break;
    }

        /* Possibly inplace assigning return val, so on failure return pixd */
    if ((buffer = (l_uint32 *)LEPT_CALLOC(wpl, sizeof(l_uint32))) == NULL) {
        if (tab) LEPT_FREE(tab);
        return (PIX *)ERROR_PTR("buffer not made", __func__, pixd);
    }

    bpl = 4 * wpl;
    switch (d)
    {
        case 32:
            for (i = 0; i < h; i++) {
                line = data + i * wpl;
                memcpy(buffer, line, bpl);
                for (j = 0; j < w; j++)
                    line[j] = buffer[w - 1 - j];
            }
            break;
        case 16:
            for (i = 0; i < h; i++) {
                line = data + i * wpl;
                memcpy(buffer, line, bpl);
                for (j = 0; j < w; j++) {
                    val = GET_DATA_TWO_BYTES(buffer, w - 1 - j);
                    SET_DATA_TWO_BYTES(line, j, val);
                }
            }
            break;
        case 8:
            for (i = 0; i < h; i++) {
                line = data + i * wpl;
                memcpy(buffer, line, bpl);
                for (j = 0; j < w; j++) {
                    val = GET_DATA_BYTE(buffer, w - 1 - j);
                    SET_DATA_BYTE(line, j, val);
                }
            }
            break;
        case 4:
            extra = (w * d) & 31;
            if (extra)
                shift = 8 - extra / 4;
            else
                shift = 0;
            if (shift)
                rasteropHipLow(data, h, d, wpl, 0, h, shift);

            databpl = (w + 1) / 2;
            for (i = 0; i < h; i++) {
                line = data + i * wpl;
                memcpy(buffer, line, bpl);
                for (j = 0; j < databpl; j++) {
                    val = GET_DATA_BYTE(buffer, bpl - 1 - j);
                    SET_DATA_BYTE(line, j, tab[val]);
                }
            }
            break;
        case 2:
            extra = (w * d) & 31;
            if (extra)
                shift = 16 - extra / 2;
            else
                shift = 0;
            if (shift)
                rasteropHipLow(data, h, d, wpl, 0, h, shift);

            databpl = (w + 3) / 4;
            for (i = 0; i < h; i++) {
                line = data + i * wpl;
                memcpy(buffer, line, bpl);
                for (j = 0; j < databpl; j++) {
                    val = GET_DATA_BYTE(buffer, bpl - 1 - j);
                    SET_DATA_BYTE(line, j, tab[val]);
                }
            }
            break;
        case 1:
            extra = (w * d) & 31;
            if (extra)
                shift = 32 - extra;
            else
                shift = 0;
            if (shift)
                rasteropHipLow(data, h, d, wpl, 0, h, shift);

            databpl = (w + 7) / 8;
            for (i = 0; i < h; i++) {
                line = data + i * wpl;
                memcpy(buffer, line, bpl);
                for (j = 0; j < databpl; j++) {
                    val = GET_DATA_BYTE(buffer, bpl - 1 - j);
                    SET_DATA_BYTE(line, j, tab[val]);
                }
            }
            break;
        default:
            pixDestroy(&pixd);
            L_ERROR("illegal depth: %d\n", __func__, d);
            break;
    }

    LEPT_FREE(buffer);
    if (tab) LEPT_FREE(tab);
    return pixd;
}


/*------------------------------------------------------------------*
 *                            Top-bottom flip                       *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixFlipTB()
 *
 * \param[in]    pixd   [optional]; can be null, equal to pixs,
 *                      or different from pixs
 * \param[in]    pixs   all depths
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does a top-bottom flip of the image, which is
 *          equivalent to a rotation out of the plane about a
 *          horizontal line through the image center.
 *      (2) There are 3 cases for input:
 *          (a) pixd == null (creates a new pixd)
 *          (b) pixd == pixs (in-place operation)
 *          (c) pixd != pixs (existing pixd)
 *      (3) For clarity, use these three patterns, respectively:
 *          (a) pixd = pixFlipTB(NULL, pixs);
 *          (b) pixFlipTB(pixs, pixs);
 *          (c) pixFlipTB(pixd, pixs);
 *      (4) If an existing pixd is not the same size as pixs, the
 *          image data will be reallocated.
 *      (5) This is simple and fast.  We use the memcpy function
 *          to do all the work on aligned data, regardless of pixel
 *          depth.
 * </pre>
 */
PIX *
pixFlipTB(PIX  *pixd,
          PIX  *pixs)
{
l_int32    h, d, wpl, i, k, h2, bpl;
l_uint32  *linet, *lineb;
l_uint32  *data, *buffer;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, NULL, &h, &d);
    if (d != 1 && d != 2 && d != 4 && d != 8 && d != 16 && d != 32)
        return (PIX *)ERROR_PTR("pixs not in {1,2,4,8,16,32} bpp",
                                __func__, NULL);

        /* Prepare pixd for in-place operation */
    if ((pixd = pixCopy(pixd, pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);

    data = pixGetData(pixd);
    wpl = pixGetWpl(pixd);
    if ((buffer = (l_uint32 *)LEPT_CALLOC(wpl, sizeof(l_uint32))) == NULL)
        return (PIX *)ERROR_PTR("buffer not made", __func__, pixd);

    h2 = h / 2;
    bpl = 4 * wpl;
    for (i = 0, k = h - 1; i < h2; i++, k--) {
        linet = data + i * wpl;
        lineb = data + k * wpl;
        memcpy(buffer, linet, bpl);
        memcpy(linet, lineb, bpl);
        memcpy(lineb, buffer, bpl);
    }

    LEPT_FREE(buffer);
    return pixd;
}


/*------------------------------------------------------------------*
 *                      Static byte reverse tables                  *
 *------------------------------------------------------------------*/
/*!
 * \brief   makeReverseByteTab1()
 *
 *  Notes:
 *      (1) This generates an 8 bit lookup table for reversing
 *          the order of eight 1-bit pixels.
 */
static l_uint8 *
makeReverseByteTab1(void)
{
l_int32   i;
l_uint8  *tab;

    tab = (l_uint8 *)LEPT_CALLOC(256, sizeof(l_uint8));
    for (i = 0; i < 256; i++)
        tab[i] = ((0x80 & i) >> 7) |
                 ((0x40 & i) >> 5) |
                 ((0x20 & i) >> 3) |
                 ((0x10 & i) >> 1) |
                 ((0x08 & i) << 1) |
                 ((0x04 & i) << 3) |
                 ((0x02 & i) << 5) |
                 ((0x01 & i) << 7);
    return tab;
}


/*!
 * \brief   makeReverseByteTab2()
 *
 *  Notes:
 *      (1) This generates an 8 bit lookup table for reversing
 *          the order of four 2-bit pixels.
 */
static l_uint8 *
makeReverseByteTab2(void)
{
l_int32   i;
l_uint8  *tab;

    tab = (l_uint8 *)LEPT_CALLOC(256, sizeof(l_uint8));
    for (i = 0; i < 256; i++)
        tab[i] = ((0xc0 & i) >> 6) |
                 ((0x30 & i) >> 2) |
                 ((0x0c & i) << 2) |
                 ((0x03 & i) << 6);
    return tab;
}


/*!
 * \brief   makeReverseByteTab4()
 *
 *  Notes:
 *      (1) This generates an 8 bit lookup table for reversing
 *          the order of two 4-bit pixels.
 */
static l_uint8 *
makeReverseByteTab4(void)
{
l_int32   i;
l_uint8  *tab;

    tab = (l_uint8 *)LEPT_CALLOC(256, sizeof(l_uint8));
    for (i = 0; i < 256; i++)
        tab[i] = ((0xf0 & i) >> 4) | ((0x0f & i) << 4);
    return tab;
}

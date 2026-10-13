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
 * \file binexpand.c
 * <pre>
 *
 *      Replicated expansion (integer scaling)
 *         PIX     *pixExpandBinaryReplicate()
 *
 *      Special case: power of 2 replicated expansion
 *         PIX     *pixExpandBinaryPower2()
 *
 *      Expansion tables for power of 2 expansion
 *         static l_uint16    *makeExpandTab2x()
 *         static l_uint32    *makeExpandTab4x()
 *         static l_uint32    *makeExpandTab8x()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

    /* Static table functions and tables */
static l_uint16 * makeExpandTab2x(void);
static l_uint32 * makeExpandTab4x(void);
static l_uint32 * makeExpandTab8x(void);
static  l_uint32 expandtab16[] = {
              0x00000000, 0x0000ffff, 0xffff0000, 0xffffffff};

/*------------------------------------------------------------------*
 *              Replicated expansion (integer scaling)              *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixExpandBinaryReplicate()
 *
 * \param[in]    pixs   1 bpp
 * \param[in]    xfact  integer scale factor for horiz. replicative expansion
 * \param[in]    yfact  integer scale factor for vertical replicative expansion
 * \return  pixd scaled up, or NULL on error
 */
PIX *
pixExpandBinaryReplicate(PIX     *pixs,
                         l_int32  xfact,
                         l_int32  yfact)
{
l_int32    w, h, d, wd, hd, wpls, wpld, i, j, k, start;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 1)
        return (PIX *)ERROR_PTR("pixs not binary", __func__, NULL);
    if (xfact <= 0 || yfact <= 0)
        return (PIX *)ERROR_PTR("invalid scale factor: <= 0", __func__, NULL);

    if (xfact == yfact) {
        if (xfact == 1)
            return pixCopy(NULL, pixs);
        if (xfact == 2 || xfact == 4 || xfact == 8 || xfact == 16)
            return pixExpandBinaryPower2(pixs, xfact);
    }

    wpls = pixGetWpl(pixs);
    datas = pixGetData(pixs);
    wd = xfact * w;
    hd = yfact * h;
    if ((pixd = pixCreate(wd, hd, 1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, (l_float32)xfact, (l_float32)yfact);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);

    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + yfact * i * wpld;
        for (j = 0; j < w; j++) {  /* replicate pixels on a single line */
            if (GET_DATA_BIT(lines, j)) {
                start = xfact * j;
                for (k = 0; k < xfact; k++)
                    SET_DATA_BIT(lined, start + k);
            }
        }
        for (k = 1; k < yfact; k++)  /* replicate the line */
            memcpy(lined + k * wpld, lined, 4 * wpld);
    }

    return pixd;
}


/*------------------------------------------------------------------*
 *                      Power of 2 expansion                        *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixExpandBinaryPower2()
 *
 * \param[in]    pixs      1 bpp
 * \param[in]    factor    expansion factor: 1, 2, 4, 8, 16
 * \return  pixd expanded 1 bpp by replication, or NULL on error
 */
PIX *
pixExpandBinaryPower2(PIX     *pixs,
                      l_int32  factor)
{
l_uint8    sval;
l_uint16  *tab2;
l_int32    i, j, k, w, h, d, wd, hd, wpls, wpld, sdibits, sqbits, sbytes;
l_uint32  *datas, *datad, *lines, *lined, *tab4, *tab8;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 1)
        return (PIX *)ERROR_PTR("pixs not binary", __func__, NULL);
    if (factor == 1)
        return pixCopy(NULL, pixs);
    if (factor != 2 && factor != 4 && factor != 8 && factor != 16)
        return (PIX *)ERROR_PTR("factor must be in {2,4,8,16}", __func__, NULL);

    wpls = pixGetWpl(pixs);
    datas = pixGetData(pixs);
    wd = factor * w;
    hd = factor * h;
    if ((pixd = pixCreate(wd, hd, 1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixCopyResolution(pixd, pixs);
    pixScaleResolution(pixd, (l_float32)factor, (l_float32)factor);
    wpld = pixGetWpl(pixd);
    datad = pixGetData(pixd);
    if (factor == 2) {
        tab2 = makeExpandTab2x();
        sbytes = (w + 7) / 8;
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + 2 * i * wpld;
            for (j = 0; j < sbytes; j++) {
                sval = GET_DATA_BYTE(lines, j);
                SET_DATA_TWO_BYTES(lined, j, tab2[sval]);
            }
            memcpy(lined + wpld, lined, 4 * wpld);
        }
        LEPT_FREE(tab2);
    } else if (factor == 4) {
        tab4 = makeExpandTab4x();
        sbytes = (w + 7) / 8;
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + 4 * i * wpld;
            for (j = 0; j < sbytes; j++) {
                sval = GET_DATA_BYTE(lines, j);
                lined[j] = tab4[sval];
            }
            for (k = 1; k < 4; k++)
                memcpy(lined + k * wpld, lined, 4 * wpld);
        }
        LEPT_FREE(tab4);
    } else if (factor == 8) {
        tab8 = makeExpandTab8x();
        sqbits = (w + 3) / 4;
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + 8 * i * wpld;
            for (j = 0; j < sqbits; j++) {
                sval = GET_DATA_QBIT(lines, j);
                lined[j] = tab8[sval];
            }
            for (k = 1; k < 8; k++)
                memcpy(lined + k * wpld, lined, 4 * wpld);
        }
        LEPT_FREE(tab8);
    } else {  /* factor == 16 */
        sdibits = (w + 1) / 2;
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + 16 * i * wpld;
            for (j = 0; j < sdibits; j++) {
                sval = GET_DATA_DIBIT(lines, j);
                lined[j] = expandtab16[sval];
            }
            for (k = 1; k < 16; k++)
                memcpy(lined + k * wpld, lined, 4 * wpld);
        }
    }

    return pixd;
}


/*-------------------------------------------------------------------*
 *             Expansion tables for 2x, 4x and 8x expansion          *
 *-------------------------------------------------------------------*/
static l_uint16 *
makeExpandTab2x(void)
{
l_uint16  *tab;
l_int32    i;

    tab = (l_uint16 *) LEPT_CALLOC(256, sizeof(l_uint16));
    for (i = 0; i < 256; i++) {
        if (i & 0x01)
            tab[i] = 0x3;
        if (i & 0x02)
            tab[i] |= 0xc;
        if (i & 0x04)
            tab[i] |= 0x30;
        if (i & 0x08)
            tab[i] |= 0xc0;
        if (i & 0x10)
            tab[i] |= 0x300;
        if (i & 0x20)
            tab[i] |= 0xc00;
        if (i & 0x40)
            tab[i] |= 0x3000;
        if (i & 0x80)
            tab[i] |= 0xc000;
    }
    return tab;
}


static l_uint32 *
makeExpandTab4x(void)
{
l_uint32  *tab;
l_int32    i;

    tab = (l_uint32 *) LEPT_CALLOC(256, sizeof(l_uint32));
    for (i = 0; i < 256; i++) {
        if (i & 0x01)
            tab[i] = 0xf;
        if (i & 0x02)
            tab[i] |= 0xf0;
        if (i & 0x04)
            tab[i] |= 0xf00;
        if (i & 0x08)
            tab[i] |= 0xf000;
        if (i & 0x10)
            tab[i] |= 0xf0000;
        if (i & 0x20)
            tab[i] |= 0xf00000;
        if (i & 0x40)
            tab[i] |= 0xf000000;
        if (i & 0x80)
            tab[i] |= 0xf0000000;
    }
    return tab;
}


static l_uint32 *
makeExpandTab8x(void)
{
l_uint32  *tab;
l_int32    i;

    tab = (l_uint32 *) LEPT_CALLOC(16, sizeof(l_uint32));
    for (i = 0; i < 16; i++) {
        if (i & 0x01)
            tab[i] = 0xff;
        if (i & 0x02)
            tab[i] |= 0xff00;
        if (i & 0x04)
            tab[i] |= 0xff0000;
        if (i & 0x08)
            tab[i] |= 0xff000000;
    }
    return tab;
}

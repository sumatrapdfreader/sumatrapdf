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
 * \file colormorph.c
 * <pre>
 *
 *      Top-level color morphological operations
 *
 *            PIX     *pixColorMorph()
 *
 *      Method: Algorithm by van Herk and Gil and Werman, 1992
 *              Apply grayscale morphological operations separately
 *              to each component.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

/*-----------------------------------------------------------------*
 *              Top-level color morphological operations           *
 *-----------------------------------------------------------------*/
/*!
 * \brief   pixColorMorph()
 *
 * \param[in]    pixs
 * \param[in]    type    L_MORPH_DILATE, L_MORPH_ERODE, L_MORPH_OPEN,
 *                       or L_MORPH_CLOSE
 * \param[in]    hsize   width of Sel; must be odd; origin implicitly in center
 * \param[in]    vsize   ditto for height of Sel
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      (1) This does the morph operation on each component separately,
 *          and recombines the result.
 *      (2) Sel is a brick with all elements being hits.
 *      (3) If hsize = vsize = 1, just returns a copy.
 * </pre>
 */
PIX *
pixColorMorph(PIX     *pixs,
              l_int32  type,
              l_int32  hsize,
              l_int32  vsize)
{
PIX  *pixr, *pixg, *pixb, *pixrm, *pixgm, *pixbm, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (type != L_MORPH_DILATE && type != L_MORPH_ERODE &&
        type != L_MORPH_OPEN && type != L_MORPH_CLOSE)
        return (PIX *)ERROR_PTR("invalid morph type", __func__, NULL);
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

    if (hsize == 1 && vsize == 1)
        return pixCopy(NULL, pixs);

    pixr = pixGetRGBComponent(pixs, COLOR_RED);
    pixg = pixGetRGBComponent(pixs, COLOR_GREEN);
    pixb = pixGetRGBComponent(pixs, COLOR_BLUE);
    if (type == L_MORPH_DILATE) {
        pixrm = pixDilateGray(pixr, hsize, vsize);
        pixgm = pixDilateGray(pixg, hsize, vsize);
        pixbm = pixDilateGray(pixb, hsize, vsize);
    } else if (type == L_MORPH_ERODE) {
        pixrm = pixErodeGray(pixr, hsize, vsize);
        pixgm = pixErodeGray(pixg, hsize, vsize);
        pixbm = pixErodeGray(pixb, hsize, vsize);
    } else if (type == L_MORPH_OPEN) {
        pixrm = pixOpenGray(pixr, hsize, vsize);
        pixgm = pixOpenGray(pixg, hsize, vsize);
        pixbm = pixOpenGray(pixb, hsize, vsize);
    } else {   /* type == L_MORPH_CLOSE */
        pixrm = pixCloseGray(pixr, hsize, vsize);
        pixgm = pixCloseGray(pixg, hsize, vsize);
        pixbm = pixCloseGray(pixb, hsize, vsize);
    }
    pixd = pixCreateRGBImage(pixrm, pixgm, pixbm);
    pixDestroy(&pixr);
    pixDestroy(&pixrm);
    pixDestroy(&pixg);
    pixDestroy(&pixgm);
    pixDestroy(&pixb);
    pixDestroy(&pixbm);

    return pixd;
}

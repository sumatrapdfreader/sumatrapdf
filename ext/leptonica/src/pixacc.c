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
 * \file  pixacc.c
 * <pre>
 *
 *      Pixacc creation, destruction
 *           PIXACC   *pixaccCreate()
 *           PIXACC   *pixaccCreateFromPix()
 *           void      pixaccDestroy()
 *
 *      Pixacc finalization
 *           PIX      *pixaccFinal()
 *
 *      Pixacc accessors
 *           PIX      *pixaccGetPix()
 *           l_int32   pixaccGetOffset()
 *
 *      Pixacc accumulators
 *           l_int32   pixaccAdd()
 *           l_int32   pixaccSubtract()
 *           l_int32   pixaccMultConst()
 *           l_int32   pixaccMultConstAccumulate()
 *
 *  This is a simple interface for some of the pixel arithmetic operations
 *  in pixarith.c.  These are easy to code up, but not as fast as
 *  hand-coded functions that do arithmetic on corresponding pixels.
 *
 *  Suppose you want to make a linear combination of pix1 and pix2:
 *     pixd = 0.4 * pix1 + 0.6 * pix2
 *  where pix1 and pix2 are the same size and have depth 'd'.  Then:
 *     Pixacc *pacc = pixaccCreateFromPix(pix1, 0);  // first; addition only
 *     pixaccMultConst(pacc, 0.4);
 *     pixaccMultConstAccumulate(pacc, pix2, 0.6);  // Add in 0.6 of the second
 *     pixd = pixaccFinal(pacc, d);  // Get the result
 *     pixaccDestroy(&pacc);
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"
#include "pix_internal.h"

/*---------------------------------------------------------------------*
 *                     Pixacc creation, destruction                    *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaccCreate()
 *
 * \param[in]    w, h      of 32 bpp internal Pix
 * \param[in]    negflag   0 if only positive numbers are involved;
 *                         1 if there will be negative numbers
 * \return  pixacc, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Use %negflag = 1 for safety if any negative numbers are going
 *          to be used in the chain of operations.  Negative numbers
 *          arise, e.g., by subtracting a pix, or by adding a pix
 *          that has been pre-multiplied by a negative number.
 *      (2) Initializes the internal 32 bpp pix, similarly to the
 *          initialization in pixInitAccumulate().
 * </pre>
 */
PIXACC *
pixaccCreate(l_int32  w,
             l_int32  h,
             l_int32  negflag)
{
PIXACC  *pixacc;

    pixacc = (PIXACC *)LEPT_CALLOC(1, sizeof(PIXACC));
    pixacc->w = w;
    pixacc->h = h;

    if ((pixacc->pix = pixCreate(w, h, 32)) == NULL) {
        pixaccDestroy(&pixacc);
        return (PIXACC *)ERROR_PTR("pix not made", __func__, NULL);
    }

    if (negflag) {
        pixacc->offset = 0x40000000;
        pixSetAllArbitrary(pixacc->pix, pixacc->offset);
    }

    return pixacc;
}


/*!
 * \brief   pixaccCreateFromPix()
 *
 * \param[in]    pix
 * \param[in]    negflag   0 if only positive numbers are involved;
 *                         1 if there will be negative numbers
 * \return  pixacc, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See pixaccCreate()
 * </pre>
 */
PIXACC *
pixaccCreateFromPix(PIX     *pix,
                    l_int32  negflag)
{
l_int32  w, h;
PIXACC  *pixacc;

    if (!pix)
        return (PIXACC *)ERROR_PTR("pix not defined", __func__, NULL);

    pixGetDimensions(pix, &w, &h, NULL);
    pixacc = pixaccCreate(w, h, negflag);
    pixaccAdd(pixacc, pix);
    return pixacc;
}


/*!
 * \brief   pixaccDestroy()
 *
 * \param[in,out]   ppixacc   will be set to null before returning
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) Always nulls the input ptr.
 * </pre>
 */
void
pixaccDestroy(PIXACC  **ppixacc)
{
PIXACC  *pixacc;

    if (ppixacc == NULL) {
        L_WARNING("ptr address is NULL!", __func__);
        return;
    }

    if ((pixacc = *ppixacc) == NULL)
        return;

    pixDestroy(&pixacc->pix);
    LEPT_FREE(pixacc);
    *ppixacc = NULL;
}


/*---------------------------------------------------------------------*
 *                            Pixacc finalization                      *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaccFinal()
 *
 * \param[in]    pixacc
 * \param[in]    outdepth    8, 16 or 32 bpp
 * \return  pixd 8, 16 or 32 bpp, or NULL on error
 */
PIX *
pixaccFinal(PIXACC  *pixacc,
            l_int32  outdepth)
{
    if (!pixacc)
        return (PIX *)ERROR_PTR("pixacc not defined", __func__, NULL);

    return pixFinalAccumulate(pixaccGetPix(pixacc), pixaccGetOffset(pixacc),
                              outdepth);
}


/*---------------------------------------------------------------------*
 *                            Pixacc accessors                         *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaccGetPix()
 *
 * \param[in]    pixacc
 * \return  pix, or NULL on error
 */
PIX *
pixaccGetPix(PIXACC  *pixacc)
{
    if (!pixacc)
        return (PIX *)ERROR_PTR("pixacc not defined", __func__, NULL);
    return pixacc->pix;
}


/*!
 * \brief   pixaccGetOffset()
 *
 * \param[in]    pixacc
 * \return  offset, or -1 on error
 */
l_int32
pixaccGetOffset(PIXACC  *pixacc)
{
    if (!pixacc)
        return ERROR_INT("pixacc not defined", __func__, -1);
    return pixacc->offset;
}


/*---------------------------------------------------------------------*
 *                          Pixacc accumulators                        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaccAdd()
 *
 * \param[in]    pixacc
 * \param[in]    pix     to be added
 * \return  0 if OK, 1 on error
 */
l_ok
pixaccAdd(PIXACC  *pixacc,
          PIX     *pix)
{
    if (!pixacc)
        return ERROR_INT("pixacc not defined", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    pixAccumulate(pixaccGetPix(pixacc), pix, L_ARITH_ADD);
    return 0;
}


/*!
 * \brief   pixaccSubtract()
 *
 * \param[in]    pixacc
 * \param[in]    pix      to be subtracted
 * \return  0 if OK, 1 on error
 */
l_ok
pixaccSubtract(PIXACC  *pixacc,
               PIX     *pix)
{
    if (!pixacc)
        return ERROR_INT("pixacc not defined", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    pixAccumulate(pixaccGetPix(pixacc), pix, L_ARITH_SUBTRACT);
    return 0;
}


/*!
 * \brief   pixaccMultConst()
 *
 * \param[in]    pixacc
 * \param[in]    factor
 * \return  0 if OK, 1 on error
 */
l_ok
pixaccMultConst(PIXACC    *pixacc,
                l_float32  factor)
{
    if (!pixacc)
        return ERROR_INT("pixacc not defined", __func__, 1);
    pixMultConstAccumulate(pixaccGetPix(pixacc), factor,
                           pixaccGetOffset(pixacc));
    return 0;
}


/*!
 * \brief   pixaccMultConstAccumulate()
 *
 * \param[in]    pixacc
 * \param[in]    pix
 * \param[in]    factor
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This creates a temp pix that is %pix multiplied by the
 *          constant %factor.  It then adds that into %pixacc.
 * </pre>
 */
l_ok
pixaccMultConstAccumulate(PIXACC    *pixacc,
                          PIX       *pix,
                          l_float32  factor)
{
l_int32  w, h, d, negflag;
PIX     *pixt;
PIXACC  *pacct;

    if (!pixacc)
        return ERROR_INT("pixacc not defined", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    if (factor == 0.0) return 0;

    pixGetDimensions(pix, &w, &h, &d);
    negflag = (factor > 0.0) ? 0 : 1;
    pacct = pixaccCreate(w, h, negflag);
    pixaccAdd(pacct, pix);
    pixaccMultConst(pacct, factor);
    pixt = pixaccFinal(pacct, d);
    pixaccAdd(pixacc, pixt);

    pixaccDestroy(&pacct);
    pixDestroy(&pixt);
    return 0;
}

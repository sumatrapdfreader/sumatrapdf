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
 * \file morphdwa.c
 * <pre>
 *
 *    Binary morphological (dwa) ops with brick Sels
 *         PIX     *pixDilateBrickDwa()
 *         PIX     *pixErodeBrickDwa()
 *         PIX     *pixOpenBrickDwa()
 *         PIX     *pixCloseBrickDwa()
 *
 *    Binary composite morphological (dwa) ops with brick Sels
 *         PIX     *pixDilateCompBrickDwa()
 *         PIX     *pixErodeCompBrickDwa()
 *         PIX     *pixOpenCompBrickDwa()
 *         PIX     *pixCloseCompBrickDwa()
 *
 *    Binary extended composite morphological (dwa) ops with brick Sels
 *         PIX     *pixDilateCompBrickExtendDwa()
 *         PIX     *pixErodeCompBrickExtendDwa()
 *         PIX     *pixOpenCompBrickExtendDwa()
 *         PIX     *pixCloseCompBrickExtendDwa()
 *         l_int32  getExtendedCompositeParameters()
 *
 *    These are higher-level interfaces for dwa morphology with brick Sels.
 *    Because many morphological operations are performed using
 *    separable brick Sels, it is useful to have a simple interface
 *    for this.
 *
 *    We have included all 58 of the brick Sels that are generated
 *    by selaAddBasic().  These are sufficient for all the decomposable
 *    bricks up to size 63, which is the limit for dwa Sels with
 *    origins at the center of the Sel.
 *
 *    All three sets can be used as the basic interface for general
 *    brick operations.  Here are the internal calling sequences:
 *
 *      (1) If you try to apply a non-decomposable operation, such as
 *          pixErodeBrickDwa(), with a Sel size that doesn't exist,
 *          this calls a decomposable operation, pixErodeCompBrickDwa(),
 *          instead.  This can differ in linear Sel size by up to
 *          2 pixels from the request.
 *
 *      (2) If either Sel brick dimension is greater than 63, the extended
 *          composite function is called.
 *
 *      (3) The extended composite function calls the composite function
 *          a number of times with size 63, and once with size < 63.
 *          Because each operation with a size of 63 is done compositely
 *          with 7 x 9 (exactly 63), the net result is correct in
 *          length to within 2 pixels.
 *
 *    For composite operations, both using a comb and extended (beyond 63),
 *    horizontal and vertical operations are composed separately
 *    and sequentially.
 *
 *    We have also included use of all the 76 comb Sels that are generated
 *    by selaAddDwaCombs().  The generated code is in dwacomb.2.c
 *    and dwacomblow.2.c.  These are used for the composite dwa
 *    brick operations.
 *
 *    The non-composite brick operations, such as pixDilateBrickDwa(),
 *    will call the associated composite operation in situations where
 *    the requisite brick Sel has not been compiled into fmorphgen*.1.c.
 *
 *    If you want to use brick Sels that are not represented in the
 *    basic set of 58, you must generate the dwa code to implement them.
 *    You have three choices for how to use these:
 *
 *    (1) Add both the new Sels and the dwa code to the library:
 *        ~ For simplicity, add your new brick Sels to those defined
 *          in selaAddBasic().
 *        ~ Recompile the library.
 *        ~ Make prog/fmorphautogen.
 *        ~ Run prog/fmorphautogen, to generate new versions of the
 *          dwa code in fmorphgen.1.c and fmorphgenlow.1.c.
 *        ~ Copy these two files to src.
 *        ~ Recompile the library again.
 *        ~ Use the new brick Sels in your program and compile it.
 *
 *    (2) Make both the new Sels and dwa code outside the library,
 *        and link it directly to an executable:
 *        ~ Write a function to generate the new Sels in a Sela, and call
 *          fmorphautogen(sela, <N>, filename) to generate the code.
 *        ~ Compile your program that uses the newly generated function
 *          pixMorphDwa_<N>(), and link to the two new C files.
 *
 *    (3) Make the new Sels in the library and use the dwa code outside it:
 *        ~ Add code in the library to generate your new brick Sels.
 *          (It is suggested that you NOT add these Sels to the
 *          selaAddBasic() function; write a new function that generates
 *          a new Sela.)
 *        ~ Recompile the library.
 *        ~ Write a small program that generates the Sela and calls
 *          fmorphautogen(sela, <N>, filename) to generate the code.
 *        ~ Compile your program that uses the newly generated function
 *          pixMorphDwa_<N>(), and link to the two new C files.
 *       As an example of this approach, see prog/dwamorph*_reg.c:
 *        ~ added selaAddDwaLinear() to sel2.c
 *        ~ wrote dwamorph1_reg.c, to generate the dwa code.
 *        ~ compiled and linked the generated code with the application,
 *          dwamorph2_reg.c.  (Note: because this was a regression test,
 *          dwamorph1_reg also builds and runs the application program.)
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

#ifndef  NO_CONSOLE_IO
#define  DEBUG_SEL_LOOKUP   0
#endif  /* ~NO_CONSOLE_IO */

/*-----------------------------------------------------------------*
 *           Binary morphological (dwa) ops with brick Sels        *
 *-----------------------------------------------------------------*/
/*!
 * \brief   pixDilateBrickDwa()
 *
 * \param[in]    pixd    [optional]; this can be null, equal to pixs,
 *                       or different from pixs
 * \param[in]    pixs    1 bpp
 * \param[in]    hsize   width of brick Sel
 * \param[in]    vsize   height of brick Sel
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      (1) These implement 2D brick Sels, using linear Sels generated
 *          with selaAddBasic().
 *      (2) A brick Sel has hits for all elements.
 *      (3) The origin of the Sel is at (x, y) = (hsize/2, vsize/2)
 *      (4) Do separably if both hsize and vsize are > 1.
 *      (5) It is necessary that both horizontal and vertical Sels
 *          of the input size are defined in the basic sela.
 *      (6) There are three cases:
 *          (a) pixd == null   (result into new pixd)
 *          (b) pixd == pixs   (in-place; writes result back to pixs)
 *          (c) pixd != pixs   (puts result into existing pixd)
 *      (7) For clarity, if the case is known, use these patterns:
 *          (a) pixd = pixDilateBrickDwa(NULL, pixs, ...);
 *          (b) pixDilateBrickDwa(pixs, pixs, ...);
 *          (c) pixDilateBrickDwa(pixd, pixs, ...);
 *      (8) The size of pixd is determined by pixs.
 *      (9) If either linear Sel is not found, this calls
 *          the appropriate decomposible function.
 * </pre>
 */
PIX *
pixDilateBrickDwa(PIX     *pixd,
                  PIX     *pixs,
                  l_int32  hsize,
                  l_int32  vsize)
{
l_int32  found;
char    *selnameh, *selnamev;
SELA    *sela;
PIX     *pixt1, *pixt2, *pixt3;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, pixd);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize and vsize not >= 1", __func__, pixd);

    if (hsize == 1 && vsize == 1)
        return pixCopy(pixd, pixs);

    sela = selaAddBasic(NULL);
    found = TRUE;
    selnameh = selnamev = NULL;
    if (hsize > 1) {
        selnameh = selaGetBrickName(sela, hsize, 1);
        if (!selnameh) found = FALSE;
    }
    if (vsize > 1) {
        selnamev = selaGetBrickName(sela, 1, vsize);
        if (!selnamev) found = FALSE;
    }
    selaDestroy(&sela);
    if (!found) {
        L_INFO("Calling the decomposable dwa function\n", __func__);
        if (selnameh) LEPT_FREE(selnameh);
        if (selnamev) LEPT_FREE(selnamev);
        return pixDilateCompBrickDwa(pixd, pixs, hsize, vsize);
    }

    if (vsize == 1) {
        pixt2 = pixMorphDwa_1(NULL, pixs, L_MORPH_DILATE, selnameh);
        LEPT_FREE(selnameh);
    } else if (hsize == 1) {
        pixt2 = pixMorphDwa_1(NULL, pixs, L_MORPH_DILATE, selnamev);
        LEPT_FREE(selnamev);
    } else {
        pixt1 = pixAddBorder(pixs, 32, 0);
        pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_DILATE, selnameh);
        pixFMorphopGen_1(pixt1, pixt3, L_MORPH_DILATE, selnamev);
        pixt2 = pixRemoveBorder(pixt1, 32);
        pixDestroy(&pixt1);
        pixDestroy(&pixt3);
        LEPT_FREE(selnameh);
        LEPT_FREE(selnamev);
    }

    if (!pixd)
        return pixt2;

    pixTransferAllData(pixd, &pixt2, 0, 0);
    return pixd;
}


/*!
 * \brief   pixErodeBrickDwa()
 *
 * \param[in]    pixd     [optional]; this can be null, equal to pixs,
 *                        or different from pixs
 * \param[in]    pixs     1 bpp
 * \param[in]    hsize    width of brick Sel
 * \param[in]    vsize    height of brick Sel
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      (1) These implement 2D brick Sels, using linear Sels generated
 *          with selaAddBasic().
 *      (2) A brick Sel has hits for all elements.
 *      (3) The origin of the Sel is at (x, y) = (hsize/2, vsize/2)
 *      (4) Do separably if both hsize and vsize are > 1.
 *      (5) It is necessary that both horizontal and vertical Sels
 *          of the input size are defined in the basic sela.
 *      (6) Note that we must always set or clear the border pixels
 *          before each operation, depending on the the b.c.
 *          (symmetric or asymmetric).
 *      (7) There are three cases:
 *          (a) pixd == null   (result into new pixd)
 *          (b) pixd == pixs   (in-place; writes result back to pixs)
 *          (c) pixd != pixs   (puts result into existing pixd)
 *      (8) For clarity, if the case is known, use these patterns:
 *          (a) pixd = pixErodeBrickDwa(NULL, pixs, ...);
 *          (b) pixErodeBrickDwa(pixs, pixs, ...);
 *          (c) pixErodeBrickDwa(pixd, pixs, ...);
 *      (9) The size of the result is determined by pixs.
 *      (10) If either linear Sel is not found, this calls
 *           the appropriate decomposible function.
 * </pre>
 */
PIX *
pixErodeBrickDwa(PIX     *pixd,
                 PIX     *pixs,
                 l_int32  hsize,
                 l_int32  vsize)
{
l_int32  found;
char    *selnameh, *selnamev;
SELA    *sela;
PIX     *pixt1, *pixt2, *pixt3;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, pixd);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize and vsize not >= 1", __func__, pixd);

    if (hsize == 1 && vsize == 1)
        return pixCopy(pixd, pixs);

    sela = selaAddBasic(NULL);
    found = TRUE;
    selnameh = selnamev = NULL;
    if (hsize > 1) {
        selnameh = selaGetBrickName(sela, hsize, 1);
        if (!selnameh) found = FALSE;
    }
    if (vsize > 1) {
        selnamev = selaGetBrickName(sela, 1, vsize);
        if (!selnamev) found = FALSE;
    }
    selaDestroy(&sela);
    if (!found) {
        L_INFO("Calling the decomposable dwa function\n", __func__);
        if (selnameh) LEPT_FREE(selnameh);
        if (selnamev) LEPT_FREE(selnamev);
        return pixErodeCompBrickDwa(pixd, pixs, hsize, vsize);
    }

    if (vsize == 1) {
        pixt2 = pixMorphDwa_1(NULL, pixs, L_MORPH_ERODE, selnameh);
        LEPT_FREE(selnameh);
    } else if (hsize == 1) {
        pixt2 = pixMorphDwa_1(NULL, pixs, L_MORPH_ERODE, selnamev);
        LEPT_FREE(selnamev);
    } else {
        pixt1 = pixAddBorder(pixs, 32, 0);
        pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnameh);
        pixFMorphopGen_1(pixt1, pixt3, L_MORPH_ERODE, selnamev);
        pixt2 = pixRemoveBorder(pixt1, 32);
        pixDestroy(&pixt1);
        pixDestroy(&pixt3);
        LEPT_FREE(selnameh);
        LEPT_FREE(selnamev);
    }

    if (!pixd)
        return pixt2;

    pixTransferAllData(pixd, &pixt2, 0, 0);
    return pixd;
}


/*!
 * \brief   pixOpenBrickDwa()
 *
 * \param[in]    pixd     [optional]; this can be null, equal to pixs,
 *                        or different from pixs
 * \param[in]    pixs     1 bpp
 * \param[in]    hsize    width of brick Sel
 * \param[in]    vsize    height of brick Sel
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      (1) These implement 2D brick Sels, using linear Sels generated
 *          with selaAddBasic().
 *      (2) A brick Sel has hits for all elements.
 *      (3) The origin of the Sel is at (x, y) = (hsize/2, vsize/2)
 *      (4) Do separably if both hsize and vsize are > 1.
 *      (5) It is necessary that both horizontal and vertical Sels
 *          of the input size are defined in the basic sela.
 *      (6) Note that we must always set or clear the border pixels
 *          before each operation, depending on the the b.c.
 *          (symmetric or asymmetric).
 *      (7) There are three cases:
 *          (a) pixd == null   (result into new pixd)
 *          (b) pixd == pixs   (in-place; writes result back to pixs)
 *          (c) pixd != pixs   (puts result into existing pixd)
 *      (8) For clarity, if the case is known, use these patterns:
 *          (a) pixd = pixOpenBrickDwa(NULL, pixs, ...);
 *          (b) pixOpenBrickDwa(pixs, pixs, ...);
 *          (c) pixOpenBrickDwa(pixd, pixs, ...);
 *      (9) The size of the result is determined by pixs.
 *      (10) If either linear Sel is not found, this calls
 *           the appropriate decomposible function.
 * </pre>
 */
PIX *
pixOpenBrickDwa(PIX     *pixd,
                PIX     *pixs,
                l_int32  hsize,
                l_int32  vsize)
{
l_int32  found;
char    *selnameh, *selnamev;
SELA    *sela;
PIX     *pixt1, *pixt2, *pixt3;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, pixd);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize and vsize not >= 1", __func__, pixd);

    if (hsize == 1 && vsize == 1)
        return pixCopy(pixd, pixs);

    sela = selaAddBasic(NULL);
    found = TRUE;
    selnameh = selnamev = NULL;
    if (hsize > 1) {
        selnameh = selaGetBrickName(sela, hsize, 1);
        if (!selnameh) found = FALSE;
    }
    if (vsize > 1) {
        selnamev = selaGetBrickName(sela, 1, vsize);
        if (!selnamev) found = FALSE;
    }
    selaDestroy(&sela);
    if (!found) {
        L_INFO("Calling the decomposable dwa function\n", __func__);
        if (selnameh) LEPT_FREE(selnameh);
        if (selnamev) LEPT_FREE(selnamev);
        return pixOpenCompBrickDwa(pixd, pixs, hsize, vsize);
    }

    pixt1 = pixAddBorder(pixs, 32, 0);
    if (vsize == 1) {   /* horizontal only */
        pixt2 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_OPEN, selnameh);
        LEPT_FREE(selnameh);
    } else if (hsize == 1) {   /* vertical only */
        pixt2 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_OPEN, selnamev);
        LEPT_FREE(selnamev);
    } else {  /* do separable */
        pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnameh);
        pixt2 = pixFMorphopGen_1(NULL, pixt3, L_MORPH_ERODE, selnamev);
        pixFMorphopGen_1(pixt3, pixt2, L_MORPH_DILATE, selnameh);
        pixFMorphopGen_1(pixt2, pixt3, L_MORPH_DILATE, selnamev);
        LEPT_FREE(selnameh);
        LEPT_FREE(selnamev);
        pixDestroy(&pixt3);
    }
    pixt3 = pixRemoveBorder(pixt2, 32);
    pixDestroy(&pixt1);
    pixDestroy(&pixt2);

    if (!pixd)
        return pixt3;

    pixTransferAllData(pixd, &pixt3, 0, 0);
    return pixd;
}


/*!
 * \brief   pixCloseBrickDwa()
 *
 * \param[in]    pixd     [optional]; this can be null, equal to pixs,
 *                        or different from pixs
 * \param[in]    pixs     1 bpp
 * \param[in]    hsize    width of brick Sel
 * \param[in]    vsize    height of brick Sel
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      (1) This is a 'safe' closing; we add an extra border of 32 OFF
 *          pixels for the standard asymmetric b.c.
 *      (2) These implement 2D brick Sels, using linear Sels generated
 *          with selaAddBasic().
 *      (3) A brick Sel has hits for all elements.
 *      (4) The origin of the Sel is at (x, y) = (hsize/2, vsize/2)
 *      (5) Do separably if both hsize and vsize are > 1.
 *      (6) It is necessary that both horizontal and vertical Sels
 *          of the input size are defined in the basic sela.
 *      (7) Note that we must always set or clear the border pixels
 *          before each operation, depending on the the b.c.
 *          (symmetric or asymmetric).
 *      (8) There are three cases:
 *          (a) pixd == null   (result into new pixd)
 *          (b) pixd == pixs   (in-place; writes result back to pixs)
 *          (c) pixd != pixs   (puts result into existing pixd)
 *      (9) For clarity, if the case is known, use these patterns:
 *          (a) pixd = pixCloseBrickDwa(NULL, pixs, ...);
 *          (b) pixCloseBrickDwa(pixs, pixs, ...);
 *          (c) pixCloseBrickDwa(pixd, pixs, ...);
 *      (10) The size of the result is determined by pixs.
 *      (11) If either linear Sel is not found, this calls
 *           the appropriate decomposible function.
 * </pre>
 */
PIX *
pixCloseBrickDwa(PIX     *pixd,
                 PIX     *pixs,
                 l_int32  hsize,
                 l_int32  vsize)
{
l_int32  bordercolor, bordersize, found;
char    *selnameh, *selnamev;
SELA    *sela;
PIX     *pixt1, *pixt2, *pixt3;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, pixd);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize and vsize not >= 1", __func__, pixd);

    if (hsize == 1 && vsize == 1)
        return pixCopy(pixd, pixs);

    sela = selaAddBasic(NULL);
    found = TRUE;
    selnameh = selnamev = NULL;
    if (hsize > 1) {
        selnameh = selaGetBrickName(sela, hsize, 1);
        if (!selnameh) found = FALSE;
    }
    if (vsize > 1) {
        selnamev = selaGetBrickName(sela, 1, vsize);
        if (!selnamev) found = FALSE;
    }
    selaDestroy(&sela);
    if (!found) {
        L_INFO("Calling the decomposable dwa function\n", __func__);
        if (selnameh) LEPT_FREE(selnameh);
        if (selnamev) LEPT_FREE(selnamev);
        return pixCloseCompBrickDwa(pixd, pixs, hsize, vsize);
    }

        /* For "safe closing" with ASYMMETRIC_MORPH_BC, we always need
         * an extra 32 OFF pixels around the image (in addition to
         * the 32 added pixels for all dwa operations), whereas with
         * SYMMETRIC_MORPH_BC this is not necessary. */
    bordercolor = getMorphBorderPixelColor(L_MORPH_ERODE, 1);
    if (bordercolor == 0)   /* asymmetric b.c. */
        bordersize = 64;
    else   /* symmetric b.c. */
        bordersize = 32;
    pixt1 = pixAddBorder(pixs, bordersize, 0);

    if (vsize == 1) {   /* horizontal only */
        pixt2 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_CLOSE, selnameh);
        LEPT_FREE(selnameh);
    } else if (hsize == 1) {   /* vertical only */
        pixt2 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_CLOSE, selnamev);
        LEPT_FREE(selnamev);
    } else {  /* do separable */
        pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_DILATE, selnameh);
        pixt2 = pixFMorphopGen_1(NULL, pixt3, L_MORPH_DILATE, selnamev);
        pixFMorphopGen_1(pixt3, pixt2, L_MORPH_ERODE, selnameh);
        pixFMorphopGen_1(pixt2, pixt3, L_MORPH_ERODE, selnamev);
        LEPT_FREE(selnameh);
        LEPT_FREE(selnamev);
        pixDestroy(&pixt3);
    }
    pixt3 = pixRemoveBorder(pixt2, bordersize);
    pixDestroy(&pixt1);
    pixDestroy(&pixt2);

    if (!pixd)
        return pixt3;

    pixTransferAllData(pixd, &pixt3, 0, 0);
    return pixd;
}


/*-----------------------------------------------------------------*
 *    Binary composite morphological (dwa) ops with brick Sels     *
 *-----------------------------------------------------------------*/
/*!
 * \brief   pixDilateCompBrickDwa()
 *
 * \param[in]    pixd     [optional]; this can be null, equal to pixs,
 *                        or different from pixs
 * \param[in]    pixs     1 bpp
 * \param[in]    hsize    width of brick Sel
 * \param[in]    vsize    height of brick Sel
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      (1) These implement a separable composite dilation with 2D brick Sels.
 *      (2) For efficiency, it may decompose each linear morphological
 *          operation into two (brick + comb).
 *      (3) A brick Sel has hits for all elements.
 *      (4) The origin of the Sel is at (x, y) = (hsize/2, vsize/2)
 *      (5) Do separably if both hsize and vsize are > 1.
 *      (6) It is necessary that both horizontal and vertical Sels
 *          of the input size are defined in the basic sela.
 *      (7) There are three cases:
 *          (a) pixd == null   (result into new pixd)
 *          (b) pixd == pixs   (in-place; writes result back to pixs)
 *          (c) pixd != pixs   (puts result into existing pixd)
 *      (8) For clarity, if the case is known, use these patterns:
 *          (a) pixd = pixDilateCompBrickDwa(NULL, pixs, ...);
 *          (b) pixDilateCompBrickDwa(pixs, pixs, ...);
 *          (c) pixDilateCompBrickDwa(pixd, pixs, ...);
 *      (9) The size of pixd is determined by pixs.
 *      (10) CAUTION: both hsize and vsize are being decomposed.
 *          The decomposer chooses a product of sizes (call them
 *          'terms') for each that is close to the input size,
 *           but not necessarily equal to it.  It attempts to optimize:
 *              (a) for consistency with the input values: the product
 *                  of terms is close to the input size
 *              (b) for efficiency of the operation: the sum of the
 *                  terms is small; ideally about twice the square
 *                   root of the input size.
 *           So, for example, if the input hsize = 37, which is
 *           a prime number, the decomposer will break this into two
 *           terms, 6 and 6, so that the net result is a dilation
 *           with hsize = 36.
 * </pre>
 */
PIX *
pixDilateCompBrickDwa(PIX     *pixd,
                      PIX     *pixs,
                      l_int32  hsize,
                      l_int32  vsize)
{
char    *selnameh1, *selnameh2, *selnamev1, *selnamev2;
l_int32  hsize1, hsize2, vsize1, vsize2;
PIX     *pixt1, *pixt2, *pixt3;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, pixd);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize and vsize not >= 1", __func__, pixd);
    if (hsize > 63 || vsize > 63)
        return pixDilateCompBrickExtendDwa(pixd, pixs, hsize, vsize);

    if (hsize == 1 && vsize == 1)
        return pixCopy(pixd, pixs);

    hsize1 = hsize2 = vsize1 = vsize2 = 1;
    selnameh1 = selnameh2 = selnamev1 = selnamev2 = NULL;
    if (hsize > 1)
        getCompositeParameters(hsize, &hsize1, &hsize2, &selnameh1,
                               &selnameh2, NULL, NULL);
    if (vsize > 1)
        getCompositeParameters(vsize, &vsize1, &vsize2, NULL, NULL,
                               &selnamev1, &selnamev2);

#if DEBUG_SEL_LOOKUP
    lept_stderr("nameh1=%s, nameh2=%s, namev1=%s, namev2=%s\n",
                selnameh1, selnameh2, selnamev1, selnamev2);
    lept_stderr("hsize1=%d, hsize2=%d, vsize1=%d, vsize2=%d\n",
                hsize1, hsize2, vsize1, vsize2);
#endif  /* DEBUG_SEL_LOOKUP */

    pixt1 = pixAddBorder(pixs, 64, 0);
    if (vsize == 1) {
        if (hsize2 == 1) {
            pixt2 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_DILATE, selnameh1);
        } else {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_DILATE, selnameh1);
            pixt2 = pixFMorphopGen_2(NULL, pixt3, L_MORPH_DILATE, selnameh2);
            pixDestroy(&pixt3);
        }
    } else if (hsize == 1) {
        if (vsize2 == 1) {
            pixt2 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_DILATE, selnamev1);
        } else {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_DILATE, selnamev1);
            pixt2 = pixFMorphopGen_2(NULL, pixt3, L_MORPH_DILATE, selnamev2);
            pixDestroy(&pixt3);
        }
    } else {  /* vsize and hsize both > 1 */
        if (hsize2 == 1) {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_DILATE, selnameh1);
        } else {
            pixt2 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_DILATE, selnameh1);
            pixt3 = pixFMorphopGen_2(NULL, pixt2, L_MORPH_DILATE, selnameh2);
            pixDestroy(&pixt2);
        }
        if (vsize2 == 1) {
            pixt2 = pixFMorphopGen_1(NULL, pixt3, L_MORPH_DILATE, selnamev1);
        } else {
            pixt2 = pixFMorphopGen_1(NULL, pixt3, L_MORPH_DILATE, selnamev1);
            pixFMorphopGen_2(pixt2, pixt2, L_MORPH_DILATE, selnamev2);
        }
        pixDestroy(&pixt3);
    }
    pixDestroy(&pixt1);
    pixt1 = pixRemoveBorder(pixt2, 64);
    pixDestroy(&pixt2);
    if (selnameh1) LEPT_FREE(selnameh1);
    if (selnameh2) LEPT_FREE(selnameh2);
    if (selnamev1) LEPT_FREE(selnamev1);
    if (selnamev2) LEPT_FREE(selnamev2);

    if (!pixd)
        return pixt1;

    pixTransferAllData(pixd, &pixt1, 0, 0);
    return pixd;
}


/*!
 * \brief   pixErodeCompBrickDwa()
 *
 * \param[in]    pixd     [optional]; this can be null, equal to pixs,
 *                        or different from pixs
 * \param[in]    pixs     1 bpp
 * \param[in]    hsize    width of brick Sel
 * \param[in]    vsize    height of brick Sel
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      (1) These implement a separable composite erosion with 2D brick Sels.
 *      (2) For efficiency, it may decompose each linear morphological
 *          operation into two (brick + comb).
 *      (3) A brick Sel has hits for all elements.
 *      (4) The origin of the Sel is at (x, y) = (hsize/2, vsize/2)
 *      (5) Do separably if both hsize and vsize are > 1.
 *      (6) It is necessary that both horizontal and vertical Sels
 *          of the input size are defined in the basic sela.
 *      (7) There are three cases:
 *          (a) pixd == null   (result into new pixd)
 *          (b) pixd == pixs   (in-place; writes result back to pixs)
 *          (c) pixd != pixs   (puts result into existing pixd)
 *      (8) For clarity, if the case is known, use these patterns:
 *          (a) pixd = pixErodeCompBrickDwa(NULL, pixs, ...);
 *          (b) pixErodeCompBrickDwa(pixs, pixs, ...);
 *          (c) pixErodeCompBrickDwa(pixd, pixs, ...);
 *      (9) The size of pixd is determined by pixs.
 *      (10) CAUTION: both hsize and vsize are being decomposed.
 *          The decomposer chooses a product of sizes (call them
 *          'terms') for each that is close to the input size,
 *           but not necessarily equal to it.  It attempts to optimize:
 *              (a) for consistency with the input values: the product
 *                  of terms is close to the input size
 *              (b) for efficiency of the operation: the sum of the
 *                  terms is small; ideally about twice the square
 *                   root of the input size.
 *           So, for example, if the input hsize = 37, which is
 *           a prime number, the decomposer will break this into two
 *           terms, 6 and 6, so that the net result is a dilation
 *           with hsize = 36.
 * </pre>
 */
PIX *
pixErodeCompBrickDwa(PIX     *pixd,
                     PIX     *pixs,
                     l_int32  hsize,
                     l_int32  vsize)
{
char    *selnameh1, *selnameh2, *selnamev1, *selnamev2;
l_int32  hsize1, hsize2, vsize1, vsize2, bordercolor;
PIX     *pixt1, *pixt2, *pixt3;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, pixd);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize and vsize not >= 1", __func__, pixd);
    if (hsize > 63 || vsize > 63)
        return pixErodeCompBrickExtendDwa(pixd, pixs, hsize, vsize);

    if (hsize == 1 && vsize == 1)
        return pixCopy(pixd, pixs);

    hsize1 = hsize2 = vsize1 = vsize2 = 1;
    selnameh1 = selnameh2 = selnamev1 = selnamev2 = NULL;
    if (hsize > 1)
        getCompositeParameters(hsize, &hsize1, &hsize2, &selnameh1,
                               &selnameh2, NULL, NULL);
    if (vsize > 1)
        getCompositeParameters(vsize, &vsize1, &vsize2, NULL, NULL,
                               &selnamev1, &selnamev2);

        /* For symmetric b.c., bordercolor == 1 for erosion */
    bordercolor = getMorphBorderPixelColor(L_MORPH_ERODE, 1);
    pixt1 = pixAddBorder(pixs, 64, bordercolor);

    if (vsize == 1) {
        if (hsize2 == 1) {
            pixt2 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnameh1);
        } else {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnameh1);
            pixt2 = pixFMorphopGen_2(NULL, pixt3, L_MORPH_ERODE, selnameh2);
            pixDestroy(&pixt3);
        }
    } else if (hsize == 1) {
        if (vsize2 == 1) {
            pixt2 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnamev1);
        } else {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnamev1);
            pixt2 = pixFMorphopGen_2(NULL, pixt3, L_MORPH_ERODE, selnamev2);
            pixDestroy(&pixt3);
        }
    } else {  /* vsize and hsize both > 1 */
        if (hsize2 == 1) {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnameh1);
        } else {
            pixt2 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnameh1);
            pixt3 = pixFMorphopGen_2(NULL, pixt2, L_MORPH_ERODE, selnameh2);
            pixDestroy(&pixt2);
        }
        if (vsize2 == 1) {
            pixt2 = pixFMorphopGen_1(NULL, pixt3, L_MORPH_ERODE, selnamev1);
        } else {
            pixt2 = pixFMorphopGen_1(NULL, pixt3, L_MORPH_ERODE, selnamev1);
            pixFMorphopGen_2(pixt2, pixt2, L_MORPH_ERODE, selnamev2);
        }
        pixDestroy(&pixt3);
    }
    pixDestroy(&pixt1);
    pixt1 = pixRemoveBorder(pixt2, 64);
    pixDestroy(&pixt2);
    if (selnameh1) LEPT_FREE(selnameh1);
    if (selnameh2) LEPT_FREE(selnameh2);
    if (selnamev1) LEPT_FREE(selnamev1);
    if (selnamev2) LEPT_FREE(selnamev2);

    if (!pixd)
        return pixt1;

    pixTransferAllData(pixd, &pixt1, 0, 0);
    return pixd;
}


/*!
 * \brief   pixOpenCompBrickDwa()
 *
 * \param[in]    pixd     [optional]; this can be null, equal to pixs,
 *                        or different from pixs
 * \param[in]    pixs     1 bpp
 * \param[in]    hsize    width of brick Sel
 * \param[in]    vsize    height of brick Sel
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      (1) These implement a separable composite opening with 2D brick Sels.
 *      (2) For efficiency, it may decompose each linear morphological
 *          operation into two (brick + comb).
 *      (3) A brick Sel has hits for all elements.
 *      (4) The origin of the Sel is at (x, y) = (hsize/2, vsize/2)
 *      (5) Do separably if both hsize and vsize are > 1.
 *      (6) It is necessary that both horizontal and vertical Sels
 *          of the input size are defined in the basic sela.
 *      (7) There are three cases:
 *          (a) pixd == null   (result into new pixd)
 *          (b) pixd == pixs   (in-place; writes result back to pixs)
 *          (c) pixd != pixs   (puts result into existing pixd)
 *      (8) For clarity, if the case is known, use these patterns:
 *          (a) pixd = pixOpenCompBrickDwa(NULL, pixs, ...);
 *          (b) pixOpenCompBrickDwa(pixs, pixs, ...);
 *          (c) pixOpenCompBrickDwa(pixd, pixs, ...);
 *      (9) The size of pixd is determined by pixs.
 *      (10) CAUTION: both hsize and vsize are being decomposed.
 *          The decomposer chooses a product of sizes (call them
 *          'terms') for each that is close to the input size,
 *           but not necessarily equal to it.  It attempts to optimize:
 *              (a) for consistency with the input values: the product
 *                  of terms is close to the input size
 *              (b) for efficiency of the operation: the sum of the
 *                  terms is small; ideally about twice the square
 *                   root of the input size.
 *           So, for example, if the input hsize = 37, which is
 *           a prime number, the decomposer will break this into two
 *           terms, 6 and 6, so that the net result is a dilation
 *           with hsize = 36.
 * </pre>
 */
PIX *
pixOpenCompBrickDwa(PIX     *pixd,
                    PIX     *pixs,
                    l_int32  hsize,
                    l_int32  vsize)
{
char    *selnameh1, *selnameh2, *selnamev1, *selnamev2;
l_int32  hsize1, hsize2, vsize1, vsize2, bordercolor;
PIX     *pixt1, *pixt2, *pixt3;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, pixd);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize and vsize not >= 1", __func__, pixd);
    if (hsize > 63 || vsize > 63)
        return pixOpenCompBrickExtendDwa(pixd, pixs, hsize, vsize);

    if (hsize == 1 && vsize == 1)
        return pixCopy(pixd, pixs);

    hsize1 = hsize2 = vsize1 = vsize2 = 1;
    selnameh1 = selnameh2 = selnamev1 = selnamev2 = NULL;
    if (hsize > 1)
        getCompositeParameters(hsize, &hsize1, &hsize2, &selnameh1,
                               &selnameh2, NULL, NULL);
    if (vsize > 1)
        getCompositeParameters(vsize, &vsize1, &vsize2, NULL, NULL,
                               &selnamev1, &selnamev2);

        /* For symmetric b.c., initialize erosion with bordercolor == 1 */
    bordercolor = getMorphBorderPixelColor(L_MORPH_ERODE, 1);
    pixt1 = pixAddBorder(pixs, 64, bordercolor);

    if (vsize == 1) {
        if (hsize2 == 1) {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnameh1);
            if (bordercolor == 1)
                pixSetOrClearBorder(pixt3, 64, 64, 64, 64, PIX_CLR);
            pixt2 = pixFMorphopGen_1(NULL, pixt3, L_MORPH_DILATE, selnameh1);
        } else {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnameh1);
            pixt2 = pixFMorphopGen_2(NULL, pixt3, L_MORPH_ERODE, selnameh2);
            if (bordercolor == 1)
                pixSetOrClearBorder(pixt2, 64, 64, 64, 64, PIX_CLR);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_DILATE, selnameh1);
            pixFMorphopGen_2(pixt2, pixt3, L_MORPH_DILATE, selnameh2);
        }
    } else if (hsize == 1) {
        if (vsize2 == 1)  {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnamev1);
            if (bordercolor == 1)
                pixSetOrClearBorder(pixt3, 64, 64, 64, 64, PIX_CLR);
            pixt2 = pixFMorphopGen_1(NULL, pixt3, L_MORPH_DILATE, selnamev1);
        } else {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnamev1);
            pixt2 = pixFMorphopGen_2(NULL, pixt3, L_MORPH_ERODE, selnamev2);
            if (bordercolor == 1)
                pixSetOrClearBorder(pixt2, 64, 64, 64, 64, PIX_CLR);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_DILATE, selnamev1);
            pixFMorphopGen_2(pixt2, pixt3, L_MORPH_DILATE, selnamev2);
        }
    } else {  /* vsize and hsize both > 1 */
        if (hsize2 == 1 && vsize2 == 1) {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnameh1);
            pixt2 = pixFMorphopGen_1(NULL, pixt3, L_MORPH_ERODE, selnamev1);
            if (bordercolor == 1)
                pixSetOrClearBorder(pixt2, 64, 64, 64, 64, PIX_CLR);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_DILATE, selnameh1);
            pixFMorphopGen_1(pixt2, pixt3, L_MORPH_DILATE, selnamev1);
        } else if (vsize2 == 1) {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnameh1);
            pixt2 = pixFMorphopGen_2(NULL, pixt3, L_MORPH_ERODE, selnameh2);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_ERODE, selnamev1);
            if (bordercolor == 1)
                pixSetOrClearBorder(pixt3, 64, 64, 64, 64, PIX_CLR);
            pixFMorphopGen_1(pixt2, pixt3, L_MORPH_DILATE, selnameh1);
            pixFMorphopGen_2(pixt3, pixt2, L_MORPH_DILATE, selnameh2);
            pixFMorphopGen_1(pixt2, pixt3, L_MORPH_DILATE, selnamev1);
        } else if (hsize2 == 1) {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnameh1);
            pixt2 = pixFMorphopGen_1(NULL, pixt3, L_MORPH_ERODE, selnamev1);
            pixFMorphopGen_2(pixt3, pixt2, L_MORPH_ERODE, selnamev2);
            if (bordercolor == 1)
                pixSetOrClearBorder(pixt3, 64, 64, 64, 64, PIX_CLR);
            pixFMorphopGen_1(pixt2, pixt3, L_MORPH_DILATE, selnameh1);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_DILATE, selnamev1);
            pixFMorphopGen_2(pixt2, pixt3, L_MORPH_DILATE, selnamev2);
        } else {   /* both directions are combed */
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_ERODE, selnameh1);
            pixt2 = pixFMorphopGen_2(NULL, pixt3, L_MORPH_ERODE, selnameh2);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_ERODE, selnamev1);
            pixFMorphopGen_2(pixt2, pixt3, L_MORPH_ERODE, selnamev2);
            if (bordercolor == 1)
                pixSetOrClearBorder(pixt2, 64, 64, 64, 64, PIX_CLR);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_DILATE, selnameh1);
            pixFMorphopGen_2(pixt2, pixt3, L_MORPH_DILATE, selnameh2);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_DILATE, selnamev1);
            pixFMorphopGen_2(pixt2, pixt3, L_MORPH_DILATE, selnamev2);
        }
    }
    pixDestroy(&pixt3);

    pixDestroy(&pixt1);
    pixt1 = pixRemoveBorder(pixt2, 64);
    pixDestroy(&pixt2);
    if (selnameh1) LEPT_FREE(selnameh1);
    if (selnameh2) LEPT_FREE(selnameh2);
    if (selnamev1) LEPT_FREE(selnamev1);
    if (selnamev2) LEPT_FREE(selnamev2);

    if (!pixd)
        return pixt1;

    pixTransferAllData(pixd, &pixt1, 0, 0);
    return pixd;
}


/*!
 * \brief   pixCloseCompBrickDwa()
 *
 * \param[in]    pixd     [optional]; this can be null, equal to pixs,
 *                        or different from pixs
 * \param[in]    pixs     1 bpp
 * \param[in]    hsize    width of brick Sel
 * \param[in]    vsize    height of brick Sel
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      (1) This implements a separable composite safe closing with 2D
 *          brick Sels.
 *      (2) For efficiency, it may decompose each linear morphological
 *          operation into two (brick + comb).
 *      (3) A brick Sel has hits for all elements.
 *      (4) The origin of the Sel is at (x, y) = (hsize/2, vsize/2)
 *      (5) Do separably if both hsize and vsize are > 1.
 *      (6) It is necessary that both horizontal and vertical Sels
 *          of the input size are defined in the basic sela.
 *      (7) There are three cases:
 *          (a) pixd == null   (result into new pixd)
 *          (b) pixd == pixs   (in-place; writes result back to pixs)
 *          (c) pixd != pixs   (puts result into existing pixd)
 *      (8) For clarity, if the case is known, use these patterns:
 *          (a) pixd = pixCloseCompBrickDwa(NULL, pixs, ...);
 *          (b) pixCloseCompBrickDwa(pixs, pixs, ...);
 *          (c) pixCloseCompBrickDwa(pixd, pixs, ...);
 *      (9) The size of pixd is determined by pixs.
 *      (10) CAUTION: both hsize and vsize are being decomposed.
 *          The decomposer chooses a product of sizes (call them
 *          'terms') for each that is close to the input size,
 *           but not necessarily equal to it.  It attempts to optimize:
 *              (a) for consistency with the input values: the product
 *                  of terms is close to the input size
 *              (b) for efficiency of the operation: the sum of the
 *                  terms is small; ideally about twice the square
 *                   root of the input size.
 *           So, for example, if the input hsize = 37, which is
 *           a prime number, the decomposer will break this into two
 *           terms, 6 and 6, so that the net result is a dilation
 *           with hsize = 36.
 * </pre>
 */
PIX *
pixCloseCompBrickDwa(PIX     *pixd,
                     PIX     *pixs,
                     l_int32  hsize,
                     l_int32  vsize)
{
char    *selnameh1, *selnameh2, *selnamev1, *selnamev2;
l_int32  hsize1, hsize2, vsize1, vsize2, setborder;
PIX     *pixt1, *pixt2, *pixt3;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, pixd);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize and vsize not >= 1", __func__, pixd);
    if (hsize > 63 || vsize > 63)
        return pixCloseCompBrickExtendDwa(pixd, pixs, hsize, vsize);

    if (hsize == 1 && vsize == 1)
        return pixCopy(pixd, pixs);

    hsize1 = hsize2 = vsize1 = vsize2 = 1;
    selnameh1 = selnameh2 = selnamev1 = selnamev2 = NULL;
    if (hsize > 1)
        getCompositeParameters(hsize, &hsize1, &hsize2, &selnameh1,
                               &selnameh2, NULL, NULL);
    if (vsize > 1)
        getCompositeParameters(vsize, &vsize1, &vsize2, NULL, NULL,
                               &selnamev1, &selnamev2);

    pixt3 = NULL;
        /* For symmetric b.c., PIX_SET border for erosions */
    setborder = getMorphBorderPixelColor(L_MORPH_ERODE, 1);
    pixt1 = pixAddBorder(pixs, 64, 0);

    if (vsize == 1) {
        if (hsize2 == 1) {
            pixt2 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_CLOSE, selnameh1);
        } else {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_DILATE, selnameh1);
            pixt2 = pixFMorphopGen_2(NULL, pixt3, L_MORPH_DILATE, selnameh2);
            if (setborder == 1)
                pixSetOrClearBorder(pixt2, 64, 64, 64, 64, PIX_SET);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_ERODE, selnameh1);
            pixFMorphopGen_2(pixt2, pixt3, L_MORPH_ERODE, selnameh2);
        }
    } else if (hsize == 1) {
        if (vsize2 == 1) {
            pixt2 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_CLOSE, selnamev1);
        } else {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_DILATE, selnamev1);
            pixt2 = pixFMorphopGen_2(NULL, pixt3, L_MORPH_DILATE, selnamev2);
            if (setborder == 1)
                pixSetOrClearBorder(pixt2, 64, 64, 64, 64, PIX_SET);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_ERODE, selnamev1);
            pixFMorphopGen_2(pixt2, pixt3, L_MORPH_ERODE, selnamev2);
        }
    } else {  /* vsize and hsize both > 1 */
        if (hsize2 == 1 && vsize2 == 1) {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_DILATE, selnameh1);
            pixt2 = pixFMorphopGen_1(NULL, pixt3, L_MORPH_DILATE, selnamev1);
            if (setborder == 1)
                pixSetOrClearBorder(pixt2, 64, 64, 64, 64, PIX_SET);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_ERODE, selnameh1);
            pixFMorphopGen_1(pixt2, pixt3, L_MORPH_ERODE, selnamev1);
        } else if (vsize2 == 1) {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_DILATE, selnameh1);
            pixt2 = pixFMorphopGen_2(NULL, pixt3, L_MORPH_DILATE, selnameh2);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_DILATE, selnamev1);
            if (setborder == 1)
                pixSetOrClearBorder(pixt3, 64, 64, 64, 64, PIX_SET);
            pixFMorphopGen_1(pixt2, pixt3, L_MORPH_ERODE, selnameh1);
            pixFMorphopGen_2(pixt3, pixt2, L_MORPH_ERODE, selnameh2);
            pixFMorphopGen_1(pixt2, pixt3, L_MORPH_ERODE, selnamev1);
        } else if (hsize2 == 1) {
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_DILATE, selnameh1);
            pixt2 = pixFMorphopGen_1(NULL, pixt3, L_MORPH_DILATE, selnamev1);
            pixFMorphopGen_2(pixt3, pixt2, L_MORPH_DILATE, selnamev2);
            if (setborder == 1)
                pixSetOrClearBorder(pixt3, 64, 64, 64, 64, PIX_SET);
            pixFMorphopGen_1(pixt2, pixt3, L_MORPH_ERODE, selnameh1);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_ERODE, selnamev1);
            pixFMorphopGen_2(pixt2, pixt3, L_MORPH_ERODE, selnamev2);
        } else {   /* both directions are combed */
            pixt3 = pixFMorphopGen_1(NULL, pixt1, L_MORPH_DILATE, selnameh1);
            pixt2 = pixFMorphopGen_2(NULL, pixt3, L_MORPH_DILATE, selnameh2);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_DILATE, selnamev1);
            pixFMorphopGen_2(pixt2, pixt3, L_MORPH_DILATE, selnamev2);
            if (setborder == 1)
                pixSetOrClearBorder(pixt2, 64, 64, 64, 64, PIX_SET);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_ERODE, selnameh1);
            pixFMorphopGen_2(pixt2, pixt3, L_MORPH_ERODE, selnameh2);
            pixFMorphopGen_1(pixt3, pixt2, L_MORPH_ERODE, selnamev1);
            pixFMorphopGen_2(pixt2, pixt3, L_MORPH_ERODE, selnamev2);
        }
    }
    pixDestroy(&pixt3);

    pixDestroy(&pixt1);
    pixt1 = pixRemoveBorder(pixt2, 64);
    pixDestroy(&pixt2);
    if (selnameh1) LEPT_FREE(selnameh1);
    if (selnameh2) LEPT_FREE(selnameh2);
    if (selnamev1) LEPT_FREE(selnamev1);
    if (selnamev2) LEPT_FREE(selnamev2);

    if (!pixd)
        return pixt1;

    pixTransferAllData(pixd, &pixt1, 0, 0);
    return pixd;
}


/*--------------------------------------------------------------------------*
 *    Binary expanded composite morphological (dwa) ops with brick Sels     *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   pixDilateCompBrickExtendDwa()
 *
 * \param[in]    pixd     [optional]; this can be null, equal to pixs,
 *                        or different from pixs
 * \param[in]    pixs     1 bpp
 * \param[in]    hsize    width of brick Sel
 * \param[in]    vsize    height of brick Sel
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      (1) Ankur Jain suggested and implemented extending the composite
 *          DWA operations beyond the 63 pixel limit.  This is a
 *          simplified and approximate implementation of the extension.
 *          This allows arbitrary Dwa morph operations using brick Sels,
 *          by decomposing the horizontal and vertical dilations into
 *          a sequence of 63-element dilations plus a dilation of size
 *          between 3 and 62.
 *      (2) The 63-element dilations are exact, whereas the extra dilation
 *          is approximate, because the underlying decomposition is
 *          in pixDilateCompBrickDwa().  See there for further details.
 *      (3) There are three cases:
 *          (a) pixd == null   (result into new pixd)
 *          (b) pixd == pixs   (in-place; writes result back to pixs)
 *          (c) pixd != pixs   (puts result into existing pixd)
 *      (4) There is no need to call this directly:  pixDilateCompBrickDwa()
 *          calls this function if either brick dimension exceeds 63.
 * </pre>
 */
PIX *
pixDilateCompBrickExtendDwa(PIX     *pixd,
                            PIX     *pixs,
                            l_int32  hsize,
                            l_int32  vsize)
{
l_int32  i, nops, nh, extrah, nv, extrav;
PIX     *pixt1, *pixt2, *pixt3;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, pixd);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize and vsize not >= 1", __func__, pixd);

    if (hsize < 64 && vsize < 64)
        return pixDilateCompBrickDwa(pixd, pixs, hsize, vsize);

    if (hsize > 63)
        getExtendedCompositeParameters(hsize, &nh, &extrah, NULL);
    if (vsize > 63)
        getExtendedCompositeParameters(vsize, &nv, &extrav, NULL);

        /* Horizontal dilation first: pixs --> pixt2.  Do not alter pixs. */
    pixt1 = pixCreateTemplate(pixs);  /* temp image */
    if (hsize == 1) {
        pixt2 = pixClone(pixs);
    } else if (hsize < 64) {
        pixt2 = pixDilateCompBrickDwa(NULL, pixs, hsize, 1);
    } else if (hsize == 64) {  /* approximate */
        pixt2 = pixDilateCompBrickDwa(NULL, pixs, 63, 1);
    } else {
        nops = (extrah < 3) ? nh : nh + 1;
        if (nops & 1) {  /* odd */
            if (extrah > 2)
                pixt2 = pixDilateCompBrickDwa(NULL, pixs, extrah, 1);
            else
                pixt2 = pixDilateCompBrickDwa(NULL, pixs, 63, 1);
            for (i = 0; i < nops / 2; i++) {
                pixDilateCompBrickDwa(pixt1, pixt2, 63, 1);
                pixDilateCompBrickDwa(pixt2, pixt1, 63, 1);
            }
        } else {  /* nops even */
            if (extrah > 2) {
                pixDilateCompBrickDwa(pixt1, pixs, extrah, 1);
                pixt2 = pixDilateCompBrickDwa(NULL, pixt1, 63, 1);
            } else {  /* they're all 63s */
                pixDilateCompBrickDwa(pixt1, pixs, 63, 1);
                pixt2 = pixDilateCompBrickDwa(NULL, pixt1, 63, 1);
            }
            for (i = 0; i < nops / 2 - 1; i++) {
                pixDilateCompBrickDwa(pixt1, pixt2, 63, 1);
                pixDilateCompBrickDwa(pixt2, pixt1, 63, 1);
            }
        }
    }

        /* Vertical dilation: pixt2 --> pixt3.  */
    if (vsize == 1) {
        pixt3 = pixClone(pixt2);
    } else if (vsize < 64) {
        pixt3 = pixDilateCompBrickDwa(NULL, pixt2, 1, vsize);
    } else if (vsize == 64) {  /* approximate */
        pixt3 = pixDilateCompBrickDwa(NULL, pixt2, 1, 63);
    } else {
        nops = (extrav < 3) ? nv : nv + 1;
        if (nops & 1) {  /* odd */
            if (extrav > 2)
                pixt3 = pixDilateCompBrickDwa(NULL, pixt2, 1, extrav);
            else
                pixt3 = pixDilateCompBrickDwa(NULL, pixt2, 1, 63);
            for (i = 0; i < nops / 2; i++) {
                pixDilateCompBrickDwa(pixt1, pixt3, 1, 63);
                pixDilateCompBrickDwa(pixt3, pixt1, 1, 63);
            }
        } else {  /* nops even */
            if (extrav > 2) {
                pixDilateCompBrickDwa(pixt1, pixt2, 1, extrav);
                pixt3 = pixDilateCompBrickDwa(NULL, pixt1, 1, 63);
            } else {  /* they're all 63s */
                pixDilateCompBrickDwa(pixt1, pixt2, 1, 63);
                pixt3 = pixDilateCompBrickDwa(NULL, pixt1, 1, 63);
            }
            for (i = 0; i < nops / 2 - 1; i++) {
                pixDilateCompBrickDwa(pixt1, pixt3, 1, 63);
                pixDilateCompBrickDwa(pixt3, pixt1, 1, 63);
            }
        }
    }
    pixDestroy(&pixt1);
    pixDestroy(&pixt2);

    if (!pixd)
        return pixt3;

    pixTransferAllData(pixd, &pixt3, 0, 0);
    return pixd;
}


/*!
 * \brief   pixErodeCompBrickExtendDwa()
 *
 * \param[in]    pixd     [optional]; this can be null, equal to pixs,
 *                        or different from pixs
 * \param[in]    pixs     1 bpp
 * \param[in]    hsize    width of brick Sel
 * \param[in]    vsize    height of brick Sel
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      (1) See pixDilateCompBrickExtendDwa() for usage.
 *      (2) There is no need to call this directly:  pixErodeCompBrickDwa()
 *          calls this function if either brick dimension exceeds 63.
 * </pre>
 */
PIX *
pixErodeCompBrickExtendDwa(PIX     *pixd,
                           PIX     *pixs,
                           l_int32  hsize,
                           l_int32  vsize)
{
l_int32  i, nops, nh, extrah, nv, extrav;
PIX     *pixt1, *pixt2, *pixt3;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, pixd);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize and vsize not >= 1", __func__, pixd);

    if (hsize < 64 && vsize < 64)
        return pixErodeCompBrickDwa(pixd, pixs, hsize, vsize);

    if (hsize > 63)
        getExtendedCompositeParameters(hsize, &nh, &extrah, NULL);
    if (vsize > 63)
        getExtendedCompositeParameters(vsize, &nv, &extrav, NULL);

        /* Horizontal erosion first: pixs --> pixt2.  Do not alter pixs. */
    pixt1 = pixCreateTemplate(pixs);  /* temp image */
    if (hsize == 1) {
        pixt2 = pixClone(pixs);
    } else if (hsize < 64) {
        pixt2 = pixErodeCompBrickDwa(NULL, pixs, hsize, 1);
    } else if (hsize == 64) {  /* approximate */
        pixt2 = pixErodeCompBrickDwa(NULL, pixs, 63, 1);
    } else {
        nops = (extrah < 3) ? nh : nh + 1;
        if (nops & 1) {  /* odd */
            if (extrah > 2)
                pixt2 = pixErodeCompBrickDwa(NULL, pixs, extrah, 1);
            else
                pixt2 = pixErodeCompBrickDwa(NULL, pixs, 63, 1);
            for (i = 0; i < nops / 2; i++) {
                pixErodeCompBrickDwa(pixt1, pixt2, 63, 1);
                pixErodeCompBrickDwa(pixt2, pixt1, 63, 1);
            }
        } else {  /* nops even */
            if (extrah > 2) {
                pixErodeCompBrickDwa(pixt1, pixs, extrah, 1);
                pixt2 = pixErodeCompBrickDwa(NULL, pixt1, 63, 1);
            } else {  /* they're all 63s */
                pixErodeCompBrickDwa(pixt1, pixs, 63, 1);
                pixt2 = pixErodeCompBrickDwa(NULL, pixt1, 63, 1);
            }
            for (i = 0; i < nops / 2 - 1; i++) {
                pixErodeCompBrickDwa(pixt1, pixt2, 63, 1);
                pixErodeCompBrickDwa(pixt2, pixt1, 63, 1);
            }
        }
    }

        /* Vertical erosion: pixt2 --> pixt3.  */
    if (vsize == 1) {
        pixt3 = pixClone(pixt2);
    } else if (vsize < 64) {
        pixt3 = pixErodeCompBrickDwa(NULL, pixt2, 1, vsize);
    } else if (vsize == 64) {  /* approximate */
        pixt3 = pixErodeCompBrickDwa(NULL, pixt2, 1, 63);
    } else {
        nops = (extrav < 3) ? nv : nv + 1;
        if (nops & 1) {  /* odd */
            if (extrav > 2)
                pixt3 = pixErodeCompBrickDwa(NULL, pixt2, 1, extrav);
            else
                pixt3 = pixErodeCompBrickDwa(NULL, pixt2, 1, 63);
            for (i = 0; i < nops / 2; i++) {
                pixErodeCompBrickDwa(pixt1, pixt3, 1, 63);
                pixErodeCompBrickDwa(pixt3, pixt1, 1, 63);
            }
        } else {  /* nops even */
            if (extrav > 2) {
                pixErodeCompBrickDwa(pixt1, pixt2, 1, extrav);
                pixt3 = pixErodeCompBrickDwa(NULL, pixt1, 1, 63);
            } else {  /* they're all 63s */
                pixErodeCompBrickDwa(pixt1, pixt2, 1, 63);
                pixt3 = pixErodeCompBrickDwa(NULL, pixt1, 1, 63);
            }
            for (i = 0; i < nops / 2 - 1; i++) {
                pixErodeCompBrickDwa(pixt1, pixt3, 1, 63);
                pixErodeCompBrickDwa(pixt3, pixt1, 1, 63);
            }
        }
    }
    pixDestroy(&pixt1);
    pixDestroy(&pixt2);

    if (!pixd)
        return pixt3;

    pixTransferAllData(pixd, &pixt3, 0, 0);
    return pixd;
}


/*!
 * \brief   pixOpenCompBrickExtendDwa()
 *
 * \param[in]    pixd     [optional]; this can be null, equal to pixs,
 *                        or different from pixs
 * \param[in]    pixs     1 bpp
 * \param[in]    hsize    width of brick Sel
 * \param[in]    vsize    height of brick Sel
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      1) There are three cases:
 *          a) pixd == null   (result into new pixd
 *          b) pixd == pixs   (in-place; writes result back to pixs
 *          c) pixd != pixs   (puts result into existing pixd
 *      2) There is no need to call this directly:  pixOpenCompBrickDwa(
 *          calls this function if either brick dimension exceeds 63.
 * </pre>
 */
PIX *
pixOpenCompBrickExtendDwa(PIX     *pixd,
                          PIX     *pixs,
                          l_int32  hsize,
                          l_int32  vsize)
{
PIX     *pixt;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, pixd);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize and vsize not >= 1", __func__, pixd);

    pixt = pixErodeCompBrickExtendDwa(NULL, pixs, hsize, vsize);
    pixd = pixDilateCompBrickExtendDwa(pixd, pixt, hsize, vsize);
    pixDestroy(&pixt);
    return pixd;
}


/*!
 * \brief   pixCloseCompBrickExtendDwa()
 *
 * \param[in]    pixd     [optional]; this can be null, equal to pixs,
 *                        or different from pixs
 * \param[in]    pixs     1 bpp
 * \param[in]    hsize    width of brick Sel
 * \param[in]    vsize    height of brick Sel
 * \return  pixd
 *
 * <pre>
 * Notes:
 *      1) There are three cases:
 *          a) pixd == null   (result into new pixd
 *          b) pixd == pixs   (in-place; writes result back to pixs
 *          c) pixd != pixs   (puts result into existing pixd
 *      2) There is no need to call this directly:  pixCloseCompBrickDwa(
 *          calls this function if either brick dimension exceeds 63.
 * </pre>
 */
PIX *
pixCloseCompBrickExtendDwa(PIX     *pixd,
                           PIX     *pixs,
                           l_int32  hsize,
                           l_int32  vsize)
{
l_int32  bordercolor, borderx, bordery;
PIX     *pixt1, *pixt2, *pixt3;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, pixd);
    if (hsize < 1 || vsize < 1)
        return (PIX *)ERROR_PTR("hsize and vsize not >= 1", __func__, pixd);

        /* For "safe closing" with ASYMMETRIC_MORPH_BC, we always need
         * an extra 32 OFF pixels around the image (in addition to
         * the 32 added pixels for all dwa operations), whereas with
         * SYMMETRIC_MORPH_BC this is not necessary. */
    bordercolor = getMorphBorderPixelColor(L_MORPH_ERODE, 1);
    if (bordercolor == 0) {  /* asymmetric b.c. */
        borderx = 32 + (hsize / 64) * 32;
        bordery = 32 + (vsize / 64) * 32;
    } else {  /* symmetric b.c. */
        borderx = bordery = 32;
    }
    pixt1 = pixAddBorderGeneral(pixs, borderx, borderx, bordery, bordery, 0);

    pixt2 = pixDilateCompBrickExtendDwa(NULL, pixt1, hsize, vsize);
    pixErodeCompBrickExtendDwa(pixt1, pixt2, hsize, vsize);

    pixt3 = pixRemoveBorderGeneral(pixt1, borderx, borderx, bordery, bordery);
    pixDestroy(&pixt1);
    pixDestroy(&pixt2);

    if (!pixd)
        return pixt3;

    pixTransferAllData(pixd, &pixt3, 0, 0);
    return pixd;
}


/*!
 * \brief   getExtendedCompositeParameters()
 *
 * \param[in]    size          of linear Sel
 * \param[out]   pn            number of 63 wide convolutions
 * \param[out]   pextra        size of extra Sel
 * \param[out]   pactualsize   [optional] actual size used in operation
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The DWA implementation allows Sels to be used with hits
 *          up to 31 pixels from the origin, either horizontally or
 *          vertically.  Larger Sels can be used if decomposed into
 *          a set of operations with Sels not exceeding 63 pixels
 *          in either width or height (and with the origin as close
 *          to the center of the Sel as possible).
 *      (2) This returns the decomposition of a linear Sel of length
 *          %size into a set of %n Sels of length 63 plus an extra
 *          Sel of length %extra.
 *      (3) For notation, let w == %size, n == %n, and e == %extra.
 *          We have 1 < e < 63.
 *
 *          Then if w < 64, we have n = 0 and e = w.
 *          The general formula for w > 63 is:
 *             w = 63 + (n - 1) * 62 + (e - 1)
 *
 *          Where did this come from?  Each successive convolution with
 *          a Sel of length L adds a total length (L - 1) to w.
 *          This accounts for using 62 for each additional Sel of size 63,
 *          and using (e - 1) for the additional Sel of size e.
 *
 *          Solving for n and e for w > 63:
 *             n = 1 + Int((w - 63) / 62)
 *             e = w - 63 - (n - 1) * 62 + 1
 *
 *          The extra part is decomposed into two factors f1 and f2,
 *          and the actual size of the extra part is
 *             e' = f1 * f2
 *          Then the actual width is:
 *             w' = 63 + (n - 1) * 62 + f1 * f2 - 1
 * </pre>
 */
l_ok
getExtendedCompositeParameters(l_int32   size,
                               l_int32  *pn,
                               l_int32  *pextra,
                               l_int32  *pactualsize)
{
l_int32  n, extra, fact1, fact2;

    if (!pn || !pextra)
        return ERROR_INT("&n and &extra not both defined", __func__, 1);

    if (size <= 63) {
        n = 0;
        extra = L_MIN(1, size);
    } else {  /* size > 63 */
        n = 1 + (l_int32)((size - 63) / 62);
        extra = size - 63 - (n - 1) * 62 + 1;
    }

    if (pactualsize) {
        selectComposableSizes(extra, &fact1, &fact2);
        *pactualsize = 63 + (n - 1) * 62 + fact1 * fact2 - 1;
    }

    *pn = n;
    *pextra = extra;
    return 0;
}

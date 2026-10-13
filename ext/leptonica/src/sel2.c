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
 * \file sel2.c
 * <pre>
 *
 *      Contains definitions of simple structuring elements
 *
 *      Basic brick structuring elements
 *          SELA    *selaAddBasic()
 *               Linear horizontal and vertical
 *               Square
 *               Diagonals
 *
 *      Simple hit-miss structuring elements
 *          SELA    *selaAddHitMiss()
 *               Isolated foreground pixel
 *               Horizontal and vertical edges
 *               Slanted edge
 *               Corners
 *
 *      Structuring elements for comparing with DWA operations
 *          SELA    *selaAddDwaLinear()
 *          SELA    *selaAddDwaCombs()
 *
 *      Structuring elements for the intersection of lines
 *          SELA    *selaAddCrossJunctions()
 *          SELA    *selaAddTJunctions()
 *
 *      Structuring elements for connectivity-preserving thinning operations
 *          SELA    *sela4ccThin()
 *          SELA    *sela8ccThin()
 *          SELA    *sela4and8ccThin()
 *
 *      Other structuring elements
 *          SEL    *selMakePlusSign()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"

#define L_BUF_SIZE 512

    /* Linear brick sel sizes, including all those that are required
     * for decomposable sels up to size 63. */
static const l_int32  num_linear = 25;
static const l_int32  basic_linear[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
       12, 13, 14, 15, 20, 21, 25, 30, 31, 35, 40, 41, 45, 50, 51};


/* ------------------------------------------------------------------- *
 *                    Basic brick structuring elements                 *
 * ------------------------------------------------------------------- */
/*!
 * \brief   selaAddBasic()
 *
 * \param[in]    sela [optional]
 * \return  sela with additional sels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Adds the following sels:
 *            ~ all linear (horiz, vert) brick sels that are
 *              necessary for decomposable sels up to size 63
 *            ~ square brick sels up to size 10
 *            ~ 4 diagonal sels
 * </pre>
 */
SELA *
selaAddBasic(SELA  *sela)
{
char     name[L_BUF_SIZE];
l_int32  i, size;
SEL     *sel;

    if (!sela) {
        if ((sela = selaCreate(0)) == NULL)
            return (SELA *)ERROR_PTR("sela not made", __func__, NULL);
    }

    /*--------------------------------------------------------------*
     *             Linear horizontal and vertical sels              *
     *--------------------------------------------------------------*/
    for (i = 0; i < num_linear; i++) {
        size = basic_linear[i];
        sel = selCreateBrick(1, size, 0, size / 2, 1);
        snprintf(name, L_BUF_SIZE, "sel_%dh", size);
        selaAddSel(sela, sel, name, 0);
    }
    for (i = 0; i < num_linear; i++) {
        size = basic_linear[i];
        sel = selCreateBrick(size, 1, size / 2, 0, 1);
        snprintf(name, L_BUF_SIZE, "sel_%dv", size);
        selaAddSel(sela, sel, name, 0);
    }

    /*-----------------------------------------------------------*
     *                      2-d Bricks                           *
     *-----------------------------------------------------------*/
    for (i = 2; i <= 5; i++) {
        sel = selCreateBrick(i, i, i / 2, i / 2, 1);
        snprintf(name, L_BUF_SIZE, "sel_%d", i);
        selaAddSel(sela, sel, name, 0);
    }

    /*-----------------------------------------------------------*
     *                        Diagonals                          *
     *-----------------------------------------------------------*/
        /*  0c  1
            1   0  */
    sel = selCreateBrick(2, 2, 0, 0, 1);
    selSetElement(sel, 0, 0, 0);
    selSetElement(sel, 1, 1, 0);
    selaAddSel(sela, sel, "sel_2dp", 0);

        /*  1c  0
            0   1   */
    sel = selCreateBrick(2, 2, 0, 0, 1);
    selSetElement(sel, 0, 1, 0);
    selSetElement(sel, 1, 0, 0);
    selaAddSel(sela, sel, "sel_2dm", 0);

        /*  Diagonal, slope +, size 5 */
    sel = selCreate(5, 5, "sel_5dp");
    selSetOrigin(sel, 2, 2);
    selSetElement(sel, 0, 4, 1);
    selSetElement(sel, 1, 3, 1);
    selSetElement(sel, 2, 2, 1);
    selSetElement(sel, 3, 1, 1);
    selSetElement(sel, 4, 0, 1);
    selaAddSel(sela, sel, "sel_5dp", 0);

        /*  Diagonal, slope -, size 5 */
    sel = selCreate(5, 5, "sel_5dm");
    selSetOrigin(sel, 2, 2);
    selSetElement(sel, 0, 0, 1);
    selSetElement(sel, 1, 1, 1);
    selSetElement(sel, 2, 2, 1);
    selSetElement(sel, 3, 3, 1);
    selSetElement(sel, 4, 4, 1);
    selaAddSel(sela, sel, "sel_5dm", 0);

    return sela;
}


/* ------------------------------------------------------------------- *
 *                 Simple hit-miss structuring elements                *
 * ------------------------------------------------------------------- */
/*!
 * \brief   selaAddHitMiss()
 *
 * \param[in]    sela  [optional]
 * \return  sela with additional sels, or NULL on error
 */
SELA *
selaAddHitMiss(SELA  *sela)
{
SEL  *sel;

    if (!sela) {
        if ((sela = selaCreate(0)) == NULL)
            return (SELA *)ERROR_PTR("sela not made", __func__, NULL);
    }

#if 0   /*  use just for testing */
    sel = selCreateBrick(3, 3, 1, 1, 2);
    selaAddSel(sela, sel, "sel_bad", 0);
#endif


    /*--------------------------------------------------------------*
     *                   Isolated foreground pixel                  *
     *--------------------------------------------------------------*/
    sel = selCreateBrick(3, 3, 1, 1, SEL_MISS);
    selSetElement(sel, 1, 1, SEL_HIT);
    selaAddSel(sela, sel, "sel_3hm", 0);

    /*--------------------------------------------------------------*
     *                Horizontal and vertical edges                 *
     *--------------------------------------------------------------*/
    sel = selCreateBrick(2, 3, 0, 1, SEL_HIT);
    selSetElement(sel, 1, 0, SEL_MISS);
    selSetElement(sel, 1, 1, SEL_MISS);
    selSetElement(sel, 1, 2, SEL_MISS);
    selaAddSel(sela, sel, "sel_3de", 0);

    sel = selCreateBrick(2, 3, 1, 1, SEL_HIT);
    selSetElement(sel, 0, 0, SEL_MISS);
    selSetElement(sel, 0, 1, SEL_MISS);
    selSetElement(sel, 0, 2, SEL_MISS);
    selaAddSel(sela, sel, "sel_3ue", 0);

    sel = selCreateBrick(3, 2, 1, 0, SEL_HIT);
    selSetElement(sel, 0, 1, SEL_MISS);
    selSetElement(sel, 1, 1, SEL_MISS);
    selSetElement(sel, 2, 1, SEL_MISS);
    selaAddSel(sela, sel, "sel_3re", 0);

    sel = selCreateBrick(3, 2, 1, 1, SEL_HIT);
    selSetElement(sel, 0, 0, SEL_MISS);
    selSetElement(sel, 1, 0, SEL_MISS);
    selSetElement(sel, 2, 0, SEL_MISS);
    selaAddSel(sela, sel, "sel_3le", 0);

    /*--------------------------------------------------------------*
     *                        Slanted edge                          *
     *--------------------------------------------------------------*/
    sel = selCreateBrick(13, 6, 6, 2, SEL_DONT_CARE);
    selSetElement(sel, 0, 3, SEL_MISS);
    selSetElement(sel, 0, 5, SEL_HIT);
    selSetElement(sel, 4, 2, SEL_MISS);
    selSetElement(sel, 4, 4, SEL_HIT);
    selSetElement(sel, 8, 1, SEL_MISS);
    selSetElement(sel, 8, 3, SEL_HIT);
    selSetElement(sel, 12, 0, SEL_MISS);
    selSetElement(sel, 12, 2, SEL_HIT);
    selaAddSel(sela, sel, "sel_sl1", 0);

    /*--------------------------------------------------------------*
     *                           Corners                            *
     *  This allows for up to 3 missing edge pixels at the corner   *
     *--------------------------------------------------------------*/
    sel = selCreateBrick(4, 4, 1, 1, SEL_MISS);
    selSetElement(sel, 1, 1, SEL_DONT_CARE);
    selSetElement(sel, 1, 2, SEL_DONT_CARE);
    selSetElement(sel, 2, 1, SEL_DONT_CARE);
    selSetElement(sel, 1, 3, SEL_HIT);
    selSetElement(sel, 2, 2, SEL_HIT);
    selSetElement(sel, 2, 3, SEL_HIT);
    selSetElement(sel, 3, 1, SEL_HIT);
    selSetElement(sel, 3, 2, SEL_HIT);
    selSetElement(sel, 3, 3, SEL_HIT);
    selaAddSel(sela, sel, "sel_ulc", 0);

    sel = selCreateBrick(4, 4, 1, 2, SEL_MISS);
    selSetElement(sel, 1, 1, SEL_DONT_CARE);
    selSetElement(sel, 1, 2, SEL_DONT_CARE);
    selSetElement(sel, 2, 2, SEL_DONT_CARE);
    selSetElement(sel, 1, 0, SEL_HIT);
    selSetElement(sel, 2, 0, SEL_HIT);
    selSetElement(sel, 2, 1, SEL_HIT);
    selSetElement(sel, 3, 0, SEL_HIT);
    selSetElement(sel, 3, 1, SEL_HIT);
    selSetElement(sel, 3, 2, SEL_HIT);
    selaAddSel(sela, sel, "sel_urc", 0);

    sel = selCreateBrick(4, 4, 2, 1, SEL_MISS);
    selSetElement(sel, 1, 1, SEL_DONT_CARE);
    selSetElement(sel, 2, 1, SEL_DONT_CARE);
    selSetElement(sel, 2, 2, SEL_DONT_CARE);
    selSetElement(sel, 0, 1, SEL_HIT);
    selSetElement(sel, 0, 2, SEL_HIT);
    selSetElement(sel, 0, 3, SEL_HIT);
    selSetElement(sel, 1, 2, SEL_HIT);
    selSetElement(sel, 1, 3, SEL_HIT);
    selSetElement(sel, 2, 3, SEL_HIT);
    selaAddSel(sela, sel, "sel_llc", 0);

    sel = selCreateBrick(4, 4, 2, 2, SEL_MISS);
    selSetElement(sel, 1, 2, SEL_DONT_CARE);
    selSetElement(sel, 2, 1, SEL_DONT_CARE);
    selSetElement(sel, 2, 2, SEL_DONT_CARE);
    selSetElement(sel, 0, 0, SEL_HIT);
    selSetElement(sel, 0, 1, SEL_HIT);
    selSetElement(sel, 0, 2, SEL_HIT);
    selSetElement(sel, 1, 0, SEL_HIT);
    selSetElement(sel, 1, 1, SEL_HIT);
    selSetElement(sel, 2, 0, SEL_HIT);
    selaAddSel(sela, sel, "sel_lrc", 0);

    return sela;
}


/* ------------------------------------------------------------------- *
 *        Structuring elements for comparing with DWA operations       *
 * ------------------------------------------------------------------- */
/*!
 * \brief   selaAddDwaLinear()
 *
 * \param[in]    sela [optional]
 * \return  sela with additional sels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Adds all linear (horizontal, vertical) sels from
 *          2 to 63 pixels in length, which are the sizes over
 *          which dwa code can be generated.
 * </pre>
 */
SELA *
selaAddDwaLinear(SELA  *sela)
{
char     name[L_BUF_SIZE];
l_int32  i;
SEL     *sel;

    if (!sela) {
        if ((sela = selaCreate(0)) == NULL)
            return (SELA *)ERROR_PTR("sela not made", __func__, NULL);
    }

    for (i = 2; i < 64; i++) {
        sel = selCreateBrick(1, i, 0, i / 2, 1);
        snprintf(name, L_BUF_SIZE, "sel_%dh", i);
        selaAddSel(sela, sel, name, 0);
    }
    for (i = 2; i < 64; i++) {
        sel = selCreateBrick(i, 1, i / 2, 0, 1);
        snprintf(name, L_BUF_SIZE, "sel_%dv", i);
        selaAddSel(sela, sel, name, 0);
    }
    return sela;
}


/*!
 * \brief   selaAddDwaCombs()
 *
 * \param[in]    sela [optional]
 * \return  sela with additional sels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Adds all comb (horizontal, vertical) Sels that are
 *          used in composite linear morphological operations
 *          up to 63 pixels in length, which are the sizes over
 *          which dwa code can be generated.
 * </pre>
 */
SELA *
selaAddDwaCombs(SELA  *sela)
{
char     name[L_BUF_SIZE];
l_int32  i, f1, f2, prevsize, size;
SEL     *selh, *selv;

    if (!sela) {
        if ((sela = selaCreate(0)) == NULL)
            return (SELA *)ERROR_PTR("sela not made", __func__, NULL);
    }

    prevsize = 0;
    for (i = 4; i < 64; i++) {
        selectComposableSizes(i, &f1, &f2);
        size = f1 * f2;
        if (size == prevsize)
            continue;
        selectComposableSels(i, L_HORIZ, NULL, &selh);
        if (selh) {
            snprintf(name, L_BUF_SIZE, "sel_comb_%dh", size);
            selaAddSel(sela, selh, name, 0);
        } else {
            L_ERROR("selh not made for i = %d\n", __func__, i);
        }
        selectComposableSels(i, L_VERT, NULL, &selv);
        if (selv) {
            snprintf(name, L_BUF_SIZE, "sel_comb_%dv", size);
            selaAddSel(sela, selv, name, 0);
        } else {
            L_ERROR("selv not made for i = %d\n", __func__, i);
        }
        prevsize = size;
    }

    return sela;
}


/* ------------------------------------------------------------------- *
 *          Structuring elements for the intersection of lines         *
 * ------------------------------------------------------------------- */
/*!
 * \brief   selaAddCrossJunctions()
 *
 * \param[in]    sela [optional]
 * \param[in]    hlsize length of each line of hits from origin
 * \param[in]    mdist distance of misses from the origin
 * \param[in]    norient number of orientations; max of 8
 * \param[in]    debugflag 1 for debug output
 * \return  sela with additional sels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Adds hitmiss Sels for the intersection of two lines.
 *          If the lines are very thin, they must be nearly orthogonal
 *          to register.
 *      (2) The number of Sels generated is equal to %norient.
 *      (3) If %norient == 2, this generates 2 Sels of crosses, each with
 *          two perpendicular lines of hits.  One Sel has horizontal and
 *          vertical hits; the other has hits along lines at +-45 degrees.
 *          Likewise, if %norient == 3, this generates 3 Sels of crosses
 *          oriented at 30 degrees with each other.
 *      (4) It is suggested that %hlsize be chosen at least 1 greater
 *          than %mdist.  Try values of (%hlsize, %mdist) such as
 *          (6,5), (7,6), (8,7), (9,7), etc.
 * </pre>
 */
SELA *
selaAddCrossJunctions(SELA      *sela,
                      l_float32  hlsize,
                      l_float32  mdist,
                      l_int32    norient,
                      l_int32    debugflag)
{
char       name[L_BUF_SIZE];
l_int32    i, j, w, xc, yc;
l_float64  pi, halfpi, radincr, radang;
l_float64  angle;
PIX       *pixc, *pixm, *pixt;
PIXA      *pixa;
PTA       *pta1, *pta2, *pta3, *pta4;
SEL       *sel;

    if (hlsize <= 0)
        return (SELA *)ERROR_PTR("hlsize not > 0", __func__, NULL);
    if (norient < 1 || norient > 8)
        return (SELA *)ERROR_PTR("norient not in [1, ... 8]", __func__, NULL);

    if (!sela) {
        if ((sela = selaCreate(0)) == NULL)
            return (SELA *)ERROR_PTR("sela not made", __func__, NULL);
    }

    pi = 3.1415926535;
    halfpi = 3.1415926535 / 2.0;
    radincr = halfpi / (l_float64)norient;
    w = (l_int32)(2.2 * (L_MAX(hlsize, mdist) + 0.5));
    if (w % 2 == 0)
        w++;
    xc = w / 2;
    yc = w / 2;

    pixa = pixaCreate(norient);
    for (i = 0; i < norient; i++) {

            /* Set the don't cares */
        pixc = pixCreate(w, w, 32);
        pixSetAll(pixc);

            /* Add the green lines of hits */
        pixm = pixCreate(w, w, 1);
        radang = (l_float32)i * radincr;
        pta1 = generatePtaLineFromPt(xc, yc, hlsize + 1, radang);
        pta2 = generatePtaLineFromPt(xc, yc, hlsize + 1, radang + halfpi);
        pta3 = generatePtaLineFromPt(xc, yc, hlsize + 1, radang + pi);
        pta4 = generatePtaLineFromPt(xc, yc, hlsize + 1, radang + pi + halfpi);
        ptaJoin(pta1, pta2, 0, -1);
        ptaJoin(pta1, pta3, 0, -1);
        ptaJoin(pta1, pta4, 0, -1);
        pixRenderPta(pixm, pta1, L_SET_PIXELS);
        pixPaintThroughMask(pixc, pixm, 0, 0, 0x00ff0000);
        ptaDestroy(&pta1);
        ptaDestroy(&pta2);
        ptaDestroy(&pta3);
        ptaDestroy(&pta4);

            /* Add red misses between the lines */
        for (j = 0; j < 4; j++) {
            angle = radang + (j - 0.5) * halfpi;
            pixSetPixel(pixc, xc + (l_int32)(mdist * cos(angle)),
                        yc + (l_int32)(mdist * sin(angle)), 0xff000000);
        }

            /* Add dark green for origin */
        pixSetPixel(pixc, xc, yc, 0x00550000);

            /* Generate the sel */
        sel = selCreateFromColorPix(pixc, NULL);
        snprintf(name, sizeof(name), "sel_cross_%d", i);
        selaAddSel(sela, sel, name, 0);

        if (debugflag) {
            pixt = pixScaleBySampling(pixc, 10.0, 10.0);
            pixaAddPix(pixa, pixt, L_INSERT);
        }
        pixDestroy(&pixm);
        pixDestroy(&pixc);
    }

    if (debugflag) {
        l_int32  w;
        lept_mkdir("lept/sel");
        pixaGetPixDimensions(pixa, 0, &w, NULL, NULL);
        pixt = pixaDisplayTiledAndScaled(pixa, 32, w, 1, 0, 10, 2);
        pixWriteDebug("/tmp/lept/sel/xsel1.png", pixt, IFF_PNG);
        pixDisplay(pixt, 0, 100);
        pixDestroy(&pixt);
        pixt = selaDisplayInPix(sela, 15, 2, 20, 1);
        pixWriteDebug("/tmp/lept/sel/xsel2.png", pixt, IFF_PNG);
        pixDisplay(pixt, 500, 100);
        pixDestroy(&pixt);
        selaWriteStream(stderr, sela);
    }
    pixaDestroy(&pixa);

    return sela;
}


/*!
 * \brief   selaAddTJunctions()
 *
 * \param[in]    sela [optional]
 * \param[in]    hlsize length of each line of hits from origin
 * \param[in]    mdist distance of misses from the origin
 * \param[in]    norient number of orientations; max of 8
 * \param[in]    debugflag 1 for debug output
 * \return  sela with additional sels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Adds hitmiss Sels for the T-junction of two lines.
 *          If the lines are very thin, they must be nearly orthogonal
 *          to register.
 *      (2) The number of Sels generated is 4 * %norient.
 *      (3) It is suggested that %hlsize be chosen at least 1 greater
 *          than %mdist.  Try values of (%hlsize, %mdist) such as
 *          (6,5), (7,6), (8,7), (9,7), etc.
 * </pre>
 */
SELA *
selaAddTJunctions(SELA      *sela,
                  l_float32  hlsize,
                  l_float32  mdist,
                  l_int32    norient,
                  l_int32    debugflag)
{
char       name[L_BUF_SIZE];
l_int32    i, j, k, w, xc, yc;
l_float64  pi, halfpi, radincr, jang, radang;
l_float64  angle[3], dist[3];
PIX       *pixc, *pixm, *pixt;
PIXA      *pixa;
PTA       *pta1, *pta2, *pta3;
SEL       *sel;

    if (hlsize <= 2)
        return (SELA *)ERROR_PTR("hlsizel not > 1", __func__, NULL);
    if (norient < 1 || norient > 8)
        return (SELA *)ERROR_PTR("norient not in [1, ... 8]", __func__, NULL);

    if (!sela) {
        if ((sela = selaCreate(0)) == NULL)
            return (SELA *)ERROR_PTR("sela not made", __func__, NULL);
    }

    pi = 3.1415926535;
    halfpi = 3.1415926535 / 2.0;
    radincr = halfpi / (l_float32)norient;
    w = (l_int32)(2.4 * (L_MAX(hlsize, mdist) + 0.5));
    if (w % 2 == 0)
        w++;
    xc = w / 2;
    yc = w / 2;

    pixa = pixaCreate(4 * norient);
    for (i = 0; i < norient; i++) {
        for (j = 0; j < 4; j++) {  /* 4 orthogonal orientations */
            jang = (l_float32)j * halfpi;

                /* Set the don't cares */
            pixc = pixCreate(w, w, 32);
            pixSetAll(pixc);

                /* Add the green lines of hits */
            pixm = pixCreate(w, w, 1);
            radang = (l_float32)i * radincr;
            pta1 = generatePtaLineFromPt(xc, yc, hlsize + 1, jang + radang);
            pta2 = generatePtaLineFromPt(xc, yc, hlsize + 1,
                                         jang + radang + halfpi);
            pta3 = generatePtaLineFromPt(xc, yc, hlsize + 1,
                                         jang + radang + pi);
            ptaJoin(pta1, pta2, 0, -1);
            ptaJoin(pta1, pta3, 0, -1);
            pixRenderPta(pixm, pta1, L_SET_PIXELS);
            pixPaintThroughMask(pixc, pixm, 0, 0, 0x00ff0000);
            ptaDestroy(&pta1);
            ptaDestroy(&pta2);
            ptaDestroy(&pta3);

                /* Add red misses between the lines */
            angle[0] = radang + jang - halfpi;
            angle[1] = radang + jang + 0.5 * halfpi;
            angle[2] = radang + jang + 1.5 * halfpi;
            dist[0] = 0.8 * mdist;
            dist[1] = dist[2] = mdist;
            for (k = 0; k < 3; k++) {
                pixSetPixel(pixc, xc + (l_int32)(dist[k] * cos(angle[k])),
                            yc + (l_int32)(dist[k] * sin(angle[k])),
                            0xff000000);
            }

                /* Add dark green for origin */
            pixSetPixel(pixc, xc, yc, 0x00550000);

                /* Generate the sel */
            sel = selCreateFromColorPix(pixc, NULL);
            snprintf(name, sizeof(name), "sel_cross_%d", 4 * i + j);
            selaAddSel(sela, sel, name, 0);

            if (debugflag) {
                pixt = pixScaleBySampling(pixc, 10.0, 10.0);
                pixaAddPix(pixa, pixt, L_INSERT);
            }
            pixDestroy(&pixm);
            pixDestroy(&pixc);
        }
    }

    if (debugflag) {
        l_int32  w;
        lept_mkdir("lept/sel");
        pixaGetPixDimensions(pixa, 0, &w, NULL, NULL);
        pixt = pixaDisplayTiledAndScaled(pixa, 32, w, 4, 0, 10, 2);
        pixWriteDebug("/tmp/lept/sel/tsel1.png", pixt, IFF_PNG);
        pixDisplay(pixt, 0, 100);
        pixDestroy(&pixt);
        pixt = selaDisplayInPix(sela, 15, 2, 20, 4);
        pixWriteDebug("/tmp/lept/sel/tsel2.png", pixt, IFF_PNG);
        pixDisplay(pixt, 500, 100);
        pixDestroy(&pixt);
        selaWriteStream(stderr, sela);
    }
    pixaDestroy(&pixa);

    return sela;
}


/* -------------------------------------------------------------------------- *
 *    Structuring elements for connectivity-preserving thinning operations    *
 * -------------------------------------------------------------------------- */

    /* ------------------------------------------------------------
     * These sels (and their rotated counterparts) are the useful
     * 3x3 Sels for thinning.   The notation is based on
     * "Connectivity-preserving morphological image transformations,"
     * a version of which can be found at
     *           http://www.leptonica.com/papers/conn.pdf
     * ------------------------------------------------------------ */

    /* Sels for 4-connected thinning */
static const char *sel_4_1 = "  x"
                             "oCx"
                             "  x";
static const char *sel_4_2 = "  x"
                             "oCx"
                             " o ";
static const char *sel_4_3 = " o "
                             "oCx"
                             "  x";
static const char *sel_4_4 = " o "
                             "oCx"
                             " o ";
static const char *sel_4_5 = " ox"
                             "oCx"
                             " o ";
static const char *sel_4_6 = " o "
                             "oCx"
                             " ox";
static const char *sel_4_7 = " xx"
                             "oCx"
                             " o ";
static const char *sel_4_8 = "  x"
                             "oCx"
                             "o x";
static const char *sel_4_9 = "o x"
                             "oCx"
                             "  x";

    /* Sels for 8-connected thinning */
static const char *sel_8_1 = " x "
                             "oCx"
                             " x ";
static const char *sel_8_2 = " x "
                             "oCx"
                             "o  ";
static const char *sel_8_3 = "o  "
                             "oCx"
                             " x ";
static const char *sel_8_4 = "o  "
                             "oCx"
                             "o  ";
static const char *sel_8_5 = "o x"
                             "oCx"
                             "o  ";
static const char *sel_8_6 = "o  "
                             "oCx"
                             "o x";
static const char *sel_8_7 = " x "
                             "oCx"
                             "oo ";
static const char *sel_8_8 = " x "
                             "oCx"
                             "ox ";
static const char *sel_8_9 = "ox "
                             "oCx"
                             " x ";

    /* Sels for both 4 and 8-connected thinning */
static const char *sel_48_1 = " xx"
                              "oCx"
                              "oo ";
static const char *sel_48_2 = "o x"
                              "oCx"
                              "o x";


/*!
 * \brief   sela4ccThin()
 *
 * \param[in]    sela [optional]
 * \return  sela with additional sels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Adds the 9 basic sels for 4-cc thinning.
 * </pre>
 */
SELA *
sela4ccThin(SELA  *sela)
{
SEL  *sel;

    if (!sela) sela = selaCreate(9);

    sel = selCreateFromString(sel_4_1, 3, 3, "sel_4_1");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_4_2, 3, 3, "sel_4_2");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_4_3, 3, 3, "sel_4_3");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_4_4, 3, 3, "sel_4_4");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_4_5, 3, 3, "sel_4_5");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_4_6, 3, 3, "sel_4_6");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_4_7, 3, 3, "sel_4_7");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_4_8, 3, 3, "sel_4_8");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_4_9, 3, 3, "sel_4_9");
    selaAddSel(sela, sel, NULL, 0);

    return sela;
}


/*!
 * \brief   sela8ccThin()
 *
 * \param[in]    sela [optional]
 * \return  sela with additional sels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Adds the 9 basic sels for 8-cc thinning.
 * </pre>
 */
SELA *
sela8ccThin(SELA  *sela)
{
SEL  *sel;

    if (!sela) sela = selaCreate(9);

    sel = selCreateFromString(sel_8_1, 3, 3, "sel_8_1");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_8_2, 3, 3, "sel_8_2");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_8_3, 3, 3, "sel_8_3");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_8_4, 3, 3, "sel_8_4");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_8_5, 3, 3, "sel_8_5");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_8_6, 3, 3, "sel_8_6");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_8_7, 3, 3, "sel_8_7");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_8_8, 3, 3, "sel_8_8");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_8_9, 3, 3, "sel_8_9");
    selaAddSel(sela, sel, NULL, 0);

    return sela;
}


/*!
 * \brief   sela4and8ccThin()
 *
 * \param[in]    sela [optional]
 * \return  sela with additional sels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Adds the 2 basic sels for either 4-cc or 8-cc thinning.
 * </pre>
 */
SELA *
sela4and8ccThin(SELA  *sela)
{
SEL  *sel;

    if (!sela) sela = selaCreate(2);

    sel = selCreateFromString(sel_48_1, 3, 3, "sel_48_1");
    selaAddSel(sela, sel, NULL, 0);
    sel = selCreateFromString(sel_48_2, 3, 3, "sel_48_2");
    selaAddSel(sela, sel, NULL, 0);

    return sela;
}


/* -------------------------------------------------------------------------- *
 *                        Other structuring elements                          *
 * -------------------------------------------------------------------------- */
/*!
 * \brief   selMakePlusSign()
 *
 * \param[in]    size        side of containing square
 * \param[in]    linewidth   of lines
 * \return  sel, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Useful for debugging to show location of selected pixels.
 *      (2) See displaySelectedPixels() for an example of use.
 * </pre>
 */
SEL *
selMakePlusSign(l_int32  size,
                l_int32  linewidth)
{
PIX  *pix;
SEL  *sel;

    if (size < 3 || linewidth > size)
        return (SEL *)ERROR_PTR("invalid input", __func__, NULL);

    pix = pixCreate(size, size, 1);
    pixRenderLine(pix, size / 2, 0, size / 2, size - 1,
                  linewidth, L_SET_PIXELS);
    pixRenderLine(pix, 0, size / 2, size, size / 2,
                  linewidth, L_SET_PIXELS);
    sel = selCreateFromPix(pix, size / 2, size / 2, "plus_sign");
    pixDestroy(&pix);
    return sel;
}

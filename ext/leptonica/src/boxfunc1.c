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
 * \file  boxfunc1.c
 * <pre>
 *
 *      Box geometry
 *           l_int32   boxContains()
 *           l_int32   boxIntersects()
 *           BOXA     *boxaContainedInBox()
 *           l_int32   boxaContainedInBoxCount()
 *           l_int32   boxaContainedInBoxa()
 *           BOXA     *boxaIntersectsBox()
 *           l_int32   boxaIntersectsBoxCount()
 *           BOXA     *boxaClipToBox()
 *           BOXA     *boxaCombineOverlaps()
 *           l_int32   boxaCombineOverlapsInPair()
 *           BOX      *boxOverlapRegion()
 *           BOX      *boxBoundingRegion()
 *           l_int32   boxOverlapFraction()
 *           l_int32   boxOverlapArea()
 *           BOXA     *boxaHandleOverlaps()
 *           l_int32   boxOverlapDistance()
 *           l_int32   boxSeparationDistance()
 *           l_int32   boxCompareSize()
 *           l_int32   boxContainsPt()
 *           BOX      *boxaGetNearestToPt()
 *           BOX      *boxaGetNearestToLine()
 *           l_int32   boxaFindNearestBoxes()
 *           l_int32   boxaGetNearestByDirection()
 *    static l_int32   boxHasOverlapInXorY()
 *    static l_int32   boxGetDistanceInXorY()
 *           l_int32   boxIntersectByLine()
 *           l_int32   boxGetCenter()
 *           BOX      *boxClipToRectangle()
 *           l_int32   boxClipToRectangleParams()
 *           BOX      *boxRelocateOneSide()
 *           BOXA     *boxaAdjustSides()
 *           BOXA     *boxaAdjustBoxSides()
 *           BOX      *boxAdjustSides()
 *           BOXA     *boxaSetSide()
 *           l_int32   boxSetSide()
 *           BOXA     *boxaAdjustWidthToTarget()
 *           BOXA     *boxaAdjustHeightToTarget()
 *           l_int32   boxEqual()
 *           l_int32   boxaEqual()
 *           l_int32   boxSimilar()
 *           l_int32   boxaSimilar()
 *
 *      Boxa combine and split
 *           l_int32   boxaJoin()
 *           l_int32   boxaaJoin()
 *           l_int32   boxaSplitEvenOdd()
 *           BOXA     *boxaMergeEvenOdd()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"
#include "pix_internal.h"

static l_int32 boxHasOverlapInXorY(l_int32 c1, l_int32 s1, l_int32 c2,
                                   l_int32 s2);
static l_int32 boxGetDistanceInXorY(l_int32 c1, l_int32 s1, l_int32 c2,
                                    l_int32 s2);


/*---------------------------------------------------------------------*
 *                             Box geometry                            *
 *---------------------------------------------------------------------*/
/*!
 * \brief   boxContains()
 *
 * \param[in]    box1, box2
 * \param[out]   presult     1 if box2 is entirely contained within box1;
 *                           0 otherwise
 * \return  0 if OK, 1 on error
 */
l_ok
boxContains(BOX     *box1,
            BOX     *box2,
            l_int32 *presult)
{
l_int32  x1, y1, w1, h1, x2, y2, w2, h2, valid1, valid2;

    if (!presult)
        return ERROR_INT("&result not defined", __func__, 1);
    *presult = 0;
    if (!box1 || !box2)
        return ERROR_INT("boxes not both defined", __func__, 1);
    boxIsValid(box1, &valid1);
    boxIsValid(box2, &valid2);
    if (!valid1 || !valid2)
        return ERROR_INT("boxes not both valid", __func__, 1);

    boxGetGeometry(box1, &x1, &y1, &w1, &h1);
    boxGetGeometry(box2, &x2, &y2, &w2, &h2);
    if (x1 <= x2 && y1 <= y2 && (x1 + w1 >= x2 + w2) && (y1 + h1 >= y2 + h2))
        *presult = 1;
    return 0;
}


/*!
 * \brief   boxIntersects()
 *
 * \param[in]    box1, box2
 * \param[out]   presult    1 if any part of box2 is contained in box1;
 *                          0 otherwise
 * \return  0 if OK, 1 on error
 */
l_ok
boxIntersects(BOX      *box1,
              BOX      *box2,
              l_int32  *presult)
{
l_int32  l1, l2, r1, r2, t1, t2, b1, b2, w1, h1, w2, h2, valid1, valid2;

    if (!presult)
        return ERROR_INT("&result not defined", __func__, 1);
    *presult = 0;
    if (!box1 || !box2)
        return ERROR_INT("boxes not both defined", __func__, 1);
    boxIsValid(box1, &valid1);
    boxIsValid(box2, &valid2);
    if (!valid1 || !valid2)
        return ERROR_INT("boxes not both valid", __func__, 1);

    boxGetGeometry(box1, &l1, &t1, &w1, &h1);
    boxGetGeometry(box2, &l2, &t2, &w2, &h2);
    r1 = l1 + w1 - 1;
    r2 = l2 + w2 - 1;
    b1 = t1 + h1 - 1;
    b2 = t2 + h2 - 1;
    if (b2 < t1 || b1 < t2 || r1 < l2 || r2 < l1)
        *presult = 0;
    else
        *presult = 1;
    return 0;
}


/*!
 * \brief   boxaContainedInBox()
 *
 * \param[in]    boxas
 * \param[in]    box     for containment
 * \return  boxad  boxa with all boxes in boxas that are entirely
 *                 contained in box, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) All boxes in %boxas that are entirely outside box are removed.
 *      (2) If %box is not valid, returns an empty boxa.
 * </pre>
 */
BOXA *
boxaContainedInBox(BOXA  *boxas,
                   BOX   *box)
{
l_int32  i, n, val, valid;
BOX     *box1;
BOXA    *boxad;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (!box)
        return (BOXA *)ERROR_PTR("box not defined", __func__, NULL);
    n = boxaGetCount(boxas);
    boxIsValid(box, &valid);
    if (n == 0 || !valid)
        return boxaCreate(1);  /* empty */

    boxad = boxaCreate(0);
    for (i = 0; i < n; i++) {
        if ((box1 = boxaGetValidBox(boxas, i, L_CLONE)) == NULL)
            continue;
        boxContains(box, box1, &val);
        if (val == 1)
            boxaAddBox(boxad, box1, L_COPY);
        boxDestroy(&box1);  /* destroy the clone */
    }

    return boxad;
}


/*!
 * \brief   boxaContainedInBoxCount()
 *
 * \param[in]    boxa
 * \param[in]    box      for selecting contained boxes in %boxa
 * \param[out]   pcount   number of boxes intersecting the box
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If %box is not valid, returns a zero count.
 * </pre>
 */
l_ok
boxaContainedInBoxCount(BOXA     *boxa,
                        BOX      *box,
                        l_int32  *pcount)
{
l_int32  i, n, val, valid;
BOX     *box1;

    if (!pcount)
        return ERROR_INT("&count not defined", __func__, 1);
    *pcount = 0;
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);
    n = boxaGetCount(boxa);
    boxIsValid(box, &valid);
    if (n == 0 || !valid)
        return 0;

    for (i = 0; i < n; i++) {
        if ((box1 = boxaGetValidBox(boxa, i, L_CLONE)) == NULL)
            continue;
        boxContains(box, box1, &val);
        if (val == 1)
            (*pcount)++;
        boxDestroy(&box1);
    }
    return 0;
}


/*!
 * \brief   boxaContainedInBoxa()
 *
 * \param[in]     boxa1, boxa2
 * \param[out]    pcontained    1 if every box in boxa2 is contained in
 *                              some box in boxa1; 0 otherwise
 * \return  0 if OK, 1 on error
 */
l_ok
boxaContainedInBoxa(BOXA     *boxa1,
                    BOXA     *boxa2,
                    l_int32  *pcontained)
{
l_int32  i, j, n1, n2, cont, result;
BOX     *box1, *box2;

    if (!pcontained)
        return ERROR_INT("&contained not defined", __func__, 1);
    *pcontained = 0;
    if (!boxa1 || !boxa2)
        return ERROR_INT("boxa1 and boxa2 not both defined", __func__, 1);

    n1 = boxaGetCount(boxa1);
    n2 = boxaGetCount(boxa2);
    for (i = 0; i < n2; i++) {
        if ((box2 = boxaGetValidBox(boxa2, i, L_CLONE)) == NULL)
            continue;
        cont = 0;
        for (j = 0; j < n1; j++) {
            if ((box1 = boxaGetValidBox(boxa1, j, L_CLONE)) == NULL)
                continue;
            boxContains(box1, box2, &result);
            boxDestroy(&box1);
            if (result) {
                cont = 1;
                break;
            }
        }
        boxDestroy(&box2);
        if (!cont) return 0;
    }

    *pcontained = 1;
    return 0;
}


/*!
 * \brief   boxaIntersectsBox()
 *
 * \param[in]    boxas
 * \param[in]    box     for intersecting
 * \return  boxad    boxa with all boxes in boxas that intersect box,
 *                   or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) All boxes in boxa that intersect with box (i.e., are completely
 *          or partially contained in box) are retained.
 * </pre>
 */
BOXA *
boxaIntersectsBox(BOXA  *boxas,
                  BOX   *box)
{
l_int32  i, n, val, valid;
BOX     *box1;
BOXA    *boxad;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (!box)
        return (BOXA *)ERROR_PTR("box not defined", __func__, NULL);
    n = boxaGetCount(boxas);
    boxIsValid(box, &valid);
    if (n == 0 || !valid)
        return boxaCreate(1);  /* empty */

    boxad = boxaCreate(0);
    for (i = 0; i < n; i++) {
        if ((box1 = boxaGetValidBox(boxas, i, L_CLONE)) == NULL)
            continue;
        boxIntersects(box, box1, &val);
        if (val == 1)
            boxaAddBox(boxad, box1, L_COPY);
        boxDestroy(&box1);  /* destroy the clone */
    }

    return boxad;
}


/*!
 * \brief   boxaIntersectsBoxCount()
 *
 * \param[in]    boxa
 * \param[in]    box      for selecting intersecting boxes in %boxa
 * \param[out]   pcount   number of boxes intersecting the box
 * \return  0 if OK, 1 on error
 */
l_ok
boxaIntersectsBoxCount(BOXA     *boxa,
                       BOX      *box,
                       l_int32  *pcount)
{
l_int32  i, n, val, valid;
BOX     *box1;

    if (!pcount)
        return ERROR_INT("&count not defined", __func__, 1);
    *pcount = 0;
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);
    n = boxaGetCount(boxa);
    boxIsValid(box, &valid);
    if (n == 0 || !valid)
        return 0;

    for (i = 0; i < n; i++) {
        if ((box1 = boxaGetValidBox(boxa, i, L_CLONE)) == NULL)
            continue;
        boxIntersects(box, box1, &val);
        if (val == 1)
            (*pcount)++;
        boxDestroy(&box1);
    }
    return 0;
}


/*!
 * \brief   boxaClipToBox()
 *
 * \param[in]    boxas
 * \param[in]    box     for clipping
 * \return  boxad     boxa with boxes in boxas clipped to box, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) All boxes in boxa not intersecting with box are removed, and
 *          the remaining boxes are clipped to box.
 * </pre>
 */
BOXA *
boxaClipToBox(BOXA  *boxas,
              BOX   *box)
{
l_int32  i, n, valid;
BOX     *box1, *boxo;
BOXA    *boxad;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (!box)
        return (BOXA *)ERROR_PTR("box not defined", __func__, NULL);
    n = boxaGetCount(boxas);
    boxIsValid(box, &valid);
    if (n == 0 || !valid)
        return boxaCreate(1);  /* empty */

    boxad = boxaCreate(0);
    for (i = 0; i < n; i++) {
        if ((box1 = boxaGetValidBox(boxas, i, L_CLONE)) == NULL)
            continue;
        if ((boxo = boxOverlapRegion(box, box1)) != NULL)
            boxaAddBox(boxad, boxo, L_INSERT);
        boxDestroy(&box1);
    }

    return boxad;
}


/*!
 * \brief   boxaCombineOverlaps()
 *
 * \param[in]       boxas
 * \param[in,out]   pixadb     debug output
 * \return  boxad   where each set of boxes in boxas that overlap are combined
 *                  into a single bounding box in boxad, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) If there are no overlapping boxes, it simply returns a copy
 *          of %boxas.
 *      (2) Input an empty %pixadb, using pixaCreate(0), for debug output.
 *          The output gives 2 visualizations of the boxes per iteration;
 *          boxes in red before, and added boxes in green after. Note that
 *          all pixels in the red boxes are contained in the green ones.
 *      (3) The alternative method of painting each rectangle and finding
 *          the 4-connected components gives a different result in
 *          general, because two non-overlapping (but touching)
 *          rectangles, when rendered, are 4-connected and will be joined.
 *      (4) A bad case computationally is to have n boxes, none of which
 *          overlap.  Then you have one iteration with O(n^2) compares.
 *          This is still faster than painting each rectangle and finding
 *          the bounding boxes of the connected components, even for
 *          thousands of rectangles.
 * </pre>
 */
BOXA *
boxaCombineOverlaps(BOXA  *boxas,
                    PIXA  *pixadb)
{
l_int32  i, j, w, h, n1, n2, overlap, niters;
BOX     *box1, *box2, *box3;
BOXA    *boxa1, *boxa2;
PIX     *pix1 = NULL;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);

    if (pixadb) boxaGetExtent(boxas, &w, &h, NULL);

    boxa1 = boxaCopy(boxas, L_COPY);
    n1 = boxaGetCount(boxa1);
    niters = 0;
    while (1) {  /* loop until no change from previous iteration */
        niters++;
        if (pixadb) {
            pix1 = pixCreate(w + 5, h + 5, 32);
            pixSetAll(pix1);
            pixRenderBoxaArb(pix1, boxa1, 2, 255, 0, 0);
            pixaAddPix(pixadb, pix1, L_COPY);
        }

            /* Combine overlaps for this iteration */
        for (i = 0; i < n1; i++) {
            if ((box1 = boxaGetValidBox(boxa1, i, L_COPY)) == NULL)
                continue;
            for (j = i + 1; j < n1; j++) {
                if ((box2 = boxaGetValidBox(boxa1, j, L_COPY)) == NULL)
                    continue;
                boxIntersects(box1, box2, &overlap);
                if (overlap) {
                    box3 = boxBoundingRegion(box1, box2);
                    boxaReplaceBox(boxa1, i, box3);
                    boxaReplaceBox(boxa1, j, boxCreate(0, 0, 0, 0));
                    boxDestroy(&box1);
                    box1 = boxCopy(box3);
                }
                boxDestroy(&box2);
            }
            boxDestroy(&box1);
        }
        boxa2 = boxaSaveValid(boxa1, L_COPY);
        n2 = boxaGetCount(boxa2);
        boxaDestroy(&boxa1);
        boxa1 = boxa2;
        if (n1 == n2) {
            if (pixadb) pixDestroy(&pix1);
            break;
        }
        n1 = n2;
        if (pixadb) {
            pixRenderBoxaArb(pix1, boxa1, 2, 0, 255, 0);
            pixaAddPix(pixadb, pix1, L_INSERT);
        }
    }

    if (pixadb)
        L_INFO("number of iterations: %d\n", __func__, niters);
    return boxa1;
}


/*!
 * \brief   boxaCombineOverlapsInPair()
 *
 * \param[in]       boxas1     input boxa1
 * \param[in]       boxas2     input boxa2
 * \param[out]      pboxad1    output boxa1
 * \param[out]      pboxad2    output boxa2
 * \param[in,out]   pixadb     debug output
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) One of three things happens to each box in %boxa1 and %boxa2:
 *           * it gets absorbed into a larger box that it overlaps with
 *           * it absorbs a smaller (by area) box that it overlaps with
 *             and gets larger, using the bounding region of the 2 boxes
 *           * it is unchanged (including absorbing smaller boxes that
 *             are contained within it).
 *      (2) If all the boxes from one of the input boxa are absorbed, this
 *          returns an empty boxa.
 *      (3) Input an empty %pixadb, using pixaCreate(0), for debug output
 *      (4) This is useful if different operations are to be carried out
 *          on possibly overlapping rectangular regions, and it is desired
 *          to have only one operation on any rectangular region.
 * </pre>
 */
l_ok
boxaCombineOverlapsInPair(BOXA   *boxas1,
                          BOXA   *boxas2,
                          BOXA  **pboxad1,
                          BOXA  **pboxad2,
                          PIXA   *pixadb)
{
l_int32  i, j, w, h, w2, h2, n1, n2, n1i, n2i, niters;
l_int32  overlap, bigger, area1, area2;
BOX     *box1, *box2, *box3;
BOXA    *boxa1, *boxa2, *boxac1, *boxac2;
PIX     *pix1;

    if (pboxad1) *pboxad1 = NULL;
    if (pboxad2) *pboxad2 = NULL;
    if (!boxas1 || !boxas2)
        return ERROR_INT("boxas1 and boxas2 not both defined", __func__, 1);
    if (!pboxad1 || !pboxad2)
        return ERROR_INT("&boxad1 and &boxad2 not both defined", __func__, 1);

    if (pixadb) {
        boxaGetExtent(boxas1, &w, &h, NULL);
        boxaGetExtent(boxas2, &w2, &h2, NULL);
        w = L_MAX(w, w2);
        h = L_MAX(h, w2);
    }

        /* Let the boxa with the largest area have first crack at the other */
    boxaGetArea(boxas1, &area1);
    boxaGetArea(boxas2, &area2);
    if (area1 >= area2) {
        boxac1 = boxaCopy(boxas1, L_COPY);
        boxac2 = boxaCopy(boxas2, L_COPY);
    } else {
        boxac1 = boxaCopy(boxas2, L_COPY);
        boxac2 = boxaCopy(boxas1, L_COPY);
    }

    n1i = boxaGetCount(boxac1);
    n2i = boxaGetCount(boxac2);
    niters = 0;
    while (1) {
        niters++;
        if (pixadb) {
            pix1 = pixCreate(w + 5, h + 5, 32);
            pixSetAll(pix1);
            pixRenderBoxaArb(pix1, boxac1, 2, 255, 0, 0);
            pixRenderBoxaArb(pix1, boxac2, 2, 0, 255, 0);
            pixaAddPix(pixadb, pix1, L_INSERT);
        }

            /* First combine boxes in each set */
        boxa1 = boxaCombineOverlaps(boxac1, NULL);
        boxa2 = boxaCombineOverlaps(boxac2, NULL);

            /* Now combine boxes between sets */
        n1 = boxaGetCount(boxa1);
        n2 = boxaGetCount(boxa2);
        for (i = 0; i < n1; i++) {  /* 1 eats 2 */
            if ((box1 = boxaGetValidBox(boxa1, i, L_COPY)) == NULL)
                continue;
            for (j = 0; j < n2; j++) {
                if ((box2 = boxaGetValidBox(boxa2, j, L_COPY)) == NULL)
                    continue;
                boxIntersects(box1, box2, &overlap);
                boxCompareSize(box1, box2, L_SORT_BY_AREA, &bigger);
                if (overlap && (bigger == 1)) {
                    box3 = boxBoundingRegion(box1, box2);
                    boxaReplaceBox(boxa1, i, box3);
                    boxaReplaceBox(boxa2, j, boxCreate(0, 0, 0, 0));
                    boxDestroy(&box1);
                    box1 = boxCopy(box3);
                }
                boxDestroy(&box2);
            }
            boxDestroy(&box1);
        }
        for (i = 0; i < n2; i++) {  /* 2 eats 1 */
            if ((box2 = boxaGetValidBox(boxa2, i, L_COPY)) == NULL)
                continue;
            for (j = 0; j < n1; j++) {
                if ((box1 = boxaGetValidBox(boxa1, j, L_COPY)) == NULL)
                    continue;
                boxIntersects(box1, box2, &overlap);
                boxCompareSize(box2, box1, L_SORT_BY_AREA, &bigger);
                if (overlap && (bigger == 1)) {
                    box3 = boxBoundingRegion(box1, box2);
                    boxaReplaceBox(boxa2, i, box3);
                    boxaReplaceBox(boxa1, j, boxCreate(0, 0, 0, 0));
                    boxDestroy(&box2);
                    box2 = boxCopy(box3);
                }
                boxDestroy(&box1);
            }
            boxDestroy(&box2);
        }
        boxaDestroy(&boxac1);
        boxaDestroy(&boxac2);
        boxac1 = boxaSaveValid(boxa1, L_COPY);  /* remove invalid boxes */
        boxac2 = boxaSaveValid(boxa2, L_COPY);
        boxaDestroy(&boxa1);
        boxaDestroy(&boxa2);
        n1 = boxaGetCount(boxac1);
        n2 = boxaGetCount(boxac2);
        if (n1 == n1i && n2 == n2i) break;
        n1i = n1;
        n2i = n2;
        if (pixadb) {
            pix1 = pixCreate(w + 5, h + 5, 32);
            pixSetAll(pix1);
            pixRenderBoxaArb(pix1, boxac1, 2, 255, 0, 0);
            pixRenderBoxaArb(pix1, boxac2, 2, 0, 255, 0);
            pixaAddPix(pixadb, pix1, L_INSERT);
        }
    }

    if (pixadb)
        L_INFO("number of iterations: %d\n", __func__, niters);
    *pboxad1 = boxac1;
    *pboxad2 = boxac2;
    return 0;
}


/*!
 * \brief   boxOverlapRegion()
 *
 * \param[in]    box1, box2
 * \return  box     of overlap region between input boxes;
 *                  NULL if no overlap or on error
 *
 * <pre>
 * Notes:
 *      (1) This is the geometric intersection of the two rectangles.
 * </pre>
 */
BOX *
boxOverlapRegion(BOX  *box1,
                 BOX  *box2)
{
l_int32  l1, l2, r1, r2, t1, t2, b1, b2, w1, h1, w2, h2, ld, td, rd, bd;
l_int32  valid1, valid2;

    if (!box1 || !box2)
        return (BOX *)ERROR_PTR("boxes not both defined", __func__, NULL);
    boxIsValid(box1, &valid1);
    boxIsValid(box2, &valid2);
    if (!valid1 || !valid2) {
        L_WARNING("at least one box is invalid\n", __func__);
        return NULL;
    }

    boxGetGeometry(box1, &l1, &t1, &w1, &h1);
    boxGetGeometry(box2, &l2, &t2, &w2, &h2);
    r1 = l1 + w1 - 1;
    r2 = l2 + w2 - 1;
    b1 = t1 + h1 - 1;
    b2 = t2 + h2 - 1;
    if (b2 < t1 || b1 < t2 || r1 < l2 || r2 < l1)
        return NULL;

    ld = L_MAX(l1, l2);
    td = L_MAX(t1, t2);
    rd = L_MIN(r1, r2);
    bd = L_MIN(b1, b2);
    return boxCreate(ld, td, rd - ld + 1, bd - td + 1);
}


/*!
 * \brief   boxBoundingRegion()
 *
 * \param[in]    box1, box2
 * \return  box  of bounding region containing the input boxes;
 *               NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is the geometric union of the two rectangles.
 *      (2) Invalid boxes are ignored.  This returns an invalid box
 *          if both input boxes are invalid.
 *      (3) For the geometric union of a boxa, use boxaGetExtent().
 * </pre>
 */
BOX *
boxBoundingRegion(BOX  *box1,
                  BOX  *box2)
{
l_int32  l1, l2, r1, r2, t1, t2, b1, b2, w1, h1, w2, h2, ld, td, rd, bd;
l_int32  valid1, valid2;

    if (!box1 || !box2)
        return (BOX *)ERROR_PTR("boxes not both defined", __func__, NULL);
    boxIsValid(box1, &valid1);
    boxIsValid(box2, &valid2);
    if (!valid1 && !valid2) {
        L_WARNING("both boxes are invalid\n", __func__);
        return boxCreate(0, 0, 0, 0);
    }
    if (valid1 && !valid2)
        return boxCopy(box1);
    if (!valid1 && valid2)
        return boxCopy(box2);

    boxGetGeometry(box1, &l1, &t1, &w1, &h1);
    boxGetGeometry(box2, &l2, &t2, &w2, &h2);
    r1 = l1 + w1 - 1;
    r2 = l2 + w2 - 1;
    b1 = t1 + h1 - 1;
    b2 = t2 + h2 - 1;
    ld = L_MIN(l1, l2);
    td = L_MIN(t1, t2);
    rd = L_MAX(r1, r2);
    bd = L_MAX(b1, b2);
    return boxCreate(ld, td, rd - ld + 1, bd - td + 1);
}


/*!
 * \brief   boxOverlapFraction()
 *
 * \param[in]    box1, box2
 * \param[out]   pfract      the fraction of box2 overlapped by box1
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) The result depends on the order of the input boxes,
 *          because the overlap is taken as a fraction of box2.
 *      (2) If at least one box is not valid, there is no overlap.
 * </pre>
 */
l_ok
boxOverlapFraction(BOX        *box1,
                   BOX        *box2,
                   l_float32  *pfract)
{
l_int32  w2, h2, w, h, valid1, valid2;
BOX     *boxo;

    if (!pfract)
        return ERROR_INT("&fract not defined", __func__, 1);
    *pfract = 0.0;
    if (!box1 || !box2)
        return ERROR_INT("boxes not both defined", __func__, 1);
    boxIsValid(box1, &valid1);
    boxIsValid(box2, &valid2);
    if (!valid1 || !valid2) {
        L_WARNING("boxes not both valid\n", __func__);
        return 0;
    }

    if ((boxo = boxOverlapRegion(box1, box2)) == NULL)  /* no overlap */
        return 0;

    boxGetGeometry(box2, NULL, NULL, &w2, &h2);
    boxGetGeometry(boxo, NULL, NULL, &w, &h);
    *pfract = (l_float32)(w * h) / (l_float32)(w2 * h2);
    boxDestroy(&boxo);
    return 0;
}


/*!
 * \brief   boxOverlapArea()
 *
 * \param[in]    box1, box2
 * \param[out]   parea       the number of pixels in the overlap
 * \return  0 if OK, 1 on error.
 */
l_ok
boxOverlapArea(BOX      *box1,
               BOX      *box2,
               l_int32  *parea)
{
l_int32  w, h, valid1, valid2;
BOX     *box;

    if (!parea)
        return ERROR_INT("&area not defined", __func__, 1);
    *parea = 0;
    if (!box1 || !box2)
        return ERROR_INT("boxes not both defined", __func__, 1);
    boxIsValid(box1, &valid1);
    boxIsValid(box2, &valid2);
    if (!valid1 || !valid2)
        return ERROR_INT("boxes not both valid", __func__, 1);

    if ((box = boxOverlapRegion(box1, box2)) == NULL)  /* no overlap */
        return 0;

    boxGetGeometry(box, NULL, NULL, &w, &h);
    *parea = w * h;
    boxDestroy(&box);
    return 0;
}


/*!
 * \brief   boxaHandleOverlaps()
 *
 * \param[in]    boxas
 * \param[in]    op            L_COMBINE, L_REMOVE_SMALL
 * \param[in]    range         forward distance over which overlaps
 *                             are checked; > 0
 * \param[in]    min_overlap   minimum fraction of smaller box required for
 *                             overlap to count; 0.0 to ignore
 * \param[in]    max_ratio     maximum fraction of small/large areas for
 *                             overlap to count; 1.0 to ignore
 * \param[out]   pnamap        [optional] combining map
 * \return  boxad, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) For all n(n-1)/2 box pairings, if two boxes overlap, either:
 *          (a) op == L_COMBINE: get the bounding region for the two,
 *              replace the larger with the bounding region, and remove
 *              the smaller of the two, or
 *          (b) op == L_REMOVE_SMALL: just remove the smaller.
 *      (2) If boxas is 2D sorted, range can be small, but if it is
 *          not spatially sorted, range should be large to allow all
 *          pairwise comparisons to be made.
 *      (3) The %min_overlap parameter allows ignoring small overlaps.
 *          If %min_overlap == 1.0, only boxes fully contained in larger
 *          boxes can be considered for removal; if %min_overlap == 0.0,
 *          this constraint is ignored.
 *      (4) The %max_ratio parameter allows ignoring overlaps between
 *          boxes that are not too different in size.  If %max_ratio == 0.0,
 *          no boxes can be removed; if %max_ratio == 1.0, this constraint
 *          is ignored.
 * </pre>
 */
BOXA *
boxaHandleOverlaps(BOXA    *boxas,
                   l_int32  op,
                   l_int32  range,
                   l_float32  min_overlap,
                   l_float32  max_ratio,
                   NUMA   **pnamap)
{
l_int32    i, j, n, w, h, area1, area2, val;
l_int32    overlap_area;
l_float32  overlap_ratio, area_ratio;
BOX       *box1, *box2, *box3;
BOXA      *boxat, *boxad;
NUMA      *namap;

    if (pnamap) *pnamap = NULL;
    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (op != L_COMBINE && op != L_REMOVE_SMALL)
        return (BOXA *)ERROR_PTR("invalid op", __func__, NULL);

    n = boxaGetCount(boxas);
    if (n == 0)
        return boxaCreate(1);  /* empty */
    if (range == 0) {
        L_WARNING("range is 0\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }

        /* Identify smaller boxes in overlap pairs, and mark to eliminate. */
    namap = numaMakeConstant(-1, n);
    for (i = 0; i < n; i++) {
        if ((box1 = boxaGetValidBox(boxas, i, L_CLONE)) == NULL)
            continue;
        boxGetGeometry(box1, NULL, NULL, &w, &h);
        area1 = w * h;
        if (area1 == 0) {
            boxDestroy(&box1);
            continue;
        }
        for (j = i + 1; j < i + 1 + range && j < n; j++) {
            if ((box2 = boxaGetValidBox(boxas, j, L_CLONE)) == NULL)
                continue;
            boxOverlapArea(box1, box2, &overlap_area);
            if (overlap_area > 0) {
                boxGetGeometry(box2, NULL, NULL, &w, &h);
                area2 = w * h;
                if (area2 == 0) {
                    /* do nothing */
                } else if (area1 >= area2) {
                    overlap_ratio = (l_float32)overlap_area / (l_float32)area2;
                    area_ratio = (l_float32)area2 / (l_float32)area1;
                    if (overlap_ratio >= min_overlap &&
                        area_ratio <= max_ratio) {
                        numaSetValue(namap, j, i);
                    }
                } else {
                    overlap_ratio = (l_float32)overlap_area / (l_float32)area1;
                    area_ratio = (l_float32)area1 / (l_float32)area2;
                    if (overlap_ratio >= min_overlap &&
                        area_ratio <= max_ratio) {
                        numaSetValue(namap, i, j);
                    }
                }
            }
            boxDestroy(&box2);
        }
        boxDestroy(&box1);
    }

    boxat = boxaCopy(boxas, L_COPY);
    if (op == L_COMBINE) {
            /* Resize the larger of the pair to the bounding region */
        for (i = 0; i < n; i++) {
            numaGetIValue(namap, i, &val);
            if (val >= 0) {
                box1 = boxaGetBox(boxas, i, L_CLONE);  /* smaller */
                box2 = boxaGetBox(boxas, val, L_CLONE);  /* larger */
                box3 = boxBoundingRegion(box1, box2);
                boxaReplaceBox(boxat, val, box3);
                boxDestroy(&box1);
                boxDestroy(&box2);
            }
        }
    }

        /* Remove the smaller of the pairs */
    boxad = boxaCreate(n);
    for (i = 0; i < n; i++) {
        numaGetIValue(namap, i, &val);
        if (val == -1) {
            box1 = boxaGetBox(boxat, i, L_COPY);
            boxaAddBox(boxad, box1, L_INSERT);
        }
    }
    boxaDestroy(&boxat);
    if (pnamap)
        *pnamap = namap;
    else
        numaDestroy(&namap);
    return boxad;
}


/*!
 * \brief   boxOverlapDistance()
 *
 * \param[in]    box1, box2    two boxes, in any order
 * \param[out]   ph_ovl        [optional] horizontal overlap
 * \param[out]   pv_ovl        [optional] vertical overlap
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This measures horizontal and vertical overlap of the
 *          two boxes.  Horizontal and vertical overlap are measured
 *          independently.  We need to consider several cases to clarify.
 *      (2) A positive horizontal overlap means that there is at least
 *          one point on the the %box1 boundary with the same x-component
 *          as some point on the %box2 boundary.  Conversely, with a zero
 *          or negative horizontal overlap, there are no boundary pixels
 *          in %box1 that share an x-component with a boundary pixel in %box2.
 *      (3) For a zero or negative horizontal overlap, o <= 0, the minimum
 *          difference in the x-component between pixels on the boundaries
 *          of the two boxes is d = -o + 1.
 *      (4) Likewise for vertical overlaps.
 * </pre>
 */
l_ok
boxOverlapDistance(BOX      *box1,
                   BOX      *box2,
                   l_int32  *ph_ovl,
                   l_int32  *pv_ovl)
{
l_int32  l1, t1, w1, h1, r1, b1, l2, t2, w2, h2, r2, b2, valid1, valid2;

    if (!ph_ovl && !pv_ovl)
        return ERROR_INT("nothing to do", __func__, 1);
    if (ph_ovl) *ph_ovl = 0;
    if (pv_ovl) *pv_ovl = 0;
    if (!box1 || !box2)
        return ERROR_INT("boxes not both defined", __func__, 1);
    boxIsValid(box1, &valid1);
    boxIsValid(box2, &valid2);
    if (!valid1 || !valid2)
        return ERROR_INT("boxes not both valid", __func__, 1);

    if (ph_ovl) {
        boxGetGeometry(box1, &l1, NULL, &w1, NULL);
        boxGetGeometry(box2, &l2, NULL, &w2, NULL);
        r1 = l1 + w1;  /* 1 pixel to the right of box 1 */
        r2 = l2 + w2;
        if (l2 >= l1)
            *ph_ovl = r1 - l2;
        else
            *ph_ovl = r2 - l1;
    }
    if (pv_ovl) {
        boxGetGeometry(box1, NULL, &t1, NULL, &h1);
        boxGetGeometry(box2, NULL, &t2, NULL, &h2);
        b1 = t1 + h1;  /* 1 pixel below box 1 */
        b2 = t2 + h2;
        if (t2 >= t1)
            *pv_ovl = b1 - t2;
        else
            *pv_ovl = b2 - t1;
    }
    return 0;
}


/*!
 * \brief   boxSeparationDistance()
 *
 * \param[in]    box1, box2    two boxes, in any order
 * \param[out]   ph_sep        horizontal separation
 * \param[out]   pv_sep        vertical separation
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This measures the Manhattan distance between the closest points
 *          on the boundaries of the two boxes.  When the boxes overlap
 *          (including touching along a line or at a corner), the
 *          horizontal and vertical distances are 0.
 *      (2) The distances represent the horizontal and vertical separation
 *          of the two boxes.  The boxes have a nonzero intersection when
 *          both the horizontal and vertical overlaps are positive, and
 *          for that case both horizontal and vertical separation
 *          distances are 0.
 *      (3) If the horizontal overlap of the boxes is positive, the
 *          horizontal separation between nearest points on respective
 *          boundaries is 0, and likewise for the vertical overlap.
 *      (4) If the horizontal overlap ho <= 0, the horizontal
 *          separation between nearest points is d = -ho + 1.
 *          Likewise, if the vertical overlap vo <= 0, the vertical
 *          separation between nearest points is d = -vo + 1.
 * </pre>
 */
l_ok
boxSeparationDistance(BOX      *box1,
                      BOX      *box2,
                      l_int32  *ph_sep,
                      l_int32  *pv_sep)
{
l_int32  h_ovl, v_ovl, valid1, valid2;

    if (ph_sep) *ph_sep = 0;
    if (pv_sep) *pv_sep = 0;
    if (!ph_sep || !pv_sep)
        return ERROR_INT("&h_sep and &v_sep not both defined", __func__, 1);
    if (!box1 || !box2)
        return ERROR_INT("boxes not both defined", __func__, 1);
    boxIsValid(box1, &valid1);
    boxIsValid(box2, &valid2);
    if (!valid1 || !valid2)
        return ERROR_INT("boxes not both valid", __func__, 1);

    boxOverlapDistance(box1, box2, &h_ovl, &v_ovl);
    if (h_ovl <= 0)
      *ph_sep = -h_ovl + 1;
    if (v_ovl <= 0)
      *pv_sep = -v_ovl + 1;
    return 0;
}


/*!
 * \brief   boxCompareSize()
 *
 * \param[in]    box1, box2
 * \param[in]    type     L_SORT_BY_WIDTH, L_SORT_BY_HEIGHT,
 *                        L_SORT_BY_MAX_DIMENSION, L_SORT_BY_PERIMETER,
 *                        L_SORT_BY_AREA,
 * \param[out]   prel     1 if box1 > box2, 0 if the same, -1 if box1 < box2
 * \return   0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) We're re-using the SORT enum for these comparisons.
 * </pre>
 */
l_ok
boxCompareSize(BOX      *box1,
               BOX      *box2,
               l_int32   type,
               l_int32  *prel)
{
l_int32  w1, h1, w2, h2, size1, size2, valid1, valid2;

    if (!prel)
        return ERROR_INT("&rel not defined", __func__, 1);
    *prel = 0;
    if (!box1 || !box2)
        return ERROR_INT("boxes not both defined", __func__, 1);
    boxIsValid(box1, &valid1);
    boxIsValid(box2, &valid2);
    if (!valid1 || !valid2)
        return ERROR_INT("boxes not both valid", __func__, 1);
    if (type != L_SORT_BY_WIDTH && type != L_SORT_BY_HEIGHT &&
        type != L_SORT_BY_MAX_DIMENSION && type != L_SORT_BY_PERIMETER &&
        type != L_SORT_BY_AREA)
        return ERROR_INT("invalid compare type", __func__, 1);

    boxGetGeometry(box1, NULL, NULL, &w1, &h1);
    boxGetGeometry(box2, NULL, NULL, &w2, &h2);
    if (type == L_SORT_BY_WIDTH) {
        *prel = (w1 > w2) ? 1 : ((w1 == w2) ? 0 : -1);
    } else if (type == L_SORT_BY_HEIGHT) {
        *prel = (h1 > h2) ? 1 : ((h1 == h2) ? 0 : -1);
    } else if (type == L_SORT_BY_MAX_DIMENSION) {
        size1 = L_MAX(w1, h1);
        size2 = L_MAX(w2, h2);
        *prel = (size1 > size2) ? 1 : ((size1 == size2) ? 0 : -1);
    } else if (type == L_SORT_BY_PERIMETER) {
        size1 = w1 + h1;
        size2 = w2 + h2;
        *prel = (size1 > size2) ? 1 : ((size1 == size2) ? 0 : -1);
    } else if (type == L_SORT_BY_AREA) {
        size1 = w1 * h1;
        size2 = w2 * h2;
        *prel = (size1 > size2) ? 1 : ((size1 == size2) ? 0 : -1);
    }
    return 0;
}


/*!
 * \brief   boxContainsPt()
 *
 * \param[in]    box
 * \param[in]    x, y        a point
 * \param[out]   pcontains   1 if box contains point; 0 otherwise
 * \return  0 if OK, 1 on error.
 */
l_ok
boxContainsPt(BOX       *box,
              l_float32  x,
              l_float32  y,
              l_int32   *pcontains)
{
l_int32  bx, by, bw, bh;

    if (!pcontains)
        return ERROR_INT("&contains not defined", __func__, 1);
    *pcontains = 0;
    if (!box)
        return ERROR_INT("&box not defined", __func__, 1);
    boxGetGeometry(box, &bx, &by, &bw, &bh);
    if (x >= bx && x < bx + bw && y >= by && y < by + bh)
        *pcontains = 1;
    return 0;
}


/*!
 * \brief   boxaGetNearestToPt()
 *
 * \param[in]    boxa
 * \param[in]    x, y    point
 * \return  box   with centroid closest to the given point [x,y],
 *                or NULL if no boxes in boxa
 *
 * <pre>
 * Notes:
 *      (1) Uses euclidean distance between centroid and point.
 * </pre>
 */
BOX *
boxaGetNearestToPt(BOXA    *boxa,
                   l_int32  x,
                   l_int32  y)
{
l_int32    i, n, minindex;
l_float32  delx, dely, dist, mindist, cx, cy;
BOX       *box;

    if (!boxa)
        return (BOX *)ERROR_PTR("boxa not defined", __func__, NULL);
    if ((n = boxaGetCount(boxa)) == 0)
        return (BOX *)ERROR_PTR("n = 0", __func__, NULL);

    mindist = 1000000000.;
    minindex = 0;
    for (i = 0; i < n; i++) {
        if ((box = boxaGetValidBox(boxa, i, L_CLONE)) == NULL)
            continue;
        boxGetCenter(box, &cx, &cy);
        delx = (l_float32)(cx - x);
        dely = (l_float32)(cy - y);
        dist = delx * delx + dely * dely;
        if (dist < mindist) {
            minindex = i;
            mindist = dist;
        }
        boxDestroy(&box);
    }

    return boxaGetBox(boxa, minindex, L_COPY);
}


/*!
 * \brief   boxaGetNearestToLine()
 *
 * \param[in]    boxa
 * \param[in]    x, y   (y = -1 for vertical line; x = -1 for horiz line)
 * \return  box  with centroid closest to the given line,
 *               or NULL if no boxes in boxa
 *
 * <pre>
 * Notes:
 *      (1) For a horizontal line at some value y, get the minimum of the
 *          distance |yc - y| from the box centroid yc value to y;
 *          likewise minimize |xc - x| for a vertical line at x.
 *      (2) Input y < 0, x >= 0 to indicate a vertical line at x, and
 *          x < 0, y >= 0 for a horizontal line at y.
 * </pre>
 */
BOX *
boxaGetNearestToLine(BOXA    *boxa,
                     l_int32  x,
                     l_int32  y)
{
l_int32    i, n, minindex;
l_float32  dist, mindist, cx, cy;
BOX       *box;

    if (!boxa)
        return (BOX *)ERROR_PTR("boxa not defined", __func__, NULL);
    if ((n = boxaGetCount(boxa)) == 0)
        return (BOX *)ERROR_PTR("n = 0", __func__, NULL);
    if (y >= 0 && x >= 0)
        return (BOX *)ERROR_PTR("either x or y must be < 0", __func__, NULL);
    if (y < 0 && x < 0)
        return (BOX *)ERROR_PTR("either x or y must be >= 0", __func__, NULL);

    mindist = 1000000000.;
    minindex = 0;
    for (i = 0; i < n; i++) {
        if ((box = boxaGetValidBox(boxa, i, L_CLONE)) == NULL)
            continue;
        boxGetCenter(box, &cx, &cy);
        if (x >= 0)
            dist = L_ABS(cx - (l_float32)x);
        else  /* y >= 0 */
            dist = L_ABS(cy - (l_float32)y);
        if (dist < mindist) {
            minindex = i;
            mindist = dist;
        }
        boxDestroy(&box);
    }

    return boxaGetBox(boxa, minindex, L_COPY);
}


/*!
 * \brief   boxaFindNearestBoxes()
 *
 * \param[in]    boxa         either unsorted, or 2D sorted in LR/TB scan order
 * \param[in]    dist_select  L_NON_NEGATIVE, L_ALL
 * \param[in]    range        search distance from box i; use 0 to search
 *                            entire boxa (e.g., if it's not 2D sorted)
 * \param[out]   pnaaindex    for each box in %boxa, contains a numa of 4
 *                            box indices (per direction) of the nearest box
 * \param[out]   pnaadist     for each box in %boxa, this contains a numa
 * \return  0 if OK, 1 on error
 * <pre>
 * Notes:
 *      (1) See boxaGetNearestByDirection() for usage of %dist_select
 *          and %range.
 * </pre>
 */
l_ok
boxaFindNearestBoxes(BOXA     *boxa,
                     l_int32   dist_select,
                     l_int32   range,
                     NUMAA   **pnaaindex,
                     NUMAA   **pnaadist)
{
l_int32  i, n, index, dist;
NUMA    *nai, *nad;
NUMAA   *naai, *naad;

    if (pnaaindex) *pnaaindex = NULL;
    if (pnaadist) *pnaadist = NULL;
    if (!pnaaindex)
        return ERROR_INT("&naaindex not defined", __func__, 1);
    if (!pnaadist)
        return ERROR_INT("&naadist not defined", __func__, 1);
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    n = boxaGetCount(boxa);
    naai = numaaCreate(n);
    naad = numaaCreate(n);
    *pnaaindex = naai;
    *pnaadist = naad;
    for (i = 0; i < n; i++) {
        nai = numaCreate(4);
        nad = numaCreate(4);
        boxaGetNearestByDirection(boxa, i, L_FROM_LEFT, dist_select,
                                  range, &index, &dist);
        numaAddNumber(nai, index);
        numaAddNumber(nad, dist);
        boxaGetNearestByDirection(boxa, i, L_FROM_RIGHT, dist_select,
                                  range, &index, &dist);
        numaAddNumber(nai, index);
        numaAddNumber(nad, dist);
        boxaGetNearestByDirection(boxa, i, L_FROM_TOP, dist_select,
                                  range, &index, &dist);
        numaAddNumber(nai, index);
        numaAddNumber(nad, dist);
        boxaGetNearestByDirection(boxa, i, L_FROM_BOT, dist_select,
                                  range, &index, &dist);
        numaAddNumber(nai, index);
        numaAddNumber(nad, dist);
        numaaAddNuma(naai, nai, L_INSERT);
        numaaAddNuma(naad, nad, L_INSERT);
    }
    return 0;
}


/*!
 * \brief   boxaGetNearestByDirection()
 *
 * \param[in]    boxa         either unsorted, or 2D sorted in LR/TB scan order
 * \param[in]    i            box we test against
 * \param[in]    dir          direction to look: L_FROM_LEFT, L_FROM_RIGHT,
 *                            L_FROM_TOP, L_FROM_BOT
 * \param[in]    dist_select  L_NON_NEGATIVE, L_ALL
 * \param[in]    range        search distance from box i; use 0 to search
 *                            entire boxa (e.g., if it's not 2D sorted)
 * \param[out]   pindex       index in boxa of nearest box with overlapping
 *                            coordinates in the indicated direction;
 *                            -1 if there is no box
 * \param[out]   pdist        distance of the nearest box in the indicated
 *                            direction; 100000 if no box
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) For efficiency, use a LR/TD sorted %boxa, which can be
 *          made by flattening a 2D sorted boxaa.  In that case,
 *          %range can be some positive integer like 50.
 *      (2) If boxes overlap, the distance will be < 0.  Use %dist_select
 *          to determine if these should count or not.  If L_ALL, then
 *          one box will match as the nearest to another in 2 or more
 *          directions.
 * </pre>
 */
l_ok
boxaGetNearestByDirection(BOXA     *boxa,
                          l_int32   i,
                          l_int32   dir,
                          l_int32   dist_select,
                          l_int32   range,
                          l_int32  *pindex,
                          l_int32  *pdist)
{
l_int32  j, jmin, jmax, n, mindist, dist, index;
l_int32  x, y, w, h, bx, by, bw, bh;

    if (pindex) *pindex = -1;
    if (pdist) *pdist = 100000;
    if (!pindex)
        return ERROR_INT("&index not defined", __func__, 1);
    if (!pdist)
        return ERROR_INT("&dist not defined", __func__, 1);
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if (dir != L_FROM_LEFT && dir != L_FROM_RIGHT &&
        dir != L_FROM_TOP && dir != L_FROM_BOT)
        return ERROR_INT("invalid dir", __func__, 1);
    if (dist_select != L_NON_NEGATIVE && dist_select != L_ALL)
        return ERROR_INT("invalid dist_select", __func__, 1);
    n = boxaGetCount(boxa);
    if (i < 0 || i >= n)
        return ERROR_INT("invalid box index", __func__, 1);

    jmin = (range <= 0) ? 0 : L_MAX(0, i - range);
    jmax = (range <= 0) ? n - 1 : L_MIN(n -1, i + range);
    boxaGetBoxGeometry(boxa, i, &x, &y, &w, &h);
    mindist = 100000;
    index = -1;
    if (dir == L_FROM_LEFT || dir == L_FROM_RIGHT) {
        for (j = jmin; j <= jmax; j++) {
            if (j == i) continue;
            boxaGetBoxGeometry(boxa, j, &bx, &by, &bw, &bh);
            if ((bx >= x && dir == L_FROM_LEFT) ||  /* not to the left */
                (x >= bx && dir == L_FROM_RIGHT))   /* not to the right */
                continue;
            if (boxHasOverlapInXorY(y, h, by, bh) == 1) {
                dist = boxGetDistanceInXorY(x, w, bx, bw);
                if (dist_select == L_NON_NEGATIVE && dist < 0) continue;
                if (dist < mindist) {
                    mindist = dist;
                    index = j;
                }
            }
        }
    } else if (dir == L_FROM_TOP || dir == L_FROM_BOT) {
        for (j = jmin; j <= jmax; j++) {
            if (j == i) continue;
            boxaGetBoxGeometry(boxa, j, &bx, &by, &bw, &bh);
            if ((by >= y && dir == L_FROM_TOP) ||  /* not above */
                (y >= by && dir == L_FROM_BOT))   /* not below */
                continue;
            if (boxHasOverlapInXorY(x, w, bx, bw) == 1) {
                dist = boxGetDistanceInXorY(y, h, by, bh);
                if (dist_select == L_NON_NEGATIVE && dist < 0) continue;
                if (dist < mindist) {
                    mindist = dist;
                    index = j;
                }
            }
        }
    }
    *pindex = index;
    *pdist = mindist;
    return 0;
}


/*!
 * \brief   boxHasOverlapInXorY()
 *
 * \param[in]    c1   left or top coordinate of box1
 * \param[in]    s1   width or height of box1
 * \param[in]    c2   left or top coordinate of box2
 * \param[in]    s2   width or height of box2
 * \return  0 if no overlap; 1 if any overlap
 *
 * <pre>
 * Notes:
 *      (1) Like boxGetDistanceInXorY(), this is used for overlaps both in
 *          x (which projected vertically) and in y (projected horizontally)
 * </pre>
 */
static l_int32
boxHasOverlapInXorY(l_int32  c1,
                    l_int32  s1,
                    l_int32  c2,
                    l_int32  s2)
{
l_int32  ovlp;

    if (c1 > c2)
        ovlp = c2 + s2 - 1 - c1;
    else
        ovlp = c1 + s1 - 1 - c2;
    return (ovlp < 0) ? 0 : 1;
}


/*!
 * \brief   boxGetDistanceInXorY()
 *
 * \param[in]    c1   left or top coordinate of box1
 * \param[in]    s1   width or height of box1
 * \param[in]    c2   left or top coordinate of box2
 * \param[in]    s2   width or height of box2
 * \return  distance between them (if < 0, box2 overlaps box1 in the
 *                                 dimension considered)
 */
static l_int32
boxGetDistanceInXorY(l_int32  c1,
                     l_int32  s1,
                     l_int32  c2,
                     l_int32  s2)
{
l_int32  dist;

    if (c1 > c2)
        dist = c1 - (c2 + s2 - 1);
    else
        dist = c2 - (c1 + s1 - 1);
    return dist;
}


/*!
 * \brief   boxGetCenter()
 *
 * \param[in]    box
 * \param[out]   pcx, pcy location of center of box
 * \return  0 if OK, 1 on error or if box is not valid
 */
l_ok
boxGetCenter(const BOX  *box,
             l_float32  *pcx,
             l_float32  *pcy)
{
l_int32  x, y, w, h;

    if (pcx) *pcx = 0;
    if (pcy) *pcy = 0;
    if (!pcx || !pcy)
        return ERROR_INT("&cx, &cy not both defined", __func__, 1);
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);
    boxGetGeometry(box, &x, &y, &w, &h);
    if (w == 0 || h == 0) return 1;
    *pcx = (l_float32)(x + 0.5 * w);
    *pcy = (l_float32)(y + 0.5 * h);

    return 0;
}


/*!
 * \brief   boxIntersectByLine()
 *
 * \param[in]    box
 * \param[in]    x, y point that line goes through
 * \param[in]    slope of line
 * \param[out]   px1, py1 1st point of intersection with box
 * \param[out]   px2, py2 2nd point of intersection with box
 * \param[out]   pn number of points of intersection
 * \return  0 if OK, 1 on error or if box is not valid
 *
 * <pre>
 * Notes:
 *      (1) If the intersection is at only one point (a corner), the
 *          coordinates are returned in (x1, y1).
 *      (2) Represent a vertical line by one with a large but finite slope.
 * </pre>
 */
l_ok
boxIntersectByLine(const BOX *box,
                   l_int32    x,
                   l_int32    y,
                   l_float32  slope,
                   l_int32   *px1,
                   l_int32   *py1,
                   l_int32   *px2,
                   l_int32   *py2,
                   l_int32   *pn)
{
l_int32    bx, by, bw, bh, xp, yp, xt, yt, i, n;
l_float32  invslope;
PTA       *pta;

    if (px1) *px1 = 0;
    if (px2) *px2 = 0;
    if (py1) *py1 = 0;
    if (py2) *py2 = 0;
    if (pn) *pn = 0;
    if (!px1 || !py1 || !px2 || !py2)
        return ERROR_INT("&x1, &y1, &x2, &y2 not all defined", __func__, 1);
    if (!pn)
        return ERROR_INT("&n not defined", __func__, 1);
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);
    boxGetGeometry(box, &bx, &by, &bw, &bh);
    if (bw == 0 || bh == 0) return 1;

    if (slope == 0.0) {
        if (y >= by && y < by + bh) {
            *py1 = *py2 = y;
            *px1 = bx;
            *px2 = bx + bw - 1;
        }
        return 0;
    }

    if (slope > 1000000.0) {
        if (x >= bx && x < bx + bw) {
            *px1 = *px2 = x;
            *py1 = by;
            *py2 = by + bh - 1;
        }
        return 0;
    }

        /* Intersection with top and bottom lines of box */
    pta = ptaCreate(2);
    invslope = 1.0 / slope;
    xp = (l_int32)(x + invslope * (y - by));
    if (xp >= bx && xp < bx + bw)
        ptaAddPt(pta, xp, by);
    xp = (l_int32)(x + invslope * (y - by - bh + 1));
    if (xp >= bx && xp < bx + bw)
        ptaAddPt(pta, xp, by + bh - 1);

        /* Intersection with left and right lines of box */
    yp = (l_int32)(y + slope * (x - bx));
    if (yp >= by && yp < by + bh)
        ptaAddPt(pta, bx, yp);
    yp = (l_int32)(y + slope * (x - bx - bw + 1));
    if (yp >= by && yp < by + bh)
        ptaAddPt(pta, bx + bw - 1, yp);

        /* There is a maximum of 2 unique points; remove duplicates.  */
    n = ptaGetCount(pta);
    if (n > 0) {
        ptaGetIPt(pta, 0, px1, py1);  /* accept the first one */
        *pn = 1;
    }
    for (i = 1; i < n; i++) {
        ptaGetIPt(pta, i, &xt, &yt);
        if ((*px1 != xt) || (*py1 != yt)) {
            *px2 = xt;
            *py2 = yt;
            *pn = 2;
            break;
        }
    }

    ptaDestroy(&pta);
    return 0;
}


/*!
 * \brief   boxClipToRectangle()
 *
 * \param[in]    box
 * \param[in]    wi, hi rectangle representing image
 * \return  part of box within given rectangle, or NULL on error
 *          or if box is entirely outside the rectangle
 *
 * <pre>
 * Notes:
 *      (1) This can be used to clip a rectangle to an image.
 *          The clipping rectangle is assumed to have a UL corner at (0, 0),
 *          and a LR corner at (wi - 1, hi - 1).
 * </pre>
 */
BOX *
boxClipToRectangle(BOX     *box,
                   l_int32  wi,
                   l_int32  hi)
{
BOX  *boxd;

    if (!box)
        return (BOX *)ERROR_PTR("box not defined", __func__, NULL);
    if (box->x >= wi || box->y >= hi ||
        box->x + box->w <= 0 || box->y + box->h <= 0)
        return (BOX *)ERROR_PTR("box outside rectangle", __func__, NULL);

    boxd = boxCopy(box);
    if (boxd->x < 0) {
        boxd->w += boxd->x;
        boxd->x = 0;
    }
    if (boxd->y < 0) {
        boxd->h += boxd->y;
        boxd->y = 0;
    }
    if (boxd->x + boxd->w > wi)
        boxd->w = wi - boxd->x;
    if (boxd->y + boxd->h > hi)
        boxd->h = hi - boxd->y;
    return boxd;
}


/*!
 * \brief   boxClipToRectangleParams()
 *
 * \param[in]    box [optional] requested box; can be null
 * \param[in]    w, h clipping box size; typ. the size of an image
 * \param[out]   pxstart start x coordinate
 * \param[out]   pystart start y coordinate
 * \param[out]   pxend one pixel beyond clipping box
 * \param[out]   pyend one pixel beyond clipping box
 * \param[out]   pbw [optional] clipped width
 * \param[out]   pbh [optional] clipped height
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The return value should be checked.  If it is 1, the
 *          returned parameter values are bogus.
 *      (2) This simplifies the selection of pixel locations within
 *          a given rectangle:
 *             for (i = ystart; i < yend; i++ {
 *                 ...
 *                 for (j = xstart; j < xend; j++ {
 *                     ....
 * </pre>
 */
l_ok
boxClipToRectangleParams(BOX      *box,
                         l_int32   w,
                         l_int32   h,
                         l_int32  *pxstart,
                         l_int32  *pystart,
                         l_int32  *pxend,
                         l_int32  *pyend,
                         l_int32  *pbw,
                         l_int32  *pbh)
{
l_int32  bw, bh;
BOX     *boxc;

    if (pxstart) *pxstart = 0;
    if (pystart) *pystart = 0;
    if (pxend) *pxend = w;
    if (pyend) *pyend = h;
    if (pbw) *pbw = w;
    if (pbh) *pbh = h;
    if (!pxstart || !pystart || !pxend || !pyend)
        return ERROR_INT("invalid ptr input", __func__, 1);
    if (!box) return 0;

    if ((boxc = boxClipToRectangle(box, w, h)) == NULL)
        return ERROR_INT("box outside image", __func__, 1);
    boxGetGeometry(boxc, pxstart, pystart, &bw, &bh);
    boxDestroy(&boxc);

    if (pbw) *pbw = bw;
    if (pbh) *pbh = bh;
    if (bw == 0 || bh == 0)
        return ERROR_INT("invalid clipping box", __func__, 1);
    *pxend = *pxstart + bw;  /* 1 past the end */
    *pyend = *pystart + bh;  /* 1 past the end */
    return 0;
}


/*!
 * \brief   boxRelocateOneSide()
 *
 * \param[in]    boxd [optional]; this can be null, equal to boxs,
 *                    or different from boxs;
 * \param[in]    boxs starting box; to have one side relocated
 * \param[in]    loc new location of the side that is changing
 * \param[in]    sideflag L_FROM_LEFT, etc., indicating the side that moves
 * \return  boxd, or NULL on error or if the computed boxd has
 *          width or height <= 0.
 *
 * <pre>
 * Notes:
 *      (1) Set boxd == NULL to get new box; boxd == boxs for in-place;
 *          or otherwise to resize existing boxd.
 *      (2) For usage, suggest one of these:
 *               boxd = boxRelocateOneSide(NULL, boxs, ...);   // new
 *               boxRelocateOneSide(boxs, boxs, ...);          // in-place
 *               boxRelocateOneSide(boxd, boxs, ...);          // other
 * </pre>
 */
BOX *
boxRelocateOneSide(BOX     *boxd,
                   BOX     *boxs,
                   l_int32  loc,
                   l_int32  sideflag)
{
l_int32  x, y, w, h;

    if (!boxs)
        return (BOX *)ERROR_PTR("boxs not defined", __func__, NULL);
    if (!boxd)
        boxd = boxCopy(boxs);

    boxGetGeometry(boxs, &x, &y, &w, &h);
    if (w == 0 || h == 0)
        return boxd;
    if (sideflag == L_FROM_LEFT)
        boxSetGeometry(boxd, loc, -1, w + x - loc, -1);
    else if (sideflag == L_FROM_RIGHT)
        boxSetGeometry(boxd, -1, -1, loc - x + 1, -1);
    else if (sideflag == L_FROM_TOP)
        boxSetGeometry(boxd, -1, loc, -1, h + y - loc);
    else if (sideflag == L_FROM_BOT)
        boxSetGeometry(boxd, -1, -1, -1, loc - y + 1);
    return boxd;
}


/*!
 * \brief   boxaAdjustSides()
 *
 * \param[in]    boxas
 * \param[in]    delleft, delright, deltop, delbot   changes in location of
 *                                                   each side for each box
 * \return  boxad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) New box dimensions are cropped at left and top to x >= 0 and y >= 0.
 *      (2) If the width or height of a box goes to 0, we generate a box with
 *          w == 1 and h == 1, as a placeholder.
 *      (3) See boxAdjustSides().
 * </pre>
 */
BOXA *
boxaAdjustSides(BOXA    *boxas,
                l_int32  delleft,
                l_int32  delright,
                l_int32  deltop,
                l_int32  delbot)
{
l_int32  n, i, x, y;
BOX     *box1, *box2;
BOXA    *boxad;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);

    n = boxaGetCount(boxas);
    boxad = boxaCreate(n);
    for (i = 0; i < n; i++) {
        box1 = boxaGetBox(boxas, i, L_COPY);
        box2 = boxAdjustSides(NULL, box1, delleft, delright, deltop, delbot);
        if (!box2) {
            boxGetGeometry(box1, &x, &y, NULL, NULL);
            box2 = boxCreate(x, y, 1, 1);
        }
        boxaAddBox(boxad, box2, L_INSERT);
        boxDestroy(&box1);
    }

    return boxad;
}


/*!
 * \brief   boxaAdjustBoxSides()
 *
 * \param[in]    boxas
 * \param[in]    index
 * \param[in]    delleft, delright, deltop, delbot   changes to box side locs
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) In-place operation on a box in a boxa.
 *      (2) New box dimensions are cropped at left and top to x >= 0 and y >= 0.
 *      (3) If a box ends up with no area, an error message is emitted,
 *          but the box dimensions are not changed.
 *      (4) See boxaAdjustSides().
 * </pre>
 */
l_ok
boxaAdjustBoxSides(BOXA    *boxa,
                   l_int32  index,
                   l_int32  delleft,
                   l_int32  delright,
                   l_int32  deltop,
                   l_int32  delbot)
{
BOX  *box;

    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    if ((box = boxaGetBox(boxa, index, L_CLONE)) == NULL)
        return ERROR_INT("invalid index", __func__, 1);

    boxAdjustSides(box, box, delleft, delright, deltop, delbot);
    boxDestroy(&box);  /* the clone */
    return 0;
}


/*!
 * \brief   boxAdjustSides()
 *
 * \param[in]    boxd     [optional]; this can be null, equal to boxs,
 *                        or different from boxs
 * \param[in]    boxs     starting box; to have sides adjusted
 * \param[in]    delleft, delright, deltop, delbot    changes in location
 *                                                    of each side
 * \return  boxd, or NULL on error or if the computed boxd has
 *          width or height <= 0.
 *
 * <pre>
 * Notes:
 *      (1) Set boxd == NULL to get new box; boxd == boxs for in-place;
 *          or otherwise to resize existing boxd.
 *      (2) For usage, suggest one of these:
 *               boxd = boxAdjustSides(NULL, boxs, ...);   // new
 *               boxAdjustSides(boxs, boxs, ...);          // in-place
 *               boxAdjustSides(boxd, boxs, ...);          // other
 *      (3) New box dimensions are cropped at left and top to x >= 0 and y >= 0.
 *      (4) For example, to expand in-place by 20 pixels on each side, use
 *             boxAdjustSides(box, box, -20, 20, -20, 20);
 * </pre>
 */
BOX *
boxAdjustSides(BOX     *boxd,
               BOX     *boxs,
               l_int32  delleft,
               l_int32  delright,
               l_int32  deltop,
               l_int32  delbot)
{
l_int32  x, y, w, h, xl, xr, yt, yb, wnew, hnew;

    if (!boxs)
        return (BOX *)ERROR_PTR("boxs not defined", __func__, NULL);

    boxGetGeometry(boxs, &x, &y, &w, &h);
    xl = L_MAX(0, x + delleft);
    yt = L_MAX(0, y + deltop);
    xr = x + w + delright;  /* one pixel beyond right edge */
    yb = y + h + delbot;    /* one pixel below bottom edge */
    wnew = xr - xl;
    hnew = yb - yt;

    if (wnew < 1 || hnew < 1)
        return (BOX *)ERROR_PTR("boxd has 0 area", __func__, NULL);
    if (!boxd)
        return boxCreate(xl, yt, wnew, hnew);

    boxSetGeometry(boxd, xl, yt, wnew, hnew);
    return boxd;
}


/*!
 * \brief   boxaSetSide()
 *
 * \param[in]    boxad    use NULL to get a new one; same as boxas for in-place
 * \param[in]    boxas
 * \param[in]    side     L_SET_LEFT, L_SET_RIGHT, L_SET_TOP, L_SET_BOT
 * \param[in]    val      location to set for given side, for each box
 * \param[in]    thresh   min abs difference to cause resetting to %val
 * \return  boxad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Sets the given side of each box.  Use boxad == NULL for a new
 *          boxa, and boxad == boxas for in-place.
 *      (2) Use one of these:
 *               boxad = boxaSetSide(NULL, boxas, ...);   // new
 *               boxaSetSide(boxas, boxas, ...);  // in-place
 * </pre>
 */
BOXA *
boxaSetSide(BOXA    *boxad,
            BOXA    *boxas,
            l_int32  side,
            l_int32  val,
            l_int32  thresh)
{
l_int32  n, i;
BOX     *box;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (boxad && (boxas != boxad))
        return (BOXA *)ERROR_PTR("not in-place", __func__, NULL);
    if (side != L_SET_LEFT && side != L_SET_RIGHT &&
        side != L_SET_TOP && side != L_SET_BOT)
        return (BOXA *)ERROR_PTR("invalid side", __func__, NULL);
    if (val < 0)
        return (BOXA *)ERROR_PTR("val < 0", __func__, NULL);

    if (!boxad)
        boxad = boxaCopy(boxas, L_COPY);
    n = boxaGetCount(boxad);
    for (i = 0; i < n; i++) {
        box = boxaGetBox(boxad, i, L_CLONE);
        boxSetSide(box, side, val, thresh);
        boxDestroy(&box);  /* the clone */
    }

    return boxad;
}


/*!
 * \brief   boxSetSide()
 *
 * \param[in]    boxs
 * \param[in]    side     L_SET_LEFT, L_SET_RIGHT, L_SET_TOP, L_SET_BOT
 * \param[in]    val      location to set for given side, for each box
 * \param[in]    thresh   min abs difference to cause resetting to %val
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) In-place operation.
 *      (2) Use %thresh = 0 to definitely set the side to %val.
 * </pre>
 */
l_ok
boxSetSide(BOX     *boxs,
           l_int32  side,
           l_int32  val,
           l_int32  thresh)
{
l_int32  x, y, w, h, diff;

    if (!boxs)
        return ERROR_INT("box not defined", __func__, 1);
    if (side != L_SET_LEFT && side != L_SET_RIGHT &&
        side != L_SET_TOP && side != L_SET_BOT)
        return ERROR_INT("invalid side", __func__, 1);
    if (val < 0)
        return ERROR_INT("val < 0", __func__, 1);

    boxGetGeometry(boxs, &x, &y, &w, &h);
    if (side == L_SET_LEFT) {
        diff = x - val;
        if (L_ABS(diff) >= thresh)
            boxSetGeometry(boxs, val, y, w + diff, h);
    } else if (side == L_SET_RIGHT) {
        diff = x + w -1 - val;
        if (L_ABS(diff) >= thresh)
            boxSetGeometry(boxs, x, y, val - x + 1, h);
    } else if (side == L_SET_TOP) {
        diff = y - val;
        if (L_ABS(diff) >= thresh)
            boxSetGeometry(boxs, x, val, w, h + diff);
    } else { /* side == L_SET_BOT */
        diff = y + h - 1 - val;
        if (L_ABS(diff) >= thresh)
            boxSetGeometry(boxs, x, y, w, val - y + 1);
    }

    return 0;
}


/*!
 * \brief   boxaAdjustWidthToTarget()
 *
 * \param[in]    boxad    use NULL to get a new one; same as boxas for in-place
 * \param[in]    boxas
 * \param[in]    sides    L_ADJUST_LEFT, L_ADJUST_RIGHT, L_ADJUST_LEFT_AND_RIGHT
 * \param[in]    target   target width if differs by more than thresh
 * \param[in]    thresh   min abs difference in width to cause adjustment
 * \return  boxad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Conditionally adjusts the width of each box, by moving
 *          the indicated edges (left and/or right) if the width differs
 *          by %thresh or more from %target.
 *      (2) Use boxad == NULL for a new boxa, and boxad == boxas for in-place.
 *          Use one of these:
 *               boxad = boxaAdjustWidthToTarget(NULL, boxas, ...);   // new
 *               boxaAdjustWidthToTarget(boxas, boxas, ...);  // in-place
 * </pre>
 */
BOXA *
boxaAdjustWidthToTarget(BOXA    *boxad,
                        BOXA    *boxas,
                        l_int32  sides,
                        l_int32  target,
                        l_int32  thresh)
{
l_int32  x, y, w, h, n, i, diff;
BOX     *box;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (boxad && (boxas != boxad))
        return (BOXA *)ERROR_PTR("not in-place", __func__, NULL);
    if (sides != L_ADJUST_LEFT && sides != L_ADJUST_RIGHT &&
        sides != L_ADJUST_LEFT_AND_RIGHT)
        return (BOXA *)ERROR_PTR("invalid sides", __func__, NULL);
    if (target < 1)
        return (BOXA *)ERROR_PTR("target < 1", __func__, NULL);

    if (!boxad)
        boxad = boxaCopy(boxas, L_COPY);
    n = boxaGetCount(boxad);
    for (i = 0; i < n; i++) {
        if ((box = boxaGetValidBox(boxad, i, L_CLONE)) == NULL)
            continue;
        boxGetGeometry(box, &x, &y, &w, &h);
        diff = w - target;
        if (sides == L_ADJUST_LEFT) {
            if (L_ABS(diff) >= thresh)
                boxSetGeometry(box, L_MAX(0, x + diff), y, target, h);
        } else if (sides == L_ADJUST_RIGHT) {
            if (L_ABS(diff) >= thresh)
                boxSetGeometry(box, x, y, target, h);
        } else { /* sides == L_ADJUST_LEFT_AND_RIGHT */
            if (L_ABS(diff) >= thresh)
                boxSetGeometry(box, L_MAX(0, x + diff/2), y, target, h);
        }
        boxDestroy(&box);
    }

    return boxad;
}


/*!
 * \brief   boxaAdjustHeightToTarget()
 *
 * \param[in]    boxad    use NULL to get a new one
 * \param[in]    boxas
 * \param[in]    sides    L_ADJUST_TOP, L_ADJUST_BOT, L_ADJUST_TOP_AND_BOT
 * \param[in]    target   target height if differs by more than thresh
 * \param[in]    thresh   min abs difference in height to cause adjustment
 * \return  boxad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Conditionally adjusts the height of each box, by moving
 *          the indicated edges (top and/or bot) if the height differs
 *          by %thresh or more from %target.
 *      (2) Use boxad == NULL for a new boxa, and boxad == boxas for in-place.
 *          Use one of these:
 *               boxad = boxaAdjustHeightToTarget(NULL, boxas, ...);   // new
 *               boxaAdjustHeightToTarget(boxas, boxas, ...);  // in-place
 * </pre>
 */
BOXA *
boxaAdjustHeightToTarget(BOXA    *boxad,
                         BOXA    *boxas,
                        l_int32  sides,
                        l_int32  target,
                        l_int32  thresh)
{
l_int32  x, y, w, h, n, i, diff;
BOX     *box;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (boxad && (boxas != boxad))
        return (BOXA *)ERROR_PTR("not in-place", __func__, NULL);
    if (sides != L_ADJUST_TOP && sides != L_ADJUST_BOT &&
        sides != L_ADJUST_TOP_AND_BOT)
        return (BOXA *)ERROR_PTR("invalid sides", __func__, NULL);
    if (target < 1)
        return (BOXA *)ERROR_PTR("target < 1", __func__, NULL);

    if (!boxad)
        boxad = boxaCopy(boxas, L_COPY);
    n = boxaGetCount(boxad);
    for (i = 0; i < n; i++) {
        if ((box = boxaGetValidBox(boxad, i, L_CLONE)) == NULL)
            continue;
        boxGetGeometry(box, &x, &y, &w, &h);
        diff = h - target;
        if (sides == L_ADJUST_TOP) {
            if (L_ABS(diff) >= thresh)
                boxSetGeometry(box, x, L_MAX(0, y + diff), w, target);
        } else if (sides == L_ADJUST_BOT) {
            if (L_ABS(diff) >= thresh)
                boxSetGeometry(box, x, y, w, target);
        } else { /* sides == L_ADJUST_TOP_AND_BOT */
            if (L_ABS(diff) >= thresh)
                boxSetGeometry(box, x, L_MAX(0, y + diff/2), w, target);
        }
        boxDestroy(&box);
    }

    return boxad;
}


/*!
 * \brief   boxEqual()
 *
 * \param[in]    box1
 * \param[in]    box2
 * \param[out]   psame    1 if equal; 0 otherwise
 * \return  0 if OK, 1 on error
 */
l_ok
boxEqual(BOX      *box1,
         BOX      *box2,
         l_int32  *psame)
{
    if (!psame)
        return ERROR_INT("&same not defined", __func__, 1);
    *psame = 0;
    if (!box1 || !box2)
        return ERROR_INT("boxes not both defined", __func__, 1);
    if (box1->x == box2->x && box1->y == box2->y &&
        box1->w == box2->w && box1->h == box2->h)
        *psame = 1;
    return 0;
}


/*!
 * \brief   boxaEqual()
 *
 * \param[in]    boxa1
 * \param[in]    boxa2
 * \param[in]    maxdist
 * \param[out]   pnaindex     [optional] index array of correspondences
 * \param[out]   psame        1 if equal; 0 otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The two boxa are the "same" if they contain the same
 *          boxes and each box is within %maxdist of its counterpart
 *          in their positions within the boxa.  This allows for
 *          small rearrangements.  Use 0 for maxdist if the boxa
 *          must be identical.
 *      (2) This applies only to geometry and ordering; refcounts
 *          are not considered.
 *      (3) %maxdist allows some latitude in the ordering of the boxes.
 *          For the boxa to be the "same", corresponding boxes must
 *          be within %maxdist of each other.  Note that for large
 *          %maxdist, we should use a hash function for efficiency.
 *      (4) naindex[i] gives the position of the box in boxa2 that
 *          corresponds to box i in boxa1.  It is only returned if the
 *          boxa are equal.
 * </pre>
 */
l_ok
boxaEqual(BOXA     *boxa1,
          BOXA     *boxa2,
          l_int32   maxdist,
          NUMA    **pnaindex,
          l_int32  *psame)
{
l_int32   i, j, n, jstart, jend, found, samebox;
l_int32  *countarray;
BOX      *box1, *box2;
NUMA     *na;

    if (pnaindex) *pnaindex = NULL;
    if (!psame)
        return ERROR_INT("&same not defined", __func__, 1);
    *psame = 0;
    if (!boxa1 || !boxa2)
        return ERROR_INT("boxa1 and boxa2 not both defined", __func__, 1);
    n = boxaGetCount(boxa1);
    if (n != boxaGetCount(boxa2))
        return 0;

    if ((countarray = (l_int32 *)LEPT_CALLOC(n, sizeof(l_int32))) == NULL)
        return ERROR_INT("calloc fail for countarray", __func__, 1);
    na = numaMakeConstant(0.0, n);

    for (i = 0; i < n; i++) {
        box1 = boxaGetBox(boxa1, i, L_CLONE);
        jstart = L_MAX(0, i - maxdist);
        jend = L_MIN(n-1, i + maxdist);
        found = FALSE;
        for (j = jstart; j <= jend; j++) {
            box2 = boxaGetBox(boxa2, j, L_CLONE);
            boxEqual(box1, box2, &samebox);
            if (samebox && countarray[j] == 0) {
                countarray[j] = 1;
                numaReplaceNumber(na, i, j);
                found = TRUE;
                boxDestroy(&box2);
                break;
            }
            boxDestroy(&box2);
        }
        boxDestroy(&box1);
        if (!found) {
            numaDestroy(&na);
            LEPT_FREE(countarray);
            return 0;
        }
    }

    *psame = 1;
    if (pnaindex)
        *pnaindex = na;
    else
        numaDestroy(&na);
    LEPT_FREE(countarray);
    return 0;
}


/*!
 * \brief   boxSimilar()
 *
 * \param[in]    box1
 * \param[in]    box2
 * \param[in]    leftdiff, rightdiff, topdiff, botdiff
 * \param[out]   psimilar   1 if similar; 0 otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The values of leftdiff (etc) are the maximum allowed deviations
 *          between the locations of the left (etc) sides.  If any side
 *          pairs differ by more than this amount, the boxes are not similar.
 * </pre>
 */
l_ok
boxSimilar(BOX      *box1,
           BOX      *box2,
           l_int32   leftdiff,
           l_int32   rightdiff,
           l_int32   topdiff,
           l_int32   botdiff,
           l_int32  *psimilar)
{
l_int32  l1, l2, r1, r2, t1, t2, b1, b2, valid1, valid2;

    if (!psimilar)
        return ERROR_INT("&similar not defined", __func__, 1);
    *psimilar = 0;
    if (!box1 || !box2)
        return ERROR_INT("boxes not both defined", __func__, 1);
    boxIsValid(box1, &valid1);
    boxIsValid(box2, &valid2);
    if (!valid1 || !valid2)
        return ERROR_INT("boxes not both valid", __func__, 1);

    boxGetSideLocations(box1, &l1, &r1, &t1, &b1);
    boxGetSideLocations(box2, &l2, &r2, &t2, &b2);
    if (L_ABS(l1 - l2) > leftdiff)
        return 0;
    if (L_ABS(r1 - r2) > rightdiff)
        return 0;
    if (L_ABS(t1 - t2) > topdiff)
        return 0;
    if (L_ABS(b1 - b2) > botdiff)
        return 0;

    *psimilar = 1;
    return 0;
}


/*!
 * \brief   boxaSimilar()
 *
 * \param[in]    boxa1
 * \param[in]    boxa2
 * \param[in]    leftdiff, rightdiff, topdiff, botdiff
 * \param[in]    debug      output details of non-similar boxes
 * \param[out]   psimilar   1 if similar; 0 otherwise
 * \param[out]   pnasim     [optional] na containing 1 if similar; else 0
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See boxSimilar() for parameter usage.
 *      (2) Corresponding boxes are taken in order in the two boxa.
 *      (3) %nasim is an indicator array with a (0/1) for each box pair.
 *      (4) With %nasim or debug == 1, boxes continue to be tested
 *          after failure.
 * </pre>
 */
l_ok
boxaSimilar(BOXA     *boxa1,
            BOXA     *boxa2,
            l_int32   leftdiff,
            l_int32   rightdiff,
            l_int32   topdiff,
            l_int32   botdiff,
            l_int32   debug,
            l_int32  *psimilar,
            NUMA    **pnasim)
{
l_int32  i, n1, n2, match, mismatch;
BOX     *box1, *box2;

    if (psimilar) *psimilar = 0;
    if (pnasim) *pnasim = NULL;
    if (!boxa1 || !boxa2)
        return ERROR_INT("boxa1 and boxa2 not both defined", __func__, 1);
    if (!psimilar)
        return ERROR_INT("&similar not defined", __func__, 1);
    n1 = boxaGetCount(boxa1);
    n2 = boxaGetCount(boxa2);
    if (n1 != n2) {
        L_ERROR("boxa counts differ: %d vs %d\n", __func__, n1, n2);
        return 1;
    }
    if (pnasim) *pnasim = numaCreate(n1);

    mismatch = FALSE;
    for (i = 0; i < n1; i++) {
        box1 = boxaGetBox(boxa1, i, L_CLONE);
        box2 = boxaGetBox(boxa2, i, L_CLONE);
        boxSimilar(box1, box2, leftdiff, rightdiff, topdiff, botdiff,
                   &match);
        boxDestroy(&box1);
        boxDestroy(&box2);
        if (pnasim)
            numaAddNumber(*pnasim, match);
        if (!match) {
            mismatch = TRUE;
            if (!debug && pnasim == NULL)
                return 0;
            else if (debug)
                L_INFO("box %d not similar\n", __func__, i);
        }
    }

    if (!mismatch) *psimilar = 1;
    return 0;
}


/*----------------------------------------------------------------------*
 *                      Boxa combine and split                          *
 *----------------------------------------------------------------------*/
/*!
 * \brief   boxaJoin()
 *
 * \param[in]    boxad     dest boxa; add to this one
 * \param[in]    boxas     source boxa; add from this one
 * \param[in]    istart    starting index in boxas
 * \param[in]    iend      ending index in boxas; use -1 to cat all
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This appends a clone of each indicated box in boxas to boxad
 *      (2) istart < 0 is taken to mean 'read from the start' (istart = 0)
 *      (3) iend < 0 means 'read to the end'
 *      (4) if boxas == NULL or has no boxes, this is a no-op.
 * </pre>
 */
l_ok
boxaJoin(BOXA    *boxad,
         BOXA    *boxas,
         l_int32  istart,
         l_int32  iend)
{
l_int32  n, i;
BOX     *box;

    if (!boxad)
        return ERROR_INT("boxad not defined", __func__, 1);
    if (!boxas || ((n = boxaGetCount(boxas)) == 0))
        return 0;

    if (istart < 0)
        istart = 0;
    if (iend < 0 || iend >= n)
        iend = n - 1;
    if (istart > iend)
        return ERROR_INT("istart > iend; nothing to add", __func__, 1);

    for (i = istart; i <= iend; i++) {
        box = boxaGetBox(boxas, i, L_CLONE);
        boxaAddBox(boxad, box, L_INSERT);
    }

    return 0;
}


/*!
 * \brief   boxaaJoin()
 *
 * \param[in]    baad     dest boxaa; add to this one
 * \param[in]    baas     source boxaa; add from this one
 * \param[in]    istart   starting index in baas
 * \param[in]    iend     ending index in baas; use -1 to cat all
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This appends a clone of each indicated boxa in baas to baad
 *      (2) istart < 0 is taken to mean 'read from the start' (istart = 0)
 *      (3) iend < 0 means 'read to the end'
 *      (4) if baas == NULL, this is a no-op.
 * </pre>
 */
l_ok
boxaaJoin(BOXAA   *baad,
          BOXAA   *baas,
          l_int32  istart,
          l_int32  iend)
{
l_int32  n, i;
BOXA    *boxa;

    if (!baad)
        return ERROR_INT("baad not defined", __func__, 1);
    if (!baas)
        return 0;

    if (istart < 0)
        istart = 0;
    n = boxaaGetCount(baas);
    if (iend < 0 || iend >= n)
        iend = n - 1;
    if (istart > iend)
        return ERROR_INT("istart > iend; nothing to add", __func__, 1);

    for (i = istart; i <= iend; i++) {
        boxa = boxaaGetBoxa(baas, i, L_CLONE);
        boxaaAddBoxa(baad, boxa, L_INSERT);
    }

    return 0;
}


/*!
 * \brief   boxaSplitEvenOdd()
 *
 * \param[in]    boxa
 * \param[in]    fillflag         1 to put invalid boxes in place; 0 to omit
 * \param[out]   pboxae, pboxao   save even and odd boxes in their separate
 *                                boxa, setting the other type to invalid boxes.
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If %fillflag == 1, boxae has copies of the even boxes
 *          in their original location, and nvalid boxes are placed
 *          in the odd array locations.  And v.v.
 *      (2) If %fillflag == 0, boxae has only copies of the even boxes.
 * </pre>
 */
l_ok
boxaSplitEvenOdd(BOXA    *boxa,
                 l_int32  fillflag,
                 BOXA   **pboxae,
                 BOXA   **pboxao)
{
l_int32  i, n;
BOX     *box, *box1;

    if (pboxae) *pboxae = NULL;
    if (pboxao) *pboxao = NULL;
    if (!pboxae || !pboxao)
        return ERROR_INT("&boxae and &boxao not both defined", __func__, 1);
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    n = boxaGetCount(boxa);
    *pboxae = boxaCreate(n);
    *pboxao = boxaCreate(n);
    if (fillflag == 0) {
            /* don't fill with invalid boxes; end up with half-size boxa */
        for (i = 0; i < n; i++) {
            box = boxaGetBox(boxa, i, L_COPY);
            if ((i & 1) == 0)
                boxaAddBox(*pboxae, box, L_INSERT);
            else
                boxaAddBox(*pboxao, box, L_INSERT);
        }
    } else {
        for (i = 0; i < n; i++) {
            box = boxaGetBox(boxa, i, L_COPY);
            box1 = boxCreate(0, 0, 0, 0);  /* empty placeholder */
            if ((i & 1) == 0) {
                boxaAddBox(*pboxae, box, L_INSERT);
                boxaAddBox(*pboxao, box1, L_INSERT);
            } else {
                boxaAddBox(*pboxae, box1, L_INSERT);
                boxaAddBox(*pboxao, box, L_INSERT);
            }
        }
    }
    return 0;
}


/*!
 * \brief   boxaMergeEvenOdd()
 *
 * \param[in]    boxae       boxes to go in even positions in merged boxa
 * \param[in]    boxao       boxes to go in odd positions in merged boxa
 * \param[in]    fillflag    1 if there are invalid boxes in placeholders
 * \return  boxad merged, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is essentially the inverse of boxaSplitEvenOdd().
 *          Typically, boxae and boxao were generated by boxaSplitEvenOdd(),
 *          and the value of %fillflag needs to be the same in both calls.
 *      (2) If %fillflag == 1, both boxae and boxao are of the same size;
 *          otherwise boxae may have one more box than boxao.
 * </pre>
 */
BOXA *
boxaMergeEvenOdd(BOXA    *boxae,
                 BOXA    *boxao,
                 l_int32  fillflag)
{
l_int32  i, n, ne, no;
BOX     *box;
BOXA    *boxad;

    if (!boxae || !boxao)
        return (BOXA *)ERROR_PTR("boxae and boxao not defined", __func__, NULL);
    ne = boxaGetCount(boxae);
    no = boxaGetCount(boxao);
    if (ne < no || ne > no + 1)
        return (BOXA *)ERROR_PTR("boxa sizes invalid", __func__, NULL);

    boxad = boxaCreate(ne);
    if (fillflag == 0) {  /* both are approx. half-sized; all valid boxes */
        n = ne + no;
        for (i = 0; i < n; i++) {
            if ((i & 1) == 0)
                box = boxaGetBox(boxae, i / 2, L_COPY);
            else
                box = boxaGetBox(boxao, i / 2, L_COPY);
            boxaAddBox(boxad, box, L_INSERT);
        }
    } else {  /* both are full size and have invalid placeholders */
        for (i = 0; i < ne; i++) {
            if ((i & 1) == 0)
                box = boxaGetBox(boxae, i, L_COPY);
            else
                box = boxaGetBox(boxao, i, L_COPY);
            boxaAddBox(boxad, box, L_INSERT);
        }
    }
    return boxad;
}

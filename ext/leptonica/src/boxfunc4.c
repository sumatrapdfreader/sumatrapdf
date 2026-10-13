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
 * \file  boxfunc4.c
 * <pre>
 *
 *      Boxa and Boxaa range selection
 *           BOXA     *boxaSelectRange()
 *           BOXAA    *boxaaSelectRange()
 *
 *      Boxa size selection
 *           BOXA     *boxaSelectBySize()
 *           NUMA     *boxaMakeSizeIndicator()
 *           BOXA     *boxaSelectByArea()
 *           NUMA     *boxaMakeAreaIndicator()
 *           BOXA     *boxaSelectByWHRatio()
 *           NUMA     *boxaMakeWHRatioIndicator()
 *           BOXA     *boxaSelectWithIndicator()
 *
 *      Boxa permutation
 *           BOXA     *boxaPermutePseudorandom()
 *           BOXA     *boxaPermuteRandom()
 *           l_int32   boxaSwapBoxes()
 *
 *      Boxa and box conversions
 *           PTA      *boxaConvertToPta()
 *           BOXA     *ptaConvertToBoxa()
 *           PTA      *boxConvertToPta()
 *           BOX      *ptaConvertToBox()
 *
 *      Miscellaneous boxa functions
 *           l_int32   boxaGetExtent()
 *           l_int32   boxaGetCoverage()
 *           l_int32   boxaaSizeRange()
 *           l_int32   boxaSizeRange()
 *           l_int32   boxaLocationRange()
 *           NUMA     *boxaGetSizes()
 *           l_int32   boxaGetArea()
 *           PIX      *boxaDisplayTiled()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"
#include "pix_internal.h"

/*---------------------------------------------------------------------*
 *                     Boxa and boxaa range selection                  *
 *---------------------------------------------------------------------*/
/*!
 * \brief   boxaSelectRange()
 *
 * \param[in]    boxas
 * \param[in]    first      use 0 to select from the beginning
 * \param[in]    last       use -1 to select to the end
 * \param[in]    copyflag   L_COPY, L_CLONE
 * \return  boxad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The copyflag specifies what we do with each box from boxas.
 *          Specifically, L_CLONE inserts a clone into boxad of each
 *          selected box from boxas.
 * </pre>
 */
BOXA *
boxaSelectRange(BOXA    *boxas,
                l_int32  first,
                l_int32  last,
                l_int32  copyflag)
{
l_int32  n, nbox, i;
BOX     *box;
BOXA    *boxad;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (copyflag != L_COPY && copyflag != L_CLONE)
        return (BOXA *)ERROR_PTR("invalid copyflag", __func__, NULL);
    if ((n = boxaGetCount(boxas)) == 0) {
        L_WARNING("boxas is empty\n", __func__);
        return boxaCopy(boxas, copyflag);
    }
    first = L_MAX(0, first);
    if (last < 0) last = n - 1;
    if (first >= n)
        return (BOXA *)ERROR_PTR("invalid first", __func__, NULL);
    if (last >= n) {
        L_WARNING("last = %d is beyond max index = %d; adjusting\n",
                  __func__, last, n - 1);
        last = n - 1;
    }
    if (first > last)
        return (BOXA *)ERROR_PTR("first > last", __func__, NULL);

    nbox = last - first + 1;
    boxad = boxaCreate(nbox);
    for (i = first; i <= last; i++) {
        box = boxaGetBox(boxas, i, copyflag);
        boxaAddBox(boxad, box, L_INSERT);
    }
    return boxad;
}


/*!
 * \brief   boxaaSelectRange()
 *
 * \param[in]    baas
 * \param[in]    first      use 0 to select from the beginning
 * \param[in]    last       use -1 to select to the end
 * \param[in]    copyflag   L_COPY, L_CLONE
 * \return  baad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The copyflag specifies what we do with each boxa from baas.
 *          Specifically, L_CLONE inserts a clone into baad of each
 *          selected boxa from baas.
 * </pre>
 */
BOXAA *
boxaaSelectRange(BOXAA   *baas,
                 l_int32  first,
                 l_int32  last,
                 l_int32  copyflag)
{
l_int32  n, nboxa, i;
BOXA    *boxa;
BOXAA   *baad;

    if (!baas)
        return (BOXAA *)ERROR_PTR("baas not defined", __func__, NULL);
    if (copyflag != L_COPY && copyflag != L_CLONE)
        return (BOXAA *)ERROR_PTR("invalid copyflag", __func__, NULL);
    if ((n = boxaaGetCount(baas)) == 0)
        return (BOXAA *)ERROR_PTR("empty baas", __func__, NULL);
    first = L_MAX(0, first);
    if (last < 0) last = n - 1;
    if (first >= n)
        return (BOXAA *)ERROR_PTR("invalid first", __func__, NULL);
    if (last >= n) {
        L_WARNING("last = %d is beyond max index = %d; adjusting\n",
                  __func__, last, n - 1);
        last = n - 1;
    }
    if (first > last)
        return (BOXAA *)ERROR_PTR("first > last", __func__, NULL);

    nboxa = last - first + 1;
    baad = boxaaCreate(nboxa);
    for (i = first; i <= last; i++) {
        boxa = boxaaGetBoxa(baas, i, copyflag);
        boxaaAddBoxa(baad, boxa, L_INSERT);
    }
    return baad;
}


/*---------------------------------------------------------------------*
 *                          Boxa size selection                        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   boxaSelectBySize()
 *
 * \param[in]    boxas
 * \param[in]    width, height    threshold dimensions
 * \param[in]    type             L_SELECT_WIDTH, L_SELECT_HEIGHT,
 *                                L_SELECT_IF_EITHER, L_SELECT_IF_BOTH
 * \param[in]    relation         L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                                L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \param[out]   pchanged         [optional] 1 if changed; 0 if clone returned
 * \return  boxad filtered set, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The args specify constraints on the size of the
 *          components that are kept.
 *      (2) Uses box copies in the new boxa.
 *      (3) If the selection type is L_SELECT_WIDTH, the input
 *          height is ignored, and v.v.
 *      (4) To keep small components, use relation = L_SELECT_IF_LT or
 *          L_SELECT_IF_LTE.
 *          To keep large components, use relation = L_SELECT_IF_GT or
 *          L_SELECT_IF_GTE.
 * </pre>
 */
BOXA *
boxaSelectBySize(BOXA     *boxas,
                 l_int32   width,
                 l_int32   height,
                 l_int32   type,
                 l_int32   relation,
                 l_int32  *pchanged)
{
BOXA  *boxad;
NUMA  *na;

    if (pchanged) *pchanged = FALSE;
    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (boxaGetCount(boxas) == 0) {
        L_WARNING("boxas is empty\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if (type != L_SELECT_WIDTH && type != L_SELECT_HEIGHT &&
        type != L_SELECT_IF_EITHER && type != L_SELECT_IF_BOTH)
        return (BOXA *)ERROR_PTR("invalid type", __func__, NULL);
    if (relation != L_SELECT_IF_LT && relation != L_SELECT_IF_GT &&
        relation != L_SELECT_IF_LTE && relation != L_SELECT_IF_GTE)
        return (BOXA *)ERROR_PTR("invalid relation", __func__, NULL);

        /* Compute the indicator array for saving components */
    if ((na =
         boxaMakeSizeIndicator(boxas, width, height, type, relation)) == NULL)
        return (BOXA *)ERROR_PTR("na not made", __func__, NULL);

        /* Filter to get output */
    boxad = boxaSelectWithIndicator(boxas, na, pchanged);

    numaDestroy(&na);
    return boxad;
}


/*!
 * \brief   boxaMakeSizeIndicator()
 *
 * \param[in]    boxa
 * \param[in]    width, height   threshold dimensions
 * \param[in]    type            L_SELECT_WIDTH, L_SELECT_HEIGHT,
 *                               L_SELECT_IF_EITHER, L_SELECT_IF_BOTH
 * \param[in]    relation        L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                               L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \return  na indicator array, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The args specify constraints on the size of the
 *          components that are kept.
 *      (2) If the selection type is L_SELECT_WIDTH, the input
 *          height is ignored, and v.v.
 *      (3) To keep small components, use relation = L_SELECT_IF_LT or
 *          L_SELECT_IF_LTE.
 *          To keep large components, use relation = L_SELECT_IF_GT or
 *          L_SELECT_IF_GTE.
 * </pre>
 */
NUMA *
boxaMakeSizeIndicator(BOXA     *boxa,
                      l_int32   width,
                      l_int32   height,
                      l_int32   type,
                      l_int32   relation)
{
l_int32  i, n, w, h, ival;
NUMA    *na;

    if (!boxa)
        return (NUMA *)ERROR_PTR("boxa not defined", __func__, NULL);
    if ((n = boxaGetCount(boxa)) == 0)
        return (NUMA *)ERROR_PTR("boxa is empty", __func__, NULL);
    if (type != L_SELECT_WIDTH && type != L_SELECT_HEIGHT &&
        type != L_SELECT_IF_EITHER && type != L_SELECT_IF_BOTH)
        return (NUMA *)ERROR_PTR("invalid type", __func__, NULL);
    if (relation != L_SELECT_IF_LT && relation != L_SELECT_IF_GT &&
        relation != L_SELECT_IF_LTE && relation != L_SELECT_IF_GTE)
        return (NUMA *)ERROR_PTR("invalid relation", __func__, NULL);

    na = numaCreate(n);
    for (i = 0; i < n; i++) {
        ival = 0;
        boxaGetBoxGeometry(boxa, i, NULL, NULL, &w, &h);
        switch (type)
        {
        case L_SELECT_WIDTH:
            if ((relation == L_SELECT_IF_LT && w < width) ||
                (relation == L_SELECT_IF_GT && w > width) ||
                (relation == L_SELECT_IF_LTE && w <= width) ||
                (relation == L_SELECT_IF_GTE && w >= width))
                ival = 1;
            break;
        case L_SELECT_HEIGHT:
            if ((relation == L_SELECT_IF_LT && h < height) ||
                (relation == L_SELECT_IF_GT && h > height) ||
                (relation == L_SELECT_IF_LTE && h <= height) ||
                (relation == L_SELECT_IF_GTE && h >= height))
                ival = 1;
            break;
        case L_SELECT_IF_EITHER:
            if (((relation == L_SELECT_IF_LT) && (w < width || h < height)) ||
                ((relation == L_SELECT_IF_GT) && (w > width || h > height)) ||
               ((relation == L_SELECT_IF_LTE) && (w <= width || h <= height)) ||
                ((relation == L_SELECT_IF_GTE) && (w >= width || h >= height)))
                    ival = 1;
            break;
        case L_SELECT_IF_BOTH:
            if (((relation == L_SELECT_IF_LT) && (w < width && h < height)) ||
                ((relation == L_SELECT_IF_GT) && (w > width && h > height)) ||
               ((relation == L_SELECT_IF_LTE) && (w <= width && h <= height)) ||
                ((relation == L_SELECT_IF_GTE) && (w >= width && h >= height)))
                    ival = 1;
            break;
        default:
            L_WARNING("can't get here!\n", __func__);
            break;
        }
        numaAddNumber(na, ival);
    }

    return na;
}


/*!
 * \brief   boxaSelectByArea()
 *
 * \param[in]    boxas
 * \param[in]    area       threshold value of width * height
 * \param[in]    relation   L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                          L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \param[out]   pchanged   [optional] 1 if changed; 0 if clone returned
 * \return  boxad filtered set, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Uses box copies in the new boxa.
 *      (2) To keep small components, use relation = L_SELECT_IF_LT or
 *          L_SELECT_IF_LTE.
 *          To keep large components, use relation = L_SELECT_IF_GT or
 *          L_SELECT_IF_GTE.
 * </pre>
 */
BOXA *
boxaSelectByArea(BOXA     *boxas,
                 l_int32   area,
                 l_int32   relation,
                 l_int32  *pchanged)
{
BOXA  *boxad;
NUMA  *na;

    if (pchanged) *pchanged = FALSE;
    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (boxaGetCount(boxas) == 0) {
        L_WARNING("boxas is empty\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if (relation != L_SELECT_IF_LT && relation != L_SELECT_IF_GT &&
        relation != L_SELECT_IF_LTE && relation != L_SELECT_IF_GTE)
        return (BOXA *)ERROR_PTR("invalid relation", __func__, NULL);

        /* Compute the indicator array for saving components */
    na = boxaMakeAreaIndicator(boxas, area, relation);

        /* Filter to get output */
    boxad = boxaSelectWithIndicator(boxas, na, pchanged);

    numaDestroy(&na);
    return boxad;
}


/*!
 * \brief   boxaMakeAreaIndicator()
 *
 * \param[in]    boxa
 * \param[in]    area       threshold value of width * height
 * \param[in]    relation   L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                          L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \return  na indicator array, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) To keep small components, use relation = L_SELECT_IF_LT or
 *          L_SELECT_IF_LTE.
 *          To keep large components, use relation = L_SELECT_IF_GT or
 *          L_SELECT_IF_GTE.
 * </pre>
 */
NUMA *
boxaMakeAreaIndicator(BOXA     *boxa,
                      l_int32   area,
                      l_int32   relation)
{
l_int32  i, n, w, h, ival;
NUMA    *na;

    if (!boxa)
        return (NUMA *)ERROR_PTR("boxa not defined", __func__, NULL);
    if ((n = boxaGetCount(boxa)) == 0)
        return (NUMA *)ERROR_PTR("boxa is empty", __func__, NULL);
    if (relation != L_SELECT_IF_LT && relation != L_SELECT_IF_GT &&
        relation != L_SELECT_IF_LTE && relation != L_SELECT_IF_GTE)
        return (NUMA *)ERROR_PTR("invalid relation", __func__, NULL);

    na = numaCreate(n);
    for (i = 0; i < n; i++) {
        ival = 0;
        boxaGetBoxGeometry(boxa, i, NULL, NULL, &w, &h);

        if ((relation == L_SELECT_IF_LT && w * h < area) ||
            (relation == L_SELECT_IF_GT && w * h > area) ||
            (relation == L_SELECT_IF_LTE && w * h <= area) ||
            (relation == L_SELECT_IF_GTE && w * h >= area))
            ival = 1;
        numaAddNumber(na, ival);
    }

    return na;
}


/*!
 * \brief   boxaSelectByWHRatio()
 *
 * \param[in]    boxas
 * \param[in]    ratio     width/height threshold value
 * \param[in]    relation  L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                         L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \param[out]   pchanged  [optional] 1 if changed; 0 if clone returned
 * \return  boxad filtered set, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Uses box copies in the new boxa.
 *      (2) To keep narrow components, use relation = L_SELECT_IF_LT or
 *          L_SELECT_IF_LTE.
 *          To keep wide components, use relation = L_SELECT_IF_GT or
 *          L_SELECT_IF_GTE.
 * </pre>
 */
BOXA *
boxaSelectByWHRatio(BOXA      *boxas,
                    l_float32  ratio,
                    l_int32    relation,
                    l_int32   *pchanged)
{
BOXA  *boxad;
NUMA  *na;

    if (pchanged) *pchanged = FALSE;
    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (boxaGetCount(boxas) == 0) {
        L_WARNING("boxas is empty\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if (relation != L_SELECT_IF_LT && relation != L_SELECT_IF_GT &&
        relation != L_SELECT_IF_LTE && relation != L_SELECT_IF_GTE)
        return (BOXA *)ERROR_PTR("invalid relation", __func__, NULL);

        /* Compute the indicator array for saving components */
    na = boxaMakeWHRatioIndicator(boxas, ratio, relation);

        /* Filter to get output */
    boxad = boxaSelectWithIndicator(boxas, na, pchanged);

    numaDestroy(&na);
    return boxad;
}


/*!
 * \brief   boxaMakeWHRatioIndicator()
 *
 * \param[in]    boxa
 * \param[in]    ratio     width/height threshold value
 * \param[in]    relation  L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                         L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \return  na indicator array, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) To keep narrow components, use relation = L_SELECT_IF_LT or
 *          L_SELECT_IF_LTE.
 *          To keep wide components, use relation = L_SELECT_IF_GT or
 *          L_SELECT_IF_GTE.
 * </pre>
 */
NUMA *
boxaMakeWHRatioIndicator(BOXA      *boxa,
                         l_float32  ratio,
                         l_int32    relation)
{
l_int32    i, n, w, h, ival;
l_float32  whratio;
NUMA      *na;

    if (!boxa)
        return (NUMA *)ERROR_PTR("boxa not defined", __func__, NULL);
    if ((n = boxaGetCount(boxa)) == 0)
        return (NUMA *)ERROR_PTR("boxa is empty", __func__, NULL);
    if (relation != L_SELECT_IF_LT && relation != L_SELECT_IF_GT &&
        relation != L_SELECT_IF_LTE && relation != L_SELECT_IF_GTE)
        return (NUMA *)ERROR_PTR("invalid relation", __func__, NULL);

    na = numaCreate(n);
    for (i = 0; i < n; i++) {
        ival = 0;
        boxaGetBoxGeometry(boxa, i, NULL, NULL, &w, &h);
        whratio = (l_float32)w / (l_float32)h;

        if ((relation == L_SELECT_IF_LT && whratio < ratio) ||
            (relation == L_SELECT_IF_GT && whratio > ratio) ||
            (relation == L_SELECT_IF_LTE && whratio <= ratio) ||
            (relation == L_SELECT_IF_GTE && whratio >= ratio))
            ival = 1;
        numaAddNumber(na, ival);
    }

    return na;
}


/*!
 * \brief   boxaSelectWithIndicator()
 *
 * \param[in]    boxas
 * \param[in]    na         indicator numa
 * \param[out]   pchanged   [optional] 1 if changed; 0 if clone returned
 * \return  boxad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Returns a copy of the boxa if no components are removed.
 *      (2) Uses box copies in the new boxa.
 *      (3) The indicator numa has values 0 (ignore) and 1 (accept).
 *      (4) If all indicator values are 0, the returned boxa is empty.
 * </pre>
 */
BOXA *
boxaSelectWithIndicator(BOXA     *boxas,
                        NUMA     *na,
                        l_int32  *pchanged)
{
l_int32  i, n, ival, nsave;
BOX     *box;
BOXA    *boxad;

    if (pchanged) *pchanged = FALSE;
    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (!na)
        return (BOXA *)ERROR_PTR("na not defined", __func__, NULL);

    nsave = 0;
    n = numaGetCount(na);
    for (i = 0; i < n; i++) {
        numaGetIValue(na, i, &ival);
        if (ival == 1) nsave++;
    }

    if (nsave == n) {
        if (pchanged) *pchanged = FALSE;
        return boxaCopy(boxas, L_COPY);
    }
    if (pchanged) *pchanged = TRUE;
    boxad = boxaCreate(nsave);
    for (i = 0; i < n; i++) {
        numaGetIValue(na, i, &ival);
        if (ival == 0) continue;
        box = boxaGetBox(boxas, i, L_COPY);
        boxaAddBox(boxad, box, L_INSERT);
    }

    return boxad;
}


/*---------------------------------------------------------------------*
 *                           Boxa Permutation                          *
 *---------------------------------------------------------------------*/
/*!
 * \brief   boxaPermutePseudorandom()
 *
 * \param[in]    boxas   input boxa
 * \return  boxad with boxes permuted, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This does a pseudorandom in-place permutation of the boxes.
 *      (2) The result is guaranteed not to have any boxes in their
 *          original position, but it is not very random.  If you
 *          need randomness, use boxaPermuteRandom().
 * </pre>
 */
BOXA *
boxaPermutePseudorandom(BOXA  *boxas)
{
l_int32  n;
NUMA    *na;
BOXA    *boxad;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxa not defined", __func__, NULL);

    n = boxaGetCount(boxas);
    na = numaPseudorandomSequence(n, 0);
    boxad = boxaSortByIndex(boxas, na);
    numaDestroy(&na);
    return boxad;
}


/*!
 * \brief   boxaPermuteRandom()
 *
 * \param[in]    boxad    [optional]   can be null or equal to boxas
 * \param[in]    boxas    input boxa
 * \return  boxad with boxes permuted, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If boxad is null, make a copy of boxas and permute the copy.
 *          Otherwise, boxad must be equal to boxas, and the operation
 *          is done in-place.
 *      (2) If boxas is empty, return an empty boxad.
 *      (3) This does a random in-place permutation of the boxes,
 *          by swapping each box in turn with a random box.  The
 *          result is almost guaranteed not to have any boxes in their
 *          original position.
 *      (4) MSVC rand() has MAX_RAND = 2^15 - 1, so it will not do
 *          a proper permutation is the number of boxes exceeds this.
 * </pre>
 */
BOXA *
boxaPermuteRandom(BOXA  *boxad,
                  BOXA  *boxas)
{
l_int32  i, n, index;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxa not defined", __func__, NULL);
    if (boxad && (boxad != boxas))
        return (BOXA *)ERROR_PTR("boxad defined but in-place", __func__, NULL);

    if (!boxad)
        boxad = boxaCopy(boxas, L_COPY);
    if ((n = boxaGetCount(boxad)) == 0)
        return boxad;
    index = (l_uint32)rand() % n;
    index = L_MAX(1, index);
    boxaSwapBoxes(boxad, 0, index);
    for (i = 1; i < n; i++) {
        index = (l_uint32)rand() % n;
        if (index == i) index--;
        boxaSwapBoxes(boxad, i, index);
    }

    return boxad;
}


/*!
 * \brief   boxaSwapBoxes()
 *
 * \param[in]    boxa
 * \param[in]    i, j      two indices of boxes, that are to be swapped
 * \return  0 if OK, 1 on error
 */
l_ok
boxaSwapBoxes(BOXA    *boxa,
              l_int32  i,
              l_int32  j)
{
l_int32  n;
BOX     *box;

    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    n = boxaGetCount(boxa);
    if (i < 0 || i >= n)
        return ERROR_INT("i invalid", __func__, 1);
    if (j < 0 || j >= n)
        return ERROR_INT("j invalid", __func__, 1);
    if (i == j)
        return ERROR_INT("i == j", __func__, 1);

    box = boxa->box[i];
    boxa->box[i] = boxa->box[j];
    boxa->box[j] = box;
    return 0;
}


/*---------------------------------------------------------------------*
 *                     Boxa and Box Conversions                        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   boxaConvertToPta()
 *
 * \param[in]    boxa
 * \param[in]    ncorners     2 or 4 for the representation of each box
 * \return  pta with %ncorners points for each box in the boxa,
 *                   or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If ncorners == 2, we select the UL and LR corners.
 *          Otherwise we save all 4 corners in this order: UL, UR, LL, LR.
 *      (2) Other boxa --> pta functions are:
 *          * boxaExtractAsPta(): allows extraction of any dimension
 *            and/or side location, with each in a separate pta.
 *          * boxaExtractCorners(): extracts any of the four corners as a pta.
 * </pre>
 */
PTA *
boxaConvertToPta(BOXA    *boxa,
                 l_int32  ncorners)
{
l_int32  i, n;
BOX     *box;
PTA     *pta, *pta1;

    if (!boxa)
        return (PTA *)ERROR_PTR("boxa not defined", __func__, NULL);
    if (ncorners != 2 && ncorners != 4)
        return (PTA *)ERROR_PTR("ncorners not 2 or 4", __func__, NULL);

    n = boxaGetCount(boxa);
    if ((pta = ptaCreate(n)) == NULL)
        return (PTA *)ERROR_PTR("pta not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        box = boxaGetBox(boxa, i, L_COPY);
        pta1 = boxConvertToPta(box, ncorners);
        ptaJoin(pta, pta1, 0, -1);
        boxDestroy(&box);
        ptaDestroy(&pta1);
    }

    return pta;
}


/*!
 * \brief   ptaConvertToBoxa()
 *
 * \param[in]    pta
 * \param[in]    ncorners   2 or 4 for the representation of each box
 * \return  boxa with one box for each 2 or 4 points in the pta,
 *                    or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For 2 corners, the order of the 2 points is UL, LR.
 *          For 4 corners, the order of points is UL, UR, LL, LR.
 *      (2) Each derived box is the minimum size containing all corners.
 * </pre>
 */
BOXA *
ptaConvertToBoxa(PTA     *pta,
                 l_int32  ncorners)
{
l_int32  i, n, nbox, x1, y1, x2, y2, x3, y3, x4, y4, x, y, xmax, ymax;
BOX     *box;
BOXA    *boxa;

    if (!pta)
        return (BOXA *)ERROR_PTR("pta not defined", __func__, NULL);
    if (ncorners != 2 && ncorners != 4)
        return (BOXA *)ERROR_PTR("ncorners not 2 or 4", __func__, NULL);
    n = ptaGetCount(pta);
    if (n % ncorners != 0)
        return (BOXA *)ERROR_PTR("size % ncorners != 0", __func__, NULL);
    nbox = n / ncorners;
    if ((boxa = boxaCreate(nbox)) == NULL)
        return (BOXA *)ERROR_PTR("boxa not made", __func__, NULL);
    for (i = 0; i < n; i += ncorners) {
        ptaGetIPt(pta, i, &x1, &y1);
        ptaGetIPt(pta, i + 1, &x2, &y2);
        if (ncorners == 2) {
            box = boxCreate(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
            boxaAddBox(boxa, box, L_INSERT);
            continue;
        }
        ptaGetIPt(pta, i + 2, &x3, &y3);
        ptaGetIPt(pta, i + 3, &x4, &y4);
        x = L_MIN(x1, x3);
        y = L_MIN(y1, y2);
        xmax = L_MAX(x2, x4);
        ymax = L_MAX(y3, y4);
        box = boxCreate(x, y, xmax - x + 1, ymax - y + 1);
        boxaAddBox(boxa, box, L_INSERT);
    }

    return boxa;
}


/*!
 * \brief   boxConvertToPta()
 *
 * \param[in]    box
 * \param[in]    ncorners   2 or 4 for the representation of the box
 * \return  pta with %ncorners points, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If ncorners == 2, we select the UL and LR corners.
 *          Otherwise we save all 4 corners in this order: UL, UR, LL, LR.
 * </pre>
 */
PTA *
boxConvertToPta(BOX     *box,
                l_int32  ncorners)
{
l_int32  x, y, w, h;
PTA     *pta;

    if (!box)
        return (PTA *)ERROR_PTR("box not defined", __func__, NULL);
    if (ncorners != 2 && ncorners != 4)
        return (PTA *)ERROR_PTR("ncorners not 2 or 4", __func__, NULL);

    if ((pta = ptaCreate(ncorners)) == NULL)
        return (PTA *)ERROR_PTR("pta not made", __func__, NULL);
    boxGetGeometry(box, &x, &y, &w, &h);
    ptaAddPt(pta, x, y);
    if (ncorners == 2) {
        ptaAddPt(pta, x + w - 1, y + h - 1);
    } else {
        ptaAddPt(pta, x + w - 1, y);
        ptaAddPt(pta, x, y + h - 1);
        ptaAddPt(pta, x + w - 1, y + h - 1);
    }

    return pta;
}


/*!
 * \brief   ptaConvertToBox()
 *
 * \param[in]    pta
 * \return  box minimum containing all points in the pta, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For 2 corners, the order of the 2 points is UL, LR.
 *          For 4 corners, the order of points is UL, UR, LL, LR.
 * </pre>
 */
BOX *
ptaConvertToBox(PTA  *pta)
{
l_int32  n, x1, y1, x2, y2, x3, y3, x4, y4, x, y, xmax, ymax;

    if (!pta)
        return (BOX *)ERROR_PTR("pta not defined", __func__, NULL);
    n = ptaGetCount(pta);
    if (n != 2 && n != 4)
        return (BOX *)ERROR_PTR("n must be 2 or 4", __func__, NULL);
    ptaGetIPt(pta, 0, &x1, &y1);
    ptaGetIPt(pta, 1, &x2, &y2);
    if (n == 2)
        return boxCreate(x1, y1, x2 - x1 + 1, y2 - y1 + 1);

        /* 4 corners */
    ptaGetIPt(pta, 2, &x3, &y3);
    ptaGetIPt(pta, 3, &x4, &y4);
    x = L_MIN(x1, x3);
    y = L_MIN(y1, y2);
    xmax = L_MAX(x2, x4);
    ymax = L_MAX(y3, y4);
    return boxCreate(x, y, xmax - x + 1, ymax - y + 1);
}


/*---------------------------------------------------------------------*
 *                    Miscellaneous Boxa functions                     *
 *---------------------------------------------------------------------*/
/*!
 * \brief   boxaGetExtent()
 *
 * \param[in]    boxa
 * \param[out]   pw      [optional] width
 * \param[out]   ph      [optional] height
 * \param[out]   pbox    [optional]  minimum box containing all boxes in boxa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This computes the minimum rectangular bounding region
 *          that contains all valid boxes in a boxa.
 *      (2) The returned w and h are the minimum size image
 *          that would contain all boxes untranslated.
 *      (3) If there are no valid boxes, returned w and h are 0 and
 *          all parameters in the returned box are 0.  This
 *          is not an error, because an empty boxa is valid and
 *          boxaGetExtent() is required for serialization.
 * </pre>
 */
l_ok
boxaGetExtent(BOXA     *boxa,
              l_int32  *pw,
              l_int32  *ph,
              BOX     **pbox)
{
l_int32  i, n, x, y, w, h, xmax, ymax, xmin, ymin, found;

    if (!pw && !ph && !pbox)
        return ERROR_INT("no ptrs defined", __func__, 1);
    if (pw) *pw = 0;
    if (ph) *ph = 0;
    if (pbox) *pbox = NULL;
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    n = boxaGetCount(boxa);
    xmax = ymax = 0;
    xmin = ymin = 100000000;
    found = FALSE;
    for (i = 0; i < n; i++) {
        boxaGetBoxGeometry(boxa, i, &x, &y, &w, &h);
        if (w <= 0 || h <= 0)
            continue;
        found = TRUE;
        xmin = L_MIN(xmin, x);
        ymin = L_MIN(ymin, y);
        xmax = L_MAX(xmax, x + w);
        ymax = L_MAX(ymax, y + h);
    }
    if (found == FALSE)  /* no valid boxes in boxa */
        xmin = ymin = 0;
    if (pw) *pw = xmax;
    if (ph) *ph = ymax;
    if (pbox)
      *pbox = boxCreate(xmin, ymin, xmax - xmin, ymax - ymin);

    return 0;
}


/*!
 * \brief   boxaGetCoverage()
 *
 * \param[in]    boxa
 * \param[in]    wc, hc     dimensions of overall clipping rectangle with UL
 *                          corner at (0, 0 that is covered by the boxes.
 * \param[in]    exactflag  1 for guaranteeing an exact result; 0 for getting
 *                          an exact result only if the boxes do not overlap
 * \param[out]   pfract     sum of box area as fraction of w * h
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The boxes in boxa are clipped to the input rectangle.
 *      (2) * When %exactflag == 1, we generate a 1 bpp pix of size
 *            wc x hc, paint all the boxes black, and count the fg pixels.
 *            This can take 1 msec on a large page with many boxes.
 *          * When %exactflag == 0, we clip each box to the wc x hc region
 *            and sum the resulting areas.  This is faster.
 *          * The results are the same when none of the boxes overlap
 *            within the wc x hc region.
 * </pre>
 */
l_ok
boxaGetCoverage(BOXA       *boxa,
                l_int32     wc,
                l_int32     hc,
                l_int32     exactflag,
                l_float32  *pfract)
{
l_int32  i, n, x, y, w, h, sum;
BOX     *box, *boxc;
PIX     *pixt;

    if (!pfract)
        return ERROR_INT("&fract not defined", __func__, 1);
    *pfract = 0.0;
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    n = boxaGetCount(boxa);
    if (n == 0)
        return ERROR_INT("no boxes in boxa", __func__, 1);

    if (exactflag == 0) {  /* quick and dirty */
        sum = 0;
        for (i = 0; i < n; i++) {
            box = boxaGetBox(boxa, i, L_CLONE);
            if ((boxc = boxClipToRectangle(box, wc, hc)) != NULL) {
                boxGetGeometry(boxc, NULL, NULL, &w, &h);
                sum += w * h;
                boxDestroy(&boxc);
            }
            boxDestroy(&box);
        }
    } else {  /* slower and exact */
        pixt = pixCreate(wc, hc, 1);
        for (i = 0; i < n; i++) {
            box = boxaGetBox(boxa, i, L_CLONE);
            boxGetGeometry(box, &x, &y, &w, &h);
            pixRasterop(pixt, x, y, w, h, PIX_SET, NULL, 0, 0);
            boxDestroy(&box);
        }
        pixCountPixels(pixt, &sum, NULL);
        pixDestroy(&pixt);
    }

    *pfract = (l_float32)sum / (l_float32)(wc * hc);
    return 0;
}


/*!
 * \brief   boxaaSizeRange()
 *
 * \param[in]    baa
 * \param[out]   pminw    [optional] min width of all boxes
 * \param[out]   pmaxw    [optional] max width of all boxes
 * \param[out]   pminh    [optional] min height of all boxes
 * \param[out]   pmaxh    [optional] max height of all boxes
 * \return  0 if OK, 1 on error
 */
l_ok
boxaaSizeRange(BOXAA    *baa,
               l_int32  *pminw,
               l_int32  *pminh,
               l_int32  *pmaxw,
               l_int32  *pmaxh)
{
l_int32  minw, minh, maxw, maxh, minbw, minbh, maxbw, maxbh, i, n;
BOXA    *boxa;

    if (!pminw && !pmaxw && !pminh && !pmaxh)
        return ERROR_INT("no data can be returned", __func__, 1);
    if (pminw) *pminw = 0;
    if (pminh) *pminh = 0;
    if (pmaxw) *pmaxw = 0;
    if (pmaxh) *pmaxh = 0;
    if (!baa)
        return ERROR_INT("baa not defined", __func__, 1);

    minw = minh = 100000000;
    maxw = maxh = 0;
    n = boxaaGetCount(baa);
    for (i = 0; i < n; i++) {
        boxa = boxaaGetBoxa(baa, i, L_CLONE);
        boxaSizeRange(boxa, &minbw, &minbh, &maxbw, &maxbh);
        if (minbw < minw)
            minw = minbw;
        if (minbh < minh)
            minh = minbh;
        if (maxbw > maxw)
            maxw = maxbw;
        if (maxbh > maxh)
            maxh = maxbh;
        boxaDestroy(&boxa);
    }

    if (pminw) *pminw = minw;
    if (pminh) *pminh = minh;
    if (pmaxw) *pmaxw = maxw;
    if (pmaxh) *pmaxh = maxh;
    return 0;
}


/*!
 * \brief   boxaSizeRange()
 *
 * \param[in]    boxa
 * \param[out]   pminw    [optional] min width of all boxes
 * \param[out]   pmaxw    [optional] max width of all boxes
 * \param[out]   pminh    [optional] min height of all boxes
 * \param[out]   pmaxh    [optional] max height of all boxes
 * \return  0 if OK, 1 on error
 */
l_ok
boxaSizeRange(BOXA     *boxa,
              l_int32  *pminw,
              l_int32  *pminh,
              l_int32  *pmaxw,
              l_int32  *pmaxh)
{
l_int32  minw, minh, maxw, maxh, i, n, w, h;

    if (!pminw && !pmaxw && !pminh && !pmaxh)
        return ERROR_INT("no data can be returned", __func__, 1);
    if (pminw) *pminw = 0;
    if (pminh) *pminh = 0;
    if (pmaxw) *pmaxw = 0;
    if (pmaxh) *pmaxh = 0;
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    minw = minh = 100000000;
    maxw = maxh = 0;
    n = boxaGetCount(boxa);
    for (i = 0; i < n; i++) {
        boxaGetBoxGeometry(boxa, i, NULL, NULL, &w, &h);
        if (w < minw)
            minw = w;
        if (h < minh)
            minh = h;
        if (w > maxw)
            maxw = w;
        if (h > maxh)
            maxh = h;
    }

    if (pminw) *pminw = minw;
    if (pminh) *pminh = minh;
    if (pmaxw) *pmaxw = maxw;
    if (pmaxh) *pmaxh = maxh;
    return 0;
}


/*!
 * \brief   boxaLocationRange()
 *
 * \param[in]    boxa
 * \param[out]   pminx    [optional] min (UL corner) x value of all boxes
 * \param[out]   pminy    [optional] min (UL corner) y value of all boxes
 * \param[out]   pmaxx    [optional] max (UL corner) x value of all boxes
 * \param[out]   pmaxy    [optional] max (UL corner) y value of all boxes
 * \return  0 if OK, 1 on error
 */
l_ok
boxaLocationRange(BOXA     *boxa,
                  l_int32  *pminx,
                  l_int32  *pminy,
                  l_int32  *pmaxx,
                  l_int32  *pmaxy)
{
l_int32  minx, miny, maxx, maxy, i, n, x, y;

    if (!pminx && !pminy && !pmaxx && !pmaxy)
        return ERROR_INT("no data can be returned", __func__, 1);
    if (pminx) *pminx = 0;
    if (pminy) *pminy = 0;
    if (pmaxx) *pmaxx = 0;
    if (pmaxy) *pmaxy = 0;
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    minx = miny = 100000000;
    maxx = maxy = 0;
    n = boxaGetCount(boxa);
    for (i = 0; i < n; i++) {
        boxaGetBoxGeometry(boxa, i, &x, &y, NULL, NULL);
        if (x < minx)
            minx = x;
        if (y < miny)
            miny = y;
        if (x > maxx)
            maxx = x;
        if (y > maxy)
            maxy = y;
    }

    if (pminx) *pminx = minx;
    if (pminy) *pminy = miny;
    if (pmaxx) *pmaxx = maxx;
    if (pmaxy) *pmaxy = maxy;

    return 0;
}


/*!
 * \brief   boxaGetSizes()
 *
 * \param[in]    boxa
 * \param[out]   pnaw    [optional] widths of valid boxes
 * \param[out]   pnah    [optional] heights of valid boxes
 * \return  0 if OK, 1 on error
 */
l_ok
boxaGetSizes(BOXA   *boxa,
             NUMA  **pnaw,
             NUMA  **pnah)
{
l_int32  i, n, w, h;
BOX     *box;

    if (pnaw) *pnaw = NULL;
    if (pnah) *pnah = NULL;
    if (!pnaw && !pnah)
        return ERROR_INT("no output requested", __func__, 1);
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    n = boxaGetValidCount(boxa);
    if (pnaw) *pnaw = numaCreate(n);
    if (pnah) *pnah = numaCreate(n);
    for (i = 0; i < n; i++) {
        box = boxaGetValidBox(boxa, i, L_COPY);
        if (box) {
            boxGetGeometry(box, NULL, NULL, &w, &h);
            if (pnaw) numaAddNumber(*pnaw, w);
            if (pnah) numaAddNumber(*pnah, h);
            boxDestroy(&box);
        }
    }

    return 0;
}


/*!
 * \brief   boxaGetArea()
 *
 * \param[in]    boxa
 * \param[out]   parea    total area of all boxes
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Measures the total area of the boxes, without regard to overlaps.
 * </pre>
 */
l_ok
boxaGetArea(BOXA     *boxa,
            l_int32  *parea)
{
l_int32  i, n, w, h;

    if (!parea)
        return ERROR_INT("&area not defined", __func__, 1);
    *parea = 0;
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    n = boxaGetCount(boxa);
    for (i = 0; i < n; i++) {
        boxaGetBoxGeometry(boxa, i, NULL, NULL, &w, &h);
        *parea += w * h;
    }
    return 0;
}


/*!
 * \brief   boxaDisplayTiled()
 *
 * \param[in]    boxas
 * \param[in]    pixa          [optional] background for each box
 * \param[in]    first         index of first box
 * \param[in]    last          index of last box; use -1 to go to end
 * \param[in]    maxwidth      of output image
 * \param[in]    linewidth     width of box outlines, before scaling
 * \param[in]    scalefactor   applied to every box; use 1.0 for no scaling
 * \param[in]    background    0 for white, 1 for black; this is the color
 *                             of the spacing between the images
 * \param[in]    spacing       between images, and on outside
 * \param[in]    border        width of black border added to each image;
 *                             use 0 for no border
 * \return  pixd of tiled images of boxes, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Displays each box separately in a tiled 32 bpp image.
 *      (2) If pixa is defined, it must have the same count as the boxa,
 *          and it will be a background over with each box is rendered.
 *          If pixa is not defined, the boxes will be rendered over
 *          blank images of identical size.
 *      (3) See pixaDisplayTiledInRows() for other parameters.
 * </pre>
 */
PIX *
boxaDisplayTiled(BOXA        *boxas,
                 PIXA        *pixa,
                 l_int32      first,
                 l_int32      last,
                 l_int32      maxwidth,
                 l_int32      linewidth,
                 l_float32    scalefactor,
                 l_int32      background,
                 l_int32      spacing,
                 l_int32      border)
{
char     buf[32];
l_int32  i, n, npix, w, h, fontsize;
L_BMF   *bmf;
BOX     *box;
BOXA    *boxa;
PIX     *pix1, *pix2, *pixd;
PIXA    *pixat;

    if (!boxas)
        return (PIX *)ERROR_PTR("boxas not defined", __func__, NULL);

    boxa = boxaSaveValid(boxas, L_COPY);
    n = boxaGetCount(boxa);
    if (pixa) {
        npix = pixaGetCount(pixa);
        if (n != npix) {
            boxaDestroy(&boxa);
            return (PIX *)ERROR_PTR("boxa and pixa counts differ",
                                    __func__, NULL);
        }
    }
    first = L_MAX(0, first);
    if (last < 0) last = n - 1;
    if (first >= n) {
        boxaDestroy(&boxa);
        return (PIX *)ERROR_PTR("invalid first", __func__, NULL);
    }
    if (last >= n) {
        L_WARNING("last = %d is beyond max index = %d; adjusting\n",
                  __func__, last, n - 1);
        last = n - 1;
    }
    if (first > last) {
        boxaDestroy(&boxa);
        return (PIX *)ERROR_PTR("first > last", __func__, NULL);
    }

        /* Because the bitmap font will be reduced when tiled, choose the
         * font size inversely with the scale factor. */
    if (scalefactor > 0.8)
        fontsize = 6;
    else if (scalefactor > 0.6)
        fontsize = 10;
    else if (scalefactor > 0.4)
        fontsize = 14;
    else if (scalefactor > 0.3)
        fontsize = 18;
    else fontsize = 20;
    bmf = bmfCreate(NULL, fontsize);

    pixat = pixaCreate(n);
    boxaGetExtent(boxa, &w, &h, NULL);
    for (i = first; i <= last; i++) {
        box = boxaGetBox(boxa, i, L_CLONE);
        if (!pixa) {
            pix1 = pixCreate(w, h, 32);
            pixSetAll(pix1);
        } else {
            pix1 = pixaGetPix(pixa, i, L_COPY);
        }
        pixSetBorderVal(pix1, 0, 0, 0, 2, 0x0000ff00);
        snprintf(buf, sizeof(buf), "%d", i);
        pix2 = pixAddSingleTextblock(pix1, bmf, buf, 0x00ff0000,
                                     L_ADD_BELOW, NULL);
        pixDestroy(&pix1);
        pixRenderBoxArb(pix2, box, linewidth, 255, 0, 0);
        pixaAddPix(pixat, pix2, L_INSERT);
        boxDestroy(&box);
    }
    bmfDestroy(&bmf);
    boxaDestroy(&boxa);

    pixd = pixaDisplayTiledInRows(pixat, 32, maxwidth, scalefactor, background,
                                  spacing, border);
    pixaDestroy(&pixat);
    return pixd;
}

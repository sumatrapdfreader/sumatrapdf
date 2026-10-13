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
 * \file  pixafunc1.c
 * <pre>
 *
 *      Filters
 *           PIX      *pixSelectBySize()
 *           PIXA     *pixaSelectBySize()
 *           NUMA     *pixaMakeSizeIndicator()
 *
 *           PIX      *pixSelectByPerimToAreaRatio()
 *           PIXA     *pixaSelectByPerimToAreaRatio()
 *           PIX      *pixSelectByPerimSizeRatio()
 *           PIXA     *pixaSelectByPerimSizeRatio()
 *           PIX      *pixSelectByAreaFraction()
 *           PIXA     *pixaSelectByAreaFraction()
 *           PIX      *pixSelectByArea()
 *           PIXA     *pixaSelectByArea()
 *           PIX      *pixSelectByWidthHeightRatio()
 *           PIXA     *pixaSelectByWidthHeightRatio()
 *           PIXA     *pixaSelectByNumConnComp()
 *
 *           PIXA     *pixaSelectWithIndicator()
 *           l_int32   pixRemoveWithIndicator()
 *           l_int32   pixAddWithIndicator()
 *           PIXA     *pixaSelectWithString()
 *           PIX      *pixaRenderComponent()
 *
 *      Sort functions
 *           PIXA     *pixaSort()
 *           PIXA     *pixaBinSort()
 *           PIXA     *pixaSortByIndex()
 *           PIXAA    *pixaSort2dByIndex()
 *
 *      Pixa and Pixaa range selection
 *           PIXA     *pixaSelectRange()
 *           PIXAA    *pixaaSelectRange()
 *
 *      Pixa and Pixaa scaling
 *           PIXAA    *pixaaScaleToSize()
 *           PIXAA    *pixaaScaleToSizeVar()
 *           PIXA     *pixaScaleToSize()
 *           PIXA     *pixaScaleToSizeRel()
 *           PIXA     *pixaScale()
 *           PIXA     *pixaScaleBySampling()
 *
 *      Pixa rotation and translation
 *           PIXA     *pixaRotate()
 *           PIXA     *pixaRotateOrth()
 *           PIXA     *pixaTranslate()
 *
 *      Miscellaneous
 *           PIXA     *pixaAddBorderGeneral()
 *           PIXA     *pixaaFlattenToPixa()
 *           l_int32   pixaaSizeRange()
 *           l_int32   pixaSizeRange()
 *           PIXA     *pixaClipToPix()
 *           PIXA     *pixaClipToForeground()
 *           l_int32   pixaGetRenderingDepth()
 *           l_int32   pixaHasColor()
 *           l_int32   pixaAnyColormaps()
 *           l_int32   pixaGetDepthInfo()
 *           PIXA     *pixaConvertToSameDepth()
 *           PIXA     *pixaConvertToGivenDepth()
 *           l_int32   pixaEqual()
 *           l_int32   pixaSetFullSizeBoxa()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"
#include "pix_internal.h"

    /* For more than this number of c.c. in a binarized image of
     * semi-perimeter (w + h) about 5000 or less, the O(n) binsort
     * is faster than the O(nlogn) shellsort.  */
static const l_int32   MinCompsForBinSort = 200;

    /* Don't rotate any angle smaller than this */
static const l_float32  MinAngleToRotate = 0.001f;  /* radians; ~0.06 deg */

/*---------------------------------------------------------------------*
 *                                Filters                              *
 *---------------------------------------------------------------------*/
/*
 * These filters work on the connected components of 1 bpp images.
 * They are typically used on pixa that have been generated from a Pix
 * using pixConnComp(), so that the corresponding Boxa is available.
 *
 * The filters remove or retain c.c. based on these properties:
 *    (a) size  [pixaFindDimensions()]
 *    (b) area-to-perimeter ratio   [pixaFindAreaPerimRatio()]
 *    (c) foreground area as a fraction of bounding box area (w * h)
 *        [pixaFindForegroundArea()]
 *    (d) number of foreground pixels   [pixaCountPixels()]
 *    (e) width/height aspect ratio  [pixFindWidthHeightRatio()]
 *
 * We provide two different high-level interfaces:
 *    (1) Functions that use one of the filters on either
 *        a pix or the pixa of components.
 *    (2) A general method that generates numas of indicator functions,
 *        logically combines them, and efficiently removes or adds
 *        the selected components.
 *
 * For interface (1), the filtering is performed with a single function call.
 * This is the easiest way to do simple filtering.  These functions
 * are named pixSelectBy*() and pixaSelectBy*(), where the '*' is one of:
 *        Size
 *        PerimToAreaRatio
 *        PerimSizeRatio
 *        Area
 *        AreaFraction
 *        WidthHeightRatio
 *
 * For more complicated filtering, use the general method (2).
 * The numa indicator functions for a pixa are generated by these functions:
 *        pixaFindDimensions()
 *        pixaFindPerimToAreaRatio()
 *        pixaFindPerimSizeRatio()
 *        pixaFindAreaFraction()
 *        pixaCountPixels()
 *        pixaFindWidthHeightRatio()
 *        pixaFindWidthHeightProduct()
 *
 * Here is an illustration using the general method.  Suppose you want
 * all 8-connected components that have a height greater than 40 pixels,
 * a width not more than 30 pixels, between 150 and 300 fg pixels,
 * and a perimeter-to-size ratio between 1.2 and 2.0.
 *
 *        // Generate the pixa of 8 cc pieces.
 *    boxa = pixConnComp(pixs, &pixa, 8);
 *
 *        // Extract the data we need about each component.
 *    pixaFindDimensions(pixa, &naw, &nah);
 *    nas = pixaCountPixels(pixa);
 *    nar = pixaFindPerimSizeRatio(pixa);
 *
 *        // Build the indicator arrays for the set of components,
 *        // based on thresholds and selection criteria.
 *    na1 = numaMakeThresholdIndicator(nah, 40, L_SELECT_IF_GT);
 *    na2 = numaMakeThresholdIndicator(naw, 30, L_SELECT_IF_LTE);
 *    na3 = numaMakeThresholdIndicator(nas, 150, L_SELECT_IF_GTE);
 *    na4 = numaMakeThresholdIndicator(nas, 300, L_SELECT_IF_LTE);
 *    na5 = numaMakeThresholdIndicator(nar, 1.2, L_SELECT_IF_GTE);
 *    na6 = numaMakeThresholdIndicator(nar, 2.0, L_SELECT_IF_LTE);
 *
 *        // Combine the indicator arrays logically to find
 *        // the components that will be retained.
 *    nad = numaLogicalOp(NULL, na1, na2, L_INTERSECTION);
 *    numaLogicalOp(nad, nad, na3, L_INTERSECTION);
 *    numaLogicalOp(nad, nad, na4, L_INTERSECTION);
 *    numaLogicalOp(nad, nad, na5, L_INTERSECTION);
 *    numaLogicalOp(nad, nad, na6, L_INTERSECTION);
 *
 *        // Invert to get the components that will be removed.
 *    numaInvert(nad, nad);
 *
 *        // Remove the components, in-place.
 *    pixRemoveWithIndicator(pixs, pixa, nad);
 */


/*!
 * \brief   pixSelectBySize()
 *
 * \param[in]    pixs           1 bpp
 * \param[in]    width, height  threshold dimensions
 * \param[in]    connectivity   4 or 8
 * \param[in]    type           L_SELECT_WIDTH, L_SELECT_HEIGHT,
 *                              L_SELECT_IF_EITHER, L_SELECT_IF_BOTH
 * \param[in]    relation       L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                              L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \param[out]   pchanged       [optional] 1 if changed; 0 otherwise
 * \return  filtered pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The args specify constraints on the size of the
 *          components that are kept.
 *      (2) If unchanged, returns a copy of pixs.  Otherwise,
 *          returns a new pix with the filtered components.
 *      (3) If the selection type is L_SELECT_WIDTH, the input
 *          height is ignored, and v.v.
 *      (4) To keep small components, use relation = L_SELECT_IF_LT or
 *          L_SELECT_IF_LTE.
 *          To keep large components, use relation = L_SELECT_IF_GT or
 *          L_SELECT_IF_GTE.
 * </pre>
 */
PIX *
pixSelectBySize(PIX      *pixs,
                l_int32   width,
                l_int32   height,
                l_int32   connectivity,
                l_int32   type,
                l_int32   relation,
                l_int32  *pchanged)
{
l_int32  w, h, empty, changed, count;
BOXA    *boxa;
PIX     *pixd;
PIXA    *pixas, *pixad;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);
    if (type != L_SELECT_WIDTH && type != L_SELECT_HEIGHT &&
        type != L_SELECT_IF_EITHER && type != L_SELECT_IF_BOTH)
        return (PIX *)ERROR_PTR("invalid type", __func__, NULL);
    if (relation != L_SELECT_IF_LT && relation != L_SELECT_IF_GT &&
        relation != L_SELECT_IF_LTE && relation != L_SELECT_IF_GTE)
        return (PIX *)ERROR_PTR("invalid relation", __func__, NULL);
    if (pchanged) *pchanged = FALSE;

        /* Check if any components exist */
    pixZero(pixs, &empty);
    if (empty)
        return pixCopy(NULL, pixs);

        /* Identify and select the components */
    boxa = pixConnComp(pixs, &pixas, connectivity);
    pixad = pixaSelectBySize(pixas, width, height, type, relation, &changed);
    boxaDestroy(&boxa);
    pixaDestroy(&pixas);

    if (!changed) {
        pixaDestroy(&pixad);
        return pixCopy(NULL, pixs);
    }

        /* Render the result */
    if (pchanged) *pchanged = TRUE;
    pixGetDimensions(pixs, &w, &h, NULL);
    count = pixaGetCount(pixad);
    if (count == 0) {  /* return empty pix */
        pixd = pixCreateTemplate(pixs);
    } else {
        pixd = pixaDisplay(pixad, w, h);
        pixCopyResolution(pixd, pixs);
        pixCopyColormap(pixd, pixs);
        pixCopyText(pixd, pixs);
        pixCopyInputFormat(pixd, pixs);
    }
    pixaDestroy(&pixad);
    return pixd;
}


/*!
 * \brief   pixaSelectBySize()
 *
 * \param[in]    pixas
 * \param[in]    width, height  threshold dimensions
 * \param[in]    type           L_SELECT_WIDTH, L_SELECT_HEIGHT,
 *                              L_SELECT_IF_EITHER, L_SELECT_IF_BOTH
 * \param[in]    relation       L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                              L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \param[out]   pchanged       [optional] 1 if changed; 0 otherwise
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The args specify constraints on the size of the
 *          components that are kept.
 *      (2) Uses pix and box clones in the new pixa.
 *      (3) If the selection type is L_SELECT_WIDTH, the input
 *          height is ignored, and v.v.
 *      (4) To keep small components, use relation = L_SELECT_IF_LT or
 *          L_SELECT_IF_LTE.
 *          To keep large components, use relation = L_SELECT_IF_GT or
 *          L_SELECT_IF_GTE.
 * </pre>
 */
PIXA *
pixaSelectBySize(PIXA     *pixas,
                 l_int32   width,
                 l_int32   height,
                 l_int32   type,
                 l_int32   relation,
                 l_int32  *pchanged)
{
NUMA  *na;
PIXA  *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (type != L_SELECT_WIDTH && type != L_SELECT_HEIGHT &&
        type != L_SELECT_IF_EITHER && type != L_SELECT_IF_BOTH)
        return (PIXA *)ERROR_PTR("invalid type", __func__, NULL);
    if (relation != L_SELECT_IF_LT && relation != L_SELECT_IF_GT &&
        relation != L_SELECT_IF_LTE && relation != L_SELECT_IF_GTE)
        return (PIXA *)ERROR_PTR("invalid relation", __func__, NULL);

        /* Compute the indicator array for saving components */
    na = pixaMakeSizeIndicator(pixas, width, height, type, relation);

        /* Filter to get output */
    pixad = pixaSelectWithIndicator(pixas, na, pchanged);

    numaDestroy(&na);
    return pixad;
}


/*!
 * \brief   pixaMakeSizeIndicator()
 *
 * \param[in]    pixa
 * \param[in]    width, height  threshold dimensions
 * \param[in]    type           L_SELECT_WIDTH, L_SELECT_HEIGHT,
 *                              L_SELECT_IF_EITHER, L_SELECT_IF_BOTH
 * \param[in]    relation       L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                              L_SELECT_IF_LTE, L_SELECT_IF_GTE
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
pixaMakeSizeIndicator(PIXA     *pixa,
                      l_int32   width,
                      l_int32   height,
                      l_int32   type,
                      l_int32   relation)
{
l_int32  i, n, w, h, ival;
NUMA    *na;

    if (!pixa)
        return (NUMA *)ERROR_PTR("pixa not defined", __func__, NULL);
    if (type != L_SELECT_WIDTH && type != L_SELECT_HEIGHT &&
        type != L_SELECT_IF_EITHER && type != L_SELECT_IF_BOTH)
        return (NUMA *)ERROR_PTR("invalid type", __func__, NULL);
    if (relation != L_SELECT_IF_LT && relation != L_SELECT_IF_GT &&
        relation != L_SELECT_IF_LTE && relation != L_SELECT_IF_GTE)
        return (NUMA *)ERROR_PTR("invalid relation", __func__, NULL);

    n = pixaGetCount(pixa);
    na = numaCreate(n);
    for (i = 0; i < n; i++) {
        ival = 0;
        pixaGetPixDimensions(pixa, i, &w, &h, NULL);
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
 * \brief   pixSelectByPerimToAreaRatio()
 *
 * \param[in]    pixs          1 bpp
 * \param[in]    thresh        threshold ratio of fg boundary to fg pixels
 * \param[in]    connectivity  4 or 8
 * \param[in]    type          L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                             L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \param[out]   pchanged      [optional] 1 if changed; 0 if clone returned
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The args specify constraints on the size of the
 *          components that are kept.
 *      (2) If unchanged, returns a copy of pixs.  Otherwise,
 *          returns a new pix with the filtered components.
 *      (3) This filters "thick" components, where a thick component
 *          is defined to have a ratio of boundary to interior pixels
 *          that is smaller than a given threshold value.
 *      (4) Use L_SELECT_IF_LT or L_SELECT_IF_LTE to save the thicker
 *          components, and L_SELECT_IF_GT or L_SELECT_IF_GTE to remove them.
 * </pre>
 */
PIX *
pixSelectByPerimToAreaRatio(PIX       *pixs,
                            l_float32  thresh,
                            l_int32    connectivity,
                            l_int32    type,
                            l_int32   *pchanged)
{
l_int32  w, h, empty, changed, count;
BOXA    *boxa;
PIX     *pixd;
PIXA    *pixas, *pixad;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);
    if (type != L_SELECT_IF_LT && type != L_SELECT_IF_GT &&
        type != L_SELECT_IF_LTE && type != L_SELECT_IF_GTE)
        return (PIX *)ERROR_PTR("invalid type", __func__, NULL);
    if (pchanged) *pchanged = FALSE;

        /* Check if any components exist */
    pixZero(pixs, &empty);
    if (empty)
        return pixCopy(NULL, pixs);

        /* Filter thin components */
    boxa = pixConnComp(pixs, &pixas, connectivity);
    pixad = pixaSelectByPerimToAreaRatio(pixas, thresh, type, &changed);
    boxaDestroy(&boxa);
    pixaDestroy(&pixas);

    if (!changed) {
        pixaDestroy(&pixad);
        return pixCopy(NULL, pixs);
    }

        /* Render the result */
    if (pchanged) *pchanged = TRUE;
    pixGetDimensions(pixs, &w, &h, NULL);
    count = pixaGetCount(pixad);
    if (count == 0) {  /* return empty pix */
        pixd = pixCreateTemplate(pixs);
    } else {
        pixd = pixaDisplay(pixad, w, h);
        pixCopyResolution(pixd, pixs);
        pixCopyColormap(pixd, pixs);
        pixCopyText(pixd, pixs);
        pixCopyInputFormat(pixd, pixs);
    }
    pixaDestroy(&pixad);
    return pixd;
}


/*!
 * \brief   pixaSelectByPerimToAreaRatio()
 *
 * \param[in]    pixas
 * \param[in]    thresh     threshold ratio of fg boundary to fg pixels
 * \param[in]    type       L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                          L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \param[out]   pchanged   [optional] 1 if changed; 0 if clone returned
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Returns a pixa clone if no components are removed.
 *      (2) Uses pix and box clones in the new pixa.
 *      (3) See pixSelectByPerimToAreaRatio().
 * </pre>
 */
PIXA *
pixaSelectByPerimToAreaRatio(PIXA      *pixas,
                             l_float32  thresh,
                             l_int32    type,
                             l_int32   *pchanged)
{
NUMA  *na, *nai;
PIXA  *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (type != L_SELECT_IF_LT && type != L_SELECT_IF_GT &&
        type != L_SELECT_IF_LTE && type != L_SELECT_IF_GTE)
        return (PIXA *)ERROR_PTR("invalid type", __func__, NULL);

        /* Compute component ratios. */
    na = pixaFindPerimToAreaRatio(pixas);

        /* Generate indicator array for elements to be saved. */
    nai = numaMakeThresholdIndicator(na, thresh, type);
    numaDestroy(&na);

        /* Filter to get output */
    pixad = pixaSelectWithIndicator(pixas, nai, pchanged);

    numaDestroy(&nai);
    return pixad;
}


/*!
 * \brief   pixSelectByPerimSizeRatio()
 *
 * \param[in]    pixs          1 bpp
 * \param[in]    thresh        threshold ratio of fg boundary to fg pixels
 * \param[in]    connectivity  4 or 8
 * \param[in]    type          L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                             L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \param[out]   pchanged      [optional] 1 if changed; 0 if clone returned
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The args specify constraints on the size of the
 *          components that are kept.
 *      (2) If unchanged, returns a copy of pixs.  Otherwise,
 *          returns a new pix with the filtered components.
 *      (3) This filters components with smooth vs. dendritic shape, using
 *          the ratio of the fg boundary pixels to the circumference of
 *          the bounding box, and comparing it to a threshold value.
 *      (4) Use L_SELECT_IF_LT or L_SELECT_IF_LTE to save the smooth
 *          boundary components, and L_SELECT_IF_GT or L_SELECT_IF_GTE
 *          to remove them.
 * </pre>
 */
PIX *
pixSelectByPerimSizeRatio(PIX       *pixs,
                          l_float32  thresh,
                          l_int32    connectivity,
                          l_int32    type,
                          l_int32   *pchanged)
{
l_int32  w, h, empty, changed, count;
BOXA    *boxa;
PIX     *pixd;
PIXA    *pixas, *pixad;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);
    if (type != L_SELECT_IF_LT && type != L_SELECT_IF_GT &&
        type != L_SELECT_IF_LTE && type != L_SELECT_IF_GTE)
        return (PIX *)ERROR_PTR("invalid type", __func__, NULL);
    if (pchanged) *pchanged = FALSE;

        /* Check if any components exist */
    pixZero(pixs, &empty);
    if (empty)
        return pixCopy(NULL, pixs);

        /* Filter thin components */
    boxa = pixConnComp(pixs, &pixas, connectivity);
    pixad = pixaSelectByPerimSizeRatio(pixas, thresh, type, &changed);
    boxaDestroy(&boxa);
    pixaDestroy(&pixas);

    if (!changed) {
        pixaDestroy(&pixad);
        return pixCopy(NULL, pixs);
    }

        /* Render the result */
    if (pchanged) *pchanged = TRUE;
    pixGetDimensions(pixs, &w, &h, NULL);
    count = pixaGetCount(pixad);
    if (count == 0) {  /* return empty pix */
        pixd = pixCreateTemplate(pixs);
    } else {
        pixd = pixaDisplay(pixad, w, h);
        pixCopyResolution(pixd, pixs);
        pixCopyColormap(pixd, pixs);
        pixCopyText(pixd, pixs);
        pixCopyInputFormat(pixd, pixs);
    }
    pixaDestroy(&pixad);
    return pixd;
}


/*!
 * \brief   pixaSelectByPerimSizeRatio()
 *
 * \param[in]    pixas
 * \param[in]    thresh    threshold ratio of fg boundary to b.b. circumference
 * \param[in]    type      L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                         L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \param[out]   pchanged  [optional] 1 if changed; 0 if clone returned
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Returns a pixa clone if no components are removed.
 *      (2) Uses pix and box clones in the new pixa.
 *      (3) See pixSelectByPerimSizeRatio().
 * </pre>
 */
PIXA *
pixaSelectByPerimSizeRatio(PIXA      *pixas,
                           l_float32  thresh,
                           l_int32    type,
                           l_int32   *pchanged)
{
NUMA  *na, *nai;
PIXA  *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (type != L_SELECT_IF_LT && type != L_SELECT_IF_GT &&
        type != L_SELECT_IF_LTE && type != L_SELECT_IF_GTE)
        return (PIXA *)ERROR_PTR("invalid type", __func__, NULL);

        /* Compute component ratios. */
    na = pixaFindPerimSizeRatio(pixas);

        /* Generate indicator array for elements to be saved. */
    nai = numaMakeThresholdIndicator(na, thresh, type);
    numaDestroy(&na);

        /* Filter to get output */
    pixad = pixaSelectWithIndicator(pixas, nai, pchanged);

    numaDestroy(&nai);
    return pixad;
}


/*!
 * \brief   pixSelectByAreaFraction()
 *
 * \param[in]    pixs          1 bpp
 * \param[in]    thresh        threshold ratio of fg pixels to (w * h)
 * \param[in]    connectivity  4 or 8
 * \param[in]    type          L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                             L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \param[out]   pchanged      [optional] 1 if changed; 0 if clone returned
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The args specify constraints on the amount of foreground
 *          coverage of the components that are kept.
 *      (2) If unchanged, returns a copy of pixs.  Otherwise,
 *          returns a new pix with the filtered components.
 *      (3) This filters components based on the fraction of fg pixels
 *          of the component in its bounding box.
 *      (4) Use L_SELECT_IF_LT or L_SELECT_IF_LTE to save components
 *          with less than the threshold fraction of foreground, and
 *          L_SELECT_IF_GT or L_SELECT_IF_GTE to remove them.
 * </pre>
 */
PIX *
pixSelectByAreaFraction(PIX       *pixs,
                        l_float32  thresh,
                        l_int32    connectivity,
                        l_int32    type,
                        l_int32   *pchanged)
{
l_int32  w, h, empty, changed, count;
BOXA    *boxa;
PIX     *pixd;
PIXA    *pixas, *pixad;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);
    if (type != L_SELECT_IF_LT && type != L_SELECT_IF_GT &&
        type != L_SELECT_IF_LTE && type != L_SELECT_IF_GTE)
        return (PIX *)ERROR_PTR("invalid type", __func__, NULL);
    if (pchanged) *pchanged = FALSE;

        /* Check if any components exist */
    pixZero(pixs, &empty);
    if (empty)
        return pixCopy(NULL, pixs);

        /* Filter components */
    boxa = pixConnComp(pixs, &pixas, connectivity);
    pixad = pixaSelectByAreaFraction(pixas, thresh, type, &changed);
    boxaDestroy(&boxa);
    pixaDestroy(&pixas);

    if (!changed) {
        pixaDestroy(&pixad);
        return pixCopy(NULL, pixs);
    }

        /* Render the result */
    if (pchanged) *pchanged = TRUE;
    pixGetDimensions(pixs, &w, &h, NULL);
    count = pixaGetCount(pixad);
    if (count == 0) {  /* return empty pix */
        pixd = pixCreateTemplate(pixs);
    } else {
        pixd = pixaDisplay(pixad, w, h);
        pixCopyResolution(pixd, pixs);
        pixCopyColormap(pixd, pixs);
        pixCopyText(pixd, pixs);
        pixCopyInputFormat(pixd, pixs);
    }
    pixaDestroy(&pixad);
    return pixd;
}


/*!
 * \brief   pixaSelectByAreaFraction()
 *
 * \param[in]    pixas
 * \param[in]    thresh      threshold ratio of fg pixels to (w * h)
 * \param[in]    type        L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                           L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \param[out]   pchanged    [optional] 1 if changed; 0 if clone returned
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Returns a pixa clone if no components are removed.
 *      (2) Uses pix and box clones in the new pixa.
 *      (3) This filters components based on the fraction of fg pixels
 *          of the component in its bounding box.
 *      (4) Use L_SELECT_IF_LT or L_SELECT_IF_LTE to save components
 *          with less than the threshold fraction of foreground, and
 *          L_SELECT_IF_GT or L_SELECT_IF_GTE to remove them.
 * </pre>
 */
PIXA *
pixaSelectByAreaFraction(PIXA      *pixas,
                         l_float32  thresh,
                         l_int32    type,
                         l_int32   *pchanged)
{
NUMA  *na, *nai;
PIXA  *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (type != L_SELECT_IF_LT && type != L_SELECT_IF_GT &&
        type != L_SELECT_IF_LTE && type != L_SELECT_IF_GTE)
        return (PIXA *)ERROR_PTR("invalid type", __func__, NULL);

        /* Compute component ratios. */
    na = pixaFindAreaFraction(pixas);

        /* Generate indicator array for elements to be saved. */
    nai = numaMakeThresholdIndicator(na, thresh, type);
    numaDestroy(&na);

        /* Filter to get output */
    pixad = pixaSelectWithIndicator(pixas, nai, pchanged);

    numaDestroy(&nai);
    return pixad;
}


/*!
 * \brief   pixSelectByArea()
 *
 * \param[in]    pixs          1 bpp
 * \param[in]    thresh        threshold number of FG pixels
 * \param[in]    connectivity  4 or 8
 * \param[in]    type          L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                             L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \param[out]   pchanged      [optional] 1 if changed; 0 if clone returned
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The args specify constraints on the number of foreground
 *          pixels in the components that are kept.
 *      (2) If unchanged, returns a copy of pixs.  Otherwise,
 *          returns a new pix with the filtered components.
 *      (3) This filters components based on the number of fg pixels
 *          in each component.
 *      (4) Use L_SELECT_IF_LT or L_SELECT_IF_LTE to save components
 *          with less than the threshold number of fg pixels, and
 *          L_SELECT_IF_GT or L_SELECT_IF_GTE to remove them.
 * </pre>
 */
PIX *
pixSelectByArea(PIX       *pixs,
                l_float32  thresh,
                l_int32    connectivity,
                l_int32    type,
                l_int32   *pchanged)
{
l_int32  w, h, empty, changed, count;
BOXA    *boxa;
PIX     *pixd;
PIXA    *pixas, *pixad;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);
    if (type != L_SELECT_IF_LT && type != L_SELECT_IF_GT &&
        type != L_SELECT_IF_LTE && type != L_SELECT_IF_GTE)
        return (PIX *)ERROR_PTR("invalid type", __func__, NULL);
    if (pchanged) *pchanged = FALSE;

        /* Check if any components exist */
    pixZero(pixs, &empty);
    if (empty)
        return pixCopy(NULL, pixs);

        /* Filter components */
    boxa = pixConnComp(pixs, &pixas, connectivity);
    pixad = pixaSelectByArea(pixas, thresh, type, &changed);
    boxaDestroy(&boxa);
    pixaDestroy(&pixas);

    if (!changed) {
        pixaDestroy(&pixad);
        return pixCopy(NULL, pixs);
    }

        /* Render the result */
    if (pchanged) *pchanged = TRUE;
    pixGetDimensions(pixs, &w, &h, NULL);
    count = pixaGetCount(pixad);
    if (count == 0) {  /* return empty pix */
        pixd = pixCreateTemplate(pixs);
    } else {
        pixd = pixaDisplay(pixad, w, h);
        pixCopyResolution(pixd, pixs);
        pixCopyColormap(pixd, pixs);
        pixCopyText(pixd, pixs);
        pixCopyInputFormat(pixd, pixs);
    }
    pixaDestroy(&pixad);
    return pixd;
}


/*!
 * \brief   pixaSelectByArea()
 *
 * \param[in]    pixas
 * \param[in]    thresh      threshold number of fg pixels
 * \param[in]    type        L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                           L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \param[out]   pchanged    [optional] 1 if changed; 0 if clone returned
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Returns a pixa clone if no components are removed.
 *      (2) Uses pix and box clones in the new pixa.
 *      (3) This filters components based on the number of fg pixels
 *          in the component.
 *      (4) Use L_SELECT_IF_LT or L_SELECT_IF_LTE to save components
 *          with less than the threshold number of fg pixels, and
 *          L_SELECT_IF_GT or L_SELECT_IF_GTE to remove them.
 * </pre>
 */
PIXA *
pixaSelectByArea(PIXA      *pixas,
                 l_float32  thresh,
                 l_int32    type,
                 l_int32   *pchanged)
{
NUMA  *na, *nai;
PIXA  *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (type != L_SELECT_IF_LT && type != L_SELECT_IF_GT &&
        type != L_SELECT_IF_LTE && type != L_SELECT_IF_GTE)
        return (PIXA *)ERROR_PTR("invalid type", __func__, NULL);

        /* Compute area of each component */
    na = pixaCountPixels(pixas);

        /* Generate indicator array for elements to be saved. */
    nai = numaMakeThresholdIndicator(na, thresh, type);
    numaDestroy(&na);

        /* Filter to get output */
    pixad = pixaSelectWithIndicator(pixas, nai, pchanged);

    numaDestroy(&nai);
    return pixad;
}


/*!
 * \brief   pixSelectByWidthHeightRatio()
 *
 * \param[in]    pixs          1 bpp
 * \param[in]    thresh        threshold ratio of width/height
 * \param[in]    connectivity  4 or 8
 * \param[in]    type          L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                             L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \param[out]   pchanged      [optional] 1 if changed; 0 if clone returned
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The args specify constraints on the width-to-height ratio
 *          for components that are kept.
 *      (2) If unchanged, returns a copy of pixs.  Otherwise,
 *          returns a new pix with the filtered components.
 *      (3) This filters components based on the width-to-height ratios.
 *      (4) Use L_SELECT_IF_LT or L_SELECT_IF_LTE to save components
 *          with less than the threshold ratio, and
 *          L_SELECT_IF_GT or L_SELECT_IF_GTE to remove them.
 * </pre>
 */
PIX *
pixSelectByWidthHeightRatio(PIX       *pixs,
                            l_float32  thresh,
                            l_int32    connectivity,
                            l_int32    type,
                            l_int32   *pchanged)
{
l_int32  w, h, empty, changed, count;
BOXA    *boxa;
PIX     *pixd;
PIXA    *pixas, *pixad;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);
    if (type != L_SELECT_IF_LT && type != L_SELECT_IF_GT &&
        type != L_SELECT_IF_LTE && type != L_SELECT_IF_GTE)
        return (PIX *)ERROR_PTR("invalid type", __func__, NULL);
    if (pchanged) *pchanged = FALSE;

        /* Check if any components exist */
    pixZero(pixs, &empty);
    if (empty)
        return pixCopy(NULL, pixs);

        /* Filter components */
    boxa = pixConnComp(pixs, &pixas, connectivity);
    pixad = pixaSelectByWidthHeightRatio(pixas, thresh, type, &changed);
    boxaDestroy(&boxa);
    pixaDestroy(&pixas);

    if (!changed) {
        pixaDestroy(&pixad);
        return pixCopy(NULL, pixs);
    }

        /* Render the result */
    if (pchanged) *pchanged = TRUE;
    pixGetDimensions(pixs, &w, &h, NULL);
    count = pixaGetCount(pixad);
    if (count == 0) {  /* return empty pix */
        pixd = pixCreateTemplate(pixs);
    } else {
        pixd = pixaDisplay(pixad, w, h);
        pixCopyResolution(pixd, pixs);
        pixCopyColormap(pixd, pixs);
        pixCopyText(pixd, pixs);
        pixCopyInputFormat(pixd, pixs);
    }
    pixaDestroy(&pixad);
    return pixd;
}


/*!
 * \brief   pixaSelectByWidthHeightRatio()
 *
 * \param[in]    pixas
 * \param[in]    thresh      threshold ratio of width/height
 * \param[in]    type        L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                           L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \param[out]   pchanged    [optional] 1 if changed; 0 if clone returned
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Returns a pixa clone if no components are removed.
 *      (2) Uses pix and box clones in the new pixa.
 *      (3) This filters components based on the width-to-height ratio
 *          of each pix.
 *      (4) Use L_SELECT_IF_LT or L_SELECT_IF_LTE to save components
 *          with less than the threshold ratio, and
 *          L_SELECT_IF_GT or L_SELECT_IF_GTE to remove them.
 * </pre>
 */
PIXA *
pixaSelectByWidthHeightRatio(PIXA      *pixas,
                             l_float32  thresh,
                             l_int32    type,
                             l_int32   *pchanged)
{
NUMA  *na, *nai;
PIXA  *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (type != L_SELECT_IF_LT && type != L_SELECT_IF_GT &&
        type != L_SELECT_IF_LTE && type != L_SELECT_IF_GTE)
        return (PIXA *)ERROR_PTR("invalid type", __func__, NULL);

        /* Compute component ratios. */
    na = pixaFindWidthHeightRatio(pixas);

        /* Generate indicator array for elements to be saved. */
    nai = numaMakeThresholdIndicator(na, thresh, type);
    numaDestroy(&na);

        /* Filter to get output */
    pixad = pixaSelectWithIndicator(pixas, nai, pchanged);

    numaDestroy(&nai);
    return pixad;
}


/*!
 * \brief   pixaSelectByNumConnComp()
 *
 * \param[in]    pixas
 * \param[in]    nmin          minimum number of components
 * \param[in]    nmax          maximum number of components
 * \param[in]    connectivity  4 or 8
 * \param[out]   pchanged      [optional] 1 if changed; 0 if clone returned
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Returns a pixa clone if no components are removed.
 *      (2) Uses pix and box clones in the new pixa.
 *      (3) This filters by the number of connected components in
 *          a given range.
 * </pre>
 */
PIXA *
pixaSelectByNumConnComp(PIXA      *pixas,
                        l_int32    nmin,
                        l_int32    nmax,
                        l_int32    connectivity,
                        l_int32   *pchanged)
{
l_int32  n, i, count;
NUMA    *na;
PIX     *pix;
PIXA    *pixad;

    if (pchanged) *pchanged = 0;
    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (nmin > nmax)
        return (PIXA *)ERROR_PTR("nmin > nmax", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PIXA *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);

        /* Get indicator array based on number of c.c. */
    n = pixaGetCount(pixas);
    na = numaCreate(n);
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixas, i, L_CLONE);
        pixCountConnComp(pix, connectivity, &count);
        if (count >= nmin && count <= nmax)
            numaAddNumber(na, 1);
        else
            numaAddNumber(na, 0);
        pixDestroy(&pix);
    }

        /* Filter to get output */
    pixad = pixaSelectWithIndicator(pixas, na, pchanged);
    numaDestroy(&na);
    return pixad;
}


/*!
 * \brief   pixaSelectWithIndicator()
 *
 * \param[in]    pixas
 * \param[in]    na         indicator numa
 * \param[out]   pchanged   [optional] 1 if changed; 0 if clone returned
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Returns a pixa clone if no components are removed.
 *      (2) Uses pix and box clones in the new pixa.
 *      (3) The indicator numa has values 0 (ignore) and 1 (accept).
 *      (4) If the source boxa is not fully populated, it is left
 *          empty in the dest pixa.
 * </pre>
 */
PIXA *
pixaSelectWithIndicator(PIXA     *pixas,
                        NUMA     *na,
                        l_int32  *pchanged)
{
l_int32  i, n, nbox, ival, nsave;
BOX     *box;
PIX     *pix1;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (!na)
        return (PIXA *)ERROR_PTR("na not defined", __func__, NULL);

    nsave = 0;
    n = numaGetCount(na);
    for (i = 0; i < n; i++) {
        numaGetIValue(na, i, &ival);
        if (ival == 1) nsave++;
    }

    if (nsave == n) {
        if (pchanged) *pchanged = FALSE;
        return pixaCopy(pixas, L_CLONE);
    }
    if (pchanged) *pchanged = TRUE;
    pixad = pixaCreate(nsave);
    nbox = pixaGetBoxaCount(pixas);
    for (i = 0; i < n; i++) {
        numaGetIValue(na, i, &ival);
        if (ival == 0) continue;
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        pixaAddPix(pixad, pix1, L_INSERT);
        if (nbox == n) {   /* fully populated boxa */
            box = pixaGetBox(pixas, i, L_CLONE);
            pixaAddBox(pixad, box, L_INSERT);
        }
    }

    return pixad;
}


/*!
 * \brief   pixRemoveWithIndicator()
 *
 * \param[in]    pixs     1 bpp pix from which components are removed; in-place
 * \param[in]    pixa     of connected components in pixs
 * \param[in]    na       numa indicator: remove components corresponding to 1s
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This complements pixAddWithIndicator().   Here, the selected
 *          components are set subtracted from pixs.
 * </pre>
 */
l_ok
pixRemoveWithIndicator(PIX   *pixs,
                       PIXA  *pixa,
                       NUMA  *na)
{
l_int32  i, n, ival, x, y, w, h;
BOX     *box;
PIX     *pix;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    n = pixaGetCount(pixa);
    if (n != numaGetCount(na))
        return ERROR_INT("pixa and na sizes not equal", __func__, 1);

    for (i = 0; i < n; i++) {
        numaGetIValue(na, i, &ival);
        if (ival == 1) {
            pix = pixaGetPix(pixa, i, L_CLONE);
            box = pixaGetBox(pixa, i, L_CLONE);
            boxGetGeometry(box, &x, &y, &w, &h);
            pixRasterop(pixs, x, y, w, h, PIX_DST & PIX_NOT(PIX_SRC),
                        pix, 0, 0);
            boxDestroy(&box);
            pixDestroy(&pix);
        }
    }

    return 0;
}


/*!
 * \brief   pixAddWithIndicator()
 *
 * \param[in]    pixs     1 bpp pix from which components are added; in-place
 * \param[in]    pixa     of connected components, some of which will be put
 *                        into pixs
 * \param[in]    na       numa indicator: add components corresponding to 1s
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This complements pixRemoveWithIndicator().   Here, the selected
 *          components are added to pixs.
 * </pre>
 */
l_ok
pixAddWithIndicator(PIX   *pixs,
                    PIXA  *pixa,
                    NUMA  *na)
{
l_int32  i, n, ival, x, y, w, h;
BOX     *box;
PIX     *pix;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    n = pixaGetCount(pixa);
    if (n != numaGetCount(na))
        return ERROR_INT("pixa and na sizes not equal", __func__, 1);

    for (i = 0; i < n; i++) {
        numaGetIValue(na, i, &ival);
        if (ival == 1) {
            pix = pixaGetPix(pixa, i, L_CLONE);
            box = pixaGetBox(pixa, i, L_CLONE);
            boxGetGeometry(box, &x, &y, &w, &h);
            pixRasterop(pixs, x, y, w, h, PIX_SRC | PIX_DST, pix, 0, 0);
            boxDestroy(&box);
            pixDestroy(&pix);
        }
    }

    return 0;
}


/*!
 * \brief   pixaSelectWithString()
 *
 * \param[in]    pixas
 * \param[in]    str      string of indices into pixa, giving the pix to
 *                        be selected
 * \param[out]   perror   [optional] 1 if any indices are invalid;
 *                        0 if all indices are valid
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Returns a pixa with copies of selected pix.
 *      (2) Associated boxes are also copied, if fully populated.
 * </pre>
 */
PIXA *
pixaSelectWithString(PIXA        *pixas,
                     const char  *str,
                     l_int32     *perror)
{
l_int32    i, nval, npix, nbox, val, imaxval;
l_float32  maxval;
BOX       *box;
NUMA      *na;
PIX       *pix1;
PIXA      *pixad;

    if (perror) *perror = 0;
    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (!str)
        return (PIXA *)ERROR_PTR("str not defined", __func__, NULL);

    if ((na = numaCreateFromString(str)) == NULL)
        return (PIXA *)ERROR_PTR("na not made", __func__, NULL);
    if ((nval = numaGetCount(na)) == 0) {
        numaDestroy(&na);
        return (PIXA *)ERROR_PTR("no indices found", __func__, NULL);
    }
    numaGetMax(na, &maxval, NULL);
    imaxval = (l_int32)(maxval + 0.1);
    nbox = pixaGetBoxaCount(pixas);
    npix = pixaGetCount(pixas);
    if (imaxval >= npix) {
        if (perror) *perror = 1;
        L_ERROR("max index = %d, size of pixa = %d\n", __func__, imaxval, npix);
    }

    pixad = pixaCreate(nval);
    for (i = 0; i < nval; i++) {
        numaGetIValue(na, i, &val);
        if (val < 0 || val >= npix) {
            L_ERROR("index %d out of range of pix\n", __func__, val);
            continue;
        }
        pix1 = pixaGetPix(pixas, val, L_COPY);
        pixaAddPix(pixad, pix1, L_INSERT);
        if (nbox == npix) {   /* fully populated boxa */
            box = pixaGetBox(pixas, val, L_COPY);
            pixaAddBox(pixad, box, L_INSERT);
        }
    }
    numaDestroy(&na);
    return pixad;
}


/*!
 * \brief   pixaRenderComponent()
 *
 * \param[in]    pixs    [optional] 1 bpp pix
 * \param[in]    pixa    of 1 bpp connected components, one of which will
 *                       be rendered in pixs, with its origin determined
 *                       by the associated box.
 * \param[in]    index   of component to be rendered
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If pixs is null, this generates an empty pix of a size determined
 *          by union of the component bounding boxes, and including the origin.
 *      (2) The selected component is blitted into pixs.
 * </pre>
 */
PIX *
pixaRenderComponent(PIX     *pixs,
                    PIXA    *pixa,
                    l_int32  index)
{
l_int32  n, x, y, w, h, same, maxd;
BOX     *box;
BOXA    *boxa;
PIX     *pix;

    if (!pixa)
        return (PIX *)ERROR_PTR("pixa not defined", __func__, pixs);
    n = pixaGetCount(pixa);
    if (index < 0 || index >= n)
        return (PIX *)ERROR_PTR("invalid index", __func__, pixs);
    if (pixs && (pixGetDepth(pixs) != 1))
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, pixs);
    pixaVerifyDepth(pixa, &same, &maxd);
    if (maxd > 1)
        return (PIX *)ERROR_PTR("not all pix with d == 1", __func__, pixs);

    boxa = pixaGetBoxa(pixa, L_CLONE);
    if (!pixs) {
        boxaGetExtent(boxa, &w, &h, NULL);
        pixs = pixCreate(w, h, 1);
    }

    pix = pixaGetPix(pixa, index, L_CLONE);
    box = boxaGetBox(boxa, index, L_CLONE);
    boxGetGeometry(box, &x, &y, &w, &h);
    pixRasterop(pixs, x, y, w, h, PIX_SRC | PIX_DST, pix, 0, 0);
    boxDestroy(&box);
    pixDestroy(&pix);
    boxaDestroy(&boxa);

    return pixs;
}


/*---------------------------------------------------------------------*
 *                              Sort functions                         *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaSort()
 *
 * \param[in]    pixas
 * \param[in]    sorttype   L_SORT_BY_X, L_SORT_BY_Y, L_SORT_BY_WIDTH,
 *                          L_SORT_BY_HEIGHT, L_SORT_BY_MIN_DIMENSION,
 *                          L_SORT_BY_MAX_DIMENSION, L_SORT_BY_PERIMETER,
 *                          L_SORT_BY_AREA, L_SORT_BY_ASPECT_RATIO
 * \param[in]    sortorder  L_SORT_INCREASING, L_SORT_DECREASING
 * \param[out]   pnaindex   [optional] index of sorted order into
 *                          original array
 * \param[in]    copyflag   L_COPY, L_CLONE
 * \return  pixad sorted version of pixas, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This sorts based on the data in the boxa.  If the boxa
 *          count is not the same as the pixa count, this returns an error.
 *      (2) If the boxa is empty, it makes one corresponding to the
 *          dimensions of each pix, which allows meaningful sorting on
 *          all types except x and y.
 *      (3) The copyflag refers to the pix and box copies that are
 *          inserted into the sorted pixa.  These are either L_COPY
 *          or L_CLONE.
 * </pre>
 */
PIXA *
pixaSort(PIXA    *pixas,
         l_int32  sorttype,
         l_int32  sortorder,
         NUMA   **pnaindex,
         l_int32  copyflag)
{
l_int32  i, n, nb, x, y, w, h;
BOXA    *boxa;
NUMA    *na, *naindex;
PIXA    *pixad;

    if (pnaindex) *pnaindex = NULL;
    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (sorttype != L_SORT_BY_X && sorttype != L_SORT_BY_Y &&
        sorttype != L_SORT_BY_WIDTH && sorttype != L_SORT_BY_HEIGHT &&
        sorttype != L_SORT_BY_MIN_DIMENSION &&
        sorttype != L_SORT_BY_MAX_DIMENSION &&
        sorttype != L_SORT_BY_PERIMETER &&
        sorttype != L_SORT_BY_AREA &&
        sorttype != L_SORT_BY_ASPECT_RATIO)
        return (PIXA *)ERROR_PTR("invalid sort type", __func__, NULL);
    if (sortorder != L_SORT_INCREASING && sortorder != L_SORT_DECREASING)
        return (PIXA *)ERROR_PTR("invalid sort order", __func__, NULL);
    if (copyflag != L_COPY && copyflag != L_CLONE)
        return (PIXA *)ERROR_PTR("invalid copy flag", __func__, NULL);

        /* Check the pixa and boxa counts. Make a boxa if required. */
    if ((n = pixaGetCount(pixas)) == 0) {
        L_INFO("no pix in pixa\n", __func__);
        return pixaCopy(pixas, copyflag);
    }
    if ((boxa = pixas->boxa) == NULL)   /* not owned; do not destroy */
        return (PIXA *)ERROR_PTR("boxa not found!", __func__, NULL);
    nb = boxaGetCount(boxa);
    if (nb == 0) {
        pixaSetFullSizeBoxa(pixas);
        nb = n;
        boxa = pixas->boxa;  /* not owned */
        if (sorttype == L_SORT_BY_X || sorttype == L_SORT_BY_Y)
            L_WARNING("sort by x or y where all values are 0\n", __func__);
    }
    if (nb != n)
        return (PIXA *)ERROR_PTR("boxa and pixa counts differ", __func__, NULL);

        /* Use O(n) binsort if possible */
    if (n > MinCompsForBinSort &&
        ((sorttype == L_SORT_BY_X) || (sorttype == L_SORT_BY_Y) ||
         (sorttype == L_SORT_BY_WIDTH) || (sorttype == L_SORT_BY_HEIGHT) ||
         (sorttype == L_SORT_BY_PERIMETER)))
        return pixaBinSort(pixas, sorttype, sortorder, pnaindex, copyflag);

        /* Build up numa of specific data */
    if ((na = numaCreate(n)) == NULL)
        return (PIXA *)ERROR_PTR("na not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        boxaGetBoxGeometry(boxa, i, &x, &y, &w, &h);
        switch (sorttype)
        {
        case L_SORT_BY_X:
            numaAddNumber(na, x);
            break;
        case L_SORT_BY_Y:
            numaAddNumber(na, y);
            break;
        case L_SORT_BY_WIDTH:
            numaAddNumber(na, w);
            break;
        case L_SORT_BY_HEIGHT:
            numaAddNumber(na, h);
            break;
        case L_SORT_BY_MIN_DIMENSION:
            numaAddNumber(na, L_MIN(w, h));
            break;
        case L_SORT_BY_MAX_DIMENSION:
            numaAddNumber(na, L_MAX(w, h));
            break;
        case L_SORT_BY_PERIMETER:
            numaAddNumber(na, w + h);
            break;
        case L_SORT_BY_AREA:
            numaAddNumber(na, w * h);
            break;
        case L_SORT_BY_ASPECT_RATIO:
            numaAddNumber(na, (l_float32)w / (l_float32)h);
            break;
        default:
            L_WARNING("invalid sort type\n", __func__);
        }
    }

        /* Get the sort index for data array */
    naindex = numaGetSortIndex(na, sortorder);
    numaDestroy(&na);
    if (!naindex)
        return (PIXA *)ERROR_PTR("naindex not made", __func__, NULL);

        /* Build up sorted pixa using sort index */
    if ((pixad = pixaSortByIndex(pixas, naindex, copyflag)) == NULL) {
        numaDestroy(&naindex);
        return (PIXA *)ERROR_PTR("pixad not made", __func__, NULL);
    }

    if (pnaindex)
        *pnaindex = naindex;
    else
        numaDestroy(&naindex);
    return pixad;
}


/*!
 * \brief   pixaBinSort()
 *
 * \param[in]    pixas
 * \param[in]    sorttype    L_SORT_BY_X, L_SORT_BY_Y, L_SORT_BY_WIDTH,
 *                           L_SORT_BY_HEIGHT, L_SORT_BY_PERIMETER
 * \param[in]    sortorder   L_SORT_INCREASING, L_SORT_DECREASING
 * \param[out]   pnaindex    [optional] index of sorted order into
 *                           original array
 * \param[in]    copyflag    L_COPY, L_CLONE
 * \return  pixad sorted version of pixas, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This sorts based on the data in the boxa.  If the boxa
 *          count is not the same as the pixa count, this returns an error.
 *      (2) The copyflag refers to the pix and box copies that are
 *          inserted into the sorted pixa.  These are either L_COPY
 *          or L_CLONE.
 *      (3) For a large number of boxes (say, greater than 1000), this
 *          O(n) binsort is much faster than the O(nlogn) shellsort.
 *          For 5000 components, this is over 20x faster than boxaSort().
 *      (4) Consequently, pixaSort() calls this function if it will
 *          likely go much faster.
 * </pre>
 */
PIXA *
pixaBinSort(PIXA    *pixas,
            l_int32  sorttype,
            l_int32  sortorder,
            NUMA   **pnaindex,
            l_int32  copyflag)
{
l_int32  i, n, x, y, w, h;
BOXA    *boxa;
NUMA    *na, *naindex;
PIXA    *pixad;

    if (pnaindex) *pnaindex = NULL;
    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (sorttype != L_SORT_BY_X && sorttype != L_SORT_BY_Y &&
        sorttype != L_SORT_BY_WIDTH && sorttype != L_SORT_BY_HEIGHT &&
        sorttype != L_SORT_BY_PERIMETER)
        return (PIXA *)ERROR_PTR("invalid sort type", __func__, NULL);
    if (sortorder != L_SORT_INCREASING && sortorder != L_SORT_DECREASING)
        return (PIXA *)ERROR_PTR("invalid sort order", __func__, NULL);
    if (copyflag != L_COPY && copyflag != L_CLONE)
        return (PIXA *)ERROR_PTR("invalid copy flag", __func__, NULL);

        /* Verify that the pixa and its boxa have the same count */
    if ((boxa = pixas->boxa) == NULL)   /* not owned; do not destroy */
        return (PIXA *)ERROR_PTR("boxa not found", __func__, NULL);
    n = pixaGetCount(pixas);
    if (boxaGetCount(boxa) != n)
        return (PIXA *)ERROR_PTR("boxa and pixa counts differ", __func__, NULL);

        /* Generate Numa of appropriate box dimensions */
    if ((na = numaCreate(n)) == NULL)
        return (PIXA *)ERROR_PTR("na not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        boxaGetBoxGeometry(boxa, i, &x, &y, &w, &h);
        switch (sorttype)
        {
        case L_SORT_BY_X:
            numaAddNumber(na, x);
            break;
        case L_SORT_BY_Y:
            numaAddNumber(na, y);
            break;
        case L_SORT_BY_WIDTH:
            numaAddNumber(na, w);
            break;
        case L_SORT_BY_HEIGHT:
            numaAddNumber(na, h);
            break;
        case L_SORT_BY_PERIMETER:
            numaAddNumber(na, w + h);
            break;
        default:
            L_WARNING("invalid sort type\n", __func__);
        }
    }

        /* Get the sort index for data array */
    naindex = numaGetBinSortIndex(na, sortorder);
    numaDestroy(&na);
    if (!naindex)
        return (PIXA *)ERROR_PTR("naindex not made", __func__, NULL);

        /* Build up sorted pixa using sort index */
    if ((pixad = pixaSortByIndex(pixas, naindex, copyflag)) == NULL) {
        numaDestroy(&naindex);
        return (PIXA *)ERROR_PTR("pixad not made", __func__, NULL);
    }

    if (pnaindex)
        *pnaindex = naindex;
    else
        numaDestroy(&naindex);
    return pixad;
}


/*!
 * \brief   pixaSortByIndex()
 *
 * \param[in]    pixas
 * \param[in]    naindex    na that maps from the new pixa to the input pixa
 * \param[in]    copyflag   L_COPY, L_CLONE
 * \return  pixad sorted, or NULL on error
 */
PIXA *
pixaSortByIndex(PIXA    *pixas,
                NUMA    *naindex,
                l_int32  copyflag)
{
l_int32  i, n, index;
BOX     *box;
PIX     *pix;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (!naindex)
        return (PIXA *)ERROR_PTR("naindex not defined", __func__, NULL);
    if (copyflag != L_CLONE && copyflag != L_COPY)
        return (PIXA *)ERROR_PTR("invalid copyflag", __func__, NULL);

    n = pixaGetCount(pixas);
    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        numaGetIValue(naindex, i, &index);
        pix = pixaGetPix(pixas, index, copyflag);
        box = pixaGetBox(pixas, index, copyflag);
        pixaAddPix(pixad, pix, L_INSERT);
        pixaAddBox(pixad, box, L_INSERT);
    }

    return pixad;
}


/*!
 * \brief   pixaSort2dByIndex()
 *
 * \param[in]    pixas
 * \param[in]    naa       numaa that maps from the new pixaa to the input pixas
 * \param[in]    copyflag  L_CLONE or L_COPY
 * \return  paa sorted, or NULL on error
 */
PIXAA *
pixaSort2dByIndex(PIXA    *pixas,
                  NUMAA   *naa,
                  l_int32  copyflag)
{
l_int32  pixtot, ntot, i, j, n, nn, index;
BOX     *box;
NUMA    *na;
PIX     *pix;
PIXA    *pixa;
PIXAA   *paa;

    if (!pixas)
        return (PIXAA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (!naa)
        return (PIXAA *)ERROR_PTR("naindex not defined", __func__, NULL);

        /* Check counts */
    ntot = numaaGetNumberCount(naa);
    pixtot = pixaGetCount(pixas);
    if (ntot != pixtot)
        return (PIXAA *)ERROR_PTR("element count mismatch", __func__, NULL);

    n = numaaGetCount(naa);
    paa = pixaaCreate(n);
    for (i = 0; i < n; i++) {
        na = numaaGetNuma(naa, i, L_CLONE);
        nn = numaGetCount(na);
        pixa = pixaCreate(nn);
        for (j = 0; j < nn; j++) {
            numaGetIValue(na, j, &index);
            pix = pixaGetPix(pixas, index, copyflag);
            box = pixaGetBox(pixas, index, copyflag);
            pixaAddPix(pixa, pix, L_INSERT);
            pixaAddBox(pixa, box, L_INSERT);
        }
        pixaaAddPixa(paa, pixa, L_INSERT);
        numaDestroy(&na);
    }

    return paa;
}


/*---------------------------------------------------------------------*
 *                    Pixa and Pixaa range selection                   *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaSelectRange()
 *
 * \param[in]    pixas
 * \param[in]    first     use 0 to select from the beginning
 * \param[in]    last      use -1 to select to the end
 * \param[in]    copyflag  L_COPY, L_CLONE
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The copyflag specifies what we do with each pix from pixas.
 *          Specifically, L_CLONE inserts a clone into pixad of each
 *          selected pix from pixas.
 * </pre>
 */
PIXA *
pixaSelectRange(PIXA    *pixas,
                l_int32  first,
                l_int32  last,
                l_int32  copyflag)
{
l_int32  n, npix, i;
PIX     *pix;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (copyflag != L_COPY && copyflag != L_CLONE)
        return (PIXA *)ERROR_PTR("invalid copyflag", __func__, NULL);
    n = pixaGetCount(pixas);
    first = L_MAX(0, first);
    if (last < 0) last = n - 1;
    if (first >= n)
        return (PIXA *)ERROR_PTR("invalid first", __func__, NULL);
    if (last >= n) {
        L_WARNING("last = %d is beyond max index = %d; adjusting\n",
                  __func__, last, n - 1);
        last = n - 1;
    }
    if (first > last)
        return (PIXA *)ERROR_PTR("first > last", __func__, NULL);

    npix = last - first + 1;
    pixad = pixaCreate(npix);
    for (i = first; i <= last; i++) {
        pix = pixaGetPix(pixas, i, copyflag);
        pixaAddPix(pixad, pix, L_INSERT);
    }
    return pixad;
}


/*!
 * \brief   pixaaSelectRange()
 *
 * \param[in]    paas
 * \param[in]    first    use 0 to select from the beginning
 * \param[in]    last     use -1 to select to the end
 * \param[in]    copyflag L_COPY, L_CLONE
 * \return  paad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The copyflag specifies what we do with each pixa from paas.
 *          Specifically, L_CLONE inserts a clone into paad of each
 *          selected pixa from paas.
 * </pre>
 */
PIXAA *
pixaaSelectRange(PIXAA   *paas,
                 l_int32  first,
                 l_int32  last,
                 l_int32  copyflag)
{
l_int32  n, npixa, i;
PIXA    *pixa;
PIXAA   *paad;

    if (!paas)
        return (PIXAA *)ERROR_PTR("paas not defined", __func__, NULL);
    if (copyflag != L_COPY && copyflag != L_CLONE)
        return (PIXAA *)ERROR_PTR("invalid copyflag", __func__, NULL);
    n = pixaaGetCount(paas, NULL);
    first = L_MAX(0, first);
    if (last < 0) last = n - 1;
    if (first >= n)
        return (PIXAA *)ERROR_PTR("invalid first", __func__, NULL);
    if (last >= n) {
        L_WARNING("last = %d is beyond max index = %d; adjusting\n",
                  __func__, last, n - 1);
        last = n - 1;
    }
    if (first > last)
        return (PIXAA *)ERROR_PTR("first > last", __func__, NULL);

    npixa = last - first + 1;
    paad = pixaaCreate(npixa);
    for (i = first; i <= last; i++) {
        pixa = pixaaGetPixa(paas, i, copyflag);
        pixaaAddPixa(paad, pixa, L_INSERT);
    }
    return paad;
}


/*---------------------------------------------------------------------*
 *                        Pixa and Pixaa scaling                       *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaaScaleToSize()
 *
 * \param[in]    paas
 * \param[in]    wd    target width; use 0 if using height as target
 * \param[in]    hd    target height; use 0 if using width as target
 * \return  paad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This guarantees that each output scaled image has the
 *          dimension(s) you specify.
 *           ~ To specify the width with isotropic scaling, set %hd = 0.
 *           ~ To specify the height with isotropic scaling, set %wd = 0.
 *           ~ If both %wd and %hd are specified, the image is scaled
 *             (in general, anisotropically) to that size.
 *           ~ It is an error to set both %wd and %hd to 0.
 * </pre>
 */
PIXAA *
pixaaScaleToSize(PIXAA   *paas,
                 l_int32  wd,
                 l_int32  hd)
{
l_int32  n, i;
PIXA    *pixa1, *pixa2;
PIXAA   *paad;

    if (!paas)
        return (PIXAA *)ERROR_PTR("paas not defined", __func__, NULL);
    if (wd <= 0 && hd <= 0)
        return (PIXAA *)ERROR_PTR("neither wd nor hd > 0", __func__, NULL);

    n = pixaaGetCount(paas, NULL);
    paad = pixaaCreate(n);
    for (i = 0; i < n; i++) {
        pixa1 = pixaaGetPixa(paas, i, L_CLONE);
        pixa2 = pixaScaleToSize(pixa1, wd, hd);
        pixaaAddPixa(paad, pixa2, L_INSERT);
        pixaDestroy(&pixa1);
    }
    return paad;
}


/*!
 * \brief   pixaaScaleToSizeVar()
 *
 * \param[in]    paas
 * \param[in]    nawd  [optional] target widths; use NULL if using height
 * \param[in]    nahd  [optional] target height; use NULL if using width
 * \return  paad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This guarantees that the scaled images in each pixa have the
 *          dimension(s) you specify in the numas.
 *           ~ To specify the width with isotropic scaling, set %nahd = NULL.
 *           ~ To specify the height with isotropic scaling, set %nawd = NULL.
 *           ~ If both %nawd and %nahd are specified, the image is scaled
 *             (in general, anisotropically) to that size.
 *           ~ It is an error to set both %nawd and %nahd to NULL.
 *      (2) If either nawd and/or nahd is defined, it must have the same
 *          count as the number of pixa in paas.
 * </pre>
 */
PIXAA *
pixaaScaleToSizeVar(PIXAA  *paas,
                    NUMA   *nawd,
                    NUMA   *nahd)
{
l_int32  n, i, wd, hd;
PIXA    *pixa1, *pixa2;
PIXAA   *paad;

    if (!paas)
        return (PIXAA *)ERROR_PTR("paas not defined", __func__, NULL);
    if (!nawd && !nahd)
        return (PIXAA *)ERROR_PTR("!nawd && !nahd", __func__, NULL);

    n = pixaaGetCount(paas, NULL);
    if (nawd && (n != numaGetCount(nawd)))
        return (PIXAA *)ERROR_PTR("nawd wrong size", __func__, NULL);
    if (nahd && (n != numaGetCount(nahd)))
        return (PIXAA *)ERROR_PTR("nahd wrong size", __func__, NULL);
    paad = pixaaCreate(n);
    for (i = 0; i < n; i++) {
        wd = hd = 0;
        if (nawd) numaGetIValue(nawd, i, &wd);
        if (nahd) numaGetIValue(nahd, i, &hd);
        pixa1 = pixaaGetPixa(paas, i, L_CLONE);
        pixa2 = pixaScaleToSize(pixa1, wd, hd);
        pixaaAddPixa(paad, pixa2, L_INSERT);
        pixaDestroy(&pixa1);
    }
    return paad;
}


/*!
 * \brief   pixaScaleToSize()
 *
 * \param[in]    pixas
 * \param[in]    wd    target width; use 0 if using height as target
 * \param[in]    hd    target height; use 0 if using width as target
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See pixaaScaleToSize()
 * </pre>
 */
PIXA *
pixaScaleToSize(PIXA    *pixas,
                l_int32  wd,
                l_int32  hd)
{
l_int32  n, i;
PIX     *pix1, *pix2;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);

    if (wd <= 0 && hd <= 0)  /* no scaling requested */
        return pixaCopy(pixas, L_CLONE);

    n = pixaGetCount(pixas);
    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        pix2 = pixScaleToSize(pix1, wd, hd);
        pixCopyText(pix2, pix1);
        pixaAddPix(pixad, pix2, L_INSERT);
        pixDestroy(&pix1);
    }
    return pixad;
}


/*!
 * \brief   pixaScaleToSizeRel()
 *
 * \param[in]    pixas
 * \param[in]    delw   change in width, in pixels; 0 means no change
 * \param[in]    delh   change in height, in pixels; 0 means no change
 * return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If a requested change in a pix is not possible because
 *          either the requested width or height is <= 0, issue a
 *          warning and return a copy.
 * </pre>
 */
PIXA *
pixaScaleToSizeRel(PIXA    *pixas,
                   l_int32  delw,
                   l_int32  delh)
{
l_int32  n, i;
PIX     *pix1, *pix2;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);

    n = pixaGetCount(pixas);
    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        pix2 = pixScaleToSizeRel(pix1, delw, delh);
        if (pix2) {
            pixaAddPix(pixad, pix2, L_INSERT);
        } else {
            L_WARNING("relative scale to size failed; use a copy\n", __func__);
            pixaAddPix(pixad, pix1, L_COPY);
        }
        pixDestroy(&pix1);
    }
    return pixad;
}


/*!
 * \brief   pixaScale()
 *
 * \param[in]    pixas
 * \param[in]    scalex
 * \param[in]    scaley
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If pixas has a full boxes, it is scaled as well.
 * </pre>
 */
PIXA *
pixaScale(PIXA      *pixas,
          l_float32  scalex,
          l_float32  scaley)
{
l_int32  i, n, nb;
BOXA    *boxa1, *boxa2;
PIX     *pix1, *pix2;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (scalex <= 0.0 || scaley <= 0.0)
        return (PIXA *)ERROR_PTR("invalid scaling parameters", __func__, NULL);

    n = pixaGetCount(pixas);
    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        pix2 = pixScale(pix1, scalex, scaley);
        pixCopyText(pix2, pix1);
        pixaAddPix(pixad, pix2, L_INSERT);
        pixDestroy(&pix1);
    }

    boxa1 = pixaGetBoxa(pixas, L_CLONE);
    nb = boxaGetCount(boxa1);
    if (nb == n) {
        boxa2 = boxaTransform(boxa1, 0, 0, scalex, scaley);
        pixaSetBoxa(pixad, boxa2, L_INSERT);
    }
    boxaDestroy(&boxa1);
    return pixad;
}


/*!
 * \brief   pixaScaleBySampling()
 *
 * \param[in]    pixas
 * \param[in]    scalex
 * \param[in]    scaley
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If pixas has a full boxes, it is scaled as well.
 * </pre>
 */
PIXA *
pixaScaleBySampling(PIXA      *pixas,
                    l_float32  scalex,
                    l_float32  scaley)
{
l_int32  i, n, nb;
BOXA    *boxa1, *boxa2;
PIX     *pix1, *pix2;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (scalex <= 0.0 || scaley <= 0.0)
        return (PIXA *)ERROR_PTR("invalid scaling parameters", __func__, NULL);

    n = pixaGetCount(pixas);
    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        pix2 = pixScaleBySampling(pix1, scalex, scaley);
        pixCopyText(pix2, pix1);
        pixaAddPix(pixad, pix2, L_INSERT);
        pixDestroy(&pix1);
    }

    boxa1 = pixaGetBoxa(pixas, L_CLONE);
    nb = boxaGetCount(boxa1);
    if (nb == n) {
        boxa2 = boxaTransform(boxa1, 0, 0, scalex, scaley);
        pixaSetBoxa(pixad, boxa2, L_INSERT);
    }
    boxaDestroy(&boxa1);
    return pixad;
}


/*---------------------------------------------------------------------*
 *                     Pixa rotation and translation                   *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaRotate()
 *
 * \param[in]    pixas    1, 2, 4, 8, 32 bpp rgb
 * \param[in]    angle    rotation angle in radians; clockwise is positive
 * \param[in]    type     L_ROTATE_AREA_MAP, L_ROTATE_SHEAR, L_ROTATE_SAMPLING
 * \param[in]    incolor  L_BRING_IN_WHITE, L_BRING_IN_BLACK
 * \param[in]    width    original width; use 0 to avoid embedding
 * \param[in]    height   original height; use 0 to avoid embedding
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Each pix is rotated about its center.  See pixRotate() for details.
 *      (2) The boxa array is copied.  Why is it not rotated?
 *          If a boxa exists, the array of boxes is in 1-to-1
 *          correspondence with the array of pix, and each box typically
 *          represents the location of the pix relative to an image from
 *          which it has been extracted.  Like the pix, we could rotate
 *          each box around its center, and then generate a box that
 *          contains all four corners, as is done in boxaRotate(), but
 *          this seems unnecessary.
 * </pre>
 */
PIXA *
pixaRotate(PIXA      *pixas,
           l_float32  angle,
           l_int32    type,
           l_int32    incolor,
           l_int32    width,
           l_int32    height)
{
l_int32  i, n;
BOXA    *boxa;
PIX     *pixs, *pixd;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (type != L_ROTATE_SHEAR && type != L_ROTATE_AREA_MAP &&
        type != L_ROTATE_SAMPLING)
        return (PIXA *)ERROR_PTR("invalid type", __func__, NULL);
    if (incolor != L_BRING_IN_WHITE && incolor != L_BRING_IN_BLACK)
        return (PIXA *)ERROR_PTR("invalid incolor", __func__, NULL);
    if (L_ABS(angle) < MinAngleToRotate)
        return pixaCopy(pixas, L_COPY);

    n = pixaGetCount(pixas);
    if ((pixad = pixaCreate(n)) == NULL)
        return (PIXA *)ERROR_PTR("pixad not made", __func__, NULL);
    boxa = pixaGetBoxa(pixad, L_COPY);
    pixaSetBoxa(pixad, boxa, L_INSERT);
    for (i = 0; i < n; i++) {
        if ((pixs = pixaGetPix(pixas, i, L_CLONE)) == NULL) {
            pixaDestroy(&pixad);
            return (PIXA *)ERROR_PTR("pixs not found", __func__, NULL);
        }
        pixd = pixRotate(pixs, angle, type, incolor, width, height);
        pixaAddPix(pixad, pixd, L_INSERT);
        pixDestroy(&pixs);
    }

    return pixad;
}


/*!
 * \brief   pixaRotateOrth()
 *
 * \param[in]    pixas
 * \param[in]    rotation    0 = noop, 1 = 90 deg, 2 = 180 deg, 3 = 270 deg;
 *                           all rotations are clockwise
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Rotates each pix in the pixa.  Rotates and saves the boxes in
 *          the boxa if the boxa is full.
 * </pre>
 */
PIXA *
pixaRotateOrth(PIXA    *pixas,
               l_int32  rotation)
{
l_int32  i, n, nb, w, h;
BOX     *boxs, *boxd;
PIX     *pixs, *pixd;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (rotation < 0 || rotation > 3)
        return (PIXA *)ERROR_PTR("rotation not in {0,1,2,3}", __func__, NULL);
    if (rotation == 0)
        return pixaCopy(pixas, L_COPY);

    n = pixaGetCount(pixas);
    nb = pixaGetBoxaCount(pixas);
    if ((pixad = pixaCreate(n)) == NULL)
        return (PIXA *)ERROR_PTR("pixad not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        if ((pixs = pixaGetPix(pixas, i, L_CLONE)) == NULL) {
            pixaDestroy(&pixad);
            return (PIXA *)ERROR_PTR("pixs not found", __func__, NULL);
        }
        pixd = pixRotateOrth(pixs, rotation);
        pixaAddPix(pixad, pixd, L_INSERT);
        if (n == nb) {
            boxs = pixaGetBox(pixas, i, L_COPY);
            pixGetDimensions(pixs, &w, &h, NULL);
            boxd = boxRotateOrth(boxs, w, h, rotation);
            pixaAddBox(pixad, boxd, L_INSERT);
            boxDestroy(&boxs);
        }
        pixDestroy(&pixs);
    }

    return pixad;
}


/*!
 * \brief   pixaTranslate()
 *
 * \param[in]    pixas
 * \param[in]    hshift   horizontal shift; hshift > 0 is to right
 * \param[in]    vshift   vertical shift; vshift > 0 is down
 * \param[in]    incolor  L_BRING_IN_WHITE, L_BRING_IN_BLACK
 * \return  pixad, or NULL on error.
 */
PIXA *
pixaTranslate(PIXA    *pixas,
              l_int32  hshift,
              l_int32  vshift,
              l_int32  incolor)
{
l_int32  i, n, nb;
BOXA    *boxas, *boxad;
PIX     *pixs, *pixd;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (hshift == 0 && vshift == 0)
        return pixaCopy(pixas, L_COPY);

    n = pixaGetCount(pixas);
    nb = pixaGetBoxaCount(pixas);
    if ((pixad = pixaCreate(n)) == NULL)
        return (PIXA *)ERROR_PTR("pixad not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        if ((pixs = pixaGetPix(pixas, i, L_CLONE)) == NULL) {
            pixaDestroy(&pixad);
            return (PIXA *)ERROR_PTR("pixs not found", __func__, NULL);
        }
        pixd = pixTranslate(NULL, pixs, hshift, vshift, incolor);
        pixaAddPix(pixad, pixd, L_INSERT);
        pixDestroy(&pixs);
    }
    if (n == nb) {
        boxas = pixaGetBoxa(pixas, L_CLONE);
        boxad = boxaTransform(boxas, hshift, vshift, 1.0, 1.0);
        pixaSetBoxa(pixad, boxad, L_INSERT);
        boxaDestroy(&boxas);
    }

    return pixad;
}


/*---------------------------------------------------------------------*
 *                        Miscellaneous functions                      *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaAddBorderGeneral()
 *
 * \param[in]    pixad    can be null or equal to pixas
 * \param[in]    pixas    containing pix of all depths; colormap ok
 * \param[in]    left, right, top, bot     number of pixels added
 * \param[in]    val      value of added border pixels
 * \return  pixad with border added to each pix, including on error
 *
 * <pre>
 * Notes:
 *      (1) For binary images:
 *             white:  val = 0
 *             black:  val = 1
 *          For grayscale images:
 *             white:  val = 2 ** d - 1
 *             black:  val = 0
 *          For rgb color images:
 *             white:  val = 0xffffff00
 *             black:  val = 0
 *          For colormapped images, use 'index' found this way:
 *             white: pixcmapGetRankIntensity(cmap, 1.0, &index);
 *             black: pixcmapGetRankIntensity(cmap, 0.0, &index);
 *      (2) For in-place replacement of each pix with a bordered version,
 *          use %pixad = %pixas.  To make a new pixa, use %pixad = NULL.
 *      (3) In both cases, the boxa has sides adjusted as if it were
 *          expanded by the border.
 * </pre>
 */
PIXA *
pixaAddBorderGeneral(PIXA     *pixad,
                     PIXA     *pixas,
                     l_int32   left,
                     l_int32   right,
                     l_int32   top,
                     l_int32   bot,
                     l_uint32  val)
{
l_int32  i, n, nbox;
BOX     *box;
BOXA    *boxad;
PIX     *pixs, *pixd;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, pixad);
    if (left < 0 || right < 0 || top < 0 || bot < 0)
        return (PIXA *)ERROR_PTR("negative border added!", __func__, pixad);
    if (pixad && (pixad != pixas))
        return (PIXA *)ERROR_PTR("pixad defined but != pixas", __func__, pixad);

    n = pixaGetCount(pixas);
    if (!pixad)
        pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pixs = pixaGetPix(pixas, i, L_CLONE);
        pixd = pixAddBorderGeneral(pixs, left, right, top, bot, val);
        if (pixad == pixas)  /* replace */
            pixaReplacePix(pixad, i, pixd, NULL);
        else
            pixaAddPix(pixad, pixd, L_INSERT);
        pixDestroy(&pixs);
    }

    nbox = pixaGetBoxaCount(pixas);
    boxad = pixaGetBoxa(pixad, L_CLONE);
    for (i = 0; i < nbox; i++) {
        if ((box = pixaGetBox(pixas, i, L_COPY)) == NULL) {
            L_WARNING("box %d not found\n", __func__, i);
            break;
        }
        boxAdjustSides(box, box, -left, right, -top, bot);
        if (pixad == pixas)  /* replace */
            boxaReplaceBox(boxad, i, box);
        else
            boxaAddBox(boxad, box, L_INSERT);
    }
    boxaDestroy(&boxad);

    return pixad;
}


/*!
 * \brief   pixaaFlattenToPixa()
 *
 * \param[in]    paa
 * \param[out]   pnaindex  [optional] the pixa index in the pixaa
 * \param[in]    copyflag  L_COPY or L_CLONE
 * \return  pixa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This 'flattens' the pixaa to a pixa, taking the pix in
 *          order in the first pixa, then the second, etc.
 *      (2) If &naindex is defined, we generate a Numa that gives, for
 *          each pix in the pixaa, the index of the pixa to which it belongs.
 * </pre>
 */
PIXA *
pixaaFlattenToPixa(PIXAA   *paa,
                   NUMA   **pnaindex,
                   l_int32  copyflag)
{
l_int32  i, j, m, mb, n;
BOX     *box;
NUMA    *naindex = NULL;
PIX     *pix;
PIXA    *pixa, *pixat;

    if (pnaindex) *pnaindex = NULL;
    if (!paa)
        return (PIXA *)ERROR_PTR("paa not defined", __func__, NULL);
    if (copyflag != L_COPY && copyflag != L_CLONE)
        return (PIXA *)ERROR_PTR("invalid copyflag", __func__, NULL);

    if (pnaindex) {
        naindex = numaCreate(0);
        *pnaindex = naindex;
    }

    n = pixaaGetCount(paa, NULL);
    pixa = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pixat = pixaaGetPixa(paa, i, L_CLONE);
        m = pixaGetCount(pixat);
        mb = pixaGetBoxaCount(pixat);
        for (j = 0; j < m; j++) {
            pix = pixaGetPix(pixat, j, copyflag);
            pixaAddPix(pixa, pix, L_INSERT);
            if (j < mb) {
                box = pixaGetBox(pixat, j, copyflag);
                pixaAddBox(pixa, box, L_INSERT);
            }
            if (pnaindex)
                numaAddNumber(naindex, i);  /* save 'row' number */
        }
        pixaDestroy(&pixat);
    }

    return pixa;
}


/*!
 * \brief   pixaaSizeRange()
 *
 * \param[in]    paa
 * \param[out]   pminw, pminh, pmaxw, pmaxh   [optional] range of
 *                                            dimensions of all boxes
 * \return  0 if OK, 1 on error
 */
l_ok
pixaaSizeRange(PIXAA    *paa,
               l_int32  *pminw,
               l_int32  *pminh,
               l_int32  *pmaxw,
               l_int32  *pmaxh)
{
l_int32  minw, minh, maxw, maxh, minpw, minph, maxpw, maxph, i, n;
PIXA    *pixa;

    if (pminw) *pminw = 0;
    if (pminh) *pminh = 0;
    if (pmaxw) *pmaxw = 0;
    if (pmaxh) *pmaxh = 0;
    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);
    if (!pminw && !pmaxw && !pminh && !pmaxh)
        return ERROR_INT("no data can be returned", __func__, 1);

    minw = minh = 100000000;
    maxw = maxh = 0;
    n = pixaaGetCount(paa, NULL);
    for (i = 0; i < n; i++) {
        pixa = pixaaGetPixa(paa, i, L_CLONE);
        pixaSizeRange(pixa, &minpw, &minph, &maxpw, &maxph);
        if (minpw < minw)
            minw = minpw;
        if (minph < minh)
            minh = minph;
        if (maxpw > maxw)
            maxw = maxpw;
        if (maxph > maxh)
            maxh = maxph;
        pixaDestroy(&pixa);
    }

    if (pminw) *pminw = minw;
    if (pminh) *pminh = minh;
    if (pmaxw) *pmaxw = maxw;
    if (pmaxh) *pmaxh = maxh;
    return 0;
}


/*!
 * \brief   pixaSizeRange()
 *
 * \param[in]    pixa
 * \param[out]   pminw, pminh, pmaxw, pmaxh   [optional] range of
 *                                            dimensions of pix in the array
 * \return  0 if OK, 1 on error
 */
l_ok
pixaSizeRange(PIXA     *pixa,
              l_int32  *pminw,
              l_int32  *pminh,
              l_int32  *pmaxw,
              l_int32  *pmaxh)
{
l_int32  minw, minh, maxw, maxh, i, n, w, h;
PIX     *pix;

    if (pminw) *pminw = 0;
    if (pminh) *pminh = 0;
    if (pmaxw) *pmaxw = 0;
    if (pmaxh) *pmaxh = 0;
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (!pminw && !pmaxw && !pminh && !pmaxh)
        return ERROR_INT("no data can be returned", __func__, 1);

    minw = minh = 1000000;
    maxw = maxh = 0;
    n = pixaGetCount(pixa);
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixa, i, L_CLONE);
        w = pixGetWidth(pix);
        h = pixGetHeight(pix);
        if (w < minw)
            minw = w;
        if (h < minh)
            minh = h;
        if (w > maxw)
            maxw = w;
        if (h > maxh)
            maxh = h;
        pixDestroy(&pix);
    }

    if (pminw) *pminw = minw;
    if (pminh) *pminh = minh;
    if (pmaxw) *pmaxw = maxw;
    if (pmaxh) *pmaxh = maxh;

    return 0;
}


/*!
 * \brief   pixaClipToPix()
 *
 * \param[in]    pixas
 * \param[in]    pixs
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is intended for use in situations where pixas
 *          was originally generated from the input pixs.
 *      (2) Returns a pixad where each pix in pixas is ANDed
 *          with its associated region of the input pixs.  This
 *          region is specified by the the box that is associated
 *          with the pix.
 *      (3) In a typical application of this function, pixas has
 *          a set of region masks, so this generates a pixa of
 *          the parts of pixs that correspond to each region
 *          mask component, along with the bounding box for
 *          the region.
 * </pre>
 */
PIXA *
pixaClipToPix(PIXA  *pixas,
              PIX   *pixs)
{
l_int32  i, n;
BOX     *box;
PIX     *pix, *pixc;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (!pixs)
        return (PIXA *)ERROR_PTR("pixs not defined", __func__, NULL);

    n = pixaGetCount(pixas);
    if ((pixad = pixaCreate(n)) == NULL)
        return (PIXA *)ERROR_PTR("pixad not made", __func__, NULL);

    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixas, i, L_CLONE);
        box = pixaGetBox(pixas, i, L_COPY);
        pixc = pixClipRectangle(pixs, box, NULL);
        pixAnd(pixc, pixc, pix);
        pixaAddPix(pixad, pixc, L_INSERT);
        pixaAddBox(pixad, box, L_INSERT);
        pixDestroy(&pix);
    }

    return pixad;
}


/*!
 * \brief   pixaClipToForeground()
 *
 * \param[in]    pixas
 * \param[out]   ppixad   [optional] pixa of clipped pix returned
 * \param[out]   pboxa    [optional] clipping boxes returned
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) At least one of [&pixd, &boxa] must be specified.
 *      (2) Any pix with no fg pixels is skipped.
 *      (3) See pixClipToForeground().
 * </pre>
 */
l_ok
pixaClipToForeground(PIXA   *pixas,
                     PIXA  **ppixad,
                     BOXA  **pboxa)
{
l_int32  i, n;
BOX     *box1;
PIX     *pix1, *pix2;

    if (ppixad) *ppixad = NULL;
    if (pboxa) *pboxa = NULL;
    if (!pixas)
        return ERROR_INT("pixas not defined", __func__, 1);
    if (!ppixad && !pboxa)
        return ERROR_INT("no output requested", __func__, 1);

    n = pixaGetCount(pixas);
    if (ppixad) *ppixad = pixaCreate(n);
    if (pboxa) *pboxa = boxaCreate(n);
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        pixClipToForeground(pix1, &pix2, &box1);
        pixDestroy(&pix1);
        if (ppixad)
            pixaAddPix(*ppixad, pix2, L_INSERT);
        else
            pixDestroy(&pix2);
        if (pboxa)
            boxaAddBox(*pboxa, box1, L_INSERT);
        else
            boxDestroy(&box1);
    }

    return 0;
}


/*!
 * \brief   pixaGetRenderingDepth()
 *
 * \param[in]    pixa
 * \param[out]   pdepth   depth required to render if all colormaps are removed
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Any pix with 16 bpp will be considered as having 8 bpp.
 *          If all pix have bpp = 1, this returns 1.
 *          If any pix has color (either rgb or a colormap with color),
 *          this return 32.
 *          Otherwise, this returns the maximum of 8 and the max depth
 *          of all the pix.
 *      (2) This can be used to allow lossless rendering onto a single pix.
 * </pre>
 */
l_ok
pixaGetRenderingDepth(PIXA     *pixa,
                      l_int32  *pdepth)
{
l_int32  hascolor, maxdepth;

    if (!pdepth)
        return ERROR_INT("&depth not defined", __func__, 1);
    *pdepth = 0;
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    pixaHasColor(pixa, &hascolor);
    if (hascolor) {
        *pdepth = 32;
        return 0;
    }

    pixaGetDepthInfo(pixa, &maxdepth, NULL);
    if (maxdepth == 1)
        *pdepth = 1;
    else  /* 2, 4, 8 or 16 */
        *pdepth = 8;
    return 0;
}


/*!
 * \brief   pixaHasColor()
 *
 * \param[in]    pixa
 * \param[out]   phascolor   1 if any pix is rgb or has a colormap with color;
 *                           0 otherwise
 * \return  0 if OK; 1 on error
 */
l_ok
pixaHasColor(PIXA     *pixa,
             l_int32  *phascolor)
{
l_int32   i, n, hascolor, d;
PIX      *pix;
PIXCMAP  *cmap;

    if (!phascolor)
        return ERROR_INT("&hascolor not defined", __func__, 1);
    *phascolor = 0;
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    n = pixaGetCount(pixa);
    hascolor = 0;
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixa, i, L_CLONE);
        if ((cmap = pixGetColormap(pix)) != NULL)
            pixcmapHasColor(cmap, &hascolor);
        d = pixGetDepth(pix);
        pixDestroy(&pix);
        if (d == 32 || hascolor == 1) {
            *phascolor = 1;
            break;
        }
    }

    return 0;
}


/*!
 * \brief   pixaAnyColormaps()
 *
 * \param[in]    pixa
 * \param[out]   phascmap    1 if any pix has a colormap; 0 otherwise
 * \return  0 if OK; 1 on error
 */
l_ok
pixaAnyColormaps(PIXA     *pixa,
                 l_int32  *phascmap)
{
l_int32   i, n;
PIX      *pix;
PIXCMAP  *cmap;

    if (!phascmap)
        return ERROR_INT("&hascmap not defined", __func__, 1);
    *phascmap = 0;
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    n = pixaGetCount(pixa);
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixa, i, L_CLONE);
        cmap = pixGetColormap(pix);
        pixDestroy(&pix);
        if (cmap) {
            *phascmap = 1;
            return 0;
        }
    }

    return 0;
}


/*!
 * \brief   pixaGetDepthInfo()
 *
 * \param[in]    pixa
 * \param[out]   pmaxdepth  [optional] max pixel depth of pix in pixa
 * \param[out]   psame      [optional] true if all depths are equal
 * \return  0 if OK; 1 on error
 */
l_ok
pixaGetDepthInfo(PIXA     *pixa,
                 l_int32  *pmaxdepth,
                 l_int32  *psame)
{
l_int32  i, n, d, d0;
l_int32  maxd, same;  /* depth info */

    if (pmaxdepth) *pmaxdepth = 0;
    if (psame) *psame = TRUE;
    if (!pmaxdepth && !psame) return 0;
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if ((n = pixaGetCount(pixa)) == 0)
        return ERROR_INT("pixa is empty", __func__, 1);

    same = TRUE;
    maxd = 0;
    for (i = 0; i < n; i++) {
        pixaGetPixDimensions(pixa, i, NULL, NULL, &d);
        if (i == 0)
            d0 = d;
        else if (d != d0)
            same = FALSE;
        if (d > maxd) maxd = d;
    }

    if (pmaxdepth) *pmaxdepth = maxd;
    if (psame) *psame = same;
    return 0;
}


/*!
 * \brief   pixaConvertToSameDepth()
 *
 * \param[in]    pixas
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Any pix with 16 bpp will be converted to 8 bpp.
 *          If all pix have bpp = 1, the output depth will be 1.
 *          If any pix has color (either rgb or a colormap with color),
 *          the output depth will be 32.
 *          Otherwise, the output depth is the maximum of 8 and the
 *          the max depth of all the pix.
 *      (2) This can be used to allow lossless rendering onto
 *          a single pix. (Except: 16 bpp gets converted to 8.)
 * </pre>
 */
PIXA *
pixaConvertToSameDepth(PIXA  *pixas)
{
l_int32  i, n, depth, same, hascmap, maxdepth;
BOXA    *boxa;
PIX     *pix1, *pix2;
PIXA    *pixa1, *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if ((n = pixaGetCount(pixas)) == 0)
        return (PIXA *)ERROR_PTR("no components", __func__, NULL);


        /* Remove colormaps if necessary */
    pixaGetRenderingDepth(pixas, &depth);
    pixaAnyColormaps(pixas, &hascmap);
    if (hascmap) {
        pixa1 = pixaCreate(n);
        for (i = 0; i < n; i++) {
            pix1 = pixaGetPix(pixas, i, L_CLONE);
            if (depth == 32)
                pix2 = pixConvertTo32(pix1);
            else  /* depth = 8 */
                pix2 = pixConvertTo8(pix1, 0);
            pixaAddPix(pixa1, pix2, L_INSERT);
            pixDestroy(&pix1);
        }
    } else {
        pixa1 = pixaCopy(pixas, L_CLONE);
    }

    pixaGetDepthInfo(pixa1, &maxdepth, &same);
    if (!same) {  /* at least one pix has depth < maxdepth */
        pixad = pixaCreate(n);
        for (i = 0; i < n; i++) {
            pix1 = pixaGetPix(pixa1, i, L_CLONE);
            if (maxdepth <= 16)
                pix2 = pixConvertTo8(pix1, 0);
            else
                pix2 = pixConvertTo32(pix1);
            pixaAddPix(pixad, pix2, L_INSERT);
            pixDestroy(&pix1);
        }
    } else {
        pixad = pixaCopy(pixa1, L_CLONE);
    }

    boxa = pixaGetBoxa(pixas, L_COPY);
    pixaSetBoxa(pixad, boxa, L_INSERT);
    pixaDestroy(&pixa1);
    return pixad;
}


/*!
 * \brief   pixaConvertToGivenDepth()
 *
 * \param[in]    pixas
 * \param[in]    depth    specify either 8 or 32 bpp
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Use this to remove any colormaps and convert all pix to either
 *          8 or 32 bpp.
 *      (2) To convert losslessly, get %depth from pixaGetRenderingDepth().
 *      (3) Clone pix may be in the returned pixa if conversion is to 32 bpp.
 * </pre>
 */
PIXA *
pixaConvertToGivenDepth(PIXA    *pixas,
                        l_int32  depth)
{
l_int32  i, n, maxd;
BOXA    *boxa;
PIX     *pix1, *pix2;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if ((n = pixaGetCount(pixas)) == 0)
        return (PIXA *)ERROR_PTR("no components", __func__, NULL);
    if (depth != 8 && depth != 32)
        return (PIXA *)ERROR_PTR("depth not 8 or 32", __func__, NULL);

        /* Warn with 1 --> {8,32} or lossy conversions */
    pixaGetRenderingDepth(pixas, &maxd);
    if (maxd == 1)
        L_WARNING("All pix are 1 bpp; converting to %d bpp\n", __func__, depth);
    if (maxd > depth)
        L_WARNING("Lossy conversion: max rendering depth %d > input %d\n",
                  __func__, maxd, depth);

    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        if (depth == 32) {
            pix2 = (pixGetDepth(pix1) == 32) ? pixClone(pix1) :
                   pixConvertTo32(pix1);
        } else {  /* depth = 8 */
            pix2 = pixConvertTo8(pix1, 0);
        }
        pixaAddPix(pixad, pix2, L_INSERT);
        pixDestroy(&pix1);
    }

    boxa = pixaGetBoxa(pixas, L_COPY);
    pixaSetBoxa(pixad, boxa, L_INSERT);
    return pixad;
}


/*!
 * \brief   pixaEqual()
 *
 * \param[in]    pixa1
 * \param[in]    pixa2
 * \param[in]    maxdist
 * \param[out]   pnaindex  [optional] index array of correspondences
 * \param[out]   psame     1 if equal; 0 otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The two pixa are the "same" if they contain the same
 *          boxa and the same ordered set of pix.  However, if they
 *          have boxa, the pix in each pixa can differ in ordering
 *          by an amount given by the parameter %maxdist.  If they
 *          don't have a boxa, the %maxdist parameter is ignored,
 *          and the ordering of the pix must be identical.
 *      (2) This applies only to boxa geometry, pixels and ordering;
 *          other fields in the pix are ignored.
 *      (3) naindex[i] gives the position of the box in pixa2 that
 *          corresponds to box i in pixa1.  It is only returned if the
 *          pixa have boxa and the boxa are equal.
 *      (4) In situations where the ordering is very different, so that
 *          a large %maxdist is required for "equality", this should be
 *          implemented with a hash function for efficiency.
 * </pre>
 */
l_ok
pixaEqual(PIXA     *pixa1,
          PIXA     *pixa2,
          l_int32   maxdist,
          NUMA    **pnaindex,
          l_int32  *psame)
{
l_int32   i, j, n, empty1, empty2, same, sameboxa;
BOXA     *boxa1, *boxa2;
NUMA     *na;
PIX      *pix1, *pix2;

    if (pnaindex) *pnaindex = NULL;
    if (!psame)
        return ERROR_INT("&same not defined", __func__, 1);
    *psame = 0;
    sameboxa = 0;
    na = NULL;
    if (!pixa1 || !pixa2)
        return ERROR_INT("pixa1 and pixa2 not both defined", __func__, 1);
    n = pixaGetCount(pixa1);
    if (n != pixaGetCount(pixa2))
        return 0;

        /* If there are no boxes, strict ordering of the pix in each
         * pixa  is required. */
    boxa1 = pixaGetBoxa(pixa1, L_CLONE);
    boxa2 = pixaGetBoxa(pixa2, L_CLONE);
    empty1 = (boxaGetCount(boxa1) == 0) ? 1 : 0;
    empty2 = (boxaGetCount(boxa2) == 0) ? 1 : 0;
    if (!empty1 && !empty2) {
        boxaEqual(boxa1, boxa2, maxdist, &na, &sameboxa);
        if (!sameboxa) {
            boxaDestroy(&boxa1);
            boxaDestroy(&boxa2);
            numaDestroy(&na);
            return 0;
        }
    }
    boxaDestroy(&boxa1);
    boxaDestroy(&boxa2);
    if ((!empty1 && empty2) || (empty1 && !empty2))
        return 0;

    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixa1, i, L_CLONE);
        if (na)
            numaGetIValue(na, i, &j);
        else
            j = i;
        pix2 = pixaGetPix(pixa2, j, L_CLONE);
        pixEqual(pix1, pix2, &same);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
        if (!same) {
            numaDestroy(&na);
            return 0;
        }
    }

    *psame = 1;
    if (pnaindex)
        *pnaindex = na;
    else
        numaDestroy(&na);
    return 0;
}


/*!
 * \brief   pixaSetFullSizeBoxa()
 *
 * \param[in]    pixa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Replaces the existing boxa.  Each box gives the dimensions
 *          of the corresponding pix.  This is needed for functions
 *          like pixaSort() that sort based on the boxes.
 * </pre>
 */
l_ok
pixaSetFullSizeBoxa(PIXA  *pixa)
{
l_int32  i, n, w, h;
BOX     *box;
BOXA    *boxa;
PIX     *pix;

    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if ((n = pixaGetCount(pixa)) == 0) {
        L_INFO("pixa contains no pix\n", __func__);
        return 0;
    }

    boxa = boxaCreate(n);
    pixaSetBoxa(pixa, boxa, L_INSERT);
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixa, i, L_CLONE);
        pixGetDimensions(pix, &w, &h, NULL);
        box = boxCreate(0, 0, w, h);
        boxaAddBox(boxa, box, L_INSERT);
        pixDestroy(&pix);
    }
    return 0;
}


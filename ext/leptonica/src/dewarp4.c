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
 * \file dewarp4.c
 * <pre>
 *
 *    Single page dewarper
 *
 *    Reference model (book-level, dewarpa) operations and debugging output
 *
 *      Top-level single page dewarper
 *          l_int32            dewarpSinglePage()
 *          l_int32            dewarpSinglePageInit()
 *          l_int32            dewarpSinglePageRun()
 *
 *      Operations on dewarpa
 *          l_int32            dewarpaListPages()
 *          l_int32            dewarpaSetValidModels()
 *          l_int32            dewarpaInsertRefModels()
 *          l_int32            dewarpaStripRefModels()
 *          l_int32            dewarpaRestoreModels()
 *
 *      Dewarp debugging output
 *          l_int32            dewarpaInfo()
 *          l_int32            dewarpaModelStats()
 *          static l_int32     dewarpaTestForValidModel()
 *          l_int32            dewarpaShowArrays()
 *          l_int32            dewarpDebug()
 *          l_int32            dewarpShowResults()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"

static l_int32 dewarpaTestForValidModel(L_DEWARPA *dewa, L_DEWARP *dew,
                                        l_int32 notests);

#ifndef  NO_CONSOLE_IO
#define  DEBUG_INVALID_MODELS      0   /* set this to 1 for debugging */
#endif  /* !NO_CONSOLE_IO */

    /* Special parameter value */
static const l_int32  GrayInValue = 200;

/*----------------------------------------------------------------------*
 *                   Top-level single page dewarper                     *
 *----------------------------------------------------------------------*/
/*!
 * \brief   dewarpSinglePage()
 *
 * \param[in]    pixs            with text, any depth
 * \param[in]    thresh          for global thresh to 1 bpp; ignore otherwise
 * \param[in]    adaptive        1 for adaptive thresh; 0 for global threshold
 * \param[in]    useboth         1 for both horiz and vert; 0 for vertical only
 * \param[in]    check_columns   1 to skip horizontal if multiple columns;
 *                               0 otherwise; default is to skip
 * \param[out]   ppixd           dewarped result
 * \param[out]   pdewa           [optional] dewa with single page; NULL to skip
 * \param[in]    debug           1 for debugging output, 0 otherwise
 * \return  0 if OK, 1 on error list of page numbers, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Dewarps pixs and returns the result in &pixd.
 *      (2) This uses default values for all model parameters.
 *      (3) If pixs is 1 bpp, the parameters %adaptive and %thresh are ignored.
 *      (4) If it can't build a model, returns a copy of pixs in &pixd.
 * </pre>
 */
l_ok
dewarpSinglePage(PIX         *pixs,
                 l_int32      thresh,
                 l_int32      adaptive,
                 l_int32      useboth,
                 l_int32      check_columns,
                 PIX        **ppixd,
                 L_DEWARPA  **pdewa,
                 l_int32      debug)
{
L_DEWARPA  *dewa;
PIX        *pixb;

    if (!ppixd)
        return ERROR_INT("&pixd not defined", __func__, 1);
    *ppixd = NULL;
    if (pdewa) *pdewa = NULL;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

    dewarpSinglePageInit(pixs, thresh, adaptive, useboth,
                         check_columns, &pixb, &dewa);
    if (!pixb) {
        dewarpaDestroy(&dewa);
        return ERROR_INT("pixb not made", __func__, 1);
    }

    dewarpSinglePageRun(pixs, pixb, dewa, ppixd, debug);

    if (pdewa)
        *pdewa = dewa;
    else
        dewarpaDestroy(&dewa);
    pixDestroy(&pixb);
    return 0;
}


/*!
 * \brief   dewarpSinglePageInit()
 *
 * \param[in]    pixs            with text, any depth
 * \param[in]    thresh          for global thresh to 1 bpp; ignore otherwise
 * \param[in]    adaptive        1 for adaptive thresh; 0 for global threshold
 * \param[in]    useboth         1 for both horiz and vert; 0 for vertical only
 * \param[in]    check_columns   1 to skip horizontal if multiple columns;
 *                               0 otherwise; default is to skip
 * \param[out]   ppixb           1 bpp debug image
 * \param[out]   pdewa           initialized dewa
 * \return  0 if OK, 1 on error list of page numbers, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This binarizes the input pixs if necessary, returning the
 *          binarized image.  It also initializes the dewa to default values
 *          for the model parameters.
 *      (2) If pixs is 1 bpp, the parameters %adaptive and %thresh are ignored.
 *      (3) To change the model parameters, call dewarpaSetCurvatures()
 *          before running dewarpSinglePageRun().  For example:
 *             dewarpSinglePageInit(pixs, 0, 1, 1, 1, &pixb, &dewa);
 *             dewarpaSetCurvatures(dewa, 250, -1, -1, 80, 70, 150);
 *             dewarpSinglePageRun(pixs, pixb, dewa, &pixd, 0);
 *             dewarpaDestroy(&dewa);
 *             pixDestroy(&pixb);
 * </pre>
 */
l_ok
dewarpSinglePageInit(PIX         *pixs,
                     l_int32      thresh,
                     l_int32      adaptive,
                     l_int32      useboth,
                     l_int32      check_columns,
                     PIX        **ppixb,
                     L_DEWARPA  **pdewa)
{
PIX  *pix1, *pix2;

    if (ppixb) *ppixb = NULL;
    if (pdewa) *pdewa = NULL;
    if (!ppixb || !pdewa)
        return ERROR_INT("&pixb and &dewa not both defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

         /* Generate a binary image, if necessary */
    if (pixGetDepth(pixs) > 1) {
        if ((pix1 = pixConvertTo8(pixs, 0)) == NULL)
            return ERROR_INT("pix1 not made", __func__, 1);
        if (adaptive)
            pix2 = pixAdaptThresholdToBinary(pix1, NULL, 1.0);
        else
            pix2 = pixThresholdToBinary(pix1, thresh);
        pixDestroy(&pix1);
        if (!pix2)
            return ERROR_INT("pix2 not made", __func__, 1);
        *ppixb = pix2;
    } else {
        *ppixb = pixClone(pixs);
    }

    *pdewa = dewarpaCreate(1, 0, 1, 0, -1);
    dewarpaUseBothArrays(*pdewa, useboth);
    dewarpaSetCheckColumns(*pdewa, check_columns);
    return 0;
}


/*!
 * \brief   dewarpSinglePageRun()
 *
 * \param[in]    pixs    any depth
 * \param[in]    pixb    1 bpp
 * \param[in]    dewa    initialized
 * \param[out]   ppixd   dewarped result
 * \param[in]    debug   1 for debugging output, 0 otherwise
 * \return  0 if OK, 1 on error list of page numbers, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Dewarps pixs and returns the result in &pixd.
 *      (2) The 1 bpp version %pixb and %dewa are conveniently generated by
 *          dewarpSinglePageInit().
 *      (3) Non-default model parameters must be set before calling this.
 *      (4) If a model cannot be built, this returns a copy of pixs in &pixd.
 * </pre>
 */
l_ok
dewarpSinglePageRun(PIX        *pixs,
                    PIX        *pixb,
                    L_DEWARPA  *dewa,
                    PIX       **ppixd,
                    l_int32     debug)
{
const char  *debugfile;
l_int32      vsuccess, ret;
L_DEWARP    *dew;

    if (!ppixd)
        return ERROR_INT("&pixd not defined", __func__, 1);
    *ppixd = NULL;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!pixb)
        return ERROR_INT("pixb not defined", __func__, 1);
    if (!dewa)
        return ERROR_INT("dewa not defined", __func__, 1);

    if (debug)
        lept_mkdir("lept/dewarp");

        /* Generate the page model */
    dew = dewarpCreate(pixb, 0);
    dewarpaInsertDewarp(dewa, dew);
    debugfile = (debug) ? "/tmp/lept/dewarp/singlepage_model.pdf" : NULL;
    dewarpBuildPageModel(dew, debugfile);
    dewarpaModelStatus(dewa, 0, &vsuccess, NULL);
    if (vsuccess == 0) {
        L_ERROR("failure to build model for vertical disparity\n", __func__);
        *ppixd = pixCopy(NULL, pixs);
        return 0;
    }

        /* Apply the page model */
    debugfile = (debug) ? "/tmp/lept/dewarp/singlepage_apply.pdf" : NULL;
    ret = dewarpaApplyDisparity(dewa, 0, pixs, 255, 0, 0, ppixd, debugfile);
    if (ret)
        L_ERROR("invalid model; failure to apply disparity\n", __func__);
    return 0;
}


/*----------------------------------------------------------------------*
 *                        Operations on dewarpa                         *
 *----------------------------------------------------------------------*/
/*!
 * \brief   dewarpaListPages()
 *
 * \param[in]    dewa    populated with dewarp structs for pages
 * \return  0 if OK, 1 on error list of page numbers, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This generates two numas, stored in the dewarpa, that give:
 *          (a) the page number for each dew that has a page model.
 *          (b) the page number for each dew that has either a page
 *              model or a reference model.
 *          It can be called at any time.
 *      (2) It is called by the dewarpa serializer before writing.
 * </pre>
 */
l_ok
dewarpaListPages(L_DEWARPA  *dewa)
{
l_int32    i;
L_DEWARP  *dew;
NUMA      *namodels, *napages;

    if (!dewa)
        return ERROR_INT("dewa not defined", __func__, 1);

    numaDestroy(&dewa->namodels);
    numaDestroy(&dewa->napages);
    namodels = numaCreate(dewa->maxpage + 1);
    napages = numaCreate(dewa->maxpage + 1);
    dewa->namodels = namodels;
    dewa->napages = napages;
    for (i = 0; i <= dewa->maxpage; i++) {
        if ((dew = dewarpaGetDewarp(dewa, i)) != NULL) {
            if (dew->hasref == 0)
                numaAddNumber(namodels, dew->pageno);
            numaAddNumber(napages, dew->pageno);
        }
    }
    return 0;
}


/*!
 * \brief   dewarpaSetValidModels()
 *
 * \param[in]    dewa
 * \param[in]    notests
 * \param[in]    debug     1 to output information on invalid page models
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) A valid model must meet the rendering requirements, which
 *          include whether or not a vertical disparity model exists
 *          and conditions on curvatures for vertical and horizontal
 *          disparity models.
 *      (2) If %notests == 1, this ignores the curvature constraints
 *          and assumes that all successfully built models are valid.
 *      (3) This function does not need to be called by the application.
 *          It is called by dewarpaInsertRefModels(), which
 *          will destroy all invalid dewarps.  Consequently, to inspect
 *          an invalid dewarp model, it must be done before calling
 *          dewarpaInsertRefModels().
 * </pre>
 */
l_ok
dewarpaSetValidModels(L_DEWARPA  *dewa,
                      l_int32     notests,
                      l_int32     debug)
{
l_int32    i, n, maxcurv, diffcurv, diffedge;
L_DEWARP  *dew;

    if (!dewa)
        return ERROR_INT("dewa not defined", __func__, 1);

    n = dewa->maxpage + 1;
    for (i = 0; i < n; i++) {
        if ((dew = dewarpaGetDewarp(dewa, i)) == NULL)
            continue;

        if (debug) {
            if (dew->hasref == 1) {
                L_INFO("page %d: has only a ref model\n", __func__, i);
            } else if (dew->vsuccess == 0) {
                L_INFO("page %d: no model successfully built\n",
                       __func__, i);
            } else if (!notests) {
                maxcurv = L_MAX(L_ABS(dew->mincurv), L_ABS(dew->maxcurv));
                diffcurv = dew->maxcurv - dew->mincurv;
                if (dewa->useboth && !dew->hsuccess)
                    L_INFO("page %d: useboth, but no horiz disparity\n",
                               __func__, i);
                if (maxcurv > dewa->max_linecurv)
                    L_INFO("page %d: max curvature %d > max_linecurv\n",
                                __func__, i, diffcurv);
                if (diffcurv < dewa->min_diff_linecurv)
                    L_INFO("page %d: diff curv %d < min_diff_linecurv\n",
                                __func__, i, diffcurv);
                if (diffcurv > dewa->max_diff_linecurv)
                    L_INFO("page %d: abs diff curv %d > max_diff_linecurv\n",
                                __func__, i, diffcurv);
                if (dew->hsuccess) {
                    if (L_ABS(dew->leftslope) > dewa->max_edgeslope)
                        L_INFO("page %d: abs left slope %d > max_edgeslope\n",
                                    __func__, i, dew->leftslope);
                    if (L_ABS(dew->rightslope) > dewa->max_edgeslope)
                        L_INFO("page %d: abs right slope %d > max_edgeslope\n",
                                    __func__, i, dew->rightslope);
                    diffedge = L_ABS(dew->leftcurv - dew->rightcurv);
                    if (L_ABS(dew->leftcurv) > dewa->max_edgecurv)
                        L_INFO("page %d: left curvature %d > max_edgecurv\n",
                                    __func__, i, dew->leftcurv);
                    if (L_ABS(dew->rightcurv) > dewa->max_edgecurv)
                        L_INFO("page %d: right curvature %d > max_edgecurv\n",
                               __func__, i, dew->rightcurv);
                    if (diffedge > dewa->max_diff_edgecurv)
                        L_INFO("page %d: abs diff left-right curv %d > "
                               "max_diff_edgecurv\n", __func__, i, diffedge);
                }
            }
        }

        dewarpaTestForValidModel(dewa, dew, notests);
    }

    return 0;
}


/*!
 * \brief   dewarpaInsertRefModels()
 *
 * \param[in]    dewa
 * \param[in]    notests    if 1, ignore curvature constraints on model
 * \param[in]    debug      1 to output information on invalid page models
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This destroys all dewarp models that are invalid, and then
 *          inserts reference models where possible.
 *      (2) If %notests == 1, this ignores the curvature constraints
 *          and assumes that all successfully built models are valid.
 *      (3) If useboth == 0, it uses the closest valid model within the
 *          distance and parity constraints.  If useboth == 1, it tries
 *          to use the closest allowed hvalid model; if it doesn't find
 *          an hvalid model, it uses the closest valid model.
 *      (4) For all pages without a model, this clears out any existing
 *          invalid and reference dewarps, finds the nearest valid model
 *          with the same parity, and inserts an empty dewarp with the
 *          reference page.
 *      (5) Then if it is requested to use both vertical and horizontal
 *          disparity arrays (useboth == 1), it tries to replace any
 *          hvalid == 0 model or reference with an hvalid == 1 reference.
 *      (6) The distance constraint is that any reference model must
 *          be within maxdist.  Note that with the parity constraint,
 *          no reference models will be used if maxdist < 2.
 *      (7) This function must be called, even if reference models will
 *          not be used.  It should be called after building models on all
 *          available pages, and after setting the rendering parameters.
 *      (8) If the dewa has been serialized, this function is called by
 *          dewarpaRead() when it is read back.  It is also called
 *          any time the rendering parameters are changed.
 *      (9) Note: if this has been called with useboth == 1, and useboth
 *          is reset to 0, you should first call dewarpaRestoreModels()
 *          to bring real models from the cache back to the primary array.
 * </pre>
 */
l_ok
dewarpaInsertRefModels(L_DEWARPA  *dewa,
                       l_int32     notests,
                       l_int32     debug)
{
l_int32    i, j, n, val, min, distdown, distup;
L_DEWARP  *dew;
NUMA      *na, *nah;

    if (!dewa)
        return ERROR_INT("dewa not defined", __func__, 1);
    if (dewa->maxdist < 2)
        L_INFO("maxdist < 2; no ref models can be used\n", __func__);

        /* Make an indicator numa for pages with valid models. */
    dewarpaSetValidModels(dewa, notests, debug);
    n = dewa->maxpage + 1;
    na = numaMakeConstant(0, n);
    for (i = 0; i < n; i++) {
        dew = dewarpaGetDewarp(dewa, i);
        if (dew && dew->vvalid)
            numaReplaceNumber(na, i, 1);
    }

        /* Remove all existing ref models and restore models from cache */
    dewarpaRestoreModels(dewa);

        /* Move invalid models to the cache, and insert reference dewarps
         * for pages that need to borrow a model.
         * First, try to find a valid model for each page. */
    for (i = 0; i < n; i++) {
        numaGetIValue(na, i, &val);
        if (val == 1) continue;  /* already has a valid model */
        if ((dew = dewa->dewarp[i]) != NULL) {  /* exists but is not valid; */
            dewa->dewarpcache[i] = dew;  /* move it to the cache */
            dewa->dewarp[i] = NULL;
        }
        if (dewa->maxdist < 2) continue;  /* can't use a ref model */
            /* Look back for nearest model */
        distdown = distup = dewa->maxdist + 1;
        for (j = i - 2; j >= 0 && distdown > dewa->maxdist; j -= 2) {
            numaGetIValue(na, j, &val);
            if (val == 1) distdown = i - j;
        }
            /* Look ahead for nearest model */
        for (j = i + 2; j < n && distup > dewa->maxdist; j += 2) {
            numaGetIValue(na, j, &val);
            if (val == 1) distup = j - i;
        }
        min = L_MIN(distdown, distup);
        if (min > dewa->maxdist) continue;  /* no valid model in range */
        if (distdown <= distup)
            dewarpaInsertDewarp(dewa, dewarpCreateRef(i, i - distdown));
        else
            dewarpaInsertDewarp(dewa, dewarpCreateRef(i, i + distup));
    }
    numaDestroy(&na);

        /* If a valid model will do, we're finished. */
    if (dewa->useboth == 0) {
        dewa->modelsready = 1;  /* validated */
        return 0;
    }

        /* The request is useboth == 1.  Now try to find an hvalid model */
    nah = numaMakeConstant(0, n);
    for (i = 0; i < n; i++) {
        dew = dewarpaGetDewarp(dewa, i);
        if (dew && dew->hvalid)
            numaReplaceNumber(nah, i, 1);
    }
    for (i = 0; i < n; i++) {
        numaGetIValue(nah, i, &val);
        if (val == 1) continue;  /* already has a hvalid model */
        if (dewa->maxdist < 2) continue;  /* can't use a ref model */
        distdown = distup = 100000;
        for (j = i - 2; j >= 0; j -= 2) {  /* look back for nearest model */
            numaGetIValue(nah, j, &val);
            if (val == 1) {
                distdown = i - j;
                break;
            }
        }
        for (j = i + 2; j < n; j += 2) {  /* look ahead for nearest model */
            numaGetIValue(nah, j, &val);
            if (val == 1) {
                distup = j - i;
                break;
            }
        }
        min = L_MIN(distdown, distup);
        if (min > dewa->maxdist) continue;  /* no hvalid model within range */

            /* We can replace the existing valid model with an hvalid model.
             * If it's not a reference, save it in the cache. */
        if ((dew = dewarpaGetDewarp(dewa, i)) == NULL) {
            L_ERROR("dew is null for page %d!\n", __func__, i);
        } else {
            if (dew->hasref == 0) {  /* not a ref model */
                dewa->dewarpcache[i] = dew;  /* move it to the cache */
                dewa->dewarp[i] = NULL;  /* must null the ptr */
            }
        }
        if (distdown <= distup)  /* insert the hvalid ref model */
            dewarpaInsertDewarp(dewa, dewarpCreateRef(i, i - distdown));
        else
            dewarpaInsertDewarp(dewa, dewarpCreateRef(i, i + distup));
    }
    numaDestroy(&nah);

    dewa->modelsready = 1;  /* validated */
    return 0;
}


/*!
 * \brief   dewarpaStripRefModels()
 *
 * \param[in]    dewa    populated with dewarp structs for pages
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This examines each dew in a dewarpa, and removes
 *          all that don't have their own page model (i.e., all
 *          that have "references" to nearby pages with valid models).
 *          These references were generated by dewarpaInsertRefModels(dewa).
 * </pre>
 */
l_ok
dewarpaStripRefModels(L_DEWARPA  *dewa)
{
l_int32    i;
L_DEWARP  *dew;

    if (!dewa)
        return ERROR_INT("dewa not defined", __func__, 1);

    for (i = 0; i <= dewa->maxpage; i++) {
        if ((dew = dewarpaGetDewarp(dewa, i)) != NULL) {
            if (dew->hasref)
                dewarpDestroy(&dewa->dewarp[i]);
        }
    }
    dewa->modelsready = 0;

        /* Regenerate the page lists */
    dewarpaListPages(dewa);
    return 0;
}


/*!
 * \brief   dewarpaRestoreModels()
 *
 * \param[in]    dewa     populated with dewarp structs for pages
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This puts all real models (and only real models) in the
 *          primary dewarpa array.  First remove all dewarps that are
 *          only references to other page models.  Then move all models
 *          that had been cached back into the primary dewarp array.
 *      (2) After this is done, we still need to recompute and insert
 *          the reference models before dewa->modelsready is true.
 * </pre>
 */
l_ok
dewarpaRestoreModels(L_DEWARPA  *dewa)
{
l_int32    i;
L_DEWARP  *dew;

    if (!dewa)
        return ERROR_INT("dewa not defined", __func__, 1);

        /* Strip out ref models.  Then only real models will be in the
         * primary dewarp array. */
    dewarpaStripRefModels(dewa);

        /* The cache holds only real models, which are not necessarily valid. */
    for (i = 0; i <= dewa->maxpage; i++) {
        if ((dew = dewa->dewarpcache[i]) != NULL) {
            if (dewa->dewarp[i]) {
                L_ERROR("dew in both cache and main array!: page %d\n",
                        __func__, i);
            } else {
                dewa->dewarp[i] = dew;
                dewa->dewarpcache[i] = NULL;
            }
        }
    }
    dewa->modelsready = 0;  /* new ref models not yet inserted */

        /* Regenerate the page lists */
    dewarpaListPages(dewa);
    return 0;
}


/*----------------------------------------------------------------------*
 *                      Dewarp debugging output                         *
 *----------------------------------------------------------------------*/
/*!
 * \brief   dewarpaInfo()
 *
 * \param[in]    fp
 * \param[in]    dewa
 * \return  0 if OK, 1 on error
 */
l_ok
dewarpaInfo(FILE       *fp,
            L_DEWARPA  *dewa)
{
l_int32    i, n, pageno, nnone, nvsuccess, nvvalid, nhsuccess, nhvalid, nref;
L_DEWARP  *dew;

    if (!fp)
        return ERROR_INT("dewa not defined", __func__, 1);
    if (!dewa)
        return ERROR_INT("dewa not defined", __func__, 1);

    fprintf(fp, "\nDewarpaInfo: %p\n", dewa);
    fprintf(fp, "nalloc = %d, maxpage = %d\n", dewa->nalloc, dewa->maxpage);
    fprintf(fp, "sampling = %d, redfactor = %d, minlines = %d\n",
            dewa->sampling, dewa->redfactor, dewa->minlines);
    fprintf(fp, "maxdist = %d, useboth = %d\n",
            dewa->maxdist, dewa->useboth);

    dewarpaModelStats(dewa, &nnone, &nvsuccess, &nvvalid,
                      &nhsuccess, &nhvalid, &nref);
    n = numaGetCount(dewa->napages);
    lept_stderr("Total number of pages with a dew = %d\n", n);
    lept_stderr("Number of pages without any models = %d\n", nnone);
    lept_stderr("Number of pages with a vert model = %d\n", nvsuccess);
    lept_stderr("Number of pages with a valid vert model = %d\n", nvvalid);
    lept_stderr("Number of pages with both models = %d\n", nhsuccess);
    lept_stderr("Number of pages with both models valid = %d\n", nhvalid);
    lept_stderr("Number of pages with a ref model = %d\n", nref);

    for (i = 0; i < n; i++) {
        numaGetIValue(dewa->napages, i, &pageno);
        if ((dew = dewarpaGetDewarp(dewa, pageno)) == NULL)
            continue;
        lept_stderr("Page: %d\n", dew->pageno);
        lept_stderr("  hasref = %d, refpage = %d\n",
                    dew->hasref, dew->refpage);
        lept_stderr("  nlines = %d\n", dew->nlines);
        lept_stderr("  w = %d, h = %d, nx = %d, ny = %d\n",
                    dew->w, dew->h, dew->nx, dew->ny);
        if (dew->sampvdispar)
            lept_stderr("  Vertical disparity builds:\n"
                    "    (min,max,abs-diff) line curvature = (%d,%d,%d)\n",
                    dew->mincurv, dew->maxcurv, dew->maxcurv - dew->mincurv);
        if (dew->samphdispar)
            lept_stderr("  Horizontal disparity builds:\n"
                    "    left edge slope = %d, right edge slope = %d\n"
                    "    (left,right,abs-diff) edge curvature = (%d,%d,%d)\n",
                    dew->leftslope, dew->rightslope, dew->leftcurv,
                    dew->rightcurv, L_ABS(dew->leftcurv - dew->rightcurv));
    }
    return 0;
}


/*!
 * \brief   dewarpaModelStats()
 *
 * \param[in]    dewa
 * \param[out]   pnnone       [optional] number without any model
 * \param[out]   pnvsuccess   [optional] number with a vert model
 * \param[out]   pnvvalid     [optional] number with a valid vert model
 * \param[out]   pnhsuccess   [optional] number with both models
 * \param[out]   pnhvalid     [optional] number with both models valid
 * \param[out]   pnref        [optional] number with a reference model
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) A page without a model has no dew.  It most likely failed to
 *          generate a vertical model, and has not been assigned a ref
 *          model from a neighboring page with a valid vertical model.
 *      (2) A page has vsuccess == 1 if there is at least a model of the
 *          vertical disparity.  The model may be invalid, in which case
 *          dewarpaInsertRefModels() will stash it in the cache and
 *          attempt to replace it by a valid ref model.
 *      (3) A vvvalid model is a vertical disparity model whose parameters
 *          satisfy the constraints given in dewarpaSetValidModels().
 *      (4) A page has hsuccess == 1 if both the vertical and horizontal
 *          disparity arrays have been constructed.
 *      (5) An  hvalid model has vertical and horizontal disparity
 *          models whose parameters satisfy the constraints given
 *          in dewarpaSetValidModels().
 *      (6) A page has a ref model if it failed to generate a valid
 *          model but was assigned a vvalid or hvalid model on another
 *          page (within maxdist) by dewarpaInsertRefModel().
 *      (7) This calls dewarpaTestForValidModel(); it ignores the vvalid
 *          and hvalid fields.
 * </pre>
 */
l_ok
dewarpaModelStats(L_DEWARPA  *dewa,
                  l_int32    *pnnone,
                  l_int32    *pnvsuccess,
                  l_int32    *pnvvalid,
                  l_int32    *pnhsuccess,
                  l_int32    *pnhvalid,
                  l_int32    *pnref)
{
l_int32    i, n, pageno, nnone, nvsuccess, nvvalid, nhsuccess, nhvalid, nref;
L_DEWARP  *dew;

    if (!dewa)
        return ERROR_INT("dewa not defined", __func__, 1);

    dewarpaListPages(dewa);
    n = numaGetCount(dewa->napages);
    nnone = nref = nvsuccess = nvvalid = nhsuccess = nhvalid = 0;
    for (i = 0; i < n; i++) {
        numaGetIValue(dewa->napages, i, &pageno);
        dew = dewarpaGetDewarp(dewa, pageno);
        if (!dew) {
            nnone++;
            continue;
        }
        if (dew->hasref == 1)
            nref++;
        if (dew->vsuccess == 1)
            nvsuccess++;
        if (dew->hsuccess == 1)
            nhsuccess++;
        dewarpaTestForValidModel(dewa, dew, 0);
        if (dew->vvalid == 1)
            nvvalid++;
        if (dew->hvalid == 1)
            nhvalid++;
    }

    if (pnnone) *pnnone = nnone;
    if (pnref) *pnref = nref;
    if (pnvsuccess) *pnvsuccess = nvsuccess;
    if (pnvvalid) *pnvvalid = nvvalid;
    if (pnhsuccess) *pnhsuccess = nhsuccess;
    if (pnhvalid) *pnhvalid = nhvalid;
    return 0;
}


/*!
 * \brief   dewarpaTestForValidModel()
 *
 * \param[in]    dewa
 * \param[in]    dew
 * \param[in]    notests
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Computes validity of vertical (vvalid) model and both
 *          vertical and horizontal (hvalid) models.
 *      (2) If %notests == 1, this ignores the curvature constraints
 *          and assumes that all successfully built models are valid.
 *      (3) This is just about the models, not the rendering process,
 *          so the value of useboth is not considered here.
 * </pre>
 */
static l_int32
dewarpaTestForValidModel(L_DEWARPA  *dewa,
                         L_DEWARP   *dew,
                         l_int32     notests)
{
l_int32  maxcurv, diffcurv, diffedge;

    if (!dewa || !dew)
        return ERROR_INT("dewa and dew not both defined", __func__, 1);

    if (notests) {
       dew->vvalid = dew->vsuccess;
       dew->hvalid = dew->hsuccess;
       return 0;
    }

        /* No actual model was built */
    if (dew->vsuccess == 0) return 0;

        /* Was previously found not to have a valid model  */
    if (dew->hasref == 1) return 0;

        /* vsuccess == 1; a vertical (line) model exists.
         * First test that the vertical curvatures are within allowed
         * bounds.  Note that all curvatures are signed.*/
    maxcurv = L_MAX(L_ABS(dew->mincurv), L_ABS(dew->maxcurv));
    diffcurv = dew->maxcurv - dew->mincurv;
    if (maxcurv <= dewa->max_linecurv &&
        diffcurv >= dewa->min_diff_linecurv &&
        diffcurv <= dewa->max_diff_linecurv) {
        dew->vvalid = 1;
    } else {
        L_INFO("invalid vert model for page %d:\n", __func__, dew->pageno);
#if DEBUG_INVALID_MODELS
        lept_stderr("  max line curv = %d, max allowed = %d\n",
                    maxcurv, dewa->max_linecurv);
        lept_stderr("  diff line curv = %d, max allowed = %d\n",
                    diffcurv, dewa->max_diff_linecurv);
#endif  /* DEBUG_INVALID_MODELS */
    }

        /* If a horizontal (edge) model exists, test for validity. */
    if (dew->hsuccess) {
        diffedge = L_ABS(dew->leftcurv - dew->rightcurv);
        if (L_ABS(dew->leftslope) <= dewa->max_edgeslope &&
            L_ABS(dew->rightslope) <= dewa->max_edgeslope &&
            L_ABS(dew->leftcurv) <= dewa->max_edgecurv &&
            L_ABS(dew->rightcurv) <= dewa->max_edgecurv &&
            diffedge <= dewa->max_diff_edgecurv) {
            dew->hvalid = 1;
        } else {
            L_INFO("invalid horiz model for page %d:\n", __func__, dew->pageno);
#if DEBUG_INVALID_MODELS
            lept_stderr("  left edge slope = %d, max allowed = %d\n",
                        dew->leftslope, dewa->max_edgeslope);
            lept_stderr("  right edge slope = %d, max allowed = %d\n",
                        dew->rightslope, dewa->max_edgeslope);
            lept_stderr("  left edge curv = %d, max allowed = %d\n",
                        dew->leftcurv, dewa->max_edgecurv);
            lept_stderr("  right edge curv = %d, max allowed = %d\n",
                        dew->rightcurv, dewa->max_edgecurv);
            lept_stderr("  diff edge curv = %d, max allowed = %d\n",
                        diffedge, dewa->max_diff_edgecurv);
#endif  /* DEBUG_INVALID_MODELS */
        }
    }

    return 0;
}


/*!
 * \brief   dewarpaShowArrays()
 *
 * \param[in]    dewa
 * \param[in]    scalefact    on contour images; typ. 0.5
 * \param[in]    first        first page model to render
 * \param[in]    last         last page model to render; use 0 to go to end
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Generates a pdf of contour plots of the disparity arrays.
 *      (2) This only shows actual models; not ref models
 * </pre>
 */
l_ok
dewarpaShowArrays(L_DEWARPA   *dewa,
                  l_float32    scalefact,
                  l_int32      first,
                  l_int32      last)
{
char       buf[256];
l_int32    i, svd, shd;
L_BMF     *bmf;
L_DEWARP  *dew;
PIX       *pixv, *pixvs, *pixh, *pixhs = NULL, *pixt, *pixd;
PIXA      *pixa;

    if (!dewa)
        return ERROR_INT("dew not defined", __func__, 1);
    if (first < 0 || first > dewa->maxpage)
        return ERROR_INT("first out of bounds", __func__, 1);
    if (last <= 0 || last > dewa->maxpage) last = dewa->maxpage;
    if (last < first)
        return ERROR_INT("last < first", __func__, 1);

    lept_rmdir("lept/dewarp1");  /* temp directory for contour plots */
    lept_mkdir("lept/dewarp1");
    if ((bmf = bmfCreate(NULL, 8)) == NULL)
        L_ERROR("bmf not made; page info not displayed", __func__);

    lept_stderr("Generating contour plots\n");
    for (i = first; i <= last; i++) {
        if (i && ((i % 10) == 0))
            lept_stderr(" .. %d", i);
        dew = dewarpaGetDewarp(dewa, i);
        if (!dew) continue;
        if (dew->hasref == 1) continue;
        svd = shd = 0;
        if (dew->sampvdispar) svd = 1;
        if (dew->samphdispar) shd = 1;
        if (!svd) {
            L_ERROR("sampvdispar not made for page %d!\n", __func__, i);
            continue;
        }

            /* Generate contour plots at reduced resolution */
        dewarpPopulateFullRes(dew, NULL, 0, 0);
        pixv = fpixRenderContours(dew->fullvdispar, 3.0f, 0.15f);
        pixvs = pixScaleBySampling(pixv, scalefact, scalefact);
        pixDestroy(&pixv);
        if (shd) {
            pixh = fpixRenderContours(dew->fullhdispar, 3.0f, 0.15f);
            pixhs = pixScaleBySampling(pixh, scalefact, scalefact);
            pixDestroy(&pixh);
        }
        dewarpMinimize(dew);

            /* Save side-by-side */
        pixa = pixaCreate(2);
        pixaAddPix(pixa, pixvs, L_INSERT);
        if (shd)
            pixaAddPix(pixa, pixhs, L_INSERT);
        pixt = pixaDisplayTiledInRows(pixa, 32, 1500, 1.0, 0, 30, 2);
        snprintf(buf, sizeof(buf), "Page %d", i);
        pixd = pixAddSingleTextblock(pixt, bmf, buf, 0x0000ff00,
                                     L_ADD_BELOW, NULL);
        snprintf(buf, sizeof(buf), "/tmp/lept/dewarp1/arrays_%04d.png", i);
        pixWriteDebug(buf, pixd, IFF_PNG);
        pixaDestroy(&pixa);
        pixDestroy(&pixt);
        pixDestroy(&pixd);
    }
    bmfDestroy(&bmf);
    lept_stderr("\n");

    lept_stderr("Generating pdf of contour plots\n");
    convertFilesToPdf("/tmp/lept/dewarp1", "arrays_", 90, 1.0, L_FLATE_ENCODE,
                      0, "Disparity arrays", "/tmp/lept/disparity_arrays.pdf");
    lept_stderr("Output written to: /tmp/lept/disparity_arrays.pdf\n");
    return 0;
}


/*!
 * \brief   dewarpDebug()
 *
 * \param[in]    dew
 * \param[in]    subdirs   one or more subdirectories of /tmp; e.g., "dew1"
 * \param[in]    index     to help label output images; e.g., the page number
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Prints dewarp fields and generates disparity array contour images.
 *          The contour images are written to file:
 *                /tmp/[subdirs]/pixv_[index].png
 * </pre>
 */
l_ok
dewarpDebug(L_DEWARP    *dew,
            const char  *subdirs,
            l_int32      index)
{
char     fname[256];
char    *outdir;
l_int32  svd, shd;
PIX     *pixv, *pixh;

    if (!dew)
        return ERROR_INT("dew not defined", __func__, 1);
    if (!subdirs)
        return ERROR_INT("subdirs not defined", __func__, 1);

    lept_stderr("pageno = %d, hasref = %d, refpage = %d\n",
                dew->pageno, dew->hasref, dew->refpage);
    lept_stderr("sampling = %d, redfactor = %d, minlines = %d\n",
                dew->sampling, dew->redfactor, dew->minlines);
    svd = shd = 0;
    if (!dew->hasref) {
        if (dew->sampvdispar) svd = 1;
        if (dew->samphdispar) shd = 1;
        lept_stderr("sampv = %d, samph = %d\n", svd, shd);
        lept_stderr("w = %d, h = %d\n", dew->w, dew->h);
        lept_stderr("nx = %d, ny = %d\n", dew->nx, dew->ny);
        lept_stderr("nlines = %d\n", dew->nlines);
        if (svd) {
            lept_stderr("(min,max,abs-diff) line curvature = (%d,%d,%d)\n",
                       dew->mincurv, dew->maxcurv, dew->maxcurv - dew->mincurv);
        }
        if (shd) {
            lept_stderr("(left edge slope = %d, right edge slope = %d\n",
                        dew->leftslope, dew->rightslope);
            lept_stderr("(left,right,abs-diff) edge curvature = "
                        "(%d,%d,%d)\n", dew->leftcurv, dew->rightcurv,
                        L_ABS(dew->leftcurv - dew->rightcurv));
        }
    }
    if (!svd && !shd) {
        lept_stderr("No disparity arrays\n");
        return 0;
    }

    dewarpPopulateFullRes(dew, NULL, 0, 0);
    lept_mkdir(subdirs);
    outdir = pathJoin("/tmp", subdirs);
    if (svd) {
        pixv = fpixRenderContours(dew->fullvdispar, 3.0f, 0.15f);
        snprintf(fname, sizeof(fname), "%s/pixv_%d.png", outdir, index);
        pixWriteDebug(fname, pixv, IFF_PNG);
        pixDestroy(&pixv);
    }
    if (shd) {
        pixh = fpixRenderContours(dew->fullhdispar, 3.0f, 0.15f);
        snprintf(fname, sizeof(fname), "%s/pixh_%d.png", outdir, index);
        pixWriteDebug(fname, pixh, IFF_PNG);
        pixDestroy(&pixh);
    }
    LEPT_FREE(outdir);
    return 0;
}


/*!
 * \brief   dewarpShowResults()
 *
 * \param[in]    dewa
 * \param[in]    sa          of indexed input images
 * \param[in]    boxa        crop boxes for input images; can be null
 * \param[in]    firstpage
 * \param[in]    lastpage
 * \param[in]    pdfout      filename
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a pdf of image pairs (before, after) for
 *          the designated set of input pages.
 *      (2) If the boxa exists, its elements are aligned with numbers
 *          in the filenames in %sa.  It is used to crop the input images.
 *          It is assumed that the dewa was generated from the cropped
 *          images.  No undercropping is applied before rendering.
 * </pre>
 */
l_ok
dewarpShowResults(L_DEWARPA   *dewa,
                  SARRAY      *sa,
                  BOXA        *boxa,
                  l_int32      firstpage,
                  l_int32      lastpage,
                  const char  *pdfout)
{
char       bufstr[256];
l_int32    i, modelpage;
L_BMF     *bmf;
BOX       *box;
L_DEWARP  *dew;
PIX       *pixs, *pixc, *pixd, *pixt1, *pixt2;
PIXA      *pixa;

    if (!dewa)
        return ERROR_INT("dewa not defined", __func__, 1);
    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);
    if (!pdfout)
        return ERROR_INT("pdfout not defined", __func__, 1);
    if (firstpage > lastpage)
        return ERROR_INT("invalid first/last page numbers", __func__, 1);

    lept_rmdir("lept/dewarp_pdfout");
    lept_mkdir("lept/dewarp_pdfout");
    bmf = bmfCreate(NULL, 6);

    lept_stderr("Dewarping and generating s/by/s view\n");
    for (i = firstpage; i <= lastpage; i++) {
        if (i && (i % 10 == 0)) lept_stderr(".. %d ", i);
        pixs = pixReadIndexed(sa, i);
        if (boxa) {
            box = boxaGetBox(boxa, i, L_CLONE);
            pixc = pixClipRectangle(pixs, box, NULL);
            boxDestroy(&box);
        }
        else
            pixc = pixClone(pixs);
        dew = dewarpaGetDewarp(dewa, i);
        pixd = NULL;
        if (dew) {
            dewarpaApplyDisparity(dewa, dew->pageno, pixc,
                                  GrayInValue, 0, 0, &pixd, NULL);
            dewarpMinimize(dew);
        }
        pixa = pixaCreate(2);
        pixaAddPix(pixa, pixc, L_INSERT);
        if (pixd)
            pixaAddPix(pixa, pixd, L_INSERT);
        pixt1 = pixaDisplayTiledAndScaled(pixa, 32, 500, 2, 0, 35, 2);
        if (dew) {
            modelpage = (dew->hasref) ? dew->refpage : dew->pageno;
            snprintf(bufstr, sizeof(bufstr), "Page %d; using %d\n",
                     i, modelpage);
        }
        else
            snprintf(bufstr, sizeof(bufstr), "Page %d; no dewarp\n", i);
        pixt2 = pixAddSingleTextblock(pixt1, bmf, bufstr, 0x0000ff00,
                                      L_ADD_BELOW, 0);
        snprintf(bufstr, sizeof(bufstr), "/tmp/lept/dewarp_pdfout/%05d", i);
        pixWriteDebug(bufstr, pixt2, IFF_JFIF_JPEG);
        pixaDestroy(&pixa);
        pixDestroy(&pixs);
        pixDestroy(&pixt1);
        pixDestroy(&pixt2);
    }
    lept_stderr("\n");

    lept_stderr("Generating pdf of result\n");
    convertFilesToPdf("/tmp/lept/dewarp_pdfout", NULL, 100, 1.0, L_JPEG_ENCODE,
                      0, "Dewarp sequence", pdfout);
    lept_stderr("Output written to: %s\n", pdfout);
    bmfDestroy(&bmf);
    return 0;
}

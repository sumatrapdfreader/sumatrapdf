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
 * \file  boxfunc5.c
 * <pre>
 *
 *      Boxa sequence fitting
 *           BOXA     *boxaSmoothSequenceMedian()
 *           BOXA     *boxaWindowedMedian()
 *           BOXA     *boxaModifyWithBoxa()
 *           BOXA     *boxaReconcilePairWidth()
 *           l_int32   boxaSizeConsistency()
 *           BOXA     *boxaReconcileAllByMedian()
 *           BOXA     *boxaReconcileSidesByMedian()
 *    static void      adjustSidePlotName()  -- debug
 *           BOXA     *boxaReconcileSizeByMedian()
 *           l_int32   boxaPlotSides()   [for debugging]
 *           l_int32   boxaPlotSizes()   [for debugging]
 *           BOXA     *boxaFillSequence()
 *    static l_int32   boxaFillAll()
 *           l_int32   boxaSizeVariation()
 *           l_int32   boxaMedianDimensions()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"

static l_int32 boxaFillAll(BOXA *boxa);
static void adjustSidePlotName(char *buf, size_t size, const char *preface,
                               l_int32 select);

/*---------------------------------------------------------------------*
 *                        Boxa sequence fitting                        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   boxaSmoothSequenceMedian()
 *
 * \param[in]    boxas        source boxa
 * \param[in]    halfwin      half-width of sliding window; used to find median
 * \param[in]    subflag      L_USE_MINSIZE, L_USE_MAXSIZE,
 *                            L_SUB_ON_LOC_DIFF, L_SUB_ON_SIZE_DIFF,
 *                            L_USE_CAPPED_MIN, L_USE_CAPPED_MAX
 * \param[in]    maxdiff      parameter used with L_SUB_ON_LOC_DIFF,
 *                            L_SUB_ON_SIZE_DIFF, L_USE_CAPPED_MIN,
 *                            L_USE_CAPPED_MAX
 * \param[in]    extrapixels  pixels added on all sides (or subtracted
 *                            if %extrapixels < 0) when using
 *                            L_SUB_ON_LOC_DIFF and L_SUB_ON_SIZE_DIFF
 * \param[in]    debug        1 for debug output
 * \return  boxad fitted boxa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The target width of the sliding window is 2 * %halfwin + 1.
 *          If necessary, this will be reduced by boxaWindowedMedian().
 *      (2) This returns a modified version of %boxas by constructing
 *          for each input box a box that has been smoothed with windowed
 *          median filtering.  The filtering is done to each of the
 *          box sides independently, and it is computed separately for
 *          sequences of even and odd boxes.  The output %boxad is
 *          constructed from the input boxa and the filtered boxa,
 *          depending on %subflag.  See boxaModifyWithBoxa() for
 *          details on the use of %subflag, %maxdiff and %extrapixels.
 *      (3) This is useful for removing noise separately in the even
 *          and odd sets, where the box edge locations can have
 *          discontinuities but otherwise vary roughly linearly within
 *          intervals of size %halfwin or larger.
 *      (4) If you don't need to handle even and odd sets separately,
 *          just do this:
 *              boxam = boxaWindowedMedian(boxas, halfwin, debug);
 *              boxad = boxaModifyWithBoxa(boxas, boxam, subflag, maxdiff,
 *                                         extrapixels);
 *              boxaDestroy(&boxam);
 * </pre>
 */
BOXA *
boxaSmoothSequenceMedian(BOXA    *boxas,
                         l_int32  halfwin,
                         l_int32  subflag,
                         l_int32  maxdiff,
                         l_int32  extrapixels,
                         l_int32  debug)
{
l_int32  n;
BOXA    *boxae, *boxao, *boxamede, *boxamedo, *boxame, *boxamo, *boxad;
PIX     *pix1;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (halfwin <= 0) {
        L_WARNING("halfwin must be > 0; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if (maxdiff < 0) {
        L_WARNING("maxdiff must be >= 0; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if (subflag != L_USE_MINSIZE && subflag != L_USE_MAXSIZE &&
        subflag != L_SUB_ON_LOC_DIFF && subflag != L_SUB_ON_SIZE_DIFF &&
        subflag != L_USE_CAPPED_MIN && subflag != L_USE_CAPPED_MAX) {
        L_WARNING("invalid subflag; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if ((n = boxaGetCount(boxas)) < 6) {
        L_WARNING("need at least 6 boxes; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }

    boxaSplitEvenOdd(boxas, 0, &boxae, &boxao);
    if (debug) {
        lept_mkdir("lept/smooth");
        boxaWriteDebug("/tmp/lept/smooth/boxae.ba", boxae);
        boxaWriteDebug("/tmp/lept/smooth/boxao.ba", boxao);
    }

    boxamede = boxaWindowedMedian(boxae, halfwin, debug);
    boxamedo = boxaWindowedMedian(boxao, halfwin, debug);
    if (debug) {
        boxaWriteDebug("/tmp/lept/smooth/boxamede.ba", boxamede);
        boxaWriteDebug("/tmp/lept/smooth/boxamedo.ba", boxamedo);
    }

    boxame = boxaModifyWithBoxa(boxae, boxamede, subflag, maxdiff, extrapixels);
    boxamo = boxaModifyWithBoxa(boxao, boxamedo, subflag, maxdiff, extrapixels);
    if (debug) {
        boxaWriteDebug("/tmp/lept/smooth/boxame.ba", boxame);
        boxaWriteDebug("/tmp/lept/smooth/boxamo.ba", boxamo);
    }

    boxad = boxaMergeEvenOdd(boxame, boxamo, 0);
    if (debug) {
        boxaPlotSides(boxas, NULL, NULL, NULL, NULL, NULL, &pix1);
        pixWrite("/tmp/lept/smooth/plotsides1.png", pix1, IFF_PNG);
        pixDestroy(&pix1);
        boxaPlotSides(boxad, NULL, NULL, NULL, NULL, NULL, &pix1);
        pixWrite("/tmp/lept/smooth/plotsides2.png", pix1, IFF_PNG);
        pixDestroy(&pix1);
        boxaPlotSizes(boxas, NULL, NULL, NULL, &pix1);
        pixWrite("/tmp/lept/smooth/plotsizes1.png", pix1, IFF_PNG);
        pixDestroy(&pix1);
        boxaPlotSizes(boxad, NULL, NULL, NULL, &pix1);
        pixWrite("/tmp/lept/smooth/plotsizes2.png", pix1, IFF_PNG);
        pixDestroy(&pix1);
    }

    boxaDestroy(&boxae);
    boxaDestroy(&boxao);
    boxaDestroy(&boxamede);
    boxaDestroy(&boxamedo);
    boxaDestroy(&boxame);
    boxaDestroy(&boxamo);
    return boxad;
}


/*!
 * \brief   boxaWindowedMedian()
 *
 * \param[in]    boxas     source boxa
 * \param[in]    halfwin   half width of window over which the median is found
 * \param[in]    debug     1 for debug output
 * \return  boxad smoothed boxa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This finds a set of boxes (boxad) where each edge of each box is
 *          a windowed median smoothed value to the edges of the
 *          input set of boxes (boxas).
 *      (2) Invalid input boxes are filled from nearby ones.
 *      (3) The returned boxad can then be used in boxaModifyWithBoxa()
 *          to selectively change the boxes in the source boxa.
 * </pre>
 */
BOXA *
boxaWindowedMedian(BOXA    *boxas,
                   l_int32  halfwin,
                   l_int32  debug)
{
l_int32  n, i, left, top, right, bot;
BOX     *box;
BOXA    *boxaf, *boxad;
NUMA    *nal, *nat, *nar, *nab, *naml, *namt, *namr, *namb;
PIX     *pix1;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if ((n = boxaGetCount(boxas)) < 3) {
        L_WARNING("less than 3 boxes; returning a copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if (halfwin <= 0) {
        L_WARNING("halfwin must be > 0; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }

        /* Fill invalid boxes in the input sequence */
    if ((boxaf = boxaFillSequence(boxas, L_USE_ALL_BOXES, debug)) == NULL)
        return (BOXA *)ERROR_PTR("filled boxa not made", __func__, NULL);

        /* Get the windowed median output from each of the sides */
    boxaExtractAsNuma(boxaf, &nal, &nat, &nar, &nab, NULL, NULL, 0);
    naml = numaWindowedMedian(nal, halfwin);
    namt = numaWindowedMedian(nat, halfwin);
    namr = numaWindowedMedian(nar, halfwin);
    namb = numaWindowedMedian(nab, halfwin);

    n = boxaGetCount(boxaf);
    boxad = boxaCreate(n);
    for (i = 0; i < n; i++) {
        numaGetIValue(naml, i, &left);
        numaGetIValue(namt, i, &top);
        numaGetIValue(namr, i, &right);
        numaGetIValue(namb, i, &bot);
        box = boxCreate(left, top, right - left + 1, bot - top + 1);
        boxaAddBox(boxad, box, L_INSERT);
    }

    if (debug) {
        lept_mkdir("lept/windowed");
        boxaPlotSides(boxaf, NULL, NULL, NULL, NULL, NULL, &pix1);
        pixWrite("/tmp/lept/windowed/plotsides1.png", pix1, IFF_PNG);
        pixDestroy(&pix1);
        boxaPlotSides(boxad, NULL, NULL, NULL, NULL, NULL, &pix1);
        pixWrite("/tmp/lept/windowed/plotsides2.png", pix1, IFF_PNG);
        pixDestroy(&pix1);
        boxaPlotSizes(boxaf, NULL, NULL, NULL, &pix1);
        pixWrite("/tmp/lept/windowed/plotsizes1.png", pix1, IFF_PNG);
        pixDestroy(&pix1);
        boxaPlotSizes(boxad, NULL, NULL, NULL, &pix1);
        pixWrite("/tmp/lept/windowed/plotsizes2.png", pix1, IFF_PNG);
        pixDestroy(&pix1);
    }

    boxaDestroy(&boxaf);
    numaDestroy(&nal);
    numaDestroy(&nat);
    numaDestroy(&nar);
    numaDestroy(&nab);
    numaDestroy(&naml);
    numaDestroy(&namt);
    numaDestroy(&namr);
    numaDestroy(&namb);
    return boxad;
}


/*!
 * \brief   boxaModifyWithBoxa()
 *
 * \param[in]    boxas
 * \param[in]    boxam        boxa with boxes used to modify those in boxas
 * \param[in]    subflag      L_USE_MINSIZE, L_USE_MAXSIZE,
 *                            L_SUB_ON_LOC_DIFF, L_SUB_ON_SIZE_DIFF,
 *                            L_USE_CAPPED_MIN, L_USE_CAPPED_MAX
 * \param[in]    maxdiff      parameter used with L_SUB_ON_LOC_DIFF,
 *                            L_SUB_ON_SIZE_DIFF, L_USE_CAPPED_MIN,
 *                            L_USE_CAPPED_MAX
 * \param[in]    extrapixels  pixels added on all sides (or subtracted
 *                            if %extrapixels < 0) when using
 *                            L_SUB_ON_LOC_DIFF and L_SUB_ON_SIZE_DIFF
 * \return  boxad  result after adjusting boxes in boxas, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) This takes two input boxa (boxas, boxam) and constructs boxad,
 *          where each box in boxad is generated from the corresponding
 *          boxes in boxas and boxam.  The rule for constructing each
 *          output box depends on %subflag and %maxdiff.  Let boxs be
 *          a box from %boxas and boxm be a box from %boxam.
 *          * If %subflag == L_USE_MINSIZE: the output box is the intersection
 *            of the two input boxes.
 *          * If %subflag == L_USE_MAXSIZE: the output box is the union of the
 *            two input boxes; i.e., the minimum bounding rectangle for the
 *            two input boxes.
 *          * If %subflag == L_SUB_ON_LOC_DIFF: each side of the output box
 *            is found separately from the corresponding side of boxs and boxm.
 *            Use the boxm side, expanded by %extrapixels, if greater than
 *            %maxdiff pixels from the boxs side.
 *          * If %subflag == L_SUB_ON_SIZE_DIFF: the sides of the output box
 *            are determined in pairs from the width and height of boxs
 *            and boxm.  If the boxm width differs by more than %maxdiff
 *            pixels from boxs, use the boxm left and right sides,
 *            expanded by %extrapixels.  Ditto for the height difference.
 *          For the last two flags, each side of the output box is found
 *          separately from the corresponding side of boxs and boxm,
 *          according to these rules, where "smaller"("bigger") mean in a
 *          direction that decreases(increases) the size of the output box:
 *          * If %subflag == L_USE_CAPPED_MIN: use the Min of boxm
 *            with the Max of (boxs, boxm +- %maxdiff), where the sign
 *            is adjusted to make the box smaller (e.g., use "+" on left side).
 *          * If %subflag == L_USE_CAPPED_MAX: use the Max of boxm
 *            with the Min of (boxs, boxm +- %maxdiff), where the sign
 *            is adjusted to make the box bigger (e.g., use "-" on left side).
 *          Use of the last 2 flags is further explained in (3) and (4).
 *      (2) boxas and boxam must be the same size.  If boxam == NULL,
 *          this returns a copy of boxas with a warning.
 *      (3) If %subflag == L_SUB_ON_LOC_DIFF, use boxm for each side
 *          where the corresponding sides differ by more than %maxdiff.
 *          Two extreme cases:
 *          (a) set %maxdiff == 0 to use only values from boxam in boxad.
 *          (b) set %maxdiff == 10000 to ignore all values from boxam;
 *              then boxad will be the same as boxas.
 *      (4) If %subflag == L_USE_CAPPED_MAX: use boxm if boxs is smaller;
 *          use boxs if boxs is bigger than boxm by an amount up to %maxdiff;
 *          and use boxm +- %maxdiff (the 'capped' value) if boxs is
 *          bigger than boxm by an amount larger than %maxdiff.
 *          Similarly, with interchange of Min/Max and sign of %maxdiff,
 *          for %subflag == L_USE_CAPPED_MIN.
 *      (5) If either of corresponding boxes in boxas and boxam is invalid,
 *          an invalid box is copied to the result.
 *      (6) Typical input for boxam may be the output of boxaLinearFit().
 *          where outliers have been removed and each side is LS fit to a line.
 *      (7) Unlike boxaAdjustWidthToTarget() and boxaAdjustHeightToTarget(),
 *          this uses two boxes and does not specify target dimensions.
 * </pre>
 */
BOXA *
boxaModifyWithBoxa(BOXA    *boxas,
                   BOXA    *boxam,
                   l_int32  subflag,
                   l_int32  maxdiff,
                   l_int32  extrapixels)
{
l_int32  n, i, ls, ts, rs, bs, ws, hs, lm, tm, rm, bm, wm, hm, ld, td, rd, bd;
BOX     *boxs, *boxm, *boxd, *boxempty;
BOXA    *boxad;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (!boxam) {
        L_WARNING("boxam not defined; returning copy", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if (subflag != L_USE_MINSIZE && subflag != L_USE_MAXSIZE &&
        subflag != L_SUB_ON_LOC_DIFF && subflag != L_SUB_ON_SIZE_DIFF &&
        subflag != L_USE_CAPPED_MIN && subflag != L_USE_CAPPED_MAX) {
        L_WARNING("invalid subflag; returning copy", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    n = boxaGetCount(boxas);
    if (n != boxaGetCount(boxam)) {
        L_WARNING("boxas and boxam sizes differ; returning copy", __func__);
        return boxaCopy(boxas, L_COPY);
    }

    boxad = boxaCreate(n);
    boxempty = boxCreate(0, 0, 0, 0);  /* placeholders */
    for (i = 0; i < n; i++) {
        boxs = boxaGetValidBox(boxas, i, L_CLONE);
        boxm = boxaGetValidBox(boxam, i, L_CLONE);
        if (!boxs || !boxm) {
            boxaAddBox(boxad, boxempty, L_COPY);
        } else {
            boxGetGeometry(boxs, &ls, &ts, &ws, &hs);
            boxGetGeometry(boxm, &lm, &tm, &wm, &hm);
            rs = ls + ws - 1;
            bs = ts + hs - 1;
            rm = lm + wm - 1;
            bm = tm + hm - 1;
            if (subflag == L_USE_MINSIZE) {
                ld = L_MAX(ls, lm);
                rd = L_MIN(rs, rm);
                td = L_MAX(ts, tm);
                bd = L_MIN(bs, bm);
            } else if (subflag == L_USE_MAXSIZE) {
                ld = L_MIN(ls, lm);
                rd = L_MAX(rs, rm);
                td = L_MIN(ts, tm);
                bd = L_MAX(bs, bm);
            } else if (subflag == L_SUB_ON_LOC_DIFF) {
                ld = (L_ABS(lm - ls) <= maxdiff) ? ls : lm - extrapixels;
                td = (L_ABS(tm - ts) <= maxdiff) ? ts : tm - extrapixels;
                rd = (L_ABS(rm - rs) <= maxdiff) ? rs : rm + extrapixels;
                bd = (L_ABS(bm - bs) <= maxdiff) ? bs : bm + extrapixels;
            } else if (subflag == L_SUB_ON_SIZE_DIFF) {
                ld = (L_ABS(wm - ws) <= maxdiff) ? ls : lm - extrapixels;
                td = (L_ABS(hm - hs) <= maxdiff) ? ts : tm - extrapixels;
                rd = (L_ABS(wm - ws) <= maxdiff) ? rs : rm + extrapixels;
                bd = (L_ABS(hm - hs) <= maxdiff) ? bs : bm + extrapixels;
            } else if (subflag == L_USE_CAPPED_MIN) {
                ld = L_MAX(lm, L_MIN(ls, lm + maxdiff));
                td = L_MAX(tm, L_MIN(ts, tm + maxdiff));
                rd = L_MIN(rm, L_MAX(rs, rm - maxdiff));
                bd = L_MIN(bm, L_MAX(bs, bm - maxdiff));
            } else {  /* subflag == L_USE_CAPPED_MAX */
                ld = L_MIN(lm, L_MAX(ls, lm - maxdiff));
                td = L_MIN(tm, L_MAX(ts, tm - maxdiff));
                rd = L_MAX(rm, L_MIN(rs, rm + maxdiff));
                bd = L_MAX(bm, L_MIN(bs, bm + maxdiff));
            }
            boxd = boxCreate(ld, td, rd - ld + 1, bd - td + 1);
            boxaAddBox(boxad, boxd, L_INSERT);
        }
        boxDestroy(&boxs);
        boxDestroy(&boxm);
    }
    boxDestroy(&boxempty);

    return boxad;
}


/*!
 * \brief   boxaReconcilePairWidth()
 *
 * \param[in]    boxas
 * \param[in]    delw      threshold on adjacent width difference
 * \param[in]    op        L_ADJUST_CHOOSE_MIN, L_ADJUST_CHOOSE_MAX
 * \param[in]    factor    > 0.0, typically near 1.0
 * \param[in]    na        [optional] indicator array allowing change
 * \return  boxad adjusted, or a copy of boxas on error
 *
 * <pre>
 * Notes:
 *      (1) This reconciles differences in the width of adjacent boxes,
 *          by moving one side of one of the boxes in each pair.
 *          If the widths in the pair differ by more than some
 *          threshold, move either the left side for even boxes or
 *          the right side for odd boxes, depending on if we're choosing
 *          the min or max.  If choosing min, the width of the max is
 *          set to factor * (width of min).  If choosing max, the width
 *          of the min is set to factor * (width of max).
 *      (2) If %na exists, it is an indicator array corresponding to the
 *          boxes in %boxas.  If %na != NULL, only boxes with an
 *          indicator value of 1 are allowed to adjust; otherwise,
 *          all boxes can adjust.
 *      (3) Typical input might be the output of boxaSmoothSequenceMedian(),
 *          where even and odd boxa have been independently regulated.
 * </pre>
 */
BOXA *
boxaReconcilePairWidth(BOXA      *boxas,
                       l_int32    delw,
                       l_int32    op,
                       l_float32  factor,
                       NUMA      *na)
{
l_int32  i, ne, no, nmin, xe, we, xo, wo, inde, indo, x, w;
BOX     *boxe, *boxo;
BOXA    *boxae, *boxao, *boxad;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (factor <= 0.0) {
        L_WARNING("invalid factor; setting to 1.0\n", __func__);
        factor = 1.0;
    }

        /* Taking the boxes in pairs, if the difference in width reaches
         * the threshold %delw, adjust the left or right side of one
         * of the pair. */
    boxaSplitEvenOdd(boxas, 0, &boxae, &boxao);
    ne = boxaGetCount(boxae);
    no = boxaGetCount(boxao);
    nmin = L_MIN(ne, no);
    for (i = 0; i < nmin; i++) {
            /* Set indicator values */
        if (na) {
            numaGetIValue(na, 2 * i, &inde);
            numaGetIValue(na, 2 * i + 1, &indo);
        } else {
            inde = indo = 1;
        }
        if (inde == 0 && indo == 0) continue;

        boxe = boxaGetBox(boxae, i, L_CLONE);
        boxo = boxaGetBox(boxao, i, L_CLONE);
        boxGetGeometry(boxe, &xe, NULL, &we, NULL);
        boxGetGeometry(boxo, &xo, NULL, &wo, NULL);
        if (we == 0 || wo == 0) {  /* if either is invalid; skip */
            boxDestroy(&boxe);
            boxDestroy(&boxo);
            continue;
        } else if (L_ABS(we - wo) > delw) {
            if (op == L_ADJUST_CHOOSE_MIN) {
                if (we > wo && inde == 1) {
                        /* move left side of even to the right */
                    w = factor * wo;
                    x = xe + (we - w);
                    boxSetGeometry(boxe, x, -1, w, -1);
                } else if (we < wo && indo == 1) {
                        /* move right side of odd to the left */
                    w = factor * we;
                    boxSetGeometry(boxo, -1, -1, w, -1);
                }
            } else {  /* maximize width */
                if (we < wo && inde == 1) {
                        /* move left side of even to the left */
                    w = factor * wo;
                    x = L_MAX(0, xe + (we - w));
                    w = we + (xe - x);  /* covers both cases for the max */
                    boxSetGeometry(boxe, x, -1, w, -1);
                } else if (we > wo && indo == 1) {
                        /* move right side of odd to the right */
                    w = factor * we;
                    boxSetGeometry(boxo, -1, -1, w, -1);
                }
            }
        }
        boxDestroy(&boxe);
        boxDestroy(&boxo);
    }

    boxad = boxaMergeEvenOdd(boxae, boxao, 0);
    boxaDestroy(&boxae);
    boxaDestroy(&boxao);
    return boxad;
}


/*!
 * \brief   boxaSizeConsistency()
 *
 * \param[in]    boxas     of size >= 10
 * \param[in]    type      L_CHECK_WIDTH, L_CHECK_HEIGHT
 * \param[in]    threshp   threshold for pairwise fractional variation
 * \param[in]    threshm   threshold for fractional variation from median
 * \param[out]   pfvarp    [optional] average fractional pairwise variation
 * \param[out]   pfvarm    [optional] average fractional median variation
 * \param[out]   psame     decision for uniformity of page size (1, 0, -1)
 *
 * <pre>
 * Notes:
 *      (1) This evaluates a boxa for particular types of dimensional
 *          variation.  Select either width or height variation.  Then
 *          it returns two numbers: one is based on pairwise (even/odd)
 *          variation; the other is based on the average variation
 *          from the boxa median.
 *      (2) For the pairwise variation, get the fraction of the absolute
 *          difference in dimension of each pair of boxes, and take
 *          the average value.  The median variation is simply the
 *          the average of the fractional deviation from the median
 *          of all the boxes.
 *      (3) Use 0 for default values of %threshp and %threshm.  They are
 *            threshp:  0.02
 *            threshm:  0.015
 *      (4) The intended application is that the boxes are a sequence of
 *          page regions in a book scan, and we calculate two numbers
 *          that can give an indication if the pages are approximately
 *          the same size.  The pairwise variation should be small if
 *          the boxes are correctly calculated.  If there are a
 *          significant number of random or systematic outliers, the
 *          pairwise variation will be large, and no decision will be made
 *          (i.e., return same == -1).  Here are the possible outcomes:
 *            Pairwise Var    Median Var    Decision
 *            ------------    ----------    --------
 *            small           small         same size  (1)
 *            small           large         different size  (0)
 *            large           small/large   unknown   (-1)
 * </pre>
 */
l_ok
boxaSizeConsistency(BOXA       *boxas,
                    l_int32     type,
                    l_float32   threshp,
                    l_float32   threshm,
                    l_float32  *pfvarp,
                    l_float32  *pfvarm,
                    l_int32    *psame)
{
l_int32    i, n, bw1, bh1, bw2, bh2, npairs;
l_float32  ave, fdiff, sumdiff, med, fvarp, fvarm;
NUMA      *na1;

    if (pfvarp) *pfvarp = 0.0;
    if (pfvarm) *pfvarm = 0.0;
    if (!psame)
        return ERROR_INT("&same not defined", __func__, 1);
    *psame = -1;
    if (!boxas)
        return ERROR_INT("boxas not defined", __func__, 1);
    if (boxaGetValidCount(boxas) < 6)
        return ERROR_INT("need a least 6 valid boxes", __func__, 1);
    if (type != L_CHECK_WIDTH && type != L_CHECK_HEIGHT)
        return ERROR_INT("invalid type", __func__, 1);
    if (threshp < 0.0 || threshp >= 0.5)
        return ERROR_INT("invalid threshp", __func__, 1);
    if (threshm < 0.0 || threshm >= 0.5)
        return ERROR_INT("invalid threshm", __func__, 1);
    if (threshp == 0.0) threshp = 0.02f;
    if (threshm == 0.0) threshm = 0.015f;

        /* Evaluate pairwise variation */
    n = boxaGetCount(boxas);
    na1 = numaCreate(0);
    for (i = 0, npairs = 0, sumdiff = 0; i < n - 1; i += 2) {
        boxaGetBoxGeometry(boxas, i, NULL, NULL, &bw1, &bh1);
        boxaGetBoxGeometry(boxas, i + 1, NULL, NULL, &bw2, &bh2);
        if (bw1 == 0 || bh1 == 0 || bw2 == 0 || bh2 == 0)
            continue;
        npairs++;
        if (type == L_CHECK_WIDTH) {
            ave = (bw1 + bw2) / 2.0;
            fdiff = L_ABS(bw1 - bw2) / ave;
            numaAddNumber(na1, bw1);
            numaAddNumber(na1, bw2);
        } else {  /* type == L_CHECK_HEIGHT) */
            ave = (bh1 + bh2) / 2.0;
            fdiff = L_ABS(bh1 - bh2) / ave;
            numaAddNumber(na1, bh1);
            numaAddNumber(na1, bh2);
        }
        sumdiff += fdiff;
    }
    fvarp = sumdiff / npairs;
    if (pfvarp) *pfvarp = fvarp;

        /* Evaluate the average abs fractional deviation from the median */
    numaGetMedian(na1, &med);
    if (med == 0.0) {
        L_WARNING("median value is 0\n", __func__);
    } else {
        numaGetMeanDevFromMedian(na1, med, &fvarm);
        fvarm /= med;
        if (pfvarm) *pfvarm = fvarm;
    }
    numaDestroy(&na1);

        /* Make decision */
    if (fvarp < threshp && fvarm < threshm)
        *psame = 1;
    else if (fvarp < threshp && fvarm > threshm)
        *psame = 0;
    else
        *psame = -1;  /* unknown */
    return 0;
}


/*!
 * \brief   boxaReconcileAllByMedian()
 *
 * \param[in]    boxas    containing at least 6 valid boxes
 * \param[in]    select1  L_ADJUST_LEFT_AND_RIGHT or L_ADJUST_SKIP
 * \param[in]    select2  L_ADJUST_TOP_AND_BOT or L_ADJUST_SKIP
 * \param[in]    thresh   threshold number of pixels to make adjustment
 * \param[in]    extra    extra pixels to add beyond median value
 * \param[in]    pixadb   use NULL to skip debug output
 * \return  boxad  possibly adjusted from boxas; a copy of boxas on error
 *
 * <pre>
 * Notes:
 *      (1) This uses boxaReconcileSidesByMedian() to reconcile
 *          the left-and-right and/or top-and-bottom sides of the
 *          even and odd boxes, separately.
 *      (2) See boxaReconcileSidesByMedian() for use of %thresh and %extra.
 *      (3) If all box sides are within %thresh of the median value,
 *          the returned box will be identical to %boxas.
 * </pre>
 */
BOXA *
boxaReconcileAllByMedian(BOXA    *boxas,
                         l_int32  select1,
                         l_int32  select2,
                         l_int32  thresh,
                         l_int32  extra,
                         PIXA    *pixadb)
 {
l_int32  ncols;
BOXA    *boxa1e, *boxa1o, *boxa2e, *boxa2o, *boxa3e, *boxa3o, *boxad;
PIX     *pix1;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (select1 != L_ADJUST_LEFT_AND_RIGHT && select1 != L_ADJUST_SKIP) {
        L_WARNING("invalid select1; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if (select2 != L_ADJUST_TOP_AND_BOT && select2 != L_ADJUST_SKIP) {
        L_WARNING("invalid select2; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if (thresh < 0) {
        L_WARNING("thresh must be >= 0; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if (boxaGetValidCount(boxas) < 3) {
        L_WARNING("need at least 3 valid boxes; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }

        /* Adjust even and odd box sides separately */
    boxaSplitEvenOdd(boxas, 0, &boxa1e, &boxa1o);
    ncols = 1;
    if (select1 == L_ADJUST_LEFT_AND_RIGHT) {
        ncols += 2;
        boxa2e = boxaReconcileSidesByMedian(boxa1e, select1, thresh,
                                            extra, pixadb);
    } else {
        boxa2e = boxaCopy(boxa1e, L_COPY);
    }
    if (select2 == L_ADJUST_TOP_AND_BOT) {
        ncols += 2;
        boxa3e = boxaReconcileSidesByMedian(boxa2e, select2, thresh,
                                            extra, pixadb);
    } else {
        boxa3e = boxaCopy(boxa2e, L_COPY);
    }
    if (select1 == L_ADJUST_LEFT_AND_RIGHT)
        boxa2o = boxaReconcileSidesByMedian(boxa1o, select1, thresh,
                                            extra, pixadb);
    else
        boxa2o = boxaCopy(boxa1o, L_COPY);
    if (select2 == L_ADJUST_TOP_AND_BOT)
        boxa3o = boxaReconcileSidesByMedian(boxa2o, select2, thresh,
                                            extra, pixadb);
    else
        boxa3o = boxaCopy(boxa2o, L_COPY);
    boxad = boxaMergeEvenOdd(boxa3e, boxa3o, 0);

        /* This generates 2 sets of 3 or 5 plots in a row, depending
         * on whether select1 and select2 are true (not skipping).
         * The top row is for even boxes; the bottom row is for odd boxes. */
    if (pixadb) {
        lept_mkdir("lept/boxa");
        pix1 = pixaDisplayTiledInColumns(pixadb, ncols, 1.0, 30, 2);
        pixWrite("/tmp/lept/boxa/recon_sides.png", pix1, IFF_PNG);
        pixDestroy(&pix1);
    }

    boxaDestroy(&boxa1e);
    boxaDestroy(&boxa1o);
    boxaDestroy(&boxa2e);
    boxaDestroy(&boxa2o);
    boxaDestroy(&boxa3e);
    boxaDestroy(&boxa3o);
    return boxad;
}


/*!
 * \brief   boxaReconcileSidesByMedian()
 *
 * \param[in]    boxas    containing at least 3 valid boxes
 * \param[in]    select   L_ADJUST_LEFT, L_ADJUST_RIGHT, etc.
 * \param[in]    thresh   threshold number of pixels to make adjustment
 * \param[in]    extra    extra pixels to add beyond median value
 * \param[in]    pixadb   use NULL to skip debug output
 * \return  boxad  possibly adjusted from boxas; a copy of boxas on error
 *
 * <pre>
 * Notes:
 *      (1) This modifies individual box sides if their location differs
 *          significantly (>= %thresh) from the median value.
 *      (2) %select specifies which sides are to be checked.
 *      (3) %thresh specifies the tolerance for different side locations.
 *          Any box side that differs from the median by this much will
 *          be set to the median value, plus the %extra amount.
 *      (4) If %extra is positive, the box dimensions are expanded.
 *          For example, for the left side, a positive %extra results in
 *          moving the left side farther to the left (i.e., in a negative
 *          direction).
 *      (5) If all box sides are within %thresh - 1 of the median value,
 *          the returned box will be identical to %boxas.
 *      (6) N.B. If you expect that even and odd box sides should be
 *          significantly different, this function must be called separately
 *          on the even and odd boxes in %boxas.  Note also that the
 *          higher level function boxaReconcileAllByMedian() handles the
 *          even and odd box sides separately.
 * </pre>
 */
BOXA *
boxaReconcileSidesByMedian(BOXA    *boxas,
                           l_int32  select,
                           l_int32  thresh,
                           l_int32  extra,
                           PIXA    *pixadb)
 {
char     buf[128];
l_int32  i, n, diff;
l_int32  left, right, top, bot, medleft, medright, medtop, medbot;
BOX     *box;
BOXA    *boxa1, *boxad;
PIX     *pix;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (select != L_ADJUST_LEFT && select != L_ADJUST_RIGHT &&
        select != L_ADJUST_TOP && select != L_ADJUST_BOT &&
        select != L_ADJUST_LEFT_AND_RIGHT && select != L_ADJUST_TOP_AND_BOT) {
        L_WARNING("invalid select; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if (thresh < 0) {
        L_WARNING("thresh must be >= 0; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if (boxaGetValidCount(boxas) < 3) {
        L_WARNING("need at least 3 valid boxes; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }

    if (select == L_ADJUST_LEFT_AND_RIGHT) {
        boxa1 = boxaReconcileSidesByMedian(boxas, L_ADJUST_LEFT, thresh, extra,
                                           pixadb);
        boxad = boxaReconcileSidesByMedian(boxa1, L_ADJUST_RIGHT, thresh, extra,
                                           pixadb);
        boxaDestroy(&boxa1);
        return boxad;
    }
    if (select == L_ADJUST_TOP_AND_BOT) {
        boxa1 = boxaReconcileSidesByMedian(boxas, L_ADJUST_TOP, thresh, extra,
                                           pixadb);
        boxad = boxaReconcileSidesByMedian(boxa1, L_ADJUST_BOT, thresh, extra,
                                           pixadb);
        boxaDestroy(&boxa1);
        return boxad;
    }

    if (pixadb) {
        l_int32 ndb = pixaGetCount(pixadb);
        if (ndb == 0 || ndb == 5) {  /* first of even and odd box sets */
            adjustSidePlotName(buf, sizeof(buf), "init", select);
            boxaPlotSides(boxas, buf, NULL, NULL, NULL, NULL, &pix);
            pixaAddPix(pixadb, pix, L_INSERT);
        }
    }

    n = boxaGetCount(boxas);
    boxad = boxaCreate(n);
    if (select == L_ADJUST_LEFT) {
        boxaGetMedianVals(boxas, &medleft, NULL, NULL, NULL, NULL, NULL);
        for (i = 0; i < n; i++) {
            box = boxaGetBox(boxas, i, L_COPY);
            boxGetSideLocations(box, &left, NULL, NULL, NULL);
            diff = medleft - left;
            if (L_ABS(diff) >= thresh)
                boxAdjustSides(box, box, diff - extra, 0, 0, 0);
            boxaAddBox(boxad, box, L_INSERT);
        }
    } else if (select == L_ADJUST_RIGHT) {
        boxaGetMedianVals(boxas, NULL, NULL, &medright, NULL, NULL, NULL);
        for (i = 0; i < n; i++) {
            box = boxaGetBox(boxas, i, L_COPY);
            boxGetSideLocations(box, NULL, &right, NULL, NULL);
            diff = medright - right;
            if (L_ABS(diff) >= thresh)
                boxAdjustSides(box, box, 0, diff + extra, 0, 0);
            boxaAddBox(boxad, box, L_INSERT);
        }
    } else if (select == L_ADJUST_TOP) {
        boxaGetMedianVals(boxas, NULL, &medtop, NULL, NULL, NULL, NULL);
        for (i = 0; i < n; i++) {
            box = boxaGetBox(boxas, i, L_COPY);
            boxGetSideLocations(box, NULL, NULL, &top, NULL);
            diff = medtop - top;
            if (L_ABS(diff) >= thresh)
                boxAdjustSides(box, box, 0, 0, diff - extra, 0);
            boxaAddBox(boxad, box, L_INSERT);
        }
    } else {  /* select == L_ADJUST_BOT */
        boxaGetMedianVals(boxas, NULL, NULL, NULL, &medbot, NULL, NULL);
        for (i = 0; i < n; i++) {
            box = boxaGetBox(boxas, i, L_COPY);
            boxGetSideLocations(box, NULL, NULL, NULL, &bot);
            diff = medbot - bot;
            if (L_ABS(diff) >= thresh)
                boxAdjustSides(box, box, 0, 0, 0, diff + extra);
            boxaAddBox(boxad, box, L_INSERT);
        }
    }

    if (pixadb) {
        adjustSidePlotName(buf, sizeof(buf), "final", select);
        boxaPlotSides(boxad, buf, NULL, NULL, NULL, NULL, &pix);
        pixaAddPix(pixadb, pix, L_INSERT);
    }
    return boxad;
}


static void
adjustSidePlotName(char        *buf,
                   size_t       size,
                   const char  *preface,
                   l_int32      select)
{
    stringCopy(buf, preface, size - 8);
    if (select == L_ADJUST_LEFT)
        stringCat(buf, size, "-left");
    else if (select == L_ADJUST_RIGHT)
        stringCat(buf, size, "-right");
    else if (select == L_ADJUST_TOP)
        stringCat(buf, size, "-top");
    else if (select == L_ADJUST_BOT)
        stringCat(buf, size, "-bot");
}


/*!
 * \brief   boxaReconcileSizeByMedian()
 *
 * \param[in]    boxas    containing at least 6 valid boxes
 * \param[in]    type     L_CHECK_WIDTH, L_CHECK_HEIGHT, L_CHECK_BOTH
 * \param[in]    dfract   threshold fraction of dimensional variation from
 *                        median; in range (0 ... 1); typ. about 0.05.
 * \param[in]    sfract   threshold fraction of side variation from median;
 *                        in range (0 ... 1); typ. about 0.04.
 * \param[in]    factor   expansion for fixed box beyond median width;
 *                        should be near 1.0.
 * \param[out]   pnadelw  [optional] diff from median width for boxes
 *                        above threshold
 * \param[out]   pnadelh  [optional] diff from median height for boxes
 *                        above threshold
 * \param[out]   pratiowh [optional] ratio of median width/height of boxas
 * \return  boxad  possibly adjusted from boxas; a copy of boxas on error
 *
 * <pre>
 * Notes:
 *      (1) The basic idea is to identify significant differences in box
 *          dimension (either width or height) and modify the outlier boxes.
 *      (2) %type specifies if we are reconciling the width, height or both.
 *      (3) %dfract specifies the tolerance for different dimensions. Any
 *          box with a fractional difference from the median size that
 *          exceeds %dfract will be altered.
 *      (4) %sfract specifies the tolerance for different side locations.
 *          If a box has been marked by (3) for alteration, any side
 *          location that differs from the median side location by
 *          more than %sfract of the median dimension (medw or medh)
 *          will be moved.
 *      (5) Median width and height are found for all valid boxes (i.e.,
 *          for all boxes with width and height > 0.
 *          Median side locations are found separately for even and odd boxes,
 *          using only boxes that are "inliers"; i.e., that have been
 *          found by (3) to be within tolerance for width or height.
 *      (6) If all box dimensions are within threshold of the median size,
 *          just return a copy.  Otherwise, box sides of the outliers
 *          will be adjusted.
 *      (7) Using %sfract, sides that are sufficiently far from the median
 *          are first moved to the median value.  Then they are moved
 *          together (in or out) so that the final box dimension
 *          is %factor times the median dimension.
 *      (8) The arrays that are the initial deviation from median size
 *          (width and height) are optionally returned.  Also optionally
 *          returned is the median w/h asperity ratio of the input %boxas.
 * </pre>
 */
BOXA *
boxaReconcileSizeByMedian(BOXA       *boxas,
                          l_int32     type,
                          l_float32   dfract,
                          l_float32   sfract,
                          l_float32   factor,
                          NUMA      **pnadelw,
                          NUMA      **pnadelh,
                          l_float32  *pratiowh)
{
l_int32    i, n, ne, no, outfound, isvalid, ind, del, maxdel;
l_int32    medw, medh, bw, bh, left, right, top, bot;
l_int32    medleft, medlefte, medlefto, medright, medrighte, medrighto;
l_int32    medtop, medtope, medtopo, medbot, medbote, medboto;
l_float32  brat;
BOX       *box;
BOXA      *boxa1, *boxae, *boxao, *boxad;
NUMA      *naind, *nadelw, *nadelh;

    if (pnadelw) *pnadelw = NULL;
    if (pnadelh) *pnadelh = NULL;
    if (pratiowh) *pratiowh = 0.0;
    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (type != L_CHECK_WIDTH && type != L_CHECK_HEIGHT &&
        type != L_CHECK_BOTH) {
        L_WARNING("invalid type; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if (dfract <= 0.0 || dfract >= 0.5) {
        L_WARNING("invalid dimensional fract; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if (sfract <= 0.0 || sfract >= 0.5) {
        L_WARNING("invalid side fract; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }
    if (factor < 0.8 || factor > 1.25)
        L_WARNING("factor %5.3f is typ. closer to 1.0\n", __func__, factor);
    if (boxaGetValidCount(boxas) < 6) {
        L_WARNING("need at least 6 valid boxes; returning copy\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }

        /* If reconciling both width and height, optionally return array of
         * median deviations and even/odd ratio for width measurements */
    if (type == L_CHECK_BOTH) {
        boxa1 = boxaReconcileSizeByMedian(boxas, L_CHECK_WIDTH, dfract, sfract,
                                          factor, pnadelw, NULL, pratiowh);
        boxad = boxaReconcileSizeByMedian(boxa1, L_CHECK_HEIGHT, dfract, sfract,
                                          factor, NULL, pnadelh, NULL);
        boxaDestroy(&boxa1);
        return boxad;
    }

    n = boxaGetCount(boxas);
    naind = numaCreate(n);  /* outlier indicator array */
    boxae = boxaCreate(0);  /* even inliers */
    boxao = boxaCreate(0);  /* odd inliers */
    outfound = FALSE;
    if (type == L_CHECK_WIDTH) {
        boxaMedianDimensions(boxas, &medw, &medh, NULL, NULL, NULL, NULL,
                             &nadelw, NULL);
        if (pratiowh) {
            *pratiowh = (l_float32)medw / (l_float32)medh;
            L_INFO("median ratio w/h = %5.3f\n", __func__, *pratiowh);
        }
        if (pnadelw)
            *pnadelw = nadelw;
        else
            numaDestroy(&nadelw);

            /* Check for outliers; assemble inliers */
        for (i = 0; i < n; i++) {
            if ((box = boxaGetValidBox(boxas, i, L_COPY)) == NULL) {
                numaAddNumber(naind, 0);
                continue;
            }
            boxGetGeometry(box, NULL, NULL, &bw, NULL);
            brat = (l_float32)bw / (l_float32)medw;
            if (brat < 1.0 - dfract || brat > 1.0 + dfract) {
                outfound = TRUE;
                numaAddNumber(naind, 1);
                boxDestroy(&box);
            } else {  /* add to inliers */
                numaAddNumber(naind, 0);
                if (i % 2 == 0)
                    boxaAddBox(boxae, box, L_INSERT);
                else
                    boxaAddBox(boxao, box, L_INSERT);
            }
        }
        if (!outfound) {  /* nothing to do */
            numaDestroy(&naind);
            boxaDestroy(&boxae);
            boxaDestroy(&boxao);
            L_INFO("no width outlier boxes found\n", __func__);
            return boxaCopy(boxas, L_COPY);
        }

            /* Get left/right parameters from inliers.  Handle the case
             * where there are no inliers for one of the sets.  For example,
             * when all the even boxes have a different dimension from
             * the odd boxes, and the median arbitrarily gets assigned
             * to the even boxes, there are no odd inliers; in that case,
             * use the even inliers sides to decide whether to adjust
             * the left or the right sides of individual outliers. */
        L_INFO("fixing width of outlier boxes\n", __func__);
        medlefte = medrighte = medlefto = medrighto = 0;
        if ((ne = boxaGetValidCount(boxae)) > 0)
            boxaGetMedianVals(boxae, &medlefte, NULL, &medrighte, NULL,
                              NULL, NULL);
        if ((no = boxaGetValidCount(boxao)) > 0)
            boxaGetMedianVals(boxao, &medlefto, NULL, &medrighto, NULL,
                              NULL, NULL);
        if (ne == 0) {  /* use odd inliers values for both */
            medlefte = medlefto;
            medrighte = medrighto;
        } else if (no == 0) {  /* use even inliers values for both */
            medlefto = medlefte;
            medrighto = medrighte;
        }

            /* Adjust the left and/or right sides of outliers.
             * For each box that is a dimensional outlier, consider each side.
             * Any side that differs fractionally from the median value
             * by more than %sfract times the median width (medw) is set to
             * the median value for that side.  Then both sides are moved
             * an equal distance in or out to make w = %factor * medw. */
        boxad = boxaCreate(n);
        maxdel = (l_int32)(sfract * medw + 0.5);
        for (i = 0; i < n; i++) {
            box = boxaGetBox(boxas, i, L_COPY);
            boxIsValid(box, &isvalid);
            numaGetIValue(naind, i, &ind);
            medleft = (i % 2 == 0) ? medlefte : medlefto;
            medright = (i % 2 == 0) ? medrighte : medrighto;
            if (ind == 1 && isvalid) {  /* adjust sides */
                boxGetSideLocations(box, &left, &right, NULL, NULL);
                if (L_ABS(left - medleft) > maxdel) left = medleft;
                if (L_ABS(right - medright) > maxdel) right = medright;
                del = (l_int32)(factor * medw - (right - left)) / 2;
                boxSetSide(box, L_SET_LEFT, left - del, 0);
                boxSetSide(box, L_SET_RIGHT, right + del, 0);
            }
            boxaAddBox(boxad, box, L_INSERT);
        }
    } else {  /* L_CHECK_HEIGHT */
        boxaMedianDimensions(boxas, &medw, &medh, NULL, NULL, NULL, NULL,
                             NULL, &nadelh);
        if (pratiowh) {
            *pratiowh = (l_float32)medw / (l_float32)medh;
            L_INFO("median ratio w/h = %5.3f\n", __func__, *pratiowh);
        }
        if (pnadelh)
            *pnadelh = nadelh;
        else
            numaDestroy(&nadelh);

            /* Check for outliers; assemble inliers */
        for (i = 0; i < n; i++) {
            if ((box = boxaGetValidBox(boxas, i, L_COPY)) == NULL) {
                numaAddNumber(naind, 0);
                continue;
            }
            boxGetGeometry(box, NULL, NULL, NULL, &bh);
            brat = (l_float32)bh / (l_float32)medh;
            if (brat < 1.0 - dfract || brat > 1.0 + dfract) {
                outfound = TRUE;
                numaAddNumber(naind, 1);
                boxDestroy(&box);
            } else {  /* add to inliers */
                numaAddNumber(naind, 0);
                if (i % 2 == 0)
                    boxaAddBox(boxae, box, L_INSERT);
                else
                    boxaAddBox(boxao, box, L_INSERT);
            }
        }
        if (!outfound) {  /* nothing to do */
            numaDestroy(&naind);
            boxaDestroy(&boxae);
            boxaDestroy(&boxao);
            L_INFO("no height outlier boxes found\n", __func__);
            return boxaCopy(boxas, L_COPY);
        }

            /* Get top/bot parameters from inliers.  Handle the case
             * where there are no inliers for one of the sets.  For example,
             * when all the even boxes have a different dimension from
             * the odd boxes, and the median arbitrarily gets assigned
             * to the even boxes, there are no odd inliers; in that case,
             * use the even inlier sides to decide whether to adjust
             * the top or the bottom sides of individual outliers. */
        L_INFO("fixing height of outlier boxes\n", __func__);
        medlefte = medtope = medbote = medtopo = medboto = 0;
        if ((ne = boxaGetValidCount(boxae)) > 0)
            boxaGetMedianVals(boxae, NULL, &medtope, NULL, &medbote,
                              NULL, NULL);
        if ((no = boxaGetValidCount(boxao)) > 0)
            boxaGetMedianVals(boxao, NULL, &medtopo, NULL, &medboto,
                              NULL, NULL);
        if (ne == 0) {  /* use odd inliers values for both */
            medtope = medtopo;
            medbote = medboto;
        } else if (no == 0) {  /* use even inliers values for both */
            medtopo = medtope;
            medboto = medbote;
        }

            /* Adjust the top and/or bottom sides of outliers.
             * For each box that is a dimensional outlier, consider each side.
             * Any side that differs fractionally from the median value
             * by more than %sfract times the median height (medh) is
             * set to the median value for that that side.  Then both
             * sides are moved an equal distance in or out to make
             * h = %factor * medh). */
        boxad = boxaCreate(n);
        maxdel = (l_int32)(sfract * medh + 0.5);
        for (i = 0; i < n; i++) {
            box = boxaGetBox(boxas, i, L_COPY);
            boxIsValid(box, &isvalid);
            numaGetIValue(naind, i, &ind);
            medtop = (i % 2 == 0) ? medtope : medtopo;
            medbot = (i % 2 == 0) ? medbote : medboto;
            if (ind == 1 && isvalid) {  /* adjust sides */
                boxGetSideLocations(box, NULL, NULL, &top, &bot);
                if (L_ABS(top - medtop) > maxdel) top = medtop;
                if (L_ABS(bot - medbot) > maxdel) bot = medbot;
                del = (l_int32)(factor * medh - (bot - top)) / 2;  /* typ > 0 */
                boxSetSide(box, L_SET_TOP, L_MAX(0, top - del), 0);
                boxSetSide(box, L_SET_BOT, bot + del, 0);
            }
            boxaAddBox(boxad, box, L_INSERT);
        }
    }
    numaDestroy(&naind);
    boxaDestroy(&boxae);
    boxaDestroy(&boxao);
    return boxad;
}


/*!
 * \brief   boxaPlotSides()
 *
 * \param[in]    boxa       source boxa
 * \param[in]    plotname   [optional], can be NULL
 * \param[out]   pnal       [optional] na of left sides
 * \param[out]   pnat       [optional] na of top sides
 * \param[out]   pnar       [optional] na of right sides
 * \param[out]   pnab       [optional] na of bottom sides
 * \param[out]   ppixd      pix of the output plot
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This debugging function shows the progression of the four
 *          sides in the boxa.  There must be at least 2 boxes.
 *      (2) If there are invalid boxes (e.g., if only even or odd
 *          indices have valid boxes), this will fill them with the
 *          nearest valid box before plotting.
 *      (3) The plotfiles are put in /tmp/lept/plots/, and are named
 *          either with %plotname or, if NULL, a default name.  If
 *          %plotname is used, make sure it has no whitespace characters.
 * </pre>
 */
l_ok
boxaPlotSides(BOXA        *boxa,
              const char  *plotname,
              NUMA       **pnal,
              NUMA       **pnat,
              NUMA       **pnar,
              NUMA       **pnab,
              PIX        **ppixd)
{
char            buf[128], titlebuf[128];
char           *dataname;
static l_int32  plotid = 0;
l_int32         n, i, w, h, left, top, right, bot;
l_int32         debugprint = FALSE;  /* change to TRUE to spam stderr */
l_float32       med, dev;
BOXA           *boxat;
GPLOT          *gplot;
NUMA           *nal, *nat, *nar, *nab;

    if (pnal) *pnal = NULL;
    if (pnat) *pnat = NULL;
    if (pnar) *pnar = NULL;
    if (pnab) *pnab = NULL;
    if (ppixd) *ppixd = NULL;
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if ((n = boxaGetCount(boxa)) < 2)
        return ERROR_INT("less than 2 boxes", __func__, 1);
    if (!ppixd)
        return ERROR_INT("&pixd not defined", __func__, 1);

    boxat = boxaFillSequence(boxa, L_USE_ALL_BOXES, 0);

        /* Build the numas for each side */
    nal = numaCreate(n);
    nat = numaCreate(n);
    nar = numaCreate(n);
    nab = numaCreate(n);

    for (i = 0; i < n; i++) {
        boxaGetBoxGeometry(boxat, i, &left, &top, &w, &h);
        right = left + w - 1;
        bot = top + h - 1;
        numaAddNumber(nal, left);
        numaAddNumber(nat, top);
        numaAddNumber(nar, right);
        numaAddNumber(nab, bot);
    }
    boxaDestroy(&boxat);

    lept_mkdir("lept/plots");
    if (plotname) {
        snprintf(buf, sizeof(buf), "/tmp/lept/plots/sides.%s", plotname);
        snprintf(titlebuf, sizeof(titlebuf), "%s: Box sides vs. box index",
                 plotname);
    } else {
        snprintf(buf, sizeof(buf), "/tmp/lept/plots/sides.%d", plotid++);
        snprintf(titlebuf, sizeof(titlebuf), "Box sides vs. box index");
    }
    gplot = gplotCreate(buf, GPLOT_PNG, titlebuf,
                        "box index", "side location");
    gplotAddPlot(gplot, NULL, nal, GPLOT_LINES, "left side");
    gplotAddPlot(gplot, NULL, nat, GPLOT_LINES, "top side");
    gplotAddPlot(gplot, NULL, nar, GPLOT_LINES, "right side");
    gplotAddPlot(gplot, NULL, nab, GPLOT_LINES, "bottom side");
    *ppixd = gplotMakeOutputPix(gplot);
    gplotDestroy(&gplot);

    if (debugprint) {
        dataname = (plotname) ? stringNew(plotname) : stringNew("no_name");
        numaGetMedian(nal, &med);
        numaGetMeanDevFromMedian(nal, med, &dev);
        lept_stderr("%s left: med = %7.3f, meandev = %7.3f\n",
                    dataname, med, dev);
        numaGetMedian(nat, &med);
        numaGetMeanDevFromMedian(nat, med, &dev);
        lept_stderr("%s top: med = %7.3f, meandev = %7.3f\n",
                    dataname, med, dev);
        numaGetMedian(nar, &med);
        numaGetMeanDevFromMedian(nar, med, &dev);
        lept_stderr("%s right: med = %7.3f, meandev = %7.3f\n",
                    dataname, med, dev);
        numaGetMedian(nab, &med);
        numaGetMeanDevFromMedian(nab, med, &dev);
        lept_stderr("%s bot: med = %7.3f, meandev = %7.3f\n",
                    dataname, med, dev);
        LEPT_FREE(dataname);
    }

    if (pnal)
        *pnal = nal;
    else
        numaDestroy(&nal);
    if (pnat)
        *pnat = nat;
    else
        numaDestroy(&nat);
    if (pnar)
        *pnar = nar;
    else
        numaDestroy(&nar);
    if (pnab)
        *pnab = nab;
    else
        numaDestroy(&nab);
    return 0;
}


/*!
 * \brief   boxaPlotSizes()
 *
 * \param[in]    boxa       source boxa
 * \param[in]    plotname   [optional], can be NULL
 * \param[out]   pnaw       [optional] na of widths
 * \param[out]   pnah       [optional] na of heights
 * \param[out]   ppixd      pix of the output plot
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This debugging function shows the progression of box width
 *          and height in the boxa.  There must be at least 2 boxes.
 *      (2) If there are invalid boxes (e.g., if only even or odd
 *          indices have valid boxes), this will fill them with the
 *          nearest valid box before plotting.
 *      (3) The plotfiles are put in /tmp/lept/plots/, and are named
 *          either with %plotname or, if NULL, a default name.  If
 *          %plotname is used, make sure it has no whitespace characters.
 * </pre>
 */
l_ok
boxaPlotSizes(BOXA        *boxa,
              const char  *plotname,
              NUMA       **pnaw,
              NUMA       **pnah,
              PIX        **ppixd)
{
char            buf[128], titlebuf[128];
static l_int32  plotid = 0;
l_int32         n, i, w, h;
BOXA           *boxat;
GPLOT          *gplot;
NUMA           *naw, *nah;

    if (pnaw) *pnaw = NULL;
    if (pnah) *pnah = NULL;
    if (ppixd) *ppixd = NULL;
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if ((n = boxaGetCount(boxa)) < 2)
        return ERROR_INT("less than 2 boxes", __func__, 1);
    if (!ppixd)
        return ERROR_INT("&pixd not defined", __func__, 1);

    boxat = boxaFillSequence(boxa, L_USE_ALL_BOXES, 0);

        /* Build the numas for the width and height */
    naw = numaCreate(n);
    nah = numaCreate(n);
    for (i = 0; i < n; i++) {
        boxaGetBoxGeometry(boxat, i, NULL, NULL, &w, &h);
        numaAddNumber(naw, w);
        numaAddNumber(nah, h);
    }
    boxaDestroy(&boxat);

    lept_mkdir("lept/plots");
    if (plotname) {
        snprintf(buf, sizeof(buf), "/tmp/lept/plots/size.%s", plotname);
        snprintf(titlebuf, sizeof(titlebuf), "%s: Box size vs. box index",
                 plotname);
    } else {
        snprintf(buf, sizeof(buf), "/tmp/lept/plots/size.%d", plotid++);
        snprintf(titlebuf, sizeof(titlebuf), "Box size vs. box index");
    }
    gplot = gplotCreate(buf, GPLOT_PNG, titlebuf,
                        "box index", "box dimension");
    gplotAddPlot(gplot, NULL, naw, GPLOT_LINES, "width");
    gplotAddPlot(gplot, NULL, nah, GPLOT_LINES, "height");
    *ppixd = gplotMakeOutputPix(gplot);
    gplotDestroy(&gplot);

    if (pnaw)
        *pnaw = naw;
    else
        numaDestroy(&naw);
    if (pnah)
        *pnah = nah;
    else
        numaDestroy(&nah);
    return 0;
}


/*!
 * \brief   boxaFillSequence()
 *
 * \param[in]    boxas      with at least 3 boxes
 * \param[in]    useflag    L_USE_ALL_BOXES, L_USE_SAME_PARITY_BOXES
 * \param[in]    debug      1 for debug output
 * \return  boxad filled boxa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This simple function replaces invalid boxes with a copy of
 *          the nearest valid box, selected from either the entire
 *          sequence (L_USE_ALL_BOXES) or from the boxes with the
 *          same parity (L_USE_SAME_PARITY_BOXES).  It returns a new boxa.
 *      (2) This is useful if you expect boxes in the sequence to
 *          vary slowly with index.
 * </pre>
 */
BOXA *
boxaFillSequence(BOXA    *boxas,
                 l_int32  useflag,
                 l_int32  debug)
{
l_int32  n, nv;
BOXA    *boxae, *boxao, *boxad;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (useflag != L_USE_ALL_BOXES && useflag != L_USE_SAME_PARITY_BOXES)
        return (BOXA *)ERROR_PTR("invalid useflag", __func__, NULL);

    n = boxaGetCount(boxas);
    nv = boxaGetValidCount(boxas);
    if (n == nv)
        return boxaCopy(boxas, L_COPY);  /* all valid */
    if (debug)
        L_INFO("%d valid boxes, %d invalid boxes\n", __func__, nv, n - nv);
    if (useflag == L_USE_SAME_PARITY_BOXES && n < 3) {
        L_WARNING("n < 3; some invalid\n", __func__);
        return boxaCopy(boxas, L_COPY);
    }

    if (useflag == L_USE_ALL_BOXES) {
        boxad = boxaCopy(boxas, L_COPY);
        boxaFillAll(boxad);
    } else {
        boxaSplitEvenOdd(boxas, 0, &boxae, &boxao);
        boxaFillAll(boxae);
        boxaFillAll(boxao);
        boxad = boxaMergeEvenOdd(boxae, boxao, 0);
        boxaDestroy(&boxae);
        boxaDestroy(&boxao);
    }

    nv = boxaGetValidCount(boxad);
    if (n != nv)
        L_WARNING("there are still %d invalid boxes\n", __func__, n - nv);

    return boxad;
}


/*!
 * \brief   boxaFillAll()
 *
 * \param[in]    boxa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This static function replaces every invalid box with the
 *          nearest valid box.  If there are no valid boxes, it
 *          issues a warning.
 * </pre>
 */
static l_int32
boxaFillAll(BOXA  *boxa)
{
l_int32   n, nv, i, j, spandown, spanup;
l_int32  *indic;
BOX      *box, *boxt;

    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    n = boxaGetCount(boxa);
    nv = boxaGetValidCount(boxa);
    if (n == nv) return 0;
    if (nv == 0) {
        L_WARNING("no valid boxes out of %d boxes\n", __func__, n);
        return 0;
    }

        /* Make indicator array for valid boxes */
    if ((indic = (l_int32 *)LEPT_CALLOC(n, sizeof(l_int32))) == NULL)
        return ERROR_INT("indic not made", __func__, 1);
    for (i = 0; i < n; i++) {
        box = boxaGetValidBox(boxa, i, L_CLONE);
        if (box)
            indic[i] = 1;
        boxDestroy(&box);
    }

        /* Replace invalid boxes with the nearest valid one */
    for (i = 0; i < n; i++) {
        box = boxaGetValidBox(boxa, i, L_CLONE);
        if (!box) {
            spandown = spanup = 10000000;
            for (j = i - 1; j >= 0; j--) {
                if (indic[j] == 1) {
                    spandown = i - j;
                    break;
                }
            }
            for (j = i + 1; j < n; j++) {
                if (indic[j] == 1) {
                    spanup = j - i;
                    break;
                }
            }
            if (spandown < spanup)
                boxt = boxaGetBox(boxa, i - spandown, L_COPY);
            else
                boxt = boxaGetBox(boxa, i + spanup, L_COPY);
            boxaReplaceBox(boxa, i, boxt);
        }
        boxDestroy(&box);
    }

    LEPT_FREE(indic);
    return 0;
}


/*!
 * \brief   boxaSizeVariation()
 *
 * \param[in]    boxa           at least 4 boxes
 * \param[in]    type           L_SELECT_WIDTH, L_SELECT_HEIGHT
 * \param[out]   pdel_evenodd   [optional] average absolute value of
 *                              (even - odd) size pairs
 * \param[out]   prms_even      [optional] rms deviation of even boxes
 * \param[out]   prms_odd       [optional] rms deviation of odd boxes
 * \param[out]   prms_all       [optional] rms deviation of all boxes
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This gives several measures of the smoothness of either the
 *          width or height of a sequence of boxes.
 *          See boxaMedianDimensions() for some other measures.
 *      (2) Statistics can be found separately for even and odd boxes.
 *          Additionally, the average pair-wise difference between
 *          adjacent even and odd boxes can be returned.
 *      (3) The use case is bounding boxes for scanned page images,
 *          where ideally the sizes should have little variance.
 * </pre>
 */
l_ok
boxaSizeVariation(BOXA       *boxa,
                  l_int32     type,
                  l_float32  *pdel_evenodd,
                  l_float32  *prms_even,
                  l_float32  *prms_odd,
                  l_float32  *prms_all)
{
l_int32    n, ne, no, nmin, vale, valo, i;
l_float32  sum;
BOXA      *boxae, *boxao;
NUMA      *nae, *nao, *na_all;

    if (pdel_evenodd) *pdel_evenodd = 0.0;
    if (prms_even) *prms_even = 0.0;
    if (prms_odd) *prms_odd = 0.0;
    if (prms_all) *prms_all = 0.0;
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if (type != L_SELECT_WIDTH && type != L_SELECT_HEIGHT)
        return ERROR_INT("invalid type", __func__, 1);
    if (!pdel_evenodd && !prms_even && !prms_odd && !prms_all)
        return ERROR_INT("nothing to do", __func__, 1);
    n = boxaGetCount(boxa);
    if (n < 4)
        return ERROR_INT("too few boxes", __func__, 1);

    boxaSplitEvenOdd(boxa, 0, &boxae, &boxao);
    ne = boxaGetCount(boxae);
    no = boxaGetCount(boxao);
    nmin = L_MIN(ne, no);
    if (nmin == 0) {
        boxaDestroy(&boxae);
        boxaDestroy(&boxao);
        return ERROR_INT("either no even or no odd boxes", __func__, 1);
    }

    if (type == L_SELECT_WIDTH) {
        boxaGetSizes(boxae, &nae, NULL);
        boxaGetSizes(boxao, &nao, NULL);
        boxaGetSizes(boxa, &na_all, NULL);
    } else {   /* L_SELECT_HEIGHT) */
        boxaGetSizes(boxae, NULL, &nae);
        boxaGetSizes(boxao, NULL, &nao);
        boxaGetSizes(boxa, NULL, &na_all);
    }

    if (pdel_evenodd) {
        sum = 0.0;
        for (i = 0; i < nmin; i++) {
            numaGetIValue(nae, i, &vale);
            numaGetIValue(nao, i, &valo);
            sum += L_ABS(vale - valo);
        }
        *pdel_evenodd = sum / nmin;
    }
    if (prms_even)
        numaSimpleStats(nae, 0, -1, NULL, NULL, prms_even);
    if (prms_odd)
        numaSimpleStats(nao, 0, -1, NULL, NULL, prms_odd);
    if (prms_all)
        numaSimpleStats(na_all, 0, -1, NULL, NULL, prms_all);

    boxaDestroy(&boxae);
    boxaDestroy(&boxao);
    numaDestroy(&nae);
    numaDestroy(&nao);
    numaDestroy(&na_all);
    return 0;
}


/*!
 * \brief   boxaMedianDimensions()
 *
 * \param[in]    boxas    containing at least 3 valid boxes in even and odd
 * \param[out]   pmedw    [optional] median width of all boxes
 * \param[out]   pmedh    [optional] median height of all boxes
 * \param[out]   pmedwe   [optional] median width of even boxes
 * \param[out]   pmedwo   [optional] median width of odd boxes
 * \param[out]   pmedhe   [optional] median height of even boxes
 * \param[out]   pmedho   [optional] median height of odd boxes
 * \param[out]   pnadelw  [optional] width diff of each box from median
 * \param[out]   pnadelh  [optional] height diff of each box from median
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This provides information that (1) allows identification of
 *          boxes that have unusual (outlier) width or height, and (2) can
 *          be used to regularize the sizes of the outlier boxes, assuming
 *          that the boxes satisfy a fairly regular sequence and should
 *          mostly have the same width and height.
 *      (2) This finds the median width and height, as well as separate
 *          median widths and heights of even and odd boxes.  It also
 *          generates arrays that give the difference in width and height
 *          of each box from the median, which can be used to correct
 *          individual boxes.
 *      (3) All return values are optional.
 * </pre>
 */
l_ok
boxaMedianDimensions(BOXA     *boxas,
                     l_int32  *pmedw,
                     l_int32  *pmedh,
                     l_int32  *pmedwe,
                     l_int32  *pmedwo,
                     l_int32  *pmedhe,
                     l_int32  *pmedho,
                     NUMA    **pnadelw,
                     NUMA    **pnadelh)
{
l_int32  i, n, bw, bh, medw, medh, medwe, medwo, medhe, medho;
BOXA    *boxae, *boxao;
NUMA    *nadelw, *nadelh;

    if (pmedw) *pmedw = 0;
    if (pmedh) *pmedh = 0;
    if (pmedwe) *pmedwe= 0;
    if (pmedwo) *pmedwo= 0;
    if (pmedhe) *pmedhe= 0;
    if (pmedho) *pmedho= 0;
    if (pnadelw) *pnadelw = NULL;
    if (pnadelh) *pnadelh = NULL;
    if (!boxas)
        return ERROR_INT("boxas not defined", __func__, 1);
    if (boxaGetValidCount(boxas) < 6)
        return ERROR_INT("need at least 6 valid boxes", __func__, 1);

        /* Require at least 3 valid boxes of both types */
    boxaSplitEvenOdd(boxas, 0, &boxae, &boxao);
    if (boxaGetValidCount(boxae) < 3 || boxaGetValidCount(boxao) < 3) {
        boxaDestroy(&boxae);
        boxaDestroy(&boxao);
        return ERROR_INT("don't have 3+ valid boxes of each type", __func__, 1);
    }

        /* Get the relevant median widths and heights */
    boxaGetMedianVals(boxas, NULL, NULL, NULL, NULL, &medw, &medh);
    boxaGetMedianVals(boxae, NULL, NULL, NULL, NULL, &medwe, &medhe);
    boxaGetMedianVals(boxao, NULL, NULL, NULL, NULL, &medwo, &medho);
    if (pmedw) *pmedw = medw;
    if (pmedh) *pmedh = medh;
    if (pmedwe) *pmedwe = medwe;
    if (pmedwo) *pmedwo = medwo;
    if (pmedhe) *pmedhe = medhe;
    if (pmedho) *pmedho = medho;

        /* Find the variation from median dimension for each box */
    n = boxaGetCount(boxas);
    nadelw = numaCreate(n);
    nadelh = numaCreate(n);
    for (i = 0; i < n; i++) {
        boxaGetBoxGeometry(boxas, i, NULL, NULL, &bw, &bh);
        if (bw == 0 || bh == 0) {  /* invalid box */
            numaAddNumber(nadelw, 0);
            numaAddNumber(nadelh, 0);
        } else {
            numaAddNumber(nadelw, bw - medw);
            numaAddNumber(nadelh, bh - medh);
        }
    }
    if (pnadelw)
        *pnadelw = nadelw;
    else
        numaDestroy(&nadelw);
    if (pnadelh)
        *pnadelh = nadelh;
    else
        numaDestroy(&nadelh);

    boxaDestroy(&boxae);
    boxaDestroy(&boxao);
    return 0;
}


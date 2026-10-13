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

/*
 * correlscore.c
 *
 *     These are functions for computing correlation between
 *     pairs of 1 bpp images.
 *
 *     Optimized 2 pix correlators (for jbig2 clustering)
 *         l_int32     pixCorrelationScore()
 *         l_int32     pixCorrelationScoreThresholded()
 *
 *     Simple 2 pix correlators
 *         l_int32     pixCorrelationScoreSimple()
 *         l_int32     pixCorrelationScoreShifted()
 *
 *     There are other, more application-oriented functions, that
 *     compute the correlation between two binary images, taking into
 *     account small translational shifts, between two binary images.
 *     These are:
 *         compare.c:     pixBestCorrelation()
 *                        Uses coarse-to-fine translations of full image
 *         recogident.c:  pixCorrelationBestShift()
 *                        Uses small shifts between c.c. centroids.
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"

/* -------------------------------------------------------------------- *
 *           Optimized 2 pix correlators (for jbig2 clustering)         *
 * -------------------------------------------------------------------- */
/*!
 * \brief   pixCorrelationScore()
 *
 * \param[in]    pix1       test pix, 1 bpp
 * \param[in]    pix2       exemplar pix, 1 bpp
 * \param[in]    area1      number of on pixels in pix1
 * \param[in]    area2      number of on pixels in pix2
 * \param[in]    delx       x comp of centroid difference
 * \param[in]    dely       y comp of centroid difference
 * \param[in]    maxdiffw   max width difference of pix1 and pix2
 * \param[in]    maxdiffh   max height difference of pix1 and pix2
 * \param[in]    tab        sum tab for byte
 * \param[out]   pscore     correlation score
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *  We check first that the two pix are roughly the same size.
 *  For jbclass (jbig2) applications at roughly 300 ppi, maxdiffw and
 *  maxdiffh should be at least 2.
 *
 *  Only if they meet that criterion do we compare the bitmaps.
 *  The centroid difference is used to align the two images to the
 *  nearest integer for the correlation.
 *
 *  The correlation score is the ratio of the square of the number of
 *  pixels in the AND of the two bitmaps to the product of the number
 *  of ON pixels in each.  Denote the number of ON pixels in pix1
 *  by |1|, the number in pix2 by |2|, and the number in the AND
 *  of pix1 and pix2 by |1 & 2|.  The correlation score is then
 *  (|1 & 2|)**2 / (|1|*|2|).
 *
 *  This score is compared with an input threshold, which can
 *  be modified depending on the weight of the template.
 *  The modified threshold is
 *     thresh + (1.0 - thresh) * weight * R
 *  where
 *     weight is a fixed input factor between 0.0 and 1.0
 *     R = |2| / area(2)
 *  and area(2) is the total number of pixels in 2 (i.e., width x height).
 *
 *  To understand why a weight factor is useful, consider what happens
 *  with thick, sans-serif characters that look similar and have a value
 *  of R near 1.  Different characters can have a high correlation value,
 *  and the classifier will make incorrect substitutions.  The weight
 *  factor raises the threshold for these characters.
 *
 *  Yet another approach to reduce such substitutions is to run the classifier
 *  in a non-greedy way, matching to the template with the highest
 *  score, not the first template with a score satisfying the matching
 *  constraint.  However, this is not particularly effective.
 *
 *  The implementation here gives the same result as in
 *  pixCorrelationScoreSimple(), where a temporary Pix is made to hold
 *  the AND and implementation uses rasterop:
 *      pixt = pixCreateTemplate(pix1);
 *      pixRasterop(pixt, idelx, idely, wt, ht, PIX_SRC, pix2, 0, 0);
 *      pixRasterop(pixt, 0, 0, wi, hi, PIX_SRC & PIX_DST, pix1, 0, 0);
 *      pixCountPixels(pixt, &count, tab);
 *      pixDestroy(&pixt);
 *  However, here it is done in a streaming fashion, counting as it goes,
 *  and touching memory exactly once, giving a 3-4x speedup over the
 *  simple implementation.  This very fast correlation matcher was
 *  contributed by William Rucklidge.
 * </pre>
 */
l_ok
pixCorrelationScore(PIX        *pix1,
                    PIX        *pix2,
                    l_int32     area1,
                    l_int32     area2,
                    l_float32   delx,   /* x(1) - x(3) */
                    l_float32   dely,   /* y(1) - y(3) */
                    l_int32     maxdiffw,
                    l_int32     maxdiffh,
                    l_int32    *tab,
                    l_float32  *pscore)
{
l_int32    wi, hi, wt, ht, delw, delh, idelx, idely, count;
l_int32    wpl1, wpl2, lorow, hirow, locol, hicol;
l_int32    x, y, pix1lskip, pix2lskip, rowwords1, rowwords2;
l_uint32   word1, word2, andw;
l_uint32  *row1, *row2;

    if (!pscore)
        return ERROR_INT("&score not defined", __func__, 1);
    *pscore = 0.0;
    if (!pix1 || pixGetDepth(pix1) != 1)
        return ERROR_INT("pix1 undefined or not 1 bpp", __func__, 1);
    if (!pix2 || pixGetDepth(pix2) != 1)
        return ERROR_INT("pix2 undefined or not 1 bpp", __func__, 1);
    if (!tab)
        return ERROR_INT("tab not defined", __func__, 1);
    if (area1 <= 0 || area2 <= 0)
        return ERROR_INT("areas must be > 0", __func__, 1);

        /* Eliminate based on size difference */
    pixGetDimensions(pix1, &wi, &hi, NULL);
    pixGetDimensions(pix2, &wt, &ht, NULL);
    delw = L_ABS(wi - wt);
    if (delw > maxdiffw)
        return 0;
    delh = L_ABS(hi - ht);
    if (delh > maxdiffh)
        return 0;

        /* Round difference to nearest integer */
    if (delx >= 0)
        idelx = (l_int32)(delx + 0.5);
    else
        idelx = (l_int32)(delx - 0.5);
    if (dely >= 0)
        idely = (l_int32)(dely + 0.5);
    else
        idely = (l_int32)(dely - 0.5);

    count = 0;
    wpl1 = pixGetWpl(pix1);
    wpl2 = pixGetWpl(pix2);
    rowwords2 = wpl2;

        /* What rows of pix1 need to be considered?  Only those underlying the
         * shifted pix2. */
    lorow = L_MAX(idely, 0);
    hirow = L_MIN(ht + idely, hi);

        /* Get the pointer to the first row of each image that will be
         * considered. */
    row1 = pixGetData(pix1) + wpl1 * lorow;
    row2 = pixGetData(pix2) + wpl2 * (lorow - idely);

        /* Similarly, figure out which columns of pix1 will be considered. */
    locol = L_MAX(idelx, 0);
    hicol = L_MIN(wt + idelx, wi);

    if (idelx >= 32) {
            /* pix2 is shifted far enough to the right that pix1's first
             * word(s) won't contribute to the count.  Increment its
             * pointer to point to the first word that will contribute,
             * and adjust other values accordingly. */
        pix1lskip = idelx >> 5;  /* # of words to skip on left */
        row1 += pix1lskip;
        locol -= pix1lskip << 5;
        hicol -= pix1lskip << 5;
        idelx &= 31;
    } else if (idelx <= -32) {
            /* pix2 is shifted far enough to the left that its first word(s)
             * won't contribute to the count.  Increment its pointer
             * to point to the first word that will contribute,
             * and adjust other values accordingly. */
        pix2lskip = -((idelx + 31) >> 5);  /* # of words to skip on left */
        row2 += pix2lskip;
        rowwords2 -= pix2lskip;
        idelx += pix2lskip << 5;
    }

    if ((locol >= hicol) || (lorow >= hirow)) {  /* there is no overlap */
        count = 0;
    } else {
            /* How many words of each row of pix1 need to be considered? */
        rowwords1 = (hicol + 31) >> 5;

        if (idelx == 0) {
                /* There's no lateral offset; simple case. */
            for (y = lorow; y < hirow; y++, row1 += wpl1, row2 += wpl2) {
                for (x = 0; x < rowwords1; x++) {
                    andw = row1[x] & row2[x];
                    count += tab[andw & 0xff] +
                        tab[(andw >> 8) & 0xff] +
                        tab[(andw >> 16) & 0xff] +
                        tab[andw >> 24];
                }
            }
        } else if (idelx > 0) {
                /* pix2 is shifted to the right.  word 0 of pix1 is touched by
                 * word 0 of pix2; word 1 of pix1 is touched by word 0 and word
                 * 1 of pix2, and so on up to the last word of pix1 (word N),
                 * which is touched by words N-1 and N of pix1... if there is a
                 * word N.  Handle the two cases (pix2 has N-1 words and pix2
                 * has at least N words) separately.
                 *
                 * Note: we know that pix2 has at least N-1 words (i.e.,
                 * rowwords2 >= rowwords1 - 1) by the following logic.
                 * We can pretend that idelx <= 31 because the >= 32 logic
                 * above adjusted everything appropriately.  Then
                 * hicol <= wt + idelx <= wt + 31, so
                 * hicol + 31 <= wt + 62
                 * rowwords1 = (hicol + 31) >> 5 <= (wt + 62) >> 5
                 * rowwords2 == (wt + 31) >> 5, so
                 * rowwords1 <= rowwords2 + 1 */
            if (rowwords2 < rowwords1) {
                for (y = lorow; y < hirow; y++, row1 += wpl1, row2 += wpl2) {
                        /* Do the first iteration so the loop can be
                         * branch-free. */
                    word1 = row1[0];
                    word2 = row2[0] >> idelx;
                    andw = word1 & word2;
                    count += tab[andw & 0xff] +
                        tab[(andw >> 8) & 0xff] +
                        tab[(andw >> 16) & 0xff] +
                        tab[andw >> 24];

                    for (x = 1; x < rowwords2; x++) {
                        word1 = row1[x];
                        word2 = (row2[x] >> idelx) |
                            (row2[x - 1] << (32 - idelx));
                        andw = word1 & word2;
                        count += tab[andw & 0xff] +
                            tab[(andw >> 8) & 0xff] +
                            tab[(andw >> 16) & 0xff] +
                            tab[andw >> 24];
                    }

                        /* Now the last iteration - we know that this is safe
                         * (i.e.  rowwords1 >= 2) because rowwords1 > rowwords2
                         * > 0 (if it was 0, we'd have taken the "count = 0"
                         * fast-path out of here). */
                    word1 = row1[x];
                    word2 = row2[x - 1] << (32 - idelx);
                    andw = word1 & word2;
                    count += tab[andw & 0xff] +
                        tab[(andw >> 8) & 0xff] +
                        tab[(andw >> 16) & 0xff] +
                        tab[andw >> 24];
                }
            } else {
                for (y = lorow; y < hirow; y++, row1 += wpl1, row2 += wpl2) {
                        /* Do the first iteration so the loop can be
                         * branch-free.  This section is the same as above
                         * except for the different limit on the loop, since
                         * the last iteration is the same as all the other
                         * iterations (beyond the first). */
                    word1 = row1[0];
                    word2 = row2[0] >> idelx;
                    andw = word1 & word2;
                    count += tab[andw & 0xff] +
                        tab[(andw >> 8) & 0xff] +
                        tab[(andw >> 16) & 0xff] +
                        tab[andw >> 24];

                    for (x = 1; x < rowwords1; x++) {
                        word1 = row1[x];
                        word2 = (row2[x] >> idelx) |
                            (row2[x - 1] << (32 - idelx));
                        andw = word1 & word2;
                        count += tab[andw & 0xff] +
                            tab[(andw >> 8) & 0xff] +
                            tab[(andw >> 16) & 0xff] +
                            tab[andw >> 24];
                    }
                }
            }
        } else {
                /* pix2 is shifted to the left.  word 0 of pix1 is touched by
                 * word 0 and word 1 of pix2, and so on up to the last word of
                 * pix1 (word N), which is touched by words N and N+1 of
                 * pix2... if there is a word N+1.  Handle the two cases (pix2
                 * has N words and pix2 has at least N+1 words) separately. */
            if (rowwords1 < rowwords2) {
                    /* pix2 has at least N+1 words, so every iteration through
                     * the loop can be the same. */
                for (y = lorow; y < hirow; y++, row1 += wpl1, row2 += wpl2) {
                    for (x = 0; x < rowwords1; x++) {
                        word1 = row1[x];
                        word2 = row2[x] << -idelx;
                        word2 |= row2[x + 1] >> (32 + idelx);
                        andw = word1 & word2;
                        count += tab[andw & 0xff] +
                            tab[(andw >> 8) & 0xff] +
                            tab[(andw >> 16) & 0xff] +
                            tab[andw >> 24];
                    }
                }
            } else {
                    /* pix2 has only N words, so the last iteration is broken
                     * out. */
                for (y = lorow; y < hirow; y++, row1 += wpl1, row2 += wpl2) {
                    for (x = 0; x < rowwords1 - 1; x++) {
                        word1 = row1[x];
                        word2 = row2[x] << -idelx;
                        word2 |= row2[x + 1] >> (32 + idelx);
                        andw = word1 & word2;
                        count += tab[andw & 0xff] +
                            tab[(andw >> 8) & 0xff] +
                            tab[(andw >> 16) & 0xff] +
                            tab[andw >> 24];
                    }

                    word1 = row1[x];
                    word2 = row2[x] << -idelx;
                    andw = word1 & word2;
                    count += tab[andw & 0xff] +
                        tab[(andw >> 8) & 0xff] +
                        tab[(andw >> 16) & 0xff] +
                        tab[andw >> 24];
                }
            }
        }
    }

    *pscore = (l_float32)count * (l_float32)count /
              ((l_float32)area1 * (l_float32)area2);
/*    lept_stderr("score = %5.3f, count = %d, area1 = %d, area2 = %d\n",
                  *pscore, count, area1, area2); */
    return 0;
}


/*!
 * \brief   pixCorrelationScoreThresholded()
 *
 * \param[in]    pix1         test pix, 1 bpp
 * \param[in]    pix2         exemplar pix, 1 bpp
 * \param[in]    area1        number of on pixels in pix1
 * \param[in]    area2        number of on pixels in pix2
 * \param[in]    delx         x comp of centroid difference
 * \param[in]    dely         y comp of centroid difference
 * \param[in]    maxdiffw     max width difference of pix1 and pix2
 * \param[in]    maxdiffh     max height difference of pix1 and pix2
 * \param[in]    tab          sum tab for byte
 * \param[in]    downcount    count of 1 pixels below each row of pix1
 * \param[in]    score_threshold
 * \return  whether the correlation score is >= score_threshold
 *
 *
 * <pre>
 * Notes:
 *  We check first that the two pix are roughly the same size.
 *  Only if they meet that criterion do we compare the bitmaps.
 *  The centroid difference is used to align the two images to the
 *  nearest integer for the correlation.
 *
 *  The correlation score is the ratio of the square of the number of
 *  pixels in the AND of the two bitmaps to the product of the number
 *  of ON pixels in each.  Denote the number of ON pixels in pix1
 *  by |1|, the number in pix2 by |2|, and the number in the AND
 *  of pix1 and pix2 by |1 & 2|.  The correlation score is then
 *  (|1 & 2|)**2 / (|1|*|2|).
 *
 *  This score is compared with an input threshold, which can
 *  be modified depending on the weight of the template.
 *  The modified threshold is
 *     thresh + (1.0 - thresh) * weight * R
 *  where
 *     weight is a fixed input factor between 0.0 and 1.0
 *     R = |2| / area(2)
 *  and area(2) is the total number of pixels in 2 (i.e., width x height).
 *
 *  To understand why a weight factor is useful, consider what happens
 *  with thick, sans-serif characters that look similar and have a value
 *  of R near 1.  Different characters can have a high correlation value,
 *  and the classifier will make incorrect substitutions.  The weight
 *  factor raises the threshold for these characters.
 *
 *  Yet another approach to reduce such substitutions is to run the classifier
 *  in a non-greedy way, matching to the template with the highest
 *  score, not the first template with a score satisfying the matching
 *  constraint.  However, this is not particularly effective.
 *
 *  This very fast correlation matcher was contributed by William Rucklidge.
 * </pre>
 */
l_int32
pixCorrelationScoreThresholded(PIX       *pix1,
                               PIX       *pix2,
                               l_int32    area1,
                               l_int32    area2,
                               l_float32  delx,   /* x(1) - x(3) */
                               l_float32  dely,   /* y(1) - y(3) */
                               l_int32    maxdiffw,
                               l_int32    maxdiffh,
                               l_int32   *tab,
                               l_int32   *downcount,
                               l_float32  score_threshold)
{
l_int32    wi, hi, wt, ht, delw, delh, idelx, idely, count;
l_int32    wpl1, wpl2, lorow, hirow, locol, hicol, untouchable;
l_int32    x, y, pix1lskip, pix2lskip, rowwords1, rowwords2;
l_uint32   word1, word2, andw;
l_uint32  *row1, *row2;
l_float32  score;
l_int32    threshold;

    if (!pix1 || pixGetDepth(pix1) != 1)
        return ERROR_INT("pix1 undefined or not 1 bpp", __func__, 0);
    if (!pix2 || pixGetDepth(pix2) != 1)
        return ERROR_INT("pix2 undefined or not 1 bpp", __func__, 0);
    if (!tab)
        return ERROR_INT("tab not defined", __func__, 0);
    if (area1 <= 0 || area2 <= 0)
        return ERROR_INT("areas must be > 0", __func__, 0);

        /* Eliminate based on size difference */
    pixGetDimensions(pix1, &wi, &hi, NULL);
    pixGetDimensions(pix2, &wt, &ht, NULL);
    delw = L_ABS(wi - wt);
    if (delw > maxdiffw)
        return FALSE;
    delh = L_ABS(hi - ht);
    if (delh > maxdiffh)
        return FALSE;

        /* Round difference to nearest integer */
    if (delx >= 0)
        idelx = (l_int32)(delx + 0.5);
    else
        idelx = (l_int32)(delx - 0.5);
    if (dely >= 0)
        idely = (l_int32)(dely + 0.5);
    else
        idely = (l_int32)(dely - 0.5);

        /* Compute the correlation count that is needed so that
         * count * count / (area1 * area2) >= score_threshold */
    threshold = (l_int32)ceil(sqrt((l_float64)score_threshold * area1 * area2));

    count = 0;
    wpl1 = pixGetWpl(pix1);
    wpl2 = pixGetWpl(pix2);
    rowwords2 = wpl2;

        /* What rows of pix1 need to be considered?  Only those underlying the
         * shifted pix2. */
    lorow = L_MAX(idely, 0);
    hirow = L_MIN(ht + idely, hi);

        /* Get the pointer to the first row of each image that will be
         * considered. */
    row1 = pixGetData(pix1) + wpl1 * lorow;
    row2 = pixGetData(pix2) + wpl2 * (lorow - idely);
    if (hirow <= hi) {
            /* Some rows of pix1 will never contribute to count */
        untouchable = downcount[hirow - 1];
    }

        /* Similarly, figure out which columns of pix1 will be considered. */
    locol = L_MAX(idelx, 0);
    hicol = L_MIN(wt + idelx, wi);

    if (idelx >= 32) {
            /* pix2 is shifted far enough to the right that pix1's first
             * word(s) won't contribute to the count.  Increment its
             * pointer to point to the first word that will contribute,
             * and adjust other values accordingly. */
        pix1lskip = idelx >> 5;  /* # of words to skip on left */
        row1 += pix1lskip;
        locol -= pix1lskip << 5;
        hicol -= pix1lskip << 5;
        idelx &= 31;
    } else if (idelx <= -32) {
            /* pix2 is shifted far enough to the left that its first word(s)
             * won't contribute to the count.  Increment its pointer
             * to point to the first word that will contribute,
             * and adjust other values accordingly. */
        pix2lskip = -((idelx + 31) >> 5);  /* # of words to skip on left */
        row2 += pix2lskip;
        rowwords2 -= pix2lskip;
        idelx += pix2lskip << 5;
    }

    if ((locol >= hicol) || (lorow >= hirow)) {  /* there is no overlap */
        count = 0;
    } else {
            /* How many words of each row of pix1 need to be considered? */
        rowwords1 = (hicol + 31) >> 5;

        if (idelx == 0) {
                /* There's no lateral offset; simple case. */
            for (y = lorow; y < hirow; y++, row1 += wpl1, row2 += wpl2) {
                for (x = 0; x < rowwords1; x++) {
                    andw = row1[x] & row2[x];
                    count += tab[andw & 0xff] +
                        tab[(andw >> 8) & 0xff] +
                        tab[(andw >> 16) & 0xff] +
                        tab[andw >> 24];
                }

                    /* If the count is over the threshold, no need to
                     * calculate any further.  Likewise, return early if the
                     * count plus the maximum count attainable from further
                     * rows is below the threshold. */
                if (count >= threshold) return TRUE;
                if (count + downcount[y] - untouchable < threshold) {
                    return FALSE;
                }
            }
        } else if (idelx > 0) {
                /* pix2 is shifted to the right.  word 0 of pix1 is touched by
                 * word 0 of pix2; word 1 of pix1 is touched by word 0 and word
                 * 1 of pix2, and so on up to the last word of pix1 (word N),
                 * which is touched by words N-1 and N of pix1... if there is a
                 * word N.  Handle the two cases (pix2 has N-1 words and pix2
                 * has at least N words) separately.
                 *
                 * Note: we know that pix2 has at least N-1 words (i.e.,
                 * rowwords2 >= rowwords1 - 1) by the following logic.
                 * We can pretend that idelx <= 31 because the >= 32 logic
                 * above adjusted everything appropriately.  Then
                 * hicol <= wt + idelx <= wt + 31, so
                 * hicol + 31 <= wt + 62
                 * rowwords1 = (hicol + 31) >> 5 <= (wt + 62) >> 5
                 * rowwords2 == (wt + 31) >> 5, so
                 * rowwords1 <= rowwords2 + 1 */
            if (rowwords2 < rowwords1) {
                for (y = lorow; y < hirow; y++, row1 += wpl1, row2 += wpl2) {
                        /* Do the first iteration so the loop can be
                         * branch-free. */
                    word1 = row1[0];
                    word2 = row2[0] >> idelx;
                    andw = word1 & word2;
                    count += tab[andw & 0xff] +
                        tab[(andw >> 8) & 0xff] +
                        tab[(andw >> 16) & 0xff] +
                        tab[andw >> 24];

                    for (x = 1; x < rowwords2; x++) {
                        word1 = row1[x];
                        word2 = (row2[x] >> idelx) |
                            (row2[x - 1] << (32 - idelx));
                        andw = word1 & word2;
                        count += tab[andw & 0xff] +
                            tab[(andw >> 8) & 0xff] +
                            tab[(andw >> 16) & 0xff] +
                            tab[andw >> 24];
                    }

                        /* Now the last iteration - we know that this is safe
                         * (i.e.  rowwords1 >= 2) because rowwords1 > rowwords2
                         * > 0 (if it was 0, we'd have taken the "count = 0"
                         * fast-path out of here). */
                    word1 = row1[x];
                    word2 = row2[x - 1] << (32 - idelx);
                    andw = word1 & word2;
                    count += tab[andw & 0xff] +
                        tab[(andw >> 8) & 0xff] +
                        tab[(andw >> 16) & 0xff] +
                        tab[andw >> 24];

                    if (count >= threshold) return TRUE;
                    if (count + downcount[y] - untouchable < threshold) {
                        return FALSE;
                    }
                }
            } else {
                for (y = lorow; y < hirow; y++, row1 += wpl1, row2 += wpl2) {
                        /* Do the first iteration so the loop can be
                         * branch-free.  This section is the same as above
                         * except for the different limit on the loop, since
                         * the last iteration is the same as all the other
                         * iterations (beyond the first). */
                    word1 = row1[0];
                    word2 = row2[0] >> idelx;
                    andw = word1 & word2;
                    count += tab[andw & 0xff] +
                        tab[(andw >> 8) & 0xff] +
                        tab[(andw >> 16) & 0xff] +
                        tab[andw >> 24];

                    for (x = 1; x < rowwords1; x++) {
                        word1 = row1[x];
                        word2 = (row2[x] >> idelx) |
                            (row2[x - 1] << (32 - idelx));
                        andw = word1 & word2;
                        count += tab[andw & 0xff] +
                            tab[(andw >> 8) & 0xff] +
                            tab[(andw >> 16) & 0xff] +
                            tab[andw >> 24];
                    }

                    if (count >= threshold) return TRUE;
                    if (count + downcount[y] - untouchable < threshold) {
                        return FALSE;
                    }
                }
            }
        } else {
                /* pix2 is shifted to the left.  word 0 of pix1 is touched by
                 * word 0 and word 1 of pix2, and so on up to the last word of
                 * pix1 (word N), which is touched by words N and N+1 of
                 * pix2... if there is a word N+1.  Handle the two cases (pix2
                 * has N words and pix2 has at least N+1 words) separately. */
            if (rowwords1 < rowwords2) {
                    /* pix2 has at least N+1 words, so every iteration through
                     * the loop can be the same. */
                for (y = lorow; y < hirow; y++, row1 += wpl1, row2 += wpl2) {
                    for (x = 0; x < rowwords1; x++) {
                        word1 = row1[x];
                        word2 = row2[x] << -idelx;
                        word2 |= row2[x + 1] >> (32 + idelx);
                        andw = word1 & word2;
                        count += tab[andw & 0xff] +
                            tab[(andw >> 8) & 0xff] +
                            tab[(andw >> 16) & 0xff] +
                            tab[andw >> 24];
                    }

                    if (count >= threshold) return TRUE;
                    if (count + downcount[y] - untouchable < threshold) {
                        return FALSE;
                    }
                }
            } else {
                    /* pix2 has only N words, so the last iteration is broken
                     * out. */
                for (y = lorow; y < hirow; y++, row1 += wpl1, row2 += wpl2) {
                    for (x = 0; x < rowwords1 - 1; x++) {
                        word1 = row1[x];
                        word2 = row2[x] << -idelx;
                        word2 |= row2[x + 1] >> (32 + idelx);
                        andw = word1 & word2;
                        count += tab[andw & 0xff] +
                            tab[(andw >> 8) & 0xff] +
                            tab[(andw >> 16) & 0xff] +
                            tab[andw >> 24];
                    }

                    word1 = row1[x];
                    word2 = row2[x] << -idelx;
                    andw = word1 & word2;
                    count += tab[andw & 0xff] +
                        tab[(andw >> 8) & 0xff] +
                        tab[(andw >> 16) & 0xff] +
                        tab[andw >> 24];

                    if (count >= threshold) return TRUE;
                    if (count + downcount[y] - untouchable < threshold) {
                        return FALSE;
                    }
                }
            }
        }
    }

    score = (l_float32)count * (l_float32)count /
             ((l_float32)area1 * (l_float32)area2);
    if (score >= score_threshold) {
        lept_stderr(
            "count %d < threshold %d but score %g >= score_threshold %g\n",
            count, threshold, score, score_threshold);
    }
    return FALSE;
}


/* -------------------------------------------------------------------- *
 *             Simple 2 pix correlators (for jbig2 clustering)          *
 * -------------------------------------------------------------------- */
/*!
 * \brief   pixCorrelationScoreSimple()
 *
 * \param[in]    pix1       test pix, 1 bpp
 * \param[in]    pix2       exemplar pix, 1 bpp
 * \param[in]    area1      number of on pixels in pix1
 * \param[in]    area2      number of on pixels in pix2
 * \param[in]    delx       x comp of centroid difference
 * \param[in]    dely       y comp of centroid difference
 * \param[in]    maxdiffw   max width difference of pix1 and pix2
 * \param[in]    maxdiffh   max height difference of pix1 and pix2
 * \param[in]    tab        sum tab for byte
 * \param[out]   pscore     correlation score, in range [0.0 ... 1.0]
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This calculates exactly the same value as pixCorrelationScore().
 *          It is 2-3x slower, but much simpler to understand.
 *      (2) The returned correlation score is 0.0 if the width or height
 *          exceed %maxdiffw or %maxdiffh.
 * </pre>
 */
l_ok
pixCorrelationScoreSimple(PIX        *pix1,
                          PIX        *pix2,
                          l_int32     area1,
                          l_int32     area2,
                          l_float32   delx,   /* x(1) - x(3) */
                          l_float32   dely,   /* y(1) - y(3) */
                          l_int32     maxdiffw,
                          l_int32     maxdiffh,
                          l_int32    *tab,
                          l_float32  *pscore)
{
l_int32  wi, hi, wt, ht, delw, delh, idelx, idely, count;
PIX     *pixt;

    if (!pscore)
        return ERROR_INT("&score not defined", __func__, 1);
    *pscore = 0.0;
    if (!pix1 || pixGetDepth(pix1) != 1)
        return ERROR_INT("pix1 undefined or not 1 bpp", __func__, 1);
    if (!pix2 || pixGetDepth(pix2) != 1)
        return ERROR_INT("pix2 undefined or not 1 bpp", __func__, 1);
    if (!tab)
        return ERROR_INT("tab not defined", __func__, 1);
    if (!area1 || !area2)
        return ERROR_INT("areas must be > 0", __func__, 1);

        /* Eliminate based on size difference */
    pixGetDimensions(pix1, &wi, &hi, NULL);
    pixGetDimensions(pix2, &wt, &ht, NULL);
    delw = L_ABS(wi - wt);
    if (delw > maxdiffw)
        return 0;
    delh = L_ABS(hi - ht);
    if (delh > maxdiffh)
        return 0;

        /* Round difference to nearest integer */
    if (delx >= 0)
        idelx = (l_int32)(delx + 0.5);
    else
        idelx = (l_int32)(delx - 0.5);
    if (dely >= 0)
        idely = (l_int32)(dely + 0.5);
    else
        idely = (l_int32)(dely - 0.5);

        /*  pixt = pixAnd(NULL, pix1, pix2), including shift.
         *  To insure that pixels are ON only within the
         *  intersection of pix1 and the shifted pix2:
         *  (1) Start with pixt cleared and equal in size to pix1.
         *  (2) Blit the shifted pix2 onto pixt.  Then all ON pixels
         *      are within the intersection of pix1 and the shifted pix2.
         *  (3) AND pix1 with pixt. */
    pixt = pixCreateTemplate(pix1);
    pixRasterop(pixt, idelx, idely, wt, ht, PIX_SRC, pix2, 0, 0);
    pixRasterop(pixt, 0, 0, wi, hi, PIX_SRC & PIX_DST, pix1, 0, 0);
    pixCountPixels(pixt, &count, tab);
    pixDestroy(&pixt);

    *pscore = (l_float32)count * (l_float32)count /
               ((l_float32)area1 * (l_float32)area2);
/*    lept_stderr("score = %5.3f, count = %d, area1 = %d, area2 = %d\n",
                  *pscore, count, area1, area2); */
    return 0;
}


/*!
 * \brief   pixCorrelationScoreShifted()
 *
 * \param[in]    pix1     1 bpp
 * \param[in]    pix2     1 bpp
 * \param[in]    area1    number of on pixels in pix1
 * \param[in]    area2    number of on pixels in pix2
 * \param[in]    delx x   translation of pix2 relative to pix1
 * \param[in]    dely y   translation of pix2 relative to pix1
 * \param[in]    tab      sum tab for byte
 * \param[out]   pscore   correlation score
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This finds the correlation between two 1 bpp images,
 *          when pix2 is shifted by (delx, dely) with respect
 *          to each other.
 *      (2) This is implemented by starting with a copy of pix1 and
 *          ANDing its pixels with those of a shifted pix2.
 *      (3) Get the pixel counts for area1 and area2 using piCountPixels().
 *      (4) A good estimate for a shift that would maximize the correlation
 *          is to align the centroids (cx1, cy1; cx2, cy2), giving the
 *          relative translations etransx and etransy:
 *             etransx = cx1 - cx2
 *             etransy = cy1 - cy2
 *          Typically delx is chosen to be near etransx; ditto for dely.
 *          This function is used in pixBestCorrelation(), where the
 *          translations delx and dely are varied to find the best alignment.
 *      (5) We do not check the sizes of pix1 and pix2, because they should
 *          be comparable.
 * </pre>
 */
l_ok
pixCorrelationScoreShifted(PIX        *pix1,
                           PIX        *pix2,
                           l_int32     area1,
                           l_int32     area2,
                           l_int32     delx,
                           l_int32     dely,
                           l_int32    *tab,
                           l_float32  *pscore)
{
l_int32  w1, h1, w2, h2, count;
PIX     *pixt;

    if (!pscore)
        return ERROR_INT("&score not defined", __func__, 1);
    *pscore = 0.0;
    if (!pix1 || pixGetDepth(pix1) != 1)
        return ERROR_INT("pix1 undefined or not 1 bpp", __func__, 1);
    if (!pix2 || pixGetDepth(pix2) != 1)
        return ERROR_INT("pix2 undefined or not 1 bpp", __func__, 1);
    if (!tab)
        return ERROR_INT("tab not defined", __func__, 1);
    if (!area1 || !area2)
        return ERROR_INT("areas must be > 0", __func__, 1);

    pixGetDimensions(pix1, &w1, &h1, NULL);
    pixGetDimensions(pix2, &w2, &h2, NULL);

        /*  To insure that pixels are ON only within the
         *  intersection of pix1 and the shifted pix2:
         *  (1) Start with pixt cleared and equal in size to pix1.
         *  (2) Blit the shifted pix2 onto pixt.  Then all ON pixels
         *      are within the intersection of pix1 and the shifted pix2.
         *  (3) AND pix1 with pixt. */
    pixt = pixCreateTemplate(pix1);
    pixRasterop(pixt, delx, dely, w2, h2, PIX_SRC, pix2, 0, 0);
    pixRasterop(pixt, 0, 0, w1, h1, PIX_SRC & PIX_DST, pix1, 0, 0);
    pixCountPixels(pixt, &count, tab);
    pixDestroy(&pixt);

    *pscore = (l_float32)count * (l_float32)count /
               ((l_float32)area1 * (l_float32)area2);
    return 0;
}

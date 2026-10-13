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
 * \file rank.c
 * <pre>
 *
 *      Rank filter (gray and rgb)
 *          PIX      *pixRankFilter()
 *          PIX      *pixRankFilterRGB()
 *          PIX      *pixRankFilterGray()
 *
 *      Median filter
 *          PIX      *pixMedianFilter()
 *
 *      Rank filter (accelerated with downscaling)
 *          PIX      *pixRankFilterWithScaling()
 *
 *  What is a brick rank filter?
 *
 *    A brick rank order filter evaluates, for every pixel in the image,
 *    a rectangular set of n = wf x hf pixels in its neighborhood (where the
 *    pixel in question is at the "center" of the rectangle and is
 *    included in the evaluation).  It determines the value of the
 *    neighboring pixel that is the r-th smallest in the set,
 *    where r is some integer between 1 and n.  The input rank parameter
 *    is a fraction between 0.0 and 1.0, where 0.0 represents the
 *    smallest value (r = 1) and 1.0 represents the largest value (r = n).
 *    A median filter is a rank filter where rank = 0.5.
 *
 *    It is important to note that grayscale erosion is equivalent
 *    to rank = 0.0, and grayscale dilation is equivalent to rank = 1.0.
 *    These are much easier to calculate than the general rank value,
 *    thanks to the van Herk/Gil-Werman algorithm:
 *       http://www.leptonica.com/grayscale-morphology.html
 *    so you should use pixErodeGray() and pixDilateGray() for
 *    rank 0.0 and 1.0, rsp.  See notes below in the function header.
 *
 *  How is a rank filter implemented efficiently on an image?
 *
 *    Sorting will not work.
 *
 *      * The best sort algorithms are O(n*logn), where n is the number
 *        of values to be sorted (the area of the filter).  For large
 *        filters this is an impractically large number.
 *
 *      * Selection of the rank value is O(n).  (To understand why it's not
 *        O(n*logn), see Numerical Recipes in C, 2nd edition, 1992,  p. 355ff).
 *        This also still far too much computation for large filters.
 *
 *      * Suppose we get clever.  We really only need to do an incremental
 *        selection or sorting, because, for example, moving the filter
 *        down by one pixel causes one filter width of pixels to be added
 *        and another to be removed.  Can we do this incrementally in
 *        an efficient way?  Unfortunately, no.  The sorted values will be
 *        in an array.  Even if the filter width is 1, we can expect to
 *        have to move O(n) pixels, because insertion and deletion can happen
 *        anywhere in the array.  By comparison, heapsort is excellent for
 *        incremental sorting, where the cost for insertion or deletion
 *        is O(logn), because the array itself doesn't need to
 *        be sorted into strictly increasing order.  However, heapsort
 *        only gives the max (or min) value, not the general rank value.
 *
 *    This leaves histograms.
 *
 *      * Represented as an array.  The problem with an array of 256
 *        bins is that, in general, a significant fraction of the
 *        entire histogram must be summed to find the rank value bin.
 *        Suppose the filter size is 5x5.  You spend most of your time
 *        adding zeroes.  Ouch!
 *
 *      * Represented as a linked list.  This would overcome the
 *        summing-over-empty-bin problem, but you lose random access
 *        for insertions and deletions.  No way.
 *
 *      * Two histogram solution.  Maintain two histograms with
 *        bin sizes of 1 and 16.  Proceed from coarse to fine.
 *        First locate the coarse bin for the given rank, of which
 *        there are only 16.  Then, in the 256 entry (fine) histogram,
 *        you need look at a maximum of 16 bins.  For each output
 *        pixel, the average number of bins summed over, both in the
 *        coarse and fine histograms, is thus 16.
 *
 *  If someone has a better method, please let me know!
 *
 *  The rank filtering operation is relatively expensive, compared to most
 *  of the other imaging operations.  The speed is only weakly dependent
 *  on the size of the rank filter.  On standard hardware, it runs at
 *  about 10 Mpix/sec for a 50 x 50 filter, and 25 Mpix/sec for
 *  a 5 x 5 filter.   For applications where the rank filter can be
 *  performed on a downscaled image, significant speedup can be
 *  achieved because the time goes as the square of the scaling factor.
 *  We provide an interface that handles the details, and only
 *  requires the amount of downscaling to be input.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

/*----------------------------------------------------------------------*
 *                           Rank order filter                          *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixRankFilter()
 *
 * \param[in]    pixs     8 or 32 bpp; no colormap
 * \param[in]    wf, hf   width and height of filter; each is >= 1
 * \param[in]    rank     in [0.0 ... 1.0]
 * \return  pixd of rank values, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This defines, for each pixel in pixs, a neighborhood of
 *          pixels given by a rectangle "centered" on the pixel.
 *          This set of wf*hf pixels has a distribution of values.
 *          For each component, if the values are sorted in increasing
 *          order, we choose the component such that rank*(wf*hf-1)
 *          pixels have a lower or equal value and
 *          (1-rank)*(wf*hf-1) pixels have an equal or greater value.
 *      (2) See notes in pixRankFilterGray() for further details.
 * </pre>
 */
PIX  *
pixRankFilter(PIX       *pixs,
              l_int32    wf,
              l_int32    hf,
              l_float32  rank)
{
l_int32  d;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetColormap(pixs) != NULL)
        return (PIX *)ERROR_PTR("pixs has colormap", __func__, NULL);
    d = pixGetDepth(pixs);
    if (d != 8 && d != 32)
        return (PIX *)ERROR_PTR("pixs not 8 or 32 bpp", __func__, NULL);
    if (wf < 1 || hf < 1)
        return (PIX *)ERROR_PTR("wf < 1 || hf < 1", __func__, NULL);
    if (rank < 0.0 || rank > 1.0)
        return (PIX *)ERROR_PTR("rank must be in [0.0, 1.0]", __func__, NULL);
    if (wf == 1 && hf == 1)   /* no-op */
        return pixCopy(NULL, pixs);

    if (d == 8)
        return pixRankFilterGray(pixs, wf, hf, rank);
    else  /* d == 32 */
        return pixRankFilterRGB(pixs, wf, hf, rank);
}


/*!
 * \brief   pixRankFilterRGB()
 *
 * \param[in]    pixs     32 bpp
 * \param[in]    wf, hf   width and height of filter; each is >= 1
 * \param[in]    rank     in [0.0 ... 1.0]
 * \return  pixd of rank values, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This defines, for each pixel in pixs, a neighborhood of
 *          pixels given by a rectangle "centered" on the pixel.
 *          This set of wf*hf pixels has a distribution of values.
 *          For each component, if the values are sorted in increasing
 *          order, we choose the component such that rank*(wf*hf-1)
 *          pixels have a lower or equal value and
 *          (1-rank)*(wf*hf-1) pixels have an equal or greater value.
 *      (2) Apply gray rank filtering to each component independently.
 *      (3) See notes in pixRankFilterGray() for further details.
 * </pre>
 */
PIX  *
pixRankFilterRGB(PIX       *pixs,
                 l_int32    wf,
                 l_int32    hf,
                 l_float32  rank)
{
PIX  *pixr, *pixg, *pixb, *pixrf, *pixgf, *pixbf, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("pixs not 32 bpp", __func__, NULL);
    if (wf < 1 || hf < 1)
        return (PIX *)ERROR_PTR("wf < 1 || hf < 1", __func__, NULL);
    if (rank < 0.0 || rank > 1.0)
        return (PIX *)ERROR_PTR("rank must be in [0.0, 1.0]", __func__, NULL);
    if (wf == 1 && hf == 1)   /* no-op */
        return pixCopy(NULL, pixs);

    pixr = pixGetRGBComponent(pixs, COLOR_RED);
    pixg = pixGetRGBComponent(pixs, COLOR_GREEN);
    pixb = pixGetRGBComponent(pixs, COLOR_BLUE);

    pixrf = pixRankFilterGray(pixr, wf, hf, rank);
    pixgf = pixRankFilterGray(pixg, wf, hf, rank);
    pixbf = pixRankFilterGray(pixb, wf, hf, rank);

    pixd = pixCreateRGBImage(pixrf, pixgf, pixbf);
    pixDestroy(&pixr);
    pixDestroy(&pixg);
    pixDestroy(&pixb);
    pixDestroy(&pixrf);
    pixDestroy(&pixgf);
    pixDestroy(&pixbf);
    return pixd;
}


/*!
 * \brief   pixRankFilterGray()
 *
 * \param[in]    pixs     8 bpp; no colormap
 * \param[in]    wf, hf   width and height of filter; each is >= 1
 * \param[in]    rank     in [0.0 ... 1.0]
 * \return  pixd of rank values, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This defines, for each pixel in pixs, a neighborhood of
 *          pixels given by a rectangle "centered" on the pixel.
 *          This set of wf*hf pixels has a distribution of values,
 *          and if they are sorted in increasing order, we choose
 *          the pixel such that rank*(wf*hf-1) pixels have a lower
 *          or equal value and (1-rank)*(wf*hf-1) pixels have an equal
 *          or greater value.
 *      (2) By this definition, the rank = 0.0 pixel has the lowest
 *          value, and the rank = 1.0 pixel has the highest value.
 *      (3) We add mirrored boundary pixels to avoid boundary effects,
 *          and put the filter center at (0, 0).
 *      (4) This dispatches to grayscale erosion or dilation if the
 *          filter dimensions are odd and the rank is 0.0 or 1.0, rsp.
 *      (5) Returns a copy if both wf and hf are 1.
 *      (6) Uses row-major or column-major incremental updates to the
 *          histograms depending on whether hf > wf or hv <= wf, rsp.
 * </pre>
 */
PIX  *
pixRankFilterGray(PIX       *pixs,
                  l_int32    wf,
                  l_int32    hf,
                  l_float32  rank)
{
l_int32    w, h, d, i, j, k, m, n, rankloc, wplt, wpld, val, sum;
l_int32   *histo, *histo16;
l_uint32  *datat, *linet, *datad, *lined;
PIX       *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetColormap(pixs) != NULL)
        return (PIX *)ERROR_PTR("pixs has colormap", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (wf < 1 || hf < 1)
        return (PIX *)ERROR_PTR("wf < 1 || hf < 1", __func__, NULL);
    if (rank < 0.0 || rank > 1.0)
        return (PIX *)ERROR_PTR("rank must be in [0.0, 1.0]", __func__, NULL);
    if (wf == 1 && hf == 1)   /* no-op */
        return pixCopy(NULL, pixs);

        /* For rank = 0.0, this is a grayscale erosion, and for rank = 1.0,
         * a dilation.  Grayscale morphology operations are implemented
         * for filters of odd dimension, so we dispatch to grayscale
         * morphology if both wf and hf are odd.  Otherwise, we
         * slightly adjust the rank (to get the correct behavior) and
         * use the slower rank filter here. */
    if (wf % 2 && hf % 2) {
        if (rank == 0.0)
            return pixErodeGray(pixs, wf, hf);
        else if (rank == 1.0)
            return pixDilateGray(pixs, wf, hf);
    }
    if (rank == 0.0) rank = 0.0001f;
    if (rank == 1.0) rank = 0.9999f;

        /* Add wf/2 to each side, and hf/2 to top and bottom of the
         * image, mirroring for accuracy and to avoid special-casing
         * the boundary. */
    if ((pixt = pixAddMirroredBorder(pixs, wf / 2, wf / 2, hf / 2, hf / 2))
        == NULL)
        return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);

        /* Set up the two histogram arrays. */
    histo = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    histo16 = (l_int32 *)LEPT_CALLOC(16, sizeof(l_int32));
    rankloc = (l_int32)(rank * wf * hf);

        /* Place the filter center at (0, 0).  This is just a
         * convenient location, because it allows us to perform
         * the rank filter over x:(0 ... w - 1) and y:(0 ... h - 1). */
    pixd = pixCreateTemplate(pixs);
    datat = pixGetData(pixt);
    wplt = pixGetWpl(pixt);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

        /* If hf > wf, it's more efficient to use row-major scanning.
         * Otherwise, traverse the image in use column-major order.  */
    if (hf > wf) {
        for (j = 0; j < w; j++) {  /* row-major */
                /* Start each column with clean histogram arrays. */
            for (n = 0; n < 256; n++)
                histo[n] = 0;
            for (n = 0; n < 16; n++)
                histo16[n] = 0;

            for (i = 0; i < h; i++) {  /* fast scan on columns */
                    /* Update the histos for the new location */
                lined = datad + i * wpld;
                if (i == 0) {  /* do full histo */
                    for (k = 0; k < hf; k++) {
                        linet = datat + (i + k) * wplt;
                        for (m = 0; m < wf; m++) {
                            val = GET_DATA_BYTE(linet, j + m);
                            histo[val]++;
                            histo16[val >> 4]++;
                        }
                    }
                } else {  /* incremental update */
                    linet = datat + (i - 1) * wplt;
                    for (m = 0; m < wf; m++) {  /* remove top line */
                        val = GET_DATA_BYTE(linet, j + m);
                        histo[val]--;
                        histo16[val >> 4]--;
                    }
                    linet = datat + (i + hf -  1) * wplt;
                    for (m = 0; m < wf; m++) {  /* add bottom line */
                        val = GET_DATA_BYTE(linet, j + m);
                        histo[val]++;
                        histo16[val >> 4]++;
                    }
                }

                    /* Find the rank value */
                sum = 0;
                for (n = 0; n < 16; n++) {  /* search over coarse histo */
                    sum += histo16[n];
                    if (sum > rankloc) {
                        sum -= histo16[n];
                        break;
                    }
                }
                if (n == 16) {  /* avoid accessing out of bounds */
                    L_WARNING("n = 16; reducing\n", __func__);
                    n = 15;
                    sum -= histo16[n];
                }
                k = 16 * n;  /* starting value in fine histo */
                for (m = 0; m < 16; m++) {
                    sum += histo[k];
                    if (sum > rankloc) {
                        SET_DATA_BYTE(lined, j, k);
                        break;
                    }
                    k++;
                }
            }
        }
    } else {  /* wf >= hf */
        for (i = 0; i < h; i++) {  /* column-major */
                /* Start each row with clean histogram arrays. */
            for (n = 0; n < 256; n++)
                histo[n] = 0;
            for (n = 0; n < 16; n++)
                histo16[n] = 0;
            lined = datad + i * wpld;
            for (j = 0; j < w; j++) {  /* fast scan on rows */
                    /* Update the histos for the new location */
                if (j == 0) {  /* do full histo */
                    for (k = 0; k < hf; k++) {
                        linet = datat + (i + k) * wplt;
                        for (m = 0; m < wf; m++) {
                            val = GET_DATA_BYTE(linet, j + m);
                            histo[val]++;
                            histo16[val >> 4]++;
                        }
                    }
                } else {  /* incremental update at left and right sides */
                    for (k = 0; k < hf; k++) {
                        linet = datat + (i + k) * wplt;
                        val = GET_DATA_BYTE(linet, j - 1);
                        histo[val]--;
                        histo16[val >> 4]--;
                        val = GET_DATA_BYTE(linet, j + wf - 1);
                        histo[val]++;
                        histo16[val >> 4]++;
                    }
                }

                    /* Find the rank value */
                sum = 0;
                for (n = 0; n < 16; n++) {  /* search over coarse histo */
                    sum += histo16[n];
                    if (sum > rankloc) {
                        sum -= histo16[n];
                        break;
                    }
                }
                if (n == 16) {  /* avoid accessing out of bounds */
                    L_WARNING("n = 16; reducing\n", __func__);
                    n = 15;
                    sum -= histo16[n];
                }
                k = 16 * n;  /* starting value in fine histo */
                for (m = 0; m < 16; m++) {
                    sum += histo[k];
                    if (sum > rankloc) {
                        SET_DATA_BYTE(lined, j, k);
                        break;
                    }
                    k++;
                }
            }
        }
    }

    pixDestroy(&pixt);
    LEPT_FREE(histo);
    LEPT_FREE(histo16);
    return pixd;
}


/*----------------------------------------------------------------------*
 *                             Median filter                            *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixMedianFilter()
 *
 * \param[in]    pixs     8 or 32 bpp; no colormap
 * \param[in]    wf, hf   width and height of filter; each is >= 1
 * \return  pixd of median values, or NULL on error
 */
PIX  *
pixMedianFilter(PIX     *pixs,
                l_int32  wf,
                l_int32  hf)
{
    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    return pixRankFilter(pixs, wf, hf, 0.5);
}


/*----------------------------------------------------------------------*
 *                Rank filter (accelerated with downscaling)            *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixRankFilterWithScaling()
 *
 * \param[in]    pixs          8 or 32 bpp; no colormap
 * \param[in]    wf, hf        width and height of filter; each is >= 1
 * \param[in]    rank          in [0.0 ... 1.0]
 * \param[in]    scalefactor   scale factor; must be >= 0.2 and <= 0.7
 * \return  pixd of rank values, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a convenience function that downscales, does
 *          the rank filtering, and upscales.  Because the down-
 *          and up-scaling functions are very fast compared to
 *          rank filtering, the time it takes is reduced from that
 *          for the simple rank filtering operation by approximately
 *          the square of the scaling factor.
 * </pre>
 */
PIX  *
pixRankFilterWithScaling(PIX       *pixs,
                         l_int32    wf,
                         l_int32    hf,
                         l_float32  rank,
                         l_float32  scalefactor)
{
l_int32  w, h, d, wfs, hfs;
PIX     *pix1, *pix2, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetColormap(pixs) != NULL)
        return (PIX *)ERROR_PTR("pixs has colormap", __func__, NULL);
    d = pixGetDepth(pixs);
    if (d != 8 && d != 32)
        return (PIX *)ERROR_PTR("pixs not 8 or 32 bpp", __func__, NULL);
    if (wf < 1 || hf < 1)
        return (PIX *)ERROR_PTR("wf < 1 || hf < 1", __func__, NULL);
    if (rank < 0.0 || rank > 1.0)
        return (PIX *)ERROR_PTR("rank must be in [0.0, 1.0]", __func__, NULL);
    if (wf == 1 && hf == 1)   /* no-op */
        return pixCopy(NULL, pixs);
    if (scalefactor < 0.2 || scalefactor > 0.7) {
        L_ERROR("invalid scale factor; no scaling used\n", __func__);
        return pixRankFilter(pixs, wf, hf, rank);
    }

    pix1 = pixScaleAreaMap(pixs, scalefactor, scalefactor);
    wfs = L_MAX(1, (l_int32)(scalefactor * wf + 0.5));
    hfs = L_MAX(1, (l_int32)(scalefactor * hf + 0.5));
    pix2 = pixRankFilter(pix1, wfs, hfs, rank);
    pixGetDimensions(pixs, &w, &h, NULL);
    pixd = pixScaleToSize(pix2, w, h);
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    return pixd;
}

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
 * \file edge.c
 * <pre>
 *
 *      Sobel edge detecting filter
 *          PIX      *pixSobelEdgeFilter()
 *
 *      Two-sided edge gradient filter
 *          PIX      *pixTwoSidedEdgeFilter()
 *
 *      Measurement of edge smoothness
 *          l_int32   pixMeasureEdgeSmoothness()
 *          NUMA     *pixGetEdgeProfile()
 *          l_int32   pixGetLastOffPixelInRun()
 *          l_int32   pixGetLastOnPixelInRun()
 *
 *
 *  The Sobel edge detector uses these two simple gradient filters.
 *
 *       1    2    1             1    0   -1
 *       0    0    0             2    0   -2
 *      -1   -2   -1             1    0   -1
 *
 *      (horizontal)             (vertical)
 *
 *  To use both the vertical and horizontal filters, set the orientation
 *  flag to L_ALL_EDGES; this sums the abs. value of their outputs,
 *  clipped to 255.
 *
 *  See comments below for displaying the resulting image with
 *  the edges dark, both for 8 bpp and 1 bpp.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

/*----------------------------------------------------------------------*
 *                    Sobel edge detecting filter                       *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixSobelEdgeFilter()
 *
 * \param[in]    pixs         8 bpp; no colormap
 * \param[in]    orientflag   L_HORIZONTAL_EDGES, L_VERTICAL_EDGES, L_ALL_EDGES
 * \return  pixd   8 bpp, edges are brighter, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Invert pixd to see larger gradients as darker (grayscale).
 *      (2) To generate a binary image of the edges, threshold
 *          the result using pixThresholdToBinary().  If the high
 *          edge values are to be fg (1), invert after running
 *          pixThresholdToBinary().
 *      (3) Label the pixels as follows:
 *              1    4    7
 *              2    5    8
 *              3    6    9
 *          Read the data incrementally across the image and unroll
 *          the loop.
 *      (4) This runs at about 45 Mpix/sec on a 3 GHz processor.
 * </pre>
 */
PIX *
pixSobelEdgeFilter(PIX     *pixs,
                   l_int32  orientflag)
{
l_int32    w, h, d, i, j, wplt, wpld, gx, gy, vald;
l_int32    val1, val2, val3, val4, val5, val6, val7, val8, val9;
l_uint32  *datat, *linet, *datad, *lined;
PIX       *pixt, *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (orientflag != L_HORIZONTAL_EDGES && orientflag != L_VERTICAL_EDGES &&
        orientflag != L_ALL_EDGES)
        return (PIX *)ERROR_PTR("invalid orientflag", __func__, NULL);

        /* Add 1 pixel (mirrored) to each side of the image. */
    if ((pixt = pixAddMirroredBorder(pixs, 1, 1, 1, 1)) == NULL)
        return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);

        /* Compute filter output at each location. */
    pixd = pixCreateTemplate(pixs);
    datat = pixGetData(pixt);
    wplt = pixGetWpl(pixt);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    for (i = 0; i < h; i++) {
        linet = datat + i * wplt;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            if (j == 0) {  /* start a new row */
                val1 = GET_DATA_BYTE(linet, j);
                val2 = GET_DATA_BYTE(linet + wplt, j);
                val3 = GET_DATA_BYTE(linet + 2 * wplt, j);
                val4 = GET_DATA_BYTE(linet, j + 1);
                val5 = GET_DATA_BYTE(linet + wplt, j + 1);
                val6 = GET_DATA_BYTE(linet + 2 * wplt, j + 1);
                val7 = GET_DATA_BYTE(linet, j + 2);
                val8 = GET_DATA_BYTE(linet + wplt, j + 2);
                val9 = GET_DATA_BYTE(linet + 2 * wplt, j + 2);
            } else {  /* shift right by 1 pixel; update incrementally */
                val1 = val4;
                val2 = val5;
                val3 = val6;
                val4 = val7;
                val5 = val8;
                val6 = val9;
                val7 = GET_DATA_BYTE(linet, j + 2);
                val8 = GET_DATA_BYTE(linet + wplt, j + 2);
                val9 = GET_DATA_BYTE(linet + 2 * wplt, j + 2);
            }
            if (orientflag == L_HORIZONTAL_EDGES)
                vald = L_ABS(val1 + 2 * val4 + val7
                             - val3 - 2 * val6 - val9) >> 3;
            else if (orientflag == L_VERTICAL_EDGES)
                vald = L_ABS(val1 + 2 * val2 + val3 - val7
                             - 2 * val8 - val9) >> 3;
            else {  /* L_ALL_EDGES */
                gx = L_ABS(val1 + 2 * val2 + val3 - val7
                           - 2 * val8 - val9) >> 3;
                gy = L_ABS(val1 + 2 * val4 + val7
                             - val3 - 2 * val6 - val9) >> 3;
                vald = L_MIN(255, gx + gy);
            }
            SET_DATA_BYTE(lined, j, vald);
        }
    }

    pixDestroy(&pixt);
    return pixd;
}


/*----------------------------------------------------------------------*
 *                   Two-sided edge gradient filter                     *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixTwoSidedEdgeFilter()
 *
 * \param[in]    pixs         8 bpp; no colormap
 * \param[in]    orientflag   L_HORIZONTAL_EDGES, L_VERTICAL_EDGES
 * \return  pixd    8 bpp, edges are brighter, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For detecting vertical edges, this considers the
 *          difference of the central pixel from those on the left
 *          and right.  For situations where the gradient is the same
 *          sign on both sides, this computes and stores the minimum
 *          (absolute value of the) difference.  The reason for
 *          checking the sign is that we are looking for pixels within
 *          a transition.  By contrast, for single pixel noise, the pixel
 *          value is either larger than or smaller than its neighbors,
 *          so the gradient would change direction on each side.  Horizontal
 *          edges are handled similarly, looking for vertical gradients.
 *      (2) To generate a binary image of the edges, threshold
 *          the result using pixThresholdToBinary().  If the high
 *          edge values are to be fg (1), invert after running
 *          pixThresholdToBinary().
 *      (3) This runs at about 60 Mpix/sec on a 3 GHz processor.
 *          It is about 30% faster than Sobel, and the results are
 *          similar.
 * </pre>
 */
PIX *
pixTwoSidedEdgeFilter(PIX     *pixs,
                      l_int32  orientflag)
{
l_int32    w, h, d, i, j, wpls, wpld;
l_int32    cval, rval, bval, val, lgrad, rgrad, tgrad, bgrad;
l_uint32  *datas, *lines, *datad, *lined;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8)
        return (PIX *)ERROR_PTR("pixs not 8 bpp", __func__, NULL);
    if (orientflag != L_HORIZONTAL_EDGES && orientflag != L_VERTICAL_EDGES)
        return (PIX *)ERROR_PTR("invalid orientflag", __func__, NULL);

    pixd = pixCreateTemplate(pixs);
    datas = pixGetData(pixs);
    wpls = pixGetWpl(pixs);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    if (orientflag == L_VERTICAL_EDGES) {
        for (i = 0; i < h; i++) {
            lines = datas + i * wpls;
            lined = datad + i * wpld;
            cval = GET_DATA_BYTE(lines, 1);
            lgrad = cval - GET_DATA_BYTE(lines, 0);
            for (j = 1; j < w - 1; j++) {
                rval = GET_DATA_BYTE(lines, j + 1);
                rgrad = rval - cval;
                if (lgrad * rgrad > 0) {
                    if (lgrad < 0)
                        val = -L_MAX(lgrad, rgrad);
                    else
                        val = L_MIN(lgrad, rgrad);
                    SET_DATA_BYTE(lined, j, val);
                }
                lgrad = rgrad;
                cval = rval;
            }
        }
    }
    else {  /* L_HORIZONTAL_EDGES) */
        for (j = 0; j < w; j++) {
            lines = datas + wpls;
            cval = GET_DATA_BYTE(lines, j);  /* for line 1 */
            tgrad = cval - GET_DATA_BYTE(datas, j);
            for (i = 1; i < h - 1; i++) {
                lines += wpls;  /* for line i + 1 */
                lined = datad + i * wpld;
                bval = GET_DATA_BYTE(lines, j);
                bgrad = bval - cval;
                if (tgrad * bgrad > 0) {
                    if (tgrad < 0)
                        val = -L_MAX(tgrad, bgrad);
                    else
                        val = L_MIN(tgrad, bgrad);
                    SET_DATA_BYTE(lined, j, val);
                }
                tgrad = bgrad;
                cval = bval;
            }
        }
    }

    return pixd;
}


/*----------------------------------------------------------------------*
 *                   Measurement of edge smoothness                     *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixMeasureEdgeSmoothness()
 *
 * \param[in]    pixs          1 bpp
 * \param[in]    side          L_FROM_LEFT, L_FROM_RIGHT, L_FROM_TOP, L_FROM_BOT
 * \param[in]    minjump       minimum jump to be counted; >= 1
 * \param[in]    minreversal   minimum reversal size for new peak or valley
 * \param[out]   pjpl          [optional] jumps/length: number of jumps,
 *                             normalized to length of component side
 * \param[out]   pjspl         [optional] jumpsum/length: sum of all
 *                             sufficiently large jumps, normalized to length
 *                             of component side
 * \param[out]   prpl          [optional] reversals/length: number of
 *                             peak-to-valley or valley-to-peak reversals,
 *                             normalized to length of component side
 * \param[in]    debugfile     [optional] displays constructed edge; use NULL
 *                             for no output
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This computes three measures of smoothness of the edge of a
 *          connected component:
 *            * jumps/length: (jpl) the number of jumps of size >= %minjump,
 *              normalized to the length of the side
 *            * jump sum/length: (jspl) the sum of all jump lengths of
 *              size >= %minjump, normalized to the length of the side
 *            * reversals/length: (rpl) the number of peak <--> valley
 *              reversals, using %minreverse as a minimum deviation of
 *              the peak or valley from its preceding extremum,
 *              normalized to the length of the side
 *      (2) The input pix should be a single connected component, but
 *          this is not required.
 * </pre>
 */
l_ok
pixMeasureEdgeSmoothness(PIX         *pixs,
                         l_int32      side,
                         l_int32      minjump,
                         l_int32      minreversal,
                         l_float32   *pjpl,
                         l_float32   *pjspl,
                         l_float32   *prpl,
                         const char  *debugfile)
{
l_int32  i, n, val, nval, diff, njumps, jumpsum, nreversal;
NUMA    *na, *nae;

    if (pjpl) *pjpl = 0.0;
    if (pjspl) *pjspl = 0.0;
    if (prpl) *prpl = 0.0;
    if (!pjpl && !pjspl && !prpl && !debugfile)
        return ERROR_INT("no output requested", __func__, 1);
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);
    if (side != L_FROM_LEFT && side != L_FROM_RIGHT &&
        side != L_FROM_TOP && side != L_FROM_BOT)
        return ERROR_INT("invalid side", __func__, 1);
    if (minjump < 1)
        return ERROR_INT("invalid minjump; must be >= 1", __func__, 1);
    if (minreversal < 1)
        return ERROR_INT("invalid minreversal; must be >= 1", __func__, 1);

    if ((na = pixGetEdgeProfile(pixs, side, debugfile)) == NULL)
        return ERROR_INT("edge profile not made", __func__, 1);
    if ((n = numaGetCount(na)) < 2) {
        numaDestroy(&na);
        return 0;
    }

    if (pjpl || pjspl) {
        jumpsum = 0;
        njumps = 0;
        numaGetIValue(na, 0, &val);
        for (i = 1; i < n; i++) {
            numaGetIValue(na, i, &nval);
            diff = L_ABS(nval - val);
            if (diff >= minjump) {
                njumps++;
                jumpsum += diff;
            }
            val = nval;
        }
        if (pjpl)
            *pjpl = (l_float32)njumps / (l_float32)(n - 1);
        if (pjspl)
            *pjspl = (l_float32)jumpsum / (l_float32)(n - 1);
    }

    if (prpl) {
        nae = numaFindExtrema(na, minreversal, NULL);
        nreversal = numaGetCount(nae) - 1;
        *prpl = (l_float32)nreversal / (l_float32)(n - 1);
        numaDestroy(&nae);
    }

    numaDestroy(&na);
    return 0;
}


/*!
 * \brief   pixGetEdgeProfile()
 *
 * \param[in]    pixs        1 bpp
 * \param[in]    side        L_FROM_LEFT, L_FROM_RIGHT, L_FROM_TOP, L_FROM_BOT
 * \param[in]    debugfile   [optional] displays constructed edge; use NULL
 *                           for no output
 * \return  na   of fg edge pixel locations, or NULL on error
 */
NUMA *
pixGetEdgeProfile(PIX         *pixs,
                  l_int32      side,
                  const char  *debugfile)
{
l_int32   x, y, w, h, loc, index, ival;
l_uint32  val;
NUMA     *na;
PIX      *pixt;
PIXCMAP  *cmap;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (NUMA *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (side != L_FROM_LEFT && side != L_FROM_RIGHT &&
        side != L_FROM_TOP && side != L_FROM_BOT)
        return (NUMA *)ERROR_PTR("invalid side", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    if (side == L_FROM_LEFT || side == L_FROM_RIGHT)
        na = numaCreate(h);
    else
        na = numaCreate(w);
    if (side == L_FROM_LEFT) {
        pixGetLastOffPixelInRun(pixs, 0, 0, L_FROM_LEFT, &loc);
        loc = (loc == w - 1) ? 0 : loc + 1;  /* back to the left edge */
        numaAddNumber(na, loc);
        for (y = 1; y < h; y++) {
            pixGetPixel(pixs, loc, y, &val);
            if (val == 1) {
                pixGetLastOnPixelInRun(pixs, loc, y, L_FROM_RIGHT, &loc);
            } else {
                pixGetLastOffPixelInRun(pixs, loc, y, L_FROM_LEFT, &loc);
                loc = (loc == w - 1) ? 0 : loc + 1;
            }
            numaAddNumber(na, loc);
        }
    }
    else if (side == L_FROM_RIGHT) {
        pixGetLastOffPixelInRun(pixs, w - 1, 0, L_FROM_RIGHT, &loc);
        loc = (loc == 0) ? w - 1 : loc - 1;  /* back to the right edge */
        numaAddNumber(na, loc);
        for (y = 1; y < h; y++) {
            pixGetPixel(pixs, loc, y, &val);
            if (val == 1) {
                pixGetLastOnPixelInRun(pixs, loc, y, L_FROM_LEFT, &loc);
            } else {
                pixGetLastOffPixelInRun(pixs, loc, y, L_FROM_RIGHT, &loc);
                loc = (loc == 0) ? w - 1 : loc - 1;
            }
            numaAddNumber(na, loc);
        }
    }
    else if (side == L_FROM_TOP) {
        pixGetLastOffPixelInRun(pixs, 0, 0, L_FROM_TOP, &loc);
        loc = (loc == h - 1) ? 0 : loc + 1;  /* back to the top edge */
        numaAddNumber(na, loc);
        for (x = 1; x < w; x++) {
            pixGetPixel(pixs, x, loc, &val);
            if (val == 1) {
                pixGetLastOnPixelInRun(pixs, x, loc, L_FROM_BOT, &loc);
            } else {
                pixGetLastOffPixelInRun(pixs, x, loc, L_FROM_TOP, &loc);
                loc = (loc == h - 1) ? 0 : loc + 1;
            }
            numaAddNumber(na, loc);
        }
    }
    else {  /* side == L_FROM_BOT */
        pixGetLastOffPixelInRun(pixs, 0, h - 1, L_FROM_BOT, &loc);
        loc = (loc == 0) ? h - 1 : loc - 1;  /* back to the bottom edge */
        numaAddNumber(na, loc);
        for (x = 1; x < w; x++) {
            pixGetPixel(pixs, x, loc, &val);
            if (val == 1) {
                pixGetLastOnPixelInRun(pixs, x, loc, L_FROM_TOP, &loc);
            } else {
                pixGetLastOffPixelInRun(pixs, x, loc, L_FROM_BOT, &loc);
                loc = (loc == 0) ? h - 1 : loc - 1;
            }
            numaAddNumber(na, loc);
        }
    }

    if (debugfile) {
        pixt = pixConvertTo8(pixs, TRUE);
        cmap = pixGetColormap(pixt);
        pixcmapAddColor(cmap, 255, 0, 0);
        index = pixcmapGetCount(cmap) - 1;
        if (side == L_FROM_LEFT || side == L_FROM_RIGHT) {
            for (y = 0; y < h; y++) {
                numaGetIValue(na, y, &ival);
                pixSetPixel(pixt, ival, y, index);
            }
        } else {  /* L_FROM_TOP or L_FROM_BOT */
            for (x = 0; x < w; x++) {
                numaGetIValue(na, x, &ival);
                pixSetPixel(pixt, x, ival, index);
            }
        }
        pixWrite(debugfile, pixt, IFF_PNG);
        pixDestroy(&pixt);
    }

    return na;
}


/*
 * \brief   pixGetLastOffPixelInRun()
 *
 * \param[in]    pixs        1 bpp
 * \param[in]    x, y        starting location
 * \param[in]    direction   L_FROM_LEFT, L_FROM_RIGHT, L_FROM_TOP, L_FROM_BOT
 * \param[out]   ploc        location in scan direction coordinate
 *                           of last OFF pixel found
 * \return   0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Search starts from the pixel at (x, y), which is OFF.
 *      (2) It returns the location in the scan direction of the last
 *          pixel in the current run that is OFF.
 *      (3) The interface for these pixel run functions is cleaner when
 *          you ask for the last pixel in the current run, rather than the
 *          first pixel of opposite polarity that is found, because the
 *          current run may go to the edge of the image, in which case
 *          no pixel of opposite polarity is found.
 * </pre>
 */
l_ok
pixGetLastOffPixelInRun(PIX      *pixs,
                        l_int32   x,
                        l_int32   y,
                        l_int32   direction,
                        l_int32  *ploc)
{
l_int32   loc, w, h;
l_uint32  val;

    if (!ploc)
        return ERROR_INT("&loc not defined", __func__, 1);
    *ploc = 0;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs undefined or not 1 bpp", __func__, 1);
    if (direction != L_FROM_LEFT && direction != L_FROM_RIGHT &&
        direction != L_FROM_TOP && direction != L_FROM_BOT)
        return ERROR_INT("invalid side", __func__, 1);

    pixGetDimensions(pixs, &w, &h, NULL);
    if (direction == L_FROM_LEFT) {
        for (loc = x; loc < w; loc++) {
            pixGetPixel(pixs, loc, y, &val);
            if (val == 1)
                break;
        }
        *ploc = loc - 1;
    } else if (direction == L_FROM_RIGHT) {
        for (loc = x; loc >= 0; loc--) {
            pixGetPixel(pixs, loc, y, &val);
            if (val == 1)
                break;
        }
        *ploc = loc + 1;
    }
    else if (direction == L_FROM_TOP) {
        for (loc = y; loc < h; loc++) {
            pixGetPixel(pixs, x, loc, &val);
            if (val == 1)
                break;
        }
        *ploc = loc - 1;
    }
    else if (direction == L_FROM_BOT) {
        for (loc = y; loc >= 0; loc--) {
            pixGetPixel(pixs, x, loc, &val);
            if (val == 1)
                break;
        }
        *ploc = loc + 1;
    }
    return 0;
}


/*
 * \brief   pixGetLastOnPixelInRun()
 *
 * \param[in]    pixs        1 bpp
 * \param[in]    x, y        starting location
 * \param[in]    direction   L_FROM_LEFT, L_FROM_RIGHT, L_FROM_TOP, L_FROM_BOT
 * \param[out]   ploc        location in scan direction coordinate
 *                           of first ON pixel found
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Search starts from the pixel at (x, y), which is ON.
 *      (2) It returns the location in the scan direction of the last
 *          pixel in the current run that is ON.
 * </pre>
 */
l_int32
pixGetLastOnPixelInRun(PIX      *pixs,
                       l_int32   x,
                       l_int32   y,
                       l_int32   direction,
                       l_int32  *ploc)
{
l_int32   loc, w, h;
l_uint32  val;

    if (!ploc)
        return ERROR_INT("&loc not defined", __func__, 1);
    *ploc = 0;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs undefined or not 1 bpp", __func__, 1);
    if (direction != L_FROM_LEFT && direction != L_FROM_RIGHT &&
        direction != L_FROM_TOP && direction != L_FROM_BOT)
        return ERROR_INT("invalid side", __func__, 1);

    pixGetDimensions(pixs, &w, &h, NULL);
    if (direction == L_FROM_LEFT) {
        for (loc = x; loc < w; loc++) {
            pixGetPixel(pixs, loc, y, &val);
            if (val == 0)
                break;
        }
        *ploc = loc - 1;
    } else if (direction == L_FROM_RIGHT) {
        for (loc = x; loc >= 0; loc--) {
            pixGetPixel(pixs, loc, y, &val);
            if (val == 0)
                break;
        }
        *ploc = loc + 1;
    }
    else if (direction == L_FROM_TOP) {
        for (loc = y; loc < h; loc++) {
            pixGetPixel(pixs, x, loc, &val);
            if (val == 0)
                break;
        }
        *ploc = loc - 1;
    }
    else if (direction == L_FROM_BOT) {
        for (loc = y; loc >= 0; loc--) {
            pixGetPixel(pixs, x, loc, &val);
            if (val == 0)
                break;
        }
        *ploc = loc + 1;
    }
    return 0;
}

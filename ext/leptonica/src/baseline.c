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
 * \file baseline.c
 * <pre>
 *
 *      Locate text baselines in an image
 *           NUMA     *pixFindBaselines()
 *           NUMA     *pixFindBaselinesGen()
 *
 *      Projective transform to remove local skew
 *           PIX      *pixDeskewLocal()
 *
 *      Determine local skew
 *           l_int32   pixGetLocalSkewTransform()
 *           NUMA     *pixGetLocalSkewAngles()
 *
 *  We have two apparently different functions here:
 *    ~ finding baselines
 *    ~ finding a projective transform to remove keystone warping
 *  The function pixGetLocalSkewAngles() returns an array of angles,
 *  one for each raster line, and the baselines of the text lines
 *  should intersect the left edge of the image with that angle.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"

    /* Default minimum textblock width */
static const l_int32  DefaultMinBlockWidth = 80;

    /* Minimum distance to travel after finding max before abandoning peak.
     * If MinDistFromPeak < 25, this risks bogus lines at the xheight. */
static const l_int32  MinDistFromPeak = 30;

    /* Thresholds for peaks and zeros, relative to the max peak.
     * If PeakThresholdRatio < 40, this risks not identifying lines.
     * Results appear insensitive to the value of ZeroThresholdRatio.  */
static const l_int32  PeakThresholdRatio = 80;
static const l_int32  ZeroThresholdRatio = 100;

    /* Default values for determining local skew */
static const l_int32  DefaultSlices = 10;
static const l_int32  DefaultSweepReduction = 2;
static const l_int32  DefaultBsReduction = 1;
static const l_float32  DefaultSweepRange = 5.;     /* degrees */
static const l_float32  DefaultSweepDelta = 1.;     /* degrees */
static const l_float32  DefaultMinbsDelta = 0.01f;  /* degrees */

    /* Overlap slice fraction added to top and bottom of each slice */
static const l_float32  OverlapFraction = 0.5;

    /* Minimum allowed confidence (ratio) for accepting a value */
static const l_float32  MinAllowedConfidence = 3.0;


/*---------------------------------------------------------------------*
 *                    Locate text baselines in an image                *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixFindBaselines()
 *
 * \param[in]    pixs     1 bpp, 300 ppi
 * \param[out]   ppta     [optional] pairs of pts corresponding to
 *                        approx. ends of each text line
 * \param[in]    pixadb   for debug output; use NULL to skip
 * \return  na of baseline y values, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a simplified interface to pixFindBaselinesGen().
 *          See Notes there.
 * </pre>
 */
NUMA *
pixFindBaselines(PIX   *pixs,
                 PTA  **ppta,
                 PIXA  *pixadb)
{
NUMA  *na;

    if ((na = pixFindBaselinesGen(pixs, DefaultMinBlockWidth,
                                  ppta, pixadb)) == NULL)
        return (NUMA *)ERROR_PTR("na not returned", __func__, NULL);

    return na;
}

/*!
 * \brief   pixFindBaselinesGen()
 *
 * \param[in]    pixs     1 bpp, 300 ppi
 * \param[in]    minw     approx min block width returned baselines, in pixels
 * \param[out]   ppta     [optional] pairs of pts corresponding to
 *                        approx. ends of each text line
 * \param[in]    pixadb   for debug output; use NULL to skip
 * \return  na of baseline y values, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Input binary image must have text lines already aligned
 *          horizontally.  This can be done by either rotating the
 *          image with pixDeskew(), or, if a projective transform
 *          is required, by doing pixDeskewLocal() first.
 *      (2) Input null for &pta if you don't want this returned.
 *          The pta will come in pairs of points (left and right end
 *          of each baseline).
 *      (3) Very short text blocks are ignored.  Use the parameter %minw
 *          to specify the (approx.) minimum length baseline for a text block
 *          that is returned.  Suggest using minw = 80 pixels to skip small
 *          text blocks consisting of up to 3 characters.
 *      (4) This function returns the locations of baselines for which
 *          the end points of the the text are found.  Return of those
 *          end points is optional.
 *      (5) This function was designed to identify short and long text lines
 *          without using dangerous thresholds on the peak heights.  It does
 *          this by combining the differential signal with a morphological
 *          analysis of the locations of the text lines.  One can also
 *          combine this data to normalize the peak heights, by weighting
 *          the differential signal in the region of each baseline
 *          by the inverse of the width of the text line found there.
 *      (6) Caution: this will not work properly on text with multiple
 *          columns, where the lines are not aligned between columns.
 *          If there are multiple columns, they should be extracted
 *          separately before finding the baselines.
 * </pre>
 */
NUMA *
pixFindBaselinesGen(PIX     *pixs,
                    l_int32  minw,
                    PTA    **ppta,
                    PIXA    *pixadb)
{
char       cmd[64];
l_int32    h, i, j, nbox, val1, val2, ndiff, bx, by, bw, bh;
l_int32    imaxloc, peakthresh, zerothresh, inpeak;
l_int32    mintosearch, max, maxloc, nloc, locval, found, nremoved;
l_int32   *array;
l_float32  maxval;
BOXA      *boxa1, *boxa2, *boxa3;
GPLOT     *gplot;
NUMA      *nasum, *nadiff, *naloc, *naval;
PIX       *pix1, *pix2;
PTA       *pta;

    if (ppta) *ppta = NULL;
    if (!pixs || pixGetDepth(pixs) != 1)
        return (NUMA *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);

        /* minw / 6 must be >= 1 */
    if (minw < 6) minw = 6;

        /* Close up the text characters, removing noise */
    pix1 = pixMorphSequence(pixs, "c25.1 + e15.1", 0);

        /* Estimate the resolution */
    if (pixadb) pixaAddPix(pixadb, pixScale(pix1, 0.25, 0.25), L_INSERT);

        /* Save the difference of adjacent row sums.
         * The high positive-going peaks are the baselines */
    if ((nasum = pixCountPixelsByRow(pix1, NULL)) == NULL) {
        pixDestroy(&pix1);
        return (NUMA *)ERROR_PTR("nasum not made", __func__, NULL);
    }
    h = pixGetHeight(pixs);
    nadiff = numaCreate(h);
    numaGetIValue(nasum, 0, &val2);
    for (i = 0; i < h - 1; i++) {
        val1 = val2;
        numaGetIValue(nasum, i + 1, &val2);
        numaAddNumber(nadiff, val1 - val2);
    }
    numaDestroy(&nasum);

    if (pixadb) {  /* show the difference signal */
        lept_mkdir("lept/baseline");
        gplotSimple1(nadiff, GPLOT_PNG, "/tmp/lept/baseline/diff", "Diff Sig");
        pix2 = pixRead("/tmp/lept/baseline/diff.png");
        pixaAddPix(pixadb, pix2, L_INSERT);
    }

        /* Use the zeroes of the profile to locate each baseline. */
    array = numaGetIArray(nadiff);
    ndiff = numaGetCount(nadiff);
    numaGetMax(nadiff, &maxval, &imaxloc);
    numaDestroy(&nadiff);

        /* Use this to begin locating a new peak: */
    peakthresh = (l_int32)maxval / PeakThresholdRatio;
        /* Use this to begin a region between peaks: */
    zerothresh = (l_int32)maxval / ZeroThresholdRatio;

    naloc = numaCreate(0);
    naval = numaCreate(0);
    inpeak = FALSE;
    for (i = 0; i < ndiff; i++) {
        if (inpeak == FALSE) {
            if (array[i] > peakthresh) {  /* transition to in-peak */
                inpeak = TRUE;
                mintosearch = i + MinDistFromPeak; /* accept no zeros
                                                 * between i and mintosearch */
                max = array[i];
                maxloc = i;
            }
        } else {  /* inpeak == TRUE; look for max */
            if (array[i] > max) {
                max = array[i];
                maxloc = i;
                mintosearch = i + MinDistFromPeak;
            } else if (i >= mintosearch && array[i] <= zerothresh) {
                                     /* leave and store previous peak */
                inpeak = FALSE;
                numaAddNumber(naval, max);
                numaAddNumber(naloc, maxloc);
            }
        }
    }
    LEPT_FREE(array);

        /* If array[ndiff-1] is max, eg. no descenders, baseline at bottom */
    if (inpeak) {
        numaAddNumber(naval, max);
        numaAddNumber(naloc, maxloc);
    }

    if (pixadb) {  /* show the raster locations for the peaks */
        gplot = gplotCreate("/tmp/lept/baseline/loc", GPLOT_PNG, "Peak locs",
                            "rasterline", "height");
        gplotAddPlot(gplot, naloc, naval, GPLOT_POINTS, "locs");
        gplotMakeOutput(gplot);
        gplotDestroy(&gplot);
        pix2 = pixRead("/tmp/lept/baseline/loc.png");
        pixaAddPix(pixadb, pix2, L_INSERT);
    }
    numaDestroy(&naval);

        /* Generate an approximate profile of text line width.
         * First, consolidate and filter the boxes of text.
         * The horizontal opening removes text blocks with width
         * less than about 'minw' pixels at full resolution. */
    snprintf(cmd, sizeof(cmd), "r11 + c20.1 + o%d.1", minw / 6);
    pix2 = pixMorphSequence(pix1, cmd, 0);
    if (pixadb) pixaAddPix(pixadb, pix2, L_COPY);
    boxa1 = pixConnComp(pix2, NULL, 4);
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    if (boxaGetCount(boxa1) == 0) {
        numaDestroy(&naloc);
        boxaDestroy(&boxa1);
        L_INFO("no components after filtering\n", __func__);
        return NULL;
    }
    boxa2 = boxaTransform(boxa1, 0, 0, 4., 4.);
    boxa3 = boxaSort(boxa2, L_SORT_BY_Y, L_SORT_INCREASING, NULL);
    boxaDestroy(&boxa1);
    boxaDestroy(&boxa2);

        /* For each baseline, find the corresponding textboxes.
         * There may be more than one textbox to a baseline.
         * Bogus textboxes of very small height may have been
         * generated, and these are removed.  Bogus textboxes can
         * also be eliminated if the bottom is too far from any of
         * the baselines.  If there are no valid textboxes for a
         * baseline, that baseline is removed.
         * Note that the boxes have been expanded from 4x reduction,
         * so box parameters are multiples of 4. */
    pta = ptaCreate(0);
    nloc = numaGetCount(naloc);
    nbox = boxaGetCount(boxa3);
    nremoved = 0;  /* keeps track of baselines removed */
    for (i = 0; i < nloc; i++) {
        numaGetIValue(naloc, i, &locval);
        found = FALSE;
        for (j = 0; j < nbox; j++) {
            boxaGetBoxGeometry(boxa3, j, &bx, &by, &bw, &bh);
            if (bh > 12 && L_ABS(locval - (by + bh)) <= 24) {
                ptaAddPt(pta, bx, locval);
                ptaAddPt(pta, bx + bw, locval);
                found = TRUE;
            }
        }
        if (!found) {  /* no textbox corresponding to this baseline */
            L_INFO("short baseline %d at y = %d removed\n", __func__,
                    i + nremoved, locval);
            numaRemoveNumber(naloc, i);
            nremoved++;
            i--;
            nloc--;
        }
    }
    boxaDestroy(&boxa3);

    if (ppta)
        *ppta = pta;
    else
        ptaDestroy(&pta);
    if (pixadb && pta) {  /* display baselines */
        l_int32  npts, x1, y1, x2, y2;
        pix1 = pixConvertTo32(pixs);
        npts = ptaGetCount(pta);
        for (i = 0; i < npts; i += 2) {
            ptaGetIPt(pta, i, &x1, &y1);
            ptaGetIPt(pta, i + 1, &x2, &y2);
            pixRenderLineArb(pix1, x1, y1, x2, y2, 2, 255, 0, 0);
        }
        pixWriteDebug("/tmp/lept/baseline/baselines.png", pix1, IFF_PNG);
        pixaAddPix(pixadb, pixScale(pix1, 0.25, 0.25), L_INSERT);
        pixDestroy(&pix1);
    }

    return naloc;
}


/*---------------------------------------------------------------------*
 *               Projective transform to remove local skew             *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixDeskewLocal()
 *
 * \param[in]    pixs        1 bpp
 * \param[in]    nslices     the number of horizontal overlapping slices;
 *                           must be larger than 1 and not exceed 20;
 *                           use 0 for default
 * \param[in]    redsweep    sweep reduction factor: 1, 2, 4 or 8;
 *                           use 0 for default value
 * \param[in]    redsearch   search reduction factor: 1, 2, 4 or 8, and
 *                           not larger than redsweep; use 0 for default value
 * \param[in]    sweeprange  half the full range, assumed about 0; in degrees;
 *                           use 0.0 for default value
 * \param[in]    sweepdelta  angle increment of sweep; in degrees;
 *                           use 0.0 for default value
 * \param[in]    minbsdelta  min binary search increment angle; in degrees;
 *                           use 0.0 for default value
 * \return  pixd, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This function allows deskew of a page whose skew changes
 *          approximately linearly with vertical position.  It uses
 *          a projective transform that in effect does a differential
 *          shear about the LHS of the page, and makes all text lines
 *          horizontal.
 *      (2) The origin of the keystoning can be either a cheap document
 *          feeder that rotates the page as it is passed through, or a
 *          camera image taken from either the left or right side
 *          of the vertical.
 *      (3) The image transformation is a projective warping,
 *          not a rotation.  Apart from this function, the text lines
 *          must be properly aligned vertically with respect to each
 *          other.  This can be done by pre-processing the page; e.g.,
 *          by rotating or horizontally shearing it.
 *          Typically, this can be achieved by vertically aligning
 *          the page edge.
 * </pre>
 */
PIX *
pixDeskewLocal(PIX       *pixs,
               l_int32    nslices,
               l_int32    redsweep,
               l_int32    redsearch,
               l_float32  sweeprange,
               l_float32  sweepdelta,
               l_float32  minbsdelta)
{
l_int32    ret;
PIX       *pixd;
PTA       *ptas, *ptad;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);

        /* Skew array gives skew angle (deg) as fctn of raster line
         * where it intersects the LHS of the image */
    ret = pixGetLocalSkewTransform(pixs, nslices, redsweep, redsearch,
                                   sweeprange, sweepdelta, minbsdelta,
                                   &ptas, &ptad);
    if (ret != 0)
        return (PIX *)ERROR_PTR("transform pts not found", __func__, NULL);

        /* Use a projective transform */
    pixd = pixProjectiveSampledPta(pixs, ptad, ptas, L_BRING_IN_WHITE);

    ptaDestroy(&ptas);
    ptaDestroy(&ptad);
    return pixd;
}


/*---------------------------------------------------------------------*
 *                       Determine the local skew                      *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixGetLocalSkewTransform()
 *
 * \param[in]    pixs
 * \param[in]    nslices      the number of horizontal overlapping slices;
 *                            must be larger than 1 and not exceed 20;
 *                            use 0 for default
 * \param[in]    redsweep     sweep reduction factor: 1, 2, 4 or 8;
 *                            use 0 for default value
 * \param[in]    redsearch    search reduction factor: 1, 2, 4 or 8, and not
 *                            larger than redsweep; use 0 for default value
 * \param[in]    sweeprange   half the full range, assumed about 0;
 *                            in degrees; use 0.0 for default value
 * \param[in]    sweepdelta   angle increment of sweep; in degrees;
 *                            use 0.0 for default value
 * \param[in]    minbsdelta   min binary search increment angle; in degrees;
 *                            use 0.0 for default value
 * \param[out]   pptas        4 points in the source
 * \param[out]   pptad        the corresponding 4 pts in the dest
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This generates two pairs of points in the src, each pair
 *          corresponding to a pair of points that would lie along
 *          the same raster line in a transformed (dewarped) image.
 *      (2) The sets of 4 src and 4 dest points returned by this function
 *          can then be used, in a projective or bilinear transform,
 *          to remove keystoning in the src.
 * </pre>
 */
l_ok
pixGetLocalSkewTransform(PIX       *pixs,
                         l_int32    nslices,
                         l_int32    redsweep,
                         l_int32    redsearch,
                         l_float32  sweeprange,
                         l_float32  sweepdelta,
                         l_float32  minbsdelta,
                         PTA      **pptas,
                         PTA      **pptad)
{
l_int32    w, h, i;
l_float32  deg2rad, angr, angd, dely;
NUMA      *naskew;
PTA       *ptas, *ptad;

    if (!pptas || !pptad)
        return ERROR_INT("&ptas and &ptad not defined", __func__, 1);
    *pptas = *pptad = NULL;
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);
    if (nslices < 2 || nslices > 20)
        nslices = DefaultSlices;
    if (redsweep < 1 || redsweep > 8)
        redsweep = DefaultSweepReduction;
    if (redsearch < 1 || redsearch > redsweep)
        redsearch = DefaultBsReduction;
    if (sweeprange == 0.0)
        sweeprange = DefaultSweepRange;
    if (sweepdelta == 0.0)
        sweepdelta = DefaultSweepDelta;
    if (minbsdelta == 0.0)
        minbsdelta = DefaultMinbsDelta;

    naskew = pixGetLocalSkewAngles(pixs, nslices, redsweep, redsearch,
                                   sweeprange, sweepdelta, minbsdelta,
                                   NULL, NULL, 0);
    if (!naskew)
        return ERROR_INT("naskew not made", __func__, 1);

    deg2rad = 3.14159265f / 180.f;
    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    ptas = ptaCreate(4);
    ptad = ptaCreate(4);
    *pptas = ptas;
    *pptad = ptad;

        /* Find i for skew line that intersects LHS at i and RHS at h / 20 */
    for (i = 0; i < h; i++) {
        numaGetFValue(naskew, i, &angd);
        angr = angd * deg2rad;
        dely = w * tan(angr);
        if (i - dely > 0.05 * h)
            break;
    }
    ptaAddPt(ptas, 0, i);
    ptaAddPt(ptas, w - 1, i - dely);
    ptaAddPt(ptad, 0, i);
    ptaAddPt(ptad, w - 1, i);

        /* Find i for skew line that intersects LHS at i and RHS at 19h / 20 */
    for (i = h - 1; i > 0; i--) {
        numaGetFValue(naskew, i, &angd);
        angr = angd * deg2rad;
        dely = w * tan(angr);
        if (i - dely < 0.95 * h)
            break;
    }
    ptaAddPt(ptas, 0, i);
    ptaAddPt(ptas, w - 1, i - dely);
    ptaAddPt(ptad, 0, i);
    ptaAddPt(ptad, w - 1, i);

    numaDestroy(&naskew);
    return 0;
}


/*!
 * \brief   pixGetLocalSkewAngles()
 *
 * \param[in]    pixs          1 bpp
 * \param[in]    nslices       the number of horizontal overlapping slices;
 *                             must be larger than 1 and not exceed 20;
 *                             use 0 for default
 * \param[in]    redsweep      sweep reduction factor: 1, 2, 4 or 8;
 *                             use 0 for default value
 * \param[in]    redsearch     search reduction factor: 1, 2, 4 or 8, and not
 *                             larger than redsweep; use 0 for default value
 * \param[in]    sweeprange    half the full range, assumed about 0;
 *                             in degrees; use 0.0 for default value
 * \param[in]    sweepdelta    angle increment of sweep; in degrees;
 *                             use 0.0 for default value
 * \param[in]    minbsdelta    min binary search increment angle; in degrees;
 *                             use 0.0 for default value
 * \param[out]   pa            [optional] slope of skew as fctn of y
 * \param[out]   pb            [optional] intercept at y = 0 of skew,
 8                             as a function of y
 * \param[in]    debug         1 for generating plot of skew angle vs. y;
 *                             0 otherwise
 * \return  naskew, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The local skew is measured in a set of overlapping strips.
 *          We then do a least square linear fit parameters to get
 *          the slope and intercept parameters a and b in
 *              skew-angle = a * y + b  (degrees)
 *          for the local skew as a function of raster line y.
 *          This is then used to make naskew, which can be interpreted
 *          as the computed skew angle (in degrees) at the left edge
 *          of each raster line.
 *      (2) naskew can then be used to find the baselines of text, because
 *          each text line has a baseline that should intersect
 *          the left edge of the image with the angle given by this
 *          array, evaluated at the raster line of intersection.
 * </pre>
 */
NUMA *
pixGetLocalSkewAngles(PIX        *pixs,
                      l_int32     nslices,
                      l_int32     redsweep,
                      l_int32     redsearch,
                      l_float32   sweeprange,
                      l_float32   sweepdelta,
                      l_float32   minbsdelta,
                      l_float32  *pa,
                      l_float32  *pb,
                      l_int32     debug)
{
l_int32    w, h, hs, i, ystart, yend, ovlap, npts;
l_float32  angle, conf, ycenter, a, b;
BOX       *box;
GPLOT     *gplot;
NUMA      *naskew, *nax, *nay;
PIX       *pix;
PTA       *pta;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (NUMA *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (nslices < 2 || nslices > 20)
        nslices = DefaultSlices;
    if (redsweep < 1 || redsweep > 8)
        redsweep = DefaultSweepReduction;
    if (redsearch < 1 || redsearch > redsweep)
        redsearch = DefaultBsReduction;
    if (sweeprange == 0.0)
        sweeprange = DefaultSweepRange;
    if (sweepdelta == 0.0)
        sweepdelta = DefaultSweepDelta;
    if (minbsdelta == 0.0)
        minbsdelta = DefaultMinbsDelta;

    pixGetDimensions(pixs, &w, &h, NULL);
    hs = h / nslices;
    ovlap = (l_int32)(OverlapFraction * hs);
    pta = ptaCreate(nslices);
    for (i = 0; i < nslices; i++) {
        ystart = L_MAX(0, hs * i - ovlap);
        yend = L_MIN(h - 1, hs * (i + 1) + ovlap);
        ycenter = (l_float32)(ystart + yend) / 2;
        box = boxCreate(0, ystart, w, yend - ystart + 1);
        pix = pixClipRectangle(pixs, box, NULL);
        pixFindSkewSweepAndSearch(pix, &angle, &conf, redsweep, redsearch,
                                  sweeprange, sweepdelta, minbsdelta);
        if (conf > MinAllowedConfidence)
            ptaAddPt(pta, ycenter, angle);
        pixDestroy(&pix);
        boxDestroy(&box);
    }

        /* Do linear least squares fit */
    if ((npts = ptaGetCount(pta)) < 2) {
        ptaDestroy(&pta);
        return (NUMA *)ERROR_PTR("can't fit skew", __func__, NULL);
    }
    ptaGetLinearLSF(pta, &a, &b, NULL);
    if (pa) *pa = a;
    if (pb) *pb = b;

        /* Make skew angle array as function of raster line */
    naskew = numaCreate(h);
    for (i = 0; i < h; i++) {
        angle = a * i + b;
        numaAddNumber(naskew, angle);
    }

    if (debug) {
        lept_mkdir("lept/baseline");
        ptaGetArrays(pta, &nax, &nay);
        gplot = gplotCreate("/tmp/lept/baseline/skew", GPLOT_PNG,
                            "skew as fctn of y", "y (in raster lines from top)",
                            "angle (in degrees)");
        gplotAddPlot(gplot, NULL, naskew, GPLOT_POINTS, "linear lsf");
        gplotAddPlot(gplot, nax, nay, GPLOT_POINTS, "actual data pts");
        gplotMakeOutput(gplot);
        gplotDestroy(&gplot);
        numaDestroy(&nax);
        numaDestroy(&nay);
    }

    ptaDestroy(&pta);
    return naskew;
}

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
 * \file dewarp2.c
 * <pre>
 *
 *    Build the page disparity model
 *
 *      Build basic page disparity model
 *          l_int32            dewarpBuildPageModel()
 *          l_int32            dewarpFindVertDisparity()
 *          l_int32            dewarpFindHorizDisparity()
 *          PTAA              *dewarpGetTextlineCenters()
 *          static PTA        *dewarpGetMeanVerticals()
 *          PTAA              *dewarpRemoveShortLines()
 *          static l_int32     dewarpGetLineEndPoints()
 *          static l_int32     dewarpFilterLineEndPoints()
 *          static PTA        *dewarpRemoveBadEndPoints()
 *          static l_int32     dewarpIsLineCoverageValid()
 *          static l_int32     dewarpLinearLSF()
 *          static l_int32     dewarpQuadraticLSF()
 *
 *      Build disparity model for slope near binding
 *          l_int32            dewarpFindHorizSlopeDisparity()
 *
 *      Build the line disparity model
 *          l_int32            dewarpBuildLineModel()
 *
 *      Query model status
 *          l_int32            dewarpaModelStatus()
 *
 *      Rendering helpers
 *          static l_int32     pixRenderMidYs()
 *          static l_int32     pixRenderHorizEndPoints
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"

static PTA *dewarpGetMeanVerticals(PIX *pixs, l_int32 x, l_int32 y);
static l_int32 dewarpGetLineEndPoints(l_int32 h, PTAA *ptaa, PTA **pptal,
                                      PTA **pptar);
static l_int32 dewarpFilterLineEndPoints(L_DEWARP  *dew, PTA *ptal1, PTA *ptar1,
                                         PTA **pptal2, PTA **pptar2);
static PTA *dewarpRemoveBadEndPoints(l_int32 w, PTA *ptas);
static l_int32 dewarpIsLineCoverageValid(PTAA *ptaa2, l_int32 h,
                                         l_int32 *pntop, l_int32 *pnbot,
                                         l_int32 *pytop, l_int32 *pybot);
static l_int32 dewarpLinearLSF(PTA *ptad, l_float32 *pa, l_float32 *pb,
                               l_float32 *pmederr);
static l_int32 dewarpQuadraticLSF(PTA *ptad, l_float32 *pa, l_float32 *pb,
                                  l_float32 *pc, l_float32 *pmederr);
static l_int32 pixRenderMidYs(PIX *pixs, NUMA *namidys, l_int32 linew);
static l_int32 pixRenderHorizEndPoints(PIX *pixs, PTA *ptal, PTA *ptar,
                                       l_uint32 color);


#ifndef  NO_CONSOLE_IO
#define  DEBUG_TEXTLINE_CENTERS    0   /* set this to 1 for debugging */
#define  DEBUG_SHORT_LINES         0   /* ditto */
#else
#define  DEBUG_TEXTLINE_CENTERS    0   /* always must be 0 */
#define  DEBUG_SHORT_LINES         0   /* ditto */
#endif  /* !NO_CONSOLE_IO */

    /* Special parameter values for reducing horizontal disparity */
static const l_float32   MinRatioLinesToHeight = 0.45f;
static const l_int32     MinLinesForHoriz1 = 10; /* initially */
static const l_int32     MinLinesForHoriz2 = 3;  /* after, in each half */
static const l_float32   AllowedWidthFract = 0.05f;  /* no bigger */


/*----------------------------------------------------------------------*
 *                   Build basic page disparity model                   *
 *----------------------------------------------------------------------*/
/*!
 * \brief   dewarpBuildPageModel()
 *
 * \param[in]    dew
 * \param[in]    debugfile    use NULL to skip writing this
 * \return  0 if OK, 1 if unable to build the model or on error
 *
 * <pre>
 * Notes:
 *      (1) This is the basic function that builds the horizontal and
 *          vertical disparity arrays, which allow determination of the
 *          src pixel in the input image corresponding to each
 *          dest pixel in the dewarped image.
 *      (2) Sets vsuccess = 1 if the vertical disparity array builds.
 *          Always attempts to build the horizontal disparity array,
 *          even if it will not be requested (useboth == 0).
 *          Sets hsuccess = 1 if horizontal disparity builds.
 *      (3) The method is as follows:
 *          (a) Estimate the points along the centers of all the
 *              long textlines.  If there are too few lines, no
 *              disparity models are built.
 *          (b) From the vertical deviation of the lines, estimate
 *              the vertical disparity.
 *          (c) From the ends of the lines, estimate the horizontal
 *              disparity, assuming that the text is made of lines
 *              that are close to left and right justified.
 *          (d) One can also compute an additional contribution to the
 *              horizontal disparity, inferred from slopes of the top
 *              and bottom lines.  We do not do this.
 *      (4) In more detail for the vertical disparity:
 *          (a) Fit a LS quadratic to center locations along each line.
 *              This smooths the curves.
 *          (b) Sample each curve at a regular interval, find the y-value
 *              of the mid-point on each curve, and subtract the sampled
 *              curve value from this value.  This is the vertical
 *              disparity at sampled points along each curve.
 *          (c) Fit a LS quadratic to each set of vertically aligned
 *              disparity samples.  This smooths the disparity values
 *              in the vertical direction.  Then resample at the same
 *              regular interval.  We now have a regular grid of smoothed
 *              vertical disparity valuels.
 *      (5) Once the sampled vertical disparity array is found, it can be
 *          interpolated to get a full resolution vertical disparity map.
 *          This can be applied directly to the src image pixels
 *          to dewarp the image in the vertical direction, making
 *          all textlines horizontal.  Likewise, the horizontal
 *          disparity array is used to left- and right-align the
 *          longest textlines.
 * </pre>
 */
l_ok
dewarpBuildPageModel(L_DEWARP    *dew,
                     const char  *debugfile)
{
l_int32  linecount, ntop, nbot, ytop, ybot, ret;
PIX     *pixs, *pix1, *pix2, *pix3;
PTA     *pta;
PTAA    *ptaa1, *ptaa2;

    if (!dew)
        return ERROR_INT("dew not defined", __func__, 1);

    dew->debug = (debugfile) ? 1 : 0;
    dew->vsuccess = dew->hsuccess = 0;
    pixs = dew->pixs;
    if (debugfile) {
        lept_rmdir("lept/dewmod");  /* erase previous images */
        lept_mkdir("lept/dewmod");
        pixDisplayWithTitle(pixs, 0, 0, "pixs", 1);
        pixWriteDebug("/tmp/lept/dewmod/0010.png", pixs, IFF_PNG);
    }

        /* Make initial estimate of centers of textlines */
    ptaa1 = dewarpGetTextlineCenters(pixs, debugfile || DEBUG_TEXTLINE_CENTERS);
    if (!ptaa1) {
        L_WARNING("textline centers not found; model not built\n", __func__);
        return 1;
    }
    if (debugfile) {
        pix1 = pixConvertTo32(pixs);
        pta = generatePtaFilledCircle(1);
        pix2 = pixGenerateFromPta(pta, 5, 5);
        pix3 = pixDisplayPtaaPattern(NULL, pix1, ptaa1, pix2, 2, 2);
        pixWriteDebug("/tmp/lept/dewmod/0020.png", pix3, IFF_PNG);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
        pixDestroy(&pix3);
        ptaDestroy(&pta);
    }

        /* Remove all lines that are not at least 0.8 times the length
         * of the longest line. */
    ptaa2 = dewarpRemoveShortLines(pixs, ptaa1, 0.8f,
                                   debugfile || DEBUG_SHORT_LINES);
    if (debugfile) {
        pix1 = pixConvertTo32(pixs);
        pta = generatePtaFilledCircle(1);
        pix2 = pixGenerateFromPta(pta, 5, 5);
        pix3 = pixDisplayPtaaPattern(NULL, pix1, ptaa2, pix2, 2, 2);
        pixWriteDebug("/tmp/lept/dewmod/0030.png", pix3, IFF_PNG);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
        pixDestroy(&pix3);
        ptaDestroy(&pta);
    }
    ptaaDestroy(&ptaa1);

        /* Verify that there are sufficient "long" lines */
    linecount = ptaaGetCount(ptaa2);
    if (linecount < dew->minlines) {
        ptaaDestroy(&ptaa2);
        L_WARNING("linecount %d < min req'd number of lines (%d) for model\n",
                  __func__, linecount, dew->minlines);
        return 1;
    }

        /* Verify that the lines have a reasonable coverage of the
         * vertical extent of the page. */
    if (dewarpIsLineCoverageValid(ptaa2, pixGetHeight(pixs),
                                  &ntop, &nbot, &ytop, &ybot) == FALSE) {
        ptaaDestroy(&ptaa2);
        L_WARNING("invalid line coverage: ntop = %d, nbot = %d;"
                  " spanning [%d ... %d] in height %d\n", __func__,
                  ntop, nbot, ytop, ybot, pixGetHeight(pixs));
        return 1;
    }

        /* Get the sampled vertical disparity from the textline centers.
         * The disparity array will push pixels vertically so that each
         * textline is flat and centered at the y-position of the mid-point. */
    if (dewarpFindVertDisparity(dew, ptaa2, 0) != 0) {
        L_WARNING("vertical disparity not built\n", __func__);
        ptaaDestroy(&ptaa2);
        return 1;
    }

        /* Get the sampled horizontal disparity from the left and right
         * edges of the text.  The disparity array will expand the image
         * linearly outward to align the text edges vertically.
         * Do this even if useboth == 0; we still calculate it even
         * if we don't plan to use it. */
    if ((ret = dewarpFindHorizDisparity(dew, ptaa2)) == 0)
        L_INFO("hsuccess = 1\n", __func__);

        /* Debug output */
    if (debugfile) {
        dewarpPopulateFullRes(dew, NULL, 0, 0);
        pix1 = fpixRenderContours(dew->fullvdispar, 3.0f, 0.15f);
        pixWriteDebug("/tmp/lept/dewmod/0060.png", pix1, IFF_PNG);
        pixDisplay(pix1, 1000, 0);
        pixDestroy(&pix1);
        if (ret == 0) {
            pix1 = fpixRenderContours(dew->fullhdispar, 3.0f, 0.15f);
            pixWriteDebug("/tmp/lept/dewmod/0070.png", pix1, IFF_PNG);
            pixDisplay(pix1, 1000, 0);
            pixDestroy(&pix1);
        }
        convertFilesToPdf("/tmp/lept/dewmod", NULL, 135, 1.0, 0, 0,
                          "Dewarp Build Model", debugfile);
        lept_stderr("pdf file: %s\n", debugfile);
    }

    ptaaDestroy(&ptaa2);
    return 0;
}


/*!
 * \brief   dewarpFindVertDisparity()
 *
 * \param[in]    dew
 * \param[in]    ptaa       unsmoothed lines, not vertically ordered
 * \param[in]    rotflag    0 if using dew->pixs; 1 if rotated by 90 degrees cw
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This starts with points along the centers of textlines.
 *          It does quadratic fitting (and smoothing), first along the
 *          lines and then in the vertical direction, to generate
 *          the sampled vertical disparity map.  This can then be
 *          interpolated to full resolution and used to remove
 *          the vertical line warping.
 *      (2) Use %rotflag == 1 if you are dewarping vertical lines, as
 *          is done in dewarpBuildLineModel().  The usual case is for
 *          %rotflag == 0.
 *      (3) Note that this builds a vertical disparity model (VDM), but
 *          does not check it against constraints for validity.
 *          Constraint checking is done after building the models,
 *          and before inserting reference models.
 *      (4) This sets the vsuccess flag to 1 on success.
 *      (5) Pix debug output goes to /tmp/dewvert/ for collection into
 *          a pdf.  Non-pix debug output goes to /tmp.
 * </pre>
 */
l_ok
dewarpFindVertDisparity(L_DEWARP  *dew,
                        PTAA      *ptaa,
                        l_int32    rotflag)
{
l_int32     i, j, nlines, npts, nx, ny, sampling;
l_float32   c0, c1, c2, x, y, midy, val, medval, meddev, minval, maxval;
l_float32  *famidys;
NUMA       *nax, *nafit, *nacurve0, *nacurve1, *nacurves;
NUMA       *namidy, *namidys, *namidysi;
PIX        *pix1, *pix2, *pixcirc, *pixdb;
PTA        *pta, *ptad, *ptacirc;
PTAA       *ptaa0, *ptaa1, *ptaa2, *ptaa3, *ptaa4, *ptaa5, *ptaat;
FPIX       *fpix;

    if (!dew)
        return ERROR_INT("dew not defined", __func__, 1);
    dew->vsuccess = 0;
    if (!ptaa)
        return ERROR_INT("ptaa not defined", __func__, 1);

    if (dew->debug) L_INFO("finding vertical disparity\n", __func__);

        /* Do quadratic fit to smooth each line.  A single quadratic
         * over the entire width of the line appears to be sufficient.
         * Quartics tend to overfit to noise.  Each line is thus
         * represented by three coefficients: y(x) = c2 * x^2 + c1 * x + c0.
         * Using the coefficients, sample each fitted curve uniformly
         * across the full width of the image.  The result is in ptaa0.  */
    sampling = dew->sampling;
    nx = (rotflag) ? dew->ny : dew->nx;
    ny = (rotflag) ? dew->nx : dew->ny;
    nlines = ptaaGetCount(ptaa);
    dew->nlines = nlines;
    ptaa0 = ptaaCreate(nlines);
    nacurve0 = numaCreate(nlines);  /* stores curvature coeff c2 */
    pixdb = (rotflag) ? pixRotateOrth(dew->pixs, 1) : pixClone(dew->pixs);
    for (i = 0; i < nlines; i++) {  /* for each line */
        pta = ptaaGetPta(ptaa, i, L_CLONE);
        ptaGetQuadraticLSF(pta, &c2, &c1, &c0, NULL);
        numaAddNumber(nacurve0, c2);
        ptad = ptaCreate(nx);
        for (j = 0; j < nx; j++) {  /* uniformly sampled in x */
             x = j * sampling;
             applyQuadraticFit(c2, c1, c0, x, &y);
             ptaAddPt(ptad, x, y);
        }
        ptaaAddPta(ptaa0, ptad, L_INSERT);
        ptaDestroy(&pta);
    }
    if (dew->debug) {
        lept_mkdir("lept/dewarp");
        lept_mkdir("lept/dewdebug");
        lept_mkdir("lept/dewmod");
        ptaat = ptaaCreate(nlines);
        for (i = 0; i < nlines; i++) {
            pta = ptaaGetPta(ptaa, i, L_CLONE);
            ptaGetArrays(pta, &nax, NULL);
            ptaGetQuadraticLSF(pta, NULL, NULL, NULL, &nafit);
            ptad = ptaCreateFromNuma(nax, nafit);
            ptaaAddPta(ptaat, ptad, L_INSERT);
            ptaDestroy(&pta);
            numaDestroy(&nax);
            numaDestroy(&nafit);
        }
        pix1 = pixConvertTo32(pixdb);
        pta = generatePtaFilledCircle(1);
        pixcirc = pixGenerateFromPta(pta, 5, 5);
        pix2 = pixDisplayPtaaPattern(NULL, pix1, ptaat, pixcirc, 2, 2);
        pixWriteDebug("/tmp/lept/dewmod/0041.png", pix2, IFF_PNG);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
        ptaDestroy(&pta);
        pixDestroy(&pixcirc);
        ptaaDestroy(&ptaat);
    }

        /* Remove lines with outlier curvatures.
         * Note that this is just looking for internal consistency in
         * the line curvatures.  It is not rejecting lines based on
         * the magnitude of the curvature.  That is done when constraints
         * are applied for valid models. */
    numaGetMedianDevFromMedian(nacurve0, &medval, &meddev);
    L_INFO("\nPage %d\n", __func__, dew->pageno);
    L_INFO("Pass 1: Curvature: medval = %f, meddev = %f\n",
           __func__, medval, meddev);
    ptaa1 = ptaaCreate(nlines);
    nacurve1 = numaCreate(nlines);
    for (i = 0; i < nlines; i++) {  /* for each line */
        numaGetFValue(nacurve0, i, &val);
        if (L_ABS(val - medval) > 7.0 * meddev)  /* TODO: reduce to ~ 3.0 */
            continue;
        pta = ptaaGetPta(ptaa0, i, L_CLONE);
        ptaaAddPta(ptaa1, pta, L_INSERT);
        numaAddNumber(nacurve1, val);
    }
    nlines = ptaaGetCount(ptaa1);
    numaDestroy(&nacurve0);

        /* Save the min and max curvature (in micro-units) */
    numaGetMin(nacurve1, &minval, NULL);
    numaGetMax(nacurve1, &maxval, NULL);
    dew->mincurv = lept_roundftoi(1000000. * minval);
    dew->maxcurv = lept_roundftoi(1000000. * maxval);
    L_INFO("Pass 2: Min/max curvature = (%d, %d)\n", __func__,
           dew->mincurv, dew->maxcurv);

        /* Find and save the y values at the mid-points in each curve.
         * If the slope is zero anywhere, it will typically be here. */
    namidy = numaCreate(nlines);
    for (i = 0; i < nlines; i++) {
        pta = ptaaGetPta(ptaa1, i, L_CLONE);
        npts = ptaGetCount(pta);
        ptaGetPt(pta, npts / 2, NULL, &midy);
        numaAddNumber(namidy, midy);
        ptaDestroy(&pta);
    }

        /* Sort the lines in ptaa1c by their vertical position, going down */
    namidysi = numaGetSortIndex(namidy, L_SORT_INCREASING);
    namidys = numaSortByIndex(namidy, namidysi);
    nacurves = numaSortByIndex(nacurve1, namidysi);
    numaDestroy(&dew->namidys);  /* in case previously made */
    numaDestroy(&dew->nacurves);
    dew->namidys = namidys;
    dew->nacurves = nacurves;
    ptaa2 = ptaaSortByIndex(ptaa1, namidysi);
    numaDestroy(&namidy);
    numaDestroy(&nacurve1);
    numaDestroy(&namidysi);
    if (dew->debug) {
        numaWriteDebug("/tmp/lept/dewdebug/midys.na", namidys);
        numaWriteDebug("/tmp/lept/dewdebug/curves.na", nacurves);
        pix1 = pixConvertTo32(pixdb);
        ptacirc = generatePtaFilledCircle(5);
        pixcirc = pixGenerateFromPta(ptacirc, 11, 11);
        srand(3);
        pixDisplayPtaaPattern(pix1, pix1, ptaa2, pixcirc, 5, 5);
        srand(3);  /* use the same colors for text and reference lines */
        pixRenderMidYs(pix1, namidys, 2);
        pix2 = (rotflag) ? pixRotateOrth(pix1, 3) : pixClone(pix1);
        pixWriteDebug("/tmp/lept/dewmod/0042.png", pix2, IFF_PNG);
        pixDisplay(pix2, 0, 0);
        ptaDestroy(&ptacirc);
        pixDestroy(&pixcirc);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }
    pixDestroy(&pixdb);

        /* Convert the sampled points in ptaa2 to a sampled disparity with
         * with respect to the y value at the mid point in the curve.
         * The disparity is the distance the point needs to move;
         * plus is downward.  */
    ptaa3 = ptaaCreate(nlines);
    for (i = 0; i < nlines; i++) {
        pta = ptaaGetPta(ptaa2, i, L_CLONE);
        numaGetFValue(namidys, i, &midy);
        ptad = ptaCreate(nx);
        for (j = 0; j < nx; j++) {
            ptaGetPt(pta, j, &x, &y);
            ptaAddPt(ptad, x, midy - y);
        }
        ptaaAddPta(ptaa3, ptad, L_INSERT);
        ptaDestroy(&pta);
    }
    if (dew->debug) {
        ptaaWriteDebug("/tmp/lept/dewdebug/ptaa3.ptaa", ptaa3, 0);
    }

        /* Generate ptaa4 by taking vertical 'columns' from ptaa3.
         * We want to fit the vertical disparity on the column to the
         * vertical position of the line, which we call 'y' here and
         * obtain from namidys.  So each pta in ptaa4 is the set of
         * vertical disparities down a column of points.  The columns
         * in ptaa4 are equally spaced in x. */
    ptaa4 = ptaaCreate(nx);
    famidys = numaGetFArray(namidys, L_NOCOPY);
    for (j = 0; j < nx; j++) {
        pta = ptaCreate(nlines);
        for (i = 0; i < nlines; i++) {
            y = famidys[i];
            ptaaGetPt(ptaa3, i, j, NULL, &val);  /* disparity value */
            ptaAddPt(pta, y, val);
        }
        ptaaAddPta(ptaa4, pta, L_INSERT);
    }
    if (dew->debug) {
        ptaaWriteDebug("/tmp/lept/dewdebug/ptaa4.ptaa", ptaa4, 0);
    }

        /* Do quadratic fit vertically on each of the pixel columns
         * in ptaa4, for the vertical displacement (which identifies the
         * src pixel(s) for each dest pixel) as a function of y (the
         * y value of the mid-points for each line).  Then generate
         * ptaa5 by sampling the fitted vertical displacement on a
         * regular grid in the vertical direction.  Each pta in ptaa5
         * gives the vertical displacement for regularly sampled y values
         * at a fixed x. */
    ptaa5 = ptaaCreate(nx);  /* uniformly sampled across full height of image */
    for (j = 0; j < nx; j++) {  /* for each column */
        pta = ptaaGetPta(ptaa4, j, L_CLONE);
        ptaGetQuadraticLSF(pta, &c2, &c1, &c0, NULL);
        ptad = ptaCreate(ny);
        for (i = 0; i < ny; i++) {  /* uniformly sampled in y */
             y = i * sampling;
             applyQuadraticFit(c2, c1, c0, y, &val);
             ptaAddPt(ptad, y, val);
        }
        ptaaAddPta(ptaa5, ptad, L_INSERT);
        ptaDestroy(&pta);
    }
    if (dew->debug) {
        ptaaWriteDebug("/tmp/lept/dewdebug/ptaa5.ptaa", ptaa5, 0);
        convertFilesToPdf("/tmp/lept/dewmod", "004", 135, 1.0, 0, 0,
                          "Dewarp Vert Disparity",
                          "/tmp/lept/dewarp/vert_disparity.pdf");
        lept_stderr("pdf file: /tmp/lept/dewarp/vert_disparity.pdf\n");
    }

        /* Save the result in a fpix at the specified subsampling  */
    fpix = fpixCreate(nx, ny);
    for (i = 0; i < ny; i++) {
        for (j = 0; j < nx; j++) {
            ptaaGetPt(ptaa5, j, i, NULL, &val);
            fpixSetPixel(fpix, j, i, val);
        }
    }
    dew->sampvdispar = fpix;
    dew->vsuccess = 1;

    ptaaDestroy(&ptaa0);
    ptaaDestroy(&ptaa1);
    ptaaDestroy(&ptaa2);
    ptaaDestroy(&ptaa3);
    ptaaDestroy(&ptaa4);
    ptaaDestroy(&ptaa5);
    return 0;
}


/*!
 * \brief   dewarpFindHorizDisparity()
 *
 * \param[in]    dew
 * \param[in]    ptaa     unsmoothed lines, not vertically ordered
 * \return  0 if OK, 1 if horizontal disparity array is not built, or on error
 *
 * <pre>
 * Notes:
 *      (1) This builds a horizontal disparity model (HDM), but
 *          does not check it against constraints for validity.
 *          Constraint checking is done at rendering time.
 *      (2) Horizontal disparity is not required for a successful model;
 *          only the vertical disparity is required.  This will not be
 *          called if the function to build the vertical disparity fails.
 *      (3) This sets the hsuccess flag to 1 on success.
 *      (4) Internally in ptal1, ptar1, ptal2, ptar2: x and y are reversed,
 *          so the 'y' value is horizontal distance across the image width.
 *      (5) Debug output goes to /tmp/lept/dewmod/ for collection into a pdf.
 * </pre>
 */
l_ok
dewarpFindHorizDisparity(L_DEWARP  *dew,
                         PTAA      *ptaa)
{
l_int32    i, j, h, nx, ny, sampling, ret, linear_edgefit;
l_float32  c0, c1, cl0, cl1, cl2, cr0, cr1, cr2;
l_float32  x, y, refl, refr;
l_float32  val, mederr;
NUMA      *nald, *nard;
PIX       *pix1;
PTA       *ptal1, *ptar1;  /* left/right end points of lines; initial */
PTA       *ptal2, *ptar2;  /* left/right end points; after filtering */
PTA       *ptal3, *ptar3;  /* left and right block, fitted, uniform spacing */
PTA       *pta, *ptat, *pta1, *pta2;
PTAA      *ptaah;
FPIX      *fpix;

    if (!dew)
        return ERROR_INT("dew not defined", __func__, 1);
    dew->hsuccess = 0;
    if (!ptaa)
        return ERROR_INT("ptaa not defined", __func__, 1);

    if (dew->debug) L_INFO("finding horizontal disparity\n", __func__);

        /* Get the endpoints of the lines, and sort from top to bottom */
    h = pixGetHeight(dew->pixs);
    ret = dewarpGetLineEndPoints(h, ptaa, &ptal1, &ptar1);
    if (ret) {
        L_INFO("Horiz disparity not built\n", __func__);
        return 1;
    }
    if (dew->debug) {
        lept_mkdir("lept/dewdebug");
        lept_mkdir("lept/dewarp");
        ptaWriteDebug("/tmp/lept/dewdebug/endpts_left1.pta", ptal1, 1);
        ptaWriteDebug("/tmp/lept/dewdebug/endpts_right1.pta", ptar1, 1);
    }

        /* Filter the points by x-location to prevent 2-column images
         * from getting confused about left and right endpoints. We
         * require valid left points to not be farther than
         *     0.20 * (remaining distance to the right edge of the image)
         * to the right of the leftmost endpoint, and similarly for
         * the right endpoints. (Note: x and y are reversed in the pta.)
         * Also require end points to be near the medians in the
         * upper and lower halves. */
    ret = dewarpFilterLineEndPoints(dew, ptal1, ptar1, &ptal2, &ptar2);
    ptaDestroy(&ptal1);
    ptaDestroy(&ptar1);
    if (ret) {
        L_INFO("Not enough filtered end points\n", __func__);
        return 1;
    }

        /* Do either a linear or a quadratic fit to the left and right
         * endpoints of the longest lines.  It is not necessary to use
         * the noisy LSF fit function, because we've removed outlier
         * end points by selecting the long lines.
         * For the linear fit, each line is represented by 2 coefficients:
         *     x(y) = c1 * y + c0.
         * For the quadratic fit, each line is represented by 3 coefficients:
         *     x(y) = c2 * y^2 + c1 * y + c0.
         * Then using the coefficients, sample each fitted curve uniformly
         * along the full height of the image. */
    sampling = dew->sampling;
    nx = dew->nx;
    ny = dew->ny;
    linear_edgefit = (dew->dewa->max_edgecurv == 0) ? TRUE : FALSE;

    if (linear_edgefit) {
            /* Fit the left side, using linear LSF on the set of long lines. */
        dewarpLinearLSF(ptal2, &cl1, &cl0, &mederr);
        dew->leftslope = lept_roundftoi(1000. * cl1);  /* milli-units */
        dew->leftcurv = 0;  /* micro-units */
        L_INFO("Left linear LSF median error = %5.2f\n", __func__,  mederr);
        L_INFO("Left edge slope = %d\n", __func__, dew->leftslope);
        ptal3 = ptaCreate(ny);
        for (i = 0; i < ny; i++) {  /* uniformly sample in y */
            y = i * sampling;
            applyLinearFit(cl1, cl0, y, &x);
            ptaAddPt(ptal3, x, y);
        }

            /* Do a linear LSF on the right side. */
        dewarpLinearLSF(ptar2, &cr1, &cr0, &mederr);
        dew->rightslope = lept_roundftoi(1000.0 * cr1);  /* milli-units */
        dew->rightcurv = 0;  /* micro-units */
        L_INFO("Right linear LSF median error = %5.2f\n", __func__,  mederr);
        L_INFO("Right edge slope = %d\n", __func__, dew->rightslope);
        ptar3 = ptaCreate(ny);
        for (i = 0; i < ny; i++) {  /* uniformly sample in y */
            y = i * sampling;
            applyLinearFit(cr1, cr0, y, &x);
            ptaAddPt(ptar3, x, y);
        }
    } else {  /* quadratic edge fit */
            /* Fit the left side, using quadratic LSF on the long lines. */
        dewarpQuadraticLSF(ptal2, &cl2, &cl1, &cl0, &mederr);
        dew->leftslope = lept_roundftoi(1000. * cl1);  /* milli-units */
        dew->leftcurv = lept_roundftoi(1000000. * cl2);  /* micro-units */
        L_INFO("Left quad LSF median error = %5.2f\n", __func__,  mederr);
        L_INFO("Left edge slope = %d\n", __func__, dew->leftslope);
        L_INFO("Left edge curvature = %d\n", __func__, dew->leftcurv);
        ptal3 = ptaCreate(ny);
        for (i = 0; i < ny; i++) {  /* uniformly sample in y */
            y = i * sampling;
            applyQuadraticFit(cl2, cl1, cl0, y, &x);
            ptaAddPt(ptal3, x, y);
        }

            /* Do a quadratic LSF on the right side. */
        dewarpQuadraticLSF(ptar2, &cr2, &cr1, &cr0, &mederr);
        dew->rightslope = lept_roundftoi(1000.0 * cr1);  /* milli-units */
        dew->rightcurv = lept_roundftoi(1000000. * cr2);  /* micro-units */
        L_INFO("Right quad LSF median error = %5.2f\n", __func__,  mederr);
        L_INFO("Right edge slope = %d\n", __func__, dew->rightslope);
        L_INFO("Right edge curvature = %d\n", __func__, dew->rightcurv);
        ptar3 = ptaCreate(ny);
        for (i = 0; i < ny; i++) {  /* uniformly sample in y */
            y = i * sampling;
            applyQuadraticFit(cr2, cr1, cr0, y, &x);
            ptaAddPt(ptar3, x, y);
        }
    }

    if (dew->debug) {
        PTA  *ptalft, *ptarft;
        h = pixGetHeight(dew->pixs);
        pta1 = ptaCreate(h);
        pta2 = ptaCreate(h);
        if (linear_edgefit) {
            for (i = 0; i < h; i++) {
                applyLinearFit(cl1, cl0, i, &x);
                ptaAddPt(pta1, x, i);
                applyLinearFit(cr1, cr0, i, &x);
                ptaAddPt(pta2, x, i);
            }
        } else {  /* quadratic edge fit */
            for (i = 0; i < h; i++) {
                applyQuadraticFit(cl2, cl1, cl0, i, &x);
                ptaAddPt(pta1, x, i);
                applyQuadraticFit(cr2, cr1, cr0, i, &x);
                ptaAddPt(pta2, x, i);
            }
        }
        pix1 = pixDisplayPta(NULL, dew->pixs, pta1);
        pixDisplayPta(pix1, pix1, pta2);
        pixRenderHorizEndPoints(pix1, ptal2, ptar2, 0xff000000);
        pixDisplay(pix1, 600, 800);
        pixWriteDebug("/tmp/lept/dewmod/0051.png", pix1, IFF_PNG);
        pixDestroy(&pix1);

        pix1 = pixDisplayPta(NULL, dew->pixs, pta1);
        pixDisplayPta(pix1, pix1, pta2);
        ptalft = ptaTranspose(ptal3);
        ptarft = ptaTranspose(ptar3);
        pixRenderHorizEndPoints(pix1, ptalft, ptarft, 0x0000ff00);
        pixDisplay(pix1, 800, 800);
        pixWriteDebug("/tmp/lept/dewmod/0052.png", pix1, IFF_PNG);
        convertFilesToPdf("/tmp/lept/dewmod", "005", 135, 1.0, 0, 0,
                          "Dewarp Horiz Disparity",
                          "/tmp/lept/dewarp/horiz_disparity.pdf");
        lept_stderr("pdf file: /tmp/lept/dewarp/horiz_disparity.pdf\n");
        pixDestroy(&pix1);
        ptaDestroy(&pta1);
        ptaDestroy(&pta2);
        ptaDestroy(&ptalft);
        ptaDestroy(&ptarft);
    }

        /* Find the x value at the midpoints (in y) of the two vertical lines,
         * ptal3 and ptar3.  These are the reference values for each of the
         * lines.  Then use the difference between the these midpoint
         * values and the actual x coordinates of the lines to represent
         * the horizontal disparity (nald, nard) on the vertical lines
         * for the sampled y values. */
    ptaGetPt(ptal3, ny / 2, &refl, NULL);
    ptaGetPt(ptar3, ny / 2, &refr, NULL);
    nald = numaCreate(ny);
    nard = numaCreate(ny);
    for (i = 0; i < ny; i++) {
        ptaGetPt(ptal3, i, &x, NULL);
        numaAddNumber(nald, refl - x);
        ptaGetPt(ptar3, i, &x, NULL);
        numaAddNumber(nard, refr - x);
    }

        /* Now for each pair of sampled values of the two lines (at the
         * same value of y), do a linear interpolation to generate
         * the horizontal disparity on all sampled points between them.  */
    ptaah = ptaaCreate(ny);
    for (i = 0; i < ny; i++) {
        pta = ptaCreate(2);
        numaGetFValue(nald, i, &val);
        ptaAddPt(pta, refl, val);
        numaGetFValue(nard, i, &val);
        ptaAddPt(pta, refr, val);
        ptaGetLinearLSF(pta, &c1, &c0, NULL);  /* horiz disparity along line */
        ptat = ptaCreate(nx);
        for (j = 0; j < nx; j++) {
            x = j * sampling;
            applyLinearFit(c1, c0, x, &val);
            ptaAddPt(ptat, x, val);
        }
        ptaaAddPta(ptaah, ptat, L_INSERT);
        ptaDestroy(&pta);
    }
    numaDestroy(&nald);
    numaDestroy(&nard);

        /* Save the result in a fpix at the specified subsampling  */
    fpix = fpixCreate(nx, ny);
    for (i = 0; i < ny; i++) {
        for (j = 0; j < nx; j++) {
            ptaaGetPt(ptaah, i, j, NULL, &val);
            fpixSetPixel(fpix, j, i, val);
        }
    }
    dew->samphdispar = fpix;
    dew->hsuccess = 1;
    ptaDestroy(&ptal2);
    ptaDestroy(&ptar2);
    ptaDestroy(&ptal3);
    ptaDestroy(&ptar3);
    ptaaDestroy(&ptaah);
    return 0;
}


/*!
 * \brief   dewarpGetTextlineCenters()
 *
 * \param[in]    pixs        1 bpp
 * \param[in]    debugflag   1 for debug output
 * \return  ptaa of center values of textlines
 *
 * <pre>
 * Notes:
 *      (1) This in general does not have a point for each value
 *          of x, because there will be gaps between words.
 *          It doesn't matter because we will fit a quadratic to the
 *          points that we do have.
 * </pre>
 */
PTAA *
dewarpGetTextlineCenters(PIX     *pixs,
                         l_int32  debugflag)
{
char      buf[64];
l_int32   i, w, h, bx, by, nsegs, csize1, csize2;
BOXA     *boxa;
PIX      *pix1, *pix2;
PIXA     *pixa1, *pixa2;
PTA      *pta;
PTAA     *ptaa;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PTAA *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);

    if (debugflag) L_INFO("finding text line centers\n", __func__);

        /* Filter to solidify the text lines within the x-height region,
         * and to remove most of the ascenders and descenders.
         * We start with a small vertical opening to remove noise beyond
         * the line that can cause an error in the line end points.
         * The small closing (csize1) is used to bridge the gaps between
         * letters.  The large closing (csize2) bridges the gaps between
         * words; using 1/30 of the page width usually suffices. */
    csize1 = L_MAX(15, w / 80);
    csize2 = L_MAX(40, w / 30);
    snprintf(buf, sizeof(buf), "o1.3 + c%d.1 + o%d.1 + c%d.1",
             csize1, csize1, csize2);
    pix1 = pixMorphSequence(pixs, buf, 0);

        /* Remove the components (e.g., embedded images) that have
         * long vertical runs (>= 50 pixels).  You can't use bounding
         * boxes because connected component b.b. of lines can be quite
         * tall due to slope and curvature.  */
    pix2 = pixMorphSequence(pix1, "e1.50", 0);  /* seed */
    pixSeedfillBinary(pix2, pix2, pix1, 8);  /* tall components */
    pixXor(pix2, pix2, pix1);  /* remove tall */

    if (debugflag) {
        lept_mkdir("lept/dewmod");
        pixWriteDebug("/tmp/lept/dewmod/0011.tif", pix1, IFF_TIFF_G4);
        pixDisplayWithTitle(pix1, 0, 600, "pix1", 1);
        pixWriteDebug("/tmp/lept/dewmod/0012.tif", pix2, IFF_TIFF_G4);
        pixDisplayWithTitle(pix2, 0, 800, "pix2", 1);
    }
    pixDestroy(&pix1);

        /* Get the 8-connected components ... */
    boxa = pixConnComp(pix2, &pixa1, 8);
    pixDestroy(&pix2);
    boxaDestroy(&boxa);
    if (pixaGetCount(pixa1) == 0) {
        pixaDestroy(&pixa1);
        return NULL;
    }

        /* ... and remove the short width and very short height c.c */
    pixa2 = pixaSelectBySize(pixa1, 100, 4, L_SELECT_IF_BOTH,
                                   L_SELECT_IF_GT, NULL);
    if ((nsegs = pixaGetCount(pixa2)) == 0) {
        pixaDestroy(&pixa1);
        pixaDestroy(&pixa2);
        return NULL;
    }
    if (debugflag) {
        pix2 = pixaDisplay(pixa2, w, h);
        pixWriteDebug("/tmp/lept/dewmod/0013.tif", pix2, IFF_TIFF_G4);
        pixDisplayWithTitle(pix2, 0, 1000, "pix2", 1);
        pixDestroy(&pix2);
    }

        /* For each c.c., get the weighted center of each vertical column.
         * The result is a set of points going approximately through
         * the center of the x-height part of the text line.  */
    ptaa = ptaaCreate(nsegs);
    for (i = 0; i < nsegs; i++) {
        pixaGetBoxGeometry(pixa2, i, &bx, &by, NULL, NULL);
        pix2 = pixaGetPix(pixa2, i, L_CLONE);
        pta = dewarpGetMeanVerticals(pix2, bx, by);
        ptaaAddPta(ptaa, pta, L_INSERT);
        pixDestroy(&pix2);
    }
    if (debugflag) {
        pix1 = pixCreateTemplate(pixs);
        pix2 = pixDisplayPtaa(pix1, ptaa);
        pixWriteDebug("/tmp/lept/dewmod/0014.tif", pix2, IFF_PNG);
        pixDisplayWithTitle(pix2, 0, 1200, "pix3", 1);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }

    pixaDestroy(&pixa1);
    pixaDestroy(&pixa2);
    return ptaa;
}


/*!
 * \brief   dewarpGetMeanVerticals()
 *
 * \param[in]    pixs     1 bpp, single c.c.
 * \param[in]    x,y      location of UL corner of pixs, relative to page image
 * \return  pta (mean y-values in component for each x-value,
 *                   both translated by (x,y
 */
static PTA *
dewarpGetMeanVerticals(PIX     *pixs,
                       l_int32  x,
                       l_int32  y)
{
l_int32    w, h, i, j, wpl, sum, count;
l_uint32  *line, *data;
PTA       *pta;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PTA *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    pta = ptaCreate(w);
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    for (j = 0; j < w; j++) {
        line = data;
        sum = count = 0;
        for (i = 0; i < h; i++) {
            if (GET_DATA_BIT(line, j) == 1) {
                sum += i;
                count += 1;
            }
            line += wpl;
        }
        if (count == 0) continue;
        ptaAddPt(pta, x + j, y + (sum / count));
    }

    return pta;
}


/*!
 * \brief   dewarpRemoveShortLines()
 *
 * \param[in]    pixs       1 bpp
 * \param[in]    ptaas      input lines
 * \param[in]    fract      minimum fraction of longest line to keep
 * \param[in]    debugflag
 * \return  ptaad containing only lines of sufficient length,
 *                     or NULL on error
 */
PTAA *
dewarpRemoveShortLines(PIX       *pixs,
                       PTAA      *ptaas,
                       l_float32  fract,
                       l_int32    debugflag)
{
l_int32    w, n, i, index, maxlen, len;
l_float32  minx, maxx;
NUMA      *na, *naindex;
PIX       *pix1, *pix2;
PTA       *pta;
PTAA      *ptaad;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PTAA *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (!ptaas)
        return (PTAA *)ERROR_PTR("ptaas undefined", __func__, NULL);

    pixGetDimensions(pixs, &w, NULL, NULL);
    n = ptaaGetCount(ptaas);
    ptaad = ptaaCreate(n);
    na = numaCreate(n);
    for (i = 0; i < n; i++) {
        pta = ptaaGetPta(ptaas, i, L_CLONE);
        ptaGetRange(pta, &minx, &maxx, NULL, NULL);
        numaAddNumber(na, maxx - minx + 1);
        ptaDestroy(&pta);
    }

        /* Sort by length and find all that are long enough */
    naindex = numaGetSortIndex(na, L_SORT_DECREASING);
    numaGetIValue(naindex, 0, &index);
    numaGetIValue(na, index, &maxlen);
    if (maxlen < 0.5 * w)
        L_WARNING("lines are relatively short\n", __func__);
    pta = ptaaGetPta(ptaas, index, L_CLONE);
    ptaaAddPta(ptaad, pta, L_INSERT);
    for (i = 1; i < n; i++) {
        numaGetIValue(naindex, i, &index);
        numaGetIValue(na, index, &len);
        if (len < fract * maxlen) break;
        pta = ptaaGetPta(ptaas, index, L_CLONE);
        ptaaAddPta(ptaad, pta, L_INSERT);
    }

    if (debugflag) {
        pix1 = pixCopy(NULL, pixs);
        pix2 = pixDisplayPtaa(pix1, ptaad);
        pixDisplayWithTitle(pix2, 0, 200, "pix4", 1);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }

    numaDestroy(&na);
    numaDestroy(&naindex);
    return ptaad;
}


/*!
 * \brief   dewarpGetLineEndPoints()
 *
 * \param[in]    h        height of pixs
 * \param[in]    ptaa     lines
 * \param[out]   pptal    left end points of each line
 * \param[out]   pptar    right end points of each line
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) We require that the set of end points extends over 45% of the
 *          height of the input image, to insure good coverage and
 *          avoid extrapolating the curvature too far beyond the
 *          actual textlines.  Large extrapolations are particularly
 *          dangerous if used as a reference model.  We also require
 *          at least 10 lines of text.
 *      (2) We sort the lines from top to bottom (sort by x in the ptas).
 *      (3) For fitting the endpoints, x = f(y), we transpose x and y.
 *          Thus all these ptas have x and y swapped!
 * </pre>
 */
static l_int32
dewarpGetLineEndPoints(l_int32  h,
                       PTAA    *ptaa,
                       PTA    **pptal,
                       PTA    **pptar)
{
l_int32    i, n, npt, x, y;
l_float32  miny, maxy, ratio;
PTA       *pta, *ptal1, *ptar1;

    if (!pptal || !pptar)
        return ERROR_INT("&ptal and &ptar not both defined", __func__, 1);
    *pptal = *pptar = NULL;
    if (!ptaa)
        return ERROR_INT("ptaa undefined", __func__, 1);

        /* Are there at least 10 lines? */
    n = ptaaGetCount(ptaa);
    if (n < MinLinesForHoriz1) {
        L_INFO("only %d lines; too few\n", __func__, n);
        return 1;
    }

        /* Extract the line end points, and transpose x and y values */
    ptal1 = ptaCreate(n);
    ptar1 = ptaCreate(n);
    for (i = 0; i < n; i++) {
        pta = ptaaGetPta(ptaa, i, L_CLONE);
        ptaGetIPt(pta, 0, &x, &y);
        ptaAddPt(ptal1, y, x);  /* transpose */
        npt = ptaGetCount(pta);
        ptaGetIPt(pta, npt - 1, &x, &y);
        ptaAddPt(ptar1, y, x);  /* transpose */
        ptaDestroy(&pta);
    }

        /* Use the min and max of the y value on the left side. */
    ptaGetRange(ptal1, &miny, &maxy, NULL, NULL);
    ratio = (maxy - miny) / (l_float32)h;
    if (ratio < MinRatioLinesToHeight) {
        L_INFO("ratio lines to height, %f, too small\n", __func__, ratio);
        ptaDestroy(&ptal1);
        ptaDestroy(&ptar1);
        return 1;
    }

        /* Sort from top to bottom */
    *pptal = ptaSort(ptal1, L_SORT_BY_X, L_SORT_INCREASING, NULL);
    *pptar = ptaSort(ptar1, L_SORT_BY_X, L_SORT_INCREASING, NULL);
    ptaDestroy(&ptal1);
    ptaDestroy(&ptar1);
    return 0;
}


/*!
 * \brief   dewarpFilterLineEndPoints()
 *
 * \param[in]    dew
 * \param[in]    ptal     input left end points of each line
 * \param[in]    ptar     input right end points of each line
 * \param[out]   pptalf   filtered left end points
 * \param[out]   pptarf   filtered right end points
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) Avoid confusion with multiple columns by requiring that line
 *          end points be close enough to leftmost and rightmost end points.
 *          Must have at least 8 points on left and right after this step.
 *      (2) Apply second filtering step, find the median positions in
 *          top and bottom halves, and removing end points that are
 *          displaced too much from these in the x direction.
 *          Must have at least 6 points on left and right after this step.
 *      (3) Reminder: x and y in the pta are transposed; think x = f(y).
 * </pre>
 */
static l_int32
dewarpFilterLineEndPoints(L_DEWARP  *dew,
                          PTA       *ptal,
                          PTA       *ptar,
                          PTA      **pptalf,
                          PTA      **pptarf)
{
l_int32    w, i, n;
l_float32  ymin, ymax, xvall, xvalr, yvall, yvalr;
PTA       *ptal1, *ptar1, *ptal2, *ptar2;

    if (!ptal || !ptar)
        return ERROR_INT("ptal or ptar not defined", __func__, 1);
    *pptalf = *pptarf = NULL;

        /* First filter for lines near left and right margins */
    w = pixGetWidth(dew->pixs);
    ptaGetMinMax(ptal, NULL, &ymin, NULL, NULL);
    ptaGetMinMax(ptar, NULL, NULL, NULL, &ymax);
    n = ptaGetCount(ptal);  /* ptar is the same size; at least 10 */
    ptal1 = ptaCreate(n);
    ptar1 = ptaCreate(n);
    for (i = 0; i < n; i++) {
        ptaGetPt(ptal, i, &xvall, &yvall);
        ptaGetPt(ptar, i, &xvalr, &yvalr);
        if (yvall < ymin + 0.20 * (w - ymin) &&
            yvalr > 0.80 * ymax) {
            ptaAddPt(ptal1, xvall, yvall);
            ptaAddPt(ptar1, xvalr, yvalr);
        }
    }
    if (dew->debug) {
        ptaWriteDebug("/tmp/lept/dewdebug/endpts_left2.pta", ptal1, 1);
        ptaWriteDebug("/tmp/lept/dewdebug/endpts_right2.pta", ptar1, 1);
    }

    n = L_MIN(ptaGetCount(ptal1), ptaGetCount(ptar1));
    if (n < MinLinesForHoriz1 - 2) {
        ptaDestroy(&ptal1);
        ptaDestroy(&ptar1);
        L_INFO("First filter: only %d endpoints; needed 8\n", __func__, n);
        return 1;
    }

        /* Remove outlier points */
    ptal2 = dewarpRemoveBadEndPoints(w, ptal1);
    ptar2 = dewarpRemoveBadEndPoints(w, ptar1);
    ptaDestroy(&ptal1);
    ptaDestroy(&ptar1);
    if (!ptal2 || !ptar2) {
        ptaDestroy(&ptal2);
        ptaDestroy(&ptar2);
        L_INFO("Second filter: too few endpoints left after outliers removed\n",
                __func__);
        return 1;
    }
    if (dew->debug) {
        ptaWriteDebug("/tmp/lept/dewdebug/endpts_left3.pta", ptal2, 1);
        ptaWriteDebug("/tmp/lept/dewdebug/endpts_right3.pta", ptar2, 1);
    }

    *pptalf = ptal2;
    *pptarf = ptar2;
    return 0;
}


/*!
 * \brief   dewarpRemoveBadEndPoints()
 *
 * \param[in]   w       width of input image
 * \param[in]   ptas    left or right line end points
 * \return  ptad   filtered left or right end points, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) The input set is sorted by line position (x value).
 *          Break into two (upper and lower); for each find the median
 *          horizontal (y value), and remove all points farther than
 *          a fraction of the image width from this.  Make sure each
 *          part still has at least 3 points, and join the two sections
 *          before returning.
 *      (2) Reminder: x and y in the pta are transposed; think x = f(y).
 * </pre>
 */
static PTA  *
dewarpRemoveBadEndPoints(l_int32  w,
                         PTA     *ptas)
{
l_int32    i, n, nu, nd;
l_float32  rval, xval, yval, delta;
PTA       *ptau1, *ptau2, *ptad1, *ptad2;

    if (!ptas)
        return (PTA *)ERROR_PTR("ptas not defined", __func__, NULL);

    delta = AllowedWidthFract * w;
    n = ptaGetCount(ptas);  /* will be at least 8 */

        /* Check the upper half */
    ptau1 = ptaSelectRange(ptas, 0, n / 2);
    ptaGetRankValue(ptau1, 0.5, NULL, L_SORT_BY_Y, &rval);
    nu = ptaGetCount(ptau1);
    ptau2 = ptaCreate(nu);
    for (i = 0; i < nu; i++) {
        ptaGetPt(ptau1, i, &xval, &yval);  /* transposed */
        if (L_ABS(rval - yval) <= delta)
            ptaAddPt(ptau2, xval, yval);
    }
    ptaDestroy(&ptau1);
    if (ptaGetCount(ptau2) < MinLinesForHoriz2) {
        ptaDestroy(&ptau2);
        L_INFO("Second filter: upper set is too small after outliers removed\n",
               __func__);
        return NULL;
    }

        /* Check the lower half */
    ptad1 = ptaSelectRange(ptas, n / 2 + 1, -1);
    ptaGetRankValue(ptad1, 0.5, NULL, L_SORT_BY_Y, &rval);
    nd = ptaGetCount(ptad1);
    ptad2 = ptaCreate(nd);
    for (i = 0; i < nd; i++) {
        ptaGetPt(ptad1, i, &xval, &yval);  /* transposed */
        if (L_ABS(rval - yval) <= delta)
            ptaAddPt(ptad2, xval, yval);
    }
    ptaDestroy(&ptad1);
    if (ptaGetCount(ptad2) < MinLinesForHoriz2) {
        ptaDestroy(&ptau2);
        ptaDestroy(&ptad2);
        L_INFO("Second filter: lower set is too small after outliers removed\n",
               __func__);
        return NULL;
    }

    ptaJoin(ptau2, ptad2, 0, -1);
    ptaDestroy(&ptad2);
    return ptau2;
}


/*!
 * \brief   dewarpIsLineCoverageValid()
 *
 * \param[in]    ptaa       of validated lines
 * \param[in]    h          height of pix
 * \param[out]   pntop      number of lines in top half
 * \param[out]   pnbot      number of lines in bottom half
 * \param[out]   pytop      location of top line
 * \param[out]   pybot      location of bottom line
 * \return  1 if coverage is valid, 0 if not or on error.
 *
 * <pre>
 * Notes:
 *      (1) The criterion for valid coverage is:
 *          (a) there must be at least 4 lines in each half (top and bottom)
 *              of the image.
 *          (b) the coverage must be at least 50% of the image height
 * </pre>
 */
static l_int32
dewarpIsLineCoverageValid(PTAA     *ptaa,
                          l_int32   h,
                          l_int32  *pntop,
                          l_int32  *pnbot,
                          l_int32  *pytop,
                          l_int32  *pybot)
{
l_int32    i, n, iy, both_halves, ntop, nbot, ytop, ybot, nmin;
l_float32  y, fraction;
NUMA      *na;

    if (!ptaa)
        return ERROR_INT("ptaa not defined", __func__, 0);
    if ((n = ptaaGetCount(ptaa)) == 0)
        return ERROR_INT("ptaa empty", __func__, 0);
    if (h <= 0)
        return ERROR_INT("invalid h", __func__, 0);
    if (!pntop || !pnbot)
        return ERROR_INT("&ntop and &nbot not defined", __func__, 0);
    if (!pytop || !pybot)
        return ERROR_INT("&ytop and &ybot not defined", __func__, 0);

    na = numaCreate(n);
    for (i = 0; i < n; i++) {
        ptaaGetPt(ptaa, i, 0, NULL, &y);
        numaAddNumber(na, y);
    }
    numaSort(na, na, L_SORT_INCREASING);
    for (i = 0, ntop = 0; i < n; i++) {
        numaGetIValue(na, i, &iy);
        if (i == 0) ytop = iy;
        if (i == n - 1) ybot = iy;
        if (iy < 0.5 * h)
            ntop++;
    }
    numaDestroy(&na);
    nbot = n - ntop;
    *pntop = ntop;
    *pnbot = nbot;
    *pytop = ytop;
    *pybot = ybot;
    nmin = 4;  /* minimum number of lines required in each half */
    both_halves = (ntop >= nmin) && (nbot >= nmin);
    fraction = (l_float32)(ybot - ytop) / (l_float32)h;
    if (both_halves && fraction > 0.50)
        return 1;
    return 0;
}


/*!
 * \brief   dewarpLinearLSF()
 *
 * \param[in]    ptad      left or right end points of longest lines
 * \param[out]   pa        coeff a of LSF: y = ax + b
 * \param[out]   pb        coeff b of LSF: y = ax + b
 * \param[out]   pmederr   [optional] median error
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) This is used for finding the left or right sides of the text
 *          block, computed as a best-fit line.  Only the longest lines
 *          are input, so there are no outlier line ends.
 *      (2) The ptas for the end points all have x and y swapped.
 * </pre>
 */
static l_int32
dewarpLinearLSF(PTA        *ptad,
                l_float32  *pa,
                l_float32  *pb,
                l_float32  *pmederr)
{
l_int32    i, n;
l_float32  x, y, xp, c0, c1;
NUMA      *naerr;

    if (pmederr) *pmederr = 0.0;
    if (!pa || !pb)
        return ERROR_INT("not all ptrs are defined", __func__, 1);
    *pa = *pb = 0.0;
    if (!ptad)
        return ERROR_INT("ptad not defined", __func__, 1);

        /* Fit to the longest lines */
    ptaGetLinearLSF(ptad, &c1, &c0, NULL);
    *pa = c1;
    *pb = c0;

        /* Optionally, find the median error */
    if (pmederr) {
        n = ptaGetCount(ptad);
        naerr = numaCreate(n);
        for (i = 0; i < n; i++) {
            ptaGetPt(ptad, i, &y, &xp);
            applyLinearFit(c1, c0, y, &x);
            numaAddNumber(naerr, L_ABS(x - xp));
        }
        numaGetMedian(naerr, pmederr);
        numaDestroy(&naerr);
    }
    return 0;
}


/*!
 * \brief   dewarpQuadraticLSF()
 *
 * \param[in]    ptad      left or right end points of longest lines
 * \param[out]   pa        coeff a of LSF: y = ax^2 + bx + c
 * \param[out]   pb        coeff b of LSF: y = ax^2 + bx + c
 * \param[out]   pc        coeff c of LSF: y = ax^2 + bx + c
 * \param[out]   pmederr   [optional] median error
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) This is used for finding the left or right sides of the text
 *          block, computed as a best-fit quadratic curve.  Only the
 *          longest lines are input, so there are no outlier line ends.
 *      (2) The ptas for the end points all have x and y swapped.
 * </pre>
 */
static l_int32
dewarpQuadraticLSF(PTA        *ptad,
                   l_float32  *pa,
                   l_float32  *pb,
                   l_float32  *pc,
                   l_float32  *pmederr)
{
l_int32    i, n;
l_float32  x, y, xp, c0, c1, c2;
NUMA      *naerr;

    if (pmederr) *pmederr = 0.0;
    if (!pa || !pb || !pc)
        return ERROR_INT("not all ptrs are defined", __func__, 1);
    *pa = *pb = *pc = 0.0;
    if (!ptad)
        return ERROR_INT("ptad not defined", __func__, 1);

        /* Fit to the longest lines */
    ptaGetQuadraticLSF(ptad, &c2, &c1, &c0, NULL);
    *pa = c2;
    *pb = c1;
    *pc = c0;

        /* Optionally, find the median error */
    if (pmederr) {
        n = ptaGetCount(ptad);
        naerr = numaCreate(n);
        for (i = 0; i < n; i++) {
            ptaGetPt(ptad, i, &y, &xp);
            applyQuadraticFit(c2, c1, c0, y, &x);
            numaAddNumber(naerr, L_ABS(x - xp));
        }
        numaGetMedian(naerr, pmederr);
        numaDestroy(&naerr);
    }
    return 0;
}


/*----------------------------------------------------------------------*
 *              Build disparity model for slope near binding            *
 *----------------------------------------------------------------------*/
/*!
 * \brief   dewarpFindHorizSlopeDisparity()
 *
 * \param[in]    dew
 * \param[in]    pixb         1 bpp, with vert and horiz disparity removed
 * \param[in]    fractthresh  threshold fractional difference in density
 * \param[in]    parity       0 if even page, 1 if odd page
 * \return       0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) %fractthresh is a threshold on the fractional difference in stroke
 *          density between between left and right sides.  Process this
 *          disparity only if the absolute value of the fractional
 *          difference equals or exceeds this threshold.
 *      (2) %parity indicates where the binding is: on the left for
 *          %parity == 0 and on the right for %parity == 1.
 *      (3) This takes a 1 bpp %pixb where both vertical and horizontal
 *          disparity have been applied, so the text lines are straight and,
 *          more importantly, the line end points are vertically aligned.
 *          It estimates the foreshortening of the characters on the
 *          binding side, and if significant, computes a one-dimensional
 *          horizontal disparity function to compensate.
 *      (4) The first attempt was to use the average width of the
 *          connected components (c.c.) in vertical slices.  This does not work
 *          reliably, because the horizontal compression of the text is
 *          often accompanied by horizontal joining of c.c.
 *      (5) We use the density of vertical strokes, measured by first using
 *          a vertical opening, which improves the signal.  The result
 *          is relatively insensitive to the size of the opening; we use
 *          a 10-pixel opening.  The relative density is measured by
 *          finding the number of c.c. in a full height sliding window
 *          of width 50 pixels, and compute every 25 pixels.  Similar results
 *          are obtained counting c.c. that either intersect the window
 *          or are fully contained within it.
 *      (6) Debug output goes to /tmp/lept/dewmod/ for collection into a pdf.
 * </pre>
 */
l_ok
dewarpFindHorizSlopeDisparity(L_DEWARP  *dew,
                              PIX       *pixb,
                              l_float32  fractthresh,
                              l_int32    parity)
{
l_int32    i, j, x, n1, n2, nb, ne, count, w, h, ival, prev;
l_int32    istart, iend, first, last, x0, x1, nx, ny;
l_float32  fract, delta, sum, aveval, fval, del, denom;
l_float32  ca, cb, cc, cd, ce, y;
BOX       *box;
BOXA      *boxa1, *boxa2;
GPLOT     *gplot;
NUMA      *na1, *na2, *na3, *na4, *nasum;
PIX       *pix1;
PTA       *pta1;
FPIX      *fpix;

    if (!dew)
        return ERROR_INT("dew not defined", __func__, 1);
    if (!dew->vvalid || !dew->hvalid)
        return ERROR_INT("invalid vert or horiz disparity model", __func__, 1);
    if (!pixb || pixGetDepth(pixb) != 1)
        return ERROR_INT("pixb not defined or not 1 bpp", __func__, 1);

    if (dew->debug) L_INFO("finding slope horizontal disparity\n", __func__);

        /* Find the bounding boxes of the vertical strokes; remove noise */
    pix1 = pixMorphSequence(pixb, "o1.10", 0);
    pixDisplay(pix1, 100, 100);
    boxa1 = pixConnCompBB(pix1, 4);
    boxa2 = boxaSelectBySize(boxa1, 0, 5, L_SELECT_HEIGHT, L_SELECT_IF_GT,
                             NULL);
    nb = boxaGetCount(boxa2);
    lept_stderr("number of components: %d\n", nb);
    boxaDestroy(&boxa1);

        /* Estimate the horizontal density of vertical strokes */
    na1 = numaCreate(0);
    numaSetParameters(na1, 0, 25);
    pixGetDimensions(pixb, &w, &h, NULL);
    for (x = 0; x + 50 < w; x += 25) {
        box = boxCreate(x, 0, 50, h);
        boxaContainedInBoxCount(boxa2, box, &count);
        numaAddNumber(na1, count);
        boxDestroy(&box);
    }
    if (dew->debug) {
        lept_mkdir("lept/dew");
        gplotSimple1(na1, GPLOT_PNG, "/tmp/lept/dew/0091", NULL);
        lept_mv("/tmp/lept/dew/0091.png", "lept/dewmod", NULL, NULL);
        pixWriteDebug("/tmp/lept/dewmod/0090.png", pix1, IFF_PNG);
    }
    pixDestroy(&pix1);
    boxaDestroy(&boxa2);

        /* Find the left and right end local maxima; if the difference
         * is small, quit.  */
    n1 = numaGetCount(na1);
    prev = 0;
    istart = 0;
    first = 0;
    for (i = 0; i < n1; i++) {
        numaGetIValue(na1, i, &ival);
        if (ival >= prev) {
            prev = ival;
            continue;
        } else {
            first = prev;
            istart = i - 1;
            break;
        }
    }
    prev = 0;
    last = 0;
    iend = n1 - 1;
    for (i = n1 - 1; i >= 0; i--) {
        numaGetIValue(na1, i, &ival);
        if (ival >= prev) {
            prev = ival;
            continue;
        } else {
            last = prev;
            iend = i + 1;
            break;
        }
    }
    na2 = numaClipToInterval(na1, istart, iend);
    numaDestroy(&na1);
    n2 = numaGetCount(na2);
    delta = (parity == 0) ? last - first : first - last;
    denom = L_MAX(1.0, (l_float32)(L_MIN(first, last)));
    fract = (l_float32)delta / denom;
    if (dew->debug) {
        L_INFO("Slope-disparity: first = %d, last = %d, fract = %7.3f\n",
               __func__, first, last, fract);
        gplotSimple1(na2, GPLOT_PNG, "/tmp/lept/dew/0092", NULL);
        lept_mv("/tmp/lept/dew/0092.png", "lept/dewmod", NULL, NULL);
    }
    if (fract < fractthresh) {
        L_INFO("Small slope-disparity: first = %d, last = %d, fract = %7.3f\n",
               __func__, first, last, fract);
        numaDestroy(&na2);
        return 0;
    }

        /* Find the density far from the binding, and normalize to 1.  */
    ne = n2 - n2 % 2;
    if (parity == 0)
        numaGetSumOnInterval(na2, 0, ne / 2 - 1, &sum);
    else  /* parity == 1 */
        numaGetSumOnInterval(na2, ne / 2, ne - 1, &sum);
    denom = L_MAX(1.0, (l_float32)(ne / 2));
    aveval = sum / denom;
    na3 = numaMakeConstant(aveval, n2);
    numaArithOp(na2, na2, na3, L_ARITH_DIVIDE);
    numaDestroy(&na3);
    if (dew->debug) {
        L_INFO("Average background density: %5.1f\n", __func__, aveval);
        gplotSimple1(na2, GPLOT_PNG, "/tmp/lept/dew/0093", NULL);
        lept_mv("/tmp/lept/dew/0093.png", "lept/dewmod", NULL, NULL);
    }

        /* Fit the normalized density curve to a quartic */
    pta1 = numaConvertToPta1(na2);
    ptaWriteStream(stderr, pta1, 0);
/*    ptaGetQuadraticLSF(pta1, NULL, NULL, NULL, &na3); */
    ptaGetQuarticLSF(pta1, &ca, &cb, &cc, &cd, &ce, &na3);
    ptaGetArrays(pta1, &na4, NULL);
    if (dew->debug) {
        gplot = gplotSimpleXY1(na4, na3, GPLOT_LINES, GPLOT_PNG,
                              "/tmp/lept/dew/0094", NULL);
        gplotDestroy(&gplot);
        lept_mv("/tmp/lept/dew/0094.png", "lept/dewmod", NULL, NULL);
    }
    ptaDestroy(&pta1);

        /* Integrate from the high point down to 1 (or v.v) to get the
         * disparity needed to make the density constant. */
    nasum = numaMakeConstant(0, w);  /* area under the curve above 1.0 */
    if (parity == 0) {
        for (i = n2 - 1; i >= 0; i--) {
            numaGetFValue(na3, i, &fval);
            if (fval < 1.0) break;
        }
        numaGetIValue(na4, i + 1, &x0);
        numaGetIValue(na4, n2 - 1, &x1);
        numaSetParameters(nasum, x0, 1);
        sum = 0.0;
        for (x = x0; x < x1; x++) {
            applyQuarticFit(ca, cb, cc, cd, ce, (l_float32)x, &y);
            sum += (y - 1.0);
            numaReplaceNumber(nasum, x, sum);
        }
        for (x = x1; x < w; x++)
            numaReplaceNumber(nasum, x, sum);
    } else {  /* parity == 1 */
        for (i = 0; i < n2; i++) {
            numaGetFValue(na3, i, &fval);
            if (fval < 1.0) break;
        }
        numaGetIValue(na4, 0, &x0);
        numaGetIValue(na4, i - 1, &x1);
        numaSetParameters(nasum, x0, 1);
        sum = 0.0;
        for (x = x1; x >= x0; x--) {
            applyQuarticFit(ca, cb, cc, cd, ce, (l_float32)x, &y);
            sum += (y - 1.0);
            numaReplaceNumber(nasum, x, sum);
        }
        for (x = x0; x >= 0; x--)
            numaReplaceNumber(nasum, x, sum);
    }

        /* Save the result in a fpix at the specified subsampling  */
    nx = dew->nx;
    ny = dew->ny;
    fpix = fpixCreate(nx, ny);
    del = (l_float32)w / (l_float32)nx;
    for (i = 0; i < ny; i++) {
        for (j = 0; j < nx; j++) {
            x = del * j;
            numaGetFValue(nasum, x, &fval);
            fpixSetPixel(fpix, j, i, fval);
        }
    }
    dew->sampydispar = fpix;
    dew->ysuccess = 1;

    numaDestroy(&na2);
    numaDestroy(&na3);
    numaDestroy(&na4);
    numaDestroy(&nasum);
    return 0;
}


/*----------------------------------------------------------------------*
 *                      Build line disparity model                     *
 *----------------------------------------------------------------------*/
/*!
 * \brief   dewarpBuildLineModel()
 *
 * \param[in]    dew
 * \param[in]    opensize    size of opening to remove perpendicular lines
 * \param[in]    debugfile   use NULL to skip writing this
 * \return  0 if OK, 1 if unable to build the model or on error
 *
 * <pre>
 * Notes:
 *      (1) This builds the horizontal and vertical disparity arrays
 *          for an input of ruled lines, typically for calibration.
 *          In book scanning, you could lay the ruled paper over a page.
 *          Then for that page and several below it, you can use the
 *          disparity correction of the line model to dewarp the pages.
 *      (2) The dew has been initialized with the image of ruled lines.
 *          These lines must be continuous, but we do a small amount
 *          of pre-processing here to insure that.
 *      (3) %opensize is typically about 8.  It must be larger than
 *          the thickness of the lines to be extracted.  This is the
 *          default value, which is applied if %opensize < 3.
 *      (4) Sets vsuccess = 1 and hsuccess = 1 if the vertical and/or
 *          horizontal disparity arrays build.
 *      (5) Similar to dewarpBuildPageModel(), except here the vertical
 *          and horizontal disparity arrays are both built from ruled lines.
 *          See notes there.
 * </pre>
 */
l_ok
dewarpBuildLineModel(L_DEWARP    *dew,
                     l_int32      opensize,
                     const char  *debugfile)
{
char     buf[64];
l_int32  i, j, bx, by, ret, nlines;
BOXA    *boxa;
PIX     *pixs, *pixh, *pixv, *pix, *pix1, *pix2;
PIXA    *pixa1, *pixa2;
PTA     *pta;
PTAA    *ptaa1, *ptaa2;

    if (!dew)
        return ERROR_INT("dew not defined", __func__, 1);
    if (opensize < 3) {
        L_WARNING("opensize should be >= 3; setting to 8\n", __func__);
        opensize = 8;  /* default */
    }

    dew->debug = (debugfile) ? 1 : 0;
    dew->vsuccess = dew->hsuccess = 0;
    pixs = dew->pixs;
    if (debugfile) {
        lept_rmdir("lept/dewline");  /* erase previous images */
        lept_mkdir("lept/dewline");
        lept_rmdir("lept/dewmod");  /* erase previous images */
        lept_mkdir("lept/dewmod");
        lept_mkdir("lept/dewarp");
        pixDisplayWithTitle(pixs, 0, 0, "pixs", 1);
        pixWriteDebug("/tmp/lept/dewline/001.png", pixs, IFF_PNG);
    }

        /* Extract and solidify the horizontal and vertical lines.  We use
         * the horizontal lines to derive the vertical disparity, and v.v.
         * Both disparities are computed using the vertical disparity
         * algorithm; the horizontal disparity is found from the
         * vertical lines by rotating them clockwise by 90 degrees.
         * On the first pass, we compute the horizontal disparity, from
         * the vertical lines, by rotating them by 90 degrees (so they
         * are horizontal) and computing the vertical disparity on them;
         * we rotate the resulting fpix array for the horizontal disparity
         * back by -90 degrees.  On the second pass, we compute the vertical
         * disparity from the horizontal lines in the usual fashion. */
    snprintf(buf, sizeof(buf), "d1.3 + c%d.1 + o%d.1", opensize - 2, opensize);
    pixh = pixMorphSequence(pixs, buf, 0);  /* horiz */
    snprintf(buf, sizeof(buf), "d3.1 + c1.%d + o1.%d", opensize - 2, opensize);
    pix1 = pixMorphSequence(pixs, buf, 0);  /* vert */
    pixv = pixRotateOrth(pix1, 1); /* vert rotated to horizontal */
    pixa1 = pixaCreate(2);
    pixaAddPix(pixa1, pixv, L_INSERT);  /* get horizontal disparity first */
    pixaAddPix(pixa1, pixh, L_INSERT);
    pixDestroy(&pix1);

        /*--------------------------------------------------------------*/
        /*    Process twice: first for horiz disparity, then for vert   */
        /*--------------------------------------------------------------*/
    for (i = 0; i < 2; i++) {
        pix = pixaGetPix(pixa1, i, L_CLONE);
        pixDisplay(pix, 0, 900);
        boxa = pixConnComp(pix, &pixa2, 8);
        nlines = boxaGetCount(boxa);
        boxaDestroy(&boxa);
        if (nlines < dew->minlines) {
            L_WARNING("only found %d lines\n", __func__, nlines);
            pixDestroy(&pix);
            pixaDestroy(&pixa1);
            continue;
        }

            /* Identify the pixels along the skeleton of each line */
        ptaa1 = ptaaCreate(nlines);
        for (j = 0; j < nlines; j++) {
            pixaGetBoxGeometry(pixa2, j, &bx, &by, NULL, NULL);
            pix1 = pixaGetPix(pixa2, j, L_CLONE);
            pta = dewarpGetMeanVerticals(pix1, bx, by);
            ptaaAddPta(ptaa1, pta, L_INSERT);
            pixDestroy(&pix1);
        }
        pixaDestroy(&pixa2);
        if (debugfile) {
            pix1 = pixConvertTo32(pix);
            pix2 = pixDisplayPtaa(pix1, ptaa1);
            snprintf(buf, sizeof(buf), "/tmp/lept/dewline/%03d.png", 2 + 2 * i);
            pixWriteDebug(buf, pix2, IFF_PNG);
            pixDestroy(&pix1);
            pixDestroy(&pix2);
        }

            /* Remove all lines that are not at least 0.75 times the length
             * of the longest line. */
        ptaa2 = dewarpRemoveShortLines(pix, ptaa1, 0.75, DEBUG_SHORT_LINES);
        if (debugfile) {
            pix1 = pixConvertTo32(pix);
            pix2 = pixDisplayPtaa(pix1, ptaa2);
            snprintf(buf, sizeof(buf), "/tmp/lept/dewline/%03d.png", 3 + 2 * i);
            pixWriteDebug(buf, pix2, IFF_PNG);
            pixDestroy(&pix1);
            pixDestroy(&pix2);
        }
        ptaaDestroy(&ptaa1);
        nlines = ptaaGetCount(ptaa2);
        if (nlines < dew->minlines) {
            pixDestroy(&pix);
            ptaaDestroy(&ptaa2);
            L_WARNING("%d lines: too few to build model\n", __func__, nlines);
            continue;
        }

            /* Get the sampled 'vertical' disparity from the textline
             * centers.  The disparity array will push pixels vertically
             * so that each line is flat and centered at the y-position
             * of the mid-point. */
        ret = dewarpFindVertDisparity(dew, ptaa2, 1 - i);

            /* If i == 0, move the result to the horizontal disparity,
             * rotating it back by -90 degrees. */
        if (i == 0) {  /* horizontal disparity, really */
            if (ret) {
                L_WARNING("horizontal disparity not built\n", __func__);
            } else {
                L_INFO("hsuccess = 1\n", __func__);
                dew->samphdispar = fpixRotateOrth(dew->sampvdispar, 3);
                fpixDestroy(&dew->sampvdispar);
                if (debugfile)
                    lept_mv("/tmp/lept/dewarp/vert_disparity.pdf",
                            "lept/dewarp", "horiz_disparity.pdf", NULL);
            }
            dew->hsuccess = dew->vsuccess;
            dew->vsuccess = 0;
        } else {  /* i == 1 */
            if (ret)
                L_WARNING("vertical disparity not built\n", __func__);
            else
                L_INFO("vsuccess = 1\n", __func__);
        }
        ptaaDestroy(&ptaa2);
        pixDestroy(&pix);
    }
    pixaDestroy(&pixa1);

        /* Debug output */
    if (debugfile) {
        if (dew->vsuccess == 1) {
            dewarpPopulateFullRes(dew, NULL, 0, 0);
            pix1 = fpixRenderContours(dew->fullvdispar, 3.0f, 0.15f);
            pixWriteDebug("/tmp/lept/dewline/006.png", pix1, IFF_PNG);
            pixDisplay(pix1, 1000, 0);
            pixDestroy(&pix1);
        }
        if (dew->hsuccess == 1) {
            pix1 = fpixRenderContours(dew->fullhdispar, 3.0f, 0.15f);
            pixWriteDebug("/tmp/lept/dewline/007.png", pix1, IFF_PNG);
            pixDisplay(pix1, 1000, 0);
            pixDestroy(&pix1);
        }
        convertFilesToPdf("/tmp/lept/dewline", NULL, 135, 1.0, 0, 0,
                          "Dewarp Build Line Model", debugfile);
        lept_stderr("pdf file: %s\n", debugfile);
    }

    return 0;
}


/*----------------------------------------------------------------------*
 *                         Query model status                           *
 *----------------------------------------------------------------------*/
/*!
 * \brief   dewarpaModelStatus()
 *
 * \param[in]    dewa
 * \param[in]    pageno
 * \param[out]   pvsuccess    [optional] 1 on success
 * \param[out]   phsuccess    [optional] 1 on success
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This tests if a model has been built, not if it is valid.
 * </pre>
 */
l_ok
dewarpaModelStatus(L_DEWARPA  *dewa,
                   l_int32     pageno,
                   l_int32    *pvsuccess,
                   l_int32    *phsuccess)
{
L_DEWARP  *dew;

    if (pvsuccess) *pvsuccess = 0;
    if (phsuccess) *phsuccess = 0;
    if (!dewa)
        return ERROR_INT("dewa not defined", __func__, 1);

    if ((dew = dewarpaGetDewarp(dewa, pageno)) == NULL)
        return ERROR_INT("dew not retrieved", __func__, 1);
    if (pvsuccess) *pvsuccess = dew->vsuccess;
    if (phsuccess) *phsuccess = dew->hsuccess;
    return 0;
}


/*----------------------------------------------------------------------*
 *                          Rendering helpers                           *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixRenderMidYs()
 *
 * \param[in]    pixs      32 bpp
 * \param[in]    namidys   y location of reference lines for vertical disparity
 * \param[in]    linew     width of rendered line; typ 2
 * \return  0 if OK, 1 on error
 */
static l_int32
pixRenderMidYs(PIX     *pixs,
               NUMA    *namidys,
               l_int32  linew)
{
l_int32   i, n, w, yval, rval, gval, bval;
PIXCMAP  *cmap;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!namidys)
        return ERROR_INT("namidys not defined", __func__, 1);

    w = pixGetWidth(pixs);
    n = numaGetCount(namidys);
    cmap = pixcmapCreateRandom(8, 0, 0);
    for (i = 0; i < n; i++) {
        pixcmapGetColor(cmap, i % 256, &rval, &gval, &bval);
        numaGetIValue(namidys, i, &yval);
        pixRenderLineArb(pixs, 0, yval, w, yval, linew, rval, gval, bval);
    }
    pixcmapDestroy(&cmap);
    return 0;
}


/*!
 * \brief   pixRenderHorizEndPoints()
 *
 * \param[in]    pixs     32 bpp
 * \param[in]    ptal     left side line end points
 * \param[in]    ptar     right side line end points
 * \param[in]    color    0xrrggbb00
 * \return  0 if OK, 1 on error
 */
static l_int32
pixRenderHorizEndPoints(PIX      *pixs,
                        PTA      *ptal,
                        PTA      *ptar,
                        l_uint32  color)
{
PIX      *pixcirc;
PTA      *ptalt, *ptart, *ptacirc;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!ptal || !ptar)
        return ERROR_INT("ptal and ptar not both defined", __func__, 1);

    ptacirc = generatePtaFilledCircle(5);
    pixcirc = pixGenerateFromPta(ptacirc, 11, 11);
    ptalt = ptaTranspose(ptal);
    ptart = ptaTranspose(ptar);

    pixDisplayPtaPattern(pixs, pixs, ptalt, pixcirc, 5, 5, color);
    pixDisplayPtaPattern(pixs, pixs, ptart, pixcirc, 5, 5, color);
    ptaDestroy(&ptacirc);
    ptaDestroy(&ptalt);
    ptaDestroy(&ptart);
    pixDestroy(&pixcirc);
    return 0;
}

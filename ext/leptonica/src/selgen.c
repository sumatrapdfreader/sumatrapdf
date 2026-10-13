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
 * \file selgen.c
 * <pre>
 *
 *      This file contains functions that generate hit-miss Sels
 *      for doing a loose match to a small bitmap.  The hit-miss
 *      Sel is made from a given bitmap.  Several "knobs"
 *      are available to control the looseness of the match.
 *      In general, a tight match will have fewer false positives
 *      (bad matches) but more false negatives (missed patterns).
 *      The values to be used depend on the quality and variation
 *      of the image in which the pattern is to be searched,
 *      and the relative penalties of false positives and
 *      false negatives.  Default values for the three knobs --
 *      minimum distance to boundary pixels, number of extra pixels
 *      added to selected sides, and minimum acceptable runlength
 *      in eroded version -- are provided.
 *
 *      The generated hit-miss Sels can always be used in the
 *      rasterop implementation of binary morphology (in morph.h).
 *      If they are small enough (not more than 31 pixels extending
 *      in any direction from the Sel origin), they can also be used
 *      to auto-generate dwa code (fmorphauto.c).
 *
 *      In production, use pixGenerateSelBoundary().
 *
 *      Generate a subsampled structuring element
 *            SEL     *pixGenerateSelBoundary()
 *            SEL     *pixGenerateSelWithRuns()
 *            SEL     *pixGenerateSelRandom()
 *
 *      Accumulate data on runs along lines
 *            NUMA    *pixGetRunCentersOnLine()
 *            NUMA    *pixGetRunsOnLine()
 *
 *      Subsample boundary pixels in relatively ordered way
 *            PTA     *pixSubsampleBoundaryPixels()
 *            PTA     *adjacentOnPixelInRaster()
 *
 *      Display generated sel with originating image
 *            PIX     *pixDisplayHitMissSel()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

    /* Default minimum distance of a hit-miss pixel element to
     * a boundary pixel of its color. */
static const l_int32  DefaultDistanceToBoundary = 1;
static const l_int32  MaxDistanceToBoundary = 4;

    /* Default min runlength to accept a hit or miss element located
     * at its center */
static const l_int32  DefaultMinRunlength = 3;

    /* Default scalefactor for displaying image and hit-miss sel
     * that is derived from it */
static const l_int32  DefaultSelScalefactor = 7;
static const l_int32  MaxSelScalefactor = 31;  /* should be big enough */

#ifndef  NO_CONSOLE_IO
#define  DEBUG_DISPLAY_HM_SEL   0
#endif  /* ~NO_CONSOLE_IO */


/*-----------------------------------------------------------------*
 *           Generate a subsampled structuring element             *
 *-----------------------------------------------------------------*/

/*!
 * \brief   pixGenerateSelBoundary()
 *
 * \param[in]    pixs        1 bpp, typically small, to be used as a pattern
 * \param[in]    hitdist     min distance from fg boundary pixel
 * \param[in]    missdist    min distance from bg boundary pixel
 * \param[in]    hitskip     number of boundary pixels skipped between hits
 * \param[in]    missskip    number of boundary pixels skipped between misses
 * \param[in]    topflag     flag for extra pixels of bg added above
 * \param[in]    botflag     flag for extra pixels of bg added below
 * \param[in]    leftflag    flag for extra pixels of bg added to left
 * \param[in]    rightflag   flag for extra pixels of bg added to right
 * \param[out]   ppixe       [optional] input pix expanded by extra pixels
 * \return  sel hit-miss for input pattern, or NULL on error
 *
 * <pre>
 * Notes:
 *    (1) All fg elements selected are exactly hitdist pixels away from
 *        the nearest fg boundary pixel, and ditto for bg elements.
 *        Valid inputs of hitdist and missdist are 0, 1, 2, 3 and 4.
 *        For example, a hitdist of 0 puts the hits at the fg boundary.
 *        Usually, the distances should be > 0 avoid the effect of
 *        noise at the boundary.
 *    (2) Set hitskip < 0 if no hits are to be used.  Ditto for missskip.
 *        If both hitskip and missskip are < 0, the sel would be empty,
 *        and NULL is returned.
 *    (3) The 4 flags determine whether the sel is increased on that side
 *        to allow bg misses to be placed all along that boundary.
 *        The increase in sel size on that side is the minimum necessary
 *        to allow the misses to be placed at mindist.  For text characters,
 *        the topflag and botflag are typically set to 1, and the leftflag
 *        and rightflag to 0.
 *    (4) The input pix, as extended by the extra pixels on selected sides,
 *        can optionally be returned.  For debugging, call
 *        pixDisplayHitMissSel() to visualize the hit-miss sel superimposed
 *        on the generating bitmap.
 *    (5) This is the best of the three sel generators: it gives the most
 *        flexibility with the smallest number of hits and misses.
 *        The other two generators are presented as examples of other
 *        approaches, but they should not be used in production.
 * </pre>
 */
SEL *
pixGenerateSelBoundary(PIX     *pixs,
                       l_int32  hitdist,
                       l_int32  missdist,
                       l_int32  hitskip,
                       l_int32  missskip,
                       l_int32  topflag,
                       l_int32  botflag,
                       l_int32  leftflag,
                       l_int32  rightflag,
                       PIX      **ppixe)
{
l_int32  ws, hs, w, h, x, y, ix, iy, i, npt;
PIX     *pixt1, *pixt2, *pixt3, *pixfg, *pixbg;
SEL     *selh, *selm, *sel_3, *sel;
PTA     *ptah = NULL, *ptam = NULL;

    if (ppixe) *ppixe = NULL;
    if (!pixs)
        return (SEL *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (SEL *)ERROR_PTR("pixs not 1 bpp", __func__, NULL);
    if (hitdist < 0 || hitdist > 4 || missdist < 0 || missdist > 4)
        return (SEL *)ERROR_PTR("dist not in {0 .. 4}", __func__, NULL);
    if (hitskip < 0 && missskip < 0)
        return (SEL *)ERROR_PTR("no hits or misses", __func__, NULL);

        /* Locate the foreground */
    pixClipToForeground(pixs, &pixt1, NULL);
    if (!pixt1)
        return (SEL *)ERROR_PTR("pixt1 not made", __func__, NULL);
    ws = pixGetWidth(pixt1);
    hs = pixGetHeight(pixt1);
    w = ws;
    h = hs;

        /* Crop out a region including the foreground, and add pixels
         * on sides depending on the side flags */
    if (topflag || botflag || leftflag || rightflag) {
        x = y = 0;
        if (topflag) {
            h += missdist + 1;
            y = missdist + 1;
        }
        if (botflag)
            h += missdist + 1;
        if (leftflag) {
            w += missdist + 1;
            x = missdist + 1;
        }
        if (rightflag)
            w += missdist + 1;
        pixt2 = pixCreate(w, h, 1);
        pixRasterop(pixt2, x, y, ws, hs, PIX_SRC, pixt1, 0, 0);
    } else {
        pixt2 = pixClone(pixt1);
    }
    if (ppixe)
        *ppixe = pixClone(pixt2);
    pixDestroy(&pixt1);

        /* Identify fg and bg pixels that are exactly hitdist and
         * missdist (rsp) away from the boundary pixels in their set.
         * Then get a subsampled set of these points. */
    sel_3 = selCreateBrick(3, 3, 1, 1, SEL_HIT);
    if (hitskip >= 0) {
        selh = selCreateBrick(2 * hitdist + 1, 2 * hitdist + 1,
                              hitdist, hitdist, SEL_HIT);
        pixt3 = pixErode(NULL, pixt2, selh);
        pixfg = pixErode(NULL, pixt3, sel_3);
        pixXor(pixfg, pixfg, pixt3);
        ptah = pixSubsampleBoundaryPixels(pixfg, hitskip);
        pixDestroy(&pixt3);
        pixDestroy(&pixfg);
        selDestroy(&selh);
    }
    if (missskip >= 0) {
        selm = selCreateBrick(2 * missdist + 1, 2 * missdist + 1,
                              missdist, missdist, SEL_HIT);
        pixt3 = pixDilate(NULL, pixt2, selm);
        pixbg = pixDilate(NULL, pixt3, sel_3);
        pixXor(pixbg, pixbg, pixt3);
        ptam = pixSubsampleBoundaryPixels(pixbg, missskip);
        pixDestroy(&pixt3);
        pixDestroy(&pixbg);
        selDestroy(&selm);
    }
    selDestroy(&sel_3);
    pixDestroy(&pixt2);

        /* Generate the hit-miss sel from these point */
    sel = selCreateBrick(h, w, h / 2, w / 2, SEL_DONT_CARE);
    if (hitskip >= 0) {
        npt = ptaGetCount(ptah);
        for (i = 0; i < npt; i++) {
            ptaGetIPt(ptah, i, &ix, &iy);
            selSetElement(sel, iy, ix, SEL_HIT);
        }
    }
    if (missskip >= 0) {
        npt = ptaGetCount(ptam);
        for (i = 0; i < npt; i++) {
            ptaGetIPt(ptam, i, &ix, &iy);
            selSetElement(sel, iy, ix, SEL_MISS);
        }
    }

    ptaDestroy(&ptah);
    ptaDestroy(&ptam);
    return sel;
}


/*!
 * \brief   pixGenerateSelWithRuns()
 *
 * \param[in]    pixs        1 bpp, typically small, to be used as a pattern
 * \param[in]    nhlines     number of hor lines along which elements are found
 * \param[in]    nvlines     number of vert lines along which elements are found
 * \param[in]    distance    min distance from boundary pixel; use 0 for default
 * \param[in]    minlength   min runlength to set hit or miss; use 0 for default
 * \param[in]    toppix      number of extra pixels of bg added above
 * \param[in]    botpix      number of extra pixels of bg added below
 * \param[in]    leftpix     number of extra pixels of bg added to left
 * \param[in]    rightpix    number of extra pixels of bg added to right
 * \param[out]   ppixe       [optional] input pix expanded by extra pixels
 * \return  sel hit-miss for input pattern, or NULL on error
 *
 * <pre>
 * Notes:
 *    (1) The horizontal and vertical lines along which elements are
 *        selected are roughly equally spaced.  The actual locations of
 *        the hits and misses are the centers of respective run-lengths.
 *    (2) No elements are selected that are less than 'distance' pixels away
 *        from a boundary pixel of the same color.  This makes the
 *        match much more robust to edge noise.  Valid inputs of
 *        'distance' are 0, 1, 2, 3 and 4.  If distance is either 0 or
 *        greater than 4, we reset it to the default value.
 *    (3) The 4 numbers for adding rectangles of pixels outside the fg
 *        can be use if the pattern is expected to be surrounded by bg
 *        (white) pixels.  On the other hand, if the pattern may be near
 *        other fg (black) components on some sides, use 0 for those sides.
 *    (4) The pixels added to a side allow you to have miss elements there.
 *        There is a constraint between distance, minlength, and
 *        the added pixels for this to work.  We illustrate using the
 *        default values.  If you add 5 pixels to the top, and use a
 *        distance of 1, then you end up with a vertical run of at least
 *        4 bg pixels along the top edge of the image.  If you use a
 *        minimum runlength of 3, each vertical line will always find
 *        a miss near the center of its run.  However, if you use a
 *        minimum runlength of 5, you will not get a miss on every vertical
 *        line.  As another example, if you have 7 added pixels and a
 *        distance of 2, you can use a runlength up to 5 to guarantee
 *        that the miss element is recorded.  We give a warning if the
 *        constraint does not guarantee a miss element outside the
 *        image proper.
 *    (5) The input pix, as extended by the extra pixels on selected sides,
 *        can optionally be returned.  For debugging, call
 *        pixDisplayHitMissSel() to visualize the hit-miss sel superimposed
 *        on the generating bitmap.
 *    (6) This method is inferior to the boundary method.
 * </pre>
 */
SEL *
pixGenerateSelWithRuns(PIX     *pixs,
                       l_int32  nhlines,
                       l_int32  nvlines,
                       l_int32  distance,
                       l_int32  minlength,
                       l_int32  toppix,
                       l_int32  botpix,
                       l_int32  leftpix,
                       l_int32  rightpix,
                       PIX    **ppixe)
{
l_int32    ws, hs, w, h, x, y, xval, yval, i, j, nh, nm;
l_float32  delh, delw;
NUMA      *nah, *nam;
PIX       *pixt1, *pixt2, *pixfg, *pixbg;
PTA       *ptah, *ptam;
SEL       *seld, *sel;

    if (ppixe) *ppixe = NULL;
    if (!pixs)
        return (SEL *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (SEL *)ERROR_PTR("pixs not 1 bpp", __func__, NULL);
    if (nhlines < 1 && nvlines < 1)
        return (SEL *)ERROR_PTR("nvlines and nhlines both < 1", __func__, NULL);

    if (distance <= 0)
        distance = DefaultDistanceToBoundary;
    if (minlength <= 0)
        minlength = DefaultMinRunlength;
    if (distance > MaxDistanceToBoundary) {
        L_WARNING("distance too large; setting to max value\n", __func__);
        distance = MaxDistanceToBoundary;
    }

        /* Locate the foreground */
    pixClipToForeground(pixs, &pixt1, NULL);
    if (!pixt1)
        return (SEL *)ERROR_PTR("pixt1 not made", __func__, NULL);
    ws = pixGetWidth(pixt1);
    hs = pixGetHeight(pixt1);
    w = ws;
    h = hs;

        /* Crop out a region including the foreground, and add pixels
         * on sides depending on the side flags */
    if (toppix || botpix || leftpix || rightpix) {
        x = y = 0;
        if (toppix) {
            h += toppix;
            y = toppix;
            if (toppix < distance + minlength)
                L_WARNING("no miss elements in added top pixels\n", __func__);
        }
        if (botpix) {
            h += botpix;
            if (botpix < distance + minlength)
                L_WARNING("no miss elements in added bot pixels\n", __func__);
        }
        if (leftpix) {
            w += leftpix;
            x = leftpix;
            if (leftpix < distance + minlength)
                L_WARNING("no miss elements in added left pixels\n", __func__);
        }
        if (rightpix) {
            w += rightpix;
            if (rightpix < distance + minlength)
                L_WARNING("no miss elements in added right pixels\n", __func__);
        }
        pixt2 = pixCreate(w, h, 1);
        pixRasterop(pixt2, x, y, ws, hs, PIX_SRC, pixt1, 0, 0);
    } else {
        pixt2 = pixClone(pixt1);
    }
    if (ppixe)
        *ppixe = pixClone(pixt2);
    pixDestroy(&pixt1);

        /* Identify fg and bg pixels that are at least 'distance' pixels
         * away from the boundary pixels in their set */
    seld = selCreateBrick(2 * distance + 1, 2 * distance + 1,
                          distance, distance, SEL_HIT);
    pixfg = pixErode(NULL, pixt2, seld);
    pixbg = pixDilate(NULL, pixt2, seld);
    pixInvert(pixbg, pixbg);
    selDestroy(&seld);
    pixDestroy(&pixt2);

        /* Accumulate hit and miss points */
    ptah = ptaCreate(0);
    ptam = ptaCreate(0);
    if (nhlines >= 1) {
        delh = (l_float32)h / (l_float32)(nhlines + 1);
        for (i = 0, y = 0; i < nhlines; i++) {
            y += (l_int32)(delh + 0.5);
            nah = pixGetRunCentersOnLine(pixfg, -1, y, minlength);
            nam = pixGetRunCentersOnLine(pixbg, -1, y, minlength);
            nh = numaGetCount(nah);
            nm = numaGetCount(nam);
            for (j = 0; j < nh; j++) {
                numaGetIValue(nah, j, &xval);
                ptaAddPt(ptah, xval, y);
            }
            for (j = 0; j < nm; j++) {
                numaGetIValue(nam, j, &xval);
                ptaAddPt(ptam, xval, y);
            }
            numaDestroy(&nah);
            numaDestroy(&nam);
        }
    }
    if (nvlines >= 1) {
        delw = (l_float32)w / (l_float32)(nvlines + 1);
        for (i = 0, x = 0; i < nvlines; i++) {
            x += (l_int32)(delw + 0.5);
            nah = pixGetRunCentersOnLine(pixfg, x, -1, minlength);
            nam = pixGetRunCentersOnLine(pixbg, x, -1, minlength);
            nh = numaGetCount(nah);
            nm = numaGetCount(nam);
            for (j = 0; j < nh; j++) {
                numaGetIValue(nah, j, &yval);
                ptaAddPt(ptah, x, yval);
            }
            for (j = 0; j < nm; j++) {
                numaGetIValue(nam, j, &yval);
                ptaAddPt(ptam, x, yval);
            }
            numaDestroy(&nah);
            numaDestroy(&nam);
        }
    }

        /* Make the Sel with those points */
    sel = selCreateBrick(h, w, h / 2, w / 2, SEL_DONT_CARE);
    nh = ptaGetCount(ptah);
    for (i = 0; i < nh; i++) {
        ptaGetIPt(ptah, i, &x, &y);
        selSetElement(sel, y, x, SEL_HIT);
    }
    nm = ptaGetCount(ptam);
    for (i = 0; i < nm; i++) {
        ptaGetIPt(ptam, i, &x, &y);
        selSetElement(sel, y, x, SEL_MISS);
    }

    pixDestroy(&pixfg);
    pixDestroy(&pixbg);
    ptaDestroy(&ptah);
    ptaDestroy(&ptam);
    return sel;
}


/*!
 * \brief   pixGenerateSelRandom()
 *
 * \param[in]    pixs        1 bpp, typically small, to be used as a pattern
 * \param[in]    hitfract    fraction of allowable fg pixels that are hits
 * \param[in]    missfract   fraction of allowable bg pixels that are misses
 * \param[in]    distance    min distance from boundary pixel; use 0 for default
 * \param[in]    toppix      number of extra pixels of bg added above
 * \param[in]    botpix      number of extra pixels of bg added below
 * \param[in]    leftpix     number of extra pixels of bg added to left
 * \param[in]    rightpix    number of extra pixels of bg added to right
 * \param[out]   ppixe       [optional] input pix expanded by extra pixels
 * \return  sel hit-miss for input pattern, or NULL on error
 *
 * <pre>
 * Notes:
 *    (1) Either of hitfract and missfract can be zero.  If both are zero,
 *        the sel would be empty, and NULL is returned.
 *    (2) No elements are selected that are less than 'distance' pixels away
 *        from a boundary pixel of the same color.  This makes the
 *        match much more robust to edge noise.  Valid inputs of
 *        'distance' are 0, 1, 2, 3 and 4.  If distance is either 0 or
 *        greater than 4, we reset it to the default value.
 *    (3) The 4 numbers for adding rectangles of pixels outside the fg
 *        can be use if the pattern is expected to be surrounded by bg
 *        (white) pixels.  On the other hand, if the pattern may be near
 *        other fg (black) components on some sides, use 0 for those sides.
 *    (4) The input pix, as extended by the extra pixels on selected sides,
 *        can optionally be returned.  For debugging, call
 *        pixDisplayHitMissSel() to visualize the hit-miss sel superimposed
 *        on the generating bitmap.
 *    (5) This method is inferior to both the boundary and run based methods.
 * </pre>
 */
SEL *
pixGenerateSelRandom(PIX       *pixs,
                     l_float32  hitfract,
                     l_float32  missfract,
                     l_int32    distance,
                     l_int32    toppix,
                     l_int32    botpix,
                     l_int32    leftpix,
                     l_int32    rightpix,
                     PIX      **ppixe)
{
l_int32    ws, hs, w, h, x, y, i, j, thresh;
l_uint32   val;
PIX       *pixt1, *pixt2, *pixfg, *pixbg;
SEL       *seld, *sel;

    if (ppixe) *ppixe = NULL;
    if (!pixs)
        return (SEL *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (SEL *)ERROR_PTR("pixs not 1 bpp", __func__, NULL);
    if (hitfract <= 0.0 && missfract <= 0.0)
        return (SEL *)ERROR_PTR("no hits or misses", __func__, NULL);
    if (hitfract > 1.0 || missfract > 1.0)
        return (SEL *)ERROR_PTR("fraction can't be > 1.0", __func__, NULL);

    if (distance <= 0)
        distance = DefaultDistanceToBoundary;
    if (distance > MaxDistanceToBoundary) {
        L_WARNING("distance too large; setting to max value\n", __func__);
        distance = MaxDistanceToBoundary;
    }

        /* Locate the foreground */
    pixClipToForeground(pixs, &pixt1, NULL);
    if (!pixt1)
        return (SEL *)ERROR_PTR("pixt1 not made", __func__, NULL);
    ws = pixGetWidth(pixt1);
    hs = pixGetHeight(pixt1);
    w = ws;
    h = hs;

        /* Crop out a region including the foreground, and add pixels
         * on sides depending on the side flags */
    if (toppix || botpix || leftpix || rightpix) {
        x = y = 0;
        if (toppix) {
            h += toppix;
            y = toppix;
        }
        if (botpix)
            h += botpix;
        if (leftpix) {
            w += leftpix;
            x = leftpix;
        }
        if (rightpix)
            w += rightpix;
        pixt2 = pixCreate(w, h, 1);
        pixRasterop(pixt2, x, y, ws, hs, PIX_SRC, pixt1, 0, 0);
    } else {
        pixt2 = pixClone(pixt1);
    }
    if (ppixe)
        *ppixe = pixClone(pixt2);
    pixDestroy(&pixt1);

        /* Identify fg and bg pixels that are at least 'distance' pixels
         * away from the boundary pixels in their set */
    seld = selCreateBrick(2 * distance + 1, 2 * distance + 1,
                          distance, distance, SEL_HIT);
    pixfg = pixErode(NULL, pixt2, seld);
    pixbg = pixDilate(NULL, pixt2, seld);
    pixInvert(pixbg, pixbg);
    selDestroy(&seld);
    pixDestroy(&pixt2);

        /* Generate the sel from a random selection of these points */
    sel = selCreateBrick(h, w, h / 2, w / 2, SEL_DONT_CARE);
    if (hitfract > 0.0) {
        thresh = (l_int32)(hitfract * (l_float64)RAND_MAX);
        for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
                pixGetPixel(pixfg, j, i, &val);
                if (val) {
                    if (rand() < thresh)
                        selSetElement(sel, i, j, SEL_HIT);
                }
            }
        }
    }
    if (missfract > 0.0) {
        thresh = (l_int32)(missfract * (l_float64)RAND_MAX);
        for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
                pixGetPixel(pixbg, j, i, &val);
                if (val) {
                    if (rand() < thresh)
                        selSetElement(sel, i, j, SEL_MISS);
                }
            }
        }
    }

    pixDestroy(&pixfg);
    pixDestroy(&pixbg);
    return sel;
}


/*-----------------------------------------------------------------*
 *              Accumulate data on runs along lines                *
 *-----------------------------------------------------------------*/
/*!
 * \brief   pixGetRunCentersOnLine()
 *
 * \param[in]    pixs        1 bpp
 * \param[in]    x, y        set one of these to -1; see notes
 * \param[in]    minlength   minimum length of acceptable run
 * \return  numa of fg runs, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Action: this function computes the fg (black) and bg (white)
 *          pixel runlengths along the specified horizontal or vertical line,
 *          and returns a Numa of the "center" pixels of each fg run
 *          whose length equals or exceeds the minimum length.
 *      (2) This only works on horizontal and vertical lines.
 *      (3) For horizontal runs, set x = -1 and y to the value
 *          for all points along the raster line.  For vertical runs,
 *          set y = -1 and x to the value for all points along the
 *          pixel column.
 *      (4) For horizontal runs, the points in the Numa are the x
 *          values in the center of fg runs that are of length at
 *          least 'minlength'.  For vertical runs, the points in the
 *          Numa are the y values in the center of fg runs, again
 *          of length 'minlength' or greater.
 *      (5) If there are no fg runs along the line that satisfy the
 *          minlength constraint, the returned Numa is empty.  This
 *          is not an error.
 * </pre>
 */
NUMA *
pixGetRunCentersOnLine(PIX     *pixs,
                       l_int32  x,
                       l_int32  y,
                       l_int32  minlength)
{
l_int32   w, h, i, r, nruns, len;
NUMA     *naruns, *nad;

    if (!pixs)
        return (NUMA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (NUMA *)ERROR_PTR("pixs not 1 bpp", __func__, NULL);
    if (x != -1 && y != -1)
        return (NUMA *)ERROR_PTR("x or y must be -1", __func__, NULL);
    if (x == -1 && y == -1)
        return (NUMA *)ERROR_PTR("x or y cannot both be -1", __func__, NULL);

    if ((nad = numaCreate(0)) == NULL)
        return (NUMA *)ERROR_PTR("nad not made", __func__, NULL);
    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    if (x == -1) {  /* horizontal run */
        if (y < 0 || y >= h)
            return nad;
        naruns = pixGetRunsOnLine(pixs, 0, y, w - 1, y);
    } else {  /* vertical run */
        if (x < 0 || x >= w)
            return nad;
        naruns = pixGetRunsOnLine(pixs, x, 0, x, h - 1);
    }
    nruns = numaGetCount(naruns);

        /* extract run center values; the first run is always bg */
    r = 0;  /* cumulative distance along line */
    for (i = 0; i < nruns; i++) {
        if (i % 2 == 0) {  /* bg run */
            numaGetIValue(naruns, i, &len);
            r += len;
            continue;
        } else {
            numaGetIValue(naruns, i, &len);
            if (len >= minlength)
                numaAddNumber(nad, r + len / 2);
            r += len;
        }
    }

    numaDestroy(&naruns);
    return nad;
}


/*!
 * \brief   pixGetRunsOnLine()
 *
 * \param[in]    pixs    1 bpp
 * \param[in]    x1, y1, x2, y2
 * \return  numa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Action: this function uses the bresenham algorithm to compute
 *          the pixels along the specified line.  It returns a Numa of the
 *          runlengths of the fg (black) and bg (white) runs, always
 *          starting with a white run.
 *      (2) If the first pixel on the line is black, the length of the
 *          first returned run (which is white) is 0.
 * </pre>
 */
NUMA *
pixGetRunsOnLine(PIX     *pixs,
                 l_int32  x1,
                 l_int32  y1,
                 l_int32  x2,
                 l_int32  y2)
{
l_int32   w, h, x, y, npts;
l_int32   i, runlen, preval;
l_uint32  val;
NUMA     *numa;
PTA      *pta;

    if (!pixs)
        return (NUMA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (NUMA *)ERROR_PTR("pixs not 1 bpp", __func__, NULL);

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    if (x1 < 0 || x1 >= w)
        return (NUMA *)ERROR_PTR("x1 not valid", __func__, NULL);
    if (x2 < 0 || x2 >= w)
        return (NUMA *)ERROR_PTR("x2 not valid", __func__, NULL);
    if (y1 < 0 || y1 >= h)
        return (NUMA *)ERROR_PTR("y1 not valid", __func__, NULL);
    if (y2 < 0 || y2 >= h)
        return (NUMA *)ERROR_PTR("y2 not valid", __func__, NULL);

    if ((pta = generatePtaLine(x1, y1, x2, y2)) == NULL)
        return (NUMA *)ERROR_PTR("pta not made", __func__, NULL);
    if ((npts = ptaGetCount(pta)) == 0) {
        ptaDestroy(&pta);
        return (NUMA *)ERROR_PTR("pta has no pts", __func__, NULL);
    }
    if ((numa = numaCreate(0)) == NULL) {
        ptaDestroy(&pta);
        return (NUMA *)ERROR_PTR("numa not made", __func__, NULL);
    }

    for (i = 0; i < npts; i++) {
        ptaGetIPt(pta, i, &x, &y);
        pixGetPixel(pixs, x, y, &val);
        if (i == 0) {
            if (val == 1) {  /* black pixel; append white run of size 0 */
                numaAddNumber(numa, 0);
            }
            preval = val;
            runlen = 1;
            continue;
        }
        if (val == preval) {  /* extend current run */
            preval = val;
            runlen++;
        } else {  /* end previous run */
            numaAddNumber(numa, runlen);
            preval = val;
            runlen = 1;
        }
    }
    numaAddNumber(numa, runlen);  /* append last run */

    ptaDestroy(&pta);
    return numa;
}


/*-----------------------------------------------------------------*
 *        Subsample boundary pixels in relatively ordered way      *
 *-----------------------------------------------------------------*/
/*!
 * \brief   pixSubsampleBoundaryPixels()
 *
 * \param[in]    pixs   1 bpp, with only boundary pixels in fg
 * \param[in]    skip   number to skip between samples as you traverse boundary
 * \return  pta, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If skip = 0, we take all the fg pixels.
 *      (2) We try to traverse the boundaries in a regular way.
 *          Some pixels may be missed, and these are then subsampled
 *          randomly with a fraction determined by 'skip'.
 *      (3) The most natural approach is to use a depth first (stack-based)
 *          method to find the fg pixels.  However, the pixel runs are
 *          4-connected and there are relatively few branches.  So
 *          instead of doing a proper depth-first search, we get nearly
 *          the same result using two nested while loops: the outer
 *          one continues a raster-based search for the next fg pixel,
 *          and the inner one does a reasonable job running along
 *          each 4-connected coutour.
 * </pre>
 */
PTA *
pixSubsampleBoundaryPixels(PIX     *pixs,
                           l_int32  skip)
{
l_int32  x, y, xn, yn, xs, ys, xa, ya, count;
PIX     *pixt;
PTA     *pta;

    if (!pixs)
        return (PTA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PTA *)ERROR_PTR("pixs not 1 bpp", __func__, NULL);
    if (skip < 0)
        return (PTA *)ERROR_PTR("skip < 0", __func__, NULL);

    if (skip == 0)
        return ptaGetPixelsFromPix(pixs, NULL);

    pta = ptaCreate(0);
    pixt = pixCopy(NULL, pixs);
    xs = ys = 0;
    while (nextOnPixelInRaster(pixt, xs, ys, &xn, &yn)) {  /* new series */
        xs = xn;
        ys = yn;

            /* Add first point in this series */
        ptaAddPt(pta, xs, ys);

            /* Trace out boundary, erasing all and saving every (skip + 1)th */
        x = xs;
        y = ys;
        pixSetPixel(pixt, x, y, 0);
        count = 0;
        while (adjacentOnPixelInRaster(pixt, x, y, &xa, &ya)) {
            x = xa;
            y = ya;
            pixSetPixel(pixt, x, y, 0);
            if (count == skip) {
                ptaAddPt(pta, x, y);
                count = 0;
            } else {
                count++;
            }
        }
    }

    pixDestroy(&pixt);
    return pta;
}


/*!
 * \brief   adjacentOnPixelInRaster()
 *
 * \param[in]    pixs        1 bpp
 * \param[in]    x, y        current pixel
 * \param[out]   pxa, pya    adjacent ON pixel, found by simple CCW search
 * \return  1 if a pixel is found; 0 otherwise or on error
 *
 * <pre>
 * Notes:
 *      (1) Search is in 4-connected directions first; then on diagonals.
 *          This allows traversal along a 4-connected boundary.
 * </pre>
 */
l_int32
adjacentOnPixelInRaster(PIX      *pixs,
                        l_int32   x,
                        l_int32   y,
                        l_int32  *pxa,
                        l_int32  *pya)
{
l_int32   w, h, i, xa, ya, found;
l_int32   xdel[] = {-1, 0, 1, 0, -1, 1, 1, -1};
l_int32   ydel[] = {0, 1, 0, -1, 1, 1, -1, -1};
l_uint32  val;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 0);
    if (pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not 1 bpp", __func__, 0);
    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    found = 0;
    for (i = 0; i < 8; i++) {
        xa = x + xdel[i];
        ya = y + ydel[i];
        if (xa < 0 || xa >= w || ya < 0 || ya >= h)
            continue;
        pixGetPixel(pixs, xa, ya, &val);
        if (val == 1) {
            found = 1;
            *pxa = xa;
            *pya = ya;
            break;
        }
    }
    return found;
}



/*-----------------------------------------------------------------*
 *          Display generated sel with originating image           *
 *-----------------------------------------------------------------*/
/*!
 * \brief   pixDisplayHitMissSel()
 *
 * \param[in]    pixs          1 bpp
 * \param[in]    sel           hit-miss in general
 * \param[in]    scalefactor   an integer >= 1; use 0 for default
 * \param[in]    hitcolor      RGB0 color for center of hit pixels
 * \param[in]    misscolor     RGB0 color for center of miss pixels
 * \return  pixd RGB showing both pixs and sel, or NULL on error
 * <pre>
 * Notes:
 *    (1) We don't allow scalefactor to be larger than MaxSelScalefactor
 *    (2) The colors are conveniently given as 4 bytes in hex format,
 *        such as 0xff008800.  The least significant byte is ignored.
 * </pre>
 */
PIX *
pixDisplayHitMissSel(PIX      *pixs,
                     SEL      *sel,
                     l_int32   scalefactor,
                     l_uint32  hitcolor,
                     l_uint32  misscolor)
{
l_int32    i, j, type;
l_float32  fscale;
PIX       *pixt, *pixd;
PIXCMAP   *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", __func__, NULL);
    if (!sel)
        return (PIX *)ERROR_PTR("sel not defined", __func__, NULL);

    if (scalefactor <= 0)
        scalefactor = DefaultSelScalefactor;
    if (scalefactor > MaxSelScalefactor) {
        L_WARNING("scalefactor too large; using max value\n", __func__);
        scalefactor = MaxSelScalefactor;
    }

        /* Generate a version of pixs with a colormap */
    pixt = pixConvert1To8(NULL, pixs, 0, 1);
    cmap = pixcmapCreate(8);
    pixcmapAddColor(cmap, 255, 255, 255);
    pixcmapAddColor(cmap, 0, 0, 0);
    pixcmapAddColor(cmap, hitcolor >> 24, (hitcolor >> 16) & 0xff,
                    (hitcolor >> 8) & 0xff);
    pixcmapAddColor(cmap, misscolor >> 24, (misscolor >> 16) & 0xff,
                    (misscolor >> 8) & 0xff);
    pixSetColormap(pixt, cmap);

        /* Color the hits and misses */
    for (i = 0; i < sel->sy; i++) {
        for (j = 0; j < sel->sx; j++) {
            selGetElement(sel, i, j, &type);
            if (type == SEL_DONT_CARE)
                continue;
            if (type == SEL_HIT)
                pixSetPixel(pixt, j, i, 2);
            else  /* type == SEL_MISS */
                pixSetPixel(pixt, j, i, 3);
        }
    }

        /* Scale it up */
    fscale = (l_float32)scalefactor;
    pixd = pixScaleBySampling(pixt, fscale, fscale);

    pixDestroy(&pixt);
    return pixd;
}

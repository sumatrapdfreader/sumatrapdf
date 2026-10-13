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
 * \file colorseg.c
 * <pre>
 *
 *    Unsupervised color segmentation
 *
 *               PIX     *pixColorSegment()
 *               PIX     *pixColorSegmentCluster()
 *       static  l_int32  pixColorSegmentTryCluster()
 *               l_int32  pixAssignToNearestColor()
 *               l_int32  pixColorSegmentClean()
 *               l_int32  pixColorSegmentRemoveColors()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

    /* Maximum allowed iterations in Phase 1. */
static const l_int32  MAX_ALLOWED_ITERATIONS = 20;

    /* Factor by which max dist is increased on each iteration */
static const l_float32  DIST_EXPAND_FACT = 1.3f;

    /* Octcube division level for computing nearest colormap color using LUT.
     * Using 4 should suffice for up to 50 - 100 colors, and it is
     * very fast.  Using 5 takes 8 times as long to set up the LUT
     * for little perceptual gain, even with 100 colors. */
static const l_int32  LEVEL_IN_OCTCUBE = 4;


static l_int32 pixColorSegmentTryCluster(PIX *pixd, PIX *pixs,
                                         l_int32 maxdist, l_int32 maxcolors,
                                         l_int32 debugflag);

/*------------------------------------------------------------------*
 *                 Unsupervised color segmentation                  *
 *------------------------------------------------------------------*/
/*!
 * \brief   pixColorSegment()
 *
 * \param[in]    pixs  32 bpp; 24-bit color
 * \param[in]    maxdist max euclidean dist to existing cluster
 * \param[in]    maxcolors max number of colors allowed in first pass
 * \param[in]    selsize linear size of sel for closing to remove noise
 * \param[in]    finalcolors max number of final colors allowed after 4th pass
 * \param[in]    debugflag  1 for debug output; 0 otherwise
 * \return  pixd 8 bit with colormap, or NULL on error
 *
 * <pre>
 *  Color segmentation proceeds in four phases:
 *
 *  Phase 1:  pixColorSegmentCluster()
 *  The image is traversed in raster order.  Each pixel either
 *  becomes the representative for a new cluster or is assigned to an
 *  existing cluster.  Assignment is greedy.  The data is stored in
 *  a colormapped image.  Three auxiliary arrays are used to hold
 *  the colors of the representative pixels, for fast lookup.
 *  The average color in each cluster is computed.
 *
 *  Phase 2.  pixAssignToNearestColor()
 *  A second non-greedy clustering pass is performed, where each pixel
 *  is assigned to the nearest cluster average.  We also keep track
 *  of how many pixels are assigned to each cluster.
 *
 *  Phase 3.  pixColorSegmentClean()
 *  For each cluster, starting with the largest, do a morphological
 *  closing to eliminate small components within larger ones.
 *
 *  Phase 4.  pixColorSegmentRemoveColors()
 *  Eliminate all colors except the most populated 'finalcolors'.
 *  Then remove unused colors from the colormap, and reassign those
 *  pixels to the nearest remaining cluster, using the original pixel values.
 *
 * Notes:
 *      (1) The goal is to generate a small number of colors.
 *          Typically this would be specified by 'finalcolors',
 *          a number that would be somewhere between 3 and 6.
 *          The parameter 'maxcolors' specifies the maximum number of
 *          colors generated in the first phase.  This should be
 *          larger than finalcolors, perhaps twice as large.
 *          If more than 'maxcolors' are generated in the first phase
 *          using the input 'maxdist', the distance is repeatedly
 *          increased by a multiplicative factor until the condition
 *          is satisfied.  The implicit relation between 'maxdist'
 *          and 'maxcolors' is thus adjusted programmatically.
 *      (2) As a very rough guideline, given a target value of 'finalcolors',
 *          here are approximate values of 'maxdist' and 'maxcolors'
 *          to start with:
 *
 *               finalcolors    maxcolors    maxdist
 *               -----------    ---------    -------
 *                   3             6          100
 *                   4             8           90
 *                   5            10           75
 *                   6            12           60
 *
 *          For a given number of finalcolors, if you use too many
 *          maxcolors, the result will be noisy.  If you use too few,
 *          the result will be a relatively poor assignment of colors.
 * </pre>
 */
PIX *
pixColorSegment(PIX     *pixs,
                l_int32  maxdist,
                l_int32  maxcolors,
                l_int32  selsize,
                l_int32  finalcolors,
                l_int32  debugflag)
{
l_int32   *countarray;
PIX       *pixd;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("must be rgb color", __func__, NULL);

        /* Phase 1; original segmentation */
    pixd = pixColorSegmentCluster(pixs, maxdist, maxcolors, debugflag);
    if (!pixd)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    if (debugflag) {
        lept_mkdir("lept/segment");
        pixWriteDebug("/tmp/lept/segment/colorseg1.png", pixd, IFF_PNG);
    }

        /* Phase 2; refinement in pixel assignment */
    countarray = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    pixAssignToNearestColor(pixd, pixs, NULL, LEVEL_IN_OCTCUBE, countarray);
    if (debugflag)
        pixWriteDebug("/tmp/lept/segment/colorseg2.png", pixd, IFF_PNG);

        /* Phase 3: noise removal by separately closing each color */
    pixColorSegmentClean(pixd, selsize, countarray);
    LEPT_FREE(countarray);
    if (debugflag)
        pixWriteDebug("/tmp/lept/segment/colorseg3.png", pixd, IFF_PNG);

        /* Phase 4: removal of colors with small population and
         * reassignment of pixels to remaining colors */
    pixColorSegmentRemoveColors(pixd, pixs, finalcolors);
    return pixd;
}


/*!
 * \brief   pixColorSegmentCluster()
 *
 * \param[in]    pixs  32 bpp; 24-bit color
 * \param[in]    maxdist max euclidean dist to existing cluster
 * \param[in]    maxcolors max number of colors allowed in first pass
 * \param[in]    debugflag  1 for debug output; 0 otherwise
 * \return  pixd 8 bit with colormap, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is phase 1.  See description in pixColorSegment().
 *      (2) Greedy unsupervised classification.  If the limit 'maxcolors'
 *          is exceeded, the computation is repeated with a larger
 *          allowed cluster size.
 *      (3) On each successive iteration, 'maxdist' is increased by a
 *          constant factor.  See comments in pixColorSegment() for
 *          a guideline on parameter selection.
 *          Note that the diagonal of the 8-bit rgb color cube is about
 *          440, so for 'maxdist' = 440, you are guaranteed to get 1 color!
 * </pre>
 */
PIX *
pixColorSegmentCluster(PIX     *pixs,
                       l_int32  maxdist,
                       l_int32  maxcolors,
                       l_int32  debugflag)
{
l_int32   w, h, newmaxdist, ret, niters, ncolors, success;
PIX      *pixd;
PIXCMAP  *cmap;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 32)
        return (PIX *)ERROR_PTR("must be rgb color", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    if ((pixd = pixCreate(w, h, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    cmap = pixcmapCreate(8);
    pixSetColormap(pixd, cmap);
    pixCopyResolution(pixd, pixs);

    newmaxdist = maxdist;
    niters = 0;
    success = TRUE;
    while (1) {
        ret = pixColorSegmentTryCluster(pixd, pixs, newmaxdist,
                                        maxcolors, debugflag);
        niters++;
        if (!ret) {
            ncolors = pixcmapGetCount(cmap);
            if (debugflag)
                L_INFO("Success with %d colors after %d iters\n", __func__,
                       ncolors, niters);
            break;
        }
        if (niters == MAX_ALLOWED_ITERATIONS) {
            L_WARNING("too many iters; newmaxdist = %d\n",
                      __func__, newmaxdist);
            success = FALSE;
            break;
        }
        newmaxdist = (l_int32)(DIST_EXPAND_FACT * (l_float32)newmaxdist);
    }

    if (!success) {
        pixDestroy(&pixd);
        return (PIX *)ERROR_PTR("failure in phase 1", __func__, NULL);
    }

    return pixd;
}


/*!
 * \brief   pixColorSegmentTryCluster()
 *
 * \param[in]    pixd
 * \param[in]    pixs
 * \param[in]    maxdist
 * \param[in]    maxcolors
 * \param[in]    debugflag  1 for debug output; 0 otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      This function should only be called from pixColorSegCluster()
 * </pre>
 */
static l_int32
pixColorSegmentTryCluster(PIX     *pixd,
                          PIX     *pixs,
                          l_int32  maxdist,
                          l_int32  maxcolors,
                          l_int32  debugflag)
{
l_int32    rmap[256], gmap[256], bmap[256];
l_int32    w, h, wpls, wpld, i, j, k, found, ret, index, ncolors;
l_int32    rval, gval, bval, dist2, maxdist2;
l_int32    countarray[256];
l_int32    rsum[256], gsum[256], bsum[256];
l_uint32  *ppixel;
l_uint32  *datas, *datad, *lines, *lined;
PIXCMAP   *cmap;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    maxdist2 = maxdist * maxdist;
    cmap = pixGetColormap(pixd);
    pixcmapClear(cmap);
    for (k = 0; k < 256; k++) {
        rsum[k] = gsum[k] = bsum[k] = 0;
        rmap[k] = gmap[k] = bmap[k] = 0;
    }

    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);
    ncolors = 0;
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            ppixel = lines + j;
            rval = GET_DATA_BYTE(ppixel, COLOR_RED);
            gval = GET_DATA_BYTE(ppixel, COLOR_GREEN);
            bval = GET_DATA_BYTE(ppixel, COLOR_BLUE);
            ncolors = pixcmapGetCount(cmap);
            found = FALSE;
            for (k = 0; k < ncolors; k++) {
                dist2 = (rval - rmap[k]) * (rval - rmap[k]) +
                        (gval - gmap[k]) * (gval - gmap[k]) +
                        (bval - bmap[k]) * (bval - bmap[k]);
                if (dist2 <= maxdist2) {  /* take it; greedy */
                    found = TRUE;
                    SET_DATA_BYTE(lined, j, k);
                    countarray[k]++;
                    rsum[k] += rval;
                    gsum[k] += gval;
                    bsum[k] += bval;
                    break;
                }
            }
            if (!found) {  /* Add a new color */
                ret = pixcmapAddNewColor(cmap, rval, gval, bval, &index);
/*                lept_stderr(
                        "index = %d, (i,j) = (%d,%d), rgb = (%d, %d, %d)\n",
                        index, i, j, rval, gval, bval); */
                if (ret == 0 && index < maxcolors) {
                    countarray[index] = 1;
                    SET_DATA_BYTE(lined, j, index);
                    rmap[index] = rval;
                    gmap[index] = gval;
                    bmap[index] = bval;
                    rsum[index] = rval;
                    gsum[index] = gval;
                    bsum[index] = bval;
                } else {
                    if (debugflag) {
                        L_INFO("maxcolors exceeded for maxdist = %d\n",
                               __func__, maxdist);
                    }
                    return 1;
                }
            }
        }
    }

        /* Replace the colors in the colormap by the averages */
    for (k = 0; k < ncolors; k++) {
        rval = rsum[k] / countarray[k];
        gval = gsum[k] / countarray[k];
        bval = bsum[k] / countarray[k];
        pixcmapResetColor(cmap, k, rval, gval, bval);
    }

    return 0;
}


/*!
 * \brief   pixAssignToNearestColor()
 *
 * \param[in]    pixd  8 bpp, colormapped
 * \param[in]    pixs  32 bpp; 24-bit color
 * \param[in]    pixm  [optional] 1 bpp
 * \param[in]    level of octcube used for finding nearest color in cmap
 * \param[in]    countarray [optional] ptr to array, in which we can store
 *                          the number of pixels found in each color in
 *                          the colormap in pixd
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is used in phase 2 of color segmentation, where pixs
 *          is the original input image to pixColorSegment(), and
 *          pixd is the colormapped image returned from
 *          pixColorSegmentCluster().  It is also used, with a mask,
 *          in phase 4.
 *      (2) This is an in-place operation.
 *      (3) The colormap in pixd is unchanged.
 *      (4) pixs and pixd must be the same size (w, h).
 *      (5) The selection mask pixm can be null.  If it exists, it must
 *          be the same size as pixs and pixd, and only pixels
 *          corresponding to fg in pixm are assigned.  Set to
 *          NULL if all pixels in pixd are to be assigned.
 *      (6) The countarray can be null.  If it exists, it is pre-allocated
 *          and of a size at least equal to the size of the colormap in pixd.
 *      (7) This does a best-fit (non-greedy) assignment of pixels to
 *          existing clusters.  Specifically, it assigns each pixel
 *          in pixd to the color index in the pixd colormap that has a
 *          color closest to the corresponding rgb pixel in pixs.
 *      (8) 'level' is the octcube level used to quickly find the nearest
 *          color in the colormap for each pixel.  For color segmentation,
 *          this parameter is set to LEVEL_IN_OCTCUBE.
 *      (9) We build a mapping table from octcube to colormap index so
 *          that this function can run in a time (otherwise) independent
 *          of the number of colors in the colormap.  This avoids a
 *          brute-force search for the closest colormap color to each
 *          pixel in the image.
 * </pre>
 */
l_ok
pixAssignToNearestColor(PIX      *pixd,
                        PIX      *pixs,
                        PIX      *pixm,
                        l_int32   level,
                        l_int32  *countarray)
{
l_int32    w, h, wpls, wpld, wplm, i, j, success;
l_int32    rval, gval, bval, index;
l_int32   *cmaptab;
l_uint32   octindex;
l_uint32  *rtab, *gtab, *btab;
l_uint32  *ppixel;
l_uint32  *datas, *datad, *datam = NULL, *lines, *lined, *linem = NULL;
PIXCMAP   *cmap;

    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if ((cmap = pixGetColormap(pixd)) == NULL)
        return ERROR_INT("cmap not found", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs not 32 bpp", __func__, 1);
    if (level < 1 || level > 6)
        return ERROR_INT("level not in [1 ... 6]", __func__, 1);

        /* Set up the tables to map rgb to the nearest colormap index */
    success = TRUE;
    makeRGBToIndexTables(level, &rtab, &gtab, &btab);
    cmaptab = pixcmapToOctcubeLUT(cmap, level, L_MANHATTAN_DISTANCE);
    if (!rtab || !gtab || !btab || !cmaptab) {
        L_ERROR("failure to make a table\n", __func__);
        success = FALSE;
        goto cleanup_arrays;
    }

    pixGetDimensions(pixs, &w, &h, NULL);
    datas = pixGetData(pixs);
    datad = pixGetData(pixd);
    wpls = pixGetWpl(pixs);
    wpld = pixGetWpl(pixd);
    if (pixm) {
        datam = pixGetData(pixm);
        wplm = pixGetWpl(pixm);
    }
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        if (pixm)
            linem = datam + i * wplm;
        for (j = 0; j < w; j++) {
            if (pixm) {
                if (!GET_DATA_BIT(linem, j))
                    continue;
            }
            ppixel = lines + j;
            rval = GET_DATA_BYTE(ppixel, COLOR_RED);
            gval = GET_DATA_BYTE(ppixel, COLOR_GREEN);
            bval = GET_DATA_BYTE(ppixel, COLOR_BLUE);
                /* Map from rgb to octcube index */
            getOctcubeIndexFromRGB(rval, gval, bval, rtab, gtab, btab,
                                   &octindex);
                /* Map from octcube index to nearest colormap index */
            index = cmaptab[octindex];
            if (countarray)
                countarray[index]++;
            SET_DATA_BYTE(lined, j, index);
        }
    }

cleanup_arrays:
    LEPT_FREE(cmaptab);
    LEPT_FREE(rtab);
    LEPT_FREE(gtab);
    LEPT_FREE(btab);
    return (success) ? 0 : 1;
}


/*!
 * \brief   pixColorSegmentClean()
 *
 * \param[in]    pixs  8 bpp, colormapped
 * \param[in]    selsize for closing
 * \param[in]    countarray ptr to array containing the number of pixels
 *                          found in each color in the colormap
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This operation is in-place.
 *      (2) This is phase 3 of color segmentation.  It is the first
 *          part of a two-step noise removal process.  Colors with a
 *          large population are closed first; this operation absorbs
 *          small sets of intercolated pixels of a different color.
 * </pre>
 */
l_ok
pixColorSegmentClean(PIX      *pixs,
                     l_int32   selsize,
                     l_int32  *countarray)
{
l_int32    i, ncolors, val;
l_uint32   val32;
NUMA      *na, *nasi;
PIX       *pixt1, *pixt2;
PIXCMAP   *cmap;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not 8 bpp", __func__, 1);
    if ((cmap = pixGetColormap(pixs)) == NULL)
        return ERROR_INT("cmap not found", __func__, 1);
    if (!countarray)
        return ERROR_INT("countarray not defined", __func__, 1);
    if (selsize <= 1)
        return 0;  /* nothing to do */

        /* Sort colormap indices in decreasing order of pixel population */
    ncolors = pixcmapGetCount(cmap);
    na = numaCreate(ncolors);
    for (i = 0; i < ncolors; i++)
        numaAddNumber(na, countarray[i]);
    nasi = numaGetSortIndex(na, L_SORT_DECREASING);
    numaDestroy(&na);
    if (!nasi)
        return ERROR_INT("nasi not made", __func__, 1);

        /* For each color, in order of decreasing population,
         * do a closing and absorb the added pixels.  Note that
         * if the closing removes pixels at the border, they'll
         * still appear in the xor and will be properly (re)set. */
    for (i = 0; i < ncolors; i++) {
        numaGetIValue(nasi, i, &val);
        pixt1 = pixGenerateMaskByValue(pixs, val, 1);
        pixt2 = pixCloseSafeCompBrick(NULL, pixt1, selsize, selsize);
        pixXor(pixt2, pixt2, pixt1);  /* pixels to be added to type 'val' */
        pixcmapGetColor32(cmap, val, &val32);
        pixSetMasked(pixs, pixt2, val32);  /* add them */
        pixDestroy(&pixt1);
        pixDestroy(&pixt2);
    }
    numaDestroy(&nasi);
    return 0;
}


/*!
 * \brief   pixColorSegmentRemoveColors()
 *
 * \param[in]    pixd  8 bpp, colormapped
 * \param[in]    pixs  32 bpp rgb, with initial pixel values
 * \param[in]    finalcolors max number of colors to retain
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This operation is in-place.
 *      (2) This is phase 4 of color segmentation, and the second part
 *          of the 2-step noise removal.  Only 'finalcolors' different
 *          colors are retained, with colors with smaller populations
 *          being replaced by the nearest color of the remaining colors.
 *          For highest accuracy, for pixels that are being replaced,
 *          we find the nearest colormap color  to the original rgb color.
 * </pre>
 */
l_ok
pixColorSegmentRemoveColors(PIX     *pixd,
                            PIX     *pixs,
                            l_int32  finalcolors)
{
l_int32    i, ncolors, index, tempindex;
l_int32   *tab;
l_uint32   tempcolor;
NUMA      *na, *nasi;
PIX       *pixm;
PIXCMAP   *cmap;

    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (pixGetDepth(pixd) != 8)
        return ERROR_INT("pixd not 8 bpp", __func__, 1);
    if ((cmap = pixGetColormap(pixd)) == NULL)
        return ERROR_INT("cmap not found", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    ncolors = pixcmapGetCount(cmap);
    if (finalcolors >= ncolors)  /* few enough colors already; nothing to do */
        return 0;

        /* Generate a mask over all pixels that are not in the
         * 'finalcolors' most populated colors.  Save the colormap
         * index of any one of the retained colors in 'tempindex'.
         * The LUT has values 0 for the 'finalcolors' most populated colors,
         * which will be retained; and 1 for the rest, which are marked
         * by fg pixels in pixm and will be removed. */
    na = pixGetCmapHistogram(pixd, 1);
    if ((nasi = numaGetSortIndex(na, L_SORT_DECREASING)) == NULL) {
        numaDestroy(&na);
        return ERROR_INT("nasi not made", __func__, 1);
    }
    numaGetIValue(nasi, finalcolors - 1, &tempindex);  /* retain down to this */
    pixcmapGetColor32(cmap, tempindex, &tempcolor);  /* use this color */
    tab = (l_int32 *)LEPT_CALLOC(256, sizeof(l_int32));
    for (i = finalcolors; i < ncolors; i++) {
        numaGetIValue(nasi, i, &index);
        tab[index] = 1;
    }

    pixm = pixMakeMaskFromLUT(pixd, tab);
    LEPT_FREE(tab);

        /* Reassign the masked pixels temporarily to the saved index
         * (tempindex).  This guarantees that no pixels are labeled by
         * a colormap index of any colors that will be removed.
         * The actual value doesn't matter, as long as it's one
         * of the retained colors, because these pixels will later
         * be reassigned based on the full set of colors retained
         * in the colormap. */
    pixSetMasked(pixd, pixm, tempcolor);

        /* Now remove unused colors from the colormap.  This reassigns
         * image pixels as required. */
    pixRemoveUnusedColors(pixd);

        /* Finally, reassign the pixels under the mask (those that were
         * given a 'tempindex' value) to the nearest color in the colormap.
         * This is the function used in phase 2 on all image pixels; here
         * it is only used on the masked pixels given by pixm. */
    pixAssignToNearestColor(pixd, pixs, pixm, LEVEL_IN_OCTCUBE, NULL);

    pixDestroy(&pixm);
    numaDestroy(&na);
    numaDestroy(&nasi);
    return 0;
}

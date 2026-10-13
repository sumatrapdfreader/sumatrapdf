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
 * \file pixlabel.c
 * <pre>
 *
 *     Label pixels by an index for connected component membership
 *           PIX         *pixConnCompTransform()
 *
 *     Label pixels by the area of their connected component
 *           PIX         *pixConnCompAreaTransform()
 *
 *     Label pixels to allow incremental computation of connected components
 *           l_int32      pixConnCompIncrInit()
 *           l_int32      pixConnCompIncrAdd()
 *           l_int32      pixGetSortedNeighborValues()
 *
 *     Label pixels with spatially-dependent color coding
 *           PIX         *pixLocToColorTransform()
 *
 *  Pixels get labelled in various ways throughout the leptonica library,
 *  but most of the labelling is implicit, where the new value isn't
 *  even considered to be a label -- it is just a transformed pixel value
 *  that may be transformed again by another operation.  Quantization
 *  by thresholding, and dilation by a structuring element, are examples
 *  of these typical image processing operations.
 *
 *  However, there are some explicit labelling procedures that are useful
 *  as end-points of analysis, where it typically would not make sense
 *  to do further image processing on the result.  Assigning false color
 *  based on pixel properties is an example of such labelling operations.
 *  Such operations typically have 1 bpp input images, and result
 *  in grayscale or color images.
 *
 *  The procedures in this file are concerned with such explicit labelling.
 *  Some of these labelling procedures are also in other places in leptonica:
 *
 *    runlength.c:
 *       This file has two labelling transforms based on runlengths:
 *       pixStrokeWidthTransform() and pixvRunlengthTransform().
 *       The pixels are labelled based on the width of the "stroke" to
 *       which they belong, or on the length of the horizontal or
 *       vertical run in which they are a member.  Runlengths can easily
 *       be filtered using a threshold.
 *
 *    pixafunc2.c:
 *       This file has an operation, pixaDisplayRandomCmap(), that
 *       randomly labels pix in a pixa (that are typically found using
 *       pixConnComp) with up to 256 values, and assigns each value to
 *       a random colormap color.
 *
 *    seedfill.c:
 *       This file has pixDistanceFunction(), that labels each pixel with
 *       its distance from either the foreground or the background.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>
#include "allheaders.h"
#include "pix_internal.h"

/*-----------------------------------------------------------------------*
 *      Label pixels by an index for connected component membership      *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixConnCompTransform()
 *
 * \param[in]     pixs       1 bpp
 * \param[in]     connect    connectivity: 4 or 8
 * \param[in]     depth      of pixd: 8 or 16 bpp; use 0 for auto determination
 * \return   pixd   8, 16 or 32 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) pixd is 8, 16 or 32 bpp, and the pixel values label the
 *          fg component, starting with 1.  Pixels in the bg are labelled 0.
 *      (2) If %depth = 0, the depth of pixd is 8 if the number of c.c.
 *          is less than 254, 16 if the number of c.c is less than 0xfffe,
 *          and 32 otherwise.
 *      (3) If %depth = 8, the assigned label for the n-th component is
 *          1 + n % 254.  We use mod 254 because 0 is uniquely assigned
 *          to black: e.g., see pixcmapCreateRandom().  Likewise,
 *          if %depth = 16, the assigned label uses mod(2^16 - 2), and
 *          if %depth = 32, no mod is taken.
 * </pre>
 */
PIX *
pixConnCompTransform(PIX     *pixs,
                     l_int32  connect,
                     l_int32  depth)
{
l_int32  i, n, index, w, h, xb, yb, wb, hb;
BOXA    *boxa;
PIX     *pix1, *pix2, *pixd;
PIXA    *pixa;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (connect != 4 && connect != 8)
        return (PIX *)ERROR_PTR("connectivity must be 4 or 8", __func__, NULL);
    if (depth != 0 && depth != 8 && depth != 16 && depth != 32)
        return (PIX *)ERROR_PTR("depth must be 0, 8, 16 or 32", __func__, NULL);

    boxa = pixConnComp(pixs, &pixa, connect);
    n = pixaGetCount(pixa);
    boxaDestroy(&boxa);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (depth == 0) {
        if (n < 254)
            depth = 8;
        else if (n < 0xfffe)
            depth = 16;
        else
            depth = 32;
    }
    pixd = pixCreate(w, h, depth);
    pixSetSpp(pixd, 1);
    if (n == 0) {  /* no fg */
        pixaDestroy(&pixa);
        return pixd;
    }

       /* Label each component and blit it in */
    for (i = 0; i < n; i++) {
        pixaGetBoxGeometry(pixa, i, &xb, &yb, &wb, &hb);
        pix1 = pixaGetPix(pixa, i, L_CLONE);
        if (depth == 8) {
            index = 1 + (i % 254);
            pix2 = pixConvert1To8(NULL, pix1, 0, index);
        } else if (depth == 16) {
            index = 1 + (i % 0xfffe);
            pix2 = pixConvert1To16(NULL, pix1, 0, index);
        } else {  /* depth == 32 */
            index = 1 + i;
            pix2 = pixConvert1To32(NULL, pix1, 0, index);
        }
        pixRasterop(pixd, xb, yb, wb, hb, PIX_PAINT, pix2, 0, 0);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }

    pixaDestroy(&pixa);
    return pixd;
}


/*-----------------------------------------------------------------------*
 *         Label pixels by the area of their connected component         *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixConnCompAreaTransform()
 *
 * \param[in]     pixs       1 bpp
 * \param[in]     connect    connectivity: 4 or 8
 * \return   pixd   32 bpp, 1 spp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The pixel values in pixd label the area of the fg component
 *          to which the pixel belongs.  Pixels in the bg are labelled 0.
 *      (2) For purposes of visualization, the output can be converted
 *          to 8 bpp, using pixConvert32To8() or pixMaxDynamicRange().
 * </pre>
 */
PIX *
pixConnCompAreaTransform(PIX     *pixs,
                         l_int32  connect)
{
l_int32   i, n, npix, w, h, xb, yb, wb, hb;
l_int32  *tab8;
BOXA     *boxa;
PIX      *pix1, *pix2, *pixd;
PIXA     *pixa;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (connect != 4 && connect != 8)
        return (PIX *)ERROR_PTR("connectivity must be 4 or 8", __func__, NULL);

    boxa = pixConnComp(pixs, &pixa, connect);
    n = pixaGetCount(pixa);
    boxaDestroy(&boxa);
    pixGetDimensions(pixs, &w, &h, NULL);
    pixd = pixCreate(w, h, 32);
    pixSetSpp(pixd, 1);
    if (n == 0) {  /* no fg */
        pixaDestroy(&pixa);
        return pixd;
    }

       /* Label each component and blit it in */
    tab8 = makePixelSumTab8();
    for (i = 0; i < n; i++) {
        pixaGetBoxGeometry(pixa, i, &xb, &yb, &wb, &hb);
        pix1 = pixaGetPix(pixa, i, L_CLONE);
        pixCountPixels(pix1, &npix, tab8);
        pix2 = pixConvert1To32(NULL, pix1, 0, npix);
        pixRasterop(pixd, xb, yb, wb, hb, PIX_PAINT, pix2, 0, 0);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }

    pixaDestroy(&pixa);
    LEPT_FREE(tab8);
    return pixd;
}


/*-------------------------------------------------------------------------*
 *  Label pixels to allow incremental computation of connected components  *
 *-------------------------------------------------------------------------*/
/*!
 * \brief   pixConnCompIncrInit()
 *
 * \param[in]     pixs     1 bpp
 * \param[in]     conn     connectivity: 4 or 8
 * \param[out]    ppixd    32 bpp, with c.c. labelled
 * \param[out]    pptaa    with pixel locations indexed by c.c.
 * \param[out]    pncc     initial number of c.c.
 * \return   0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This labels the connected components in a 1 bpp pix, and
 *          additionally sets up a ptaa that lists the locations of pixels
 *          in each of the components.
 *      (2) It can be used to initialize the output image and arrays for
 *          an application that maintains information about connected
 *          components incrementally as pixels are added.
 *      (3) pixs can be empty or have some foreground pixels.
 *      (4) The connectivity is stored in pixd->special.
 *      (5) Always initialize with the first pta in ptaa being empty
 *          and representing the background value (index 0) in the pix.
 * </pre>
 */
l_ok
pixConnCompIncrInit(PIX     *pixs,
                    l_int32  conn,
                    PIX    **ppixd,
                    PTAA   **pptaa,
                    l_int32 *pncc)
{
l_int32  empty, w, h, ncc;
PIX     *pixd;
PTA     *pta;
PTAA    *ptaa;

    if (ppixd) *ppixd = NULL;
    if (pptaa) *pptaa = NULL;
    if (pncc) *pncc = 0;
    if (!ppixd || !pptaa || !pncc)
        return ERROR_INT("&pixd, &ptaa, &ncc not all defined", __func__, 1);
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs undefined or not 1 bpp", __func__, 1);
    if (conn != 4 && conn != 8)
        return ERROR_INT("connectivity must be 4 or 8", __func__, 1);

    pixGetDimensions(pixs, &w, &h, NULL);
    pixZero(pixs, &empty);
    if (empty) {
        *ppixd = pixCreate(w, h, 32);
        pixSetSpp(*ppixd, 1);
        pixSetSpecial(*ppixd, conn);
        *pptaa = ptaaCreate(0);
        pta = ptaCreate(1);
        ptaaAddPta(*pptaa, pta, L_INSERT);  /* reserve index 0 for background */
        return 0;
    }

        /* Set up the initial labeled image and indexed pixel arrays */
    if ((pixd = pixConnCompTransform(pixs, conn, 32)) == NULL)
        return ERROR_INT("pixd not made", __func__, 1);
    pixSetSpecial(pixd, conn);
    *ppixd = pixd;
    if ((ptaa = ptaaIndexLabeledPixels(pixd, &ncc)) == NULL)
        return ERROR_INT("ptaa not made", __func__, 1);
    *pptaa = ptaa;
    *pncc = ncc;
    return 0;
}


/*!
 * \brief   pixConnCompIncrAdd()
 *
 * \param[in]     pixs     32 bpp, with pixels labeled by c.c.
 * \param[in]     ptaa     with each pta of pixel locations indexed by c.c.
 * \param[out]    pncc     number of c.c
 * \param[in]     x,y      location of added pixel
 * \param[in]     debug    0 for no output; otherwise output whenever
 *                         debug <= nvals, up to debug == 3
 * \return   -1 if nothing happens; 0 if a pixel is added; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This adds a pixel and updates the labeled connected components.
 *          Before calling this function, initialize the process using
 *          pixConnCompIncrInit().
 *      (2) As a result of adding a pixel, one of the following can happen,
 *          depending on the number of neighbors with non-zero value:
 *          (a) nothing: the pixel is already a member of a c.c.
 *          (b) no neighbors: a new component is added, increasing the
 *              number of c.c.
 *          (c) one neighbor: the pixel is added to an existing c.c.
 *          (d) more than one neighbor: the added pixel causes joining of
 *              two or more c.c., reducing the number of c.c.  A maximum
 *              of 4 c.c. can be joined.
 *      (3) When two c.c. are joined, the pixels in the larger index are
 *          relabeled to those of the smaller in pixs, and their locations
 *          are transferred to the pta with the smaller index in the ptaa.
 *          The pta corresponding to the larger index is then deleted.
 *      (4) This is an efficient implementation of a "union-find" operation,
 *          which supports the generation and merging of disjoint sets
 *          of pixels.  This function can be called about 1.3 million times
 *          per second.
 * </pre>
 */
l_int32
pixConnCompIncrAdd(PIX       *pixs,
                   PTAA      *ptaa,
                   l_int32   *pncc,
                   l_float32  x,
                   l_float32  y,
                   l_int32    debug)
{
l_int32   conn, i, j, w, h, count, nvals, ns, firstindex;
l_uint32  val;
l_int32  *neigh;
PTA      *ptas, *ptad;

    if (!pixs || pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs not defined or not 32 bpp", __func__, 1);
    if (!ptaa)
        return ERROR_INT("ptaa not defined", __func__, 1);
    if (!pncc)
        return ERROR_INT("&ncc not defined", __func__, 1);
    conn = pixs->special;
    if (conn != 4 && conn != 8)
        return ERROR_INT("connectivity must be 4 or 8", __func__, 1);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (x < 0 || x >= w)
        return ERROR_INT("invalid x pixel location", __func__, 1);
    if (y < 0 || y >= h)
        return ERROR_INT("invalid y pixel location", __func__, 1);

    pixGetPixel(pixs, x, y, &val);
    if (val > 0)  /* already belongs to a set */
        return -1;

        /* Find unique neighbor pixel values in increasing order of value.
         * If %nvals > 0, these are returned in the %neigh array, which
         * is of size %nvals.  Note that the pixel values in each
         * connected component are used as the index into the pta
         * array of the ptaa, giving the pixel locations. */
    pixGetSortedNeighborValues(pixs, x, y, conn, &neigh, &nvals);

        /* If there are no neighbors, just add a new component */
    if (nvals == 0) {
        count = ptaaGetCount(ptaa);
        pixSetPixel(pixs, x, y, count);
        ptas = ptaCreate(1);
        ptaAddPt(ptas, x, y);
        ptaaAddPta(ptaa, ptas, L_INSERT);
        *pncc += 1;
        LEPT_FREE(neigh);
        return 0;
    }

        /* Otherwise, there is at least one neighbor.  Add the pixel
         * to the first neighbor c.c. */
    firstindex = neigh[0];
    pixSetPixel(pixs, x, y, firstindex);
    ptaaAddPt(ptaa, neigh[0], x, y);
    if (nvals == 1) {
        if (debug == 1)
            lept_stderr("nvals = %d: neigh = (%d)\n", nvals, neigh[0]);
        LEPT_FREE(neigh);
        return 0;
    }

        /* If nvals > 1, there are at least 2 neighbors, so this pixel
         * joins at least one pair of existing c.c.  Join each component
         * to the first component in the list, which is the one with
         * the smallest integer label.  This is done in two steps:
         *  (a) re-label the pixels in the component to the label of the
         *      first component, and
         *  (b) save the pixel locations in the pta for the first component. */
    if (nvals == 2) {
        if (debug >= 1 && debug <= 2) {
            lept_stderr("nvals = %d: neigh = (%d,%d)\n", nvals,
                        neigh[0], neigh[1]);
        }
    } else if (nvals == 3) {
        if (debug >= 1 && debug <= 3) {
            lept_stderr("nvals = %d: neigh = (%d,%d,%d)\n", nvals,
                        neigh[0], neigh[1], neigh[2]);
        }
    } else {  /* nvals == 4 */
        if (debug >= 1 && debug <= 4) {
            lept_stderr("nvals = %d: neigh = (%d,%d,%d,%d)\n", nvals,
                        neigh[0], neigh[1], neigh[2], neigh[3]);
        }
    }
    ptad = ptaaGetPta(ptaa, firstindex, L_CLONE);
    for (i = 1; i < nvals; i++) {
        ptas = ptaaGetPta(ptaa, neigh[i], L_CLONE);
        ns = ptaGetCount(ptas);
        for (j = 0; j < ns; j++) {  /* relabel pixels */
            ptaGetPt(ptas, j, &x, &y);
            pixSetPixel(pixs, x, y, firstindex);
        }
        ptaJoin(ptad, ptas, 0, -1);  /* add relabeled pixel locations */
        *pncc -= 1;
        ptaDestroy(&ptaa->pta[neigh[i]]);
        ptaDestroy(&ptas);  /* the clone */
    }
    ptaDestroy(&ptad);  /* the clone */
    LEPT_FREE(neigh);
    return 0;
}


/*!
 * \brief   pixGetSortedNeighborValues()
 *
 * \param[in]     pixs     8, 16 or 32 bpp, with pixels labeled by c.c.
 * \param[in]     x, y     location of pixel
 * \param[in]     conn     4 or 8 connected neighbors
 * \param[out]    pneigh   array of integers, to be filled with
 *                         the values of the neighbors, if any
 * \param[out]    pnvals   the number of unique neighbor values found
 * \return   0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The returned %neigh array is the unique set of neighboring
 *          pixel values, of size nvals, sorted from smallest to largest.
 *          The value 0, which represents background pixels that do
 *          not belong to any set of connected components, is discarded.
 *      (2) If there are no neighbors, this returns %neigh = NULL; otherwise,
 *          the caller must free the array.
 *      (3) For either 4 or 8 connectivity, the maximum number of unique
 *          neighbor values is 4.
 * </pre>
 */
l_ok
pixGetSortedNeighborValues(PIX       *pixs,
                           l_int32    x,
                           l_int32    y,
                           l_int32    conn,
                           l_int32  **pneigh,
                           l_int32   *pnvals)
{
l_int32       i, npt, index;
l_int32       neigh[4];
l_uint32      val;
l_float32     fx, fy;
L_ASET       *aset;
L_ASET_NODE  *node;
PTA          *pta;
RB_TYPE       key;

    if (pneigh) *pneigh = NULL;
    if (pnvals) *pnvals = 0;
    if (!pneigh || !pnvals)
        return ERROR_INT("&neigh and &nvals not both defined", __func__, 1);
    if (!pixs || pixGetDepth(pixs) < 8)
        return ERROR_INT("pixs not defined or depth < 8", __func__, 1);

        /* Identify the locations of nearest neighbor pixels */
    if ((pta = ptaGetNeighborPixLocs(pixs, x, y, conn)) == NULL)
        return ERROR_INT("pta of neighbors not made", __func__, 1);

        /* Find the pixel values and insert into a set as keys */
    aset = l_asetCreate(L_UINT_TYPE);
    npt = ptaGetCount(pta);
    for (i = 0; i < npt; i++) {
        ptaGetPt(pta, i, &fx, &fy);
        pixGetPixel(pixs, (l_int32)fx, (l_int32)fy, &val);
        key.utype = val;
        l_asetInsert(aset, key);
    }

        /* Extract the set keys and put them into the %neigh array.
         * Omit the value 0, which indicates the pixel doesn't
         * belong to one of the sets of connected components. */
    node = l_asetGetFirst(aset);
    index = 0;
    while (node) {
        val = node->key.utype;
        if (val > 0)
            neigh[index++] = (l_int32)val;
        node = l_asetGetNext(node);
    }
    *pnvals = index;
    if (index > 0) {
        *pneigh = (l_int32 *)LEPT_CALLOC(index, sizeof(l_int32));
        for (i = 0; i < index; i++)
            (*pneigh)[i] = neigh[i];
    }

    ptaDestroy(&pta);
    l_asetDestroy(&aset);
    return 0;
}


/*-----------------------------------------------------------------------*
 *          Label pixels with spatially-dependent color coding           *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixLocToColorTransform()
 *
 * \param[in]     pixs    1 bpp
 * \return   pixd   32 bpp rgb, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This generates an RGB image where each component value
 *          is coded depending on the (x.y) location and the size
 *          of the fg connected component that the pixel in pixs belongs to.
 *          It is independent of the 4-fold orthogonal orientation, and
 *          only weakly depends on translations and small angle rotations.
 *          Background pixels are black.
 *      (2) Such encodings can be compared between two 1 bpp images
 *          by performing this transform and calculating the
 *          "earth-mover" distance on the resulting R,G,B histograms.
 * </pre>
 */
PIX *
pixLocToColorTransform(PIX  *pixs)
{
l_int32    w, h, w2, h2, wpls, wplr, wplg, wplb, wplcc, i, j, rval, gval, bval;
l_float32  invw2, invh2;
l_uint32  *datas, *datar, *datag, *datab, *datacc;
l_uint32  *lines, *liner, *lineg, *lineb, *linecc;
PIX       *pix1, *pixcc, *pixr, *pixg, *pixb, *pixd;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);

        /* Label each pixel with the area of the c.c. to which it belongs.
         * Clip the result to 255 in an 8 bpp pix. This is used for
         * the blue component of pixd.  */
    pixGetDimensions(pixs, &w, &h, NULL);
    w2 = w / 2;
    h2 = h / 2;
    invw2 = 255.0f / (l_float32)w2;
    invh2 = 255.0f / (l_float32)h2;
    pix1 = pixConnCompAreaTransform(pixs, 8);
    pixcc = pixConvert32To8(pix1, L_LS_TWO_BYTES, L_CLIP_TO_FF);
    pixDestroy(&pix1);

        /* Label the red and green components depending on the location
         * of the fg pixels, in a way that is 4-fold rotationally invariant. */
    pixr = pixCreate(w, h, 8);
    pixg = pixCreate(w, h, 8);
    pixb = pixCreate(w, h, 8);
    wpls = pixGetWpl(pixs);
    wplr = pixGetWpl(pixr);
    wplg = pixGetWpl(pixg);
    wplb = pixGetWpl(pixb);
    wplcc = pixGetWpl(pixcc);
    datas = pixGetData(pixs);
    datar = pixGetData(pixr);
    datag = pixGetData(pixg);
    datab = pixGetData(pixb);
    datacc = pixGetData(pixcc);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        liner = datar + i * wplr;
        lineg = datag + i * wplg;
        lineb = datab + i * wplb;
        linecc = datacc+ i * wplcc;
        for (j = 0; j < w; j++) {
            if (GET_DATA_BIT(lines, j) == 0) continue;
            if (w < h) {
                rval = invh2 * L_ABS((l_float32)(i - h2));
                gval = invw2 * L_ABS((l_float32)(j - w2));
            } else {
                rval = invw2 * L_ABS((l_float32)(j - w2));
                gval = invh2 * L_ABS((l_float32)(i - h2));
            }
            bval = GET_DATA_BYTE(linecc, j);
            SET_DATA_BYTE(liner, j, rval);
            SET_DATA_BYTE(lineg, j, gval);
            SET_DATA_BYTE(lineb, j, bval);
        }
    }
    pixd = pixCreateRGBImage(pixr, pixg, pixb);

    pixDestroy(&pixcc);
    pixDestroy(&pixr);
    pixDestroy(&pixg);
    pixDestroy(&pixb);
    return pixd;
}

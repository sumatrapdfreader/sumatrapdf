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
 * \file  partition.c
 * <pre>
 *
 *      Whitespace block extraction
 *          BOXA            *boxaGetWhiteblocks()
 *
 *      Helpers
 *          static PARTEL   *partelCreate()
 *          static void      partelDestroy()
 *          static l_int32   partelSetSize()
 *          static BOXA     *boxaGenerateSubboxes()
 *          static BOX      *boxaSelectPivotBox()
 *          static l_int32   boxaCheckIfOverlapIsSmall()
 *          BOXA            *boxaPruneSortedOnOverlap()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

/*! Partition element */
struct PartitionElement {
    l_float32  size;  /* sorting key */
    BOX       *box;   /* region of the element */
    BOXA      *boxa;  /* set of intersecting boxes */
};
typedef struct PartitionElement PARTEL;

static PARTEL * partelCreate(BOX *box);
static void partelDestroy(PARTEL **ppartel);
static l_int32 partelSetSize(PARTEL *partel, l_int32 sortflag);
static BOXA * boxaGenerateSubboxes(BOX *box, BOXA *boxa, l_int32 maxperim,
                                   l_float32  fract);
static BOX * boxaSelectPivotBox(BOX *box, BOXA *boxa, l_int32 maxperim,
                                l_float32 fract);
static l_int32 boxCheckIfOverlapIsBig(BOX *box, BOXA *boxa,
                                      l_float32 maxoverlap);

static const l_int32  DefaultMaxPops = 20000;


#ifndef  NO_CONSOLE_IO
#define  OUTPUT_HEAP_STATS   0
#endif  /* ~NO_CONSOLE_IO */

/*------------------------------------------------------------------*
 *                    Whitespace block extraction                   *
 *------------------------------------------------------------------*/
/*!
 * \brief   boxaGetWhiteblocks()
 *
 * \param[in]    boxas        typ. a set of bounding boxes of fg components
 * \param[in]    box          initial region; typically including all boxes
 *                            in boxas; if null, it computes the region to
 *                            include all boxes in boxas
 * \param[in]    sortflag     L_SORT_BY_WIDTH, L_SORT_BY_HEIGHT,
 *                            L_SORT_BY_MIN_DIMENSION, L_SORT_BY_MAX_DIMENSION,
 *                            L_SORT_BY_PERIMETER, L_SORT_BY_AREA
 * \param[in]    maxboxes     max number of output whitespace boxes; e.g., 100
 * \param[in]    maxoverlap   maximum fractional overlap of a box by any
 *                            of the larger boxes; e.g., 0.2
 * \param[in]    maxperim     maximum half-perimeter, in pixels, for which
 *                            pivot is selected by proximity to box centroid;
 *                            e.g., 200
 * \param[in]    fract        fraction of box diagonal that is an acceptable
 *                            distance from the box centroid to select
 *                            the pivot; e.g., 0.2
 * \param[in]    maxpops      max number of pops from the heap; use 0 as default
 * \return  boxa  of sorted whitespace boxes, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This uses the elegant Breuel algorithm, found in "Two
 *          Geometric Algorithms for Layout Analysis", 2002,
 *          url: "citeseer.ist.psu.edu/breuel02two.html".
 *          It starts with the bounding boxes (b.b.) of the connected
 *          components (c.c.) in a region, along with the rectangle
 *          representing that region.  It repeatedly divides the
 *          rectangle into four maximal rectangles that exclude a
 *          pivot rectangle, sorting them in a priority queue
 *          according to one of the six sort flags.  It returns a boxa
 *          of the "largest" set that have no intersection with boxes
 *          from the input boxas.
 *      (2) If box == NULL, the initial region is the minimal region
 *          that includes the origin and every box in boxas.
 *      (3) maxboxes is the maximum number of whitespace boxes that will
 *          be returned.  The actual number will depend on the image
 *          and the values chosen for maxoverlap and maxpops.  In many
 *          cases, the actual number will be 'maxboxes'.
 *      (4) maxoverlap allows pruning of whitespace boxes depending on
 *          the overlap.  To avoid all pruning, use maxoverlap = 1.0.
 *          To select only boxes that have no overlap with each other
 *          (maximal pruning), choose maxoverlap = 0.0.
 *          Otherwise, no box can have more than the 'maxoverlap' fraction
 *          of its area overlapped by any larger (in the sense of the
 *          sortflag) box.
 *      (5) Choose maxperim (actually, maximum half-perimeter) to
 *          represent a c.c. that is small enough so that you don't care
 *          about the white space that could be inside of it.  For all such
 *          c.c., the pivot for 'quadfurcation' of a rectangle is selected
 *          as having a reasonable proximity to the rectangle centroid.
 *      (6) Use fract in the range [0.0 ... 1.0].  Set fract = 0.0
 *          to choose the small box nearest the centroid as the pivot.
 *          If you choose fract > 0.0, it is suggested that you call
 *          boxaPermuteRandom() first, to permute the boxes (see usage below).
 *          This should reduce the search time for each of the pivot boxes.
 *      (7) Choose maxpops to be the maximum number of rectangles that
 *          are popped from the heap.  This is an indirect way to limit the
 *          execution time.  Use 0 for default (a fairly large number).
 *          At any time, you can expect the heap to contain about
 *          2.5 times as many boxes as have been popped off.
 *      (8) The output result is a sorted set of overlapping
 *          boxes, constrained by 'maxboxes', 'maxoverlap' and 'maxpops'.
 *      (9) The main defect of the method is that it abstracts out the
 *          actual components, retaining only the b.b. for analysis.
 *          Consider a component with a large b.b.  If this is chosen
 *          as a pivot, all white space inside is immediately taken
 *          out of consideration.  Furthermore, even if it is never chosen
 *          as a pivot, as the partitioning continues, at no time will
 *          any of the whitespace inside this component be part of a
 *          rectangle with zero overlapping boxes.  Thus, the interiors
 *          of all boxes are necessarily excluded from the union of
 *          the returned whitespace boxes.
 *     (10) It should be noted that the algorithm puts a large number
 *          of partels on the queue.  Setting a limit of X partels to
 *          remove from the queue, one typically finds that there will be
 *          several times that number (say, 2X - 3X) left on the queue.
 *          For an efficient algorithm to find the largest white or
 *          or black rectangles, without permitting them to overlap,
 *          see pixFindLargeRectangles().
 *     (11) USAGE: One way to accommodate to this weakness is to remove such
 *          large b.b. before starting the computation.  For example,
 *          if 'box' is an input image region containing 'boxa' b.b. of c.c.:
 *
 *                   // Faster pivot choosing
 *               boxaPermuteRandom(boxa, boxa);
 *
 *                   // Remove anything either large width or height
 *               boxat = boxaSelectBySize(boxa, maxwidth, maxheight,
 *                                        L_SELECT_IF_BOTH, L_SELECT_IF_LT,
 *                                        NULL);
 *
 *               boxad = boxaGetWhiteblocks(boxat, box, type, maxboxes,
 *                                          maxoverlap, maxperim, fract,
 *                                          maxpops);
 *
 *          The result will be rectangular regions of "white space" that
 *          extend into (and often through) the excluded components.
 *     (11) As a simple example, suppose you wish to find the columns on a page.
 *          First exclude large c.c. that may block the columns, and then call:
 *
 *               boxad = boxaGetWhiteblocks(boxa, box, L_SORT_BY_HEIGHT,
 *                                          20, 0.15, 200, 0.2, 2000);
 *
 *          to get the 20 tallest boxes with no more than 0.15 overlap
 *          between a box and any of the taller ones, and avoiding the
 *          use of any c.c. with a b.b. half perimeter greater than 200
 *          as a pivot.
 * </pre>
 */
BOXA *
boxaGetWhiteblocks(BOXA      *boxas,
                   BOX       *box,
                   l_int32    sortflag,
                   l_int32    maxboxes,
                   l_float32  maxoverlap,
                   l_int32    maxperim,
                   l_float32  fract,
                   l_int32    maxpops)
{
l_int32  i, w, h, n, nsub, npush, npop;
BOX     *boxsub;
BOXA    *boxa, *boxa4, *boxasub, *boxad;
PARTEL  *partel;
L_HEAP  *lh;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (sortflag != L_SORT_BY_WIDTH && sortflag != L_SORT_BY_HEIGHT &&
        sortflag != L_SORT_BY_MIN_DIMENSION &&
        sortflag != L_SORT_BY_MAX_DIMENSION &&
        sortflag != L_SORT_BY_PERIMETER && sortflag != L_SORT_BY_AREA)
        return (BOXA *)ERROR_PTR("invalid sort flag", __func__, NULL);
    if (maxboxes < 1) {
        maxboxes = 1;
        L_WARNING("setting maxboxes = 1\n", __func__);
    }
    if (maxoverlap < 0.0 || maxoverlap > 1.0)
        return (BOXA *)ERROR_PTR("invalid maxoverlap", __func__, NULL);
    if (maxpops == 0)
        maxpops = DefaultMaxPops;

    if (!box) {
        boxaGetExtent(boxas, &w, &h, NULL);
        box = boxCreate(0, 0, w, h);
    }

        /* Prime the heap */
    lh = lheapCreate(20, L_SORT_DECREASING);
    partel = partelCreate(box);
    partel->boxa = boxaCopy(boxas, L_CLONE);
    partelSetSize(partel, sortflag);
    lheapAdd(lh, partel);

    npush = 1;
    npop = 0;
    boxad = boxaCreate(0);
    while (1) {
        if ((partel = (PARTEL *)lheapRemove(lh)) == NULL)  /* we're done */
            break;

        npop++;  /* How many boxes have we retrieved from the queue? */
        if (npop > maxpops) {
            partelDestroy(&partel);
            break;
        }

            /* Extract the contents */
        boxa = boxaCopy(partel->boxa, L_CLONE);
        box = boxClone(partel->box);
        partelDestroy(&partel);

            /* Can we output this one? */
        n = boxaGetCount(boxa);
        if (n == 0) {
            if (boxCheckIfOverlapIsBig(box, boxad, maxoverlap) == 0)
                boxaAddBox(boxad, box, L_INSERT);
            else
                boxDestroy(&box);
            boxaDestroy(&boxa);
            if (boxaGetCount(boxad) >= maxboxes)  /* we're done */
                break;
            continue;
        }


            /* Generate up to 4 subboxes and put them on the heap */
        boxa4 = boxaGenerateSubboxes(box, boxa, maxperim, fract);
        boxDestroy(&box);
        nsub = boxaGetCount(boxa4);
        for (i = 0; i < nsub; i++) {
            boxsub = boxaGetBox(boxa4, i, L_CLONE);
            boxasub = boxaIntersectsBox(boxa, boxsub);
            partel = partelCreate(boxsub);
            partel->boxa = boxasub;
            partelSetSize(partel, sortflag);
            lheapAdd(lh, partel);
            boxDestroy(&boxsub);
        }
        npush += nsub;  /* How many boxes have we put on the queue? */

/*        boxaWriteStderr(boxa4); */

        boxaDestroy(&boxa4);
        boxaDestroy(&boxa);
    }

#if  OUTPUT_HEAP_STATS
    lept_stderr("Heap statistics:\n");
    lept_stderr("  Number of boxes pushed: %d\n", npush);
    lept_stderr("  Number of boxes popped: %d\n", npop);
    lept_stderr("  Number of boxes on heap: %d\n", lheapGetCount(lh));
#endif  /* OUTPUT_HEAP_STATS */

        /* Clean up the heap */
    while ((partel = (PARTEL *)lheapRemove(lh)) != NULL)
        partelDestroy(&partel);
    lheapDestroy(&lh, FALSE);

    return boxad;
}


/*------------------------------------------------------------------*
 *                               Helpers                            *
 *------------------------------------------------------------------*/
/*!
 * \brief   partelCreate()
 *
 * \param[in]    box    region; inserts a copy
 * \return  partel, or NULL on error
 */
static PARTEL *
partelCreate(BOX  *box)
{
PARTEL  *partel;

    partel = (PARTEL *)LEPT_CALLOC(1, sizeof(PARTEL));
    partel->box = boxCopy(box);
    return partel;
}


/*!
 * \brief   partelDestroy()
 *
 * \param[in,out]   ppartel   contents will be set to null before returning
 * \return  void
 */
static void
partelDestroy(PARTEL  **ppartel)
{
PARTEL  *partel;

    if (ppartel == NULL) {
        L_WARNING("ptr address is null!\n", __func__);
        return;
    }

    if ((partel = *ppartel) == NULL)
        return;

    boxDestroy(&partel->box);
    boxaDestroy(&partel->boxa);
    LEPT_FREE(partel);
    *ppartel = NULL;
    return;
}


/*!
 * \brief   partelSetSize()
 *
 * \param[in]    partel
 * \param[in]    sortflag   L_SORT_BY_WIDTH, L_SORT_BY_HEIGHT,
 *                          L_SORT_BY_MIN_DIMENSION, L_SORT_BY_MAX_DIMENSION,
 *                          L_SORT_BY_PERIMETER, L_SORT_BY_AREA
 * \return  0 if OK, 1 on error
 */
static l_int32
partelSetSize(PARTEL  *partel,
              l_int32  sortflag)
{
l_int32  w, h;

    if (!partel)
        return ERROR_INT("partel not defined", __func__, 1);

    boxGetGeometry(partel->box, NULL, NULL, &w, &h);
    if (sortflag == L_SORT_BY_WIDTH)
        partel->size = (l_float32)w;
    else if (sortflag == L_SORT_BY_HEIGHT)
        partel->size = (l_float32)h;
    else if (sortflag == L_SORT_BY_MIN_DIMENSION)
        partel->size = (l_float32)L_MIN(w, h);
    else if (sortflag == L_SORT_BY_MAX_DIMENSION)
        partel->size = (l_float32)L_MAX(w, h);
    else if (sortflag == L_SORT_BY_PERIMETER)
        partel->size = (l_float32)(w + h);
    else if (sortflag == L_SORT_BY_AREA)
        partel->size = (l_float32)(w * h);
    else
        return ERROR_INT("invalid sortflag", __func__, 1);
    return 0;
}


/*!
 * \brief   boxaGenerateSubboxes()
 *
 * \param[in]    box         region to be split into up to four overlapping
 *                           subregions
 * \param[in]    boxa        boxes of rectangles intersecting the box
 * \param[in]    maxperim    maximum half-perimeter for which pivot
 *                           is selected by proximity to box centroid
 * \param[in]    fract       fraction of box diagonal that is an acceptable
 *                           distance from the box centroid to select the pivot
 * \return  boxa             of four or less overlapping subrectangles of
 *                           the box, or NULL on error
 */
static BOXA *
boxaGenerateSubboxes(BOX       *box,
                     BOXA      *boxa,
                     l_int32    maxperim,
                     l_float32  fract)
{
l_int32  x, y, w, h, xp, yp, wp, hp;
BOX     *boxp;  /* pivot box */
BOX     *boxsub;
BOXA    *boxa4;

    if (!box)
        return (BOXA *)ERROR_PTR("box not defined", __func__, NULL);
    if (!boxa)
        return (BOXA *)ERROR_PTR("boxa not defined", __func__, NULL);

    boxa4 = boxaCreate(4);
    boxp = boxaSelectPivotBox(box, boxa, maxperim, fract);
    boxGetGeometry(box, &x, &y, &w, &h);
    boxGetGeometry(boxp, &xp, &yp, &wp, &hp);
    boxDestroy(&boxp);
    if (xp > x) {   /* left sub-box */
        boxsub = boxCreate(x, y, xp - x, h);
        boxaAddBox(boxa4, boxsub, L_INSERT);
    }
    if (yp > y) {   /* top sub-box */
        boxsub = boxCreate(x, y, w, yp - y);
        boxaAddBox(boxa4, boxsub, L_INSERT);
    }
    if (xp + wp < x + w) {   /* right sub-box */
        boxsub = boxCreate(xp + wp, y, x + w - xp - wp, h);
        boxaAddBox(boxa4, boxsub, L_INSERT);
    }
    if (yp + hp < y + h) {   /* bottom sub-box */
        boxsub = boxCreate(x, yp + hp, w, y + h - yp - hp);
        boxaAddBox(boxa4, boxsub, L_INSERT);
    }

    return boxa4;
}


/*!
 * \brief   boxaSelectPivotBox()
 *
 * \param[in]    box        containing box; to be split by the pivot box
 * \param[in]    boxa       boxes of rectangles, from which 1 is to be chosen
 * \param[in]    maxperim   maximum half-perimeter for which pivot
 *                          is selected by proximity to box centroid
 * \param[in]    fract      fraction of box diagonal that is an acceptable
 *                          distance from the box centroid to select the pivot
 * \return  box             pivot box for subdivision into 4 rectangles,
 *                          or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a tricky piece that wasn't discussed in the
 *          Breuel's 2002 paper.
 *      (2) Selects a box from boxa whose centroid is reasonably close to
 *          the centroid of the containing box (xc, yc) and whose
 *          half-perimeter does not exceed the maxperim value.
 *      (3) If there are no boxes in the boxa that are small enough,
 *          then it selects the smallest of the larger boxes,
 *          without reference to its location in the containing box.
 *      (4) If a small box has a centroid at a distance from the
 *          centroid of the containing box that is not more than
 *          the fraction 'fract' of the diagonal of the containing
 *          box, that box is chosen as the pivot, terminating the
 *          search for the nearest small box.
 *      (5) Use fract in the range [0.0 ... 1.0].  Set fract = 0.0
 *          to choose the small box nearest the centroid.
 *      (6) Choose maxperim to represent a connected component that is
 *          small enough so that you don't care about the white space
 *          that could be inside of it.
 * </pre>
 */
static BOX *
boxaSelectPivotBox(BOX       *box,
                   BOXA      *boxa,
                   l_int32    maxperim,
                   l_float32  fract)
{
l_int32    i, n, bw, bh, w, h;
l_int32    smallfound, minindex, perim, minsize;
l_float32  delx, dely, mindist, threshdist, dist, x, y, cx, cy;
BOX       *boxt;

    if (!box)
        return (BOX *)ERROR_PTR("box not defined", __func__, NULL);
    if (!boxa)
        return (BOX *)ERROR_PTR("boxa not defined", __func__, NULL);
    n = boxaGetCount(boxa);
    if (n == 0)
        return (BOX *)ERROR_PTR("no boxes in boxa", __func__, NULL);
    if (fract < 0.0 || fract > 1.0) {
        L_WARNING("fract out of bounds; using 0.0\n", __func__);
        fract = 0.0;
    }

    boxGetGeometry(box, NULL, NULL, &w, &h);
    boxGetCenter(box, &x, &y);
    threshdist = fract * (w * w + h * h);
    mindist = 1000000000.;
    minindex = 0;
    smallfound = FALSE;
    for (i = 0; i < n; i++) {
        boxt = boxaGetBox(boxa, i, L_CLONE);
        boxGetGeometry(boxt, NULL, NULL, &bw, &bh);
        boxGetCenter(boxt, &cx, &cy);
        boxDestroy(&boxt);
        if (bw + bh > maxperim)
            continue;
        smallfound = TRUE;
        delx = cx - x;
        dely = cy - y;
        dist = delx * delx + dely * dely;
        if (dist <= threshdist)
            return boxaGetBox(boxa, i, L_COPY);
        if (dist < mindist) {
            minindex = i;
            mindist = dist;
        }
    }

        /* If there are small boxes but none are within 'fract' of the
         * centroid, return the nearest one. */
    if (smallfound == TRUE)
        return boxaGetBox(boxa, minindex, L_COPY);

        /* No small boxes; return the smallest of the large boxes */
    minsize = 1000000000;
    minindex = 0;
    for (i = 0; i < n; i++) {
        boxaGetBoxGeometry(boxa, i, NULL, NULL, &bw, &bh);
        perim = bw + bh;
        if (perim < minsize) {
            minsize = perim;
            minindex = i;
        }
    }
    return boxaGetBox(boxa, minindex, L_COPY);
}


/*!
 * \brief   boxCheckIfOverlapIsBig()
 *
 * \param[in]    box          to be tested
 * \param[in]    boxa         of boxes already stored
 * \param[in]    maxoverlap   maximum fractional overlap of the input box
 *                            by any of the boxes in boxa
 * \return      0 if box has small overlap with every box in boxa;
 *              1 otherwise or on error
 */
static l_int32
boxCheckIfOverlapIsBig(BOX       *box,
                       BOXA      *boxa,
                       l_float32  maxoverlap)
{
l_int32    i, n, bigoverlap;
l_float32  fract;
BOX       *boxt;

    if (!box)
        return ERROR_INT("box not defined", __func__, 1);
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if (maxoverlap < 0.0 || maxoverlap > 1.0)
        return ERROR_INT("invalid maxoverlap", __func__, 1);

    n = boxaGetCount(boxa);
    if (n == 0 || maxoverlap == 1.0)
        return 0;

    bigoverlap = 0;
    for (i = 0; i < n; i++) {
        boxt = boxaGetBox(boxa, i, L_CLONE);
        boxOverlapFraction(boxt, box, &fract);
        boxDestroy(&boxt);
        if (fract > maxoverlap) {
            bigoverlap = 1;
            break;
        }
    }

    return bigoverlap;
}


/*!
 * \brief   boxaPruneSortedOnOverlap()
 *
 * \param[in]    boxas        sorted by size in decreasing order
 * \param[in]    maxoverlap   maximum fractional overlap of a box by any
 *                            of the larger boxes
 * \return  boxad pruned, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This selectively removes smaller boxes when they are overlapped
 *          by any larger box by more than the input 'maxoverlap' fraction.
 *      (2) To avoid all pruning, use maxoverlap = 1.0.  To select only
 *          boxes that have no overlap with each other (maximal pruning),
 *          set maxoverlap = 0.0.
 *      (3) If there are no boxes in boxas, returns an empty boxa.
 * </pre>
 */
BOXA *
boxaPruneSortedOnOverlap(BOXA      *boxas,
                         l_float32  maxoverlap)
{
l_int32    i, j, n, remove;
l_float32  fract;
BOX       *box1, *box2;
BOXA      *boxad;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (maxoverlap < 0.0 || maxoverlap > 1.0)
        return (BOXA *)ERROR_PTR("invalid maxoverlap", __func__, NULL);

    n = boxaGetCount(boxas);
    if (n == 0 || maxoverlap == 1.0)
        return boxaCopy(boxas, L_COPY);

    boxad = boxaCreate(0);
    box2 = boxaGetBox(boxas, 0, L_COPY);
    boxaAddBox(boxad, box2, L_INSERT);
    for (j = 1; j < n; j++) {   /* prune on j */
        box2 = boxaGetBox(boxas, j, L_COPY);
        remove = FALSE;
        for (i = 0; i < j; i++) {   /* test on i */
            box1 = boxaGetBox(boxas, i, L_CLONE);
            boxOverlapFraction(box1, box2, &fract);
            boxDestroy(&box1);
            if (fract > maxoverlap) {
                remove = TRUE;
                break;
            }
        }
        if (remove == TRUE)
            boxDestroy(&box2);
        else
            boxaAddBox(boxad, box2, L_INSERT);
    }

    return boxad;
}

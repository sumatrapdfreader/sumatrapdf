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
 * \file ccbord.c
 * <pre>
 *
 *     CCBORDA and CCBORD creation and destruction
 *         static CCBORDA  *ccbaCreate()
 *         void            *ccbaDestroy()
 *         static CCBORD   *ccbCreate()
 *         static void      ccbDestroy()
 *
 *     CCBORDA addition
 *         static l_int32   ccbaAddCcb()
 *         static l_int32   ccbaExtendArray()
 *
 *     CCBORDA accessors
 *         static l_int32   ccbaGetCount()
 *         static l_int32   ccbaGetCcb()
 *
 *     Top-level border-finding routines
 *         CCBORDA         *pixGetAllCCBorders()
 *         static CCBORD   *pixGetCCBorders()
 *         PTAA            *pixGetOuterBordersPtaa()
 *         static PTA      *pixGetOuterBorderPta()
 *
 *     Lower-level border location routines
 *         PTAA            *pixGetOuterBorder()
 *         static l_int32   pixGetHoleBorder()
 *         static l_int32   findNextBorderPixel()
 *         static void      locateOutsideSeedPixel()
 *
 *     Border conversions
 *         l_int32          ccbaGenerateGlobalLocs()
 *         l_int32          ccbaGenerateStepChains()
 *         l_int32          ccbaStepChainsToPixCoords()
 *         l_int32          ccbaGenerateSPGlobalLocs()
 *
 *     Conversion to single path
 *         l_int32          ccbaGenerateSinglePath()
 *         static PTA      *getCutPathForHole()
 *
 *     Border and full image rendering
 *         PIX             *ccbaDisplayBorder()
 *         PIX             *ccbaDisplaySPBorder()
 *         PIX             *ccbaDisplayImage1()
 *         PIX             *ccbaDisplayImage2()
 *
 *     Serialize for I/O
 *         l_int32          ccbaWrite()
 *         l_int32          ccbaWriteStream()
 *         l_int32          ccbaRead()
 *         l_int32          ccbaReadStream()
 *
 *     SVG output
 *         l_int32          ccbaWriteSVG()
 *         char            *ccbaWriteSVGString()
 *
 *
 *     Border finding is tricky because components can have
 *     holes, which also need to be traced out.  The outer
 *     border can be connected with all the hole borders,
 *     so that there is a single border for each component.
 *     [Alternatively, the connecting paths can be eliminated if
 *     you're willing to have a set of borders for each
 *     component (an exterior border and some number of
 *     interior ones), with "line to" operations tracing
 *     out each border and "move to" operations going from
 *     one border to the next.]
 *
 *     Here's the plan.  We get the pix for each connected
 *     component, and trace its exterior border.  We then
 *     find the holes (if any) in the pix, and separately
 *     trace out their borders, all using the same
 *     border-following rule that has ON pixels on the right
 *     side of the path.
 *
 *     [For svg, we may want to turn each set of borders for a c.c.
 *     into a closed path.  This can be done by tunnelling
 *     through the component from the outer border to each of the
 *     holes, going in and coming out along the same path so
 *     the connection will be invisible in any rendering
 *     (display or print) from the outline.  The result is a
 *     closed path, where the outside border is traversed
 *     cw and each hole is traversed ccw.  The svg renderer
 *     is assumed to handle these closed borders properly.]
 *
 *     Each border is a closed path that is traversed in such
 *     a way that the stuff inside the c.c. is on the right
 *     side of the traveller.  The border of a singly-connected
 *     component is thus traversed cw, and the border of the
 *     holes inside a c.c. are traversed ccw.  Suppose we have
 *     a list of all the borders of each c.c., both the cw and ccw
 *     traversals.  How do we reconstruct the image?
 *
 *   Reconstruction:
 *
 *     Method 1.  Topological method using connected components.
 *     We have closed borders composed of cw border pixels for the
 *     exterior of c.c. and ccw border pixels for the interior (holes)
 *     in the c.c.
 *         (a) Initialize the destination to be OFF.  Then,
 *             in any order:
 *         (b) Fill the components within and including the cw borders,
 *             and sequentially XOR them onto the destination.
 *         (c) Fill the components within but not including the ccw
 *             borders and sequentially XOR them onto the destination.
 *     The components that are XOR'd together can be generated as follows:
 *         (a) For each closed cw path, use pixFillClosedBorders():
 *               (1) Turn on the path pixels in a subimage that
 *                   minimally supports the border.
 *               (2) Do a 4-connected fill from a seed of 1 pixel width
 *                   on the border, using the inverted image in (1) as
 *                   a filling mask.
 *               (3) Invert the fill result: this gives the component
 *                   including the exterior cw path, with all holes
 *                   filled.
 *         (b) For each closed ccw path (hole):
 *               (1) Turn on the path pixels in a subimage that minimally
 *                   supports the path.
 *               (2) Find a seed pixel on the inside of this path.
 *               (3) Do a 4-connected fill from this seed pixel, using
 *                   the inverted image of the path in (1) as a filling
 *                   mask.
 *
 *     ------------------------------------------------------
 *
 *     Method 2.  A variant of Method 1.  Topological.
 *     In Method 1, we treat the exterior border differently from
 *     the interior (hole) borders.  Here, all borders in a c.c.
 *     are treated equally:
 *         (1) Start with a pix with a 1 pixel OFF boundary
 *             enclosing all the border pixels of the c.c.
 *             This is the filling mask.
 *         (2) Make a seed image of the same size as follows:  for
 *             each border, put one seed pixel OUTSIDE the border
 *             (where OUTSIDE is determined by the inside/outside
 *             convention for borders).
 *         (3) Seedfill into the seed image, filling in the regions
 *             determined by the filling mask.  The fills are clipped
 *             by the border pixels.
 *         (4) Inverting this, we get the c.c. properly filled,
 *             with the holes empty!
 *         (5) Rasterop using XOR the filled c.c. (but not the 1
 *             pixel boundary) into the full dest image.
 *
 *     Method 2 is about 1.2x faster than Method 1 on text images,
 *     and about 2x faster on complex images (e.g., with halftones).
 *
 *     ------------------------------------------------------
 *
 *     Method 3.  The traditional way to fill components delineated
 *     by boundaries is through scan line conversion.  It's a bit
 *     tricky, and I have not yet tried to implement it.
 *
 *     ------------------------------------------------------
 *
 *     Method 4.  [Nota Bene: this method probably doesn't work, and
 *     won't be implemented.  If I get a more traditional scan line
 *     conversion algorithm working, I'll erase these notes.]
 *     Render all border pixels on a destination image,
 *     which will be the final result after scan conversion.  Assign
 *     a value 1 to pixels on cw paths, 2 to pixels on ccw paths,
 *     and 3 to pixels that are on both paths.  Each of the paths
 *     is an 8-connected component.  Now scan across each raster
 *     line.  The attempt is to make rules for each scan line
 *     that are independent of neighboring scanlines.  Here are
 *     a set of rules for writing ON pixels on a destination raster image:
 *
 *         (a) The rasterizer will be in one of two states: ON and OFF.
 *         (b) Start each line in the OFF state.  In the OFF state,
 *             skip pixels until you hit a path of any type.  Turn
 *             the path pixel ON.
 *         (c) If the state is ON, each pixel you encounter will
 *             be turned on, until and including hitting a path pixel.
 *         (d) When you hit a path pixel, if the path does NOT cut
 *             through the line, so that there is not an 8-cc path
 *             pixel (of any type) both above and below, the state
 *             is unchanged (it stays either ON or OFF).
 *         (e) If the path does cut through, but with a possible change
 *             of pixel type, then we decide whether or
 *             not to toggle the state based on the values of the
 *             path pixel and the path pixels above and below:
 *               (1) if a 1 path cuts through, toggle;
 *               (1) if a 2 path cuts through, toggle;
 *               (3) if a 3 path cuts through, do not toggle;
 *               (4) if on one side a 3 touches both a 1 and a 2, use the 2
 *               (5) if a 3 has any 1 neighbors, toggle; else if it has
 *                   no 1 neighbors, do not toggle;
 *               (6) if a 2 has any neighbors that are 1 or 3,
 *                   do not toggle
 *               (7) if a 1 has neighbors 1 and x (x = 2 or 3),
 *                   toggle
 *
 *
 *     To visualize how these rules work, consider the following
 *     component with border pixels labeled according to the scheme
 *     above.  We also show the values of the interior pixels
 *     (w=OFF, b=ON), but these of course must be inferred properly
 *     from the rules above:
 *
 *                     3
 *                  3  w  3             1  1  1
 *                  1  2  1          1  b  2  b  1
 *                  1  b  1             3  w  2  1
 *                  3  b  1          1  b  2  b  1
 *               3  w  3                1  1  1
 *               3  w  3
 *            1  b  2  b  1
 *            1  2  w  2  1
 *         1  b  2  w  2  b  1
 *            1  2  w  2  1
 *               1  2  b  1
 *               1  b  1
 *                  1
 *
 *
 *     Even if this works, which is unlikely, it will certainly be
 *     slow because decisions have to be made on a pixel-by-pixel
 *     basis when encountering borders.
 *
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"
#include "pix_internal.h"
#include "ccbord_internal.h"

static const l_int32  INITIAL_PTR_ARRAYSIZE = 20;    /* n'import quoi */

    /* In ccbaGenerateSinglePath(): don't save holes
     * in c.c. with ridiculously many small holes   */
static const l_int32  NMAX_HOLES = 150;

    /*  Tables used to trace the border.
     *   - The 8 pixel positions of neighbors Q are labeled clockwise
     *     starting from the west:
     *                  1   2   3
     *                  0   P   4
     *                  7   6   5
     *     where the labels are the index offset [0, ... 7] of Q relative to P.
     *   - xpostab[] and ypostab[] give the actual x and y pixel offsets
     *     of Q relative to P, indexed by the index offset.
     *   - qpostab[pos] gives the new index offset of Q relative to P, at
     *     the time that a new P has been chosen to be in index offset
     *     position 'pos' relative to the previous P.   The relation
     *     between P and Q is always 4-connected.  */
static const l_int32   xpostab[] = {-1, -1, 0, 1, 1, 1, 0, -1};
static const l_int32   ypostab[] = {0, -1, -1, -1, 0, 1, 1, 1};
static const l_int32   qpostab[] = {6, 6, 0, 0, 2, 2, 4, 4};

    /* Static functions */
static CCBORDA *ccbaCreate(PIX *pixs, l_int32 n);
static CCBORD *ccbCreate(PIX *pixs);
static void ccbDestroy(CCBORD **pccb);
static l_ok ccbaAddCcb(CCBORDA *ccba, CCBORD  *ccb);
static l_int32 ccbaExtendArray(CCBORDA *ccba);
static l_int32 ccbaGetCount(CCBORDA *ccba);
static CCBORD *ccbaGetCcb(CCBORDA *ccba, l_int32 index);
static CCBORD *pixGetCCBorders(PIX *pixs, BOX *box);
static PTA *pixGetOuterBorderPta(PIX *pixs, BOX *box);
static l_ok pixGetHoleBorder(CCBORD *ccb, PIX *pixs, BOX *box,
                             l_int32 xs, l_int32 ys);
static l_int32 findNextBorderPixel(l_int32 w, l_int32 h, l_uint32 *data,
                                   l_int32 wpl, l_int32 px, l_int32 py,
                                   l_int32 *pqpos, l_int32 *pnpx,
                                   l_int32 *pnpy);
static void locateOutsideSeedPixel(l_int32 fpx, l_int32 fpy, l_int32 spx,
                                   l_int32 spy, l_int32 *pxs, l_int32 *pys);
static PTA *getCutPathForHole(PIX *pix, PTA *pta, BOX *boxinner, l_int32 *pdir,
                              l_int32 *plen);

#ifndef  NO_CONSOLE_IO
#define  DEBUG_PRINT   0
#endif   /* NO CONSOLE_IO */


/*---------------------------------------------------------------------*
 *                   ccba and ccb creation and destruction             *
 *---------------------------------------------------------------------*/
/*!
 * \brief    ccbaCreate()
 *
 * \param[in]    pixs    1 bpp; can be null
 * \param[in]    n       initial number of ptrs
 * \return  ccba, or NULL on error
 */
static CCBORDA *
ccbaCreate(PIX     *pixs,
           l_int32  n)
{
CCBORDA  *ccba;

    if (n <= 0)
        n = INITIAL_PTR_ARRAYSIZE;

    ccba = (CCBORDA *)LEPT_CALLOC(1, sizeof(CCBORDA));
    if (pixs) {
        ccba->pix = pixClone(pixs);
        ccba->w = pixGetWidth(pixs);
        ccba->h = pixGetHeight(pixs);
    }
    ccba->n = 0;
    ccba->nalloc = n;
    if ((ccba->ccb = (CCBORD **)LEPT_CALLOC(n, sizeof(CCBORD *))) == NULL) {
        ccbaDestroy(&ccba);
        return (CCBORDA *)ERROR_PTR("ccba ptrs not made", __func__, NULL);
    }
    return ccba;
}


/*!
 * \brief   ccbaDestroy()
 *
 * \param[in,out]   pccba     will be set to null befoe returning
 * \return  void
 */
void
ccbaDestroy(CCBORDA  **pccba)
{
l_int32   i;
CCBORDA  *ccba;

    if (pccba == NULL) {
        L_WARNING("ptr address is NULL!\n", __func__);
        return;
    }

    if ((ccba = *pccba) == NULL)
        return;

    pixDestroy(&ccba->pix);
    for (i = 0; i < ccba->n; i++)
        ccbDestroy(&ccba->ccb[i]);
    LEPT_FREE(ccba->ccb);
    LEPT_FREE(ccba);
    *pccba = NULL;
}


/*!
 * \brief   ccbCreate()
 *
 * \param[in]    pixs    [optional]; can be null
 * \return  ccb or NULL on error
 */
static CCBORD *
ccbCreate(PIX  *pixs)
{
BOXA    *boxa;
CCBORD  *ccb;
PTA     *start;
PTAA    *local;

    if (pixs && pixGetDepth(pixs) != 1)  /* pixs can be null */
        return (CCBORD *)ERROR_PTR("pixs defined and not 1bpp", __func__, NULL);

    ccb = (CCBORD *)LEPT_CALLOC(1, sizeof(CCBORD));
    ccb->refcount = 1;
    if (pixs)
        ccb->pix = pixClone(pixs);
    boxa = boxaCreate(1);
    ccb->boxa = boxa;
    start = ptaCreate(1);
    ccb->start = start;
    local = ptaaCreate(1);
    ccb->local = local;
    return ccb;
}


/*!
 * \brief   ccbDestroy()
 *
 * \param[in,out]   pccb    will be set to null before returning
 * \return  void
 */
static void
ccbDestroy(CCBORD  **pccb)
{
CCBORD  *ccb;

    if (pccb == NULL) {
        L_WARNING("ptr address is NULL!\n", __func__);
        return;
    }

    if ((ccb = *pccb) == NULL)
        return;

    if (--ccb->refcount == 0) {
        if (ccb->pix)
            pixDestroy(&ccb->pix);
        if (ccb->boxa)
            boxaDestroy(&ccb->boxa);
        if (ccb->start)
            ptaDestroy(&ccb->start);
        if (ccb->local)
            ptaaDestroy(&ccb->local);
        if (ccb->global)
            ptaaDestroy(&ccb->global);
        if (ccb->step)
            numaaDestroy(&ccb->step);
        if (ccb->splocal)
            ptaDestroy(&ccb->splocal);
        if (ccb->spglobal)
            ptaDestroy(&ccb->spglobal);
        LEPT_FREE(ccb);
        *pccb = NULL;
    }
}


/*---------------------------------------------------------------------*
 *                            ccba addition                            *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ccbaAddCcb()
 *
 * \param[in]    ccba
 * \param[in]    ccb     to be added by insertion
 * \return  0 if OK; 1 on error
 */
static l_ok
ccbaAddCcb(CCBORDA  *ccba,
           CCBORD   *ccb)
{
l_int32  n;

    if (!ccba)
        return ERROR_INT("ccba not defined", __func__, 1);
    if (!ccb)
        return ERROR_INT("ccb not defined", __func__, 1);

    n = ccbaGetCount(ccba);
    if (n >= ccba->nalloc) {
        if (ccbaExtendArray(ccba))
            return ERROR_INT("extension failed", __func__, 1);
    }
    ccba->ccb[n] = ccb;
    ccba->n++;
    return 0;
}


/*!
 * \brief   ccbaExtendArray()
 *
 * \param[in]    ccba
 * \return  0 if OK; 1 on error
 */
static l_int32
ccbaExtendArray(CCBORDA  *ccba)
{
    if (!ccba)
        return ERROR_INT("ccba not defined", __func__, 1);

    if ((ccba->ccb = (CCBORD **)reallocNew((void **)&ccba->ccb,
                                sizeof(CCBORD *) * ccba->nalloc,
                                2 * sizeof(CCBORD *) * ccba->nalloc)) == NULL)
        return ERROR_INT("new ptr array not returned", __func__, 1);

    ccba->nalloc = 2 * ccba->nalloc;
    return 0;
}



/*---------------------------------------------------------------------*
 *                            ccba accessors                           *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ccbaGetCount()
 *
 * \param[in]    ccba
 * \return  count, with 0 on error
 */
static l_int32
ccbaGetCount(CCBORDA  *ccba)
{

    if (!ccba)
        return ERROR_INT("ccba not defined", __func__, 0);

    return ccba->n;
}


/*!
 * \brief   ccbaGetCcb()
 *
 * \param[in]    ccba
 * \param[in]    index
 * \return  ccb, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This returns a clone of the ccb; it must be destroyed
 * </pre>
 */
static CCBORD *
ccbaGetCcb(CCBORDA  *ccba,
           l_int32   index)
{
CCBORD  *ccb;

    if (!ccba)
        return (CCBORD *)ERROR_PTR("ccba not defined", __func__, NULL);
    if (index < 0 || index >= ccba->n)
        return (CCBORD *)ERROR_PTR("index out of bounds", __func__, NULL);

    ccb = ccba->ccb[index];
    ccb->refcount++;
    return ccb;
}



/*---------------------------------------------------------------------*
 *                   Top-level border-finding routines                 *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixGetAllCCBorders()
 *
 * \param[in]    pixs    1 bpp
 * \return  ccborda, or NULL on error
 */
CCBORDA *
pixGetAllCCBorders(PIX  *pixs)
{
l_int32   n, i;
BOX      *box;
BOXA     *boxa;
CCBORDA  *ccba;
CCBORD   *ccb;
PIX      *pix;
PIXA     *pixa;

    if (!pixs)
        return (CCBORDA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (CCBORDA *)ERROR_PTR("pixs not binary", __func__, NULL);

    if ((boxa = pixConnComp(pixs, &pixa, 8)) == NULL)
        return (CCBORDA *)ERROR_PTR("boxa not made", __func__, NULL);
    n = boxaGetCount(boxa);

    if ((ccba = ccbaCreate(pixs, n)) == NULL) {
        boxaDestroy(&boxa);
        pixaDestroy(&pixa);
        return (CCBORDA *)ERROR_PTR("ccba not made", __func__, NULL);
    }
    for (i = 0; i < n; i++) {
        if ((pix = pixaGetPix(pixa, i, L_CLONE)) == NULL) {
            ccbaDestroy(&ccba);
            pixaDestroy(&pixa);
            boxaDestroy(&boxa);
            return (CCBORDA *)ERROR_PTR("pix not found", __func__, NULL);
        }
        if ((box = pixaGetBox(pixa, i, L_CLONE)) == NULL) {
            ccbaDestroy(&ccba);
            pixaDestroy(&pixa);
            boxaDestroy(&boxa);
            pixDestroy(&pix);
            return (CCBORDA *)ERROR_PTR("box not found", __func__, NULL);
        }
        ccb = pixGetCCBorders(pix, box);
        pixDestroy(&pix);
        boxDestroy(&box);
        if (!ccb) {
            ccbaDestroy(&ccba);
            pixaDestroy(&pixa);
            boxaDestroy(&boxa);
            return (CCBORDA *)ERROR_PTR("ccb not made", __func__, NULL);
        }
/*        ptaWriteStream(stderr, ccb->local, 1); */
        ccbaAddCcb(ccba, ccb);
    }

    boxaDestroy(&boxa);
    pixaDestroy(&pixa);
    return ccba;
}


/*!
 * \brief   pixGetCCBorders()
 *
 * \param[in]    pixs     1 bpp, one 8-connected component
 * \param[in]    box      of %pixs, in global coords
 * \return  ccbord, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) We are finding the exterior and interior borders
 *          of an 8-connected component.   This should be used
 *          on a pix that has exactly one 8-connected component.
 *      (2) Typically, pixs is a c.c. in some larger pix.  The
 *          input box gives its location in global coordinates.
 *          This box is saved, as well as the boxes for the
 *          borders of any holes within the c.c., but the latter
 *          are given in relative coords within the c.c.
 *      (3) The calculations for the exterior border are done
 *          on a pix with a 1-pixel
 *          added border, but the saved pixel coordinates
 *          are the correct (relative) ones for the input pix
 *          (without a 1-pixel border)
 *      (4) For the definition of the three tables -- xpostab[], ypostab[]
 *          and qpostab[] -- see above where they are defined.
 * </pre>
 */
static CCBORD *
pixGetCCBorders(PIX      *pixs,
                BOX      *box)
{
l_int32   allzero, i, x, xh, w, nh;
l_int32   xs, ys;   /* starting hole border pixel, relative in pixs */
l_uint32  val;
BOX      *boxt, *boxe;
BOXA     *boxa;
CCBORD   *ccb;
PIX      *pixh;  /* for hole components */
PIX      *pixt;
PIXA     *pixa;

    if (!pixs)
        return (CCBORD *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (!box)
        return (CCBORD *)ERROR_PTR("box not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (CCBORD *)ERROR_PTR("pixs not binary", __func__, NULL);

    pixZero(pixs, &allzero);
    if (allzero)
        return (CCBORD *)ERROR_PTR("pixs all 0", __func__, NULL);

    if ((ccb = ccbCreate(pixs)) == NULL)
        return (CCBORD *)ERROR_PTR("ccb not made", __func__, NULL);

        /* Get the exterior border */
    pixGetOuterBorder(ccb, pixs, box);

        /* Find the holes, if any */
    if ((pixh = pixHolesByFilling(pixs, 4)) == NULL) {
        ccbDestroy(&ccb);
        return (CCBORD *)ERROR_PTR("pixh not made", __func__, NULL);
    }
    pixZero(pixh, &allzero);
    if (allzero) {  /* no holes */
        pixDestroy(&pixh);
        return ccb;
    }

        /* Get c.c. and locations of the holes */
    if ((boxa = pixConnComp(pixh, &pixa, 4)) == NULL) {
        ccbDestroy(&ccb);
        pixDestroy(&pixh);
        return (CCBORD *)ERROR_PTR("boxa not made", __func__, NULL);
    }
    nh = boxaGetCount(boxa);
/*    lept_stderr("%d holes\n", nh); */

        /* For each hole, find an interior pixel within the hole,
         * then march to the right and stop at the first border
         * pixel.  Save the bounding box of the border, which
         * is 1 pixel bigger on each side than the bounding box
         * of the hole itself.  Note that we use a pix of the
         * c.c. of the hole itself to be sure that we start
         * with a pixel in the hole of the proper component.
         * If we did everything from the parent component, it is
         * possible to start in a different hole that is within
         * the b.b. of a larger hole.  */
    w = pixGetWidth(pixs);
    for (i = 0; i < nh; i++) {
        boxt = boxaGetBox(boxa, i, L_CLONE);
        pixt = pixaGetPix(pixa, i, L_CLONE);
        ys = boxt->y;   /* there must be a hole pixel on this raster line */
        for (x = 0; x < boxt->w; x++) {  /* look for (fg) hole pixel */
            pixGetPixel(pixt, x, 0, &val);
            if (val == 1) {
                xh = x;
                break;
            }
        }
        if (x == boxt->w) {
            L_WARNING("no hole pixel found!\n", __func__);
            continue;
        }
        for (x = xh + boxt->x; x < w; x++) {  /* look for (fg) border pixel */
            pixGetPixel(pixs, x, ys, &val);
            if (val == 1) {
                xs = x;
                break;
            }
        }
        boxe = boxCreate(boxt->x - 1, boxt->y - 1, boxt->w + 2, boxt->h + 2);
#if  DEBUG_PRINT
        boxPrintStreamInfo(stderr, box);
        boxPrintStreamInfo(stderr, boxe);
        lept_stderr("xs = %d, ys = %d\n", xs, ys);
#endif   /* DEBUG_PRINT */
        pixGetHoleBorder(ccb, pixs, boxe, xs, ys);
        boxDestroy(&boxt);
        boxDestroy(&boxe);
        pixDestroy(&pixt);
    }

    boxaDestroy(&boxa);
    pixaDestroy(&pixa);
    pixDestroy(&pixh);
    return ccb;
}


/*!
 * \brief   pixGetOuterBordersPtaa()
 *
 * \param[in]    pixs     1 bpp
 * \return  ptaa of outer borders, in global coords, or NULL on error
 */
PTAA *
pixGetOuterBordersPtaa(PIX  *pixs)
{
l_int32  i, n;
BOX     *box;
BOXA    *boxa;
PIX     *pix;
PIXA    *pixa;
PTA     *pta;
PTAA    *ptaa;

    if (!pixs)
        return (PTAA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PTAA *)ERROR_PTR("pixs not binary", __func__, NULL);

    boxa = pixConnComp(pixs, &pixa, 8);
    n = boxaGetCount(boxa);
    if (n == 0) {
        boxaDestroy(&boxa);
        pixaDestroy(&pixa);
        return (PTAA *)ERROR_PTR("pixs empty", __func__, NULL);
    }

    ptaa = ptaaCreate(n);
    for (i = 0; i < n; i++) {
        box = boxaGetBox(boxa, i, L_CLONE);
        pix = pixaGetPix(pixa, i, L_CLONE);
        pta = pixGetOuterBorderPta(pix, box);
        if (pta)
            ptaaAddPta(ptaa, pta, L_INSERT);
        boxDestroy(&box);
        pixDestroy(&pix);
    }

    pixaDestroy(&pixa);
    boxaDestroy(&boxa);
    return ptaa;
}


/*!
 * \brief   pixGetOuterBorderPta()
 *
 * \param[in]    pixs    1 bpp, one 8-connected component
 * \param[in]    box     [optional] of %pixs, in global coordinates
 * \return  pta of outer border, in global coords, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) We are finding the exterior border of a single 8-connected
 *          component.
 *      (2) If box is NULL, the outline returned is in the local coords
 *          of the input pix.  Otherwise, box is assumed to give the
 *          location of the pix in global coordinates, and the returned
 *          pta will be in those global coordinates.
 * </pre>
 */
static PTA *
pixGetOuterBorderPta(PIX  *pixs,
                     BOX  *box)
{
l_int32  allzero, x, y;
BOX     *boxt;
CCBORD  *ccb;
PTA     *ptaloc, *ptad;

    if (!pixs)
        return (PTA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PTA *)ERROR_PTR("pixs not binary", __func__, NULL);

    pixZero(pixs, &allzero);
    if (allzero)
        return (PTA *)ERROR_PTR("pixs all 0", __func__, NULL);

    if ((ccb = ccbCreate(pixs)) == NULL)
        return (PTA *)ERROR_PTR("ccb not made", __func__, NULL);
    if (!box)
        boxt = boxCreate(0, 0, pixGetWidth(pixs), pixGetHeight(pixs));
    else
        boxt = boxClone(box);

        /* Get the exterior border in local coords */
    pixGetOuterBorder(ccb, pixs, boxt);
    if ((ptaloc = ptaaGetPta(ccb->local, 0, L_CLONE)) == NULL) {
        ccbDestroy(&ccb);
        boxDestroy(&boxt);
        return (PTA *)ERROR_PTR("ptaloc not made", __func__, NULL);
    }

        /* Transform to global coordinates, if they are given */
    if (box) {
        boxGetGeometry(box, &x, &y, NULL, NULL);
        ptad = ptaTransform(ptaloc, x, y, 1.0, 1.0);
    } else {
        ptad = ptaClone(ptaloc);
    }

    ptaDestroy(&ptaloc);
    boxDestroy(&boxt);
    ccbDestroy(&ccb);
    return ptad;
}


/*---------------------------------------------------------------------*
 *                   Lower-level border-finding routines               *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixGetOuterBorder()
 *
 * \param[in]    ccb     unfilled
 * \param[in]    pixs    for the component at hand
 * \param[in]    box     for the component, in global coords
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) the border is saved in relative coordinates within
 *          the c.c. (pixs).  Because the calculation is done
 *          in pixb with added 1 pixel border, we must subtract
 *          1 from each pixel value before storing it.
 *      (2) the stopping condition is that after the first pixel is
 *          returned to, the next pixel is the second pixel.  Having
 *          these 2 pixels recur in sequence proves the path is closed,
 *          and we do not store the second pixel again.
 * </pre>
 */
l_ok
pixGetOuterBorder(CCBORD   *ccb,
                  PIX      *pixs,
                  BOX      *box)
{
l_int32    fpx, fpy, spx, spy, qpos;
l_int32    px, py, npx, npy;
l_int32    w, h, wpl;
l_uint32  *data;
PTA       *pta;
PIX       *pixb;  /* with 1 pixel border */

    if (!ccb)
        return ERROR_INT("ccb not defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);

        /* Add 1-pixel border all around, and find start pixel */
    if ((pixb = pixAddBorder(pixs, 1, 0)) == NULL)
        return ERROR_INT("pixs not made", __func__, 1);
    if (!nextOnPixelInRaster(pixb, 1, 1, &px, &py)) {
        pixDestroy(&pixb);
        return ERROR_INT("no start pixel found", __func__, 1);
    }
    qpos = 0;   /* relative to p */
    fpx = px;  /* save location of first pixel on border */
    fpy = py;

        /* Save box and start pixel in relative coords */
    boxaAddBox(ccb->boxa, box, L_COPY);
    ptaAddPt(ccb->start, px - 1, py - 1);

    pta = ptaCreate(0);
    ptaaAddPta(ccb->local, pta, L_INSERT);
    ptaAddPt(pta, px - 1, py - 1);   /* initial point */
    pixGetDimensions(pixb, &w, &h, NULL);
    data = pixGetData(pixb);
    wpl = pixGetWpl(pixb);

        /* Get the second point; if there is none, return */
    if (findNextBorderPixel(w, h, data, wpl, px, py, &qpos, &npx, &npy)) {
        pixDestroy(&pixb);
        return 0;
    }

    spx = npx;  /* save location of second pixel on border */
    spy = npy;
    ptaAddPt(pta, npx - 1, npy - 1);   /* second point */
    px = npx;
    py = npy;

    while (1) {
        findNextBorderPixel(w, h, data, wpl, px, py, &qpos, &npx, &npy);
        if (px == fpx && py == fpy && npx == spx && npy == spy)
            break;
        ptaAddPt(pta, npx - 1, npy - 1);
        px = npx;
        py = npy;
    }

    pixDestroy(&pixb);
    return 0;
}


/*!
 * \brief   pixGetHoleBorder()
 *
 * \param[in]    ccb      the exterior border is already made
 * \param[in]    pixs     for the connected component at hand
 * \param[in]    box      for the specific hole border, in relative
 *                        coordinates to the c.c.
 * \param[in]    xs, ys   first pixel on hole border, relative to c.c.
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) we trace out hole border on pixs without addition
 *          of single pixel added border to pixs
 *      (2) therefore all coordinates are relative within the c.c. (pixs)
 *      (3) same position tables and stopping condition as for
 *          exterior borders
 * </pre>
 */
static l_ok
pixGetHoleBorder(CCBORD   *ccb,
                 PIX      *pixs,
                 BOX      *box,
                 l_int32   xs,
                 l_int32   ys)
{
l_int32    fpx, fpy, spx, spy, qpos;
l_int32    px, py, npx, npy;
l_int32    w, h, wpl;
l_uint32  *data;
PTA       *pta;

    if (!ccb)
        return ERROR_INT("ccb not defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);

        /* Add border and find start pixel */
    qpos = 0;   /* orientation of Q relative to P */
    fpx = xs;  /* save location of first pixel on border */
    fpy = ys;

        /* Save box and start pixel */
    boxaAddBox(ccb->boxa, box, L_COPY);
    ptaAddPt(ccb->start, xs, ys);

    pta = ptaCreate(0);
    ptaaAddPta(ccb->local, pta, L_INSERT);
    ptaAddPt(pta, xs, ys);   /* initial pixel */

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);

        /* Get the second point; there should always be at least 4 pts
         * in a minimal hole border!  */
    if (findNextBorderPixel(w, h, data, wpl, xs, ys, &qpos, &npx, &npy))
        return ERROR_INT("isolated hole border point!", __func__, 1);

    spx = npx;  /* save location of second pixel on border */
    spy = npy;
    ptaAddPt(pta, npx, npy);   /* second pixel */
    px = npx;
    py = npy;

    while (1) {
        findNextBorderPixel(w, h, data, wpl, px, py, &qpos, &npx, &npy);
        if (px == fpx && py == fpy && npx == spx && npy == spy)
            break;
        ptaAddPt(pta, npx, npy);
        px = npx;
        py = npy;
    }

    return 0;
}


/*!
 * \brief   findNextBorderPixel()
 *
 * \param[in]       w, h
 * \param[in]       data, wpl
 * \param[in]       px, py       current P
 * \param[in,out]   pqpos        input current Q; new Q
 * \param[out]      pnpx, pnpy   new P
 * \return  0 if next pixel found; 1 otherwise
 *
 * <pre>
 * Notes:
 *      (1) qpos increases clockwise from 0 to 7, with 0 at
 *          location with Q to left of P:   Q P
 *      (2) this is a low-level function that does not check input
 *          parameters.  All calling functions should check them.
 * </pre>
 */
static l_int32
findNextBorderPixel(l_int32    w,
                    l_int32    h,
                    l_uint32  *data,
                    l_int32    wpl,
                    l_int32    px,
                    l_int32    py,
                    l_int32   *pqpos,
                    l_int32   *pnpx,
                    l_int32   *pnpy)
{
l_int32    qpos, i, pos, npx, npy, val;
l_uint32  *line;

    qpos = *pqpos;
    for (i = 1; i < 8; i++) {
        pos = (qpos + i) % 8;
        npx = px + xpostab[pos];
        npy = py + ypostab[pos];
        if (npx < 0 || npx >= w || npy < 0 || npy >= h)
            continue;
        line = data + npy * wpl;
        val = GET_DATA_BIT(line, npx);
        if (val) {
            *pnpx = npx;
            *pnpy = npy;
            *pqpos = qpostab[pos];
            return 0;
        }
    }

    return 1;
}


/*!
 * \brief   locateOutsideSeedPixel()
 *
 * \param[in]   fpx, fpy    location of first pixel
 * \param[in]   spx, spy    location of second pixel
 * \param[out]  pxs, pys    seed pixel to be returned
 *
 * <pre>
 * Notes:
 *      (1) The first and second pixels must be 8-adjacent,
 *          so |dx| <= 1 and |dy| <= 1 and both dx and dy
 *          cannot be 0.  There are 8 possible cases.
 *      (2) The seed pixel is OUTSIDE the foreground of the c.c.
 *      (3) These rules are for the situation where the INSIDE
 *          of the c.c. is on the right as you follow the border:
 *          cw for an exterior border and ccw for a hole border.
 * </pre>
 */
static void
locateOutsideSeedPixel(l_int32   fpx,
                       l_int32   fpy,
                       l_int32   spx,
                       l_int32   spy,
                       l_int32  *pxs,
                       l_int32  *pys)
{
l_int32  dx, dy;

    dx = spx - fpx;
    dy = spy - fpy;

    if (dx * dy == 1) {
        *pxs = fpx + dx;
        *pys = fpy;
    } else if (dx * dy == -1) {
        *pxs = fpx;
        *pys = fpy + dy;
    } else if (dx == 0) {
        *pxs = fpx + dy;
        *pys = fpy + dy;
    } else  /* dy == 0 */ {
        *pxs = fpx + dx;
        *pys = fpy - dx;
    }

    return;
}



/*---------------------------------------------------------------------*
 *                            Border conversions                       *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ccbaGenerateGlobalLocs()
 *
 * \param[in]    ccba     with local chain ptaa of borders computed
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This uses the pixel locs in the local ptaa, which are all
 *          relative to each c.c., to find the global pixel locations,
 *          and stores them in the global ptaa.
 * </pre>
 */
l_ok
ccbaGenerateGlobalLocs(CCBORDA  *ccba)
{
l_int32  ncc, nb, n, i, j, k, xul, yul, x, y;
CCBORD  *ccb;
PTAA    *ptaal, *ptaag;
PTA     *ptal, *ptag;

    if (!ccba)
        return ERROR_INT("ccba not defined", __func__, 1);

    ncc = ccbaGetCount(ccba);  /* number of c.c. */
    for (i = 0; i < ncc; i++) {
        ccb = ccbaGetCcb(ccba, i);

            /* Get the UL corner in global coords, (xul, yul), of the c.c. */
        boxaGetBoxGeometry(ccb->boxa, 0, &xul, &yul, NULL, NULL);

            /* Make a new global ptaa, removing any old one */
        ptaal = ccb->local;
        nb = ptaaGetCount(ptaal);   /* number of borders */
        if (ccb->global)   /* remove old one */
            ptaaDestroy(&ccb->global);
        if ((ptaag = ptaaCreate(nb)) == NULL) {
            ccbDestroy(&ccb);
            return ERROR_INT("ptaag not made", __func__, 1);
        }
        ccb->global = ptaag;  /* save new one */

            /* Iterate through the borders for this c.c. */
        for (j = 0; j < nb; j++) {
            ptal = ptaaGetPta(ptaal, j, L_CLONE);
            n = ptaGetCount(ptal);   /* number of pixels in border */
            ptag = ptaCreate(n);
            ptaaAddPta(ptaag, ptag, L_INSERT);
            for (k = 0; k < n; k++) {
                ptaGetIPt(ptal, k, &x, &y);
                ptaAddPt(ptag, x  + xul, y + yul);
            }
            ptaDestroy(&ptal);
        }
        ccbDestroy(&ccb);
    }

    return 0;
}


/*!
 * \brief   ccbaGenerateStepChains()
 *
 * \param[in]    ccba     with local chain ptaa of borders computed
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This uses the pixel locs in the local ptaa,
 *          which are all relative to each c.c., to find
 *          the step directions for successive pixels in
 *          the chain, and stores them in the step numaa.
 *      (2) To get the step direction, use
 *              1   2   3
 *              0   P   4
 *              7   6   5
 *          where P is the previous pixel at (px, py).  The step direction
 *          is the number (from 0 through 7) for each relative location
 *          of the current pixel at (cx, cy).  It is easily found by
 *          indexing into a 2-d 3x3 array (dirtab).
 * </pre>
 */
l_ok
ccbaGenerateStepChains(CCBORDA  *ccba)
{
l_int32  ncc, nb, n, i, j, k;
l_int32  px, py, cx, cy, stepdir;
l_int32  dirtab[][3] = {{1, 2, 3}, {0, -1, 4}, {7, 6, 5}};
CCBORD  *ccb;
NUMA    *na;
NUMAA   *naa;   /* step chain code; to be made */
PTA     *ptal;
PTAA    *ptaal;  /* local chain code */

    if (!ccba)
        return ERROR_INT("ccba not defined", __func__, 1);

    ncc = ccbaGetCount(ccba);  /* number of c.c. */
    for (i = 0; i < ncc; i++) {
        ccb = ccbaGetCcb(ccba, i);

            /* Make a new step numaa, removing any old one */
        ptaal = ccb->local;
        nb = ptaaGetCount(ptaal);  /* number of borders */
        if (ccb->step)  /* remove old one */
            numaaDestroy(&ccb->step);
        if ((naa = numaaCreate(nb)) == NULL) {
            ccbDestroy(&ccb);
            return ERROR_INT("naa not made", __func__, 1);
        }
        ccb->step = naa;  /* save new one */

            /* Iterate through the borders for this c.c. */
        for (j = 0; j < nb; j++) {
            ptal = ptaaGetPta(ptaal, j, L_CLONE);
            n = ptaGetCount(ptal);   /* number of pixels in border */
            if (n == 1) {  /* isolated pixel */
                na = numaCreate(1);   /* but leave it empty */
            } else {   /* trace out the boundary */
                na = numaCreate(n);
                ptaGetIPt(ptal, 0, &px, &py);
                for (k = 1; k < n; k++) {
                    ptaGetIPt(ptal, k, &cx, &cy);
                    stepdir = dirtab[1 + cy - py][1 + cx - px];
                    numaAddNumber(na, stepdir);
                    px = cx;
                    py = cy;
                }
            }
            numaaAddNuma(naa, na, L_INSERT);
            ptaDestroy(&ptal);
        }
        ccbDestroy(&ccb);  /* just decrement refcount */
    }

    return 0;
}


/*!
 * \brief   ccbaStepChainsToPixCoords()
 *
 * \param[in]    ccba        with step chains numaa of borders
 * \param[in]    coordtype   CCB_GLOBAL_COORDS or CCB_LOCAL_COORDS
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This uses the step chain data in each ccb to determine
 *          the pixel locations, either global or local,
 *          and stores them in the appropriate ptaa,
 *          either global or local.  For the latter, the
 *          pixel locations are relative to the c.c.
 * </pre>
 */
l_ok
ccbaStepChainsToPixCoords(CCBORDA  *ccba,
                          l_int32   coordtype)
{
l_int32  ncc, nb, n, i, j, k;
l_int32  xul, yul, xstart, ystart, x, y, stepdir;
BOXA    *boxa;
CCBORD  *ccb;
NUMA    *na;
NUMAA   *naa;
PTAA    *ptaan;  /* new pix coord ptaa */
PTA     *ptas, *ptan;

    if (!ccba)
        return ERROR_INT("ccba not defined", __func__, 1);
    if (coordtype != CCB_GLOBAL_COORDS && coordtype != CCB_LOCAL_COORDS)
        return ERROR_INT("coordtype not valid", __func__, 1);

    ncc = ccbaGetCount(ccba);  /* number of c.c. */
    for (i = 0; i < ncc; i++) {
        ccb = ccbaGetCcb(ccba, i);
        if ((naa = ccb->step) == NULL) {
            ccbDestroy(&ccb);
            return ERROR_INT("step numaa not found", __func__, 1);
        } if ((boxa = ccb->boxa) == NULL) {
            ccbDestroy(&ccb);
            return ERROR_INT("boxa not found", __func__, 1);
        } if ((ptas = ccb->start) == NULL) {
            ccbDestroy(&ccb);
            return ERROR_INT("start pta not found", __func__, 1);
        }

            /* For global coords, get the (xul, yul) of the c.c.;
             * otherwise, use relative coords. */
        if (coordtype == CCB_LOCAL_COORDS) {
            xul = 0;
            yul = 0;
        } else {  /* coordtype == CCB_GLOBAL_COORDS */
                /* Get UL corner in global coords */
            if (boxaGetBoxGeometry(boxa, 0, &xul, &yul, NULL, NULL)) {
                ccbDestroy(&ccb);
                return ERROR_INT("bounding rectangle not found", __func__, 1);
            }
        }

            /* Make a new ptaa, removing any old one */
        nb = numaaGetCount(naa);   /* number of borders */
        if ((ptaan = ptaaCreate(nb)) == NULL) {
            ccbDestroy(&ccb);
            return ERROR_INT("ptaan not made", __func__, 1);
        }
        if (coordtype == CCB_LOCAL_COORDS) {
            if (ccb->local)   /* remove old one */
                ptaaDestroy(&ccb->local);
            ccb->local = ptaan;  /* save new local chain */
        } else {   /* coordtype == CCB_GLOBAL_COORDS */
            if (ccb->global)   /* remove old one */
                ptaaDestroy(&ccb->global);
            ccb->global = ptaan;  /* save new global chain */
        }

            /* Iterate through the borders for this c.c. */
        for (j = 0; j < nb; j++) {
            na = numaaGetNuma(naa, j, L_CLONE);
            n = numaGetCount(na);   /* number of steps in border */
            if ((ptan = ptaCreate(n + 1)) == NULL) {
                ccbDestroy(&ccb);
                numaDestroy(&na);
                return ERROR_INT("ptan not made", __func__, 1);
            }
            ptaaAddPta(ptaan, ptan, L_INSERT);
            ptaGetIPt(ptas, j, &xstart, &ystart);
            x = xul + xstart;
            y = yul + ystart;
            ptaAddPt(ptan, x, y);
            for (k = 0; k < n; k++) {
                numaGetIValue(na, k, &stepdir);
                x += xpostab[stepdir];
                y += ypostab[stepdir];
                ptaAddPt(ptan, x, y);
            }
            numaDestroy(&na);
        }
        ccbDestroy(&ccb);
    }

    return 0;
}


/*!
 * \brief   ccbaGenerateSPGlobalLocs()
 *
 * \param[in]    ccba
 * \param[in]    ptsflag      CCB_SAVE_ALL_PTS or CCB_SAVE_TURNING_PTS
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This calculates the splocal rep if not yet made.
 *      (2) It uses the local pixel values in splocal, the single
 *          path pta, which are all relative to each c.c., to find
 *          the corresponding global pixel locations, and stores
 *          them in the spglobal pta.
 *      (3) This lists only the turning points: it both makes a
 *          valid svg file and is typically about half the size
 *          when all border points are listed.
 * </pre>
 */
l_ok
ccbaGenerateSPGlobalLocs(CCBORDA  *ccba,
                         l_int32   ptsflag)
{
l_int32  ncc, npt, i, j, xul, yul, x, y, delx, dely;
l_int32  xp, yp, delxp, delyp;   /* prev point and increments */
CCBORD  *ccb;
PTA     *ptal, *ptag;

    if (!ccba)
        return ERROR_INT("ccba not defined", __func__, 1);

        /* Make sure we have a local single path representation */
    if ((ccb = ccbaGetCcb(ccba, 0)) == NULL)
        return ERROR_INT("no ccb", __func__, 1);
    if (!ccb->splocal)
        ccbaGenerateSinglePath(ccba);
    ccbDestroy(&ccb);  /* clone ref */

    ncc = ccbaGetCount(ccba);  /* number of c.c. */
    for (i = 0; i < ncc; i++) {
        ccb = ccbaGetCcb(ccba, i);

            /* Get the UL corner in global coords, (xul, yul), of the c.c. */
        if (boxaGetBoxGeometry(ccb->boxa, 0, &xul, &yul, NULL, NULL)) {
            ccbDestroy(&ccb);
            return ERROR_INT("bounding rectangle not found", __func__, 1);
        }

            /* Make a new spglobal pta, removing any old one */
        ptal = ccb->splocal;
        npt = ptaGetCount(ptal);   /* number of points */
        if (ccb->spglobal)   /* remove old one */
            ptaDestroy(&ccb->spglobal);
        if ((ptag = ptaCreate(npt)) == NULL) {
            ccbDestroy(&ccb);
            return ERROR_INT("ptag not made", __func__, 1);
        }
        ccb->spglobal = ptag;  /* save new one */

            /* Convert local to global */
        if (ptsflag == CCB_SAVE_ALL_PTS) {
            for (j = 0; j < npt; j++) {
                ptaGetIPt(ptal, j, &x, &y);
                ptaAddPt(ptag, x  + xul, y + yul);
            }
        } else {   /* ptsflag = CCB_SAVE_TURNING_PTS */
            ptaGetIPt(ptal, 0, &xp, &yp);   /* get the 1st pt */
            ptaAddPt(ptag, xp  + xul, yp + yul);   /* save the 1st pt */
            if (npt == 2) {  /* get and save the 2nd pt  */
                ptaGetIPt(ptal, 1, &x, &y);
                ptaAddPt(ptag, x  + xul, y + yul);
            } else if (npt > 2)  {
                ptaGetIPt(ptal, 1, &x, &y);
                delxp = x - xp;
                delyp = y - yp;
                xp = x;
                yp = y;
                for (j = 2; j < npt; j++) {
                    ptaGetIPt(ptal, j, &x, &y);
                    delx = x - xp;
                    dely = y - yp;
                    if (delx != delxp || dely != delyp)
                        ptaAddPt(ptag, xp  + xul, yp + yul);
                    xp = x;
                    yp = y;
                    delxp = delx;
                    delyp = dely;
                }
                ptaAddPt(ptag, xp  + xul, yp + yul);
            }
        }

        ccbDestroy(&ccb);  /* clone ref */
    }

    return 0;
}



/*---------------------------------------------------------------------*
 *                       Conversion to single path                     *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ccbaGenerateSinglePath()
 *
 * \param[in]    ccba
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Generates a single border in local pixel coordinates.
 *          For each c.c., if there is just an outer border, copy it.
 *          If there are also hole borders, for each hole border,
 *          determine the smallest horizontal or vertical
 *          distance from the border to the outside of the c.c.,
 *          and find a path through the c.c. for this cut.
 *          We do this in a way that guarantees a pixel from the
 *          hole border is the starting point of the path, and
 *          we must verify that the path intersects the outer
 *          border (if it intersects it, then it ends on it).
 *          One can imagine pathological cases, but they may not
 *          occur in images of text characters and un-textured
 *          line graphics.
 *      (2) Once it is verified that the path through the c.c.
 *          intersects both the hole and outer borders, we
 *          generate the full single path for all borders in the
 *          c.c.  Starting at the start point on the outer
 *          border, when we hit a line on a cut, we take
 *          the cut, do the hole border, and return on the cut
 *          to the outer border.  We compose a pta of the
 *          outer border pts that are on cut paths, and for
 *          every point on the outer border (as we go around),
 *          we check against this pta.  When we find a matching
 *          point in the pta, we do its cut path and hole border.
 *          The single path is saved in the ccb.
 * </pre>
 */
l_ok
ccbaGenerateSinglePath(CCBORDA  *ccba)
{
l_int32   i, j, k, ncc, nb, ncut, npt, dir, len, state, lostholes;
l_int32   x, y, xl, yl, xf, yf;
BOX      *boxinner;
BOXA     *boxa;
CCBORD   *ccb;
PTA      *pta, *ptac, *ptah;
PTA      *ptahc;  /* cyclic permutation of hole border, with end pts at cut */
PTA      *ptas;  /* output result: new single path for c.c. */
PTA      *ptaf;  /* points on the hole borders that intersect with cuts */
PTA      *ptal;  /* points on outer border that intersect with cuts */
PTA      *ptap, *ptarp;   /* path and reverse path between borders */
PTAA     *ptaa;
PTAA     *ptaap;  /* ptaa for all paths between borders */

    if (!ccba)
        return ERROR_INT("ccba not defined", __func__, 1);

    ncc = ccbaGetCount(ccba);   /* number of c.c. */
    lostholes = 0;
    for (i = 0; i < ncc; i++) {
        ccb = ccbaGetCcb(ccba, i);
        if ((ptaa = ccb->local) == NULL) {
            L_WARNING("local pixel loc array not found\n", __func__);
            continue;
        }
        nb = ptaaGetCount(ptaa);   /* number of borders in the c.c.  */

            /* Prepare the output pta */
        if (ccb->splocal)
            ptaDestroy(&ccb->splocal);
        ptas = ptaCreate(0);
        ccb->splocal = ptas;

            /* If no holes, just concat the outer border */
        pta = ptaaGetPta(ptaa, 0, L_CLONE);
        if (nb == 1 || nb > NMAX_HOLES + 1) {
            ptaJoin(ptas, pta, 0, -1);
            ptaDestroy(&pta);  /* remove clone */
            ccbDestroy(&ccb);  /* remove clone */
            continue;
        }

            /* Find the (nb - 1) cut paths that connect holes
             * with outer border */
        boxa = ccb->boxa;
        ptaap = ptaaCreate(nb - 1);
        ptaf = ptaCreate(nb - 1);
        ptal = ptaCreate(nb - 1);
        for (j = 1; j < nb; j++) {
            boxinner = boxaGetBox(boxa, j, L_CLONE);

                /* Find a short path and store it */
            ptac = getCutPathForHole(ccb->pix, pta, boxinner, &dir, &len);
            if (len == 0) {  /* lost the hole */
                lostholes++;
/*                boxPrintStreamInfo(stderr, boxa->box[0]); */
            }
            ptaaAddPta(ptaap, ptac, L_INSERT);
/*            lept_stderr("dir = %d, length = %d\n", dir, len); */
/*            ptaWriteStream(stderr, ptac, 1); */

                /* Store the first and last points in the cut path,
                 * which must be on a hole border and the outer
                 * border, respectively */
            ncut = ptaGetCount(ptac);
            if (ncut == 0) {   /* missed hole; neg coords won't match */
                ptaAddPt(ptaf, -1, -1);
                ptaAddPt(ptal, -1, -1);
            } else {
                ptaGetIPt(ptac, 0, &x, &y);
                ptaAddPt(ptaf, x, y);
                ptaGetIPt(ptac, ncut - 1, &x, &y);
                ptaAddPt(ptal, x, y);
            }
            boxDestroy(&boxinner);
        }

            /* Make a single path for the c.c. using these connections */
        npt = ptaGetCount(pta);  /* outer border pts */
        for (k = 0; k < npt; k++) {
            ptaGetIPt(pta, k, &x, &y);
            if (k == 0) {   /* if there is a cut at the first point,
                             * we can wait until the end to take it */
                ptaAddPt(ptas, x, y);
                continue;
            }
            state = L_NOT_FOUND;
            for (j = 0; j < nb - 1; j++) {  /* iterate over cut end pts */
                ptaGetIPt(ptal, j, &xl, &yl);  /* cut point on outer border */
                if (x == xl && y == yl) {  /* take this cut to the hole */
                    state = L_FOUND;
                    ptap = ptaaGetPta(ptaap, j, L_CLONE);
                    ptarp = ptaReverse(ptap, 1);
                        /* Cut point on hole border: */
                    ptaGetIPt(ptaf, j, &xf, &yf);
                        /* Hole border: */
                    ptah = ptaaGetPta(ptaa, j + 1, L_CLONE);
                    ptahc = ptaCyclicPerm(ptah, xf, yf);
/*                    ptaWriteStream(stderr, ptahc, 1); */
                    ptaJoin(ptas, ptarp, 0, -1);
                    ptaJoin(ptas, ptahc, 0, -1);
                    ptaJoin(ptas, ptap, 0, -1);
                    ptaDestroy(&ptap);
                    ptaDestroy(&ptarp);
                    ptaDestroy(&ptah);
                    ptaDestroy(&ptahc);
                    break;
                }
            }
            if (state == L_NOT_FOUND)
                ptaAddPt(ptas, x, y);
        }

/*        ptaWriteStream(stderr, ptas, 1); */
        ptaaDestroy(&ptaap);
        ptaDestroy(&ptaf);
        ptaDestroy(&ptal);
        ptaDestroy(&pta);  /* remove clone */
        ccbDestroy(&ccb);  /* remove clone */
    }

    if (lostholes > 0)
        L_INFO("***** %d lost holes *****\n", __func__, lostholes);
    return 0;
}


/*!
 * \brief   getCutPathForHole()
 *
 * \param[in]    pix        1 bpp, of c.c.
 * \param[in]    pta        of outer border
 * \param[in]    boxinner   bounding box of hole path
 * \param[out]   pdir       direction (0-3), returned; only needed for debug
 * \param[out]   plen       length of path, returned
 * \return  pta of pts on cut path from the hole border
 *              to the outer border, including end points on
 *              both borders; or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If we don't find a path, we return a pta with no pts
 *          in it and len = 0.
 *      (2) The goal is to get a reasonably short path between the
 *          inner and outer borders, that goes entirely within the fg of
 *          the pix.  This function is cheap-and-dirty, may fail for some
 *          holes in complex topologies such as those you might find in a
 *          moderately dark scanned halftone.  If it fails to find a
 *          path to any particular hole, the hole will not be rendered.
 *          Nevertheless, the image can be perfectly reconstructed
 *          from the boundary representation.
 * </pre>
 */
static PTA *
getCutPathForHole(PIX      *pix,
                  PTA      *pta,
                  BOX      *boxinner,
                  l_int32  *pdir,
                  l_int32  *plen)
{
l_int32   w, h, nc, x, y, xl, yl, xmid, ymid;
l_uint32  val;
PTA      *ptac;

    if (!pix)
        return (PTA *)ERROR_PTR("pix not defined", __func__, NULL);
    if (!pta)
        return (PTA *)ERROR_PTR("pta not defined", __func__, NULL);
    if (!boxinner)
        return (PTA *)ERROR_PTR("boxinner not defined", __func__, NULL);

    pixGetDimensions(pix, &w, &h, NULL);
    ptac = ptaCreate(4);
    xmid = boxinner->x + boxinner->w / 2;
    ymid = boxinner->y + boxinner->h / 2;

        /* try top first */
    for (y = ymid; y >= 0; y--) {
        pixGetPixel(pix, xmid, y, &val);
        if (val == 1) {
            ptaAddPt(ptac, xmid, y);
            break;
        }
    }
    for (y = y - 1; y >= 0; y--) {
        pixGetPixel(pix, xmid, y, &val);
        if (val == 1)
            ptaAddPt(ptac, xmid, y);
        else
            break;
    }
    nc = ptaGetCount(ptac);
    ptaGetIPt(ptac, nc - 1, &xl, &yl);
    if (ptaContainsPt(pta, xl, yl)) {
        *pdir = 1;
        *plen = nc;
        return ptac;
    }

        /* Next try bottom */
    ptaEmpty(ptac);
    for (y = ymid; y < h; y++) {
        pixGetPixel(pix, xmid, y, &val);
        if (val == 1) {
            ptaAddPt(ptac, xmid, y);
            break;
        }
    }
    for (y = y + 1; y < h; y++) {
        pixGetPixel(pix, xmid, y, &val);
        if (val == 1)
            ptaAddPt(ptac, xmid, y);
        else
            break;
    }
    nc = ptaGetCount(ptac);
    ptaGetIPt(ptac, nc - 1, &xl, &yl);
    if (ptaContainsPt(pta, xl, yl)) {
        *pdir = 3;
        *plen = nc;
        return ptac;
    }

        /* Next try left */
    ptaEmpty(ptac);
    for (x = xmid; x >= 0; x--) {
        pixGetPixel(pix, x, ymid, &val);
        if (val == 1) {
            ptaAddPt(ptac, x, ymid);
            break;
        }
    }
    for (x = x - 1; x >= 0; x--) {
        pixGetPixel(pix, x, ymid, &val);
        if (val == 1)
            ptaAddPt(ptac, x, ymid);
        else
            break;
    }
    nc = ptaGetCount(ptac);
    ptaGetIPt(ptac, nc - 1, &xl, &yl);
    if (ptaContainsPt(pta, xl, yl)) {
        *pdir = 0;
        *plen = nc;
        return ptac;
    }

        /* Finally try right */
    ptaEmpty(ptac);
    for (x = xmid; x < w; x++) {
        pixGetPixel(pix, x, ymid, &val);
        if (val == 1) {
            ptaAddPt(ptac, x, ymid);
            break;
        }
    }
    for (x = x + 1; x < w; x++) {
        pixGetPixel(pix, x, ymid, &val);
        if (val == 1)
            ptaAddPt(ptac, x, ymid);
        else
            break;
    }
    nc = ptaGetCount(ptac);
    ptaGetIPt(ptac, nc - 1, &xl, &yl);
    if (ptaContainsPt(pta, xl, yl)) {
        *pdir = 2;
        *plen = nc;
        return ptac;
    }

        /* Sometimes, there is nothing. */
    ptaEmpty(ptac);
    *plen = 0;
    return ptac;
}



/*---------------------------------------------------------------------*
 *                            Border rendering                         *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ccbaDisplayBorder()
 *
 * \param[in]    ccba
 * \return  pix of border pixels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Uses global ptaa, which gives each border pixel in
 *          global coordinates, and must be computed in advance
 *          by calling ccbaGenerateGlobalLocs().
 * </pre>
 */
PIX *
ccbaDisplayBorder(CCBORDA  *ccba)
{
l_int32  ncc, nb, n, i, j, k, x, y;
CCBORD  *ccb;
PIX     *pixd;
PTAA    *ptaa;
PTA     *pta;

    if (!ccba)
        return (PIX *)ERROR_PTR("ccba not defined", __func__, NULL);

    if ((pixd = pixCreate(ccba->w, ccba->h, 1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    ncc = ccbaGetCount(ccba);   /* number of c.c. */
    for (i = 0; i < ncc; i++) {
        ccb = ccbaGetCcb(ccba, i);
        if ((ptaa = ccb->global) == NULL) {
            L_WARNING("global pixel loc array not found", __func__);
            ccbDestroy(&ccb);
            continue;
        }
        nb = ptaaGetCount(ptaa);   /* number of borders in the c.c.  */
        for (j = 0; j < nb; j++) {
            pta = ptaaGetPta(ptaa, j, L_CLONE);
            n = ptaGetCount(pta);   /* number of pixels in the border */
            for (k = 0; k < n; k++) {
                ptaGetIPt(pta, k, &x, &y);
                pixSetPixel(pixd, x, y, 1);
            }
            ptaDestroy(&pta);
        }
        ccbDestroy(&ccb);
    }

    return pixd;
}


/*!
 * \brief   ccbaDisplaySPBorder()
 *
 * \param[in]    ccba
 * \return  pix of border pixels, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Uses spglobal pta, which gives each border pixel in
 *          global coordinates, one path per c.c., and must
 *          be computed in advance by calling ccbaGenerateSPGlobalLocs().
 * </pre>
 */
PIX *
ccbaDisplaySPBorder(CCBORDA  *ccba)
{
l_int32  ncc, npt, i, j, x, y;
CCBORD  *ccb;
PIX     *pixd;
PTA     *ptag;

    if (!ccba)
        return (PIX *)ERROR_PTR("ccba not defined", __func__, NULL);

    if ((pixd = pixCreate(ccba->w, ccba->h, 1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    ncc = ccbaGetCount(ccba);   /* number of c.c. */
    for (i = 0; i < ncc; i++) {
        ccb = ccbaGetCcb(ccba, i);
        if ((ptag = ccb->spglobal) == NULL) {
            L_WARNING("spglobal pixel loc array not found\n", __func__);
            ccbDestroy(&ccb);
            continue;
        }
        npt = ptaGetCount(ptag);   /* number of pixels on path */
        for (j = 0; j < npt; j++) {
            ptaGetIPt(ptag, j, &x, &y);
            pixSetPixel(pixd, x, y, 1);
        }
        ccbDestroy(&ccb);  /* clone ref */
    }

    return pixd;
}


/*!
 * \brief   ccbaDisplayImage1()
 *
 * \param[in]    ccba
 * \return  pix of image, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Uses local ptaa, which gives each border pixel in
 *          local coordinates, so the actual pixel positions must
 *          be computed using all offsets.
 *      (2) For the holes, use coordinates relative to the c.c.
 *      (3) This is slower than Method 2.
 *      (4) This uses topological properties (Method 1) to do scan
 *          conversion to raster
 *
 *  This algorithm deserves some commentary.
 *
 *  I first tried the following:
 *    ~ outer borders: 4-fill from outside, stopping at the
 *         border, using pixFillClosedBorders()
 *    ~ inner borders: 4-fill from outside, stopping again
 *         at the border, XOR with the border, and invert
 *         to get the hole.  This did not work, because if
 *         you have a hole border that looks like:
 *
 *                x x x x x x
 *                x          x
 *                x   x x x   x
 *                  x x o x   x
 *                      x     x
 *                      x     x
 *                        x x x
 *
 *         if you 4-fill from the outside, the pixel 'o' will
 *         not be filled!  XORing with the border leaves it OFF.
 *         Inverting then gives a single bad ON pixel that is not
 *         actually part of the hole.
 *
 *  So what you must do instead is 4-fill the holes from inside.
 *  You can do this from a seedfill, using a pix with the hole
 *  border as the filling mask.  But you need to start with a
 *  pixel inside the hole.  How is this determined?  The best
 *  way is from the contour.  We have a right-hand shoulder
 *  rule for inside (i.e., the filled region).   Take the
 *  first 2 pixels of the hole border, and compute dx and dy
 *  (second coord minus first coord:  dx = sx - fx, dy = sy - fy).
 *  There are 8 possibilities, depending on the values of dx and
 *  dy (which can each be -1, 0, and +1, but not both 0).
 *  These 8 cases can be broken into 4; see the simple algorithm below.
 *  Once you have an interior seed pixel, you fill from the seed,
 *  clipping with the hole border pix by filling into its invert.
 *
 *  You then successively XOR these interior filled components, in any order.
 * </pre>
 */
PIX *
ccbaDisplayImage1(CCBORDA  *ccba)
{
l_int32  ncc, i, nb, n, j, k, x, y, xul, yul, xoff, yoff, w, h;
l_int32  fpx, fpy, spx, spy, xs, ys;
BOX     *box;
BOXA    *boxa;
CCBORD  *ccb;
PIX     *pixd, *pixt, *pixh;
PTAA    *ptaa;
PTA     *pta;

    if (!ccba)
        return (PIX *)ERROR_PTR("ccba not defined", __func__, NULL);

    if ((pixd = pixCreate(ccba->w, ccba->h, 1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    ncc = ccbaGetCount(ccba);
    for (i = 0; i < ncc; i++) {
        ccb = ccbaGetCcb(ccba, i);
        if ((boxa = ccb->boxa) == NULL) {
            pixDestroy(&pixd);
            ccbDestroy(&ccb);
            return (PIX *)ERROR_PTR("boxa not found", __func__, NULL);
        }

            /* Render border in pixt */
        if ((ptaa = ccb->local) == NULL) {
            L_WARNING("local chain array not found\n", __func__);
            ccbDestroy(&ccb);
            continue;
        }

        nb = ptaaGetCount(ptaa);   /* number of borders in the c.c.  */
        for (j = 0; j < nb; j++) {
            if ((box = boxaGetBox(boxa, j, L_CLONE)) == NULL) {
                pixDestroy(&pixd);
                ccbDestroy(&ccb);
                return (PIX *)ERROR_PTR("b. box not found", __func__, NULL);
            }
            if (j == 0) {
                boxGetGeometry(box, &xul, &yul, &w, &h);
                xoff = yoff = 0;
            } else {
                boxGetGeometry(box, &xoff, &yoff, &w, &h);
            }
            boxDestroy(&box);

                /* Render the border in a minimum-sized pix;
                 * subtract xoff and yoff because the pixel
                 * location is stored relative to the c.c., but
                 * we need it relative to just the hole border. */
            if ((pixt = pixCreate(w, h, 1)) == NULL) {
                pixDestroy(&pixd);
                ccbDestroy(&ccb);
                return (PIX *)ERROR_PTR("pixt not made", __func__, NULL);
            }
            pta = ptaaGetPta(ptaa, j, L_CLONE);
            n = ptaGetCount(pta);   /* number of pixels in the border */
            for (k = 0; k < n; k++) {
                ptaGetIPt(pta, k, &x, &y);
                pixSetPixel(pixt, x - xoff, y - yoff, 1);
                if (j > 0) {   /* need this for finding hole border pixel */
                    if (k == 0) {
                        fpx = x - xoff;
                        fpy = y - yoff;
                    }
                    if (k == 1) {
                        spx = x - xoff;
                        spy = y - yoff;
                    }
                }
            }
            ptaDestroy(&pta);

                /* Get the filled component */
            if (j == 0) {  /* if outer border, fill from outer boundary */
                if ((pixh = pixFillClosedBorders(pixt, 4)) == NULL) {
                    pixDestroy(&pixd);
                    pixDestroy(&pixt);
                    ccbDestroy(&ccb);
                    return (PIX *)ERROR_PTR("pixh not made", __func__, NULL);
                }
            } else {   /* fill the hole from inside */
                    /* get the location of a seed pixel in the hole */
                locateOutsideSeedPixel(fpx, fpy, spx, spy, &xs, &ys);

                    /* Put seed in hole and fill interior of hole,
                     * using pixt as clipping mask */
                pixh = pixCreateTemplate(pixt);
                pixSetPixel(pixh, xs, ys, 1);  /* put seed pixel in hole */
                pixInvert(pixt, pixt);  /* to make filling mask */
                pixSeedfillBinary(pixh, pixh, pixt, 4);  /* 4-fill hole */
            }

                /* XOR into the dest */
            pixRasterop(pixd, xul + xoff, yul + yoff, w, h, PIX_XOR,
                        pixh, 0, 0);
            pixDestroy(&pixt);
            pixDestroy(&pixh);
        }
        ccbDestroy(&ccb);
    }
    return pixd;
}



/*!
 * \brief   ccbaDisplayImage2()
 *
 * \param[in]   ccba
 * \return  pix of image, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Uses local chain ptaa, which gives each border pixel in
 *          local coordinates, so the actual pixel positions must
 *          be computed using all offsets.
 *      (2) Treats exterior and hole borders on equivalent
 *          footing, and does all calculations on a pix
 *          that spans the c.c. with a 1 pixel added boundary.
 *      (3) This uses topological properties (Method 2) to do scan
 *          conversion to raster
 *      (4) The algorithm is described at the top of this file (Method 2).
 *          It is preferred to Method 1 because it is between 1.2x and 2x
 *          faster than Method 1.
 * </pre>
 */
PIX *
ccbaDisplayImage2(CCBORDA  *ccba)
{
l_int32  ncc, nb, n, i, j, k, x, y, xul, yul, w, h;
l_int32  fpx, fpy, spx, spy, xs, ys;
BOXA    *boxa;
CCBORD  *ccb;
PIX     *pixd, *pixc, *pixs;
PTAA    *ptaa;
PTA     *pta;

    if (!ccba)
        return (PIX *)ERROR_PTR("ccba not defined", __func__, NULL);

    if ((pixd = pixCreate(ccba->w, ccba->h, 1)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    ncc = ccbaGetCount(ccba);
    for (i = 0; i < ncc; i++) {
            /* Generate clipping mask from border pixels and seed image
             * from one seed for each closed border. */
        ccb = ccbaGetCcb(ccba, i);
        if ((boxa = ccb->boxa) == NULL) {
            pixDestroy(&pixd);
            ccbDestroy(&ccb);
            return (PIX *)ERROR_PTR("boxa not found", __func__, NULL);
        }
        if (boxaGetBoxGeometry(boxa, 0, &xul, &yul, &w, &h)) {
            pixDestroy(&pixd);
            ccbDestroy(&ccb);
            return (PIX *)ERROR_PTR("b. box not found", __func__, NULL);
        }
        pixc = pixCreate(w + 2, h + 2, 1);
        pixs = pixCreateTemplate(pixc);

        if ((ptaa = ccb->local) == NULL) {
            pixDestroy(&pixc);
            pixDestroy(&pixs);
            ccbDestroy(&ccb);
            L_WARNING("local chain array not found\n", __func__);
            continue;
        }
        nb = ptaaGetCount(ptaa);   /* number of borders in the c.c.  */
        for (j = 0; j < nb; j++) {
            pta = ptaaGetPta(ptaa, j, L_CLONE);
            n = ptaGetCount(pta);   /* number of pixels in the border */

                /* Render border pixels in pixc */
            for (k = 0; k < n; k++) {
                ptaGetIPt(pta, k, &x, &y);
                pixSetPixel(pixc, x + 1, y + 1, 1);
                if (k == 0) {
                    fpx = x + 1;
                    fpy = y + 1;
                } else if (k == 1) {
                    spx = x + 1;
                    spy = y + 1;
                }
            }

                /* Get and set seed pixel for this border in pixs */
            if (n > 1)
                locateOutsideSeedPixel(fpx, fpy, spx, spy, &xs, &ys);
            else  /* isolated c.c. */
                xs = ys = 0;
            pixSetPixel(pixs, xs, ys, 1);
            ptaDestroy(&pta);
        }

            /* Fill from seeds in pixs, using pixc as the clipping mask,
             * to reconstruct the c.c. */
        pixInvert(pixc, pixc);  /* to convert clipping -> filling mask */
        pixSeedfillBinary(pixs, pixs, pixc, 4);  /* 4-fill */
        pixInvert(pixs, pixs);  /* to make the c.c. */

            /* XOR into the dest */
        pixRasterop(pixd, xul, yul, w, h, PIX_XOR, pixs, 1, 1);

        pixDestroy(&pixc);
        pixDestroy(&pixs);
        ccbDestroy(&ccb);  /* ref-counted */
    }
    return pixd;
}


/*---------------------------------------------------------------------*
 *                            Serialize for I/O                        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ccbaWrite()
 *
 * \param[in]    filename
 * \param[in]    ccba
 * \return  0 if OK, 1 on error
 */
l_ok
ccbaWrite(const char  *filename,
          CCBORDA     *ccba)
{
FILE  *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!ccba)
        return ERROR_INT("ccba not defined", __func__, 1);

    if ((fp = fopenWriteStream(filename, "wb+")) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    if (ccbaWriteStream(fp, ccba)) {
        fclose(fp);
        return ERROR_INT_1("ccba not written to stream", filename, __func__, 1);
    }

    fclose(fp);
    return 0;
}



/*!
 * \brief   ccbaWriteStream()
 *
 * \param[in]    fp       file stream
 * \param[in]    ccba
 * \return  0 if OK; 1 on error
 *
 *  Format:
 * \code
 *           ccba: %7d cc\n num. c.c.) (ascii)   (18B
 *           pix width 4B
 *           pix height 4B
 *           [for i = 1, ncc]
 *               ulx  4B
 *               uly  4B
 *               w    4B       -- not req'd for reconstruction
 *               h    4B       -- not req'd for reconstruction
 *               number of borders 4B
 *               [for j = 1, nb]
 *                   startx  4B
 *                   starty  4B
 *                   [for k = 1, nb]
 *                        2 steps 1B
 *                   end in z8 or 88  1B
 * \endcode
 */
l_ok
ccbaWriteStream(FILE     *fp,
                CCBORDA  *ccba)
{
#if  HAVE_LIBZ  /* defined in environ.h */
char        strbuf[256];
l_uint8     bval;
l_uint8    *datain, *dataout;
l_int32     i, j, k, bx, by, bw, bh, val, startx, starty;
l_int32     ncc, nb, n;
l_uint32    w, h;
size_t      inbytes, outbytes;
L_BBUFFER  *bbuf;
CCBORD     *ccb;
NUMA       *na;
NUMAA      *naa;
PTA        *pta;
#endif

#if  !HAVE_LIBZ  /* defined in environ.h */
    return ERROR_INT("no libz: can't write data", __func__, 1);
#else

    if (!fp)
        return ERROR_INT("stream not open", __func__, 1);
    if (!ccba)
        return ERROR_INT("ccba not defined", __func__, 1);

    if ((bbuf = bbufferCreate(NULL, 1000)) == NULL)
        return ERROR_INT("bbuf not made", __func__, 1);

    ncc = ccbaGetCount(ccba);
    snprintf(strbuf, sizeof(strbuf), "ccba: %7d cc\n", ncc);
    bbufferRead(bbuf, (l_uint8 *)strbuf, 18);
    w = pixGetWidth(ccba->pix);
    h = pixGetHeight(ccba->pix);
    bbufferRead(bbuf, (l_uint8 *)&w, 4);  /* width */
    bbufferRead(bbuf, (l_uint8 *)&h, 4);  /* height */
    for (i = 0; i < ncc; i++) {
        ccb = ccbaGetCcb(ccba, i);
        if (boxaGetBoxGeometry(ccb->boxa, 0, &bx, &by, &bw, &bh)) {
            bbufferDestroy(&bbuf);
            ccbDestroy(&ccb);
            return ERROR_INT("bounding box not found", __func__, 1);
        }
        bbufferRead(bbuf, (l_uint8 *)&bx, 4);  /* ulx of c.c. */
        bbufferRead(bbuf, (l_uint8 *)&by, 4);  /* uly of c.c. */
        bbufferRead(bbuf, (l_uint8 *)&bw, 4);  /* w of c.c. */
        bbufferRead(bbuf, (l_uint8 *)&bh, 4);  /* h of c.c. */
        if ((naa = ccb->step) == NULL) {
            ccbaGenerateStepChains(ccba);
            naa = ccb->step;
        }
        nb = numaaGetCount(naa);
        bbufferRead(bbuf, (l_uint8 *)&nb, 4);  /* number of borders in c.c. */
        pta = ccb->start;
        for (j = 0; j < nb; j++) {
            ptaGetIPt(pta, j, &startx, &starty);
            bbufferRead(bbuf, (l_uint8 *)&startx, 4); /* starting x in border */
            bbufferRead(bbuf, (l_uint8 *)&starty, 4); /* starting y in border */
            na = numaaGetNuma(naa, j, L_CLONE);
            n = numaGetCount(na);
            for (k = 0; k < n; k++) {
                numaGetIValue(na, k, &val);
                if (k % 2 == 0)
                    bval = (l_uint8)val << 4;
                else
                    bval |= (l_uint8)val;
                if (k % 2 == 1)
                    bbufferRead(bbuf, (l_uint8 *)&bval, 1); /* 2 border steps */
            }
            if (n % 2 == 1) {
                bval |= 0x8;
                bbufferRead(bbuf, (l_uint8 *)&bval, 1); /* end with 0xz8,   */
                                             /* where z = {0..7} */
            } else {  /* n % 2 == 0 */
                bval = 0x88;
                bbufferRead(bbuf, (l_uint8 *)&bval, 1);   /* end with 0x88 */
            }
            numaDestroy(&na);
        }
        ccbDestroy(&ccb);
    }

    datain = bbufferDestroyAndSaveData(&bbuf, &inbytes);
    dataout = zlibCompress(datain, inbytes, &outbytes);
    fwrite(dataout, 1, outbytes, fp);

    LEPT_FREE(datain);
    LEPT_FREE(dataout);
    return 0;

#endif  /* !HAVE_LIBZ */
}


/*!
 * \brief   ccbaRead()
 *
 * \param[in]    filename
 * \return  ccba, or NULL on error
 */
CCBORDA *
ccbaRead(const char  *filename)
{
FILE     *fp;
CCBORDA  *ccba;

    if (!filename)
        return (CCBORDA *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (CCBORDA *)ERROR_PTR_1("stream not opened",
                                      filename, __func__, NULL);
    ccba = ccbaReadStream(fp);
    fclose(fp);

    if (!ccba)
        return (CCBORDA *)ERROR_PTR_1("ccba not returned",
                                      filename, __func__, NULL);
    return ccba;
}


/*!
 * \brief   ccbaReadStream()
 *
 * \param[in]     fp     file stream
 * \return   ccba, or NULL on error
 *
 * \code
 *  Format:  ccba: %7d cc\n num. c.c.) (ascii)   (17B
 *           pix width 4B
 *           pix height 4B
 *           [for i = 1, ncc]
 *               ulx  4B
 *               uly  4B
 *               w    4B       -- not req'd for reconstruction
 *               h    4B       -- not req'd for reconstruction
 *               number of borders 4B
 *               [for j = 1, nb]
 *                   startx  4B
 *                   starty  4B
 *                   [for k = 1, nb]
 *                        2 steps 1B
 *                   end in z8 or 88  1B
 * \endcode
 */
CCBORDA *
ccbaReadStream(FILE  *fp)
{
#if  HAVE_LIBZ  /* defined in environ.h */
char      strbuf[256];
l_uint8   bval;
l_uint8  *datain, *dataout;
l_int32   i, j, startx, starty;
l_int32   offset, nib1, nib2;
l_int32   ncc, nb;
l_uint32  width, height, w, h, xoff, yoff;
size_t    inbytes, outbytes;
BOX      *box;
CCBORD   *ccb;
CCBORDA  *ccba;
NUMA     *na;
NUMAA    *step;
#endif

#if  !HAVE_LIBZ  /* defined in environ.h */
    return (CCBORDA *)ERROR_PTR("no libz: can't read data", __func__, NULL);
#else

    if (!fp)
        return (CCBORDA *)ERROR_PTR("stream not open", __func__, NULL);

    if ((datain = l_binaryReadStream(fp, &inbytes)) == NULL)
        return (CCBORDA *)ERROR_PTR("data not read from file", __func__, NULL);
    dataout = zlibUncompress(datain, inbytes, &outbytes);
    LEPT_FREE(datain);
    if (!dataout)
        return (CCBORDA *)ERROR_PTR("dataout not made", __func__, NULL);

    offset = 18;
    memcpy(strbuf, dataout, offset);
    strbuf[17] = '\0';
    if (memcmp(strbuf, "ccba:", 5) != 0) {
        LEPT_FREE(dataout);
        return (CCBORDA *)ERROR_PTR("file not type ccba", __func__, NULL);
    }
    sscanf(strbuf, "ccba: %7d cc\n", &ncc);
/*    lept_stderr("ncc = %d\n", ncc); */
    if ((ccba = ccbaCreate(NULL, ncc)) == NULL) {
        LEPT_FREE(dataout);
        return (CCBORDA *)ERROR_PTR("ccba not made", __func__, NULL);
    }

    memcpy(&width, dataout + offset, 4);
    offset += 4;
    memcpy(&height, dataout + offset, 4);
    offset += 4;
    ccba->w = width;
    ccba->h = height;
/*    lept_stderr("width = %d, height = %d\n", width, height); */

    for (i = 0; i < ncc; i++) {  /* should be ncc */
        ccb = ccbCreate(NULL);
        ccbaAddCcb(ccba, ccb);

        memcpy(&xoff, dataout + offset, 4);
        offset += 4;
        memcpy(&yoff, dataout + offset, 4);
        offset += 4;
        memcpy(&w, dataout + offset, 4);
        offset += 4;
        memcpy(&h, dataout + offset, 4);
        offset += 4;
        box = boxCreate(xoff, yoff, w, h);
        boxaAddBox(ccb->boxa, box, L_INSERT);
/*        lept_stderr("xoff = %d, yoff = %d, w = %d, h = %d\n",
                xoff, yoff, w, h); */

        memcpy(&nb, dataout + offset, 4);
        offset += 4;
/*        lept_stderr("num borders = %d\n", nb); */
        step = numaaCreate(nb);
        ccb->step = step;

        for (j = 0; j < nb; j++) {  /* should be nb */
            memcpy(&startx, dataout + offset, 4);
            offset += 4;
            memcpy(&starty, dataout + offset, 4);
            offset += 4;
            ptaAddPt(ccb->start, startx, starty);
/*            lept_stderr("startx = %d, starty = %d\n", startx, starty); */
            na = numaCreate(0);
            numaaAddNuma(step, na, L_INSERT);

            while(1) {
                bval = *(dataout + offset);
                offset++;
                nib1 = (bval >> 4);
                nib2 = bval & 0xf;
                if (nib1 != 8)
                    numaAddNumber(na, nib1);
                else
                    break;
                if (nib2 != 8)
                    numaAddNumber(na, nib2);
                else
                    break;
            }
        }
    }
    LEPT_FREE(dataout);
    return ccba;

#endif  /* !HAVE_LIBZ */
}


/*---------------------------------------------------------------------*
 *                                SVG Output                           *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ccbaWriteSVG()
 *
 * \param[in]    filename
 * \param[in]    ccba
 * \return  0 if OK, 1 on error
 */
l_ok
ccbaWriteSVG(const char  *filename,
             CCBORDA     *ccba)
{
char  *svgstr;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!ccba)
        return ERROR_INT("ccba not defined", __func__, 1);

    if ((svgstr = ccbaWriteSVGString(ccba)) == NULL)
        return ERROR_INT("svgstr not made", __func__, 1);

    l_binaryWrite(filename, "w", svgstr, strlen(svgstr));
    LEPT_FREE(svgstr);

    return 0;
}


/*!
 * \brief   ccbaWriteSVGString()
 *
 * \param[in]    ccba
 * \return  string in svg-formatted, that can be written to file,
 *              or NULL on error.
 */
char  *
ccbaWriteSVGString(CCBORDA *ccba)
{
char    *svgstr;
char     smallbuf[256];
char     line0[] = "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>";
char     line1[] = "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 20000303 Stylable//EN\" \"http://www.w3.org/TR/2000/03/WD-SVG-20000303/DTD/svg-20000303-stylable.dtd\">";
char     line2[] = "<svg>";
char     line3[] = "<polygon style=\"stroke-width:1;stroke:black;\" points=\"";
char     line4[] = "\" />";
char     line5[] = "</svg>";
char     space[] = " ";
l_int32  i, j, ncc, npt, x, y;
CCBORD  *ccb;
PTA     *pta;
SARRAY  *sa;

    if (!ccba)
        return (char *)ERROR_PTR("ccba not defined", __func__, NULL);

    sa = sarrayCreate(0);
    sarrayAddString(sa, line0, L_COPY);
    sarrayAddString(sa, line1, L_COPY);
    sarrayAddString(sa, line2, L_COPY);
    ncc = ccbaGetCount(ccba);
    for (i = 0; i < ncc; i++) {
        if ((ccb = ccbaGetCcb(ccba, i)) == NULL) {
            sarrayDestroy(&sa);
            return (char *)ERROR_PTR("ccb not found", __func__, NULL);
        }
        if ((pta = ccb->spglobal) == NULL) {
            sarrayDestroy(&sa);
            ccbDestroy(&ccb);
            return (char *)ERROR_PTR("spglobal not made", __func__, NULL);
        }
        sarrayAddString(sa, line3, L_COPY);
        npt = ptaGetCount(pta);
        for (j = 0; j < npt; j++) {
            ptaGetIPt(pta, j, &x, &y);
            snprintf(smallbuf, sizeof(smallbuf), "%0d,%0d", x, y);
            sarrayAddString(sa, smallbuf, L_COPY);
        }
        sarrayAddString(sa, line4, L_COPY);
        ccbDestroy(&ccb);
    }
    sarrayAddString(sa, line5, L_COPY);
    sarrayAddString(sa, space, L_COPY);

    svgstr = sarrayToString(sa, 1);
/*    lept_stderr("%s", svgstr); */

    sarrayDestroy(&sa);
    return svgstr;
}

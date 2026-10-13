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
 * \file conncomp.c
 * <pre>
 *
 *    Connected component counting and extraction, using Heckbert's
 *    stack-based filling algorithm.
 *
 *      4- and 8-connected components: counts, bounding boxes and images
 *
 *      Top-level calls:
 *            BOXA     *pixConnComp()
 *            BOXA     *pixConnCompPixa()
 *            BOXA     *pixConnCompBB()
 *            l_int32   pixCountConnComp()
 *
 *      Identify the next c.c. to be erased:
 *            l_int32   nextOnPixelInRaster()
 *    static  l_int32   nextOnPixelInRasterLow()
 *
 *      Erase the c.c., saving the b.b.:
 *            BOX      *pixSeedfillBB()
 *            BOX      *pixSeedfill4BB()
 *            BOX      *pixSeedfill8BB()
 *
 *      Just erase the c.c.:
 *            l_int32   pixSeedfill()
 *            l_int32   pixSeedfill4()
 *            l_int32   pixSeedfill8()
 *
 *      Static stack helper functions for single raster line seedfill:
 *            static void    pushFillsegBB()
 *            static void    pushFillseg()
 *            static void    popFillseg()
 *
 *  The basic method in pixConnCompBB() is very simple.  We scan the
 *  image in raster order, looking for the next ON pixel.  When it
 *  is found, we erase it and every pixel of the 4- or 8-connected
 *  component to which it belongs, using Heckbert's seedfill
 *  algorithm.  As pixels are erased, we keep track of the
 *  minimum rectangle that encloses all erased pixels; after
 *  the connected component has been erased, we save its
 *  bounding box in an array of boxes.  When all pixels in the
 *  image have been erased, we have an array that describes every
 *  4- or 8-connected component in terms of its bounding box.
 *
 *  pixConnCompPixa() is a slight variation on pixConnCompBB(),
 *  where we additionally save an array of images (in a Pixa)
 *  of each of the 4- or 8-connected components.  This is done trivially
 *  by maintaining two temporary images.  We erase a component from one,
 *  and use the bounding box to extract the pixels within the b.b.
 *  from each of the two images.  An XOR between these subimages
 *  gives the erased component.  Then we erase the component from the
 *  second image using the XOR again, with the extracted component
 *  placed on the second image at the location of the bounding box.
 *  Rasterop does all the work.  At the end, we have an array
 *  of the 4- or 8-connected components, as well as an array of the
 *  bounding boxes that describe where they came from in the original image.
 *
 *  If you just want the number of connected components, pixCountConnComp()
 *  is a bit faster than pixConnCompBB(), because it doesn't have to
 *  keep track of the bounding rectangles for each c.c.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"
#include "pix_internal.h"

/*!
 * \brief   The struct FillSeg is used by the Heckbert seedfill algorithm to
 *  hold information about image segments that are waiting to be
 *  investigated.  We use two Stacks, one to hold the FillSegs in use,
 *  and an auxiliary Stack as a reservoir to hold FillSegs for re-use.
 */
struct FillSeg
{
    l_int32    xleft;    /*!< left edge of run */
    l_int32    xright;   /*!< right edge of run */
    l_int32    y;        /*!< run y  */
    l_int32    dy;       /*!< parent segment direction: 1 above, -1 below) */
};
typedef struct FillSeg    FILLSEG;

static l_int32 nextOnPixelInRasterLow(l_uint32 *data, l_int32 w, l_int32 h,
                                      l_int32 wpl, l_int32 xstart,
                                      l_int32 ystart, l_int32 *px, l_int32 *py);

    /* Static accessors for FillSegs on a stack */
static void pushFillsegBB(L_STACK *stack, l_int32 xleft, l_int32 xright,
                          l_int32 y, l_int32 dy, l_int32 ymax,
                          l_int32 *pminx, l_int32 *pmaxx,
                          l_int32 *pminy, l_int32 *pmaxy);
static void pushFillseg(L_STACK *stack, l_int32 xleft, l_int32 xright,
                        l_int32 y, l_int32 dy, l_int32 ymax);
static void popFillseg(L_STACK *stack, l_int32 *pxleft, l_int32 *pxright,
                       l_int32 *py, l_int32 *pdy);


#ifndef  NO_CONSOLE_IO
#define   DEBUG    0
#endif  /* ~NO_CONSOLE_IO */


/*-----------------------------------------------------------------------*
 *                Bounding boxes of 4 Connected Components               *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pixConnComp()
 *
 * \param[in]    pixs           1 bpp
 * \param[out]   ppixa          [optional] pixa of each c.c.
 * \param[in]    connectivity   4 or 8
 * \return  boxa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is the top-level call for getting bounding boxes or
 *          a pixa of the components, and it can be used instead
 *          of either pixConnCompBB() or pixConnCompPixa(), rsp.
 * </pre>
 */
BOXA *
pixConnComp(PIX     *pixs,
            PIXA   **ppixa,
            l_int32  connectivity)
{

    if (ppixa) *ppixa = NULL;
    if (!pixs || pixGetDepth(pixs) != 1)
        return (BOXA *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (BOXA *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);

    if (!ppixa)
        return pixConnCompBB(pixs, connectivity);
    else
        return pixConnCompPixa(pixs, ppixa, connectivity);
}


/*!
 * \brief   pixConnCompPixa()
 *
 * \param[in]    pixs           1 bpp
 * \param[out]   ppixa          pixa of each c.c.
 * \param[in]    connectivity   4 or 8
 * \return  boxa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This finds bounding boxes of 4- or 8-connected components
 *          in a binary image, and saves images of each c.c
 *          in a pixa array.
 *      (2) It sets up 2 temporary pix, and for each c.c. that is
 *          located in raster order, it erases the c.c. from one pix,
 *          then uses the b.b. to extract the c.c. from the two pix using
 *          an XOR, and finally erases the c.c. from the second pix.
 *      (3) A clone of the returned boxa (where all boxes in the array
 *          are clones) is inserted into the pixa.
 *      (4) If the input is valid, this always returns a boxa and a pixa.
 *          If pixs is empty, the boxa and pixa will be empty.
 * </pre>
 */
BOXA *
pixConnCompPixa(PIX     *pixs,
                PIXA   **ppixa,
                l_int32  connectivity)
{
l_int32   h, iszero;
l_int32   x, y, xstart, ystart;
PIX      *pix1, *pix2, *pix3, *pix4;
PIXA     *pixa;
BOX      *box;
BOXA     *boxa;
L_STACK  *stack, *auxstack;

    if (!ppixa)
        return (BOXA *)ERROR_PTR("&pixa not defined", __func__, NULL);
    *ppixa = NULL;
    if (!pixs || pixGetDepth(pixs) != 1)
        return (BOXA *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (BOXA *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);

    pix1 = pix2 = pix3 = pix4 = NULL;
    stack = NULL;
    pixa = pixaCreate(0);
    boxa = NULL;
    *ppixa = pixa;
    pixZero(pixs, &iszero);
    if (iszero)
        return boxaCreate(1);  /* return empty boxa and empty pixa */

    pixSetPadBits(pixs, 0);
    pix1 = pixCopy(NULL, pixs);
    pix2 = pixCopy(NULL, pixs);
    if (!pix1 || !pix2) {
        L_ERROR("pix1 or pix2 not made\n", __func__);
        pixaDestroy(ppixa);
        goto cleanup;
    }

    h = pixGetHeight(pixs);
    if ((stack = lstackCreate(h)) == NULL) {
        L_ERROR("stack not made\n", __func__);
        pixaDestroy(ppixa);
        goto cleanup;
    }
    auxstack = lstackCreate(0);
    stack->auxstack = auxstack;
    boxa = boxaCreate(0);

    xstart = 0;
    ystart = 0;
    while (1) {
        if (!nextOnPixelInRaster(pix1, xstart, ystart, &x, &y))
            break;

        if ((box = pixSeedfillBB(pix1, stack, x, y, connectivity)) == NULL) {
            boxaDestroy(&boxa);
            pixaDestroy(ppixa);
            L_ERROR("box not made\n", __func__);
            goto cleanup;
        }
        boxaAddBox(boxa, box, L_INSERT);

            /* Save the c.c. and remove from pix2 as well */
        pix3 = pixClipRectangle(pix1, box, NULL);
        pix4 = pixClipRectangle(pix2, box, NULL);
        pixXor(pix3, pix3, pix4);
        pixRasterop(pix2, box->x, box->y, box->w, box->h, PIX_SRC ^ PIX_DST,
                    pix3, 0, 0);
        pixaAddPix(pixa, pix3, L_INSERT);
        pixDestroy(&pix4);

        xstart = x;
        ystart = y;
    }

#if  DEBUG
    pixCountPixels(pix1, &iszero, NULL);
    lept_stderr("Number of remaining pixels = %d\n", iszero);
    lept_mkdir("lept/cc");
    pixWriteDebug("/tmp/lept/cc/remain.png", pix1, IFF_PNG);
#endif  /* DEBUG */

        /* Remove old boxa of pixa and replace with a copy */
    boxaDestroy(&pixa->boxa);
    pixa->boxa = boxaCopy(boxa, L_COPY);
    *ppixa = pixa;

        /* Cleanup, freeing the fillsegs on each stack */
cleanup:
    lstackDestroy(&stack, TRUE);
    pixDestroy(&pix1);
    pixDestroy(&pix2);
    return boxa;
}


/*!
 * \brief   pixConnCompBB()
 *
 * \param[in]    pixs           1 bpp
 * \param[in]    connectivity   4 or 8
 * \return  boxa, or NULL on error
 *
 * <pre>
 * Notes:
 *     (1) Finds bounding boxes of 4- or 8-connected components
 *         in a binary image.
 *     (2) This works on a copy of the input pix.  The c.c. are located
 *         in raster order and erased one at a time.  In the process,
 *         the b.b. is computed and saved.
 * </pre>
 */
BOXA *
pixConnCompBB(PIX     *pixs,
              l_int32  connectivity)
{
l_int32   h, iszero;
l_int32   x, y, xstart, ystart;
PIX      *pix1;
BOX      *box;
BOXA     *boxa;
L_STACK  *stack, *auxstack;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (BOXA *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (BOXA *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);

    boxa = NULL;
    pix1 = NULL;
    stack = NULL;
    pixZero(pixs, &iszero);
    if (iszero)
        return boxaCreate(1);  /* return empty boxa */

    pixSetPadBits(pixs, 0);
    if ((pix1 = pixCopy(NULL, pixs)) == NULL)
        return (BOXA *)ERROR_PTR("pix1 not made", __func__, NULL);

    h = pixGetHeight(pixs);
    if ((stack = lstackCreate(h)) == NULL) {
        L_ERROR("stack not made\n", __func__);
        goto cleanup;
    }
    auxstack = lstackCreate(0);
    stack->auxstack = auxstack;
    boxa = boxaCreate(0);

    xstart = 0;
    ystart = 0;
    while (1) {
        if (!nextOnPixelInRaster(pix1, xstart, ystart, &x, &y))
            break;

        if ((box = pixSeedfillBB(pix1, stack, x, y, connectivity)) == NULL) {
            L_ERROR("box not made\n", __func__);
            boxaDestroy(&boxa);
            goto cleanup;
        }
        boxaAddBox(boxa, box, L_INSERT);

        xstart = x;
        ystart = y;
    }

#if  DEBUG
    pixCountPixels(pix1, &iszero, NULL);
    lept_stderr("Number of remaining pixels = %d\n", iszero);
    lept_mkdir("lept/cc");
    pixWriteDebug("/tmp/lept/cc/remain.png", pix1, IFF_PNG);
#endif  /* DEBUG */

        /* Cleanup, freeing the fillsegs on each stack */
cleanup:
    lstackDestroy(&stack, TRUE);
    pixDestroy(&pix1);
    return boxa;
}


/*!
 * \brief   pixCountConnComp()
 *
 * \param[in]    pixs           1 bpp
 * \param[in]    connectivity   4 or 8
 * \param[out]   pcount
 * \return  0 if OK, 1 on error
 *
 * Notes:
 *     (1 This is the top-level call for getting the number of
 *         4- or 8-connected components in a 1 bpp image.
 *     2 It works on a copy of the input pix.  The c.c. are located
 *         in raster order and erased one at a time.
 */
l_ok
pixCountConnComp(PIX      *pixs,
                 l_int32   connectivity,
                 l_int32  *pcount)
{
l_int32   h, iszero;
l_int32   x, y, xstart, ystart;
PIX      *pix1;
L_STACK  *stack, *auxstack;

    if (!pcount)
        return ERROR_INT("&count not defined", __func__, 1);
    *pcount = 0;  /* initialize the count to 0 */
    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);
    if (connectivity != 4 && connectivity != 8)
        return ERROR_INT("connectivity not 4 or 8", __func__, 1);

    stack = NULL;
    pixZero(pixs, &iszero);
    if (iszero)
        return 0;

    pixSetPadBits(pixs, 0);
    if ((pix1 = pixCopy(NULL, pixs)) == NULL)
        return ERROR_INT("pix1 not made", __func__, 1);
    h = pixGetHeight(pixs);
    if ((stack = lstackCreate(h)) == NULL) {
        pixDestroy(&pix1);
        return ERROR_INT("stack not made\n", __func__, 1);
    }
    auxstack = lstackCreate(0);
    stack->auxstack = auxstack;

    xstart = 0;
    ystart = 0;
    while (1) {
        if (!nextOnPixelInRaster(pix1, xstart, ystart, &x, &y))
            break;

        pixSeedfill(pix1, stack, x, y, connectivity);
        (*pcount)++;
        xstart = x;
        ystart = y;
    }

        /* Cleanup, freeing the fillsegs on each stack */
    lstackDestroy(&stack, TRUE);
    pixDestroy(&pix1);
    return 0;
}


/*!
 * \brief   nextOnPixelInRaster()
 *
 * \param[in]    pixs             1 bpp
 * \param[in]    xstart, ystart   starting point for search
 * \param[out]   px, py           coord value of next ON pixel
 * \return  1 if a pixel is found; 0 otherwise or on error
 */
l_int32
nextOnPixelInRaster(PIX      *pixs,
                    l_int32   xstart,
                    l_int32   ystart,
                    l_int32  *px,
                    l_int32  *py)
{
l_int32    w, h, d, wpl;
l_uint32  *data;

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 0);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 1)
        return ERROR_INT("pixs not 1 bpp", __func__, 0);

    wpl = pixGetWpl(pixs);
    data = pixGetData(pixs);
    return nextOnPixelInRasterLow(data, w, h, wpl, xstart, ystart, px, py);
}


/*!
 * \brief   nextOnPixelInRasterLow()
 *
 * \param[in]    data             pix data
 * \param[in]    w, h             width and height
 * \param[in]    wpl              words per line
 * \param[in]    xstart, ystart   starting point for search
 * \param[out]   px, py           coord value of next ON pixel
 * \return  1 if a pixel is found; 0 otherwise or on error
 */
static l_int32
nextOnPixelInRasterLow(l_uint32  *data,
                       l_int32    w,
                       l_int32    h,
                       l_int32    wpl,
                       l_int32    xstart,
                       l_int32    ystart,
                       l_int32   *px,
                       l_int32   *py)
{
l_int32    i, x, y, xend, startword;
l_uint32  *line, *pword;

        /* Look at the first word */
    line = data + ystart * wpl;
    pword = line + (xstart / 32);
    if (*pword) {
        xend = xstart - (xstart % 32) + 31;
        for (x = xstart; x <= xend && x < w; x++) {
            if (GET_DATA_BIT(line, x)) {
                *px = x;
                *py = ystart;
                return 1;
            }
        }
    }

        /* Continue with the rest of the line */
    startword = (xstart / 32) + 1;
    x = 32 * startword;
    for (pword = line + startword; x < w; pword++, x += 32) {
        if (*pword) {
            for (i = 0; i < 32 && x < w; i++, x++) {
                if (GET_DATA_BIT(line, x)) {
                    *px = x;
                    *py = ystart;
                    return 1;
                }
            }
        }
    }

        /* Continue with following lines */
    for (y = ystart + 1; y < h; y++) {
        line = data + y * wpl;
        for (pword = line, x = 0; x < w; pword++, x += 32) {
            if (*pword) {
                for (i = 0; i < 32 && x < w; i++, x++) {
                    if (GET_DATA_BIT(line, x)) {
                        *px = x;
                        *py = y;
                        return 1;
                    }
                }
            }
        }
    }

    return 0;
}


/*!
 * \brief   pixSeedfillBB()
 *
 * \param[in]    pixs           1 bpp
 * \param[in]    stack          for holding fillsegs
 * \param[in]    x,y            location of seed pixel
 * \param[in]    connectivity   4 or 8
 * \return  box or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is the high-level interface to Paul Heckbert's
 *          stack-based seedfill algorithm.
 * </pre>
 */
BOX *
pixSeedfillBB(PIX      *pixs,
              L_STACK  *stack,
              l_int32   x,
              l_int32   y,
              l_int32   connectivity)
{
BOX  *box;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (BOX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (!stack)
        return (BOX *)ERROR_PTR("stack not defined", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (BOX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);

    if (connectivity == 4) {
        if ((box = pixSeedfill4BB(pixs, stack, x, y)) == NULL)
            return (BOX *)ERROR_PTR("box not made", __func__, NULL);
    } else if (connectivity == 8) {
        if ((box = pixSeedfill8BB(pixs, stack, x, y)) == NULL)
            return (BOX *)ERROR_PTR("box not made", __func__, NULL);
    } else {
        return (BOX *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);
    }

    return box;
}


/*!
 * \brief   pixSeedfill4BB()
 *
 * \param[in]    pixs     1 bpp
 * \param[in]    stack    for holding fillsegs
 * \param[in]    x,y      location of seed pixel
 * \return  box or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) This is Paul Heckbert's stack-based 4-cc seedfill algorithm.
 *      (2) This operates on the input 1 bpp pix to remove the fg seed
 *          pixel, at (x,y), and all pixels that are 4-connected to it.
 *          The seed pixel at (x,y) must initially be ON.
 *      (3) Returns the bounding box of the erased 4-cc component.
 *      (4) Reference: see Paul Heckbert's stack-based seed fill algorithm
 *          in "Graphic Gems", ed. Andrew Glassner, Academic
 *          Press, 1990.  The algorithm description is given
 *          on pp. 275-277; working C code is on pp. 721-722.)
 *          The code here follows Heckbert's exactly, except
 *          we use function calls instead of macros for
 *          pushing data on and popping data off the stack.
 *          This makes sense to do because Heckbert's fixed-size
 *          stack with macros is dangerous: images exist that
 *          will overrun the stack and crash.   The stack utility
 *          here grows dynamically as needed, and the fillseg
 *          structures that are not in use are stored in another
 *          stack for reuse.  It should be noted that the
 *          overhead in the function calls (vs. macros) is negligible.
 * </pre>
 */
BOX *
pixSeedfill4BB(PIX      *pixs,
               L_STACK  *stack,
               l_int32   x,
               l_int32   y)
{
l_int32    w, h, xstart, wpl, x1, x2, dy;
l_int32    xmax, ymax;
l_int32    minx, maxx, miny, maxy;  /* for bounding box of this c.c. */
l_uint32  *data, *line;
BOX       *box;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (BOX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (!stack)
        return (BOX *)ERROR_PTR("stack not defined", __func__, NULL);
    if (!stack->auxstack)
        stack->auxstack = lstackCreate(0);

    pixGetDimensions(pixs, &w, &h, NULL);
    xmax = w - 1;
    ymax = h - 1;
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    line = data + y * wpl;

        /* Check pix value of seed; must be within the image and ON */
    if (x < 0 || x > xmax || y < 0 || y > ymax || (GET_DATA_BIT(line, x) == 0))
        return NULL;

        /* Init stack to seed:
         * Must first init b.b. values to prevent valgrind from complaining;
         * then init b.b. boundaries correctly to seed.  */
    minx = miny = 100000;
    maxx = maxy = 0;
    pushFillsegBB(stack, x, x, y, 1, ymax, &minx, &maxx, &miny, &maxy);
    pushFillsegBB(stack, x, x, y + 1, -1, ymax, &minx, &maxx, &miny, &maxy);
    minx = maxx = x;
    miny = maxy = y;

    while (lstackGetCount(stack) > 0) {
            /* Pop segment off stack and fill a neighboring scan line */
        popFillseg(stack, &x1, &x2, &y, &dy);
        line = data + y * wpl;

            /* A segment of scanline y - dy for x1 <= x <= x2 was
             * previously filled.  We now explore adjacent pixels
             * in scan line y.  There are three regions: to the
             * left of x1 - 1, between x1 and x2, and to the right of x2.
             * These regions are handled differently.  Leaks are
             * possible expansions beyond the previous segment and
             * going back in the -dy direction.  These can happen
             * for x < x1 - 1 and for x > x2 + 1.  Any "leak" segments
             * are plugged with a push in the -dy (opposite) direction.
             * And any segments found anywhere are always extended
             * in the +dy direction.  */
        for (x = x1; x >= 0 && (GET_DATA_BIT(line, x) == 1); x--)
            CLEAR_DATA_BIT(line,x);
        if (x >= x1)  /* pix at x1 was off and was not cleared */
            goto skip;
        xstart = x + 1;
        if (xstart < x1 - 1)   /* leak on left? */
            pushFillsegBB(stack, xstart, x1 - 1, y, -dy,
                          ymax, &minx, &maxx, &miny, &maxy);

        x = x1 + 1;
        do {
            for (; x <= xmax && (GET_DATA_BIT(line, x) == 1); x++)
                CLEAR_DATA_BIT(line, x);
            pushFillsegBB(stack, xstart, x - 1, y, dy,
                          ymax, &minx, &maxx, &miny, &maxy);
            if (x > x2 + 1)   /* leak on right? */
                pushFillsegBB(stack, x2 + 1, x - 1, y, -dy,
                              ymax, &minx, &maxx, &miny, &maxy);
    skip:   for (x++; x <= x2 &&
                      x <= xmax &&
                      (GET_DATA_BIT(line, x) == 0); x++)
                ;
            xstart = x;
        } while (x <= x2 && x <= xmax);
    }

    if ((box = boxCreate(minx, miny, maxx - minx + 1, maxy - miny + 1))
            == NULL)
        return (BOX *)ERROR_PTR("box not made", __func__, NULL);
    return box;
}


/*!
 * \brief   pixSeedfill8BB()
 *
 * \param[in]    pixs    1 bpp
 * \param[in]    stack   for holding fillsegs
 * \param[in]    x,y     location of seed pixel
 * \return  box or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) This is Paul Heckbert's stack-based 8-cc seedfill algorithm.
 *      (2) This operates on the input 1 bpp pix to remove the fg seed
 *          pixel, at (x,y), and all pixels that are 8-connected to it.
 *          The seed pixel at (x,y) must initially be ON.
 *      (3) Returns the bounding box of the erased 8-cc component.
 *      (4) Reference: see Paul Heckbert's stack-based seed fill algorithm
 *          in "Graphic Gems", ed. Andrew Glassner, Academic
 *          Press, 1990.  The algorithm description is given
 *          on pp. 275-277; working C code is on pp. 721-722.)
 *          The code here follows Heckbert's closely, except
 *          the leak checks are changed for 8 connectivity.
 *          See comments on pixSeedfill4BB() for more details.
 * </pre>
 */
BOX *
pixSeedfill8BB(PIX      *pixs,
               L_STACK  *stack,
               l_int32   x,
               l_int32   y)
{
l_int32    w, h, xstart, wpl, x1, x2, dy;
l_int32    xmax, ymax;
l_int32    minx, maxx, miny, maxy;  /* for bounding box of this c.c. */
l_uint32  *data, *line;
BOX       *box;

    if (!pixs || pixGetDepth(pixs) != 1)
        return (BOX *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (!stack)
        return (BOX *)ERROR_PTR("stack not defined", __func__, NULL);
    if (!stack->auxstack)
        stack->auxstack = lstackCreate(0);

    pixGetDimensions(pixs, &w, &h, NULL);
    xmax = w - 1;
    ymax = h - 1;
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    line = data + y * wpl;

        /* Check pix value of seed; must be ON */
    if (x < 0 || x > xmax || y < 0 || y > ymax || (GET_DATA_BIT(line, x) == 0))
        return NULL;

        /* Init stack to seed:
         * Must first init b.b. values to prevent valgrind from complaining;
         * then init b.b. boundaries correctly to seed.  */
    minx = miny = 100000;
    maxx = maxy = 0;
    pushFillsegBB(stack, x, x, y, 1, ymax, &minx, &maxx, &miny, &maxy);
    pushFillsegBB(stack, x, x, y + 1, -1, ymax, &minx, &maxx, &miny, &maxy);
    minx = maxx = x;
    miny = maxy = y;

    while (lstackGetCount(stack) > 0) {
            /* Pop segment off stack and fill a neighboring scan line */
        popFillseg(stack, &x1, &x2, &y, &dy);
        line = data + y * wpl;

            /* A segment of scanline y - dy for x1 <= x <= x2 was
             * previously filled.  We now explore adjacent pixels
             * in scan line y.  There are three regions: to the
             * left of x1, between x1 and x2, and to the right of x2.
             * These regions are handled differently.  Leaks are
             * possible expansions beyond the previous segment and
             * going back in the -dy direction.  These can happen
             * for x < x1 and for x > x2.  Any "leak" segments
             * are plugged with a push in the -dy (opposite) direction.
             * And any segments found anywhere are always extended
             * in the +dy direction.  */
        for (x = x1 - 1; x >= 0 && (GET_DATA_BIT(line, x) == 1); x--)
            CLEAR_DATA_BIT(line,x);
        if (x >= x1 - 1)  /* pix at x1 - 1 was off and was not cleared */
            goto skip;
        xstart = x + 1;
        if (xstart < x1)   /* leak on left? */
            pushFillsegBB(stack, xstart, x1 - 1, y, -dy,
                          ymax, &minx, &maxx, &miny, &maxy);

        x = x1;
        do {
            for (; x <= xmax && (GET_DATA_BIT(line, x) == 1); x++)
                CLEAR_DATA_BIT(line, x);
            pushFillsegBB(stack, xstart, x - 1, y, dy,
                          ymax, &minx, &maxx, &miny, &maxy);
            if (x > x2)   /* leak on right? */
                pushFillsegBB(stack, x2 + 1, x - 1, y, -dy,
                              ymax, &minx, &maxx, &miny, &maxy);
    skip:   for (x++; x <= x2 + 1 &&
                      x <= xmax &&
                      (GET_DATA_BIT(line, x) == 0); x++)
                ;
            xstart = x;
        } while (x <= x2 + 1 && x <= xmax);
    }

    if ((box = boxCreate(minx, miny, maxx - minx + 1, maxy - miny + 1))
            == NULL)
        return (BOX *)ERROR_PTR("box not made", __func__, NULL);
    return box;
}


/*!
 * \brief   pixSeedfill()
 *
 * \param[in]    pixs           1 bpp
 * \param[in]    stack          for holding fillsegs
 * \param[in]    x,y            location of seed pixel
 * \param[in]    connectivity   4 or 8
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This removes the component from pixs with a fg pixel at (x,y).
 *      (2) See pixSeedfill4() and pixSeedfill8() for details.
 * </pre>
 */
l_ok
pixSeedfill(PIX      *pixs,
            L_STACK  *stack,
            l_int32   x,
            l_int32   y,
            l_int32   connectivity)
{
l_int32  retval;

    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);
    if (!stack)
        return ERROR_INT("stack not defined", __func__, 1);
    if (connectivity != 4 && connectivity != 8)
        return ERROR_INT("connectivity not 4 or 8", __func__, 1);

    if (connectivity == 4)
        retval = pixSeedfill4(pixs, stack, x, y);
    else  /* connectivity == 8  */
        retval = pixSeedfill8(pixs, stack, x, y);

    return retval;
}


/*!
 * \brief   pixSeedfill4()
 *
 * \param[in]    pixs    1 bpp
 * \param[in]    stack   for holding fillsegs
 * \param[in]    x,y     location of seed pixel
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is Paul Heckbert's stack-based 4-cc seedfill algorithm.
 *      (2) This operates on the input 1 bpp pix to remove the fg seed
 *          pixel, at (x,y), and all pixels that are 4-connected to it.
 *          The seed pixel at (x,y) must initially be ON.
 *      (3) Reference: see pixSeedFill4BB()
 * </pre>
 */
l_ok
pixSeedfill4(PIX      *pixs,
             L_STACK  *stack,
             l_int32   x,
             l_int32   y)
{
l_int32    w, h, xstart, wpl, x1, x2, dy;
l_int32    xmax, ymax;
l_uint32  *data, *line;

    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);
    if (!stack)
        return ERROR_INT("stack not defined", __func__, 1);
    if (!stack->auxstack)
        stack->auxstack = lstackCreate(0);

    pixGetDimensions(pixs, &w, &h, NULL);
    xmax = w - 1;
    ymax = h - 1;
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    line = data + y * wpl;

        /* Check pix value of seed; must be within the image and ON */
    if (x < 0 || x > xmax || y < 0 || y > ymax || (GET_DATA_BIT(line, x) == 0))
        return 0;

        /* Init stack to seed */
    pushFillseg(stack, x, x, y, 1, ymax);
    pushFillseg(stack, x, x, y + 1, -1, ymax);

    while (lstackGetCount(stack) > 0) {
            /* Pop segment off stack and fill a neighboring scan line */
        popFillseg(stack, &x1, &x2, &y, &dy);
        line = data + y * wpl;

            /* A segment of scanline y - dy for x1 <= x <= x2 was
             * previously filled.  We now explore adjacent pixels
             * in scan line y.  There are three regions: to the
             * left of x1 - 1, between x1 and x2, and to the right of x2.
             * These regions are handled differently.  Leaks are
             * possible expansions beyond the previous segment and
             * going back in the -dy direction.  These can happen
             * for x < x1 - 1 and for x > x2 + 1.  Any "leak" segments
             * are plugged with a push in the -dy (opposite) direction.
             * And any segments found anywhere are always extended
             * in the +dy direction.  */
        for (x = x1; x >= 0 && (GET_DATA_BIT(line, x) == 1); x--)
            CLEAR_DATA_BIT(line,x);
        if (x >= x1)  /* pix at x1 was off and was not cleared */
            goto skip;
        xstart = x + 1;
        if (xstart < x1 - 1)   /* leak on left? */
            pushFillseg(stack, xstart, x1 - 1, y, -dy, ymax);

        x = x1 + 1;
        do {
            for (; x <= xmax && (GET_DATA_BIT(line, x) == 1); x++)
                CLEAR_DATA_BIT(line, x);
            pushFillseg(stack, xstart, x - 1, y, dy, ymax);
            if (x > x2 + 1)   /* leak on right? */
                pushFillseg(stack, x2 + 1, x - 1, y, -dy, ymax);
    skip:   for (x++; x <= x2 &&
                      x <= xmax &&
                      (GET_DATA_BIT(line, x) == 0); x++)
                ;
            xstart = x;
        } while (x <= x2 && x <= xmax);
    }

    return 0;
}


/*!
 * \brief   pixSeedfill8()
 *
 * \param[in]    pixs    1 bpp
 * \param[in]    stack   for holding fillsegs
 * \param[in]    x,y     location of seed pixel
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is Paul Heckbert's stack-based 8-cc seedfill algorithm.
 *      (2) This operates on the input 1 bpp pix to remove the fg seed
 *          pixel, at (x,y), and all pixels that are 8-connected to it.
 *          The seed pixel at (x,y) must initially be ON.
 *      (3) Reference: see pixSeedFill8BB()
 * </pre>
 */
l_ok
pixSeedfill8(PIX      *pixs,
             L_STACK  *stack,
             l_int32   x,
             l_int32   y)
{
l_int32    w, h, xstart, wpl, x1, x2, dy;
l_int32    xmax, ymax;
l_uint32  *data, *line;

    if (!pixs || pixGetDepth(pixs) != 1)
        return ERROR_INT("pixs not defined or not 1 bpp", __func__, 1);
    if (!stack)
        return ERROR_INT("stack not defined", __func__, 1);
    if (!stack->auxstack)
        stack->auxstack = lstackCreate(0);

    pixGetDimensions(pixs, &w, &h, NULL);
    xmax = w - 1;
    ymax = h - 1;
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    line = data + y * wpl;

        /* Check pix value of seed; must be ON */
    if (x < 0 || x > xmax || y < 0 || y > ymax || (GET_DATA_BIT(line, x) == 0))
        return 0;

        /* Init stack to seed */
    pushFillseg(stack, x, x, y, 1, ymax);
    pushFillseg(stack, x, x, y + 1, -1, ymax);

    while (lstackGetCount(stack) > 0) {
            /* Pop segment off stack and fill a neighboring scan line */
        popFillseg(stack, &x1, &x2, &y, &dy);
        line = data + y * wpl;

            /* A segment of scanline y - dy for x1 <= x <= x2 was
             * previously filled.  We now explore adjacent pixels
             * in scan line y.  There are three regions: to the
             * left of x1, between x1 and x2, and to the right of x2.
             * These regions are handled differently.  Leaks are
             * possible expansions beyond the previous segment and
             * going back in the -dy direction.  These can happen
             * for x < x1 and for x > x2.  Any "leak" segments
             * are plugged with a push in the -dy (opposite) direction.
             * And any segments found anywhere are always extended
             * in the +dy direction.  */
        for (x = x1 - 1; x >= 0 && (GET_DATA_BIT(line, x) == 1); x--)
            CLEAR_DATA_BIT(line,x);
        if (x >= x1 - 1)  /* pix at x1 - 1 was off and was not cleared */
            goto skip;
        xstart = x + 1;
        if (xstart < x1)   /* leak on left? */
            pushFillseg(stack, xstart, x1 - 1, y, -dy, ymax);

        x = x1;
        do {
            for (; x <= xmax && (GET_DATA_BIT(line, x) == 1); x++)
                CLEAR_DATA_BIT(line, x);
            pushFillseg(stack, xstart, x - 1, y, dy, ymax);
            if (x > x2)   /* leak on right? */
                pushFillseg(stack, x2 + 1, x - 1, y, -dy, ymax);
    skip:   for (x++; x <= x2 + 1 &&
                      x <= xmax &&
                      (GET_DATA_BIT(line, x) == 0); x++)
                ;
            xstart = x;
        } while (x <= x2 + 1 && x <= xmax);
    }

    return 0;
}



/*-----------------------------------------------------------------------*
 *          Static stack helper functions: push and pop fillsegs         *
 *-----------------------------------------------------------------------*/
/*!
 * \brief   pushFillsegBB()
 *
 * \param[in]    stack
 * \param[in]    xleft, xright
 * \param[in]    y
 * \param[in]    dy
 * \param[in]    ymax
 * \param[out]   pminx            minimum x
 * \param[out]   pmaxx            maximum x
 * \param[out]   pminy            minimum y
 * \param[out]   pmaxy            maximum y
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) This adds a line segment to the stack, and returns its size.
 *      (2) The auxiliary stack is used as a storage area to recycle
 *          fillsegs that are no longer in use.  We only calloc new
 *          fillsegs if the auxiliary stack is empty.
 * </pre>
 */
static void
pushFillsegBB(L_STACK  *stack,
              l_int32   xleft,
              l_int32   xright,
              l_int32   y,
              l_int32   dy,
              l_int32   ymax,
              l_int32  *pminx,
              l_int32  *pmaxx,
              l_int32  *pminy,
              l_int32  *pmaxy)
{
FILLSEG  *fseg;
L_STACK  *auxstack;

    if (!stack) {
        L_ERROR("stack not defined\n", __func__);
        return;
    }

    *pminx = L_MIN(*pminx, xleft);
    *pmaxx = L_MAX(*pmaxx, xright);
    *pminy = L_MIN(*pminy, y);
    *pmaxy = L_MAX(*pmaxy, y);

    if (y + dy >= 0 && y + dy <= ymax) {
        if ((auxstack = stack->auxstack) == NULL) {
            L_ERROR("auxstack not defined\n", __func__);
            return;
        }

            /* Get a fillseg to use */
        if (lstackGetCount(auxstack) > 0)
            fseg = (FILLSEG *)lstackRemove(auxstack);
        else
            fseg = (FILLSEG *)LEPT_CALLOC(1, sizeof(FILLSEG));
        fseg->xleft = xleft;
        fseg->xright = xright;
        fseg->y = y;
        fseg->dy = dy;
        lstackAdd(stack, fseg);
    }
}


/*!
 * \brief   pushFillseg()
 *
 * \param[in]    stack
 * \param[in]    xleft, xright
 * \param[in]    y
 * \param[in]    dy
 * \param[in]    ymax
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) This adds a line segment to the stack.
 *      (2) The auxiliary stack is used as a storage area to recycle
 *          fillsegs that are no longer in use.  We only calloc new
 *          fillsegs if the auxiliary stack is empty.
 * </pre>
 */
static void
pushFillseg(L_STACK  *stack,
            l_int32   xleft,
            l_int32   xright,
            l_int32   y,
            l_int32   dy,
            l_int32   ymax)
{
FILLSEG  *fseg;
L_STACK  *auxstack;

    if (!stack) {
        L_ERROR("stack not defined\n", __func__);
        return;
    }

    if (y + dy >= 0 && y + dy <= ymax) {
        if ((auxstack = stack->auxstack) == NULL) {
            L_ERROR("auxstack not defined\n", __func__);
            return;
        }

            /* Get a fillseg to use */
        if (lstackGetCount(auxstack) > 0)
            fseg = (FILLSEG *)lstackRemove(auxstack);
        else
            fseg = (FILLSEG *)LEPT_CALLOC(1, sizeof(FILLSEG));
        fseg->xleft = xleft;
        fseg->xright = xright;
        fseg->y = y;
        fseg->dy = dy;
        lstackAdd(stack, fseg);
    }
}


/*!
 * \brief   popFillseg()
 *
 * \param[in]    stack
 * \param[out]   pxleft    left x
 * \param[out]   pxright   right x
 * \param[out]   py        y coordinate
 * \param[out]   pdy       delta y
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) This removes a line segment from the stack, and returns its size.
 *      (2) The surplussed fillseg is placed on the auxiliary stack
 *          for future use.
 * </pre>
 */
static void
popFillseg(L_STACK  *stack,
           l_int32  *pxleft,
           l_int32  *pxright,
           l_int32  *py,
           l_int32  *pdy)
{
FILLSEG  *fseg;
L_STACK  *auxstack;

    if (!stack) {
        L_ERROR("stack not defined\n", __func__);
        return;
    }
    if ((auxstack = stack->auxstack) == NULL) {
        L_ERROR("auxstack not defined\n", __func__);
        return;
    }

    if ((fseg = (FILLSEG *)lstackRemove(stack)) == NULL)
        return;

    *pxleft = fseg->xleft;
    *pxright = fseg->xright;
    *py = fseg->y + fseg->dy;  /* this now points to the new line */
    *pdy = fseg->dy;

        /* Save it for re-use */
    lstackAdd(auxstack, fseg);
}

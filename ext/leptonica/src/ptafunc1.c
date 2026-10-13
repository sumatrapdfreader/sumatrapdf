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
 * \file  ptafunc1.c
 * <pre>
 *
 *      --------------------------------------
 *      This file has these Pta utilities:
 *         - simple rearrangements
 *         - geometric analysis
 *         - min/max and filtering
 *         - least squares fitting
 *         - interconversions with Pix and Numa
 *         - display into a pix
 *      --------------------------------------
 *
 *      Simple rearrangements
 *           PTA      *ptaSubsample()
 *           l_int32   ptaJoin()
 *           l_int32   ptaaJoin()
 *           PTA      *ptaReverse()
 *           PTA      *ptaTranspose()
 *           PTA      *ptaCyclicPerm()
 *           PTA      *ptaSelectRange()
 *
 *      Geometric
 *           BOX      *ptaGetBoundingRegion()
 *           l_int32  *ptaGetRange()
 *           PTA      *ptaGetInsideBox()
 *           PTA      *pixFindCornerPixels()
 *           l_int32   ptaContainsPt()
 *           l_int32   ptaTestIntersection()
 *           PTA      *ptaTransform()
 *           l_int32   ptaPtInsidePolygon()
 *           l_float32 l_angleBetweenVectors()
 *           l_int32   ptaPolygonIsConvex()
 *
 *      Min/max and filtering
 *           l_int32   ptaGetMinMax()
 *           PTA      *ptaSelectByValue()
 *           PTA      *ptaCropToMask()
 *
 *      Least Squares Fit
 *           l_int32   ptaGetLinearLSF()
 *           l_int32   ptaGetQuadraticLSF()
 *           l_int32   ptaGetCubicLSF()
 *           l_int32   ptaGetQuarticLSF()
 *           l_int32   ptaNoisyLinearLSF()
 *           l_int32   ptaNoisyQuadraticLSF()
 *           l_int32   applyLinearFit()
 *           l_int32   applyQuadraticFit()
 *           l_int32   applyCubicFit()
 *           l_int32   applyQuarticFit()
 *
 *      Interconversions with Pix
 *           l_int32   pixPlotAlongPta()
 *           PTA      *ptaGetPixelsFromPix()
 *           PIX      *pixGenerateFromPta()
 *           PTA      *ptaGetBoundaryPixels()
 *           PTAA     *ptaaGetBoundaryPixels()
 *           PTAA     *ptaaIndexLabeledPixels()
 *           PTA      *ptaGetNeighborPixLocs()
 *
 *      Interconversion with Numa
 *           PTA      *numaConvertToPta1()
 *           PTA      *numaConvertToPta2()
 *           l_int32   ptaConvertToNuma()
 *
 *      Display Pta and Ptaa
 *           PIX      *pixDisplayPta()
 *           PIX      *pixDisplayPtaaPattern()
 *           PIX      *pixDisplayPtaPattern()
 *           PTA      *ptaReplicatePattern()
 *           PIX      *pixDisplayPtaa()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <math.h>
#include "allheaders.h"
#include "pix_internal.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif  /* M_PI */

/*---------------------------------------------------------------------*
 *                        Simple rearrangements                        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ptaSubsample()
 *
 * \param[in]    ptas
 * \param[in]    subfactor    subsample factor, >= 1
 * \return  ptad evenly sampled pt values from ptas, or NULL on error
 */
PTA *
ptaSubsample(PTA     *ptas,
             l_int32  subfactor)
{
l_int32    n, i;
l_float32  x, y;
PTA       *ptad;

    if (!ptas)
        return (PTA *)ERROR_PTR("ptas not defined", __func__, NULL);
    if (subfactor < 1)
        return (PTA *)ERROR_PTR("subfactor < 1", __func__, NULL);

    ptad = ptaCreate(0);
    n = ptaGetCount(ptas);
    for (i = 0; i < n; i++) {
        if (i % subfactor != 0) continue;
        ptaGetPt(ptas, i, &x, &y);
        ptaAddPt(ptad, x, y);
    }

    return ptad;
}


/*!
 * \brief   ptaJoin()
 *
 * \param[in]    ptad     dest pta; add to this one
 * \param[in]    ptas     source pta; add from this one
 * \param[in]    istart   starting index in ptas
 * \param[in]    iend     ending index in ptas; use -1 to cat all
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) istart < 0 is taken to mean 'read from the start' (istart = 0)
 *      (2) iend < 0 means 'read to the end'
 *      (3) if ptas == NULL, this is a no-op
 * </pre>
 */
l_ok
ptaJoin(PTA     *ptad,
        PTA     *ptas,
        l_int32  istart,
        l_int32  iend)
{
l_int32  n, i, x, y;

    if (!ptad)
        return ERROR_INT("ptad not defined", __func__, 1);
    if (!ptas)
        return 0;

    if (istart < 0)
        istart = 0;
    n = ptaGetCount(ptas);
    if (iend < 0 || iend >= n)
        iend = n - 1;
    if (istart > iend)
        return ERROR_INT("istart > iend; no pts", __func__, 1);

    for (i = istart; i <= iend; i++) {
        ptaGetIPt(ptas, i, &x, &y);
        if (ptaAddPt(ptad, x, y) == 1) {
            L_ERROR("failed to add pt at i = %d\n", __func__, i);
            return 1;
        }
    }
    return 0;
}


/*!
 * \brief   ptaaJoin()
 *
 * \param[in]    ptaad    dest ptaa; add to this one
 * \param[in]    ptaas    source ptaa; add from this one
 * \param[in]    istart   starting index in ptaas
 * \param[in]    iend     ending index in ptaas; use -1 to cat all
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) istart < 0 is taken to mean 'read from the start' (istart = 0)
 *      (2) iend < 0 means 'read to the end'
 *      (3) if ptas == NULL, this is a no-op
 * </pre>
 */
l_ok
ptaaJoin(PTAA    *ptaad,
         PTAA    *ptaas,
         l_int32  istart,
         l_int32  iend)
{
l_int32  n, i;
PTA     *pta;

    if (!ptaad)
        return ERROR_INT("ptaad not defined", __func__, 1);
    if (!ptaas)
        return 0;

    if (istart < 0)
        istart = 0;
    n = ptaaGetCount(ptaas);
    if (iend < 0 || iend >= n)
        iend = n - 1;
    if (istart > iend)
        return ERROR_INT("istart > iend; no pts", __func__, 1);

    for (i = istart; i <= iend; i++) {
        pta = ptaaGetPta(ptaas, i, L_CLONE);
        ptaaAddPta(ptaad, pta, L_INSERT);
    }

    return 0;
}


/*!
 * \brief   ptaReverse()
 *
 * \param[in]    ptas
 * \param[in]    type     0 for float values; 1 for integer values
 * \return  ptad reversed pta, or NULL on error
 */
PTA  *
ptaReverse(PTA     *ptas,
           l_int32  type)
{
l_int32    n, i, ix, iy;
l_float32  x, y;
PTA       *ptad;

    if (!ptas)
        return (PTA *)ERROR_PTR("ptas not defined", __func__, NULL);

    n = ptaGetCount(ptas);
    if ((ptad = ptaCreate(n)) == NULL)
        return (PTA *)ERROR_PTR("ptad not made", __func__, NULL);
    for (i = n - 1; i >= 0; i--) {
        if (type == 0) {
            ptaGetPt(ptas, i, &x, &y);
            ptaAddPt(ptad, x, y);
        } else {  /* type == 1 */
            ptaGetIPt(ptas, i, &ix, &iy);
            ptaAddPt(ptad, ix, iy);
        }
    }

    return ptad;
}


/*!
 * \brief   ptaTranspose()
 *
 * \param[in]    ptas
 * \return  ptad with x and y values swapped, or NULL on error
 */
PTA  *
ptaTranspose(PTA  *ptas)
{
l_int32    n, i;
l_float32  x, y;
PTA       *ptad;

    if (!ptas)
        return (PTA *)ERROR_PTR("ptas not defined", __func__, NULL);

    n = ptaGetCount(ptas);
    if ((ptad = ptaCreate(n)) == NULL)
        return (PTA *)ERROR_PTR("ptad not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        ptaGetPt(ptas, i, &x, &y);
        ptaAddPt(ptad, y, x);
    }

    return ptad;
}


/*!
 * \brief   ptaCyclicPerm()
 *
 * \param[in]    ptas
 * \param[in]    xs, ys     start point; must be in ptas
 * \return  ptad cyclic permutation, starting and ending at (xs, ys,
 *              or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Check to insure that (a) ptas is a closed path where
 *          the first and last points are identical, and (b) the
 *          resulting pta also starts and ends on the same point
 *          (which in this case is (xs, ys).
 * </pre>
 */
PTA  *
ptaCyclicPerm(PTA     *ptas,
              l_int32  xs,
              l_int32  ys)
{
l_int32  n, i, x, y, j, index, state;
l_int32  x1, y1, x2, y2;
PTA     *ptad;

    if (!ptas)
        return (PTA *)ERROR_PTR("ptas not defined", __func__, NULL);

    n = ptaGetCount(ptas);

        /* Verify input data */
    ptaGetIPt(ptas, 0, &x1, &y1);
    ptaGetIPt(ptas, n - 1, &x2, &y2);
    if (x1 != x2 || y1 != y2)
        return (PTA *)ERROR_PTR("start and end pts not same", __func__, NULL);
    state = L_NOT_FOUND;
    for (i = 0; i < n; i++) {
        ptaGetIPt(ptas, i, &x, &y);
        if (x == xs && y == ys) {
            state = L_FOUND;
            break;
        }
    }
    if (state == L_NOT_FOUND)
        return (PTA *)ERROR_PTR("start pt not in ptas", __func__, NULL);

    if ((ptad = ptaCreate(n)) == NULL)
        return (PTA *)ERROR_PTR("ptad not made", __func__, NULL);
    for (j = 0; j < n - 1; j++) {
        if (i + j < n - 1)
            index = i + j;
        else
            index = (i + j + 1) % n;
        ptaGetIPt(ptas, index, &x, &y);
        ptaAddPt(ptad, x, y);
    }
    ptaAddPt(ptad, xs, ys);

    return ptad;
}


/*!
 * \brief   ptaSelectRange()
 *
 * \param[in]    ptas
 * \param[in]    first    use 0 to select from the beginning
 * \param[in]    last     use -1 to select to the end
 * \return  ptad, or NULL on error
 */
PTA *
ptaSelectRange(PTA     *ptas,
               l_int32  first,
               l_int32  last)
{
l_int32    n, npt, i;
l_float32  x, y;
PTA       *ptad;

    if (!ptas)
        return (PTA *)ERROR_PTR("ptas not defined", __func__, NULL);
    if ((n = ptaGetCount(ptas)) == 0) {
        L_WARNING("ptas is empty\n", __func__);
        return ptaCopy(ptas);
    }
    first = L_MAX(0, first);
    if (last < 0) last = n - 1;
    if (first >= n)
        return (PTA *)ERROR_PTR("invalid first", __func__, NULL);
    if (last >= n) {
        L_WARNING("last = %d is beyond max index = %d; adjusting\n",
                  __func__, last, n - 1);
        last = n - 1;
    }
    if (first > last)
        return (PTA *)ERROR_PTR("first > last", __func__, NULL);

    npt = last - first + 1;
    ptad = ptaCreate(npt);
    for (i = first; i <= last; i++) {
        ptaGetPt(ptas, i, &x, &y);
        ptaAddPt(ptad, x, y);
    }
    return ptad;
}


/*---------------------------------------------------------------------*
 *                               Geometric                             *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ptaGetBoundingRegion()
 *
 * \param[in]    pta
 * \return  box, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is used when the pta represents a set of points in
 *          a two-dimensional image.  It returns the box of minimum
 *          size containing the pts in the pta.
 * </pre>
 */
BOX *
ptaGetBoundingRegion(PTA  *pta)
{
l_int32  n, i, x, y, minx, maxx, miny, maxy;

    if (!pta)
        return (BOX *)ERROR_PTR("pta not defined", __func__, NULL);

    minx = 10000000;
    miny = 10000000;
    maxx = -10000000;
    maxy = -10000000;
    n = ptaGetCount(pta);
    for (i = 0; i < n; i++) {
        ptaGetIPt(pta, i, &x, &y);
        if (x < minx) minx = x;
        if (x > maxx) maxx = x;
        if (y < miny) miny = y;
        if (y > maxy) maxy = y;
    }

    return boxCreate(minx, miny, maxx - minx + 1, maxy - miny + 1);
}


/*!
 * \brief   ptaGetRange()
 *
 * \param[in]    pta
 * \param[out]   pminx    [optional] min value of x
 * \param[out]   pmaxx    [optional] max value of x
 * \param[out]   pminy    [optional] min value of y
 * \param[out]   pmaxy    [optional] max value of y
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) We can use pts to represent pairs of floating values, that
 *          are not necessarily tied to a two-dimension region.  For
 *          example, the pts can represent a general function y(x).
 * </pre>
 */
l_ok
ptaGetRange(PTA        *pta,
            l_float32  *pminx,
            l_float32  *pmaxx,
            l_float32  *pminy,
            l_float32  *pmaxy)
{
l_int32    n, i;
l_float32  x, y, minx, maxx, miny, maxy;

    if (!pminx && !pmaxx && !pminy && !pmaxy)
        return ERROR_INT("no output requested", __func__, 1);
    if (pminx) *pminx = 0;
    if (pmaxx) *pmaxx = 0;
    if (pminy) *pminy = 0;
    if (pmaxy) *pmaxy = 0;
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if ((n = ptaGetCount(pta)) == 0)
        return ERROR_INT("no points in pta", __func__, 1);

    ptaGetPt(pta, 0, &x, &y);
    minx = x;
    maxx = x;
    miny = y;
    maxy = y;
    for (i = 1; i < n; i++) {
        ptaGetPt(pta, i, &x, &y);
        if (x < minx) minx = x;
        if (x > maxx) maxx = x;
        if (y < miny) miny = y;
        if (y > maxy) maxy = y;
    }
    if (pminx) *pminx = minx;
    if (pmaxx) *pmaxx = maxx;
    if (pminy) *pminy = miny;
    if (pmaxy) *pmaxy = maxy;
    return 0;
}


/*!
 * \brief   ptaGetInsideBox()
 *
 * \param[in]    ptas    input pts
 * \param[in]    box
 * \return  ptad of pts in ptas that are inside the box, or NULL on error
 */
PTA *
ptaGetInsideBox(PTA  *ptas,
                BOX  *box)
{
PTA       *ptad;
l_int32    n, i, contains;
l_float32  x, y;

    if (!ptas)
        return (PTA *)ERROR_PTR("ptas not defined", __func__, NULL);
    if (!box)
        return (PTA *)ERROR_PTR("box not defined", __func__, NULL);

    n = ptaGetCount(ptas);
    ptad = ptaCreate(0);
    for (i = 0; i < n; i++) {
        ptaGetPt(ptas, i, &x, &y);
        boxContainsPt(box, x, y, &contains);
        if (contains)
            ptaAddPt(ptad, x, y);
    }

    return ptad;
}


/*!
 * \brief   pixFindCornerPixels()
 *
 * \param[in]    pixs    1 bpp
 * \return  pta, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Finds the 4 corner-most pixels, as defined by a search
 *          inward from each corner, using a 45 degree line.
 * </pre>
 */
PTA *
pixFindCornerPixels(PIX  *pixs)
{
l_int32    i, j, x, y, w, h, wpl, mindim, found;
l_uint32  *data, *line;
PTA       *pta;

    if (!pixs)
        return (PTA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PTA *)ERROR_PTR("pixs not 1 bpp", __func__, NULL);

    w = pixGetWidth(pixs);
    h = pixGetHeight(pixs);
    mindim = L_MIN(w, h);
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);

    if ((pta = ptaCreate(4)) == NULL)
        return (PTA *)ERROR_PTR("pta not made", __func__, NULL);

    for (found = FALSE, i = 0; i < mindim; i++) {
        for (j = 0; j <= i; j++) {
            y = i - j;
            line = data + y * wpl;
            if (GET_DATA_BIT(line, j)) {
                ptaAddPt(pta, j, y);
                found = TRUE;
                break;
            }
        }
        if (found == TRUE)
            break;
    }

    for (found = FALSE, i = 0; i < mindim; i++) {
        for (j = 0; j <= i; j++) {
            y = i - j;
            line = data + y * wpl;
            x = w - 1 - j;
            if (GET_DATA_BIT(line, x)) {
                ptaAddPt(pta, x, y);
                found = TRUE;
                break;
            }
        }
        if (found == TRUE)
            break;
    }

    for (found = FALSE, i = 0; i < mindim; i++) {
        for (j = 0; j <= i; j++) {
            y = h - 1 - i + j;
            line = data + y * wpl;
            if (GET_DATA_BIT(line, j)) {
                ptaAddPt(pta, j, y);
                found = TRUE;
                break;
            }
        }
        if (found == TRUE)
            break;
    }

    for (found = FALSE, i = 0; i < mindim; i++) {
        for (j = 0; j <= i; j++) {
            y = h - 1 - i + j;
            line = data + y * wpl;
            x = w - 1 - j;
            if (GET_DATA_BIT(line, x)) {
                ptaAddPt(pta, x, y);
                found = TRUE;
                break;
            }
        }
        if (found == TRUE)
            break;
    }

    return pta;
}


/*!
 * \brief   ptaContainsPt()
 *
 * \param[in]    pta
 * \param[in]    x, y     point
 * \return  1 if contained, 0 otherwise or on error
 */
l_int32
ptaContainsPt(PTA     *pta,
              l_int32  x,
              l_int32  y)
{
l_int32  i, n, ix, iy;

    if (!pta)
        return ERROR_INT("pta not defined", __func__, 0);

    n = ptaGetCount(pta);
    for (i = 0; i < n; i++) {
        ptaGetIPt(pta, i, &ix, &iy);
        if (x == ix && y == iy)
            return 1;
    }
    return 0;
}


/*!
 * \brief   ptaTestIntersection()
 *
 * \param[in]    pta1, pta2
 * \return  bval which is 1 if they have any elements in common;
 *              0 otherwise or on error.
 */
l_int32
ptaTestIntersection(PTA  *pta1,
                    PTA  *pta2)
{
l_int32  i, j, n1, n2, x1, y1, x2, y2;

    if (!pta1)
        return ERROR_INT("pta1 not defined", __func__, 0);
    if (!pta2)
        return ERROR_INT("pta2 not defined", __func__, 0);

    n1 = ptaGetCount(pta1);
    n2 = ptaGetCount(pta2);
    for (i = 0; i < n1; i++) {
        ptaGetIPt(pta1, i, &x1, &y1);
        for (j = 0; j < n2; j++) {
            ptaGetIPt(pta2, i, &x2, &y2);
            if (x1 == x2 && y1 == y2)
                return 1;
        }
    }

    return 0;
}


/*!
 * \brief   ptaTransform()
 *
 * \param[in]    ptas
 * \param[in]    shiftx, shifty
 * \param[in]    scalex, scaley
 * \return  pta, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Shift first, then scale.
 * </pre>
 */
PTA *
ptaTransform(PTA       *ptas,
             l_int32    shiftx,
             l_int32    shifty,
             l_float32  scalex,
             l_float32  scaley)
{
l_int32  n, i, x, y;
PTA     *ptad;

    if (!ptas)
        return (PTA *)ERROR_PTR("ptas not defined", __func__, NULL);
    n = ptaGetCount(ptas);
    ptad = ptaCreate(n);
    for (i = 0; i < n; i++) {
        ptaGetIPt(ptas, i, &x, &y);
        x = (l_int32)(scalex * (x + shiftx) + 0.5);
        y = (l_int32)(scaley * (y + shifty) + 0.5);
        ptaAddPt(ptad, x, y);
    }

    return ptad;
}


/*!
 * \brief   ptaPtInsidePolygon()
 *
 * \param[in]    pta       vertices of a polygon
 * \param[in]    x, y      point to be tested
 * \param[out]   pinside   1 if inside; 0 if outside or on boundary
 * \return  1 if OK, 0 on error
 *
 *  The abs value of the sum of the angles subtended from a point by
 *  the sides of a polygon, when taken in order traversing the polygon,
 *  is 0 if the point is outside the polygon and 2*pi if inside.
 *  The sign will be positive if traversed cw and negative if ccw.
 */
l_int32
ptaPtInsidePolygon(PTA       *pta,
                   l_float32  x,
                   l_float32  y,
                   l_int32   *pinside)
{
l_int32    i, n;
l_float32  sum, x1, y1, x2, y2, xp1, yp1, xp2, yp2;

    if (!pinside)
        return ERROR_INT("&inside not defined", __func__, 1);
    *pinside = 0;
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);

        /* Think of (x1,y1) as the end point of a vector that starts
         * from the origin (0,0), and ditto for (x2,y2). */
    n = ptaGetCount(pta);
    sum = 0.0;
    for (i = 0; i < n; i++) {
        ptaGetPt(pta, i, &xp1, &yp1);
        ptaGetPt(pta, (i + 1) % n, &xp2, &yp2);
        x1 = xp1 - x;
        y1 = yp1 - y;
        x2 = xp2 - x;
        y2 = yp2 - y;
        sum += l_angleBetweenVectors(x1, y1, x2, y2);
    }

    if (L_ABS(sum) > M_PI)
        *pinside = 1;
    return 0;
}


/*!
 * \brief   l_angleBetweenVectors()
 *
 * \param[in]    x1, y1     end point of first vector
 * \param[in]    x2, y2     end point of second vector
 * \return  angle radians, or 0.0 on error
 *
 * <pre>
 * Notes:
 *      (1) This gives the angle between two vectors, going between
 *          vector1 (x1,y1) and vector2 (x2,y2).  The angle is swept
 *          out from 1 --> 2.  If this is clockwise, the angle is
 *          positive, but the result is folded into the interval [-pi, pi].
 * </pre>
 */
l_float32
l_angleBetweenVectors(l_float32  x1,
                      l_float32  y1,
                      l_float32  x2,
                      l_float32  y2)
{
l_float64  ang;

    ang = atan2(y2, x2) - atan2(y1, x1);
    if (ang > M_PI) ang -= 2.0 * M_PI;
    if (ang < -M_PI) ang += 2.0 * M_PI;
    return ang;
}


/*!
 * \brief   ptaPolygonIsConvex()
 *
 * \param[in]     pta      corners of polygon
 * \param[out]    pisconvex   1 if convex; 0 otherwise
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) A Pta of size n describes a polygon with n sides, where
 *          the n-th side goes from point[n - 1] to point[0].
 *      (2) The pta must describe a CLOCKWISE traversal of the boundary
 *          of the polygon.
 *      (3) Algorithm: traversing the boundary in a cw direction, the
 *          polygon interior is always on the right.  If the polygon is
 *          convex, for each set of 3 points, the third point is either
 *          on the ray extending from the first point and going through
 *          the second point, or to the right of it.
 * </pre>
 */
l_int32
ptaPolygonIsConvex(PTA      *pta,
                   l_int32  *pisconvex)
{
l_int32    i, n;
l_float32  x0, y0, x1, y1, x2, y2;
l_float64  cprod;

    if (!pisconvex)
        return ERROR_INT("&isconvex not defined", __func__, 1);
    *pisconvex = 0;
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if ((n = ptaGetCount(pta)) < 3)
        return ERROR_INT("pta has < 3 pts", __func__, 1);

    for (i = 0; i < n; i++) {
        ptaGetPt(pta, i, &x0, &y0);
        ptaGetPt(pta, (i + 1) % n, &x1, &y1);
        ptaGetPt(pta, (i + 2) % n, &x2, &y2);
            /* The vector v02 from p0 to p2 must be to the right of the
               vector v01 from p0 to p1.  This is true if the cross
               product v02 x v01 > 0.  In coordinates:
                   v02x * v01y - v01x * v02y, where
                   v01x = x1 - x0, v01y = y1 - y0,
                   v02x = x2 - x0, v02y = y2 - y0   */
        cprod = (x2 - x0) * (y1 - y0) - (x1 - x0) * (y2 - y0);
        if (cprod < -0.0001)  /* small delta for float accuracy; test fails */
            return 0;
    }
    *pisconvex = 1;
    return 0;
}


/*---------------------------------------------------------------------*
 *                       Min/max and filtering                         *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ptaGetMinMax()
 *
 * \param[in]    pta
 * \param[out]   pxmin   [optional] min of x
 * \param[out]   pymin   [optional] min of y
 * \param[out]   pxmax   [optional] max of x
 * \param[out]   pymax   [optional] max of y
 * \return  0 if OK, 1 on error.  If pta is empty, requested
 *              values are returned as -1.0.
 */
l_ok
ptaGetMinMax(PTA        *pta,
             l_float32  *pxmin,
             l_float32  *pymin,
             l_float32  *pxmax,
             l_float32  *pymax)
{
l_int32    i, n;
l_float32  x, y, xmin, ymin, xmax, ymax;

    if (pxmin) *pxmin = -1.0;
    if (pymin) *pymin = -1.0;
    if (pxmax) *pxmax = -1.0;
    if (pymax) *pymax = -1.0;
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if (!pxmin && !pxmax && !pymin && !pymax)
        return ERROR_INT("no output requested", __func__, 1);
    if ((n = ptaGetCount(pta)) == 0) {
        L_WARNING("pta is empty\n", __func__);
        return 0;
    }

    xmin = ymin = 1.0e20f;
    xmax = ymax = -1.0e20f;
    for (i = 0; i < n; i++) {
        ptaGetPt(pta, i, &x, &y);
        if (x < xmin) xmin = x;
        if (y < ymin) ymin = y;
        if (x > xmax) xmax = x;
        if (y > ymax) ymax = y;
    }
    if (pxmin) *pxmin = xmin;
    if (pymin) *pymin = ymin;
    if (pxmax) *pxmax = xmax;
    if (pymax) *pymax = ymax;
    return 0;
}


/*!
 * \brief   ptaSelectByValue()
 *
 * \param[in]    ptas
 * \param[in]    xth, yth    threshold values
 * \param[in]    type        L_SELECT_XVAL, L_SELECT_YVAL,
 *                           L_SELECT_IF_EITHER, L_SELECT_IF_BOTH
 * \param[in]    relation    L_SELECT_IF_LT, L_SELECT_IF_GT,
 *                           L_SELECT_IF_LTE, L_SELECT_IF_GTE
 * \return  ptad filtered set, or NULL on error
 */
PTA *
ptaSelectByValue(PTA       *ptas,
                 l_float32  xth,
                 l_float32  yth,
                 l_int32    type,
                 l_int32    relation)
{
l_int32    i, n;
l_float32  x, y;
PTA       *ptad;

    if (!ptas)
        return (PTA *)ERROR_PTR("ptas not defined", __func__, NULL);
    if (ptaGetCount(ptas) == 0) {
        L_WARNING("ptas is empty\n", __func__);
        return ptaCopy(ptas);
    }
    if (type != L_SELECT_XVAL && type != L_SELECT_YVAL &&
        type != L_SELECT_IF_EITHER && type != L_SELECT_IF_BOTH)
        return (PTA *)ERROR_PTR("invalid type", __func__, NULL);
    if (relation != L_SELECT_IF_LT && relation != L_SELECT_IF_GT &&
        relation != L_SELECT_IF_LTE && relation != L_SELECT_IF_GTE)
        return (PTA *)ERROR_PTR("invalid relation", __func__, NULL);

    n = ptaGetCount(ptas);
    ptad = ptaCreate(n);
    for (i = 0; i < n; i++) {
        ptaGetPt(ptas, i, &x, &y);
        if (type == L_SELECT_XVAL) {
            if ((relation == L_SELECT_IF_LT && x < xth) ||
                (relation == L_SELECT_IF_GT && x > xth) ||
                (relation == L_SELECT_IF_LTE && x <= xth) ||
                (relation == L_SELECT_IF_GTE && x >= xth))
                ptaAddPt(ptad, x, y);
        } else if (type == L_SELECT_YVAL) {
            if ((relation == L_SELECT_IF_LT && y < yth) ||
                (relation == L_SELECT_IF_GT && y > yth) ||
                (relation == L_SELECT_IF_LTE && y <= yth) ||
                (relation == L_SELECT_IF_GTE && y >= yth))
                ptaAddPt(ptad, x, y);
        } else if (type == L_SELECT_IF_EITHER) {
            if (((relation == L_SELECT_IF_LT) && (x < xth || y < yth)) ||
                ((relation == L_SELECT_IF_GT) && (x > xth || y > yth)) ||
                ((relation == L_SELECT_IF_LTE) && (x <= xth || y <= yth)) ||
                ((relation == L_SELECT_IF_GTE) && (x >= xth || y >= yth)))
                ptaAddPt(ptad, x, y);
        } else {  /* L_SELECT_IF_BOTH */
            if (((relation == L_SELECT_IF_LT) && (x < xth && y < yth)) ||
                ((relation == L_SELECT_IF_GT) && (x > xth && y > yth)) ||
                ((relation == L_SELECT_IF_LTE) && (x <= xth && y <= yth)) ||
                ((relation == L_SELECT_IF_GTE) && (x >= xth && y >= yth)))
                ptaAddPt(ptad, x, y);
        }
    }

    return ptad;
}


/*!
 * \brief   ptaCropToMask()
 *
 * \param[in]    ptas    input pta
 * \param[in]    pixm    1 bpp mask
 * \return  ptad  with only pts under the mask fg, or NULL on error
 */
PTA *
ptaCropToMask(PTA  *ptas,
              PIX  *pixm)
{
l_int32   i, n, x, y;
l_uint32  val;
PTA      *ptad;

    if (!ptas)
        return (PTA *)ERROR_PTR("ptas not defined", __func__, NULL);
    if (!pixm || pixGetDepth(pixm) != 1)
        return (PTA *)ERROR_PTR("pixm undefined or not 1 bpp", __func__, NULL);
    if (ptaGetCount(ptas) == 0) {
        L_INFO("ptas is empty\n", __func__);
        return ptaCopy(ptas);
    }

    n = ptaGetCount(ptas);
    ptad = ptaCreate(n);
    for (i = 0; i < n; i++) {
        ptaGetIPt(ptas, i, &x, &y);
        pixGetPixel(pixm, x, y, &val);
        if (val == 1)
            ptaAddPt(ptad, x, y);
    }
    return ptad;
}


/*---------------------------------------------------------------------*
 *                            Least Squares Fit                        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ptaGetLinearLSF()
 *
 * \param[in]    pta
 * \param[out]   pa      [optional] slope a of least square fit: y = ax + b
 * \param[out]   pb      [optional] intercept b of least square fit
 * \param[out]   pnafit  [optional] numa of least square fit
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Either or both &a and &b must be input.  They determine the
 *          type of line that is fit.
 *      (2) If both &a and &b are defined, this returns a and b that minimize:
 *
 *              sum (yi - axi -b)^2
 *               i
 *
 *          The method is simple: differentiate this expression w/rt a and b,
 *          and solve the resulting two equations for a and b in terms of
 *          various sums over the input data (xi, yi).
 *      (3) We also allow two special cases, where either a = 0 or b = 0:
 *           (a) If &a is given and &b = null, find the linear LSF that
 *               goes through the origin (b = 0).
 *           (b) If &b is given and &a = null, find the linear LSF with
 *               zero slope (a = 0).
 *      (4) If &nafit is defined, this returns an array of fitted values,
 *          corresponding to the two implicit Numa arrays (nax and nay) in pta.
 *          Thus, just as you can plot the data in pta as nay vs. nax,
 *          you can plot the linear least square fit as nafit vs. nax.
 *          Get the nax array using ptaGetArrays(pta, &nax, NULL);
 * </pre>
 */
l_ok
ptaGetLinearLSF(PTA        *pta,
                l_float32  *pa,
                l_float32  *pb,
                NUMA      **pnafit)
{
l_int32     n, i;
l_float32   a, b, factor, sx, sy, sxx, sxy, val;
l_float32  *xa, *ya;

    if (pa) *pa = 0.0;
    if (pb) *pb = 0.0;
    if (pnafit) *pnafit = NULL;
    if (!pa && !pb && !pnafit)
        return ERROR_INT("no output requested", __func__, 1);
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if ((n = ptaGetCount(pta)) < 2)
        return ERROR_INT("less than 2 pts found", __func__, 1);

    xa = pta->x;  /* not a copy */
    ya = pta->y;  /* not a copy */
    sx = sy = sxx = sxy = 0.;
    if (pa && pb) {  /* general line */
        for (i = 0; i < n; i++) {
            sx += xa[i];
            sy += ya[i];
            sxx += xa[i] * xa[i];
            sxy += xa[i] * ya[i];
        }
        factor = n * sxx - sx * sx;
        if (factor == 0.0)
            return ERROR_INT("no solution found", __func__, 1);
        factor = 1.f / factor;

        a = factor * ((l_float32)n * sxy - sx * sy);
        b = factor * (sxx * sy - sx * sxy);
    } else if (pa) {  /* b = 0; line through origin */
        for (i = 0; i < n; i++) {
            sxx += xa[i] * xa[i];
            sxy += xa[i] * ya[i];
        }
        if (sxx == 0.0)
            return ERROR_INT("no solution found", __func__, 1);
        a = sxy / sxx;
        b = 0.0;
    } else {  /* a = 0; horizontal line */
        for (i = 0; i < n; i++)
            sy += ya[i];
        a = 0.0;
        b = sy / (l_float32)n;
    }

    if (pnafit) {
        *pnafit = numaCreate(n);
        for (i = 0; i < n; i++) {
            val = a * xa[i] + b;
            numaAddNumber(*pnafit, val);
        }
    }

    if (pa) *pa = a;
    if (pb) *pb = b;
    return 0;
}


/*!
 * \brief   ptaGetQuadraticLSF()
 *
 * \param[in]    pta
 * \param[out]   pa      [optional] coeff a of LSF: y = ax^2 + bx + c
 * \param[out]   pb      [optional] coeff b of LSF: y = ax^2 + bx + c
 * \param[out]   pc      [optional] coeff c of LSF: y = ax^2 + bx + c
 * \param[out]   pnafit  [optional] numa of least square fit
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This does a quadratic least square fit to the set of points
 *          in %pta.  That is, it finds coefficients a, b and c that minimize:
 *
 *              sum (yi - a*xi*xi -b*xi -c)^2
 *               i
 *
 *          The method is simple: differentiate this expression w/rt
 *          a, b and c, and solve the resulting three equations for these
 *          coefficients in terms of various sums over the input data (xi, yi).
 *          The three equations are in the form:
 *             f[0][0]a + f[0][1]b + f[0][2]c = g[0]
 *             f[1][0]a + f[1][1]b + f[1][2]c = g[1]
 *             f[2][0]a + f[2][1]b + f[2][2]c = g[2]
 *      (2) If &nafit is defined, this returns an array of fitted values,
 *          corresponding to the two implicit Numa arrays (nax and nay) in pta.
 *          Thus, just as you can plot the data in pta as nay vs. nax,
 *          you can plot the linear least square fit as nafit vs. nax.
 *          Get the nax array using ptaGetArrays(pta, &nax, NULL);
 * </pre>
 */
l_ok
ptaGetQuadraticLSF(PTA        *pta,
                   l_float32  *pa,
                   l_float32  *pb,
                   l_float32  *pc,
                   NUMA      **pnafit)
{
l_int32     n, i, ret;
l_float32   x, y, sx, sy, sx2, sx3, sx4, sxy, sx2y;
l_float32  *xa, *ya;
l_float32  *f[3];
l_float32   g[3];

    if (pa) *pa = 0.0;
    if (pb) *pb = 0.0;
    if (pc) *pc = 0.0;
    if (pnafit) *pnafit = NULL;
    if (!pa && !pb && !pc && !pnafit)
        return ERROR_INT("no output requested", __func__, 1);
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if ((n = ptaGetCount(pta)) < 3)
        return ERROR_INT("less than 3 pts found", __func__, 1);

    xa = pta->x;  /* not a copy */
    ya = pta->y;  /* not a copy */
    sx = sy = sx2 = sx3 = sx4 = sxy = sx2y = 0.;
    for (i = 0; i < n; i++) {
        x = xa[i];
        y = ya[i];
        sx += x;
        sy += y;
        sx2 += x * x;
        sx3 += x * x * x;
        sx4 += x * x * x * x;
        sxy += x * y;
        sx2y += x * x * y;
    }

    for (i = 0; i < 3; i++)
        f[i] = (l_float32 *)LEPT_CALLOC(3, sizeof(l_float32));
    f[0][0] = sx4;
    f[0][1] = sx3;
    f[0][2] = sx2;
    f[1][0] = sx3;
    f[1][1] = sx2;
    f[1][2] = sx;
    f[2][0] = sx2;
    f[2][1] = sx;
    f[2][2] = n;
    g[0] = sx2y;
    g[1] = sxy;
    g[2] = sy;

        /* Solve for the unknowns, also putting f-inverse into f */
    ret = gaussjordan(f, g, 3);
    for (i = 0; i < 3; i++)
        LEPT_FREE(f[i]);
    if (ret)
        return ERROR_INT("quadratic solution failed", __func__, 1);

    if (pa) *pa = g[0];
    if (pb) *pb = g[1];
    if (pc) *pc = g[2];
    if (pnafit) {
        *pnafit = numaCreate(n);
        for (i = 0; i < n; i++) {
            x = xa[i];
            y = g[0] * x * x + g[1] * x + g[2];
            numaAddNumber(*pnafit, y);
        }
    }
    return 0;
}


/*!
 * \brief   ptaGetCubicLSF()
 *
 * \param[in]    pta
 * \param[out]   pa      [optional] coeff a of LSF: y = ax^3 + bx^2 + cx + d
 * \param[out]   pb      [optional] coeff b of LSF
 * \param[out]   pc      [optional] coeff c of LSF
 * \param[out]   pd      [optional] coeff d of LSF
 * \param[out]   pnafit  [optional] numa of least square fit
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This does a cubic least square fit to the set of points
 *          in %pta.  That is, it finds coefficients a, b, c and d
 *          that minimize:
 *
 *              sum (yi - a*xi*xi*xi -b*xi*xi -c*xi - d)^2
 *               i
 *
 *          Differentiate this expression w/rt a, b, c and d, and solve
 *          the resulting four equations for these coefficients in
 *          terms of various sums over the input data (xi, yi).
 *          The four equations are in the form:
 *             f[0][0]a + f[0][1]b + f[0][2]c + f[0][3] = g[0]
 *             f[1][0]a + f[1][1]b + f[1][2]c + f[1][3] = g[1]
 *             f[2][0]a + f[2][1]b + f[2][2]c + f[2][3] = g[2]
 *             f[3][0]a + f[3][1]b + f[3][2]c + f[3][3] = g[3]
 *      (2) If &nafit is defined, this returns an array of fitted values,
 *          corresponding to the two implicit Numa arrays (nax and nay) in pta.
 *          Thus, just as you can plot the data in pta as nay vs. nax,
 *          you can plot the linear least square fit as nafit vs. nax.
 *          Get the nax array using ptaGetArrays(pta, &nax, NULL);
 * </pre>
 */
l_ok
ptaGetCubicLSF(PTA        *pta,
               l_float32  *pa,
               l_float32  *pb,
               l_float32  *pc,
               l_float32  *pd,
               NUMA      **pnafit)
{
l_int32     n, i, ret;
l_float32   x, y, sx, sy, sx2, sx3, sx4, sx5, sx6, sxy, sx2y, sx3y;
l_float32  *xa, *ya;
l_float32  *f[4];
l_float32   g[4];

    if (pa) *pa = 0.0;
    if (pb) *pb = 0.0;
    if (pc) *pc = 0.0;
    if (pd) *pd = 0.0;
    if (pnafit) *pnafit = NULL;
    if (!pa && !pb && !pc && !pd && !pnafit)
        return ERROR_INT("no output requested", __func__, 1);
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if ((n = ptaGetCount(pta)) < 4)
        return ERROR_INT("less than 4 pts found", __func__, 1);

    xa = pta->x;  /* not a copy */
    ya = pta->y;  /* not a copy */
    sx = sy = sx2 = sx3 = sx4 = sx5 = sx6 = sxy = sx2y = sx3y = 0.;
    for (i = 0; i < n; i++) {
        x = xa[i];
        y = ya[i];
        sx += x;
        sy += y;
        sx2 += x * x;
        sx3 += x * x * x;
        sx4 += x * x * x * x;
        sx5 += x * x * x * x * x;
        sx6 += x * x * x * x * x * x;
        sxy += x * y;
        sx2y += x * x * y;
        sx3y += x * x * x * y;
    }

    for (i = 0; i < 4; i++)
        f[i] = (l_float32 *)LEPT_CALLOC(4, sizeof(l_float32));
    f[0][0] = sx6;
    f[0][1] = sx5;
    f[0][2] = sx4;
    f[0][3] = sx3;
    f[1][0] = sx5;
    f[1][1] = sx4;
    f[1][2] = sx3;
    f[1][3] = sx2;
    f[2][0] = sx4;
    f[2][1] = sx3;
    f[2][2] = sx2;
    f[2][3] = sx;
    f[3][0] = sx3;
    f[3][1] = sx2;
    f[3][2] = sx;
    f[3][3] = n;
    g[0] = sx3y;
    g[1] = sx2y;
    g[2] = sxy;
    g[3] = sy;

        /* Solve for the unknowns, also putting f-inverse into f */
    ret = gaussjordan(f, g, 4);
    for (i = 0; i < 4; i++)
        LEPT_FREE(f[i]);
    if (ret)
        return ERROR_INT("cubic solution failed", __func__, 1);

    if (pa) *pa = g[0];
    if (pb) *pb = g[1];
    if (pc) *pc = g[2];
    if (pd) *pd = g[3];
    if (pnafit) {
        *pnafit = numaCreate(n);
        for (i = 0; i < n; i++) {
            x = xa[i];
            y = g[0] * x * x * x + g[1] * x * x + g[2] * x + g[3];
            numaAddNumber(*pnafit, y);
        }
    }
    return 0;
}


/*!
 * \brief   ptaGetQuarticLSF()
 *
 * \param[in]    pta
 * \param[out]   pa      [optional] coeff a of LSF:
 *                            y = ax^4 + bx^3 + cx^2 + dx + e
 * \param[out]   pb      [optional] coeff b of LSF
 * \param[out]   pc      [optional] coeff c of LSF
 * \param[out]   pd      [optional] coeff d of LSF
 * \param[out]   pe      [optional] coeff e of LSF
 * \param[out]   pnafit  [optional] numa of least square fit
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This does a quartic least square fit to the set of points
 *          in %pta.  That is, it finds coefficients a, b, c, d and 3
 *          that minimize:
 *
 *              sum (yi - a*xi*xi*xi*xi -b*xi*xi*xi -c*xi*xi - d*xi - e)^2
 *               i
 *
 *          Differentiate this expression w/rt a, b, c, d and e, and solve
 *          the resulting five equations for these coefficients in
 *          terms of various sums over the input data (xi, yi).
 *          The five equations are in the form:
 *             f[0][0]a + f[0][1]b + f[0][2]c + f[0][3] + f[0][4] = g[0]
 *             f[1][0]a + f[1][1]b + f[1][2]c + f[1][3] + f[1][4] = g[1]
 *             f[2][0]a + f[2][1]b + f[2][2]c + f[2][3] + f[2][4] = g[2]
 *             f[3][0]a + f[3][1]b + f[3][2]c + f[3][3] + f[3][4] = g[3]
 *             f[4][0]a + f[4][1]b + f[4][2]c + f[4][3] + f[4][4] = g[4]
 *      (2) If &nafit is defined, this returns an array of fitted values,
 *          corresponding to the two implicit Numa arrays (nax and nay) in pta.
 *          Thus, just as you can plot the data in pta as nay vs. nax,
 *          you can plot the linear least square fit as nafit vs. nax.
 *          Get the nax array using ptaGetArrays(pta, &nax, NULL);
 * </pre>
 */
l_ok
ptaGetQuarticLSF(PTA        *pta,
                 l_float32  *pa,
                 l_float32  *pb,
                 l_float32  *pc,
                 l_float32  *pd,
                 l_float32  *pe,
                 NUMA      **pnafit)
{
l_int32     n, i, ret;
l_float32   x, y, sx, sy, sx2, sx3, sx4, sx5, sx6, sx7, sx8;
l_float32   sxy, sx2y, sx3y, sx4y;
l_float32  *xa, *ya;
l_float32  *f[5];
l_float32   g[5];

    if (pa) *pa = 0.0;
    if (pb) *pb = 0.0;
    if (pc) *pc = 0.0;
    if (pd) *pd = 0.0;
    if (pe) *pe = 0.0;
    if (pnafit) *pnafit = NULL;
    if (!pa && !pb && !pc && !pd && !pe && !pnafit)
        return ERROR_INT("no output requested", __func__, 1);
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if ((n = ptaGetCount(pta)) < 5)
        return ERROR_INT("less than 5 pts found", __func__, 1);

    xa = pta->x;  /* not a copy */
    ya = pta->y;  /* not a copy */
    sx = sy = sx2 = sx3 = sx4 = sx5 = sx6 = sx7 = sx8 = 0;
    sxy = sx2y = sx3y = sx4y = 0.;
    for (i = 0; i < n; i++) {
        x = xa[i];
        y = ya[i];
        sx += x;
        sy += y;
        sx2 += x * x;
        sx3 += x * x * x;
        sx4 += x * x * x * x;
        sx5 += x * x * x * x * x;
        sx6 += x * x * x * x * x * x;
        sx7 += x * x * x * x * x * x * x;
        sx8 += x * x * x * x * x * x * x * x;
        sxy += x * y;
        sx2y += x * x * y;
        sx3y += x * x * x * y;
        sx4y += x * x * x * x * y;
    }

    for (i = 0; i < 5; i++)
        f[i] = (l_float32 *)LEPT_CALLOC(5, sizeof(l_float32));
    f[0][0] = sx8;
    f[0][1] = sx7;
    f[0][2] = sx6;
    f[0][3] = sx5;
    f[0][4] = sx4;
    f[1][0] = sx7;
    f[1][1] = sx6;
    f[1][2] = sx5;
    f[1][3] = sx4;
    f[1][4] = sx3;
    f[2][0] = sx6;
    f[2][1] = sx5;
    f[2][2] = sx4;
    f[2][3] = sx3;
    f[2][4] = sx2;
    f[3][0] = sx5;
    f[3][1] = sx4;
    f[3][2] = sx3;
    f[3][3] = sx2;
    f[3][4] = sx;
    f[4][0] = sx4;
    f[4][1] = sx3;
    f[4][2] = sx2;
    f[4][3] = sx;
    f[4][4] = n;
    g[0] = sx4y;
    g[1] = sx3y;
    g[2] = sx2y;
    g[3] = sxy;
    g[4] = sy;

        /* Solve for the unknowns, also putting f-inverse into f */
    ret = gaussjordan(f, g, 5);
    for (i = 0; i < 5; i++)
        LEPT_FREE(f[i]);
    if (ret)
        return ERROR_INT("quartic solution failed", __func__, 1);

    if (pa) *pa = g[0];
    if (pb) *pb = g[1];
    if (pc) *pc = g[2];
    if (pd) *pd = g[3];
    if (pe) *pe = g[4];
    if (pnafit) {
        *pnafit = numaCreate(n);
        for (i = 0; i < n; i++) {
            x = xa[i];
            y = g[0] * x * x * x * x + g[1] * x * x * x + g[2] * x * x
                 + g[3] * x + g[4];
            numaAddNumber(*pnafit, y);
        }
    }
    return 0;
}


/*!
 * \brief   ptaNoisyLinearLSF()
 *
 * \param[in]    pta
 * \param[in]    factor    reject outliers with error greater than this
 *                         number of medians; typically ~ 3
 * \param[out]   pptad     [optional] with outliers removed
 * \param[out]   pa        [optional] slope a of least square fit: y = ax + b
 * \param[out]   pb        [optional] intercept b of least square fit
 * \param[out]   pmederr   [optional] median error
 * \param[out]   pnafit    [optional] numa of least square fit to ptad
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This does a linear least square fit to the set of points
 *          in %pta.  It then evaluates the errors and removes points
 *          whose error is >= factor * median_error.  It then re-runs
 *          the linear LSF on the resulting points.
 *      (2) Either or both &a and &b must be input.  They determine the
 *          type of line that is fit.
 *      (3) The median error can give an indication of how good the fit
 *          is likely to be.
 * </pre>
 */
l_ok
ptaNoisyLinearLSF(PTA        *pta,
                  l_float32   factor,
                  PTA       **pptad,
                  l_float32  *pa,
                  l_float32  *pb,
                  l_float32  *pmederr,
                  NUMA      **pnafit)
{
l_int32    n, i, ret;
l_float32  x, y, yf, val, mederr;
NUMA      *nafit, *naerror;
PTA       *ptad;

    if (pptad) *pptad = NULL;
    if (pa) *pa = 0.0;
    if (pb) *pb = 0.0;
    if (pmederr) *pmederr = 0.0;
    if (pnafit) *pnafit = NULL;
    if (!pptad && !pa && !pb && !pnafit)
        return ERROR_INT("no output requested", __func__, 1);
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if (factor <= 0.0)
        return ERROR_INT("factor must be > 0.0", __func__, 1);
    if ((n = ptaGetCount(pta)) < 3)
        return ERROR_INT("less than 2 pts found", __func__, 1);

    if (ptaGetLinearLSF(pta, pa, pb, &nafit) != 0)
        return ERROR_INT("error in linear LSF", __func__, 1);

        /* Get the median error */
    naerror = numaCreate(n);
    for (i = 0; i < n; i++) {
        ptaGetPt(pta, i, &x, &y);
        numaGetFValue(nafit, i, &yf);
        numaAddNumber(naerror, L_ABS(y - yf));
    }
    numaGetMedian(naerror, &mederr);
    if (pmederr) *pmederr = mederr;
    numaDestroy(&nafit);

        /* Remove outliers */
    ptad = ptaCreate(n);
    for (i = 0; i < n; i++) {
        ptaGetPt(pta, i, &x, &y);
        numaGetFValue(naerror, i, &val);
        if (val <= factor * mederr)  /* <= in case mederr = 0 */
            ptaAddPt(ptad, x, y);
    }
    numaDestroy(&naerror);

       /* Do LSF again */
    ret = ptaGetLinearLSF(ptad, pa, pb, pnafit);
    if (pptad)
        *pptad = ptad;
    else
        ptaDestroy(&ptad);

    return ret;
}


/*!
 * \brief   ptaNoisyQuadraticLSF()
 *
 * \param[in]    pta
 * \param[in]    factor    reject outliers with error greater than this
 *                         number of medians; typically ~ 3
 * \param[out]   pptad     [optional] with outliers removed
 * \param[out]   pa        [optional] coeff a of LSF: y = ax^2 + bx + c
 * \param[out]   pb        [optional] coeff b of LSF: y = ax^2 + bx + c
 * \param[out]   pc        [optional] coeff c of LSF: y = ax^2 + bx + c
 * \param[out]   pmederr   [optional] median error
 * \param[out]   pnafit    [optional] numa of least square fit to ptad
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This does a quadratic least square fit to the set of points
 *          in %pta.  It then evaluates the errors and removes points
 *          whose error is >= factor * median_error.  It then re-runs
 *          a quadratic LSF on the resulting points.
 * </pre>
 */
l_ok
ptaNoisyQuadraticLSF(PTA        *pta,
                     l_float32   factor,
                     PTA       **pptad,
                     l_float32  *pa,
                     l_float32  *pb,
                     l_float32  *pc,
                     l_float32  *pmederr,
                     NUMA      **pnafit)
{
l_int32    n, i, ret;
l_float32  x, y, yf, val, mederr;
NUMA      *nafit, *naerror;
PTA       *ptad;

    if (pptad) *pptad = NULL;
    if (pa) *pa = 0.0;
    if (pb) *pb = 0.0;
    if (pc) *pc = 0.0;
    if (pmederr) *pmederr = 0.0;
    if (pnafit) *pnafit = NULL;
    if (!pptad && !pa && !pb && !pc && !pnafit)
        return ERROR_INT("no output requested", __func__, 1);
    if (factor <= 0.0)
        return ERROR_INT("factor must be > 0.0", __func__, 1);
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if ((n = ptaGetCount(pta)) < 3)
        return ERROR_INT("less than 3 pts found", __func__, 1);

    if (ptaGetQuadraticLSF(pta, NULL, NULL, NULL, &nafit) != 0)
        return ERROR_INT("error in quadratic LSF", __func__, 1);

        /* Get the median error */
    naerror = numaCreate(n);
    for (i = 0; i < n; i++) {
        ptaGetPt(pta, i, &x, &y);
        numaGetFValue(nafit, i, &yf);
        numaAddNumber(naerror, L_ABS(y - yf));
    }
    numaGetMedian(naerror, &mederr);
    if (pmederr) *pmederr = mederr;
    numaDestroy(&nafit);

        /* Remove outliers */
    ptad = ptaCreate(n);
    for (i = 0; i < n; i++) {
        ptaGetPt(pta, i, &x, &y);
        numaGetFValue(naerror, i, &val);
        if (val <= factor * mederr)  /* <= in case mederr = 0 */
            ptaAddPt(ptad, x, y);
    }
    numaDestroy(&naerror);
    n = ptaGetCount(ptad);
    if ((n = ptaGetCount(ptad)) < 3) {
        ptaDestroy(&ptad);
        return ERROR_INT("less than 3 pts found", __func__, 1);
    }

       /* Do LSF again */
    ret = ptaGetQuadraticLSF(ptad, pa, pb, pc, pnafit);
    if (pptad)
        *pptad = ptad;
    else
        ptaDestroy(&ptad);

    return ret;
}


/*!
 * \brief   applyLinearFit()
 *
 * \param[in]   a, b    linear fit coefficients
 * \param[in]   x
 * \param[out]  py      y = a * x + b
 * \return  0 if OK, 1 on error
 */
l_ok
applyLinearFit(l_float32   a,
                  l_float32   b,
                  l_float32   x,
                  l_float32  *py)
{
    if (!py)
        return ERROR_INT("&y not defined", __func__, 1);

    *py = a * x + b;
    return 0;
}


/*!
 * \brief   applyQuadraticFit()
 *
 * \param[in]   a, b, c    quadratic fit coefficients
 * \param[in]   x
 * \param[out]  py         y = a * x^2 + b * x + c
 * \return  0 if OK, 1 on error
 */
l_ok
applyQuadraticFit(l_float32   a,
                  l_float32   b,
                  l_float32   c,
                  l_float32   x,
                  l_float32  *py)
{
    if (!py)
        return ERROR_INT("&y not defined", __func__, 1);

    *py = a * x * x + b * x + c;
    return 0;
}


/*!
 * \brief   applyCubicFit()
 *
 * \param[in]   a, b, c, d   cubic fit coefficients
 * \param[in]   x
 * \param[out]  py           y = a * x^3 + b * x^2  + c * x + d
 * \return  0 if OK, 1 on error
 */
l_ok
applyCubicFit(l_float32   a,
              l_float32   b,
              l_float32   c,
              l_float32   d,
              l_float32   x,
              l_float32  *py)
{
    if (!py)
        return ERROR_INT("&y not defined", __func__, 1);

    *py = a * x * x * x + b * x * x + c * x + d;
    return 0;
}


/*!
 * \brief   applyQuarticFit()
 *
 * \param[in]   a, b, c, d, e   quartic fit coefficients
 * \param[in]   x
 * \param[out]  py              y = a * x^4 + b * x^3  + c * x^2 + d * x + e
 * \return  0 if OK, 1 on error
 */
l_ok
applyQuarticFit(l_float32   a,
                l_float32   b,
                l_float32   c,
                l_float32   d,
                l_float32   e,
                l_float32   x,
                l_float32  *py)
{
l_float32  x2;

    if (!py)
        return ERROR_INT("&y not defined", __func__, 1);

    x2 = x * x;
    *py = a * x2 * x2 + b * x2 * x + c * x2 + d * x + e;
    return 0;
}


/*---------------------------------------------------------------------*
 *                        Interconversions with Pix                    *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixPlotAlongPta()
 *
 * \param[in]   pixs        any depth
 * \param[in]   pta         set of points on which to plot
 * \param[in]   outformat   GPLOT_PNG, GPLOT_PS, GPLOT_EPS, GPLOT_LATEX
 * \param[in]   title       [optional] for plot; can be null
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is a debugging function.
 *      (2) Removes existing colormaps and clips the pta to the input %pixs.
 *      (3) If the image is RGB, three separate plots are generated.
 * </pre>
 */
l_ok
pixPlotAlongPta(PIX         *pixs,
                PTA         *pta,
                l_int32      outformat,
                const char  *title)
{
char            buffer[128];
char           *rtitle, *gtitle, *btitle;
static l_int32  count = 0;  /* require separate temp files for each call */
l_int32         i, x, y, d, w, h, npts, rval, gval, bval;
l_uint32        val;
NUMA           *na, *nar, *nag, *nab;
PIX            *pixt;

    lept_mkdir("lept/plot");

    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if (outformat != GPLOT_PNG && outformat != GPLOT_PS &&
        outformat != GPLOT_EPS && outformat != GPLOT_LATEX) {
        L_WARNING("outformat invalid; using GPLOT_PNG\n", __func__);
        outformat = GPLOT_PNG;
    }

    pixt = pixRemoveColormap(pixs, REMOVE_CMAP_BASED_ON_SRC);
    d = pixGetDepth(pixt);
    w = pixGetWidth(pixt);
    h = pixGetHeight(pixt);
    npts = ptaGetCount(pta);
    if (d == 32) {
        nar = numaCreate(npts);
        nag = numaCreate(npts);
        nab = numaCreate(npts);
        for (i = 0; i < npts; i++) {
            ptaGetIPt(pta, i, &x, &y);
            if (x < 0 || x >= w)
                continue;
            if (y < 0 || y >= h)
                continue;
            pixGetPixel(pixt, x, y, &val);
            rval = GET_DATA_BYTE(&val, COLOR_RED);
            gval = GET_DATA_BYTE(&val, COLOR_GREEN);
            bval = GET_DATA_BYTE(&val, COLOR_BLUE);
            numaAddNumber(nar, rval);
            numaAddNumber(nag, gval);
            numaAddNumber(nab, bval);
        }

        snprintf(buffer, sizeof(buffer), "/tmp/lept/plot/%03d", count++);
        rtitle = stringJoin("Red: ", title);
        gplotSimple1(nar, outformat, buffer, rtitle);
        snprintf(buffer, sizeof(buffer), "/tmp/lept/plot/%03d", count++);
        gtitle = stringJoin("Green: ", title);
        gplotSimple1(nag, outformat, buffer, gtitle);
        snprintf(buffer, sizeof(buffer), "/tmp/lept/plot/%03d", count++);
        btitle = stringJoin("Blue: ", title);
        gplotSimple1(nab, outformat, buffer, btitle);
        numaDestroy(&nar);
        numaDestroy(&nag);
        numaDestroy(&nab);
        LEPT_FREE(rtitle);
        LEPT_FREE(gtitle);
        LEPT_FREE(btitle);
    } else {
        na = numaCreate(npts);
        for (i = 0; i < npts; i++) {
            ptaGetIPt(pta, i, &x, &y);
            if (x < 0 || x >= w)
                continue;
            if (y < 0 || y >= h)
                continue;
            pixGetPixel(pixt, x, y, &val);
            numaAddNumber(na, (l_float32)val);
        }

        snprintf(buffer, sizeof(buffer), "/tmp/lept/plot/%03d", count++);
        gplotSimple1(na, outformat, buffer, title);
        numaDestroy(&na);
    }
    pixDestroy(&pixt);
    return 0;
}


/*!
 * \brief   ptaGetPixelsFromPix()
 *
 * \param[in]    pixs     1 bpp
 * \param[in]    box      [optional] can be null
 * \return  pta, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Generates a pta of fg pixels in the pix, within the box.
 *          If box == NULL, it uses the entire pix.
 * </pre>
 */
PTA *
ptaGetPixelsFromPix(PIX  *pixs,
                    BOX  *box)
{
l_int32    i, j, w, h, wpl, xstart, xend, ystart, yend, bw, bh;
l_uint32  *data, *line;
PTA       *pta;

    if (!pixs || (pixGetDepth(pixs) != 1))
        return (PTA *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    xstart = ystart = 0;
    xend = w - 1;
    yend = h - 1;
    if (box) {
        boxGetGeometry(box, &xstart, &ystart, &bw, &bh);
        xend = xstart + bw - 1;
        yend = ystart + bh - 1;
    }

    if ((pta = ptaCreate(0)) == NULL)
        return (PTA *)ERROR_PTR("pta not made", __func__, NULL);
    for (i = ystart; i <= yend; i++) {
        line = data + i * wpl;
        for (j = xstart; j <= xend; j++) {
            if (GET_DATA_BIT(line, j))
                ptaAddPt(pta, j, i);
        }
    }

    return pta;
}


/*!
 * \brief   pixGenerateFromPta()
 *
 * \param[in]    pta
 * \param[in]    w, h    of pix
 * \return  pix 1 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Points are rounded to nearest ints.
 *      (2) Any points outside (w,h) are silently discarded.
 *      (3) Output 1 bpp pix has values 1 for each point in the pta.
 * </pre>
 */
PIX *
pixGenerateFromPta(PTA     *pta,
                   l_int32  w,
                   l_int32  h)
{
l_int32  n, i, x, y;
PIX     *pix;

    if (!pta)
        return (PIX *)ERROR_PTR("pta not defined", __func__, NULL);

    if ((pix = pixCreate(w, h, 1)) == NULL)
        return (PIX *)ERROR_PTR("pix not made", __func__, NULL);
    n = ptaGetCount(pta);
    for (i = 0; i < n; i++) {
        ptaGetIPt(pta, i, &x, &y);
        if (x < 0 || x >= w || y < 0 || y >= h)
            continue;
        pixSetPixel(pix, x, y, 1);
    }

    return pix;
}


/*!
 * \brief   ptaGetBoundaryPixels()
 *
 * \param[in]    pixs    1 bpp
 * \param[in]    type    L_BOUNDARY_FG, L_BOUNDARY_BG
 * \return  pta, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a pta of either fg or bg boundary pixels.
 *      (2) See also pixGeneratePtaBoundary() for rendering of
 *          fg boundary pixels.
 * </pre>
 */
PTA *
ptaGetBoundaryPixels(PIX     *pixs,
                     l_int32  type)
{
PIX  *pixt;
PTA  *pta;

    if (!pixs || (pixGetDepth(pixs) != 1))
        return (PTA *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (type != L_BOUNDARY_FG && type != L_BOUNDARY_BG)
        return (PTA *)ERROR_PTR("invalid type", __func__, NULL);

    if (type == L_BOUNDARY_FG)
        pixt = pixMorphSequence(pixs, "e3.3", 0);
    else
        pixt = pixMorphSequence(pixs, "d3.3", 0);
    pixXor(pixt, pixt, pixs);
    pta = ptaGetPixelsFromPix(pixt, NULL);

    pixDestroy(&pixt);
    return pta;
}


/*!
 * \brief   ptaaGetBoundaryPixels()
 *
 * \param[in]    pixs          1 bpp
 * \param[in]    type          L_BOUNDARY_FG, L_BOUNDARY_BG
 * \param[in]    connectivity  4 or 8
 * \param[out]   pboxa         [optional] bounding boxes of the c.c.
 * \param[out]   ppixa         [optional] pixa of the c.c.
 * \return  ptaa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This generates a ptaa of either fg or bg boundary pixels,
 *          where each pta has the boundary pixels for a connected
 *          component.
 *      (2) We can't simply find all the boundary pixels and then select
 *          those within the bounding box of each component, because
 *          bounding boxes can overlap.  It is necessary to extract and
 *          dilate or erode each component separately.  Note also that
 *          special handling is required for bg pixels when the
 *          component touches the pix boundary.
 * </pre>
 */
PTAA *
ptaaGetBoundaryPixels(PIX     *pixs,
                      l_int32  type,
                      l_int32  connectivity,
                      BOXA   **pboxa,
                      PIXA   **ppixa)
{
l_int32  i, n, w, h, x, y, bw, bh, left, right, top, bot;
BOXA    *boxa;
PIX     *pixt1, *pixt2;
PIXA    *pixa;
PTA     *pta1, *pta2;
PTAA    *ptaa;

    if (pboxa) *pboxa = NULL;
    if (ppixa) *ppixa = NULL;
    if (!pixs || (pixGetDepth(pixs) != 1))
        return (PTAA *)ERROR_PTR("pixs undefined or not 1 bpp", __func__, NULL);
    if (type != L_BOUNDARY_FG && type != L_BOUNDARY_BG)
        return (PTAA *)ERROR_PTR("invalid type", __func__, NULL);
    if (connectivity != 4 && connectivity != 8)
        return (PTAA *)ERROR_PTR("connectivity not 4 or 8", __func__, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    boxa = pixConnComp(pixs, &pixa, connectivity);
    n = boxaGetCount(boxa);
    ptaa = ptaaCreate(0);
    for (i = 0; i < n; i++) {
        pixt1 = pixaGetPix(pixa, i, L_CLONE);
        boxaGetBoxGeometry(boxa, i, &x, &y, &bw, &bh);
        left = right = top = bot = 0;
        if (type == L_BOUNDARY_BG) {
            if (x > 0) left = 1;
            if (y > 0) top = 1;
            if (x + bw < w) right = 1;
            if (y + bh < h) bot = 1;
            pixt2 = pixAddBorderGeneral(pixt1, left, right, top, bot, 0);
        } else {
            pixt2 = pixClone(pixt1);
        }
        pta1 = ptaGetBoundaryPixels(pixt2, type);
        pta2 = ptaTransform(pta1, x - left, y - top, 1.0, 1.0);
        ptaaAddPta(ptaa, pta2, L_INSERT);
        ptaDestroy(&pta1);
        pixDestroy(&pixt1);
        pixDestroy(&pixt2);
    }

    if (pboxa)
        *pboxa = boxa;
    else
        boxaDestroy(&boxa);
    if (ppixa)
        *ppixa = pixa;
    else
        pixaDestroy(&pixa);
    return ptaa;
}


/*!
 * \brief   ptaaIndexLabeledPixels()
 *
 * \param[in]    pixs     32 bpp, of indices of c.c.
 * \param[out]   pncc     [optional] number of connected components
 * \return  ptaa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The pixel values in %pixs are the index of the connected component
 *          to which the pixel belongs; %pixs is typically generated from
 *          a 1 bpp pix by pixConnCompTransform().  Background pixels in
 *          the generating 1 bpp pix are represented in %pixs by 0.
 *          We do not check that the pixel values are correctly labelled.
 *      (2) Each pta in the returned ptaa gives the pixel locations
 *          corresponding to a connected component, with the label of each
 *          given by the index of the pta into the ptaa.
 *      (3) Initialize with the first pta in ptaa being empty and
 *          representing the background value (index 0) in the pix.
 * </pre>
 */
PTAA *
ptaaIndexLabeledPixels(PIX      *pixs,
                       l_int32  *pncc)
{
l_int32    wpl, index, i, j, w, h;
l_uint32   maxval;
l_uint32  *data, *line;
PTA       *pta;
PTAA      *ptaa;

    if (pncc) *pncc = 0;
    if (!pixs || (pixGetDepth(pixs) != 32))
        return (PTAA *)ERROR_PTR("pixs undef or not 32 bpp", __func__, NULL);

        /* The number of c.c. is the maximum pixel value.  Use this to
         * initialize ptaa with sufficient pta arrays */
    pixGetMaxValueInRect(pixs, NULL, &maxval, NULL, NULL);
    if (pncc) *pncc = maxval;
    pta = ptaCreate(1);
    ptaa = ptaaCreate(maxval + 1);
    ptaaInitFull(ptaa, pta);
    ptaDestroy(&pta);

        /* Sweep over %pixs, saving the pixel coordinates of each pixel
         * with nonzero value in the appropriate pta, indexed by that value. */
    pixGetDimensions(pixs, &w, &h, NULL);
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    for (i = 0; i < h; i++) {
        line = data + wpl * i;
        for (j = 0; j < w; j++) {
            index = line[j];
            if (index > 0)
                ptaaAddPt(ptaa, index, j, i);
        }
    }

    return ptaa;
}


/*!
 * \brief   ptaGetNeighborPixLocs()
 *
 * \param[in]    pixs    any depth
 * \param[in]    x, y    pixel from which we search for nearest neighbors
 * \param[in]    conn    4 or 8 connectivity
 * \return  pta, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Generates a pta of all valid neighbor pixel locations,
 *          or NULL on error.
 * </pre>
 */
PTA *
ptaGetNeighborPixLocs(PIX  *pixs,
                      l_int32  x,
                      l_int32  y,
                      l_int32  conn)
{
l_int32  w, h;
PTA     *pta;

    if (!pixs)
        return (PTA *)ERROR_PTR("pixs not defined", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    if (x < 0 || x >= w || y < 0 || y >= h)
        return (PTA *)ERROR_PTR("(x,y) not in pixs", __func__, NULL);
    if (conn != 4 && conn != 8)
        return (PTA *)ERROR_PTR("conn not 4 or 8", __func__, NULL);

    pta = ptaCreate(conn);
    if (x > 0)
        ptaAddPt(pta, x - 1, y);
    if (x < w - 1)
        ptaAddPt(pta, x + 1, y);
    if (y > 0)
        ptaAddPt(pta, x, y - 1);
    if (y < h - 1)
        ptaAddPt(pta, x, y + 1);
    if (conn == 8) {
        if (x > 0) {
            if (y > 0)
                ptaAddPt(pta, x - 1, y - 1);
            if (y < h - 1)
                ptaAddPt(pta, x - 1, y + 1);
        }
        if (x < w - 1) {
            if (y > 0)
                ptaAddPt(pta, x + 1, y - 1);
            if (y < h - 1)
                ptaAddPt(pta, x + 1, y + 1);
        }
    }

    return pta;
}


/*---------------------------------------------------------------------*
 *                    Interconversion with Numa                        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   numaConvertToPta1()
 *
 * \param[in]   na    numa with implicit y(x)
 * \return  pta if OK; null on error
 */
PTA *
numaConvertToPta1(NUMA  *na)
{
l_int32    i, n;
l_float32  startx, delx, val;
PTA       *pta;

    if (!na)
        return (PTA *)ERROR_PTR("na not defined", __func__, NULL);

    n = numaGetCount(na);
    pta = ptaCreate(n);
    numaGetParameters(na, &startx, &delx);
    for (i = 0; i < n; i++) {
        numaGetFValue(na, i, &val);
        ptaAddPt(pta, startx + i * delx, val);
    }
    return pta;
}


/*!
 * \brief   numaConvertToPta2()
 *
 * \param[in]   nax
 * \param[in]   nay
 * \return  pta if OK; null on error
 */
PTA *
numaConvertToPta2(NUMA  *nax,
                  NUMA  *nay)
{
l_int32    i, n, nx, ny;
l_float32  valx, valy;
PTA       *pta;

    if (!nax || !nay)
        return (PTA *)ERROR_PTR("nax and nay not both defined", __func__, NULL);

    nx = numaGetCount(nax);
    ny = numaGetCount(nay);
    n = L_MIN(nx, ny);
    if (nx != ny)
        L_WARNING("nx = %d does not equal ny = %d\n", __func__, nx, ny);
    pta = ptaCreate(n);
    for (i = 0; i < n; i++) {
        numaGetFValue(nax, i, &valx);
        numaGetFValue(nay, i, &valy);
        ptaAddPt(pta, valx, valy);
    }
    return pta;
}


/*!
 * \brief   ptaConvertToNuma()
 *
 * \param[in]   pta
 * \param[out]  pnax    addr of nax
 * \param[out]  pnay    addr of nay
 * \return  0 if OK, 1 on error
 */
l_ok
ptaConvertToNuma(PTA    *pta,
                 NUMA  **pnax,
                 NUMA  **pnay)
{
l_int32    i, n;
l_float32  valx, valy;

    if (pnax) *pnax = NULL;
    if (pnay) *pnay = NULL;
    if (!pnax || !pnay)
        return ERROR_INT("&nax and &nay not both defined", __func__, 1);
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);

    n = ptaGetCount(pta);
    *pnax = numaCreate(n);
    *pnay = numaCreate(n);
    for (i = 0; i < n; i++) {
        ptaGetPt(pta, i, &valx, &valy);
        numaAddNumber(*pnax, valx);
        numaAddNumber(*pnay, valy);
    }
    return 0;
}


/*---------------------------------------------------------------------*
 *                          Display Pta and Ptaa                       *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixDisplayPta()
 *
 * \param[in]    pixd    can be same as pixs or NULL; 32 bpp if in-place
 * \param[in]    pixs    1, 2, 4, 8, 16 or 32 bpp
 * \param[in]    pta     of path to be plotted
 * \return  pixd 32 bpp RGB version of pixs, with path in green.
 *
 * <pre>
 * Notes:
 *      (1) To write on an existing pixs, pixs must be 32 bpp and
 *          call with pixd == pixs:
 *             pixDisplayPta(pixs, pixs, pta);
 *          To write to a new pix, use pixd == NULL and call:
 *             pixd = pixDisplayPta(NULL, pixs, pta);
 *      (2) On error, returns pixd to avoid losing pixs if called as
 *             pixs = pixDisplayPta(pixs, pixs, pta);
 * </pre>
 */
PIX *
pixDisplayPta(PIX  *pixd,
              PIX  *pixs,
              PTA  *pta)
{
l_int32   i, n, w, h, x, y;
l_uint32  rpixel, gpixel, bpixel;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (!pta)
        return (PIX *)ERROR_PTR("pta not defined", __func__, pixd);
    if (pixd && (pixd != pixs || pixGetDepth(pixd) != 32))
        return (PIX *)ERROR_PTR("invalid pixd", __func__, pixd);

    if (!pixd)
        pixd = pixConvertTo32(pixs);
    pixGetDimensions(pixd, &w, &h, NULL);
    composeRGBPixel(255, 0, 0, &rpixel);  /* start point */
    composeRGBPixel(0, 255, 0, &gpixel);
    composeRGBPixel(0, 0, 255, &bpixel);  /* end point */

    n = ptaGetCount(pta);
    for (i = 0; i < n; i++) {
        ptaGetIPt(pta, i, &x, &y);
        if (x < 0 || x >= w || y < 0 || y >= h)
            continue;
        if (i == 0)
            pixSetPixel(pixd, x, y, rpixel);
        else if (i < n - 1)
            pixSetPixel(pixd, x, y, gpixel);
        else
            pixSetPixel(pixd, x, y, bpixel);
    }

    return pixd;
}


/*!
 * \brief   pixDisplayPtaaPattern()
 *
 * \param[in]    pixd     32 bpp
 * \param[in]    pixs     1, 2, 4, 8, 16 or 32 bpp; 32 bpp if in place
 * \param[in]    ptaa     giving locations at which the pattern is displayed
 * \param[in]    pixp     1 bpp pattern to be placed such that its reference
 *                        point co-locates with each point in pta
 * \param[in]    cx, cy   reference point in pattern
 * \return  pixd 32 bpp RGB version of pixs.
 *
 * <pre>
 * Notes:
 *      (1) To write on an existing pixs, pixs must be 32 bpp and
 *          call with pixd == pixs:
 *             pixDisplayPtaPattern(pixs, pixs, pta, ...);
 *          To write to a new pix, use pixd == NULL and call:
 *             pixd = pixDisplayPtaPattern(NULL, pixs, pta, ...);
 *      (2) Puts a random color on each pattern associated with a pta.
 *      (3) On error, returns pixd to avoid losing pixs if called as
 *             pixs = pixDisplayPtaPattern(pixs, pixs, pta, ...);
 *      (4) A typical pattern to be used is a circle, generated with
 *             generatePtaFilledCircle()
 * </pre>
 */
PIX *
pixDisplayPtaaPattern(PIX      *pixd,
                      PIX      *pixs,
                      PTAA     *ptaa,
                      PIX      *pixp,
                      l_int32   cx,
                      l_int32   cy)
{
l_int32   i, n;
l_uint32  color;
PIXCMAP  *cmap;
PTA      *pta;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (!ptaa)
        return (PIX *)ERROR_PTR("ptaa not defined", __func__, pixd);
    if (pixd && (pixd != pixs || pixGetDepth(pixd) != 32))
        return (PIX *)ERROR_PTR("invalid pixd", __func__, pixd);
    if (!pixp)
        return (PIX *)ERROR_PTR("pixp not defined", __func__, pixd);

    if (!pixd)
        pixd = pixConvertTo32(pixs);

        /* Use 256 random colors */
    cmap = pixcmapCreateRandom(8, 0, 0);
    n = ptaaGetCount(ptaa);
    for (i = 0; i < n; i++) {
        pixcmapGetColor32(cmap, i % 256, &color);
        pta = ptaaGetPta(ptaa, i, L_CLONE);
        pixDisplayPtaPattern(pixd, pixd, pta, pixp, cx, cy, color);
        ptaDestroy(&pta);
    }

    pixcmapDestroy(&cmap);
    return pixd;
}


/*!
 * \brief   pixDisplayPtaPattern()
 *
 * \param[in]    pixd     can be same as pixs or NULL; 32 bpp if in-place
 * \param[in]    pixs     1, 2, 4, 8, 16 or 32 bpp
 * \param[in]    pta      giving locations at which the pattern is displayed
 * \param[in]    pixp     1 bpp pattern to be placed such that its reference
 *                        point co-locates with each point in pta
 * \param[in]    cx, cy   reference point in pattern
 * \param[in]    color    in 0xrrggbb00 format
 * \return  pixd 32 bpp RGB version of pixs.
 *
 * <pre>
 * Notes:
 *      (1) To write on an existing pixs, pixs must be 32 bpp and
 *          call with pixd == pixs:
 *             pixDisplayPtaPattern(pixs, pixs, pta, ...);
 *          To write to a new pix, use pixd == NULL and call:
 *             pixd = pixDisplayPtaPattern(NULL, pixs, pta, ...);
 *      (2) On error, returns pixd to avoid losing pixs if called as
 *             pixs = pixDisplayPtaPattern(pixs, pixs, pta, ...);
 *      (3) A typical pattern to be used is a circle, generated with
 *             generatePtaFilledCircle()
 * </pre>
 */
PIX *
pixDisplayPtaPattern(PIX      *pixd,
                     PIX      *pixs,
                     PTA      *pta,
                     PIX      *pixp,
                     l_int32   cx,
                     l_int32   cy,
                     l_uint32  color)
{
l_int32  i, n, w, h, x, y;
PTA     *ptat;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, pixd);
    if (!pta)
        return (PIX *)ERROR_PTR("pta not defined", __func__, pixd);
    if (pixd && (pixd != pixs || pixGetDepth(pixd) != 32))
        return (PIX *)ERROR_PTR("invalid pixd", __func__, pixd);
    if (!pixp)
        return (PIX *)ERROR_PTR("pixp not defined", __func__, pixd);

    if (!pixd)
        pixd = pixConvertTo32(pixs);
    pixGetDimensions(pixs, &w, &h, NULL);
    ptat = ptaReplicatePattern(pta, pixp, NULL, cx, cy, w, h);

    n = ptaGetCount(ptat);
    for (i = 0; i < n; i++) {
        ptaGetIPt(ptat, i, &x, &y);
        if (x < 0 || x >= w || y < 0 || y >= h)
            continue;
        pixSetPixel(pixd, x, y, color);
    }

    ptaDestroy(&ptat);
    return pixd;
}


/*!
 * \brief   ptaReplicatePattern()
 *
 * \param[in]    ptas    "sparse" input pta
 * \param[in]    pixp    [optional] 1 bpp pattern, to be replicated
 *                                  in output pta
 * \param[in]    ptap    [optional] set of pts, to be replicated in output pta
 * \param[in]    cx, cy  reference point in pattern
 * \param[in]    w, h    clipping sizes for output pta
 * \return  ptad with all points of replicated pattern, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) You can use either the image %pixp or the set of pts %ptap.
 *      (2) The pattern is placed with its reference point at each point
 *          in ptas, and all the fg pixels are colleced into ptad.
 *          For %pixp, this is equivalent to blitting pixp at each point
 *          in ptas, and then converting the resulting pix to a pta.
 * </pre>
 */
PTA *
ptaReplicatePattern(PTA     *ptas,
                    PIX     *pixp,
                    PTA     *ptap,
                    l_int32  cx,
                    l_int32  cy,
                    l_int32  w,
                    l_int32  h)
{
l_int32  i, j, n, np, x, y, xp, yp, xf, yf;
PTA     *ptat, *ptad;

    if (!ptas)
        return (PTA *)ERROR_PTR("ptas not defined", __func__, NULL);
    if (!pixp && !ptap)
        return (PTA *)ERROR_PTR("no pattern is defined", __func__, NULL);
    if (pixp && ptap)
        L_WARNING("pixp and ptap defined; using ptap\n", __func__);

    n = ptaGetCount(ptas);
    ptad = ptaCreate(n);
    if (ptap)
        ptat = ptaClone(ptap);
    else
        ptat = ptaGetPixelsFromPix(pixp, NULL);
    np = ptaGetCount(ptat);
    for (i = 0; i < n; i++) {
        ptaGetIPt(ptas, i, &x, &y);
        for (j = 0; j < np; j++) {
            ptaGetIPt(ptat, j, &xp, &yp);
            xf = x - cx + xp;
            yf = y - cy + yp;
            if (xf >= 0 && xf < w && yf >= 0 && yf < h)
                ptaAddPt(ptad, xf, yf);
        }
    }

    ptaDestroy(&ptat);
    return ptad;
}


/*!
 * \brief   pixDisplayPtaa()
 *
 * \param[in]    pixs    1, 2, 4, 8, 16 or 32 bpp
 * \param[in]    ptaa    array of paths to be plotted
 * \return  pixd 32 bpp RGB version of pixs, with paths plotted
 *                    in different colors, or NULL on error
 */
PIX *
pixDisplayPtaa(PIX   *pixs,
               PTAA  *ptaa)
{
l_int32    i, j, w, h, npta, npt, x, y, rv, gv, bv;
l_uint32  *pixela;
NUMA      *na1, *na2, *na3;
PIX       *pixd;
PTA       *pta;

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (!ptaa)
        return (PIX *)ERROR_PTR("ptaa not defined", __func__, NULL);
    npta = ptaaGetCount(ptaa);
    if (npta == 0)
        return (PIX *)ERROR_PTR("no pta", __func__, NULL);

    if ((pixd = pixConvertTo32(pixs)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixGetDimensions(pixd, &w, &h, NULL);

        /* Make a colormap for the paths */
    if ((pixela = (l_uint32 *)LEPT_CALLOC(npta, sizeof(l_uint32))) == NULL) {
        pixDestroy(&pixd);
        return (PIX *)ERROR_PTR("calloc fail for pixela", __func__, NULL);
    }
    na1 = numaPseudorandomSequence(256, 14657);
    na2 = numaPseudorandomSequence(256, 34631);
    na3 = numaPseudorandomSequence(256, 54617);
    for (i = 0; i < npta; i++) {
        numaGetIValue(na1, i % 256, &rv);
        numaGetIValue(na2, i % 256, &gv);
        numaGetIValue(na3, i % 256, &bv);
        composeRGBPixel(rv, gv, bv, &pixela[i]);
    }
    numaDestroy(&na1);
    numaDestroy(&na2);
    numaDestroy(&na3);

    for (i = 0; i < npta; i++) {
        pta = ptaaGetPta(ptaa, i, L_CLONE);
        npt = ptaGetCount(pta);
        for (j = 0; j < npt; j++) {
            ptaGetIPt(pta, j, &x, &y);
            if (x < 0 || x >= w || y < 0 || y >= h)
                continue;
            pixSetPixel(pixd, x, y, pixela[i]);
        }
        ptaDestroy(&pta);
    }

    LEPT_FREE(pixela);
    return pixd;
}

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
 * \file  pixtiling.c
 * <pre>
 *
 *        PIXTILING       *pixTilingCreate()
 *        void            *pixTilingDestroy()
 *        l_int32          pixTilingGetCount()
 *        l_int32          pixTilingGetSize()
 *        PIX             *pixTilingGetTile()
 *        l_int32          pixTilingNoStripOnPaint()
 *        l_int32          pixTilingPaintTile()
 *
 *   This provides a simple way to split an image into tiles
 *   and to perform operations independently on each tile.
 *
 *   The tile created with pixTilingGetTile() can have pixels in
 *   adjacent tiles for computation.  The number of extra pixels
 *   on each side of the tile is given by an 'overlap' parameter
 *   to pixTilingCreate().  For tiles at the boundary of
 *   the input image, quasi-overlap pixels are created by reflection
 *   symmetry into the tile.
 *
 *   Here's a typical intended usage.  Suppose you want to parallelize
 *   the operation on an image, by operating on tiles.  For each
 *   tile, you want to generate an in-place image result at the same
 *   resolution.  Suppose you choose a one-dimensional vertical tiling,
 *   where the desired tile width is 256 pixels and the overlap is
 *   30 pixels on left and right sides:
 *
 *     PIX *pixd = pixCreateTemplate(pixs);  // output
 *     PIXTILING  *pt = pixTilingCreate(pixs, 0, 1, 256, 30, 0);
 *     pixTilingGetCount(pt, &nx, NULL);
 *     for (j = 0; j < nx; j++) {
 *         PIX *pixt = pixTilingGetTile(pt, 0, j);
 *         SomeInPlaceOperation(pixt, 30, 0, ...);
 *         pixTilingPaintTile(pixd, 0, j, pixt, pt);
 *         pixDestroy(&pixt);
 *     }
 *
 *   In this example, note the following:
 *    ~ The unspecfified in-place operation could instead generate
 *      a new pix.  If this is done, the resulting pix must be the
 *      same size as pixt, because pixTilingPaintTile() makes that
 *      assumption, removing the overlap pixels before painting
 *      into the destination.
 *    ~ The 'overlap' parameters have been included in your function,
 *      to indicate which pixels are not in the exterior overlap region.
 *      You will need to change only pixels that are not in the overlap
 *      region, because those are the pixels that will be painted
 *      into the destination.
 *    ~ For tiles on the outside of the image, mirrored pixels are
 *      added to substitute for the overlap that is added to interior
 *      tiles.  This allows you to implement your function without
 *      reference to which tile it is; no special coding is necessary
 *      for pixels that are near the image boundary.
 *    ~ The tiles are labeled by (i, j) = (row, column),
 *      and in this example there is one row and nx columns.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"
#include "pix_internal.h"

/*!
 * \brief   pixTilingCreate()
 *
 * \param[in]    pixs       pix to be tiled; any depth; colormap OK
 * \param[in]    nx         number of tiles across image
 * \param[in]    ny         number of tiles down image
 * \param[in]    w          desired width of each tile
 * \param[in]    h          desired height of each tile
 * \param[in]    xoverlap   overlap into neighboring tiles on each side
 * \param[in]    yoverlap   overlap into neighboring tiles above and below
 * \return  pixtiling, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) We put a clone of pixs in the PixTiling.
 *      (2) The input to pixTilingCreate() for horizontal tiling can be
 *          either the number of tiles across the image or the approximate
 *          width of the tiles.  If the latter, the actual width will be
 *          determined by making all tiles but the last of equal width, and
 *          making the last as close to the others as possible.  The same
 *          consideration is applied independently to the vertical tiling.
 *          To specify tile width, set nx = 0; to specify the number of
 *          tiles horizontally across the image, set w = 0.
 *      (3) If pixs is to be tiled in one-dimensional strips, use ny = 1 for
 *          vertical strips and nx = 1 for horizontal strips.
 *      (4) The overlap must not be larger than the width or height of
 *          the leftmost or topmost tile(s).
 * </pre>
 */
PIXTILING *
pixTilingCreate(PIX     *pixs,
                l_int32  nx,
                l_int32  ny,
                l_int32  w,
                l_int32  h,
                l_int32  xoverlap,
                l_int32  yoverlap)
{
l_int32     width, height;
PIXTILING  *pt;

    if (!pixs)
        return (PIXTILING *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (nx < 1 && w < 1)
        return (PIXTILING *)ERROR_PTR("invalid width spec", __func__, NULL);
    if (ny < 1 && h < 1)
        return (PIXTILING *)ERROR_PTR("invalid height spec", __func__, NULL);

        /* Find the tile width and number of tiles.  All tiles except the
         * rightmost ones have the same width.  The width of the
         * rightmost ones are at least the width of the others and
         * less than twice that width.  Ditto for tile height. */
    pixGetDimensions(pixs, &width, &height, NULL);
    if (nx == 0)
        nx = L_MAX(1, width / w);
    w = width / nx;  /* possibly reset */
    if (ny == 0)
        ny = L_MAX(1, height / h);
    h = height / ny;  /* possibly reset */
    if (xoverlap > w || yoverlap > h) {
        L_INFO("tile width = %d, tile height = %d\n", __func__, w, h);
        return (PIXTILING *)ERROR_PTR("overlap too large", __func__, NULL);
    }

    pt = (PIXTILING *)LEPT_CALLOC(1, sizeof(PIXTILING));
    pt->pix = pixClone(pixs);
    pt->xoverlap = xoverlap;
    pt->yoverlap = yoverlap;
    pt->nx = nx;
    pt->ny = ny;
    pt->w = w;
    pt->h = h;
    pt->strip = TRUE;
    return pt;
}


/*!
 * \brief   pixTilingDestroy()
 *
 * \param[in,out]   ppt   will be set to null before returning
 * \return  void
 */
void
pixTilingDestroy(PIXTILING  **ppt)
{
PIXTILING  *pt;

    if (ppt == NULL) {
        L_WARNING("ptr address is null!\n", __func__);
        return;
    }

    if ((pt = *ppt) == NULL)
        return;

    pixDestroy(&pt->pix);
    LEPT_FREE(pt);
    *ppt = NULL;
}


/*!
 * \brief   pixTilingGetCount()
 *
 * \param[in]    pt     pixtiling
 * \param[out]   pnx    [optional] nx; can be null
 * \param[out]   pny    [optional] ny; can be null
 * \return  0 if OK, 1 on error
 */
l_ok
pixTilingGetCount(PIXTILING  *pt,
                  l_int32    *pnx,
                  l_int32    *pny)
{
    if (!pt)
        return ERROR_INT("pt not defined", __func__, 1);
    if (pnx) *pnx = pt->nx;
    if (pny) *pny = pt->ny;
    return 0;
}


/*!
 * \brief   pixTilingGetSize()
 *
 * \param[in]    pt    pixtiling
 * \param[out]   pw    [optional] tile width; can be null
 * \param[out]   ph    [optional] tile height; can be null
 * \return  0 if OK, 1 on error
 */
l_ok
pixTilingGetSize(PIXTILING  *pt,
                 l_int32    *pw,
                 l_int32    *ph)
{
    if (!pt)
        return ERROR_INT("pt not defined", __func__, 1);
    if (pw) *pw = pt->w;
    if (ph) *ph = pt->h;
    return 0;
}


/*!
 * \brief   pixTilingGetTile()
 *
 * \param[in]    pt    pixtiling
 * \param[in]    i     tile row index
 * \param[in]    j     tile column index
 * \return  pixd   tile with appropriate boundary (overlap) pixels added,
 *                    or NULL on error
 */
PIX *
pixTilingGetTile(PIXTILING  *pt,
                 l_int32     i,
                 l_int32     j)
{
l_int32  wpix, hpix, wt, ht, nx, ny;
l_int32  xoverlap, yoverlap, wtlast, htlast;
l_int32  left, top, xtraleft, xtraright, xtratop, xtrabot, width, height;
BOX     *box;
PIX     *pixs, *pixt, *pixd;

    if (!pt)
        return (PIX *)ERROR_PTR("pt not defined", __func__, NULL);
    if ((pixs = pt->pix) == NULL)
        return (PIX *)ERROR_PTR("pix not found", __func__, NULL);
    pixTilingGetCount(pt, &nx, &ny);
    if (i < 0 || i >= ny)
        return (PIX *)ERROR_PTR("invalid row index i", __func__, NULL);
    if (j < 0 || j >= nx)
        return (PIX *)ERROR_PTR("invalid column index j", __func__, NULL);

        /* Grab the tile with as much overlap as exists within the
         * input pix.   First, compute the (left, top) coordinates.  */
    pixGetDimensions(pixs, &wpix, &hpix, NULL);
    pixTilingGetSize(pt, &wt, &ht);
    xoverlap = pt->xoverlap;
    yoverlap = pt->yoverlap;
    wtlast = wpix - wt * (nx - 1);
    htlast = hpix - ht * (ny - 1);
    left = L_MAX(0, j * wt - xoverlap);
    top = L_MAX(0, i * ht - yoverlap);

        /* Get the width and height of the tile, including whatever
         * overlap is available. */
    if (nx == 1)
        width = wpix;
    else if (j == 0)
        width = wt + xoverlap;
    else if (j == nx - 1)
        width = wtlast + xoverlap;
    else
        width = wt + 2 * xoverlap;

    if (ny == 1)
        height = hpix;
    else if (i == 0)
        height = ht + yoverlap;
    else if (i == ny - 1)
        height = htlast + yoverlap;
    else
        height = ht + 2 * yoverlap;
    box = boxCreate(left, top, width, height);
    pixt = pixClipRectangle(pixs, box, NULL);
    boxDestroy(&box);

        /* If no overlap, do not add any special case borders */
    if (xoverlap == 0 && yoverlap == 0)
        return pixt;

        /* Add overlap as a mirrored border, in the 8 special cases where
         * the tile touches the border of the input pix.  The xtratop (etc)
         * parameters are required where the tile is either full width
         * or full height.  */
    xtratop = xtrabot = xtraleft = xtraright = 0;
    if (nx == 1)
        xtraleft = xtraright = xoverlap;
    if (ny == 1)
        xtratop = xtrabot = yoverlap;
    if (i == 0 && j == 0)
        pixd = pixAddMirroredBorder(pixt, xoverlap, xtraright,
                                    yoverlap, xtrabot);
    else if (i == 0 && j == nx - 1)
        pixd = pixAddMirroredBorder(pixt, xtraleft, xoverlap,
                                    yoverlap, xtrabot);
    else if (i == ny - 1 && j == 0)
        pixd = pixAddMirroredBorder(pixt, xoverlap, xtraright,
                                    xtratop, yoverlap);
    else if (i == ny - 1 && j == nx - 1)
        pixd = pixAddMirroredBorder(pixt, xtraleft, xoverlap,
                                    xtratop, yoverlap);
    else if (i == 0)
        pixd = pixAddMirroredBorder(pixt, 0, 0, yoverlap, xtrabot);
    else if (i == ny - 1)
        pixd = pixAddMirroredBorder(pixt, 0, 0, xtratop, yoverlap);
    else if (j == 0)
        pixd = pixAddMirroredBorder(pixt, xoverlap, xtraright, 0, 0);
    else if (j == nx - 1)
        pixd = pixAddMirroredBorder(pixt, xtraleft, xoverlap, 0, 0);
    else
        pixd = pixClone(pixt);
    pixDestroy(&pixt);

    return pixd;
}


/*!
 * \brief   pixTilingNoStripOnPaint()
 *
 * \param[in]    pt    pixtiling
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The default for paint is to strip out the overlap pixels
 *          that are added by pixTilingGetTile().  However, some
 *          operations will generate an image with these pixels
 *          stripped off.  This tells the paint operation not
 *          to strip the added boundary pixels when painting.
 * </pre>
 */
l_ok
pixTilingNoStripOnPaint(PIXTILING  *pt)
{
    if (!pt)
        return ERROR_INT("pt not defined", __func__, 1);
    pt->strip = FALSE;
    return 0;
}


/*!
 * \brief   pixTilingPaintTile()
 *
 * \param[in]    pixd    dest: paint tile onto this, without overlap
 * \param[in]    i       tile row index
 * \param[in]    j       tile column index
 * \param[in]    pixs    source: tile to be painted from
 * \param[in]    pt      pixtiling struct
 * \return  0 if OK, 1 on error
 */
l_ok
pixTilingPaintTile(PIX        *pixd,
                   l_int32     i,
                   l_int32     j,
                   PIX        *pixs,
                   PIXTILING  *pt)
{
l_int32  w, h;

    if (!pixd)
        return ERROR_INT("pixd not defined", __func__, 1);
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);
    if (!pt)
        return ERROR_INT("pt not defined", __func__, 1);
    if (i < 0 || i >= pt->ny)
        return ERROR_INT("invalid row index i", __func__, 1);
    if (j < 0 || j >= pt->nx)
        return ERROR_INT("invalid column index j", __func__, 1);

        /* Strip added border pixels off if requested */
    pixGetDimensions(pixs, &w, &h, NULL);
    if (pt->strip == TRUE) {
        pixRasterop(pixd, j * pt->w, i * pt->h,
                    w - 2 * pt->xoverlap, h - 2 * pt->yoverlap, PIX_SRC,
                    pixs, pt->xoverlap, pt->yoverlap);
    } else {
        pixRasterop(pixd, j * pt->w, i * pt->h, w, h, PIX_SRC, pixs, 0, 0);
    }

    return 0;
}

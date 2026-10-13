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
 * \file  pixafunc2.c
 * <pre>
 *
 *      Pixa display (render into a pix)
 *           PIX      *pixaDisplay()
 *           PIX      *pixaDisplayRandomCmap()
 *           PIX      *pixaDisplayLinearly()
 *           PIX      *pixaDisplayOnLattice()
 *           PIX      *pixaDisplayUnsplit()
 *           PIX      *pixaDisplayTiled()
 *           PIX      *pixaDisplayTiledInRows()
 *           PIX      *pixaDisplayTiledInColumns()
 *           PIX      *pixaDisplayTiledAndScaled()
 *           PIX      *pixaDisplayTiledWithText()
 *           PIX      *pixaDisplayTiledByIndex()
 *
 *      Pixa pair display (render into a pix)
 *           PIX      *pixaDisplayPairTiledInColumns()
 *
 *      Pixaa display (render into a pix)
 *           PIX      *pixaaDisplay()
 *           PIX      *pixaaDisplayByPixa()
 *           PIXA     *pixaaDisplayTiledAndScaled()
 *
 *      Conversion of all pix to specified type (e.g., depth)
 *           PIXA     *pixaConvertTo1()
 *           PIXA     *pixaConvertTo8()
 *           PIXA     *pixaConvertTo8Colormap()
 *           PIXA     *pixaConvertTo32()
 *
 *      Pixa constrained selection and pdf generation
 *           PIXA     *pixaConstrainedSelect()
 *           l_int32   pixaSelectToPdf()
 *
 *      Generate pixa from tiled images
 *           PIXA     *pixaMakeFromTiledPixa()
 *           PIXA     *pixaMakeFromTiledPix()
 *           l_int32   pixGetTileCount()
 *
 *      Pixa display into multiple tiles
 *           PIXA     *pixaDisplayMultiTiled()
 *
 *      Split pixa into files
 *           l_int32   pixaSplitIntoFiles()
 *
 *      Tile N-Up
 *           l_int32   convertToNUpFiles()
 *           PIXA     *convertToNUpPixa()
 *           PIXA     *pixaConvertToNUpPixa()
 *
 *      Render two pixa side-by-side for comparison                   *
 *           l_int32   pixaCompareInPdf()
 *
 *  We give twelve pixaDisplay*() methods for tiling a pixa in a pix.
 *  Some work for 1 bpp input; others for any input depth.
 *  Some give an output depth that depends on the input depth;
 *  others give a different output depth or allow you to choose it.
 *  Some use a boxes to determine where each pix goes; others tile
 *  onto a regular lattice; others tile onto an irregular lattice;
 *  one uses an associated index array to determine which column
 *  each pix goes into.
 *
 *  Here is a brief description of what the pixa display functions do.
 *
 *    pixaDisplay()
 *        This uses the boxes in the pixa to lay out each pix.  This
 *        can be used to reconstruct a pix that has been broken into
 *        components, if the boxes represents the positions of the
 *        components in the original image.
 *    pixaDisplayRandomCmap()
 *        This also uses the boxes to lay out each pix.  However, it creates
 *        a colormapped dest, where each 1 bpp pix is given a randomly
 *        generated color (up to 256 are used).
 *    pixaDisplayLinearly()
 *        This puts each pix, sequentially, in a line, either horizontally
 *        or vertically.
 *    pixaDisplayOnLattice()
 *        This puts each pix, sequentially, onto a regular lattice,
 *        omitting any pix that are too big for the lattice size.
 *        This is useful, for example, to store bitmapped fonts,
 *        where all the characters are stored in a single image.
 *    pixaDisplayUnsplit()
 *        This lays out a mosaic of tiles (the pix in the pixa) that
 *        are all of equal size.  (Don't use this for unequal sized pix!)
 *        For example, it can be used to invert the action of
 *        pixaSplitPix().
 *    pixaDisplayTiled()
 *        Like pixaDisplayOnLattice(), this places each pix on a regular
 *        lattice, but here the lattice size is determined by the
 *        largest component, and no components are omitted.  This is
 *        dangerous if there are thousands of small components and
 *        one or more very large one, because the size of the resulting
 *        pix can be huge!
 *    pixaDisplayTiledInRows()
 *        This puts each pix down in a series of rows, where the upper
 *        edges of each pix in a row are aligned and there is a uniform
 *        spacing between the pix.  The height of each row is determined
 *        by the tallest pix that was put in the row.  This function
 *        is a reasonably efficient way to pack the subimages.
 *        A boxa of the locations of each input pix is stored in the output.
 *    pixaDisplayTiledInColumns()
 *        This puts each pix down in a series of rows, each row having
 *        a specified number of pix.  The upper edges of each pix in a
 *        row are aligned and there is a uniform spacing between the pix.
 *        The height of each row is determined by the tallest pix that
 *        was put in the row.  A boxa of the locations of each input
 *        pix is stored in the output.
 *    pixaDisplayTiledAndScaled()
 *        This scales each pix to a given width and output depth, and then
 *        tiles them in rows with a given number placed in each row.
 *        This is useful for presenting a sequence of images that can be
 *        at different resolutions, but which are derived from the same
 *        initial image.
 *    pixaDisplayTiledWithText()
 *        This is a version of pixaDisplayTiledInRows() that prints, below
 *        each pix, the text in the pix text field.  It renders a pixa
 *        to an image with white background that does not exceed a
 *        given value in width.
 *    pixaDisplayTiledByIndex()
 *        This scales each pix to a given width and output depth,
 *        and then tiles them in columns corresponding to the value
 *        in an associated numa.  All pix with the same index value are
 *        rendered in the same column.  Text in the pix text field are
 *        rendered below the pix.
 *
 *  To render mosaics of images in a pixaa, display functions are
 *  provided that handle situations where the images are all scaled to
 *  the same size, or the number of images on each row needs to vary.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>   /* for sqrt() */
#include "allheaders.h"

/*---------------------------------------------------------------------*
 *                               Pixa Display                          *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaDisplay()
 *
 * \param[in]    pixa
 * \param[in]    w, h    if set to 0, the size is determined from the
 *                       bounding box of the components in pixa
 * \return  pix, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This uses the boxes to place each pix in the rendered composite.
 *      (2) Set w = h = 0 to use the b.b. of the components to determine
 *          the size of the returned pix.
 *      (3) Uses the first pix in pixa to determine the depth.
 *      (4) The background is written "white".  On 1 bpp, each successive
 *          pix is "painted" (adding foreground), whereas for grayscale
 *          or color each successive pix is blitted with just the src.
 *      (5) If the pixa is empty, returns an empty 1 bpp pix.
 * </pre>
 */
PIX *
pixaDisplay(PIXA    *pixa,
            l_int32  w,
            l_int32  h)
{
l_int32  i, n, d, xb, yb, wb, hb, res;
BOXA    *boxa;
PIX     *pix1, *pixd;

    if (!pixa)
        return (PIX *)ERROR_PTR("pixa not defined", __func__, NULL);

    n = pixaGetCount(pixa);
    if (n == 0 && w == 0 && h == 0)
        return (PIX *)ERROR_PTR("no components; no size", __func__, NULL);
    if (n == 0) {
        L_WARNING("no components; returning empty 1 bpp pix\n", __func__);
        return pixCreate(w, h, 1);
    }

        /* If w and h not input, determine the minimum size required
         * to contain the origin and all c.c. */
    if (w == 0 || h == 0) {
        boxa = pixaGetBoxa(pixa, L_CLONE);
        boxaGetExtent(boxa, &w, &h, NULL);
        boxaDestroy(&boxa);
        if (w == 0 || h == 0)
            return (PIX *)ERROR_PTR("no associated boxa", __func__, NULL);
    }

        /* Use the first pix in pixa to determine depth and resolution  */
    pix1 = pixaGetPix(pixa, 0, L_CLONE);
    d = pixGetDepth(pix1);
    res = pixGetXRes(pix1);
    pixDestroy(&pix1);

    if ((pixd = pixCreate(w, h, d)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pixSetResolution(pixd, res, res);
    if (d > 1)
        pixSetAll(pixd);
    for (i = 0; i < n; i++) {
        if (pixaGetBoxGeometry(pixa, i, &xb, &yb, &wb, &hb)) {
            L_WARNING("no box found!\n", __func__);
            continue;
        }
        pix1 = pixaGetPix(pixa, i, L_CLONE);
        if (d == 1)
            pixRasterop(pixd, xb, yb, wb, hb, PIX_PAINT, pix1, 0, 0);
        else
            pixRasterop(pixd, xb, yb, wb, hb, PIX_SRC, pix1, 0, 0);
        pixDestroy(&pix1);
    }

    return pixd;
}


/*!
 * \brief   pixaDisplayRandomCmap()
 *
 * \param[in]    pixa    1 bpp regions, with boxa delineating those regions
 * \param[in]    w, h    if set to 0, the size is determined from the
 *                       bounding box of the components in pixa
 * \return  pix   8 bpp, cmapped, with random colors assigned to each region,
 *                or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) This uses the boxes to place each pix in the rendered composite.
 *          The fg of each pix in %pixa, such as a single connected
 *          component or a line of text, is given a random color.
 *      (2) By default, the background color is black (cmap index 0).
 *          This can be changed by pixcmapResetColor()
 * </pre>
 */
PIX *
pixaDisplayRandomCmap(PIXA    *pixa,
                      l_int32  w,
                      l_int32  h)
{
l_int32   i, n, same, maxd, index, xb, yb, wb, hb, res;
BOXA     *boxa;
PIX      *pixs, *pix1, *pixd;
PIXCMAP  *cmap;

    if (!pixa)
        return (PIX *)ERROR_PTR("pixa not defined", __func__, NULL);

    if ((n = pixaGetCount(pixa)) == 0)
        return (PIX *)ERROR_PTR("no components", __func__, NULL);
    pixaVerifyDepth(pixa, &same, &maxd);
    if (maxd > 1)
        return (PIX *)ERROR_PTR("not all components are 1 bpp", __func__, NULL);

        /* If w and h are not input, determine the minimum size required
         * to contain the origin and all c.c. */
    if (w == 0 || h == 0) {
        boxa = pixaGetBoxa(pixa, L_CLONE);
        boxaGetExtent(boxa, &w, &h, NULL);
        boxaDestroy(&boxa);
    }

        /* Set up an 8 bpp dest pix, with a colormap with 254 random colors */
    if ((pixd = pixCreate(w, h, 8)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    cmap = pixcmapCreateRandom(8, 1, 1);
    pixSetColormap(pixd, cmap);

        /* Color each component and blit it in */
    for (i = 0; i < n; i++) {
        index = 1 + (i % 254);
        pixaGetBoxGeometry(pixa, i, &xb, &yb, &wb, &hb);
        pixs = pixaGetPix(pixa, i, L_CLONE);
        if (i == 0) res = pixGetXRes(pixs);
        pix1 = pixConvert1To8(NULL, pixs, 0, index);
        pixRasterop(pixd, xb, yb, wb, hb, PIX_PAINT, pix1, 0, 0);
        pixDestroy(&pixs);
        pixDestroy(&pix1);
    }

    pixSetResolution(pixd, res, res);
    return pixd;
}


/*!
 * \brief   pixaDisplayLinearly()
 *
 * \param[in]    pixas
 * \param[in]    direction    L_HORIZ or L_VERT
 * \param[in]    scalefactor  applied to every pix; use 1.0 for no scaling
 * \param[in]    background   0 for white, 1 for black; this is the color
 *                            of the spacing between the images
 * \param[in]    spacing      between images, and on outside
 * \param[in]    border       width of black border added to each image;
 *                            use 0 for no border
 * \param[out]   pboxa        [optional] location of images in output pix
 * \return  pix of composite images, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This puts each pix, sequentially, in a line, either horizontally
 *          or vertically.
 *      (2) If any pix has a colormap, all pix are rendered in rgb.
 *      (3) The boxa gives the location of each image.
 * </pre>
 */
PIX *
pixaDisplayLinearly(PIXA      *pixas,
                    l_int32    direction,
                    l_float32  scalefactor,
                    l_int32    background,  /* not used */
                    l_int32    spacing,
                    l_int32    border,
                    BOXA     **pboxa)
{
l_int32  i, n, x, y, w, h, depth, bordval;
BOX     *box;
PIX     *pix1, *pix2, *pix3, *pixd;
PIXA    *pixa1, *pixa2;

    if (pboxa) *pboxa = NULL;
    if (!pixas)
        return (PIX *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (direction != L_HORIZ && direction != L_VERT)
        return (PIX *)ERROR_PTR("invalid direction", __func__, NULL);

        /* Make sure all pix are at the same depth */
    pixa1 = pixaConvertToSameDepth(pixas);
    pixaGetDepthInfo(pixa1, &depth, NULL);

        /* Scale and add border if requested */
    n = pixaGetCount(pixa1);
    pixa2 = pixaCreate(n);
    bordval = (depth == 1) ? 1 : 0;
    x = y = 0;
    for (i = 0; i < n; i++) {
        if ((pix1 = pixaGetPix(pixa1, i, L_CLONE)) == NULL) {
            L_WARNING("missing pix at index %d\n", __func__, i);
            continue;
        }

        if (scalefactor != 1.0)
            pix2 = pixScale(pix1, scalefactor, scalefactor);
        else
            pix2 = pixClone(pix1);
        if (border)
            pix3 = pixAddBorder(pix2, border, bordval);
        else
            pix3 = pixClone(pix2);

        pixGetDimensions(pix3, &w, &h, NULL);
        box = boxCreate(x, y, w, h);
        if (direction == L_HORIZ)
            x += w + spacing;
        else  /* vertical */
            y += h + spacing;
        pixaAddPix(pixa2, pix3, L_INSERT);
        pixaAddBox(pixa2, box, L_INSERT);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }
    pixd = pixaDisplay(pixa2, 0, 0);

    if (pboxa)
        *pboxa = pixaGetBoxa(pixa2, L_COPY);
    pixaDestroy(&pixa1);
    pixaDestroy(&pixa2);
    return pixd;
}


/*!
 * \brief   pixaDisplayOnLattice()
 *
 * \param[in]    pixa
 * \param[in]    cellw    lattice cell width
 * \param[in]    cellh    lattice cell height
 * \param[out]   pncols   [optional] number of columns in output lattice
 * \param[out]   pboxa    [optional] location of images in lattice
 * \return  pix of composite images, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This places each pix on sequentially on a regular lattice
 *          in the rendered composite.  If a pix is too large to fit in the
 *          allocated lattice space, it is not rendered.
 *      (2) If any pix has a colormap, all pix are rendered in rgb.
 *      (3) This is useful when putting bitmaps of components,
 *          such as characters, into a single image.
 *      (4) Save the number of tiled images in the text field of the pix,
 *          in the format: n = %d.  This survives write/read into png files,
 *          for example.
 *      (5) The boxa gives the location of each image.  The UL corner
 *          of each image is on a lattice cell corner.  Omitted images
 *          (due to size) are assigned an invalid width and height of 0.
 * </pre>
 */
PIX *
pixaDisplayOnLattice(PIXA     *pixa,
                     l_int32   cellw,
                     l_int32   cellh,
                     l_int32  *pncols,
                     BOXA    **pboxa)
{
char     buf[16];
l_int32  n, nw, nh, w, h, d, wt, ht, res, samedepth;
l_int32  index, i, j, hascmap;
BOX     *box;
BOXA    *boxa;
PIX     *pix1, *pix2, *pixd;
PIXA    *pixa1;

    if (pncols) *pncols = 0;
    if (pboxa) *pboxa = NULL;
    if (!pixa)
        return (PIX *)ERROR_PTR("pixa not defined", __func__, NULL);

        /* If any pix have colormaps, or if the depths differ, generate rgb */
    if ((n = pixaGetCount(pixa)) == 0)
        return (PIX *)ERROR_PTR("no components", __func__, NULL);
    pixaAnyColormaps(pixa, &hascmap);
    pixaVerifyDepth(pixa, &samedepth, NULL);
    if (hascmap || !samedepth) {
        pixa1 = pixaCreate(n);
        for (i = 0; i < n; i++) {
            pix1 = pixaGetPix(pixa, i, L_CLONE);
            pix2 = pixConvertTo32(pix1);
            pixaAddPix(pixa1, pix2, L_INSERT);
            pixDestroy(&pix1);
        }
    } else {
        pixa1 = pixaCopy(pixa, L_CLONE);
    }

        /* Have number of rows and columns approximately equal */
    nw = (l_int32)sqrt((l_float64)n);
    nh = (n + nw - 1) / nw;
    w = cellw * nw;
    h = cellh * nh;

        /* Use the first pix to determine output depth and resolution  */
    pix1 = pixaGetPix(pixa1, 0, L_CLONE);
    d = pixGetDepth(pix1);
    res = pixGetXRes(pix1);
    pixDestroy(&pix1);
    if ((pixd = pixCreate(w, h, d)) == NULL) {
        pixaDestroy(&pixa1);
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    }
    pixSetBlackOrWhite(pixd, L_SET_WHITE);
    pixSetResolution(pixd, res, res);
    boxa = boxaCreate(n);

        /* Tile the output */
    index = 0;
    for (i = 0; i < nh; i++) {
        for (j = 0; j < nw && index < n; j++, index++) {
            pix1 = pixaGetPix(pixa1, index, L_CLONE);
            pixGetDimensions(pix1, &wt, &ht, NULL);
            if (wt > cellw || ht > cellh) {
                L_INFO("pix(%d) omitted; size %dx%x\n", __func__, index,
                       wt, ht);
                box = boxCreate(0, 0, 0, 0);
                boxaAddBox(boxa, box, L_INSERT);
                pixDestroy(&pix1);
                continue;
            }
            pixRasterop(pixd, j * cellw, i * cellh, wt, ht,
                        PIX_SRC, pix1, 0, 0);
            box = boxCreate(j * cellw, i * cellh, wt, ht);
            boxaAddBox(boxa, box, L_INSERT);
            pixDestroy(&pix1);
        }
    }

        /* Save the number of tiles in the text field */
    snprintf(buf, sizeof(buf), "n = %d", boxaGetCount(boxa));
    pixSetText(pixd, buf);

    if (pncols) *pncols = nw;
    if (pboxa)
        *pboxa = boxa;
    else
        boxaDestroy(&boxa);
    pixaDestroy(&pixa1);
    return pixd;
}


/*!
 * \brief   pixaDisplayUnsplit()
 *
 * \param[in]    pixa
 * \param[in]    nx           number of mosaic cells horizontally
 * \param[in]    ny           number of mosaic cells vertically
 * \param[in]    borderwidth  of added border on all sides
 * \param[in]    bordercolor  in our RGBA format: 0xrrggbbaa
 * \return  pix of tiled images, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a logical inverse of pixaSplitPix().  It
 *          constructs a pix from a mosaic of tiles, all of equal size.
 *      (2) For added generality, a border of arbitrary color can
 *          be added to each of the tiles.
 *      (3) In use, pixa will typically have either been generated
 *          from pixaSplitPix() or will derived from a pixa that
 *          was so generated.
 *      (4) All pix in the pixa must be of equal depth, and, if
 *          colormapped, have the same colormap.
 * </pre>
 */
PIX *
pixaDisplayUnsplit(PIXA     *pixa,
                   l_int32   nx,
                   l_int32   ny,
                   l_int32   borderwidth,
                   l_uint32  bordercolor)
{
l_int32  w, h, d, wt, ht;
l_int32  i, j, k, x, y, n;
PIX     *pix1, *pixd;

    if (!pixa)
        return (PIX *)ERROR_PTR("pixa not defined", __func__, NULL);
    if (nx <= 0 || ny <= 0)
        return (PIX *)ERROR_PTR("nx and ny must be > 0", __func__, NULL);
    if ((n = pixaGetCount(pixa)) == 0)
        return (PIX *)ERROR_PTR("no components", __func__, NULL);
    if (n != nx * ny)
        return (PIX *)ERROR_PTR("n != nx * ny", __func__, NULL);
    borderwidth = L_MAX(0, borderwidth);

    pixaGetPixDimensions(pixa, 0, &wt, &ht, &d);
    w = nx * (wt + 2 * borderwidth);
    h = ny * (ht + 2 * borderwidth);

    if ((pixd = pixCreate(w, h, d)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    pix1 = pixaGetPix(pixa, 0, L_CLONE);
    pixCopyColormap(pixd, pix1);
    pixDestroy(&pix1);
    if (borderwidth > 0)
        pixSetAllArbitrary(pixd, bordercolor);

    y = borderwidth;
    for (i = 0, k = 0; i < ny; i++) {
        x = borderwidth;
        for (j = 0; j < nx; j++, k++) {
            pix1 = pixaGetPix(pixa, k, L_CLONE);
            pixRasterop(pixd, x, y, wt, ht, PIX_SRC, pix1, 0, 0);
            pixDestroy(&pix1);
            x += wt + 2 * borderwidth;
        }
        y += ht + 2 * borderwidth;
    }

    return pixd;
}


/*!
 * \brief   pixaDisplayTiled()
 *
 * \param[in]    pixa
 * \param[in]    maxwidth     of output image
 * \param[in]    background   0 for white, 1 for black
 * \param[in]    spacing
 * \return  pix of tiled images, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This renders a pixa to a single image of width not to
 *          exceed maxwidth, with background color either white or black,
 *          and with each subimage spaced on a regular lattice.
 *      (2) The lattice size is determined from the largest width and height,
 *          separately, of all pix in the pixa.
 *      (3) All pix in the pixa must be of equal depth.
 *      (4) If any pix has a colormap, all pix are rendered in rgb.
 *      (5) Careful: because no components are omitted, this is
 *          dangerous if there are thousands of small components and
 *          one or more very large one, because the size of the
 *          resulting pix can be huge!
 * </pre>
 */
PIX *
pixaDisplayTiled(PIXA    *pixa,
                 l_int32  maxwidth,
                 l_int32  background,
                 l_int32  spacing)
{
l_int32  wmax, hmax, wd, hd, d, hascmap, res, same;
l_int32  i, j, n, ni, ncols, nrows;
l_int32  ystart, xstart, wt, ht;
PIX     *pix1, *pix2, *pixd;
PIXA    *pixa1;

    if (!pixa)
        return (PIX *)ERROR_PTR("pixa not defined", __func__, NULL);
    spacing = L_MAX(spacing, 0);
    if ((n = pixaGetCount(pixa)) == 0)
        return (PIX *)ERROR_PTR("no components", __func__, NULL);

        /* If any pix have colormaps, generate rgb */
    pixaAnyColormaps(pixa, &hascmap);
    if (hascmap) {
        pixa1 = pixaCreate(n);
        for (i = 0; i < n; i++) {
            pix1 = pixaGetPix(pixa, i, L_CLONE);
            pix2 = pixConvertTo32(pix1);
            pixaAddPix(pixa1, pix2, L_INSERT);
            pixDestroy(&pix1);
        }
    } else {
        pixa1 = pixaCopy(pixa, L_CLONE);
    }

        /* Find the max dimensions and depth subimages */
    pixaGetDepthInfo(pixa1, &d, &same);
    if (!same) {
        pixaDestroy(&pixa1);
        return (PIX *)ERROR_PTR("depths not equal", __func__, NULL);
    }
    pixaSizeRange(pixa1, NULL, NULL, &wmax, &hmax);

        /* Get the number of rows and columns and the output image size */
    ncols = (l_int32)((l_float32)(maxwidth - spacing) /
                      (l_float32)(wmax + spacing));
    ncols = L_MAX(ncols, 1);
    nrows = (n + ncols - 1) / ncols;
    wd = wmax * ncols + spacing * (ncols + 1);
    hd = hmax * nrows + spacing * (nrows + 1);
    if ((pixd = pixCreate(wd, hd, d)) == NULL) {
        pixaDestroy(&pixa1);
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    }

        /* Reset the background color if necessary */
    if ((background == 1 && d == 1) || (background == 0 && d != 1))
        pixSetAll(pixd);

        /* Blit the images to the dest */
    for (i = 0, ni = 0; i < nrows; i++) {
        ystart = spacing + i * (hmax + spacing);
        for (j = 0; j < ncols && ni < n; j++, ni++) {
            xstart = spacing + j * (wmax + spacing);
            pix1 = pixaGetPix(pixa1, ni, L_CLONE);
            if (ni == 0) res = pixGetXRes(pix1);
            pixGetDimensions(pix1, &wt, &ht, NULL);
            pixRasterop(pixd, xstart, ystart, wt, ht, PIX_SRC, pix1, 0, 0);
            pixDestroy(&pix1);
        }
    }
    pixSetResolution(pixd, res, res);

    pixaDestroy(&pixa1);
    return pixd;
}


/*!
 * \brief   pixaDisplayTiledInRows()
 *
 * \param[in]    pixa
 * \param[in]    outdepth     output depth: 1, 8 or 32 bpp
 * \param[in]    maxwidth     of output image
 * \param[in]    scalefactor  applied to every pix; use 1.0 for no scaling
 * \param[in]    background   0 for white, 1 for black; this is the color
 *                            of the spacing between the images
 * \param[in]    spacing      between images, and on outside
 * \param[in]    border       width of black border added to each image;
 *                            use 0 for no border
 * \return  pixd of tiled images, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This renders a pixa to a single image of width not to
 *          exceed maxwidth, with background color either white or black,
 *          and with each row tiled such that the top of each pix is
 *          aligned and separated by 'spacing' from the next one.
 *          A black border can be added to each pix.
 *      (2) All pix are converted to outdepth; existing colormaps are removed.
 *      (3) This does a reasonably spacewise-efficient job of laying
 *          out the individual pix images into a tiled composite.
 *      (4) A serialized boxa giving the location in pixd of each input
 *          pix (without added border) is stored in the text string of pixd.
 *          This allows, e.g., regeneration of a pixa from pixd, using
 *          pixaCreateFromBoxa().  If there is no scaling and the depth of
 *          each input pix in the pixa is the same, this tiling operation
 *          can be inverted using the boxa (except for loss of text in
 *          each of the input pix):
 *            pix1 = pixaDisplayTiledInRows(pixa1, 1, 1500, 1.0, 0, 30, 0);
 *            char *boxatxt = pixGetText(pix1);
 *            boxa1 = boxaReadMem((l_uint8 *)boxatxt, strlen(boxatxt));
 *            pixa2 = pixaCreateFromBoxa(pix1, boxa1, 0, 0, NULL);
 * </pre>
 */
PIX *
pixaDisplayTiledInRows(PIXA      *pixa,
                       l_int32    outdepth,
                       l_int32    maxwidth,
                       l_float32  scalefactor,
                       l_int32    background,
                       l_int32    spacing,
                       l_int32    border)
{
l_int32   h;  /* cumulative height over all the rows */
l_int32   w;  /* cumulative height in the current row */
l_int32   bordval, wtry, wt, ht;
l_int32   irow;  /* index of current pix in current row */
l_int32   wmaxrow;  /* width of the largest row */
l_int32   maxh;  /* max height in row */
l_int32   i, j, index, n, x, y, nrows, ninrow, res;
size_t    size;
l_uint8  *data;
BOXA     *boxa;
NUMA     *nainrow;  /* number of pix in the row */
NUMA     *namaxh;  /* height of max pix in the row */
PIX      *pix, *pixn, *pix1, *pixd;
PIXA     *pixan;

    if (!pixa)
        return (PIX *)ERROR_PTR("pixa not defined", __func__, NULL);
    if (outdepth != 1 && outdepth != 8 && outdepth != 32)
        return (PIX *)ERROR_PTR("outdepth not in {1, 8, 32}", __func__, NULL);
    spacing = L_MAX(spacing, 0);
    border = L_MAX(border, 0);
    if (scalefactor <= 0.0) scalefactor = 1.0;

    if ((n = pixaGetCount(pixa)) == 0)
        return (PIX *)ERROR_PTR("no components", __func__, NULL);

        /* Normalize depths, scale, remove colormaps; optionally add border */
    pixan = pixaCreate(n);
    bordval = (outdepth == 1) ? 1 : 0;
    for (i = 0; i < n; i++) {
        if ((pix = pixaGetPix(pixa, i, L_CLONE)) == NULL)
            continue;

        if (outdepth == 1)
            pixn = pixConvertTo1(pix, 128);
        else if (outdepth == 8)
            pixn = pixConvertTo8(pix, FALSE);
        else  /* outdepth == 32 */
            pixn = pixConvertTo32(pix);
        pixDestroy(&pix);

        if (scalefactor != 1.0)
            pix1 = pixScale(pixn, scalefactor, scalefactor);
        else
            pix1 = pixClone(pixn);
        if (border)
            pixd = pixAddBorder(pix1, border, bordval);
        else
            pixd = pixClone(pix1);
        pixDestroy(&pixn);
        pixDestroy(&pix1);

        pixaAddPix(pixan, pixd, L_INSERT);
    }
    if (pixaGetCount(pixan) != n) {
        n = pixaGetCount(pixan);
        L_WARNING("only got %d components\n", __func__, n);
        if (n == 0) {
            pixaDestroy(&pixan);
            return (PIX *)ERROR_PTR("no components", __func__, NULL);
        }
    }

        /* Compute parameters for layout */
    nainrow = numaCreate(0);
    namaxh = numaCreate(0);
    wmaxrow = 0;
    w = h = spacing;
    maxh = 0;  /* max height in row */
    for (i = 0, irow = 0; i < n; i++, irow++) {
        pixaGetPixDimensions(pixan, i, &wt, &ht, NULL);
        wtry = w + wt + spacing;
        if (wtry > maxwidth) {  /* end the current row and start next one */
            numaAddNumber(nainrow, irow);
            numaAddNumber(namaxh, maxh);
            wmaxrow = L_MAX(wmaxrow, w);
            h += maxh + spacing;
            irow = 0;
            w = wt + 2 * spacing;
            maxh = ht;
        } else {
            w = wtry;
            maxh = L_MAX(maxh, ht);
        }
    }

        /* Enter the parameters for the last row */
    numaAddNumber(nainrow, irow);
    numaAddNumber(namaxh, maxh);
    wmaxrow = L_MAX(wmaxrow, w);
    h += maxh + spacing;

    if ((pixd = pixCreate(wmaxrow, h, outdepth)) == NULL) {
        numaDestroy(&nainrow);
        numaDestroy(&namaxh);
        pixaDestroy(&pixan);
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    }

        /* Reset the background color if necessary */
    if ((background == 1 && outdepth == 1) ||
        (background == 0 && outdepth != 1))
        pixSetAll(pixd);

        /* Blit the images to the dest, and save the boxa identifying
         * the image regions that do not include the borders. */
    nrows = numaGetCount(nainrow);
    y = spacing;
    boxa = boxaCreate(n);
    for (i = 0, index = 0; i < nrows; i++) {  /* over rows */
        numaGetIValue(nainrow, i, &ninrow);
        numaGetIValue(namaxh, i, &maxh);
        x = spacing;
        for (j = 0; j < ninrow; j++, index++) {   /* over pix in row */
            pix = pixaGetPix(pixan, index, L_CLONE);
            if (index == 0) {
                res = pixGetXRes(pix);
                pixSetResolution(pixd, res, res);
            }
            pixGetDimensions(pix, &wt, &ht, NULL);
            boxaAddBox(boxa, boxCreate(x + border, y + border,
                wt - 2 * border, ht - 2 *border), L_INSERT);
            pixRasterop(pixd, x, y, wt, ht, PIX_SRC, pix, 0, 0);
            pixDestroy(&pix);
            x += wt + spacing;
        }
        y += maxh + spacing;
    }
    if (boxaWriteMem(&data, &size, boxa) == 0)
        pixSetText(pixd, (char *)data);  /* data is ascii */
    LEPT_FREE(data);
    boxaDestroy(&boxa);

    numaDestroy(&nainrow);
    numaDestroy(&namaxh);
    pixaDestroy(&pixan);
    return pixd;
}


/*!
 * \brief   pixaDisplayTiledInColumns()
 *
 * \param[in]    pixas
 * \param[in]    nx           number of columns in output image
 * \param[in]    scalefactor  applied to every pix; use 1.0 for no scaling
 * \param[in]    spacing      between images, and on outside; can be < 0
 * \param[in]    border       width of black border added to each image;
 *                            use 0 for no border
 * \return  pixd of tiled images, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This renders a pixa to a single image with &nx columns of
 *          subimages.  The background color is white, and each row
 *          is tiled such that the top of each pix is aligned and
 *          each pix is separated by 'spacing' from the next one.
 *          A black border can be added to each pix.
 *      (2) The output depth is determined by the largest depth
 *          required by the pix in the pixa.  Colormaps are removed.
 *      (3) A serialized boxa giving the location in pixd of each input
 *          pix (without added border) is stored in the text string of pixd.
 *          This allows, e.g., regeneration of a pixa from pixd, using
 *          pixaCreateFromBoxa().  If there is no scaling and the depth of
 *          each input pix in the pixa is the same, this tiling operation
 *          can be inverted using the boxa (except for loss of text in
 *          each of the input pix):
 *            pix1 = pixaDisplayTiledInColumns(pixa1, 3, 1.0, 0, 30, 2);
 *            char *boxatxt = pixGetText(pix1);
 *            boxa1 = boxaReadMem((l_uint8 *)boxatxt, strlen(boxatxt));
 *            pixa2 = pixaCreateFromBoxa(pix1, boxa1, NULL);
 * </pre>
 */
PIX *
pixaDisplayTiledInColumns(PIXA      *pixas,
                          l_int32    nx,
                          l_float32  scalefactor,
                          l_int32    spacing,
                          l_int32    border)
{
l_int32   i, j, index, n, x, y, nrows, wb, hb, w, h, maxd, maxh, bordval, res;
size_t    size;
l_uint8  *data;
BOX      *box;
BOXA     *boxa;
PIX      *pix1, *pix2, *pix3, *pixd;
PIXA     *pixa1, *pixa2;

    if (!pixas)
        return (PIX *)ERROR_PTR("pixas not defined", __func__, NULL);
    border = L_MAX(border, 0);
    if (scalefactor <= 0.0) scalefactor = 1.0;
    if ((n = pixaGetCount(pixas)) == 0)
        return (PIX *)ERROR_PTR("no components", __func__, NULL);

        /* Convert to same depth, if necessary */
    pixa1 = pixaConvertToSameDepth(pixas);
    pixaGetDepthInfo(pixa1, &maxd, NULL);

        /* Scale and optionally add border */
    pixa2 = pixaCreate(n);
    bordval = (maxd == 1) ? 1 : 0;
    for (i = 0; i < n; i++) {
        if ((pix1 = pixaGetPix(pixa1, i, L_CLONE)) == NULL)
            continue;
        if (scalefactor != 1.0)
            pix2 = pixScale(pix1, scalefactor, scalefactor);
        else
            pix2 = pixClone(pix1);
        if (border)
            pix3 = pixAddBorder(pix2, border, bordval);
        else
            pix3 = pixClone(pix2);
        if (i == 0) res = pixGetXRes(pix3);
        pixaAddPix(pixa2, pix3, L_INSERT);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
    }
    pixaDestroy(&pixa1);
    if (pixaGetCount(pixa2) != n) {
        n = pixaGetCount(pixa2);
        L_WARNING("only got %d components\n", __func__, n);
        if (n == 0) {
            pixaDestroy(&pixa2);
            return (PIX *)ERROR_PTR("no components", __func__, NULL);
        }
    }

        /* Compute layout parameters and save as a boxa */
    boxa = boxaCreate(n);
    nrows = (n + nx - 1) / nx;
    y = spacing;
    for (i = 0, index = 0; i < nrows; i++) {
        x = spacing;
        maxh = 0;
        for (j = 0; j < nx && index < n; j++) {
            pixaGetPixDimensions(pixa2, index, &wb, &hb, NULL);
            box = boxCreate(x, y, wb, hb);
            boxaAddBox(boxa, box, L_INSERT);
            maxh = L_MAX(maxh, hb + spacing);
            x += wb + spacing;
            index++;
        }
        y += maxh;
    }
    pixaSetBoxa(pixa2, boxa, L_INSERT);

        /* Render the output pix */
    boxaGetExtent(boxa, &w, &h, NULL);
    pixd = pixaDisplay(pixa2, w + spacing, h + spacing);
    pixSetResolution(pixd, res, res);

        /* Save the boxa in the text field of the output pix */
    if (boxaWriteMem(&data, &size, boxa) == 0)
        pixSetText(pixd, (char *)data);  /* data is ascii */
    LEPT_FREE(data);

    pixaDestroy(&pixa2);
    return pixd;
}


/*!
 * \brief   pixaDisplayTiledAndScaled()
 *
 * \param[in]    pixa
 * \param[in]    outdepth    output depth: 1, 8 or 32 bpp
 * \param[in]    tilewidth   each pix is scaled to this width
 * \param[in]    ncols       number of tiles in each row
 * \param[in]    background  0 for white, 1 for black; this is the color
 *                           of the spacing between the images
 * \param[in]    spacing     between images, and on outside
 * \param[in]    border      width of additional black border on each image;
 *                           use 0 for no border
 * \return  pix of tiled images, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This can be used to tile a number of renderings of
 *          an image that are at different scales and depths.
 *      (2) Each image, after scaling and optionally adding the
 *          black border, has width 'tilewidth'.  Thus, the border does
 *          not affect the spacing between the image tiles.  The
 *          maximum allowed border width is tilewidth / 5.
 * </pre>
 */
PIX *
pixaDisplayTiledAndScaled(PIXA    *pixa,
                          l_int32  outdepth,
                          l_int32  tilewidth,
                          l_int32  ncols,
                          l_int32  background,
                          l_int32  spacing,
                          l_int32  border)
{
l_int32    x, y, w, h, wd, hd, d, res;
l_int32    i, n, nrows, maxht, ninrow, irow, bordval;
l_int32   *rowht;
l_float32  scalefact;
PIX       *pix, *pixn, *pix1, *pixb, *pixd;
PIXA      *pixan;

    if (!pixa)
        return (PIX *)ERROR_PTR("pixa not defined", __func__, NULL);
    if (outdepth != 1 && outdepth != 8 && outdepth != 32)
        return (PIX *)ERROR_PTR("outdepth not in {1, 8, 32}", __func__, NULL);
    if (ncols <= 0)
        return (PIX *)ERROR_PTR("ncols must be > 0", __func__, NULL);
    spacing = L_MAX(spacing, 0);
    if (border < 0 || border > tilewidth / 5)
        border = 0;
    if ((n = pixaGetCount(pixa)) == 0)
        return (PIX *)ERROR_PTR("no components", __func__, NULL);

        /* Normalize scale and depth for each pix; optionally add border */
    pixan = pixaCreate(n);
    bordval = (outdepth == 1) ? 1 : 0;
    for (i = 0; i < n; i++) {
        if ((pix = pixaGetPix(pixa, i, L_CLONE)) == NULL)
            continue;

        pixGetDimensions(pix, &w, &h, &d);
        scalefact = (l_float32)(tilewidth - 2 * border) / (l_float32)w;
        if (d == 1 && outdepth > 1 && scalefact < 1.0)
            pix1 = pixScaleToGray(pix, scalefact);
        else
            pix1 = pixScale(pix, scalefact, scalefact);

        if (outdepth == 1)
            pixn = pixConvertTo1(pix1, 128);
        else if (outdepth == 8)
            pixn = pixConvertTo8(pix1, FALSE);
        else  /* outdepth == 32 */
            pixn = pixConvertTo32(pix1);
        pixDestroy(&pix1);

        if (border)
            pixb = pixAddBorder(pixn, border, bordval);
        else
            pixb = pixClone(pixn);

        pixaAddPix(pixan, pixb, L_INSERT);
        pixDestroy(&pix);
        pixDestroy(&pixn);
    }
    if ((n = pixaGetCount(pixan)) == 0) { /* should not have changed! */
        pixaDestroy(&pixan);
        return (PIX *)ERROR_PTR("no components", __func__, NULL);
    }

        /* Determine the size of each row and of pixd */
    wd = tilewidth * ncols + spacing * (ncols + 1);
    nrows = (n + ncols - 1) / ncols;
    if ((rowht = (l_int32 *)LEPT_CALLOC(nrows, sizeof(l_int32))) == NULL) {
        pixaDestroy(&pixan);
        return (PIX *)ERROR_PTR("rowht array not made", __func__, NULL);
    }
    maxht = 0;
    ninrow = 0;
    irow = 0;
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixan, i, L_CLONE);
        ninrow++;
        pixGetDimensions(pix, &w, &h, NULL);
        maxht = L_MAX(h, maxht);
        if (ninrow == ncols) {
            rowht[irow] = maxht;
            maxht = ninrow = 0;  /* reset */
            irow++;
        }
        pixDestroy(&pix);
    }
    if (ninrow > 0) {   /* last fencepost */
        rowht[irow] = maxht;
        irow++;  /* total number of rows */
    }
    nrows = irow;
    hd = spacing * (nrows + 1);
    for (i = 0; i < nrows; i++)
        hd += rowht[i];

    pixd = pixCreate(wd, hd, outdepth);
    if ((background == 1 && outdepth == 1) ||
        (background == 0 && outdepth != 1))
        pixSetAll(pixd);

        /* Now blit images to pixd */
    x = y = spacing;
    irow = 0;
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixan, i, L_CLONE);
        if (i == 0) {
            res = pixGetXRes(pix);
            pixSetResolution(pixd, res, res);
        }
        pixGetDimensions(pix, &w, &h, NULL);
        if (i && ((i % ncols) == 0)) {  /* start new row */
            x = spacing;
            y += spacing + rowht[irow];
            irow++;
        }
        pixRasterop(pixd, x, y, w, h, PIX_SRC, pix, 0, 0);
        x += tilewidth + spacing;
        pixDestroy(&pix);
    }

    pixaDestroy(&pixan);
    LEPT_FREE(rowht);
    return pixd;
}


/*!
 * \brief   pixaDisplayTiledWithText()
 *
 * \param[in]    pixa
 * \param[in]    maxwidth     of output image
 * \param[in]    scalefactor  applied to every pix; use 1.0 for no scaling
 * \param[in]    spacing      between images, and on outside
 * \param[in]    border       width of black border added to each image;
 *                            use 0 for no border
 * \param[in]    fontsize     4, 6, ... 20
 * \param[in]    textcolor    0xrrggbb00
 * \return  pixd of tiled images, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a version of pixaDisplayTiledInRows() that prints, below
 *          each pix, the text in the pix text field.  Up to 127 chars
 *          of text in the pix text field are rendered below each pix.
 *      (2) It renders a pixa to a single image of width not to
 *          exceed %maxwidth, with white background color, with each row
 *          tiled such that the top of each pix is aligned and separated
 *          by %spacing from the next one.
 *      (3) All pix are converted to 32 bpp.
 *      (4) This does a reasonably spacewise-efficient job of laying
 *          out the individual pix images into a tiled composite.
 * </pre>
 */
PIX *
pixaDisplayTiledWithText(PIXA      *pixa,
                         l_int32    maxwidth,
                         l_float32  scalefactor,
                         l_int32    spacing,
                         l_int32    border,
                         l_int32    fontsize,
                         l_uint32   textcolor)
{
char      buf[128];
char     *textstr;
l_int32   i, n, maxw;
L_BMF    *bmf;
PIX      *pix1, *pix2, *pix3, *pix4, *pixd;
PIXA     *pixad;

    if (!pixa)
        return (PIX *)ERROR_PTR("pixa not defined", __func__, NULL);
    if ((n = pixaGetCount(pixa)) == 0)
        return (PIX *)ERROR_PTR("no components", __func__, NULL);
    if (maxwidth <= 0)
        return (PIX *)ERROR_PTR("invalid maxwidth", __func__, NULL);
    spacing = L_MAX(spacing, 0);
    border = L_MAX(border, 0);
    if (scalefactor <= 0.0) scalefactor = 1.0;
    if (fontsize < 4 || fontsize > 20 || (fontsize & 1)) {
        l_int32 fsize = L_MAX(L_MIN(fontsize, 20), 4);
        if (fsize & 1) fsize--;
        L_WARNING("changed fontsize from %d to %d\n", __func__,
                  fontsize, fsize);
        fontsize = fsize;
    }

        /* Be sure the width can accommodate a single column of images */
    pixaSizeRange(pixa, NULL, NULL, &maxw, NULL);
    maxwidth = L_MAX(maxwidth, scalefactor * (maxw + 2 * spacing + 2 * border));

    bmf = bmfCreate(NULL, fontsize);
    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixa, i, L_CLONE);
        pix2 = pixConvertTo32(pix1);
        pix3 = pixAddBorderGeneral(pix2, spacing / 2, spacing / 2, spacing / 2,
                                   spacing / 2, 0xffffff00);
        textstr = pixGetText(pix1);
        if (textstr && strlen(textstr) > 0) {
            snprintf(buf, sizeof(buf), "%s", textstr);
            pix4 = pixAddSingleTextblock(pix3, bmf, buf, textcolor,
                                     L_ADD_BELOW, NULL);
        } else {
            pix4 = pixClone(pix3);
        }
        pixaAddPix(pixad, pix4, L_INSERT);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
        pixDestroy(&pix3);
    }
    bmfDestroy(&bmf);

    pixd = pixaDisplayTiledInRows(pixad, 32, maxwidth, scalefactor,
                                  0, spacing, border);
    pixaDestroy(&pixad);
    return pixd;
}


/*!
 * \brief   pixaDisplayTiledByIndex()
 *
 * \param[in]    pixa
 * \param[in]    na         numa with indices corresponding to the pix in pixa
 * \param[in]    width      each pix is scaled to this width
 * \param[in]    spacing    between images, and on outside
 * \param[in]    border     width of black border added to each image;
 *                          use 0 for no border
 * \param[in]    fontsize   4, 6, ... 20
 * \param[in]    textcolor  0xrrggbb00
 * \return  pixd of tiled images, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This renders a pixa to a single image with white
 *          background color, where the pix are placed in columns
 *          given by the index value in the numa.  Each pix
 *          is separated by %spacing from the adjacent ones, and
 *          an optional border is placed around them.
 *      (2) Up to 127 chars of text in the pix text field are rendered
 *          below each pix.  Use newlines in the text field to write
 *          the text in multiple lines that fit within the pix width.
 *      (3) To avoid having empty columns, if there are N different
 *          index values, they should be in [0 ... N-1].
 *      (4) All pix are converted to 32 bpp.
 * </pre>
 */
PIX *
pixaDisplayTiledByIndex(PIXA     *pixa,
                        NUMA     *na,
                        l_int32   width,
                        l_int32   spacing,
                        l_int32   border,
                        l_int32   fontsize,
                        l_uint32  textcolor)
{
char      buf[128];
char     *textstr;
l_int32    i, n, x, y, w, h, yval, index;
l_float32  maxindex;
L_BMF     *bmf;
BOX       *box;
NUMA      *nay;  /* top of the next pix to add in that column */
PIX       *pix1, *pix2, *pix3, *pix4, *pix5, *pixd;
PIXA      *pixad;

    if (!pixa)
        return (PIX *)ERROR_PTR("pixa not defined", __func__, NULL);
    if (!na)
        return (PIX *)ERROR_PTR("na not defined", __func__, NULL);
    if ((n = pixaGetCount(pixa)) == 0)
        return (PIX *)ERROR_PTR("no pixa components", __func__, NULL);
    if (n != numaGetCount(na))
        return (PIX *)ERROR_PTR("pixa and na counts differ", __func__, NULL);
    if (width <= 0)
        return (PIX *)ERROR_PTR("invalid width", __func__, NULL);
    if (width < 20)
        L_WARNING("very small width: %d\n", __func__, width);
    spacing = L_MAX(spacing, 0);
    border = L_MAX(border, 0);
    if (fontsize < 4 || fontsize > 20 || (fontsize & 1)) {
        l_int32 fsize = L_MAX(L_MIN(fontsize, 20), 4);
        if (fsize & 1) fsize--;
        L_WARNING("changed fontsize from %d to %d\n", __func__,
                  fontsize, fsize);
        fontsize = fsize;
    }

        /* The pix will be rendered in the order they occupy in pixa. */
    bmf = bmfCreate(NULL, fontsize);
    pixad = pixaCreate(n);
    numaGetMax(na, &maxindex, NULL);
    nay = numaMakeConstant(spacing, lept_roundftoi(maxindex) + 1);
    for (i = 0; i < n; i++) {
        numaGetIValue(na, i, &index);
        numaGetIValue(nay, index, &yval);
        pix1 = pixaGetPix(pixa, i, L_CLONE);
        pix2 = pixConvertTo32(pix1);
        pix3 = pixScaleToSize(pix2, width, 0);
        pix4 = pixAddBorderGeneral(pix3, border, border, border, border, 0);
        textstr = pixGetText(pix1);
        if (textstr && strlen(textstr) > 0) {
            snprintf(buf, sizeof(buf), "%s", textstr);
            pix5 = pixAddTextlines(pix4, bmf, textstr, textcolor, L_ADD_BELOW);
        } else {
            pix5 = pixClone(pix4);
        }
        pixaAddPix(pixad, pix5, L_INSERT);
        x = spacing + border + index * (2 * border + width + spacing);
        y = yval;
        pixGetDimensions(pix5, &w, &h, NULL);
        yval += h + spacing;
        numaSetValue(nay, index, yval);
        box = boxCreate(x, y, w, h);
        pixaAddBox(pixad, box, L_INSERT);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
        pixDestroy(&pix3);
        pixDestroy(&pix4);
    }
    numaDestroy(&nay);
    bmfDestroy(&bmf);

    pixd = pixaDisplay(pixad, 0, 0);
    pixaDestroy(&pixad);
    return pixd;
}


/*---------------------------------------------------------------------*
 *                         Pixa pair display                           *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaDisplayPairTiledInColumns()
 *
 * \param[in]    pixas1
 * \param[in]    pixas2
 * \param[in]    nx           number of columns in output image
 * \param[in]    scalefactor  applied to every pix; use 1.0 for no scaling
 * \param[in]    spacing1     between images within a pair
 * \param[in]    spacing2     between image pairs, and on outside
 * \param[in]    border1      width of black border added to each image;
 *                            use 0 for no border
 * \param[in]    border2      width of black border added to each image pair.
 *                            use 0 for no border
 * \param[in]    fontsize     to print index below each pair. Valid set is
 *                            {4,6,8,10,12,14,16,18,20}.  Use 0 to disable.
 * \param[in]    startindex   index for the first pair; ignore if %fontsize= 0
 * \param[in]    sa           [optional] array of text strings to display
 * \return  pixd of tiled images, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This renders a pair of pixa in a single image with &nx columns of
 *          tiled pairs.  The background color is white, and each row
 *          is tiled such that the top of each pix is aligned.
 *          The pix are displayed in pairs, taken from the input pixas.
 *          Input %pixas1 and %pixas2 must have the same count of pix.
 *      (2) If %fontsize != 0, text is displayed below each pair, and the
 *          output depth is 32 bpp.  If %sa is defined, the text is taken
 *          sequentially from %sa; otherwise, an integer is displayed with
 *          numbers chosen consecutively starting with %startindex.
 *      (3) If %fontsize == 0, the output depth is determined by the largest
 *          depth required by the pix in the pixa.  Colormaps are removed.
 *      (4) Start with these values and tune for aesthetics:
 *            %nx = 5, %spacing1 = %spacing2 = 15, %border1 = %border2 = 2,
 *            %fontsize = 8.
 * </pre>
 */
PIX *
pixaDisplayPairTiledInColumns(PIXA      *pixas1,
                              PIXA      *pixas2,
                              l_int32    nx,
                              l_float32  scalefactor,
                              l_int32    spacing1,
                              l_int32    spacing2,
                              l_int32    border1,
                              l_int32    border2,
                              l_int32    fontsize,
                              l_int32    startindex,
                              SARRAY    *sa)
{
l_int32  i, n, w, maxd, maxd1, maxd2, text;
NUMA    *na;
PIX     *pixs1, *pixs2, *pix1, *pix2, *pix3, *pix4;
PIX     *pix5, *pix6, *pix7, *pix8, *pix9;
PIXA    *pixa1, *pixa2;
SARRAY  *sa1;

    if (!pixas1)
        return (PIX *)ERROR_PTR("pixas1 not defined", __func__, NULL);
    if (!pixas2)
        return (PIX *)ERROR_PTR("pixas2 not defined", __func__, NULL);
    spacing1 = L_MAX(spacing1, 0);
    spacing2 = L_MAX(spacing2, 0);
    border1 = L_MAX(border1, 0);
    border2 = L_MAX(border2, 0);
    if (scalefactor <= 0.0) scalefactor = 1.0;
    if ((n = pixaGetCount(pixas1)) == 0)
        return (PIX *)ERROR_PTR("no components", __func__, NULL);
    if (n != pixaGetCount(pixas2))
        return (PIX *)ERROR_PTR("pixa sizes differ", __func__, NULL);
    text = (fontsize <= 0) ? 0 : 1;
    if (text && (fontsize < 4 || fontsize > 20 || (fontsize & 1))) {
        l_int32 fsize = L_MAX(L_MIN(fontsize, 20), 4);
        if (fsize & 1) fsize--;
        L_WARNING("changed fontsize from %d to %d\n", __func__,
                  fontsize, fsize);
        fontsize = fsize;
    }

        /* Convert to same depth, if necessary */
    if (text) {  /* adding color text; convert to 32 bpp */
        maxd = 32;
    } else {
        pixaGetRenderingDepth(pixas1, &maxd1);
        pixaGetRenderingDepth(pixas2, &maxd2);
        maxd = L_MAX(maxd1, maxd2);
    }

        /* Optionally scale and add borders to each pair;
           then combine the pairs and add outer border.  */
    pixa1 = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pixs1 = pixaGetPix(pixas1, i, L_CLONE);
        pixs2 = pixaGetPix(pixas2, i, L_CLONE);
        if (!pixs1 || !pixs2) continue;
        if (maxd == 1) {
            pix1 = pixClone(pixs1);
            pix2 = pixClone(pixs2);
        } else if (maxd == 8) {
            pix1 = pixConvertTo8(pixs1, 0);
            pix2 = pixConvertTo8(pixs2, 0);
        } else {  /* maxd == 32 */
            pix1 = pixConvertTo32(pixs1);
            pix2 = pixConvertTo32(pixs2);
        }
        pixDestroy(&pixs1);
        pixDestroy(&pixs2);
        if (scalefactor != 1.0) {
            pix3 = pixScale(pix1, scalefactor, scalefactor);
            pix4 = pixScale(pix2, scalefactor, scalefactor);
        } else {
            pix3 = pixClone(pix1);
            pix4 = pixClone(pix2);
        }
        pixDestroy(&pix1);
        pixDestroy(&pix2);
        if (border1) {
            pix5 = pixAddBlackOrWhiteBorder(pix3, border1, border1, border1,
                                            border1, L_GET_BLACK_VAL);
            pix6 = pixAddBlackOrWhiteBorder(pix4, border1, border1, border1,
                                            border1, L_GET_BLACK_VAL);
        } else {
            pix5 = pixClone(pix3);
            pix6 = pixClone(pix4);
        }
        pixDestroy(&pix3);
        pixDestroy(&pix4);
        if (spacing1) {  /* white border */
            pix7 = pixAddBlackOrWhiteBorder(pix5, spacing1 / 2, spacing1 / 2,
                                spacing1 / 2, spacing1 / 2, L_GET_WHITE_VAL);
            pix8 = pixAddBlackOrWhiteBorder(pix6, spacing1 / 2, spacing1 / 2,
                                spacing1 / 2, spacing1 / 2, L_GET_WHITE_VAL);
        } else {
            pix7 = pixClone(pix5);
            pix8 = pixClone(pix6);
        }
        pixDestroy(&pix5);
        pixDestroy(&pix6);
        pixa2 = pixaCreate(2);
        pixaAddPix(pixa2, pix7, L_INSERT);
        pixaAddPix(pixa2, pix8, L_INSERT);
        pix9 = pixaDisplayTiledInColumns(pixa2, 2, 1.0, 0, 0);
        pixaAddPix(pixa1, pix9, L_INSERT);
        pixaDestroy(&pixa2);
    }

    if (!text) {
        pix1 = pixaDisplayTiledInColumns(pixa1, nx, 1.0, spacing2, border2);
    } else {
        if (sa) {
            pixaSetText(pixa1, NULL, sa);
        } else {
            n = pixaGetCount(pixa1);
            na = numaMakeSequence(startindex, 1, n);
            sa1 = numaConvertToSarray(na, 4, 0, 0, L_INTEGER_VALUE);
            pixaSetText(pixa1, NULL, sa1);
            numaDestroy(&na);
            sarrayDestroy(&sa1);
        }
        pixaSizeRange(pixa1, NULL, NULL, &w, NULL);
        pix1 = pixaDisplayTiledWithText(pixa1, w * (nx + 1), 1.0, spacing2,
                                        border2, fontsize, 0xff000000);
    }
    pixaDestroy(&pixa1);
    return pix1;
}


/*---------------------------------------------------------------------*
 *                              Pixaa Display                          *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaaDisplay()
 *
 * \param[in]    paa
 * \param[in]    w, h   if set to 0, the size is determined from the
 *                      bounding box of the components in pixa
 * \return  pix, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Each pix of the paa is displayed at the location given by
 *          its box, translated by the box of the containing pixa
 *          if it exists.
 * </pre>
 */
PIX *
pixaaDisplay(PIXAA   *paa,
             l_int32  w,
             l_int32  h)
{
l_int32  i, j, n, nbox, na, d, wmax, hmax, x, y, xb, yb, wb, hb;
BOXA    *boxa1;  /* top-level boxa */
BOXA    *boxa;
PIX     *pix1, *pixd;
PIXA    *pixa;

    if (!paa)
        return (PIX *)ERROR_PTR("paa not defined", __func__, NULL);

    n = pixaaGetCount(paa, NULL);
    if (n == 0)
        return (PIX *)ERROR_PTR("no components", __func__, NULL);

        /* If w and h not input, determine the minimum size required
         * to contain the origin and all c.c. */
    boxa1 = pixaaGetBoxa(paa, L_CLONE);
    nbox = boxaGetCount(boxa1);
    if (w == 0 || h == 0) {
        if (nbox == n) {
            boxaGetExtent(boxa1, &w, &h, NULL);
        } else {  /* have to use the lower-level boxa for each pixa */
            wmax = hmax = 0;
            for (i = 0; i < n; i++) {
                pixa = pixaaGetPixa(paa, i, L_CLONE);
                boxa = pixaGetBoxa(pixa, L_CLONE);
                boxaGetExtent(boxa, &w, &h, NULL);
                wmax = L_MAX(wmax, w);
                hmax = L_MAX(hmax, h);
                pixaDestroy(&pixa);
                boxaDestroy(&boxa);
            }
            w = wmax;
            h = hmax;
        }
    }

        /* Get depth from first pix */
    pixa = pixaaGetPixa(paa, 0, L_CLONE);
    pix1 = pixaGetPix(pixa, 0, L_CLONE);
    d = pixGetDepth(pix1);
    pixaDestroy(&pixa);
    pixDestroy(&pix1);

    if ((pixd = pixCreate(w, h, d)) == NULL) {
        boxaDestroy(&boxa1);
        return (PIX *)ERROR_PTR("pixd not made", __func__, NULL);
    }

    x = y = 0;
    for (i = 0; i < n; i++) {
        pixa = pixaaGetPixa(paa, i, L_CLONE);
        if (nbox == n)
            boxaGetBoxGeometry(boxa1, i, &x, &y, NULL, NULL);
        na = pixaGetCount(pixa);
        for (j = 0; j < na; j++) {
            pixaGetBoxGeometry(pixa, j, &xb, &yb, &wb, &hb);
            pix1 = pixaGetPix(pixa, j, L_CLONE);
            pixRasterop(pixd, x + xb, y + yb, wb, hb, PIX_PAINT, pix1, 0, 0);
            pixDestroy(&pix1);
        }
        pixaDestroy(&pixa);
    }
    boxaDestroy(&boxa1);

    return pixd;
}


/*!
 * \brief   pixaaDisplayByPixa()
 *
 * \param[in]    paa
 * \param[in]    maxnx        maximum number of columns for rendering each pixa
 * \param[in]    scalefactor  applied to every pix; use 1.0 for no scaling
 * \param[in]    hspacing     between images on a row (in the pixa)
 * \param[in]    vspacing     between tiles rows, each corresponding to a pixa
 * \param[in]    border       width of black border added to each image;
 *                            use 0 for no border
 * \return  pixd of images in %paa, tiled by pixa in row-major order
 *
 * <pre>
 * Notes:
 *      (1) This renders a pixaa into a single image.  The pix from each pixa
 *          are rendered on a row.  If the number of pix in the pixa is
 *          larger than %maxnx, the pix will be rendered into more than 1 row.
 *          To insure that each pixa is rendered into one row, use %maxnx
 *          at least as large as the max number of pix in the pixa.
 *      (2) Each row is tiled such that the top of each pix is aligned and
 *          each pix is separated by %hspacing from the next one.
 *          A black border can be added to each pix.
 *      (3) The resulting pix from each row are then rendered vertically,
 *          separated by %vspacing from each other.
 *      (4) The output depth is determined by the largest depth of all
 *          the pix in %paa. Colormaps are removed.
 * </pre>
 */
PIX *
pixaaDisplayByPixa(PIXAA     *paa,
                   l_int32    maxnx,
                   l_float32  scalefactor,
                   l_int32    hspacing,
                   l_int32    vspacing,
                   l_int32    border)
{
l_int32  i, n, vs;
PIX     *pix1, *pix2;
PIXA    *pixa1, *pixa2;

    if (!paa)
        return (PIX *)ERROR_PTR("paa not defined", __func__, NULL);
    if (scalefactor <= 0.0) scalefactor = 1.0;
    if (hspacing < 0) hspacing = 0;
    if (vspacing < 0) vspacing = 0;
    if (border < 0) border = 0;

    if ((n = pixaaGetCount(paa, NULL)) == 0)
        return (PIX *)ERROR_PTR("no components", __func__, NULL);

        /* Vertical spacing of amount %hspacing is also added at this step */
    pixa2 = pixaCreate(0);
    for (i = 0; i < n; i++) {
        pixa1 = pixaaGetPixa(paa, i, L_CLONE);
        pix1 = pixaDisplayTiledInColumns(pixa1, maxnx, scalefactor,
                                         hspacing, border);
        pixaAddPix(pixa2, pix1, L_INSERT);
        pixaDestroy(&pixa1);
    }

    vs = vspacing - 2 * hspacing;
    pix2 = pixaDisplayTiledInColumns(pixa2, 1, scalefactor, vs, 0);
    pixaDestroy(&pixa2);
    return pix2;
}


/*!
 * \brief   pixaaDisplayTiledAndScaled()
 *
 * \param[in]    paa
 * \param[in]    outdepth    output depth: 1, 8 or 32 bpp
 * \param[in]    tilewidth   each pix is scaled to this width
 * \param[in]    ncols       number of tiles in each row
 * \param[in]    background  0 for white, 1 for black; this is the color
 *                           of the spacing between the images
 * \param[in]    spacing     between images, and on outside
 * \param[in]    border      width of additional black border on each image;
 *                           use 0 for no border
 * \return  pixa of tiled images, one image for each pixa in
 *                    the paa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For each pixa, this generates from all the pix a
 *          tiled/scaled output pix, and puts it in the output pixa.
 *      (2) See comments in pixaDisplayTiledAndScaled().
 * </pre>
 */
PIXA *
pixaaDisplayTiledAndScaled(PIXAA   *paa,
                           l_int32  outdepth,
                           l_int32  tilewidth,
                           l_int32  ncols,
                           l_int32  background,
                           l_int32  spacing,
                           l_int32  border)
{
l_int32  i, n;
PIX     *pix;
PIXA    *pixa, *pixad;

    if (!paa)
        return (PIXA *)ERROR_PTR("paa not defined", __func__, NULL);
    if (outdepth != 1 && outdepth != 8 && outdepth != 32)
        return (PIXA *)ERROR_PTR("outdepth not in {1, 8, 32}", __func__, NULL);
    if (ncols <= 0)
        return (PIXA *)ERROR_PTR("ncols must be > 0", __func__, NULL);
    if (border < 0 || border > tilewidth / 5)
        border = 0;

    if ((n = pixaaGetCount(paa, NULL)) == 0)
        return (PIXA *)ERROR_PTR("no components", __func__, NULL);

    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pixa = pixaaGetPixa(paa, i, L_CLONE);
        pix = pixaDisplayTiledAndScaled(pixa, outdepth, tilewidth, ncols,
                                        background, spacing, border);
        pixaAddPix(pixad, pix, L_INSERT);
        pixaDestroy(&pixa);
    }

    return pixad;
}


/*---------------------------------------------------------------------*
 *         Conversion of all pix to specified type (e.g., depth)       *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaConvertTo1()
 *
 * \param[in]    pixas
 * \param[in]    thresh    threshold for final binarization from 8 bpp gray
 * \return  pixad, or NULL on error
 */
PIXA *
pixaConvertTo1(PIXA    *pixas,
               l_int32  thresh)
{
l_int32  i, n;
BOXA    *boxa;
PIX     *pix1, *pix2;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);

    n = pixaGetCount(pixas);
    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        pix2 = pixConvertTo1(pix1, thresh);
        pixaAddPix(pixad, pix2, L_INSERT);
        pixDestroy(&pix1);
    }

    boxa = pixaGetBoxa(pixas, L_COPY);
    pixaSetBoxa(pixad, boxa, L_INSERT);
    return pixad;
}


/*!
 * \brief   pixaConvertTo8()
 *
 * \param[in]    pixas
 * \param[in]    cmapflag   1 to give pixd a colormap; 0 otherwise
 * \return  pixad each pix is 8 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See notes for pixConvertTo8(), applied to each pix in pixas.
 * </pre>
 */
PIXA *
pixaConvertTo8(PIXA    *pixas,
               l_int32  cmapflag)
{
l_int32  i, n;
BOXA    *boxa;
PIX     *pix1, *pix2;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);

    n = pixaGetCount(pixas);
    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        pix2 = pixConvertTo8(pix1, cmapflag);
        pixaAddPix(pixad, pix2, L_INSERT);
        pixDestroy(&pix1);
    }

    boxa = pixaGetBoxa(pixas, L_COPY);
    pixaSetBoxa(pixad, boxa, L_INSERT);
    return pixad;
}


/*!
 * \brief   pixaConvertTo8Colormap()
 *
 * \param[in]    pixas
 * \param[in]    dither   1 to dither if necessary; 0 otherwise
 * \return  pixad each pix is 8 bpp, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See notes for pixConvertTo8Colormap(), applied to each pix in pixas.
 * </pre>
 */
PIXA *
pixaConvertTo8Colormap(PIXA    *pixas,
                       l_int32  dither)
{
l_int32  i, n;
BOXA    *boxa;
PIX     *pix1, *pix2;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);

    n = pixaGetCount(pixas);
    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        pix2 = pixConvertTo8Colormap(pix1, dither);
        pixaAddPix(pixad, pix2, L_INSERT);
        pixDestroy(&pix1);
    }

    boxa = pixaGetBoxa(pixas, L_COPY);
    pixaSetBoxa(pixad, boxa, L_INSERT);
    return pixad;
}


/*!
 * \brief   pixaConvertTo32()
 *
 * \param[in]    pixas
 * \return  pixad 32 bpp rgb, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See notes for pixConvertTo32(), applied to each pix in pixas.
 *      (2) This can be used to allow 1 bpp pix in a pixa to be displayed
 *          with color.
 * </pre>
 */
PIXA *
pixaConvertTo32(PIXA  *pixas)
{
l_int32  i, n;
BOXA    *boxa;
PIX     *pix1, *pix2;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);

    n = pixaGetCount(pixas);
    pixad = pixaCreate(n);
    for (i = 0; i < n; i++) {
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        pix2 = pixConvertTo32(pix1);
        pixaAddPix(pixad, pix2, L_INSERT);
        pixDestroy(&pix1);
    }

    boxa = pixaGetBoxa(pixas, L_COPY);
    pixaSetBoxa(pixad, boxa, L_INSERT);
    return pixad;
}


/*---------------------------------------------------------------------*
 *                        Pixa constrained selection                   *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaConstrainedSelect()
 *
 * \param[in]    pixas
 * \param[in]    first      first index to choose; >= 0
 * \param[in]    last       biggest possible index to reach;
 *                          use -1 to go to the end; otherwise, last >= first
 * \param[in]    nmax       maximum number of pix to select; > 0
 * \param[in]    use_pairs  1 = select pairs of adjacent pix;
 *                          0 = select individual pix
 * \param[in]    copyflag   L_COPY, L_CLONE
 * \return  pixad if OK, NULL on error
 *
 * <pre>
 * Notes:
 *     (1) See notes in genConstrainedNumaInRange() for how selection
 *         is made.
 *     (2) This returns a selection of the pix in the input pixa.
 *     (3) Use copyflag == L_COPY if you don't want changes in the pix
 *         in the returned pixa to affect those in the input pixa.
 * </pre>
 */
PIXA *
pixaConstrainedSelect(PIXA    *pixas,
                      l_int32  first,
                      l_int32  last,
                      l_int32  nmax,
                      l_int32  use_pairs,
                      l_int32  copyflag)
{
l_int32  i, n, nselect, index;
NUMA    *na;
PIX     *pix1;
PIXA    *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    n = pixaGetCount(pixas);
    first = L_MAX(0, first);
    last = (last < 0) ? n - 1 : L_MIN(n - 1, last);
    if (last < first)
        return (PIXA *)ERROR_PTR("last < first!", __func__, NULL);
    if (nmax < 1)
        return (PIXA *)ERROR_PTR("nmax < 1!", __func__, NULL);

    na = genConstrainedNumaInRange(first, last, nmax, use_pairs);
    nselect = numaGetCount(na);
    pixad = pixaCreate(nselect);
    for (i = 0; i < nselect; i++) {
        numaGetIValue(na, i, &index);
        pix1 = pixaGetPix(pixas, index, copyflag);
        pixaAddPix(pixad, pix1, L_INSERT);
    }
    numaDestroy(&na);
    return pixad;
}


/*!
 * \brief   pixaSelectToPdf()
 *
 * \param[in]    pixas
 * \param[in]    first     first index to choose; >= 0
 * \param[in]    last      biggest possible index to reach;
 *                         use -1 to go to the end; otherwise, last >= first
 * \param[in]    res       override the resolution of each input image, in ppi;
 *                         use 0 to respect the resolution embedded in the input
 * \param[in]    scalefactor   scaling factor applied to each image; > 0.0
 * \param[in]    type      encoding type (L_JPEG_ENCODE, L_G4_ENCODE,
 *                         L_FLATE_ENCODE, or 0 for default
 * \param[in]    quality   used for JPEG only; 0 for default (75)
 * \param[in]    color     of numbers added to each image (e.g., 0xff000000)
 * \param[in]    fontsize  to print number below each image.  The valid set
 *                         is {4,6,8,10,12,14,16,18,20}.  Use 0 to disable.
 * \param[in]    fileout   pdf file of all images
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This writes a pdf of the selected images from %pixas, one to
 *          a page.  They are optionally scaled and annotated with the
 *          index printed to the left of the image.
 *      (2) If the input images are 1 bpp and you want the numbers to be
 *          in color, first promote each pix to 8 bpp with a colormap:
 *                pixa1 = pixaConvertTo8(pixas, 1);
 *          and then call this function with the specified color
 * </pre>
 */
l_ok
pixaSelectToPdf(PIXA        *pixas,
                l_int32      first,
                l_int32      last,
                l_int32      res,
                l_float32    scalefactor,
                l_int32      type,
                l_int32      quality,
                l_uint32     color,
                l_int32      fontsize,
                const char  *fileout)
{
l_int32  n;
L_BMF   *bmf;
NUMA    *na;
PIXA    *pixa1, *pixa2;

    if (!pixas)
        return ERROR_INT("pixas not defined", __func__, 1);
    if (type < 0 || type > L_FLATE_ENCODE) {
        L_WARNING("invalid compression type; using default\n", __func__);
        type = 0;
    }
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);

        /* Select from given range */
    n = pixaGetCount(pixas);
    first = L_MAX(0, first);
    last = (last < 0) ? n - 1 : L_MIN(n - 1, last);
    if (first > last) {
        L_ERROR("first = %d > last = %d\n", __func__, first, last);
        return 1;
    }
    pixa1 = pixaSelectRange(pixas, first, last, L_CLONE);

        /* Optionally add index numbers */
    bmf = (fontsize <= 0) ? NULL : bmfCreate(NULL, fontsize);
    if (bmf) {
        na = numaMakeSequence(first, 1.0, last - first + 1);
        pixa2 = pixaAddTextNumber(pixa1, bmf, na, color, L_ADD_LEFT);
        numaDestroy(&na);
    } else {
        pixa2 = pixaCopy(pixa1, L_CLONE);
    }
    pixaDestroy(&pixa1);
    bmfDestroy(&bmf);

    pixaConvertToPdf(pixa2, res, scalefactor, type, quality, NULL, fileout);
    pixaDestroy(&pixa2);
    return 0;
}


/*---------------------------------------------------------------------*
 *                    Generate pixa from tiled images                  *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaMakeFromTiledPixa()
 *
 * \param[in]    pixas    of mosaiced templates, one for each digit
 * \param[in]    w        width of samples (use 0 for default = 20)
 * \param[in]    h        height of samples (use 0 for default = 30)
 * \param[in]    nsamp    number of requested samples (use 0 for default = 100)
 * \return  pixa of individual, scaled templates, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This converts from a compressed representation of 1 bpp digit
 *          templates to a pixa where each pix has a single labeled template.
 *      (2) The mosaics hold 100 templates each, and the number of templates
 *          %nsamp selected for each digit can be between 1 and 100.
 *      (3) Each mosaic has the number of images written in the text field,
 *          and the i-th pix contains samples of the i-th digit.  That value
 *          is written into the text field of each template in the output.
 * </pre>
 */
PIXA *
pixaMakeFromTiledPixa(PIXA    *pixas,
                      l_int32  w,
                      l_int32  h,
                      l_int32  nsamp)
{
char     buf[8];
l_int32  ntiles, i;
PIX     *pix1;
PIXA    *pixad, *pixa1;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (nsamp > 1000)
        return (PIXA *)ERROR_PTR("nsamp too large; typ. 100", __func__, NULL);

    if (w <= 0) w = 20;
    if (h <= 0) h = 30;
    if (nsamp <= 0) nsamp = 100;

        /* pixas has 10 pix of mosaic'd digits.  Each of these images
         * must be extracted into a pixa of templates, where each template
         * is labeled with the digit value, and then selectively
         * concatenated into an output pixa. */
    pixad = pixaCreate(10 * nsamp);
    for (i = 0; i < 10; i++) {
        pix1 = pixaGetPix(pixas, i, L_CLONE);
        pixGetTileCount(pix1, &ntiles);
        if (nsamp > ntiles)
            L_WARNING("requested %d; only %d tiles\n", __func__, nsamp, ntiles);
        pixa1 = pixaMakeFromTiledPix(pix1, w, h, 0, nsamp, NULL);
        snprintf(buf, sizeof(buf), "%d", i);
        pixaSetText(pixa1, buf, NULL);
        pixaJoin(pixad, pixa1, 0, -1);
        pixaDestroy(&pixa1);
        pixDestroy(&pix1);
    }
    return pixad;
}


/*!
 * \brief   pixaMakeFromTiledPix()
 *
 * \param[in]    pixs        any depth; colormap OK
 * \param[in]    w           width of each tile
 * \param[in]    h           height of each tile
 * \param[in]    start       first tile to use
 * \param[in]    num         number of tiles; use 0 to go to the end
 * \param[in]    boxa        [optional] location of rectangular regions
 *                           to be extracted
 * \return  pixa if OK, NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Operations that generate a pix by tiling from a pixa, and
 *          the inverse that generate a pixa from tiles of a pix,
 *          are useful.  One such pair is pixaDisplayUnsplit() and
 *          pixaSplitPix().  This function is a very simple one that
 *          generates a pixa from tiles of a pix. There are two cases:
 *            - the tiles can all be the same size (the inverse of
 *              pixaDisplayOnLattice(), or
 *            - the tiles can differ in size, where there is an
 *              associated boxa (the inverse of pixaCreateFromBoxa().
 *      (2) If all tiles are the same size, %w by %h, use %boxa = NULL.
 *          If the tiles differ in size, use %boxa to extract the
 *          individual images (%w and %h are then ignored).
 *      (3) If the pix was made by pixaDisplayOnLattice(), the number
 *          of tiled images is written into the text field, in the format
 *               n = <number>.
 *      (4) Typical usage: a set of character templates all scaled to
 *          the same size can be stored on a lattice of that size in
 *          a pix, and this function can regenerate the pixa.  If the
 *          templates differ in size, a boxa generated when the tiled
 *          pix was made can be used to indicate the location of
 *          the templates.
 * </pre>
 */
PIXA *
pixaMakeFromTiledPix(PIX     *pixs,
                     l_int32  w,
                     l_int32  h,
                     l_int32  start,
                     l_int32  num,
                     BOXA    *boxa)
{
l_int32   i, j, k, ws, hs, d, nx, ny, n, n_isvalid, ntiles, nmax;
PIX      *pix1;
PIXA     *pixa1;
PIXCMAP  *cmap;

    if (!pixs)
        return (PIXA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (!boxa && (w <= 0 || h <= 0))
        return (PIXA *)ERROR_PTR("w and h must be > 0", __func__, NULL);

    if (boxa)  /* general case */
       return pixaCreateFromBoxa(pixs, boxa, start, num, NULL);

        /* All tiles are the same size */
    pixGetDimensions(pixs, &ws, &hs, &d);
    nx = ws / w;
    ny = hs / h;
    if (nx < 1 || ny < 1)
        return (PIXA *)ERROR_PTR("invalid dimensions", __func__, NULL);
    if (nx * w != ws || ny * h != hs)
        L_WARNING("some tiles will be clipped\n", __func__);

        /* Check the text field of the pix.  It may tell how many
         * tiles hold valid data.  If a valid value is not found,
         * assume all (nx * ny) tiles are valid.  */
    pixGetTileCount(pixs, &n);
    n_isvalid = (n <= nx * ny && n > nx * (ny - 1)) ? TRUE : FALSE;
    ntiles = (n_isvalid) ? n : nx * ny;
    nmax = ntiles - start;  /* max available from start */
    num = (num == 0) ? nmax : L_MIN(num, nmax);

        /* Extract the tiles */
    if ((pixa1 = pixaCreate(num)) == NULL) {
        return (PIXA *)ERROR_PTR("pixa1 not made", __func__, NULL);
    }
    cmap = pixGetColormap(pixs);
    for (i = 0, k = 0; i < ny; i++) {
        for (j = 0; j < nx; j++, k++) {
            if (k < start) continue;
            if (k >= start + num) break;
            pix1 = pixCreate(w, h, d);
            if (cmap) pixSetColormap(pix1, pixcmapCopy(cmap));
            pixRasterop(pix1, 0, 0, w, h, PIX_SRC, pixs, j * w, i * h);
            pixaAddPix(pixa1, pix1, L_INSERT);
        }
    }
    return pixa1;
}


/*!
 * \brief   pixGetTileCount()
 *
 * \param[in]    pix
 * \param[out]  *pn     number embedded in pix text field
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If the pix was made by pixaDisplayOnLattice(), the number
 *          of tiled images is written into the text field, in the format
 *               n = <number>.
 *      (2) This returns 0 if the data is not in the text field, or on error.
 * </pre>
 */
l_ok
pixGetTileCount(PIX      *pix,
                l_int32  *pn)
{
char    *text;
l_int32  n;

    if (!pn)
        return ERROR_INT("&n not defined", __func__, 1);
    *pn = 0;
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    text = pixGetText(pix);
    if (text && strlen(text) > 4) {
        if (sscanf(text, "n = %d", &n) == 1)
            *pn = n;
    }
    return 0;
}


/*---------------------------------------------------------------------*
 *                    Pixa display into multiple tiles                 *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaDisplayMultiTiled()
 *
 * \param[in]    pixas
 * \param[in]    nx, ny       in [1, ... 50], tiling factors in each direction
 * \param[in]    maxw, maxh   max sizes to keep
 * \param[in]    scalefactor  scale each image by this
 * \param[in]    spacing      between images, and on outside
 * \param[in]    border       width of additional black border on each image;
 *                            use 0 for no border
 * \return  pixad if OK, NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Each set of %nx * %ny images is optionally scaled and saved
 *          into a new pix, and then aggregated.
 *      (2) Set %maxw = %maxh = 0 if you want to include all pix from %pixs.
 *      (3) This is useful for generating a pdf from the output pixa, where
 *          each page is a tile of (%nx * %ny) images from the input pixa.
 * </pre>
 */
PIXA *
pixaDisplayMultiTiled(PIXA      *pixas,
                      l_int32    nx,
                      l_int32    ny,
                      l_int32    maxw,
                      l_int32    maxh,
                      l_float32  scalefactor,
                      l_int32    spacing,
                      l_int32    border)
{
l_int32  n, i, j, ntile, nout, index;
PIX     *pix1, *pix2;
PIXA    *pixa1, *pixa2, *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (nx < 1 || ny < 1 || nx > 50 || ny > 50)
        return (PIXA *)ERROR_PTR("invalid tiling factor(s)", __func__, NULL);
    if ((n = pixaGetCount(pixas)) == 0)
        return (PIXA *)ERROR_PTR("pixas is empty", __func__, NULL);

        /* Filter out large ones if requested */
    if (maxw == 0 && maxh == 0) {
        pixa1 = pixaCopy(pixas, L_CLONE);
    } else {
        maxw = (maxw == 0) ? 1000000 : maxw;
        maxh = (maxh == 0) ? 1000000 : maxh;
        pixa1 = pixaSelectBySize(pixas, maxw, maxh, L_SELECT_IF_BOTH,
                                 L_SELECT_IF_LTE, NULL);
        n = pixaGetCount(pixa1);
    }

    ntile = nx * ny;
    nout = L_MAX(1, (n + ntile - 1) / ntile);
    pixad = pixaCreate(nout);
    for (i = 0, index = 0; i < nout; i++) {  /* over tiles */
        pixa2 = pixaCreate(ntile);
        for (j = 0; j < ntile && index < n; j++, index++) {
            pix1 = pixaGetPix(pixa1, index, L_COPY);
            pixaAddPix(pixa2, pix1, L_INSERT);
        }
        pix2 = pixaDisplayTiledInColumns(pixa2, nx, scalefactor, spacing,
                                         border);
        pixaAddPix(pixad, pix2, L_INSERT);
        pixaDestroy(&pixa2);
    }
    pixaDestroy(&pixa1);

    return pixad;
}


/*---------------------------------------------------------------------*
 *                       Split pixa into files                         *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaSplitIntoFiles()
 *
 * \param[in]    pixas
 * \param[in]    nsplit       split pixas into this number of pixa; >= 2
 * \param[in]    scale        scalefactor applied to each pix
 * \param[in]    outwidth     the maxwidth parameter of tiled images
 *                            for write_pix
 * \param[in]    write_pixa  1 to write the split pixa as separate files
 * \param[in]    write_pix   1 to write tiled images of the split pixa
 * \param[in]    write_pdf   1 to write pdfs of the split pixa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) For each requested output, %nsplit files are written into
 *          directory /tmp/lept/split/.
 *      (2) This is useful when a pixa is so large that the images
 *          are not conveniently displayed as a single tiled image at
 *          full resolution.
 * </pre>
 */
l_ok
pixaSplitIntoFiles(PIXA      *pixas,
                   l_int32    nsplit,
                   l_float32  scale,
                   l_int32    outwidth,
                   l_int32    write_pixa,
                   l_int32    write_pix,
                   l_int32    write_pdf)
{
char     buf[64];
l_int32  i, j, index, n, nt;
PIX     *pix1, *pix2;
PIXA    *pixa1;

    if (!pixas)
        return ERROR_INT("pixas not defined", __func__, 1);
    if (nsplit <= 1)
        return ERROR_INT("nsplit must be >= 2", __func__, 1);
    if ((nt = pixaGetCount(pixas)) == 0)
        return ERROR_INT("pixas is empty", __func__, 1);
    if (!write_pixa && !write_pix && !write_pdf)
        return ERROR_INT("no output is requested", __func__, 1);

    lept_mkdir("lept/split");
    n = (nt + nsplit - 1) / nsplit;
    lept_stderr("nt = %d, n = %d, nsplit = %d\n", nt, n, nsplit);
    for (i = 0, index = 0; i < nsplit; i++) {
        pixa1 = pixaCreate(n);
        for (j = 0; j < n && index < nt; j++, index++) {
            pix1 = pixaGetPix(pixas, index, L_CLONE);
            pix2 = pixScale(pix1, scale, scale);
            pixaAddPix(pixa1, pix2, L_INSERT);
            pixDestroy(&pix1);
        }
        if (write_pixa) {
            snprintf(buf, sizeof(buf), "/tmp/lept/split/split%d.pa", i + 1);
            pixaWriteDebug(buf, pixa1);
        }
        if (write_pix) {
            snprintf(buf, sizeof(buf), "/tmp/lept/split/split%d.tif", i + 1);
            pix1 = pixaDisplayTiledInRows(pixa1, 1, outwidth, 1.0, 0, 20, 2);
            pixWriteDebug(buf, pix1, IFF_TIFF_G4);
            pixDestroy(&pix1);
        }
        if (write_pdf) {
            snprintf(buf, sizeof(buf), "/tmp/lept/split/split%d.pdf", i + 1);
            pixaConvertToPdf(pixa1, 0, 1.0, L_G4_ENCODE, 0, buf, buf);
        }
        pixaDestroy(&pixa1);
    }

    return 0;
}


/*---------------------------------------------------------------------*
 *                               Tile N-Up                             *
 *---------------------------------------------------------------------*/
/*!
 * \brief   convertToNUpFiles()
 *
 * \param[in]    dir        full path to directory of images
 * \param[in]    substr     [optional] can be null
 * \param[in]    nx, ny     in [1, ... 50], tiling factors in each direction
 * \param[in]    tw         target width, in pixels; must be >= 20
 * \param[in]    spacing    between images, and on outside
 * \param[in]    border     width of additional black border on each image;
 *                          use 0 for no border
 * \param[in]    fontsize   to print tail of filename with image.  Valid set is
 *                          {4,6,8,10,12,14,16,18,20}.  Use 0 to disable.
 * \param[in]    outdir     subdirectory of /tmp to put N-up tiled images
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Each set of %nx * %ny images is scaled and tiled into a single
 *          image, that is written out to %outdir.
 *      (2) All images in each %nx * %ny set are scaled to the same
 *          width, %tw.  This is typically used when all images are
 *          roughly the same size.
 *      (3) This is useful for generating a pdf from the set of input
 *          files, where each page is a tile of (%nx * %ny) input images.
 *          Typical values for %nx and %ny are in the range [2 ... 5].
 *      (4) If %fontsize != 0, each image has the tail of its filename
 *          rendered below it.
 * </pre>
 */
l_ok
convertToNUpFiles(const char  *dir,
                  const char  *substr,
                  l_int32      nx,
                  l_int32      ny,
                  l_int32      tw,
                  l_int32      spacing,
                  l_int32      border,
                  l_int32      fontsize,
                  const char  *outdir)
{
l_int32  d, format;
char     rootpath[256];
PIXA    *pixa;

    if (!dir)
        return ERROR_INT("dir not defined", __func__, 1);
    if (nx < 1 || ny < 1 || nx > 50 || ny > 50)
        return ERROR_INT("invalid tiling N-factor", __func__, 1);
    if (fontsize < 0 || fontsize > 20 || fontsize & 1 || fontsize == 2)
        return ERROR_INT("invalid fontsize", __func__, 1);
    if (!outdir)
        return ERROR_INT("outdir not defined", __func__, 1);

    pixa = convertToNUpPixa(dir, substr, nx, ny, tw, spacing, border,
                            fontsize);
    if (!pixa)
        return ERROR_INT("pixa not made", __func__, 1);

    lept_rmdir(outdir);
    lept_mkdir(outdir);
    pixaGetRenderingDepth(pixa, &d);
    format = (d == 1) ? IFF_TIFF_G4 : IFF_JFIF_JPEG;
    makeTempDirname(rootpath, 256, outdir);
    modifyTrailingSlash(rootpath, 256, L_ADD_TRAIL_SLASH);
    pixaWriteFiles(rootpath, pixa, format);
    pixaDestroy(&pixa);
    return 0;
}


/*!
 * \brief   convertToNUpPixa()
 *
 * \param[in]    dir       full path to directory of images
 * \param[in]    substr    [optional] can be null
 * \param[in]    nx, ny    in [1, ... 50], tiling factors in each direction
 * \param[in]    tw        target width, in pixels; must be >= 20
 * \param[in]    spacing   between images, and on outside
 * \param[in]    border    width of additional black border on each image;
 *                         use 0 for no border
 * \param[in]    fontsize  to print tail of filename with image.  Valid set is
 *                         {4,6,8,10,12,14,16,18,20}.  Use 0 to disable.
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See notes for convertToNUpFiles()
 * </pre>
 */
PIXA *
convertToNUpPixa(const char  *dir,
                 const char  *substr,
                 l_int32      nx,
                 l_int32      ny,
                 l_int32      tw,
                 l_int32      spacing,
                 l_int32      border,
                 l_int32      fontsize)
{
l_int32  i, n;
char    *fname, *tail;
PIXA    *pixa1, *pixa2;
SARRAY  *sa1, *sa2;

    if (!dir)
        return (PIXA *)ERROR_PTR("dir not defined", __func__, NULL);
    if (nx < 1 || ny < 1 || nx > 50 || ny > 50)
        return (PIXA *)ERROR_PTR("invalid tiling N-factor", __func__, NULL);
    if (tw < 20)
        return (PIXA *)ERROR_PTR("tw must be >= 20", __func__, NULL);
    if (fontsize < 0 || fontsize > 20 || fontsize & 1 || fontsize == 2)
        return (PIXA *)ERROR_PTR("invalid fontsize", __func__, NULL);

    sa1 = getSortedPathnamesInDirectory(dir, substr, 0, 0);
    pixa1 = pixaReadFilesSA(sa1);
    n = sarrayGetCount(sa1);
    sa2 = sarrayCreate(n);
    for (i = 0; i < n; i++) {
        fname = sarrayGetString(sa1, i, L_NOCOPY);
        splitPathAtDirectory(fname, NULL, &tail);
        sarrayAddString(sa2, tail, L_INSERT);
    }
    sarrayDestroy(&sa1);
    pixa2 = pixaConvertToNUpPixa(pixa1, sa2, nx, ny, tw, spacing,
                                 border, fontsize);
    pixaDestroy(&pixa1);
    sarrayDestroy(&sa2);
    return pixa2;
}


/*!
 * \brief   pixaConvertToNUpPixa()
 *
 * \param[in]    pixas
 * \param[in]    sa        [optional] array of strings associated with each pix
 * \param[in]    nx, ny    in [1, ... 50], tiling factors in each direction
 * \param[in]    tw        target width, in pixels; must be >= 20
 * \param[in]    spacing   between images, and on outside
 * \param[in]    border    width of additional black border on each image;
 *                         use 0 for no border
 * \param[in]    fontsize  to print string with each image.  Valid set is
 *                         {4,6,8,10,12,14,16,18,20}.  Use 0 to disable.
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This takes an input pixa and an optional array of strings, and
 *          generates a pixa of NUp tiles from the input, labeled with
 *          the strings if they exist and %fontsize != 0.
 *      (2) See notes for convertToNUpFiles()
 * </pre>
 */
PIXA *
pixaConvertToNUpPixa(PIXA    *pixas,
                     SARRAY  *sa,
                     l_int32  nx,
                     l_int32  ny,
                     l_int32  tw,
                     l_int32  spacing,
                     l_int32  border,
                     l_int32  fontsize)
{
l_int32    i, j, k, nt, n2, nout, d;
char      *str;
L_BMF     *bmf;
PIX       *pix1, *pix2, *pix3, *pix4;
PIXA      *pixa1, *pixad;

    if (!pixas)
        return (PIXA *)ERROR_PTR("pixas not defined", __func__, NULL);
    if (nx < 1 || ny < 1 || nx > 50 || ny > 50)
        return (PIXA *)ERROR_PTR("invalid tiling N-factor", __func__, NULL);
    if (tw < 20)
        return (PIXA *)ERROR_PTR("tw must be >= 20", __func__, NULL);
    if (fontsize < 0 || fontsize > 20 || fontsize & 1 || fontsize == 2)
        return (PIXA *)ERROR_PTR("invalid fontsize", __func__, NULL);

    nt = pixaGetCount(pixas);
    if (sa && (sarrayGetCount(sa) != nt)) {
        L_WARNING("pixa size %d not equal to sarray size %d\n", __func__,
                  nt, sarrayGetCount(sa));
    }

    n2 = nx * ny;
    nout = (nt + n2 - 1) / n2;
    pixad = pixaCreate(nout);
    bmf = (fontsize == 0) ? NULL : bmfCreate(NULL, fontsize);
    for (i = 0, j = 0; i < nout; i++) {
        pixa1 = pixaCreate(n2);
        for (k = 0; k < n2 && j < nt; j++, k++) {
            pix1 = pixaGetPix(pixas, j, L_CLONE);
            pix2 = pixScaleToSize(pix1, tw, 0);  /* all images have width tw */
            if (bmf && sa) {
                str = sarrayGetString(sa, j, L_NOCOPY);
                pix3 = pixAddTextlines(pix2, bmf, str, 0xff000000,
                                       L_ADD_BELOW);
            } else {
                pix3 = pixClone(pix2);
            }
            pixaAddPix(pixa1, pix3, L_INSERT);
            pixDestroy(&pix1);
            pixDestroy(&pix2);
        }
        if (pixaGetCount(pixa1) == 0) {  /* probably won't happen */
            pixaDestroy(&pixa1);
            continue;
        }

            /* Add 2 * border to image width to prevent scaling */
        pixaGetRenderingDepth(pixa1, &d);
        pix4 = pixaDisplayTiledAndScaled(pixa1, d, tw + 2 * border, nx, 0,
                                         spacing, border);
        pixaAddPix(pixad, pix4, L_INSERT);
        pixaDestroy(&pixa1);
    }

    bmfDestroy(&bmf);
    return pixad;
}


/*---------------------------------------------------------------------*
 *            Render two pixa side-by-side for comparison              *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaCompareInPdf()
 *
 * \param[in]    pixa1
 * \param[in]    pixa2
 * \param[in]    nx, ny     in [1, ... 20], tiling factors in each direction
 * \param[in]    tw         target width, in pixels; must be >= 20
 * \param[in]    spacing    between images, and on outside
 * \param[in]    border     width of additional black border on each image
 *                          and on each pair; use 0 for no border
 * \param[in]    fontsize   to print index of each pair of images.  Valid set
 *                          is {4,6,8,10,12,14,16,18,20}.  Use 0 to disable.
 * \param[in]    fileout    output pdf file
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This takes two pixa and renders them interleaved, side-by-side
 *          in a pdf.  A warning is issued if the input pixa arrays
 *          have different lengths.
 *      (2) %nx and %ny specify how many side-by-side pairs are displayed
 *          on each pdf page.  For example, if %nx = 1 and %ny = 2, then
 *          two pairs are shown, one above the other, on each page.
 *      (3) The input pix are scaled to a target width of %tw, and
 *          then paired with optional %spacing between and optional
 *          black border of width %border.
 *      (4) After a pixa is generated of these tiled images, it is
 *          written to %fileout as a pdf.
 *      (5) Typical numbers for the input parameters are:
 *            %nx = small integer (1 - 4)
 *            %ny = 2 * %nx
 *            %tw = 200 - 500 pixels
 *            %spacing = 10
 *            %border = 2
 *            %fontsize = 10
 *      (6) If %fontsize != 0, the index of the pix pair in their pixa
 *          is printed out below each pair.
 * </pre>
 */
l_ok
pixaCompareInPdf(PIXA        *pixa1,
                 PIXA        *pixa2,
                 l_int32      nx,
                 l_int32      ny,
                 l_int32      tw,
                 l_int32      spacing,
                 l_int32      border,
                 l_int32      fontsize,
                 const char  *fileout)
{
l_int32  n1, n2, npairs;
PIXA    *pixa3, *pixa4, *pixa5;
SARRAY  *sa;

    if (!pixa1 || !pixa2)
        return ERROR_INT("pixa1 and pixa2 not both defined", __func__, 1);
    if (nx < 1 || ny < 1 || nx > 20 || ny > 20)
        return ERROR_INT("invalid tiling factors", __func__, 1);
    if (tw < 20)
        return ERROR_INT("invalid tw; tw must be >= 20", __func__, 1);
    if (fontsize < 0 || fontsize > 20 || fontsize & 1 || fontsize == 2)
        return ERROR_INT("invalid fontsize", __func__, 1);
    if (!fileout)
        return ERROR_INT("fileout not defined", __func__, 1);
    n1 = pixaGetCount(pixa1);
    n2 = pixaGetCount(pixa2);
    if (n1 == 0 || n2 == 0)
        return ERROR_INT("at least one pixa is empty", __func__, 1);
    if (n1 != n2)
        L_WARNING("sizes (%d, %d) differ; using the minimum in interleave\n",
                  __func__, n1, n2);

        /* Interleave the input pixa */
    if ((pixa3 = pixaInterleave(pixa1, pixa2, L_CLONE)) == NULL)
        return ERROR_INT("pixa3 not made", __func__, 1);

        /* Scale the images if necessary and pair them up side/by/side */
    pixa4 = pixaConvertToNUpPixa(pixa3, NULL, 2, 1, tw, spacing, border, 0);
    pixaDestroy(&pixa3);

        /* Label the pairs and mosaic into pages without further scaling */
    npairs = pixaGetCount(pixa4);
    sa = (fontsize > 0) ? sarrayGenerateIntegers(npairs) : NULL;
    pixa5 = pixaConvertToNUpPixa(pixa4, sa, nx, ny,
                                 2 * tw + 4 * border + spacing,
                                 spacing, border, fontsize);
    pixaDestroy(&pixa4);
    sarrayDestroy(&sa);

        /* Output as pdf without scaling */
    pixaConvertToPdf(pixa5, 0, 1.0, 0, 0, NULL, fileout);
    pixaDestroy(&pixa5);
    return 0;
}



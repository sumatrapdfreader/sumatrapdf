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
 * \file  pixabasic.c
 * <pre>
 *
 *      Pixa creation, destruction, copying
 *           PIXA     *pixaCreate()
 *           PIXA     *pixaCreateFromPix()
 *           PIXA     *pixaCreateFromBoxa()
 *           PIXA     *pixaSplitPix()
 *           void      pixaDestroy()
 *           PIXA     *pixaCopy()
 *
 *      Pixa addition
 *           l_int32   pixaAddPix()
 *           l_int32   pixaAddBox()
 *           static l_int32   pixaExtendArray()
 *           l_int32   pixaExtendArrayToSize()
 *
 *      Pixa accessors
 *           l_int32   pixaGetCount()
 *           PIX      *pixaGetPix()
 *           l_int32   pixaGetPixDimensions()
 *           BOXA     *pixaGetBoxa()
 *           l_int32   pixaGetBoxaCount()
 *           BOX      *pixaGetBox()
 *           l_int32   pixaGetBoxGeometry()
 *           l_int32   pixaSetBoxa()
 *           PIX     **pixaGetPixArray()
 *           l_int32   pixaVerifyDepth()
 *           l_int32   pixaVerifyDimensions()
 *           l_int32   pixaIsFull()
 *           l_int32   pixaCountText()
 *           l_int32   pixaSetText()
 *           void   ***pixaGetLinePtrs()
 *
 *      Pixa output info
 *           l_int32   pixaWriteStreamInfo()
 *
 *      Pixa array modifiers
 *           l_int32   pixaReplacePix()
 *           l_int32   pixaInsertPix()
 *           l_int32   pixaRemovePix()
 *           l_int32   pixaRemovePixAndSave()
 *           l_int32   pixaRemoveSelected()
 *           l_int32   pixaInitFull()
 *           l_int32   pixaClear()
 *
 *      Pixa and Pixaa combination
 *           l_int32   pixaJoin()
 *           PIXA     *pixaInterleave()
 *           l_int32   pixaaJoin()
 *
 *      Pixaa creation, destruction
 *           PIXAA    *pixaaCreate()
 *           PIXAA    *pixaaCreateFromPixa()
 *           void      pixaaDestroy()
 *
 *      Pixaa addition
 *           l_int32   pixaaAddPixa()
 *           static l_int32   pixaaExtendArray()
 *           l_int32   pixaaAddPix()
 *           l_int32   pixaaAddBox()
 *
 *      Pixaa accessors
 *           l_int32   pixaaGetCount()
 *           PIXA     *pixaaGetPixa()
 *           BOXA     *pixaaGetBoxa()
 *           PIX      *pixaaGetPix()
 *           l_int32   pixaaVerifyDepth()
 *           l_int32   pixaaVerifyDimensions()
 *           l_int32   pixaaIsFull()
 *
 *      Pixaa array modifiers
 *           l_int32   pixaaInitFull()
 *           l_int32   pixaaReplacePixa()
 *           l_int32   pixaaClear()
 *           l_int32   pixaaTruncate()
 *
 *      Pixa serialized I/O  (requires png support)
 *           PIXA     *pixaRead()
 *           PIXA     *pixaReadStream()
 *           PIXA     *pixaReadMem()
 *           l_int32   pixaWriteDebug()
 *           l_int32   pixaWrite()
 *           l_int32   pixaWriteStream()
 *           l_int32   pixaWriteMem()
 *           PIXA     *pixaReadBoth()
 *
 *      Pixaa serialized I/O  (requires png support)
 *           PIXAA    *pixaaReadFromFiles()
 *           PIXAA    *pixaaRead()
 *           PIXAA    *pixaaReadStream()
 *           PIXAA    *pixaaReadMem()
 *           l_int32   pixaaWrite()
 *           l_int32   pixaaWriteStream()
 *           l_int32   pixaaWriteMem()
 *
 *
 *   Important note on reference counting:
 *     Reference counting for the Pixa is analogous to that for the Boxa.
 *     See pix.h for details.   pixaCopy() provides three possible modes
 *     of copy.  The basic rule is that however a Pixa is obtained
 *     (e.g., from pixaCreate*(), pixaCopy(), or a Pixaa accessor),
 *     it is necessary to call pixaDestroy() on it.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"
#include "pix_internal.h"

    /* Bounds on array sizes */
static const size_t  MaxInitPtrArraySize = 100000;
static const size_t  MaxPixaPtrArraySize = 5000000;
static const size_t  MaxPixaaPtrArraySize = 5000000;
static const size_t  InitialPtrArraySize = 20;      /*!< n'importe quoi */

    /* Static functions */
static l_int32 pixaExtendArray(PIXA  *pixa);
static l_int32 pixaaExtendArray(PIXAA *paa);

/*---------------------------------------------------------------------*
 *                    Pixa creation, destruction, copy                 *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaCreate()
 *
 * \param[in]    n    initial number of ptrs
 * \return  pixa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This creates an empty boxa.
 * </pre>
 */
PIXA *
pixaCreate(l_int32  n)
{
PIXA  *pixa;

    if (n <= 0 || n > MaxInitPtrArraySize)
        n = InitialPtrArraySize;

    pixa = (PIXA *)LEPT_CALLOC(1, sizeof(PIXA));
    pixa->n = 0;
    pixa->nalloc = n;
    pixa->refcount = 1;
    pixa->pix = (PIX **)LEPT_CALLOC(n, sizeof(PIX *));
    pixa->boxa = boxaCreate(n);
    if (!pixa->pix || !pixa->boxa) {
        pixaDestroy(&pixa);
        return (PIXA *)ERROR_PTR("pix or boxa not made", __func__, NULL);
    }
    return pixa;
}


/*!
 * \brief   pixaCreateFromPix()
 *
 * \param[in]    pixs    with individual components on a lattice
 * \param[in]    n       number of components
 * \param[in]    cellw   width of each cell
 * \param[in]    cellh   height of each cell
 * \return  pixa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) For bpp = 1, we truncate each retrieved pix to the ON
 *          pixels, which we assume for now start at (0,0)
 * </pre>
 */
PIXA *
pixaCreateFromPix(PIX     *pixs,
                  l_int32  n,
                  l_int32  cellw,
                  l_int32  cellh)
{
l_int32  w, h, d, nw, nh, i, j, index;
PIX     *pix1, *pix2;
PIXA    *pixa;

    if (!pixs)
        return (PIXA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (n <= 0)
        return (PIXA *)ERROR_PTR("n must be > 0", __func__, NULL);

    if ((pixa = pixaCreate(n)) == NULL)
        return (PIXA *)ERROR_PTR("pixa not made", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if ((pix1 = pixCreate(cellw, cellh, d)) == NULL) {
        pixaDestroy(&pixa);
        return (PIXA *)ERROR_PTR("pix1 not made", __func__, NULL);
    }

    nw = (w + cellw - 1) / cellw;
    nh = (h + cellh - 1) / cellh;
    for (i = 0, index = 0; i < nh; i++) {
        for (j = 0; j < nw && index < n; j++, index++) {
            pixRasterop(pix1, 0, 0, cellw, cellh, PIX_SRC, pixs,
                   j * cellw, i * cellh);
            if (d == 1 && !pixClipToForeground(pix1, &pix2, NULL))
                pixaAddPix(pixa, pix2, L_INSERT);
            else
                pixaAddPix(pixa, pix1, L_COPY);
        }
    }

    pixDestroy(&pix1);
    return pixa;
}


/*!
 * \brief   pixaCreateFromBoxa()
 *
 * \param[in]    pixs
 * \param[in]    boxa
 * \param[in]    start       first box to use
 * \param[in]    num         number of boxes; use 0 to go to the end
 * \param[out]   pcropwarn   [optional] TRUE if the boxa extent
 *                           is larger than pixs.
 * \return  pixad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This simply extracts from pixs the region corresponding to each
 *          box in the boxa.  To extract all the regions, set both %start
 *          and %num to 0.
 *      (2) The 5th arg is optional.  If the extent of the boxa exceeds the
 *          size of the pixa, so that some boxes are either clipped
 *          or entirely outside the pix, a warning is returned as TRUE.
 *      (3) pixad will have only the properly clipped elements, and
 *          the internal boxa will be correct.
 * </pre>
 */
PIXA *
pixaCreateFromBoxa(PIX      *pixs,
                   BOXA     *boxa,
                   l_int32   start,
                   l_int32   num,
                   l_int32  *pcropwarn)
{
l_int32  i, n, end, w, h, wbox, hbox, cropwarn;
BOX     *box, *boxc;
PIX     *pixd;
PIXA    *pixad;

    if (!pixs)
        return (PIXA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (!boxa)
        return (PIXA *)ERROR_PTR("boxa not defined", __func__, NULL);
    if (num < 0)
        return (PIXA *)ERROR_PTR("num must be >= 0", __func__, NULL);

    n = boxaGetCount(boxa);
    end = (num == 0) ? n - 1 : L_MIN(start + num - 1, n - 1);
    if ((pixad = pixaCreate(end - start + 1)) == NULL)
        return (PIXA *)ERROR_PTR("pixad not made", __func__, NULL);

    boxaGetExtent(boxa, &wbox, &hbox, NULL);
    pixGetDimensions(pixs, &w, &h, NULL);
    cropwarn = FALSE;
    if (wbox > w || hbox > h)
        cropwarn = TRUE;
    if (pcropwarn)
        *pcropwarn = cropwarn;

    for (i = start; i <= end; i++) {
        box = boxaGetBox(boxa, i, L_COPY);
        if (cropwarn) {  /* if box is outside pixs, pixd is NULL */
            pixd = pixClipRectangle(pixs, box, &boxc);  /* may be NULL */
            if (pixd) {
                pixaAddPix(pixad, pixd, L_INSERT);
                pixaAddBox(pixad, boxc, L_INSERT);
            }
            boxDestroy(&box);
        } else {
            pixd = pixClipRectangle(pixs, box, NULL);
            pixaAddPix(pixad, pixd, L_INSERT);
            pixaAddBox(pixad, box, L_INSERT);
        }
    }

    return pixad;
}


/*!
 * \brief   pixaSplitPix()
 *
 * \param[in]    pixs          with individual components on a lattice
 * \param[in]    nx            number of mosaic cells horizontally
 * \param[in]    ny            number of mosaic cells vertically
 * \param[in]    borderwidth   of added border on all sides
 * \param[in]    bordercolor   in our RGBA format: 0xrrggbbaa
 * \return  pixa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This is a variant on pixaCreateFromPix(), where we
 *          simply divide the image up into (approximately) equal
 *          subunits.  If you want the subimages to have essentially
 *          the same aspect ratio as the input pix, use nx = ny.
 *      (2) If borderwidth is 0, we ignore the input bordercolor and
 *          redefine it to white.
 *      (3) The bordercolor is always used to initialize each tiled pix,
 *          so that if the src is clipped, the unblitted part will
 *          be this color.  This avoids 1 pixel wide black stripes at the
 *          left and lower edges.
 * </pre>
 */
PIXA *
pixaSplitPix(PIX      *pixs,
             l_int32   nx,
             l_int32   ny,
             l_int32   borderwidth,
             l_uint32  bordercolor)
{
l_int32  w, h, d, cellw, cellh, i, j;
PIX     *pix1;
PIXA    *pixa;

    if (!pixs)
        return (PIXA *)ERROR_PTR("pixs not defined", __func__, NULL);
    if (nx <= 0 || ny <= 0)
        return (PIXA *)ERROR_PTR("nx and ny must be > 0", __func__, NULL);
    borderwidth = L_MAX(0, borderwidth);

    if ((pixa = pixaCreate(nx * ny)) == NULL)
        return (PIXA *)ERROR_PTR("pixa not made", __func__, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    cellw = (w + nx - 1) / nx;  /* round up */
    cellh = (h + ny - 1) / ny;

    for (i = 0; i < ny; i++) {
        for (j = 0; j < nx; j++) {
            if ((pix1 = pixCreate(cellw + 2 * borderwidth,
                                  cellh + 2 * borderwidth, d)) == NULL) {
                pixaDestroy(&pixa);
                return (PIXA *)ERROR_PTR("pix1 not made", __func__, NULL);
            }
            pixCopyColormap(pix1, pixs);
            if (borderwidth == 0) {  /* initialize full image to white */
                if (d == 1)
                    pixClearAll(pix1);
                else
                    pixSetAll(pix1);
            } else {
                pixSetAllArbitrary(pix1, bordercolor);
            }
            pixRasterop(pix1, borderwidth, borderwidth, cellw, cellh,
                        PIX_SRC, pixs, j * cellw, i * cellh);
            pixaAddPix(pixa, pix1, L_INSERT);
        }
    }

    return pixa;
}


/*!
 * \brief   pixaDestroy()
 *
 * \param[in,out]  ppixa    use ptr address so it will be nulled
 *
 * <pre>
 * Notes:
 *      (1) Decrements the ref count and, if 0, destroys the pixa.
 *      (2) Always nulls the input ptr.
 * </pre>
 */
void
pixaDestroy(PIXA  **ppixa)
{
l_int32  i;
PIXA    *pixa;

    if (ppixa == NULL) {
        L_WARNING("ptr address is NULL!\n", __func__);
        return;
    }

    if ((pixa = *ppixa) == NULL)
        return;

        /* Decrement the refcount.  If it is 0, destroy the pixa. */
    if (--pixa->refcount == 0) {
        for (i = 0; i < pixa->n; i++)
            pixDestroy(&pixa->pix[i]);
        LEPT_FREE(pixa->pix);
        boxaDestroy(&pixa->boxa);
        LEPT_FREE(pixa);
    }

    *ppixa = NULL;
}


/*!
 * \brief   pixaCopy()
 *
 * \param[in]    pixa
 * \param[in]    copyflag  see pix.h for details:
 *                 L_COPY makes a new pixa and copies each pix and each box;
 *                 L_CLONE gives a new ref-counted handle to the input pixa;
 *                 L_COPY_CLONE makes a new pixa and inserts clones of
 *                     all pix and boxes
 * \return  new pixa, or NULL on error
 */
PIXA *
pixaCopy(PIXA    *pixa,
         l_int32  copyflag)
{
l_int32  i, nb;
BOX     *boxc = NULL;
PIX     *pixc;
PIXA    *pixac;

    if (!pixa)
        return (PIXA *)ERROR_PTR("pixa not defined", __func__, NULL);

    if (copyflag == L_CLONE) {
        ++pixa->refcount;
        return pixa;
    }

    if (copyflag != L_COPY && copyflag != L_COPY_CLONE)
        return (PIXA *)ERROR_PTR("invalid copyflag", __func__, NULL);

    if ((pixac = pixaCreate(pixa->n)) == NULL)
        return (PIXA *)ERROR_PTR("pixac not made", __func__, NULL);
    nb = pixaGetBoxaCount(pixa);
    for (i = 0; i < pixa->n; i++) {
        if (copyflag == L_COPY) {
            pixc = pixaGetPix(pixa, i, L_COPY);
            if (i < nb) boxc = pixaGetBox(pixa, i, L_COPY);
        } else {  /* copy-clone */
            pixc = pixaGetPix(pixa, i, L_CLONE);
            if (i < nb) boxc = pixaGetBox(pixa, i, L_CLONE);
        }
        pixaAddPix(pixac, pixc, L_INSERT);
        if (i < nb) pixaAddBox(pixac, boxc, L_INSERT);
    }

    return pixac;
}



/*---------------------------------------------------------------------*
 *                              Pixa addition                          *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaAddPix()
 *
 * \param[in]    pixa
 * \param[in]    pix        to be added
 * \param[in]    copyflag   L_INSERT, L_COPY, L_CLONE
 * \return  0 if OK; 1 on error
 */
l_ok
pixaAddPix(PIXA    *pixa,
           PIX     *pix,
           l_int32  copyflag)
{
l_int32  n;
PIX     *pixc;

    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    if (copyflag == L_INSERT)
        pixc = pix;
    else if (copyflag == L_COPY)
        pixc = pixCopy(NULL, pix);
    else if (copyflag == L_CLONE)
        pixc = pixClone(pix);
    else
        return ERROR_INT("invalid copyflag", __func__, 1);
    if (!pixc)
        return ERROR_INT("pixc not made", __func__, 1);

    n = pixaGetCount(pixa);
    if (n >= pixa->nalloc) {
        if (pixaExtendArray(pixa)) {
            if (copyflag != L_INSERT)
                pixDestroy(&pixc);
            return ERROR_INT("extension failed", __func__, 1);
        }
    }

    pixa->pix[n] = pixc;
    pixa->n++;
    return 0;
}


/*!
 * \brief   pixaAddBox()
 *
 * \param[in]    pixa
 * \param[in]    box
 * \param[in]    copyflag    L_INSERT, L_COPY, L_CLONE
 * \return  0 if OK, 1 on error
 */
l_ok
pixaAddBox(PIXA    *pixa,
           BOX     *box,
           l_int32  copyflag)
{
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);
    if (copyflag != L_INSERT && copyflag != L_COPY && copyflag != L_CLONE)
        return ERROR_INT("invalid copyflag", __func__, 1);

    boxaAddBox(pixa->boxa, box, copyflag);
    return 0;
}


/*!
 * \brief   pixaExtendArray()
 *
 * \param[in]    pixa
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Doubles the size of the pixa and boxa ptr arrays.
 *      (2) The max number of pix in the array is 5 million.
 * </pre>
 */
static l_int32
pixaExtendArray(PIXA  *pixa)
{
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    return pixaExtendArrayToSize(pixa, 2 * pixa->nalloc);
}


/*!
 * \brief   pixaExtendArrayToSize()
 *
 * \param[in]    pixa
 * \param[in]    size     number of pix ptrs in new array
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If necessary, reallocs new pixa and boxa ptrs arrays to %size.
 *          The pixa and boxa ptr arrays must always be equal in size.
 *      (2) The max number of pix ptrs is 5M.
 * </pre>
 */
l_ok
pixaExtendArrayToSize(PIXA   *pixa,
                      size_t  size)
{
size_t  oldsize, newsize;

    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (pixa->nalloc > MaxPixaPtrArraySize)  /* belt & suspenders */
        return ERROR_INT("pixa has too many ptrs", __func__, 1);
    if (size > MaxPixaPtrArraySize)
        return ERROR_INT("size > 5M ptrs; too large", __func__, 1);
    if (size <= pixa->nalloc) {
        L_INFO("size too small; no extension\n", __func__);
        return 0;
    }

    oldsize = pixa->nalloc * sizeof(PIX *);
    newsize = size * sizeof(PIX *);
    if ((pixa->pix = (PIX **)reallocNew((void **)&pixa->pix,
                                         oldsize, newsize)) == NULL)
        return ERROR_INT("new ptr array not returned", __func__, 1);
    pixa->nalloc = size;
    return boxaExtendArrayToSize(pixa->boxa, size);
}


/*---------------------------------------------------------------------*
 *                             Pixa accessors                          *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaGetCount()
 *
 * \param[in]    pixa
 * \return  count, or 0 if no pixa
 */
l_int32
pixaGetCount(PIXA  *pixa)
{
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 0);

    return pixa->n;
}


/*!
 * \brief   pixaGetPix()
 *
 * \param[in]    pixa
 * \param[in]    index        to the index-th pix
 * \param[in]    accesstype   L_COPY or L_CLONE
 * \return  pix, or NULL on error
 */
PIX *
pixaGetPix(PIXA    *pixa,
           l_int32  index,
           l_int32  accesstype)
{
PIX  *pix;

    if (!pixa)
        return (PIX *)ERROR_PTR("pixa not defined", __func__, NULL);
    if (index < 0 || index >= pixa->n)
        return (PIX *)ERROR_PTR("index not valid", __func__, NULL);
    if ((pix = pixa->pix[index]) == NULL) {
        L_ERROR("no pix at pixa[%d]\n", __func__, index);
        return (PIX *)ERROR_PTR("pix not found!", __func__, NULL);
    }

    if (accesstype == L_COPY)
        return pixCopy(NULL, pix);
    else if (accesstype == L_CLONE)
        return pixClone(pix);
    else
        return (PIX *)ERROR_PTR("invalid accesstype", __func__, NULL);
}


/*!
 * \brief   pixaGetPixDimensions()
 *
 * \param[in]    pixa
 * \param[in]    index         to the index-th box
 * \param[out]   pw, ph, pd    [optional] each can be null
 * \return  0 if OK, 1 on error
 */
l_ok
pixaGetPixDimensions(PIXA     *pixa,
                     l_int32   index,
                     l_int32  *pw,
                     l_int32  *ph,
                     l_int32  *pd)
{
PIX  *pix;

    if (pw) *pw = 0;
    if (ph) *ph = 0;
    if (pd) *pd = 0;
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (index < 0 || index >= pixa->n)
        return ERROR_INT("index not valid", __func__, 1);

    if ((pix = pixaGetPix(pixa, index, L_CLONE)) == NULL)
        return ERROR_INT("pix not found!", __func__, 1);
    pixGetDimensions(pix, pw, ph, pd);
    pixDestroy(&pix);
    return 0;
}


/*!
 * \brief   pixaGetBoxa()
 *
 * \param[in]    pixa
 * \param[in]    accesstype   L_COPY, L_CLONE, L_COPY_CLONE
 * \return  boxa, or NULL on error
 */
BOXA *
pixaGetBoxa(PIXA    *pixa,
            l_int32  accesstype)
{
    if (!pixa)
        return (BOXA *)ERROR_PTR("pixa not defined", __func__, NULL);
    if (!pixa->boxa)
        return (BOXA *)ERROR_PTR("boxa not defined", __func__, NULL);
    if (accesstype != L_COPY && accesstype != L_CLONE &&
        accesstype != L_COPY_CLONE)
        return (BOXA *)ERROR_PTR("invalid accesstype", __func__, NULL);

    return boxaCopy(pixa->boxa, accesstype);
}


/*!
 * \brief   pixaGetBoxaCount()
 *
 * \param[in]    pixa
 * \return  count, or 0 on error
 */
l_int32
pixaGetBoxaCount(PIXA  *pixa)
{
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 0);

    return boxaGetCount(pixa->boxa);
}


/*!
 * \brief   pixaGetBox()
 *
 * \param[in]    pixa
 * \param[in]    index        to the index-th pix
 * \param[in]    accesstype   L_COPY or L_CLONE
 * \return  box if null, not automatically an error, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) There is always a boxa with a pixa, and it is initialized so
 *          that each box ptr is NULL.
 *      (2) In general, we expect that there is either a box associated
 *          with each pix, or no boxes at all in the boxa.
 *      (3) Having no boxes is thus not an automatic error.  Whether it
 *          is an actual error is determined by the calling program.
 *          If the caller expects to get a box, it is an error; see, e.g.,
 *          pixaGetBoxGeometry().
 * </pre>
 */
BOX *
pixaGetBox(PIXA    *pixa,
           l_int32  index,
           l_int32  accesstype)
{
BOX  *box;

    if (!pixa)
        return (BOX *)ERROR_PTR("pixa not defined", __func__, NULL);
    if (!pixa->boxa)
        return (BOX *)ERROR_PTR("boxa not defined", __func__, NULL);
    if (index < 0 || index >= pixa->boxa->n)
        return (BOX *)ERROR_PTR("index not valid", __func__, NULL);
    if (accesstype != L_COPY && accesstype != L_CLONE)
        return (BOX *)ERROR_PTR("invalid accesstype", __func__, NULL);

    box = pixa->boxa->box[index];
    if (box) {
        if (accesstype == L_COPY)
            return boxCopy(box);
        else  /* accesstype == L_CLONE */
            return boxClone(box);
    } else {
        return NULL;
    }
}


/*!
 * \brief   pixaGetBoxGeometry()
 *
 * \param[in]    pixa
 * \param[in]    index            to the index-th box
 * \param[out]   px, py, pw, ph   [optional] each can be null
 * \return  0 if OK, 1 on error
 */
l_ok
pixaGetBoxGeometry(PIXA     *pixa,
                   l_int32   index,
                   l_int32  *px,
                   l_int32  *py,
                   l_int32  *pw,
                   l_int32  *ph)
{
BOX  *box;

    if (px) *px = 0;
    if (py) *py = 0;
    if (pw) *pw = 0;
    if (ph) *ph = 0;
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (index < 0 || index >= pixa->n)
        return ERROR_INT("index not valid", __func__, 1);

    if ((box = pixaGetBox(pixa, index, L_CLONE)) == NULL)
        return ERROR_INT("box not found!", __func__, 1);
    boxGetGeometry(box, px, py, pw, ph);
    boxDestroy(&box);
    return 0;
}


/*!
 * \brief   pixaSetBoxa()
 *
 * \param[in]    pixa
 * \param[in]    boxa
 * \param[in]    accesstype   L_INSERT, L_COPY, L_CLONE
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This destroys the existing boxa in the pixa.
 * </pre>
 */
l_ok
pixaSetBoxa(PIXA    *pixa,
            BOXA    *boxa,
            l_int32  accesstype)
{
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if (accesstype != L_INSERT && accesstype != L_COPY &&
        accesstype != L_CLONE)
        return ERROR_INT("invalid access type", __func__, 1);

    boxaDestroy(&pixa->boxa);
    if (accesstype == L_INSERT)
        pixa->boxa = boxa;
    else
        pixa->boxa = boxaCopy(boxa, accesstype);

    return 0;
}


/*!
 * \brief   pixaGetPixArray()
 *
 * \param[in]    pixa
 * \return  pix array, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This returns a ptr to the actual array.  The array is
 *          owned by the pixa, so it must not be destroyed.
 *      (2) The caller should always check if the return value is NULL
 *          before accessing any of the pix ptrs in this array!
 * </pre>
 */
PIX **
pixaGetPixArray(PIXA  *pixa)
{
    if (!pixa)
        return (PIX **)ERROR_PTR("pixa not defined", __func__, NULL);

    return pixa->pix;
}


/*!
 * \brief   pixaVerifyDepth()
 *
 * \param[in]    pixa
 * \param[out]   psame   1 if depth is the same for all pix; 0 otherwise
 * \param[out]   pmaxd   [optional] max depth of all pix
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) It is considered to be an error if there are no pix.
 * </pre>
 */
l_ok
pixaVerifyDepth(PIXA     *pixa,
                l_int32  *psame,
                l_int32  *pmaxd)
{
l_int32  i, n, d, maxd, same;

    if (pmaxd) *pmaxd = 0;
    if (!psame)
        return ERROR_INT("psame not defined", __func__, 1);
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if ((n = pixaGetCount(pixa)) == 0)
        return ERROR_INT("no pix in pixa", __func__, 1);

    same = 1;
    pixaGetPixDimensions(pixa, 0, NULL, NULL, &maxd);
    for (i = 1; i < n; i++) {
        if (pixaGetPixDimensions(pixa, i, NULL, NULL, &d))
            return ERROR_INT("pix depth not found", __func__, 1);
        maxd = L_MAX(maxd, d);
        if (d != maxd)
            same = 0;
    }
    *psame = same;
    if (pmaxd) *pmaxd = maxd;
    return 0;
}


/*!
 * \brief   pixaVerifyDimensions()
 *
 * \param[in]    pixa
 * \param[out]   psame   1 if dimensions are the same for all pix; 0 otherwise
 * \param[out]   pmaxw   [optional] max width of all pix
 * \param[out]   pmaxh   [optional] max height of all pix
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) It is considered to be an error if there are no pix.
 * </pre>
 */
l_ok
pixaVerifyDimensions(PIXA     *pixa,
                     l_int32  *psame,
                     l_int32  *pmaxw,
                     l_int32  *pmaxh)
{
l_int32  i, n, w, h, maxw, maxh, same;

    if (pmaxw) *pmaxw = 0;
    if (pmaxh) *pmaxh = 0;
    if (!psame)
        return ERROR_INT("psame not defined", __func__, 1);
    *psame = 0;
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if ((n = pixaGetCount(pixa)) == 0)
        return ERROR_INT("no pix in pixa", __func__, 1);

    same = 1;
    pixaGetPixDimensions(pixa, 0, &maxw, &maxh, NULL);
    for (i = 1; i < n; i++) {
        if (pixaGetPixDimensions(pixa, i, &w, &h, NULL))
            return ERROR_INT("pix dimensions not found", __func__, 1);
        maxw = L_MAX(maxw, w);
        maxh = L_MAX(maxh, h);
        if (w != maxw || h != maxh)
            same = 0;
    }
    *psame = same;
    if (pmaxw) *pmaxw = maxw;
    if (pmaxh) *pmaxh = maxh;
    return 0;
}


/*!
 * \brief   pixaIsFull()
 *
 * \param[in]    pixa
 * \param[out]   pfullpa   [optional] 1 if pixa is full
 * \param[out]   pfullba   [optional] 1 if boxa is full
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) A pixa is "full" if the array of pix is fully
 *          occupied from index 0 to index (pixa->n - 1).
 * </pre>
 */
l_ok
pixaIsFull(PIXA     *pixa,
           l_int32  *pfullpa,
           l_int32  *pfullba)
{
l_int32  i, n, full;
BOXA    *boxa;
PIX     *pix;

    if (pfullpa) *pfullpa = 0;
    if (pfullba) *pfullba = 0;
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    n = pixaGetCount(pixa);
    if (pfullpa) {
        full = 1;
        for (i = 0; i < n; i++) {
            if ((pix = pixaGetPix(pixa, i, L_CLONE)) == NULL) {
                full = 0;
                break;
            }
            pixDestroy(&pix);
        }
        *pfullpa = full;
    }
    if (pfullba) {
        boxa = pixaGetBoxa(pixa, L_CLONE);
        boxaIsFull(boxa, pfullba);
        boxaDestroy(&boxa);
    }
    return 0;
}


/*!
 * \brief   pixaCountText()
 *
 * \param[in]    pixa
 * \param[out]   pntext    number of pix with non-empty text strings
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) All pix have non-empty text strings if the returned value %ntext
 *          equals the pixa count.
 * </pre>
 */
l_ok
pixaCountText(PIXA     *pixa,
              l_int32  *pntext)
{
char    *text;
l_int32  i, n;
PIX     *pix;

    if (!pntext)
        return ERROR_INT("&ntext not defined", __func__, 1);
    *pntext = 0;
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    n = pixaGetCount(pixa);
    for (i = 0; i < n; i++) {
        if ((pix = pixaGetPix(pixa, i, L_CLONE)) == NULL)
            continue;
        text = pixGetText(pix);
        if (text && strlen(text) > 0)
            (*pntext)++;
        pixDestroy(&pix);
    }

    return 0;
}


/*!
 * \brief   pixaSetText()
 *
 * \param[in]    pixa
 * \param[in]    text  [optional] single text string, to insert in each pix
 * \param[in]    sa    [optional] array of text strings, to insert in each pix
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) To clear all the text fields, use %sa == NULL and %text == NULL.
 *      (2) Otherwise, this replaces all text fields with a copy of a string,
 *          either the same string or a string from %sa.
 *      (3) To set all the text fields to the same value %text, use %sa = NULL.
 *      (4) If %sa is defined, ignore %text and use the strings in %sa.
 *          %sa must have the same count as %pixa.
 * </pre>
 */
l_ok
pixaSetText(PIXA        *pixa,
            const char  *text,
            SARRAY      *sa)
{
char    *str;
l_int32  i, n;
PIX     *pix;

    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    n = pixaGetCount(pixa);
    if (sa && (sarrayGetCount(sa) != n))
        return ERROR_INT("pixa and sa sizes differ", __func__, 1);

    if (!sa) {
        for (i = 0; i < n; i++) {
            if ((pix = pixaGetPix(pixa, i, L_CLONE)) == NULL)
                continue;
            pixSetText(pix, text);
            pixDestroy(&pix);
        }
        return 0;
    }

    for (i = 0; i < n; i++) {
        if ((pix = pixaGetPix(pixa, i, L_CLONE)) == NULL)
            continue;
        str = sarrayGetString(sa, i, L_NOCOPY);
        pixSetText(pix, str);
        pixDestroy(&pix);
    }

    return 0;
}


/*!
 * \brief   pixaGetLinePtrs()
 *
 * \param[in]    pixa    of pix that all have the same depth
 * \param[out]   psize   [optional] number of pix in the pixa
 * \return  array of array of line ptrs, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See pixGetLinePtrs() for details.
 *      (2) It is best if all pix in the pixa are the same size.
 *          The size of each line ptr array is equal to the height
 *          of the pix that it refers to.
 *      (3) This is an array of arrays.  To destroy it:
 *            for (i = 0; i < size; i++)
 *                LEPT_FREE(lineset[i]);
 *            LEPT_FREE(lineset);
 * </pre>
 */
void ***
pixaGetLinePtrs(PIXA     *pixa,
                l_int32  *psize)
{
l_int32  i, n, same;
void   **lineptrs;
void  ***lineset;
PIX     *pix;

    if (psize) *psize = 0;
    if (!pixa)
        return (void ***)ERROR_PTR("pixa not defined", __func__, NULL);
    pixaVerifyDepth(pixa, &same, NULL);
    if (!same)
        return (void ***)ERROR_PTR("pixa not all same depth", __func__, NULL);
    n = pixaGetCount(pixa);
    if (psize) *psize = n;
    if ((lineset = (void ***)LEPT_CALLOC(n, sizeof(void **))) == NULL)
        return (void ***)ERROR_PTR("lineset not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixa, i, L_CLONE);
        lineptrs = pixGetLinePtrs(pix, NULL);
        lineset[i] = lineptrs;
        pixDestroy(&pix);
    }

    return lineset;
}


/*---------------------------------------------------------------------*
 *                         Pixa output info                            *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaWriteStreamInfo()
 *
 * \param[in]    fp     file stream
 * \param[in]    pixa
 * \return  0 if OK, 1 on error.
 *
 * <pre>
 * Notes:
 *      (1) For each pix in the pixa, write out the pix dimensions, spp,
 *          text string (if it exists), and cmap info.
 * </pre>
 */
l_ok
pixaWriteStreamInfo(FILE  *fp,
                    PIXA  *pixa)
{
char     *text;
l_int32   i, n, w, h, d, spp, count, hastext;
PIX      *pix;
PIXCMAP  *cmap;

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    n = pixaGetCount(pixa);
    for (i = 0; i < n; i++) {
        if ((pix = pixaGetPix(pixa, i, L_CLONE)) == NULL) {
            fprintf(fp, "%d: no pix at this index\n", i);
            continue;
        }
        pixGetDimensions(pix, &w, &h, &d);
        spp = pixGetSpp(pix);
        text = pixGetText(pix);
        hastext = (text && strlen(text) > 0);
        if ((cmap = pixGetColormap(pix)) != NULL)
            count = pixcmapGetCount(cmap);
        fprintf(fp, "Pix %d: w = %d, h = %d, d = %d, spp = %d",
                i, w, h, d, spp);
        if (cmap) fprintf(fp, ", cmap(%d colors)", count);
        if (hastext) fprintf(fp, ", text = %s", text);
        fprintf(fp, "\n");
        pixDestroy(&pix);
    }

    return 0;
}


/*---------------------------------------------------------------------*
 *                       Pixa array modifiers                          *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaReplacePix()
 *
 * \param[in]    pixa
 * \param[in]    index   to the index-th pix
 * \param[in]    pix     insert to replace existing one
 * \param[in]    box     [optional] insert to replace existing
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) In-place replacement of one pix.
 *      (2) The previous pix at that location is destroyed.
 * </pre>
 */
l_ok
pixaReplacePix(PIXA    *pixa,
               l_int32  index,
               PIX     *pix,
               BOX     *box)
{
BOXA  *boxa;

    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (index < 0 || index >= pixa->n)
        return ERROR_INT("index not valid", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    pixDestroy(&(pixa->pix[index]));
    pixa->pix[index] = pix;

    if (box) {
        boxa = pixa->boxa;
        if (index > boxa->n)
            return ERROR_INT("boxa index not valid", __func__, 1);
        boxaReplaceBox(boxa, index, box);
    }

    return 0;
}


/*!
 * \brief   pixaInsertPix()
 *
 * \param[in]    pixa
 * \param[in]    index   at which pix is to be inserted
 * \param[in]    pixs    new pix to be inserted
 * \param[in]    box     [optional] new box to be inserted
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This shifts pixa[i] --> pixa[i + 1] for all i >= index,
 *          and then inserts at pixa[index].
 *      (2) To insert at the beginning of the array, set index = 0.
 *      (3) It should not be used repeatedly on large arrays,
 *          because the function is O(n).
 *      (4) To append a pix to a pixa, it's easier to use pixaAddPix().
 * </pre>
 */
l_ok
pixaInsertPix(PIXA    *pixa,
              l_int32  index,
              PIX     *pixs,
              BOX     *box)
{
l_int32  i, n;

    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    n = pixaGetCount(pixa);
    if (index < 0 || index > n) {
        L_ERROR("index %d not in [0,...,%d]\n", __func__, index, n);
        return 1;
    }
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

    if (n >= pixa->nalloc) {  /* extend both ptr arrays */
        if (pixaExtendArray(pixa))
            return ERROR_INT("extension failed", __func__, 1);
        if (boxaExtendArray(pixa->boxa))
            return ERROR_INT("extension failed", __func__, 1);
    }
    pixa->n++;
    for (i = n; i > index; i--)
      pixa->pix[i] = pixa->pix[i - 1];
    pixa->pix[index] = pixs;

        /* Optionally, insert the box */
    if (box)
        boxaInsertBox(pixa->boxa, index, box);
    return 0;
}


/*!
 * \brief   pixaRemovePix()
 *
 * \param[in]    pixa
 * \param[in]    index    of pix to be removed
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This shifts pixa[i] --> pixa[i - 1] for all i > index.
 *      (2) It should not be used repeatedly on large arrays,
 *          because the function is O(n).
 *      (3) The corresponding box is removed as well, if it exists.
 * </pre>
 */
l_ok
pixaRemovePix(PIXA    *pixa,
              l_int32  index)
{
l_int32  i, n, nbox;
BOXA    *boxa;
PIX    **array;

    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    n = pixaGetCount(pixa);
    if (index < 0 || index >= n) {
        L_ERROR("index %d not in [0,...,%d]\n", __func__, index, n - 1);
        return 1;
    }

        /* Remove the pix */
    array = pixa->pix;
    pixDestroy(&array[index]);
    for (i = index + 1; i < n; i++)
        array[i - 1] = array[i];
    array[n - 1] = NULL;
    pixa->n--;

        /* Remove the box if it exists */
    boxa = pixa->boxa;
    nbox = boxaGetCount(boxa);
    if (index < nbox)
        boxaRemoveBox(boxa, index);

    return 0;
}


/*!
 * \brief   pixaRemovePixAndSave()
 *
 * \param[in]    pixa
 * \param[in]    index   of pix to be removed
 * \param[out]   ppix    [optional] removed pix
 * \param[out]   pbox    [optional] removed box
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This shifts pixa[i] --> pixa[i - 1] for all i > index.
 *      (2) It should not be used repeatedly on large arrays,
 *          because the function is O(n).
 *      (3) The corresponding box is removed as well, if it exists.
 *      (4) The removed pix and box can either be retained or destroyed.
 * </pre>
 */
l_ok
pixaRemovePixAndSave(PIXA    *pixa,
                     l_int32  index,
                     PIX    **ppix,
                     BOX    **pbox)
{
l_int32  i, n, nbox;
BOXA    *boxa;
PIX    **array;

    if (ppix) *ppix = NULL;
    if (pbox) *pbox = NULL;
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    n = pixaGetCount(pixa);
    if (index < 0 || index >= n) {
        L_ERROR("index %d not in [0,...,%d]\n", __func__, index, n - 1);
        return 1;
    }

        /* Remove the pix */
    array = pixa->pix;
    if (ppix)
        *ppix = pixaGetPix(pixa, index, L_CLONE);
    pixDestroy(&array[index]);
    for (i = index + 1; i < n; i++)
        array[i - 1] = array[i];
    array[n - 1] = NULL;
    pixa->n--;

        /* Remove the box if it exists  */
    boxa = pixa->boxa;
    nbox = boxaGetCount(boxa);
    if (index < nbox)
        boxaRemoveBoxAndSave(boxa, index, pbox);

    return 0;
}


/*!
 * \brief   pixaRemoveSelected()
 *
 * \param[in]    pixa
 * \param[in]    naindex   numa of indices of pix to be removed
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This gives error messages for invalid indices
 * </pre>
 */
l_ok
pixaRemoveSelected(PIXA  *pixa,
                   NUMA  *naindex)
{
l_int32  i, n, index;
NUMA    *na1;

    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (!naindex)
        return ERROR_INT("naindex not defined", __func__, 1);
    if ((n = numaGetCount(naindex)) == 0)
        return ERROR_INT("naindex is empty", __func__, 1);

        /* Remove from highest indices first */
    na1 = numaSort(NULL, naindex, L_SORT_DECREASING);
    for (i = 0; i < n; i++) {
        numaGetIValue(na1, i, &index);
        pixaRemovePix(pixa, index);
    }
    numaDestroy(&na1);
    return 0;
}


/*!
 * \brief   pixaInitFull()
 *
 * \param[in]    pixa   typically empty
 * \param[in]    pix    [optional] to be replicated to the entire pixa ptr array
 * \param[in]    box    [optional] to be replicated to the entire boxa ptr array
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This initializes a pixa by filling up the entire pix ptr array
 *          with copies of %pix.  If %pix == NULL, we use a tiny placeholder
 *          pix (w = h = d = 1).  Any existing pix are destroyed.
 *          It also optionally fills the boxa with copies of %box.
 *          After this operation, the numbers of pix and (optionally)
 *          boxes are equal to the number of allocated ptrs.
 *      (2) Note that we use pixaReplacePix() instead of pixaInsertPix().
 *          They both have the same effect when inserting into a NULL ptr
 *          in the pixa ptr array:
 *      (3) If the boxa is not initialized (i.e., filled with boxes),
 *          later insertion of boxes will cause an error, because the
 *          'n' field is 0.
 *      (4) Example usage.  This function is useful to prepare for a
 *          random insertion (or replacement) of pix into a pixa.
 *          To randomly insert pix into a pixa, without boxes, up to
 *          some index "max":
 *             Pixa *pixa = pixaCreate(max);
 *             pixaInitFull(pixa, NULL, NULL);
 *          An existing pixa with a smaller ptr array can also be reused:
 *             pixaExtendArrayToSize(pixa, max);
 *             pixaInitFull(pixa, NULL, NULL);
 *          The initialization allows the pixa to always be properly
 *          filled, even if all pix (and boxes) are not later replaced.
 * </pre>
 */
l_ok
pixaInitFull(PIXA  *pixa,
             PIX   *pix,
             BOX   *box)
{
l_int32  i, n;
PIX     *pix1;

    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    n = pixa->nalloc;
    pixa->n = n;
    for (i = 0; i < n; i++) {
        if (pix)
            pix1 = pixCopy(NULL, pix);
        else
            pix1 = pixCreate(1, 1, 1);
        pixaReplacePix(pixa, i, pix1, NULL);
    }
    if (box)
        boxaInitFull(pixa->boxa, box);

    return 0;
}


/*!
 * \brief   pixaClear()
 *
 * \param[in]    pixa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This destroys all pix in the pixa, as well as
 *          all boxes in the boxa.  The ptrs in the pix ptr array
 *          are all null'd.  The number of allocated pix, n, is set to 0.
 * </pre>
 */
l_ok
pixaClear(PIXA  *pixa)
{
l_int32  i, n;

    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    n = pixaGetCount(pixa);
    for (i = 0; i < n; i++)
        pixDestroy(&pixa->pix[i]);
    pixa->n = 0;
    return boxaClear(pixa->boxa);
}


/*---------------------------------------------------------------------*
 *                      Pixa and Pixaa combination                     *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaJoin()
 *
 * \param[in]    pixad    dest pixa; add to this one
 * \param[in]    pixas    [optional] source pixa; add from this one
 * \param[in]    istart   starting index in pixas
 * \param[in]    iend     ending index in pixas; use -1 to cat all
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This appends a clone of each indicated pix in pixas to pixad
 *      (2) istart < 0 is taken to mean 'read from the start' (istart = 0)
 *      (3) iend < 0 means 'read to the end'
 *      (4) If pixas is NULL or contains no pix, this is a no-op.
 * </pre>
 */
l_ok
pixaJoin(PIXA    *pixad,
         PIXA    *pixas,
         l_int32  istart,
         l_int32  iend)
{
l_int32  i, n, nb;
BOXA    *boxas, *boxad;
PIX     *pix;

    if (!pixad)
        return ERROR_INT("pixad not defined", __func__, 1);
    if (!pixas || ((n = pixaGetCount(pixas)) == 0))
        return 0;

    if (istart < 0)
        istart = 0;
    if (iend < 0 || iend >= n)
        iend = n - 1;
    if (istart > iend)
        return ERROR_INT("istart > iend; nothing to add", __func__, 1);

    for (i = istart; i <= iend; i++) {
        pix = pixaGetPix(pixas, i, L_CLONE);
        pixaAddPix(pixad, pix, L_INSERT);
    }

    boxas = pixaGetBoxa(pixas, L_CLONE);
    boxad = pixaGetBoxa(pixad, L_CLONE);
    nb = pixaGetBoxaCount(pixas);
    iend = L_MIN(iend, nb - 1);
    boxaJoin(boxad, boxas, istart, iend);
    boxaDestroy(&boxas);  /* just the clones */
    boxaDestroy(&boxad);
    return 0;
}


/*!
 * \brief   pixaInterleave()
 *
 * \param[in]    pixa1      first src pixa
 * \param[in]    pixa2      second src pixa
 * \param[in]    copyflag   L_CLONE, L_COPY
 * \return  pixa  interleaved from sources, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) %copyflag determines if the pix are copied or cloned.
 *          The boxes, if they exist, are copied.
 *      (2) If the two pixa have different sizes, a warning is issued,
 *          and the number of pairs returned is the minimum size.
 * </pre>
 */
PIXA *
pixaInterleave(PIXA    *pixa1,
               PIXA    *pixa2,
               l_int32  copyflag)
{
l_int32  i, n1, n2, n, nb1, nb2;
BOX     *box;
PIX     *pix;
PIXA    *pixad;

    if (!pixa1)
        return (PIXA *)ERROR_PTR("pixa1 not defined", __func__, NULL);
    if (!pixa2)
        return (PIXA *)ERROR_PTR("pixa2 not defined", __func__, NULL);
    if (copyflag != L_COPY && copyflag != L_CLONE)
        return (PIXA *)ERROR_PTR("invalid copyflag", __func__, NULL);
    n1 = pixaGetCount(pixa1);
    n2 = pixaGetCount(pixa2);
    n = L_MIN(n1, n2);
    if (n == 0)
        return (PIXA *)ERROR_PTR("at least one input pixa is empty",
                                 __func__, NULL);
    if (n1 != n2)
        L_WARNING("counts differ: %d != %d\n", __func__, n1, n2);

    pixad = pixaCreate(2 * n);
    nb1 = pixaGetBoxaCount(pixa1);
    nb2 = pixaGetBoxaCount(pixa2);
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixa1, i, copyflag);
        pixaAddPix(pixad, pix, L_INSERT);
        if (i < nb1) {
            box = pixaGetBox(pixa1, i, L_COPY);
            pixaAddBox(pixad, box, L_INSERT);
        }
        pix = pixaGetPix(pixa2, i, copyflag);
        pixaAddPix(pixad, pix, L_INSERT);
        if (i < nb2) {
            box = pixaGetBox(pixa2, i, L_COPY);
            pixaAddBox(pixad, box, L_INSERT);
        }
    }

    return pixad;
}


/*!
 * \brief   pixaaJoin()
 *
 * \param[in]    paad     dest pixaa; add to this one
 * \param[in]    paas     [optional] source pixaa; add from this one
 * \param[in]    istart   starting index in pixaas
 * \param[in]    iend     ending index in pixaas; use -1 to cat all
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This appends a clone of each indicated pixa in paas to pixaad
 *      (2) istart < 0 is taken to mean 'read from the start' (istart = 0)
 *      (3) iend < 0 means 'read to the end'
 * </pre>
 */
l_ok
pixaaJoin(PIXAA   *paad,
          PIXAA   *paas,
          l_int32  istart,
          l_int32  iend)
{
l_int32  i, n;
PIXA    *pixa;

    if (!paad)
        return ERROR_INT("pixaad not defined", __func__, 1);
    if (!paas)
        return 0;

    if (istart < 0)
        istart = 0;
    n = pixaaGetCount(paas, NULL);
    if (iend < 0 || iend >= n)
        iend = n - 1;
    if (istart > iend)
        return ERROR_INT("istart > iend; nothing to add", __func__, 1);

    for (i = istart; i <= iend; i++) {
        pixa = pixaaGetPixa(paas, i, L_CLONE);
        pixaaAddPixa(paad, pixa, L_INSERT);
    }

    return 0;
}


/*---------------------------------------------------------------------*
 *                    Pixaa creation and destruction                   *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaaCreate()
 *
 * \param[in]    n    initial number of pixa ptrs
 * \return  paa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) A pixaa provides a 2-level hierarchy of images.
 *          A common use is for segmentation masks, which are
 *          inexpensive to store in png format.
 *      (2) For example, suppose you want a mask for each textline
 *          in a two-column page.  The textline masks for each column
 *          can be represented by a pixa, of which there are 2 in the pixaa.
 *          The boxes for the textline mask components within a column
 *          can have their origin referred to the column rather than the page.
 *          Then the boxa field can be used to represent the two box (regions)
 *          for the columns, and the (x,y) components of each box can
 *          be used to get the absolute position of the textlines on
 *          the page.
 * </pre>
 */
PIXAA *
pixaaCreate(l_int32  n)
{
PIXAA  *paa;

    if (n <= 0 || n > MaxInitPtrArraySize)
        n = InitialPtrArraySize;

    paa = (PIXAA *)LEPT_CALLOC(1, sizeof(PIXAA));
    paa->n = 0;
    paa->nalloc = n;
    if ((paa->pixa = (PIXA **)LEPT_CALLOC(n, sizeof(PIXA *))) == NULL) {
        pixaaDestroy(&paa);
        return (PIXAA *)ERROR_PTR("pixa ptrs not made", __func__, NULL);
    }
    paa->boxa = boxaCreate(n);

    return paa;
}


/*!
 * \brief   pixaaCreateFromPixa()
 *
 * \param[in]    pixa
 * \param[in]    n          number specifying subdivision of pixa
 * \param[in]    type       L_CHOOSE_CONSECUTIVE, L_CHOOSE_SKIP_BY
 * \param[in]    copyflag   L_CLONE, L_COPY
 * \return  paa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This subdivides a pixa into a set of smaller pixa that
 *          are accumulated into a pixaa.
 *      (2) If type == L_CHOOSE_CONSECUTIVE, the first 'n' pix are
 *          put in a pixa and added to pixaa, then the next 'n', etc.
 *          If type == L_CHOOSE_SKIP_BY, the first pixa is made by
 *          aggregating pix[0], pix[n], pix[2*n], etc.
 *      (3) The copyflag specifies if each new pix is a copy or a clone.
 * </pre>
 */
PIXAA *
pixaaCreateFromPixa(PIXA    *pixa,
                    l_int32  n,
                    l_int32  type,
                    l_int32  copyflag)
{
l_int32  count, i, j, npixa;
PIX     *pix;
PIXA    *pixat = NULL;
PIXAA   *paa;

    if (!pixa)
        return (PIXAA *)ERROR_PTR("pixa not defined", __func__, NULL);
    count = pixaGetCount(pixa);
    if (count == 0)
        return (PIXAA *)ERROR_PTR("no pix in pixa", __func__, NULL);
    if (n <= 0)
        return (PIXAA *)ERROR_PTR("n must be > 0", __func__, NULL);
    if (type != L_CHOOSE_CONSECUTIVE && type != L_CHOOSE_SKIP_BY)
        return (PIXAA *)ERROR_PTR("invalid type", __func__, NULL);
    if (copyflag != L_CLONE && copyflag != L_COPY)
        return (PIXAA *)ERROR_PTR("invalid copyflag", __func__, NULL);

    if (type == L_CHOOSE_CONSECUTIVE)
        npixa = (count + n - 1) / n;
    else  /* L_CHOOSE_SKIP_BY */
        npixa = L_MIN(n, count);
    paa = pixaaCreate(npixa);
    if (type == L_CHOOSE_CONSECUTIVE) {
        for (i = 0; i < count; i++) {
            if (i % n == 0)
                pixat = pixaCreate(n);
            pix = pixaGetPix(pixa, i, copyflag);
            pixaAddPix(pixat, pix, L_INSERT);
            if (i % n == n - 1)
                pixaaAddPixa(paa, pixat, L_INSERT);
        }
        if (i % n != 0)
            pixaaAddPixa(paa, pixat, L_INSERT);
    } else {  /* L_CHOOSE_SKIP_BY */
        for (i = 0; i < npixa; i++) {
            pixat = pixaCreate(count / npixa + 1);
            for (j = i; j < count; j += n) {
                pix = pixaGetPix(pixa, j, copyflag);
                pixaAddPix(pixat, pix, L_INSERT);
            }
            pixaaAddPixa(paa, pixat, L_INSERT);
        }
    }

    return paa;
}


/*!
 * \brief   pixaaDestroy()
 *
 * \param[in,out]   ppaa    use ptr address so it will be nulled
 * \return  void
 */
void
pixaaDestroy(PIXAA  **ppaa)
{
l_int32  i;
PIXAA   *paa;

    if (ppaa == NULL) {
        L_WARNING("ptr address is NULL!\n", __func__);
        return;
    }

    if ((paa = *ppaa) == NULL)
        return;

    for (i = 0; i < paa->n; i++)
        pixaDestroy(&paa->pixa[i]);
    LEPT_FREE(paa->pixa);
    boxaDestroy(&paa->boxa);
    LEPT_FREE(paa);
    *ppaa = NULL;
}


/*---------------------------------------------------------------------*
 *                             Pixaa addition                          *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaaAddPixa()
 *
 * \param[in]    paa
 * \param[in]    pixa    to be added
 * \param[in]    copyflag:
 *                 L_INSERT inserts the pixa directly;
 *                 L_COPY makes a new pixa and copies each pix and each box;
 *                 L_CLONE gives a new handle to the input pixa;
 *                 L_COPY_CLONE makes a new pixa and inserts clones of
 *                     all pix and boxes
 * \return  0 if OK; 1 on error
 */
l_ok
pixaaAddPixa(PIXAA   *paa,
             PIXA    *pixa,
             l_int32  copyflag)
{
l_int32  n;
PIXA    *pixac;

    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (copyflag != L_INSERT && copyflag != L_COPY &&
        copyflag != L_CLONE && copyflag != L_COPY_CLONE)
        return ERROR_INT("invalid copyflag", __func__, 1);

    if (copyflag == L_INSERT) {
        pixac = pixa;
    } else {
        if ((pixac = pixaCopy(pixa, copyflag)) == NULL)
            return ERROR_INT("pixac not made", __func__, 1);
    }

    n = pixaaGetCount(paa, NULL);
    if (n >= paa->nalloc) {
        if (pixaaExtendArray(paa)) {
            if (copyflag != L_INSERT)
                pixaDestroy(&pixac);
            return ERROR_INT("extension failed", __func__, 1);
        }
    }
    paa->pixa[n] = pixac;
    paa->n++;
    return 0;
}


/*!
 * \brief   pixaaExtendArray()
 *
 * \param[in]    paa
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The max number of pixa ptrs is 5M.  The reason it is so large
 *          is that some applications, like jbig2enc, can create a very
 *          large array of Pixa, each representing a character class
 *          that contains one or a few tiny bitmaps.
 * </pre>
 */
static l_int32
pixaaExtendArray(PIXAA  *paa)
{
size_t  oldsize, newsize;

    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);
    if (paa->nalloc > MaxPixaaPtrArraySize)  /* belt & suspenders */
        return ERROR_INT("paa has too many ptrs", __func__, 1);
    oldsize = paa->nalloc * sizeof(PIXA *);
    newsize = 2 * oldsize;
    if (newsize > 8 * MaxPixaaPtrArraySize) {
        L_ERROR("newsize = %zu > 40 MB; too large\n", __func__, newsize);
        return 1;
    }
    if ((paa->pixa = (PIXA **)reallocNew((void **)&paa->pixa,
                                         oldsize, newsize)) == NULL)
        return ERROR_INT("new ptr array not returned", __func__, 1);

    paa->nalloc *= 2;
    return 0;
}


/*!
 * \brief   pixaaAddPix()
 *
 * \param[in]    paa        input paa
 * \param[in]    index      index of pixa in paa
 * \param[in]    pix        to be added
 * \param[in]    box        [optional] to be added
 * \param[in]    copyflag   L_INSERT, L_COPY, L_CLONE
 * \return  0 if OK; 1 on error
 */
l_ok
pixaaAddPix(PIXAA   *paa,
            l_int32  index,
            PIX     *pix,
            BOX     *box,
            l_int32  copyflag)
{
PIXA  *pixa;

    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    if ((pixa = pixaaGetPixa(paa, index, L_CLONE)) == NULL)
        return ERROR_INT("pixa not found", __func__, 1);
    pixaAddPix(pixa, pix, copyflag);
    if (box) pixaAddBox(pixa, box, copyflag);
    pixaDestroy(&pixa);
    return 0;
}


/*!
 * \brief   pixaaAddBox()
 *
 * \param[in]    paa
 * \param[in]    box
 * \param[in]    copyflag    L_INSERT, L_COPY, L_CLONE
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The box can be used, for example, to hold the support region
 *          of a pixa that is being added to the pixaa.
 * </pre>
 */
l_ok
pixaaAddBox(PIXAA   *paa,
            BOX     *box,
            l_int32  copyflag)
{
    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);
    if (copyflag != L_INSERT && copyflag != L_COPY && copyflag != L_CLONE)
        return ERROR_INT("invalid copyflag", __func__, 1);

    boxaAddBox(paa->boxa, box, copyflag);
    return 0;
}



/*---------------------------------------------------------------------*
 *                            Pixaa accessors                          *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaaGetCount()
 *
 * \param[in]    paa
 * \param[out]   pna    [optional] number of pix in each pixa
 * \return  count, or 0 if no pixaa
 *
 * <pre>
 * Notes:
 *      (1) If paa is empty, a returned na will also be empty.
 * </pre>
 */
l_int32
pixaaGetCount(PIXAA  *paa,
              NUMA  **pna)
{
l_int32  i, n;
NUMA    *na;
PIXA    *pixa;

    if (pna) *pna = NULL;
    if (!paa)
        return ERROR_INT("paa not defined", __func__, 0);

    n = paa->n;
    if (pna) {
        if ((na = numaCreate(n)) == NULL)
            return ERROR_INT("na not made", __func__, 0);
        *pna = na;
        for (i = 0; i < n; i++) {
            pixa = pixaaGetPixa(paa, i, L_CLONE);
            numaAddNumber(na, pixaGetCount(pixa));
            pixaDestroy(&pixa);
        }
    }
    return n;
}


/*!
 * \brief   pixaaGetPixa()
 *
 * \param[in]    paa
 * \param[in]    index        to the index-th pixa
 * \param[in]    accesstype   L_COPY, L_CLONE, L_COPY_CLONE
 * \return  pixa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) L_COPY makes a new pixa with a copy of every pix
 *      (2) L_CLONE just makes a new reference to the pixa,
 *          and bumps the counter.  You would use this, for example,
 *          when you need to extract some data from a pix within a
 *          pixa within a pixaa.
 *      (3) L_COPY_CLONE makes a new pixa with a clone of every pix
 *          and box
 *      (4) In all cases, you must invoke pixaDestroy() on the returned pixa
 * </pre>
 */
PIXA *
pixaaGetPixa(PIXAA   *paa,
             l_int32  index,
             l_int32  accesstype)
{
PIXA  *pixa;

    if (!paa)
        return (PIXA *)ERROR_PTR("paa not defined", __func__, NULL);
    if (index < 0 || index >= paa->n)
        return (PIXA *)ERROR_PTR("index not valid", __func__, NULL);
    if (accesstype != L_COPY && accesstype != L_CLONE &&
        accesstype != L_COPY_CLONE)
        return (PIXA *)ERROR_PTR("invalid accesstype", __func__, NULL);

    if ((pixa = paa->pixa[index]) == NULL) {  /* shouldn't happen! */
        L_ERROR("missing pixa[%d]\n", __func__, index);
        return (PIXA *)ERROR_PTR("pixa not found at index", __func__, NULL);
    }
    return pixaCopy(pixa, accesstype);
}


/*!
 * \brief   pixaaGetBoxa()
 *
 * \param[in]    paa
 * \param[in]    accesstype    L_COPY, L_CLONE
 * \return  boxa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) L_COPY returns a copy; L_CLONE returns a new reference to the boxa.
 *      (2) In both cases, invoke boxaDestroy() on the returned boxa.
 * </pre>
 */
BOXA *
pixaaGetBoxa(PIXAA   *paa,
             l_int32  accesstype)
{
    if (!paa)
        return (BOXA *)ERROR_PTR("paa not defined", __func__, NULL);
    if (accesstype != L_COPY && accesstype != L_CLONE)
        return (BOXA *)ERROR_PTR("invalid access type", __func__, NULL);

    return boxaCopy(paa->boxa, accesstype);
}


/*!
 * \brief   pixaaGetPix()
 *
 * \param[in]    paa
 * \param[in]    index        index into the pixa array in the pixaa
 * \param[in]    ipix         index into the pix array in the pixa
 * \param[in]    accessflag   L_COPY or L_CLONE
 * \return  pix, or NULL on error
 */
PIX *
pixaaGetPix(PIXAA   *paa,
            l_int32  index,
            l_int32  ipix,
            l_int32  accessflag)
{
PIX   *pix;
PIXA  *pixa;

    if ((pixa = pixaaGetPixa(paa, index, L_CLONE)) == NULL)
        return (PIX *)ERROR_PTR("pixa not retrieved", __func__, NULL);
    if ((pix = pixaGetPix(pixa, ipix, accessflag)) == NULL)
        L_ERROR("pix not retrieved\n", __func__);
    pixaDestroy(&pixa);
    return pix;
}


/*!
 * \brief   pixaaVerifyDepth()
 *
 * \param[in]    paa
 * \param[out]   psame   1 if all pix have the same depth; 0 otherwise
 * \param[out]   pmaxd   [optional] max depth of all pix in pixaa
 * \return   0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) It is considered to be an error if any pixa have no pix.
 * </pre>
 */
l_ok
pixaaVerifyDepth(PIXAA    *paa,
                 l_int32  *psame,
                 l_int32  *pmaxd)
{
l_int32  i, n, d, maxd, same, samed;
PIXA    *pixa;

    if (pmaxd) *pmaxd = 0;
    if (!psame)
        return ERROR_INT("psame not defined", __func__, 1);
    *psame = 0;
    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);
    if ((n = pixaaGetCount(paa, NULL)) == 0)
        return ERROR_INT("no pixa in paa", __func__, 1);

    pixa = pixaaGetPixa(paa, 0, L_CLONE);
    pixaVerifyDepth(pixa, &same, &maxd);  /* init same, maxd with first pixa */
    pixaDestroy(&pixa);
    for (i = 1; i < n; i++) {
        pixa = pixaaGetPixa(paa, i, L_CLONE);
        pixaVerifyDepth(pixa, &samed, &d);
        pixaDestroy(&pixa);
        maxd = L_MAX(maxd, d);
        if (!samed || maxd != d)
            same = 0;
    }
    *psame = same;
    if (pmaxd) *pmaxd = maxd;
    return 0;
}


/*!
 * \brief   pixaaVerifyDimensions()
 *
 * \param[in]    paa
 * \param[out]   psame   1 if all pix have the same depth; 0 otherwise
 * \param[out]   pmaxw   [optional] max width of all pix in pixaa
 * \param[out]   pmaxh   [optional] max height of all pix in pixaa
 * \return   0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) It is considered to be an error if any pixa have no pix.
 * </pre>
 */
l_ok
pixaaVerifyDimensions(PIXAA    *paa,
                      l_int32  *psame,
                      l_int32  *pmaxw,
                      l_int32  *pmaxh)
{
l_int32  i, n, w, h, maxw, maxh, same, same2;
PIXA    *pixa;

    if (pmaxw) *pmaxw = 0;
    if (pmaxh) *pmaxh = 0;
    if (!psame)
        return ERROR_INT("psame not defined", __func__, 1);
    *psame = 0;
    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);
    if ((n = pixaaGetCount(paa, NULL)) == 0)
        return ERROR_INT("no pixa in paa", __func__, 1);

        /* Init same; init maxw and maxh from first pixa */
    pixa = pixaaGetPixa(paa, 0, L_CLONE);
    pixaVerifyDimensions(pixa, &same, &maxw, &maxh);
    pixaDestroy(&pixa);

    for (i = 1; i < n; i++) {
        pixa = pixaaGetPixa(paa, i, L_CLONE);
        pixaVerifyDimensions(pixa, &same2, &w, &h);
        pixaDestroy(&pixa);
        maxw = L_MAX(maxw, w);
        maxh = L_MAX(maxh, h);
        if (!same2 || maxw != w || maxh != h)
            same = 0;
    }
    *psame = same;
    if (pmaxw) *pmaxw = maxw;
    if (pmaxh) *pmaxh = maxh;
    return 0;
}


/*!
 * \brief   pixaaIsFull()
 *
 * \param[in]    paa
 * \param[out]   pfull    1 if all pixa in the paa have full pix arrays
 * \return  return 0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Does not require boxa associated with each pixa to be full.
 * </pre>
 */
l_int32
pixaaIsFull(PIXAA    *paa,
            l_int32  *pfull)
{
l_int32  i, n, full;
PIXA    *pixa;

    if (!pfull)
        return ERROR_INT("&full not defined", __func__, 0);
    *pfull = 0;
    if (!paa)
        return ERROR_INT("paa not defined", __func__, 0);

    n = pixaaGetCount(paa, NULL);
    full = 1;
    for (i = 0; i < n; i++) {
        pixa = pixaaGetPixa(paa, i, L_CLONE);
        pixaIsFull(pixa, &full, NULL);
        pixaDestroy(&pixa);
        if (!full) break;
    }
    *pfull = full;
    return 0;
}


/*---------------------------------------------------------------------*
 *                         Pixaa array modifiers                       *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaaInitFull()
 *
 * \param[in]    paa     typically empty
 * \param[in]    pixa    to be replicated into the entire pixa ptr array
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This initializes a pixaa by filling up the entire pixa ptr array
 *          with copies of %pixa.  Any existing pixa are destroyed.
 *      (2) Example usage.  This function is useful to prepare for a
 *          random insertion (or replacement) of pixa into a pixaa.
 *          To randomly insert pixa into a pixaa, up to some index "max":
 *             Pixaa *paa = pixaaCreate(max);
 *             Pixa *pixa = pixaCreate(1);  // if you want little memory
 *             pixaaInitFull(paa, pixa);  // copy it to entire array
 *             pixaDestroy(&pixa);  // no longer needed
 *          The initialization allows the pixaa to always be properly filled.
 * </pre>
 */
l_ok
pixaaInitFull(PIXAA  *paa,
              PIXA   *pixa)
{
l_int32  i, n;
PIXA    *pixat;

    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    n = paa->nalloc;
    paa->n = n;
    for (i = 0; i < n; i++) {
        pixat = pixaCopy(pixa, L_COPY);
        pixaaReplacePixa(paa, i, pixat);
    }

    return 0;
}


/*!
 * \brief   pixaaReplacePixa()
 *
 * \param[in]    paa
 * \param[in]    index  to the index-th pixa
 * \param[in]    pixa   insert to replace existing one
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This allows random insertion of a pixa into a pixaa, with
 *          destruction of any existing pixa at that location.
 *          The input pixa is now owned by the pixaa.
 *      (2) No other pixa in the array are affected.
 *      (3) The index must be within the allowed set.
 * </pre>
 */
l_ok
pixaaReplacePixa(PIXAA   *paa,
                 l_int32  index,
                 PIXA    *pixa)
{

    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);
    if (index < 0 || index >= paa->n)
        return ERROR_INT("index not valid", __func__, 1);
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    pixaDestroy(&(paa->pixa[index]));
    paa->pixa[index] = pixa;
    return 0;
}


/*!
 * \brief   pixaaClear()
 *
 * \param[in]    paa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This destroys all pixa in the pixaa, and nulls the ptrs
 *          in the pixa ptr array.
 * </pre>
 */
l_ok
pixaaClear(PIXAA  *paa)
{
l_int32  i, n;

    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);

    n = pixaaGetCount(paa, NULL);
    for (i = 0; i < n; i++)
        pixaDestroy(&paa->pixa[i]);
    paa->n = 0;
    return 0;
}


/*!
 * \brief   pixaaTruncate()
 *
 * \param[in]    paa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This identifies the largest index containing a pixa that
 *          has any pix within it, destroys all pixa above that index,
 *          and resets the count.
 * </pre>
 */
l_ok
pixaaTruncate(PIXAA  *paa)
{
l_int32  i, n, np;
PIXA    *pixa;

    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);

    n = pixaaGetCount(paa, NULL);
    for (i = n - 1; i >= 0; i--) {
        pixa = pixaaGetPixa(paa, i, L_CLONE);
        if (!pixa) {
            paa->n--;
            continue;
        }
        np = pixaGetCount(pixa);
        pixaDestroy(&pixa);
        if (np == 0) {
            pixaDestroy(&paa->pixa[i]);
            paa->n--;
        } else {
            break;
        }
    }
    return 0;
}



/*---------------------------------------------------------------------*
 *                          Pixa serialized I/O                        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaRead()
 *
 * \param[in]    filename
 * \return  pixa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The pix are stored in the file as png.
 *          If the png library is not linked, this will fail.
 * </pre>
 */
PIXA *
pixaRead(const char  *filename)
{
FILE  *fp;
PIXA  *pixa;

#if !HAVE_LIBPNG     /* defined in environ.h and config_auto.h */
    return (PIXA *)ERROR_PTR("no libpng: can't read data", __func__, NULL);
#endif  /* !HAVE_LIBPNG */

    if (!filename)
        return (PIXA *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (PIXA *)ERROR_PTR_1("stream not opened",
                                   filename, __func__, NULL);
    pixa = pixaReadStream(fp);
    fclose(fp);
    if (!pixa)
        return (PIXA *)ERROR_PTR_1("pixa not read",
                                   filename, __func__, NULL);
    return pixa;
}


/*!
 * \brief   pixaReadStream()
 *
 * \param[in]    fp    file stream
 * \return  pixa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The pix are stored in the file as png.
 *          If the png library is not linked, this will fail.
 *      (2) It is OK for the pixa to be empty.
 * </pre>
 */
PIXA *
pixaReadStream(FILE  *fp)
{
l_int32  n, i, xres, yres, version;
l_int32  ignore;
BOXA    *boxa;
PIX     *pix;
PIXA    *pixa;

#if !HAVE_LIBPNG     /* defined in environ.h and config_auto.h */
    return (PIXA *)ERROR_PTR("no libpng: can't read data", __func__, NULL);
#endif  /* !HAVE_LIBPNG */

    if (!fp)
        return (PIXA *)ERROR_PTR("stream not defined", __func__, NULL);

    if (fscanf(fp, "\nPixa Version %d\n", &version) != 1)
        return (PIXA *)ERROR_PTR("not a pixa file", __func__, NULL);
    if (version != PIXA_VERSION_NUMBER)
        return (PIXA *)ERROR_PTR("invalid pixa version", __func__, NULL);
    if (fscanf(fp, "Number of pix = %d\n", &n) != 1)
        return (PIXA *)ERROR_PTR("not a pixa file", __func__, NULL);
    if (n < 0)
        return (PIXA *)ERROR_PTR("num pix ptrs < 0", __func__, NULL);
    if (n > MaxPixaPtrArraySize)
        return (PIXA *)ERROR_PTR("too many pix ptrs", __func__, NULL);
    if (n == 0) L_INFO("the pixa is empty\n", __func__);

    if ((boxa = boxaReadStream(fp)) == NULL)
        return (PIXA *)ERROR_PTR("boxa not made", __func__, NULL);
    if ((pixa = pixaCreate(n)) == NULL) {
        boxaDestroy(&boxa);
        return (PIXA *)ERROR_PTR("pixa not made", __func__, NULL);
    }
    boxaDestroy(&pixa->boxa);
    pixa->boxa = boxa;

    for (i = 0; i < n; i++) {
        if ((fscanf(fp, " pix[%d]: xres = %d, yres = %d\n",
              &ignore, &xres, &yres)) != 3) {
            pixaDestroy(&pixa);
            return (PIXA *)ERROR_PTR("res reading error", __func__, NULL);
        }
        if ((pix = pixReadStreamPng(fp)) == NULL) {
            pixaDestroy(&pixa);
            return (PIXA *)ERROR_PTR("pix not read", __func__, NULL);
        }
        pixSetXRes(pix, xres);
        pixSetYRes(pix, yres);
        pixaAddPix(pixa, pix, L_INSERT);
    }
    return pixa;
}


/*!
 * \brief   pixaReadMem()
 *
 * \param[in]    data   of serialized pixa
 * \param[in]    size   of data in bytes
 * \return  pixa, or NULL on error
 */
PIXA *
pixaReadMem(const l_uint8  *data,
            size_t          size)
{
FILE  *fp;
PIXA  *pixa;

    if (!data)
        return (PIXA *)ERROR_PTR("data not defined", __func__, NULL);
    if ((fp = fopenReadFromMemory(data, size)) == NULL)
        return (PIXA *)ERROR_PTR("stream not opened", __func__, NULL);

    pixa = pixaReadStream(fp);
    fclose(fp);
    if (!pixa) L_ERROR("pixa not read\n", __func__);
    return pixa;
}


/*!
 * \brief   pixaWriteDebug()
 *
 * \param[in]    fname
 * \param[in]    pixa
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Debug version, intended for use in the library when writing
 *          to files in a temp directory with names that are compiled in.
 *          This is used instead of pixaWrite() for all such library calls.
 *      (2) The global variable LeptDebugOK defaults to 0, and can be set
 *          or cleared by the function setLeptDebugOK().
 * </pre>
 */
l_ok
pixaWriteDebug(const char  *fname,
               PIXA        *pixa)
{
    if (LeptDebugOK) {
        return pixaWrite(fname, pixa);
    } else {
        L_INFO("write to named temp file %s is disabled\n", __func__, fname);
        return 0;
    }
}


/*!
 * \brief   pixaWrite()
 *
 * \param[in]    filename
 * \param[in]    pixa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The pix are stored in the file as png.
 *          If the png library is not linked, this will fail.
 * </pre>
 */
l_ok
pixaWrite(const char  *filename,
          PIXA        *pixa)
{
l_int32  ret;
FILE    *fp;

#if !HAVE_LIBPNG     /* defined in environ.h and config_auto.h */
    return ERROR_INT("no libpng: can't write data", __func__, 1);
#endif  /* !HAVE_LIBPNG */

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    if ((fp = fopenWriteStream(filename, "wb")) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    ret = pixaWriteStream(fp, pixa);
    fclose(fp);
    if (ret)
        return ERROR_INT_1("pixa not written to stream", filename, __func__, 1);
    return 0;
}


/*!
 * \brief   pixaWriteStream()
 *
 * \param[in]    fp     file stream opened for "wb"
 * \param[in]    pixa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The pix are stored in the file as png.
 *          If the png library is not linked, this will fail.
 * </pre>
 */
l_ok
pixaWriteStream(FILE  *fp,
                PIXA  *pixa)
{
l_int32  n, i;
PIX     *pix;

#if !HAVE_LIBPNG     /* defined in environ.h and config_auto.h */
    return ERROR_INT("no libpng: can't write data", __func__, 1);
#endif  /* !HAVE_LIBPNG */

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

    n = pixaGetCount(pixa);
    fprintf(fp, "\nPixa Version %d\n", PIXA_VERSION_NUMBER);
    fprintf(fp, "Number of pix = %d\n", n);
    boxaWriteStream(fp, pixa->boxa);
    for (i = 0; i < n; i++) {
        if ((pix = pixaGetPix(pixa, i, L_CLONE)) == NULL)
            return ERROR_INT("pix not found", __func__, 1);
        fprintf(fp, " pix[%d]: xres = %d, yres = %d\n",
                i, pix->xres, pix->yres);
        pixWriteStreamPng(fp, pix, 0.0);
        pixDestroy(&pix);
    }
    return 0;
}


/*!
 * \brief   pixaWriteMem()
 *
 * \param[out]   pdata    data of serialized pixa
 * \param[out]   psize    size of returned data
 * \param[in]    pixa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Serializes a pixa in memory and puts the result in a buffer.
 * </pre>
 */
l_ok
pixaWriteMem(l_uint8  **pdata,
             size_t    *psize,
             PIXA      *pixa)
{
l_int32  ret;
FILE    *fp;

    if (pdata) *pdata = NULL;
    if (psize) *psize = 0;
    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1);
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);

#if HAVE_FMEMOPEN
    if ((fp = open_memstream((char **)pdata, psize)) == NULL)
        return ERROR_INT("stream not opened", __func__, 1);
    ret = pixaWriteStream(fp, pixa);
    fputc('\0', fp);
    fclose(fp);
    if (*psize > 0) *psize = *psize - 1;
#else
    L_INFO("no fmemopen API --> work-around: write to temp file\n", __func__);
  #ifdef _WIN32
    if ((fp = fopenWriteWinTempfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #else
    if ((fp = tmpfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #endif  /* _WIN32 */
    ret = pixaWriteStream(fp, pixa);
    rewind(fp);
    *pdata = l_binaryReadStream(fp, psize);
    fclose(fp);
#endif  /* HAVE_FMEMOPEN */
    return ret;
}


/*!
 * \brief   pixaReadBoth()
 *
 * \param[in]    filename
 * \return  pixa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This reads serialized files of either a pixa or a pixacomp,
 *          and returns a pixa in memory.  It requires png and jpeg libraries.
 * </pre>
 */
PIXA *
pixaReadBoth(const char  *filename)
{
char    buf[32];
char   *sname;
PIXA   *pixa;
PIXAC  *pac;

    if (!filename)
        return (PIXA *)ERROR_PTR("filename not defined", __func__, NULL);

    l_getStructStrFromFile(filename, L_STR_NAME, &sname);
    if (!sname)
        return (PIXA *)ERROR_PTR("struct name not found", __func__, NULL);
    snprintf(buf, sizeof(buf), "%s", sname);
    LEPT_FREE(sname);

    if (strcmp(buf, "Pixacomp") == 0) {
        if ((pac = pixacompRead(filename)) == NULL)
            return (PIXA *)ERROR_PTR("pac not made", __func__, NULL);
        pixa = pixaCreateFromPixacomp(pac, L_COPY);
        pixacompDestroy(&pac);
    } else if (strcmp(buf, "Pixa") == 0) {
        if ((pixa = pixaRead(filename)) == NULL)
            return (PIXA *)ERROR_PTR("pixa not made", __func__, NULL);
    } else {
        return (PIXA *)ERROR_PTR("invalid file type", __func__, NULL);
    }
    return pixa;
}


/*---------------------------------------------------------------------*
 *                         Pixaa serialized I/O                        *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaaReadFromFiles()
 *
 * \param[in]    dirname   directory
 * \param[in]    substr    [optional] substring filter on filenames; can be NULL
 * \param[in]    first     0-based
 * \param[in]    nfiles    use 0 for everything from %first to the end
 * \return  paa, or NULL on error or if no pixa files are found.
 *
 * <pre>
 * Notes:
 *      (1) The files must be serialized pixa files (e.g., *.pa)
 *          If some files cannot be read, warnings are issued.
 *      (2) Use %substr to filter filenames in the directory.  If
 *          %substr == NULL, this takes all files.
 *      (3) After filtering, use %first and %nfiles to select
 *          a contiguous set of files, that have been lexically
 *          sorted in increasing order.
 * </pre>
 */
PIXAA *
pixaaReadFromFiles(const char  *dirname,
                   const char  *substr,
                   l_int32      first,
                   l_int32      nfiles)
{
char    *fname;
l_int32  i, n;
PIXA    *pixa;
PIXAA   *paa;
SARRAY  *sa;

  if (!dirname)
      return (PIXAA *)ERROR_PTR("dirname not defined", __func__, NULL);

  sa = getSortedPathnamesInDirectory(dirname, substr, first, nfiles);
  if (!sa || ((n = sarrayGetCount(sa)) == 0)) {
      sarrayDestroy(&sa);
      return (PIXAA *)ERROR_PTR("no pixa files found", __func__, NULL);
  }

  paa = pixaaCreate(n);
  for (i = 0; i < n; i++) {
      fname = sarrayGetString(sa, i, L_NOCOPY);
      if ((pixa = pixaRead(fname)) == NULL) {
          L_ERROR("pixa not read for %d-th file", __func__, i);
          continue;
      }
      pixaaAddPixa(paa, pixa, L_INSERT);
  }

  sarrayDestroy(&sa);
  return paa;
}


/*!
 * \brief   pixaaRead()
 *
 * \param[in]    filename
 * \return  paa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The pix are stored in the file as png.
 *          If the png library is not linked, this will fail.
 * </pre>
 */
PIXAA *
pixaaRead(const char  *filename)
{
FILE   *fp;
PIXAA  *paa;

#if !HAVE_LIBPNG     /* defined in environ.h and config_auto.h */
    return (PIXAA *)ERROR_PTR("no libpng: can't read data", __func__, NULL);
#endif  /* !HAVE_LIBPNG */

    if (!filename)
        return (PIXAA *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (PIXAA *)ERROR_PTR_1("stream not opened",
                                    filename, __func__, NULL);
    paa = pixaaReadStream(fp);
    fclose(fp);
    if (!paa)
        return (PIXAA *)ERROR_PTR_1("paa not read", filename, __func__, NULL);
    return paa;
}


/*!
 * \brief   pixaaReadStream()
 *
 * \param[in]    fp    file stream
 * \return  paa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The pix are stored in the file as png.
 *          If the png library is not linked, this will fail.
 *      (2) It is OK for the pixaa to be empty.
 * </pre>
 */
PIXAA *
pixaaReadStream(FILE  *fp)
{
l_int32  n, i, version;
l_int32  ignore;
BOXA    *boxa;
PIXA    *pixa;
PIXAA   *paa;

#if !HAVE_LIBPNG     /* defined in environ.h and config_auto.h */
    return (PIXAA *)ERROR_PTR("no libpng: can't read data", __func__, NULL);
#endif  /* !HAVE_LIBPNG */

    if (!fp)
        return (PIXAA *)ERROR_PTR("stream not defined", __func__, NULL);

    if (fscanf(fp, "\nPixaa Version %d\n", &version) != 1)
        return (PIXAA *)ERROR_PTR("not a pixaa file", __func__, NULL);
    if (version != PIXAA_VERSION_NUMBER)
        return (PIXAA *)ERROR_PTR("invalid pixaa version", __func__, NULL);
    if (fscanf(fp, "Number of pixa = %d\n", &n) != 1)
        return (PIXAA *)ERROR_PTR("not a pixaa file", __func__, NULL);
    if (n < 0)
        return (PIXAA *)ERROR_PTR("num pixa ptrs < 0", __func__, NULL);
    if (n > MaxPixaaPtrArraySize)
        return (PIXAA *)ERROR_PTR("too many pixa ptrs", __func__, NULL);
    if (n == 0) L_INFO("the pixaa is empty\n", __func__);

    if ((paa = pixaaCreate(n)) == NULL)
        return (PIXAA *)ERROR_PTR("paa not made", __func__, NULL);
    if ((boxa = boxaReadStream(fp)) == NULL) {
        pixaaDestroy(&paa);
        return (PIXAA *)ERROR_PTR("boxa not made", __func__, NULL);
    }
    boxaDestroy(&paa->boxa);
    paa->boxa = boxa;

    for (i = 0; i < n; i++) {
        if ((fscanf(fp, "\n\n --------------- pixa[%d] ---------------\n",
                    &ignore)) != 1) {
            pixaaDestroy(&paa);
            return (PIXAA *)ERROR_PTR("text reading", __func__, NULL);
        }
        if ((pixa = pixaReadStream(fp)) == NULL) {
            pixaaDestroy(&paa);
            return (PIXAA *)ERROR_PTR("pixa not read", __func__, NULL);
        }
        pixaaAddPixa(paa, pixa, L_INSERT);
    }

    return paa;
}


/*!
 * \brief   pixaaReadMem()
 *
 * \param[in]    data   of serialized pixaa
 * \param[in]    size   of data in bytes
 * \return  paa, or NULL on error
 */
PIXAA *
pixaaReadMem(const l_uint8  *data,
             size_t          size)
{
FILE   *fp;
PIXAA  *paa;

    if (!data)
        return (PIXAA *)ERROR_PTR("data not defined", __func__, NULL);
    if ((fp = fopenReadFromMemory(data, size)) == NULL)
        return (PIXAA *)ERROR_PTR("stream not opened", __func__, NULL);

    paa = pixaaReadStream(fp);
    fclose(fp);
    if (!paa) L_ERROR("paa not read\n", __func__);
    return paa;
}


/*!
 * \brief   pixaaWrite()
 *
 * \param[in]    filename
 * \param[in]    paa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The pix are stored in the file as png.
 *          If the png library is not linked, this will fail.
 * </pre>
 */
l_ok
pixaaWrite(const char  *filename,
           PIXAA       *paa)
{
l_int32  ret;
FILE    *fp;

#if !HAVE_LIBPNG     /* defined in environ.h and config_auto.h */
    return ERROR_INT("no libpng: can't read data", __func__, 1);
#endif  /* !HAVE_LIBPNG */

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);

    if ((fp = fopenWriteStream(filename, "wb")) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    ret = pixaaWriteStream(fp, paa);
    fclose(fp);
    if (ret)
        return ERROR_INT_1("paa not written to stream", filename, __func__, 1);
    return 0;
}


/*!
 * \brief   pixaaWriteStream()
 *
 * \param[in]    fp    file stream opened for "wb"
 * \param[in]    paa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The pix are stored in the file as png.
 *          If the png library is not linked, this will fail.
 * </pre>
 */
l_ok
pixaaWriteStream(FILE   *fp,
                 PIXAA  *paa)
{
l_int32  n, i;
PIXA    *pixa;

#if !HAVE_LIBPNG     /* defined in environ.h and config_auto.h */
    return ERROR_INT("no libpng: can't read data", __func__, 1);
#endif  /* !HAVE_LIBPNG */

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);

    n = pixaaGetCount(paa, NULL);
    fprintf(fp, "\nPixaa Version %d\n", PIXAA_VERSION_NUMBER);
    fprintf(fp, "Number of pixa = %d\n", n);
    boxaWriteStream(fp, paa->boxa);
    for (i = 0; i < n; i++) {
        if ((pixa = pixaaGetPixa(paa, i, L_CLONE)) == NULL)
            return ERROR_INT("pixa not found", __func__, 1);
        fprintf(fp, "\n\n --------------- pixa[%d] ---------------\n", i);
        pixaWriteStream(fp, pixa);
        pixaDestroy(&pixa);
    }
    return 0;
}


/*!
 * \brief   pixaaWriteMem()
 *
 * \param[out]   pdata   data of serialized pixaa
 * \param[out]   psize   size of returned data
 * \param[in]    paa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Serializes a pixaa in memory and puts the result in a buffer.
 * </pre>
 */
l_ok
pixaaWriteMem(l_uint8  **pdata,
              size_t    *psize,
              PIXAA     *paa)
{
l_int32  ret;
FILE    *fp;

    if (pdata) *pdata = NULL;
    if (psize) *psize = 0;
    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1);
    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);

#if HAVE_FMEMOPEN
    if ((fp = open_memstream((char **)pdata, psize)) == NULL)
        return ERROR_INT("stream not opened", __func__, 1);
    ret = pixaaWriteStream(fp, paa);
    fputc('\0', fp);
    fclose(fp);
    if (*psize > 0) *psize = *psize - 1;
#else
    L_INFO("no fmemopen API --> work-around: write to temp file\n", __func__);
  #ifdef _WIN32
    if ((fp = fopenWriteWinTempfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #else
    if ((fp = tmpfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #endif  /* _WIN32 */
    ret = pixaaWriteStream(fp, paa);
    rewind(fp);
    *pdata = l_binaryReadStream(fp, psize);
    fclose(fp);
#endif  /* HAVE_FMEMOPEN */
    return ret;
}


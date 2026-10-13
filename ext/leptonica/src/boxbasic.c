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
 * \file  boxbasic.c
 * <pre>
 *
 *   Basic 'class' functions for box, boxa and boxaa,
 *   including accessors and serialization.
 *
 *      Box creation, copy, clone, destruction
 *           BOX      *boxCreate()
 *           BOX      *boxCreateValid()
 *           BOX      *boxCopy()
 *           BOX      *boxClone()
 *           void      boxDestroy()
 *
 *      Box accessors
 *           l_int32   boxGetGeometry()
 *           l_int32   boxSetGeometry()
 *           l_int32   boxGetSideLocations()
 *           l_int32   boxSetSideLocations()
 *           l_int32   boxIsValid()
 *
 *      Boxa creation, copy, destruction
 *           BOXA     *boxaCreate()
 *           BOXA     *boxaCopy()
 *           void      boxaDestroy()
 *
 *      Boxa array extension
 *           l_int32   boxaAddBox()
 *           l_int32   boxaExtendArray()
 *           l_int32   boxaExtendArrayToSize()
 *
 *      Boxa accessors
 *           l_int32   boxaGetCount()
 *           l_int32   boxaGetValidCount()
 *           BOX      *boxaGetBox()
 *           BOX      *boxaGetValidBox()
 *           NUMA     *boxaFindInvalidBoxes()
 *           l_int32   boxaGetBoxGeometry()
 *           l_int32   boxaIsFull()
 *
 *      Boxa array modifiers
 *           l_int32   boxaReplaceBox()
 *           l_int32   boxaInsertBox()
 *           l_int32   boxaRemoveBox()
 *           l_int32   boxaRemoveBoxAndSave()
 *           BOXA     *boxaSaveValid()
 *           l_int32   boxaInitFull()
 *           l_int32   boxaClear()
 *
 *      Boxaa creation, copy, destruction
 *           BOXAA    *boxaaCreate()
 *           BOXAA    *boxaaCopy()
 *           void      boxaaDestroy()
 *
 *      Boxaa array extension
 *           l_int32   boxaaAddBoxa()
 *           l_int32   boxaaExtendArray()
 *           l_int32   boxaaExtendArrayToSize()
 *
 *      Boxaa accessors
 *           l_int32   boxaaGetCount()
 *           l_int32   boxaaGetBoxCount()
 *           BOXA     *boxaaGetBoxa()
 *           BOX      *boxaaGetBox()
 *
 *      Boxaa array modifiers
 *           l_int32   boxaaInitFull()
 *           l_int32   boxaaExtendWithInit()
 *           l_int32   boxaaReplaceBoxa()
 *           l_int32   boxaaInsertBoxa()
 *           l_int32   boxaaRemoveBoxa()
 *           l_int32   boxaaAddBox()
 *
 *      Boxaa serialized I/O
 *           BOXAA    *boxaaReadFromFiles()
 *           BOXAA    *boxaaRead()
 *           BOXAA    *boxaaReadStream()
 *           BOXAA    *boxaaReadMem()
 *           l_int32   boxaaWrite()
 *           l_int32   boxaaWriteStream()
 *           l_int32   boxaaWriteMem()
 *
 *      Boxa serialized I/O
 *           BOXA     *boxaRead()
 *           BOXA     *boxaReadStream()
 *           BOXA     *boxaReadMem()
 *           l_int32   boxaWriteDebug()
 *           l_int32   boxaWrite()
 *           l_int32   boxaWriteStream()
 *           l_int32   boxaWriteStderr()
 *           l_int32   boxaWriteMem()
 *
 *      Box print (for debug)
 *           l_int32   boxPrintStreamInfo()
 *
 *   Most functions use only valid boxes, which are boxes that have both
 *   width and height > 0.  However, a few functions, such as
 *   boxaGetMedianVals() do not assume that all boxes are valid.  For any
 *   function that can use a boxa with invalid boxes, it is convenient
 *   to use these accessors:
 *       boxaGetValidCount()   :  count of valid boxes
 *       boxaGetValidBox()     :  returns NULL for invalid boxes
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"
#include "pix_internal.h"

    /* Bounds on array sizes */
static const size_t  MaxBoxaPtrArraySize = 10000000;
static const size_t  MaxBoxaaPtrArraySize = 1000000;
static const size_t  InitialPtrArraySize = 20;      /*!< n'importe quoi */

/*---------------------------------------------------------------------*
 *                  Box creation, destruction and copy                 *
 *---------------------------------------------------------------------*/
/*!
 * \brief   boxCreate()
 *
 * \param[in]    x, y, w, h
 * \return  box, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This clips the box to the +quad.  If no part of the
 *          box is in the +quad, this returns NULL.
 *      (2) We allow you to make a box with w = 0 and/or h = 0.
 *          This does not represent a valid region, but it is useful
 *          as a placeholder in a boxa for which the index of the
 *          box in the boxa is important.  This is an atypical
 *          situation; usually you want to put only valid boxes with
 *          nonzero width and height in a boxa.  If you have a boxa
 *          with invalid boxes, the accessor boxaGetValidBox()
 *          will return NULL on each invalid box.
 *      (3) If you want to create only valid boxes, use boxCreateValid(),
 *          which returns NULL if either w or h is 0.
 * </pre>
 */
BOX *
boxCreate(l_int32  x,
          l_int32  y,
          l_int32  w,
          l_int32  h)
{
BOX  *box;

    if (w < 0 || h < 0)
        return (BOX *)ERROR_PTR("w and h not both >= 0", __func__, NULL);
    if (x < 0) {  /* take part in +quad */
        w = w + x;
        x = 0;
        if (w <= 0)
            return (BOX *)ERROR_PTR("x < 0 and box off +quad", __func__, NULL);
    }
    if (y < 0) {  /* take part in +quad */
        h = h + y;
        y = 0;
        if (h <= 0)
            return (BOX *)ERROR_PTR("y < 0 and box off +quad", __func__, NULL);
    }

    box = (BOX *)LEPT_CALLOC(1, sizeof(BOX));
    boxSetGeometry(box, x, y, w, h);
    box->refcount = 1;
    return box;
}


/*!
 * \brief   boxCreateValid()
 *
 * \param[in]    x, y, w, h
 * \return  box, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This returns NULL if either w = 0 or h = 0.
 * </pre>
 */
BOX *
boxCreateValid(l_int32  x,
               l_int32  y,
               l_int32  w,
               l_int32  h)
{
    if (w <= 0 || h <= 0)
        return (BOX *)ERROR_PTR("w and h not both > 0", __func__, NULL);
    return boxCreate(x, y, w, h);
}


/*!
 * \brief   boxCopy()
 *
 * \param[in]    box
 * \return  copy of box, or NULL on error
 */
BOX *
boxCopy(BOX  *box)
{
BOX  *boxc;

    if (!box)
        return (BOX *)ERROR_PTR("box not defined", __func__, NULL);

    boxc = boxCreate(box->x, box->y, box->w, box->h);
    return boxc;
}


/*!
 * \brief   boxClone()
 *
 * \param[in]    box
 * \return  ptr to same box, or NULL on error
 */
BOX *
boxClone(BOX  *box)
{

    if (!box)
        return (BOX *)ERROR_PTR("box not defined", __func__, NULL);

    ++box->refcount;
    return box;
}


/*!
 * \brief   boxDestroy()
 *
 * \param[in,out]   pbox     will be set to null before returning
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) Decrements the ref count and, if 0, destroys the box.
 *      (2) Always nulls the input ptr.
 * </pre>
 */
void
boxDestroy(BOX  **pbox)
{
BOX  *box;

    if (pbox == NULL) {
        L_WARNING("ptr address is null!\n", __func__);
        return;
    }
    if ((box = *pbox) == NULL)
        return;

    if (--box->refcount == 0)
        LEPT_FREE(box);
    *pbox = NULL;
}


/*---------------------------------------------------------------------*
 *                              Box accessors                          *
 *---------------------------------------------------------------------*/
/*!
 * \brief   boxGetGeometry()
 *
 * \param[in]    box
 * \param[out]   px, py, pw, ph [optional]  each can be null
 * \return  0 if OK, 1 on error
 */
l_ok
boxGetGeometry(const BOX *box,
               l_int32   *px,
               l_int32   *py,
               l_int32   *pw,
               l_int32   *ph)
{
    if (px) *px = 0;
    if (py) *py = 0;
    if (pw) *pw = 0;
    if (ph) *ph = 0;
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);
    if (px) *px = box->x;
    if (py) *py = box->y;
    if (pw) *pw = box->w;
    if (ph) *ph = box->h;
    return 0;
}


/*!
 * \brief   boxSetGeometry()
 *
 * \param[in]    box
 * \param[in]    x, y, w, h     [optional] use -1 to leave unchanged
 * \return  0 if OK, 1 on error
 */
l_ok
boxSetGeometry(BOX     *box,
               l_int32  x,
               l_int32  y,
               l_int32  w,
               l_int32  h)
{
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);
    if (x != -1) box->x = x;
    if (y != -1) box->y = y;
    if (w != -1) box->w = w;
    if (h != -1) box->h = h;
    return 0;
}


/*!
 * \brief   boxGetSideLocations()
 *
 * \param[in]    box
 * \param[out]   pl, pr, pt, pb     [optional] each can be null
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) All returned values are within the box.
 * </pre>
 */
l_ok
boxGetSideLocations(const BOX *box,
                    l_int32   *pl,
                    l_int32   *pr,
                    l_int32   *pt,
                    l_int32   *pb)
{
l_int32  x, y, w, h;

    if (pl) *pl = 0;
    if (pr) *pr = 0;
    if (pt) *pt = 0;
    if (pb) *pb = 0;
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);

    boxGetGeometry(box, &x, &y, &w, &h);
    if (pl) *pl = x;
    if (pr) *pr = x + w - 1;
    if (pt) *pt = y;
    if (pb) *pb = y + h - 1;
    return 0;
}


/*!
 * \brief   boxSetSideLocations()
 *
 * \param[in]    box
 * \param[in]    l, r, t, b     [optional] use -1 to leave unchanged
 * \return  0 if OK, 1 on error
 */
l_ok
boxSetSideLocations(BOX     *box,
                    l_int32  l,
                    l_int32  r,
                    l_int32  t,
                    l_int32  b)
{
l_int32  x, y, w, h;

    if (!box)
        return ERROR_INT("box not defined", __func__, 1);
    x = (l != -1) ? l : box->x;
    w = (r != -1) ? r - x + 1 : box->x + box->w - x;
    y = (t != -1) ? t : box->y;
    h = (b != -1) ? b - y + 1 : box->y + box->h - y;
    boxSetGeometry(box, x, y, w, h);
    return 0;
}


/*!
 * \brief   boxIsValid()
 *
 * \param[in]    box
 * \param[out]   pvalid    1 if valid; 0 otherwise
 * \return  0 if OK, 1 on error
 */
l_ok
boxIsValid(BOX      *box,
           l_int32  *pvalid)
{
    if (!pvalid)
        return ERROR_INT("&valid not defined", __func__, 1);
    *pvalid = 0;
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);

    if (box->w > 0 && box->h > 0)
        *pvalid = 1;
    return 0;
}


/*---------------------------------------------------------------------*
 *             Boxa creation, destruction, copy, extension             *
 *---------------------------------------------------------------------*/
/*!
 * \brief   boxaCreate()
 *
 * \param[in]    n    initial number of ptrs; 0 for default
 * \return  boxa, or NULL on error
 */
BOXA *
boxaCreate(l_int32  n)
{
BOXA  *boxa;

    if (n <= 0 || n > MaxBoxaPtrArraySize)
        n = InitialPtrArraySize;

    boxa = (BOXA *)LEPT_CALLOC(1, sizeof(BOXA));
    boxa->n = 0;
    boxa->nalloc = n;
    boxa->refcount = 1;
    if ((boxa->box = (BOX **)LEPT_CALLOC(n, sizeof(BOX *))) == NULL) {
        boxaDestroy(&boxa);
        return (BOXA *)ERROR_PTR("boxa ptrs not made", __func__, NULL);
    }
    return boxa;
}


/*!
 * \brief   boxaCopy()
 *
 * \param[in]    boxa
 * \param[in]    copyflag    L_COPY, L_CLONE, L_COPY_CLONE
 * \return  new boxa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See pix.h for description of the copyflag.
 *      (2) The copy-clone makes a new boxa that holds clones of each box.
 * </pre>
 */
BOXA *
boxaCopy(BOXA    *boxa,
         l_int32  copyflag)
{
l_int32  i;
BOX     *boxc;
BOXA    *boxac;

    if (!boxa)
        return (BOXA *)ERROR_PTR("boxa not defined", __func__, NULL);

    if (copyflag == L_CLONE) {
        boxa->refcount++;
        return boxa;
    }

    if (copyflag != L_COPY && copyflag != L_COPY_CLONE)
        return (BOXA *)ERROR_PTR("invalid copyflag", __func__, NULL);

    if ((boxac = boxaCreate(boxa->nalloc)) == NULL)
        return (BOXA *)ERROR_PTR("boxac not made", __func__, NULL);
    for (i = 0; i < boxa->n; i++) {
        if (copyflag == L_COPY)
            boxc = boxaGetBox(boxa, i, L_COPY);
        else   /* copy-clone */
            boxc = boxaGetBox(boxa, i, L_CLONE);
        boxaAddBox(boxac, boxc, L_INSERT);
    }
    return boxac;
}


/*!
 * \brief   boxaDestroy()
 *
 * \param[in,out]   pboxa    will be set to null before returning
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) Decrements the ref count and, if 0, destroys the boxa.
 *      (2) Always nulls the input ptr.
 * </pre>
 */
void
boxaDestroy(BOXA  **pboxa)
{
l_int32  i;
BOXA    *boxa;

    if (pboxa == NULL) {
        L_WARNING("ptr address is null!\n", __func__);
        return;
    }

    if ((boxa = *pboxa) == NULL)
        return;

        /* Decrement the ref count.  If it is 0, destroy the boxa. */
    if (--boxa->refcount == 0) {
        for (i = 0; i < boxa->n; i++)
            boxDestroy(&boxa->box[i]);
        LEPT_FREE(boxa->box);
        LEPT_FREE(boxa);
    }

    *pboxa = NULL;
}


/*!
 * \brief   boxaAddBox()
 *
 * \param[in]    boxa
 * \param[in]    box         to be added
 * \param[in]    copyflag    L_INSERT, L_COPY, L_CLONE
 * \return  0 if OK, 1 on error
 */
l_ok
boxaAddBox(BOXA    *boxa,
           BOX     *box,
           l_int32  copyflag)
{
l_int32  n;
BOX     *boxc;

    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);

    if (copyflag == L_INSERT)
        boxc = box;
    else if (copyflag == L_COPY)
        boxc = boxCopy(box);
    else if (copyflag == L_CLONE)
        boxc = boxClone(box);
    else
        return ERROR_INT("invalid copyflag", __func__, 1);
    if (!boxc)
        return ERROR_INT("boxc not made", __func__, 1);

    n = boxaGetCount(boxa);
    if (n >= boxa->nalloc) {
        if (boxaExtendArray(boxa)) {
            if (copyflag != L_INSERT)
                boxDestroy(&boxc);
            return ERROR_INT("extension failed", __func__, 1);
        }
    }
    boxa->box[n] = boxc;
    boxa->n++;
    return 0;
}


/*!
 * \brief   boxaExtendArray()
 *
 * \param[in]    boxa
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Reallocs with doubled size of ptr array.
 * </pre>
 */
l_ok
boxaExtendArray(BOXA  *boxa)
{
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    return boxaExtendArrayToSize(boxa, 2 * boxa->nalloc);
}


/*!
 * \brief   boxaExtendArrayToSize()
 *
 * \param[in]    boxa
 * \param[in]    size     new size of boxa ptr array
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If necessary, reallocs new boxa ptr array to %size.
 *      (2) The max number of box ptrs is 10M.
 * </pre>
 */
l_ok
boxaExtendArrayToSize(BOXA   *boxa,
                      size_t  size)
{
size_t  oldsize, newsize;

    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if (boxa->nalloc > MaxBoxaPtrArraySize)  /* belt & suspenders */
        return ERROR_INT("boxa has too many ptrs", __func__, 1);
    if (size > MaxBoxaPtrArraySize)
        return ERROR_INT("size > 10M box ptrs; too large", __func__, 1);
    if (size <= boxa->nalloc) {
        L_INFO("size too small; no extension\n", __func__);
        return 0;
    }

    oldsize = boxa->nalloc * sizeof(BOX *);
    newsize = size * sizeof(BOX *);
    if ((boxa->box = (BOX **)reallocNew((void **)&boxa->box,
                                        oldsize, newsize)) == NULL)
        return ERROR_INT("new ptr array not returned", __func__, 1);
    boxa->nalloc = size;
    return 0;
}


/*---------------------------------------------------------------------*
 *                             Boxa accessors                          *
 *---------------------------------------------------------------------*/
/*!
 * \brief   boxaGetCount()
 *
 * \param[in]    boxa
 * \return  count of all boxes; 0 if no boxes or on error
 */
l_int32
boxaGetCount(const BOXA  *boxa)
{
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 0);
    return boxa->n;
}


/*!
 * \brief   boxaGetValidCount()
 *
 * \param[in]    boxa
 * \return  count of valid boxes; 0 if no valid boxes or on error
 */
l_int32
boxaGetValidCount(BOXA  *boxa)
{
l_int32  n, i, w, h, count;

    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 0);

    n = boxaGetCount(boxa);
    for (i = 0, count = 0; i < n; i++) {
        boxaGetBoxGeometry(boxa, i, NULL, NULL, &w, &h);
        if (w > 0 && h > 0)
            count++;
    }
    return count;
}


/*!
 * \brief   boxaGetBox()
 *
 * \param[in]    boxa
 * \param[in]    index        to the index-th box
 * \param[in]    accessflag   L_COPY or L_CLONE
 * \return  box, or NULL on error
 */
BOX *
boxaGetBox(BOXA    *boxa,
           l_int32  index,
           l_int32  accessflag)
{
    if (!boxa)
        return (BOX *)ERROR_PTR("boxa not defined", __func__, NULL);
    if (index < 0 || index >= boxa->n)
        return (BOX *)ERROR_PTR("index not valid", __func__, NULL);

    if (accessflag == L_COPY)
        return boxCopy(boxa->box[index]);
    else if (accessflag == L_CLONE)
        return boxClone(boxa->box[index]);
    else
        return (BOX *)ERROR_PTR("invalid accessflag", __func__, NULL);
}


/*!
 * \brief   boxaGetValidBox()
 *
 * \param[in]    boxa
 * \param[in]    index        to the index-th box
 * \param[in]    accessflag   L_COPY or L_CLONE
 * \return  box, or NULL if box is not valid or on error
 *
 * <pre>
 * Notes:
 *      (1) This returns NULL for an invalid box in a boxa.
 *          For a box to be valid, both the width and height must be > 0.
 *      (2) We allow invalid boxes, with w = 0 or h = 0, as placeholders
 *          in boxa for which the index of the box in the boxa is important.
 *          This is an atypical situation; usually you want to put only
 *          valid boxes in a boxa.
 * </pre>
 */
BOX *
boxaGetValidBox(BOXA    *boxa,
                l_int32  index,
                l_int32  accessflag)
{
l_int32  w, h;
BOX     *box;

    if (!boxa)
        return (BOX *)ERROR_PTR("boxa not defined", __func__, NULL);

    if ((box = boxaGetBox(boxa, index, accessflag)) == NULL)
        return (BOX *)ERROR_PTR("box not returned", __func__, NULL);
    boxGetGeometry(box, NULL, NULL, &w, &h);
    if (w <= 0 || h <= 0)  /* not valid, but not necessarily an error */
        boxDestroy(&box);
    return box;
}


/*!
 * \brief   boxaFindInvalidBoxes()
 *
 * \param[in]    boxa
 * \return  na   numa of invalid boxes; NULL if there are none or on error
 */
NUMA *
boxaFindInvalidBoxes(BOXA  *boxa)
{
l_int32  i, n, w, h;
NUMA    *na;

    if (!boxa)
        return (NUMA *)ERROR_PTR("boxa not defined", __func__, NULL);

    n = boxaGetCount(boxa);
    if (boxaGetValidCount(boxa) == n)
        return NULL;

    na = numaMakeConstant(0, n);
    for (i = 0; i < n; i++) {
        boxaGetBoxGeometry(boxa, i, NULL, NULL, &w, &h);
        if (w == 0 || h == 0)
            numaSetValue(na, i, 1);
    }
    return na;
}


/*!
 * \brief   boxaGetBoxGeometry()
 *
 * \param[in]    boxa
 * \param[in]    index            to the index-th box
 * \param[out]   px, py, pw, ph   [optional] each can be null
 * \return  0 if OK, 1 on error
 */
l_ok
boxaGetBoxGeometry(BOXA     *boxa,
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
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if (index < 0 || index >= boxa->n)
        return ERROR_INT("index not valid", __func__, 1);

    if ((box = boxaGetBox(boxa, index, L_CLONE)) == NULL)
        return ERROR_INT("box not found!", __func__, 1);
    boxGetGeometry(box, px, py, pw, ph);
    boxDestroy(&box);
    return 0;
}


/*!
 * \brief   boxaIsFull()
 *
 * \param[in]    boxa
 * \param[out]   pfull    1 if boxa is full; 0 otherwise
 * \return  0 if OK, 1 on error
 */
l_ok
boxaIsFull(BOXA     *boxa,
           l_int32  *pfull)
{
l_int32  i, n, full;
BOX     *box;

    if (!pfull)
        return ERROR_INT("&full not defined", __func__, 1);
    *pfull = 0;
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    n = boxaGetCount(boxa);
    full = 1;
    for (i = 0; i < n; i++) {
        if ((box = boxaGetBox(boxa, i, L_CLONE)) == NULL) {
            full = 0;
            break;
        }
        boxDestroy(&box);
    }
    *pfull = full;
    return 0;
}


/*---------------------------------------------------------------------*
 *                        Boxa array modifiers                         *
 *---------------------------------------------------------------------*/
/*!
 * \brief   boxaReplaceBox()
 *
 * \param[in]    boxa
 * \param[in]    index     to the index-th box
 * \param[in]    box       insert this box to replace existing one
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) In-place replacement of one box; the input %box is now
 *          owned by the boxa.
 *      (2) The previous box at that location, if any, is destroyed.
 * </pre>
 */
l_ok
boxaReplaceBox(BOXA    *boxa,
               l_int32  index,
               BOX     *box)
{
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if (index < 0 || index >= boxa->n)
        return ERROR_INT("index not valid", __func__, 1);
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);

    boxDestroy(&(boxa->box[index]));
    boxa->box[index] = box;
    return 0;
}


/*!
 * \brief   boxaInsertBox()
 *
 * \param[in]    boxa
 * \param[in]    index    location in boxa to insert new value
 * \param[in]    box      new box to be inserted; the boxa now owns it
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This shifts box[i] --> box[i + 1] for all i >= index,
 *          and then inserts box as box[index].
 *      (2) To insert at the beginning of the array, set index = 0.
 *      (3) To append to the array, it's easier to use boxaAddBox().
 *      (4) This should not be used repeatedly to insert into large arrays,
 *          because the function is O(n).
 * </pre>
 */
l_ok
boxaInsertBox(BOXA    *boxa,
              l_int32  index,
              BOX     *box)
{
l_int32  i, n;
BOX    **array;

    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    n = boxaGetCount(boxa);
    if (index < 0 || index > n) {
        L_ERROR("index %d not in [0,...,%d]\n", __func__, index, n);
        return 1;
    }
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);

    if (n >= boxa->nalloc) {
        if (boxaExtendArray(boxa))
            return ERROR_INT("extension failed", __func__, 1);
    }
    array = boxa->box;
    boxa->n++;
    for (i = n; i > index; i--)
        array[i] = array[i - 1];
    array[index] = box;
    return 0;
}


/*!
 * \brief   boxaRemoveBox()
 *
 * \param[in]    boxa
 * \param[in]    index    of box to be removed and destroyed
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This removes box[index] and then shifts
 *          box[i] --> box[i - 1] for all i > index.
 *      (2) It should not be used repeatedly to remove boxes from
 *          large arrays, because the function is O(n).
 * </pre>
 */
l_ok
boxaRemoveBox(BOXA    *boxa,
              l_int32  index)
{
    return boxaRemoveBoxAndSave(boxa, index, NULL);
}


/*!
 * \brief   boxaRemoveBoxAndSave()
 *
 * \param[in]    boxa
 * \param[in]    index     of box to be removed
 * \param[out]   pbox      [optional] removed box
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This removes box[index] and then shifts
 *          box[i] --> box[i - 1] for all i > index.
 *      (2) It should not be used repeatedly to remove boxes from
 *          large arrays, because the function is O(n).
 * </pre>
 */
l_ok
boxaRemoveBoxAndSave(BOXA    *boxa,
                     l_int32  index,
                     BOX    **pbox)
{
l_int32  i, n;
BOX    **array;

    if (pbox) *pbox = NULL;
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    n = boxaGetCount(boxa);
    if (index < 0 || index >= n) {
        L_ERROR("index %d not in [0,...,%d]\n", __func__, index, n - 1);
        return 1;
    }

    if (pbox)
        *pbox = boxaGetBox(boxa, index, L_CLONE);
    array = boxa->box;
    boxDestroy(&array[index]);
    for (i = index + 1; i < n; i++)
        array[i - 1] = array[i];
    array[n - 1] = NULL;
    boxa->n--;

    return 0;
}


/*!
 * \brief   boxaSaveValid()
 *
 * \param[in]    boxas
 * \param[in]    copyflag    L_COPY or L_CLONE
 * \return  boxad if OK, NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This makes a copy/clone of each valid box.
 * </pre>
 */
BOXA *
boxaSaveValid(BOXA    *boxas,
              l_int32  copyflag)
{
l_int32  i, n;
BOX     *box;
BOXA    *boxad;

    if (!boxas)
        return (BOXA *)ERROR_PTR("boxas not defined", __func__, NULL);
    if (copyflag != L_COPY && copyflag != L_CLONE)
        return (BOXA *)ERROR_PTR("invalid copyflag", __func__, NULL);

    n = boxaGetCount(boxas);
    boxad = boxaCreate(n);
    for (i = 0; i < n; i++) {
        if ((box = boxaGetValidBox(boxas, i, copyflag)) != NULL)
            boxaAddBox(boxad, box, L_INSERT);
    }

    return boxad;
}


/*!
 * \brief   boxaInitFull()
 *
 * \param[in]    boxa    typically empty
 * \param[in]    box     [optional] to be replicated into the entire ptr array
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This initializes a boxa by filling up the entire box ptr array
 *          with copies of %box.  If %box == NULL, use a placeholder box
 *          of zero size.  Any existing boxes are destroyed.
 *          After this opepration, the number of boxes is equal to
 *          the number of allocated ptrs.
 *      (2) Note that we use boxaReplaceBox() instead of boxaInsertBox().
 *          They both have the same effect when inserting into a NULL ptr
 *          in the boxa ptr array:
 *      (3) Example usage.  This function is useful to prepare for a
 *          random insertion (or replacement) of boxes into a boxa.
 *          To randomly insert boxes into a boxa, up to some index "max":
 *             Boxa *boxa = boxaCreate(max);
 *             boxaInitFull(boxa, NULL);
 *          If you want placeholder boxes of non-zero size:
 *             Boxa *boxa = boxaCreate(max);
 *             Box *box = boxCreate(...);
 *             boxaInitFull(boxa, box);
 *             boxDestroy(&box);
 *          If we have an existing boxa with a smaller ptr array, it can
 *          be reused for up to max boxes:
 *             boxaExtendArrayToSize(boxa, max);
 *             boxaInitFull(boxa, NULL);
 *          The initialization allows the boxa to always be properly
 *          filled, even if all the boxes are not later replaced.
 *          If you want to know which boxes have been replaced,
 *          and you initialized with invalid zero-sized boxes,
 *          use boxaGetValidBox() to return NULL for the invalid boxes.
 * </pre>
 */
l_ok
boxaInitFull(BOXA  *boxa,
             BOX   *box)
{
l_int32  i, n;
BOX     *boxt;

    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    n = boxa->nalloc;
    boxa->n = n;
    for (i = 0; i < n; i++) {
        if (box)
            boxt = boxCopy(box);
        else
            boxt = boxCreate(0, 0, 0, 0);
        boxaReplaceBox(boxa, i, boxt);
    }
    return 0;
}


/*!
 * \brief   boxaClear()
 *
 * \param[in]    boxa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This destroys all boxes in the boxa, setting the ptrs
 *          to null.  The number of allocated boxes, n, is set to 0.
 * </pre>
 */
l_ok
boxaClear(BOXA  *boxa)
{
l_int32  i, n;

    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    n = boxaGetCount(boxa);
    for (i = 0; i < n; i++)
        boxDestroy(&boxa->box[i]);
    boxa->n = 0;
    return 0;
}


/*--------------------------------------------------------------------------*
 *                     Boxaa creation, destruction                          *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   boxaaCreate()
 *
 * \param[in]    n     size of boxa ptr array to be alloc'd; 0 for default
 * \return  baa, or NULL on error
 */
BOXAA *
boxaaCreate(l_int32  n)
{
BOXAA  *baa;

    if (n <= 0 || n > MaxBoxaaPtrArraySize)
        n = InitialPtrArraySize;

    baa = (BOXAA *)LEPT_CALLOC(1, sizeof(BOXAA));
    if ((baa->boxa = (BOXA **)LEPT_CALLOC(n, sizeof(BOXA *))) == NULL) {
        boxaaDestroy(&baa);
        return (BOXAA *)ERROR_PTR("boxa ptr array not made", __func__, NULL);
    }
    baa->nalloc = n;
    baa->n = 0;
    return baa;
}


/*!
 * \brief   boxaaCopy()
 *
 * \param[in]    baas       input boxaa to be copied
 * \param[in]    copyflag   L_COPY, L_CLONE
 * \return  baad new boxaa, composed of copies or clones of the boxa
 *                    in baas, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) L_COPY makes a copy of each boxa in baas.
 *          L_CLONE makes a clone of each boxa in baas.
 * </pre>
 */
BOXAA *
boxaaCopy(BOXAA   *baas,
          l_int32  copyflag)
{
l_int32  i, n;
BOXA    *boxa;
BOXAA   *baad;

    if (!baas)
        return (BOXAA *)ERROR_PTR("baas not defined", __func__, NULL);
    if (copyflag != L_COPY && copyflag != L_CLONE)
        return (BOXAA *)ERROR_PTR("invalid copyflag", __func__, NULL);

    n = boxaaGetCount(baas);
    baad = boxaaCreate(n);
    for (i = 0; i < n; i++) {
        boxa = boxaaGetBoxa(baas, i, copyflag);
        boxaaAddBoxa(baad, boxa, L_INSERT);
    }

    return baad;
}


/*!
 * \brief   boxaaDestroy()
 *
 * \param[in,out]   pbaa     will be set to null before returning
 */
void
boxaaDestroy(BOXAA  **pbaa)
{
l_int32  i;
BOXAA   *baa;

    if (pbaa == NULL) {
        L_WARNING("ptr address is NULL!\n", __func__);
        return;
    }

    if ((baa = *pbaa) == NULL)
        return;

    for (i = 0; i < baa->n; i++)
        boxaDestroy(&baa->boxa[i]);
    LEPT_FREE(baa->boxa);
    LEPT_FREE(baa);
    *pbaa = NULL;
}



/*--------------------------------------------------------------------------*
 *                              Add Boxa to Boxaa                           *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   boxaaAddBoxa()
 *
 * \param[in]    baa
 * \param[in]    ba         to be added
 * \param[in]    copyflag   L_INSERT, L_COPY, L_CLONE
 * \return  0 if OK, 1 on error
 */
l_ok
boxaaAddBoxa(BOXAA   *baa,
             BOXA    *ba,
             l_int32  copyflag)
{
l_int32  n;
BOXA    *bac;

    if (!baa)
        return ERROR_INT("baa not defined", __func__, 1);
    if (!ba)
        return ERROR_INT("ba not defined", __func__, 1);
    if (copyflag != L_INSERT && copyflag != L_COPY && copyflag != L_CLONE)
        return ERROR_INT("invalid copyflag", __func__, 1);

    if (copyflag == L_INSERT)
        bac = ba;
    else
        bac = boxaCopy(ba, copyflag);

    n = boxaaGetCount(baa);
    if (n >= baa->nalloc) {
        if (boxaaExtendArray(baa))
            return ERROR_INT("extension failed", __func__, 1);
    }
    baa->boxa[n] = bac;
    baa->n++;
    return 0;
}


/*!
 * \brief   boxaaExtendArray()
 *
 * \param[in]    baa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Doubles the size of the boxa ptr array.
 *      (2) The max number of boxa ptrs is 1 million.
 * </pre>
 */
l_ok
boxaaExtendArray(BOXAA  *baa)
{
    if (!baa)
        return ERROR_INT("baa not defined", __func__, 1);

    return boxaaExtendArrayToSize(baa, 2 * baa->nalloc);
}


/*!
 * \brief   boxaaExtendArrayToSize()
 *
 * \param[in]    baa
 * \param[in]    size     new size of boxa array
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If necessary, reallocs the boxa ptr array to %size.
 *      (2) %size limited to 1M boxa ptrs.
 * </pre>
 */
l_ok
boxaaExtendArrayToSize(BOXAA   *baa,
                       l_int32  size)
{
size_t  oldsize, newsize;

    if (!baa)
        return ERROR_INT("baa not defined", __func__, 1);
    if (baa->nalloc > MaxBoxaaPtrArraySize)  /* belt & suspenders */
        return ERROR_INT("baa has too many ptrs", __func__, 1);
    if (size > MaxBoxaaPtrArraySize)
        return ERROR_INT("size > 1M boxa ptrs; too large", __func__, 1);
    if (size <= baa->nalloc) {
        L_INFO("size too small; no extension\n", __func__);
        return 0;
    }

    oldsize = baa->nalloc * sizeof(BOXA *);
    newsize = size * sizeof(BOXA *);
    if ((baa->boxa = (BOXA **)reallocNew((void **)&baa->boxa,
                                         oldsize, newsize)) == NULL)
        return ERROR_INT("new ptr array not returned", __func__, 1);
    baa->nalloc = size;
    return 0;
}


/*----------------------------------------------------------------------*
 *                           Boxaa accessors                            *
 *----------------------------------------------------------------------*/
/*!
 * \brief   boxaaGetCount()
 *
 * \param[in]    baa
 * \return  count number of boxa, or 0 if no boxa or on error
 */
l_int32
boxaaGetCount(BOXAA  *baa)
{
    if (!baa)
        return ERROR_INT("baa not defined", __func__, 0);
    return baa->n;
}


/*!
 * \brief   boxaaGetBoxCount()
 *
 * \param[in]    baa
 * \return  count number of boxes, or 0 if no boxes or on error
 */
l_int32
boxaaGetBoxCount(BOXAA  *baa)
{
BOXA    *boxa;
l_int32  n, sum, i;

    if (!baa)
        return ERROR_INT("baa not defined", __func__, 0);

    n = boxaaGetCount(baa);
    for (sum = 0, i = 0; i < n; i++) {
        boxa = boxaaGetBoxa(baa, i, L_CLONE);
        sum += boxaGetCount(boxa);
        boxaDestroy(&boxa);
    }

    return sum;
}


/*!
 * \brief   boxaaGetBoxa()
 *
 * \param[in]    baa
 * \param[in]    index        to the index-th boxa
 * \param[in]    accessflag   L_COPY or L_CLONE
 * \return  boxa, or NULL on error
 */
BOXA *
boxaaGetBoxa(BOXAA   *baa,
             l_int32  index,
             l_int32  accessflag)
{
l_int32  n;

    if (!baa)
        return (BOXA *)ERROR_PTR("baa not defined", __func__, NULL);
    n = boxaaGetCount(baa);
    if (index < 0 || index >= n)
        return (BOXA *)ERROR_PTR("index not valid", __func__, NULL);
    if (accessflag != L_COPY && accessflag != L_CLONE)
        return (BOXA *)ERROR_PTR("invalid accessflag", __func__, NULL);

    return boxaCopy(baa->boxa[index], accessflag);
}


/*!
 * \brief   boxaaGetBox()
 *
 * \param[in]    baa
 * \param[in]    iboxa        index into the boxa array in the boxaa
 * \param[in]    ibox         index into the box array in the boxa
 * \param[in]    accessflag   L_COPY or L_CLONE
 * \return  box, or NULL on error
 */
BOX *
boxaaGetBox(BOXAA   *baa,
            l_int32  iboxa,
            l_int32  ibox,
            l_int32  accessflag)
{
BOX   *box;
BOXA  *boxa;

    if ((boxa = boxaaGetBoxa(baa, iboxa, L_CLONE)) == NULL)
        return (BOX *)ERROR_PTR("boxa not retrieved", __func__, NULL);
    if ((box = boxaGetBox(boxa, ibox, accessflag)) == NULL)
        L_ERROR("box not retrieved\n", __func__);
    boxaDestroy(&boxa);
    return box;
}


/*----------------------------------------------------------------------*
 *                           Boxaa array modifiers                      *
 *----------------------------------------------------------------------*/
/*!
 * \brief   boxaaInitFull()
 *
 * \param[in]    baa      typically empty
 * \param[in]    boxa     to be replicated into the entire ptr array
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This initializes a boxaa by filling up the entire boxa ptr array
 *          with copies of %boxa.  Any existing boxa are destroyed.
 *          After this operation, the number of boxa is equal to
 *          the number of allocated ptrs.
 *      (2) Note that we use boxaaReplaceBoxa() which replaces a boxa,
 *          instead of boxaaInsertBoxa(), which is O(n) and shifts all
 *          the boxa pointers from the insertion point to the end.
 *      (3) Example usage.  This function is useful to prepare for a
 *          random insertion (or replacement) of boxa into a boxaa.
 *          To randomly insert boxa into a boxaa, up to some index "max":
 *             Boxaa *baa = boxaaCreate(max);
 *               // initialize the boxa
 *             Boxa *boxa = boxaCreate(...);
 *             ...  [optionally fill with boxes]
 *             boxaaInitFull(baa, boxa);
 *          A typical use is to initialize the array with empty boxa,
 *          and to replace only a subset that must be aligned with
 *          something else, such as a pixa.
 * </pre>
 */
l_ok
boxaaInitFull(BOXAA  *baa,
              BOXA   *boxa)
{
l_int32  i, n;
BOXA    *boxat;

    if (!baa)
        return ERROR_INT("baa not defined", __func__, 1);
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    n = baa->nalloc;
    baa->n = n;
    for (i = 0; i < n; i++) {
        boxat = boxaCopy(boxa, L_COPY);
        boxaaReplaceBoxa(baa, i, boxat);
    }
    return 0;
}


/*!
 * \brief   boxaaExtendWithInit()
 *
 * \param[in]    baa
 * \param[in]    maxindex
 * \param[in]    boxa       to be replicated into the extended ptr array
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This should be used on an existing boxaa that has been
 *          fully loaded with boxa.  It then extends the boxaa,
 *          loading all the additional ptrs with copies of boxa.
 *          Typically, boxa will be empty.
 * </pre>
 */
l_ok
boxaaExtendWithInit(BOXAA   *baa,
                    l_int32  maxindex,
                    BOXA    *boxa)
{
l_int32  i, n;

    if (!baa)
        return ERROR_INT("baa not defined", __func__, 1);
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

        /* Extend the ptr array if necessary */
    n = boxaaGetCount(baa);
    if (maxindex < n) return 0;
    if (boxaaExtendArrayToSize(baa, maxindex + 1))
        return ERROR_INT("extension failed", __func__, 1);

        /* Fill the new entries with copies of boxa */
    for (i = n; i <= maxindex; i++)
        boxaaAddBoxa(baa, boxa, L_COPY);
    return 0;
}


/*!
 * \brief   boxaaReplaceBoxa()
 *
 * \param[in]    baa
 * \param[in]    index    to the index-th boxa
 * \param[in]    boxa     insert and replace any existing one
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Any existing boxa is destroyed, and the input one
 *          is inserted in its place.
 *      (2) If the index is invalid, return 1 (error)
 * </pre>
 */
l_ok
boxaaReplaceBoxa(BOXAA   *baa,
                 l_int32  index,
                 BOXA    *boxa)
{
l_int32  n;

    if (!baa)
        return ERROR_INT("baa not defined", __func__, 1);
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    n = boxaaGetCount(baa);
    if (index < 0 || index >= n)
        return ERROR_INT("index not valid", __func__, 1);

    boxaDestroy(&baa->boxa[index]);
    baa->boxa[index] = boxa;
    return 0;
}


/*!
 * \brief   boxaaInsertBoxa()
 *
 * \param[in]    baa
 * \param[in]    index    location in boxaa to insert new boxa
 * \param[in]    boxa     new boxa to be inserted
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This shifts boxa[i] --> boxa[i + 1] for all i >= index,
 *          and then inserts boxa as boxa[index].  It is typically used
 *          when %baa is full of boxa.
 *      (2) To insert at the beginning of the array, set %index = 0.
 *      (3) To append to the array, it is equivalent to boxaaAddBoxa().
 *      (4) This should not be used repeatedly to insert into large arrays,
 *          because the function is O(n).
 * </pre>
 */
l_ok
boxaaInsertBoxa(BOXAA   *baa,
                l_int32  index,
                BOXA    *boxa)
{
l_int32  i, n;
BOXA   **array;

    if (!baa)
        return ERROR_INT("baa not defined", __func__, 1);
    n = boxaaGetCount(baa);
    if (index < 0 || index > n) {
        L_ERROR("index %d not in [0,...,%d]\n", __func__, index, n);
        return 1;
    }
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    if (n >= baa->nalloc) {
        if (boxaaExtendArray(baa))
            return ERROR_INT("extension failed", __func__, 1);
    }
    array = baa->boxa;
    baa->n++;
    for (i = n; i > index; i--)
        array[i] = array[i - 1];
    array[index] = boxa;
    return 0;
}


/*!
 * \brief   boxaaRemoveBoxa()
 *
 * \param[in]    baa
 * \param[in]    index   of the boxa to be removed and destroyed
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This removes boxa[index] and then shifts
 *          boxa[i] --> boxa[i - 1] for all i > index.
 *      (2) The removed boxaa is destroyed.
 *      (2) This should not be used repeatedly on large arrays,
 *          because the function is O(n).
 * </pre>
 */
l_ok
boxaaRemoveBoxa(BOXAA   *baa,
                l_int32  index)
{
l_int32  i, n;
BOXA   **array;

    if (!baa)
        return ERROR_INT("baa not defined", __func__, 1);
    n = boxaaGetCount(baa);
    if (index < 0 || index >= n)
        return ERROR_INT("index not valid", __func__, 1);

    array = baa->boxa;
    boxaDestroy(&array[index]);
    for (i = index + 1; i < n; i++)
        array[i - 1] = array[i];
    array[n - 1] = NULL;
    baa->n--;

    return 0;
}


/*!
 * \brief   boxaaAddBox()
 *
 * \param[in]    baa
 * \param[in]    index       of boxa with boxaa
 * \param[in]    box         to be added
 * \param[in]    accessflag  L_INSERT, L_COPY or L_CLONE
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Adds to an existing boxa only.
 * </pre>
 */
l_ok
boxaaAddBox(BOXAA   *baa,
            l_int32  index,
            BOX     *box,
            l_int32  accessflag)
{
l_int32  n;
BOXA    *boxa;
    if (!baa)
        return ERROR_INT("baa not defined", __func__, 1);
    n = boxaaGetCount(baa);
    if (index < 0 || index >= n)
        return ERROR_INT("index not valid", __func__, 1);
    if (accessflag != L_INSERT && accessflag != L_COPY && accessflag != L_CLONE)
        return ERROR_INT("invalid accessflag", __func__, 1);

    boxa = boxaaGetBoxa(baa, index, L_CLONE);
    boxaAddBox(boxa, box, accessflag);
    boxaDestroy(&boxa);
    return 0;
}


/*---------------------------------------------------------------------*
 *                        Boxaa serialized I/O                         *
 *---------------------------------------------------------------------*/
/*!
 * \brief   boxaaReadFromFiles()
 *
 * \param[in]    dirname   directory
 * \param[in]    substr    [optional] substring filter on filenames; can be NULL
 * \param[in]    first     0-based
 * \param[in]    nfiles    use 0 for everything from %first to the end
 * \return  baa, or NULL on error or if no boxa files are found.
 *
 * <pre>
 * Notes:
 *      (1) The files must be serialized boxa files (e.g., *.ba).
 *          If some files cannot be read, warnings are issued.
 *      (2) Use %substr to filter filenames in the directory.  If
 *          %substr == NULL, this takes all files.
 *      (3) After filtering, use %first and %nfiles to select
 *          a contiguous set of files, that have been lexically
 *          sorted in increasing order.
 * </pre>
 */
BOXAA *
boxaaReadFromFiles(const char  *dirname,
                   const char  *substr,
                   l_int32      first,
                   l_int32      nfiles)
{
char    *fname;
l_int32  i, n;
BOXA    *boxa;
BOXAA   *baa;
SARRAY  *sa;

  if (!dirname)
      return (BOXAA *)ERROR_PTR("dirname not defined", __func__, NULL);

  sa = getSortedPathnamesInDirectory(dirname, substr, first, nfiles);
  if (!sa || ((n = sarrayGetCount(sa)) == 0)) {
      sarrayDestroy(&sa);
      return (BOXAA *)ERROR_PTR("no pixa files found", __func__, NULL);
  }

  baa = boxaaCreate(n);
  for (i = 0; i < n; i++) {
      fname = sarrayGetString(sa, i, L_NOCOPY);
      if ((boxa = boxaRead(fname)) == NULL) {
          L_ERROR("boxa not read for %d-th file", __func__, i);
          continue;
      }
      boxaaAddBoxa(baa, boxa, L_INSERT);
  }

  sarrayDestroy(&sa);
  return baa;
}


/*!
 * \brief   boxaaRead()
 *
 * \param[in]    filename
 * \return  boxaa, or NULL on error
 */
BOXAA *
boxaaRead(const char  *filename)
{
FILE   *fp;
BOXAA  *baa;

    if (!filename)
        return (BOXAA *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (BOXAA *)ERROR_PTR_1("stream not opened",
                                    filename, __func__, NULL);
    baa = boxaaReadStream(fp);
    fclose(fp);
    if (!baa)
        return (BOXAA *)ERROR_PTR_1("boxaa not read",
                                    filename, __func__, NULL);
    return baa;
}


/*!
 * \brief   boxaaReadStream()
 *
 * \param[in]    fp    input file stream
 * \return  boxaa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) It is OK for the boxaa to be empty (n == 0).
 * </pre>
 */
BOXAA *
boxaaReadStream(FILE  *fp)
{
l_int32  n, i, x, y, w, h, version;
l_int32  ignore;
BOXA    *boxa;
BOXAA   *baa;

    if (!fp)
        return (BOXAA *)ERROR_PTR("stream not defined", __func__, NULL);

    if (fscanf(fp, "\nBoxaa Version %d\n", &version) != 1)
        return (BOXAA *)ERROR_PTR("not a boxaa file", __func__, NULL);
    if (version != BOXAA_VERSION_NUMBER)
        return (BOXAA *)ERROR_PTR("invalid boxa version", __func__, NULL);
    if (fscanf(fp, "Number of boxa = %d\n", &n) != 1)
        return (BOXAA *)ERROR_PTR("not a boxaa file", __func__, NULL);
    if (n < 0)
        return (BOXAA *)ERROR_PTR("num boxa ptrs < 0", __func__, NULL);
    if (n > MaxBoxaaPtrArraySize)
        return (BOXAA *)ERROR_PTR("too many boxa ptrs", __func__, NULL);
    if (n == 0) L_INFO("the boxaa is empty\n", __func__);

    if ((baa = boxaaCreate(n)) == NULL)
        return (BOXAA *)ERROR_PTR("boxaa not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        if (fscanf(fp, "\nBoxa[%d] extent: x = %d, y = %d, w = %d, h = %d",
                   &ignore, &x, &y, &w, &h) != 5) {
            boxaaDestroy(&baa);
            return (BOXAA *)ERROR_PTR("boxa descr not valid", __func__, NULL);
        }
        if ((boxa = boxaReadStream(fp)) == NULL) {
            boxaaDestroy(&baa);
            return (BOXAA *)ERROR_PTR("boxa not made", __func__, NULL);
        }
        boxaaAddBoxa(baa, boxa, L_INSERT);
    }
    return baa;
}


/*!
 * \brief   boxaaReadMem()
 *
 * \param[in]    data     serialization of boxaa; in ascii
 * \param[in]    size     of data in bytes; can use strlen to get it
 * \return  baa, or NULL on error
 */
BOXAA *
boxaaReadMem(const l_uint8  *data,
             size_t          size)
{
FILE   *fp;
BOXAA  *baa;

    if (!data)
        return (BOXAA *)ERROR_PTR("data not defined", __func__, NULL);
    if ((fp = fopenReadFromMemory(data, size)) == NULL)
        return (BOXAA *)ERROR_PTR("stream not opened", __func__, NULL);

    baa = boxaaReadStream(fp);
    fclose(fp);
    if (!baa) L_ERROR("baa not read\n", __func__);
    return baa;
}


/*!
 * \brief   boxaaWrite()
 *
 * \param[in]    filename
 * \param[in]    baa
 * \return  0 if OK, 1 on error
 */
l_ok
boxaaWrite(const char  *filename,
           BOXAA       *baa)
{
l_int32  ret;
FILE    *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!baa)
        return ERROR_INT("baa not defined", __func__, 1);

    if ((fp = fopenWriteStream(filename, "w")) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    ret = boxaaWriteStream(fp, baa);
    fclose(fp);
    if (ret)
        return ERROR_INT_1("baa not written to stream", filename, __func__, 1);
    return 0;
}


/*!
 * \brief   boxaaWriteStream()
 *
 * \param[in]   fp    output file stream
 * \param[in]   baa
 * \return  0 if OK, 1 on error
 */
l_ok
boxaaWriteStream(FILE   *fp,
                 BOXAA  *baa)
{
l_int32  n, i, x, y, w, h;
BOX     *box;
BOXA    *boxa;

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!baa)
        return ERROR_INT("baa not defined", __func__, 1);

    n = boxaaGetCount(baa);
    fprintf(fp, "\nBoxaa Version %d\n", BOXAA_VERSION_NUMBER);
    fprintf(fp, "Number of boxa = %d\n", n);

    for (i = 0; i < n; i++) {
        if ((boxa = boxaaGetBoxa(baa, i, L_CLONE)) == NULL)
            return ERROR_INT("boxa not found", __func__, 1);
        boxaGetExtent(boxa, NULL, NULL, &box);
        boxGetGeometry(box, &x, &y, &w, &h);
        fprintf(fp, "\nBoxa[%d] extent: x = %d, y = %d, w = %d, h = %d",
                i, x, y, w, h);
        boxaWriteStream(fp, boxa);
        boxDestroy(&box);
        boxaDestroy(&boxa);
    }
    return 0;
}


/*!
 * \brief   boxaaWriteMem()
 *
 * \param[out]   pdata    data of serialized boxaa; ascii
 * \param[out]   psize    size of returned data
 * \param[in]    baa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Serializes a boxaa in memory and puts the result in a buffer.
 * </pre>
 */
l_ok
boxaaWriteMem(l_uint8  **pdata,
              size_t    *psize,
              BOXAA     *baa)
{
l_int32  ret;
FILE    *fp;

    if (pdata) *pdata = NULL;
    if (psize) *psize = 0;
    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1);
    if (!baa)
        return ERROR_INT("baa not defined", __func__, 1);

#if HAVE_FMEMOPEN
    if ((fp = open_memstream((char **)pdata, psize)) == NULL)
        return ERROR_INT("stream not opened", __func__, 1);
    ret = boxaaWriteStream(fp, baa);
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
    ret = boxaaWriteStream(fp, baa);
    rewind(fp);
    *pdata = l_binaryReadStream(fp, psize);
    fclose(fp);
#endif  /* HAVE_FMEMOPEN */
    return ret;
}


/*---------------------------------------------------------------------*
 *                         Boxa serialized I/O                         *
 *---------------------------------------------------------------------*/
/*!
 * \brief   boxaRead()
 *
 * \param[in]    filename
 * \return  boxa, or NULL on error
 */
BOXA *
boxaRead(const char  *filename)
{
FILE  *fp;
BOXA  *boxa;

    if (!filename)
        return (BOXA *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (BOXA *)ERROR_PTR_1("stream not opened",
                                   filename, __func__, NULL);
    boxa = boxaReadStream(fp);
    fclose(fp);
    if (!boxa)
        return (BOXA *)ERROR_PTR_1("boxa not read",
                                   filename, __func__, NULL);
    return boxa;
}


/*!
 * \brief   boxaReadStream()
 *
 * \param[in]    fp   input file stream
 * \return  boxa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) It is OK for the boxa to be empty (n == 0).
 * </pre>
 */
BOXA *
boxaReadStream(FILE  *fp)
{
l_int32  n, i, x, y, w, h, version;
l_int32  ignore;
BOX     *box;
BOXA    *boxa;

    if (!fp)
        return (BOXA *)ERROR_PTR("stream not defined", __func__, NULL);

    if (fscanf(fp, "\nBoxa Version %d\n", &version) != 1)
        return (BOXA *)ERROR_PTR("not a boxa file", __func__, NULL);
    if (version != BOXA_VERSION_NUMBER)
        return (BOXA *)ERROR_PTR("invalid boxa version", __func__, NULL);
    if (fscanf(fp, "Number of boxes = %d\n", &n) != 1)
        return (BOXA *)ERROR_PTR("not a boxa file", __func__, NULL);
    if (n < 0)
        return (BOXA *)ERROR_PTR("num box ptrs < 0", __func__, NULL);
    if (n > MaxBoxaPtrArraySize)
        return (BOXA *)ERROR_PTR("too many box ptrs", __func__, NULL);
    if (n == 0) L_INFO("the boxa is empty\n", __func__);

    if ((boxa = boxaCreate(n)) == NULL)
        return (BOXA *)ERROR_PTR("boxa not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        if (fscanf(fp, "  Box[%d]: x = %d, y = %d, w = %d, h = %d\n",
                &ignore, &x, &y, &w, &h) != 5) {
            boxaDestroy(&boxa);
            return (BOXA *)ERROR_PTR("box descr not valid", __func__, NULL);
        }
        box = boxCreate(x, y, w, h);
        boxaAddBox(boxa, box, L_INSERT);
    }
    return boxa;
}


/*!
 * \brief   boxaReadMem()
 *
 * \param[in]    data    serialization of boxa; in ascii
 * \param[in]    size    of data in bytes; can use strlen to get it
 * \return  boxa, or NULL on error
 */
BOXA *
boxaReadMem(const l_uint8  *data,
            size_t          size)
{
FILE  *fp;
BOXA  *boxa;

    if (!data)
        return (BOXA *)ERROR_PTR("data not defined", __func__, NULL);
    if ((fp = fopenReadFromMemory(data, size)) == NULL)
        return (BOXA *)ERROR_PTR("stream not opened", __func__, NULL);

    boxa = boxaReadStream(fp);
    fclose(fp);
    if (!boxa) L_ERROR("boxa not read\n", __func__);
    return boxa;
}


/*!
 * \brief   boxaWriteDebug()
 *
 * \param[in]    filename
 * \param[in]    boxa
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Debug version, intended for use in the library when writing
 *          to files in a temp directory with names that are compiled in.
 *          This is used instead of boxaWrite() for all such library calls.
 *      (2) The global variable LeptDebugOK defaults to 0, and can be set
 *          or cleared by the function setLeptDebugOK().
 * </pre>
 */
l_ok
boxaWriteDebug(const char  *filename,
               BOXA        *boxa)
{
    if (LeptDebugOK) {
        return boxaWrite(filename, boxa);
    } else {
        L_INFO("write to named temp file %s is disabled\n", __func__, filename);
        return 0;
    }
}


/*!
 * \brief   boxaWrite()
 *
 * \param[in]    filename
 * \param[in]    boxa
 * \return  0 if OK, 1 on error
 */
l_ok
boxaWrite(const char  *filename,
          BOXA        *boxa)
{
l_int32  ret;
FILE    *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    if ((fp = fopenWriteStream(filename, "w")) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    ret = boxaWriteStream(fp, boxa);
    fclose(fp);
    if (ret)
        return ERROR_INT_1("boxa not written to stream", filename, __func__, 1);

    return 0;
}


/*!
 * \brief   boxaWriteStream()
 *
 * \param[in]   fp     file stream; use NULL for stderr
 * \param[in]   boxa
 * \return  0 if OK, 1 on error
 */
l_ok
boxaWriteStream(FILE  *fp,
                BOXA  *boxa)
{
l_int32  n, i;
BOX     *box;

    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);
    if (!fp)
        return boxaWriteStderr(boxa);

    n = boxaGetCount(boxa);
    fprintf(fp, "\nBoxa Version %d\n", BOXA_VERSION_NUMBER);
    fprintf(fp, "Number of boxes = %d\n", n);
    for (i = 0; i < n; i++) {
        if ((box = boxaGetBox(boxa, i, L_CLONE)) == NULL)
            return ERROR_INT("box not found", __func__, 1);
        fprintf(fp, "  Box[%d]: x = %d, y = %d, w = %d, h = %d\n",
                i, box->x, box->y, box->w, box->h);
        boxDestroy(&box);
    }
    return 0;
}


/*!
 * \brief   boxaWriteStderr()
 *
 * \param[in]   boxa
 * \return  0 if OK, 1 on error
 */
l_ok
boxaWriteStderr(BOXA  *boxa)
{
l_int32  n, i;
BOX     *box;

    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

    n = boxaGetCount(boxa);
    lept_stderr("\nBoxa Version %d\n", BOXA_VERSION_NUMBER);
    lept_stderr("Number of boxes = %d\n", n);
    for (i = 0; i < n; i++) {
        if ((box = boxaGetBox(boxa, i, L_CLONE)) == NULL)
            return ERROR_INT("box not found", __func__, 1);
        lept_stderr("  Box[%d]: x = %d, y = %d, w = %d, h = %d\n",
                i, box->x, box->y, box->w, box->h);
        boxDestroy(&box);
    }
    return 0;
}


/*!
 * \brief   boxaWriteMem()
 *
 * \param[out]   pdata   data of serialized boxa; ascii
 * \param[out]   psize   size of returned data
 * \param[in]    boxa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Serializes a boxa in memory and puts the result in a buffer.
 * </pre>
 */
l_ok
boxaWriteMem(l_uint8  **pdata,
             size_t    *psize,
             BOXA      *boxa)
{
l_int32  ret;
FILE    *fp;

    if (pdata) *pdata = NULL;
    if (psize) *psize = 0;
    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1);
    if (!boxa)
        return ERROR_INT("boxa not defined", __func__, 1);

#if HAVE_FMEMOPEN
    if ((fp = open_memstream((char **)pdata, psize)) == NULL)
        return ERROR_INT("stream not opened", __func__, 1);
    ret = boxaWriteStream(fp, boxa);
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
    ret = boxaWriteStream(fp, boxa);
    rewind(fp);
    *pdata = l_binaryReadStream(fp, psize);
    fclose(fp);
#endif  /* HAVE_FMEMOPEN */
    return ret;
}


/*---------------------------------------------------------------------*
 *                            Debug printing                           *
 *---------------------------------------------------------------------*/
/*!
 * \brief   boxPrintStreamInfo()
 *
 * \param[in]    fp    file stream; use NULL for stderr
 * \param[in]    box
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This outputs debug info.  Use serialization functions to
 *          write to file if you want to read the data back.
 * </pre>
 */
l_ok
boxPrintStreamInfo(FILE  *fp,
                   BOX   *box)
{
    if (!box)
        return ERROR_INT("box not defined", __func__, 1);

    if (!fp) {  /* output to stderr */
        lept_stderr(" Box: x = %d, y = %d, w = %d, h = %d\n",
                    box->x, box->y, box->w, box->h);
    } else {
        fprintf(fp, " Box: x = %d, y = %d, w = %d, h = %d\n",
                box->x, box->y, box->w, box->h);
    }
    return 0;
}

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
 * \file  ptra.c
 * <pre>
 *
 *      Ptra creation and destruction
 *          L_PTRA      *ptraCreate()
 *          void        *ptraDestroy()
 *
 *      Add/insert/remove/replace generic ptr object
 *          l_int32      ptraAdd()
 *          static l_int32  ptraExtendArray()
 *          l_int32      ptraInsert()
 *          void        *ptraRemove()
 *          void        *ptraRemoveLast()
 *          void        *ptraReplace()
 *          l_int32      ptraSwap()
 *          l_int32      ptraCompactArray()
 *
 *      Other array operations
 *          l_int32      ptraReverse()
 *          l_int32      ptraJoin()
 *
 *      Simple Ptra accessors
 *          l_int32      ptraGetMaxIndex()
 *          l_int32      ptraGetActualCount()
 *          void        *ptraGetPtrToItem()
 *
 *      Ptraa creation and destruction
 *          L_PTRAA     *ptraaCreate()
 *          void        *ptraaDestroy()
 *
 *      Ptraa accessors
 *          l_int32      ptraaGetSize()
 *          l_int32      ptraaInsertPtra()
 *          L_PTRA      *ptraaGetPtra()
 *
 *      Ptraa conversion
 *          L_PTRA      *ptraaFlattenToPtra()
 *
 *    Notes on the Ptra:
 *
 *    (1) The Ptra is a struct, not an array.  Always use the accessors
 *        in this file, never the fields directly.
 *    (2) Items can be placed anywhere in the allocated ptr array,
 *        including one index beyond the last ptr (in which case the
 *        ptr array is realloc'd).
 *    (3) Thus, the items on the ptr array need not be compacted.  In
 *        general there will be null pointers in the ptr array.
 *    (4) A compacted array will remain compacted on removal if
 *        arbitrary items are removed with compaction, or if items
 *        are removed from the end of the array.
 *    (5) For addition to and removal from the end of the array, this
 *        functions exactly like a stack, and with the same O(1) cost.
 *    (6) This differs from the generic stack in that we allow
 *        random access for insertion, removal and replacement.
 *        Removal can be done without compacting the array.
 *        Insertion into a null ptr in the array has no effect on
 *        the other pointers, but insertion into a location already
 *        occupied by an item has a cost proportional to the
 *        distance to the next null ptr in the array.
 *    (7) Null ptrs are valid input args for both insertion and
 *        replacement; this allows arbitrary swapping.
 *    (8) The item in the array with the largest index is at pa->imax.
 *        This can be any value from -1 (initialized; all array ptrs
 *        are null) up to pa->nalloc - 1 (the last ptr in the array).
 *    (9) In referring to the array: the first ptr is the "top" or
 *        "beginning"; the last pointer is the "bottom" or "end";
 *        items are shifted "up" towards the top when compaction occurs;
 *        and items are shifted "down" towards the bottom when forced to
 *        move due to an insertion.
 *   (10) It should be emphasized that insertion, removal and replacement
 *        are general:
 *         * You can insert an item into any ptr location in the
 *           allocated ptr array, as well as into the next ptr address
 *           beyond the allocated array (in which case a realloc will occur).
 *         * You can remove or replace an item from any ptr location
 *           in the allocated ptr array.
 *         * When inserting into an occupied location, you have
 *           three options for downshifting.
 *         * When removing, you can either leave the ptr null or
 *           compact the array.
 *
 *    Notes on the Ptraa:
 *
 *    (1) The Ptraa is a fixed size ptr array for holding Ptra.
 *        In that respect, it is different from other pointer arrays, which
 *        are extensible and grow using the *Add*() functions.
 *    (2) In general, the Ptra ptrs in the Ptraa can be randomly occupied.
 *        A typical usage is to allow an O(n) horizontal sort of Pix,
 *        where the size of the Ptra array is the width of the image,
 *        and each Ptra is an array of all the Pix at a specific x location.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

    /* Bounds on initial array size */
LEPT_DLL const l_uint32  MaxInitPtraSize = 1000001;
static const l_int32 DefaultInitPtraSize = 20;      /*!< n'importe quoi */

    /* Static function */
static l_int32 ptraExtendArray(L_PTRA *pa);

/*--------------------------------------------------------------------------*
 *                       Ptra creation and destruction                      *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   ptraCreate()
 *
 * \param[in]    n     size of ptr array to be alloc'd; use 0 for default
 * \return  pa, or NULL on error
 */
L_PTRA *
ptraCreate(l_int32  n)
{
L_PTRA  *pa;

    if (n > (l_int32)MaxInitPtraSize) {
        L_ERROR("n = %d > maxsize = %d\n", __func__, n, MaxInitPtraSize);
        return NULL;
    }
    if (n <= 0) n = DefaultInitPtraSize;

    pa = (L_PTRA *)LEPT_CALLOC(1, sizeof(L_PTRA));
    if ((pa->array = (void **)LEPT_CALLOC(n, sizeof(void *))) == NULL) {
        ptraDestroy(&pa, 0, 0);
        return (L_PTRA *)ERROR_PTR("ptr array not made", __func__, NULL);
    }
    pa->nalloc = n;
    pa->imax = -1;
    pa->nactual = 0;
    return pa;
}


/*!
 * \brief   ptraDestroy()
 *
 * \param[in,out]   ppa        will be set to null before returning
 * \param[in]       freeflag   TRUE to free each remaining item in the array
 * \param[in]       warnflag   TRUE to warn if any remaining items
 *                             are not destroyed
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) If %freeflag == TRUE, frees each item in the array.
 *      (2) If %freeflag == FALSE and %warnflag == TRUE, and there are
 *          items on the array, this gives a warning and destroys the array.
 *          If these items are not owned elsewhere, this will cause
 *          a memory leak of all the items that were on the array.
 *          So if the items are not owned elsewhere and require their
 *          own destroy function, they must be destroyed before the ptra.
 *      (3) If %warnflag == FALSE, no warnings will be issued.  This is
 *          useful if the items are owned elsewhere, such as a
 *          PixMemoryStore().
 *      (4) To destroy the ptra, we destroy the ptr array, then
 *          the ptra, and then null the contents of the input ptr.
 * </pre>
 */
void
ptraDestroy(L_PTRA  **ppa,
            l_int32   freeflag,
            l_int32   warnflag)
{
l_int32  i, nactual;
void    *item;
L_PTRA  *pa;

    if (ppa == NULL) {
        L_WARNING("ptr address is NULL\n", __func__);
        return;
    }
    if ((pa = *ppa) == NULL)
        return;

    ptraGetActualCount(pa, &nactual);
    if (nactual > 0) {
        if (freeflag) {
            for (i = 0; i <= pa->imax; i++) {
                if ((item = ptraRemove(pa, i, L_NO_COMPACTION)) != NULL)
                    LEPT_FREE(item);
            }
        } else if (warnflag) {
            L_WARNING("potential memory leak of %d items in ptra\n",
                      __func__, nactual);
        }
    }

    LEPT_FREE(pa->array);
    LEPT_FREE(pa);
    *ppa = NULL;
}


/*--------------------------------------------------------------------------*
 *               Add/insert/remove/replace generic ptr object               *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   ptraAdd()
 *
 * \param[in]    pa      ptra
 * \param[in]    item    generic ptr to a struct
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This adds the element to the next location beyond imax,
 *          which is the largest occupied ptr in the array.  This is
 *          what you expect from a stack, where all ptrs up to and
 *          including imax are occupied, but here the occuption of
 *          items in the array is entirely arbitrary.
 * </pre>
 */
l_ok
ptraAdd(L_PTRA  *pa,
        void    *item)
{
l_int32  imax;

    if (!pa)
        return ERROR_INT("pa not defined", __func__, 1);
    if (!item)
        return ERROR_INT("item not defined", __func__, 1);

    ptraGetMaxIndex(pa, &imax);
    if (imax >= pa->nalloc - 1 && ptraExtendArray(pa))
        return ERROR_INT("extension failure", __func__, 1);
    pa->array[imax + 1] = (void *)item;
    pa->imax++;
    pa->nactual++;
    return 0;
}


/*!
 * \brief   ptraExtendArray()
 *
 * \param[in]    pa
 * \return  0 if OK, 1 on error
 */
static l_int32
ptraExtendArray(L_PTRA  *pa)
{
    if (!pa)
        return ERROR_INT("pa not defined", __func__, 1);

    if ((pa->array = (void **)reallocNew((void **)&pa->array,
                                sizeof(void *) * pa->nalloc,
                                2 * sizeof(void *) * pa->nalloc)) == NULL)
            return ERROR_INT("new ptr array not returned", __func__, 1);

    pa->nalloc *= 2;
    return 0;
}


/*!
 * \brief   ptraInsert()
 *
 * \param[in]    pa          ptra
 * \param[in]    index       location in ptra to insert new value
 * \param[in]    item        generic ptr to a struct; can be null
 * \param[in]    shiftflag   L_AUTO_DOWNSHIFT, L_MIN_DOWNSHIFT, L_FULL_DOWNSHIFT
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This checks first to see if the location is valid, and
 *          then if there is presently an item there.  If there is not,
 *          it is simply inserted into that location.
 *      (2) If there is an item at the insert location, items must be
 *          moved down to make room for the insert.  In the downward
 *          shift there are three options, given by %shiftflag.
 *            ~ If %shiftflag == L_AUTO_DOWNSHIFT, a decision is made
 *              whether, in a cascade of items, to downshift a minimum
 *              amount or for all items above %index.  The decision is
 *              based on the expectation of finding holes (null ptrs)
 *              between %index and the bottom of the array.
 *              Assuming the holes are distributed uniformly, if 2 or more
 *              holes are expected, we do a minimum shift.
 *            ~ If %shiftflag == L_MIN_DOWNSHIFT, the downward shifting
 *              cascade of items progresses a minimum amount, until
 *              the first empty slot is reached.  This mode requires
 *              some computation before the actual shifting is done.
 *            ~ If %shiftflag == L_FULL_DOWNSHIFT, a shifting cascade is
 *              performed where pa[i] --> pa[i + 1] for all i >= index.
 *              Then, the item is inserted at pa[index].
 *      (3) If you are not using L_AUTO_DOWNSHIFT, the rule of thumb is
 *          to use L_FULL_DOWNSHIFT if the array is compacted (each
 *          element points to an item), and to use L_MIN_DOWNSHIFT
 *          if there are a significant number of null pointers.
 *          There is no penalty to using L_MIN_DOWNSHIFT for a
 *          compacted array, however, because the full shift is required
 *          and we don't do the O(n) computation to look for holes.
 *      (4) This should not be used repeatedly on large arrays,
 *          because the function is generally O(n).
 *      (5) However, it can be used repeatedly if we start with an empty
 *          ptr array and insert only once at each location.  For example,
 *          you can support an array of Numa, where at each ptr location
 *          you store either 0 or 1 Numa, and the Numa can be added
 *          randomly to the ptr array.
 * </pre>
 */
l_ok
ptraInsert(L_PTRA  *pa,
           l_int32  index,
           void    *item,
           l_int32  shiftflag)
{
l_int32    i, ihole, imax;
l_float32  nexpected;

    if (!pa)
        return ERROR_INT("pa not defined", __func__, 1);
    if (index < 0 || index > pa->nalloc)
        return ERROR_INT("index not in [0 ... nalloc]", __func__, 1);
    if (shiftflag != L_AUTO_DOWNSHIFT && shiftflag != L_MIN_DOWNSHIFT &&
        shiftflag != L_FULL_DOWNSHIFT)
        return ERROR_INT("invalid shiftflag", __func__, 1);

    if (item) pa->nactual++;
    if (index == pa->nalloc) {  /* can happen when index == n */
        if (ptraExtendArray(pa))
            return ERROR_INT("extension failure", __func__, 1);
    }

        /* We are inserting into a hole or adding to the end of the array.
         * No existing items are moved. */
    ptraGetMaxIndex(pa, &imax);
    if (pa->array[index] == NULL) {
        pa->array[index] = item;
        if (item && index > imax)  /* new item put beyond max so far */
            pa->imax = index;
        return 0;
    }

        /* We are inserting at the location of an existing item,
         * forcing the existing item and those below to shift down.
         * First, extend the array automatically if the last element
         * (nalloc - 1) is occupied (imax).  This may not be necessary
         * in every situation, but only an anomalous sequence of insertions
         * into the array would cause extra ptr allocation.  */
    if (imax >= pa->nalloc - 1 && ptraExtendArray(pa))
        return ERROR_INT("extension failure", __func__, 1);

        /* If there are no holes, do a full downshift.
         * Otherwise, if L_AUTO_DOWNSHIFT, use the expected number
         * of holes between index and n to determine the shift mode */
    if (imax + 1 == pa->nactual) {
        shiftflag = L_FULL_DOWNSHIFT;
    } else if (shiftflag == L_AUTO_DOWNSHIFT) {
        if (imax < 10) {
            shiftflag = L_FULL_DOWNSHIFT;  /* no big deal */
        } else {
            nexpected = (l_float32)(imax - pa->nactual) *
                         (l_float32)((imax - index) / imax);
            shiftflag = (nexpected > 2.0) ? L_MIN_DOWNSHIFT : L_FULL_DOWNSHIFT;
        }
    }

    if (shiftflag == L_MIN_DOWNSHIFT) {  /* run down looking for a hole */
        for (ihole = index + 1; ihole <= imax; ihole++) {
             if (pa->array[ihole] == NULL)
                 break;
        }
    } else {  /* L_FULL_DOWNSHIFT */
        ihole = imax + 1;
    }

    for (i = ihole; i > index; i--)
        pa->array[i] = pa->array[i - 1];
    pa->array[index] = (void *)item;
    if (ihole == imax + 1)  /* the last item was shifted down */
        pa->imax++;

    return 0;
}


/*!
 * \brief   ptraRemove()
 *
 * \param[in]    pa       ptra
 * \param[in]    index    element to be removed
 * \param[in]    flag     L_NO_COMPACTION, L_COMPACTION
 * \return  item, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If flag == L_NO_COMPACTION, this removes the item and
 *          nulls the ptr on the array.  If it takes the last item
 *          in the array, pa->n is reduced to the next item.
 *      (2) If flag == L_COMPACTION, this compacts the array for
 *          for all i >= index.  It should not be used repeatedly on
 *          large arrays, because compaction is O(n).
 *      (3) The ability to remove without automatic compaction allows
 *          removal with cost O(1).
 * </pre>
 */
void *
ptraRemove(L_PTRA  *pa,
           l_int32  index,
           l_int32  flag)
{
l_int32  i, imax, fromend, icurrent;
void    *item;

    if (!pa)
        return (void *)ERROR_PTR("pa not defined", __func__, NULL);
    ptraGetMaxIndex(pa, &imax);
    if (index < 0 || index > imax)
        return (void *)ERROR_PTR("index not in [0 ... imax]", __func__, NULL);

    item = pa->array[index];
    if (item)
        pa->nactual--;
    pa->array[index] = NULL;

        /* If we took the last item, need to reduce pa->n */
    fromend = (index == imax);
    if (fromend) {
        for (i = index - 1; i >= 0; i--) {
            if (pa->array[i])
                break;
        }
        pa->imax = i;
    }

        /* Compact from index to the end of the array */
    if (!fromend && flag == L_COMPACTION) {
        for (icurrent = index, i = index + 1; i <= imax; i++) {
            if (pa->array[i])
                pa->array[icurrent++] = pa->array[i];
        }
        pa->imax = icurrent - 1;
    }
    return item;
}


/*!
 * \brief   ptraRemoveLast()
 *
 * \param[in]    pa    ptra
 * \return  item, or NULL on error or if the array is empty
 */
void *
ptraRemoveLast(L_PTRA  *pa)
{
l_int32  imax;

    if (!pa)
        return (void *)ERROR_PTR("pa not defined", __func__, NULL);

        /* Remove the last item in the array.  No compaction is required. */
    ptraGetMaxIndex(pa, &imax);
    if (imax >= 0)
        return ptraRemove(pa, imax, L_NO_COMPACTION);
    else  /* empty */
        return NULL;
}


/*!
 * \brief   ptraReplace()
 *
 * \param[in]    pa          ptra
 * \param[in]    index       element to be replaced
 * \param[in]    item        new generic ptr to a struct; can be null
 * \param[in]    freeflag    TRUE to free old item; FALSE to return it
 * \return  item  old item, if it exists and is not freed,
 *                     or NULL on error
 */
void *
ptraReplace(L_PTRA  *pa,
            l_int32  index,
            void    *item,
            l_int32  freeflag)
{
l_int32  imax;
void    *olditem;

    if (!pa)
        return (void *)ERROR_PTR("pa not defined", __func__, NULL);
    ptraGetMaxIndex(pa, &imax);
    if (index < 0 || index > imax)
        return (void *)ERROR_PTR("index not in [0 ... imax]", __func__, NULL);

    olditem = pa->array[index];
    pa->array[index] = item;
    if (!item && olditem)
        pa->nactual--;
    else if (item && !olditem)
        pa->nactual++;

    if (freeflag == FALSE)
        return olditem;

    if (olditem)
        LEPT_FREE(olditem);
    return NULL;
}


/*!
 * \brief   ptraSwap()
 *
 * \param[in]    pa ptra
 * \param[in]    index1
 * \param[in]    index2
 * \return  0 if OK, 1 on error
 */
l_ok
ptraSwap(L_PTRA  *pa,
         l_int32  index1,
         l_int32  index2)
{
l_int32  imax;
void    *item;

    if (!pa)
        return ERROR_INT("pa not defined", __func__, 1);
    if (index1 == index2)
        return 0;
    ptraGetMaxIndex(pa, &imax);
    if (index1 < 0 || index1 > imax || index2 < 0 || index2 > imax)
        return ERROR_INT("invalid index: not in [0 ... imax]", __func__, 1);

    item = ptraRemove(pa, index1, L_NO_COMPACTION);
    item = ptraReplace(pa, index2, item, FALSE);
    ptraInsert(pa, index1, item, L_MIN_DOWNSHIFT);
    return 0;
}


/*!
 * \brief   ptraCompactArray()
 *
 * \param[in]    pa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This compacts the items on the array, filling any empty ptrs.
 *      (2) This does not change the size of the array of ptrs.
 * </pre>
 */
l_ok
ptraCompactArray(L_PTRA  *pa)
{
l_int32  i, imax, nactual, index;

    if (!pa)
        return ERROR_INT("pa not defined", __func__, 1);
    ptraGetMaxIndex(pa, &imax);
    ptraGetActualCount(pa, &nactual);
    if (imax + 1 == nactual) return 0;

        /* Compact the array */
    for (i = 0, index = 0; i <= imax; i++) {
        if (pa->array[i])
             pa->array[index++] = pa->array[i];
    }
    pa->imax = index - 1;
    if (nactual != index)
        L_ERROR("index = %d; != nactual\n", __func__, index);

    return 0;
}


/*----------------------------------------------------------------------*
 *                        Other array operations                        *
 *----------------------------------------------------------------------*/
/*!
 * \brief   ptraReverse()
 *
 * \param[in]    pa     ptra
 * \return  0 if OK, 1 on error
 */
l_ok
ptraReverse(L_PTRA  *pa)
{
l_int32  i, imax;

    if (!pa)
        return ERROR_INT("pa not defined", __func__, 1);
    ptraGetMaxIndex(pa, &imax);

    for (i = 0; i < (imax + 1) / 2; i++)
        ptraSwap(pa, i, imax - i);
    return 0;
}


/*!
 * \brief   ptraJoin()
 *
 * \param[in]    pa1    add to this one
 * \param[in]    pa2    appended to pa1, and emptied of items; can be null
 * \return  0 if OK, 1 on error
 */
l_ok
ptraJoin(L_PTRA  *pa1,
         L_PTRA  *pa2)
{
l_int32  i, imax;
void    *item;

    if (!pa1)
        return ERROR_INT("pa1 not defined", __func__, 1);
    if (!pa2)
        return 0;

    ptraGetMaxIndex(pa2, &imax);
    for (i = 0; i <= imax; i++) {
        item = ptraRemove(pa2, i, L_NO_COMPACTION);
        ptraAdd(pa1, item);
    }

    return 0;
}



/*----------------------------------------------------------------------*
 *                        Simple ptra accessors                         *
 *----------------------------------------------------------------------*/
/*!
 * \brief   ptraGetMaxIndex()
 *
 * \param[in]    pa          ptra
 * \param[out]   pmaxindex   index of last item in the array;
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The largest index to an item in the array is %maxindex.
 *          %maxindex is one less than the number of items that would be
 *          in the array if there were no null pointers between 0
 *          and %maxindex - 1.  However, because the internal ptr array
 *          need not be compacted, there may be NULL pointers at
 *          indices below %maxindex; for example, if items have
 *          been removed.
 *      (2) When an item is added to the end of the array, it goes
 *          into pa->array[maxindex + 1], and maxindex is then
 *          incremented by 1.
 *      (3) If there are no items in the array, this returns %maxindex = -1.
 * </pre>
 */
l_ok
ptraGetMaxIndex(L_PTRA   *pa,
                l_int32  *pmaxindex)
{
    if (!pa)
        return ERROR_INT("pa not defined", __func__, 1);
    if (!pmaxindex)
        return ERROR_INT("&maxindex not defined", __func__, 1);
    *pmaxindex = pa->imax;
    return 0;
}


/*!
 * \brief   ptraGetActualCount()
 *
 * \param[in]    pa        ptra
 * \param[out]   pcount    actual number of items on the ptr array
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The actual number of items on the ptr array, pa->nactual,
 *          will be smaller than pa->n if the array is not compacted.
 * </pre>
 */
l_ok
ptraGetActualCount(L_PTRA   *pa,
                   l_int32  *pcount)
{
    if (!pa)
        return ERROR_INT("pa not defined", __func__, 1);
    if (!pcount)
        return ERROR_INT("&count not defined", __func__, 1);
    *pcount = pa->nactual;

    return 0;
}


/*!
 * \brief   ptraGetPtrToItem()
 *
 * \param[in]    pa       ptra
 * \param[in]    index    of element to be retrieved
 * \return  a ptr to the element, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This returns a ptr to the item.  You must cast it to
 *          the type of item.  Do not destroy it; the item belongs
 *          to the Ptra.
 *      (2) This can access all possible items on the ptr array.
 *          If an item doesn't exist, it returns null.
 * </pre>
 */
void *
ptraGetPtrToItem(L_PTRA  *pa,
                 l_int32  index)
{
    if (!pa)
        return (void *)ERROR_PTR("pa not defined", __func__, NULL);
    if (index < 0 || index >= pa->nalloc)
        return (void *)ERROR_PTR("index not in [0 ... nalloc-1]",
                                 __func__, NULL);

    return pa->array[index];
}


/*--------------------------------------------------------------------------*
 *                      Ptraa creation and destruction                      *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   ptraaCreate()
 *
 * \param[in]    n    size of ptr array to be alloc'd
 * \return  paa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The ptraa is generated with a fixed size, that can not change.
 *          The ptra can be generated and inserted randomly into this array.
 * </pre>
 */
L_PTRAA *
ptraaCreate(l_int32  n)
{
L_PTRAA  *paa;

    if (n <= 0)
        return (L_PTRAA *)ERROR_PTR("n must be > 0", __func__, NULL);

    paa = (L_PTRAA *)LEPT_CALLOC(1, sizeof(L_PTRAA));
    if ((paa->ptra = (L_PTRA **)LEPT_CALLOC(n, sizeof(L_PTRA *))) == NULL) {
        ptraaDestroy(&paa, 0, 0);
        return (L_PTRAA *)ERROR_PTR("ptr array not made", __func__, NULL);
    }
    paa->nalloc = n;
    return paa;
}


/*!
 * \brief   ptraaDestroy()
 *
 * \param[in,out]   ppaa       will be set to null before returning
 * \param[in]       freeflag   TRUE to free each remaining item in each ptra
 * \param[in]       warnflag   TRUE to warn if any remaining items
 *                             are not destroyed
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) See ptraDestroy() for use of %freeflag and %warnflag.
 *      (2) To destroy the ptraa, we destroy each ptra, then the ptr array,
 *          then the ptraa, and then null the contents of the input ptr.
 * </pre>
 */
void
ptraaDestroy(L_PTRAA  **ppaa,
             l_int32    freeflag,
             l_int32    warnflag)
{
l_int32   i, n;
L_PTRA   *pa;
L_PTRAA  *paa;

    if (ppaa == NULL) {
        L_WARNING("ptr address is NULL\n", __func__);
        return;
    }
    if ((paa = *ppaa) == NULL)
        return;

    ptraaGetSize(paa, &n);
    for (i = 0; i < n; i++) {
        pa = ptraaGetPtra(paa, i, L_REMOVE);
        ptraDestroy(&pa, freeflag, warnflag);
    }

    LEPT_FREE(paa->ptra);
    LEPT_FREE(paa);
    *ppaa = NULL;
}


/*--------------------------------------------------------------------------*
 *                             Ptraa accessors                              *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   ptraaGetSize()
 *
 * \param[in]    paa
 * \param[out]   psize    size of ptr array
 * \return  0 if OK; 1 on error
 */
l_ok
ptraaGetSize(L_PTRAA  *paa,
             l_int32  *psize)
{
    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1);
    *psize = paa->nalloc;

    return 0;
}


/*!
 * \brief   ptraaInsertPtra()
 *
 * \param[in]    paa      ptraa
 * \param[in]    index    location in array for insertion
 * \param[in]    pa       to be inserted
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Caller should check return value.  On success, the Ptra
 *          is inserted in the Ptraa and is owned by it.  However,
 *          on error, the Ptra remains owned by the caller.
 * </pre>
 */
l_ok
ptraaInsertPtra(L_PTRAA  *paa,
                l_int32   index,
                L_PTRA   *pa)
{
l_int32  n;

    if (!paa)
        return ERROR_INT("paa not defined", __func__, 1);
    if (!pa)
        return ERROR_INT("pa not defined", __func__, 1);
    ptraaGetSize(paa, &n);
    if (index < 0 || index >= n)
        return ERROR_INT("invalid index", __func__, 1);
    if (paa->ptra[index] != NULL)
        return ERROR_INT("ptra already stored at index", __func__, 1);

    paa->ptra[index] = pa;
    return 0;
}


/*!
 * \brief   ptraaGetPtra()
 *
 * \param[in]    paa          ptraa
 * \param[in]    index        location in array
 * \param[in]    accessflag   L_HANDLE_ONLY, L_REMOVE
 * \return  ptra at index location, or NULL on error or if there
 *              is no ptra there.
 *
 * <pre>
 * Notes:
 *      (1) This returns the ptra ptr.  If %accessflag == L_HANDLE_ONLY,
 *          the ptra is left on the ptraa.  If %accessflag == L_REMOVE,
 *          the ptr in the ptraa is set to NULL, and the caller
 *          is responsible for disposing of the ptra (either putting it
 *          back on the ptraa, or destroying it).
 *      (2) This returns NULL if there is no Ptra at the index location.
 * </pre>
 */
L_PTRA *
ptraaGetPtra(L_PTRAA  *paa,
             l_int32   index,
             l_int32   accessflag)
{
l_int32  n;
L_PTRA  *pa;

    if (!paa)
        return (L_PTRA *)ERROR_PTR("paa not defined", __func__, NULL);
    ptraaGetSize(paa, &n);
    if (index < 0 || index >= n)
        return (L_PTRA *)ERROR_PTR("invalid index", __func__, NULL);
    if (accessflag != L_HANDLE_ONLY && accessflag != L_REMOVE)
        return (L_PTRA *)ERROR_PTR("invalid accessflag", __func__, NULL);

    pa = paa->ptra[index];
    if (accessflag == L_REMOVE)
        paa->ptra[index] = NULL;
    return pa;
}


/*--------------------------------------------------------------------------*
 *                             Ptraa conversion                             *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   ptraaFlattenToPtra()
 *
 * \param[in]    paa    ptraa
 * \return  ptra, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This 'flattens' the ptraa to a ptra, taking the items in
 *          each ptra, in order, starting with the first ptra, etc.
 *      (2) As a side-effect, the ptra are all removed from the ptraa
 *          and destroyed, leaving an empty ptraa.
 * </pre>
 */
L_PTRA *
ptraaFlattenToPtra(L_PTRAA  *paa)
{
l_int32  i, n;
L_PTRA    *pat, *pad;

    if (!paa)
        return (L_PTRA *)ERROR_PTR("paa not defined", __func__, NULL);

    pad = ptraCreate(0);
    ptraaGetSize(paa, &n);
    for (i = 0; i < n; i++) {
        pat = ptraaGetPtra(paa, i, L_REMOVE);
        if (!pat) continue;
        ptraJoin(pad, pat);
        ptraDestroy(&pat, FALSE, FALSE);  /* they're all empty */
    }

    return pad;
}

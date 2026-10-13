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
 * \file  ptafunc2.c
 * <pre>
 *
 *      --------------------------------------
 *      This file has these Pta utilities:
 *         - sorting
 *         - ordered set operations
 *         - hash map operations
 *      --------------------------------------
 *
 *      Sorting
 *           PTA        *ptaSort()
 *           l_int32     ptaGetSortIndex()
 *           PTA        *ptaSortByIndex()
 *           PTAA       *ptaaSortByIndex()
 *           l_int32     ptaGetRankValue()
 *           PTA        *ptaSort2d()
 *           l_int32     ptaEqual()
 *
 *      Set operations using aset (rbtree)
 *           L_ASET     *l_asetCreateFromPta()
 *           PTA        *ptaRemoveDupsByAset()
 *           PTA        *ptaUnionByAset()
 *           PTA        *ptaIntersectionByAset()
 *
 *      Hashmap operations
 *          L_HASHMAP   *l_hmapCreateFromPta()
 *          l_int32      ptaRemoveDupsByHmap()
 *          l_int32      ptaUnionByHmap()
 *          l_int32      ptaIntersectionByHmap()
 *
 * We have two implementations of set operations on an array of points:
 *
 *   (1) Using an underlying tree (rbtree)
 *       This uses a good 64 bit hashing function for the key,
 *       that is not expected to have hash collisions (and we do
 *       not test for them).  The tree is built up of the keys,
 *       values, and is traversed looking for the key in O(log n).
 *
 *   (2) Building a hashmap from the keys (hashmap)
 *       This uses a fast 64 bit hashing function for the key, which
 *       is then hashed into a hashtable.  Collisions of hashkeys are
 *       very rare, but the hashtable is designed to allow more than one
 *       hashitem in a table entry.  The hashitems are put in a list at
 *       each hashtable entry, which is traversed looking for the key.
 *
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

/*---------------------------------------------------------------------*
 *                               Sorting                               *
 *---------------------------------------------------------------------*/
/*!
 * \brief   ptaSort()
 *
 * \param[in]    ptas
 * \param[in]    sorttype    L_SORT_BY_X, L_SORT_BY_Y
 * \param[in]    sortorder   L_SORT_INCREASING, L_SORT_DECREASING
 * \param[out]   pnaindex    [optional] index of sorted order into
 *                           original array
 * \return  ptad sorted version of ptas, or NULL on error
 */
PTA *
ptaSort(PTA     *ptas,
        l_int32  sorttype,
        l_int32  sortorder,
        NUMA   **pnaindex)
{
PTA   *ptad;
NUMA  *naindex;

    if (pnaindex) *pnaindex = NULL;
    if (!ptas)
        return (PTA *)ERROR_PTR("ptas not defined", __func__, NULL);
    if (sorttype != L_SORT_BY_X && sorttype != L_SORT_BY_Y)
        return (PTA *)ERROR_PTR("invalid sort type", __func__, NULL);
    if (sortorder != L_SORT_INCREASING && sortorder != L_SORT_DECREASING)
        return (PTA *)ERROR_PTR("invalid sort order", __func__, NULL);

    if (ptaGetSortIndex(ptas, sorttype, sortorder, &naindex) != 0)
        return (PTA *)ERROR_PTR("naindex not made", __func__, NULL);

    ptad = ptaSortByIndex(ptas, naindex);
    if (pnaindex)
        *pnaindex = naindex;
    else
        numaDestroy(&naindex);
    if (!ptad)
        return (PTA *)ERROR_PTR("ptad not made", __func__, NULL);
    return ptad;
}


/*!
 * \brief   ptaGetSortIndex()
 *
 * \param[in]    ptas
 * \param[in]    sorttype    L_SORT_BY_X, L_SORT_BY_Y
 * \param[in]    sortorder   L_SORT_INCREASING, L_SORT_DECREASING
 * \param[out]   pnaindex    index of sorted order into original array
 * \return  0 if OK, 1 on error
 */
l_ok
ptaGetSortIndex(PTA     *ptas,
                l_int32  sorttype,
                l_int32  sortorder,
                NUMA   **pnaindex)
{
l_int32    i, n;
l_float32  x, y;
NUMA      *na, *nai;

    if (!pnaindex)
        return ERROR_INT("&naindex not defined", __func__, 1);
    *pnaindex = NULL;
    if (!ptas)
        return ERROR_INT("ptas not defined", __func__, 1);
    if (sorttype != L_SORT_BY_X && sorttype != L_SORT_BY_Y)
        return ERROR_INT("invalid sort type", __func__, 1);
    if (sortorder != L_SORT_INCREASING && sortorder != L_SORT_DECREASING)
        return ERROR_INT("invalid sort order", __func__, 1);

        /* Build up numa of specific data */
    n = ptaGetCount(ptas);
    if ((na = numaCreate(n)) == NULL)
        return ERROR_INT("na not made", __func__, 1);
    for (i = 0; i < n; i++) {
        ptaGetPt(ptas, i, &x, &y);
        if (sorttype == L_SORT_BY_X)
            numaAddNumber(na, x);
        else
            numaAddNumber(na, y);
    }

        /* Get the sort index for data array */
    nai = numaGetSortIndex(na, sortorder);
    numaDestroy(&na);
    if (!nai)
        return ERROR_INT("naindex not made", __func__, 1);
    *pnaindex = nai;
    return 0;
}


/*!
 * \brief   ptaSortByIndex()
 *
 * \param[in]    ptas
 * \param[in]    naindex    na that maps from the new pta to the input pta
 * \return  ptad sorted, or NULL on  error
 */
PTA *
ptaSortByIndex(PTA   *ptas,
               NUMA  *naindex)
{
l_int32    i, index, n;
l_float32  x, y;
PTA       *ptad;

    if (!ptas)
        return (PTA *)ERROR_PTR("ptas not defined", __func__, NULL);
    if (!naindex)
        return (PTA *)ERROR_PTR("naindex not defined", __func__, NULL);

        /* Build up sorted pta using sort index */
    n = numaGetCount(naindex);
    if ((ptad = ptaCreate(n)) == NULL)
        return (PTA *)ERROR_PTR("ptad not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        numaGetIValue(naindex, i, &index);
        ptaGetPt(ptas, index, &x, &y);
        ptaAddPt(ptad, x, y);
    }

    return ptad;
}


/*!
 * \brief   ptaaSortByIndex()
 *
 * \param[in]    ptaas
 * \param[in]    naindex    na that maps from the new ptaa to the input ptaa
 * \return  ptaad sorted, or NULL on error
 */
PTAA *
ptaaSortByIndex(PTAA  *ptaas,
                NUMA  *naindex)
{
l_int32  i, n, index;
PTA     *pta;
PTAA    *ptaad;

    if (!ptaas)
        return (PTAA *)ERROR_PTR("ptaas not defined", __func__, NULL);
    if (!naindex)
        return (PTAA *)ERROR_PTR("naindex not defined", __func__, NULL);

    n = ptaaGetCount(ptaas);
    if (numaGetCount(naindex) != n)
        return (PTAA *)ERROR_PTR("numa and ptaa sizes differ", __func__, NULL);
    ptaad = ptaaCreate(n);
    for (i = 0; i < n; i++) {
        numaGetIValue(naindex, i, &index);
        pta = ptaaGetPta(ptaas, index, L_COPY);
        ptaaAddPta(ptaad, pta, L_INSERT);
    }

    return ptaad;
}


/*!
 * \brief   ptaGetRankValue()
 *
 * \param[in]    pta
 * \param[in]    fract      use 0.0 for smallest, 1.0 for largest
 * \param[in]    ptasort    [optional] version of %pta sorted by %sorttype
 * \param[in]    sorttype   L_SORT_BY_X, L_SORT_BY_Y
 * \param[out]   pval       rankval: the x or y value at %fract
 * \return  0 if OK, 1 on error
 */
l_ok
ptaGetRankValue(PTA        *pta,
                l_float32   fract,
                PTA        *ptasort,
                l_int32     sorttype,
                l_float32  *pval)
{
l_int32  index, n;
PTA     *ptas;

    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0.0;
    if (!pta)
        return ERROR_INT("pta not defined", __func__, 1);
    if (sorttype != L_SORT_BY_X && sorttype != L_SORT_BY_Y)
        return ERROR_INT("invalid sort type", __func__, 1);
    if (fract < 0.0 || fract > 1.0)
        return ERROR_INT("fract not in [0.0 ... 1.0]", __func__, 1);
    if ((n = ptaGetCount(pta)) == 0)
        return ERROR_INT("pta empty", __func__, 1);

    if (ptasort)
        ptas = ptasort;
    else
        ptas = ptaSort(pta, sorttype, L_SORT_INCREASING, NULL);

    index = (l_int32)(fract * (l_float32)(n - 1) + 0.5);
    if (sorttype == L_SORT_BY_X)
        ptaGetPt(ptas, index, pval, NULL);
    else  /* sort by y */
        ptaGetPt(ptas, index, NULL, pval);

    if (!ptasort) ptaDestroy(&ptas);
    return 0;
}


/*!
 * \brief   ptaSort2d()
 *
 * \param[in]    ptas
 * \return  ptad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Sort increasing by row-major, scanning down from the UL corner,
 *          where for each value of y, order the pts from left to right.
 * </pre>
 */
PTA *
ptaSort2d(PTA  *pta)
{
l_int32    index, i, j, n, nx, ny, start, end;
l_float32  x, y, yp, val;
NUMA      *na1, *na2, *nas, *nax;
PTA       *pta1, *ptad;

    if (!pta)
        return (PTA *)ERROR_PTR("pta not defined", __func__, NULL);

        /* Sort by row-major (y first, then x).  After sort by y,
         * the x values at the same y are not sorted.  */
    pta1 = ptaSort(pta, L_SORT_BY_Y, L_SORT_INCREASING, NULL);

        /* Find start and ending indices with the same y value */
    n = ptaGetCount(pta1);
    na1 = numaCreate(0);  /* holds start index of sequence with same y */
    na2 = numaCreate(0);  /* holds end index of sequence with same y */
    numaAddNumber(na1, 0);
    ptaGetPt(pta1, 0, &x, &yp);
    for (i = 1; i < n; i++) {
        ptaGetPt(pta1, i, &x, &y);
        if (y != yp) {
            numaAddNumber(na1, i);
            numaAddNumber(na2, i - 1);
        }
        yp = y;
    }
    numaAddNumber(na2, n - 1);

        /* Sort by increasing x each set with the same y value */
    ptad = ptaCreate(n);
    ny = numaGetCount(na1);   /* number of distinct y values */
    for (i = 0, index = 0; i < ny; i++) {
        numaGetIValue(na1, i, &start);
        numaGetIValue(na2, i, &end);
        nx = end - start + 1;  /* number of points with current y value */
        if (nx == 1) {
            ptaGetPt(pta1, index++, &x, &y);
            ptaAddPt(ptad, x, y);
        } else {
                /* More than 1 point; extract and sort the x values */
            nax = numaCreate(nx);
            for (j = 0; j < nx; j++) {
                 ptaGetPt(pta1, index + j, &x, &y);
                 numaAddNumber(nax, x);
            }
            nas = numaSort(NULL, nax, L_SORT_INCREASING);
                /* Add the points with x sorted */
            for (j = 0; j < nx; j++) {
                numaGetFValue(nas, j, &val);
                ptaAddPt(ptad, val, y);
            }
            index += nx;
            numaDestroy(&nax);
            numaDestroy(&nas);
        }
    }
    numaDestroy(&na1);
    numaDestroy(&na2);
    ptaDestroy(&pta1);
    return ptad;
}


/*!
 * \brief   ptaEqual()
 *
 * \param[in]    pta1
 * \param[in]    pta2
 * \param[out]   psame  1 if same; 0 if different
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Equality is defined as having the same set of points,
 *          independent of the order in which they are presented.
 * </pre>
 */
l_ok
ptaEqual(PTA      *pta1,
         PTA      *pta2,
         l_int32  *psame)
{
l_int32    i, n1, n2;
l_float32  x1, y1, x2, y2;
PTA       *ptas1, *ptas2;

    if (!psame)
        return ERROR_INT("&same not defined", __func__, 1);
    *psame = 0.0f;
    if (!pta1 || !pta2)
        return ERROR_INT("pta1 and pta2 not both defined", __func__, 1);

    n1 = ptaGetCount(pta1);
    n2 = ptaGetCount(pta2);
    if (n1 != n2) return 0;

        /* 2d sort each and compare */
    ptas1 = ptaSort2d(pta1);
    ptas2 = ptaSort2d(pta2);
    for (i = 0; i < n1; i++) {
        ptaGetPt(ptas1, i, &x1, &y1);
        ptaGetPt(ptas2, i, &x2, &y2);
        if (x1 != x2 || y1 != y2) {
            ptaDestroy(&ptas1);
            ptaDestroy(&ptas2);
            return 0;
        }
    }

    *psame = 1;
    ptaDestroy(&ptas1);
    ptaDestroy(&ptas2);
    return 0;
}


/*---------------------------------------------------------------------*
 *                   Set operations using aset (rbtree)                *
 *---------------------------------------------------------------------*/
/*!
 * \brief   l_asetCreateFromPta()
 *
 * \param[in]    pta
 * \return  set using a 64-bit hash of (x,y) as the key
 */
L_ASET *
l_asetCreateFromPta(PTA  *pta)
{
l_int32   i, n, x, y;
l_uint64  hash;
L_ASET   *set;
RB_TYPE   key;

    if (!pta)
        return (L_ASET *)ERROR_PTR("pta not defined", __func__, NULL);

    set = l_asetCreate(L_UINT_TYPE);
    n = ptaGetCount(pta);
    for (i = 0; i < n; i++) {
        ptaGetIPt(pta, i, &x, &y);
        l_hashPtToUint64(x, y, &hash);
        key.utype = hash;
        l_asetInsert(set, key);
    }

    return set;
}


/*!
 * \brief   ptaRemoveDupsByAset()
 *
 * \param[in]    ptas     assumed to be integer values
 * \param[out]   pptad    assumed to be integer values
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is slower than ptaRemoveDupsByHmap(), mostly because
 *          of the nlogn sort to build up the rbtree.  Do not use for
 *          large numbers of points (say, > 100K).
 * </pre>
 */
l_ok
ptaRemoveDupsByAset(PTA   *ptas,
                    PTA  **pptad)
{
l_int32   i, n, x, y;
PTA      *ptad;
l_uint64  hash;
L_ASET   *set;
RB_TYPE   key;

    if (!pptad)
        return ERROR_INT("&ptad not defined", __func__, 1);
    *pptad = NULL;
    if (!ptas)
        return ERROR_INT("ptas not defined", __func__, 1);

    set = l_asetCreate(L_UINT_TYPE);
    n = ptaGetCount(ptas);
    ptad = ptaCreate(n);
    *pptad = ptad;
    for (i = 0; i < n; i++) {
        ptaGetIPt(ptas, i, &x, &y);
        l_hashPtToUint64(x, y, &hash);
        key.utype = hash;
        if (!l_asetFind(set, key)) {
            ptaAddPt(ptad, x, y);
            l_asetInsert(set, key);
        }
    }

    l_asetDestroy(&set);
    return 0;
}


/*!
 * \brief   ptaUnionByAset()
 *
 * \param[in]    pta1
 * \param[in]    pta2
 * \param[out]   pptad     union of the two point arrays
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See sarrayRemoveDupsByAset() for the approach.
 *      (2) The key is a 64-bit hash from the (x,y) pair.
 *      (3) This is slower than ptaUnionByHmap(), mostly because of the
 *          nlogn sort to build up the rbtree.  Do not use for large
 *          numbers of points (say, > 100K).
 *      (4) The *Aset() functions use the sorted l_Aset, which is just
 *          an rbtree in disguise.
 * </pre>
 */
l_ok
ptaUnionByAset(PTA   *pta1,
               PTA   *pta2,
               PTA  **pptad)
{
PTA  *pta3;

    if (!pptad)
        return ERROR_INT("&ptad not defined", __func__, 1);
    *pptad = NULL;
    if (!pta1)
        return ERROR_INT("pta1 not defined", __func__, 1);
    if (!pta2)
        return ERROR_INT("pta2 not defined", __func__, 1);

        /* Join */
    pta3 = ptaCopy(pta1);
    ptaJoin(pta3, pta2, 0, -1);

        /* Eliminate duplicates */
    ptaRemoveDupsByAset(pta3, pptad);
    ptaDestroy(&pta3);
    return 0;
}


/*!
 * \brief   ptaIntersectionByAset()
 *
 * \param[in]    pta1
 * \param[in]    pta2
 * \param[out]   pptad       intersection of the two point arrays
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See sarrayIntersectionByAset() for the approach.
 *      (2) The key is a 64-bit hash from the (x,y) pair.
 *      (3) This is slower than ptaIntersectionByHmap(), mostly because
 *          of the nlogn sort to build up the rbtree.  Do not use for
 *          large numbers of points (say, > 100K).
 * </pre>
 */
l_ok
ptaIntersectionByAset(PTA   *pta1,
                      PTA   *pta2,
                      PTA  **pptad)
{
l_int32   n1, n2, i, n, x, y;
l_uint64  hash;
L_ASET   *set1, *set2;
RB_TYPE   key;
PTA      *pta_small, *pta_big, *ptad;

    if (!pptad)
        return ERROR_INT("&ptad not defined", __func__, 1);
    *pptad = NULL;
    if (!pta1)
        return ERROR_INT("pta1 not defined", __func__, 1);
    if (!pta2)
        return ERROR_INT("pta2 not defined", __func__, 1);

        /* Put the elements of the biggest array into a set */
    n1 = ptaGetCount(pta1);
    n2 = ptaGetCount(pta2);
    pta_small = (n1 < n2) ? pta1 : pta2;   /* do not destroy pta_small */
    pta_big = (n1 < n2) ? pta2 : pta1;   /* do not destroy pta_big */
    set1 = l_asetCreateFromPta(pta_big);

        /* Build up the intersection of points */
    ptad = ptaCreate(0);
    *pptad = ptad;
    n = ptaGetCount(pta_small);
    set2 = l_asetCreate(L_UINT_TYPE);
    for (i = 0; i < n; i++) {
        ptaGetIPt(pta_small, i, &x, &y);
        l_hashPtToUint64(x, y, &hash);
        key.utype = hash;
        if (l_asetFind(set1, key) && !l_asetFind(set2, key)) {
            ptaAddPt(ptad, x, y);
            l_asetInsert(set2, key);
        }
    }

    l_asetDestroy(&set1);
    l_asetDestroy(&set2);
    return 0;
}


/*--------------------------------------------------------------------------*
 *                            Hashmap operations                            *
 *--------------------------------------------------------------------------*/
/*!
 * \brief  l_hmapCreateFromPta()
 *
 * \param[in]   pta     input pta
 * \return      hmap    hashmap, or NULL on error
 *
 * <pre>
 *  Notes:
 *       (1) The indices into %pta are stored in the val field of the hashitems.
 *           This is necessary so that %hmap and %pta can be used together.
 * </pre>
 */
L_HASHMAP *
l_hmapCreateFromPta(PTA  *pta)
{
l_int32      i, n, x, y;
l_uint64     key;
L_HASHMAP   *hmap;

    if (!pta)
        return (L_HASHMAP *)ERROR_PTR("pta not defined", __func__, NULL);

    n = ptaGetCount(pta);
    if ((hmap = l_hmapCreate(0.51 * n, 2)) == NULL)
        return (L_HASHMAP *)ERROR_PTR("hmap not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        ptaGetIPt(pta, i, &x, &y);
        l_hashPtToUint64(x, y, &key);
        l_hmapLookup(hmap, key, i, L_HMAP_CREATE);
    }
    return hmap;
}


/*!
 * \brief  ptaRemoveDupsByHmap()
 *
 * \param[in]   ptas
 * \param[out]  pptad    set of unique values
 * \param[out]  phmap    [optional] hashmap used for lookup
 * \return  0 if OK; 1 on error
 *
 * <pre>
 *  Notes:
 *       (1) Generates a set of (unique) points from %ptas.
 * </pre>
 */
l_ok
ptaRemoveDupsByHmap(PTA         *ptas,
                    PTA        **pptad,
                    L_HASHMAP  **phmap)
{
l_int32      i, x, y, tabsize;
PTA         *ptad;
L_HASHITEM  *hitem;
L_HASHMAP   *hmap;

    if (phmap) *phmap = NULL;
    if (!pptad)
        return ERROR_INT("&ptad not defined", __func__, 1);
    *pptad = NULL;
    if (!ptas)
        return ERROR_INT("ptas not defined", __func__, 1);

        /* Traverse the hashtable lists */
    if ((hmap = l_hmapCreateFromPta(ptas)) == NULL)
        return ERROR_INT("hmap not made", __func__, 1);
    ptad = ptaCreate(0);
    *pptad = ptad;
    tabsize = hmap->tabsize;
    for (i = 0; i < tabsize; i++) {
        hitem = hmap->hashtab[i];
        while (hitem) {
            ptaGetIPt(ptas, hitem->val, &x, &y);
            ptaAddPt(ptad, x, y);
            hitem = hitem->next;
        }
    }

    if (phmap)
        *phmap = hmap;
    else
        l_hmapDestroy(&hmap);
    return 0;
}


/*!
 * \brief  ptaUnionByHmap()
 *
 * \param[in]   pta1
 * \param[in]   pta2
 * \param[out]  pptad     union of the two point arrays
 * \return  0 if OK; 1 on error
 *
 * <pre>
 *  Notes:
 *       (1) Make pta with points found in either of the input arrays.
 * </pre>
 */
l_ok
ptaUnionByHmap(PTA   *pta1,
               PTA   *pta2,
               PTA  **pptad)
{
PTA  *pta3;

    if (!pptad)
        return ERROR_INT("&ptad not defined", __func__, 1);
    *pptad = NULL;
    if (!pta1)
        return ERROR_INT("pta1 not defined", __func__, 1);
    if (!pta2)
        return ERROR_INT("pta2 not defined", __func__, 1);

    pta3 = ptaCopy(pta1);
    if (ptaJoin(pta3, pta2, 0, -1) == 1) {
        ptaDestroy(&pta3);
        return ERROR_INT("pta join failed", __func__, 1);
    }
    ptaRemoveDupsByHmap(pta3, pptad, NULL);
    ptaDestroy(&pta3);
    return 0;
}


/*!
 * \brief  ptaIntersectionByHmap()
 *
 * \param[in]    pta1
 * \param[in]    pta2
 * \param[out]   pptad     intersection of the two point arrays
 * \return  0 if OK; 1 on error
 *
 * <pre>
 *  Notes:
 *       (1) Make pta with pts common to both input arrays.
 * </pre>
 */
l_ok
ptaIntersectionByHmap(PTA   *pta1,
                      PTA   *pta2,
                      PTA  **pptad)
{
l_int32      i, n1, n2, n, x, y;
l_uint64     key;
PTA         *pta_small, *pta_big, *ptad;
L_HASHITEM  *hitem;
L_HASHMAP   *hmap;

    if (!pptad)
        return ERROR_INT("&ptad not defined", __func__, 1);
    *pptad = NULL;
    if (!pta1)
        return ERROR_INT("pta1 not defined", __func__, 1);
    if (!pta2)
        return ERROR_INT("pta2 not defined", __func__, 1);

        /* Make a hashmap for the elements of the biggest array */
    n1 = ptaGetCount(pta1);
    n2 = ptaGetCount(pta2);
    pta_small = (n1 < n2) ? pta1 : pta2;   /* do not destroy pta_small */
    pta_big = (n1 < n2) ? pta2 : pta1;   /* do not destroy pta_big */
    if ((hmap = l_hmapCreateFromPta(pta_big)) == NULL)
        return ERROR_INT("hmap not made", __func__, 1);

        /* Go through the smallest array, doing a lookup of its (x,y) into
         * the big array hashmap.  If an hitem is returned, check the count.
         * If the count is 0, ignore; otherwise, add the point to the
         * output ptad and set the count in the hitem to 0, indicating
         * that the point has already been added. */
    ptad = ptaCreate(0);
    *pptad = ptad;
    n = ptaGetCount(pta_small);
    for (i = 0; i < n; i++) {
        ptaGetIPt(pta_small, i, &x, &y);
        l_hashPtToUint64(x, y, &key);
        hitem = l_hmapLookup(hmap, key, i, L_HMAP_CHECK);
        if (!hitem || hitem->count == 0)
            continue;
        ptaAddPt(ptad, x, y);
        hitem->count = 0;
    }
    l_hmapDestroy(&hmap);
    return 0;
}



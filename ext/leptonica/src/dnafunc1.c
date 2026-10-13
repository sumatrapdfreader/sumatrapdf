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
 * \file  dnafunc1.c
 * <pre>
 *
 *      Rearrangements
 *          l_int32     *l_dnaJoin()
 *          l_int32     *l_dnaaFlattenToDna()
 *          L_DNA       *l_dnaSelectRange()
 *
 *      Conversion between numa and dna
 *          NUMA        *l_dnaConvertToNuma()
 *          L_DNA       *numaConvertToDna()
 *
 *      Conversion from pix data to dna
 *          L_DNA       *pixConvertDataToDna()
 *
 *      Set operations using aset (rbtree)
 *          L_ASET      *l_asetCreateFromDna()
 *          L_DNA       *l_dnaRemoveDupsByAset()
 *          L_DNA       *l_dnaUnionByAset()
 *          L_DNA       *l_dnaIntersectionByAset()
 *
 *      Hashmap operations
 *          L_HASHMAP   *l_hmapCreateFromDna()
 *          l_int32      l_dnaRemoveDupsByHmap()
 *          l_int32      l_dnaUnionByHmap()
 *          l_int32      l_dnaIntersectionByHmap()
 *          l_int32      l_dnaMakeHistoByHmap()
 *
 *      Miscellaneous operations
 *          L_DNA       *l_dnaDiffAdjValues()
 *
 *
 * We have two implementations of set operations on an array of doubles:
 *
 *   (1) Using an underlying tree (rbtree)
 *       The key for each float64 value is the value itself.
 *       No collisions can occur.  The tree is sorted by the keys.
 *       Lookup is done in O(log n) by traversing from the root,
 *       looking for the key.
 *
 *   (2) Building a hashmap from the keys (hashmap)
 *       The keys are made from each float64 by casting into a uint64.
 *       The key is then hashed into a hashtable.  Collisions of hashkeys are
 *       very rare, and the hashtable is designed to allow more than one
 *       hashitem in a table entry.  The hashitems are put in a list at
 *       each hashtable entry, which is traversed looking for the key.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"
#include "array_internal.h"


/*----------------------------------------------------------------------*
 *                            Rearrangements                            *
 *----------------------------------------------------------------------*/
/*!
 * \brief   l_dnaJoin()
 *
 * \param[in]    dad       dest dna; add to this one
 * \param[in]    das       [optional] source dna; add from this one
 * \param[in]    istart    starting index in das
 * \param[in]    iend      ending index in das; use -1 to cat all
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) istart < 0 is taken to mean 'read from the start' (istart = 0)
 *      (2) iend < 0 means 'read to the end'
 *      (3) if das == NULL, this is a no-op
 * </pre>
 */
l_ok
l_dnaJoin(L_DNA   *dad,
          L_DNA   *das,
          l_int32  istart,
          l_int32  iend)
{
l_int32    n, i;
l_float64  val;

    if (!dad)
        return ERROR_INT("dad not defined", __func__, 1);
    if (!das)
        return 0;

    if (istart < 0)
        istart = 0;
    n = l_dnaGetCount(das);
    if (iend < 0 || iend >= n)
        iend = n - 1;
    if (istart > iend)
        return ERROR_INT("istart > iend; nothing to add", __func__, 1);

    for (i = istart; i <= iend; i++) {
        l_dnaGetDValue(das, i, &val);
        if (l_dnaAddNumber(dad, val) == 1) {
            L_ERROR("failed to add double at i = %d\n", __func__, i);
            return 1;
        }

    }
    return 0;
}


/*!
 * \brief   l_dnaaFlattenToDna()
 *
 * \param[in]    daa
 * \return  dad, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This 'flattens' the dnaa to a dna, by joining successively
 *          each dna in the dnaa.
 *      (2) It leaves the input dnaa unchanged.
 * </pre>
 */
L_DNA *
l_dnaaFlattenToDna(L_DNAA  *daa)
{
l_int32  i, nalloc;
L_DNA   *da, *dad;
L_DNA  **array;

    if (!daa)
        return (L_DNA *)ERROR_PTR("daa not defined", __func__, NULL);

    nalloc = daa->nalloc;
    array = daa->dna;
    dad = l_dnaCreate(0);
    for (i = 0; i < nalloc; i++) {
        da = array[i];
        if (!da) continue;
        l_dnaJoin(dad, da, 0, -1);
    }

    return dad;
}


/*!
 * \brief   l_dnaSelectRange()
 *
 * \param[in]    das
 * \param[in]    first    use 0 to select from the beginning
 * \param[in]    last     use -1 to select to the end
 * \return  dad, or NULL on error
 */
L_DNA *
l_dnaSelectRange(L_DNA   *das,
                 l_int32  first,
                 l_int32  last)
{
l_int32    n, i;
l_float64  dval;
L_DNA     *dad;

    if (!das)
        return (L_DNA *)ERROR_PTR("das not defined", __func__, NULL);
    if ((n = l_dnaGetCount(das)) == 0) {
        L_WARNING("das is empty\n", __func__);
        return l_dnaCopy(das);
    }
    first = L_MAX(0, first);
    if (last < 0) last = n - 1;
    if (first >= n)
        return (L_DNA *)ERROR_PTR("invalid first", __func__, NULL);
    if (last >= n) {
        L_WARNING("last = %d is beyond max index = %d; adjusting\n",
                  __func__, last, n - 1);
        last = n - 1;
    }
    if (first > last)
        return (L_DNA *)ERROR_PTR("first > last", __func__, NULL);

    dad = l_dnaCreate(last - first + 1);
    for (i = first; i <= last; i++) {
        l_dnaGetDValue(das, i, &dval);
        l_dnaAddNumber(dad, dval);
    }
    return dad;
}


/*----------------------------------------------------------------------*
 *                   Conversion between numa and dna                    *
 *----------------------------------------------------------------------*/
/*!
 * \brief   l_dnaConvertToNuma()
 *
 * \param[in]    da
 * \return  na, or NULL on error
 */
NUMA *
l_dnaConvertToNuma(L_DNA  *da)
{
l_int32    i, n;
l_float64  val;
NUMA      *na;

    if (!da)
        return (NUMA *)ERROR_PTR("da not defined", __func__, NULL);

    n = l_dnaGetCount(da);
    na = numaCreate(n);
    for (i = 0; i < n; i++) {
        l_dnaGetDValue(da, i, &val);
        numaAddNumber(na, val);
    }
    return na;
}


/*!
 * \brief   numaConvertToDna
 *
 * \param[in]    na
 * \return  da, or NULL on error
 */
L_DNA *
numaConvertToDna(NUMA  *na)
{
l_int32    i, n;
l_float32  val;
L_DNA     *da;

    if (!na)
        return (L_DNA *)ERROR_PTR("na not defined", __func__, NULL);

    n = numaGetCount(na);
    da = l_dnaCreate(n);
    for (i = 0; i < n; i++) {
        numaGetFValue(na, i, &val);
        l_dnaAddNumber(da, val);
    }
    return da;
}


/*----------------------------------------------------------------------*
 *                    Conversion from pix data to dna                   *
 *----------------------------------------------------------------------*/
/*!
 * \brief   pixConvertDataToDna()
 *
 * \param[in]    pix      32 bpp RGB(A)
 * \return  da, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This writes the RGBA pixel values into the dna, in row-major order.
 * </pre>
 */
L_DNA *
pixConvertDataToDna(PIX  *pix)
{
l_int32    i, j, w, h, wpl;
l_uint32  *data, *line;
L_DNA     *da;

    if (!pix)
        return (L_DNA *)ERROR_PTR("pix not defined", __func__, NULL);
    if (pixGetDepth(pix) != 32)
        return (L_DNA *)ERROR_PTR("pix not 32 bpp", __func__, NULL);

    pixGetDimensions(pix, &w, &h, NULL);
    data = pixGetData(pix);
    wpl = pixGetWpl(pix);
    da = l_dnaCreate(w * h);
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        for (j = 0; j < w; j++)
            l_dnaAddNumber(da, (l_float64)line[j]);
    }
    return da;
}


/*----------------------------------------------------------------------*
 *                   Set operations using aset (rbtree)                 *
 *----------------------------------------------------------------------*/
/*!
 * \brief   l_asetCreateFromDna()
 *
 * \param[in]    da    source dna
 * \return  set using the doubles in %da as keys
 */
L_ASET *
l_asetCreateFromDna(L_DNA  *da)
{
l_int32    i, n;
l_float64  val;
L_ASET    *set;
RB_TYPE    key;

    if (!da)
        return (L_ASET *)ERROR_PTR("da not defined", __func__, NULL);

    set = l_asetCreate(L_FLOAT_TYPE);
    n = l_dnaGetCount(da);
    for (i = 0; i < n; i++) {
        l_dnaGetDValue(da, i, &val);
        key.ftype = val;
        l_asetInsert(set, key);
    }

    return set;
}


/*!
 * \brief   l_dnaRemoveDupsByAset()
 *
 * \param[in]    das
 * \param[out]   pdad     with duplicated removed
 * \return  0 if OK; 1 on error
 */
l_ok
l_dnaRemoveDupsByAset(L_DNA   *das,
                      L_DNA  **pdad)
{
l_int32    i, n;
l_float64  val;
L_DNA     *dad;
L_ASET    *set;
RB_TYPE    key;

    if (!pdad)
        return ERROR_INT("&dad not defined", __func__, 1);
    *pdad = NULL;
    if (!das)
        return ERROR_INT("das not defined", __func__, 1);

    set = l_asetCreate(L_FLOAT_TYPE);
    dad = l_dnaCreate(0);
    *pdad = dad;
    n = l_dnaGetCount(das);
    for (i = 0; i < n; i++) {
        l_dnaGetDValue(das, i, &val);
        key.ftype = val;
        if (!l_asetFind(set, key)) {
            l_dnaAddNumber(dad, val);
            l_asetInsert(set, key);
        }
    }

    l_asetDestroy(&set);
    return 0;
}


/*!
 * \brief   l_dnaUnionByAset()
 *
 * \param[in]    da1
 * \param[in]    da2
 * \param[out]   pdad       union of the two arrays
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See sarrayUnionByAset() for the approach.
 *      (2) Here, the key in building the sorted tree is the number itself.
 *      (3) Operations using an underlying tree are O(nlogn), which is
 *          typically less efficient than hashing, which is O(n).
 * </pre>
 */
l_ok
l_dnaUnionByAset(L_DNA   *da1,
                 L_DNA   *da2,
                 L_DNA  **pdad)
{
L_DNA  *da3;

    if (!pdad)
        return ERROR_INT("&dad not defined", __func__, 1);
    if (!da1)
        return ERROR_INT("da1 not defined", __func__, 1);
    if (!da2)
        return ERROR_INT("da2 not defined", __func__, 1);

        /* Join */
    da3 = l_dnaCopy(da1);
    if (l_dnaJoin(da3, da2, 0, -1) == 1) {
        l_dnaDestroy(&da3);
        return ERROR_INT("join failed for da3", __func__, 1);
    }

        /* Eliminate duplicates */
    l_dnaRemoveDupsByAset(da3, pdad);
    l_dnaDestroy(&da3);
    return 0;
}


/*!
 * \brief   l_dnaIntersectionByAset()
 *
 * \param[in]    da1
 * \param[in]    da2
 * \param[out]   pdad      intersection of the two arrays
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See sarrayIntersection() for the approach.
 *      (2) Here, the key in building the sorted tree is the number itself.
 *      (3) Operations using an underlying tree are O(nlogn), which is
 *          typically less efficient than hashing, which is O(n).
 * </pre>
 */
l_ok
l_dnaIntersectionByAset(L_DNA   *da1,
                        L_DNA   *da2,
                        L_DNA  **pdad)
{
l_int32    n1, n2, i, n;
l_float64  val;
L_ASET    *set1, *set2;
RB_TYPE    key;
L_DNA     *da_small, *da_big, *dad;

    if (!pdad)
        return ERROR_INT("&dad not defined", __func__, 1);
    *pdad = NULL;
    if (!da1)
        return ERROR_INT("&da1 not defined", __func__, 1);
    if (!da2)
        return ERROR_INT("&da2 not defined", __func__, 1);

        /* Put the elements of the largest array into a set */
    n1 = l_dnaGetCount(da1);
    n2 = l_dnaGetCount(da2);
    da_small = (n1 < n2) ? da1 : da2;   /* do not destroy da_small */
    da_big = (n1 < n2) ? da2 : da1;   /* do not destroy da_big */
    set1 = l_asetCreateFromDna(da_big);

        /* Build up the intersection of doubles */
    dad = l_dnaCreate(0);
    *pdad = dad;
    n = l_dnaGetCount(da_small);
    set2 = l_asetCreate(L_FLOAT_TYPE);
    for (i = 0; i < n; i++) {
        l_dnaGetDValue(da_small, i, &val);
        key.ftype = val;
        if (l_asetFind(set1, key) && !l_asetFind(set2, key)) {
            l_dnaAddNumber(dad, val);
            l_asetInsert(set2, key);
        }
    }

    l_asetDestroy(&set1);
    l_asetDestroy(&set2);
    return 0;
}


/*--------------------------------------------------------------------------*
 *                           Hashmap operations                             *
 *--------------------------------------------------------------------------*/
/*!
 * \brief  l_hmapCreateFromDna()
 *
 * \param[in]   da     input dna
 * \return      hmap   hashmap, or NULL on error
 *
 * <pre>
 *  Notes:
 *       (1) Derive the hash keys from the values in %da.
 *       (2) The indices into %da are stored in the val field of the hashitems.
 *           This is necessary so that %hmap and %da can be used together.
 * </pre>
 */
L_HASHMAP *
l_hmapCreateFromDna(L_DNA  *da)
{
l_int32      i, n;
l_uint64     key;
l_float64    dval;
L_HASHMAP   *hmap;

    if (!da)
        return (L_HASHMAP *)ERROR_PTR("da not defined", __func__, NULL);

    n = l_dnaGetCount(da);
    hmap = l_hmapCreate(0, 0);
    for (i = 0; i < n; i++) {
        l_dnaGetDValue(da, i, &dval);
        l_hashFloat64ToUint64(dval, &key);
        l_hmapLookup(hmap, key, i, L_HMAP_CREATE);
    }
    return hmap;
}


/*!
 * \brief  l_dnaRemoveDupsByHmap()
 *
 * \param[in]   das
 * \param[out]  pdad    hash set of unique values
 * \param[out]  phmap   [optional] hashmap used for lookup
 * \return  0 if OK; 1 on error
 *
 * <pre>
 *  Notes:
 *       (1) Generates the set of (unique) values from %das.
 *       (2) The values in the hashitems are indices into %das.
 * </pre>
 */
l_ok
l_dnaRemoveDupsByHmap(L_DNA       *das,
                      L_DNA      **pdad,
                      L_HASHMAP  **phmap)
{
l_int32      i, tabsize;
l_float64    dval;
L_DNA       *dad;
L_HASHITEM  *hitem;
L_HASHMAP   *hmap;

    if (phmap) *phmap = NULL;
    if (!pdad)
        return ERROR_INT("&dad not defined", __func__, 1);
    *pdad = NULL;
    if (!das)
        return ERROR_INT("das not defined", __func__, 1);

        /* Traverse the hashtable lists */
    if ((hmap = l_hmapCreateFromDna(das)) == NULL)
        return ERROR_INT("hmap not made", __func__, 1);
    dad = l_dnaCreate(0);
    *pdad = dad;
    tabsize = hmap->tabsize;
    for (i = 0; i < tabsize; i++) {
        hitem = hmap->hashtab[i];
        while (hitem) {
            l_dnaGetDValue(das, hitem->val, &dval);
            l_dnaAddNumber(dad, dval);
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
 * \brief  l_dnaUnionByHmap()
 *
 * \param[in]   da1
 * \param[in]   da2
 * \param[out]  pdad     union of the array values
 * \return  0 if OK; 1 on error
 *
 * <pre>
 *  Notes:
 *       (1) Make dna with numbers found in either of the input arrays.
 * </pre>
 */
l_ok
l_dnaUnionByHmap(L_DNA   *da1,
                 L_DNA   *da2,
                 L_DNA  **pdad)
{
L_DNA  *da3;

    if (!pdad)
        return ERROR_INT("&dad not defined", __func__, 1);
    *pdad = NULL;
    if (!da1)
        return ERROR_INT("da1 not defined", __func__, 1);
    if (!da2)
        return ERROR_INT("da2 not defined", __func__, 1);

    da3 = l_dnaCopy(da1);
    if (l_dnaJoin(da3, da2, 0, -1) == 1) {
        l_dnaDestroy(&da3);
        return ERROR_INT("da3 join failed", __func__, 1);
    }
    l_dnaRemoveDupsByHmap(da3, pdad, NULL);
    l_dnaDestroy(&da3);
    return 0;
}


/*!
 * \brief  l_dnaIntersectionByHmap()
 *
 * \param[in]    da1
 * \param[in]    da2
 * \param[out]   pdad     intersection of the array values
 * \return  0 if OK; 1 on error
 *
 * <pre>
 *  Notes:
 *       (1) Make dna with numbers common to both input arrays.
 *       (2) Use the values in the dna as the hash keys.
 * </pre>
 */
l_ok
l_dnaIntersectionByHmap(L_DNA   *da1,
                        L_DNA   *da2,
                        L_DNA  **pdad)
{
l_int32      i, n1, n2, n;
l_uint64     key;
l_float64    dval;
L_DNA       *da_small, *da_big, *dad;
L_HASHITEM  *hitem;
L_HASHMAP   *hmap;

    if (!pdad)
        return ERROR_INT("&dad not defined", __func__, 1);
    *pdad = NULL;
    if (!da1)
        return ERROR_INT("da1 not defined", __func__, 1);
    if (!da2)
        return ERROR_INT("da2 not defined", __func__, 1);

        /* Make a hashmap for the elements of the biggest array */
    n1 = l_dnaGetCount(da1);
    n2 = l_dnaGetCount(da2);
    da_small = (n1 < n2) ? da1 : da2;   /* do not destroy da_small */
    da_big = (n1 < n2) ? da2 : da1;   /* do not destroy da_big */
    if ((hmap = l_hmapCreateFromDna(da_big)) == NULL)
        return ERROR_INT("hmap not made", __func__, 1);

        /* Go through the smallest array, doing a lookup of its dval into
         * the big array hashmap.  If an hitem is returned, check the count.
         * If the count is 0, ignore; otherwise, add the dval to the
         * output dad and set the count in the hitem to 0, indicating
         * that the dval has already been added. */
    dad = l_dnaCreate(0);
    *pdad = dad;
    n = l_dnaGetCount(da_small);
    for (i = 0; i < n; i++) {
        l_dnaGetDValue(da_small, i, &dval);
        l_hashFloat64ToUint64(dval, &key);
        hitem = l_hmapLookup(hmap, key, i, L_HMAP_CHECK);
        if (!hitem || hitem->count == 0)
            continue;
        l_dnaAddNumber(dad, dval);
        hitem->count = 0;
    }
    l_hmapDestroy(&hmap);
    return 0;
}


/*!
 * \brief  l_dnaMakeHistoByHmap()
 *
 * \param[in]   das
 * \param[out]  pdav    array (set) of unique values
 * \param[out]  pdac    array of counts, aligned with the array of values
 * \return  0 if OK; 1 on error
 *
 * <pre>
 *  Notes:
 *       (1) Generates a histogram represented by two aligned arrays:
 *           value and count.
 * </pre>
 */
l_ok
l_dnaMakeHistoByHmap(L_DNA   *das,
                     L_DNA  **pdav,
                     L_DNA  **pdac)
{
l_int32      i, tabsize;
l_float64    dval;
L_DNA       *dac, *dav;
L_HASHITEM  *hitem;
L_HASHMAP   *hmap;

    if (pdav) *pdav = NULL;
    if (pdac) *pdac = NULL;
    if (!das)
        return ERROR_INT("das not defined", __func__, 1);
    if (!pdav)
        return ERROR_INT("&dav not defined", __func__, 1);
    if (!pdac)
        return ERROR_INT("&dac not defined", __func__, 1);

        /* Traverse the hashtable lists */
    if ((hmap = l_hmapCreateFromDna(das)) == NULL)
        return ERROR_INT("hmap not made", __func__, 1);
    dav = l_dnaCreate(0);
    *pdav = dav;
    dac = l_dnaCreate(0);
    *pdac = dac;
    tabsize = hmap->tabsize;
    for (i = 0; i < tabsize; i++) {
        hitem = hmap->hashtab[i];
        while (hitem) {
            l_dnaGetDValue(das, hitem->val, &dval);
            l_dnaAddNumber(dav, dval);
            l_dnaAddNumber(dac, hitem->count);
            hitem = hitem->next;
        }
    }

    l_hmapDestroy(&hmap);
    return 0;
}


/*----------------------------------------------------------------------*
 *                       Miscellaneous operations                       *
 *----------------------------------------------------------------------*/
/*!
 * \brief   l_dnaDiffAdjValues()
 *
 * \param[in]    das    input l_dna
 * \return  dad of difference values val[i+1] - val[i],
 *                   or NULL on error
 */
L_DNA *
l_dnaDiffAdjValues(L_DNA  *das)
{
l_int32  i, n, prev, cur;
L_DNA   *dad;

    if (!das)
        return (L_DNA *)ERROR_PTR("das not defined", __func__, NULL);
    n = l_dnaGetCount(das);
    dad = l_dnaCreate(n - 1);
    prev = 0;
    for (i = 1; i < n; i++) {
        l_dnaGetIValue(das, i, &cur);
        l_dnaAddNumber(dad, cur - prev);
        prev = cur;
    }
    return dad;
}


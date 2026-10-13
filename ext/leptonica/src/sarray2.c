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
 * \file  sarray2.c
 * <pre>
 *
 *      Sort
 *          SARRAY     *sarraySort()
 *          SARRAY     *sarraySortByIndex()
 *          l_int32     stringCompareLexical()
 *
 *      Set operations using aset (rbtree)
 *          L_ASET     *l_asetCreateFromSarray()
 *          l_int32     sarrayRemoveDupsByAset()
 *          l_int32     sarrayUnionByAset()
 *          l_int32     sarrayIntersectionByAset()
 *
 *      Hashmap operations
 *          L_HASHMAP  *l_hmapCreateFromSarray()
 *          l_int32     sarrayRemoveDupsByHmap()
 *          l_int32     sarrayUnionByHmap()
 *          l_int32     sarrayIntersectionByHmap()
 *
 *      Miscellaneous operations
 *          SARRAY     *sarrayGenerateIntegers()
 *          l_int32     sarrayLookupCSKV()
 *
 *
 * We have two implementations of set operations on an array of strings:
 *
 *   (1) Using an underlying tree (rbtree).
 *       This uses a good 64 bit hashing function for the key,
 *       that is not expected to have hash collisions (and we do
 *       not test for them).  The tree is built up of the hash
 *       values, and if the hash is found in the tree, it is
 *       assumed that the string has already been found.
 *
 *   (2) Building a hashmap from the keys (hashmap).
 *       This uses a fast 64 bit hashing function for the key, which
 *       is then hashed into a hashtable.  Collisions of hashkeys are
 *       very rare, but the hashtable is designed to allow more than one
 *       hashitem in a table entry.  The hashitems are put in a list at
 *       each hashtable entry, which is traversed looking for the key.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"
#include "array_internal.h"

/*----------------------------------------------------------------------*
 *                                   Sort                               *
 *----------------------------------------------------------------------*/
/*!
 * \brief   sarraySort()
 *
 * \param[in]    saout       output sarray; can be NULL or equal to sain
 * \param[in]    sain        input sarray
 * \param[in]    sortorder   L_SORT_INCREASING or L_SORT_DECREASING
 * \return  saout output sarray, sorted by ascii value, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Set saout = sain for in-place; otherwise, set naout = NULL.
 *      (2) Shell sort, modified from K&R, 2nd edition, p.62.
 *          Slow but simple O(n logn) sort.
 * </pre>
 */
SARRAY *
sarraySort(SARRAY  *saout,
           SARRAY  *sain,
           l_int32  sortorder)
{
char   **array;
char    *tmp;
l_int32  n, i, j, gap;

    if (!sain)
        return (SARRAY *)ERROR_PTR("sain not defined", __func__, NULL);

        /* Make saout if necessary; otherwise do in-place */
    if (!saout)
        saout = sarrayCopy(sain);
    else if (sain != saout)
        return (SARRAY *)ERROR_PTR("invalid: not in-place", __func__, NULL);
    array = saout->array;  /* operate directly on the array */
    n = sarrayGetCount(saout);

        /* Shell sort */
    for (gap = n/2; gap > 0; gap = gap / 2) {
        for (i = gap; i < n; i++) {
            for (j = i - gap; j >= 0; j -= gap) {
                if ((sortorder == L_SORT_INCREASING &&
                     stringCompareLexical(array[j], array[j + gap])) ||
                    (sortorder == L_SORT_DECREASING &&
                     stringCompareLexical(array[j + gap], array[j])))
                {
                    tmp = array[j];
                    array[j] = array[j + gap];
                    array[j + gap] = tmp;
                }
            }
        }
    }

    return saout;
}


/*!
 * \brief   sarraySortByIndex()
 *
 * \param[in]    sain
 * \param[in]    naindex   na that maps from the new sarray to the input sarray
 * \return  saout sorted, or NULL on error
 */
SARRAY *
sarraySortByIndex(SARRAY  *sain,
                  NUMA    *naindex)
{
char    *str;
l_int32  i, n, index;
SARRAY  *saout;

    if (!sain)
        return (SARRAY *)ERROR_PTR("sain not defined", __func__, NULL);
    if (!naindex)
        return (SARRAY *)ERROR_PTR("naindex not defined", __func__, NULL);

    n = sarrayGetCount(sain);
    saout = sarrayCreate(n);
    for (i = 0; i < n; i++) {
        numaGetIValue(naindex, i, &index);
        str = sarrayGetString(sain, index, L_COPY);
        sarrayAddString(saout, str, L_INSERT);
    }

    return saout;
}


/*!
 * \brief   stringCompareLexical()
 *
 * \param[in]    str1
 * \param[in]    str2
 * \return  1 if str1 > str2 lexically; 0 otherwise
 *
 * <pre>
 * Notes:
 *      (1) If the lexical values are identical, return a 0, to
 *          indicate that no swapping is required to sort the strings.
 * </pre>
 */
l_int32
stringCompareLexical(const char *str1,
                     const char *str2)
{
l_int32  i, len1, len2, len;

    if (!str1)
        return ERROR_INT("str1 not defined", __func__, 1);
    if (!str2)
        return ERROR_INT("str2 not defined", __func__, 1);

    len1 = strlen(str1);
    len2 = strlen(str2);
    len = L_MIN(len1, len2);

    for (i = 0; i < len; i++) {
        if (str1[i] == str2[i])
            continue;
        if (str1[i] > str2[i])
            return 1;
        else
            return 0;
    }

    if (len1 > len2)
        return 1;
    else
        return 0;
}


/*----------------------------------------------------------------------*
 *                   Set operations using aset (rbtree)                 *
 *----------------------------------------------------------------------*/
/*!
 * \brief   l_asetCreateFromSarray()
 *
 * \param[in]    sa
 * \return  set using a string hash into a uint64 as the key
 */
L_ASET *
l_asetCreateFromSarray(SARRAY  *sa)
{
char     *str;
l_int32   i, n;
l_uint64  hash;
L_ASET   *set;
RB_TYPE   key;

    if (!sa)
        return (L_ASET *)ERROR_PTR("sa not defined", __func__, NULL);

    set = l_asetCreate(L_UINT_TYPE);
    n = sarrayGetCount(sa);
    for (i = 0; i < n; i++) {
        str = sarrayGetString(sa, i, L_NOCOPY);
        l_hashStringToUint64Fast(str, &hash);
        key.utype = hash;
        l_asetInsert(set, key);
    }

    return set;
}


/*!
 * \brief   sarrayRemoveDupsByAset()
 *
 * \param[in]    sas
 * \param[out]   psad      with duplicates removed
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is O(nlogn), considerably slower than
 *          sarrayRemoveDupsByHmap() for large string arrays.
 *      (2) The key for each string is a 64-bit hash.
 *      (3) Build a set, using hashed strings as keys.  As the set is
 *          built, first do a find; if not found, add the key to the
 *          set and add the string to the output sarray.
 * </pre>
 */
l_ok
sarrayRemoveDupsByAset(SARRAY   *sas,
                       SARRAY  **psad)
{
char     *str;
l_int32   i, n;
l_uint64  hash;
L_ASET   *set;
RB_TYPE   key;
SARRAY   *sad;

    if (!psad)
        return ERROR_INT("&sad not defined", __func__, 1);
    *psad = NULL;
    if (!sas)
        return ERROR_INT("sas not defined", __func__, 1);

    set = l_asetCreate(L_UINT_TYPE);
    sad = sarrayCreate(0);
    *psad = sad;
    n = sarrayGetCount(sas);
    for (i = 0; i < n; i++) {
        str = sarrayGetString(sas, i, L_NOCOPY);
        l_hashStringToUint64Fast(str, &hash);
        key.utype = hash;
        if (!l_asetFind(set, key)) {
            sarrayAddString(sad, str, L_COPY);
            l_asetInsert(set, key);
        }
    }

    l_asetDestroy(&set);
    return 0;
}


/*!
 * \brief   sarrayUnionByAset()
 *
 * \param[in]    sa1
 * \param[in]    sa2
 * \param[out]   psad      union of the two string arrays
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Duplicates are removed from the concatenation of the two arrays.
 *      (2) The key for each string is a 64-bit hash.
 *      (2) Algorithm: Concatenate the two sarrays.  Then build a set,
 *          using hashed strings as keys.  As the set is built, first do
 *          a find; if not found, add the key to the set and add the string
 *          to the output sarray.  This is O(nlogn).
 * </pre>
 */
l_ok
sarrayUnionByAset(SARRAY   *sa1,
                  SARRAY   *sa2,
                  SARRAY  **psad)
{
SARRAY  *sa3;

    if (!psad)
        return ERROR_INT("&sad not defined", __func__, 1);
    *psad = NULL;
    if (!sa1)
        return ERROR_INT("sa1 not defined", __func__, 1);
    if (!sa2)
        return ERROR_INT("sa2 not defined", __func__, 1);

        /* Join */
    sa3 = sarrayCopy(sa1);
    if (sarrayJoin(sa3, sa2) == 1) {
        sarrayDestroy(&sa3);
        return ERROR_INT("join failed for sa3", __func__, 1);
    }

        /* Eliminate duplicates */
    sarrayRemoveDupsByAset(sa3, psad);
    sarrayDestroy(&sa3);
    return 0;
}


/*!
 * \brief   sarrayIntersectionByAset()
 *
 * \param[in]    sa1
 * \param[in]    sa2
 * \param[out]   psad      intersection of the two string arrays
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Algorithm: put the larger sarray into a set, using the string
 *          hashes as the key values.  Then run through the smaller sarray,
 *          building an output sarray and a second set from the strings
 *          in the larger array: if a string is in the first set but
 *          not in the second, add the string to the output sarray and hash
 *          it into the second set.  The second set is required to make
 *          sure only one instance of each string is put into the output sarray.
 *          This is O(mlogn), {m,n} = sizes of {smaller,larger} input arrays.
 * </pre>
 */
l_ok
sarrayIntersectionByAset(SARRAY   *sa1,
                         SARRAY   *sa2,
                         SARRAY  **psad)
{
char     *str;
l_int32   n1, n2, i, n;
l_uint64  hash;
L_ASET   *set1, *set2;
RB_TYPE   key;
SARRAY   *sa_small, *sa_big, *sad;

    if (!psad)
        return ERROR_INT("&sad not defined", __func__, 1);
    *psad = NULL;
    if (!sa1)
        return ERROR_INT("sa1 not defined", __func__, 1);
    if (!sa2)
        return ERROR_INT("sa2 not defined", __func__, 1);

        /* Put the elements of the biggest array into a set */
    n1 = sarrayGetCount(sa1);
    n2 = sarrayGetCount(sa2);
    sa_small = (n1 < n2) ? sa1 : sa2;   /* do not destroy sa_small */
    sa_big = (n1 < n2) ? sa2 : sa1;   /* do not destroy sa_big */
    set1 = l_asetCreateFromSarray(sa_big);

        /* Build up the intersection of strings */
    sad = sarrayCreate(0);
    *psad = sad;
    n = sarrayGetCount(sa_small);
    set2 = l_asetCreate(L_UINT_TYPE);
    for (i = 0; i < n; i++) {
        str = sarrayGetString(sa_small, i, L_NOCOPY);
        l_hashStringToUint64Fast(str, &hash);
        key.utype = hash;
        if (l_asetFind(set1, key) && !l_asetFind(set2, key)) {
            sarrayAddString(sad, str, L_COPY);
            l_asetInsert(set2, key);
        }
    }

    l_asetDestroy(&set1);
    l_asetDestroy(&set2);
    return 0;
}


/*----------------------------------------------------------------------*
 *                          Hashmap operations                          *
 *----------------------------------------------------------------------*/
/*!
 * \brief  l_hmapCreateFromSarray()
 *
 * \param[in]   sa     input sarray
 * \return      hmap   hashmap, or NULL on error
 */
L_HASHMAP *
l_hmapCreateFromSarray(SARRAY  *sa)
{
l_int32      i, n;
l_uint64     key;
char        *str;
L_HASHMAP   *hmap;

    if (!sa)
        return (L_HASHMAP *)ERROR_PTR("sa not defined", __func__, NULL);

    n = sarrayGetCount(sa);
    if ((hmap = l_hmapCreate(0.51 * n, 2)) == NULL)
        return (L_HASHMAP *)ERROR_PTR("hmap not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        str = sarrayGetString(sa, i, L_NOCOPY);
        l_hashStringToUint64Fast(str, &key);
        l_hmapLookup(hmap, key, i, L_HMAP_CREATE);
    }
    return hmap;
}


/*!
 * \brief  sarrayRemoveDupsByHmap()
 *
 * \param[in]   sas
 * \param[out]  psad    hash set of unique values
 * \param[out]  phmap   [optional] hashmap used for lookup
 * \return  0 if OK; 1 on error
 */
l_ok
sarrayRemoveDupsByHmap(SARRAY      *sas,
                       SARRAY     **psad,
                       L_HASHMAP  **phmap)
{
l_int32      i, tabsize;
char        *str;
SARRAY      *sad;
L_HASHITEM  *hitem;
L_HASHMAP   *hmap;

    if (phmap) *phmap = NULL;
    if (!psad)
        return ERROR_INT("&sad not defined", __func__, 1);
    *psad = NULL;
    if (!sas)
        return ERROR_INT("sas not defined", __func__, 1);

        /* Traverse the hashtable lists */
    if ((hmap = l_hmapCreateFromSarray(sas)) == NULL)
        return ERROR_INT("hmap not made", __func__, 1);
    sad = sarrayCreate(0);
    *psad = sad;
    tabsize = hmap->tabsize;
    for (i = 0; i < tabsize; i++) {
        hitem = hmap->hashtab[i];
        while (hitem) {
            str = sarrayGetString(sas, hitem->val, L_COPY);
            sarrayAddString(sad, str, L_INSERT);
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
 * \brief  sarrayUnionByHmap()
 *
 * \param[in]   sa1
 * \param[in]   sa2
 * \param[out]  *psad     union of the array values
 * \return  0 if OK; 1 on error
 */
l_ok
sarrayUnionByHmap(SARRAY   *sa1,
                  SARRAY   *sa2,
                  SARRAY  **psad)
{
SARRAY  *sa3;

    if (!psad)
        return ERROR_INT("&sad not defined", __func__, 1);
    *psad = NULL;
    if (!sa1)
        return ERROR_INT("sa1 not defined", __func__, 1);
    if (!sa2)
        return ERROR_INT("sa2 not defined", __func__, 1);

    sa3 = sarrayCopy(sa1);
    if (sarrayJoin(sa3, sa2) == 1) {
        sarrayDestroy(&sa3);
        return ERROR_INT("sa3 join failed", __func__, 1);
    }
    sarrayRemoveDupsByHmap(sa3, psad, NULL);
    sarrayDestroy(&sa3);
    return 0;
}


/*!
 * \brief  sarrayIntersectionByHmap()
 *
 * \param[in]   sa1
 * \param[in]   sa2
 * \param[out]  *psad     intersection of the array values
 * \return  0 if OK; 1 on error
 */
l_ok
sarrayIntersectionByHmap(SARRAY   *sa1,
                         SARRAY   *sa2,
                         SARRAY  **psad)
{
l_int32      i, n1, n2, n;
l_uint64     key;
char        *str;
SARRAY      *sa_small, *sa_big, *sa3, *sad;
L_HASHITEM  *hitem;
L_HASHMAP   *hmap;

    if (!psad)
        return ERROR_INT("&sad not defined", __func__, 1);
    *psad = NULL;
    if (!sa1)
        return ERROR_INT("sa1 not defined", __func__, 1);
    if (!sa2)
        return ERROR_INT("sa2 not defined", __func__, 1);

        /* Make a hashmap for the elements of the biggest array */
    n1 = sarrayGetCount(sa1);
    n2 = sarrayGetCount(sa2);
    sa_small = (n1 < n2) ? sa1 : sa2;   /* do not destroy sa_small */
    sa_big = (n1 < n2) ? sa2 : sa1;   /* do not destroy sa_big */
    if ((hmap = l_hmapCreateFromSarray(sa_big)) == NULL)
        return ERROR_INT("hmap not made", __func__, 1);

        /* Remove duplicates from the smallest array.  Alternatively,
         * we can skip this step and avoid counting duplicates in
         * sa_small by modifying the count fields in the sa_big hashitems;
         * e.g., see l_hmapIntersectionDna(). */
    sarrayRemoveDupsByHmap(sa_small, &sa3, NULL);

        /* Go through sa3, the set of strings derived from the smallest array,
         * hashing into the big array table.  Any string found belongs to both,
         * so add it to the output array. */
    sad = sarrayCreate(0);
    *psad = sad;
    n = sarrayGetCount(sa3);
    for (i = 0; i < n; i++) {
        str = sarrayGetString(sa3, i, L_NOCOPY);
        l_hashStringToUint64Fast(str, &key);
        hitem = l_hmapLookup(hmap, key, i, L_HMAP_CHECK);
        if (hitem)
            sarrayAddString(sad, str, L_COPY);
    }
    l_hmapDestroy(&hmap);
    sarrayDestroy(&sa3);
    return 0;
}


/*----------------------------------------------------------------------*
 *                      Miscellaneous operations                        *
 *----------------------------------------------------------------------*/
/*!
 * \brief   sarrayGenerateIntegers()
 *
 * \param[in]   n
 * \return  sa  of printed numbers, 1 - n, or NULL on error
 */
SARRAY *
sarrayGenerateIntegers(l_int32  n)
{
char     buf[32];
l_int32  i;
SARRAY  *sa;

    if ((sa = sarrayCreate(n)) == NULL)
        return (SARRAY *)ERROR_PTR("sa not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        snprintf(buf, sizeof(buf), "%d", i);
        sarrayAddString(sa, buf, L_COPY);
    }
    return sa;
}


/*!
 * \brief   sarrayLookupCSKV()
 *
 * \param[in]    sa          of strings, each being a comma-separated pair
 *                           of strings, the first being a key and the
 *                           second a value
 * \param[in]    keystring   an input string to match with each key in %sa
 * \param[out]   pvalstring  the returned value string corresponding to the
 *                           input key string, if found; otherwise NULL
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The input %sa can have other strings that are not in
 *          comma-separated key-value format.  These will be ignored.
 *      (2) This returns a copy of the first value string in %sa whose
 *          key string matches the input %keystring.
 *      (3) White space is not ignored; all white space before the ','
 *          is used for the keystring in matching.  This allows the
 *          key and val strings to have white space (e.g., multiple words).
 * </pre>
 */
l_ok
sarrayLookupCSKV(SARRAY      *sa,
                 const char  *keystring,
                 char       **pvalstring)
{
char    *key, *val, *str;
l_int32  i, n;
SARRAY  *sa1;

    if (!pvalstring)
        return ERROR_INT("&valstring not defined", __func__, 1);
    *pvalstring = NULL;
    if (!sa)
        return ERROR_INT("sa not defined", __func__, 1);
    if (!keystring)
        return ERROR_INT("keystring not defined", __func__, 1);

    n = sarrayGetCount(sa);
    for (i = 0; i < n; i++) {
        str = sarrayGetString(sa, i, L_NOCOPY);
        sa1 = sarrayCreate(2);
        sarraySplitString(sa1, str, ",");
        if (sarrayGetCount(sa1) != 2) {
            sarrayDestroy(&sa1);
            continue;
        }
        key = sarrayGetString(sa1, 0, L_NOCOPY);
        val = sarrayGetString(sa1, 1, L_NOCOPY);
        if (!strcmp(key, keystring)) {
            *pvalstring = stringNew(val);
            sarrayDestroy(&sa1);
            return 0;
        }
        sarrayDestroy(&sa1);
    }

    return 0;
}

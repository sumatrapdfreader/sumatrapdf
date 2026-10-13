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

/*
 * \file  hashmap.c
 * <pre>
 *
 *      Hashmap creation, destruction
 *          L_HASHMAP    *l_hmapCreate()
 *          void          l_hmapDestroy()
 *
 *      Hashmap: Accessors and modifiers
 *          L_HASHITEM   *l_hmapLookup()
 *          l_int32       l_hmapRehash()
 *
 *    (1) See also hashmap.h for a brief description of the design.
 *    (2) In a typical use, a set of objects (in an array or associated
 *        with image pixels) is represented by a hashmap.  A uint64 key is
 *        produced for each object.  This integer is then hashed into an
 *        index in a hashtable, using the mod function with the table size
 *        which is a prime number.  Each entry in the hash table is a list
 *        of hash items.  In lookup, the appropriate list is traversed,
 *        looking for the object key found earlier.
 *    (3) Hash functions that map points, strings and float64 to uint64
 *        are given in utils1.c.
 *    (4) Use of the hashmap on points, strings and float64 data are
 *        given in ptafunc2.c, sarray2.c and dnafunc1.c.
 *    (5) Useful rule of thumb for hashing collisions:
 *        For a random hashing function (say, from strings to l_uint64),
 *        the probability of a collision increases as N^2 for N much
 *        less than 2^32.  The quadratic behavior switches over to
 *        approaching 1.0 around 2^32, which is the square root of 2^64.
 *        So, for example, if you have 10^7 strings, the probability
 *        of a single collision using an l_uint64 key is on the order of
 *            (10^7/4x10^9)^2 ~ 10^-5.
 *        For a million strings, collisions are very rare (~10^-7 probability).
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

    /* Limit on the hashtable size */
static const l_uint32  MaxTabsize = 50000000;
    /* Default values for creating the hashmap. */
static const l_int32   DefaultInitNItems = 2000;
static const l_int32   DefaultMaxOcc = 2;

/*--------------------------------------------------------------------------*
 *                     Hashmap creation and destruction                     *
 *--------------------------------------------------------------------------*/
/*!
 * \brief  l_hmapCreate()
 *
 * \param[in]   ninit   initial estimate of the number of items to be stored;
 *                      use 0 for default value.
 * \param[in]   maxocc  max average occupancy of each list of hashitme;
 *                      it should be in range [1 ... 5]; use 0 for default
 * \return      ptr to new hashmap, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If the maximum number n of items to be hashed is known in
 *          advance, suggested values are:
 *                 %nint = 0.51 * n
 *                 %maxocc = 2
 *          With these values, the table will not need to be rehashed,
 *          even if all items have unique keys.
 *      (2) The actual initial size of the hashtab is the first prime
 *          number larger than %ninit/%maxocc.
 *      (3) Each entry into the hashtab points to alist of hash items
 *          (key, val, count).
 * </pre>
 */
L_HASHMAP *
l_hmapCreate(l_int32  ninit,
             l_int32  maxocc)
{
l_uint32    size, tabsize;
L_HASHMAP  *hmap;

    ninit = L_MAX(ninit, DefaultInitNItems);
    if (maxocc <= 0) maxocc = DefaultMaxOcc;
    if (maxocc > 5) {
        L_WARNING("maxocc = %d; non-optimal value. Set to default = %d\n",
                  __func__, maxocc, DefaultMaxOcc);
        maxocc = DefaultMaxOcc;
    }
    size = ninit / maxocc;
    if (size > MaxTabsize) {
        L_ERROR("ninit/maxocc = %d > MaxTabsize = %d\n", __func__,
                size, MaxTabsize);
        return NULL;
    }

    hmap = (L_HASHMAP *)LEPT_CALLOC(1, sizeof(L_HASHMAP));
    findNextLargerPrime(size, &tabsize);  /* at least 101 */
    if ((hmap->hashtab =
        (L_HASHITEM **)LEPT_CALLOC(tabsize, sizeof(L_HASHITEM *))) == NULL) {
        LEPT_FREE(hmap);
        return (L_HASHMAP *)ERROR_PTR("hashtab not made", __func__, NULL);
    }

    hmap->nitems = 0;
    hmap->ntogo = ninit;
    hmap->maxocc = maxocc;
    hmap->tabsize = tabsize;
    return hmap;
}


/*!
 * \brief  l_hmapDestroy()
 *
 * \param[in,out]   phmap  to be nulled, if it exists
 * \return          void
 */
void
l_hmapDestroy(L_HASHMAP  **phmap)
{
l_int32      i;
L_HASHITEM  *hitem, *next;
L_HASHMAP   *hmap;

    if (phmap == NULL) {
        L_WARNING("ptr address is NULL!\n", __func__);
        return;
    }

    if ((hmap = *phmap) == NULL)
        return;

    for (i = 0; i < hmap->tabsize; i++) {
        for (hitem = hmap->hashtab[i]; hitem != NULL; hitem = next) {
            next = hitem->next;
            LEPT_FREE(hitem);
        }
    }
    LEPT_FREE(hmap->hashtab);
    LEPT_FREE(hmap);
    *phmap = NULL;
}


/*--------------------------------------------------------------------------*
 *                    Hashmap: Accessors and modifiers                      *
 *--------------------------------------------------------------------------*/
/*!
 * \brief  l_hmapLookup()
 *
 * \param[in]   hmap
 * \param[in]   key     to be hashed into an index in the hashtab
 * \param[in]   val     to be stored in the hitem if creating it
 * \param[in]   op      L_HMAP_CHECK, L_HMAP_CREATE
 * \return      ptr     hitem; or null either on error or if not found
 *                      with op == L_HMAP_CHECK.
 *
 * <pre>
 * Notes:
 *      (1) This lookup function will also create a new hitem if requested.
 *      (2) The %op parameter does the following:
 *          op == L_HMAP_CHECK: return the hitem, or null if not found
 *          op == L_HMAP_CREATE: if found, increment the count; otherwise, make
 *                               and store a new hitem; always return the hitem.
 *      (3) The key is a uint64.  It is made by hashing some data in the object.
 *      (4) The value is an index into an array of the objects from which
 *          the hashtable has been constructed.
 *      (5) If an hitem is found, a pointer to it is returned.  It is owned
 *          by the hashtable; do not destroy it.
 * </pre>
 */
L_HASHITEM *
l_hmapLookup(L_HASHMAP  *hmap,
             l_uint64    key,
             l_uint64    val,
             l_int32     op)
{
l_uint32     index;
L_HASHITEM  *hlist, *hitem;

    if (!hmap)
        return (L_HASHITEM *)ERROR_PTR("hmap not defined", __func__, NULL);
    if (op != L_HMAP_CHECK && op != L_HMAP_CREATE)
        return (L_HASHITEM *)ERROR_PTR("invalid op", __func__, NULL);

        /* If found, return a ptr to the hitem (not a copy) */
    index = key % hmap->tabsize;  /* into hashtab */
    hlist = hmap->hashtab[index];  /* head of the list */
    for (hitem = hlist; hitem != NULL; hitem = hitem->next) {
        if (key == hitem->key) {
            if (op == L_HMAP_CREATE) hitem->count++;
            return hitem;
        }
    }
    if (op == L_HMAP_CHECK) return NULL;

        /* Not found and %op == L_HMAP_CREATE.
         * Make a new hitem and add to the head of the list */
    hitem = (L_HASHITEM *)LEPT_CALLOC(1, sizeof(L_HASHITEM));
    hitem->key = key;
    hitem->val = val;
    hitem->count = 1;
    hitem->next = hlist;
    hmap->hashtab[index] = hitem;
    hmap->nitems++;
    hmap->ntogo--;

        /* If hmap is full based on average occupancy, rehash */
    if (hmap->ntogo == 0)
        l_hmapRehash(hmap);

    return hitem;
}


/*!
 * \brief  l_hmapRehash()
 *
 * \param[in]  hmap
 * \return     0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is called when the average occupancy reaches maxocc.
 *          It doubles the size of the hashtab, and reuses the hash items.
 * </pre>
 */
l_ok
l_hmapRehash(L_HASHMAP  *hmap)
{
l_int32      i, index;
l_uint32     tabsize;
L_HASHITEM  *hstore, *hitem, *next;

    if (!hmap)
        return ERROR_INT("hmap not defined", __func__, 1);

        /* Put hash items in temporary storage in a single list,
         * successively adding each to the list head. */
    hstore = NULL;  /* ptr to resulting list */
    for (i = 0; i < hmap->tabsize; i++) {
        for (hitem = hmap->hashtab[i]; hitem != NULL; hitem = next) {
            next = hitem->next;
            hitem->next = hstore;
            hstore = hitem;
        }
    }

        /* Destroy the old hashtab and make a new one that is twice as big */
    LEPT_FREE(hmap->hashtab);
    findNextLargerPrime(2 * hmap->tabsize, &tabsize);
    hmap->tabsize = tabsize;
    hmap->hashtab = (L_HASHITEM **)LEPT_CALLOC(tabsize, sizeof(L_HASHITEM *));
    if (hmap->hashtab == NULL) {
        hmap->tabsize = 0;
        return ERROR_INT("hashtab ptr array not made", __func__, 1);
    }
    hmap->ntogo = hmap->maxocc * tabsize - hmap->nitems;

        /* Populate with the stored hash items */
    for (hitem = hstore; hitem != NULL; hitem = next) {
        next = hitem->next;
        index = hitem->key % tabsize;  /* into new hashtab */
        hitem->next = hmap->hashtab[index];  /* link to head of existing list */
        hmap->hashtab[index] = hitem;  /* put at head */
    }

    return 0;
}

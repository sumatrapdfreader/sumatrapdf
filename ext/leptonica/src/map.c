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
 * \file map.c
 * <pre>
 *
 *  This is an interface for map and set functions, based on using
 *  red-black binary search trees.  Because these trees are sorted,
 *  they are O(nlogn) to build.  They allow logn insertion, find
 *  and deletion of elements.
 *
 *  Both the map and set are ordered by key value, with unique keys.
 *  For the map, the elements are key/value pairs.
 *  For the set we only store unique, ordered keys, and the value
 *  (set to 0 in the implementation) is ignored.
 *
 *  The keys for the map and set can be any of the three types in the
 *  l_rbtree_keytype enum.  The values stored can be any of the four
 *  types in the rb_type union.
 *
 *  In-order forward and reverse iterators are provided for maps and sets.
 *  To forward iterate over the map for any type of key (in this example,
 *  uint32), extracting integer values:
 *
 *      L_AMAP  *m = l_amapCreate(L_UINT_TYPE);
 *      [add elements to the map ...]
 *      L_AMAP_NODE  *n = l_amapGetFirst(m);
 *      while (n) {
 *          l_int32 val = n->value.itype;
 *          // do something ...
 *          n = l_amapGetNext(n);
 *      }
 *
 *  If the nodes are deleted during the iteration:
 *
 *      L_AMAP  *m = l_amapCreate(L_UINT_TYPE);
 *      [add elements to the map ...]
 *      L_AMAP_NODE  *n = l_amapGetFirst(m);
 *      L_AMAP_NODE  *nn;
 *      while (n) {
 *          nn = l_amapGetNext(n);
 *          l_int32 val = n->value.itype;
 *          l_uint32 key = n->key.utype;
 *          // do something ...
 *          l_amapDelete(m, n->key);
 *          n = nn;
 *      }
 *
 *  See prog/maptest.c and prog/settest.c for more examples of usage.
 *
 *  Interface to (a) map using a general key and storing general values
 *           L_AMAP        *l_amapCreate()
 *           RB_TYPE       *l_amapFind()
 *           void           l_amapInsert()
 *           void           l_amapDelete()
 *           void           l_amapDestroy()
 *           L_AMAP_NODE   *l_amapGetFirst()
 *           L_AMAP_NODE   *l_amapGetNext()
 *           L_AMAP_NODE   *l_amapGetLast()
 *           L_AMAP_NODE   *l_amapGetPrev()
 *           l_int32        l_amapSize()
 *
 *  Interface to (a) set using a general key
 *           L_ASET        *l_asetCreate()
 *           RB_TYPE       *l_asetFind()
 *           void           l_asetInsert()
 *           void           l_asetDelete()
 *           void           l_asetDestroy()
 *           L_ASET_NODE   *l_asetGetFirst()
 *           L_ASET_NODE   *l_asetGetNext()
 *           L_ASET_NODE   *l_asetGetLast()
 *           L_ASET_NODE   *l_asetGetPrev()
 *           l_int32        l_asetSize()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

/* ------------------------------------------------------------- *
 *                         Interface to Map                      *
 * ------------------------------------------------------------- */
L_AMAP *
l_amapCreate(l_int32  keytype)
{
L_AMAP  *m;

    if (keytype != L_INT_TYPE && keytype != L_UINT_TYPE &&
        keytype != L_FLOAT_TYPE)
        return (L_AMAP *)ERROR_PTR("invalid keytype", __func__, NULL);

    m = (L_AMAP *)LEPT_CALLOC(1, sizeof(L_AMAP));
    m->keytype = keytype;
    return m;
}

RB_TYPE *
l_amapFind(L_AMAP  *m,
           RB_TYPE  key)
{
    return l_rbtreeLookup(m, key);
}

void
l_amapInsert(L_AMAP  *m,
             RB_TYPE  key,
             RB_TYPE  value)
{
    l_rbtreeInsert(m, key, value);
}

void
l_amapDelete(L_AMAP  *m,
             RB_TYPE  key)
{
    l_rbtreeDelete(m, key);
}

void
l_amapDestroy(L_AMAP  **pm)
{
    l_rbtreeDestroy(pm);
}

L_AMAP_NODE *
l_amapGetFirst(L_AMAP  *m)
{
    return l_rbtreeGetFirst(m);
}

L_AMAP_NODE *
l_amapGetNext(L_AMAP_NODE  *n)
{
    return l_rbtreeGetNext(n);
}

L_AMAP_NODE *
l_amapGetLast(L_AMAP  *m)
{
    return l_rbtreeGetLast(m);
}

L_AMAP_NODE *
l_amapGetPrev(L_AMAP_NODE  *n)
{
    return l_rbtreeGetPrev(n);
}

l_int32
l_amapSize(L_AMAP  *m)
{
    return l_rbtreeGetCount(m);
}


/* ------------------------------------------------------------- *
 *                         Interface to Set                      *
 * ------------------------------------------------------------- */
L_ASET *
l_asetCreate(l_int32  keytype)
{
L_ASET  *s;

    if (keytype != L_INT_TYPE && keytype != L_UINT_TYPE &&
        keytype != L_FLOAT_TYPE)
        return (L_ASET *)ERROR_PTR("invalid keytype", __func__, NULL);

    s = (L_ASET *)LEPT_CALLOC(1, sizeof(L_ASET));
    s->keytype = keytype;
    return s;
}

/*
 *  l_asetFind()
 *
 *  This returns NULL if not found, non-null if it is.  In the latter
 *  case, the value stored in the returned pointer has no significance.
 */
RB_TYPE *
l_asetFind(L_ASET  *s,
           RB_TYPE  key)
{
    return l_rbtreeLookup(s, key);
}

void
l_asetInsert(L_ASET  *s,
             RB_TYPE  key)
{
RB_TYPE  value;

    value.itype = 0;  /* meaningless */
    l_rbtreeInsert(s, key, value);
}

void
l_asetDelete(L_ASET  *s,
             RB_TYPE  key)
{
    l_rbtreeDelete(s, key);
}

void
l_asetDestroy(L_ASET  **ps)
{
    l_rbtreeDestroy(ps);
}

L_ASET_NODE *
l_asetGetFirst(L_ASET  *s)
{
    return l_rbtreeGetFirst(s);
}

L_ASET_NODE *
l_asetGetNext(L_ASET_NODE  *n)
{
    return l_rbtreeGetNext(n);
}

L_ASET_NODE *
l_asetGetLast(L_ASET  *s)
{
    return l_rbtreeGetLast(s);
}

L_ASET_NODE *
l_asetGetPrev(L_ASET_NODE  *n)
{
    return l_rbtreeGetPrev(n);
}

l_int32
l_asetSize(L_ASET  *s)
{
    return l_rbtreeGetCount(s);
}

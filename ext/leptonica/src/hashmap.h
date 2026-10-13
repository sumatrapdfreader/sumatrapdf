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

#ifndef  LEPTONICA_HASHMAP_H
#define  LEPTONICA_HASHMAP_H

/*
 * \file hashmap.h
 *
 * <pre>
 *  Contains the following structs:
 *      struct L_Hashmap
 *      struct L_Hashitem
 *
 *  Goal:
 *      You have a set of objects (integers, strings, pts, whatever),
 *      and you want to store them in a data structure (L_Hashmap) that allows
 *      O(1) to find, insert and count the occurrences of such an object.
 *      The tool is a hash map.  This is not ordered, unlike the O(log n)
 *      ordered map (L_Amap), which is implemented by an rbtree.
 *
 *  In slightly more detail:
 *      Store the set of objects in an array, which in general can be
 *      held in a pointer array (L_Ptra).  You need a hash function that
 *      will generate a unique uint64 key from each object.  For our simple
 *      built-in arrays, such as float, double and Pta (points), these hash
 *      functions are in utils1.c.  Then for each object in the array,
 *      you store the key and the index to the array of objects (the val)
 *      in a list of hashitems in the hash table, where the specific
 *      list is determined by the key (specifically, the mod of the key
 *      with the size of the hashtable).
 *
 *  In yet more detail:
 *  (1) The design loosely follows the design of a hashmap in "The Practice
 *      of Programming by Brian Kernighan and Rob Pike, Addison Wesley, 1999.
 *  (2) The L_Hashmap contains a hashtable with a prime number of pointers
 *      to lists of hashitems.  The lookup function takes a key and a value,
 *      which are both 64-bit unsigned integers.  The key has been generated
 *      by hashing the input object in a way that avoids collisions between
 *      different objects. The value is an integer that identifies the
 *      object; typically it is the index into an array of objects.
 *      The hashtable size is a prime number, and an index into the table
 *      is made from the key by taking its mod with the hashtable size.
 *      The index points to a list of hashitems, which have all been hashed
 *      by the mod function into the same index in the table.
 *      Because the key is expected to be randomly distributed in uint64,
 *      the table indices should be uniformly distributed, resulting in
 *      approximately the same number of items being placed in each of
 *      these lists.  The list of hashitems is traversed, comparing the
 *      input uint64 key in the lookup() function with the key stored in
 *      each hashitem.  If a hashitem is found with a matching key,
 *      return a pointer to that hashitem.  If not found and the op is
 *      L_HASH_CREATE, make a new hash item, add it to the list, and
 *      return a pointer to it.
 *  (3) The count field in the hashitem gives the number of times the
 *      key has been seen when storing key/value pairs.
 *  (4) The val field is the index into an array of the objects.  When
 *      the hashmap is initially made, it is the index of the first item
 *      seen with its key.
 *  (5) For the hashmap to work efficiently, the lists must not become too
 *      long.  Because in general you do not know the number of objects
 *      in advance, it is important to be able to dynamically resize
 *      the hashtable as it grows.  The hashmap is initialized with
 *      room for some number of hashitems and the maximum average list
 *      size.  These two numbers determine the size of the hashtable,
 *      which is constrained to be a prime number.  As the hashtable grows,
 *      if the average occupancy exceeds the input %maxocc, the hashtable
 *      size is approximately doubled and the existing items are re-hashed
 *      into it, mod the new (prime number) table size.
 * </pre>
 */

/*------------------------------------------------------------------------*
 *                           Hash map structs                             *
 *------------------------------------------------------------------------*/
/*! General hash map */
struct L_Hashmap
{
    l_int32              nitems;    /*!< number of stored items              */
    l_int32              ntogo;     /*!< number of items to be stored        */
                                    /*!< before resizing the hashmap         */
    l_int32              maxocc;    /*!< max average occupancy allowed       */
    struct L_Hashitem  **hashtab;   /*!< array of hash item ptrs             */
    l_int32              tabsize;   /*!< size of array of hash item ptrs     */
};
typedef struct L_Hashmap  L_HASHMAP;

/*! Hash item, containing storage for the key, value and count.  The key
    is a l_uint64, which is hashed by the mod function to find the index
    into the hashtab. */
struct L_Hashitem
{
    l_uint64            key;    /*!< key is hashed into index into hashtab   */
    l_uint64            val;    /*!< number stored associated with the key   */
    l_int32             count;  /*!< number of elements seen with this key   */
    struct L_Hashitem  *next;   /*!< ptr to the next in the list             */
};
typedef struct L_Hashitem  L_HASHITEM;


/*------------------------------------------------------------------------*
 *                            Hashmap flags                               *
 *------------------------------------------------------------------------*/
/*! Hashmap Lookup */
enum {
    L_UNDEFINED = 0,         /*!< invalid operation                         */
    L_HMAP_CHECK = 1,        /*!< check if this key/val has been stored     */
    L_HMAP_CREATE = 2        /*!< create and store a hashitem if not found  */
};

#endif  /* LEPTONICA_HASHMAP_H */

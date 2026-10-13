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

#ifndef  LEPTONICA_HEAP_H
#define  LEPTONICA_HEAP_H

/*!
 * \file heap.h
 *
 * <pre>
 *      Expandable priority queue configured as a heap for arbitrary void* data
 *
 *      The L_Heap is used to implement a priority queue.  The elements
 *      in the heap are ordered in either increasing or decreasing key value.
 *      The key is a float field 'keyval' that is required to be
 *      contained in the elements of the queue.
 *
 *      The heap is a simple binary tree with the following constraints:
 *         - the key of each node is >= the keys of the two children
 *         - the tree is complete, meaning that each level (1, 2, 4, ...)
 *           is filled and the last level is filled from left to right
 *
 *      The tree structure is implicit in the queue array, with the
 *      array elements numbered as a breadth-first search of the tree
 *      from left to right.  It is thus guaranteed that the largest
 *      (or smallest) key belongs to the first element in the array.
 *
 *      Heap sort is used to sort the array.  Once an array has been
 *      sorted as a heap, it is convenient to use it as a priority queue,
 *      because the min (or max) elements are always at the root of
 *      the tree (element 0), and once removed, the heap can be
 *      resorted in not more than log[n] steps, where n is the number
 *      of elements on the heap.  Likewise, if an arbitrary element is
 *      added to the end of the array A, the sorted heap can be restored
 *      in not more than log[n] steps.
 *
 *      A L_Heap differs from a L_Queue in that the elements in the former
 *      are sorted by a key.  Internally, the array is maintained
 *      as a queue, with a pointer to the end of the array.  The
 *      head of the array always remains at array[0].  The array is
 *      maintained (sorted) as a heap.  When an item is removed from
 *      the head, the last item takes its place (thus reducing the
 *      array length by 1), and this is followed by array element
 *      swaps to restore the heap property.   When an item is added,
 *      it goes at the end of the array, and is swapped up to restore
 *      the heap.  If the ptr array is full, adding another item causes
 *      the ptr array size to double.
 *
 *      For further implementation details, see heap.c.
 * </pre>
 */

/*! Heap of arbitrary void* data */
struct L_Heap
{
    l_int32      nalloc;      /*!< size of allocated ptr array               */
    l_int32      n;           /*!< number of elements stored in the heap     */
    void       **array;       /*!< ptr array                                 */
    l_int32      direction;   /*!< L_SORT_INCREASING or L_SORT_DECREASING    */
};
typedef struct L_Heap  L_HEAP;


#endif  /* LEPTONICA_HEAP_H */

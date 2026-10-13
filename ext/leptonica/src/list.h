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


#ifndef  LEPTONICA_LIST_H
#define  LEPTONICA_LIST_H

/*!
 * \file list.h
 *
 * <pre>
 *       Cell for double-linked lists
 *
 *       This allows composition of a list of cells with
 *           prev, next and data pointers.  Generic data
 *           structures hang on the list cell data pointers.
 *
 *       The list is not circular because that would add much
 *           complexity in traversing the list under general
 *           conditions where list cells can be added and removed.
 *           The only disadvantage of not having the head point to
 *           the last cell is that the list must be traversed to
 *           find its tail.  However, this traversal is fast, and
 *           the listRemoveFromTail() function updates the tail
 *           so there is no searching overhead with repeated use.
 *
 *       The list macros are used to run through a list, and their
 *       use is encouraged.  They are invoked, e.g., as
 *
 *             DLLIST  *head, *elem;
 *             ...
 *             L_BEGIN_LIST_FORWARD(head, elem)
 *                 <do something with elem and/or elem->data >
 *             L_END_LIST
 * </pre>
 */

struct DoubleLinkedList
{
    struct DoubleLinkedList    *prev;
    struct DoubleLinkedList    *next;
    void                       *data;
};
typedef struct DoubleLinkedList    DLLIST;


    /*!  Simple list traverse macro - forward */
#define L_BEGIN_LIST_FORWARD(head, element) \
        { \
        DLLIST   *_leptvar_nextelem_; \
        for ((element) = (head); (element); (element) = _leptvar_nextelem_) { \
            _leptvar_nextelem_ = (element)->next;


    /*!  Simple list traverse macro - reverse */
#define L_BEGIN_LIST_REVERSE(tail, element) \
        { \
        DLLIST   *_leptvar_prevelem_; \
        for ((element) = (tail); (element); (element) = _leptvar_prevelem_) { \
            _leptvar_prevelem_ = (element)->prev;


    /*!  Simple list traverse macro - end of a list traverse */
#define L_END_LIST    }}


#endif  /* LEPTONICA_LIST_H */

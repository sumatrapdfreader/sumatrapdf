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
 * \file  list.c
 * <pre>
 *
 *      Inserting and removing elements
 *
 *           void      listDestroy()
 *           DLLIST   *listAddToHead()
 *           l_int32   listAddToTail()
 *           l_int32   listInsertBefore()
 *           l_int32   listInsertAfter()
 *           void     *listRemoveElement()
 *           void     *listRemoveFromHead()
 *           void     *listRemoveFromTail()
 *
 *      Other list operations
 *
 *           DLLIST   *listFindElement()
 *           DLLIST   *listFindTail()
 *           l_int32   listGetCount()
 *           l_int32   listReverse()
 *           DLLIST   *listJoin()
 *
 *      Lists are much harder to handle than arrays.  There is
 *      more overhead for the programmer, both cognitive and
 *      codewise, and more likelihood that an error can be made.
 *      For that reason, lists should only be used when it is
 *      inefficient to use arrays, such as when elements are
 *      routinely inserted or deleted from inside arrays whose
 *      average size is greater than about 10.
 *
 *      A list of data structures can be implemented in a number
 *      of ways.  The two most popular are:
 *
 *         (1) The list can be composed of a linked list of
 *             pointer cells ("cons cells"), where the data structures
 *             are hung off the cells.  This is more difficult
 *             to use because you have to keep track of both
 *             your hanging data and the cell structures.
 *             It requires 3 pointers for every data structure
 *             that is put in a list.  There is no problem
 *             cloning (using reference counts) for structures that
 *             are put in such a list.  We implement lists by this
 *             method here.
 *
 *         (2) The list pointers can be inserted directly into
 *             the data structures.  This is easy to implement
 *             and easier to use, but it adds 2 ptrs of overhead
 *             to every data structure in which the ptrs are embedded.
 *             It also requires special care not to put the ptrs
 *             in any data that is cloned with a reference count;
 *             else your lists will break.
 *
 *      Writing C code that uses list pointers explicitly to make
 *      and alter lists is difficult and prone to error.
 *      Consequently, a generic list utility that handles lists
 *      of arbitrary objects and doesn't force the programmer to
 *      touch the "next" and "prev" pointers, is quite useful.
 *      Such functions are provided here.   However, the usual
 *      situation requires traversing a list and applying some
 *      function to one or more of the list elements.  Macros
 *      for traversing the list are, in general, necessary, to
 *      achieve the goal of invisibly handling all "next" and "prev"
 *      pointers in generic lists.  We provide macros for
 *      traversing a list in both forward and reverse directions.
 *
 *      Because of the typing in C, implementation of a general
 *      list utility requires casting.  If macros are used, the
 *      casting can be done implicitly; otherwise, using functions,
 *      some of the casts must be explicit.  Fortunately, this
 *      can be implemented with void* so the programmer using
 *      the library will not have to make any casts!  (Unless you
 *      compile with g++, in which case the rules on implicit
 *      conversion are more strict.)
 *
 *      For example, to add an arbitrary data structure foo to the
 *      tail of a list, use
 *             listAddToTail(&head, &tail, pfoo);
 *      where head and tail are list cell ptrs and pfoo is
 *      a pointer to the foo object.
 *      And to remove an arbitrary data structure foo from a
 *      list, when you know the list cell element it is hanging from,
 *      use
 *             pfoo = listRemoveElement(&head, elem)
 *      where head and elem are list cell ptrs and pfoo is a pointer
 *      to the foo object.  No casts are required for foo in
 *      either direction in ANSI C.  (However, casts are
 *      required for ANSI C++).
 *
 *      We use lists that are composed of doubly-linked
 *      cells with data structures hanging off the cells.
 *      We use doubly-linked cells to simplify insertion
 *      and deletion, and to allow operations to proceed in either
 *      direction along the list.  With doubly-linked lists,
 *      it is tempting to make them circular, by setting head->prev
 *      to the tail of the list and tail->next to the head.
 *      The circular list costs nothing extra in storage, and
 *      allows operations to proceed from either end of the list
 *      with equal speed.  However, the circular link adds
 *      cognitive overhead for the application programmer in
 *      general, and it greatly complicates list traversal when
 *      arbitrary list elements can be added or removed as you
 *      move through.  It can be done, but in the spirit of
 *      simplicity, we avoid the temptation.  The price to be paid
 *      is the extra cost to find the tail of a list -- a full
 *      traversal -- before the tail can be used.  This is a
 *      cheap price to pay to avoid major headaches and buggy code.
 *
 *      When you are only applying some function to each element
 *      in a list, you can go either forwards or backwards.
 *      To run through a list forwards, use:
 * \code
 *          for (elem = head; elem; elem = nextelem) {
 *              nextelem = elem->next;   (in case we destroy elem)
 *              <do something with elem->data>
 *          }
 * \endcode
 *      To run through a list backwards, find the tail and use:
 *
 *          for (elem = tail; elem; elem = prevelem) {
 #              prevelem = elem->prev;  (in case we destroy elem)
 *              <do something with elem->data>
 *          }
 *
 *      Even though these patterns are very simple, they are so common
 *      that we've provided macros for them in list.h.  Using the
 *      macros, this becomes:
 * \code
 *          L_BEGIN_LIST_FORWARD(head, elem)
 *              <do something with elem->data>
 *          L_END_LIST
 *
 *          L_BEGIN_LIST_REVERSE(tail, elem)
 *              <do something with elem->data>
 *          L_END_LIST
 * \endcode
 *      Note again that with macros, the application programmer does
 *      not need to refer explicitly to next and prev fields.  Also,
 *      in the reverse case, note that we do not explicitly
 *      show the head of the list.  However, the head of the list
 *      is always in scope, and functions can be called within the
 *      iterator that change the head.
 *
 *      Some special cases are simpler.  For example, when
 *      removing all items from the head of the list, you can use
 * \code
 *          while (head) {
 *              obj = listRemoveFromHead(&head);
 *              <do something with obj>
 *          }
 * \endcode
 *      Removing successive elements from the tail is equally simple:
 * \code
 *          while (tail) {
 *              obj = listRemoveFromTail(&head, &tail);
 *              <do something with obj>
 *          }
 * \endcode
 *      When removing an arbitrary element from a list, use
 * \code
 *              obj = listRemoveElement(&head, elem);
 * \endcode
 *      All the listRemove*() functions hand you the object,
 *      destroy the list cell to which it was attached, and
 *      reset the list pointers if necessary.
 *
 *      Several other list operations, that do not involve
 *      inserting or removing objects, are also provided.
 *      The function listFindElement() locates a list pointer
 *      by matching the object hanging on it to a given
 *      object.  The function listFindTail() gets a handle
 *      to the tail list ptr, allowing backwards traversals of
 *      the list.  listGetCount() gives the number of elements
 *      in a list.  Functions that reverse a list and concatenate
 *      two lists are also provided.
 *
 *      These functions can be modified for efficiency in the
 *      situation where there is a large amount of creation and
 *      destruction of list cells.  If millions of cells are
 *      made and destroyed, but a relatively small number are
 *      around at any time, the list cells can be stored for
 *      later re-use in a stack (see the generic stack functions
 *      in stack.c).
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

/*---------------------------------------------------------------------*
 *                    Inserting and removing elements                  *
 *---------------------------------------------------------------------*/
/*!
 * \brief   listDestroy()
 *
 * \param[in,out]   phead   head of list; will be set to null before returning
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) This only destroys the cons cells.  Before destroying
 *          the list, it is necessary to remove all data and set the
 *          data pointers in each cons cell to NULL.
 *      (2) listDestroy() will give a warning message for each data
 *          ptr that is not NULL.
 * </pre>
 */
void
listDestroy(DLLIST  **phead)
{
DLLIST  *elem, *next, *head;

    if (phead == NULL) {
        L_WARNING("ptr address is null!\n", __func__);
        return;
    }

    if ((head = *phead) == NULL)
        return;

    for (elem = head; elem; elem = next) {
        if (elem->data)
            L_WARNING("list data ptr is not null\n", __func__);
        next = elem->next;
        LEPT_FREE(elem);
    }
    *phead = NULL;
}


/*!
 * \brief   listAddToHead()
 *
 * \param[in,out]   phead    [optional] input head
 * \param[in]       data     void* ptr, to be added
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This makes a new cell, attaches %data, and adds the
 *          cell to the head of the list.
 *      (2) When consing from NULL, be sure to initialize head to NULL
 *          before calling this function.
 * </pre>
 */
l_ok
listAddToHead(DLLIST  **phead,
              void     *data)
{
DLLIST  *cell, *head;

    if (!phead)
        return ERROR_INT("&head not defined", __func__, 1);
    head = *phead;
    if (!data)
        return ERROR_INT("data not defined", __func__, 1);

    cell = (DLLIST *)LEPT_CALLOC(1, sizeof(DLLIST));
    cell->data = data;
    if (!head) {  /* start the list; initialize the ptrs */
        cell->prev = NULL;
        cell->next = NULL;
    } else {
        cell->prev = NULL;
        cell->next = head;
        head->prev = cell;
    }
    *phead = cell;
    return 0;
}


/*!
 * \brief   listAddToTail()
 *
 * \param[in,out]   phead    [may be updated], can be NULL
 * \param[in,out]   ptail    [updated], can be NULL
 * \param[in]       data     void* ptr, to be hung on tail cons cell
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This makes a new cell, attaches %data, and adds the
 *          cell to the tail of the list.
 *      (2) &head is input to allow the list to be "cons'd" up from NULL.
 *      (3) &tail is input to allow the tail to be updated
 *          for efficient sequential operation with this function.
 *      (4) We assume that if *phead and/or *ptail are not NULL,
 *          then they are valid addresses.  Therefore:
 *           (a) when consing from NULL, be sure to initialize both
 *               head and tail to NULL.
 *           (b) when tail == NULL for an existing list, the tail
 *               will be found and updated.
 * </pre>
 */
l_ok
listAddToTail(DLLIST  **phead,
              DLLIST  **ptail,
              void     *data)
{
DLLIST  *cell, *head, *tail;

    if (!phead)
        return ERROR_INT("&head not defined", __func__, 1);
    head = *phead;
    if (!ptail)
        return ERROR_INT("&tail not defined", __func__, 1);
    if (!data)
        return ERROR_INT("data not defined", __func__, 1);

    cell = (DLLIST *)LEPT_CALLOC(1, sizeof(DLLIST));
    cell->data = data;
    if (!head) {  /*   Start the list and initialize the ptrs.  *ptail
                   *   should also have been initialized to NULL */
        cell->prev = NULL;
        cell->next = NULL;
        *phead = cell;
        *ptail = cell;
    } else {
        if ((tail = *ptail) == NULL)
            tail = listFindTail(head);
        cell->prev = tail;
        cell->next = NULL;
        tail->next = cell;
        *ptail = cell;
    }

    return 0;
}


/*!
 * \brief   listInsertBefore()
 *
 * \param[in,out]   phead    [optional] input head
 * \param[in]       elem     list element to be inserted in front of;
 *                           must be NULL if head is NULL
 * \param[in]       data     void* address, to be added
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This can be called on a null list, in which case both
 *          head and elem must be null.
 *      (2) If you are searching through a list, looking for a condition
 *          to add an element, you can do something like this:
 * \code
 *            L_BEGIN_LIST_FORWARD(head, elem)
 *                <identify an elem to insert before>
 *                listInsertBefore(&head, elem, data);
 *            L_END_LIST
 * \endcode
 * </pre>
 */
l_ok
listInsertBefore(DLLIST  **phead,
                 DLLIST   *elem,
                 void     *data)
{
DLLIST  *cell, *head;

    if (!phead)
        return ERROR_INT("&head not defined", __func__, 1);
    head = *phead;
    if (!data)
        return ERROR_INT("data not defined", __func__, 1);
    if ((!head && elem) || (head && !elem))
        return ERROR_INT("head and elem not consistent", __func__, 1);

        /* New cell to insert */
    cell = (DLLIST *)LEPT_CALLOC(1, sizeof(DLLIST));
    cell->data = data;
    if (!head) {  /* start the list; initialize the ptrs */
        cell->prev = NULL;
        cell->next = NULL;
        *phead = cell;
    } else if (head == elem) {  /* insert before head of list */
        cell->prev = NULL;
        cell->next = head;
        head->prev = cell;
        *phead = cell;
    } else  {   /* insert before elem and after head of list */
        cell->prev = elem->prev;
        cell->next = elem;
        elem->prev->next = cell;
        elem->prev = cell;
    }
    return 0;
}


/*!
 * \brief   listInsertAfter()
 *
 * \param[in,out]   phead    [optional] input head
 * \param[in]       elem     list element to be inserted after;
 *                           must be NULL if head is NULL
 * \param[in]       data     void* ptr, to be added
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This can be called on a null list, in which case both
 *          head and elem must be null.  The head is included
 *          in the call to allow "consing" up from NULL.
 *      (2) If you are searching through a list, looking for a condition
 *          to add an element, you can do something like this:
 * \code
 *            L_BEGIN_LIST_FORWARD(head, elem)
 *                <identify an elem to insert after>
 *                listInsertAfter(&head, elem, data);
 *            L_END_LIST
 * \endcode
 * </pre>
 */
l_ok
listInsertAfter(DLLIST  **phead,
                DLLIST   *elem,
                void     *data)
{
DLLIST  *cell, *head;

    if (!phead)
        return ERROR_INT("&head not defined", __func__, 1);
    head = *phead;
    if (!data)
        return ERROR_INT("data not defined", __func__, 1);
    if ((!head && elem) || (head && !elem))
        return ERROR_INT("head and elem not consistent", __func__, 1);

        /* New cell to insert */
    cell = (DLLIST *)LEPT_CALLOC(1, sizeof(DLLIST));
    cell->data = data;
    if (!head) {  /* start the list; initialize the ptrs */
        cell->prev = NULL;
        cell->next = NULL;
        *phead = cell;
    } else if (elem->next == NULL) {  /* insert after last */
        cell->prev = elem;
        cell->next = NULL;
        elem->next = cell;
    } else  {  /* insert after elem and before the end */
        cell->prev = elem;
        cell->next = elem->next;
        elem->next->prev = cell;
        elem->next = cell;
    }
    return 0;
}


/*!
 * \brief   listRemoveElement()
 *
 * \param[in,out]   phead    input head; can be changed
 * \param[in]       elem     list element to be removed
 * \return  data  void* struct on cell
 *
 * <pre>
 * Notes:
 *      (1) in ANSI C, it is not necessary to cast return to actual type; e.g.,
 *             pix = listRemoveElement(&head, elem);
 *          but in ANSI C++, it is necessary to do the cast:
 *             pix = (Pix *)listRemoveElement(&head, elem);
 * </pre>
 */
void *
listRemoveElement(DLLIST  **phead,
                  DLLIST   *elem)
{
void    *data;
DLLIST  *head;

    if (!phead)
        return (void *)ERROR_PTR("&head not defined", __func__, NULL);
    head = *phead;
    if (!head)
        return (void *)ERROR_PTR("head not defined", __func__, NULL);
    if (!elem)
        return (void *)ERROR_PTR("elem not defined", __func__, NULL);

    data = elem->data;

    if (head->next == NULL) {  /* only one */
        if (elem != head)
            return (void *)ERROR_PTR("elem must be head", __func__, NULL);
        *phead = NULL;
    } else if (head == elem) {   /* first one */
        elem->next->prev = NULL;
        *phead = elem->next;
    } else if (elem->next == NULL) {   /* last one */
        elem->prev->next = NULL;
    } else {  /* neither the first nor the last one */
        elem->next->prev = elem->prev;
        elem->prev->next = elem->next;
    }

    LEPT_FREE(elem);
    return data;
}


/*!
 * \brief   listRemoveFromHead()
 *
 * \param[in,out]   phead     head of list; updated
 * \return  data  void* struct on cell, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) in ANSI C, it is not necessary to cast return to actual type; e.g.,
 *            pix = listRemoveFromHead(&head);
 *          but in ANSI C++, it is necessary to do the cast; e.g.,
 *            pix = (Pix *)listRemoveFromHead(&head);
 * </pre>
 */
void *
listRemoveFromHead(DLLIST  **phead)
{
DLLIST  *head;
void    *data;

    if (!phead)
        return (void *)ERROR_PTR("&head not defined", __func__, NULL);
    if ((head = *phead) == NULL)
        return (void *)ERROR_PTR("head not defined", __func__, NULL);

    if (head->next == NULL) {  /* only one */
        *phead = NULL;
    } else {
        head->next->prev = NULL;
        *phead = head->next;
    }

    data = head->data;
    LEPT_FREE(head);
    return data;
}


/*!
 * \brief   listRemoveFromTail()
 *
 * \param[in,out]   phead    list head must NOT be NULL; may be changed
 * \param[in,out]   ptail    list tail may be NULL; always updated
 * \return  data  void* struct on cell or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) We include &head so that it can be set to NULL if
 *          if the only element in the list is removed.
 *      (2) The function is relying on the fact that if tail is
 *          not NULL, then is is a valid address.  You can use
 *          this function with tail == NULL for an existing list, in
 *          which case  the tail is found and updated, and the
 *          removed element is returned.
 *      (3) In ANSI C, it is not necessary to cast return to actual type; e.g.,
 *            pix = listRemoveFromTail(&head, &tail);
 *          but in ANSI C++, it is necessary to do the cast; e.g.,
 *            pix = (Pix *)listRemoveFromTail(&head, &tail);
 * </pre>
 */
void *
listRemoveFromTail(DLLIST  **phead,
                   DLLIST  **ptail)
{
DLLIST  *head, *tail;
void    *data;

    if (!phead)
        return (void *)ERROR_PTR("&head not defined", __func__, NULL);
    if ((head = *phead) == NULL)
        return (void *)ERROR_PTR("head not defined", __func__, NULL);
    if (!ptail)
        return (void *)ERROR_PTR("&tail not defined", __func__, NULL);
    if ((tail = *ptail) == NULL)
        tail = listFindTail(head);

    if (head->next == NULL) { /* only one */
        *phead = NULL;
        *ptail = NULL;
    } else {
        tail->prev->next = NULL;
        *ptail = tail->prev;
    }

    data = tail->data;
    LEPT_FREE(tail);
    return data;
}



/*---------------------------------------------------------------------*
 *                         Other list operations                       *
 *---------------------------------------------------------------------*/
/*!
 * \brief   listFindElement()
 *
 * \param[in]    head    list head
 * \param[in]    data    void* address, to be searched for
 * \return  cell  the containing cell, or NULL if not found or on error
 *
 * <pre>
 * Notes:
 *      (1) This returns a ptr to the cell, which is still embedded in
 *          the list.
 *      (2) This handle and the attached data have not been copied or
 *          reference counted, so they must not be destroyed.  This
 *          violates our basic rule that every handle returned from a
 *          function is owned by that function and must be destroyed,
 *          but if rules aren't there to be broken, why have them?
 * </pre>
 */
DLLIST *
listFindElement(DLLIST  *head,
                void    *data)
{
DLLIST  *cell;

    if (!head)
        return (DLLIST *)ERROR_PTR("head not defined", __func__, NULL);
    if (!data)
        return (DLLIST *)ERROR_PTR("data not defined", __func__, NULL);

    for (cell = head; cell; cell = cell->next) {
        if (cell->data == data)
            return cell;
    }

    return NULL;
}


/*!
 * \brief   listFindTail()
 *
 * \param[in]    head
 * \return  tail, or NULL on error
 */
DLLIST *
listFindTail(DLLIST  *head)
{
DLLIST  *cell;

    if (!head)
        return (DLLIST *)ERROR_PTR("head not defined", __func__, NULL);

    for (cell = head; cell; cell = cell->next) {
        if (cell->next == NULL)
            return cell;
    }

    return (DLLIST *)ERROR_PTR("tail not found !!", __func__, NULL);
}


/*!
 * \brief   listGetCount()
 *
 * \param[in]    head     of list
 * \return  number of elements; 0 if no list or on error
 */
l_int32
listGetCount(DLLIST  *head)
{
l_int32  count;
DLLIST  *elem;

    if (!head)
        return ERROR_INT("head not defined", __func__, 0);

    count = 0;
    for (elem = head; elem; elem = elem->next)
        count++;

    return count;
}


/*!
 * \brief   listReverse()
 *
 * \param[in,out]   phead    list head; may be changed
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This reverses the list in-place.
 * </pre>
 */
l_ok
listReverse(DLLIST  **phead)
{
void    *obj;  /* whatever */
DLLIST  *head, *rhead;

    if (!phead)
        return ERROR_INT("&head not defined", __func__, 1);
    if ((head = *phead) == NULL)
        return ERROR_INT("head not defined", __func__, 1);

    rhead = NULL;
    while (head) {
        obj = listRemoveFromHead(&head);
        listAddToHead(&rhead, obj);
    }

    *phead = rhead;
    return 0;
}


/*!
 * \brief   listJoin()
 *
 * \param[in,out]   phead1   head of first list; may be changed
 * \param[in,out]   phead2   head of second list; to be nulled
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The concatenated list is returned with head1 as the new head.
 *      (2) Both input ptrs must exist, though either can have the value NULL.
 * </pre>
 */
l_ok
listJoin(DLLIST  **phead1,
         DLLIST  **phead2)
{
void    *obj;
DLLIST  *head1, *head2, *tail1;

    if (!phead1)
        return ERROR_INT("&head1 not defined", __func__, 1);
    if (!phead2)
        return ERROR_INT("&head2 not defined", __func__, 1);

        /* If no list2, just return list1 unchanged */
    if ((head2 = *phead2) == NULL)
        return 0;

        /* If no list1, just return list2 */
    if ((head1 = *phead1) == NULL) {
        *phead1 = head2;
        *phead2 = NULL;
        return 0;
    }

        /* General case for concatenation into list 1 */
    tail1 = listFindTail(head1);
    while (head2) {
        obj = listRemoveFromHead(&head2);
        listAddToTail(&head1, &tail1, obj);
    }
    *phead2 = NULL;
    return 0;
}

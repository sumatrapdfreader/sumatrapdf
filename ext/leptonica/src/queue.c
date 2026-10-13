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
 * \file  queue.c
 * <pre>
 *
 *      Create/Destroy L_Queue
 *          L_QUEUE        *lqueueCreate()
 *          void           *lqueueDestroy()
 *
 *      Operations to add/remove to/from a L_Queue
 *          l_int32         lqueueAdd()
 *          static l_int32  lqueueExtendArray()
 *          void           *lqueueRemove()
 *
 *      Accessors
 *          l_int32         lqueueGetCount()
 *
 *      Debug output
 *          l_int32         lqueuePrint()
 *
 *    The lqueue is a fifo that implements a queue of void* pointers.
 *    It can be used to hold a queue of any type of struct.
 *    Internally, it maintains two counters:
 *        nhead:  location of head (in ptrs) from the beginning
 *                of the buffer
 *        nelem:  number of ptr elements stored in the queue
 *    As items are added to the queue, nelem increases.
 *    As items are removed, nhead increases and nelem decreases.
 *    Any time the tail reaches the end of the allocated buffer,
 *      all the pointers are shifted to the left, so that the head
 *      is at the beginning of the array.
 *    If the buffer becomes more than 3/4 full, it doubles in size.
 *
 *    [A circular queue would allow us to skip the shifting and
 *    to resize only when the buffer is full.  For most applications,
 *    the extra work we do for a linear queue is not significant.]
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

static const l_int32  MIN_BUFFER_SIZE = 20;             /* n'importe quoi */
static const l_int32  INITIAL_BUFFER_ARRAYSIZE = 1024;  /* n'importe quoi */

    /* Static function */
static l_int32 lqueueExtendArray(L_QUEUE *lq);

/*--------------------------------------------------------------------------*
 *                         L_Queue create/destroy                           *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   lqueueCreate()
 *
 * \param[in]    nalloc     size of ptr array to be alloc'd; 0 for default
 * \return  lqueue, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) Allocates a ptr array of given size, and initializes counters.
 * </pre>
 */
L_QUEUE *
lqueueCreate(l_int32  nalloc)
{
L_QUEUE  *lq;

    if (nalloc < MIN_BUFFER_SIZE)
        nalloc = INITIAL_BUFFER_ARRAYSIZE;

    lq = (L_QUEUE *)LEPT_CALLOC(1, sizeof(L_QUEUE));
    if ((lq->array = (void **)LEPT_CALLOC(nalloc, sizeof(void *))) == NULL) {
        lqueueDestroy(&lq, 0);
        return (L_QUEUE *)ERROR_PTR("ptr array not made", __func__, NULL);
    }
    lq->nalloc = nalloc;
    lq->nhead = lq->nelem = 0;
    return lq;
}


/*!
 * \brief   lqueueDestroy()
 *
 * \param[in,out]   plq       will be set to null before returning
 * \param[in]       freeflag  TRUE to free each remaining struct in the array
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) If freeflag is TRUE, frees each struct in the array.
 *      (2) If freeflag is FALSE but there are elements on the array,
 *          gives a warning and destroys the array.  This will
 *          cause a memory leak of all the items that were on the queue.
 *          So if the items require their own destroy function, they
 *          must be destroyed before the queue.  The same applies to the
 *          auxiliary stack, if it is used.
 *      (3) To destroy the L_Queue, we destroy the ptr array, then
 *          the lqueue, and then null the contents of the input ptr.
 * </pre>
 */
void
lqueueDestroy(L_QUEUE  **plq,
              l_int32    freeflag)
{
void     *item;
L_QUEUE  *lq;

    if (plq == NULL) {
        L_WARNING("ptr address is NULL\n", __func__);
        return;
    }
    if ((lq = *plq) == NULL)
        return;

    if (freeflag) {
        while(lq->nelem > 0) {
            item = lqueueRemove(lq);
            LEPT_FREE(item);
        }
    } else if (lq->nelem > 0) {
        L_WARNING("memory leak of %d items in lqueue!\n", __func__, lq->nelem);
    }

    if (lq->array)
        LEPT_FREE(lq->array);
    if (lq->stack)
        lstackDestroy(&lq->stack, freeflag);
    LEPT_FREE(lq);
    *plq = NULL;
}


/*--------------------------------------------------------------------------*
 *                                  Accessors                               *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   lqueueAdd()
 *
 * \param[in]    lq     lqueue
 * \param[in]    item   to be added to the tail of the queue
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The algorithm is as follows.  If the queue is populated
 *          to the end of the allocated array, shift all ptrs toward
 *          the beginning of the array, so that the head of the queue
 *          is at the beginning of the array.  Then, if the array is
 *          more than 0.75 full, realloc with double the array size.
 *          Finally, add the item to the tail of the queue.
 * </pre>
 */
l_ok
lqueueAdd(L_QUEUE  *lq,
          void     *item)
{
    if (!lq)
        return ERROR_INT("lq not defined", __func__, 1);
    if (!item)
        return ERROR_INT("item not defined", __func__, 1);

        /* If filled to the end and the ptrs can be shifted to the left,
         * shift them. */
    if ((lq->nhead + lq->nelem >= lq->nalloc) && (lq->nhead != 0)) {
        memmove(lq->array, lq->array + lq->nhead, sizeof(void *) * lq->nelem);
        lq->nhead = 0;
    }

        /* If necessary, expand the allocated array by a factor of 2 */
    if (lq->nelem > 0.75 * lq->nalloc) {
        if (lqueueExtendArray(lq))
            return ERROR_INT("extension failed", __func__, 1);
    }

        /* Now add the item */
    lq->array[lq->nhead + lq->nelem] = (void *)item;
    lq->nelem++;

    return 0;
}


/*!
 * \brief   lqueueExtendArray()
 *
 * \param[in]    lq    lqueue
 * \return  0 if OK, 1 on error
 */
static l_int32
lqueueExtendArray(L_QUEUE  *lq)
{
    if (!lq)
        return ERROR_INT("lq not defined", __func__, 1);

    if ((lq->array = (void **)reallocNew((void **)&lq->array,
                                sizeof(void *) * lq->nalloc,
                                2 * sizeof(void *) * lq->nalloc)) == NULL)
        return ERROR_INT("new ptr array not returned", __func__, 1);

    lq->nalloc = 2 * lq->nalloc;
    return 0;
}


/*!
 * \brief   lqueueRemove()
 *
 * \param[in]    lq   lqueue
 * \return  ptr to item popped from the head of the queue,
 *              or NULL if the queue is empty or on error
 *
 * <pre>
 * Notes:
 *      (1) If this is the last item on the queue, so that the queue
 *          becomes empty, nhead is reset to the beginning of the array.
 * </pre>
 */
void *
lqueueRemove(L_QUEUE  *lq)
{
void  *item;

    if (!lq)
        return (void *)ERROR_PTR("lq not defined", __func__, NULL);

    if (lq->nelem == 0)
        return NULL;
    item = lq->array[lq->nhead];
    lq->array[lq->nhead] = NULL;
    if (lq->nelem == 1)
        lq->nhead = 0;  /* reset head ptr */
    else
        (lq->nhead)++;  /* can't go off end of array because nelem > 1 */
    lq->nelem--;
    return item;
}


/*!
 * \brief   lqueueGetCount()
 *
 * \param[in]    lq   lqueue
 * \return  count, or 0 on error
 */
l_int32
lqueueGetCount(L_QUEUE  *lq)
{
    if (!lq)
        return ERROR_INT("lq not defined", __func__, 0);

    return lq->nelem;
}


/*---------------------------------------------------------------------*
 *                            Debug output                             *
 *---------------------------------------------------------------------*/
/*!
 * \brief   lqueuePrint()
 *
 * \param[in]    fp   file stream
 * \param[in]    lq   lqueue
 * \return  0 if OK; 1 on error
 */
l_ok
lqueuePrint(FILE     *fp,
            L_QUEUE  *lq)
{
l_int32  i;

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!lq)
        return ERROR_INT("lq not defined", __func__, 1);

    fprintf(fp, "\n L_Queue: nalloc = %d, nhead = %d, nelem = %d, array = %p\n",
            lq->nalloc, lq->nhead, lq->nelem, lq->array);
    for (i = lq->nhead; i < lq->nhead + lq->nelem; i++)
    fprintf(fp,   "array[%d] = %p\n", i, lq->array[i]);

    return 0;
}

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
 * \file stack.c
 * <pre>
 *
 *      Generic stack
 *
 *      The lstack is an array of void * ptrs, onto which
 *      objects can be stored.  At any time, the number of
 *      stored objects is lstack->n.  The object at the bottom
 *      of the lstack is at array[0]; the object at the top of
 *      the lstack is at array[n-1].  New objects are added
 *      to the top of the lstack; i.e., the first available
 *      location, which is at array[n].  The lstack is expanded
 *      by doubling, when needed.  Objects are removed
 *      from the top of the lstack.  When an attempt is made
 *      to remove an object from an empty lstack, the result is null.
 *
 *      Create/Destroy
 *           L_STACK        *lstackCreate()
 *           void            lstackDestroy()
 *
 *      Accessors
 *           l_int32         lstackAdd()
 *           void           *lstackRemove()
 *           static l_int32  lstackExtendArray()
 *           l_int32         lstackGetCount()
 *
 *      Text description
 *           l_int32         lstackPrint()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

    /* Bounds on initial array size */
static const l_uint32  MaxPtrArraySize = 100000;
static const l_int32 InitialPtrArraySize = 20;      /*!< n'importe quoi */

    /* Static function */
static l_int32 lstackExtendArray(L_STACK *lstack);

/*---------------------------------------------------------------------*
 *                          Create/Destroy                             *
 *---------------------------------------------------------------------*/
/*!
 * \brief   lstackCreate()
 *
 * \param[in]    n   initial ptr array size; use 0 for default
 * \return  lstack, or NULL on error
 */
L_STACK *
lstackCreate(l_int32  n)
{
L_STACK  *lstack;

    if (n <= 0 || n > (l_int32)MaxPtrArraySize)
        n = InitialPtrArraySize;

    lstack = (L_STACK *)LEPT_CALLOC(1, sizeof(L_STACK));
    lstack->array = (void **)LEPT_CALLOC(n, sizeof(void *));
    if (!lstack->array) {
        lstackDestroy(&lstack, FALSE);
        return (L_STACK *)ERROR_PTR("lstack array not made", __func__, NULL);
    }

    lstack->nalloc = n;
    lstack->n = 0;
    return lstack;
}


/*!
 * \brief   lstackDestroy()
 *
 * \param[in,out]   plstack    will be set to null before returning
 * \param[in]    freeflag TRUE to free each remaining struct in the array
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) If %freeflag is TRUE, frees each struct in the array.
 *      (2) If %freeflag is FALSE but there are elements on the array,
 *          gives a warning and destroys the array.  This will
 *          cause a memory leak of all the items that were on the lstack.
 *          So if the items require their own destroy function, they
 *          must be destroyed before the lstack.
 *      (3) To destroy the lstack, we destroy the ptr array, then
 *          the lstack, and then null the contents of the input ptr.
 * </pre>
 */
void
lstackDestroy(L_STACK  **plstack,
              l_int32    freeflag)
{
void     *item;
L_STACK  *lstack;

    if (plstack == NULL) {
        L_WARNING("ptr address is NULL\n", __func__);
        return;
    }
    if ((lstack = *plstack) == NULL)
        return;

    if (freeflag) {
        while(lstack->n > 0) {
            item = lstackRemove(lstack);
            LEPT_FREE(item);
        }
    } else if (lstack->n > 0) {
        L_WARNING("memory leak of %d items in lstack\n", __func__, lstack->n);
    }

    if (lstack->auxstack)
        lstackDestroy(&lstack->auxstack, freeflag);

    if (lstack->array)
        LEPT_FREE(lstack->array);
    LEPT_FREE(lstack);
    *plstack = NULL;
}



/*---------------------------------------------------------------------*
 *                               Accessors                             *
 *---------------------------------------------------------------------*/
/*!
 * \brief   lstackAdd()
 *
 * \param[in]    lstack
 * \param[in]    item      to be added to the lstack
 * \return  0 if OK; 1 on error.
 */
l_ok
lstackAdd(L_STACK  *lstack,
          void     *item)
{
    if (!lstack)
        return ERROR_INT("lstack not defined", __func__, 1);
    if (!item)
        return ERROR_INT("item not defined", __func__, 1);

        /* Do we need to extend the array? */
    if (lstack->n >= lstack->nalloc) {
        if (lstackExtendArray(lstack))
            return ERROR_INT("extension failed", __func__, 1);
    }

        /* Store the new pointer */
    lstack->array[lstack->n] = (void *)item;
    lstack->n++;

    return 0;
}


/*!
 * \brief   lstackRemove()
 *
 * \param[in]    lstack
 * \return  ptr to item popped from the top of the lstack,
 *              or NULL if the lstack is empty or on error
 */
void *
lstackRemove(L_STACK  *lstack)
{
void  *item;

    if (!lstack)
        return ERROR_PTR("lstack not defined", __func__, NULL);

    if (lstack->n == 0)
        return NULL;

    lstack->n--;
    item = lstack->array[lstack->n];

    return item;
}


/*!
 * \brief   lstackExtendArray()
 *
 * \param[in]    lstack
 * \return  0 if OK; 1 on error
 */
static l_int32
lstackExtendArray(L_STACK  *lstack)
{
    if (!lstack)
        return ERROR_INT("lstack not defined", __func__, 1);

    if ((lstack->array = (void **)reallocNew((void **)&lstack->array,
                              sizeof(void *) * lstack->nalloc,
                              2 * sizeof(void *) * lstack->nalloc)) == NULL)
        return ERROR_INT("new lstack array not defined", __func__, 1);

    lstack->nalloc = 2 * lstack->nalloc;
    return 0;
}


/*!
 * \brief   lstackGetCount()
 *
 * \param[in]    lstack
 * \return  count, or 0 on error
 */
l_int32
lstackGetCount(L_STACK  *lstack)
{
    if (!lstack)
        return ERROR_INT("lstack not defined", __func__, 1);

    return lstack->n;
}



/*---------------------------------------------------------------------*
 *                            Debug output                             *
 *---------------------------------------------------------------------*/
/*!
 * \brief   lstackPrint()
 *
 * \param[in]    fp       file stream
 * \param[in]    lstack
 * \return  0 if OK; 1 on error
 */
l_ok
lstackPrint(FILE     *fp,
            L_STACK  *lstack)
{
l_int32  i;

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!lstack)
        return ERROR_INT("lstack not defined", __func__, 1);

    fprintf(fp, "\n Stack: nalloc = %d, n = %d, array = %p\n",
            lstack->nalloc, lstack->n, lstack->array);
    for (i = 0; i < lstack->n; i++)
        fprintf(fp,   "array[%d] = %p\n", i, lstack->array[i]);

    return 0;
}

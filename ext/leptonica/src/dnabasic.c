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
 * \file  dnabasic.c
 * <pre>
 *
 *      Dna creation, destruction, copy, clone, etc.
 *          L_DNA       *l_dnaCreate()
 *          L_DNA       *l_dnaCreateFromIArray()
 *          L_DNA       *l_dnaCreateFromDArray()
 *          L_DNA       *l_dnaMakeSequence()
 *          void        *l_dnaDestroy()
 *          L_DNA       *l_dnaCopy()
 *          L_DNA       *l_dnaClone()
 *          l_int32      l_dnaEmpty()
 *
 *      Dna: add/remove number and extend array
 *          l_int32      l_dnaAddNumber()
 *          static l_int32  l_dnaExtendArray()
 *          l_int32      l_dnaInsertNumber()
 *          l_int32      l_dnaRemoveNumber()
 *          l_int32      l_dnaReplaceNumber()
 *
 *      Dna accessors
 *          l_int32      l_dnaGetCount()
 *          l_int32      l_dnaSetCount()
 *          l_int32      l_dnaGetIValue()
 *          l_int32      l_dnaGetDValue()
 *          l_int32      l_dnaSetValue()
 *          l_int32      l_dnaShiftValue()
 *          l_int32     *l_dnaGetIArray()
 *          l_float64   *l_dnaGetDArray()
 *          l_int32      l_dnaGetParameters()
 *          l_int32      l_dnaSetParameters()
 *          l_int32      l_dnaCopyParameters()
 *
 *      Serialize Dna for I/O
 *          L_DNA       *l_dnaRead()
 *          L_DNA       *l_dnaReadStream()
 *          L_DNA       *l_dnaReadMem()
 *          l_int32      l_dnaWrite()
 *          l_int32      l_dnaWriteStream()
 *          l_int32      l_dnaWriteStderr()
 *          l_int32      l_dnaWriteMem()
 *
 *      Dnaa creation, destruction
 *          L_DNAA      *l_dnaaCreate()
 *          L_DNAA      *l_dnaaCreateFull()
 *          l_int32      l_dnaaTruncate()
 *          void        *l_dnaaDestroy()
 *
 *      Add Dna to Dnaa
 *          l_int32      l_dnaaAddDna()
 *          static l_int32  l_dnaaExtendArray()
 *
 *      Dnaa accessors
 *          l_int32      l_dnaaGetCount()
 *          l_int32      l_dnaaGetDnaCount()
 *          l_int32      l_dnaaGetNumberCount()
 *          L_DNA       *l_dnaaGetDna()
 *          L_DNA       *l_dnaaReplaceDna()
 *          l_int32      l_dnaaGetValue()
 *          l_int32      l_dnaaAddNumber()
 *
 *      Serialize Dnaa for I/O
 *          L_DNAA      *l_dnaaRead()
 *          L_DNAA      *l_dnaaReadStream()
 *          L_DNAA      *l_dnaaReadMem()
 *          l_int32      l_dnaaWrite()
 *          l_int32      l_dnaaWriteStream()
 *          l_int32      l_dnaaWriteMem()
 *
 *    (1) The Dna is a struct holding an array of doubles.  It can also
 *        be used to store l_int32 values, up to the full precision
 *        of int32.  Always use it whenever integers larger than a
 *        few million need to be stored.
 *
 *    (2) Always use the accessors in this file, never the fields directly.
 *
 *    (3) Storing and retrieving numbers:
 *
 *       * to append a new number to the array, use l_dnaAddNumber().  If
 *         the number is an int, it will will automatically be converted
 *         to l_float64 and stored.
 *
 *       * to reset a value stored in the array, use l_dnaSetValue().
 *
 *       * to increment or decrement a value stored in the array,
 *         use l_dnaShiftValue().
 *
 *       * to obtain a value from the array, use either l_dnaGetIValue()
 *         or l_dnaGetDValue(), depending on whether you are retrieving
 *         an integer or a float64.  This avoids doing an explicit cast,
 *         such as
 *           (a) return a l_float64 and cast it to an l_int32
 *           (b) cast the return directly to (l_float64 *) to
 *               satisfy the function prototype, as in
 *                 l_dnaGetDValue(da, index, (l_float64 *)&ival);   [ugly!]
 *
 *    (4) int <--> double conversions:
 *
 *        Conversions go automatically from l_int32 --> l_float64,
 *        without loss of precision.  You must cast (l_int32)
 *        to go from l_float64 --> l_int32 because you're truncating
 *        to the integer value.
 *
 *    (5) As with other arrays in leptonica, the l_dna has both an allocated
 *        size and a count of the stored numbers.  When you add a number, it
 *        goes on the end of the array, and causes a realloc if the array
 *        is already filled.  However, in situations where you want to
 *        add numbers randomly into an array, such as when you build a
 *        histogram, you must set the count of stored numbers in advance.
 *        This is done with l_dnaSetCount().  If you set a count larger
 *        than the allocated array, it does a realloc to the size requested.
 *
 *    (6) In situations where the data in a l_dna correspond to a function
 *        y(x), the values can be either at equal spacings in x or at
 *        arbitrary spacings.  For the former, we can represent all x values
 *        by two parameters: startx (corresponding to y[0]) and delx
 *        for the change in x for adjacent values y[i] and y[i+1].
 *        startx and delx are initialized to 0.0 and 1.0, rsp.
 *        For arbitrary spacings, we use a second l_dna, and the two
 *        l_dnas are typically denoted dnay and dnax.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include <math.h>
#include "allheaders.h"
#include "array_internal.h"

    /* Bounds on initial array size */
static const l_uint32  MaxDoubleArraySize = 100000000;   /* for dna */
static const l_uint32  MaxPtrArraySize = 1000000;   /* for dnaa */
static const l_int32  InitialArraySize = 50;      /*!< n'importe quoi */

    /* Static functions */
static l_int32 l_dnaExtendArray(L_DNA *da);
static l_int32 l_dnaaExtendArray(L_DNAA *daa);

/*--------------------------------------------------------------------------*
 *                 Dna creation, destruction, copy, clone, etc.             *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   l_dnaCreate()
 *
 * \param[in]    n   size of number array to be alloc'd; 0 for default
 * \return  da, or NULL on error
 */
L_DNA *
l_dnaCreate(l_int32  n)
{
L_DNA  *da;

    if (n <= 0 || n > MaxDoubleArraySize)
        n = InitialArraySize;

    da = (L_DNA *)LEPT_CALLOC(1, sizeof(L_DNA));
    if ((da->array = (l_float64 *)LEPT_CALLOC(n, sizeof(l_float64))) == NULL) {
        l_dnaDestroy(&da);
        return (L_DNA *)ERROR_PTR("double array not made", __func__, NULL);
    }

    da->nalloc = n;
    da->n = 0;
    da->refcount = 1;
    da->startx = 0.0;
    da->delx = 1.0;

    return da;
}


/*!
 * \brief   l_dnaCreateFromIArray()
 *
 * \param[in]    iarray   integer array
 * \param[in]    size     of the array
 * \return  da, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) We can't insert this int array into the l_dna, because a l_dna
 *          takes a double array.  So this just copies the data from the
 *          input array into the l_dna.  The input array continues to be
 *          owned by the caller.
 * </pre>
 */
L_DNA *
l_dnaCreateFromIArray(l_int32  *iarray,
                      l_int32   size)
{
l_int32  i;
L_DNA   *da;

    if (!iarray)
        return (L_DNA *)ERROR_PTR("iarray not defined", __func__, NULL);
    if (size <= 0)
        return (L_DNA *)ERROR_PTR("size must be > 0", __func__, NULL);

    da = l_dnaCreate(size);
    for (i = 0; i < size; i++)
        l_dnaAddNumber(da, iarray[i]);

    return da;
}


/*!
 * \brief   l_dnaCreateFromDArray()
 *
 * \param[in]    darray     float
 * \param[in]    size       of the array
 * \param[in]    copyflag   L_INSERT or L_COPY
 * \return  da, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) With L_INSERT, ownership of the input array is transferred
 *          to the returned l_dna, and all %size elements are considered
 *          to be valid.
 * </pre>
 */
L_DNA *
l_dnaCreateFromDArray(l_float64  *darray,
                      l_int32     size,
                      l_int32     copyflag)
{
l_int32  i;
L_DNA   *da;

    if (!darray)
        return (L_DNA *)ERROR_PTR("darray not defined", __func__, NULL);
    if (size <= 0)
        return (L_DNA *)ERROR_PTR("size must be > 0", __func__, NULL);
    if (copyflag != L_INSERT && copyflag != L_COPY)
        return (L_DNA *)ERROR_PTR("invalid copyflag", __func__, NULL);

    da = l_dnaCreate(size);
    if (copyflag == L_INSERT) {
        if (da->array) LEPT_FREE(da->array);
        da->array = darray;
        da->n = size;
    } else {  /* just copy the contents */
        for (i = 0; i < size; i++)
            l_dnaAddNumber(da, darray[i]);
    }

    return da;
}


/*!
 * \brief   l_dnaMakeSequence()
 *
 * \param[in]    startval
 * \param[in]    increment
 * \param[in]    size       of sequence
 * \return  l_dna of sequence of evenly spaced values, or NULL on error
 */
L_DNA *
l_dnaMakeSequence(l_float64  startval,
                  l_float64  increment,
                  l_int32    size)
{
l_int32    i;
l_float64  val;
L_DNA     *da;

    if ((da = l_dnaCreate(size)) == NULL)
        return (L_DNA *)ERROR_PTR("da not made", __func__, NULL);

    for (i = 0; i < size; i++) {
        val = startval + i * increment;
        l_dnaAddNumber(da, val);
    }

    return da;
}


/*!
 * \brief   l_dnaDestroy()
 *
 * \param[in,out]   pda   will be set to null before returning
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) Decrements the ref count and, if 0, destroys the l_dna.
 *      (2) Always nulls the input ptr.
 * </pre>
 */
void
l_dnaDestroy(L_DNA  **pda)
{
L_DNA  *da;

    if (pda == NULL) {
        L_WARNING("ptr address is NULL\n", __func__);
        return;
    }

    if ((da = *pda) == NULL)
        return;

        /* Decrement the ref count.  If it is 0, destroy the l_dna. */
    if (--da->refcount == 0) {
        if (da->array)
            LEPT_FREE(da->array);
        LEPT_FREE(da);
    }
    *pda = NULL;
}


/*!
 * \brief   l_dnaCopy()
 *
 * \param[in]    da
 * \return  copy of da, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This removes unused ptrs above da->n.
 * </pre>
 */
L_DNA *
l_dnaCopy(L_DNA  *da)
{
l_int32  i;
L_DNA   *dac;

    if (!da)
        return (L_DNA *)ERROR_PTR("da not defined", __func__, NULL);

    if ((dac = l_dnaCreate(da->n)) == NULL)
        return (L_DNA *)ERROR_PTR("dac not made", __func__, NULL);
    dac->startx = da->startx;
    dac->delx = da->delx;

    for (i = 0; i < da->n; i++)
        l_dnaAddNumber(dac, da->array[i]);

    return dac;
}


/*!
 * \brief   l_dnaClone()
 *
 * \param[in]    da
 * \return  ptr to same da, or NULL on error
 */
L_DNA *
l_dnaClone(L_DNA  *da)
{
    if (!da)
        return (L_DNA *)ERROR_PTR("da not defined", __func__, NULL);

    ++da->refcount;
    return da;
}


/*!
 * \brief   l_dnaEmpty()
 *
 * \param[in]    da
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This does not change the allocation of the array.
 *          It just clears the number of stored numbers, so that
 *          the array appears to be empty.
 * </pre>
 */
l_ok
l_dnaEmpty(L_DNA  *da)
{
    if (!da)
        return ERROR_INT("da not defined", __func__, 1);

    da->n = 0;
    return 0;
}



/*--------------------------------------------------------------------------*
 *                  Dna: add/remove number and extend array                 *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   l_dnaAddNumber()
 *
 * \param[in]    da
 * \param[in]    val   float or int to be added; stored as a float
 * \return  0 if OK, 1 on error
 */
l_ok
l_dnaAddNumber(L_DNA     *da,
               l_float64  val)
{
l_int32  n;

    if (!da)
        return ERROR_INT("da not defined", __func__, 1);

    n = l_dnaGetCount(da);
    if (n >= da->nalloc) {
        if (l_dnaExtendArray(da))
            return ERROR_INT("extension failed", __func__, 1);
    }
    da->array[n] = val;
    da->n++;
    return 0;
}


/*!
 * \brief   l_dnaExtendArray()
 *
 * \param[in]    da
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Doubles the size of the array.
 *      (2) The max number of doubles is 100M.
 * </pre>
 */
static l_int32
l_dnaExtendArray(L_DNA  *da)
{
size_t  oldsize, newsize;

    if (!da)
        return ERROR_INT("da not defined", __func__, 1);
    if (da->nalloc > MaxDoubleArraySize)
        return ERROR_INT("da at maximum size; can't extend", __func__, 1);
    oldsize = da->nalloc * sizeof(l_float64);
    if (da->nalloc > MaxDoubleArraySize / 2) {
        newsize = MaxDoubleArraySize * sizeof(l_float64);
        da->nalloc = MaxDoubleArraySize;
    } else {
        newsize = 2 * oldsize;
        da->nalloc *= 2;
    }
    if ((da->array = (l_float64 *)reallocNew((void **)&da->array,
                                             oldsize, newsize)) == NULL)
        return ERROR_INT("new ptr array not returned", __func__, 1);

    return 0;
}


/*!
 * \brief   l_dnaInsertNumber()
 *
 * \param[in]    da
 * \param[in]    index   location in da to insert new value
 * \param[in]    val     float64 or integer to be added
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This shifts da[i] --> da[i + 1] for all i >= %index,
 *          and then inserts %val as da[%index].
 *      (2) It should not be used repeatedly on large arrays,
 *          because the function is O(n).
 *
 * </pre>
 */
l_ok
l_dnaInsertNumber(L_DNA      *da,
                  l_int32    index,
                  l_float64  val)
{
l_int32  i, n;

    if (!da)
        return ERROR_INT("da not defined", __func__, 1);
    n = l_dnaGetCount(da);
    if (index < 0 || index > n) {
        L_ERROR("index %d not in [0,...,%d]\n", __func__, index, n);
        return 1;
    }

    if (n >= da->nalloc) {
        if (l_dnaExtendArray(da))
            return ERROR_INT("extension failed", __func__, 1);
    }
    for (i = n; i > index; i--)
        da->array[i] = da->array[i - 1];
    da->array[index] = val;
    da->n++;
    return 0;
}


/*!
 * \brief   l_dnaRemoveNumber()
 *
 * \param[in]    da
 * \param[in]    index    element to be removed
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This shifts da[i] --> da[i - 1] for all i > %index.
 *      (2) It should not be used repeatedly on large arrays,
 *          because the function is O(n).
 * </pre>
 */
l_ok
l_dnaRemoveNumber(L_DNA   *da,
                  l_int32  index)
{
l_int32  i, n;

    if (!da)
        return ERROR_INT("da not defined", __func__, 1);
    n = l_dnaGetCount(da);
    if (index < 0 || index >= n) {
        L_ERROR("index %d not in [0,...,%d]\n", __func__, index, n - 1);
        return 1;
    }

    for (i = index + 1; i < n; i++)
        da->array[i - 1] = da->array[i];
    da->n--;
    return 0;
}


/*!
 * \brief   l_dnaReplaceNumber()
 *
 * \param[in]    da
 * \param[in]    index    element to be replaced
 * \param[in]    val      new value to replace old one
 * \return  0 if OK, 1 on error
 */
l_ok
l_dnaReplaceNumber(L_DNA     *da,
                   l_int32    index,
                   l_float64  val)
{
l_int32  n;

    if (!da)
        return ERROR_INT("da not defined", __func__, 1);
    n = l_dnaGetCount(da);
    if (index < 0 || index >= n) {
        L_ERROR("index %d not in [0,...,%d]\n", __func__, index, n - 1);
        return 1;
    }

    da->array[index] = val;
    return 0;
}


/*----------------------------------------------------------------------*
 *                             Dna accessors                            *
 *----------------------------------------------------------------------*/
/*!
 * \brief   l_dnaGetCount()
 *
 * \param[in]    da
 * \return  count, or 0 if no numbers or on error
 */
l_int32
l_dnaGetCount(L_DNA  *da)
{
    if (!da)
        return ERROR_INT("da not defined", __func__, 0);
    return da->n;
}


/*!
 * \brief   l_dnaSetCount()
 *
 * \param[in]    da
 * \param[in]    newcount
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If %newcount <= da->nalloc, this resets da->n.
 *          Using %newcount = 0 is equivalent to l_dnaEmpty().
 *      (2) If %newcount > da->nalloc, this causes a realloc
 *          to a size da->nalloc = %newcount.
 *      (3) All the previously unused values in da are set to 0.0.
 * </pre>
 */
l_ok
l_dnaSetCount(L_DNA   *da,
              l_int32  newcount)
{
    if (!da)
        return ERROR_INT("da not defined", __func__, 1);
    if (newcount > da->nalloc) {
        if ((da->array = (l_float64 *)reallocNew((void **)&da->array,
                         sizeof(l_float64) * da->nalloc,
                         sizeof(l_float64) * newcount)) == NULL)
            return ERROR_INT("new ptr array not returned", __func__, 1);
        da->nalloc = newcount;
    }
    da->n = newcount;
    return 0;
}


/*!
 * \brief   l_dnaGetDValue()
 *
 * \param[in]    da
 * \param[in]    index    into l_dna
 * \param[out]   pval     double value; 0.0 on error
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Caller may need to check the function return value to
 *          decide if a 0.0 in the returned ival is valid.
 * </pre>
 */
l_ok
l_dnaGetDValue(L_DNA      *da,
               l_int32     index,
               l_float64  *pval)
{
    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0.0;
    if (!da)
        return ERROR_INT("da not defined", __func__, 1);

    if (index < 0 || index >= da->n)
        return ERROR_INT("index not valid", __func__, 1);

    *pval = da->array[index];
    return 0;
}


/*!
 * \brief   l_dnaGetIValue()
 *
 * \param[in]    da
 * \param[in]    index    into l_dna
 * \param[out]   pival    integer value; 0 on error
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Caller may need to check the function return value to
 *          decide if a 0 in the returned ival is valid.
 * </pre>
 */
l_ok
l_dnaGetIValue(L_DNA    *da,
               l_int32   index,
               l_int32  *pival)
{
l_float64  val;

    if (!pival)
        return ERROR_INT("&ival not defined", __func__, 1);
    *pival = 0;
    if (!da)
        return ERROR_INT("da not defined", __func__, 1);

    if (index < 0 || index >= da->n)
        return ERROR_INT("index not valid", __func__, 1);

    val = da->array[index];
    *pival = (l_int32)(val + L_SIGN(val) * 0.5);
    return 0;
}


/*!
 * \brief   l_dnaSetValue()
 *
 * \param[in]    da
 * \param[in]    index    to element to be set
 * \param[in]    val      to set element
 * \return  0 if OK; 1 on error
 */
l_ok
l_dnaSetValue(L_DNA     *da,
              l_int32    index,
              l_float64  val)
{
    if (!da)
        return ERROR_INT("da not defined", __func__, 1);
    if (index < 0 || index >= da->n)
        return ERROR_INT("index not valid", __func__, 1);

    da->array[index] = val;
    return 0;
}


/*!
 * \brief   l_dnaShiftValue()
 *
 * \param[in]    da
 * \param[in]    index   to element to change relative to the current value
 * \param[in]    diff    increment if diff > 0 or decrement if diff < 0
 * \return  0 if OK; 1 on error
 */
l_ok
l_dnaShiftValue(L_DNA     *da,
                l_int32    index,
                l_float64  diff)
{
    if (!da)
        return ERROR_INT("da not defined", __func__, 1);
    if (index < 0 || index >= da->n)
        return ERROR_INT("index not valid", __func__, 1);

    da->array[index] += diff;
    return 0;
}


/*!
 * \brief   l_dnaGetIArray()
 *
 * \param[in]    da
 * \return  a copy of the bare internal array, integerized
 *              by rounding, or NULL on error
 * <pre>
 * Notes:
 *      (1) A copy of the array is made, because we need to
 *          generate an integer array from the bare double array.
 *          The caller is responsible for freeing the array.
 *      (2) The array size is determined by the number of stored numbers,
 *          not by the size of the allocated array in the l_dna.
 *      (3) This function is provided to simplify calculations
 *          using the bare internal array, rather than continually
 *          calling accessors on the l_dna.  It is typically used
 *          on an array of size 256.
 * </pre>
 */
l_int32 *
l_dnaGetIArray(L_DNA  *da)
{
l_int32   i, n, ival;
l_int32  *array;

    if (!da)
        return (l_int32 *)ERROR_PTR("da not defined", __func__, NULL);

    n = l_dnaGetCount(da);
    if ((array = (l_int32 *)LEPT_CALLOC(n, sizeof(l_int32))) == NULL)
        return (l_int32 *)ERROR_PTR("array not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        l_dnaGetIValue(da, i, &ival);
        array[i] = ival;
    }

    return array;
}


/*!
 * \brief   l_dnaGetDArray()
 *
 * \param[in]    da
 * \param[in]    copyflag   L_NOCOPY or L_COPY
 * \return  either the bare internal array or a copy of it, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If %copyflag == L_COPY, it makes a copy which the caller
 *          is responsible for freeing.  Otherwise, it operates
 *          directly on the bare array of the l_dna.
 *      (2) Very important: for L_NOCOPY, any writes to the array
 *          will be in the l_dna.  Do not write beyond the size of
 *          the count field, because it will not be accessible
 *          from the l_dna!  If necessary, be sure to set the count
 *          field to a larger number (such as the alloc size)
 *          BEFORE calling this function.  Creating with l_dnaMakeConstant()
 *          is another way to insure full initialization.
 * </pre>
 */
l_float64 *
l_dnaGetDArray(L_DNA   *da,
               l_int32  copyflag)
{
l_int32     i, n;
l_float64  *array;

    if (!da)
        return (l_float64 *)ERROR_PTR("da not defined", __func__, NULL);

    if (copyflag == L_NOCOPY) {
        array = da->array;
    } else {  /* copyflag == L_COPY */
        n = l_dnaGetCount(da);
        if ((array = (l_float64 *)LEPT_CALLOC(n, sizeof(l_float64))) == NULL)
            return (l_float64 *)ERROR_PTR("array not made", __func__, NULL);
        for (i = 0; i < n; i++)
            array[i] = da->array[i];
    }

    return array;
}


/*!
 * \brief   l_dnaGetParameters()
 *
 * \param[in]    da
 * \param[out]   pstartx   [optional] startx
 * \param[out]   pdelx     [optional] delx
 * \return  0 if OK, 1 on error
 */
l_ok
l_dnaGetParameters(L_DNA     *da,
                   l_float64  *pstartx,
                   l_float64  *pdelx)
{
    if (pstartx) *pstartx = 0.0;
    if (pdelx) *pdelx = 1.0;
    if (!pstartx && !pdelx)
        return ERROR_INT("neither &startx nor &delx are defined", __func__, 1);
    if (!da)
        return ERROR_INT("da not defined", __func__, 1);

    if (pstartx) *pstartx = da->startx;
    if (pdelx) *pdelx = da->delx;
    return 0;
}


/*!
 * \brief   l_dnaSetParameters()
 *
 * \param[in]    da
 * \param[in]    startx   x value corresponding to da[0]
 * \param[in]    delx     difference in x values for the situation where the
 *                        elements of da correspond to the evaluation of a
 *                        function at equal intervals of size %delx
 * \return  0 if OK, 1 on error
 */
l_ok
l_dnaSetParameters(L_DNA     *da,
                   l_float64  startx,
                   l_float64  delx)
{
    if (!da)
        return ERROR_INT("da not defined", __func__, 1);

    da->startx = startx;
    da->delx = delx;
    return 0;
}


/*!
 * \brief   l_dnaCopyParameters()
 *
 * \param[in]    dad    destination DNuma
 * \param[in]    das    source DNuma
 * \return  0 if OK, 1 on error
 */
l_ok
l_dnaCopyParameters(L_DNA  *dad,
                    L_DNA  *das)
{
l_float64  start, binsize;

    if (!das || !dad)
        return ERROR_INT("das and dad not both defined", __func__, 1);

    l_dnaGetParameters(das, &start, &binsize);
    l_dnaSetParameters(dad, start, binsize);
    return 0;
}


/*----------------------------------------------------------------------*
 *                        Serialize Dna for I/O                         *
 *----------------------------------------------------------------------*/
/*!
 * \brief   l_dnaRead()
 *
 * \param[in]    filename
 * \return  da, or NULL on error
 */
L_DNA *
l_dnaRead(const char  *filename)
{
FILE   *fp;
L_DNA  *da;

    if (!filename)
        return (L_DNA *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (L_DNA *)ERROR_PTR_1("stream not opened",
                                    filename, __func__, NULL);
    da = l_dnaReadStream(fp);
    fclose(fp);
    if (!da)
        return (L_DNA *)ERROR_PTR_1("da not read",
                                    filename, __func__, NULL);
    return da;
}


/*!
 * \brief   l_dnaReadStream()
 *
 * \param[in]    fp    file stream
 * \return  da, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) fscanf takes %lf to read a double; fprintf takes %f to write it.
 *      (2) It is OK for the dna to be empty.
 * </pre>
 */
L_DNA *
l_dnaReadStream(FILE  *fp)
{
l_int32    i, n, index, ret, version;
l_float64  val, startx, delx;
L_DNA     *da;

    if (!fp)
        return (L_DNA *)ERROR_PTR("stream not defined", __func__, NULL);

    ret = fscanf(fp, "\nL_Dna Version %d\n", &version);
    if (ret != 1)
        return (L_DNA *)ERROR_PTR("not a l_dna file", __func__, NULL);
    if (version != DNA_VERSION_NUMBER)
        return (L_DNA *)ERROR_PTR("invalid l_dna version", __func__, NULL);
    if (fscanf(fp, "Number of numbers = %d\n", &n) != 1)
        return (L_DNA *)ERROR_PTR("invalid number of numbers", __func__, NULL);
    if (n < 0)
        return (L_DNA *)ERROR_PTR("num doubles < 0", __func__, NULL);
    if (n > MaxDoubleArraySize)
        return (L_DNA *)ERROR_PTR("too many doubles", __func__, NULL);
    if (n == 0) L_INFO("the dna is empty\n", __func__);

    if ((da = l_dnaCreate(n)) == NULL)
        return (L_DNA *)ERROR_PTR("da not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        if (fscanf(fp, "  [%d] = %lf\n", &index, &val) != 2) {
            l_dnaDestroy(&da);
            return (L_DNA *)ERROR_PTR("bad input data", __func__, NULL);
        }
        l_dnaAddNumber(da, val);
    }

        /* Optional data */
    if (fscanf(fp, "startx = %lf, delx = %lf\n", &startx, &delx) == 2)
        l_dnaSetParameters(da, startx, delx);
    return da;
}


/*!
 * \brief   l_dnaReadMem()
 *
 * \param[in]    data    dna serialization; in ascii
 * \param[in]    size    of data; can use strlen to get it
 * \return  da, or NULL on error
 */
L_DNA *
l_dnaReadMem(const l_uint8  *data,
             size_t          size)
{
FILE   *fp;
L_DNA  *da;

    if (!data)
        return (L_DNA *)ERROR_PTR("data not defined", __func__, NULL);
    if ((fp = fopenReadFromMemory(data, size)) == NULL)
        return (L_DNA *)ERROR_PTR("stream not opened", __func__, NULL);

    da = l_dnaReadStream(fp);
    fclose(fp);
    if (!da) L_ERROR("dna not read\n", __func__);
    return da;
}


/*!
 * \brief   l_dnaWrite()
 *
 * \param[in]    filename
 * \param[in]    da
 * \return  0 if OK, 1 on error
 */
l_ok
l_dnaWrite(const char  *filename,
           L_DNA       *da)
{
l_int32  ret;
FILE    *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!da)
        return ERROR_INT("da not defined", __func__, 1);

    if ((fp = fopenWriteStream(filename, "w")) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    ret = l_dnaWriteStream(fp, da);
    fclose(fp);
    if (ret)
        return ERROR_INT_1("da not written to stream", filename, __func__, 1);
    return 0;
}


/*!
 * \brief   l_dnaWriteStream()
 *
 * \param[in]    fp    file stream; use NULL to write to stderr
 * \param[in]    da
 * \return  0 if OK, 1 on error
 */
l_ok
l_dnaWriteStream(FILE   *fp,
                 L_DNA  *da)
{
l_int32    i, n;
l_float64  startx, delx;

    if (!da)
        return ERROR_INT("da not defined", __func__, 1);
    if (!fp)
        return l_dnaWriteStderr(da);

    n = l_dnaGetCount(da);
    fprintf(fp, "\nL_Dna Version %d\n", DNA_VERSION_NUMBER);
    fprintf(fp, "Number of numbers = %d\n", n);
    for (i = 0; i < n; i++)
        fprintf(fp, "  [%d] = %f\n", i, da->array[i]);
    fprintf(fp, "\n");

        /* Optional data */
    l_dnaGetParameters(da, &startx, &delx);
    if (startx != 0.0 || delx != 1.0)
        fprintf(fp, "startx = %f, delx = %f\n", startx, delx);

    return 0;
}


/*!
 * \brief   l_dnaWriteStrderr()
 *
 * \param[in]    da
 * \return  0 if OK, 1 on error
 */
l_ok
l_dnaWriteStderr(L_DNA  *da)
{
l_int32    i, n;
l_float64  startx, delx;

    if (!da)
        return ERROR_INT("da not defined", __func__, 1);

    n = l_dnaGetCount(da);
    lept_stderr("\nL_Dna Version %d\n", DNA_VERSION_NUMBER);
    lept_stderr("Number of numbers = %d\n", n);
    for (i = 0; i < n; i++)
        lept_stderr("  [%d] = %f\n", i, da->array[i]);
    lept_stderr("\n");

        /* Optional data */
    l_dnaGetParameters(da, &startx, &delx);
    if (startx != 0.0 || delx != 1.0)
        lept_stderr("startx = %f, delx = %f\n", startx, delx);

    return 0;
}


/*!
 * \brief   l_dnaWriteMem()
 *
 * \param[out]   pdata    data of serialized dna; ascii
 * \param[out]   psize    size of returned data
 * \param[in]    da
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Serializes a dna in memory and puts the result in a buffer.
 * </pre>
 */
l_ok
l_dnaWriteMem(l_uint8  **pdata,
              size_t    *psize,
              L_DNA     *da)
{
l_int32  ret;
FILE    *fp;

    if (pdata) *pdata = NULL;
    if (psize) *psize = 0;
    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1);
    if (!da)
        return ERROR_INT("da not defined", __func__, 1);

#if HAVE_FMEMOPEN
    if ((fp = open_memstream((char **)pdata, psize)) == NULL)
        return ERROR_INT("stream not opened", __func__, 1);
    ret = l_dnaWriteStream(fp, da);
    fputc('\0', fp);
    fclose(fp);
    if (*psize > 0) *psize = *psize - 1;
#else
    L_INFO("no fmemopen API --> work-around: write to temp file\n", __func__);
  #ifdef _WIN32
    if ((fp = fopenWriteWinTempfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #else
    if ((fp = tmpfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #endif  /* _WIN32 */
    ret = l_dnaWriteStream(fp, da);
    rewind(fp);
    *pdata = l_binaryReadStream(fp, psize);
    fclose(fp);
#endif  /* HAVE_FMEMOPEN */
    return ret;
}


/*--------------------------------------------------------------------------*
 *                       Dnaa creation, destruction                         *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   l_dnaaCreate()
 *
 * \param[in]    n    size of l_dna ptr array to be alloc'd 0 for default
 * \return  daa, or NULL on error
 *
 */
L_DNAA *
l_dnaaCreate(l_int32  n)
{
L_DNAA  *daa;

    if (n <= 0 || n > MaxPtrArraySize)
        n = InitialArraySize;

    daa = (L_DNAA *)LEPT_CALLOC(1, sizeof(L_DNAA));
    if ((daa->dna = (L_DNA **)LEPT_CALLOC(n, sizeof(L_DNA *))) == NULL) {
        l_dnaaDestroy(&daa);
        return (L_DNAA *)ERROR_PTR("l_dna ptr array not made", __func__, NULL);
    }
    daa->nalloc = n;
    daa->n = 0;
    return daa;
}


/*!
 * \brief   l_dnaaCreateFull()
 *
 * \param[in]    nptr  size of dna ptr array to be alloc'd
 * \param[in]    n     size of individual dna arrays to be alloc'd 0 for default
 * \return  daa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This allocates a dnaa and fills the array with allocated dnas.
 *          In use, after calling this function, use
 *              l_dnaaAddNumber(dnaa, index, val);
 *          to add val to the index-th dna in dnaa.
 * </pre>
 */
L_DNAA *
l_dnaaCreateFull(l_int32  nptr,
                 l_int32  n)
{
l_int32  i;
L_DNAA  *daa;
L_DNA   *da;

    daa = l_dnaaCreate(nptr);
    for (i = 0; i < nptr; i++) {
        da = l_dnaCreate(n);
        l_dnaaAddDna(daa, da, L_INSERT);
    }

    return daa;
}


/*!
 * \brief   l_dnaaTruncate()
 *
 * \param[in]    daa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This identifies the largest index containing a dna that
 *          has any numbers within it, destroys all dna beyond that
 *          index, and resets the count.
 * </pre>
 */
l_ok
l_dnaaTruncate(L_DNAA  *daa)
{
l_int32  i, n, nn;
L_DNA   *da;

    if (!daa)
        return ERROR_INT("daa not defined", __func__, 1);

    n = l_dnaaGetCount(daa);
    for (i = n - 1; i >= 0; i--) {
        da = l_dnaaGetDna(daa, i, L_CLONE);
        if (!da)
            continue;
        nn = l_dnaGetCount(da);
        l_dnaDestroy(&da);  /* the clone */
        if (nn == 0)
            l_dnaDestroy(&daa->dna[i]);
        else
            break;
    }
    daa->n = i + 1;
    return 0;
}


/*!
 * \brief   l_dnaaDestroy()
 *
 * \param[in,out]   pdaa    will be set to null before returning
 * \return  void
 */
void
l_dnaaDestroy(L_DNAA  **pdaa)
{
l_int32  i;
L_DNAA  *daa;

    if (pdaa == NULL) {
        L_WARNING("ptr address is NULL!\n", __func__);
        return;
    }

    if ((daa = *pdaa) == NULL)
        return;

    for (i = 0; i < daa->n; i++)
        l_dnaDestroy(&daa->dna[i]);
    LEPT_FREE(daa->dna);
    LEPT_FREE(daa);
    *pdaa = NULL;
}


/*--------------------------------------------------------------------------*
 *                             Add Dna to Dnaa                              *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   l_dnaaAddDna()
 *
 * \param[in]    daa
 * \param[in]    da         to be added
 * \param[in]    copyflag   L_INSERT, L_COPY, L_CLONE
 * \return  0 if OK, 1 on error
 */
l_ok
l_dnaaAddDna(L_DNAA  *daa,
             L_DNA   *da,
             l_int32  copyflag)
{
l_int32  n;
L_DNA   *dac;

    if (!daa)
        return ERROR_INT("daa not defined", __func__, 1);
    if (!da)
        return ERROR_INT("da not defined", __func__, 1);

    if (copyflag == L_INSERT) {
        dac = da;
    } else if (copyflag == L_COPY) {
        if ((dac = l_dnaCopy(da)) == NULL)
            return ERROR_INT("dac not made", __func__, 1);
    } else if (copyflag == L_CLONE) {
        dac = l_dnaClone(da);
    } else {
        return ERROR_INT("invalid copyflag", __func__, 1);
    }

    n = l_dnaaGetCount(daa);
    if (n >= daa->nalloc) {
        if (l_dnaaExtendArray(daa)) {
            if (copyflag != L_INSERT)
                l_dnaDestroy(&dac);
            return ERROR_INT("extension failed", __func__, 1);
        }
    }
    daa->dna[n] = dac;
    daa->n++;
    return 0;
}


/*!
 * \brief   l_dnaaExtendArray()
 *
 * \param[in]    daa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Doubles the number of dna ptrs.
 *      (2) The max size of the dna array is 1M ptrs.
 * </pre>
 */
static l_int32
l_dnaaExtendArray(L_DNAA  *daa)
{
size_t  oldsize, newsize;

    if (!daa)
        return ERROR_INT("daa not defined", __func__, 1);
    if (daa->nalloc > MaxPtrArraySize)  /* belt & suspenders */
        return ERROR_INT("daa has too many ptrs", __func__, 1);
    oldsize = daa->nalloc * sizeof(L_DNA *);
    newsize = 2 * oldsize;
    if (newsize > 8 * MaxPtrArraySize)
        return ERROR_INT("newsize > 8 MB; too large", __func__, 1);

    if ((daa->dna = (L_DNA **)reallocNew((void **)&daa->dna,
                                         oldsize, newsize)) == NULL)
        return ERROR_INT("new ptr array not returned", __func__, 1);

    daa->nalloc *= 2;
    return 0;
}


/*----------------------------------------------------------------------*
 *                           DNumaa accessors                           *
 *----------------------------------------------------------------------*/
/*!
 * \brief   l_dnaaGetCount()
 *
 * \param[in]    daa
 * \return  count   number of l_dna, or 0 if no l_dna or on error
 */
l_int32
l_dnaaGetCount(L_DNAA  *daa)
{
    if (!daa)
        return ERROR_INT("daa not defined", __func__, 0);
    return daa->n;
}


/*!
 * \brief   l_dnaaGetDnaCount()
 *
 * \param[in]    daa
 * \param[in]    index   of l_dna in daa
 * \return  count   of numbers in the referenced l_dna, or 0 on error.
 */
l_int32
l_dnaaGetDnaCount(L_DNAA   *daa,
                    l_int32  index)
{
    if (!daa)
        return ERROR_INT("daa not defined", __func__, 0);
    if (index < 0 || index >= daa->n)
        return ERROR_INT("invalid index into daa", __func__, 0);
    return l_dnaGetCount(daa->dna[index]);
}


/*!
 * \brief   l_dnaaGetNumberCount()
 *
 * \param[in]    daa
 * \return  count   total number of numbers in the l_dnaa,
 *                  or 0 if no numbers or on error
 */
l_int32
l_dnaaGetNumberCount(L_DNAA  *daa)
{
L_DNA   *da;
l_int32  n, sum, i;

    if (!daa)
        return ERROR_INT("daa not defined", __func__, 0);

    n = l_dnaaGetCount(daa);
    for (sum = 0, i = 0; i < n; i++) {
        da = l_dnaaGetDna(daa, i, L_CLONE);
        sum += l_dnaGetCount(da);
        l_dnaDestroy(&da);
    }

    return sum;
}


/*!
 * \brief   l_dnaaGetDna()
 *
 * \param[in]    daa
 * \param[in]    index        to the index-th l_dna
 * \param[in]    accessflag   L_COPY or L_CLONE
 * \return  l_dna, or NULL on error
 */
L_DNA *
l_dnaaGetDna(L_DNAA  *daa,
             l_int32  index,
             l_int32  accessflag)
{
    if (!daa)
        return (L_DNA *)ERROR_PTR("daa not defined", __func__, NULL);
    if (index < 0 || index >= daa->n)
        return (L_DNA *)ERROR_PTR("index not valid", __func__, NULL);

    if (accessflag == L_COPY)
        return l_dnaCopy(daa->dna[index]);
    else if (accessflag == L_CLONE)
        return l_dnaClone(daa->dna[index]);
    else
        return (L_DNA *)ERROR_PTR("invalid accessflag", __func__, NULL);
}


/*!
 * \brief   l_dnaaReplaceDna()
 *
 * \param[in]    daa
 * \param[in]    index   to the index-th l_dna
 * \param[in]    da      insert and replace any existing one
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Any existing l_dna is destroyed, and the input one
 *          is inserted in its place.
 *      (2) If %index is invalid, return 1 (error)
 * </pre>
 */
l_ok
l_dnaaReplaceDna(L_DNAA  *daa,
                 l_int32  index,
                 L_DNA   *da)
{
l_int32  n;

    if (!daa)
        return ERROR_INT("daa not defined", __func__, 1);
    if (!da)
        return ERROR_INT("da not defined", __func__, 1);
    n = l_dnaaGetCount(daa);
    if (index < 0 || index >= n)
        return ERROR_INT("index not valid", __func__, 1);

    l_dnaDestroy(&daa->dna[index]);
    daa->dna[index] = da;
    return 0;
}


/*!
 * \brief   l_dnaaGetValue()
 *
 * \param[in]    daa
 * \param[in]    i      index of l_dna within l_dnaa
 * \param[in]    j      index into l_dna
 * \param[out]   pval   double value
 * \return  0 if OK, 1 on error
 */
l_ok
l_dnaaGetValue(L_DNAA     *daa,
               l_int32     i,
               l_int32     j,
               l_float64  *pval)
{
l_int32  n;
L_DNA   *da;

    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0.0;
    if (!daa)
        return ERROR_INT("daa not defined", __func__, 1);
    n = l_dnaaGetCount(daa);
    if (i < 0 || i >= n)
        return ERROR_INT("invalid index into daa", __func__, 1);
    da = daa->dna[i];
    if (j < 0 || j >= da->n)
        return ERROR_INT("invalid index into da", __func__, 1);
    *pval = da->array[j];
    return 0;
}


/*!
 * \brief   l_dnaaAddNumber()
 *
 * \param[in]    daa
 * \param[in]    index    of l_dna within l_dnaa
 * \param[in]    val      number to be added; stored as a double
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Adds to an existing l_dna only.
 * </pre>
 */
l_ok
l_dnaaAddNumber(L_DNAA    *daa,
                l_int32    index,
                l_float64  val)
{
l_int32  n;
L_DNA   *da;

    if (!daa)
        return ERROR_INT("daa not defined", __func__, 1);
    n = l_dnaaGetCount(daa);
    if (index < 0 || index >= n)
        return ERROR_INT("invalid index in daa", __func__, 1);

    da = l_dnaaGetDna(daa, index, L_CLONE);
    l_dnaAddNumber(da, val);
    l_dnaDestroy(&da);
    return 0;
}


/*----------------------------------------------------------------------*
 *                        Serialize Dna for I/O                         *
 *----------------------------------------------------------------------*/
/*!
 * \brief   l_dnaaRead()
 *
 * \param[in]    filename
 * \return  daa, or NULL on error
 */
L_DNAA *
l_dnaaRead(const char  *filename)
{
FILE    *fp;
L_DNAA  *daa;

    if (!filename)
        return (L_DNAA *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (L_DNAA *)ERROR_PTR_1("stream not opened",
                                     filename, __func__, NULL);
    daa = l_dnaaReadStream(fp);
    fclose(fp);
    if (!daa)
        return (L_DNAA *)ERROR_PTR_1("daa not read",
                                     filename, __func__, NULL);
    return daa;
}


/*!
 * \brief   l_dnaaReadStream()
 *
 * \param[in]    fp   file stream
 * \return  daa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) It is OK for the dnaa to be empty.
 * </pre>
 */
L_DNAA *
l_dnaaReadStream(FILE  *fp)
{
l_int32    i, n, index, ret, version;
L_DNA     *da;
L_DNAA    *daa;

    if (!fp)
        return (L_DNAA *)ERROR_PTR("stream not defined", __func__, NULL);

    ret = fscanf(fp, "\nL_Dnaa Version %d\n", &version);
    if (ret != 1)
        return (L_DNAA *)ERROR_PTR("not a l_dna file", __func__, NULL);
    if (version != DNA_VERSION_NUMBER)
        return (L_DNAA *)ERROR_PTR("invalid l_dnaa version", __func__, NULL);
    if (fscanf(fp, "Number of L_Dna = %d\n\n", &n) != 1)
        return (L_DNAA *)ERROR_PTR("invalid number of l_dna", __func__, NULL);
    if (n < 0)
        return (L_DNAA *)ERROR_PTR("num l_dna <= 0", __func__, NULL);
    if (n > MaxPtrArraySize)
        return (L_DNAA *)ERROR_PTR("too many l_dna", __func__, NULL);
    if (n == 0) L_INFO("the dnaa is empty\n", __func__);

    if ((daa = l_dnaaCreate(n)) == NULL)
        return (L_DNAA *)ERROR_PTR("daa not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        if (fscanf(fp, "L_Dna[%d]:", &index) != 1) {
            l_dnaaDestroy(&daa);
            return (L_DNAA *)ERROR_PTR("invalid l_dna header", __func__, NULL);
        }
        if ((da = l_dnaReadStream(fp)) == NULL) {
            l_dnaaDestroy(&daa);
            return (L_DNAA *)ERROR_PTR("da not made", __func__, NULL);
        }
        l_dnaaAddDna(daa, da, L_INSERT);
    }

    return daa;
}


/*!
 * \brief   l_dnaaReadMem()
 *
 * \param[in]    data     dnaa serialization; in ascii
 * \param[in]    size     of data; can use strlen to get it
 * \return  daa, or NULL on error
 */
L_DNAA *
l_dnaaReadMem(const l_uint8  *data,
              size_t          size)
{
FILE    *fp;
L_DNAA  *daa;

    if (!data)
        return (L_DNAA *)ERROR_PTR("data not defined", __func__, NULL);
    if ((fp = fopenReadFromMemory(data, size)) == NULL)
        return (L_DNAA *)ERROR_PTR("stream not opened", __func__, NULL);

    daa = l_dnaaReadStream(fp);
    fclose(fp);
    if (!daa) L_ERROR("daa not read\n", __func__);
    return daa;
}


/*!
 * \brief   l_dnaaWrite()
 *
 * \param[in]    filename
 * \param[in]    daa
 * \return  0 if OK, 1 on error
 */
l_ok
l_dnaaWrite(const char  *filename,
            L_DNAA      *daa)
{
l_int32  ret;
FILE    *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!daa)
        return ERROR_INT("daa not defined", __func__, 1);

    if ((fp = fopenWriteStream(filename, "w")) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    ret = l_dnaaWriteStream(fp, daa);
    fclose(fp);
    if (ret)
        return ERROR_INT_1("daa not written to stream", filename, __func__, 1);
    return 0;
}


/*!
 * \brief   l_dnaaWriteStream()
 *
 * \param[in]    fp     file stream
 * \param[in]    daa
 * \return  0 if OK, 1 on error
 */
l_ok
l_dnaaWriteStream(FILE    *fp,
                  L_DNAA  *daa)
{
l_int32  i, n;
L_DNA   *da;

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!daa)
        return ERROR_INT("daa not defined", __func__, 1);

    n = l_dnaaGetCount(daa);
    fprintf(fp, "\nL_Dnaa Version %d\n", DNA_VERSION_NUMBER);
    fprintf(fp, "Number of L_Dna = %d\n\n", n);
    for (i = 0; i < n; i++) {
        if ((da = l_dnaaGetDna(daa, i, L_CLONE)) == NULL)
            return ERROR_INT("da not found", __func__, 1);
        fprintf(fp, "L_Dna[%d]:", i);
        l_dnaWriteStream(fp, da);
        l_dnaDestroy(&da);
    }

    return 0;
}


/*!
 * \brief   l_dnaaWriteMem()
 *
 * \param[out]   pdata    data of serialized dnaa; ascii
 * \param[out]   psize    size of returned data
 * \param[in]    daa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Serializes a dnaa in memory and puts the result in a buffer.
 * </pre>
 */
l_ok
l_dnaaWriteMem(l_uint8  **pdata,
               size_t    *psize,
               L_DNAA    *daa)
{
l_int32  ret;
FILE    *fp;

    if (pdata) *pdata = NULL;
    if (psize) *psize = 0;
    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1);
    if (!daa)
        return ERROR_INT("daa not defined", __func__, 1);

#if HAVE_FMEMOPEN
    if ((fp = open_memstream((char **)pdata, psize)) == NULL)
        return ERROR_INT("stream not opened", __func__, 1);
    ret = l_dnaaWriteStream(fp, daa);
    fputc('\0', fp);
    fclose(fp);
    if (*psize > 0) *psize = *psize - 1;
#else
    L_INFO("no fmemopen API --> work-around: write to temp file\n", __func__);
  #ifdef _WIN32
    if ((fp = fopenWriteWinTempfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #else
    if ((fp = tmpfile()) == NULL)
        return ERROR_INT("tmpfile stream not opened", __func__, 1);
  #endif  /* _WIN32 */
    ret = l_dnaaWriteStream(fp, daa);
    rewind(fp);
    *pdata = l_binaryReadStream(fp, psize);
    fclose(fp);
#endif  /* HAVE_FMEMOPEN */
    return ret;
}


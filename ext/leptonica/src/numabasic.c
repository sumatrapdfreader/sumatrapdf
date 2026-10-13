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
 * \file  numabasic.c
 * <pre>
 *
 *      Numa creation, destruction, copy, clone, etc.
 *          NUMA        *numaCreate()
 *          NUMA        *numaCreateFromIArray()
 *          NUMA        *numaCreateFromFArray()
 *          NUMA        *numaCreateFromString()
 *          void        *numaDestroy()
 *          NUMA        *numaCopy()
 *          NUMA        *numaClone()
 *          l_int32      numaEmpty()
 *
 *      Add/remove number (float or integer)
 *          l_int32      numaAddNumber()
 *          static l_int32  numaExtendArray()
 *          l_int32      numaInsertNumber()
 *          l_int32      numaRemoveNumber()
 *          l_int32      numaReplaceNumber()
 *
 *      Numa accessors
 *          l_int32      numaGetCount()
 *          l_int32      numaSetCount()
 *          l_int32      numaGetIValue()
 *          l_int32      numaGetFValue()
 *          l_int32      numaSetValue()
 *          l_int32      numaShiftValue()
 *          l_int32     *numaGetIArray()
 *          l_float32   *numaGetFArray()
 *          l_int32      numaGetParameters()
 *          l_int32      numaSetParameters()
 *          l_int32      numaCopyParameters()
 *
 *      Convert to string array
 *          SARRAY      *numaConvertToSarray()
 *
 *      Serialize numa for I/O
 *          NUMA        *numaRead()
 *          NUMA        *numaReadStream()
 *          NUMA        *numaReadMem()
 *          l_int32      numaWriteDebug()
 *          l_int32      numaWrite()
 *          l_int32      numaWriteStream()
 *          l_int32      numaWriteStderr()
 *          l_int32      numaWriteMem()
 *
 *      Numaa creation, destruction, truncation
 *          NUMAA       *numaaCreate()
 *          NUMAA       *numaaCreateFull()
 *          NUMAA       *numaaTruncate()
 *          void        *numaaDestroy()
 *
 *      Add Numa to Numaa
 *          l_int32      numaaAddNuma()
 *          static l_int32   numaaExtendArray()
 *
 *      Numaa accessors
 *          l_int32      numaaGetCount()
 *          l_int32      numaaGetNumaCount()
 *          l_int32      numaaGetNumberCount()
 *          NUMA       **numaaGetPtrArray()
 *          NUMA        *numaaGetNuma()
 *          NUMA        *numaaReplaceNuma()
 *          l_int32      numaaGetValue()
 *          l_int32      numaaAddNumber()
 *
 *      Serialize numaa for I/O
 *          NUMAA       *numaaRead()
 *          NUMAA       *numaaReadStream()
 *          NUMAA       *numaaReadMem()
 *          l_int32      numaaWrite()
 *          l_int32      numaaWriteStream()
 *          l_int32      numaaWriteMem()
 *
 *    (1) The Numa is a struct holding an array of floats.  It can also
 *        be used to store l_int32 values, with some loss of precision
 *        for floats larger than about 10 million.  Use the L_Dna instead
 *        if integers larger than a few million need to be stored.
 *
 *    (2) Always use the accessors in this file, never the fields directly.
 *
 *    (3) Storing and retrieving numbers:
 *
 *       * to append a new number to the array, use numaAddNumber().  If
 *         the number is an int, it will will automatically be converted
 *         to l_float32 and stored.
 *
 *       * to reset a value stored in the array, use numaSetValue().
 *
 *       * to increment or decrement a value stored in the array,
 *         use numaShiftValue().
 *
 *       * to obtain a value from the array, use either numaGetIValue()
 *         or numaGetFValue(), depending on whether you are retrieving
 *         an integer or a float.  This avoids doing an explicit cast,
 *         such as
 *           (a) return a l_float32 and cast it to an l_int32
 *           (b) cast the return directly to (l_float32 *) to
 *               satisfy the function prototype, as in
 *                 numaGetFValue(na, index, (l_float32 *)&ival);   [ugly!]
 *
 *    (4) int <--> float conversions:
 *
 *        Tradition dictates that type conversions go automatically from
 *        l_int32 --> l_float32, even though it is possible to lose
 *        precision for large integers, whereas you must cast (l_int32)
 *        to go from l_float32 --> l_int32 because you're truncating
 *        to the integer value.
 *
 *    (5) As with other arrays in leptonica, the numa has both an allocated
 *        size and a count of the stored numbers.  When you add a number, it
 *        goes on the end of the array, and causes a realloc if the array
 *        is already filled.  However, in situations where you want to
 *        add numbers randomly into an array, such as when you build a
 *        histogram, you must set the count of stored numbers in advance.
 *        This is done with numaSetCount().  If you set a count larger
 *        than the allocated array, it does a realloc to the size requested.
 *
 *    (6) In situations where the data in a numa correspond to a function
 *        y(x), the values can be either at equal spacings in x or at
 *        arbitrary spacings.  For the former, we can represent all x values
 *        by two parameters: startx (corresponding to y[0]) and delx
 *        for the change in x for adjacent values y[i] and y[i+1].
 *        startx and delx are initialized to 0.0 and 1.0, rsp.
 *        For arbitrary spacings, we use a second numa, and the two
 *        numas are typically denoted nay and nax.
 *
 *    (7) The numa is also the basic struct used for histograms.  Every numa
 *        has startx and delx fields, initialized to 0.0 and 1.0, that can
 *        be used to represent the "x" value for the location of the
 *        first bin and the bin width, respectively.  Accessors are the
 *        numa*Parameters() functions.  All functions that make numa
 *        histograms must set these fields properly, and many functions
 *        that use numa histograms rely on the correctness of these values.
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
static const l_uint32  MaxFloatArraySize = 100000000;  /* for numa */
static const l_uint32  MaxPtrArraySize = 1000000;  /* for numaa */
static const l_int32 InitialArraySize = 50;      /*!< n'importe quoi */

    /* Static functions */
static l_int32 numaExtendArray(NUMA  *na);
static l_int32 numaaExtendArray(NUMAA  *naa);

/*--------------------------------------------------------------------------*
 *               Numa creation, destruction, copy, clone, etc.              *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   numaCreate()
 *
 * \param[in]    n    size of number array to be alloc'd 0 for default
 * \return  na, or NULL on error
 */
NUMA *
numaCreate(l_int32  n)
{
NUMA  *na;

    if (n <= 0 || n > MaxFloatArraySize)
        n = InitialArraySize;

    na = (NUMA *)LEPT_CALLOC(1, sizeof(NUMA));
    if ((na->array = (l_float32 *)LEPT_CALLOC(n, sizeof(l_float32))) == NULL) {
        numaDestroy(&na);
        return (NUMA *)ERROR_PTR("number array not made", __func__, NULL);
    }

    na->nalloc = n;
    na->n = 0;
    na->refcount = 1;
    na->startx = 0.0;
    na->delx = 1.0;
    return na;
}


/*!
 * \brief   numaCreateFromIArray()
 *
 * \param[in]    iarray    integer array
 * \param[in]    size      of the array
 * \return  na, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) We can't insert this int array into the numa, because a numa
 *          takes a float array.  So this just copies the data from the
 *          input array into the numa.  The input array continues to be
 *          owned by the caller.
 * </pre>
 */
NUMA *
numaCreateFromIArray(l_int32  *iarray,
                     l_int32   size)
{
l_int32  i;
NUMA    *na;

    if (!iarray)
        return (NUMA *)ERROR_PTR("iarray not defined", __func__, NULL);
    if (size <= 0)
        return (NUMA *)ERROR_PTR("size must be > 0", __func__, NULL);

    na = numaCreate(size);
    for (i = 0; i < size; i++)
        numaAddNumber(na, iarray[i]);

    return na;
}


/*!
 * \brief   numaCreateFromFArray()
 *
 * \param[in]    farray     float array
 * \param[in]    size       of the array
 * \param[in]    copyflag   L_INSERT or L_COPY
 * \return  na, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) With L_INSERT, ownership of the input array is transferred
 *          to the returned numa, and all %size elements are considered
 *          to be valid.
 * </pre>
 */
NUMA *
numaCreateFromFArray(l_float32  *farray,
                     l_int32     size,
                     l_int32     copyflag)
{
l_int32  i;
NUMA    *na;

    if (!farray)
        return (NUMA *)ERROR_PTR("farray not defined", __func__, NULL);
    if (size <= 0)
        return (NUMA *)ERROR_PTR("size must be > 0", __func__, NULL);
    if (copyflag != L_INSERT && copyflag != L_COPY)
        return (NUMA *)ERROR_PTR("invalid copyflag", __func__, NULL);

    na = numaCreate(size);
    if (copyflag == L_INSERT) {
        if (na->array) LEPT_FREE(na->array);
        na->array = farray;
        na->n = size;
    } else {  /* just copy the contents */
        for (i = 0; i < size; i++)
            numaAddNumber(na, farray[i]);
    }

    return na;
}


/*!
 * \brief   numaCreateFromString()
 *
 * \param[in]    str    string of comma-separated numbers
 * \return  na, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The numbers can be ints or floats; they will be interpreted
 *          and stored as floats.  To use them as integers (e.g., for
 *          indexing into arrays), use numaGetIValue(...).
 * </pre>
 */
NUMA *
numaCreateFromString(const char  *str)
{
char      *substr;
l_int32    i, n, nerrors;
l_float32  val;
NUMA      *na;
SARRAY    *sa;

    if (!str || (strlen(str) == 0))
        return (NUMA *)ERROR_PTR("str not defined or empty", __func__, NULL);

    sa = sarrayCreate(0);
    sarraySplitString(sa, str, ",");
    n = sarrayGetCount(sa);
    na = numaCreate(n);
    nerrors = 0;
    for (i = 0; i < n; i++) {
        substr = sarrayGetString(sa, i, L_NOCOPY);
        if (sscanf(substr, "%f", &val) != 1) {
            L_ERROR("substr %d not float\n", __func__, i);
            nerrors++;
        } else {
            numaAddNumber(na, val);
        }
    }

    sarrayDestroy(&sa);
    if (nerrors > 0) {
        numaDestroy(&na);
        return (NUMA *)ERROR_PTR("non-floats in string", __func__, NULL);
    }

    return na;
}


/*!
 * \brief   numaDestroy()
 *
 * \param[in,out] pna   numa to be destroyed and nulled if it exists
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) Decrements the ref count and, if 0, destroys the numa.
 *      (2) Always nulls the input ptr.
 * </pre>
 */
void
numaDestroy(NUMA  **pna)
{
NUMA  *na;

    if (pna == NULL) {
        L_WARNING("ptr address is NULL\n", __func__);
        return;
    }

    if ((na = *pna) == NULL)
        return;

        /* Decrement the ref count.  If it is 0, destroy the numa. */
    if (--na->refcount == 0) {
        if (na->array)
            LEPT_FREE(na->array);
        LEPT_FREE(na);
    }

    *pna = NULL;
}


/*!
 * \brief   numaCopy()
 *
 * \param[in]    na
 * \return  copy of numa, or NULL on error
 */
NUMA *
numaCopy(NUMA  *na)
{
l_int32  i;
NUMA    *cna;

    if (!na)
        return (NUMA *)ERROR_PTR("na not defined", __func__, NULL);

    if ((cna = numaCreate(na->nalloc)) == NULL)
        return (NUMA *)ERROR_PTR("cna not made", __func__, NULL);
    cna->startx = na->startx;
    cna->delx = na->delx;

    for (i = 0; i < na->n; i++)
        numaAddNumber(cna, na->array[i]);

    return cna;
}


/*!
 * \brief   numaClone()
 *
 * \param[in]    na
 * \return  ptr to same numa, or NULL on error
 */
NUMA *
numaClone(NUMA  *na)
{
    if (!na)
        return (NUMA *)ERROR_PTR("na not defined", __func__, NULL);

    ++na->refcount;
    return na;
}


/*!
 * \brief   numaEmpty()
 *
 * \param[in]    na
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
numaEmpty(NUMA  *na)
{
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

    na->n = 0;
    return 0;
}



/*--------------------------------------------------------------------------*
 *                 Number array: add number and extend array                *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   numaAddNumber()
 *
 * \param[in]    na
 * \param[in]    val    float or int to be added; stored as a float
 * \return  0 if OK, 1 on error
 */
l_ok
numaAddNumber(NUMA      *na,
              l_float32  val)
{
l_int32  n;

    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

    n = numaGetCount(na);
    if (n >= na->nalloc) {
        if (numaExtendArray(na))
            return ERROR_INT("extension failed", __func__, 1);
    }
    na->array[n] = val;
    na->n++;
    return 0;
}


/*!
 * \brief   numaExtendArray()
 *
 * \param[in]    na
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The max number of floats is 100M.
 * </pre>
 */
static l_int32
numaExtendArray(NUMA  *na)
{
size_t  oldsize, newsize;

    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if (na->nalloc > MaxFloatArraySize)  /* belt & suspenders */
        return ERROR_INT("na has too many ptrs", __func__, 1);
    oldsize = na->nalloc * sizeof(l_float32);
    newsize = 2 * oldsize;
    if (newsize > 4 * MaxFloatArraySize)
        return ERROR_INT("newsize > 400 MB; too large", __func__, 1);

    if ((na->array = (l_float32 *)reallocNew((void **)&na->array,
                                             oldsize, newsize)) == NULL)
        return ERROR_INT("new ptr array not returned", __func__, 1);

    na->nalloc *= 2;
    return 0;
}


/*!
 * \brief   numaInsertNumber()
 *
 * \param[in]    na
 * \param[in]    index    location in na to insert new value
 * \param[in]    val      float32 or integer to be added
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This shifts na[i] --> na[i + 1] for all i >= index,
 *          and then inserts val as na[index].
 *      (2) It should not be used repeatedly on large arrays,
 *          because the function is O(n).
 *
 * </pre>
 */
l_ok
numaInsertNumber(NUMA      *na,
                 l_int32    index,
                 l_float32  val)
{
l_int32  i, n;

    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    n = numaGetCount(na);
    if (index < 0 || index > n) {
        L_ERROR("index %d not in [0,...,%d]\n", __func__, index, n);
        return 1;
    }

    if (n >= na->nalloc) {
        if (numaExtendArray(na))
            return ERROR_INT("extension failed", __func__, 1);
    }
    for (i = n; i > index; i--)
        na->array[i] = na->array[i - 1];
    na->array[index] = val;
    na->n++;
    return 0;
}


/*!
 * \brief   numaRemoveNumber()
 *
 * \param[in]    na
 * \param[in]    index    element to be removed
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This shifts na[i] --> na[i - 1] for all i > index.
 *      (2) It should not be used repeatedly on large arrays,
 *          because the function is O(n).
 * </pre>
 */
l_ok
numaRemoveNumber(NUMA    *na,
                 l_int32  index)
{
l_int32  i, n;

    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    n = numaGetCount(na);
    if (index < 0 || index >= n) {
        L_ERROR("index %d not in [0,...,%d]\n", __func__, index, n - 1);
        return 1;
    }

    for (i = index + 1; i < n; i++)
        na->array[i - 1] = na->array[i];
    na->n--;
    return 0;
}


/*!
 * \brief   numaReplaceNumber()
 *
 * \param[in]    na
 * \param[in]    index    element to be replaced
 * \param[in]    val      new value to replace old one
 * \return  0 if OK, 1 on error
 */
l_ok
numaReplaceNumber(NUMA      *na,
                  l_int32    index,
                  l_float32  val)
{
l_int32  n;

    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    n = numaGetCount(na);
    if (index < 0 || index >= n) {
        L_ERROR("index %d not in [0,...,%d]\n", __func__, index, n - 1);
        return 1;
    }

    na->array[index] = val;
    return 0;
}


/*----------------------------------------------------------------------*
 *                            Numa accessors                            *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaGetCount()
 *
 * \param[in]    na
 * \return  count, or 0 if no numbers or on error
 */
l_int32
numaGetCount(NUMA  *na)
{
    if (!na)
        return ERROR_INT("na not defined", __func__, 0);
    return na->n;
}


/*!
 * \brief   numaSetCount()
 *
 * \param[in]    na
 * \param[in]    newcount
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If newcount <= na->nalloc, this resets na->n.
 *          Using newcount = 0 is equivalent to numaEmpty().
 *      (2) If newcount > na->nalloc, this causes a realloc
 *          to a size na->nalloc = newcount.
 *      (3) All the previously unused values in na are set to 0.0.
 * </pre>
 */
l_ok
numaSetCount(NUMA    *na,
             l_int32  newcount)
{
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if (newcount > na->nalloc) {
        if ((na->array = (l_float32 *)reallocNew((void **)&na->array,
                         sizeof(l_float32) * na->nalloc,
                         sizeof(l_float32) * newcount)) == NULL)
            return ERROR_INT("new ptr array not returned", __func__, 1);
        na->nalloc = newcount;
    }
    na->n = newcount;
    return 0;
}


/*!
 * \brief   numaGetFValue()
 *
 * \param[in]    na
 * \param[in]    index    into numa
 * \param[out]   pval     float value; set to 0.0 on error
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Caller may need to check the function return value to
 *          decide if a 0.0 in the returned ival is valid.
 * </pre>
 */
l_ok
numaGetFValue(NUMA       *na,
              l_int32     index,
              l_float32  *pval)
{
    if (!pval)
        return ERROR_INT("&val not defined", __func__, 1);
    *pval = 0.0;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

    if (index < 0 || index >= na->n)
        return ERROR_INT("index not valid", __func__, 1);

    *pval = na->array[index];
    return 0;
}


/*!
 * \brief   numaGetIValue()
 *
 * \param[in]    na
 * \param[in]    index into numa
 * \param[out]   pival  integer value; set to 0 on error
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Caller may need to check the function return value to
 *          decide if a 0 in the returned ival is valid.
 * </pre>
 */
l_ok
numaGetIValue(NUMA     *na,
              l_int32   index,
              l_int32  *pival)
{
l_float32  val;

    if (!pival)
        return ERROR_INT("&ival not defined", __func__, 1);
    *pival = 0;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

    if (index < 0 || index >= na->n)
        return ERROR_INT("index not valid", __func__, 1);

    val = na->array[index];
    *pival = (l_int32)(val + L_SIGN(val) * 0.5);
    return 0;
}


/*!
 * \brief   numaSetValue()
 *
 * \param[in]    na
 * \param[in]    index   to element to be set
 * \param[in]    val     to set
 * \return  0 if OK; 1 on error
 */
l_ok
numaSetValue(NUMA      *na,
             l_int32    index,
             l_float32  val)
{
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if (index < 0 || index >= na->n)
        return ERROR_INT("index not valid", __func__, 1);

    na->array[index] = val;
    return 0;
}


/*!
 * \brief   numaShiftValue()
 *
 * \param[in]    na
 * \param[in]    index   to element to change relative to the current value
 * \param[in]    diff    increment if diff > 0 or decrement if diff < 0
 * \return  0 if OK; 1 on error
 */
l_ok
numaShiftValue(NUMA      *na,
               l_int32    index,
               l_float32  diff)
{
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if (index < 0 || index >= na->n)
        return ERROR_INT("index not valid", __func__, 1);

    na->array[index] += diff;
    return 0;
}


/*!
 * \brief   numaGetIArray()
 *
 * \param[in]    na
 * \return  a copy of the bare internal array, integerized
 *              by rounding, or NULL on error
 * <pre>
 * Notes:
 *      (1) A copy of the array is always made, because we need to
 *          generate an integer array from the bare float array.
 *          The caller is responsible for freeing the array.
 *      (2) The array size is determined by the number of stored numbers,
 *          not by the size of the allocated array in the Numa.
 *      (3) This function is provided to simplify calculations
 *          using the bare internal array, rather than continually
 *          calling accessors on the numa.  It is typically used
 *          on an array of size 256.
 * </pre>
 */
l_int32 *
numaGetIArray(NUMA  *na)
{
l_int32   i, n, ival;
l_int32  *array;

    if (!na)
        return (l_int32 *)ERROR_PTR("na not defined", __func__, NULL);

    if ((n = numaGetCount(na)) == 0)
        return (l_int32 *)ERROR_PTR("na is empty", __func__, NULL);
    if ((array = (l_int32 *)LEPT_CALLOC(n, sizeof(l_int32))) == NULL)
        return (l_int32 *)ERROR_PTR("array not made", __func__, NULL);
    for (i = 0; i < n; i++) {
        numaGetIValue(na, i, &ival);
        array[i] = ival;
    }

    return array;
}


/*!
 * \brief   numaGetFArray()
 *
 * \param[in]    na
 * \param[in]    copyflag    L_NOCOPY or L_COPY
 * \return  either the bare internal array or a copy of it,
 *              or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If copyflag == L_COPY, it makes a copy which the caller
 *          is responsible for freeing.  Otherwise, it operates
 *          directly on the bare array of the numa.
 *      (2) Very important: for L_NOCOPY, any writes to the array
 *          will be in the numa.  Do not write beyond the size of
 *          the count field, because it will not be accessible
 *          from the numa!  If necessary, be sure to set the count
 *          field to a larger number (such as the alloc size)
 *          BEFORE calling this function.  Creating with numaMakeConstant()
 *          is another way to insure full initialization.
 * </pre>
 */
l_float32 *
numaGetFArray(NUMA    *na,
              l_int32  copyflag)
{
l_int32     i, n;
l_float32  *array;

    if (!na)
        return (l_float32 *)ERROR_PTR("na not defined", __func__, NULL);

    if (copyflag == L_NOCOPY) {
        array = na->array;
    } else {  /* copyflag == L_COPY */
        if ((n = numaGetCount(na)) == 0)
            return (l_float32 *)ERROR_PTR("na is empty", __func__, NULL);
        if ((array = (l_float32 *)LEPT_CALLOC(n, sizeof(l_float32))) == NULL)
            return (l_float32 *)ERROR_PTR("array not made", __func__, NULL);
        for (i = 0; i < n; i++)
            array[i] = na->array[i];
    }

    return array;
}


/*!
 * \brief   numaGetParameters()
 *
 * \param[in]    na
 * \param[out]   pstartx    [optional] startx
 * \param[out]   pdelx      [optional] delx
 * \return  0 if OK, 1 on error
 */
l_ok
numaGetParameters(NUMA       *na,
                  l_float32  *pstartx,
                  l_float32  *pdelx)
{
    if (!pdelx && !pstartx)
        return ERROR_INT("no return val requested", __func__, 1);
    if (pstartx) *pstartx = 0.0;
    if (pdelx) *pdelx = 1.0;
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

    if (pstartx) *pstartx = na->startx;
    if (pdelx) *pdelx = na->delx;
    return 0;
}


/*!
 * \brief   numaSetParameters()
 *
 * \param[in]    na
 * \param[in]    startx  x value corresponding to na[0]
 * \param[in]    delx    difference in x values for the situation where the
 *                       elements of na correspond to the evaluation of a
 *                       function at equal intervals of size %delx
 * \return  0 if OK, 1 on error
 */
l_ok
numaSetParameters(NUMA      *na,
                  l_float32  startx,
                  l_float32  delx)
{
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

    na->startx = startx;
    na->delx = delx;
    return 0;
}


/*!
 * \brief   numaCopyParameters()
 *
 * \param[in]    nad    destination Numa
 * \param[in]    nas    source Numa
 * \return  0 if OK, 1 on error
 */
l_ok
numaCopyParameters(NUMA  *nad,
                   NUMA  *nas)
{
l_float32  start, binsize;

    if (!nas || !nad)
        return ERROR_INT("nas and nad not both defined", __func__, 1);

    numaGetParameters(nas, &start, &binsize);
    numaSetParameters(nad, start, binsize);
    return 0;
}


/*----------------------------------------------------------------------*
 *                      Convert to string array                         *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaConvertToSarray()
 *
 * \param[in]    na
 * \param[in]    size1      size of conversion field
 * \param[in]    size2      for float conversion: size of field to the right
 *                          of the decimal point
 * \param[in]    addzeros   for integer conversion: to add lead zeros
 * \param[in]    type       L_INTEGER_VALUE, L_FLOAT_VALUE
 * \return  a sarray of the float values converted to strings
 *              representing either integer or float values; or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) For integer conversion, size2 is ignored.
 *          For float conversion, addzeroes is ignored.
 * </pre>
 */
SARRAY *
numaConvertToSarray(NUMA    *na,
                    l_int32  size1,
                    l_int32  size2,
                    l_int32  addzeros,
                    l_int32  type)
{
char       fmt[32], strbuf[64];
l_int32    i, n, ival;
l_float32  fval;
SARRAY    *sa;

    if (!na)
        return (SARRAY *)ERROR_PTR("na not defined", __func__, NULL);
    if (type != L_INTEGER_VALUE && type != L_FLOAT_VALUE)
        return (SARRAY *)ERROR_PTR("invalid type", __func__, NULL);

    if (type == L_INTEGER_VALUE) {
        if (addzeros)
            snprintf(fmt, sizeof(fmt), "%%0%dd", size1);
        else
            snprintf(fmt, sizeof(fmt), "%%%dd", size1);
    } else {  /* L_FLOAT_VALUE */
        snprintf(fmt, sizeof(fmt), "%%%d.%df", size1, size2);
    }

    n = numaGetCount(na);
    if ((sa = sarrayCreate(n)) == NULL)
        return (SARRAY *)ERROR_PTR("sa not made", __func__, NULL);

    for (i = 0; i < n; i++) {
        if (type == L_INTEGER_VALUE) {
            numaGetIValue(na, i, &ival);
            snprintf(strbuf, sizeof(strbuf), fmt, ival);
        } else {  /* L_FLOAT_VALUE */
            numaGetFValue(na, i, &fval);
            snprintf(strbuf, sizeof(strbuf), fmt, fval);
        }
        sarrayAddString(sa, strbuf, L_COPY);
    }

    return sa;
}


/*----------------------------------------------------------------------*
 *                       Serialize numa for I/O                         *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaRead()
 *
 * \param[in]    filename
 * \return  na, or NULL on error
 */
NUMA *
numaRead(const char  *filename)
{
FILE  *fp;
NUMA  *na;

    if (!filename)
        return (NUMA *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (NUMA *)ERROR_PTR_1("stream not opened",
                                   filename, __func__, NULL);
    na = numaReadStream(fp);
    fclose(fp);
    if (!na)
        return (NUMA *)ERROR_PTR_1("na not read",
                                   filename, __func__, NULL);
    return na;
}


/*!
 * \brief   numaReadStream()
 *
 * \param[in]    fp    file stream
 * \return  numa, or NULL on error
 */
NUMA *
numaReadStream(FILE  *fp)
{
l_int32    i, n, index, ret, version;
l_float32  val, startx, delx;
NUMA      *na;

    if (!fp)
        return (NUMA *)ERROR_PTR("stream not defined", __func__, NULL);

    ret = fscanf(fp, "\nNuma Version %d\n", &version);
    if (ret != 1)
        return (NUMA *)ERROR_PTR("not a numa file", __func__, NULL);
    if (version != NUMA_VERSION_NUMBER)
        return (NUMA *)ERROR_PTR("invalid numa version", __func__, NULL);
    if (fscanf(fp, "Number of numbers = %d\n", &n) != 1)
        return (NUMA *)ERROR_PTR("invalid number of numbers", __func__, NULL);

    if (n > MaxFloatArraySize) {
        L_ERROR("n = %d > %d\n", __func__, n, MaxFloatArraySize);
        return NULL;
    }
    if ((na = numaCreate(n)) == NULL)
        return (NUMA *)ERROR_PTR("na not made", __func__, NULL);

    for (i = 0; i < n; i++) {
        if (fscanf(fp, "  [%d] = %f\n", &index, &val) != 2) {
            numaDestroy(&na);
            return (NUMA *)ERROR_PTR("bad input data", __func__, NULL);
        }
        numaAddNumber(na, val);
    }

        /* Optional data */
    if (fscanf(fp, "startx = %f, delx = %f\n", &startx, &delx) == 2)
        numaSetParameters(na, startx, delx);

    return na;
}


/*!
 * \brief   numaReadMem()
 *
 * \param[in]    data    numa serialization; in ascii
 * \param[in]    size    of data; can use strlen to get it
 * \return  na, or NULL on error
 */
NUMA *
numaReadMem(const l_uint8  *data,
            size_t          size)
{
FILE  *fp;
NUMA  *na;

    if (!data)
        return (NUMA *)ERROR_PTR("data not defined", __func__, NULL);
    if ((fp = fopenReadFromMemory(data, size)) == NULL)
        return (NUMA *)ERROR_PTR("stream not opened", __func__, NULL);

    na = numaReadStream(fp);
    fclose(fp);
    if (!na) L_ERROR("numa not read\n", __func__);
    return na;
}


/*!
 * \brief   numaWriteDebug()
 *
 * \param[in]    filename
 * \param[in]    na
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Debug version, intended for use in the library when writing
 *          to files in a temp directory with names that are compiled in.
 *          This is used instead of numaWrite() for all such library calls.
 *      (2) The global variable LeptDebugOK defaults to 0, and can be set
 *          or cleared by the function setLeptDebugOK().
 * </pre>
 */
l_ok
numaWriteDebug(const char  *filename,
               NUMA        *na)
{
    if (LeptDebugOK) {
        return numaWrite(filename, na);
    } else {
        L_INFO("write to named temp file %s is disabled\n", __func__, filename);
        return 0;
    }
}


/*!
 * \brief   numaWrite()
 *
 * \param[in]    filename
 * \param[in]    na
 * \return  0 if OK, 1 on error
 */
l_ok
numaWrite(const char  *filename,
          NUMA        *na)
{
l_int32  ret;
FILE    *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

    if ((fp = fopenWriteStream(filename, "w")) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    ret = numaWriteStream(fp, na);
    fclose(fp);
    if (ret)
        return ERROR_INT_1("na not written to stream", filename, __func__, 1);
    return 0;
}


/*!
 * \brief   numaWriteStream()
 *
 * \param[in]    fp    file stream; use NULL to write to stderr
 * \param[in]    na
 * \return  0 if OK, 1 on error
 */
l_ok
numaWriteStream(FILE  *fp,
                NUMA  *na)
{
l_int32    i, n;
l_float32  startx, delx;

    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    if (!fp)
        return numaWriteStderr(na);

    n = numaGetCount(na);
    fprintf(fp, "\nNuma Version %d\n", NUMA_VERSION_NUMBER);
    fprintf(fp, "Number of numbers = %d\n", n);
    for (i = 0; i < n; i++)
        fprintf(fp, "  [%d] = %f\n", i, na->array[i]);
    fprintf(fp, "\n");

        /* Optional data */
    numaGetParameters(na, &startx, &delx);
    if (startx != 0.0 || delx != 1.0)
        fprintf(fp, "startx = %f, delx = %f\n", startx, delx);

    return 0;
}


/*!
 * \brief   numaWriteStderr()
 *
 * \param[in]    na
 * \return  0 if OK, 1 on error
 */
l_ok
numaWriteStderr(NUMA  *na)
{
l_int32    i, n;
l_float32  startx, delx;

    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

    n = numaGetCount(na);
    lept_stderr("\nNuma Version %d\n", NUMA_VERSION_NUMBER);
    lept_stderr("Number of numbers = %d\n", n);
    for (i = 0; i < n; i++)
        lept_stderr("  [%d] = %f\n", i, na->array[i]);
    lept_stderr("\n");

        /* Optional data */
    numaGetParameters(na, &startx, &delx);
    if (startx != 0.0 || delx != 1.0)
        lept_stderr("startx = %f, delx = %f\n", startx, delx);

    return 0;
}


/*!
 * \brief   numaWriteMem()
 *
 * \param[out]   pdata    data of serialized numa; ascii
 * \param[out]   psize    size of returned data
 * \param[in]    na
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Serializes a numa in memory and puts the result in a buffer.
 * </pre>
 */
l_ok
numaWriteMem(l_uint8  **pdata,
             size_t    *psize,
             NUMA      *na)
{
l_int32  ret;
FILE    *fp;

    if (pdata) *pdata = NULL;
    if (psize) *psize = 0;
    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1);
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

#if HAVE_FMEMOPEN
    if ((fp = open_memstream((char **)pdata, psize)) == NULL)
        return ERROR_INT("stream not opened", __func__, 1);
    ret = numaWriteStream(fp, na);
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
    ret = numaWriteStream(fp, na);
    rewind(fp);
    *pdata = l_binaryReadStream(fp, psize);
    fclose(fp);
#endif  /* HAVE_FMEMOPEN */
    return ret;
}


/*--------------------------------------------------------------------------*
 *                     Numaa creation, destruction                          *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   numaaCreate()
 *
 * \param[in]    n     size of numa ptr array to be alloc'd 0 for default
 * \return  naa, or NULL on error
 *
 */
NUMAA *
numaaCreate(l_int32  n)
{
NUMAA  *naa;

    if (n <= 0 || n > MaxPtrArraySize)
        n = InitialArraySize;

    naa = (NUMAA *)LEPT_CALLOC(1, sizeof(NUMAA));
    if ((naa->numa = (NUMA **)LEPT_CALLOC(n, sizeof(NUMA *))) == NULL) {
        numaaDestroy(&naa);
        return (NUMAA *)ERROR_PTR("numa ptr array not made", __func__, NULL);
    }

    naa->nalloc = n;
    naa->n = 0;
    return naa;
}


/*!
 * \brief   numaaCreateFull()
 *
 * \param[in]    nptr   size of numa ptr array to be alloc'd
 * \param[in]    n      size of individual numa arrays to be allocated
 *                      to 0 for default
 * \return  naa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This allocates numaa and fills the array with allocated numas.
 *          In use, after calling this function, use
 *              numaaAddNumber(naa, index, val);
 *          to add val to the index-th numa in naa.
 * </pre>
 */
NUMAA *
numaaCreateFull(l_int32  nptr,
                l_int32  n)
{
l_int32  i;
NUMAA   *naa;
NUMA    *na;

    naa = numaaCreate(nptr);
    for (i = 0; i < nptr; i++) {
        na = numaCreate(n);
        numaaAddNuma(naa, na, L_INSERT);
    }

    return naa;
}


/*!
 * \brief   numaaTruncate()
 *
 * \param[in]    naa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This identifies the largest index containing a numa that
 *          has any numbers within it, destroys all numa beyond that
 *          index, and resets the count.
 * </pre>
 */
l_ok
numaaTruncate(NUMAA  *naa)
{
l_int32  i, n, nn;
NUMA    *na;

    if (!naa)
        return ERROR_INT("naa not defined", __func__, 1);

    n = numaaGetCount(naa);
    for (i = n - 1; i >= 0; i--) {
        na = numaaGetNuma(naa, i, L_CLONE);
        if (!na)
            continue;
        nn = numaGetCount(na);
        numaDestroy(&na);
        if (nn == 0)
            numaDestroy(&naa->numa[i]);
        else
            break;
    }
    naa->n = i + 1;
    return 0;
}


/*!
 * \brief   numaaDestroy()
 *
 * \param[in,out]  pnaa   to be destroyed and nulled, if it exists
 * \return  void
 */
void
numaaDestroy(NUMAA  **pnaa)
{
l_int32  i;
NUMAA   *naa;

    if (pnaa == NULL) {
        L_WARNING("ptr address is NULL!\n", __func__);
        return;
    }

    if ((naa = *pnaa) == NULL)
        return;

    for (i = 0; i < naa->n; i++)
        numaDestroy(&naa->numa[i]);
    LEPT_FREE(naa->numa);
    LEPT_FREE(naa);
    *pnaa = NULL;
}



/*--------------------------------------------------------------------------*
 *                              Add Numa to Numaa                           *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   numaaAddNuma()
 *
 * \param[in]    naa
 * \param[in]    na         to be added
 * \param[in]    copyflag   L_INSERT, L_COPY, L_CLONE
 * \return  0 if OK, 1 on error
 */
l_ok
numaaAddNuma(NUMAA   *naa,
             NUMA    *na,
             l_int32  copyflag)
{
l_int32  n;
NUMA    *nac;

    if (!naa)
        return ERROR_INT("naa not defined", __func__, 1);
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);

    if (copyflag == L_INSERT) {
        nac = na;
    } else if (copyflag == L_COPY) {
        if ((nac = numaCopy(na)) == NULL)
            return ERROR_INT("nac not made", __func__, 1);
    } else if (copyflag == L_CLONE) {
        nac = numaClone(na);
    } else {
        return ERROR_INT("invalid copyflag", __func__, 1);
    }

    n = numaaGetCount(naa);
    if (n >= naa->nalloc) {
        if (numaaExtendArray(naa)) {
            if (copyflag != L_INSERT)
                numaDestroy(&nac);
            return ERROR_INT("extension failed", __func__, 1);
        }
    }
    naa->numa[n] = nac;
    naa->n++;
    return 0;
}


/*!
 * \brief   numaaExtendArray()
 *
 * \param[in]    naa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The max number of numa ptrs is 1M.
 * </pre>
 */
static l_int32
numaaExtendArray(NUMAA  *naa)
{
size_t  oldsize, newsize;

    if (!naa)
        return ERROR_INT("naa not defined", __func__, 1);
    if (naa->nalloc > MaxPtrArraySize)  /* belt & suspenders */
        return ERROR_INT("naa has too many ptrs", __func__, 1);
    oldsize = naa->nalloc * sizeof(NUMA *);
    newsize = 2 * oldsize;
    if (newsize > 8 * MaxPtrArraySize)
        return ERROR_INT("newsize > 8 MB; too large", __func__, 1);

    if ((naa->numa = (NUMA **)reallocNew((void **)&naa->numa,
                                         oldsize, newsize)) == NULL)
        return ERROR_INT("new ptr array not returned", __func__, 1);

    naa->nalloc *= 2;
    return 0;
}


/*----------------------------------------------------------------------*
 *                           Numaa accessors                            *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaaGetCount()
 *
 * \param[in]    naa
 * \return  count number of numa, or 0 if no numa or on error
 */
l_int32
numaaGetCount(NUMAA  *naa)
{
    if (!naa)
        return ERROR_INT("naa not defined", __func__, 0);
    return naa->n;
}


/*!
 * \brief   numaaGetNumaCount()
 *
 * \param[in]    naa
 * \param[in]    index     of numa in naa
 * \return  count of numbers in the referenced numa, or 0 on error.
 */
l_int32
numaaGetNumaCount(NUMAA   *naa,
                  l_int32  index)
{
    if (!naa)
        return ERROR_INT("naa not defined", __func__, 0);
    if (index < 0 || index >= naa->n)
        return ERROR_INT("invalid index into naa", __func__, 0);
    return numaGetCount(naa->numa[index]);
}


/*!
 * \brief   numaaGetNumberCount()
 *
 * \param[in]    naa
 * \return  count total number of numbers in the numaa,
 *          or 0 if no numbers or on error
 */
l_int32
numaaGetNumberCount(NUMAA  *naa)
{
NUMA    *na;
l_int32  n, sum, i;

    if (!naa)
        return ERROR_INT("naa not defined", __func__, 0);

    n = numaaGetCount(naa);
    for (sum = 0, i = 0; i < n; i++) {
        na = numaaGetNuma(naa, i, L_CLONE);
        sum += numaGetCount(na);
        numaDestroy(&na);
    }

    return sum;
}


/*!
 * \brief   numaaGetPtrArray()
 *
 * \param[in]    naa
 * \return  the internal array of ptrs to Numa, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) This function is convenient for doing direct manipulation on
 *          a fixed size array of Numas.  To do this, it sets the count
 *          to the full size of the allocated array of Numa ptrs.
 *          The originating Numaa owns this array: DO NOT free it!
 *      (2) Intended usage:
 *            Numaa *naa = numaaCreate(n);
 *            Numa **array = numaaGetPtrArray(naa);
 *             ...  [manipulate Numas directly on the array]
 *            numaaDestroy(&naa);
 *      (3) Cautions:
 *           ~ Do not free this array; it is owned by tne Numaa.
 *           ~ Do not call any functions on the Numaa, other than
 *             numaaDestroy() when you're finished with the array.
 *             Adding a Numa will force a resize, destroying the ptr array.
 *           ~ Do not address the array outside its allocated size.
 *             With the bare array, there are no protections.  If the
 *             allocated size is n, array[n] is an error.
 * </pre>
 */
NUMA **
numaaGetPtrArray(NUMAA  *naa)
{
    if (!naa)
        return (NUMA **)ERROR_PTR("naa not defined", __func__, NULL);

    naa->n = naa->nalloc;
    return naa->numa;
}


/*!
 * \brief   numaaGetNuma()
 *
 * \param[in]    naa
 * \param[in]    index        to the index-th numa
 * \param[in]    accessflag   L_COPY or L_CLONE
 * \return  numa, or NULL on error
 */
NUMA *
numaaGetNuma(NUMAA   *naa,
             l_int32  index,
             l_int32  accessflag)
{
    if (!naa)
        return (NUMA *)ERROR_PTR("naa not defined", __func__, NULL);
    if (index < 0 || index >= naa->n)
        return (NUMA *)ERROR_PTR("index not valid", __func__, NULL);

    if (accessflag == L_COPY)
        return numaCopy(naa->numa[index]);
    else if (accessflag == L_CLONE)
        return numaClone(naa->numa[index]);
    else
        return (NUMA *)ERROR_PTR("invalid accessflag", __func__, NULL);
}


/*!
 * \brief   numaaReplaceNuma()
 *
 * \param[in]    naa
 * \param[in]    index    to the index-th numa
 * \param[in]    na       insert and replace any existing one
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Any existing numa is destroyed, and the input one
 *          is inserted in its place.
 *      (2) If the index is invalid, return 1 (error)
 * </pre>
 */
l_ok
numaaReplaceNuma(NUMAA   *naa,
                 l_int32  index,
                 NUMA    *na)
{
l_int32  n;

    if (!naa)
        return ERROR_INT("naa not defined", __func__, 1);
    if (!na)
        return ERROR_INT("na not defined", __func__, 1);
    n = numaaGetCount(naa);
    if (index < 0 || index >= n)
        return ERROR_INT("index not valid", __func__, 1);

    numaDestroy(&naa->numa[index]);
    naa->numa[index] = na;
    return 0;
}


/*!
 * \brief   numaaGetValue()
 *
 * \param[in]    naa
 * \param[in]    i       index of numa within numaa
 * \param[in]    j       index into numa
 * \param[out]   pfval   [optional] float value
 * \param[out]   pival   [optional] int value
 * \return  0 if OK, 1 on error
 */
l_ok
numaaGetValue(NUMAA      *naa,
              l_int32     i,
              l_int32     j,
              l_float32  *pfval,
              l_int32    *pival)
{
l_int32  n;
NUMA    *na;

    if (!pfval && !pival)
        return ERROR_INT("no return val requested", __func__, 1);
    if (pfval) *pfval = 0.0;
    if (pival) *pival = 0;
    if (!naa)
        return ERROR_INT("naa not defined", __func__, 1);
    n = numaaGetCount(naa);
    if (i < 0 || i >= n)
        return ERROR_INT("invalid index into naa", __func__, 1);
    na = naa->numa[i];
    if (j < 0 || j >= na->n)
        return ERROR_INT("invalid index into na", __func__, 1);
    if (pfval) *pfval = na->array[j];
    if (pival) *pival = (l_int32)(na->array[j]);
    return 0;
}


/*!
 * \brief   numaaAddNumber()
 *
 * \param[in]    naa
 * \param[in]    index    of numa within numaa
 * \param[in]    val      float or int to be added; stored as a float
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Adds to an existing numa only.
 * </pre>
 */
l_ok
numaaAddNumber(NUMAA     *naa,
               l_int32    index,
               l_float32  val)
{
l_int32  n;
NUMA    *na;

    if (!naa)
        return ERROR_INT("naa not defined", __func__, 1);
    n = numaaGetCount(naa);
    if (index < 0 || index >= n)
        return ERROR_INT("invalid index in naa", __func__, 1);

    na = numaaGetNuma(naa, index, L_CLONE);
    numaAddNumber(na, val);
    numaDestroy(&na);
    return 0;
}


/*----------------------------------------------------------------------*
 *                      Serialize numaa for I/O                         *
 *----------------------------------------------------------------------*/
/*!
 * \brief   numaaRead()
 *
 * \param[in]    filename
 * \return  naa, or NULL on error
 */
NUMAA *
numaaRead(const char  *filename)
{
FILE   *fp;
NUMAA  *naa;

    if (!filename)
        return (NUMAA *)ERROR_PTR("filename not defined", __func__, NULL);

    if ((fp = fopenReadStream(filename)) == NULL)
        return (NUMAA *)ERROR_PTR_1("stream not opened",
                                    filename, __func__, NULL);
    naa = numaaReadStream(fp);
    fclose(fp);
    if (!naa)
        return (NUMAA *)ERROR_PTR_1("naa not read",
                                    filename, __func__, NULL);
    return naa;
}


/*!
 * \brief   numaaReadStream()
 *
 * \param[in]    fp     file stream
 * \return  naa, or NULL on error
 */
NUMAA *
numaaReadStream(FILE  *fp)
{
l_int32    i, n, index, ret, version;
NUMA      *na;
NUMAA     *naa;

    if (!fp)
        return (NUMAA *)ERROR_PTR("stream not defined", __func__, NULL);

    ret = fscanf(fp, "\nNumaa Version %d\n", &version);
    if (ret != 1)
        return (NUMAA *)ERROR_PTR("not a numa file", __func__, NULL);
    if (version != NUMA_VERSION_NUMBER)
        return (NUMAA *)ERROR_PTR("invalid numaa version", __func__, NULL);
    if (fscanf(fp, "Number of numa = %d\n\n", &n) != 1)
        return (NUMAA *)ERROR_PTR("invalid number of numa", __func__, NULL);

    if (n > MaxPtrArraySize) {
        L_ERROR("n = %d > %d\n", __func__, n, MaxPtrArraySize);
        return NULL;
    }
    if ((naa = numaaCreate(n)) == NULL)
        return (NUMAA *)ERROR_PTR("naa not made", __func__, NULL);

    for (i = 0; i < n; i++) {
        if (fscanf(fp, "Numa[%d]:", &index) != 1) {
            numaaDestroy(&naa);
            return (NUMAA *)ERROR_PTR("invalid numa header", __func__, NULL);
        }
        if ((na = numaReadStream(fp)) == NULL) {
            numaaDestroy(&naa);
            return (NUMAA *)ERROR_PTR("na not made", __func__, NULL);
        }
        numaaAddNuma(naa, na, L_INSERT);
    }

    return naa;
}


/*!
 * \brief   numaaReadMem()
 *
 * \param[in]    data     numaa serialization; in ascii
 * \param[in]    size     of data; can use strlen to get it
 * \return  naa, or NULL on error
 */
NUMAA *
numaaReadMem(const l_uint8  *data,
             size_t          size)
{
FILE   *fp;
NUMAA  *naa;

    if (!data)
        return (NUMAA *)ERROR_PTR("data not defined", __func__, NULL);
    if ((fp = fopenReadFromMemory(data, size)) == NULL)
        return (NUMAA *)ERROR_PTR("stream not opened", __func__, NULL);

    naa = numaaReadStream(fp);
    fclose(fp);
    if (!naa) L_ERROR("naa not read\n", __func__);
    return naa;
}


/*!
 * \brief   numaaWrite()
 *
 * \param[in]    filename
 * \param[in]    naa
 * \return  0 if OK, 1 on error
 */
l_ok
numaaWrite(const char  *filename,
           NUMAA       *naa)
{
l_int32  ret;
FILE    *fp;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!naa)
        return ERROR_INT("naa not defined", __func__, 1);

    if ((fp = fopenWriteStream(filename, "w")) == NULL)
        return ERROR_INT_1("stream not opened", filename, __func__, 1);
    ret = numaaWriteStream(fp, naa);
    fclose(fp);
    if (ret)
        return ERROR_INT_1("naa not written to stream", filename, __func__, 1);
    return 0;
}


/*!
 * \brief   numaaWriteStream()
 *
 * \param[in]    fp     file stream
 * \param[in]    naa
 * \return  0 if OK, 1 on error
 */
l_ok
numaaWriteStream(FILE   *fp,
                 NUMAA  *naa)
{
l_int32  i, n;
NUMA    *na;

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!naa)
        return ERROR_INT("naa not defined", __func__, 1);

    n = numaaGetCount(naa);
    fprintf(fp, "\nNumaa Version %d\n", NUMA_VERSION_NUMBER);
    fprintf(fp, "Number of numa = %d\n\n", n);
    for (i = 0; i < n; i++) {
        if ((na = numaaGetNuma(naa, i, L_CLONE)) == NULL)
            return ERROR_INT("na not found", __func__, 1);
        fprintf(fp, "Numa[%d]:", i);
        numaWriteStream(fp, na);
        numaDestroy(&na);
    }

    return 0;
}


/*!
 * \brief   numaaWriteMem()
 *
 * \param[out]   pdata    data of serialized numaa; ascii
 * \param[out]   psize    size of returned data
 * \param[in]    naa
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Serializes a numaa in memory and puts the result in a buffer.
 * </pre>
 */
l_ok
numaaWriteMem(l_uint8  **pdata,
              size_t    *psize,
              NUMAA     *naa)
{
l_int32  ret;
FILE    *fp;

    if (pdata) *pdata = NULL;
    if (psize) *psize = 0;
    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1);
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1);
    if (!naa)
        return ERROR_INT("naa not defined", __func__, 1);

#if HAVE_FMEMOPEN
    if ((fp = open_memstream((char **)pdata, psize)) == NULL)
        return ERROR_INT("stream not opened", __func__, 1);
    ret = numaaWriteStream(fp, naa);
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
    ret = numaaWriteStream(fp, naa);
    rewind(fp);
    *pdata = l_binaryReadStream(fp, psize);
    fclose(fp);
#endif  /* HAVE_FMEMOPEN */
    return ret;
}


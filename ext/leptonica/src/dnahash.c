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
 * \file dnahash.c
 * <pre>
 *      Dnahash creation, destruction
 *          L_DNAHASH   *l_dnaHashCreate()
 *          void         l_dnaHashDestroy()
 *
 *      Dnahash accessor and modifier
 *          L_DNA       *l_dnaHashGetDna()
 *          l_int32      l_dnaHashAdd()
 *
 *    (1) The DnaHash is an array of Dna.  It is a simple method used for
 *        fast lookup of templates in the jbig2 classifier (jbclass.c).
 * </pre>
 */


#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"
#include "array_internal.h"

/*--------------------------------------------------------------------------*
 *                     Dnahash creation and destruction                   *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   l_dnaHashCreate()
 *
 * \param[in]   nbuckets   the number of buckets in the hash table,
 *                         which should be prime.
 * \param[in]   initsize   initial size of each allocated dna; 0 for default
 * \return  ptr to new dnahash, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If %nbuckets is not prime, use the next largest prime.
 *      (2) In use, stored dna are created by l_dnaHashAdd().
 * </pre>
 */
L_DNAHASH *
l_dnaHashCreate(l_int32  nbuckets,
                l_int32  initsize)
{
l_int32     is_prime;
l_uint32    newsize;
L_DNAHASH  *dahash;

    if (nbuckets <= 0)
        return (L_DNAHASH *)ERROR_PTR("negative hash size", __func__, NULL);
    lept_isPrime(nbuckets, &is_prime, NULL);
    if (!is_prime) {
        findNextLargerPrime(nbuckets, &newsize);
        nbuckets = newsize;
    }

    dahash = (L_DNAHASH *)LEPT_CALLOC(1, sizeof(L_DNAHASH));
    if ((dahash->dna = (L_DNA **)LEPT_CALLOC(nbuckets, sizeof(L_DNA *)))
        == NULL) {
        LEPT_FREE(dahash);
        return (L_DNAHASH *)ERROR_PTR("dna ptr array not made", __func__, NULL);
    }

    dahash->nbuckets = nbuckets;
    dahash->initsize = initsize;
    return dahash;
}


/*!
 * \brief   l_dnaHashDestroy()
 *
 * \param[in,out]   pdahash   will be set to null before returning
 * \return  void
 */
void
l_dnaHashDestroy(L_DNAHASH **pdahash)
{
L_DNAHASH  *dahash;
l_int32    i;

    if (pdahash == NULL) {
        L_WARNING("ptr address is NULL!\n", __func__);
        return;
    }

    if ((dahash = *pdahash) == NULL)
        return;

    for (i = 0; i < dahash->nbuckets; i++)
        l_dnaDestroy(&dahash->dna[i]);
    LEPT_FREE(dahash->dna);
    LEPT_FREE(dahash);
    *pdahash = NULL;
}


/*--------------------------------------------------------------------------*
 *                      Dnahash accessor and modifier                       *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   l_dnaHashGetDna()
 *
 * \param[in]    dahash
 * \param[in]    key        key to be hashed into a bucket number
 * \param[in]    copyflag   L_NOCOPY, L_COPY, L_CLONE
 * \return  ptr to dna
 */
L_DNA *
l_dnaHashGetDna(L_DNAHASH  *dahash,
                l_uint64    key,
                l_int32     copyflag)
{
l_int32  bucket;
L_DNA   *da;

    if (!dahash)
        return (L_DNA *)ERROR_PTR("dahash not defined", __func__, NULL);
    bucket = key % dahash->nbuckets;
    da = dahash->dna[bucket];
    if (da) {
        if (copyflag == L_NOCOPY)
            return da;
        else if (copyflag == L_COPY)
            return l_dnaCopy(da);
        else
            return l_dnaClone(da);
    }
    else
        return NULL;
}


/*!
 * \brief   l_dnaHashAdd()
 *
 * \param[in]    dahash
 * \param[in]    key      key to be hashed into a bucket number
 * \param[in]    value    float value to be appended to the specific dna
 * \return  0 if OK; 1 on error
 */
l_ok
l_dnaHashAdd(L_DNAHASH  *dahash,
             l_uint64    key,
             l_float64   value)
{
l_int32  bucket;
L_DNA   *da;

    if (!dahash)
        return ERROR_INT("dahash not defined", __func__, 1);
    bucket = key % dahash->nbuckets;
    da = dahash->dna[bucket];
    if (!da) {
        if ((da = l_dnaCreate(dahash->initsize)) == NULL)
            return ERROR_INT("da not made", __func__, 1);
        dahash->dna[bucket] = da;
    }
    l_dnaAddNumber(da, value);
    return 0;
}

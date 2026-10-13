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
 * \file  bytearray.c
 * <pre>
 *
 *   Functions for handling byte arrays, in analogy with C++ 'strings'
 *
 *      Creation, copy, clone, destruction
 *           L_BYTEA      *l_byteaCreate()
 *           L_BYTEA      *l_byteaInitFromMem()
 *           L_BYTEA      *l_byteaInitFromFile()
 *           L_BYTEA      *l_byteaInitFromStream()
 *           L_BYTEA      *l_byteaCopy()
 *           void          l_byteaDestroy()
 *
 *      Accessors
 *           size_t        l_byteaGetSize()
 *           l_uint8      *l_byteaGetData()
 *           l_uint8      *l_byteaCopyData()
 *
 *      Appending
 *           l_int32       l_byteaAppendData()
 *           l_int32       l_byteaAppendString()
 *           static l_int32  l_byteaExtendArrayToSize()
 *
 *      Join/Split
 *           l_int32       l_byteaJoin()
 *           l_int32       l_byteaSplit()
 *
 *      Search
 *           l_int32       l_byteaFindEachSequence()
 *
 *      Output to file
 *           l_int32       l_byteaWrite()
 *           l_int32       l_byteaWriteStream()
 *
 *   The internal data array is always null-terminated, for ease of use
 *   in the event that it is an ascii string without null bytes.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"
#include "array_internal.h"

    /* Bounds on array size */
static const l_uint32  MaxArraySize = 1000000000;   /* 10^9 bytes */
static const l_int32   InitialArraySize = 200;      /*!< n'importe quoi */

    /* Static function */
static l_int32 l_byteaExtendArrayToSize(L_BYTEA *ba, size_t size);

/*---------------------------------------------------------------------*
 *                  Creation, copy, clone, destruction                 *
 *---------------------------------------------------------------------*/
/*!
 * \brief   l_byteaCreate()
 *
 * \param[in]    nbytes    determines initial size of data array
 * \return  l_bytea, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The allocated array is n + 1 bytes.  This allows room
 *          for null termination.
 * </pre>
 */
L_BYTEA *
l_byteaCreate(size_t  nbytes)
{
L_BYTEA  *ba;

    if (nbytes <= 0 || nbytes > MaxArraySize)
        nbytes = InitialArraySize;
    ba = (L_BYTEA *)LEPT_CALLOC(1, sizeof(L_BYTEA));
    ba->data = (l_uint8 *)LEPT_CALLOC(nbytes + 1, sizeof(l_uint8));
    if (!ba->data) {
        l_byteaDestroy(&ba);
        return (L_BYTEA *)ERROR_PTR("ba array not made", __func__, NULL);
    }
    ba->nalloc = nbytes + 1;
    ba->refcount = 1;
    return ba;
}


/*!
 * \brief   l_byteaInitFromMem()
 *
 * \param[in]    data    to be copied to the array
 * \param[in]    size    amount of data
 * \return  l_bytea, or NULL on error
 */
L_BYTEA *
l_byteaInitFromMem(const l_uint8  *data,
                   size_t          size)
{
L_BYTEA  *ba;

    if (!data)
        return (L_BYTEA *)ERROR_PTR("data not defined", __func__, NULL);
    if (size <= 0)
        return (L_BYTEA *)ERROR_PTR("no bytes to initialize", __func__, NULL);
    if (size > MaxArraySize)
        return (L_BYTEA *)ERROR_PTR("size is too big", __func__, NULL);

    if ((ba = l_byteaCreate(size)) == NULL)
        return (L_BYTEA *)ERROR_PTR("ba not made", __func__, NULL);
    memcpy(ba->data, data, size);
    ba->size = size;
    return ba;
}


/*!
 * \brief   l_byteaInitFromFile()
 *
 * \param[in]    fname
 * \return  l_bytea, or NULL on error
 */
L_BYTEA *
l_byteaInitFromFile(const char  *fname)
{
FILE     *fp;
L_BYTEA  *ba;

    if (!fname)
        return (L_BYTEA *)ERROR_PTR("fname not defined", __func__, NULL);

    if ((fp = fopenReadStream(fname)) == NULL)
        return (L_BYTEA *)ERROR_PTR_1("file stream not opened",
                                      fname, __func__, NULL);
    ba = l_byteaInitFromStream(fp);
    fclose(fp);
    if (!ba)
        return (L_BYTEA *)ERROR_PTR_1("ba not made", fname, __func__, NULL);
    return ba;
}


/*!
 * \brief   l_byteaInitFromStream()
 *
 * \param[in]    fp    file stream
 * \return  l_bytea, or NULL on error
 */
L_BYTEA *
l_byteaInitFromStream(FILE  *fp)
{
l_uint8  *data;
size_t    nbytes;
L_BYTEA  *ba;

    if (!fp)
        return (L_BYTEA *)ERROR_PTR("stream not defined", __func__, NULL);

    if ((data = l_binaryReadStream(fp, &nbytes)) == NULL)
        return (L_BYTEA *)ERROR_PTR("data not read", __func__, NULL);
    if ((ba = l_byteaCreate(nbytes)) == NULL) {
        LEPT_FREE(data);
        return (L_BYTEA *)ERROR_PTR("ba not made", __func__, NULL);
    }
    memcpy(ba->data, data, nbytes);
    ba->size = nbytes;
    LEPT_FREE(data);
    return ba;
}


/*!
 * \brief   l_byteaCopy()
 *
 * \param[in]    bas        source lba
 * \param[in]    copyflag   L_COPY, L_CLONE
 * \return  clone or copy of bas, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If cloning, up the refcount and return a ptr to %bas.
 * </pre>
 */
L_BYTEA *
l_byteaCopy(L_BYTEA  *bas,
            l_int32   copyflag)
{
    if (!bas)
        return (L_BYTEA *)ERROR_PTR("bas not defined", __func__, NULL);

    if (copyflag == L_CLONE) {
        bas->refcount++;
        return bas;
    }

    return l_byteaInitFromMem(bas->data, bas->size);
}


/*!
 * \brief   l_byteaDestroy()
 *
 * \param[in,out]   pba    will be set to null before returning
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) Decrements the ref count and, if 0, destroys the lba.
 *      (2) Always nulls the input ptr.
 *      (3) If the data has been previously removed, the lba will
 *          have been nulled, so this will do nothing.
 * </pre>
 */
void
l_byteaDestroy(L_BYTEA  **pba)
{
L_BYTEA  *ba;

    if (pba == NULL) {
        L_WARNING("ptr address is null!\n", __func__);
        return;
    }

    if ((ba = *pba) == NULL)
        return;

        /* Decrement the ref count.  If it is 0, destroy the lba. */
    if (--ba->refcount == 0) {
        if (ba->data) LEPT_FREE(ba->data);
        LEPT_FREE(ba);
    }
    *pba = NULL;
}


/*---------------------------------------------------------------------*
 *                               Accessors                             *
 *---------------------------------------------------------------------*/
/*!
 * \brief   l_byteaGetSize()
 *
 * \param[in]    ba
 * \return  size of stored byte array, or 0 on error
 */
size_t
l_byteaGetSize(L_BYTEA  *ba)
{
    if (!ba)
        return ERROR_INT("ba not defined", __func__, 0);
    return ba->size;
}


/*!
 * \brief   l_byteaGetData()
 *
 * \param[in]    ba
 * \param[out]   psize     size of data in lba
 * \return  ptr to existing data array, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The returned ptr is owned by %ba.  Do not free it!
 * </pre>
 */
l_uint8 *
l_byteaGetData(L_BYTEA  *ba,
               size_t   *psize)
{
    if (!ba)
        return (l_uint8 *)ERROR_PTR("ba not defined", __func__, NULL);
    if (!psize)
        return (l_uint8 *)ERROR_PTR("&size not defined", __func__, NULL);

    *psize = ba->size;
    return ba->data;
}


/*!
 * \brief   l_byteaCopyData()
 *
 * \param[in]    ba
 * \param[out]   psize     size of data in lba
 * \return  copy of data in use in the data array, or NULL on error.
 *
 * <pre>
 * Notes:
 *      (1) The returned data is owned by the caller.  The input %ba
 *          still owns the original data array.
 * </pre>
 */
l_uint8 *
l_byteaCopyData(L_BYTEA  *ba,
                size_t   *psize)
{
l_uint8  *data;

    if (!psize)
        return (l_uint8 *)ERROR_PTR("&size not defined", __func__, NULL);
    *psize = 0;
    if (!ba)
        return (l_uint8 *)ERROR_PTR("ba not defined", __func__, NULL);

    data = l_byteaGetData(ba, psize);
    return l_binaryCopy(data, *psize);
}


/*---------------------------------------------------------------------*
 *                               Appending                             *
 *---------------------------------------------------------------------*/
/*!
 * \brief   l_byteaAppendData()
 *
 * \param[in]    ba
 * \param[in]    newdata    byte array to be appended
 * \param[in]    newbytes   size of data array
 * \return  0 if OK, 1 on error
 */
l_ok
l_byteaAppendData(L_BYTEA        *ba,
                  const l_uint8  *newdata,
                  size_t          newbytes)
{
size_t  size, nalloc, reqsize;

    if (!ba)
        return ERROR_INT("ba not defined", __func__, 1);
    if (!newdata)
        return ERROR_INT("newdata not defined", __func__, 1);

    size = l_byteaGetSize(ba);
    reqsize = size + newbytes + 1;
    nalloc = ba->nalloc;
    if (nalloc < reqsize) {
        if (l_byteaExtendArrayToSize(ba, 2 * reqsize))
            return ERROR_INT("extension failed", __func__, 1);
    }

    memcpy(ba->data + size, newdata, newbytes);
    ba->size += newbytes;
    return 0;
}


/*!
 * \brief   l_byteaAppendString()
 *
 * \param[in]    ba
 * \param[in]    str    null-terminated string to be appended
 * \return  0 if OK, 1 on error
 */
l_ok
l_byteaAppendString(L_BYTEA     *ba,
                    const char  *str)
{
size_t  size, len, nalloc, reqsize;

    if (!ba)
        return ERROR_INT("ba not defined", __func__, 1);
    if (!str)
        return ERROR_INT("str not defined", __func__, 1);

    size = l_byteaGetSize(ba);
    len = strlen(str);
    reqsize = size + len + 1;
    nalloc = ba->nalloc;
    if (nalloc < reqsize) {
        if (l_byteaExtendArrayToSize(ba, 2 * reqsize))
            return ERROR_INT("extension failed", __func__, 1);
    }

    memcpy(ba->data + size, str, len);
    ba->size += len;
    return 0;
}


/*!
 * \brief   l_byteaExtendArrayToSize()
 *
 * \param[in]    ba
 * \param[in]    size    new size of lba data array
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) If necessary, reallocs the byte array to %size.
 *      (2) The max buffer size is 1 GB.
 * </pre>
 */
static l_int32
l_byteaExtendArrayToSize(L_BYTEA  *ba,
                         size_t    size)
{
    if (!ba)
        return ERROR_INT("ba not defined", __func__, 1);
    if (ba->nalloc > MaxArraySize)  /* belt & suspenders */
        return ERROR_INT("ba has too many ptrs", __func__, 1);
    if (size > MaxArraySize)
        return ERROR_INT("size > 1 GB; too large", __func__, 1);
    if (size <= ba->nalloc) {
        L_INFO("size too small; no extension\n", __func__);
        return 0;
    }

    if ((ba->data =
        (l_uint8 *)reallocNew((void **)&ba->data, ba->nalloc, size)) == NULL)
        return ERROR_INT("new array not returned", __func__, 1);
    ba->nalloc = size;
    return 0;
}


/*---------------------------------------------------------------------*
 *                        String join/split                            *
 *---------------------------------------------------------------------*/
/*!
 * \brief   l_byteaJoin()
 *
 * \param[in]       ba1
 * \param[in,out]   pba2    data array is added to the one in ba1;
 *                          then ba2 is destroyed and its pointer is nulled.
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) It is a no-op, not an error, for %ba2 to be null.
 * </pre>
 */
l_ok
l_byteaJoin(L_BYTEA   *ba1,
            L_BYTEA  **pba2)
{
l_uint8  *data2;
size_t    nbytes2;
L_BYTEA  *ba2;

    if (!ba1)
        return ERROR_INT("ba1 not defined", __func__, 1);
    if (!pba2)
        return ERROR_INT("&ba2 not defined", __func__, 1);
    if ((ba2 = *pba2) == NULL) return 0;

    data2 = l_byteaGetData(ba2, &nbytes2);
    l_byteaAppendData(ba1, data2, nbytes2);

    l_byteaDestroy(pba2);
    return 0;
}


/*!
 * \brief   l_byteaSplit()
 *
 * \param[in]    ba1       lba to split; array bytes nulled beyond the split loc
 * \param[in]    splitloc  location in ba1 to split; ba2 begins there
 * \param[out]   pba2      with data starting at splitloc
 * \return  0 if OK, 1 on error
 */
l_ok
l_byteaSplit(L_BYTEA   *ba1,
             size_t     splitloc,
             L_BYTEA  **pba2)
{
l_uint8  *data1;
size_t    nbytes1, nbytes2;

    if (!pba2)
        return ERROR_INT("&ba2 not defined", __func__, 1);
    *pba2 = NULL;
    if (!ba1)
        return ERROR_INT("ba1 not defined", __func__, 1);

    data1 = l_byteaGetData(ba1, &nbytes1);
    if (splitloc >= nbytes1)
        return ERROR_INT("splitloc invalid", __func__, 1);
    nbytes2 = nbytes1 - splitloc;

        /* Make the new lba */
    *pba2 = l_byteaInitFromMem(data1 + splitloc, nbytes2);

        /* Null the removed bytes in the input lba */
    memset(data1 + splitloc, 0, nbytes2);
    ba1->size = splitloc;
    return 0;
}


/*---------------------------------------------------------------------*
 *                                Search                               *
 *---------------------------------------------------------------------*/
/*!
 * \brief   l_byteaFindEachSequence()
 *
 * \param[in]    ba
 * \param[in]    sequence   subarray of bytes to find in data
 * \param[in]    seqlen     length of sequence, in bytes
 * \param[out]   pda        byte positions of each occurrence of %sequence
 * \return  0 if OK, 1 on error
 */
l_ok
l_byteaFindEachSequence(L_BYTEA        *ba,
                        const l_uint8  *sequence,
                        size_t          seqlen,
                        L_DNA         **pda)
{
l_uint8  *data;
size_t    size;

    if (!pda)
        return ERROR_INT("&da not defined", __func__, 1);
    *pda = NULL;
    if (!ba)
        return ERROR_INT("ba not defined", __func__, 1);
    if (!sequence)
        return ERROR_INT("sequence not defined", __func__, 1);

    data = l_byteaGetData(ba, &size);
    *pda = arrayFindEachSequence(data, size, sequence, seqlen);
    return 0;
}


/*---------------------------------------------------------------------*
 *                              Output to file                         *
 *---------------------------------------------------------------------*/
/*!
 * \brief   l_byteaWrite()
 *
 * \param[in]    fname      output file
 * \param[in]    ba
 * \param[in]    startloc   first byte to output
 * \param[in]    nbytes     number of bytes to write; use 0 to write to
 *                          the end of the data array
 * \return  0 if OK, 1 on error
 */
l_ok
l_byteaWrite(const char  *fname,
             L_BYTEA     *ba,
             size_t       startloc,
             size_t       nbytes)
{
l_int32  ret;
FILE    *fp;

    if (!fname)
        return ERROR_INT("fname not defined", __func__, 1);
    if (!ba)
        return ERROR_INT("ba not defined", __func__, 1);

    if ((fp = fopenWriteStream(fname, "wb")) == NULL)
        return ERROR_INT_1("stream not opened", fname, __func__, 1);
    ret = l_byteaWriteStream(fp, ba, startloc, nbytes);
    fclose(fp);
    return ret;
}


/*!
 * \brief   l_byteaWriteStream()
 *
 * \param[in]    fp         file stream opened for binary write
 * \param[in]    ba
 * \param[in]    startloc   first byte to output
 * \param[in]    nbytes     number of bytes to write; use 0 to write to
 *                          the end of the data array
 * \return  0 if OK, 1 on error
 */
l_ok
l_byteaWriteStream(FILE     *fp,
                   L_BYTEA  *ba,
                   size_t    startloc,
                   size_t    nbytes)
{
l_uint8  *data;
size_t    size, maxbytes;

    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!ba)
        return ERROR_INT("ba not defined", __func__, 1);

    data = l_byteaGetData(ba, &size);
    if (startloc >= size)
        return ERROR_INT("invalid startloc", __func__, 1);
    maxbytes = size - startloc;
    nbytes = (nbytes == 0) ? maxbytes : L_MIN(nbytes, maxbytes);

    fwrite(data + startloc, 1, nbytes, fp);
    return 0;
}

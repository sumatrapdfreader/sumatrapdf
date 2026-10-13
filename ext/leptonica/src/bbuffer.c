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
 * \file  bbuffer.c
 * <pre>
 *
 *      Create/Destroy BBuffer
 *          L_BBUFFER      *bbufferCreate()
 *          void           *bbufferDestroy()
 *          l_uint8        *bbufferDestroyAndSaveData()
 *
 *      Operations to read data TO a BBuffer
 *          l_int32         bbufferRead()
 *          l_int32         bbufferReadStream()
 *          l_int32         bbufferExtendArray()
 *
 *      Operations to write data FROM a BBuffer
 *          l_int32         bbufferWrite()
 *          l_int32         bbufferWriteStream()
 *
 *    The bbuffer is an implementation of a byte queue.
 *    The bbuffer holds a byte array from which bytes are
 *    processed in a first-in/first-out fashion.  As with
 *    any queue, bbuffer maintains two "pointers," one to the
 *    tail of the queue (where you read new bytes onto it)
 *    and one to the head of the queue (where you start from
 *    when writing bytes out of it.
 *
 *    The queue can be visualized:
 *
 * \code
 *  byte 0                                           byte (nalloc - 1)
 *       |                                                |
 *       --------------------------------------------------
 *                 H                             T
 *       [   aw   ][  bytes currently on queue  ][  anr   ]
 *
 *       ---:  all allocated data in bbuffer
 *       H:    queue head (ptr to next byte to be written out)
 *       T:    queue tail (ptr to first byte to be written to)
 *       aw:   already written from queue
 *       anr:  allocated but not yet read to
 * \endcode
 *    The purpose of bbuffer is to allow you to safely read
 *    bytes in, and to sequentially write them out as well.
 *    In the process of writing bytes out, you don't actually
 *    remove the bytes in the array; you just move the pointer
 *    (nwritten) which points to the head of the queue.  In
 *    the process of reading bytes in, you sometimes need to
 *    expand the array size.  If a read is performed after a
 *    write, so that the head of the queue is not at the
 *    beginning of the array, the bytes already written are
 *    first removed by copying the others over them; then the
 *    new bytes are read onto the tail of the queue.
 *
 *    Note that the meaning of "read into" and "write from"
 *    the bbuffer is OPPOSITE to that for a stream, where
 *    you read "from" a stream and write "into" a stream.
 *    As a mnemonic for remembering the direction:
 *        ~ to read bytes from a stream into the bbuffer,
 *          you call fread on the stream
 *        ~ to write bytes from the bbuffer into a stream,
 *          you call fwrite on the stream
 *
 *    See zlibmem.c for an example use of bbuffer, where we
 *    compress and decompress an array of bytes in memory.
 *
 *    We can also use the bbuffer trivially to read from stdin
 *    into memory; e.g., to capture bytes piped from the stdout
 *    of another program.  This is equivalent to repeatedly
 *    calling bbufferReadStream() until the input queue is empty.
 *    This is implemented in l_binaryReadStream().
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

   /* Bounds on array size */
static const l_uint32  MaxArraySize = 1000000000;   /* 10^9 bytes */
static const l_int32   InitialArraySize = 1024;     /*!< n'importe quoi */

/*--------------------------------------------------------------------------*
 *                         BBuffer create/destroy                           *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   bbufferCreate()
 *
 * \param[in]    indata   address in memory [optional]
 * \param[in]    nalloc   size of byte array to be alloc'd 0 for default
 * \return  bbuffer, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If a buffer address is given, you should read all the data in.
 *      (2) Allocates a bbuffer with associated byte array of
 *          the given size.  If a buffer address is given,
 *          it then reads the number of bytes into the byte array.
 * </pre>
 */
L_BBUFFER *
bbufferCreate(const l_uint8  *indata,
              l_int32         nalloc)
{
L_BBUFFER  *bb;

    if (nalloc <= 0 || nalloc > MaxArraySize)
        nalloc = InitialArraySize;

    bb = (L_BBUFFER *)LEPT_CALLOC(1, sizeof(L_BBUFFER));
    if ((bb->array = (l_uint8 *)LEPT_CALLOC(nalloc, sizeof(l_uint8))) == NULL) {
        LEPT_FREE(bb);
        return (L_BBUFFER *)ERROR_PTR("byte array not made", __func__, NULL);
    }
    bb->nalloc = nalloc;
    bb->nwritten = 0;

    if (indata) {
        memcpy(bb->array, indata, nalloc);
        bb->n = nalloc;
    } else {
        bb->n = 0;
    }

    return bb;
}


/*!
 * \brief   bbufferDestroy()
 *
 * \param[in,out]   pbb   will be set to null before returning
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) Destroys the byte array in the bbuffer and then the bbuffer;
 *          then nulls the contents of the input ptr.
 * </pre>
 */
void
bbufferDestroy(L_BBUFFER  **pbb)
{
L_BBUFFER  *bb;

    if (pbb == NULL) {
        L_WARNING("ptr address is NULL\n", __func__);
        return;
    }

    if ((bb = *pbb) == NULL)
        return;

    if (bb->array)
        LEPT_FREE(bb->array);
    LEPT_FREE(bb);
    *pbb = NULL;
}


/*!
 * \brief   bbufferDestroyAndSaveData()
 *
 * \param[in,out]   pbb       input data buffer; will be nulled
 * \param[out]      pnbytes   number of bytes saved in array
 * \return  barray newly allocated array of data
 *
 * <pre>
 * Notes:
 *      (1) Copies data to newly allocated array; then destroys the bbuffer.
 * </pre>
 */
l_uint8 *
bbufferDestroyAndSaveData(L_BBUFFER  **pbb,
                          size_t      *pnbytes)
{
l_uint8    *array;
size_t      nbytes;
L_BBUFFER  *bb;

    if (pbb == NULL) {
        L_WARNING("ptr address is NULL\n", __func__);
        return NULL;
    }
    if (pnbytes == NULL) {
        L_WARNING("&nbytes is NULL\n", __func__);
        bbufferDestroy(pbb);
        return NULL;
    }

    if ((bb = *pbb) == NULL)
        return NULL;

        /* write all unwritten bytes out to a new array */
    nbytes = bb->n - bb->nwritten;
    *pnbytes = nbytes;
    if ((array = (l_uint8 *)LEPT_CALLOC(nbytes, sizeof(l_uint8))) == NULL) {
        L_WARNING("calloc failure for array\n", __func__);
        return NULL;
    }
    memcpy(array, bb->array + bb->nwritten, nbytes);

    bbufferDestroy(pbb);
    return array;
}


/*--------------------------------------------------------------------------*
 *                   Operations to read data INTO a BBuffer                 *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   bbufferRead()
 *
 * \param[in]    bb       bbuffer
 * \param[in]    src      source memory buffer from which bytes are read
 * \param[in]    nbytes   bytes to be read
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) For a read after write, first remove the written
 *          bytes by shifting the unwritten bytes in the array,
 *          then check if there is enough room to add the new bytes.
 *          If not, realloc with bbufferExpandArray(), resulting
 *          in a second writing of the unwritten bytes.  While less
 *          efficient, this is simpler than making a special case
 *          of reallocNew().
 * </pre>
 */
l_ok
bbufferRead(L_BBUFFER  *bb,
            l_uint8    *src,
            l_int32     nbytes)
{
l_int32  navail, nadd, nwritten;

    if (!bb)
        return ERROR_INT("bb not defined", __func__, 1);
    if (!src)
        return ERROR_INT("src not defined", __func__, 1);
    if (nbytes == 0)
        return ERROR_INT("no bytes to read", __func__, 1);

    if ((nwritten = bb->nwritten)) {  /* move the unwritten bytes over */
        memmove(bb->array, bb->array + nwritten, bb->n - nwritten);
        bb->nwritten = 0;
        bb->n -= nwritten;
    }

        /* If necessary, expand the allocated array.  Do so by
         * by at least a factor of two. */
    navail = bb->nalloc - bb->n;
    if (nbytes > navail) {
        nadd = L_MAX(bb->nalloc, nbytes);
        if (bbufferExtendArray(bb, nadd))
            return ERROR_INT("extension failed", __func__, 1);
    }

        /* Read in the new bytes */
    memcpy(bb->array + bb->n, src, nbytes);
    bb->n += nbytes;
    return 0;
}


/*!
 * \brief   bbufferReadStream()
 *
 * \param[in]    bb       bbuffer
 * \param[in]    fp       source stream from which bytes are read
 * \param[in]    nbytes   bytes to be read
 * \return  0 if OK, 1 on error
 */
l_ok
bbufferReadStream(L_BBUFFER  *bb,
                  FILE       *fp,
                  l_int32     nbytes)
{
l_int32  navail, nadd, nread, nwritten;

    if (!bb)
        return ERROR_INT("bb not defined", __func__, 1);
    if (!fp)
        return ERROR_INT("fp not defined", __func__, 1);
    if (nbytes == 0)
        return ERROR_INT("no bytes to read", __func__, 1);

    if ((nwritten = bb->nwritten)) {  /* move any unwritten bytes over */
        memmove(bb->array, bb->array + nwritten, bb->n - nwritten);
        bb->nwritten = 0;
        bb->n -= nwritten;
    }

        /* If necessary, expand the allocated array.  Do so by
         * by at least a factor of two. */
    navail = bb->nalloc - bb->n;
    if (nbytes > navail) {
        nadd = L_MAX(bb->nalloc, nbytes);
        if (bbufferExtendArray(bb, nadd))
            return ERROR_INT("extension failed", __func__, 1);
    }

        /* Read in the new bytes */
    nread = fread(bb->array + bb->n, 1, nbytes, fp);
    bb->n += nread;

    return 0;
}


/*!
 * \brief   bbufferExtendArray()
 *
 * \param[in]    bb       bbuffer
 * \param[in]    nbytes   number of bytes to extend array size
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) reallocNew() copies all bb->nalloc bytes, even though
 *          only bb->n are data.
 * </pre>
 */
l_ok
bbufferExtendArray(L_BBUFFER  *bb,
                   l_int32     nbytes)
{
    if (!bb)
        return ERROR_INT("bb not defined", __func__, 1);

    if ((bb->array = (l_uint8 *)reallocNew((void **)&bb->array,
                                bb->nalloc,
                                bb->nalloc + nbytes)) == NULL)
            return ERROR_INT("new ptr array not returned", __func__, 1);

    bb->nalloc += nbytes;
    return 0;
}


/*--------------------------------------------------------------------------*
 *                  Operations to write data FROM a BBuffer                 *
 *--------------------------------------------------------------------------*/
/*!
 * \brief   bbufferWrite()
 *
 * \param[in]    bb       bbuffer
 * \param[in]    dest     dest memory buffer to which bytes are written
 * \param[in]    nbytes   bytes requested to be written
 * \param[out]   pnout    bytes actually written
 * \return  0 if OK, 1 on error
 */
l_ok
bbufferWrite(L_BBUFFER  *bb,
             l_uint8    *dest,
             size_t      nbytes,
             size_t     *pnout)
{
size_t  nleft, nout;

    if (!bb)
        return ERROR_INT("bb not defined", __func__, 1);
    if (!dest)
        return ERROR_INT("dest not defined", __func__, 1);
    if (nbytes <= 0)
        return ERROR_INT("no bytes requested to write", __func__, 1);
    if (!pnout)
        return ERROR_INT("&nout not defined", __func__, 1);

    nleft = bb->n - bb->nwritten;
    nout = L_MIN(nleft, nbytes);
    *pnout = nout;

    if (nleft == 0) {   /* nothing to write; reinitialize the buffer */
        bb->n = 0;
        bb->nwritten = 0;
        return 0;
    }

        /* nout > 0; transfer the data out */
    memcpy(dest, bb->array + bb->nwritten, nout);
    bb->nwritten += nout;

        /* If all written; "empty" the buffer */
    if (nout == nleft) {
        bb->n = 0;
        bb->nwritten = 0;
    }

    return 0;
}


/*!
 * \brief   bbufferWriteStream()
 *
 * \param[in]    bb       bbuffer
 * \param[in]    fp       dest stream to which bytes are written
 * \param[in]    nbytes   bytes requested to be written
 * \param[out]   pnout    bytes actually written
 * \return  0 if OK, 1 on error
 */
l_ok
bbufferWriteStream(L_BBUFFER  *bb,
                   FILE       *fp,
                   size_t      nbytes,
                   size_t     *pnout)
{
size_t  nleft, nout;

    if (!bb)
        return ERROR_INT("bb not defined", __func__, 1);
    if (!fp)
        return ERROR_INT("output stream not defined", __func__, 1);
    if (nbytes <= 0)
        return ERROR_INT("no bytes requested to write", __func__, 1);
    if (!pnout)
        return ERROR_INT("&nout not defined", __func__, 1);

    nleft = bb->n - bb->nwritten;
    nout = L_MIN(nleft, nbytes);
    *pnout = nout;

    if (nleft == 0) {   /* nothing to write; reinitialize the buffer */
        bb->n = 0;
        bb->nwritten = 0;
        return 0;
    }

        /* nout > 0; transfer the data out */
    fwrite(bb->array + bb->nwritten, 1, nout, fp);
    bb->nwritten += nout;

        /* If all written; "empty" the buffer */
    if (nout == nleft) {
        bb->n = 0;
        bb->nwritten = 0;
    }

    return 0;
}

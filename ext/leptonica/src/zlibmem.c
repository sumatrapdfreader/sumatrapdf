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
 * \file  zlibmem.c
 * <pre>
 *
 *      zlib operations in memory, using bbuffer
 *          l_uint8   *zlibCompress()
 *          l_uint8   *zlibUncompress()
 *
 *
 *    This provides an example use of the byte buffer utility
 *    (see bbuffer.c for details of how the bbuffer works internally).
 *    We use zlib to compress and decompress a byte array from
 *    one memory buffer to another.  The standard method uses streams,
 *    but here we use the bbuffer as an expandable queue of pixels
 *    for both the reading and writing sides of each operation.
 *
 *    With memory mapping, one should be able to compress between
 *    memory buffers by using the file system to buffer everything in
 *    the background, but the bbuffer implementation is more portable.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

/* --------------------------------------------*/
#if  HAVE_LIBZ   /* defined in environ.h */
/* --------------------------------------------*/

#include "zlib.h"

static const l_int32  L_BUF_SIZE = 32768;
static const l_int32  ZLIB_COMPRESSION_LEVEL = 6;

#ifndef  NO_CONSOLE_IO
#define  DEBUG     0
#endif  /* ~NO_CONSOLE_IO */


/*!
 * \brief   zlibCompress()
 *
 * \param[in]    datain    byte buffer with input data
 * \param[in]    nin       number of bytes of input data
 * \param[out]   pnout     number of bytes of output data
 * \return  dataout compressed data, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) We repeatedly read in and fill up an input buffer,
 *          compress the data, and read it back out.  zlib
 *          uses two byte buffers internally in the z_stream
 *          data structure.  We use the bbuffers to feed data
 *          into the fixed bufferin, and feed it out of bufferout,
 *          in the same way that a pair of streams would normally
 *          be used if the data were being read from one file
 *          and written to another.  This is done iteratively,
 *          compressing L_BUF_SIZE bytes of input data at a time.
 * </pre>
 */
l_uint8 *
zlibCompress(const l_uint8  *datain,
             size_t          nin,
             size_t         *pnout)
{
l_uint8    *dataout;
l_int32     status, success;
l_int32     flush;
size_t      nbytes;
l_uint8    *bufferin, *bufferout;
L_BBUFFER  *bbin, *bbout;
z_stream    z;

    if (!datain)
        return (l_uint8 *)ERROR_PTR("datain not defined", __func__, NULL);

        /* Set up fixed size buffers used in z_stream */
    bufferin = (l_uint8 *)LEPT_CALLOC(L_BUF_SIZE, sizeof(l_uint8));
    bufferout = (l_uint8 *)LEPT_CALLOC(L_BUF_SIZE, sizeof(l_uint8));

        /* Set up bbuffers and load bbin with the data */
    bbin = bbufferCreate(datain, nin);
    bbout = bbufferCreate(NULL, 0);

    success = TRUE;
    if (!bufferin || !bufferout || !bbin || !bbout) {
        L_ERROR("calloc fail for buffer\n", __func__);
        success = FALSE;
        goto cleanup_arrays;
    }

    z.zalloc = (alloc_func)0;
    z.zfree = (free_func)0;
    z.opaque = (voidpf)0;
    z.next_in = bufferin;
    z.avail_in = 0;
    z.next_out = bufferout;
    z.avail_out = L_BUF_SIZE;

    status = deflateInit(&z, ZLIB_COMPRESSION_LEVEL);
    if (status != Z_OK) {
        L_ERROR("deflateInit failed\n", __func__);
        success = FALSE;
        goto cleanup_arrays;
    }

    do {
        if (z.avail_in == 0) {
            z.next_in = bufferin;
            bbufferWrite(bbin, bufferin, L_BUF_SIZE, &nbytes);
#if DEBUG
            lept_stderr(" wrote %zu bytes to bufferin\n", nbytes);
#endif  /* DEBUG */
            z.avail_in = nbytes;
        }
        flush = (bbin->n) ? Z_SYNC_FLUSH : Z_FINISH;
        status = deflate(&z, flush);
#if DEBUG
        lept_stderr(" status is %d, bytesleft = %u, totalout = %zu\n",
                  status, z.avail_out, z.total_out);
#endif  /* DEBUG */
        nbytes = L_BUF_SIZE - z.avail_out;
        if (nbytes) {
            bbufferRead(bbout, bufferout, nbytes);
#if DEBUG
            lept_stderr(" read %zu bytes from bufferout\n", nbytes);
#endif  /* DEBUG */
        }
        z.next_out = bufferout;
        z.avail_out = L_BUF_SIZE;
    } while (flush != Z_FINISH);

    deflateEnd(&z);

cleanup_arrays:
    if (success) {
        dataout = bbufferDestroyAndSaveData(&bbout, pnout);
    } else {
        dataout = NULL;
        bbufferDestroy(&bbout);
    }
    bbufferDestroy(&bbin);
    LEPT_FREE(bufferin);
    LEPT_FREE(bufferout);
    return dataout;
}


/*!
 * \brief   zlibUncompress()
 *
 * \param[in]    datain    byte buffer with compressed input data
 * \param[in]    nin       number of bytes of input data
 * \param[out]   pnout     number of bytes of output data
 * \return  dataout uncompressed data, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) See zlibCompress().
 * </pre>
 */
l_uint8 *
zlibUncompress(const l_uint8  *datain,
               size_t          nin,
               size_t         *pnout)
{
l_uint8    *dataout;
l_uint8    *bufferin, *bufferout;
l_int32     status, success;
size_t      nbytes;
L_BBUFFER  *bbin, *bbout;
z_stream    z;

    if (!datain)
        return (l_uint8 *)ERROR_PTR("datain not defined", __func__, NULL);

        /* Set up fixed size buffers used in z_stream */
    bufferin = (l_uint8 *)LEPT_CALLOC(L_BUF_SIZE, sizeof(l_uint8));
    bufferout = (l_uint8 *)LEPT_CALLOC(L_BUF_SIZE, sizeof(l_uint8));

        /* Set up bbuffers and load bbin with the data */
    bbin = bbufferCreate(datain, nin);
    bbout = bbufferCreate(NULL, 0);

    success = TRUE;
    if (!bufferin || !bufferout || !bbin || !bbout) {
        L_ERROR("calloc fail for buffer\n", __func__);
        success = FALSE;
        goto cleanup_arrays;
    }

    z.zalloc = (alloc_func)0;
    z.zfree = (free_func)0;
    z.next_in = bufferin;
    z.avail_in = 0;
    z.next_out = bufferout;
    z.avail_out = L_BUF_SIZE;

    status = inflateInit(&z);
    if (status != Z_OK) {
        L_ERROR("inflateInit fail for buffer\n", __func__);
        success = FALSE;
        goto cleanup_arrays;
    }

    for ( ; ; ) {
        if (z.avail_in == 0) {
            z.next_in = bufferin;
            bbufferWrite(bbin, bufferin, L_BUF_SIZE, &nbytes);
#if DEBUG
            lept_stderr(" wrote %d bytes to bufferin\n", nbytes);
#endif  /* DEBUG */
            z.avail_in = nbytes;
        }
        if (z.avail_in == 0)
            break;
        status = inflate(&z, Z_SYNC_FLUSH);
#if DEBUG
        lept_stderr(" status is %d, bytesleft = %d, totalout = %d\n",
                status, z.avail_out, z.total_out);
#endif  /* DEBUG */
        nbytes = L_BUF_SIZE - z.avail_out;
        if (nbytes) {
            bbufferRead(bbout, bufferout, nbytes);
#if DEBUG
            lept_stderr(" read %d bytes from bufferout\n", nbytes);
#endif  /* DEBUG */
        }
        z.next_out = bufferout;
        z.avail_out = L_BUF_SIZE;
    }

    inflateEnd(&z);

cleanup_arrays:
    if (success) {
        dataout = bbufferDestroyAndSaveData(&bbout, pnout);
    } else {
        dataout = NULL;
        bbufferDestroy(&bbout);
    }
    bbufferDestroy(&bbin);
    LEPT_FREE(bufferin);
    LEPT_FREE(bufferout);
    return dataout;
}

/* --------------------------------------------*/
#endif  /* HAVE_LIBZ */
/* --------------------------------------------*/

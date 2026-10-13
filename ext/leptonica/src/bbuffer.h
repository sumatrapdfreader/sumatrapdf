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

#ifndef  LEPTONICA_BBUFFER_H
#define  LEPTONICA_BBUFFER_H

/*!
 * \file bbuffer.h
 *
 * <pre>
 *      Expandable byte buffer for reading data in from memory and
 *      writing data out to other memory.
 *
 *      This implements a queue of bytes, so data read in is put
 *      on the "back" of the queue (i.e., the end of the byte array)
 *      and data written out is taken from the "front" of the queue
 *      (i.e., from an index marker "nwritten" that is initially set at
 *      the beginning of the array.)  As usual with expandable
 *      arrays, we keep the size of the allocated array and the
 *      number of bytes that have been read into the array.
 *
 *      For implementation details, see bbuffer.c.
 * </pre>
 */

/*! Expandable byte buffer for memory read/write operations */
struct L_ByteBuffer
{
    l_int32      nalloc;       /*!< size of allocated byte array            */
    l_int32      n;            /*!< number of bytes read into to the array  */
    l_int32      nwritten;     /*!< number of bytes written from the array  */
    l_uint8     *array;        /*!< byte array                              */
};
typedef struct L_ByteBuffer L_BBUFFER;


#endif  /* LEPTONICA_BBUFFER_H */

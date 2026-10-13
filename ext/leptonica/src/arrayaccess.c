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
 * \file arrayaccess.c
 * <pre>
 *
 *     Access within an array of 32-bit words
 *
 *           l_int32     l_getDataBit()
 *           void        l_setDataBit()
 *           void        l_clearDataBit()
 *           void        l_setDataBitVal()
 *           l_int32     l_getDataDibit()
 *           void        l_setDataDibit()
 *           void        l_clearDataDibit()
 *           l_int32     l_getDataQbit()
 *           void        l_setDataQbit()
 *           void        l_clearDataQbit()
 *           l_int32     l_getDataByte()
 *           void        l_setDataByte()
 *           l_int32     l_getDataTwoBytes()
 *           void        l_setDataTwoBytes()
 *           l_int32     l_getDataFourBytes()
 *           void        l_setDataFourBytes()
 *
 *     Note that these all require 32-bit alignment, and hence an input
 *     ptr to l_uint32.  However, this is not enforced by the compiler.
 *     Instead, we allow the use of a void* ptr, because the line ptrs
 *     are an efficient way to get random access (see pixGetLinePtrs()).
 *     It is then necessary to cast internally within each function
 *     because ptr arithmetic requires knowing the size of the units
 *     being referenced.
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

/*----------------------------------------------------------------------*
 *                 Access within an array of 32-bit words               *
 *----------------------------------------------------------------------*/
/*!
 * \brief   l_getDataBit()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \return  val of the nth 1-bit pixel.
 */
l_int32
l_getDataBit(const void *line,
             l_int32     n)
{
    return (*((const l_uint32 *)line + (n >> 5)) >> (31 - (n & 31))) & 1;
}


/*!
 * \brief   l_setDataBit()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \return  void
 *
 *  Action: sets the pixel to 1
 */
void
l_setDataBit(void    *line,
             l_int32  n)
{
    *((l_uint32 *)line + (n >> 5)) |= (0x80000000 >> (n & 31));
}


/*!
 * \brief   l_clearDataBit()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \return  void
 *
 *  Action: sets the 1-bit pixel to 0
 */
void
l_clearDataBit(void    *line,
               l_int32  n)
{
    *((l_uint32 *)line + (n >> 5)) &= ~(0x80000000 >> (n & 31));
}


/*!
 * \brief   l_setDataBitVal()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \param[in]    val   val to be inserted: 0 or 1
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) This is an accessor for a 1 bpp pix.
 *      (2) It is actually a little slower than using:
 *            if (val == 0)
 *                l_ClearDataBit(line, n);
 *            else
 *                l_SetDataBit(line, n);
 * </pre>
 */
void
l_setDataBitVal(void    *line,
                l_int32  n,
                l_int32  val)
{
l_uint32    *pword;

    pword = (l_uint32 *)line + (n >> 5);
    *pword &= ~(0x80000000 >> (n & 31));  /* clear */
    *pword |= (l_uint32)val << (31 - (n & 31));   /* set */
}


/*!
 * \brief   l_getDataDibit()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \return  val of the nth 2-bit pixel.
 */
l_int32
l_getDataDibit(const void *line,
               l_int32     n)
{
    return (*((const l_uint32 *)line + (n >> 4)) >> (2 * (15 - (n & 15)))) & 3;
}


/*!
 * \brief   l_setDataDibit()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \param[in]    val   val to be inserted: 0 - 3
 * \return  void
 */
void
l_setDataDibit(void    *line,
               l_int32  n,
               l_int32  val)
{
l_uint32    *pword;

    pword = (l_uint32 *)line + (n >> 4);
    *pword &= ~(0xc0000000 >> (2 * (n & 15)));  /* clear */
    *pword |= (l_uint32)(val & 3) << (30 - 2 * (n & 15));   /* set */
}


/*!
 * \brief   l_clearDataDibit()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \return  void
 *
 *  Action: sets the 2-bit pixel to 0
 */
void
l_clearDataDibit(void    *line,
                 l_int32  n)
{
    *((l_uint32 *)line + (n >> 4)) &= ~(0xc0000000 >> (2 * (n & 15)));
}


/*!
 * \brief   l_getDataQbit()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \return  val of the nth 4-bit pixel.
 */
l_int32
l_getDataQbit(const void *line,
              l_int32     n)
{
    return (*((const l_uint32 *)line + (n >> 3)) >> (4 * (7 - (n & 7)))) & 0xf;
}


/*!
 * \brief   l_setDataQbit()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \param[in]    val   val to be inserted: 0 - 0xf
 * \return  void
 */
void
l_setDataQbit(void    *line,
              l_int32  n,
              l_int32  val)
{
l_uint32    *pword;

    pword = (l_uint32 *)line + (n >> 3);
    *pword &= ~(0xf0000000 >> (4 * (n & 7)));  /* clear */
    *pword |= (l_uint32)(val & 15) << (28 - 4 * (n & 7));   /* set */
}


/*!
 * \brief   l_clearDataQbit()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \return  void
 *
 *  Action: sets the 4-bit pixel to 0
 */
void
l_clearDataQbit(void    *line,
                l_int32  n)
{
    *((l_uint32 *)line + (n >> 3)) &= ~(0xf0000000 >> (4 * (n & 7)));
}


/*!
 * \brief   l_getDataByte()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \return  value of the n-th byte pixel
 */
l_int32
l_getDataByte(const void *line,
              l_int32     n)
{
#ifdef  L_BIG_ENDIAN
    return *((const l_uint8 *)line + n);
#else  /* L_LITTLE_ENDIAN */
    return *(l_uint8 *)((l_uintptr_t)((const l_uint8 *)line + n) ^ 3);
#endif  /* L_BIG_ENDIAN */
}


/*!
 * \brief   l_setDataByte()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \param[in]    val   val to be inserted: 0 - 0xff
 * \return  void
 */
void
l_setDataByte(void    *line,
              l_int32  n,
              l_int32  val)
{
#ifdef  L_BIG_ENDIAN
    *((l_uint8 *)line + n) = val;
#else  /* L_LITTLE_ENDIAN */
    *(l_uint8 *)((l_uintptr_t)((l_uint8 *)line + n) ^ 3) = val;
#endif  /* L_BIG_ENDIAN */
}


/*!
 * \brief   l_getDataTwoBytes()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \return  value of the n-th 2-byte pixel
 */
l_int32
l_getDataTwoBytes(const void *line,
                  l_int32     n)
{
#ifdef  L_BIG_ENDIAN
    return *((const l_uint16 *)line + n);
#else  /* L_LITTLE_ENDIAN */
    return *(l_uint16 *)((l_uintptr_t)((const l_uint16 *)line + n) ^ 2);
#endif  /* L_BIG_ENDIAN */
}


/*!
 * \brief   l_setDataTwoBytes()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \param[in]    val   val to be inserted: 0 - 0xffff
 * \return  void
 */
void
l_setDataTwoBytes(void    *line,
                  l_int32  n,
                  l_int32  val)
{
#ifdef  L_BIG_ENDIAN
    *((l_uint16 *)line + n) = val;
#else  /* L_LITTLE_ENDIAN */
    *(l_uint16 *)((l_uintptr_t)((l_uint16 *)line + n) ^ 2) = val;
#endif  /* L_BIG_ENDIAN */
}


/*!
 * \brief   l_getDataFourBytes()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \return  value of the n-th 4-byte pixel
 */
l_int32
l_getDataFourBytes(const void *line,
                   l_int32     n)
{
    return *((const l_uint32 *)line + n);
}


/*!
 * \brief   l_setDataFourBytes()
 *
 * \param[in]    line  ptr to beginning of data line
 * \param[in]    n     pixel index
 * \param[in]    val   val to be inserted: 0 - 0xffffffff
 * \return  void
 */
void
l_setDataFourBytes(void    *line,
                   l_int32  n,
                   l_int32  val)
{
    *((l_uint32 *)line + n) = val;
}

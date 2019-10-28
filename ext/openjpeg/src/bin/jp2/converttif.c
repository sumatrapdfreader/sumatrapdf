/*
 * The copyright in this software is being made available under the 2-clauses
 * BSD License, included below. This software may be subject to other third
 * party and contributor rights, including patent rights, and no such rights
 * are granted under this license.
 *
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux
 * Copyright (c) 2003-2014, Antonin Descampe
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * Copyright (c) 2006-2007, Parvatha Elangovan
 * Copyright (c) 2015, Matthieu Darbois
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "opj_apps_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#ifndef OPJ_HAVE_LIBTIFF
# error OPJ_HAVE_LIBTIFF_NOT_DEFINED
#endif /* OPJ_HAVE_LIBTIFF */

#include <tiffio.h>
#include "openjpeg.h"
#include "convert.h"
#include "opj_inttypes.h"

/* -->> -->> -->> -->>

 TIFF IMAGE FORMAT

 <<-- <<-- <<-- <<-- */
#define PUTBITS2(s, nb) \
    trailing <<= remaining; \
    trailing |= (unsigned int)((s) >> (nb - remaining)); \
    *pDst++ = (OPJ_BYTE)trailing; \
    trailing = (unsigned int)((s) & ((1U << (nb - remaining)) - 1U)); \
    if (nb >= (remaining + 8)) { \
        *pDst++ = (OPJ_BYTE)(trailing >> (nb - (remaining + 8))); \
        trailing &= (unsigned int)((1U << (nb - (remaining + 8))) - 1U); \
        remaining += 16 - nb; \
    } else { \
        remaining += 8 - nb; \
    }

#define PUTBITS(s, nb) \
  if (nb >= remaining) { \
        PUTBITS2(s, nb) \
    } else { \
        trailing <<= nb; \
        trailing |= (unsigned int)(s); \
        remaining -= nb; \
    }
#define FLUSHBITS() \
    if (remaining != 8) { \
        trailing <<= remaining; \
        *pDst++ = (OPJ_BYTE)trailing; \
    }

static void tif_32sto3u(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
                        OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;

    for (i = 0; i < (length & ~(OPJ_SIZE_T)7U); i += 8U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = (OPJ_UINT32)pSrc[i + 1];
        OPJ_UINT32 src2 = (OPJ_UINT32)pSrc[i + 2];
        OPJ_UINT32 src3 = (OPJ_UINT32)pSrc[i + 3];
        OPJ_UINT32 src4 = (OPJ_UINT32)pSrc[i + 4];
        OPJ_UINT32 src5 = (OPJ_UINT32)pSrc[i + 5];
        OPJ_UINT32 src6 = (OPJ_UINT32)pSrc[i + 6];
        OPJ_UINT32 src7 = (OPJ_UINT32)pSrc[i + 7];

        *pDst++ = (OPJ_BYTE)((src0 << 5) | (src1 << 2) | (src2 >> 1));
        *pDst++ = (OPJ_BYTE)((src2 << 7) | (src3 << 4) | (src4 << 1) | (src5 >> 2));
        *pDst++ = (OPJ_BYTE)((src5 << 6) | (src6 << 3) | (src7));
    }

    if (length & 7U) {
        unsigned int trailing = 0U;
        int remaining = 8U;
        length &= 7U;
        PUTBITS((OPJ_UINT32)pSrc[i + 0], 3)
        if (length > 1U) {
            PUTBITS((OPJ_UINT32)pSrc[i + 1], 3)
            if (length > 2U) {
                PUTBITS((OPJ_UINT32)pSrc[i + 2], 3)
                if (length > 3U) {
                    PUTBITS((OPJ_UINT32)pSrc[i + 3], 3)
                    if (length > 4U) {
                        PUTBITS((OPJ_UINT32)pSrc[i + 4], 3)
                        if (length > 5U) {
                            PUTBITS((OPJ_UINT32)pSrc[i + 5], 3)
                            if (length > 6U) {
                                PUTBITS((OPJ_UINT32)pSrc[i + 6], 3)
                            }
                        }
                    }
                }
            }
        }
        FLUSHBITS()
    }
}

static void tif_32sto5u(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
                        OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;

    for (i = 0; i < (length & ~(OPJ_SIZE_T)7U); i += 8U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = (OPJ_UINT32)pSrc[i + 1];
        OPJ_UINT32 src2 = (OPJ_UINT32)pSrc[i + 2];
        OPJ_UINT32 src3 = (OPJ_UINT32)pSrc[i + 3];
        OPJ_UINT32 src4 = (OPJ_UINT32)pSrc[i + 4];
        OPJ_UINT32 src5 = (OPJ_UINT32)pSrc[i + 5];
        OPJ_UINT32 src6 = (OPJ_UINT32)pSrc[i + 6];
        OPJ_UINT32 src7 = (OPJ_UINT32)pSrc[i + 7];

        *pDst++ = (OPJ_BYTE)((src0 << 3) | (src1 >> 2));
        *pDst++ = (OPJ_BYTE)((src1 << 6) | (src2 << 1) | (src3 >> 4));
        *pDst++ = (OPJ_BYTE)((src3 << 4) | (src4 >> 1));
        *pDst++ = (OPJ_BYTE)((src4 << 7) | (src5 << 2) | (src6 >> 3));
        *pDst++ = (OPJ_BYTE)((src6 << 5) | (src7));

    }

    if (length & 7U) {
        unsigned int trailing = 0U;
        int remaining = 8U;
        length &= 7U;
        PUTBITS((OPJ_UINT32)pSrc[i + 0], 5)
        if (length > 1U) {
            PUTBITS((OPJ_UINT32)pSrc[i + 1], 5)
            if (length > 2U) {
                PUTBITS((OPJ_UINT32)pSrc[i + 2], 5)
                if (length > 3U) {
                    PUTBITS((OPJ_UINT32)pSrc[i + 3], 5)
                    if (length > 4U) {
                        PUTBITS((OPJ_UINT32)pSrc[i + 4], 5)
                        if (length > 5U) {
                            PUTBITS((OPJ_UINT32)pSrc[i + 5], 5)
                            if (length > 6U) {
                                PUTBITS((OPJ_UINT32)pSrc[i + 6], 5)
                            }
                        }
                    }
                }
            }
        }
        FLUSHBITS()
    }
}

static void tif_32sto7u(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
                        OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;

    for (i = 0; i < (length & ~(OPJ_SIZE_T)7U); i += 8U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = (OPJ_UINT32)pSrc[i + 1];
        OPJ_UINT32 src2 = (OPJ_UINT32)pSrc[i + 2];
        OPJ_UINT32 src3 = (OPJ_UINT32)pSrc[i + 3];
        OPJ_UINT32 src4 = (OPJ_UINT32)pSrc[i + 4];
        OPJ_UINT32 src5 = (OPJ_UINT32)pSrc[i + 5];
        OPJ_UINT32 src6 = (OPJ_UINT32)pSrc[i + 6];
        OPJ_UINT32 src7 = (OPJ_UINT32)pSrc[i + 7];

        *pDst++ = (OPJ_BYTE)((src0 << 1) | (src1 >> 6));
        *pDst++ = (OPJ_BYTE)((src1 << 2) | (src2 >> 5));
        *pDst++ = (OPJ_BYTE)((src2 << 3) | (src3 >> 4));
        *pDst++ = (OPJ_BYTE)((src3 << 4) | (src4 >> 3));
        *pDst++ = (OPJ_BYTE)((src4 << 5) | (src5 >> 2));
        *pDst++ = (OPJ_BYTE)((src5 << 6) | (src6 >> 1));
        *pDst++ = (OPJ_BYTE)((src6 << 7) | (src7));
    }

    if (length & 7U) {
        unsigned int trailing = 0U;
        int remaining = 8U;
        length &= 7U;
        PUTBITS((OPJ_UINT32)pSrc[i + 0], 7)
        if (length > 1U) {
            PUTBITS((OPJ_UINT32)pSrc[i + 1], 7)
            if (length > 2U) {
                PUTBITS((OPJ_UINT32)pSrc[i + 2], 7)
                if (length > 3U) {
                    PUTBITS((OPJ_UINT32)pSrc[i + 3], 7)
                    if (length > 4U) {
                        PUTBITS((OPJ_UINT32)pSrc[i + 4], 7)
                        if (length > 5U) {
                            PUTBITS((OPJ_UINT32)pSrc[i + 5], 7)
                            if (length > 6U) {
                                PUTBITS((OPJ_UINT32)pSrc[i + 6], 7)
                            }
                        }
                    }
                }
            }
        }
        FLUSHBITS()
    }
}

static void tif_32sto9u(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
                        OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;

    for (i = 0; i < (length & ~(OPJ_SIZE_T)7U); i += 8U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = (OPJ_UINT32)pSrc[i + 1];
        OPJ_UINT32 src2 = (OPJ_UINT32)pSrc[i + 2];
        OPJ_UINT32 src3 = (OPJ_UINT32)pSrc[i + 3];
        OPJ_UINT32 src4 = (OPJ_UINT32)pSrc[i + 4];
        OPJ_UINT32 src5 = (OPJ_UINT32)pSrc[i + 5];
        OPJ_UINT32 src6 = (OPJ_UINT32)pSrc[i + 6];
        OPJ_UINT32 src7 = (OPJ_UINT32)pSrc[i + 7];

        *pDst++ = (OPJ_BYTE)((src0 >> 1));
        *pDst++ = (OPJ_BYTE)((src0 << 7) | (src1 >> 2));
        *pDst++ = (OPJ_BYTE)((src1 << 6) | (src2 >> 3));
        *pDst++ = (OPJ_BYTE)((src2 << 5) | (src3 >> 4));
        *pDst++ = (OPJ_BYTE)((src3 << 4) | (src4 >> 5));
        *pDst++ = (OPJ_BYTE)((src4 << 3) | (src5 >> 6));
        *pDst++ = (OPJ_BYTE)((src5 << 2) | (src6 >> 7));
        *pDst++ = (OPJ_BYTE)((src6 << 1) | (src7 >> 8));
        *pDst++ = (OPJ_BYTE)(src7);
    }

    if (length & 7U) {
        unsigned int trailing = 0U;
        int remaining = 8U;
        length &= 7U;
        PUTBITS2((OPJ_UINT32)pSrc[i + 0], 9)
        if (length > 1U) {
            PUTBITS2((OPJ_UINT32)pSrc[i + 1], 9)
            if (length > 2U) {
                PUTBITS2((OPJ_UINT32)pSrc[i + 2], 9)
                if (length > 3U) {
                    PUTBITS2((OPJ_UINT32)pSrc[i + 3], 9)
                    if (length > 4U) {
                        PUTBITS2((OPJ_UINT32)pSrc[i + 4], 9)
                        if (length > 5U) {
                            PUTBITS2((OPJ_UINT32)pSrc[i + 5], 9)
                            if (length > 6U) {
                                PUTBITS2((OPJ_UINT32)pSrc[i + 6], 9)
                            }
                        }
                    }
                }
            }
        }
        FLUSHBITS()
    }
}

static void tif_32sto10u(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
                         OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)3U); i += 4U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = (OPJ_UINT32)pSrc[i + 1];
        OPJ_UINT32 src2 = (OPJ_UINT32)pSrc[i + 2];
        OPJ_UINT32 src3 = (OPJ_UINT32)pSrc[i + 3];

        *pDst++ = (OPJ_BYTE)(src0 >> 2);
        *pDst++ = (OPJ_BYTE)(((src0 & 0x3U) << 6) | (src1 >> 4));
        *pDst++ = (OPJ_BYTE)(((src1 & 0xFU) << 4) | (src2 >> 6));
        *pDst++ = (OPJ_BYTE)(((src2 & 0x3FU) << 2) | (src3 >> 8));
        *pDst++ = (OPJ_BYTE)(src3);
    }

    if (length & 3U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = 0U;
        OPJ_UINT32 src2 = 0U;
        length = length & 3U;

        if (length > 1U) {
            src1 = (OPJ_UINT32)pSrc[i + 1];
            if (length > 2U) {
                src2 = (OPJ_UINT32)pSrc[i + 2];
            }
        }
        *pDst++ = (OPJ_BYTE)(src0 >> 2);
        *pDst++ = (OPJ_BYTE)(((src0 & 0x3U) << 6) | (src1 >> 4));
        if (length > 1U) {
            *pDst++ = (OPJ_BYTE)(((src1 & 0xFU) << 4) | (src2 >> 6));
            if (length > 2U) {
                *pDst++ = (OPJ_BYTE)(((src2 & 0x3FU) << 2));
            }
        }
    }
}
static void tif_32sto11u(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
                         OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;

    for (i = 0; i < (length & ~(OPJ_SIZE_T)7U); i += 8U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = (OPJ_UINT32)pSrc[i + 1];
        OPJ_UINT32 src2 = (OPJ_UINT32)pSrc[i + 2];
        OPJ_UINT32 src3 = (OPJ_UINT32)pSrc[i + 3];
        OPJ_UINT32 src4 = (OPJ_UINT32)pSrc[i + 4];
        OPJ_UINT32 src5 = (OPJ_UINT32)pSrc[i + 5];
        OPJ_UINT32 src6 = (OPJ_UINT32)pSrc[i + 6];
        OPJ_UINT32 src7 = (OPJ_UINT32)pSrc[i + 7];

        *pDst++ = (OPJ_BYTE)((src0 >> 3));
        *pDst++ = (OPJ_BYTE)((src0 << 5) | (src1 >> 6));
        *pDst++ = (OPJ_BYTE)((src1 << 2) | (src2 >> 9));
        *pDst++ = (OPJ_BYTE)((src2 >> 1));
        *pDst++ = (OPJ_BYTE)((src2 << 7) | (src3 >> 4));
        *pDst++ = (OPJ_BYTE)((src3 << 4) | (src4 >> 7));
        *pDst++ = (OPJ_BYTE)((src4 << 1) | (src5 >> 10));
        *pDst++ = (OPJ_BYTE)((src5 >> 2));
        *pDst++ = (OPJ_BYTE)((src5 << 6) | (src6 >> 5));
        *pDst++ = (OPJ_BYTE)((src6 << 3) | (src7 >> 8));
        *pDst++ = (OPJ_BYTE)(src7);
    }

    if (length & 7U) {
        unsigned int trailing = 0U;
        int remaining = 8U;
        length &= 7U;
        PUTBITS2((OPJ_UINT32)pSrc[i + 0], 11)
        if (length > 1U) {
            PUTBITS2((OPJ_UINT32)pSrc[i + 1], 11)
            if (length > 2U) {
                PUTBITS2((OPJ_UINT32)pSrc[i + 2], 11)
                if (length > 3U) {
                    PUTBITS2((OPJ_UINT32)pSrc[i + 3], 11)
                    if (length > 4U) {
                        PUTBITS2((OPJ_UINT32)pSrc[i + 4], 11)
                        if (length > 5U) {
                            PUTBITS2((OPJ_UINT32)pSrc[i + 5], 11)
                            if (length > 6U) {
                                PUTBITS2((OPJ_UINT32)pSrc[i + 6], 11)
                            }
                        }
                    }
                }
            }
        }
        FLUSHBITS()
    }
}
static void tif_32sto12u(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
                         OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)1U); i += 2U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = (OPJ_UINT32)pSrc[i + 1];

        *pDst++ = (OPJ_BYTE)(src0 >> 4);
        *pDst++ = (OPJ_BYTE)(((src0 & 0xFU) << 4) | (src1 >> 8));
        *pDst++ = (OPJ_BYTE)(src1);
    }

    if (length & 1U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        *pDst++ = (OPJ_BYTE)(src0 >> 4);
        *pDst++ = (OPJ_BYTE)(((src0 & 0xFU) << 4));
    }
}
static void tif_32sto13u(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
                         OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;

    for (i = 0; i < (length & ~(OPJ_SIZE_T)7U); i += 8U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = (OPJ_UINT32)pSrc[i + 1];
        OPJ_UINT32 src2 = (OPJ_UINT32)pSrc[i + 2];
        OPJ_UINT32 src3 = (OPJ_UINT32)pSrc[i + 3];
        OPJ_UINT32 src4 = (OPJ_UINT32)pSrc[i + 4];
        OPJ_UINT32 src5 = (OPJ_UINT32)pSrc[i + 5];
        OPJ_UINT32 src6 = (OPJ_UINT32)pSrc[i + 6];
        OPJ_UINT32 src7 = (OPJ_UINT32)pSrc[i + 7];

        *pDst++ = (OPJ_BYTE)((src0 >> 5));
        *pDst++ = (OPJ_BYTE)((src0 << 3) | (src1 >> 10));
        *pDst++ = (OPJ_BYTE)((src1 >> 2));
        *pDst++ = (OPJ_BYTE)((src1 << 6) | (src2 >> 7));
        *pDst++ = (OPJ_BYTE)((src2 << 1) | (src3 >> 12));
        *pDst++ = (OPJ_BYTE)((src3 >> 4));
        *pDst++ = (OPJ_BYTE)((src3 << 4) | (src4 >> 9));
        *pDst++ = (OPJ_BYTE)((src4 >> 1));
        *pDst++ = (OPJ_BYTE)((src4 << 7) | (src5 >> 6));
        *pDst++ = (OPJ_BYTE)((src5 << 2) | (src6 >> 11));
        *pDst++ = (OPJ_BYTE)((src6 >> 3));
        *pDst++ = (OPJ_BYTE)((src6 << 5) | (src7 >> 8));
        *pDst++ = (OPJ_BYTE)(src7);
    }

    if (length & 7U) {
        unsigned int trailing = 0U;
        int remaining = 8U;
        length &= 7U;
        PUTBITS2((OPJ_UINT32)pSrc[i + 0], 13)
        if (length > 1U) {
            PUTBITS2((OPJ_UINT32)pSrc[i + 1], 13)
            if (length > 2U) {
                PUTBITS2((OPJ_UINT32)pSrc[i + 2], 13)
                if (length > 3U) {
                    PUTBITS2((OPJ_UINT32)pSrc[i + 3], 13)
                    if (length > 4U) {
                        PUTBITS2((OPJ_UINT32)pSrc[i + 4], 13)
                        if (length > 5U) {
                            PUTBITS2((OPJ_UINT32)pSrc[i + 5], 13)
                            if (length > 6U) {
                                PUTBITS2((OPJ_UINT32)pSrc[i + 6], 13)
                            }
                        }
                    }
                }
            }
        }
        FLUSHBITS()
    }
}
static void tif_32sto14u(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
                         OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)3U); i += 4U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = (OPJ_UINT32)pSrc[i + 1];
        OPJ_UINT32 src2 = (OPJ_UINT32)pSrc[i + 2];
        OPJ_UINT32 src3 = (OPJ_UINT32)pSrc[i + 3];

        *pDst++ = (OPJ_BYTE)(src0 >> 6);
        *pDst++ = (OPJ_BYTE)(((src0 & 0x3FU) << 2) | (src1 >> 12));
        *pDst++ = (OPJ_BYTE)(src1 >> 4);
        *pDst++ = (OPJ_BYTE)(((src1 & 0xFU) << 4) | (src2 >> 10));
        *pDst++ = (OPJ_BYTE)(src2 >> 2);
        *pDst++ = (OPJ_BYTE)(((src2 & 0x3U) << 6) | (src3 >> 8));
        *pDst++ = (OPJ_BYTE)(src3);
    }

    if (length & 3U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = 0U;
        OPJ_UINT32 src2 = 0U;
        length = length & 3U;

        if (length > 1U) {
            src1 = (OPJ_UINT32)pSrc[i + 1];
            if (length > 2U) {
                src2 = (OPJ_UINT32)pSrc[i + 2];
            }
        }
        *pDst++ = (OPJ_BYTE)(src0 >> 6);
        *pDst++ = (OPJ_BYTE)(((src0 & 0x3FU) << 2) | (src1 >> 12));
        if (length > 1U) {
            *pDst++ = (OPJ_BYTE)(src1 >> 4);
            *pDst++ = (OPJ_BYTE)(((src1 & 0xFU) << 4) | (src2 >> 10));
            if (length > 2U) {
                *pDst++ = (OPJ_BYTE)(src2 >> 2);
                *pDst++ = (OPJ_BYTE)(((src2 & 0x3U) << 6));
            }
        }
    }
}
static void tif_32sto15u(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
                         OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;

    for (i = 0; i < (length & ~(OPJ_SIZE_T)7U); i += 8U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = (OPJ_UINT32)pSrc[i + 1];
        OPJ_UINT32 src2 = (OPJ_UINT32)pSrc[i + 2];
        OPJ_UINT32 src3 = (OPJ_UINT32)pSrc[i + 3];
        OPJ_UINT32 src4 = (OPJ_UINT32)pSrc[i + 4];
        OPJ_UINT32 src5 = (OPJ_UINT32)pSrc[i + 5];
        OPJ_UINT32 src6 = (OPJ_UINT32)pSrc[i + 6];
        OPJ_UINT32 src7 = (OPJ_UINT32)pSrc[i + 7];

        *pDst++ = (OPJ_BYTE)((src0 >> 7));
        *pDst++ = (OPJ_BYTE)((src0 << 1) | (src1 >> 14));
        *pDst++ = (OPJ_BYTE)((src1 >> 6));
        *pDst++ = (OPJ_BYTE)((src1 << 2) | (src2 >> 13));
        *pDst++ = (OPJ_BYTE)((src2 >> 5));
        *pDst++ = (OPJ_BYTE)((src2 << 3) | (src3 >> 12));
        *pDst++ = (OPJ_BYTE)((src3 >> 4));
        *pDst++ = (OPJ_BYTE)((src3 << 4) | (src4 >> 11));
        *pDst++ = (OPJ_BYTE)((src4 >> 3));
        *pDst++ = (OPJ_BYTE)((src4 << 5) | (src5 >> 10));
        *pDst++ = (OPJ_BYTE)((src5 >> 2));
        *pDst++ = (OPJ_BYTE)((src5 << 6) | (src6 >> 9));
        *pDst++ = (OPJ_BYTE)((src6 >> 1));
        *pDst++ = (OPJ_BYTE)((src6 << 7) | (src7 >> 8));
        *pDst++ = (OPJ_BYTE)(src7);
    }

    if (length & 7U) {
        unsigned int trailing = 0U;
        int remaining = 8U;
        length &= 7U;
        PUTBITS2((OPJ_UINT32)pSrc[i + 0], 15)
        if (length > 1U) {
            PUTBITS2((OPJ_UINT32)pSrc[i + 1], 15)
            if (length > 2U) {
                PUTBITS2((OPJ_UINT32)pSrc[i + 2], 15)
                if (length > 3U) {
                    PUTBITS2((OPJ_UINT32)pSrc[i + 3], 15)
                    if (length > 4U) {
                        PUTBITS2((OPJ_UINT32)pSrc[i + 4], 15)
                        if (length > 5U) {
                            PUTBITS2((OPJ_UINT32)pSrc[i + 5], 15)
                            if (length > 6U) {
                                PUTBITS2((OPJ_UINT32)pSrc[i + 6], 15)
                            }
                        }
                    }
                }
            }
        }
        FLUSHBITS()
    }
}
static void tif_32sto16u(const OPJ_INT32* pSrc, OPJ_UINT16* pDst,
                         OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < length; ++i) {
        pDst[i] = (OPJ_UINT16)pSrc[i];
    }
}

int imagetotif(opj_image_t * image, const char *outfile)
{
    TIFF *tif;
    tdata_t buf;
    uint32 width, height;
    uint16 bps, tiPhoto;
    int adjust, sgnd;
    int64_t strip_size, rowStride, TIFF_MAX;
    OPJ_UINT32 i, numcomps;
    OPJ_INT32* buffer32s = NULL;
    OPJ_INT32 const* planes[4];
    convert_32s_PXCX cvtPxToCx = NULL;
    convert_32sXXx_C1R cvt32sToTif = NULL;

    bps = (uint16)image->comps[0].prec;
    planes[0] = image->comps[0].data;

    numcomps = image->numcomps;

    if (image->color_space == OPJ_CLRSPC_CMYK) {
        if (numcomps < 4U) {
            fprintf(stderr,
                    "imagetotif: CMYK images shall be composed of at least 4 planes.\n");
            fprintf(stderr, "\tAborting\n");
            return 1;
        }
        tiPhoto = PHOTOMETRIC_SEPARATED;
        if (numcomps > 4U) {
            numcomps = 4U; /* Alpha not supported */
        }
    } else if (numcomps > 2U) {
        tiPhoto = PHOTOMETRIC_RGB;
        if (numcomps > 4U) {
            numcomps = 4U;
        }
    } else {
        tiPhoto = PHOTOMETRIC_MINISBLACK;
    }
    for (i = 1U; i < numcomps; ++i) {
        if (image->comps[0].dx != image->comps[i].dx) {
            break;
        }
        if (image->comps[0].dy != image->comps[i].dy) {
            break;
        }
        if (image->comps[0].prec != image->comps[i].prec) {
            break;
        }
        if (image->comps[0].sgnd != image->comps[i].sgnd) {
            break;
        }
        planes[i] = image->comps[i].data;
    }
    if (i != numcomps) {
        fprintf(stderr,
                "imagetotif: All components shall have the same subsampling, same bit depth.\n");
        fprintf(stderr, "\tAborting\n");
        return 1;
    }

    if (bps > 16) {
        bps = 0;
    }
    if (bps == 0) {
        fprintf(stderr, "imagetotif: Bits=%d, Only 1 to 16 bits implemented\n", bps);
        fprintf(stderr, "\tAborting\n");
        return 1;
    }
    tif = TIFFOpen(outfile, "wb");
    if (!tif) {
        fprintf(stderr, "imagetotif:failed to open %s for writing\n", outfile);
        return 1;
    }
    for (i = 0U; i < numcomps; ++i) {
        clip_component(&(image->comps[i]), image->comps[0].prec);
    }
    cvtPxToCx = convert_32s_PXCX_LUT[numcomps];
    switch (bps) {
    case 1:
    case 2:
    case 4:
    case 6:
    case 8:
        cvt32sToTif = convert_32sXXu_C1R_LUT[bps];
        break;
    case 3:
        cvt32sToTif = tif_32sto3u;
        break;
    case 5:
        cvt32sToTif = tif_32sto5u;
        break;
    case 7:
        cvt32sToTif = tif_32sto7u;
        break;
    case 9:
        cvt32sToTif = tif_32sto9u;
        break;
    case 10:
        cvt32sToTif = tif_32sto10u;
        break;
    case 11:
        cvt32sToTif = tif_32sto11u;
        break;
    case 12:
        cvt32sToTif = tif_32sto12u;
        break;
    case 13:
        cvt32sToTif = tif_32sto13u;
        break;
    case 14:
        cvt32sToTif = tif_32sto14u;
        break;
    case 15:
        cvt32sToTif = tif_32sto15u;
        break;
    case 16:
        cvt32sToTif = (convert_32sXXx_C1R)tif_32sto16u;
        break;
    default:
        /* never here */
        break;
    }
    sgnd = (int)image->comps[0].sgnd;
    adjust = sgnd ? (int)(1 << (image->comps[0].prec - 1)) : 0;
    width   = (uint32)image->comps[0].w;
    height  = (uint32)image->comps[0].h;

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, (uint16)numcomps);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bps);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, tiPhoto);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, 1);
    if (sizeof(tsize_t) == 4) {
        TIFF_MAX = INT_MAX;
    } else {
        TIFF_MAX = UINT_MAX;
    }
    strip_size = (int64_t)TIFFStripSize(tif);

    if ((int64_t)width > (int64_t)(TIFF_MAX / numcomps) ||
            (int64_t)(width * numcomps) > (int64_t)(TIFF_MAX / bps) ||
            (int64_t)(width * numcomps) > (int64_t)(TIFF_MAX / (int64_t)sizeof(
                        OPJ_INT32))) {
        fprintf(stderr, "Buffer overflow\n");
        TIFFClose(tif);
        return 1;
    }
    rowStride = (int64_t)((width * numcomps * bps + 7U) / 8U);
    if (rowStride != strip_size) {
        fprintf(stderr, "Invalid TIFF strip size\n");
        TIFFClose(tif);
        return 1;
    }
    buf = malloc((OPJ_SIZE_T)strip_size);
    if (buf == NULL) {
        TIFFClose(tif);
        return 1;
    }
    buffer32s = (OPJ_INT32 *)malloc((OPJ_SIZE_T)(width * numcomps * sizeof(
                                        OPJ_INT32)));
    if (buffer32s == NULL) {
        _TIFFfree(buf);
        TIFFClose(tif);
        return 1;
    }

    for (i = 0; i < image->comps[0].h; ++i) {
        cvtPxToCx(planes, buffer32s, (OPJ_SIZE_T)width, adjust);
        cvt32sToTif(buffer32s, (OPJ_BYTE *)buf, (OPJ_SIZE_T)width * numcomps);
        (void)TIFFWriteEncodedStrip(tif, i, (void*)buf, (tsize_t)strip_size);
        planes[0] += width;
        planes[1] += width;
        planes[2] += width;
        planes[3] += width;
    }
    _TIFFfree((void*)buf);
    TIFFClose(tif);
    free(buffer32s);

    return 0;
}/* imagetotif() */

#define GETBITS(dest, nb) { \
    int needed = (nb); \
    unsigned int dst = 0U; \
    if (available == 0) { \
        val = *pSrc++; \
        available = 8; \
    } \
    while (needed > available) { \
        dst |= val & ((1U << available) - 1U); \
        needed -= available; \
        dst <<= needed; \
        val = *pSrc++; \
        available = 8; \
    } \
    dst |= (val >> (available - needed)) & ((1U << needed) - 1U); \
    available -= needed; \
    dest = (OPJ_INT32)dst; \
}

static void tif_3uto32s(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                        OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)7U); i += 8U) {
        OPJ_UINT32 val0 = *pSrc++;
        OPJ_UINT32 val1 = *pSrc++;
        OPJ_UINT32 val2 = *pSrc++;

        pDst[i + 0] = (OPJ_INT32)((val0 >> 5));
        pDst[i + 1] = (OPJ_INT32)(((val0 & 0x1FU) >> 2));
        pDst[i + 2] = (OPJ_INT32)(((val0 & 0x3U) << 1) | (val1 >> 7));
        pDst[i + 3] = (OPJ_INT32)(((val1 & 0x7FU) >> 4));
        pDst[i + 4] = (OPJ_INT32)(((val1 & 0xFU) >> 1));
        pDst[i + 5] = (OPJ_INT32)(((val1 & 0x1U) << 2) | (val2 >> 6));
        pDst[i + 6] = (OPJ_INT32)(((val2 & 0x3FU) >> 3));
        pDst[i + 7] = (OPJ_INT32)(((val2 & 0x7U)));

    }
    if (length & 7U) {
        unsigned int val;
        int available = 0;

        length = length & 7U;

        GETBITS(pDst[i + 0], 3)

        if (length > 1U) {
            GETBITS(pDst[i + 1], 3)
            if (length > 2U) {
                GETBITS(pDst[i + 2], 3)
                if (length > 3U) {
                    GETBITS(pDst[i + 3], 3)
                    if (length > 4U) {
                        GETBITS(pDst[i + 4], 3)
                        if (length > 5U) {
                            GETBITS(pDst[i + 5], 3)
                            if (length > 6U) {
                                GETBITS(pDst[i + 6], 3)
                            }
                        }
                    }
                }
            }
        }
    }
}
static void tif_5uto32s(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                        OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)7U); i += 8U) {
        OPJ_UINT32 val0 = *pSrc++;
        OPJ_UINT32 val1 = *pSrc++;
        OPJ_UINT32 val2 = *pSrc++;
        OPJ_UINT32 val3 = *pSrc++;
        OPJ_UINT32 val4 = *pSrc++;

        pDst[i + 0] = (OPJ_INT32)((val0 >> 3));
        pDst[i + 1] = (OPJ_INT32)(((val0 & 0x7U) << 2) | (val1 >> 6));
        pDst[i + 2] = (OPJ_INT32)(((val1 & 0x3FU) >> 1));
        pDst[i + 3] = (OPJ_INT32)(((val1 & 0x1U) << 4) | (val2 >> 4));
        pDst[i + 4] = (OPJ_INT32)(((val2 & 0xFU) << 1) | (val3 >> 7));
        pDst[i + 5] = (OPJ_INT32)(((val3 & 0x7FU) >> 2));
        pDst[i + 6] = (OPJ_INT32)(((val3 & 0x3U) << 3) | (val4 >> 5));
        pDst[i + 7] = (OPJ_INT32)(((val4 & 0x1FU)));

    }
    if (length & 7U) {
        unsigned int val;
        int available = 0;

        length = length & 7U;

        GETBITS(pDst[i + 0], 5)

        if (length > 1U) {
            GETBITS(pDst[i + 1], 5)
            if (length > 2U) {
                GETBITS(pDst[i + 2], 5)
                if (length > 3U) {
                    GETBITS(pDst[i + 3], 5)
                    if (length > 4U) {
                        GETBITS(pDst[i + 4], 5)
                        if (length > 5U) {
                            GETBITS(pDst[i + 5], 5)
                            if (length > 6U) {
                                GETBITS(pDst[i + 6], 5)
                            }
                        }
                    }
                }
            }
        }
    }
}
static void tif_7uto32s(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                        OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)7U); i += 8U) {
        OPJ_UINT32 val0 = *pSrc++;
        OPJ_UINT32 val1 = *pSrc++;
        OPJ_UINT32 val2 = *pSrc++;
        OPJ_UINT32 val3 = *pSrc++;
        OPJ_UINT32 val4 = *pSrc++;
        OPJ_UINT32 val5 = *pSrc++;
        OPJ_UINT32 val6 = *pSrc++;

        pDst[i + 0] = (OPJ_INT32)((val0 >> 1));
        pDst[i + 1] = (OPJ_INT32)(((val0 & 0x1U) << 6) | (val1 >> 2));
        pDst[i + 2] = (OPJ_INT32)(((val1 & 0x3U) << 5) | (val2 >> 3));
        pDst[i + 3] = (OPJ_INT32)(((val2 & 0x7U) << 4) | (val3 >> 4));
        pDst[i + 4] = (OPJ_INT32)(((val3 & 0xFU) << 3) | (val4 >> 5));
        pDst[i + 5] = (OPJ_INT32)(((val4 & 0x1FU) << 2) | (val5 >> 6));
        pDst[i + 6] = (OPJ_INT32)(((val5 & 0x3FU) << 1) | (val6 >> 7));
        pDst[i + 7] = (OPJ_INT32)(((val6 & 0x7FU)));

    }
    if (length & 7U) {
        unsigned int val;
        int available = 0;

        length = length & 7U;

        GETBITS(pDst[i + 0], 7)

        if (length > 1U) {
            GETBITS(pDst[i + 1], 7)
            if (length > 2U) {
                GETBITS(pDst[i + 2], 7)
                if (length > 3U) {
                    GETBITS(pDst[i + 3], 7)
                    if (length > 4U) {
                        GETBITS(pDst[i + 4], 7)
                        if (length > 5U) {
                            GETBITS(pDst[i + 5], 7)
                            if (length > 6U) {
                                GETBITS(pDst[i + 6], 7)
                            }
                        }
                    }
                }
            }
        }
    }
}
static void tif_9uto32s(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                        OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)7U); i += 8U) {
        OPJ_UINT32 val0 = *pSrc++;
        OPJ_UINT32 val1 = *pSrc++;
        OPJ_UINT32 val2 = *pSrc++;
        OPJ_UINT32 val3 = *pSrc++;
        OPJ_UINT32 val4 = *pSrc++;
        OPJ_UINT32 val5 = *pSrc++;
        OPJ_UINT32 val6 = *pSrc++;
        OPJ_UINT32 val7 = *pSrc++;
        OPJ_UINT32 val8 = *pSrc++;

        pDst[i + 0] = (OPJ_INT32)((val0 << 1) | (val1 >> 7));
        pDst[i + 1] = (OPJ_INT32)(((val1 & 0x7FU) << 2) | (val2 >> 6));
        pDst[i + 2] = (OPJ_INT32)(((val2 & 0x3FU) << 3) | (val3 >> 5));
        pDst[i + 3] = (OPJ_INT32)(((val3 & 0x1FU) << 4) | (val4 >> 4));
        pDst[i + 4] = (OPJ_INT32)(((val4 & 0xFU) << 5) | (val5 >> 3));
        pDst[i + 5] = (OPJ_INT32)(((val5 & 0x7U) << 6) | (val6 >> 2));
        pDst[i + 6] = (OPJ_INT32)(((val6 & 0x3U) << 7) | (val7 >> 1));
        pDst[i + 7] = (OPJ_INT32)(((val7 & 0x1U) << 8) | (val8));

    }
    if (length & 7U) {
        unsigned int val;
        int available = 0;

        length = length & 7U;

        GETBITS(pDst[i + 0], 9)

        if (length > 1U) {
            GETBITS(pDst[i + 1], 9)
            if (length > 2U) {
                GETBITS(pDst[i + 2], 9)
                if (length > 3U) {
                    GETBITS(pDst[i + 3], 9)
                    if (length > 4U) {
                        GETBITS(pDst[i + 4], 9)
                        if (length > 5U) {
                            GETBITS(pDst[i + 5], 9)
                            if (length > 6U) {
                                GETBITS(pDst[i + 6], 9)
                            }
                        }
                    }
                }
            }
        }
    }
}
static void tif_10uto32s(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                         OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)3U); i += 4U) {
        OPJ_UINT32 val0 = *pSrc++;
        OPJ_UINT32 val1 = *pSrc++;
        OPJ_UINT32 val2 = *pSrc++;
        OPJ_UINT32 val3 = *pSrc++;
        OPJ_UINT32 val4 = *pSrc++;

        pDst[i + 0] = (OPJ_INT32)((val0 << 2) | (val1 >> 6));
        pDst[i + 1] = (OPJ_INT32)(((val1 & 0x3FU) << 4) | (val2 >> 4));
        pDst[i + 2] = (OPJ_INT32)(((val2 & 0xFU) << 6) | (val3 >> 2));
        pDst[i + 3] = (OPJ_INT32)(((val3 & 0x3U) << 8) | val4);

    }
    if (length & 3U) {
        OPJ_UINT32 val0 = *pSrc++;
        OPJ_UINT32 val1 = *pSrc++;
        length = length & 3U;
        pDst[i + 0] = (OPJ_INT32)((val0 << 2) | (val1 >> 6));

        if (length > 1U) {
            OPJ_UINT32 val2 = *pSrc++;
            pDst[i + 1] = (OPJ_INT32)(((val1 & 0x3FU) << 4) | (val2 >> 4));
            if (length > 2U) {
                OPJ_UINT32 val3 = *pSrc++;
                pDst[i + 2] = (OPJ_INT32)(((val2 & 0xFU) << 6) | (val3 >> 2));
            }
        }
    }
}
static void tif_11uto32s(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                         OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)7U); i += 8U) {
        OPJ_UINT32 val0 = *pSrc++;
        OPJ_UINT32 val1 = *pSrc++;
        OPJ_UINT32 val2 = *pSrc++;
        OPJ_UINT32 val3 = *pSrc++;
        OPJ_UINT32 val4 = *pSrc++;
        OPJ_UINT32 val5 = *pSrc++;
        OPJ_UINT32 val6 = *pSrc++;
        OPJ_UINT32 val7 = *pSrc++;
        OPJ_UINT32 val8 = *pSrc++;
        OPJ_UINT32 val9 = *pSrc++;
        OPJ_UINT32 val10 = *pSrc++;

        pDst[i + 0] = (OPJ_INT32)((val0 << 3) | (val1 >> 5));
        pDst[i + 1] = (OPJ_INT32)(((val1 & 0x1FU) << 6) | (val2 >> 2));
        pDst[i + 2] = (OPJ_INT32)(((val2 & 0x3U) << 9) | (val3 << 1) | (val4 >> 7));
        pDst[i + 3] = (OPJ_INT32)(((val4 & 0x7FU) << 4) | (val5 >> 4));
        pDst[i + 4] = (OPJ_INT32)(((val5 & 0xFU) << 7) | (val6 >> 1));
        pDst[i + 5] = (OPJ_INT32)(((val6 & 0x1U) << 10) | (val7 << 2) | (val8 >> 6));
        pDst[i + 6] = (OPJ_INT32)(((val8 & 0x3FU) << 5) | (val9 >> 3));
        pDst[i + 7] = (OPJ_INT32)(((val9 & 0x7U) << 8) | (val10));

    }
    if (length & 7U) {
        unsigned int val;
        int available = 0;

        length = length & 7U;

        GETBITS(pDst[i + 0], 11)

        if (length > 1U) {
            GETBITS(pDst[i + 1], 11)
            if (length > 2U) {
                GETBITS(pDst[i + 2], 11)
                if (length > 3U) {
                    GETBITS(pDst[i + 3], 11)
                    if (length > 4U) {
                        GETBITS(pDst[i + 4], 11)
                        if (length > 5U) {
                            GETBITS(pDst[i + 5], 11)
                            if (length > 6U) {
                                GETBITS(pDst[i + 6], 11)
                            }
                        }
                    }
                }
            }
        }
    }
}
static void tif_12uto32s(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                         OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)1U); i += 2U) {
        OPJ_UINT32 val0 = *pSrc++;
        OPJ_UINT32 val1 = *pSrc++;
        OPJ_UINT32 val2 = *pSrc++;

        pDst[i + 0] = (OPJ_INT32)((val0 << 4) | (val1 >> 4));
        pDst[i + 1] = (OPJ_INT32)(((val1 & 0xFU) << 8) | val2);
    }
    if (length & 1U) {
        OPJ_UINT32 val0 = *pSrc++;
        OPJ_UINT32 val1 = *pSrc++;
        pDst[i + 0] = (OPJ_INT32)((val0 << 4) | (val1 >> 4));
    }
}
static void tif_13uto32s(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                         OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)7U); i += 8U) {
        OPJ_UINT32 val0 = *pSrc++;
        OPJ_UINT32 val1 = *pSrc++;
        OPJ_UINT32 val2 = *pSrc++;
        OPJ_UINT32 val3 = *pSrc++;
        OPJ_UINT32 val4 = *pSrc++;
        OPJ_UINT32 val5 = *pSrc++;
        OPJ_UINT32 val6 = *pSrc++;
        OPJ_UINT32 val7 = *pSrc++;
        OPJ_UINT32 val8 = *pSrc++;
        OPJ_UINT32 val9 = *pSrc++;
        OPJ_UINT32 val10 = *pSrc++;
        OPJ_UINT32 val11 = *pSrc++;
        OPJ_UINT32 val12 = *pSrc++;

        pDst[i + 0] = (OPJ_INT32)((val0 << 5) | (val1 >> 3));
        pDst[i + 1] = (OPJ_INT32)(((val1 & 0x7U) << 10) | (val2 << 2) | (val3 >> 6));
        pDst[i + 2] = (OPJ_INT32)(((val3 & 0x3FU) << 7) | (val4 >> 1));
        pDst[i + 3] = (OPJ_INT32)(((val4 & 0x1U) << 12) | (val5 << 4) | (val6 >> 4));
        pDst[i + 4] = (OPJ_INT32)(((val6 & 0xFU) << 9) | (val7 << 1) | (val8 >> 7));
        pDst[i + 5] = (OPJ_INT32)(((val8 & 0x7FU) << 6) | (val9 >> 2));
        pDst[i + 6] = (OPJ_INT32)(((val9 & 0x3U) << 11) | (val10 << 3) | (val11 >> 5));
        pDst[i + 7] = (OPJ_INT32)(((val11 & 0x1FU) << 8) | (val12));

    }
    if (length & 7U) {
        unsigned int val;
        int available = 0;

        length = length & 7U;

        GETBITS(pDst[i + 0], 13)

        if (length > 1U) {
            GETBITS(pDst[i + 1], 13)
            if (length > 2U) {
                GETBITS(pDst[i + 2], 13)
                if (length > 3U) {
                    GETBITS(pDst[i + 3], 13)
                    if (length > 4U) {
                        GETBITS(pDst[i + 4], 13)
                        if (length > 5U) {
                            GETBITS(pDst[i + 5], 13)
                            if (length > 6U) {
                                GETBITS(pDst[i + 6], 13)
                            }
                        }
                    }
                }
            }
        }
    }
}
static void tif_14uto32s(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                         OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)3U); i += 4U) {
        OPJ_UINT32 val0 = *pSrc++;
        OPJ_UINT32 val1 = *pSrc++;
        OPJ_UINT32 val2 = *pSrc++;
        OPJ_UINT32 val3 = *pSrc++;
        OPJ_UINT32 val4 = *pSrc++;
        OPJ_UINT32 val5 = *pSrc++;
        OPJ_UINT32 val6 = *pSrc++;

        pDst[i + 0] = (OPJ_INT32)((val0 << 6) | (val1 >> 2));
        pDst[i + 1] = (OPJ_INT32)(((val1 & 0x3U) << 12) | (val2 << 4) | (val3 >> 4));
        pDst[i + 2] = (OPJ_INT32)(((val3 & 0xFU) << 10) | (val4 << 2) | (val5 >> 6));
        pDst[i + 3] = (OPJ_INT32)(((val5 & 0x3FU) << 8) | val6);

    }
    if (length & 3U) {
        OPJ_UINT32 val0 = *pSrc++;
        OPJ_UINT32 val1 = *pSrc++;
        length = length & 3U;
        pDst[i + 0] = (OPJ_INT32)((val0 << 6) | (val1 >> 2));

        if (length > 1U) {
            OPJ_UINT32 val2 = *pSrc++;
            OPJ_UINT32 val3 = *pSrc++;
            pDst[i + 1] = (OPJ_INT32)(((val1 & 0x3U) << 12) | (val2 << 4) | (val3 >> 4));
            if (length > 2U) {
                OPJ_UINT32 val4 = *pSrc++;
                OPJ_UINT32 val5 = *pSrc++;
                pDst[i + 2] = (OPJ_INT32)(((val3 & 0xFU) << 10) | (val4 << 2) | (val5 >> 6));
            }
        }
    }
}
static void tif_15uto32s(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                         OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)7U); i += 8U) {
        OPJ_UINT32 val0 = *pSrc++;
        OPJ_UINT32 val1 = *pSrc++;
        OPJ_UINT32 val2 = *pSrc++;
        OPJ_UINT32 val3 = *pSrc++;
        OPJ_UINT32 val4 = *pSrc++;
        OPJ_UINT32 val5 = *pSrc++;
        OPJ_UINT32 val6 = *pSrc++;
        OPJ_UINT32 val7 = *pSrc++;
        OPJ_UINT32 val8 = *pSrc++;
        OPJ_UINT32 val9 = *pSrc++;
        OPJ_UINT32 val10 = *pSrc++;
        OPJ_UINT32 val11 = *pSrc++;
        OPJ_UINT32 val12 = *pSrc++;
        OPJ_UINT32 val13 = *pSrc++;
        OPJ_UINT32 val14 = *pSrc++;

        pDst[i + 0] = (OPJ_INT32)((val0 << 7) | (val1 >> 1));
        pDst[i + 1] = (OPJ_INT32)(((val1 & 0x1U) << 14) | (val2 << 6) | (val3 >> 2));
        pDst[i + 2] = (OPJ_INT32)(((val3 & 0x3U) << 13) | (val4 << 5) | (val5 >> 3));
        pDst[i + 3] = (OPJ_INT32)(((val5 & 0x7U) << 12) | (val6 << 4) | (val7 >> 4));
        pDst[i + 4] = (OPJ_INT32)(((val7 & 0xFU) << 11) | (val8 << 3) | (val9 >> 5));
        pDst[i + 5] = (OPJ_INT32)(((val9 & 0x1FU) << 10) | (val10 << 2) | (val11 >> 6));
        pDst[i + 6] = (OPJ_INT32)(((val11 & 0x3FU) << 9) | (val12 << 1) | (val13 >> 7));
        pDst[i + 7] = (OPJ_INT32)(((val13 & 0x7FU) << 8) | (val14));

    }
    if (length & 7U) {
        unsigned int val;
        int available = 0;

        length = length & 7U;

        GETBITS(pDst[i + 0], 15)

        if (length > 1U) {
            GETBITS(pDst[i + 1], 15)
            if (length > 2U) {
                GETBITS(pDst[i + 2], 15)
                if (length > 3U) {
                    GETBITS(pDst[i + 3], 15)
                    if (length > 4U) {
                        GETBITS(pDst[i + 4], 15)
                        if (length > 5U) {
                            GETBITS(pDst[i + 5], 15)
                            if (length > 6U) {
                                GETBITS(pDst[i + 6], 15)
                            }
                        }
                    }
                }
            }
        }
    }
}

/* seems that libtiff decodes this to machine endianness */
static void tif_16uto32s(const OPJ_UINT16* pSrc, OPJ_INT32* pDst,
                         OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < length; i++) {
        pDst[i] = pSrc[i];
    }
}

/*
 * libtiff/tif_getimage.c : 1,2,4,8,16 bitspersample accepted
 * CINEMA                 : 12 bit precision
 */
opj_image_t* tiftoimage(const char *filename, opj_cparameters_t *parameters)
{
    int subsampling_dx = parameters->subsampling_dx;
    int subsampling_dy = parameters->subsampling_dy;
    TIFF *tif;
    tdata_t buf;
    tstrip_t strip;
    int64_t strip_size, rowStride, TIFF_MAX;
    int j, currentPlane, numcomps = 0, w, h;
    OPJ_COLOR_SPACE color_space = OPJ_CLRSPC_UNKNOWN;
    opj_image_cmptparm_t cmptparm[4]; /* RGBA */
    opj_image_t *image = NULL;
    uint16 tiBps, tiPhoto, tiSf, tiSpp, tiPC;
    uint32 tiWidth, tiHeight;
    OPJ_BOOL is_cinema = OPJ_IS_CINEMA(parameters->rsiz);
    convert_XXx32s_C1R cvtTifTo32s = NULL;
    convert_32s_CXPX cvtCxToPx = NULL;
    OPJ_INT32* buffer32s = NULL;
    OPJ_INT32* planes[4];

    tif = TIFFOpen(filename, "r");

    if (!tif) {
        fprintf(stderr, "tiftoimage:Failed to open %s for reading\n", filename);
        return 0;
    }
    tiBps = tiPhoto = tiSf = tiSpp = tiPC = 0;
    tiWidth = tiHeight = 0;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &tiWidth);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &tiHeight);
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &tiBps);
    TIFFGetField(tif, TIFFTAG_SAMPLEFORMAT, &tiSf);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &tiSpp);
    TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &tiPhoto);
    TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &tiPC);
    w = (int)tiWidth;
    h = (int)tiHeight;

    if (tiSpp == 0 || tiSpp > 4) { /* should be 1 ... 4 */
        fprintf(stderr, "tiftoimage: Bad value for samples per pixel == %d.\n"
                "\tAborting.\n", tiSpp);
        TIFFClose(tif);
        return NULL;
    }
    if (tiBps > 16U || tiBps == 0) {
        fprintf(stderr, "tiftoimage: Bad values for Bits == %d.\n"
                "\tMax. 16 Bits are allowed here.\n\tAborting.\n", tiBps);
        TIFFClose(tif);
        return NULL;
    }
    if (tiPhoto != PHOTOMETRIC_MINISBLACK && tiPhoto != PHOTOMETRIC_RGB) {
        fprintf(stderr,
                "tiftoimage: Bad color format %d.\n\tOnly RGB(A) and GRAY(A) has been implemented\n\tAborting.\n",
                (int) tiPhoto);
        TIFFClose(tif);
        return NULL;
    }
    if (tiWidth == 0 || tiHeight == 0) {
        fprintf(stderr, "tiftoimage: Bad values for width(%u) "
                "and/or height(%u)\n\tAborting.\n", tiWidth, tiHeight);
        TIFFClose(tif);
        return NULL;
    }
    w = (int)tiWidth;
    h = (int)tiHeight;

    switch (tiBps) {
    case 1:
    case 2:
    case 4:
    case 6:
    case 8:
        cvtTifTo32s = convert_XXu32s_C1R_LUT[tiBps];
        break;
    /* others are specific to TIFF */
    case 3:
        cvtTifTo32s = tif_3uto32s;
        break;
    case 5:
        cvtTifTo32s = tif_5uto32s;
        break;
    case 7:
        cvtTifTo32s = tif_7uto32s;
        break;
    case 9:
        cvtTifTo32s = tif_9uto32s;
        break;
    case 10:
        cvtTifTo32s = tif_10uto32s;
        break;
    case 11:
        cvtTifTo32s = tif_11uto32s;
        break;
    case 12:
        cvtTifTo32s = tif_12uto32s;
        break;
    case 13:
        cvtTifTo32s = tif_13uto32s;
        break;
    case 14:
        cvtTifTo32s = tif_14uto32s;
        break;
    case 15:
        cvtTifTo32s = tif_15uto32s;
        break;
    case 16:
        cvtTifTo32s = (convert_XXx32s_C1R)tif_16uto32s;
        break;
    default:
        /* never here */
        break;
    }

    /* initialize image components */
    memset(&cmptparm[0], 0, 4 * sizeof(opj_image_cmptparm_t));

    if ((tiPhoto == PHOTOMETRIC_RGB) && (is_cinema) && (tiBps != 12U)) {
        fprintf(stdout, "WARNING:\n"
                "Input image bitdepth is %d bits\n"
                "TIF conversion has automatically rescaled to 12-bits\n"
                "to comply with cinema profiles.\n",
                tiBps);
    } else {
        is_cinema = 0U;
    }

    numcomps = tiSpp;
    if (tiPhoto == PHOTOMETRIC_RGB) { /* RGB(A) */
        color_space = OPJ_CLRSPC_SRGB;
    } else if (tiPhoto == PHOTOMETRIC_MINISBLACK) { /* GRAY(A) */
        color_space = OPJ_CLRSPC_GRAY;
    }

    cvtCxToPx = convert_32s_CXPX_LUT[numcomps];
    if (tiPC == PLANARCONFIG_SEPARATE) {
        cvtCxToPx = convert_32s_CXPX_LUT[1]; /* override */
        tiSpp = 1U; /* consider only one sample per plane */
    }

    for (j = 0; j < numcomps; j++) {
        cmptparm[j].prec = tiBps;
        cmptparm[j].bpp = tiBps;
        cmptparm[j].dx = (OPJ_UINT32)subsampling_dx;
        cmptparm[j].dy = (OPJ_UINT32)subsampling_dy;
        cmptparm[j].w = (OPJ_UINT32)w;
        cmptparm[j].h = (OPJ_UINT32)h;
    }

    image = opj_image_create((OPJ_UINT32)numcomps, &cmptparm[0], color_space);
    if (!image) {
        TIFFClose(tif);
        return NULL;
    }
    /* set image offset and reference grid */
    image->x0 = (OPJ_UINT32)parameters->image_offset_x0;
    image->y0 = (OPJ_UINT32)parameters->image_offset_y0;
    image->x1 = !image->x0 ? (OPJ_UINT32)(w - 1) * (OPJ_UINT32)subsampling_dx + 1 :
                image->x0 + (OPJ_UINT32)(w - 1) * (OPJ_UINT32)subsampling_dx + 1;
    if (image->x1 <= image->x0) {
        fprintf(stderr, "tiftoimage: Bad value for image->x1(%d) vs. "
                "image->x0(%d)\n\tAborting.\n", image->x1, image->x0);
        TIFFClose(tif);
        opj_image_destroy(image);
        return NULL;
    }
    image->y1 = !image->y0 ? (OPJ_UINT32)(h - 1) * (OPJ_UINT32)subsampling_dy + 1 :
                image->y0 + (OPJ_UINT32)(h - 1) * (OPJ_UINT32)subsampling_dy + 1;
    if (image->y1 <= image->y0) {
        fprintf(stderr, "tiftoimage: Bad value for image->y1(%d) vs. "
                "image->y0(%d)\n\tAborting.\n", image->y1, image->y0);
        TIFFClose(tif);
        opj_image_destroy(image);
        return NULL;
    }

    for (j = 0; j < numcomps; j++) {
        planes[j] = image->comps[j].data;
    }
    image->comps[numcomps - 1].alpha = (OPJ_UINT16)(1 - (numcomps & 1));

    strip_size = (int64_t)TIFFStripSize(tif);

    buf = malloc((OPJ_SIZE_T)strip_size);
    if (buf == NULL) {
        TIFFClose(tif);
        opj_image_destroy(image);
        return NULL;
    }
    if (sizeof(tsize_t) == 4) {
        TIFF_MAX = INT_MAX;
    } else {
        TIFF_MAX = UINT_MAX;
    }
    if ((int64_t)tiWidth > (int64_t)(TIFF_MAX / tiSpp) ||
            (int64_t)(tiWidth * tiSpp) > (int64_t)(TIFF_MAX / tiBps) ||
            (int64_t)(tiWidth * tiSpp) > (int64_t)(TIFF_MAX / (int64_t)sizeof(OPJ_INT32))) {
        fprintf(stderr, "Buffer overflow\n");
        _TIFFfree(buf);
        TIFFClose(tif);
        opj_image_destroy(image);
        return NULL;
    }

    rowStride = (int64_t)((tiWidth * tiSpp * tiBps + 7U) / 8U);
    buffer32s = (OPJ_INT32 *)malloc((OPJ_SIZE_T)(tiWidth * tiSpp * sizeof(
                                        OPJ_INT32)));
    if (buffer32s == NULL) {
        _TIFFfree(buf);
        TIFFClose(tif);
        opj_image_destroy(image);
        return NULL;
    }

    strip = 0;
    currentPlane = 0;
    do {
        planes[0] = image->comps[currentPlane].data; /* to manage planar data */
        h = (int)tiHeight;
        /* Read the Image components */
        for (; (h > 0) && (strip < TIFFNumberOfStrips(tif)); strip++) {
            const OPJ_UINT8 *dat8;
            int64_t ssize;

            ssize = (int64_t)TIFFReadEncodedStrip(tif, strip, buf, (tsize_t)strip_size);

            if (ssize < 1 || ssize > strip_size) {
                fprintf(stderr, "tiftoimage: Bad value for ssize(%" PRId64 ") "
                        "vs. strip_size(%" PRId64 ").\n\tAborting.\n", ssize, strip_size);
                _TIFFfree(buf);
                _TIFFfree(buffer32s);
                TIFFClose(tif);
                opj_image_destroy(image);
                return NULL;
            }
            dat8 = (const OPJ_UINT8*)buf;

            while (ssize >= rowStride) {
                cvtTifTo32s(dat8, buffer32s, (OPJ_SIZE_T)w * tiSpp);
                cvtCxToPx(buffer32s, planes, (OPJ_SIZE_T)w);
                planes[0] += w;
                planes[1] += w;
                planes[2] += w;
                planes[3] += w;
                dat8  += rowStride;
                ssize -= rowStride;
                h--;
            }
        }
        currentPlane++;
    } while ((tiPC == PLANARCONFIG_SEPARATE) && (currentPlane < numcomps));

    free(buffer32s);
    _TIFFfree(buf);
    TIFFClose(tif);

    if (is_cinema) {
        for (j = 0; j < numcomps; ++j) {
            scale_component(&(image->comps[j]), 12);
        }

    }
    return image;

}/* tiftoimage() */


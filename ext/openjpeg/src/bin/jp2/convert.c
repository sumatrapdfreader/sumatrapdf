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

#include "openjpeg.h"
#include "convert.h"

/*
 * Get logarithm of an integer and round downwards.
 *
 * log2(a)
 */
static int int_floorlog2(int a)
{
    int l;
    for (l = 0; a > 1; l++) {
        a >>= 1;
    }
    return l;
}

/* Component precision scaling */
void clip_component(opj_image_comp_t* component, OPJ_UINT32 precision)
{
    OPJ_SIZE_T i;
    OPJ_SIZE_T len;
    OPJ_UINT32 umax = (OPJ_UINT32)((OPJ_INT32) - 1);

    len = (OPJ_SIZE_T)component->w * (OPJ_SIZE_T)component->h;
    if (precision < 32) {
        umax = (1U << precision) - 1U;
    }

    if (component->sgnd) {
        OPJ_INT32* l_data = component->data;
        OPJ_INT32 max = (OPJ_INT32)(umax / 2U);
        OPJ_INT32 min = -max - 1;
        for (i = 0; i < len; ++i) {
            if (l_data[i] > max) {
                l_data[i] = max;
            } else if (l_data[i] < min) {
                l_data[i] = min;
            }
        }
    } else {
        OPJ_UINT32* l_data = (OPJ_UINT32*)component->data;
        for (i = 0; i < len; ++i) {
            if (l_data[i] > umax) {
                l_data[i] = umax;
            }
        }
    }
    component->prec = precision;
}

/* Component precision scaling */
static void scale_component_up(opj_image_comp_t* component,
                               OPJ_UINT32 precision)
{
    OPJ_SIZE_T i, len;

    len = (OPJ_SIZE_T)component->w * (OPJ_SIZE_T)component->h;
    if (component->sgnd) {
        OPJ_INT64  newMax = (OPJ_INT64)(1U << (precision - 1));
        OPJ_INT64  oldMax = (OPJ_INT64)(1U << (component->prec - 1));
        OPJ_INT32* l_data = component->data;
        for (i = 0; i < len; ++i) {
            l_data[i] = (OPJ_INT32)(((OPJ_INT64)l_data[i] * newMax) / oldMax);
        }
    } else {
        OPJ_UINT64  newMax = (OPJ_UINT64)((1U << precision) - 1U);
        OPJ_UINT64  oldMax = (OPJ_UINT64)((1U << component->prec) - 1U);
        OPJ_UINT32* l_data = (OPJ_UINT32*)component->data;
        for (i = 0; i < len; ++i) {
            l_data[i] = (OPJ_UINT32)(((OPJ_UINT64)l_data[i] * newMax) / oldMax);
        }
    }
    component->prec = precision;
    component->bpp = precision;
}
void scale_component(opj_image_comp_t* component, OPJ_UINT32 precision)
{
    int shift;
    OPJ_SIZE_T i, len;

    if (component->prec == precision) {
        return;
    }
    if (component->prec < precision) {
        scale_component_up(component, precision);
        return;
    }
    shift = (int)(component->prec - precision);
    len = (OPJ_SIZE_T)component->w * (OPJ_SIZE_T)component->h;
    if (component->sgnd) {
        OPJ_INT32* l_data = component->data;
        for (i = 0; i < len; ++i) {
            l_data[i] >>= shift;
        }
    } else {
        OPJ_UINT32* l_data = (OPJ_UINT32*)component->data;
        for (i = 0; i < len; ++i) {
            l_data[i] >>= shift;
        }
    }
    component->bpp = precision;
    component->prec = precision;
}


/* planar / interleaved conversions */
/* used by PNG/TIFF */
static void convert_32s_C1P1(const OPJ_INT32* pSrc, OPJ_INT32* const* pDst,
                             OPJ_SIZE_T length)
{
    memcpy(pDst[0], pSrc, length * sizeof(OPJ_INT32));
}
static void convert_32s_C2P2(const OPJ_INT32* pSrc, OPJ_INT32* const* pDst,
                             OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    OPJ_INT32* pDst0 = pDst[0];
    OPJ_INT32* pDst1 = pDst[1];

    for (i = 0; i < length; i++) {
        pDst0[i] = pSrc[2 * i + 0];
        pDst1[i] = pSrc[2 * i + 1];
    }
}
static void convert_32s_C3P3(const OPJ_INT32* pSrc, OPJ_INT32* const* pDst,
                             OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    OPJ_INT32* pDst0 = pDst[0];
    OPJ_INT32* pDst1 = pDst[1];
    OPJ_INT32* pDst2 = pDst[2];

    for (i = 0; i < length; i++) {
        pDst0[i] = pSrc[3 * i + 0];
        pDst1[i] = pSrc[3 * i + 1];
        pDst2[i] = pSrc[3 * i + 2];
    }
}
static void convert_32s_C4P4(const OPJ_INT32* pSrc, OPJ_INT32* const* pDst,
                             OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    OPJ_INT32* pDst0 = pDst[0];
    OPJ_INT32* pDst1 = pDst[1];
    OPJ_INT32* pDst2 = pDst[2];
    OPJ_INT32* pDst3 = pDst[3];

    for (i = 0; i < length; i++) {
        pDst0[i] = pSrc[4 * i + 0];
        pDst1[i] = pSrc[4 * i + 1];
        pDst2[i] = pSrc[4 * i + 2];
        pDst3[i] = pSrc[4 * i + 3];
    }
}
const convert_32s_CXPX convert_32s_CXPX_LUT[5] = {
    NULL,
    convert_32s_C1P1,
    convert_32s_C2P2,
    convert_32s_C3P3,
    convert_32s_C4P4
};

static void convert_32s_P1C1(OPJ_INT32 const* const* pSrc, OPJ_INT32* pDst,
                             OPJ_SIZE_T length, OPJ_INT32 adjust)
{
    OPJ_SIZE_T i;
    const OPJ_INT32* pSrc0 = pSrc[0];

    for (i = 0; i < length; i++) {
        pDst[i] = pSrc0[i] + adjust;
    }
}
static void convert_32s_P2C2(OPJ_INT32 const* const* pSrc, OPJ_INT32* pDst,
                             OPJ_SIZE_T length, OPJ_INT32 adjust)
{
    OPJ_SIZE_T i;
    const OPJ_INT32* pSrc0 = pSrc[0];
    const OPJ_INT32* pSrc1 = pSrc[1];

    for (i = 0; i < length; i++) {
        pDst[2 * i + 0] = pSrc0[i] + adjust;
        pDst[2 * i + 1] = pSrc1[i] + adjust;
    }
}
static void convert_32s_P3C3(OPJ_INT32 const* const* pSrc, OPJ_INT32* pDst,
                             OPJ_SIZE_T length, OPJ_INT32 adjust)
{
    OPJ_SIZE_T i;
    const OPJ_INT32* pSrc0 = pSrc[0];
    const OPJ_INT32* pSrc1 = pSrc[1];
    const OPJ_INT32* pSrc2 = pSrc[2];

    for (i = 0; i < length; i++) {
        pDst[3 * i + 0] = pSrc0[i] + adjust;
        pDst[3 * i + 1] = pSrc1[i] + adjust;
        pDst[3 * i + 2] = pSrc2[i] + adjust;
    }
}
static void convert_32s_P4C4(OPJ_INT32 const* const* pSrc, OPJ_INT32* pDst,
                             OPJ_SIZE_T length, OPJ_INT32 adjust)
{
    OPJ_SIZE_T i;
    const OPJ_INT32* pSrc0 = pSrc[0];
    const OPJ_INT32* pSrc1 = pSrc[1];
    const OPJ_INT32* pSrc2 = pSrc[2];
    const OPJ_INT32* pSrc3 = pSrc[3];

    for (i = 0; i < length; i++) {
        pDst[4 * i + 0] = pSrc0[i] + adjust;
        pDst[4 * i + 1] = pSrc1[i] + adjust;
        pDst[4 * i + 2] = pSrc2[i] + adjust;
        pDst[4 * i + 3] = pSrc3[i] + adjust;
    }
}
const convert_32s_PXCX convert_32s_PXCX_LUT[5] = {
    NULL,
    convert_32s_P1C1,
    convert_32s_P2C2,
    convert_32s_P3C3,
    convert_32s_P4C4
};

/* bit depth conversions */
/* used by PNG/TIFF up to 8bpp */
static void convert_1u32s_C1R(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                              OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)7U); i += 8U) {
        OPJ_UINT32 val = *pSrc++;
        pDst[i + 0] = (OPJ_INT32)(val >> 7);
        pDst[i + 1] = (OPJ_INT32)((val >> 6) & 0x1U);
        pDst[i + 2] = (OPJ_INT32)((val >> 5) & 0x1U);
        pDst[i + 3] = (OPJ_INT32)((val >> 4) & 0x1U);
        pDst[i + 4] = (OPJ_INT32)((val >> 3) & 0x1U);
        pDst[i + 5] = (OPJ_INT32)((val >> 2) & 0x1U);
        pDst[i + 6] = (OPJ_INT32)((val >> 1) & 0x1U);
        pDst[i + 7] = (OPJ_INT32)(val & 0x1U);
    }
    if (length & 7U) {
        OPJ_UINT32 val = *pSrc++;
        length = length & 7U;
        pDst[i + 0] = (OPJ_INT32)(val >> 7);

        if (length > 1U) {
            pDst[i + 1] = (OPJ_INT32)((val >> 6) & 0x1U);
            if (length > 2U) {
                pDst[i + 2] = (OPJ_INT32)((val >> 5) & 0x1U);
                if (length > 3U) {
                    pDst[i + 3] = (OPJ_INT32)((val >> 4) & 0x1U);
                    if (length > 4U) {
                        pDst[i + 4] = (OPJ_INT32)((val >> 3) & 0x1U);
                        if (length > 5U) {
                            pDst[i + 5] = (OPJ_INT32)((val >> 2) & 0x1U);
                            if (length > 6U) {
                                pDst[i + 6] = (OPJ_INT32)((val >> 1) & 0x1U);
                            }
                        }
                    }
                }
            }
        }
    }
}
static void convert_2u32s_C1R(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                              OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)3U); i += 4U) {
        OPJ_UINT32 val = *pSrc++;
        pDst[i + 0] = (OPJ_INT32)(val >> 6);
        pDst[i + 1] = (OPJ_INT32)((val >> 4) & 0x3U);
        pDst[i + 2] = (OPJ_INT32)((val >> 2) & 0x3U);
        pDst[i + 3] = (OPJ_INT32)(val & 0x3U);
    }
    if (length & 3U) {
        OPJ_UINT32 val = *pSrc++;
        length = length & 3U;
        pDst[i + 0] = (OPJ_INT32)(val >> 6);

        if (length > 1U) {
            pDst[i + 1] = (OPJ_INT32)((val >> 4) & 0x3U);
            if (length > 2U) {
                pDst[i + 2] = (OPJ_INT32)((val >> 2) & 0x3U);

            }
        }
    }
}
static void convert_4u32s_C1R(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                              OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)1U); i += 2U) {
        OPJ_UINT32 val = *pSrc++;
        pDst[i + 0] = (OPJ_INT32)(val >> 4);
        pDst[i + 1] = (OPJ_INT32)(val & 0xFU);
    }
    if (length & 1U) {
        OPJ_UINT8 val = *pSrc++;
        pDst[i + 0] = (OPJ_INT32)(val >> 4);
    }
}
static void convert_6u32s_C1R(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                              OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)3U); i += 4U) {
        OPJ_UINT32 val0 = *pSrc++;
        OPJ_UINT32 val1 = *pSrc++;
        OPJ_UINT32 val2 = *pSrc++;
        pDst[i + 0] = (OPJ_INT32)(val0 >> 2);
        pDst[i + 1] = (OPJ_INT32)(((val0 & 0x3U) << 4) | (val1 >> 4));
        pDst[i + 2] = (OPJ_INT32)(((val1 & 0xFU) << 2) | (val2 >> 6));
        pDst[i + 3] = (OPJ_INT32)(val2 & 0x3FU);

    }
    if (length & 3U) {
        OPJ_UINT32 val0 = *pSrc++;
        length = length & 3U;
        pDst[i + 0] = (OPJ_INT32)(val0 >> 2);

        if (length > 1U) {
            OPJ_UINT32 val1 = *pSrc++;
            pDst[i + 1] = (OPJ_INT32)(((val0 & 0x3U) << 4) | (val1 >> 4));
            if (length > 2U) {
                OPJ_UINT32 val2 = *pSrc++;
                pDst[i + 2] = (OPJ_INT32)(((val1 & 0xFU) << 2) | (val2 >> 6));
            }
        }
    }
}
static void convert_8u32s_C1R(const OPJ_BYTE* pSrc, OPJ_INT32* pDst,
                              OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < length; i++) {
        pDst[i] = pSrc[i];
    }
}
const convert_XXx32s_C1R convert_XXu32s_C1R_LUT[9] = {
    NULL,
    convert_1u32s_C1R,
    convert_2u32s_C1R,
    NULL,
    convert_4u32s_C1R,
    NULL,
    convert_6u32s_C1R,
    NULL,
    convert_8u32s_C1R
};


static void convert_32s1u_C1R(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
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

        *pDst++ = (OPJ_BYTE)((src0 << 7) | (src1 << 6) | (src2 << 5) | (src3 << 4) |
                             (src4 << 3) | (src5 << 2) | (src6 << 1) | src7);
    }

    if (length & 7U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = 0U;
        OPJ_UINT32 src2 = 0U;
        OPJ_UINT32 src3 = 0U;
        OPJ_UINT32 src4 = 0U;
        OPJ_UINT32 src5 = 0U;
        OPJ_UINT32 src6 = 0U;
        length = length & 7U;

        if (length > 1U) {
            src1 = (OPJ_UINT32)pSrc[i + 1];
            if (length > 2U) {
                src2 = (OPJ_UINT32)pSrc[i + 2];
                if (length > 3U) {
                    src3 = (OPJ_UINT32)pSrc[i + 3];
                    if (length > 4U) {
                        src4 = (OPJ_UINT32)pSrc[i + 4];
                        if (length > 5U) {
                            src5 = (OPJ_UINT32)pSrc[i + 5];
                            if (length > 6U) {
                                src6 = (OPJ_UINT32)pSrc[i + 6];
                            }
                        }
                    }
                }
            }
        }
        *pDst++ = (OPJ_BYTE)((src0 << 7) | (src1 << 6) | (src2 << 5) | (src3 << 4) |
                             (src4 << 3) | (src5 << 2) | (src6 << 1));
    }
}

static void convert_32s2u_C1R(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
                              OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)3U); i += 4U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = (OPJ_UINT32)pSrc[i + 1];
        OPJ_UINT32 src2 = (OPJ_UINT32)pSrc[i + 2];
        OPJ_UINT32 src3 = (OPJ_UINT32)pSrc[i + 3];

        *pDst++ = (OPJ_BYTE)((src0 << 6) | (src1 << 4) | (src2 << 2) | src3);
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
        *pDst++ = (OPJ_BYTE)((src0 << 6) | (src1 << 4) | (src2 << 2));
    }
}

static void convert_32s4u_C1R(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
                              OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)1U); i += 2U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = (OPJ_UINT32)pSrc[i + 1];

        *pDst++ = (OPJ_BYTE)((src0 << 4) | src1);
    }

    if (length & 1U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        *pDst++ = (OPJ_BYTE)((src0 << 4));
    }
}

static void convert_32s6u_C1R(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
                              OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < (length & ~(OPJ_SIZE_T)3U); i += 4U) {
        OPJ_UINT32 src0 = (OPJ_UINT32)pSrc[i + 0];
        OPJ_UINT32 src1 = (OPJ_UINT32)pSrc[i + 1];
        OPJ_UINT32 src2 = (OPJ_UINT32)pSrc[i + 2];
        OPJ_UINT32 src3 = (OPJ_UINT32)pSrc[i + 3];

        *pDst++ = (OPJ_BYTE)((src0 << 2) | (src1 >> 4));
        *pDst++ = (OPJ_BYTE)(((src1 & 0xFU) << 4) | (src2 >> 2));
        *pDst++ = (OPJ_BYTE)(((src2 & 0x3U) << 6) | src3);
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
        *pDst++ = (OPJ_BYTE)((src0 << 2) | (src1 >> 4));
        if (length > 1U) {
            *pDst++ = (OPJ_BYTE)(((src1 & 0xFU) << 4) | (src2 >> 2));
            if (length > 2U) {
                *pDst++ = (OPJ_BYTE)(((src2 & 0x3U) << 6));
            }
        }
    }
}
static void convert_32s8u_C1R(const OPJ_INT32* pSrc, OPJ_BYTE* pDst,
                              OPJ_SIZE_T length)
{
    OPJ_SIZE_T i;
    for (i = 0; i < length; ++i) {
        pDst[i] = (OPJ_BYTE)pSrc[i];
    }
}
const convert_32sXXx_C1R convert_32sXXu_C1R_LUT[9] = {
    NULL,
    convert_32s1u_C1R,
    convert_32s2u_C1R,
    NULL,
    convert_32s4u_C1R,
    NULL,
    convert_32s6u_C1R,
    NULL,
    convert_32s8u_C1R
};

/* -->> -->> -->> -->>

  TGA IMAGE FORMAT

 <<-- <<-- <<-- <<-- */

#ifdef INFORMATION_ONLY
/* TGA header definition. */
struct tga_header {
    unsigned char   id_length;              /* Image id field length    */
    unsigned char   colour_map_type;        /* Colour map type          */
    unsigned char   image_type;             /* Image type               */
    /*
    ** Colour map specification
    */
    unsigned short  colour_map_index;       /* First entry index        */
    unsigned short  colour_map_length;      /* Colour map length        */
    unsigned char   colour_map_entry_size;  /* Colour map entry size    */
    /*
    ** Image specification
    */
    unsigned short  x_origin;               /* x origin of image        */
    unsigned short  y_origin;               /* u origin of image        */
    unsigned short  image_width;            /* Image width              */
    unsigned short  image_height;           /* Image height             */
    unsigned char   pixel_depth;            /* Pixel depth              */
    unsigned char   image_desc;             /* Image descriptor         */
};
#endif /* INFORMATION_ONLY */

/* Returns a ushort from a little-endian serialized value */
static unsigned short get_tga_ushort(const unsigned char *data)
{
    return (unsigned short)(data[0] | (data[1] << 8));
}

#define TGA_HEADER_SIZE 18

static int tga_readheader(FILE *fp, unsigned int *bits_per_pixel,
                          unsigned int *width, unsigned int *height, int *flip_image)
{
    int palette_size;
    unsigned char tga[TGA_HEADER_SIZE];
    unsigned char id_len, /*cmap_type,*/ image_type;
    unsigned char pixel_depth, image_desc;
    unsigned short /*cmap_index,*/ cmap_len, cmap_entry_size;
    unsigned short /*x_origin, y_origin,*/ image_w, image_h;

    if (!bits_per_pixel || !width || !height || !flip_image) {
        return 0;
    }

    if (fread(tga, TGA_HEADER_SIZE, 1, fp) != 1) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        return 0 ;
    }
    id_len = tga[0];
    /*cmap_type = tga[1];*/
    image_type = tga[2];
    /*cmap_index = get_tga_ushort(&tga[3]);*/
    cmap_len = get_tga_ushort(&tga[5]);
    cmap_entry_size = tga[7];


#if 0
    x_origin = get_tga_ushort(&tga[8]);
    y_origin = get_tga_ushort(&tga[10]);
#endif
    image_w = get_tga_ushort(&tga[12]);
    image_h = get_tga_ushort(&tga[14]);
    pixel_depth = tga[16];
    image_desc  = tga[17];

    *bits_per_pixel = (unsigned int)pixel_depth;
    *width  = (unsigned int)image_w;
    *height = (unsigned int)image_h;

    /* Ignore tga identifier, if present ... */
    if (id_len) {
        unsigned char *id = (unsigned char *) malloc(id_len);
        if (id == 0) {
            fprintf(stderr, "tga_readheader: memory out\n");
            return 0;
        }
        if (!fread(id, id_len, 1, fp)) {
            fprintf(stderr,
                    "\nError: fread return a number of element different from the expected.\n");
            free(id);
            return 0 ;
        }
        free(id);
    }

    /* Test for compressed formats ... not yet supported ...
    // Note :-  9 - RLE encoded palettized.
    //         10 - RLE encoded RGB. */
    if (image_type > 8) {
        fprintf(stderr, "Sorry, compressed tga files are not currently supported.\n");
        return 0 ;
    }

    *flip_image = !(image_desc & 32);

    /* Palettized formats are not yet supported, skip over the palette, if present ... */
    palette_size = cmap_len * (cmap_entry_size / 8);

    if (palette_size > 0) {
        fprintf(stderr, "File contains a palette - not yet supported.");
        fseek(fp, palette_size, SEEK_CUR);
    }
    return 1;
}

#ifdef OPJ_BIG_ENDIAN

static INLINE OPJ_UINT16 swap16(OPJ_UINT16 x)
{
    return (OPJ_UINT16)(((x & 0x00ffU) <<  8) | ((x & 0xff00U) >>  8));
}

#endif

static int tga_writeheader(FILE *fp, int bits_per_pixel, int width, int height,
                           OPJ_BOOL flip_image)
{
    OPJ_UINT16 image_w, image_h, us0;
    unsigned char uc0, image_type;
    unsigned char pixel_depth, image_desc;

    if (!bits_per_pixel || !width || !height) {
        return 0;
    }

    pixel_depth = 0;

    if (bits_per_pixel < 256) {
        pixel_depth = (unsigned char)bits_per_pixel;
    } else {
        fprintf(stderr, "ERROR: Wrong bits per pixel inside tga_header");
        return 0;
    }
    uc0 = 0;

    if (fwrite(&uc0, 1, 1, fp) != 1) {
        goto fails;    /* id_length */
    }
    if (fwrite(&uc0, 1, 1, fp) != 1) {
        goto fails;    /* colour_map_type */
    }

    image_type = 2; /* Uncompressed. */
    if (fwrite(&image_type, 1, 1, fp) != 1) {
        goto fails;
    }

    us0 = 0;
    if (fwrite(&us0, 2, 1, fp) != 1) {
        goto fails;    /* colour_map_index */
    }
    if (fwrite(&us0, 2, 1, fp) != 1) {
        goto fails;    /* colour_map_length */
    }
    if (fwrite(&uc0, 1, 1, fp) != 1) {
        goto fails;    /* colour_map_entry_size */
    }

    if (fwrite(&us0, 2, 1, fp) != 1) {
        goto fails;    /* x_origin */
    }
    if (fwrite(&us0, 2, 1, fp) != 1) {
        goto fails;    /* y_origin */
    }

    image_w = (unsigned short)width;
    image_h = (unsigned short) height;

#ifndef OPJ_BIG_ENDIAN
    if (fwrite(&image_w, 2, 1, fp) != 1) {
        goto fails;
    }
    if (fwrite(&image_h, 2, 1, fp) != 1) {
        goto fails;
    }
#else
    image_w = swap16(image_w);
    image_h = swap16(image_h);
    if (fwrite(&image_w, 2, 1, fp) != 1) {
        goto fails;
    }
    if (fwrite(&image_h, 2, 1, fp) != 1) {
        goto fails;
    }
#endif

    if (fwrite(&pixel_depth, 1, 1, fp) != 1) {
        goto fails;
    }

    image_desc = 8; /* 8 bits per component. */

    if (flip_image) {
        image_desc |= 32;
    }
    if (fwrite(&image_desc, 1, 1, fp) != 1) {
        goto fails;
    }

    return 1;

fails:
    fputs("\nwrite_tgaheader: write ERROR\n", stderr);
    return 0;
}

opj_image_t* tgatoimage(const char *filename, opj_cparameters_t *parameters)
{
    FILE *f;
    opj_image_t *image;
    unsigned int image_width, image_height, pixel_bit_depth;
    unsigned int x, y;
    int flip_image = 0;
    opj_image_cmptparm_t cmptparm[4];   /* maximum 4 components */
    int numcomps;
    OPJ_COLOR_SPACE color_space;
    OPJ_BOOL mono ;
    OPJ_BOOL save_alpha;
    int subsampling_dx, subsampling_dy;
    int i;

    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s for reading !!\n", filename);
        return 0;
    }

    if (!tga_readheader(f, &pixel_bit_depth, &image_width, &image_height,
                        &flip_image)) {
        fclose(f);
        return NULL;
    }

    /* We currently only support 24 & 32 bit tga's ... */
    if (!((pixel_bit_depth == 24) || (pixel_bit_depth == 32))) {
        fclose(f);
        return NULL;
    }

    /* initialize image components */
    memset(&cmptparm[0], 0, 4 * sizeof(opj_image_cmptparm_t));

    mono = (pixel_bit_depth == 8) ||
           (pixel_bit_depth == 16);  /* Mono with & without alpha. */
    save_alpha = (pixel_bit_depth == 16) ||
                 (pixel_bit_depth == 32); /* Mono with alpha, or RGB with alpha */

    if (mono) {
        color_space = OPJ_CLRSPC_GRAY;
        numcomps = save_alpha ? 2 : 1;
    } else {
        numcomps = save_alpha ? 4 : 3;
        color_space = OPJ_CLRSPC_SRGB;
    }

    /* If the declared file size is > 10 MB, check that the file is big */
    /* enough to avoid excessive memory allocations */
    if (image_height != 0 &&
            image_width > 10000000U / image_height / (OPJ_UINT32)numcomps) {
        char ch;
        OPJ_UINT64 expected_file_size =
            (OPJ_UINT64)image_width * image_height * (OPJ_UINT32)numcomps;
        long curpos = ftell(f);
        if (expected_file_size > (OPJ_UINT64)INT_MAX) {
            expected_file_size = (OPJ_UINT64)INT_MAX;
        }
        fseek(f, (long)expected_file_size - 1, SEEK_SET);
        if (fread(&ch, 1, 1, f) != 1) {
            fclose(f);
            return NULL;
        }
        fseek(f, curpos, SEEK_SET);
    }

    subsampling_dx = parameters->subsampling_dx;
    subsampling_dy = parameters->subsampling_dy;

    for (i = 0; i < numcomps; i++) {
        cmptparm[i].prec = 8;
        cmptparm[i].bpp = 8;
        cmptparm[i].sgnd = 0;
        cmptparm[i].dx = (OPJ_UINT32)subsampling_dx;
        cmptparm[i].dy = (OPJ_UINT32)subsampling_dy;
        cmptparm[i].w = image_width;
        cmptparm[i].h = image_height;
    }

    /* create the image */
    image = opj_image_create((OPJ_UINT32)numcomps, &cmptparm[0], color_space);

    if (!image) {
        fclose(f);
        return NULL;
    }


    /* set image offset and reference grid */
    image->x0 = (OPJ_UINT32)parameters->image_offset_x0;
    image->y0 = (OPJ_UINT32)parameters->image_offset_y0;
    image->x1 = !image->x0 ? (OPJ_UINT32)(image_width - 1)  *
                (OPJ_UINT32)subsampling_dx + 1 : image->x0 + (OPJ_UINT32)(image_width - 1)  *
                (OPJ_UINT32)subsampling_dx + 1;
    image->y1 = !image->y0 ? (OPJ_UINT32)(image_height - 1) *
                (OPJ_UINT32)subsampling_dy + 1 : image->y0 + (OPJ_UINT32)(image_height - 1) *
                (OPJ_UINT32)subsampling_dy + 1;

    /* set image data */
    for (y = 0; y < image_height; y++) {
        int index;

        if (flip_image) {
            index = (int)((image_height - y - 1) * image_width);
        } else {
            index = (int)(y * image_width);
        }

        if (numcomps == 3) {
            for (x = 0; x < image_width; x++) {
                unsigned char r, g, b;

                if (!fread(&b, 1, 1, f)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                    opj_image_destroy(image);
                    fclose(f);
                    return NULL;
                }
                if (!fread(&g, 1, 1, f)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                    opj_image_destroy(image);
                    fclose(f);
                    return NULL;
                }
                if (!fread(&r, 1, 1, f)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                    opj_image_destroy(image);
                    fclose(f);
                    return NULL;
                }

                image->comps[0].data[index] = r;
                image->comps[1].data[index] = g;
                image->comps[2].data[index] = b;
                index++;
            }
        } else if (numcomps == 4) {
            for (x = 0; x < image_width; x++) {
                unsigned char r, g, b, a;
                if (!fread(&b, 1, 1, f)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                    opj_image_destroy(image);
                    fclose(f);
                    return NULL;
                }
                if (!fread(&g, 1, 1, f)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                    opj_image_destroy(image);
                    fclose(f);
                    return NULL;
                }
                if (!fread(&r, 1, 1, f)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                    opj_image_destroy(image);
                    fclose(f);
                    return NULL;
                }
                if (!fread(&a, 1, 1, f)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                    opj_image_destroy(image);
                    fclose(f);
                    return NULL;
                }

                image->comps[0].data[index] = r;
                image->comps[1].data[index] = g;
                image->comps[2].data[index] = b;
                image->comps[3].data[index] = a;
                index++;
            }
        } else {
            fprintf(stderr, "Currently unsupported bit depth : %s\n", filename);
        }
    }
    fclose(f);
    return image;
}

int imagetotga(opj_image_t * image, const char *outfile)
{
    int width, height, bpp, x, y;
    OPJ_BOOL write_alpha;
    unsigned int i;
    int adjustR, adjustG = 0, adjustB = 0, fails;
    unsigned int alpha_channel;
    float r, g, b, a;
    unsigned char value;
    float scale;
    FILE *fdest;
    size_t res;
    fails = 1;

    fdest = fopen(outfile, "wb");
    if (!fdest) {
        fprintf(stderr, "ERROR -> failed to open %s for writing\n", outfile);
        return 1;
    }

    for (i = 0; i < image->numcomps - 1; i++) {
        if ((image->comps[0].dx != image->comps[i + 1].dx)
                || (image->comps[0].dy != image->comps[i + 1].dy)
                || (image->comps[0].prec != image->comps[i + 1].prec)
                || (image->comps[0].sgnd != image->comps[i + 1].sgnd)) {
            fclose(fdest);
            fprintf(stderr,
                    "Unable to create a tga file with such J2K image charateristics.\n");
            return 1;
        }
    }

    width  = (int)image->comps[0].w;
    height = (int)image->comps[0].h;

    /* Mono with alpha, or RGB with alpha. */
    write_alpha = (image->numcomps == 2) || (image->numcomps == 4);

    /* Write TGA header  */
    bpp = write_alpha ? 32 : 24;

    if (!tga_writeheader(fdest, bpp, width, height, OPJ_TRUE)) {
        goto fin;
    }

    alpha_channel = image->numcomps - 1;

    scale = 255.0f / (float)((1 << image->comps[0].prec) - 1);

    adjustR = (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);
    if (image->numcomps >= 3) {
        adjustG = (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);
        adjustB = (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);
    }

    for (y = 0; y < height; y++) {
        unsigned int index = (unsigned int)(y * width);

        for (x = 0; x < width; x++, index++) {
            r = (float)(image->comps[0].data[index] + adjustR);

            if (image->numcomps > 2) {
                g = (float)(image->comps[1].data[index] + adjustG);
                b = (float)(image->comps[2].data[index] + adjustB);
            } else {
                /* Greyscale ... */
                g = r;
                b = r;
            }

            /* TGA format writes BGR ... */
            if (b > 255.) {
                b = 255.;
            } else if (b < 0.) {
                b = 0.;
            }
            value = (unsigned char)(b * scale);
            res = fwrite(&value, 1, 1, fdest);

            if (res < 1) {
                fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                goto fin;
            }
            if (g > 255.) {
                g = 255.;
            } else if (g < 0.) {
                g = 0.;
            }
            value = (unsigned char)(g * scale);
            res = fwrite(&value, 1, 1, fdest);

            if (res < 1) {
                fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                goto fin;
            }
            if (r > 255.) {
                r = 255.;
            } else if (r < 0.) {
                r = 0.;
            }
            value = (unsigned char)(r * scale);
            res = fwrite(&value, 1, 1, fdest);

            if (res < 1) {
                fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                goto fin;
            }

            if (write_alpha) {
                a = (float)(image->comps[alpha_channel].data[index]);
                if (a > 255.) {
                    a = 255.;
                } else if (a < 0.) {
                    a = 0.;
                }
                value = (unsigned char)(a * scale);
                res = fwrite(&value, 1, 1, fdest);

                if (res < 1) {
                    fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                    goto fin;
                }
            }
        }
    }
    fails = 0;
fin:
    fclose(fdest);

    return fails;
}

/* -->> -->> -->> -->>

PGX IMAGE FORMAT

<<-- <<-- <<-- <<-- */


static unsigned char readuchar(FILE * f)
{
    unsigned char c1;
    if (!fread(&c1, 1, 1, f)) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        return 0;
    }
    return c1;
}

static unsigned short readushort(FILE * f, int bigendian)
{
    unsigned char c1, c2;
    if (!fread(&c1, 1, 1, f)) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        return 0;
    }
    if (!fread(&c2, 1, 1, f)) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        return 0;
    }
    if (bigendian) {
        return (unsigned short)((c1 << 8) + c2);
    } else {
        return (unsigned short)((c2 << 8) + c1);
    }
}

static unsigned int readuint(FILE * f, int bigendian)
{
    unsigned char c1, c2, c3, c4;
    if (!fread(&c1, 1, 1, f)) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        return 0;
    }
    if (!fread(&c2, 1, 1, f)) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        return 0;
    }
    if (!fread(&c3, 1, 1, f)) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        return 0;
    }
    if (!fread(&c4, 1, 1, f)) {
        fprintf(stderr,
                "\nError: fread return a number of element different from the expected.\n");
        return 0;
    }
    if (bigendian) {
        return (unsigned int)(c1 << 24) + (unsigned int)(c2 << 16) + (unsigned int)(
                   c3 << 8) + c4;
    } else {
        return (unsigned int)(c4 << 24) + (unsigned int)(c3 << 16) + (unsigned int)(
                   c2 << 8) + c1;
    }
}

opj_image_t* pgxtoimage(const char *filename, opj_cparameters_t *parameters)
{
    FILE *f = NULL;
    int w, h, prec;
    int i, numcomps, max;
    OPJ_COLOR_SPACE color_space;
    opj_image_cmptparm_t cmptparm;  /* maximum of 1 component  */
    opj_image_t * image = NULL;
    int adjustS, ushift, dshift, force8;
    OPJ_UINT64 expected_file_size;

    char endian1, endian2, sign;
    char signtmp[32];

    char temp[32];
    int bigendian;
    opj_image_comp_t *comp = NULL;

    numcomps = 1;
    color_space = OPJ_CLRSPC_GRAY;

    memset(&cmptparm, 0, sizeof(opj_image_cmptparm_t));

    max = 0;

    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s for reading !\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_SET);
    if (fscanf(f, "PG%31[ \t]%c%c%31[ \t+-]%d%31[ \t]%d%31[ \t]%d", temp, &endian1,
               &endian2, signtmp, &prec, temp, &w, temp, &h) != 9) {
        fclose(f);
        fprintf(stderr,
                "ERROR: Failed to read the right number of element from the fscanf() function!\n");
        return NULL;
    }

    i = 0;
    sign = '+';
    while (signtmp[i] != '\0') {
        if (signtmp[i] == '-') {
            sign = '-';
        }
        i++;
    }

    fgetc(f);
    if (endian1 == 'M' && endian2 == 'L') {
        bigendian = 1;
    } else if (endian2 == 'M' && endian1 == 'L') {
        bigendian = 0;
    } else {
        fclose(f);
        fprintf(stderr, "Bad pgx header, please check input file\n");
        return NULL;
    }

    if (w < 1 || h < 1 || prec < 1 || prec > 31) {
        fclose(f);
        fprintf(stderr, "Bad pgx header, please check input file\n");
        return NULL;
    }

    expected_file_size =
        (OPJ_UINT64)w * (OPJ_UINT64)h * (prec > 16 ? 4 : prec > 8 ? 2 : 1);
    if (expected_file_size > 10000000U) {
        char ch;
        long curpos = ftell(f);
        if (expected_file_size > (OPJ_UINT64)INT_MAX) {
            expected_file_size = (OPJ_UINT64)INT_MAX;
        }
        fseek(f, (long)expected_file_size - 1, SEEK_SET);
        if (fread(&ch, 1, 1, f) != 1) {
            fprintf(stderr, "File too short\n");
            fclose(f);
            return NULL;
        }
        fseek(f, curpos, SEEK_SET);
    }

    /* initialize image component */

    cmptparm.x0 = (OPJ_UINT32)parameters->image_offset_x0;
    cmptparm.y0 = (OPJ_UINT32)parameters->image_offset_y0;
    cmptparm.w = !cmptparm.x0 ? (OPJ_UINT32)((w - 1) * parameters->subsampling_dx +
                 1) : cmptparm.x0 + (OPJ_UINT32)(w - 1) * (OPJ_UINT32)parameters->subsampling_dx
                 + 1;
    cmptparm.h = !cmptparm.y0 ? (OPJ_UINT32)((h - 1) * parameters->subsampling_dy +
                 1) : cmptparm.y0 + (OPJ_UINT32)(h - 1) * (OPJ_UINT32)parameters->subsampling_dy
                 + 1;

    if (sign == '-') {
        cmptparm.sgnd = 1;
    } else {
        cmptparm.sgnd = 0;
    }
    if (prec < 8) {
        force8 = 1;
        ushift = 8 - prec;
        dshift = prec - ushift;
        if (cmptparm.sgnd) {
            adjustS = (1 << (prec - 1));
        } else {
            adjustS = 0;
        }
        cmptparm.sgnd = 0;
        prec = 8;
    } else {
        ushift = dshift = force8 = adjustS = 0;
    }

    cmptparm.prec = (OPJ_UINT32)prec;
    cmptparm.bpp = (OPJ_UINT32)prec;
    cmptparm.dx = (OPJ_UINT32)parameters->subsampling_dx;
    cmptparm.dy = (OPJ_UINT32)parameters->subsampling_dy;

    /* create the image */
    image = opj_image_create((OPJ_UINT32)numcomps, &cmptparm, color_space);
    if (!image) {
        fclose(f);
        return NULL;
    }
    /* set image offset and reference grid */
    image->x0 = cmptparm.x0;
    image->y0 = cmptparm.x0;
    image->x1 = cmptparm.w;
    image->y1 = cmptparm.h;

    /* set image data */

    comp = &image->comps[0];

    for (i = 0; i < w * h; i++) {
        int v;
        if (force8) {
            v = readuchar(f) + adjustS;
            v = (v << ushift) + (v >> dshift);
            comp->data[i] = (unsigned char)v;

            if (v > max) {
                max = v;
            }

            continue;
        }
        if (comp->prec == 8) {
            if (!comp->sgnd) {
                v = readuchar(f);
            } else {
                v = (char) readuchar(f);
            }
        } else if (comp->prec <= 16) {
            if (!comp->sgnd) {
                v = readushort(f, bigendian);
            } else {
                v = (short) readushort(f, bigendian);
            }
        } else {
            if (!comp->sgnd) {
                v = (int)readuint(f, bigendian);
            } else {
                v = (int) readuint(f, bigendian);
            }
        }
        if (v > max) {
            max = v;
        }
        comp->data[i] = v;
    }
    fclose(f);
    comp->bpp = (OPJ_UINT32)int_floorlog2(max) + 1;

    return image;
}

#define CLAMP(x,a,b) ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))

static INLINE int clamp(const int value, const int prec, const int sgnd)
{
    if (sgnd) {
        if (prec <= 8) {
            return CLAMP(value, -128, 127);
        } else if (prec <= 16) {
            return CLAMP(value, -32768, 32767);
        } else {
            return CLAMP(value, -2147483647 - 1, 2147483647);
        }
    } else {
        if (prec <= 8) {
            return CLAMP(value, 0, 255);
        } else if (prec <= 16) {
            return CLAMP(value, 0, 65535);
        } else {
            return value;    /*CLAMP(value,0,4294967295);*/
        }
    }
}

int imagetopgx(opj_image_t * image, const char *outfile)
{
    int w, h;
    int i, j, fails = 1;
    unsigned int compno;
    FILE *fdest = NULL;

    for (compno = 0; compno < image->numcomps; compno++) {
        opj_image_comp_t *comp = &image->comps[compno];
        char bname[256]; /* buffer for name */
        char *name = bname; /* pointer */
        int nbytes = 0;
        size_t res;
        const size_t olen = strlen(outfile);
        const size_t dotpos = olen - 4;
        const size_t total = dotpos + 1 + 1 + 4; /* '-' + '[1-3]' + '.pgx' */

        if (outfile[dotpos] != '.') {
            /* `pgx` was recognized but there is no dot at expected position */
            fprintf(stderr, "ERROR -> Impossible happen.");
            goto fin;
        }
        if (total > 256) {
            name = (char*)malloc(total + 1);
            if (name == NULL) {
                fprintf(stderr, "imagetopgx: memory out\n");
                goto fin;
            }
        }
        strncpy(name, outfile, dotpos);
        sprintf(name + dotpos, "_%u.pgx", compno);
        fdest = fopen(name, "wb");
        /* don't need name anymore */

        if (!fdest) {

            fprintf(stderr, "ERROR -> failed to open %s for writing\n", name);
            if (total > 256) {
                free(name);
            }
            goto fin;
        }

        w = (int)image->comps[compno].w;
        h = (int)image->comps[compno].h;

        fprintf(fdest, "PG ML %c %d %d %d\n", comp->sgnd ? '-' : '+', comp->prec,
                w, h);

        if (comp->prec <= 8) {
            nbytes = 1;
        } else if (comp->prec <= 16) {
            nbytes = 2;
        } else {
            nbytes = 4;
        }

        if (nbytes == 1) {
            unsigned char* line_buffer = malloc((size_t)w);
            if (line_buffer == NULL) {
                fprintf(stderr, "Out of memory");
                goto fin;
            }
            for (j = 0; j < h; j++) {
                if (comp->prec == 8 && comp->sgnd == 0) {
                    for (i = 0; i < w; i++) {
                        line_buffer[i] = (unsigned char)CLAMP(image->comps[compno].data[j * w + i], 0,
                                                              255);
                    }
                } else {
                    for (i = 0; i < w; i++) {
                        line_buffer[i] = (unsigned char)
                                         clamp(image->comps[compno].data[j * w + i],
                                               (int)comp->prec, (int)comp->sgnd);
                    }
                }
                res = fwrite(line_buffer, 1, (size_t)w, fdest);
                if (res != (size_t)w) {
                    fprintf(stderr, "failed to write %d bytes for %s\n", w, name);
                    if (total > 256) {
                        free(name);
                    }
                    free(line_buffer);
                    goto fin;
                }
            }
            free(line_buffer);
        } else {

            for (i = 0; i < w * h; i++) {
                /* FIXME: clamp func is being called within a loop */
                const int val = clamp(image->comps[compno].data[i],
                                      (int)comp->prec, (int)comp->sgnd);

                for (j = nbytes - 1; j >= 0; j--) {
                    int v = (int)(val >> (j * 8));
                    unsigned char byte = (unsigned char)v;
                    res = fwrite(&byte, 1, 1, fdest);

                    if (res < 1) {
                        fprintf(stderr, "failed to write 1 byte for %s\n", name);
                        if (total > 256) {
                            free(name);
                        }
                        goto fin;
                    }
                }
            }
        }

        if (total > 256) {
            free(name);
        }
        fclose(fdest);
        fdest = NULL;
    }
    fails = 0;
fin:
    if (fdest) {
        fclose(fdest);
    }

    return fails;
}

/* -->> -->> -->> -->>

PNM IMAGE FORMAT

<<-- <<-- <<-- <<-- */

struct pnm_header {
    int width, height, maxval, depth, format;
    char rgb, rgba, gray, graya, bw;
    char ok;
};

static char *skip_white(char *s)
{
    if (s != NULL) {
        while (*s) {
            if (*s == '\n' || *s == '\r') {
                return NULL;
            }
            if (isspace(*s)) {
                ++s;
                continue;
            }
            return s;
        }
    }
    return NULL;
}

static char *skip_int(char *start, int *out_n)
{
    char *s;
    char c;

    *out_n = 0;

    s = skip_white(start);
    if (s == NULL) {
        return NULL;
    }
    start = s;

    while (*s) {
        if (!isdigit(*s)) {
            break;
        }
        ++s;
    }
    c = *s;
    *s = 0;
    *out_n = atoi(start);
    *s = c;
    return s;
}

static char *skip_idf(char *start, char out_idf[256])
{
    char *s;
    char c;

    s = skip_white(start);
    if (s == NULL) {
        return NULL;
    }
    start = s;

    while (*s) {
        if (isalpha(*s) || *s == '_') {
            ++s;
            continue;
        }
        break;
    }
    c = *s;
    *s = 0;
    strncpy(out_idf, start, 255);
    *s = c;
    return s;
}

static void read_pnm_header(FILE *reader, struct pnm_header *ph)
{
    int format, end, ttype;
    char idf[256], type[256];
    char line[256];

    if (fgets(line, 250, reader) == NULL) {
        fprintf(stderr, "\nWARNING: fgets return a NULL value");
        return;
    }

    if (line[0] != 'P') {
        fprintf(stderr, "read_pnm_header:PNM:magic P missing\n");
        return;
    }
    format = atoi(line + 1);
    if (format < 1 || format > 7) {
        fprintf(stderr, "read_pnm_header:magic format %d invalid\n", format);
        return;
    }
    ph->format = format;
    ttype = end = 0;

    while (fgets(line, 250, reader)) {
        char *s;
        int allow_null = 0;

        if (*line == '#') {
            continue;
        }

        s = line;

        if (format == 7) {
            s = skip_idf(s, idf);

            if (s == NULL || *s == 0) {
                return;
            }

            if (strcmp(idf, "ENDHDR") == 0) {
                end = 1;
                break;
            }
            if (strcmp(idf, "WIDTH") == 0) {
                s = skip_int(s, &ph->width);
                if (s == NULL || *s == 0) {
                    return;
                }

                continue;
            }
            if (strcmp(idf, "HEIGHT") == 0) {
                s = skip_int(s, &ph->height);
                if (s == NULL || *s == 0) {
                    return;
                }

                continue;
            }
            if (strcmp(idf, "DEPTH") == 0) {
                s = skip_int(s, &ph->depth);
                if (s == NULL || *s == 0) {
                    return;
                }

                continue;
            }
            if (strcmp(idf, "MAXVAL") == 0) {
                s = skip_int(s, &ph->maxval);
                if (s == NULL || *s == 0) {
                    return;
                }

                continue;
            }
            if (strcmp(idf, "TUPLTYPE") == 0) {
                s = skip_idf(s, type);
                if (s == NULL || *s == 0) {
                    return;
                }

                if (strcmp(type, "BLACKANDWHITE") == 0) {
                    ph->bw = 1;
                    ttype = 1;
                    continue;
                }
                if (strcmp(type, "GRAYSCALE") == 0) {
                    ph->gray = 1;
                    ttype = 1;
                    continue;
                }
                if (strcmp(type, "GRAYSCALE_ALPHA") == 0) {
                    ph->graya = 1;
                    ttype = 1;
                    continue;
                }
                if (strcmp(type, "RGB") == 0) {
                    ph->rgb = 1;
                    ttype = 1;
                    continue;
                }
                if (strcmp(type, "RGB_ALPHA") == 0) {
                    ph->rgba = 1;
                    ttype = 1;
                    continue;
                }
                fprintf(stderr, "read_pnm_header:unknown P7 TUPLTYPE %s\n", type);
                return;
            }
            fprintf(stderr, "read_pnm_header:unknown P7 idf %s\n", idf);
            return;
        } /* if(format == 7) */

        /* Here format is in range [1,6] */
        if (ph->width == 0) {
            s = skip_int(s, &ph->width);
            if ((s == NULL) || (*s == 0) || (ph->width < 1)) {
                return;
            }
            allow_null = 1;
        }
        if (ph->height == 0) {
            s = skip_int(s, &ph->height);
            if ((s == NULL) && allow_null) {
                continue;
            }
            if ((s == NULL) || (*s == 0) || (ph->height < 1)) {
                return;
            }
            if (format == 1 || format == 4) {
                break;
            }
            allow_null = 1;
        }
        /* here, format is in P2, P3, P5, P6 */
        s = skip_int(s, &ph->maxval);
        if ((s == NULL) && allow_null) {
            continue;
        }
        if ((s == NULL) || (*s == 0)) {
            return;
        }
        break;
    }/* while(fgets( ) */
    if (format == 2 || format == 3 || format > 4) {
        if (ph->maxval < 1 || ph->maxval > 65535) {
            return;
        }
    }
    if (ph->width < 1 || ph->height < 1) {
        return;
    }

    if (format == 7) {
        if (!end) {
            fprintf(stderr, "read_pnm_header:P7 without ENDHDR\n");
            return;
        }
        if (ph->depth < 1 || ph->depth > 4) {
            return;
        }

        if (ttype) {
            ph->ok = 1;
        }
    } else {
        ph->ok = 1;
        if (format == 1 || format == 4) {
            ph->maxval = 255;
        }
    }
}

static int has_prec(int val)
{
    if (val < 2) {
        return 1;
    }
    if (val < 4) {
        return 2;
    }
    if (val < 8) {
        return 3;
    }
    if (val < 16) {
        return 4;
    }
    if (val < 32) {
        return 5;
    }
    if (val < 64) {
        return 6;
    }
    if (val < 128) {
        return 7;
    }
    if (val < 256) {
        return 8;
    }
    if (val < 512) {
        return 9;
    }
    if (val < 1024) {
        return 10;
    }
    if (val < 2048) {
        return 11;
    }
    if (val < 4096) {
        return 12;
    }
    if (val < 8192) {
        return 13;
    }
    if (val < 16384) {
        return 14;
    }
    if (val < 32768) {
        return 15;
    }
    return 16;
}

opj_image_t* pnmtoimage(const char *filename, opj_cparameters_t *parameters)
{
    int subsampling_dx = parameters->subsampling_dx;
    int subsampling_dy = parameters->subsampling_dy;

    FILE *fp = NULL;
    int i, compno, numcomps, w, h, prec, format;
    OPJ_COLOR_SPACE color_space;
    opj_image_cmptparm_t cmptparm[4]; /* RGBA: max. 4 components */
    opj_image_t * image = NULL;
    struct pnm_header header_info;

    if ((fp = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "pnmtoimage:Failed to open %s for reading!\n", filename);
        return NULL;
    }
    memset(&header_info, 0, sizeof(struct pnm_header));

    read_pnm_header(fp, &header_info);

    if (!header_info.ok) {
        fclose(fp);
        return NULL;
    }

    /* This limitation could be removed by making sure to use size_t below */
    if (header_info.height != 0 &&
            header_info.width > INT_MAX / header_info.height) {
        fprintf(stderr, "pnmtoimage:Image %dx%d too big!\n",
                header_info.width, header_info.height);
        fclose(fp);
        return NULL;
    }

    format = header_info.format;

    switch (format) {
    case 1: /* ascii bitmap */
    case 4: /* raw bitmap */
        numcomps = 1;
        break;

    case 2: /* ascii greymap */
    case 5: /* raw greymap */
        numcomps = 1;
        break;

    case 3: /* ascii pixmap */
    case 6: /* raw pixmap */
        numcomps = 3;
        break;

    case 7: /* arbitrary map */
        numcomps = header_info.depth;
        break;

    default:
        fclose(fp);
        return NULL;
    }
    if (numcomps < 3) {
        color_space = OPJ_CLRSPC_GRAY;    /* GRAY, GRAYA */
    } else {
        color_space = OPJ_CLRSPC_SRGB;    /* RGB, RGBA */
    }

    prec = has_prec(header_info.maxval);

    if (prec < 8) {
        prec = 8;
    }

    w = header_info.width;
    h = header_info.height;
    subsampling_dx = parameters->subsampling_dx;
    subsampling_dy = parameters->subsampling_dy;

    memset(&cmptparm[0], 0, (size_t)numcomps * sizeof(opj_image_cmptparm_t));

    for (i = 0; i < numcomps; i++) {
        cmptparm[i].prec = (OPJ_UINT32)prec;
        cmptparm[i].bpp = (OPJ_UINT32)prec;
        cmptparm[i].sgnd = 0;
        cmptparm[i].dx = (OPJ_UINT32)subsampling_dx;
        cmptparm[i].dy = (OPJ_UINT32)subsampling_dy;
        cmptparm[i].w = (OPJ_UINT32)w;
        cmptparm[i].h = (OPJ_UINT32)h;
    }
    image = opj_image_create((OPJ_UINT32)numcomps, &cmptparm[0], color_space);

    if (!image) {
        fclose(fp);
        return NULL;
    }

    /* set image offset and reference grid */
    image->x0 = (OPJ_UINT32)parameters->image_offset_x0;
    image->y0 = (OPJ_UINT32)parameters->image_offset_y0;
    image->x1 = (OPJ_UINT32)(parameters->image_offset_x0 + (w - 1) * subsampling_dx
                             + 1);
    image->y1 = (OPJ_UINT32)(parameters->image_offset_y0 + (h - 1) * subsampling_dy
                             + 1);

    if ((format == 2) || (format == 3)) { /* ascii pixmap */
        unsigned int index;

        for (i = 0; i < w * h; i++) {
            for (compno = 0; compno < numcomps; compno++) {
                index = 0;
                if (fscanf(fp, "%u", &index) != 1) {
                    fprintf(stderr,
                            "\nWARNING: fscanf return a number of element different from the expected.\n");
                }

                image->comps[compno].data[i] = (OPJ_INT32)(index * 255) / header_info.maxval;
            }
        }
    } else if ((format == 5)
               || (format == 6)
               || ((format == 7)
                   && (header_info.gray || header_info.graya
                       || header_info.rgb || header_info.rgba))) { /* binary pixmap */
        unsigned char c0, c1, one;

        one = (prec < 9);

        for (i = 0; i < w * h; i++) {
            for (compno = 0; compno < numcomps; compno++) {
                if (!fread(&c0, 1, 1, fp)) {
                    fprintf(stderr,
                            "\nError: fread return a number of element different from the expected.\n");
                    opj_image_destroy(image);
                    fclose(fp);
                    return NULL;
                }
                if (one) {
                    image->comps[compno].data[i] = c0;
                } else {
                    if (!fread(&c1, 1, 1, fp)) {
                        fprintf(stderr,
                                "\nError: fread return a number of element different from the expected.\n");
                    }
                    /* netpbm: */
                    image->comps[compno].data[i] = ((c0 << 8) | c1);
                }
            }
        }
    } else if (format == 1) { /* ascii bitmap */
        for (i = 0; i < w * h; i++) {
            unsigned int index;

            if (fscanf(fp, "%u", &index) != 1) {
                fprintf(stderr,
                        "\nWARNING: fscanf return a number of element different from the expected.\n");
            }

            image->comps[0].data[i] = (index ? 0 : 255);
        }
    } else if (format == 4) {
        int x, y, bit;
        unsigned char uc;

        i = 0;
        for (y = 0; y < h; ++y) {
            bit = -1;
            uc = 0;

            for (x = 0; x < w; ++x) {
                if (bit == -1) {
                    bit = 7;
                    uc = (unsigned char)getc(fp);
                }
                image->comps[0].data[i] = (((uc >> bit) & 1) ? 0 : 255);
                --bit;
                ++i;
            }
        }
    } else if ((format == 7 && header_info.bw)) { /*MONO*/
        unsigned char uc;

        for (i = 0; i < w * h; ++i) {
            if (!fread(&uc, 1, 1, fp)) {
                fprintf(stderr,
                        "\nError: fread return a number of element different from the expected.\n");
            }
            image->comps[0].data[i] = (uc & 1) ? 0 : 255;
        }
    }
    fclose(fp);

    return image;
}/* pnmtoimage() */

static int are_comps_similar(opj_image_t * image)
{
    unsigned int i;
    for (i = 1; i < image->numcomps; i++) {
        if (image->comps[0].dx != image->comps[i].dx ||
                image->comps[0].dy != image->comps[i].dy ||
                (i <= 2 &&
                 (image->comps[0].prec != image->comps[i].prec ||
                  image->comps[0].sgnd != image->comps[i].sgnd))) {
            return OPJ_FALSE;
        }
    }
    return OPJ_TRUE;
}


int imagetopnm(opj_image_t * image, const char *outfile, int force_split)
{
    int *red, *green, *blue, *alpha;
    int wr, hr, max;
    int i;
    unsigned int compno, ncomp;
    int adjustR, adjustG, adjustB, adjustA;
    int fails, two, want_gray, has_alpha, triple;
    int prec, v;
    FILE *fdest = NULL;
    const char *tmp = outfile;
    char *destname;

    alpha = NULL;

    if ((prec = (int)image->comps[0].prec) > 16) {
        fprintf(stderr, "%s:%d:imagetopnm\n\tprecision %d is larger than 16"
                "\n\t: refused.\n", __FILE__, __LINE__, prec);
        return 1;
    }
    two = has_alpha = 0;
    fails = 1;
    ncomp = image->numcomps;

    while (*tmp) {
        ++tmp;
    }
    tmp -= 2;
    want_gray = (*tmp == 'g' || *tmp == 'G');
    ncomp = image->numcomps;

    if (want_gray) {
        ncomp = 1;
    }

    if ((force_split == 0) && ncomp >= 2 &&
            are_comps_similar(image)) {
        fdest = fopen(outfile, "wb");

        if (!fdest) {
            fprintf(stderr, "ERROR -> failed to open %s for writing\n", outfile);
            return fails;
        }
        two = (prec > 8);
        triple = (ncomp > 2);
        wr = (int)image->comps[0].w;
        hr = (int)image->comps[0].h;
        max = (1 << prec) - 1;
        has_alpha = (ncomp == 4 || ncomp == 2);

        red = image->comps[0].data;

        if (triple) {
            green = image->comps[1].data;
            blue = image->comps[2].data;
        } else {
            green = blue = NULL;
        }

        if (has_alpha) {
            const char *tt = (triple ? "RGB_ALPHA" : "GRAYSCALE_ALPHA");

            fprintf(fdest, "P7\n# OpenJPEG-%s\nWIDTH %d\nHEIGHT %d\nDEPTH %u\n"
                    "MAXVAL %d\nTUPLTYPE %s\nENDHDR\n", opj_version(),
                    wr, hr, ncomp, max, tt);
            alpha = image->comps[ncomp - 1].data;
            adjustA = (image->comps[ncomp - 1].sgnd ?
                       1 << (image->comps[ncomp - 1].prec - 1) : 0);
        } else {
            fprintf(fdest, "P6\n# OpenJPEG-%s\n%d %d\n%d\n",
                    opj_version(), wr, hr, max);
            adjustA = 0;
        }
        adjustR = (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);

        if (triple) {
            adjustG = (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);
            adjustB = (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);
        } else {
            adjustG = adjustB = 0;
        }

        for (i = 0; i < wr * hr; ++i) {
            if (two) {
                v = *red + adjustR;
                ++red;
                if (v > 65535) {
                    v = 65535;
                } else if (v < 0) {
                    v = 0;
                }

                /* netpbm: */
                fprintf(fdest, "%c%c", (unsigned char)(v >> 8), (unsigned char)v);

                if (triple) {
                    v = *green + adjustG;
                    ++green;
                    if (v > 65535) {
                        v = 65535;
                    } else if (v < 0) {
                        v = 0;
                    }

                    /* netpbm: */
                    fprintf(fdest, "%c%c", (unsigned char)(v >> 8), (unsigned char)v);

                    v =  *blue + adjustB;
                    ++blue;
                    if (v > 65535) {
                        v = 65535;
                    } else if (v < 0) {
                        v = 0;
                    }

                    /* netpbm: */
                    fprintf(fdest, "%c%c", (unsigned char)(v >> 8), (unsigned char)v);

                }/* if(triple) */

                if (has_alpha) {
                    v = *alpha + adjustA;
                    ++alpha;
                    if (v > 65535) {
                        v = 65535;
                    } else if (v < 0) {
                        v = 0;
                    }

                    /* netpbm: */
                    fprintf(fdest, "%c%c", (unsigned char)(v >> 8), (unsigned char)v);
                }
                continue;

            }   /* if(two) */

            /* prec <= 8: */
            v = *red++;
            if (v > 255) {
                v = 255;
            } else if (v < 0) {
                v = 0;
            }

            fprintf(fdest, "%c", (unsigned char)v);
            if (triple) {
                v = *green++;
                if (v > 255) {
                    v = 255;
                } else if (v < 0) {
                    v = 0;
                }

                fprintf(fdest, "%c", (unsigned char)v);
                v = *blue++;
                if (v > 255) {
                    v = 255;
                } else if (v < 0) {
                    v = 0;
                }

                fprintf(fdest, "%c", (unsigned char)v);
            }
            if (has_alpha) {
                v = *alpha++;
                if (v > 255) {
                    v = 255;
                } else if (v < 0) {
                    v = 0;
                }

                fprintf(fdest, "%c", (unsigned char)v);
            }
        }   /* for(i */

        fclose(fdest);
        return 0;
    }

    /* YUV or MONO: */

    if (image->numcomps > ncomp) {
        fprintf(stderr, "WARNING -> [PGM file] Only the first component\n");
        fprintf(stderr, "           is written to the file\n");
    }
    destname = (char*)malloc(strlen(outfile) + 8);
    if (destname == NULL) {
        fprintf(stderr, "imagetopnm: memory out\n");
        return 1;
    }
    for (compno = 0; compno < ncomp; compno++) {
        if (ncomp > 1) {
            /*sprintf(destname, "%d.%s", compno, outfile);*/
            const size_t olen = strlen(outfile);
            const size_t dotpos = olen - 4;

            strncpy(destname, outfile, dotpos);
            sprintf(destname + dotpos, "_%u.pgm", compno);
        } else {
            sprintf(destname, "%s", outfile);
        }

        fdest = fopen(destname, "wb");
        if (!fdest) {
            fprintf(stderr, "ERROR -> failed to open %s for writing\n", destname);
            free(destname);
            return 1;
        }
        wr = (int)image->comps[compno].w;
        hr = (int)image->comps[compno].h;
        prec = (int)image->comps[compno].prec;
        max = (1 << prec) - 1;

        fprintf(fdest, "P5\n#OpenJPEG-%s\n%d %d\n%d\n",
                opj_version(), wr, hr, max);

        red = image->comps[compno].data;
        adjustR =
            (image->comps[compno].sgnd ? 1 << (image->comps[compno].prec - 1) : 0);

        if (prec > 8) {
            for (i = 0; i < wr * hr; i++) {
                v = *red + adjustR;
                ++red;
                if (v > 65535) {
                    v = 65535;
                } else if (v < 0) {
                    v = 0;
                }

                /* netpbm: */
                fprintf(fdest, "%c%c", (unsigned char)(v >> 8), (unsigned char)v);

                if (has_alpha) {
                    v = *alpha++;
                    if (v > 65535) {
                        v = 65535;
                    } else if (v < 0) {
                        v = 0;
                    }

                    /* netpbm: */
                    fprintf(fdest, "%c%c", (unsigned char)(v >> 8), (unsigned char)v);
                }
            }/* for(i */
        } else { /* prec <= 8 */
            for (i = 0; i < wr * hr; ++i) {
                v = *red + adjustR;
                ++red;
                if (v > 255) {
                    v = 255;
                } else if (v < 0) {
                    v = 0;
                }

                fprintf(fdest, "%c", (unsigned char)v);
            }
        }
        fclose(fdest);
    } /* for (compno */
    free(destname);

    return 0;
}/* imagetopnm() */

/* -->> -->> -->> -->>

    RAW IMAGE FORMAT

 <<-- <<-- <<-- <<-- */
static opj_image_t* rawtoimage_common(const char *filename,
                                      opj_cparameters_t *parameters, raw_cparameters_t *raw_cp, OPJ_BOOL big_endian)
{
    int subsampling_dx = parameters->subsampling_dx;
    int subsampling_dy = parameters->subsampling_dy;

    FILE *f = NULL;
    int i, compno, numcomps, w, h;
    OPJ_COLOR_SPACE color_space;
    opj_image_cmptparm_t *cmptparm;
    opj_image_t * image = NULL;
    unsigned short ch;

    if ((!(raw_cp->rawWidth & raw_cp->rawHeight & raw_cp->rawComp &
            raw_cp->rawBitDepth)) == 0) {
        fprintf(stderr, "\nError: invalid raw image parameters\n");
        fprintf(stderr, "Please use the Format option -F:\n");
        fprintf(stderr,
                "-F <width>,<height>,<ncomp>,<bitdepth>,{s,u}@<dx1>x<dy1>:...:<dxn>x<dyn>\n");
        fprintf(stderr,
                "If subsampling is omitted, 1x1 is assumed for all components\n");
        fprintf(stderr,
                "Example: -i image.raw -o image.j2k -F 512,512,3,8,u@1x1:2x2:2x2\n");
        fprintf(stderr, "         for raw 512x512 image with 4:2:0 subsampling\n");
        fprintf(stderr, "Aborting.\n");
        return NULL;
    }

    f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s for reading !!\n", filename);
        fprintf(stderr, "Aborting\n");
        return NULL;
    }
    numcomps = raw_cp->rawComp;

    /* FIXME ADE at this point, tcp_mct has not been properly set in calling function */
    if (numcomps == 1) {
        color_space = OPJ_CLRSPC_GRAY;
    } else if ((numcomps >= 3) && (parameters->tcp_mct == 0)) {
        color_space = OPJ_CLRSPC_SYCC;
    } else if ((numcomps >= 3) && (parameters->tcp_mct != 2)) {
        color_space = OPJ_CLRSPC_SRGB;
    } else {
        color_space = OPJ_CLRSPC_UNKNOWN;
    }
    w = raw_cp->rawWidth;
    h = raw_cp->rawHeight;
    cmptparm = (opj_image_cmptparm_t*) calloc((OPJ_UINT32)numcomps,
               sizeof(opj_image_cmptparm_t));
    if (!cmptparm) {
        fprintf(stderr, "Failed to allocate image components parameters !!\n");
        fprintf(stderr, "Aborting\n");
        fclose(f);
        return NULL;
    }
    /* initialize image components */
    for (i = 0; i < numcomps; i++) {
        cmptparm[i].prec = (OPJ_UINT32)raw_cp->rawBitDepth;
        cmptparm[i].bpp = (OPJ_UINT32)raw_cp->rawBitDepth;
        cmptparm[i].sgnd = (OPJ_UINT32)raw_cp->rawSigned;
        cmptparm[i].dx = (OPJ_UINT32)(subsampling_dx * raw_cp->rawComps[i].dx);
        cmptparm[i].dy = (OPJ_UINT32)(subsampling_dy * raw_cp->rawComps[i].dy);
        cmptparm[i].w = (OPJ_UINT32)w;
        cmptparm[i].h = (OPJ_UINT32)h;
    }
    /* create the image */
    image = opj_image_create((OPJ_UINT32)numcomps, &cmptparm[0], color_space);
    free(cmptparm);
    if (!image) {
        fclose(f);
        return NULL;
    }
    /* set image offset and reference grid */
    image->x0 = (OPJ_UINT32)parameters->image_offset_x0;
    image->y0 = (OPJ_UINT32)parameters->image_offset_y0;
    image->x1 = (OPJ_UINT32)parameters->image_offset_x0 + (OPJ_UINT32)(w - 1) *
                (OPJ_UINT32)subsampling_dx + 1;
    image->y1 = (OPJ_UINT32)parameters->image_offset_y0 + (OPJ_UINT32)(h - 1) *
                (OPJ_UINT32)subsampling_dy + 1;

    if (raw_cp->rawBitDepth <= 8) {
        unsigned char value = 0;
        for (compno = 0; compno < numcomps; compno++) {
            int nloop = (w * h) / (raw_cp->rawComps[compno].dx *
                                   raw_cp->rawComps[compno].dy);
            for (i = 0; i < nloop; i++) {
                if (!fread(&value, 1, 1, f)) {
                    fprintf(stderr, "Error reading raw file. End of file probably reached.\n");
                    opj_image_destroy(image);
                    fclose(f);
                    return NULL;
                }
                image->comps[compno].data[i] = raw_cp->rawSigned ? (char)value : value;
            }
        }
    } else if (raw_cp->rawBitDepth <= 16) {
        unsigned short value;
        for (compno = 0; compno < numcomps; compno++) {
            int nloop = (w * h) / (raw_cp->rawComps[compno].dx *
                                   raw_cp->rawComps[compno].dy);
            for (i = 0; i < nloop; i++) {
                unsigned char temp1;
                unsigned char temp2;
                if (!fread(&temp1, 1, 1, f)) {
                    fprintf(stderr, "Error reading raw file. End of file probably reached.\n");
                    opj_image_destroy(image);
                    fclose(f);
                    return NULL;
                }
                if (!fread(&temp2, 1, 1, f)) {
                    fprintf(stderr, "Error reading raw file. End of file probably reached.\n");
                    opj_image_destroy(image);
                    fclose(f);
                    return NULL;
                }
                if (big_endian) {
                    value = (unsigned short)((temp1 << 8) + temp2);
                } else {
                    value = (unsigned short)((temp2 << 8) + temp1);
                }
                image->comps[compno].data[i] = raw_cp->rawSigned ? (short)value : value;
            }
        }
    } else {
        fprintf(stderr,
                "OpenJPEG cannot encode raw components with bit depth higher than 16 bits.\n");
        opj_image_destroy(image);
        fclose(f);
        return NULL;
    }

    if (fread(&ch, 1, 1, f)) {
        fprintf(stderr, "Warning. End of raw file not reached... processing anyway\n");
    }
    fclose(f);

    return image;
}

opj_image_t* rawltoimage(const char *filename, opj_cparameters_t *parameters,
                         raw_cparameters_t *raw_cp)
{
    return rawtoimage_common(filename, parameters, raw_cp, OPJ_FALSE);
}

opj_image_t* rawtoimage(const char *filename, opj_cparameters_t *parameters,
                        raw_cparameters_t *raw_cp)
{
    return rawtoimage_common(filename, parameters, raw_cp, OPJ_TRUE);
}

static int imagetoraw_common(opj_image_t * image, const char *outfile,
                             OPJ_BOOL big_endian)
{
    FILE *rawFile = NULL;
    size_t res;
    unsigned int compno, numcomps;
    int w, h, fails;
    int line, row, curr, mask;
    int *ptr;
    unsigned char uc;
    (void)big_endian;

    if ((image->numcomps * image->x1 * image->y1) == 0) {
        fprintf(stderr, "\nError: invalid raw image parameters\n");
        return 1;
    }

    numcomps = image->numcomps;

    if (numcomps > 4) {
        numcomps = 4;
    }

    for (compno = 1; compno < numcomps; ++compno) {
        if (image->comps[0].dx != image->comps[compno].dx) {
            break;
        }
        if (image->comps[0].dy != image->comps[compno].dy) {
            break;
        }
        if (image->comps[0].prec != image->comps[compno].prec) {
            break;
        }
        if (image->comps[0].sgnd != image->comps[compno].sgnd) {
            break;
        }
    }
    if (compno != numcomps) {
        fprintf(stderr,
                "imagetoraw_common: All components shall have the same subsampling, same bit depth, same sign.\n");
        fprintf(stderr, "\tAborting\n");
        return 1;
    }

    rawFile = fopen(outfile, "wb");
    if (!rawFile) {
        fprintf(stderr, "Failed to open %s for writing !!\n", outfile);
        return 1;
    }

    fails = 1;
    fprintf(stdout, "Raw image characteristics: %d components\n", image->numcomps);

    for (compno = 0; compno < image->numcomps; compno++) {
        fprintf(stdout, "Component %u characteristics: %dx%dx%d %s\n", compno,
                image->comps[compno].w,
                image->comps[compno].h, image->comps[compno].prec,
                image->comps[compno].sgnd == 1 ? "signed" : "unsigned");

        w = (int)image->comps[compno].w;
        h = (int)image->comps[compno].h;

        if (image->comps[compno].prec <= 8) {
            if (image->comps[compno].sgnd == 1) {
                mask = (1 << image->comps[compno].prec) - 1;
                ptr = image->comps[compno].data;
                for (line = 0; line < h; line++) {
                    for (row = 0; row < w; row++)    {
                        curr = *ptr;
                        if (curr > 127) {
                            curr = 127;
                        } else if (curr < -128) {
                            curr = -128;
                        }
                        uc = (unsigned char)(curr & mask);
                        res = fwrite(&uc, 1, 1, rawFile);
                        if (res < 1) {
                            fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                            goto fin;
                        }
                        ptr++;
                    }
                }
            } else if (image->comps[compno].sgnd == 0) {
                mask = (1 << image->comps[compno].prec) - 1;
                ptr = image->comps[compno].data;
                for (line = 0; line < h; line++) {
                    for (row = 0; row < w; row++)    {
                        curr = *ptr;
                        if (curr > 255) {
                            curr = 255;
                        } else if (curr < 0) {
                            curr = 0;
                        }
                        uc = (unsigned char)(curr & mask);
                        res = fwrite(&uc, 1, 1, rawFile);
                        if (res < 1) {
                            fprintf(stderr, "failed to write 1 byte for %s\n", outfile);
                            goto fin;
                        }
                        ptr++;
                    }
                }
            }
        } else if (image->comps[compno].prec <= 16) {
            if (image->comps[compno].sgnd == 1) {
                union {
                    signed short val;
                    signed char vals[2];
                } uc16;
                mask = (1 << image->comps[compno].prec) - 1;
                ptr = image->comps[compno].data;
                for (line = 0; line < h; line++) {
                    for (row = 0; row < w; row++)    {
                        curr = *ptr;
                        if (curr > 32767) {
                            curr = 32767;
                        } else if (curr < -32768) {
                            curr = -32768;
                        }
                        uc16.val = (signed short)(curr & mask);
                        res = fwrite(uc16.vals, 1, 2, rawFile);
                        if (res < 2) {
                            fprintf(stderr, "failed to write 2 byte for %s\n", outfile);
                            goto fin;
                        }
                        ptr++;
                    }
                }
            } else if (image->comps[compno].sgnd == 0) {
                union {
                    unsigned short val;
                    unsigned char vals[2];
                } uc16;
                mask = (1 << image->comps[compno].prec) - 1;
                ptr = image->comps[compno].data;
                for (line = 0; line < h; line++) {
                    for (row = 0; row < w; row++)    {
                        curr = *ptr;
                        if (curr > 65535) {
                            curr = 65535;
                        } else if (curr < 0) {
                            curr = 0;
                        }
                        uc16.val = (unsigned short)(curr & mask);
                        res = fwrite(uc16.vals, 1, 2, rawFile);
                        if (res < 2) {
                            fprintf(stderr, "failed to write 2 byte for %s\n", outfile);
                            goto fin;
                        }
                        ptr++;
                    }
                }
            }
        } else if (image->comps[compno].prec <= 32) {
            fprintf(stderr, "More than 16 bits per component not handled yet\n");
            goto fin;
        } else {
            fprintf(stderr, "Error: invalid precision: %d\n", image->comps[compno].prec);
            goto fin;
        }
    }
    fails = 0;
fin:
    fclose(rawFile);
    return fails;
}

int imagetoraw(opj_image_t * image, const char *outfile)
{
    return imagetoraw_common(image, outfile, OPJ_TRUE);
}

int imagetorawl(opj_image_t * image, const char *outfile)
{
    return imagetoraw_common(image, outfile, OPJ_FALSE);
}


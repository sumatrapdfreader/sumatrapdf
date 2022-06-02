/* compare258_sse.c -- SSE4.2 version of compare258
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 * Authors:
 *  Wajdi Feghali   <wajdi.k.feghali@intel.com>
 *  Jim Guilford    <james.guilford@intel.com>
 *  Vinodh Gopal    <vinodh.gopal@intel.com>
 *     Erdinc Ozturk   <erdinc.ozturk@intel.com>
 *  Jim Kukunas     <james.t.kukunas@linux.intel.com>
 *
 * Portions are Copyright (C) 2016 12Sided Technology, LLC.
 * Author:
 *  Phil Vachon     <pvachon@12sidedtech.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "../../zbuild.h"
#include "../../zutil.h"

#ifdef X86_SSE42_CMP_STR

#include <immintrin.h>
#ifdef _MSC_VER
#  include <nmmintrin.h>
#endif

/* UNALIGNED_OK, SSE4.2 intrinsic comparison */
static inline uint32_t compare256_unaligned_sse4_static(const unsigned char *src0, const unsigned char *src1) {
    uint32_t len = 0;

    do {
        #define mode _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_NEGATIVE_POLARITY
        __m128i xmm_src0, xmm_src1;
        uint32_t ret;

        xmm_src0 = _mm_loadu_si128((__m128i *)src0);
        xmm_src1 = _mm_loadu_si128((__m128i *)src1);
        ret = (uint32_t)_mm_cmpestri(xmm_src0, 16, xmm_src1, 16, mode);
        if (_mm_cmpestrc(xmm_src0, 16, xmm_src1, 16, mode)) {
            return len + ret;
        }
        src0 += 16, src1 += 16, len += 16;

        xmm_src0 = _mm_loadu_si128((__m128i *)src0);
        xmm_src1 = _mm_loadu_si128((__m128i *)src1);
        ret = (uint32_t)_mm_cmpestri(xmm_src0, 16, xmm_src1, 16, mode);
        if (_mm_cmpestrc(xmm_src0, 16, xmm_src1, 16, mode)) {
            return len + ret;
        }
        src0 += 16, src1 += 16, len += 16;
    } while (len < 256);

    return 256;
}

static inline uint32_t compare258_unaligned_sse4_static(const unsigned char *src0, const unsigned char *src1) {
    if (*(uint16_t *)src0 != *(uint16_t *)src1)
        return (*src0 == *src1);

    return compare256_unaligned_sse4_static(src0+2, src1+2) + 2;
}

Z_INTERNAL uint32_t compare258_unaligned_sse4(const unsigned char *src0, const unsigned char *src1) {
    return compare258_unaligned_sse4_static(src0, src1);
}

#define LONGEST_MATCH   longest_match_unaligned_sse4
#define COMPARE256      compare256_unaligned_sse4_static
#define COMPARE258      compare258_unaligned_sse4_static

#include "match_tpl.h"

#endif

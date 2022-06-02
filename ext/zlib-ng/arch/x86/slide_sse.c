/*
 * SSE optimized hash slide
 *
 * Copyright (C) 2017 Intel Corporation
 * Authors:
 *   Arjan van de Ven   <arjan@linux.intel.com>
 *   Jim Kukunas        <james.t.kukunas@linux.intel.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#include "../../zbuild.h"
#include "../../deflate.h"

#include <immintrin.h>

Z_INTERNAL void slide_hash_sse2(deflate_state *s) {
    Pos *p;
    unsigned n;
    uint16_t wsize = (uint16_t)s->w_size;
    const __m128i xmm_wsize = _mm_set1_epi16((short)wsize);

    n = HASH_SIZE;
    p = &s->head[n] - 8;
    do {
        __m128i value, result;

        value = _mm_loadu_si128((__m128i *)p);
        result= _mm_subs_epu16(value, xmm_wsize);
        _mm_storeu_si128((__m128i *)p, result);
        p -= 8;
        n -= 8;
    } while (n > 0);

    n = wsize;
    p = &s->prev[n] - 8;
    do {
        __m128i value, result;

        value = _mm_loadu_si128((__m128i *)p);
        result= _mm_subs_epu16(value, xmm_wsize);
        _mm_storeu_si128((__m128i *)p, result);

        p -= 8;
        n -= 8;
    } while (n > 0);
}

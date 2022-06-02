/*
 * AVX2 optimized hash slide, based on Intel's slide_sse implementation
 *
 * Copyright (C) 2017 Intel Corporation
 * Authors:
 *   Arjan van de Ven   <arjan@linux.intel.com>
 *   Jim Kukunas        <james.t.kukunas@linux.intel.com>
 *   Mika T. Lindqvist  <postmaster@raasu.org>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#include "../../zbuild.h"
#include "../../deflate.h"

#include <immintrin.h>

Z_INTERNAL void slide_hash_avx2(deflate_state *s) {
    Pos *p;
    unsigned n;
    uint16_t wsize = (uint16_t)s->w_size;
    const __m256i ymm_wsize = _mm256_set1_epi16((short)wsize);

    n = HASH_SIZE;
    p = &s->head[n] - 16;
    do {
        __m256i value, result;

        value = _mm256_loadu_si256((__m256i *)p);
        result= _mm256_subs_epu16(value, ymm_wsize);
        _mm256_storeu_si256((__m256i *)p, result);
        p -= 16;
        n -= 16;
    } while (n > 0);

    n = wsize;
    p = &s->prev[n] - 16;
    do {
        __m256i value, result;

        value = _mm256_loadu_si256((__m256i *)p);
        result= _mm256_subs_epu16(value, ymm_wsize);
        _mm256_storeu_si256((__m256i *)p, result);

        p -= 16;
        n -= 16;
    } while (n > 0);
}

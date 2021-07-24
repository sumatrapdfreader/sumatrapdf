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

static inline void slide_hash_chain(Pos *table, uint32_t entries, const __m128i wsize) {
    table += entries;
    table -= 8;

    do {
        __m128i value, result;

        value = _mm_loadu_si128((__m128i *)table);
        result= _mm_subs_epu16(value, wsize);
        _mm_storeu_si128((__m128i *)table, result);

        table -= 8;
        entries -= 8;
    } while (entries > 0);
}

Z_INTERNAL void slide_hash_sse2(deflate_state *s) {
    uint16_t wsize = (uint16_t)s->w_size;
    const __m128i xmm_wsize = _mm_set1_epi16((short)wsize);

    slide_hash_chain(s->head, HASH_SIZE, xmm_wsize);
    slide_hash_chain(s->prev, wsize, xmm_wsize);
}

/* slide_neon.c -- Optimized hash table shifting for ARM with support for NEON instructions
 * Copyright (C) 2017-2020 Mika T. Lindqvist
 *
 * Authors:
 * Mika T. Lindqvist <postmaster@raasu.org>
 * Jun He <jun.he@arm.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#if defined(ARM_NEON_SLIDEHASH)
#ifdef _M_ARM64
#  include <arm64_neon.h>
#else
#  include <arm_neon.h>
#endif
#include "../../zbuild.h"
#include "../../deflate.h"

/* SIMD version of hash_chain rebase */
static inline void slide_hash_chain(Pos *table, unsigned int entries, uint16_t window_size) {
    Z_REGISTER uint16x8_t v, *p;
    Z_REGISTER size_t n;

    size_t size = entries*sizeof(table[0]);
    Assert((size % sizeof(uint16x8_t) * 8 == 0), "hash table size err");

    Assert(sizeof(Pos) == 2, "Wrong Pos size");
    v = vdupq_n_u16(window_size);

    p = (uint16x8_t *)table;
    n = size / (sizeof(uint16x8_t) * 8);
    do {
        p[0] = vqsubq_u16(p[0], v);
        p[1] = vqsubq_u16(p[1], v);
        p[2] = vqsubq_u16(p[2], v);
        p[3] = vqsubq_u16(p[3], v);
        p[4] = vqsubq_u16(p[4], v);
        p[5] = vqsubq_u16(p[5], v);
        p[6] = vqsubq_u16(p[6], v);
        p[7] = vqsubq_u16(p[7], v);
        p += 8;
    } while (--n);
}

Z_INTERNAL void slide_hash_neon(deflate_state *s) {
    unsigned int wsize = s->w_size;

    slide_hash_chain(s->head, HASH_SIZE, wsize);
    slide_hash_chain(s->prev, wsize, wsize);
}
#endif

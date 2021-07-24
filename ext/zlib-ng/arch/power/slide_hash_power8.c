/* Optimized slide_hash for POWER processors
 * Copyright (C) 2019-2020 IBM Corporation
 * Author: Matheus Castanho <msc@linux.ibm.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef POWER8_VSX_SLIDEHASH

#include <altivec.h>
#include "zbuild.h"
#include "deflate.h"

static inline void slide_hash_chain(Pos *table, uint32_t entries, uint16_t wsize) {
    vector unsigned short vw, vm, *vp;
    unsigned chunks;

    table += entries;

    /* Each vector register (chunk) corresponds to 128 bits == 8 Posf,
     * so instead of processing each of the entries in the hash table
     * individually, we can do it in chunks of 8 with vector instructions.
     *
     * This function is only called from slide_hash_power8(), and both calls
     * pass entries as a power of 2 higher than 2^7, as defined by
     * deflateInit2_(), so entries will always be a multiple of 8. */
    chunks = entries >> 3;
    Assert(entries % 8 == 0, "Weird hash table size!");

    vw[0] = wsize;
    vw = vec_splat(vw,0);

    vp = (vector unsigned short *)table;

    do {
        /* Processing 8 elements at a time */
        vp--;
        vm = *vp;

        /* This is equivalent to: m >= wsize ? m - wsize : 0
         * Since we are using a saturated unsigned subtraction, any
         * values that are <= wsize will be set to 0, while the others
         * will be subtracted by wsize. */
        *vp = vec_subs(vm,vw);
    } while (--chunks);
}

void Z_INTERNAL slide_hash_power8(deflate_state *s) {
    uint16_t wsize = s->w_size;

    slide_hash_chain(s->head, HASH_SIZE, wsize);
    slide_hash_chain(s->prev, wsize, wsize);
}

#endif /* POWER8_VSX_SLIDEHASH */

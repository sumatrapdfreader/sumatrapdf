/* Optimized slide_hash for POWER processors
 * Copyright (C) 2019-2020 IBM Corporation
 * Author: Matheus Castanho <msc@linux.ibm.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef POWER8_VSX_SLIDEHASH

#include <altivec.h>
#include "zbuild.h"
#include "deflate.h"

static inline void slide_hash_power8_loop(deflate_state *s, unsigned n_elems, Pos *table_end) {
    vector unsigned short vw, vm, *vp;
    unsigned chunks;

    /* Each vector register (chunk) corresponds to 128 bits == 8 Posf,
     * so instead of processing each of the n_elems in the hash table
     * individually, we can do it in chunks of 8 with vector instructions.
     *
     * This function is only called from slide_hash_power8(), and both calls
     * pass n_elems as a power of 2 higher than 2^7, as defined by
     * deflateInit2_(), so n_elems will always be a multiple of 8. */
    chunks = n_elems >> 3;
    Assert(n_elems % 8 == 0, "Weird hash table size!");

    /* This type casting is safe since s->w_size is always <= 64KB
     * as defined by deflateInit2_() and Posf == unsigned short */
    vw[0] = (Pos) s->w_size;
    vw = vec_splat(vw,0);

    vp = (vector unsigned short *) table_end;

    do {
        /* Processing 8 elements at a time */
        vp--;
        vm = *vp;

        /* This is equivalent to: m >= w_size ? m - w_size : 0
         * Since we are using a saturated unsigned subtraction, any
         * values that are > w_size will be set to 0, while the others
         * will be subtracted by w_size. */
        *vp = vec_subs(vm,vw);
    } while (--chunks);
}

void Z_INTERNAL slide_hash_power8(deflate_state *s) {
    unsigned int n;
    Pos *p;

    n = HASH_SIZE;
    p = &s->head[n];
    slide_hash_power8_loop(s,n,p);

    n = s->w_size;
    p = &s->prev[n];
    slide_hash_power8_loop(s,n,p);
}

#endif /* POWER8_VSX_SLIDEHASH */

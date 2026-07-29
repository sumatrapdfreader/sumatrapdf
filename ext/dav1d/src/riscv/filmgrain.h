/*
 * Copyright © 2026, VideoLAN and dav1d authors
 * Copyright © 2026, Mohd Zaid
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DAV1D_SRC_RISCV_64_FILMGRAIN_H
#define DAV1D_SRC_RISCV_64_FILMGRAIN_H

#include "src/cpu.h"
#include "src/filmgrain.h"

decl_generate_grain_y_fn(BF(dav1d_generate_grain_y, rvv));

static ALWAYS_INLINE void film_grain_dsp_init_riscv(Dav1dFilmGrainDSPContext *const c){
    const unsigned flags = dav1d_get_cpu_flags();

    if (!(flags & DAV1D_RISCV_CPU_FLAG_V)) return;

#if BITDEPTH == 8
    c->generate_grain_y = dav1d_generate_grain_y_8bpc_rvv;
#elif BITDEPTH == 16
    c->generate_grain_y = dav1d_generate_grain_y_16bpc_rvv;
#endif
}

#endif /* DAV1D_SRC_RISCV_FILMGRAIN_H */

/* SumatraPDF: avx2-only subset of lf_mask.h (no SSE2 asm). */
#ifndef DAV1D_SRC_X86_LF_MASK_H
#define DAV1D_SRC_X86_LF_MASK_H

#include "src/cpu.h"

#if ARCH_X86_64
void dav1d_fill_levelcache_block_avx2(uint8_t (*level_cache_ptr)[2], ptrdiff_t b_stride,
                                      int bw, int bh, unsigned v0, unsigned v1);

static ALWAYS_INLINE void lf_mask_dsp_init_x86(Dav1dLfMaskDSPContext *const s) {
    const unsigned flags = dav1d_get_cpu_flags();

    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2)) return;
    s->fill_levelcache_block = dav1d_fill_levelcache_block_avx2;
}
#endif

#endif /* DAV1D_SRC_X86_LF_MASK_H */

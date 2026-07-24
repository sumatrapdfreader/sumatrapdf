/* SumatraPDF: avx2-only subset of pal.h (no SSE/AVX-512 asm). */
#include "src/cpu.h"

decl_pal_idx_finish_fn(dav1d_pal_idx_finish_avx2);

static ALWAYS_INLINE void pal_dsp_init_x86(Dav1dPalDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

#if ARCH_X86_64
    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2)) return;

    c->pal_idx_finish = dav1d_pal_idx_finish_avx2;
#else
    (void)flags;
    (void)c;
#endif
}

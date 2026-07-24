/* SumatraPDF: avx2-only subset of msac.h.
 * Non-avx2 entry points stay on the C implementations (no SSE2 asm macros). */
#ifndef DAV1D_SRC_X86_MSAC_H
#define DAV1D_SRC_X86_MSAC_H

#include "src/cpu.h"

unsigned dav1d_msac_decode_symbol_adapt16_avx2(MsacContext *s, uint16_t *cdf,
                                               size_t n_symbols);

#if ARCH_X86_64
#define dav1d_msac_decode_symbol_adapt16(ctx, cdf, symb) ((ctx)->symbol_adapt16(ctx, cdf, symb))

static ALWAYS_INLINE void msac_init_x86(MsacContext *const s) {
    const unsigned flags = dav1d_get_cpu_flags();

    if (flags & DAV1D_X86_CPU_FLAG_AVX2) {
        s->symbol_adapt16 = dav1d_msac_decode_symbol_adapt16_avx2;
    }
}
#endif

#endif /* DAV1D_SRC_X86_MSAC_H */

/* SumatraPDF: avx2-only subset of refmvs.h (no SSE/AVX-512 asm).
 * load_tmvs has no avx2 implementation upstream; leave C. */
#include "src/cpu.h"
#include "src/refmvs.h"

decl_save_tmvs_fn(dav1d_save_tmvs_avx2);
decl_splat_mv_fn(dav1d_splat_mv_avx2);

static ALWAYS_INLINE void refmvs_dsp_init_x86(Dav1dRefmvsDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

#if ARCH_X86_64
    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2)) return;

    c->save_tmvs = dav1d_save_tmvs_avx2;
    c->splat_mv = dav1d_splat_mv_avx2;
#else
    (void)flags;
    (void)c;
#endif
}

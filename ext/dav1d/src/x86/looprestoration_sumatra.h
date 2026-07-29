/* SumatraPDF: avx2-only subset of looprestoration.h (no SSE/AVX-512 asm). */
#include "src/cpu.h"
#include "src/looprestoration.h"

#include "common/intops.h"

#define decl_wiener_filter_fns(ext) \
decl_lr_filter_fn(BF(dav1d_wiener_filter7, ext)); \
decl_lr_filter_fn(BF(dav1d_wiener_filter5, ext))

#define decl_sgr_filter_fns(ext) \
decl_lr_filter_fn(BF(dav1d_sgr_filter_5x5, ext)); \
decl_lr_filter_fn(BF(dav1d_sgr_filter_3x3, ext)); \
decl_lr_filter_fn(BF(dav1d_sgr_filter_mix, ext))

decl_wiener_filter_fns(avx2);
decl_sgr_filter_fns(avx2);

static ALWAYS_INLINE void loop_restoration_dsp_init_x86(Dav1dLoopRestorationDSPContext *const c, const int bpc) {
    const unsigned flags = dav1d_get_cpu_flags();

#if ARCH_X86_64
    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2)) return;

    c->wiener[0] = BF(dav1d_wiener_filter7, avx2);
    c->wiener[1] = BF(dav1d_wiener_filter5, avx2);
    if (BITDEPTH == 8 || bpc == 10) {
        c->sgr[0] = BF(dav1d_sgr_filter_5x5, avx2);
        c->sgr[1] = BF(dav1d_sgr_filter_3x3, avx2);
        c->sgr[2] = BF(dav1d_sgr_filter_mix, avx2);
    }
#else
    (void)flags;
    (void)c;
    (void)bpc;
#endif
}

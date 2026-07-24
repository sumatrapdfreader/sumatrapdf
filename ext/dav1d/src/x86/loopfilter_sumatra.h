/* SumatraPDF: avx2-only subset of loopfilter.h (no SSE/AVX-512 asm). */
#include "src/cpu.h"
#include "src/loopfilter.h"

#define decl_loopfilter_sb_fns(ext) \
decl_loopfilter_sb_fn(BF(dav1d_lpf_h_sb_y, ext)); \
decl_loopfilter_sb_fn(BF(dav1d_lpf_v_sb_y, ext)); \
decl_loopfilter_sb_fn(BF(dav1d_lpf_h_sb_uv, ext)); \
decl_loopfilter_sb_fn(BF(dav1d_lpf_v_sb_uv, ext))

decl_loopfilter_sb_fns(avx2);

static ALWAYS_INLINE void loop_filter_dsp_init_x86(Dav1dLoopFilterDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

#if ARCH_X86_64
    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2)) return;

    c->loop_filter_sb[0][0] = BF(dav1d_lpf_h_sb_y, avx2);
    c->loop_filter_sb[0][1] = BF(dav1d_lpf_v_sb_y, avx2);
    c->loop_filter_sb[1][0] = BF(dav1d_lpf_h_sb_uv, avx2);
    c->loop_filter_sb[1][1] = BF(dav1d_lpf_v_sb_uv, avx2);
#else
    (void)flags;
    (void)c;
#endif
}

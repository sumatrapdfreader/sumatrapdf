/* SumatraPDF: avx2-only subset of filmgrain.h (no SSE/AVX-512 asm). */
#include "src/cpu.h"
#include "src/filmgrain.h"

#define decl_fg_fns(ext)                                         \
decl_generate_grain_y_fn(BF(dav1d_generate_grain_y, ext));       \
decl_generate_grain_uv_fn(BF(dav1d_generate_grain_uv_420, ext)); \
decl_generate_grain_uv_fn(BF(dav1d_generate_grain_uv_422, ext)); \
decl_generate_grain_uv_fn(BF(dav1d_generate_grain_uv_444, ext)); \
decl_fgy_32x32xn_fn(BF(dav1d_fgy_32x32xn, ext));                 \
decl_fguv_32x32xn_fn(BF(dav1d_fguv_32x32xn_i420, ext));          \
decl_fguv_32x32xn_fn(BF(dav1d_fguv_32x32xn_i422, ext));          \
decl_fguv_32x32xn_fn(BF(dav1d_fguv_32x32xn_i444, ext))

decl_fg_fns(avx2);

static ALWAYS_INLINE void film_grain_dsp_init_x86(Dav1dFilmGrainDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

#if ARCH_X86_64
    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2)) return;

    c->generate_grain_y = BF(dav1d_generate_grain_y, avx2);
    c->generate_grain_uv[DAV1D_PIXEL_LAYOUT_I420 - 1] = BF(dav1d_generate_grain_uv_420, avx2);
    c->generate_grain_uv[DAV1D_PIXEL_LAYOUT_I422 - 1] = BF(dav1d_generate_grain_uv_422, avx2);
    c->generate_grain_uv[DAV1D_PIXEL_LAYOUT_I444 - 1] = BF(dav1d_generate_grain_uv_444, avx2);

    if (!(flags & DAV1D_X86_CPU_FLAG_SLOW_GATHER)) {
        c->fgy_32x32xn = BF(dav1d_fgy_32x32xn, avx2);
        c->fguv_32x32xn[DAV1D_PIXEL_LAYOUT_I420 - 1] = BF(dav1d_fguv_32x32xn_i420, avx2);
        c->fguv_32x32xn[DAV1D_PIXEL_LAYOUT_I422 - 1] = BF(dav1d_fguv_32x32xn_i422, avx2);
        c->fguv_32x32xn[DAV1D_PIXEL_LAYOUT_I444 - 1] = BF(dav1d_fguv_32x32xn_i444, avx2);
    }
#else
    (void)flags;
    (void)c;
#endif
}

/* SumatraPDF: avx2-only subset of mc.h (no SSE/AVX-512 asm). */
#include "src/cpu.h"
#include "src/mc.h"

#define decl_fn(type, name) \
    decl_##type##_fn(BF(name, avx2));
#define init_mc_fn(type, name, suffix) \
    c->mc[type] = BF(dav1d_put_##name, suffix)
#define init_mct_fn(type, name, suffix) \
    c->mct[type] = BF(dav1d_prep_##name, suffix)
#define init_mc_scaled_fn(type, name, suffix) \
    c->mc_scaled[type] = BF(dav1d_put_##name, suffix)
#define init_mct_scaled_fn(type, name, suffix) \
    c->mct_scaled[type] = BF(dav1d_prep_##name, suffix)

decl_8tap_fns(avx2);

decl_fn(mc, dav1d_put_bilin);
decl_fn(mct, dav1d_prep_bilin);

decl_fn(mc_scaled, dav1d_put_8tap_scaled_regular);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_regular_smooth);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_regular_sharp);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_smooth);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_smooth_regular);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_smooth_sharp);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_sharp);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_sharp_regular);
decl_fn(mc_scaled, dav1d_put_8tap_scaled_sharp_smooth);
decl_fn(mc_scaled, dav1d_put_bilin_scaled);

decl_fn(mct_scaled, dav1d_prep_8tap_scaled_regular);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_regular_smooth);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_regular_sharp);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_smooth);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_smooth_regular);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_smooth_sharp);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_sharp);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_sharp_regular);
decl_fn(mct_scaled, dav1d_prep_8tap_scaled_sharp_smooth);
decl_fn(mct_scaled, dav1d_prep_bilin_scaled);

decl_fn(avg, dav1d_avg);
decl_fn(w_avg, dav1d_w_avg);
decl_fn(mask, dav1d_mask);
decl_fn(w_mask, dav1d_w_mask_420);
decl_fn(w_mask, dav1d_w_mask_422);
decl_fn(w_mask, dav1d_w_mask_444);
decl_fn(blend, dav1d_blend);
decl_fn(blend_dir, dav1d_blend_v);
decl_fn(blend_dir, dav1d_blend_h);

decl_fn(warp8x8, dav1d_warp_affine_8x8);
decl_fn(warp8x8t, dav1d_warp_affine_8x8t);

decl_fn(emu_edge, dav1d_emu_edge);

decl_fn(resize, dav1d_resize);

static ALWAYS_INLINE void mc_dsp_init_x86(Dav1dMCDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

#if ARCH_X86_64
    if (!(flags & DAV1D_X86_CPU_FLAG_AVX2))
        return;

    init_8tap_fns(avx2);

    init_mc_fn(FILTER_2D_BILINEAR,            bilin,               avx2);
    init_mct_fn(FILTER_2D_BILINEAR,           bilin,               avx2);

    init_mc_scaled_fn(FILTER_2D_8TAP_REGULAR,        8tap_scaled_regular,        avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_scaled_regular_smooth, avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_REGULAR_SHARP,  8tap_scaled_regular_sharp,  avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_scaled_smooth_regular, avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_SMOOTH,         8tap_scaled_smooth,         avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_scaled_smooth_sharp,   avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_SHARP_REGULAR,  8tap_scaled_sharp_regular,  avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_scaled_sharp_smooth,   avx2);
    init_mc_scaled_fn(FILTER_2D_8TAP_SHARP,          8tap_scaled_sharp,          avx2);
    init_mc_scaled_fn(FILTER_2D_BILINEAR,            bilin_scaled,               avx2);

    init_mct_scaled_fn(FILTER_2D_8TAP_REGULAR,        8tap_scaled_regular,        avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_scaled_regular_smooth, avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_REGULAR_SHARP,  8tap_scaled_regular_sharp,  avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_scaled_smooth_regular, avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_SMOOTH,         8tap_scaled_smooth,         avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_scaled_smooth_sharp,   avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_SHARP_REGULAR,  8tap_scaled_sharp_regular,  avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_scaled_sharp_smooth,   avx2);
    init_mct_scaled_fn(FILTER_2D_8TAP_SHARP,          8tap_scaled_sharp,          avx2);
    init_mct_scaled_fn(FILTER_2D_BILINEAR,            bilin_scaled,               avx2);

    c->avg = BF(dav1d_avg, avx2);
    c->w_avg = BF(dav1d_w_avg, avx2);
    c->mask = BF(dav1d_mask, avx2);
    c->w_mask[0] = BF(dav1d_w_mask_444, avx2);
    c->w_mask[1] = BF(dav1d_w_mask_422, avx2);
    c->w_mask[2] = BF(dav1d_w_mask_420, avx2);
    c->blend = BF(dav1d_blend, avx2);
    c->blend_v = BF(dav1d_blend_v, avx2);
    c->blend_h = BF(dav1d_blend_h, avx2);
    c->warp8x8  = BF(dav1d_warp_affine_8x8, avx2);
    c->warp8x8t = BF(dav1d_warp_affine_8x8t, avx2);
    c->emu_edge = BF(dav1d_emu_edge, avx2);
    c->resize = BF(dav1d_resize, avx2);
#else
    (void)flags;
    (void)c;
#endif
}

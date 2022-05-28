/*
 * Copyright © 2019, VideoLAN and dav1d authors
 * Copyright © 2019, Michail Alvanos
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

#include "common/intops.h"
#include "src/ppc/dav1d_types.h"
#include "src/cpu.h"
#include "src/looprestoration.h"

#if BITDEPTH == 8

#define REST_UNIT_STRIDE (400)

static inline i32x4 iclip_vec(i32x4 v, const i32x4 minv, const i32x4 maxv) {
    v = vec_max(minv, v);
    v = vec_min(maxv, v);
    return v;
}

#define APPLY_FILTER_H(v, f, ssum1, ssum2) do {  \
    i16x8 ktmp_u16_high = (i16x8) u8h_to_u16(v); \
    i16x8 ktmp_u16_low  = (i16x8) u8l_to_u16(v); \
    ssum1 = vec_madd(ktmp_u16_high, f, ssum1);   \
    ssum2 = vec_madd(ktmp_u16_low, f, ssum2);    \
} while (0)

static void wiener_filter_h_vsx(int32_t *hor_ptr,
                                uint8_t *tmp_ptr,
                                const int16_t filterh[8],
                                const int w, const int h)
{
    static const i32x4 zerov = vec_splats(0);
    static const i32x4 seven_vec = vec_splats(7);
    static const i32x4 bitdepth_added_vec = vec_splats(1 << 14);
    static const i32x4 round_bits_vec = vec_splats(3);
    static const i32x4 rounding_off_vec = vec_splats(1<<2);
    static const i32x4 clip_limit_v = vec_splats((1 << 13) - 1);

    i16x8 filterhvall = vec_vsx_ld(0, filterh);
    i16x8 filterhv0 =  vec_splat( filterhvall, 0);
    i16x8 filterhv1 =  vec_splat( filterhvall, 1);
    i16x8 filterhv2 =  vec_splat( filterhvall, 2);
    i16x8 filterhv3 =  vec_splat( filterhvall, 3);
    i16x8 filterhv4 =  vec_splat( filterhvall, 4);
    i16x8 filterhv5 =  vec_splat( filterhvall, 5);
    i16x8 filterhv6 =  vec_splat( filterhvall, 6);

    for (int j = 0; j < h + 6; j++) {
        for (int i = 0; i < w; i+=16) {
            i32x4 sum1 = bitdepth_added_vec;
            i32x4 sum2 = bitdepth_added_vec;
            i32x4 sum3 = bitdepth_added_vec;
            i32x4 sum4 = bitdepth_added_vec;

            u8x16 tmp_v0 = vec_ld(0, &tmp_ptr[i]);
            u8x16 tmp_v7 = vec_ld(0, &tmp_ptr[i+16]);

            u8x16 tmp_v1 = vec_sld( tmp_v7, tmp_v0, 15);
            u8x16 tmp_v2 = vec_sld( tmp_v7, tmp_v0, 14);
            u8x16 tmp_v3 = vec_sld( tmp_v7, tmp_v0, 13);
            u8x16 tmp_v4 = vec_sld( tmp_v7, tmp_v0, 12);
            u8x16 tmp_v5 = vec_sld( tmp_v7, tmp_v0, 11);
            u8x16 tmp_v6 = vec_sld( tmp_v7, tmp_v0, 10);

            u16x8 tmp_u16_high = u8h_to_u16(tmp_v3);
            u16x8 tmp_u16_low  = u8l_to_u16(tmp_v3);

            i32x4 tmp_expanded1 = i16h_to_i32(tmp_u16_high);
            i32x4 tmp_expanded2 = i16l_to_i32(tmp_u16_high);
            i32x4 tmp_expanded3 = i16h_to_i32(tmp_u16_low);
            i32x4 tmp_expanded4 = i16l_to_i32(tmp_u16_low);

            i16x8 ssum1 = (i16x8) zerov;
            i16x8 ssum2 = (i16x8) zerov;

            APPLY_FILTER_H(tmp_v0, filterhv0, ssum1, ssum2);
            APPLY_FILTER_H(tmp_v1, filterhv1, ssum1, ssum2);
            APPLY_FILTER_H(tmp_v2, filterhv2, ssum1, ssum2);
            APPLY_FILTER_H(tmp_v3, filterhv3, ssum1, ssum2);
            APPLY_FILTER_H(tmp_v4, filterhv4, ssum1, ssum2);
            APPLY_FILTER_H(tmp_v5, filterhv5, ssum1, ssum2);
            APPLY_FILTER_H(tmp_v6, filterhv6, ssum1, ssum2);

            sum1 += i16h_to_i32(ssum1) + (tmp_expanded1 << seven_vec);
            sum2 += i16l_to_i32(ssum1) + (tmp_expanded2 << seven_vec);
            sum3 += i16h_to_i32(ssum2) + (tmp_expanded3 << seven_vec);
            sum4 += i16l_to_i32(ssum2) + (tmp_expanded4 << seven_vec);

            sum1 = (sum1 + rounding_off_vec) >> round_bits_vec;
            sum2 = (sum2 + rounding_off_vec) >> round_bits_vec;
            sum3 = (sum3 + rounding_off_vec) >> round_bits_vec;
            sum4 = (sum4 + rounding_off_vec) >> round_bits_vec;

            sum1 = iclip_vec(sum1, zerov, clip_limit_v);
            sum2 = iclip_vec(sum2, zerov, clip_limit_v);
            sum3 = iclip_vec(sum3, zerov, clip_limit_v);
            sum4 = iclip_vec(sum4, zerov, clip_limit_v);

            vec_st(sum1,  0, &hor_ptr[i]);
            vec_st(sum2, 16, &hor_ptr[i]);
            vec_st(sum3, 32, &hor_ptr[i]);
            vec_st(sum4, 48, &hor_ptr[i]);
        }
        tmp_ptr += REST_UNIT_STRIDE;
        hor_ptr += REST_UNIT_STRIDE;
    }
}

static inline i16x8 iclip_u8_vec(i16x8 v) {
    static const i16x8 zerov = vec_splats((int16_t)0);
    static const i16x8 maxv = vec_splats((int16_t)255);
    v = vec_max(zerov, v);
    v = vec_min(maxv, v);
    return v;
}

#define APPLY_FILTER_V(index, f) do { \
    i32x4 v1 = vec_ld( 0, &hor[(j + index) * REST_UNIT_STRIDE + i]); \
    i32x4 v2 = vec_ld(16, &hor[(j + index) * REST_UNIT_STRIDE + i]); \
    i32x4 v3 = vec_ld(32, &hor[(j + index) * REST_UNIT_STRIDE + i]); \
    i32x4 v4 = vec_ld(48, &hor[(j + index) * REST_UNIT_STRIDE + i]); \
    sum1 = sum1 + v1 * f; \
    sum2 = sum2 + v2 * f; \
    sum3 = sum3 + v3 * f; \
    sum4 = sum4 + v4 * f; \
} while (0)

#define LOAD_AND_APPLY_FILTER_V(sumpixelv, hor) do { \
    i32x4 sum1 = round_vec; \
    i32x4 sum2 = round_vec; \
    i32x4 sum3 = round_vec; \
    i32x4 sum4 = round_vec; \
    APPLY_FILTER_V(0, filterv0); \
    APPLY_FILTER_V(1, filterv1); \
    APPLY_FILTER_V(2, filterv2); \
    APPLY_FILTER_V(3, filterv3); \
    APPLY_FILTER_V(4, filterv4); \
    APPLY_FILTER_V(5, filterv5); \
    APPLY_FILTER_V(6, filterv6); \
    sum1 = sum1 >> round_bits_vec; \
    sum2 = sum2 >> round_bits_vec; \
    sum3 = sum3 >> round_bits_vec; \
    sum4 = sum4 >> round_bits_vec; \
    i16x8 sum_short_packed_1 = (i16x8) vec_pack(sum1, sum2); \
    i16x8 sum_short_packed_2 = (i16x8) vec_pack(sum3, sum4); \
    sum_short_packed_1 = iclip_u8_vec(sum_short_packed_1); \
    sum_short_packed_2 = iclip_u8_vec(sum_short_packed_2); \
    sum_pixel = (u8x16) vec_pack(sum_short_packed_1, sum_short_packed_2); \
} while (0)

static inline void wiener_filter_v_vsx(uint8_t *p,
                                       const ptrdiff_t stride,
                                       const int32_t *hor,
                                       const int16_t filterv[8],
                                       const int w, const int h)
{
    static const i32x4 round_bits_vec = vec_splats(11);
    static const i32x4 round_vec = vec_splats((1 << 10) - (1 << 18));

    i32x4 filterv0 =  vec_splats((int32_t) filterv[0]);
    i32x4 filterv1 =  vec_splats((int32_t) filterv[1]);
    i32x4 filterv2 =  vec_splats((int32_t) filterv[2]);
    i32x4 filterv3 =  vec_splats((int32_t) filterv[3]);
    i32x4 filterv4 =  vec_splats((int32_t) filterv[4]);
    i32x4 filterv5 =  vec_splats((int32_t) filterv[5]);
    i32x4 filterv6 =  vec_splats((int32_t) filterv[6]);

    for (int j = 0; j < h; j++) {
        for (int i = 0; i <(w-w%16); i += 16) {
            u8x16 sum_pixel;
            LOAD_AND_APPLY_FILTER_V(sum_pixel, hor);
            vec_vsx_st(sum_pixel, 0, &p[j * PXSTRIDE(stride) + i]);
        }
        // remaining loop
        if (w & 0xf){
            int i=w-w%16;
            ALIGN_STK_16(uint8_t, tmp_out, 16,);
            u8x16 sum_pixel;

            LOAD_AND_APPLY_FILTER_V(sum_pixel, hor);
            vec_vsx_st(sum_pixel, 0, tmp_out);

            for (int k=0; i<w; i++, k++) {
                p[j * PXSTRIDE(stride) + i] = tmp_out[k];
            }
        }
    }
}

static inline void padding(uint8_t *dst, const uint8_t *p,
                           const ptrdiff_t stride, const uint8_t (*left)[4],
                           const uint8_t *lpf, int unit_w, const int stripe_h,
                           const enum LrEdgeFlags edges)
{
    const int have_left = !!(edges & LR_HAVE_LEFT);
    const int have_right = !!(edges & LR_HAVE_RIGHT);

    // Copy more pixels if we don't have to pad them
    unit_w += 3 * have_left + 3 * have_right;
    uint8_t *dst_l = dst + 3 * !have_left;
    p -= 3 * have_left;
    lpf -= 3 * have_left;

    if (edges & LR_HAVE_TOP) {
        // Copy previous loop filtered rows
        const uint8_t *const above_1 = lpf;
        const uint8_t *const above_2 = above_1 + PXSTRIDE(stride);
        pixel_copy(dst_l, above_1, unit_w);
        pixel_copy(dst_l + REST_UNIT_STRIDE, above_1, unit_w);
        pixel_copy(dst_l + 2 * REST_UNIT_STRIDE, above_2, unit_w);
    } else {
        // Pad with first row
        pixel_copy(dst_l, p, unit_w);
        pixel_copy(dst_l + REST_UNIT_STRIDE, p, unit_w);
        pixel_copy(dst_l + 2 * REST_UNIT_STRIDE, p, unit_w);
        if (have_left) {
            pixel_copy(dst_l, &left[0][1], 3);
            pixel_copy(dst_l + REST_UNIT_STRIDE, &left[0][1], 3);
            pixel_copy(dst_l + 2 * REST_UNIT_STRIDE, &left[0][1], 3);
        }
    }

    uint8_t *dst_tl = dst_l + 3 * REST_UNIT_STRIDE;
    if (edges & LR_HAVE_BOTTOM) {
        // Copy next loop filtered rows
        const uint8_t *const below_1 = lpf + 6 * PXSTRIDE(stride);
        const uint8_t *const below_2 = below_1 + PXSTRIDE(stride);
        pixel_copy(dst_tl + stripe_h * REST_UNIT_STRIDE, below_1, unit_w);
        pixel_copy(dst_tl + (stripe_h + 1) * REST_UNIT_STRIDE, below_2, unit_w);
        pixel_copy(dst_tl + (stripe_h + 2) * REST_UNIT_STRIDE, below_2, unit_w);
    } else {
        // Pad with last row
        const uint8_t *const src = p + (stripe_h - 1) * PXSTRIDE(stride);
        pixel_copy(dst_tl + stripe_h * REST_UNIT_STRIDE, src, unit_w);
        pixel_copy(dst_tl + (stripe_h + 1) * REST_UNIT_STRIDE, src, unit_w);
        pixel_copy(dst_tl + (stripe_h + 2) * REST_UNIT_STRIDE, src, unit_w);
        if (have_left) {
            pixel_copy(dst_tl + stripe_h * REST_UNIT_STRIDE, &left[stripe_h - 1][1], 3);
            pixel_copy(dst_tl + (stripe_h + 1) * REST_UNIT_STRIDE, &left[stripe_h - 1][1], 3);
            pixel_copy(dst_tl + (stripe_h + 2) * REST_UNIT_STRIDE, &left[stripe_h - 1][1], 3);
        }
    }

    // Inner UNIT_WxSTRIPE_H
    for (int j = 0; j < stripe_h; j++) {
        pixel_copy(dst_tl + 3 * have_left, p + 3 * have_left, unit_w - 3 * have_left);
        dst_tl += REST_UNIT_STRIDE;
        p += PXSTRIDE(stride);
    }

    if (!have_right) {
        uint8_t *pad = dst_l + unit_w;
        uint8_t *row_last = &dst_l[unit_w - 1];
        // Pad 3x(STRIPE_H+6) with last column
        for (int j = 0; j < stripe_h + 6; j++) {
            pixel_set(pad, *row_last, 3);
            pad += REST_UNIT_STRIDE;
            row_last += REST_UNIT_STRIDE;
        }
    }

    if (!have_left) {
        // Pad 3x(STRIPE_H+6) with first column
        for (int j = 0; j < stripe_h + 6; j++) {
            pixel_set(dst, *dst_l, 3);
            dst += REST_UNIT_STRIDE;
            dst_l += REST_UNIT_STRIDE;
        }
    } else {
        dst += 3 * REST_UNIT_STRIDE;
        for (int j = 0; j < stripe_h; j++) {
            pixel_copy(dst, &left[j][1], 3);
            dst += REST_UNIT_STRIDE;
        }
    }
}

// FIXME Could split into luma and chroma specific functions,
// (since first and last tops are always 0 for chroma)
// FIXME Could implement a version that requires less temporary memory
// (should be possible to implement with only 6 rows of temp storage)
static void wiener_filter_vsx(uint8_t *p, const ptrdiff_t stride,
                              const uint8_t (*const left)[4],
                              const uint8_t *lpf,
                              const int w, const int h,
                              const LooprestorationParams *const params,
                              const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX)
{
    const int16_t (*const filter)[8] = params->filter;

    // Wiener filtering is applied to a maximum stripe height of 64 + 3 pixels
    // of padding above and below
    ALIGN_STK_16(uint8_t, tmp, 70 /*(64 + 3 + 3)*/ * REST_UNIT_STRIDE,);
    padding(tmp, p, stride, left, lpf, w, h, edges);
    ALIGN_STK_16(int32_t, hor, 70 /*(64 + 3 + 3)*/ * REST_UNIT_STRIDE + 64,);

    wiener_filter_h_vsx(hor, tmp, filter[0], w, h);
    wiener_filter_v_vsx(p, stride, hor, filter[1], w, h);
}
#endif

COLD void bitfn(dav1d_loop_restoration_dsp_init_ppc)(Dav1dLoopRestorationDSPContext *const c,
                                                     const int bpc)
{
    const unsigned flags = dav1d_get_cpu_flags();

    if (!(flags & DAV1D_PPC_CPU_FLAG_VSX)) return;

#if BITDEPTH == 8
    c->wiener[0] = c->wiener[1] = wiener_filter_vsx;
#endif
}



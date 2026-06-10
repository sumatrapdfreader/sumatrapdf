/*
 * Copyright © 2019, Luca Barbato
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

#include "src/ppc/dav1d_types.h"
#include "src/ppc/cdef.h"

#if BITDEPTH == 8
static inline i16x8 vconstrain(const i16x8 diff, const int16_t threshold,
                               const uint16_t shift)
{
    const i16x8 zero = vec_splat_s16(0);
    if (!threshold) return zero;
    const i16x8 abs_diff = vec_abs(diff);
    const b16x8 mask = vec_cmplt(diff, zero);
    const i16x8 thr = vec_splats(threshold);
    const i16x8 sub = vec_sub(thr, vec_sra(abs_diff, vec_splats(shift)));
    const i16x8 max = vec_max(zero, sub);
    const i16x8 min = vec_min(abs_diff, max);
    const i16x8 neg = vec_sub(zero, min);
    return vec_sel(min, neg, mask);
}

static inline void copy4xN(uint16_t *tmp,
                           const uint8_t *src, const ptrdiff_t src_stride,
                           const uint8_t (*left)[2], const uint8_t *const top,
                           const uint8_t *const bottom, const int w, const int h,
                           const enum CdefEdgeFlags edges)
{
    const u16x8 fill = vec_splats((uint16_t)INT16_MAX);

    u16x8 l0;
    u16x8 l1;

    int y_start = -2, y_end = h + 2;

    // Copy top and bottom first
    if (!(edges & CDEF_HAVE_TOP)) {
        l0 = fill;
        l1 = fill;
        y_start = 0;
    } else {
        l0 = u8h_to_u16(vec_vsx_ld(0, top + 0 * src_stride - 2));
        l1 = u8h_to_u16(vec_vsx_ld(0, top + 1 * src_stride - 2));
    }

    vec_st(l0, 0, tmp - 2 * 8);
    vec_st(l1, 0, tmp - 1 * 8);

    if (!(edges & CDEF_HAVE_BOTTOM)) {
        l0 = fill;
        l1 = fill;
        y_end -= 2;
    } else {
        l0 = u8h_to_u16(vec_vsx_ld(0, bottom + 0 * src_stride - 2));
        l1 = u8h_to_u16(vec_vsx_ld(0, bottom + 1 * src_stride - 2));
    }

    vec_st(l0, 0, tmp + (h + 0) * 8);
    vec_st(l1, 0, tmp + (h + 1) * 8);

    int y_with_left_edge = 0;
    if (!(edges & CDEF_HAVE_LEFT)) {
        u16x8 l = u8h_to_u16(vec_vsx_ld(0, src));
        vec_vsx_st(l, 0, tmp + 2);

        y_with_left_edge = 1;
    }

    for (int y = y_with_left_edge; y < h; y++) {
        u16x8 l = u8h_to_u16(vec_vsx_ld(0, src - 2 + y * src_stride));
        vec_st(l, 0, tmp + y * 8);
    }

    if (!(edges & CDEF_HAVE_LEFT)) {
        for (int y = y_start; y < y_end; y++) {
            tmp[y * 8] = INT16_MAX;
            tmp[1 + y * 8] = INT16_MAX;
        }
    } else {
        for (int y = 0; y < h; y++) {
            tmp[y * 8] = left[y][0];
            tmp[1 + y * 8] = left[y][1];
        }
    }
    if (!(edges & CDEF_HAVE_RIGHT)) {
        for (int y = y_start; y < y_end; y++) {
            tmp[- 2 + (y + 1) * 8] = INT16_MAX;
            tmp[- 1 + (y + 1) * 8] = INT16_MAX;
        }
    }
}

static inline void copy8xN(uint16_t *tmp,
                           const uint8_t *src, const ptrdiff_t src_stride,
                           const uint8_t (*left)[2], const uint8_t *const top,
                           const uint8_t *const bottom, const int w, const int h,
                           const enum CdefEdgeFlags edges)
{
    const u16x8 fill = vec_splats((uint16_t)INT16_MAX);

    u16x8 l0h, l0l;
    u16x8 l1h, l1l;

    int y_start = -2, y_end = h + 2;

    // Copy top and bottom first
    if (!(edges & CDEF_HAVE_TOP)) {
        l0h = fill;
        l0l = fill;
        l1h = fill;
        l1l = fill;
        y_start = 0;
    } else {
        u8x16 l0 = vec_vsx_ld(0, top + 0 * src_stride - 2);
        u8x16 l1 = vec_vsx_ld(0, top + 1 * src_stride - 2);
        l0h = u8h_to_u16(l0);
        l0l = u8l_to_u16(l0);
        l1h = u8h_to_u16(l1);
        l1l = u8l_to_u16(l1);
    }

    vec_st(l0h, 0, tmp - 4 * 8);
    vec_st(l0l, 0, tmp - 3 * 8);
    vec_st(l1h, 0, tmp - 2 * 8);
    vec_st(l1l, 0, tmp - 1 * 8);

    if (!(edges & CDEF_HAVE_BOTTOM)) {
        l0h = fill;
        l0l = fill;
        l1h = fill;
        l1l = fill;
        y_end -= 2;
    } else {
        u8x16 l0 = vec_vsx_ld(0, bottom + 0 * src_stride - 2);
        u8x16 l1 = vec_vsx_ld(0, bottom + 1 * src_stride - 2);
        l0h = u8h_to_u16(l0);
        l0l = u8l_to_u16(l0);
        l1h = u8h_to_u16(l1);
        l1l = u8l_to_u16(l1);
    }

    vec_st(l0h, 0, tmp + (h + 0) * 16);
    vec_st(l0l, 0, tmp + (h + 0) * 16 + 8);
    vec_st(l1h, 0, tmp + (h + 1) * 16);
    vec_st(l1l, 0, tmp + (h + 1) * 16 + 8);

    int y_with_left_edge = 0;
    if (!(edges & CDEF_HAVE_LEFT)) {
        u8x16 l = vec_vsx_ld(0, src);
        u16x8 lh = u8h_to_u16(l);
        u16x8 ll = u8l_to_u16(l);
        vec_vsx_st(lh, 0, tmp + 2);
        vec_vsx_st(ll, 0, tmp + 8 + 2);

        y_with_left_edge = 1;
    }

    for (int y = y_with_left_edge; y < h; y++) {
        u8x16 l = vec_vsx_ld(0, src - 2 + y * src_stride);
        u16x8 lh = u8h_to_u16(l);
        u16x8 ll = u8l_to_u16(l);
        vec_st(lh, 0, tmp + y * 16);
        vec_st(ll, 0, tmp + 8 + y * 16);
    }

    if (!(edges & CDEF_HAVE_LEFT)) {
        for (int y = y_start; y < y_end; y++) {
            tmp[y * 16] = INT16_MAX;
            tmp[1 + y * 16] = INT16_MAX;
        }
    } else {
        for (int y = 0; y < h; y++) {
            tmp[y * 16] = left[y][0];
            tmp[1 + y * 16] = left[y][1];
        }
    }
    if (!(edges & CDEF_HAVE_RIGHT)) {
        for (int y = y_start; y < y_end; y++) {
            tmp[- 6 + (y + 1) * 16] = INT16_MAX;
            tmp[- 5 + (y + 1) * 16] = INT16_MAX;
        }
    }
}

static inline i16x8 max_mask(i16x8 a, i16x8 b) {
    const i16x8 I16X8_INT16_MAX = vec_splats((int16_t)INT16_MAX);

    const b16x8 mask = vec_cmpeq(a, I16X8_INT16_MAX);

    const i16x8 val = vec_sel(a, b, mask);

    return vec_max(val, b);
}

#define LOAD_PIX(addr) \
    const i16x8 px = (i16x8)vec_vsx_ld(0, addr); \
    i16x8 sum = vec_splat_s16(0);

#define LOAD_PIX4(addr) \
    const i16x8 a = (i16x8)vec_vsx_ld(0, addr); \
    const i16x8 b = (i16x8)vec_vsx_ld(0, addr + 8); \
    const i16x8 px = vec_xxpermdi(a, b, 0); \
    i16x8 sum = vec_splat_s16(0);

#define LOAD_DIR(p, addr, o0, o1) \
    const i16x8 p ## 0 = (i16x8)vec_vsx_ld(0, addr + o0); \
    const i16x8 p ## 1 = (i16x8)vec_vsx_ld(0, addr - o0); \
    const i16x8 p ## 2 = (i16x8)vec_vsx_ld(0, addr + o1); \
    const i16x8 p ## 3 = (i16x8)vec_vsx_ld(0, addr - o1);

#define LOAD_DIR4(p, addr, o0, o1) \
    LOAD_DIR(p ## a, addr, o0, o1) \
    LOAD_DIR(p ## b, addr + 8, o0, o1) \
    const i16x8 p ## 0 = vec_xxpermdi(p ## a ## 0, p ## b ## 0, 0); \
    const i16x8 p ## 1 = vec_xxpermdi(p ## a ## 1, p ## b ## 1, 0); \
    const i16x8 p ## 2 = vec_xxpermdi(p ## a ## 2, p ## b ## 2, 0); \
    const i16x8 p ## 3 = vec_xxpermdi(p ## a ## 3, p ## b ## 3, 0);

#define CONSTRAIN(p, strength, shift) \
    const i16x8 p ## _d0 = vec_sub(p ## 0, px); \
    const i16x8 p ## _d1 = vec_sub(p ## 1, px); \
    const i16x8 p ## _d2 = vec_sub(p ## 2, px); \
    const i16x8 p ## _d3 = vec_sub(p ## 3, px); \
\
    i16x8 p ## _c0 = vconstrain(p ## _d0, strength, shift); \
    i16x8 p ## _c1 = vconstrain(p ## _d1, strength, shift); \
    i16x8 p ## _c2 = vconstrain(p ## _d2, strength, shift); \
    i16x8 p ## _c3 = vconstrain(p ## _d3, strength, shift);

#define SETUP_MINMAX \
    i16x8 max = px; \
    i16x8 min = px; \

#define MIN_MAX(p) \
    max = max_mask(p ## 0, max); \
    min = vec_min(p ## 0, min); \
    max = max_mask(p ## 1, max); \
    min = vec_min(p ## 1, min); \
    max = max_mask(p ## 2, max); \
    min = vec_min(p ## 2, min); \
    max = max_mask(p ## 3, max); \
    min = vec_min(p ## 3, min);

#define MAKE_TAPS \
    const int16_t tap_odd = (pri_strength >> bitdepth_min_8) & 1; \
    const i16x8 tap0 = vec_splats((int16_t)(4 - tap_odd)); \
    const i16x8 tap1 = vec_splats((int16_t)(2 + tap_odd));

#define PRI_0_UPDATE_SUM(p) \
    sum = vec_madd(tap0, p ## _c0, sum); \
    sum = vec_madd(tap0, p ## _c1, sum); \
    sum = vec_madd(tap1, p ## _c2, sum); \
    sum = vec_madd(tap1, p ## _c3, sum);

#define UPDATE_SUM(p) \
    const i16x8 p ## sum0 = vec_add(p ## _c0, p ## _c1); \
    const i16x8 p ## sum1 = vec_add(p ## _c2, p ## _c3); \
    sum = vec_add(sum, p ## sum0); \
    sum = vec_add(sum, p ## sum1);

#define SEC_0_UPDATE_SUM(p) \
    sum = vec_madd(vec_splat_s16(2), p ## _c0, sum); \
    sum = vec_madd(vec_splat_s16(2), p ## _c1, sum); \
    sum = vec_madd(vec_splat_s16(2), p ## _c2, sum); \
    sum = vec_madd(vec_splat_s16(2), p ## _c3, sum);

#define BIAS \
    i16x8 bias = vec_and((i16x8)vec_cmplt(sum, vec_splat_s16(0)), vec_splat_s16(1)); \
    bias = vec_sub(vec_splat_s16(8), bias); \

#define STORE4 \
    dst[0] = vdst[0]; \
    dst[1] = vdst[1]; \
    dst[2] = vdst[2]; \
    dst[3] = vdst[3]; \
\
    tmp += 8; \
    dst += PXSTRIDE(dst_stride); \
    dst[0] = vdst[4]; \
    dst[1] = vdst[5]; \
    dst[2] = vdst[6]; \
    dst[3] = vdst[7]; \
\
    tmp += 8; \
    dst += PXSTRIDE(dst_stride);

#define STORE4_CLAMPED \
    BIAS \
    i16x8 unclamped = vec_add(px, vec_sra(vec_add(sum, bias), vec_splat_u16(4))); \
    i16x8 vdst = vec_max(vec_min(unclamped, max), min); \
    STORE4

#define STORE4_UNCLAMPED \
    BIAS \
    i16x8 vdst = vec_add(px, vec_sra(vec_add(sum, bias), vec_splat_u16(4))); \
    STORE4

#define STORE8 \
    dst[0] = vdst[0]; \
    dst[1] = vdst[1]; \
    dst[2] = vdst[2]; \
    dst[3] = vdst[3]; \
    dst[4] = vdst[4]; \
    dst[5] = vdst[5]; \
    dst[6] = vdst[6]; \
    dst[7] = vdst[7]; \
\
    tmp += 16; \
    dst += PXSTRIDE(dst_stride);

#define STORE8_CLAMPED \
    BIAS \
    i16x8 unclamped = vec_add(px, vec_sra(vec_add(sum, bias), vec_splat_u16(4))); \
    i16x8 vdst = vec_max(vec_min(unclamped, max), min); \
    STORE8

#define STORE8_UNCLAMPED \
    BIAS \
    i16x8 vdst = vec_add(px, vec_sra(vec_add(sum, bias), vec_splat_u16(4))); \
    STORE8

#define DIRECTIONS(w, tmp_stride) \
    static const int8_t cdef_directions##w[8 /* dir */][2 /* pass */] = { \
        { -1 * tmp_stride + 1, -2 * tmp_stride + 2 }, \
        {  0 * tmp_stride + 1, -1 * tmp_stride + 2 }, \
        {  0 * tmp_stride + 1,  0 * tmp_stride + 2 }, \
        {  0 * tmp_stride + 1,  1 * tmp_stride + 2 }, \
        {  1 * tmp_stride + 1,  2 * tmp_stride + 2 }, \
        {  1 * tmp_stride + 0,  2 * tmp_stride + 1 }, \
        {  1 * tmp_stride + 0,  2 * tmp_stride + 0 }, \
        {  1 * tmp_stride + 0,  2 * tmp_stride - 1 } \
    };

DIRECTIONS(4, 8)
DIRECTIONS(8, 16)

static inline void
filter_4xN(pixel *dst, const ptrdiff_t dst_stride,
           const pixel (*left)[2], const pixel *const top,
           const pixel *const bottom, const int w, const int h,
           const int pri_strength, const int sec_strength, const int dir,
           const int pri_shift, const int sec_shift,
           const enum CdefEdgeFlags edges, uint16_t *tmp)
{
    const int bitdepth_min_8 = bitdepth_from_max(bitdepth_max) - 8;
    const int off1 = cdef_directions4[dir][0];
    const int off1_1 = cdef_directions4[dir][1];

    const int off2 = cdef_directions4[(dir + 2) & 7][0];
    const int off3 = cdef_directions4[(dir + 6) & 7][0];

    const int off2_1 = cdef_directions4[(dir + 2) & 7][1];
    const int off3_1 = cdef_directions4[(dir + 6) & 7][1];

    MAKE_TAPS

    for (int y = 0; y < h / 2; y++) {
        LOAD_PIX4(tmp)

        SETUP_MINMAX

        // Primary pass
        LOAD_DIR4(p, tmp, off1, off1_1)

        CONSTRAIN(p, pri_strength, pri_shift)

        MIN_MAX(p)

        PRI_0_UPDATE_SUM(p)

        // Secondary pass 1
        LOAD_DIR4(s, tmp, off2, off3)

        CONSTRAIN(s, sec_strength, sec_shift)

        MIN_MAX(s)

        SEC_0_UPDATE_SUM(s)

        // Secondary pass 2
        LOAD_DIR4(s2, tmp, off2_1, off3_1)

        CONSTRAIN(s2, sec_strength, sec_shift)

        MIN_MAX(s2)

        UPDATE_SUM(s2)

        // Store
        STORE4_CLAMPED
    }
}

static inline void
filter_4xN_pri(pixel *dst, const ptrdiff_t dst_stride,
           const pixel (*left)[2], const pixel *const top,
           const pixel *const bottom, const int w, const int h,
           const int pri_strength, const int dir,
           const int pri_shift, const enum CdefEdgeFlags edges,
           uint16_t *tmp)
{
    const int bitdepth_min_8 = bitdepth_from_max(bitdepth_max) - 8;
    const int off1 = cdef_directions4[dir][0];
    const int off1_1 = cdef_directions4[dir][1];

    MAKE_TAPS

    for (int y = 0; y < h / 2; y++) {
        LOAD_PIX4(tmp)

        // Primary pass
        LOAD_DIR4(p, tmp, off1, off1_1)

        CONSTRAIN(p, pri_strength, pri_shift)

        PRI_0_UPDATE_SUM(p)

        STORE4_UNCLAMPED
    }
}

static inline void
filter_4xN_sec(pixel *dst, const ptrdiff_t dst_stride,
           const pixel (*left)[2], const pixel *const top,
           const pixel *const bottom, const int w, const int h,
           const int sec_strength, const int dir,
           const int sec_shift, const enum CdefEdgeFlags edges,
           uint16_t *tmp)
{
    const int off2 = cdef_directions4[(dir + 2) & 7][0];
    const int off3 = cdef_directions4[(dir + 6) & 7][0];

    const int off2_1 = cdef_directions4[(dir + 2) & 7][1];
    const int off3_1 = cdef_directions4[(dir + 6) & 7][1];

    for (int y = 0; y < h / 2; y++) {
        LOAD_PIX4(tmp)
        // Secondary pass 1
        LOAD_DIR4(s, tmp, off2, off3)

        CONSTRAIN(s, sec_strength, sec_shift)

        SEC_0_UPDATE_SUM(s)

        // Secondary pass 2
        LOAD_DIR4(s2, tmp, off2_1, off3_1)

        CONSTRAIN(s2, sec_strength, sec_shift)

        UPDATE_SUM(s2)

        STORE4_UNCLAMPED
    }
}

static inline void
filter_8xN(pixel *dst, const ptrdiff_t dst_stride,
           const pixel (*left)[2], const pixel *const top,
           const pixel *const bottom, const int w, const int h,
           const int pri_strength, const int sec_strength, const int dir,
           const int pri_shift, const int sec_shift, const enum CdefEdgeFlags edges,
           uint16_t *tmp)
{
    const int bitdepth_min_8 = bitdepth_from_max(bitdepth_max) - 8;

    const int off1 = cdef_directions8[dir][0];
    const int off1_1 = cdef_directions8[dir][1];

    const int off2 = cdef_directions8[(dir + 2) & 7][0];
    const int off3 = cdef_directions8[(dir + 6) & 7][0];

    const int off2_1 = cdef_directions8[(dir + 2) & 7][1];
    const int off3_1 = cdef_directions8[(dir + 6) & 7][1];

    MAKE_TAPS

    for (int y = 0; y < h; y++) {
        LOAD_PIX(tmp)

        SETUP_MINMAX

        // Primary pass
        LOAD_DIR(p, tmp, off1, off1_1)

        CONSTRAIN(p, pri_strength, pri_shift)

        MIN_MAX(p)

        PRI_0_UPDATE_SUM(p)

        // Secondary pass 1
        LOAD_DIR(s, tmp, off2, off3)

        CONSTRAIN(s, sec_strength, sec_shift)

        MIN_MAX(s)

        SEC_0_UPDATE_SUM(s)

        // Secondary pass 2
        LOAD_DIR(s2, tmp, off2_1, off3_1)

        CONSTRAIN(s2, sec_strength, sec_shift)

        MIN_MAX(s2)

        UPDATE_SUM(s2)

        // Store
        STORE8_CLAMPED
    }

}

static inline void
filter_8xN_pri(pixel *dst, const ptrdiff_t dst_stride,
           const pixel (*left)[2], const pixel *const top,
           const pixel *const bottom, const int w, const int h,
           const int pri_strength, const int dir,
           const int pri_shift, const enum CdefEdgeFlags edges,
           uint16_t *tmp)
{
    const int bitdepth_min_8 = bitdepth_from_max(bitdepth_max) - 8;
    const int off1 = cdef_directions8[dir][0];
    const int off1_1 = cdef_directions8[dir][1];

    MAKE_TAPS

    for (int y = 0; y < h; y++) {
        LOAD_PIX(tmp)

        // Primary pass
        LOAD_DIR(p, tmp, off1, off1_1)

        CONSTRAIN(p, pri_strength, pri_shift)

        PRI_0_UPDATE_SUM(p)

        STORE8_UNCLAMPED
    }
}

static inline void
filter_8xN_sec(pixel *dst, const ptrdiff_t dst_stride,
           const pixel (*left)[2], const pixel *const top,
           const pixel *const bottom, const int w, const int h,
           const int sec_strength, const int dir,
           const int sec_shift, const enum CdefEdgeFlags edges,
           uint16_t *tmp)
{
    const int off2 = cdef_directions8[(dir + 2) & 7][0];
    const int off3 = cdef_directions8[(dir + 6) & 7][0];

    const int off2_1 = cdef_directions8[(dir + 2) & 7][1];
    const int off3_1 = cdef_directions8[(dir + 6) & 7][1];

    for (int y = 0; y < h; y++) {
        LOAD_PIX(tmp)

        // Secondary pass 1
        LOAD_DIR(s, tmp, off2, off3)

        CONSTRAIN(s, sec_strength, sec_shift)

        SEC_0_UPDATE_SUM(s)

        // Secondary pass 2
        LOAD_DIR(s2, tmp, off2_1, off3_1)

        CONSTRAIN(s2, sec_strength, sec_shift)

        UPDATE_SUM(s2)

        STORE8_UNCLAMPED
    }
}

#define cdef_fn(w, h, tmp_stride) \
void dav1d_cdef_filter_##w##x##h##_vsx(pixel *const dst, \
                                       const ptrdiff_t dst_stride, \
                                       const pixel (*left)[2], \
                                       const pixel *const top, \
                                       const pixel *const bottom, \
                                       const int pri_strength, \
                                       const int sec_strength, \
                                       const int dir, \
                                       const int damping, \
                                       const enum CdefEdgeFlags edges) \
{ \
    ALIGN_STK_16(uint16_t, tmp_buf, 12 * tmp_stride + 8,); \
    uint16_t *tmp = tmp_buf + 2 * tmp_stride + 2; \
    copy##w##xN(tmp - 2, dst, dst_stride, left, top, bottom, w, h, edges); \
    if (pri_strength) { \
        const int pri_shift = imax(0, damping - ulog2(pri_strength)); \
        if (sec_strength) { \
            const int sec_shift = damping - ulog2(sec_strength); \
            filter_##w##xN(dst, dst_stride, left, top, bottom, w, h, pri_strength, \
                           sec_strength, dir, pri_shift, sec_shift, edges, tmp); \
        } else { \
            filter_##w##xN_pri(dst, dst_stride, left, top, bottom, w, h, pri_strength, \
                               dir, pri_shift, edges, tmp); \
        } \
    } else { \
        const int sec_shift = damping - ulog2(sec_strength); \
        filter_##w##xN_sec(dst, dst_stride, left, top, bottom, w, h, sec_strength, \
                           dir, sec_shift, edges, tmp); \
    } \
}

cdef_fn(4, 4, 8);
cdef_fn(4, 8, 8);
cdef_fn(8, 8, 16);
#endif

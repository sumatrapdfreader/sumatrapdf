/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
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

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "common/attributes.h"
#include "common/intops.h"

#include "src/mc.h"
#include "src/tables.h"

#if BITDEPTH == 8
#define get_intermediate_bits(bitdepth_max) 4
// Output in interval [-5132, 9212], fits in int16_t as is
#define PREP_BIAS 0
#else
// 4 for 10 bits/component, 2 for 12 bits/component
#define get_intermediate_bits(bitdepth_max) (14 - bitdepth_from_max(bitdepth_max))
// Output in interval [-20588, 36956] (10-bit), [-20602, 36983] (12-bit)
// Subtract a bias to ensure the output fits in int16_t
#define PREP_BIAS 8192
#endif

static NOINLINE void
put_c(pixel *dst, const ptrdiff_t dst_stride,
      const pixel *src, const ptrdiff_t src_stride, const int w, int h)
{
    do {
        pixel_copy(dst, src, w);

        dst += dst_stride;
        src += src_stride;
    } while (--h);
}

static NOINLINE void
prep_c(int16_t *tmp, const pixel *src, const ptrdiff_t src_stride,
       const int w, int h HIGHBD_DECL_SUFFIX)
{
    const int intermediate_bits = get_intermediate_bits(bitdepth_max);
    do {
        for (int x = 0; x < w; x++)
            tmp[x] = (src[x] << intermediate_bits) - PREP_BIAS;

        tmp += w;
        src += src_stride;
    } while (--h);
}

#define FILTER_8TAP(src, x, F, stride) \
    (F[0] * src[x + -3 * stride] + \
     F[1] * src[x + -2 * stride] + \
     F[2] * src[x + -1 * stride] + \
     F[3] * src[x + +0 * stride] + \
     F[4] * src[x + +1 * stride] + \
     F[5] * src[x + +2 * stride] + \
     F[6] * src[x + +3 * stride] + \
     F[7] * src[x + +4 * stride])

#define DAV1D_FILTER_8TAP_RND(src, x, F, stride, sh) \
    ((FILTER_8TAP(src, x, F, stride) + ((1 << (sh)) >> 1)) >> (sh))

#define DAV1D_FILTER_8TAP_RND2(src, x, F, stride, rnd, sh) \
    ((FILTER_8TAP(src, x, F, stride) + (rnd)) >> (sh))

#define DAV1D_FILTER_8TAP_CLIP(src, x, F, stride, sh) \
    iclip_pixel(DAV1D_FILTER_8TAP_RND(src, x, F, stride, sh))

#define DAV1D_FILTER_8TAP_CLIP2(src, x, F, stride, rnd, sh) \
    iclip_pixel(DAV1D_FILTER_8TAP_RND2(src, x, F, stride, rnd, sh))

#define GET_H_FILTER(mx) \
    const int8_t *const fh = !(mx) ? NULL : w > 4 ? \
        dav1d_mc_subpel_filters[filter_type & 3][(mx) - 1] : \
        dav1d_mc_subpel_filters[3 + (filter_type & 1)][(mx) - 1]

#define GET_V_FILTER(my) \
    const int8_t *const fv = !(my) ? NULL : h > 4 ? \
        dav1d_mc_subpel_filters[filter_type >> 2][(my) - 1] : \
        dav1d_mc_subpel_filters[3 + ((filter_type >> 2) & 1)][(my) - 1]

#define GET_FILTERS() \
    GET_H_FILTER(mx); \
    GET_V_FILTER(my)

static NOINLINE void
put_8tap_c(pixel *dst, ptrdiff_t dst_stride,
           const pixel *src, ptrdiff_t src_stride,
           const int w, int h, const int mx, const int my,
           const int filter_type HIGHBD_DECL_SUFFIX)
{
    const int intermediate_bits = get_intermediate_bits(bitdepth_max);
    const int intermediate_rnd = 32 + ((1 << (6 - intermediate_bits)) >> 1);

    GET_FILTERS();
    dst_stride = PXSTRIDE(dst_stride);
    src_stride = PXSTRIDE(src_stride);

    if (fh) {
        if (fv) {
            int tmp_h = h + 7;
            int16_t mid[128 * 135], *mid_ptr = mid;

            src -= src_stride * 3;
            do {
                for (int x = 0; x < w; x++)
                    mid_ptr[x] = DAV1D_FILTER_8TAP_RND(src, x, fh, 1,
                                                       6 - intermediate_bits);

                mid_ptr += 128;
                src += src_stride;
            } while (--tmp_h);

            mid_ptr = mid + 128 * 3;
            do {
                for (int x = 0; x < w; x++)
                    dst[x] = DAV1D_FILTER_8TAP_CLIP(mid_ptr, x, fv, 128,
                                                    6 + intermediate_bits);

                mid_ptr += 128;
                dst += dst_stride;
            } while (--h);
        } else {
            do {
                for (int x = 0; x < w; x++) {
                    dst[x] = DAV1D_FILTER_8TAP_CLIP2(src, x, fh, 1,
                                                     intermediate_rnd, 6);
                }

                dst += dst_stride;
                src += src_stride;
            } while (--h);
        }
    } else if (fv) {
        do {
            for (int x = 0; x < w; x++)
                dst[x] = DAV1D_FILTER_8TAP_CLIP(src, x, fv, src_stride, 6);

            dst += dst_stride;
            src += src_stride;
        } while (--h);
    } else
        put_c(dst, dst_stride, src, src_stride, w, h);
}

static NOINLINE void
put_8tap_scaled_c(pixel *dst, const ptrdiff_t dst_stride,
                  const pixel *src, ptrdiff_t src_stride,
                  const int w, int h, const int mx, int my,
                  const int dx, const int dy, const int filter_type
                  HIGHBD_DECL_SUFFIX)
{
    const int intermediate_bits = get_intermediate_bits(bitdepth_max);
    const int intermediate_rnd = (1 << intermediate_bits) >> 1;
    int tmp_h = (((h - 1) * dy + my) >> 10) + 8;
    int16_t mid[128 * (256 + 7)], *mid_ptr = mid;
    src_stride = PXSTRIDE(src_stride);

    src -= src_stride * 3;
    do {
        int x;
        int imx = mx, ioff = 0;

        for (x = 0; x < w; x++) {
            GET_H_FILTER(imx >> 6);
            mid_ptr[x] = fh ? DAV1D_FILTER_8TAP_RND(src, ioff, fh, 1,
                                                    6 - intermediate_bits) :
                              src[ioff] << intermediate_bits;
            imx += dx;
            ioff += imx >> 10;
            imx &= 0x3ff;
        }

        mid_ptr += 128;
        src += src_stride;
    } while (--tmp_h);

    mid_ptr = mid + 128 * 3;
    for (int y = 0; y < h; y++) {
        int x;
        GET_V_FILTER(my >> 6);

        for (x = 0; x < w; x++)
            dst[x] = fv ? DAV1D_FILTER_8TAP_CLIP(mid_ptr, x, fv, 128,
                                                 6 + intermediate_bits) :
                          iclip_pixel((mid_ptr[x] + intermediate_rnd) >>
                                              intermediate_bits);

        my += dy;
        mid_ptr += (my >> 10) * 128;
        my &= 0x3ff;
        dst += PXSTRIDE(dst_stride);
    }
}

static NOINLINE void
prep_8tap_c(int16_t *tmp, const pixel *src, ptrdiff_t src_stride,
            const int w, int h, const int mx, const int my,
            const int filter_type HIGHBD_DECL_SUFFIX)
{
    const int intermediate_bits = get_intermediate_bits(bitdepth_max);
    GET_FILTERS();
    src_stride = PXSTRIDE(src_stride);

    if (fh) {
        if (fv) {
            int tmp_h = h + 7;
            int16_t mid[128 * 135], *mid_ptr = mid;

            src -= src_stride * 3;
            do {
                for (int x = 0; x < w; x++)
                    mid_ptr[x] = DAV1D_FILTER_8TAP_RND(src, x, fh, 1,
                                                       6 - intermediate_bits);

                mid_ptr += 128;
                src += src_stride;
            } while (--tmp_h);

            mid_ptr = mid + 128 * 3;
            do {
                for (int x = 0; x < w; x++) {
                    int t = DAV1D_FILTER_8TAP_RND(mid_ptr, x, fv, 128, 6) -
                                  PREP_BIAS;
                    assert(t >= INT16_MIN && t <= INT16_MAX);
                    tmp[x] = t;
                }

                mid_ptr += 128;
                tmp += w;
            } while (--h);
        } else {
            do {
                for (int x = 0; x < w; x++)
                    tmp[x] = DAV1D_FILTER_8TAP_RND(src, x, fh, 1,
                                                   6 - intermediate_bits) -
                             PREP_BIAS;

                tmp += w;
                src += src_stride;
            } while (--h);
        }
    } else if (fv) {
        do {
            for (int x = 0; x < w; x++)
                tmp[x] = DAV1D_FILTER_8TAP_RND(src, x, fv, src_stride,
                                               6 - intermediate_bits) -
                         PREP_BIAS;

            tmp += w;
            src += src_stride;
        } while (--h);
    } else
        prep_c(tmp, src, src_stride, w, h HIGHBD_TAIL_SUFFIX);
}

static NOINLINE void
prep_8tap_scaled_c(int16_t *tmp, const pixel *src, ptrdiff_t src_stride,
                   const int w, int h, const int mx, int my,
                   const int dx, const int dy, const int filter_type
                   HIGHBD_DECL_SUFFIX)
{
    const int intermediate_bits = get_intermediate_bits(bitdepth_max);
    int tmp_h = (((h - 1) * dy + my) >> 10) + 8;
    int16_t mid[128 * (256 + 7)], *mid_ptr = mid;
    src_stride = PXSTRIDE(src_stride);

    src -= src_stride * 3;
    do {
        int x;
        int imx = mx, ioff = 0;

        for (x = 0; x < w; x++) {
            GET_H_FILTER(imx >> 6);
            mid_ptr[x] = fh ? DAV1D_FILTER_8TAP_RND(src, ioff, fh, 1,
                                                    6 - intermediate_bits) :
                              src[ioff] << intermediate_bits;
            imx += dx;
            ioff += imx >> 10;
            imx &= 0x3ff;
        }

        mid_ptr += 128;
        src += src_stride;
    } while (--tmp_h);

    mid_ptr = mid + 128 * 3;
    for (int y = 0; y < h; y++) {
        int x;
        GET_V_FILTER(my >> 6);

        for (x = 0; x < w; x++)
            tmp[x] = (fv ? DAV1D_FILTER_8TAP_RND(mid_ptr, x, fv, 128, 6)
                         : mid_ptr[x]) - PREP_BIAS;

        my += dy;
        mid_ptr += (my >> 10) * 128;
        my &= 0x3ff;
        tmp += w;
    }
}

#define filter_fns(type, type_h, type_v) \
static void put_8tap_##type##_c(pixel *const dst, \
                                const ptrdiff_t dst_stride, \
                                const pixel *const src, \
                                const ptrdiff_t src_stride, \
                                const int w, const int h, \
                                const int mx, const int my \
                                HIGHBD_DECL_SUFFIX) \
{ \
    put_8tap_c(dst, dst_stride, src, src_stride, w, h, mx, my, \
               type_h | (type_v << 2) HIGHBD_TAIL_SUFFIX); \
} \
static void put_8tap_##type##_scaled_c(pixel *const dst, \
                                       const ptrdiff_t dst_stride, \
                                       const pixel *const src, \
                                       const ptrdiff_t src_stride, \
                                       const int w, const int h, \
                                       const int mx, const int my, \
                                       const int dx, const int dy \
                                       HIGHBD_DECL_SUFFIX) \
{ \
    put_8tap_scaled_c(dst, dst_stride, src, src_stride, w, h, mx, my, dx, dy, \
                      type_h | (type_v << 2) HIGHBD_TAIL_SUFFIX); \
} \
static void prep_8tap_##type##_c(int16_t *const tmp, \
                                 const pixel *const src, \
                                 const ptrdiff_t src_stride, \
                                 const int w, const int h, \
                                 const int mx, const int my \
                                 HIGHBD_DECL_SUFFIX) \
{ \
    prep_8tap_c(tmp, src, src_stride, w, h, mx, my, \
                type_h | (type_v << 2) HIGHBD_TAIL_SUFFIX); \
} \
static void prep_8tap_##type##_scaled_c(int16_t *const tmp, \
                                        const pixel *const src, \
                                        const ptrdiff_t src_stride, \
                                        const int w, const int h, \
                                        const int mx, const int my, \
                                        const int dx, const int dy \
                                        HIGHBD_DECL_SUFFIX) \
{ \
    prep_8tap_scaled_c(tmp, src, src_stride, w, h, mx, my, dx, dy, \
                       type_h | (type_v << 2) HIGHBD_TAIL_SUFFIX); \
}

filter_fns(regular,        DAV1D_FILTER_8TAP_REGULAR, DAV1D_FILTER_8TAP_REGULAR)
filter_fns(regular_sharp,  DAV1D_FILTER_8TAP_REGULAR, DAV1D_FILTER_8TAP_SHARP)
filter_fns(regular_smooth, DAV1D_FILTER_8TAP_REGULAR, DAV1D_FILTER_8TAP_SMOOTH)
filter_fns(smooth,         DAV1D_FILTER_8TAP_SMOOTH,  DAV1D_FILTER_8TAP_SMOOTH)
filter_fns(smooth_regular, DAV1D_FILTER_8TAP_SMOOTH,  DAV1D_FILTER_8TAP_REGULAR)
filter_fns(smooth_sharp,   DAV1D_FILTER_8TAP_SMOOTH,  DAV1D_FILTER_8TAP_SHARP)
filter_fns(sharp,          DAV1D_FILTER_8TAP_SHARP,   DAV1D_FILTER_8TAP_SHARP)
filter_fns(sharp_regular,  DAV1D_FILTER_8TAP_SHARP,   DAV1D_FILTER_8TAP_REGULAR)
filter_fns(sharp_smooth,   DAV1D_FILTER_8TAP_SHARP,   DAV1D_FILTER_8TAP_SMOOTH)

#define FILTER_BILIN(src, x, mxy, stride) \
    (16 * src[x] + ((mxy) * (src[x + stride] - src[x])))

#define FILTER_BILIN_RND(src, x, mxy, stride, sh) \
    ((FILTER_BILIN(src, x, mxy, stride) + ((1 << (sh)) >> 1)) >> (sh))

#define FILTER_BILIN_CLIP(src, x, mxy, stride, sh) \
    iclip_pixel(FILTER_BILIN_RND(src, x, mxy, stride, sh))

static void put_bilin_c(pixel *dst, ptrdiff_t dst_stride,
                        const pixel *src, ptrdiff_t src_stride,
                        const int w, int h, const int mx, const int my
                        HIGHBD_DECL_SUFFIX)
{
    const int intermediate_bits = get_intermediate_bits(bitdepth_max);
    const int intermediate_rnd = (1 << intermediate_bits) >> 1;
    dst_stride = PXSTRIDE(dst_stride);
    src_stride = PXSTRIDE(src_stride);

    if (mx) {
        if (my) {
            int16_t mid[128 * 129], *mid_ptr = mid;
            int tmp_h = h + 1;

            do {
                for (int x = 0; x < w; x++)
                    mid_ptr[x] = FILTER_BILIN_RND(src, x, mx, 1,
                                                  4 - intermediate_bits);

                mid_ptr += 128;
                src += src_stride;
            } while (--tmp_h);

            mid_ptr = mid;
            do {
                for (int x = 0; x < w; x++)
                    dst[x] = FILTER_BILIN_CLIP(mid_ptr, x, my, 128,
                                               4 + intermediate_bits);

                mid_ptr += 128;
                dst += dst_stride;
            } while (--h);
        } else {
            do {
                for (int x = 0; x < w; x++) {
                    const int px = FILTER_BILIN_RND(src, x, mx, 1,
                                                    4 - intermediate_bits);
                    dst[x] = iclip_pixel((px + intermediate_rnd) >> intermediate_bits);
                }

                dst += dst_stride;
                src += src_stride;
            } while (--h);
        }
    } else if (my) {
        do {
            for (int x = 0; x < w; x++)
                dst[x] = FILTER_BILIN_CLIP(src, x, my, src_stride, 4);

            dst += dst_stride;
            src += src_stride;
        } while (--h);
    } else
        put_c(dst, dst_stride, src, src_stride, w, h);
}

static void put_bilin_scaled_c(pixel *dst, ptrdiff_t dst_stride,
                               const pixel *src, ptrdiff_t src_stride,
                               const int w, int h, const int mx, int my,
                               const int dx, const int dy
                               HIGHBD_DECL_SUFFIX)
{
    const int intermediate_bits = get_intermediate_bits(bitdepth_max);
    int tmp_h = (((h - 1) * dy + my) >> 10) + 2;
    int16_t mid[128 * (256 + 1)], *mid_ptr = mid;

    do {
        int x;
        int imx = mx, ioff = 0;

        for (x = 0; x < w; x++) {
            mid_ptr[x] = FILTER_BILIN_RND(src, ioff, imx >> 6, 1,
                                          4 - intermediate_bits);
            imx += dx;
            ioff += imx >> 10;
            imx &= 0x3ff;
        }

        mid_ptr += 128;
        src += PXSTRIDE(src_stride);
    } while (--tmp_h);

    mid_ptr = mid;
    do {
        int x;

        for (x = 0; x < w; x++)
            dst[x] = FILTER_BILIN_CLIP(mid_ptr, x, my >> 6, 128,
                                       4 + intermediate_bits);

        my += dy;
        mid_ptr += (my >> 10) * 128;
        my &= 0x3ff;
        dst += PXSTRIDE(dst_stride);
    } while (--h);
}

static void prep_bilin_c(int16_t *tmp,
                         const pixel *src, ptrdiff_t src_stride,
                         const int w, int h, const int mx, const int my
                         HIGHBD_DECL_SUFFIX)
{
    const int intermediate_bits = get_intermediate_bits(bitdepth_max);
    src_stride = PXSTRIDE(src_stride);

    if (mx) {
        if (my) {
            int16_t mid[128 * 129], *mid_ptr = mid;
            int tmp_h = h + 1;

            do {
                for (int x = 0; x < w; x++)
                    mid_ptr[x] = FILTER_BILIN_RND(src, x, mx, 1,
                                                  4 - intermediate_bits);

                mid_ptr += 128;
                src += src_stride;
            } while (--tmp_h);

            mid_ptr = mid;
            do {
                for (int x = 0; x < w; x++)
                    tmp[x] = FILTER_BILIN_RND(mid_ptr, x, my, 128, 4) -
                             PREP_BIAS;

                mid_ptr += 128;
                tmp += w;
            } while (--h);
        } else {
            do {
                for (int x = 0; x < w; x++)
                    tmp[x] = FILTER_BILIN_RND(src, x, mx, 1,
                                              4 - intermediate_bits) -
                             PREP_BIAS;

                tmp += w;
                src += src_stride;
            } while (--h);
        }
    } else if (my) {
        do {
            for (int x = 0; x < w; x++)
                tmp[x] = FILTER_BILIN_RND(src, x, my, src_stride,
                                          4 - intermediate_bits) - PREP_BIAS;

            tmp += w;
            src += src_stride;
        } while (--h);
    } else
        prep_c(tmp, src, src_stride, w, h HIGHBD_TAIL_SUFFIX);
}

static void prep_bilin_scaled_c(int16_t *tmp,
                                const pixel *src, ptrdiff_t src_stride,
                                const int w, int h, const int mx, int my,
                                const int dx, const int dy HIGHBD_DECL_SUFFIX)
{
    const int intermediate_bits = get_intermediate_bits(bitdepth_max);
    int tmp_h = (((h - 1) * dy + my) >> 10) + 2;
    int16_t mid[128 * (256 + 1)], *mid_ptr = mid;

    do {
        int x;
        int imx = mx, ioff = 0;

        for (x = 0; x < w; x++) {
            mid_ptr[x] = FILTER_BILIN_RND(src, ioff, imx >> 6, 1,
                                          4 - intermediate_bits);
            imx += dx;
            ioff += imx >> 10;
            imx &= 0x3ff;
        }

        mid_ptr += 128;
        src += PXSTRIDE(src_stride);
    } while (--tmp_h);

    mid_ptr = mid;
    do {
        int x;

        for (x = 0; x < w; x++)
            tmp[x] = FILTER_BILIN_RND(mid_ptr, x, my >> 6, 128, 4) - PREP_BIAS;

        my += dy;
        mid_ptr += (my >> 10) * 128;
        my &= 0x3ff;
        tmp += w;
    } while (--h);
}

static void avg_c(pixel *dst, const ptrdiff_t dst_stride,
                  const int16_t *tmp1, const int16_t *tmp2, const int w, int h
                  HIGHBD_DECL_SUFFIX)
{
    const int intermediate_bits = get_intermediate_bits(bitdepth_max);
    const int sh = intermediate_bits + 1;
    const int rnd = (1 << intermediate_bits) + PREP_BIAS * 2;
    do {
        for (int x = 0; x < w; x++)
            dst[x] = iclip_pixel((tmp1[x] + tmp2[x] + rnd) >> sh);

        tmp1 += w;
        tmp2 += w;
        dst += PXSTRIDE(dst_stride);
    } while (--h);
}

static void w_avg_c(pixel *dst, const ptrdiff_t dst_stride,
                    const int16_t *tmp1, const int16_t *tmp2, const int w, int h,
                    const int weight HIGHBD_DECL_SUFFIX)
{
    const int intermediate_bits = get_intermediate_bits(bitdepth_max);
    const int sh = intermediate_bits + 4;
    const int rnd = (8 << intermediate_bits) + PREP_BIAS * 16;
    do {
        for (int x = 0; x < w; x++)
            dst[x] = iclip_pixel((tmp1[x] * weight +
                                  tmp2[x] * (16 - weight) + rnd) >> sh);

        tmp1 += w;
        tmp2 += w;
        dst += PXSTRIDE(dst_stride);
    } while (--h);
}

static void mask_c(pixel *dst, const ptrdiff_t dst_stride,
                   const int16_t *tmp1, const int16_t *tmp2, const int w, int h,
                   const uint8_t *mask HIGHBD_DECL_SUFFIX)
{
    const int intermediate_bits = get_intermediate_bits(bitdepth_max);
    const int sh = intermediate_bits + 6;
    const int rnd = (32 << intermediate_bits) + PREP_BIAS * 64;
    do {
        for (int x = 0; x < w; x++)
            dst[x] = iclip_pixel((tmp1[x] * mask[x] +
                                  tmp2[x] * (64 - mask[x]) + rnd) >> sh);

        tmp1 += w;
        tmp2 += w;
        mask += w;
        dst += PXSTRIDE(dst_stride);
    } while (--h);
}

#define blend_px(a, b, m) (((a * (64 - m) + b * m) + 32) >> 6)
static void blend_c(pixel *dst, const ptrdiff_t dst_stride, const pixel *tmp,
                    const int w, int h, const uint8_t *mask)
{
    do {
        for (int x = 0; x < w; x++) {
            dst[x] = blend_px(dst[x], tmp[x], mask[x]);
        }
        dst += PXSTRIDE(dst_stride);
        tmp += w;
        mask += w;
    } while (--h);
}

static void blend_v_c(pixel *dst, const ptrdiff_t dst_stride, const pixel *tmp,
                      const int w, int h)
{
    const uint8_t *const mask = &dav1d_obmc_masks[w];
    do {
        for (int x = 0; x < (w * 3) >> 2; x++) {
            dst[x] = blend_px(dst[x], tmp[x], mask[x]);
        }
        dst += PXSTRIDE(dst_stride);
        tmp += w;
    } while (--h);
}

static void blend_h_c(pixel *dst, const ptrdiff_t dst_stride, const pixel *tmp,
                      const int w, int h)
{
    const uint8_t *mask = &dav1d_obmc_masks[h];
    h = (h * 3) >> 2;
    do {
        const int m = *mask++;
        for (int x = 0; x < w; x++) {
            dst[x] = blend_px(dst[x], tmp[x], m);
        }
        dst += PXSTRIDE(dst_stride);
        tmp += w;
    } while (--h);
}

static void w_mask_c(pixel *dst, const ptrdiff_t dst_stride,
                     const int16_t *tmp1, const int16_t *tmp2, const int w, int h,
                     uint8_t *mask, const int sign,
                     const int ss_hor, const int ss_ver HIGHBD_DECL_SUFFIX)
{
    // store mask at 2x2 resolution, i.e. store 2x1 sum for even rows,
    // and then load this intermediate to calculate final value for odd rows
    const int intermediate_bits = get_intermediate_bits(bitdepth_max);
    const int bitdepth = bitdepth_from_max(bitdepth_max);
    const int sh = intermediate_bits + 6;
    const int rnd = (32 << intermediate_bits) + PREP_BIAS * 64;
    const int mask_sh = bitdepth + intermediate_bits - 4;
    const int mask_rnd = 1 << (mask_sh - 5);
    do {
        for (int x = 0; x < w; x++) {
            const int m = imin(38 + ((abs(tmp1[x] - tmp2[x]) + mask_rnd) >> mask_sh), 64);
            dst[x] = iclip_pixel((tmp1[x] * m +
                                  tmp2[x] * (64 - m) + rnd) >> sh);

            if (ss_hor) {
                x++;

                const int n = imin(38 + ((abs(tmp1[x] - tmp2[x]) + mask_rnd) >> mask_sh), 64);
                dst[x] = iclip_pixel((tmp1[x] * n +
                                      tmp2[x] * (64 - n) + rnd) >> sh);

                if (h & ss_ver) {
                    mask[x >> 1] = (m + n + mask[x >> 1] + 2 - sign) >> 2;
                } else if (ss_ver) {
                    mask[x >> 1] = m + n;
                } else {
                    mask[x >> 1] = (m + n + 1 - sign) >> 1;
                }
            } else {
                mask[x] = m;
            }
        }

        tmp1 += w;
        tmp2 += w;
        dst += PXSTRIDE(dst_stride);
        if (!ss_ver || (h & 1)) mask += w >> ss_hor;
    } while (--h);
}

#define w_mask_fns(ssn, ss_hor, ss_ver) \
static void w_mask_##ssn##_c(pixel *const dst, const ptrdiff_t dst_stride, \
                             const int16_t *const tmp1, const int16_t *const tmp2, \
                             const int w, const int h, uint8_t *mask, \
                             const int sign HIGHBD_DECL_SUFFIX) \
{ \
    w_mask_c(dst, dst_stride, tmp1, tmp2, w, h, mask, sign, ss_hor, ss_ver \
             HIGHBD_TAIL_SUFFIX); \
}

w_mask_fns(444, 0, 0);
w_mask_fns(422, 1, 0);
w_mask_fns(420, 1, 1);

#undef w_mask_fns

#define FILTER_WARP_RND(src, x, F, stride, sh) \
    ((F[0] * src[x - 3 * stride] + \
      F[1] * src[x - 2 * stride] + \
      F[2] * src[x - 1 * stride] + \
      F[3] * src[x + 0 * stride] + \
      F[4] * src[x + 1 * stride] + \
      F[5] * src[x + 2 * stride] + \
      F[6] * src[x + 3 * stride] + \
      F[7] * src[x + 4 * stride] + \
      ((1 << (sh)) >> 1)) >> (sh))

#define FILTER_WARP_CLIP(src, x, F, stride, sh) \
    iclip_pixel(FILTER_WARP_RND(src, x, F, stride, sh))

static void warp_affine_8x8_c(pixel *dst, const ptrdiff_t dst_stride,
                              const pixel *src, const ptrdiff_t src_stride,
                              const int16_t *const abcd, int mx, int my
                              HIGHBD_DECL_SUFFIX)
{
    const int intermediate_bits = get_intermediate_bits(bitdepth_max);
    int16_t mid[15 * 8], *mid_ptr = mid;

    src -= 3 * PXSTRIDE(src_stride);
    for (int y = 0; y < 15; y++, mx += abcd[1]) {
        for (int x = 0, tmx = mx; x < 8; x++, tmx += abcd[0]) {
            const int8_t *const filter =
                dav1d_mc_warp_filter[64 + ((tmx + 512) >> 10)];

            mid_ptr[x] = FILTER_WARP_RND(src, x, filter, 1,
                                         7 - intermediate_bits);
        }
        src += PXSTRIDE(src_stride);
        mid_ptr += 8;
    }

    mid_ptr = &mid[3 * 8];
    for (int y = 0; y < 8; y++, my += abcd[3]) {
        for (int x = 0, tmy = my; x < 8; x++, tmy += abcd[2]) {
            const int8_t *const filter =
                dav1d_mc_warp_filter[64 + ((tmy + 512) >> 10)];

            dst[x] = FILTER_WARP_CLIP(mid_ptr, x, filter, 8,
                                      7 + intermediate_bits);
        }
        mid_ptr += 8;
        dst += PXSTRIDE(dst_stride);
    }
}

static void warp_affine_8x8t_c(int16_t *tmp, const ptrdiff_t tmp_stride,
                               const pixel *src, const ptrdiff_t src_stride,
                               const int16_t *const abcd, int mx, int my
                               HIGHBD_DECL_SUFFIX)
{
    const int intermediate_bits = get_intermediate_bits(bitdepth_max);
    int16_t mid[15 * 8], *mid_ptr = mid;

    src -= 3 * PXSTRIDE(src_stride);
    for (int y = 0; y < 15; y++, mx += abcd[1]) {
        for (int x = 0, tmx = mx; x < 8; x++, tmx += abcd[0]) {
            const int8_t *const filter =
                dav1d_mc_warp_filter[64 + ((tmx + 512) >> 10)];

            mid_ptr[x] = FILTER_WARP_RND(src, x, filter, 1,
                                         7 - intermediate_bits);
        }
        src += PXSTRIDE(src_stride);
        mid_ptr += 8;
    }

    mid_ptr = &mid[3 * 8];
    for (int y = 0; y < 8; y++, my += abcd[3]) {
        for (int x = 0, tmy = my; x < 8; x++, tmy += abcd[2]) {
            const int8_t *const filter =
                dav1d_mc_warp_filter[64 + ((tmy + 512) >> 10)];

            tmp[x] = FILTER_WARP_RND(mid_ptr, x, filter, 8, 7) - PREP_BIAS;
        }
        mid_ptr += 8;
        tmp += tmp_stride;
    }
}

static void emu_edge_c(const intptr_t bw, const intptr_t bh,
                       const intptr_t iw, const intptr_t ih,
                       const intptr_t x, const intptr_t y,
                       pixel *dst, const ptrdiff_t dst_stride,
                       const pixel *ref, const ptrdiff_t ref_stride)
{
    // find offset in reference of visible block to copy
    ref += iclip((int) y, 0, (int) ih - 1) * PXSTRIDE(ref_stride) +
           iclip((int) x, 0, (int) iw - 1);

    // number of pixels to extend (left, right, top, bottom)
    const int left_ext = iclip((int) -x, 0, (int) bw - 1);
    const int right_ext = iclip((int) (x + bw - iw), 0, (int) bw - 1);
    assert(left_ext + right_ext < bw);
    const int top_ext = iclip((int) -y, 0, (int) bh - 1);
    const int bottom_ext = iclip((int) (y + bh - ih), 0, (int) bh - 1);
    assert(top_ext + bottom_ext < bh);

    // copy visible portion first
    pixel *blk = dst + top_ext * PXSTRIDE(dst_stride);
    const int center_w = (int) (bw - left_ext - right_ext);
    const int center_h = (int) (bh - top_ext - bottom_ext);
    for (int y = 0; y < center_h; y++) {
        pixel_copy(blk + left_ext, ref, center_w);
        // extend left edge for this line
        if (left_ext)
            pixel_set(blk, blk[left_ext], left_ext);
        // extend right edge for this line
        if (right_ext)
            pixel_set(blk + left_ext + center_w, blk[left_ext + center_w - 1],
                      right_ext);
        ref += PXSTRIDE(ref_stride);
        blk += PXSTRIDE(dst_stride);
    }

    // copy top
    blk = dst + top_ext * PXSTRIDE(dst_stride);
    for (int y = 0; y < top_ext; y++) {
        pixel_copy(dst, blk, bw);
        dst += PXSTRIDE(dst_stride);
    }

    // copy bottom
    dst += center_h * PXSTRIDE(dst_stride);
    for (int y = 0; y < bottom_ext; y++) {
        pixel_copy(dst, &dst[-PXSTRIDE(dst_stride)], bw);
        dst += PXSTRIDE(dst_stride);
    }
}

static void resize_c(pixel *dst, const ptrdiff_t dst_stride,
                     const pixel *src, const ptrdiff_t src_stride,
                     const int dst_w, int h, const int src_w,
                     const int dx, const int mx0 HIGHBD_DECL_SUFFIX)
{
    do {
        int mx = mx0, src_x = -1;
        for (int x = 0; x < dst_w; x++) {
            const int8_t *const F = dav1d_resize_filter[mx >> 8];
            dst[x] = iclip_pixel((-(F[0] * src[iclip(src_x - 3, 0, src_w - 1)] +
                                    F[1] * src[iclip(src_x - 2, 0, src_w - 1)] +
                                    F[2] * src[iclip(src_x - 1, 0, src_w - 1)] +
                                    F[3] * src[iclip(src_x + 0, 0, src_w - 1)] +
                                    F[4] * src[iclip(src_x + 1, 0, src_w - 1)] +
                                    F[5] * src[iclip(src_x + 2, 0, src_w - 1)] +
                                    F[6] * src[iclip(src_x + 3, 0, src_w - 1)] +
                                    F[7] * src[iclip(src_x + 4, 0, src_w - 1)]) +
                                  64) >> 7);
            mx += dx;
            src_x += mx >> 14;
            mx &= 0x3fff;
        }

        dst += PXSTRIDE(dst_stride);
        src += PXSTRIDE(src_stride);
    } while (--h);
}

COLD void bitfn(dav1d_mc_dsp_init)(Dav1dMCDSPContext *const c) {
#define init_mc_fns(type, name) do { \
    c->mc        [type] = put_##name##_c; \
    c->mc_scaled [type] = put_##name##_scaled_c; \
    c->mct       [type] = prep_##name##_c; \
    c->mct_scaled[type] = prep_##name##_scaled_c; \
} while (0)

    init_mc_fns(FILTER_2D_8TAP_REGULAR,        8tap_regular);
    init_mc_fns(FILTER_2D_8TAP_REGULAR_SMOOTH, 8tap_regular_smooth);
    init_mc_fns(FILTER_2D_8TAP_REGULAR_SHARP,  8tap_regular_sharp);
    init_mc_fns(FILTER_2D_8TAP_SHARP_REGULAR,  8tap_sharp_regular);
    init_mc_fns(FILTER_2D_8TAP_SHARP_SMOOTH,   8tap_sharp_smooth);
    init_mc_fns(FILTER_2D_8TAP_SHARP,          8tap_sharp);
    init_mc_fns(FILTER_2D_8TAP_SMOOTH_REGULAR, 8tap_smooth_regular);
    init_mc_fns(FILTER_2D_8TAP_SMOOTH,         8tap_smooth);
    init_mc_fns(FILTER_2D_8TAP_SMOOTH_SHARP,   8tap_smooth_sharp);
    init_mc_fns(FILTER_2D_BILINEAR,            bilin);

    c->avg      = avg_c;
    c->w_avg    = w_avg_c;
    c->mask     = mask_c;
    c->blend    = blend_c;
    c->blend_v  = blend_v_c;
    c->blend_h  = blend_h_c;
    c->w_mask[0] = w_mask_444_c;
    c->w_mask[1] = w_mask_422_c;
    c->w_mask[2] = w_mask_420_c;
    c->warp8x8  = warp_affine_8x8_c;
    c->warp8x8t = warp_affine_8x8t_c;
    c->emu_edge = emu_edge_c;
    c->resize   = resize_c;

#if HAVE_ASM
#if ARCH_AARCH64 || ARCH_ARM
    bitfn(dav1d_mc_dsp_init_arm)(c);
#elif ARCH_X86
    bitfn(dav1d_mc_dsp_init_x86)(c);
#endif
#endif
}

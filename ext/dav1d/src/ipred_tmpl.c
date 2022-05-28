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

#include "src/ipred.h"
#include "src/tables.h"

static NOINLINE void
splat_dc(pixel *dst, const ptrdiff_t stride,
         const int width, const int height, const int dc HIGHBD_DECL_SUFFIX)
{
#if BITDEPTH == 8
    assert(dc <= 0xff);
    if (width > 4) {
        const uint64_t dcN = dc * 0x0101010101010101ULL;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x += sizeof(dcN))
                *((uint64_t *) &dst[x]) = dcN;
            dst += PXSTRIDE(stride);
        }
    } else {
        const unsigned dcN = dc * 0x01010101U;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x += sizeof(dcN))
                *((unsigned *) &dst[x]) = dcN;
            dst += PXSTRIDE(stride);
        }
    }
#else
    assert(dc <= bitdepth_max);
    const uint64_t dcN = dc * 0x0001000100010001ULL;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x += sizeof(dcN) >> 1)
            *((uint64_t *) &dst[x]) = dcN;
        dst += PXSTRIDE(stride);
    }
#endif
}

static NOINLINE void
cfl_pred(pixel *dst, const ptrdiff_t stride,
         const int width, const int height, const int dc,
         const int16_t *ac, const int alpha HIGHBD_DECL_SUFFIX)
{
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const int diff = alpha * ac[x];
            dst[x] = iclip_pixel(dc + apply_sign((abs(diff) + 32) >> 6, diff));
        }
        ac += width;
        dst += PXSTRIDE(stride);
    }
}

static unsigned dc_gen_top(const pixel *const topleft, const int width) {
    unsigned dc = width >> 1;
    for (int i = 0; i < width; i++)
       dc += topleft[1 + i];
    return dc >> ctz(width);
}

static void ipred_dc_top_c(pixel *dst, const ptrdiff_t stride,
                           const pixel *const topleft,
                           const int width, const int height, const int a,
                           const int max_width, const int max_height
                           HIGHBD_DECL_SUFFIX)
{
    splat_dc(dst, stride, width, height, dc_gen_top(topleft, width)
             HIGHBD_TAIL_SUFFIX);
}

static void ipred_cfl_top_c(pixel *dst, const ptrdiff_t stride,
                            const pixel *const topleft,
                            const int width, const int height,
                            const int16_t *ac, const int alpha
                            HIGHBD_DECL_SUFFIX)
{
    cfl_pred(dst, stride, width, height, dc_gen_top(topleft, width), ac, alpha
             HIGHBD_TAIL_SUFFIX);
}

static unsigned dc_gen_left(const pixel *const topleft, const int height) {
    unsigned dc = height >> 1;
    for (int i = 0; i < height; i++)
       dc += topleft[-(1 + i)];
    return dc >> ctz(height);
}

static void ipred_dc_left_c(pixel *dst, const ptrdiff_t stride,
                            const pixel *const topleft,
                            const int width, const int height, const int a,
                            const int max_width, const int max_height
                            HIGHBD_DECL_SUFFIX)
{
    splat_dc(dst, stride, width, height, dc_gen_left(topleft, height)
             HIGHBD_TAIL_SUFFIX);
}

static void ipred_cfl_left_c(pixel *dst, const ptrdiff_t stride,
                             const pixel *const topleft,
                             const int width, const int height,
                             const int16_t *ac, const int alpha
                             HIGHBD_DECL_SUFFIX)
{
    const unsigned dc = dc_gen_left(topleft, height);
    cfl_pred(dst, stride, width, height, dc, ac, alpha HIGHBD_TAIL_SUFFIX);
}

#if BITDEPTH == 8
#define MULTIPLIER_1x2 0x5556
#define MULTIPLIER_1x4 0x3334
#define BASE_SHIFT 16
#else
#define MULTIPLIER_1x2 0xAAAB
#define MULTIPLIER_1x4 0x6667
#define BASE_SHIFT 17
#endif

static unsigned dc_gen(const pixel *const topleft,
                       const int width, const int height)
{
    unsigned dc = (width + height) >> 1;
    for (int i = 0; i < width; i++)
       dc += topleft[i + 1];
    for (int i = 0; i < height; i++)
       dc += topleft[-(i + 1)];
    dc >>= ctz(width + height);

    if (width != height) {
        dc *= (width > height * 2 || height > width * 2) ? MULTIPLIER_1x4 :
                                                           MULTIPLIER_1x2;
        dc >>= BASE_SHIFT;
    }
    return dc;
}

static void ipred_dc_c(pixel *dst, const ptrdiff_t stride,
                       const pixel *const topleft,
                       const int width, const int height, const int a,
                       const int max_width, const int max_height
                       HIGHBD_DECL_SUFFIX)
{
    splat_dc(dst, stride, width, height, dc_gen(topleft, width, height)
             HIGHBD_TAIL_SUFFIX);
}

static void ipred_cfl_c(pixel *dst, const ptrdiff_t stride,
                        const pixel *const topleft,
                        const int width, const int height,
                        const int16_t *ac, const int alpha
                        HIGHBD_DECL_SUFFIX)
{
    unsigned dc = dc_gen(topleft, width, height);
    cfl_pred(dst, stride, width, height, dc, ac, alpha HIGHBD_TAIL_SUFFIX);
}

#undef MULTIPLIER_1x2
#undef MULTIPLIER_1x4
#undef BASE_SHIFT

static void ipred_dc_128_c(pixel *dst, const ptrdiff_t stride,
                           const pixel *const topleft,
                           const int width, const int height, const int a,
                           const int max_width, const int max_height
                           HIGHBD_DECL_SUFFIX)
{
#if BITDEPTH == 16
    const int dc = (bitdepth_max + 1) >> 1;
#else
    const int dc = 128;
#endif
    splat_dc(dst, stride, width, height, dc HIGHBD_TAIL_SUFFIX);
}

static void ipred_cfl_128_c(pixel *dst, const ptrdiff_t stride,
                            const pixel *const topleft,
                            const int width, const int height,
                            const int16_t *ac, const int alpha
                            HIGHBD_DECL_SUFFIX)
{
#if BITDEPTH == 16
    const int dc = (bitdepth_max + 1) >> 1;
#else
    const int dc = 128;
#endif
    cfl_pred(dst, stride, width, height, dc, ac, alpha HIGHBD_TAIL_SUFFIX);
}

static void ipred_v_c(pixel *dst, const ptrdiff_t stride,
                      const pixel *const topleft,
                      const int width, const int height, const int a,
                      const int max_width, const int max_height
                      HIGHBD_DECL_SUFFIX)
{
    for (int y = 0; y < height; y++) {
        pixel_copy(dst, topleft + 1, width);
        dst += PXSTRIDE(stride);
    }
}

static void ipred_h_c(pixel *dst, const ptrdiff_t stride,
                      const pixel *const topleft,
                      const int width, const int height, const int a,
                      const int max_width, const int max_height
                      HIGHBD_DECL_SUFFIX)
{
    for (int y = 0; y < height; y++) {
        pixel_set(dst, topleft[-(1 + y)], width);
        dst += PXSTRIDE(stride);
    }
}

static void ipred_paeth_c(pixel *dst, const ptrdiff_t stride,
                          const pixel *const tl_ptr,
                          const int width, const int height, const int a,
                          const int max_width, const int max_height
                          HIGHBD_DECL_SUFFIX)
{
    const int topleft = tl_ptr[0];
    for (int y = 0; y < height; y++) {
        const int left = tl_ptr[-(y + 1)];
        for (int x = 0; x < width; x++) {
            const int top = tl_ptr[1 + x];
            const int base = left + top - topleft;
            const int ldiff = abs(left - base);
            const int tdiff = abs(top - base);
            const int tldiff = abs(topleft - base);

            dst[x] = ldiff <= tdiff && ldiff <= tldiff ? left :
                     tdiff <= tldiff ? top : topleft;
        }
        dst += PXSTRIDE(stride);
    }
}

static void ipred_smooth_c(pixel *dst, const ptrdiff_t stride,
                           const pixel *const topleft,
                           const int width, const int height, const int a,
                           const int max_width, const int max_height
                           HIGHBD_DECL_SUFFIX)
{
    const uint8_t *const weights_hor = &dav1d_sm_weights[width];
    const uint8_t *const weights_ver = &dav1d_sm_weights[height];
    const int right = topleft[width], bottom = topleft[-height];

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const int pred = weights_ver[y]  * topleft[1 + x] +
                      (256 - weights_ver[y]) * bottom +
                             weights_hor[x]  * topleft[-(1 + y)] +
                      (256 - weights_hor[x]) * right;
            dst[x] = (pred + 256) >> 9;
        }
        dst += PXSTRIDE(stride);
    }
}

static void ipred_smooth_v_c(pixel *dst, const ptrdiff_t stride,
                             const pixel *const topleft,
                             const int width, const int height, const int a,
                             const int max_width, const int max_height
                             HIGHBD_DECL_SUFFIX)
{
    const uint8_t *const weights_ver = &dav1d_sm_weights[height];
    const int bottom = topleft[-height];

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const int pred = weights_ver[y]  * topleft[1 + x] +
                      (256 - weights_ver[y]) * bottom;
            dst[x] = (pred + 128) >> 8;
        }
        dst += PXSTRIDE(stride);
    }
}

static void ipred_smooth_h_c(pixel *dst, const ptrdiff_t stride,
                             const pixel *const topleft,
                             const int width, const int height, const int a,
                             const int max_width, const int max_height
                             HIGHBD_DECL_SUFFIX)
{
    const uint8_t *const weights_hor = &dav1d_sm_weights[width];
    const int right = topleft[width];

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const int pred = weights_hor[x]  * topleft[-(y + 1)] +
                      (256 - weights_hor[x]) * right;
            dst[x] = (pred + 128) >> 8;
        }
        dst += PXSTRIDE(stride);
    }
}

static NOINLINE int get_filter_strength(const int wh, const int angle,
                                        const int is_sm)
{
    if (is_sm) {
        if (wh <= 8) {
            if (angle >= 64) return 2;
            if (angle >= 40) return 1;
        } else if (wh <= 16) {
            if (angle >= 48) return 2;
            if (angle >= 20) return 1;
        } else if (wh <= 24) {
            if (angle >=  4) return 3;
        } else {
            return 3;
        }
    } else {
        if (wh <= 8) {
            if (angle >= 56) return 1;
        } else if (wh <= 16) {
            if (angle >= 40) return 1;
        } else if (wh <= 24) {
            if (angle >= 32) return 3;
            if (angle >= 16) return 2;
            if (angle >=  8) return 1;
        } else if (wh <= 32) {
            if (angle >= 32) return 3;
            if (angle >=  4) return 2;
            return 1;
        } else {
            return 3;
        }
    }
    return 0;
}

static NOINLINE void filter_edge(pixel *const out, const int sz,
                                 const int lim_from, const int lim_to,
                                 const pixel *const in, const int from,
                                 const int to, const int strength)
{
    static const uint8_t kernel[3][5] = {
        { 0, 4, 8, 4, 0 },
        { 0, 5, 6, 5, 0 },
        { 2, 4, 4, 4, 2 }
    };

    assert(strength > 0);
    int i = 0;
    for (; i < imin(sz, lim_from); i++)
        out[i] = in[iclip(i, from, to - 1)];
    for (; i < imin(lim_to, sz); i++) {
        int s = 0;
        for (int j = 0; j < 5; j++)
            s += in[iclip(i - 2 + j, from, to - 1)] * kernel[strength - 1][j];
        out[i] = (s + 8) >> 4;
    }
    for (; i < sz; i++)
        out[i] = in[iclip(i, from, to - 1)];
}

static inline int get_upsample(const int wh, const int angle, const int is_sm) {
    return angle < 40 && wh <= 16 >> is_sm;
}

static NOINLINE void upsample_edge(pixel *const out, const int hsz,
                                   const pixel *const in, const int from,
                                   const int to HIGHBD_DECL_SUFFIX)
{
    static const int8_t kernel[4] = { -1, 9, 9, -1 };
    int i;
    for (i = 0; i < hsz - 1; i++) {
        out[i * 2] = in[iclip(i, from, to - 1)];

        int s = 0;
        for (int j = 0; j < 4; j++)
            s += in[iclip(i + j - 1, from, to - 1)] * kernel[j];
        out[i * 2 + 1] = iclip_pixel((s + 8) >> 4);
    }
    out[i * 2] = in[iclip(i, from, to - 1)];
}

static void ipred_z1_c(pixel *dst, const ptrdiff_t stride,
                       const pixel *const topleft_in,
                       const int width, const int height, int angle,
                       const int max_width, const int max_height
                       HIGHBD_DECL_SUFFIX)
{
    const int is_sm = (angle >> 9) & 0x1;
    const int enable_intra_edge_filter = angle >> 10;
    angle &= 511;
    assert(angle < 90);
    int dx = dav1d_dr_intra_derivative[angle >> 1];
    pixel top_out[64 + 64];
    const pixel *top;
    int max_base_x;
    const int upsample_above = enable_intra_edge_filter ?
        get_upsample(width + height, 90 - angle, is_sm) : 0;
    if (upsample_above) {
        upsample_edge(top_out, width + height, &topleft_in[1], -1,
                      width + imin(width, height) HIGHBD_TAIL_SUFFIX);
        top = top_out;
        max_base_x = 2 * (width + height) - 2;
        dx <<= 1;
    } else {
        const int filter_strength = enable_intra_edge_filter ?
            get_filter_strength(width + height, 90 - angle, is_sm) : 0;
        if (filter_strength) {
            filter_edge(top_out, width + height, 0, width + height,
                        &topleft_in[1], -1, width + imin(width, height),
                        filter_strength);
            top = top_out;
            max_base_x = width + height - 1;
        } else {
            top = &topleft_in[1];
            max_base_x = width + imin(width, height) - 1;
        }
    }
    const int base_inc = 1 + upsample_above;
    for (int y = 0, xpos = dx; y < height;
         y++, dst += PXSTRIDE(stride), xpos += dx)
    {
        const int frac = xpos & 0x3E;

        for (int x = 0, base = xpos >> 6; x < width; x++, base += base_inc) {
            if (base < max_base_x) {
                const int v = top[base] * (64 - frac) + top[base + 1] * frac;
                dst[x] = (v + 32) >> 6;
            } else {
                pixel_set(&dst[x], top[max_base_x], width - x);
                break;
            }
        }
    }
}

static void ipred_z2_c(pixel *dst, const ptrdiff_t stride,
                       const pixel *const topleft_in,
                       const int width, const int height, int angle,
                       const int max_width, const int max_height
                       HIGHBD_DECL_SUFFIX)
{
    const int is_sm = (angle >> 9) & 0x1;
    const int enable_intra_edge_filter = angle >> 10;
    angle &= 511;
    assert(angle > 90 && angle < 180);
    int dy = dav1d_dr_intra_derivative[(angle - 90) >> 1];
    int dx = dav1d_dr_intra_derivative[(180 - angle) >> 1];
    const int upsample_left = enable_intra_edge_filter ?
        get_upsample(width + height, 180 - angle, is_sm) : 0;
    const int upsample_above = enable_intra_edge_filter ?
        get_upsample(width + height, angle - 90, is_sm) : 0;
    pixel edge[64 + 64 + 1];
    pixel *const topleft = &edge[64];

    if (upsample_above) {
        upsample_edge(topleft, width + 1, topleft_in, 0, width + 1
                      HIGHBD_TAIL_SUFFIX);
        dx <<= 1;
    } else {
        const int filter_strength = enable_intra_edge_filter ?
            get_filter_strength(width + height, angle - 90, is_sm) : 0;

        if (filter_strength) {
            filter_edge(&topleft[1], width, 0, max_width,
                        &topleft_in[1], -1, width,
                        filter_strength);
        } else {
            pixel_copy(&topleft[1], &topleft_in[1], width);
        }
    }
    if (upsample_left) {
        upsample_edge(&topleft[-height * 2], height + 1, &topleft_in[-height],
                      0, height + 1 HIGHBD_TAIL_SUFFIX);
        dy <<= 1;
    } else {
        const int filter_strength = enable_intra_edge_filter ?
            get_filter_strength(width + height, 180 - angle, is_sm) : 0;

        if (filter_strength) {
            filter_edge(&topleft[-height], height, height - max_height, height,
                        &topleft_in[-height],
                        0, height + 1, filter_strength);
        } else {
            pixel_copy(&topleft[-height], &topleft_in[-height], height);
        }
    }
    *topleft = *topleft_in;

    const int base_inc_x = 1 + upsample_above;
    const pixel *const left = &topleft[-(1 + upsample_left)];
    for (int y = 0, xpos = ((1 + upsample_above) << 6) - dx; y < height;
         y++, xpos -= dx, dst += PXSTRIDE(stride))
    {
        int base_x = xpos >> 6;
        const int frac_x = xpos & 0x3E;

        for (int x = 0, ypos = (y << (6 + upsample_left)) - dy; x < width;
             x++, base_x += base_inc_x, ypos -= dy)
        {
            int v;
            if (base_x >= 0) {
                v = topleft[base_x] * (64 - frac_x) +
                    topleft[base_x + 1] * frac_x;
            } else {
                const int base_y = ypos >> 6;
                assert(base_y >= -(1 + upsample_left));
                const int frac_y = ypos & 0x3E;
                v = left[-base_y] * (64 - frac_y) +
                    left[-(base_y + 1)] * frac_y;
            }
            dst[x] = (v + 32) >> 6;
        }
    }
}

static void ipred_z3_c(pixel *dst, const ptrdiff_t stride,
                       const pixel *const topleft_in,
                       const int width, const int height, int angle,
                       const int max_width, const int max_height
                       HIGHBD_DECL_SUFFIX)
{
    const int is_sm = (angle >> 9) & 0x1;
    const int enable_intra_edge_filter = angle >> 10;
    angle &= 511;
    assert(angle > 180);
    int dy = dav1d_dr_intra_derivative[(270 - angle) >> 1];
    pixel left_out[64 + 64];
    const pixel *left;
    int max_base_y;
    const int upsample_left = enable_intra_edge_filter ?
        get_upsample(width + height, angle - 180, is_sm) : 0;
    if (upsample_left) {
        upsample_edge(left_out, width + height,
                      &topleft_in[-(width + height)],
                      imax(width - height, 0), width + height + 1
                      HIGHBD_TAIL_SUFFIX);
        left = &left_out[2 * (width + height) - 2];
        max_base_y = 2 * (width + height) - 2;
        dy <<= 1;
    } else {
        const int filter_strength = enable_intra_edge_filter ?
            get_filter_strength(width + height, angle - 180, is_sm) : 0;

        if (filter_strength) {
            filter_edge(left_out, width + height, 0, width + height,
                        &topleft_in[-(width + height)],
                        imax(width - height, 0), width + height + 1,
                        filter_strength);
            left = &left_out[width + height - 1];
            max_base_y = width + height - 1;
        } else {
            left = &topleft_in[-1];
            max_base_y = height + imin(width, height) - 1;
        }
    }
    const int base_inc = 1 + upsample_left;
    for (int x = 0, ypos = dy; x < width; x++, ypos += dy) {
        const int frac = ypos & 0x3E;

        for (int y = 0, base = ypos >> 6; y < height; y++, base += base_inc) {
            if (base < max_base_y) {
                const int v = left[-base] * (64 - frac) +
                              left[-(base + 1)] * frac;
                dst[y * PXSTRIDE(stride) + x] = (v + 32) >> 6;
            } else {
                do {
                    dst[y * PXSTRIDE(stride) + x] = left[-max_base_y];
                } while (++y < height);
                break;
            }
        }
    }
}

#if ARCH_X86
#define FILTER(flt_ptr, p0, p1, p2, p3, p4, p5, p6) \
    flt_ptr[ 0] * p0 + flt_ptr[ 1] * p1 +           \
    flt_ptr[16] * p2 + flt_ptr[17] * p3 +           \
    flt_ptr[32] * p4 + flt_ptr[33] * p5 +           \
    flt_ptr[48] * p6
#define FLT_INCR 2
#else
#define FILTER(flt_ptr, p0, p1, p2, p3, p4, p5, p6) \
    flt_ptr[ 0] * p0 + flt_ptr[ 8] * p1 +           \
    flt_ptr[16] * p2 + flt_ptr[24] * p3 +           \
    flt_ptr[32] * p4 + flt_ptr[40] * p5 +           \
    flt_ptr[48] * p6
#define FLT_INCR 1
#endif

/* Up to 32x32 only */
static void ipred_filter_c(pixel *dst, const ptrdiff_t stride,
                           const pixel *const topleft_in,
                           const int width, const int height, int filt_idx,
                           const int max_width, const int max_height
                           HIGHBD_DECL_SUFFIX)
{
    filt_idx &= 511;
    assert(filt_idx < 5);

    const int8_t *const filter = dav1d_filter_intra_taps[filt_idx];
    const pixel *top = &topleft_in[1];
    for (int y = 0; y < height; y += 2) {
        const pixel *topleft = &topleft_in[-y];
        const pixel *left = &topleft[-1];
        ptrdiff_t left_stride = -1;
        for (int x = 0; x < width; x += 4) {
            const int p0 = *topleft;
            const int p1 = top[0], p2 = top[1], p3 = top[2], p4 = top[3];
            const int p5 = left[0 * left_stride], p6 = left[1 * left_stride];
            pixel *ptr = &dst[x];
            const int8_t *flt_ptr = filter;

            for (int yy = 0; yy < 2; yy++) {
                for (int xx = 0; xx < 4; xx++, flt_ptr += FLT_INCR) {
                    const int acc = FILTER(flt_ptr, p0, p1, p2, p3, p4, p5, p6);
                    ptr[xx] = iclip_pixel((acc + 8) >> 4);
                }
                ptr += PXSTRIDE(stride);
            }
            left = &dst[x + 4 - 1];
            left_stride = PXSTRIDE(stride);
            top += 4;
            topleft = &top[-1];
        }
        top = &dst[PXSTRIDE(stride)];
        dst = &dst[PXSTRIDE(stride) * 2];
    }
}

static NOINLINE void
cfl_ac_c(int16_t *ac, const pixel *ypx, const ptrdiff_t stride,
         const int w_pad, const int h_pad, const int width, const int height,
         const int ss_hor, const int ss_ver)
{
    int y, x;
    int16_t *const ac_orig = ac;

    assert(w_pad >= 0 && w_pad * 4 < width);
    assert(h_pad >= 0 && h_pad * 4 < height);

    for (y = 0; y < height - 4 * h_pad; y++) {
        for (x = 0; x < width - 4 * w_pad; x++) {
            int ac_sum = ypx[x << ss_hor];
            if (ss_hor) ac_sum += ypx[x * 2 + 1];
            if (ss_ver) {
                ac_sum += ypx[(x << ss_hor) + PXSTRIDE(stride)];
                if (ss_hor) ac_sum += ypx[x * 2 + 1 + PXSTRIDE(stride)];
            }
            ac[x] = ac_sum << (1 + !ss_ver + !ss_hor);
        }
        for (; x < width; x++)
            ac[x] = ac[x - 1];
        ac += width;
        ypx += PXSTRIDE(stride) << ss_ver;
    }
    for (; y < height; y++) {
        memcpy(ac, &ac[-width], width * sizeof(*ac));
        ac += width;
    }

    const int log2sz = ctz(width) + ctz(height);
    int sum = (1 << log2sz) >> 1;
    for (ac = ac_orig, y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            sum += ac[x];
        ac += width;
    }
    sum >>= log2sz;

    // subtract DC
    for (ac = ac_orig, y = 0; y < height; y++) {
        for (x = 0; x < width; x++)
            ac[x] -= sum;
        ac += width;
    }
}

#define cfl_ac_fn(fmt, ss_hor, ss_ver) \
static void cfl_ac_##fmt##_c(int16_t *const ac, const pixel *const ypx, \
                             const ptrdiff_t stride, const int w_pad, \
                             const int h_pad, const int cw, const int ch) \
{ \
    cfl_ac_c(ac, ypx, stride, w_pad, h_pad, cw, ch, ss_hor, ss_ver); \
}

cfl_ac_fn(420, 1, 1)
cfl_ac_fn(422, 1, 0)
cfl_ac_fn(444, 0, 0)

static void pal_pred_c(pixel *dst, const ptrdiff_t stride,
                       const uint16_t *const pal, const uint8_t *idx,
                       const int w, const int h)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++)
            dst[x] = (pixel) pal[idx[x]];
        idx += w;
        dst += PXSTRIDE(stride);
    }
}

COLD void bitfn(dav1d_intra_pred_dsp_init)(Dav1dIntraPredDSPContext *const c) {
    c->intra_pred[DC_PRED      ] = ipred_dc_c;
    c->intra_pred[DC_128_PRED  ] = ipred_dc_128_c;
    c->intra_pred[TOP_DC_PRED  ] = ipred_dc_top_c;
    c->intra_pred[LEFT_DC_PRED ] = ipred_dc_left_c;
    c->intra_pred[HOR_PRED     ] = ipred_h_c;
    c->intra_pred[VERT_PRED    ] = ipred_v_c;
    c->intra_pred[PAETH_PRED   ] = ipred_paeth_c;
    c->intra_pred[SMOOTH_PRED  ] = ipred_smooth_c;
    c->intra_pred[SMOOTH_V_PRED] = ipred_smooth_v_c;
    c->intra_pred[SMOOTH_H_PRED] = ipred_smooth_h_c;
    c->intra_pred[Z1_PRED      ] = ipred_z1_c;
    c->intra_pred[Z2_PRED      ] = ipred_z2_c;
    c->intra_pred[Z3_PRED      ] = ipred_z3_c;
    c->intra_pred[FILTER_PRED  ] = ipred_filter_c;

    c->cfl_ac[DAV1D_PIXEL_LAYOUT_I420 - 1] = cfl_ac_420_c;
    c->cfl_ac[DAV1D_PIXEL_LAYOUT_I422 - 1] = cfl_ac_422_c;
    c->cfl_ac[DAV1D_PIXEL_LAYOUT_I444 - 1] = cfl_ac_444_c;

    c->cfl_pred[DC_PRED     ] = ipred_cfl_c;
    c->cfl_pred[DC_128_PRED ] = ipred_cfl_128_c;
    c->cfl_pred[TOP_DC_PRED ] = ipred_cfl_top_c;
    c->cfl_pred[LEFT_DC_PRED] = ipred_cfl_left_c;

    c->pal_pred = pal_pred_c;

#if HAVE_ASM
#if ARCH_AARCH64 || ARCH_ARM
    bitfn(dav1d_intra_pred_dsp_init_arm)(c);
#elif ARCH_X86
    bitfn(dav1d_intra_pred_dsp_init_x86)(c);
#endif
#endif
}

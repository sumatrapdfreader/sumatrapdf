/*
 * Copyright © 2018-2019, VideoLAN and dav1d authors
 * Copyright © 2018-2019, Two Orioles, LLC
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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common/attributes.h"
#include "common/intops.h"

#include "src/itx.h"
#include "src/itx_1d.h"

static NOINLINE void
inv_txfm_add_c(pixel *dst, const ptrdiff_t stride, coef *const coeff,
               const int eob, const int w, const int h, const int shift,
               const itx_1d_fn first_1d_fn, const itx_1d_fn second_1d_fn,
               const int has_dconly HIGHBD_DECL_SUFFIX)
{
    assert(w >= 4 && w <= 64);
    assert(h >= 4 && h <= 64);
    assert(eob >= 0);

    const int is_rect2 = w * 2 == h || h * 2 == w;
    const int rnd = (1 << shift) >> 1;

    if (eob < has_dconly) {
        int dc = coeff[0];
        coeff[0] = 0;
        if (is_rect2)
            dc = (dc * 181 + 128) >> 8;
        dc = (dc * 181 + 128) >> 8;
        dc = (dc + rnd) >> shift;
        dc = (dc * 181 + 128 + 2048) >> 12;
        for (int y = 0; y < h; y++, dst += PXSTRIDE(stride))
            for (int x = 0; x < w; x++)
                dst[x] = iclip_pixel(dst[x] + dc);
        return;
    }

    const int sh = imin(h, 32), sw = imin(w, 32);
#if BITDEPTH == 8
    const int row_clip_min = INT16_MIN;
    const int col_clip_min = INT16_MIN;
#else
    const int row_clip_min = (int) ((unsigned) ~bitdepth_max << 7);
    const int col_clip_min = (int) ((unsigned) ~bitdepth_max << 5);
#endif
    const int row_clip_max = ~row_clip_min;
    const int col_clip_max = ~col_clip_min;

    int32_t tmp[64 * 64], *c = tmp;
    for (int y = 0; y < sh; y++, c += w) {
        if (is_rect2)
            for (int x = 0; x < sw; x++)
                c[x] = (coeff[y + x * sh] * 181 + 128) >> 8;
        else
            for (int x = 0; x < sw; x++)
                c[x] = coeff[y + x * sh];
        first_1d_fn(c, 1, row_clip_min, row_clip_max);
    }

    memset(coeff, 0, sizeof(*coeff) * sw * sh);
    for (int i = 0; i < w * sh; i++)
        tmp[i] = iclip((tmp[i] + rnd) >> shift, col_clip_min, col_clip_max);

    for (int x = 0; x < w; x++)
        second_1d_fn(&tmp[x], w, col_clip_min, col_clip_max);

    c = tmp;
    for (int y = 0; y < h; y++, dst += PXSTRIDE(stride))
        for (int x = 0; x < w; x++)
            dst[x] = iclip_pixel(dst[x] + ((*c++ + 8) >> 4));
}

#define inv_txfm_fn(type1, type2, w, h, shift, has_dconly) \
static void \
inv_txfm_add_##type1##_##type2##_##w##x##h##_c(pixel *dst, \
                                               const ptrdiff_t stride, \
                                               coef *const coeff, \
                                               const int eob \
                                               HIGHBD_DECL_SUFFIX) \
{ \
    inv_txfm_add_c(dst, stride, coeff, eob, w, h, shift, \
                   dav1d_inv_##type1##w##_1d_c, dav1d_inv_##type2##h##_1d_c, \
                   has_dconly HIGHBD_TAIL_SUFFIX); \
}

#define inv_txfm_fn64(w, h, shift) \
inv_txfm_fn(dct, dct, w, h, shift, 1)

#define inv_txfm_fn32(w, h, shift) \
inv_txfm_fn64(w, h, shift) \
inv_txfm_fn(identity, identity, w, h, shift, 0)

#define inv_txfm_fn16(w, h, shift) \
inv_txfm_fn32(w, h, shift) \
inv_txfm_fn(adst,     dct,      w, h, shift, 0) \
inv_txfm_fn(dct,      adst,     w, h, shift, 0) \
inv_txfm_fn(adst,     adst,     w, h, shift, 0) \
inv_txfm_fn(dct,      flipadst, w, h, shift, 0) \
inv_txfm_fn(flipadst, dct,      w, h, shift, 0) \
inv_txfm_fn(adst,     flipadst, w, h, shift, 0) \
inv_txfm_fn(flipadst, adst,     w, h, shift, 0) \
inv_txfm_fn(flipadst, flipadst, w, h, shift, 0) \
inv_txfm_fn(identity, dct,      w, h, shift, 0) \
inv_txfm_fn(dct,      identity, w, h, shift, 0) \

#define inv_txfm_fn84(w, h, shift) \
inv_txfm_fn16(w, h, shift) \
inv_txfm_fn(identity, flipadst, w, h, shift, 0) \
inv_txfm_fn(flipadst, identity, w, h, shift, 0) \
inv_txfm_fn(identity, adst,     w, h, shift, 0) \
inv_txfm_fn(adst,     identity, w, h, shift, 0) \

inv_txfm_fn84( 4,  4, 0)
inv_txfm_fn84( 4,  8, 0)
inv_txfm_fn84( 4, 16, 1)
inv_txfm_fn84( 8,  4, 0)
inv_txfm_fn84( 8,  8, 1)
inv_txfm_fn84( 8, 16, 1)
inv_txfm_fn32( 8, 32, 2)
inv_txfm_fn84(16,  4, 1)
inv_txfm_fn84(16,  8, 1)
inv_txfm_fn16(16, 16, 2)
inv_txfm_fn32(16, 32, 1)
inv_txfm_fn64(16, 64, 2)
inv_txfm_fn32(32,  8, 2)
inv_txfm_fn32(32, 16, 1)
inv_txfm_fn32(32, 32, 2)
inv_txfm_fn64(32, 64, 1)
inv_txfm_fn64(64, 16, 2)
inv_txfm_fn64(64, 32, 1)
inv_txfm_fn64(64, 64, 2)

static void inv_txfm_add_wht_wht_4x4_c(pixel *dst, const ptrdiff_t stride,
                                       coef *const coeff, const int eob
                                       HIGHBD_DECL_SUFFIX)
{
    int32_t tmp[4 * 4], *c = tmp;
    for (int y = 0; y < 4; y++, c += 4) {
        for (int x = 0; x < 4; x++)
            c[x] = coeff[y + x * 4] >> 2;
        dav1d_inv_wht4_1d_c(c, 1);
    }
    memset(coeff, 0, sizeof(*coeff) * 4 * 4);

    for (int x = 0; x < 4; x++)
        dav1d_inv_wht4_1d_c(&tmp[x], 4);

    c = tmp;
    for (int y = 0; y < 4; y++, dst += PXSTRIDE(stride))
        for (int x = 0; x < 4; x++)
            dst[x] = iclip_pixel(dst[x] + *c++);
}

COLD void bitfn(dav1d_itx_dsp_init)(Dav1dInvTxfmDSPContext *const c, int bpc) {
#define assign_itx_all_fn64(w, h, pfx) \
    c->itxfm_add[pfx##TX_##w##X##h][DCT_DCT  ] = \
        inv_txfm_add_dct_dct_##w##x##h##_c

#define assign_itx_all_fn32(w, h, pfx) \
    assign_itx_all_fn64(w, h, pfx); \
    c->itxfm_add[pfx##TX_##w##X##h][IDTX] = \
        inv_txfm_add_identity_identity_##w##x##h##_c

#define assign_itx_all_fn16(w, h, pfx) \
    assign_itx_all_fn32(w, h, pfx); \
    c->itxfm_add[pfx##TX_##w##X##h][DCT_ADST ] = \
        inv_txfm_add_adst_dct_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][ADST_DCT ] = \
        inv_txfm_add_dct_adst_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][ADST_ADST] = \
        inv_txfm_add_adst_adst_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][ADST_FLIPADST] = \
        inv_txfm_add_flipadst_adst_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][FLIPADST_ADST] = \
        inv_txfm_add_adst_flipadst_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][DCT_FLIPADST] = \
        inv_txfm_add_flipadst_dct_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][FLIPADST_DCT] = \
        inv_txfm_add_dct_flipadst_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][FLIPADST_FLIPADST] = \
        inv_txfm_add_flipadst_flipadst_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][H_DCT] = \
        inv_txfm_add_dct_identity_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][V_DCT] = \
        inv_txfm_add_identity_dct_##w##x##h##_c

#define assign_itx_all_fn84(w, h, pfx) \
    assign_itx_all_fn16(w, h, pfx); \
    c->itxfm_add[pfx##TX_##w##X##h][H_FLIPADST] = \
        inv_txfm_add_flipadst_identity_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][V_FLIPADST] = \
        inv_txfm_add_identity_flipadst_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][H_ADST] = \
        inv_txfm_add_adst_identity_##w##x##h##_c; \
    c->itxfm_add[pfx##TX_##w##X##h][V_ADST] = \
        inv_txfm_add_identity_adst_##w##x##h##_c; \

    c->itxfm_add[TX_4X4][WHT_WHT] = inv_txfm_add_wht_wht_4x4_c;
    assign_itx_all_fn84( 4,  4, );
    assign_itx_all_fn84( 4,  8, R);
    assign_itx_all_fn84( 4, 16, R);
    assign_itx_all_fn84( 8,  4, R);
    assign_itx_all_fn84( 8,  8, );
    assign_itx_all_fn84( 8, 16, R);
    assign_itx_all_fn32( 8, 32, R);
    assign_itx_all_fn84(16,  4, R);
    assign_itx_all_fn84(16,  8, R);
    assign_itx_all_fn16(16, 16, );
    assign_itx_all_fn32(16, 32, R);
    assign_itx_all_fn64(16, 64, R);
    assign_itx_all_fn32(32,  8, R);
    assign_itx_all_fn32(32, 16, R);
    assign_itx_all_fn32(32, 32, );
    assign_itx_all_fn64(32, 64, R);
    assign_itx_all_fn64(64, 16, R);
    assign_itx_all_fn64(64, 32, R);
    assign_itx_all_fn64(64, 64, );

#if HAVE_ASM
#if ARCH_AARCH64 || ARCH_ARM
    bitfn(dav1d_itx_dsp_init_arm)(c, bpc);
#endif
#if ARCH_X86
    bitfn(dav1d_itx_dsp_init_x86)(c, bpc);
#endif
#endif
}

/*
 * Copyright © 2024, VideoLAN and dav1d authors
 * Copyright © 2024, Luca Barbato
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
#include "src/ppc/itx.h"
#include "src/ppc/utils.h"

#if BITDEPTH == 8

#define LOAD_4(src, stride, a, b, c, d) \
{  \
    uint8_t *s = src; \
    a = vec_xl(0, s); \
    s += stride; \
    b = vec_xl(0, s); \
    s += stride; \
    c = vec_xl(0, s); \
    s += stride; \
    d = vec_xl(0, s); \
}

#define LOAD_DECLARE_2_I16(src, a, b) \
    i16x8 a = vec_xl(0, src); \
    i16x8 b = vec_xl(0, src + 8);

#define UNPACK_DECLARE_4_I16_I32(sa, sb, a, b, c, d) \
    i32x4 a = i16h_to_i32(sa); \
    i32x4 b = i16l_to_i32(sa); \
    i32x4 c = i16h_to_i32(sb); \
    i32x4 d = i16l_to_i32(sb);

#define LOAD_COEFF_4(coeff) \
    LOAD_DECLARE_2_I16(coeff, c01, c23) \
    UNPACK_DECLARE_4_I16_I32(c01, c23, c0, c1, c2, c3)

#define LOAD_SCALE_COEFF_4x8(coeff, scale) \
    LOAD_DECLARE_2_I16(coeff, c04, c15) \
    LOAD_DECLARE_2_I16(coeff+16, c26, c37) \
    i16x8 c01 = (i16x8)vec_mergeh((i64x2)c04, (i64x2)c15); \
    i16x8 c23 = (i16x8)vec_mergeh((i64x2)c26, (i64x2)c37); \
    i16x8 c45 = (i16x8)vec_mergel((i64x2)c04, (i64x2)c15); \
    i16x8 c67 = (i16x8)vec_mergel((i64x2)c26, (i64x2)c37); \
    c01 = vec_mradds(c01, scale, vec_splat_s16(0)); \
    c23 = vec_mradds(c23, scale, vec_splat_s16(0)); \
    UNPACK_DECLARE_4_I16_I32(c01, c23, c0, c1, c2, c3) \
    c45 = vec_mradds(c45, scale, vec_splat_s16(0)); \
    c67 = vec_mradds(c67, scale, vec_splat_s16(0)); \
    UNPACK_DECLARE_4_I16_I32(c45, c67, c4, c5, c6, c7)

#define LOAD_SCALE_COEFF_8x4(coeff, scale) \
    LOAD_DECLARE_2_I16(coeff, c01, c23) \
    LOAD_DECLARE_2_I16(coeff+16, c45, c67) \
    c01 = vec_mradds(c01, scale, vec_splat_s16(0)); \
    c23 = vec_mradds(c23, scale, vec_splat_s16(0)); \
    UNPACK_DECLARE_4_I16_I32(c01, c23, c0, c1, c2, c3) \
    c45 = vec_mradds(c45, scale, vec_splat_s16(0)); \
    c67 = vec_mradds(c67, scale, vec_splat_s16(0)); \
    UNPACK_DECLARE_4_I16_I32(c45, c67, c4, c5, c6, c7)

#define LOAD_COEFF_8x8(coeff) \
    LOAD_DECLARE_2_I16(coeff, c0, c1) \
    LOAD_DECLARE_2_I16(coeff+16, c2, c3) \
    LOAD_DECLARE_2_I16(coeff+32, c4, c5) \
    LOAD_DECLARE_2_I16(coeff+48, c6, c7) \
    UNPACK_DECLARE_4_I16_I32(c0, c1, c0h, c0l, c1h, c1l) \
    UNPACK_DECLARE_4_I16_I32(c2, c3, c2h, c2l, c3h, c3l) \
    UNPACK_DECLARE_4_I16_I32(c4, c5, c4h, c4l, c5h, c5l) \
    UNPACK_DECLARE_4_I16_I32(c6, c7, c6h, c6l, c7h, c7l) \

#define LOAD_COEFF_4x16(coeff) \
    LOAD_DECLARE_2_I16(coeff,    a0b0, c0d0) \
    LOAD_DECLARE_2_I16(coeff+16, a1b1, c1d1) \
    LOAD_DECLARE_2_I16(coeff+32, a2b2, c2d2) \
    LOAD_DECLARE_2_I16(coeff+48, a3b3, c3d3) \
    UNPACK_DECLARE_4_I16_I32(a0b0, c0d0, cA0, cB0, cC0, cD0) \
    UNPACK_DECLARE_4_I16_I32(a1b1, c1d1, cA1, cB1, cC1, cD1) \
    UNPACK_DECLARE_4_I16_I32(a2b2, c2d2, cA2, cB2, cC2, cD2) \
    UNPACK_DECLARE_4_I16_I32(a3b3, c3d3, cA3, cB3, cC3, cD3)

#define LOAD_DECLARE_4(src, stride, a, b, c, d) \
    u8x16 a, b, c, d; \
    LOAD_4(src, stride, a, b, c, d)

#define STORE_LEN(l, dst, stride, a, b, c, d) \
{ \
    uint8_t *dst2 = dst; \
    vec_xst_len(a, dst2, l); \
    dst2 += stride; \
    vec_xst_len(b, dst2, l); \
    dst2 += stride; \
    vec_xst_len(c, dst2, l); \
    dst2 += stride; \
    vec_xst_len(d, dst2, l); \
}

#define STORE_4(dst, stride, a, b, c, d) \
    STORE_LEN(4, dst, stride, a, b, c, d)

#define STORE_8(dst, stride, ab, cd, ef, gh) \
    STORE_LEN(8, dst, stride, ab, cd, ef, gh)

#define STORE_16(dst, stride, l0, l1, l2, l3) \
{ \
    uint8_t *dst##2 = dst; \
    vec_xst(l0, 0, dst##2); \
    dst##2 += stride; \
    vec_xst(l1, 0, dst##2); \
    dst##2 += stride; \
    vec_xst(l2, 0, dst##2); \
    dst##2 += stride; \
    vec_xst(l3, 0, dst##2); \
}

#define APPLY_COEFF_4(a, b, c, d, c01, c23) \
{ \
    u8x16 ab = (u8x16)vec_mergeh((u32x4)a, (u32x4)b); \
    u8x16 cd = (u8x16)vec_mergeh((u32x4)c, (u32x4)d); \
 \
    c01 = vec_adds(c01, vec_splat_s16(8)); \
    c23 = vec_adds(c23, vec_splat_s16(8)); \
    c01 = vec_sra(c01, vec_splat_u16(4)); \
    c23 = vec_sra(c23, vec_splat_u16(4)); \
 \
    i16x8 abs = u8h_to_i16(ab); \
    i16x8 cds = u8h_to_i16(cd); \
 \
    abs = vec_adds(abs, c01); \
    cds = vec_adds(cds, c23); \
 \
    a = vec_packsu(abs, abs); \
    c = vec_packsu(cds, cds); \
 \
    b = (u8x16)vec_mergeo((u32x4)a, (u32x4)a); \
    d = (u8x16)vec_mergeo((u32x4)c, (u32x4)c); \
}

#define APPLY_COEFF_8x4(ab, cd, c01, c23) \
{ \
    i16x8 abs = u8h_to_i16(ab); \
    i16x8 cds = u8h_to_i16(cd); \
    c01 = vec_adds(c01, vec_splat_s16(8)); \
    c23 = vec_adds(c23, vec_splat_s16(8)); \
    c01 = vec_sra(c01, vec_splat_u16(4)); \
    c23 = vec_sra(c23, vec_splat_u16(4)); \
 \
    abs = vec_adds(abs, c01); \
    cds = vec_adds(cds, c23); \
 \
    ab = vec_packsu(abs, abs); \
    cd = vec_packsu(cds, cds); \
}

#define APPLY_COEFF_16x4(a, b, c, d, \
                         c00c01, c02c03, c04c05, c06c07, \
                         c08c09, c10c11, c12c13, c14c15) \
{ \
    i16x8 ah = u8h_to_i16(a); \
    i16x8 al = u8l_to_i16(a); \
    i16x8 bh = u8h_to_i16(b); \
    i16x8 bl = u8l_to_i16(b); \
    i16x8 ch = u8h_to_i16(c); \
    i16x8 cl = u8l_to_i16(c); \
    i16x8 dh = u8h_to_i16(d); \
    i16x8 dl = u8l_to_i16(d); \
    SCALE_ROUND_4(c00c01, c02c03, c04c05, c06c07, vec_splat_s16(8), vec_splat_u16(4)) \
    SCALE_ROUND_4(c08c09, c10c11, c12c13, c14c15, vec_splat_s16(8), vec_splat_u16(4)) \
 \
    ah = vec_adds(ah, c00c01); \
    al = vec_adds(al, c02c03); \
    bh = vec_adds(bh, c04c05); \
    bl = vec_adds(bl, c06c07); \
    ch = vec_adds(ch, c08c09); \
    cl = vec_adds(cl, c10c11); \
    dh = vec_adds(dh, c12c13); \
    dl = vec_adds(dl, c14c15); \
 \
    a = vec_packsu(ah, al); \
    b = vec_packsu(bh, bl); \
    c = vec_packsu(ch, cl); \
    d = vec_packsu(dh, dl); \
}

#define IDCT_4_INNER(c0, c1, c2, c3) \
{ \
    i32x4 o0 = vec_add(c0, c2); \
    i32x4 o1 = vec_sub(c0, c2); \
 \
    i32x4 v2896 = vec_splats(2896); \
    i32x4 v1567 = vec_splats(1567); \
    i32x4 v3784 = vec_splats(3784); \
    i32x4 v2048 = vec_splats(2048); \
 \
    o0 = vec_mul(o0, v2896); \
    o1 = vec_mul(o1, v2896); \
 \
    i32x4 o2a = vec_mul(c1, v1567); \
    i32x4 o2b = vec_mul(c3, v3784); \
    i32x4 o3a = vec_mul(c1, v3784); \
    i32x4 o3b = vec_mul(c3, v1567); \
 \
    i32x4 o2 = vec_sub(o2a, o2b); \
    i32x4 o3 = vec_add(o3a, o3b); \
 \
    u32x4 v12 = vec_splat_u32(12); \
 \
    o0 = vec_add(o0, v2048); \
    o1 = vec_add(o1, v2048); \
    o2 = vec_add(o2, v2048); \
    o3 = vec_add(o3, v2048); \
 \
    o0 = vec_sra(o0, v12); \
    o1 = vec_sra(o1, v12); \
    o2 = vec_sra(o2, v12); \
    o3 = vec_sra(o3, v12); \
 \
    c0 = vec_add(o0, o3); \
    c1 = vec_add(o1, o2); \
    c2 = vec_sub(o1, o2); \
    c3 = vec_sub(o0, o3); \
 \
}

#define dct4_for_dct8(c0, c1, c2, c3, c03, c12) \
    IDCT_4_INNER(c0, c1, c2, c3) \
    c03 = vec_packs(c0, c3); \
    c12 = vec_packs(c1, c2); \

#define dct_4_in(c0, c1, c2, c3, c01, c23) \
{ \
    IDCT_4_INNER(c0, c1, c2, c3) \
    c01 = vec_packs(c0, c1); \
    c23 = vec_packs(c2, c3); \
    c0 = i16h_to_i32(c01); \
    c1 = i16l_to_i32(c01); \
    c2 = i16h_to_i32(c23); \
    c3 = i16l_to_i32(c23); \
}

#define dct_4_out(c0, c1, c2, c3, c01, c23) \
    IDCT_4_INNER(c0, c1, c2, c3) \
    c01 = vec_packs(c0, c1); \
    c23 = vec_packs(c2, c3); \


#define IDENTITY_4(c01, c23) \
{ \
    i16x8 v1697 = vec_splats((int16_t)(1697*8)); \
    i16x8 o01 = vec_mradds(c01, v1697, vec_splat_s16(0)); \
    i16x8 o23 = vec_mradds(c23, v1697, vec_splat_s16(0)); \
    c01 = vec_adds(c01, o01); \
    c23 = vec_adds(c23, o23); \
}

#define identity_4_in(c0, c1, c2, c3, c01, c23) \
{ \
    IDENTITY_4(c01, c23) \
    c0 = i16h_to_i32(c01); \
    c1 = i16l_to_i32(c01); \
    c2 = i16h_to_i32(c23); \
    c3 = i16l_to_i32(c23); \
}

#define identity_4_out(c0, c1, c2, c3, c01, c23) \
{ \
    c01 = vec_packs(c0, c1); \
    c23 = vec_packs(c2, c3); \
    IDENTITY_4(c01, c23) \
}

#define ADST_INNER_4(c0, c1, c2, c3, oc0, oc1, oc2, oc3) \
{ \
    i32x4 v1321 = vec_splats(1321); \
    i32x4 v3803 = vec_splats(3803); \
    i32x4 v2482 = vec_splats(2482); \
    i32x4 v3344 = vec_splats(3344); \
    i32x4 v2048 = vec_splats(2048); \
    i32x4 i0_v1321 = vec_mul(c0, v1321); \
    i32x4 i0_v2482 = vec_mul(c0, v2482); \
    i32x4 i0_v3803 = vec_mul(c0, v3803); \
    i32x4 i1 = vec_mul(c1, v3344); \
    i32x4 i2_v1321 = vec_mul(c2, v1321); \
    i32x4 i2_v2482 = vec_mul(c2, v2482); \
    i32x4 i2_v3803 = vec_mul(c2, v3803); \
    i32x4 i3_v1321 = vec_mul(c3, v1321); \
    i32x4 i3_v2482 = vec_mul(c3, v2482); \
    i32x4 i3_v3803 = vec_mul(c3, v3803); \
 \
    i32x4 n1 = vec_sub(i1, v2048); \
    i1 = vec_add(i1, v2048); \
 \
 \
    i32x4 o0 = vec_add(i0_v1321, i2_v3803); \
    i32x4 o1 = vec_sub(i0_v2482, i2_v1321); \
    i32x4 o2 = vec_sub(c0, c2); \
    i32x4 o3 = vec_add(i0_v3803, i2_v2482); \
 \
    o0 = vec_add(o0, i3_v2482); \
    o1 = vec_sub(o1, i3_v3803); \
    o2 = vec_add(o2, c3); \
    o3 = vec_sub(o3, i3_v1321); \
 \
    o0 = vec_add(o0, i1); \
    o1 = vec_add(o1, i1); \
    o2 = vec_mul(o2, v3344); \
    o3 = vec_sub(o3, n1); \
 \
    o2 = vec_add(o2, v2048); \
 \
    oc0 = vec_sra(o0, vec_splat_u32(12)); \
    oc1 = vec_sra(o1, vec_splat_u32(12)); \
    oc2 = vec_sra(o2, vec_splat_u32(12)); \
    oc3 = vec_sra(o3, vec_splat_u32(12)); \
}

#define adst_4_in(c0, c1, c2, c3, c01, c23) \
{ \
    ADST_INNER_4(c0, c1, c2, c3, c0, c1, c2, c3) \
}

#define flipadst_4_in(c0, c1, c2, c3, c01, c23) \
{ \
    ADST_INNER_4(c0, c1, c2, c3, c3, c2, c1, c0) \
}

#define adst_4_out(c0, c1, c2, c3, c01, c23) \
{ \
    ADST_INNER_4(c0, c1, c2, c3, c0, c1, c2, c3) \
    c01 = vec_packs(c0, c1); \
    c23 = vec_packs(c2, c3); \
}

#define flipadst_4_out(c0, c1, c2, c3, c01, c23) \
{ \
    ADST_INNER_4(c0, c1, c2, c3, c3, c2, c1, c0) \
    c01 = vec_packs(c0, c1); \
    c23 = vec_packs(c2, c3); \
}

static void dc_only_4xN(uint8_t *dst, const ptrdiff_t stride, int16_t *const coeff, int n, int is_rect2, int shift)
{
    int dc = coeff[0];
    const int rnd = (1 << shift) >> 1;
    if (is_rect2)
        dc = (dc * 181 + 128) >> 8;
    dc = (dc * 181 + 128) >> 8;
    dc = (dc + rnd) >> shift;
    dc = (dc * 181 + 128 + 2048) >> 12;

    i16x8 vdc = vec_splats((int16_t)dc);
    coeff[0] = 0;
    for (int i = 0; i < n; i++, dst += 4 * stride) {
        LOAD_DECLARE_4(dst, stride, a, b, c, d)

        i16x8 as = u8h_to_i16(a);
        i16x8 bs = u8h_to_i16(b);
        i16x8 cs = u8h_to_i16(c);
        i16x8 ds = u8h_to_i16(d);

        as = vec_adds(as, vdc);
        bs = vec_adds(bs, vdc);
        cs = vec_adds(cs, vdc);
        ds = vec_adds(ds, vdc);

        a = vec_packsu(as, as);
        b = vec_packsu(bs, bs);
        c = vec_packsu(cs, cs);
        d = vec_packsu(ds, ds);

        STORE_4(dst, stride, a, b, c, d)
    }
}

static void dc_only_8xN(uint8_t *dst, const ptrdiff_t stride, int16_t *const coeff, int n, int is_rect2, int shift)
{
    int dc = coeff[0];
    const int rnd = (1 << shift) >> 1;
    if (is_rect2)
        dc = (dc * 181 + 128) >> 8;
    dc = (dc * 181 + 128) >> 8;
    dc = (dc + rnd) >> shift;
    dc = (dc * 181 + 128 + 2048) >> 12;

    i16x8 vdc = vec_splats((int16_t)dc);
    coeff[0] = 0;

    for (int i = 0; i < n; i++, dst += 4 * stride) {
        LOAD_DECLARE_4(dst, stride, a, b, c, d)

        i16x8 as = u8h_to_i16(a);
        i16x8 bs = u8h_to_i16(b);
        i16x8 cs = u8h_to_i16(c);
        i16x8 ds = u8h_to_i16(d);

        as = vec_adds(as, vdc);
        bs = vec_adds(bs, vdc);
        cs = vec_adds(cs, vdc);
        ds = vec_adds(ds, vdc);

        a = vec_packsu(as, as);
        b = vec_packsu(bs, bs);
        c = vec_packsu(cs, cs);
        d = vec_packsu(ds, ds);

        STORE_8(dst, stride, a, b, c, d)
    }
}

static void dc_only_16xN(uint8_t *dst, const ptrdiff_t stride, int16_t *const coeff, int n, int is_rect2, int shift)
{
    int dc = coeff[0];
    const int rnd = (1 << shift) >> 1;
    if (is_rect2)
        dc = (dc * 181 + 128) >> 8;
    dc = (dc * 181 + 128) >> 8;
    dc = (dc + rnd) >> shift;
    dc = (dc * 181 + 128 + 2048) >> 12;

    i16x8 vdc = vec_splats((int16_t)dc);
    coeff[0] = 0;

    for (int i = 0; i < n; i++, dst += 4 * stride) {
        LOAD_DECLARE_4(dst, stride, a, b, c, d)

        i16x8 ah = u8h_to_i16(a);
        i16x8 bh = u8h_to_i16(b);
        i16x8 ch = u8h_to_i16(c);
        i16x8 dh = u8h_to_i16(d);
        i16x8 al = u8l_to_i16(a);
        i16x8 bl = u8l_to_i16(b);
        i16x8 cl = u8l_to_i16(c);
        i16x8 dl = u8l_to_i16(d);

        ah = vec_adds(ah, vdc);
        bh = vec_adds(bh, vdc);
        ch = vec_adds(ch, vdc);
        dh = vec_adds(dh, vdc);
        al = vec_adds(al, vdc);
        bl = vec_adds(bl, vdc);
        cl = vec_adds(cl, vdc);
        dl = vec_adds(dl, vdc);

        a = vec_packsu(ah, al);
        b = vec_packsu(bh, bl);
        c = vec_packsu(ch, cl);
        d = vec_packsu(dh, dl);

        STORE_16(dst, stride, a, b, c, d)
    }
}

void dav1d_inv_txfm_add_dct_dct_4x4_8bpc_pwr9(uint8_t *dst, const ptrdiff_t stride,
                                              int16_t *const coeff, const int eob)
{
    assert(eob >= 0);

    if (eob < 1) {
        return dc_only_4xN(dst, stride, coeff, 1, 0, 0);
    }

    LOAD_COEFF_4(coeff)

    dct_4_in(c0, c1, c2, c3, c01, c23)

    TRANSPOSE4_I32(c0, c1, c2, c3)

    memset(coeff, 0, sizeof(*coeff) * 4 * 4);

    dct_4_out(c0, c1, c2, c3, c01, c23)

    LOAD_DECLARE_4(dst, stride, a, b, c, d)

    APPLY_COEFF_4(a, b, c, d, c01, c23)

    STORE_4(dst, stride, a, b, c, d)
}

void dav1d_inv_txfm_add_wht_wht_4x4_8bpc_pwr9(pixel *dst, const ptrdiff_t stride,
                                              coef *const coeff, const int eob)
{
    LOAD_COEFF_4(coeff)

    u32x4 v2 = vec_splat_u32(2);

    c0 = vec_sra(c0, v2);
    c1 = vec_sra(c1, v2);
    c2 = vec_sra(c2, v2);
    c3 = vec_sra(c3, v2);

    i32x4 t0 = vec_add(c0, c1);
    i32x4 t2 = vec_sub(c2, c3);
    i32x4 t4 = vec_sra(vec_sub(t0, t2), vec_splat_u32(1));
    i32x4 t3 = vec_sub(t4, c3);
    i32x4 t1 = vec_sub(t4, c1);
    c0 = vec_sub(t0, t3);
    c1 = t3;
    c2 = t1;
    c3 = vec_add(t2, t1);

    memset(coeff, 0, sizeof(*coeff) * 4 * 4);

    TRANSPOSE4_I32(c0, c1, c2, c3)

    t0 = vec_add(c0, c1);
    t2 = vec_sub(c2, c3);
    t4 = vec_sra(vec_sub(t0, t2), vec_splat_u32(1));
    t3 = vec_sub(t4, c3);
    t1 = vec_sub(t4, c1);
    c0 = vec_sub(t0, t3);
    c1 = t3;
    c2 = t1;
    c3 = vec_add(t2, t1);

    c01 = vec_packs(c0, c1);
    c23 = vec_packs(c2, c3);

    LOAD_DECLARE_4(dst, stride, a, b, c, d)

    u8x16 ab = (u8x16)vec_mergeh((u32x4)a, (u32x4)b);
    u8x16 cd = (u8x16)vec_mergeh((u32x4)c, (u32x4)d);

    i16x8 abs = u8h_to_i16(ab);
    i16x8 cds = u8h_to_i16(cd);

    abs = vec_adds(abs, c01);
    cds = vec_adds(cds, c23);

    a = vec_packsu(abs, abs);
    c = vec_packsu(cds, cds);

    b = (u8x16)vec_mergeo((u32x4)a, (u32x4)a);
    d = (u8x16)vec_mergeo((u32x4)c, (u32x4)c);

    STORE_4(dst, stride, a, b, c, d)
}

#define inv_txfm_fn4x4(type1, type2) \
void dav1d_inv_txfm_add_##type1##_##type2##_4x4_8bpc_pwr9(uint8_t *dst, const ptrdiff_t stride, \
                                                          int16_t *const coeff, const int eob) \
{ \
    LOAD_COEFF_4(coeff) \
    type1##_4_in(c0, c1, c2, c3, c01, c23) \
    memset(coeff, 0, sizeof(*coeff) * 4 * 4); \
    TRANSPOSE4_I32(c0, c1, c2, c3) \
    type2##_4_out(c0, c1, c2, c3, c01, c23) \
    LOAD_DECLARE_4(dst, stride, a, b, c, d) \
    APPLY_COEFF_4(a, b, c, d, c01, c23) \
    STORE_4(dst, stride, a, b, c, d) \
}

inv_txfm_fn4x4(adst,     dct     )
inv_txfm_fn4x4(dct,      adst    )
inv_txfm_fn4x4(dct,      flipadst)
inv_txfm_fn4x4(flipadst, dct     )
inv_txfm_fn4x4(adst,     flipadst)
inv_txfm_fn4x4(flipadst, adst    )
inv_txfm_fn4x4(identity, dct     )
inv_txfm_fn4x4(dct,      identity)
inv_txfm_fn4x4(identity, flipadst)
inv_txfm_fn4x4(flipadst, identity)
inv_txfm_fn4x4(identity, adst   )
inv_txfm_fn4x4(adst,     identity)
inv_txfm_fn4x4(identity, identity)
inv_txfm_fn4x4(adst,     adst    )
inv_txfm_fn4x4(flipadst, flipadst)


#define IDCT_8_INNER(c0, c1, c2, c3, c4, c5, c6, c7, c03, c12, c74, c65) \
    dct4_for_dct8(c0, c2, c4, c6, c03, c12) \
 \
    i32x4 v799 = vec_splats(799); \
    i32x4 v4017 = vec_splats(4017); \
    i32x4 v3406 = vec_splats(3406); \
    i32x4 v2276 = vec_splats(2276); \
    i32x4 v2048 = vec_splats(2048); \
    u32x4 v12 = vec_splat_u32(12); \
 \
    i32x4 c1v799 = vec_mul(c1, v799); \
    i32x4 c7v4017 = vec_mul(c7, v4017); \
    i32x4 c5v3406 = vec_mul(c5, v3406); \
    i32x4 c3v2276 = vec_mul(c3, v2276); \
    i32x4 c5v2276 = vec_mul(c5, v2276); \
    i32x4 c3v3406 = vec_mul(c3, v3406); \
    i32x4 c1v4017 = vec_mul(c1, v4017); \
    i32x4 c7v799 = vec_mul(c7, v799); \
 \
    i32x4 t4a = vec_subs(c1v799, c7v4017); \
    i32x4 t5a = vec_subs(c5v3406, c3v2276); \
    i32x4 t6a = vec_adds(c5v2276, c3v3406); \
    i32x4 t7a = vec_adds(c1v4017, c7v799); \
 \
    t4a = vec_adds(t4a, v2048); \
    t5a = vec_adds(t5a, v2048); \
    t6a = vec_adds(t6a, v2048); \
    t7a = vec_adds(t7a, v2048); \
 \
    t4a = vec_sra(t4a, v12); \
    t7a = vec_sra(t7a, v12); \
    t5a = vec_sra(t5a, v12); \
    t6a = vec_sra(t6a, v12); \
 \
    i16x8 t7at4a = vec_packs(t7a, t4a); \
    i16x8 t6at5a = vec_packs(t6a, t5a); \
 \
    i16x8 t7t4 = vec_adds(t7at4a, t6at5a); \
    t6at5a = vec_subs(t7at4a, t6at5a); \
 \
    t6a = i16h_to_i32(t6at5a); \
    t5a = i16l_to_i32(t6at5a); \
 \
    i32x4 t6 = vec_add(t6a, t5a); \
    i32x4 t5 = vec_sub(t6a, t5a); \
 \
    t6 = vec_mul(t6, vec_splats(181)); \
    t5 = vec_mul(t5, vec_splats(181)); \
    t6 = vec_add(t6, vec_splats(128)); \
    t5 = vec_add(t5, vec_splats(128)); \
 \
    t6 = vec_sra(t6, vec_splat_u32(8)); \
    t5 = vec_sra(t5, vec_splat_u32(8)); \
 \
    i16x8 t6t5 = vec_packs(t6, t5); \
 \
    c74 = vec_subs(c03, t7t4); \
    c65 = vec_subs(c12, t6t5); \
    c03 = vec_adds(c03, t7t4); \
    c12 = vec_adds(c12, t6t5); \

#define UNPACK_4_I16_I32(t0, t1, t2, t3) \
    t0 = i16h_to_i32(t0##t1); \
    t1 = i16l_to_i32(t0##t1); \
    t2 = i16h_to_i32(t2##t3); \
    t3 = i16l_to_i32(t2##t3);

#define UNPACK_PAIR_I16_I32(hi, lo, v) \
    hi = i16h_to_i32(v); \
    lo = i16l_to_i32(v); \


#define dct_8_in(c0, c1, c2, c3, c4, c5, c6, c7, ...) \
{ \
    i16x8 c0##c3, c1##c2, c7##c4, c6##c5; \
    IDCT_8_INNER(c0, c1, c2, c3, c4, c5, c6, c7, c0##c3, c1##c2, c7##c4, c6##c5) \
    UNPACK_4_I16_I32(c0, c3, c1, c2) \
    UNPACK_4_I16_I32(c7, c4, c6, c5) \
}

#define dct_8_out(c0, c1, c2, c3, c4, c5, c6, c7, c01, c23, c45, c67) \
{ \
    i16x8 c03, c12, c74, c65; \
    IDCT_8_INNER(c0, c1, c2, c3, c4, c5, c6, c7, c03, c12, c74, c65) \
    c01 = (i16x8)vec_mergeh((u64x2)c03, (u64x2)c12); \
    c23 = (i16x8)vec_mergel((u64x2)c12, (u64x2)c03); \
    c45 = (i16x8)vec_mergel((u64x2)c74, (u64x2)c65); \
    c67 = (i16x8)vec_mergeh((u64x2)c65, (u64x2)c74); \
}

#define dct_8x2_in(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                   c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, \
                   c0, c1, c2, c3, c4, c5, c6, c7) \
{ \
    dct_8_in(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h,) \
    dct_8_in(c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l,) \
}

#define dct_8x2_out(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                    c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, \
                    c0, c1, c2, c3, c4, c5, c6, c7) \
{ \
    i16x8 c03h, c12h, c74h, c65h; \
    i16x8 c03l, c12l, c74l, c65l; \
    { \
        IDCT_8_INNER(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, c03h, c12h, c74h, c65h) \
    } \
    { \
        IDCT_8_INNER(c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, c03l, c12l, c74l, c65l) \
    } \
    c0 = (i16x8)vec_mergeh((u64x2)c03h, (u64x2)c03l); \
    c3 = (i16x8)vec_mergel((u64x2)c03h, (u64x2)c03l); \
    c1 = (i16x8)vec_mergeh((u64x2)c12h, (u64x2)c12l); \
    c2 = (i16x8)vec_mergel((u64x2)c12h, (u64x2)c12l); \
    c7 = (i16x8)vec_mergeh((u64x2)c74h, (u64x2)c74l); \
    c4 = (i16x8)vec_mergel((u64x2)c74h, (u64x2)c74l); \
    c6 = (i16x8)vec_mergeh((u64x2)c65h, (u64x2)c65l); \
    c5 = (i16x8)vec_mergel((u64x2)c65h, (u64x2)c65l); \
}

#define IDENTITY_8(c01, c23, c45, c67) \
{ \
    c01 = vec_adds(c01, c01); \
    c23 = vec_adds(c23, c23); \
    c45 = vec_adds(c45, c45); \
    c67 = vec_adds(c67, c67); \
}

#define identity_8_in(c0, c1, c2, c3, c4, c5, c6, c7, c01, c23, c45, c67) \
{ \
    IDENTITY_8(c01, c23, c45, c67) \
    UNPACK_PAIR_I16_I32(c0, c1, c01) \
    UNPACK_PAIR_I16_I32(c2, c3, c23) \
    UNPACK_PAIR_I16_I32(c4, c5, c45) \
    UNPACK_PAIR_I16_I32(c6, c7, c67) \
}

#define identity_8_out(c0, c1, c2, c3, c4, c5, c6, c7, c01, c23, c45, c67) \
    c01 = vec_packs(c0, c1); \
    c23 = vec_packs(c2, c3); \
    c45 = vec_packs(c4, c5); \
    c67 = vec_packs(c6, c7); \
    IDENTITY_8(c01, c23, c45, c67)

#define identity_8x2_in(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                        c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, \
                        c0, c1, c2, c3, c4, c5, c6, c7) \
{ \
    IDENTITY_8(c0, c1, c2, c3) \
    IDENTITY_8(c4, c5, c6, c7) \
    UNPACK_PAIR_I16_I32(c0h, c0l, c0) \
    UNPACK_PAIR_I16_I32(c1h, c1l, c1) \
    UNPACK_PAIR_I16_I32(c2h, c2l, c2) \
    UNPACK_PAIR_I16_I32(c3h, c3l, c3) \
    UNPACK_PAIR_I16_I32(c4h, c4l, c4) \
    UNPACK_PAIR_I16_I32(c5h, c5l, c5) \
    UNPACK_PAIR_I16_I32(c6h, c6l, c6) \
    UNPACK_PAIR_I16_I32(c7h, c7l, c7) \
}

#define PACK_4(c0, c1, c2, c3, \
               c0h, c1h, c2h, c3h, \
               c0l, c1l, c2l, c3l) \
{ \
    c0 = vec_packs(c0h, c0l); \
    c1 = vec_packs(c1h, c1l); \
    c2 = vec_packs(c2h, c2l); \
    c3 = vec_packs(c3h, c3l); \
}

#define DECLARE_PACK_4(c0, c1, c2, c3, \
                       c0h, c1h, c2h, c3h, \
                       c0l, c1l, c2l, c3l) \
    i16x8 c0, c1, c2, c3; \
    PACK_4(c0, c1, c2, c3, c0h, c1h, c2h, c3h, c0l, c1l, c2l, c3l);

#define PACK_8(c0, c1, c2, c3, c4, c5, c6, c7, \
               c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
               c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l) \
{ \
    c0 = vec_packs(c0h, c0l); \
    c1 = vec_packs(c1h, c1l); \
    c2 = vec_packs(c2h, c2l); \
    c3 = vec_packs(c3h, c3l); \
    c4 = vec_packs(c4h, c4l); \
    c5 = vec_packs(c5h, c5l); \
    c6 = vec_packs(c6h, c6l); \
    c7 = vec_packs(c7h, c7l); \
}

#define identity_8x2_out(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                         c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, \
                         c0, c1, c2, c3, c4, c5, c6, c7) \
{ \
    PACK_8(c0, c1, c2, c3, c4, c5, c6, c7, \
           c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
           c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l) \
    IDENTITY_8(c0, c1, c2, c3) \
    IDENTITY_8(c4, c5, c6, c7) \
}

#define DECLARE_SPLAT_I32(val) \
    i32x4 v##val = vec_splats(val);

#define DECLARE_MUL_PAIR_I32(ca, cb, va, vb) \
    i32x4 ca##va = vec_mul(ca, va); \
    i32x4 cb##vb = vec_mul(cb, vb); \
    i32x4 ca##vb = vec_mul(ca, vb); \
    i32x4 cb##va = vec_mul(cb, va);

#define ADD_SUB_PAIR(r0, r1, ca, cb, va, vb) \
    r0 = vec_adds(ca##va, cb##vb); \
    r1 = vec_subs(ca##vb, cb##va);

#define DECLARE_ADD_SUB_PAIR(r0, r1, ca, cb, va, vb) \
    i32x4 r0, r1; \
    ADD_SUB_PAIR(r0, r1, ca, cb, va, vb)

#define SCALE_ROUND_4(a, b, c, d, rnd, shift) \
    a = vec_adds(a, rnd); \
    b = vec_adds(b, rnd); \
    c = vec_adds(c, rnd); \
    d = vec_adds(d, rnd); \
    a = vec_sra(a, shift); \
    b = vec_sra(b, shift); \
    c = vec_sra(c, shift); \
    d = vec_sra(d, shift);

#define ADST_INNER_8(c0, c1, c2, c3, c4, c5, c6, c7, \
                     o0, o1, o2, o3, o4, o5, o6, o7) \
{ \
    DECLARE_SPLAT_I32(4076) \
    DECLARE_SPLAT_I32(401) \
 \
    DECLARE_SPLAT_I32(3612) \
    DECLARE_SPLAT_I32(1931) \
 \
    DECLARE_SPLAT_I32(2598) \
    DECLARE_SPLAT_I32(3166) \
 \
    DECLARE_SPLAT_I32(1189) \
    DECLARE_SPLAT_I32(3920) \
 \
    DECLARE_SPLAT_I32(3784) \
    DECLARE_SPLAT_I32(1567) \
 \
    DECLARE_SPLAT_I32(2048) \
    u32x4 v12 = vec_splat_u32(12); \
 \
    DECLARE_MUL_PAIR_I32(c7, c0, v4076, v401) \
    DECLARE_MUL_PAIR_I32(c5, c2, v3612, v1931) \
    DECLARE_MUL_PAIR_I32(c3, c4, v2598, v3166) \
    DECLARE_MUL_PAIR_I32(c1, c6, v1189, v3920) \
 \
    DECLARE_ADD_SUB_PAIR(t0a, t1a, c7, c0, v4076, v401) \
    DECLARE_ADD_SUB_PAIR(t2a, t3a, c5, c2, v3612, v1931) \
    DECLARE_ADD_SUB_PAIR(t4a, t5a, c3, c4, v2598, v3166) \
    DECLARE_ADD_SUB_PAIR(t6a, t7a, c1, c6, v1189, v3920) \
 \
    SCALE_ROUND_4(t0a, t1a, t2a, t3a, v2048, v12) \
    SCALE_ROUND_4(t4a, t5a, t6a, t7a, v2048, v12) \
 \
    i32x4 t0 = vec_add(t0a, t4a); \
    i32x4 t1 = vec_add(t1a, t5a); \
    i32x4 t2 = vec_add(t2a, t6a); \
    i32x4 t3 = vec_add(t3a, t7a); \
    i32x4 t4 = vec_sub(t0a, t4a); \
    i32x4 t5 = vec_sub(t1a, t5a); \
    i32x4 t6 = vec_sub(t2a, t6a); \
    i32x4 t7 = vec_sub(t3a, t7a); \
 \
    i16x8 t0t1 = vec_packs(t0, t1); \
    i16x8 t2t3 = vec_packs(t2, t3); \
    i16x8 t4t5 = vec_packs(t4, t5); \
    i16x8 t6t7 = vec_packs(t6, t7); \
 \
    UNPACK_4_I16_I32(t4, t5, t6, t7) \
    UNPACK_4_I16_I32(t0, t1, t2, t3) \
 \
    DECLARE_MUL_PAIR_I32(t4, t5, v3784, v1567) \
    DECLARE_MUL_PAIR_I32(t7, t6, v3784, v1567) \
 \
    ADD_SUB_PAIR(t4a, t5a, t4, t5, v3784, v1567) \
    ADD_SUB_PAIR(t7a, t6a, t7, t6, v1567, v3784) \
 \
    SCALE_ROUND_4(t4a, t5a, t6a, t7a, v2048, v12) \
  \
    o0 = vec_add(t0, t2); \
    o1 = vec_add(t4a, t6a); \
    o7 = vec_add(t1, t3); \
    o6 = vec_add(t5a, t7a); \
    t2 = vec_sub(t0, t2); \
    t3 = vec_sub(t1, t3); \
    t6 = vec_sub(t4a, t6a); \
    t7 = vec_sub(t5a, t7a); \
 \
    i16x8 o7##o1 = vec_packs(o7, o1); \
    i16x8 o0##o6 = vec_packs(o0, o6); \
    t2t3 = vec_packs(t2, t3); \
    t6t7 = vec_packs(t6, t7); \
 \
    UNPACK_4_I16_I32(t2, t3, t6, t7) \
    UNPACK_4_I16_I32(o7, o1, o0, o6) \
 \
    o7 = -o7; \
    o1 = -o1; \
 \
    o3 = vec_add(t2, t3); \
    o4 = vec_sub(t2, t3); \
    o5 = vec_sub(t6, t7); \
    o2 = vec_add(t6, t7); \
 \
    i32x4 v181 = vec_splats(181); \
    i32x4 v128 = vec_splats(128); \
    u32x4 v8 = vec_splat_u32(8); \
 \
    o2 = vec_mul(o2, v181); \
    o3 = vec_mul(o3, v181); \
    o4 = vec_mul(o4, v181); \
    o5 = vec_mul(o5, v181); \
 \
    SCALE_ROUND_4(o2, o3, o4, o5, v128, v8) \
 \
    o3 = -o3; \
    o5 = -o5; \
}

#define adst_8_in(c0, c1, c2, c3, c4, c5, c6, c7, c01, c23, c45, c67) \
{\
    ADST_INNER_8(c0, c1, c2, c3, c4, c5, c6, c7, \
                 c0, c1, c2, c3, c4, c5, c6, c7) \
    c01 = vec_packs(c0, c1); \
    c23 = vec_packs(c2, c3); \
    c45 = vec_packs(c4, c5); \
    c67 = vec_packs(c6, c7); \
    UNPACK_PAIR_I16_I32(c0, c1, c01) \
    UNPACK_PAIR_I16_I32(c2, c3, c23) \
    UNPACK_PAIR_I16_I32(c4, c5, c45) \
    UNPACK_PAIR_I16_I32(c6, c7, c67) \
}

#define adst_8_out(c0, c1, c2, c3, c4, c5, c6, c7, c01, c23, c45, c67) \
{\
    ADST_INNER_8(c0, c1, c2, c3, c4, c5, c6, c7, \
                 c0, c1, c2, c3, c4, c5, c6, c7) \
    c01 = vec_packs(c0, c1); \
    c23 = vec_packs(c2, c3); \
    c45 = vec_packs(c4, c5); \
    c67 = vec_packs(c6, c7); \
}

#define adst_8x2_in(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                    c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, \
                    c0, c1, c2, c3, c4, c5, c6, c7) \
{ \
    ADST_INNER_8(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                 c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h) \
    ADST_INNER_8(c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, \
                 c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l) \
}

#define adst_8x2_out(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                    c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, \
                    c0, c1, c2, c3, c4, c5, c6, c7) \
{ \
    ADST_INNER_8(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                 c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h) \
    ADST_INNER_8(c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, \
                 c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l) \
    PACK_8(c0, c1, c2, c3, c4, c5, c6, c7, \
           c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
           c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l) \
}

#define flipadst_8_in(c0, c1, c2, c3, c4, c5, c6, c7, c01, c23, c45, c67) \
{\
    ADST_INNER_8(c0, c1, c2, c3, c4, c5, c6, c7, \
                 c7, c6, c5, c4, c3, c2, c1, c0) \
    c01 = vec_packs(c0, c1); \
    c23 = vec_packs(c2, c3); \
    c45 = vec_packs(c4, c5); \
    c67 = vec_packs(c6, c7); \
    UNPACK_PAIR_I16_I32(c0, c1, c01) \
    UNPACK_PAIR_I16_I32(c2, c3, c23) \
    UNPACK_PAIR_I16_I32(c4, c5, c45) \
    UNPACK_PAIR_I16_I32(c6, c7, c67) \
}

#define flipadst_8_out(c0, c1, c2, c3, c4, c5, c6, c7, c01, c23, c45, c67) \
{\
    ADST_INNER_8(c0, c1, c2, c3, c4, c5, c6, c7, \
                 c7, c6, c5, c4, c3, c2, c1, c0) \
    c01 = vec_packs(c0, c1); \
    c23 = vec_packs(c2, c3); \
    c45 = vec_packs(c4, c5); \
    c67 = vec_packs(c6, c7); \
}

#define flipadst_8x2_in(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                        c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, \
                        c0, c1, c2, c3, c4, c5, c6, c7) \
{ \
    ADST_INNER_8(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                 c7h, c6h, c5h, c4h, c3h, c2h, c1h, c0h) \
    ADST_INNER_8(c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, \
                 c7l, c6l, c5l, c4l, c3l, c2l, c1l, c0l) \
}

#define flipadst_8x2_out(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                         c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, \
                         c0, c1, c2, c3, c4, c5, c6, c7) \
{ \
    ADST_INNER_8(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                 c7h, c6h, c5h, c4h, c3h, c2h, c1h, c0h) \
    ADST_INNER_8(c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, \
                 c7l, c6l, c5l, c4l, c3l, c2l, c1l, c0l) \
    PACK_8(c0, c1, c2, c3, c4, c5, c6, c7, \
           c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
           c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l) \
}

void dav1d_inv_txfm_add_dct_dct_4x8_8bpc_pwr9(uint8_t *dst, const ptrdiff_t stride,
                                              int16_t *const coeff, const int eob)
{
    i16x8 v = vec_splats((int16_t)(2896*8));

    if (eob < 1) {
        return dc_only_4xN(dst, stride, coeff, 2, 1, 0);
    }

    LOAD_SCALE_COEFF_4x8(coeff, v)

    dct_4_in(c0, c1, c2, c3, c01, c23)
    dct_4_in(c4, c5, c6, c7, c45, c67)


    memset(coeff, 0, sizeof(*coeff) * 4 * 8);

    TRANSPOSE4_I32(c0, c1, c2, c3);
    TRANSPOSE4_I32(c4, c5, c6, c7);

    dct_8_out(c0, c1, c2, c3, c4, c5, c6, c7, c01, c23, c45, c67)

    LOAD_DECLARE_4(dst, stride, a, b, cc, d)
    LOAD_DECLARE_4(dst + 4 * stride, stride, e, f, g, hh)

    APPLY_COEFF_4(a, b, cc, d, c01, c23)
    APPLY_COEFF_4(e, f, g, hh, c45, c67)

    STORE_4(dst, stride, a, b, cc, d)
    STORE_4(dst + 4 * stride, stride, e, f, g, hh)
}


#define inv_txfm_fn4x8(type1, type2) \
void dav1d_inv_txfm_add_##type1##_##type2##_4x8_8bpc_pwr9(uint8_t *dst, const ptrdiff_t stride, \
                                                          int16_t *const coeff, const int eob) \
{ \
    i16x8 v = vec_splats((int16_t)(2896*8)); \
    LOAD_SCALE_COEFF_4x8(coeff, v) \
    type1##_4_in(c0, c1, c2, c3, c01, c23) \
    type1##_4_in(c4, c5, c6, c7, c45, c67) \
    memset(coeff, 0, sizeof(*coeff) * 4 * 8); \
    TRANSPOSE4_I32(c0, c1, c2, c3); \
    TRANSPOSE4_I32(c4, c5, c6, c7); \
    type2##_8_out(c0, c1, c2, c3, c4, c5, c6, c7, c01, c23, c45, c67) \
    LOAD_DECLARE_4(dst, stride, a, b, c, d) \
    LOAD_DECLARE_4(dst + 4 * stride, stride, e, f, g, h) \
    APPLY_COEFF_4(a, b, c, d, c01, c23) \
    APPLY_COEFF_4(e, f, g, h, c45, c67) \
    STORE_4(dst, stride, a, b, c, d) \
    STORE_4(dst + 4 * stride, stride, e, f, g, h) \
}

inv_txfm_fn4x8(adst,     dct     )
inv_txfm_fn4x8(dct,      adst    )
inv_txfm_fn4x8(dct,      flipadst)
inv_txfm_fn4x8(flipadst, dct     )
inv_txfm_fn4x8(adst,     flipadst)
inv_txfm_fn4x8(flipadst, adst    )
inv_txfm_fn4x8(identity, dct     )
inv_txfm_fn4x8(dct,      identity)
inv_txfm_fn4x8(identity, flipadst)
inv_txfm_fn4x8(flipadst, identity)
inv_txfm_fn4x8(identity, adst   )
inv_txfm_fn4x8(adst,     identity)
inv_txfm_fn4x8(identity, identity)
inv_txfm_fn4x8(adst,     adst    )
inv_txfm_fn4x8(flipadst, flipadst)


void dav1d_inv_txfm_add_dct_dct_8x4_8bpc_pwr9(uint8_t *dst, const ptrdiff_t stride,
                                              int16_t *const coeff, const int eob)
{
    i16x8 v = vec_splats((int16_t)(2896*8));

    if (eob < 1) {
        return dc_only_8xN(dst, stride, coeff, 1, 1, 0);
    }

    LOAD_SCALE_COEFF_8x4(coeff, v)

    dct_8_in(c0, c1, c2, c3, c4, c5, c6, c7, c01, c23, c45, c67)

    memset(coeff, 0, sizeof(*coeff) * 8 * 4);

    TRANSPOSE4_I32(c0, c1, c2, c3)
    TRANSPOSE4_I32(c4, c5, c6, c7)

    dct_4_out(c0, c1, c2, c3, c01, c23)
    dct_4_out(c4, c5, c6, c7, c45, c67)

    LOAD_DECLARE_4(dst, stride, ae, bf, cg, dh)

    i16x8 c04 = (i16x8)vec_mergeh((u64x2)c01, (u64x2)c45);
    i16x8 c15 = (i16x8)vec_mergel((u64x2)c01, (u64x2)c45);
    i16x8 c26 = (i16x8)vec_mergeh((u64x2)c23, (u64x2)c67);
    i16x8 c37 = (i16x8)vec_mergel((u64x2)c23, (u64x2)c67);

    APPLY_COEFF_8x4(ae, bf, c04, c15)
    APPLY_COEFF_8x4(cg, dh, c26, c37)

    STORE_8(dst, stride, ae, bf, cg, dh)
}


#define inv_txfm_fn8x4(type1, type2) \
void dav1d_inv_txfm_add_##type1##_##type2##_8x4_8bpc_pwr9(uint8_t *dst, const ptrdiff_t stride, \
                                                          int16_t *const coeff, const int eob) \
{ \
    i16x8 v = vec_splats((int16_t)(2896*8)); \
    LOAD_SCALE_COEFF_8x4(coeff, v) \
    type1##_8_in(c0, c1, c2, c3, c4, c5, c6, c7, c01, c23, c45, c67) \
    memset(coeff, 0, sizeof(*coeff) * 8 * 4); \
    TRANSPOSE4_I32(c0, c1, c2, c3) \
    TRANSPOSE4_I32(c4, c5, c6, c7) \
    type2##_4_out(c0, c1, c2, c3, c01, c23) \
    type2##_4_out(c4, c5, c6, c7, c45, c67) \
    LOAD_DECLARE_4(dst, stride, ae, bf, cg, dh) \
    i16x8 c04 = (i16x8)vec_mergeh((u64x2)c01, (u64x2)c45); \
    i16x8 c15 = (i16x8)vec_mergel((u64x2)c01, (u64x2)c45); \
    i16x8 c26 = (i16x8)vec_mergeh((u64x2)c23, (u64x2)c67); \
    i16x8 c37 = (i16x8)vec_mergel((u64x2)c23, (u64x2)c67); \
    APPLY_COEFF_8x4(ae, bf, c04, c15) \
    APPLY_COEFF_8x4(cg, dh, c26, c37) \
    STORE_8(dst, stride, ae, bf, cg, dh) \
}
inv_txfm_fn8x4(adst,     dct     )
inv_txfm_fn8x4(dct,      adst    )
inv_txfm_fn8x4(dct,      flipadst)
inv_txfm_fn8x4(flipadst, dct     )
inv_txfm_fn8x4(adst,     flipadst)
inv_txfm_fn8x4(flipadst, adst    )
inv_txfm_fn8x4(identity, dct     )
inv_txfm_fn8x4(dct,      identity)
inv_txfm_fn8x4(identity, flipadst)
inv_txfm_fn8x4(flipadst, identity)
inv_txfm_fn8x4(identity, adst   )
inv_txfm_fn8x4(adst,     identity)
inv_txfm_fn8x4(identity, identity)
inv_txfm_fn8x4(adst,     adst    )
inv_txfm_fn8x4(flipadst, flipadst)

void dav1d_inv_txfm_add_dct_dct_8x8_8bpc_pwr9(uint8_t *dst, const ptrdiff_t stride,
                                              int16_t *const coeff, const int eob)
{
    if (eob < 1) {
        return dc_only_8xN(dst, stride, coeff, 2, 0, 1);
    }

    LOAD_COEFF_8x8(coeff)

    dct_8x2_in(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h,
               c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l,
               c0, c1, c2, c3, c4, c5, c6, c7)

    memset(coeff, 0, sizeof(*coeff) * 8 * 8);

    SCALE_ROUND_4(c0h, c1h, c2h, c3h, vec_splat_s32(1), vec_splat_u32(1))
    SCALE_ROUND_4(c4h, c5h, c6h, c7h, vec_splat_s32(1), vec_splat_u32(1))
    SCALE_ROUND_4(c0l, c1l, c2l, c3l, vec_splat_s32(1), vec_splat_u32(1))
    SCALE_ROUND_4(c4l, c5l, c6l, c7l, vec_splat_s32(1), vec_splat_u32(1))

    TRANSPOSE8_I32(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h,
                   c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l)

    dct_8x2_out(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h,
                c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l,
                c0, c1, c2, c3, c4, c5, c6, c7)

    LOAD_DECLARE_4(dst, stride, a, b, cc, d)
    LOAD_DECLARE_4(dst + 4 * stride, stride, e, f, g, hh)

    APPLY_COEFF_8x4(a, b, c0, c1)
    APPLY_COEFF_8x4(cc, d, c2, c3)
    APPLY_COEFF_8x4(e, f, c4, c5)
    APPLY_COEFF_8x4(g, hh, c6, c7)

    STORE_8(dst, stride, a, b, cc, d)
    STORE_8(dst + 4 * stride, stride, e, f, g, hh)
}

#define inv_txfm_fn8x8(type1, type2) \
void dav1d_inv_txfm_add_##type1##_##type2##_8x8_8bpc_pwr9(uint8_t *dst, const ptrdiff_t stride, \
                                                          int16_t *const coeff, const int eob) \
{ \
    LOAD_COEFF_8x8(coeff) \
    type1##_8x2_in(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                   c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, \
                   c0, c1, c2, c3, c4, c5, c6, c7) \
    SCALE_ROUND_4(c0h, c1h, c2h, c3h, vec_splat_s32(1), vec_splat_u32(1)) \
    SCALE_ROUND_4(c4h, c5h, c6h, c7h, vec_splat_s32(1), vec_splat_u32(1)) \
    SCALE_ROUND_4(c0l, c1l, c2l, c3l, vec_splat_s32(1), vec_splat_u32(1)) \
    SCALE_ROUND_4(c4l, c5l, c6l, c7l, vec_splat_s32(1), vec_splat_u32(1)) \
    memset(coeff, 0, sizeof(*coeff) * 8 * 8); \
    TRANSPOSE8_I32(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                   c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l) \
    type2##_8x2_out(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                    c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, \
                    c0, c1, c2, c3, c4, c5, c6, c7) \
    LOAD_DECLARE_4(dst, stride, a, b, c, d) \
    LOAD_DECLARE_4(dst + 4 * stride, stride, e, f, g, h) \
    APPLY_COEFF_8x4(a, b, c0, c1) \
    APPLY_COEFF_8x4(c, d, c2, c3) \
    APPLY_COEFF_8x4(e, f, c4, c5) \
    APPLY_COEFF_8x4(g, h, c6, c7) \
    STORE_8(dst, stride, a, b, c, d) \
    STORE_8(dst + 4 * stride, stride, e, f, g, h) \
}
inv_txfm_fn8x8(adst,     dct     )
inv_txfm_fn8x8(dct,      adst    )
inv_txfm_fn8x8(dct,      flipadst)
inv_txfm_fn8x8(flipadst, dct     )
inv_txfm_fn8x8(adst,     flipadst)
inv_txfm_fn8x8(flipadst, adst    )
inv_txfm_fn8x8(dct,      identity)
inv_txfm_fn8x8(flipadst, identity)
inv_txfm_fn8x8(adst,     identity)
inv_txfm_fn8x8(adst,     adst    )
inv_txfm_fn8x8(flipadst, flipadst)

// identity + scale is a no op
#define inv_txfm_fn8x8_identity(type2) \
void dav1d_inv_txfm_add_identity_##type2##_8x8_8bpc_pwr9(uint8_t *dst, const ptrdiff_t stride, \
                                                         int16_t *const coeff, const int eob) \
{ \
    LOAD_COEFF_8x8(coeff) \
    memset(coeff, 0, sizeof(*coeff) * 8 * 8); \
    TRANSPOSE8_I32(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                   c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l) \
    type2##_8x2_out(c0h, c1h, c2h, c3h, c4h, c5h, c6h, c7h, \
                    c0l, c1l, c2l, c3l, c4l, c5l, c6l, c7l, \
                    c0, c1, c2, c3, c4, c5, c6, c7) \
    LOAD_DECLARE_4(dst, stride, a, b, c, d) \
    LOAD_DECLARE_4(dst + 4 * stride, stride, e, f, g, h) \
    APPLY_COEFF_8x4(a, b, c0, c1) \
    APPLY_COEFF_8x4(c, d, c2, c3) \
    APPLY_COEFF_8x4(e, f, c4, c5) \
    APPLY_COEFF_8x4(g, h, c6, c7) \
    STORE_8(dst, stride, a, b, c, d) \
    STORE_8(dst + 4 * stride, stride, e, f, g, h) \
}
inv_txfm_fn8x8_identity(dct     )
inv_txfm_fn8x8_identity(flipadst)
inv_txfm_fn8x8_identity(adst    )
inv_txfm_fn8x8_identity(identity)

#define CLIP16_I32_8(a, b, c, d, e, f, g, h, \
                     ab, cd, ef, gh) \
{ \
    ab = vec_packs(a, b); \
    cd = vec_packs(c, d); \
    ef = vec_packs(e, f); \
    gh = vec_packs(g, h); \
    UNPACK_PAIR_I16_I32(a, b, ab) \
    UNPACK_PAIR_I16_I32(c, d, cd) \
    UNPACK_PAIR_I16_I32(e, f, ef) \
    UNPACK_PAIR_I16_I32(g, h, gh) \
}

#define MUL_4_INPLACE(a, b, c, d, v) \
    a = vec_mul(a, v); \
    b = vec_mul(b, v); \
    c = vec_mul(c, v); \
    d = vec_mul(d, v); \

#define IDENTITY_16_V(v) \
{ \
    i16x8 v_ = vec_adds(v, v); \
    v = vec_mradds(v, v1697_16, v_); \
}

#define IDENTITY_16_INNER(c00c01, c02c03, c04c05, c06c07, \
                          c08c09, c10c11, c12c13, c14c15) \
{ \
    i16x8 v1697_16 = vec_splats((int16_t)(1697*16)); \
    IDENTITY_16_V(c00c01) \
    IDENTITY_16_V(c02c03) \
    IDENTITY_16_V(c04c05) \
    IDENTITY_16_V(c06c07) \
    IDENTITY_16_V(c08c09) \
    IDENTITY_16_V(c10c11) \
    IDENTITY_16_V(c12c13) \
    IDENTITY_16_V(c14c15) \
}

#define IDENTITY_16_4_I32(a, b, c, d) \
{ \
    i32x4 a2 = vec_add(a, a); \
    i32x4 b2 = vec_add(b, b); \
    i32x4 c2 = vec_add(c, c); \
    i32x4 d2 = vec_add(d, d); \
    MUL_4_INPLACE(a, b, c, d, v1697) \
    SCALE_ROUND_4(a, b, c, d, v1024, vec_splat_u32(11)); \
    a = vec_add(a2, a); \
    b = vec_add(b2, b); \
    c = vec_add(c2, c); \
    d = vec_add(d2, d); \
}


#define identity_16_in(c00, c01, c02, c03, c04, c05, c06, c07, \
                       c08, c09, c10, c11, c12, c13, c14, c15, \
                       c00c01, c02c03, c04c05, c06c07, \
                       c08c09, c10c11, c12c13, c14c15) \
{ \
    DECLARE_SPLAT_I32(1697) \
    DECLARE_SPLAT_I32(1024) \
    IDENTITY_16_4_I32(c00, c01, c02, c03) \
    IDENTITY_16_4_I32(c04, c05, c06, c07) \
    IDENTITY_16_4_I32(c08, c09, c10, c11) \
    IDENTITY_16_4_I32(c12, c13, c14, c15) \
}

#define identity_16_out(c00, c01, c02, c03, c04, c05, c06, c07, \
                        c08, c09, c10, c11, c12, c13, c14, c15, \
                        c00c01, c02c03, c04c05, c06c07, \
                        c08c09, c10c11, c12c13, c14c15) \
{ \
    PACK_8(c00c01, c02c03, c04c05, c06c07, c08c09, c10c11, c12c13, c14c15, \
           c00, c02, c04, c06, c08, c10, c12, c14, \
           c01, c03, c05, c07, c09, c11, c13, c15)  \
    IDENTITY_16_INNER(c00c01, c02c03, c04c05, c06c07, \
                      c08c09, c10c11, c12c13, c14c15) \
}

#define IDCT_16_INNER(c00, c01, c02, c03, c04, c05, c06, c07, \
                      c08, c09, c10, c11, c12, c13, c14, c15, \
                      c00c03, c01c02, c07c04, c06c05, \
                      c08c11, c09c10, c14c13, c15c12) \
    IDCT_8_INNER(c00, c02, c04, c06, c08, c10, c12, c14, \
                 c00c03, c01c02, c07c04, c06c05) \
    DECLARE_SPLAT_I32(128) \
    DECLARE_SPLAT_I32(181) \
    DECLARE_SPLAT_I32(401) \
    DECLARE_SPLAT_I32(4076) \
    DECLARE_SPLAT_I32(3166) \
    DECLARE_SPLAT_I32(2598) \
    DECLARE_SPLAT_I32(1931) \
    DECLARE_SPLAT_I32(3612) \
    DECLARE_SPLAT_I32(3920) \
    DECLARE_SPLAT_I32(1189) \
    DECLARE_SPLAT_I32(1567) \
    DECLARE_SPLAT_I32(3784) \
\
    DECLARE_MUL_PAIR_I32(c01, c15,  v401, v4076) \
    DECLARE_MUL_PAIR_I32(c09, c07, v3166, v2598) \
    DECLARE_MUL_PAIR_I32(c05, c11, v1931, v3612) \
    DECLARE_MUL_PAIR_I32(c13, c03, v3920, v1189) \
\
    DECLARE_ADD_SUB_PAIR(t15a, t08a, c01, c15, v4076,  v401) \
    DECLARE_ADD_SUB_PAIR(t14a, t09a, c09, c07, v2598, v3166) \
    DECLARE_ADD_SUB_PAIR(t13a, t10a, c05, c11, v3612, v1931) \
    DECLARE_ADD_SUB_PAIR(t12a, t11a, c13, c03, v1189, v3920) \
\
    SCALE_ROUND_4(t15a, t08a, t14a, t09a, v2048, v12) \
    SCALE_ROUND_4(t13a, t10a, t12a, t11a, v2048, v12) \
\
    CLIP16_I32_8(t15a, t08a, t14a, t09a, \
                 t13a, t10a, t12a, t11a, \
                 c08c11, c09c10, c14c13, c15c12) \
    DECLARE_ADD_SUB_PAIR(t08, t09, t08a, t09a,,) \
    DECLARE_ADD_SUB_PAIR(t11, t10, t11a, t10a,,) \
    DECLARE_ADD_SUB_PAIR(t12, t13, t12a, t13a,,) \
    DECLARE_ADD_SUB_PAIR(t15, t14, t15a, t14a,,) \
\
    CLIP16_I32_8(t08, t09, t11, t10, \
                 t12, t13, t15, t14, \
                 c08c11, c09c10, c14c13, c15c12) \
\
    DECLARE_MUL_PAIR_I32(t14, t09, v1567, v3784) \
    DECLARE_MUL_PAIR_I32(t13, t10, v1567, v3784) \
    \
    ADD_SUB_PAIR(t14a, t09a, t14, t09, v3784, v1567) \
    ADD_SUB_PAIR(t10a, t13a, t13, t10, v3784, v1567) \
    t10a = -t10a; \
\
    SCALE_ROUND_4(t14a, t09a, t13a, t10a, v2048, v12) \
\
    ADD_SUB_PAIR(t08a, t11a, t08, t11,,) \
    ADD_SUB_PAIR(t09, t10, t09a, t10a,,) \
    ADD_SUB_PAIR(t15a, t12a, t15, t12,,) \
    ADD_SUB_PAIR(t14, t13, t14a, t13a,,) \
\
    CLIP16_I32_8(t08a, t11a, t09, t10, \
                 t15a, t12a, t14, t13, \
                 c08c11, c09c10, c14c13, c15c12) \
    ADD_SUB_PAIR(t13a, t10a, t13, t10,,); \
    ADD_SUB_PAIR(t12, t11, t12a, t11a,,); \
\
    MUL_4_INPLACE(t13a, t10a, t12, t11, v181); \
    SCALE_ROUND_4(t13a, t10a, t12, t11, v128, vec_splat_u32(8)) \
\
    DECLARE_PACK_4(t15at12, t14t13a, t08at11, t09t10a, \
                   t15a, t14, t08a, t09, \
                   t12, t13a, t11,  t10a) \
\
    c15c12 = vec_subs(c00c03, t15at12); \
    c14c13 = vec_subs(c01c02, t14t13a); \
    c08c11 = vec_subs(c07c04, t08at11); \
    c09c10 = vec_subs(c06c05, t09t10a); \
    c00c03 = vec_adds(c00c03, t15at12); \
    c01c02 = vec_adds(c01c02, t14t13a); \
    c07c04 = vec_adds(c07c04, t08at11); \
    c06c05 = vec_adds(c06c05, t09t10a); \

#define dct_16_out(c00, c01, c02, c03, c04, c05, c06, c07, \
                   c08, c09, c10, c11, c12, c13, c14, c15, \
                   c00c01, c02c03, c04c05, c06c07, c08c09, c10c11, c12c13, c14c15) \
\
    i16x8 c00c03, c01c02, c07c04, c06c05, c08c11, c09c10, c14c13, c15c12; \
    IDCT_16_INNER(c00, c01, c02, c03, c04, c05, c06, c07, c08, c09, c10, c11, c12, c13, c14, c15, \
                  c00c03, c01c02, c07c04, c06c05, c08c11, c09c10, c14c13, c15c12) \
    c00c01 = (i16x8)vec_mergeh((u64x2)c00c03, (u64x2)c01c02); \
    c02c03 = (i16x8)vec_mergel((u64x2)c01c02, (u64x2)c00c03); \
    c04c05 = (i16x8)vec_mergel((u64x2)c07c04, (u64x2)c06c05); \
    c06c07 = (i16x8)vec_mergeh((u64x2)c06c05, (u64x2)c07c04); \
    c08c09 = (i16x8)vec_mergeh((u64x2)c08c11, (u64x2)c09c10); \
    c10c11 = (i16x8)vec_mergel((u64x2)c09c10, (u64x2)c08c11); \
    c12c13 = (i16x8)vec_mergel((u64x2)c15c12, (u64x2)c14c13); \
    c14c15 = (i16x8)vec_mergeh((u64x2)c14c13, (u64x2)c15c12); \

#define dct_16_in(c00, c01, c02, c03, c04, c05, c06, c07, \
                  c08, c09, c10, c11, c12, c13, c14, c15, \
                  c00c03, c01c02, c07c04, c06c05, c08c11, c09c10, c14c13, c15c12) \
\
    IDCT_16_INNER(c00, c01, c02, c03, c04, c05, c06, c07, c08, c09, c10, c11, c12, c13, c14, c15, \
                  c00c03, c01c02, c07c04, c06c05, c08c11, c09c10, c14c13, c15c12) \
    UNPACK_PAIR_I16_I32(c00, c03, c00c03) \
    UNPACK_PAIR_I16_I32(c01, c02, c01c02) \
    UNPACK_PAIR_I16_I32(c07, c04, c07c04) \
    UNPACK_PAIR_I16_I32(c06, c05, c06c05) \
    UNPACK_PAIR_I16_I32(c08, c11, c08c11) \
    UNPACK_PAIR_I16_I32(c09, c10, c09c10) \
    UNPACK_PAIR_I16_I32(c14, c13, c14c13) \
    UNPACK_PAIR_I16_I32(c15, c12, c15c12) \


#define dct_4x4_in(cA0, cA1, cA2, cA3, cB0, cB1, cB2, cB3, \
                   cC0, cC1, cC2, cC3, cD0, cD1, cD2, cD3, \
                   a0b0, c0d0, a1b1, c1d1, a2b2, c2d2, a3b3, c3d3) \
    dct_4_in(cA0, cA1, cA2, cA3, a0b0, c0d0) \
    dct_4_in(cB0, cB1, cB2, cB3, a1b1, c1d1) \
    dct_4_in(cC0, cC1, cC2, cC3, a2b2, c2d2) \
    dct_4_in(cD0, cD1, cD2, cD3, a3b3, c3d3)


#define PACK_4x4(c00, c01, c02, c03, \
                 c04, c05, c06, c07, \
                 c08, c09, c10, c11, \
                 c12, c13, c14, c15, \
                 c00c01, c02c03, c04c05, c06c07, \
                 c08c09, c10c11, c12c13, c14c15) \
{ \
    c00c01 = vec_packs(c00, c04); c02c03 = vec_packs(c08, c12); \
    c04c05 = vec_packs(c01, c05); c06c07 = vec_packs(c09, c13); \
    c08c09 = vec_packs(c02, c06); c10c11 = vec_packs(c10, c14); \
    c12c13 = vec_packs(c03, c07); c14c15 = vec_packs(c11, c15); \
}



#define dct_4x4_out(c00, c01, c02, c03, \
                    c04, c05, c06, c07, \
                    c08, c09, c10, c11, \
                    c12, c13, c14, c15, \
                    c00c01, c02c03, c04c05, c06c07, \
                    c08c09, c10c11, c12c13, c14c15) \
{ \
    IDCT_4_INNER(c00, c01, c02, c03) \
    IDCT_4_INNER(c04, c05, c06, c07) \
    IDCT_4_INNER(c08, c09, c10, c11) \
    IDCT_4_INNER(c12, c13, c14, c15) \
\
    PACK_4x4(c00, c01, c02, c03, \
             c04, c05, c06, c07, \
             c08, c09, c10, c11, \
             c12, c13, c14, c15, \
             c00c01, c02c03, c04c05, c06c07, \
             c08c09, c10c11, c12c13, c14c15) \
}

#define IDENTITY_4_I32(a, b, c, d) \
{ \
    DECLARE_SPLAT_I32(5793) \
    DECLARE_SPLAT_I32(2048) \
    MUL_4_INPLACE(a, b, c, d, v5793) \
    SCALE_ROUND_4(a, b, c, d, v2048, vec_splat_u32(12)) \
}

#define identity_4x4_in(cA0, cA1, cA2, cA3, cB0, cB1, cB2, cB3, \
                       cC0, cC1, cC2, cC3, cD0, cD1, cD2, cD3, \
                       a0b0, c0d0, a1b1, c1d1, a2b2, c2d2, a3b3, c3d3) \
{ \
    IDENTITY_4_I32(cA0, cA1, cA2, cA3) \
    IDENTITY_4_I32(cB0, cB1, cB2, cB3) \
    IDENTITY_4_I32(cC0, cC1, cC2, cC3) \
    IDENTITY_4_I32(cD0, cD1, cD2, cD3) \
}

#define identity_4x4_out(c00, c01, c02, c03, \
                         c04, c05, c06, c07, \
                         c08, c09, c10, c11, \
                         c12, c13, c14, c15, \
                         c00c01, c02c03, c04c05, c06c07, \
                         c08c09, c10c11, c12c13, c14c15) \
{ \
    PACK_4x4(c00, c01, c02, c03, \
             c04, c05, c06, c07, \
             c08, c09, c10, c11, \
             c12, c13, c14, c15, \
             c00c01, c02c03, c04c05, c06c07, \
             c08c09, c10c11, c12c13, c14c15) \
    IDENTITY_4(c00c01, c02c03) \
    IDENTITY_4(c04c05, c06c07) \
    IDENTITY_4(c08c09, c10c11) \
    IDENTITY_4(c12c13, c14c15) \
}

#define adst_4x4_in(cA0, cA1, cA2, cA3, cB0, cB1, cB2, cB3, \
                    cC0, cC1, cC2, cC3, cD0, cD1, cD2, cD3, \
                    a0b0, c0d0, a1b1, c1d1, a2b2, c2d2, a3b3, c3d3) \
    adst_4_in(cA0, cA1, cA2, cA3, a0b0, c0d0) \
    adst_4_in(cB0, cB1, cB2, cB3, a1b1, c1d1) \
    adst_4_in(cC0, cC1, cC2, cC3, a2b2, c2d2) \
    adst_4_in(cD0, cD1, cD2, cD3, a3b3, c3d3)

#define adst_4x4_out(c00, c01, c02, c03, \
                     c04, c05, c06, c07, \
                     c08, c09, c10, c11, \
                     c12, c13, c14, c15, \
                     c00c01, c02c03, c04c05, c06c07, \
                     c08c09, c10c11, c12c13, c14c15) \
{ \
    ADST_INNER_4(c00, c01, c02, c03, c00, c01, c02, c03) \
    ADST_INNER_4(c04, c05, c06, c07, c04, c05, c06, c07) \
    ADST_INNER_4(c08, c09, c10, c11, c08, c09, c10, c11) \
    ADST_INNER_4(c12, c13, c14, c15, c12, c13, c14, c15) \
\
    PACK_4x4(c00, c01, c02, c03, \
             c04, c05, c06, c07, \
             c08, c09, c10, c11, \
             c12, c13, c14, c15, \
             c00c01, c02c03, c04c05, c06c07, \
             c08c09, c10c11, c12c13, c14c15) \
}

#define flipadst_4x4_in(cA0, cA1, cA2, cA3, cB0, cB1, cB2, cB3, \
                        cC0, cC1, cC2, cC3, cD0, cD1, cD2, cD3, \
                        a0b0, c0d0, a1b1, c1d1, a2b2, c2d2, a3b3, c3d3) \
    flipadst_4_in(cA0, cA1, cA2, cA3, a0b0, c0d0) \
    flipadst_4_in(cB0, cB1, cB2, cB3, a1b1, c1d1) \
    flipadst_4_in(cC0, cC1, cC2, cC3, a2b2, c2d2) \
    flipadst_4_in(cD0, cD1, cD2, cD3, a3b3, c3d3)

#define flipadst_4x4_out(c00, c01, c02, c03, \
                         c04, c05, c06, c07, \
                         c08, c09, c10, c11, \
                         c12, c13, c14, c15, \
                         c00c01, c02c03, c04c05, c06c07, \
                         c08c09, c10c11, c12c13, c14c15) \
{ \
    ADST_INNER_4(c00, c01, c02, c03, c03, c02, c01, c00) \
    ADST_INNER_4(c04, c05, c06, c07, c07, c06, c05, c04) \
    ADST_INNER_4(c08, c09, c10, c11, c11, c10, c09, c08) \
    ADST_INNER_4(c12, c13, c14, c15, c15, c14, c13, c12) \
\
    PACK_4x4(c00, c01, c02, c03, \
             c04, c05, c06, c07, \
             c08, c09, c10, c11, \
             c12, c13, c14, c15, \
             c00c01, c02c03, c04c05, c06c07, \
             c08c09, c10c11, c12c13, c14c15) \
}

#define ADST_INNER_16(c00, c01, c02, c03, c04, c05, c06, c07, \
                      c08, c09, c10, c11, c12, c13, c14, c15, \
                      o00, o01, o02, o03, o04, o05, o06, o07, \
                      o08, o09, o10, o11, o12, o13, o14, o15, \
                      c00c01, c02c03, c04c05, c06c07) \
    DECLARE_SPLAT_I32(2048); \
    u32x4 v12 = vec_splat_u32(12); \
    DECLARE_SPLAT_I32(4091) \
    DECLARE_SPLAT_I32(201) \
    DECLARE_SPLAT_I32(3973) \
    DECLARE_SPLAT_I32(995) \
    DECLARE_SPLAT_I32(3703) \
    DECLARE_SPLAT_I32(1751) \
    DECLARE_SPLAT_I32(3290) \
    DECLARE_SPLAT_I32(2440) \
    DECLARE_SPLAT_I32(2751) \
    DECLARE_SPLAT_I32(3035) \
    DECLARE_SPLAT_I32(2106) \
    DECLARE_SPLAT_I32(3513) \
    DECLARE_SPLAT_I32(1380) \
    DECLARE_SPLAT_I32(3857) \
    DECLARE_SPLAT_I32(601) \
    DECLARE_SPLAT_I32(4052) \
\
    DECLARE_MUL_PAIR_I32(c15, c00, v4091, v201) \
    DECLARE_MUL_PAIR_I32(c13, c02, v3973, v995) \
    DECLARE_MUL_PAIR_I32(c11, c04, v3703, v1751) \
    DECLARE_MUL_PAIR_I32(c09, c06, v3290, v2440) \
    DECLARE_MUL_PAIR_I32(c07, c08, v2751, v3035) \
    DECLARE_MUL_PAIR_I32(c05, c10, v2106, v3513) \
    DECLARE_MUL_PAIR_I32(c03, c12, v1380, v3857) \
    DECLARE_MUL_PAIR_I32(c01, c14,  v601, v4052) \
\
    DECLARE_ADD_SUB_PAIR(t00, t01, c15, c00, v4091, v201);\
    DECLARE_ADD_SUB_PAIR(t02, t03, c13, c02, v3973, v995) \
    DECLARE_ADD_SUB_PAIR(t04, t05, c11, c04, v3703, v1751) \
    DECLARE_ADD_SUB_PAIR(t06, t07, c09, c06, v3290, v2440) \
    DECLARE_ADD_SUB_PAIR(t08, t09, c07, c08, v2751, v3035) \
    DECLARE_ADD_SUB_PAIR(t10, t11, c05, c10, v2106, v3513) \
    DECLARE_ADD_SUB_PAIR(t12, t13, c03, c12, v1380, v3857) \
    DECLARE_ADD_SUB_PAIR(t14, t15, c01, c14,  v601, v4052) \
\
    SCALE_ROUND_4(t00, t01, t02, t03, v2048, v12) \
    SCALE_ROUND_4(t04, t05, t06, t07, v2048, v12) \
    SCALE_ROUND_4(t08, t09, t10, t11, v2048, v12) \
    SCALE_ROUND_4(t12, t13, t14, t15, v2048, v12) \
\
    DECLARE_ADD_SUB_PAIR(t00a, t08a, t00, t08,,) \
    DECLARE_ADD_SUB_PAIR(t01a, t09a, t01, t09,,) \
    DECLARE_ADD_SUB_PAIR(t02a, t10a, t02, t10,,) \
    DECLARE_ADD_SUB_PAIR(t03a, t11a, t03, t11,,) \
    DECLARE_ADD_SUB_PAIR(t04a, t12a, t04, t12,,) \
    DECLARE_ADD_SUB_PAIR(t05a, t13a, t05, t13,,) \
    DECLARE_ADD_SUB_PAIR(t06a, t14a, t06, t14,,) \
    DECLARE_ADD_SUB_PAIR(t07a, t15a, t07, t15,,) \
\
    CLIP16_I32_8(t00a, t08a, t01a, t09a, t02a, t10a, t03a, t11a, \
                 c00c01, c02c03, c04c05, c06c07); \
    CLIP16_I32_8(t04a, t12a, t05a, t13a, t06a, t14a, t07a, t15a, \
                 c00c01, c02c03, c04c05, c06c07); \
\
    DECLARE_SPLAT_I32(4017) \
    DECLARE_SPLAT_I32(799) \
    DECLARE_SPLAT_I32(2276) \
    DECLARE_SPLAT_I32(3406) \
\
    DECLARE_MUL_PAIR_I32(t08a, t09a, v4017,  v799); \
    DECLARE_MUL_PAIR_I32(t10a, t11a, v2276, v3406); \
    DECLARE_MUL_PAIR_I32(t13a, t12a,  v799, v4017); \
    DECLARE_MUL_PAIR_I32(t15a, t14a, v3406, v2276); \
\
    ADD_SUB_PAIR(t08, t09, t08a, t09a, v4017,  v799); \
    ADD_SUB_PAIR(t10, t11, t10a, t11a, v2276, v3406); \
    ADD_SUB_PAIR(t13, t12, t13a, t12a,  v799, v4017); \
    ADD_SUB_PAIR(t15, t14, t15a, t14a, v3406, v2276); \
\
    SCALE_ROUND_4(t08, t09, t10, t11, v2048, v12) \
    SCALE_ROUND_4(t13, t12, t15, t14, v2048, v12) \
\
    ADD_SUB_PAIR(t00, t04, t00a, t04a,,); \
    ADD_SUB_PAIR(t01, t05, t01a, t05a,,); \
    ADD_SUB_PAIR(t02, t06, t02a, t06a,,); \
    ADD_SUB_PAIR(t03, t07, t03a, t07a,,); \
    ADD_SUB_PAIR(t08a, t12a, t08, t12,,); \
    ADD_SUB_PAIR(t09a, t13a, t09, t13,,); \
    ADD_SUB_PAIR(t10a, t14a, t10, t14,,); \
    ADD_SUB_PAIR(t11a, t15a, t11, t15,,); \
\
    CLIP16_I32_8(t00, t04, t01, t05, t02, t06, t03, t07, \
                 c00c01, c02c03, c04c05, c06c07) \
    CLIP16_I32_8(t08a, t12a, t09a, t13a, t10a, t14a, t11a, t15a, \
                 c00c01, c02c03, c04c05, c06c07) \
\
    DECLARE_SPLAT_I32(3784) \
    DECLARE_SPLAT_I32(1567) \
\
    DECLARE_MUL_PAIR_I32(t04, t05, v3784, v1567) \
    DECLARE_MUL_PAIR_I32(t07, t06, v1567, v3784) \
    DECLARE_MUL_PAIR_I32(t12a, t13a, v3784, v1567) \
    DECLARE_MUL_PAIR_I32(t15a, t14a, v1567, v3784) \
\
    ADD_SUB_PAIR(t04a, t05a, t04, t05, v3784, v1567) \
    ADD_SUB_PAIR(t07a, t06a, t07, t06, v1567, v3784) \
    ADD_SUB_PAIR(t12, t13, t12a, t13a, v3784, v1567) \
    ADD_SUB_PAIR(t15, t14, t15a, t14a, v1567, v3784) \
\
    SCALE_ROUND_4(t04a, t05a, t07a, t06a, v2048, v12) \
    SCALE_ROUND_4(t12, t13, t15, t14, v2048, v12) \
\
    ADD_SUB_PAIR(o00, t02a, t00,  t02,,) \
    ADD_SUB_PAIR(o15, t03a, t01,  t03,,) \
    ADD_SUB_PAIR(o03, t06,  t04a, t06a,,) \
    ADD_SUB_PAIR(o12, t07,  t05a, t07a,,) \
    ADD_SUB_PAIR(o01, t10,  t08a, t10a,,) \
    ADD_SUB_PAIR(o14, t11,  t09a, t11a,,) \
    ADD_SUB_PAIR(o02, t14a, t12,  t14,,) \
    ADD_SUB_PAIR(o13, t15a, t13,  t15,,) \
\
    CLIP16_I32_8(o00, t02a, o15, t03a, o03, t06, o12, t07, \
                 c00c01, c02c03, c04c05, c06c07) \
    CLIP16_I32_8(o01, t10, o14, t11, o02, t14a, o13, t15a, \
                 c00c01, c02c03, c04c05, c06c07) \
\
    DECLARE_SPLAT_I32(181) \
    DECLARE_SPLAT_I32(128) \
    u32x4 v8 = vec_splat_u32(8); \
\
    ADD_SUB_PAIR(o07, o08, t02a, t03a,,) \
    ADD_SUB_PAIR(o04, o11, t06,  t07,,) \
    ADD_SUB_PAIR(o06, o09, t10,  t11,,) \
    ADD_SUB_PAIR(o05, o10, t14a, t15a,,) \
\
    MUL_4_INPLACE(o07, o08, o04, o11, v181) \
    MUL_4_INPLACE(o06, o09, o05, o10, v181) \
\
    SCALE_ROUND_4(o07, o08, o04, o11, v128, v8) \
    SCALE_ROUND_4(o06, o09, o05, o10, v128, v8) \
\
    o01 = -o01; \
    o03 = -o03; \
    o05 = -o05; \
    o07 = -o07; \
    o09 = -o09; \
    o11 = -o11; \
    o13 = -o13; \
    o15 = -o15; \

#define adst_16_in(c00, c01, c02, c03, c04, c05, c06, c07, \
                   c08, c09, c10, c11, c12, c13, c14, c15, \
                   c00c01, c02c03, c04c05, c06c07, c08c09, c10c11, c12c13, c14c15) \
{ \
    ADST_INNER_16(c00, c01, c02, c03, c04, c05, c06, c07, c08, c09, c10, c11, c12, c13, c14, c15, \
                  c00, c01, c02, c03, c04, c05, c06, c07, c08, c09, c10, c11, c12, c13, c14, c15, \
                  c00c01, c02c03, c04c05, c06c07) \
}

#define adst_16_out(c00, c01, c02, c03, c04, c05, c06, c07, \
                    c08, c09, c10, c11, c12, c13, c14, c15, \
                    c00c01, c02c03, c04c05, c06c07, c08c09, c10c11, c12c13, c14c15) \
{ \
    ADST_INNER_16(c00, c01, c02, c03, c04, c05, c06, c07, c08, c09, c10, c11, c12, c13, c14, c15, \
                  c00, c01, c02, c03, c04, c05, c06, c07, c08, c09, c10, c11, c12, c13, c14, c15, \
                  c00c01, c02c03, c04c05, c06c07) \
    PACK_8(c00c01, c02c03, c04c05, c06c07, c08c09, c10c11, c12c13, c14c15, \
           c00, c02, c04, c06, c08, c10, c12, c14, \
           c01, c03, c05, c07, c09, c11, c13, c15) \
}

#define flipadst_16_in(c00, c01, c02, c03, c04, c05, c06, c07, \
                       c08, c09, c10, c11, c12, c13, c14, c15, \
                       c00c01, c02c03, c04c05, c06c07, c08c09, c10c11, c12c13, c14c15) \
{ \
    ADST_INNER_16(c00, c01, c02, c03, c04, c05, c06, c07, c08, c09, c10, c11, c12, c13, c14, c15, \
                  c15, c14, c13, c12, c11, c10, c09, c08, c07, c06, c05, c04, c03, c02, c01, c00, \
                  c00c01, c02c03, c04c05, c06c07) \
}

#define flipadst_16_out(c00, c01, c02, c03, c04, c05, c06, c07, \
                        c08, c09, c10, c11, c12, c13, c14, c15, \
                        c00c01, c02c03, c04c05, c06c07, c08c09, c10c11, c12c13, c14c15) \
{ \
    ADST_INNER_16(c00, c01, c02, c03, c04, c05, c06, c07, c08, c09, c10, c11, c12, c13, c14, c15, \
                  c15, c14, c13, c12, c11, c10, c09, c08, c07, c06, c05, c04, c03, c02, c01, c00, \
                  c00c01, c02c03, c04c05, c06c07) \
    PACK_8(c00c01, c02c03, c04c05, c06c07, c08c09, c10c11, c12c13, c14c15, \
           c00, c02, c04, c06, c08, c10, c12, c14, \
           c01, c03, c05, c07, c09, c11, c13, c15) \
}


void dav1d_inv_txfm_add_dct_dct_4x16_8bpc_pwr9(uint8_t *dst, const ptrdiff_t stride,
                                               int16_t *const coeff, const int eob
                                               HIGHBD_DECL_SUFFIX)
{
    if (eob < 1) {
        return dc_only_4xN(dst, stride, coeff, 4, 0, 1);
    }

    LOAD_COEFF_4x16(coeff)

    dct_4x4_in(cA0, cA1, cA2, cA3, cB0, cB1, cB2, cB3,
               cC0, cC1, cC2, cC3, cD0, cD1, cD2, cD3,
               a0b0, c0d0, a1b1, c1d1, a2b2, c2d2, a3b3, c3d3)

    memset(coeff, 0, sizeof(*coeff) * 4 * 16);

    SCALE_ROUND_4(cA0, cB0, cC0, cD0, vec_splat_s32(1), vec_splat_u32(1))
    SCALE_ROUND_4(cA1, cB1, cC1, cD1, vec_splat_s32(1), vec_splat_u32(1))
    SCALE_ROUND_4(cA2, cB2, cC2, cD2, vec_splat_s32(1), vec_splat_u32(1))
    SCALE_ROUND_4(cA3, cB3, cC3, cD3, vec_splat_s32(1), vec_splat_u32(1))
    TRANSPOSE4x16_I32(cA0, cA1, cA2, cA3, cB0, cB1, cB2, cB3,
                      cC0, cC1, cC2, cC3, cD0, cD1, cD2, cD3)

    dct_16_out(cA0, cA1, cA2, cA3, cB0, cB1, cB2, cB3,
               cC0, cC1, cC2, cC3, cD0, cD1, cD2, cD3,
               a0b0, c0d0, a1b1, c1d1, a2b2, c2d2, a3b3, c3d3)

    LOAD_DECLARE_4(dst, stride, l00, l01, l02, l03)
    LOAD_DECLARE_4(dst + 4 * stride, stride, l04, l05, l06, l07)
    LOAD_DECLARE_4(dst + 8 * stride, stride, l08, l09, l10, l11)
    LOAD_DECLARE_4(dst + 12 * stride, stride, l12, l13, l14, l15)

    APPLY_COEFF_4(l00, l01, l02, l03, a0b0, c0d0);
    APPLY_COEFF_4(l04, l05, l06, l07, a1b1, c1d1);
    APPLY_COEFF_4(l08, l09, l10, l11, a2b2, c2d2);
    APPLY_COEFF_4(l12, l13, l14, l15, a3b3, c3d3);

    STORE_4(dst, stride,               l00, l01, l02, l03);
    STORE_4(dst + 4 * stride, stride,  l04, l05, l06, l07);
    STORE_4(dst + 8 * stride, stride,  l08, l09, l10, l11);
    STORE_4(dst + 12 * stride, stride, l12, l13, l14, l15);
}

#define inv_txfm_fn4x16(type1, type2) \
void dav1d_inv_txfm_add_##type1##_##type2##_4x16_8bpc_pwr9(uint8_t *dst, const ptrdiff_t stride, \
                                                          int16_t *const coeff, const int eob) \
{ \
    LOAD_COEFF_4x16(coeff) \
    type1##_4x4_in(cA0, cA1, cA2, cA3, cB0, cB1, cB2, cB3, \
                   cC0, cC1, cC2, cC3, cD0, cD1, cD2, cD3, \
                   a0b0, c0d0, a1b1, c1d1, a2b2, c2d2, a3b3, c3d3) \
    memset(coeff, 0, sizeof(*coeff) * 4 * 16); \
    SCALE_ROUND_4(cA0, cB0, cC0, cD0, vec_splat_s32(1), vec_splat_u32(1)) \
    SCALE_ROUND_4(cA1, cB1, cC1, cD1, vec_splat_s32(1), vec_splat_u32(1)) \
    SCALE_ROUND_4(cA2, cB2, cC2, cD2, vec_splat_s32(1), vec_splat_u32(1)) \
    SCALE_ROUND_4(cA3, cB3, cC3, cD3, vec_splat_s32(1), vec_splat_u32(1)) \
    TRANSPOSE4x16_I32(cA0, cA1, cA2, cA3, cB0, cB1, cB2, cB3, \
                      cC0, cC1, cC2, cC3, cD0, cD1, cD2, cD3) \
    type2##_16_out(cA0, cA1, cA2, cA3, cB0, cB1, cB2, cB3, \
                   cC0, cC1, cC2, cC3, cD0, cD1, cD2, cD3, \
                   a0b0, c0d0, a1b1, c1d1, a2b2, c2d2, a3b3, c3d3) \
    LOAD_DECLARE_4(dst, stride, l00, l01, l02, l03) \
    LOAD_DECLARE_4(dst + 4 * stride, stride, l04, l05, l06, l07) \
    LOAD_DECLARE_4(dst + 8 * stride, stride, l08, l09, l10, l11) \
    LOAD_DECLARE_4(dst + 12 * stride, stride, l12, l13, l14, l15) \
    APPLY_COEFF_4(l00, l01, l02, l03, a0b0, c0d0); \
    APPLY_COEFF_4(l04, l05, l06, l07, a1b1, c1d1); \
    APPLY_COEFF_4(l08, l09, l10, l11, a2b2, c2d2); \
    APPLY_COEFF_4(l12, l13, l14, l15, a3b3, c3d3); \
    STORE_4(dst, stride,               l00, l01, l02, l03); \
    STORE_4(dst + 4 * stride, stride,  l04, l05, l06, l07); \
    STORE_4(dst + 8 * stride, stride,  l08, l09, l10, l11); \
    STORE_4(dst + 12 * stride, stride, l12, l13, l14, l15); \
}
inv_txfm_fn4x16(adst,     dct     )
inv_txfm_fn4x16(dct,      adst    )
inv_txfm_fn4x16(dct,      flipadst)
inv_txfm_fn4x16(flipadst, dct     )
inv_txfm_fn4x16(adst,     flipadst)
inv_txfm_fn4x16(flipadst, adst    )
inv_txfm_fn4x16(identity, dct     )
inv_txfm_fn4x16(dct,      identity)
inv_txfm_fn4x16(identity, flipadst)
inv_txfm_fn4x16(flipadst, identity)
inv_txfm_fn4x16(identity, adst   )
inv_txfm_fn4x16(adst,     identity)
inv_txfm_fn4x16(identity, identity)
inv_txfm_fn4x16(adst,     adst    )
inv_txfm_fn4x16(flipadst, flipadst)

void dav1d_inv_txfm_add_dct_dct_16x4_8bpc_pwr9(uint8_t *dst, const ptrdiff_t stride,
                                               int16_t *const coeff, const int eob)
{

    if (eob < 1) {
        return dc_only_16xN(dst, stride, coeff, 1, 0, 1);
    }

    LOAD_DECLARE_2_I16(coeff, c00c01, c02c03) \
    LOAD_DECLARE_2_I16(coeff+16, c04c05, c06c07) \
    LOAD_DECLARE_2_I16(coeff+32, c08c09, c10c11) \
    LOAD_DECLARE_2_I16(coeff+48, c12c13, c14c15) \
    UNPACK_DECLARE_4_I16_I32(c00c01, c02c03, c00, c01, c02, c03)
    UNPACK_DECLARE_4_I16_I32(c04c05, c06c07, c04, c05, c06, c07)
    UNPACK_DECLARE_4_I16_I32(c08c09, c10c11, c08, c09, c10, c11)
    UNPACK_DECLARE_4_I16_I32(c12c13, c14c15, c12, c13, c14, c15)

    dct_16_in(c00, c01, c02, c03, c04, c05, c06, c07,
              c08, c09, c10, c11, c12, c13, c14, c15,
              c00c01, c02c03, c04c05, c06c07, c08c09, c10c11, c12c13, c14c15)
    memset(coeff, 0, sizeof(*coeff) * 16 * 4);
    SCALE_ROUND_4(c00, c01, c02, c03, vec_splat_s32(1), vec_splat_u32(1))
    SCALE_ROUND_4(c04, c05, c06, c07, vec_splat_s32(1), vec_splat_u32(1))
    SCALE_ROUND_4(c08, c09, c10, c11, vec_splat_s32(1), vec_splat_u32(1))
    SCALE_ROUND_4(c12, c13, c14, c15, vec_splat_s32(1), vec_splat_u32(1))

    TRANSPOSE4_I32(c00, c01, c02, c03);
    TRANSPOSE4_I32(c04, c05, c06, c07);
    TRANSPOSE4_I32(c08, c09, c10, c11);
    TRANSPOSE4_I32(c12, c13, c14, c15);

    dct_4x4_out(c00, c01, c02, c03,
                c04, c05, c06, c07,
                c08, c09, c10, c11,
                c12, c13, c14, c15,
                c00c01, c02c03, c04c05, c06c07,
                c08c09, c10c11, c12c13, c14c15)

    LOAD_DECLARE_4(dst, stride, l0, l1, l2, l3)

    APPLY_COEFF_16x4(l0, l1, l2, l3,
                     c00c01, c02c03, c04c05, c06c07,
                     c08c09, c10c11, c12c13, c14c15)

    STORE_16(dst, stride, l0, l1, l2, l3)
}

#define inv_txfm_fn16x4(type1, type2) \
void dav1d_inv_txfm_add_##type1##_##type2##_16x4_8bpc_pwr9(uint8_t *dst, const ptrdiff_t stride, \
                                                          int16_t *const coeff, const int eob) \
{ \
    LOAD_DECLARE_2_I16(coeff, c00c01, c02c03) \
    LOAD_DECLARE_2_I16(coeff+16, c04c05, c06c07) \
    LOAD_DECLARE_2_I16(coeff+32, c08c09, c10c11) \
    LOAD_DECLARE_2_I16(coeff+48, c12c13, c14c15) \
    UNPACK_DECLARE_4_I16_I32(c00c01, c02c03, c00, c01, c02, c03) \
    UNPACK_DECLARE_4_I16_I32(c04c05, c06c07, c04, c05, c06, c07) \
    UNPACK_DECLARE_4_I16_I32(c08c09, c10c11, c08, c09, c10, c11) \
    UNPACK_DECLARE_4_I16_I32(c12c13, c14c15, c12, c13, c14, c15) \
    type1##_16_in(c00, c01, c02, c03, c04, c05, c06, c07, \
                  c08, c09, c10, c11, c12, c13, c14, c15, \
                  c00c01, c02c03, c04c05, c06c07, c08c09, c10c11, c12c13, c14c15) \
    memset(coeff, 0, sizeof(*coeff) * 16 * 4); \
    SCALE_ROUND_4(c00, c01, c02, c03, vec_splat_s32(1), vec_splat_u32(1)) \
    SCALE_ROUND_4(c04, c05, c06, c07, vec_splat_s32(1), vec_splat_u32(1)) \
    SCALE_ROUND_4(c08, c09, c10, c11, vec_splat_s32(1), vec_splat_u32(1)) \
    SCALE_ROUND_4(c12, c13, c14, c15, vec_splat_s32(1), vec_splat_u32(1)) \
    TRANSPOSE4_I32(c00, c01, c02, c03); \
    TRANSPOSE4_I32(c04, c05, c06, c07); \
    TRANSPOSE4_I32(c08, c09, c10, c11); \
    TRANSPOSE4_I32(c12, c13, c14, c15); \
    type2##_4x4_out(c00, c01, c02, c03, \
                    c04, c05, c06, c07, \
                    c08, c09, c10, c11, \
                    c12, c13, c14, c15, \
                    c00c01, c02c03, c04c05, c06c07, \
                    c08c09, c10c11, c12c13, c14c15); \
    LOAD_DECLARE_4(dst, stride, l0, l1, l2, l3) \
    APPLY_COEFF_16x4(l0, l1, l2, l3, \
                     c00c01, c02c03, c04c05, c06c07, \
                     c08c09, c10c11, c12c13, c14c15) \
    STORE_16(dst, stride, l0, l1, l2, l3) \
}

inv_txfm_fn16x4(adst,     dct     )
inv_txfm_fn16x4(dct,      adst    )
inv_txfm_fn16x4(dct,      flipadst)
inv_txfm_fn16x4(flipadst, dct     )
inv_txfm_fn16x4(adst,     flipadst)
inv_txfm_fn16x4(flipadst, adst    )
inv_txfm_fn16x4(dct,      identity)
inv_txfm_fn16x4(flipadst, identity)
inv_txfm_fn16x4(adst,     identity)
inv_txfm_fn16x4(identity, identity)
inv_txfm_fn16x4(adst,     adst    )
inv_txfm_fn16x4(flipadst, flipadst)

#define inv_txfm_fn16x4_identity(type2) \
void dav1d_inv_txfm_add_identity_##type2##_16x4_8bpc_pwr9(uint8_t *dst, const ptrdiff_t stride, \
                                                          int16_t *const coeff, const int eob) \
{ \
    LOAD_DECLARE_2_I16(coeff, c00c01, c02c03) \
    LOAD_DECLARE_2_I16(coeff+16, c04c05, c06c07) \
    LOAD_DECLARE_2_I16(coeff+32, c08c09, c10c11) \
    LOAD_DECLARE_2_I16(coeff+48, c12c13, c14c15) \
    UNPACK_DECLARE_4_I16_I32(c00c01, c02c03, c00, c01, c02, c03) \
    UNPACK_DECLARE_4_I16_I32(c04c05, c06c07, c04, c05, c06, c07) \
    UNPACK_DECLARE_4_I16_I32(c08c09, c10c11, c08, c09, c10, c11) \
    UNPACK_DECLARE_4_I16_I32(c12c13, c14c15, c12, c13, c14, c15) \
    identity_16_in(c00, c01, c02, c03, c04, c05, c06, c07, \
                  c08, c09, c10, c11, c12, c13, c14, c15, \
                  c00c01, c02c03, c04c05, c06c07, c08c09, c10c11, c12c13, c14c15) \
    memset(coeff, 0, sizeof(*coeff) * 16 * 4); \
    SCALE_ROUND_4(c00, c01, c02, c03, vec_splat_s32(1), vec_splat_u32(1)) \
    SCALE_ROUND_4(c04, c05, c06, c07, vec_splat_s32(1), vec_splat_u32(1)) \
    SCALE_ROUND_4(c08, c09, c10, c11, vec_splat_s32(1), vec_splat_u32(1)) \
    SCALE_ROUND_4(c12, c13, c14, c15, vec_splat_s32(1), vec_splat_u32(1)) \
    CLIP16_I32_8(c00, c01, c02, c03, c04, c05, c06, c07, c00c01, c02c03, c04c05, c06c07) \
    CLIP16_I32_8(c08, c09, c10, c11, c12, c13, c14, c15, c08c09, c10c11, c12c13, c14c15) \
    TRANSPOSE4_I32(c00, c01, c02, c03); \
    TRANSPOSE4_I32(c04, c05, c06, c07); \
    TRANSPOSE4_I32(c08, c09, c10, c11); \
    TRANSPOSE4_I32(c12, c13, c14, c15); \
    type2##_4x4_out(c00, c01, c02, c03, \
                    c04, c05, c06, c07, \
                    c08, c09, c10, c11, \
                    c12, c13, c14, c15, \
                    c00c01, c02c03, c04c05, c06c07, \
                    c08c09, c10c11, c12c13, c14c15); \
    LOAD_DECLARE_4(dst, stride, l0, l1, l2, l3) \
    APPLY_COEFF_16x4(l0, l1, l2, l3, \
                     c00c01, c02c03, c04c05, c06c07, \
                     c08c09, c10c11, c12c13, c14c15) \
    STORE_16(dst, stride, l0, l1, l2, l3) \
}

inv_txfm_fn16x4_identity(dct)
inv_txfm_fn16x4_identity(adst)
inv_txfm_fn16x4_identity(flipadst)

#endif // BITDEPTH

/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
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

#include "config.h"

#undef NDEBUG
#include <assert.h>

#include <stdlib.h>

#include "common/attributes.h"
#include "common/intops.h"

#include "src/ppc/dav1d_types.h"
#include "src/ppc/loopfilter.h"

#if BITDEPTH == 8

#define LOAD4_H(idx) \
    u8x16 idx##0 = vec_xl(0, dst); /* p1_0 p0_0 q0_0 q1_0 */ \
    dst += stridea; \
    u8x16 idx##1 = vec_xl(0, dst); /* p1_1 p0_1 q0_1 q1_1 */ \
    dst += stridea; \
    u8x16 idx##2 = vec_xl(0, dst); /* p1_2 p0_2 q0_2 q1_2 */ \
    dst += stridea; \
    u8x16 idx##3 = vec_xl(0, dst); /* p1_3 p0_3 q0_3 q1_3 */ \

// return idx##_01 and idx##_23
#define LOAD4_H_SINGLE(idx) \
    LOAD4_H(idx) \
    \
    u8x16 idx##_01 = vec_mergeh(idx##0, idx##1); /* p1_0 p1_1 p0_0 p0_1 q0_0 q0_1 q1_0 q1_1 */ \
    u8x16 idx##_23 = vec_mergeh(idx##2, idx##3); /* p1_2 p1_3 p0_2 p0_3 q0_2 q0_3 q1_2 q1_3 */


#define DECLARE_ADD_16HL(r, a, b) \
    u16x8 r##h = vec_add(a##h, b##h); \
    u16x8 r##l = vec_add(a##l, b##l);

#define ADD_16HL(r, a, b) \
    r##h = vec_add(a##h, b##h); \
    r##l = vec_add(a##l, b##l);

#define ADD_AND_SHIFT4(v) \
    v##h = vec_sr(vec_add(v##h, v4u16), v3u16); \
    v##l = vec_sr(vec_add(v##l, v4u16), v3u16);
#define ADD_AND_SHIFT8(v) \
    v##h = vec_sr(vec_add(v##h, v8u16), v4u16); \
    v##l = vec_sr(vec_add(v##l, v8u16), v4u16);

#define PACK_AND_SEL(v, m) \
    vec_sel(v, vec_pack(o##v##h, o##v##l), m)

#define UNPACK_16(v) \
    u16x8 v##h = u8h_to_u16(v); \
    u16x8 v##l = u8l_to_u16(v);


#define APPLY_4 \
    b8x16 hev = vec_cmpgt(max_a_p1p0_q1q0, H); \
 \
    i8x16 ps1 = (i8x16)vec_xor(p1, s); \
    i8x16 ps0 = (i8x16)vec_xor(p0, s); \
    i8x16 qs0 = (i8x16)vec_xor(q0, s); \
    i8x16 qs1 = (i8x16)vec_xor(q1, s); \
    i8x16 f0 = vec_and(vec_subs(ps1, qs1), hev); \
    i16x8 q0sh = (i16x8)q0h; \
    i16x8 q0sl = (i16x8)q0l; \
    i16x8 p0sh = (i16x8)p0h; \
    i16x8 p0sl = (i16x8)p0l; \
    i16x8 f0h = i8h_to_i16(f0); \
    i16x8 f0l = i8l_to_i16(f0); \
    i16x8 d0h = vec_sub(q0sh, p0sh); \
    i16x8 d0l = vec_sub(q0sl, p0sl); \
    u8x16 v3u8 = vec_splat_u8(3); \
    i16x8 d0h_2 = vec_add(d0h, d0h); \
    i16x8 d0l_2 = vec_add(d0l, d0l); \
    u8x16 v4u8 = vec_splat_u8(4); \
    i16x8 f0_d0h = vec_add(d0h, f0h); \
    i16x8 f0_d0l = vec_add(d0l, f0l); \
    i16x8 fh = vec_add(d0h_2, f0_d0h); \
    i16x8 fl = vec_add(d0l_2, f0_d0l); \
    i8x16 f = vec_packs(fh, fl); \
    i8x16 f1 = vec_adds(f, (i8x16)v4u8); \
    i8x16 f2 = vec_adds(f, (i8x16)v3u8); \
    f1 = vec_sra(f1, v3u8); \
    f2 = vec_sra(f2, v3u8); \
    f1 = vec_and(f1, fm); \
    f2 = vec_and(f2, fm); \
    i8x16 f3 = vec_adds(f1, (i8x16)v1u8); \
    b8x16 m3 = vec_and(~hev, (b8x16)fm); \
    f3 = vec_sra(f3, v1u8); \
    f3 = vec_and(f3, m3); \
    i8x16 op0s = vec_adds(ps0, f2); \
    i8x16 oq0s = vec_subs(qs0, f1); \
    i8x16 oq1s = vec_subs(qs1, f3); \
    i8x16 op1s = vec_adds(ps1, f3); \
    p0 = (u8x16)vec_xor(op0s, s); \
    q0 = (u8x16)vec_xor(oq0s, s); \
    q1 = (u8x16)vec_xor(oq1s, s); \
    p1 = (u8x16)vec_xor(op1s, s);

#define APPLY_8 \
    DECLARE_ADD_16HL(p1p0, p1, p0) \
    DECLARE_ADD_16HL(p2q0, p2, q0) \
    DECLARE_ADD_16HL(q1q2, q1, q2) \
    DECLARE_ADD_16HL(p3p3, p3, p3) \
    DECLARE_ADD_16HL(q0q3, q0, q3) \
    DECLARE_ADD_16HL(p3p2, p3, p2) \
    DECLARE_ADD_16HL(p1q1, p1, q1) \
    DECLARE_ADD_16HL(p3p0, p3, p0) \
    DECLARE_ADD_16HL(p0q2, p0, q2) \
    DECLARE_ADD_16HL(q1q3, q1, q3) \
    DECLARE_ADD_16HL(q3q3, q3, q3) \
    DECLARE_ADD_16HL(q0q1q2q3, q0q3, q1q2) \
    DECLARE_ADD_16HL(p2p1p0q0, p1p0, p2q0) \
    DECLARE_ADD_16HL(p3p3p3p2, p3p3, p3p2) \
    DECLARE_ADD_16HL(p3p3p1q1, p3p3, p1q1) \
    DECLARE_ADD_16HL(p3p0q1q2, p3p0, q1q2) \
    DECLARE_ADD_16HL(p1p0q1q3, p1p0, q1q3) \
    DECLARE_ADD_16HL(p0q2q3q3, p0q2, q3q3) \
 \
    DECLARE_ADD_16HL(op2, p3p3p3p2, p2p1p0q0) \
    DECLARE_ADD_16HL(op1, p3p3p1q1, p2p1p0q0) \
    DECLARE_ADD_16HL(op0, p3p0q1q2, p2p1p0q0) \
    DECLARE_ADD_16HL(oq0, p2p1p0q0, q0q1q2q3) \
    DECLARE_ADD_16HL(oq1, p1p0q1q3, q0q1q2q3) \
    DECLARE_ADD_16HL(oq2, p0q2q3q3, q0q1q2q3) \
 \
    ADD_AND_SHIFT4(op2) \
    ADD_AND_SHIFT4(op1) \
    ADD_AND_SHIFT4(op0) \
    ADD_AND_SHIFT4(oq0) \
    ADD_AND_SHIFT4(oq1) \
    ADD_AND_SHIFT4(oq2) \
 \
    p2 = PACK_AND_SEL(p2, apply_8); \
    p1 = PACK_AND_SEL(p1, apply_8); \
    p0 = PACK_AND_SEL(p0, apply_8); \
    q0 = PACK_AND_SEL(q0, apply_8); \
    q1 = PACK_AND_SEL(q1, apply_8); \
    q2 = PACK_AND_SEL(q2, apply_8);

#define APPLY_16 \
    DECLARE_ADD_16HL(p6p6, p6, p6) \
    DECLARE_ADD_16HL(p6p5, p6, p5) \
    DECLARE_ADD_16HL(p6p4, p6, p4) \
    DECLARE_ADD_16HL(p4p3, p4, p3) \
    DECLARE_ADD_16HL(p2p1, p2, p1) \
    DECLARE_ADD_16HL(p2q2, p2, q2) \
    DECLARE_ADD_16HL(p3q1, p3, q1) \
    DECLARE_ADD_16HL(p0q0, p0, q0) \
    DECLARE_ADD_16HL(p0q1, p0, q1) \
    DECLARE_ADD_16HL(p1q3, p1, q3) \
    DECLARE_ADD_16HL(p1q0, p1, q0) \
    DECLARE_ADD_16HL(p1q5, p1, q5) \
    DECLARE_ADD_16HL(q3q4, q3, q4) \
    DECLARE_ADD_16HL(q2q5, q2, q5) \
    DECLARE_ADD_16HL(q1q6, q1, q6) \
    DECLARE_ADD_16HL(q0q1, q0, q1) \
    DECLARE_ADD_16HL(q6q6, q6, q6) \
    DECLARE_ADD_16HL(q2q6, q2, q6) \
    DECLARE_ADD_16HL(q3q6, q3, q6) \
    DECLARE_ADD_16HL(q4q6, q4, q6) \
    DECLARE_ADD_16HL(p5q0, p5, q0) \
 \
    DECLARE_ADD_16HL(p6q2, p6, q2) \
    DECLARE_ADD_16HL(p6p6p6p4, p6p6, p6p4) \
    DECLARE_ADD_16HL(p6p5p2p1, p6p5, p2p1) \
    DECLARE_ADD_16HL(p4p3p0q0, p4p3, p0q0) \
    DECLARE_ADD_16HL(p2q2p3q1, p2q2, p3q1) \
    DECLARE_ADD_16HL(p6p5p6p6, p6p5, p6p6) \
    DECLARE_ADD_16HL(p6p5p3q1, p6p5, p3q1) \
    DECLARE_ADD_16HL(p6p6p1q3, p6p6, p1q3) \
    DECLARE_ADD_16HL(q2q5q3q4, q2q5, q3q4) \
    DECLARE_ADD_16HL(p2p1q1q6, p2p1, q1q6) \
    DECLARE_ADD_16HL(p0q0q3q6, p0q0, q3q6) \
    DECLARE_ADD_16HL(q4q6q6q6, q4q6, q6q6) \
    u16x8 q5q6q6q6h = vec_madd(v3u16, q6h, q5h); \
    u16x8 q5q6q6q6l = vec_madd(v3u16, q6l, q5l); \
    DECLARE_ADD_16HL(p0q0q1q6, p0q0, q1q6) \
    DECLARE_ADD_16HL(p0q1q3q4, p0q1, q3q4) \
 \
    DECLARE_ADD_16HL(p6q2p2p1, p6q2, p2p1) \
    DECLARE_ADD_16HL(p1q0q2q5, p1q0, q2q5) \
    DECLARE_ADD_16HL(p0q1p5q0, p0q1, p5q0) \
    DECLARE_ADD_16HL(q0q1q2q6, q0q1, q2q6) \
    DECLARE_ADD_16HL(p3q1q2q6, p3q1, q2q6) \
    DECLARE_ADD_16HL(q2q6q4q6, q2q6, q4q6) \
    DECLARE_ADD_16HL(q3q6p1q5, q3q6, p1q5) \
 \
    DECLARE_ADD_16HL(p4p3p0q0p2p1q1q6, p4p3p0q0, p2p1q1q6) \
    DECLARE_ADD_16HL(p6p5p2p1p4p3p0q0, p6p5p2p1, p4p3p0q0) \
    DECLARE_ADD_16HL(p2p1q1q6q2q5q3q4, p2p1q1q6, q2q5q3q4) \
    DECLARE_ADD_16HL(q2q5q3q4q4q6q6q6, q2q5q3q4, q4q6q6q6) \
    DECLARE_ADD_16HL(p6p5p2p1p4p3p0q0p2q2p3q1, p6p5p2p1p4p3p0q0, p2q2p3q1) \
    DECLARE_ADD_16HL(p6p6p6p4p6p5p2p1p4p3p0q0, p6p6p6p4, p6p5p2p1p4p3p0q0) \
    DECLARE_ADD_16HL(p4p3p0q0p2p1q1q6q2q5q3q4, p4p3p0q0p2p1q1q6, q2q5q3q4) \
    DECLARE_ADD_16HL(p2p1q1q6q2q5q3q4p0q0q3q6, p2p1q1q6q2q5q3q4, p0q0q3q6) \
    DECLARE_ADD_16HL(p0q0q1q6q2q5q3q4q4q6q6q6, p0q0q1q6, q2q5q3q4q4q6q6q6) \
    DECLARE_ADD_16HL(p6p5p2p1p4p3p0q0p0q1q3q4, p6p5p2p1p4p3p0q0, p0q1q3q4) \
 \
    DECLARE_ADD_16HL(op5, p6p6p6p4p6p5p2p1p4p3p0q0, p6p5p6p6) \
    DECLARE_ADD_16HL(op4, p6p6p6p4p6p5p2p1p4p3p0q0, p6p5p3q1) \
    DECLARE_ADD_16HL(op3, p6p6p6p4, p6p5p2p1p4p3p0q0p2q2p3q1) \
    DECLARE_ADD_16HL(op2, p6p6p1q3, p6p5p2p1p4p3p0q0p2q2p3q1) \
    DECLARE_ADD_16HL(op1, p6p5p2p1p4p3p0q0p0q1q3q4, p6q2p2p1) \
    DECLARE_ADD_16HL(op0, p6p5p2p1p4p3p0q0p0q1q3q4, p1q0q2q5) \
    DECLARE_ADD_16HL(oq0, p4p3p0q0p2p1q1q6q2q5q3q4, p0q1p5q0) \
    DECLARE_ADD_16HL(oq1, p4p3p0q0p2p1q1q6q2q5q3q4, q0q1q2q6) \
    DECLARE_ADD_16HL(oq2, p2p1q1q6q2q5q3q4p0q0q3q6, p3q1q2q6) \
    DECLARE_ADD_16HL(oq3, p2p1q1q6q2q5q3q4p0q0q3q6, q2q6q4q6) \
    DECLARE_ADD_16HL(oq4, p0q0q1q6q2q5q3q4q4q6q6q6, q3q6p1q5) \
    DECLARE_ADD_16HL(oq5, p0q0q1q6q2q5q3q4q4q6q6q6, q5q6q6q6) \
 \
    ADD_AND_SHIFT8(op5) \
    ADD_AND_SHIFT8(op4) \
    ADD_AND_SHIFT8(op3) \
    ADD_AND_SHIFT8(op2) \
    ADD_AND_SHIFT8(op1) \
    ADD_AND_SHIFT8(op0) \
    ADD_AND_SHIFT8(oq0) \
    ADD_AND_SHIFT8(oq1) \
    ADD_AND_SHIFT8(oq2) \
    ADD_AND_SHIFT8(oq3) \
    ADD_AND_SHIFT8(oq4) \
    ADD_AND_SHIFT8(oq5) \
 \
    p5 = PACK_AND_SEL(p5, apply_16); \
    p4 = PACK_AND_SEL(p4, apply_16); \
    p3 = PACK_AND_SEL(p3, apply_16); \
    p2 = PACK_AND_SEL(p2, apply_16); \
    p1 = PACK_AND_SEL(p1, apply_16); \
    p0 = PACK_AND_SEL(p0, apply_16); \
    q0 = PACK_AND_SEL(q0, apply_16); \
    q1 = PACK_AND_SEL(q1, apply_16); \
    q2 = PACK_AND_SEL(q2, apply_16); \
    q3 = PACK_AND_SEL(q3, apply_16); \
    q4 = PACK_AND_SEL(q4, apply_16); \
    q5 = PACK_AND_SEL(q5, apply_16); \



static inline void store_h_4(u8x16 out, uint8_t *dst, int stridea)
{
    u8x16 out1 = (u8x16)vec_splat((u32x4)out, 1);
    u8x16 out2 = (u8x16)vec_splat((u32x4)out, 2);
    u8x16 out3 = (u8x16)vec_splat((u32x4)out, 3);
    vec_xst_len(out, dst, 4);
    dst += stridea;
    vec_xst_len(out1, dst, 4);
    dst += stridea;
    vec_xst_len(out2, dst, 4);
    dst += stridea;
    vec_xst_len(out3, dst, 4);
}

static inline void store_h_8(u8x16 outa, u8x16 outb, uint8_t *dst, int stridea)
{
    u8x16 out1 = (u8x16)vec_mergel((u64x2)outa, (u64x2)outa);
    u8x16 out3 = (u8x16)vec_mergel((u64x2)outb, (u64x2)outb);
    vec_xst_len(outa, dst, 6);
    dst += stridea;
    vec_xst_len(out1, dst, 6);
    dst += stridea;
    vec_xst_len(outb, dst, 6);
    dst += stridea;
    vec_xst_len(out3, dst, 6);
}

// Assume a layout  {v}0 {v}1 {v}2 {v}3, produces {v}01 {v}23
#define MERGEH_4(v) \
    u8x16 v##01 = vec_mergeh(v##0, v##1); \
    u8x16 v##23 = vec_mergeh(v##2, v##3);

#define MERGEL_4(v) \
    u8x16 v##01 = vec_mergel(v##0, v##1); \
    u8x16 v##23 = vec_mergel(v##2, v##3);

// produce {v}0123h
#define MERGEH_U16_0123(v) \
    u16x8 v##0123h = vec_mergeh((u16x8)v##01, (u16x8)v##23);

#define MERGEHL_U16_0123(v) \
    u16x8 v##0123l = vec_mergel((u16x8)v##01, (u16x8)v##23);

#define MERGE_U16_0123(v) \
    u16x8 v##0123h = vec_mergeh((u16x8)v##01, (u16x8)v##23); \
    u16x8 v##0123l = vec_mergel((u16x8)v##01, (u16x8)v##23);

// produce {ac,bd}0123h{dir}
#define MERGEH_U32_LINE(dir) \
    u32x4 ac0123h##dir = vec_mergeh((u32x4)a0123##dir, (u32x4)c0123##dir); \
    u32x4 bd0123h##dir = vec_mergeh((u32x4)b0123##dir, (u32x4)d0123##dir);

#define MERGEL_U32_LINE(dir) \
    u32x4 ac0123l##dir = vec_mergel((u32x4)a0123##dir, (u32x4)c0123##dir); \
    u32x4 bd0123l##dir = vec_mergel((u32x4)b0123##dir, (u32x4)d0123##dir);


// produce the pair of mergeh/mergel of {ac,bd}01234{dira}{dirb}
#define MERGE_U32(oh, ol, dira, dirb) \
    oh = (u8x16)vec_mergeh(ac0123##dira##dirb, bd0123##dira##dirb); \
    ol = (u8x16)vec_mergel(ac0123##dira##dirb, bd0123##dira##dirb);

#define MERGEHL_U8(a, b) \
    u8x16 a##b##h = vec_mergeh(a, b); \
    u8x16 a##b##l = vec_mergel(a, b);

#define MERGEHL_U16(out, a, b) \
    u8x16 out##h = (u8x16)vec_mergeh((u16x8)a, (u16x8)b); \
    u8x16 out##l = (u8x16)vec_mergel((u16x8)a, (u16x8)b);

#define MERGEHL_U32(out, a, b) \
    u8x16 out##h = (u8x16)vec_mergeh((u32x4)a, (u32x4)b); \
    u8x16 out##l = (u8x16)vec_mergel((u32x4)a, (u32x4)b);

static inline void
loop_filter_h_4_all(uint8_t *dst, u8x16 E, u8x16 I, u8x16 H,
                const ptrdiff_t stridea, b32x4 apply)
{
    dst -= 2;
    uint8_t *dst2 = dst;
    u8x16 p1, p0, q0, q1;

    LOAD4_H(a)
    dst += stridea;
    LOAD4_H(b)
    dst += stridea;
    LOAD4_H(c)
    dst += stridea;
    LOAD4_H(d)

    MERGEH_4(a)
    MERGEH_4(b)
    MERGEH_4(c)
    MERGEH_4(d)

    MERGEH_U16_0123(a)
    MERGEH_U16_0123(b)
    MERGEH_U16_0123(c)
    MERGEH_U16_0123(d)

    MERGEH_U32_LINE(h)
    MERGEL_U32_LINE(h)

    MERGE_U32(p1, p0, h, h)
    MERGE_U32(q0, q1, l, h)

    const u8x16 zero = vec_splat_u8(0);
    const u8x16 v1u8 = vec_splat_u8(1);
    const b8x16 s = (b8x16)vec_splats((uint8_t)128);

    const u8x16 a_p1_p0 = vec_absd(p1, p0);
    const u8x16 a_q1_q0 = vec_absd(q1, q0);
    const u8x16 a_p0_q0 = vec_absd(p0, q0);
    const u8x16 a_p1_q1 = vec_absd(p1, q1);

    u8x16 cmp_E = vec_adds(a_p0_q0, a_p0_q0);
    const u8x16 max_a_p1p0_q1q0 = vec_max(a_p1_p0, a_q1_q0);
    const u8x16 cmp_I = max_a_p1p0_q1q0;
    cmp_E = vec_adds(vec_sr(a_p1_q1, v1u8), cmp_E);
    const b8x16 ltI = vec_cmple(cmp_I, I);
    const b8x16 ltE = vec_cmple(cmp_E, E);
    b8x16 fm = vec_and(ltI, ltE);

    fm = vec_and(fm, (b8x16)apply);
    if (vec_all_eq(fm, zero))
        return;

    UNPACK_16(p0)
    UNPACK_16(q0)

    APPLY_4

    u8x16 p1p0ab = (u8x16)vec_mergeh(p1, p0); // p1 p0 ...
    u8x16 q0q1ab = (u8x16)vec_mergeh(q0, q1); // q0 q1 ...
    u8x16 p1p0cd = (u8x16)vec_mergel(p1, p0); // p1 p0 ...
    u8x16 q0q1cd = (u8x16)vec_mergel(q0, q1); // q0 q1 ...

    u8x16 outa = (u8x16)vec_mergeh((u16x8)p1p0ab, (u16x8)q0q1ab); // op1 op0 oq0 oq1 ...
    u8x16 outb = (u8x16)vec_mergel((u16x8)p1p0ab, (u16x8)q0q1ab);
    u8x16 outc = (u8x16)vec_mergeh((u16x8)p1p0cd, (u16x8)q0q1cd);
    u8x16 outd = (u8x16)vec_mergel((u16x8)p1p0cd, (u16x8)q0q1cd);

    if (apply[0]) {
        store_h_4(outa, dst2, stridea);
    }
    dst2 += 4 * stridea;
    if (apply[1]) {
        store_h_4(outb, dst2, stridea);
    }
    dst2 += 4 * stridea;
    if (apply[2]) {
        store_h_4(outc, dst2, stridea);
    }
    dst2 += 4 * stridea;
    if (apply[3]) {
        store_h_4(outd, dst2, stridea);
    }
}

static inline void
loop_filter_h_6_all(uint8_t *dst, u8x16 E, u8x16 I, u8x16 H,
                    const ptrdiff_t stridea, b32x4 apply, b32x4 m6)
{
    uint8_t *dst2 = dst - 2;
    dst -= 3;
    u8x16 p2, p1, p0, q0, q1, q2;

    LOAD4_H(a)
    dst += stridea;
    LOAD4_H(b)
    dst += stridea;
    LOAD4_H(c)
    dst += stridea;
    LOAD4_H(d)

    MERGEH_4(a)
    MERGEH_4(b)
    MERGEH_4(c)
    MERGEH_4(d)

    MERGE_U16_0123(a)
    MERGE_U16_0123(b)
    MERGE_U16_0123(c)
    MERGE_U16_0123(d)

    MERGEH_U32_LINE(h)
    MERGEL_U32_LINE(h)
    MERGEH_U32_LINE(l)

    MERGE_U32(p2, p1, h, h)
    MERGE_U32(p0, q0, l, h)
    MERGE_U32(q1, q2, h, l)

    const u8x16 F = vec_splat_u8(1);

    const u8x16 zero = vec_splat_u8(0);
    const u16x8 v3u16 = vec_splat_u16(3);
    const u16x8 v4u16 = vec_splat_u16(4);
    const u8x16 v1u8 = vec_splat_u8(1);
    const b8x16 s = (b8x16)vec_splats((uint8_t)128);

    const u8x16 a_p1_p0 = vec_absd(p1, p0);
    const u8x16 a_q1_q0 = vec_absd(q1, q0);
    const u8x16 a_p0_q0 = vec_absd(p0, q0);
    const u8x16 a_p1_q1 = vec_absd(p1, q1);
    const u8x16 a_p2_p1 = vec_absd(p2, p1);
    const u8x16 a_q2_q1 = vec_absd(q2, q1);
    const u8x16 a_p2_p0 = vec_absd(p2, p0);
    const u8x16 a_q2_q0 = vec_absd(q2, q0);

    u8x16 max_a_p2p1_q2q1 = vec_max(a_p2_p1, a_q2_q1);
    u8x16 cmp_E = vec_adds(a_p0_q0, a_p0_q0);
    const u8x16 max_a_p1p0_q1q0 = vec_max(a_p1_p0, a_q1_q0);
    u8x16 max_a_p2p0_q2q0 = vec_max(a_p2_p0, a_q2_q0);
    u8x16 cmp_I_m6 = max_a_p2p1_q2q1;
    u8x16 cmp_I_m4 = max_a_p1p0_q1q0;
    cmp_E = vec_adds(vec_sr(a_p1_q1, v1u8), cmp_E);
    cmp_I_m6 = vec_and(cmp_I_m6, (u8x16)m6);
    u8x16 cmp_I = vec_max(cmp_I_m4, cmp_I_m6);
    const b8x16 ltE = vec_cmple(cmp_E, E);
    const b8x16 ltI = vec_cmple(cmp_I, I);
    b8x16 fm = vec_and(ltI, ltE);

    fm = vec_and(fm, (b8x16)apply);
    if (vec_all_eq(fm, zero))
        return;

    UNPACK_16(p2)
    UNPACK_16(p1)
    UNPACK_16(p0)
    UNPACK_16(q0)
    UNPACK_16(q1)
    UNPACK_16(q2)

    m6 = vec_and(m6, (b32x4)fm);

    u8x16 cmp_flat8in = vec_max(max_a_p2p0_q2q0, max_a_p1p0_q1q0);
    b8x16 apply_6 = vec_and(vec_cmple(cmp_flat8in, F), (b8x16)m6);

    b8x16 apply_4 = vec_andc(fm, apply_6);

    if (vec_any_ne(apply_4, zero)) {
        APPLY_4
    }

    if (vec_any_ne(apply_6, zero)) {
        DECLARE_ADD_16HL(p2p2, p2, p2)
        DECLARE_ADD_16HL(p2p1, p2, p1)
        DECLARE_ADD_16HL(p1p0, p1, p0)
        DECLARE_ADD_16HL(p0q0, p0, q0)
        DECLARE_ADD_16HL(q0q1, q0, q1)
        DECLARE_ADD_16HL(q1q2, q1, q2)
        DECLARE_ADD_16HL(p2p2p0q0, p2p2, p0q0)
        DECLARE_ADD_16HL(p2p1p1p0, p2p1, p1p0)
        DECLARE_ADD_16HL(p1p0q1q2, p1p0, q1q2)
        DECLARE_ADD_16HL(p0q0q0q1, p0q0, q0q1)
        u16x8 q1q2q2q2h = q2h * 3 + q1h;
        u16x8 q1q2q2q2l = q2l * 3 + q1l;

        DECLARE_ADD_16HL(op1, p2p2p0q0, p2p1p1p0)
        DECLARE_ADD_16HL(op0, p2p1p1p0, p0q0q0q1)
        DECLARE_ADD_16HL(oq0, p1p0q1q2, p0q0q0q1)
        DECLARE_ADD_16HL(oq1, p0q0q0q1, q1q2q2q2)

        ADD_AND_SHIFT4(op1)
        ADD_AND_SHIFT4(op0)
        ADD_AND_SHIFT4(oq0)
        ADD_AND_SHIFT4(oq1)

        p1 = PACK_AND_SEL(p1, apply_6);
        p0 = PACK_AND_SEL(p0, apply_6);
        q0 = PACK_AND_SEL(q0, apply_6);
        q1 = PACK_AND_SEL(q1, apply_6);
    }

    u8x16 p1p0ab = (u8x16)vec_mergeh(p1, p0); // p1 p0 ...
    u8x16 q0q1ab = (u8x16)vec_mergeh(q0, q1); // q0 q1 ...
    u8x16 p1p0cd = (u8x16)vec_mergel(p1, p0); // p1 p0 ...
    u8x16 q0q1cd = (u8x16)vec_mergel(q0, q1); // q0 q1 ...

    u8x16 outa = (u8x16)vec_mergeh((u16x8)p1p0ab, (u16x8)q0q1ab); // op1 op0 oq0 oq1 ...
    u8x16 outb = (u8x16)vec_mergel((u16x8)p1p0ab, (u16x8)q0q1ab);
    u8x16 outc = (u8x16)vec_mergeh((u16x8)p1p0cd, (u16x8)q0q1cd);
    u8x16 outd = (u8x16)vec_mergel((u16x8)p1p0cd, (u16x8)q0q1cd);

    if (apply[0]) {
        store_h_4(outa, dst2, stridea);
    }
    dst2 += 4 * stridea;
    if (apply[1]) {
        store_h_4(outb, dst2, stridea);
    }
    dst2 += 4 * stridea;
    if (apply[2]) {
        store_h_4(outc, dst2, stridea);
    }
    dst2 += 4 * stridea;
    if (apply[3]) {
        store_h_4(outd, dst2, stridea);
    }
}

static inline void
loop_filter_h_8_all(uint8_t *dst, u8x16 E, u8x16 I, u8x16 H,
                    const ptrdiff_t stridea, b32x4 apply, b32x4 m8)
{
    uint8_t *dst2 = dst - 3;
    dst -= 4;
    u8x16 p3, p2, p1, p0, q0, q1, q2, q3;

    LOAD4_H(a)
    dst += stridea;
    LOAD4_H(b)
    dst += stridea;
    LOAD4_H(c)
    dst += stridea;
    LOAD4_H(d)

    MERGEH_4(a)
    MERGEH_4(b)
    MERGEH_4(c)
    MERGEH_4(d)

    MERGE_U16_0123(a)
    MERGE_U16_0123(b)
    MERGE_U16_0123(c)
    MERGE_U16_0123(d)

    MERGEH_U32_LINE(h)
    MERGEL_U32_LINE(h)
    MERGEH_U32_LINE(l)
    MERGEL_U32_LINE(l)

    MERGE_U32(p3, p2, h, h)
    MERGE_U32(p1, p0, l, h)
    MERGE_U32(q0, q1, h, l)
    MERGE_U32(q2, q3, l, l)

    const u8x16 F = vec_splat_u8(1);

    const u8x16 zero = vec_splat_u8(0);
    const u16x8 v3u16 = vec_splat_u16(3);
    const u16x8 v4u16 = vec_splat_u16(4);
    const u8x16 v1u8 = vec_splat_u8(1);
    const b8x16 s = (b8x16)vec_splats((uint8_t)128);

    const u8x16 a_p1_p0 = vec_absd(p1, p0);
    const u8x16 a_q1_q0 = vec_absd(q1, q0);
    const u8x16 a_p0_q0 = vec_absd(p0, q0);
    const u8x16 a_p1_q1 = vec_absd(p1, q1);
    const u8x16 a_p2_p1 = vec_absd(p2, p1);
    const u8x16 a_q2_q1 = vec_absd(q2, q1);
    const u8x16 a_p2_p0 = vec_absd(p2, p0);
    const u8x16 a_q2_q0 = vec_absd(q2, q0);
    const u8x16 a_p3_p0 = vec_absd(p3, p0);
    const u8x16 a_q3_q0 = vec_absd(q3, q0);
    const u8x16 a_p3_p2 = vec_absd(p3, p2);
    const u8x16 a_q3_q2 = vec_absd(q3, q2);

    u8x16 max_a_p2p1_q2q1 = vec_max(a_p2_p1, a_q2_q1);
    u8x16 max_a_p3p2_q3q2 = vec_max(a_p3_p2, a_q3_q2);
    u8x16 cmp_E = vec_adds(a_p0_q0, a_p0_q0);
    const u8x16 max_a_p1p0_q1q0 = vec_max(a_p1_p0, a_q1_q0);
    const u8x16 max_a_p2p0_q2q0 = vec_max(a_p2_p0, a_q2_q0);
    u8x16 max_a_p3p0_q3q0 = vec_max(a_p3_p0, a_q3_q0);
    u8x16 cmp_I_m8 = vec_max(max_a_p2p1_q2q1, max_a_p3p2_q3q2);
    u8x16 cmp_I_m4 = max_a_p1p0_q1q0;
    cmp_E = vec_adds(vec_sr(a_p1_q1, v1u8), cmp_E);
    cmp_I_m8 = vec_and(cmp_I_m8, (u8x16)m8);
    u8x16 cmp_I = vec_max(cmp_I_m4, cmp_I_m8);
    const b8x16 ltE = vec_cmple(cmp_E, E);
    const b8x16 ltI = vec_cmple(cmp_I, I);
    b8x16 fm = vec_and(ltI, ltE);

    fm = vec_and(fm, (b8x16)apply);
    if (vec_all_eq(fm, zero))
        return;

    #define UNPACK_16(v) \
        u16x8 v##h = u8h_to_u16(v); \
        u16x8 v##l = u8l_to_u16(v);

    UNPACK_16(p3)
    UNPACK_16(p2)
    UNPACK_16(p1)
    UNPACK_16(p0)
    UNPACK_16(q0)
    UNPACK_16(q1)
    UNPACK_16(q2)
    UNPACK_16(q3)

    m8 = vec_and(m8, (b32x4)fm);

    u8x16 cmp_flat8in = vec_max(max_a_p2p0_q2q0, max_a_p1p0_q1q0);
    cmp_flat8in = vec_max(max_a_p3p0_q3q0, cmp_flat8in);
    b8x16 apply_8 = vec_and(vec_cmple(cmp_flat8in, F), (b8x16)m8);

    b8x16 apply_4 = vec_andc(fm, apply_8);

    if (vec_any_ne(apply_4, zero)) {
        APPLY_4
    }

    if (vec_any_ne(apply_8, zero)) {
        APPLY_8
    }

    MERGEHL_U8(p2, p1) // A0 A1 A2 A3 B0 B1 B2 B3
    MERGEHL_U8(p0, q0)
    MERGEHL_U8(q1, q2)

    MERGEHL_U16(ab_p2p1p0q0, p2p1h, p0q0h)       // A0 p2 p1 p0 q0 | A1 p2 p1 p0 q0 | A2 ...
                                                 // B0 ...
    MERGEHL_U16(cd_p2p1p0q0, p2p1l, p0q0l)       // C0 ...
                                                 // D0 ...
    MERGEHL_U16(ab_q1q2, q1q2h, q1q2h)           // A0 q1 q2 q1 q2 | A1 q1 q2 ...
                                                 // B0 ...
    MERGEHL_U16(cd_q1q2, q1q2l, q1q2l)           // C0 ...
                                                 // D0 ...

    MERGEHL_U32(a, ab_p2p1p0q0h, ab_q1q2h) // A0 p2 p1 p0 q0 q1 q2 q1 q2 | A1 ..
                                           // A2 ... | A3 ...
    MERGEHL_U32(b, ab_p2p1p0q0l, ab_q1q2l) // B0 ...
                                           // C2 ...
    MERGEHL_U32(c, cd_p2p1p0q0h, cd_q1q2h) // C0 ...
                                           // C2
    MERGEHL_U32(d, cd_p2p1p0q0l, cd_q1q2l) // D0 ..
                                           // D2 ..
    if (apply[0]) {
        store_h_8(ah, al, dst2, stridea);
    }
    dst2 += 4 * stridea;
    if (apply[1]) {
        store_h_8(bh, bl, dst2, stridea);
    }
    dst2 += 4 * stridea;

    if (apply[2]) {
        store_h_8(ch, cl, dst2, stridea);
    }
    dst2 += 4 * stridea;
    if (apply[3]) {
        store_h_8(dh, dl, dst2, stridea);
    }

}

static inline void
loop_filter_h_16_all(uint8_t *dst, u8x16 E, u8x16 I, u8x16 H,
                    const ptrdiff_t stridea, b32x4 apply, b32x4 m8, b32x4 m16)
{
    uint8_t *dst2 = dst -6 ;
    dst -= 7;
    u8x16 p3, p2, p1, p0, q0, q1, q2, q3;
    u8x16 p6, p5, p4, q4, q5, q6;

    LOAD4_H(a)
    dst += stridea;
    LOAD4_H(b)
    dst += stridea;
    LOAD4_H(c)
    dst += stridea;
    LOAD4_H(d)

    {
        MERGEH_4(a)
        MERGEH_4(b)
        MERGEH_4(c)
        MERGEH_4(d)

        MERGE_U16_0123(a)
        MERGE_U16_0123(b)
        MERGE_U16_0123(c)
        MERGE_U16_0123(d)

        MERGEH_U32_LINE(h)
        MERGEL_U32_LINE(h)
        MERGEH_U32_LINE(l)
        MERGEL_U32_LINE(l)

        MERGE_U32(p6, p5, h, h)
        MERGE_U32(p4, p3, l, h)
        MERGE_U32(p2, p1, h, l)
        MERGE_U32(p0, q0, l, l)
    }
    {
        MERGEL_4(a)
        MERGEL_4(b)
        MERGEL_4(c)
        MERGEL_4(d)

        MERGE_U16_0123(a)
        MERGE_U16_0123(b)
        MERGE_U16_0123(c)
        MERGE_U16_0123(d)

        MERGEH_U32_LINE(h)
        MERGEL_U32_LINE(h)
        MERGEH_U32_LINE(l)

        MERGE_U32(q1, q2, h, h)
        MERGE_U32(q3, q4, l, h)
        MERGE_U32(q5, q6, h, l)
    }

    const u8x16 F = vec_splat_u8(1);

    const u8x16 zero = vec_splat_u8(0);
    const u16x8 v3u16 = vec_splat_u16(3);
    const u16x8 v4u16 = vec_splat_u16(4);
    const u16x8 v8u16 = vec_splat_u16(8);
    const u8x16 v1u8 = vec_splat_u8(1);
    const b8x16 s = (b8x16)vec_splats((uint8_t)128);

    const u8x16 a_p6_p0 = vec_absd(p6, p0);
    const u8x16 a_p5_p0 = vec_absd(p5, p0);
    const u8x16 a_p4_p0 = vec_absd(p4, p0);
    const u8x16 a_q4_q0 = vec_absd(q4, q0);
    const u8x16 a_q5_q0 = vec_absd(q5, q0);
    const u8x16 a_q6_q0 = vec_absd(q6, q0);

    const u8x16 a_p1_p0 = vec_absd(p1, p0);
    const u8x16 a_q1_q0 = vec_absd(q1, q0);
    const u8x16 a_p0_q0 = vec_absd(p0, q0);
    const u8x16 a_p1_q1 = vec_absd(p1, q1);
    const u8x16 a_p2_p1 = vec_absd(p2, p1);
    const u8x16 a_q2_q1 = vec_absd(q2, q1);
    const u8x16 a_p2_p0 = vec_absd(p2, p0);
    const u8x16 a_q2_q0 = vec_absd(q2, q0);
    const u8x16 a_p3_p0 = vec_absd(p3, p0);
    const u8x16 a_q3_q0 = vec_absd(q3, q0);
    const u8x16 a_p3_p2 = vec_absd(p3, p2);
    const u8x16 a_q3_q2 = vec_absd(q3, q2);

    u8x16 max_a_p2p1_q2q1 = vec_max(a_p2_p1, a_q2_q1);
    u8x16 max_a_p3p2_q3q2 = vec_max(a_p3_p2, a_q3_q2);
    u8x16 cmp_E = vec_adds(a_p0_q0, a_p0_q0);
    const u8x16 max_a_p1p0_q1q0 = vec_max(a_p1_p0, a_q1_q0);
    const u8x16 max_a_p2p0_q2q0 = vec_max(a_p2_p0, a_q2_q0);

    const u8x16 max_a_p4p0_q4q0 = vec_max(a_p4_p0, a_q4_q0);
    const u8x16 max_a_p5p0_q5q0 = vec_max(a_p5_p0, a_q5_q0);
    const u8x16 max_a_p6p0_q6q0 = vec_max(a_p6_p0, a_q6_q0);

    b32x4 m8_16 = vec_or(m8, m16);

    u8x16 max_a_p3p0_q3q0 = vec_max(a_p3_p0, a_q3_q0);
    u8x16 cmp_I_m8 = vec_max(max_a_p2p1_q2q1, max_a_p3p2_q3q2);
    u8x16 cmp_I_m4 = max_a_p1p0_q1q0;
    cmp_E = vec_adds(vec_sr(a_p1_q1, v1u8), cmp_E);
    cmp_I_m8 = vec_and(cmp_I_m8, (b8x16)m8_16);
    u8x16 cmp_I = vec_max(cmp_I_m4, cmp_I_m8);
    const b8x16 ltE = vec_cmple(cmp_E, E);
    const b8x16 ltI = vec_cmple(cmp_I, I);
    b8x16 fm = vec_and(ltI, ltE);

    fm = vec_and(fm, (b8x16)apply);
    if (vec_all_eq(fm, zero))
        return;

    u8x16 cmp_flat8in = vec_max(max_a_p2p0_q2q0, max_a_p1p0_q1q0);
    u8x16 cmp_flat8out = vec_max(max_a_p6p0_q6q0, max_a_p5p0_q5q0);

    m8_16 = vec_and(m8_16, (b32x4)fm);
    m16 = vec_and(m16, (b32x4)fm);

    cmp_flat8in = vec_max(max_a_p3p0_q3q0, cmp_flat8in);
    cmp_flat8out = vec_max(max_a_p4p0_q4q0, cmp_flat8out);
    b8x16 flat8in = vec_cmple(cmp_flat8in, F);
    b8x16 flat8out = vec_cmple(cmp_flat8out, F);
    flat8in = vec_and(flat8in, (b8x16)m8_16);
    flat8out = vec_and(flat8out, (b8x16)m16);

    b8x16 apply_16 = vec_and(flat8out, flat8in);
    b8x16 apply_8 = vec_andc(flat8in, flat8out);

    UNPACK_16(p6)
    UNPACK_16(p5)
    UNPACK_16(p4)
    UNPACK_16(p3)
    UNPACK_16(p2)
    UNPACK_16(p1)
    UNPACK_16(p0)

    b8x16 apply_4 = vec_and(fm, vec_nor(apply_16, apply_8));

    UNPACK_16(q0)
    UNPACK_16(q1)
    UNPACK_16(q2)
    UNPACK_16(q3)
    UNPACK_16(q4)
    UNPACK_16(q5)
    UNPACK_16(q6)

    if (vec_any_ne(apply_4, zero)) {
        APPLY_4
    }

    if (vec_any_ne(apply_16, zero)) {
        APPLY_16
    }

    if (vec_any_ne(apply_8, zero)) {
        APPLY_8
    }

    MERGEHL_U8(p5, p4)
    MERGEHL_U8(p3, p2)
    MERGEHL_U8(p1, p0)
    MERGEHL_U8(q0, q1)
    MERGEHL_U8(q2, q3)
    MERGEHL_U8(q4, q5)

    MERGEHL_U16(ab_p5p4p3p2, p5p4h, p3p2h)
    MERGEHL_U16(cd_p5p4p3p2, p5p4l, p3p2l)
    MERGEHL_U16(ab_p1p0q0q1, p1p0h, q0q1h)
    MERGEHL_U16(cd_p1p0q0q1, p1p0l, q0q1l)
    MERGEHL_U16(ab_q2q3q4q5, q2q3h, q4q5h)
    MERGEHL_U16(cd_q2q3q4q5, q2q3l, q4q5l)


    MERGEHL_U32(a_p5p4p3p2q2q3q4q5, ab_p5p4p3p2h, ab_q2q3q4q5h) // A0 p5p4p3p2 q2q3q4q5 A1
                                                                // A2                   A3
    MERGEHL_U32(a_p1p0q0q1q2q3q4q5, ab_p1p0q0q1h, ab_q2q3q4q5h) // A0 p1p0q0q1 q2q3q4q5 A1
                                                                // A2                   A3
    MERGEHL_U32(b_p5p4p3p2q2q3q4q5, ab_p5p4p3p2l, ab_q2q3q4q5l) // B0 p5p4p3p2 q2q3q4q5 B1
                                                                // A2                   A3
    MERGEHL_U32(b_p1p0q0q1q2q3q4q5, ab_p1p0q0q1l, ab_q2q3q4q5l) // B0 p1p0q0q1 q2q3q4q5 B1
                                                                // B2                   B3
    MERGEHL_U32(c_p5p4p3p2q2q3q4q5, cd_p5p4p3p2h, cd_q2q3q4q5h) // C0 p5p4p3p2 q2q3q4q5 C1
                                                                // C2                   C3
    MERGEHL_U32(c_p1p0q0q1q2q3q4q5, cd_p1p0q0q1h, cd_q2q3q4q5h) // C0 p1p0q0q1 q2q3q4q5 C1
                                                                // C2                   C3
    MERGEHL_U32(d_p5p4p3p2q2q3q4q5, cd_p5p4p3p2l, cd_q2q3q4q5l) // D0 p5p4p3p2 q2q3q4q5 D1
                                                                // D2                   D3
    MERGEHL_U32(d_p1p0q0q1q2q3q4q5, cd_p1p0q0q1l, cd_q2q3q4q5l) // D0 p1p0q0q1 q2q3q4q5 D1
                                                                // D2                   D3

    MERGEHL_U32(a01, a_p5p4p3p2q2q3q4q5h, a_p1p0q0q1q2q3q4q5h) // A0 p5p4p3p2 p1p0q0q1 q2q3q4q5 q2q3q4q5
                                                               // A1
    vec_xst_len(a01h, dst2, 12);
    dst2 += stridea;
    vec_xst_len(a01l, dst2, 12);
    dst2 += stridea;
    MERGEHL_U32(a23, a_p5p4p3p2q2q3q4q5l, a_p1p0q0q1q2q3q4q5l) // A2
                                                               // A3
    vec_xst_len(a23h, dst2, 12);
    dst2 += stridea;
    vec_xst_len(a23l, dst2, 12);
    dst2 += stridea;
    MERGEHL_U32(b01, b_p5p4p3p2q2q3q4q5h, b_p1p0q0q1q2q3q4q5h) // B0 p5p4p3p2 p1p0q0q1 q2q3q4q5 q2q3q4q5
                                                               // B1
    vec_xst_len(b01h, dst2, 12);
    dst2 += stridea;
    vec_xst_len(b01l, dst2, 12);
    dst2 += stridea;
    MERGEHL_U32(b23, b_p5p4p3p2q2q3q4q5l, b_p1p0q0q1q2q3q4q5l) // B2
                                                               // B3
    vec_xst_len(b23h, dst2, 12);
    dst2 += stridea;
    vec_xst_len(b23l, dst2, 12);
    dst2 += stridea;
    MERGEHL_U32(c01, c_p5p4p3p2q2q3q4q5h, c_p1p0q0q1q2q3q4q5h) // C0 p5p4p3p2 p1p0q0q1 q2q3q4q5 q2q3q4q5
                                                               // C1
    vec_xst_len(c01h, dst2, 12);
    dst2 += stridea;
    vec_xst_len(c01l, dst2, 12);
    dst2 += stridea;
    MERGEHL_U32(c23, c_p5p4p3p2q2q3q4q5l, c_p1p0q0q1q2q3q4q5l) // C2
                                                               // C3
    vec_xst_len(c23h, dst2, 12);
    dst2 += stridea;
    vec_xst_len(c23l, dst2, 12);
    dst2 += stridea;
    MERGEHL_U32(d01, d_p5p4p3p2q2q3q4q5h, d_p1p0q0q1q2q3q4q5h) // D0 p5p4p3p2 p1p0q0q1 q2q3q4q5 q2q3q4q5
                                                               // D1
    vec_xst_len(d01h, dst2, 12);
    dst2 += stridea;
    vec_xst_len(d01l, dst2, 12);
    dst2 += stridea;
    MERGEHL_U32(d23, d_p5p4p3p2q2q3q4q5l, d_p1p0q0q1q2q3q4q5l) // D2
                                                               // D3
    vec_xst_len(d23h, dst2, 12);
    dst2 += stridea;
    vec_xst_len(d23l, dst2, 12);
    dst2 += stridea;
}

static inline void
loop_filter_v_4_all(uint8_t *dst, u8x16 E, u8x16 I, u8x16 H,
                    const ptrdiff_t strideb, b32x4 apply)
{
    uint8_t *p1d = dst + strideb * -2;
    uint8_t *p0d = dst + strideb * -1;
    uint8_t *q0d = dst + strideb * +0;
    uint8_t *q1d = dst + strideb * +1;

    u8x16 p1 = vec_xl(0, p1d);
    u8x16 p0 = vec_xl(0, p0d);
    u8x16 q0 = vec_xl(0, q0d);
    u8x16 q1 = vec_xl(0, q1d);

    const u8x16 zero = vec_splat_u8(0);
    const u8x16 v1u8 = vec_splat_u8(1);
    const b8x16 s = (b8x16)vec_splats((uint8_t)128);

    const u8x16 a_p1_p0 = vec_absd(p1, p0);
    const u8x16 a_q1_q0 = vec_absd(q1, q0);
    const u8x16 a_p0_q0 = vec_absd(p0, q0);
    const u8x16 a_p1_q1 = vec_absd(p1, q1);

    u8x16 cmp_E = vec_adds(a_p0_q0, a_p0_q0);
    const u8x16 max_a_p1p0_q1q0 = vec_max(a_p1_p0, a_q1_q0);
    const u8x16 cmp_I = max_a_p1p0_q1q0;
    cmp_E = vec_adds(vec_sr(a_p1_q1, v1u8), cmp_E);
    const b8x16 ltI = vec_cmple(cmp_I, I);
    const b8x16 ltE = vec_cmple(cmp_E, E);
    b8x16 fm = vec_and(ltI, ltE);

    fm = vec_and(fm, (b8x16)apply);
    if (vec_all_eq(fm, zero))
        return;

    UNPACK_16(p0)
    UNPACK_16(q0)

    APPLY_4

    vec_xst(p0, 0, p0d);
    vec_xst(q0, 0, q0d);
    vec_xst(q1, 0, q1d);
    vec_xst(p1, 0, p1d);
}

static inline void
loop_filter_v_6_all(uint8_t *dst, u8x16 E, u8x16 I, u8x16 H,
                    const ptrdiff_t strideb, b32x4 apply, b32x4 m6)
{
    uint8_t *p2d = dst + strideb * -3;
    uint8_t *p1d = dst + strideb * -2;
    uint8_t *p0d = dst + strideb * -1;
    uint8_t *q0d = dst + strideb * +0;
    uint8_t *q1d = dst + strideb * +1;
    uint8_t *q2d = dst + strideb * +2;

    u8x16 p2 = vec_xl(0, p2d);
    u8x16 p1 = vec_xl(0, p1d);
    u8x16 p0 = vec_xl(0, p0d);
    u8x16 q0 = vec_xl(0, q0d);
    u8x16 q1 = vec_xl(0, q1d);
    u8x16 q2 = vec_xl(0, q2d);

    const u8x16 F = vec_splat_u8(1);

    const u8x16 zero = vec_splat_u8(0);
    const u16x8 v3u16 = vec_splat_u16(3);
    const u16x8 v4u16 = vec_splat_u16(4);
    const u8x16 v1u8 = vec_splat_u8(1);
    const b8x16 s = (b8x16)vec_splats((uint8_t)128);

    const u8x16 a_p1_p0 = vec_absd(p1, p0);
    const u8x16 a_q1_q0 = vec_absd(q1, q0);
    const u8x16 a_p0_q0 = vec_absd(p0, q0);
    const u8x16 a_p1_q1 = vec_absd(p1, q1);
    const u8x16 a_p2_p1 = vec_absd(p2, p1);
    const u8x16 a_q2_q1 = vec_absd(q2, q1);
    const u8x16 a_p2_p0 = vec_absd(p2, p0);
    const u8x16 a_q2_q0 = vec_absd(q2, q0);

    u8x16 max_a_p2p1_q2q1 = vec_max(a_p2_p1, a_q2_q1);
    u8x16 cmp_E = vec_adds(a_p0_q0, a_p0_q0);
    const u8x16 max_a_p1p0_q1q0 = vec_max(a_p1_p0, a_q1_q0);
    u8x16 max_a_p2p0_q2q0 = vec_max(a_p2_p0, a_q2_q0);
    u8x16 cmp_I_m6 = max_a_p2p1_q2q1;
    u8x16 cmp_I_m4 = max_a_p1p0_q1q0;
    cmp_E = vec_adds(vec_sr(a_p1_q1, v1u8), cmp_E);
    cmp_I_m6 = vec_and(cmp_I_m6, (u8x16)m6);
    u8x16 cmp_I = vec_max(cmp_I_m4, cmp_I_m6);
    const b8x16 ltE = vec_cmple(cmp_E, E);
    const b8x16 ltI = vec_cmple(cmp_I, I);
    b8x16 fm = vec_and(ltI, ltE);

    fm = vec_and(fm, (b8x16)apply);
    if (vec_all_eq(fm, zero))
        return;

    UNPACK_16(p2)
    UNPACK_16(p1)
    UNPACK_16(p0)
    UNPACK_16(q0)
    UNPACK_16(q1)
    UNPACK_16(q2)

    m6 = vec_and(m6, (b32x4)fm);

    u8x16 cmp_flat8in = vec_max(max_a_p2p0_q2q0, max_a_p1p0_q1q0);
    b8x16 apply_6 = vec_and(vec_cmple(cmp_flat8in, F), (b8x16)m6);

    b8x16 apply_4 = vec_andc(fm, apply_6);

    if (vec_any_ne(apply_4, zero)) {
        APPLY_4
    }

    if (vec_any_ne(apply_6, zero)) {
        DECLARE_ADD_16HL(p2p2, p2, p2)
        DECLARE_ADD_16HL(p2p1, p2, p1)
        DECLARE_ADD_16HL(p1p0, p1, p0)
        DECLARE_ADD_16HL(p0q0, p0, q0)
        DECLARE_ADD_16HL(q0q1, q0, q1)
        DECLARE_ADD_16HL(q1q2, q1, q2)
        DECLARE_ADD_16HL(p2p2p0q0, p2p2, p0q0)
        DECLARE_ADD_16HL(p2p1p1p0, p2p1, p1p0)
        DECLARE_ADD_16HL(p1p0q1q2, p1p0, q1q2)
        DECLARE_ADD_16HL(p0q0q0q1, p0q0, q0q1)
        u16x8 q1q2q2q2h = q2h * 3 + q1h;
        u16x8 q1q2q2q2l = q2l * 3 + q1l;

        DECLARE_ADD_16HL(op1, p2p2p0q0, p2p1p1p0)
        DECLARE_ADD_16HL(op0, p2p1p1p0, p0q0q0q1)
        DECLARE_ADD_16HL(oq0, p1p0q1q2, p0q0q0q1)
        DECLARE_ADD_16HL(oq1, p0q0q0q1, q1q2q2q2)

        ADD_AND_SHIFT4(op1)
        ADD_AND_SHIFT4(op0)
        ADD_AND_SHIFT4(oq0)
        ADD_AND_SHIFT4(oq1)

        p1 = PACK_AND_SEL(p1, apply_6);
        p0 = PACK_AND_SEL(p0, apply_6);
        q0 = PACK_AND_SEL(q0, apply_6);
        q1 = PACK_AND_SEL(q1, apply_6);
    }

    vec_xst(p0, 0, p0d);
    vec_xst(q0, 0, q0d);
    vec_xst(q1, 0, q1d);
    vec_xst(p1, 0, p1d);
}

static inline void
loop_filter_v_8_all(uint8_t *dst, u8x16 E, u8x16 I, u8x16 H,
                    const ptrdiff_t strideb, b32x4 apply, b32x4 m8)
{

    uint8_t *p3d = dst + strideb * -4;
    uint8_t *p2d = dst + strideb * -3;
    uint8_t *p1d = dst + strideb * -2;
    uint8_t *p0d = dst + strideb * -1;
    uint8_t *q0d = dst + strideb * +0;
    uint8_t *q1d = dst + strideb * +1;
    uint8_t *q2d = dst + strideb * +2;
    uint8_t *q3d = dst + strideb * +3;

    u8x16 p3 = vec_xl(0, p3d);
    u8x16 p2 = vec_xl(0, p2d);
    u8x16 p1 = vec_xl(0, p1d);
    u8x16 p0 = vec_xl(0, p0d);
    u8x16 q0 = vec_xl(0, q0d);
    u8x16 q1 = vec_xl(0, q1d);
    u8x16 q2 = vec_xl(0, q2d);
    u8x16 q3 = vec_xl(0, q3d);

    const u8x16 F = vec_splat_u8(1);

    const u8x16 zero = vec_splat_u8(0);
    const u16x8 v3u16 = vec_splat_u16(3);
    const u16x8 v4u16 = vec_splat_u16(4);
    const u8x16 v1u8 = vec_splat_u8(1);
    const b8x16 s = (b8x16)vec_splats((uint8_t)128);

    const u8x16 a_p1_p0 = vec_absd(p1, p0);
    const u8x16 a_q1_q0 = vec_absd(q1, q0);
    const u8x16 a_p0_q0 = vec_absd(p0, q0);
    const u8x16 a_p1_q1 = vec_absd(p1, q1);
    const u8x16 a_p2_p1 = vec_absd(p2, p1);
    const u8x16 a_q2_q1 = vec_absd(q2, q1);
    const u8x16 a_p2_p0 = vec_absd(p2, p0);
    const u8x16 a_q2_q0 = vec_absd(q2, q0);
    const u8x16 a_p3_p0 = vec_absd(p3, p0);
    const u8x16 a_q3_q0 = vec_absd(q3, q0);
    const u8x16 a_p3_p2 = vec_absd(p3, p2);
    const u8x16 a_q3_q2 = vec_absd(q3, q2);

    u8x16 max_a_p2p1_q2q1 = vec_max(a_p2_p1, a_q2_q1);
    u8x16 max_a_p3p2_q3q2 = vec_max(a_p3_p2, a_q3_q2);
    u8x16 cmp_E = vec_adds(a_p0_q0, a_p0_q0);
    const u8x16 max_a_p1p0_q1q0 = vec_max(a_p1_p0, a_q1_q0);
    const u8x16 max_a_p2p0_q2q0 = vec_max(a_p2_p0, a_q2_q0);
    u8x16 max_a_p3p0_q3q0 = vec_max(a_p3_p0, a_q3_q0);
    u8x16 cmp_I_m8 = vec_max(max_a_p2p1_q2q1, max_a_p3p2_q3q2);
    u8x16 cmp_I_m4 = max_a_p1p0_q1q0;
    cmp_E = vec_adds(vec_sr(a_p1_q1, v1u8), cmp_E);
    cmp_I_m8 = vec_and(cmp_I_m8, (u8x16)m8);
    u8x16 cmp_I = vec_max(cmp_I_m4, cmp_I_m8);
    const b8x16 ltE = vec_cmple(cmp_E, E);
    const b8x16 ltI = vec_cmple(cmp_I, I);
    b8x16 fm = vec_and(ltI, ltE);

    fm = vec_and(fm, (b8x16)apply);
    if (vec_all_eq(fm, zero))
        return;

    #define UNPACK_16(v) \
        u16x8 v##h = u8h_to_u16(v); \
        u16x8 v##l = u8l_to_u16(v);

    UNPACK_16(p3)
    UNPACK_16(p2)
    UNPACK_16(p1)
    UNPACK_16(p0)
    UNPACK_16(q0)
    UNPACK_16(q1)
    UNPACK_16(q2)
    UNPACK_16(q3)

    m8 = vec_and(m8, (b32x4)fm);

    u8x16 cmp_flat8in = vec_max(max_a_p2p0_q2q0, max_a_p1p0_q1q0);
    cmp_flat8in = vec_max(max_a_p3p0_q3q0, cmp_flat8in);
    b8x16 apply_8 = vec_and(vec_cmple(cmp_flat8in, F), (b8x16)m8);

    b8x16 apply_4 = vec_andc(fm, apply_8);

    if (vec_any_ne(apply_4, zero)) {
        APPLY_4
    }

    if (vec_any_ne(apply_8, zero)) {
        APPLY_8
    }

    vec_xst(p0, 0, p0d);
    vec_xst(q0, 0, q0d);
    vec_xst(q1, 0, q1d);
    vec_xst(p1, 0, p1d);
    vec_xst(q2, 0, q2d);
    vec_xst(p2, 0, p2d);
}

static inline void
loop_filter_v_16_all(uint8_t *dst, u8x16 E, u8x16 I, u8x16 H,
                    const ptrdiff_t strideb, b32x4 apply, b32x4 m8, b32x4 m16)
{

    uint8_t *p6d = dst + strideb * -7;
    uint8_t *p5d = dst + strideb * -6;
    uint8_t *p4d = dst + strideb * -5;
    uint8_t *p3d = dst + strideb * -4;
    uint8_t *p2d = dst + strideb * -3;
    uint8_t *p1d = dst + strideb * -2;
    uint8_t *p0d = dst + strideb * -1;
    uint8_t *q0d = dst + strideb * +0;
    uint8_t *q1d = dst + strideb * +1;
    uint8_t *q2d = dst + strideb * +2;
    uint8_t *q3d = dst + strideb * +3;
    uint8_t *q4d = dst + strideb * +4;
    uint8_t *q5d = dst + strideb * +5;
    uint8_t *q6d = dst + strideb * +6;

    u8x16 p6 = vec_xl(0, p6d);
    u8x16 p5 = vec_xl(0, p5d);
    u8x16 p4 = vec_xl(0, p4d);
    u8x16 p3 = vec_xl(0, p3d);
    u8x16 p2 = vec_xl(0, p2d);
    u8x16 p1 = vec_xl(0, p1d);
    u8x16 p0 = vec_xl(0, p0d);
    u8x16 q0 = vec_xl(0, q0d);
    u8x16 q1 = vec_xl(0, q1d);
    u8x16 q2 = vec_xl(0, q2d);
    u8x16 q3 = vec_xl(0, q3d);
    u8x16 q4 = vec_xl(0, q4d);
    u8x16 q5 = vec_xl(0, q5d);
    u8x16 q6 = vec_xl(0, q6d);

    const u8x16 F = vec_splat_u8(1);

    const u8x16 zero = vec_splat_u8(0);
    const u16x8 v3u16 = vec_splat_u16(3);
    const u16x8 v4u16 = vec_splat_u16(4);
    const u16x8 v8u16 = vec_splat_u16(8);
    const u8x16 v1u8 = vec_splat_u8(1);
    const b8x16 s = (b8x16)vec_splats((uint8_t)128);

    const u8x16 a_p6_p0 = vec_absd(p6, p0);
    const u8x16 a_p5_p0 = vec_absd(p5, p0);
    const u8x16 a_p4_p0 = vec_absd(p4, p0);
    const u8x16 a_q4_q0 = vec_absd(q4, q0);
    const u8x16 a_q5_q0 = vec_absd(q5, q0);
    const u8x16 a_q6_q0 = vec_absd(q6, q0);

    const u8x16 a_p1_p0 = vec_absd(p1, p0);
    const u8x16 a_q1_q0 = vec_absd(q1, q0);
    const u8x16 a_p0_q0 = vec_absd(p0, q0);
    const u8x16 a_p1_q1 = vec_absd(p1, q1);
    const u8x16 a_p2_p1 = vec_absd(p2, p1);
    const u8x16 a_q2_q1 = vec_absd(q2, q1);
    const u8x16 a_p2_p0 = vec_absd(p2, p0);
    const u8x16 a_q2_q0 = vec_absd(q2, q0);
    const u8x16 a_p3_p0 = vec_absd(p3, p0);
    const u8x16 a_q3_q0 = vec_absd(q3, q0);
    const u8x16 a_p3_p2 = vec_absd(p3, p2);
    const u8x16 a_q3_q2 = vec_absd(q3, q2);

    u8x16 max_a_p2p1_q2q1 = vec_max(a_p2_p1, a_q2_q1);
    u8x16 max_a_p3p2_q3q2 = vec_max(a_p3_p2, a_q3_q2);
    u8x16 cmp_E = vec_adds(a_p0_q0, a_p0_q0);
    const u8x16 max_a_p1p0_q1q0 = vec_max(a_p1_p0, a_q1_q0);
    const u8x16 max_a_p2p0_q2q0 = vec_max(a_p2_p0, a_q2_q0);

    const u8x16 max_a_p4p0_q4q0 = vec_max(a_p4_p0, a_q4_q0);
    const u8x16 max_a_p5p0_q5q0 = vec_max(a_p5_p0, a_q5_q0);
    const u8x16 max_a_p6p0_q6q0 = vec_max(a_p6_p0, a_q6_q0);

    b32x4 m8_16 = vec_or(m8, m16);

    u8x16 max_a_p3p0_q3q0 = vec_max(a_p3_p0, a_q3_q0);
    u8x16 cmp_I_m8 = vec_max(max_a_p2p1_q2q1, max_a_p3p2_q3q2);
    u8x16 cmp_I_m4 = max_a_p1p0_q1q0;
    cmp_E = vec_adds(vec_sr(a_p1_q1, v1u8), cmp_E);
    cmp_I_m8 = vec_and(cmp_I_m8, (u8x16)m8_16);
    u8x16 cmp_I = vec_max(cmp_I_m4, cmp_I_m8);
    const b8x16 ltE = vec_cmple(cmp_E, E);
    const b8x16 ltI = vec_cmple(cmp_I, I);
    b8x16 fm = vec_and(ltI, ltE);

    fm = vec_and(fm, (b8x16)apply);
    if (vec_all_eq(fm, zero))
        return;

    u8x16 cmp_flat8in = vec_max(max_a_p2p0_q2q0, max_a_p1p0_q1q0);
    u8x16 cmp_flat8out = vec_max(max_a_p6p0_q6q0, max_a_p5p0_q5q0);

    m8_16 = vec_and(m8_16, (b32x4)fm);
    m16 = vec_and(m16, (b32x4)fm);

    cmp_flat8in = vec_max(max_a_p3p0_q3q0, cmp_flat8in);
    cmp_flat8out = vec_max(max_a_p4p0_q4q0, cmp_flat8out);
    b8x16 flat8in = vec_cmple(cmp_flat8in, F);
    b8x16 flat8out = vec_cmple(cmp_flat8out, F);
    flat8in = vec_and(flat8in, (b8x16)m8_16);
    flat8out = vec_and(flat8out, (b8x16)m16);

    b8x16 apply_16 = vec_and(flat8out, flat8in);
    b8x16 apply_8 = vec_andc(flat8in, flat8out);

    UNPACK_16(p6)
    UNPACK_16(p5)
    UNPACK_16(p4)
    UNPACK_16(p3)
    UNPACK_16(p2)
    UNPACK_16(p1)
    UNPACK_16(p0)

    b8x16 apply_4 = vec_nor(apply_16, apply_8);

    UNPACK_16(q0)
    UNPACK_16(q1)
    UNPACK_16(q2)
    UNPACK_16(q3)
    UNPACK_16(q4)
    UNPACK_16(q5)
    UNPACK_16(q6)

    if (vec_any_ne(apply_4, zero)) {
        APPLY_4
    }
    if (vec_any_ne(apply_16, zero)) {
        APPLY_16
    }
    if (vec_any_ne(apply_8, zero)) {
        APPLY_8
    }

    vec_xst(p5, 0, p5d);
    vec_xst(p4, 0, p4d);
    vec_xst(p3, 0, p3d);
    vec_xst(p2, 0, p2d);
    vec_xst(p1, 0, p1d);
    vec_xst(p0, 0, p0d);
    vec_xst(q0, 0, q0d);
    vec_xst(q1, 0, q1d);
    vec_xst(q2, 0, q2d);
    vec_xst(q3, 0, q3d);
    vec_xst(q4, 0, q4d);
    vec_xst(q5, 0, q5d);
}

#if defined(DAV1D_VSX)
#define LPF(fn) BF(dav1d_lpf_##fn, vsx)
#elif defined(DAV1D_PWR9)
#define LPF(fn) BF(dav1d_lpf_##fn, pwr9)
#endif

void LPF(h_sb_y)(pixel *dst, const ptrdiff_t stride,
                 const uint32_t *const vmask,
                 const uint8_t (*l)[4], ptrdiff_t b4_stride,
                 const Av1FilterLUT *lut, const int h)
{
    unsigned vm = vmask[0] | vmask[1] | vmask[2];

    u32x4 vm0 = vec_splats(vmask[0] | vmask[1] | vmask[2]);
    u32x4 vm1 = vec_splats(vmask[1]);
    u32x4 vm2 = vec_splats(vmask[2]);
    u32x4 mm = (u32x4){1, 2, 4, 8};

    const u8x16 sharp = vec_xl(0, (uint8_t *)lut->sharp);
    const u8x16 s0 = vec_splat(sharp, 0);
    const u8x16 s1 = vec_splat(sharp, 8);
    const u32x4 v4u32 = vec_splat_u32(4);
    const u32x4 zero = vec_splat_u32(0);
    const u8x16 v1u8 = vec_splat_u8(1);
    const u8x16 v2u8 = vec_splat_u8(2);
    const u8x16 v4u8 = vec_splat_u8(4);
    const uint8_t (*pl)[4] = &l[-1];

    const u8x16 spread = (u8x16){
        0x00, 0x00, 0x00, 0x00,
        0x04, 0x04, 0x04, 0x04,
        0x08, 0x08, 0x08, 0x08,
        0x0c, 0x0c, 0x0c, 0x0c,
    };

    for (;
        vm;
        vm >>= 4,
        mm = vec_sl(mm, v4u32),
        dst += 4 * 4 * PXSTRIDE(stride),
        pl += 4 * b4_stride) {
        if (!(vm & 0x0f))
            continue;
        u32x4 la = (u32x4)vec_xl(0, (uint8_t *)pl);  // l[-1] l[0] ...
        u32x4 lb = (u32x4)vec_xl(1 * 4 * b4_stride, (uint8_t *)pl);
        u32x4 lc = (u32x4)vec_xl(2 * 4 * b4_stride, (uint8_t *)pl);
        u32x4 ld = (u32x4)vec_xl(3 * 4 * b4_stride, (uint8_t *)pl);

        u32x4 Lac = vec_mergeh(la, lc); // la[-1] lb[-1] la[0] lb[0]
        u32x4 Lbd = vec_mergeh(lb, ld); // lc[-1] ld[-1] lc[0] ld[0]

        u32x4 wd16 = vec_and(vm2, mm); // vmask[2] & [1,2,4,8]
        u32x4 wd8 = vec_and(vm1, mm); // vmask[1] & [1,2,4,8]
        u32x4 wd4 = vec_and(vm0, mm); // vm & [1,2,4,8]

        u32x4 L_1 = (u32x4)vec_mergeh(Lac, Lbd); // la[-1] lb[-1] lc[-1] ld[-1]
        u32x4 L_0 = (u32x4)vec_mergel(Lac, Lbd); // la[ 0] lb[ 0] lc[ 0] ld[ 0]

        b8x16 mask = vec_cmpeq((u8x16)L_0, (u8x16)zero);

        u32x4 L4 = (u32x4)vec_sel((u8x16)L_0, (u8x16)L_1, mask); // if !l[0][0] { l[-1][0] }

        u8x16 L = (u8x16)vec_perm((u8x16)L4, (u8x16)L4, spread); // La La La La Lb Lb Lb Lb ...

        b32x4 m16 = vec_cmpeq(wd16, mm);
        b32x4 m8 = vec_cmpeq(wd8, mm);
        b32x4 m4 = vec_cmpeq(wd4, mm);

        b32x4 apply = vec_cmpne((u32x4)L, zero);

        if (vec_all_eq((u32x4)L, zero))
            continue;

        u8x16 I = vec_sr(L, s0); // L >> sharp[0]
        u8x16 H = vec_sr(L, v4u8);
        I = vec_min(I, s1); // min(L >> sharp[0], sharp[1])
        u8x16 E = vec_add(L, v2u8); // L + 2
        I = vec_max(I, v1u8); // max(min(L >> sharp[0], sharp[1]), 1)
        E = vec_add(E, E); // 2 * (L + 2)
        E = vec_add(E, I); // 2 * (L + 2) + limit

        apply = vec_and(m4, apply);

        if (vec_any_ne(wd16, zero)) {
            loop_filter_h_16_all(dst, E, I, H, PXSTRIDE(stride), apply, m8, m16);
        } else if (vec_any_ne(wd8, zero)) {
            loop_filter_h_8_all(dst, E, I, H, PXSTRIDE(stride), apply, m8);
        } else { // wd4 == 0 already tested
            loop_filter_h_4_all(dst, E, I, H, PXSTRIDE(stride), apply);
        }
    }
}

void LPF(v_sb_y)(pixel *dst, const ptrdiff_t stride,
                 const uint32_t *const vmask,
                 const uint8_t (*l)[4], ptrdiff_t b4_stride,
                 const Av1FilterLUT *lut, const int w)
{
    unsigned vm = vmask[0] | vmask[1] | vmask[2];

    u32x4 vm0 = vec_splats(vmask[0] | vmask[1] | vmask[2]);
    u32x4 vm1 = vec_splats(vmask[1]);
    u32x4 vm2 = vec_splats(vmask[2]);

    u8x16 sharp = vec_xl(0, (uint8_t *)lut->sharp);
    u8x16 s0 = vec_splat(sharp, 0);
    u8x16 s1 = vec_splat(sharp, 8);
    u32x4 mm = (u32x4){1, 2, 4, 8};
    u32x4 v4u32 = vec_splat_u32(4);
    u32x4 zero = vec_splat_u32(0);
    u8x16 v1u8 = vec_splat_u8(1);
    u8x16 v2u8 = vec_splat_u8(2);
    u8x16 v4u8 = vec_splat_u8(4);
    const uint8_t (*pl)[4] = l;
    const uint8_t (*plb4)[4] = l - b4_stride;
    const u8x16 spread = (u8x16){
        0x00, 0x00, 0x00, 0x00,
        0x04, 0x04, 0x04, 0x04,
        0x08, 0x08, 0x08, 0x08,
        0x0c, 0x0c, 0x0c, 0x0c,
    };

    for (;
        vm;
        vm >>= 4,
        mm = vec_sl(mm, v4u32),
        dst += 4 * 4,
        pl += 4,
        plb4 += 4) {
        if (!(vm & 0x0f))
            continue;
        u32x4 L_0  = (u32x4)vec_xl(0, (uint8_t *)pl);
        u32x4 L_b4 = (u32x4)vec_xl(0, (uint8_t *)plb4);

        u32x4 wd16 = vec_and(vm2, mm); // vmask[2] & [1,2,4,8]
        u32x4 wd8 = vec_and(vm1, mm); // vmask[1] & [1,2,4,8]
        u32x4 wd4 = vec_and(vm0, mm); // vm & [1,2,4,8]

        b8x16 mask = vec_cmpeq((u8x16)L_0, (u8x16)zero);

        u32x4 L4 = (u32x4)vec_sel((u8x16)L_0, (u8x16)L_b4, mask); // if !l[0][0] { l[-b4_stride][0] }

        u8x16 L = (u8x16)vec_perm((u8x16)L4, (u8x16)L4, spread); // La La La La Lb Lb Lb Lb ...

        b32x4 m16 = vec_cmpeq(wd16, mm);
        b32x4 m8 = vec_cmpeq(wd8, mm);
        b32x4 m4 = vec_cmpeq(wd4, mm);

        b32x4 apply = vec_cmpne((u32x4)L, zero);

        if (vec_all_eq((u32x4)L, zero))
            continue;

        u8x16 I = vec_sr(L, s0); // L >> sharp[0]
        u8x16 H = vec_sr(L, v4u8);
        I = vec_min(I, s1); // min(L >> sharp[0], sharp[1])
        u8x16 E = vec_add(L, v2u8); // L + 2
        I = vec_max(I, v1u8); // max(min(L >> sharp[0], sharp[1]), 1)
        E = vec_add(E, E); // 2 * (L + 2)
        E = vec_add(E, I); // 2 * (L + 2) + limit

        apply = vec_and(apply, m4);

        if (vec_any_ne(wd16, zero)) {
            loop_filter_v_16_all(dst, E, I, H, PXSTRIDE(stride), apply, m8, m16);
        } else if (vec_any_ne(wd8, zero)) {
            loop_filter_v_8_all(dst, E, I, H, PXSTRIDE(stride), apply, m8);
        } else {
            loop_filter_v_4_all(dst, E, I, H, PXSTRIDE(stride), apply);
        }

    }
}

void LPF(h_sb_uv)(pixel *dst, const ptrdiff_t stride,
                  const uint32_t *const vmask,
                  const uint8_t (*l)[4], ptrdiff_t b4_stride,
                  const Av1FilterLUT *lut, const int h)
{
    unsigned vm = vmask[0] | vmask[1];
    u32x4 vm0 = vec_splats(vm);
    u32x4 vm1 = vec_splats(vmask[1]);
    u32x4 mm = (u32x4){1, 2, 4, 8};

    const u8x16 sharp = vec_xl(0, (uint8_t *)lut->sharp);
    const u8x16 s0 = vec_splat(sharp, 0);
    const u8x16 s1 = vec_splat(sharp, 8);
    const u32x4 v4u32 = vec_splat_u32(4);
    const u32x4 zero = vec_splat_u32(0);
    const u8x16 v1u8 = vec_splat_u8(1);
    const u8x16 v2u8 = vec_splat_u8(2);
    const u8x16 v4u8 = vec_splat_u8(4);
    const uint8_t (*pl)[4] = &l[-1];
    const u8x16 spread = (u8x16){
        0x00, 0x00, 0x00, 0x00,
        0x04, 0x04, 0x04, 0x04,
        0x08, 0x08, 0x08, 0x08,
        0x0c, 0x0c, 0x0c, 0x0c,
    };

    for (;
        vm;
        vm >>= 4,
        mm = vec_sl(mm, v4u32),
        dst += 4 * 4 * PXSTRIDE(stride),
        pl += 4 * b4_stride) {
        if (!(vm & 0x0f))
            continue;
        u32x4 la = (u32x4)vec_xl(0, (uint8_t *)pl);  // l[-1] l[0] ...
        u32x4 lb = (u32x4)vec_xl(1 * 4 * b4_stride, (uint8_t *)pl);
        u32x4 lc = (u32x4)vec_xl(2 * 4 * b4_stride, (uint8_t *)pl);
        u32x4 ld = (u32x4)vec_xl(3 * 4 * b4_stride, (uint8_t *)pl);

        u32x4 Lac = vec_mergeh(la, lc); // la[-1] lb[-1] la[0] lb[0]
        u32x4 Lbd = vec_mergeh(lb, ld); // lc[-1] ld[-1] lc[0] ld[0]

        u32x4 wd6 = vec_and(vm1, mm); // vmask[1] & [1,2,4,8]
        u32x4 wd4 = vec_and(vm0, mm); // vm & [1,2,4,8]

        u32x4 L_1 = (u32x4)vec_mergeh(Lac, Lbd); // la[-1] lb[-1] lc[-1] ld[-1]
        u32x4 L_0 = (u32x4)vec_mergel(Lac, Lbd); // la[ 0] lb[ 0] lc[ 0] ld[ 0]

        b8x16 mask = vec_cmpeq((u8x16)L_0, (u8x16)zero);

        u32x4 L4 = (u32x4)vec_sel((u8x16)L_0, (u8x16)L_1, mask); // if !l[0][0] { l[-1][0] }

        u8x16 L = (u8x16)vec_perm((u8x16)L4, (u8x16)L4, spread); // La La La La Lb Lb Lb Lb ...

        b32x4 m6 = vec_cmpeq(wd6, mm);
        b32x4 m4 = vec_cmpeq(wd4, mm);

        b32x4 apply = vec_cmpne((u32x4)L, zero);

        if (vec_all_eq((u32x4)L, zero))
            continue;

        u8x16 I = vec_sr(L, s0); // L >> sharp[0]
        u8x16 H = vec_sr(L, v4u8);
        I = vec_min(I, s1); // min(L >> sharp[0], sharp[1])
        u8x16 E = vec_add(L, v2u8); // L + 2
        I = vec_max(I, v1u8); // max(min(L >> sharp[0], sharp[1]), 1)
        E = vec_add(E, E); // 2 * (L + 2)
        E = vec_add(E, I); // 2 * (L + 2) + limit

        apply = vec_and(m4, apply);

        if (vec_any_ne(wd6, zero)) {
            loop_filter_h_6_all(dst, E, I, H, PXSTRIDE(stride), apply, m6);
            // loop_filter_h_8
        } else { // wd4 == 0 already tested
            loop_filter_h_4_all(dst, E, I, H, PXSTRIDE(stride), apply);

            // loop_filter_h_4
        }

    }
}

void LPF(v_sb_uv)(pixel *dst, const ptrdiff_t stride,
                  const uint32_t *const vmask,
                  const uint8_t (*l)[4], ptrdiff_t b4_stride,
                  const Av1FilterLUT *lut, const int w)
{
    unsigned vm = vmask[0] | vmask[1];

    u32x4 vm0 = vec_splats(vm);
    u32x4 vm1 = vec_splats(vmask[1]);

    u8x16 sharp = vec_xl(0, (uint8_t *)lut->sharp);
    u8x16 s0 = vec_splat(sharp, 0);
    u8x16 s1 = vec_splat(sharp, 8);
    u32x4 mm = (u32x4){1, 2, 4, 8};
    u32x4 v4u32 = vec_splat_u32(4);
    u32x4 zero = vec_splat_u32(0);
    u8x16 v1u8 = vec_splat_u8(1);
    u8x16 v2u8 = vec_splat_u8(2);
    u8x16 v4u8 = vec_splat_u8(4);
    const uint8_t (*pl)[4] = l;
    const uint8_t (*plb4)[4] = l - b4_stride;
    const u8x16 spread = (u8x16){
        0x00, 0x00, 0x00, 0x00,
        0x04, 0x04, 0x04, 0x04,
        0x08, 0x08, 0x08, 0x08,
        0x0c, 0x0c, 0x0c, 0x0c,
    };

    for (;
        vm;
        vm >>= 4,
        mm = vec_sl(mm, v4u32),
        dst += 4 * 4,
        pl += 4,
        plb4 += 4) {
        if (!(vm & 0x0f))
            continue;
        u32x4 L_0  = (u32x4)vec_xl(0, (uint8_t *)pl);
        u32x4 L_b4 = (u32x4)vec_xl(0, (uint8_t *)plb4);

        u32x4 wd6 = vec_and(vm1, mm); // vmask[1] & [1,2,4,8]
        u32x4 wd4 = vec_and(vm0, mm); // vm & [1,2,4,8]

        b8x16 mask = vec_cmpeq((u8x16)L_0, (u8x16)zero);

        u32x4 L4 = (u32x4)vec_sel((u8x16)L_0, (u8x16)L_b4, mask); // if !l[0][0] { l[-b4_stride][0] }

        u8x16 L = (u8x16)vec_perm((u8x16)L4, (u8x16)L4, spread); // La La La La Lb Lb Lb Lb ...

        b32x4 m6 = vec_cmpeq(wd6, mm);
        b32x4 m4 = vec_cmpeq(wd4, mm);

        b32x4 apply = vec_cmpne((u32x4)L, zero);

        if (vec_all_eq((u32x4)L, zero))
            continue;

        u8x16 I = vec_sr(L, s0); // L >> sharp[0]
        u8x16 H = vec_sr(L, v4u8);
        I = vec_min(I, s1); // min(L >> sharp[0], sharp[1])
        u8x16 E = vec_add(L, v2u8); // L + 2
        I = vec_max(I, v1u8); // max(min(L >> sharp[0], sharp[1]), 1)
        E = vec_add(E, E); // 2 * (L + 2)
        E = vec_add(E, I); // 2 * (L + 2) + limit

        apply = vec_and(apply, m4);

        if (vec_any_ne(wd6, zero)) {
            loop_filter_v_6_all(dst, E, I, H, PXSTRIDE(stride), apply, m6);
        } else {
            loop_filter_v_4_all(dst, E, I, H, PXSTRIDE(stride), apply);
        }
    }
}

#endif // BITDEPTH

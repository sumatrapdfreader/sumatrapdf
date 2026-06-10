/*
 * Copyright © 2024, Arm Limited
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

#include "src/arm/asm.S"
#include "util.S"

#define PREP_BIAS 32, lsl #8        // 8192
#define PREP_BIAS_NEG 224, lsl #8   // -8192

#if HAVE_SVE2
ENABLE_SVE
ENABLE_SVE2

// No spaces in these expressions, due to gas-preprocessor. It is translated by
// -1 to save the negative offset when getting the address of `mc_subpel_filters`.
#define REGULAR1        (((0*15-1)<<7)|(3*15-1))
#define SMOOTH1         (((1*15-1)<<7)|(4*15-1))
#define SHARP1          (((2*15-1)<<7)|(3*15-1))

#define FUNC_ALIGN      2
#define JUMP_ALIGN      2
#define LOOP_ALIGN      2


// Shuffle indices to permute horizontal samples in preparation for input to
// 16-bit SDOT instructions. The 8-tap horizontal convolution uses sample
// indices in the interval of [-3, 4] relative to the current sample position.
const h_tbl_sve, align=4
        .byte  0,  1,  2,  3,  4,  5,  6,  7,   2,  3,  4,  5,  6,  7,  8,  9
        .byte  4,  5,  6,  7,  8,  9, 10, 11,   6,  7,  8,  9, 10, 11, 12, 13
endconst

// Vertical convolutions also use 16-bit SDOT instructions, where two 128-bit
// registers contain a transposed 4x4 matrix of values. Subsequent iterations
// of the vertical convolution can reuse the 3x4 sub-matrix from the previous
// loop iteration. These shuffle indices shift and merge this 4x4 matrix with
// the values of a new line.
const v_tbl_sve, align=4
        .byte  2,  3,  4,  5,  6,  7, 16, 17,  10, 11, 12, 13, 14, 15, 24, 25
        .byte  2,  3,  4,  5,  6,  7, 16, 17,  10, 11, 12, 13, 14, 15, 18, 19
        .byte  2,  3,  4,  5,  6,  7, 20, 21,  10, 11, 12, 13, 14, 15, 22, 23
        .byte  2,  3,  4,  5,  6,  7, 24, 25,  10, 11, 12, 13, 14, 15, 26, 27
        .byte  2,  3,  4,  5,  6,  7, 28, 29,  10, 11, 12, 13, 14, 15, 30, 31
endconst


.macro make_8tap_fn op, type, type_h, type_v, isa, jump=1
function \op\()_8tap_\type\()_16bpc_\isa, export=1, align=FUNC_ALIGN
        mov             x9,  \type_h
        mov             x10, \type_v
    .if \jump
        b               \op\()_8tap_\isa
    .endif
endfunc
.endm

.macro filter_8tap_fn type, isa, dst, d_strd, src, s_strd, w, h, mx, my, bdmax, xmx, xmy, ldst, lsrc, wd_strd, ws_strd
make_8tap_fn \type, sharp,          SHARP1,   SHARP1,   \isa
make_8tap_fn \type, sharp_smooth,   SHARP1,   SMOOTH1,  \isa
make_8tap_fn \type, sharp_regular,  SHARP1,   REGULAR1, \isa
make_8tap_fn \type, smooth_sharp,   SMOOTH1,  SHARP1,   \isa
make_8tap_fn \type, smooth,         SMOOTH1,  SMOOTH1,  \isa
make_8tap_fn \type, smooth_regular, SMOOTH1,  REGULAR1, \isa
make_8tap_fn \type, regular_sharp,  REGULAR1, SHARP1,   \isa
make_8tap_fn \type, regular_smooth, REGULAR1, SMOOTH1,  \isa
make_8tap_fn \type, regular,        REGULAR1, REGULAR1, \isa, jump=0

function \type\()_8tap_\isa, align=FUNC_ALIGN
        clz             w8, \w
        mov             w11, #0x4081                    // (1<<14) | (1<<7) | 1
        ptrue           p0.b, vl16
        sub             w8, w8, #24                     // for jump tables
        movrel          x12, X(mc_subpel_filters)
        cbnz            \mx, L(\type\()_8tap_h_hv_\isa)
.ifc \type, prep
        cbz             \my, prep_sve
.else   // put
        cbnz            \my, L(\type\()_8tap_v_\isa)
        mov             w9, w8
        b               X(put_16bpc_neon)

        .align JUMP_ALIGN
.endif

L(\type\()_8tap_v_\isa):
        madd            \my, \my, w11, w10
        movrel          x13, v_tbl_sve
.ifc \bdmax, w8                                         // put case, but skip
        ld1r            {v5.8h}, [sp]                   // loading into w8
.endif
        sub             \src, \src, \s_strd             // src - s_strd
        ubfx            w11, \my, #7, #7
        and             \my, \my, #0x7F
        ldr             q6, [x13]
        cmp             \h, #4
        csel            \my, \my, w11, le
        sub             \src, \src, \s_strd, lsl #1     // src - 3 * s_strd
        add             \xmy, x12, \xmy, lsl #3         // subpel V filter address
        ldp             q28, q29, [x13, #16]
        ld1sb           {z7.h}, p0/z, [\xmy]
.ifc \type, prep
        clz             \bdmax, \bdmax
        sub             \bdmax, \bdmax, #24
        dup             v5.4s, \bdmax
.endif
        cmp             \w, #8
        b.lt            40f

        // .align JUMP_ALIGN   // fallthrough
80:     // V - 8xN+
        ldp             q30, q31, [x13, #48]
.ifc \type, prep
        add             \wd_strd, \w, \w                // d_strd = 2 * w
.endif
        .align LOOP_ALIGN
81:
        add             \lsrc, \src, \s_strd, lsl #1

        ldr             q16, [\src]
        ldr             q17, [\src, \s_strd]
        ldr             q18, [\lsrc]
        ldr             q19, [\lsrc, \s_strd]
        add             \lsrc, \lsrc, \s_strd, lsl #1
        mov             \ldst, \dst

        ldr             q20, [\lsrc]
        ldr             q21, [\lsrc, \s_strd]
        add             \lsrc, \lsrc, \s_strd, lsl #1
        ldr             q22, [\lsrc]
        ldr             q23, [\lsrc, \s_strd]
        add             \lsrc, \lsrc, \s_strd, lsl #1
        sub             w8, \h, #1

        zip1            v0.8h, v16.8h, v17.8h
        zip2            v1.8h, v16.8h, v17.8h
        zip1            v2.8h, v18.8h, v19.8h
        zip2            v3.8h, v18.8h, v19.8h

        zip1            v18.8h, v20.8h, v21.8h
        zip2            v21.8h, v20.8h, v21.8h
        zip1            v24.8h, v22.8h, v23.8h
        zip2            v27.8h, v22.8h, v23.8h

        zip1            v16.4s, v0.4s, v2.4s
        zip2            v19.4s, v0.4s, v2.4s
        zip1            v22.4s, v1.4s, v3.4s
        zip2            v25.4s, v1.4s, v3.4s

        zip1            v17.4s, v18.4s, v24.4s
        zip2            v20.4s, v18.4s, v24.4s
        zip1            v23.4s, v21.4s, v27.4s
        zip2            v26.4s, v21.4s, v27.4s

        .align LOOP_ALIGN
8:
        ld1             {v18.16b}, [\lsrc], \s_strd

        movi            v0.2d, #0
        movi            v1.2d, #0
        movi            v2.2d, #0
        movi            v3.2d, #0
        mov             v21.16b, v18.16b
        mov             v24.16b, v18.16b
        mov             v27.16b, v18.16b

        sdot            z0.d, z16.h, z7.h[0]
        tbl             v16.16b, {v16.16b, v17.16b}, v6.16b
        sdot            z1.d, z19.h, z7.h[0]
        tbl             v19.16b, {v19.16b, v20.16b}, v6.16b
        sdot            z2.d, z22.h, z7.h[0]
        tbl             v22.16b, {v22.16b, v23.16b}, v6.16b
        subs            w8, w8, #1
        sdot            z3.d, z25.h, z7.h[0]
        tbl             v25.16b, {v25.16b, v26.16b}, v6.16b

        sdot            z0.d, z17.h, z7.h[1]
        tbl             v17.16b, {v17.16b, v18.16b}, v28.16b
        sdot            z1.d, z20.h, z7.h[1]
        tbl             v20.16b, {v20.16b, v21.16b}, v29.16b
        sdot            z2.d, z23.h, z7.h[1]
        tbl             v23.16b, {v23.16b, v24.16b}, v30.16b
        sdot            z3.d, z26.h, z7.h[1]
        tbl             v26.16b, {v26.16b, v27.16b}, v31.16b

        uzp1            v0.4s, v0.4s, v1.4s
        uzp1            v1.4s, v2.4s, v3.4s
.ifc \type, prep
        srshl           v0.4s, v0.4s, v5.4s
        srshl           v1.4s, v1.4s, v5.4s
        uzp1            v0.8h, v0.8h, v1.8h
        sub             z0.h, z0.h, #PREP_BIAS
.else   // put
        sqrshrun        v0.4h, v0.4s, #6
        sqrshrun2       v0.8h, v1.4s, #6
        umin            v0.8h, v0.8h, v5.8h
.endif
        st1             {v0.16b}, [\ldst], \d_strd
        b.gt            8b

        movi            v0.2d, #0
        movi            v1.2d, #0
        movi            v2.2d, #0
        movi            v3.2d, #0

        sdot            z0.d, z16.h, z7.h[0]
        sdot            z1.d, z19.h, z7.h[0]
        sdot            z2.d, z22.h, z7.h[0]
        sdot            z3.d, z25.h, z7.h[0]

        sdot            z0.d, z17.h, z7.h[1]
        sdot            z1.d, z20.h, z7.h[1]
        sdot            z2.d, z23.h, z7.h[1]
        sdot            z3.d, z26.h, z7.h[1]
        subs            \w, \w, #8

        uzp1            v0.4s, v0.4s, v1.4s
        uzp1            v1.4s, v2.4s, v3.4s
.ifc \type, prep
        srshl           v0.4s, v0.4s, v5.4s
        srshl           v1.4s, v1.4s, v5.4s
        uzp1            v0.8h, v0.8h, v1.8h
        sub             z0.h, z0.h, #PREP_BIAS
.else   // put
        sqrshrun        v0.4h, v0.4s, #6
        sqrshrun2       v0.8h, v1.4s, #6
        umin            v0.8h, v0.8h, v5.8h
.endif
        str             q0, [\ldst]

        add             \dst, \dst, #16
        add             \src, \src, #16
        b.gt            81b
        ret

        .align JUMP_ALIGN
40:     // V - 4xN, put only: 2xN
.ifc \type, put
        lsr             \d_strd, \d_strd, #1        // hword index for `st1h`
        whilelt         p1.h, wzr, \w               // masking for writes
.endif
        cmp             \h, #4
        b.le            44f

        ldr             d16, [\src]
        ldr             d17, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1
        ldr             d18, [\src]
        ldr             d19, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1

        ldr             d20, [\src]
        ldr             d21, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1
        ldr             d22, [\src]
        ldr             d23, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1
        sub             \h, \h, #2

        zip1            v0.8h, v16.8h, v17.8h
        zip1            v2.8h, v18.8h, v19.8h
        zip1            v18.8h, v20.8h, v21.8h
        zip1            v24.8h, v22.8h, v23.8h

        zip1            v16.4s, v0.4s, v2.4s
        zip2            v19.4s, v0.4s, v2.4s
        zip1            v17.4s, v18.4s, v24.4s
        zip2            v20.4s, v18.4s, v24.4s

        .align LOOP_ALIGN
4:
        ldr             d18, [\src]
        ldr             d24, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1

        movi            v0.2d, #0
        movi            v1.2d, #0
        movi            v2.2d, #0
        movi            v3.2d, #0
        mov             v21.16b, v18.16b
        mov             v27.16b, v24.16b

        sdot            z0.d, z16.h, z7.h[0]
        tbl             v22.16b, {v16.16b, v17.16b}, v6.16b
        sdot            z1.d, z19.h, z7.h[0]
        tbl             v25.16b, {v19.16b, v20.16b}, v6.16b
        sdot            z0.d, z17.h, z7.h[1]
        tbl             v23.16b, {v17.16b, v18.16b}, v28.16b
        sdot            z1.d, z20.h, z7.h[1]
        tbl             v26.16b, {v20.16b, v21.16b}, v29.16b
        subs            \h, \h, #2

        sdot            z2.d, z22.h, z7.h[0]
        tbl             v16.16b, {v22.16b, v23.16b}, v6.16b
        sdot            z3.d, z25.h, z7.h[0]
        tbl             v19.16b, {v25.16b, v26.16b}, v6.16b
        sdot            z2.d, z23.h, z7.h[1]
        tbl             v17.16b, {v23.16b, v24.16b}, v28.16b
        sdot            z3.d, z26.h, z7.h[1]
        tbl             v20.16b, {v26.16b, v27.16b}, v29.16b

        uzp1            v0.4s, v0.4s, v1.4s
        uzp1            v1.4s, v2.4s, v3.4s
.ifc \type, prep
        srshl           v0.4s, v0.4s, v5.4s
        srshl           v1.4s, v1.4s, v5.4s
        uzp1            v0.8h, v0.8h, v1.8h
        sub             z0.h, z0.h, #PREP_BIAS
        str             q0, [\dst], #16
.else   // put
        sqrshrun        v0.4h, v0.4s, #6
        sqrshrun        v1.4h, v1.4s, #6
        umin            v0.4h, v0.4h, v5.4h
        umin            v1.4h, v1.4h, v5.4h
        st1h            {z0.h}, p1, [\dst]
        st1h            {z1.h}, p1, [\dst, \d_strd, lsl #1]
        add             \dst, \dst, \d_strd, lsl #2
.endif
        b.gt            4b

        ldr             d18, [\src]

        movi            v0.2d, #0
        movi            v1.2d, #0
        movi            v2.2d, #0
        movi            v3.2d, #0
        mov             v21.16b, v18.16b

        sdot            z0.d, z16.h, z7.h[0]
        tbl             v22.16b, {v16.16b, v17.16b}, v6.16b
        sdot            z1.d, z19.h, z7.h[0]
        tbl             v25.16b, {v19.16b, v20.16b}, v6.16b
        sdot            z0.d, z17.h, z7.h[1]
        tbl             v23.16b, {v17.16b, v18.16b}, v28.16b
        sdot            z1.d, z20.h, z7.h[1]
        tbl             v26.16b, {v20.16b, v21.16b}, v29.16b

        sdot            z2.d, z22.h, z7.h[0]
        sdot            z3.d, z25.h, z7.h[0]
        sdot            z2.d, z23.h, z7.h[1]
        sdot            z3.d, z26.h, z7.h[1]

        uzp1            v0.4s, v0.4s, v1.4s
        uzp1            v1.4s, v2.4s, v3.4s
.ifc \type, prep
        srshl           v0.4s, v0.4s, v5.4s
        srshl           v1.4s, v1.4s, v5.4s
        uzp1            v0.8h, v0.8h, v1.8h
        sub             z0.h, z0.h, #PREP_BIAS
        str             q0, [\dst]
.else   // put
        sqrshrun        v0.4h, v0.4s, #6
        sqrshrun        v1.4h, v1.4s, #6
        umin            v0.4h, v0.4h, v5.4h
        umin            v1.4h, v1.4h, v5.4h
        st1h            {z0.h}, p1, [\dst]
        st1h            {z1.h}, p1, [\dst, \d_strd, lsl #1]
.endif
        ret

        .align JUMP_ALIGN
44:     // V - 4x4, put only: 4x2, 2x4, 2x2
        add             \src, \src, \s_strd, lsl #1     // src - s_strd
        subs            \h, \h, #2

        ldr             d16, [\src]
        ldr             d17, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1
        ldr             d18, [\src]
        ldr             d19, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1

        ext             v7.16b, v7.16b, v7.16b, #4      // [\xmy + 2 * 2]

        zip1            v0.8h, v16.8h, v17.8h
        zip1            v2.8h, v18.8h, v19.8h
        zip1            v16.4s, v0.4s, v2.4s
        zip2            v19.4s, v0.4s, v2.4s

.ifc \type, put
        b.eq            42f
.endif
        ldr             d17, [\src]
        ldr             d23, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1

        movi            v0.2d, #0
        movi            v1.2d, #0
        movi            v2.2d, #0
        movi            v3.2d, #0
        mov             v20.16b, v17.16b
        mov             v26.16b, v23.16b

        sdot            z0.d, z16.h, z7.h[0]
        tbl             v22.16b, {v16.16b, v17.16b}, v28.16b
        sdot            z1.d, z19.h, z7.h[0]
        tbl             v25.16b, {v19.16b, v20.16b}, v29.16b
        sdot            z2.d, z22.h, z7.h[0]
        tbl             v16.16b, {v22.16b, v23.16b}, v28.16b
        sdot            z3.d, z25.h, z7.h[0]
        tbl             v19.16b, {v25.16b, v26.16b}, v29.16b

        uzp1            v0.4s, v0.4s, v1.4s
        uzp1            v1.4s, v2.4s, v3.4s
.ifc \type, prep
        srshl           v0.4s, v0.4s, v5.4s
        srshl           v1.4s, v1.4s, v5.4s
        uzp1            v0.8h, v0.8h, v1.8h
        sub             z0.h, z0.h, #PREP_BIAS
        str             q0, [\dst], #16
.else   // put
        sqrshrun        v0.4h, v0.4s, #6
        sqrshrun        v1.4h, v1.4s, #6
        umin            v0.4h, v0.4h, v5.4h
        umin            v1.4h, v1.4h, v5.4h
        st1h            {z0.h}, p1, [\dst]
        st1h            {z1.h}, p1, [\dst, \d_strd, lsl #1]
        add             \dst, \dst, \d_strd, lsl #2
.endif

.ifc \type, put
        .align JUMP_ALIGN
42:
.endif
        ldr             d17, [\src]

        movi            v0.2d, #0
        movi            v1.2d, #0
        movi            v2.2d, #0
        movi            v3.2d, #0
        mov             v20.16b, v17.16b

        sdot            z0.d, z16.h, z7.h[0]
        tbl             v22.16b, {v16.16b, v17.16b}, v28.16b
        sdot            z1.d, z19.h, z7.h[0]
        tbl             v25.16b, {v19.16b, v20.16b}, v29.16b

        sdot            z2.d, z22.h, z7.h[0]
        sdot            z3.d, z25.h, z7.h[0]

        uzp1            v0.4s, v0.4s, v1.4s
        uzp1            v1.4s, v2.4s, v3.4s
.ifc \type, prep
        srshl           v0.4s, v0.4s, v5.4s
        srshl           v1.4s, v1.4s, v5.4s
        uzp1            v0.8h, v0.8h, v1.8h
        sub             z0.h, z0.h, #PREP_BIAS
        str             q0, [\dst]
.else   // put
        sqrshrun        v0.4h, v0.4s, #6
        sqrshrun        v1.4h, v1.4s, #6
        umin            v0.4h, v0.4h, v5.4h
        umin            v1.4h, v1.4h, v5.4h
        st1h            {z0.h}, p1, [\dst]
        st1h            {z1.h}, p1, [\dst, \d_strd, lsl #1]
.endif
        ret

        .align JUMP_ALIGN
L(\type\()_8tap_h_hv_\isa):
        madd            \mx, \mx, w11, w9
        movrel          x13, h_tbl_sve
        sub             \src, \src, #6              // src - 3 * 2
        ubfx            w9, \mx, #7, #7
        and             \mx, \mx, #0x7F
        cmp             \w, #4
        csel            \mx, \mx, w9, le
        ldp             q30, q31, [x13]
        add             \xmx, x12, \xmx, lsl #3     // subpel H filter address
        cbz             \my, L(\type\()_8tap_h_\isa)

        // HV cases
        madd            w14, \my, w11, w10
.ifc \bdmax, w8
        ldr             \bdmax, [sp]
.endif
        ubfx            w11, w14, #7, #7
        and             w14, w14, #0x7F
        ld1sb           {z4.h}, p0/z, [\xmx]
        cmp             \h, #4
        csel            w14, w14, w11, le
.ifc \type, put
        dup             v29.8h, \bdmax
.endif
        clz             \bdmax, \bdmax
        add             \xmy, x12, x14, lsl #3      // subpel V filter address
        ld1sb           {z7.h}, p0/z, [\xmy]
.ifc \type, put
        mov             w9, #12
        sub             w9, w9, \bdmax
        dup             v6.4s, w9
.endif
        sub             \bdmax, \bdmax, #24
        mov             x15, x30
        sub             \src, \src, \s_strd         // src - s_strd - 3 * 2
        dup             v5.4s, \bdmax
        cmp             w10, SHARP1
        b.ne            L(\type\()_6tap_hv_\isa)    // vertical != SHARP1

        // HV 8-tap cases
        cmp             \w, #4
        b.le            40f

        // .align JUMP_ALIGN    // fallthrough
80:     // HV8 - 8xN+
.ifc \type, prep
        add             \wd_strd, \w, \w                // d_strd = 2 * w
.endif
        cmp             \h, #4
        b.le            84f
        sub             \src, \src, \s_strd, lsl #1     // src - 3 * s_strd - 3 * 2

        .align LOOP_ALIGN
81:
        mov             \lsrc, \src
        mov             \ldst, \dst
        mov             w8, \h

        bl              L(\type\()_hv_filter8_\isa)
        uzp1            v16.8h, v23.8h, v24.8h
        bl              L(\type\()_hv_filter8_\isa)
        uzp1            v17.8h, v23.8h, v24.8h
        bl              L(\type\()_hv_filter8_\isa)
        uzp1            v18.8h, v23.8h, v24.8h
        bl              L(\type\()_hv_filter8_\isa)
        uzp1            v19.8h, v23.8h, v24.8h
        bl              L(\type\()_hv_filter8_\isa)
        uzp1            v20.8h, v23.8h, v24.8h
        bl              L(\type\()_hv_filter8_\isa)
        uzp1            v21.8h, v23.8h, v24.8h
        bl              L(\type\()_hv_filter8_\isa)
        uzp1            v22.8h, v23.8h, v24.8h

        .align LOOP_ALIGN
8:
        ldp             q24, q28, [\lsrc]
        smull           v0.4s, v16.4h, v7.h[0]
        smull2          v1.4s, v16.8h, v7.h[0]
        mov             v16.16b, v17.16b

        movi            v2.2d, #0
        movi            v3.2d, #0
        tbl             v23.16b, {v24.16b}, v30.16b
        tbl             v24.16b, {v24.16b}, v31.16b

        ldur            q26, [\lsrc, #8]
        smlal           v0.4s, v17.4h, v7.h[1]
        smlal2          v1.4s, v17.8h, v7.h[1]
        mov             v17.16b, v18.16b
        add             \lsrc, \lsrc, \s_strd

        sdot            z2.d, z23.h, z4.h[0]
        sdot            z3.d, z24.h, z4.h[0]
        movi            v23.2d, #0
        movi            v24.2d, #0
        tbl             v25.16b, {v26.16b}, v30.16b
        tbl             v26.16b, {v26.16b}, v31.16b
        smlal           v0.4s, v18.4h, v7.h[2]
        smlal2          v1.4s, v18.8h, v7.h[2]
        mov             v18.16b, v19.16b

        sdot            z23.d, z25.h, z4.h[0]
        sdot            z24.d, z26.h, z4.h[0]
        tbl             v27.16b, {v28.16b}, v30.16b
        tbl             v28.16b, {v28.16b}, v31.16b
        smlal           v0.4s, v19.4h, v7.h[3]
        smlal2          v1.4s, v19.8h, v7.h[3]
        mov             v19.16b, v20.16b

        subs            w8, w8, #1
        sdot            z2.d, z25.h, z4.h[1]
        sdot            z3.d, z26.h, z4.h[1]
        sdot            z23.d, z27.h, z4.h[1]
        sdot            z24.d, z28.h, z4.h[1]

        smlal           v0.4s, v20.4h, v7.h[4]
        smlal2          v1.4s, v20.8h, v7.h[4]
        mov             v20.16b, v21.16b

        uzp1            v3.4s, v2.4s, v3.4s
        uzp1            v24.4s, v23.4s, v24.4s
        smlal           v0.4s, v21.4h, v7.h[5]
        smlal2          v1.4s, v21.8h, v7.h[5]
        mov             v21.16b, v22.16b

        srshl           v23.4s, v3.4s, v5.4s
        srshl           v24.4s, v24.4s, v5.4s
        smlal           v0.4s, v22.4h, v7.h[6]
        smlal2          v1.4s, v22.8h, v7.h[6]

        uzp1            v22.8h, v23.8h, v24.8h
        smlal           v0.4s, v22.4h, v7.h[7]
        smlal2          v1.4s, v22.8h, v7.h[7]

.ifc \type, prep
        rshrn           v0.4h, v0.4s, #6
        rshrn2          v0.8h, v1.4s, #6
        sub             z0.h, z0.h, #PREP_BIAS
.else   // put
        srshl           v0.4s, v0.4s, v6.4s
        srshl           v1.4s, v1.4s, v6.4s
        sqxtun          v0.4h, v0.4s
        sqxtun2         v0.8h, v1.4s
        umin            v0.8h, v0.8h, v29.8h
.endif
        st1             {v0.8h}, [\ldst], \d_strd
        b.gt            8b

        subs            \w, \w, #8
        add             \src, \src, #16
        add             \dst, \dst, #16
        b.gt            81b
        ret             x15

        .align JUMP_ALIGN
40:     // HV8 - 4xN, put only: 2xN
.ifc \type, put
        lsr             \d_strd, \d_strd, #1        // hword index for `st1h`
        whilelt         p1.h, wzr, \w               // masking for writes
.endif
        ext             v4.16b, v4.16b, v4.16b, #4  // [\xmy + 2 * 2]
        add             \src, \src, #4

        cmp             \h, #4
        b.le            44f

        sub             \src, \src, \s_strd, lsl #1 // src - 3 * s_strd - 3 * 2
        bl              L(\type\()_hv_filter4_\isa)
        xtn             v16.4h, v0.4s
        bl              L(\type\()_hv_filter4_\isa)
        xtn             v17.4h, v0.4s
        bl              L(\type\()_hv_filter4_\isa)
        xtn             v18.4h, v0.4s
        bl              L(\type\()_hv_filter4_\isa)
        xtn             v19.4h, v0.4s
        bl              L(\type\()_hv_filter4_\isa)
        xtn             v20.4h, v0.4s
        bl              L(\type\()_hv_filter4_\isa)
        xtn             v21.4h, v0.4s
        bl              L(\type\()_hv_filter4_\isa)
        xtn             v22.4h, v0.4s

        .align LOOP_ALIGN
4:
        ld1             {v3.16b}, [\src], \s_strd

        smull           v24.4s, v16.4h, v7.h[0]
        smlal           v24.4s, v17.4h, v7.h[1]
        tbl             v2.16b, {v3.16b}, v30.16b
        tbl             v3.16b, {v3.16b}, v31.16b
        movi            v0.2d, #0
        movi            v1.2d, #0
        mov             v16.16b, v17.16b
        mov             v17.16b, v18.16b

        smlal           v24.4s, v18.4h, v7.h[2]
        smlal           v24.4s, v19.4h, v7.h[3]
        sdot            z0.d, z2.h, z4.h[0]
        sdot            z1.d, z3.h, z4.h[0]
        mov             v18.16b, v19.16b
        mov             v19.16b, v20.16b
        uzp1            v0.4s, v0.4s, v1.4s

        smlal           v24.4s, v20.4h, v7.h[4]
        smlal           v24.4s, v21.4h, v7.h[5]
        srshl           v0.4s, v0.4s, v5.4s
        mov             v20.16b, v21.16b
        mov             v21.16b, v22.16b

        subs            \h, \h, #1
        smlal           v24.4s, v22.4h, v7.h[6]
        xtn             v22.4h, v0.4s
        smlal           v24.4s, v22.4h, v7.h[7]

.ifc \type, prep
        rshrn           v0.4h, v24.4s, #6
        sub             z0.h, z0.h, #PREP_BIAS
        str             d0, [\dst], #8
.else   // put
        srshl           v0.4s, v24.4s, v6.4s
        sqxtun          v0.4h, v0.4s
        umin            v0.4h, v0.4h, v29.4h
        st1h            {z0.h}, p1, [\dst]
        add             \dst, \dst, \d_strd, lsl #1
.endif
        b.gt            4b
        ret             x15

        .align JUMP_ALIGN
L(\type\()_6tap_hv_\isa):
        cmp             \w, #4
        b.le            46f

        // .align JUMP_ALIGN    // fallthrough
80:     // HV6 - 8xN+
.ifc \type, prep
        add             \wd_strd, \w, \w        // d_strd = 2 * w
.endif
        cmp             \h, #4
        b.le            84f
        sub             \src, \src, \s_strd     // src - 2 * s_strd - 3 * 2

        .align LOOP_ALIGN
81:
        mov             \lsrc, \src
        mov             \ldst, \dst
        mov             w8, \h

        bl              L(\type\()_hv_filter8_\isa)
        uzp1            v16.8h, v23.8h, v24.8h
        bl              L(\type\()_hv_filter8_\isa)
        uzp1            v17.8h, v23.8h, v24.8h
        bl              L(\type\()_hv_filter8_\isa)
        uzp1            v18.8h, v23.8h, v24.8h
        bl              L(\type\()_hv_filter8_\isa)
        uzp1            v19.8h, v23.8h, v24.8h
        bl              L(\type\()_hv_filter8_\isa)
        uzp1            v20.8h, v23.8h, v24.8h

        .align LOOP_ALIGN
8:
        ldp             q24, q28, [\lsrc]

        smull           v0.4s, v16.4h, v7.h[1]
        smull2          v1.4s, v16.8h, v7.h[1]
        mov             v16.16b, v17.16b

        tbl             v23.16b, {v24.16b}, v30.16b
        tbl             v24.16b, {v24.16b}, v31.16b
        movi            v2.2d, #0
        movi            v3.2d, #0

        ldur            q26, [\lsrc, #8]
        add             \lsrc, \lsrc, \s_strd

        sdot            z2.d, z23.h, z4.h[0]
        sdot            z3.d, z24.h, z4.h[0]
        tbl             v25.16b, {v26.16b}, v30.16b
        tbl             v26.16b, {v26.16b}, v31.16b
        movi            v23.2d, #0
        movi            v24.2d, #0

        sdot            z23.d, z25.h, z4.h[0]
        sdot            z24.d, z26.h, z4.h[0]
        tbl             v27.16b, {v28.16b}, v30.16b
        tbl             v28.16b, {v28.16b}, v31.16b
        smlal           v0.4s, v17.4h, v7.h[2]
        smlal2          v1.4s, v17.8h, v7.h[2]
        mov             v17.16b, v18.16b

        sdot            z2.d, z25.h, z4.h[1]
        sdot            z3.d, z26.h, z4.h[1]
        sdot            z23.d, z27.h, z4.h[1]
        sdot            z24.d, z28.h, z4.h[1]

        smlal           v0.4s, v18.4h, v7.h[3]
        smlal2          v1.4s, v18.8h, v7.h[3]
        mov             v18.16b, v19.16b

        uzp1            v3.4s, v2.4s, v3.4s
        uzp1            v24.4s, v23.4s, v24.4s
        smlal           v0.4s, v19.4h, v7.h[4]
        smlal2          v1.4s, v19.8h, v7.h[4]
        mov             v19.16b, v20.16b

        srshl           v23.4s, v3.4s, v5.4s
        srshl           v24.4s, v24.4s, v5.4s
        smlal           v0.4s, v20.4h, v7.h[5]
        smlal2          v1.4s, v20.8h, v7.h[5]

        subs            w8, w8, #1
        uzp1            v20.8h, v23.8h, v24.8h
        smlal           v0.4s, v20.4h, v7.h[6]
        smlal2          v1.4s, v20.8h, v7.h[6]

.ifc \type, prep
        rshrn           v0.4h, v0.4s, #6
        rshrn2          v0.8h, v1.4s, #6
        sub             z0.h, z0.h, #PREP_BIAS
.else   // put
        srshl           v0.4s, v0.4s, v6.4s
        srshl           v1.4s, v1.4s, v6.4s
        sqxtun          v0.4h, v0.4s
        sqxtun2         v0.8h, v1.4s
        umin            v0.8h, v0.8h, v29.8h
.endif
        st1             {v0.8h}, [\ldst], \d_strd
        b.gt            8b

        add             \dst, \dst, #16
        subs            \w, \w, #8
        add             \src, \src, #16
        b.gt            81b
        ret             x15

        .align LOOP_ALIGN
84:     // HV4 - 8x4, 8x2
        mov             \lsrc, \src
        mov             \ldst, \dst
        mov             w8, \h

        bl              L(\type\()_hv_filter8_\isa)
        uzp1            v17.8h, v23.8h, v24.8h
        bl              L(\type\()_hv_filter8_\isa)
        uzp1            v18.8h, v23.8h, v24.8h
        bl              L(\type\()_hv_filter8_\isa)
        uzp1            v19.8h, v23.8h, v24.8h

        .align LOOP_ALIGN
81:
        ldp             q24, q28, [\lsrc]
        ldur            q26, [\lsrc, #8]
        add             \lsrc, \lsrc, \s_strd

        tbl             v23.16b, {v24.16b}, v30.16b
        tbl             v24.16b, {v24.16b}, v31.16b
        movi            v2.2d, #0
        movi            v3.2d, #0
        sdot            z2.d, z23.h, z4.h[0]
        sdot            z3.d, z24.h, z4.h[0]

        tbl             v25.16b, {v26.16b}, v30.16b
        tbl             v26.16b, {v26.16b}, v31.16b
        movi            v23.2d, #0
        movi            v24.2d, #0
        sdot            z23.d, z25.h, z4.h[0]
        sdot            z24.d, z26.h, z4.h[0]

        tbl             v27.16b, {v28.16b}, v30.16b
        tbl             v28.16b, {v28.16b}, v31.16b
        sdot            z2.d, z25.h, z4.h[1]
        sdot            z3.d, z26.h, z4.h[1]
        sdot            z23.d, z27.h, z4.h[1]
        sdot            z24.d, z28.h, z4.h[1]

        smull           v0.4s, v17.4h, v7.h[2]
        smull2          v1.4s, v17.8h, v7.h[2]
        mov             v17.16b, v18.16b

        subs            w8, w8, #1
        uzp1            v3.4s, v2.4s, v3.4s
        uzp1            v24.4s, v23.4s, v24.4s
        smlal           v0.4s, v18.4h, v7.h[3]
        smlal2          v1.4s, v18.8h, v7.h[3]
        mov             v18.16b, v19.16b

        srshl           v23.4s, v3.4s, v5.4s
        srshl           v24.4s, v24.4s, v5.4s
        smlal           v0.4s, v19.4h, v7.h[4]
        smlal2          v1.4s, v19.8h, v7.h[4]

        uzp1            v19.8h, v23.8h, v24.8h
        smlal           v0.4s, v19.4h, v7.h[5]
        smlal2          v1.4s, v19.8h, v7.h[5]

.ifc \type, prep
        rshrn           v0.4h, v0.4s, #6
        rshrn2          v0.8h, v1.4s, #6
        sub             z0.h, z0.h, #PREP_BIAS
.else   // put
        srshl           v0.4s, v0.4s, v6.4s
        srshl           v1.4s, v1.4s, v6.4s
        sqxtun          v0.4h, v0.4s
        sqxtun2         v0.8h, v1.4s
        umin            v0.8h, v0.8h, v29.8h
.endif
        st1             {v0.8h}, [\ldst], \d_strd
        b.gt            81b

        subs            \w, \w, #8
        add             \dst, \dst, #16
        add             \src, \src, #16
        b.gt            84b
        ret             x15

        .align FUNC_ALIGN
L(\type\()_hv_filter8_\isa):
        ldp             q24, q28, [\lsrc]
        ldur            q26, [\lsrc, #8]
        add             \lsrc, \lsrc, \s_strd

        tbl             v23.16b, {v24.16b}, v30.16b
        tbl             v24.16b, {v24.16b}, v31.16b
        movi            v2.2d, #0
        movi            v3.2d, #0
        sdot            z2.d, z23.h, z4.h[0]
        sdot            z3.d, z24.h, z4.h[0]

        tbl             v25.16b, {v26.16b}, v30.16b
        tbl             v26.16b, {v26.16b}, v31.16b
        movi            v23.2d, #0
        movi            v24.2d, #0
        sdot            z23.d, z25.h, z4.h[0]
        sdot            z24.d, z26.h, z4.h[0]

        tbl             v27.16b, {v28.16b}, v30.16b
        tbl             v28.16b, {v28.16b}, v31.16b
        sdot            z2.d, z25.h, z4.h[1]
        sdot            z3.d, z26.h, z4.h[1]
        sdot            z23.d, z27.h, z4.h[1]
        sdot            z24.d, z28.h, z4.h[1]

        uzp1            v3.4s, v2.4s, v3.4s
        uzp1            v24.4s, v23.4s, v24.4s
        srshl           v23.4s, v3.4s, v5.4s
        srshl           v24.4s, v24.4s, v5.4s
        ret

        .align FUNC_ALIGN
L(\type\()_hv_filter4_\isa):
        ld1             {v3.16b}, [\src], \s_strd

        tbl             v2.16b, {v3.16b}, v30.16b
        tbl             v3.16b, {v3.16b}, v31.16b
        movi            v0.2d, #0
        movi            v1.2d, #0
        sdot            z0.d, z2.h, z4.h[0]
        sdot            z1.d, z3.h, z4.h[0]

        uzp1            v0.4s, v0.4s, v1.4s
        srshl           v0.4s, v0.4s, v5.4s
        ret

        .align JUMP_ALIGN
46:     // H4V6 - 4xN, put only: 2xN
.ifc \type, put
        lsr             \d_strd, \d_strd, #1        // hword index for `st1h`
        whilelt         p1.h, wzr, \w               // masking for writes
.endif
        ext             v4.16b, v4.16b, v4.16b, #4  // [\xmy + 2 * 2]
        add             \src, \src, #4

        cmp             \h, #4
        b.le            44f

        sub             \src, \src, \s_strd         // src - 2 * s_strd - 3 * 2
        bl              L(\type\()_hv_filter4_\isa)
        xtn             v16.4h, v0.4s
        bl              L(\type\()_hv_filter4_\isa)
        xtn             v17.4h, v0.4s
        bl              L(\type\()_hv_filter4_\isa)
        xtn             v18.4h, v0.4s
        bl              L(\type\()_hv_filter4_\isa)
        xtn             v19.4h, v0.4s
        bl              L(\type\()_hv_filter4_\isa)
        xtn             v20.4h, v0.4s

        .align LOOP_ALIGN
4:
        ld1             {v3.16b}, [\src], \s_strd
        smull           v24.4s, v16.4h, v7.h[1]
        smlal           v24.4s, v17.4h, v7.h[2]

        tbl             v2.16b, {v3.16b}, v30.16b
        tbl             v3.16b, {v3.16b}, v31.16b
        movi            v0.2d, #0
        movi            v1.2d, #0
        sdot            z0.d, z2.h, z4.h[0]
        sdot            z1.d, z3.h, z4.h[0]

        mov             v16.16b, v17.16b
        mov             v17.16b, v18.16b
        smlal           v24.4s, v18.4h, v7.h[3]
        smlal           v24.4s, v19.4h, v7.h[4]
        uzp1            v0.4s, v0.4s, v1.4s

        mov             v18.16b, v19.16b
        mov             v19.16b, v20.16b
        subs            \h, \h, #1
        srshl           v0.4s, v0.4s, v5.4s
        smlal           v24.4s, v20.4h, v7.h[5]
        xtn             v20.4h, v0.4s
        smlal           v24.4s, v20.4h, v7.h[6]

.ifc \type, prep
        rshrn           v0.4h, v24.4s, #6
        sub             z0.h, z0.h, #PREP_BIAS
        str             d0, [\dst], #8
.else   // put
        srshl           v0.4s, v24.4s, v6.4s
        sqxtun          v0.4h, v0.4s
        umin            v0.4h, v0.4h, v29.4h
        st1h            {z0.h}, p1, [\dst]
        add             \dst, \dst, \d_strd, lsl #1
.endif
        b.gt            4b
        ret             x15

        .align JUMP_ALIGN
44:     // H4V4 - 4x4, put only: 4x2, 2x4, 2x2
        bl              L(\type\()_hv_filter4_\isa)
        xtn             v17.4h, v0.4s
        bl              L(\type\()_hv_filter4_\isa)
        xtn             v18.4h, v0.4s
        bl              L(\type\()_hv_filter4_\isa)
        xtn             v19.4h, v0.4s

        .align LOOP_ALIGN
4:
        ld1             {v3.16b}, [\src], \s_strd
        smull           v24.4s, v17.4h, v7.h[2]
        smlal           v24.4s, v18.4h, v7.h[3]

        tbl             v2.16b, {v3.16b}, v30.16b
        tbl             v3.16b, {v3.16b}, v31.16b
        movi            v0.2d, #0
        movi            v1.2d, #0
        sdot            z0.d, z2.h, z4.h[0]
        sdot            z1.d, z3.h, z4.h[0]
        uzp1            v0.4s, v0.4s, v1.4s

        mov             v17.16b, v18.16b
        mov             v18.16b, v19.16b
        subs            \h, \h, #1
        srshl           v0.4s, v0.4s, v5.4s
        smlal           v24.4s, v19.4h, v7.h[4]
        xtn             v19.4h, v0.4s
        smlal           v24.4s, v19.4h, v7.h[5]

.ifc \type, prep
        rshrn           v0.4h, v24.4s, #6
        sub             z0.h, z0.h, #PREP_BIAS
        str             d0, [\dst], #8
.else   // put
        srshl           v0.4s, v24.4s, v6.4s
        sqxtun          v0.4h, v0.4s
        umin            v0.4h, v0.4h, v29.4h
        st1h            {z0.h}, p1, [\dst]
        add             \dst, \dst, \d_strd, lsl #1
.endif
        b.gt            4b
        ret             x15

        .align JUMP_ALIGN
L(\type\()_8tap_h_\isa):
        movrel          x11, \type\()_8tap_h_\isa\()_tbl
        ldrsw           x12, [x11, x8, lsl #2]
.ifc \bdmax, w8
        ldr             \bdmax, [sp]
.endif
.ifc \type, prep
        clz             \bdmax, \bdmax
        sub             \bdmax, \bdmax, #24
        dup             v5.4s, \bdmax
.else   // put
        mov             w9, #34             // rounding for 10-bit case
        mov             w10, #40            // rounding for 12-bit case
        cmp             \bdmax, #0xFFF
        csel            w9, w9, w10, ne     // select rounding based on \bdmax
        dup             v5.8h, \bdmax
        dup             v6.2d, x9
.endif
        add             x11, x11, x12
        ld1sb           {z4.h}, p0/z, [\xmx]
        br              x11

        .align JUMP_ALIGN
20:     // H - 4xN, put only: 2xN
40:
        AARCH64_VALID_JUMP_TARGET
        add             \src, \src, #4              // src - 1 * 2
        ext             v4.16b, v4.16b, v4.16b, #4  // [\xmy + 2 * 2]
.ifc \type, put
        lsr             \d_strd, \d_strd, #1        // hword index for `st1h`
        whilelt         p1.h, wzr, \w               // masking for writes
.endif
        .align LOOP_ALIGN
4:
        ldr             q17, [\src]
        ldr             q19, [\src, \s_strd]
        add             \src, \src, \s_strd, lsl #1

.ifc \type, prep
        movi            v0.2d, #0
        movi            v1.2d, #0
        movi            v2.2d, #0
        movi            v3.2d, #0
.else
        mov             v0.16b, v6.16b
        mov             v1.16b, v6.16b
        mov             v2.16b, v6.16b
        mov             v3.16b, v6.16b
.endif
        tbl             v16.16b, {v17.16b}, v30.16b
        tbl             v17.16b, {v17.16b}, v31.16b
        sdot            z0.d, z16.h, z4.h[0]
        sdot            z1.d, z17.h, z4.h[0]
        subs            \h, \h, #2
        tbl             v18.16b, {v19.16b}, v30.16b
        tbl             v19.16b, {v19.16b}, v31.16b
        sdot            z2.d, z18.h, z4.h[0]
        sdot            z3.d, z19.h, z4.h[0]

        uzp1            v0.4s, v0.4s, v1.4s
        uzp1            v1.4s, v2.4s, v3.4s
.ifc \type, prep
        srshl           v0.4s, v0.4s, v5.4s
        srshl           v1.4s, v1.4s, v5.4s
        uzp1            v0.8h, v0.8h, v1.8h
        sub             z0.h, z0.h, #PREP_BIAS
        str             q0, [\dst], #16
.else   // put
        sqshrun         v0.4h, v0.4s, #6
        sqshrun         v1.4h, v1.4s, #6
        umin            v0.4h, v0.4h, v5.4h
        umin            v1.4h, v1.4h, v5.4h
        st1h            {z0.h}, p1, [\dst]
        st1h            {z1.h}, p1, [\dst, \d_strd, lsl #1]
        add             \dst, \dst, \d_strd, lsl #2
.endif
        b.gt            4b
        ret

        .align JUMP_ALIGN
80:     // H - 8xN
        AARCH64_VALID_JUMP_TARGET

        .align LOOP_ALIGN
8:
        ldp             q17, q21, [\src]
        ldur            q19, [\src, #8]

.ifc \type, prep
        movi            v0.2d, #0
        movi            v2.2d, #0
.else
        mov             v0.16b, v6.16b
        mov             v2.16b, v6.16b
.endif
        tbl             v16.16b, {v17.16b}, v30.16b
        tbl             v17.16b, {v17.16b}, v31.16b
        add             \src, \src, \s_strd
        sdot            z0.d, z16.h, z4.h[0]
        sdot            z2.d, z17.h, z4.h[0]

        tbl             v18.16b, {v19.16b}, v30.16b
        tbl             v19.16b, {v19.16b}, v31.16b
.ifc \type, prep
        movi            v16.2d, #0
        movi            v17.2d, #0
.else
        mov             v16.16b, v6.16b
        mov             v17.16b, v6.16b
.endif
        ldp             q23, q27, [\src]
        ldur            q25, [\src, #8]

        sdot            z16.d, z18.h, z4.h[0]
        sdot            z17.d, z19.h, z4.h[0]

        tbl             v22.16b, {v23.16b}, v30.16b
        tbl             v23.16b, {v23.16b}, v31.16b
.ifc \type, prep
        movi            v1.2d, #0
        movi            v3.2d, #0
.else
        mov             v1.16b, v6.16b
        mov             v3.16b, v6.16b
.endif
        add             \src, \src, \s_strd
        sdot            z1.d, z22.h, z4.h[0]
        sdot            z3.d, z23.h, z4.h[0]

        tbl             v24.16b, {v25.16b}, v30.16b
        tbl             v25.16b, {v25.16b}, v31.16b
.ifc \type, prep
        movi            v22.2d, #0
        movi            v23.2d, #0
.else
        mov             v22.16b, v6.16b
        mov             v23.16b, v6.16b
.endif
        sdot            z22.d, z24.h, z4.h[0]
        sdot            z23.d, z25.h, z4.h[0]

        tbl             v20.16b, {v21.16b}, v30.16b
        tbl             v21.16b, {v21.16b}, v31.16b
        sdot            z0.d, z18.h, z4.h[1]
        sdot            z2.d, z19.h, z4.h[1]
        tbl             v26.16b, {v27.16b}, v30.16b
        tbl             v27.16b, {v27.16b}, v31.16b
        sdot            z16.d, z20.h, z4.h[1]
        sdot            z17.d, z21.h, z4.h[1]

        sdot            z1.d, z24.h, z4.h[1]
        sdot            z3.d, z25.h, z4.h[1]

        sdot            z22.d, z26.h, z4.h[1]
        sdot            z23.d, z27.h, z4.h[1]

        subs            \h, \h, #2
        uzp1            v0.4s, v0.4s, v2.4s
        uzp1            v2.4s, v16.4s, v17.4s
        uzp1            v1.4s, v1.4s, v3.4s
        uzp1            v3.4s, v22.4s, v23.4s
.ifc \type, prep
        srshl           v0.4s, v0.4s, v5.4s
        srshl           v2.4s, v2.4s, v5.4s
        srshl           v1.4s, v1.4s, v5.4s
        srshl           v3.4s, v3.4s, v5.4s
        uzp1            v0.8h, v0.8h, v2.8h
        uzp1            v1.8h, v1.8h, v3.8h
        sub             z0.h, z0.h, #PREP_BIAS
        sub             z1.h, z1.h, #PREP_BIAS
        stp             q0, q1, [\dst], #32
.else   // put
        sqshrun         v0.4h, v0.4s, #6
        sqshrun2        v0.8h, v2.4s, #6
        sqshrun         v1.4h, v1.4s, #6
        sqshrun2        v1.8h, v3.4s, #6
        umin            v0.8h, v0.8h, v5.8h
        umin            v1.8h, v1.8h, v5.8h
        st1             {v0.16b}, [\dst], \d_strd
        st1             {v1.16b}, [\dst], \d_strd
.endif
        b.gt            8b
        ret

        .align JUMP_ALIGN
160:    // H - 16xN
        AARCH64_VALID_JUMP_TARGET

        .align LOOP_ALIGN
16:
        ldp             q17, q21, [\src]
        ldur            q19, [\src, #8]

.ifc \type, prep
        movi            v0.2d, #0
        movi            v2.2d, #0
.else
        mov             v0.16b, v6.16b
        mov             v2.16b, v6.16b
.endif
        tbl             v16.16b, {v17.16b}, v30.16b
        tbl             v17.16b, {v17.16b}, v31.16b
        sdot            z0.d, z16.h, z4.h[0]
        sdot            z2.d, z17.h, z4.h[0]

        tbl             v18.16b, {v19.16b}, v30.16b
        tbl             v19.16b, {v19.16b}, v31.16b
.ifc \type, prep
        movi            v16.2d, #0
        movi            v17.2d, #0
.else
        mov             v16.16b, v6.16b
        mov             v17.16b, v6.16b
.endif
        ldur            q25, [\src, #24]
        ldr             q27, [\src, #32]

        sdot            z16.d, z18.h, z4.h[0]
        sdot            z17.d, z19.h, z4.h[0]

        tbl             v22.16b, {v21.16b}, v30.16b
        tbl             v23.16b, {v21.16b}, v31.16b
.ifc \type, prep
        movi            v1.2d, #0
        movi            v3.2d, #0
.else
        mov             v1.16b, v6.16b
        mov             v3.16b, v6.16b
.endif
        add             \src, \src, \s_strd
        sdot            z1.d, z22.h, z4.h[0]
        sdot            z3.d, z23.h, z4.h[0]

        tbl             v24.16b, {v25.16b}, v30.16b
        tbl             v25.16b, {v25.16b}, v31.16b
.ifc \type, prep
        movi            v22.2d, #0
        movi            v23.2d, #0
.else
        mov             v22.16b, v6.16b
        mov             v23.16b, v6.16b
.endif
        sdot            z22.d, z24.h, z4.h[0]
        sdot            z23.d, z25.h, z4.h[0]

        tbl             v20.16b, {v21.16b}, v30.16b
        tbl             v21.16b, {v21.16b}, v31.16b
        sdot            z0.d, z18.h, z4.h[1]
        sdot            z2.d, z19.h, z4.h[1]
        tbl             v26.16b, {v27.16b}, v30.16b
        tbl             v27.16b, {v27.16b}, v31.16b
        sdot            z16.d, z20.h, z4.h[1]
        sdot            z17.d, z21.h, z4.h[1]

        sdot            z1.d, z24.h, z4.h[1]
        sdot            z3.d, z25.h, z4.h[1]

        sdot            z22.d, z26.h, z4.h[1]
        sdot            z23.d, z27.h, z4.h[1]

        subs            \h, \h, #1
        uzp1            v0.4s, v0.4s, v2.4s
        uzp1            v2.4s, v16.4s, v17.4s
        uzp1            v1.4s, v1.4s, v3.4s
        uzp1            v3.4s, v22.4s, v23.4s
.ifc \type, prep
        srshl           v0.4s, v0.4s, v5.4s
        srshl           v2.4s, v2.4s, v5.4s
        srshl           v1.4s, v1.4s, v5.4s
        srshl           v3.4s, v3.4s, v5.4s
        uzp1            v0.8h, v0.8h, v2.8h
        uzp1            v1.8h, v1.8h, v3.8h
        sub             z0.h, z0.h, #PREP_BIAS
        sub             z1.h, z1.h, #PREP_BIAS
        stp             q0, q1, [\dst], #32
.else   // put
        sqshrun         v0.4h, v0.4s, #6
        sqshrun2        v0.8h, v2.4s, #6
        sqshrun         v1.4h, v1.4s, #6
        sqshrun2        v1.8h, v3.4s, #6
        umin            v0.8h, v0.8h, v5.8h
        umin            v1.8h, v1.8h, v5.8h
        st1             {v0.16b, v1.16b}, [\dst], \d_strd
.endif
        b.gt            16b
        ret

        .align JUMP_ALIGN
320:    // H - 32xN+
640:
1280:
        AARCH64_VALID_JUMP_TARGET
.ifc \type, put
        sub             \d_strd, \d_strd, \w, uxtw #1
.endif
        sub             \s_strd, \s_strd, \w, uxtw #1
        mov             w8, \w

        .align LOOP_ALIGN
32:
        ldp             q17, q21, [\src]
        ldur            q19, [\src, #8]

.ifc \type, prep
        movi            v0.2d, #0
        movi            v2.2d, #0
.else
        mov             v0.16b, v6.16b
        mov             v2.16b, v6.16b
.endif
        tbl             v16.16b, {v17.16b}, v30.16b
        tbl             v17.16b, {v17.16b}, v31.16b
        sdot            z0.d, z16.h, z4.h[0]
        sdot            z2.d, z17.h, z4.h[0]

        tbl             v18.16b, {v19.16b}, v30.16b
        tbl             v19.16b, {v19.16b}, v31.16b
.ifc \type, prep
        movi            v16.2d, #0
        movi            v17.2d, #0
.else
        mov             v16.16b, v6.16b
        mov             v17.16b, v6.16b
.endif
        ldur            q25, [\src, #24]

        sdot            z16.d, z18.h, z4.h[0]
        sdot            z17.d, z19.h, z4.h[0]

        ldr             q27, [\src, #32]!

        tbl             v22.16b, {v21.16b}, v30.16b
        tbl             v23.16b, {v21.16b}, v31.16b
.ifc \type, prep
        movi            v1.2d, #0
        movi            v3.2d, #0
.else
        mov             v1.16b, v6.16b
        mov             v3.16b, v6.16b
.endif
        sdot            z1.d, z22.h, z4.h[0]
        sdot            z3.d, z23.h, z4.h[0]

        tbl             v24.16b, {v25.16b}, v30.16b
        tbl             v25.16b, {v25.16b}, v31.16b
.ifc \type, prep
        movi            v22.2d, #0
        movi            v23.2d, #0
.else
        mov             v22.16b, v6.16b
        mov             v23.16b, v6.16b
.endif
        sdot            z22.d, z24.h, z4.h[0]
        sdot            z23.d, z25.h, z4.h[0]

        tbl             v20.16b, {v21.16b}, v30.16b
        tbl             v21.16b, {v21.16b}, v31.16b
        sdot            z0.d, z18.h, z4.h[1]
        sdot            z2.d, z19.h, z4.h[1]
        tbl             v26.16b, {v27.16b}, v30.16b
        tbl             v27.16b, {v27.16b}, v31.16b
        sdot            z16.d, z20.h, z4.h[1]
        sdot            z17.d, z21.h, z4.h[1]

        sdot            z1.d, z24.h, z4.h[1]
        sdot            z3.d, z25.h, z4.h[1]

        sdot            z22.d, z26.h, z4.h[1]
        sdot            z23.d, z27.h, z4.h[1]

        subs            w8, w8, #16
        uzp1            v0.4s, v0.4s, v2.4s
        uzp1            v2.4s, v16.4s, v17.4s
        uzp1            v1.4s, v1.4s, v3.4s
        uzp1            v3.4s, v22.4s, v23.4s
.ifc \type, prep
        srshl           v0.4s, v0.4s, v5.4s
        srshl           v2.4s, v2.4s, v5.4s
        srshl           v1.4s, v1.4s, v5.4s
        srshl           v3.4s, v3.4s, v5.4s
        uzp1            v0.8h, v0.8h, v2.8h
        uzp1            v1.8h, v1.8h, v3.8h
        sub             z0.h, z0.h, #PREP_BIAS
        sub             z1.h, z1.h, #PREP_BIAS
.else   // put
        sqshrun         v0.4h, v0.4s, #6
        sqshrun2        v0.8h, v2.4s, #6
        sqshrun         v1.4h, v1.4s, #6
        sqshrun2        v1.8h, v3.4s, #6
        umin            v0.8h, v0.8h, v5.8h
        umin            v1.8h, v1.8h, v5.8h
.endif
        stp             q0, q1, [\dst], #32
        b.gt            32b

        add             \src, \src, \s_strd
.ifc \type, put
        add             \dst, \dst, \d_strd
.endif
        subs            \h, \h, #1
        mov             w8, \w
        b.gt            32b
        ret
endfunc

jumptable \type\()_8tap_h_\isa\()_tbl
        .word 1280b - \type\()_8tap_h_\isa\()_tbl
        .word 640b  - \type\()_8tap_h_\isa\()_tbl
        .word 320b  - \type\()_8tap_h_\isa\()_tbl
        .word 160b  - \type\()_8tap_h_\isa\()_tbl
        .word 80b   - \type\()_8tap_h_\isa\()_tbl
        .word 40b   - \type\()_8tap_h_\isa\()_tbl
.ifc \type, put
        .word 20b   - \type\()_8tap_h_\isa\()_tbl
.endif
endjumptable
.endm


function prep_sve
        movrel          x9, prep_tbl
        mov             w6, #19
        ldrsw           x8, [x9, x8, lsl #2]
        sub             w6, w6, w7, lsr #8          // 19 - bdmax / 256
        add             x9, x9, x8
        movi            v30.8h, #PREP_BIAS_NEG
        dup             v29.8h, w6                  // 10b: 1 << 4, 12b: 1 << 2
        br              x9

        .align JUMP_ALIGN
40:     // prep - 4xN
        AARCH64_VALID_JUMP_TARGET

        .align LOOP_ALIGN
4:
        ldr             d0, [x1]
        ldr             d1, [x1, x2]
        add             x1, x1, x2, lsl #1
        subs            w4, w4, #2
        mad             z0.h, p0/m, z29.h, z30.h
        mad             z1.h, p0/m, z29.h, z30.h
        stp             d0, d1, [x0], #16
        b.gt            4b
        ret

        .align JUMP_ALIGN
80:     // prep - 8xN
        AARCH64_VALID_JUMP_TARGET

        .align LOOP_ALIGN
8:
        ld1             {v0.8h}, [x1], x2
        ld1             {v1.8h}, [x1], x2
        subs            w4, w4, #2
        mad             z0.h, p0/m, z29.h, z30.h
        mad             z1.h, p0/m, z29.h, z30.h
        stp             q0, q1, [x0], #32
        b.gt            8b
        ret

        .align JUMP_ALIGN
160:    // prep - 16xN
        AARCH64_VALID_JUMP_TARGET

        .align LOOP_ALIGN
16:
        ld1             {v0.8h, v1.8h}, [x1], x2
        mad             z0.h, p0/m, z29.h, z30.h
        mad             z1.h, p0/m, z29.h, z30.h
        subs            w4, w4, #2
        ld1             {v2.8h, v3.8h}, [x1], x2
        mad             z2.h, p0/m, z29.h, z30.h
        mad             z3.h, p0/m, z29.h, z30.h
        stp             q0, q1, [x0]
        stp             q2, q3, [x0, #32]
        add             x0, x0, #64
        b.gt            16b
        ret

        .align JUMP_ALIGN
320:    // prep - 32xN
        AARCH64_VALID_JUMP_TARGET

        .align LOOP_ALIGN
32:
        ldp             q0, q1, [x1]
        mad             z0.h, p0/m, z29.h, z30.h
        mad             z1.h, p0/m, z29.h, z30.h
        ldp             q2, q3, [x1, #32]
        subs            w4, w4, #1
        mad             z2.h, p0/m, z29.h, z30.h
        mad             z3.h, p0/m, z29.h, z30.h
        add             x1, x1, x2
        stp             q0, q1, [x0]
        stp             q2, q3, [x0, #32]
        add             x0, x0, #64
        b.gt            32b
        ret

        .align JUMP_ALIGN
640:    // prep - 64xN
        AARCH64_VALID_JUMP_TARGET

        .align LOOP_ALIGN
64:
        ldp             q0, q1, [x1]
        mad             z0.h, p0/m, z29.h, z30.h
        mad             z1.h, p0/m, z29.h, z30.h
        ldp             q2, q3, [x1, #32]
        mad             z2.h, p0/m, z29.h, z30.h
        mad             z3.h, p0/m, z29.h, z30.h
        ldp             q4, q5, [x1, #64]
        mad             z4.h, p0/m, z29.h, z30.h
        mad             z5.h, p0/m, z29.h, z30.h
        ldp             q6, q7, [x1, #96]
        add             x1, x1, x2
        subs            w4, w4, #1
        mad             z6.h, p0/m, z29.h, z30.h
        mad             z7.h, p0/m, z29.h, z30.h
        stp             q0, q1, [x0]
        stp             q2, q3, [x0, #32]
        stp             q4, q5, [x0, #64]
        stp             q6, q7, [x0, #96]
        add             x0, x0, #128
        b.gt            64b
        ret

        .align JUMP_ALIGN
1280:   // prep - 128xN
        AARCH64_VALID_JUMP_TARGET

        .align LOOP_ALIGN
128:
        ldp             q0, q1, [x1]
        mad             z0.h, p0/m, z29.h, z30.h
        mad             z1.h, p0/m, z29.h, z30.h
        ldp             q2, q3, [x1, #32]
        mad             z2.h, p0/m, z29.h, z30.h
        mad             z3.h, p0/m, z29.h, z30.h
        ldp             q4, q5, [x1, #64]
        mad             z4.h, p0/m, z29.h, z30.h
        mad             z5.h, p0/m, z29.h, z30.h
        ldp             q6, q7, [x1, #96]
        mad             z6.h, p0/m, z29.h, z30.h
        mad             z7.h, p0/m, z29.h, z30.h
        ldp             q16, q17, [x1, #128]
        mad             z16.h, p0/m, z29.h, z30.h
        mad             z17.h, p0/m, z29.h, z30.h
        ldp             q18, q19, [x1, #160]
        mad             z18.h, p0/m, z29.h, z30.h
        mad             z19.h, p0/m, z29.h, z30.h
        ldp             q20, q21, [x1, #192]
        mad             z20.h, p0/m, z29.h, z30.h
        mad             z21.h, p0/m, z29.h, z30.h
        ldp             q22, q23, [x1, #224]
        add             x1, x1, x2
        mad             z22.h, p0/m, z29.h, z30.h
        mad             z23.h, p0/m, z29.h, z30.h
        subs            w4, w4, #1
        stp             q0, q1, [x0]
        stp             q2, q3, [x0, #32]
        stp             q4, q5, [x0, #64]
        stp             q6, q7, [x0, #96]
        stp             q16, q17, [x0, #128]
        stp             q18, q19, [x0, #160]
        stp             q20, q21, [x0, #192]
        stp             q22, q23, [x0, #224]
        add             x0, x0, #256
        b.gt            128b
        ret
endfunc

jumptable prep_tbl
        .word 1280b - prep_tbl
        .word 640b  - prep_tbl
        .word 320b  - prep_tbl
        .word 160b  - prep_tbl
        .word 80b   - prep_tbl
        .word 40b   - prep_tbl
endjumptable


// dst(x0), d_strd(x9), src(x1), s_strd(x2), w(w3), h(w4), mx(w5), my(w6), bdmax(w7)
// xmx(x5), xmy(x6), ldst(x5), lsrc(x6), wd_strd(w9), ws_strd(w2)
filter_8tap_fn prep, sve2, x0, x9, x1, x2, w3, w4, w5, w6, w7, x5, x6, x5, x6, w9, w2

// dst(x0) d_strd(x1) src(x2) s_strd(x3) w(w4) h(w5) mx(w6) my(w7), bdmax(w8)
// xmx(x6), xmy(x7), ldst(x6), lsrc(x7), wd_strd(w1), ws_strd(w3)
filter_8tap_fn  put, sve2, x0, x1, x2, x3, w4, w5, w6, w7, w8, x6, x7, x6, x7, w1, w3

DISABLE_SVE2
DISABLE_SVE
#endif  // HAVE_SVE2

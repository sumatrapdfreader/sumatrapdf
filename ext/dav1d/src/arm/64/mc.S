/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Janne Grunau
 * Copyright © 2018, Martin Storsjo
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

.macro avg dst, t0, t1, t2, t3
        ld1             {\t0\().8h,\t1\().8h},   [x2],  32
        ld1             {\t2\().8h,\t3\().8h},   [x3],  32
        add             \t0\().8h,   \t0\().8h,   \t2\().8h
        add             \t1\().8h,   \t1\().8h,   \t3\().8h
        sqrshrun        \dst\().8b,  \t0\().8h,   #5
        sqrshrun2       \dst\().16b, \t1\().8h,   #5
.endm

.macro w_avg dst, t0, t1, t2, t3
        ld1             {\t0\().8h,\t1\().8h},   [x2],  32
        ld1             {\t2\().8h,\t3\().8h},   [x3],  32
        sub             \t0\().8h,   \t2\().8h,   \t0\().8h
        sub             \t1\().8h,   \t3\().8h,   \t1\().8h
        sqdmulh         \t0\().8h,   \t0\().8h,   v30.8h
        sqdmulh         \t1\().8h,   \t1\().8h,   v30.8h
        add             \t0\().8h,   \t2\().8h,   \t0\().8h
        add             \t1\().8h,   \t3\().8h,   \t1\().8h
        sqrshrun        \dst\().8b,  \t0\().8h,   #4
        sqrshrun2       \dst\().16b, \t1\().8h,   #4
.endm

.macro mask dst, t0, t1, t2, t3
        ld1             {v30.16b}, [x6],  16
        ld1             {\t0\().8h,\t1\().8h},   [x2],  32
        mul             v30.16b, v30.16b, v31.16b
        ld1             {\t2\().8h,\t3\().8h},   [x3],  32
        shll            v28.8h, v30.8b,  #8
        shll2           v29.8h, v30.16b, #8
        sub             \t0\().8h,   \t2\().8h,   \t0\().8h
        sub             \t1\().8h,   \t3\().8h,   \t1\().8h
        sqdmulh         \t0\().8h,   \t0\().8h,   v28.8h
        sqdmulh         \t1\().8h,   \t1\().8h,   v29.8h
        add             \t0\().8h,   \t2\().8h,   \t0\().8h
        add             \t1\().8h,   \t3\().8h,   \t1\().8h
        sqrshrun        \dst\().8b,  \t0\().8h,   #4
        sqrshrun2       \dst\().16b, \t1\().8h,   #4
.endm

.macro bidir_fn type
function \type\()_8bpc_neon, export=1
        clz             w4,  w4
.ifc \type, w_avg
        dup             v30.8h, w6
        neg             v30.8h, v30.8h
        shl             v30.8h, v30.8h, #11
.endif
.ifc \type, mask
        movi            v31.16b, #256-2
.endif
        movrel          x7,  \type\()_tbl
        sub             w4,  w4,  #24
        ldrsw           x4,  [x7, x4, lsl #2]
        \type           v4,  v0,  v1,  v2,  v3
        add             x7,  x7,  x4
        br              x7
40:
        AARCH64_VALID_JUMP_TARGET
        add             x7,  x0,  x1
        lsl             x1,  x1,  #1
4:
        cmp             w5,  #4
        st1             {v4.s}[0],  [x0], x1
        st1             {v4.s}[1],  [x7], x1
        st1             {v4.s}[2],  [x0], x1
        st1             {v4.s}[3],  [x7], x1
        b.eq            0f
        \type           v5,  v0,  v1,  v2,  v3
        cmp             w5,  #8
        st1             {v5.s}[0],  [x0], x1
        st1             {v5.s}[1],  [x7], x1
        st1             {v5.s}[2],  [x0], x1
        st1             {v5.s}[3],  [x7], x1
        b.eq            0f
        \type           v4,  v0,  v1,  v2,  v3
        st1             {v4.s}[0],  [x0], x1
        st1             {v4.s}[1],  [x7], x1
        \type           v5,  v0,  v1,  v2,  v3
        st1             {v4.s}[2],  [x0], x1
        st1             {v4.s}[3],  [x7], x1
        st1             {v5.s}[0],  [x0], x1
        st1             {v5.s}[1],  [x7], x1
        st1             {v5.s}[2],  [x0], x1
        st1             {v5.s}[3],  [x7], x1
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        add             x7,  x0,  x1
        lsl             x1,  x1,  #1
8:
        st1             {v4.8b},    [x0], x1
        \type           v5,  v0,  v1,  v2,  v3
        st1             {v4.d}[1],  [x7], x1
        st1             {v5.8b},    [x0], x1
        subs            w5,  w5,  #4
        st1             {v5.d}[1],  [x7], x1
        b.le            0f
        \type           v4,  v0,  v1,  v2,  v3
        b               8b
160:
        AARCH64_VALID_JUMP_TARGET
16:
        \type           v5,  v0,  v1,  v2,  v3
        st1             {v4.16b}, [x0], x1
        \type           v6,  v0,  v1,  v2,  v3
        st1             {v5.16b}, [x0], x1
        \type           v7,  v0,  v1,  v2,  v3
        st1             {v6.16b}, [x0], x1
        subs            w5,  w5,  #4
        st1             {v7.16b}, [x0], x1
        b.le            0f
        \type           v4,  v0,  v1,  v2,  v3
        b               16b
320:
        AARCH64_VALID_JUMP_TARGET
        add             x7,  x0,  x1
        lsl             x1,  x1,  #1
32:
        \type           v5,  v0,  v1,  v2,  v3
        \type           v6,  v0,  v1,  v2,  v3
        st1             {v4.16b,v5.16b}, [x0], x1
        \type           v7,  v0,  v1,  v2,  v3
        subs            w5,  w5,  #2
        st1             {v6.16b,v7.16b}, [x7], x1
        b.le            0f
        \type           v4,  v0,  v1,  v2,  v3
        b               32b
640:
        AARCH64_VALID_JUMP_TARGET
        add             x7,  x0,  x1
        lsl             x1,  x1,  #1
64:
        \type           v5,  v0,  v1,  v2,  v3
        \type           v6,  v0,  v1,  v2,  v3
        \type           v7,  v0,  v1,  v2,  v3
        \type           v16, v0,  v1,  v2,  v3
        \type           v17, v0,  v1,  v2,  v3
        st1             {v4.16b,v5.16b,v6.16b,v7.16b}, [x0], x1
        \type           v18, v0,  v1,  v2,  v3
        \type           v19, v0,  v1,  v2,  v3
        subs            w5,  w5,  #2
        st1             {v16.16b,v17.16b,v18.16b,v19.16b}, [x7], x1
        b.le            0f
        \type           v4, v0,  v1,  v2,  v3
        b               64b
1280:
        AARCH64_VALID_JUMP_TARGET
        add             x7,  x0,  #64
128:
        \type           v5,  v0,  v1,  v2,  v3
        \type           v6,  v0,  v1,  v2,  v3
        \type           v7,  v0,  v1,  v2,  v3
        \type           v16, v0,  v1,  v2,  v3
        \type           v17, v0,  v1,  v2,  v3
        st1             {v4.16b,v5.16b,v6.16b,v7.16b}, [x0], x1
        \type           v18, v0,  v1,  v2,  v3
        \type           v19, v0,  v1,  v2,  v3
        subs            w5,  w5,  #1
        st1             {v16.16b,v17.16b,v18.16b,v19.16b}, [x7], x1
        b.le            0f
        \type           v4, v0,  v1,  v2,  v3
        b               128b
0:
        ret
endfunc

jumptable \type\()_tbl
        .word 1280b - \type\()_tbl
        .word 640b  - \type\()_tbl
        .word 320b  - \type\()_tbl
        .word 160b  - \type\()_tbl
        .word 80b   - \type\()_tbl
        .word 40b   - \type\()_tbl
endjumptable
.endm

bidir_fn avg
bidir_fn w_avg
bidir_fn mask


.macro w_mask_fn type
function w_mask_\type\()_8bpc_neon, export=1
        clz             w8,  w4
        movrel          x9,  w_mask_\type\()_tbl
        sub             w8,  w8,  #24
        ldrsw           x8,  [x9,  x8,  lsl #2]
        add             x9,  x9,  x8
        mov             w10, #6903
        dup             v0.8h,   w10
.if \type == 444
        movi            v1.16b,  #64
.elseif \type == 422
        dup             v2.8b,   w7
        movi            v3.8b,   #129
        sub             v3.8b,   v3.8b,   v2.8b
.elseif \type == 420
        dup             v2.8h,   w7
        movi            v3.8h,   #1, lsl #8
        sub             v3.8h,   v3.8h,   v2.8h
.endif
        add             x12,  x0,  x1
        lsl             x1,   x1,  #1
        br              x9
40:
        AARCH64_VALID_JUMP_TARGET
4:
        ld1             {v4.8h,   v5.8h},   [x2],  #32  // tmp1 (four rows at once)
        ld1             {v6.8h,   v7.8h},   [x3],  #32  // tmp2 (four rows at once)
        subs            w5,  w5,  #4
        sub             v16.8h,  v6.8h,   v4.8h
        sub             v17.8h,  v7.8h,   v5.8h
        sabd            v18.8h,  v4.8h,   v6.8h
        sabd            v19.8h,  v5.8h,   v7.8h
        uqsub           v18.8h,  v0.8h,   v18.8h
        uqsub           v19.8h,  v0.8h,   v19.8h
        ushr            v18.8h,  v18.8h,  #8
        ushr            v19.8h,  v19.8h,  #8
        shl             v20.8h,  v18.8h,  #9
        shl             v21.8h,  v19.8h,  #9
        sqdmulh         v20.8h,  v20.8h,  v16.8h
        sqdmulh         v21.8h,  v21.8h,  v17.8h
        add             v20.8h,  v20.8h,  v4.8h
        add             v21.8h,  v21.8h,  v5.8h
        sqrshrun        v22.8b,  v20.8h,  #4
        sqrshrun        v23.8b,  v21.8h,  #4
.if \type == 444
        uzp1            v18.16b,  v18.16b, v19.16b      // Same as xtn, xtn2
        sub             v18.16b,  v1.16b,  v18.16b
        st1             {v18.16b}, [x6],  #16
.elseif \type == 422
        addp            v18.8h,   v18.8h,  v19.8h
        xtn             v18.8b,   v18.8h
        uhsub           v18.8b,   v3.8b,   v18.8b
        st1             {v18.8b},  [x6],  #8
.elseif \type == 420
        trn1            v24.2d,   v18.2d,  v19.2d
        trn2            v25.2d,   v18.2d,  v19.2d
        add             v24.8h,   v24.8h,  v25.8h
        addp            v18.8h,   v24.8h,  v24.8h
        sub             v18.4h,   v3.4h,   v18.4h
        rshrn           v18.8b,   v18.8h,  #2
        str             s18,         [x6],  #4
.endif
        st1             {v22.s}[0],  [x0],  x1
        st1             {v22.s}[1],  [x12], x1
        st1             {v23.s}[0],  [x0],  x1
        st1             {v23.s}[1],  [x12], x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
8:
        ld1             {v4.8h,   v5.8h},   [x2],  #32
        ld1             {v6.8h,   v7.8h},   [x3],  #32
        subs            w5,  w5,  #2
        sub             v16.8h,  v6.8h,   v4.8h
        sub             v17.8h,  v7.8h,   v5.8h
        sabd            v18.8h,  v4.8h,   v6.8h
        sabd            v19.8h,  v5.8h,   v7.8h
        uqsub           v18.8h,  v0.8h,   v18.8h
        uqsub           v19.8h,  v0.8h,   v19.8h
        ushr            v18.8h,  v18.8h,  #8
        ushr            v19.8h,  v19.8h,  #8
        shl             v20.8h,  v18.8h,  #9
        shl             v21.8h,  v19.8h,  #9
        sqdmulh         v20.8h,  v20.8h,  v16.8h
        sqdmulh         v21.8h,  v21.8h,  v17.8h
        add             v20.8h,  v20.8h,  v4.8h
        add             v21.8h,  v21.8h,  v5.8h
        sqrshrun        v22.8b,  v20.8h,  #4
        sqrshrun        v23.8b,  v21.8h,  #4
.if \type == 444
        uzp1            v18.16b, v18.16b, v19.16b       // Same as xtn, xtn2
        sub             v18.16b, v1.16b,  v18.16b
        st1             {v18.16b}, [x6],  #16
.elseif \type == 422
        addp            v18.8h,  v18.8h,  v19.8h
        xtn             v18.8b,  v18.8h
        uhsub           v18.8b,  v3.8b,   v18.8b
        st1             {v18.8b},  [x6],  #8
.elseif \type == 420
        add             v18.8h,  v18.8h,  v19.8h
        addp            v18.8h,  v18.8h,  v18.8h
        sub             v18.4h,  v3.4h,   v18.4h
        rshrn           v18.8b,  v18.8h,  #2
        str             s18,       [x6],  #4
.endif
        st1             {v22.8b},  [x0],  x1
        st1             {v23.8b},  [x12], x1
        b.gt            8b
        ret
1280:
640:
320:
160:
        AARCH64_VALID_JUMP_TARGET
        mov             w11, w4
        sub             x1,  x1,  w4,  uxtw
.if \type == 444
        add             x10, x6,  w4,  uxtw
.elseif \type == 422
        add             x10, x6,  x11, lsr #1
.endif
        add             x9,  x3,  w4,  uxtw #1
        add             x7,  x2,  w4,  uxtw #1
161:
        mov             w8,  w4
16:
        ld1             {v4.8h,   v5.8h},   [x2],  #32
        ld1             {v6.8h,   v7.8h},   [x3],  #32
        ld1             {v16.8h,  v17.8h},  [x7],  #32
        ld1             {v18.8h,  v19.8h},  [x9],  #32
        subs            w8,  w8,  #16
        sub             v6.8h,   v6.8h,   v4.8h
        sub             v7.8h,   v7.8h,   v5.8h
        sub             v18.8h,  v18.8h,  v16.8h
        sub             v19.8h,  v19.8h,  v17.8h
        abs             v20.8h,  v6.8h
        abs             v21.8h,  v7.8h
        abs             v22.8h,  v18.8h
        abs             v23.8h,  v19.8h
        uqsub           v20.8h,  v0.8h,   v20.8h
        uqsub           v21.8h,  v0.8h,   v21.8h
        uqsub           v22.8h,  v0.8h,   v22.8h
        uqsub           v23.8h,  v0.8h,   v23.8h
        ushr            v20.8h,  v20.8h,  #8
        ushr            v21.8h,  v21.8h,  #8
        ushr            v22.8h,  v22.8h,  #8
        ushr            v23.8h,  v23.8h,  #8
        shl             v24.8h,  v20.8h,  #9
        shl             v25.8h,  v21.8h,  #9
        shl             v26.8h,  v22.8h,  #9
        shl             v27.8h,  v23.8h,  #9
        sqdmulh         v24.8h,  v24.8h,  v6.8h
        sqdmulh         v25.8h,  v25.8h,  v7.8h
        sqdmulh         v26.8h,  v26.8h,  v18.8h
        sqdmulh         v27.8h,  v27.8h,  v19.8h
        add             v24.8h,  v24.8h,  v4.8h
        add             v25.8h,  v25.8h,  v5.8h
        add             v26.8h,  v26.8h,  v16.8h
        add             v27.8h,  v27.8h,  v17.8h
        sqrshrun        v24.8b,  v24.8h,  #4
        sqrshrun        v25.8b,  v25.8h,  #4
        sqrshrun        v26.8b,  v26.8h,  #4
        sqrshrun        v27.8b,  v27.8h,  #4
.if \type == 444
        uzp1            v20.16b, v20.16b, v21.16b       // Same as xtn, xtn2
        uzp1            v21.16b, v22.16b, v23.16b       // Ditto
        sub             v20.16b, v1.16b,  v20.16b
        sub             v21.16b, v1.16b,  v21.16b
        st1             {v20.16b}, [x6],  #16
        st1             {v21.16b}, [x10], #16
.elseif \type == 422
        addp            v20.8h,  v20.8h,  v21.8h
        addp            v21.8h,  v22.8h,  v23.8h
        xtn             v20.8b,  v20.8h
        xtn             v21.8b,  v21.8h
        uhsub           v20.8b,  v3.8b,   v20.8b
        uhsub           v21.8b,  v3.8b,   v21.8b
        st1             {v20.8b},  [x6],  #8
        st1             {v21.8b},  [x10], #8
.elseif \type == 420
        add             v20.8h,  v20.8h,  v22.8h
        add             v21.8h,  v21.8h,  v23.8h
        addp            v20.8h,  v20.8h,  v21.8h
        sub             v20.8h,  v3.8h,   v20.8h
        rshrn           v20.8b,  v20.8h,  #2
        st1             {v20.8b},  [x6],  #8
.endif
        st1             {v24.8b,  v25.8b},  [x0],  #16
        st1             {v26.8b,  v27.8b},  [x12], #16
        b.gt            16b
        subs            w5,  w5,  #2
        add             x2,  x2,  w4,  uxtw #1
        add             x3,  x3,  w4,  uxtw #1
        add             x7,  x7,  w4,  uxtw #1
        add             x9,  x9,  w4,  uxtw #1
.if \type == 444
        add             x6,  x6,  w4,  uxtw
        add             x10, x10, w4,  uxtw
.elseif \type == 422
        add             x6,  x6,  x11, lsr #1
        add             x10, x10, x11, lsr #1
.endif
        add             x0,  x0,  x1
        add             x12, x12, x1
        b.gt            161b
        ret
endfunc

jumptable w_mask_\type\()_tbl
        .word 1280b - w_mask_\type\()_tbl
        .word 640b  - w_mask_\type\()_tbl
        .word 320b  - w_mask_\type\()_tbl
        .word 160b  - w_mask_\type\()_tbl
        .word 80b   - w_mask_\type\()_tbl
        .word 40b   - w_mask_\type\()_tbl
endjumptable
.endm

w_mask_fn 444
w_mask_fn 422
w_mask_fn 420


function blend_8bpc_neon, export=1
        movrel          x6,  blend_tbl
        clz             w3,  w3
        sub             w3,  w3,  #26
        ldrsw           x3,  [x6,  x3,  lsl #2]
        add             x6,  x6,  x3
        movi            v4.16b,  #64
        add             x8,  x0,  x1
        lsl             x1,  x1,  #1
        br              x6
40:
        AARCH64_VALID_JUMP_TARGET
4:
        ld1             {v2.8b},  [x5],  #8
        ldr             d1,       [x2],  #8
        ldr             s0,       [x0]
        subs            w4,  w4,  #2
        ld1             {v0.s}[1],   [x8]
        sub             v3.8b,   v4.8b,   v2.8b
        umull           v5.8h,   v1.8b,   v2.8b
        umlal           v5.8h,   v0.8b,   v3.8b
        rshrn           v6.8b,   v5.8h,   #6
        st1             {v6.s}[0],   [x0],  x1
        st1             {v6.s}[1],   [x8],  x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
8:
        ld1             {v2.16b},  [x5],  #16
        ld1             {v1.16b},  [x2],  #16
        ldr             d0,        [x0]
        ld1             {v0.d}[1], [x8]
        sub             v3.16b,  v4.16b,  v2.16b
        subs            w4,  w4,  #2
        umull           v5.8h,   v1.8b,   v2.8b
        umlal           v5.8h,   v0.8b,   v3.8b
        umull2          v6.8h,   v1.16b,  v2.16b
        umlal2          v6.8h,   v0.16b,  v3.16b
        rshrn           v7.8b,   v5.8h,   #6
        rshrn           v16.8b,  v6.8h,   #6
        st1             {v7.8b},   [x0],  x1
        st1             {v16.8b},  [x8],  x1
        b.gt            8b
        ret
160:
        AARCH64_VALID_JUMP_TARGET
16:
        ld1             {v1.16b,  v2.16b},  [x5],  #32
        ld1             {v5.16b,  v6.16b},  [x2],  #32
        ld1             {v0.16b},  [x0]
        subs            w4,  w4,  #2
        sub             v7.16b,  v4.16b,  v1.16b
        sub             v20.16b, v4.16b,  v2.16b
        ld1             {v3.16b},  [x8]
        umull           v16.8h,  v5.8b,   v1.8b
        umlal           v16.8h,  v0.8b,   v7.8b
        umull2          v17.8h,  v5.16b,  v1.16b
        umlal2          v17.8h,  v0.16b,  v7.16b
        umull           v21.8h,  v6.8b,   v2.8b
        umlal           v21.8h,  v3.8b,   v20.8b
        umull2          v22.8h,  v6.16b,  v2.16b
        umlal2          v22.8h,  v3.16b,  v20.16b
        rshrn           v18.8b,  v16.8h,  #6
        rshrn2          v18.16b, v17.8h,  #6
        rshrn           v19.8b,  v21.8h,  #6
        rshrn2          v19.16b, v22.8h,  #6
        st1             {v18.16b}, [x0],  x1
        st1             {v19.16b}, [x8],  x1
        b.gt            16b
        ret
320:
        AARCH64_VALID_JUMP_TARGET
32:
        ld1             {v0.16b,  v1.16b,  v2.16b,  v3.16b},  [x5],  #64
        ld1             {v16.16b, v17.16b, v18.16b, v19.16b}, [x2],  #64
        ld1             {v20.16b, v21.16b}, [x0]
        subs            w4,  w4,  #2
        ld1             {v22.16b, v23.16b}, [x8]
        sub             v5.16b,  v4.16b,  v0.16b
        sub             v6.16b,  v4.16b,  v1.16b
        sub             v30.16b, v4.16b,  v2.16b
        sub             v31.16b, v4.16b,  v3.16b
        umull           v24.8h,  v16.8b,  v0.8b
        umlal           v24.8h,  v20.8b,  v5.8b
        umull2          v26.8h,  v16.16b, v0.16b
        umlal2          v26.8h,  v20.16b, v5.16b
        umull           v28.8h,  v17.8b,  v1.8b
        umlal           v28.8h,  v21.8b,  v6.8b
        umull2          v7.8h,   v17.16b, v1.16b
        umlal2          v7.8h,   v21.16b, v6.16b
        umull           v27.8h,  v18.8b,  v2.8b
        umlal           v27.8h,  v22.8b,  v30.8b
        umull2          v1.8h,   v18.16b, v2.16b
        umlal2          v1.8h,   v22.16b, v30.16b
        umull           v29.8h,  v19.8b,  v3.8b
        umlal           v29.8h,  v23.8b,  v31.8b
        umull2          v21.8h,  v19.16b, v3.16b
        umlal2          v21.8h,  v23.16b, v31.16b
        rshrn           v24.8b,  v24.8h,  #6
        rshrn2          v24.16b, v26.8h,  #6
        rshrn           v25.8b,  v28.8h,  #6
        rshrn2          v25.16b, v7.8h,   #6
        rshrn           v27.8b,  v27.8h,  #6
        rshrn2          v27.16b, v1.8h,   #6
        rshrn           v28.8b,  v29.8h,  #6
        rshrn2          v28.16b, v21.8h,  #6
        st1             {v24.16b, v25.16b}, [x0],  x1
        st1             {v27.16b, v28.16b}, [x8],  x1
        b.gt            32b
        ret
endfunc

jumptable blend_tbl
        .word 320b - blend_tbl
        .word 160b - blend_tbl
        .word 80b  - blend_tbl
        .word 40b  - blend_tbl
endjumptable

function blend_h_8bpc_neon, export=1
        movrel          x6,  blend_h_tbl
        movrel          x5,  X(obmc_masks)
        add             x5,  x5,  w4,  uxtw
        sub             w4,  w4,  w4,  lsr #2
        clz             w7,  w3
        movi            v4.16b,  #64
        add             x8,  x0,  x1
        lsl             x1,  x1,  #1
        sub             w7,  w7,  #24
        ldrsw           x7,  [x6,  x7,  lsl #2]
        add             x6,  x6,  x7
        br              x6
20:
        AARCH64_VALID_JUMP_TARGET
2:
        ldr             h0,  [x5],  #2
        ldr             s1,  [x2],  #4
        subs            w4,  w4,  #2
        ldr             h2,  [x0]
        zip1            v0.8b,   v0.8b,   v0.8b
        sub             v3.8b,   v4.8b,   v0.8b
        ld1             {v2.h}[1],   [x8]
        umull           v5.8h,   v1.8b,   v0.8b
        umlal           v5.8h,   v2.8b,   v3.8b
        rshrn           v5.8b,   v5.8h,   #6
        st1             {v5.h}[0],   [x0],  x1
        st1             {v5.h}[1],   [x8],  x1
        b.gt            2b
        ret
40:
        AARCH64_VALID_JUMP_TARGET
4:
        ld2r            {v0.8b,   v1.8b},   [x5],  #2
        ld1             {v2.8b},   [x2],  #8
        subs            w4,  w4,  #2
        ext             v0.8b,   v0.8b,   v1.8b,   #4
        ldr             s3,          [x0]
        sub             v5.8b,   v4.8b,   v0.8b
        ld1             {v3.s}[1],   [x8]
        umull           v6.8h,   v2.8b,   v0.8b
        umlal           v6.8h,   v3.8b,   v5.8b
        rshrn           v6.8b,   v6.8h,   #6
        st1             {v6.s}[0],   [x0],  x1
        st1             {v6.s}[1],   [x8],  x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
8:
        ld2r            {v0.16b,  v1.16b},  [x5],  #2
        ld1             {v2.16b},  [x2],  #16
        ldr             d3,        [x0]
        ext             v0.16b,  v0.16b,  v1.16b,  #8
        sub             v5.16b,  v4.16b,  v0.16b
        ld1             {v3.d}[1], [x8]
        subs            w4,  w4,  #2
        umull           v6.8h,   v0.8b,   v2.8b
        umlal           v6.8h,   v3.8b,   v5.8b
        umull2          v7.8h,   v0.16b,  v2.16b
        umlal2          v7.8h,   v3.16b,  v5.16b
        rshrn           v16.8b,  v6.8h,   #6
        rshrn           v17.8b,  v7.8h,   #6
        st1             {v16.8b},  [x0],  x1
        st1             {v17.8b},  [x8],  x1
        b.gt            8b
        ret
160:
        AARCH64_VALID_JUMP_TARGET
16:
        ld2r            {v0.16b,  v1.16b},  [x5],  #2
        ld1             {v2.16b,  v3.16b},  [x2],  #32
        ld1             {v5.16b},  [x0]
        sub             v7.16b,  v4.16b,  v0.16b
        sub             v16.16b, v4.16b,  v1.16b
        ld1             {v6.16b},  [x8]
        subs            w4,  w4,  #2
        umull           v17.8h,  v0.8b,   v2.8b
        umlal           v17.8h,  v5.8b,   v7.8b
        umull2          v18.8h,  v0.16b,  v2.16b
        umlal2          v18.8h,  v5.16b,  v7.16b
        umull           v19.8h,  v1.8b,   v3.8b
        umlal           v19.8h,  v6.8b,   v16.8b
        umull2          v20.8h,  v1.16b,  v3.16b
        umlal2          v20.8h,  v6.16b,  v16.16b
        rshrn           v21.8b,  v17.8h,  #6
        rshrn2          v21.16b, v18.8h,  #6
        rshrn           v22.8b,  v19.8h,  #6
        rshrn2          v22.16b, v20.8h,  #6
        st1             {v21.16b}, [x0],  x1
        st1             {v22.16b}, [x8],  x1
        b.gt            16b
        ret
1280:
640:
320:
        AARCH64_VALID_JUMP_TARGET
        sub             x1,  x1,  w3,  uxtw
        add             x7,  x2,  w3,  uxtw
321:
        ld2r            {v0.16b,  v1.16b},  [x5],  #2
        mov             w6,  w3
        sub             v20.16b, v4.16b,  v0.16b
        sub             v21.16b, v4.16b,  v1.16b
32:
        ld1             {v16.16b, v17.16b}, [x2],  #32
        ld1             {v2.16b,  v3.16b},  [x0]
        subs            w6,  w6,  #32
        umull           v23.8h,  v0.8b,   v16.8b
        umlal           v23.8h,  v2.8b,   v20.8b
        ld1             {v18.16b, v19.16b}, [x7],  #32
        umull2          v27.8h,  v0.16b,  v16.16b
        umlal2          v27.8h,  v2.16b,  v20.16b
        ld1             {v6.16b,  v7.16b},  [x8]
        umull           v24.8h,  v0.8b,   v17.8b
        umlal           v24.8h,  v3.8b,   v20.8b
        umull2          v28.8h,  v0.16b,  v17.16b
        umlal2          v28.8h,  v3.16b,  v20.16b
        umull           v25.8h,  v1.8b,   v18.8b
        umlal           v25.8h,  v6.8b,   v21.8b
        umull2          v5.8h,   v1.16b,  v18.16b
        umlal2          v5.8h,   v6.16b,  v21.16b
        rshrn           v29.8b,  v23.8h,  #6
        rshrn2          v29.16b, v27.8h,  #6
        umull           v26.8h,  v1.8b,   v19.8b
        umlal           v26.8h,  v7.8b,   v21.8b
        umull2          v31.8h,  v1.16b,  v19.16b
        umlal2          v31.8h,  v7.16b,  v21.16b
        rshrn           v30.8b,  v24.8h,  #6
        rshrn2          v30.16b, v28.8h,  #6
        rshrn           v23.8b,  v25.8h,  #6
        rshrn2          v23.16b, v5.8h,   #6
        rshrn           v24.8b,  v26.8h,  #6
        st1             {v29.16b, v30.16b}, [x0],  #32
        rshrn2          v24.16b, v31.8h,  #6
        st1             {v23.16b, v24.16b}, [x8],  #32
        b.gt            32b
        subs            w4,  w4,  #2
        add             x0,  x0,  x1
        add             x8,  x8,  x1
        add             x2,  x2,  w3,  uxtw
        add             x7,  x7,  w3,  uxtw
        b.gt            321b
        ret
endfunc

jumptable blend_h_tbl
        .word 1280b - blend_h_tbl
        .word 640b  - blend_h_tbl
        .word 320b  - blend_h_tbl
        .word 160b  - blend_h_tbl
        .word 80b   - blend_h_tbl
        .word 40b   - blend_h_tbl
        .word 20b   - blend_h_tbl
endjumptable

function blend_v_8bpc_neon, export=1
        movrel          x6,  blend_v_tbl
        movrel          x5,  X(obmc_masks)
        add             x5,  x5,  w3,  uxtw
        clz             w3,  w3
        movi            v4.16b,  #64
        add             x8,  x0,  x1
        lsl             x1,  x1,  #1
        sub             w3,  w3,  #26
        ldrsw           x3,  [x6,  x3,  lsl #2]
        add             x6,  x6,  x3
        br              x6
20:
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v0.8b},   [x5]
        sub             v1.8b,   v4.8b,   v0.8b
2:
        ldr             h2,          [x2],  #2
        ldr             b3,          [x0]
        subs            w4,  w4,  #2
        ld1             {v2.b}[1],   [x2]
        ld1             {v3.b}[1],   [x8]
        umull           v5.8h,   v2.8b,   v0.8b
        umlal           v5.8h,   v3.8b,   v1.8b
        rshrn           v5.8b,   v5.8h,   #6
        add             x2,  x2,  #2
        st1             {v5.b}[0],   [x0],  x1
        st1             {v5.b}[1],   [x8],  x1
        b.gt            2b
        ret
40:
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v0.2s},   [x5]
        sub             x1,  x1,  #2
        sub             v1.8b,   v4.8b,   v0.8b
4:
        ld1             {v2.8b},   [x2],  #8
        ldr             s3,          [x0]
        ld1             {v3.s}[1],   [x8]
        subs            w4,  w4,  #2
        umull           v5.8h,   v2.8b,   v0.8b
        umlal           v5.8h,   v3.8b,   v1.8b
        rshrn           v5.8b,   v5.8h,   #6
        str             h5,          [x0],  #2
        st1             {v5.h}[2],   [x8],  #2
        st1             {v5.b}[2],   [x0],  x1
        st1             {v5.b}[6],   [x8],  x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v0.2d},   [x5]
        sub             x1,  x1,  #4
        sub             v1.16b,  v4.16b,  v0.16b
        zip2            v16.2d,  v1.2d,   v1.2d
8:
        ld1             {v2.16b},  [x2],  #16
        ldr             d3,          [x0]
        ldr             d4,          [x8]
        subs            w4,  w4,  #2
        umull           v5.8h,  v0.8b,  v2.8b
        umlal           v5.8h,  v3.8b,  v1.8b
        umull2          v6.8h,  v0.16b, v2.16b
        umlal           v6.8h,  v4.8b,  v16.8b
        rshrn           v7.8b,  v5.8h,  #6
        rshrn           v17.8b, v6.8h,  #6
        str             s7,          [x0],  #4
        str             s17,         [x8],  #4
        st1             {v7.h}[2],   [x0],  x1
        st1             {v17.h}[2],  [x8],  x1
        b.gt            8b
        ret
160:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b},  [x5]
        sub             x1,  x1,  #8
        sub             v2.16b,  v4.16b,  v0.16b
16:
        ld1             {v5.16b,  v6.16b},  [x2],  #32
        ld1             {v7.16b},  [x0]
        subs            w4,  w4,  #2
        ld1             {v16.16b}, [x8]
        umull           v17.8h,  v5.8b,   v0.8b
        umlal           v17.8h,  v7.8b,   v2.8b
        umull2          v18.8h,  v5.16b,  v0.16b
        umlal2          v18.8h,  v7.16b,  v2.16b
        umull           v20.8h,  v6.8b,   v0.8b
        umlal           v20.8h,  v16.8b,  v2.8b
        umull2          v21.8h,  v6.16b,  v0.16b
        umlal2          v21.8h,  v16.16b, v2.16b
        rshrn           v19.8b,  v17.8h,  #6
        rshrn2          v19.16b, v18.8h,  #6
        rshrn           v22.8b,  v20.8h,  #6
        rshrn2          v22.16b, v21.8h,  #6
        st1             {v19.8b},  [x0],  #8
        st1             {v22.8b},  [x8],  #8
        st1             {v19.s}[2],  [x0],  x1
        st1             {v22.s}[2],  [x8],  x1
        b.gt            16b
        ret
320:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b,  v1.16b},  [x5]
        sub             x1,  x1,  #16
        sub             v2.16b,  v4.16b,  v0.16b
        sub             v3.8b,   v4.8b,   v1.8b
32:
        ld1             {v16.16b, v17.16b, v18.16b, v19.16b}, [x2],  #64
        ld1             {v5.16b,  v6.16b},  [x0]
        subs            w4,  w4,  #2
        ld1             {v20.16b, v21.16b}, [x8]
        umull           v22.8h,  v16.8b,  v0.8b
        umlal           v22.8h,  v5.8b,   v2.8b
        umull2          v23.8h,  v16.16b, v0.16b
        umlal2          v23.8h,  v5.16b,  v2.16b
        umull           v28.8h,  v17.8b,  v1.8b
        umlal           v28.8h,  v6.8b,   v3.8b
        umull           v30.8h,  v18.8b,  v0.8b
        umlal           v30.8h,  v20.8b,  v2.8b
        umull2          v31.8h,  v18.16b, v0.16b
        umlal2          v31.8h,  v20.16b, v2.16b
        umull           v25.8h,  v19.8b,  v1.8b
        umlal           v25.8h,  v21.8b,  v3.8b
        rshrn           v24.8b,  v22.8h,  #6
        rshrn2          v24.16b, v23.8h,  #6
        rshrn           v28.8b,  v28.8h,  #6
        rshrn           v30.8b,  v30.8h,  #6
        rshrn2          v30.16b, v31.8h,  #6
        rshrn           v27.8b,  v25.8h,  #6
        st1             {v24.16b}, [x0],  #16
        st1             {v30.16b}, [x8],  #16
        st1             {v28.8b},  [x0],  x1
        st1             {v27.8b},  [x8],  x1
        b.gt            32b
        ret
endfunc

jumptable blend_v_tbl
        .word 320b - blend_v_tbl
        .word 160b - blend_v_tbl
        .word 80b  - blend_v_tbl
        .word 40b  - blend_v_tbl
        .word 20b  - blend_v_tbl
endjumptable


// This has got the same signature as the put_8tap functions,
// and assumes that x8 is set to (clz(w)-24).
function put_neon, export=1
        movrel          x9,  put_tbl
        ldrsw           x8,  [x9, x8, lsl #2]
        add             x9,  x9,  x8
        br              x9

20:
        AARCH64_VALID_JUMP_TARGET
2:
        ldrh            w9, [x2]
        ldrh            w10, [x2, x3]
        add             x2, x2, x3, lsl #1
        subs            w5, w5, #2
        strh            w9, [x0]
        strh            w10, [x0, x1]
        add             x0, x0, x1, lsl #1
        b.gt            2b
        ret
40:
        AARCH64_VALID_JUMP_TARGET
4:
        ldr             w9, [x2]
        ldr             w10, [x2, x3]
        add             x2, x2, x3, lsl #1
        subs            w5, w5, #2
        str             w9, [x0]
        str             w10, [x0, x1]
        add             x0, x0, x1, lsl #1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
8:
        ldr             x9, [x2]
        ldr             x10, [x2, x3]
        add             x2, x2, x3, lsl #1
        subs            w5, w5, #2
        str             x9, [x0]
        str             x10, [x0, x1]
        add             x0, x0, x1, lsl #1
        b.gt            8b
        ret
160:
        AARCH64_VALID_JUMP_TARGET
16:
        ldr             q0, [x2]
        ldr             q1, [x2, x3]
        add             x2, x2, x3, lsl #1
        subs            w5, w5, #2
        str             q0, [x0]
        str             q1, [x0, x1]
        add             x0, x0, x1, lsl #1
        b.gt            16b
        ret
320:
        AARCH64_VALID_JUMP_TARGET
32:
        ldp             q0, q1, [x2]
        add             x2, x2, x3
        stp             q0, q1, [x0]
        add             x0, x0, x1
        ldp             q2, q3, [x2]
        add             x2, x2, x3
        stp             q2, q3, [x0]
        subs            w5, w5, #2
        add             x0, x0, x1
        b.gt            32b
        ret
640:
        AARCH64_VALID_JUMP_TARGET
64:
        ldp             q0, q1, [x2]
        stp             q0, q1, [x0]
        ldp             q2, q3, [x2, #32]
        add             x2, x2, x3
        stp             q2, q3, [x0, #32]
        subs            w5, w5, #1
        add             x0, x0, x1
        b.gt            64b
        ret
1280:
        AARCH64_VALID_JUMP_TARGET
128:
        ldp             q0, q1, [x2]
        stp             q0, q1, [x0]
        ldp             q2, q3, [x2, #32]
        stp             q2, q3, [x0, #32]
        ldp             q4, q5, [x2, #64]
        stp             q4, q5, [x0, #64]
        ldp             q6, q7, [x2, #96]
        add             x2, x2, x3
        stp             q6, q7, [x0, #96]
        subs            w5, w5, #1
        add             x0, x0, x1
        b.gt            128b
        ret
endfunc

jumptable put_tbl
        .word 1280b - put_tbl
        .word 640b  - put_tbl
        .word 320b  - put_tbl
        .word 160b  - put_tbl
        .word 80b   - put_tbl
        .word 40b   - put_tbl
        .word 20b   - put_tbl
endjumptable


// This has got the same signature as the prep_8tap functions,
// and assumes that x8 is set to (clz(w)-24), and x7 to w*2.
function prep_neon, export=1
        movrel          x9,  prep_tbl
        ldrsw           x8,  [x9, x8, lsl #2]
        movi            v24.16b, #16
        add             x9,  x9,  x8
        br              x9

40:
        AARCH64_VALID_JUMP_TARGET
4:
        ldr             s0, [x1]
        ldr             s2, [x1, x2]
        add             x1, x1, x2, lsl #1
        ldr             s1, [x1]
        ldr             s3, [x1, x2]
        add             x1, x1, x2, lsl #1
        mov             v0.s[1], v2.s[0]
        mov             v1.s[1], v3.s[0]
        ushll           v0.8h, v0.8b, #4
        ushll           v1.8h, v1.8b, #4
        subs            w4, w4, #4
        stp             q0, q1, [x0], #32
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
8:
        ldr             d0, [x1]
        ldr             d1, [x1, x2]
        add             x1, x1, x2, lsl #1
        ldr             d2, [x1]
        ldr             d3, [x1, x2]
        add             x1, x1, x2, lsl #1
        ushll           v0.8h, v0.8b, #4
        ushll           v1.8h, v1.8b, #4
        umull           v2.8h, v2.8b, v24.8b
        umull           v3.8h, v3.8b, v24.8b
        subs            w4, w4, #4
        stp             q0, q1, [x0]
        stp             q2, q3, [x0, #32]
        add             x0, x0, #64
        b.gt            8b
        ret
160:
        AARCH64_VALID_JUMP_TARGET
16:
        ldr             q1, [x1]
        ldr             q3, [x1, x2]
        add             x1, x1, x2, lsl #1
        ushll           v0.8h, v1.8b, #4
        ushll2          v1.8h, v1.16b, #4
        ldr             q5, [x1]
        ldr             q7, [x1, x2]
        add             x1, x1, x2, lsl #1
        umull           v2.8h, v3.8b, v24.8b
        umull2          v3.8h, v3.16b, v24.16b
        ushll           v4.8h, v5.8b, #4
        ushll2          v5.8h, v5.16b, #4
        umull           v6.8h, v7.8b, v24.8b
        umull2          v7.8h, v7.16b, v24.16b
        subs            w4, w4, #4
        stp             q0, q1, [x0]
        stp             q2, q3, [x0, #32]
        stp             q4, q5, [x0, #64]
        stp             q6, q7, [x0, #96]
        add             x0, x0, #128
        b.gt            16b
        ret
320:
        AARCH64_VALID_JUMP_TARGET
32:
        ldp             q4, q5, [x1]
        add             x1, x1, x2
        ldp             q6, q7, [x1]
        add             x1, x1, x2
        ushll           v0.8h, v4.8b, #4
        ushll2          v1.8h, v4.16b, #4
        umull           v2.8h, v5.8b, v24.8b
        umull2          v3.8h, v5.16b, v24.16b
        ushll           v4.8h, v6.8b, #4
        ushll2          v5.8h, v6.16b, #4
        umull           v6.8h, v7.8b, v24.8b
        umull2          v7.8h, v7.16b, v24.16b
        subs            w4, w4, #2
        stp             q0, q1, [x0]
        stp             q2, q3, [x0, #32]
        stp             q4, q5, [x0, #64]
        stp             q6, q7, [x0, #96]
        add             x0, x0, #128
        b.gt            32b
        ret
640:
        AARCH64_VALID_JUMP_TARGET
64:
        ldp             q4, q5, [x1]
        ldp             q6, q7, [x1, #32]
        add             x1, x1, x2
        ushll           v0.8h, v4.8b, #4
        ushll2          v1.8h, v4.16b, #4
        umull           v2.8h, v5.8b, v24.8b
        umull2          v3.8h, v5.16b, v24.16b
        ushll           v4.8h, v6.8b, #4
        ushll2          v5.8h, v6.16b, #4
        umull           v6.8h, v7.8b, v24.8b
        umull2          v7.8h, v7.16b, v24.16b
        subs            w4, w4, #1
        stp             q0, q1, [x0]
        stp             q2, q3, [x0, #32]
        stp             q4, q5, [x0, #64]
        stp             q6, q7, [x0, #96]
        add             x0, x0, #128
        b.gt            64b
        ret
1280:
        AARCH64_VALID_JUMP_TARGET
128:
        ldp             q28, q29, [x1]
        ldp             q30, q31, [x1, #32]
        ushll           v16.8h, v28.8b, #4
        ushll2          v17.8h, v28.16b, #4
        umull           v18.8h, v29.8b, v24.8b
        umull2          v19.8h, v29.16b, v24.16b
        ushll           v20.8h, v30.8b, #4
        ushll2          v21.8h, v30.16b, #4
        umull           v22.8h, v31.8b, v24.8b
        umull2          v23.8h, v31.16b, v24.16b
        ldp             q28, q29, [x1, #64]
        ldp             q30, q31, [x1, #96]
        add             x1, x1, x2
        stp             q16, q17, [x0]
        stp             q18, q19, [x0, #32]
        stp             q20, q21, [x0, #64]
        stp             q22, q23, [x0, #96]
        ushll           v16.8h, v28.8b, #4
        ushll2          v17.8h, v28.16b, #4
        umull           v18.8h, v29.8b, v24.8b
        umull2          v19.8h, v29.16b, v24.16b
        ushll           v20.8h, v30.8b, #4
        ushll2          v21.8h, v30.16b, #4
        umull           v22.8h, v31.8b, v24.8b
        umull2          v23.8h, v31.16b, v24.16b
        subs            w4, w4, #1
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


.macro load_slice s0, s1, strd, wd, d0, d1, d2, d3, d4, d5, d6
        ld1             {\d0\wd}[0], [\s0], \strd
        ld1             {\d1\wd}[0], [\s1], \strd
.ifnb \d2
        ld1             {\d2\wd}[0], [\s0], \strd
        ld1             {\d3\wd}[0], [\s1], \strd
.endif
.ifnb \d4
        ld1             {\d4\wd}[0], [\s0], \strd
.endif
.ifnb \d5
        ld1             {\d5\wd}[0], [\s1], \strd
.endif
.ifnb \d6
        ld1             {\d6\wd}[0], [\s0], \strd
.endif
.endm
.macro load_reg s0, s1, strd, wd, d0, d1, d2, d3, d4, d5, d6
        ld1             {\d0\wd}, [\s0], \strd
        ld1             {\d1\wd}, [\s1], \strd
.ifnb \d2
        ld1             {\d2\wd}, [\s0], \strd
        ld1             {\d3\wd}, [\s1], \strd
.endif
.ifnb \d4
        ld1             {\d4\wd}, [\s0], \strd
.endif
.ifnb \d5
        ld1             {\d5\wd}, [\s1], \strd
.endif
.ifnb \d6
        ld1             {\d6\wd}, [\s0], \strd
.endif
.endm
.macro load_h s0, s1, strd, d0, d1, d2, d3, d4, d5, d6
        load_slice      \s0, \s1, \strd, .h, \d0, \d1, \d2, \d3, \d4, \d5, \d6
.endm
.macro load_s s0, s1, strd, d0, d1, d2, d3, d4, d5, d6
        load_slice      \s0, \s1, \strd, .s, \d0, \d1, \d2, \d3, \d4, \d5, \d6
.endm
.macro load_8b s0, s1, strd, d0, d1, d2, d3, d4, d5, d6
        load_reg        \s0, \s1, \strd, .8b, \d0, \d1, \d2, \d3, \d4, \d5, \d6
.endm
.macro load_16b s0, s1, strd, d0, d1, d2, d3, d4, d5, d6
        load_reg        \s0, \s1, \strd, .16b, \d0, \d1, \d2, \d3, \d4, \d5, \d6
.endm
.macro interleave_1 wd, r0, r1, r2, r3, r4
        trn1            \r0\wd, \r0\wd, \r1\wd
        trn1            \r1\wd, \r1\wd, \r2\wd
.ifnb \r3
        trn1            \r2\wd, \r2\wd, \r3\wd
        trn1            \r3\wd, \r3\wd, \r4\wd
.endif
.endm
.macro interleave_1_h r0, r1, r2, r3, r4
        interleave_1    .4h, \r0, \r1, \r2, \r3, \r4
.endm
.macro interleave_1_s r0, r1, r2, r3, r4
        interleave_1    .2s, \r0, \r1, \r2, \r3, \r4
.endm
.macro interleave_2 wd, r0, r1, r2, r3, r4, r5
        trn1            \r0\wd,  \r0\wd, \r2\wd
        trn1            \r1\wd,  \r1\wd, \r3\wd
        trn1            \r2\wd,  \r2\wd, \r4\wd
        trn1            \r3\wd,  \r3\wd, \r5\wd
.endm
.macro interleave_2_s r0, r1, r2, r3, r4, r5
        interleave_2    .2s, \r0, \r1, \r2, \r3, \r4, \r5
.endm
.macro uxtl_b r0, r1, r2, r3, r4, r5, r6
        uxtl            \r0\().8h, \r0\().8b
        uxtl            \r1\().8h, \r1\().8b
.ifnb \r2
        uxtl            \r2\().8h, \r2\().8b
        uxtl            \r3\().8h, \r3\().8b
.endif
.ifnb \r4
        uxtl            \r4\().8h, \r4\().8b
.endif
.ifnb \r5
        uxtl            \r5\().8h, \r5\().8b
.endif
.ifnb \r6
        uxtl            \r6\().8h, \r6\().8b
.endif
.endm
.macro mul_mla_4tap d, s0, s1, s2, s3, wd
        mul             \d\wd,  \s0\wd,  v0.h[0]
        mla             \d\wd,  \s1\wd,  v0.h[1]
        mla             \d\wd,  \s2\wd,  v0.h[2]
        mla             \d\wd,  \s3\wd,  v0.h[3]
.endm
// Interleaving the mul/mla chains actually hurts performance
// significantly on Cortex A53, thus keeping mul/mla tightly
// chained like this.
.macro mul_mla_6tap_0_4h d0, s0, s1, s2, s3, s4, s5, s6, s7
        mul             \d0\().4h, \s1\().4h, v0.h[1]
        mla             \d0\().4h, \s2\().4h, v0.h[2]
        mla             \d0\().4h, \s3\().4h, v0.h[3]
        mla             \d0\().4h, \s4\().4h, v0.h[4]
        mla             \d0\().4h, \s5\().4h, v0.h[5]
        mla             \d0\().4h, \s6\().4h, v0.h[6]
.endm
.macro mul_mla_6tap_0 d0, s0, s1, s2, s3, s4, s5, s6, s7
        mul             \d0\().8h, \s1\().8h, v0.h[1]
        mla             \d0\().8h, \s2\().8h, v0.h[2]
        mla             \d0\().8h, \s3\().8h, v0.h[3]
        mla             \d0\().8h, \s4\().8h, v0.h[4]
        mla             \d0\().8h, \s5\().8h, v0.h[5]
        mla             \d0\().8h, \s6\().8h, v0.h[6]
.endm
.macro mul_mla_6tap_1 d0, d1, s0, s1, s2, s3, s4, s5, s6, s7, s8
        mul             \d0\().8h, \s1\().8h, v0.h[1]
        mla             \d0\().8h, \s2\().8h, v0.h[2]
        mla             \d0\().8h, \s3\().8h, v0.h[3]
        mla             \d0\().8h, \s4\().8h, v0.h[4]
        mla             \d0\().8h, \s5\().8h, v0.h[5]
        mla             \d0\().8h, \s6\().8h, v0.h[6]
        mul             \d1\().8h, \s2\().8h, v0.h[1]
        mla             \d1\().8h, \s3\().8h, v0.h[2]
        mla             \d1\().8h, \s4\().8h, v0.h[3]
        mla             \d1\().8h, \s5\().8h, v0.h[4]
        mla             \d1\().8h, \s6\().8h, v0.h[5]
        mla             \d1\().8h, \s7\().8h, v0.h[6]
.endm
.macro mul_mla_6tap_2 d0, d1, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9
        mul             \d0\().8h, \s1\().8h, v0.h[1]
        mla             \d0\().8h, \s2\().8h, v0.h[2]
        mla             \d0\().8h, \s3\().8h, v0.h[3]
        mla             \d0\().8h, \s4\().8h, v0.h[4]
        mla             \d0\().8h, \s5\().8h, v0.h[5]
        mla             \d0\().8h, \s6\().8h, v0.h[6]
        mul             \d1\().8h, \s3\().8h, v0.h[1]
        mla             \d1\().8h, \s4\().8h, v0.h[2]
        mla             \d1\().8h, \s5\().8h, v0.h[3]
        mla             \d1\().8h, \s6\().8h, v0.h[4]
        mla             \d1\().8h, \s7\().8h, v0.h[5]
        mla             \d1\().8h, \s8\().8h, v0.h[6]
.endm
.macro mul_mla_8tap_0_4h d0, s0, s1, s2, s3, s4, s5, s6, s7
        mul             \d0\().4h, \s0\().4h, v0.h[0]
        mla             \d0\().4h, \s1\().4h, v0.h[1]
        mla             \d0\().4h, \s2\().4h, v0.h[2]
        mla             \d0\().4h, \s3\().4h, v0.h[3]
        mla             \d0\().4h, \s4\().4h, v0.h[4]
        mla             \d0\().4h, \s5\().4h, v0.h[5]
        mla             \d0\().4h, \s6\().4h, v0.h[6]
        mla             \d0\().4h, \s7\().4h, v0.h[7]
.endm
.macro mul_mla_8tap_0 d0, s0, s1, s2, s3, s4, s5, s6, s7
        mul             \d0\().8h, \s0\().8h, v0.h[0]
        mla             \d0\().8h, \s1\().8h, v0.h[1]
        mla             \d0\().8h, \s2\().8h, v0.h[2]
        mla             \d0\().8h, \s3\().8h, v0.h[3]
        mla             \d0\().8h, \s4\().8h, v0.h[4]
        mla             \d0\().8h, \s5\().8h, v0.h[5]
        mla             \d0\().8h, \s6\().8h, v0.h[6]
        mla             \d0\().8h, \s7\().8h, v0.h[7]
.endm
.macro mul_mla_8tap_1 d0, d1, s0, s1, s2, s3, s4, s5, s6, s7, s8
        mul             \d0\().8h, \s0\().8h, v0.h[0]
        mla             \d0\().8h, \s1\().8h, v0.h[1]
        mla             \d0\().8h, \s2\().8h, v0.h[2]
        mla             \d0\().8h, \s3\().8h, v0.h[3]
        mla             \d0\().8h, \s4\().8h, v0.h[4]
        mla             \d0\().8h, \s5\().8h, v0.h[5]
        mla             \d0\().8h, \s6\().8h, v0.h[6]
        mla             \d0\().8h, \s7\().8h, v0.h[7]
        mul             \d1\().8h, \s1\().8h, v0.h[0]
        mla             \d1\().8h, \s2\().8h, v0.h[1]
        mla             \d1\().8h, \s3\().8h, v0.h[2]
        mla             \d1\().8h, \s4\().8h, v0.h[3]
        mla             \d1\().8h, \s5\().8h, v0.h[4]
        mla             \d1\().8h, \s6\().8h, v0.h[5]
        mla             \d1\().8h, \s7\().8h, v0.h[6]
        mla             \d1\().8h, \s8\().8h, v0.h[7]
.endm
.macro mul_mla_8tap_2 d0, d1, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9
        mul             \d0\().8h, \s0\().8h, v0.h[0]
        mla             \d0\().8h, \s1\().8h, v0.h[1]
        mla             \d0\().8h, \s2\().8h, v0.h[2]
        mla             \d0\().8h, \s3\().8h, v0.h[3]
        mla             \d0\().8h, \s4\().8h, v0.h[4]
        mla             \d0\().8h, \s5\().8h, v0.h[5]
        mla             \d0\().8h, \s6\().8h, v0.h[6]
        mla             \d0\().8h, \s7\().8h, v0.h[7]
        mul             \d1\().8h, \s2\().8h, v0.h[0]
        mla             \d1\().8h, \s3\().8h, v0.h[1]
        mla             \d1\().8h, \s4\().8h, v0.h[2]
        mla             \d1\().8h, \s5\().8h, v0.h[3]
        mla             \d1\().8h, \s6\().8h, v0.h[4]
        mla             \d1\().8h, \s7\().8h, v0.h[5]
        mla             \d1\().8h, \s8\().8h, v0.h[6]
        mla             \d1\().8h, \s9\().8h, v0.h[7]
.endm
.macro sqrshrun_b shift, r0, r1, r2, r3
        sqrshrun        \r0\().8b, \r0\().8h,  #\shift
.ifnb \r1
        sqrshrun        \r1\().8b, \r1\().8h,  #\shift
.endif
.ifnb \r2
        sqrshrun        \r2\().8b, \r2\().8h,  #\shift
        sqrshrun        \r3\().8b, \r3\().8h,  #\shift
.endif
.endm
.macro srshr_h shift, r0, r1, r2, r3
        srshr           \r0\().8h, \r0\().8h,  #\shift
.ifnb \r1
        srshr           \r1\().8h, \r1\().8h,  #\shift
.endif
.ifnb \r2
        srshr           \r2\().8h, \r2\().8h,  #\shift
        srshr           \r3\().8h, \r3\().8h,  #\shift
.endif
.endm
.macro st_h strd, reg, lanes
        st1             {\reg\().h}[0], [x0], \strd
        st1             {\reg\().h}[1], [x8], \strd
.if \lanes > 2
        st1             {\reg\().h}[2], [x0], \strd
        st1             {\reg\().h}[3], [x8], \strd
.endif
.endm
.macro st_s strd, r0, r1
        st1             {\r0\().s}[0], [x0], \strd
        st1             {\r0\().s}[1], [x8], \strd
.ifnb \r1
        st1             {\r1\().s}[0], [x0], \strd
        st1             {\r1\().s}[1], [x8], \strd
.endif
.endm
.macro st_d strd, r0, r1
        st1             {\r0\().8b},   [x0], \strd
        st1             {\r0\().d}[1], [x8], \strd
.ifnb \r1
        st1             {\r1\().8b},   [x0], \strd
        st1             {\r1\().d}[1], [x8], \strd
.endif
.endm
.macro shift_store_4 type, strd, r0, r1
.ifc \type, put
        sqrshrun_b      6,     \r0, \r1
        st_s            \strd, \r0, \r1
.else
        srshr_h         2,     \r0, \r1
        st_d            \strd, \r0, \r1
.endif
.endm
.macro st_reg strd, wd, r0, r1, r2, r3, r4, r5, r6, r7
        st1             {\r0\wd}, [x0], \strd
        st1             {\r1\wd}, [x8], \strd
.ifnb \r2
        st1             {\r2\wd}, [x0], \strd
        st1             {\r3\wd}, [x8], \strd
.endif
.ifnb \r4
        st1             {\r4\wd}, [x0], \strd
        st1             {\r5\wd}, [x8], \strd
        st1             {\r6\wd}, [x0], \strd
        st1             {\r7\wd}, [x8], \strd
.endif
.endm
.macro st_8b strd, r0, r1, r2, r3, r4, r5, r6, r7
        st_reg          \strd, .8b,  \r0, \r1, \r2, \r3, \r4, \r5, \r6, \r7
.endm
.macro st_16b strd, r0, r1, r2, r3, r4, r5, r6, r7
        st_reg          \strd, .16b, \r0, \r1, \r2, \r3, \r4, \r5, \r6, \r7
.endm
.macro shift_store_8 type, strd, r0, r1, r2, r3
.ifc \type, put
        sqrshrun_b      6,     \r0, \r1, \r2, \r3
        st_8b           \strd, \r0, \r1, \r2, \r3
.else
        srshr_h         2,     \r0, \r1, \r2, \r3
        st_16b          \strd, \r0, \r1, \r2, \r3
.endif
.endm
.macro shift_store_16 type, strd, r0, r1, r2, r3
.ifc \type, put
        sqrshrun        \r0\().8b,  \r0\().8h, #6
        sqrshrun2       \r0\().16b, \r1\().8h, #6
        sqrshrun        \r2\().8b,  \r2\().8h, #6
        sqrshrun2       \r2\().16b, \r3\().8h, #6
        st_16b          \strd, \r0, \r2
.else
        srshr_h         2,     \r0, \r1, \r2, \r3
        st1             {\r0\().8h, \r1\().8h}, [x0], \strd
        st1             {\r2\().8h, \r3\().8h}, [x8], \strd
.endif
.endm

.macro make_8tap_fn op, type, type_h, type_v, taps
function \op\()_8tap_\type\()_8bpc_neon, export=1
        mov             x8,  \type_h
        mov             x9,  \type_v
        b               \op\()_\taps\()_neon
endfunc
.endm

// No spaces in these expressions, due to gas-preprocessor.
#define REGULAR ((0*15<<7)|3*15)
#define SMOOTH  ((1*15<<7)|4*15)
#define SHARP   ((2*15<<7)|3*15)

.macro filter_fn type, dst, d_strd, src, s_strd, w, h, mx, xmx, my, xmy, ds2, sr2, shift_hv, taps
function \type\()_\taps\()_neon
        mov             w10,  #0x4081  // (1 << 14) | (1 << 7) | (1 << 0)
        mul             \mx,  \mx, w10
        mul             \my,  \my, w10
        add             \mx,  \mx, w8 // mx, 8tap_h, 4tap_h
        add             \my,  \my, w9 // my, 8tap_v, 4tap_v
.ifc \type, prep
        uxtw            \d_strd, \w
        lsl             \d_strd, \d_strd, #1
.endif

        clz             w8,  \w
        tst             \mx, #(0x7f << 14)
        sub             w8,  w8,  #24
        movrel          x10, X(mc_subpel_filters), -8
        b.ne            L(\type\()_\taps\()_h)
        tst             \my, #(0x7f << 14)
        b.ne            L(\type\()_\taps\()_v)
        b               \type\()_neon

L(\type\()_\taps\()_h):
        cmp             \w,  #4
        ubfx            w9,  \mx, #7, #7
        and             \mx, \mx, #0x7f
        b.le            4f
        mov             \mx,  w9
4:
        tst             \my,  #(0x7f << 14)
        add             \xmx, x10, \mx, uxtw #3
        b.ne            L(\type\()_\taps\()_hv)

        movrel          x9,  \type\()_\taps\()_h_tbl
        ldrsw           x8,  [x9, x8, lsl #2]
        add             x9,  x9,  x8
        br              x9

20:     // 2xN h
        AARCH64_VALID_JUMP_TARGET
.ifc \type, put
        ldur            s0,  [\xmx, #2]
        sub             \src,  \src,  #1
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
        sxtl            v0.8h,  v0.8b
2:
        ld1             {v4.8b},  [\src], \s_strd
        ld1             {v6.8b},  [\sr2], \s_strd
        uxtl            v4.8h,  v4.8b
        uxtl            v6.8h,  v6.8b
        ext             v5.16b, v4.16b, v4.16b, #2
        ext             v7.16b, v6.16b, v6.16b, #2
        subs            \h,  \h,  #2
        trn1            v3.2s,  v4.2s,  v6.2s
        trn2            v6.2s,  v4.2s,  v6.2s
        trn1            v4.2s,  v5.2s,  v7.2s
        trn2            v7.2s,  v5.2s,  v7.2s
        mul             v3.4h,  v3.4h,  v0.h[0]
        mla             v3.4h,  v4.4h,  v0.h[1]
        mla             v3.4h,  v6.4h,  v0.h[2]
        mla             v3.4h,  v7.4h,  v0.h[3]
        srshr           v3.4h,  v3.4h,  #2
        sqrshrun        v3.8b,  v3.8h,  #4
        st1             {v3.h}[0], [\dst], \d_strd
        st1             {v3.h}[1], [\ds2], \d_strd
        b.gt            2b
        ret
.endif

40:     // 4xN h
        AARCH64_VALID_JUMP_TARGET
        ldur            s0,  [\xmx, #2]
        sub             \src,  \src,  #1
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
        sxtl            v0.8h,  v0.8b
4:
        ld1             {v16.8b}, [\src], \s_strd
        ld1             {v20.8b}, [\sr2], \s_strd
        uxtl            v16.8h,  v16.8b
        uxtl            v20.8h,  v20.8b
        ext             v17.16b, v16.16b, v16.16b, #2
        ext             v18.16b, v16.16b, v16.16b, #4
        ext             v19.16b, v16.16b, v16.16b, #6
        ext             v21.16b, v20.16b, v20.16b, #2
        ext             v22.16b, v20.16b, v20.16b, #4
        ext             v23.16b, v20.16b, v20.16b, #6
        subs            \h,  \h,  #2
        mul             v16.4h,  v16.4h,  v0.h[0]
        mla             v16.4h,  v17.4h,  v0.h[1]
        mla             v16.4h,  v18.4h,  v0.h[2]
        mla             v16.4h,  v19.4h,  v0.h[3]
        mul             v20.4h,  v20.4h,  v0.h[0]
        mla             v20.4h,  v21.4h,  v0.h[1]
        mla             v20.4h,  v22.4h,  v0.h[2]
        mla             v20.4h,  v23.4h,  v0.h[3]
        srshr           v16.4h,  v16.4h,  #2
        srshr           v20.4h,  v20.4h,  #2
.ifc \type, put
        sqrshrun        v16.8b,  v16.8h,  #4
        sqrshrun        v20.8b,  v20.8h,  #4
        str             s16,  [\dst]
        str             s20,  [\ds2]
        add             \dst, \dst, \d_strd
        add             \ds2, \ds2, \d_strd
.else
        st1             {v16.4h}, [\dst], \d_strd
        st1             {v20.4h}, [\ds2], \d_strd
.endif
        b.gt            4b
        ret

80:     // 8xN h
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8b}, [\xmx]
.ifc \taps, 6tap
        sub             \src,  \src,  #2
.else
        sub             \src,  \src,  #3
.endif
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
        sxtl            v0.8h, v0.8b
8:
        ld1             {v16.8b, v17.8b},  [\src], \s_strd
        ld1             {v20.8b, v21.8b},  [\sr2], \s_strd
        uxtl            v16.8h,  v16.8b
        uxtl            v17.8h,  v17.8b
        uxtl            v20.8h,  v20.8b
        uxtl            v21.8h,  v21.8b

.ifc \taps, 6tap
        mul             v18.8h,  v16.8h,  v0.h[1]
        mul             v22.8h,  v20.8h,  v0.h[1]
    .irpc i, 23456
        ext             v19.16b, v16.16b, v17.16b, #(2*\i-2)
        ext             v23.16b, v20.16b, v21.16b, #(2*\i-2)
        mla             v18.8h,  v19.8h,  v0.h[\i]
        mla             v22.8h,  v23.8h,  v0.h[\i]
    .endr
.else   // 8tap
        mul             v18.8h,  v16.8h,  v0.h[0]
        mul             v22.8h,  v20.8h,  v0.h[0]
    .irpc i, 1234567
        ext             v19.16b, v16.16b, v17.16b, #(2*\i)
        ext             v23.16b, v20.16b, v21.16b, #(2*\i)
        mla             v18.8h,  v19.8h,  v0.h[\i]
        mla             v22.8h,  v23.8h,  v0.h[\i]
    .endr
.endif
        subs            \h,  \h,  #2
        srshr           v18.8h,  v18.8h, #2
        srshr           v22.8h,  v22.8h, #2
.ifc \type, put
        sqrshrun        v18.8b,  v18.8h, #4
        sqrshrun        v22.8b,  v22.8h, #4
        st1             {v18.8b}, [\dst], \d_strd
        st1             {v22.8b}, [\ds2], \d_strd
.else
        st1             {v18.8h}, [\dst], \d_strd
        st1             {v22.8h}, [\ds2], \d_strd
.endif
        b.gt            8b
        ret
160:
320:
640:
1280:   // 16xN, 32xN, ... h
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8b}, [\xmx]
.ifc \taps, 6tap
        sub             \src,  \src,  #2
.else
        sub             \src,  \src,  #3
.endif
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \s_strd,  \s_strd,  #1
        sxtl            v0.8h, v0.8b

        sub             \s_strd,  \s_strd,  \w, uxtw
        sub             \s_strd,  \s_strd,  #8
.ifc \type, put
        lsl             \d_strd,  \d_strd,  #1
        sub             \d_strd,  \d_strd,  \w, uxtw
.endif
161:
        ld1             {v16.8b, v17.8b, v18.8b},  [\src], #24
        ld1             {v20.8b, v21.8b, v22.8b},  [\sr2], #24
        mov             \mx, \w
        uxtl            v16.8h,  v16.8b
        uxtl            v17.8h,  v17.8b
        uxtl            v18.8h,  v18.8b
        uxtl            v20.8h,  v20.8b
        uxtl            v21.8h,  v21.8b
        uxtl            v22.8h,  v22.8b

16:
.ifc \taps, 6tap
        mul             v24.8h,  v16.8h,  v0.h[1]
        mul             v25.8h,  v17.8h,  v0.h[1]
        mul             v26.8h,  v20.8h,  v0.h[1]
        mul             v27.8h,  v21.8h,  v0.h[1]
    .irpc i, 23456
        ext             v28.16b, v16.16b, v17.16b, #(2*\i-2)
        ext             v29.16b, v17.16b, v18.16b, #(2*\i-2)
        ext             v30.16b, v20.16b, v21.16b, #(2*\i-2)
        ext             v31.16b, v21.16b, v22.16b, #(2*\i-2)
        mla             v24.8h,  v28.8h,  v0.h[\i]
        mla             v25.8h,  v29.8h,  v0.h[\i]
        mla             v26.8h,  v30.8h,  v0.h[\i]
        mla             v27.8h,  v31.8h,  v0.h[\i]
    .endr
.else   // 8tap
        mul             v24.8h,  v16.8h,  v0.h[0]
        mul             v25.8h,  v17.8h,  v0.h[0]
        mul             v26.8h,  v20.8h,  v0.h[0]
        mul             v27.8h,  v21.8h,  v0.h[0]
    .irpc i, 1234567
        ext             v28.16b, v16.16b, v17.16b, #(2*\i)
        ext             v29.16b, v17.16b, v18.16b, #(2*\i)
        ext             v30.16b, v20.16b, v21.16b, #(2*\i)
        ext             v31.16b, v21.16b, v22.16b, #(2*\i)
        mla             v24.8h,  v28.8h,  v0.h[\i]
        mla             v25.8h,  v29.8h,  v0.h[\i]
        mla             v26.8h,  v30.8h,  v0.h[\i]
        mla             v27.8h,  v31.8h,  v0.h[\i]
    .endr
.endif
        srshr           v24.8h,  v24.8h, #2
        srshr           v25.8h,  v25.8h, #2
        srshr           v26.8h,  v26.8h, #2
        srshr           v27.8h,  v27.8h, #2
        subs            \mx, \mx, #16
.ifc \type, put
        sqrshrun        v24.8b,  v24.8h, #4
        sqrshrun2       v24.16b, v25.8h, #4
        sqrshrun        v26.8b,  v26.8h, #4
        sqrshrun2       v26.16b, v27.8h, #4
        st1             {v24.16b}, [\dst], #16
        st1             {v26.16b}, [\ds2], #16
.else
        st1             {v24.8h, v25.8h}, [\dst], #32
        st1             {v26.8h, v27.8h}, [\ds2], #32
.endif
        b.le            9f

        mov             v16.16b, v18.16b
        mov             v20.16b, v22.16b
        ld1             {v17.8b, v18.8b}, [\src], #16
        ld1             {v21.8b, v22.8b}, [\sr2], #16
        uxtl            v17.8h,  v17.8b
        uxtl            v18.8h,  v18.8b
        uxtl            v21.8h,  v21.8b
        uxtl            v22.8h,  v22.8b
        b               16b

9:
        add             \dst,  \dst,  \d_strd
        add             \ds2,  \ds2,  \d_strd
        add             \src,  \src,  \s_strd
        add             \sr2,  \sr2,  \s_strd

        subs            \h,  \h,  #2
        b.gt            161b
        ret
endfunc

jumptable \type\()_\taps\()_h_tbl
        .word 1280b - \type\()_\taps\()_h_tbl
        .word 640b  - \type\()_\taps\()_h_tbl
        .word 320b  - \type\()_\taps\()_h_tbl
        .word 160b  - \type\()_\taps\()_h_tbl
        .word 80b   - \type\()_\taps\()_h_tbl
        .word 40b   - \type\()_\taps\()_h_tbl
        .word 20b   - \type\()_\taps\()_h_tbl
endjumptable

function L(\type\()_\taps\()_v)
        cmp             \h,  #4
        ubfx            w9,  \my, #7, #7
        and             \my, \my, #0x7f
        b.le            4f
        mov             \my, w9
4:
        add             \xmy, x10, \my, uxtw #3

        movrel          x9,  \type\()_\taps\()_v_tbl
        ldrsw           x8,  [x9, x8, lsl #2]
        add             x9,  x9,  x8
        br              x9

20:     // 2xN v
        AARCH64_VALID_JUMP_TARGET
.ifc \type, put
        b.gt            28f

        cmp             \h,  #2
        ldur            s0,  [\xmy, #2]
        sub             \src,  \src,  \s_strd
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \s_strd,  \s_strd,  #1
        lsl             \d_strd,  \d_strd,  #1
        sxtl            v0.8h, v0.8b

        // 2x2 v
        load_h          \src, \sr2, \s_strd, v1, v2, v3, v4, v5
        interleave_1_h  v1, v2, v3, v4, v5
        b.gt            24f
        uxtl_b          v1, v2, v3, v4
        mul_mla_4tap    v6, v1, v2, v3, v4, .4h
        sqrshrun_b      6,  v6
        st_h            \d_strd, v6, 2
        ret

24:     // 2x4 v
        load_h          \sr2, \src, \s_strd, v6, v7
        interleave_1_h  v5, v6, v7
        interleave_2_s  v1, v2, v3, v4, v5, v6
        uxtl_b          v1, v2, v3, v4
        mul_mla_4tap    v6, v1, v2, v3, v4, .8h
        sqrshrun_b      6,  v6
        st_h            \d_strd, v6, 4
        ret

28:     // 2x6, 2x8, 2x12, 2x16 v
        ld1             {v0.8b}, [\xmy]
        sub             \sr2,  \src,  \s_strd, lsl #1
        add             \ds2,  \dst,  \d_strd
        sub             \src,  \sr2,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
        sxtl            v0.8h, v0.8b

        load_h          \src, \sr2, \s_strd, v1,  v2,  v3,  v4, v5, v6, v7
        interleave_1_h  v1,  v2,  v3,  v4,  v5
        interleave_1_h  v5,  v6,  v7
        interleave_2_s  v1,  v2,  v3,  v4,  v5,  v6
        uxtl_b          v1,  v2,  v3,  v4
216:
        subs            \h,  \h,  #4
        load_h          \sr2, \src, \s_strd, v16, v17, v18, v19
        interleave_1_h  v7,  v16, v17, v18, v19
        interleave_2_s  v5,  v6,  v7,  v16, v17, v18
        uxtl_b          v5,  v6,  v7,  v16
        mul_mla_\taps\()_0 v30, v1, v2, v3, v4, v5, v6, v7, v16
        sqrshrun_b      6,   v30
        st_h            \d_strd, v30, 4
        b.le            0f
        cmp             \h,  #2
        mov             v1.16b,  v5.16b
        mov             v2.16b,  v6.16b
        mov             v3.16b,  v7.16b
        mov             v4.16b,  v16.16b
        mov             v5.16b,  v17.16b
        mov             v6.16b,  v18.16b
        mov             v7.16b,  v19.16b
        b.eq            26f
        b               216b
26:
        load_h          \sr2, \src, \s_strd, v16, v17
        interleave_1_h  v7,  v16, v17
        uxtl_b          v5,  v6,  v7,  v16
        mul_mla_\taps\()_0_4h v30, v1, v2, v3, v4, v5, v6, v7, v16
        sqrshrun_b      6,   v30
        st_h            \d_strd, v30, 2
0:
        ret
.endif

40:
        AARCH64_VALID_JUMP_TARGET
        b.gt            480f

        // 4x2, 4x4 v
        cmp             \h,  #2
        ldur            s0,  [\xmy, #2]
        sub             \src, \src, \s_strd
        add             \ds2, \dst, \d_strd
        add             \sr2, \src, \s_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        sxtl            v0.8h, v0.8b

        load_s          \src, \sr2, \s_strd, v1, v2, v3, v4, v5
        interleave_1_s  v1, v2, v3, v4, v5
        uxtl_b          v1, v2, v3, v4
        mul_mla_4tap    v6, v1, v2, v3, v4, .8h
        shift_store_4   \type, \d_strd, v6
        b.le            0f
        load_s          \sr2, \src, \s_strd, v6, v7
        interleave_1_s  v5, v6, v7
        uxtl_b          v5, v6
        mul_mla_4tap    v7, v3, v4, v5, v6, .8h
        shift_store_4   \type, \d_strd, v7
0:
        ret

480:    // 4x6, 4x8, 4x12, 4x16 v
        ld1             {v0.8b}, [\xmy]
        sub             \sr2, \src, \s_strd, lsl #1
        add             \ds2, \dst, \d_strd
        sub             \src, \sr2, \s_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        sxtl            v0.8h, v0.8b

        load_s          \src, \sr2, \s_strd, v16, v17, v18, v19, v20, v21, v22
        interleave_1_s  v16, v17, v18
        interleave_1_s  v18, v19, v20, v21, v22
        uxtl_b          v16, v17
        uxtl_b          v18, v19, v20, v21

48:
        subs            \h,  \h,  #4
        load_s          \sr2, \src, \s_strd, v23, v24, v25, v26
        interleave_1_s  v22, v23, v24, v25, v26
        uxtl_b          v22, v23, v24, v25
        mul_mla_\taps\()_2 v1, v2, v16, v17, v18, v19, v20, v21, v22, v23, v24, v25
        shift_store_4   \type, \d_strd, v1, v2
        b.le            0f
        load_s          \sr2,  \src, \s_strd, v27, v16
        subs            \h,  \h,  #2
        interleave_1_s  v26, v27, v16
        uxtl_b          v26, v27
        mul_mla_\taps\()_0 v1, v20, v21, v22, v23, v24, v25, v26, v27
        shift_store_4   \type, \d_strd, v1
        b.le            0f
        load_s          \sr2,  \src, \s_strd, v17, v18
        subs            \h,  \h,  #2
        interleave_1_s  v16, v17, v18
        uxtl_b          v16, v17
        mul_mla_\taps\()_0 v2, v22, v23, v24, v25, v26, v27, v16, v17
        shift_store_4   \type, \d_strd, v2
        b.le            0f
        subs            \h,  \h,  #4
        load_s          \sr2, \src, \s_strd, v19, v20, v21, v22
        interleave_1_s  v18, v19, v20, v21, v22
        uxtl_b          v18, v19, v20, v21
        mul_mla_\taps\()_2 v1, v2, v24, v25, v26, v27, v16, v17, v18, v19, v20, v21
        shift_store_4   \type, \d_strd, v1, v2
        b.gt            48b
0:
        ret

80:
        AARCH64_VALID_JUMP_TARGET
        b.gt            880f

        // 8x2, 8x4 v
        cmp             \h,  #2
        ldur            s0,  [\xmy, #2]
        sub             \src, \src, \s_strd
        add             \ds2, \dst, \d_strd
        add             \sr2, \src, \s_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        sxtl            v0.8h, v0.8b

        load_8b         \src, \sr2, \s_strd, v1, v2, v3, v4, v5
        uxtl_b          v1, v2, v3, v4, v5
        mul_mla_4tap    v6, v1, v2, v3, v4, .8h
        mul_mla_4tap    v7, v2, v3, v4, v5, .8h
        shift_store_8   \type, \d_strd, v6, v7
        b.le            0f
        load_8b         \sr2, \src, \s_strd, v6, v7
        uxtl_b          v6, v7
        mul_mla_4tap    v1, v3, v4, v5, v6, .8h
        mul_mla_4tap    v2, v4, v5, v6, v7, .8h
        shift_store_8   \type, \d_strd, v1, v2
0:
        ret

880:    // 8x6, 8x8, 8x16, 8x32 v
1680:   // 16x8, 16x16, ...
320:    // 32x8, 32x16, ...
640:
1280:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8b}, [\xmy]
        sub             \src, \src, \s_strd
        sub             \src, \src, \s_strd, lsl #1
        sxtl            v0.8h, v0.8b
        mov             \my,  \h
168:
        add             \ds2, \dst, \d_strd
        add             \sr2, \src, \s_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1

        load_8b         \src, \sr2, \s_strd, v16, v17, v18, v19, v20, v21, v22
        uxtl_b          v16, v17, v18, v19, v20, v21, v22

88:
        subs            \h,  \h,  #2
        load_8b         \sr2, \src, \s_strd, v23, v24
        uxtl_b          v23, v24
        mul_mla_\taps\()_1 v1, v2, v16, v17, v18, v19, v20, v21, v22, v23, v24
        shift_store_8   \type, \d_strd, v1, v2
        b.le            9f
        subs            \h,  \h,  #2
        load_8b         \sr2, \src, \s_strd, v25, v26
        uxtl_b          v25, v26
        mul_mla_\taps\()_1 v3, v4, v18, v19, v20, v21, v22, v23, v24, v25, v26
        shift_store_8   \type, \d_strd, v3, v4
        b.le            9f
        subs            \h,  \h,  #2
        load_8b         \sr2, \src, \s_strd, v27, v16
        uxtl_b          v27, v16
        mul_mla_\taps\()_1 v1, v2, v20, v21, v22, v23, v24, v25, v26, v27, v16
        shift_store_8   \type, \d_strd, v1, v2
        b.le            9f
        subs            \h,  \h,  #2
        load_8b         \sr2, \src, \s_strd, v17, v18
        uxtl_b          v17, v18
        mul_mla_\taps\()_1 v3, v4, v22, v23, v24, v25, v26, v27, v16, v17, v18
        shift_store_8   \type, \d_strd, v3, v4
        b.le            9f
        subs            \h,  \h,  #4
        load_8b         \sr2, \src, \s_strd, v19, v20, v21, v22
        uxtl_b          v19, v20, v21, v22
        mul_mla_\taps\()_1 v1, v2, v24, v25, v26, v27, v16, v17, v18, v19, v20
        mul_mla_\taps\()_1 v3, v4, v26, v27, v16, v17, v18, v19, v20, v21, v22
        shift_store_8   \type, \d_strd, v1, v2, v3, v4
        b.gt            88b
9:
        subs            \w,  \w,  #8
        b.le            0f
        asr             \s_strd, \s_strd, #1
        asr             \d_strd, \d_strd, #1
        msub            \src, \s_strd, \xmy, \src
        msub            \dst, \d_strd, \xmy, \dst
        sub             \src, \src, \s_strd, lsl #3
        mov             \h,  \my
        add             \src, \src, #8
.ifc \type, put
        add             \dst, \dst, #8
.else
        add             \dst, \dst, #16
.endif
        b               168b
0:
        ret

160:
        AARCH64_VALID_JUMP_TARGET
        b.gt            1680b

        // 16x2, 16x4 v
        ldur            s0,  [\xmy, #2]
        sub             \src, \src, \s_strd
        add             \ds2, \dst, \d_strd
        add             \sr2, \src, \s_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        sxtl            v0.8h, v0.8b

        cmp             \h,  #2
        load_16b        \src, \sr2, \s_strd, v1,  v2,  v3,  v4,  v5
        uxtl            v16.8h, v1.8b
        uxtl            v17.8h, v2.8b
        uxtl            v18.8h, v3.8b
        uxtl            v19.8h, v4.8b
        uxtl            v20.8h, v5.8b
        uxtl2           v23.8h, v1.16b
        uxtl2           v24.8h, v2.16b
        uxtl2           v25.8h, v3.16b
        uxtl2           v26.8h, v4.16b
        uxtl2           v27.8h, v5.16b
        mul_mla_4tap    v1,  v16, v17, v18, v19, .8h
        mul_mla_4tap    v16, v17, v18, v19, v20, .8h
        mul_mla_4tap    v2,  v23, v24, v25, v26, .8h
        mul_mla_4tap    v17, v24, v25, v26, v27, .8h
        shift_store_16  \type, \d_strd, v1, v2, v16, v17
        b.le            0f
        load_16b        \sr2, \src, \s_strd, v6,  v7
        uxtl            v21.8h, v6.8b
        uxtl            v22.8h, v7.8b
        uxtl2           v28.8h, v6.16b
        uxtl2           v29.8h, v7.16b
        mul_mla_4tap    v1,  v18, v19, v20, v21, .8h
        mul_mla_4tap    v3,  v19, v20, v21, v22, .8h
        mul_mla_4tap    v2,  v25, v26, v27, v28, .8h
        mul_mla_4tap    v4,  v26, v27, v28, v29, .8h
        shift_store_16  \type, \d_strd, v1, v2, v3, v4
0:
        ret
endfunc

jumptable \type\()_\taps\()_v_tbl
        .word 1280b - \type\()_\taps\()_v_tbl
        .word 640b  - \type\()_\taps\()_v_tbl
        .word 320b  - \type\()_\taps\()_v_tbl
        .word 160b  - \type\()_\taps\()_v_tbl
        .word 80b   - \type\()_\taps\()_v_tbl
        .word 40b   - \type\()_\taps\()_v_tbl
        .word 20b   - \type\()_\taps\()_v_tbl
endjumptable

function L(\type\()_\taps\()_hv)
        cmp             \h,  #4
        ubfx            w9,  \my, #7, #7
        and             \my, \my, #0x7f
        b.le            4f
        mov             \my,  w9
4:
        add             \xmy,  x10, \my, uxtw #3

        movrel          x9,  \type\()_\taps\()_hv_tbl
        ldrsw           x8,  [x9, x8, lsl #2]
        add             x9,  x9,  x8
        br              x9

20:
        AARCH64_VALID_JUMP_TARGET
.ifc \type, put
        ldur            s0,  [\xmx, #2]
        b.gt            280f
        ldur            s1,  [\xmy, #2]

        // 2x2, 2x4 hv
        sub             \sr2, \src, #1
        sub             \src, \sr2, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        sxtl            v0.8h,  v0.8b
        sxtl            v1.8h,  v1.8b
        mov             x15, x30

        ld1             {v28.8b}, [\src], \s_strd
        uxtl            v28.8h,  v28.8b
        ext             v29.16b, v28.16b, v28.16b, #2
        mul             v28.4h,  v28.4h,  v0.4h
        mul             v29.4h,  v29.4h,  v0.4h
        addp            v28.4h,  v28.4h,  v29.4h
        addp            v16.4h,  v28.4h,  v28.4h
        srshr           v16.4h,  v16.4h,  #2
        bl              L(\type\()_\taps\()_filter_2)

        trn1            v16.2s, v16.2s, v28.2s
        mov             v17.8b, v28.8b

2:
        bl              L(\type\()_\taps\()_filter_2)

        ext             v18.8b, v17.8b, v28.8b, #4
        smull           v2.4s,  v16.4h, v1.h[0]
        smlal           v2.4s,  v17.4h, v1.h[1]
        smlal           v2.4s,  v18.4h, v1.h[2]
        smlal           v2.4s,  v28.4h, v1.h[3]

        sqrshrn         v2.4h,  v2.4s,  #\shift_hv
        sqxtun          v2.8b,  v2.8h
        subs            \h,  \h,  #2
        st1             {v2.h}[0], [\dst], \d_strd
        st1             {v2.h}[1], [\ds2], \d_strd
        b.le            0f
        mov             v16.8b, v18.8b
        mov             v17.8b, v28.8b
        b               2b

280:    // 2x8, 2x16, 2x32 hv
        ld1             {v1.8b},  [\xmy]
        sub             \src, \src, #1
        sub             \sr2, \src, \s_strd, lsl #1
        sub             \src, \sr2, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        sxtl            v0.8h,  v0.8b
        sxtl            v1.8h,  v1.8b
        mov             x15, x30

        ld1             {v28.8b}, [\src], \s_strd
        uxtl            v28.8h,  v28.8b
        ext             v29.16b, v28.16b, v28.16b, #2
        mul             v28.4h,  v28.4h,  v0.4h
        mul             v29.4h,  v29.4h,  v0.4h
        addp            v28.4h,  v28.4h,  v29.4h
        addp            v16.4h,  v28.4h,  v28.4h
        srshr           v16.4h,  v16.4h,  #2

        bl              L(\type\()_\taps\()_filter_2)
        trn1            v16.2s, v16.2s, v28.2s
        mov             v17.8b, v28.8b
        bl              L(\type\()_\taps\()_filter_2)
        ext             v18.8b, v17.8b, v28.8b, #4
        mov             v19.8b, v28.8b
        bl              L(\type\()_\taps\()_filter_2)
        ext             v20.8b, v19.8b, v28.8b, #4
        mov             v21.8b, v28.8b

28:
        bl              L(\type\()_\taps\()_filter_2)
        ext             v22.8b, v21.8b, v28.8b, #4
.ifc \taps, 6tap
        smull           v2.4s,  v17.4h, v1.h[1]
        smlal           v2.4s,  v18.4h, v1.h[2]
        smlal           v2.4s,  v19.4h, v1.h[3]
        smlal           v2.4s,  v20.4h, v1.h[4]
        smlal           v2.4s,  v21.4h, v1.h[5]
        smlal           v2.4s,  v22.4h, v1.h[6]
.else   // 8tap
        smull           v2.4s,  v16.4h, v1.h[0]
        smlal           v2.4s,  v17.4h, v1.h[1]
        smlal           v2.4s,  v18.4h, v1.h[2]
        smlal           v2.4s,  v19.4h, v1.h[3]
        smlal           v2.4s,  v20.4h, v1.h[4]
        smlal           v2.4s,  v21.4h, v1.h[5]
        smlal           v2.4s,  v22.4h, v1.h[6]
        smlal           v2.4s,  v28.4h, v1.h[7]
.endif

        sqrshrn         v2.4h,  v2.4s,  #\shift_hv
        sqxtun          v2.8b,  v2.8h
        subs            \h,  \h,  #2
        st1             {v2.h}[0], [\dst], \d_strd
        st1             {v2.h}[1], [\ds2], \d_strd
        b.le            0f
        mov             v16.8b, v18.8b
        mov             v17.8b, v19.8b
        mov             v18.8b, v20.8b
        mov             v19.8b, v21.8b
        mov             v20.8b, v22.8b
        mov             v21.8b, v28.8b
        b               28b

0:
        ret             x15

L(\type\()_\taps\()_filter_2):
        ld1             {v28.8b},  [\sr2], \s_strd
        ld1             {v30.8b},  [\src], \s_strd
        uxtl            v28.8h,  v28.8b
        uxtl            v30.8h,  v30.8b
        ext             v29.16b, v28.16b, v28.16b, #2
        ext             v31.16b, v30.16b, v30.16b, #2
        trn1            v27.2s,  v28.2s,  v30.2s
        trn2            v30.2s,  v28.2s,  v30.2s
        trn1            v28.2s,  v29.2s,  v31.2s
        trn2            v31.2s,  v29.2s,  v31.2s
        mul             v27.4h,  v27.4h,  v0.h[0]
        mla             v27.4h,  v28.4h,  v0.h[1]
        mla             v27.4h,  v30.4h,  v0.h[2]
        mla             v27.4h,  v31.4h,  v0.h[3]
        srshr           v28.4h,  v27.4h,  #2
        ret
.endif

40:
        AARCH64_VALID_JUMP_TARGET
        ldur            s0,  [\xmx, #2]
        b.gt            480f
        ldur            s1,  [\xmy, #2]
        sub             \sr2, \src, #1
        sub             \src, \sr2, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        sxtl            v0.8h,  v0.8b
        sxtl            v1.8h,  v1.8b
        mov             x15, x30

        // 4x2, 4x4 hv
        ld1             {v26.8b}, [\src], \s_strd
        uxtl            v26.8h,  v26.8b
        ext             v28.16b, v26.16b, v26.16b, #2
        ext             v29.16b, v26.16b, v26.16b, #4
        ext             v30.16b, v26.16b, v26.16b, #6
        mul             v31.4h,  v26.4h,  v0.h[0]
        mla             v31.4h,  v28.4h,  v0.h[1]
        mla             v31.4h,  v29.4h,  v0.h[2]
        mla             v31.4h,  v30.4h,  v0.h[3]
        srshr           v16.4h,  v31.4h,  #2

        bl              L(\type\()_\taps\()_filter_4)
        mov             v17.8b, v28.8b
        mov             v18.8b, v29.8b

4:
        bl              L(\type\()_\taps\()_filter_4)
        // Interleaving the mul/mla chains actually hurts performance
        // significantly on Cortex A53, thus keeping mul/mla tightly
        // chained like this.
        smull           v2.4s,  v16.4h, v1.h[0]
        smlal           v2.4s,  v17.4h, v1.h[1]
        smlal           v2.4s,  v18.4h, v1.h[2]
        smlal           v2.4s,  v28.4h, v1.h[3]
        smull           v3.4s,  v17.4h, v1.h[0]
        smlal           v3.4s,  v18.4h, v1.h[1]
        smlal           v3.4s,  v28.4h, v1.h[2]
        smlal           v3.4s,  v29.4h, v1.h[3]
        sqrshrn         v2.4h,  v2.4s,  #\shift_hv
        sqrshrn         v3.4h,  v3.4s,  #\shift_hv
        subs            \h,  \h,  #2
.ifc \type, put
        sqxtun          v2.8b,  v2.8h
        sqxtun          v3.8b,  v3.8h
        str             s2,  [\dst]
        str             s3,  [\ds2]
        add             \dst, \dst, \d_strd
        add             \ds2, \ds2, \d_strd
.else
        st1             {v2.4h}, [\dst], \d_strd
        st1             {v3.4h}, [\ds2], \d_strd
.endif
        b.le            0f
        mov             v16.8b,  v18.8b
        mov             v17.8b,  v28.8b
        mov             v18.8b,  v29.8b
        b               4b

480:    // 4x8, 4x16, 4x32 hv
        ld1             {v1.8b},  [\xmy]
        sub             \src, \src, #1
.ifc \taps, 6tap
        sub             \sr2, \src, \s_strd
        sub             \src, \src, \s_strd, lsl #1
.else
        sub             \sr2, \src, \s_strd, lsl #1
        sub             \src, \sr2, \s_strd
.endif
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1
        sxtl            v0.8h,  v0.8b
        sxtl            v1.8h,  v1.8b
        mov             x15, x30

        ld1             {v26.8b}, [\src], \s_strd
        uxtl            v26.8h,  v26.8b
        ext             v28.16b, v26.16b, v26.16b, #2
        ext             v29.16b, v26.16b, v26.16b, #4
        ext             v30.16b, v26.16b, v26.16b, #6
        mul             v31.4h,  v26.4h,  v0.h[0]
        mla             v31.4h,  v28.4h,  v0.h[1]
        mla             v31.4h,  v29.4h,  v0.h[2]
        mla             v31.4h,  v30.4h,  v0.h[3]
.ifc \taps, 6tap
        srshr           v18.4h,  v31.4h,  #2
.else
        srshr           v16.4h,  v31.4h,  #2

        bl              L(\type\()_\taps\()_filter_4)
        mov             v17.8b, v28.8b
        mov             v18.8b, v29.8b
.endif
        bl              L(\type\()_\taps\()_filter_4)
        mov             v19.8b, v28.8b
        mov             v20.8b, v29.8b
        bl              L(\type\()_\taps\()_filter_4)
        mov             v21.8b, v28.8b
        mov             v22.8b, v29.8b

48:
        bl              L(\type\()_\taps\()_filter_4)
.ifc \taps, 6tap
        smull           v2.4s,  v18.4h, v1.h[1]
        smlal           v2.4s,  v19.4h, v1.h[2]
        smlal           v2.4s,  v20.4h, v1.h[3]
        smlal           v2.4s,  v21.4h, v1.h[4]
        smlal           v2.4s,  v22.4h, v1.h[5]
        smlal           v2.4s,  v28.4h, v1.h[6]
        smull           v3.4s,  v19.4h, v1.h[1]
        smlal           v3.4s,  v20.4h, v1.h[2]
        smlal           v3.4s,  v21.4h, v1.h[3]
        smlal           v3.4s,  v22.4h, v1.h[4]
        smlal           v3.4s,  v28.4h, v1.h[5]
        smlal           v3.4s,  v29.4h, v1.h[6]
.else   // 8tap
        smull           v2.4s,  v16.4h, v1.h[0]
        smlal           v2.4s,  v17.4h, v1.h[1]
        smlal           v2.4s,  v18.4h, v1.h[2]
        smlal           v2.4s,  v19.4h, v1.h[3]
        smlal           v2.4s,  v20.4h, v1.h[4]
        smlal           v2.4s,  v21.4h, v1.h[5]
        smlal           v2.4s,  v22.4h, v1.h[6]
        smlal           v2.4s,  v28.4h, v1.h[7]
        smull           v3.4s,  v17.4h, v1.h[0]
        smlal           v3.4s,  v18.4h, v1.h[1]
        smlal           v3.4s,  v19.4h, v1.h[2]
        smlal           v3.4s,  v20.4h, v1.h[3]
        smlal           v3.4s,  v21.4h, v1.h[4]
        smlal           v3.4s,  v22.4h, v1.h[5]
        smlal           v3.4s,  v28.4h, v1.h[6]
        smlal           v3.4s,  v29.4h, v1.h[7]
.endif
        sqrshrn         v2.4h,  v2.4s,  #\shift_hv
        sqrshrn         v3.4h,  v3.4s,  #\shift_hv
        subs            \h,  \h,  #2
.ifc \type, put
        sqxtun          v2.8b,  v2.8h
        sqxtun          v3.8b,  v3.8h
        str             s2,  [\dst]
        str             s3,  [\ds2]
        add             \dst, \dst, \d_strd
        add             \ds2, \ds2, \d_strd
.else
        st1             {v2.4h}, [\dst], \d_strd
        st1             {v3.4h}, [\ds2], \d_strd
.endif
        b.le            0f
.ifc \taps, 8tap
        mov             v16.8b,  v18.8b
        mov             v17.8b,  v19.8b
.endif
        mov             v18.8b,  v20.8b
        mov             v19.8b,  v21.8b
        mov             v20.8b,  v22.8b
        mov             v21.8b,  v28.8b
        mov             v22.8b,  v29.8b
        b               48b
0:
        ret             x15

L(\type\()_\taps\()_filter_4):
        ld1             {v26.8b}, [\sr2], \s_strd
        ld1             {v27.8b}, [\src], \s_strd
        uxtl            v26.8h,  v26.8b
        uxtl            v27.8h,  v27.8b
        ext             v28.16b, v26.16b, v26.16b, #2
        ext             v29.16b, v26.16b, v26.16b, #4
        ext             v30.16b, v26.16b, v26.16b, #6
        mul             v31.4h,  v26.4h,  v0.h[0]
        mla             v31.4h,  v28.4h,  v0.h[1]
        mla             v31.4h,  v29.4h,  v0.h[2]
        mla             v31.4h,  v30.4h,  v0.h[3]
        ext             v28.16b, v27.16b, v27.16b, #2
        ext             v29.16b, v27.16b, v27.16b, #4
        ext             v30.16b, v27.16b, v27.16b, #6
        mul             v27.4h,  v27.4h,  v0.h[0]
        mla             v27.4h,  v28.4h,  v0.h[1]
        mla             v27.4h,  v29.4h,  v0.h[2]
        mla             v27.4h,  v30.4h,  v0.h[3]
        srshr           v28.4h,  v31.4h,  #2
        srshr           v29.4h,  v27.4h,  #2
        ret

80:
160:
320:
        AARCH64_VALID_JUMP_TARGET
        b.gt            880f
        ld1             {v0.8b},  [\xmx]
        ldur            s1,  [\xmy, #2]
.ifc \taps, 6tap
        sub             \src,  \src,  #2
.else
        sub             \src,  \src,  #3
.endif
        sub             \src,  \src,  \s_strd
        sxtl            v0.8h,  v0.8b
        sxtl            v1.8h,  v1.8b
        mov             x15, x30
        mov             \my,  \h

164:    // 8x2, 8x4, 16x2, 16x4, 32x2, 32x4 hv
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd, \d_strd, #1
        lsl             \s_strd, \s_strd, #1

        bl              L(\type\()_\taps\()_filter_8_first)
        bl              L(\type\()_\taps\()_filter_8)
        mov             v17.16b, v24.16b
        mov             v18.16b, v25.16b

8:
        smull           v2.4s,  v16.4h, v1.h[0]
        smull2          v3.4s,  v16.8h, v1.h[0]
        bl              L(\type\()_\taps\()_filter_8)
        smull           v4.4s,  v17.4h, v1.h[0]
        smull2          v5.4s,  v17.8h, v1.h[0]
        smlal           v2.4s,  v17.4h, v1.h[1]
        smlal2          v3.4s,  v17.8h, v1.h[1]
        smlal           v4.4s,  v18.4h, v1.h[1]
        smlal2          v5.4s,  v18.8h, v1.h[1]
        smlal           v2.4s,  v18.4h, v1.h[2]
        smlal2          v3.4s,  v18.8h, v1.h[2]
        smlal           v4.4s,  v24.4h, v1.h[2]
        smlal2          v5.4s,  v24.8h, v1.h[2]
        smlal           v2.4s,  v24.4h, v1.h[3]
        smlal2          v3.4s,  v24.8h, v1.h[3]
        smlal           v4.4s,  v25.4h, v1.h[3]
        smlal2          v5.4s,  v25.8h, v1.h[3]
        sqrshrn         v2.4h,  v2.4s,  #\shift_hv
        sqrshrn2        v2.8h,  v3.4s,  #\shift_hv
        sqrshrn         v4.4h,  v4.4s,  #\shift_hv
        sqrshrn2        v4.8h,  v5.4s,  #\shift_hv
        subs            \h,  \h,  #2
.ifc \type, put
        sqxtun          v2.8b,  v2.8h
        sqxtun          v4.8b,  v4.8h
        st1             {v2.8b}, [\dst], \d_strd
        st1             {v4.8b}, [\ds2], \d_strd
.else
        st1             {v2.8h}, [\dst], \d_strd
        st1             {v4.8h}, [\ds2], \d_strd
.endif
        b.le            9f
        mov             v16.16b, v18.16b
        mov             v17.16b, v24.16b
        mov             v18.16b, v25.16b
        b               8b
9:
        subs            \w,  \w,  #8
        b.le            0f
        asr             \s_strd,  \s_strd,  #1
        asr             \d_strd,  \d_strd,  #1
        msub            \src,  \s_strd,  \xmy,  \src
        msub            \dst,  \d_strd,  \xmy,  \dst
        sub             \src,  \src,  \s_strd,  lsl #2
        mov             \h,  \my
        add             \src,  \src,  #8
.ifc \type, put
        add             \dst,  \dst,  #8
.else
        add             \dst,  \dst,  #16
.endif
        b               164b

880:    // 8x8, 8x16, ..., 16x8, ..., 32x8, ... hv
640:
1280:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8b},  [\xmx]
        ld1             {v1.8b},  [\xmy]
.ifc \taps, 6tap
        sub             \src,  \src,  #2
.else
        sub             \src,  \src,  #3
        sub             \src,  \src,  \s_strd
.endif
        sub             \src,  \src,  \s_strd, lsl #1
        sxtl            v0.8h,  v0.8b
        sxtl            v1.8h,  v1.8b
        mov             x15, x30
        mov             \my,  \h

168:
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd, \d_strd, #1
        lsl             \s_strd, \s_strd, #1

        bl              L(\type\()_\taps\()_filter_8_first)
.ifc \taps, 6tap
        mov             v18.16b, v16.16b
.else
        bl              L(\type\()_\taps\()_filter_8)
        mov             v17.16b, v24.16b
        mov             v18.16b, v25.16b
.endif
        bl              L(\type\()_\taps\()_filter_8)
        mov             v19.16b, v24.16b
        mov             v20.16b, v25.16b
        bl              L(\type\()_\taps\()_filter_8)
        mov             v21.16b, v24.16b
        mov             v22.16b, v25.16b

88:
.ifc \taps, 6tap
        smull           v2.4s,  v18.4h, v1.h[1]
        smull2          v3.4s,  v18.8h, v1.h[1]
        bl              L(\type\()_\taps\()_filter_8)
        smull           v4.4s,  v19.4h, v1.h[1]
        smull2          v5.4s,  v19.8h, v1.h[1]
        smlal           v2.4s,  v19.4h, v1.h[2]
        smlal2          v3.4s,  v19.8h, v1.h[2]
        smlal           v4.4s,  v20.4h, v1.h[2]
        smlal2          v5.4s,  v20.8h, v1.h[2]
        smlal           v2.4s,  v20.4h, v1.h[3]
        smlal2          v3.4s,  v20.8h, v1.h[3]
        smlal           v4.4s,  v21.4h, v1.h[3]
        smlal2          v5.4s,  v21.8h, v1.h[3]
        smlal           v2.4s,  v21.4h, v1.h[4]
        smlal2          v3.4s,  v21.8h, v1.h[4]
        smlal           v4.4s,  v22.4h, v1.h[4]
        smlal2          v5.4s,  v22.8h, v1.h[4]
        smlal           v2.4s,  v22.4h, v1.h[5]
        smlal2          v3.4s,  v22.8h, v1.h[5]
        smlal           v4.4s,  v24.4h, v1.h[5]
        smlal2          v5.4s,  v24.8h, v1.h[5]
        smlal           v2.4s,  v24.4h, v1.h[6]
        smlal2          v3.4s,  v24.8h, v1.h[6]
        smlal           v4.4s,  v25.4h, v1.h[6]
        smlal2          v5.4s,  v25.8h, v1.h[6]
.else   // 8tap
        smull           v2.4s,  v16.4h, v1.h[0]
        smull2          v3.4s,  v16.8h, v1.h[0]
        bl              L(\type\()_\taps\()_filter_8)
        smull           v4.4s,  v17.4h, v1.h[0]
        smull2          v5.4s,  v17.8h, v1.h[0]
        smlal           v2.4s,  v17.4h, v1.h[1]
        smlal2          v3.4s,  v17.8h, v1.h[1]
        smlal           v4.4s,  v18.4h, v1.h[1]
        smlal2          v5.4s,  v18.8h, v1.h[1]
        smlal           v2.4s,  v18.4h, v1.h[2]
        smlal2          v3.4s,  v18.8h, v1.h[2]
        smlal           v4.4s,  v19.4h, v1.h[2]
        smlal2          v5.4s,  v19.8h, v1.h[2]
        smlal           v2.4s,  v19.4h, v1.h[3]
        smlal2          v3.4s,  v19.8h, v1.h[3]
        smlal           v4.4s,  v20.4h, v1.h[3]
        smlal2          v5.4s,  v20.8h, v1.h[3]
        smlal           v2.4s,  v20.4h, v1.h[4]
        smlal2          v3.4s,  v20.8h, v1.h[4]
        smlal           v4.4s,  v21.4h, v1.h[4]
        smlal2          v5.4s,  v21.8h, v1.h[4]
        smlal           v2.4s,  v21.4h, v1.h[5]
        smlal2          v3.4s,  v21.8h, v1.h[5]
        smlal           v4.4s,  v22.4h, v1.h[5]
        smlal2          v5.4s,  v22.8h, v1.h[5]
        smlal           v2.4s,  v22.4h, v1.h[6]
        smlal2          v3.4s,  v22.8h, v1.h[6]
        smlal           v4.4s,  v24.4h, v1.h[6]
        smlal2          v5.4s,  v24.8h, v1.h[6]
        smlal           v2.4s,  v24.4h, v1.h[7]
        smlal2          v3.4s,  v24.8h, v1.h[7]
        smlal           v4.4s,  v25.4h, v1.h[7]
        smlal2          v5.4s,  v25.8h, v1.h[7]
.endif
        sqrshrn         v2.4h,  v2.4s,  #\shift_hv
        sqrshrn2        v2.8h,  v3.4s,  #\shift_hv
        sqrshrn         v4.4h,  v4.4s,  #\shift_hv
        sqrshrn2        v4.8h,  v5.4s,  #\shift_hv
        subs            \h,  \h,  #2
.ifc \type, put
        sqxtun          v2.8b,  v2.8h
        sqxtun          v4.8b,  v4.8h
        st1             {v2.8b}, [\dst], \d_strd
        st1             {v4.8b}, [\ds2], \d_strd
.else
        st1             {v2.8h}, [\dst], \d_strd
        st1             {v4.8h}, [\ds2], \d_strd
.endif
        b.le            9f
.ifc \taps, 8tap
        mov             v16.16b, v18.16b
        mov             v17.16b, v19.16b
.endif
        mov             v18.16b, v20.16b
        mov             v19.16b, v21.16b
        mov             v20.16b, v22.16b
        mov             v21.16b, v24.16b
        mov             v22.16b, v25.16b
        b               88b
9:
        subs            \w,  \w,  #8
        b.le            0f
        asr             \s_strd,  \s_strd,  #1
        asr             \d_strd,  \d_strd,  #1
        msub            \src,  \s_strd,  \xmy,  \src
        msub            \dst,  \d_strd,  \xmy,  \dst
        sub             \src,  \src,  \s_strd,  lsl #3
        mov             \h,  \my
        add             \src,  \src,  #8
.ifc \type, put
        add             \dst,  \dst,  #8
.else
        add             \dst,  \dst,  #16
.endif
.ifc \taps, 6tap
        add             \src,  \src,  \s_strd,  lsl #1
.endif
        b               168b
0:
        ret             x15

L(\type\()_\taps\()_filter_8_first):
        ld1             {v28.8b, v29.8b},  [\src], \s_strd
        uxtl            v28.8h,  v28.8b
        uxtl            v29.8h,  v29.8b
.ifc \taps, 6tap
        mul             v16.8h,  v28.8h,  v0.h[1]
        ext             v25.16b, v28.16b, v29.16b, #(2*1)
        ext             v26.16b, v28.16b, v29.16b, #(2*2)
        ext             v27.16b, v28.16b, v29.16b, #(2*3)
        mla             v16.8h,  v25.8h,  v0.h[2]
        mla             v16.8h,  v26.8h,  v0.h[3]
        mla             v16.8h,  v27.8h,  v0.h[4]
        ext             v24.16b, v28.16b, v29.16b, #(2*4)
        ext             v25.16b, v28.16b, v29.16b, #(2*5)
        mla             v16.8h,  v24.8h,  v0.h[5]
        mla             v16.8h,  v25.8h,  v0.h[6]
.else   // 8tap
        mul             v16.8h,  v28.8h,  v0.h[0]
        ext             v24.16b, v28.16b, v29.16b, #(2*1)
        ext             v25.16b, v28.16b, v29.16b, #(2*2)
        ext             v26.16b, v28.16b, v29.16b, #(2*3)
        ext             v27.16b, v28.16b, v29.16b, #(2*4)
        mla             v16.8h,  v24.8h,  v0.h[1]
        mla             v16.8h,  v25.8h,  v0.h[2]
        mla             v16.8h,  v26.8h,  v0.h[3]
        mla             v16.8h,  v27.8h,  v0.h[4]
        ext             v24.16b, v28.16b, v29.16b, #(2*5)
        ext             v25.16b, v28.16b, v29.16b, #(2*6)
        ext             v26.16b, v28.16b, v29.16b, #(2*7)
        mla             v16.8h,  v24.8h,  v0.h[5]
        mla             v16.8h,  v25.8h,  v0.h[6]
        mla             v16.8h,  v26.8h,  v0.h[7]
.endif
        srshr           v16.8h,  v16.8h,  #2
        ret

L(\type\()_\taps\()_filter_8):
        ld1             {v28.8b, v29.8b},  [\sr2], \s_strd
        ld1             {v30.8b, v31.8b},  [\src], \s_strd
        uxtl            v28.8h,  v28.8b
        uxtl            v29.8h,  v29.8b
        uxtl            v30.8h,  v30.8b
        uxtl            v31.8h,  v31.8b
.ifc \taps, 6tap
        mul             v24.8h,  v28.8h,  v0.h[1]
        mul             v25.8h,  v30.8h,  v0.h[1]
    .irpc i, 23456
        ext             v26.16b, v28.16b, v29.16b, #(2*\i-2)
        ext             v27.16b, v30.16b, v31.16b, #(2*\i-2)
        mla             v24.8h,  v26.8h,  v0.h[\i]
        mla             v25.8h,  v27.8h,  v0.h[\i]
    .endr
.else   // 8tap
        mul             v24.8h,  v28.8h,  v0.h[0]
        mul             v25.8h,  v30.8h,  v0.h[0]
    .irpc i, 1234567
        ext             v26.16b, v28.16b, v29.16b, #(2*\i)
        ext             v27.16b, v30.16b, v31.16b, #(2*\i)
        mla             v24.8h,  v26.8h,  v0.h[\i]
        mla             v25.8h,  v27.8h,  v0.h[\i]
    .endr
.endif
        srshr           v24.8h,  v24.8h, #2
        srshr           v25.8h,  v25.8h, #2
        ret
endfunc

jumptable \type\()_\taps\()_hv_tbl
        .word 1280b - \type\()_\taps\()_hv_tbl
        .word 640b  - \type\()_\taps\()_hv_tbl
        .word 320b  - \type\()_\taps\()_hv_tbl
        .word 160b  - \type\()_\taps\()_hv_tbl
        .word 80b   - \type\()_\taps\()_hv_tbl
        .word 40b   - \type\()_\taps\()_hv_tbl
        .word 20b   - \type\()_\taps\()_hv_tbl
endjumptable
.endm


.macro filter_bilin_fn type, dst, d_strd, src, s_strd, w, h, mx, xmx, my, xmy, ds2, sr2, shift_hv
function \type\()_bilin_8bpc_neon, export=1
        dup             v1.16b, \mx
        dup             v3.16b, \my
        mov             w9,  #16
        sub             w8, w9, \mx
        sub             w9, w9, \my
        dup             v0.16b, w8
        dup             v2.16b, w9
.ifc \type, prep
        uxtw            \d_strd, \w
        lsl             \d_strd, \d_strd, #1
.endif

        clz             w8,  \w
        sub             w8,  w8,  #24
        cbnz            \mx, L(\type\()_bilin_h)
        cbnz            \my, L(\type\()_bilin_v)
        b               \type\()_neon

L(\type\()_bilin_h):
        cbnz            \my, L(\type\()_bilin_hv)

        movrel          x9,  \type\()_bilin_h_tbl
        ldrsw           x8,  [x9, x8, lsl #2]
        add             x9,  x9,  x8
        br              x9

20:     // 2xN h
        AARCH64_VALID_JUMP_TARGET
.ifc \type, put
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
2:
        ld1r            {v4.4s},  [\src], \s_strd
        ld1r            {v6.4s},  [\sr2], \s_strd
        ext             v5.8b,  v4.8b,  v4.8b, #1
        ext             v7.8b,  v6.8b,  v6.8b, #1
        trn1            v4.4h,  v4.4h,  v6.4h
        trn1            v5.4h,  v5.4h,  v7.4h
        subs            \h,  \h,  #2
        umull           v4.8h,  v4.8b,  v0.8b
        umlal           v4.8h,  v5.8b,  v1.8b
        uqrshrn         v4.8b,  v4.8h,  #4
        st1             {v4.h}[0], [\dst], \d_strd
        st1             {v4.h}[1], [\ds2], \d_strd
        b.gt            2b
        ret
.endif

40:     // 4xN h
        AARCH64_VALID_JUMP_TARGET
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
4:
        ld1             {v4.8b}, [\src], \s_strd
        ld1             {v6.8b}, [\sr2], \s_strd
        ext             v5.8b,  v4.8b,  v4.8b, #1
        ext             v7.8b,  v6.8b,  v6.8b, #1
        trn1            v4.2s,  v4.2s,  v6.2s
        trn1            v5.2s,  v5.2s,  v7.2s
        subs            \h,  \h,  #2
        umull           v4.8h,  v4.8b,  v0.8b
        umlal           v4.8h,  v5.8b,  v1.8b
.ifc \type, put
        uqrshrn         v4.8b,  v4.8h,  #4
        st1             {v4.s}[0], [\dst], \d_strd
        st1             {v4.s}[1], [\ds2], \d_strd
.else
        st1             {v4.8b},   [\dst], \d_strd
        st1             {v4.d}[1], [\ds2], \d_strd
.endif
        b.gt            4b
        ret

80:     // 8xN h
        AARCH64_VALID_JUMP_TARGET
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \d_strd,  \d_strd,  #1
        lsl             \s_strd,  \s_strd,  #1
8:
        ld1             {v4.16b}, [\src], \s_strd
        ld1             {v6.16b}, [\sr2], \s_strd
        ext             v5.16b, v4.16b, v4.16b, #1
        ext             v7.16b, v6.16b, v6.16b, #1
        subs            \h,  \h,  #2
        umull           v4.8h,  v4.8b,  v0.8b
        umull           v6.8h,  v6.8b,  v0.8b
        umlal           v4.8h,  v5.8b,  v1.8b
        umlal           v6.8h,  v7.8b,  v1.8b
.ifc \type, put
        uqrshrn         v4.8b,  v4.8h,  #4
        uqrshrn         v6.8b,  v6.8h,  #4
        st1             {v4.8b}, [\dst], \d_strd
        st1             {v6.8b}, [\ds2], \d_strd
.else
        st1             {v4.8h}, [\dst], \d_strd
        st1             {v6.8h}, [\ds2], \d_strd
.endif
        b.gt            8b
        ret
160:
320:
640:
1280:   // 16xN, 32xN, ... h
        AARCH64_VALID_JUMP_TARGET
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \s_strd,  \s_strd,  #1

        sub             \s_strd,  \s_strd,  \w, uxtw
        sub             \s_strd,  \s_strd,  #8
.ifc \type, put
        lsl             \d_strd,  \d_strd,  #1
        sub             \d_strd,  \d_strd,  \w, uxtw
.endif
161:
        ld1             {v16.d}[1],  [\src], #8
        ld1             {v20.d}[1],  [\sr2], #8
        mov             \mx, \w

16:
        ld1             {v18.16b},  [\src], #16
        ld1             {v22.16b},  [\sr2], #16
        ext             v17.16b, v16.16b, v18.16b, #8
        ext             v19.16b, v16.16b, v18.16b, #9
        ext             v21.16b, v20.16b, v22.16b, #8
        ext             v23.16b, v20.16b, v22.16b, #9
        umull           v16.8h,  v17.8b,  v0.8b
        umull2          v17.8h,  v17.16b, v0.16b
        umull           v20.8h,  v21.8b,  v0.8b
        umull2          v21.8h,  v21.16b, v0.16b
        umlal           v16.8h,  v19.8b,  v1.8b
        umlal2          v17.8h,  v19.16b, v1.16b
        umlal           v20.8h,  v23.8b,  v1.8b
        umlal2          v21.8h,  v23.16b, v1.16b
        subs            \mx, \mx, #16
.ifc \type, put
        uqrshrn         v16.8b,  v16.8h, #4
        uqrshrn2        v16.16b, v17.8h, #4
        uqrshrn         v20.8b,  v20.8h, #4
        uqrshrn2        v20.16b, v21.8h, #4
        st1             {v16.16b}, [\dst], #16
        st1             {v20.16b}, [\ds2], #16
.else
        st1             {v16.8h, v17.8h}, [\dst], #32
        st1             {v20.8h, v21.8h}, [\ds2], #32
.endif
        b.le            9f

        mov             v16.16b, v18.16b
        mov             v20.16b, v22.16b
        b               16b

9:
        add             \dst,  \dst,  \d_strd
        add             \ds2,  \ds2,  \d_strd
        add             \src,  \src,  \s_strd
        add             \sr2,  \sr2,  \s_strd

        subs            \h,  \h,  #2
        b.gt            161b
        ret
endfunc

jumptable \type\()_bilin_h_tbl
        .word 1280b - \type\()_bilin_h_tbl
        .word 640b  - \type\()_bilin_h_tbl
        .word 320b  - \type\()_bilin_h_tbl
        .word 160b  - \type\()_bilin_h_tbl
        .word 80b   - \type\()_bilin_h_tbl
        .word 40b   - \type\()_bilin_h_tbl
        .word 20b   - \type\()_bilin_h_tbl
endjumptable


function L(\type\()_bilin_v)
        cmp             \h,  #4
        movrel          x9,  \type\()_bilin_v_tbl
        ldrsw           x8,  [x9, x8, lsl #2]
        add             x9,  x9,  x8
        br              x9

20:     // 2xN v
        AARCH64_VALID_JUMP_TARGET
.ifc \type, put
        cmp             \h,  #2
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \s_strd,  \s_strd,  #1
        lsl             \d_strd,  \d_strd,  #1

        // 2x2 v
        ld1r            {v16.8h}, [\src], \s_strd
        b.gt            24f
22:
        ld1r            {v17.8h}, [\sr2], \s_strd
        ld1r            {v18.8h}, [\src], \s_strd
        trn1            v16.4h, v16.4h, v17.4h
        trn1            v17.4h, v17.4h, v18.4h
        umull           v4.8h,  v16.8b,  v2.8b
        umlal           v4.8h,  v17.8b,  v3.8b
        uqrshrn         v4.8b,  v4.8h,  #4
        str             h4,        [\dst]
        st1             {v4.h}[1], [\ds2]
        ret
24:     // 2x4, 2x6, 2x8, ... v
        ld1r            {v17.8h}, [\sr2], \s_strd
        ld1r            {v18.8h}, [\src], \s_strd
        ld1r            {v19.8h}, [\sr2], \s_strd
        ld1r            {v20.8h}, [\src], \s_strd
        sub             \h,  \h,  #4
        trn1            v16.4h, v16.4h, v17.4h
        trn1            v17.4h, v17.4h, v18.4h
        trn1            v18.4h, v18.4h, v19.4h
        trn1            v19.4h, v19.4h, v20.4h
        trn1            v16.2s, v16.2s, v18.2s
        trn1            v17.2s, v17.2s, v19.2s
        umull           v4.8h,  v16.8b,  v2.8b
        umlal           v4.8h,  v17.8b,  v3.8b
        cmp             \h,  #2
        uqrshrn         v4.8b,  v4.8h,  #4
        st1             {v4.h}[0], [\dst], \d_strd
        st1             {v4.h}[1], [\ds2], \d_strd
        st1             {v4.h}[2], [\dst], \d_strd
        st1             {v4.h}[3], [\ds2], \d_strd
        b.lt            0f
        mov             v16.8b, v20.8b
        b.eq            22b
        b               24b
0:
        ret
.endif

40:     // 4xN v
        AARCH64_VALID_JUMP_TARGET
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \s_strd,  \s_strd,  #1
        lsl             \d_strd,  \d_strd,  #1
        ld1r            {v16.4s}, [\src], \s_strd
4:
        ld1r            {v17.4s}, [\sr2], \s_strd
        ld1r            {v18.4s}, [\src], \s_strd
        trn1            v16.2s, v16.2s, v17.2s
        trn1            v17.2s, v17.2s, v18.2s
        umull           v4.8h,  v16.8b,  v2.8b
        umlal           v4.8h,  v17.8b,  v3.8b
        subs            \h,  \h,  #2
.ifc \type, put
        uqrshrn         v4.8b,  v4.8h,  #4
        st1             {v4.s}[0], [\dst], \d_strd
        st1             {v4.s}[1], [\ds2], \d_strd
.else
        st1             {v4.8b},   [\dst], \d_strd
        st1             {v4.d}[1], [\ds2], \d_strd
.endif
        b.le            0f
        mov             v16.8b, v18.8b
        b               4b
0:
        ret

80:     // 8xN v
        AARCH64_VALID_JUMP_TARGET
        add             \ds2,  \dst,  \d_strd
        add             \sr2,  \src,  \s_strd
        lsl             \s_strd,  \s_strd,  #1
        lsl             \d_strd,  \d_strd,  #1
        ld1             {v16.8b}, [\src], \s_strd
8:
        ld1             {v17.8b}, [\sr2], \s_strd
        ld1             {v18.8b}, [\src], \s_strd
        umull           v4.8h,  v16.8b,  v2.8b
        umull           v5.8h,  v17.8b,  v2.8b
        umlal           v4.8h,  v17.8b,  v3.8b
        umlal           v5.8h,  v18.8b,  v3.8b
        subs            \h,  \h,  #2
.ifc \type, put
        uqrshrn         v4.8b,  v4.8h,  #4
        uqrshrn         v5.8b,  v5.8h,  #4
        st1             {v4.8b}, [\dst], \d_strd
        st1             {v5.8b}, [\ds2], \d_strd
.else
        st1             {v4.8h}, [\dst], \d_strd
        st1             {v5.8h}, [\ds2], \d_strd
.endif
        b.le            0f
        mov             v16.8b, v18.8b
        b               8b
0:
        ret

160:    // 16xN, 32xN, ...
320:
640:
1280:
        AARCH64_VALID_JUMP_TARGET
        mov             \my,  \h
1:
        add             \ds2, \dst, \d_strd
        add             \sr2, \src, \s_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1

        ld1             {v16.16b}, [\src], \s_strd
2:
        ld1             {v17.16b}, [\sr2], \s_strd
        ld1             {v18.16b}, [\src], \s_strd
        umull           v4.8h,  v16.8b,  v2.8b
        umull2          v5.8h,  v16.16b, v2.16b
        umull           v6.8h,  v17.8b,  v2.8b
        umull2          v7.8h,  v17.16b, v2.16b
        umlal           v4.8h,  v17.8b,  v3.8b
        umlal2          v5.8h,  v17.16b, v3.16b
        umlal           v6.8h,  v18.8b,  v3.8b
        umlal2          v7.8h,  v18.16b, v3.16b
        subs            \h,  \h,  #2
.ifc \type, put
        uqrshrn         v4.8b,  v4.8h,  #4
        uqrshrn2        v4.16b, v5.8h,  #4
        uqrshrn         v6.8b,  v6.8h,  #4
        uqrshrn2        v6.16b, v7.8h,  #4
        st1             {v4.16b}, [\dst], \d_strd
        st1             {v6.16b}, [\ds2], \d_strd
.else
        st1             {v4.8h, v5.8h}, [\dst], \d_strd
        st1             {v6.8h, v7.8h}, [\ds2], \d_strd
.endif
        b.le            9f
        mov             v16.16b, v18.16b
        b               2b
9:
        subs            \w,  \w,  #16
        b.le            0f
        asr             \s_strd, \s_strd, #1
        asr             \d_strd, \d_strd, #1
        msub            \src, \s_strd, \xmy, \src
        msub            \dst, \d_strd, \xmy, \dst
        sub             \src, \src, \s_strd, lsl #1
        mov             \h,  \my
        add             \src, \src, #16
.ifc \type, put
        add             \dst, \dst, #16
.else
        add             \dst, \dst, #32
.endif
        b               1b
0:
        ret
endfunc

jumptable \type\()_bilin_v_tbl
        .word 1280b - \type\()_bilin_v_tbl
        .word 640b  - \type\()_bilin_v_tbl
        .word 320b  - \type\()_bilin_v_tbl
        .word 160b  - \type\()_bilin_v_tbl
        .word 80b   - \type\()_bilin_v_tbl
        .word 40b   - \type\()_bilin_v_tbl
        .word 20b   - \type\()_bilin_v_tbl
endjumptable

function L(\type\()_bilin_hv)
        uxtl            v2.8h, v2.8b
        uxtl            v3.8h, v3.8b
        movrel          x9,  \type\()_bilin_hv_tbl
        ldrsw           x8,  [x9, x8, lsl #2]
        add             x9,  x9,  x8
        br              x9

20:     // 2xN hv
        AARCH64_VALID_JUMP_TARGET
.ifc \type, put
        add             \sr2, \src, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1

        ld1r            {v28.4s},  [\src], \s_strd
        ext             v29.8b, v28.8b, v28.8b, #1
        umull           v16.8h, v28.8b, v0.8b
        umlal           v16.8h, v29.8b, v1.8b

2:
        ld1r            {v28.4s},  [\sr2], \s_strd
        ld1r            {v30.4s},  [\src], \s_strd
        ext             v29.8b, v28.8b, v28.8b, #1
        ext             v31.8b, v30.8b, v30.8b, #1
        trn1            v28.4h, v28.4h, v30.4h
        trn1            v29.4h, v29.4h, v31.4h
        umull           v17.8h, v28.8b, v0.8b
        umlal           v17.8h, v29.8b, v1.8b

        trn1            v16.2s, v16.2s, v17.2s

        mul             v4.4h,  v16.4h, v2.4h
        mla             v4.4h,  v17.4h, v3.4h
        uqrshrn         v4.8b,  v4.8h,  #8
        subs            \h,  \h,  #2
        st1             {v4.h}[0], [\dst], \d_strd
        st1             {v4.h}[1], [\ds2], \d_strd
        b.le            0f
        trn2            v16.2s, v17.2s, v17.2s
        b               2b
0:
        ret
.endif

40:     // 4xN hv
        AARCH64_VALID_JUMP_TARGET
        add             \sr2, \src, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1

        ld1             {v28.8b},  [\src], \s_strd
        ext             v29.8b, v28.8b, v28.8b, #1
        umull           v16.8h, v28.8b, v0.8b
        umlal           v16.8h, v29.8b, v1.8b

4:
        ld1             {v28.8b},  [\sr2], \s_strd
        ld1             {v30.8b},  [\src], \s_strd
        ext             v29.8b, v28.8b, v28.8b, #1
        ext             v31.8b, v30.8b, v30.8b, #1
        trn1            v28.2s, v28.2s, v30.2s
        trn1            v29.2s, v29.2s, v31.2s
        umull           v17.8h, v28.8b, v0.8b
        umlal           v17.8h, v29.8b, v1.8b

        trn1            v16.2d, v16.2d, v17.2d

        mul             v4.8h,  v16.8h, v2.8h
        mla             v4.8h,  v17.8h, v3.8h
        subs            \h,  \h,  #2
.ifc \type, put
        uqrshrn         v4.8b,  v4.8h,  #8
        st1             {v4.s}[0], [\dst], \d_strd
        st1             {v4.s}[1], [\ds2], \d_strd
.else
        urshr           v4.8h,  v4.8h,  #4
        st1             {v4.8b},   [\dst], \d_strd
        st1             {v4.d}[1], [\ds2], \d_strd
.endif
        b.le            0f
        trn2            v16.2d, v17.2d, v17.2d
        b               4b
0:
        ret

80:     // 8xN, 16xN, ... hv
160:
320:
640:
1280:
        AARCH64_VALID_JUMP_TARGET
        mov             \my,  \h

1:
        add             \sr2, \src, \s_strd
        add             \ds2, \dst, \d_strd
        lsl             \s_strd, \s_strd, #1
        lsl             \d_strd, \d_strd, #1

        ld1             {v28.16b},  [\src], \s_strd
        ext             v29.16b, v28.16b, v28.16b, #1
        umull           v16.8h, v28.8b, v0.8b
        umlal           v16.8h, v29.8b, v1.8b

2:
        ld1             {v28.16b},  [\sr2], \s_strd
        ld1             {v30.16b},  [\src], \s_strd
        ext             v29.16b, v28.16b, v28.16b, #1
        ext             v31.16b, v30.16b, v30.16b, #1
        umull           v17.8h, v28.8b, v0.8b
        umlal           v17.8h, v29.8b, v1.8b
        umull           v18.8h, v30.8b, v0.8b
        umlal           v18.8h, v31.8b, v1.8b

        mul             v4.8h,  v16.8h, v2.8h
        mla             v4.8h,  v17.8h, v3.8h
        mul             v5.8h,  v17.8h, v2.8h
        mla             v5.8h,  v18.8h, v3.8h
        subs            \h,  \h,  #2
.ifc \type, put
        uqrshrn         v4.8b,  v4.8h,  #8
        uqrshrn         v5.8b,  v5.8h,  #8
        st1             {v4.8b}, [\dst], \d_strd
        st1             {v5.8b}, [\ds2], \d_strd
.else
        urshr           v4.8h,  v4.8h,  #4
        urshr           v5.8h,  v5.8h,  #4
        st1             {v4.8h}, [\dst], \d_strd
        st1             {v5.8h}, [\ds2], \d_strd
.endif
        b.le            9f
        mov             v16.16b, v18.16b
        b               2b
9:
        subs            \w,  \w,  #8
        b.le            0f
        asr             \s_strd,  \s_strd,  #1
        asr             \d_strd,  \d_strd,  #1
        msub            \src,  \s_strd,  \xmy,  \src
        msub            \dst,  \d_strd,  \xmy,  \dst
        sub             \src,  \src,  \s_strd,  lsl #1
        mov             \h,  \my
        add             \src,  \src,  #8
.ifc \type, put
        add             \dst,  \dst,  #8
.else
        add             \dst,  \dst,  #16
.endif
        b               1b
0:
        ret
endfunc

jumptable \type\()_bilin_hv_tbl
        .word 1280b - \type\()_bilin_hv_tbl
        .word 640b  - \type\()_bilin_hv_tbl
        .word 320b  - \type\()_bilin_hv_tbl
        .word 160b  - \type\()_bilin_hv_tbl
        .word 80b   - \type\()_bilin_hv_tbl
        .word 40b   - \type\()_bilin_hv_tbl
        .word 20b   - \type\()_bilin_hv_tbl
endjumptable
.endm

make_8tap_fn    put,  regular_sharp,  REGULAR, SHARP,   8tap
make_8tap_fn    put,  smooth_sharp,   SMOOTH,  SHARP,   8tap
make_8tap_fn    put,  sharp,          SHARP,   SHARP,   8tap
make_8tap_fn    put,  sharp_regular,  SHARP,   REGULAR, 8tap
make_8tap_fn    put,  sharp_smooth,   SHARP,   SMOOTH,  8tap
filter_fn       put,  x0, x1, x2, x3, w4, w5, w6, x6, w7, x7, x8, x9, 10, 8tap

make_8tap_fn    put,  regular,        REGULAR, REGULAR, 6tap
make_8tap_fn    put,  regular_smooth, REGULAR, SMOOTH,  6tap
make_8tap_fn    put,  smooth,         SMOOTH,  SMOOTH,  6tap
make_8tap_fn    put,  smooth_regular, SMOOTH,  REGULAR, 6tap
filter_fn       put,  x0, x1, x2, x3, w4, w5, w6, x6, w7, x7, x8, x9, 10, 6tap
filter_bilin_fn put,  x0, x1, x2, x3, w4, w5, w6, x6, w7, x7, x8, x9, 10

make_8tap_fn    prep, regular_sharp,  REGULAR, SHARP,   8tap
make_8tap_fn    prep, smooth_sharp,   SMOOTH,  SHARP,   8tap
make_8tap_fn    prep, sharp,          SHARP,   SHARP,   8tap
make_8tap_fn    prep, sharp_regular,  SHARP,   REGULAR, 8tap
make_8tap_fn    prep, sharp_smooth,   SHARP,   SMOOTH,  8tap
filter_fn       prep, x0, x7, x1, x2, w3, w4, w5, x5, w6, x6, x8, x9, 6,  8tap

make_8tap_fn    prep, regular,        REGULAR, REGULAR, 6tap
make_8tap_fn    prep, regular_smooth, REGULAR, SMOOTH,  6tap
make_8tap_fn    prep, smooth,         SMOOTH,  SMOOTH,  6tap
make_8tap_fn    prep, smooth_regular, SMOOTH,  REGULAR, 6tap
filter_fn       prep, x0, x7, x1, x2, w3, w4, w5, x5, w6, x6, x8, x9, 6,  6tap
filter_bilin_fn prep, x0, x7, x1, x2, w3, w4, w5, x5, w6, x6, x8, x9, 6


.macro load_filter_row dst, src, inc
        asr             w13, \src, #10
        add             \src, \src, \inc
        ldr             \dst, [x11, w13, sxtw #3]
.endm

function warp_filter_horz_neon
        add             w12, w5,  #512

        ld1             {v16.8b, v17.8b}, [x2], x3

        load_filter_row d0, w12, w7
        load_filter_row d1, w12, w7
        load_filter_row d2, w12, w7
        load_filter_row d3, w12, w7
        load_filter_row d4, w12, w7
        load_filter_row d5, w12, w7
        load_filter_row d6, w12, w7
        // subtract by 128 to allow using smull
        eor             v16.8b,  v16.8b,  v22.8b
        eor             v17.8b,  v17.8b,  v22.8b
        load_filter_row d7, w12, w7

        ext             v18.8b,  v16.8b,  v17.8b,  #1
        ext             v19.8b,  v16.8b,  v17.8b,  #2
        smull           v0.8h,   v0.8b,   v16.8b
        smull           v1.8h,   v1.8b,   v18.8b
        ext             v18.8b,  v16.8b,  v17.8b,  #3
        ext             v20.8b,  v16.8b,  v17.8b,  #4
        smull           v2.8h,   v2.8b,   v19.8b
        smull           v3.8h,   v3.8b,   v18.8b
        ext             v18.8b,  v16.8b,  v17.8b,  #5
        ext             v19.8b,  v16.8b,  v17.8b,  #6
        smull           v4.8h,   v4.8b,   v20.8b
        smull           v5.8h,   v5.8b,   v18.8b
        ext             v18.8b,  v16.8b,  v17.8b,  #7
        smull           v6.8h,   v6.8b,   v19.8b
        smull           v7.8h,   v7.8b,   v18.8b

        addp            v0.8h,   v0.8h,   v1.8h
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v4.8h,   v4.8h,   v5.8h
        addp            v6.8h,   v6.8h,   v7.8h

        addp            v0.8h,   v0.8h,   v2.8h
        addp            v4.8h,   v4.8h,   v6.8h

        addp            v0.8h,   v0.8h,   v4.8h

        add             w5,  w5,  w8

        ret
endfunc

// void dav1d_warp_affine_8x8_8bpc_neon(
//         pixel *dst, const ptrdiff_t dst_stride,
//         const pixel *src, const ptrdiff_t src_stride,
//         const int16_t *const abcd, int mx, int my)
.macro warp t, shift
function warp_affine_8x8\t\()_8bpc_neon, export=1
        ldr             x4,  [x4]
        sbfx            x7,  x4, #0,  #16
        sbfx            x8,  x4, #16, #16
        sbfx            x9,  x4, #32, #16
        sbfx            x4,  x4, #48, #16
        mov             w10, #8
        sub             x2,  x2,  x3, lsl #1
        sub             x2,  x2,  x3
        sub             x2,  x2,  #3
        movrel          x11, X(mc_warp_filter), 64*8
        mov             x15, x30
.ifnb \t
        lsl             x1,  x1,  #1
.endif

        movi            v22.8b,  #128
.ifb \t
        movi            v23.8h,  #128
.else
        movi            v23.8h,  #8, lsl #8
.endif

        bl              warp_filter_horz_neon
        srshr           v24.8h,  v0.8h,  #3
        bl              warp_filter_horz_neon
        srshr           v25.8h,  v0.8h,  #3
        bl              warp_filter_horz_neon
        srshr           v26.8h,  v0.8h,  #3
        bl              warp_filter_horz_neon
        srshr           v27.8h,  v0.8h,  #3
        bl              warp_filter_horz_neon
        srshr           v28.8h,  v0.8h,  #3
        bl              warp_filter_horz_neon
        srshr           v29.8h,  v0.8h,  #3
        bl              warp_filter_horz_neon
        srshr           v30.8h,  v0.8h,  #3

1:
        add             w14, w6,  #512
        bl              warp_filter_horz_neon
        srshr           v31.8h,  v0.8h,  #3

        load_filter_row d0, w14, w9
        load_filter_row d1, w14, w9
        load_filter_row d2, w14, w9
        load_filter_row d3, w14, w9
        load_filter_row d4, w14, w9
        load_filter_row d5, w14, w9
        load_filter_row d6, w14, w9
        load_filter_row d7, w14, w9
        transpose_8x8b_xtl v0, v1, v2, v3, v4, v5, v6, v7, sxtl

        // This ordering of smull/smlal/smull2/smlal2 is highly
        // beneficial for Cortex A53 here.
        smull           v16.4s,  v24.4h,  v0.4h
        smlal           v16.4s,  v25.4h,  v1.4h
        smlal           v16.4s,  v26.4h,  v2.4h
        smlal           v16.4s,  v27.4h,  v3.4h
        smlal           v16.4s,  v28.4h,  v4.4h
        smlal           v16.4s,  v29.4h,  v5.4h
        smlal           v16.4s,  v30.4h,  v6.4h
        smlal           v16.4s,  v31.4h,  v7.4h
        smull2          v17.4s,  v24.8h,  v0.8h
        smlal2          v17.4s,  v25.8h,  v1.8h
        smlal2          v17.4s,  v26.8h,  v2.8h
        smlal2          v17.4s,  v27.8h,  v3.8h
        smlal2          v17.4s,  v28.8h,  v4.8h
        smlal2          v17.4s,  v29.8h,  v5.8h
        smlal2          v17.4s,  v30.8h,  v6.8h
        smlal2          v17.4s,  v31.8h,  v7.8h

        mov             v24.16b, v25.16b
        mov             v25.16b, v26.16b
        sqrshrn         v16.4h,  v16.4s,  #\shift
        mov             v26.16b, v27.16b
        sqrshrn2        v16.8h,  v17.4s,  #\shift
        mov             v27.16b, v28.16b
        mov             v28.16b, v29.16b
        add             v16.8h,  v16.8h,  v23.8h
.ifb \t
        sqxtun          v16.8b,  v16.8h
.endif
        mov             v29.16b, v30.16b
        mov             v30.16b, v31.16b
        subs            w10, w10, #1
.ifnb \t
        st1             {v16.8h}, [x0], x1
.else
        st1             {v16.8b}, [x0], x1
.endif

        add             w6,  w6,  w4
        b.gt            1b

        ret             x15
endfunc
.endm

warp  , 11
warp t, 7

// void dav1d_emu_edge_8bpc_neon(
//         const intptr_t bw, const intptr_t bh,
//         const intptr_t iw, const intptr_t ih,
//         const intptr_t x, const intptr_t y,
//         pixel *dst, const ptrdiff_t dst_stride,
//         const pixel *ref, const ptrdiff_t ref_stride)
function emu_edge_8bpc_neon, export=1
        ldp             x8,  x9,  [sp]

        // ref += iclip(y, 0, ih - 1) * PXSTRIDE(ref_stride)
        // ref += iclip(x, 0, iw - 1)
        sub             x12, x3,  #1           // ih - 1
        cmp             x5,  x3
        sub             x13, x2,  #1           // iw - 1
        csel            x12, x12, x5,  ge      // min(y, ih - 1)
        cmp             x4,  x2
        bic             x12, x12, x12, asr #63 // max(min(y, ih - 1), 0)
        csel            x13, x13, x4,  ge      // min(x, iw - 1)
        bic             x13, x13, x13, asr #63 // max(min(x, iw - 1), 0)
        madd            x8,  x12, x9,  x8      // ref += iclip() * stride
        add             x8,  x8,  x13          // ref += iclip()

        // bottom_ext = iclip(y + bh - ih, 0, bh - 1)
        // top_ext = iclip(-y, 0, bh - 1)
        add             x10, x5,  x1           // y + bh
        neg             x5,  x5                // -y
        sub             x10, x10, x3           // y + bh - ih
        sub             x12, x1,  #1           // bh - 1
        cmp             x10, x1
        bic             x5,  x5,  x5,  asr #63 // max(-y, 0)
        csel            x10, x10, x12, lt      // min(y + bh - ih, bh-1)
        cmp             x5,  x1
        bic             x10, x10, x10, asr #63 // max(min(y + bh - ih, bh-1), 0)
        csel            x5,  x5,  x12, lt      // min(max(-y, 0), bh-1)

        // right_ext = iclip(x + bw - iw, 0, bw - 1)
        // left_ext = iclip(-x, 0, bw - 1)
        add             x11, x4,  x0           // x + bw
        neg             x4,  x4                // -x
        sub             x11, x11, x2           // x + bw - iw
        sub             x13, x0,  #1           // bw - 1
        cmp             x11, x0
        bic             x4,  x4,  x4,  asr #63 // max(-x, 0)
        csel            x11, x11, x13, lt      // min(x + bw - iw, bw-1)
        cmp             x4,  x0
        bic             x11, x11, x11, asr #63 // max(min(x + bw - iw, bw-1), 0)
        csel            x4,  x4,  x13, lt      // min(max(-x, 0), bw - 1)

        // center_h = bh - top_ext - bottom_ext
        // dst += top_ext * PXSTRIDE(dst_stride)
        // center_w = bw - left_ext - right_ext
        sub             x1,  x1,  x5           // bh - top_ext
        madd            x6,  x5,  x7,  x6
        sub             x2,  x0,  x4           // bw - left_ext
        sub             x1,  x1,  x10          // center_h = bh - top_ext - bottom_ext
        sub             x2,  x2,  x11          // center_w = bw - left_ext - right_ext

        mov             x14, x6                // backup of dst

.macro v_loop need_left, need_right
0:
.if \need_left
        ld1r            {v0.16b}, [x8]
        mov             x12, x6                // out = dst
        mov             x3,  x4
1:
        subs            x3,  x3,  #16
        st1             {v0.16b}, [x12], #16
        b.gt            1b
.endif
        mov             x13, x8
        add             x12, x6,  x4           // out = dst + left_ext
        mov             x3,  x2
1:
        ld1             {v0.16b, v1.16b}, [x13], #32
        subs            x3,  x3,  #32
        st1             {v0.16b, v1.16b}, [x12], #32
        b.gt            1b
.if \need_right
        add             x3,  x8,  x2           // in + center_w
        sub             x3,  x3,  #1           // in + center_w - 1
        add             x12, x6,  x4           // dst + left_ext
        ld1r            {v0.16b}, [x3]
        add             x12, x12, x2           // out = dst + left_ext + center_w
        mov             x3,  x11
1:
        subs            x3,  x3,  #16
        st1             {v0.16b}, [x12], #16
        b.gt            1b
.endif

        subs            x1,  x1,  #1           // center_h--
        add             x6,  x6,  x7
        add             x8,  x8,  x9
        b.gt            0b
.endm

        cbz             x4,  2f
        // need_left
        cbz             x11, 3f
        // need_left + need_right
        v_loop          1,   1
        b               5f

2:
        // !need_left
        cbz             x11, 4f
        // !need_left + need_right
        v_loop          0,   1
        b               5f

3:
        // need_left + !need_right
        v_loop          1,   0
        b               5f

4:
        // !need_left + !need_right
        v_loop          0,   0

5:

        cbz             x10, 3f
        // need_bottom
        sub             x8,  x6,  x7           // ref = dst - stride
        mov             x4,  x0
1:
        ld1             {v0.16b, v1.16b}, [x8], #32
        mov             x3,  x10
2:
        subs            x3,  x3,  #1
        st1             {v0.16b, v1.16b}, [x6], x7
        b.gt            2b
        msub            x6,  x7,  x10,  x6     // dst -= bottom_ext * stride
        subs            x4,  x4,  #32          // bw -= 32
        add             x6,  x6,  #32          // dst += 32
        b.gt            1b

3:
        cbz             x5,  3f
        // need_top
        msub            x6,  x7,  x5,  x14     // dst = stored_dst - top_ext * stride
1:
        ld1             {v0.16b, v1.16b}, [x14], #32
        mov             x3,  x5
2:
        subs            x3,  x3,  #1
        st1             {v0.16b, v1.16b}, [x6], x7
        b.gt            2b
        msub            x6,  x7,  x5,  x6      // dst -= top_ext * stride
        subs            x0,  x0,  #32          // bw -= 32
        add             x6,  x6,  #32          // dst += 32
        b.gt            1b

3:
        ret
endfunc

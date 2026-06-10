/*
 * Copyright © 2018, VideoLAN and dav1d authors
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

const right_ext_mask_buf
        .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
right_ext_mask:
        .byte 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        .byte 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        .byte 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        .byte 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        .byte 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        .byte 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
endconst

// void dav1d_wiener_filter7_8bpc_neon(pixel *p, const ptrdiff_t stride,
//                                     const pixel (*left)[4], const pixel *lpf,
//                                     const int w, int h,
//                                     const int16_t filter[2][8],
//                                     const enum LrEdgeFlags edges);
function wiener_filter7_8bpc_neon, export=1
        AARCH64_SIGN_LINK_REGISTER
        stp             x29, x30, [sp, #-16]!
        mov             x29, sp
        ld1             {v0.8h, v1.8h},  [x6]
        tst             w7,  #4               // LR_HAVE_TOP
        sub_sp          384*2*6

        mov             w17, #(1 << 14) - (1 << 2)
        dup             v30.8h,  w17
        movi            v31.8h,  #8, lsl #8

        // x9  - t6
        // x10 - t5
        // x11 - t4
        // x12 - t3
        // x13 - t2
        // x14 - t1
        // x15 - t0
        mov             x14, sp               // t1
        b.eq            L(no_top_7)

        mov             x16, x2               // backup left
        mov             x2,  #0
        bl              wiener_filter7_h_8bpc_neon
        add             x3,  x3,  x1          // lpf += stride
        mov             x9,  x14              // t6
        mov             x10, x14              // t5
        add             x14, x14, #384*2      // t1 += 384*2
        bl              wiener_filter7_h_8bpc_neon
        add             x3,  x3,  x1,  lsl #2
        add             x3,  x3,  x1          // lpf += stride*5
        mov             x11, x14              // t4
        add             x14, x14, #384*2      // t1 += 384*2
        mov             x2,  x16              // left
        mov             x16, x3               // backup lpf
        mov             x3,  x0               // lpf = p
        bl              wiener_filter7_h_8bpc_neon
        subs            w5,  w5,  #1          // h--
        mov             x12, x14              // t3
        mov             x13, x14              // t2
        b.eq            L(v1_7)
        add             x3,  x3,  x1          // src += stride
        add             x14, x14, #384*2      // t1 += 384*2
        bl              wiener_filter7_h_8bpc_neon
        mov             x13, x14              // t2
        subs            w5,  w5,  #1          // h--
        b.eq            L(v2_7)
        add             x3,  x3,  x1          // src += stride
        add             x14, x14, #384*2      // t1 += 384*2
        bl              wiener_filter7_h_8bpc_neon
        subs            w5,  w5,  #1          // h--
        b.eq            L(v3_7)
        add             x3,  x3,  x1          // src += stride

L(main_7):
        add             x15, x14, #384*2      // t0 = t1 + 384*2
L(main_loop_7):
        bl              wiener_filter7_hv_8bpc_neon
        subs            w5,  w5,  #1          // h--
        b.ne            L(main_loop_7)
        tst             w7,  #8 // LR_HAVE_BOTTOM
        b.eq            L(v3_7)

        mov             x3,  x16              // restore lpf
        mov             x2,  #0               // left = NULL
        bl              wiener_filter7_hv_8bpc_neon
        bl              wiener_filter7_hv_8bpc_neon
L(v1_7):
        bl              wiener_filter7_v_8bpc_neon

        mov             sp,  x29
        ldp             x29, x30, [sp], #16
        AARCH64_VALIDATE_LINK_REGISTER
        ret

L(no_top_7):
        add             x3,  x3,  x1,  lsl #2
        add             x16, x3,  x1,  lsl #1 // lpf += stride*6, backup
        mov             x3,  x0               // lpf = p

        bl              wiener_filter7_h_8bpc_neon
        subs            w5,  w5,  #1          // h--
        mov             x9,  x14              // t6
        mov             x10, x14              // t5
        mov             x11, x14              // t4
        mov             x12, x14              // t3
        mov             x13, x14              // t2
        b.eq            L(v1_7)
        add             x3,  x3,  x1          // src += stride
        add             x14, x14, #384*2      // t1 += 384*2
        bl              wiener_filter7_h_8bpc_neon
        subs            w5,  w5,  #1          // h--
        mov             x13, x14              // t2
        b.eq            L(v2_7)
        add             x3,  x3,  x1          // src += stride
        add             x14, x14, #384*2      // t1 += 384*2
        bl              wiener_filter7_h_8bpc_neon
        subs            w5,  w5,  #1          // h--
        b.eq            L(v3_7)
        add             x3,  x3,  x1          // src += stride
        add             x15, x14, #384*2      // t0 = t1 + 384*2
        bl              wiener_filter7_hv_8bpc_neon
        subs            w5,  w5,  #1          // h--
        b.eq            L(v3_7)
        add             x15, x15, #384*2*4    // t0 += 384*2*4
        bl              wiener_filter7_hv_8bpc_neon
        subs            w5,  w5,  #1          // h--
        b.ne            L(main_7)
L(v3_7):
        bl              wiener_filter7_v_8bpc_neon
L(v2_7):
        bl              wiener_filter7_v_8bpc_neon
        b               L(v1_7)
endfunc


function wiener_filter7_h_8bpc_neon
        stp             x3,  x4,  [sp, #-32]!
        str             x14,      [sp, #16]

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             w7,  #1 // LR_HAVE_LEFT
        b.eq            1f
        // LR_HAVE_LEFT
        cbnz            x2,  0f
        // left == NULL
        sub             x3,  x3,  #3
        ld1             {v3.16b}, [x3], #16
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        ld1             {v3.16b},  [x3], #16
        ld1             {v2.s}[3], [x2], #4
        // Move x3 back to account for the last 3 bytes we loaded earlier,
        // which we'll shift out.
        sub             x3,  x3,  #3
        ext             v3.16b,  v2.16b,  v3.16b, #13
        b               2f

1:
        ld1             {v3.16b}, [x3], #16
        // !LR_HAVE_LEFT, fill v2 with the leftmost byte
        // and shift v3 to have 3x the first byte at the front.
        dup             v2.16b,  v3.b[0]
        // Move x3 back to account for the last 3 bytes we loaded before,
        // which we shifted out.
        sub             x3,  x3,  #3
        ext             v3.16b,  v2.16b,  v3.16b, #13

2:
        ld1             {v4.8b}, [x3], #8
        uxtl            v2.8h,   v3.8b
        uxtl2           v3.8h,   v3.16b
        uxtl            v4.8h,   v4.8b

        tst             w7,  #2 // LR_HAVE_RIGHT
        b.ne            4f

3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             w4,  #19
        b.ge            4f   // If w >= 19, all used input pixels are valid

        // 1 <= w < 19, w+3 pixels valid in v2-v4. For w>=9,
        // this ends up called again; it's not strictly needed in those
        // cases (we pad enough here), but keeping the code as simple as possible.

        // The padding pixel is v2/3/4.h[w+2]. x3 points at the next input, ie
        // v2/3/4.h[24]. Thus read from x3[w-22] to find the padding pixel.
        sub             w17, w4,  #22
        // Insert padding in v2/3/4.h[w+3] onwards; fuse the +3 (*2) into the
        // buffer pointer.
        movrel          x6,  right_ext_mask, -6
        ldr             b28, [x3,  w17, sxtw]
        sub             x6,  x6,  w4,  uxtw #1
        dup             v28.8h,  v28.h[0]
        ld1             {v25.16b, v26.16b, v27.16b}, [x6]

        bit             v2.16b,  v28.16b, v25.16b
        bit             v3.16b,  v28.16b, v26.16b
        bit             v4.16b,  v28.16b, v27.16b

4:      // Loop horizontally
        // Interleaving the mul/mla chains actually hurts performance
        // significantly on Cortex A53, thus keeping mul/mla tightly
        // chained like this.
        ext             v17.16b, v2.16b,  v3.16b, #4
        ext             v19.16b, v2.16b,  v3.16b, #8
        ext             v16.16b, v2.16b,  v3.16b, #2
        ext             v20.16b, v2.16b,  v3.16b, #10
        ext             v21.16b, v2.16b,  v3.16b, #12
        ext             v18.16b, v2.16b,  v3.16b, #6
        add             v19.8h,  v19.8h,  v17.8h
        add             v20.8h,  v20.8h,  v16.8h
        add             v21.8h,  v21.8h,  v2.8h
        shl             v22.8h,  v18.8h,  #7
        mul             v6.8h,   v18.8h,  v0.h[3]
        mla             v6.8h,   v19.8h,  v0.h[4]
        mla             v6.8h,   v20.8h,  v0.h[5]
        mla             v6.8h,   v21.8h,  v0.h[6]

        ext             v17.16b, v3.16b,  v4.16b, #4
        ext             v19.16b, v3.16b,  v4.16b, #8
        ext             v16.16b, v3.16b,  v4.16b, #2
        ext             v20.16b, v3.16b,  v4.16b, #10
        ext             v21.16b, v3.16b,  v4.16b, #12
        ext             v18.16b, v3.16b,  v4.16b, #6

        add             v19.8h,  v19.8h,  v17.8h
        add             v20.8h,  v20.8h,  v16.8h
        add             v21.8h,  v21.8h,  v3.8h
        shl             v23.8h,  v18.8h,  #7
        mul             v7.8h,   v18.8h,  v0.h[3]
        mla             v7.8h,   v19.8h,  v0.h[4]
        mla             v7.8h,   v20.8h,  v0.h[5]
        mla             v7.8h,   v21.8h,  v0.h[6]

        sub             v22.8h,  v22.8h,  v30.8h
        sub             v23.8h,  v23.8h,  v30.8h
        sqadd           v6.8h,   v6.8h,   v22.8h
        sqadd           v7.8h,   v7.8h,   v23.8h
        sshr            v6.8h,   v6.8h,   #3
        sshr            v7.8h,   v7.8h,   #3
        add             v6.8h,   v6.8h,   v31.8h
        add             v7.8h,   v7.8h,   v31.8h

        subs            w4,  w4,  #16

        st1             {v6.8h, v7.8h}, [x14], #32

        b.le            0f
        mov             v2.16b,  v4.16b
        ld1             {v4.16b}, [x3], #16
        tst             w7,  #2 // LR_HAVE_RIGHT
        uxtl            v3.8h,   v4.8b
        uxtl2           v4.8h,   v4.16b
        b.ne            4b // If we don't need to pad, just keep filtering.
        b               3b // If we need to pad, check how many pixels we have left.

0:
        ldr             x14,      [sp, #16]
        ldp             x3,  x4,  [sp], #32
        ret
endfunc

function wiener_filter7_v_8bpc_neon
        // Backing up/restoring registers shifted, so that x9 gets the value
        // of x10, etc, afterwards.
        stp             x10, x11, [sp, #-64]!
        stp             x12, x13, [sp, #16]
        stp             x14, x14, [sp, #32]
        stp             x0,  x4,  [sp, #48]
1:
        ld1             {v20.8h, v21.8h}, [x11], #32
        ld1             {v24.8h, v25.8h}, [x13], #32

        ld1             {v18.8h, v19.8h}, [x10], #32
        add             v24.8h,  v24.8h,  v20.8h
        ld1             {v26.8h, v27.8h}, [x14], #32

        ld1             {v16.8h, v17.8h}, [x9],  #32
        add             v28.8h,  v26.8h,  v18.8h
        ld1             {v22.8h, v23.8h}, [x12], #32

        add             v16.8h,  v26.8h,  v16.8h
        add             v25.8h,  v25.8h,  v21.8h

        smull           v2.4s,   v22.4h,  v1.h[3]
        smlal           v2.4s,   v24.4h,  v1.h[4]
        smlal           v2.4s,   v28.4h,  v1.h[5]
        smlal           v2.4s,   v16.4h,  v1.h[6]
        add             v29.8h,  v27.8h,  v19.8h
        smull2          v3.4s,   v22.8h,  v1.h[3]
        smlal2          v3.4s,   v24.8h,  v1.h[4]
        smlal2          v3.4s,   v28.8h,  v1.h[5]
        smlal2          v3.4s,   v16.8h,  v1.h[6]
        add             v17.8h,  v27.8h,  v17.8h
        smull           v4.4s,   v23.4h,  v1.h[3]
        smlal           v4.4s,   v25.4h,  v1.h[4]
        smlal           v4.4s,   v29.4h,  v1.h[5]
        smlal           v4.4s,   v17.4h,  v1.h[6]
        smull2          v5.4s,   v23.8h,  v1.h[3]
        smlal2          v5.4s,   v25.8h,  v1.h[4]
        smlal2          v5.4s,   v29.8h,  v1.h[5]
        smlal2          v5.4s,   v17.8h,  v1.h[6]
        sqrshrun        v2.4h,   v2.4s,   #11
        sqrshrun2       v2.8h,   v3.4s,   #11
        sqrshrun        v3.4h,   v4.4s,   #11
        sqrshrun2       v3.8h,   v5.4s,   #11
        sqxtun          v2.8b,   v2.8h
        sqxtun2         v2.16b,  v3.8h
        subs            w4,  w4,  #16
        st1             {v2.16b}, [x0], #16
        b.gt            1b

        ldp             x0,  x4,  [sp, #48]
        ldp             x13, x14, [sp, #32]
        ldp             x11, x12, [sp, #16]
        ldp             x9,  x10, [sp], #64

        add             x0,  x0,  x1
        ret
endfunc

function wiener_filter7_hv_8bpc_neon
        // Backing up/restoring registers shifted, so that x9 gets the value
        // of x10, etc, and x15==x9, afterwards.
        stp             x10, x11, [sp, #-80]!
        stp             x12, x13, [sp, #16]
        stp             x14, x15, [sp, #32]
        stp             x10, x0,  [sp, #48]
        stp             x3,  x4,  [sp, #64]

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             w7,  #1 // LR_HAVE_LEFT
        b.eq            1f
        // LR_HAVE_LEFT
        cbnz            x2,  0f
        // left == NULL
        sub             x3,  x3,  #3
        ld1             {v3.16b}, [x3], #16
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        ld1             {v3.16b},  [x3], #16
        ld1             {v2.s}[3], [x2], #4
        // Move x3 back to account for the last 3 bytes we loaded earlier,
        // which we'll shift out.
        sub             x3,  x3,  #3
        ext             v3.16b,  v2.16b,  v3.16b, #13
        b               2f
1:
        ld1             {v3.16b}, [x3], #16
        // !LR_HAVE_LEFT, fill v2 with the leftmost byte
        // and shift v3 to have 3x the first byte at the front.
        dup             v2.16b,  v3.b[0]
        // Move x3 back to account for the last 3 bytes we loaded before,
        // which we shifted out.
        sub             x3,  x3,  #3
        ext             v3.16b,  v2.16b,  v3.16b, #13

2:
        ld1             {v4.8b}, [x3], #8
        uxtl            v2.8h,   v3.8b
        uxtl2           v3.8h,   v3.16b
        uxtl            v4.8h,   v4.8b

        tst             w7,  #2 // LR_HAVE_RIGHT
        b.ne            4f

3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             w4,  #19
        b.ge            4f   // If w >= 19, all used input pixels are valid

        // 1 <= w < 19, w+3 pixels valid in v2-v4. For w>=9,
        // this ends up called again; it's not strictly needed in those
        // cases (we pad enough here), but keeping the code as simple as possible.

        // The padding pixel is v2/3/4.h[w+2]. x3 points at the next input, ie
        // v2/3/4.h[24]. Thus read from x3[w-22] to find the padding pixel.
        sub             w17, w4,  #22
        // Insert padding in v2/3/4.h[w+3] onwards; fuse the +3 (*2) into the
        // buffer pointer.
        movrel          x6,  right_ext_mask, -6
        ldr             b28, [x3,  w17, sxtw]
        sub             x6,  x6,  w4,  uxtw #1
        dup             v28.8h,  v28.h[0]
        ld1             {v25.16b, v26.16b, v27.16b}, [x6]

        bit             v2.16b,  v28.16b, v25.16b
        bit             v3.16b,  v28.16b, v26.16b
        bit             v4.16b,  v28.16b, v27.16b

4:      // Loop horizontally
        ext             v17.16b, v2.16b,  v3.16b, #4
        ext             v19.16b, v2.16b,  v3.16b, #8
        ext             v16.16b, v2.16b,  v3.16b, #2
        ext             v20.16b, v2.16b,  v3.16b, #10
        ext             v21.16b, v2.16b,  v3.16b, #12
        ext             v18.16b, v2.16b,  v3.16b, #6
        add             v19.8h,  v19.8h,  v17.8h
        add             v20.8h,  v20.8h,  v16.8h
        add             v21.8h,  v21.8h,  v2.8h
        shl             v22.8h,  v18.8h,  #7
        mul             v6.8h,   v18.8h,  v0.h[3]
        mla             v6.8h,   v19.8h,  v0.h[4]
        mla             v6.8h,   v20.8h,  v0.h[5]
        mla             v6.8h,   v21.8h,  v0.h[6]

        ext             v17.16b, v3.16b,  v4.16b, #4
        ext             v19.16b, v3.16b,  v4.16b, #8
        ext             v16.16b, v3.16b,  v4.16b, #2
        ext             v20.16b, v3.16b,  v4.16b, #10
        ext             v21.16b, v3.16b,  v4.16b, #12
        ext             v18.16b, v3.16b,  v4.16b, #6

        add             v19.8h,  v19.8h,  v17.8h
        add             v20.8h,  v20.8h,  v16.8h
        add             v21.8h,  v21.8h,  v3.8h
        shl             v23.8h,  v18.8h,  #7
        mul             v7.8h,   v18.8h,  v0.h[3]
        mla             v7.8h,   v19.8h,  v0.h[4]
        mla             v7.8h,   v20.8h,  v0.h[5]
        mla             v7.8h,   v21.8h,  v0.h[6]

        ld1             {v20.8h, v21.8h}, [x11], #32

        sub             v22.8h,  v22.8h,  v30.8h
        sub             v23.8h,  v23.8h,  v30.8h
        ld1             {v26.8h, v27.8h}, [x13], #32
        sqadd           v6.8h,   v6.8h,   v22.8h
        sqadd           v7.8h,   v7.8h,   v23.8h
        ld1             {v18.8h, v19.8h}, [x10], #32
        sshr            v6.8h,   v6.8h,   #3
        sshr            v7.8h,   v7.8h,   #3
        ld1             {v28.8h, v29.8h}, [x14], #32
        add             v6.8h,   v6.8h,   v31.8h
        add             v7.8h,   v7.8h,   v31.8h

        ld1             {v16.8h, v17.8h}, [x9],  #32
        add             v26.8h,  v20.8h,  v26.8h

        ld1             {v24.8h, v25.8h}, [x12], #32
        add             v28.8h,  v18.8h,  v28.8h

        add             v16.8h,  v16.8h,  v6.8h
        add             v27.8h,  v21.8h,  v27.8h

        smull           v18.4s,  v24.4h,  v1.h[3]
        smlal           v18.4s,  v26.4h,  v1.h[4]
        smlal           v18.4s,  v28.4h,  v1.h[5]
        smlal           v18.4s,  v16.4h,  v1.h[6]
        add             v29.8h,  v19.8h,  v29.8h
        smull2          v19.4s,  v24.8h,  v1.h[3]
        smlal2          v19.4s,  v26.8h,  v1.h[4]
        smlal2          v19.4s,  v28.8h,  v1.h[5]
        smlal2          v19.4s,  v16.8h,  v1.h[6]
        add             v17.8h,  v17.8h,  v7.8h
        smull           v20.4s,  v25.4h,  v1.h[3]
        smlal           v20.4s,  v27.4h,  v1.h[4]
        smlal           v20.4s,  v29.4h,  v1.h[5]
        smlal           v20.4s,  v17.4h,  v1.h[6]
        smull2          v21.4s,  v25.8h,  v1.h[3]
        smlal2          v21.4s,  v27.8h,  v1.h[4]
        smlal2          v21.4s,  v29.8h,  v1.h[5]
        smlal2          v21.4s,  v17.8h,  v1.h[6]
        sqrshrun        v18.4h,  v18.4s,  #11
        sqrshrun2       v18.8h,  v19.4s,  #11
        sqrshrun        v19.4h,  v20.4s,  #11
        sqrshrun2       v19.8h,  v21.4s,  #11
        st1             {v6.8h, v7.8h}, [x15], #32
        sqxtun          v18.8b,  v18.8h
        sqxtun2         v18.16b, v19.8h
        subs            w4,  w4,  #16

        st1             {v18.16b}, [x0], #16

        b.le            0f
        mov             v2.16b,  v4.16b
        ld1             {v4.16b}, [x3], #16
        tst             w7,  #2 // LR_HAVE_RIGHT
        uxtl            v3.8h,   v4.8b
        uxtl2           v4.8h,   v4.16b
        b.ne            4b // If we don't need to pad, just keep filtering.
        b               3b // If we need to pad, check how many pixels we have left.

0:
        ldp             x3,  x4,  [sp, #64]
        ldp             x15, x0,  [sp, #48]
        ldp             x13, x14, [sp, #32]
        ldp             x11, x12, [sp, #16]
        ldp             x9,  x10, [sp], #80

        add             x3,  x3,  x1
        add             x0,  x0,  x1

        ret
endfunc

// void dav1d_wiener_filter5_8bpc_neon(pixel *p, const ptrdiff_t stride,
//                                     const pixel (*left)[4], const pixel *lpf,
//                                     const int w, int h,
//                                     const int16_t filter[2][8],
//                                     const enum LrEdgeFlags edges);
function wiener_filter5_8bpc_neon, export=1
        AARCH64_SIGN_LINK_REGISTER
        stp             x29, x30, [sp, #-16]!
        mov             x29, sp
        ld1             {v0.8h, v1.8h},  [x6]
        tst             w7,  #4               // LR_HAVE_TOP
        sub_sp          384*2*4

        mov             w17, #(1 << 14) - (1 << 2)
        dup             v30.8h,  w17
        movi            v31.8h,  #8, lsl #8

        // x11 - t4
        // x12 - t3
        // x13 - t2
        // x14 - t1
        // x15 - t0
        mov             x14, sp               // t1
        b.eq            L(no_top_5)

        mov             x16, x2               // backup left
        mov             x2,  #0
        bl              wiener_filter5_h_8bpc_neon
        add             x3,  x3,  x1          // lpf += stride
        mov             x11, x14              // t4
        add             x14, x14, #384*2      // t1 += 384*2
        bl              wiener_filter5_h_8bpc_neon
        add             x3,  x3,  x1,  lsl #2
        add             x3,  x3,  x1          // lpf += stride*5
        mov             x12, x14              // t3
        add             x14, x14, #384*2      // t1 += 384*2
        mov             x2,  x16              // left
        mov             x16, x3               // backup lpf
        mov             x3,  x0               // lpf = p
        bl              wiener_filter5_h_8bpc_neon
        subs            w5,  w5,  #1          // h--
        mov             x13, x14              // t2
        b.eq            L(v1_5)
        add             x3,  x3,  x1          // src += stride
        add             x14, x14, #384*2      // t1 += 384*2
        bl              wiener_filter5_h_8bpc_neon
        subs            w5,  w5,  #1          // h--
        b.eq            L(v2_5)
        add             x3,  x3,  x1          // src += stride

L(main_5):
        mov             x15, x11              // t0 = t4
L(main_loop_5):
        bl              wiener_filter5_hv_8bpc_neon
        subs            w5,  w5,  #1          // h--
        b.ne            L(main_loop_5)
        tst             w7,  #8 // LR_HAVE_BOTTOM
        b.eq            L(v2_5)

        mov             x3,  x16              // restore lpf
        mov             x2,  #0               // left = NULL
        bl              wiener_filter5_hv_8bpc_neon
        bl              wiener_filter5_hv_8bpc_neon
L(end_5):

        mov             sp,  x29
        ldp             x29, x30, [sp], #16
        AARCH64_VALIDATE_LINK_REGISTER
        ret

L(no_top_5):
        add             x3,  x3,  x1,  lsl #2
        add             x16, x3,  x1,  lsl #1 // lpf += stride*6, backup
        mov             x3,  x0               // lpf = p

        bl              wiener_filter5_h_8bpc_neon
        subs            w5,  w5,  #1          // h--
        mov             x11, x14              // t4
        mov             x12, x14              // t3
        mov             x13, x14              // t2
        b.eq            L(v1_5)
        add             x3,  x3,  x1          // src += stride
        add             x14, x14, #384*2      // t1 += 384*2
        bl              wiener_filter5_h_8bpc_neon
        subs            w5,  w5,  #1          // h--
        b.eq            L(v2_5)
        add             x3,  x3,  x1          // src += stride
        add             x15, x14, #384*2      // t0 = t1 + 384*2
        bl              wiener_filter5_hv_8bpc_neon
        subs            w5,  w5,  #1          // h--
        b.eq            L(v2_5)
        add             x15, x15, #384*2*3    // t0 += 384*2*3
        bl              wiener_filter5_hv_8bpc_neon
        subs            w5,  w5,  #1          // h--
        b.ne            L(main_5)
L(v2_5):
        bl              wiener_filter5_v_8bpc_neon
        add             x0,  x0,  x1
        mov             x11, x12
        mov             x12, x13
        mov             x13, x14
L(v1_5):
        bl              wiener_filter5_v_8bpc_neon
        b               L(end_5)
endfunc


function wiener_filter5_h_8bpc_neon
        stp             x3,  x4,  [sp, #-32]!
        str             x14,      [sp, #16]

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             w7,  #1 // LR_HAVE_LEFT
        b.eq            1f
        // LR_HAVE_LEFT
        cbnz            x2,  0f
        // left == NULL
        sub             x3,  x3,  #2
        ld1             {v3.16b}, [x3], #16
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        ld1             {v3.16b},  [x3], #16
        ld1             {v2.s}[3], [x2], #4
        // Move x3 back to account for the last 2 bytes we loaded earlier,
        // which we'll shift out.
        sub             x3,  x3,  #2
        ext             v3.16b,  v2.16b,  v3.16b, #14
        b               2f

1:
        ld1             {v3.16b}, [x3], #16
        // !LR_HAVE_LEFT, fill v2 with the leftmost byte
        // and shift v3 to have 3x the first byte at the front.
        dup             v2.16b,  v3.b[0]
        // Move x3 back to account for the last 2 bytes we loaded before,
        // which we shifted out.
        sub             x3,  x3,  #2
        ext             v3.16b,  v2.16b,  v3.16b, #14

2:
        ld1             {v4.8b}, [x3], #8
        uxtl            v2.8h,   v3.8b
        uxtl2           v3.8h,   v3.16b
        uxtl            v4.8h,   v4.8b

        tst             w7,  #2 // LR_HAVE_RIGHT
        b.ne            4f

3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             w4,  #18
        b.ge            4f   // If w >= 18, all used input pixels are valid

        // 1 <= w < 18, w+2 pixels valid in v2-v4. For w>=9,
        // this ends up called again; it's not strictly needed in those
        // cases (we pad enough here), but keeping the code as simple as possible.

        // The padding pixel is v2/3/4.h[w+2]. x3 points at the next input, ie
        // v2/3/4.h[24]. Thus read from x3[w-23] to find the padding pixel.
        sub             w17, w4,  #23
        // Insert padding in v2/3/4.h[w+2] onwards; fuse the +2 (*2) into the
        // buffer pointer.
        movrel          x6,  right_ext_mask, -4
        ldr             b28, [x3,  w17, sxtw]
        sub             x6,  x6,  w4,  uxtw #1
        dup             v28.8h,  v28.h[0]
        ld1             {v25.16b, v26.16b, v27.16b}, [x6]

        bit             v2.16b,  v28.16b, v25.16b
        bit             v3.16b,  v28.16b, v26.16b
        bit             v4.16b,  v28.16b, v27.16b

4:      // Loop horizontally
        // Interleaving the mul/mla chains actually hurts performance
        // significantly on Cortex A53, thus keeping mul/mla tightly
        // chained like this.
        ext             v16.16b, v2.16b,  v3.16b, #2
        ext             v18.16b, v2.16b,  v3.16b, #6
        ext             v19.16b, v2.16b,  v3.16b, #8
        ext             v17.16b, v2.16b,  v3.16b, #4
        add             v18.8h,  v18.8h,  v16.8h
        add             v19.8h,  v19.8h,  v2.8h
        shl             v22.8h,  v17.8h,  #7
        mul             v6.8h,   v17.8h,  v0.h[3]
        mla             v6.8h,   v18.8h,  v0.h[4]
        mla             v6.8h,   v19.8h,  v0.h[5]

        ext             v16.16b, v3.16b,  v4.16b, #2
        ext             v18.16b, v3.16b,  v4.16b, #6
        ext             v19.16b, v3.16b,  v4.16b, #8
        ext             v17.16b, v3.16b,  v4.16b, #4
        add             v18.8h,  v18.8h,  v16.8h
        add             v19.8h,  v19.8h,  v3.8h
        shl             v23.8h,  v17.8h,  #7
        mul             v7.8h,   v17.8h,  v0.h[3]
        mla             v7.8h,   v18.8h,  v0.h[4]
        mla             v7.8h,   v19.8h,  v0.h[5]

        sub             v22.8h,  v22.8h,  v30.8h
        sub             v23.8h,  v23.8h,  v30.8h
        sqadd           v6.8h,   v6.8h,   v22.8h
        sqadd           v7.8h,   v7.8h,   v23.8h
        sshr            v6.8h,   v6.8h,   #3
        sshr            v7.8h,   v7.8h,   #3
        add             v6.8h,   v6.8h,   v31.8h
        add             v7.8h,   v7.8h,   v31.8h

        subs            w4,  w4,  #16

        st1             {v6.8h, v7.8h}, [x14], #32

        b.le            0f
        mov             v2.16b,  v4.16b
        ld1             {v4.16b}, [x3], #16
        tst             w7,  #2 // LR_HAVE_RIGHT
        uxtl            v3.8h,   v4.8b
        uxtl2           v4.8h,   v4.16b
        b.ne            4b // If we don't need to pad, just keep filtering.
        b               3b // If we need to pad, check how many pixels we have left.

0:
        ldr             x14,      [sp, #16]
        ldp             x3,  x4,  [sp], #32
        ret
endfunc

function wiener_filter5_v_8bpc_neon
        stp             x11, x12, [sp, #-48]!
        stp             x13, x14, [sp, #16]
        stp             x0,  x4,  [sp, #32]
1:
        ld1             {v18.8h, v19.8h}, [x12], #32
        ld1             {v22.8h, v23.8h}, [x14], #32
        ld1             {v16.8h, v17.8h}, [x11], #32

        add             v24.8h,  v22.8h,  v18.8h
        ld1             {v20.8h, v21.8h}, [x13], #32
        add             v16.8h,  v22.8h,  v16.8h
        add             v25.8h,  v23.8h,  v19.8h

        smull           v2.4s,   v20.4h,  v1.h[3]
        smlal           v2.4s,   v24.4h,  v1.h[4]
        smlal           v2.4s,   v16.4h,  v1.h[5]
        add             v17.8h,  v23.8h,  v17.8h
        smull2          v3.4s,   v20.8h,  v1.h[3]
        smlal2          v3.4s,   v24.8h,  v1.h[4]
        smlal2          v3.4s,   v16.8h,  v1.h[5]
        smull           v4.4s,   v21.4h,  v1.h[3]
        smlal           v4.4s,   v25.4h,  v1.h[4]
        smlal           v4.4s,   v17.4h,  v1.h[5]
        smull2          v5.4s,   v21.8h,  v1.h[3]
        smlal2          v5.4s,   v25.8h,  v1.h[4]
        smlal2          v5.4s,   v17.8h,  v1.h[5]
        sqrshrun        v2.4h,   v2.4s,   #11
        sqrshrun2       v2.8h,   v3.4s,   #11
        sqrshrun        v3.4h,   v4.4s,   #11
        sqrshrun2       v3.8h,   v5.4s,   #11
        sqxtun          v2.8b,   v2.8h
        sqxtun2         v2.16b,  v3.8h
        subs            w4,  w4,  #16
        st1             {v2.16b}, [x0], #16
        b.gt            1b

        ldp             x0,  x4,  [sp, #32]
        ldp             x13, x14, [sp, #16]
        ldp             x11, x12, [sp], #48

        ret
endfunc

function wiener_filter5_hv_8bpc_neon
        // Backing up/restoring registers shifted, so that x11 gets the value
        // of x12, etc, and x15==x11, afterwards.
        stp             x12, x13, [sp, #-64]!
        stp             x14, x15, [sp, #16]
        stp             x12, x0,  [sp, #32]
        stp             x3,  x4,  [sp, #48]

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             w7,  #1 // LR_HAVE_LEFT
        b.eq            1f
        // LR_HAVE_LEFT
        cbnz            x2,  0f
        // left == NULL
        sub             x3,  x3,  #2
        ld1             {v3.16b}, [x3], #16
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        ld1             {v3.16b},  [x3], #16
        ld1             {v2.s}[3], [x2], #4
        // Move x3 back to account for the last 2 bytes we loaded earlier,
        // which we'll shift out.
        sub             x3,  x3,  #2
        ext             v3.16b,  v2.16b,  v3.16b, #14
        b               2f
1:
        ld1             {v3.16b}, [x3], #16
        // !LR_HAVE_LEFT, fill v2 with the leftmost byte
        // and shift v3 to have 2x the first byte at the front.
        dup             v2.16b,  v3.b[0]
        // Move x3 back to account for the last 2 bytes we loaded before,
        // which we shifted out.
        sub             x3,  x3,  #2
        ext             v3.16b, v2.16b, v3.16b, #14

2:
        ld1             {v4.8b}, [x3], #8
        uxtl            v2.8h,  v3.8b
        uxtl2           v3.8h,  v3.16b
        uxtl            v4.8h,  v4.8b

        tst             w7,  #2 // LR_HAVE_RIGHT
        b.ne            4f

3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             w4,  #18
        b.ge            4f   // If w >= 18, all used input pixels are valid

        // 1 <= w < 18, w+2 pixels valid in v2-v4. For w>=9,
        // this ends up called again; it's not strictly needed in those
        // cases (we pad enough here), but keeping the code as simple as possible.

        // The padding pixel is v2/3/4.h[w+1]. x3 points at the next input, ie
        // v2/3/4.h[24]. Thus read from x3[w-23] to find the padding pixel.
        sub             w17, w4,  #23
        // Insert padding in v2/3/4.h[w+2] onwards; fuse the +2 (*2) into the
        // buffer pointer.
        movrel          x6,  right_ext_mask, -4
        ldr             b28, [x3,  w17, sxtw]
        sub             x6,  x6,  w4,  uxtw #1
        dup             v28.8h,  v28.h[0]
        ld1             {v25.16b, v26.16b, v27.16b}, [x6]

        bit             v2.16b,  v28.16b, v25.16b
        bit             v3.16b,  v28.16b, v26.16b
        bit             v4.16b,  v28.16b, v27.16b

4:      // Loop horizontally

        ext             v16.16b, v2.16b,  v3.16b, #2
        ext             v18.16b, v2.16b,  v3.16b, #6
        ext             v19.16b, v2.16b,  v3.16b, #8
        ext             v17.16b, v2.16b,  v3.16b, #4
        add             v18.8h,  v18.8h,  v16.8h
        add             v19.8h,  v19.8h,  v2.8h
        shl             v22.8h,  v17.8h,  #7
        mul             v6.8h,   v17.8h,  v0.h[3]
        mla             v6.8h,   v18.8h,  v0.h[4]
        mla             v6.8h,   v19.8h,  v0.h[5]

        ext             v16.16b, v3.16b,  v4.16b, #2
        ext             v18.16b, v3.16b,  v4.16b, #6
        ext             v19.16b, v3.16b,  v4.16b, #8
        ext             v17.16b, v3.16b,  v4.16b, #4
        add             v18.8h,  v18.8h,  v16.8h
        add             v19.8h,  v19.8h,  v3.8h
        shl             v23.8h,  v17.8h,  #7
        mul             v7.8h,   v17.8h,  v0.h[3]
        mla             v7.8h,   v18.8h,  v0.h[4]
        mla             v7.8h,   v19.8h,  v0.h[5]

        ld1             {v18.8h, v19.8h}, [x12], #32

        sub             v22.8h,  v22.8h,  v30.8h
        sub             v23.8h,  v23.8h,  v30.8h
        ld1             {v24.8h, v25.8h}, [x14], #32
        sqadd           v6.8h,   v6.8h,   v22.8h
        sqadd           v7.8h,   v7.8h,   v23.8h
        ld1             {v16.8h, v17.8h}, [x11], #32
        sshr            v6.8h,   v6.8h,   #3
        sshr            v7.8h,   v7.8h,   #3
        ld1             {v20.8h, v21.8h}, [x13], #32
        add             v6.8h,   v6.8h,   v31.8h
        add             v7.8h,   v7.8h,   v31.8h

        add             v24.8h,  v24.8h,  v18.8h
        add             v16.8h,  v16.8h,  v6.8h

        smull           v18.4s,  v20.4h,  v1.h[3]
        smlal           v18.4s,  v24.4h,  v1.h[4]
        smlal           v18.4s,  v16.4h,  v1.h[5]
        add             v25.8h,  v25.8h,  v19.8h
        smull2          v19.4s,  v20.8h,  v1.h[3]
        smlal2          v19.4s,  v24.8h,  v1.h[4]
        smlal2          v19.4s,  v16.8h,  v1.h[5]
        add             v17.8h,  v17.8h,  v7.8h
        smull           v20.4s,  v21.4h,  v1.h[3]
        smlal           v20.4s,  v25.4h,  v1.h[4]
        smlal           v20.4s,  v17.4h,  v1.h[5]
        smull2          v21.4s,  v21.8h,  v1.h[3]
        smlal2          v21.4s,  v25.8h,  v1.h[4]
        smlal2          v21.4s,  v17.8h,  v1.h[5]
        sqrshrun        v18.4h,  v18.4s,  #11
        sqrshrun2       v18.8h,  v19.4s,  #11
        sqrshrun        v19.4h,  v20.4s,  #11
        sqrshrun2       v19.8h,  v21.4s,  #11
        st1             {v6.8h, v7.8h}, [x15], #32
        sqxtun          v18.8b,  v18.8h
        sqxtun2         v18.16b, v19.8h
        subs            w4,  w4,  #16

        st1             {v18.16b}, [x0], #16

        b.le            0f
        mov             v2.16b,  v4.16b
        ld1             {v4.16b}, [x3], #16
        tst             w7,  #2 // LR_HAVE_RIGHT
        uxtl            v3.8h,   v4.8b
        uxtl2           v4.8h,   v4.16b
        b.ne            4b // If we don't need to pad, just keep filtering.
        b               3b // If we need to pad, check how many pixels we have left.

0:
        ldp             x3,  x4,  [sp, #48]
        ldp             x15, x0,  [sp, #32]
        ldp             x13, x14, [sp, #16]
        ldp             x11, x12, [sp], #64

        add             x3,  x3,  x1
        add             x0,  x0,  x1

        ret
endfunc

#include "looprestoration_tmpl.S"

// void dav1d_sgr_box3_row_h_8bpc_neon(int32_t *sumsq, int16_t *sum,
//                                     const pixel (*left)[4],
//                                     const pixel *src, const int w,
//                                     const enum LrEdgeFlags edges);
function sgr_box3_row_h_8bpc_neon, export=1
        add             w4,  w4,  #2 // w += 2

        tst             w5,  #1 // LR_HAVE_LEFT
        b.eq            1f
        cbnz            x2,  0f

        // LR_HAVE_LEFT && left == NULL
        sub             x3,  x3,  #2
        ld1             {v0.16b}, [x3], #16
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        ld1             {v0.16b},  [x3], #16
        ld1             {v1.s}[3], [x2]
        // Move x3 back to account for the last 2 bytes we loaded earlier,
        // which we'll shift out.
        sub             x3,  x3,  #2
        ext             v0.16b, v1.16b, v0.16b, #14
        b               2f

1:
        ld1             {v0.16b}, [x3], #16
        // !LR_HAVE_LEFT, fill v1 with the leftmost byte
        // and shift v0 to have 2x the first byte at the front.
        dup             v1.16b, v0.b[0]
        // Move x3 back to account for the last 2 bytes we loaded before,
        // which we shifted out.
        sub             x3,  x3,  #2
        ext             v0.16b, v1.16b, v0.16b, #14

2:
        umull           v1.8h,   v0.8b,   v0.8b
        umull2          v2.8h,   v0.16b,  v0.16b

        tst             w5,  #2 // LR_HAVE_RIGHT
        b.ne            4f
        // If we'll need to pad the right edge, load that byte to pad with
        // here since we can find it pretty easily from here.
        sub             w13, w4, #(2 + 16 - 2 + 1)
        ldr             b30, [x3,  w13, sxtw]
        // Fill v30 with the right padding pixel
        dup             v30.16b, v30.b[0]
3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             w4,  #10
        b.ge            4f   // If w >= 10, all used input pixels are valid

        // 1 <= w < 10, w pixels valid in v0. For w=9, this ends up called
        // again; it's not strictly needed in those cases (we pad enough here),
        // but keeping the code as simple as possible.

        // Insert padding in v0.b[w] onwards
        movrel          x13, right_ext_mask
        sub             x13, x13, w4,  uxtw
        ld1             {v29.16b}, [x13]

        bit             v0.16b,  v30.16b, v29.16b

        // Update the precalculated squares
        umull           v1.8h,   v0.8b,   v0.8b
        umull2          v2.8h,   v0.16b,  v0.16b

4:      // Loop horizontally
        ext             v16.16b, v0.16b,  v0.16b, #1
        ext             v17.16b, v0.16b,  v0.16b, #2
        uaddl           v3.8h,   v0.8b,   v16.8b
        ext             v20.16b, v1.16b,  v2.16b, #2
        uaddw           v3.8h,   v3.8h,   v17.8b

        ext             v21.16b, v1.16b,  v2.16b, #4

        uaddl           v26.4s,  v1.4h,   v20.4h
        uaddl2          v27.4s,  v1.8h,   v20.8h
        uaddw           v26.4s,  v26.4s,  v21.4h
        uaddw2          v27.4s,  v27.4s,  v21.8h

        subs            w4,  w4,  #8

        st1             {v3.8h},         [x1],  #16
        st1             {v26.4s,v27.4s}, [x0],  #32

        b.le            9f
        tst             w5,  #2 // LR_HAVE_RIGHT
        ld1             {v3.8b},  [x3],  #8
        mov             v1.16b,  v2.16b
        ext             v0.16b,  v0.16b,  v3.16b, #8
        umull           v2.8h,   v3.8b,   v3.8b

        b.ne            4b // If we don't need to pad, just keep summing.
        b               3b // If we need to pad, check how many pixels we have left.

9:
        ret
endfunc

// void dav1d_sgr_box5_row_h_8bpc_neon(int32_t *sumsq, int16_t *sum,
//                                     const pixel (*left)[4],
//                                     const pixel *src, const int w,
//                                     const enum LrEdgeFlags edges);
function sgr_box5_row_h_8bpc_neon, export=1
        add             w4,  w4,  #2 // w += 2

        tst             w5,  #1 // LR_HAVE_LEFT
        b.eq            1f
        cbnz            x2,  0f

        // LR_HAVE_LEFT && left == NULL
        sub             x3,  x3,  #3
        ld1             {v0.16b}, [x3], #16
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        ld1             {v0.16b},  [x3], #16
        ld1             {v1.s}[3], [x2], #4
        // Move x3 back to account for the last 3 bytes we loaded earlier,
        // which we'll shift out.
        sub             x3,  x3,  #3
        ext             v0.16b, v1.16b, v0.16b, #13
        b               2f

1:
        ld1             {v0.16b}, [x3], #16
        // !LR_HAVE_LEFT, fill v1 with the leftmost byte
        // and shift v0 to have 3x the first byte at the front.
        dup             v1.16b, v0.b[0]
        // Move x3 back to account for the last 3 bytes we loaded before,
        // which we shifted out.
        sub             x3,  x3,  #3
        ext             v0.16b, v1.16b, v0.16b, #13

2:
        umull           v1.8h,   v0.8b,   v0.8b
        umull2          v2.8h,   v0.16b,  v0.16b

        tst             w5,  #2 // LR_HAVE_RIGHT
        b.ne            4f
        // If we'll need to pad the right edge, load that byte to pad with
        // here since we can find it pretty easily from here.
        sub             w13, w4, #(2 + 16 - 3 + 1)
        ldr             b30, [x3,  w13, sxtw]
        // Fill v30 with the right padding pixel
        dup             v30.16b, v30.b[0]
3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             w4,  #11
        b.ge            4f   // If w >= 11, all used input pixels are valid

        // 1 <= w < 11, w+1 pixels valid in v0. For w=9 or w=10,
        // this ends up called again; it's not strictly needed in those
        // cases (we pad enough here), but keeping the code as simple as possible.

        // Insert padding in v0.b[w+1] onwards; fuse the +1 into the
        // buffer pointer.
        movrel          x13, right_ext_mask, -1
        sub             x13, x13, w4,  uxtw
        ld1             {v29.16b}, [x13]

        bit             v0.16b,  v30.16b, v29.16b

        // Update the precalculated squares
        umull           v1.8h,   v0.8b,   v0.8b
        umull2          v2.8h,   v0.16b,  v0.16b

4:      // Loop horizontally
        ext             v16.16b, v0.16b,  v0.16b, #1
        ext             v17.16b, v0.16b,  v0.16b, #2
        ext             v18.16b, v0.16b,  v0.16b, #3
        ext             v19.16b, v0.16b,  v0.16b, #4
        uaddl           v3.8h,   v0.8b,   v16.8b
        uaddl           v24.8h,  v17.8b,  v18.8b
        uaddw           v3.8h,   v3.8h,   v19.8b
        add             v3.8h,   v3.8h,   v24.8h

        ext             v16.16b, v1.16b,  v2.16b, #2
        ext             v17.16b, v1.16b,  v2.16b, #4
        ext             v18.16b, v1.16b,  v2.16b, #6
        ext             v19.16b, v1.16b,  v2.16b, #8

        uaddl           v26.4s,  v1.4h,   v16.4h
        uaddl2          v27.4s,  v1.8h,   v16.8h
        uaddl           v16.4s,  v17.4h,  v18.4h
        uaddl2          v17.4s,  v17.8h,  v18.8h
        uaddw           v26.4s,  v26.4s,  v19.4h
        uaddw2          v27.4s,  v27.4s,  v19.8h
        add             v26.4s,  v26.4s,  v16.4s
        add             v27.4s,  v27.4s,  v17.4s

        subs            w4,  w4,  #8

        st1             {v3.8h},         [x1],  #16
        st1             {v26.4s,v27.4s}, [x0],  #32

        b.le            9f
        tst             w5,  #2 // LR_HAVE_RIGHT
        ld1             {v3.8b},  [x3],  #8
        mov             v1.16b,  v2.16b
        ext             v0.16b,  v0.16b,  v3.16b, #8
        umull           v2.8h,   v3.8b,   v3.8b

        b.ne            4b // If we don't need to pad, just keep summing.
        b               3b // If we need to pad, check how many pixels we have left.

9:
        ret
endfunc

// void dav1d_sgr_box35_row_h_8bpc_neon(int32_t *sumsq3, int16_t *sum3,
//                                      int32_t *sumsq5, int16_t *sum5,
//                                      const pixel (*left)[4],
//                                      const pixel *src, const int w,
//                                      const enum LrEdgeFlags edges);
function sgr_box35_row_h_8bpc_neon, export=1
        add             w6,  w6,  #2 // w += 2

        tst             w7,  #1 // LR_HAVE_LEFT
        b.eq            1f
        cbnz            x4,  0f

        // LR_HAVE_LEFT && left == NULL
        sub             x5,  x5,  #3
        ld1             {v0.16b},  [x5], #16
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        ld1             {v0.16b},  [x5], #16
        ld1             {v1.s}[3], [x4], #4
        // Move x3 back to account for the last 3 bytes we loaded earlier,
        // which we'll shift out.
        sub             x5,  x5,  #3
        ext             v0.16b, v1.16b, v0.16b, #13
        b               2f

1:
        ld1             {v0.16b}, [x5], #16
        // !LR_HAVE_LEFT, fill v1 with the leftmost byte
        // and shift v0 to have 3x the first byte at the front.
        dup             v1.16b, v0.b[0]
        // Move x3 back to account for the last 3 bytes we loaded before,
        // which we shifted out.
        sub             x5,  x5,  #3
        ext             v0.16b, v1.16b, v0.16b, #13

2:
        umull           v1.8h,   v0.8b,   v0.8b
        umull2          v2.8h,   v0.16b,  v0.16b

        tst             w7,  #2 // LR_HAVE_RIGHT
        b.ne            4f
        // If we'll need to pad the right edge, load that byte to pad with
        // here since we can find it pretty easily from here.
        sub             w13, w6, #(2 + 16 - 3 + 1)
        ldr             b30, [x5,  w13, sxtw]
        // Fill v30 with the right padding pixel
        dup             v30.16b, v30.b[0]
3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             w6,  #11
        b.ge            4f   // If w >= 11, all used input pixels are valid

        // 1 <= w < 11, w+1 pixels valid in v0. For w=9 or w=10,
        // this ends up called again; it's not strictly needed in those
        // cases (we pad enough here), but keeping the code as simple as possible.

        // Insert padding in v0.b[w+1] onwards; fuse the +1 into the
        // buffer pointer.
        movrel          x13, right_ext_mask, -1
        sub             x13, x13, w6,  uxtw
        ld1             {v29.16b}, [x13]

        bit             v0.16b,  v30.16b, v29.16b

        // Update the precalculated squares
        umull           v1.8h,   v0.8b,   v0.8b
        umull2          v2.8h,   v0.16b,  v0.16b

4:      // Loop horizontally
        ext             v16.16b, v0.16b,  v0.16b, #1
        ext             v17.16b, v0.16b,  v0.16b, #2
        ext             v19.16b, v0.16b,  v0.16b, #4
        ext             v18.16b, v0.16b,  v0.16b, #3
        uaddl           v3.8h,   v16.8b,  v17.8b
        uaddl           v24.8h,  v0.8b,   v19.8b
        uaddw           v3.8h,   v3.8h,   v18.8b

        ext             v16.16b, v1.16b,  v2.16b, #2
        ext             v17.16b, v1.16b,  v2.16b, #4
        ext             v19.16b, v1.16b,  v2.16b, #8
        ext             v18.16b, v1.16b,  v2.16b, #6

        st1             {v3.8h},         [x1], #16
        add             v3.8h,   v3.8h,   v24.8h

        uaddl           v26.4s,  v16.4h,  v17.4h
        uaddl2          v27.4s,  v16.8h,  v17.8h
        uaddl           v16.4s,  v1.4h,   v19.4h
        uaddl2          v17.4s,  v1.8h,   v19.8h
        uaddw           v26.4s,  v26.4s,  v18.4h
        uaddw2          v27.4s,  v27.4s,  v18.8h

        st1             {v26.4s,v27.4s}, [x0], #32
        add             v26.4s,  v26.4s,  v16.4s
        add             v27.4s,  v27.4s,  v17.4s

        subs            w6,  w6,  #8

        st1             {v3.8h},         [x3], #16
        st1             {v26.4s,v27.4s}, [x2], #32

        b.le            9f
        tst             w7,  #2 // LR_HAVE_RIGHT
        ld1             {v3.8b},  [x5],  #8
        mov             v1.16b,  v2.16b
        ext             v0.16b,  v0.16b,  v3.16b, #8
        umull           v2.8h,   v3.8b,   v3.8b

        b.ne            4b // If we don't need to pad, just keep summing.
        b               3b // If we need to pad, check how many pixels we have left.

9:
        ret
endfunc

sgr_funcs 8

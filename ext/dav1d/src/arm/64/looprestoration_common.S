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

// Series of LUTs for efficiently computing sgr's 1 - x/(x+1) table.
// In the comments, let RefTable denote the original, reference table.
const x_by_x_tables
// RangeMins
//
// Min(RefTable[i*8:i*8+8])
// First two values are zeroed.
//
// Lookup using RangeMins[(x >> 3)]
        .byte 0,  0, 11,  8,  6,  5,  5,  4,  4,  3,  3,  3,  2,  2,  2,  2
        .byte 2,  2,  2,  2,  2,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0

// DiffMasks
//
// This contains a bit pattern, indicating at which index positions the value of RefTable changes. For each range
// in the RangeMins table (covering 8 RefTable entries), we have one byte; each bit indicates whether the value of
// RefTable changes at that particular index.
// Using popcount, we can integrate the diff bit field. By shifting away bits in a byte, we can refine the range of
// the integral. Finally, adding the integral to RangeMins[(x>>3)] reconstructs RefTable (for x > 15).
//
// Lookup using DiffMasks[(x >> 3)]
        .byte 0x00, 0x00, 0xD4, 0x44
        .byte 0x42, 0x04, 0x00, 0x00
        .byte 0x00, 0x80, 0x00, 0x00
        .byte 0x04, 0x00, 0x00, 0x00
        .byte 0x00, 0x00, 0x00, 0x00
        .byte 0x00, 0x40, 0x00, 0x00
        .byte 0x00, 0x00, 0x00, 0x00
        .byte 0x00, 0x00, 0x00, 0x02
// Binary form:
// 0b00000000, 0b00000000, 0b11010100, 0b01000100
// 0b01000010, 0b00000100, 0b00000000, 0b00000000
// 0b00000000, 0b10000000, 0b00000000, 0b00000000
// 0b00000100, 0b00000000, 0b00000000, 0b00000000
// 0b00000000, 0b00000000, 0b00000000, 0b00000000
// 0b00000000, 0b01000000, 0b00000000, 0b00000000
// 0b00000000, 0b00000000, 0b00000000, 0b00000000
// 0b00000000, 0b00000000, 0b00000000, 0b00000010

// RefLo
//
// RefTable[0:16]
//      i.e. First 16 elements of the original table.
// Add to the sum obtained in the rest of the other lut logic to include the first 16 bytes of RefTable.
//
// Lookup using RangeMins[x] (tbl will replace x > 15 with 0)
        .byte 255, 128,  85,  64,  51,  43,  37,  32, 28,  26,  23,  21,  20,  18,  17,  16

// Pseudo assembly
//
// hi_bits = x >> 3
// tbl             ref,    {RefLo}, x
// tbl             diffs,  {DiffMasks[0:16], DiffMasks[16:32]}, hi_bits
// tbl             min,    {RangeMins[0:16], RangeMins[16:32]}, hi_bits
// lo_bits = x & 0x7
// diffs = diffs << lo_bits
// ref = ref + min
// integral = popcnt(diffs)
// ref = ref + integral
// return ref
endconst

// void dav1d_sgr_box3_vert_neon(int32_t **sumsq, int16_t **sum,
//                               int32_t *AA, int16_t *BB,
//                               const int w, const int s,
//                               const int bitdepth_max);
function sgr_box3_vert_neon, export=1
        stp             d8,  d9,  [sp, #-0x40]!
        stp             d10, d11, [sp, #0x10]
        stp             d12, d13, [sp, #0x20]
        stp             d14, d15, [sp, #0x30]

        add             w4,  w4,  #2
        clz             w9,  w6        // bitdepth_max
        dup             v28.4s,   w5   // strength

        ldp             x5,  x6,  [x0]
        ldr             x0,       [x0, #16]
        ldp             x7,  x8,  [x1]
        ldr             x1,       [x1, #16]

        movi            v31.4s,   #9   // n

        sub             w9,  w9,  #24  // -bitdepth_min_8
        movrel          x12, x_by_x_tables
        mov             w13, #455      // one_by_x
        ld1             {v24.16b, v25.16b, v26.16b, v27.16b}, [x12] // RangeMins, DiffMasks
        movi            v22.16b, #0x7
        ldr             q23, [x12, #64] //RefLo
        dup             v6.8h,    w9   // -bitdepth_min_8
        saddl           v7.4s,    v6.4h,   v6.4h  // -2*bitdepth_min_8
        dup             v30.4s,   w13  // one_by_x

        ld1             {v8.4s,  v9.4s,  v10.4s, v11.4s}, [x5], #64
        ld1             {v12.4s, v13.4s, v14.4s, v15.4s}, [x6], #64
        ld1             {v16.4s, v17.4s, v18.4s, v19.4s}, [x0], #64
        ld1             {v20.8h, v21.8h}, [x8], #32
        ld1             {v0.8h,  v1.8h},  [x7], #32
1:
        ld1             {v2.8h,  v3.8h},   [x1], #32
        add             v8.4s,   v8.4s,   v12.4s
        add             v9.4s,   v9.4s,   v13.4s
        add             v10.4s,  v10.4s,  v14.4s
        add             v11.4s,  v11.4s,  v15.4s
        add             v0.8h,   v0.8h,   v20.8h
        add             v1.8h,   v1.8h,   v21.8h

        add             v16.4s,  v16.4s,  v8.4s
        add             v17.4s,  v17.4s,  v9.4s
        add             v18.4s,  v18.4s,  v10.4s
        add             v19.4s,  v19.4s,  v11.4s
        add             v4.8h,   v2.8h,   v0.8h
        add             v5.8h,   v3.8h,   v1.8h

        srshl           v16.4s,  v16.4s,  v7.4s
        srshl           v17.4s,  v17.4s,  v7.4s
        srshl           v18.4s,  v18.4s,  v7.4s
        srshl           v19.4s,  v19.4s,  v7.4s
        srshl           v9.8h,   v4.8h,   v6.8h
        srshl           v13.8h,  v5.8h,   v6.8h
        mul             v16.4s,  v16.4s,  v31.4s // a * n
        mul             v17.4s,  v17.4s,  v31.4s // a * n
        mul             v18.4s,  v18.4s,  v31.4s // a * n
        mul             v19.4s,  v19.4s,  v31.4s // a * n
        umull           v8.4s,   v9.4h,   v9.4h  // b * b
        umull2          v9.4s,   v9.8h,   v9.8h  // b * b
        umull           v12.4s,  v13.4h,  v13.4h // b * b
        umull2          v13.4s,  v13.8h,  v13.8h // b * b
        uqsub           v16.4s,  v16.4s,  v8.4s  // imax(a * n - b * b, 0)
        uqsub           v17.4s,  v17.4s,  v9.4s  // imax(a * n - b * b, 0)
        uqsub           v18.4s,  v18.4s,  v12.4s // imax(a * n - b * b, 0)
        uqsub           v19.4s,  v19.4s,  v13.4s // imax(a * n - b * b, 0)
        mul             v16.4s,  v16.4s,  v28.4s // p * s
        mul             v17.4s,  v17.4s,  v28.4s // p * s
        mul             v18.4s,  v18.4s,  v28.4s // p * s
        mul             v19.4s,  v19.4s,  v28.4s // p * s
        uqshrn          v16.4h,  v16.4s,  #16
        uqshrn2         v16.8h,  v17.4s,  #16
        uqshrn          v18.4h,  v18.4s,  #16
        uqshrn2         v18.8h,  v19.4s,  #16
        uqrshrn         v1.8b,   v16.8h,  #4     // imin(z, 255)
        uqrshrn2        v1.16b,  v18.8h,  #4     // imin(z, 255)

        ld1             {v16.4s, v17.4s}, [x0], #32
        subs            w4,  w4,  #16

        ushr            v0.16b,  v1.16b,  #3
        ld1             {v8.4s,  v9.4s}, [x5], #32
        tbl             v2.16b,  {v26.16b, v27.16b}, v0.16b // RangeMins
        tbl             v0.16b,  {v24.16b, v25.16b}, v0.16b // DiffMasks
        tbl             v3.16b,  {v23.16b}, v1.16b          // RefLo
        and             v1.16b,  v1.16b,   v22.16b
        ld1             {v12.4s, v13.4s}, [x6], #32
        ushl            v1.16b,  v2.16b,  v1.16b
        ld1             {v20.8h, v21.8h}, [x8], #32
        add             v3.16b,  v3.16b,  v0.16b
        cnt             v1.16b,  v1.16b
        ld1             {v18.4s, v19.4s}, [x0], #32
        add             v3.16b,  v3.16b,  v1.16b
        ld1             {v10.4s, v11.4s}, [x5], #32
        uxtl            v0.8h,   v3.8b           // x
        uxtl2           v1.8h,   v3.16b          // x

        ld1             {v14.4s, v15.4s}, [x6], #32

        umull           v2.4s,   v0.4h,   v4.4h // x * BB[i]
        umull2          v3.4s,   v0.8h,   v4.8h // x * BB[i]
        umull           v4.4s,   v1.4h,   v5.4h // x * BB[i]
        umull2          v5.4s,   v1.8h,   v5.8h // x * BB[i]
        mul             v2.4s,   v2.4s,  v30.4s // x * BB[i] * sgr_one_by_x
        mul             v3.4s,   v3.4s,  v30.4s // x * BB[i] * sgr_one_by_x
        mul             v4.4s,   v4.4s,  v30.4s // x * BB[i] * sgr_one_by_x
        mul             v5.4s,   v5.4s,  v30.4s // x * BB[i] * sgr_one_by_x
        st1             {v0.8h, v1.8h}, [x3], #32
        ld1             {v0.8h, v1.8h}, [x7], #32
        srshr           v2.4s,   v2.4s,  #12    // AA[i]
        srshr           v3.4s,   v3.4s,  #12    // AA[i]
        srshr           v4.4s,   v4.4s,  #12    // AA[i]
        srshr           v5.4s,   v5.4s,  #12    // AA[i]

        st1             {v2.4s, v3.4s, v4.4s, v5.4s}, [x2], #64
        b.gt            1b

        ldp             d14, d15, [sp, #0x30]
        ldp             d12, d13, [sp, #0x20]
        ldp             d10, d11, [sp, #0x10]
        ldp             d8,  d9,  [sp], 0x40
        ret
endfunc

// void dav1d_sgr_box5_vert_neon(int32_t **sumsq, int16_t **sum,
//                               int32_t *AA, int16_t *BB,
//                               const int w, const int s,
//                               const int bitdepth_max);
function sgr_box5_vert_neon, export=1
        stp             d8,  d9,  [sp, #-0x30]!
        stp             d10, d11, [sp, #0x10]
        stp             d12, d13, [sp, #0x20]

        add             w4,  w4,  #2
        clz             w15, w6        // bitdepth_max
        dup             v28.4s,   w5   // strength

        ldp             x5,  x6,  [x0]
        ldp             x7,  x8,  [x0, #16]
        ldr             x0,       [x0, #32]
        ldp             x9,  x10, [x1]
        ldp             x11, x12, [x1, #16]
        ldr             x1,       [x1, #32]

        movi            v31.4s,   #25   // n

        sub             w15, w15, #24  // -bitdepth_min_8
        movrel          x13, x_by_x_tables
        movi            v30.4s,  #164
        ld1             {v24.16b, v25.16b, v26.16b, v27.16b}, [x13] // RangeMins, DiffMasks
        dup             v6.8h,   w15  // -bitdepth_min_8
        movi            v19.8b,  #0x7
        ldr             q18, [x13, #64] // RefLo
        saddl           v7.4s,   v6.4h,   v6.4h  // -2*bitdepth_min_8

        ld1             {v8.4s,  v9.4s},  [x5], #32
        ld1             {v10.4s, v11.4s}, [x6], #32
        ld1             {v12.4s, v13.4s}, [x7], #32
        ld1             {v16.4s, v17.4s}, [x8], #32
        ld1             {v20.8h},         [x9], #16
        ld1             {v21.8h},         [x10], #16
        ld1             {v22.8h},         [x11], #16
        ld1             {v23.8h},         [x12], #16
        ld1             {v0.4s,  v1.4s},  [x0], #32
        ld1             {v2.8h},          [x1], #16

1:
        add             v8.4s,   v8.4s,   v10.4s
        add             v9.4s,   v9.4s,   v11.4s
        add             v12.4s,  v12.4s,  v16.4s
        add             v13.4s,  v13.4s,  v17.4s

        add             v20.8h,  v20.8h,  v21.8h
        add             v22.8h,  v22.8h,  v23.8h

        add             v0.4s,   v0.4s,   v8.4s
        add             v1.4s,   v1.4s,   v9.4s
        add             v2.8h,   v2.8h,   v20.8h

        add             v0.4s,   v0.4s,   v12.4s
        add             v1.4s,   v1.4s,   v13.4s
        add             v2.8h,   v2.8h,   v22.8h

        subs            w4,  w4,  #8

        srshl           v0.4s,   v0.4s,   v7.4s
        srshl           v1.4s,   v1.4s,   v7.4s
        srshl           v4.8h,   v2.8h,   v6.8h
        mul             v0.4s,   v0.4s,   v31.4s // a * n
        mul             v1.4s,   v1.4s,   v31.4s // a * n
        umull           v3.4s,   v4.4h,   v4.4h  // b * b
        umull2          v4.4s,   v4.8h,   v4.8h  // b * b
        uqsub           v0.4s,   v0.4s,   v3.4s  // imax(a * n - b * b, 0)
        uqsub           v1.4s,   v1.4s,   v4.4s  // imax(a * n - b * b, 0)
        mul             v0.4s,   v0.4s,   v28.4s // p * s
        mul             v1.4s,   v1.4s,   v28.4s // p * s
        ld1             {v8.4s,  v9.4s},  [x5], #32
        uqshrn          v0.4h,   v0.4s,   #16
        uqshrn2         v0.8h,   v1.4s,   #16
        ld1             {v10.4s, v11.4s}, [x6], #32
        uqrshrn         v0.8b,   v0.8h,   #4     // imin(z, 255)

        ld1             {v12.4s, v13.4s}, [x7], #32

        ushr            v1.8b,   v0.8b,  #3
        ld1             {v16.4s, v17.4s}, [x8], #32
        tbl             v5.8b,   {v26.16b, v27.16b}, v1.8b // RangeMins
        tbl             v1.8b,   {v24.16b, v25.16b}, v1.8b // DiffMasks
        tbl             v4.8b,   {v18.16b}, v0.8b          // RefLo
        and             v0.8b,   v0.8b,  v19.8b
        ld1             {v20.8h},         [x9], #16
        ushl            v5.8b,   v5.8b,  v0.8b
        add             v4.8b,   v4.8b,  v1.8b
        ld1             {v21.8h},         [x10], #16
        cnt             v5.8b,   v5.8b
        ld1             {v22.8h},         [x11], #16
        add             v5.8b,   v4.8b,  v5.8b
        ld1             {v23.8h},         [x12], #16
        uxtl            v5.8h,   v5.8b           // x

        ld1             {v0.4s,  v1.4s},  [x0], #32
        umull           v3.4s,   v5.4h,   v2.4h  // x * BB[i]
        umull2          v4.4s,   v5.8h,   v2.8h  // x * BB[i]
        mul             v3.4s,   v3.4s,   v30.4s // x * BB[i] * sgr_one_by_x
        mul             v4.4s,   v4.4s,   v30.4s // x * BB[i] * sgr_one_by_x
        srshr           v3.4s,   v3.4s,   #12    // AA[i]
        srshr           v4.4s,   v4.4s,   #12    // AA[i]
        ld1             {v2.8h},          [x1], #16

        st1             {v3.4s, v4.4s}, [x2], #32
        st1             {v5.8h}, [x3], #16
        b.gt            1b

        ldp             d12, d13, [sp, #0x20]
        ldp             d10, d11, [sp, #0x10]
        ldp             d8,  d9,  [sp], 0x30
        ret
endfunc

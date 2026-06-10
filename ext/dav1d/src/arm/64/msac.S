/*
 * Copyright © 2019, VideoLAN and dav1d authors
 * Copyright © 2019, Martin Storsjo
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

#define BUF_POS 0
#define BUF_END 8
#define DIF 16
#define RNG 24
#define CNT 28
#define ALLOW_UPDATE_CDF 32

#define COEFFS_BASE_OFFSET 30
#define MASKS8_OFFSET (64-COEFFS_BASE_OFFSET)

const coeffs
        .short 60, 56, 52, 48, 44, 40, 36, 32, 28, 24, 20, 16, 12, 8, 4, 0
        .short 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0
        // masks8
        .short -0x202, -0x202, -0x202, -0x202, -0x202, -0x202, -0x202, 0xF0E
endconst

.macro ld1_n d0, d1, src, sz, n
.if \n <= 8
        ld1             {\d0\sz},  [\src]
.else
        ld1             {\d0\sz, \d1\sz},  [\src]
.endif
.endm

.macro st1_n s0, s1, dst, sz, n
.if \n <= 8
        st1             {\s0\sz},  [\dst]
.else
        st1             {\s0\sz, \s1\sz},  [\dst]
.endif
.endm

.macro ushr_n d0, d1, s0, s1, shift, sz, n
        ushr            \d0\sz,  \s0\sz,  \shift
.if \n == 16
        ushr            \d1\sz,  \s1\sz,  \shift
.endif
.endm

.macro add_n d0, d1, s0, s1, s2, s3, sz, n
        add             \d0\sz,  \s0\sz,  \s2\sz
.if \n == 16
        add             \d1\sz,  \s1\sz,  \s3\sz
.endif
.endm

.macro sub_n d0, d1, s0, s1, s2, s3, sz, n
        sub             \d0\sz,  \s0\sz,  \s2\sz
.if \n == 16
        sub             \d1\sz,  \s1\sz,  \s3\sz
.endif
.endm

.macro and_n d0, d1, s0, s1, s2, s3, sz, n
        and             \d0\sz,  \s0\sz,  \s2\sz
.if \n == 16
        and             \d1\sz,  \s1\sz,  \s3\sz
.endif
.endm

.macro cmhs_n d0, d1, s0, s1, s2, s3, sz, n
        cmhs            \d0\sz,  \s0\sz,  \s2\sz
.if \n == 16
        cmhs            \d1\sz,  \s1\sz,  \s3\sz
.endif
.endm

.macro sshl_n d0, d1, s0, s1, s2, s3, sz, n
        sshl            \d0\sz,  \s0\sz,  \s2\sz
.if \n == 16
        sshl            \d1\sz,  \s1\sz,  \s3\sz
.endif
.endm

.macro sqdmulh_n d0, d1, s0, s1, s2, s3, sz, n
        sqdmulh         \d0\sz,  \s0\sz,  \s2\sz
.if \n == 16
        sqdmulh         \d1\sz,  \s1\sz,  \s3\sz
.endif
.endm

.macro str_n            idx0, idx1, dstreg, dstoff, n
        str             \idx0,  [\dstreg, \dstoff]
.if \n == 16
        str             \idx1,  [\dstreg, \dstoff + 16]
.endif
.endm

// unsigned dav1d_msac_decode_symbol_adapt4_neon(MsacContext *s, uint16_t *cdf,
//                                               size_t n_symbols);

function msac_decode_symbol_adapt4_neon, export=1
.macro decode_update sz, szb, n
.if \n == 16
        sub             sp,  sp,  #48
.endif
        add             x8,  x0,  #RNG
        ld1_n           v0,  v1,  x1,  \sz, \n                    // cdf
        ld1r            {v29\sz}, [x8]                            // rng
        movrel          x9,  coeffs, COEFFS_BASE_OFFSET
        movi            v31\sz, #0x7f, lsl #8                     // 0x7f00
        sub             x10, x9,  x2, lsl #1
        mvni            v30\sz, #0x3f                             // 0xffc0
        and             v7\szb, v29\szb, v31\szb                  // rng & 0x7f00
.if \n == 16
        str             h29, [sp, #14]                            // store original u = s->rng
.endif
        and_n           v2,  v3,  v0,  v1,  v30, v30, \szb, \n    // cdf & 0xffc0

        ld1_n           v4,  v5,  x10, \sz, \n                    // EC_MIN_PROB * (n_symbols - ret)
        sqdmulh_n       v6,  v7,  v2,  v3,  v7,  v7,  \sz, \n     // ((cdf >> EC_PROB_SHIFT) * (r - 128)) >> 1
        ldr             d28, [x0, #DIF]

        add_n           v4,  v5,  v2,  v3,  v4,  v5,  \sz, \n     // v = cdf + EC_MIN_PROB * (n_symbols - ret)
        add_n           v4,  v5,  v6,  v7,  v4,  v5,  \sz, \n     // v = ((cdf >> EC_PROB_SHIFT) * r) >> 1 + EC_MIN_PROB * (n_symbols - ret)

        dup             v30\sz, v28.h[3]                          // dif >> (EC_WIN_SIZE - 16)
.if \n == 8
        ldur            q31, [x9, #MASKS8_OFFSET]
.elseif \n == 16
        str_n           q4,  q5,  sp, #16, \n                     // store v values to allow indexed access
.endif

        // After the condition starts being true it continues, such that the vector looks like:
        //   0, 0, 0 ... -1, -1
        cmhs_n          v2,  v3,  v30, v30, v4,  v5,  \sz,  \n    // c >= v
.if \n == 4
        ext             v29\szb, v29\szb, v4\szb, #6              // u
        umov            x15, v2.d[0]
        ldr             w4,  [x0, #ALLOW_UPDATE_CDF]
        rev             x15, x15
        sub             v29\sz, v29\sz, v4\sz                     // rng = u-v
        // rev + clz = count trailing zeros
        clz             x15, x15                                  // 16*ret
.elseif \n == 8
        // The final short of the compare is always set.
        // Using addv, subtract -0x202*ret from this value to create a lookup table for a short.
        //  For n == 8:
        // -0x202 + -0x202 + ... + 0xF0E
        //                    (0x202*7) | (1 << 8)
        //                                    ^-------offset for second byte of the short
        and             v31\szb, v31\szb, v2\szb
        ext             v29\szb, v29\szb, v4\szb, #14             // u
        addv            h31, v31\sz                               // ((2*ret + 1) << 8) | (2*ret)
        ldr             w4,  [x0, #ALLOW_UPDATE_CDF]
        sub             v30\sz, v30\sz, v4\sz                     // (dif >> 48) - v
        smov            w15, v31.b[0]                             // 2*ret
        sub             v29\sz, v29\sz, v4\sz                     // rng = u-v
.elseif \n == 16
        add             v6\sz,  v2\sz,  v3\sz
        addv            h31, v6\sz                                // -n + ret
        ldr             w4,  [x0, #ALLOW_UPDATE_CDF]
        smov            w15, v31.h[0]
.endif

        cbz             w4,  0f

        // update_cdf
        ldrh            w3,  [x1, x2, lsl #1]                     // count = cdf[n_symbols]
.if \n == 16
        // 16 case has a lower bound that guarantees n_symbols > 2
        mov             w4,  #-5
.elseif \n == 8
        mvn             w14, w2
        mov             w4,  #-4
        cmn             w14, #3                                   // set C if n_symbols <= 2
.else
        // if n_symbols < 4 (or < 6 even) then
        //   (1 + n_symbols) >> 2 == n_symbols > 2
        add             w14, w2,  #17                             // (1 + n_symbols) + (4 << 2)
.endif
        sub_n           v16, v17, v0,  v1,  v2,  v3,  \sz, \n     // cdf + (i >= val ? 1 : 0)
        orr             v2\sz, #0x80, lsl #8
.if \n == 16
        orr             v3\sz, #0x80, lsl #8
.endif
.if \n == 16
        sub             w4,  w4,  w3, lsr #4                      // -((count >> 4) + 5)
.elseif \n == 8
        lsr             w14, w3,  #4                              // count >> 4
        sbc             w4,  w4,  w14                             // -((count >> 4) + (n_symbols > 2) + 4)
.else
        neg             w4, w14, lsr #2                           // -((n_symbols > 2) + 4)
        sub             w4,  w4,  w3,  lsr #4                     // -((count >> 4) + (n_symbols > 2) + 4)
.endif
        sub_n           v2,  v3,  v2,  v3,  v0,  v1,  \sz, \n     // (32768 - cdf[i]) or (-1 - cdf[i])
        dup             v6\sz,    w4                              // -rate

        sub             w3,  w3,  w3, lsr #5                      // count - (count == 32)
        sshl_n          v2,  v3,  v2,  v3,  v6,  v6,  \sz, \n     // ({32768,-1} - cdf[i]) >> rate
        add             w3,  w3,  #1                              // count + (count < 32)
        add_n           v0,  v1,  v16, v17, v2,  v3,  \sz, \n     // cdf + (32768 - cdf[i]) >> rate
        st1_n           v0,  v1,  x1,  \sz, \n
        strh            w3,  [x1, x2, lsl #1]

0:
        // renorm
.if \n == 4
        ldr             w6,  [x0, #CNT]
        ldr             x7,  [x0, #DIF]
        mov             x4,  v29.d[0]          // rng (packed)
        mov             x3,  v4.d[0]           // v (packed)

        // Shift 'v'/'rng' for ret into the 16 least sig bits. There is
        //  garbage in the remaining bits, but we can work around this.
        lsr             x4,  x4,  x15          // rng
        lsr             x3,  x3,  x15          // v
        lsl             w5,  w4,  #16          // rng << 16
        sub             x7,  x7,  x3, lsl #48  // dif - (v << 48)
        clz             w5,  w5                // d = clz(rng << 16)
        lsl             w4,  w4,  w5           // rng << d
        subs            w6,  w6,  w5           // cnt -= d
        lsl             x7,  x7,  x5           // (dif - (v << 48)) << d
        strh            w4,  [x0, #RNG]
        b.lo            1f
        str             w6,  [x0, #CNT]
        str             x7,  [x0, #DIF]
        lsr             w0,  w15, #4
        ret
1:
        lsr             w15, w15, #4
        b L(refill)
.elseif \n == 8
        ldr             w6,  [x0, #CNT]
        tbl             v30.8b, {v30.16b}, v31.8b
        tbl             v29.8b, {v29.16b}, v31.8b
        ins             v28.h[3], v30.h[0]     // dif - (v << 48)
        clz             v0.4h,  v29.4h         // d = clz(rng)
        umov            w5,  v0.h[0]
        ushl            v29.4h, v29.4h, v0.4h  // rng << d

        // The vec for clz(rng) is filled with garbage after the first short,
        //  but ushl/sshl conveniently uses only the first byte for the shift
        //  amount.
        ushl            d28, d28, d0           // (dif - (v << 48)) << d

        subs            w6,  w6,  w5           // cnt -= d
        str             h29, [x0, #RNG]
        b.lo            1f
        str             w6,  [x0, #CNT]
        str             d28, [x0, #DIF]
        lsr             w0,  w15, #1           // ret
        ret
1:
        lsr             w15, w15, #1           // ret
        mov             x7, v28.d[0]
        b L(refill)
.elseif \n == 16
        add             x8,  sp,  w15, sxtw #1
        ldrh            w3,  [x8, #48]         // v
        ldurh           w4,  [x8, #46]         // u
        ldr             w6,  [x0, #CNT]
        ldr             x7,  [x0, #DIF]
        sub             w4,  w4,  w3           // rng = u - v
        clz             w5,  w4                // clz(rng)
        eor             w5,  w5,  #16          // d = clz(rng) ^ 16
        sub             x7,  x7,  x3, lsl #48  // dif - (v << 48)
        lsl             w4,  w4,  w5           // rng << d
        subs            w6,  w6,  w5           // cnt -= d
        lsl             x7,  x7,  x5           // (dif - (v << 48)) << d
        str             w4,  [x0, #RNG]
        add             sp,  sp,  #48
        b.lo            1f
        str             w6,  [x0, #CNT]
        str             x7,  [x0, #DIF]
        add             w0,  w15, #\n          // ret
        ret
1:
        add             w15, w15, #\n          // ret
        b L(refill)
.endif
.endm

        decode_update   .4h, .8b, 4

L(refill):
        // refill
        ldp             x3,  x4,  [x0]         // BUF_POS, BUF_END
        add             x5,  x3,  #8
        subs            x5,  x5,  x4
        b.hi            6f

        ldr             x8,  [x3]              // next_bits
        add             w4,  w6,  #-48         // shift_bits = cnt + 16 (- 64)
        mvn             x8,  x8
        neg             w5,  w4
        rev             x8,  x8                // next_bits = bswap(next_bits)
        lsr             w5,  w5,  #3           // num_bytes_read
        lsr             x8,  x8,  x4           // next_bits >>= (shift_bits & 63)

2:      // refill_end
        add             x3,  x3,  x5
        add             w6,  w6,  w5, lsl #3   // cnt += num_bits_read
        str             x3,  [x0, #BUF_POS]

3:      // refill_end2
        orr             x7,  x7,  x8           // dif |= next_bits

4:      // end
        str             w6,  [x0, #CNT]
        str             x7,  [x0, #DIF]

        mov             w0,  w15
        ret

5:      // pad_with_ones
        add             w8,  w6,  #-16
        ror             x8,  x8,  x8
        b               3b

6:      // refill_eob
        cmp             x3,  x4
        b.hs            5b

        ldr             x8,  [x4, #-8]
        lsl             w5,  w5,  #3
        lsr             x8,  x8,  x5
        add             w5,  w6,  #-48
        mvn             x8,  x8
        sub             w4,  w4,  w3           // num_bytes_left
        rev             x8,  x8
        lsr             x8,  x8,  x5
        neg             w5,  w5
        lsr             w5,  w5,  #3
        cmp             w5,  w4
        csel            w5,  w5,  w4,  lo      // num_bytes_read
        b               2b
endfunc

function msac_decode_symbol_adapt8_neon, export=1
        decode_update   .8h, .16b, 8
endfunc

function msac_decode_symbol_adapt16_neon, export=1
        decode_update   .8h, .16b, 16
endfunc

function msac_decode_hi_tok_neon, export=1
        ld1             {v0.4h},  [x1]            // cdf
        add             x16, x0,  #RNG
        movi            v31.4h, #0x7f, lsl #8     // 0x7f00
        movrel          x17, coeffs, COEFFS_BASE_OFFSET-2*3
        mvni            v30.4h, #0x3f             // 0xffc0
        ldrh            w9,  [x1, #6]             // count = cdf[n_symbols]
        ld1r            {v3.4h},  [x16]           // rng
        ld1             {v29.4h}, [x17]           // EC_MIN_PROB * (n_symbols - ret)
        add             x17, x0,  #DIF + 6
        mov             w13, #-24*8
        and             v17.8b,  v0.8b,   v30.8b  // cdf & 0xffc0
        ldr             w10, [x0, #ALLOW_UPDATE_CDF]
        ld1r            {v1.8h},  [x17]           // dif >> (EC_WIN_SIZE - 16)
        ldr             w6,  [x0, #CNT]
        ldr             x7,  [x0, #DIF]
1:
        and             v7.8b,   v3.8b,   v31.8b  // rng & 0x7f00
        sqdmulh         v6.4h,   v17.4h,  v7.4h   // ((cdf >> EC_PROB_SHIFT) * (r - 128)) >> 1
        add             v4.4h,   v17.4h,  v29.4h  // v = cdf + EC_MIN_PROB * (n_symbols - ret)
        add             v4.4h,   v6.4h,   v4.4h   // v = ((cdf >> EC_PROB_SHIFT) * r) >> 1 + EC_MIN_PROB * (n_symbols - ret)
        cmhs            v2.4h,   v1.4h,   v4.4h   // c >= v
        add             w13, w13, #5*8
        ext             v18.8b, v3.8b,  v4.8b, #6 // u
        umov            x15, v2.d[0]
        rev             x15, x15
        sub             v18.4h, v18.4h, v4.4h     // rng = u-v
        // rev + clz = count trailing zeros
        clz             x15, x15                  // 16*ret

        cbz             w10, 2f
        // update_cdf
        sub             v5.4h,   v0.4h,   v2.4h   // cdf[i] + (i >= val ? 1 : 0)
        mov             w4,  #-5
        orr             v2.4h, #0x80, lsl #8      // i >= val ? -1 : 32768
        sub             w4,  w4,  w9, lsr #4      // -((count >> 4) + 5)
        sub             v2.4h,   v2.4h,   v0.4h   // (32768 - cdf[i]) or (-1 - cdf[i])
        dup             v6.4h,    w4              // -rate

        sub             w9,  w9,  w9, lsr #5      // count - (count == 32)
        sshl            v2.4h,   v2.4h,   v6.4h   // ({32768,-1} - cdf[i]) >> rate
        add             w9,  w9,  #1              // count + (count < 32)
        add             v0.4h,   v5.4h,   v2.4h   // cdf[i] + (32768 - cdf[i]) >> rate
        st1             {v0.4h},  [x1]
        and             v17.8b,  v0.8b,   v30.8b  // cdf & 0xffc0
        strh            w9,  [x1, #6]

2:
        mov             x4,  v18.d[0]          // rng (packed)
        mov             x3,  v4.d[0]           // v (packed)

        // Shift 'v'/'rng' for ret into the 16 least sig bits. There is
        //  garbage in the remaining bits, but we can work around this.
        lsr             x4,  x4,  x15          // rng
        lsr             x3,  x3,  x15          // v
        lsl             w5,  w4,  #16          // rng << 16
        sub             x7,  x7,  x3, lsl #48  // dif - (v << 48)
        clz             w5,  w5                // d = clz(rng << 16)
        lsl             w4,  w4,  w5           // rng << d
        subs            w6,  w6,  w5           // cnt -= d
        lsl             x7,  x7,  x5           // (dif - (v << 48)) << d
        strh            w4,  [x0, #RNG]
        dup             v3.4h,   w4
        b.hs            5f

        // refill
        ldp             x3,  x4,  [x0]         // BUF_POS, BUF_END
        add             x5,  x3,  #8
        subs            x5,  x5,  x4
        b.hi            7f

        ldr             x8,  [x3]              // next_bits
        add             w4,  w6,  #-48         // shift_bits = cnt + 16 (- 64)
        mvn             x8,  x8
        neg             w5,  w4
        rev             x8,  x8                // next_bits = bswap(next_bits)
        lsr             w5,  w5,  #3           // num_bytes_read
        lsr             x8,  x8,  x4           // next_bits >>= (shift_bits & 63)

3:      // refill_end
        add             x3,  x3,  x5
        add             w6,  w6,  w5, lsl #3   // cnt += num_bits_read
        str             x3,  [x0, #BUF_POS]

4:      // refill_end2
        orr             x7,  x7,  x8           // dif |= next_bits

5:      // end
        sub             w15, w15, #5*8
        lsr             x12, x7,  #48
        adds            w13, w13, w15          // carry = tok_br < 3 || tok == 15
        dup             v1.8h,   w12
        b.cc            1b                     // loop if !carry
        add             w13, w13, #30*8
        str             w6,  [x0, #CNT]
        str             x7,  [x0, #DIF]
        lsr             w0,  w13, #4
        ret

6:      // pad_with_ones
        add             w8,  w6,  #-16
        ror             x8,  x8,  x8
        b               4b

7:      // refill_eob
        cmp             x3,  x4
        b.hs            6b

        ldr             x8,  [x4, #-8]
        lsl             w5,  w5,  #3
        lsr             x8,  x8,  x5
        add             w5,  w6,  #-48
        mvn             x8,  x8
        sub             w4,  w4,  w3           // num_bytes_left
        rev             x8,  x8
        lsr             x8,  x8,  x5
        neg             w5,  w5
        lsr             w5,  w5,  #3
        cmp             w5,  w4
        csel            w5,  w5,  w4,  lo      // num_bytes_read
        b               3b
endfunc

function msac_decode_bool_equi_neon, export=1
        ldp             w5,  w6,  [x0, #RNG]   // + CNT
        ldr             x7,  [x0, #DIF]
        bic             w4,  w5,  #0xff        // r &= 0xff00
        add             w4,  w4,  #8
        subs            x8,  x7,  x4, lsl #47  // dif - vw
        lsr             w4,  w4,  #1           // v
        sub             w5,  w5,  w4           // r - v
        cset            w15, lo
        csel            w4,  w5,  w4,  hs      // if (ret) v = r - v;
        csel            x7,  x8,  x7,  hs      // if (ret) dif = dif - vw;

        clz             w5,  w4                // clz(rng)
        eor             w5,  w5,  #16          // d = clz(rng) ^ 16
        lsl             w4,  w4,  w5           // rng << d
        subs            w6,  w6,  w5           // cnt -= d
        lsl             x7,  x7,  x5           // (dif - (v << 48)) << d
        str             w4,  [x0, #RNG]
        b.lo            L(refill)

        str             w6,  [x0, #CNT]
        str             x7,  [x0, #DIF]
        mov             w0,  w15
        ret
endfunc

function msac_decode_bool_neon, export=1
        ldp             w5,  w6,  [x0, #RNG]   // + CNT
        ldr             x7,  [x0, #DIF]
        lsr             w4,  w5,  #8           // r >> 8
        bic             w1,  w1,  #0x3f        // f &= ~63
        mul             w4,  w4,  w1
        lsr             w4,  w4,  #7
        add             w4,  w4,  #4           // v
        subs            x8,  x7,  x4, lsl #48  // dif - vw
        sub             w5,  w5,  w4           // r - v
        cset            w15, lo
        csel            w4,  w5,  w4,  hs      // if (ret) v = r - v;
        csel            x7,  x8,  x7,  hs      // if (ret) dif = dif - vw;

        clz             w5,  w4                // clz(rng)
        eor             w5,  w5,  #16          // d = clz(rng) ^ 16
        lsl             w4,  w4,  w5           // rng << d
        subs            w6,  w6,  w5           // cnt -= d
        lsl             x7,  x7,  x5           // (dif - (v << 48)) << d
        str             w4,  [x0, #RNG]
        b.lo            L(refill)

        str             w6,  [x0, #CNT]
        str             x7,  [x0, #DIF]
        mov             w0,  w15
        ret
endfunc

function msac_decode_bool_adapt_neon, export=1
        ldr             w9,  [x1]              // cdf[0-1]
        ldp             w5,  w6,  [x0, #RNG]   // + CNT
        ldr             x7,  [x0, #DIF]
        lsr             w4,  w5,  #8           // r >> 8
        and             w2,  w9,  #0xffc0      // f &= ~63
        mul             w4,  w4,  w2
        lsr             w4,  w4,  #7
        add             w4,  w4,  #4           // v
        subs            x8,  x7,  x4, lsl #48  // dif - vw
        sub             w5,  w5,  w4           // r - v
        cset            w15, lo
        csel            w4,  w5,  w4,  hs      // if (ret) v = r - v;
        csel            x7,  x8,  x7,  hs      // if (ret) dif = dif - vw;

        ldr             w10, [x0, #ALLOW_UPDATE_CDF]

        clz             w5,  w4                // clz(rng)
        eor             w5,  w5,  #16          // d = clz(rng) ^ 16

        cbz             w10, 1f

        lsr             w2,  w9,  #16          // count = cdf[1]
        and             w9,  w9,  #0xffff      // cdf[0]

        sub             w3,  w2,  w2, lsr #5   // count - (count >= 32)
        lsr             w2,  w2,  #4           // count >> 4
        add             w10, w3,  #1           // count + (count < 32)
        add             w2,  w2,  #4           // rate = (count >> 4) | 4

        sub             w9,  w9,  w15          // cdf[0] -= bit
        sub             w11, w9,  w15, lsl #15 // {cdf[0], cdf[0] - 32769}
        asr             w11, w11, w2           // {cdf[0], cdf[0] - 32769} >> rate
        sub             w9,  w9,  w11          // cdf[0]

        strh            w9,  [x1]
        strh            w10, [x1, #2]

1:
        lsl             w4,  w4,  w5           // rng << d
        subs            w6,  w6,  w5           // cnt -= d
        lsl             x7,  x7,  x5           // (dif - (v << 48)) << d
        str             w4,  [x0, #RNG]
        b.lo            L(refill)

        str             w6,  [x0, #CNT]
        str             x7,  [x0, #DIF]
        mov             w0,  w15
        ret
endfunc

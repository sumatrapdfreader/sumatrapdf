/*
 * Copyright © 2023, VideoLAN and dav1d authors
 * Copyright © 2023, Loongson Technology Corporation Limited
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

#include "loongson_asm.S"

const min_prob
  .short 60, 56, 52, 48, 44, 40, 36, 32, 28, 24, 20, 16, 12, 8, 4, 0
endconst

const ph_0xff00
.rept 8
  .short 0xff00
.endr
endconst

.macro decode_symbol_adapt w
    addi.d          sp,      sp,     -48
    vldrepl.h       vr0,     a0,      24   //rng
    fst.s           f0,      sp,      0    //val==0
    vld             vr1,     a1,      0    //cdf
.if \w == 16
    vld             vr11,    a1,      16
.endif
    vldrepl.d       vr2,     a0,      16   //dif
    ld.w            t1,      a0,      32   //allow_update_cdf
    la.local        t2,      min_prob
    addi.d          t2,      t2,      30
    slli.w          t3,      a2,      1
    sub.d           t2,      t2,      t3
    vld             vr3,     t2,      0    //min_prob
.if \w == 16
    vld             vr13,    t2,      16
.endif
    vsrli.h         vr4,     vr0,     8    //r = s->rng >> 8
    vslli.h         vr4,     vr4,     8    //r << 8
    vsrli.h         vr5,     vr1,     6
    vslli.h         vr5,     vr5,     7
.if \w == 16
    vsrli.h         vr15,    vr11,    6
    vslli.h         vr15,    vr15,    7
.endif
    vmuh.hu         vr5,     vr4,     vr5
    vadd.h          vr5,     vr5,     vr3  //v
.if \w == 16
    vmuh.hu         vr15,    vr4,     vr15
    vadd.h          vr15,    vr15,    vr13
.endif
    addi.d          t8,      sp,      2
    vst             vr5,     t8,      0    //store v
.if \w == 16
    vst             vr15,    t8,      16
.endif
    vreplvei.h      vr20,    vr2,     3    //c
    vsle.hu         vr6,     vr5,     vr20
.if \w == 16
    vsle.hu         vr16,    vr15,    vr20
    vpickev.b       vr21,    vr16,    vr6
.endif
.if \w <= 8
    vmskltz.h       vr10,    vr6
.else
    vmskltz.b       vr10,    vr21
.endif
    beqz            t1,      .renorm\()\w

    // update_cdf
    alsl.d          t1,      a2,      a1,   1
    ld.h            t2,      t1,      0    //count
    srli.w          t3,      t2,      4    //count >> 4
.if \w == 16
    addi.w          t3,      t3,      5    //rate
.else
    addi.w          t3,      t3,      4
    li.w            t5,      2
    sltu            t5,      t5,      a2
    add.w           t3,      t3,      t5   //rate
.endif
    sltui           t5,      t2,      32
    add.w           t2,      t2,      t5   //count + (count < 32)
    vreplgr2vr.h    vr9,     t3
    vseq.h          vr7,     vr7,     vr7
    vavgr.hu        vr5,     vr6,     vr7  //i >= val ? -1 : 32768
    vsub.h          vr5,     vr5,     vr1
    vsub.h          vr8,     vr1,     vr6
.if \w == 16
    vavgr.hu        vr15,    vr16,    vr7
    vsub.h          vr15,    vr15,    vr11
    vsub.h          vr18,    vr11,    vr16
.endif
    vsra.h          vr5,     vr5,     vr9
    vadd.h          vr8,     vr8,     vr5
.if \w == 4
    fst.d           f8,      a1,      0
.else
    vst             vr8,     a1,      0
.endif
.if \w == 16
    vsra.h          vr15,    vr15,    vr9
    vadd.h          vr18,    vr18,    vr15
    vst             vr18,    a1,      16
.endif
    st.h            t2,      t1,      0

.renorm\()\w:
    vpickve2gr.h    t3,      vr10,    0
    ctz.w           a7,      t3            // ret
    alsl.d          t3,      a7,      t8,      1
    ld.hu           t4,      t3,      0    // v
    ld.hu           t5,      t3,      -2   // u
    sub.w           t5,      t5,      t4   // rng
    slli.d          t4,      t4,      48
    vpickve2gr.d    t6,      vr2,     0
    sub.d           t6,      t6,      t4   // dif
    clz.w           t4,      t5            // d
    xori            t4,      t4,      16   // d
    sll.d           t6,      t6,      t4
    ld.w            t0,      a0,      28   //cnt
    sll.w           t5,      t5,      t4
    sub.w           t7,      t0,      t4   // cnt-d
    st.w            t5,      a0,      24   // store rng
    bgeu            t0,      t4,      9f

    // refill
    ld.d            t0,      a0,      0    // buf_pos
    ld.d            t1,      a0,      8    // buf_end
    addi.d          t2,      t0,      8
    bltu            t1,      t2,      2f

    ld.d            t3,      t0,      0    // next_bits
    addi.w          t1,      t7,      -48  // shift_bits = cnt + 16 (- 64)
    nor             t3,      t3,      t3
    sub.w           t2,      zero,    t1
    revb.d          t3,      t3            // next_bits = bswap(next_bits)
    srli.w          t2,      t2,      3    // num_bytes_read
    srl.d           t3,      t3,      t1   // next_bits >>= (shift_bits & 63)
    b               3f
1:
    addi.w          t3,      t7,      -48
    srl.d           t3,      t3,      t3   // pad with ones
    b               4f
2:
    bgeu            t0,      t1,      1b
    ld.d            t3,      t1,      -8   // next_bits
    sub.w           t2,      t2,      t1
    sub.w           t1,      t1,      t0   // num_bytes_left
    slli.w          t2,      t2,      3
    srl.d           t3,      t3,      t2
    addi.w          t2,      t7,      -48
    nor             t3,      t3,      t3
    sub.w           t4,      zero,    t2
    revb.d          t3,      t3
    srli.w          t4,      t4,      3
    srl.d           t3,      t3,      t2
    sltu            t2,      t1,      t4
    maskeqz         t1,      t1,      t2
    masknez         t2,      t4,      t2
    or              t2,      t2,      t1   // num_bytes_read
3:
    slli.w          t1,      t2,      3
    add.d           t0,      t0,      t2
    add.w           t7,      t7,      t1   // cnt += num_bits_read
    st.d            t0,      a0,      0
4:
    or              t6,      t6,      t3   // dif |= next_bits
9:
    st.w            t7,      a0,      28   // store cnt
    st.d            t6,      a0,      16   // store dif
    move            a0,      a7
    addi.d          sp,      sp,      48
.endm

function msac_decode_symbol_adapt4_lsx
    decode_symbol_adapt 4
endfunc

function msac_decode_symbol_adapt8_lsx
    decode_symbol_adapt 8
endfunc

function msac_decode_symbol_adapt16_lsx
    decode_symbol_adapt 16
endfunc

function msac_decode_bool_lsx
    ld.w            t0,      a0,      24   // rng
    srli.w          a1,      a1,      6
    ld.d            t1,      a0,      16   // dif
    srli.w          t2,      t0,      8    // r >> 8
    mul.w           t2,      t2,      a1
    ld.w            a5,      a0,      28   // cnt
    srli.w          t2,      t2,      1
    addi.w          t2,      t2,      4    // v
    slli.d          t3,      t2,      48   // vw
    sltu            t4,      t1,      t3
    move            t8,      t4            // ret
    xori            t4,      t4,      1
    maskeqz         t6,      t3,      t4   // if (ret) vw
    sub.d           t6,      t1,      t6   // dif
    slli.w          t5,      t2,      1
    sub.w           t5,      t0,      t5   // r - 2v
    maskeqz         t7,      t5,      t4   // if (ret) r - 2v
    add.w           t5,      t2,      t7   // v(rng)

    // renorm
    clz.w           t4,      t5            // d
    xori            t4,      t4,      16   // d
    sll.d           t6,      t6,      t4
    sll.w           t5,      t5,      t4
    sub.w           t7,      a5,      t4   // cnt-d
    st.w            t5,      a0,      24   // store rng
    bgeu            a5,      t4,      9f

    // refill
    ld.d            t0,      a0,      0    // buf_pos
    ld.d            t1,      a0,      8    // buf_end
    addi.d          t2,      t0,      8
    bltu            t1,      t2,      2f

    ld.d            t3,      t0,      0    // next_bits
    addi.w          t1,      t7,      -48  // shift_bits = cnt + 16 (- 64)
    nor             t3,      t3,      t3
    sub.w           t2,      zero,    t1
    revb.d          t3,      t3            // next_bits = bswap(next_bits)
    srli.w          t2,      t2,      3    // num_bytes_read
    srl.d           t3,      t3,      t1   // next_bits >>= (shift_bits & 63)
    b               3f
1:
    addi.w          t3,      t7,      -48
    srl.d           t3,      t3,      t3   // pad with ones
    b               4f
2:
    bgeu            t0,      t1,      1b
    ld.d            t3,      t1,      -8   // next_bits
    sub.w           t2,      t2,      t1
    sub.w           t1,      t1,      t0   // num_bytes_left
    slli.w          t2,      t2,      3
    srl.d           t3,      t3,      t2
    addi.w          t2,      t7,      -48
    nor             t3,      t3,      t3
    sub.w           t4,      zero,    t2
    revb.d          t3,      t3
    srli.w          t4,      t4,      3
    srl.d           t3,      t3,      t2
    sltu            t2,      t1,      t4
    maskeqz         t1,      t1,      t2
    masknez         t2,      t4,      t2
    or              t2,      t2,      t1   // num_bytes_read
3:
    slli.w          t1,      t2,      3
    add.d           t0,      t0,      t2
    add.w           t7,      t7,      t1   // cnt += num_bits_read
    st.d            t0,      a0,      0
4:
    or              t6,      t6,      t3   // dif |= next_bits
9:
    st.w            t7,      a0,      28   // store cnt
    st.d            t6,      a0,      16   // store dif
    move            a0,      t8
endfunc

function msac_decode_bool_equi_lsx
    ld.w            t0,      a0,      24   // rng
    ld.d            t1,      a0,      16   // dif
    ld.w            a5,      a0,      28   // cnt
    srli.w          t2,      t0,      8    // r >> 8
    slli.w          t2,      t2,      7
    addi.w          t2,      t2,      4    // v

    slli.d          t3,      t2,      48   // vw
    sltu            t4,      t1,      t3
    move            t8,      t4            // ret
    xori            t4,      t4,      1
    maskeqz         t6,      t3,      t4   // if (ret) vw
    sub.d           t6,      t1,      t6   // dif
    slli.w          t5,      t2,      1
    sub.w           t5,      t0,      t5   // r - 2v
    maskeqz         t7,      t5,      t4   // if (ret) r - 2v
    add.w           t5,      t2,      t7   // v(rng)

    // renorm
    clz.w           t4,      t5            // d
    xori            t4,      t4,      16   // d
    sll.d           t6,      t6,      t4
    sll.w           t5,      t5,      t4
    sub.w           t7,      a5,      t4   // cnt-d
    st.w            t5,      a0,      24   // store rng
    bgeu            a5,      t4,      9f

    // refill
    ld.d            t0,      a0,      0    // buf_pos
    ld.d            t1,      a0,      8    // buf_end
    addi.d          t2,      t0,      8
    bltu            t1,      t2,      2f

    ld.d            t3,      t0,      0    // next_bits
    addi.w          t1,      t7,      -48  // shift_bits = cnt + 16 (- 64)
    nor             t3,      t3,      t3
    sub.w           t2,      zero,    t1
    revb.d          t3,      t3            // next_bits = bswap(next_bits)
    srli.w          t2,      t2,      3    // num_bytes_read
    srl.d           t3,      t3,      t1   // next_bits >>= (shift_bits & 63)
    b               3f
1:
    addi.w          t3,      t7,      -48
    srl.d           t3,      t3,      t3   // pad with ones
    b               4f
2:
    bgeu            t0,      t1,      1b
    ld.d            t3,      t1,      -8   // next_bits
    sub.w           t2,      t2,      t1
    sub.w           t1,      t1,      t0   // num_bytes_left
    slli.w          t2,      t2,      3
    srl.d           t3,      t3,      t2
    addi.w          t2,      t7,      -48
    nor             t3,      t3,      t3
    sub.w           t4,      zero,    t2
    revb.d          t3,      t3
    srli.w          t4,      t4,      3
    srl.d           t3,      t3,      t2
    sltu            t2,      t1,      t4
    maskeqz         t1,      t1,      t2
    masknez         t2,      t4,      t2
    or              t2,      t2,      t1   // num_bytes_read
3:
    slli.w          t1,      t2,      3
    add.d           t0,      t0,      t2
    add.w           t7,      t7,      t1   // cnt += num_bits_read
    st.d            t0,      a0,      0
4:
    or              t6,      t6,      t3   // dif |= next_bits
9:
    st.w            t7,      a0,      28   // store cnt
    st.d            t6,      a0,      16   // store dif
    move            a0,      t8
endfunc

function msac_decode_bool_adapt_lsx
    ld.hu           a3,      a1,      0    // cdf[0] /f
    ld.w            t0,      a0,      24   // rng
    ld.d            t1,      a0,      16   // dif
    srli.w          t2,      t0,      8    // r >> 8
    srli.w          a7,      a3,      6
    mul.w           t2,      t2,      a7
    ld.w            a4,      a0,      32   // allow_update_cdf
    ld.w            a5,      a0,      28   // cnt
    srli.w          t2,      t2,      1
    addi.w          t2,      t2,      4    // v
    slli.d          t3,      t2,      48   // vw
    sltu            t4,      t1,      t3
    move            t8,      t4            // bit
    xori            t4,      t4,      1
    maskeqz         t6,      t3,      t4   // if (ret) vw
    sub.d           t6,      t1,      t6   // dif
    slli.w          t5,      t2,      1
    sub.w           t5,      t0,      t5   // r - 2v
    maskeqz         t7,      t5,      t4   // if (ret) r - 2v
    add.w           t5,      t2,      t7   // v(rng)
    beqz            a4,      .renorm

    // update_cdf
    ld.hu           t0,      a1,      2    // cdf[1]
    srli.w          t1,      t0,      4
    addi.w          t1,      t1,      4    // rate
    sltui           t2,      t0,      32   // count < 32
    add.w           t0,      t0,      t2   // count + (count < 32)
    sub.w           a3,      a3,      t8   // cdf[0] -= bit
    slli.w          t4,      t8,      15
    sub.w           t7,      a3,      t4   // cdf[0] - bit - 32768
    sra.w           t7,      t7,      t1   // (cdf[0] - bit - 32768) >> rate
    sub.w           t7,      a3,      t7   // cdf[0]
    st.h            t7,      a1,      0
    st.h            t0,      a1,      2

.renorm:
    clz.w           t4,      t5            // d
    xori            t4,      t4,      16   // d
    sll.d           t6,      t6,      t4
    sll.w           t5,      t5,      t4
    sub.w           t7,      a5,      t4   // cnt-d
    st.w            t5,      a0,      24   // store rng
    bgeu            a5,      t4,      9f

    // refill
    ld.d            t0,      a0,      0    // buf_pos
    ld.d            t1,      a0,      8    // buf_end
    addi.d          t2,      t0,      8
    bltu            t1,      t2,      2f

    ld.d            t3,      t0,      0    // next_bits
    addi.w          t1,      t7,      -48  // shift_bits = cnt + 16 (- 64)
    nor             t3,      t3,      t3
    sub.w           t2,      zero,    t1
    revb.d          t3,      t3            // next_bits = bswap(next_bits)
    srli.w          t2,      t2,      3    // num_bytes_read
    srl.d           t3,      t3,      t1   // next_bits >>= (shift_bits & 63)
    b               3f
1:
    addi.w          t3,      t7,      -48
    srl.d           t3,      t3,      t3   // pad with ones
    b               4f
2:
    bgeu            t0,      t1,      1b
    ld.d            t3,      t1,      -8   // next_bits
    sub.w           t2,      t2,      t1
    sub.w           t1,      t1,      t0   // num_bytes_left
    slli.w          t2,      t2,      3
    srl.d           t3,      t3,      t2
    addi.w          t2,      t7,      -48
    nor             t3,      t3,      t3
    sub.w           t4,      zero,    t2
    revb.d          t3,      t3
    srli.w          t4,      t4,      3
    srl.d           t3,      t3,      t2
    sltu            t2,      t1,      t4
    maskeqz         t1,      t1,      t2
    masknez         t2,      t4,      t2
    or              t2,      t2,      t1   // num_bytes_read
3:
    slli.w          t1,      t2,      3
    add.d           t0,      t0,      t2
    add.w           t7,      t7,      t1   // cnt += num_bits_read
    st.d            t0,      a0,      0
4:
    or              t6,      t6,      t3   // dif |= next_bits
9:
    st.w            t7,      a0,      28   // store cnt
    st.d            t6,      a0,      16   // store dif
    move            a0,      t8
endfunc

.macro HI_TOK allow_update_cdf
.\allow_update_cdf\()_hi_tok_lsx_start:
.if \allow_update_cdf == 1
    ld.hu        a4,    a1,    0x06 // cdf[3]
.endif
    vor.v        vr1,   vr0,   vr0
    vsrli.h      vr1,   vr1,   0x06 // cdf[val] >> EC_PROB_SHIFT
    vstelm.h     vr2,   sp,    0, 0 // -0x1a
    vand.v       vr2,   vr2,   vr4  // (8 x rng) & 0xff00
    vslli.h      vr1,   vr1,   0x07
    vmuh.hu      vr1,   vr1,   vr2
    vadd.h       vr1,   vr1,   vr5 // v += EC_MIN_PROB/* 4 */ * ((unsigned)n_symbols/* 3 */ - val);
    vst          vr1,   sp,    0x02 // -0x18
    vssub.hu     vr1,   vr1,   vr3 // v - c
    vseqi.h      vr1,   vr1,   0
.if \allow_update_cdf == 1
    addi.d       t4,    a4,    0x50
    srli.d       t4,    t4,    0x04
    sltui        t7,    a4,    32
    add.w        a4,    a4,    t7

    vreplgr2vr.h vr7,   t4
    vavgr.hu     vr9,   vr8,   vr1
    vsub.h       vr9,   vr9,   vr0
    vsub.h       vr0,   vr0,   vr1
    vsra.h       vr9,   vr9,   vr7
    vadd.h       vr0,   vr0,   vr9
    vstelm.d     vr0,   a1,    0,  0
    st.h         a4,    a1,    0x06
.endif
    vmsknz.b     vr7,   vr1
    movfr2gr.s   t4,    f7
    ctz.w        t4,    t4 // loop_times * 2
    addi.d       t7,    t4,    2
    ldx.hu       t6,    sp,    t4  // u
    ldx.hu       t5,    sp,    t7  // v
    addi.w       t3,    t3,    0x05
    addi.w       t4,    t4,   -0x05 // if t4 == 3, continue
    sub.w        t6,    t6,    t5   // u - v , rng for ctx_norm
    slli.d       t5,    t5,    0x30 //  (ec_win)v << (EC_WIN_SIZE - 16)
    sub.d        t1,    t1,    t5   //  s->dif - ((ec_win)v << (EC_WIN_SIZE - 16))
    // Init ctx_norm  param
    clz.w        t7,    t6
    xori         t7,    t7,    0x1f
    xori         t7,    t7,    0x0f //  d = 15 ^ (31 ^ clz(rng));
    sll.d        t1,    t1,    t7   //  dif << d
    sll.d        t6,    t6,    t7   //  rng << d
    // update vr2 8 x rng
    vreplgr2vr.h vr2,   t6
    vreplvei.h   vr2,   vr2,   0
    st.w         t6,    a0,    0x18 // store rng
    move         t0,    t2
    sub.w        t2,    t2,    t7   // cnt - d
    bgeu         t0,    t7,    .\allow_update_cdf\()_hi_tok_lsx_ctx_norm_end     // if ((unsigned)cnt < (unsigned)d)  goto ctx_norm_end
    // Step into ctx_fill
    ld.d         t5,    a0,    0x00 // buf_pos
    ld.d         t6,    a0,    0x08 // end_pos
    addi.d       t7,    t5,    0x08 // buf_pos + 8
    sub.d        t7,    t7,    t6   // (buf_pos + 8) - end_pos
    blt          zero,  t7,    .\allow_update_cdf\()_hi_tok_lsx_ctx_refill_eob
    // (end_pos - buf_pos) >= 8
    ld.d         t6,    t5,    0x00 // load buf_pos[0]~buf_pos[7]
    addi.w       t7,    t2,   -0x30 // cnt - 0x30
    nor          t6,    t6,    t6   // not buf data
    revb.d       t6,    t6          // Byte reversal
    srl.d        t6,    t6,    t7   // Replace left shift with right shift
    sub.w        t7,    zero,  t7   // neg
    srli.w       t7,    t7,    0x03 // Loop times
    or           t1,    t1,    t6   // dif |= (ec_win)(*buf_pos++ ^ 0xff) << c
    b            .\allow_update_cdf\()_hi_tok_lsx_ctx_refill_end
.\allow_update_cdf\()_hi_tok_lsx_ctx_refill_eob:
    bge          t5,    t6,    .\allow_update_cdf\()_hi_tok_lsx_ctx_refill_one
    // end_pos - buf_pos < 8 && buf_pos < end_pos
    ld.d         t0,    t6,   -0x08
    slli.d       t7,    t7,    0x03
    srl.d        t6,    t0,    t7   // Retrieve the buf data and remove the excess data
    addi.w       t7,    t2,   -0x30 // cnt - 0x30
    nor          t6,    t6,    t6   // not
    revb.d       t6,    t6          // Byte reversal
    srl.d        t6,    t6,    t7   // Replace left shift with right shift
    sub.w        t7,    zero,  t7   // neg
    or           t1,    t1,    t6   // dif |= (ec_win)(*buf_pos++ ^ 0xff) << c
    ld.d         t6,    a0,    0x08 // end_pos
    srli.w       t7,    t7,    0x03 // Loop times
    sub.d        t6,    t6,    t5   // end_pos - buf_pos
    slt          t0,    t6,    t7
    maskeqz      a3,    t6,    t0   // min(loop_times, end_pos - buf_pos)
    masknez      t0,    t7,    t0
    or           t7,    a3,    t0
    b            .\allow_update_cdf\()_hi_tok_lsx_ctx_refill_end
.\allow_update_cdf\()_hi_tok_lsx_ctx_refill_one:
    // buf_pos >= end_pos
    addi.w       t7,    t2,   -0x10
    andi         t7,    t7,    0xf
    nor          t0,    zero,  zero
    srl.d        t0,    t0,    t7
    or           t1,    t1,    t0 // dif |= ~(~(ec_win)0xff << c);
    b            .\allow_update_cdf\()_hi_tok_lsx_ctx_norm_end
.\allow_update_cdf\()_hi_tok_lsx_ctx_refill_end:
    add.d        t5,    t5,    t7        // buf_pos + Loop_times
    st.d         t5,    a0,    0x00      // Store buf_pos
    alsl.w       t2,    t7,    t2,  0x03 // update cnt
.\allow_update_cdf\()_hi_tok_lsx_ctx_norm_end:
    srli.d       t7,    t1,    0x30
    vreplgr2vr.h vr3,   t7        // broadcast the high 16 bits of dif
    add.w        t3,    t4,    t3 // update control parameter
    beqz         t3,    .\allow_update_cdf\()_hi_tok_lsx_end // control loop for at most 4 times.
    blt          zero,  t4,    .\allow_update_cdf\()_hi_tok_lsx_start // tok_br == 3
.\allow_update_cdf\()_hi_tok_lsx_end:
    addi.d       t3,    t3,    0x1e
    st.d         t1,    a0,    0x10 // store dif
    st.w         t2,    a0,    0x1c // store cnt
    srli.w       a0,    t3,    0x01 // tok
    addi.d       sp,    sp,    0x1a
.endm

/**
 * @param unsigned dav1d_msac_decode_hi_tok_c(MsacContext *const s, uint16_t *const cdf)
 * * Reg Alloction
 * * vr0: cdf;
 * * vr1: temp;
 * * vr2: rng;
 * * vr3: dif;
 * * vr4: const 0xff00ff00...ff00ff00;
 * * vr5: const 0x0004080c;
 * * vr6: const 0;
 * * t0: allow_update_cdf, tmp;
 * * t1: dif;
 * * t2: cnt;
 * * t3: 0xffffffe8, outermost control parameter;
 * * t4: loop time
 * * t5: v, buf_pos, temp;
 * * t6: u, rng, end_pos, buf, temp;
 * * t7: temp;
 */
function msac_decode_hi_tok_lsx
    fld.d     f0,    a1,   0    // Load cdf[0]~cdf[3]
    vldrepl.h vr2,   a0,   0x18 //  8 x rng, assert(rng <= 65535U), only the lower 16 bits are valid
    vldrepl.h vr3,   a0,   0x16 // broadcast the high 16 bits of dif, c = s->dif >> (EC_WIN_SIZE - 16)
    ld.w      t0,    a0,   0x20 // allow_update_cdf
    la.local  t7,    ph_0xff00
    vld       vr4,   t7,   0x00 // 0xff00ff00...ff00ff00
    la.local  t7,    min_prob
    vld       vr5,   t7,   12 * 2 // 0x0004080c
    vxor.v    vr6,   vr6,  vr6    // const 0
    ld.d      t1,    a0,   0x10   // dif
    ld.w      t2,    a0,   0x1c   // cnt
    orn       t3,    t3,   t3
    srli.d    t3,    t3,   32
    addi.d    t3,    t3,  -0x17 // 0xffffffe8
    vseq.h    vr8,   vr8,  vr8
    addi.d    sp,    sp,  -0x1a // alloc stack
    beqz      t0,    .hi_tok_lsx_no_update_cdf
    HI_TOK 1
    jirl      zero,  ra,   0x0
.hi_tok_lsx_no_update_cdf:
    HI_TOK 0
endfunc

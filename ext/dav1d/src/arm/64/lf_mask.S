/*
 * Copyright © 2026, VideoLAN and dav1d authors
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

.macro LC_PTR_LOOP label
        add             x0,  x0,  x1,  lsl #1
        subs            w3,  w3,  #1
        b.ne            \label
        ret
.endm

// void fill_levelcache_block(uint8_t (*level_cache_ptr)[2], ptrdiff_t b_stride,
//                            int bw, int bh, unsigned v0, unsigned v1)
function fill_levelcache_block_neon, export=1
        orr             w4,  w4,  w5,  lsl #8  // v0 | v1
        sub             w5,  w2,  #1
        tst             w5,  w2                // bw & (bw - 1)
        b.ne            0f                     // handle non power of two

        movrel          x6,  fill_levelcache_block_tbl
        clz             w2,  w2
        sub             w2,  w2,  #26          // 5 - ulog2(bw)
        ldrsw           x2,  [x6, x2, lsl #2]
        add             x6,  x6,  x2
        br              x6

// scalar, row to fill in [x0,x6)
0:
        mov             x5,  x0
        add             x6,  x0,  w2,  sxtw #1
3:
        strh            w4,  [x5], #2
        cmp             x5,  x6
        b.ne            3b

        LC_PTR_LOOP 0b

10:
        AARCH64_VALID_JUMP_TARGET
1:
        strh            w4,  [x0]
        LC_PTR_LOOP 1b

20:
        AARCH64_VALID_JUMP_TARGET
        orr             w4,  w4,  w4,  lsl #16
2:
        str             w4,  [x0]
        LC_PTR_LOOP 2b

40:
        AARCH64_VALID_JUMP_TARGET
        orr             w4,  w4,  w4,  lsl #16
        orr             x4,  x4,  x4,  lsl #32
4:
        str             x4,  [x0]
        LC_PTR_LOOP 4b

80:
        AARCH64_VALID_JUMP_TARGET
        dup             v0.8h,  w4
8:
        str             q0,  [x0]
        LC_PTR_LOOP 8b

160:
        AARCH64_VALID_JUMP_TARGET
        dup             v0.8h,  w4
16:
        stp             q0,  q0,  [x0]
        LC_PTR_LOOP 16b

320:
        AARCH64_VALID_JUMP_TARGET
        dup             v0.8h, w4
32:
        stp             q0,  q0,  [x0]
        stp             q0,  q0,  [x0, #32]
        LC_PTR_LOOP 32b

endfunc

jumptable fill_levelcache_block_tbl
        .word 320b - fill_levelcache_block_tbl
        .word 160b - fill_levelcache_block_tbl
        .word 80b  - fill_levelcache_block_tbl
        .word 40b  - fill_levelcache_block_tbl
        .word 20b  - fill_levelcache_block_tbl
        .word 10b  - fill_levelcache_block_tbl
endjumptable

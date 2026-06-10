/******************************************************************************
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2024, Nathan Egge
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
 *****************************************************************************/

#include "src/riscv/asm.S"

function blend_vl512_16bpc_rvv, export=1, ext=zbb
  srai t0, a3, 3
  ctz t0, t0
  addi t0, t0, 0xc6
  j L(blend_epilog)
endfunc

function blend_vl256_16bpc_rvv, export=1, ext=zbb
  ctz t0, a3
  addi t0, t0, 0xc4
  j L(blend_epilog)
endfunc

function blend_16bpc_rvv, export=1, ext="v,zbb"
  ctz t0, a3
  addi t0, t0, 0xc5
L(blend_epilog):
  csrw vxrm, zero
  andi t0, t0, 0xc7
  ori t0, t0, 8
  add a6, a3, a3
  vsetvl zero, a3, t0
1:
  addi a4, a4, -2
  vle8.v v24, (a5)
  add a5, a5, a3
  vle8.v v28, (a5)
  add a5, a5, a3
  vle16.v v8, (a2)
  add a2, a2, a6
  vle16.v v12, (a2)
  add a2, a2, a6
  vzext.vf2 v16, v24
  vzext.vf2 v20, v28
  vle16.v v0, (a0)
  add t0, a0, a1
  vle16.v v4, (t0)
  vrsub.vx v16, v16, x0
  vrsub.vx v20, v20, x0
  vsub.vv  v8, v0, v8
  vsub.vv  v12, v4, v12
  vsll.vi  v16, v16, 9
  vsll.vi  v20, v20, 9
  vsmul.vv v8, v8, v16
  vsmul.vv v12, v12, v20
  vadd.vv  v0, v0, v8
  vadd.vv  v4, v4, v12
  vse16.v v0, (a0)
  vse16.v v4, (t0)
  add a0, t0, a1
  bnez a4, 1b
  ret
endfunc

function blend_v_vl512_16bpc_rvv, export=1, ext=zbb
  srai t0, a3, 3
  ctz t0, t0
  addi t0, t0, 0xc6
  j L(blend_v_epilog)
endfunc

function blend_v_vl256_16bpc_rvv, export=1, ext=zbb
  srai t0, a3, 2
  ctz t0, t0
  addi t0, t0, 0xc6
  j L(blend_v_epilog)
endfunc

function blend_v_16bpc_rvv, export=1, ext="v,zbb"
  ctz t0, a3
  addi t0, t0, 0xc5
L(blend_v_epilog):
  andi t0, t0, 0xc7
  ori t0, t0, 8
  srai t1, a3, 2
  sub t1, a3, t1
  vsetvl zero, t1, t0
  csrw vxrm, zero
  la t1, dav1d_obmc_masks
  add t1, t1, a3
  vle8.v v20, (t1)
  vzext.vf2 v16, v20
  add a3, a3, a3
  vrsub.vx v20, v16, x0
  vsll.vi v20, v20, 9
1:
  addi a4, a4, -2
  vle16.v v8, (a2)
  add a2, a2, a3
  vle16.v v12, (a2)
  add a2, a2, a3
  vle16.v v0, (a0)
  add t0, a0, a1
  vle16.v v4, (t0)
  vsub.vv v8, v0, v8
  vsub.vv v12, v4, v12
  vsmul.vv v8, v8, v20
  vsmul.vv v12, v12, v20
  vadd.vv  v0, v0, v8
  vadd.vv  v4, v4, v12
  vse16.v v0, (a0)
  vse16.v v4, (t0)
  add a0, t0, a1
  bnez a4, 1b
  ret
endfunc

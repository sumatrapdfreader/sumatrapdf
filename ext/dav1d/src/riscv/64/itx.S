/******************************************************************************
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2023, Nathan Egge
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

function inv_txfm_add_4x4_rvv, export=1, ext=v
  csrw vxrm, zero

  vsetivli zero, 4, e16, mf2, ta, ma
  vle16.v v0, (a2)
  addi t0, a2, 8
  vle16.v v1, (t0)
  addi t0, t0, 8
  vle16.v v2, (t0)
  addi t0, t0, 8
  vle16.v v3, (t0)

  jalr t0, a4

  vmv.v.x v4, zero

  vsseg4e16.v v0, (a2)
  vle16.v v0, (a2)
  vse16.v v4, (a2)
  addi t0, a2, 8
  vle16.v v1, (t0)
  vse16.v v4, (t0)
  addi t0, t0, 8
  vle16.v v2, (t0)
  vse16.v v4, (t0)
  addi t0, t0, 8
  vle16.v v3, (t0)
  vse16.v v4, (t0)

  jalr t0, a5

  vssra.vi v0, v0, 4
  vssra.vi v1, v1, 4
  vssra.vi v2, v2, 4
  vssra.vi v3, v3, 4

itx_4x4_end:
  vsetvli zero, zero, e8, mf4, ta, ma
  vle8.v v4, (a0)
  add t0, a0, a1
  vle8.v v5, (t0)
  add t0, t0, a1
  vle8.v v6, (t0)
  add t0, t0, a1
  vle8.v v7, (t0)

  vwaddu.wv v0, v0, v4
  vwaddu.wv v1, v1, v5
  vwaddu.wv v2, v2, v6
  vwaddu.wv v3, v3, v7

  vsetvli zero, zero, e16, mf2, ta, ma
  vmax.vx v0, v0, zero
  vmax.vx v1, v1, zero
  vmax.vx v2, v2, zero
  vmax.vx v3, v3, zero

  vsetvli zero, zero, e8, mf4, ta, ma

  vnclipu.wi v4, v0, 0
  vnclipu.wi v5, v1, 0
  vnclipu.wi v6, v2, 0
  vnclipu.wi v7, v3, 0

  vse8.v v4, (a0)
  add a0, a0, a1
  vse8.v v5, (a0)
  add a0, a0, a1
  vse8.v v6, (a0)
  add a0, a0, a1
  vse8.v v7, (a0)

  ret
endfunc

function inv_identity_e16_x4_rvv, export=1, ext=v
  li t1, (5793-4096)*8
  vsmul.vx v4, v0, t1
  vsmul.vx v5, v1, t1
  vsmul.vx v6, v2, t1
  vsmul.vx v7, v3, t1

  vsadd.vv v0, v0, v4
  vsadd.vv v1, v1, v5
  vsadd.vv v2, v2, v6
  vsadd.vv v3, v3, v7

  jr t0
endfunc

.macro iwht_4
  vadd.vv v0, v0, v1
  vsub.vv v5, v2, v3
  vsub.vv v4, v0, v5
  vsra.vi v4, v4, 1
  vsub.vv v2, v4, v1
  vsub.vv v1, v4, v3
  vadd.vv v3, v5, v2
  vsub.vv v0, v0, v1
.endm

.macro idct_4 o0, o1, o2, o3
  li t1, 2896
  li t2, 1567
  li t3, 3784

  vwmul.vx v16, \o0, t1
  vwmul.vx v18, \o0, t1
  vwmacc.vx v16, t1, \o2
  neg t1, t1
  vwmacc.vx v18, t1, \o2

  vwmul.vx v20, \o1, t3
  neg t3, t3
  vwmul.vx v22, \o1, t2
  vwmacc.vx v20, t2, \o3
  vwmacc.vx v22, t3, \o3

  vnclip.wi v16, v16, 12
  vnclip.wi v18, v18, 12
  vnclip.wi v20, v20, 12
  vnclip.wi v22, v22, 12

  vsadd.vv \o0, v16, v20
  vsadd.vv \o1, v18, v22
  vssub.vv \o2, v18, v22
  vssub.vv \o3, v16, v20
.endm

.macro iadst_4 o0, o1, o2, o3, lm2, lm
  li t1, 1321
  li t2, 3803
  li t3, 2482

  vwmul.vx v16, v0, t1
  vwmul.vx v18, v0, t3
  neg t1, t1
  vwmacc.vx v16, t2, v2
  vwmacc.vx v18, t1, v2
  neg t2, t2
  vwmacc.vx v16, t3, v3
  vwmacc.vx v18, t2, v3

  vwsub.vv v20,  v0, v2
  vwadd.wv v20, v20, v3

  li t1, 3344
  vwmul.vx v22, v1, t1

  vsetvli zero, zero, e32, \lm2, ta, ma

  vmul.vx v20, v20, t1

  vadd.vv v24, v16, v18
  vadd.vv v16, v16, v22
  vadd.vv v18, v18, v22
  vsub.vv v22, v24, v22

  vsetvli zero, zero, e16, \lm, ta, ma

  vnclip.wi \o0, v16, 12
  vnclip.wi \o1, v18, 12
  vnclip.wi \o2, v20, 12
  vnclip.wi \o3, v22, 12
.endm

function inv_dct_e16_x4_rvv, export=1, ext=v
  idct_4 v0, v1, v2, v3
  jr t0
endfunc

function inv_adst_e16_x4_rvv, export=1, ext=v
  iadst_4 v0, v1, v2, v3, m1, mf2
  jr t0
endfunc

function inv_flipadst_e16_x4_rvv, export=1, ext=v
  iadst_4 v3, v2, v1, v0, m1, mf2
  jr t0
endfunc

function inv_adst_e16_x4w_rvv, export=1, ext=v
  iadst_4 v0, v1, v2, v3, m2, m1
  jr t0
endfunc

function inv_flipadst_e16_x4w_rvv, export=1, ext=v
  iadst_4 v3, v2, v1, v0, m2, m1
  jr t0
endfunc

function inv_txfm_add_wht_wht_4x4_8bpc_rvv, export=1, ext=v
  csrw vxrm, zero

  vsetivli zero, 4, e16, mf2, ta, ma
  vle16.v v0, (a2)
  addi t0, a2, 8
  vle16.v v1, (t0)
  addi t0, t0, 8
  vle16.v v2, (t0)
  addi t0, t0, 8
  vle16.v v3, (t0)

  vsra.vi v0, v0, 2
  vsra.vi v1, v1, 2
  vsra.vi v2, v2, 2
  vsra.vi v3, v3, 2

  iwht_4

  vmv.v.x v4, zero

  vsseg4e16.v v0, (a2)
  vle16.v v0, (a2)
  vse16.v v4, (a2)
  addi t0, a2, 8
  vle16.v v1, (t0)
  vse16.v v4, (t0)
  addi t0, t0, 8
  vle16.v v2, (t0)
  vse16.v v4, (t0)
  addi t0, t0, 8
  vle16.v v3, (t0)
  vse16.v v4, (t0)

  iwht_4

  j itx_4x4_end
endfunc

.macro def_fn_4x4 txfm1, txfm2
function inv_txfm_add_\txfm1\()_\txfm2\()_4x4_8bpc_rvv, export=1, ext=v
.ifc \txfm1\()_\txfm2, dct_dct
  beqz a3, 1f
.endif
  la a4, inv_\txfm1\()_e16_x4_rvv
  la a5, inv_\txfm2\()_e16_x4_rvv
  j inv_txfm_add_4x4_rvv
.ifc \txfm1\()_\txfm2, dct_dct
1:
  csrw vxrm, zero
  vsetivli zero, 4, e16, mf2, ta, ma
  ld t2, (a2)
  li t1, 2896*8
  vmv.v.x v0, t2
  vsmul.vx v0, v0, t1
  sd x0, (a2)
  vsmul.vx v0, v0, t1
  vssra.vi v0, v0, 4
  vmv.v.v v1, v0
  vmv.v.v v2, v0
  vmv.v.v v3, v0
  j itx_4x4_end
.endif
endfunc
.endm

def_fn_4x4 dct, dct
def_fn_4x4 identity, identity
def_fn_4x4 dct, adst
def_fn_4x4 dct, flipadst
def_fn_4x4 dct, identity
def_fn_4x4 adst, dct
def_fn_4x4 adst, adst
def_fn_4x4 adst, flipadst
def_fn_4x4 flipadst, dct
def_fn_4x4 flipadst, adst
def_fn_4x4 flipadst, flipadst
def_fn_4x4 identity, dct
def_fn_4x4 adst, identity
def_fn_4x4 flipadst, identity
def_fn_4x4 identity, adst
def_fn_4x4 identity, flipadst

.macro def_fn_8x8_base variant
function inv_txfm_\variant\()add_8x8_rvv, export=1, ext=v
  csrw vxrm, zero

  vsetivli zero, 8, e16, m1, ta, ma
  vle16.v v0, (a2)
  addi t0, a2, 16
  vle16.v v1, (t0)
  addi t0, t0, 16
  vle16.v v2, (t0)
  addi t0, t0, 16
  vle16.v v3, (t0)
  addi t0, t0, 16
  vle16.v v4, (t0)
  addi t0, t0, 16
  vle16.v v5, (t0)
  addi t0, t0, 16
  vle16.v v6, (t0)
  addi t0, t0, 16
  vle16.v v7, (t0)

.ifc \variant, identity_
  // The identity vsadd.vv and downshift vssra.vi 1 cancel out

  j L(itx_8x8_epilog)
.else
  jalr t0, a4

  vssra.vi v0, v0, 1
  vssra.vi v1, v1, 1
  vssra.vi v2, v2, 1
  vssra.vi v3, v3, 1
  vssra.vi v4, v4, 1
  vssra.vi v5, v5, 1
  vssra.vi v6, v6, 1
  vssra.vi v7, v7, 1

L(itx_8x8_epilog):
  vsseg8e16.v v0, (a2)
  vle16.v v0, (a2)
  addi t0, a2, 16
  vle16.v v1, (t0)
  addi t0, t0, 16
  vle16.v v2, (t0)
  addi t0, t0, 16
  vle16.v v3, (t0)
  addi t0, t0, 16
  vle16.v v4, (t0)
  addi t0, t0, 16
  vle16.v v5, (t0)
  addi t0, t0, 16
  vle16.v v6, (t0)
  addi t0, t0, 16
  vle16.v v7, (t0)

  jalr t0, a5

  vssra.vi v0, v0, 4
  vssra.vi v1, v1, 4
  vssra.vi v2, v2, 4
  vssra.vi v3, v3, 4
  vssra.vi v4, v4, 4
  vssra.vi v5, v5, 4
  vssra.vi v6, v6, 4
  vssra.vi v7, v7, 4

  li t1, 64
  vsetvli zero, t1, e16, m8, ta, ma
  vmv.v.x v8, zero
  vse16.v v8, (a2)

itx_8x8_end:
  vsetivli zero, 8, e8, mf2, ta, ma
  vle8.v v8, (a0)
  add t0, a0, a1
  vle8.v v9, (t0)
  add t0, t0, a1
  vle8.v v10, (t0)
  add t0, t0, a1
  vle8.v v11, (t0)
  add t0, t0, a1
  vle8.v v12, (t0)
  add t0, t0, a1
  vle8.v v13, (t0)
  add t0, t0, a1
  vle8.v v14, (t0)
  add t0, t0, a1
  vle8.v v15, (t0)

  vwaddu.wv v0, v0, v8
  vwaddu.wv v1, v1, v9
  vwaddu.wv v2, v2, v10
  vwaddu.wv v3, v3, v11
  vwaddu.wv v4, v4, v12
  vwaddu.wv v5, v5, v13
  vwaddu.wv v6, v6, v14
  vwaddu.wv v7, v7, v15

  vsetvli zero, zero, e16, m1, ta, ma
  vmax.vx v0, v0, zero
  vmax.vx v1, v1, zero
  vmax.vx v2, v2, zero
  vmax.vx v3, v3, zero
  vmax.vx v4, v4, zero
  vmax.vx v5, v5, zero
  vmax.vx v6, v6, zero
  vmax.vx v7, v7, zero

  vsetvli zero, zero, e8, mf2, ta, ma

  vnclipu.wi v8, v0, 0
  vnclipu.wi v9, v1, 0
  vnclipu.wi v10, v2, 0
  vnclipu.wi v11, v3, 0
  vnclipu.wi v12, v4, 0
  vnclipu.wi v13, v5, 0
  vnclipu.wi v14, v6, 0
  vnclipu.wi v15, v7, 0

  vse8.v v8, (a0)
  add a0, a0, a1
  vse8.v v9, (a0)
  add a0, a0, a1
  vse8.v v10, (a0)
  add a0, a0, a1
  vse8.v v11, (a0)
  add a0, a0, a1
  vse8.v v12, (a0)
  add a0, a0, a1
  vse8.v v13, (a0)
  add a0, a0, a1
  vse8.v v14, (a0)
  add a0, a0, a1
  vse8.v v15, (a0)

  ret
.endif
endfunc
.endm

def_fn_8x8_base identity_
def_fn_8x8_base

function inv_identity_e16_x8_rvv, export=1, ext=v
  vsadd.vv v0, v0, v0
  vsadd.vv v1, v1, v1
  vsadd.vv v2, v2, v2
  vsadd.vv v3, v3, v3
  vsadd.vv v4, v4, v4
  vsadd.vv v5, v5, v5
  vsadd.vv v6, v6, v6
  vsadd.vv v7, v7, v7

  jr t0
endfunc

.macro idct_8 o0, o1, o2, o3, o4, o5, o6, o7
  idct_4 \o0, \o2, \o4, \o6

  li t1, 799
  li t2, 4017
  li t3, 3406
  li t4, 2276

  vwmul.vx v22, \o1, t2
  neg t2, t2
  vwmul.vx v16, \o1, t1
  vwmacc.vx v22, t1, \o7
  vwmacc.vx v16, t2, \o7

  vwmul.vx v20, \o5, t4
  neg t4, t4
  vwmul.vx v18, \o5, t3
  vwmacc.vx v20, t3, \o3
  vwmacc.vx v18, t4, \o3

  vnclip.wi v16, v16, 12
  vnclip.wi v18, v18, 12
  vnclip.wi v20, v20, 12
  vnclip.wi v22, v22, 12

  vssub.vv \o7, v22, v20
  vsadd.vv v22, v22, v20
  vssub.vv \o1, v16, v18
  vsadd.vv v16, v16, v18

  li t2, 2896

  vwmul.vx v18, \o7, t2
  vwmul.vx v20, \o7, t2
  vwmacc.vx v20, t2, \o1
  neg t2, t2
  vwmacc.vx v18, t2, \o1

  vnclip.wi v18, v18, 12
  vnclip.wi v20, v20, 12

  vssub.vv \o7, \o0, v22
  vsadd.vv \o0, \o0, v22
  vssub.vv v17, \o2, v20
  vsadd.vv \o1, \o2, v20
  vssub.vv \o5, \o4, v18
  vsadd.vv \o2, \o4, v18
  vssub.vv \o4, \o6, v16
  vsadd.vv \o3, \o6, v16
  vmv.v.v \o6, v17
.endm

.macro iadst_8 o0, o1, o2, o3, o4, o5, o6, o7
  li t1, 4076
  li t2, 401
  li t3, 3612
  li t4, 1931
  li t5, 2598
  li t6, 3166

  vwmul.vx v16, v7, t1
  neg t1, t1
  vwmul.vx v18, v7, t2
  vwmacc.vx v16, t2, v0
  vwmacc.vx v18, t1, v0

  vwmul.vx v20, v5, t3
  neg t3, t3
  vwmul.vx v22, v5, t4
  vwmacc.vx v20, t4, v2
  vwmacc.vx v22, t3, v2

  vwmul.vx v24, v3, t5
  neg t5, t5
  vwmul.vx v26, v3, t6
  vwmacc.vx v24, t6, v4
  vwmacc.vx v26, t5, v4

  li t2, 1189
  li t3, 3920
  li t4, 1567
  li t5, 3784
  li t6, 2896

  vwmul.vx v28, v1, t2
  neg t2, t2
  vwmul.vx v30, v1, t3
  vwmacc.vx v28, t3, v6
  vwmacc.vx v30, t2, v6

  vnclip.wi v16, v16, 12
  vnclip.wi v18, v18, 12
  vnclip.wi v20, v20, 12
  vnclip.wi v22, v22, 12
  vnclip.wi v24, v24, 12
  vnclip.wi v26, v26, 12
  vnclip.wi v28, v28, 12
  vnclip.wi v30, v30, 12

  vssub.vv  v4, v16, v24
  vsadd.vv v16, v16, v24
  vsadd.vv  v1, v18, v26
  vsadd.vv  v2, v20, v28
  vsadd.vv  v3, v22, v30
  vssub.vv  v5, v18, v26
  vssub.vv  v6, v20, v28
  vssub.vv v30, v22, v30

  vsadd.vv \o0, v16, v2
  vsadd.vv \o7,  v1, v3
  vssub.vv  v2, v16, v2
  vssub.vv  v3,  v1, v3

  vwmul.vx v16,  v4, t5
  vwmul.vx v18,  v4, t4
  vwmul.vx v20, v30, t5
  vwmul.vx v22, v30, t4
  vwmacc.vx v16, t4, v5
  neg t4, t4
  vwmacc.vx v22, t5, v6
  neg t5, t5
  vwmacc.vx v20, t4, v6
  vwmacc.vx v18, t5, v5

  vnclip.wi v16, v16, 12
  vnclip.wi v18, v18, 12
  vnclip.wi v20, v20, 12
  vnclip.wi v22, v22, 12

  vsadd.vv \o1, v16, v20
  vsadd.vv \o6, v18, v22
  vssub.vv v16, v16, v20
  vssub.vv v17, v18, v22

  vwmul.vx v18, v2, t6
  vwmul.vx v20, v2, t6
  vwmul.vx v22, v16, t6
  vwmul.vx v24, v16, t6
  vwmacc.vx v18, t6, v3
  vwmacc.vx v22, t6, v17
  neg t6, t6
  vwmacc.vx v20, t6, v3
  vwmacc.vx v24, t6, v17

  vnclip.wi \o3, v18, 12
  vnclip.wi \o4, v20, 12
  vnclip.wi \o2, v22, 12
  vnclip.wi \o5, v24, 12

  vmv.v.x v16, zero
  vssub.vv \o1, v16, \o1
  vssub.vv \o3, v16, \o3
  vssub.vv \o5, v16, \o5
  vssub.vv \o7, v16, \o7
.endm

function inv_dct_e16_x8_rvv, export=1, ext=v
  idct_8 v0, v1, v2, v3, v4, v5, v6, v7
  jr t0
endfunc

function inv_adst_e16_x8_rvv, export=1, ext=v
  iadst_8 v0, v1, v2, v3, v4, v5, v6, v7
  jr t0
endfunc

function inv_flipadst_e16_x8_rvv, export=1, ext=v
  iadst_8 v7, v6, v5, v4, v3, v2, v1, v0
  jr t0
endfunc

.macro def_fn_8x8 txfm1, txfm2
function inv_txfm_add_\txfm1\()_\txfm2\()_8x8_8bpc_rvv, export=1, ext=v
.ifc \txfm1\()_\txfm2, dct_dct
  beqz a3, 1f
.endif
  la a5, inv_\txfm2\()_e16_x8_rvv
.ifc \txfm1, identity
  j inv_txfm_identity_add_8x8_rvv
.else
  la a4, inv_\txfm1\()_e16_x8_rvv
  j inv_txfm_add_8x8_rvv
.endif
.ifc \txfm1\()_\txfm2, dct_dct
1:
  csrw vxrm, zero
  vsetivli zero, 8, e16, m1, ta, ma
  ld t2, (a2)
  li t1, 2896*8
  vmv.v.x v0, t2
  vsmul.vx v0, v0, t1
  sd x0, (a2)
  vssra.vi v0, v0, 1
  vsmul.vx v0, v0, t1
  vssra.vi v0, v0, 4
  vmv.v.v v1, v0
  vmv.v.v v2, v0
  vmv.v.v v3, v0
  vmv.v.v v4, v0
  vmv.v.v v5, v0
  vmv.v.v v6, v0
  vmv.v.v v7, v0
  j itx_8x8_end
.endif
endfunc
.endm

def_fn_8x8 dct, dct
def_fn_8x8 identity, identity
def_fn_8x8 dct, adst
def_fn_8x8 dct, flipadst
def_fn_8x8 dct, identity
def_fn_8x8 adst, dct
def_fn_8x8 adst, adst
def_fn_8x8 adst, flipadst
def_fn_8x8 flipadst, dct
def_fn_8x8 flipadst, adst
def_fn_8x8 flipadst, flipadst
def_fn_8x8 identity, dct
def_fn_8x8 adst, identity
def_fn_8x8 flipadst, identity
def_fn_8x8 identity, adst
def_fn_8x8 identity, flipadst

function inv_txfm_add_4x8_rvv, export=1, ext=v
  csrw vxrm, zero

  vsetivli zero, 8, e16, m1, ta, ma
  vle16.v v0, (a2)
  addi t0, a2, 16
  vle16.v v1, (t0)
  addi t0, t0, 16
  vle16.v v2, (t0)
  addi t0, t0, 16
  vle16.v v3, (t0)

  li t1, 2896*8
.irp i, 0, 1, 2, 3
  vsmul.vx v\i, v\i, t1
.endr

  jalr t0, a4

  vsseg4e16.v v0, (a2)

  vsetivli zero, 4, e16, mf2, ta, ma
  vmv.v.x v8, zero
  vle16.v v0, (a2)
  vse16.v v8, (a2)
.irp i, 1, 2, 3, 4, 5, 6, 7
  addi a2, a2, 8
  vle16.v v\i, (a2)
  vse16.v v8, (a2)
.endr

  jalr t0, a5

.irp i, 0, 1, 2, 3, 4, 5, 6, 7
  vssra.vi v\i, v\i, 4
.endr

  vsetvli zero, zero, e8, mf4, ta, ma
  vle8.v v8, (a0)
  add t0, a0, a1
  vle8.v v9, (t0)
.irp i, 10, 11, 12, 13, 14, 15
  add t0, t0, a1
  vle8.v v\i, (t0)
.endr

  vwaddu.wv v0, v0,  v8
  vwaddu.wv v1, v1,  v9
  vwaddu.wv v2, v2, v10
  vwaddu.wv v3, v3, v11
  vwaddu.wv v4, v4, v12
  vwaddu.wv v5, v5, v13
  vwaddu.wv v6, v6, v14
  vwaddu.wv v7, v7, v15

  vsetvli zero, zero, e16, mf2, ta, ma
.irp i, 0, 1, 2, 3, 4, 5, 6, 7
  vmax.vx v\i, v\i, zero
.endr

  vsetvli zero, zero, e8, mf4, ta, ma

  vnclipu.wi  v8, v0, 0
  vnclipu.wi  v9, v1, 0
  vnclipu.wi v10, v2, 0
  vnclipu.wi v11, v3, 0
  vnclipu.wi v12, v4, 0
  vnclipu.wi v13, v5, 0
  vnclipu.wi v14, v6, 0
  vnclipu.wi v15, v7, 0

  vse8.v v8, (a0)
.irp i, 9, 10, 11, 12, 13, 14, 15
  add a0, a0, a1
  vse8.v v\i, (a0)
.endr

  ret
endfunc

function inv_txfm_add_8x4_rvv, export=1, ext=v
  csrw vxrm, zero

  vsetivli zero, 4, e16, mf2, ta, ma
  vle16.v v0, (a2)
  addi t0, a2, 8
  vle16.v v1, (t0)
.irp i, 2, 3, 4, 5, 6, 7
  addi t0, t0, 8
  vle16.v v\i, (t0)
.endr

  li t1, 2896*8
.irp i, 0, 1, 2, 3, 4, 5, 6, 7
  vsmul.vx v\i, v\i, t1
.endr

  jalr t0, a4

  vsseg8e16.v v0, (a2)

  vsetivli zero, 8, e16, m1, ta, ma
  vmv.v.x v4, zero
  vle16.v v0, (a2)
  vse16.v v4, (a2)
.irp i, 1, 2, 3
  addi a2, a2, 16
  vle16.v v\i, (a2)
  vse16.v v4, (a2)
.endr

  jalr t0, a5

  vssra.vi v0, v0, 4
  vssra.vi v1, v1, 4
  vssra.vi v2, v2, 4
  vssra.vi v3, v3, 4

  vsetvli zero, zero, e8, mf2, ta, ma
  vle8.v v4, (a0)
  add t0, a0, a1
  vle8.v v5, (t0)
  add t0, t0, a1
  vle8.v v6, (t0)
  add t0, t0, a1
  vle8.v v7, (t0)

  vwaddu.wv v0, v0, v4
  vwaddu.wv v1, v1, v5
  vwaddu.wv v2, v2, v6
  vwaddu.wv v3, v3, v7

  vsetvli zero, zero, e16, m1, ta, ma
  vmax.vx v0, v0, zero
  vmax.vx v1, v1, zero
  vmax.vx v2, v2, zero
  vmax.vx v3, v3, zero

  vsetvli zero, zero, e8, mf2, ta, ma

  vnclipu.wi v4, v0, 0
  vnclipu.wi v5, v1, 0
  vnclipu.wi v6, v2, 0
  vnclipu.wi v7, v3, 0

  vse8.v v4, (a0)
  add a0, a0, a1
  vse8.v v5, (a0)
  add a0, a0, a1
  vse8.v v6, (a0)
  add a0, a0, a1
  vse8.v v7, (a0)

  ret
endfunc

/* Define symbols added in .if statement */
.equ dct, 1
.equ identity, 2
.equ adst, 3
.equ flipadst, 4

.macro def_fn_48 w, h, txfm1, txfm2
function inv_txfm_add_\txfm1\()_\txfm2\()_\w\()x\h\()_8bpc_rvv, export=1
.if \w == 4 && (\txfm1 == adst || \txfm1 == flipadst)
  la a4, inv_\txfm1\()_e16_x\w\()w_rvv
.else
  la a4, inv_\txfm1\()_e16_x\w\()_rvv
.endif
.if \h == 4 && (\txfm2 == adst || \txfm2 == flipadst)
  la a5, inv_\txfm2\()_e16_x\h\()w_rvv
.else
  la a5, inv_\txfm2\()_e16_x\h\()_rvv
.endif
  j inv_txfm_add_\w\()x\h\()_rvv
endfunc
.endm

.macro def_fns_48 w, h
def_fn_48 \w, \h, dct, dct
def_fn_48 \w, \h, identity, identity
def_fn_48 \w, \h, dct, adst
def_fn_48 \w, \h, dct, flipadst
def_fn_48 \w, \h, dct, identity
def_fn_48 \w, \h, adst, dct
def_fn_48 \w, \h, adst, adst
def_fn_48 \w, \h, adst, flipadst
def_fn_48 \w, \h, flipadst, dct
def_fn_48 \w, \h, flipadst, adst
def_fn_48 \w, \h, flipadst, flipadst
def_fn_48 \w, \h, identity, dct
def_fn_48 \w, \h, adst, identity
def_fn_48 \w, \h, flipadst, identity
def_fn_48 \w, \h, identity, adst
def_fn_48 \w, \h, identity, flipadst
.endm

def_fns_48 4, 8
def_fns_48 8, 4

function inv_identity_e16_x16_rvv, export=1, ext=v
  li t1, 2*(5793-4096)*8
.irp i, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  vsmul.vx v16, v\i, t1
  vsadd.vv v\i, v\i, v\i
  vsadd.vv v\i, v\i, v16
.endr
  jr t0
endfunc

function inv_dct_e16_x16_rvv, export=1, ext=v
  idct_8 v0, v2, v4, v6, v8, v10, v12, v14

  li t1, 401
  li t2, 4076
  li t3, 3166
  li t4, 2598

  vwmul.vx v30, v1, t2
  neg t2, t2
  vwmul.vx v16, v1, t1
  vwmacc.vx v30, t1, v15
  vwmacc.vx v16, t2, v15

  vwmul.vx v28, v9, t4
  neg t4, t4
  vwmul.vx v18, v9, t3
  vwmacc.vx v28, t3, v7
  vwmacc.vx v18, t4, v7

  li t1, 1931
  li t2, 3612
  li t3, 3920
  li t4, 1189

  vwmul.vx v26, v5, t2
  neg t2, t2
  vwmul.vx v20, v5, t1
  vwmacc.vx v26, t1, v11
  vwmacc.vx v20, t2, v11

  vwmul.vx v24, v13, t4
  neg t4, t4
  vwmul.vx v22, v13, t3
  vwmacc.vx v24, t3, v3
  vwmacc.vx v22, t4, v3

  li t2, 2896
  li t3, 1567
  li t4, 3784

  vnclip.wi v16, v16, 12
  vnclip.wi v18, v18, 12
  vnclip.wi v20, v20, 12
  vnclip.wi v22, v22, 12
  vnclip.wi v24, v24, 12
  vnclip.wi v26, v26, 12
  vnclip.wi v28, v28, 12
  vnclip.wi v30, v30, 12

  vssub.vv  v3, v16, v18
  vsadd.vv v16, v16, v18
  vssub.vv  v5, v22, v20
  vsadd.vv v22, v22, v20
  vssub.vv v11, v24, v26
  vsadd.vv v24, v24, v26
  vssub.vv v13, v30, v28
  vsadd.vv v30, v30, v28

  vwmul.vx v28, v13, t4
  neg t4, t4
  vwmul.vx v18, v13, t3
  vwmul.vx v26, v11, t3
  vwmacc.vx v28, t3, v3
  neg t3, t3
  vwmul.vx v20, v11, t4
  vwmacc.vx v18, t4, v3
  vwmacc.vx v20, t3, v5
  vwmacc.vx v26, t4, v5

  vnclip.wi v18, v18, 12
  vnclip.wi v20, v20, 12
  vnclip.wi v26, v26, 12
  vnclip.wi v28, v28, 12

  vssub.vv  v5, v18, v20
  vsadd.vv v18, v18, v20
  vssub.vv v11, v28, v26
  vsadd.vv v28, v28, v26

  vssub.vv  v7, v16, v22
  vsadd.vv v16, v16, v22
  vssub.vv  v9, v30, v24
  vsadd.vv v30, v30, v24

  vwmul.vx v20, v11, t2
  vwmul.vx v22,  v9, t2
  vwmul.vx v24,  v9, t2
  vwmul.vx v26, v11, t2
  vwmacc.vx v24, t2, v7
  vwmacc.vx v26, t2, v5
  neg t2, t2
  vwmacc.vx v20, t2, v5
  vwmacc.vx v22, t2, v7

  vnclip.wi v20, v20, 12
  vnclip.wi v22, v22, 12
  vnclip.wi v24, v24, 12
  vnclip.wi v26, v26, 12

  vssub.vv v15,  v0, v30
  vsadd.vv  v0,  v0, v30
  vssub.vv v17,  v2, v28
  vsadd.vv  v1,  v2, v28
  vssub.vv v13,  v4, v26
  vsadd.vv  v2,  v4, v26
  vssub.vv v19,  v6, v24
  vsadd.vv  v3,  v6, v24
  vssub.vv v11,  v8, v22
  vsadd.vv  v4,  v8, v22
  vsadd.vv  v5, v10, v20
  vssub.vv v10, v10, v20
  vssub.vv  v9, v12, v18
  vsadd.vv  v6, v12, v18
  vssub.vv  v8, v14, v16
  vsadd.vv  v7, v14, v16
  vmv.v.v v14, v17
  vmv.v.v v12, v19

  jr t0
endfunc

.macro iadst_16 o0, o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12, o13, o14, o15
  li t1, 4091
  li t2, 201
  li t3, 3973
  li t4, 995

  vwmul.vx v16, v15, t1
  neg t1, t1
  vwmul.vx v18, v15, t2
  vwmacc.vx v16, t2, v0
  vwmacc.vx v18, t1, v0

  vwmul.vx v20, v13, t3
  neg t3, t3
  vwmul.vx v22, v13, t4
  vwmacc.vx v20, t4, v2
  vwmacc.vx v22, t3, v2

  li t1, 3703
  li t2, 1751
  li t3, 3290
  li t4, 2440

  vwmul.vx v24, v11, t1
  neg t1, t1
  vwmul.vx v26, v11, t2
  vwmacc.vx v24, t2, v4
  vwmacc.vx v26, t1, v4

  vwmul.vx v28, v9, t3
  neg t3, t3
  vwmul.vx v30, v9, t4
  vwmacc.vx v28, t4, v6
  vwmacc.vx v30, t3, v6

  vnclip.wi  v0, v16, 12
  vnclip.wi v18, v18, 12
  vnclip.wi  v2, v20, 12
  vnclip.wi v22, v22, 12
  vnclip.wi  v4, v24, 12
  vnclip.wi v26, v26, 12
  vnclip.wi  v6, v28, 12
  vnclip.wi v30, v30, 12

  li t1, 2751
  li t2, 3035
  li t3, 2106
  li t4, 3513

  vwmul.vx v16, v7, t1
  neg t1, t1
  vwmul.vx v20, v7, t2
  vwmacc.vx v16, t2, v8
  vwmacc.vx v20, t1, v8

  vwmul.vx v24, v5, t3
  neg t3, t3
  vwmul.vx v28, v5, t4
  vwmacc.vx v24, t4, v10
  vwmacc.vx v28, t3, v10

  vnclip.wi v16, v16, 12
  vnclip.wi  v9, v20, 12
  vnclip.wi v24, v24, 12
  vnclip.wi v11, v28, 12

  vssub.vv  v8,  v0, v16
  vsadd.vv  v0,  v0, v16
  vssub.vv v10,  v2, v24
  vsadd.vv  v2,  v2, v24

  li t1, 1380
  li t2, 3857
  li t3, 601
  li t4, 4052

  vwmul.vx v16, v3, t1
  neg t1, t1
  vwmul.vx v20, v3, t2
  vwmacc.vx v16, t2, v12
  vwmacc.vx v20, t1, v12

  vwmul.vx v24, v1, t3
  neg t3, t3
  vwmul.vx v28, v1, t4
  vwmacc.vx v24, t4, v14
  vwmacc.vx v28, t3, v14

  vnclip.wi v16, v16, 12
  vnclip.wi v13, v20, 12
  vnclip.wi v24, v24, 12
  vnclip.wi v15, v28, 12

  vssub.vv v12,  v4, v16
  vsadd.vv v16,  v4, v16
  vssub.vv v14,  v6, v24
  vsadd.vv v20,  v6, v24

  vsadd.vv  v1, v18,  v9
  vssub.vv  v9, v18,  v9
  vsadd.vv  v3, v22, v11
  vssub.vv v11, v22, v11
  vsadd.vv v18, v26, v13
  vssub.vv v13, v26, v13
  vsadd.vv v22, v30, v15
  vssub.vv v15, v30, v15

  vssub.vv v4, v0, v16
  vsadd.vv v0, v0, v16
  vssub.vv v5, v1, v18
  vsadd.vv v1, v1, v18
  vssub.vv v6, v2, v20
  vsadd.vv v2, v2, v20
  vssub.vv v7, v3, v22
  vsadd.vv v3, v3, v22

  li t1, 799
  li t2, 4017
  li t3, 3406
  li t4, 2276

  vwmul.vx v16,  v8, t2
  vwmul.vx v18,  v8, t1
  vwmul.vx v20, v10, t4
  vwmul.vx v22, v10, t3
  vwmul.vx v24, v13, t2
  vwmul.vx v26, v13, t1
  vwmul.vx v28, v15, t4
  vwmul.vx v30, v15, t3
  vwmacc.vx v16, t1,  v9
  neg t1, t1
  vwmacc.vx v20, t3, v11
  neg t3, t3
  vwmacc.vx v26, t2, v12
  neg t2, t2
  vwmacc.vx v30, t4, v14
  neg t4, t4
  vwmacc.vx v18, t2,  v9
  vwmacc.vx v22, t4, v11
  vwmacc.vx v24, t1, v12
  vwmacc.vx v28, t3, v14

  li t2, 2896
  li t3, 1567
  li t4, 3784

  vnclip.wi v16, v16, 12
  vnclip.wi v18, v18, 12
  vnclip.wi v20, v20, 12
  vnclip.wi v22, v22, 12
  vnclip.wi v24, v24, 12
  vnclip.wi v26, v26, 12
  vnclip.wi v28, v28, 12
  vnclip.wi v30, v30, 12

  vsadd.vv  v8, v16, v24
  vsadd.vv  v9, v18, v26
  vsadd.vv v10, v20, v28
  vsadd.vv v11, v22, v30
  vssub.vv v12, v16, v24
  vssub.vv v13, v18, v26
  vssub.vv v14, v20, v28
  vssub.vv v15, v22, v30

  vwmul.vx v16,  v4, t4
  vwmul.vx v18,  v4, t3
  vwmul.vx v20,  v7, t4
  vwmul.vx v22,  v7, t3
  vwmul.vx v24, v12, t4
  vwmul.vx v26, v12, t3
  vwmul.vx v28, v15, t4
  vwmul.vx v30, v15, t3
  vwmacc.vx v16, t3,  v5
  vwmacc.vx v22, t4,  v6
  vwmacc.vx v24, t3, v13
  neg t3, t3
  vwmacc.vx v30, t4, v14
  neg t4, t4
  vwmacc.vx v20, t3,  v6
  vwmacc.vx v28, t3, v14
  vwmacc.vx v18, t4,  v5
  vwmacc.vx v26, t4, v13

  vnclip.wi v16, v16, 12
  vnclip.wi v18, v18, 12
  vnclip.wi v20, v20, 12
  vnclip.wi v22, v22, 12
  vnclip.wi v24, v24, 12
  vnclip.wi v26, v26, 12
  vnclip.wi v28, v28, 12
  vnclip.wi v30, v30, 12

.ifc \o0, v0
  vsadd.vv \o14, v9, v11
  vssub.vv  v11, v9, v11
  vssub.vv   v9, v1,  v3
  vsadd.vv \o15, v1,  v3
  vsadd.vv  \o1, v8, v10
  vssub.vv  v10, v8, v10
  vssub.vv   v8, v0,  v2
  vsadd.vv  \o0, v0,  v2
.else
  vsadd.vv  \o1, v8, v10
  vssub.vv  v10, v8, v10
  vssub.vv   v8, v0,  v2
  vsadd.vv  \o0, v0,  v2
  vsadd.vv   v2, v9, v11
  vssub.vv  v11, v9, v11
  vssub.vv   v9, v1,  v3
  vsadd.vv \o15, v1,  v3
  vmv.v.v  \o14, v2
.endif

  vsadd.vv  \o3, v16, v20
  vssub.vv   v6, v16, v20
  vsadd.vv \o12, v18, v22
  vssub.vv   v7, v18, v22
  vsadd.vv  \o2, v24, v28
  vssub.vv  v24, v24, v28
  vsadd.vv \o13, v26, v30
  vssub.vv  v26, v26, v30

  neg t3, t2

  vwmul.vx v28, v24, t2
  vwmul.vx v30, v24, t2
  vwmacc.vx v28, t2, v26
  vwmacc.vx v30, t3, v26

  vwmul.vx v24, v10, t2
  vwmul.vx v26, v10, t2
  vwmacc.vx v24, t2, v11
  vwmacc.vx v26, t3, v11

  vwmul.vx v20, v6, t2
  vwmul.vx v22, v6, t2
  vwmacc.vx v20, t2, v7
  vwmacc.vx v22, t3, v7

  vwmul.vx v16, v8, t2
  vwmul.vx v18, v8, t2
  vwmacc.vx v16, t2, v9
  vwmacc.vx v18, t3, v9

  vnclip.wi  \o7, v16, 12
  vnclip.wi  \o8, v18, 12
  vnclip.wi  \o4, v20, 12
  vnclip.wi \o11, v22, 12
  vnclip.wi  \o6, v24, 12
  vnclip.wi  \o9, v26, 12
  vnclip.wi  \o5, v28, 12
  vnclip.wi \o10, v30, 12

  vmv.v.x v16, zero
  vssub.vv  \o1, v16,  \o1
  vssub.vv  \o3, v16,  \o3
  vssub.vv  \o5, v16,  \o5
  vssub.vv  \o7, v16,  \o7
  vssub.vv  \o9, v16,  \o9
  vssub.vv \o11, v16, \o11
  vssub.vv \o13, v16, \o13
  vssub.vv \o15, v16, \o15
.endm

function inv_adst_e16_x16_rvv, export=1, ext=v
  iadst_16 v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15
  jr t0
endfunc

function inv_flipadst_e16_x16_rvv, export=1, ext=v
  iadst_16 v15, v14, v13, v12, v11, v10, v9, v8, v7, v6, v5, v4, v3, v2, v1, v0
  jr t0
endfunc

.macro def_horz_16 variant
function inv_txfm_horz\variant\()_16x8_rvv, export=1, ext=v
  vmv.v.x v16, zero
  vle16.v v0, (t4)
  vse16.v v16, (t4)
.irp i, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  add t4, t4, t6
  vle16.v v\i, (t4)
  vse16.v v16, (t4)
.endr
.ifc \variant, _identity
  li t1, 2*(5793-4096)*8
.irp i, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  vsmul.vx v16, v\i, t1
  vsra.vi v16, v16, 1
  vaadd.vv v\i, v\i, v16
.endr
  j L(horz_16x8_epilog)
.else
  jalr t0, a4
.irp i, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  vssra.vi v\i, v\i, 2
.endr
L(horz_16x8_epilog):
  vsse16.v v0, (t5), t6
.irp i, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  addi t5, t5, 2
  vsse16.v v\i, (t5), t6
.endr
  jr a7
.endif
endfunc
.endm

def_horz_16 _identity
def_horz_16

function inv_txfm_add_vert_8x16_rvv, export=1, ext=v
  vsetivli zero, 8, e16, m1, ta, ma

  vle16.v v0, (t4)
.irp i, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  add t4, t4, t6
  vle16.v v\i, (t4)
.endr

  jalr t0, a5

.irp i, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  vssra.vi v\i, v\i, 4
.endr

  vsetivli zero, 8, e8, mf2, ta, ma

  vle8.v v16, (t5)
  add t0, t5, a1
  vle8.v v17, (t0)
.irp i, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
  add t0, t0, a1
  vle8.v v\i, (t0)
.endr

  vwaddu.wv v0, v0, v16
  vwaddu.wv v1, v1, v17
  vwaddu.wv v2, v2, v18
  vwaddu.wv v3, v3, v19
  vwaddu.wv v4, v4, v20
  vwaddu.wv v5, v5, v21
  vwaddu.wv v6, v6, v22
  vwaddu.wv v7, v7, v23
  vwaddu.wv v8, v8, v24
  vwaddu.wv v9, v9, v25
  vwaddu.wv v10, v10, v26
  vwaddu.wv v11, v11, v27
  vwaddu.wv v12, v12, v28
  vwaddu.wv v13, v13, v29
  vwaddu.wv v14, v14, v30
  vwaddu.wv v15, v15, v31

  vsetvli zero, zero, e16, m1, ta, ma
.irp i, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  vmax.vx v\i, v\i, zero
.endr

  vsetvli zero, zero, e8, mf2, ta, ma
  vnclipu.wi v16, v0, 0
  vnclipu.wi v17, v1, 0
  vnclipu.wi v18, v2, 0
  vnclipu.wi v19, v3, 0
  vnclipu.wi v20, v4, 0
  vnclipu.wi v21, v5, 0
  vnclipu.wi v22, v6, 0
  vnclipu.wi v23, v7, 0
  vnclipu.wi v24, v8, 0
  vnclipu.wi v25, v9, 0
  vnclipu.wi v26, v10, 0
  vnclipu.wi v27, v11, 0
  vnclipu.wi v28, v12, 0
  vnclipu.wi v29, v13, 0
  vnclipu.wi v30, v14, 0
  vnclipu.wi v31, v15, 0

  vse8.v v16, (t5)
.irp i, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
  add t5, t5, a1
  vse8.v v\i, (t5)
.endr

  jr a7
endfunc

function inv_txfm_add_16x16_rvv, export=1, ext=v
  csrw vxrm, zero
  vsetivli zero, 8, e16, m1, ta, ma
  addi sp, sp, -16*32
.irp i, 8, 0
  addi t4, a2, \i*2
  addi t5, sp, \i*16*2
.if \i == 8
  blt a3, a7, 1f
.endif
  li t6, 16*2
  jalr a7, a6
.if \i == 8
  j 2f
1:
  li t1, 64
  vsetvli zero, t1, e16, m8, ta, ma
  vmv.v.x v0, zero
  vse16.v v0, (t5)
  addi t5, t5, 128
  vse16.v v0, (t5)
  vsetivli zero, 8, e16, m1, ta, ma
2:
.endif
.endr
.irp i, 0, 8
  addi t4, sp, \i*2
  addi t5, a0, \i
  li t6, 16*2
  jal a7, inv_txfm_add_vert_8x16_rvv
.endr
  addi sp, sp, 16*32
  ret
endfunc

.macro def_fn_16x16 txfm1, txfm2, eob_half
function inv_txfm_add_\txfm1\()_\txfm2\()_16x16_8bpc_rvv, export=1, ext=v
.ifc \txfm1\()_\txfm2, dct_dct
 beqz a3, 1f
.endif
.ifc \txfm1, identity
  la a6, inv_txfm_horz_identity_16x8_rvv
.else
  la a6, inv_txfm_horz_16x8_rvv
  la a4, inv_\txfm1\()_e16_x16_rvv
.endif
  la a5, inv_\txfm2\()_e16_x16_rvv
  li a7, \eob_half
  j inv_txfm_add_16x16_rvv
.ifc \txfm1\()_\txfm2, dct_dct
1:
  csrw vxrm, zero
  vsetivli zero, 16, e16, m2, ta, ma
  lh t2, (a2)
  li t3, 2896*8
  li t4, 1<<14
  li t5, 0xFFFF
  li t6, -0x10000

  sh x0, (a2)

  mul t2, t2, t3
  add t2, t2, t4
  srai t2, t2, 15
  ble t2, t5, 3f
  mv t2, t5
3:
  ble t6, t2, 4f
  mv t2, t6
4:
  addi t2, t2, 2
  srai t2, t2, 2
  mul t2, t2, t3
  add t2, t2, t4
  srai t2, t2, 15
  ble t2, t5, 5f
  mv t2, t5
5:
  ble t6, t2, 6f
  mv t2, t6
6:
  addi t2, t2, 8
  srai t2, t2, 4
  vmv.v.x v24, t2

  vsetvli zero, zero, e8, m1, ta, ma
  add t2, a1, a1
  li t3, 16
2:
  add t0, a0, a1
  vle8.v v16, (a0)
  vle8.v v17, (t0)

  vwaddu.wv v0, v24, v16
  vwaddu.wv v2, v24, v17

  addi t3, t3, -2 # loop counter


  vsetvli zero, zero, e16, m2, ta, ma
.irp i, 0, 2
  vmax.vx v\i, v\i, zero
.endr

  vsetvli zero, zero, e8, m1, ta, ma

  vnclipu.wi  v16, v0, 0
  vnclipu.wi  v17, v2, 0

  add t0, a0, a1
  vse8.v v16, (a0)
  add a0, a0, t2
  vse8.v v17, (t0)

  bnez t3, 2b

  ret
.endif
endfunc
.endm

def_fn_16x16 dct, dct, 36
def_fn_16x16 identity, identity, 36
def_fn_16x16 dct, adst, 36
def_fn_16x16 dct, flipadst, 36
def_fn_16x16 dct, identity, 8
def_fn_16x16 adst, dct, 36
def_fn_16x16 adst, adst, 36
def_fn_16x16 adst, flipadst, 36
def_fn_16x16 flipadst, dct, 36
def_fn_16x16 flipadst, adst, 36
def_fn_16x16 flipadst, flipadst, 36
def_fn_16x16 identity, dct, 8

.macro def_fn_416_base variant
function inv_txfm_\variant\()add_4x16_rvv, export=1, ext=v
  csrw vxrm, zero

  vsetivli zero, 8, e16, m1, ta, ma

  blt a3, a6, 1f

  addi t0, a2, 16
  vle16.v v0, (t0)
  addi t0, t0, 32
  vle16.v v1, (t0)
  addi t0, t0, 32
  vle16.v v2, (t0)
  addi t0, t0, 32
  vle16.v v3, (t0)

.ifc \variant, identity_
  li t1, (5793-4096)*8
  vsmul.vx v8, v0, t1
  vaadd.vv v4, v0, v8
  vsmul.vx v8, v1, t1
  vaadd.vv v5, v1, v8
  vsmul.vx v8, v2, t1
  vaadd.vv v6, v2, v8
  vsmul.vx v8, v3, t1
  vaadd.vv v7, v3, v8
.else
  jalr t0, a4

  vssra.vi v4, v0, 1
  vssra.vi v5, v1, 1
  vssra.vi v6, v2, 1
  vssra.vi v7, v3, 1
.endif

  j 2f

1:
.irp i, 4, 5, 6, 7
  vmv.v.x v\i, zero
.endr

2:
  vle16.v v0, (a2)
  addi t0, a2, 32
  vle16.v v1, (t0)
  addi t0, t0, 32
  vle16.v v2, (t0)
  addi t0, t0, 32
  vle16.v v3, (t0)

.ifc \variant, identity_
  li t1, (5793-4096)*8
.irp i, 0, 1, 2, 3
  vsmul.vx v8, v\i, t1
  vaadd.vv v\i, v\i, v8
.endr

  j L(itx_4x16_epilog)
.else
  jalr t0, a4

  vssra.vi v0, v0, 1
  vssra.vi v1, v1, 1
  vssra.vi v2, v2, 1
  vssra.vi v3, v3, 1

L(itx_4x16_epilog):
  vsseg4e16.v v0, (a2)
  addi t0, a2, 64
  vsseg4e16.v v4, (t0)

  vsetivli zero, 4, e16, mf2, ta, ma

  vmv.v.x v16, zero
  vle16.v v0, (a2)
  vse16.v v16, (a2)
  addi t0, a2, 8
  vle16.v v1, (t0)
  vse16.v v16, (t0)
.irp i, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  addi t0, t0, 8
  vle16.v v\i, (t0)
  vse16.v v16, (t0)
.endr

  jalr t0, a5

.irp i, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  vssra.vi v\i, v\i, 4
.endr

  vsetvli zero, zero, e8, mf4, ta, ma

  vle8.v v16, (a0)
  add t0, a0, a1
  vle8.v v17, (t0)
.irp i, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
  add t0, t0, a1
  vle8.v v\i, (t0)
.endr

  vwaddu.wv  v0,  v0, v16
  vwaddu.wv  v1,  v1, v17
  vwaddu.wv  v2,  v2, v18
  vwaddu.wv  v3,  v3, v19
  vwaddu.wv  v4,  v4, v20
  vwaddu.wv  v5,  v5, v21
  vwaddu.wv  v6,  v6, v22
  vwaddu.wv  v7,  v7, v23
  vwaddu.wv  v8,  v8, v24
  vwaddu.wv  v9,  v9, v25
  vwaddu.wv v10, v10, v26
  vwaddu.wv v11, v11, v27
  vwaddu.wv v12, v12, v28
  vwaddu.wv v13, v13, v29
  vwaddu.wv v14, v14, v30
  vwaddu.wv v15, v15, v31

  vsetvli zero, zero, e16, mf2, ta, ma
.irp i, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  vmax.vx v\i, v\i, zero
.endr

  vsetvli zero, zero, e8, mf4, ta, ma

  vnclipu.wi v16,  v0, 0
  vnclipu.wi v17,  v1, 0
  vnclipu.wi v18,  v2, 0
  vnclipu.wi v19,  v3, 0
  vnclipu.wi v20,  v4, 0
  vnclipu.wi v21,  v5, 0
  vnclipu.wi v22,  v6, 0
  vnclipu.wi v23,  v7, 0
  vnclipu.wi v24,  v8, 0
  vnclipu.wi v25,  v9, 0
  vnclipu.wi v26, v10, 0
  vnclipu.wi v27, v11, 0
  vnclipu.wi v28, v12, 0
  vnclipu.wi v29, v13, 0
  vnclipu.wi v30, v14, 0
  vnclipu.wi v31, v15, 0

  vse8.v v16, (a0)
.irp i, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
  add a0, a0, a1
  vse8.v v\i, (a0)
.endr

  ret
.endif
endfunc

function inv_txfm_\variant\()add_16x4_rvv, export=1, ext=v
  csrw vxrm, zero

  vsetivli zero, 4, e16, mf2, ta, ma
  vle16.v v0, (a2)
  addi t0, a2, 8
  vle16.v v1, (t0)
.irp i, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  addi t0, t0, 8
  vle16.v v\i, (t0)
.endr

.ifc \variant, identity_
  li t1, 2*(5793-4096)*8
.irp i, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  vsmul.vx v16, v\i, t1
  vssra.vi v16, v16, 1
  vsadd.vv v\i, v\i, v16
.endr

  j L(itx_16x4_epilog)
.else
  jalr t0, a4

.irp i, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  vssra.vi v\i, v\i, 1
.endr

L(itx_16x4_epilog):
  li t0, 32
  vssseg8e16.v v0, (a2), t0
  addi t1, a2, 16
  vssseg8e16.v v8, (t1), t0

.irp j, 0, 8
  vsetivli zero, 8, e16, m1, ta, ma

  vmv.v.x v4, zero
  addi t0, a2, \j*2
  vle16.v v0, (t0)
  vse16.v v4, (t0)
.irp i, 1, 2, 3
  addi t0, t0, 32
  vle16.v v\i, (t0)
  vse16.v v4, (t0)
.endr

  jalr t0, a5

  vssra.vi v0, v0, 4
  vssra.vi v1, v1, 4
  vssra.vi v2, v2, 4
  vssra.vi v3, v3, 4

  vsetvli zero, zero, e8, mf2, ta, ma
  addi t0, a0, \j
  vle8.v v4, (t0)
  add t0, t0, a1
  vle8.v v5, (t0)
  add t0, t0, a1
  vle8.v v6, (t0)
  add t0, t0, a1
  vle8.v v7, (t0)

  vwaddu.wv v0, v0, v4
  vwaddu.wv v1, v1, v5
  vwaddu.wv v2, v2, v6
  vwaddu.wv v3, v3, v7

  vsetvli zero, zero, e16, m1, ta, ma
  vmax.vx v0, v0, zero
  vmax.vx v1, v1, zero
  vmax.vx v2, v2, zero
  vmax.vx v3, v3, zero

  vsetvli zero, zero, e8, mf2, ta, ma

  vnclipu.wi v4, v0, 0
  vnclipu.wi v5, v1, 0
  vnclipu.wi v6, v2, 0
  vnclipu.wi v7, v3, 0

  addi t0, a0, \j
  vse8.v v4, (t0)
  add t0, t0, a1
  vse8.v v5, (t0)
  add t0, t0, a1
  vse8.v v6, (t0)
  add t0, t0, a1
  vse8.v v7, (t0)
.endr

  ret
.endif
endfunc
.endm

def_fn_416_base identity_
def_fn_416_base

.macro def_fn_416 w, h, txfm1, txfm2, eob_half
function inv_txfm_add_\txfm1\()_\txfm2\()_\w\()x\h\()_8bpc_rvv, export=1
.if \w == 4 && (\txfm1 == adst || \txfm1 == flipadst)
  la a4, inv_\txfm1\()_e16_x\w\()w_rvv
.elseif \txfm1 != identity
  la a4, inv_\txfm1\()_e16_x\w\()_rvv
.endif
.if \h == 4 && (\txfm2 == adst || \txfm2 == flipadst)
  la a5, inv_\txfm2\()_e16_x\h\()w_rvv
.else
  la a5, inv_\txfm2\()_e16_x\h\()_rvv
.endif
.if \w == 4
  li a6, \eob_half
.endif
.ifc \txfm1, identity
  j inv_txfm_identity_add_\w\()x\h\()_rvv
.else
  j inv_txfm_add_\w\()x\h\()_rvv
.endif
endfunc
.endm

.macro def_fns_416 w, h
def_fn_416 \w, \h, dct, dct, 29
def_fn_416 \w, \h, identity, identity, 29
def_fn_416 \w, \h, dct, adst, 29
def_fn_416 \w, \h, dct, flipadst, 29
def_fn_416 \w, \h, dct, identity, 8
def_fn_416 \w, \h, adst, dct, 29
def_fn_416 \w, \h, adst, adst, 29
def_fn_416 \w, \h, adst, flipadst, 29
def_fn_416 \w, \h, flipadst, dct, 29
def_fn_416 \w, \h, flipadst, adst, 29
def_fn_416 \w, \h, flipadst, flipadst, 29
def_fn_416 \w, \h, identity, dct, 32
def_fn_416 \w, \h, adst, identity, 8
def_fn_416 \w, \h, flipadst, identity, 8
def_fn_416 \w, \h, identity, adst, 32
def_fn_416 \w, \h, identity, flipadst, 32
.endm

def_fns_416 4, 16
def_fns_416 16, 4

.macro def_fn_816_base variant
function inv_txfm_\variant\()add_8x16_rvv, export=1, ext=v
  csrw vxrm, zero

  vsetivli zero, 8, e16, m1, ta, ma

  blt a3, a6, 1f

  vmv.v.x v16, zero
  addi t0, a2, 16
  vle16.v v0, (t0)
  vse16.v v16, (t0)
.irp i, 1, 2, 3, 4, 5, 6, 7
  addi t0, t0, 32
  vle16.v v\i, (t0)
  vse16.v v16, (t0)
.endr

  li t1, 2896*8
.ifc \variant, identity_
  vsmul.vx  v8, v0, t1
  vsmul.vx  v9, v1, t1
  vsmul.vx v10, v2, t1
  vsmul.vx v11, v3, t1
  vsmul.vx v12, v4, t1
  vsmul.vx v13, v5, t1
  vsmul.vx v14, v6, t1
  vsmul.vx v15, v7, t1
.else
.irp i, 0, 1, 2, 3, 4, 5, 6, 7
  vsmul.vx v\i, v\i, t1
.endr

  jalr t0, a4

  vssra.vi  v8, v0, 1
  vssra.vi  v9, v1, 1
  vssra.vi v10, v2, 1
  vssra.vi v11, v3, 1
  vssra.vi v12, v4, 1
  vssra.vi v13, v5, 1
  vssra.vi v14, v6, 1
  vssra.vi v15, v7, 1
.endif

  j 2f

1:
.irp i, 8, 9, 10, 11, 12, 13, 14, 15
  vmv.v.x v\i, zero
.endr

2:
  vmv.v.x v16, zero
  vle16.v v0, (a2)
  vse16.v v16, (a2)
  addi t0, a2, 32
  vle16.v v1, (t0)
  vse16.v v16, (t0)
.irp i, 2, 3, 4, 5, 6, 7
  addi t0, t0, 32
  vle16.v v\i, (t0)
  vse16.v v16, (t0)
.endr

  li t1, 2896*8
.irp i, 0, 1, 2, 3, 4, 5, 6, 7
  vsmul.vx v\i, v\i, t1
.endr

.ifc \variant, identity_
  j L(itx_8x16_epilog)
.else
  jalr t0, a4

.irp i, 0, 1, 2, 3, 4, 5, 6, 7
  vssra.vi v\i, v\i, 1
.endr

L(itx_8x16_epilog):
  addi t4, sp, -8*32
  vsseg8e16.v v0, (t4)
  addi t0, t4, 8*16
  vsseg8e16.v v8, (t0)

  mv t5, a0
  li t6, 16
  jal a7, inv_txfm_add_vert_8x16_rvv

  ret
.endif
endfunc

function inv_txfm_\variant\()add_16x8_rvv, export=1, ext=v
  csrw vxrm, zero

  vsetivli zero, 8, e16, m1, ta, ma
  vle16.v v0, (a2)
  addi t0, a2, 16
  vle16.v v1, (t0)
.irp i, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  addi t0, t0, 16
  vle16.v v\i, (t0)
.endr

  li t1, 2896*8
.irp i, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  vsmul.vx v\i, v\i, t1
.endr

.ifc \variant, identity_
  li t1, 2*(5793-4096)*8
.irp i, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  vsmul.vx v16, v\i, t1
  vssra.vi v16, v16, 1
  vsadd.vv v\i, v\i, v16
.endr

  j L(itx_16x8_epilog)
.else
  jalr t0, a4

.irp i, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
  vssra.vi v\i, v\i, 1
.endr

L(itx_16x8_epilog):
  li t0, 32
  vssseg8e16.v v0, (a2), t0
  addi t1, a2, 16
  vssseg8e16.v v8, (t1), t0

.irp j, 0, 8
  vsetivli zero, 8, e16, m1, ta, ma

  vmv.v.x v8, zero
  addi t0, a2, \j*2
  vle16.v v0, (t0)
  vse16.v v8, (t0)
.irp i, 1, 2, 3, 4, 5, 6, 7
  addi t0, t0, 32
  vle16.v v\i, (t0)
  vse16.v v8, (t0)
.endr

  jalr t0, a5

.irp i, 0, 1, 2, 3, 4, 5, 6, 7
  vssra.vi v\i, v\i, 4
.endr

  vsetvli zero, zero, e8, mf2, ta, ma
  addi t0, a0, \j
  vle8.v v8, (t0)
.irp i, 9, 10, 11, 12, 13, 14, 15
  add t0, t0, a1
  vle8.v v\i, (t0)
.endr

  vwaddu.wv v0, v0, v8
  vwaddu.wv v1, v1, v9
  vwaddu.wv v2, v2, v10
  vwaddu.wv v3, v3, v11
  vwaddu.wv v4, v4, v12
  vwaddu.wv v5, v5, v13
  vwaddu.wv v6, v6, v14
  vwaddu.wv v7, v7, v15

  vsetvli zero, zero, e16, m1, ta, ma
.irp i, 0, 1, 2, 3, 4, 5, 6, 7
  vmax.vx v\i, v\i, zero
.endr

  vsetvli zero, zero, e8, mf2, ta, ma

  vnclipu.wi  v8, v0, 0
  vnclipu.wi  v9, v1, 0
  vnclipu.wi v10, v2, 0
  vnclipu.wi v11, v3, 0
  vnclipu.wi v12, v4, 0
  vnclipu.wi v13, v5, 0
  vnclipu.wi v14, v6, 0
  vnclipu.wi v15, v7, 0

  addi t0, a0, \j
  vse8.v v8, (t0)
.irp i, 9, 10, 11, 12, 13, 14, 15
  add t0, t0, a1
  vse8.v v\i, (t0)
.endr
.endr

  ret
.endif
endfunc
.endm

def_fn_816_base identity_
def_fn_816_base

.macro def_fn_816 w, h, txfm1, txfm2, eob_half
function inv_txfm_add_\txfm1\()_\txfm2\()_\w\()x\h\()_8bpc_rvv, export=1
.ifnc \txfm1, identity
  la a4, inv_\txfm1\()_e16_x\w\()_rvv
.endif
  la a5, inv_\txfm2\()_e16_x\h\()_rvv
.if \w == 8
  li a6, \eob_half
.endif
.ifc \txfm1, identity
  j inv_txfm_identity_add_\w\()x\h\()_rvv
.else
  j inv_txfm_add_\w\()x\h\()_rvv
.endif
endfunc
.endm

.macro def_fns_816 w, h
def_fn_816 \w, \h, dct, dct, 43
def_fn_816 \w, \h, identity, identity, 43
def_fn_816 \w, \h, dct, adst, 43
def_fn_816 \w, \h, dct, flipadst, 43
def_fn_816 \w, \h, dct, identity, 8
def_fn_816 \w, \h, adst, dct, 43
def_fn_816 \w, \h, adst, adst, 43
def_fn_816 \w, \h, adst, flipadst, 43
def_fn_816 \w, \h, flipadst, dct, 43
def_fn_816 \w, \h, flipadst, adst, 43
def_fn_816 \w, \h, flipadst, flipadst, 43
def_fn_816 \w, \h, identity, dct, 64
def_fn_816 \w, \h, adst, identity, 8
def_fn_816 \w, \h, flipadst, identity, 8
def_fn_816 \w, \h, identity, adst, 64
def_fn_816 \w, \h, identity, flipadst, 64
.endm

def_fns_816 8, 16
def_fns_816 16, 8

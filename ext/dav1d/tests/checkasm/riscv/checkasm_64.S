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

#define PRIVATE_PREFIX checkasm_

#include "src/riscv/asm.S"

// max number of args used by any asm function.
#define MAX_ARGS 15

// + 16 for stack canary reference
#define ARG_STACK ((8*(MAX_ARGS - 8) + 15) & ~15 + 16)

const register_init, align=4
        .quad 0x68909d060f4a7fdd
        .quad 0x924f739e310218a1
        .quad 0xb988385a8254174c
        .quad 0x4c1110430bf09fd7
        .quad 0x2b310edf6a5d7ecf
        .quad 0xda8112e98ddbb559
        .quad 0x6da5854aa2f84b62
        .quad 0x72b761199e9b1f38
        .quad 0x13f27aa74ae5dcdf
        .quad 0x36a6c12a7380e827
        .quad 0x5c452889aefc8548
        .quad 0x6a9ea1ddb236235f
        .quad 0x0449854bdfc94b1e
        .quad 0x4f849b7076a156f5
        .quad 0x1baa4275e734930e
        .quad 0x77df3503ba3e073d
        .quad 0x6060e073705a4bf2
        .quad 0xa7b482508471e44b
        .quad 0xd296a3158d6da2b9
        .quad 0x1c0ed711a93d970b
        .quad 0x9359537fdd79569d
        .quad 0x2b1dc95c1e232d62
        .quad 0xab06cd578e2bb5a0
        .quad 0x4100b4987a0af30f
        .quad 0x2523e36f9bb1e36f
        .quad 0xfb0b815930c6d25c
        .quad 0x89acc810c2902fcf
        .quad 0xa65854b4c2b381f1
        .quad 0x78150d69a1accedf
        .quad 0x057e24868e022de1
        .quad 0x88f6e79ed4b8d362
        .quad 0x1f4a420e262c9035
endconst

const error_message_register
error_message_rsvd:
        .asciz "unallocatable register clobbered"
error_message_sreg:
        .asciz "callee-saved integer register s%i modified"
error_message_fsreg:
        .asciz "callee-saved floating-point register fs%i modified"
error_message_stack:
        .asciz "stack clobbered"
endconst

thread_local saved_regs, quads=29 # 5 + 12 + 12

function checked_call, export=1, ext=v
  /* Save the function ptr, RA, SP, unallocatable and callee-saved registers */
  la.tls.ie t0, saved_regs
  add t0, tp, t0
  sd a0, (t0)
  sd ra, 8(t0)
  sd sp, 16(t0)
  sd gp, 24(t0)
  sd tp, 32(t0)
.irp n, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
  sd s\n, 40 + 16*\n(t0)
#ifdef __riscv_float_abi_double
  fsd fs\n, 48 + 16*\n(t0)
#endif
.endr

  /* Check for vector extension */
  call dav1d_get_cpu_flags_riscv
  and a0, a0, 1 # DAV1D_RISCV_CPU_FLAG_RVV
  beqz a0, 0f

  /* Clobber vector configuration */
  vsetvli t0, zero, e32, m8, ta, ma
  lla t0, register_init
  ld t0, (t0)
.irp n, 0, 8, 16, 24
  vmv.v.x v0, t0
.endr
  li t0, -1 << 31
  vsetvl zero, zero, t0
  csrwi vxrm, 3
  csrwi vxsat, 1

0:
  /* Load the register arguments */
.irp n, 0, 1, 2, 3, 4, 5, 6, 7
  ld a\n, 8*\n(sp)
.endr

  /* Load the stack arguments */
.irp n, 8, 9, 10, 11, 12, 13, 14, 15
   ld t0, 8*\n(sp)
   sd t0, 8*(\n - 8) - ARG_STACK(sp)
.endr

  /* Setup the stack canary */
  ld t0, MAX_ARGS*8(sp)
  addi sp, sp, -ARG_STACK
  slli t0, t0, 3
  add t0, t0, sp
  ld t0, (t0)
  not t0, t0
  sd t0, ARG_STACK - 8(sp)

  /* Clobber the stack space right below SP */
  lla t0, register_init
  ld t1, (t0)
.rept 16
  addi sp, sp, -16
  sd t1, (sp)
  sd t1, 8(sp)
.endr
  addi sp, sp, 16*16

  /* Clobber the callee-saved and temporary registers */
.irp n, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
.if (\n > 0 && \n < 7)
  ld t\n, 16*\n(t0)
.endif
  ld s\n, 8 + 8*\n(t0)
#ifdef __riscv_float_abi_double
  fld ft\n, 16 + 16*\n(t0)
  fld fs\n, 24 + 8*\n(t0)
#endif
.endr

  /* Call the checked function */
  la.tls.ie t0, saved_regs
  add t0, tp, t0
  ld t0, (t0)
  jalr t0

  /* Check the value of callee-saved registers */
  lla t0, register_init
.irp n, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
  ld t1, 8 + 8*\n(t0)
  li a1, \n
  bne t1, s\n, 2f
#ifdef __riscv_float_abi_double
  ld t1, 24 + 8*\n(t0)
  fmv.x.d t2, fs\n
  bne t1, t2, 3f
#endif
.endr

  /* Check unallocatable register values */
  la.tls.ie t0, saved_regs
  add t0, tp, t0
  ld t1, 16(t0)
  addi t1, t1, -ARG_STACK
  bne t1, sp, 4f
  ld t1, 24(t0)
  bne t1, gp, 4f
  ld t1, 32(t0)
  bne t1, tp, 4f

  /* Check the stack canary */
  ld t0, ARG_STACK + MAX_ARGS*8(sp)
  slli t0, t0, 3
  add t0, t0, sp
  ld t0, (t0)
  not t0, t0
  ld t1, ARG_STACK - 8(sp)
  bne t0, t1, 5f

1:
  /* Restore RA, SP and callee-saved registers from thread local storage */
  la.tls.ie t0, saved_regs
  add t0, tp, t0
  ld ra, 8(t0)
  ld sp, 16(t0)
.irp n, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
  ld s\n, 40 + 16*\n(t0)
#ifdef __riscv_float_abi_double
  fld fs\n, 48 + 16*\n(t0)
#endif
.endr
  ret

2:
  lla a0, error_message_sreg
#ifdef PREFIX
  call _checkasm_fail_func
#else
  call checkasm_fail_func
#endif
  j 1b

#ifdef __riscv_float_abi_double
3:
  lla a0, error_message_fsreg
#ifdef PREFIX
  call _checkasm_fail_func
#else
  call checkasm_fail_func
#endif
  j 1b
#endif

4:
  lla a0, error_message_rsvd
#ifdef PREFIX
  call _checkasm_fail_func
#else
  call checkasm_fail_func
#endif
  j 1b

5:
  lla a0, error_message_stack
#ifdef PREFIX
  call _checkasm_fail_func
#else
  call checkasm_fail_func
#endif
  j 1b
endfunc

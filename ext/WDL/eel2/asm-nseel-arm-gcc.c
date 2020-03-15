#define FUNCTION_MARKER  "mov r0, r0\n" \
                         "mov r1, r1\n" \
                         "mov r2, r2\n"

#if EEL_F_SIZE == 8

__attribute__((naked)) void nseel_asm_1pdd(void)
{

  __asm__ __volatile__( 
    FUNCTION_MARKER
    "movw r3, 0xdead\n" 
    "movt r3, 0xbeef\n"  
    "str lr, [sp, #-8]!\n"
    "blx r3\n" 
    "ldr lr, [sp], #8\n"
    FUNCTION_MARKER
   :: );
}
__attribute__((naked)) void nseel_asm_1pdd_end(void){}

__attribute__((naked)) void nseel_asm_2pdd(void)
{

  __asm__ __volatile__( 
    FUNCTION_MARKER
    "movw r3, 0xdead\n" 
    "movt r3, 0xbeef\n"  
    "fcpyd d1, d0\n" 
    "fldd d0, [r1]\n" 
    "str lr, [sp, #-8]!\n"
    "blx r3\n" 
    "ldr lr, [sp], #8\n"
    FUNCTION_MARKER
   :: );
};
__attribute__((naked)) void nseel_asm_2pdd_end(void){}

__attribute__((naked)) void nseel_asm_2pdds(void)
{
  __asm__ __volatile__( 
    FUNCTION_MARKER
    "movw r3, 0xdead\n" 
    "movt r3, 0xbeef\n"  
    "push {r1, lr}\n"
    "fcpyd d1, d0\n" 
    "fldd d0, [r1]\n" 
    "blx r3\n" 
    "pop {r0, lr}\n"
    "fstd d0, [r0]\n"
    FUNCTION_MARKER
   :: );
}
__attribute__((naked)) void nseel_asm_2pdds_end(void){}

#else // 32 bit floating point calls

#error no 32 bit float support

#endif

//---------------------------------------------------------------------------------------------------------------



// do nothing, eh
__attribute__((naked)) void nseel_asm_exec2(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_exec2_end(void) { }



__attribute__((naked)) void nseel_asm_invsqrt(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "movw r0, 0x59df\n"
    "movt r0, 0x5f37\n"
    "fcvtsd s2, d0\n"
    "fldd d3, [r6, #32]\n"
    "fmrs r1, s2\n"
    "fmuld d0, d0, d3\n"
    "mov r1, r1, asr #1\n" 

    "sub r0, r0, r1\n"

    "fmsr s4, r0\n"
    "fcvtds d1, s4\n"

    "fldd d2, [r6, #40]\n"
    "fmuld d0, d0, d1\n"
    "fmuld d0, d0, d1\n"
    "faddd d0, d0, d2\n"
    "fmuld d0, d0, d1\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_invsqrt_end(void) {}

__attribute__((naked)) void nseel_asm_dbg_getstackptr(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fmsr  s0, sp\n"
    "fsitod  d0, s0\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_dbg_getstackptr_end(void) {}


//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_sqr(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fmuld d0, d0, d0\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_sqr_end(void) {}


//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_abs(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fabsd d0, d0\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_abs_end(void) {}


//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_assign(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fldd d0, [r0]\n"
   "mov r0, r1\n"
   "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_assign_end(void) {}
//
//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_assign_fromfp(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "mov r0, r1\n"
   "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_assign_fromfp_end(void) {}

//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_assign_fast(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fldd d0, [r0]\n"
   "mov r0, r1\n"
   "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_assign_fast_end(void) {}
//
//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_assign_fast_fromfp(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "mov r0, r1\n"
   "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_assign_fast_fromfp_end(void) {}



//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_add(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fldd d1, [r1]\n"
   "faddd d0, d1, d0\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_add_end(void) {}

__attribute__((naked)) void nseel_asm_add_op(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fldd d1, [r1]\n"
   "faddd d0, d1, d0\n"
   "mov r0, r1\n"
   "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_add_op_end(void) {}

__attribute__((naked)) void nseel_asm_add_op_fast(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fldd d1, [r1]\n"
   "faddd d0, d1, d0\n"
   "mov r0, r1\n"
   "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_add_op_fast_end(void) {}


//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_sub(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fldd d1, [r1]\n"
   "fsubd d0, d1, d0\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_sub_end(void) {}

__attribute__((naked)) void nseel_asm_sub_op(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fldd d1, [r1]\n"
   "fsubd d0, d1, d0\n"
   "mov r0, r1\n"
   "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_sub_op_end(void) {}

__attribute__((naked)) void nseel_asm_sub_op_fast(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fldd d1, [r1]\n"
   "fsubd d0, d1, d0\n"
   "mov r0, r1\n"
   "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_sub_op_fast_end(void) {}

//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_mul(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fldd d1, [r1]\n"
   "fmuld d0, d0, d1\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_mul_end(void) {}

__attribute__((naked)) void nseel_asm_mul_op(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fldd d1, [r1]\n"
   "fmuld d0, d0, d1\n"
   "mov r0, r1\n"
   "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_mul_op_end(void) {}

__attribute__((naked)) void nseel_asm_mul_op_fast(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fldd d1, [r1]\n"
   "fmuld d0, d0, d1\n"
   "mov r0, r1\n"
   "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_mul_op_fast_end(void) {}

//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_div(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fldd d1, [r1]\n"
   "fdivd d0, d1, d0\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_div_end(void) {}

__attribute__((naked)) void nseel_asm_div_op(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fldd d1, [r1]\n"
   "fdivd d0, d1, d0\n"
   "mov r0, r1\n"
   "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_div_op_end(void) {}

__attribute__((naked)) void nseel_asm_div_op_fast(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fldd d1, [r1]\n"
   "fdivd d0, d1, d0\n"
   "mov r0, r1\n"
   "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_div_op_fast_end(void) {}

//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_mod(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "ftouizd s0, d0\n" // round to unsigned integers
    "fmrs r3, s0\n"
    "fuitod  d0, s0\n" // divisor
    "ftouizd s2, d1\n"

    "cmp r3, #0\n"
    "beq 0f\n"

    "fuitod  d1, s2\n" // value
    "fdivd d2, d1, d0\n"
    "ftouizd s4, d2\n"
    "fuitod  d2, s4\n"
    "fmuld d2, d2, d0\n"
    "fsubd d0, d1, d2\n"
    "0:\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_mod_end(void) {}

__attribute__((naked)) void nseel_asm_shl(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "ftosizd s0, d0\n"
    "ftosizd s1, d1\n"
    "fmrs r3, s0\n"
    "fmrs r2, s1\n"
    "mov r3, r2, asl r3\n"
    "fmsr  s0, r3\n"
    "fsitod  d0, s0\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_shl_end(void) {}

__attribute__((naked)) void nseel_asm_shr(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "ftosizd s0, d0\n"
    "ftosizd s1, d1\n"
    "fmrs r3, s0\n"
    "fmrs r2, s1\n"
    "mov r3, r2, asr r3\n"
    "fmsr  s0, r3\n"
    "fsitod  d0, s0\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_shr_end(void) {}

__attribute__((naked)) void nseel_asm_mod_op(void)
{

  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "ftouizd s0, d0\n" // round to unsigned integers
    "fmrs r3, s0\n"
    "fuitod  d0, s0\n" // divisor
    "ftouizd s2, d1\n"

    "cmp r3, #0\n"
    "beq 0f\n"

    "fuitod  d1, s2\n" // value
    "fdivd d2, d1, d0\n"
    "ftouizd s4, d2\n"
    "fuitod  d2, s4\n"
    "fmuld d2, d2, d0\n"
    "fsubd d0, d1, d2\n"
    "0:\n"
    "mov r0, r1\n"
    "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );

}
__attribute__((naked)) void nseel_asm_mod_op_end(void) {}

//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_or(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "ftosizd s0, d0\n"
    "ftosizd s1, d1\n"
    "fmrs r3, s0\n"
    "fmrs r2, s1\n"
    "orr r3, r3, r2\n"
    "fmsr  s0, r3\n"
    "fsitod  d0, s0\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_or_end(void) {}

__attribute__((naked)) void nseel_asm_or0(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ftosizd s0, d0\n"
    "fsitod  d0, s0\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_or0_end(void) {}

__attribute__((naked)) void nseel_asm_or_op(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "ftosizd s0, d0\n"
    "ftosizd s1, d1\n"
    "fmrs r3, s0\n"
    "fmrs r2, s1\n"
    "orr r3, r3, r2\n"
    "fmsr  s0, r3\n"
    "fsitod  d0, s0\n"
    "mov r0, r1\n"
    "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_or_op_end(void) {}

//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_xor(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "ftosizd s0, d0\n"
    "ftosizd s1, d1\n"
    "fmrs r3, s0\n"
    "fmrs r2, s1\n"
    "eor r3, r3, r2\n"
    "fmsr  s0, r3\n"
    "fsitod  d0, s0\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_xor_end(void) {}

__attribute__((naked)) void nseel_asm_xor_op(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "ftosizd s0, d0\n"
    "ftosizd s1, d1\n"
    "fmrs r3, s0\n"
    "fmrs r2, s1\n"
    "eor r3, r3, r2\n"
    "fmsr  s0, r3\n"
    "fsitod  d0, s0\n"
    "mov r0, r1\n"
    "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_xor_op_end(void) {}

//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_and(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "ftosizd s0, d0\n"
    "ftosizd s1, d1\n"
    "fmrs r3, s0\n"
    "fmrs r2, s1\n"
    "and r3, r3, r2\n"
    "fmsr  s0, r3\n"
    "fsitod  d0, s0\n"
    FUNCTION_MARKER
  );}
__attribute__((naked)) void nseel_asm_and_end(void) {}

__attribute__((naked)) void nseel_asm_and_op(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "ftosizd s0, d0\n"
    "ftosizd s1, d1\n"
    "fmrs r3, s0\n"
    "fmrs r2, s1\n"
    "and r3, r3, r2\n"
    "fmsr  s0, r3\n"
    "fsitod  d0, s0\n"
    "mov r0, r1\n"
    "fstd d0, [r1]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_and_op_end(void) {}


//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_uplus(void) // this is the same as doing nothing, it seems
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_uplus_end(void) {}

//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_uminus(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fnegd d0, d0\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_uminus_end(void) {}


//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_sign(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fcmpzd d0\n"
    "fmstat\n"
    "flddgt d0, [r6, #16]\n"
    "flddlt d0, [r6, #24]\n"
    FUNCTION_MARKER
    :: 
  );
}
__attribute__((naked)) void nseel_asm_sign_end(void) {}



//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_bnot(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "cmp r0, #0\n"
    "movne r0, #0\n"
    "moveq r0, #1\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_bnot_end(void) {}

//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_if(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "str lr, [sp, #-8]!\n"
    "movw r1, 0xdead\n" 
    "movt r1, 0xbeef\n"  
    "movw r2, 0xdead\n" 
    "movt r2, 0xbeef\n"  
    "cmp r0, #0\n"
    "moveq r1, r2\n"
    "blx r1\n" 
    "ldr lr, [sp], #8\n"
    FUNCTION_MARKER
  :: );
}
__attribute__((naked)) void nseel_asm_if_end(void) {}

//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_repeat(void)
{
#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN > 0
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ftosizd s0, d0\n"
    "fmrs r3, s0\n"
    "cmp r3, #0\n"
    "ble 0f\n"
    "movw r2, %0\n"
    "movt r2, %1\n"
    "cmp r3, r2\n"
    "movgt r3, r2\n"
    "push {r10,lr}\n"

    "movw r10, 0xdead\n"
    "movt r10, 0xbeef\n"  
  "1:\n"
    "push {r3,r5}\n" // save counter + worktable
    "blx r10\n"
    "pop {r3,r5}\n"
    "sub r3, r3, #1\n"
    "cmp r3, #0\n"
    "bgt 1b\n"
    "pop {r10,lr}\n"

    "0:\n"
    FUNCTION_MARKER
    ::"g" (NSEEL_LOOPFUNC_SUPPORT_MAXLEN&65535),
      "g" (NSEEL_LOOPFUNC_SUPPORT_MAXLEN>>16)
  );
#else
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ftosizd s0, d0\n"
    "fmrs r3, s0\n"
    "cmp r3, #0\n"
    "ble 0f\n"
    "push {r10,lr}\n"

    "movw r10, 0xdead\n"
    "movt r10, 0xbeef\n"  
  "1:\n"
    "push {r3,r5}\n" // save counter + worktable
    "blx r10\n"
    "pop {r3,r5}\n"
    "sub r3, r3, #1\n"
    "cmp r3, #0\n"
    "bgt 1b\n"
    "pop {r10,lr}\n"

    "0:\n"
    FUNCTION_MARKER
  );
#endif
}
__attribute__((naked)) void nseel_asm_repeat_end(void) {}

__attribute__((naked)) void nseel_asm_repeatwhile(void)
{
#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN > 0
  __asm__ __volatile__(
    FUNCTION_MARKER
    "movw r3, %0\n"
    "movt r3, %1\n"
    "push {r10,lr}\n"

    "movw r10, 0xdead\n"
    "movt r10, 0xbeef\n"  
  "0:\n"
    "push {r3,r5}\n" // save counter + worktable
    "blx r10\n"
    "pop {r3,r5}\n"
    "sub r3, r3, #1\n"
    "cmp r0, #0\n"
    "cmpne r3, #0\n"
    "bne 0b\n"
    "pop {r10,lr}\n"

    FUNCTION_MARKER
    ::"g" (NSEEL_LOOPFUNC_SUPPORT_MAXLEN&65535),
      "g" (NSEEL_LOOPFUNC_SUPPORT_MAXLEN>>16)
  );
#else
  __asm__ __volatile__(
    FUNCTION_MARKER
    "push {r10,lr}\n"

    "movw r10, 0xdead\n"
    "movt r10, 0xbeef\n"  
  "0:\n"
    "push {r3,r5}\n" // save worktable (r3 just for alignment)
    "blx r10\n"
    "pop {r3,r5}\n"
    "cmp r0, #0\n"
    "bne 0b\n"
    "pop {r10,lr}\n"

    "0:\n"
    FUNCTION_MARKER
  );
#endif
}
__attribute__((naked)) void nseel_asm_repeatwhile_end(void) {}


__attribute__((naked)) void nseel_asm_band(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "cmp r0, #0\n"
    "beq 0f\n"
    "movw r3, 0xdead\n"
    "movt r3, 0xbeef\n"  
    "push {r3, lr}\n"
    "blx r3\n"
    "pop {r3, lr}\n"
    "0:\n"
    FUNCTION_MARKER
  :: );
}
__attribute__((naked)) void nseel_asm_band_end(void) {}

__attribute__((naked)) void nseel_asm_bor(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "cmp r0, #0\n"
    "bne 0f\n"
    "movw r3, 0xdead\n"
    "movt r3, 0xbeef\n"  
    "push {r3, lr}\n"
    "blx r3\n"
    "pop {r3, lr}\n"
    "0:\n"
    FUNCTION_MARKER
  :: );
}
__attribute__((naked)) void nseel_asm_bor_end(void) {}

//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_equal(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "fldd d2, [r6]\n"
    "fsubd d0, d0, d1\n"
    "fabsd d0, d0\n"
    "fcmpd d2, d0\n"
    "fmstat\n"
    "movlt r0, #0\n"
    "movge r0, #1\n"
    FUNCTION_MARKER
    :: 
  );
}
__attribute__((naked)) void nseel_asm_equal_end(void) {}
//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_equal_exact(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "fcmpd d0, d1\n"
    "fmstat\n"
    "movne r0, #0\n"
    "moveq r0, #1\n"
    FUNCTION_MARKER
    :: 
  );
}
__attribute__((naked)) void nseel_asm_equal_exact_end(void) {}
//
//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_notequal_exact(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "fcmpd d0, d1\n"
    "fmstat\n"
    "moveq r0, #0\n"
    "movne r0, #1\n"
    FUNCTION_MARKER
    :: 
  );
}
__attribute__((naked)) void nseel_asm_notequal_exact_end(void) {}
//
//
//
//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_notequal(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "fldd d2, [r6]\n"
    "fsubd d0, d0, d1\n"
    "fabsd d0, d0\n"
    "fcmpd d2, d0\n"
    "fmstat\n"
    "movlt r0, #1\n"
    "movge r0, #0\n"
    FUNCTION_MARKER
    :: 
  );
}
__attribute__((naked)) void nseel_asm_notequal_end(void) {}


//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_below(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "fcmpd d1, d0\n"
    "fmstat\n"
    "movlt r0, #1\n"
    "movge r0, #0\n"
    FUNCTION_MARKER
    ::
  );
}
__attribute__((naked)) void nseel_asm_below_end(void) {}

//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_beloweq(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "fcmpd d1, d0\n"
    "fmstat\n"
    "movle r0, #1\n"
    "movgt r0, #0\n"
    FUNCTION_MARKER
    ::
  );
}
__attribute__((naked)) void nseel_asm_beloweq_end(void) {}


//---------------------------------------------------------------------------------------------------------------
__attribute__((naked)) void nseel_asm_above(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "fcmpd d1, d0\n"
    "fmstat\n"
    "movgt r0, #1\n"
    "movle r0, #0\n"
    FUNCTION_MARKER
    ::
  );
}
__attribute__((naked)) void nseel_asm_above_end(void) {}

__attribute__((naked)) void nseel_asm_aboveeq(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "fcmpd d1, d0\n"
    "fmstat\n"
    "movge r0, #1\n"
    "movlt r0, #0\n"
    FUNCTION_MARKER
    ::
  );
}
__attribute__((naked)) void nseel_asm_aboveeq_end(void) {}



__attribute__((naked)) void nseel_asm_min(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d0, [r0]\n"
    "fldd d1, [r1]\n"
    "fcmpd d1, d0\n"
    "fmstat\n"
    "movlt r0, r1\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_min_end(void) {}

__attribute__((naked)) void nseel_asm_max(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d0, [r0]\n"
    "fldd d1, [r1]\n"
    "fcmpd d1, d0\n"
    "fmstat\n"
    "movge r0, r1\n"
    FUNCTION_MARKER
  );
}

__attribute__((naked)) void nseel_asm_max_end(void) {}


__attribute__((naked)) void nseel_asm_min_fp(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "fcmpd d1, d0\n"
    "fmstat\n"
    "fcpydlt d0, d1\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_min_fp_end(void) {}

__attribute__((naked)) void nseel_asm_max_fp(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r1]\n"
    "fcmpd d1, d0\n"
    "fmstat\n"
    "fcpydge d0, d1\n"
    FUNCTION_MARKER
  );
}

__attribute__((naked)) void nseel_asm_max_fp_end(void) {}






__attribute__((naked)) void _asm_generic3parm(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "push {r4, lr}\n" // input: r0 last, r1=second to last, r2=third to last
                      // output: r0=context, r1=r2, r2=r1, r3=r0
    "mov r3, r0\n" // r0 (last parameter) -> r3
    "mov r4, r1\n" // r1 (second to last parameter) r1->r4->r2

    "movw r0, 0xdead\n"  // r0 is first parm (context)
    "movt r0, 0xbeef\n"  

    "mov r1, r2\n" // r2->r1
    "mov r2, r4\n" // r1->r2

    "movw r4, 0xdead\n" 
    "movt r4, 0xbeef\n"  

    "blx r4\n" 
    "pop {r4, lr}\n"
    FUNCTION_MARKER
  ::
 ); 
}
__attribute__((naked)) void _asm_generic3parm_end(void) {}

__attribute__((naked)) void _asm_generic3parm_retd(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "push {r4, lr}\n" // input: r0 last, r1=second to last, r2=third to last
                      // output: r0=context, r1=r2, r2=r1, r3=r0
    "mov r3, r0\n" // r0 (last parameter) -> r3
    "mov r4, r1\n" // r1 (second to last parameter) r1->r4->r2

    "movw r0, 0xdead\n"  // r0 is first parm (context)
    "movt r0, 0xbeef\n"  

    "mov r1, r2\n" // r2->r1
    "mov r2, r4\n" // r1->r2

    "movw r4, 0xdead\n" 
    "movt r4, 0xbeef\n"  

    "blx r4\n" 
    "pop {r4, lr}\n"
    FUNCTION_MARKER
  ::
 ); 
}
__attribute__((naked)) void _asm_generic3parm_retd_end(void) {}


__attribute__((naked)) void _asm_generic2parm(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "str lr, [sp, #-8]!\n"
    "mov r2, r0\n" // r0 -> r2, r1-r1, 
    "movw r0, 0xdead\n"  // r0 is first parm
    "movt r0, 0xbeef\n"  
    "movw r3, 0xdead\n" 
    "movt r3, 0xbeef\n"  
    "blx r3\n" 
    "ldr lr, [sp], #8\n"
    FUNCTION_MARKER
  ::
 ); 
}
__attribute__((naked)) void _asm_generic2parm_end(void) {}


__attribute__((naked)) void _asm_generic2parm_retd(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "str lr, [sp, #-8]!\n"
    "mov r2, r0\n" // r0 -> r2, r1-r1, 
    "movw r0, 0xdead\n"  // r0 is first parm
    "movt r0, 0xbeef\n"  
    "movw r3, 0xdead\n" 
    "movt r3, 0xbeef\n"  
    "blx r3\n" 
    "ldr lr, [sp], #8\n"
    FUNCTION_MARKER
  ::
 ); 
}
__attribute__((naked)) void _asm_generic2parm_retd_end(void) {}

__attribute__((naked)) void _asm_generic1parm(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "str lr, [sp, #-8]!\n"
    "mov r1, r0\n" // r0 -> r1
    "movw r0, 0xdead\n"  // r0 is first parm
    "movt r0, 0xbeef\n"  
    "movw r3, 0xdead\n" 
    "movt r3, 0xbeef\n"  
    "blx r3\n" 
    "ldr lr, [sp], #8\n"
    FUNCTION_MARKER
  ::
 ); 
}
__attribute__((naked)) void _asm_generic1parm_end(void) {}



__attribute__((naked)) void _asm_generic1parm_retd(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "str lr, [sp, #-8]!\n"
    "mov r1, r0\n" // r0 -> r1
    "movw r0, 0xdead\n"  // r0 is first parm
    "movt r0, 0xbeef\n"  
    "movw r3, 0xdead\n" 
    "movt r3, 0xbeef\n"  
    "blx r3\n" 
    "ldr lr, [sp], #8\n"
    FUNCTION_MARKER
  ::
 ); 
}
__attribute__((naked)) void _asm_generic1parm_retd_end(void) {}




__attribute__((naked)) void _asm_megabuf(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r7, #-8]\n"
    "faddd d0, d0, d1\n"
    "ftouizd s0, d0\n"
    "fmrs r3, s0\n" //  r3 is slot index
    "mov r2, r3, asr %0\n" 
    "bic r2, r2, #3\n"  // r2 is page index*4
    "cmp r2, %1\n"
    "bge 0f\n"

    "add r2, r2, r7\n"
    "ldr r2, [r2]\n"
    "cmp r2, #0\n"
    "beq 0f\n"

    "movw r0, %2\n"
    "and r3, r3, r0\n" // r3 mask item in slot
    "add r0, r2, r3, asl #3\n"  // set result
    "b 1f\n"
    "0:\n"

    // failed, call stub function
    "movw r2, 0xdead\n"
    "movt r2, 0xbeef\n"  
    "str lr, [sp, #-8]!\n"
    "mov r0, r7\n" // first parameter: blocks
    "mov r1, r3\n" // second parameter: slot index
    "blx r2\n" 
    "ldr lr, [sp], #8\n"

    "1:\n"

    FUNCTION_MARKER
  :: 
    "i" (NSEEL_RAM_ITEMSPERBLOCK_LOG2 - 2/*log2(sizeof(void*))*/),
    "i" (NSEEL_RAM_BLOCKS*4 /*(sizeof(void*))*/),
    "i" (NSEEL_RAM_ITEMSPERBLOCK-1)
 ); 
}

__attribute__((naked)) void _asm_megabuf_end(void) {}

__attribute__((naked)) void _asm_gmegabuf(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "movw r0, 0xdead\n"
    "movt r0, 0xbeef\n"  

    "movw r2, 0xdead\n"
    "movt r2, 0xbeef\n"  

    "fldd d1, [r7, #-8]\n"
    "faddd d0, d0, d1\n"
    "ftouizd s0, d0\n"
    "fmrs r1, s0\n" //  r1 is slot index

    "push {r4, lr}\n"

    "blx r2\n"

    "pop {r4, lr}\n"

    FUNCTION_MARKER
  ::
 ); 
}

__attribute__((naked)) void _asm_gmegabuf_end(void) {}

__attribute__((naked)) void nseel_asm_fcall(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "movw r0, 0xdead\n"
    "movt r0, 0xbeef\n"  
    "push {r4, lr}\n"
    "blx r0\n"
    "pop {r4, lr}\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_fcall_end(void) {}



__attribute__((naked)) void nseel_asm_stack_push(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d0, [r0]\n"

    "movw r3, 0xdead\n" // r3 is stack
    "movt r3, 0xbeef\n"  
    "ldr r0, [r3]\n"

    "add r0, r0, #8\n"

    "movw r2, 0xdead\n"
    "movt r2, 0xbeef\n"  
    "and r0, r0, r2\n"
    "movw r2, 0xdead\n"
    "movt r2, 0xbeef\n"  
    "orr r0, r0, r2\n"

    "str r0, [r3]\n"
    "fstd d0, [r0]\n"

    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_stack_push_end(void) {}

__attribute__((naked)) void nseel_asm_stack_pop(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "movw r3, 0xdead\n" // r3 is stack
    "movt r3, 0xbeef\n"  
    "ldr r1, [r3]\n"
    "fldd d0, [r1]\n"
    "sub r1, r1, #8\n"
    "movw r2, 0xdead\n"
    "movt r2, 0xbeef\n"  
    "fstd d0, [r0]\n"
    "and r1, r1, r2\n"
    "movw r2, 0xdead\n"
    "movt r2, 0xbeef\n"  
    "orr r1, r1, r2\n"

    "str r1, [r3]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_stack_pop_end(void) {}



__attribute__((naked)) void nseel_asm_stack_pop_fast(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "movw r3, 0xdead\n" // r3 is stack
    "movt r3, 0xbeef\n"  
    "ldr r1, [r3]\n"
    "mov r0, r1\n"
    "sub r1, r1, #8\n"
    "movw r2, 0xdead\n"
    "movt r2, 0xbeef\n"  
    "and r1, r1, r2\n"
    "movw r2, 0xdead\n"
    "movt r2, 0xbeef\n"  
    "orr r1, r1, r2\n"
    "str r1, [r3]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_stack_pop_fast_end(void) {}

__attribute__((naked)) void nseel_asm_stack_peek(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "movw r3, 0xdead\n" // r3 is stack
    "movt r3, 0xbeef\n"  

    "ftosizd s0, d0\n"
    "fmrs r2, s0\n" // r2 is index in stack

    "ldr r1, [r3]\n"
    "sub r1, r1, r2, asl #3\n"
    "movw r2, 0xdead\n"
    "movt r2, 0xbeef\n"  
    "and r1, r1, r2\n"
    "movw r2, 0xdead\n"
    "movt r2, 0xbeef\n"  
    "orr r0, r1, r2\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_stack_peek_end(void) {}


__attribute__((naked)) void nseel_asm_stack_peek_top(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "movw r3, 0xdead\n" // r3 is stack
    "movt r3, 0xbeef\n"  
    "ldr r0, [r3]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_stack_peek_top_end(void) {}


__attribute__((naked)) void nseel_asm_stack_peek_int(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "movw r3, 0xdead\n" // r3 is stack
    "movt r3, 0xbeef\n"  

    "movw r2, 0xdead\n" // r3 is stack
    "movt r2, 0xbeef\n"  

    "ldr r1, [r3]\n"
    "sub r1, r1, r2\n"
    "movw r2, 0xdead\n"
    "movt r2, 0xbeef\n"  
    "and r1, r1, r2\n"
    "movw r2, 0xdead\n"
    "movt r2, 0xbeef\n"  
    "orr r0, r1, r2\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_stack_peek_int_end(void) {}

__attribute__((naked)) void nseel_asm_stack_exch(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "movw r3, 0xdead\n" // r3 is stack
    "movt r3, 0xbeef\n"  
    "ldr r1, [r3]\n"
    "fldd d0, [r0]\n"
    "fldd d1, [r1]\n"
    "fstd d0, [r1]\n"
    "fstd d1, [r0]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_stack_exch_end(void) {}


__attribute__((naked)) void nseel_asm_booltofp(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "cmp r0, #0\n"
    "flddne d0, [r6, #16]\n"
    "flddeq d0, [r6, #8]\n"
    FUNCTION_MARKER
  );
}
__attribute__((naked)) void nseel_asm_booltofp_end(void){ }

__attribute__((naked)) void nseel_asm_fptobool(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r6]\n"
    "fabsd d0, d0\n"
    "fcmpd d0, d1\n"
    "fmstat\n"
    "movgt r0, #1\n"
    "movlt r0, #0\n"
    FUNCTION_MARKER
    :: 
          );
}
__attribute__((naked)) void nseel_asm_fptobool_end(void){ }

__attribute__((naked)) void nseel_asm_fptobool_rev(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fldd d1, [r6]\n"
    "fabsd d0, d0\n"
    "fcmpd d0, d1\n"
    "fmstat\n"
    "movgt r0, #0\n"
    "movlt r0, #1\n"
    FUNCTION_MARKER
    :: 
          );
}
__attribute__((naked)) void nseel_asm_fptobool_rev_end(void){ }


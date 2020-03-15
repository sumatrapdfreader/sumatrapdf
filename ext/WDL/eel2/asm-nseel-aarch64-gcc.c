#define FUNCTION_MARKER  "mov x0, x0\n" \
                         "mov x1, x1\n" \
                         "mov x2, x2\n"

#if EEL_F_SIZE == 8

void nseel_asm_1pdd(void)
{

  __asm__ __volatile__( 
    FUNCTION_MARKER
    "mov x3, 0xdead\n" 
    "movk x3, 0xbeef, lsl 16\n"  
    "movk x3, 0xbeef, lsl 32\n"  
    "str x30, [sp, #-16]!\n"
    "blr x3\n" 
    "ldr x30, [sp], #16\n"
    FUNCTION_MARKER
   :: );
}
void nseel_asm_1pdd_end(void){}

void nseel_asm_2pdd(void)
{

  __asm__ __volatile__( 
    FUNCTION_MARKER
    "mov x3, 0xdead\n" 
    "movk x3, 0xbeef, lsl 16\n"  
    "movk x3, 0xbeef, lsl 32\n"  
    "fmov d1, d0\n" 
    "ldr d0, [x1]\n" 
    "str x30, [sp, #-16]!\n"
    "blr x3\n" 
    "ldr x30, [sp], #16\n"
    FUNCTION_MARKER
   :: );
};
void nseel_asm_2pdd_end(void){}

void nseel_asm_2pdds(void)
{
  __asm__ __volatile__( 
    FUNCTION_MARKER
    "mov x3, 0xdead\n" 
    "movk x3, 0xbeef, lsl 16\n"  
    "movk x3, 0xbeef, lsl 32\n"  
    "stp x1, x30, [sp, #-16]!\n"
    "fmov d1, d0\n" 
    "ldr d0, [x1]\n" 
    "blr x3\n" 
    "ldp x0, x30, [sp], 16\n"
    "str d0, [x0]\n"
    FUNCTION_MARKER
   :: );
}
void nseel_asm_2pdds_end(void){}

#else // 32 bit floating point calls

#error no 32 bit float support

#endif

//---------------------------------------------------------------------------------------------------------------



// do nothing, eh
void nseel_asm_exec2(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    FUNCTION_MARKER
  );
}
void nseel_asm_exec2_end(void) { }



void nseel_asm_invsqrt(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "mov x0, 0x59df\n"
    "movk x0, 0x5f37, lsl 16\n"
    "fcvt s2, d0\n"
    "ldr d3, [x21, #32]\n"
    "fmov w1, s2\n"
    "fmul d0, d0, d3\n"
    "asr w1, w1, #1\n" 

    "sub w0, w0, w1\n"

    "fmov s4, w0\n"
    "fcvt d1, s4\n"

    "ldr d2, [x21, #40]\n"
    "fmul d0, d0, d1\n"
    "fmul d0, d0, d1\n"
    "fadd d0, d0, d2\n"
    "fmul d0, d0, d1\n"
    
    FUNCTION_MARKER
  );
}
void nseel_asm_invsqrt_end(void) {}

void nseel_asm_dbg_getstackptr(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "mov x0, sp\n"
    "ucvtf d0, x0\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_dbg_getstackptr_end(void) {}


//---------------------------------------------------------------------------------------------------------------
void nseel_asm_sqr(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fmul d0, d0, d0\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_sqr_end(void) {}


//---------------------------------------------------------------------------------------------------------------
void nseel_asm_abs(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fabs d0, d0\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_abs_end(void) {}


//---------------------------------------------------------------------------------------------------------------
void nseel_asm_assign(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "ldr d0, [x0]\n"
   "mov x0, x1\n"
   "str d0, [x1]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_assign_end(void) {}
//
//---------------------------------------------------------------------------------------------------------------
void nseel_asm_assign_fromfp(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "mov x0, x1\n"
   "str d0, [x1]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_assign_fromfp_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_assign_fast(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "ldr d0, [x0]\n"
   "mov x0, x1\n"
   "str d0, [x1]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_assign_fast_end(void) {}
//
//---------------------------------------------------------------------------------------------------------------
void nseel_asm_assign_fast_fromfp(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "mov x0, x1\n"
   "str d0, [x1]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_assign_fast_fromfp_end(void) {}



//---------------------------------------------------------------------------------------------------------------
void nseel_asm_add(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "ldr d1, [x1]\n"
   "fadd d0, d1, d0\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_add_end(void) {}

void nseel_asm_add_op(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "ldr d1, [x1]\n"
   "fadd d0, d1, d0\n"
   "mov x0, x1\n"
   "str d0, [x1]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_add_op_end(void) {}

void nseel_asm_add_op_fast(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "ldr d1, [x1]\n"
   "fadd d0, d1, d0\n"
   "mov x0, x1\n"
   "str d0, [x1]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_add_op_fast_end(void) {}


//---------------------------------------------------------------------------------------------------------------
void nseel_asm_sub(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "ldr d1, [x1]\n"
   "fsub d0, d1, d0\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_sub_end(void) {}

void nseel_asm_sub_op(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "ldr d1, [x1]\n"
   "fsub d0, d1, d0\n"
   "mov x0, x1\n"
   "str d0, [x1]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_sub_op_end(void) {}

void nseel_asm_sub_op_fast(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "ldr d1, [x1]\n"
   "fsub d0, d1, d0\n"
   "mov x0, x1\n"
   "str d0, [x1]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_sub_op_fast_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_mul(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "ldr d1, [x1]\n"
   "fmul d0, d0, d1\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_mul_end(void) {}

void nseel_asm_mul_op(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "ldr d1, [x1]\n"
   "fmul d0, d0, d1\n"
   "mov x0, x1\n"
   "str d0, [x1]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_mul_op_end(void) {}

void nseel_asm_mul_op_fast(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "ldr d1, [x1]\n"
   "fmul d0, d0, d1\n"
   "mov x0, x1\n"
   "str d0, [x1]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_mul_op_fast_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_div(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "ldr d1, [x1]\n"
   "fdiv d0, d1, d0\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_div_end(void) {}

void nseel_asm_div_op(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "ldr d1, [x1]\n"
   "fdiv d0, d1, d0\n"
   "mov x0, x1\n"
   "str d0, [x1]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_div_op_end(void) {}

void nseel_asm_div_op_fast(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "ldr d1, [x1]\n"
   "fdiv d0, d1, d0\n"
   "mov x0, x1\n"
   "str d0, [x1]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_div_op_fast_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_mod(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fabs d0, d0\n"
    "fabs d1, d1\n"
    "fcvtzu w1, d0\n"
    "fcvtzu w0, d1\n"
    "udiv w2, w0, w1\n"
    "msub w0, w2, w1, w0\n"
    "ucvtf d0, w0\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_mod_end(void) {}

void nseel_asm_shl(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcvtzs w0, d1\n"
    "fcvtzs w1, d0\n"
    "lsl w0, w0, w1\n"
    "scvtf d0, w0\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_shl_end(void) {}

void nseel_asm_shr(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcvtzs w0, d1\n"
    "fcvtzs w1, d0\n"
    "asr w0, w0, w1\n"
    "scvtf d0, w0\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_shr_end(void) {}

void nseel_asm_mod_op(void)
{

  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fabs d0, d0\n"
    "fabs d1, d1\n"
    "fcvtzu w3, d0\n"
    "fcvtzu w0, d1\n"
    "udiv w2, w0, w3\n"
    "msub w0, w2, w3, w0\n"
    "ucvtf d0, w0\n"

    "str d0, [x1]\n"
    "mov x0, x1\n"
    FUNCTION_MARKER
  );

}
void nseel_asm_mod_op_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_or(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcvtzs w0, d0\n"
    "fcvtzs w1, d1\n"
    "orr w0, w0, w1\n"
    "scvtf d0, w0\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_or_end(void) {}

void nseel_asm_or0(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fcvtzs w0, d0\n"
    "scvtf d0, w0\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_or0_end(void) {}

void nseel_asm_or_op(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcvtzs w0, d0\n"
    "fcvtzs w3, d1\n"
    "orr w0, w0, w3\n"
    "scvtf d0, w0\n"
    "mov x0, x1\n"
    "str d0, [x1]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_or_op_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_xor(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcvtzs w0, d0\n"
    "fcvtzs w1, d1\n"
    "eor w0, w0, w1\n"
    "scvtf d0, w0\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_xor_end(void) {}

void nseel_asm_xor_op(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcvtzs w0, d0\n"
    "fcvtzs w3, d1\n"
    "eor w0, w0, w3\n"
    "scvtf d0, w0\n"
    "mov x0, x1\n"
    "str d0, [x1]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_xor_op_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_and(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcvtzs w0, d0\n"
    "fcvtzs w1, d1\n"
    "and w0, w0, w1\n"
    "scvtf d0, w0\n"
    FUNCTION_MARKER
  );}
void nseel_asm_and_end(void) {}

void nseel_asm_and_op(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcvtzs w0, d0\n"
    "fcvtzs w3, d1\n"
    "and w0, w0, w3\n"
    "scvtf d0, w0\n"
    "mov x0, x1\n"
    "str d0, [x1]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_and_op_end(void) {}


//---------------------------------------------------------------------------------------------------------------
void nseel_asm_uplus(void) // this is the same as doing nothing, it seems
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    FUNCTION_MARKER
  );
}
void nseel_asm_uplus_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_uminus(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
   "fneg d0, d0\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_uminus_end(void) {}


//---------------------------------------------------------------------------------------------------------------
void nseel_asm_sign(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fmov d1, #-1.0\n"
    "fmov d2, #1.0\n"
    "fcmpe d0, #0.0\n"
    "fcsel d0, d0, d1, gt\n"
    "fcsel d0, d0, d2, lt\n"
    FUNCTION_MARKER
    :: 
  );
}
void nseel_asm_sign_end(void) {}



//---------------------------------------------------------------------------------------------------------------
void nseel_asm_bnot(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "cmp w0, #0\n"
    "cset w0, eq\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_bnot_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_if(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "str x30, [sp, #-16]!\n"
    "mov x1, 0xdead\n" 
    "movk x1, 0xbeef, lsl 16\n"  
    "movk x1, 0xbeef, lsl 32\n"  
    "mov x2, 0xdead\n" 
    "movk x2, 0xbeef, lsl 16\n"  
    "movk x2, 0xbeef, lsl 32\n"  
    "cmp w0, #0\n"
    "csel x1, x1, x2, ne\n"
    "blr x1\n" 
    "ldr x30, [sp], #16\n"
    FUNCTION_MARKER
  :: );
}
void nseel_asm_if_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_repeat(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "fcvtzs w3, d0\n"
    "cmp w3, #0\n"
    "ble 0f\n"
#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN > 0
    "mov x2, %0\n"
    "movk x2, %1, lsl 16\n"
    "cmp w3, w2\n"
    "csel w3, w2, w3, gt\n"
#endif
    "stp x24, x30, [sp, #-16]!\n"

    "mov x24, 0xdead\n"
    "movk x24, 0xbeef, lsl 16\n"  
    "movk x24, 0xbeef, lsl 32\n"  
  "1:\n"
    "stp x3, x22, [sp, #-16]!\n"
    "blr x24\n"
    "ldp x3, x22, [sp], 16\n"
    "sub x3, x3, #1\n"
    "cmp x3, #0\n"
    "bgt 1b\n"
    "ldp x24, x30, [sp], 16\n"
    "0:\n"
    FUNCTION_MARKER
#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN > 0
    ::"g" (NSEEL_LOOPFUNC_SUPPORT_MAXLEN&65535),
      "g" (NSEEL_LOOPFUNC_SUPPORT_MAXLEN>>16)
#endif
  );
}
void nseel_asm_repeat_end(void) {}

void nseel_asm_repeatwhile(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN > 0
    "mov x3, %0\n"
    "movk x3, %1, lsl 16\n"
#endif
    "stp x24, x30, [sp, #-16]!\n"

    "mov x24, 0xdead\n"
    "movk x24, 0xbeef, lsl 16\n"  
    "movk x24, 0xbeef, lsl 32\n"  
  "0:\n"
    "stp x3, x22, [sp, #-16]!\n"
    "blr x24\n"
    "ldp x3, x22, [sp], 16\n"
    "cmp w0, #0\n"
#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN > 0
    "beq 0f\n"
    "sub x3, x3, #1\n"
    "cmp x3, #0\n"
    "bne 0b\n"
    "0:\n"
#else
    "bne 0b\n"
#endif
    "ldp x24, x30, [sp], 16\n"

    FUNCTION_MARKER
#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN > 0
    ::"g" (NSEEL_LOOPFUNC_SUPPORT_MAXLEN&65535),
      "g" (NSEEL_LOOPFUNC_SUPPORT_MAXLEN>>16)
#endif
  );
}
void nseel_asm_repeatwhile_end(void) {}


void nseel_asm_band(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "cmp w0, #0\n"
    "beq 0f\n"
    "mov x3, 0xdead\n"
    "movk x3, 0xbeef, lsl 16\n"  
    "movk x3, 0xbeef, lsl 32\n"  
    "str x30, [sp, #-16]!\n"
    "blr x3\n"
    "ldr x30, [sp], #16\n"
    "0:\n"
    FUNCTION_MARKER
  :: );
}
void nseel_asm_band_end(void) {}

void nseel_asm_bor(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "cmp w0, #0\n"
    "bne 0f\n"
    "mov x3, 0xdead\n"
    "movk x3, 0xbeef, lsl 16\n"  
    "movk x3, 0xbeef, lsl 32\n"  
    "str x30, [sp, #-16]!\n"
    "blr x3\n"
    "ldr x30, [sp], #16\n"
    "0:\n"
    FUNCTION_MARKER
  :: );
}
void nseel_asm_bor_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_equal(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "ldr d2, [x21]\n"
    "fsub d0, d0, d1\n"
    "fabs d0, d0\n"
    "fcmp d0, d2\n"
    "cset w0, lt\n"
    FUNCTION_MARKER
    :: 
  );
}
void nseel_asm_equal_end(void) {}
//---------------------------------------------------------------------------------------------------------------
void nseel_asm_equal_exact(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcmp d1, d0\n"
    "cset w0, eq\n"
    FUNCTION_MARKER
    :: 
  );
}
void nseel_asm_equal_exact_end(void) {}
//
//---------------------------------------------------------------------------------------------------------------
void nseel_asm_notequal_exact(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcmp d1, d0\n"
    "cset w0, ne\n"
    FUNCTION_MARKER
    :: 
  );
}
void nseel_asm_notequal_exact_end(void) {}
//
//
//
//---------------------------------------------------------------------------------------------------------------
void nseel_asm_notequal(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "ldr d2, [x21]\n"
    "fsub d0, d0, d1\n"
    "fabs d0, d0\n"
    "fcmp d0, d2\n"
    "cset w0, ge\n"
    FUNCTION_MARKER
    :: 
  );
}
void nseel_asm_notequal_end(void) {}


//---------------------------------------------------------------------------------------------------------------
void nseel_asm_below(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcmp d1, d0\n"
    "cset w0, lt\n"
    FUNCTION_MARKER
    ::
  );
}
void nseel_asm_below_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_beloweq(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcmp d1, d0\n"
    "cset w0, le\n"
    FUNCTION_MARKER
    ::
  );
}
void nseel_asm_beloweq_end(void) {}


//---------------------------------------------------------------------------------------------------------------
void nseel_asm_above(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcmp d1, d0\n"
    "cset w0, gt\n"
    FUNCTION_MARKER
    ::
  );
}
void nseel_asm_above_end(void) {}

void nseel_asm_aboveeq(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcmp d1, d0\n"
    "cset w0, ge\n"
    FUNCTION_MARKER
    ::
  );
}
void nseel_asm_aboveeq_end(void) {}



void nseel_asm_min(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d0, [x0]\n"
    "ldr d1, [x1]\n"
    "fcmp d1, d0\n"
    "csel x0, x1, x0, lt\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_min_end(void) {}

void nseel_asm_max(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d0, [x0]\n"
    "ldr d1, [x1]\n"
    "fcmp d1, d0\n"
    "csel x0, x1, x0, gt\n"
    FUNCTION_MARKER
  );
}

void nseel_asm_max_end(void) {}


void nseel_asm_min_fp(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcmp d1, d0\n"
    "fcsel d0, d1, d0, lt\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_min_fp_end(void) {}

void nseel_asm_max_fp(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x1]\n"
    "fcmp d1, d0\n"
    "fcsel d0, d1, d0, gt\n"
    FUNCTION_MARKER
  );
}

void nseel_asm_max_fp_end(void) {}






void _asm_generic3parm(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "str x30, [sp, #-16]!\n" // input: r0 last, r1=second to last, r2=third to last
                             // output: r0=context, r1=r2, r2=r1, r3=r0
    "mov x3, x0\n" // r0 (last parameter) -> r3

    "mov x0, 0xdead\n"  // r0 is first parm (context)
    "movk x0, 0xbeef, lsl 16\n"  
    "movk x0, 0xbeef, lsl 32\n"  

    "mov x4, 0xdead\n" 
    "movk x4, 0xbeef, lsl 16\n"  
    "movk x4, 0xbeef, lsl 32\n"  

    "mov x5, x1\n" // swap x1/x2
    "mov x1, x2\n"
    "mov x2, x5\n"

    "blr x4\n" 
    "ldr x30, [sp], #16\n"
    FUNCTION_MARKER
  ::
 ); 
}
void _asm_generic3parm_end(void) {}

void _asm_generic3parm_retd(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "str x30, [sp, #-16]!\n" // input: r0 last, r1=second to last, r2=third to last
    "mov x3, x0\n" // r0 (last parameter) -> r3

    "mov x0, 0xdead\n"  // r0 is first parm (context)
    "movk x0, 0xbeef, lsl 16\n"  
    "movk x0, 0xbeef, lsl 32\n"  

    "mov x4, 0xdead\n" 
    "movk x4, 0xbeef, lsl 16\n"  
    "movk x4, 0xbeef, lsl 32\n"  

    "mov x5, x1\n" // swap x1/x2
    "mov x1, x2\n"
    "mov x2, x5\n"

    "blr x4\n" 
    "ldr x30, [sp], #16\n"
    FUNCTION_MARKER
  ::
 ); 
}
void _asm_generic3parm_retd_end(void) {}


void _asm_generic2parm(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "str x30, [sp, #-16]!\n" // input: r0 last, r1=second to last
    "mov x2, x0\n" 

    "mov x0, 0xdead\n"  // r0 is first parm (context)
    "movk x0, 0xbeef, lsl 16\n"  
    "movk x0, 0xbeef, lsl 32\n"  

    "mov x4, 0xdead\n" 
    "movk x4, 0xbeef, lsl 16\n"  
    "movk x4, 0xbeef, lsl 32\n"  

    "blr x4\n" 
    "ldr x30, [sp], #16\n"
    FUNCTION_MARKER
  ::
 ); 
}
void _asm_generic2parm_end(void) {}


void _asm_generic2parm_retd(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "str x30, [sp, #-16]!\n" // input: r0 last, r1=second to last
    "mov x2, x0\n" 

    "mov x0, 0xdead\n"  // r0 is first parm (context)
    "movk x0, 0xbeef, lsl 16\n"  
    "movk x0, 0xbeef, lsl 32\n"  

    "mov x4, 0xdead\n" 
    "movk x4, 0xbeef, lsl 16\n"  
    "movk x4, 0xbeef, lsl 32\n"  

    "blr x4\n" 
    "ldr x30, [sp], #16\n"
    FUNCTION_MARKER
  ::
 ); 
}
void _asm_generic2parm_retd_end(void) {}

void _asm_generic1parm(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "str x30, [sp, #-16]!\n"
    "mov x1, x0\n" 

    "mov x0, 0xdead\n"  // r0 is first parm (context)
    "movk x0, 0xbeef, lsl 16\n"  
    "movk x0, 0xbeef, lsl 32\n"  

    "mov x4, 0xdead\n" 
    "movk x4, 0xbeef, lsl 16\n"  
    "movk x4, 0xbeef, lsl 32\n"  

    "blr x4\n" 
    "ldr x30, [sp], #16\n"
    FUNCTION_MARKER
  ::
 ); 
}
void _asm_generic1parm_end(void) {}



void _asm_generic1parm_retd(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "str x30, [sp, #-16]!\n"
    "mov x1, x0\n" 

    "mov x0, 0xdead\n"  // r0 is first parm (context)
    "movk x0, 0xbeef, lsl 16\n"  
    "movk x0, 0xbeef, lsl 32\n"  

    "mov x4, 0xdead\n" 
    "movk x4, 0xbeef, lsl 16\n"  
    "movk x4, 0xbeef, lsl 32\n"  

    "blr x4\n" 
    "ldr x30, [sp], #16\n"

    FUNCTION_MARKER
  ::
 ); 
}
void _asm_generic1parm_retd_end(void) {}




void _asm_megabuf(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x20, #-8]\n"
    "fadd d0, d0, d1\n"
    "fcvtzu w3, d0\n"
    "asr w2, w3, %0\n" 
    "bic w2, w2, #7\n"  // r2 is page index*8
    "cmp w2, %1\n"
    "bge 0f\n"

    "add x2, x2, x20\n"
    "ldr x2, [x2]\n"
    "cmp x2, #0\n"
    "beq 0f\n"

    "mov x0, %2\n"
    "and x3, x3, x0\n" // r3 mask item in slot
    "add x0, x2, x3, lsl #3\n"  // set result
    "b 1f\n"
    "0:\n"

    // failed, call stub function
    "mov x2, 0xdead\n"
    "movk x2, 0xbeef, lsl 16\n"  
    "movk x2, 0xbeef, lsl 32\n"  
    "str x30, [sp, #-16]!\n"
    "mov x0, x20\n" // first parameter: blocks
    "mov x1, x3\n" // second parameter: slot index
    "blr x2\n" 
    "ldr x30, [sp], #16\n"
    "1:\n"

    FUNCTION_MARKER
  :: 
    "i" (NSEEL_RAM_ITEMSPERBLOCK_LOG2 - 3/*log2(sizeof(void*))*/),
    "i" (NSEEL_RAM_BLOCKS*sizeof(void*)),
    "i" (NSEEL_RAM_ITEMSPERBLOCK-1)
 ); 
}

void _asm_megabuf_end(void) {}

void _asm_gmegabuf(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "mov x0, 0xdead\n"
    "movk x0, 0xbeef, lsl 16\n"  
    "movk x0, 0xbeef, lsl 32\n"  

    "mov x2, 0xdead\n"
    "movk x2, 0xbeef, lsl 16\n"  
    "movk x2, 0xbeef, lsl 32\n"  

    "ldr d1, [x20, #-8]\n"
    "fadd d0, d0, d1\n"

    "fcvtzu w1, d0\n"

    "str x30, [sp, #-16]!\n"

    "blr x2\n"

    "ldr x30, [sp], #16\n"

    FUNCTION_MARKER
  ::
 ); 
}

void _asm_gmegabuf_end(void) {}

void nseel_asm_fcall(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "mov x0, 0xdead\n"
    "movk x0, 0xbeef, lsl 16\n"  
    "movk x0, 0xbeef, lsl 32\n"  
    "str x30, [sp, #-16]!\n"
    "blr x0\n"
    "ldr x30, [sp], #16\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_fcall_end(void) {}



void nseel_asm_stack_push(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d0, [x0]\n"

    "mov x3, 0xdead\n" // r3 is stack
    "movk x3, 0xbeef, lsl 16\n"  
    "movk x3, 0xbeef, lsl 32\n"  
    "ldr x0, [x3]\n"

    "add x0, x0, #8\n"

    "mov x2, 0xdead\n"
    "movk x2, 0xbeef, lsl 16\n"  
    "movk x2, 0xbeef, lsl 32\n"  
    "and x0, x0, x2\n"
    "mov x2, 0xdead\n"
    "movk x2, 0xbeef, lsl 16\n"  
    "movk x2, 0xbeef, lsl 32\n"  
    "orr x0, x0, x2\n"

    "str x0, [x3]\n"
    "str d0, [x0]\n"

    FUNCTION_MARKER
  );
}
void nseel_asm_stack_push_end(void) {}

void nseel_asm_stack_pop(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "mov x3, 0xdead\n" // r3 is stack
    "movk x3, 0xbeef, lsl 16\n"  
    "movk x3, 0xbeef, lsl 32\n"  
    "ldr x1, [x3]\n"
    "ldr d0, [x1]\n"
    "sub x1, x1, #8\n"
    "mov x2, 0xdead\n"
    "movk x2, 0xbeef, lsl 16\n"  
    "movk x2, 0xbeef, lsl 32\n"  
    "str d0, [x0]\n"
    "and x1, x1, x2\n"
    "mov x2, 0xdead\n"
    "movk x2, 0xbeef, lsl 16\n"  
    "movk x2, 0xbeef, lsl 32\n"  
    "orr x1, x1, x2\n"

    "str x1, [x3]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_stack_pop_end(void) {}



void nseel_asm_stack_pop_fast(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "mov x3, 0xdead\n" // r3 is stack
    "movk x3, 0xbeef, lsl 16\n"  
    "movk x3, 0xbeef, lsl 32\n"  
    "ldr x1, [x3]\n"
    "mov x0, x1\n"
    "sub x1, x1, #8\n"
    "mov x2, 0xdead\n"
    "movk x2, 0xbeef, lsl 16\n"  
    "movk x2, 0xbeef, lsl 32\n"  
    "and x1, x1, x2\n"
    "mov x2, 0xdead\n"
    "movk x2, 0xbeef, lsl 16\n"  
    "movk x2, 0xbeef, lsl 32\n"  
    "orr x1, x1, x2\n"
    "str x1, [x3]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_stack_pop_fast_end(void) {}

void nseel_asm_stack_peek(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "mov x3, 0xdead\n" // r3 is stack
    "movk x3, 0xbeef, lsl 16\n"  
    "movk x3, 0xbeef, lsl 32\n"  

    "fcvtzs w2, d0\n"

    "ldr x1, [x3]\n"
    "sub x1, x1, x2, lsl #3\n"
    "mov x2, 0xdead\n"
    "movk x2, 0xbeef, lsl 16\n"  
    "movk x2, 0xbeef, lsl 32\n"  
    "and x1, x1, x2\n"
    "mov x2, 0xdead\n"
    "movk x2, 0xbeef, lsl 16\n"  
    "movk x2, 0xbeef, lsl 32\n"  
    "orr x0, x1, x2\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_stack_peek_end(void) {}


void nseel_asm_stack_peek_top(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "mov x3, 0xdead\n" // r3 is stack
    "movk x3, 0xbeef, lsl 16\n"  
    "movk x3, 0xbeef, lsl 32\n"  
    "ldr x0, [x3]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_stack_peek_top_end(void) {}


void nseel_asm_stack_peek_int(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "mov x3, 0xdead\n" // r3 is stack
    "movk x3, 0xbeef, lsl 16\n"  
    "movk x3, 0xbeef, lsl 32\n"  

    "mov x2, 0xdead\n" 
    "movk x2, 0xbeef, lsl 16\n"  
    "movk x2, 0xbeef, lsl 32\n"  

    "ldr x1, [x3]\n"
    "sub x1, x1, x2\n"
    "mov x2, 0xdead\n"
    "movk x2, 0xbeef, lsl 16\n"  
    "movk x2, 0xbeef, lsl 32\n"  
    "and x1, x1, x2\n"
    "mov x2, 0xdead\n"
    "movk x2, 0xbeef, lsl 16\n"  
    "movk x2, 0xbeef, lsl 32\n"  
    "orr x0, x1, x2\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_stack_peek_int_end(void) {}

void nseel_asm_stack_exch(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "mov x3, 0xdead\n" // r3 is stack
    "movk x3, 0xbeef, lsl 16\n"  
    "movk x3, 0xbeef, lsl 32\n"  
    "ldr x1, [x3]\n"
    "ldr d0, [x0]\n"
    "ldr d1, [x1]\n"
    "str d0, [x1]\n"
    "str d1, [x0]\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_stack_exch_end(void) {}


void nseel_asm_booltofp(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "cmp w0, #0\n"
    "fmov d0, #1.0\n"
    "movi d1, #0\n"
    "fcsel d0, d0, d1, ne\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_booltofp_end(void){ }

void nseel_asm_fptobool(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x21]\n"
    "fabs d0, d0\n"
    "fcmp d0, d1\n"
    "cset w0, ge\n"
    FUNCTION_MARKER
    :: 
          );
}
void nseel_asm_fptobool_end(void){ }

void nseel_asm_fptobool_rev(void)
{
  __asm__ __volatile__(
    FUNCTION_MARKER
    "ldr d1, [x21]\n"
    "fabs d0, d0\n"
    "fcmp d0, d1\n"
    "cset w0, lt\n"
    FUNCTION_MARKER
    :: 
          );
}
void nseel_asm_fptobool_rev_end(void){ }


/* note: only EEL_F_SIZE=8 is now supported (no float EEL_F's) */

#ifndef AMD64ABI
#define X64_EXTRA_STACK_SPACE 32 // win32 requires allocating space for 4 parameters at 8 bytes each, even though we pass via register
#endif

void nseel_asm_1pdd(void)
{
  __asm__(

    FUNCTION_MARKER
     
    "movl $0xfefefefe, %edi\n" 
#ifdef TARGET_X64
    "fstpl (%rsi)\n"
    "movq (%rsi), %xmm0\n"
    #ifdef AMD64ABI
       "movl %rsi, %r15\n"
       "call *%edi\n" 
       "movl %r15, %rsi\n"
    #else
       "subl X64_EXTRA_STACK_SPACE, %rsp\n"
       "call *%edi\n" 
       "addl X64_EXTRA_STACK_SPACE, %rsp\n"
    #endif
    "movq xmm0, (%rsi)\n"
    "fldl (%rsi)\n"
#else
    "subl $16, %esp\n"
    "fstpl (%esp)\n"
    "call *%edi\n" 
    "addl $16, %esp\n" 
#endif

    FUNCTION_MARKER
     
  );
}
void nseel_asm_1pdd_end(void){}

void nseel_asm_2pdd(void)
{
  __asm__(
    FUNCTION_MARKER
    
    "movl $0xfefefefe, %edi\n"
#ifdef TARGET_X64
    "fstpl 8(%rsi)\n"
    "fstpl (%rsi)\n"
    "movq 8(%rsi), %xmm1\n"
    "movq (%rsi), %xmm0\n"
    #ifdef AMD64ABI
      "movl %rsi, %r15\n"
      "call *%edi\n"
      "movl %r15, %rsi\n"
    #else
      "subl X64_EXTRA_STACK_SPACE, %rsp\n"
      "call *%edi\n"
      "addl X64_EXTRA_STACK_SPACE, %rsp\n"
    #endif
    "movq xmm0, (%rsi)\n"
    "fldl (%rsi)\n"
#else
    "subl $16, %esp\n"
    "fstpl 8(%esp)\n"
    "fstpl (%esp)\n"
    "call *%edi\n"
    "addl $16, %esp\n"
#endif
    
    FUNCTION_MARKER
  );
}
void nseel_asm_2pdd_end(void){}

void nseel_asm_2pdds(void)
{
  __asm__(
    FUNCTION_MARKER
    
    "movl $0xfefefefe, %eax\n"
#ifdef TARGET_X64
    "fstpl (%rsi)\n"
    "movq (%rdi), %xmm0\n"
    "movq (%rsi), %xmm1\n"
    #ifdef AMD64ABI
      "movl %rsi, %r15\n"
      "movl %rdi, %r14\n"
      "call *%eax\n"
      "movl %r14, %rdi\n" /* restore thrashed rdi */
      "movl %r15, %rsi\n"
      "movl %r14, %rax\n" /* set return value */
      "movq xmm0, (%r14)\n"
    #else
      "subl X64_EXTRA_STACK_SPACE, %rsp\n"
      "call *%eax\n"
      "movq xmm0, (%edi)\n"
      "movl %edi, %eax\n" /* set return value */
      "addl X64_EXTRA_STACK_SPACE, %rsp\n"
    #endif
#else
    "subl $8, %esp\n"
    "fstpl (%esp)\n"
    "pushl 4(%edi)\n" /* push parameter */
    "pushl (%edi)\n"    /* push the rest of the parameter */
    "call *%eax\n"
    "addl $16, %esp\n"
    "fstpl (%edi)\n" /* store result */
    "movl %edi, %eax\n" /* set return value */
#endif

    // denormal-fix result (this is only currently used for pow_op, so we want this!)
    "movl 4(%edi), %edx\n"
    "addl $0x00100000, %edx\n"
    "andl $0x7FF00000, %edx\n"
    "cmpl $0x00200000, %edx\n"
    "jg 0f\n"
      "subl %edx, %edx\n"
#ifdef TARGET_X64
      "movll %rdx, (%rdi)\n"
#else
      "movl %edx, (%edi)\n"
      "movl %edx, 4(%edi)\n"
#endif
    "0:\n"

    FUNCTION_MARKER
    
  );
}
void nseel_asm_2pdds_end(void){}



//---------------------------------------------------------------------------------------------------------------


// do nothing, eh
void nseel_asm_exec2(void)
{
   __asm__(
      FUNCTION_MARKER
      ""
      FUNCTION_MARKER
    );
}
void nseel_asm_exec2_end(void) { }



void nseel_asm_invsqrt(void)
{
  __asm__(
      FUNCTION_MARKER
    "movl $0x5f3759df, %edx\n"
    "fsts (%esi)\n"
#ifdef TARGET_X64
    "movl 0xfefefefe, %rax\n"
    "fmul" EEL_F_SUFFIX " (%rax)\n"
    "movsxl (%esi), %rcx\n"
#else
    "fmul" EEL_F_SUFFIX " (0xfefefefe)\n"
    "movl (%esi), %ecx\n"
#endif
    "sarl $1, %ecx\n"
    "subl %ecx, %edx\n"
    "movl %edx, (%esi)\n"
    "fmuls (%esi)\n"
    "fmuls (%esi)\n"
#ifdef TARGET_X64
    "movl 0xfefefefe, %rax\n"
    "fadd" EEL_F_SUFFIX " (%rax)\n"
#else
    "fadd" EEL_F_SUFFIX " (0xfefefefe)\n"
#endif
    "fmuls (%esi)\n"

     FUNCTION_MARKER
  );
}
void nseel_asm_invsqrt_end(void) {}


void nseel_asm_dbg_getstackptr(void)
{
  __asm__(
      FUNCTION_MARKER
#ifdef __clang__
    "ffree %st(0)\n"
#else
    "fstpl %st(0)\n"
#endif
    "movl %esp, (%esi)\n"
    "fildl (%esi)\n"

     FUNCTION_MARKER
  );
}
void nseel_asm_dbg_getstackptr_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_sin(void)
{
  __asm__(
      FUNCTION_MARKER
    "fsin\n"
     FUNCTION_MARKER
  );
}
void nseel_asm_sin_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_cos(void)
{
  __asm__(
      FUNCTION_MARKER
    "fcos\n"
     FUNCTION_MARKER
  );
}
void nseel_asm_cos_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_tan(void)
{
  __asm__(
      FUNCTION_MARKER
    "fptan\n"
    "fstp %st(0)\n"
     FUNCTION_MARKER
  );
}
void nseel_asm_tan_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_sqr(void)
{
  __asm__(
      FUNCTION_MARKER
    "fmul %st(0), %st(0)\n"
     FUNCTION_MARKER
  );
}
void nseel_asm_sqr_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_sqrt(void)
{
  __asm__(
      FUNCTION_MARKER
    "fabs\n"
    "fsqrt\n"
     FUNCTION_MARKER
  );
}
void nseel_asm_sqrt_end(void) {}


//---------------------------------------------------------------------------------------------------------------
void nseel_asm_log(void)
{
  __asm__(
      FUNCTION_MARKER
    "fldln2\n"
    "fxch\n"
    "fyl2x\n"
     FUNCTION_MARKER
  );
}
void nseel_asm_log_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_log10(void)
{
  __asm__(
      FUNCTION_MARKER
    "fldlg2\n"
    "fxch\n"
    "fyl2x\n"
    
     FUNCTION_MARKER
  );
}
void nseel_asm_log10_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_abs(void)
{
  __asm__(
      FUNCTION_MARKER
    "fabs\n"
     FUNCTION_MARKER
  );
}
void nseel_asm_abs_end(void) {}


//---------------------------------------------------------------------------------------------------------------
void nseel_asm_assign(void)
{
#ifdef TARGET_X64

  __asm__(
      FUNCTION_MARKER
    "movll (%rax), %rdx\n"
    "movll %rdx, %rcx\n"
    "shrl $32, %rdx\n"
    "addl $0x00100000, %edx\n"
    "andl $0x7FF00000, %edx\n"
    "cmpl $0x00200000, %edx\n"
    "movll %rdi, %rax\n"
    "jg 0f\n"
      "subl %ecx, %ecx\n"
    "0:\n"
    "movll %rcx, (%edi)\n"

     FUNCTION_MARKER
    );

#else

  __asm__(
      FUNCTION_MARKER
    "movl (%eax), %ecx\n"
    "movl 4(%eax), %edx\n"
    "movl %edx, %eax\n"
    "addl $0x00100000, %eax\n" // if exponent is zero, make exponent 0x7ff, if 7ff, make 7fe
    "andl $0x7ff00000, %eax\n" 
    "cmpl $0x00200000, %eax\n"
    "jg 0f\n"
      "subl %ecx, %ecx\n"
      "subl %edx, %edx\n"
    "0:\n"
    "movl %edi, %eax\n"
    "movl %ecx, (%edi)\n"
    "movl %edx, 4(%edi)\n"

     FUNCTION_MARKER
  );

#endif
}
void nseel_asm_assign_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_assign_fromfp(void)
{
  __asm__(
      FUNCTION_MARKER
    "fstpl (%edi)\n"
    "movl 4(%edi), %edx\n"
    "addl $0x00100000, %edx\n"
    "andl $0x7FF00000, %edx\n"
    "cmpl $0x00200000, %edx\n"
    "movl %edi, %eax\n"
    "jg 0f\n"
      "subl %edx, %edx\n"
#ifdef TARGET_X64
      "movll %rdx, (%rdi)\n"
#else
      "movl %edx, (%edi)\n"
      "movl %edx, 4(%edi)\n"
#endif
    "0:\n"

     FUNCTION_MARKER
    );
}
void nseel_asm_assign_fromfp_end(void) {}


//---------------------------------------------------------------------------------------------------------------
void nseel_asm_assign_fast_fromfp(void)
{
  __asm__(
      FUNCTION_MARKER
    "movl %edi, %eax\n"
    "fstpl (%edi)\n"
     FUNCTION_MARKER
   );
}
void nseel_asm_assign_fast_fromfp_end(void) {}



//---------------------------------------------------------------------------------------------------------------
void nseel_asm_assign_fast(void)
{
#ifdef TARGET_X64

  __asm__(
      FUNCTION_MARKER
    "movll (%rax), %rdx\n"
    "movll %rdx, (%edi)\n"
    "movll %rdi, %rax\n"
     FUNCTION_MARKER
    );

#else

  __asm__(
      FUNCTION_MARKER
    "movl (%eax), %ecx\n"
    "movl %ecx, (%edi)\n"
    "movl 4(%eax), %ecx\n"

    "movl %edi, %eax\n"
    "movl %ecx, 4(%edi)\n"
     FUNCTION_MARKER
  );

#endif
}
void nseel_asm_assign_fast_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_add(void)
{
  __asm__(
          FUNCTION_MARKER
#ifdef __clang__
          "faddp %st(1)\n"
#else
          "fadd\n"
#endif
          FUNCTION_MARKER
          );
}
void nseel_asm_add_end(void) {}

void nseel_asm_add_op(void)
{
  __asm__(
      FUNCTION_MARKER
    "fadd" EEL_F_SUFFIX " (%edi)\n"
    "movl %edi, %eax\n"
    "fstp" EEL_F_SUFFIX " (%edi)\n"

    "movl 4(%edi), %edx\n"
    "addl $0x00100000, %edx\n"
    "andl $0x7FF00000, %edx\n"
    "cmpl $0x00200000, %edx\n"
    "jg 0f\n"
      "subl %edx, %edx\n"
#ifdef TARGET_X64
      "movll %rdx, (%rdi)\n"
#else
      "movl %edx, (%edi)\n"
      "movl %edx, 4(%edi)\n"
#endif
    "0:\n"
     FUNCTION_MARKER
  );
}
void nseel_asm_add_op_end(void) {}

void nseel_asm_add_op_fast(void)
{
  __asm__(
      FUNCTION_MARKER
    "fadd" EEL_F_SUFFIX " (%edi)\n"
    "movl %edi, %eax\n"
    "fstp" EEL_F_SUFFIX " (%edi)\n"
     FUNCTION_MARKER
  );
}
void nseel_asm_add_op_fast_end(void) {}


//---------------------------------------------------------------------------------------------------------------
void nseel_asm_sub(void)
{
  __asm__(
      FUNCTION_MARKER
#ifdef __clang__
    "fsubrp %st(0), %st(1)\n"
#else
  #ifdef __GNUC__
    #ifdef __INTEL_COMPILER
      "fsub\n"
    #else
      "fsubr\n" // gnuc has fsub/fsubr backwards, ack
    #endif
  #else
    "fsub\n"
  #endif
#endif
     FUNCTION_MARKER
  );
}
void nseel_asm_sub_end(void) {}

void nseel_asm_sub_op(void)
{
  __asm__(
      FUNCTION_MARKER
    "fsubr" EEL_F_SUFFIX " (%edi)\n"
    "movl %edi, %eax\n"
    "fstp" EEL_F_SUFFIX " (%edi)\n"

    "movl 4(%edi), %edx\n"
    "addl $0x00100000, %edx\n"
    "andl $0x7FF00000, %edx\n"
    "cmpl $0x00200000, %edx\n"
    "jg 0f\n"
      "subl %edx, %edx\n"
#ifdef TARGET_X64
      "movll %rdx, (%rdi)\n"
#else
      "movl %edx, (%edi)\n"
      "movl %edx, 4(%edi)\n"
#endif
    "0:\n"
     FUNCTION_MARKER
  );
}
void nseel_asm_sub_op_end(void) {}

void nseel_asm_sub_op_fast(void)
{
  __asm__(
      FUNCTION_MARKER
    "fsubr" EEL_F_SUFFIX " (%edi)\n"
    "movl %edi, %eax\n"
    "fstp" EEL_F_SUFFIX " (%edi)\n"
     FUNCTION_MARKER
  );
}
void nseel_asm_sub_op_fast_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_mul(void)
{
  __asm__(
      FUNCTION_MARKER
#ifdef __clang__
          "fmulp %st(0), %st(1)\n"
#else
          "fmul\n"
#endif
     FUNCTION_MARKER
  );
}
void nseel_asm_mul_end(void) {}

void nseel_asm_mul_op(void)
{
  __asm__(
      FUNCTION_MARKER
    "fmul" EEL_F_SUFFIX " (%edi)\n"
    "movl %edi, %eax\n"
    "fstp" EEL_F_SUFFIX " (%edi)\n"

    "movl 4(%edi), %edx\n"
    "addl $0x00100000, %edx\n"
    "andl $0x7FF00000, %edx\n"
    "cmpl $0x00200000, %edx\n"
    "jg 0f\n"
      "subl %edx, %edx\n"
#ifdef TARGET_X64
      "movll %rdx, (%rdi)\n"
#else
      "movl %edx, (%edi)\n"
      "movl %edx, 4(%edi)\n"
#endif
    "0:\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_mul_op_end(void) {}

void nseel_asm_mul_op_fast(void)
{
  __asm__(
      FUNCTION_MARKER
    "fmul" EEL_F_SUFFIX " (%edi)\n"
    "movl %edi, %eax\n"
    "fstp" EEL_F_SUFFIX " (%edi)\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_mul_op_fast_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_div(void)
{
  __asm__(
      FUNCTION_MARKER
#ifdef __clang__
    "fdivrp %st(1)\n"
#else
  #ifdef __GNUC__
    #ifdef __INTEL_COMPILER
      "fdiv\n" 
    #else
      "fdivr\n" // gcc inline asm seems to have fdiv/fdivr backwards
    #endif
  #else
    "fdiv\n"
  #endif
#endif
    FUNCTION_MARKER
  );
}
void nseel_asm_div_end(void) {}

void nseel_asm_div_op(void)
{
  __asm__(
      FUNCTION_MARKER
    "fld" EEL_F_SUFFIX " (%edi)\n"
#ifdef __clang__
    "fdivp %st(1)\n"
#else
  #ifndef __GNUC__
    "fdivr\n"
  #else
    #ifdef __INTEL_COMPILER
      "fdivp %st(1)\n"
    #else
      "fdiv\n"
    #endif
  #endif
#endif
    "movl %edi, %eax\n"
    "fstp" EEL_F_SUFFIX " (%edi)\n"

    "movl 4(%edi), %edx\n"
    "addl $0x00100000, %edx\n"
    "andl $0x7FF00000, %edx\n"
    "cmpl $0x00200000, %edx\n"
    "jg 0f\n"
      "subl %edx, %edx\n"
#ifdef TARGET_X64
      "movll %rdx, (%rdi)\n"
#else
      "movl %edx, (%edi)\n"
      "movl %edx, 4(%edi)\n"
#endif
    "0:\n"

    FUNCTION_MARKER
  );
}
void nseel_asm_div_op_end(void) {}

void nseel_asm_div_op_fast(void)
{
  __asm__(
      FUNCTION_MARKER
    "fld" EEL_F_SUFFIX " (%edi)\n"
#ifdef __clang__
    "fdivp %st(1)\n"
#else
  #ifndef __GNUC__
    "fdivr\n"
  #else 
    #ifdef __INTEL_COMPILER
      "fdivp %st(1)\n"
    #else
      "fdiv\n"
    #endif
  #endif
#endif
    "movl %edi, %eax\n"
    "fstp" EEL_F_SUFFIX " (%edi)\n"

    FUNCTION_MARKER
  );
}
void nseel_asm_div_op_fast_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_mod(void)
{
  __asm__(
      FUNCTION_MARKER
    "fabs\n"
    "fistpl (%esi)\n"
    "fabs\n"
    "fistpl 4(%esi)\n"
    "xorl %edx, %edx\n"
    "cmpl $0, (%esi)\n"
    "je 0f\n" // skip devide, set return to 0
    "movl 4(%esi), %eax\n"
    "divl (%esi)\n"
    "0:\n"
    "movl %edx, (%esi)\n"
    "fildl (%esi)\n"

    FUNCTION_MARKER
  );
}
void nseel_asm_mod_end(void) {}

void nseel_asm_shl(void)
{
  __asm__(
      FUNCTION_MARKER
    "fistpl (%esi)\n"
    "fistpl 4(%esi)\n"
    "movl (%esi), %ecx\n"
    "movl 4(%esi), %eax\n"
    "shll %cl, %eax\n"
    "movl %eax, (%esi)\n"
    "fildl (%esi)\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_shl_end(void) {}

void nseel_asm_shr(void)
{
  __asm__(
      FUNCTION_MARKER
    "fistpl (%esi)\n"
    "fistpl 4(%esi)\n"
    "movl (%esi), %ecx\n"
    "movl 4(%esi), %eax\n"
    "sarl %cl, %eax\n"
    "movl %eax, (%esi)\n"
    "fildl (%esi)\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_shr_end(void) {}


void nseel_asm_mod_op(void)
{
  __asm__(
      FUNCTION_MARKER
    "fld" EEL_F_SUFFIX " (%edi)\n"
    "fxch\n"
    "fabs\n"
    "fistpl (%edi)\n"
    "fabs\n"
    "fistpl (%esi)\n"
    "xorl %edx, %edx\n"
    "cmpl $0, (%edi)\n"
    "je 0f\n" // skip devide, set return to 0
    "movl (%esi), %eax\n"
    "divl (%edi)\n"
    "0:\n"
    "movl %edx, (%edi)\n"
    "fildl (%edi)\n"
    "movl %edi, %eax\n"
    "fstp" EEL_F_SUFFIX " (%edi)\n"

    FUNCTION_MARKER
    );
}
void nseel_asm_mod_op_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_or(void)
{
  __asm__(
      FUNCTION_MARKER
    "fistpll (%esi)\n"
    "fistpll 8(%esi)\n"
#ifdef TARGET_X64
    "movll 8(%rsi), %rdi\n"
    "orll %rdi, (%rsi)\n"
#else
    "movl 8(%esi), %edi\n"
    "movl 12(%esi), %ecx\n"
    "orl %edi, (%esi)\n"
    "orl %ecx, 4(%esi)\n"
#endif
    "fildll (%esi)\n"

    FUNCTION_MARKER
  );
}
void nseel_asm_or_end(void) {}

void nseel_asm_or0(void)
{
  __asm__(
      FUNCTION_MARKER
    "fistpll (%esi)\n"
    "fildll (%esi)\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_or0_end(void) {}

void nseel_asm_or_op(void)
{
  __asm__(
      FUNCTION_MARKER
    "fld" EEL_F_SUFFIX " (%edi)\n"
    "fxch\n"
    "fistpll (%edi)\n"
    "fistpll (%esi)\n"
#ifdef TARGET_X64
    "movll (%rsi), %rax\n"
    "orll %rax, (%rdi)\n"
#else
    "movl (%esi), %eax\n"
    "movl 4(%esi), %ecx\n"
    "orl %eax, (%edi)\n"
    "orl %ecx, 4(%edi)\n"
#endif
    "fildll (%edi)\n"
    "movl %edi, %eax\n"
    "fstp" EEL_F_SUFFIX " (%edi)\n"

    FUNCTION_MARKER
  );
}
void nseel_asm_or_op_end(void) {}


void nseel_asm_xor(void)
{
  __asm__(
      FUNCTION_MARKER
    "fistpll (%esi)\n"
    "fistpll 8(%esi)\n"
#ifdef TARGET_X64
    "movll 8(%rsi), %rdi\n"
    "xorll %rdi, (%rsi)\n"
#else
    "movl 8(%esi), %edi\n"
    "movl 12(%esi), %ecx\n"
    "xorl %edi, (%esi)\n"
    "xorl %ecx, 4(%esi)\n"
#endif
    "fildll (%esi)\n"

    FUNCTION_MARKER
  );
}
void nseel_asm_xor_end(void) {}

void nseel_asm_xor_op(void)
{
  __asm__(
      FUNCTION_MARKER
    "fld" EEL_F_SUFFIX " (%edi)\n"
    "fxch\n"
    "fistpll (%edi)\n"
    "fistpll (%esi)\n"
#ifdef TARGET_X64
    "movll (%rsi), %rax\n"
    "xorll %rax, (%rdi)\n"
#else
    "movl (%esi), %eax\n"
    "movl 4(%esi), %ecx\n"
    "xorl %eax, (%edi)\n"
    "xorl %ecx, 4(%edi)\n"
#endif
    "fildll (%edi)\n"
    "movl %edi, %eax\n"
    "fstp" EEL_F_SUFFIX " (%edi)\n"

    FUNCTION_MARKER
  );
}
void nseel_asm_xor_op_end(void) {}


//---------------------------------------------------------------------------------------------------------------
void nseel_asm_and(void)
{
  __asm__(
      FUNCTION_MARKER
    "fistpll (%esi)\n"
    "fistpll 8(%esi)\n"
#ifdef TARGET_X64
    "movll 8(%rsi), %rdi\n"
    "andll %rdi, (%rsi)\n"
#else
    "movl 8(%esi), %edi\n"
    "movl 12(%esi), %ecx\n"
    "andl %edi, (%esi)\n"
    "andl %ecx, 4(%esi)\n"
#endif
    "fildll (%esi)\n"

    FUNCTION_MARKER
  );
}
void nseel_asm_and_end(void) {}

void nseel_asm_and_op(void)
{
  __asm__(
      FUNCTION_MARKER
    "fld" EEL_F_SUFFIX " (%edi)\n"
    "fxch\n"
    "fistpll (%edi)\n"
    "fistpll (%esi)\n"
#ifdef TARGET_X64
    "movll (%rsi), %rax\n"
    "andll %rax, (%rdi)\n"
#else
    "movl (%esi), %eax\n"
    "movl 4(%esi), %ecx\n"
    "andl %eax, (%edi)\n"
    "andl %ecx, 4(%edi)\n"
#endif
    "fildll (%edi)\n"
    "movl %edi, %eax\n"
    "fstp" EEL_F_SUFFIX " (%edi)\n"

    FUNCTION_MARKER
  );
}
void nseel_asm_and_op_end(void) {}


//---------------------------------------------------------------------------------------------------------------
void nseel_asm_uplus(void) // this is the same as doing nothing, it seems
{
   __asm__(
      FUNCTION_MARKER
      ""
      FUNCTION_MARKER
    );
}
void nseel_asm_uplus_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_uminus(void)
{
  __asm__(
      FUNCTION_MARKER
    "fchs\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_uminus_end(void) {}



//---------------------------------------------------------------------------------------------------------------
void nseel_asm_sign(void)
{
  __asm__(
      FUNCTION_MARKER

#ifdef TARGET_X64


    "fst" EEL_F_SUFFIX " (%rsi)\n"
    "mov" EEL_F_SUFFIX " (%rsi), %rdx\n"
    "movll $0x7FFFFFFFFFFFFFFF, %rcx\n"
    "testll %rcx, %rdx\n"
    "jz 0f\n" // zero zero, return the value passed directly
      // calculate sign
      "incll %rcx\n" // rcx becomes 0x80000...
      "fstp %st(0)\n"
      "fld1\n"
      "testl %rcx, %rdx\n"
      "jz 0f\n"
      "fchs\n"      
  	"0:\n"

#else

    "fsts (%esi)\n"
    "movl (%esi), %ecx\n"
    "movl $0x7FFFFFFF, %edx\n"
    "testl %edx, %ecx\n"
    "jz 0f\n" // zero zero, return the value passed directly
      // calculate sign
      "incl %edx\n" // edx becomes 0x8000...
      "fstp %st(0)\n"
      "fld1\n"
      "testl %edx, %ecx\n"
      "jz 0f\n"
      "fchs\n"      
  	"0:\n"
   
#endif
    FUNCTION_MARKER
);
}
void nseel_asm_sign_end(void) {}



//---------------------------------------------------------------------------------------------------------------
void nseel_asm_bnot(void)
{
  __asm__(
      FUNCTION_MARKER
    "testl %eax, %eax\n"
    "setz %al\n"   
    "andl $0xff, %eax\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_bnot_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_fcall(void)
{
  __asm__(
      FUNCTION_MARKER
     "movl $0xfefefefe, %edx\n"
#ifdef TARGET_X64
     "subl $8, %esp\n" 
     "call *%edx\n"
     "addl $8, %esp\n"
#else
     "subl $12, %esp\n" /* keep stack 16 byte aligned, 4 bytes for return address */
     "call *%edx\n"
     "addl $12, %esp\n"
#endif
      FUNCTION_MARKER
  );
}
void nseel_asm_fcall_end(void) {}

void nseel_asm_band(void)
{
  __asm__(
      FUNCTION_MARKER
    "testl %eax, %eax\n"
    "jz 0f\n"

     "movl $0xfefefefe, %ecx\n"
#ifdef TARGET_X64
        "subl $8, %rsp\n"
#else
        "subl $12, %esp\n"
#endif
        "call *%ecx\n"
#ifdef TARGET_X64
        "addl $8, %rsp\n"
#else
        "addl $12, %esp\n"
#endif
    "0:\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_band_end(void) {}

void nseel_asm_bor(void)
{
  __asm__(
      FUNCTION_MARKER
    "testl %eax, %eax\n"
    "jnz 0f\n"

    "movl $0xfefefefe, %ecx\n"
#ifdef TARGET_X64
    "subl $8, %rsp\n"
#else
    "subl $12, %esp\n"
#endif
    "call *%ecx\n"
#ifdef TARGET_X64
    "addl $8, %rsp\n"
#else
    "addl $12, %esp\n"
#endif
    "0:\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_bor_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_equal(void)
{
  __asm__(
      FUNCTION_MARKER
#ifdef __clang__
    "fsubp %st(1)\n"
#else
    "fsub\n"
#endif

    "fabs\n"
#ifdef TARGET_X64
    "fcomp" EEL_F_SUFFIX " -8(%r12)\n" //[g_closefact]
#else
    "fcomp" EEL_F_SUFFIX " -8(%ebx)\n" //[g_closefact]
#endif
    "fstsw %ax\n"
    "andl $256, %eax\n" // old behavior: if 256 set, true (NaN means true)

    FUNCTION_MARKER
  );
}
void nseel_asm_equal_end(void) {}
//
//---------------------------------------------------------------------------------------------------------------
void nseel_asm_equal_exact(void)
{
  __asm__(
      FUNCTION_MARKER
    "fcompp\n"
    "fstsw %ax\n" // for equal 256 and 1024 should be clear, 16384 should be set
    "andl $17664, %eax\n"  // mask C4/C3/C1, bits 8/10/14, 16384|256|1024 -- if equals 16384, then equality
    "cmp $16384, %eax\n" 
    "je 0f\n"
    "subl %eax, %eax\n"
    "0:\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_equal_exact_end(void) {}

void nseel_asm_notequal_exact(void)
{
  __asm__(
      FUNCTION_MARKER
    "fcompp\n"
    "fstsw %ax\n" // for equal 256 and 1024 should be clear, 16384 should be set
    "andl $17664, %eax\n"  // mask C4/C3/C1, bits 8/10/14, 16384|256|1024 -- if equals 16384, then equality
    "cmp $16384, %eax\n" 
    "je 0f\n"
    "subl %eax, %eax\n"
    "0:\n"
    "xorl $16384, %eax\n" // flip the result
    FUNCTION_MARKER
  );
}
void nseel_asm_notequal_exact_end(void) {}
//
//---------------------------------------------------------------------------------------------------------------
void nseel_asm_notequal(void)
{
  __asm__(
      FUNCTION_MARKER
#ifdef __clang__
    "fsubp %st(1)\n"
#else
    "fsub\n"
#endif

    "fabs\n"
#ifdef TARGET_X64
    "fcomp" EEL_F_SUFFIX " -8(%r12)\n" //[g_closefact]
#else
    "fcomp" EEL_F_SUFFIX " -8(%ebx)\n" //[g_closefact]
#endif
    "fstsw %ax\n"
    "andl $256, %eax\n"
    "xorl $256, %eax\n" // old behavior: if 256 set, FALSE (NaN makes for false)
    FUNCTION_MARKER
  );
}
void nseel_asm_notequal_end(void) {}


//---------------------------------------------------------------------------------------------------------------
void nseel_asm_above(void)
{
  __asm__(
      FUNCTION_MARKER
    "fcompp\n"
    "fstsw %ax\n"
    "andl $1280, %eax\n" //  (1024+256) old behavior: NaN would mean 1, preserve that
    FUNCTION_MARKER
  );
}
void nseel_asm_above_end(void) {}

//---------------------------------------------------------------------------------------------------------------
void nseel_asm_beloweq(void)
{
  __asm__(
      FUNCTION_MARKER
    "fcompp\n"
    "fstsw %ax\n"
    "andl $256, %eax\n" // old behavior: NaN would be 0 (ugh)
    "xorl $256, %eax\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_beloweq_end(void) {}


void nseel_asm_booltofp(void)
{
  __asm__(
      FUNCTION_MARKER
    "testl %eax, %eax\n"
    "jz 0f\n"
    "fld1\n"
    "jmp 1f\n"
    "0:\n"
    "fldz\n"
    "1:\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_booltofp_end(void) {}

void nseel_asm_fptobool(void)
{
  __asm__(
      FUNCTION_MARKER
    "fabs\n"
#ifdef TARGET_X64
    "fcomp" EEL_F_SUFFIX " -8(%r12)\n" //[g_closefact]
#else
    "fcomp" EEL_F_SUFFIX " -8(%ebx)\n" //[g_closefact]
#endif
    "fstsw %ax\n"
    "andl $256, %eax\n"
    "xorl $256, %eax\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_fptobool_end(void) {}

void nseel_asm_fptobool_rev(void)
{
  __asm__(
      FUNCTION_MARKER
    "fabs\n"
#ifdef TARGET_X64
    "fcomp" EEL_F_SUFFIX " -8(%r12)\n" //[g_closefact]
#else
    "fcomp" EEL_F_SUFFIX " -8(%ebx)\n" //[g_closefact]
#endif
    "fstsw %ax\n"
    "andl $256, %eax\n"
    FUNCTION_MARKER
  );
}
void nseel_asm_fptobool_rev_end(void) {}

void nseel_asm_min(void)
{
  __asm__(
      FUNCTION_MARKER
    "fld" EEL_F_SUFFIX " (%edi)\n"
    "fcomp" EEL_F_SUFFIX " (%eax)\n"
    "movl %eax, %ecx\n"
    "fstsw %ax\n"
    "testl $256, %eax\n"
    "movl %ecx, %eax\n"
    "jz 0f\n"
    "movl %edi, %eax\n"
    "0:\n"
    FUNCTION_MARKER
    );

}
void nseel_asm_min_end(void) {}

void nseel_asm_max(void)
{
  __asm__(
      FUNCTION_MARKER
    "fld" EEL_F_SUFFIX " (%edi)\n"
    "fcomp" EEL_F_SUFFIX " (%eax)\n"
    "movl %eax, %ecx\n"
    "fstsw %ax\n"
    "testl $256, %eax\n"
    "movl %ecx, %eax\n"
    "jnz 0f\n"
    "movl %edi, %eax\n"
    "0:\n"
    FUNCTION_MARKER
    );
}
void nseel_asm_max_end(void) {}



void nseel_asm_min_fp(void)
{
  __asm__(
      FUNCTION_MARKER
    "fcom\n"
    "fstsw %ax\n"
    "testl $256, %eax\n"
    "jz 0f\n"
    "fxch\n"
    "0:\n"
    "fstp %st(0)\n"
    FUNCTION_MARKER
    );

}
void nseel_asm_min_fp_end(void) {}

void nseel_asm_max_fp(void)
{
  __asm__(
      FUNCTION_MARKER
    "fcom\n"
    "fstsw %ax\n"
    "testl $256, %eax\n"
    "jnz 0f\n"
    "fxch\n"
    "0:\n"
    "fstp %st(0)\n"
    FUNCTION_MARKER
    );
}
void nseel_asm_max_fp_end(void) {}



// just generic functions left, yay




void _asm_generic3parm(void)
{
  __asm__(
      FUNCTION_MARKER
#ifdef TARGET_X64

#ifdef AMD64ABI

    "movl %rsi, %r15\n"
    "movl %rdi, %rdx\n" // third parameter = parm
    "movl $0xfefefefe, %rdi\n" // first parameter= context

    "movl %ecx, %rsi\n" // second parameter = parm
    "movl %rax, %rcx\n" // fourth parameter = parm
    "movl $0xfefefefe, %rax\n" // call function
    "call *%rax\n"

    "movl %r15, %rsi\n"
#else
    "movl %ecx, %edx\n" // second parameter = parm
    "movl $0xfefefefe, %ecx\n" // first parameter= context
    "movl %rdi, %r8\n" // third parameter = parm
    "movl %rax, %r9\n" // fourth parameter = parm
    "movl $0xfefefefe, %edi\n" // call function
    "subl X64_EXTRA_STACK_SPACE, %rsp\n"
    "call *%edi\n"
    "addl X64_EXTRA_STACK_SPACE, %rsp\n"
#endif

#else
    
    "movl $0xfefefefe, %edx\n"
    "pushl %eax\n" // push parameter
    "pushl %edi\n" // push parameter
    "movl $0xfefefefe, %edi\n"
    "pushl %ecx\n" // push parameter
    "pushl %edx\n" // push context pointer
    "call *%edi\n"
    "addl $16, %esp\n"
    
#endif
    FUNCTION_MARKER
 );
}
void _asm_generic3parm_end(void) {}


void _asm_generic3parm_retd(void)
{
  __asm__(
      FUNCTION_MARKER
#ifdef TARGET_X64
#ifdef AMD64ABI
    "movl %rsi, %r15\n"
    "movl %rdi, %rdx\n" // third parameter = parm
    "movl $0xfefefefe, %rdi\n" // first parameter= context
    "movl %ecx, %rsi\n" // second parameter = parm
    "movl %rax, %rcx\n" // fourth parameter = parm
    "movl $0xfefefefe, %rax\n" // call function
    "call *%rax\n"
    "movl %r15, %rsi\n"
    "movq xmm0, (%r15)\n"
    "fldl (%r15)\n"
#else
    "movl %ecx, %edx\n" // second parameter = parm
    "movl $0xfefefefe, %ecx\n" // first parameter= context
    "movl %rdi, %r8\n" // third parameter = parm
    "movl %rax, %r9\n" // fourth parameter = parm
    "movl $0xfefefefe, %edi\n" // call function
    "subl X64_EXTRA_STACK_SPACE, %rsp\n"
    "call *%edi\n"
    "addl X64_EXTRA_STACK_SPACE, %rsp\n"
    "movq xmm0, (%rsi)\n"
    "fldl (%rsi)\n"
#endif
#else
    
    "subl $16, %esp\n"
    "movl $0xfefefefe, %edx\n"
    "movl %edi, 8(%esp)\n"
    "movl $0xfefefefe, %edi\n"
    "movl %eax, 12(%esp)\n"
    "movl %ecx, 4(%esp)\n"
    "movl %edx, (%esp)\n"
    "call *%edi\n"
    "addl $16, %esp\n"
    
#endif
    FUNCTION_MARKER
 );
}
void _asm_generic3parm_retd_end(void) {}


void _asm_generic2parm(void) // this prob neds to be fixed for ppc
{
  __asm__(
      FUNCTION_MARKER
#ifdef TARGET_X64

#ifdef AMD64ABI
    "movl %rsi, %r15\n"
    "movl %edi, %esi\n" // second parameter = parm
    "movl $0xfefefefe, %edi\n" // first parameter= context
    "movl %rax, %rdx\n" // third parameter = parm
    "movl $0xfefefefe, %rcx\n" // call function
    "call *%rcx\n"
    "movl %r15, %rsi\n"
#else
    "movl $0xfefefefe, %ecx\n" // first parameter= context
    "movl %edi, %edx\n" // second parameter = parm
    "movl %rax, %r8\n" // third parameter = parm
    "movl $0xfefefefe, %edi\n" // call function
    "subl X64_EXTRA_STACK_SPACE, %rsp\n"
    "call *%edi\n"
    "addl X64_EXTRA_STACK_SPACE, %rsp\n"
#endif
#else
    
    "movl $0xfefefefe, %edx\n"
    "movl $0xfefefefe, %ecx\n"
    "subl $4, %esp\n" // keep stack aligned
    "pushl %eax\n" // push parameter
    "pushl %edi\n" // push parameter
    "pushl %edx\n" // push context pointer
    "call *%ecx\n"
    "addl $16, %esp\n"
    
#endif
    FUNCTION_MARKER
 );
}
void _asm_generic2parm_end(void) {}


void _asm_generic2parm_retd(void)
{
  __asm__(
      FUNCTION_MARKER
#ifdef TARGET_X64
#ifdef AMD64ABI
    "movl %rsi, %r15\n"
    "movl %rdi, %rsi\n" // second parameter = parm
    "movl $0xfefefefe, %rdi\n" // first parameter= context
    "movl $0xfefefefe, %rcx\n" // call function
    "movl %rax, %rdx\n" // third parameter = parm
    "call *%rcx\n"
    "movl %r15, %rsi\n"
    "movq xmm0, (%r15)\n"
    "fldl (%r15)\n"
#else
    "movl %rdi, %rdx\n" // second parameter = parm
    "movl $0xfefefefe, %rcx\n" // first parameter= context
    "movl $0xfefefefe, %rdi\n" // call function
    "movl %rax, %r8\n" // third parameter = parm
    "subl X64_EXTRA_STACK_SPACE, %rsp\n"
    "call *%edi\n"
    "addl X64_EXTRA_STACK_SPACE, %rsp\n"
    "movq xmm0, (%rsi)\n"
    "fldl (%rsi)\n"
#endif
#else
    
    "subl $16, %esp\n"
    "movl $0xfefefefe, %edx\n"
    "movl $0xfefefefe, %ecx\n"
    "movl %edx, (%esp)\n"
    "movl %edi, 4(%esp)\n"
    "movl %eax, 8(%esp)\n"
    "call *%ecx\n"
    "addl $16, %esp\n"
    
#endif
    FUNCTION_MARKER
 );
}
void _asm_generic2parm_retd_end(void) {}





void _asm_generic1parm(void) 
{
  __asm__(
      FUNCTION_MARKER
#ifdef TARGET_X64
#ifdef AMD64ABI
    "movl $0xfefefefe, %rdi\n" // first parameter= context
    "movl %rsi, %r15\n"
    "movl %eax, %rsi\n" // second parameter = parm
    "movl $0xfefefefe, %rcx\n" // call function
    "call *%rcx\n"
    "movl %r15, %rsi\n"
#else
    "movl $0xfefefefe, %ecx\n" // first parameter= context
    "movl %eax, %edx\n" // second parameter = parm
    "movl $0xfefefefe, %edi\n" // call function
    "subl X64_EXTRA_STACK_SPACE, %rsp\n"
    "call *%edi\n"
    "addl X64_EXTRA_STACK_SPACE, %rsp\n"
#endif
#else
    
    "movl $0xfefefefe, %edx\n"
    "subl $8, %esp\n" // keep stack aligned
    "movl $0xfefefefe, %ecx\n"
    "pushl %eax\n" // push parameter
    "pushl %edx\n" // push context pointer
    "call *%ecx\n"
    "addl $16, %esp\n"
    
#endif

    FUNCTION_MARKER
 );
}
void _asm_generic1parm_end(void) {}


void _asm_generic1parm_retd(void) // 1 parameter returning double
{
  __asm__(
      FUNCTION_MARKER
#ifdef TARGET_X64
#ifdef AMD64ABI
    "movl $0xfefefefe, %rdi\n" // first parameter = context pointer
    "movl $0xfefefefe, %rcx\n" // function address
    "movl %rsi, %r15\n" // save rsi
    "movl %rax, %rsi\n" // second parameter = parameter

    "call *%rcx\n"
    
    "movl %r15, %rsi\n"
    "movq xmm0, (%r15)\n"
    "fldl (%r15)\n"
#else
    "movl $0xfefefefe, %ecx\n" // first parameter= context
    "movl $0xfefefefe, %edi\n" // call function

    "movl %rax, %rdx\n" // second parameter = parm

    "subl X64_EXTRA_STACK_SPACE, %rsp\n"
    "call *%edi\n"
    "addl X64_EXTRA_STACK_SPACE, %rsp\n"
    "movq xmm0, (%rsi)\n"
    "fldl (%rsi)\n"
#endif
#else
    
    "movl $0xfefefefe, %edx\n" // context pointer
    "movl $0xfefefefe, %ecx\n" // func-addr
    "subl $16, %esp\n"
    "movl %eax, 4(%esp)\n" // push parameter
    "movl %edx, (%esp)\n" // push context pointer
    "call *%ecx\n"
    "addl $16, %esp\n"
    
#endif
    FUNCTION_MARKER
 );
}
void _asm_generic1parm_retd_end(void) {}





// this gets its own stub because it's pretty crucial for performance :/

void _asm_megabuf(void)
{
  __asm__(

      FUNCTION_MARKER

#ifdef TARGET_X64


#ifdef AMD64ABI

    "fadd" EEL_F_SUFFIX " -8(%r12)\n"

    "fistpl (%rsi)\n"

    // check if (%rsi) is in range, and buffer available, otherwise call function
    "movl (%rsi), %edx\n"
    "cmpl %1, %rdx\n"      //REPLACE=((NSEEL_RAM_BLOCKS*NSEEL_RAM_ITEMSPERBLOCK))
    "jae 0f\n"
      "movll %rdx, %rax\n"
      "shrll %2, %rax\n"     //REPLACE=(NSEEL_RAM_ITEMSPERBLOCK_LOG2 - 3/*log2(sizeof(void *))*/   )
      "andll %3, %rax\n"     //REPLACE=((NSEEL_RAM_BLOCKS-1)*8 /*sizeof(void*)*/                   )
      "movll (%r12, %rax), %rax\n"
      "testl %rax, %rax\n"
      "jnz 1f\n"   
    "0:\n"
      "movl $0xfefefefe, %rax\n"
      "movl %r12, %rdi\n" // set first parm to ctx
      "movl %rsi, %r15\n" // save rsi
      "movl %rdx, %esi\n" // esi becomes second parameter (edi is first, context pointer)
      "call *%rax\n"
      "movl %r15, %rsi\n" // restore rsi
      "jmp 2f\n"
    "1:\n"
      "andll %4, %rdx\n"      //REPLACE=(NSEEL_RAM_ITEMSPERBLOCK-1)
      "shlll $3, %rdx\n"      // 3 is log2(sizeof(EEL_F))
      "addll %rdx, %rax\n"
    "2:\n"

#else

    "fadd" EEL_F_SUFFIX " -8(%r12)\n"

    "fistpl (%rsi)\n"

    // check if (%rsi) is in range...
    "movl (%rsi), %edi\n"
    "cmpl %1, %edi\n"       //REPLACE=((NSEEL_RAM_BLOCKS*NSEEL_RAM_ITEMSPERBLOCK))
    "jae 0f\n"
      "movll %rdi, %rax\n"
      "shrll %2, %rax\n"       //REPLACE=(NSEEL_RAM_ITEMSPERBLOCK_LOG2 - 3/*log2(sizeof(void *))*/   )
      "andll %3, %rax\n"       //REPLACE=((NSEEL_RAM_BLOCKS-1)*8 /*sizeof(void*)*/                   )
      "movll (%r12, %rax), %rax\n"
      "testl %rax, %rax\n"
      "jnz 1f\n"
    "0:\n"
      "movl $0xfefefefe, %rax\n" // function ptr
      "movl %r12, %rcx\n" // set first parm to ctx
      "movl %rdi, %rdx\n" // rdx is second parameter (rcx is first)
      "subl X64_EXTRA_STACK_SPACE, %rsp\n"
      "call *%rax\n"
      "addl X64_EXTRA_STACK_SPACE, %rsp\n"
      "jmp 2f\n"
    "1:\n"
      "andll %4, %rdi\n"       //REPLACE=(NSEEL_RAM_ITEMSPERBLOCK-1)
      "shlll $3, %rdi\n"       // 3 is log2(sizeof(EEL_F))
      "addll %rdi, %rax\n"
    "2:\n"
#endif


    FUNCTION_MARKER
#else
    "fadd" EEL_F_SUFFIX " -8(%%ebx)\n"
    "fistpl (%%esi)\n"

    // check if (%esi) is in range, and buffer available, otherwise call function
    "movl (%%esi), %%edi\n"
    "cmpl %0, %%edi\n"     //REPLACE=((NSEEL_RAM_BLOCKS*NSEEL_RAM_ITEMSPERBLOCK))
    "jae 0f\n"

      "movl %%edi, %%eax\n"
      "shrl %1, %%eax\n"      //REPLACE=(NSEEL_RAM_ITEMSPERBLOCK_LOG2 - 2/*log2(sizeof(void *))*/   )
      "andl %2, %%eax\n"      //REPLACE=((NSEEL_RAM_BLOCKS-1)*4 /*sizeof(void*)*/                   )
      "movl (%%ebx, %%eax), %%eax\n"
      "testl %%eax, %%eax\n"
      "jnz 1f\n"
    "0:\n"
      "subl $8, %%esp\n" // keep stack aligned
      "movl $0xfefefefe, %%ecx\n"
      "pushl %%edi\n" // parameter
      "pushl %%ebx\n" // push context pointer
      "call *%%ecx\n"
      "addl $16, %%esp\n"
      "jmp 2f\n"
    "1:\n"
      "andl %3, %%edi\n"      //REPLACE=(NSEEL_RAM_ITEMSPERBLOCK-1)
      "shll $3, %%edi\n"      // 3 is log2(sizeof(EEL_F))
      "addl %%edi, %%eax\n"
    "2:"
    FUNCTION_MARKER

    #ifndef _MSC_VER
        :: "i" (((NSEEL_RAM_BLOCKS*NSEEL_RAM_ITEMSPERBLOCK))),
           "i" ((NSEEL_RAM_ITEMSPERBLOCK_LOG2 - 2/*log2(sizeof(void *))*/   )),
           "i" (((NSEEL_RAM_BLOCKS-1)*4 /*sizeof(void*)*/                   )),
           "i" ((NSEEL_RAM_ITEMSPERBLOCK-1                                  ))
    #endif



#endif

  );
}

void _asm_megabuf_end(void) {}


void _asm_gmegabuf(void)
{
  __asm__(

      FUNCTION_MARKER

#ifdef TARGET_X64


#ifdef AMD64ABI

    "movl %rsi, %r15\n"
    "fadd" EEL_F_SUFFIX " -8(%r12)\n"
    "movl $0xfefefefe, %rdi\n" // first parameter = context pointer
    "fistpl (%rsi)\n"
    "movl $0xfefefefe, %edx\n"
    "movl (%rsi), %esi\n" 
    "call *%rdx\n"
    "movl %r15, %rsi\n"

#else
    "fadd" EEL_F_SUFFIX " -8(%r12)\n"
    "movl $0xfefefefe, %rcx\n" // first parameter = context pointer
    "fistpl (%rsi)\n"
    "movl $0xfefefefe, %rdi\n"
    "movl (%rsi), %edx\n"
    "subl X64_EXTRA_STACK_SPACE, %rsp\n"
    "call *%rdi\n"
    "addl X64_EXTRA_STACK_SPACE, %rsp\n"
#endif


#else
    "subl $16, %esp\n" // keep stack aligned
    "movl $0xfefefefe, (%esp)\n"
    "fadd" EEL_F_SUFFIX " -8(%ebx)\n"
    "movl $0xfefefefe, %edi\n"
    "fistpl 4(%esp)\n"
    "call *%edi\n"
    "addl $16, %esp\n"

#endif



    FUNCTION_MARKER
 );
}

void _asm_gmegabuf_end(void) {}

void nseel_asm_stack_push(void)
{
#ifdef TARGET_X64
  __asm__(
      FUNCTION_MARKER
    "movl $0xfefefefe, %rdi\n"
    "movll (%rax), %rcx\n"
    "movll (%rdi), %rax\n"
    "addll $8, %rax\n"
    "movl $0xFEFEFEFE, %rdx\n"
    "andll %rdx, %rax\n"
    "movl $0xFEFEFEFE, %rdx\n"
    "orll %rdx, %rax\n"
    "movll %rcx, (%rax)\n"
    "movll %rax, (%rdi)\n"
    FUNCTION_MARKER
    );
#else

  __asm__(
      FUNCTION_MARKER
    "movl $0xfefefefe, %edi\n"
    
    "movl (%eax), %ecx\n"
    "movl 4(%eax), %edx\n"

    "movl (%edi), %eax\n"

    "addl $8, %eax\n"
    "andl $0xfefefefe, %eax\n"
    "orl $0xfefefefe, %eax\n"
    
    "movl %ecx, (%eax)\n"
    "movl %edx, 4(%eax)\n"

    "movl %eax, (%edi)\n"
    FUNCTION_MARKER
  );

#endif

}
void nseel_asm_stack_push_end(void) {}



void nseel_asm_stack_pop(void)
{
#ifdef TARGET_X64

  __asm__(
      FUNCTION_MARKER
      "movl $0xfefefefe, %rdi\n"
      "movll (%rdi), %rcx\n"
      "movq (%rcx), %xmm0\n"
      "subll $8, %rcx\n"
      "movl $0xFEFEFEFE, %rdx\n"
      "andll %rdx, %rcx\n"
      "movl $0xFEFEFEFE, %rdx\n"
      "orll %rdx, %rcx\n"
      "movll %rcx, (%rdi)\n"
      "movq %xmm0, (%eax)\n"
      FUNCTION_MARKER
    );

#else

  __asm__(
      FUNCTION_MARKER
    "movl $0xfefefefe, %edi\n"
    "movl (%edi), %ecx\n"
    "fld" EEL_F_SUFFIX  " (%ecx)\n"
    "subl $8, %ecx\n"
    "andl $0xfefefefe, %ecx\n"
    "orl $0xfefefefe, %ecx\n"
    "movl %ecx, (%edi)\n"
    "fstp" EEL_F_SUFFIX " (%eax)\n"
    FUNCTION_MARKER
  );

#endif
}
void nseel_asm_stack_pop_end(void) {}


void nseel_asm_stack_pop_fast(void)
{
#ifdef TARGET_X64

  __asm__(
      FUNCTION_MARKER
      "movl $0xfefefefe, %rdi\n"
      "movll (%rdi), %rcx\n"
      "movll %rcx, %rax\n"
      "subll $8, %rcx\n"
      "movl $0xFEFEFEFE, %rdx\n"
      "andll %rdx, %rcx\n"
      "movl $0xFEFEFEFE, %rdx\n"
      "orll %rdx, %rcx\n"
      "movll %rcx, (%rdi)\n"
      FUNCTION_MARKER
    );

#else

  __asm__(
      FUNCTION_MARKER
    "movl $0xfefefefe, %edi\n"
    "movl (%edi), %ecx\n"
    "movl %ecx, %eax\n"
    "subl $8, %ecx\n"
    "andl $0xfefefefe, %ecx\n"
    "orl $0xfefefefe, %ecx\n"
    "movl %ecx, (%edi)\n"        
    FUNCTION_MARKER
  );

#endif
}
void nseel_asm_stack_pop_fast_end(void) {}

void nseel_asm_stack_peek_int(void)
{
#ifdef TARGET_X64

  __asm__(
      FUNCTION_MARKER
    "movll $0xfefefefe, %rdi\n"
    "movll (%rdi), %rax\n"   
    "movl $0xfefefefe, %rdx\n"
    "subll %rdx, %rax\n"
    "movl $0xFEFEFEFE, %rdx\n"
    "andll %rdx, %rax\n"
    "movl $0xFEFEFEFE, %rdx\n"
    "orll %rdx, %rax\n"
    FUNCTION_MARKER
  );

#else

  __asm__(
      FUNCTION_MARKER
    "movl $0xfefefefe, %edi\n"
    "movl (%edi), %eax\n"   
    "movl $0xfefefefe, %edx\n"
    "subl %edx, %eax\n"
    "andl $0xfefefefe, %eax\n"
    "orl $0xfefefefe, %eax\n"
    FUNCTION_MARKER
  );

#endif

}
void nseel_asm_stack_peek_int_end(void) {}



void nseel_asm_stack_peek(void)
{
#ifdef TARGET_X64

  __asm__(
      FUNCTION_MARKER
    "movll $0xfefefefe, %rdi\n"
    "fistpl (%rsi)\n"
    "movll (%rdi), %rax\n"   
    "movll (%rsi), %rdx\n"
    "shll $3, %rdx\n" // log2(sizeof(EEL_F))
    "subl %rdx, %rax\n"
    "movl $0xFEFEFEFE, %rdx\n"
    "andll %rdx, %rax\n"
    "movl $0xFEFEFEFE, %rdx\n"
    "orll %rdx, %rax\n"
    FUNCTION_MARKER
  );

#else

  __asm__(
      FUNCTION_MARKER
    "movl $0xfefefefe, %edi\n"
    "fistpl (%esi)\n"
    "movl (%edi), %eax\n"   
    "movl (%esi), %edx\n"
    "shll $3, %edx\n" // log2(sizeof(EEL_F))
    "subl %edx, %eax\n"
    "andl $0xfefefefe, %eax\n"
    "orl $0xfefefefe, %eax\n"
    FUNCTION_MARKER
  );

#endif

}
void nseel_asm_stack_peek_end(void) {}


void nseel_asm_stack_peek_top(void)
{
#ifdef TARGET_X64

  __asm__(
      FUNCTION_MARKER
    "movll $0xfefefefe, %rdi\n"
    "movll (%rdi), %rax\n"   
    FUNCTION_MARKER
  );

#else

  __asm__(
      FUNCTION_MARKER
    "movl $0xfefefefe, %edi\n"
    "movl (%edi), %eax\n"   
    FUNCTION_MARKER
  );

#endif

}
void nseel_asm_stack_peek_top_end(void) {}

void nseel_asm_stack_exch(void)
{
#ifdef TARGET_X64

  __asm__(
      FUNCTION_MARKER
    "movll $0xfefefefe, %rdi\n"
    "movll (%rdi), %rcx\n"   
    "movq (%rcx), %xmm0\n"
    "movq (%rax), %xmm1\n"
    "movq %xmm0, (%rax)\n"
    "movq %xmm1, (%rcx)\n"
    FUNCTION_MARKER
  );

#else

  __asm__(
      FUNCTION_MARKER
    "movl $0xfefefefe, %edi\n"
    "movl (%edi), %ecx\n"   
    "fld" EEL_F_SUFFIX  " (%ecx)\n"
    "fld" EEL_F_SUFFIX  " (%eax)\n"
    "fstp" EEL_F_SUFFIX  " (%ecx)\n"
    "fstp" EEL_F_SUFFIX " (%eax)\n"
    FUNCTION_MARKER
  );

#endif

}
void nseel_asm_stack_exch_end(void) {}

#ifdef TARGET_X64
void eel_callcode64() 
{
	__asm__(
#ifndef EEL_X64_NO_CHANGE_FPFLAGS
		"subl $16, %rsp\n"
		"fnstcw (%rsp)\n"
		"mov (%rsp), %ax\n"
		"or $0xE3F, %ax\n" // 53 or 64 bit precision, trunc, and masking all exceptions
		"mov %ax, 4(%rsp)\n"
		"fldcw 4(%rsp)\n"
#endif
		"push %rbx\n"
		"push %rbp\n"
		"push %r12\n"
		"push %r13\n"
		"push %r14\n"
		"push %r15\n"

#ifdef AMD64ABI
    		"movll %rsi, %r12\n" // second parameter is ram-blocks pointer
		"call %rdi\n"
#else
		"push %rdi\n"
		"push %rsi\n"
    		"movll %rdx, %r12\n" // second parameter is ram-blocks pointer
		"call %rcx\n"
		"pop %rsi\n"
		"pop %rdi\n"
#endif

		"fclex\n"

		"pop %r15\n"
		"pop %r14\n"
		"pop %r13\n"
		"pop %r12\n"
		"pop %rbp\n"
		"pop %rbx\n"

#ifndef EEL_X64_NO_CHANGE_FPFLAGS
		"fldcw (%rsp)\n"
		"addl $16, %rsp\n"
#endif

		"ret\n"
	);
}

void eel_callcode64_fast() 
{
	__asm__(
		"push %rbx\n"
		"push %rbp\n"
		"push %r12\n"
		"push %r13\n"
		"push %r14\n"
		"push %r15\n"

#ifdef AMD64ABI
    		"movll %rsi, %r12\n" // second parameter is ram-blocks pointer
		"call %rdi\n"
#else
		"push %rdi\n"
		"push %rsi\n"
    		"movll %rdx, %r12\n" // second parameter is ram-blocks pointer
		"call %rcx\n"
		"pop %rsi\n"
		"pop %rdi\n"
#endif

		"pop %r15\n"
		"pop %r14\n"
		"pop %r13\n"
		"pop %r12\n"
		"pop %rbp\n"
		"pop %rbx\n"

		"ret\n"
	);
}

void eel_setfp_round()
{
	__asm__(
#ifndef EEL_X64_NO_CHANGE_FPFLAGS
		"subl $16, %rsp\n"
		"fnstcw (%rsp)\n"
		"mov (%rsp), %ax\n"
		"and $0xF3FF, %ax\n" // set round to nearest
		"mov %ax, 4(%rsp)\n"
		"fldcw 4(%rsp)\n"
		"addl $16, %rsp\n"
#endif
		"ret\n"
	);
}

void eel_setfp_trunc()
{
	__asm__(
#ifndef EEL_X64_NO_CHANGE_FPFLAGS
		"subl $16, %rsp\n"
		"fnstcw (%rsp)\n"
		"mov (%rsp), %ax\n"
		"or $0xC00, %ax\n" // set to truncate
		"mov %ax, 4(%rsp)\n"
		"fldcw 4(%rsp)\n"
		"addl $16, %rsp\n"
#endif
		"ret\n"
	);
}

void eel_enterfp(int s[2]) 
{
	__asm__(
#ifdef AMD64ABI
		"fnstcw (%rdi)\n"
		"mov (%rdi), %ax\n"
		"or $0xE3F, %ax\n" // 53 or 64 bit precision, trunc, and masking all exceptions
		"mov %ax, 4(%rdi)\n"
		"fldcw 4(%rdi)\n"
#else
		"fnstcw (%rcx)\n"
		"mov (%rcx), %ax\n"
		"or $0xE3F, %ax\n" // 53 or 64 bit precision, trunc, and masking all exceptions
		"mov %ax, 4(%rcx)\n"
		"fldcw 4(%rcx)\n"
#endif
            "ret\n"
        );
}
void eel_leavefp(int s[2]) 
{
	__asm__(
#ifdef AMD64ABI
		"fldcw (%rdi)\n"
#else
		"fldcw (%rcx)\n"
#endif
                "ret\n";
        );
}

#endif

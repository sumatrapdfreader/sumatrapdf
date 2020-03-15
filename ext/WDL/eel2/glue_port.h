#ifndef _EEL_GLUE_PORTABLE_H_
#define _EEL_GLUE_PORTABLE_H_


#define DECL_ASMFUNC(x) 
#define GLUE_JMP_TYPE int
#define GLUE_JMP_SET_OFFSET(endOfInstruction,offset) (((GLUE_JMP_TYPE *)(endOfInstruction))[-1] = (offset))

#define GLUE_HAS_FXCH 
#define GLUE_MAX_FPSTACK_SIZE 64
#define BIF_FPSTACKUSE(x) (0) // fp stack is not used within functions
#define BIF_GETFPSTACKUSE(x) (1)

enum {
  EEL_BC_NOP=1,
  EEL_BC_RET,
  EEL_BC_JMP_NC, // followed by GLUE_JMP_TYPE
  EEL_BC_JMP_IF_P1_Z,
  EEL_BC_JMP_IF_P1_NZ,

  EEL_BC_MOV_FPTOP_DV,
  EEL_BC_MOV_P1_DV, // followed by INT_PTR ptr
  EEL_BC_MOV_P2_DV,
  EEL_BC_MOV_P3_DV,
  EEL_BC__RESET_WTP,

  EEL_BC_PUSH_P1,
  EEL_BC_PUSH_P1PTR_AS_VALUE,
  EEL_BC_POP_P1,
  EEL_BC_POP_P2,
  EEL_BC_POP_P3,
  EEL_BC_POP_VALUE_TO_ADDR,

  EEL_BC_MOVE_STACK,
  EEL_BC_STORE_P1_TO_STACK_AT_OFFS,
  EEL_BC_MOVE_STACKPTR_TO_P1,
  EEL_BC_MOVE_STACKPTR_TO_P2,
  EEL_BC_MOVE_STACKPTR_TO_P3,

  EEL_BC_SET_P2_FROM_P1,
  EEL_BC_SET_P3_FROM_P1,
  EEL_BC_COPY_VALUE_AT_P1_TO_ADDR,
  EEL_BC_SET_P1_FROM_WTP,
  EEL_BC_SET_P2_FROM_WTP,
  EEL_BC_SET_P3_FROM_WTP,

  EEL_BC_POP_FPSTACK_TO_PTR,
  EEL_BC_POP_FPSTACK_TOSTACK,

  EEL_BC_PUSH_VAL_AT_P1_TO_FPSTACK, 
  EEL_BC_PUSH_VAL_AT_P2_TO_FPSTACK, 
  EEL_BC_PUSH_VAL_AT_P3_TO_FPSTACK, 
  EEL_BC_POP_FPSTACK_TO_WTP,
  EEL_BC_SET_P1_Z,
  EEL_BC_SET_P1_NZ,


  EEL_BC_LOOP_LOADCNT,
  EEL_BC_LOOP_END,

#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN > 0
  EEL_BC_WHILE_SETUP,
#endif

  EEL_BC_WHILE_BEGIN,
  EEL_BC_WHILE_END,
  EEL_BC_WHILE_CHECK_RV,



  EEL_BC_BNOT,
  EEL_BC_BNOTNOT,
  EEL_BC_EQUAL,
  EEL_BC_EQUAL_EXACT,
  EEL_BC_NOTEQUAL,
  EEL_BC_NOTEQUAL_EXACT,
  EEL_BC_ABOVE,
  EEL_BC_BELOWEQ,


  EEL_BC_ADD,
  EEL_BC_SUB,
  EEL_BC_MUL,
  EEL_BC_DIV,
  EEL_BC_AND,
  EEL_BC_OR,
  EEL_BC_OR0,
  EEL_BC_XOR,

  EEL_BC_ADD_OP,
  EEL_BC_SUB_OP,
  EEL_BC_ADD_OP_FAST,
  EEL_BC_SUB_OP_FAST,
  EEL_BC_MUL_OP,
  EEL_BC_DIV_OP,
  EEL_BC_MUL_OP_FAST,
  EEL_BC_DIV_OP_FAST,
  EEL_BC_AND_OP,
  EEL_BC_OR_OP,
  EEL_BC_XOR_OP,

  EEL_BC_UMINUS,

  EEL_BC_ASSIGN,
  EEL_BC_ASSIGN_FAST,
  EEL_BC_ASSIGN_FAST_FROMFP,
  EEL_BC_ASSIGN_FROMFP,
  EEL_BC_MOD,
  EEL_BC_MOD_OP,
  EEL_BC_SHR,
  EEL_BC_SHL,

  EEL_BC_SQR,
  EEL_BC_MIN,
  EEL_BC_MAX,
  EEL_BC_MIN_FP,
  EEL_BC_MAX_FP,
  EEL_BC_ABS,
  EEL_BC_SIGN,
  EEL_BC_INVSQRT,

  EEL_BC_FXCH,
  EEL_BC_POP_FPSTACK,

  EEL_BC_FCALL,
  EEL_BC_BOOLTOFP,
  EEL_BC_FPTOBOOL,
  EEL_BC_FPTOBOOL_REV,

  EEL_BC_CFUNC_1PDD,
  EEL_BC_CFUNC_2PDD,
  EEL_BC_CFUNC_2PDDS,

  EEL_BC_MEGABUF,
  EEL_BC_GMEGABUF,

  EEL_BC_GENERIC1PARM,
  EEL_BC_GENERIC2PARM,
  EEL_BC_GENERIC3PARM,
  EEL_BC_GENERIC1PARM_RETD,
  EEL_BC_GENERIC2PARM_RETD,
  EEL_BC_GENERIC3PARM_RETD,

  EEL_BC_USERSTACK_PUSH,
  EEL_BC_USERSTACK_POP,
  EEL_BC_USERSTACK_POPFAST,
  EEL_BC_USERSTACK_PEEK,
  EEL_BC_USERSTACK_PEEK_INT,
  EEL_BC_USERSTACK_PEEK_TOP,
  EEL_BC_USERSTACK_EXCH,

  EEL_BC_DBG_GETSTACKPTR,

};

#define BC_DECL(x) static const EEL_BC_TYPE GLUE_##x[] = { EEL_BC_##x };
#define BC_DECL_JMP(x) static const EEL_BC_TYPE GLUE_##x[1 + sizeof(GLUE_JMP_TYPE) / sizeof(EEL_BC_TYPE)] = { EEL_BC_##x };
BC_DECL_JMP(JMP_NC)
BC_DECL_JMP(JMP_IF_P1_Z)
BC_DECL_JMP(JMP_IF_P1_NZ)
BC_DECL(RET)
BC_DECL(FXCH)
BC_DECL(POP_FPSTACK)
#define GLUE_POP_FPSTACK_SIZE sizeof(EEL_BC_TYPE)
BC_DECL(PUSH_P1)
BC_DECL(PUSH_P1PTR_AS_VALUE)
BC_DECL(POP_FPSTACK_TOSTACK)
BC_DECL(POP_FPSTACK_TO_WTP)
BC_DECL(SET_P1_Z)
BC_DECL(SET_P1_NZ)
BC_DECL_JMP(LOOP_LOADCNT)

BC_DECL_JMP(LOOP_END)

#define GLUE_LOOP_BEGIN_SIZE 0
#define GLUE_LOOP_BEGIN ((void*)"")
#define GLUE_LOOP_CLAMPCNT_SIZE 0
#define GLUE_LOOP_CLAMPCNT ((void*)"")

#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN > 0
  BC_DECL(WHILE_SETUP)
  #define GLUE_WHILE_SETUP_SIZE sizeof(GLUE_WHILE_SETUP)
  BC_DECL_JMP(WHILE_END)
#else
  #define GLUE_WHILE_SETUP_SIZE 0
  #define GLUE_WHILE_SETUP ((void *)"")
  #define GLUE_WHILE_END_NOJUMP
  BC_DECL(WHILE_END)
#endif

BC_DECL(WHILE_BEGIN);
BC_DECL_JMP(WHILE_CHECK_RV)  

#define GLUE_MOV_PX_DIRECTVALUE_SIZE (sizeof(EEL_BC_TYPE) + sizeof(INT_PTR))
#define GLUE_MOV_PX_DIRECTVALUE_TOSTACK_SIZE GLUE_MOV_PX_DIRECTVALUE_SIZE 
static void GLUE_MOV_PX_DIRECTVALUE_GEN(void *b, INT_PTR v, int wv) 
{   
  static const EEL_BC_TYPE tab[] = {
    EEL_BC_MOV_FPTOP_DV,
    EEL_BC_MOV_P1_DV,
    EEL_BC_MOV_P2_DV,
    EEL_BC_MOV_P3_DV,
  };
  *(EEL_BC_TYPE *)b = tab[wv+1];
  *(INT_PTR *) ((char *)b + sizeof(EEL_BC_TYPE)) = v;
}

#define GLUE_FUNC_ENTER_SIZE 0
#define GLUE_FUNC_LEAVE_SIZE 0
static const EEL_BC_TYPE GLUE_FUNC_ENTER[1]={-1};
static const EEL_BC_TYPE GLUE_FUNC_LEAVE[1]={-1};

static int GLUE_RESET_WTP(unsigned char *out, void *ptr)
{
  BC_DECL(_RESET_WTP)
  if (out) memcpy(out,&GLUE__RESET_WTP,sizeof(GLUE__RESET_WTP));
  if (out) *(void **) (out+sizeof(GLUE__RESET_WTP)) = ptr;
  return sizeof(GLUE__RESET_WTP) + sizeof(void *);
}

#define GLUE_POP_PX_SIZE sizeof(EEL_BC_TYPE)
static void GLUE_POP_PX(void *b, int wv)
{
  static const EEL_BC_TYPE tab[3] ={
    EEL_BC_POP_P1,
    EEL_BC_POP_P2,
    EEL_BC_POP_P3,
  };
  *(EEL_BC_TYPE *)b = tab[wv];
}

#define GLUE_SET_PX_FROM_P1_SIZE sizeof(EEL_BC_TYPE)
static void GLUE_SET_PX_FROM_P1(void *b, int wv)
{
  static const unsigned int tab[3]={
    EEL_BC_NOP,
    EEL_BC_SET_P2_FROM_P1,
    EEL_BC_SET_P3_FROM_P1,
  };
  *(EEL_BC_TYPE *)b = tab[wv];
}

#define GLUE_MOVE_STACK_SIZE (sizeof(EEL_BC_TYPE) + sizeof(int))
static void GLUE_MOVE_STACK(void *b, int amt)
{
  *(EEL_BC_TYPE *)b = EEL_BC_MOVE_STACK;
  *(int *)(((EEL_BC_TYPE *)b)+1) = amt;
}
#define GLUE_STORE_P1_TO_STACK_AT_OFFS_SIZE(x) (sizeof(EEL_BC_TYPE) + sizeof(int))
static void GLUE_STORE_P1_TO_STACK_AT_OFFS(void *b, int offs)
{
  *(EEL_BC_TYPE *)b = EEL_BC_STORE_P1_TO_STACK_AT_OFFS;
  *(int *)(((EEL_BC_TYPE *)b)+1) = offs;
}

#define GLUE_MOVE_PX_STACKPTR_SIZE sizeof(EEL_BC_TYPE)
static void GLUE_MOVE_PX_STACKPTR_GEN(void *b, int wv)
{
  static const EEL_BC_TYPE tab[3] = {
    EEL_BC_MOVE_STACKPTR_TO_P1,
    EEL_BC_MOVE_STACKPTR_TO_P2,
    EEL_BC_MOVE_STACKPTR_TO_P3
  };    
  *(EEL_BC_TYPE *)b = tab[wv];
}


static int GLUE_POP_VALUE_TO_ADDR(unsigned char *buf, void *destptr)
{    
  if (buf)
  {
    *(EEL_BC_TYPE *)buf = EEL_BC_POP_VALUE_TO_ADDR;
    *(void **) (buf+sizeof(EEL_BC_TYPE)) = destptr;
  }
  return sizeof(EEL_BC_TYPE) + sizeof(void *);
}

static int GLUE_COPY_VALUE_AT_P1_TO_PTR(unsigned char *buf, void *destptr)
{    
  if (buf)
  {
    *(EEL_BC_TYPE *)buf = EEL_BC_COPY_VALUE_AT_P1_TO_ADDR;
    *(void **) (buf+sizeof(EEL_BC_TYPE)) = destptr;
  }
  return sizeof(EEL_BC_TYPE) + sizeof(void *);
}




static unsigned char *EEL_GLUE_set_immediate(void *_p, INT_PTR newv)
{
  int mv=5;
  char *p=(char*)_p;
  p+=sizeof(EEL_BC_TYPE);
  while (*(INT_PTR*)p && mv-- > 0) p++;
  if (!mv) return (unsigned char *)p;

  *(INT_PTR *)p = newv;
  return (unsigned char *) p + sizeof(INT_PTR) - sizeof(EEL_BC_TYPE);
}

#define GLUE_SET_PX_FROM_WTP_SIZE sizeof(EEL_BC_TYPE)
static void GLUE_SET_PX_FROM_WTP(void *b, int wv)
{
  static const EEL_BC_TYPE tab[3]={
    EEL_BC_SET_P1_FROM_WTP,
    EEL_BC_SET_P2_FROM_WTP,
    EEL_BC_SET_P3_FROM_WTP,
  };
  *(EEL_BC_TYPE *)b = tab[wv];
}

static int GLUE_POP_FPSTACK_TO_PTR(unsigned char *buf, void *destptr)
{
  if (buf)
  {
    *(EEL_BC_TYPE *)buf = EEL_BC_POP_FPSTACK_TO_PTR;
    *(void **) (buf+sizeof(EEL_BC_TYPE)) = destptr;
  }
  return sizeof(EEL_BC_TYPE) + sizeof(void *);
}

  #define GLUE_PUSH_VAL_AT_PX_TO_FPSTACK_SIZE sizeof(EEL_BC_TYPE)
  static void GLUE_PUSH_VAL_AT_PX_TO_FPSTACK(void *b, int wv)
  {
    static const EEL_BC_TYPE tab[3] = {
      EEL_BC_PUSH_VAL_AT_P1_TO_FPSTACK, 
      EEL_BC_PUSH_VAL_AT_P2_TO_FPSTACK, 
      EEL_BC_PUSH_VAL_AT_P3_TO_FPSTACK, 
    };
    *(EEL_BC_TYPE *)b = tab[wv];
  }

#define GLUE_POP_FPSTACK_TO_WTP_TO_PX_SIZE (sizeof(GLUE_POP_FPSTACK_TO_WTP) + GLUE_SET_PX_FROM_WTP_SIZE)
static void GLUE_POP_FPSTACK_TO_WTP_TO_PX(unsigned char *buf, int wv)
{
  GLUE_SET_PX_FROM_WTP(buf,wv);
  memcpy(buf + GLUE_SET_PX_FROM_WTP_SIZE,GLUE_POP_FPSTACK_TO_WTP,sizeof(GLUE_POP_FPSTACK_TO_WTP));
};

static unsigned char GLUE_POP_STACK_TO_FPSTACK[1] = { 0 }; // todo

#define GLUE_INLINE_LOOPS

// end of bytecode glue, now for stubbage

#define EEL_BC_ENDOF(x) (((char*)(x))+sizeof(x))
#define BC_DECLASM(x,y) static EEL_BC_TYPE nseel_asm_##x[1]={EEL_BC_##y};

BC_DECLASM(band,NOP)
BC_DECLASM(bor,NOP)

BC_DECLASM(bnot,BNOT)
BC_DECLASM(bnotnot,BNOTNOT)
BC_DECLASM(equal,EQUAL)
BC_DECLASM(equal_exact,EQUAL_EXACT)
BC_DECLASM(notequal_exact,NOTEQUAL_EXACT)
BC_DECLASM(notequal,NOTEQUAL)
BC_DECLASM(above,ABOVE)
BC_DECLASM(beloweq,BELOWEQ)

BC_DECLASM(add,ADD)
BC_DECLASM(sub,SUB)
BC_DECLASM(mul,MUL)
BC_DECLASM(div,DIV)
BC_DECLASM(and,AND)
BC_DECLASM(or,OR)
BC_DECLASM(or0,OR0)
BC_DECLASM(xor,XOR)

BC_DECLASM(add_op,ADD_OP)
BC_DECLASM(sub_op,SUB_OP)
BC_DECLASM(add_op_fast,ADD_OP_FAST)
BC_DECLASM(sub_op_fast,SUB_OP_FAST)
BC_DECLASM(mul_op,MUL_OP)
BC_DECLASM(div_op,DIV_OP)
BC_DECLASM(mul_op_fast,MUL_OP_FAST)
BC_DECLASM(div_op_fast,DIV_OP_FAST)
BC_DECLASM(and_op,AND_OP)
BC_DECLASM(or_op,OR_OP)
BC_DECLASM(xor_op,XOR_OP)

BC_DECLASM(uminus,UMINUS)

BC_DECLASM(assign,ASSIGN)
BC_DECLASM(assign_fast,ASSIGN_FAST)
BC_DECLASM(assign_fast_fromfp,ASSIGN_FAST_FROMFP)
BC_DECLASM(assign_fromfp,ASSIGN_FROMFP)
BC_DECLASM(mod,MOD)
BC_DECLASM(mod_op,MOD_OP)
BC_DECLASM(shr,SHR)
BC_DECLASM(shl,SHL)
BC_DECLASM(sqr,SQR)

BC_DECLASM(min,MIN)
BC_DECLASM(max,MAX)
BC_DECLASM(min_fp,MIN_FP)
BC_DECLASM(max_fp,MAX_FP)
BC_DECLASM(abs,ABS)
BC_DECLASM(sign,SIGN)
BC_DECLASM(invsqrt,INVSQRT)
BC_DECLASM(dbg_getstackptr,DBG_GETSTACKPTR)

BC_DECLASM(booltofp,BOOLTOFP)
BC_DECLASM(fptobool,FPTOBOOL)
BC_DECLASM(fptobool_rev,FPTOBOOL_REV)

#define BC_DECLASM_N(x,y,n) static EEL_BC_TYPE nseel_asm_##x[1 + (n*sizeof(INT_PTR))/sizeof(EEL_BC_TYPE)]={EEL_BC_##y, };
#define BC_DECLASM_N2(x,y,n) static EEL_BC_TYPE _asm_##x[1 + (n*sizeof(INT_PTR))/sizeof(EEL_BC_TYPE)]={EEL_BC_##y, };

#define BC_DECLASM_N_EXPORT(x,y,n) EEL_BC_TYPE _asm_##x[1 + (n*sizeof(INT_PTR))/sizeof(EEL_BC_TYPE)]={EEL_BC_##y, }; const void *const _asm_##x##_end = EEL_BC_ENDOF(_asm_##x);

BC_DECLASM_N(stack_push,USERSTACK_PUSH,3)
BC_DECLASM_N(stack_pop,USERSTACK_POP,3)
BC_DECLASM_N(stack_pop_fast,USERSTACK_POPFAST,3)
BC_DECLASM_N(stack_peek,USERSTACK_PEEK,3)

BC_DECLASM_N(stack_peek_int,USERSTACK_PEEK_INT,4)

BC_DECLASM_N(stack_peek_top,USERSTACK_PEEK_TOP,1)
BC_DECLASM_N(stack_exch,USERSTACK_EXCH,1)

BC_DECLASM_N(fcall,FCALL,1)

BC_DECLASM_N(1pdd,CFUNC_1PDD,1)
BC_DECLASM_N(2pdd,CFUNC_2PDD,1)
BC_DECLASM_N(2pdds,CFUNC_2PDDS,1)

BC_DECLASM_N2(megabuf,MEGABUF,0)
BC_DECLASM_N2(gmegabuf,GMEGABUF,2)
#define _asm_megabuf_end EEL_BC_ENDOF(_asm_megabuf)
#define _asm_gmegabuf_end EEL_BC_ENDOF(_asm_gmegabuf)

BC_DECLASM_N_EXPORT(generic1parm,GENERIC1PARM,2)
BC_DECLASM_N_EXPORT(generic2parm,GENERIC2PARM,2)
BC_DECLASM_N_EXPORT(generic3parm,GENERIC3PARM,2)
BC_DECLASM_N_EXPORT(generic1parm_retd,GENERIC1PARM_RETD,2)
BC_DECLASM_N_EXPORT(generic2parm_retd,GENERIC2PARM_RETD,2)
BC_DECLASM_N_EXPORT(generic3parm_retd,GENERIC3PARM_RETD,2)


#define _asm_generic1parm_end EEL_BC_ENDOF(_asm_generic1parm)
#define _asm_generic2parm_end EEL_BC_ENDOF(_asm_generic2parm)
#define _asm_generic3parm_end EEL_BC_ENDOF(_asm_generic3parm)
#define _asm_generic1parm_retd_end EEL_BC_ENDOF(_asm_generic1parm_retd)
#define _asm_generic2parm_retd_end EEL_BC_ENDOF(_asm_generic2parm_retd)
#define _asm_generic3parm_retd_end EEL_BC_ENDOF(_asm_generic3parm_retd)

#define nseel_asm_1pdd_end EEL_BC_ENDOF(nseel_asm_1pdd)
#define nseel_asm_2pdd_end EEL_BC_ENDOF(nseel_asm_2pdd)
#define nseel_asm_2pdds_end EEL_BC_ENDOF(nseel_asm_2pdds)

#define nseel_asm_fcall_end EEL_BC_ENDOF(nseel_asm_fcall)

#define nseel_asm_band_end EEL_BC_ENDOF(nseel_asm_band)
#define nseel_asm_bor_end EEL_BC_ENDOF(nseel_asm_bor)
#define nseel_asm_bnot_end EEL_BC_ENDOF(nseel_asm_bnot)
#define nseel_asm_bnotnot_end EEL_BC_ENDOF(nseel_asm_bnotnot)
#define nseel_asm_equal_end EEL_BC_ENDOF(nseel_asm_equal)
#define nseel_asm_equal_exact_end EEL_BC_ENDOF(nseel_asm_equal_exact)
#define nseel_asm_notequal_end EEL_BC_ENDOF(nseel_asm_notequal)
#define nseel_asm_notequal_exact_end EEL_BC_ENDOF(nseel_asm_notequal_exact)
#define nseel_asm_above_end EEL_BC_ENDOF(nseel_asm_above)
#define nseel_asm_beloweq_end EEL_BC_ENDOF(nseel_asm_beloweq)

#define nseel_asm_min_end EEL_BC_ENDOF(nseel_asm_min)
#define nseel_asm_max_end EEL_BC_ENDOF(nseel_asm_max)
#define nseel_asm_abs_end EEL_BC_ENDOF(nseel_asm_abs)
#define nseel_asm_min_fp_end EEL_BC_ENDOF(nseel_asm_min_fp)
#define nseel_asm_max_fp_end EEL_BC_ENDOF(nseel_asm_max_fp)
#define nseel_asm_sign_end EEL_BC_ENDOF(nseel_asm_sign)
#define nseel_asm_invsqrt_end EEL_BC_ENDOF(nseel_asm_invsqrt)
#define nseel_asm_dbg_getstackptr_end EEL_BC_ENDOF(nseel_asm_dbg_getstackptr)


#define nseel_asm_add_end EEL_BC_ENDOF(nseel_asm_add)
#define nseel_asm_sub_end EEL_BC_ENDOF(nseel_asm_sub)
#define nseel_asm_mul_end EEL_BC_ENDOF(nseel_asm_mul)
#define nseel_asm_div_end EEL_BC_ENDOF(nseel_asm_div)
#define nseel_asm_and_end EEL_BC_ENDOF(nseel_asm_and)
#define nseel_asm_or_end EEL_BC_ENDOF(nseel_asm_or)
#define nseel_asm_or0_end EEL_BC_ENDOF(nseel_asm_or0)
#define nseel_asm_xor_end EEL_BC_ENDOF(nseel_asm_xor)

#define nseel_asm_add_op_end EEL_BC_ENDOF(nseel_asm_add_op)
#define nseel_asm_sub_op_end EEL_BC_ENDOF(nseel_asm_sub_op)
#define nseel_asm_add_op_fast_end EEL_BC_ENDOF(nseel_asm_add_op_fast)
#define nseel_asm_sub_op_fast_end EEL_BC_ENDOF(nseel_asm_sub_op_fast)
#define nseel_asm_mul_op_end EEL_BC_ENDOF(nseel_asm_mul_op)
#define nseel_asm_mul_op_fast_end EEL_BC_ENDOF(nseel_asm_mul_op_fast)
#define nseel_asm_div_op_end EEL_BC_ENDOF(nseel_asm_div_op)
#define nseel_asm_div_op_fast_end EEL_BC_ENDOF(nseel_asm_div_op_fast)
#define nseel_asm_and_op_end EEL_BC_ENDOF(nseel_asm_and_op)
#define nseel_asm_or_op_end EEL_BC_ENDOF(nseel_asm_or_op)
#define nseel_asm_xor_op_end EEL_BC_ENDOF(nseel_asm_xor_op)

#define nseel_asm_uminus_end EEL_BC_ENDOF(nseel_asm_uminus)
#define nseel_asm_assign_end EEL_BC_ENDOF(nseel_asm_assign)
#define nseel_asm_assign_fast_end EEL_BC_ENDOF(nseel_asm_assign_fast)
#define nseel_asm_assign_fast_fromfp_end EEL_BC_ENDOF(nseel_asm_assign_fast_fromfp)
#define nseel_asm_assign_fromfp_end EEL_BC_ENDOF(nseel_asm_assign_fromfp)
#define nseel_asm_mod_end EEL_BC_ENDOF(nseel_asm_mod)
#define nseel_asm_mod_op_end EEL_BC_ENDOF(nseel_asm_mod_op)
#define nseel_asm_shr_end EEL_BC_ENDOF(nseel_asm_shr)
#define nseel_asm_shl_end EEL_BC_ENDOF(nseel_asm_shl)

#define nseel_asm_sqr_end EEL_BC_ENDOF(nseel_asm_sqr)


#define nseel_asm_booltofp_end EEL_BC_ENDOF(nseel_asm_booltofp)
#define nseel_asm_fptobool_end EEL_BC_ENDOF(nseel_asm_fptobool)
#define nseel_asm_fptobool_rev_end EEL_BC_ENDOF(nseel_asm_fptobool_rev)

#define nseel_asm_stack_push_end EEL_BC_ENDOF(nseel_asm_stack_push)
#define nseel_asm_stack_pop_end EEL_BC_ENDOF(nseel_asm_stack_pop)
#define nseel_asm_stack_pop_fast_end EEL_BC_ENDOF(nseel_asm_stack_pop_fast)
#define nseel_asm_stack_peek_end EEL_BC_ENDOF(nseel_asm_stack_peek)
#define nseel_asm_stack_peek_int_end EEL_BC_ENDOF(nseel_asm_stack_peek_int)
#define nseel_asm_stack_peek_top_end EEL_BC_ENDOF(nseel_asm_stack_peek_top)
#define nseel_asm_stack_exch_end EEL_BC_ENDOF(nseel_asm_stack_exch)


static void *GLUE_realAddress(void *fn, void *fn_e, int *size)
{
  *size = (char *)fn_e - (char *)fn;
  return fn;
}

#define EEL_BC_STACKSIZE (65536)

// todo: check for stack overflows! we could determine if this is possible at compile time.
#define EEL_BC_STACK_POP_SIZE 8
#define EEL_BC_STACK_PUSH(type, val) (*(type *)(stackptr -= EEL_BC_STACK_POP_SIZE)) = (val)
#define EEL_BC_STACK_POP() (stackptr += EEL_BC_STACK_POP_SIZE)

#define EEL_BC_TRUE ((EEL_F*)(INT_PTR)1)




static void GLUE_CALL_CODE(INT_PTR bp, INT_PTR cp, INT_PTR rt) 
{
  char __stack[EEL_BC_STACKSIZE];
  char *iptr = (char*)cp;
  char *stackptr=__stack + EEL_BC_STACKSIZE;
  EEL_F *p1 = NULL, *p2 = NULL, *p3 = NULL, *wtp = (EEL_F*)bp;
#define fp_top (_fpstacktop[0])
#define fp_top2 (_fpstacktop[-1])
#define fp_push(x) *++_fpstacktop=(x)
#define fp_pop() (*_fpstacktop--)
#define fp_rewind(x) (_fpstacktop -= (x))

  EEL_F fpstack[GLUE_MAX_FPSTACK_SIZE];
  EEL_F *_fpstacktop=fpstack-1;
  for (;;)
  {
    EEL_BC_TYPE inst = *(EEL_BC_TYPE *)iptr;
    iptr += sizeof(EEL_BC_TYPE);
    switch (inst)
    {
      case EEL_BC_FXCH:
        {
          EEL_F a = fp_top;
          fp_top=fp_top2;
          fp_top2=a;
        }
      break;
      case EEL_BC_POP_FPSTACK: fp_rewind(1); break;
      case EEL_BC_NOP: break;
      case EEL_BC_RET: 
        if (EEL_BC_STACK_POP() > __stack+EEL_BC_STACKSIZE) 
        {
          return;
        }
        iptr = *(void **)(stackptr - EEL_BC_STACK_POP_SIZE);
      break;
      case EEL_BC_JMP_NC: 
        iptr += sizeof(GLUE_JMP_TYPE)+*(GLUE_JMP_TYPE *)iptr;
      break;
      case EEL_BC_JMP_IF_P1_Z:
        iptr += p1 ? sizeof(GLUE_JMP_TYPE) : sizeof(GLUE_JMP_TYPE)+*(GLUE_JMP_TYPE *)iptr;
      break;
      case EEL_BC_JMP_IF_P1_NZ:
        iptr += p1 ? sizeof(GLUE_JMP_TYPE)+*(GLUE_JMP_TYPE *)iptr : sizeof(GLUE_JMP_TYPE);
      break;
      case EEL_BC_MOV_FPTOP_DV:
        fp_push(**(EEL_F **)iptr);
        iptr += sizeof(void*);
      break;
      case EEL_BC_MOV_P1_DV:
        p1 = *(void **)iptr;
        iptr += sizeof(void*);
      break;
      case EEL_BC_MOV_P2_DV:
        p2 = *(void **)iptr;
        iptr += sizeof(void*);
      break;
      case EEL_BC_MOV_P3_DV:
        p3 = *(void **)iptr;
        iptr += sizeof(void*);
      break;
      case EEL_BC__RESET_WTP:
        wtp = *(void **)iptr;
        iptr += sizeof(void*);
      break;    
      case EEL_BC_PUSH_P1:
        EEL_BC_STACK_PUSH(void *, p1);
      break;
      case EEL_BC_PUSH_P1PTR_AS_VALUE:
        EEL_BC_STACK_PUSH(EEL_F, *p1);
      break;
      case EEL_BC_POP_P1:
        p1 = *(EEL_F **) stackptr;
        EEL_BC_STACK_POP();
      break;
      case EEL_BC_POP_P2:
        p2 = *(EEL_F **) stackptr;
        EEL_BC_STACK_POP();
      break;
      case EEL_BC_POP_P3:
        p3 = *(EEL_F **) stackptr;
        EEL_BC_STACK_POP();
      break;
      case EEL_BC_POP_VALUE_TO_ADDR:
        **(EEL_F**)iptr = *(EEL_F *)stackptr;
        EEL_BC_STACK_POP();
        iptr += sizeof(void*);
      break;
      case EEL_BC_MOVE_STACK:
        stackptr += *(int *)iptr;
        iptr += sizeof(int);
      break;
      case EEL_BC_STORE_P1_TO_STACK_AT_OFFS:
        *(void **) (stackptr + *(int *)iptr) = p1;
        iptr += sizeof(int);
      break;
      case EEL_BC_MOVE_STACKPTR_TO_P1:
        p1 = (double *)stackptr;
      break;
      case EEL_BC_MOVE_STACKPTR_TO_P2:
        p2 = (double *)stackptr;
      break;
      case EEL_BC_MOVE_STACKPTR_TO_P3:
        p3 = (double *)stackptr;
      break;
      case EEL_BC_SET_P2_FROM_P1:
        p2=p1;
      break;
      case EEL_BC_SET_P3_FROM_P1:
        p3=p1;
      break;
      case EEL_BC_COPY_VALUE_AT_P1_TO_ADDR:
        **(EEL_F **)iptr = *p1;
        iptr += sizeof(void*);
      break;
      case EEL_BC_SET_P1_FROM_WTP:
        p1 = wtp;
      break;
      case EEL_BC_SET_P2_FROM_WTP:
        p2 = wtp;
      break;
      case EEL_BC_SET_P3_FROM_WTP:
        p3 = wtp;
      break;
      case EEL_BC_POP_FPSTACK_TO_PTR:
        **((EEL_F **)iptr) = fp_pop();
        iptr += sizeof(void *);
      break;
      case EEL_BC_POP_FPSTACK_TOSTACK:
        EEL_BC_STACK_PUSH(EEL_F, fp_pop());
      break;
      case EEL_BC_PUSH_VAL_AT_P1_TO_FPSTACK: 
        fp_push(*p1);
      break;
      case EEL_BC_PUSH_VAL_AT_P2_TO_FPSTACK: 
        fp_push(*p2);
      break;
      case EEL_BC_PUSH_VAL_AT_P3_TO_FPSTACK: 
        fp_push(*p3);
      break;
      case EEL_BC_POP_FPSTACK_TO_WTP:
        *wtp++ = fp_pop();
      break;
      case EEL_BC_SET_P1_Z:
        p1=NULL;
      break;
      case EEL_BC_SET_P1_NZ:
        p1 = EEL_BC_TRUE;
      break;

      case EEL_BC_LOOP_LOADCNT:
        if ((EEL_BC_STACK_PUSH(int, (int)fp_pop())) < 1)
        {
          EEL_BC_STACK_POP();
          iptr+= sizeof(GLUE_JMP_TYPE)+*(GLUE_JMP_TYPE *)iptr;
        }
        else
        {
          iptr += sizeof(GLUE_JMP_TYPE);
#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN > 0
          if ((*(int *)stackptr) > NSEEL_LOOPFUNC_SUPPORT_MAXLEN) (*(int *)stackptr) = NSEEL_LOOPFUNC_SUPPORT_MAXLEN;
#endif
          EEL_BC_STACK_PUSH(void *, wtp);
        }
      break;
      case EEL_BC_LOOP_END:
        wtp = *(void **) (stackptr);
        if (--(*(int *)(stackptr+EEL_BC_STACK_POP_SIZE)) <= 0)
        {
          stackptr += EEL_BC_STACK_POP_SIZE*2;
          iptr += sizeof(GLUE_JMP_TYPE);
        }
        else
        {
          iptr += sizeof(GLUE_JMP_TYPE)+*(GLUE_JMP_TYPE *)iptr; // back to the start!
        }
      break;

#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN > 0
      case EEL_BC_WHILE_SETUP:
        EEL_BC_STACK_PUSH(int,NSEEL_LOOPFUNC_SUPPORT_MAXLEN);
      break;
#endif
      case EEL_BC_WHILE_BEGIN:
        EEL_BC_STACK_PUSH(void *, wtp);
      break;
      case EEL_BC_WHILE_END:
        wtp = *(EEL_F **) stackptr;
        EEL_BC_STACK_POP();

#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN > 0
        if (--(*(int *)stackptr) <= 0)
        {
          EEL_BC_STACK_POP();
          iptr += sizeof(GLUE_JMP_TYPE)+*(GLUE_JMP_TYPE *)iptr; // endpt
        }
        else
        {
          iptr += sizeof(GLUE_JMP_TYPE);
        }
#endif
      break;
      case EEL_BC_WHILE_CHECK_RV:
        if (p1)
        {
          iptr += sizeof(GLUE_JMP_TYPE)+*(GLUE_JMP_TYPE *)iptr; // loop
        }
        else
        {
          // done
#if NSEEL_LOOPFUNC_SUPPORT_MAXLEN > 0
          EEL_BC_STACK_POP();
#endif
          iptr += sizeof(GLUE_JMP_TYPE);
        }
      break; 
      case EEL_BC_BNOT:
        p1 = p1 ? NULL : EEL_BC_TRUE;
      break;
      case EEL_BC_BNOTNOT:
        p1 = p1 ? EEL_BC_TRUE : NULL;
      break;
      case EEL_BC_EQUAL:
        p1 = fabs(fp_top - fp_top2) < NSEEL_CLOSEFACTOR ? EEL_BC_TRUE : NULL;
        fp_rewind(2);
      break;
      case EEL_BC_EQUAL_EXACT:
        p1 = fp_top == fp_top2 ? EEL_BC_TRUE : NULL;
        fp_rewind(2);
      break;
      case EEL_BC_NOTEQUAL:
        p1 = fabs(fp_top - fp_top2) >= NSEEL_CLOSEFACTOR ? EEL_BC_TRUE : NULL;
        fp_rewind(2);
      break;
      case EEL_BC_NOTEQUAL_EXACT:
        p1 = fp_top != fp_top2 ? EEL_BC_TRUE : NULL;
        fp_rewind(2);
      break;
      case EEL_BC_ABOVE:
        p1 = fp_top < fp_top2 ? EEL_BC_TRUE : NULL;
        fp_rewind(2);
      break;
      case EEL_BC_BELOWEQ:
        p1 = fp_top >= fp_top2 ? EEL_BC_TRUE : NULL;
        fp_rewind(2);
      break;

      case EEL_BC_ADD:
        fp_top2 += fp_top;
        fp_rewind(1);
      break;
      case EEL_BC_SUB:
        fp_top2 -= fp_top;
        fp_rewind(1);
      break;
      case EEL_BC_MUL:
        fp_top2 *= fp_top;
        fp_rewind(1);
      break;
      case EEL_BC_DIV:
        fp_top2 /= fp_top;
        fp_rewind(1);
      break;
      case EEL_BC_AND:
        fp_top2 = (EEL_F) (((int)fp_top) & (int)(fp_top2));
        fp_rewind(1);
      break;
      case EEL_BC_OR:
        fp_top2 = (EEL_F) (((int)fp_top) | (int)(fp_top2));
        fp_rewind(1);
      break;
      case EEL_BC_OR0:
        fp_top = (EEL_F) ((int)(fp_top));
      break;
      case EEL_BC_XOR:
        fp_top2 = (EEL_F) (((int)fp_top) ^ (int)(fp_top2));
        fp_rewind(1);
      break;

      case EEL_BC_ADD_OP:
        *(p1 = p2) = denormal_filter_double2(*p2 + fp_pop());
      break;
      case EEL_BC_SUB_OP:
        *(p1 = p2) = denormal_filter_double2(*p2 - fp_pop());
      break;
      case EEL_BC_ADD_OP_FAST:
        *(p1 = p2) += fp_pop();        
      break;
      case EEL_BC_SUB_OP_FAST:
        *(p1 = p2) -= fp_pop();
      break;
      case EEL_BC_MUL_OP:
        *(p1 = p2) = denormal_filter_double2(*p2 * fp_pop());
      break;
      case EEL_BC_DIV_OP:
        *(p1 = p2) = denormal_filter_double2(*p2 / fp_pop());
      break;
      case EEL_BC_MUL_OP_FAST:
        *(p1 = p2) *= fp_pop();
      break;
      case EEL_BC_DIV_OP_FAST:
        *(p1 = p2) /= fp_pop();
      break;
      case EEL_BC_AND_OP:
        p1 = p2;
        *p2 = (EEL_F) (((int)*p2) & (int)fp_pop());
      break;
      case EEL_BC_OR_OP:
        p1 = p2;
        *p2 = (EEL_F) (((int)*p2) | (int)fp_pop());
      break;
      case EEL_BC_XOR_OP:
        p1 = p2;
        *p2 = (EEL_F) (((int)*p2) ^ (int)fp_pop());
      break;
      case EEL_BC_UMINUS:
        fp_top = -fp_top;
      break;
      case EEL_BC_ASSIGN:
        *p2 = denormal_filter_double2(*p1);
        p1 = p2;
      break;
      case EEL_BC_ASSIGN_FAST:
        *p2 = *p1;
        p1 = p2;
      break;
      case EEL_BC_ASSIGN_FAST_FROMFP:
        *p2 = fp_pop();
        p1 = p2;
      break;
      case EEL_BC_ASSIGN_FROMFP:
        *p2 = denormal_filter_double2(fp_pop());
        p1 = p2;
      break;
      case EEL_BC_MOD:
        {
          int a = (int) (fp_pop());
          fp_top = a ? (EEL_F) ((int)fp_top % a) : 0.0;
        }
      break;
      case EEL_BC_MOD_OP:
        {
          int a = (int) (fp_pop());
          *p2 = a ? (EEL_F) ((int)*p2 % a) : 0.0;
          p1=p2;

        }
      break;
      case EEL_BC_SHR:
        fp_top2 = (EEL_F) (((int)fp_top2) >> (int)fp_top);
        fp_rewind(1);
      break;
      case EEL_BC_SHL:
        fp_top2 = (EEL_F) (((int)fp_top2) << (int)fp_top);
        fp_rewind(1);
      break;
      case EEL_BC_SQR:
        fp_top *= fp_top;
      break;
      case EEL_BC_MIN:
        if (*p1 > *p2) p1 = p2;
      break;
      case EEL_BC_MAX:
        if (*p1 < *p2) p1 = p2;
      break;
      case EEL_BC_MIN_FP:
        {
          EEL_F a=fp_pop();
          if (a<fp_top) fp_top=a;
        }
      break;
      case EEL_BC_MAX_FP:
        {
          EEL_F a=fp_pop();
          if (a>fp_top) fp_top=a;
        }
      break;
      case EEL_BC_ABS:
        fp_top = fabs(fp_top);
      break;
      case EEL_BC_SIGN:
        if (fp_top<0.0) fp_top=-1.0;
        else if (fp_top>0.0) fp_top=1.0;
      break;
      case EEL_BC_DBG_GETSTACKPTR:
        fp_top = (int)(stackptr - __stack);
      break;
      case EEL_BC_INVSQRT:
        {
          float y = (float)fp_top;
          int i  = 0x5f3759df - ( (* (int *) &y) >> 1 );
          y  = *(float *) &i;
          fp_top  = y * ( 1.5F - ( (fp_top * 0.5) * y * y ) );
        }
      break;
      case EEL_BC_FCALL:
        {
          char *newiptr = *(char **)iptr;
          EEL_BC_STACK_PUSH(void *, (iptr += sizeof(void *)));
          iptr = newiptr;
        }
      break;
      case EEL_BC_BOOLTOFP:
        fp_push(p1 ? 1.0 : 0.0);
      break;
      case EEL_BC_FPTOBOOL:
        p1 = fabs(fp_pop()) >= NSEEL_CLOSEFACTOR ? EEL_BC_TRUE : NULL;
      break;
      case EEL_BC_FPTOBOOL_REV:
        p1 = fabs(fp_pop()) < NSEEL_CLOSEFACTOR ? EEL_BC_TRUE : NULL;
      break;

      case EEL_BC_CFUNC_1PDD:
        {
          double (*f)(double) = *(double (**)(double)) iptr;
          fp_top = f(fp_top);
          iptr += sizeof(void *);
        }
      break;
      case EEL_BC_CFUNC_2PDD:
        {
          double (*f)(double,double) = *(double (**)(double,double))iptr;
          fp_top2 = f(fp_top2,fp_top);
          fp_rewind(1);
          iptr += sizeof(void *);
        }
      break;
      case EEL_BC_CFUNC_2PDDS:
        {
          double (*f)(double,double) = *(double (**)(double,double))iptr;
          *p2 = f(*p2,fp_pop());
          p1 = p2;
          iptr += sizeof(void *);
        }
      break;

      case EEL_BC_MEGABUF:
        {
          unsigned int idx=(unsigned int) (fp_pop() + NSEEL_CLOSEFACTOR);
          EEL_F **f = (EEL_F **)rt,*f2;
          p1 = (idx < NSEEL_RAM_BLOCKS*NSEEL_RAM_ITEMSPERBLOCK && (f2=f[idx/NSEEL_RAM_ITEMSPERBLOCK])) ? 
              (f2 + (idx&(NSEEL_RAM_ITEMSPERBLOCK-1))) : 
             __NSEEL_RAMAlloc((void*)rt,idx);
        }
      break;
      case EEL_BC_GMEGABUF:
        {
          p1 = __NSEEL_RAMAllocGMEM(*(EEL_F ****)iptr,(int) (fp_pop() + NSEEL_CLOSEFACTOR));
          iptr += sizeof(void *)*2; // also includes ptr to __NSEEL_RAMAllocGMEM, which we ignore
        }
      break;
      case EEL_BC_GENERIC1PARM:
        {
          EEL_F *(*f)(void *,EEL_F*) = *(EEL_F *(**)(void *, EEL_F *)) (iptr+sizeof(void *));
          p1 = f(*(void **)iptr,p1);
          iptr += sizeof(void *)*2;
        }
      break;
      case EEL_BC_GENERIC2PARM:
        {
          EEL_F *(*f)(void *,EEL_F*,EEL_F*) = *(EEL_F *(**)(void *, EEL_F *, EEL_F *)) (iptr+sizeof(void *));
          p1 = f(*(void **)iptr,p2, p1);
          iptr += sizeof(void *)*2;
        }
      break;
      case EEL_BC_GENERIC3PARM:
        {
          EEL_F *(*f)(void *,EEL_F*,EEL_F*,EEL_F*) = *(EEL_F *(**)(void *, EEL_F *, EEL_F *, EEL_F *)) (iptr+sizeof(void *));
          p1 = f(*(void **)iptr,p3, p2, p1);
          iptr += sizeof(void *)*2;
        }
      break;
      case EEL_BC_GENERIC1PARM_RETD:
        {
          EEL_F (*f)(void *,EEL_F*) = *(EEL_F (**)(void *, EEL_F *)) (iptr+sizeof(void *));
          fp_push(f(*(void **)iptr,p1));
          iptr += sizeof(void *)*2;
        }
      break;
      case EEL_BC_GENERIC2PARM_RETD:
        {
          EEL_F (*f)(void *,EEL_F*,EEL_F*) = *(EEL_F (**)(void *, EEL_F *, EEL_F *)) (iptr+sizeof(void *));
          fp_push(f(*(void **)iptr,p2, p1));
          iptr += sizeof(void *)*2;
        }
      break;
      case EEL_BC_GENERIC3PARM_RETD:
        {
          EEL_F (*f)(void *,EEL_F*,EEL_F*,EEL_F*) = *(EEL_F (**)(void *, EEL_F *, EEL_F *, EEL_F *)) (iptr+sizeof(void *));
          fp_push(f(*(void **)iptr,p3, p2, p1));
          iptr += sizeof(void *)*2;
        }
      break;

      case EEL_BC_USERSTACK_PUSH:
        {
          UINT_PTR *sptr = *(UINT_PTR **)iptr;
          (*sptr) += 8;
          (*sptr) &= *(UINT_PTR*)(iptr+sizeof(void *));
          (*sptr) |= *(UINT_PTR*)(iptr+2*sizeof(void *));
          *(EEL_F *)*sptr = *p1;
        }
        iptr += sizeof(void*)*3;
      break;
      case EEL_BC_USERSTACK_POP:
        {
          UINT_PTR *sptr = *(UINT_PTR **)iptr;
          *p1 = *(EEL_F *)*sptr;
          (*sptr) -= 8;
          (*sptr) &= *(UINT_PTR*)(iptr+sizeof(void *));
          (*sptr) |= *(UINT_PTR*)(iptr+2*sizeof(void *));
        }
        iptr += sizeof(void*)*3;
      break;
      case EEL_BC_USERSTACK_POPFAST:
        {
          UINT_PTR *sptr = *(UINT_PTR **)iptr;
          p1 = (EEL_F *)*sptr;
          (*sptr) -= 8;
          (*sptr) &= *(UINT_PTR*)(iptr+sizeof(void *));
          (*sptr) |= *(UINT_PTR*)(iptr+2*sizeof(void *));
        }
        iptr += sizeof(void*)*3;
      break;
      case EEL_BC_USERSTACK_PEEK:
        {
          UINT_PTR sptr = **(UINT_PTR **)iptr;
          sptr -= sizeof(EEL_F) * (int)(fp_pop());
          sptr &= *(UINT_PTR*)(iptr+sizeof(void *));
          sptr |= *(UINT_PTR*)(iptr+2*sizeof(void *));
          p1 = (EEL_F *)sptr;
        }
        iptr += sizeof(void*)*3;
      break;
      case EEL_BC_USERSTACK_PEEK_INT:
        {
          UINT_PTR sptr = **(UINT_PTR **)iptr;
          sptr -= *(UINT_PTR*)(iptr+sizeof(void*));
          sptr &= *(UINT_PTR*)(iptr+2*sizeof(void *));
          sptr |= *(UINT_PTR*)(iptr+3*sizeof(void *));
          p1 = (EEL_F *)sptr;
        }
        iptr += sizeof(void*)*4;
      break;
      case EEL_BC_USERSTACK_PEEK_TOP:
        p1 = **(EEL_F ***)iptr;
        iptr += sizeof(void*);
      break;
      case EEL_BC_USERSTACK_EXCH:
        {
          EEL_F *p=**(EEL_F ***)iptr;
          EEL_F a=*p;
          *p=*p1;
          *p1=a;
        }
        iptr += sizeof(void*);
      break;
    }
  }
#undef fp_top
#undef fp_top2
#undef fp_pop
#undef fp_push
};

#endif

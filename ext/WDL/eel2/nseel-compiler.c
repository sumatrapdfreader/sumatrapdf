/*
  Expression Evaluator Library (NS-EEL) v2
  Copyright (C) 2004-2013 Cockos Incorporated
  Copyright (C) 1999-2003 Nullsoft, Inc.
  
  nseel-compiler.c

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "ns-eel-int.h"

#include "../denormal.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <ctype.h>

#include "../wdlcstring.h"

#if !defined(EEL_TARGET_PORTABLE) && !defined(_WIN32)
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>
#endif

#define NSEEL_VARS_MALLOC_CHUNKSIZE 8

//#define LOG_OPT
//#define EEL_PRINT_FAILS
//#define EEL_VALIDATE_WORKTABLE_USE
//#define EEL_VALIDATE_FSTUBS


#ifdef EEL_PRINT_FAILS
  #ifdef _WIN32
    #define RET_MINUS1_FAIL(x) { OutputDebugString(x); return -1; }
  #else
    #define RET_MINUS1_FAIL(x) { printf("%s\n",x); return -1; }
  #endif
#else
#define RET_MINUS1_FAIL(x) return -1;
#endif

#ifdef EEL_DUMP_OPS
FILE *g_eel_dump_fp, *g_eel_dump_fp2;
#endif

#ifdef EEL_VALIDATE_WORKTABLE_USE
  #define MIN_COMPUTABLE_SIZE 0
  #define COMPUTABLE_EXTRA_SPACE 64 // safety buffer, if EEL_VALIDATE_WORKTABLE_USE set, used for magic-value-checking
#else
  #define MIN_COMPUTABLE_SIZE 32 // always use at least this big of a temp storage table (and reset the temp ptr when it goes past this boundary)
  #define COMPUTABLE_EXTRA_SPACE 16 // safety buffer, if EEL_VALIDATE_WORKTABLE_USE set, used for magic-value-checking
#endif


/*
  P1 is rightmost parameter
  P2 is second rightmost, if any
  P3 is third rightmost, if any
  registers on x86 are  (RAX etc on x86-64)
    P1(ret) EAX
    P2 EDI
    P3 ECX
    WTP RSI
    x86_64: r12 is a pointer to ram_state.blocks
    x86_64: r13 is a pointer to closenessfactor

  registers on PPC are:
    P1(ret) r3
    P2 r14 
    P3 r15
    WTP r16 (r17 has the original value)
    r13 is a pointer to ram_state.blocks

    ppc uses f31 and f30 and others for certain constants

  */


#ifdef EEL_TARGET_PORTABLE

#define EEL_DOESNT_NEED_EXEC_PERMS

#ifdef EEL_PORTABLE_TAILCALL
#include "glue_port_new.h"
#else
#include "glue_port.h"
#endif

#elif defined(__ppc__)

#include "glue_ppc.h"

#elif defined(__aarch64__)

#include "glue_aarch64.h"

#elif defined(__arm__) || (defined (_M_ARM) && _M_ARM  == 7)

#include "glue_arm.h"

#elif defined(_WIN64) || defined(__LP64__)

#include "glue_x86_64.h"

#else

#include "glue_x86.h"

#endif

#ifndef GLUE_INVSQRT_NEEDREPL
#define GLUE_INVSQRT_NEEDREPL 0
#endif


// used by //#eel-no-optimize:xxx, in ctx->optimizeDisableFlags
#define OPTFLAG_NO_OPTIMIZE 1
#define OPTFLAG_NO_FPSTACK 2
#define OPTFLAG_NO_INLINEFUNC 4
#define OPTFLAG_FULL_DENORMAL_CHECKS 8 // if set, denormals/NaN are always filtered on assign
#define OPTFLAG_NO_DENORMAL_CHECKS 16 // if set and FULL not set, denormals/NaN are never filtered on assign


#define DENORMAL_CLEARING_THRESHOLD 1.0e-50 // when adding/subtracting a constant, assume if it's greater than this, it will clear denormal (the actual value is probably 10^-290...)


#define MAX_SUB_NAMESPACES 32
typedef struct
{
  const char *namespacePathToThis;
  const char *subParmInfo[MAX_SUB_NAMESPACES];
} namespaceInformation;




static int nseel_evallib_stats[5]; // source bytes, static code bytes, call code bytes, data bytes, segments
int *NSEEL_getstats()
{
  return nseel_evallib_stats;
}

static int findLineNumber(const char *exp, int byteoffs)
{
  int lc=0;
  while (byteoffs-->0 && *exp) if (*exp++ =='\n') lc++;
  return lc;
}


static int nseel_vms_referencing_globallist_cnt;
nseel_globalVarItem *nseel_globalreg_list;
static EEL_F *get_global_var(compileContext *ctx, const char *gv, int addIfNotPresent);

static void *__newBlock(llBlock **start,int size, int wantMprotect);

#define OPCODE_IS_TRIVIAL(x) ((x)->opcodeType <= OPCODETYPE_VARPTRPTR)
enum {
  OPCODETYPE_DIRECTVALUE=0,
  OPCODETYPE_DIRECTVALUE_TEMPSTRING, // like directvalue, but will generate a new tempstring value on generate
  OPCODETYPE_VALUE_FROM_NAMESPACENAME, // this.* or namespace.* are encoded this way
  OPCODETYPE_VARPTR,
  OPCODETYPE_VARPTRPTR,
  OPCODETYPE_FUNC1,
  OPCODETYPE_FUNC2,
  OPCODETYPE_FUNC3,
  OPCODETYPE_FUNCX,

  OPCODETYPE_MOREPARAMS,

  OPCODETYPE_INVALID,
};

struct opcodeRec
{
 int opcodeType; 
 int fntype;
 void *fn;
 
 union {
   struct opcodeRec *parms[3];
   struct {
     double directValue;
     EEL_F *valuePtr; // if direct value, valuePtr can be cached
   } dv;
 } parms;
  
 int namespaceidx;
 
 // OPCODETYPE_VALUE_FROM_NAMESPACENAME (relname is either empty or blah)
 // OPCODETYPE_VARPTR if it represents a global variable, will be nonempty
 // OPCODETYPE_FUNC* with fntype=FUNCTYPE_EELFUNC
 const char *relname;
};




static void *newTmpBlock(compileContext *ctx, int size)
{
  const int align = 8;
  const int a1=align-1;
  char *p=(char*)__newBlock(&ctx->tmpblocks_head,size+a1, 0);
  return p+((align-(((INT_PTR)p)&a1))&a1);
}

static void *__newBlock_align(compileContext *ctx, int size, int align, int isForCode) 
{
  const int a1=align-1;
  char *p=(char*)__newBlock(
                            (                            
                             isForCode < 0 ? (isForCode == -2 ? &ctx->pblocks : &ctx->tmpblocks_head) : 
                             isForCode > 0 ? &ctx->blocks_head : 
                             &ctx->blocks_head_data) ,size+a1, isForCode>0);
  return p+((align-(((INT_PTR)p)&a1))&a1);
}

static opcodeRec *newOpCode(compileContext *ctx, const char *str, int opType)
{
  const size_t strszfull = str ? strlen(str) : 0;
  const size_t str_sz = wdl_min(NSEEL_MAX_VARIABLE_NAMELEN, strszfull);

  opcodeRec *rec = (opcodeRec*)__newBlock_align(ctx,
                         (int) (sizeof(opcodeRec) + (str_sz>0 ? str_sz+1 : 0)),
                         8, ctx->isSharedFunctions ? 0 : -1); 
  if (rec)
  {
    memset(rec,0,sizeof(*rec));
    rec->opcodeType = opType;

    if (str_sz > 0) 
    {
      char *p = (char *)(rec+1);
      memcpy(p,str,str_sz);
      p[str_sz]=0;

      rec->relname = p;
    }
    else
    {
      rec->relname = "";
    }
  }

  return rec;
}

#define newCodeBlock(x,a) __newBlock_align(ctx,x,a,1)
#define newDataBlock(x,a) __newBlock_align(ctx,x,a,0)
#define newCtxDataBlock(x,a) __newBlock_align(ctx,x,a,-2)

static void freeBlocks(llBlock **start);

static int __growbuf_resize(eel_growbuf *buf, int newsize)
{
  if (newsize<0)
  {
    free(buf->ptr);
    buf->ptr=NULL;
    buf->alloc=buf->size=0;
    return 0;
  }

  if (newsize > buf->alloc)
  {
    const int newalloc = newsize + 4096 + newsize/2;
    void *newptr = realloc(buf->ptr,newalloc);
    if (!newptr)
    {
      newptr = malloc(newalloc);
      if (!newptr) return 1;
      if (buf->ptr && buf->size) memcpy(newptr,buf->ptr,buf->size);
      free(buf->ptr);
      buf->ptr=newptr;
    }
    else
      buf->ptr = newptr;

    buf->alloc=newalloc;
  }
  buf->size = newsize;
  return 0;
}


#ifndef DECL_ASMFUNC
#define DECL_ASMFUNC(x)         \
  void nseel_asm_##x(void);        \
  void nseel_asm_##x##_end(void);


void _asm_megabuf(void);
void _asm_megabuf_end(void);
void _asm_gmegabuf(void);
void _asm_gmegabuf_end(void);

#endif


  DECL_ASMFUNC(booltofp)
  DECL_ASMFUNC(fptobool)
  DECL_ASMFUNC(fptobool_rev)
  DECL_ASMFUNC(sin)
  DECL_ASMFUNC(cos)
  DECL_ASMFUNC(tan)
  DECL_ASMFUNC(1pdd)
  DECL_ASMFUNC(2pdd)
  DECL_ASMFUNC(2pdds)
  DECL_ASMFUNC(1pp)
  DECL_ASMFUNC(2pp)
  DECL_ASMFUNC(sqr)
  DECL_ASMFUNC(sqrt)
  DECL_ASMFUNC(log)
  DECL_ASMFUNC(log10)
  DECL_ASMFUNC(abs)
  DECL_ASMFUNC(min)
  DECL_ASMFUNC(max)
  DECL_ASMFUNC(min_fp)
  DECL_ASMFUNC(max_fp)
  DECL_ASMFUNC(sig)
  DECL_ASMFUNC(sign)
  DECL_ASMFUNC(band)
  DECL_ASMFUNC(bor)
  DECL_ASMFUNC(bnot)
  DECL_ASMFUNC(bnotnot)
  DECL_ASMFUNC(if)
  DECL_ASMFUNC(fcall)
  DECL_ASMFUNC(repeat)
  DECL_ASMFUNC(repeatwhile)
  DECL_ASMFUNC(equal)
  DECL_ASMFUNC(equal_exact)
  DECL_ASMFUNC(notequal_exact)
  DECL_ASMFUNC(notequal)
  DECL_ASMFUNC(below)
  DECL_ASMFUNC(above)
  DECL_ASMFUNC(beloweq)
  DECL_ASMFUNC(aboveeq)
  DECL_ASMFUNC(assign)
  DECL_ASMFUNC(assign_fromfp)
  DECL_ASMFUNC(assign_fast)
  DECL_ASMFUNC(assign_fast_fromfp)
  DECL_ASMFUNC(add)
  DECL_ASMFUNC(sub)
  DECL_ASMFUNC(add_op)
  DECL_ASMFUNC(sub_op)
  DECL_ASMFUNC(add_op_fast)
  DECL_ASMFUNC(sub_op_fast)
  DECL_ASMFUNC(mul)
  DECL_ASMFUNC(div)
  DECL_ASMFUNC(mul_op)
  DECL_ASMFUNC(div_op)
  DECL_ASMFUNC(mul_op_fast)
  DECL_ASMFUNC(div_op_fast)
  DECL_ASMFUNC(mod)
  DECL_ASMFUNC(shl)
  DECL_ASMFUNC(shr)
  DECL_ASMFUNC(mod_op)
  DECL_ASMFUNC(or)
  DECL_ASMFUNC(or0)
  DECL_ASMFUNC(xor)
  DECL_ASMFUNC(xor_op)
  DECL_ASMFUNC(and)
  DECL_ASMFUNC(or_op)
  DECL_ASMFUNC(and_op)
  DECL_ASMFUNC(uplus)
  DECL_ASMFUNC(uminus)
  DECL_ASMFUNC(invsqrt)
  DECL_ASMFUNC(dbg_getstackptr)
#ifdef NSEEL_EEL1_COMPAT_MODE
  DECL_ASMFUNC(exec2)
#endif

  DECL_ASMFUNC(stack_push)
  DECL_ASMFUNC(stack_pop)
  DECL_ASMFUNC(stack_pop_fast) // just returns value, doesn't mod param
  DECL_ASMFUNC(stack_peek)
  DECL_ASMFUNC(stack_peek_int)
  DECL_ASMFUNC(stack_peek_top)
  DECL_ASMFUNC(stack_exch)

static void *NSEEL_PProc_GRAM(void *data, int data_size, compileContext *ctx)
{
  if (data_size>0) data=EEL_GLUE_set_immediate(data, (INT_PTR)ctx->gram_blocks);
  return data;
}

static void *NSEEL_PProc_Stack(void *data, int data_size, compileContext *ctx)
{
  codeHandleType *ch=ctx->tmpCodeHandle;

  if (data_size>0) 
  {
    UINT_PTR m1=(UINT_PTR)(NSEEL_STACK_SIZE * sizeof(EEL_F) - 1);
    UINT_PTR stackptr = ((UINT_PTR) (&ch->stack));

    ch->want_stack=1;
    if (!ch->stack) ch->stack = newDataBlock(NSEEL_STACK_SIZE*sizeof(EEL_F),NSEEL_STACK_SIZE*sizeof(EEL_F));

    data=EEL_GLUE_set_immediate(data, stackptr);
    data=EEL_GLUE_set_immediate(data, m1); // and
    data=EEL_GLUE_set_immediate(data, ((UINT_PTR)ch->stack&~m1)); //or
  }
  return data;
}

static void *NSEEL_PProc_Stack_PeekInt(void *data, int data_size, compileContext *ctx, INT_PTR offs)
{
  codeHandleType *ch=ctx->tmpCodeHandle;

  if (data_size>0) 
  {
    UINT_PTR m1=(UINT_PTR)(NSEEL_STACK_SIZE * sizeof(EEL_F) - 1);
    UINT_PTR stackptr = ((UINT_PTR) (&ch->stack));

    ch->want_stack=1;
    if (!ch->stack) ch->stack = newDataBlock(NSEEL_STACK_SIZE*sizeof(EEL_F),NSEEL_STACK_SIZE*sizeof(EEL_F));

    data=EEL_GLUE_set_immediate(data, stackptr);
    data=EEL_GLUE_set_immediate(data, offs);
    data=EEL_GLUE_set_immediate(data, m1); // and
    data=EEL_GLUE_set_immediate(data, ((UINT_PTR)ch->stack&~m1)); //or
  }
  return data;
}
static void *NSEEL_PProc_Stack_PeekTop(void *data, int data_size, compileContext *ctx)
{
  codeHandleType *ch=ctx->tmpCodeHandle;

  if (data_size>0) 
  {
    UINT_PTR stackptr = ((UINT_PTR) (&ch->stack));

    ch->want_stack=1;
    if (!ch->stack) ch->stack = newDataBlock(NSEEL_STACK_SIZE*sizeof(EEL_F),NSEEL_STACK_SIZE*sizeof(EEL_F));

    data=EEL_GLUE_set_immediate(data, stackptr);
  }
  return data;
}

#if defined(_MSC_VER) && _MSC_VER >= 1400
static double __floor(double a) { return floor(a); }
static double __ceil(double a) { return ceil(a); }
#define floor __floor
#define ceil __ceil
#endif


#ifdef NSEEL_EEL1_COMPAT_MODE
static double eel1band(double a, double b)
{
  return (fabs(a)>NSEEL_CLOSEFACTOR && fabs(b) > NSEEL_CLOSEFACTOR) ? 1.0 : 0.0;
}
static double eel1bor(double a, double b)
{
  return (fabs(a)>NSEEL_CLOSEFACTOR || fabs(b) > NSEEL_CLOSEFACTOR) ? 1.0 : 0.0;
}

static double eel1sigmoid(double x, double constraint)
{
  double t = (1+exp(-x * (constraint)));
  return fabs(t)>NSEEL_CLOSEFACTOR ? 1.0/t : 0;
}

#endif



#define FUNCTIONTYPE_PARAMETERCOUNTMASK 0xff

#define BIF_NPARAMS_MASK       0x7ffff00
#define BIF_RETURNSONSTACK     0x0000100
#define BIF_LASTPARMONSTACK    0x0000200
#define BIF_RETURNSBOOL        0x0000400
#define BIF_LASTPARM_ASBOOL    0x0000800
//                             0x00?0000 -- taken by FP stack flags
#define BIF_TAKES_VARPARM      0x0400000
#define BIF_TAKES_VARPARM_EX   0x0C00000 // this is like varparm but check count exactly
#define BIF_WONTMAKEDENORMAL   0x0100000
#define BIF_CLEARDENORMAL      0x0200000

#if defined(GLUE_HAS_FXCH) && GLUE_MAX_FPSTACK_SIZE > 0
  #define BIF_SECONDLASTPARMST 0x0001000 // use with BIF_LASTPARMONSTACK only (last two parameters get passed on fp stack)
  #define BIF_LAZYPARMORDERING 0x0002000 // allow optimizer to avoid fxch when using BIF_TWOPARMSONFPSTACK_LAZY etc
  #define BIF_REVERSEFPORDER   0x0004000 // force a fxch (reverse order of last two parameters on fp stack, used by comparison functions)

  #ifndef BIF_FPSTACKUSE
    #define BIF_FPSTACKUSE(x) (((x)>=0&&(x)<8) ? ((7-(x))<<16):0)
  #endif
  #ifndef BIF_GETFPSTACKUSE
    #define BIF_GETFPSTACKUSE(x) (7 - (((x)>>16)&7))
  #endif
#else
  // do not support fp stack use unless GLUE_HAS_FXCH and GLUE_MAX_FPSTACK_SIZE>0
  #define BIF_SECONDLASTPARMST 0
  #define BIF_LAZYPARMORDERING 0
  #define BIF_REVERSEFPORDER   0
  #define BIF_FPSTACKUSE(x) 0
  #define BIF_GETFPSTACKUSE(x) 0
#endif

#define BIF_TWOPARMSONFPSTACK (BIF_SECONDLASTPARMST|BIF_LASTPARMONSTACK)
#define BIF_TWOPARMSONFPSTACK_LAZY (BIF_LAZYPARMORDERING|BIF_SECONDLASTPARMST|BIF_LASTPARMONSTACK)


#ifndef GLUE_HAS_NATIVE_TRIGSQRTLOG
static double sqrt_fabs(double a) { return sqrt(fabs(a)); }
#endif


EEL_F NSEEL_CGEN_CALL nseel_int_rand(EEL_F f);

#define FNPTR_HAS_CONDITIONAL_EXEC(op)  \
  (op->fntype == FN_LOGICAL_AND || \
   op->fntype == FN_LOGICAL_OR ||  \
   op->fntype == FN_IF_ELSE || \
   op->fntype == FN_WHILE || \
   op->fntype == FN_LOOP)

static functionType fnTable1[] = {
#ifndef GLUE_HAS_NATIVE_TRIGSQRTLOG
   { "sin",   nseel_asm_1pdd,nseel_asm_1pdd_end,   1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_WONTMAKEDENORMAL, {&sin} },
   { "cos",    nseel_asm_1pdd,nseel_asm_1pdd_end,   1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_CLEARDENORMAL, {&cos} },
   { "tan",    nseel_asm_1pdd,nseel_asm_1pdd_end,   1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK, {&tan}  },
   { "sqrt",   nseel_asm_1pdd,nseel_asm_1pdd_end,  1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_WONTMAKEDENORMAL, {&sqrt_fabs}, },
   { "log",    nseel_asm_1pdd,nseel_asm_1pdd_end,   1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK, {&log} },
   { "log10",  nseel_asm_1pdd,nseel_asm_1pdd_end, 1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK, {&log10} },
#else
   { "sin",   nseel_asm_sin,nseel_asm_sin_end,   1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_WONTMAKEDENORMAL|BIF_FPSTACKUSE(1) },
   { "cos",    nseel_asm_cos,nseel_asm_cos_end,   1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_CLEARDENORMAL|BIF_FPSTACKUSE(1) },
   { "tan",    nseel_asm_tan,nseel_asm_tan_end,   1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(1) },
   { "sqrt",   nseel_asm_sqrt,nseel_asm_sqrt_end,  1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(1)|BIF_WONTMAKEDENORMAL },
   { "log",    nseel_asm_log,nseel_asm_log_end,   1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(3), },
   { "log10",  nseel_asm_log10,nseel_asm_log10_end, 1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(3), },
#endif


   { "asin",   nseel_asm_1pdd,nseel_asm_1pdd_end,  1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK, {&asin}, },
   { "acos",   nseel_asm_1pdd,nseel_asm_1pdd_end,  1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK, {&acos}, },
   { "atan",   nseel_asm_1pdd,nseel_asm_1pdd_end,  1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK, {&atan}, },
   { "atan2",  nseel_asm_2pdd,nseel_asm_2pdd_end, 2|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK, {&atan2}, },
   { "exp",    nseel_asm_1pdd,nseel_asm_1pdd_end,   1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK, {&exp}, },
   { "abs",    nseel_asm_abs,nseel_asm_abs_end,   1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(0)|BIF_WONTMAKEDENORMAL },
   { "sqr",    nseel_asm_sqr,nseel_asm_sqr_end,   1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(1) },
   { "min",    nseel_asm_min,nseel_asm_min_end,   2|NSEEL_NPARAMS_FLAG_CONST|BIF_FPSTACKUSE(3)|BIF_WONTMAKEDENORMAL },
   { "max",    nseel_asm_max,nseel_asm_max_end,   2|NSEEL_NPARAMS_FLAG_CONST|BIF_FPSTACKUSE(3)|BIF_WONTMAKEDENORMAL },
   { "sign",   nseel_asm_sign,nseel_asm_sign_end,  1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(2)|BIF_CLEARDENORMAL, },
   { "rand",   nseel_asm_1pdd,nseel_asm_1pdd_end,  1|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_CLEARDENORMAL, {&nseel_int_rand}, },

   { "floor",  nseel_asm_1pdd,nseel_asm_1pdd_end, 1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_CLEARDENORMAL, {&floor} },
   { "ceil",   nseel_asm_1pdd,nseel_asm_1pdd_end,  1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_CLEARDENORMAL, {&ceil} },

   { "invsqrt",   nseel_asm_invsqrt,nseel_asm_invsqrt_end,  1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(3), {GLUE_INVSQRT_NEEDREPL} },

   { "__dbg_getstackptr",   nseel_asm_dbg_getstackptr,nseel_asm_dbg_getstackptr_end,  1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(1),  },

#ifdef NSEEL_EEL1_COMPAT_MODE
  { "sigmoid", nseel_asm_2pdd,nseel_asm_2pdd_end, 2|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK, {&eel1sigmoid}, },

  // these differ from _and/_or, they always evaluate both...
  { "band",  nseel_asm_2pdd,nseel_asm_2pdd_end, 2|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK|BIF_CLEARDENORMAL , {&eel1band}, },
  { "bor",  nseel_asm_2pdd,nseel_asm_2pdd_end, 2|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK|BIF_CLEARDENORMAL , {&eel1bor}, },

  {"exec2",nseel_asm_exec2,nseel_asm_exec2_end,2|NSEEL_NPARAMS_FLAG_CONST|BIF_WONTMAKEDENORMAL},
  {"exec3",nseel_asm_exec2,nseel_asm_exec2_end,3|NSEEL_NPARAMS_FLAG_CONST|BIF_WONTMAKEDENORMAL},
#endif // end EEL1 compat


  {"freembuf",_asm_generic1parm,_asm_generic1parm_end,1,{&__NSEEL_RAM_MemFree},NSEEL_PProc_RAM},
  {"memcpy",_asm_generic3parm,_asm_generic3parm_end,3,{&__NSEEL_RAM_MemCpy},NSEEL_PProc_RAM},
  {"memset",_asm_generic3parm,_asm_generic3parm_end,3,{&__NSEEL_RAM_MemSet},NSEEL_PProc_RAM},
  {"__memtop",_asm_generic1parm,_asm_generic1parm_end,1,{&__NSEEL_RAM_MemTop},NSEEL_PProc_RAM},
  {"mem_set_values",_asm_generic2parm_retd,_asm_generic2parm_retd_end,2|BIF_TAKES_VARPARM|BIF_RETURNSONSTACK,{&__NSEEL_RAM_Mem_SetValues},NSEEL_PProc_RAM},
  {"mem_get_values",_asm_generic2parm_retd,_asm_generic2parm_retd_end,2|BIF_TAKES_VARPARM|BIF_RETURNSONSTACK,{&__NSEEL_RAM_Mem_GetValues},NSEEL_PProc_RAM},

  {"stack_push",nseel_asm_stack_push,nseel_asm_stack_push_end,1|BIF_FPSTACKUSE(0),{0,},NSEEL_PProc_Stack},
  {"stack_pop",nseel_asm_stack_pop,nseel_asm_stack_pop_end,1|BIF_FPSTACKUSE(1),{0,},NSEEL_PProc_Stack},
  {"stack_peek",nseel_asm_stack_peek,nseel_asm_stack_peek_end,1|NSEEL_NPARAMS_FLAG_CONST|BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(0),{0,},NSEEL_PProc_Stack},
  {"stack_exch",nseel_asm_stack_exch,nseel_asm_stack_exch_end,1|BIF_FPSTACKUSE(1), {0,},NSEEL_PProc_Stack_PeekTop},
};

static eel_function_table default_user_funcs;

static int functable_lowerbound(functionType *list, int list_sz, const char *name, int *ismatch)
{
  int a = 0, c = list_sz;
  while (a != c)
  {
    const int b = (a+c)/2;
    const int cmp = stricmp(name,list[b].name);
    if (cmp > 0) a = b+1;
    else if (cmp < 0) c = b;
    else
    {
      *ismatch = 1;
      return b;
    }
  }
  *ismatch = 0;
  return a;
}

static int funcTypeCmp(const void *a, const void *b) { return stricmp(((functionType*)a)->name,((functionType*)b)->name); }
functionType *nseel_getFunctionByName(compileContext *ctx, const char *name, int *mchk)
{
  eel_function_table *tab = ctx && ctx->registered_func_tab ? ctx->registered_func_tab : &default_user_funcs;
  static char sorted;
  const int fn1size = (int) (sizeof(fnTable1)/sizeof(fnTable1[0]));
  int idx,match;
  if (!sorted)
  {
    NSEEL_HOSTSTUB_EnterMutex();
    if (!sorted) qsort(fnTable1,fn1size,sizeof(fnTable1[0]),funcTypeCmp);
    sorted=1;
    NSEEL_HOSTSTUB_LeaveMutex();
  }
  idx=functable_lowerbound(fnTable1,fn1size,name,&match);
  if (match) return fnTable1+idx;

  if ((!ctx || !(ctx->current_compile_flags&NSEEL_CODE_COMPILE_FLAG_ONLY_BUILTIN_FUNCTIONS)) && tab->list)
  {
    idx=functable_lowerbound(tab->list,tab->list_size,name,&match);
    if (match) 
    {
      if (mchk)
      {
        while (idx>0 && !stricmp(tab->list[idx-1].name,name)) idx--;
        *mchk = tab->list_size - 1 - idx;
      }
      return tab->list + idx;
    }
  }

  return NULL;
}

int NSEEL_init() // returns 0 on success
{

#ifdef EEL_VALIDATE_FSTUBS
  int a;
  for (a=0;a < sizeof(fnTable1)/sizeof(fnTable1[0]);a++)
  {
    char *code_startaddr = (char*)fnTable1[a].afunc;
    char *endp = (char *)fnTable1[a].func_e;
    // validate
    int sz=0;
    char *f=(char *)GLUE_realAddress(code_startaddr,endp,&sz);

    if (f+sz > endp) 
    {
#ifdef _WIN32
      OutputDebugString("bad eel function stub\n");
#else
      printf("bad eel function stub\n");
#endif
      *(char *)NULL = 0;
    }
  }
#ifdef _WIN32
      OutputDebugString("eel function stub (builtin) validation complete\n");
#else
      printf("eel function stub (builtin) validation complete\n");
#endif
#endif

  NSEEL_quit();
  return 0;
}

void NSEEL_quit()
{
  free(default_user_funcs.list);
  default_user_funcs.list = NULL;
  default_user_funcs.list_size = 0;
}

void NSEEL_addfunc_varparm_ex(const char *name, int min_np, int want_exact, NSEEL_PPPROC pproc, EEL_F (NSEEL_CGEN_CALL *fptr)(void *, INT_PTR, EEL_F **), eel_function_table *destination)
{
  const int sz = (int) ((char *)_asm_generic2parm_retd_end-(char *)_asm_generic2parm_retd);
  NSEEL_addfunctionex2(name,min_np|(want_exact?BIF_TAKES_VARPARM_EX:BIF_TAKES_VARPARM),(char *)_asm_generic2parm_retd,sz,pproc,fptr,NULL,destination);
}
void NSEEL_addfunc_ret_type(const char *name, int np, int ret_type,  NSEEL_PPPROC pproc, void *fptr, eel_function_table *destination) // ret_type=-1 for bool, 1 for value, 0 for ptr
{
  char *stub=NULL;
  int stubsz=0;
#define DOSTUB(np) { \
    stub = (ret_type == 1 ? (char*)_asm_generic##np##parm_retd : (char*)_asm_generic##np##parm); \
    stubsz = (int) ((ret_type == 1 ? (char*)_asm_generic##np##parm_retd_end : (char *)_asm_generic##np##parm_end) - stub); \
  }

  if (np == 1) DOSTUB(1)
  else if (np == 2) DOSTUB(2)
  else if (np == 3) DOSTUB(3)
#undef DOSTUB

  if (stub) NSEEL_addfunctionex2(name,np|(ret_type == -1 ? BIF_RETURNSBOOL:0), stub, stubsz, pproc,fptr,NULL,destination);
}

void NSEEL_addfunctionex2(const char *name, int nparms, char *code_startaddr, int code_len, NSEEL_PPPROC pproc, void *fptr, void *fptr2, eel_function_table *destination)
{
  const int list_size_chunk = 128;
  functionType *r;
  if (!destination) destination = &default_user_funcs;

  if (!destination->list || !(destination->list_size & (list_size_chunk-1)))
  {
    void *nv = realloc(destination->list, (destination->list_size + list_size_chunk)*sizeof(functionType));
    if (!nv) return;
    destination->list = (functionType *)nv;
  }
  if (destination->list)
  {
    int match,idx;

    idx=functable_lowerbound(destination->list,destination->list_size,name,&match);

#ifdef EEL_VALIDATE_FSTUBS
    {
      char *endp = code_startaddr+code_len;
      // validate
      int sz=0;
      char *f=(char *)GLUE_realAddress(code_startaddr,endp,&sz);

      if (f+sz > endp) 
      {
#ifdef _WIN32
        OutputDebugString("bad eel function stub\n");
#else
        printf("bad eel function stub\n");
#endif
        *(char *)NULL = 0;
      }
#ifdef _WIN32
      OutputDebugString(name);
      OutputDebugString(" - validated eel function stub\n");
#else
      printf("eel function stub validation complete for %s\n",name);
#endif
    }
#endif

    r = destination->list + idx;
    if (idx < destination->list_size)
      memmove(r + 1, r, (destination->list_size - idx) * sizeof(functionType));
    destination->list_size++;

    memset(r, 0, sizeof(functionType));

    if (!(nparms & BIF_RETURNSBOOL)) 
    {
      if (code_startaddr == (void *)&_asm_generic1parm_retd || 
          code_startaddr == (void *)&_asm_generic2parm_retd ||
          code_startaddr == (void *)&_asm_generic3parm_retd)
      {
        nparms |= BIF_RETURNSONSTACK;
      }
    }
    r->nParams = nparms;
    r->name = name;
    r->afunc = code_startaddr;
    r->func_e = code_startaddr + code_len;
    r->pProc = pproc;
    r->replptrs[0] = fptr;
    r->replptrs[1] = fptr2;
  }
}


//---------------------------------------------------------------------------------------------------------------
static void freeBlocks(llBlock **start)
{
  llBlock *s=*start;
  *start=0;
  while (s)
  {
    llBlock *llB = s->next;
    free(s);
    s=llB;
  }
}

//---------------------------------------------------------------------------------------------------------------
static void *__newBlock(llBlock **start, int size, int wantMprotect)
{
#if !defined(EEL_DOESNT_NEED_EXEC_PERMS) && defined(_WIN32)
  DWORD ov;
  UINT_PTR offs,eoffs;
#endif
  llBlock *llb;
  int alloc_size;
  if (*start && (LLB_DSIZE - (*start)->sizeused) >= size)
  {
    void *t=(*start)->block+(*start)->sizeused;
    (*start)->sizeused+=(size+7)&~7;
    return t;
  }

  alloc_size=sizeof(llBlock);
  if ((int)size > LLB_DSIZE) alloc_size += size - LLB_DSIZE;
  llb = (llBlock *)malloc(alloc_size); // grab bigger block if absolutely necessary (heh)
  if (!llb) return NULL;
  
#ifndef EEL_DOESNT_NEED_EXEC_PERMS
  if (wantMprotect) 
  {
  #ifdef _WIN32
    offs=((UINT_PTR)llb)&~4095;
    eoffs=((UINT_PTR)llb + alloc_size + 4095)&~4095;
    VirtualProtect((LPVOID)offs,eoffs-offs,PAGE_EXECUTE_READWRITE,&ov);
  //  MessageBox(NULL,"vprotecting, yay\n","a",0);
  #else
    {
      static int pagesize = 0;
      if (!pagesize)
      {
        pagesize=(int)sysconf(_SC_PAGESIZE);
        if (!pagesize) pagesize=4096;
      }
      uintptr_t offs,eoffs;
      offs=((uintptr_t)llb)&~(pagesize-1);
      eoffs=((uintptr_t)llb + alloc_size + pagesize-1)&~(pagesize-1);
      mprotect((void*)offs,eoffs-offs,PROT_WRITE|PROT_READ|PROT_EXEC);
    }
  #endif
  }
#endif
  llb->sizeused=(size+7)&~7;
  llb->next = *start;  
  *start = llb;
  return llb->block;
}


//---------------------------------------------------------------------------------------------------------------
opcodeRec *nseel_createCompiledValue(compileContext *ctx, EEL_F value)
{
  opcodeRec *r=newOpCode(ctx,NULL,OPCODETYPE_DIRECTVALUE);
  if (r)
  {
    r->parms.dv.directValue = value; 
  }
  return r;
}

opcodeRec *nseel_createCompiledValuePtr(compileContext *ctx, EEL_F *addrValue, const char *namestr)
{
  opcodeRec *r=newOpCode(ctx,namestr,OPCODETYPE_VARPTR);
  if (!r) return 0;

  r->parms.dv.valuePtr=addrValue;

  return r;
}

static int validate_varname_for_function(compileContext *ctx, const char *name)
{
  if (!ctx->function_curName || !ctx->function_globalFlag) return 1;

  if (ctx->function_localTable_Size[2] > 0 && ctx->function_localTable_Names[2])
  {
    char * const * const namelist = ctx->function_localTable_Names[2];
    const int namelist_sz = ctx->function_localTable_Size[2];
    int i;
    const size_t name_len = strlen(name);

    for (i=0;i<namelist_sz;i++) 
    {
      const char *nmchk=namelist[i];
      const size_t l = strlen(nmchk);
      if (l > 1 && nmchk[l-1] == '*')
      {
        if (name_len >= l && !strnicmp(nmchk,name,l-1) && name[l-1]=='.')  return 1;
      }
      else
      {
        if (name_len == l && !stricmp(nmchk,name)) return 1;
      }
    }
  }

  return 0;
}

opcodeRec *nseel_resolve_named_symbol(compileContext *ctx, opcodeRec *rec, int parmcnt, int *errOut)
{
  const int isFunctionMode = parmcnt >= 0;
  int rel_prefix_len=0;
  int rel_prefix_idx=-2;
  int i;    
  char match_parmcnt[4]={-1,-1,-1,-1}; // [3] is guess
  unsigned char match_parmcnt_pos=0;
  char *sname = (char *)rec->relname;
  int is_string_prefix = parmcnt < 0 && sname[0] == '#';
  const char *prevent_function_calls = NULL;

  if (errOut) *errOut = 0;

  if (sname) sname += is_string_prefix;

  if (rec->opcodeType != OPCODETYPE_VARPTR || !sname || !sname[0]) return NULL;

  if (!isFunctionMode && !is_string_prefix && !strnicmp(sname,"reg",3) && isdigit(sname[3]) && isdigit(sname[4]) && !sname[5])
  {
    EEL_F *a=get_global_var(ctx,sname,1);
    if (a) 
    {
      rec->parms.dv.valuePtr = a;
      sname[0]=0; // for dump_ops compat really, but this shouldn't be needed anyway
    }
    return rec;
  }

  if (ctx->function_curName)
  {
    if (!strnicmp(sname,"this.",5))
    {
      rel_prefix_len=5;
      rel_prefix_idx=-1;
    } 
    else if (!stricmp(sname,"this"))
    {
      rel_prefix_len=4;
      rel_prefix_idx=-1;
    } 
  
    // scan for parameters/local variables before user functions   
    if (rel_prefix_idx < -1 &&
        ctx->function_localTable_Size[0] > 0 &&
        ctx->function_localTable_Names[0] && 
        ctx->function_localTable_ValuePtrs)
    {
      char * const * const namelist = ctx->function_localTable_Names[0];
      const int namelist_sz = ctx->function_localTable_Size[0];
      for (i=0; i < namelist_sz; i++)
      {
        const char *p = namelist[i];
        if (p)
        {
          if (!isFunctionMode && !is_string_prefix && !strnicmp(p,sname,NSEEL_MAX_VARIABLE_NAMELEN))
          {
            rec->opcodeType = OPCODETYPE_VARPTRPTR;
            rec->parms.dv.valuePtr=(EEL_F *)(ctx->function_localTable_ValuePtrs+i);
            rec->parms.dv.directValue=0.0;
            return rec;
          }
          else 
          {
            const size_t plen = strlen(p);
            if (plen > 1 && p[plen-1] == '*' && !strnicmp(p,sname,plen-1) && ((sname[plen-1] == '.'&&sname[plen]) || !sname[plen-1]))
            {
              rel_prefix_len=(int) (sname[plen-1] ? plen : plen-1);
              rel_prefix_idx=i;
              break;
            }
          }
        }
      }
    }
    // if instance name set, translate sname or sname.* into "this.sname.*"
    if (rel_prefix_idx < -1 &&
        ctx->function_localTable_Size[1] > 0 && 
        ctx->function_localTable_Names[1])
    {
      char * const * const namelist = ctx->function_localTable_Names[1];
      const int namelist_sz = ctx->function_localTable_Size[1];
      const char *full_sname = rec->relname; // include # in checks
      for (i=0; i < namelist_sz; i++)
      {
        const char *p = namelist[i];
        if (p && *p)
        {
          const size_t tl = strlen(p);     
          if (!strnicmp(p,full_sname,tl) && (full_sname[tl] == 0 || full_sname[tl] == '.'))
          {
            rel_prefix_len=0; // treat as though this. prefixes is present
            rel_prefix_idx=-1;
            break;
          }
        }
      }
    }
    if (rel_prefix_idx >= -1) 
    {
      ctx->function_usesNamespaces=1;
    }
  } // ctx->function_curName

  if (!isFunctionMode)
  {
    // instance variables
    if (rel_prefix_idx >= -1) 
    {
      rec->opcodeType = OPCODETYPE_VALUE_FROM_NAMESPACENAME;
      rec->namespaceidx = rel_prefix_idx;
      if (rel_prefix_len > 0) 
      {
        if (is_string_prefix) sname[-1] = '#';
        memmove(sname, sname+rel_prefix_len, strlen(sname + rel_prefix_len) + 1);
      }
    }
    else 
    {
      // no namespace index, so it must be a global
      if (!validate_varname_for_function(ctx,rec->relname)) 
      {
        if (errOut) *errOut = 1;
        if (ctx->last_error_string[0]) lstrcatn(ctx->last_error_string, ", ", sizeof(ctx->last_error_string));
        snprintf_append(ctx->last_error_string,sizeof(ctx->last_error_string),"global '%s' inaccessible",rec->relname);
        return NULL;
      }
    }
  
    return rec;
  }

  if (ctx->func_check)
    prevent_function_calls = ctx->func_check(sname,ctx->func_check_user);
 
  ////////// function mode
  // first off, while() and loop() are special and can't be overridden
  //
  if (parmcnt == 1 && !stricmp("while",sname) && !prevent_function_calls)
  {
    rec->opcodeType = OPCODETYPE_FUNC1;
    rec->fntype = FN_WHILE;
    return rec;
  }
  if (parmcnt == 2 && !stricmp("loop",sname) && !prevent_function_calls)
  {
    rec->opcodeType = OPCODETYPE_FUNC2;
    rec->fntype = FN_LOOP;
    return rec;
  }
  
  //
  // resolve user function names before builtin functions -- this allows the user to override default functions
  if (!(ctx->current_compile_flags & NSEEL_CODE_COMPILE_FLAG_ONLY_BUILTIN_FUNCTIONS))
  {
    _codeHandleFunctionRec *best=NULL;
    size_t bestlen=0;
    const char * const ourcall = sname+rel_prefix_len;
    const size_t ourcall_len = strlen(ourcall);
    int pass;
    for (pass=0;pass<2;pass++)
    {
      _codeHandleFunctionRec *fr = pass ? ctx->functions_common : ctx->functions_local;
      // sname is [namespace.[ns.]]function, find best match of function that matches the right end   
      while (fr)
      {
        int this_np = fr->num_params;
        const char *thisfunc = fr->fname;
        const size_t thisfunc_len = strlen(thisfunc);
        if (this_np < 1) this_np=1;
        if (thisfunc_len == ourcall_len && !stricmp(thisfunc,ourcall))
        {
          if (this_np == parmcnt)
          {
            bestlen = thisfunc_len;
            best = fr;
            break; // found exact match, finished
          }
          else
          {
            if (match_parmcnt_pos < 3) match_parmcnt[match_parmcnt_pos++] = fr->num_params;
          }
        }

        if (thisfunc_len > bestlen && thisfunc_len < ourcall_len && ourcall[ourcall_len - thisfunc_len - 1] == '.' && !stricmp(thisfunc,ourcall + ourcall_len - thisfunc_len))
        {
          if (this_np == parmcnt) 
          {
            bestlen = thisfunc_len;
            best = fr;
          }
          else
            if (match_parmcnt[3]<0) match_parmcnt[3]=fr->num_params;
        }
        fr=fr->next;
      }
      if (fr) break; // found exact match, finished
    }
    
    if (best)
    {
      switch (parmcnt)
      {
        case 0:
        case 1: rec->opcodeType = OPCODETYPE_FUNC1; break;
        case 2: rec->opcodeType = OPCODETYPE_FUNC2; break;
        case 3: rec->opcodeType = OPCODETYPE_FUNC3; break;
        default: rec->opcodeType = OPCODETYPE_FUNCX; break;
      }
      if (ourcall != rec->relname) memmove((char *)rec->relname, ourcall, strlen(ourcall)+1);

      if (ctx->function_curName && rel_prefix_idx<0)
      {
        // if no namespace specified, and this.commonprefix.func() called, remove common prefixes and set prefixidx to be this
        const char *p=ctx->function_curName;
        if (*p) p++;
        while (*p && *p != '.')  p++;
        if (*p && p[1]) // we have a dot!
        {
          while (p[1]) p++; // go to last char of string, which doesn't allow possible trailing dot to be checked

          while (--p > ctx->function_curName) // do not check possible leading dot
          {            
            if (*p == '.')
            {
              const size_t cmplen = p+1-ctx->function_curName;
              if (!strnicmp(rec->relname,ctx->function_curName,cmplen) && rec->relname[cmplen])
              {
                const char *src=rec->relname + cmplen;
                memmove((char *)rec->relname, src, strlen(src)+1);
                rel_prefix_idx=-1; 
                ctx->function_usesNamespaces=1;
                break;
              }
            }
          }
        }
      }

      if (ctx->function_curName && rel_prefix_idx < -1 && 
          strchr(rec->relname,'.') && !validate_varname_for_function(ctx,rec->relname))
      {
        if (errOut) *errOut = 1;
        if (ctx->last_error_string[0]) lstrcatn(ctx->last_error_string, ", ", sizeof(ctx->last_error_string));
        snprintf_append(ctx->last_error_string,sizeof(ctx->last_error_string),"namespaced function '%s' inaccessible",rec->relname);
        return NULL;
      }

      rec->namespaceidx = rel_prefix_idx;
      rec->fntype = FUNCTYPE_EELFUNC;
      rec->fn = best;
      return rec;
    }    
  }

  if (prevent_function_calls)
  {
    if (ctx->last_error_string[0]) lstrcatn(ctx->last_error_string, ", ", sizeof(ctx->last_error_string));
    snprintf_append(ctx->last_error_string,sizeof(ctx->last_error_string),"'%.30s': %s",sname, prevent_function_calls);
    if (errOut) *errOut = 0;
    return NULL;
  }

#ifdef NSEEL_EEL1_COMPAT_MODE
    if (!stricmp(sname,"assign")) 
    {
      if (parmcnt == 2)
      {
        rec->opcodeType = OPCODETYPE_FUNC2;
        rec->fntype = FN_ASSIGN;
        return rec;
      }
      if (match_parmcnt_pos < 3) match_parmcnt[match_parmcnt_pos++] = 2;
    }
    else if (!stricmp(sname,"if")) 
    {
      if (parmcnt == 3)
      {
        rec->opcodeType = OPCODETYPE_FUNC3;
        rec->fntype = FN_IF_ELSE;
        return rec;
      }
      if (match_parmcnt_pos < 3) match_parmcnt[match_parmcnt_pos++] = 3;
    }
    else if (!stricmp(sname,"equal")) 
    {
      if (parmcnt == 2)
      {
        rec->opcodeType = OPCODETYPE_FUNC2;
        rec->fntype = FN_EQ;
        return rec;
      }
      if (match_parmcnt_pos < 3) match_parmcnt[match_parmcnt_pos++] = 2;
    }
    else if (!stricmp(sname,"below")) 
    {
      if (parmcnt == 2)
      {
        rec->opcodeType = OPCODETYPE_FUNC2;
        rec->fntype = FN_LT;
        return rec;
      }
      if (match_parmcnt_pos < 3) match_parmcnt[match_parmcnt_pos++] = 2;
    }
    else if (!stricmp(sname,"above")) 
    {
      if (parmcnt == 2)
      {
        rec->opcodeType = OPCODETYPE_FUNC2;
        rec->fntype = FN_GT;
        return rec;
      }
      if (match_parmcnt_pos < 3) match_parmcnt[match_parmcnt_pos++] = 2;
    }
    else if (!stricmp(sname,"bnot")) 
    {
      if (parmcnt == 1)
      {
        rec->opcodeType = OPCODETYPE_FUNC1;
        rec->fntype = FN_NOT;
        return rec;
      }
      if (match_parmcnt_pos < 3) match_parmcnt[match_parmcnt_pos++] = 1;
    }
    else if (!stricmp(sname,"megabuf")) 
    {
      if (parmcnt == 1)
      {
        rec->opcodeType = OPCODETYPE_FUNC1;
        rec->fntype = FN_MEMORY;
        return rec;
      }
      if (match_parmcnt_pos < 3) match_parmcnt[match_parmcnt_pos++] = 1;
    }
    else if (!stricmp(sname,"gmegabuf")) 
    {
      if (parmcnt == 1)
      {
        rec->opcodeType = OPCODETYPE_FUNC1;
        rec->fntype = FN_GMEMORY;
        return rec;
      }
      if (match_parmcnt_pos < 3) match_parmcnt[match_parmcnt_pos++] = 1;
    }
    else
#endif
  // convert legacy pow() to FN_POW
  if (!stricmp("pow",sname))
  {
    if (parmcnt == 2)
    {
      rec->opcodeType = OPCODETYPE_FUNC2;
      rec->fntype = FN_POW;
      return rec;
    }
    if (match_parmcnt_pos < 3) match_parmcnt[match_parmcnt_pos++] = 2;
  }
  else if (!stricmp("__denormal_likely",sname) || !stricmp("__denormal_unlikely",sname))
  {
    if (parmcnt == 1)
    {
      rec->opcodeType = OPCODETYPE_FUNC1;
      rec->fntype = !stricmp("__denormal_likely",sname) ? FN_DENORMAL_LIKELY : FN_DENORMAL_UNLIKELY;
      return rec;
    }
  }
    
  {
    int chkamt=0;
    functionType *f=nseel_getFunctionByName(ctx,sname,&chkamt);
    if (f) while (chkamt-->=0)
    {
      const int pc_needed=(f->nParams&FUNCTIONTYPE_PARAMETERCOUNTMASK);
      if ((f->nParams&BIF_TAKES_VARPARM_EX)==BIF_TAKES_VARPARM ? (parmcnt >= pc_needed) : (parmcnt == pc_needed))
      {
        rec->fntype = FUNCTYPE_FUNCTIONTYPEREC;
        rec->fn = (void *)f;
        switch (parmcnt)
        {
          case 0:
          case 1: rec->opcodeType = OPCODETYPE_FUNC1; break;
          case 2: rec->opcodeType = OPCODETYPE_FUNC2; break;
          case 3: rec->opcodeType = OPCODETYPE_FUNC3; break;
          default: rec->opcodeType = OPCODETYPE_FUNCX; break;
        }
        return rec;
      }
      if (match_parmcnt_pos < 3) match_parmcnt[match_parmcnt_pos++] = (f->nParams&FUNCTIONTYPE_PARAMETERCOUNTMASK);
      f++;
      if (stricmp(f->name,sname)) break;
    }
  }
  if (ctx->last_error_string[0]) lstrcatn(ctx->last_error_string, ", ", sizeof(ctx->last_error_string));
  if (match_parmcnt[3] >= 0)
  {
    if (match_parmcnt_pos<3) match_parmcnt[match_parmcnt_pos] = match_parmcnt[3];
    match_parmcnt_pos++;
  }

  if (!match_parmcnt_pos)
    snprintf_append(ctx->last_error_string,sizeof(ctx->last_error_string),"'%.30s' undefined",sname);
  else
  {
    int x;
    snprintf_append(ctx->last_error_string,sizeof(ctx->last_error_string),"'%.30s' needs ",sname);
    for (x = 0; x < match_parmcnt_pos; x++)
      snprintf_append(ctx->last_error_string,sizeof(ctx->last_error_string),"%s%d",x==0?"" : x == match_parmcnt_pos-1?" or ":",",match_parmcnt[x]);
    lstrcatn(ctx->last_error_string," parms",sizeof(ctx->last_error_string));
  }
  if (errOut) *errOut = match_parmcnt_pos > 0 ? parmcnt<match_parmcnt[0]?2:(match_parmcnt[0] < 2 ? 4:1) : 0;
  return NULL;
}

opcodeRec *nseel_setCompiledFunctionCallParameters(compileContext *ctx, opcodeRec *fn, opcodeRec *code1, opcodeRec *code2, opcodeRec *code3, opcodeRec *postCode, int *errOut)
{
  opcodeRec *r;
  int np=0,x;
  if (!fn || fn->opcodeType != OPCODETYPE_VARPTR || !fn->relname || !fn->relname[0]) 
  {
    return NULL;
  }
  fn->parms.parms[0] = code1;
  fn->parms.parms[1] = code2;
  fn->parms.parms[2] = code3;

  for (x=0;x<3;x++)
  {
    opcodeRec *prni=fn->parms.parms[x];
    while (prni && np < NSEEL_MAX_EELFUNC_PARAMETERS)
    {
      const int isMP = prni->opcodeType == OPCODETYPE_MOREPARAMS;
      np++;
      if (!isMP) break;
      prni = prni->parms.parms[1];
    }
  }
  r = nseel_resolve_named_symbol(ctx, fn, np<1 ? 1 : np ,errOut);
  if (postCode && r)
  {
    if (code1 && r->opcodeType == OPCODETYPE_FUNC1 && r->fntype == FN_WHILE)
    {
      // change while(x) (postcode) to be 
      // while ((x) ? (postcode;1) : 0);
      
      r->parms.parms[0] = 
        nseel_createIfElse(ctx,r->parms.parms[0],
                               nseel_createSimpleCompiledFunction(ctx,FN_JOIN_STATEMENTS,2,postCode,nseel_createCompiledValue(ctx,1.0f)),
                               NULL); // NULL defaults to 0.0
        
    }
    else
    {
      snprintf_append(ctx->last_error_string,sizeof(ctx->last_error_string),"syntax error following function");
      *errOut = -1;
      return NULL;
    }
  }
  return r;
}


struct eelStringSegmentRec *nseel_createStringSegmentRec(compileContext *ctx, const char *str, int len)
{
  struct eelStringSegmentRec *r = newTmpBlock(ctx,sizeof(struct eelStringSegmentRec));
  if (r)
  {
    r->_next=0;
    r->str_start=str;
    r->str_len = len;
  }
  return r;
}

opcodeRec *nseel_eelMakeOpcodeFromStringSegments(compileContext *ctx, struct eelStringSegmentRec *rec)
{
  if (ctx && ctx->onString)
  {
    return nseel_createCompiledValue(ctx, ctx->onString(ctx->caller_this,rec));
  }

  return NULL;
}

opcodeRec *nseel_createMoreParametersOpcode(compileContext *ctx, opcodeRec *code1, opcodeRec *code2)
{
  opcodeRec *r=code1 && code2 ? newOpCode(ctx,NULL,OPCODETYPE_MOREPARAMS) : NULL;
  if (r)
  {
    r->parms.parms[0] = code1;
    r->parms.parms[1] = code2;
  }
  return r;
}


opcodeRec *nseel_createIfElse(compileContext *ctx, opcodeRec *code1, opcodeRec *code2, opcodeRec *code3)
{
  opcodeRec *r=code1 ? newOpCode(ctx,NULL,OPCODETYPE_FUNC3) : NULL;
  if (r)
  {
    if (!code2) code2 = nseel_createCompiledValue(ctx,0.0);
    if (!code3) code3 = nseel_createCompiledValue(ctx,0.0);
    if (!code2||!code3) return NULL;

    r->fntype = FN_IF_ELSE;
    r->parms.parms[0] = code1;
    r->parms.parms[1] = code2;
    r->parms.parms[2] = code3;
  }
  return r;
}


opcodeRec *nseel_createMemoryAccess(compileContext *ctx, opcodeRec *code1, opcodeRec *code2)
{
  if (code1 && code1->opcodeType == OPCODETYPE_VARPTR && !stricmp(code1->relname,"gmem"))
  {
    return nseel_createSimpleCompiledFunction(ctx, FN_GMEMORY,1,code2?code2:nseel_createCompiledValue(ctx,0.0),0);
  }
  if (code2 && (code2->opcodeType != OPCODETYPE_DIRECTVALUE || code2->parms.dv.directValue != 0.0))
  {
    code1 = nseel_createSimpleCompiledFunction(ctx,FN_ADD,2,code1,code2);
  }
  return nseel_createSimpleCompiledFunction(ctx, FN_MEMORY,1,code1,0);
}

opcodeRec *nseel_createSimpleCompiledFunction(compileContext *ctx, int fn, int np, opcodeRec *code1, opcodeRec *code2)
{
  opcodeRec *r=code1 && (np<2 || code2) ? newOpCode(ctx,NULL,np>=2 ? OPCODETYPE_FUNC2:OPCODETYPE_FUNC1) : NULL;
  if (r)
  {
    r->fntype = fn;
    r->parms.parms[0] = code1;
    r->parms.parms[1] = code2;
    if (fn == FN_JOIN_STATEMENTS)
    {
      r->fn = r; // for joins, fn is temporarily used for tail pointers
      if (code1 && code1->opcodeType == OPCODETYPE_FUNC2 && code1->fntype == fn)
      {
        opcodeRec *t = (opcodeRec *)code1->fn;
        // keep joins in the form of dosomething->morestuff. 
        // in this instance, code1 is previous stuff to do, code2 is new stuff to do
        r->parms.parms[0] = t->parms.parms[1];

        code1->fn = (t->parms.parms[1] = r);
        return code1;
      }
    }
  }
  return r;  
}


// these are bitmasks; on request you can tell what is supported, and compileOpcodes will return one of them
#define RETURNVALUE_IGNORE 0 // ignore return value
#define RETURNVALUE_NORMAL 1 // pointer
#define RETURNVALUE_FPSTACK 2
#define RETURNVALUE_BOOL 4 // P1 is nonzero if true
#define RETURNVALUE_BOOL_REVERSED 8 // P1 is zero if true
#define RETURNVALUE_CACHEABLE 16 // only to be used when (at least) RETURNVALUE_NORMAL is set



static int compileOpcodes(compileContext *ctx, opcodeRec *op, unsigned char *bufOut, int bufOut_len, int *computTable, const namespaceInformation *namespacePathToThis, 
                          int supportedReturnValues, int *rvType, int *fpStackUsage, int *canHaveDenormalOutput);


static unsigned char *compileCodeBlockWithRet(compileContext *ctx, opcodeRec *rec, int *computTableSize, const namespaceInformation *namespacePathToThis, 
                                              int supportedReturnValues, int *rvType, int *fpStackUse, int *canHaveDenormalOutput);

_codeHandleFunctionRec *eel_createFunctionNamespacedInstance(compileContext *ctx, _codeHandleFunctionRec *fr, const char *nameptr)
{
  size_t n;
  _codeHandleFunctionRec *subfr = 
    fr->isCommonFunction ? 
      ctx->isSharedFunctions ? newDataBlock(sizeof(_codeHandleFunctionRec),8) : 
      newCtxDataBlock(sizeof(_codeHandleFunctionRec),8) :  // if common function, but derived version is in non-common context, set ownership to VM rather than us
    newTmpBlock(ctx,sizeof(_codeHandleFunctionRec));

  if (!subfr) return 0;
  // fr points to functionname()'s rec, nameptr to blah.functionname()

  *subfr = *fr;
  n = strlen(nameptr);
  if (n > sizeof(subfr->fname)-1) n=sizeof(subfr->fname)-1;
  memcpy(subfr->fname,nameptr,n);
  subfr->fname[n]=0;

  subfr->next = NULL;
  subfr->startptr=0; // make sure this code gets recompiled (with correct member ptrs) for this instance!
  subfr->startptr_size=-1;

  // subfr->derivedCopies already points to the right place
  fr->derivedCopies = subfr; 
  
  return subfr;

}
static void combineNamespaceFields(char *nm, const namespaceInformation *namespaceInfo, const char *relname, int thisctx) // nm must be NSEEL_MAX_VARIABLE_NAMELEN+1 bytes
{
  const char *prefix = namespaceInfo ? 
                          thisctx<0 ? (thisctx == -1 ? namespaceInfo->namespacePathToThis : NULL) :  (thisctx < MAX_SUB_NAMESPACES ? namespaceInfo->subParmInfo[thisctx] : NULL)
                        : NULL;
  int lfp = 0, lrn=relname ? (int)strlen(relname) : 0;
  if (prefix) while (prefix[lfp] && prefix[lfp] != ':' && lfp < NSEEL_MAX_VARIABLE_NAMELEN) lfp++;
  if (!relname) relname = "";

  while (*relname == '.') // if relname begins with ., then remove a chunk of context from prefix
  {
    relname++;
    while (lfp>0 && prefix[lfp-1] != '.') lfp--;
    if (lfp>0) lfp--;       
  }

  if (lfp > NSEEL_MAX_VARIABLE_NAMELEN-3) lfp=NSEEL_MAX_VARIABLE_NAMELEN-3;
  if (lfp>0) memcpy(nm,prefix,lfp);

  if (lrn > NSEEL_MAX_VARIABLE_NAMELEN - lfp - (lfp>0)) lrn=NSEEL_MAX_VARIABLE_NAMELEN - lfp - (lfp>0);
  if (lrn > 0)
  {
    if (lfp>0) nm[lfp++] = '.';
    memcpy(nm+lfp,relname,lrn);
    lfp+=lrn;
  }
  nm[lfp++]=0;
}


//---------------------------------------------------------------------------------------------------------------
static void *nseel_getBuiltinFunctionAddress(compileContext *ctx, 
      int fntype, void *fn, 
      NSEEL_PPPROC *pProc, void ***replList, 
      void **endP, int *abiInfo, int preferredReturnValues, const EEL_F *hasConstParm1, const EEL_F *hasConstParm2)
{
  const EEL_F *firstConstParm = hasConstParm1 ? hasConstParm1 : hasConstParm2;
  static void *pow_replptrs[4]={&pow,};      

  switch (fntype)
  {
#define RF(x) *endP = nseel_asm_##x##_end; return (void*)nseel_asm_##x


    case FN_MUL_OP:
      *abiInfo=BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(2)|BIF_CLEARDENORMAL;
    RF(mul_op);
    case FN_DIV_OP:
      *abiInfo=BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(2)|BIF_CLEARDENORMAL;
    RF(div_op);
    case FN_OR_OP:
      *abiInfo=BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(2)|BIF_CLEARDENORMAL;
    RF(or_op);
    case FN_XOR_OP:
      *abiInfo=BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(2)|BIF_CLEARDENORMAL;
    RF(xor_op);
    case FN_AND_OP:
      *abiInfo=BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(2)|BIF_CLEARDENORMAL;
    RF(and_op);
    case FN_MOD_OP:
      *abiInfo=BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(2)|BIF_CLEARDENORMAL;
    RF(mod_op);
    case FN_ADD_OP:
      *abiInfo=BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(2)|BIF_CLEARDENORMAL;
    RF(add_op);
    case FN_SUB_OP:
      *abiInfo=BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(2)|BIF_CLEARDENORMAL;
    RF(sub_op);
    case FN_POW_OP:
      *abiInfo=BIF_LASTPARMONSTACK|BIF_CLEARDENORMAL;
      *replList = pow_replptrs;
    RF(2pdds);
    case FN_POW: 
      *abiInfo = BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK;//BIF_FPSTACKUSE(2) might be safe, need to look at pow()'s implementation, but safer bet is to disallow fp stack caching for this expression
      *replList = pow_replptrs;
    RF(2pdd);
    case FN_ADD: 
       *abiInfo = BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK_LAZY|BIF_FPSTACKUSE(2);
        // for x +- non-denormal-constant,  we can set BIF_CLEARDENORMAL
       if (firstConstParm && fabs(*firstConstParm) > DENORMAL_CLEARING_THRESHOLD) *abiInfo |= BIF_CLEARDENORMAL;
    RF(add);
    case FN_SUB: 
       *abiInfo = BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK|BIF_FPSTACKUSE(2);
        // for x +- non-denormal-constant,  we can set BIF_CLEARDENORMAL
       if (firstConstParm && fabs(*firstConstParm) > DENORMAL_CLEARING_THRESHOLD) *abiInfo |= BIF_CLEARDENORMAL;
    RF(sub);
    case FN_MULTIPLY: 
        *abiInfo = BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK_LAZY|BIF_FPSTACKUSE(2); 
         // for x*constant-greater-than-eq-1, we can set BIF_WONTMAKEDENORMAL
        if (firstConstParm && fabs(*firstConstParm) >= 1.0) *abiInfo |= BIF_WONTMAKEDENORMAL;
    RF(mul);
    case FN_DIVIDE: 
        *abiInfo = BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK|BIF_FPSTACKUSE(2); 
        // for x/constant-less-than-eq-1, we can set BIF_WONTMAKEDENORMAL
        if (firstConstParm && fabs(*firstConstParm) <= 1.0) *abiInfo |= BIF_WONTMAKEDENORMAL;
    RF(div);
    case FN_MOD:
      *abiInfo = BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK|BIF_FPSTACKUSE(1)|BIF_CLEARDENORMAL;
    RF(mod);
    case FN_ASSIGN:
      *abiInfo = BIF_FPSTACKUSE(1)|BIF_CLEARDENORMAL;
    RF(assign);
    case FN_AND: *abiInfo = BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK_LAZY|BIF_FPSTACKUSE(2)|BIF_CLEARDENORMAL; RF(and);
    case FN_OR: *abiInfo = BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK_LAZY|BIF_FPSTACKUSE(2)|BIF_CLEARDENORMAL; RF(or);
    case FN_XOR:
      *abiInfo = BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK_LAZY|BIF_FPSTACKUSE(2)|BIF_CLEARDENORMAL;
    RF(xor);
    case FN_SHR:
      *abiInfo = BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK|BIF_FPSTACKUSE(2)|BIF_CLEARDENORMAL;
    RF(shr);
    case FN_SHL:
      *abiInfo = BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK|BIF_FPSTACKUSE(2)|BIF_CLEARDENORMAL;
    RF(shl);
#ifndef EEL_TARGET_PORTABLE
    case FN_NOTNOT: *abiInfo = BIF_LASTPARM_ASBOOL|BIF_RETURNSBOOL|BIF_FPSTACKUSE(1); RF(uplus);
#else
    case FN_NOTNOT: *abiInfo = BIF_LASTPARM_ASBOOL|BIF_RETURNSBOOL|BIF_FPSTACKUSE(1); RF(bnotnot);
#endif
    case FN_UMINUS: *abiInfo = BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_WONTMAKEDENORMAL; RF(uminus);
    case FN_NOT: *abiInfo = BIF_LASTPARM_ASBOOL|BIF_RETURNSBOOL|BIF_FPSTACKUSE(1); RF(bnot);

    case FN_EQ:
      *abiInfo = BIF_TWOPARMSONFPSTACK_LAZY|BIF_RETURNSBOOL|BIF_FPSTACKUSE(2);
    RF(equal);
    case FN_EQ_EXACT:
      *abiInfo=BIF_TWOPARMSONFPSTACK_LAZY|BIF_RETURNSBOOL|BIF_FPSTACKUSE(2);
    RF(equal_exact);
    case FN_NE:
      *abiInfo=BIF_TWOPARMSONFPSTACK_LAZY|BIF_RETURNSBOOL|BIF_FPSTACKUSE(2);
    RF(notequal);
    case FN_NE_EXACT:
      *abiInfo=BIF_TWOPARMSONFPSTACK_LAZY|BIF_RETURNSBOOL|BIF_FPSTACKUSE(2);
    RF(notequal_exact);
    case FN_LOGICAL_AND:
      *abiInfo = BIF_RETURNSBOOL;
    RF(band);
    case FN_LOGICAL_OR:
      *abiInfo = BIF_RETURNSBOOL;
    RF(bor);

#ifdef GLUE_HAS_FXCH
    case FN_GT:
      *abiInfo = BIF_TWOPARMSONFPSTACK|BIF_RETURNSBOOL|BIF_FPSTACKUSE(2);
    RF(above);
    case FN_GTE:
      *abiInfo = BIF_TWOPARMSONFPSTACK|BIF_RETURNSBOOL|BIF_REVERSEFPORDER|BIF_FPSTACKUSE(2);
    RF(beloweq);
    case FN_LT:
      *abiInfo = BIF_TWOPARMSONFPSTACK|BIF_RETURNSBOOL|BIF_REVERSEFPORDER|BIF_FPSTACKUSE(2);
    RF(above);
    case FN_LTE:
      *abiInfo = BIF_TWOPARMSONFPSTACK|BIF_RETURNSBOOL|BIF_FPSTACKUSE(2);
    RF(beloweq);
#else
    case FN_GT:
      *abiInfo = BIF_RETURNSBOOL|BIF_LASTPARMONSTACK;
    RF(above);
    case FN_GTE:
      *abiInfo = BIF_RETURNSBOOL|BIF_LASTPARMONSTACK;
    RF(aboveeq);
    case FN_LT:
      *abiInfo = BIF_RETURNSBOOL|BIF_LASTPARMONSTACK;
    RF(below);
    case FN_LTE:
      *abiInfo = BIF_RETURNSBOOL|BIF_LASTPARMONSTACK;
    RF(beloweq);
#endif


#undef RF
#define RF(x) *endP = _asm_##x##_end; return (void*)_asm_##x

    case FN_MEMORY:
      {
        static void *replptrs[4]={&__NSEEL_RAMAlloc,};      
        *replList = replptrs;
        *abiInfo = BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(1)|BIF_CLEARDENORMAL;
        #ifdef GLUE_MEM_NEEDS_PPROC
          *pProc = NSEEL_PProc_RAM;
        #endif
        RF(megabuf);
      }
    break;
    case FN_GMEMORY:
      {
        static void *replptrs[4]={&__NSEEL_RAMAllocGMEM,};      
        *replList = replptrs;
        *abiInfo=BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(1)|BIF_CLEARDENORMAL;
        *pProc=NSEEL_PProc_GRAM;
        RF(gmegabuf);
      }
    break;
#undef RF

    case FUNCTYPE_FUNCTIONTYPEREC:
      if (fn)
      {
        functionType *p=(functionType *)fn;

        // if prefers fpstack or bool, or ignoring value, then use fp-stack versions
        if ((preferredReturnValues&(RETURNVALUE_BOOL|RETURNVALUE_FPSTACK)) || !preferredReturnValues)
        {
          static functionType min2={ "min",    nseel_asm_min_fp,nseel_asm_min_fp_end,   2|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK_LAZY|BIF_FPSTACKUSE(2)|BIF_WONTMAKEDENORMAL };
          static functionType max2={ "max",    nseel_asm_max_fp,nseel_asm_max_fp_end,   2|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_TWOPARMSONFPSTACK_LAZY|BIF_FPSTACKUSE(2)|BIF_WONTMAKEDENORMAL };

          if (p->afunc == (void*)nseel_asm_min) p = &min2;
          else if (p->afunc == (void*)nseel_asm_max) p = &max2;
        }

        *replList=p->replptrs;
        *pProc=p->pProc;
        *endP = p->func_e;
        *abiInfo = p->nParams & BIF_NPARAMS_MASK;
        if (firstConstParm)
        {
          const char *name=p->name;
          if (!strcmp(name,"min") && *firstConstParm < -1.0e-10) *abiInfo |= BIF_CLEARDENORMAL;
          else if (!strcmp(name,"max") && *firstConstParm > 1.0e-10) *abiInfo |= BIF_CLEARDENORMAL;
        }
        return p->afunc; 
      }
    break;
  }
  
  return 0;
}



static void *nseel_getEELFunctionAddress(compileContext *ctx, 
      opcodeRec *op,
      int *customFuncParmSize, int *customFuncLocalStorageSize,
      EEL_F ***customFuncLocalStorage, int *computTableTop, 
      void **endP, int *isRaw, int wantCodeGenerated,
      const namespaceInformation *namespacePathToThis, int *rvMode, int *fpStackUse, int *canHaveDenormalOutput,
      opcodeRec **ordered_parmptrs, int num_ordered_parmptrs      
      ) // if wantCodeGenerated is false, can return bogus pointers in raw mode
{
  _codeHandleFunctionRec *fn = (_codeHandleFunctionRec*)op->fn;

  namespaceInformation local_namespace={NULL};
  char prefix_buf[NSEEL_MAX_VARIABLE_NAMELEN+1], nm[NSEEL_MAX_FUNCSIG_NAME+1];
  if (!fn) return NULL;

  // op->relname ptr is [whatever.]funcname
  if (fn->parameterAsNamespaceMask || fn->usesNamespaces)
  {
    if (wantCodeGenerated)
    {
      char *p = prefix_buf;
      combineNamespaceFields(nm,namespacePathToThis,op->relname,op->namespaceidx);
      lstrcpyn_safe(prefix_buf,nm,sizeof(prefix_buf));
      local_namespace.namespacePathToThis = prefix_buf;
      // nm is full path of function, prefix_buf will be the path not including function name (unless function name only)
      while (*p) p++;
      while (p >= prefix_buf && *p != '.') p--;
      if (p > prefix_buf) *p=0;
    }
    if (fn->parameterAsNamespaceMask)
    {
      int x;
      for(x=0;x<MAX_SUB_NAMESPACES && x < fn->num_params;x++)
      {
        if (fn->parameterAsNamespaceMask & (((unsigned int)1)<<x))
        {
          if (wantCodeGenerated)
          {
            const char *rn=NULL;
            char tmp[NSEEL_MAX_VARIABLE_NAMELEN+1];
            if (x < num_ordered_parmptrs && ordered_parmptrs[x]) 
            {
              if (ordered_parmptrs[x]->opcodeType == OPCODETYPE_VARPTR) 
              {
                rn=ordered_parmptrs[x]->relname;
              }
              else if (ordered_parmptrs[x]->opcodeType == OPCODETYPE_VALUE_FROM_NAMESPACENAME)
              {
                const char *p=ordered_parmptrs[x]->relname;
                if (*p == '#') p++;
                combineNamespaceFields(tmp,namespacePathToThis,p,ordered_parmptrs[x]->namespaceidx);
                rn = tmp;
              }
            }
          
            if (!rn) 
            {
              // todo: figure out how to give correct line number/offset (ugh)
              snprintf(ctx->last_error_string,sizeof(ctx->last_error_string),"parameter %d to %.120s() must be namespace",x+1,fn->fname);
              return NULL;
            }

            lstrcatn(nm,":",sizeof(nm));

            local_namespace.subParmInfo[x] = nm+strlen(nm);
            lstrcatn(nm,rn,sizeof(nm));
          }         
          ordered_parmptrs[x] = NULL; // prevent caller from bothering generating parameters
        }
      }
    }
    if (wantCodeGenerated)
    {
      _codeHandleFunctionRec *fr = fn;
      // find namespace-adjusted function (if generating code, otherwise assume size is the same)
      fn = 0; // if this gets re-set, it will be the new function
      while (fr && !fn)
      {
        if (!stricmp(fr->fname,nm)) fn = fr;
        fr=fr->derivedCopies;
      }
      if (!fn) // generate copy of function
      {
        fn = eel_createFunctionNamespacedInstance(ctx,(_codeHandleFunctionRec*)op->fn,nm);
      }
    }
  }
  if (!fn) return NULL;

  if (!fn->startptr && fn->opcodes && fn->startptr_size != 0)
  {
    int sz = fn->startptr_size;

    if (sz < 0) 
    {
      fn->tmpspace_req=0;
      fn->rvMode = RETURNVALUE_IGNORE;
      fn->canHaveDenormalOutput=0;

      sz = compileOpcodes(ctx,fn->opcodes,NULL,128*1024*1024,&fn->tmpspace_req,
          wantCodeGenerated ? &local_namespace : NULL,RETURNVALUE_NORMAL|RETURNVALUE_FPSTACK,
          &fn->rvMode,&fn->fpStackUsage,&fn->canHaveDenormalOutput);
      if (sz<0) return NULL;

      fn->startptr_size = sz;
    }

    if (!wantCodeGenerated)
    {
      // don't compile anything for now, just give stats
      if (computTableTop) *computTableTop += fn->tmpspace_req;
      *customFuncParmSize = fn->num_params;
      *customFuncLocalStorage = fn->localstorage;
      *customFuncLocalStorageSize = fn->localstorage_size;
      *rvMode = fn->rvMode;
      *fpStackUse = fn->fpStackUsage;
      if (canHaveDenormalOutput) *canHaveDenormalOutput=fn->canHaveDenormalOutput;

      if (sz <= NSEEL_MAX_FUNCTION_SIZE_FOR_INLINE && !(ctx->optimizeDisableFlags&OPTFLAG_NO_INLINEFUNC))
      {
        *isRaw = 1;
        *endP = ((char *)1) + sz;
        return (char *)1;
      }
      *endP = (void*)nseel_asm_fcall_end;
      return (void*)nseel_asm_fcall;
    }

    if (sz <= NSEEL_MAX_FUNCTION_SIZE_FOR_INLINE && !(ctx->optimizeDisableFlags&OPTFLAG_NO_INLINEFUNC))
    {
      void *p=newTmpBlock(ctx,sz);
      fn->tmpspace_req=0;
      if (p)
      {
        fn->canHaveDenormalOutput=0;
        if (fn->isCommonFunction) ctx->isGeneratingCommonFunction++;
        sz=compileOpcodes(ctx,fn->opcodes,(unsigned char*)p,sz,&fn->tmpspace_req,&local_namespace,RETURNVALUE_NORMAL|RETURNVALUE_FPSTACK,&fn->rvMode,&fn->fpStackUsage,&fn->canHaveDenormalOutput);
        if (fn->isCommonFunction) ctx->isGeneratingCommonFunction--;
        // recompile function with native context pointers
        if (sz>0)
        {
          fn->startptr_size=sz;
          fn->startptr=p;
        }
      }
    }
    else
    {
      unsigned char *codeCall;
      fn->tmpspace_req=0;
      fn->fpStackUsage=0;
      fn->canHaveDenormalOutput=0;
      if (fn->isCommonFunction) ctx->isGeneratingCommonFunction++;
      codeCall=compileCodeBlockWithRet(ctx,fn->opcodes,&fn->tmpspace_req,&local_namespace,RETURNVALUE_NORMAL|RETURNVALUE_FPSTACK,&fn->rvMode,&fn->fpStackUsage,&fn->canHaveDenormalOutput);
      if (fn->isCommonFunction) ctx->isGeneratingCommonFunction--;
      if (codeCall)
      {
        void *f=GLUE_realAddress(nseel_asm_fcall,nseel_asm_fcall_end,&sz);
        fn->startptr = newTmpBlock(ctx,sz);
        if (fn->startptr)
        {
          memcpy(fn->startptr,f,sz);
          EEL_GLUE_set_immediate(fn->startptr,(INT_PTR)codeCall);
          fn->startptr_size = sz;
        }
      }
    }
  }

  if (fn->startptr)
  {
    if (computTableTop) *computTableTop += fn->tmpspace_req;
    *customFuncParmSize = fn->num_params;
    *customFuncLocalStorage = fn->localstorage;
    *customFuncLocalStorageSize = fn->localstorage_size;
    *rvMode = fn->rvMode;
    *fpStackUse = fn->fpStackUsage;
    if (canHaveDenormalOutput) *canHaveDenormalOutput= fn->canHaveDenormalOutput;
    *endP = (char*)fn->startptr + fn->startptr_size;
    *isRaw=1;
    return fn->startptr;
  }
  
  return 0;
}



// returns true if does something (other than calculating and throwing away a value)
static char optimizeOpcodes(compileContext *ctx, opcodeRec *op, int needsResult)
{
  opcodeRec *lastJoinOp=NULL;
  char retv, retv_parm[3], joined_retv=0;
  while (op && op->opcodeType == OPCODETYPE_FUNC2 && op->fntype == FN_JOIN_STATEMENTS)
  {
    if (!optimizeOpcodes(ctx,op->parms.parms[0], 0) || OPCODE_IS_TRIVIAL(op->parms.parms[0]))
    {
      // direct value, can skip ourselves
      memcpy(op,op->parms.parms[1],sizeof(*op));
    }
    else
    {
      joined_retv |= 1;
      lastJoinOp = op;
      op = op->parms.parms[1];
    }
  }
goto start_over;

#define RESTART_DIRECTVALUE(X) { op->parms.dv.directValue = (X); goto start_over_directvalue; }
start_over_directvalue:
  op->opcodeType = OPCODETYPE_DIRECTVALUE;
  op->parms.dv.valuePtr=NULL;
  
start_over: // when an opcode changed substantially in optimization, goto here to reprocess it

  retv = retv_parm[0]=retv_parm[1]=retv_parm[2]=0;

  if (!op || // should never really happen
      OPCODE_IS_TRIVIAL(op) || // should happen often (vars)
      op->opcodeType < 0 || op->opcodeType >= OPCODETYPE_INVALID // should never happen (assert would be appropriate heh)
      ) return joined_retv;
  
  if (!needsResult)
  {
    if (op->fntype == FUNCTYPE_EELFUNC) 
    {
      needsResult=1; // assume eel functions are non-const for now
    }
    else if (op->fntype == FUNCTYPE_FUNCTIONTYPEREC)
    {
      functionType  *pfn = (functionType *)op->fn;
      if (!pfn || !(pfn->nParams&NSEEL_NPARAMS_FLAG_CONST)) needsResult=1;
    }
    else if (op->fntype >= FN_NONCONST_BEGIN && op->fntype < FUNCTYPE_SIMPLEMAX)
    {
      needsResult=1;
    }
  }

  if (op->opcodeType>=OPCODETYPE_FUNC2) retv_parm[1] = optimizeOpcodes(ctx,op->parms.parms[1], needsResult);
  if (op->opcodeType>=OPCODETYPE_FUNC3) retv_parm[2] = optimizeOpcodes(ctx,op->parms.parms[2], needsResult);

  retv_parm[0] = optimizeOpcodes(ctx,op->parms.parms[0], needsResult || 
      (FNPTR_HAS_CONDITIONAL_EXEC(op) && (retv_parm[1] || retv_parm[2] || op->opcodeType <= OPCODETYPE_FUNC1)) );

  if (op->opcodeType != OPCODETYPE_MOREPARAMS)
  {
    if (op->fntype >= 0 && op->fntype < FUNCTYPE_SIMPLEMAX)
    {
      if (op->opcodeType == OPCODETYPE_FUNC1) // within FUNCTYPE_SIMPLE
      {
        if (op->parms.parms[0]->opcodeType == OPCODETYPE_DIRECTVALUE)
        {
          switch (op->fntype)
          {
            case FN_NOTNOT: RESTART_DIRECTVALUE(fabs(op->parms.parms[0]->parms.dv.directValue)>=NSEEL_CLOSEFACTOR ? 1.0 : 0.0);
            case FN_NOT:    RESTART_DIRECTVALUE(fabs(op->parms.parms[0]->parms.dv.directValue)>=NSEEL_CLOSEFACTOR ? 0.0 : 1.0);
            case FN_UMINUS: RESTART_DIRECTVALUE(- op->parms.parms[0]->parms.dv.directValue);
          }
        }
        else if (op->fntype == FN_NOT || op->fntype == FN_NOTNOT)
        {
          if (op->parms.parms[0]->opcodeType == OPCODETYPE_FUNC1)
          {
            switch (op->parms.parms[0]->fntype)
            {
              case FN_UMINUS:
              case FN_NOTNOT: // ignore any NOTNOTs UMINUS or UPLUS, they would have no effect anyway
                op->parms.parms[0] = op->parms.parms[0]->parms.parms[0];
              goto start_over;

              case FN_NOT:
                op->fntype = op->fntype==FN_NOT ? FN_NOTNOT : FN_NOT; // switch between FN_NOT and FN_NOTNOT
                op->parms.parms[0] = op->parms.parms[0]->parms.parms[0];
              goto start_over;
            }
          }
          else if (op->parms.parms[0]->opcodeType == OPCODETYPE_FUNC2)
          {
            int repl_type = -1;
            switch (op->parms.parms[0]->fntype)
            {
              case FN_EQ: repl_type = FN_NE; break;
              case FN_NE: repl_type = FN_EQ; break;
              case FN_EQ_EXACT: repl_type = FN_NE_EXACT; break;
              case FN_NE_EXACT: repl_type = FN_EQ_EXACT; break;
              case FN_LT:  repl_type = FN_GTE; break;
              case FN_LTE: repl_type = FN_GT; break;
              case FN_GT:  repl_type = FN_LTE; break;
              case FN_GTE: repl_type = FN_LT; break;
            }
            if (repl_type != -1)
            {
              const int oldtype = op->fntype;
              memcpy(op,op->parms.parms[0],sizeof(*op));
              if (oldtype == FN_NOT) op->fntype = repl_type;
              goto start_over;
            }
          }
        }
      }
      else if (op->opcodeType == OPCODETYPE_FUNC2)  // within FUNCTYPE_SIMPLE
      {
        const int dv0 = op->parms.parms[0]->opcodeType == OPCODETYPE_DIRECTVALUE;
        const int dv1 = op->parms.parms[1]->opcodeType == OPCODETYPE_DIRECTVALUE;
        if (dv0 && dv1)
        {
          int reval = -1;
          switch (op->fntype)
          {
            case FN_MOD:
              {
                int a = (int) op->parms.parms[1]->parms.dv.directValue;
                if (a) 
                {
                  a = (int) op->parms.parms[0]->parms.dv.directValue % a;
                  if (a<0) a=-a;
                }
                RESTART_DIRECTVALUE((EEL_F)a);
              }
            break;
            case FN_SHL:      RESTART_DIRECTVALUE(((int)op->parms.parms[0]->parms.dv.directValue) << ((int)op->parms.parms[1]->parms.dv.directValue));
            case FN_SHR:      RESTART_DIRECTVALUE(((int)op->parms.parms[0]->parms.dv.directValue) >> ((int)op->parms.parms[1]->parms.dv.directValue));
            case FN_POW:      RESTART_DIRECTVALUE(pow(op->parms.parms[0]->parms.dv.directValue, op->parms.parms[1]->parms.dv.directValue));
            case FN_DIVIDE:   RESTART_DIRECTVALUE(op->parms.parms[0]->parms.dv.directValue / op->parms.parms[1]->parms.dv.directValue);
            case FN_MULTIPLY: RESTART_DIRECTVALUE(op->parms.parms[0]->parms.dv.directValue * op->parms.parms[1]->parms.dv.directValue);

            case FN_ADD:      RESTART_DIRECTVALUE(op->parms.parms[0]->parms.dv.directValue + op->parms.parms[1]->parms.dv.directValue);
            case FN_SUB:      RESTART_DIRECTVALUE(op->parms.parms[0]->parms.dv.directValue - op->parms.parms[1]->parms.dv.directValue);
            case FN_AND:      RESTART_DIRECTVALUE((double) (((WDL_INT64)op->parms.parms[0]->parms.dv.directValue) & ((WDL_INT64)op->parms.parms[1]->parms.dv.directValue)));
            case FN_OR:       RESTART_DIRECTVALUE((double) (((WDL_INT64)op->parms.parms[0]->parms.dv.directValue) | ((WDL_INT64)op->parms.parms[1]->parms.dv.directValue)));
            case FN_XOR:      RESTART_DIRECTVALUE((double) (((WDL_INT64)op->parms.parms[0]->parms.dv.directValue) ^ ((WDL_INT64)op->parms.parms[1]->parms.dv.directValue)));

            case FN_EQ:       reval = fabs(op->parms.parms[0]->parms.dv.directValue - op->parms.parms[1]->parms.dv.directValue) < NSEEL_CLOSEFACTOR; break;
            case FN_NE:       reval = fabs(op->parms.parms[0]->parms.dv.directValue - op->parms.parms[1]->parms.dv.directValue) >= NSEEL_CLOSEFACTOR; break;
            case FN_EQ_EXACT: reval = op->parms.parms[0]->parms.dv.directValue == op->parms.parms[1]->parms.dv.directValue; break;
            case FN_NE_EXACT: reval = op->parms.parms[0]->parms.dv.directValue != op->parms.parms[1]->parms.dv.directValue; break;
            case FN_LT:       reval = op->parms.parms[0]->parms.dv.directValue < op->parms.parms[1]->parms.dv.directValue; break;
            case FN_LTE:      reval = op->parms.parms[0]->parms.dv.directValue <= op->parms.parms[1]->parms.dv.directValue; break;
            case FN_GT:       reval = op->parms.parms[0]->parms.dv.directValue > op->parms.parms[1]->parms.dv.directValue; break;
            case FN_GTE:      reval = op->parms.parms[0]->parms.dv.directValue >= op->parms.parms[1]->parms.dv.directValue; break;
            case FN_LOGICAL_AND: reval = fabs(op->parms.parms[0]->parms.dv.directValue) >= NSEEL_CLOSEFACTOR && fabs(op->parms.parms[1]->parms.dv.directValue) >= NSEEL_CLOSEFACTOR; break;
            case FN_LOGICAL_OR:  reval = fabs(op->parms.parms[0]->parms.dv.directValue) >= NSEEL_CLOSEFACTOR || fabs(op->parms.parms[1]->parms.dv.directValue) >= NSEEL_CLOSEFACTOR; break;
          }
          
          if (reval >= 0) RESTART_DIRECTVALUE((EEL_F) reval);
        }
        else if (dv0 || dv1)
        {
          double dvalue = op->parms.parms[!dv0]->parms.dv.directValue;
          switch (op->fntype)
          {
            case FN_OR:
            case FN_XOR:
              if (!(WDL_INT64)dvalue)
              {
                // replace with or0
                static functionType fr={"or0",nseel_asm_or0, nseel_asm_or0_end, 1|NSEEL_NPARAMS_FLAG_CONST|BIF_LASTPARMONSTACK|BIF_RETURNSONSTACK|BIF_CLEARDENORMAL, {0}, NULL};

                op->opcodeType = OPCODETYPE_FUNC1;
                op->fntype = FUNCTYPE_FUNCTIONTYPEREC;
                op->fn = &fr;
                if (dv0) op->parms.parms[0] = op->parms.parms[1];
                goto start_over;
              }
            break;
            case FN_SUB:
              if (dv0) 
              {
                if (dvalue == 0.0)
                {
                  op->opcodeType = OPCODETYPE_FUNC1;
                  op->fntype = FN_UMINUS;
                  op->parms.parms[0] = op->parms.parms[1];
                  goto start_over;
                }
                break;
              }
              // fall through, if dv1 we can remove +0.0

            case FN_ADD:
              if (dvalue == 0.0) 
              {
                memcpy(op,op->parms.parms[!!dv0],sizeof(*op));
                goto start_over;
              }
            break;
            case FN_AND:
              if ((WDL_INT64)dvalue) break;
              dvalue = 0.0; // treat x&0 as x*0, which optimizes to 0
            
              // fall through
            case FN_MULTIPLY:
              if (dvalue == 0.0) // remove multiply by 0.0 (using 0.0 direct value as replacement), unless the nonzero side did something
              {
                if (!retv_parm[!!dv0]) 
                {
                  memcpy(op,op->parms.parms[!dv0],sizeof(*op)); // set to 0 if other action wouldn't do anything
                  goto start_over;
                }
                else
                {
                  // this is 0.0 * oldexpressionthatmustbeprocessed or oldexpressionthatmustbeprocessed*0.0
                  op->fntype = FN_JOIN_STATEMENTS;

                  if (dv0) // 0.0*oldexpression, reverse the order so that 0 is returned
                  {
                    // set to (oldexpression;0)
                    opcodeRec *tmp = op->parms.parms[1];
                    op->parms.parms[1] = op->parms.parms[0];
                    op->parms.parms[0] = tmp;
                  }
                  goto start_over;
                }
              }
              else if (dvalue == 1.0) // remove multiply by 1.0 (using non-1.0 value as replacement)
              {
                memcpy(op,op->parms.parms[!!dv0],sizeof(*op));
                goto start_over;
              }
            break;
            case FN_POW:
              if (dv1)
              {
                // x^0 = 1
                if (fabs(dvalue) < 1e-30)
                {
                  RESTART_DIRECTVALUE(1.0);
                }
                // x^1 = x
                if (fabs(dvalue-1.0) < 1e-30)
                {
                  memcpy(op,op->parms.parms[0],sizeof(*op));
                  goto start_over;
                }
              }
              else if (dv0)
              {
                // pow(constant, x) = exp((x) * ln(constant)), if constant>0
                // opcodeRec *parm0 = op->parms.parms[0];
                if (dvalue > 0.0)
                {
                  static functionType expcpy={ "exp",    nseel_asm_1pdd,nseel_asm_1pdd_end,   1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK, {&exp}, };

                  // 1^x = 1
                  if (fabs(dvalue-1.0) < 1e-30)
                  {
                    RESTART_DIRECTVALUE(1.0);
                  }

                  dvalue=log(dvalue);

                  if (fabs(dvalue-1.0) < 1e-9)
                  {
                    // caller wanted e^x
                    op->parms.parms[0]=op->parms.parms[1];
                  }
                  else
                  {
                    // it would be nice to replace 10^x with exp(log(10)*x) or 2^x with exp(log(2),x), but 
                    // doing so breaks rounding. we could maybe only allow 10^x, which is used for dB conversion,
                    // but for now we should just force the programmer do it exp(log(10)*x) themselves.
                    break;

                    /* 
                    parm0->opcodeType = OPCODETYPE_FUNC2;
                    parm0->fntype = FN_MULTIPLY;
                    parm0->parms.parms[0] = nseel_createCompiledValue(ctx,dvalue);
                    parm0->parms.parms[1] = op->parms.parms[1];
                    */
                  }

                  op->opcodeType = OPCODETYPE_FUNC1;
                  op->fntype = FUNCTYPE_FUNCTIONTYPEREC;
                  op->fn = &expcpy;
                  goto start_over;
                }
              }
            break;
            case FN_MOD:
              if (dv1)
              {
                const int a = (int) dvalue;
                if (!a) 
                {
                  RESTART_DIRECTVALUE(0.0);
                }
              }
            break;
            case FN_DIVIDE:
              if (dv1)
              {
                if (dvalue == 1.0)  // remove divide by 1.0  (using non-1.0 value as replacement)
                {
                  memcpy(op,op->parms.parms[!!dv0],sizeof(*op));
                  goto start_over;
                }
                else
                {
                  // change to a multiply
                  if (dvalue == 0.0)
                  {
                    op->fntype = FN_MULTIPLY;
                    goto start_over;
                  }
                  else
                  {
                    double d = 1.0/dvalue;

                    WDL_DenormalDoubleAccess *p = (WDL_DenormalDoubleAccess*)&d;
                    // allow conversion to multiply if reciprocal is exact
                    // we could also just look to see if the last few digits of the mantissa were 0, which would probably be good
                    // enough, but if the user really wants it they should do * (1/x) instead to force precalculation of reciprocal.
                    if (!p->w.lw && !(p->w.hw & 0xfffff)) 
                    {
                      op->fntype = FN_MULTIPLY;
                      op->parms.parms[1]->parms.dv.directValue = d;
                      op->parms.parms[1]->parms.dv.valuePtr=NULL;
                      goto start_over;
                    }
                  }
                }
              }
              else if (dvalue == 0.0)
              {
                if (!retv_parm[!!dv0])
                {
                  // if 0/x set to always 0.
                  // this is 0.0 / (oldexpression that can be eliminated)
                  memcpy(op,op->parms.parms[!dv0],sizeof(*op)); // set to 0 if other action wouldn't do anything
                }
                else
                {
                  opcodeRec *tmp;
                  // this is 0.0 / oldexpressionthatmustbeprocessed
                  op->fntype = FN_JOIN_STATEMENTS;
                  tmp = op->parms.parms[1];
                  op->parms.parms[1] = op->parms.parms[0];
                  op->parms.parms[0] = tmp;
                  // set to (oldexpression;0)
                }
                goto start_over;
              }
            break;
            case FN_EQ:
              if (dvalue == 0.0)
              {
                // convert x == 0.0 to !x
                op->opcodeType=OPCODETYPE_FUNC1;
                op->fntype = FN_NOT;
                if (dv0) op->parms.parms[0]=op->parms.parms[1];
                goto start_over;
              }
            break;
            case FN_NE:
              if (dvalue == 0.0)
              {
                // convert x != 0.0 to !!
                op->opcodeType=OPCODETYPE_FUNC1;
                op->fntype = FN_NOTNOT;
                if (dv0) op->parms.parms[0]=op->parms.parms[1];
                goto start_over;
              }
            break;
            case FN_LOGICAL_AND:
              if (dv0)
              {
                // dvalue && expr
                if (fabs(dvalue) < NSEEL_CLOSEFACTOR)
                {
                  // 0 && expr, replace with 0
                  RESTART_DIRECTVALUE(0.0);
                }
                else
                {
                  // 1 && expr, replace with 0 != expr
                  op->fntype = FN_NE;
                  op->parms.parms[0]->parms.dv.valuePtr=NULL;
                  op->parms.parms[0]->parms.dv.directValue = 0.0;
                }
              }
              else
              {
                // expr && dvalue
                if (fabs(dvalue) < NSEEL_CLOSEFACTOR)
                {
                  // expr && 0
                  if (!retv_parm[0]) 
                  {
                    // expr has no consequence, drop it
                    RESTART_DIRECTVALUE(0.0);
                  }
                  else
                  {
                    // replace with (expr; 0)
                    op->fntype = FN_JOIN_STATEMENTS;
                    op->parms.parms[1]->parms.dv.valuePtr=NULL;
                    op->parms.parms[1]->parms.dv.directValue = 0.0;
                  }
                }
                else
                {
                  // expr && 1, replace with expr != 0
                  op->fntype = FN_NE;
                  op->parms.parms[1]->parms.dv.valuePtr=NULL;
                  op->parms.parms[1]->parms.dv.directValue = 0.0;
                }
              }
            goto start_over;
            case FN_LOGICAL_OR:
              if (dv0)
              {
                // dvalue || expr
                if (fabs(dvalue) >= NSEEL_CLOSEFACTOR)
                {
                  // 1 || expr, replace with 1
                  RESTART_DIRECTVALUE(1.0);
                }
                else
                {
                  // 0 || expr, replace with 0 != expr
                  op->fntype = FN_NE;
                  op->parms.parms[0]->parms.dv.valuePtr=NULL;
                  op->parms.parms[0]->parms.dv.directValue = 0.0;
                }
              }
              else
              {
                // expr || dvalue
                if (fabs(dvalue) >= NSEEL_CLOSEFACTOR)
                {
                  // expr || 1
                  if (!retv_parm[0]) 
                  {
                    // expr has no consequence, drop it and return 1
                    RESTART_DIRECTVALUE(1.0);
                  }
                  else
                  {
                    // replace with (expr; 1)
                    op->fntype = FN_JOIN_STATEMENTS;
                    op->parms.parms[1]->parms.dv.valuePtr=NULL;
                    op->parms.parms[1]->parms.dv.directValue = 1.0;
                  }
                }
                else
                {
                  // expr || 0, replace with expr != 0
                  op->fntype = FN_NE;
                  op->parms.parms[1]->parms.dv.valuePtr=NULL;
                  op->parms.parms[1]->parms.dv.directValue = 0.0;
                }
              }
            goto start_over;
          }
        } // dv0 || dv1

        // general optimization of two parameters
        switch (op->fntype)
        {
          case FN_MULTIPLY:
          {
            opcodeRec *first_parm = op->parms.parms[0],*second_parm = op->parms.parms[1];

            if (second_parm->opcodeType == first_parm->opcodeType) 
            {
              switch(first_parm->opcodeType)
              {
                case OPCODETYPE_VALUE_FROM_NAMESPACENAME:
                  if (first_parm->namespaceidx != second_parm->namespaceidx) break;
                  // fall through
                case OPCODETYPE_VARPTR:
                  if (first_parm->relname && second_parm->relname && !stricmp(second_parm->relname,first_parm->relname)) second_parm=NULL;
                break;
                case OPCODETYPE_VARPTRPTR:
                  if (first_parm->parms.dv.valuePtr && first_parm->parms.dv.valuePtr==second_parm->parms.dv.valuePtr) second_parm=NULL;
                break;

              }
              if (!second_parm) // switch from x*x to sqr(x)
              {
                static functionType sqrcpy={ "sqr",    nseel_asm_sqr,nseel_asm_sqr_end,   1|NSEEL_NPARAMS_FLAG_CONST|BIF_RETURNSONSTACK|BIF_LASTPARMONSTACK|BIF_FPSTACKUSE(1) };
                op->opcodeType = OPCODETYPE_FUNC1;
                op->fntype = FUNCTYPE_FUNCTIONTYPEREC;
                op->fn = &sqrcpy;
                goto start_over;
              }
            }
          }
          break;
          case FN_POW:
            {
              opcodeRec *first_parm = op->parms.parms[0];
              if (first_parm->opcodeType == op->opcodeType && first_parm->fntype == FN_POW)
              {
                // since first_parm is a pow too, we can multiply the exponents.

                // set our base to be the base of the inner pow
                op->parms.parms[0] = first_parm->parms.parms[0];

                // make the old extra pow be a multiply of the exponents
                first_parm->fntype = FN_MULTIPLY;
                first_parm->parms.parms[0] = op->parms.parms[1];

                // put that as the exponent
                op->parms.parms[1] = first_parm;

                goto start_over;
              }
            }
          break;
          case FN_LOGICAL_AND:
          case FN_LOGICAL_OR:
            if (op->parms.parms[0]->fntype == FN_NOTNOT)
            {
              // remove notnot, unnecessary for input to &&/|| operators
              op->parms.parms[0] = op->parms.parms[0]->parms.parms[0];
              goto start_over;
            }
            if (op->parms.parms[1]->fntype == FN_NOTNOT)
            {
              // remove notnot, unnecessary for input to &&/|| operators
              op->parms.parms[1] = op->parms.parms[1]->parms.parms[0];
              goto start_over;
            }        
          break;
        }
      }
      else if (op->opcodeType==OPCODETYPE_FUNC3)  // within FUNCTYPE_SIMPLE
      {
        if (op->fntype == FN_IF_ELSE)
        {
          if (op->parms.parms[0]->opcodeType == OPCODETYPE_DIRECTVALUE)
          {
            int s = fabs(op->parms.parms[0]->parms.dv.directValue) >= NSEEL_CLOSEFACTOR;
            memcpy(op,op->parms.parms[s ? 1 : 2],sizeof(opcodeRec));
            goto start_over;
          }
          if (op->parms.parms[0]->opcodeType == OPCODETYPE_FUNC1)
          {
            if (op->parms.parms[0]->fntype == FN_NOTNOT)
            {
              // remove notnot, unnecessary for input to ? operator
              op->parms.parms[0] = op->parms.parms[0]->parms.parms[0];
              goto start_over;
            }
          }
        }
      }
      if (op->fntype >= FN_NONCONST_BEGIN && op->fntype < FUNCTYPE_SIMPLEMAX) retv|=1;

      // FUNCTYPE_SIMPLE
    }   
    else if (op->fntype == FUNCTYPE_FUNCTIONTYPEREC && op->fn)
    {

      /*
      probably worth doing reduction on:
      _divop (constant change to multiply)
      _and
      _or
      abs

      maybe:
      min
      max


      also, optimize should (recursively or maybe iteratively?) search transitive functions (mul/div) for more constant reduction possibilities


      */


      functionType  *pfn = (functionType *)op->fn;

      if (!(pfn->nParams&NSEEL_NPARAMS_FLAG_CONST)) retv|=1;

      if (op->opcodeType==OPCODETYPE_FUNC1) // within FUNCTYPE_FUNCTIONTYPEREC
      {
        if (op->parms.parms[0]->opcodeType == OPCODETYPE_DIRECTVALUE)
        {
          int suc=1;
          EEL_F v = op->parms.parms[0]->parms.dv.directValue;
  #define DOF(x) if (!strcmp(pfn->name,#x)) v = x(v); else
  #define DOF2(x,y) if (!strcmp(pfn->name,#x)) v = x(y); else
          DOF(sin)
          DOF(cos)
          DOF(tan)
          DOF(asin)
          DOF(acos)
          DOF(atan)
          DOF2(sqrt, fabs(v))
          DOF(exp)
          DOF(log)
          DOF(log10)
          /* else */ suc=0;
  #undef DOF
  #undef DOF2
          if (suc)
          {
            RESTART_DIRECTVALUE(v);
          }


        }
      }
      else if (op->opcodeType==OPCODETYPE_FUNC2)  // within FUNCTYPE_FUNCTIONTYPEREC
      {
        const int dv0=op->parms.parms[0]->opcodeType == OPCODETYPE_DIRECTVALUE;
        const int dv1=op->parms.parms[1]->opcodeType == OPCODETYPE_DIRECTVALUE;
        if (dv0 && dv1)
        {
          if (!strcmp(pfn->name,"atan2")) 
          {
            RESTART_DIRECTVALUE(atan2(op->parms.parms[0]->parms.dv.directValue, op->parms.parms[1]->parms.dv.directValue));
          }
        }
      }
      // FUNCTYPE_FUNCTIONTYPEREC
    }
    else
    {
      // unknown or eel func, assume non-const
      retv |= 1;
    }
  }

  // if we need results, or our function has effects itself, then finish
  if (retv || needsResult)
  {
    return retv || joined_retv || retv_parm[0] || retv_parm[1] || retv_parm[2];
  }

  // we don't need results here, and our function is const, which means we can remove it
  {
    int cnt=0, idx1=0, idx2=0, x;
    for (x=0;x<3;x++) if (retv_parm[x]) { if (!cnt++) idx1=x; else idx2=x; }

    if (!cnt) // none of the parameters do anything, remove this opcode
    {
      if (lastJoinOp)
      {
        // replace previous join with its first linked opcode, removing this opcode completely
        memcpy(lastJoinOp,lastJoinOp->parms.parms[0],sizeof(*lastJoinOp));
      }
      else if (op->opcodeType != OPCODETYPE_DIRECTVALUE)
      {
        // allow caller to easily detect this as trivial and remove it
        op->opcodeType = OPCODETYPE_DIRECTVALUE;
        op->parms.dv.valuePtr=NULL;
        op->parms.dv.directValue=0.0;
      }
      // return joined_retv below
    }
    else
    {
      // if parameters are non-const, and we're a conditional, preserve our function
      if (FNPTR_HAS_CONDITIONAL_EXEC(op)) return 1;

      // otherwise, condense into either the non-const statement, or a join
      if (cnt==1)
      {
        memcpy(op,op->parms.parms[idx1],sizeof(*op));
      }
      else if (cnt == 2)
      {
        op->opcodeType = OPCODETYPE_FUNC2;
        op->fntype = FN_JOIN_STATEMENTS;
        op->fn = op;
        op->parms.parms[0] = op->parms.parms[idx1];
        op->parms.parms[1] = op->parms.parms[idx2];
        op->parms.parms[2] = NULL;
      }
      else
      {
        // todo need to create a new opcodeRec here, for now just leave as is 
        // (non-conditional const 3 parameter functions are rare anyway)
      }
      return 1;
    }
  }
  return joined_retv;
}


static int generateValueToReg(compileContext *ctx, opcodeRec *op, unsigned char *bufOut, int whichReg, const namespaceInformation *functionPrefix, int allowCache)
{
  EEL_F *b=NULL;
  if (op->opcodeType==OPCODETYPE_VALUE_FROM_NAMESPACENAME)
  {
    char nm[NSEEL_MAX_VARIABLE_NAMELEN+1];
    const char *p = op->relname;
    combineNamespaceFields(nm,functionPrefix,p+(*p == '#'),op->namespaceidx);
    if (!nm[0]) return -1;
    if (*p == '#') 
    {
      if (ctx->isGeneratingCommonFunction)
        b = newCtxDataBlock(sizeof(EEL_F),sizeof(EEL_F));
      else
        b = newDataBlock(sizeof(EEL_F),sizeof(EEL_F));

      if (!b) RET_MINUS1_FAIL("error creating storage for str")

      if (!ctx->onNamedString) return -1; // should never happen, will not generate OPCODETYPE_VALUE_FROM_NAMESPACENAME with # prefix if !onNamedString

      *b = ctx->onNamedString(ctx->caller_this,nm);
    }
    else
    {
      b = nseel_int_register_var(ctx,nm,0,NULL);
      if (!b) RET_MINUS1_FAIL("error registering var")
    }
  }
  else
  {
    if (op->opcodeType != OPCODETYPE_DIRECTVALUE) allowCache=0;

    if (op->opcodeType==OPCODETYPE_DIRECTVALUE_TEMPSTRING && ctx->onNamedString)
    {
      op->parms.dv.directValue = ctx->onNamedString(ctx->caller_this,"");
      op->parms.dv.valuePtr = NULL;
    }

    b=op->parms.dv.valuePtr;
    if (!b && op->opcodeType == OPCODETYPE_VARPTR && op->relname && op->relname[0]) 
    {
      op->parms.dv.valuePtr = b = nseel_int_register_var(ctx,op->relname,0,NULL);
    }

    if (b && op->opcodeType == OPCODETYPE_VARPTRPTR) b = *(EEL_F **)b;
    if (!b && allowCache)
    {
      int n=50; // only scan last X items
      opcodeRec *r = ctx->directValueCache;
      while (r && n--)
      {
        if (r->parms.dv.directValue == op->parms.dv.directValue && (b=r->parms.dv.valuePtr)) break;
        r=(opcodeRec*)r->fn;
      }
    }
    if (!b)
    {
      ctx->l_stats[3]++;
      if (ctx->isGeneratingCommonFunction)
        b = newCtxDataBlock(sizeof(EEL_F),sizeof(EEL_F));
      else
        b = newDataBlock(sizeof(EEL_F),sizeof(EEL_F));

      if (!b) RET_MINUS1_FAIL("error allocating data block")

      if (op->opcodeType != OPCODETYPE_VARPTRPTR) op->parms.dv.valuePtr = b;
      #if EEL_F_SIZE == 8
        *b = denormal_filter_double2(op->parms.dv.directValue);
      #else
        *b = denormal_filter_float2(op->parms.dv.directValue);
      #endif

      if (allowCache)
      {
        op->fn = ctx->directValueCache;
        ctx->directValueCache = op;
      }
    }
  }

  GLUE_MOV_PX_DIRECTVALUE_GEN(bufOut,(INT_PTR)b,whichReg);
  return GLUE_MOV_PX_DIRECTVALUE_SIZE;
}


unsigned char *compileCodeBlockWithRet(compileContext *ctx, opcodeRec *rec, int *computTableSize, const namespaceInformation *namespacePathToThis, 
                                       int supportedReturnValues, int *rvType, int *fpStackUsage, int *canHaveDenormalOutput)
{
  unsigned char *p, *newblock2;
  // generate code call
  int funcsz=compileOpcodes(ctx,rec,NULL,1024*1024*128,NULL,namespacePathToThis,supportedReturnValues, rvType,fpStackUsage, NULL);
  if (funcsz<0) return NULL;
 
  p = newblock2 = newCodeBlock(funcsz+ sizeof(GLUE_RET)+GLUE_FUNC_ENTER_SIZE+GLUE_FUNC_LEAVE_SIZE,32);
  if (!newblock2) return NULL;
  #if GLUE_FUNC_ENTER_SIZE > 0
    memcpy(p,&GLUE_FUNC_ENTER,GLUE_FUNC_ENTER_SIZE); 
    p += GLUE_FUNC_ENTER_SIZE;
  #endif
  *fpStackUsage=0;
  funcsz=compileOpcodes(ctx,rec,p, funcsz, computTableSize,namespacePathToThis,supportedReturnValues, rvType,fpStackUsage, canHaveDenormalOutput);         
  if (funcsz<0) return NULL;
  p+=funcsz;

  #if GLUE_FUNC_LEAVE_SIZE > 0
    memcpy(p,&GLUE_FUNC_LEAVE,GLUE_FUNC_LEAVE_SIZE); 
    p+=GLUE_FUNC_LEAVE_SIZE;
  #endif
  memcpy(p,&GLUE_RET,sizeof(GLUE_RET)); p+=sizeof(GLUE_RET);
#if defined(__arm__) || defined(__aarch64__)
  __clear_cache(newblock2,p);
#endif
  
  ctx->l_stats[2]+=funcsz+2;
  return newblock2;
}      

static int compileNativeFunctionCall(compileContext *ctx, opcodeRec *op, unsigned char *bufOut, int bufOut_len, int *computTableSize, const namespaceInformation *namespacePathToThis, 
                                     int *rvMode, int *fpStackUsage, int preferredReturnValues, int *canHaveDenormalOutput)
{
  // builtin function generation
  int func_size=0;
  int cfunc_abiinfo=0;
  int local_fpstack_use=0; // how many items we have pushed onto the fp stack
  int parm_size=0;
  int restore_stack_amt=0;

  void *func_e=NULL;
  NSEEL_PPPROC preProc=0;
  void **repl=NULL;

  int n_params= 1 + op->opcodeType - OPCODETYPE_FUNC1;

  const int parm0_dv = op->parms.parms[0]->opcodeType == OPCODETYPE_DIRECTVALUE;
  const int parm1_dv = n_params > 1 && op->parms.parms[1]->opcodeType == OPCODETYPE_DIRECTVALUE;

  void *func = nseel_getBuiltinFunctionAddress(ctx, op->fntype, op->fn, &preProc,&repl,&func_e,&cfunc_abiinfo,preferredReturnValues, 
       parm0_dv ? &op->parms.parms[0]->parms.dv.directValue : NULL,
       parm1_dv ? &op->parms.parms[1]->parms.dv.directValue : NULL
       );

  if (!func) RET_MINUS1_FAIL("error getting funcaddr")

  *fpStackUsage=BIF_GETFPSTACKUSE(cfunc_abiinfo);
  *rvMode = RETURNVALUE_NORMAL;

  if (cfunc_abiinfo & BIF_TAKES_VARPARM)
  {
#if defined(__arm__) || (defined (_M_ARM) && _M_ARM  == 7)
    const int max_params=16384; // arm uses up to two instructions, should be good for at leaast 64k (16384*4)
#elif defined(__ppc__)
    const int max_params=4096; // 32kb max offset addressing for stack, so 4096*4 = 16384, should be safe
#elif defined(__aarch64__)
    const int max_params=32768; 
#else
    const int max_params=32768; // sanity check, the stack is free to grow on x86/x86-64
#endif
    int x;
    // this mode is less efficient in that it creates a list of pointers on the stack to pass to the function
    // but it is more flexible and works for >3 parameters.
    if (op->opcodeType == OPCODETYPE_FUNCX)
    {
      n_params=0;
      for (x=0;x<3;x++)
      {
        opcodeRec *prni=op->parms.parms[x];
        while (prni)
        {
          const int isMP = prni->opcodeType == OPCODETYPE_MOREPARAMS;
          n_params++;
          if (!isMP||n_params>=max_params) break;
          prni = prni->parms.parms[1];
        }
      }
    }

    restore_stack_amt = (sizeof(void *) * n_params + 15)&~15;

    if (restore_stack_amt)
    {
      int offs = restore_stack_amt;
      while (offs > 0)
      {
        int amt = offs;
        if (amt > 4096) amt=4096;

        if (bufOut_len < parm_size+GLUE_MOVE_STACK_SIZE) RET_MINUS1_FAIL("insufficient size for varparm")
        if (bufOut) GLUE_MOVE_STACK(bufOut+parm_size, - amt);
        parm_size += GLUE_MOVE_STACK_SIZE;
        offs -= amt;

        if (offs>0) // make sure this page is in memory
        {
          if (bufOut_len < parm_size+GLUE_STORE_P1_TO_STACK_AT_OFFS_SIZE(0)) 
            RET_MINUS1_FAIL("insufficient size for varparm stackchk")
          if (bufOut) GLUE_STORE_P1_TO_STACK_AT_OFFS(bufOut+parm_size,0);
          parm_size += GLUE_STORE_P1_TO_STACK_AT_OFFS_SIZE(0);
        }
      }
    }

    if (op->opcodeType == OPCODETYPE_FUNCX)
    {     
      n_params=0;
      for (x=0;x<3;x++)
      {
        opcodeRec *prni=op->parms.parms[x];
        while (prni)
        {
          const int isMP = prni->opcodeType == OPCODETYPE_MOREPARAMS;
          opcodeRec *r = isMP ? prni->parms.parms[0] : prni;
          if (r)
          {
            int canHaveDenorm=0;
            int rvt=RETURNVALUE_NORMAL;
            int subfpstackuse=0, use_offs;
                
            int lsz = compileOpcodes(ctx,r,bufOut ? bufOut + parm_size : NULL,bufOut_len - parm_size, computTableSize, namespacePathToThis, rvt,&rvt, &subfpstackuse, &canHaveDenorm);
            if (canHaveDenorm && canHaveDenormalOutput) *canHaveDenormalOutput = 1;

            if (lsz<0) RET_MINUS1_FAIL("call coc for varparmX failed")
            if (rvt != RETURNVALUE_NORMAL) RET_MINUS1_FAIL("call coc for varparmX gave bad type back");

            parm_size += lsz;            
            use_offs = n_params*(int) sizeof(void *);

            if (bufOut_len < parm_size+GLUE_STORE_P1_TO_STACK_AT_OFFS_SIZE(use_offs)) 
              RET_MINUS1_FAIL("call coc for varparmX size");
            if (bufOut) GLUE_STORE_P1_TO_STACK_AT_OFFS(bufOut + parm_size, use_offs);
            parm_size+=GLUE_STORE_P1_TO_STACK_AT_OFFS_SIZE(use_offs);

            if (subfpstackuse+local_fpstack_use > *fpStackUsage) *fpStackUsage = subfpstackuse+local_fpstack_use;
          }
          else RET_MINUS1_FAIL("zero parameter varparmX")

          n_params++;

          if (!isMP||n_params>=max_params) break;
          prni = prni->parms.parms[1];
        }
      }
    }
    else for (x=0;x<n_params;x++)
    {
      opcodeRec *r = op->parms.parms[x];
      if (r)
      {
        int canHaveDenorm=0;
        int subfpstackuse=0;
        int rvt=RETURNVALUE_NORMAL;
        int use_offs;
               
        int lsz = compileOpcodes(ctx,r,bufOut ? bufOut + parm_size : NULL,bufOut_len - parm_size, computTableSize, namespacePathToThis, rvt,&rvt, &subfpstackuse, &canHaveDenorm);
        if (canHaveDenorm && canHaveDenormalOutput) *canHaveDenormalOutput = 1;

        if (lsz<0) RET_MINUS1_FAIL("call coc for varparm123 failed")
        if (rvt != RETURNVALUE_NORMAL) RET_MINUS1_FAIL("call coc for varparm123 gave bad type back");

        parm_size += lsz;

        use_offs = x*(int)sizeof(void *);
        if (bufOut_len < parm_size+GLUE_STORE_P1_TO_STACK_AT_OFFS_SIZE(use_offs)) 
          RET_MINUS1_FAIL("call coc for varparm123 size");
        if (bufOut) GLUE_STORE_P1_TO_STACK_AT_OFFS(bufOut + parm_size, use_offs);
        parm_size+=GLUE_STORE_P1_TO_STACK_AT_OFFS_SIZE(use_offs);

        if (subfpstackuse+local_fpstack_use > *fpStackUsage) *fpStackUsage = subfpstackuse+local_fpstack_use;
      }
      else RET_MINUS1_FAIL("zero parameter for varparm123");
    }

    if (bufOut_len < parm_size+GLUE_MOV_PX_DIRECTVALUE_SIZE+GLUE_MOVE_PX_STACKPTR_SIZE) RET_MINUS1_FAIL("insufficient size for varparm p1")
    if (bufOut) GLUE_MOV_PX_DIRECTVALUE_GEN(bufOut+parm_size, (INT_PTR)n_params,1);
    parm_size+=GLUE_MOV_PX_DIRECTVALUE_SIZE;
    if (bufOut) GLUE_MOVE_PX_STACKPTR_GEN(bufOut+parm_size, 0);
    parm_size+=GLUE_MOVE_PX_STACKPTR_SIZE;
    
  }
  else // not varparm
  {
    int pn;
  #ifdef GLUE_HAS_FXCH
    int need_fxch=0;
  #endif
    int last_nt_parm=-1, last_nt_parm_type=-1;
    
    if (op->opcodeType == OPCODETYPE_FUNCX)
    {
      // this is not yet supported (calling conventions will need to be sorted, among other things)
      RET_MINUS1_FAIL("funcx for native functions requires BIF_TAKES_VARPARM or BIF_TAKES_VARPARM_EX")
    }

    if (parm0_dv) 
    {
      if (func == nseel_asm_stack_pop)
      {
        func = GLUE_realAddress(nseel_asm_stack_pop_fast,nseel_asm_stack_pop_fast_end,&func_size);
        if (!func || bufOut_len < func_size) RET_MINUS1_FAIL(func?"failed on popfast size":"failed on popfast addr")

        if (bufOut) 
        {
          memcpy(bufOut,func,func_size);
          NSEEL_PProc_Stack(bufOut,func_size,ctx);
        }
        return func_size;            
      }
      else if (func == nseel_asm_stack_peek)
      {
        int f = (int) op->parms.parms[0]->parms.dv.directValue;
        if (!f)
        {
          func = GLUE_realAddress(nseel_asm_stack_peek_top,nseel_asm_stack_peek_top_end,&func_size);
          if (!func || bufOut_len < func_size) RET_MINUS1_FAIL(func?"failed on peek size":"failed on peek addr")

          if (bufOut) 
          {
            memcpy(bufOut,func,func_size);
            NSEEL_PProc_Stack_PeekTop(bufOut,func_size,ctx);
          }
          return func_size;
        }
        else
        {
          func = GLUE_realAddress(nseel_asm_stack_peek_int,nseel_asm_stack_peek_int_end,&func_size);
          if (!func || bufOut_len < func_size) RET_MINUS1_FAIL(func?"failed on peekint size":"failed on peekint addr")

          if (bufOut)
          {
            memcpy(bufOut,func,func_size);
            NSEEL_PProc_Stack_PeekInt(bufOut,func_size,ctx,f*sizeof(EEL_F));
          }
          return func_size;
        }
      }
    }
    // end of built-in function specific special casing


    // first pass, calculate any non-trivial parameters
    for (pn=0; pn < n_params; pn++)
    { 
      if (!OPCODE_IS_TRIVIAL(op->parms.parms[pn]))
      {
        int canHaveDenorm=0;
        int subfpstackuse=0;
        int lsz=0; 
        int rvt=RETURNVALUE_NORMAL;
        int may_need_fppush=-1;
        if (last_nt_parm>=0)
        {
          if (last_nt_parm_type==RETURNVALUE_FPSTACK)
          {          
            may_need_fppush= parm_size;
          }
          else
          {
            // push last result
            if (bufOut_len < parm_size + (int)sizeof(GLUE_PUSH_P1)) RET_MINUS1_FAIL("failed on size, pushp1")
            if (bufOut) memcpy(bufOut + parm_size, &GLUE_PUSH_P1, sizeof(GLUE_PUSH_P1));
            parm_size += sizeof(GLUE_PUSH_P1);
          }
        }         

        if (func == nseel_asm_bnot) rvt=RETURNVALUE_BOOL_REVERSED|RETURNVALUE_BOOL;
        else if (pn == n_params - 1)
        {
          if (cfunc_abiinfo&BIF_LASTPARMONSTACK) rvt=RETURNVALUE_FPSTACK;
          else if (cfunc_abiinfo&BIF_LASTPARM_ASBOOL) rvt=RETURNVALUE_BOOL;
          else if (func == nseel_asm_assign) rvt=RETURNVALUE_FPSTACK|RETURNVALUE_NORMAL;
        }
        else if (pn == n_params -2 && (cfunc_abiinfo&BIF_SECONDLASTPARMST))
        {
          rvt=RETURNVALUE_FPSTACK;
        }

        lsz = compileOpcodes(ctx,op->parms.parms[pn],bufOut ? bufOut + parm_size : NULL,bufOut_len - parm_size, computTableSize, namespacePathToThis, rvt,&rvt, &subfpstackuse, &canHaveDenorm);

        if (lsz<0) RET_MINUS1_FAIL("call coc failed")

        if (func == nseel_asm_bnot && rvt==RETURNVALUE_BOOL_REVERSED)
        {
          // remove bnot, compileOpcodes() used fptobool_rev
#ifndef EEL_TARGET_PORTABLE
          func = nseel_asm_uplus;
          func_e = nseel_asm_uplus_end;
#else
          func = nseel_asm_bnotnot;
          func_e = nseel_asm_bnotnot_end;
#endif
          rvt = RETURNVALUE_BOOL;
        }

        if (canHaveDenorm && canHaveDenormalOutput) *canHaveDenormalOutput = 1;

        parm_size += lsz;            

        if (may_need_fppush>=0)
        {
          if (local_fpstack_use+subfpstackuse >= (GLUE_MAX_FPSTACK_SIZE-1) || (ctx->optimizeDisableFlags&OPTFLAG_NO_FPSTACK))
          {
            if (bufOut_len < parm_size + (int)sizeof(GLUE_POP_FPSTACK_TOSTACK)) 
              RET_MINUS1_FAIL("failed on size, popfpstacktostack")

            if (bufOut) 
            {
              memmove(bufOut + may_need_fppush + sizeof(GLUE_POP_FPSTACK_TOSTACK), bufOut + may_need_fppush, parm_size - may_need_fppush);
              memcpy(bufOut + may_need_fppush, &GLUE_POP_FPSTACK_TOSTACK, sizeof(GLUE_POP_FPSTACK_TOSTACK));

            }
            parm_size += sizeof(GLUE_POP_FPSTACK_TOSTACK);
          }
          else
          {
            local_fpstack_use++;
          }
        }

        if (subfpstackuse+local_fpstack_use > *fpStackUsage) *fpStackUsage = subfpstackuse+local_fpstack_use;

        last_nt_parm = pn;
        last_nt_parm_type = rvt;

        if (pn == n_params - 1 && func == nseel_asm_assign)
        {
          if (!(ctx->optimizeDisableFlags & OPTFLAG_FULL_DENORMAL_CHECKS) && 
              (!canHaveDenorm || (ctx->optimizeDisableFlags & OPTFLAG_NO_DENORMAL_CHECKS)))
          {
            if (rvt == RETURNVALUE_FPSTACK)
            {
              cfunc_abiinfo |= BIF_LASTPARMONSTACK;
              func = nseel_asm_assign_fast_fromfp;
              func_e = nseel_asm_assign_fast_fromfp_end;
            }
            else
            {
              func = nseel_asm_assign_fast;
              func_e = nseel_asm_assign_fast_end;
            }
          }
          else
          {
            if (rvt == RETURNVALUE_FPSTACK)
            {
              cfunc_abiinfo |= BIF_LASTPARMONSTACK;
              func = nseel_asm_assign_fromfp;
              func_e = nseel_asm_assign_fromfp_end;
            }
          }
        
        }
      }
    }

    pn = last_nt_parm;
  
    if (pn >= 0) // if the last thing executed doesn't go to the last parameter, move it there
    {
      if ((cfunc_abiinfo&BIF_SECONDLASTPARMST) && pn == n_params-2)
      {
        // do nothing, things are in the right place
      }
      else if (pn != n_params-1)
      {
        // generate mov p1->pX
        if (bufOut_len < parm_size + GLUE_SET_PX_FROM_P1_SIZE) RET_MINUS1_FAIL("size, pxfromp1")
        if (bufOut) GLUE_SET_PX_FROM_P1(bufOut + parm_size,n_params - 1 - pn);
        parm_size += GLUE_SET_PX_FROM_P1_SIZE;
      }
    }

    // pop any pushed parameters
    while (--pn >= 0)
    { 
      if (!OPCODE_IS_TRIVIAL(op->parms.parms[pn]))
      {
        if ((cfunc_abiinfo&BIF_SECONDLASTPARMST) && pn == n_params-2)
        {
          if (!local_fpstack_use)
          {
            if (bufOut_len < parm_size + (int)sizeof(GLUE_POP_STACK_TO_FPSTACK)) RET_MINUS1_FAIL("size, popstacktofpstack 2")
            if (bufOut) memcpy(bufOut+parm_size,GLUE_POP_STACK_TO_FPSTACK,sizeof(GLUE_POP_STACK_TO_FPSTACK));
            parm_size += sizeof(GLUE_POP_STACK_TO_FPSTACK);
            #ifdef GLUE_HAS_FXCH
              need_fxch = 1;
            #endif
          }
          else
          {
            local_fpstack_use--;
          }
        }
        else
        {
          if (bufOut_len < parm_size + GLUE_POP_PX_SIZE) RET_MINUS1_FAIL("size, poppx")
          if (bufOut) GLUE_POP_PX(bufOut + parm_size,n_params - 1 - pn);
          parm_size += GLUE_POP_PX_SIZE;
        }
      }
    }

    // finally, set trivial pointers
    for (pn=0; pn < n_params; pn++)
    { 
      if (OPCODE_IS_TRIVIAL(op->parms.parms[pn]))
      {
        if (pn == n_params-2 && (cfunc_abiinfo&(BIF_SECONDLASTPARMST)))  // second to last parameter
        {
          int a = compileOpcodes(ctx,op->parms.parms[pn],bufOut ? bufOut+parm_size : NULL,bufOut_len - parm_size,computTableSize,namespacePathToThis,
                                  RETURNVALUE_FPSTACK,NULL,NULL,canHaveDenormalOutput);
          if (a<0) RET_MINUS1_FAIL("coc call here 2")
          parm_size+=a;
          #ifdef GLUE_HAS_FXCH
            need_fxch = 1;
          #endif
        }
        else if (pn == n_params-1)  // last parameter, but we should call compileOpcodes to get it in the right format (compileOpcodes can optimize that process if it needs to)
        {
          int rvt=0, a;
          int wantFpStack = func == nseel_asm_assign;
  #ifdef GLUE_PREFER_NONFP_DV_ASSIGNS // x86-64, and maybe others, prefer to avoid the fp stack for a simple copy
          if (wantFpStack &&
              (op->parms.parms[pn]->opcodeType != OPCODETYPE_DIRECTVALUE ||
              (op->parms.parms[pn]->parms.dv.directValue != 1.0 && op->parms.parms[pn]->parms.dv.directValue != 0.0)))
          {
            wantFpStack=-1; // cacheable but non-FP stack
          }
  #endif
          a = compileOpcodes(ctx,op->parms.parms[pn],bufOut ? bufOut+parm_size : NULL,bufOut_len - parm_size,computTableSize,namespacePathToThis,
            func == nseel_asm_bnot ? (RETURNVALUE_BOOL_REVERSED|RETURNVALUE_BOOL) :
              (cfunc_abiinfo & BIF_LASTPARMONSTACK) ? RETURNVALUE_FPSTACK : 
              (cfunc_abiinfo & BIF_LASTPARM_ASBOOL) ? RETURNVALUE_BOOL : 
              wantFpStack < 0 ? (RETURNVALUE_CACHEABLE|RETURNVALUE_NORMAL) : 
              wantFpStack ? (RETURNVALUE_FPSTACK|RETURNVALUE_NORMAL) : 
              RETURNVALUE_NORMAL,       
            &rvt, NULL,canHaveDenormalOutput);
           
          if (a<0) RET_MINUS1_FAIL("coc call here 3")

          if (func == nseel_asm_bnot && rvt == RETURNVALUE_BOOL_REVERSED)
          {
            // remove bnot, compileOpcodes() used fptobool_rev
#ifndef EEL_TARGET_PORTABLE
            func = nseel_asm_uplus;
            func_e = nseel_asm_uplus_end;
#else
            func = nseel_asm_bnotnot;
            func_e = nseel_asm_bnotnot_end;
#endif
            rvt = RETURNVALUE_BOOL;
          }

          parm_size+=a;
          #ifdef GLUE_HAS_FXCH
            need_fxch = 0;
          #endif

          if (func == nseel_asm_assign)
          {
            if (rvt == RETURNVALUE_FPSTACK)
            {           
              if (!(ctx->optimizeDisableFlags & OPTFLAG_FULL_DENORMAL_CHECKS))
              {
                func = nseel_asm_assign_fast_fromfp;
                func_e = nseel_asm_assign_fast_fromfp_end;
              }
              else
              {
                func = nseel_asm_assign_fromfp;
                func_e = nseel_asm_assign_fromfp_end;
              }
            }
            else if (!(ctx->optimizeDisableFlags & OPTFLAG_FULL_DENORMAL_CHECKS))
            {
               // assigning a value (from a variable or other non-computer), can use a fast assign (no denormal/result checking)
              func = nseel_asm_assign_fast;
              func_e = nseel_asm_assign_fast_end;
            }
          }
        }
        else
        {
          if (bufOut_len < parm_size + GLUE_MOV_PX_DIRECTVALUE_SIZE) RET_MINUS1_FAIL("size, pxdvsz")
          if (bufOut) 
          {
            if (generateValueToReg(ctx,op->parms.parms[pn],bufOut + parm_size,n_params - 1 - pn,namespacePathToThis, 0/*nocaching, function gets pointer*/)<0) RET_MINUS1_FAIL("gvtr")
          }
          parm_size += GLUE_MOV_PX_DIRECTVALUE_SIZE;
        }
      }
    }

  #ifdef GLUE_HAS_FXCH
    if ((cfunc_abiinfo&(BIF_SECONDLASTPARMST)) && !(cfunc_abiinfo&(BIF_LAZYPARMORDERING))&&
        ((!!need_fxch)^!!(cfunc_abiinfo&BIF_REVERSEFPORDER)) 
        )
    {
      // emit fxch
      if (bufOut_len < sizeof(GLUE_FXCH)) RET_MINUS1_FAIL("len,fxch")
      if (bufOut) 
      { 
        memcpy(bufOut+parm_size,GLUE_FXCH,sizeof(GLUE_FXCH));
      }
      parm_size+=sizeof(GLUE_FXCH);
    }
  #endif
  
    if (!*canHaveDenormalOutput)
    {
      // if add_op or sub_op, and constant non-denormal input, safe to omit denormal checks
      if (func == (void*)nseel_asm_add_op && parm1_dv && fabs(op->parms.parms[1]->parms.dv.directValue) >= DENORMAL_CLEARING_THRESHOLD)
      {
        func = nseel_asm_add_op_fast;
        func_e = nseel_asm_add_op_fast_end;
      }
      else if (func == (void*)nseel_asm_sub_op && parm1_dv && fabs(op->parms.parms[1]->parms.dv.directValue) >= DENORMAL_CLEARING_THRESHOLD)
      {
        func = nseel_asm_sub_op_fast;
        func_e = nseel_asm_sub_op_fast_end;
      }
      // or if mul/div by a fixed value of >= or <= 1.0
      else if (func == (void *)nseel_asm_mul_op && parm1_dv && fabs(op->parms.parms[1]->parms.dv.directValue) >= 1.0)
      {
        func = nseel_asm_mul_op_fast;
        func_e = nseel_asm_mul_op_fast_end;
      }
      else if (func == (void *)nseel_asm_div_op && parm1_dv && fabs(op->parms.parms[1]->parms.dv.directValue) <= 1.0)
      {
        func = nseel_asm_div_op_fast;
        func_e = nseel_asm_div_op_fast_end;
      }
    }
  } // not varparm

  if (cfunc_abiinfo & (BIF_CLEARDENORMAL | BIF_RETURNSBOOL) ) *canHaveDenormalOutput=0;
  else if (!(cfunc_abiinfo & BIF_WONTMAKEDENORMAL)) *canHaveDenormalOutput=1;

  func = GLUE_realAddress(func,func_e,&func_size);
  if (!func) RET_MINUS1_FAIL("failrealladdrfunc")
                   
  if (bufOut_len < parm_size + func_size) RET_MINUS1_FAIL("funcsz")

  if (bufOut)
  {
    unsigned char *p=bufOut + parm_size;
    memcpy(p, func, func_size);
    if (preProc) p=preProc(p,func_size,ctx);
    if (repl)
    {
      if (repl[0]) p=EEL_GLUE_set_immediate(p,(INT_PTR)repl[0]);
      if (repl[1]) p=EEL_GLUE_set_immediate(p,(INT_PTR)repl[1]);
      if (repl[2]) p=EEL_GLUE_set_immediate(p,(INT_PTR)repl[2]);
      if (repl[3]) p=EEL_GLUE_set_immediate(p,(INT_PTR)repl[3]);
    }
  }

  if (restore_stack_amt)
  {
    int rem = restore_stack_amt;
    while (rem > 0)
    {
      int amt = rem;
      if (amt > 4096) amt=4096;
      rem -= amt;

      if (bufOut_len < parm_size + func_size + GLUE_MOVE_STACK_SIZE) RET_MINUS1_FAIL("insufficient size for varparm")
      if (bufOut) GLUE_MOVE_STACK(bufOut + parm_size + func_size, amt);
      parm_size += GLUE_MOVE_STACK_SIZE;
    }
  }

  if (cfunc_abiinfo&BIF_RETURNSONSTACK) *rvMode = RETURNVALUE_FPSTACK;
  else if (cfunc_abiinfo&BIF_RETURNSBOOL) *rvMode=RETURNVALUE_BOOL;

  return parm_size + func_size;
}

static int compileEelFunctionCall(compileContext *ctx, opcodeRec *op, unsigned char *bufOut, int bufOut_len, int *computTableSize, const namespaceInformation *namespacePathToThis, 
                                  int *rvMode, int *fpStackUse, int *canHaveDenormalOutput)
{
  int func_size=0, parm_size=0;
  int pn;
  int last_nt_parm=-1,last_nt_parm_mode=0;
  void *func_e=NULL;
  int n_params;
  opcodeRec *parmptrs[NSEEL_MAX_EELFUNC_PARAMETERS];
  int cfp_numparams=-1;
  int cfp_statesize=0;
  EEL_F **cfp_ptrs=NULL;
  int func_raw=0;
  int do_parms;
  int x;

  void *func;

  for (x=0; x < 3; x ++) parmptrs[x] = op->parms.parms[x];

  if (op->opcodeType == OPCODETYPE_FUNCX)
  {
    n_params=0;
    for (x=0;x<3;x++)
    {
      opcodeRec *prni=op->parms.parms[x];
      while (prni && n_params < NSEEL_MAX_EELFUNC_PARAMETERS)
      {
        const int isMP = prni->opcodeType == OPCODETYPE_MOREPARAMS;
        parmptrs[n_params++] = isMP ? prni->parms.parms[0] : prni;
        if (!isMP) break;
        prni = prni->parms.parms[1];
      }
    }
  }
  else 
  {
    n_params = 1 + op->opcodeType - OPCODETYPE_FUNC1;
  }
          
  *fpStackUse = 0;
  func = nseel_getEELFunctionAddress(ctx, op,
                                      &cfp_numparams,&cfp_statesize,&cfp_ptrs, 
                                      computTableSize, 
                                      &func_e, &func_raw,                                              
                                      !!bufOut,namespacePathToThis,rvMode,fpStackUse,canHaveDenormalOutput, parmptrs, n_params);

  if (func_raw) func_size = (int) ((char*)func_e  - (char*)func);
  else if (func) func = GLUE_realAddress(func,func_e,&func_size);
  
  if (!func) RET_MINUS1_FAIL("eelfuncaddr")

  *fpStackUse += 1;


  if (cfp_numparams>0 && n_params != cfp_numparams)
  {
    RET_MINUS1_FAIL("eelfuncnp")
  }

  // user defined function
  do_parms = cfp_numparams>0 && cfp_ptrs && cfp_statesize>0;

  // if function local/parameter state is zero, we need to allocate storage for it
  if (cfp_statesize>0 && cfp_ptrs && !cfp_ptrs[0])
  {
    EEL_F *pstate = newDataBlock(sizeof(EEL_F)*cfp_statesize,8);
    if (!pstate) RET_MINUS1_FAIL("eelfuncdb")

    for (pn=0;pn<cfp_statesize;pn++)
    {
      pstate[pn]=0;
      cfp_ptrs[pn] = pstate + pn;
    }
  }


  // first process parameters that are non-trivial
  for (pn=0; pn < n_params; pn++)
  { 
    int needDenorm=0;
    int lsz,sUse=0;                      
    
    if (!parmptrs[pn] || OPCODE_IS_TRIVIAL(parmptrs[pn])) continue; // skip and process after

    if (last_nt_parm >= 0 && do_parms)
    {
      if (last_nt_parm_mode == RETURNVALUE_FPSTACK)
      {
        if (bufOut_len < parm_size + (int)sizeof(GLUE_POP_FPSTACK_TOSTACK)) RET_MINUS1_FAIL("eelfunc_size popfpstacktostack")
        if (bufOut) memcpy(bufOut + parm_size,GLUE_POP_FPSTACK_TOSTACK,sizeof(GLUE_POP_FPSTACK_TOSTACK));
        parm_size+=sizeof(GLUE_POP_FPSTACK_TOSTACK);
      }
      else
      {
        if (bufOut_len < parm_size + (int)sizeof(GLUE_PUSH_P1PTR_AS_VALUE)) RET_MINUS1_FAIL("eelfunc_size pushp1ptrasval")
    
        // push
        if (bufOut) memcpy(bufOut + parm_size,&GLUE_PUSH_P1PTR_AS_VALUE,sizeof(GLUE_PUSH_P1PTR_AS_VALUE));
        parm_size+=sizeof(GLUE_PUSH_P1PTR_AS_VALUE);
      }
    }

    last_nt_parm_mode=0;
    lsz = compileOpcodes(ctx,parmptrs[pn],bufOut ? bufOut + parm_size : NULL,bufOut_len - parm_size, computTableSize, namespacePathToThis,
      do_parms ? (RETURNVALUE_FPSTACK|RETURNVALUE_NORMAL) : RETURNVALUE_IGNORE,&last_nt_parm_mode,&sUse, &needDenorm);

    // todo: if needDenorm, denorm convert when copying parameter

    if (lsz<0) RET_MINUS1_FAIL("eelfunc, coc fail")

    if (last_nt_parm_mode == RETURNVALUE_FPSTACK) sUse++;
    if (sUse > *fpStackUse) *fpStackUse=sUse;
    parm_size += lsz;

    last_nt_parm = pn;
  }
  // pop non-trivial results into place
  if (last_nt_parm >=0 && do_parms)
  {
    while (--pn >= 0)
    { 
      if (!parmptrs[pn] || OPCODE_IS_TRIVIAL(parmptrs[pn])) continue; // skip and process after
      if (pn == last_nt_parm)
      {
        if (last_nt_parm_mode == RETURNVALUE_FPSTACK)
        {
          // pop to memory directly
          const int cpsize = GLUE_POP_FPSTACK_TO_PTR(NULL,NULL);
          if (bufOut_len < parm_size + cpsize) RET_MINUS1_FAIL("eelfunc size popfpstacktoptr")

          if (bufOut) GLUE_POP_FPSTACK_TO_PTR((unsigned char *)bufOut + parm_size,cfp_ptrs[pn]);
          parm_size += cpsize;
        }
        else
        {
          // copy direct p1ptr to mem
          const int cpsize = GLUE_COPY_VALUE_AT_P1_TO_PTR(NULL,NULL);
          if (bufOut_len < parm_size + cpsize) RET_MINUS1_FAIL("eelfunc size copyvalueatp1toptr")

          if (bufOut) GLUE_COPY_VALUE_AT_P1_TO_PTR((unsigned char *)bufOut + parm_size,cfp_ptrs[pn]);
          parm_size += cpsize;
        }
      }
      else
      {
        const int popsize =  GLUE_POP_VALUE_TO_ADDR(NULL,NULL);
        if (bufOut_len < parm_size + popsize) RET_MINUS1_FAIL("eelfunc size pop value to addr")

        if (bufOut) GLUE_POP_VALUE_TO_ADDR((unsigned char *)bufOut + parm_size,cfp_ptrs[pn]);
        parm_size+=popsize;

      }
    }
  }

  // finally, set any trivial parameters
  if (do_parms)
  {
    const int cpsize = GLUE_MOV_PX_DIRECTVALUE_SIZE + GLUE_COPY_VALUE_AT_P1_TO_PTR(NULL,NULL);
    for (pn=0; pn < n_params; pn++)
    { 
      if (!parmptrs[pn] || !OPCODE_IS_TRIVIAL(parmptrs[pn])) continue; // set trivial values, we already set nontrivials

      if (bufOut_len < parm_size + cpsize) RET_MINUS1_FAIL("eelfunc size trivial set")

      if (bufOut) 
      {
        if (generateValueToReg(ctx,parmptrs[pn],bufOut + parm_size,0,namespacePathToThis, 1)<0) RET_MINUS1_FAIL("eelfunc gvr fail")
        GLUE_COPY_VALUE_AT_P1_TO_PTR(bufOut + parm_size + GLUE_MOV_PX_DIRECTVALUE_SIZE,cfp_ptrs[pn]);
      }
      parm_size += cpsize;

    }
  }

  if (bufOut_len < parm_size + func_size) RET_MINUS1_FAIL("eelfunc size combined")
  
  if (bufOut) memcpy(bufOut + parm_size, func, func_size);

  return parm_size + func_size;
  // end of EEL function generation
}

#ifdef DUMP_OPS_DURING_COMPILE
void dumpOp(compileContext *ctx, opcodeRec *op, int start);
#endif

#ifdef EEL_DUMP_OPS
void dumpOpcodeTree(compileContext *ctx, FILE *fp, opcodeRec *op, int indent_amt)
{
  const char *fname="";
  fprintf(fp,"%*sOP TYPE %d", indent_amt, "",
         op->opcodeType==OPCODETYPE_DIRECTVALUE_TEMPSTRING ? 10000 : // remap around OPCODETYPE_DIRECTVALUE_TEMPSTRING
         op->opcodeType > OPCODETYPE_DIRECTVALUE_TEMPSTRING ? op->opcodeType - 1 : 
         op->opcodeType);

  if ((op->opcodeType == OPCODETYPE_FUNC1 || 
      op->opcodeType == OPCODETYPE_FUNC2 || 
      op->opcodeType == OPCODETYPE_FUNC3 || 
      op->opcodeType == OPCODETYPE_FUNCX))
  {
    if (op->fntype == FUNCTYPE_FUNCTIONTYPEREC)
    {
      functionType *fn_ptr = (functionType *)op->fn;
      fname = fn_ptr->name;
    }
    else if (op->fntype == FUNCTYPE_EELFUNC)
    {
      fname = op->relname;
    }
    if (!fname) fname ="";
  }

  switch (op->opcodeType)
  {
    case OPCODETYPE_DIRECTVALUE:
      fprintf(fp," DV=%f\r\n",op->parms.dv.directValue);
    break;
    case OPCODETYPE_VALUE_FROM_NAMESPACENAME: // this.* or namespace.* are encoded this way
      fprintf(fp," NSN=%s(%d)\r\n",op->relname?op->relname : "(null)",op->namespaceidx);
    break;
    case OPCODETYPE_VARPTR:
      {
        const char *nm = op->relname;
        if (!nm || !*nm)
        {
          int wb; 
          for (wb = 0; wb < ctx->varTable_numBlocks; wb ++)
          {
            char **plist=ctx->varTable_Names[wb];
            if (!plist) break;
  
            if (op->parms.dv.valuePtr >= ctx->varTable_Values[wb] && op->parms.dv.valuePtr < ctx->varTable_Values[wb] + NSEEL_VARS_PER_BLOCK)
            {
              nm = plist[op->parms.dv.valuePtr - ctx->varTable_Values[wb]];
              break;
            }
          }        
        }
        fprintf(fp," VP=%s\r\n", nm?nm : "(null)");
      }
    break;
    case OPCODETYPE_VARPTRPTR:
      fprintf(fp, " VPP?\r\n");
    break;
    case OPCODETYPE_FUNC1:
      if (op->fntype == FN_NOT)
        fprintf(fp," FUNC1 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_not");
      else if (op->fntype == FN_NOTNOT)
        fprintf(fp," FUNC1 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_notnot");
      else if (op->fntype == FN_MEMORY)
        fprintf(fp," FUNC1 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_mem");
      else if (op->fntype == FN_GMEMORY)
        fprintf(fp," FUNC1 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_gmem");
      else if (op->fntype == FN_WHILE)
        fprintf(fp," FUNC1 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "while");
      else
        fprintf(fp," FUNC1 %d %s {\r\n",op->fntype, fname);

      if (op->parms.parms[0])
        dumpOpcodeTree(ctx,fp,op->parms.parms[0],indent_amt+2);
      else
        fprintf(fp,"%*sINVALID PARM\r\n",indent_amt+2,"");
      fprintf(fp,"%*s}\r\n", indent_amt, "");
    break;
    case OPCODETYPE_MOREPARAMS:
    case OPCODETYPE_FUNC2:
      if (op->opcodeType == OPCODETYPE_MOREPARAMS)
        fprintf(fp," MOREPARAMS {\r\n");
      else
      {
        if (op->fntype == FN_POW)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "pow");
        else if (op->fntype == FN_MOD)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_mod");
        else if (op->fntype == FN_XOR)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_xor");
        else if (op->fntype == FN_SHL)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_shl");
        else if (op->fntype == FN_SHR)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_shr");
        else if (op->fntype == FN_LT)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_below");
        else if (op->fntype == FN_GT)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_above");
        else if (op->fntype == FN_LTE)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_beleq");
        else if (op->fntype == FN_GTE)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_aboeq");
        else if (op->fntype == FN_EQ)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_equal");
        else if (op->fntype == FN_NE)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_noteq");
        else if (op->fntype == FN_EQ_EXACT)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_equal_exact");
        else if (op->fntype == FN_NE_EXACT)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_noteq_exact");
        else if (op->fntype == FN_LOGICAL_AND)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_and");
        else if (op->fntype == FN_LOGICAL_OR)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_or");
        else if (op->fntype == FN_ASSIGN)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_set");
        else if (op->fntype == FN_ADD_OP)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_addop");
        else if (op->fntype == FN_SUB_OP)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_subop");
        else if (op->fntype == FN_MUL_OP)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_mulop");
        else if (op->fntype == FN_DIV_OP)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_divop");
        else if (op->fntype == FN_OR_OP)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_orop");
        else if (op->fntype == FN_AND_OP)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_andop");
        else if (op->fntype == FN_XOR_OP)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_xorop");
        else if (op->fntype == FN_MOD_OP)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_modop");
        else if (op->fntype == FN_POW_OP)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_powop");
        else if (op->fntype == FN_LOOP)
          fprintf(fp," FUNC2 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "loop");
        else
          fprintf(fp," FUNC2 %d %s {\r\n",op->fntype, fname);
      }
      if (op->parms.parms[0])
        dumpOpcodeTree(ctx,fp,op->parms.parms[0],indent_amt+2);
      else
        fprintf(fp,"%*sINVALID PARM\r\n",indent_amt+2,"");

      if (op->parms.parms[1])
        dumpOpcodeTree(ctx,fp,op->parms.parms[1],indent_amt+2);
      else
        fprintf(fp,"%*sINVALID PARM\r\n",indent_amt+2,"");
      fprintf(fp,"%*s}\r\n", indent_amt, "");
    break;
    case OPCODETYPE_FUNCX:
    case OPCODETYPE_FUNC3:
      if (op->opcodeType == OPCODETYPE_FUNCX)
        fprintf(fp," FUNCX %d %s {\r\n",op->fntype, fname);
      else if (op->fntype == FN_IF_ELSE)
        fprintf(fp," FUNC3 %d %s {\r\n",FUNCTYPE_FUNCTIONTYPEREC, "_if");
      else
        fprintf(fp," FUNC3 %d %s {\r\n",op->fntype, fname);
      if (op->parms.parms[0])
        dumpOpcodeTree(ctx,fp,op->parms.parms[0],indent_amt+2);
      else
        fprintf(fp,"%*sINVALID PARM\r\n",indent_amt+2,"");

      if (op->parms.parms[1])
        dumpOpcodeTree(ctx,fp,op->parms.parms[1],indent_amt+2);
      else
        fprintf(fp,"%*sINVALID PARM\r\n",indent_amt+2,"");

      if (op->parms.parms[2])
        dumpOpcodeTree(ctx,fp,op->parms.parms[2],indent_amt+2);
      else
        fprintf(fp,"%*sINVALID PARM\r\n",indent_amt+2,"");
      fprintf(fp,"%*s}\r\n", indent_amt, "");

    break;
  }
}

#endif

#ifdef GLUE_MAX_JMPSIZE
#define CHECK_SIZE_FORJMP(x,y) if ((x)<0 || (x)>=GLUE_MAX_JMPSIZE) goto y;
#define RET_MINUS1_FAIL_FALLBACK(err,j) goto j;
#else
#define CHECK_SIZE_FORJMP(x,y)
#define RET_MINUS1_FAIL_FALLBACK(err,j) RET_MINUS1_FAIL(err)
#endif
static int compileOpcodesInternal(compileContext *ctx, opcodeRec *op, unsigned char *bufOut, int bufOut_len, int *computTableSize, const namespaceInformation *namespacePathToThis, int *calledRvType, int preferredReturnValues, int *fpStackUse, int *canHaveDenormalOutput)
{
  int rv_offset=0, denormal_force=-1;
  if (!op) RET_MINUS1_FAIL("coi !op")

  *fpStackUse=0;
  for (;;)
  {
    // special case: statement delimiting means we can process the left side into place, and iteratively do the second parameter without recursing
    // also we don't need to save/restore anything to the stack (which the normal 2 parameter function processing does)
    if (op->opcodeType == OPCODETYPE_FUNC2 && op->fntype == FN_JOIN_STATEMENTS)
    {
      int fUse1;
      int parm_size = compileOpcodes(ctx,op->parms.parms[0],bufOut,bufOut_len, computTableSize, namespacePathToThis, RETURNVALUE_IGNORE, NULL,&fUse1,NULL);
      if (parm_size < 0) RET_MINUS1_FAIL("coc join fail")
      op = op->parms.parms[1];
      if (!op) RET_MINUS1_FAIL("join got to null")

      if (fUse1>*fpStackUse) *fpStackUse=fUse1;
      if (bufOut) bufOut += parm_size;
      bufOut_len -= parm_size;
      rv_offset += parm_size;
#ifdef DUMP_OPS_DURING_COMPILE
      if (op->opcodeType != OPCODETYPE_FUNC2 || op->fntype != FN_JOIN_STATEMENTS) dumpOp(ctx,op,0);
#endif
      denormal_force=-1;
    }
    // special case: __denormal_likely(), __denormal_unlikely()
    else if (op->opcodeType == OPCODETYPE_FUNC1 && (op->fntype == FN_DENORMAL_LIKELY || op->fntype == FN_DENORMAL_UNLIKELY))
    {
      denormal_force = op->fntype == FN_DENORMAL_LIKELY;
      op = op->parms.parms[0];
    }
    else 
    {
      break;
    }
  }
  if (denormal_force >= 0 && canHaveDenormalOutput)
  {  
    *canHaveDenormalOutput = denormal_force;
    canHaveDenormalOutput = &denormal_force; // prevent it from being changed by functions below
  }

  // special case: BAND/BOR
  if (op->opcodeType == OPCODETYPE_FUNC2 && (op->fntype == FN_LOGICAL_AND || op->fntype == FN_LOGICAL_OR))
  {
    int fUse1=0;
    int parm_size;
#ifdef GLUE_MAX_JMPSIZE
    int parm_size_pre;
#endif
    int retType=RETURNVALUE_IGNORE;
    if (preferredReturnValues != RETURNVALUE_IGNORE) retType = RETURNVALUE_BOOL;

    *calledRvType = retType;
    
    parm_size = compileOpcodes(ctx,op->parms.parms[0],bufOut,bufOut_len, computTableSize, namespacePathToThis, RETURNVALUE_BOOL, NULL, &fUse1, NULL);
    if (parm_size < 0) RET_MINUS1_FAIL("loop band/bor coc fail")
    
    if (fUse1 > *fpStackUse) *fpStackUse=fUse1;


#ifdef GLUE_MAX_JMPSIZE
    parm_size_pre=parm_size;
#endif

    {
      int sz2, fUse2=0;
      unsigned char *destbuf;
      const int testsz=op->fntype == FN_LOGICAL_OR ? sizeof(GLUE_JMP_IF_P1_NZ) : sizeof(GLUE_JMP_IF_P1_Z);
      if (bufOut_len < parm_size+testsz) RET_MINUS1_FAIL_FALLBACK("band/bor size fail",doNonInlinedAndOr_)

      if (bufOut)  memcpy(bufOut+parm_size,op->fntype == FN_LOGICAL_OR ? GLUE_JMP_IF_P1_NZ : GLUE_JMP_IF_P1_Z,testsz); 
      parm_size += testsz;
      destbuf = bufOut + parm_size;

      sz2= compileOpcodes(ctx,op->parms.parms[1],bufOut?bufOut+parm_size:NULL,bufOut_len-parm_size, computTableSize, namespacePathToThis, retType, NULL,&fUse2, NULL);

      CHECK_SIZE_FORJMP(sz2,doNonInlinedAndOr_)
      if (sz2<0) RET_MINUS1_FAIL("band/bor coc fail")

      parm_size+=sz2;
      if (bufOut) GLUE_JMP_SET_OFFSET(destbuf, (bufOut + parm_size) - destbuf);

      if (fUse2 > *fpStackUse) *fpStackUse=fUse2;
      return rv_offset + parm_size;
    }
#ifdef GLUE_MAX_JMPSIZE
    if (0) 
    {
      void *stub;
      int stubsize;        
      unsigned char *newblock2, *p;
    
      // encode as function call
doNonInlinedAndOr_:
      parm_size = parm_size_pre;

      if (op->fntype == FN_LOGICAL_AND) 
      {
        stub = GLUE_realAddress(nseel_asm_band,nseel_asm_band_end,&stubsize);
      }
      else 
      {
        stub = GLUE_realAddress(nseel_asm_bor,nseel_asm_bor_end,&stubsize);
      }
    
      if (bufOut_len < parm_size + stubsize) RET_MINUS1_FAIL("band/bor len fail")
    
      if (bufOut)
      {
        int fUse2=0;
        newblock2 = compileCodeBlockWithRet(ctx,op->parms.parms[1],computTableSize,namespacePathToThis, retType, NULL, &fUse2, NULL);
        if (!newblock2) RET_MINUS1_FAIL("band/bor ccbwr fail")

        if (fUse2 > *fpStackUse) *fpStackUse=fUse2;
    
        p = bufOut + parm_size;
        memcpy(p, stub, stubsize);
    
        p=EEL_GLUE_set_immediate(p,(INT_PTR)newblock2);
      }
      return rv_offset + parm_size + stubsize;
    }
#endif
  }  

  if (op->opcodeType == OPCODETYPE_FUNC3 && op->fntype == FN_IF_ELSE) // special case: IF
  {
    int fUse1=0;
#ifdef GLUE_MAX_JMPSIZE
    int parm_size_pre;
#endif
    int use_rv = RETURNVALUE_IGNORE;
    int rvMode=0;
    int parm_size = compileOpcodes(ctx,op->parms.parms[0],bufOut,bufOut_len, computTableSize, namespacePathToThis, RETURNVALUE_BOOL|RETURNVALUE_BOOL_REVERSED, &rvMode,&fUse1, NULL);
    if (parm_size < 0) RET_MINUS1_FAIL("if coc fail")
    if (fUse1 > *fpStackUse) *fpStackUse=fUse1;

    if (preferredReturnValues & RETURNVALUE_NORMAL) use_rv=RETURNVALUE_NORMAL;
    else if (preferredReturnValues & RETURNVALUE_FPSTACK) use_rv=RETURNVALUE_FPSTACK;
    else if (preferredReturnValues & RETURNVALUE_BOOL) use_rv=RETURNVALUE_BOOL;
    
    *calledRvType = use_rv;
#ifdef GLUE_MAX_JMPSIZE
    parm_size_pre = parm_size;
#endif

    {
      int csz,hasSecondHalf;
      if (rvMode & RETURNVALUE_BOOL_REVERSED)
      {
        if (bufOut_len < parm_size + (int)sizeof(GLUE_JMP_IF_P1_NZ)) RET_MINUS1_FAIL_FALLBACK("if size fail",doNonInlineIf_)
        if (bufOut) memcpy(bufOut+parm_size,GLUE_JMP_IF_P1_NZ,sizeof(GLUE_JMP_IF_P1_NZ));
        parm_size += sizeof(GLUE_JMP_IF_P1_NZ);
      }
      else
      {
        if (bufOut_len < parm_size + (int)sizeof(GLUE_JMP_IF_P1_Z)) RET_MINUS1_FAIL_FALLBACK("if size fail",doNonInlineIf_)
        if (bufOut) memcpy(bufOut+parm_size,GLUE_JMP_IF_P1_Z,sizeof(GLUE_JMP_IF_P1_Z));
        parm_size += sizeof(GLUE_JMP_IF_P1_Z);
      }
      csz=compileOpcodes(ctx,op->parms.parms[1],bufOut ? bufOut+parm_size : NULL,bufOut_len - parm_size, computTableSize, namespacePathToThis, use_rv, NULL,&fUse1, canHaveDenormalOutput);
      if (fUse1 > *fpStackUse) *fpStackUse=fUse1;
      hasSecondHalf = preferredReturnValues || !OPCODE_IS_TRIVIAL(op->parms.parms[2]);

      CHECK_SIZE_FORJMP(csz,doNonInlineIf_)
      if (csz<0) RET_MINUS1_FAIL("if coc fial")

      if (bufOut) GLUE_JMP_SET_OFFSET(bufOut + parm_size, csz + (hasSecondHalf?sizeof(GLUE_JMP_NC):0));
      parm_size+=csz;

      if (hasSecondHalf)
      {
        if (bufOut_len < parm_size + (int)sizeof(GLUE_JMP_NC)) RET_MINUS1_FAIL_FALLBACK("if len fail",doNonInlineIf_)
        if (bufOut) memcpy(bufOut+parm_size,GLUE_JMP_NC,sizeof(GLUE_JMP_NC));
        parm_size+=sizeof(GLUE_JMP_NC);

        csz=compileOpcodes(ctx,op->parms.parms[2],bufOut ? bufOut+parm_size : NULL,bufOut_len - parm_size, computTableSize, namespacePathToThis, use_rv, NULL, &fUse1, canHaveDenormalOutput);

        CHECK_SIZE_FORJMP(csz,doNonInlineIf_)
        if (csz<0) RET_MINUS1_FAIL("if coc 2 fail")

        // update jump address
        if (bufOut) GLUE_JMP_SET_OFFSET(bufOut + parm_size,csz); 
        parm_size+=csz;       
        if (fUse1 > *fpStackUse) *fpStackUse=fUse1;
      }
      return rv_offset + parm_size;
    }
#ifdef GLUE_MAX_JMPSIZE
    if (0)
    {
      unsigned char *newblock2,*newblock3,*ptr;
      void *stub;
      int stubsize;
doNonInlineIf_:
      parm_size = parm_size_pre;
      stub = GLUE_realAddress(nseel_asm_if,nseel_asm_if_end,&stubsize);
    
      if (!stub || bufOut_len < parm_size + stubsize) RET_MINUS1_FAIL(stub ? "if sz fail" : "if addr fail")
    
      if (bufOut)
      {
        int fUse2=0;
        newblock2 = compileCodeBlockWithRet(ctx,op->parms.parms[1],computTableSize,namespacePathToThis, use_rv, NULL,&fUse2, canHaveDenormalOutput); 
        if (fUse2 > *fpStackUse) *fpStackUse=fUse2;
        newblock3 = compileCodeBlockWithRet(ctx,op->parms.parms[2],computTableSize,namespacePathToThis, use_rv, NULL,&fUse2, canHaveDenormalOutput);
        if (fUse2 > *fpStackUse) *fpStackUse=fUse2;
        if (!newblock2 || !newblock3) RET_MINUS1_FAIL("if subblock gen fail")
    
        ptr = bufOut + parm_size;
        memcpy(ptr, stub, stubsize);
         
        ptr=EEL_GLUE_set_immediate(ptr,(INT_PTR)newblock2);
        EEL_GLUE_set_immediate(ptr,(INT_PTR)newblock3);
      }
      return rv_offset + parm_size + stubsize;
    }
#endif
  }

  {
    // special case: while
    if (op->opcodeType == OPCODETYPE_FUNC1 && op->fntype == FN_WHILE)
    {
      *calledRvType = RETURNVALUE_BOOL;

#ifndef GLUE_INLINE_LOOPS
      // todo: PPC looping support when loop length is small enough
      {
        unsigned char *pwr=bufOut;
        unsigned char *newblock2;
        int stubsz;
        void *stubfunc = GLUE_realAddress(nseel_asm_repeatwhile,nseel_asm_repeatwhile_end,&stubsz);
        if (!stubfunc || bufOut_len < stubsz) RET_MINUS1_FAIL(stubfunc ? "repeatwhile size fail" :"repeatwhile addr fail")

        if (bufOut)
        {
          newblock2=compileCodeBlockWithRet(ctx,op->parms.parms[0],computTableSize,namespacePathToThis, RETURNVALUE_BOOL, NULL, fpStackUse, NULL);
          if (!newblock2) RET_MINUS1_FAIL("repeatwhile ccbwr fail")
      
          memcpy(pwr,stubfunc,stubsz);
          pwr=EEL_GLUE_set_immediate(pwr,(INT_PTR)newblock2); 
        }
      
        return rv_offset+stubsz;
      }
#else
      {
#ifndef GLUE_WHILE_END_NOJUMP
        unsigned char *jzoutpt;
#endif
        unsigned char *looppt;
        int parm_size=0,subsz;
        if (bufOut_len < parm_size + (int)(GLUE_WHILE_SETUP_SIZE + sizeof(GLUE_WHILE_BEGIN))) RET_MINUS1_FAIL("while size fail 1")

        if (bufOut) memcpy(bufOut + parm_size,GLUE_WHILE_SETUP,GLUE_WHILE_SETUP_SIZE);
        parm_size+=GLUE_WHILE_SETUP_SIZE;

        looppt = bufOut + parm_size;
        if (bufOut) memcpy(bufOut + parm_size,GLUE_WHILE_BEGIN,sizeof(GLUE_WHILE_BEGIN));
        parm_size+=sizeof(GLUE_WHILE_BEGIN);

        subsz = compileOpcodes(ctx,op->parms.parms[0],bufOut ? (bufOut + parm_size) : NULL,bufOut_len - parm_size, computTableSize, namespacePathToThis, RETURNVALUE_BOOL, NULL,fpStackUse, NULL);
        if (subsz<0) RET_MINUS1_FAIL("while coc fail")

        if (bufOut_len < parm_size + (int)(sizeof(GLUE_WHILE_END) + sizeof(GLUE_WHILE_CHECK_RV))) RET_MINUS1_FAIL("which size fial 2")

        parm_size+=subsz;
        if (bufOut) memcpy(bufOut + parm_size, GLUE_WHILE_END, sizeof(GLUE_WHILE_END));
        parm_size+=sizeof(GLUE_WHILE_END);
#ifndef GLUE_WHILE_END_NOJUMP
        jzoutpt = bufOut + parm_size;
#endif

        if (bufOut) memcpy(bufOut + parm_size, GLUE_WHILE_CHECK_RV, sizeof(GLUE_WHILE_CHECK_RV));
        parm_size+=sizeof(GLUE_WHILE_CHECK_RV);
        if (bufOut) 
        {
          GLUE_JMP_SET_OFFSET(bufOut + parm_size,(looppt - (bufOut+parm_size)) );
#ifndef GLUE_WHILE_END_NOJUMP
          GLUE_JMP_SET_OFFSET(jzoutpt, (bufOut + parm_size) - jzoutpt);
#endif
        }
        return rv_offset+parm_size;
      }

#endif
    }

    // special case: loop
    if (op->opcodeType == OPCODETYPE_FUNC2 && op->fntype == FN_LOOP)
    {
      int fUse1;
      int parm_size = compileOpcodes(ctx,op->parms.parms[0],bufOut,bufOut_len, computTableSize, namespacePathToThis, RETURNVALUE_FPSTACK, NULL,&fUse1, NULL);
      if (parm_size < 0) RET_MINUS1_FAIL("loop coc fail")
      
      *calledRvType = RETURNVALUE_BOOL;
      if (fUse1 > *fpStackUse) *fpStackUse=fUse1;
           
#ifndef GLUE_INLINE_LOOPS
      // todo: PPC looping support when loop length is small enough
      {
        void *stub;
        int stubsize;        
        unsigned char *newblock2, *p;
        stub = GLUE_realAddress(nseel_asm_repeat,nseel_asm_repeat_end,&stubsize);
        if (bufOut_len < parm_size + stubsize) RET_MINUS1_FAIL("loop size fail")
        if (bufOut)
        {
          newblock2 = compileCodeBlockWithRet(ctx,op->parms.parms[1],computTableSize,namespacePathToThis, RETURNVALUE_IGNORE, NULL,fpStackUse, NULL);
      
          p = bufOut + parm_size;
          memcpy(p, stub, stubsize);
      
          p=EEL_GLUE_set_immediate(p,(INT_PTR)newblock2);
        }
        return rv_offset + parm_size + stubsize;
      }
#else
      {
        int subsz;
        int fUse2=0;
        unsigned char *skipptr1,*loopdest;

        if (bufOut_len < parm_size + (int)(sizeof(GLUE_LOOP_LOADCNT) + GLUE_LOOP_CLAMPCNT_SIZE + GLUE_LOOP_BEGIN_SIZE)) RET_MINUS1_FAIL("loop size fail")

        // store, convert to int, compare against 1, if less than, skip to end
        if (bufOut) memcpy(bufOut+parm_size,GLUE_LOOP_LOADCNT,sizeof(GLUE_LOOP_LOADCNT));
        parm_size += sizeof(GLUE_LOOP_LOADCNT);
        skipptr1 = bufOut+parm_size; 

        // compare aginst max loop length, jump to loop start if not above it
        if (bufOut) memcpy(bufOut+parm_size,GLUE_LOOP_CLAMPCNT,GLUE_LOOP_CLAMPCNT_SIZE);
        parm_size += GLUE_LOOP_CLAMPCNT_SIZE;

        // loop code:
        loopdest = bufOut + parm_size;

        if (bufOut) memcpy(bufOut+parm_size,GLUE_LOOP_BEGIN,GLUE_LOOP_BEGIN_SIZE);
        parm_size += GLUE_LOOP_BEGIN_SIZE;

        subsz = compileOpcodes(ctx,op->parms.parms[1],bufOut ? (bufOut + parm_size) : NULL,bufOut_len - parm_size, computTableSize, namespacePathToThis, RETURNVALUE_IGNORE, NULL, &fUse2, NULL);
        if (subsz<0) RET_MINUS1_FAIL("loop coc fail")
        if (fUse2 > *fpStackUse) *fpStackUse=fUse2;

        parm_size += subsz;

        if (bufOut_len < parm_size + (int)sizeof(GLUE_LOOP_END)) RET_MINUS1_FAIL("loop size fail 2")

        if (bufOut) memcpy(bufOut+parm_size,GLUE_LOOP_END,sizeof(GLUE_LOOP_END));
        parm_size += sizeof(GLUE_LOOP_END);
        
        if (bufOut) 
        {
          GLUE_JMP_SET_OFFSET(bufOut + parm_size,loopdest - (bufOut+parm_size));
          GLUE_JMP_SET_OFFSET(skipptr1, (bufOut+parm_size) - skipptr1);
        }

        return rv_offset + parm_size;

      }
#endif
    }    
  }
 
  switch (op->opcodeType)
  {
    case OPCODETYPE_DIRECTVALUE:
        if (preferredReturnValues == RETURNVALUE_BOOL)
        {
          int w = fabs(op->parms.dv.directValue) >= NSEEL_CLOSEFACTOR;
          int wsz=(w?sizeof(GLUE_SET_P1_NZ):sizeof(GLUE_SET_P1_Z));

          *calledRvType = RETURNVALUE_BOOL;
          if (bufOut_len < wsz) RET_MINUS1_FAIL("direct bool size fail3")
          if (bufOut) memcpy(bufOut,w?GLUE_SET_P1_NZ:GLUE_SET_P1_Z,wsz);
          return rv_offset+wsz;
        }
        else if (preferredReturnValues & RETURNVALUE_FPSTACK)
        {
#ifdef GLUE_HAS_FLDZ
          if (op->parms.dv.directValue == 0.0)
          {
            *fpStackUse = 1;
            *calledRvType = RETURNVALUE_FPSTACK;
            if (bufOut_len < sizeof(GLUE_FLDZ)) RET_MINUS1_FAIL("direct fp fail 1")
            if (bufOut) memcpy(bufOut,GLUE_FLDZ,sizeof(GLUE_FLDZ));
            return rv_offset+sizeof(GLUE_FLDZ);
          }
#endif
#ifdef GLUE_HAS_FLD1
          if (op->parms.dv.directValue == 1.0)
          {
            *fpStackUse = 1;
            *calledRvType = RETURNVALUE_FPSTACK;
            if (bufOut_len < sizeof(GLUE_FLD1)) RET_MINUS1_FAIL("direct fp fail 1")
            if (bufOut) memcpy(bufOut,GLUE_FLD1,sizeof(GLUE_FLD1));
            return rv_offset+sizeof(GLUE_FLD1);
          }
#endif
        }
        // fall through
    case OPCODETYPE_DIRECTVALUE_TEMPSTRING:
    case OPCODETYPE_VALUE_FROM_NAMESPACENAME:
    case OPCODETYPE_VARPTR:
    case OPCODETYPE_VARPTRPTR:


      #ifdef GLUE_MOV_PX_DIRECTVALUE_TOSTACK_SIZE
        if (OPCODE_IS_TRIVIAL(op))
        {
          if (preferredReturnValues & RETURNVALUE_FPSTACK)
          {
            *fpStackUse = 1;
            if (bufOut_len < GLUE_MOV_PX_DIRECTVALUE_TOSTACK_SIZE) RET_MINUS1_FAIL("direct fp fail 2")
            if (bufOut)
            {
              if (generateValueToReg(ctx,op,bufOut,-1,namespacePathToThis, 1 /*allow caching*/)<0) RET_MINUS1_FAIL("direct fp fail gvr")
            }
            *calledRvType = RETURNVALUE_FPSTACK;
            return rv_offset+GLUE_MOV_PX_DIRECTVALUE_TOSTACK_SIZE;
          }
        }
      #endif

      if (bufOut_len < GLUE_MOV_PX_DIRECTVALUE_SIZE) 
      {
        RET_MINUS1_FAIL("direct value fail 1")
      }
      if (bufOut) 
      {
        if (generateValueToReg(ctx,op,bufOut,0,namespacePathToThis, 
              (preferredReturnValues&(RETURNVALUE_FPSTACK|RETURNVALUE_CACHEABLE))!=0)<0) 
        {
          RET_MINUS1_FAIL("direct value gvr fail3")
        }
      }
    return rv_offset + GLUE_MOV_PX_DIRECTVALUE_SIZE;

    case OPCODETYPE_FUNCX:
    case OPCODETYPE_FUNC1:
    case OPCODETYPE_FUNC2:
    case OPCODETYPE_FUNC3:
      
      if (op->fntype == FUNCTYPE_EELFUNC)
      {
        int a;
        
        a = compileEelFunctionCall(ctx,op,bufOut,bufOut_len,computTableSize,namespacePathToThis, calledRvType,fpStackUse,canHaveDenormalOutput);
        if (a<0) return a;
        rv_offset += a;
      }
      else
      {
        int a;
        a = compileNativeFunctionCall(ctx,op,bufOut,bufOut_len,computTableSize,namespacePathToThis, calledRvType,fpStackUse,preferredReturnValues,canHaveDenormalOutput);
        if (a<0)return a;
        rv_offset += a;
      }        
    return rv_offset;
  }

  RET_MINUS1_FAIL("default opcode fail")
}

#ifdef DUMP_OPS_DURING_COMPILE
FILE *g_debugfp;
int g_debugfp_indent;
int g_debugfp_histsz=0;

void dumpOp(compileContext *ctx, opcodeRec *op, int start)
{
  if (start>=0)
  {
    if (g_debugfp)
    {
      static opcodeRec **hist;
      
      int x;
      int hit=0;
      if (!hist) hist = (opcodeRec**) calloc(1024,1024*sizeof(opcodeRec*));
      for(x=0;x<g_debugfp_histsz;x++)
      {
        if (hist[x] == op) { hit=1; break; }
      }
      if (x ==g_debugfp_histsz && g_debugfp_histsz<1024*1024) hist[g_debugfp_histsz++] = op;

      if (!start) 
      {
        g_debugfp_indent-=2;
        fprintf(g_debugfp,"%*s}(join)\n",g_debugfp_indent," ");
      }
      if (g_debugfp_indent>=100) *(char *)1=0;
      fprintf(g_debugfp,"%*s{ %p : %d%s: ",g_debugfp_indent," ",op,op->opcodeType, hit ? " -- DUPLICATE" : "");
      switch (op->opcodeType)
      {
        case OPCODETYPE_DIRECTVALUE:
          fprintf(g_debugfp,"dv %f",op->parms.dv.directValue);
        break;
        case OPCODETYPE_VARPTR:
          if (op->relname && op->relname[0])
          {
            fprintf(g_debugfp,"var %s",op->relname);
          }
          else
          {
            int wb; 
            for (wb = 0; wb < ctx->varTable_numBlocks; wb ++)
            {
              char **plist=ctx->varTable_Names[wb];
              if (!plist) break;
    
              if (op->parms.dv.valuePtr >= ctx->varTable_Values[wb] && op->parms.dv.valuePtr < ctx->varTable_Values[wb] + NSEEL_VARS_PER_BLOCK)
              {
                fprintf(g_debugfp,"var %s",plist[op->parms.dv.valuePtr - ctx->varTable_Values[wb]]);
                break;
              }
            }        
          }
        break;
        case OPCODETYPE_FUNC1:
        case OPCODETYPE_FUNC2:
        case OPCODETYPE_FUNC3:
        case OPCODETYPE_FUNCX:
          if (op->fntype == FUNCTYPE_FUNCTIONTYPEREC)
          {
            functionType *p=(functionType*)op->fn;
            fprintf(g_debugfp,"func %d: %s",p->nParams&0xff,p->name);
          }
          else
            fprintf(g_debugfp,"sf %d",op->fntype);
        break;

      }
      fprintf(g_debugfp,"\n");
      g_debugfp_indent+=2;
    }
  }
  else
  {
    if (g_debugfp)
    {
      g_debugfp_indent-=2;
      fprintf(g_debugfp,"%*s}%p\n",g_debugfp_indent," ",op);
    }
  }
}
#endif

int compileOpcodes(compileContext *ctx, opcodeRec *op, unsigned char *bufOut, int bufOut_len, int *computTableSize, const namespaceInformation *namespacePathToThis, 
                   int supportedReturnValues, int *rvType, int *fpStackUse, int *canHaveDenormalOutput)
{
  int code_returns=RETURNVALUE_NORMAL;
  int fpsu=0;
  int codesz;
  int denorm=0;

#ifdef DUMP_OPS_DURING_COMPILE
  dumpOp(ctx,op,1);
#endif
  
  codesz = compileOpcodesInternal(ctx,op,bufOut,bufOut_len,computTableSize,namespacePathToThis,&code_returns, supportedReturnValues,&fpsu,&denorm);
  if (denorm && canHaveDenormalOutput) *canHaveDenormalOutput=1;

#ifdef DUMP_OPS_DURING_COMPILE
  dumpOp(ctx,op,-1);
#endif
#ifdef EEL_DUMP_OPS
      // dump opcode trees for verification, after optimizing
      if (g_eel_dump_fp2)
      {
        fprintf(g_eel_dump_fp2,"-- compileOpcodes generated %d bytes of code!\r\n",codesz);
      }
#endif
  if (codesz < 0) return codesz;


  /*
  {
    char buf[512];
    sprintf(buf,"opcode %d %d (%s): fpu use: %d\n",op->opcodeType,op->fntype,
      op->opcodeType >= OPCODETYPE_FUNC1 && op->fntype == FUNCTYPE_FUNCTIONTYPEREC ? (
      ((functionType *)op->fn)->name
      ) : "",
      fpsu);
    OutputDebugString(buf);
  }
  */

  if (fpStackUse) *fpStackUse=fpsu;

  if (bufOut) bufOut += codesz;
  bufOut_len -= codesz;


  if (code_returns == RETURNVALUE_BOOL && !(supportedReturnValues & RETURNVALUE_BOOL) && supportedReturnValues)
  {
    int stubsize;
    void *stub = GLUE_realAddress(nseel_asm_booltofp,nseel_asm_booltofp_end,&stubsize);
    if (!stub || bufOut_len < stubsize) RET_MINUS1_FAIL(stub?"booltofp size":"booltfp addr")
    if (bufOut) 
    {
      memcpy(bufOut,stub,stubsize);
      bufOut += stubsize;
    }
    codesz+=stubsize;
    bufOut_len -= stubsize;
    
    code_returns = RETURNVALUE_FPSTACK;
  }


  // default processing of code_returns to meet return value requirements
  if (supportedReturnValues & code_returns) 
  {
    if (rvType) *rvType = code_returns;
    return codesz;
  }


  if (rvType) *rvType = RETURNVALUE_IGNORE;


  if (code_returns == RETURNVALUE_NORMAL)
  {
    if (supportedReturnValues & (RETURNVALUE_FPSTACK|RETURNVALUE_BOOL))
    {
      if (bufOut_len < GLUE_PUSH_VAL_AT_PX_TO_FPSTACK_SIZE) RET_MINUS1_FAIL("pushvalatpxtofpstack,size")
      if (bufOut) 
      {
        GLUE_PUSH_VAL_AT_PX_TO_FPSTACK(bufOut,0); // always fld qword [eax] but we might change that later
        bufOut += GLUE_PUSH_VAL_AT_PX_TO_FPSTACK_SIZE;
      }
      codesz += GLUE_PUSH_VAL_AT_PX_TO_FPSTACK_SIZE;  
      bufOut_len -= GLUE_PUSH_VAL_AT_PX_TO_FPSTACK_SIZE;

      if (supportedReturnValues & RETURNVALUE_BOOL) 
      {
        code_returns = RETURNVALUE_FPSTACK;
      }
      else
      {
        if (rvType) *rvType = RETURNVALUE_FPSTACK;
      }
    }
  }

  if (code_returns == RETURNVALUE_FPSTACK)
  {
    if (supportedReturnValues & (RETURNVALUE_BOOL|RETURNVALUE_BOOL_REVERSED))
    {
      int stubsize;
      void *stub;
      
      if (supportedReturnValues & RETURNVALUE_BOOL_REVERSED)
      {
        if (rvType) *rvType = RETURNVALUE_BOOL_REVERSED;
        stub = GLUE_realAddress(nseel_asm_fptobool_rev,nseel_asm_fptobool_rev_end,&stubsize);
      }
      else
      {
        if (rvType) *rvType = RETURNVALUE_BOOL;
        stub = GLUE_realAddress(nseel_asm_fptobool,nseel_asm_fptobool_end,&stubsize);
      }


      if (!stub || bufOut_len < stubsize) RET_MINUS1_FAIL(stub?"fptobool size":"fptobool addr")
      if (bufOut) 
      {
        memcpy(bufOut,stub,stubsize);
        bufOut += stubsize;
      }
      codesz+=stubsize;
      bufOut_len -= stubsize;
    }
    else if (supportedReturnValues & RETURNVALUE_NORMAL)
    {
      if (computTableSize) (*computTableSize) ++;

      if (bufOut_len < GLUE_POP_FPSTACK_TO_WTP_TO_PX_SIZE) RET_MINUS1_FAIL("popfpstacktowtptopxsize")

      // generate fp-pop to temp space
      if (bufOut) GLUE_POP_FPSTACK_TO_WTP_TO_PX(bufOut,0);
      codesz+=GLUE_POP_FPSTACK_TO_WTP_TO_PX_SIZE;
      if (rvType) *rvType = RETURNVALUE_NORMAL;
    }
    else
    {
      // toss return value that will be ignored
      if (bufOut_len < GLUE_POP_FPSTACK_SIZE) RET_MINUS1_FAIL("popfpstack size")
      if (bufOut) memcpy(bufOut,GLUE_POP_FPSTACK,GLUE_POP_FPSTACK_SIZE);   
      codesz+=GLUE_POP_FPSTACK_SIZE;
    }
  }

  return codesz;
}


#if 0
static void movestringover(char *str, int amount)
{
  char tmp[1024+8];

  int l=(int)strlen(str);
  l=wdl_min(1024-amount-1,l);

  memcpy(tmp,str,l+1);

  while (l >= 0 && tmp[l]!='\n') l--;
  l++;

  tmp[l]=0;//ensure we null terminate

  memcpy(str+amount,tmp,l+1);
}
#endif

//------------------------------------------------------------------------------
NSEEL_CODEHANDLE NSEEL_code_compile(NSEEL_VMCTX _ctx, const char *_expression, int lineoffs)
{
  return NSEEL_code_compile_ex(_ctx,_expression,lineoffs,0);
}

typedef struct topLevelCodeSegmentRec {
  struct topLevelCodeSegmentRec *_next;
  void *code;
  int codesz;
  int tmptable_use;
} topLevelCodeSegmentRec;


NSEEL_CODEHANDLE NSEEL_code_compile_ex(NSEEL_VMCTX _ctx, const char *_expression, int lineoffs, int compile_flags)
{
  compileContext *ctx = (compileContext *)_ctx;
  const char *endptr;
  const char *_expression_end;
  codeHandleType *handle;
  topLevelCodeSegmentRec *startpts_tail=NULL;
  topLevelCodeSegmentRec *startpts=NULL;
  _codeHandleFunctionRec *oldCommonFunctionList;
  int curtabptr_sz=0;
  void *curtabptr=NULL;
  int had_err=0;

  if (!ctx) return 0;

  ctx->directValueCache=0;
  ctx->optimizeDisableFlags=0;
  ctx->gotEndOfInput=0;
  ctx->current_compile_flags = compile_flags;

  if (compile_flags & NSEEL_CODE_COMPILE_FLAG_COMMONFUNCS_RESET)
  {
    ctx->functions_common=NULL; // reset common function list
  }
  else
  {
    // reset common compiled function code, forcing a recompile if shared
    _codeHandleFunctionRec *a = ctx->functions_common;
    while (a)
    {
      _codeHandleFunctionRec *b = a->derivedCopies;

      if (a->localstorage) 
      {
        // force local storage actual values to be reallocated if used again
        memset(a->localstorage,0,sizeof(EEL_F *) * a->localstorage_size);
      }

      a->startptr = NULL; // force this copy to be recompiled
      a->startptr_size = -1;

      while (b)
      {
        b->startptr = NULL; // force derived copies to get recompiled
        b->startptr_size = -1;
        // no need to reset b->localstorage, since it points to a->localstorage
        b=b->derivedCopies;
      }

      a=a->next;
    }
  }
  
  ctx->last_error_string[0]=0;

  if (!_expression || !*_expression) return 0;

  _expression_end = _expression + strlen(_expression);

  oldCommonFunctionList = ctx->functions_common;

  ctx->isGeneratingCommonFunction=0;
  ctx->isSharedFunctions = !!(compile_flags & NSEEL_CODE_COMPILE_FLAG_COMMONFUNCS);
  ctx->functions_local = NULL;

  freeBlocks(&ctx->tmpblocks_head);  // free blocks
  freeBlocks(&ctx->blocks_head);  // free blocks
  freeBlocks(&ctx->blocks_head_data);  // free blocks
  memset(ctx->l_stats,0,sizeof(ctx->l_stats));

  handle = (codeHandleType*)newDataBlock(sizeof(codeHandleType),8);

  if (!handle) 
  {
    return 0;
  }

  
  memset(handle,0,sizeof(codeHandleType));

  ctx->l_stats[0] += (int)(_expression_end - _expression);
  ctx->tmpCodeHandle = handle;
  endptr=_expression;

  while (*endptr)
  {
    int computTableTop = 0;
    int startptr_size=0;
    void *startptr=NULL;
    opcodeRec *start_opcode=NULL;
    const char *expr=endptr;
    
    int function_numparms=0;
    char is_fname[NSEEL_MAX_VARIABLE_NAMELEN+1];
    is_fname[0]=0;

    memset(ctx->function_localTable_Size,0,sizeof(ctx->function_localTable_Size));
    memset(ctx->function_localTable_Names,0,sizeof(ctx->function_localTable_Names));
    ctx->function_localTable_ValuePtrs=0;
    ctx->function_usesNamespaces=0;
    ctx->function_curName=NULL;
    ctx->function_globalFlag=0;
        
    ctx->errVar=0;

    // single out top level segment
    {
      int had_something = 0, pcnt=0, pcnt2=0;
      int state=0;
      for (;;)
      {
        int l;
        const char *p=nseel_simple_tokenizer(&endptr,_expression_end,&l,&state);
        if (!p) 
        {
          if (pcnt || pcnt2) ctx->gotEndOfInput|=4;
          break;
        }

        if (*p == ';') 
        {
          if (had_something && !pcnt && !pcnt2) break;
        }
        else if (*p == '/' && l > 1 && (p[1] == '/' || p[1] == '*')) 
        {
          if (l > 19 && !strnicmp(p,"//#eel-no-optimize:",19))
            ctx->optimizeDisableFlags = atoi(p+19);
        }
        else
        {
          if (!had_something) 
          {
            expr = p;
            had_something = 1;
          }

          if (*p == '(') pcnt++;
          else if (*p == ')')  {  if (--pcnt<0) pcnt=0; }
          else if (*p == '[') pcnt2++;
          else if (*p == ']')  {  if (--pcnt2<0) pcnt2=0; }
        }
      }
      if (!*expr || !had_something) break;
    }

    // parse   

    {
      int tmplen,funcname_len;
      const char *p = expr;
      const char *tok1 = nseel_simple_tokenizer(&p,endptr,&tmplen,NULL);
      const char *funcname = nseel_simple_tokenizer(&p,endptr,&funcname_len,NULL);
      if (tok1 && funcname && tmplen == 8 && !strnicmp(tok1,"function",8) && (isalpha(funcname[0]) || funcname[0] == '_'))
      {
        int had_parms_locals=0;
        if (funcname_len > sizeof(is_fname)-1) funcname_len=sizeof(is_fname)-1;
        memcpy(is_fname, funcname, funcname_len);
        is_fname[funcname_len]=0;
        ctx->function_curName = is_fname; // only assigned for the duration of the loop, cleared later //-V507

        while (NULL != (tok1 = nseel_simple_tokenizer(&p,endptr,&tmplen,NULL)))
        {
          int is_parms = 0, localTableContext = 0;
          int maxcnt=0;
          const char *sp_save;

          if (tok1[0] == '(')
          {
            if (had_parms_locals) 
            {
              expr = p-1; // begin compilation at this code!
              break;
            }
            is_parms = 1;
          }
          else
          {
            if (tmplen == 5 && !strnicmp(tok1,"local",tmplen)) localTableContext=0;
            else if (tmplen == 6 && !strnicmp(tok1,"static",tmplen)) localTableContext=0;
            else if (tmplen == 8 && !strnicmp(tok1,"instance",tmplen)) localTableContext=1;
            else if ((tmplen == 7 && !strnicmp(tok1,"globals",tmplen))  ||
                     (tmplen == 6 && !strnicmp(tok1,"global",tmplen)))
            {
              ctx->function_globalFlag = 1;
              localTableContext=2;
            }
            else break; // unknown token!

            tok1 = nseel_simple_tokenizer(&p,endptr,&tmplen,NULL);
            if (!tok1 || tok1[0] != '(') break;
          }
          had_parms_locals = 1;


          sp_save=p;

          while (NULL != (tok1 = nseel_simple_tokenizer(&p,endptr,&tmplen,NULL)))
          {
            if (tok1[0] == ')') break;
            if (*tok1 == '#' && localTableContext!=1 && localTableContext!=2) 
            {
              ctx->errVar = (int) (tok1 - _expression);
              lstrcpyn_safe(ctx->last_error_string,"#string can only be in instance() or globals()",sizeof(ctx->last_error_string));
              goto had_error;
            }

            if (isalpha(*tok1) || *tok1 == '_' || *tok1 == '#') 
            {
              maxcnt++;
              if (p < endptr && *p == '*')
              {
                if (!is_parms && localTableContext!=2)
                {
                  ctx->errVar = (int) (p - _expression);
                  lstrcpyn_safe(ctx->last_error_string,"namespace* can only be used in parameters or globals()",sizeof(ctx->last_error_string));
                  goto had_error;
                }
                p++;
              }
            }
            else if (*tok1 != ',')
            {
              ctx->errVar = (int)(tok1 - _expression);
              lstrcpyn_safe(ctx->last_error_string,"unknown character in function parameters",sizeof(ctx->last_error_string));
              goto had_error;
            }
          }

          if (tok1 && maxcnt > 0)
          {
            char **ot = ctx->function_localTable_Names[localTableContext];
            const int osz = ctx->function_localTable_Size[localTableContext];            
       
            maxcnt += osz;

            ctx->function_localTable_Names[localTableContext] = (char **)newTmpBlock(ctx,sizeof(char *) * maxcnt);

            if (ctx->function_localTable_Names[localTableContext])
            {
              int i=osz;
              if (osz && ot) memcpy(ctx->function_localTable_Names[localTableContext],ot,sizeof(char *) * osz);
              p=sp_save;

              while (NULL != (tok1 = nseel_simple_tokenizer(&p,endptr,&tmplen,NULL)))
              {
                if (tok1[0] == ')') break;
                if (isalpha(*tok1) || *tok1 == '_' || *tok1 == '#') 
                {
                  char *newstr;
                  int l = tmplen;
                  if (*p == '*')  // xyz* for namespace
                  {
                    p++;
                    l++;
                  }
                  if (l > NSEEL_MAX_VARIABLE_NAMELEN) l = NSEEL_MAX_VARIABLE_NAMELEN;
                  newstr = newTmpBlock(ctx,l+1);
                  if (newstr)
                  {
                    memcpy(newstr,tok1,l);
                    newstr[l]=0;
                    ctx->function_localTable_Names[localTableContext][i++] = newstr;
                  }
                }
              }
              ctx->function_localTable_Size[localTableContext]=i;
              if (is_parms) function_numparms = i;
            }         
          }
        }
      }
    }
    if (ctx->function_localTable_Size[0]>0)
    {
      ctx->function_localTable_ValuePtrs = 
          ctx->isSharedFunctions ? newDataBlock(ctx->function_localTable_Size[0] * sizeof(EEL_F *),8) : 
                                   newTmpBlock(ctx,ctx->function_localTable_Size[0] * sizeof(EEL_F *)); 
      if (!ctx->function_localTable_ValuePtrs)
      {
        ctx->function_localTable_Size[0]=0;
        function_numparms=0;
      }
      else
      {
        memset(ctx->function_localTable_ValuePtrs,0,sizeof(EEL_F *) * ctx->function_localTable_Size[0]); // force values to be allocated
      }
    }

   {
     int nseelparse(compileContext* context);
     void nseelrestart (void *input_file ,void *yyscanner );

     ctx->rdbuf_start = _expression;

#ifdef NSEEL_SUPER_MINIMAL_LEXER

     ctx->rdbuf = expr;
     ctx->rdbuf_end = endptr;
     if (!nseelparse(ctx) && !ctx->errVar)
     {
       start_opcode = ctx->result;
     }
#else

     nseelrestart(NULL,ctx->scanner);

     ctx->rdbuf = expr;
     ctx->rdbuf_end = endptr;

     if (!nseelparse(ctx) && !ctx->errVar)
     {
       start_opcode = ctx->result;
     }
     if (ctx->errVar)
     {
       const char *p=expr;
       ctx->errVar += expr-_expression;
     }
#endif
     ctx->rdbuf = NULL;
   }
           
    if (start_opcode)
    {
      int rvMode=0, fUse=0;

#ifdef LOG_OPT
      char buf[512];
      int sd=0;
      sprintf(buf,"pre opt sz=%d (tsackDepth=%d)\n",compileOpcodes(ctx,start_opcode,NULL,1024*1024*256,NULL, NULL,RETURNVALUE_IGNORE,NULL,&sd,NULL),sd);
#ifdef _WIN32
      OutputDebugString(buf);
#else
      printf("%s\n",buf);
#endif
#endif

#ifdef EEL_DUMP_OPS
      // dump opcode trees for verification, before optimizing
      if (g_eel_dump_fp)
      {
        fprintf(g_eel_dump_fp,"-- opcode chunk --\r\n");
        dumpOpcodeTree(ctx,g_eel_dump_fp,start_opcode,2);        
      }
#endif

      if (!(ctx->optimizeDisableFlags&OPTFLAG_NO_OPTIMIZE)) optimizeOpcodes(ctx,start_opcode,is_fname[0] ? 1 : 0);
#ifdef LOG_OPT
      sprintf(buf,"post opt sz=%d, stack depth=%d\n",compileOpcodes(ctx,start_opcode,NULL,1024*1024*256,NULL,NULL, RETURNVALUE_IGNORE,NULL,&sd,NULL),sd);
#ifdef _WIN32
      OutputDebugString(buf);
#else
      printf("%s\n",buf);
#endif
#endif

#ifdef EEL_DUMP_OPS
      // dump opcode trees for verification, after optimizing
      if (g_eel_dump_fp2)
      {
        fprintf(g_eel_dump_fp2,"-- POST-OPTIMIZED opcode chunk --\r\n");
        dumpOpcodeTree(ctx,g_eel_dump_fp2,start_opcode,2);        
      }
#endif

      if (is_fname[0])
      {
        _codeHandleFunctionRec *fr = ctx->isSharedFunctions ? newDataBlock(sizeof(_codeHandleFunctionRec),8) : 
                                        newTmpBlock(ctx,sizeof(_codeHandleFunctionRec)); 
        if (fr)
        {
          memset(fr,0,sizeof(_codeHandleFunctionRec));
          fr->startptr_size = -1;
          fr->opcodes = start_opcode;

          if (ctx->function_localTable_Size[0] > 0 && ctx->function_localTable_ValuePtrs)
          {
            if (ctx->function_localTable_Names[0])
            {
              int i;
              for(i=0;i<function_numparms;i++)
              {
                const char *nptr = ctx->function_localTable_Names[0][i];
                if (nptr && *nptr && nptr[strlen(nptr)-1] == '*') 
                {
                  fr->parameterAsNamespaceMask |= ((unsigned int)1)<<i;
                }
              }
            }
            fr->num_params=function_numparms;
            fr->localstorage = ctx->function_localTable_ValuePtrs;
            fr->localstorage_size = ctx->function_localTable_Size[0];
          }

          fr->usesNamespaces = ctx->function_usesNamespaces;
          fr->isCommonFunction = ctx->isSharedFunctions;

          lstrcpyn_safe(fr->fname,is_fname,sizeof(fr->fname));

          if (ctx->isSharedFunctions)
          {
            fr->next = ctx->functions_common;
            ctx->functions_common = fr;
          }
          else
          {
            fr->next = ctx->functions_local;
            ctx->functions_local = fr;
          }         
        }
        continue;
      }

#ifdef DUMP_OPS_DURING_COMPILE
      g_debugfp_indent=0;
      g_debugfp_histsz=0;
      g_debugfp = fopen("C:/temp/foo.txt","w");
#endif
      startptr_size = compileOpcodes(ctx,start_opcode,NULL,1024*1024*256,NULL, NULL, 
        is_fname[0] ? (RETURNVALUE_NORMAL|RETURNVALUE_FPSTACK) : RETURNVALUE_IGNORE, &rvMode, &fUse, NULL); // if not a function, force return value as address (avoid having to pop it ourselves
                                          // if a function, allow the code to decide how return values are generated

#ifdef DUMP_OPS_DURING_COMPILE
      if (g_debugfp) fclose(g_debugfp);
      g_debugfp=0;
#endif


      if (!startptr_size) continue; // optimized away
      if (startptr_size>0)
      {
        startptr = newTmpBlock(ctx,startptr_size);
        if (startptr)
        {
          startptr_size=compileOpcodes(ctx,start_opcode,(unsigned char*)startptr,startptr_size,&computTableTop, NULL, RETURNVALUE_IGNORE, NULL,NULL, NULL);
          if (startptr_size<=0) startptr = NULL;
          
        }
      }
    }

    if (!startptr) 
    {  
had_error:
#ifdef NSEEL_EEL1_COMPAT_MODE
      continue;

#else
      //if (!ctx->last_error_string[0])
      {
        int byteoffs = ctx->errVar;
        int linenumber;
        char cur_err[sizeof(ctx->last_error_string)];
        lstrcpyn_safe(cur_err,ctx->last_error_string,sizeof(cur_err));
        if (cur_err[0]) lstrcatn(cur_err,": ",sizeof(cur_err));
        else lstrcpyn_safe(cur_err,"syntax error: ",sizeof(cur_err));

        if (_expression + byteoffs >= _expression_end) 
        {
          if (ctx->gotEndOfInput&4) byteoffs = (int)(expr-_expression);
          else byteoffs=(int)(_expression_end-_expression);
        }

        if (byteoffs < 0) byteoffs=0;

        linenumber=findLineNumber(_expression,byteoffs)+1;

        if (ctx->gotEndOfInput&4)
        {
          snprintf(ctx->last_error_string,sizeof(ctx->last_error_string),"%d: %smissing ) or ]",linenumber+lineoffs,cur_err);
        }
        else
        {
          const char *p = _expression + byteoffs;
          int x=0, right_amt_nospace=0, left_amt_nospace=0;
          while (x < 32 && p-x > _expression && p[-x] != '\r' && p[-x] != '\n') 
          {
            if (!isspace(p[-x])) left_amt_nospace=x;
            x++;
          }
          x=0;
          while (x < 60 && p[x] && p[x] != '\r' && p[x] != '\n') 
          {
            if (!isspace(p[x])) right_amt_nospace=x;
            x++;
          }

          if (right_amt_nospace<1) right_amt_nospace=1;

          // display left_amt >>>> right_amt_nospace
          if (left_amt_nospace > 0)
            snprintf(ctx->last_error_string,sizeof(ctx->last_error_string),"%d: %s'%.*s <!> %.*s'",linenumber+lineoffs,cur_err,
              left_amt_nospace,p-left_amt_nospace,
              right_amt_nospace,p);
          else
            snprintf(ctx->last_error_string,sizeof(ctx->last_error_string),"%d: %s'%.*s'",linenumber+lineoffs,cur_err,right_amt_nospace,p);
        }
      }

      startpts=NULL;
      startpts_tail=NULL; 
      had_err=1;
      break; 
#endif
    }
    
    if (!is_fname[0]) // redundant check (if is_fname[0] is set and we succeeded, it should continue)
                      // but we'll be on the safe side
    {
      topLevelCodeSegmentRec *p = newTmpBlock(ctx,sizeof(topLevelCodeSegmentRec));
      p->_next=0;
      p->code = startptr;
      p->codesz = startptr_size;
      p->tmptable_use = computTableTop;
                  
      if (!startpts_tail) startpts_tail=startpts=p;
      else
      {
        startpts_tail->_next=p;
        startpts_tail=p;
      }

      if (curtabptr_sz < computTableTop)
      {
        curtabptr_sz=computTableTop;
      }
    }
  }

  memset(ctx->function_localTable_Size,0,sizeof(ctx->function_localTable_Size));
  memset(ctx->function_localTable_Names,0,sizeof(ctx->function_localTable_Names));
  ctx->function_localTable_ValuePtrs=0;
  ctx->function_usesNamespaces=0;
  ctx->function_curName=NULL;
  ctx->function_globalFlag=0;

  ctx->tmpCodeHandle = NULL;
    
  if (handle->want_stack)
  {
    if (!handle->stack) startpts=NULL;
  }

  if (startpts) 
  {
    curtabptr_sz += 2; // many functions use the worktable for temporary storage of up to 2 EEL_F's

    handle->workTable_size = curtabptr_sz;
    handle->workTable = curtabptr = newDataBlock((curtabptr_sz+MIN_COMPUTABLE_SIZE + COMPUTABLE_EXTRA_SPACE) * sizeof(EEL_F),32);

#ifdef EEL_VALIDATE_WORKTABLE_USE
    if (curtabptr) memset(curtabptr,0x3a,(curtabptr_sz+MIN_COMPUTABLE_SIZE + COMPUTABLE_EXTRA_SPACE) * sizeof(EEL_F));
#endif
    if (!curtabptr) startpts=NULL;
  }


  if (startpts || (!had_err && (compile_flags & NSEEL_CODE_COMPILE_FLAG_COMMONFUNCS)))
  {
    unsigned char *writeptr;
    topLevelCodeSegmentRec *p=startpts;
    int size=sizeof(GLUE_RET)+GLUE_FUNC_ENTER_SIZE+GLUE_FUNC_LEAVE_SIZE; // for ret at end :)
    int wtpos=0;

    // now we build one big code segment out of our list of them, inserting a mov esi, computable before each item as necessary
    while (p)
    {
      if (wtpos <= 0)
      {
        wtpos=MIN_COMPUTABLE_SIZE;
        size += GLUE_RESET_WTP(NULL,0);
      }
      size+=p->codesz;
      wtpos -= p->tmptable_use;
      p=p->_next;
    }
    handle->code = newCodeBlock(size,32);
    if (handle->code)
    {
      writeptr=(unsigned char *)handle->code;
      #if GLUE_FUNC_ENTER_SIZE > 0
        memcpy(writeptr,&GLUE_FUNC_ENTER,GLUE_FUNC_ENTER_SIZE); 
        writeptr += GLUE_FUNC_ENTER_SIZE;
      #endif
      p=startpts;
      wtpos=0;
      while (p)
      {
        if (wtpos <= 0)
        {
          wtpos=MIN_COMPUTABLE_SIZE;
          writeptr+=GLUE_RESET_WTP(writeptr,curtabptr);
        }
        memcpy(writeptr,(char*)p->code,p->codesz);
        writeptr += p->codesz;
        wtpos -= p->tmptable_use;
      
        p=p->_next;
      }
      #if GLUE_FUNC_LEAVE_SIZE > 0
        memcpy(writeptr,&GLUE_FUNC_LEAVE,GLUE_FUNC_LEAVE_SIZE); 
        writeptr += GLUE_FUNC_LEAVE_SIZE;
      #endif
      memcpy(writeptr,&GLUE_RET,sizeof(GLUE_RET)); writeptr += sizeof(GLUE_RET);
      ctx->l_stats[1]=size;
      handle->code_size = (int) (writeptr - (unsigned char *)handle->code);
#if defined(__arm__) || defined(__aarch64__)
      __clear_cache(handle->code,writeptr);
#endif
    }
    
    handle->blocks = ctx->blocks_head;
    handle->blocks_data = ctx->blocks_head_data;
    ctx->blocks_head=0;
    ctx->blocks_head_data=0;
  }
  else
  {
    // failed compiling, or failed calloc()
    handle=NULL;              // return NULL (after resetting blocks_head)
  }


  ctx->directValueCache=0;
  ctx->functions_local = NULL;
  
  ctx->isGeneratingCommonFunction=0;
  ctx->isSharedFunctions=0;

  freeBlocks(&ctx->tmpblocks_head);  // free blocks
  freeBlocks(&ctx->blocks_head);  // free blocks of code (will be nonzero only on error)
  freeBlocks(&ctx->blocks_head_data);  // free blocks of data (will be nonzero only on error)

  if (handle)
  {
    handle->compile_flags = compile_flags;
    handle->ramPtr = ctx->ram_state.blocks;
    memcpy(handle->code_stats,ctx->l_stats,sizeof(ctx->l_stats));
    nseel_evallib_stats[0]+=ctx->l_stats[0];
    nseel_evallib_stats[1]+=ctx->l_stats[1];
    nseel_evallib_stats[2]+=ctx->l_stats[2];
    nseel_evallib_stats[3]+=ctx->l_stats[3];
    nseel_evallib_stats[4]++;
  }
  else
  {
    ctx->functions_common = oldCommonFunctionList; // failed compiling, remove any added common functions from the list

    // remove any derived copies of functions due to error, since we may have added some that have been freed
    while (oldCommonFunctionList)
    {
      oldCommonFunctionList->derivedCopies=NULL;
      oldCommonFunctionList=oldCommonFunctionList->next;
    }
  }
  memset(ctx->l_stats,0,sizeof(ctx->l_stats));

  return (NSEEL_CODEHANDLE)handle;
}

//------------------------------------------------------------------------------
void NSEEL_code_execute(NSEEL_CODEHANDLE code)
{
#ifndef GLUE_TABPTR_IGNORED
  INT_PTR tabptr;
#endif
  INT_PTR codeptr;
  codeHandleType *h = (codeHandleType *)code;
  if (!h || !h->code) return;

  codeptr = (INT_PTR) h->code;
#if 0
  {
	unsigned int *p=(unsigned int *)codeptr;
	while (*p != GLUE_RET[0])
	{
		printf("instr:%04X:%04X\n",*p>>16,*p&0xffff);
		p++;
	}
  }
#endif

#ifndef GLUE_TABPTR_IGNORED
  tabptr=(INT_PTR)h->workTable;
#endif
  //printf("calling code!\n");
  GLUE_CALL_CODE(tabptr,codeptr,(INT_PTR)h->ramPtr);

}

int NSEEL_code_geterror_flag(NSEEL_VMCTX ctx)
{
  compileContext *c=(compileContext *)ctx;
  if (c) return (c->gotEndOfInput ? 1 : 0);
  return 0;
}

char *NSEEL_code_getcodeerror(NSEEL_VMCTX ctx)
{
  compileContext *c=(compileContext *)ctx;
  if (ctx && c->last_error_string[0]) return c->last_error_string;
  return 0;
}

//------------------------------------------------------------------------------
void NSEEL_code_free(NSEEL_CODEHANDLE code)
{
  codeHandleType *h = (codeHandleType *)code;
  if (h != NULL)
  {
#ifdef EEL_VALIDATE_WORKTABLE_USE
    if (h->workTable)
    {
      char *p = ((char*)h->workTable) + h->workTable_size*sizeof(EEL_F);
      int x;
      for(x=COMPUTABLE_EXTRA_SPACE*sizeof(EEL_F) - 1;x >= 0; x --)
        if (p[x] != 0x3a)
        {
          char buf[512];
          snprintf(buf,sizeof(buf),"worktable overrun at byte %d (wts=%d), value = %f\n",x,h->workTable_size, *(EEL_F*)(p+(x&~(sizeof(EEL_F)-1))));
#ifdef _WIN32
          OutputDebugString(buf);
#else
          printf("%s",buf);
#endif
          break;
        }
    }
#endif

    nseel_evallib_stats[0]-=h->code_stats[0];
    nseel_evallib_stats[1]-=h->code_stats[1];
    nseel_evallib_stats[2]-=h->code_stats[2];
    nseel_evallib_stats[3]-=h->code_stats[3];
    nseel_evallib_stats[4]--;

#if defined(__ppc__) && defined(__APPLE__)
    {
      FILE *fp = fopen("/var/db/receipts/com.apple.pkg.Rosetta.plist","r");
      if (fp) 
      {
        fclose(fp);
        // on PPC, but rosetta installed, do not free h->blocks, as rosetta won't detect changes to these pages
      }
      else
      {
        freeBlocks(&h->blocks);
      }
    }
#else
  freeBlocks(&h->blocks);
#endif
    
    freeBlocks(&h->blocks_data);
  }

}

//------------------------------------------------------------------------------

NSEEL_VMCTX NSEEL_VM_alloc() // return a handle
{
  compileContext *ctx=calloc(1,sizeof(compileContext));

  #ifdef NSEEL_SUPER_MINIMAL_LEXER
    if (ctx) ctx->scanner = ctx;
  #else
    if (ctx)
    {
      int nseellex_init(void ** ptr_yy_globals);
      void nseelset_extra(void *user_defined , void *yyscanner);
      if (nseellex_init(&ctx->scanner))
      {
        free(ctx);
        return NULL;
      }
      nseelset_extra(ctx,ctx->scanner);
    }
  #endif

  if (ctx) 
  {
    ctx->ram_state.maxblocks = NSEEL_RAM_BLOCKS_DEFAULTMAX;
    ctx->ram_state.closefact = NSEEL_CLOSEFACTOR;
  }
  return ctx;
}

int NSEEL_VM_setramsize(NSEEL_VMCTX _ctx, int maxent)
{
  compileContext *ctx = (compileContext *)_ctx;
  if (!ctx) return 0;
  if (maxent > 0)
  {
    maxent = (maxent + NSEEL_RAM_ITEMSPERBLOCK - 1)/NSEEL_RAM_ITEMSPERBLOCK;
    if (maxent > NSEEL_RAM_BLOCKS) maxent = NSEEL_RAM_BLOCKS;
    ctx->ram_state.maxblocks = maxent;
  }
  
  return ctx->ram_state.maxblocks * NSEEL_RAM_ITEMSPERBLOCK;
}

void NSEEL_VM_SetFunctionValidator(NSEEL_VMCTX _ctx, const char * (*validateFunc)(const char *fn_name, void *user), void *user)
{
  if (_ctx)
  {
    compileContext *ctx = (compileContext *)_ctx;
    ctx->func_check = validateFunc;
    ctx->func_check_user = user;
  }
}

void NSEEL_VM_SetFunctionTable(NSEEL_VMCTX _ctx, eel_function_table *tab)
{
  if (_ctx)
  {
    compileContext *ctx = (compileContext *)_ctx;
    ctx->registered_func_tab = tab;
  }
}
void NSEEL_VM_free(NSEEL_VMCTX _ctx) // free when done with a VM and ALL of its code have been freed, as well
{

  if (_ctx)
  {
    compileContext *ctx=(compileContext *)_ctx;
    EEL_GROWBUF_RESIZE(&ctx->varNameList,-1);
    NSEEL_VM_freeRAM(_ctx);

    freeBlocks(&ctx->pblocks);

    // these should be 0 normally but just in case
    freeBlocks(&ctx->tmpblocks_head);  // free blocks
    freeBlocks(&ctx->blocks_head);  // free blocks
    freeBlocks(&ctx->blocks_head_data);  // free blocks


    #ifndef NSEEL_SUPER_MINIMAL_LEXER
      if (ctx->scanner)
      {
       int nseellex_destroy(void *yyscanner);
       nseellex_destroy(ctx->scanner);
      }
    #endif
    ctx->scanner=0;
    if (ctx->has_used_global_vars)
    {
      nseel_globalVarItem *p = NULL;
      NSEEL_HOSTSTUB_EnterMutex();
      if (--nseel_vms_referencing_globallist_cnt == 0)
      {
        // clear and free globals
        p = nseel_globalreg_list;
        nseel_globalreg_list=0;
      }
      NSEEL_HOSTSTUB_LeaveMutex();

      while (p)
      {
        nseel_globalVarItem *op = p;
        p=p->_next;
        free(op);
      }
    }
    free(ctx);
  }

}

int *NSEEL_code_getstats(NSEEL_CODEHANDLE code)
{
  codeHandleType *h = (codeHandleType *)code;
  if (h)
  {
    return h->code_stats;
  }
  return 0;
}

void NSEEL_VM_SetStringFunc(NSEEL_VMCTX ctx, 
    EEL_F (*onString)(void *caller_this, struct eelStringSegmentRec *list),
    EEL_F (*onNamedString)(void *caller_this, const char *name))
{
  if (ctx)
  {
    compileContext *c=(compileContext*)ctx;
    c->onString = onString;
    c->onNamedString = onNamedString;
  }
}

void NSEEL_VM_SetCustomFuncThis(NSEEL_VMCTX ctx, void *thisptr)
{
  if (ctx)
  {
    compileContext *c=(compileContext*)ctx;
    c->caller_this=thisptr;
  }
}





void *NSEEL_PProc_RAM(void *data, int data_size, compileContext *ctx)
{
  if (data_size>0) data=EEL_GLUE_set_immediate(data, (INT_PTR)ctx->ram_state.blocks); 
  return data;
}

void *NSEEL_PProc_THIS(void *data, int data_size, compileContext *ctx)
{
  if (data_size>0) data=EEL_GLUE_set_immediate(data, (INT_PTR)ctx->caller_this);
  return data;
}

static int vartable_lowerbound(compileContext *ctx, const char *name, int *ismatch)
{
  int a = 0, c = EEL_GROWBUF_GET_SIZE(&ctx->varNameList);
  varNameRec **list = EEL_GROWBUF_GET(&ctx->varNameList);
  while (a != c)
  {
    const int b = (a+c)/2;
    const int cmp = strnicmp(name,list[b]->str,NSEEL_MAX_VARIABLE_NAMELEN);
    if (cmp > 0) a = b+1;
    else if (cmp < 0) c = b;
    else
    {
      *ismatch = 1;
      return b;
    }
  }
  *ismatch = 0;
  return a;
}

static void vartable_cull_list(compileContext *ctx, int refcnt_chk)
{
  const int ni = EEL_GROWBUF_GET_SIZE(&ctx->varNameList);
  int i = ni, ndel = 0;
  varNameRec **rd = EEL_GROWBUF_GET(&ctx->varNameList), **wr=rd;
  while (i--)
  {
    varNameRec *v = rd[0];
    if ((!refcnt_chk || !v->refcnt) && !v->isreg) 
    {
      ndel++;
    }
    else
    {
      if (wr != rd) *wr = *rd;
      wr++;
    }
    rd++;
  }
  if (ndel) EEL_GROWBUF_RESIZE(&ctx->varNameList,ni - ndel);
}

void NSEEL_VM_remove_unused_vars(NSEEL_VMCTX _ctx)
{
  compileContext *ctx = (compileContext *)_ctx;
  if (ctx) vartable_cull_list(ctx,1);
}

void NSEEL_VM_remove_all_nonreg_vars(NSEEL_VMCTX _ctx)
{
  compileContext *ctx = (compileContext *)_ctx;
  if (ctx) vartable_cull_list(ctx,0);
}

void NSEEL_VM_clear_var_refcnts(NSEEL_VMCTX _ctx)
{
  compileContext *ctx = (compileContext *)_ctx;
  if (ctx)
  {
    int i = EEL_GROWBUF_GET_SIZE(&ctx->varNameList);
    varNameRec **rd = EEL_GROWBUF_GET(&ctx->varNameList);
    while (i--)
    {
      rd[0]->refcnt=0;
      rd++;
    }
  }
}


#ifdef NSEEL_EEL1_COMPAT_MODE
static EEL_F __nseel_global_regs[100];
double *NSEEL_getglobalregs() { return __nseel_global_regs; }
#endif

EEL_F *get_global_var(compileContext *ctx, const char *gv, int addIfNotPresent)
{
  nseel_globalVarItem *p;
#ifdef NSEEL_EEL1_COMPAT_MODE
  if (!strnicmp(gv,"reg",3) && gv[3]>='0' && gv[3] <= '9' && gv[4] >= '0' && gv[4] <= '9' && !gv[5])
  {
    return __nseel_global_regs + atoi(gv+3);
  }
#endif

  NSEEL_HOSTSTUB_EnterMutex(); 
  if (!ctx->has_used_global_vars)
  {
    ctx->has_used_global_vars++;
    nseel_vms_referencing_globallist_cnt++;
  }

  p = nseel_globalreg_list;
  while (p)
  {
    if (!stricmp(p->name,gv)) break;
    p=p->_next;
  }

  if (!p && addIfNotPresent)
  {
    size_t gvl = strlen(gv);
    p = (nseel_globalVarItem*)malloc(sizeof(nseel_globalVarItem) + gvl);
    if (p)
    {
      p->data=0.0;
      strcpy(p->name,gv);
      p->_next = nseel_globalreg_list;
      nseel_globalreg_list=p;
    }
  }
  NSEEL_HOSTSTUB_LeaveMutex(); 
  return p ? &p->data : NULL;
}



EEL_F *nseel_int_register_var(compileContext *ctx, const char *name, int isReg, const char **namePtrOut)
{
  int slot, match;

  if (isReg == 0 && ctx->getVariable)
  {
    EEL_F *ret = ctx->getVariable(ctx->getVariable_userctx, name);
    if (ret) return ret;
  }

  if (!strnicmp(name,"_global.",8) && name[8])
  {
    EEL_F *a=get_global_var(ctx,name+8,isReg >= 0);
    if (a) return a;
  }

  slot = vartable_lowerbound(ctx,name, &match);
  if (match)
  {
    varNameRec *v = EEL_GROWBUF_GET(&ctx->varNameList)[slot];
    if (isReg >= 0)
    {
      v->refcnt++;
      if (isReg) v->isreg=isReg;
      if (namePtrOut) *namePtrOut = v->str;
    }
    return v->value;
  }
  if (isReg < 0) return NULL;

  if (ctx->varValueStore_left<1)
  {
    const int sz=500;
    ctx->varValueStore_left = sz;
    ctx->varValueStore = (EEL_F *)newCtxDataBlock((int)sizeof(EEL_F)*sz,8);
  }
  if (ctx->varValueStore)
  {
    int listsz = EEL_GROWBUF_GET_SIZE(&ctx->varNameList);
    size_t l = strlen(name);
    varNameRec *vh;
    if (l > NSEEL_MAX_VARIABLE_NAMELEN) l = NSEEL_MAX_VARIABLE_NAMELEN;
    vh = (varNameRec*) newCtxDataBlock( (int) (sizeof(varNameRec) + l),8);
    if (!vh || EEL_GROWBUF_RESIZE(&ctx->varNameList, (listsz+1))) return NULL; // alloc fail

    (vh->value = ctx->varValueStore++)[0]=0.0;
    ctx->varValueStore_left--;

    vh->refcnt=1;
    vh->isreg=isReg;
    memcpy(vh->str,name,l);
    vh->str[l] = 0;
    if (namePtrOut) *namePtrOut = vh->str;

    if (slot < listsz)
    {
      memmove(EEL_GROWBUF_GET(&ctx->varNameList) + slot+1, 
              EEL_GROWBUF_GET(&ctx->varNameList) + slot, (listsz - slot) * sizeof(EEL_GROWBUF_GET(&ctx->varNameList)[0]));
    }
    EEL_GROWBUF_GET(&ctx->varNameList)[slot] = vh;

    return vh->value;
  }
  return NULL;
}


//------------------------------------------------------------------------------

void NSEEL_VM_enumallvars(NSEEL_VMCTX ctx, int (*func)(const char *name, EEL_F *val, void *ctx), void *userctx)
{
  compileContext *tctx = (compileContext *) ctx;
  int ni;
  varNameRec **rd;
  if (!tctx) return;
  
  ni = EEL_GROWBUF_GET_SIZE(&tctx->varNameList);
  rd = EEL_GROWBUF_GET(&tctx->varNameList);
  while (ni--)
  {
    if (!func(rd[0]->str,rd[0]->value,userctx)) break;
    rd++;
  }
}


//------------------------------------------------------------------------------
EEL_F *NSEEL_VM_regvar(NSEEL_VMCTX _ctx, const char *var)
{
  compileContext *ctx = (compileContext *)_ctx;
  if (!ctx) return 0;
  
  if (!strnicmp(var,"reg",3) && strlen(var) == 5 && isdigit(var[3]) && isdigit(var[4]))
  {
    EEL_F *a=get_global_var(ctx,var,1);
    if (a) return a;
  }
  
  return nseel_int_register_var(ctx,var,1,NULL);
}

EEL_F *NSEEL_VM_getvar(NSEEL_VMCTX _ctx, const char *var)
{
  compileContext *ctx = (compileContext *)_ctx;
  if (!ctx) return 0;
  
  if (!strnicmp(var,"reg",3) && strlen(var) == 5 && isdigit(var[3]) && isdigit(var[4]))
  {
    EEL_F *a=get_global_var(ctx,var,0);
    if (a) return a;
  }
  
  return nseel_int_register_var(ctx,var,-1,NULL);
}

int  NSEEL_VM_get_var_refcnt(NSEEL_VMCTX _ctx, const char *name)
{
  compileContext *ctx = (compileContext *)_ctx;
  int slot,match;
  if (!ctx) return -1;
  slot = vartable_lowerbound(ctx,name, &match);
  return match ? EEL_GROWBUF_GET(&ctx->varNameList)[slot]->refcnt : -1;
}




opcodeRec *nseel_createFunctionByName(compileContext *ctx, const char *name, int np, opcodeRec *code1, opcodeRec *code2, opcodeRec *code3)
{
  int chkamt=0;
  functionType *f=nseel_getFunctionByName(ctx,name,&chkamt);
  if (f) while (chkamt-->=0)
  {
    if ((f->nParams&FUNCTIONTYPE_PARAMETERCOUNTMASK) == np)
    {
      opcodeRec *o=newOpCode(ctx,NULL, np==3?OPCODETYPE_FUNC3:np==2?OPCODETYPE_FUNC2:OPCODETYPE_FUNC1);
      if (o) 
      {
        o->fntype = FUNCTYPE_FUNCTIONTYPEREC;
        o->fn = f;
        o->parms.parms[0]=code1;
        o->parms.parms[1]=code2;
        o->parms.parms[2]=code3;
      }
      return o;
    }
    f++;
    if (stricmp(f->name,name)) break;
  }
  return NULL;
}




//------------------------------------------------------------------------------
opcodeRec *nseel_translate(compileContext *ctx, const char *tmp, size_t tmplen) // tmplen 0 = null term
{
  // this depends on the string being nul terminated eventually, tmplen is used more as a hint than anything else
  if ((tmp[0] == '0' || tmp[0] == '$') && toupper(tmp[1])=='X')
  {
    char *p;
    return nseel_createCompiledValue(ctx,(EEL_F)strtoul(tmp+2,&p,16));
  }
  else if (tmp[0] == '$')
  {
    if (tmp[1] == '~')
    {
      char *p=(char*)tmp+2;
      unsigned int v=(unsigned int) strtoul(tmp+2,&p,10);
      if (v>53) v=53;
      return nseel_createCompiledValue(ctx,(EEL_F)((((WDL_INT64)1) << v) - 1));
    }
    else if (!tmplen ? !stricmp(tmp,"$E") : (tmplen == 2 && !strnicmp(tmp,"$E",2)))
      return nseel_createCompiledValue(ctx,(EEL_F)2.71828183);
    else if (!tmplen ? !stricmp(tmp, "$PI") : (tmplen == 3 && !strnicmp(tmp, "$PI", 3)))
      return nseel_createCompiledValue(ctx,(EEL_F)3.141592653589793);
    else if (!tmplen ? !stricmp(tmp, "$PHI") : (tmplen == 4 && !strnicmp(tmp, "$PHI", 4)))
      return nseel_createCompiledValue(ctx,(EEL_F)1.61803399);      
    else if ((!tmplen || tmplen == 4) && tmp[1] == '\'' && tmp[2] && tmp[3] == '\'')
      return nseel_createCompiledValue(ctx,(EEL_F)tmp[2]);      
    else return NULL;
  }
  else if (tmp[0] == '\'')
  {
    char b[64];
    int x,sz;
    unsigned int rv=0;

    if (!tmplen) // nul terminated tmplen, calculate a workable length
    {
      // faster than strlen(tmp) if tmp is large, we'll never need more than ~18 chars anyway
      while (tmplen < 32 && tmp[tmplen]) tmplen++;
    }
    
    sz = tmplen > 0 ? nseel_filter_escaped_string(b,sizeof(b),tmp+1, tmplen - 1, '\'') : 0;
        
    if (sz > 4) 
    {
      if (ctx->last_error_string[0]) lstrcatn(ctx->last_error_string, ", ", sizeof(ctx->last_error_string));
      snprintf_append(ctx->last_error_string,sizeof(ctx->last_error_string),"multi-byte character '%.5s...' too long",b);
      return NULL; // do not allow 'xyzxy', limit to 4 bytes
    }

    for (x=0;x<sz;x++) rv = (rv<<8) + ((unsigned char*)b)[x];
    return nseel_createCompiledValue(ctx,(EEL_F)rv);
  }
  else if (tmp[0] == '#')
  {
    char buf[2048];
    if (!tmplen) while (tmplen < sizeof(buf)-1 && tmp[tmplen]) tmplen++;
    else if (tmplen > sizeof(buf)-1) tmplen = sizeof(buf)-1;
    memcpy(buf,tmp,tmplen);
    buf[tmplen]=0;
    if (ctx->onNamedString) 
    {
      if (tmplen>0 && buf[1]&&ctx->function_curName)
      {
        int err=0;
        opcodeRec *r = nseel_resolve_named_symbol(ctx,nseel_createCompiledValuePtr(ctx,NULL,buf),-1, &err);
        if (r)
        {
          if (r->opcodeType!=OPCODETYPE_VALUE_FROM_NAMESPACENAME) 
          {
            r->opcodeType = OPCODETYPE_DIRECTVALUE;
            r->parms.dv.directValue = ctx->onNamedString(ctx->caller_this,buf+1);
            r->parms.dv.valuePtr=NULL;
          }
          return r;
        }
        if (err) return NULL;
      }

      // if not namespaced symbol, return directly
      if (!buf[1])
      {
        opcodeRec *r=newOpCode(ctx,NULL,OPCODETYPE_DIRECTVALUE_TEMPSTRING);
        if (r) r->parms.dv.directValue = -10000.0;
        return r;
      }
      return nseel_createCompiledValue(ctx,ctx->onNamedString(ctx->caller_this,buf+1));
    }
  }
  return nseel_createCompiledValue(ctx,(EEL_F)atof(tmp));
}

void NSEEL_VM_set_var_resolver(NSEEL_VMCTX _ctx, EEL_F *(*res)(void *userctx, const char *name), void *userctx)
{
  compileContext *ctx = (compileContext *)_ctx;
  if (ctx)
  {
    ctx->getVariable = res;
    ctx->getVariable_userctx = userctx;
  }
}


#if defined(__ppc__) || defined(EEL_TARGET_PORTABLE)
  // blank stubs 
  void eel_setfp_round() { }
  void eel_setfp_trunc() { }
  void eel_enterfp(int s[2]) {}
  void eel_leavefp(int s[2]) {}
#endif

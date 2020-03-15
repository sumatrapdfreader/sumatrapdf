/*
  Nullsoft Expression Evaluator Library (NS-EEL)
  Copyright (C) 1999-2003 Nullsoft, Inc.
  
  ns-eel-int.h: internal code definition header.

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

#ifndef __NS_EELINT_H__
#define __NS_EELINT_H__

#ifdef _WIN32
#include <windows.h>
#else
#include "../wdltypes.h"
#endif

#include "ns-eel.h"
#include "ns-eel-addfuncs.h"

#ifdef __cplusplus
extern "C" {
#endif


enum { 

  // these ignore fn in opcodes, just use fntype to determine function
  FN_MULTIPLY=0,
  FN_DIVIDE,
  FN_JOIN_STATEMENTS,
  FN_DENORMAL_LIKELY,
  FN_DENORMAL_UNLIKELY,
  FN_ADD,
  FN_SUB,
  FN_AND,
  FN_OR,
  FN_UMINUS,
  FN_NOT,
  FN_NOTNOT,
  FN_XOR,
  FN_SHL,
  FN_SHR,
  FN_MOD,
  FN_POW,
  FN_LT,
  FN_GT,
  FN_LTE,
  FN_GTE,
  FN_EQ,
  FN_EQ_EXACT,
  FN_NE,
  FN_NE_EXACT,
  FN_LOGICAL_AND,
  FN_LOGICAL_OR,
  FN_IF_ELSE,
  FN_MEMORY,
  FN_GMEMORY,
  FN_NONCONST_BEGIN,
  FN_ASSIGN=FN_NONCONST_BEGIN,

  FN_ADD_OP,
  FN_SUB_OP,
  FN_MOD_OP,
  FN_OR_OP,
  FN_AND_OP,
  FN_XOR_OP,
  FN_DIV_OP,
  FN_MUL_OP,
  FN_POW_OP,

  FN_WHILE,
  FN_LOOP,

  FUNCTYPE_SIMPLEMAX,


  FUNCTYPE_FUNCTIONTYPEREC=1000, // fn is a functionType *
  FUNCTYPE_EELFUNC, // fn is a _codeHandleFunctionRec *
};



#define YYSTYPE opcodeRec *

#define NSEEL_CLOSEFACTOR 0.00001
 
  
typedef struct opcodeRec opcodeRec;

typedef struct _codeHandleFunctionRec 
{
  struct _codeHandleFunctionRec *next; // main linked list (only used for high level functions)
  struct _codeHandleFunctionRec *derivedCopies; // separate linked list, head being the main function, other copies being derived versions

  void *startptr; // compiled code (may be cleared + recompiled when shared)
  opcodeRec *opcodes;

  int startptr_size;  // 0=no code. -1 = needs calculation. >0 = size.
  int tmpspace_req;
    
  int num_params;

  int rvMode; // RETURNVALUE_*
  int fpStackUsage; // 0-8, usually
  int canHaveDenormalOutput;

  // local storage's first items are the parameters, then locals. Note that the opcodes will reference localstorage[] via VARPTRPTR, but 
  // the values localstorage[x] points are reallocated from context-to-context, if it is a common function.

  // separately allocated list of pointers, the contents of the list should be zeroed on context changes if a common function
  // note that when making variations on a function (context), it is shared, but since it is zeroed on context changes, it is context-local
  int localstorage_size;
  EEL_F **localstorage; 

  int isCommonFunction;
  int usesNamespaces;
  unsigned int parameterAsNamespaceMask;

  char fname[NSEEL_MAX_FUNCSIG_NAME+1];
} _codeHandleFunctionRec;  
  
#define LLB_DSIZE (65536-64)
typedef struct _llBlock {
  struct _llBlock *next;
  int sizeused;
  char block[LLB_DSIZE];
} llBlock;

typedef struct {
  llBlock *blocks, 
          *blocks_data;
  void *workTable; // references a chunk in blocks_data

  void *code;
  int code_size; // in case the caller wants to write it out
  int code_stats[4];

  int want_stack;
  void *stack;  // references a chunk in blocks_data, somewhere within the complete NSEEL_STACK_SIZE aligned at NSEEL_STACK_SIZE

  void *ramPtr;

  int workTable_size; // size (minus padding/extra space) of workTable -- only used if EEL_VALIDATE_WORKTABLE_USE set, but might be handy to have around too
  int compile_flags;
} codeHandleType;

typedef struct
{
  EEL_F *value;
  int refcnt;
  char isreg;
  char str[1];
} varNameRec;

typedef struct 
{
  void *ptr;
  int size, alloc;
} eel_growbuf;
#define EEL_GROWBUF(type) union { eel_growbuf _growbuf; type *_tval; }
#define EEL_GROWBUF_RESIZE(gb, newsz) __growbuf_resize(&(gb)->_growbuf, (newsz)*(int)sizeof((gb)->_tval[0])) // <0 to free, does not realloc down otherwise
#define EEL_GROWBUF_GET(gb) ((gb)->_tval)
#define EEL_GROWBUF_GET_SIZE(gb) ((gb)->_growbuf.size/(int)sizeof((gb)->_tval[0]))

typedef struct _compileContext
{
  eel_function_table *registered_func_tab;
  const char *(*func_check)(const char *fn_name, void *user); // return error message if not permitted
  void *func_check_user;

  EEL_GROWBUF(varNameRec *) varNameList;
  EEL_F *varValueStore;
  int varValueStore_left;

  int errVar,gotEndOfInput;
  opcodeRec *result;
  char last_error_string[256];

  void *scanner;
  const char *rdbuf_start, *rdbuf, *rdbuf_end;

  llBlock *tmpblocks_head, // used while compiling, and freed after compiling

          *blocks_head,  // used while compiling, transferred to code context (these are pages marked as executable)
          *blocks_head_data, // used while compiling, transferred to code context

          *pblocks; // persistent blocks, stores data used by varTable_Names, varTable_Values, etc.

  int l_stats[4]; // source bytes, static code bytes, call code bytes, data bytes
  int has_used_global_vars;

  _codeHandleFunctionRec *functions_local, *functions_common;

  // state used while generating functions
  int optimizeDisableFlags;
  int current_compile_flags;
  struct opcodeRec *directValueCache; // linked list using fn as next

  int isSharedFunctions;
  int isGeneratingCommonFunction;
  int function_usesNamespaces;
  int function_globalFlag; // set if restrict globals to function_localTable_Names[2]
  // [0] is parameter+local symbols (combined space)
  // [1] is symbols which get implied "this." if used
  // [2] is globals permitted
  int function_localTable_Size[3]; // for parameters only
  char **function_localTable_Names[3]; // lists of pointers
  EEL_F **function_localTable_ValuePtrs;
  const char *function_curName; // name of current function

  EEL_F (*onString)(void *caller_this, struct eelStringSegmentRec *list);
  EEL_F (*onNamedString)(void *caller_this, const char *name);

  EEL_F *(*getVariable)(void *userctx, const char *name);
  void *getVariable_userctx;

  codeHandleType *tmpCodeHandle;
  
  struct
  {
    int needfree;
    int maxblocks;
    double closefact;
    EEL_F *blocks[NSEEL_RAM_BLOCKS];
  } ram_state
#ifdef __GNUC__
    __attribute__ ((aligned (8)))
#endif
   ;

  void *gram_blocks;

  void *caller_this;
}
compileContext;

#define NSEEL_NPARAMS_FLAG_CONST 0x80000
typedef struct functionType {
      const char *name;
      void *afunc;
      void *func_e;
      int nParams;
      void *replptrs[4];
      NSEEL_PPPROC pProc;
} functionType;

functionType *nseel_getFunctionByName(compileContext *ctx, const char *name, int *mchk); // sets mchk (if non-NULL) to how far allowed to scan forward for duplicate names

opcodeRec *nseel_createCompiledValue(compileContext *ctx, EEL_F value);
opcodeRec *nseel_createCompiledValuePtr(compileContext *ctx, EEL_F *addrValue, const char *namestr);

opcodeRec *nseel_createMoreParametersOpcode(compileContext *ctx, opcodeRec *code1, opcodeRec *code2);
opcodeRec *nseel_createSimpleCompiledFunction(compileContext *ctx, int fn, int np, opcodeRec *code1, opcodeRec *code2);
opcodeRec *nseel_createMemoryAccess(compileContext *ctx, opcodeRec *code1, opcodeRec *code2);
opcodeRec *nseel_createIfElse(compileContext *ctx, opcodeRec *code1, opcodeRec *code2, opcodeRec *code3);
opcodeRec *nseel_createFunctionByName(compileContext *ctx, const char *name, int np, opcodeRec *code1, opcodeRec *code2, opcodeRec *code3);

// converts a generic identifier (VARPTR) opcode into either an actual variable reference (parmcnt = -1),
// or if parmcnt >= 0, to a function call (see nseel_setCompiledFunctionCallParameters())
opcodeRec *nseel_resolve_named_symbol(compileContext *ctx, opcodeRec *rec, int parmcnt, int *errOut); 

// sets parameters and calculates parameter count for opcode, and calls nseel_resolve_named_symbol() with the right
// parameter count
opcodeRec *nseel_setCompiledFunctionCallParameters(compileContext *ctx, opcodeRec *fn, opcodeRec *code1, opcodeRec *code2, opcodeRec *code3, opcodeRec *postCode, int *errOut); 
// errOut will be set if return NULL:
// -1 if postCode set when not wanted (i.e. not while())
// 0 if func not found, 
// 1 if function requires 2+ parameters but was given more
// 2 if function needs more parameters
// 4 if function requires 1 parameter but was given more



struct eelStringSegmentRec *nseel_createStringSegmentRec(compileContext *ctx, const char *str, int len);
opcodeRec *nseel_eelMakeOpcodeFromStringSegments(compileContext *ctx, struct eelStringSegmentRec *rec);

EEL_F *nseel_int_register_var(compileContext *ctx, const char *name, int isReg, const char **namePtrOut);
_codeHandleFunctionRec *eel_createFunctionNamespacedInstance(compileContext *ctx, _codeHandleFunctionRec *fr, const char *nameptr);

typedef struct nseel_globalVarItem
{
  EEL_F data;
  struct nseel_globalVarItem *_next;
  char name[1]; // varlen, does not include _global. prefix
} nseel_globalVarItem;

extern nseel_globalVarItem *nseel_globalreg_list; // if NSEEL_EEL1_COMPAT_MODE, must use NSEEL_getglobalregs() for regxx values

#include "y.tab.h"

// nseel_simple_tokenizer will return comments as tokens if state is non-NULL
const char *nseel_simple_tokenizer(const char **ptr, const char *endptr, int *lenOut, int *state);
int nseel_filter_escaped_string(char *outbuf, int outbuf_sz, const char *rdptr, size_t rdptr_size, char delim_char); // returns length used, minus NUL char

opcodeRec *nseel_translate(compileContext *ctx, const char *tmp, size_t tmplen); // tmplen=0 for nul-term
int nseel_lookup(compileContext *ctx, opcodeRec **opOut, const char *sname);

EEL_F * NSEEL_CGEN_CALL __NSEEL_RAMAlloc(EEL_F **blocks, unsigned int w);
EEL_F * NSEEL_CGEN_CALL __NSEEL_RAMAllocGMEM(EEL_F ***blocks, unsigned int w);
EEL_F * NSEEL_CGEN_CALL __NSEEL_RAM_MemSet(EEL_F **blocks,EEL_F *dest, EEL_F *v, EEL_F *lenptr);
EEL_F * NSEEL_CGEN_CALL __NSEEL_RAM_MemFree(void *blocks, EEL_F *which);
EEL_F * NSEEL_CGEN_CALL __NSEEL_RAM_MemTop(void *blocks, EEL_F *which);
EEL_F * NSEEL_CGEN_CALL __NSEEL_RAM_MemCpy(EEL_F **blocks,EEL_F *dest, EEL_F *src, EEL_F *lenptr);
EEL_F NSEEL_CGEN_CALL __NSEEL_RAM_Mem_SetValues(EEL_F **blocks, INT_PTR np, EEL_F **parms);
EEL_F NSEEL_CGEN_CALL __NSEEL_RAM_Mem_GetValues(EEL_F **blocks, INT_PTR np, EEL_F **parms);

extern EEL_F nseel_ramalloc_onfail; // address returned by __NSEEL_RAMAlloc et al on failure
extern EEL_F * volatile  nseel_gmembuf_default; // can free/zero this on DLL unload if needed

#ifdef __cplusplus
}
#endif


#endif//__NS_EELINT_H__

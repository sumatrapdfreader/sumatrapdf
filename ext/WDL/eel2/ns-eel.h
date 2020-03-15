/*
  Nullsoft Expression Evaluator Library (NS-EEL)
  Copyright (C) 1999-2003 Nullsoft, Inc.
  
  ns-eel.h: main application interface header

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


#ifndef __NS_EEL_H__
#define __NS_EEL_H__

// put standard includes here
#include <stdlib.h>
#include <stdio.h>

#ifndef EEL_F_SIZE
#define EEL_F_SIZE 8
#endif

#include "../wdltypes.h"

#if EEL_F_SIZE == 4
typedef float EEL_F;
typedef float *EEL_F_PTR;
#else
typedef double EEL_F WDL_FIXALIGN;
typedef double *EEL_F_PTR;
#endif

#ifdef _MSC_VER
#define NSEEL_CGEN_CALL __cdecl
#else
#define NSEEL_CGEN_CALL 
#endif


#ifdef __cplusplus
extern "C" {
#endif

// host should implement these (can be empty stub functions if no VM will execute code in multiple threads at once)

  // implement if you will be running the code in same VM from multiple threads, 
  // or VMs that have the same GRAM pointer from different threads, or multiple
  // VMs that have a NULL GRAM pointer from multiple threads.
  // if you give each VM it's own unique GRAM and only run each VM in one thread, then you can leave it blank.

  // or if you're daring....

void NSEEL_HOSTSTUB_EnterMutex();
void NSEEL_HOSTSTUB_LeaveMutex();


int NSEEL_init(); // returns nonzero on failure (only if EEL_VALIDATE_FSTUBS defined), otherwise the same as NSEEL_quit(), and completely optional
void NSEEL_quit(); // clears any added functions


// adds a function that returns a value (EEL_F)
#define NSEEL_addfunc_retval(name,np,pproc,fptr) \
  NSEEL_addfunc_ret_type(name,np,1,pproc,(void *)(fptr),NSEEL_ADDFUNC_DESTINATION)  

// adds a function that returns a pointer (EEL_F*)
#define NSEEL_addfunc_retptr(name,np,pproc,fptr) \
  NSEEL_addfunc_ret_type(name,np,0,pproc,(void *)(fptr),NSEEL_ADDFUNC_DESTINATION)  

// adds a void or bool function
#define NSEEL_addfunc_retbool(name,np,pproc,fptr) \
  NSEEL_addfunc_ret_type(name,np,-1,pproc,(void *)(fptr),NSEEL_ADDFUNC_DESTINATION)  

// adds a function that takes min_np or more parameters (func sig needs to be EEL_F func(void *ctx, INT_PTR np, EEL_F **parms)
#define NSEEL_addfunc_varparm(name, min_np, pproc, fptr) \
  NSEEL_addfunc_varparm_ex(name,min_np,0,pproc,fptr,NSEEL_ADDFUNC_DESTINATION)

// adds a function that takes np parameters via func: sig needs to be EEL_F func(void *ctx, INT_PTR np, EEL_F **parms)
#define NSEEL_addfunc_exparms(name, np, pproc, fptr) \
  NSEEL_addfunc_varparm_ex(name,np,1,pproc,fptr,NSEEL_ADDFUNC_DESTINATION)


// deprecated
#define NSEEL_addfunction(name,nparms,code,len) NSEEL_addfunctionex((name),(nparms),(code),(len),0,0)
#define NSEEL_addfunctionex(name,nparms,code,len,pproc,fptr) NSEEL_addfunctionex2((name),(nparms),(code),(len),(pproc),(fptr),0, NSEEL_ADDFUNC_DESTINATION)

#ifndef NSEEL_ADDFUNC_DESTINATION
#define NSEEL_ADDFUNC_DESTINATION (NULL)
#endif

struct functionType;

typedef struct
{
  struct functionType *list;
  int list_size;
} eel_function_table;

struct _compileContext;
typedef void *(*NSEEL_PPPROC)(void *data, int data_size, struct _compileContext *userfunc_data);
void NSEEL_addfunctionex2(const char *name, int nparms, char *code_startaddr, int code_len, NSEEL_PPPROC pproc, void *fptr, void *fptr2, eel_function_table *destination);

void NSEEL_addfunc_ret_type(const char *name, int np, int ret_type,  NSEEL_PPPROC pproc, void *fptr, eel_function_table *destination); // ret_type=-1 for bool, 1 for value, 0 for ptr
void NSEEL_addfunc_varparm_ex(const char *name, int min_np, int want_exact, NSEEL_PPPROC pproc, EEL_F (NSEEL_CGEN_CALL *fptr)(void *, INT_PTR, EEL_F **), eel_function_table *destination);

int *NSEEL_getstats(); // returns a pointer to 5 ints... source bytes, static code bytes, call code bytes, data bytes, number of code handles

typedef void *NSEEL_VMCTX;
typedef void *NSEEL_CODEHANDLE;

NSEEL_VMCTX NSEEL_VM_alloc(); // return a handle
void NSEEL_VM_free(NSEEL_VMCTX ctx); // free when done with a VM and ALL of its code have been freed, as well

void NSEEL_VM_SetFunctionTable(NSEEL_VMCTX, eel_function_table *tab); // use NULL to use default (global) table

// validateFunc can return error message if not permitted
void NSEEL_VM_SetFunctionValidator(NSEEL_VMCTX, const char * (*validateFunc)(const char *fn_name, void *user), void *user);

void NSEEL_VM_remove_unused_vars(NSEEL_VMCTX _ctx);
void NSEEL_VM_clear_var_refcnts(NSEEL_VMCTX _ctx);
void NSEEL_VM_remove_all_nonreg_vars(NSEEL_VMCTX _ctx);
void NSEEL_VM_enumallvars(NSEEL_VMCTX ctx, int (*func)(const char *name, EEL_F *val, void *ctx), void *userctx); // return false from func to stop

EEL_F *NSEEL_VM_regvar(NSEEL_VMCTX ctx, const char *name); // register a variable (before compilation)
EEL_F *NSEEL_VM_getvar(NSEEL_VMCTX ctx, const char *name); // get a variable (if registered or created by code)
int  NSEEL_VM_get_var_refcnt(NSEEL_VMCTX _ctx, const char *name); // returns -1 if not registered, or >=0
void NSEEL_VM_set_var_resolver(NSEEL_VMCTX ctx, EEL_F *(*res)(void *userctx, const char *name), void *userctx); 

void NSEEL_VM_freeRAM(NSEEL_VMCTX ctx); // clears and frees all (VM) RAM used
void NSEEL_VM_freeRAMIfCodeRequested(NSEEL_VMCTX); // call after code to free the script-requested memory
int NSEEL_VM_wantfreeRAM(NSEEL_VMCTX ctx); // want NSEEL_VM_freeRAMIfCodeRequested?

// if you set this, it uses a local GMEM context. 
// Must be set before compilation. 
// void *p=NULL; 
// NSEEL_VM_SetGRAM(ctx,&p);
// .. do stuff
// NSEEL_VM_FreeGRAM(&p);
void NSEEL_VM_SetGRAM(NSEEL_VMCTX ctx, void **gram); 
void NSEEL_VM_FreeGRAM(void **ufd); // frees a gmem context.
void NSEEL_VM_SetCustomFuncThis(NSEEL_VMCTX ctx, void *thisptr);

EEL_F *NSEEL_VM_getramptr(NSEEL_VMCTX ctx, unsigned int offs, int *validCount);
EEL_F *NSEEL_VM_getramptr_noalloc(NSEEL_VMCTX ctx, unsigned int offs, int *validCount);


// set 0 to query. returns actual value used (limits, granularity apply -- see NSEEL_RAM_BLOCKS)
int NSEEL_VM_setramsize(NSEEL_VMCTX ctx, int maxent);


struct eelStringSegmentRec {
  struct eelStringSegmentRec *_next;
  const char *str_start; // escaped characters, including opening/trailing characters
  int str_len; 
};
void NSEEL_VM_SetStringFunc(NSEEL_VMCTX ctx, 
    EEL_F (*onString)(void *caller_this, struct eelStringSegmentRec *list),
    EEL_F (*onNamedString)(void *caller_this, const char *name));

// call with NULL to calculate size, or non-null to generate to buffer (returning size used -- will not null terminate, caller responsibility)
int nseel_stringsegments_tobuf(char *bufOut, int bufout_sz, struct eelStringSegmentRec *list); 


NSEEL_CODEHANDLE NSEEL_code_compile(NSEEL_VMCTX ctx, const char *code, int lineoffs);
#define NSEEL_CODE_COMPILE_FLAG_COMMONFUNCS 1 // allows that code's functions to be used in other code (note you shouldn't destroy that codehandle without destroying others first if used)
#define NSEEL_CODE_COMPILE_FLAG_COMMONFUNCS_RESET 2 // resets common code functions
#define NSEEL_CODE_COMPILE_FLAG_NOFPSTATE 4 // hint that the FPU/SSE state should be good-to-go
#define NSEEL_CODE_COMPILE_FLAG_ONLY_BUILTIN_FUNCTIONS 8 // very restrictive mode (only math functions really)

NSEEL_CODEHANDLE NSEEL_code_compile_ex(NSEEL_VMCTX ctx, const char *code, int lineoffs, int flags);

char *NSEEL_code_getcodeerror(NSEEL_VMCTX ctx);
int NSEEL_code_geterror_flag(NSEEL_VMCTX ctx);
void NSEEL_code_execute(NSEEL_CODEHANDLE code);
void NSEEL_code_free(NSEEL_CODEHANDLE code);
int *NSEEL_code_getstats(NSEEL_CODEHANDLE code); // 4 ints...source bytes, static code bytes, call code bytes, data bytes
  

// global memory control/view
extern unsigned int NSEEL_RAM_limitmem; // if nonzero, memory limit for user data, in bytes
extern unsigned int NSEEL_RAM_memused;
extern int NSEEL_RAM_memused_errors;



// configuration:

// use the handwritten lexer -- the flex (eel2.l generated) lexer mostly works, but doesn't support string parsing at the moment
// this mode is faster and uses less ram than eel2.l anyway, so leave it on
#define NSEEL_SUPER_MINIMAL_LEXER 

 // #define NSEEL_EEL1_COMPAT_MODE // supports old behaviors (continue after failed compile), old functions _bnot etc. disables string support (strings were used as comments in eel1 etc)

#define NSEEL_MAX_VARIABLE_NAMELEN 128  // define this to override the max variable length
#define NSEEL_MAX_EELFUNC_PARAMETERS 40
#define NSEEL_MAX_FUNCSIG_NAME 2048 // longer than variable maxlen, due to multiple namespaces

// maximum loop length (0 for unlimited)
#ifndef NSEEL_LOOPFUNC_SUPPORT_MAXLEN
#define NSEEL_LOOPFUNC_SUPPORT_MAXLEN 1048576 
#endif

#define NSEEL_MAX_FUNCTION_SIZE_FOR_INLINE 2048

// when a VM ctx doesn't have a GRAM context set, make the global one this big
#define NSEEL_SHARED_GRAM_SIZE (1<<20)

//#define EEL_DUMP_OPS // used for testing frontend parser/logic changes

// note: if you wish to change NSEEL_RAM_*, and your target is x86-64, you will need to regenerate things.

// on osx:
//  php a2x64.php win64x
//  php a2x64.php macho64

// or on win32:
//  php a2x64.php
//  php a2x64.php macho64x
// this will regenerate the .asm files and object files

// 512 * 65536 = 32 million entries maximum (256MB RAM)
// default is limited to 128 * 65536 = 8 million entries (64MB RAM)

// default to 8 million entries, use NSEEL_VM_setramsize() to change at runtime
#define NSEEL_RAM_BLOCKS_DEFAULTMAX 128

// 512 entry block table maximum (2k/4k per VM)
#define NSEEL_RAM_BLOCKS_LOG2 9

 // 65536 items per block (512KB)
#define NSEEL_RAM_ITEMSPERBLOCK_LOG2 16

#define NSEEL_RAM_BLOCKS (1 << NSEEL_RAM_BLOCKS_LOG2)
#define NSEEL_RAM_ITEMSPERBLOCK (1<<NSEEL_RAM_ITEMSPERBLOCK_LOG2)

#define NSEEL_STACK_SIZE 4096 // about 64k overhead if the stack functions are used in a given code handle

// arch neutral mode, runs about 1/8th speed or so
//#define EEL_TARGET_PORTABLE

#ifdef EEL_TARGET_PORTABLE
#ifdef EEL_PORTABLE_TAILCALL
typedef void (*EEL_BC_TYPE)(void *next_inst, void *state);
#else
#define EEL_BC_TYPE int
#endif
#endif

#ifdef NSEEL_EEL1_COMPAT_MODE
double *NSEEL_getglobalregs();
#endif

void eel_setfp_round(); // use to set fp to rounding mode (normal) -- only really use this when being called from EEL
void eel_setfp_trunc(); // use to restore fp to trunc mode -- only really use this when being called from EEL

void eel_enterfp(int s[2]);
void eel_leavefp(int s[2]);

extern void *(*nseel_gmem_calloc)(size_t,size_t); // set this to the calloc() implementation used by the context that will call NSEEL_VM_FreeGRAM()

#ifdef __cplusplus
}
#endif

#endif//__NS_EEL_H__

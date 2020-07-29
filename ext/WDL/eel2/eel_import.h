/*******************************************************************************************
 * imported EEL 
 ******************************************************************************************/

void (*NSEEL_addfunc_ret_type)(const char *name, int np, int ret_type,  NSEEL_PPPROC pproc, void *fptr, eel_function_table *destination); // ret_type=-1 for bool, 1 for value, 0 for ptr
void (*NSEEL_addfunc_varparm_ex)(const char *name, int min_np, int want_exact, NSEEL_PPPROC pproc, EEL_F (NSEEL_CGEN_CALL *fptr)(void *, INT_PTR, EEL_F **), eel_function_table *destination);


NSEEL_VMCTX (*NSEEL_VM_alloc)(); // return a handle
void (*NSEEL_VM_SetGRAM)(NSEEL_VMCTX, void **);
void (*NSEEL_VM_free)(NSEEL_VMCTX ctx); // free when done with a VM and ALL of its code have been freed, as well
void (*NSEEL_VM_SetFunctionTable)(NSEEL_VMCTX, eel_function_table *tab); // use NULL to use default (global) table
EEL_F *(*NSEEL_VM_regvar)(NSEEL_VMCTX ctx, const char *name); // register a variable (before compilation)

void (*NSEEL_VM_SetCustomFuncThis)(NSEEL_VMCTX ctx, void *thisptr);
NSEEL_CODEHANDLE (*NSEEL_code_compile_ex)(NSEEL_VMCTX ctx, const char *code, int lineoffs, int flags);
void (*NSEEL_VM_set_var_resolver)(NSEEL_VMCTX ctx, EEL_F *(*res)(void *userctx, const char *name), void *userctx); 
char *(*NSEEL_code_getcodeerror)(NSEEL_VMCTX ctx);
void (*NSEEL_code_execute)(NSEEL_CODEHANDLE code);
void (*NSEEL_code_free)(NSEEL_CODEHANDLE code);
EEL_F *(*nseel_int_register_var)(compileContext *ctx, const char *name, int isReg, const char **namePtrOut);
void (*NSEEL_VM_enumallvars)(NSEEL_VMCTX ctx, int (*func)(const char *name, EEL_F *val, void *ctx), void *userctx); 
EEL_F *(*NSEEL_VM_getramptr)(NSEEL_VMCTX ctx, unsigned int offs, int *validAmt);
void ** (*eel_gmem_attach)(const char *nm, bool is_alloc);
void (*eel_fft_register)(eel_function_table*);

struct eelStringSegmentRec {
  struct eelStringSegmentRec *_next;
  const char *str_start; // escaped characters, including opening/trailing characters
  int str_len; 
};
void (*NSEEL_VM_SetStringFunc)(NSEEL_VMCTX ctx, 
    EEL_F (*onString)(void *caller_this, struct eelStringSegmentRec *list),
    EEL_F (*onNamedString)(void *caller_this, const char *name));

// call with NULL to calculate size, or non-null to generate to buffer (returning size used -- will not null terminate, caller responsibility)
int (*nseel_stringsegments_tobuf)(char *bufOut, int bufout_sz, struct eelStringSegmentRec *list); 

void *(*NSEEL_PProc_RAM)(void *data, int data_size, struct _compileContext *ctx);
void *(*NSEEL_PProc_THIS)(void *data, int data_size, struct _compileContext *ctx);

void (*eel_setfp_round)();
void (*eel_setfp_trunc)();

void (*eel_enterfp)(int s[2]);
void (*eel_leavefp)(int s[2]);


eel_function_table g_eel_function_table;
#define NSEEL_ADDFUNC_DESTINATION (&g_eel_function_table)

//
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

class eel_string_context_state;

#define __NS_EELINT_H__

#define EEL_IMPORT_ALL(IMPORT_FUNC) \
    IMPORT_FUNC(NSEEL_addfunc_ret_type) \
    IMPORT_FUNC(NSEEL_addfunc_varparm_ex) \
    IMPORT_FUNC(NSEEL_VM_free) \
    IMPORT_FUNC(NSEEL_VM_SetFunctionTable) \
    IMPORT_FUNC(NSEEL_VM_regvar) \
    IMPORT_FUNC(NSEEL_VM_SetCustomFuncThis) \
    IMPORT_FUNC(NSEEL_code_compile_ex) \
    IMPORT_FUNC(NSEEL_code_getcodeerror) \
    IMPORT_FUNC(NSEEL_code_execute) \
    IMPORT_FUNC(NSEEL_code_free) \
    IMPORT_FUNC(NSEEL_PProc_THIS) \
    IMPORT_FUNC(NSEEL_PProc_RAM) \
    IMPORT_FUNC(NSEEL_VM_SetStringFunc) \
    IMPORT_FUNC(NSEEL_VM_enumallvars) \
    IMPORT_FUNC(NSEEL_VM_getramptr) \
    IMPORT_FUNC(NSEEL_VM_SetGRAM) \
    IMPORT_FUNC(eel_gmem_attach) \
    IMPORT_FUNC(eel_fft_register) \
    IMPORT_FUNC(nseel_stringsegments_tobuf) \
    IMPORT_FUNC(nseel_int_register_var) \
    IMPORT_FUNC(eel_setfp_round) \
    IMPORT_FUNC(eel_setfp_trunc) \
    IMPORT_FUNC(eel_leavefp) \
    IMPORT_FUNC(eel_enterfp) \
    IMPORT_FUNC(NSEEL_VM_set_var_resolver) \
    IMPORT_FUNC(NSEEL_VM_alloc) /* keep NSEEL_VM_alloc last */
    
    

/*******************************************************************************************
 * END of imported EEL 
 ******************************************************************************************/

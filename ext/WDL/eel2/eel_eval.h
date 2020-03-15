#ifndef _EEL_EVAL_H_
#define _EEL_EVAL_H_

#ifndef EEL_EVAL_GET_CACHED
#define EEL_EVAL_GET_CACHED(str, ch) (NULL)
#endif

#ifndef EEL_EVAL_SET_CACHED
#define EEL_EVAL_SET_CACHED(sv, ch) { NSEEL_code_free(ch); free(sv); }
#endif

#ifndef EEL_EVAL_SCOPE_ENTER
#define EEL_EVAL_SCOPE_ENTER 1
#define EEL_EVAL_SCOPE_LEAVE
#endif

static EEL_F NSEEL_CGEN_CALL _eel_eval(void *opaque, EEL_F *s)
{
  NSEEL_VMCTX r = EEL_EVAL_GET_VMCTX(opaque);
  NSEEL_CODEHANDLE ch = NULL;
  char *sv=NULL;
  if (r)
  {
    EEL_STRING_MUTEXLOCK_SCOPE
    const char *str=EEL_STRING_GET_FOR_INDEX(*s,NULL);
#ifdef EEL_STRING_DEBUGOUT
    if (!str)
    {
      EEL_STRING_DEBUGOUT("eval() passed invalid string handle %f",*s);
    }
#endif
    if (str && *str) 
    {
      sv=EEL_EVAL_GET_CACHED(str,ch);
      if (!sv) sv=strdup(str);
    }
  }
  if (sv)
  {
    if (!ch) ch = NSEEL_code_compile(r,sv,0);
    if (ch)
    {
      if (EEL_EVAL_SCOPE_ENTER)
      {
        NSEEL_code_execute(ch);
        EEL_EVAL_SCOPE_LEAVE
      }
      else
      {
#ifdef EEL_STRING_DEBUGOUT
        EEL_STRING_DEBUGOUT("eval() reentrancy limit reached");
#endif
      }

      EEL_EVAL_SET_CACHED(sv,ch);
      return 1.0;
    }
    else
    { 
#ifdef EEL_STRING_DEBUGOUT
      const char *err=NSEEL_code_getcodeerror(r);
      if (err) EEL_STRING_DEBUGOUT("eval() error: %s",err);
#endif
    }
    free(sv);
  }
  return 0.0;
}

void EEL_eval_register()
{
  NSEEL_addfunc_retval("eval",1,NSEEL_PProc_THIS,&_eel_eval);
}

#ifdef EEL_WANT_DOCUMENTATION
static const char *eel_eval_function_reference = 
  "eval\t\"code\"\tExecutes code passed in. Code can use functions, but functions created in code can't be used elsewhere.\0"
;
#endif

#endif

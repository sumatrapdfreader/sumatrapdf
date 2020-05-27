#ifndef _WIN32
#include <unistd.h>
  #ifndef EELSCRIPT_NO_LICE
    #include "../swell/swell.h"
  #endif
#endif

#include "../wdltypes.h"
#include "../ptrlist.h"
#include "../wdlstring.h"
#include "../assocarray.h"
#include "../queue.h"
#include "../win32_utf8.h"
#include "ns-eel.h"


#ifndef EELSCRIPT_MAX_FILE_HANDLES
#define EELSCRIPT_MAX_FILE_HANDLES 512
#endif
#ifndef EELSCRIPT_FILE_HANDLE_INDEX_BASE
#define EELSCRIPT_FILE_HANDLE_INDEX_BASE 1000000
#endif
#ifndef EEL_STRING_MAXUSERSTRING_LENGTH_HINT
#define EEL_STRING_MAXUSERSTRING_LENGTH_HINT (1<<16) // 64KB per string max
#endif
#ifndef EEL_STRING_MAX_USER_STRINGS
#define EEL_STRING_MAX_USER_STRINGS 32768
#endif
#ifndef EEL_STRING_LITERAL_BASE
#define EEL_STRING_LITERAL_BASE 2000000
#endif
#ifndef EELSCRIPT_LICE_MAX_IMAGES
#define EELSCRIPT_LICE_MAX_IMAGES 1024
#endif

#ifndef EELSCRIPT_LICE_MAX_FONTS
#define EELSCRIPT_LICE_MAX_FONTS 128
#endif

#ifndef EELSCRIPT_NET_MAXCON
#define EELSCRIPT_NET_MAXCON 4096
#endif

#ifndef EELSCRIPT_LICE_CLASSNAME
#define EELSCRIPT_LICE_CLASSNAME "eelscript_gfx"
#endif


// #define EELSCRIPT_NO_NET
// #define EELSCRIPT_NO_LICE
// #define EELSCRIPT_NO_FILE
// #define EELSCRIPT_NO_FFT
// #define EELSCRIPT_NO_MDCT
// #define EELSCRIPT_NO_EVAL

class eel_string_context_state;
#ifndef EELSCRIPT_NO_NET
class eel_net_state;
#endif
#ifndef EELSCRIPT_NO_LICE
class eel_lice_state;
#endif

class eelScriptInst {
  public:

    static int init();

    eelScriptInst();
    virtual ~eelScriptInst();

    NSEEL_CODEHANDLE compile_code(const char *code, const char **err);
    int runcode(const char *code, int showerr, const char *showerrfn, bool canfree, bool ignoreEndOfInputChk, bool doExec);
    int loadfile(const char *fn, const char *callerfn, bool allowstdin);

    NSEEL_VMCTX m_vm;

    WDL_PtrList<void> m_code_freelist;

#ifndef EELSCRIPT_NO_FILE
    FILE *m_handles[EELSCRIPT_MAX_FILE_HANDLES];
    virtual EEL_F OpenFile(const char *fn, const char *mode)
    {
      if (!*fn || !*mode) return 0.0;
#ifndef EELSCRIPT_NO_STDIO
      if (!strcmp(fn,"stdin")) return 1;
      if (!strcmp(fn,"stdout")) return 2;
      if (!strcmp(fn,"stderr")) return 3;
#endif

      WDL_FastString fnstr(fn);
      if (!translateFilename(&fnstr,mode)) return 0.0;

      int x;
      for (x=0;x<EELSCRIPT_MAX_FILE_HANDLES && m_handles[x];x++);
      if (x>= EELSCRIPT_MAX_FILE_HANDLES) return 0.0;

      FILE *fp = fopenUTF8(fnstr.Get(),mode);
      if (!fp) return 0.0;
      m_handles[x]=fp;
      return x + EELSCRIPT_FILE_HANDLE_INDEX_BASE;
    }
    virtual EEL_F CloseFile(int fp_idx)
    {
      fp_idx-=EELSCRIPT_FILE_HANDLE_INDEX_BASE;
      if (fp_idx>=0 && fp_idx<EELSCRIPT_MAX_FILE_HANDLES && m_handles[fp_idx])
      {
        fclose(m_handles[fp_idx]);
        m_handles[fp_idx]=0;
        return 0.0;
      }
      return -1.0;
    }
    virtual FILE *GetFileFP(int fp_idx)
    {
#ifndef EELSCRIPT_NO_STDIO
      if (fp_idx==1) return stdin;
      if (fp_idx==2) return stdout;
      if (fp_idx==3) return stderr;
#endif
      fp_idx-=EELSCRIPT_FILE_HANDLE_INDEX_BASE;
      if (fp_idx>=0 && fp_idx<EELSCRIPT_MAX_FILE_HANDLES) return m_handles[fp_idx];
      return NULL;
    }

#endif

    virtual bool translateFilename(WDL_FastString *fs, const char *mode) { return true; }

    virtual bool GetFilenameForParameter(EEL_F idx, WDL_FastString *fs, int iswrite);

    eel_string_context_state *m_string_context;
#ifndef EELSCRIPT_NO_NET
    eel_net_state *m_net_state;
#endif
#ifndef EELSCRIPT_NO_LICE
    eel_lice_state *m_gfx_state;
#endif

#ifndef EELSCRIPT_NO_EVAL
    struct evalCacheEnt {
      char *str;
      NSEEL_CODEHANDLE ch;
    };
    int m_eval_depth;
    WDL_TypedBuf<evalCacheEnt> m_eval_cache;
    virtual char *evalCacheGet(const char *str, NSEEL_CODEHANDLE *ch);
    virtual void evalCacheDispose(char *key, NSEEL_CODEHANDLE ch);
    WDL_Queue m_defer_eval, m_atexit_eval;
    void runCodeQ(WDL_Queue *q, const char *fname);
    void runAtExitCode()
    {
      runCodeQ(&m_atexit_eval,"atexit");
      m_atexit_eval.Clear(); // make sure nothing gets added in atexit(), in case the user called runAtExitCode before destroying
    }
#endif
    virtual bool run_deferred(); // requires eval support to be useful
    virtual bool has_deferred();


    WDL_StringKeyedArray<bool> m_loaded_fnlist; // imported file list (to avoid repeats)
};

//#define EEL_STRINGS_MUTABLE_LITERALS
//#define EEL_STRING_WANT_MUTEX


#define EEL_STRING_GET_CONTEXT_POINTER(opaque) (((eelScriptInst *)opaque)->m_string_context)
#ifndef EEL_STRING_STDOUT_WRITE
  #ifndef EELSCRIPT_NO_STDIO
    #define EEL_STRING_STDOUT_WRITE(x,len) { fwrite(x,len,1,stdout); fflush(stdout); }
  #endif
#endif
#include "eel_strings.h"

#include "eel_misc.h"


#ifndef EELSCRIPT_NO_FILE
  #define EEL_FILE_OPEN(fn,mode) ((eelScriptInst*)opaque)->OpenFile(fn,mode)
  #define EEL_FILE_GETFP(fp) ((eelScriptInst*)opaque)->GetFileFP(fp)
  #define EEL_FILE_CLOSE(fpindex) ((eelScriptInst*)opaque)->CloseFile(fpindex)

  #include "eel_files.h"
#endif

#ifndef EELSCRIPT_NO_FFT
  #include "eel_fft.h"
#endif

#ifndef EELSCRIPT_NO_MDCT
  #include "eel_mdct.h"
#endif

#ifndef EELSCRIPT_NO_NET
  #define EEL_NET_GET_CONTEXT(opaque) (((eelScriptInst *)opaque)->m_net_state)
  #include "eel_net.h"
#endif

#ifndef EELSCRIPT_NO_LICE
  #ifndef EEL_LICE_WANT_STANDALONE
    #define EEL_LICE_WANT_STANDALONE
  #endif
  #ifndef EELSCRIPT_LICE_NOUPDATE
    #define EEL_LICE_WANT_STANDALONE_UPDATE // gfx_update() which runs message pump and updates screen etc
  #endif
 
  #define EEL_LICE_GET_FILENAME_FOR_STRING(idx, fs, p) (((eelScriptInst*)opaque)->GetFilenameForParameter(idx,fs,p))
  #define EEL_LICE_GET_CONTEXT(opaque) ((opaque) ? (((eelScriptInst *)opaque)->m_gfx_state) : NULL)
  #include "eel_lice.h"
#endif

#ifndef EELSCRIPT_NO_EVAL
  #define EEL_EVAL_GET_CACHED(str, ch) ((eelScriptInst *)opaque)->evalCacheGet(str,&(ch))
  #define EEL_EVAL_SET_CACHED(str, ch) ((eelScriptInst *)opaque)->evalCacheDispose(str,ch)
  #define EEL_EVAL_GET_VMCTX(opaque) (((eelScriptInst *)opaque)->m_vm)
  #define EEL_EVAL_SCOPE_ENTER (((eelScriptInst *)opaque)->m_eval_depth < 3 ? \
                                 ++((eelScriptInst *)opaque)->m_eval_depth : 0)
  #define EEL_EVAL_SCOPE_LEAVE ((eelScriptInst *)opaque)->m_eval_depth--;
  #include "eel_eval.h"

static EEL_F NSEEL_CGEN_CALL _eel_defer(void *opaque, EEL_F *s)
{
  EEL_STRING_MUTEXLOCK_SCOPE
  const char *str=EEL_STRING_GET_FOR_INDEX(*s,NULL);
  if (str && *str && *s >= EEL_STRING_MAX_USER_STRINGS)  // don't allow defer(0) etc
  {
    eelScriptInst *inst = (eelScriptInst *)opaque;
    if (inst->m_defer_eval.Available() < EEL_STRING_MAXUSERSTRING_LENGTH_HINT)
    {
      inst->m_defer_eval.Add(str,strlen(str)+1);
      return 1.0;
    }
#ifdef EEL_STRING_DEBUGOUT
    EEL_STRING_DEBUGOUT("defer(): too much defer() code already added, ignoring");
#endif
  }
#ifdef EEL_STRING_DEBUGOUT
  else if (!str)
  {
    EEL_STRING_DEBUGOUT("defer(): invalid string identifier specified %f",*s);
  }
  else if (*s < EEL_STRING_MAX_USER_STRINGS)
  {
    EEL_STRING_DEBUGOUT("defer(): user string identifier %f specified but not allowed",*s);
  }
#endif
  return 0.0;
}
static EEL_F NSEEL_CGEN_CALL _eel_atexit(void *opaque, EEL_F *s)
{
  EEL_STRING_MUTEXLOCK_SCOPE
  const char *str=EEL_STRING_GET_FOR_INDEX(*s,NULL);
  if (str && *str && *s >= EEL_STRING_MAX_USER_STRINGS)  // don't allow atexit(0) etc
  {
    eelScriptInst *inst = (eelScriptInst *)opaque;
    if (inst->m_atexit_eval.Available() < EEL_STRING_MAXUSERSTRING_LENGTH_HINT)
    {
      inst->m_atexit_eval.Add(str,strlen(str)+1);
      return 1.0;
    }
#ifdef EEL_STRING_DEBUGOUT
    EEL_STRING_DEBUGOUT("atexit(): too much atexit() code already added, ignoring");
#endif
  }
#ifdef EEL_STRING_DEBUGOUT
  else if (!str)
  {
    EEL_STRING_DEBUGOUT("atexit(): invalid string identifier specified %f",*s);
  }
  else if (*s < EEL_STRING_MAX_USER_STRINGS)
  {
    EEL_STRING_DEBUGOUT("atexit(): user string identifier %f specified but not allowed",*s);
  }
#endif
  return 0.0;
}
#endif


#define opaque ((void *)this)

eelScriptInst::eelScriptInst() : m_loaded_fnlist(false)
{
#ifndef EELSCRIPT_NO_FILE
  memset(m_handles,0,sizeof(m_handles));
#endif
  m_vm = NSEEL_VM_alloc();
#ifdef EEL_STRING_DEBUGOUT
  if (!m_vm) EEL_STRING_DEBUGOUT("NSEEL_VM_alloc(): failed");
#endif
  NSEEL_VM_SetCustomFuncThis(m_vm,this);
#ifdef NSEEL_ADDFUNC_DESTINATION
  NSEEL_VM_SetFunctionTable(m_vm,NSEEL_ADDFUNC_DESTINATION);
#endif

  m_string_context = new eel_string_context_state;
  eel_string_initvm(m_vm);
#ifndef EELSCRIPT_NO_NET
  m_net_state = new eel_net_state(EELSCRIPT_NET_MAXCON,NULL);
#endif
#ifndef EELSCRIPT_NO_LICE
  m_gfx_state = new eel_lice_state(m_vm,this,EELSCRIPT_LICE_MAX_IMAGES,EELSCRIPT_LICE_MAX_FONTS);

  m_gfx_state->resetVarsToStock();
#endif
#ifndef EELSCRIPT_NO_EVAL
  m_eval_depth=0;
#endif
}

eelScriptInst::~eelScriptInst() 
{
#ifndef EELSCRIPT_NO_EVAL
  if (m_atexit_eval.GetSize()>0) runAtExitCode();
#endif
  int x;
  m_code_freelist.Empty((void (*)(void *))NSEEL_code_free);
#ifndef EELSCRIPT_NO_EVAL
  for (x=0;x<m_eval_cache.GetSize();x++)
  {
    free(m_eval_cache.Get()[x].str);
    NSEEL_code_free(m_eval_cache.Get()[x].ch);
  }
#endif

  if (m_vm) NSEEL_VM_free(m_vm);

#ifndef EELSCRIPT_NO_FILE
  for (x=0;x<EELSCRIPT_MAX_FILE_HANDLES;x++) 
  {
    if (m_handles[x]) fclose(m_handles[x]); 
    m_handles[x]=0;
  }
#endif
  delete m_string_context;
#ifndef EELSCRIPT_NO_NET
  delete m_net_state;
#endif
#ifndef EELSCRIPT_NO_LICE
  delete m_gfx_state;
#endif
}

bool eelScriptInst::GetFilenameForParameter(EEL_F idx, WDL_FastString *fs, int iswrite)
{
  const char *fmt = EEL_STRING_GET_FOR_INDEX(idx,NULL);
  if (!fmt) return false;
  fs->Set(fmt);
  return translateFilename(fs,iswrite?"w":"r");
}

NSEEL_CODEHANDLE eelScriptInst::compile_code(const char *code, const char **err)
{
  if (!m_vm)
  {
    *err = "EEL VM not initialized";
    return NULL;
  }
  NSEEL_CODEHANDLE ch = NSEEL_code_compile_ex(m_vm, code, 0, NSEEL_CODE_COMPILE_FLAG_COMMONFUNCS);
  if (ch)
  {
    m_string_context->update_named_vars(m_vm);
    m_code_freelist.Add((void*)ch);
    return ch;
  }
  *err = NSEEL_code_getcodeerror(m_vm);
  return NULL;
}

int eelScriptInst::runcode(const char *codeptr, int showerr, const char *showerrfn, bool canfree, bool ignoreEndOfInputChk, bool doExec)
{
  if (m_vm) 
  {
    NSEEL_CODEHANDLE code = NSEEL_code_compile_ex(m_vm,codeptr,0,canfree ? 0 : NSEEL_CODE_COMPILE_FLAG_COMMONFUNCS);
    if (code) m_string_context->update_named_vars(m_vm);

    char *err;
    if (!code && (err=NSEEL_code_getcodeerror(m_vm)))
    {
      if (!ignoreEndOfInputChk && (NSEEL_code_geterror_flag(m_vm)&1)) return 1;
      if (showerr) 
      {
#ifdef EEL_STRING_DEBUGOUT
        if (showerr==2)
        {
          EEL_STRING_DEBUGOUT("Warning: %s:%s",WDL_get_filepart(showerrfn),err);
        }
        else
        {
          EEL_STRING_DEBUGOUT("%s:%s",WDL_get_filepart(showerrfn),err);
        }
#endif
      }
      return -1;
    }
    else
    {
      if (code)
      {
#ifdef EELSCRIPT_DO_DISASSEMBLE
        codeHandleType *p = (codeHandleType*)code;

        char buf[512];
        buf[0]=0;
#ifdef _WIN32
        GetTempPath(sizeof(buf)-64,buf);
        lstrcatn(buf,"jsfx-out",sizeof(buf));
#else
        lstrcpyn_safe(buf,"/tmp/jsfx-out",sizeof(buf));
#endif
        FILE *fp = fopenUTF8(buf,"wb");
        if (fp)
        {
          fwrite(p->code,1,p->code_size,fp);
          fclose(fp);
          char buf2[2048];
#ifdef _WIN32
          snprintf(buf2,sizeof(buf2),"disasm \"%s\"",buf);
#else
  #ifdef __aarch64__
          snprintf(buf2,sizeof(buf2), "objdump -D -b binary -maarch64 \"%s\"",buf);
  #elif defined(__LP64__)
          snprintf(buf2,sizeof(buf2),"distorm3 --b64 \"%s\"",buf);
  #else
          snprintf(buf2,sizeof(buf2),"distorm3 --b32 \"%s\"",buf);
  #endif
#endif
          system(buf2);
        }
#endif

        if (doExec) NSEEL_code_execute(code);
        if (canfree) NSEEL_code_free(code);
        else m_code_freelist.Add((void*)code);
      }
      return 0;
    }
  }
  return -1;
}


FILE *eelscript_resolvePath(WDL_FastString &usefn, const char *fn, const char *callerfn)
{
    // resolve path relative to current
    int x;
    bool had_abs=false;
    for (x=0;x<2; x ++)
    {
#ifdef _WIN32
      if (!x && ((fn[0] == '\\' && fn[1] == '\\') || (fn[0] && fn[1] == ':')))
#else
      if (!x && fn[0] == '/')
#endif
      {
        usefn.Set(fn);
        had_abs=true;
      }
      else
      {
        const char *fnu = fn;
        if (x)
        {
          while (*fnu) fnu++;
          while (fnu >= fn && *fnu != '\\' && *fnu != '/') fnu--;
          if (fnu < fn) break;
          fnu++;
        }

        usefn.Set(callerfn);
        int l=usefn.GetLength();
        while (l > 0 && usefn.Get()[l-1] != '\\' && usefn.Get()[l-1] != '/') l--;
        if (l > 0) 
        {
          usefn.SetLen(l);
          usefn.Append(fnu);
        }
        else
        {
          usefn.Set(fnu);
        }
        int last_slash_pos=-1;
        for (l = 0; l < usefn.GetLength(); l ++)
        {
          if (usefn.Get()[l] == '/' || usefn.Get()[l] == '\\')
          {
            if (usefn.Get()[l+1] == '.' && usefn.Get()[l+2] == '.' && 
                (usefn.Get()[l+3] == '/' || usefn.Get()[l+3] == '\\'))
            {
              if (last_slash_pos >= 0)
                usefn.DeleteSub(last_slash_pos, l+3-last_slash_pos);
              else
                usefn.DeleteSub(0,l+3+1);
            }
            else
            {
              last_slash_pos=l;
            }
          }
        // take currentfn, remove filename part, add fnu
        }
      }

      FILE *fp = fopenUTF8(usefn.Get(),"r");
      if (fp) return fp;
    }
    if (had_abs) usefn.Set(fn);
    return NULL;
}

int eelScriptInst::loadfile(const char *fn, const char *callerfn, bool allowstdin)
{
  WDL_FastString usefn;
  FILE *fp = NULL;
  if (!strcmp(fn,"-"))
  {
    if (callerfn)
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("@import: can't import \"-\" (stdin)");
#endif
      return -1;
    }
    if (allowstdin)
    {
      fp = stdin;
      fn = "(stdin)";
    }
  }
  else if (!callerfn) 
  {
    fp = fopenUTF8(fn,"r");
    if (fp) m_loaded_fnlist.Insert(fn,true);
  }
  else
  {
    fp = eelscript_resolvePath(usefn,fn,callerfn);
    if (fp)
    {
      if (m_loaded_fnlist.Get(usefn.Get())) 
      {
        fclose(fp);
        return 0;
      }
      m_loaded_fnlist.Insert(usefn.Get(),true);
      fn = usefn.Get();
    }
  }

  if (!fp)
  {
#ifdef EEL_STRING_DEBUGOUT
    if (callerfn)
      EEL_STRING_DEBUGOUT("Warning: @import could not open '%s'",fn);
    else
      EEL_STRING_DEBUGOUT("Error opening %s",fn);
#endif
    return -1;
  }

  WDL_FastString code;
  char line[4096];
  for (;;)
  {
    line[0]=0;
    fgets(line,sizeof(line),fp);
    if (!line[0]) break;
    if (!strnicmp(line,"@import",7) && isspace(line[7]))
    {
      char *p=line+7;
      while (isspace(*p)) p++;

      char *ep=p;
      while (*ep) ep++;
      while (ep>p && isspace(ep[-1])) ep--;
      *ep=0;

      if (*p) loadfile(p,fn,false);
    }
    else
    {
      code.Append(line);
    }
  }
  if (fp != stdin) fclose(fp);

  return runcode(code.Get(),callerfn ? 2 : 1, fn,false,true,!callerfn);
}

char *eelScriptInst::evalCacheGet(const char *str, NSEEL_CODEHANDLE *ch)
{
  // should mutex protect if multiple threads access this eelScriptInst context
  int x=m_eval_cache.GetSize();
  while (--x >= 0)
  {
    char *ret;
    if (!strcmp(ret=m_eval_cache.Get()[x].str, str))
    {
      *ch = m_eval_cache.Get()[x].ch;
      m_eval_cache.Delete(x);
      return ret;
    }
  }
  return NULL;
}

void eelScriptInst::evalCacheDispose(char *key, NSEEL_CODEHANDLE ch)
{
  // should mutex protect if multiple threads access this eelScriptInst context
  evalCacheEnt ecc;
  ecc.str= key;
  ecc.ch = ch;
  if (m_eval_cache.GetSize() > 1024) 
  {
    NSEEL_code_free(m_eval_cache.Get()->ch);
    free(m_eval_cache.Get()->str);
    m_eval_cache.Delete(0);
  }
  m_eval_cache.Add(ecc);
}

int eelScriptInst::init()
{
  EEL_string_register();
#ifndef EELSCRIPT_NO_FILE
  EEL_file_register();
#endif
#ifndef EELSCRIPT_NO_FFT
  EEL_fft_register();
#endif
#ifndef EELSCRIPT_NO_MDCT
  EEL_mdct_register();
#endif
  EEL_misc_register();
#ifndef EELSCRIPT_NO_EVAL
  EEL_eval_register();
  NSEEL_addfunc_retval("defer",1,NSEEL_PProc_THIS,&_eel_defer);
  NSEEL_addfunc_retval("runloop", 1, NSEEL_PProc_THIS, &_eel_defer);
  NSEEL_addfunc_retval("atexit",1,NSEEL_PProc_THIS,&_eel_atexit);
#endif
#ifndef EELSCRIPT_NO_NET
  EEL_tcp_register();
#endif
#ifndef EELSCRIPT_NO_LICE
  eel_lice_register();
  #ifdef _WIN32
    eel_lice_register_standalone(GetModuleHandle(NULL),EELSCRIPT_LICE_CLASSNAME,NULL,NULL);
  #else
    eel_lice_register_standalone(NULL,EELSCRIPT_LICE_CLASSNAME,NULL,NULL);
  #endif
#endif
  return 0;
}

bool eelScriptInst::has_deferred()
{
#ifndef EELSCRIPT_NO_EVAL
  return m_defer_eval.Available() && m_vm;
#else
  return false;
#endif
}

#ifndef EELSCRIPT_NO_EVAL
void eelScriptInst::runCodeQ(WDL_Queue *q, const char *callername)
{
  const int endptr = q->Available();
  int offs = 0;
  while (offs < endptr)
  {
    if (q->Available() < endptr) break; // should never happen, but safety first!

    const char *ptr = (const char *)q->Get() + offs;
    offs += strlen(ptr)+1;

    NSEEL_CODEHANDLE ch=NULL;
    char *sv=evalCacheGet(ptr,&ch);

    if (!sv) sv=strdup(ptr);
    if (!ch) ch=NSEEL_code_compile(m_vm,sv,0);
    if (!ch)
    {
      free(sv);
#ifdef EEL_STRING_DEBUGOUT
      const char *err = NSEEL_code_getcodeerror(m_vm);
      if (err) EEL_STRING_DEBUGOUT("%s: error in code: %s",callername,err);
#endif
    }
    else
    {
      NSEEL_code_execute(ch);
      evalCacheDispose(sv,ch);
    }
  }
  q->Advance(endptr);
}
#endif

bool eelScriptInst::run_deferred()
{
#ifndef EELSCRIPT_NO_EVAL
  if (!m_defer_eval.Available()||!m_vm) return false;

  runCodeQ(&m_defer_eval,"defer");
  m_defer_eval.Compact();
  return m_defer_eval.Available()>0;
#else
  return false;
#endif
}

#ifdef EEL_WANT_DOCUMENTATION
#include "ns-eel-func-ref.h"

void EELScript_GenerateFunctionList(WDL_PtrList<const char> *fs)
{
  const char *p = nseel_builtin_function_reference;
  while (*p) { fs->Add(p); p += strlen(p) + 1; }
  p = eel_strings_function_reference;
  while (*p) { fs->Add(p); p += strlen(p) + 1; }
  p = eel_misc_function_reference;
  while (*p) { fs->Add(p); p += strlen(p) + 1; }
#ifndef EELSCRIPT_NO_EVAL
  fs->Add("atexit\t\"code\"\t"
#ifndef EELSCRIPT_HELP_NO_DEFER_DESC
    "Adds code to be executed when the script finishes."
#endif
    );
  fs->Add("defer\t\"code\"\t"
#ifndef EELSCRIPT_HELP_NO_DEFER_DESC
    "Adds code which will be executed some small amount of time after the current code finishes. Identical to runloop()"
#endif
    );
  fs->Add("runloop\t\"code\"\t"
#ifndef EELSCRIPT_HELP_NO_DEFER_DESC
    "Adds code which will be executed some small amount of time after the current code finishes. Identical to defer()"
#endif
    );

  p = eel_eval_function_reference;
  while (*p) { fs->Add(p); p += strlen(p) + 1; }
#endif
#ifndef EELSCRIPT_NO_NET
  p = eel_net_function_reference;
  while (*p) { fs->Add(p); p += strlen(p) + 1; }
#endif
#ifndef EELSCRIPT_NO_FFT
  p = eel_fft_function_reference;
  while (*p) { fs->Add(p); p += strlen(p) + 1; }
#endif
#ifndef EELSCRIPT_NO_FILE  
  p = eel_file_function_reference;
  while (*p) { fs->Add(p); p += strlen(p) + 1; }
#endif
#ifndef EELSCRIPT_NO_MDCT
  p = eel_mdct_function_reference;
  while (*p) { fs->Add(p); p += strlen(p) + 1; }
#endif
#ifndef EELSCRIPT_NO_LICE
  p = eel_lice_function_reference;
  while (*p) { fs->Add(p); p += strlen(p) + 1; }
#endif

}


#endif

#undef opaque

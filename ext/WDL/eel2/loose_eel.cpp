#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

int g_verbose, g_interactive;

static void writeToStandardError(const char *fmt, ...)
{
  va_list arglist;
  va_start(arglist, fmt);
  vfprintf(stderr,fmt,arglist);
  fprintf(stderr,"\n");
  fflush(stderr);
  va_end(arglist);
}
#define EEL_STRING_DEBUGOUT writeToStandardError // no parameters, since it takes varargs

#ifndef EEL_LICE_WANT_STANDALONE
  #define EELSCRIPT_NO_LICE
#endif

#include "eelscript.h"


void NSEEL_HOSTSTUB_EnterMutex() { }
void NSEEL_HOSTSTUB_LeaveMutex() { }


int main(int argc, char **argv)
{
  bool want_args = true;
  int argpos = 1;
  const char *scriptfn = argv[0];
  while (argpos < argc && argv[argpos][0] == '-' && argv[argpos][1])
  {
    if (!strcmp(argv[argpos],"-v")) g_verbose++;
    else if (!strcmp(argv[argpos],"-i")) g_interactive++;
    else if (!strcmp(argv[argpos],"--no-args")) want_args=false;
    else
    {
      fprintf(stderr,"Usage: %s [-v] [--no-args] [-i | scriptfile | -]\n",argv[0]);
      return -1;
    }
    argpos++;
  }
  if (argpos < argc && !g_interactive)
  {
    scriptfn = argv[argpos++];
  }
  else
  {
#ifndef _WIN32
    if (!g_interactive && isatty(0)) 
#else
    if (1)
#endif
       g_interactive=1;
  }

  if (eelScriptInst::init())
  {
    fprintf(stderr,"NSEEL_init(): error initializing\n");
    return -1;
  }


#ifndef EELSCRIPT_NO_LICE
  #ifdef __APPLE__
    SWELL_InitAutoRelease();
  #endif
#endif

  WDL_FastString code,t;

  eelScriptInst inst;
  if (want_args)
  {
    const int argv_offs = 1<<22;
    code.SetFormatted(64,"argc=0; argv=%d;\n",argv_offs);
    int x;
    for (x=argpos-1;x<argc;x++)
    {
      code.AppendFormatted(64,"argv[argc]=%d; argc+=1;\n",
          inst.m_string_context->AddString(new WDL_FastString(x<argpos ? scriptfn : argv[x])));
    }
    inst.runcode(code.Get(),2,"__cmdline__",true,true,true);
  }

  if (g_interactive)
  { 
#ifndef EELSCRIPT_NO_LICE
    if (inst.m_gfx_state && inst.m_gfx_state->m_gfx_clear) inst.m_gfx_state->m_gfx_clear[0] = -1;
#endif

    printf("EEL interactive mode, type quit to quit, abort to abort multiline entry\n");
    EEL_F *resultVar = NSEEL_VM_regvar(inst.m_vm,"__result");
    code.Set("");
    char line[4096];
    for (;;)
    {
#ifndef EELSCRIPT_NO_LICE
      _gfx_update(&inst,NULL);
#endif
      if (!code.Get()[0]) printf("EEL> ");
      else printf("> ");
      fflush(stdout);
      line[0]=0;
      fgets(line,sizeof(line),stdin);
      if (!line[0]) break;
      code.Append(line);
      while (line[0] && (
               line[strlen(line)-1] == '\r' ||
               line[strlen(line)-1] == '\n' ||
               line[strlen(line)-1] == '\t' ||
               line[strlen(line)-1] == ' '
              )) line[strlen(line)-1]=0;

      if (!strcmp(line,"quit")) break;
      if (!strcmp(line,"abort")) code.Set("");

      t.Set("__result = (");
      t.Append(code.Get());
      t.Append(");");
      int res=inst.runcode(t.Get(),false,"",true,true,true); // allow free, since functions can't be defined locally
      if (!res)
      {
        if (resultVar) printf("=%g ",*resultVar);
        code.Set("");
      }
      else // try compiling again allowing function definitions (and not allowing free)
           // but show errors if not continuation 
      {
        res=inst.runcode(code.Get(),true,"(stdin)", false,false,true);
        if (res<=0) code.Set("");
        // res>0 means need more lines
      }
      while (inst.run_deferred());
    }
  }
  else
  {
    inst.loadfile(scriptfn,NULL,true);
    while (inst.run_deferred());
  }

  return 0;
}

#ifndef _WIN32
INT_PTR SWELLAppMain(int msg, INT_PTR parm1, INT_PTR parm2)
{
  return 0;
}
#endif

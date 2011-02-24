#include "rar.hpp"

#if !defined(GUI) && !defined(RARDLL)
int main(int argc, char *argv[])
{

#ifdef _UNIX
  setlocale(LC_ALL,"");
#endif

#if defined(_EMX) && !defined(_DJGPP)
  uni_init(0);
#endif

#if !defined(_SFX_RTL_) && !defined(_WIN_ALL)
  setbuf(stdout,NULL);
#endif

#if !defined(SFX_MODULE) && defined(_EMX)
  EnumConfigPaths(argv[0],-1);
#endif

  ErrHandler.SetSignalHandlers(true);

  RARInitData();

#ifdef SFX_MODULE
  char ModuleNameA[NM];
  wchar ModuleNameW[NM];
#ifdef _WIN_ALL
  GetModuleFileNameW(NULL,ModuleNameW,ASIZE(ModuleNameW));
  WideToChar(ModuleNameW,ModuleNameA);
#else
  strcpy(ModuleNameA,argv[0]);
  *ModuleNameW=0;
#endif
#endif

#ifdef _WIN_ALL
  SetErrorMode(SEM_NOALIGNMENTFAULTEXCEPT|SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);


#endif

#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(SHELL_EXT)
  bool ShutdownOnClose;
#endif

#ifdef ALLOW_EXCEPTIONS
  try 
#endif
  {
  
    CommandData Cmd;
#ifdef SFX_MODULE
    strcpy(Cmd.Command,"X");
    char *Switch=NULL;
#ifdef _SFX_RTL_
    char *CmdLine=GetCommandLineA();
    if (CmdLine!=NULL && *CmdLine=='\"')
      CmdLine=strchr(CmdLine+1,'\"');
    if (CmdLine!=NULL && (CmdLine=strpbrk(CmdLine," /"))!=NULL)
    {
      while (IsSpace(*CmdLine))
        CmdLine++;
      Switch=CmdLine;
    }
#else
    Switch=argc>1 ? argv[1]:NULL;
#endif
    if (Switch!=NULL && Cmd.IsSwitch(Switch[0]))
    {
      int UpperCmd=etoupper(Switch[1]);
      switch(UpperCmd)
      {
        case 'T':
        case 'V':
          Cmd.Command[0]=UpperCmd;
          break;
        case '?':
          Cmd.OutHelp();
          break;
      }
    }
    Cmd.AddArcName(ModuleNameA,ModuleNameW);
#else
    if (Cmd.IsConfigEnabled(argc,argv))
    {
      Cmd.ReadConfig(argc,argv);
      Cmd.ParseEnvVar();
    }
    for (int I=1;I<argc;I++)
      Cmd.ParseArg(argv[I],NULL);
#endif
    Cmd.ParseDone();

#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(SHELL_EXT)
    ShutdownOnClose=Cmd.Shutdown;
#endif

    InitConsoleOptions(Cmd.MsgStream,Cmd.Sound);
    InitLogOptions(Cmd.LogName);
    ErrHandler.SetSilent(Cmd.AllYes || Cmd.MsgStream==MSG_NULL);
    ErrHandler.SetShutdown(Cmd.Shutdown);

    Cmd.OutTitle();
    Cmd.ProcessCommand();
  }
#ifdef ALLOW_EXCEPTIONS
  catch (int ErrCode)
  {
    ErrHandler.SetErrorCode(ErrCode);
  }
#ifdef ENABLE_BAD_ALLOC
  catch (bad_alloc)
  {
    ErrHandler.SetErrorCode(MEMORY_ERROR);
  }
#endif
  catch (...)
  {
    ErrHandler.SetErrorCode(FATAL_ERROR);
  }
#endif
  File::RemoveCreated();
#if defined(SFX_MODULE) && defined(_DJGPP)
  _chmod(ModuleNameA,1,0x20);
#endif
#if defined(_EMX) && !defined(_DJGPP)
  uni_done();
#endif
#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(SHELL_EXT)
  if (ShutdownOnClose)
    Shutdown();
#endif
  return(ErrHandler.GetErrorCode());
}
#endif



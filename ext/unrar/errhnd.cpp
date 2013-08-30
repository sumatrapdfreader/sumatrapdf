#include "rar.hpp"


ErrorHandler::ErrorHandler()
{
  Clean();
}


void ErrorHandler::Clean()
{
  ExitCode=RARX_SUCCESS;
  ErrCount=0;
  EnableBreak=true;
  Silent=false;
  DoShutdown=false;
  UserBreak=false;
  MainExit=false;
}


void ErrorHandler::MemoryError()
{
  MemoryErrorMsg();
  Throw(RARX_MEMORY);
}


void ErrorHandler::OpenError(const wchar *FileName)
{
#ifndef SILENT
  OpenErrorMsg(FileName);
  Throw(RARX_OPEN);
#endif
}


void ErrorHandler::CloseError(const wchar *FileName)
{
#ifndef SILENT
  if (!UserBreak)
  {
    Log(NULL,St(MErrFClose),FileName);
    SysErrMsg();
  }
#endif
#if !defined(SILENT) || defined(RARDLL)
  Throw(RARX_FATAL);
#endif
}


void ErrorHandler::ReadError(const wchar *FileName)
{
#ifndef SILENT
  ReadErrorMsg(FileName);
#endif
#if !defined(SILENT) || defined(RARDLL)
  Throw(RARX_FATAL);
#endif
}


bool ErrorHandler::AskRepeatRead(const wchar *FileName)
{
#if !defined(SILENT) && !defined(SFX_MODULE)
  if (!Silent)
  {
    SysErrMsg();
    mprintf(L"\n");
    Log(NULL,St(MErrRead),FileName);
    return Ask(St(MRetryAbort))==1;
  }
#endif
  return false;
}


void ErrorHandler::WriteError(const wchar *ArcName,const wchar *FileName)
{
#ifndef SILENT
  WriteErrorMsg(ArcName,FileName);
#endif
#if !defined(SILENT) || defined(RARDLL)
  Throw(RARX_WRITE);
#endif
}


#ifdef _WIN_ALL
void ErrorHandler::WriteErrorFAT(const wchar *FileName)
{
#if !defined(SILENT) && !defined(SFX_MODULE)
  SysErrMsg();
  Log(NULL,St(MNTFSRequired),FileName);
#endif
#if !defined(SILENT) && !defined(SFX_MODULE) || defined(RARDLL)
  Throw(RARX_WRITE);
#endif
}
#endif


bool ErrorHandler::AskRepeatWrite(const wchar *FileName,bool DiskFull)
{
#ifndef SILENT
  if (!Silent)
  {
    SysErrMsg();
    mprintf(L"\n");
    Log(NULL,St(DiskFull ? MNotEnoughDisk:MErrWrite),FileName);
    return Ask(St(MRetryAbort))==1;
  }
#endif
  return false;
}


void ErrorHandler::SeekError(const wchar *FileName)
{
#ifndef SILENT
  if (!UserBreak)
  {
    Log(NULL,St(MErrSeek),FileName);
    SysErrMsg();
  }
#endif
#if !defined(SILENT) || defined(RARDLL)
  Throw(RARX_FATAL);
#endif
}


void ErrorHandler::GeneralErrMsg(const wchar *fmt,...)
{
  va_list arglist;
  va_start(arglist,fmt);
  wchar Msg[1024];
  vswprintf(Msg,ASIZE(Msg),fmt,arglist);
#ifndef SILENT
  Log(NULL,L"%ls",Msg);
  mprintf(L"\n");
  SysErrMsg();
#endif
  va_end(arglist);
}


void ErrorHandler::MemoryErrorMsg()
{
#ifndef SILENT
  Log(NULL,St(MErrOutMem));
#endif
}


void ErrorHandler::OpenErrorMsg(const wchar *FileName)
{
  OpenErrorMsg(NULL,FileName);
}


void ErrorHandler::OpenErrorMsg(const wchar *ArcName,const wchar *FileName)
{
#ifndef SILENT
  if (FileName!=NULL)
    Log(ArcName,St(MCannotOpen),FileName);
  SysErrMsg();
#endif
}


void ErrorHandler::CreateErrorMsg(const wchar *FileName)
{
  CreateErrorMsg(NULL,FileName);
}


void ErrorHandler::CreateErrorMsg(const wchar *ArcName,const wchar *FileName)
{
#ifndef SILENT
  Log(ArcName,St(MCannotCreate),FileName);

#if defined(_WIN_ALL) && defined(MAX_PATH)
  CheckLongPathErrMsg(FileName);
#endif

  SysErrMsg();
#endif
}


// Check the path length and display the error message if it is too long.
void ErrorHandler::CheckLongPathErrMsg(const wchar *FileName)
{
#if defined(_WIN_ALL) && !defined (SILENT) && defined(MAX_PATH)
  if (GetLastError()==ERROR_PATH_NOT_FOUND)
  {
    size_t NameLength=wcslen(FileName);
    if (!IsFullPath(FileName))
    {
      wchar CurDir[NM];
      GetCurrentDirectory(ASIZE(CurDir),CurDir);
      NameLength+=wcslen(CurDir)+1;
    }
    if (NameLength>MAX_PATH)
    {
      Log(NULL,St(MMaxPathLimit),MAX_PATH);
    }
  }
#endif
}


void ErrorHandler::ReadErrorMsg(const wchar *FileName)
{
  ReadErrorMsg(NULL,FileName);
}


void ErrorHandler::ReadErrorMsg(const wchar *ArcName,const wchar *FileName)
{
#ifndef SILENT
  Log(ArcName,St(MErrRead),FileName);
  SysErrMsg();
#endif
}


void ErrorHandler::WriteErrorMsg(const wchar *ArcName,const wchar *FileName)
{
#ifndef SILENT
  Log(ArcName,St(MErrWrite),FileName);
  SysErrMsg();
#endif
}


void ErrorHandler::Exit(RAR_EXIT ExitCode)
{
#ifndef GUI
  Alarm();
#endif
  Throw(ExitCode);
}


void ErrorHandler::SetErrorCode(RAR_EXIT Code)
{
  switch(Code)
  {
    case RARX_WARNING:
    case RARX_USERBREAK:
      if (ExitCode==RARX_SUCCESS)
        ExitCode=Code;
      break;
    case RARX_FATAL:
      if (ExitCode==RARX_SUCCESS || ExitCode==RARX_WARNING)
        ExitCode=RARX_FATAL;
      break;
    default:
      ExitCode=Code;
      break;
  }
  ErrCount++;
}


#ifndef GUI
#ifdef _WIN_ALL
BOOL __stdcall ProcessSignal(DWORD SigType)
#else
#if defined(__sun)
extern "C"
#endif
void _stdfunction ProcessSignal(int SigType)
#endif
{
#ifdef _WIN_ALL
  // When a console application is run as a service, this allows the service
  // to continue running after the user logs off. 
  if (SigType==CTRL_LOGOFF_EVENT)
    return TRUE;
#endif

  ErrHandler.UserBreak=true;
  mprintf(St(MBreak));

#ifdef _WIN_ALL
  // Let the main thread to handle 'throw' and destroy file objects.
  for (uint I=0;!ErrHandler.MainExit && I<50;I++)
    Sleep(100);
#if defined(USE_RC) && !defined(SFX_MODULE) && !defined(RARDLL)
  ExtRes.UnloadDLL();
#endif
  exit(RARX_USERBREAK);
#endif

#ifdef _UNIX
  static uint BreakCount=0;
  // User continues to press Ctrl+C, exit immediately without cleanup.
  if (++BreakCount>1)
    exit(RARX_USERBREAK);
  // Otherwise return from signal handler and let Wait() function to close
  // files and quit. We cannot use the same approach as in Windows,
  // because Unix signal handler can block execution of our main code.
#endif

#if defined(_WIN_ALL) && !defined(_MSC_VER)
  // never reached, just to avoid a compiler warning
  return TRUE;
#endif
}
#endif


void ErrorHandler::SetSignalHandlers(bool Enable)
{
  EnableBreak=Enable;
#ifndef GUI
#ifdef _WIN_ALL
  SetConsoleCtrlHandler(Enable ? ProcessSignal:NULL,TRUE);
//  signal(SIGBREAK,Enable ? ProcessSignal:SIG_IGN);
#else
  signal(SIGINT,Enable ? ProcessSignal:SIG_IGN);
  signal(SIGTERM,Enable ? ProcessSignal:SIG_IGN);
#endif
#endif
}


void ErrorHandler::Throw(RAR_EXIT Code)
{
  if (Code==RARX_USERBREAK && !EnableBreak)
    return;
#if !defined(GUI) && !defined(SILENT)
  // Do not write "aborted" when just displaying online help.
  if (Code!=RARX_SUCCESS && Code!=RARX_USERERROR)
    mprintf(L"\n%s\n",St(MProgAborted));
#endif
  ErrHandler.SetErrorCode(Code);
  throw Code;
}


void ErrorHandler::SysErrMsg()
{
#if !defined(SFX_MODULE) && !defined(SILENT)
#ifdef _WIN_ALL
  wchar *lpMsgBuf=NULL;
  int ErrType=GetLastError();
  if (ErrType!=0 && FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
              NULL,ErrType,MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
              (LPTSTR)&lpMsgBuf,0,NULL))
  {
    wchar *CurMsg=lpMsgBuf;
    while (CurMsg!=NULL)
    {
      while (*CurMsg=='\r' || *CurMsg=='\n')
        CurMsg++;
      if (*CurMsg==0)
        break;
      wchar *EndMsg=wcschr(CurMsg,'\r');
      if (EndMsg==NULL)
        EndMsg=wcschr(CurMsg,'\n');
      if (EndMsg!=NULL)
      {
        *EndMsg=0;
        EndMsg++;
      }
      Log(NULL,L"\n%ls",CurMsg);
      CurMsg=EndMsg;
    }
  }
  LocalFree( lpMsgBuf );
#endif

#if defined(_UNIX) || defined(_EMX)
  if (errno!=0)
  {
    char *err=strerror(errno);
    if (err!=NULL)
    {
      wchar MsgW[1024];
      CharToWide(err,MsgW,ASIZE(MsgW));
      Log(NULL,L"\n%s",MsgW);
    }
  }
#endif

#endif
}


int ErrorHandler::GetSystemErrorCode()
{
#ifdef _WIN_ALL
  return GetLastError();
#else
  return errno;
#endif
}


void ErrorHandler::SetSystemErrorCode(int Code)
{
#ifdef _WIN_ALL
  SetLastError(Code);
#else
  errno=Code;
#endif
}

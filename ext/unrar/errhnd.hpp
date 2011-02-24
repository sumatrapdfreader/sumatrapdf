#ifndef _RAR_ERRHANDLER_
#define _RAR_ERRHANDLER_

#if (defined(GUI) || !defined(_WIN_ALL)) && !defined(SFX_MODULE) && !defined(_WIN_CE) || defined(RARDLL)
#define ALLOW_EXCEPTIONS
#endif

enum { SUCCESS,WARNING,FATAL_ERROR,CRC_ERROR,LOCK_ERROR,WRITE_ERROR,
       OPEN_ERROR,USER_ERROR,MEMORY_ERROR,CREATE_ERROR,NO_FILES_ERROR,
       USER_BREAK=255};

class ErrorHandler
{
  private:
    void ErrMsg(const char *ArcName,const char *fmt,...);

    int ExitCode;
    int ErrCount;
    bool EnableBreak;
    bool Silent;
    bool DoShutdown;
  public:
    ErrorHandler();
    void Clean();
    void MemoryError();
    void OpenError(const char *FileName,const wchar *FileNameW);
    void CloseError(const char *FileName,const wchar *FileNameW);
    void ReadError(const char *FileName,const wchar *FileNameW);
    bool AskRepeatRead(const char *FileName,const wchar *FileNameW);
    void WriteError(const char *ArcName,const wchar *ArcNameW,const char *FileName,const wchar *FileNameW);
    void WriteErrorFAT(const char *FileName,const wchar *FileNameW);
    bool AskRepeatWrite(const char *FileName,const wchar *FileNameW,bool DiskFull);
    void SeekError(const char *FileName,const wchar *FileNameW);
    void GeneralErrMsg(const char *Msg);
    void MemoryErrorMsg();
    void OpenErrorMsg(const char *FileName,const wchar *FileNameW=NULL);
    void OpenErrorMsg(const char *ArcName,const wchar *ArcNameW,const char *FileName,const wchar *FileNameW);
    void CreateErrorMsg(const char *FileName,const wchar *FileNameW=NULL);
    void CreateErrorMsg(const char *ArcName,const wchar *ArcNameW,const char *FileName,const wchar *FileNameW);
    void CheckLongPathErrMsg(const char *FileName,const wchar *FileNameW);
    void ReadErrorMsg(const char *ArcName,const wchar *ArcNameW,const char *FileName,const wchar *FileNameW);
    void WriteErrorMsg(const char *ArcName,const wchar *ArcNameW,const char *FileName,const wchar *FileNameW);
    void Exit(int ExitCode);
    void SetErrorCode(int Code);
    int GetErrorCode() {return(ExitCode);}
    int GetErrorCount() {return(ErrCount);}
    void SetSignalHandlers(bool Enable);
    void Throw(int Code);
    void SetSilent(bool Mode) {Silent=Mode;};
    void SetShutdown(bool Mode) {DoShutdown=Mode;};
    void SysErrMsg();
};


#endif

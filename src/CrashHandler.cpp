#include "SumatraPDF.h"

#include <dbghelp.h>
#include <process.h>
#include "CrashHandler.h"
#include "str_util.h"

typedef BOOL WINAPI MiniDumpWriteProc(
  HANDLE hProcess,
  DWORD ProcessId,
  HANDLE hFile,
  LONG DumpType,
  PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
  void * UserStreamParam,
  void * CallbackParam
);

typedef struct {
    DWORD                threadId;
    EXCEPTION_POINTERS * exceptionInfo;
} DumpThreadInfo;

static TCHAR g_crashDumpPath[MAX_PATH];
static TCHAR g_exePath[MAX_PATH];

static MiniDumpWriteProc *g_minidDumpWriteProc = NULL;

static bool InitDbgHelpDll()
{
#ifdef DEBUG
    static bool wasHere = false;
    assert(!wasHere);
    wasHere = true;
#endif

    HMODULE hdll = LoadLibraryA("DBGHELP.DLL");
    if (NULL == hdll)
        return false;

    g_minidDumpWriteProc = (MiniDumpWriteProc*)GetProcAddress(hdll, "MiniDumpWriteDump");
    return (g_minidDumpWriteProc != NULL);
}

static void GenPaths(const TCHAR *crashDumpDir, const TCHAR *crashDumpBasename)
{
    _tcscpy_s(g_crashDumpPath, dimof(g_crashDumpPath), crashDumpDir);
    if (!tstr_endswithi(g_crashDumpPath, DIR_SEP_TSTR))
        _tcscat_s(g_crashDumpPath, dimof(g_crashDumpPath), DIR_SEP_TSTR);
    _tcscat_s(g_crashDumpPath, dimof(g_crashDumpPath), crashDumpBasename);
    _tcscat_s(g_crashDumpPath, dimof(g_crashDumpPath), _T(".dmp"));
    GetModuleFileName(NULL, g_exePath, dimof(g_exePath));
}

static BOOL CALLBACK OpenMiniDumpCallback(void* /*param*/, MINIDUMP_CALLBACK_INPUT* input, MINIDUMP_CALLBACK_OUTPUT* output)
{
    if (!input || !output) 
        return FALSE; 

    ULONG ct = input->CallbackType;
    if (ModuleCallback == ct) {
        if (!(output->ModuleWriteFlags & ModuleReferencedByMemory))
            output->ModuleWriteFlags &= ~ModuleWriteModule; 
        return TRUE;
    } else if ( (IncludeModuleCallback == ct) ||
            (IncludeThreadCallback == ct) ||
            (ThreadCallback == ct) ||
            (ThreadExCallback == ct)) {
        return TRUE;
    }

    return FALSE;
}

static unsigned WINAPI CrushDumpThread(void* data)
{
    HANDLE dumpFile = INVALID_HANDLE_VALUE;
    DumpThreadInfo *dti = (DumpThreadInfo*)data;
    if (!dti)
        return 0;

    dumpFile = CreateFile(g_crashDumpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if (INVALID_HANDLE_VALUE == dumpFile)
        return 0;

    MINIDUMP_CALLBACK_INFORMATION mci; 
    mci.CallbackRoutine	 = (MINIDUMP_CALLBACK_ROUTINE)OpenMiniDumpCallback; 

    MINIDUMP_EXCEPTION_INFORMATION excInfo;
    excInfo.ThreadId = dti->threadId;
    excInfo.ExceptionPointers = dti->exceptionInfo;
    excInfo.ClientPointers = FALSE;

    MINIDUMP_TYPE type = (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory);
    //type |= MiniDumpWithDataSegs|MiniDumpWithHandleData|MiniDumpWithPrivateReadWriteMemory;
    BOOL ok = g_minidDumpWriteProc(GetCurrentProcess(), GetCurrentProcessId(), dumpFile, type, &excInfo, NULL, &mci);
    ///BOOL ok = minidDumpWriteProc(GetCurrentProcess(), GetCurrentProcessId(), dumpFile, type, &excInfo, NULL, NULL);
    UNUSED_VAR(ok);

    if (dumpFile != INVALID_HANDLE_VALUE)
        CloseHandle(dumpFile);

    exec_with_params(g_exePath, CMD_ARG_SEND_CRASHDUMP, TRUE /* hidden */);
    return 0;
}

static LONG WINAPI OpenDnsCrashHandler(EXCEPTION_POINTERS *exceptionInfo)
{
    static bool wasHere = false;
    if (wasHere)
        return EXCEPTION_CONTINUE_SEARCH;

    if (exceptionInfo && (EXCEPTION_BREAKPOINT == exceptionInfo->ExceptionRecord->ExceptionCode))
        return EXCEPTION_CONTINUE_SEARCH;

    wasHere = true;

    // we either forgot to call InitDbgHelpDll() or it failed to obtain address of
    // MiniDumpWriteDump(), so nothing we can do
    if (NULL == g_minidDumpWriteProc)
        return EXCEPTION_CONTINUE_SEARCH;

    // per msdn (which is backed by my experience), MiniDumpWriteDump() doesn't
    // write callstack for the calling thread correctly. We use msdn-recommended
    // work-around of spinning a thread to do the writing
    DumpThreadInfo dti;
    dti.exceptionInfo = exceptionInfo;
    dti.threadId = ::GetCurrentThreadId();

    unsigned tid;
    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, CrushDumpThread, &dti, 0, &tid) ;
    if ((HANDLE)-1 != hThread )
    {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void InstallCrashHandler(const TCHAR *crashDumpDir, const TCHAR *crashDumpBaseName)
{
    // do as much work as possible here (as opposed to in crash handler)
    // the downside is that startup time might suffer due to loading of dbghelp.dll
    bool ok = InitDbgHelpDll();
    if (!ok)
        return;
    GenPaths(crashDumpDir, crashDumpBaseName);
    SetUnhandledExceptionFilter(OpenDnsCrashHandler);
}

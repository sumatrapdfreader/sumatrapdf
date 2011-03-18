/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <windows.h>
#include <dbghelp.h>
#include "CrashHandler.h"

#include "BaseUtil.h"
#include "StrUtil.h"
#include "WinUtil.h"
#include "translations.h"

typedef BOOL WINAPI MiniDumpWriteProc(
    HANDLE hProcess,
    DWORD ProcessId,
    HANDLE hFile,
    LONG DumpType,
    PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
    PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
    PMINIDUMP_CALLBACK_INFORMATION CallbackParam
);

static TCHAR g_crashDumpPath[MAX_PATH];
static HANDLE g_dumpEvent = NULL;
static HANDLE g_dumpThread = NULL;
static MINIDUMP_EXCEPTION_INFORMATION mei = { 0 };

static BOOL CALLBACK OpenMiniDumpCallback(void* /*param*/, PMINIDUMP_CALLBACK_INPUT input, PMINIDUMP_CALLBACK_OUTPUT output)
{
    if (!input || !output) 
        return FALSE; 

    switch (input->CallbackType) {
    case ModuleCallback:
        if (!(output->ModuleWriteFlags & ModuleReferencedByMemory))
            output->ModuleWriteFlags &= ~ModuleWriteModule; 
        return TRUE;
    case IncludeModuleCallback:
    case IncludeThreadCallback:
    case ThreadCallback:
    case ThreadExCallback:
        return TRUE;
    default:
        return FALSE;
    }
}

static DWORD WINAPI CrashDumpThread(LPVOID data)
{
    WaitForSingleObject(g_dumpEvent, INFINITE);

    WinLibrary lib(_T("DBGHELP.DLL"));
    MiniDumpWriteProc *pMiniDumpWriteDump = (MiniDumpWriteProc *)lib.GetProcAddr("MiniDumpWriteDump");
    if (!pMiniDumpWriteDump)
    {
#ifdef SVN_PRE_RELEASE_VER
        MessageBox(NULL, _T("Couldn't create a crashdump file: dbghelp.dll is unexpectedly missing."), _TR("SumatraPDF crashed"), MB_ICONEXCLAMATION | MB_OK);
#endif
        return 0;
    }

    HANDLE dumpFile = CreateFile(g_crashDumpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    if (INVALID_HANDLE_VALUE == dumpFile)
    {
#ifdef SVN_PRE_RELEASE_VER
        MessageBox(NULL, _T("Couldn't create a crashdump file."), _TR("SumatraPDF crashed"), MB_ICONEXCLAMATION | MB_OK);
#endif
        return 0;
    }

    MINIDUMP_TYPE type = (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory);
    // set the SUMATRAPDF_FULLDUMP environment variable for far more complete minidumps
    if (GetEnvironmentVariable(_T("SUMATRAPDF_FULLDUMP"), NULL, 0))
        type = (MINIDUMP_TYPE)(type | MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithPrivateReadWriteMemory);
    MINIDUMP_CALLBACK_INFORMATION mci = { OpenMiniDumpCallback, NULL }; 

    pMiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFile, type, &mei, NULL, &mci);

    CloseHandle(dumpFile);

    // exec_with_params(g_exePath, CMD_ARG_SEND_CRASHDUMP, TRUE /* hidden */);
    return 0;
}

static LONG WINAPI DumpExceptionHandler(EXCEPTION_POINTERS *exceptionInfo)
{
    if (!exceptionInfo || (EXCEPTION_BREAKPOINT == exceptionInfo->ExceptionRecord->ExceptionCode))
        return EXCEPTION_CONTINUE_SEARCH;

    static bool wasHere = false;
    if (wasHere)
        return EXCEPTION_CONTINUE_SEARCH;
    wasHere = true;

    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = exceptionInfo;
    // per msdn (which is backed by my experience), MiniDumpWriteDump() doesn't
    // write callstack for the calling thread correctly. We use msdn-recommended
    // work-around of spinning a thread to do the writing
    SetEvent(g_dumpEvent);
    WaitForSingleObject(g_dumpThread, INFINITE);

    ScopedMem<TCHAR> msg(Str::Format(_T("%s\n\n%s"), _TR("Please include the following file in your crash report:"), g_crashDumpPath));
    MessageBox(NULL, msg.Get(), _TR("SumatraPDF crashed"), MB_ICONERROR | MB_OK);

    return EXCEPTION_CONTINUE_SEARCH;
}

void InstallCrashHandler(const TCHAR *crashDumpPath)
{
    if (NULL == crashDumpPath)
        return;
    Str::BufSet(g_crashDumpPath, dimof(g_crashDumpPath), crashDumpPath);
    if (!g_dumpEvent && !g_dumpThread) {
        g_dumpEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        g_dumpThread = CreateThread(NULL, 0, CrashDumpThread, NULL, 0, 0);

        SetUnhandledExceptionFilter(DumpExceptionHandler);
    }
}

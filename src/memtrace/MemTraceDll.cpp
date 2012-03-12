/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

/*
memtrace.dll is system for tracing memory allocations in arbitrary programs.
It hooks RtlFreeHeap etc. APIs in the process and sends collected information
(allocation address/size, address of freed memory, callstacks if possible etc.)
to an external collection and visualization process via named pipe.

The dll can either be injected into arbitrary processes (to trace arbitrary
processes) or an app can specifically load it to activate memory tracing.

If the collection process doesn't run when memtrace.dll is initialized, it does
nothing.
*/

#include "BaseUtil.h"
#include "MemTraceDll.h"

#include "StrUtil.h"

#define NOLOG 0  // always log
#include "DebugLog.h"

static HANDLE gModule;
static HANDLE gPipe;

#define PIPE_NAME "\\\\.\\pipe\\MemTraceCollectorPipe"

static bool WriteToPipe(const char *s)
{
    DWORD size;
    if (!gPipe)
        return false;
    DWORD sLen = str::Len(s);
    BOOL ok = WriteFile(gPipe, s, (DWORD)sLen, &size, NULL);
    if (!ok || (size != sLen))
        return false;
    return true;
}

static bool TryOpenPipe()
{
    gPipe = CreateFileA(PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING,
        SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION | FILE_FLAG_OVERLAPPED, NULL);
    if (INVALID_HANDLE_VALUE == gPipe) {
        gPipe = NULL;
        return false;
    }
    WriteToPipe("hello, sailor");
    return true;
}

static void ClosePipe()
{
    if (gPipe && (INVALID_HANDLE_VALUE != gPipe))
        CloseHandle(gPipe);
    gPipe = NULL;
}

static BOOL ProcessAttach()
{
    lf("ProcessAttach()");
    if (!TryOpenPipe()) {
        lf("couldn't open pipe");
    } else {
        lf("opened pipe");
    }
    return TRUE;
}

static BOOL ProcessDetach()
{
    lf("ProcessDetach()");
    ClosePipe();
    return TRUE;
}

static BOOL ThreadAttach()
{
    return TRUE;
}

static BOOL ThreadDetach()
{
    return TRUE;
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
    gModule = hModule;
    if (DLL_PROCESS_ATTACH == dwReason)
        return ProcessAttach();
    if (DLL_PROCESS_DETACH == dwReason)
        return ProcessDetach();
    if (DLL_THREAD_ATTACH == dwReason)
        return ThreadAttach();
    if (DLL_THREAD_DETACH == dwReason)
        return ThreadDetach();

    return TRUE;
}

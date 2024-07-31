/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "ScopedWin.h"
#include "WinDynCalls.h"
#include "WinUtil.h"

#include "ThreadUtil.h"

#if COMPILER_MSVC

// http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
const DWORD MS_VC_EXCEPTION = 0x406D1388;

#include <pshpack8.h>

typedef struct tagTHREADNAME_INFO {
    DWORD dwType;     // Must be 0x1000.
    LPCSTR szName;    // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags;    // Reserved for future use, must be zero.
} THREADNAME_INFO;

#include <poppack.h>

#pragma warning(push)
#pragma warning(disable : 6320) // silence /analyze: Exception-filter expression is the constant
                                // EXCEPTION_EXECUTE_HANDLER. This might mask exceptions that were
                                // not intended to be handled
#pragma warning(disable : 6322) // silence /analyze: Empty _except block
void SetThreadName(const char* threadName, DWORD threadId) {
    if (DynSetThreadDescription && threadId == 0) {
        TempWStr ws = ToWStrTemp(threadName);
        DynSetThreadDescription(GetCurrentThread(), ws);
        return;
    }

    if (threadId == 0) {
        threadId = GetCurrentThreadId();
    }
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadID = threadId;
    info.dwFlags = 0;

    __try {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}
#pragma warning(push)
#else
void SetThreadName(DWORD, const char*) {
    // nothing
}
#endif // COMPILER_MSVC

static DWORD WINAPI ThreadFunc0(void* data) {
    auto* fn = (Func0*)(data);
    fn->Call();
    delete fn;
    DestroyTempAllocator();
    return 0;
}

HANDLE StartThread(const Func0& fn, const char* threadName) {
    auto fp = new Func0(fn);
    DWORD threadId = 0;
    HANDLE hThread = CreateThread(nullptr, 0, ThreadFunc0, (void*)fp, 0, &threadId);
    if (!hThread) {
        return nullptr;
    }
    if (threadName != nullptr) {
        SetThreadName(threadName, threadId);
    }
    return hThread;
}

void RunAsync(const Func0& fn, const char* threadName) {
    HANDLE hThread = StartThread(fn, threadName);
    SafeCloseHandle(&hThread);
}

AtomicInt gDangerousThreadCount;

bool AreDangerousThreadsPending() {
    auto count = gDangerousThreadCount.Get();
    return count != 0;
}

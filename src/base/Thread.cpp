/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Base.h"

#if OS_WIN
#include "ScopedWin.h"
#include "WinDynCalls.h"
#include "Win.h"
#else
#include <unistd.h>
#if OS_LINUX
#include <sys/syscall.h>
#endif
#endif

#if OS_WIN && COMPILER_MSVC

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
void SetThreadName(Str threadName, ThreadId threadId) {
    if (!threadName) {
        return;
    }
    if (DynSetThreadDescription && threadId == 0) {
        WCHAR* ws = CWStrTemp(threadName);
        DynSetThreadDescription(GetCurrentThread(), ws);
        return;
    }

    if (threadId == 0) {
        threadId = GetCurrentThreadId();
    }
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = CStrTemp(threadName);
    info.dwThreadID = threadId;
    info.dwFlags = 0;

    __try {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}
#pragma warning(pop)

#elif OS_WIN

void SetThreadName(Str, ThreadId) {}

#else

ThreadId GetCurrentThreadId() {
#if OS_DARWIN
    u64 tid = 0;
    pthread_threadid_np(nullptr, &tid);
    return tid;
#elif OS_LINUX
    return (ThreadId)syscall(SYS_gettid);
#else
    return (ThreadId)pthread_self();
#endif
}

void SetThreadName(Str threadName, ThreadId threadId) {
    if (!threadName) {
        return;
    }
    if (threadId != 0 && threadId != GetCurrentThreadId()) {
        return;
    }
#if OS_DARWIN
    char buf[64];
    size_t n = (size_t)std::min(threadName.len, (int)sizeof(buf) - 1);
    memcpy(buf, threadName.s, n);
    buf[n] = 0;
    pthread_setname_np(buf);
#elif OS_LINUX
    char buf[16];
    size_t n = (size_t)std::min(threadName.len, (int)sizeof(buf) - 1);
    memcpy(buf, threadName.s, n);
    buf[n] = 0;
    pthread_setname_np(pthread_self(), buf);
#endif
}

#endif

#if OS_WIN

static DWORD WINAPI ThreadFunc0(void* data) {
    auto* fn = (Func0*)(data);
    fn->Call();
    delete fn;
    DestroyTempArena();
    return 0;
}

ThreadHandle StartThread(const Func0& fn, Str threadName) {
    auto fp = new Func0(fn);
    ThreadId threadId = 0;
    ThreadHandle hThread = CreateThread(nullptr, 0, ThreadFunc0, (void*)fp, 0, &threadId);
    if (!hThread) {
        delete fp;
        return nullptr;
    }
    if (threadName) {
        SetThreadName(threadName, threadId);
    }
    return hThread;
}

#else

struct ThreadHandlePosix {
    pthread_t thread;
};

struct ThreadFuncData {
    Func0 fn;
    Str threadName;

    ThreadFuncData(const Func0& fn, Str threadName) : fn(fn) { this->threadName = str::Dup(threadName); }
    ~ThreadFuncData() { str::Free(threadName); }
};

static void* ThreadFunc0(void* data) {
    auto* threadData = (ThreadFuncData*)data;
    if (threadData->threadName) {
        SetThreadName(threadData->threadName);
    }
    threadData->fn.Call();
    delete threadData;
    DestroyTempArena();
    return nullptr;
}

ThreadHandle StartThread(const Func0& fn, Str threadName) {
    auto threadData = new ThreadFuncData(fn, threadName);
    auto hThread = new ThreadHandlePosix();
    int err = pthread_create(&hThread->thread, nullptr, ThreadFunc0, threadData);
    if (err != 0) {
        delete hThread;
        delete threadData;
        return nullptr;
    }
    return hThread;
}

bool SafeCloseThreadHandle(ThreadHandle* hPtr) {
    ThreadHandle h = *hPtr;
    if (!h) {
        return false;
    }
    int err = pthread_detach(h->thread);
    delete h;
    *hPtr = nullptr;
    return err == 0;
}

#endif

void RunAsync(const Func0& fn, Str threadName) {
    ThreadHandle hThread = StartThread(fn, threadName);
    SafeCloseThreadHandle(&hThread);
}

void SleepInMs(int ms) {
    if (ms <= 0) {
        return;
    }
#if OS_WIN
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

AtomicInt gDangerousThreadCount = 0;

bool AreDangerousThreadsPending() {
    auto count = AtomicIntGet(&gDangerousThreadCount);
    return count != 0;
}

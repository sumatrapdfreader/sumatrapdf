/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "ScopedWin.h"
#include "WinDynCalls.h"

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

// We need a way to uniquely identified threads (so that we can test for equality).
// Thread id assigned by the OS might be recycled. The memory address given to ThreadBase
// object can be recycled as well, so we keep our own counter.
static int GenUniqueThreadId() {
    static LONG gThreadNoSeq = 0;
    return (int)InterlockedIncrement(&gThreadNoSeq);
}

// TODO: unused, remove

/* A very simple thread class that allows stopping a thread */
class ThreadBase {
  private:
    LONG cancelRequested = 0;
    int threadNo = 0;
    HANDLE hThread = nullptr;

    static DWORD WINAPI ThreadProc(void* data);

  protected:
    // for debugging
    AutoFreeStr threadName;

    virtual ~ThreadBase();

    bool WasCancelRequested() {
        LONG res = InterlockedAdd(&cancelRequested, 0);
        return res > 0;
    }

  public:
    // name is for debugging purposes, can be nullptr.
    explicit ThreadBase(const char* name = nullptr);

    // call this to start executing Run() function.
    void Start();

    // request the thread to stop. It's up to Run() function
    // to call WasCancelRequested() and stop processing if it returns true.
    void RequestCancel() {
        InterlockedIncrement(&cancelRequested);
    }

    // synchronously waits for the thread to end
    // returns true if thread stopped by itself and false if waiting timed out
    bool Join(DWORD waitMs = INFINITE);

    // get a unique number that identifies a thread and unlike an
    // address of the object, will not be reused
    LONG GetNo() const {
        return threadNo;
    }

    // over-write this to implement the actual thread functionality
    // note: for longer running threads, make sure to occasionally poll WasCancelRequested
    virtual void Run() = 0;
};

ThreadBase::ThreadBase(const char* name) {
    threadNo = GenUniqueThreadId();
    threadName = str::Dup(name);
    // lf("ThreadBase() %d", threadNo);
}

ThreadBase::~ThreadBase() {
    // lf("~ThreadBase() %d", threadNo);
    CloseHandle(hThread);
}

DWORD WINAPI ThreadBase::ThreadProc(void* data) {
    ThreadBase* thread = reinterpret_cast<ThreadBase*>(data);
    if (thread->threadName) {
        SetThreadName(thread->threadName);
    }
    thread->Run();
    return 0;
}

void ThreadBase::Start() {
    ReportIf(hThread);
    hThread = CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr);
}

bool ThreadBase::Join(DWORD waitMs) {
    DWORD res = WaitForSingleObject(hThread, waitMs);
    if (WAIT_OBJECT_0 == res) {
        CloseHandle(hThread);
        hThread = nullptr;
        return true;
    }
    return false;
}

static DWORD WINAPI ThreadFunc0(void* data) {
    auto* fn = (Func0*)(data);
    fn->Call();
    delete fn;
    DestroyTempAllocator();
    return 0;
}

void RunAsync(const Func0& fn, const char* threadName) {
    auto fp = new Func0(fn);
    DWORD threadId = 0;
    HANDLE hThread = CreateThread(nullptr, 0, ThreadFunc0, (void*)fp, 0, &threadId);
    if (!hThread) {
        return;
    }
    if (threadName != nullptr) {
        SetThreadName(threadName, threadId);
    }
    CloseHandle(hThread);
}

AtomicInt gDangerousThreadCount;

bool AreDangerousThreadsPending() {
    auto count = gDangerousThreadCount.Get();
    return count != 0;
}

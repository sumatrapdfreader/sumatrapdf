/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "ThreadUtil.h"

#if defined(_MSC_VER)

// http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
const DWORD MS_VC_EXCEPTION=0x406D1388;

#include <pshpack8.h>

typedef struct tagTHREADNAME_INFO
{
    DWORD dwType;       // Must be 0x1000.
    LPCSTR szName;      // Pointer to name (in user addr space).
    DWORD dwThreadID;   // Thread ID (-1=caller thread).
    DWORD dwFlags;      // Reserved for future use, must be zero.
} THREADNAME_INFO;

#include <poppack.h>

#pragma warning(push)
#pragma warning(disable: 6320) // silence /analyze: Exception-filter expression is the constant EXCEPTION_EXECUTE_HANDLER. This might mask exceptions that were not intended to be handled
#pragma warning(disable: 6322) // silence /analyze: Empty _except block
void SetThreadName(DWORD threadId, const char *threadName)
{
   THREADNAME_INFO info;
   info.dwType = 0x1000;
   info.szName = threadName;
   info.dwThreadID = threadId;
   info.dwFlags = 0;

   __try
   {
      RaiseException(MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info);
   }
   __except(EXCEPTION_EXECUTE_HANDLER)
   {
   }
}
#pragma warning(push)
#else
void SetThreadName(DWORD, const char *)
{
    // nothing
}
#endif

// We need a way to uniquely identified threads (so that we can test for equality).
// Thread id assigned by the OS might be recycled. The memory address given to ThreadBase
// object can be recycled as well, so we keep our own counter.
static int GenUniqueThreadId() {
    static LONG gThreadNoSeq = 0;
    return (int) InterlockedIncrement(&gThreadNoSeq);
}

ThreadBase::ThreadBase(const char *name) :
    hThread(NULL), cancelRequested(false),
    threadNo(GenUniqueThreadId()),
    threadName(str::Dup(name))
{
    //lf("ThreadBase() %d", threadNo);
}

ThreadBase::~ThreadBase()
{
    //lf("~ThreadBase() %d", threadNo);
    CloseHandle(hThread);
}

DWORD WINAPI ThreadBase::ThreadProc(void *data)
{
    ThreadBase *thread = reinterpret_cast<ThreadBase*>(data);
    if (thread->threadName)
        SetThreadName(GetCurrentThreadId(), thread->threadName);
    thread->Run();
    return 0;
}

void ThreadBase::Start()
{
    CrashIf(hThread);
    hThread = CreateThread(NULL, 0, ThreadProc, this, 0, 0);
}

bool ThreadBase::Join(DWORD waitMs)
{
    DWORD res = WaitForSingleObject(hThread, waitMs);
    if (WAIT_OBJECT_0 == res) {
        CloseHandle(hThread);
        hThread = NULL;
        return true;
    }
    return false;
}

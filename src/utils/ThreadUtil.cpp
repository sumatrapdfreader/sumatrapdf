/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "ThreadUtil.h"

// http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
const DWORD MS_VC_EXCEPTION=0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType;       // Must be 0x1000.
    LPCSTR szName;      // Pointer to name (in user addr space).
    DWORD dwThreadID;   // Thread ID (-1=caller thread).
    DWORD dwFlags;      // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

#pragma warning(push)
#pragma warning(disable: 6320) // silence /analyze: Exception-filter expression is the constant EXCEPTION_EXECUTE_HANDLER. This might mask exceptions that were not intended to be handled
#pragma warning(disable: 6322) // silence /analyze: Empty _except block
static void SetThreadName(DWORD dwThreadID, char* threadName)
{
   THREADNAME_INFO info;
   info.dwType = 0x1000;
   info.szName = threadName;
   info.dwThreadID = dwThreadID;
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

static LONG gThreadNoSeq = 0;

ThreadBase::ThreadBase() :
      hThread(NULL), cancelRequested(0), threadName(NULL)
{
    threadNo = (int)InterlockedIncrement(&gThreadNoSeq);
    //lf("ThreadBase() %d", threadNo);
}

ThreadBase::ThreadBase(const char *name)
{
    threadName = str::Dup(name);
    cancelRequested = 0;
    hThread = NULL;
}

ThreadBase::~ThreadBase()
{
    //lf("~ThreadBase() %d", threadNo);
    CloseHandle(hThread);
    free(threadName);
}

DWORD WINAPI ThreadBase::ThreadProc(void *data)
{
    ThreadBase *thread = reinterpret_cast<ThreadBase*>(data);
    if (thread->threadName)
        SetThreadName(GetCurrentThreadId(), thread->threadName);
    thread->Run();
    thread->Release();
    return 0;
}

void ThreadBase::Start()
{
    AddRef(); // will be unref'd at the end of ThreadBase::ThreadProc
    hThread = CreateThread(NULL, 0, ThreadProc, this, 0, 0);
}

bool ThreadBase::RequestCancelAndWaitToStop(DWORD waitMs, bool terminate)
{
    RequestCancel();
    DWORD res = WaitForSingleObject(hThread, waitMs);
    if (WAIT_OBJECT_0 == res) {
        CloseHandle(hThread);
        hThread = NULL;
        return true;
    }
    if (terminate) {
        TerminateThread(hThread, 1);
        CloseHandle(hThread);
        hThread = NULL;
    }
    return false;
}


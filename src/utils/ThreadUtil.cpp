/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "ThreadUtil.h"
#include "Scoped.h"

WorkerThread::WorkerThread(Functor *f)
{
    thread = CreateThread(NULL, 0, ThreadProc, f, 0, 0);
}

WorkerThread::~WorkerThread()
{
    TerminateThread(thread, 1);
    CloseHandle(thread);
}

bool WorkerThread::Join(DWORD waitMs)
{
    return WaitForSingleObject(thread, waitMs) == WAIT_OBJECT_0;
}

DWORD WINAPI WorkerThread::ThreadProc(LPVOID data)
{
    Functor *f = (Functor *)data;
    (*f)();
    delete f;
    return 0;
}

UiMessageLoop::UiMessageLoop() : threadId(GetCurrentThreadId())
{
    InitializeCriticalSection(&cs);
}

UiMessageLoop::~UiMessageLoop()
{
    DeleteVecMembers(queue);
    DeleteCriticalSection(&cs);
}

int UiMessageLoop::Run()
{
    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        Functor *task = NULL;
        if (queue.Count() > 0) {
            ScopedCritSec scope(&cs);
            task = queue.At(0);
            queue.Remove(task);
        }
        if (task) {
            (*task)();
            delete task;
        }
    }

    return msg.wParam;
}

void UiMessageLoop::AddTask(Functor *f)
{
    ScopedCritSec scope(&cs);
    queue.Append(f);
    // make sure that the message queue isn't empty
    PostThreadMessage(threadId, WM_NULL, 0, 0);
}

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

static void SetThreadName(DWORD dwThreadID, char* threadName)
{
   THREADNAME_INFO info;
   info.dwType = 0x1000;
   info.szName = threadName;
   info.dwThreadID = dwThreadID;
   info.dwFlags = 0;

   __try
   {
      RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
   }
   __except(EXCEPTION_EXECUTE_HANDLER)
   {
   }
}

ThreadBase::ThreadBase(const char *name, bool autoDeleteSelf) : autoDeleteSelf(autoDeleteSelf)
{
    threadName = str::Dup(name);
    cancelRequested = 0;
    hThread = NULL;
}

DWORD WINAPI ThreadBase::ThreadProc(void *data)
{
    ThreadBase *thread = reinterpret_cast<ThreadBase*>(data);
    if (thread->threadName)
        SetThreadName(GetCurrentThreadId(), thread->threadName);
    thread->Run();
    HANDLE hThread = thread->hThread;
    thread->hThread = NULL;
    if (thread->autoDeleteSelf)
        delete thread;
    CloseHandle(hThread);
    return 0;
}

void ThreadBase::Start()
{
    hThread = CreateThread(NULL, 0, ThreadProc, this, 0, 0);
}

bool ThreadBase::RequestCancelAndWaitToStop(DWORD waitMs, bool terminate)
{
    RequestCancel();
    DWORD res = WaitForSingleObject(hThread, waitMs);
    if (WAIT_OBJECT_0 == res)
        return true;
    if (terminate) {
        TerminateThread(hThread, 1);
        CloseHandle(hThread);
    }
    return false;
}


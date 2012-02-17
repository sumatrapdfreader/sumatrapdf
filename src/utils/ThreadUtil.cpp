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

ThreadBase::ThreadBase(IThreadObserver *threadObserver, const char *name, bool autoDeleteSelf) :
    observer(threadObserver), autoDeleteSelf(autoDeleteSelf)
{
    // TODO: name is supposed to be a name of the thread, used for debugging purposes
    cancelRequested = 0;
    hThread = NULL;
}

DWORD WINAPI ThreadBase::ThreadProc(void *data)
{
    ThreadBase *thread = reinterpret_cast<ThreadBase*>(data);
    thread->Run();
    CloseHandle(thread->hThread);
    thread->hThread = NULL;
    if (thread->observer)
        thread->observer->ThreadFinished(thread);
    if (thread->autoDeleteSelf)
        delete thread;
    return 0;
}

void ThreadBase::Start()
{
    hThread = CreateThread(NULL, 0, ThreadProc, this, 0, 0);
}


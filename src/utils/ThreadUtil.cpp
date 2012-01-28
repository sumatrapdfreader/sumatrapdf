/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "ThreadUtil.h"
#include "Scopes.h"

FunctorQueue::FunctorQueue()
{
    InitializeCriticalSection(&cs);
}

FunctorQueue::~FunctorQueue()
{
    DeleteVecMembers(tasks);
    DeleteCriticalSection(&cs);
}

void FunctorQueue::AddTask(Functor *f)
{
    ScopedCritSec scope(&cs);
    tasks.Append(f);
}

Functor *FunctorQueue::GetNextTask()
{
    if (tasks.Count() == 0)
        return NULL;

    ScopedCritSec scope(&cs);

    Functor *task = tasks.At(0);
    tasks.Remove(task);
    return task;
}

WorkerThread::WorkerThread()
{
    event = CreateEvent(NULL, FALSE, FALSE, NULL);
    thread = CreateThread(NULL, 0, ThreadProc, this, 0, 0);
}

WorkerThread::~WorkerThread()
{
    TerminateThread(thread, 1);
    CloseHandle(thread);
    CloseHandle(event);
}

void WorkerThread::AddTask(Functor *f)
{
    FunctorQueue::AddTask(f);
    SetEvent(event);
}

DWORD WINAPI WorkerThread::ThreadProc(LPVOID data)
{
    WorkerThread *wt = (WorkerThread *)data;
    for (;;) {
        Functor *task = wt->GetNextTask();
        if (task) {
            (*task)();
            delete task;
        }
        else {
            WaitForSingleObject(wt->event, INFINITE);
        }
    }
    return 0;
}

int UiMessageLoop::Run()
{
    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        Functor *task = GetNextTask();
        if (task) {
            (*task)();
            delete task;
        }
    }

    return msg.wParam;
}

void UiMessageLoop::AddTask(Functor *f)
{
    FunctorQueue::AddTask(f);
    // make sure that the message queue isn't empty
    PostThreadMessage(threadId, WM_NULL, 0, 0);
}

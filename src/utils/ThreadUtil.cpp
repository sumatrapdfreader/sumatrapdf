/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "ThreadUtil.h"
#include "Scopes.h"


WorkerThread::WorkerThread(Functor *f)
{
    thread = CreateThread(NULL, 0, ThreadProc, f, 0, 0);
}

WorkerThread::~WorkerThread()
{
    TerminateThread(thread, 1);
    CloseHandle(thread);
}

bool WorkerThread::Join(DWORD ms)
{
    return WaitForSingleObject(thread, ms) == WAIT_OBJECT_0;
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
